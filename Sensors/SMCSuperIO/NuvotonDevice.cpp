//
//  NuvotonDevice.cpp
//
//  Sensors implementation for Nuvoton SuperIO device
//
//  Based on https://github.com/kozlek/HWSensors/blob/master/SuperIOSensors/NCT677xSensors.cpp
//  @author joedm
//

#include "NuvotonDevice.hpp"
#include "SMCSuperIO.hpp"

namespace Nuvoton {
	
	uint8_t Device::readByte(uint16_t reg) {
		uint8_t bank = reg >> 8;
		uint8_t regi = reg & 0xFF;
		uint16_t address = getDeviceAddress();
		
		::outb((uint16_t)(address + NUVOTON_ADDRESS_REGISTER_OFFSET), NUVOTON_BANK_SELECT_REGISTER);
		::outb((uint16_t)(address + NUVOTON_DATA_REGISTER_OFFSET), bank);
		::outb((uint16_t)(address + NUVOTON_ADDRESS_REGISTER_OFFSET), regi);
		
		return ::inb((uint16_t)(address + NUVOTON_DATA_REGISTER_OFFSET));
	}
	
	void Device::writeByte(uint16_t reg, uint8_t value) {
		uint8_t bank = reg >> 8;
		uint8_t regi = reg & 0xFF;
		uint16_t address = getDeviceAddress();
		
		::outb((uint16_t)(address + NUVOTON_ADDRESS_REGISTER_OFFSET), NUVOTON_BANK_SELECT_REGISTER);
		::outb((uint16_t)(address + NUVOTON_DATA_REGISTER_OFFSET), bank);
		::outb((uint16_t)(address + NUVOTON_ADDRESS_REGISTER_OFFSET), regi);
		::outb((uint16_t)(address + NUVOTON_DATA_REGISTER_OFFSET), value);
	}
	
	void Device::setupKeys(VirtualSMCAPI::Plugin &vsmcPlugin) {
		VirtualSMCAPI::addKey(KeyFNum, vsmcPlugin.data, VirtualSMCAPI::valueWithUint8(deviceDescriptor.tachometerCount, nullptr, SMC_KEY_ATTRIBUTE_CONST | SMC_KEY_ATTRIBUTE_READ));
		for (uint8_t index = 0; index < deviceDescriptor.tachometerCount; ++index) {
			VirtualSMCAPI::addKey(KeyF0Ac(index), vsmcPlugin.data, VirtualSMCAPI::valueWithFp(0, SmcKeyTypeFpe2, new TachometerKey(getSmcSuperIO(), this, index)));
		}
	}
	
	void Device::update() {
		IOSimpleLockLock(getSmcSuperIO()->counterLock);
		updateTachometers();
		IOSimpleLockUnlock(getSmcSuperIO()->counterLock);
	}
	
	void Device::updateTachometers() {
		for (uint8_t index = 0; index < deviceDescriptor.tachometerCount; ++index) {
			tachometers[index] = CALL_MEMBER_FUNC(*this, deviceDescriptor.updateTachometer)(index);
		}
	}
	
	uint16_t Device::tachometerReadDefault(uint8_t index) {
		uint8_t high = readByte(deviceDescriptor.tachometerRpmBaseRegister + (index << 1));
		uint8_t low = readByte(deviceDescriptor.tachometerRpmBaseRegister + (index << 1) + 1);
	
		uint16_t value = (high << 8) | low;
		
		return value > deviceDescriptor.tachometerMinRPM ? value : 0;
	}
	
	void Device::initialize679xx() {
		i386_ioport_t port = getDevicePort();
		// disable the hardware monitor i/o space lock on NCT679xD chips
		enter(port);
		selectLogicalDevice(port, WinbondHardwareMonitorLDN);
		/* Activate logical device if needed */
		uint8_t options = listenPortByte(port, NUVOTON_REG_ENABLE);
		if (!(options & 0x01)) {
			writePortByte(port, NUVOTON_REG_ENABLE, options | 0x01);
		}
		options = listenPortByte(port, NUVOTON_HWMON_IO_SPACE_LOCK);
		// if the i/o space lock is enabled
		if (options & 0x10) {
			// disable the i/o space lock
			writePortByte(port, NUVOTON_HWMON_IO_SPACE_LOCK, (uint8_t)(options & ~0x10));
		}
		leave(port);
	}
	
	void Device::powerStateChanged(unsigned long state) {
		if (state == SMCSuperIO::PowerStateOn) {
			 CALL_MEMBER_FUNC(*this, deviceDescriptor.initialize)();
		 }
	}

	/**
	 *  Supported devices
	 */
	const Device::DeviceDescriptor Device::_NCT6771F = { NCT6771F, 3, RPM_THRESHOLD1, 0x656, &Device::tachometerReadDefault, &Device::stub };
	const Device::DeviceDescriptor Device::_NCT6776F = { NCT6776F, 3, RPM_THRESHOLD2, 0x656, &Device::tachometerReadDefault, &Device::stub };
	const Device::DeviceDescriptor Device::_NCT6779D = { NCT6779D, 5, RPM_THRESHOLD2, 0x4C0, &Device::tachometerReadDefault, &Device::stub };
	const Device::DeviceDescriptor Device::_NCT6791D = { NCT6791D, 6, RPM_THRESHOLD2, 0x4C0, &Device::tachometerReadDefault, &Device::initialize679xx };
	const Device::DeviceDescriptor Device::_NCT6792D = { NCT6792D, 6, RPM_THRESHOLD2, 0x4C0, &Device::tachometerReadDefault, &Device::initialize679xx };
	const Device::DeviceDescriptor Device::_NCT6793D = { NCT6793D, 6, RPM_THRESHOLD2, 0x4C0, &Device::tachometerReadDefault, &Device::initialize679xx };
	const Device::DeviceDescriptor Device::_NCT6795D = { NCT6795D, 6, RPM_THRESHOLD2, 0x4C0, &Device::tachometerReadDefault, &Device::initialize679xx };
	const Device::DeviceDescriptor Device::_NCT6796D = { NCT6796D, 6, RPM_THRESHOLD2, 0x4C0, &Device::tachometerReadDefault, &Device::initialize679xx };
	
	/**
	 *  Device factory helper
	 */
	const Device::DeviceDescriptor* Device::detectModel(uint16_t id, uint8_t &ldn) {
		uint8_t majorId = id >> 8;
		if (majorId == 0xB4 && (id & 0xf0) == 0x70) {
			return &_NCT6771F;
		} else if (majorId == 0xC3 && (id & 0xf0) == 0x30) {
			return &_NCT6776F;
		} else if (majorId == 0xC5 && (id & 0xf0) == 0x60) {
			return &_NCT6779D;
		} else if (majorId == 0xC8 && (id & 0xFF) == 0x03) {
			return &_NCT6791D;
		} else if (majorId == 0xC9 && (id & 0xFF) == 0x11) {
			return &_NCT6792D;
		} else if (majorId == 0xD1 && (id & 0xFF) == 0x21) {
			return &_NCT6793D;
		} else if (majorId == 0xD3 && (id & 0xFF) == 0x52) {
			return &_NCT6795D;
		} else if (majorId == 0xD4 && (id & 0xFF) == 0x23) {
			return &_NCT6796D;
		}
		return nullptr;
	}

	/**
	 *  Device factory
	 */
	SuperIODevice* Device::detect(SMCSuperIO* sio) {
		return WindbondFamilyDevice::detect<Device, DeviceDescriptor>(sio);
	}

} // namespace Nuvoton

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::collections::hash_map::ValuesMut;
use std::collections::HashMap;

use u2fprotocol::u2f_init_device;

use platform::monitor::Event;
use platform::device::Device;
use platform::iokit::*;

pub struct DeviceMap {
    map: HashMap<IOHIDDeviceRef, Device>,
}

impl DeviceMap {
    pub fn new() -> Self {
        Self { map: HashMap::new() }
    }

    pub fn values_mut(&mut self) -> ValuesMut<IOHIDDeviceRef, Device> {
        self.map.values_mut()
    }

    pub fn process_event(&mut self, event: Event) {
        match event {
            Event::Add(dev) => self.add(dev),
            Event::Remove(dev) => self.remove(dev),
        }
    }

    fn add(&mut self, device_ref: IOHIDDeviceRef) {
        if self.map.contains_key(&device_ref) {
            return;
        }

        // Create the device.
        let mut dev = Device::new(device_ref);

        if u2f_init_device(&mut dev) {
            self.map.insert(device_ref, dev);
        }
    }

    fn remove(&mut self, device_ref: IOHIDDeviceRef) {
        // Ignore errors.
        let _ = self.map.remove(&device_ref);
    }
}

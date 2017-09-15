/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::collections::hash_map::ValuesMut;
use std::collections::HashMap;

use platform::device::Device;
use platform::monitor::Event;
use u2fprotocol::u2f_init_device;

pub struct DeviceMap {
    map: HashMap<String, Device>,
}

impl DeviceMap {
    pub fn new() -> Self {
        Self { map: HashMap::new() }
    }

    pub fn values_mut(&mut self) -> ValuesMut<String, Device> {
        self.map.values_mut()
    }

    pub fn process_event(&mut self, event: Event) {
        match event {
            Event::Add(path) => self.add(path),
            Event::Remove(path) => self.remove(path),
        }
    }

    fn add(&mut self, path: String) {
        if self.map.contains_key(&path) {
            return;
        }

        // Create and try to open the device.
        if let Ok(mut dev) = Device::new(path.clone()) {
            if dev.is_u2f() && u2f_init_device(&mut dev) {
                self.map.insert(path, dev);
            }
        }
    }

    fn remove(&mut self, path: String) {
        // Ignore errors.
        let _ = self.map.remove(&path);
    }
}

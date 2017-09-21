/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::collections::{HashMap, HashSet};
use std::collections::hash_map::IterMut;
use std::hash::Hash;

// The KeyHandleMatcher is a helper class that tracks which key handles belong
// to which device (token). It is initialized with a list of key handles we
// were passed by the U2F or WebAuthn API.
//
// 'a = the lifetime of key handles, we don't want to own them
//  K = the type we use to identify devices on this platform (path, device_ref)
pub struct KeyHandleMatcher<'a, K> {
    key_handles: &'a Vec<Vec<u8>>,
    map: HashMap<K, Vec<&'a Vec<u8>>>,
}

impl<'a, K> KeyHandleMatcher<'a, K>
where
    K: Clone + Eq + Hash,
{
    pub fn new(key_handles: &'a Vec<Vec<u8>>) -> Self {
        Self {
            key_handles,
            map: HashMap::new(),
        }
    }

    // `update()` will iterate all devices and ignore the ones we've already
    // checked. It will then call the given closure exactly once for each key
    // handle and device combination to let the caller decide whether we have
    // a match.
    //
    // If a device was removed since the last `update()` call we'll remove it
    // from the internal map as well to ensure we query a device that reuses a
    // dev_ref or path next time.
    //
    // TODO In theory, it might be possible to replace a token between
    // `update()` calls while reusing the device_ref/path. We should refactor
    // this part of the code and probably merge the KeyHandleMatcher into the
    // DeviceMap and Monitor somehow so that we query key handle/token
    // assignments right when we start tracking a new device.
    pub fn update<F, V>(&mut self, devices: IterMut<K, V>, is_match: F)
    where
        F: Fn(&mut V, &Vec<u8>) -> bool,
    {
        // Collect all currently known device references.
        let mut to_remove: HashSet<K> = self.map.keys().cloned().collect();

        for (dev_ref, device) in devices {
            // This device is still connected.
            to_remove.remove(dev_ref);

            // Skip devices we've already seen.
            if self.map.contains_key(dev_ref) {
                continue;
            }

            let mut matches = vec![];

            // Collect all matching key handles.
            for key_handle in self.key_handles {
                if is_match(device, key_handle) {
                    matches.push(key_handle);
                }
            }

            self.map.insert((*dev_ref).clone(), matches);
        }

        // Remove devices that disappeared since the last call.
        for dev_ref in to_remove {
            self.map.remove(&dev_ref);
        }
    }

    // `get()` allows retrieving key handle/token assignments that were
    // process by calls to `update()` before.
    pub fn get(&self, dev_ref: &K) -> &Vec<&'a Vec<u8>> {
        self.map.get(dev_ref).expect("unknown device")
    }
}

#[cfg(test)]
mod tests {
    use super::KeyHandleMatcher;

    use std::collections::HashMap;

    #[test]
    fn test() {
        // Three key handles.
        let khs = vec![
            vec![0x01, 0x02, 0x03, 0x04],
            vec![0x02, 0x03, 0x04, 0x05],
            vec![0x03, 0x04, 0x05, 0x06],
        ];

        // Start with three devices.
        let mut map = HashMap::new();
        map.insert("device1", 1);
        map.insert("device2", 2);
        map.insert("device3", 3);

        // Assign key handles to devices.
        let mut khm = KeyHandleMatcher::new(&khs);
        khm.update(map.iter_mut(), |device, key_handle| *device > key_handle[0]);

        // Check assignments.
        assert!(khm.get(&"device1").is_empty());
        assert_eq!(*khm.get(&"device2"), vec![&vec![0x01, 0x02, 0x03, 0x04]]);
        assert_eq!(
            *khm.get(&"device3"),
            vec![&vec![0x01, 0x02, 0x03, 0x04], &vec![0x02, 0x03, 0x04, 0x05]]
        );

        // Check we don't check a device twice.
        map.insert("device4", 4);
        khm.update(map.iter_mut(), |device, key_handle| {
            assert_eq!(*device, 4);
            key_handle[0] & 1 == 1
        });

        // Check assignments.
        assert_eq!(
            *khm.get(&"device4"),
            vec![&vec![0x01, 0x02, 0x03, 0x04], &vec![0x03, 0x04, 0x05, 0x06]]
        );

        // Remove device #2.
        map.remove("device2");
        khm.update(map.iter_mut(), |_, _| {
            assert!(false);
            false
        });

        // Re-insert device #2 matching different key handles.
        map.insert("device2", 2);
        khm.update(map.iter_mut(), |device, _| {
            assert_eq!(*device, 2);
            true
        });

        // Check assignments.
        assert_eq!(
            *khm.get(&"device2"),
            vec![
                &vec![0x01, 0x02, 0x03, 0x04],
                &vec![0x02, 0x03, 0x04, 0x05],
                &vec![0x03, 0x04, 0x05, 0x06],
            ]
        );
    }
}

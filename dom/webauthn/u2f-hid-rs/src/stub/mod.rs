/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// No-op module to permit compiling token HID support for Android, where
// no results are returned.

#![allow(dead_code)]

use util::OnceCallback;

pub struct PlatformManager {}

impl PlatformManager {
    pub fn new() -> Self {
        Self {}
    }

    pub fn register(
        &mut self,
        timeout: u64,
        challenge: Vec<u8>,
        application: Vec<u8>,
        callback: OnceCallback<Vec<u8>>,
    ) {
        // No-op on Android
    }

    pub fn sign(
        &mut self,
        timeout: u64,
        challenge: Vec<u8>,
        application: Vec<u8>,
        key_handles: Vec<Vec<u8>>,
        callback: OnceCallback<(Vec<u8>, Vec<u8>)>,
    ) {
        // No-op on Android
    }

    // This blocks.
    pub fn cancel(&mut self) {
        // No-op on Android
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use util::OnceCallback;

pub struct Transaction {}

impl Transaction {
    pub fn new<F, T>(
        timeout: u64,
        callback: OnceCallback<T>,
        new_device_cb: F,
    ) -> Result<Self, ::Error>
    where
        F: Fn(String, &Fn() -> bool),
    {
        callback.call(Err(::Error::NotSupported));
        Err(::Error::NotSupported)
    }

    pub fn cancel(&mut self) {
        /* No-op. */
    }
}

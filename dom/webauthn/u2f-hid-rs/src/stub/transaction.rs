/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::io;
use util::{io_err, OnceCallback};

pub struct Transaction {}

impl Transaction {
    pub fn new<F, T>(timeout: u64, callback: OnceCallback<T>, new_device_cb: F) -> io::Result<Self>
    where
        F: Fn(String, &Fn() -> bool),
    {
        callback.call(Err(io_err("not implemented")));
        Err(io_err("not implemented"))
    }

    pub fn cancel(&mut self) {
        /* No-op. */
    }
}

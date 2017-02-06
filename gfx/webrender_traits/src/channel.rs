/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// A helper to handle the interface difference between
// IpcBytesSender and Sender<Vec<u8>>
pub trait PayloadHelperMethods {
    fn send_vec(&self, data: Vec<u8>) -> Result<(), Error>;
}

#[cfg(not(feature = "ipc"))]
include!("channel_mpsc.rs");

#[cfg(feature = "ipc")]
include!("channel_ipc.rs");

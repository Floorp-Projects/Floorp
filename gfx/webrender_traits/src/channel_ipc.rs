/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use serde::{Deserialize, Serialize};
use std::io::Error;

use ipc_channel::ipc::{self, IpcSender, IpcReceiver, IpcBytesSender, IpcBytesReceiver};

///
/// Handles the channel implementation when IPC is enabled.
///

pub type MsgSender<T> = IpcSender<T>;

pub type MsgReceiver<T> = IpcReceiver<T>;

pub type PayloadSender = IpcBytesSender;

pub type PayloadReceiver = IpcBytesReceiver;

impl PayloadHelperMethods for PayloadSender {
    fn send_vec(&self, data: Vec<u8>) -> Result<(), Error> {
        self.send(&data)
    }
}

pub fn msg_channel<T: Serialize + Deserialize>() -> Result<(MsgSender<T>, MsgReceiver<T>), Error> {
    ipc::channel()
}

pub fn payload_channel() -> Result<(PayloadSender, PayloadReceiver), Error> {
    ipc::bytes_channel()
}

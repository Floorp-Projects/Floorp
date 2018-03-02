/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use ipc_channel::ipc::{self, IpcBytesReceiver, IpcBytesSender, IpcReceiver, IpcSender};
use serde::{Deserialize, Serialize};
use std::io::{Error, ErrorKind};
use std::sync::mpsc;
use std::thread;
use std::{error, io};

///
/// Handles the channel implementation when IPC is enabled.
///

pub type MsgSender<T> = IpcSender<T>;

pub type MsgReceiver<T> = IpcReceiver<T>;

pub type PayloadSender = IpcBytesSender;

pub type PayloadReceiver = IpcBytesReceiver;

impl PayloadSenderHelperMethods for PayloadSender {
    fn send_payload(&self, data: Payload) -> Result<(), Error> {
        self.send(&data.to_data())
    }
}

impl PayloadReceiverHelperMethods for PayloadReceiver {
    fn recv_payload(&self) -> Result<Payload, Error> {
        self.recv().map(|data| Payload::from_data(&data) )
                   .map_err(|e| io::Error::new(ErrorKind::Other, error::Error::description(&e)))
    }

    fn to_mpsc_receiver(self) -> Receiver<Payload> {
        // It would be nice to use the IPC router for this,
        // but that requires an IpcReceiver, not an IpcBytesReceiver.
        let (tx, rx) = mpsc::channel();
        thread::spawn(move || {
            while let Ok(payload) = self.recv_payload() {
                if tx.send(payload).is_err() {
                    break;
                }
            }
        });
        rx
    }
}

pub fn msg_channel<T: Serialize + for<'de> Deserialize<'de>>() -> Result<(MsgSender<T>, MsgReceiver<T>), Error> {
    ipc::channel()
}

pub fn payload_channel() -> Result<(PayloadSender, PayloadReceiver), Error> {
    ipc::bytes_channel()
}

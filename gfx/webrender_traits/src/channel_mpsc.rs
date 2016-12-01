/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use serde::{Deserialize, Serialize};
use std::io::Error;

use serde::{Deserializer, Serializer};

use std::sync::mpsc;

///
/// Handles the channel implementation when in process channels are enabled.
///

pub type PayloadSender = MsgSender<Vec<u8>>;

pub type PayloadReceiver = MsgReceiver<Vec<u8>>;

impl PayloadHelperMethods for PayloadSender {
    fn send_vec(&self, data: Vec<u8>) -> Result<(), Error> {
        self.send(data)
    }
}

pub struct MsgReceiver<T> {
    rx: mpsc::Receiver<T>,
}

impl<T> MsgReceiver<T> {
    pub fn recv(&self) -> Result<T, Error> {
        Ok(self.rx.recv().unwrap())
    }
}

#[derive(Clone)]
pub struct MsgSender<T> {
    tx: mpsc::Sender<T>,
}

impl<T> MsgSender<T> {
    pub fn send(&self, data: T) -> Result<(), Error> {
        Ok(self.tx.send(data).unwrap())
    }
}

pub fn payload_channel() -> Result<(PayloadSender, PayloadReceiver), Error> {
    let (tx, rx) = mpsc::channel();
    Ok((PayloadSender { tx: tx }, PayloadReceiver { rx: rx }))
}

pub fn msg_channel<T>() -> Result<(MsgSender<T>, MsgReceiver<T>), Error> {
    let (tx, rx) = mpsc::channel();
    Ok((MsgSender { tx: tx }, MsgReceiver { rx: rx }))
}

///
/// These serialize methods are needed to satisfy the compiler
/// which uses these implementations for IPC, and also for the
/// recording tool. The recording tool only outputs messages
/// that don't contain Senders or Receivers, so in theory
/// these should never be called in the in-process config.
/// If they are called, there may be a bug in the messages
/// that the replay tool is writing.
///

impl<T> Serialize for MsgReceiver<T> {
    fn serialize<S: Serializer>(&self, _: &mut S) -> Result<(), S::Error> {
        unreachable!();
    }
}

impl<T> Serialize for MsgSender<T> {
    fn serialize<S: Serializer>(&self, _: &mut S) -> Result<(), S::Error> {
        unreachable!();
    }
}

impl<T> Deserialize for MsgReceiver<T> {
    fn deserialize<D>(_: &mut D) -> Result<MsgReceiver<T>, D::Error>
                      where D: Deserializer {
        unreachable!();
    }
}

impl<T> Deserialize for MsgSender<T> {
    fn deserialize<D>(_: &mut D) -> Result<MsgSender<T>, D::Error>
                      where D: Deserializer {
        unreachable!();
    }
}

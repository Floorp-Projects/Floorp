/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::io;
use std::sync::mpsc::{channel, Sender, RecvTimeoutError};
use std::time::Duration;

use consts::PARAMETER_SIZE;
use statemachine::StateMachine;
use runloop::RunLoop;
use util::{to_io_err, OnceCallback};

enum QueueAction {
    Register {
        flags: ::RegisterFlags,
        timeout: u64,
        challenge: Vec<u8>,
        application: Vec<u8>,
        key_handles: Vec<::KeyHandle>,
        callback: OnceCallback<Vec<u8>>,
    },
    Sign {
        flags: ::SignFlags,
        timeout: u64,
        challenge: Vec<u8>,
        application: Vec<u8>,
        key_handles: Vec<::KeyHandle>,
        callback: OnceCallback<(Vec<u8>, Vec<u8>)>,
    },
    Cancel,
}

pub struct U2FManager {
    queue: RunLoop,
    tx: Sender<QueueAction>,
}

impl U2FManager {
    pub fn new() -> io::Result<Self> {
        let (tx, rx) = channel();

        // Start a new work queue thread.
        let queue = RunLoop::new(move |alive| {
            let mut sm = StateMachine::new();

            while alive() {
                match rx.recv_timeout(Duration::from_millis(50)) {
                    Ok(QueueAction::Register {
                           flags,
                           timeout,
                           challenge,
                           application,
                           key_handles,
                           callback,
                       }) => {
                        // This must not block, otherwise we can't cancel.
                        sm.register(
                            flags,
                            timeout,
                            challenge,
                            application,
                            key_handles,
                            callback,
                        );
                    }
                    Ok(QueueAction::Sign {
                           flags,
                           timeout,
                           challenge,
                           application,
                           key_handles,
                           callback,
                       }) => {
                        // This must not block, otherwise we can't cancel.
                        sm.sign(
                            flags,
                            timeout,
                            challenge,
                            application,
                            key_handles,
                            callback,
                        );
                    }
                    Ok(QueueAction::Cancel) => {
                        // Cancelling must block so that we don't start a new
                        // polling thread before the old one has shut down.
                        sm.cancel();
                    }
                    Err(RecvTimeoutError::Disconnected) => {
                        break;
                    }
                    _ => { /* continue */ }
                }
            }

            // Cancel any ongoing activity.
            sm.cancel();
        })?;

        Ok(Self {
            queue: queue,
            tx: tx,
        })
    }

    pub fn register<F>(
        &self,
        flags: ::RegisterFlags,
        timeout: u64,
        challenge: Vec<u8>,
        application: Vec<u8>,
        key_handles: Vec<::KeyHandle>,
        callback: F,
    ) -> io::Result<()>
    where
        F: FnOnce(io::Result<Vec<u8>>),
        F: Send + 'static,
    {
        if challenge.len() != PARAMETER_SIZE || application.len() != PARAMETER_SIZE {
            return Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                "Invalid parameter sizes",
            ));
        }

        for key_handle in &key_handles {
            if key_handle.credential.len() > 256 {
                return Err(io::Error::new(
                    io::ErrorKind::InvalidInput,
                    "Key handle too large",
                ));
            }
        }

        let callback = OnceCallback::new(callback);
        let action = QueueAction::Register {
            flags,
            timeout,
            challenge,
            application,
            key_handles,
            callback,
        };
        self.tx.send(action).map_err(to_io_err)
    }

    pub fn sign<F>(
        &self,
        flags: ::SignFlags,
        timeout: u64,
        challenge: Vec<u8>,
        application: Vec<u8>,
        key_handles: Vec<::KeyHandle>,
        callback: F,
    ) -> io::Result<()>
    where
        F: FnOnce(io::Result<(Vec<u8>, Vec<u8>)>),
        F: Send + 'static,
    {
        if challenge.len() != PARAMETER_SIZE || application.len() != PARAMETER_SIZE {
            return Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                "Invalid parameter sizes",
            ));
        }

        if key_handles.len() < 1 {
            return Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                "No key handles given",
            ));
        }

        for key_handle in &key_handles {
            if key_handle.credential.len() > 256 {
                return Err(io::Error::new(
                    io::ErrorKind::InvalidInput,
                    "Key handle too large",
                ));
            }
        }

        let callback = OnceCallback::new(callback);
        let action = QueueAction::Sign {
            flags,
            timeout,
            challenge,
            application,
            key_handles,
            callback,
        };
        self.tx.send(action).map_err(to_io_err)
    }

    pub fn cancel(&self) -> io::Result<()> {
        self.tx.send(QueueAction::Cancel).map_err(to_io_err)
    }
}

impl Drop for U2FManager {
    fn drop(&mut self) {
        self.queue.cancel();
    }
}

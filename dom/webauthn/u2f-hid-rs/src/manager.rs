/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::io;
use std::sync::mpsc::{channel, Sender, RecvTimeoutError};
use std::time::Duration;

use consts::PARAMETER_SIZE;
use platform::PlatformManager;
use runloop::RunLoop;
use util::{to_io_err, OnceCallback};

pub enum QueueAction {
    Register {
        timeout: u64,
        challenge: Vec<u8>,
        application: Vec<u8>,
        callback: OnceCallback<Vec<u8>>,
    },
    Sign {
        timeout: u64,
        challenge: Vec<u8>,
        application: Vec<u8>,
        key_handles: Vec<Vec<u8>>,
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
        let queue = try!(RunLoop::new(
            move |alive| {
                let mut pm = PlatformManager::new();

                while alive() {
                    match rx.recv_timeout(Duration::from_millis(50)) {
                        Ok(QueueAction::Register {
                               timeout,
                               challenge,
                               application,
                               callback,
                           }) => {
                            // This must not block, otherwise we can't cancel.
                            pm.register(timeout, challenge, application, callback);
                        }
                        Ok(QueueAction::Sign {
                               timeout,
                               challenge,
                               application,
                               key_handles,
                               callback,
                           }) => {
                            // This must not block, otherwise we can't cancel.
                            pm.sign(timeout, challenge, application, key_handles, callback);
                        }
                        Ok(QueueAction::Cancel) => {
                            // Cancelling must block so that we don't start a new
                            // polling thread before the old one has shut down.
                            pm.cancel();
                        }
                        Err(RecvTimeoutError::Disconnected) => {
                            break;
                        }
                        _ => { /* continue */ }
                    }
                }

                // Cancel any ongoing activity.
                pm.cancel();
            },
            0, /* no timeout */
        ));

        Ok(Self {
            queue: queue,
            tx: tx,
        })
    }

    pub fn register<F>(
        &self,
        timeout: u64,
        challenge: Vec<u8>,
        application: Vec<u8>,
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

        let callback = OnceCallback::new(callback);
        let action = QueueAction::Register {
            timeout: timeout,
            challenge: challenge,
            application: application,
            callback: callback,
        };
        self.tx.send(action).map_err(to_io_err)
    }

    pub fn sign<F>(
        &self,
        timeout: u64,
        challenge: Vec<u8>,
        application: Vec<u8>,
        key_handles: Vec<Vec<u8>>,
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
            if key_handle.len() > 256 {
                return Err(io::Error::new(
                    io::ErrorKind::InvalidInput,
                    "Key handle too large",
                ));
            }
        }

        let callback = OnceCallback::new(callback);
        let action = QueueAction::Sign {
            timeout: timeout,
            challenge: challenge,
            application: application,
            key_handles: key_handles,
            callback: callback,
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

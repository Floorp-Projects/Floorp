/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate log;

mod device;
mod iokit;
mod monitor;
mod transaction;

use consts::PARAMETER_SIZE;
use platform::device::Device;
use platform::transaction::Transaction;
use std::thread;
use std::time::Duration;
use u2fprotocol::{u2f_init_device, u2f_register, u2f_sign, u2f_is_keyhandle_valid};
use util::{io_err, OnceCallback};

#[derive(Default)]
pub struct PlatformManager {
    transaction: Option<Transaction>,
}

impl PlatformManager {
    pub fn new() -> Self {
        Default::default()
    }

    pub fn register(
        &mut self,
        timeout: u64,
        challenge: Vec<u8>,
        application: Vec<u8>,
        key_handles: Vec<Vec<u8>>,
        callback: OnceCallback<Vec<u8>>,
    ) {
        // Abort any prior register/sign calls.
        self.cancel();

        let cbc = callback.clone();

        // Start a new "sign" transaction.
        let transaction = Transaction::new(timeout, cbc.clone(), move |device_ref, rx, alive| {
            // Create a new device.
            let dev = &mut Device::new(device_ref, rx);

            // Try initializing it.
            if !u2f_init_device(dev) {
                return;
            }

            // Iterate the exlude list and see if there are any matches.
            // Abort the state machine if we found a valid key handle.
            if key_handles.iter().any(|key_handle| {
                u2f_is_keyhandle_valid(dev, &challenge, &application, key_handle)
                    .unwrap_or(false) /* no match on failure */
            })
            {
                return;
            }

            while alive() {
                if let Ok(bytes) = u2f_register(dev, &challenge, &application) {
                    callback.call(Ok(bytes));
                    break;
                }

                // Sleep a bit before trying again.
                thread::sleep(Duration::from_millis(100));
            }
        });

        // Store the transaction so we can cancel it, if needed.
        self.transaction = Some(try_or!(transaction, |_| {
            cbc.call(Err(io_err("couldn't create transaction")))
        }));
    }

    pub fn sign(
        &mut self,
        timeout: u64,
        challenge: Vec<u8>,
        application: Vec<u8>,
        key_handles: Vec<Vec<u8>>,
        callback: OnceCallback<(Vec<u8>, Vec<u8>)>,
    ) {
        // Abort any prior register/sign calls.
        self.cancel();

        let cbc = callback.clone();

        // Start a new "register" transaction.
        let transaction = Transaction::new(timeout, cbc.clone(), move |device_ref, rx, alive| {
            // Create a new device.
            let dev = &mut Device::new(device_ref, rx);

            // Try initializing it.
            if !u2f_init_device(dev) {
                return;
            }

            // Find all matching key handles.
            let key_handles = key_handles
                .iter()
                .filter(|key_handle| {
                    u2f_is_keyhandle_valid(dev, &challenge, &application, key_handle)
                        .unwrap_or(false) /* no match on failure */
                })
                .collect::<Vec<&Vec<u8>>>();

            while alive() {
                // If the device matches none of the given key handles
                // then just make it blink with bogus data.
                if key_handles.is_empty() {
                    let blank = vec![0u8; PARAMETER_SIZE];
                    if let Ok(_) = u2f_register(dev, &blank, &blank) {
                        callback.call(Err(io_err("invalid key")));
                        break;
                    }
                } else {
                    // Otherwise, try to sign.
                    for key_handle in &key_handles {
                        if let Ok(bytes) = u2f_sign(dev, &challenge, &application, key_handle) {
                            callback.call(Ok((key_handle.to_vec(), bytes)));
                            break;
                        }
                    }
                }

                // Sleep a bit before trying again.
                thread::sleep(Duration::from_millis(100));
            }
        });

        // Store the transaction so we can cancel it, if needed.
        self.transaction = Some(try_or!(transaction, |_| {
            cbc.call(Err(io_err("couldn't create transaction")))
        }));
    }

    pub fn cancel(&mut self) {
        if let Some(mut transaction) = self.transaction.take() {
            transaction.cancel();
        }
    }
}

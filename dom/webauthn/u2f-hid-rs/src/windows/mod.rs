/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::thread;
use std::time::Duration;

mod device;
mod devicemap;
mod monitor;
mod winapi;

use consts::PARAMETER_SIZE;
use runloop::RunLoop;
use util::{io_err, OnceCallback};
use u2fprotocol::{u2f_register, u2f_sign, u2f_is_keyhandle_valid};

use self::devicemap::DeviceMap;
use self::monitor::Monitor;

pub struct PlatformManager {
    // Handle to the thread loop.
    thread: Option<RunLoop>,
}

impl PlatformManager {
    pub fn new() -> Self {
        Self { thread: None }
    }

    pub fn register(
        &mut self,
        timeout: u64,
        challenge: Vec<u8>,
        application: Vec<u8>,
        callback: OnceCallback<Vec<u8>>,
    ) {
        // Abort any prior register/sign calls.
        self.cancel();

        let cbc = callback.clone();

        let thread = RunLoop::new(
            move |alive| {
                let mut devices = DeviceMap::new();
                let monitor = try_or!(Monitor::new(), |e| { callback.call(Err(e)); });

                while alive() && monitor.alive() {
                    // Add/remove devices.
                    for event in monitor.events() {
                        devices.process_event(event);
                    }

                    // Try to register each device.
                    for device in devices.values_mut() {
                        if let Ok(bytes) = u2f_register(device, &challenge, &application) {
                            callback.call(Ok(bytes));
                            return;
                        }
                    }

                    // Wait a little before trying again.
                    thread::sleep(Duration::from_millis(100));
                }

                callback.call(Err(io_err("aborted or timed out")));
            },
            timeout,
        );

        self.thread = Some(try_or!(thread, |_| {
            cbc.call(Err(io_err("couldn't create runloop")));
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

        let thread = RunLoop::new(
            move |alive| {
                let mut devices = DeviceMap::new();
                let monitor = try_or!(Monitor::new(), |e| { callback.call(Err(e)); });

                while alive() && monitor.alive() {
                    // Add/remove devices.
                    for event in monitor.events() {
                        devices.process_event(event);
                    }

                    // Try signing with each device.
                    for key_handle in &key_handles {
                        for device in devices.values_mut() {
                            // Check if they key handle belongs to the current device.
                            let is_valid = match u2f_is_keyhandle_valid(
                                device,
                                &challenge,
                                &application,
                                &key_handle,
                            ) {
                                Ok(valid) => valid,
                                Err(_) => continue, // Skip this device for now.
                            };

                            if is_valid {
                                // If yes, try to sign.
                                if let Ok(bytes) = u2f_sign(
                                    device,
                                    &challenge,
                                    &application,
                                    &key_handle,
                                )
                                {
                                    callback.call(Ok((key_handle.clone(), bytes)));
                                    return;
                                }
                            } else {
                                // If no, keep registering and blinking with bogus data
                                let blank = vec![0u8; PARAMETER_SIZE];
                                if let Ok(_) = u2f_register(device, &blank, &blank) {
                                    callback.call(Err(io_err("invalid key")));
                                    return;
                                }
                            }
                        }
                    }

                    // Wait a little before trying again.
                    thread::sleep(Duration::from_millis(100));
                }

                callback.call(Err(io_err("aborted or timed out")));
            },
            timeout,
        );

        self.thread = Some(try_or!(thread, |_| {
            cbc.call(Err(io_err("couldn't create runloop")));
        }));
    }

    // This might block.
    pub fn cancel(&mut self) {
        if let Some(thread) = self.thread.take() {
            thread.cancel();
        }
    }
}

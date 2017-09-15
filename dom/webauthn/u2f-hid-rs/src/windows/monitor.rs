/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::collections::HashSet;
use std::error::Error;
use std::io;
use std::iter::FromIterator;
use std::sync::mpsc::{channel, Receiver, TryIter};
use std::thread;
use std::time::Duration;

use runloop::RunLoop;
use super::winapi::DeviceInfoSet;

pub fn io_err(msg: &str) -> io::Error {
    io::Error::new(io::ErrorKind::Other, msg)
}

pub fn to_io_err<T: Error>(err: T) -> io::Error {
    io_err(err.description())
}

pub enum Event {
    Add(String),
    Remove(String),
}

pub struct Monitor {
    // Receive events from the thread.
    rx: Receiver<Event>,
    // Handle to the thread loop.
    thread: RunLoop,
}

impl Monitor {
    pub fn new() -> io::Result<Self> {
        let (tx, rx) = channel();

        let thread = RunLoop::new(
            move |alive| -> io::Result<()> {
                let mut stored = HashSet::new();

                while alive() {
                    let device_info_set = DeviceInfoSet::new()?;
                    let devices = HashSet::from_iter(device_info_set.devices());

                    // Remove devices that are gone.
                    for path in stored.difference(&devices) {
                        tx.send(Event::Remove(path.clone())).map_err(to_io_err)?;
                    }

                    // Add devices that were plugged in.
                    for path in devices.difference(&stored) {
                        tx.send(Event::Add(path.clone())).map_err(to_io_err)?;
                    }

                    // Remember the new set.
                    stored = devices;

                    // Wait a little before looking for devices again.
                    thread::sleep(Duration::from_millis(100));
                }

                Ok(())
            },
            0, /* no timeout */
        )?;

        Ok(Self {
            rx: rx,
            thread: thread,
        })
    }

    pub fn events<'a>(&'a self) -> TryIter<'a, Event> {
        self.rx.try_iter()
    }

    pub fn alive(&self) -> bool {
        self.thread.alive()
    }
}

impl Drop for Monitor {
    fn drop(&mut self) {
        self.thread.cancel();
    }
}

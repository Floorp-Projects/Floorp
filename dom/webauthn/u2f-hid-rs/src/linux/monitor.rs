/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use libudev;
use libudev::EventType;

use libc::{c_int, c_short, c_ulong};

use std::ffi::OsString;
use std::io;
use std::os::unix::io::AsRawFd;
use std::sync::mpsc::{channel, Receiver, TryIter};

use runloop::RunLoop;
use util::to_io_err;

const UDEV_SUBSYSTEM: &'static str = "hidraw";
const POLLIN: c_short = 0x0001;
const POLL_TIMEOUT: c_int = 100;

fn poll(fds: &mut Vec<::libc::pollfd>) -> io::Result<()> {
    let nfds = fds.len() as c_ulong;

    let rv = unsafe { ::libc::poll((&mut fds[..]).as_mut_ptr(), nfds, POLL_TIMEOUT) };

    if rv < 0 {
        Err(io::Error::from_raw_os_error(rv))
    } else {
        Ok(())
    }
}

pub enum Event {
    Add(OsString),
    Remove(OsString),
}

impl Event {
    fn from_udev(event: libudev::Event) -> Option<Self> {
        let path = event.device().devnode().map(
            |dn| dn.to_owned().into_os_string(),
        );

        match (event.event_type(), path) {
            (EventType::Add, Some(path)) => Some(Event::Add(path)),
            (EventType::Remove, Some(path)) => Some(Event::Remove(path)),
            _ => None,
        }
    }
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
                let ctx = libudev::Context::new()?;
                let mut enumerator = libudev::Enumerator::new(&ctx)?;
                enumerator.match_subsystem(UDEV_SUBSYSTEM)?;

                // Iterate all existing devices.
                for dev in enumerator.scan_devices()? {
                    if let Some(path) = dev.devnode().map(|p| p.to_owned().into_os_string()) {
                        tx.send(Event::Add(path)).map_err(to_io_err)?;
                    }
                }

                let mut monitor = libudev::Monitor::new(&ctx)?;
                monitor.match_subsystem(UDEV_SUBSYSTEM)?;

                // Start listening for new devices.
                let mut socket = monitor.listen()?;
                let mut fds = vec![
                    ::libc::pollfd {
                        fd: socket.as_raw_fd(),
                        events: POLLIN,
                        revents: 0,
                    },
                ];

                // Loop until we're stopped by the controlling thread, or fail.
                while alive() {
                    // Wait for new events, break on failure.
                    poll(&mut fds)?;

                    // Send the event over.
                    let udev_event = socket.receive_event();
                    if let Some(event) = udev_event.and_then(Event::from_udev) {
                        tx.send(event).map_err(to_io_err)?;
                    }
                }

                Ok(())
            },
            0, /* no timeout */
        )?;

        Ok(Self { rx, thread })
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

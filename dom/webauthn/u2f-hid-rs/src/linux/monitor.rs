/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use libc::{c_int, c_short, c_ulong};
use libudev;
use libudev::EventType;
use runloop::RunLoop;
use std::collections::HashMap;
use std::ffi::OsString;
use std::io;
use std::os::unix::io::AsRawFd;
use std::sync::Arc;

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

pub struct Monitor<F>
where
    F: Fn(OsString, &Fn() -> bool) + Sync,
{
    runloops: HashMap<OsString, RunLoop>,
    new_device_cb: Arc<F>,
}

impl<F> Monitor<F>
where
    F: Fn(OsString, &Fn() -> bool) + Send + Sync + 'static,
{
    pub fn new(new_device_cb: F) -> Self {
        Self {
            runloops: HashMap::new(),
            new_device_cb: Arc::new(new_device_cb),
        }
    }

    pub fn run(&mut self, alive: &Fn() -> bool) -> io::Result<()> {
        let ctx = libudev::Context::new()?;

        let mut enumerator = libudev::Enumerator::new(&ctx)?;
        enumerator.match_subsystem(UDEV_SUBSYSTEM)?;

        // Iterate all existing devices.
        for dev in enumerator.scan_devices()? {
            if let Some(path) = dev.devnode().map(|p| p.to_owned().into_os_string()) {
                self.add_device(path);
            }
        }

        let mut monitor = libudev::Monitor::new(&ctx)?;
        monitor.match_subsystem(UDEV_SUBSYSTEM)?;

        // Start listening for new devices.
        let mut socket = monitor.listen()?;
        let mut fds = vec![::libc::pollfd {
            fd: socket.as_raw_fd(),
            events: POLLIN,
            revents: 0,
        }];

        while alive() {
            // Wait for new events, break on failure.
            poll(&mut fds)?;

            if let Some(event) = socket.receive_event() {
                self.process_event(event);
            }
        }

        // Remove all tracked devices.
        self.remove_all_devices();

        Ok(())
    }

    fn process_event(&mut self, event: libudev::Event) {
        let path = event
            .device()
            .devnode()
            .map(|dn| dn.to_owned().into_os_string());

        match (event.event_type(), path) {
            (EventType::Add, Some(path)) => {
                self.add_device(path);
            }
            (EventType::Remove, Some(path)) => {
                self.remove_device(path);
            }
            _ => { /* ignore other types and failures */ }
        }
    }

    fn add_device(&mut self, path: OsString) {
        let f = self.new_device_cb.clone();
        let key = path.clone();

        let runloop = RunLoop::new(move |alive| {
            if alive() {
                f(path, alive);
            }
        });

        if let Ok(runloop) = runloop {
            self.runloops.insert(key, runloop);
        }
    }

    fn remove_device(&mut self, path: OsString) {
        if let Some(runloop) = self.runloops.remove(&path) {
            runloop.cancel();
        }
    }

    fn remove_all_devices(&mut self) {
        while !self.runloops.is_empty() {
            let path = self.runloops.keys().next().unwrap().clone();
            self.remove_device(path);
        }
    }
}

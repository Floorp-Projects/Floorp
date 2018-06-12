/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate libc;
extern crate log;

use core_foundation::base::TCFType;
use core_foundation_sys::base::*;
use core_foundation_sys::runloop::*;
use platform::iokit::*;
use runloop::RunLoop;
use std::collections::HashMap;
use std::os::raw::c_void;
use std::sync::mpsc::{channel, Receiver, Sender};
use std::{io, slice};
use util::io_err;

struct DeviceData {
    tx: Sender<Vec<u8>>,
    runloop: RunLoop,
}

pub struct Monitor<F>
where
    F: Fn((IOHIDDeviceRef, Receiver<Vec<u8>>), &Fn() -> bool) + Sync,
{
    manager: IOHIDManagerRef,
    // Keep alive until the monitor goes away.
    _matcher: IOHIDDeviceMatcher,
    map: HashMap<IOHIDDeviceRef, DeviceData>,
    new_device_cb: F,
}

impl<F> Monitor<F>
where
    F: Fn((IOHIDDeviceRef, Receiver<Vec<u8>>), &Fn() -> bool) + Sync + 'static,
{
    pub fn new(new_device_cb: F) -> Self {
        let manager = unsafe { IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDManagerOptionNone) };

        // Match FIDO devices only.
        let _matcher = IOHIDDeviceMatcher::new();
        unsafe { IOHIDManagerSetDeviceMatching(manager, _matcher.dict.as_concrete_TypeRef()) };

        Self {
            manager,
            _matcher,
            new_device_cb,
            map: HashMap::new(),
        }
    }

    pub fn start(&mut self) -> io::Result<()> {
        let context = self as *mut Self as *mut c_void;

        unsafe {
            IOHIDManagerRegisterDeviceMatchingCallback(
                self.manager,
                Monitor::<F>::on_device_matching,
                context,
            );
            IOHIDManagerRegisterDeviceRemovalCallback(
                self.manager,
                Monitor::<F>::on_device_removal,
                context,
            );
            IOHIDManagerRegisterInputReportCallback(
                self.manager,
                Monitor::<F>::on_input_report,
                context,
            );

            IOHIDManagerScheduleWithRunLoop(
                self.manager,
                CFRunLoopGetCurrent(),
                kCFRunLoopDefaultMode,
            );

            let rv = IOHIDManagerOpen(self.manager, kIOHIDManagerOptionNone);
            if rv == 0 {
                Ok(())
            } else {
                Err(io_err(&format!("Couldn't open HID Manager, rv={}", rv)))
            }
        }
    }

    pub fn stop(&mut self) {
        // Remove all devices.
        while !self.map.is_empty() {
            let device_ref = *self.map.keys().next().unwrap();
            self.remove_device(&device_ref);
        }

        // Close the manager and its devices.
        unsafe { IOHIDManagerClose(self.manager, kIOHIDManagerOptionNone) };
    }

    fn remove_device(&mut self, device_ref: &IOHIDDeviceRef) {
        if let Some(DeviceData { tx, runloop }) = self.map.remove(device_ref) {
            // Dropping `tx` will make Device::read() fail eventually.
            drop(tx);

            // Wait until the runloop stopped.
            runloop.cancel();
        }
    }

    extern "C" fn on_input_report(
        context: *mut c_void,
        _: IOReturn,
        device_ref: IOHIDDeviceRef,
        _: IOHIDReportType,
        _: u32,
        report: *mut u8,
        report_len: CFIndex,
    ) {
        let this = unsafe { &mut *(context as *mut Self) };
        let mut send_failed = false;

        // Ignore the report if we can't find a device for it.
        if let Some(&DeviceData { ref tx, .. }) = this.map.get(&device_ref) {
            let data = unsafe { slice::from_raw_parts(report, report_len as usize).to_vec() };
            send_failed = tx.send(data).is_err();
        }

        // Remove the device if sending fails.
        if send_failed {
            this.remove_device(&device_ref);
        }
    }

    extern "C" fn on_device_matching(
        context: *mut c_void,
        _: IOReturn,
        _: *mut c_void,
        device_ref: IOHIDDeviceRef,
    ) {
        let this = unsafe { &mut *(context as *mut Self) };

        let (tx, rx) = channel();
        let f = &this.new_device_cb;

        // Create a new per-device runloop.
        let runloop = RunLoop::new(move |alive| {
            // Ensure that the runloop is still alive.
            if alive() {
                f((device_ref, rx), alive);
            }
        });

        if let Ok(runloop) = runloop {
            this.map.insert(device_ref, DeviceData { tx, runloop });
        }
    }

    extern "C" fn on_device_removal(
        context: *mut c_void,
        _: IOReturn,
        _: *mut c_void,
        device_ref: IOHIDDeviceRef,
    ) {
        let this = unsafe { &mut *(context as *mut Self) };
        this.remove_device(&device_ref);
    }
}

impl<F> Drop for Monitor<F>
where
    F: Fn((IOHIDDeviceRef, Receiver<Vec<u8>>), &Fn() -> bool) + Sync,
{
    fn drop(&mut self) {
        unsafe { CFRelease(self.manager as *mut c_void) };
    }
}

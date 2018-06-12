/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![allow(non_snake_case, non_camel_case_types, non_upper_case_globals)]

extern crate core_foundation_sys;
extern crate libc;

use consts::{FIDO_USAGE_U2FHID, FIDO_USAGE_PAGE};
use core_foundation_sys::base::*;
use core_foundation_sys::dictionary::*;
use core_foundation_sys::runloop::*;
use core_foundation_sys::string::*;
use core_foundation::dictionary::*;
use core_foundation::string::*;
use core_foundation::number::*;
use std::os::raw::c_void;
use std::ops::Deref;

type IOOptionBits = u32;

pub type IOReturn = libc::c_int;

pub type IOHIDManagerRef = *mut __IOHIDManager;
pub type IOHIDManagerOptions = IOOptionBits;

pub type IOHIDDeviceCallback = extern "C" fn(
    context: *mut c_void,
    result: IOReturn,
    sender: *mut c_void,
    device: IOHIDDeviceRef,
);

pub type IOHIDReportType = IOOptionBits;
pub type IOHIDReportCallback = extern "C" fn(
    context: *mut c_void,
    result: IOReturn,
    sender: IOHIDDeviceRef,
    report_type: IOHIDReportType,
    report_id: u32,
    report: *mut u8,
    report_len: CFIndex,
);

pub const kIOHIDManagerOptionNone: IOHIDManagerOptions = 0;

pub const kIOHIDReportTypeOutput: IOHIDReportType = 1;

#[repr(C)]
pub struct __IOHIDManager {
    __private: c_void,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Hash, PartialEq, Eq)]
pub struct IOHIDDeviceRef(*const c_void);

unsafe impl Send for IOHIDDeviceRef {}
unsafe impl Sync for IOHIDDeviceRef {}

pub struct SendableRunLoop(CFRunLoopRef);

impl SendableRunLoop {
    pub fn new(runloop: CFRunLoopRef) -> Self {
        // Keep the CFRunLoop alive for as long as we are.
        unsafe { CFRetain(runloop as *mut c_void) };

        SendableRunLoop(runloop)
    }
}

unsafe impl Send for SendableRunLoop {}

impl Deref for SendableRunLoop {
    type Target = CFRunLoopRef;

    fn deref(&self) -> &CFRunLoopRef {
        &self.0
    }
}

impl Drop for SendableRunLoop {
    fn drop(&mut self) {
        unsafe { CFRelease(self.0 as *mut c_void) };
    }
}

#[repr(C)]
pub struct CFRunLoopObserverContext {
    pub version: CFIndex,
    pub info: *mut c_void,
    pub retain: Option<extern "C" fn(info: *const c_void) -> *const c_void>,
    pub release: Option<extern "C" fn(info: *const c_void)>,
    pub copyDescription: Option<extern "C" fn(info: *const c_void) -> CFStringRef>,
}

impl CFRunLoopObserverContext {
    pub fn new(context: *mut c_void) -> Self {
        Self {
            version: 0 as CFIndex,
            info: context,
            retain: None,
            release: None,
            copyDescription: None,
        }
    }
}

pub struct CFRunLoopEntryObserver {
    observer: CFRunLoopObserverRef,
    // Keep alive until the observer goes away.
    context_ptr: *mut CFRunLoopObserverContext,
}

impl CFRunLoopEntryObserver {
    pub fn new(callback: CFRunLoopObserverCallBack, context: *mut c_void) -> Self {
        let context = CFRunLoopObserverContext::new(context);
        let context_ptr = Box::into_raw(Box::new(context));

        let observer = unsafe {
            CFRunLoopObserverCreate(
                kCFAllocatorDefault,
                kCFRunLoopEntry,
                false as Boolean,
                0,
                callback,
                context_ptr,
            )
        };

        Self {
            observer,
            context_ptr,
        }
    }

    pub fn add_to_current_runloop(&self) {
        unsafe {
            CFRunLoopAddObserver(CFRunLoopGetCurrent(), self.observer, kCFRunLoopDefaultMode)
        };
    }
}

impl Drop for CFRunLoopEntryObserver {
    fn drop(&mut self) {
        unsafe {
            CFRelease(self.observer as *mut c_void);

            // Drop the CFRunLoopObserverContext.
            let _ = Box::from_raw(self.context_ptr);
        };
    }
}

pub struct IOHIDDeviceMatcher {
    pub dict: CFDictionary<CFString, CFNumber>,
}

impl IOHIDDeviceMatcher {
    pub fn new() -> Self {
        let dict = CFDictionary::<CFString, CFNumber>::from_CFType_pairs(&[
            (
                CFString::from_static_string("DeviceUsage"),
                CFNumber::from(FIDO_USAGE_U2FHID as i32),
            ),
            (
                CFString::from_static_string("DeviceUsagePage"),
                CFNumber::from(FIDO_USAGE_PAGE as i32),
            ),
            ]);
        Self { dict }
    }
}

#[link(name = "IOKit", kind = "framework")]
extern "C" {
    // CFRunLoop
    pub fn CFRunLoopObserverCreate(
        allocator: CFAllocatorRef,
        activities: CFOptionFlags,
        repeats: Boolean,
        order: CFIndex,
        callout: CFRunLoopObserverCallBack,
        context: *mut CFRunLoopObserverContext,
    ) -> CFRunLoopObserverRef;

    // IOHIDManager
    pub fn IOHIDManagerCreate(
        allocator: CFAllocatorRef,
        options: IOHIDManagerOptions,
    ) -> IOHIDManagerRef;
    pub fn IOHIDManagerSetDeviceMatching(manager: IOHIDManagerRef, matching: CFDictionaryRef);
    pub fn IOHIDManagerRegisterDeviceMatchingCallback(
        manager: IOHIDManagerRef,
        callback: IOHIDDeviceCallback,
        context: *mut c_void,
    );
    pub fn IOHIDManagerRegisterDeviceRemovalCallback(
        manager: IOHIDManagerRef,
        callback: IOHIDDeviceCallback,
        context: *mut c_void,
    );
    pub fn IOHIDManagerRegisterInputReportCallback(
        manager: IOHIDManagerRef,
        callback: IOHIDReportCallback,
        context: *mut c_void,
    );
    pub fn IOHIDManagerOpen(manager: IOHIDManagerRef, options: IOHIDManagerOptions) -> IOReturn;
    pub fn IOHIDManagerClose(manager: IOHIDManagerRef, options: IOHIDManagerOptions) -> IOReturn;
    pub fn IOHIDManagerScheduleWithRunLoop(
        manager: IOHIDManagerRef,
        runLoop: CFRunLoopRef,
        runLoopMode: CFStringRef,
    );

    // IOHIDDevice
    pub fn IOHIDDeviceSetReport(
        device: IOHIDDeviceRef,
        reportType: IOHIDReportType,
        reportID: CFIndex,
        report: *const u8,
        reportLength: CFIndex,
    ) -> IOReturn;
}

////////////////////////////////////////////////////////////////////////
// Tests
////////////////////////////////////////////////////////////////////////

#[cfg(test)]
mod tests {
    use super::*;
    use std::os::raw::c_void;
    use std::ptr;
    use std::sync::mpsc::{channel, Sender};
    use std::thread;

    extern "C" fn observe(_: CFRunLoopObserverRef, _: CFRunLoopActivity, context: *mut c_void) {
        let tx: &Sender<SendableRunLoop> = unsafe { &*(context as *mut _) };

        // Send the current runloop to the receiver to unblock it.
        let _ = tx.send(SendableRunLoop::new(unsafe { CFRunLoopGetCurrent() }));
    }

    #[test]
    fn test_sendable_runloop() {
        let (tx, rx) = channel();

        let thread = thread::spawn(move || {
            // Send the runloop to the owning thread.
            let context = &tx as *const _ as *mut c_void;
            let obs = CFRunLoopEntryObserver::new(observe, context);
            obs.add_to_current_runloop();

            unsafe {
                // We need some source for the runloop to run.
                let manager = IOHIDManagerCreate(kCFAllocatorDefault, 0);
                assert!(!manager.is_null());

                IOHIDManagerScheduleWithRunLoop(
                    manager,
                    CFRunLoopGetCurrent(),
                    kCFRunLoopDefaultMode,
                );
                IOHIDManagerSetDeviceMatching(manager, ptr::null_mut());

                let rv = IOHIDManagerOpen(manager, 0);
                assert_eq!(rv, 0);

                // This will run until `CFRunLoopStop()` is called.
                CFRunLoopRun();

                let rv = IOHIDManagerClose(manager, 0);
                assert_eq!(rv, 0);

                CFRelease(manager as *mut c_void);
            }
        });

        // Block until we enter the CFRunLoop.
        let runloop: SendableRunLoop = rx.recv().expect("failed to receive runloop");

        // Stop the runloop.
        unsafe { CFRunLoopStop(*runloop) };

        // Stop the thread.
        thread.join().expect("failed to join the thread");

        // Try to stop the runloop again (without crashing).
        unsafe { CFRunLoopStop(*runloop) };
    }
}

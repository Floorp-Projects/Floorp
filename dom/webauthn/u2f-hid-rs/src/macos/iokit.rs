/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![allow(non_snake_case, non_camel_case_types, non_upper_case_globals)]

extern crate core_foundation_sys;
extern crate libc;

use core_foundation_sys::base::{Boolean, CFIndex, CFAllocatorRef, CFOptionFlags};
use core_foundation_sys::string::CFStringRef;
use core_foundation_sys::runloop::{CFRunLoopRef, CFRunLoopObserverRef, CFRunLoopObserverCallBack};
use core_foundation_sys::dictionary::CFDictionaryRef;
use libc::c_void;
use std::ops::Deref;

type IOOptionBits = u32;

pub type IOReturn = libc::c_int;

pub type IOHIDManagerRef = *mut __IOHIDManager;
pub type IOHIDManagerOptions = IOOptionBits;

pub type IOHIDDeviceCallback = extern "C" fn(context: *mut c_void,
                                             result: IOReturn,
                                             sender: *mut c_void,
                                             device: IOHIDDeviceRef);

pub type IOHIDReportType = IOOptionBits;
pub type IOHIDReportCallback = extern "C" fn(context: *mut c_void,
                                             result: IOReturn,
                                             sender: IOHIDDeviceRef,
                                             report_type: IOHIDReportType,
                                             report_id: u32,
                                             report: *mut u8,
                                             report_len: CFIndex);

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

pub struct SendableRunLoop(pub CFRunLoopRef);

unsafe impl Send for SendableRunLoop {}

impl Deref for SendableRunLoop {
    type Target = CFRunLoopRef;

    fn deref(&self) -> &CFRunLoopRef {
        &self.0
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

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![allow(non_snake_case, non_camel_case_types, non_upper_case_globals)]

extern crate core_foundation_sys;
extern crate libc;

use libc::c_void;
use core_foundation_sys::base::{CFIndex, CFAllocatorRef};
use core_foundation_sys::string::CFStringRef;
use core_foundation_sys::runloop::CFRunLoopRef;
use core_foundation_sys::dictionary::CFDictionaryRef;

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
                                             sender: *mut c_void,
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

extern "C" {
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
    pub fn IOHIDDeviceRegisterInputReportCallback(
        device: IOHIDDeviceRef,
        report: *const u8,
        reportLength: CFIndex,
        callback: IOHIDReportCallback,
        context: *mut c_void,
    );
}

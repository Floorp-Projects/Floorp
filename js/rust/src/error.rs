/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Functions to throw JavaScript exceptions from Rust.

#![deny(missing_docs)]

use jsapi::root::*;
use libc;
use std::ffi::CString;
use std::{mem, os, ptr};

/// Format string used to throw javascript errors.
static ERROR_FORMAT_STRING_STRING: [libc::c_char; 4] = [
    '{' as libc::c_char,
    '0' as libc::c_char,
    '}' as libc::c_char,
    0 as libc::c_char,
];

/// Format string struct used to throw `TypeError`s.
static mut TYPE_ERROR_FORMAT_STRING: JSErrorFormatString = JSErrorFormatString {
    name: b"RUSTMSG_TYPE_ERROR\0" as *const _ as *const libc::c_char,
    format: &ERROR_FORMAT_STRING_STRING as *const libc::c_char,
    argCount: 1,
    exnType: JSExnType::JSEXN_TYPEERR as i16,
};

/// Format string struct used to throw `RangeError`s.
static mut RANGE_ERROR_FORMAT_STRING: JSErrorFormatString = JSErrorFormatString {
    name: b"RUSTMSG_RANGE_ERROR\0" as *const _ as *const libc::c_char,
    format: &ERROR_FORMAT_STRING_STRING as *const libc::c_char,
    argCount: 1,
    exnType: JSExnType::JSEXN_RANGEERR as i16,
};

/// Callback used to throw javascript errors.
/// See throw_js_error for info about error_number.
unsafe extern "C" fn get_error_message(_user_ref: *mut os::raw::c_void,
                                       error_number: libc::c_uint)
                                       -> *const JSErrorFormatString {
    let num: JSExnType = mem::transmute(error_number);
    match num {
        JSExnType::JSEXN_TYPEERR => &TYPE_ERROR_FORMAT_STRING as *const JSErrorFormatString,
        JSExnType::JSEXN_RANGEERR => &RANGE_ERROR_FORMAT_STRING as *const JSErrorFormatString,
        _ => panic!("Bad js error number given to get_error_message: {}",
                    error_number),
    }
}

/// Helper fn to throw a javascript error with the given message and number.
/// Reuse the jsapi error codes to distinguish the error_number
/// passed back to the get_error_message callback.
/// c_uint is u32, so this cast is safe, as is casting to/from i32 from there.
unsafe fn throw_js_error(cx: *mut JSContext, error: &str, error_number: u32) {
    let error = CString::new(error).unwrap();
    JS_ReportErrorNumberUTF8(cx,
                             Some(get_error_message),
                             ptr::null_mut(),
                             error_number,
                             error.as_ptr());
}

/// Throw a `TypeError` with the given message.
pub unsafe fn throw_type_error(cx: *mut JSContext, error: &str) {
    throw_js_error(cx, error, JSExnType::JSEXN_TYPEERR as u32);
}

/// Throw a `RangeError` with the given message.
pub unsafe fn throw_range_error(cx: *mut JSContext, error: &str) {
    throw_js_error(cx, error, JSExnType::JSEXN_RANGEERR as u32);
}

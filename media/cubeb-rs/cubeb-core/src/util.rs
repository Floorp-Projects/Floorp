// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use std::ffi::CStr;
use std::os::raw::c_char;

pub unsafe fn opt_bytes<T>(_anchor: &T, c: *const c_char) -> Option<&[u8]> {
    if c.is_null() {
        None
    } else {
        Some(CStr::from_ptr(c).to_bytes())
    }
}

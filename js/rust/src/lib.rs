/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#![crate_name = "js"]
#![crate_type = "rlib"]

#![cfg_attr(feature = "nonzero", feature(nonzero))]

#![allow(non_upper_case_globals, non_camel_case_types, non_snake_case, improper_ctypes)]

#[cfg(feature = "nonzero")]
extern crate core;
#[macro_use]
extern crate lazy_static;
extern crate libc;
#[macro_use]
extern crate log;
#[allow(unused_extern_crates)]
extern crate mozjs_sys;
extern crate num_traits;

#[macro_use]
pub mod rust;

pub mod ac;
pub mod conversions;
pub mod error;
pub mod glue;
pub mod heap;
pub mod jsval;
pub mod panic;
pub mod sc;
pub mod typedarray;

pub mod jsapi;
use self::jsapi::root::*;

#[inline(always)]
pub unsafe fn JS_ARGV(_cx: *mut JSContext, vp: *mut JS::Value) -> *mut JS::Value {
    vp.offset(2)
}

impl JS::ObjectOpResult {
    /// Set this ObjectOpResult to true and return true.
    pub fn succeed(&mut self) -> bool {
        self.code_ = JS::ObjectOpResult_SpecialCodes::OkCode as usize;
        true
    }
}

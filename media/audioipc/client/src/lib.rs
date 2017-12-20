// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

extern crate audioipc;
extern crate cubeb_core;
#[macro_use]
extern crate cubeb_backend;
extern crate libc;
#[macro_use]
extern crate log;

#[macro_use]
mod send_recv;
mod context;
mod stream;

use context::ClientContext;
use cubeb_backend::capi;
use cubeb_core::ffi;
use std::os::raw::{c_char, c_int};
use std::os::unix::io::RawFd;
use stream::ClientStream;

thread_local!(static IN_CALLBACK: std::cell::RefCell<bool> = std::cell::RefCell::new(false));

fn set_in_callback(in_callback: bool) {
    IN_CALLBACK.with(|b| {
        assert_eq!(*b.borrow(), !in_callback);
        *b.borrow_mut() = in_callback;
    });
}

fn assert_not_in_callback() {
    IN_CALLBACK.with(|b| {
        assert_eq!(*b.borrow(), false);
    });
}

static mut G_SERVER_FD: Option<RawFd> = None;

#[no_mangle]
/// Entry point from C code.
pub unsafe extern "C" fn audioipc_client_init(c: *mut *mut ffi::cubeb, context_name: *const c_char, server_connection: c_int) -> c_int {
    // TODO: Windows portability (for fd).
    // TODO: Better way to pass extra parameters to Context impl.
    if G_SERVER_FD.is_some() {
        panic!("audioipc client's server connection already initialized.");
    }
    if server_connection >= 0 {
        G_SERVER_FD = Some(server_connection);
    }
    capi::capi_init::<ClientContext>(c, context_name)
}

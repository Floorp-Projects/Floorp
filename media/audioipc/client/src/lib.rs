// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

extern crate audioipc;
#[macro_use]
extern crate cubeb_backend;
extern crate foreign_types;
extern crate futures;
extern crate futures_cpupool;
extern crate libc;
#[macro_use]
extern crate log;
extern crate tokio_core;
extern crate tokio_uds;

#[macro_use]
mod send_recv;
mod context;
mod stream;

use context::ClientContext;
use cubeb_backend::{capi, ffi};
use std::os::raw::{c_char, c_int};
use std::os::unix::io::RawFd;
use stream::ClientStream;

type InitParamsTls = std::cell::RefCell<Option<CpuPoolInitParams>>;

thread_local!(static IN_CALLBACK: std::cell::RefCell<bool> = std::cell::RefCell::new(false));
thread_local!(static CPUPOOL_INIT_PARAMS: InitParamsTls = std::cell::RefCell::new(None));

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AudioIpcInitParams {
    pub server_connection: c_int,
    pub pool_size: usize,
    pub stack_size: usize,
}

#[derive(Clone, Copy, Debug)]
struct CpuPoolInitParams {
    pub pool_size: usize,
    pub stack_size: usize,
}

impl CpuPoolInitParams {
    pub fn init_with(params: &AudioIpcInitParams) -> Self {
        CpuPoolInitParams {
            pool_size: params.pool_size,
            stack_size: params.stack_size,
        }
    }
}

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

fn set_cpupool_init_params<P>(params: P)
where
    P: Into<Option<CpuPoolInitParams>>,
{
    CPUPOOL_INIT_PARAMS.with(|p| {
        *p.borrow_mut() = params.into();
    });
}

static mut G_SERVER_FD: Option<RawFd> = None;

#[no_mangle]
/// Entry point from C code.
pub unsafe extern "C" fn audioipc_client_init(
    c: *mut *mut ffi::cubeb,
    context_name: *const c_char,
    init_params: *const AudioIpcInitParams,
) -> c_int {
    if init_params.is_null() {
        return cubeb_backend::ffi::CUBEB_ERROR;
    }

    let init_params = &*init_params;

    // TODO: Windows portability (for fd).
    // TODO: Better way to pass extra parameters to Context impl.
    if G_SERVER_FD.is_some() {
        panic!("audioipc client's server connection already initialized.");
    }
    if init_params.server_connection >= 0 {
        G_SERVER_FD = Some(init_params.server_connection);
    }

    let cpupool_init_params = CpuPoolInitParams::init_with(&init_params);
    set_cpupool_init_params(cpupool_init_params);
    capi::capi_init::<ClientContext>(c, context_name)
}

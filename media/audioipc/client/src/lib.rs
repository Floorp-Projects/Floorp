// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.
#![warn(unused_extern_crates)]

#[macro_use]
extern crate cubeb_backend;
#[macro_use]
extern crate log;
#[macro_use]
extern crate lazy_static;

#[macro_use]
mod send_recv;
mod context;
mod stream;

use crate::context::ClientContext;
use crate::stream::ClientStream;
use audio_thread_priority::RtPriorityHandle;
use audioipc::{PlatformHandle, PlatformHandleType};
use cubeb_backend::{capi, ffi};
use futures_cpupool::CpuPool;
#[cfg(target_os = "linux")]
use std::ffi::CString;
use std::os::raw::{c_char, c_int};
use std::sync::Mutex;
#[cfg(target_os = "linux")]
use std::sync::{Arc, Condvar};
#[cfg(target_os = "linux")]
use std::thread;

type InitParamsTls = std::cell::RefCell<Option<CpuPoolInitParams>>;

thread_local!(static IN_CALLBACK: std::cell::RefCell<bool> = std::cell::RefCell::new(false));
thread_local!(static CPUPOOL_INIT_PARAMS: InitParamsTls = std::cell::RefCell::new(None));
thread_local!(static G_PRIORITY_HANDLES: std::cell::RefCell<Vec<RtPriorityHandle>> = std::cell::RefCell::new(vec![]));

lazy_static! {
    static ref G_THREAD_POOL: Mutex<Option<CpuPool>> = Mutex::new(None);
}

// This must match the definition of AudioIpcInitParams in
// dom/media/CubebUtils.cpp in Gecko.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct AudioIpcInitParams {
    // Fields only need to be public for ipctest.
    pub server_connection: PlatformHandleType,
    pub pool_size: usize,
    pub stack_size: usize,
    pub thread_create_callback: Option<extern "C" fn(*const ::std::os::raw::c_char)>,
}

#[derive(Clone, Copy, Debug)]
struct CpuPoolInitParams {
    pool_size: usize,
    stack_size: usize,
    thread_create_callback: Option<extern "C" fn(*const ::std::os::raw::c_char)>,
}

impl CpuPoolInitParams {
    fn init_with(params: &AudioIpcInitParams) -> Self {
        CpuPoolInitParams {
            pool_size: params.pool_size,
            stack_size: params.stack_size,
            thread_create_callback: params.thread_create_callback,
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

static mut G_SERVER_FD: Option<PlatformHandle> = None;

#[cfg(target_os = "linux")]
#[no_mangle]
pub unsafe extern "C" fn audioipc_init_threads(init_params: *const AudioIpcInitParams) {
    let thread_create_callback = (*init_params).thread_create_callback;

    // It is critical that this function waits until the various threads are created, promoted to
    // real-time, and _then_ return, because the sandbox lockdown happens right after returning
    // from here.
    let pair = Arc::new((Mutex::new((*init_params).pool_size), Condvar::new()));
    let pair2 = pair.clone();

    let register_thread = move || {
        if let Some(func) = thread_create_callback {
            let thr = thread::current();
            let name = CString::new(thr.name().unwrap()).unwrap();
            func(name.as_ptr());
            let &(ref lock, ref cvar) = &*pair2;
            let mut count = lock.lock().unwrap();
            *count -= 1;
            cvar.notify_one();
        }
    };

    let mut pool = G_THREAD_POOL.lock().unwrap();

    *pool = Some(
        futures_cpupool::Builder::new()
            .name_prefix("AudioIPC")
            .after_start(register_thread)
            .pool_size((*init_params).pool_size)
            .stack_size((*init_params).stack_size)
            .create(),
    );

    let &(ref lock, ref cvar) = &*pair;
    let mut count = lock.lock().unwrap();
    while *count != 0 {
        count = cvar.wait(count).unwrap();
    }
}

#[cfg(not(target_os = "linux"))]
#[no_mangle]
pub unsafe extern "C" fn audioipc_init_threads(_: *const AudioIpcInitParams) {
    unimplemented!();
}

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

    // TODO: Better way to pass extra parameters to Context impl.
    if G_SERVER_FD.is_some() {
        return cubeb_backend::ffi::CUBEB_ERROR;
    }
    G_SERVER_FD = PlatformHandle::try_new(init_params.server_connection);
    if G_SERVER_FD.is_none() {
        return cubeb_backend::ffi::CUBEB_ERROR;
    }

    let cpupool_init_params = CpuPoolInitParams::init_with(&init_params);
    set_cpupool_init_params(cpupool_init_params);
    capi::capi_init::<ClientContext>(c, context_name)
}

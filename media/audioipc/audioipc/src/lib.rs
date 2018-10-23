// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

#![recursion_limit = "1024"]
#[macro_use]
extern crate error_chain;

#[macro_use]
extern crate log;

#[macro_use]
extern crate serde_derive;

extern crate bincode;
extern crate bytes;
extern crate cubeb;
#[macro_use]
extern crate futures;
extern crate iovec;
extern crate libc;
extern crate memmap;
#[macro_use]
extern crate scoped_tls;
extern crate serde;
extern crate tokio_core;
#[macro_use]
extern crate tokio_io;
extern crate tokio_uds;

mod async;
mod cmsg;
pub mod codec;
pub mod core;
pub mod errors;
pub mod fd_passing;
pub mod frame;
pub mod messages;
mod msg;
pub mod rpc;
pub mod shm;

pub use messages::{ClientMessage, ServerMessage};
use std::env::temp_dir;
use std::path::PathBuf;

// This must match the definition of
// ipc::FileDescriptor::PlatformHandleType in Gecko.
#[cfg(target_os = "windows")]
pub type PlatformHandleType = *mut std::os::raw::c_void;
#[cfg(not(target_os = "windows"))]
pub type PlatformHandleType = libc::c_int;

pub fn get_shm_path(dir: &str) -> PathBuf {
    let pid = unsafe { libc::getpid() };
    let mut temp = temp_dir();
    temp.push(&format!("cubeb-shm-{}-{}", pid, dir));
    temp
}

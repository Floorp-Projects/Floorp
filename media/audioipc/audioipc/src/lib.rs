// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details
#![warn(unused_extern_crates)]
#![recursion_limit = "1024"]
#[macro_use]
extern crate error_chain;
#[macro_use]
extern crate log;
#[macro_use]
extern crate serde_derive;
#[macro_use]
extern crate futures;
#[macro_use]
extern crate tokio_io;

mod async_msg;
#[cfg(unix)]
mod cmsg;
pub mod codec;
pub mod core;
#[allow(deprecated)]
pub mod errors;
#[cfg(unix)]
pub mod fd_passing;
#[cfg(unix)]
pub use crate::fd_passing as platformhandle_passing;
#[cfg(windows)]
pub mod handle_passing;
#[cfg(windows)]
pub use handle_passing as platformhandle_passing;
pub mod frame;
pub mod messages;
#[cfg(unix)]
mod msg;
pub mod rpc;
pub mod shm;

// TODO: Remove local fork when https://github.com/tokio-rs/tokio/pull/1294 is resolved.
#[cfg(unix)]
mod tokio_uds_stream;

pub use crate::messages::{ClientMessage, ServerMessage};
use std::env::temp_dir;
use std::path::PathBuf;

// TODO: Remove hardcoded size and allow allocation based on cubeb backend requirements.
pub const SHM_AREA_SIZE: usize = 2 * 1024 * 1024;

#[cfg(unix)]
use std::os::unix::io::{FromRawFd, IntoRawFd};
#[cfg(windows)]
use std::os::windows::io::{FromRawHandle, IntoRawHandle};

// This must match the definition of
// ipc::FileDescriptor::PlatformHandleType in Gecko.
#[cfg(windows)]
pub type PlatformHandleType = std::os::windows::raw::HANDLE;
#[cfg(unix)]
pub type PlatformHandleType = libc::c_int;

// This stands in for RawFd/RawHandle.
#[derive(Copy, Clone, Debug)]
pub struct PlatformHandle(PlatformHandleType);

unsafe impl Send for PlatformHandle {}

// Custom serialization to treat HANDLEs as i64.  This is not valid in
// general, but after sending the HANDLE value to a remote process we
// use it to create a valid HANDLE via DuplicateHandle.
// To avoid duplicating the serialization code, we're lazy and treat
// file descriptors as i64 rather than i32.
impl serde::Serialize for PlatformHandle {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        serializer.serialize_i64(self.0 as i64)
    }
}

struct PlatformHandleVisitor;
impl<'de> serde::de::Visitor<'de> for PlatformHandleVisitor {
    type Value = PlatformHandle;

    fn expecting(&self, formatter: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        formatter.write_str("an integer between -2^63 and 2^63")
    }

    fn visit_i64<E>(self, value: i64) -> Result<Self::Value, E>
    where
        E: serde::de::Error,
    {
        Ok(PlatformHandle::new(value as PlatformHandleType))
    }
}

impl<'de> serde::Deserialize<'de> for PlatformHandle {
    fn deserialize<D>(deserializer: D) -> Result<PlatformHandle, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        deserializer.deserialize_i64(PlatformHandleVisitor)
    }
}

#[cfg(unix)]
fn valid_handle(handle: PlatformHandleType) -> bool {
    handle >= 0
}

#[cfg(windows)]
fn valid_handle(handle: PlatformHandleType) -> bool {
    const INVALID_HANDLE_VALUE: PlatformHandleType = -1isize as PlatformHandleType;
    const NULL_HANDLE_VALUE: PlatformHandleType = 0isize as PlatformHandleType;
    handle != INVALID_HANDLE_VALUE && handle != NULL_HANDLE_VALUE
}

impl PlatformHandle {
    pub fn new(raw: PlatformHandleType) -> PlatformHandle {
        PlatformHandle(raw)
    }

    pub fn try_new(raw: PlatformHandleType) -> Option<PlatformHandle> {
        if !valid_handle(raw) {
            return None;
        }
        Some(PlatformHandle::new(raw))
    }

    #[cfg(windows)]
    pub fn from<T: IntoRawHandle>(from: T) -> PlatformHandle {
        PlatformHandle::new(from.into_raw_handle())
    }

    #[cfg(unix)]
    pub fn from<T: IntoRawFd>(from: T) -> PlatformHandle {
        PlatformHandle::new(from.into_raw_fd())
    }

    #[cfg(windows)]
    pub unsafe fn into_file(self) -> std::fs::File {
        std::fs::File::from_raw_handle(self.0)
    }

    #[cfg(unix)]
    pub unsafe fn into_file(self) -> std::fs::File {
        std::fs::File::from_raw_fd(self.0)
    }

    pub fn as_raw(self) -> PlatformHandleType {
        self.0
    }

    pub unsafe fn close(self) {
        close_platformhandle(self.0);
    }
}

#[cfg(unix)]
unsafe fn close_platformhandle(handle: PlatformHandleType) {
    libc::close(handle);
}

#[cfg(windows)]
unsafe fn close_platformhandle(handle: PlatformHandleType) {
    winapi::um::handleapi::CloseHandle(handle);
}

pub fn get_shm_path(dir: &str) -> PathBuf {
    let pid = std::process::id();
    let mut temp = temp_dir();
    temp.push(&format!("cubeb-shm-{}-{}", pid, dir));
    temp
}

#[cfg(unix)]
pub mod messagestream_unix;
#[cfg(unix)]
pub use crate::messagestream_unix::*;

#[cfg(windows)]
pub mod messagestream_win;
#[cfg(windows)]
pub use messagestream_win::*;

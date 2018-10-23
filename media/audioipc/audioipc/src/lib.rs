// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details
#![allow(dead_code)] // TODO: Remove.
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

pub mod async;
pub mod cmsg;
pub mod codec;
pub mod errors;
pub mod fd_passing;
pub mod frame;
pub mod rpc;
pub mod core;
pub mod messages;
mod msg;
pub mod shm;

use iovec::IoVec;
#[cfg(target_os = "linux")]
use libc::MSG_CMSG_CLOEXEC;
pub use messages::{ClientMessage, ServerMessage};
use std::env::temp_dir;
use std::io;
use std::os::unix::io::{AsRawFd, FromRawFd, IntoRawFd, RawFd};
use std::path::PathBuf;
#[cfg(not(target_os = "linux"))]
const MSG_CMSG_CLOEXEC: libc::c_int = 0;

// This must match the definition of
// ipc::FileDescriptor::PlatformHandleType in Gecko.
#[cfg(target_os = "windows")]
pub type PlatformHandleType = *mut std::os::raw::c_void;
#[cfg(not(target_os = "windows"))]
pub type PlatformHandleType = libc::c_int;

// Extend sys::os::unix::net::UnixStream to support sending and receiving a single file desc.
// We can extend UnixStream by using traits, eliminating the need to introduce a new wrapped
// UnixStream type.
pub trait RecvMsg {
    fn recv_msg(
        &mut self,
        iov: &mut [&mut IoVec],
        cmsg: &mut [u8],
    ) -> io::Result<(usize, usize, i32)>;
}

pub trait SendMsg {
    fn send_msg(&mut self, iov: &[&IoVec], cmsg: &[u8]) -> io::Result<usize>;
}

impl<T: AsRawFd> RecvMsg for T {
    fn recv_msg(
        &mut self,
        iov: &mut [&mut IoVec],
        cmsg: &mut [u8],
    ) -> io::Result<(usize, usize, i32)> {
        msg::recv_msg_with_flags(self.as_raw_fd(), iov, cmsg, MSG_CMSG_CLOEXEC)
    }
}

impl<T: AsRawFd> SendMsg for T {
    fn send_msg(&mut self, iov: &[&IoVec], cmsg: &[u8]) -> io::Result<usize> {
        msg::send_msg_with_flags(self.as_raw_fd(), iov, cmsg, 0)
    }
}

////////////////////////////////////////////////////////////////////////////////

#[derive(Debug)]
pub struct AutoCloseFd(RawFd);

impl Drop for AutoCloseFd {
    fn drop(&mut self) {
        unsafe {
            libc::close(self.0);
        }
    }
}

impl FromRawFd for AutoCloseFd {
    unsafe fn from_raw_fd(fd: RawFd) -> Self {
        AutoCloseFd(fd)
    }
}

impl IntoRawFd for AutoCloseFd {
    fn into_raw_fd(self) -> RawFd {
        let fd = self.0;
        ::std::mem::forget(self);
        fd
    }
}

impl AsRawFd for AutoCloseFd {
    fn as_raw_fd(&self) -> RawFd {
        self.0
    }
}

impl<'a> AsRawFd for &'a AutoCloseFd {
    fn as_raw_fd(&self) -> RawFd {
        self.0
    }
}

////////////////////////////////////////////////////////////////////////////////

pub fn get_shm_path(dir: &str) -> PathBuf {
    let pid = unsafe { libc::getpid() };
    let mut temp = temp_dir();
    temp.push(&format!("cubeb-shm-{}-{}", pid, dir));
    temp
}

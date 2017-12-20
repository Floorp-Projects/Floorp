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
extern crate cubeb_core;
extern crate libc;
extern crate memmap;
extern crate mio;
extern crate mio_uds;
extern crate serde;

pub mod async;
pub mod codec;
mod connection;
pub mod errors;
pub mod messages;
mod msg;
pub mod shm;

pub use connection::*;
pub use messages::{ClientMessage, ServerMessage};

use std::env::temp_dir;
use std::io;
use std::os::unix::io::{AsRawFd, FromRawFd, IntoRawFd, RawFd};
use std::os::unix::net;
use std::path::PathBuf;

// Extend sys::os::unix::net::UnixStream to support sending and receiving a single file desc.
// We can extend UnixStream by using traits, eliminating the need to introduce a new wrapped
// UnixStream type.
pub trait RecvFd {
    fn recv_fd(&mut self, bytes: &mut [u8]) -> io::Result<(usize, Option<RawFd>)>;
}

pub trait SendFd {
    fn send_fd(&mut self, bytes: &[u8], fd: Option<RawFd>) -> io::Result<(usize)>;
}

impl RecvFd for net::UnixStream {
    fn recv_fd(&mut self, buf_to_recv: &mut [u8]) -> io::Result<(usize, Option<RawFd>)> {
        msg::recvmsg(self.as_raw_fd(), buf_to_recv)
    }
}

impl RecvFd for mio_uds::UnixStream {
    fn recv_fd(&mut self, buf_to_recv: &mut [u8]) -> io::Result<(usize, Option<RawFd>)> {
        msg::recvmsg(self.as_raw_fd(), buf_to_recv)
    }
}

impl SendFd for net::UnixStream {
    fn send_fd(&mut self, buf_to_send: &[u8], fd_to_send: Option<RawFd>) -> io::Result<usize> {
        msg::sendmsg(self.as_raw_fd(), buf_to_send, fd_to_send)
    }
}

impl SendFd for mio_uds::UnixStream {
    fn send_fd(&mut self, buf_to_send: &[u8], fd_to_send: Option<RawFd>) -> io::Result<usize> {
        msg::sendmsg(self.as_raw_fd(), buf_to_send, fd_to_send)
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

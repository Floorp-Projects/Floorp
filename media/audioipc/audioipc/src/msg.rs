// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use iovec::unix;
use iovec::IoVec;
use libc;
use std::os::unix::io::{AsRawFd, RawFd};
use std::{cmp, io, mem, ptr};

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
        #[cfg(target_os = "linux")]
        let flags = libc::MSG_CMSG_CLOEXEC;
        #[cfg(not(target_os = "linux"))]
        let flags = 0;
        recv_msg_with_flags(self.as_raw_fd(), iov, cmsg, flags)
    }
}

impl<T: AsRawFd> SendMsg for T {
    fn send_msg(&mut self, iov: &[&IoVec], cmsg: &[u8]) -> io::Result<usize> {
        send_msg_with_flags(self.as_raw_fd(), iov, cmsg, 0)
    }
}

fn cvt(r: libc::ssize_t) -> io::Result<usize> {
    if r == -1 {
        Err(io::Error::last_os_error())
    } else {
        Ok(r as usize)
    }
}

// Convert return of -1 into error message, handling retry on EINTR
fn cvt_r<F: FnMut() -> libc::ssize_t>(mut f: F) -> io::Result<usize> {
    loop {
        match cvt(f()) {
            Err(ref e) if e.kind() == io::ErrorKind::Interrupted => {}
            other => return other,
        }
    }
}

pub fn recv_msg_with_flags(
    socket: RawFd,
    bufs: &mut [&mut IoVec],
    cmsg: &mut [u8],
    flags: libc::c_int,
) -> io::Result<(usize, usize, libc::c_int)> {
    let slice = unix::as_os_slice_mut(bufs);
    let len = cmp::min(<libc::c_int>::max_value() as usize, slice.len());
    let (control, controllen) = if cmsg.is_empty() {
        (ptr::null_mut(), 0)
    } else {
        (cmsg.as_ptr() as *mut _, cmsg.len())
    };

    let mut msghdr: libc::msghdr = unsafe { mem::zeroed() };
    msghdr.msg_name = ptr::null_mut();
    msghdr.msg_namelen = 0;
    msghdr.msg_iov = slice.as_mut_ptr();
    msghdr.msg_iovlen = len as _;
    msghdr.msg_control = control;
    msghdr.msg_controllen = controllen as _;

    let n = cvt_r(|| unsafe { libc::recvmsg(socket, &mut msghdr as *mut _, flags) })?;

    let controllen = msghdr.msg_controllen as usize;
    Ok((n, controllen, msghdr.msg_flags))
}

pub fn send_msg_with_flags(
    socket: RawFd,
    bufs: &[&IoVec],
    cmsg: &[u8],
    flags: libc::c_int,
) -> io::Result<usize> {
    let slice = unix::as_os_slice(bufs);
    let len = cmp::min(<libc::c_int>::max_value() as usize, slice.len());
    let (control, controllen) = if cmsg.is_empty() {
        (ptr::null_mut(), 0)
    } else {
        (cmsg.as_ptr() as *mut _, cmsg.len())
    };

    let mut msghdr: libc::msghdr = unsafe { mem::zeroed() };
    msghdr.msg_name = ptr::null_mut();
    msghdr.msg_namelen = 0;
    msghdr.msg_iov = slice.as_ptr() as *mut _;
    msghdr.msg_iovlen = len as _;
    msghdr.msg_control = control;
    msghdr.msg_controllen = controllen as _;

    cvt_r(|| unsafe { libc::sendmsg(socket, &msghdr as *const _, flags) })
}

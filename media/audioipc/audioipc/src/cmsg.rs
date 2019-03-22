// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

use bytes::{BufMut, Bytes, BytesMut};
use libc::{self, cmsghdr};
use std::os::unix::io::RawFd;
use std::{convert, mem, ops, slice};

#[derive(Clone, Debug)]
pub struct Fds {
    fds: Bytes,
}

impl convert::AsRef<[RawFd]> for Fds {
    fn as_ref(&self) -> &[RawFd] {
        let n = self.fds.len() / mem::size_of::<RawFd>();
        unsafe { slice::from_raw_parts(self.fds.as_ptr() as *const _, n) }
    }
}

impl ops::Deref for Fds {
    type Target = [RawFd];

    #[inline]
    fn deref(&self) -> &[RawFd] {
        self.as_ref()
    }
}

pub struct ControlMsgIter {
    control: Bytes,
}

pub fn iterator(c: Bytes) -> ControlMsgIter {
    ControlMsgIter { control: c }
}

impl Iterator for ControlMsgIter {
    type Item = Fds;

    // This follows the logic in __cmsg_nxthdr from glibc
    // /usr/include/bits/socket.h
    fn next(&mut self) -> Option<Self::Item> {
        loop {
            let control = self.control.clone();
            let cmsghdr_len = align(mem::size_of::<cmsghdr>());

            if control.len() < cmsghdr_len {
                // No more entries---not enough data in `control` for a
                // complete message.
                return None;
            }

            let cmsg: &cmsghdr = unsafe { &*(control.as_ptr() as *const _) };
            // The offset to the next cmsghdr in control.  This must be
            // aligned to a boundary that matches the type used to
            // represent the length of the message.
            let cmsg_len = cmsg.cmsg_len;
            let next_cmsghdr = align(cmsg_len as _);
            self.control = if next_cmsghdr > control.len() {
                // No more entries---not enough data in `control` for a
                // complete message.
                Bytes::new()
            } else {
                control.slice_from(next_cmsghdr)
            };

            match (cmsg.cmsg_level, cmsg.cmsg_type) {
                (libc::SOL_SOCKET, libc::SCM_RIGHTS) => {
                    trace!("Found SCM_RIGHTS...");
                    return Some(Fds {
                        fds: control.slice(cmsghdr_len, cmsg_len as _),
                    });
                }
                (level, kind) => {
                    trace!("Skipping cmsg level, {}, type={}...", level, kind);
                }
            }
        }
    }
}

#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub enum Error {
    /// Not enough space in storage to insert control mesage.
    NoSpace,
}

#[must_use]
pub struct ControlMsgBuilder {
    result: Result<BytesMut, Error>,
}

pub fn builder(buf: &mut BytesMut) -> ControlMsgBuilder {
    let buf = aligned(buf);
    ControlMsgBuilder { result: Ok(buf) }
}

impl ControlMsgBuilder {
    fn msg(mut self, level: libc::c_int, kind: libc::c_int, msg: &[u8]) -> Self {
        self.result = self.result.and_then(align_buf).and_then(|mut cmsg| {
            let cmsg_len = len(msg.len());
            if cmsg.remaining_mut() < cmsg_len {
                return Err(Error::NoSpace);
            }

            // Some definitions of cmsghdr contain padding.  Rather
            // than try to keep an up-to-date #cfg list to handle
            // that, just use a pre-zeroed struct to fill out any
            // fields we don't care about.
            let zeroed = unsafe { mem::zeroed() };
            let cmsghdr = cmsghdr {
                cmsg_len: cmsg_len as _,
                cmsg_level: level,
                cmsg_type: kind,
                ..zeroed
            };

            let cmsghdr = unsafe {
                slice::from_raw_parts(&cmsghdr as *const _ as *const _, mem::size_of::<cmsghdr>())
            };
            cmsg.put_slice(cmsghdr);
            let mut cmsg = try!(align_buf(cmsg));
            cmsg.put_slice(msg);

            Ok(cmsg)
        });

        self
    }

    pub fn rights(self, fds: &[RawFd]) -> Self {
        self.msg(libc::SOL_SOCKET, libc::SCM_RIGHTS, fds.as_bytes())
    }

    pub fn finish(self) -> Result<Bytes, Error> {
        self.result.map(|mut cmsg| cmsg.take().freeze())
    }
}

pub trait AsBytes {
    fn as_bytes(&self) -> &[u8];
}

impl<'a, T: Sized> AsBytes for &'a [T] {
    fn as_bytes(&self) -> &[u8] {
        // TODO: This should account for the alignment of T
        let byte_count = self.len() * mem::size_of::<T>();
        unsafe { slice::from_raw_parts(self.as_ptr() as *const _, byte_count) }
    }
}

fn aligned(buf: &BytesMut) -> BytesMut {
    let mut aligned_buf = buf.clone();
    aligned_buf.reserve(buf.remaining_mut());
    let cmsghdr_align = mem::align_of::<cmsghdr>();
    let n = unsafe { aligned_buf.bytes_mut().as_ptr() } as usize & (cmsghdr_align - 1);
    if n != 0 {
        unsafe { aligned_buf.advance_mut(n) };
        drop(aligned_buf.take());
    }
    aligned_buf
}

fn align_buf(mut cmsg: BytesMut) -> Result<BytesMut, Error> {
    let offset = unsafe { cmsg.bytes_mut().as_ptr() } as usize;
    let adjust = align(offset) - offset;
    if cmsg.remaining_mut() < adjust {
        return Err(Error::NoSpace);
    }

    for _ in 0..adjust {
        cmsg.put_u8(0);
    }
    Ok(cmsg)
}

fn align(len: usize) -> usize {
    let cmsghdr_align = mem::align_of::<cmsghdr>();
    (len + cmsghdr_align - 1) & !(cmsghdr_align - 1)
}

pub fn len(len: usize) -> usize {
    align(mem::size_of::<cmsghdr>()) + len
}

pub fn space(len: usize) -> usize {
    align(mem::size_of::<cmsghdr>()) + align(len)
}

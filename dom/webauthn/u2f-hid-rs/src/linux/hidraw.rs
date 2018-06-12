/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate libc;

use std::io;
use std::mem;
use std::os::unix::io::RawFd;

use hidproto::*;
use util::{from_unix_result, io_err};

#[allow(non_camel_case_types)]
#[repr(C)]
pub struct LinuxReportDescriptor {
    size: ::libc::c_int,
    value: [u8; 4096],
}

const NRBITS: u32 = 8;
const TYPEBITS: u32 = 8;

const READ: u8 = 2;
const SIZEBITS: u8 = 14;

const NRSHIFT: u32 = 0;
const TYPESHIFT: u32 = NRSHIFT + NRBITS as u32;
const SIZESHIFT: u32 = TYPESHIFT + TYPEBITS as u32;
const DIRSHIFT: u32 = SIZESHIFT + SIZEBITS as u32;

// https://github.com/torvalds/linux/blob/master/include/uapi/linux/hid.h
const HID_MAX_DESCRIPTOR_SIZE: usize = 4096;

macro_rules! ioctl {
    ($dir:expr, $name:ident, $ioty:expr, $nr:expr; $ty:ty) => {
        pub unsafe fn $name(fd: libc::c_int, val: *mut $ty) -> io::Result<libc::c_int> {
            let size = mem::size_of::<$ty>();
            let ioc = (($dir as u32) << DIRSHIFT)
                | (($ioty as u32) << TYPESHIFT)
                | (($nr as u32) << NRSHIFT)
                | ((size as u32) << SIZESHIFT);

            #[cfg(not(target_env = "musl"))]
            type IocType = libc::c_ulong;
            #[cfg(target_env = "musl")]
            type IocType = libc::c_int;

            from_unix_result(libc::ioctl(fd, ioc as IocType, val))
        }
    };
}

// https://github.com/torvalds/linux/blob/master/include/uapi/linux/hidraw.h
ioctl!(READ, hidiocgrdescsize, b'H', 0x01; ::libc::c_int);
ioctl!(READ, hidiocgrdesc, b'H', 0x02; /*struct*/ LinuxReportDescriptor);

pub fn is_u2f_device(fd: RawFd) -> bool {
    match read_report_descriptor(fd) {
        Ok(desc) => has_fido_usage(desc),
        Err(_) => false, // Upon failure, just say it's not a U2F device.
    }
}

fn read_report_descriptor(fd: RawFd) -> io::Result<ReportDescriptor> {
    let mut desc = LinuxReportDescriptor {
        size: 0,
        value: [0; HID_MAX_DESCRIPTOR_SIZE],
    };

    let _ = unsafe { hidiocgrdescsize(fd, &mut desc.size)? };
    if desc.size == 0 || desc.size as usize > desc.value.len() {
        return Err(io_err("unexpected hidiocgrdescsize() result"));
    }

    let _ = unsafe { hidiocgrdesc(fd, &mut desc)? };
    let mut value = Vec::from(&desc.value[..]);
    value.truncate(desc.size as usize);
    Ok(ReportDescriptor { value })
}

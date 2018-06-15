/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate libc;

use std::io;
use std::os::unix::io::RawFd;
use std::ptr;

use hidproto::*;
use util::from_unix_result;

#[allow(non_camel_case_types)]
#[repr(C)]
#[derive(Debug)]
pub struct GenDescriptor {
    ugd_data: *mut u8,
    ugd_lang_id: u16,
    ugd_maxlen: u16,
    ugd_actlen: u16,
    ugd_offset: u16,
    ugd_config_index: u8,
    ugd_string_index: u8,
    ugd_iface_index: u8,
    ugd_altif_index: u8,
    ugd_endpt_index: u8,
    ugd_report_index: u8,
    reserved: [u8; 16],
}

impl Default for GenDescriptor {
    fn default() -> GenDescriptor {
        GenDescriptor {
            ugd_data: ptr::null_mut(),
            ugd_lang_id: 0,
            ugd_maxlen: 65535,
            ugd_actlen: 0,
            ugd_offset: 0,
            ugd_config_index: 0,
            ugd_string_index: 0,
            ugd_iface_index: 0,
            ugd_altif_index: 0,
            ugd_endpt_index: 0,
            ugd_report_index: 0,
            reserved: [0; 16],
        }
    }
}

const IOWR: u32 = 0x40000000 | 0x80000000;

const IOCPARM_SHIFT: u32 = 13;
const IOCPARM_MASK: u32 = ((1 << IOCPARM_SHIFT) - 1);

const TYPESHIFT: u32 = 8;
const SIZESHIFT: u32 = 16;

macro_rules! ioctl {
    ($dir:expr, $name:ident, $ioty:expr, $nr:expr, $size:expr; $ty:ty) => {
        pub unsafe fn $name(fd: libc::c_int, val: *mut $ty) -> io::Result<libc::c_int> {
            let ioc = ($dir as u32)
                | (($size as u32 & IOCPARM_MASK) << SIZESHIFT)
                | (($ioty as u32) << TYPESHIFT)
                | ($nr as u32);
            from_unix_result(libc::ioctl(fd, ioc as libc::c_ulong, val))
        }
    };
}

// https://github.com/freebsd/freebsd/blob/master/sys/dev/usb/usb_ioctl.h
ioctl!(IOWR, usb_get_report_desc, b'U', 21, 32; /*struct*/ GenDescriptor);

fn read_report_descriptor(fd: RawFd) -> io::Result<ReportDescriptor> {
    let mut desc = GenDescriptor::default();
    let _ = unsafe { usb_get_report_desc(fd, &mut desc)? };
    desc.ugd_maxlen = desc.ugd_actlen;
    let mut value = Vec::with_capacity(desc.ugd_actlen as usize);
    unsafe {
        value.set_len(desc.ugd_actlen as usize);
    }
    desc.ugd_data = value.as_mut_ptr();
    let _ = unsafe { usb_get_report_desc(fd, &mut desc)? };
    Ok(ReportDescriptor { value })
}

pub fn is_u2f_device(fd: RawFd) -> bool {
    match read_report_descriptor(fd) {
        Ok(desc) => has_fido_usage(desc),
        Err(_) => false, // Upon failure, just say it's not a U2F device.
    }
}

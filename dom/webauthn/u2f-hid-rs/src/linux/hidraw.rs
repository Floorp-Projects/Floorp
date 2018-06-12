/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate libc;

use std::io;
use std::mem;
use std::os::unix::io::RawFd;

use consts::{FIDO_USAGE_U2FHID, FIDO_USAGE_PAGE};
use util::{from_unix_result, io_err};

#[allow(non_camel_case_types)]
#[repr(C)]
pub struct ReportDescriptor {
    size: ::libc::c_int,
    value: [u8; 4096],
}

impl ReportDescriptor {
    fn iter(self) -> ReportDescriptorIterator {
        ReportDescriptorIterator::new(self)
    }
}

const NRBITS: u32 = 8;
const TYPEBITS: u32 = 8;

const READ: u8 = 2;
const SIZEBITS: u8 = 14;

const NRSHIFT: u32 = 0;
const TYPESHIFT: u32 = NRSHIFT + NRBITS as u32;
const SIZESHIFT: u32 = TYPESHIFT + TYPEBITS as u32;
const DIRSHIFT: u32 = SIZESHIFT + SIZEBITS as u32;

// The 4 MSBs (the tag) are set when it's a long item.
const HID_MASK_LONG_ITEM_TAG: u8 = 0b11110000;
// The 2 LSBs denote the size of a short item.
const HID_MASK_SHORT_ITEM_SIZE: u8 = 0b00000011;
// The 6 MSBs denote the tag (4) and type (2).
const HID_MASK_ITEM_TAGTYPE: u8 = 0b11111100;
// tag=0000, type=10 (local)
const HID_ITEM_TAGTYPE_USAGE: u8 = 0b00001000;
// tag=0000, type=01 (global)
const HID_ITEM_TAGTYPE_USAGE_PAGE: u8 = 0b00000100;

// https://github.com/torvalds/linux/blob/master/include/uapi/linux/hid.h
const HID_MAX_DESCRIPTOR_SIZE: usize = 4096;

macro_rules! ioctl {
    ($dir:expr, $name:ident, $ioty:expr, $nr:expr; $ty:ty) => {
        pub unsafe fn $name(fd: libc::c_int, val: *mut $ty) -> io::Result<libc::c_int> {
            let size = mem::size_of::<$ty>();
            let ioc = (($dir as u32) << DIRSHIFT) | (($ioty as u32) << TYPESHIFT)
                | (($nr as u32) << NRSHIFT) | ((size as u32) << SIZESHIFT);

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
ioctl!(READ, hidiocgrdesc, b'H', 0x02; /*struct*/ ReportDescriptor);

enum Data {
    UsagePage { data: u32 },
    Usage { data: u32 },
}

struct ReportDescriptorIterator {
    desc: ReportDescriptor,
    pos: usize,
}

impl ReportDescriptorIterator {
    fn new(desc: ReportDescriptor) -> Self {
        Self { desc, pos: 0 }
    }

    fn next_item(&mut self) -> Option<Data> {
        let item = get_hid_item(&self.desc.value[self.pos..]);
        if item.is_none() {
            self.pos = self.desc.size as usize; // Close, invalid data.
            return None;
        }

        let (tag_type, key_len, data) = item.unwrap();

        // Advance if we have a valid item.
        self.pos += key_len + data.len();

        // We only check short items.
        if key_len > 1 {
            return None; // Check next item.
        }

        // Short items have max. length of 4 bytes.
        assert!(data.len() <= mem::size_of::<u32>());

        // Convert data bytes to a uint.
        let data = read_uint_le(data);

        match tag_type {
            HID_ITEM_TAGTYPE_USAGE_PAGE => Some(Data::UsagePage { data }),
            HID_ITEM_TAGTYPE_USAGE => Some(Data::Usage { data }),
            _ => None,
        }
    }
}

impl Iterator for ReportDescriptorIterator {
    type Item = Data;

    fn next(&mut self) -> Option<Self::Item> {
        if self.pos >= self.desc.size as usize {
            return None;
        }

        self.next_item().or_else(|| self.next())
    }
}

fn get_hid_item<'a>(buf: &'a [u8]) -> Option<(u8, usize, &'a [u8])> {
    if (buf[0] & HID_MASK_LONG_ITEM_TAG) == HID_MASK_LONG_ITEM_TAG {
        get_hid_long_item(buf)
    } else {
        get_hid_short_item(buf)
    }
}

fn get_hid_long_item<'a>(buf: &'a [u8]) -> Option<(u8, usize, &'a [u8])> {
    // A valid long item has at least three bytes.
    if buf.len() < 3 {
        return None;
    }

    let len = buf[1] as usize;

    // Ensure that there are enough bytes left in the buffer.
    if len > buf.len() - 3 {
        return None;
    }

    Some((buf[2], 3 /* key length */, &buf[3..]))
}

fn get_hid_short_item<'a>(buf: &'a [u8]) -> Option<(u8, usize, &'a [u8])> {
    // This is a short item. The bottom two bits of the key
    // contain the length of the data section (value) for this key.
    let len = match buf[0] & HID_MASK_SHORT_ITEM_SIZE {
        s @ 0...2 => s as usize,
        _ => 4, /* _ == 3 */
    };

    // Ensure that there are enough bytes left in the buffer.
    if len > buf.len() - 1 {
        return None;
    }

    Some((
        buf[0] & HID_MASK_ITEM_TAGTYPE,
        1, /* key length */
        &buf[1..1 + len],
    ))
}

fn read_uint_le(buf: &[u8]) -> u32 {
    assert!(buf.len() <= 4);
    // Parse the number in little endian byte order.
    buf.iter().rev().fold(0, |num, b| (num << 8) | (*b as u32))
}

pub fn is_u2f_device(fd: RawFd) -> bool {
    match read_report_descriptor(fd) {
        Ok(desc) => has_fido_usage(desc),
        Err(_) => false, // Upon failure, just say it's not a U2F device.
    }
}

fn read_report_descriptor(fd: RawFd) -> io::Result<ReportDescriptor> {
    let mut desc = ReportDescriptor {
        size: 0,
        value: [0; HID_MAX_DESCRIPTOR_SIZE],
    };

    let _ = unsafe { hidiocgrdescsize(fd, &mut desc.size)? };
    if desc.size == 0 || desc.size as usize > desc.value.len() {
        return Err(io_err("unexpected hidiocgrdescsize() result"));
    }

    let _ = unsafe { hidiocgrdesc(fd, &mut desc)? };
    Ok(desc)
}

fn has_fido_usage(desc: ReportDescriptor) -> bool {
    let mut usage_page = None;
    let mut usage = None;

    for data in desc.iter() {
        match data {
            Data::UsagePage { data } => usage_page = Some(data),
            Data::Usage { data } => usage = Some(data),
        }

        // Check the values we found.
        if let (Some(usage_page), Some(usage)) = (usage_page, usage) {
            return usage_page == FIDO_USAGE_PAGE as u32 && usage == FIDO_USAGE_U2FHID as u32;
        }
    }

    false
}

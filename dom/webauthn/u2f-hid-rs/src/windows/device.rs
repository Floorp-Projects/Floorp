/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::fs::{File, OpenOptions};
use std::io;
use std::io::{Read, Write};
use std::os::windows::io::AsRawHandle;

use consts::{CID_BROADCAST, HID_RPT_SIZE, FIDO_USAGE_PAGE, FIDO_USAGE_U2FHID};
use super::winapi::DeviceCapabilities;

use u2ftypes::U2FDevice;

#[derive(Debug)]
pub struct Device {
    path: String,
    file: File,
    cid: [u8; 4],
}

impl Device {
    pub fn new(path: String) -> io::Result<Self> {
        let file = OpenOptions::new().read(true).write(true).open(&path)?;
        Ok(Self {
            path: path,
            file: file,
            cid: CID_BROADCAST,
        })
    }

    pub fn is_u2f(&self) -> bool {
        match DeviceCapabilities::new(self.file.as_raw_handle()) {
            Ok(caps) => caps.usage() == FIDO_USAGE_U2FHID && caps.usage_page() == FIDO_USAGE_PAGE,
            _ => false,
        }
    }
}

impl PartialEq for Device {
    fn eq(&self, other: &Device) -> bool {
        self.path == other.path
    }
}

impl Read for Device {
    fn read(&mut self, bytes: &mut [u8]) -> io::Result<usize> {
        // Windows always includes the report ID.
        let mut input = [0u8; HID_RPT_SIZE + 1];
        let _ = self.file.read(&mut input)?;
        bytes.clone_from_slice(&input[1..]);
        Ok(bytes.len() as usize)
    }
}

impl Write for Device {
    fn write(&mut self, bytes: &[u8]) -> io::Result<usize> {
        self.file.write(bytes)
    }

    fn flush(&mut self) -> io::Result<()> {
        self.file.flush()
    }
}

impl U2FDevice for Device {
    fn get_cid<'a>(&'a self) -> &'a [u8; 4] {
        &self.cid
    }

    fn set_cid(&mut self, cid: [u8; 4]) {
        self.cid = cid;
    }
}

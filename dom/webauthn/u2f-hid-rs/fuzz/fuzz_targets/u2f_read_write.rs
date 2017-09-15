/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![no_main]
#[macro_use] extern crate libfuzzer_sys;
extern crate rand;
extern crate u2fhid;

use rand::{thread_rng, Rng};
use std::{cmp, io};

use u2fhid::{CID_BROADCAST, HID_RPT_SIZE};
use u2fhid::{U2FDevice, sendrecv};

struct TestDevice {
    cid: [u8; 4],
    data: Vec<u8>,
}

impl TestDevice {
    pub fn new() -> TestDevice {
        TestDevice {
            cid: CID_BROADCAST,
            data: vec!(),
        }
    }
}

impl io::Read for TestDevice {
    fn read(&mut self, bytes: &mut [u8]) -> io::Result<usize> {
        assert!(bytes.len() == HID_RPT_SIZE);
        let max = cmp::min(self.data.len(), HID_RPT_SIZE);
        bytes[..max].copy_from_slice(&self.data[..max]);
        self.data = self.data[max..].to_vec();
        Ok(max)
    }
}

impl io::Write for TestDevice {
    fn write(&mut self, bytes: &[u8]) -> io::Result<usize> {
        assert!(bytes.len() == HID_RPT_SIZE + 1);
        self.data.extend_from_slice(&bytes[1..]);
        Ok(bytes.len())
    }

    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}

impl U2FDevice for TestDevice {
    fn get_cid<'a>(&'a self) -> &'a [u8; 4] {
        &self.cid
    }

    fn set_cid(&mut self, cid: [u8; 4]) {
        self.cid = cid;
    }
}

fuzz_target!(|data: &[u8]| {
    let mut dev = TestDevice::new();
    let cmd = thread_rng().gen::<u8>();
    let res = sendrecv(&mut dev, cmd, data);
    assert_eq!(data, &res.unwrap()[..]);
});

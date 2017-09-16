/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate libc;
extern crate log;

use std::fmt;
use std::io;
use std::io::{Read, Write};
use std::slice;
use std::sync::mpsc::{channel, Sender, Receiver, RecvTimeoutError};
use std::time::Duration;

use core_foundation_sys::base::*;
use libc::c_void;

use consts::{CID_BROADCAST, HID_RPT_SIZE};
use u2ftypes::U2FDevice;

use super::iokit::*;

const READ_TIMEOUT: u64 = 15;

pub struct Device {
    device_ref: IOHIDDeviceRef,
    cid: [u8; 4],
    report_rx: Receiver<Vec<u8>>,
    report_send_void: *mut c_void,
    scratch_buf_ptr: *mut u8,
}

impl Device {
    pub fn new(device_ref: IOHIDDeviceRef) -> Self {
        let (report_tx, report_rx) = channel();
        let report_send_void = Box::into_raw(Box::new(report_tx)) as *mut c_void;

        let scratch_buf = [0; HID_RPT_SIZE];
        let scratch_buf_ptr = Box::into_raw(Box::new(scratch_buf)) as *mut u8;

        unsafe {
            IOHIDDeviceRegisterInputReportCallback(
                device_ref,
                scratch_buf_ptr,
                HID_RPT_SIZE as CFIndex,
                read_new_data_cb,
                report_send_void,
            );
        }

        Self {
            device_ref,
            cid: CID_BROADCAST,
            report_rx,
            report_send_void,
            scratch_buf_ptr,
        }
    }
}

impl Drop for Device {
    fn drop(&mut self) {
        debug!("Dropping U2F device {}", self);

        unsafe {
            // Re-allocate raw pointers for destruction.
            let _ = Box::from_raw(self.report_send_void as *mut Sender<Vec<u8>>);
            let _ = Box::from_raw(self.scratch_buf_ptr as *mut [u8; HID_RPT_SIZE]);
        }
    }
}

impl fmt::Display for Device {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "InternalDevice(ref:{:?}, cid: {:02x}{:02x}{:02x}{:02x})",
            self.device_ref,
            self.cid[0],
            self.cid[1],
            self.cid[2],
            self.cid[3]
        )
    }
}

impl PartialEq for Device {
    fn eq(&self, other_device: &Device) -> bool {
        self.device_ref == other_device.device_ref
    }
}

impl Read for Device {
    fn read(&mut self, mut bytes: &mut [u8]) -> io::Result<usize> {
        let timeout = Duration::from_secs(READ_TIMEOUT);
        let data = match self.report_rx.recv_timeout(timeout) {
            Ok(v) => v,
            Err(e) if e == RecvTimeoutError::Timeout => {
                return Err(io::Error::new(io::ErrorKind::TimedOut, e));
            }
            Err(e) => {
                return Err(io::Error::new(io::ErrorKind::UnexpectedEof, e));
            }
        };
        bytes.write(&data)
    }
}

impl Write for Device {
    fn write(&mut self, bytes: &[u8]) -> io::Result<usize> {
        assert_eq!(bytes.len(), HID_RPT_SIZE + 1);

        let report_id = bytes[0] as i64;
        // Skip report number when not using numbered reports.
        let start = if report_id == 0x0 { 1 } else { 0 };
        let data = &bytes[start..];

        let result = unsafe {
            IOHIDDeviceSetReport(
                self.device_ref,
                kIOHIDReportTypeOutput,
                report_id,
                data.as_ptr(),
                data.len() as CFIndex,
            )
        };
        if result != 0 {
            warn!("set_report sending failure = {0:X}", result);
            return Err(io::Error::from_raw_os_error(result));
        }
        trace!("set_report sending success = {0:X}", result);

        Ok(bytes.len())
    }

    // USB HID writes don't buffer, so this will be a nop.
    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}

impl U2FDevice for Device {
    fn get_cid(&self) -> &[u8; 4] {
        &self.cid
    }

    fn set_cid(&mut self, cid: [u8; 4]) {
        self.cid = cid;
    }
}

// This is called from the RunLoop thread
extern "C" fn read_new_data_cb(
    context: *mut c_void,
    _: IOReturn,
    _: *mut c_void,
    report_type: IOHIDReportType,
    report_id: u32,
    report: *mut u8,
    report_len: CFIndex,
) {
    unsafe {
        let tx = &mut *(context as *mut Sender<Vec<u8>>);

        trace!(
            "read_new_data_cb type={} id={} report={:?} len={}",
            report_type,
            report_id,
            report,
            report_len
        );

        let report_len = report_len as usize;
        if report_len > HID_RPT_SIZE {
            warn!(
                "read_new_data_cb got too much data! {} > {}",
                report_len,
                HID_RPT_SIZE
            );
            return;
        }

        let data = slice::from_raw_parts(report, report_len).to_vec();

        if let Err(e) = tx.send(data) {
            // TOOD: This happens when the channel closes before this thread
            // does. This is pretty common, but let's deal with stopping
            // properly later.
            warn!("Problem returning read_new_data_cb data for thread: {}", e);
        };
    }
}

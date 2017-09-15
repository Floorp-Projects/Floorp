/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{cmp, io};

use consts::*;
use util::io_err;

use log;

fn trace_hex(data: &[u8]) {
    if log_enabled!(log::LogLevel::Trace) {
        let parts: Vec<String> = data.iter().map(|byte| format!("{:02x}", byte)).collect();
        trace!("USB send: {}", parts.join(""));
    }
}

// Trait for representing U2F HID Devices. Requires getters/setters for the
// channel ID, created during device initialization.
pub trait U2FDevice {
    fn get_cid(&self) -> &[u8; 4];
    fn set_cid(&mut self, cid: [u8; 4]);
}

// Init structure for U2F Communications. Tells the receiver what channel
// communication is happening on, what command is running, and how much data to
// expect to receive over all.
//
// Spec at https://fidoalliance.org/specs/fido-u2f-v1.
// 0-nfc-bt-amendment-20150514/fido-u2f-hid-protocol.html#message--and-packet-structure
pub struct U2FHIDInit {}

impl U2FHIDInit {
    pub fn read<T>(dev: &mut T) -> io::Result<Vec<u8>>
    where
        T: U2FDevice + io::Read,
    {
        let mut frame = [0u8; HID_RPT_SIZE];
        let count = dev.read(&mut frame)?;

        if count != HID_RPT_SIZE {
            return Err(io_err("invalid init packet"));
        }

        if dev.get_cid() != &frame[..4] {
            return Err(io_err("invalid channel id"));
        }

        let cap = (frame[5] as usize) << 8 | (frame[6] as usize);
        let mut data = Vec::with_capacity(cap);

        let len = cmp::min(cap, INIT_DATA_SIZE);
        data.extend_from_slice(&frame[7..7 + len]);

        Ok(data)
    }

    pub fn write<T>(dev: &mut T, cmd: u8, data: &[u8]) -> io::Result<usize>
    where
        T: U2FDevice + io::Write,
    {
        if data.len() > 0xffff {
            return Err(io_err("payload length > 2^16"));
        }

        let mut frame = [0; HID_RPT_SIZE + 1];
        frame[1..5].copy_from_slice(dev.get_cid());
        frame[5] = cmd;
        frame[6] = (data.len() >> 8) as u8;
        frame[7] = data.len() as u8;

        let count = cmp::min(data.len(), INIT_DATA_SIZE);
        frame[8..8 + count].copy_from_slice(&data[..count]);
        trace_hex(&frame);

        if dev.write(&frame)? != frame.len() {
            return Err(io_err("device write failed"));
        }

        Ok(count)
    }
}

// Continuation structure for U2F Communications. After an Init structure is
// sent, continuation structures are used to transmit all extra data that
// wouldn't fit in the initial packet. The sequence number increases with every
// packet, until all data is received.
//
// https://fidoalliance.org/specs/fido-u2f-v1.0-nfc-bt-amendment-20150514/fido-u2f-hid-protocol.
// html#message--and-packet-structure
pub struct U2FHIDCont {}

impl U2FHIDCont {
    pub fn read<T>(dev: &mut T, seq: u8, max: usize) -> io::Result<Vec<u8>>
    where
        T: U2FDevice + io::Read,
    {
        let mut frame = [0u8; HID_RPT_SIZE];
        let count = dev.read(&mut frame)?;

        if count != HID_RPT_SIZE {
            return Err(io_err("invalid cont packet"));
        }

        if dev.get_cid() != &frame[..4] {
            return Err(io_err("invalid channel id"));
        }

        if seq != frame[4] {
            return Err(io_err("invalid sequence number"));
        }

        let max = cmp::min(max, CONT_DATA_SIZE);
        Ok(frame[5..5 + max].to_vec())
    }

    pub fn write<T>(dev: &mut T, seq: u8, data: &[u8]) -> io::Result<usize>
    where
        T: U2FDevice + io::Write,
    {
        let mut frame = [0; HID_RPT_SIZE + 1];
        frame[1..5].copy_from_slice(dev.get_cid());
        frame[5] = seq;

        let count = cmp::min(data.len(), CONT_DATA_SIZE);
        frame[6..6 + count].copy_from_slice(&data[..count]);
        trace_hex(&frame);

        if dev.write(&frame)? != frame.len() {
            return Err(io_err("device write failed"));
        }

        Ok(count)
    }
}


// Reply sent after initialization command. Contains information about U2F USB
// Key versioning, as well as the communication channel to be used for all
// further requests.
//
// https://fidoalliance.org/specs/fido-u2f-v1.0-nfc-bt-amendment-20150514/fido-u2f-hid-protocol.
// html#u2fhid_init
pub struct U2FHIDInitResp {}

impl U2FHIDInitResp {
    pub fn read(data: &[u8], nonce: &[u8]) -> io::Result<[u8; 4]> {
        assert_eq!(nonce.len(), INIT_NONCE_SIZE);

        if data.len() != INIT_NONCE_SIZE + 9 {
            return Err(io_err("invalid init response"));
        }

        if nonce != &data[..INIT_NONCE_SIZE] {
            return Err(io_err("invalid nonce"));
        }

        let mut cid = [0u8; 4];
        cid.copy_from_slice(&data[INIT_NONCE_SIZE..INIT_NONCE_SIZE + 4]);
        Ok(cid)
    }
}

// https://en.wikipedia.org/wiki/Smart_card_application_protocol_data_unit
// https://fidoalliance.org/specs/fido-u2f-v1.
// 0-nfc-bt-amendment-20150514/fido-u2f-raw-message-formats.html#u2f-message-framing
pub struct U2FAPDUHeader {}

impl U2FAPDUHeader {
    pub fn to_bytes(ins: u8, p1: u8, data: &[u8]) -> io::Result<Vec<u8>> {
        if data.len() > 0xffff {
            return Err(io_err("payload length > 2^16"));
        }

        // Size of header + data + 2 zero bytes for maximum return size.
        let mut bytes = vec![0u8; U2FAPDUHEADER_SIZE + data.len() + 2];
        bytes[1] = ins;
        bytes[2] = p1;
        // p2 is always 0, at least, for our requirements.
        // lc[0] should always be 0.
        bytes[5] = (data.len() >> 8) as u8;
        bytes[6] = data.len() as u8;
        bytes[7..7 + data.len()].copy_from_slice(data);

        Ok(bytes)
    }
}

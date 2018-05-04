/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate std;

use rand::{thread_rng, Rng};
use std::ffi::CString;
use std::io;
use std::io::{Read, Write};

use consts::*;
use u2ftypes::*;
use util::io_err;

////////////////////////////////////////////////////////////////////////
// Device Commands
////////////////////////////////////////////////////////////////////////

pub fn u2f_init_device<T>(dev: &mut T) -> bool
where
    T: U2FDevice + Read + Write,
{
    // Do a few U2F device checks.
    let mut nonce = [0u8; 8];
    thread_rng().fill_bytes(&mut nonce);
    if init_device(dev, &nonce).is_err() {
        return false;
    }

    let mut random = [0u8; 8];
    thread_rng().fill_bytes(&mut random);
    if ping_device(dev, &random).is_err() {
        return false;
    }

    is_v2_device(dev).unwrap_or(false)
}

pub fn u2f_register<T>(dev: &mut T, challenge: &[u8], application: &[u8]) -> io::Result<Vec<u8>>
where
    T: U2FDevice + Read + Write,
{
    if challenge.len() != PARAMETER_SIZE || application.len() != PARAMETER_SIZE {
        return Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            "Invalid parameter sizes",
        ));
    }

    let mut register_data = Vec::with_capacity(2 * PARAMETER_SIZE);
    register_data.extend(challenge);
    register_data.extend(application);

    let flags = U2F_REQUEST_USER_PRESENCE;
    let (resp, status) = send_apdu(dev, U2F_REGISTER, flags, &register_data)?;
    status_word_to_result(status, resp)
}

pub fn u2f_sign<T>(
    dev: &mut T,
    challenge: &[u8],
    application: &[u8],
    key_handle: &[u8],
) -> io::Result<Vec<u8>>
where
    T: U2FDevice + Read + Write,
{
    if challenge.len() != PARAMETER_SIZE || application.len() != PARAMETER_SIZE {
        return Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            "Invalid parameter sizes",
        ));
    }

    if key_handle.len() > 256 {
        return Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            "Key handle too large",
        ));
    }

    let mut sign_data = Vec::with_capacity(2 * PARAMETER_SIZE + 1 + key_handle.len());
    sign_data.extend(challenge);
    sign_data.extend(application);
    sign_data.push(key_handle.len() as u8);
    sign_data.extend(key_handle);

    let flags = U2F_REQUEST_USER_PRESENCE;
    let (resp, status) = send_apdu(dev, U2F_AUTHENTICATE, flags, &sign_data)?;
    status_word_to_result(status, resp)
}

pub fn u2f_is_keyhandle_valid<T>(
    dev: &mut T,
    challenge: &[u8],
    application: &[u8],
    key_handle: &[u8],
) -> io::Result<bool>
where
    T: U2FDevice + Read + Write,
{
    if challenge.len() != PARAMETER_SIZE || application.len() != PARAMETER_SIZE {
        return Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            "Invalid parameter sizes",
        ));
    }

    if key_handle.len() > 256 {
        return Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            "Key handle too large",
        ));
    }

    let mut sign_data = Vec::with_capacity(2 * PARAMETER_SIZE + 1 + key_handle.len());
    sign_data.extend(challenge);
    sign_data.extend(application);
    sign_data.push(key_handle.len() as u8);
    sign_data.extend(key_handle);

    let flags = U2F_CHECK_IS_REGISTERED;
    let (_, status) = send_apdu(dev, U2F_AUTHENTICATE, flags, &sign_data)?;
    Ok(status == SW_CONDITIONS_NOT_SATISFIED)
}

////////////////////////////////////////////////////////////////////////
// Internal Device Commands
////////////////////////////////////////////////////////////////////////

fn init_device<T>(dev: &mut T, nonce: &[u8]) -> io::Result<()>
where
    T: U2FDevice + Read + Write,
{
    assert_eq!(nonce.len(), INIT_NONCE_SIZE);
    let raw = sendrecv(dev, U2FHID_INIT, nonce)?;
    dev.set_cid(U2FHIDInitResp::read(&raw, nonce)?);

    Ok(())
}

fn ping_device<T>(dev: &mut T, random: &[u8]) -> io::Result<()>
where
    T: U2FDevice + Read + Write,
{
    assert_eq!(random.len(), 8);
    if sendrecv(dev, U2FHID_PING, random)? != random {
        return Err(io_err("Ping was corrupted!"));
    }

    Ok(())
}

fn is_v2_device<T>(dev: &mut T) -> io::Result<bool>
where
    T: U2FDevice + Read + Write,
{
    let (data, status) = send_apdu(dev, U2F_VERSION, 0x00, &[])?;
    let actual = CString::new(data)?;
    let expected = CString::new("U2F_V2")?;
    status_word_to_result(status, actual == expected)
}

////////////////////////////////////////////////////////////////////////
// Error Handling
////////////////////////////////////////////////////////////////////////

fn status_word_to_result<T>(status: [u8; 2], val: T) -> io::Result<T> {
    use self::io::ErrorKind::{InvalidData, InvalidInput};

    match status {
        SW_NO_ERROR => Ok(val),
        SW_WRONG_DATA => Err(io::Error::new(InvalidData, "wrong data")),
        SW_WRONG_LENGTH => Err(io::Error::new(InvalidInput, "wrong length")),
        SW_CONDITIONS_NOT_SATISFIED => Err(io_err("conditions not satisfied")),
        _ => Err(io_err(&format!("failed with status {:?}", status))),
    }
}

////////////////////////////////////////////////////////////////////////
// Device Communication Functions
////////////////////////////////////////////////////////////////////////

pub fn sendrecv<T>(dev: &mut T, cmd: u8, send: &[u8]) -> io::Result<Vec<u8>>
where
    T: U2FDevice + Read + Write,
{
    // Send initialization packet.
    let mut count = U2FHIDInit::write(dev, cmd, send)?;

    // Send continuation packets.
    let mut sequence = 0u8;
    while count < send.len() {
        count += U2FHIDCont::write(dev, sequence, &send[count..])?;
        sequence += 1;
    }

    // Now we read. This happens in 2 chunks: The initial packet, which has the
    // size we expect overall, then continuation packets, which will fill in
    // data until we have everything.
    let mut data = U2FHIDInit::read(dev)?;

    let mut sequence = 0u8;
    while data.len() < data.capacity() {
        let max = data.capacity() - data.len();
        data.extend_from_slice(&U2FHIDCont::read(dev, sequence, max)?);
        sequence += 1;
    }

    Ok(data)
}

fn send_apdu<T>(dev: &mut T, cmd: u8, p1: u8, send: &[u8]) -> io::Result<(Vec<u8>, [u8; 2])>
where
    T: U2FDevice + Read + Write,
{
    let apdu = U2FAPDUHeader::to_bytes(cmd, p1, send)?;
    let mut data = sendrecv(dev, U2FHID_MSG, &apdu)?;

    if data.len() < 2 {
        return Err(io_err("unexpected response"));
    }

    let split_at = data.len() - 2;
    let status = data.split_off(split_at);
    Ok((data, [status[0], status[1]]))
}

////////////////////////////////////////////////////////////////////////
// Tests
////////////////////////////////////////////////////////////////////////

#[cfg(test)]
mod tests {
    use rand::{thread_rng, Rng};

    use super::{init_device, ping_device, send_apdu, sendrecv, U2FDevice};
    use consts::{U2FHID_INIT, U2FHID_MSG, U2FHID_PING, CID_BROADCAST, SW_NO_ERROR};

    mod platform {
        use std::io;
        use std::io::{Read, Write};

        use consts::{CID_BROADCAST, HID_RPT_SIZE};
        use u2ftypes::U2FDevice;

        pub struct TestDevice {
            cid: [u8; 4],
            reads: Vec<[u8; HID_RPT_SIZE]>,
            writes: Vec<[u8; HID_RPT_SIZE + 1]>,
        }

        impl TestDevice {
            pub fn new() -> TestDevice {
                TestDevice {
                    cid: CID_BROADCAST,
                    reads: vec![],
                    writes: vec![],
                }
            }

            pub fn add_write(&mut self, packet: &[u8], fill_value: u8) {
                // Add one to deal with record index check
                let mut write = [fill_value; HID_RPT_SIZE + 1];
                // Make sure we start with a 0, for HID record index
                write[0] = 0;
                // Clone packet data in at 1, since front is padded with HID record index
                write[1..packet.len() + 1].clone_from_slice(packet);
                self.writes.push(write);
            }

            pub fn add_read(&mut self, packet: &[u8], fill_value: u8) {
                let mut read = [fill_value; HID_RPT_SIZE];
                read[..packet.len()].clone_from_slice(packet);
                self.reads.push(read);
            }
        }

        impl Write for TestDevice {
            fn write(&mut self, bytes: &[u8]) -> io::Result<usize> {
                // Pop a vector from the expected writes, check for quality
                // against bytes array.
                assert!(self.writes.len() > 0, "Ran out of expected write values!");
                let check = self.writes.remove(0);
                assert_eq!(check.len(), bytes.len());
                assert_eq!(&check[..], bytes);
                Ok(bytes.len())
            }

            // nop
            fn flush(&mut self) -> io::Result<()> {
                Ok(())
            }
        }

        impl Read for TestDevice {
            fn read(&mut self, bytes: &mut [u8]) -> io::Result<usize> {
                assert!(self.reads.len() > 0, "Ran out of read values!");
                let check = self.reads.remove(0);
                assert_eq!(check.len(), bytes.len());
                bytes.clone_from_slice(&check[..]);
                Ok(check.len())
            }
        }

        impl Drop for TestDevice {
            fn drop(&mut self) {
                assert_eq!(self.reads.len(), 0);
                assert_eq!(self.writes.len(), 0);
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
    }

    #[test]
    fn test_init_device() {
        let mut device = platform::TestDevice::new();
        let nonce = vec![0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01];

        // channel id
        let mut cid = [0u8; 4];
        thread_rng().fill_bytes(&mut cid);

        // init packet
        let mut msg = CID_BROADCAST.to_vec();
        msg.extend(vec![U2FHID_INIT, 0x00, 0x08]); // cmd + bcnt
        msg.extend_from_slice(&nonce);
        device.add_write(&msg, 0);

        // init_resp packet
        let mut msg = CID_BROADCAST.to_vec();
        msg.extend(vec![U2FHID_INIT, 0x00, 0x11]); // cmd + bcnt
        msg.extend_from_slice(&nonce);
        msg.extend_from_slice(&cid); // new channel id
        msg.extend(vec![0x02, 0x04, 0x01, 0x08, 0x01]); // versions + flags
        device.add_read(&msg, 0);

        init_device(&mut device, &nonce).unwrap();
        assert_eq!(device.get_cid(), &cid);
    }

    #[test]
    fn test_sendrecv_multiple() {
        let mut device = platform::TestDevice::new();
        let cid = [0x01, 0x02, 0x03, 0x04];
        device.set_cid(cid.clone());

        // init packet
        let mut msg = cid.to_vec();
        msg.extend(vec![U2FHID_PING, 0x00, 0xe4]); // cmd + length = 228
                                                   // write msg, append [1u8; 57], 171 bytes remain
        device.add_write(&msg, 1);
        device.add_read(&msg, 1);

        // cont packet
        let mut msg = cid.to_vec();
        msg.push(0x00); // seq = 0
                        // write msg, append [1u8; 59], 112 bytes remaining
        device.add_write(&msg, 1);
        device.add_read(&msg, 1);

        // cont packet
        let mut msg = cid.to_vec();
        msg.push(0x01); // seq = 1
                        // write msg, append [1u8; 59], 53 bytes remaining
        device.add_write(&msg, 1);
        device.add_read(&msg, 1);

        // cont packet
        let mut msg = cid.to_vec();
        msg.push(0x02); // seq = 2
        msg.extend_from_slice(&[1u8; 53]);
        // write msg, append remaining 53 bytes.
        device.add_write(&msg, 0);
        device.add_read(&msg, 0);

        let data = [1u8; 228];
        let d = sendrecv(&mut device, U2FHID_PING, &data).unwrap();
        assert_eq!(d.len(), 228);
        assert_eq!(d, &data[..]);
    }

    #[test]
    fn test_sendapdu() {
        let cid = [0x01, 0x02, 0x03, 0x04];
        let data = [0x01, 0x02, 0x03, 0x04, 0x05];
        let mut device = platform::TestDevice::new();
        device.set_cid(cid.clone());

        let mut msg = cid.to_vec();
        // sendrecv header
        msg.extend(vec![U2FHID_MSG, 0x00, 0x0e]); // len = 14
                                                  // apdu header
        msg.extend(vec![0x00, U2FHID_PING, 0xaa, 0x00, 0x00, 0x00, 0x05]);
        // apdu data
        msg.extend_from_slice(&data);
        device.add_write(&msg, 0);

        // Send data back
        let mut msg = cid.to_vec();
        msg.extend(vec![U2FHID_MSG, 0x00, 0x07]);
        msg.extend_from_slice(&data);
        msg.extend_from_slice(&SW_NO_ERROR);
        device.add_read(&msg, 0);

        let (result, status) = send_apdu(&mut device, U2FHID_PING, 0xaa, &data).unwrap();
        assert_eq!(result, &data);
        assert_eq!(status, SW_NO_ERROR);
    }

    #[test]
    fn test_ping_device() {
        let mut device = platform::TestDevice::new();
        device.set_cid([0x01, 0x02, 0x03, 0x04]);

        // ping nonce
        let random = vec![0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08];

        // APDU header
        let mut msg = vec![0x01, 0x02, 0x03, 0x04, U2FHID_PING, 0x00, 0x08];
        msg.extend_from_slice(&random);
        device.add_write(&msg, 0);

        // Only expect data from APDU back
        let mut msg = vec![0x01, 0x02, 0x03, 0x04, U2FHID_MSG, 0x00, 0x08];
        msg.extend_from_slice(&random);
        device.add_read(&msg, 0);

        ping_device(&mut device, &random).unwrap();
    }
}

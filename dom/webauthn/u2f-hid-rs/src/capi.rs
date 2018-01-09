/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use libc::size_t;
use rand::{thread_rng, Rng};
use std::collections::HashMap;
use std::{ptr, slice};

use U2FManager;

type U2FKeyHandles = Vec<::KeyHandle>;
type U2FResult = HashMap<u8, Vec<u8>>;
type U2FCallback = extern "C" fn(u64, *mut U2FResult);

const RESBUF_ID_REGISTRATION: u8 = 0;
const RESBUF_ID_KEYHANDLE: u8 = 1;
const RESBUF_ID_SIGNATURE: u8 = 2;

// Generates a new 64-bit transaction id with collision probability 2^-32.
fn new_tid() -> u64 {
    thread_rng().gen::<u64>()
}

unsafe fn from_raw(ptr: *const u8, len: usize) -> Vec<u8> {
    slice::from_raw_parts(ptr, len).to_vec()
}

#[no_mangle]
pub extern "C" fn rust_u2f_mgr_new() -> *mut U2FManager {
    if let Ok(mgr) = U2FManager::new() {
        Box::into_raw(Box::new(mgr))
    } else {
        ptr::null_mut()
    }
}

#[no_mangle]
pub unsafe extern "C" fn rust_u2f_mgr_free(mgr: *mut U2FManager) {
    if !mgr.is_null() {
        Box::from_raw(mgr);
    }
}

#[no_mangle]
pub unsafe extern "C" fn rust_u2f_khs_new() -> *mut U2FKeyHandles {
    Box::into_raw(Box::new(vec![]))
}

#[no_mangle]
pub unsafe extern "C" fn rust_u2f_khs_add(
    khs: *mut U2FKeyHandles,
    key_handle_ptr: *const u8,
    key_handle_len: usize,
    transports: u8,
) {
    (*khs).push(::KeyHandle {
        credential: from_raw(key_handle_ptr, key_handle_len),
        transports: ::AuthenticatorTransports::from_bits_truncate(transports),
    });
}

#[no_mangle]
pub unsafe extern "C" fn rust_u2f_khs_free(khs: *mut U2FKeyHandles) {
    if !khs.is_null() {
        Box::from_raw(khs);
    }
}

#[no_mangle]
pub unsafe extern "C" fn rust_u2f_resbuf_length(
    res: *const U2FResult,
    bid: u8,
    len: *mut size_t,
) -> bool {
    if res.is_null() {
        return false;
    }

    if let Some(buf) = (*res).get(&bid) {
        *len = buf.len();
        return true;
    }

    false
}

#[no_mangle]
pub unsafe extern "C" fn rust_u2f_resbuf_copy(
    res: *const U2FResult,
    bid: u8,
    dst: *mut u8,
) -> bool {
    if res.is_null() {
        return false;
    }

    if let Some(buf) = (*res).get(&bid) {
        ptr::copy_nonoverlapping(buf.as_ptr(), dst, buf.len());
        return true;
    }

    false
}

#[no_mangle]
pub unsafe extern "C" fn rust_u2f_res_free(res: *mut U2FResult) {
    if !res.is_null() {
        Box::from_raw(res);
    }
}

#[no_mangle]
pub unsafe extern "C" fn rust_u2f_mgr_register(
    mgr: *mut U2FManager,
    flags: u64,
    timeout: u64,
    callback: U2FCallback,
    challenge_ptr: *const u8,
    challenge_len: usize,
    application_ptr: *const u8,
    application_len: usize,
    khs: *const U2FKeyHandles,
) -> u64 {
    if mgr.is_null() {
        return 0;
    }

    // Check buffers.
    if challenge_ptr.is_null() || application_ptr.is_null() {
        return 0;
    }

    let flags = ::RegisterFlags::from_bits_truncate(flags);
    let challenge = from_raw(challenge_ptr, challenge_len);
    let application = from_raw(application_ptr, application_len);
    let key_handles = (*khs).clone();

    let tid = new_tid();
    let res = (*mgr).register(
        flags,
        timeout,
        challenge,
        application,
        key_handles,
        move |rv| {
            if let Ok(registration) = rv {
                let mut result = U2FResult::new();
                result.insert(RESBUF_ID_REGISTRATION, registration);
                callback(tid, Box::into_raw(Box::new(result)));
            } else {
                callback(tid, ptr::null_mut());
            };
        },
    );

    if res.is_ok() { tid } else { 0 }
}

#[no_mangle]
pub unsafe extern "C" fn rust_u2f_mgr_sign(
    mgr: *mut U2FManager,
    flags: u64,
    timeout: u64,
    callback: U2FCallback,
    challenge_ptr: *const u8,
    challenge_len: usize,
    application_ptr: *const u8,
    application_len: usize,
    khs: *const U2FKeyHandles,
) -> u64 {
    if mgr.is_null() || khs.is_null() {
        return 0;
    }

    // Check buffers.
    if challenge_ptr.is_null() || application_ptr.is_null() {
        return 0;
    }

    // Need at least one key handle.
    if (*khs).len() < 1 {
        return 0;
    }

    let flags = ::SignFlags::from_bits_truncate(flags);
    let challenge = from_raw(challenge_ptr, challenge_len);
    let application = from_raw(application_ptr, application_len);
    let key_handles = (*khs).clone();

    let tid = new_tid();
    let res = (*mgr).sign(
        flags,
        timeout,
        challenge,
        application,
        key_handles,
        move |rv| {
            if let Ok((key_handle, signature)) = rv {
                let mut result = U2FResult::new();
                result.insert(RESBUF_ID_KEYHANDLE, key_handle);
                result.insert(RESBUF_ID_SIGNATURE, signature);
                callback(tid, Box::into_raw(Box::new(result)));
            } else {
                callback(tid, ptr::null_mut());
            };
        },
    );

    if res.is_ok() { tid } else { 0 }
}

#[no_mangle]
pub unsafe extern "C" fn rust_u2f_mgr_cancel(mgr: *mut U2FManager) -> u64 {
    if !mgr.is_null() {
        // Ignore return value.
        let _ = (*mgr).cancel();
    }

    new_tid()
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use libc::size_t;
use rand::{thread_rng, Rng};
use std::collections::HashMap;
use std::{ptr, slice};

use U2FManager;

type U2FAppIds = Vec<::AppId>;
type U2FKeyHandles = Vec<::KeyHandle>;
type U2FCallback = extern "C" fn(u64, *mut U2FResult);

pub enum U2FResult {
    Success(HashMap<u8, Vec<u8>>),
    Error(::Error),
}

const RESBUF_ID_REGISTRATION: u8 = 0;
const RESBUF_ID_KEYHANDLE: u8 = 1;
const RESBUF_ID_SIGNATURE: u8 = 2;
const RESBUF_ID_APPID: u8 = 3;

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
pub unsafe extern "C" fn rust_u2f_app_ids_new() -> *mut U2FAppIds {
    Box::into_raw(Box::new(vec![]))
}

#[no_mangle]
pub unsafe extern "C" fn rust_u2f_app_ids_add(
    ids: *mut U2FAppIds,
    id_ptr: *const u8,
    id_len: usize,
) {
    (*ids).push(from_raw(id_ptr, id_len));
}

#[no_mangle]
pub unsafe extern "C" fn rust_u2f_app_ids_free(ids: *mut U2FAppIds) {
    if !ids.is_null() {
        Box::from_raw(ids);
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
pub unsafe extern "C" fn rust_u2f_result_error(res: *const U2FResult) -> u8 {
    if res.is_null() {
        return ::Error::Unknown as u8;
    }

    if let U2FResult::Error(ref err) = *res {
        return *err as u8;
    }

    return 0; /* No error, the request succeeded. */
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

    if let U2FResult::Success(ref bufs) = *res {
        if let Some(buf) = bufs.get(&bid) {
            *len = buf.len();
            return true;
        }
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

    if let U2FResult::Success(ref bufs) = *res {
        if let Some(buf) = bufs.get(&bid) {
            ptr::copy_nonoverlapping(buf.as_ptr(), dst, buf.len());
            return true;
        }
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
            let result = match rv {
                Ok(registration) => {
                    let mut bufs = HashMap::new();
                    bufs.insert(RESBUF_ID_REGISTRATION, registration);
                    U2FResult::Success(bufs)
                }
                Err(e) => U2FResult::Error(e),
            };

            callback(tid, Box::into_raw(Box::new(result)));
        },
    );

    if res.is_ok() {
        tid
    } else {
        0
    }
}

#[no_mangle]
pub unsafe extern "C" fn rust_u2f_mgr_sign(
    mgr: *mut U2FManager,
    flags: u64,
    timeout: u64,
    callback: U2FCallback,
    challenge_ptr: *const u8,
    challenge_len: usize,
    app_ids: *const U2FAppIds,
    khs: *const U2FKeyHandles,
) -> u64 {
    if mgr.is_null() || khs.is_null() {
        return 0;
    }

    // Check buffers.
    if challenge_ptr.is_null() {
        return 0;
    }

    // Need at least one app_id.
    if (*app_ids).len() < 1 {
        return 0;
    }

    let flags = ::SignFlags::from_bits_truncate(flags);
    let challenge = from_raw(challenge_ptr, challenge_len);
    let app_ids = (*app_ids).clone();
    let key_handles = (*khs).clone();

    let tid = new_tid();
    let res = (*mgr).sign(flags, timeout, challenge, app_ids, key_handles, move |rv| {
        let result = match rv {
            Ok((app_id, key_handle, signature)) => {
                let mut bufs = HashMap::new();
                bufs.insert(RESBUF_ID_KEYHANDLE, key_handle);
                bufs.insert(RESBUF_ID_SIGNATURE, signature);
                bufs.insert(RESBUF_ID_APPID, app_id);
                U2FResult::Success(bufs)
            }
            Err(e) => U2FResult::Error(e),
        };

        callback(tid, Box::into_raw(Box::new(result)));
    });

    if res.is_ok() {
        tid
    } else {
        0
    }
}

#[no_mangle]
pub unsafe extern "C" fn rust_u2f_mgr_cancel(mgr: *mut U2FManager) {
    if !mgr.is_null() {
        // Ignore return value.
        let _ = (*mgr).cancel();
    }
}

// Copyright 2015-2016 Mozilla Foundation. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Adapted from third_party/rust/encoding_rs/src/lib.rs, so the
// "top-level directory" in the above notice refers to
// third_party/rust/encoding_rs/.

extern crate encoding_rs;
extern crate nserror;
extern crate nsstring;

use encoding_rs::*;
use nserror::*;
use nsstring::*;
use std::slice;

/// Takes `Option<usize>`, the destination string and a value
/// to return on failure and tries to start a bulk write of the
/// destination string with the capacity given by the `usize`
/// wrapped in the first argument. Returns the bulk write
/// handle.
macro_rules! try_start_bulk_write {
    ($needed:expr,
     $dst:ident,
     $ret:expr) => ({
        let needed = match $needed {
            Some(needed) => {
                needed
            }
            None => {
                return $ret;
            }
        };
        match unsafe { $dst.bulk_write(needed, 0, false) } {
            Err(_) => {
                return $ret;
            },
            Ok(handle) => {
                handle
            }
        }
    })
}

#[no_mangle]
pub unsafe extern "C" fn mozilla_encoding_decode_to_nsstring(encoding: *mut *const Encoding,
                                                             src: *const u8,
                                                             src_len: usize,
                                                             dst: *mut nsAString)
                                                             -> nsresult {
    let (rv, enc) = decode_to_nsstring(&**encoding, slice::from_raw_parts(src, src_len), &mut *dst);
    *encoding = enc as *const Encoding;
    rv
}

pub fn decode_to_nsstring(encoding: &'static Encoding,
                          src: &[u8],
                          dst: &mut nsAString)
                          -> (nsresult, &'static Encoding) {
    if let Some((enc, bom_length)) = Encoding::for_bom(src) {
        return (decode_to_nsstring_without_bom_handling(enc, &src[bom_length..], dst), enc);
    }
    (decode_to_nsstring_without_bom_handling(encoding, src, dst), encoding)
}

#[no_mangle]
pub unsafe extern "C" fn mozilla_encoding_decode_to_nsstring_with_bom_removal(encoding: *const Encoding, src: *const u8, src_len: usize, dst: *mut nsAString) -> nsresult{
    decode_to_nsstring_with_bom_removal(&*encoding, slice::from_raw_parts(src, src_len), &mut *dst)
}

pub fn decode_to_nsstring_with_bom_removal(encoding: &'static Encoding,
                                           src: &[u8],
                                           dst: &mut nsAString)
                                           -> nsresult {
    let without_bom = if encoding == UTF_8 && src.starts_with(b"\xEF\xBB\xBF") {
        &src[3..]
    } else if (encoding == UTF_16LE && src.starts_with(b"\xFF\xFE")) ||
              (encoding == UTF_16BE && src.starts_with(b"\xFE\xFF")) {
        &src[2..]
    } else {
        src
    };
    decode_to_nsstring_without_bom_handling(encoding, without_bom, dst)
}

#[no_mangle]
pub unsafe extern "C" fn mozilla_encoding_decode_to_nsstring_without_bom_handling(encoding: *const Encoding, src: *const u8, src_len: usize, dst: *mut nsAString) -> nsresult{
    decode_to_nsstring_without_bom_handling(&*encoding,
                                            slice::from_raw_parts(src, src_len),
                                            &mut *dst)
}

pub fn decode_to_nsstring_without_bom_handling(encoding: &'static Encoding,
                                               src: &[u8],
                                               dst: &mut nsAString)
                                               -> nsresult {
    let mut decoder = encoding.new_decoder_without_bom_handling();
    let mut handle = try_start_bulk_write!(decoder.max_utf16_buffer_length(src.len()),
                                           dst,
                                           NS_ERROR_OUT_OF_MEMORY);
    let (result, read, written, had_errors) = decoder.decode_to_utf16(src, handle.as_mut_slice(), true);
    debug_assert_eq!(result, CoderResult::InputEmpty);
    debug_assert_eq!(read, src.len());
    debug_assert!(written <= handle.as_mut_slice().len());
    let _ = handle.finish(written, true);
    if had_errors {
        return NS_OK_HAD_REPLACEMENTS;
    }
    NS_OK
}

#[no_mangle]
pub unsafe extern "C" fn mozilla_encoding_decode_to_nsstring_without_bom_handling_and_without_replacement
    (encoding: *const Encoding,
     src: *const u8,
     src_len: usize,
     dst: *mut nsAString)
     -> nsresult {
    decode_to_nsstring_without_bom_handling_and_without_replacement(&*encoding,
                                                                    slice::from_raw_parts(src,
                                                                                          src_len),
                                                                    &mut *dst)
}

pub fn decode_to_nsstring_without_bom_handling_and_without_replacement(encoding: &'static Encoding, src: &[u8], dst: &mut nsAString) -> nsresult{
    let mut decoder = encoding.new_decoder_without_bom_handling();
    let mut handle = try_start_bulk_write!(decoder.max_utf16_buffer_length(src.len()),
                                           dst,
                                           NS_ERROR_OUT_OF_MEMORY);
    let (result, read, written) = decoder
        .decode_to_utf16_without_replacement(src, handle.as_mut_slice(), true);
    match result {
        DecoderResult::InputEmpty => {
            debug_assert_eq!(read, src.len());
            debug_assert!(written <= handle.as_mut_slice().len());
            let _ = handle.finish(written, true);
            NS_OK
        }
        DecoderResult::Malformed(_, _) => {
            // Let handle's drop() run
            NS_ERROR_UDEC_ILLEGALINPUT
        }
        DecoderResult::OutputFull => unreachable!(),
    }
}

#[no_mangle]
pub unsafe extern "C" fn mozilla_encoding_encode_from_utf16(encoding: *mut *const Encoding,
                                                            src: *const u16,
                                                            src_len: usize,
                                                            dst: *mut nsACString)
                                                            -> nsresult {
    let (rv, enc) = encode_from_utf16(&**encoding, slice::from_raw_parts(src, src_len), &mut *dst);
    *encoding = enc as *const Encoding;
    rv
}

pub fn encode_from_utf16(encoding: &'static Encoding,
                         src: &[u16],
                         dst: &mut nsACString)
                         -> (nsresult, &'static Encoding) {
    let output_encoding = encoding.output_encoding();
    let mut encoder = output_encoding.new_encoder();
    let mut handle = try_start_bulk_write!(encoder.max_buffer_length_from_utf16_if_no_unmappables(src.len()),
                                           dst,
                                           (NS_ERROR_OUT_OF_MEMORY, output_encoding));

    let mut total_read = 0;
    let mut total_written = 0;
    let mut total_had_errors = false;
    loop {
        let (result, read, written, had_errors) = encoder
            .encode_from_utf16(&src[total_read..],
                               &mut (handle.as_mut_slice())[total_written..],
                               true);
        total_read += read;
        total_written += written;
        total_had_errors |= had_errors;
        match result {
            CoderResult::InputEmpty => {
                debug_assert_eq!(total_read, src.len());
                debug_assert!(total_written <= handle.as_mut_slice().len());
                let _ = handle.finish(total_written, true);
                if total_had_errors {
                    return (NS_OK_HAD_REPLACEMENTS, output_encoding);
                }
                return (NS_OK, output_encoding);
            }
            CoderResult::OutputFull => {
                if let Some(needed) =
                    checked_add(total_written,
                                encoder.max_buffer_length_from_utf16_if_no_unmappables(src.len() -
                                                                                       total_read)) {
                    if unsafe { handle.restart_bulk_write(needed, total_written, false).is_ok() } {
                        continue;
                    }
                }
                return (NS_ERROR_OUT_OF_MEMORY, output_encoding);
            }
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn mozilla_encoding_decode_to_nscstring(encoding: *mut *const Encoding,
                                                              src: *const nsACString,
                                                              dst: *mut nsACString)
                                                              -> nsresult {
    debug_assert_ne!(src as usize, dst as usize);
    let (rv, enc) = decode_to_nscstring(&**encoding, &*src, &mut *dst);
    *encoding = enc as *const Encoding;
    rv
}

pub fn decode_to_nscstring(encoding: &'static Encoding,
                           src: &nsACString,
                           dst: &mut nsACString)
                           -> (nsresult, &'static Encoding) {
    if let Some((enc, bom_length)) = Encoding::for_bom(src) {
        return (decode_from_slice_to_nscstring_without_bom_handling(enc,
                                                                    &src[bom_length..],
                                                                    dst,
                                                                    0),
                enc);
    }
    (decode_to_nscstring_without_bom_handling(encoding, src, dst), encoding)
}

#[no_mangle]
pub unsafe extern "C" fn mozilla_encoding_decode_to_nscstring_with_bom_removal(encoding: *const Encoding, src: *const nsACString, dst: *mut nsACString) -> nsresult{
    debug_assert_ne!(src as usize, dst as usize);
    decode_to_nscstring_with_bom_removal(&*encoding, &*src, &mut *dst)
}

pub fn decode_to_nscstring_with_bom_removal(encoding: &'static Encoding,
                                            src: &nsACString,
                                            dst: &mut nsACString)
                                            -> nsresult {
    let without_bom = if encoding == UTF_8 && src.starts_with(b"\xEF\xBB\xBF") {
        &src[3..]
    } else if (encoding == UTF_16LE && src.starts_with(b"\xFF\xFE")) ||
              (encoding == UTF_16BE && src.starts_with(b"\xFE\xFF")) {
        &src[2..]
    } else {
        return decode_to_nscstring_without_bom_handling(encoding, src, dst);
    };
    decode_from_slice_to_nscstring_without_bom_handling(encoding, without_bom, dst, 0)
}

#[no_mangle]
pub unsafe extern "C" fn mozilla_encoding_decode_to_nscstring_without_bom_handling(encoding: *const Encoding, src: *const nsACString, dst: *mut nsACString) -> nsresult{
    debug_assert_ne!(src as usize, dst as usize);
    decode_to_nscstring_without_bom_handling(&*encoding, &*src, &mut *dst)
}

pub fn decode_to_nscstring_without_bom_handling(encoding: &'static Encoding,
                                                src: &nsACString,
                                                dst: &mut nsACString)
                                                -> nsresult {
    let bytes = &src[..];
    let valid_up_to = if encoding == UTF_8 {
        Encoding::utf8_valid_up_to(bytes)
    } else if encoding.is_ascii_compatible() {
        Encoding::ascii_valid_up_to(bytes)
    } else if encoding == ISO_2022_JP {
        Encoding::iso_2022_jp_ascii_valid_up_to(bytes)
    } else {
        return decode_from_slice_to_nscstring_without_bom_handling(encoding, src, dst, 0);
    };
    if valid_up_to == bytes.len() {
        if dst.fallible_assign(src).is_err() {
            return NS_ERROR_OUT_OF_MEMORY;
        }
        return NS_OK;
    }
    decode_from_slice_to_nscstring_without_bom_handling(encoding, src, dst, valid_up_to)
}

#[no_mangle]
pub unsafe extern "C" fn mozilla_encoding_decode_from_slice_to_nscstring_without_bom_handling(encoding: *const Encoding, src: *const u8, src_len: usize, dst: *mut nsACString, already_validated: usize) -> nsresult {
    decode_from_slice_to_nscstring_without_bom_handling(&*encoding, slice::from_raw_parts(src, src_len), &mut *dst, already_validated)
}

fn decode_from_slice_to_nscstring_without_bom_handling(encoding: &'static Encoding,
                                                       src: &[u8],
                                                       dst: &mut nsACString,
                                                       already_validated: usize)
                                                       -> nsresult {
    let bytes = src;
    let mut decoder = encoding.new_decoder_without_bom_handling();
    let mut handle = try_start_bulk_write!(Some(src.len()),
                                           dst,
                                           NS_ERROR_OUT_OF_MEMORY);

    if already_validated != 0 {
        &mut (handle.as_mut_slice())[..already_validated].copy_from_slice(&bytes[..already_validated]);
    }
    let mut total_read = already_validated;
    let mut total_written = already_validated;
    let mut total_had_errors = false;
    loop {
        let (result, read, written, had_errors) =
            decoder.decode_to_utf8(&bytes[total_read..],
                                   &mut (handle.as_mut_slice())[total_written..],
                                   true);
        total_read += read;
        total_written += written;
        total_had_errors |= had_errors;
        match result {
            CoderResult::InputEmpty => {
                debug_assert_eq!(total_read, bytes.len());
                let _ = handle.finish(total_written, true);
                if total_had_errors {
                    return NS_OK_HAD_REPLACEMENTS;
                }
                return NS_OK;
            }
            CoderResult::OutputFull => {
                // Allocate for the worst case. That is, we should come
                // here at most once per invocation of this method.
                if let Some(needed) =
                    checked_add(total_written,
                                decoder.max_utf8_buffer_length(bytes.len() -
                                                               total_read)) {
                    if unsafe { handle.restart_bulk_write(needed, total_written, false).is_ok() } {
                        continue;
                    }
                }
                return NS_ERROR_OUT_OF_MEMORY;
            }
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn mozilla_encoding_decode_to_nscstring_without_bom_handling_and_without_replacement
    (encoding: *const Encoding,
     src: *const nsACString,
     dst: *mut nsACString)
     -> nsresult {
    decode_to_nscstring_without_bom_handling_and_without_replacement(&*encoding, &*src, &mut *dst)
}

pub fn decode_to_nscstring_without_bom_handling_and_without_replacement(encoding: &'static Encoding, src: &nsACString, dst: &mut nsACString) -> nsresult{
    let bytes = &src[..];
    if encoding == UTF_8 {
        let valid_up_to = Encoding::utf8_valid_up_to(bytes);
        if valid_up_to == bytes.len() {
            if dst.fallible_assign(src).is_err() {
                return NS_ERROR_OUT_OF_MEMORY;
            }
            return NS_OK;
        }
        return NS_ERROR_UDEC_ILLEGALINPUT;
    }
    let valid_up_to = if encoding.is_ascii_compatible() {
        Encoding::ascii_valid_up_to(bytes)
    } else if encoding == ISO_2022_JP {
        Encoding::iso_2022_jp_ascii_valid_up_to(bytes)
    } else {
        0
    };
    if valid_up_to == bytes.len() {
        if dst.fallible_assign(src).is_err() {
            return NS_ERROR_OUT_OF_MEMORY;
        }
        return NS_OK;
    }
    let mut decoder = encoding.new_decoder_without_bom_handling();
    let mut handle = try_start_bulk_write!(checked_add(valid_up_to,
                                                       decoder.max_utf8_buffer_length_without_replacement(bytes.len() -
                                                                                                          valid_up_to)),
                                           dst,
                                           NS_ERROR_OUT_OF_MEMORY);
    let (result, read, written) = {
        let dest = handle.as_mut_slice();
        dest[..valid_up_to].copy_from_slice(&bytes[..valid_up_to]);
        decoder
            .decode_to_utf8_without_replacement(&src[valid_up_to..], &mut dest[valid_up_to..], true)
    };
    match result {
        DecoderResult::InputEmpty => {
            debug_assert_eq!(valid_up_to + read, src.len());
            debug_assert!(valid_up_to + written <= handle.as_mut_slice().len());
            let _ = handle.finish(valid_up_to + written, true);
            NS_OK
        }
        DecoderResult::Malformed(_, _) => {
            // let handle's drop() run
            NS_ERROR_UDEC_ILLEGALINPUT
        }
        DecoderResult::OutputFull => unreachable!(),
    }
}

#[no_mangle]
pub unsafe extern "C" fn mozilla_encoding_encode_from_nscstring(encoding: *mut *const Encoding,
                                                                src: *const nsACString,
                                                                dst: *mut nsACString)
                                                                -> nsresult {
    let (rv, enc) = encode_from_nscstring(&**encoding, &*src, &mut *dst);
    *encoding = enc as *const Encoding;
    rv
}

pub fn encode_from_nscstring(encoding: &'static Encoding,
                             src: &nsACString,
                             dst: &mut nsACString)
                             -> (nsresult, &'static Encoding) {
    let output_encoding = encoding.output_encoding();
    let bytes = &src[..];
    if output_encoding == UTF_8 {
        let valid_up_to = Encoding::utf8_valid_up_to(bytes);
        if valid_up_to == bytes.len() {
            if dst.fallible_assign(src).is_err() {
                return (NS_ERROR_OUT_OF_MEMORY, output_encoding);
            }
            return (NS_OK, output_encoding);
        }
        return (NS_ERROR_UDEC_ILLEGALINPUT, output_encoding);
    }
    let valid_up_to = if output_encoding == ISO_2022_JP {
        Encoding::iso_2022_jp_ascii_valid_up_to(bytes)
    } else {
        debug_assert!(output_encoding.is_ascii_compatible());
        Encoding::ascii_valid_up_to(bytes)
    };
    if valid_up_to == bytes.len() {
        if dst.fallible_assign(src).is_err() {
            return (NS_ERROR_OUT_OF_MEMORY, output_encoding);
        }
        return (NS_OK, output_encoding);
    }

    // Encoder requires valid UTF-8. Using std instead of encoding_rs
    // to avoid unsafe blocks.
    let trail = if let Ok(trail) = ::std::str::from_utf8(&bytes[valid_up_to..]) {
        trail
    } else {
        return (NS_ERROR_UDEC_ILLEGALINPUT, output_encoding);
    };

    let mut encoder = output_encoding.new_encoder();
    let mut handle = try_start_bulk_write!(checked_add(valid_up_to,
                                                       encoder.max_buffer_length_from_utf8_if_no_unmappables(trail.len())),
                                           dst,
                                           (NS_ERROR_OUT_OF_MEMORY, output_encoding));

    if valid_up_to != 0 {
        // to_mut() shouldn't fail right after setting length.
        &mut (handle.as_mut_slice())[..valid_up_to].copy_from_slice(&bytes[..valid_up_to]);
    }

    // `total_read` tracks `trail` only but `total_written` tracks the overall situation!
    // This asymmetry is here, because trail is materialized as `str` without resorting
    // to unsafe code here.
    let mut total_read = 0;
    let mut total_written = valid_up_to;
    let mut total_had_errors = false;
    loop {
        let (result, read, written, had_errors) = encoder
            .encode_from_utf8(&trail[total_read..],
                              &mut (handle.as_mut_slice())[total_written..],
                              true);
        total_read += read;
        total_written += written;
        total_had_errors |= had_errors;
        match result {
            CoderResult::InputEmpty => {
                debug_assert_eq!(valid_up_to + total_read, src.len());
                debug_assert!(total_written <= handle.as_mut_slice().len());
                let _ = handle.finish(total_written, true);
                if total_had_errors {
                    return (NS_OK_HAD_REPLACEMENTS, output_encoding);
                }
                return (NS_OK, output_encoding);
            }
            CoderResult::OutputFull => {
                if let Some(needed) =
                    checked_add(total_written,
                                encoder
                                    .max_buffer_length_from_utf8_if_no_unmappables(trail.len() -
                                                                                   total_read)) {
                    if unsafe { handle.restart_bulk_write(needed, total_written, false).is_ok() } {
                        continue;
                    }
                }
                return (NS_ERROR_OUT_OF_MEMORY, output_encoding);
            }
        }
    }
}

#[inline(always)]
fn checked_add(num: usize, opt: Option<usize>) -> Option<usize> {
    if let Some(n) = opt {
        n.checked_add(num)
    } else {
        None
    }
}

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
extern crate nsstring;
extern crate nserror;

use std::slice;
use std::cmp::Ordering;
use encoding_rs::*;
use nsstring::*;
use nserror::*;

// nsStringBuffer's internal bookkeeping takes 8 bytes from
// the allocation. Plus one for termination.
const NS_CSTRING_OVERHEAD: usize = 9;

/// Takes `Option<usize>`, the destination string and a value
/// to return on failure and tries to set the length of the
/// destination string to the `usize` wrapped in the first
/// argument.
macro_rules! try_dst_set_len {
    ($needed:expr,
     $dst:ident,
     $ret:expr) => (
    let needed = match $needed {
        Some(max) => {
            // XPCOM strings use uint32_t for length.
            if max > ::std::u32::MAX as usize {
                return $ret;
            }
            max as u32
        }
        None => {
            return $ret;
        }
    };
    unsafe {
        if $dst.fallible_set_length(needed).is_err() {
            return $ret;
        }
    }
     )
}

static ENCODINGS_SORTED_BY_NAME: [&'static Encoding; 39] = [&GBK_INIT,
                                                            &BIG5_INIT,
                                                            &IBM866_INIT,
                                                            &EUC_JP_INIT,
                                                            &KOI8_R_INIT,
                                                            &EUC_KR_INIT,
                                                            &KOI8_U_INIT,
                                                            &GB18030_INIT,
                                                            &UTF_16BE_INIT,
                                                            &UTF_16LE_INIT,
                                                            &SHIFT_JIS_INIT,
                                                            &MACINTOSH_INIT,
                                                            &ISO_8859_2_INIT,
                                                            &ISO_8859_3_INIT,
                                                            &ISO_8859_4_INIT,
                                                            &ISO_8859_5_INIT,
                                                            &ISO_8859_6_INIT,
                                                            &ISO_8859_7_INIT,
                                                            &ISO_8859_8_INIT,
                                                            &ISO_8859_10_INIT,
                                                            &ISO_8859_13_INIT,
                                                            &ISO_8859_14_INIT,
                                                            &WINDOWS_874_INIT,
                                                            &ISO_8859_15_INIT,
                                                            &ISO_8859_16_INIT,
                                                            &ISO_2022_JP_INIT,
                                                            &REPLACEMENT_INIT,
                                                            &WINDOWS_1250_INIT,
                                                            &WINDOWS_1251_INIT,
                                                            &WINDOWS_1252_INIT,
                                                            &WINDOWS_1253_INIT,
                                                            &WINDOWS_1254_INIT,
                                                            &WINDOWS_1255_INIT,
                                                            &WINDOWS_1256_INIT,
                                                            &WINDOWS_1257_INIT,
                                                            &WINDOWS_1258_INIT,
                                                            &ISO_8859_8_I_INIT,
                                                            &X_MAC_CYRILLIC_INIT,
                                                            &X_USER_DEFINED_INIT];

/// If the argument matches exactly (case-sensitively; no whitespace
/// removal performed) the name of an encoding, returns
/// `const Encoding*` representing that encoding. Otherwise panics.
///
/// The motivating use case for this function is interoperability with
/// legacy Gecko code that represents encodings as name string instead of
/// type-safe `Encoding` objects. Using this function for other purposes is
/// most likely the wrong thing to do.
///
/// `name` must be non-`NULL` even if `name_len` is zero. When `name_len`
/// is zero, it is OK for `name` to be something non-dereferencable,
/// such as `0x1`. This is required due to Rust's optimization for slices
/// within `Option`.
///
/// # Panics
///
/// Panics if the argument is not the name of an encoding.
///
/// # Undefined behavior
///
/// UB ensues if `name` and `name_len` don't designate a valid memory block
/// of if `name` is `NULL`.
#[no_mangle]
pub unsafe extern "C" fn mozilla_encoding_for_name(name: *const u8, name_len: usize) -> *const Encoding {
    let name_slice = ::std::slice::from_raw_parts(name, name_len);
    encoding_for_name(name_slice)
}

/// If the argument matches exactly (case-sensitively; no whitespace
/// removal performed) the name of an encoding, returns
/// `&'static Encoding` representing that encoding. Otherwise panics.
///
/// The motivating use case for this method is interoperability with
/// legacy Gecko code that represents encodings as name string instead of
/// type-safe `Encoding` objects. Using this method for other purposes is
/// most likely the wrong thing to do.
///
/// Available via the C wrapper.
///
/// # Panics
///
/// Panics if the argument is not the name of an encoding.
#[cfg_attr(feature = "cargo-clippy", allow(match_wild_err_arm))]
pub fn encoding_for_name(name: &[u8]) -> &'static Encoding {
    // The length of `"UTF-8"` is unique, so it's easy to check the most
    // common case first.
    if name.len() == 5 {
        assert_eq!(name, b"UTF-8", "Bogus encoding name");
        return UTF_8;
    }
    match ENCODINGS_SORTED_BY_NAME.binary_search_by(
        |probe| {
            let bytes = probe.name().as_bytes();
            let c = bytes.len().cmp(&name.len());
            if c != Ordering::Equal {
                return c;
            }
            let probe_iter = bytes.iter().rev();
            let candidate_iter = name.iter().rev();
            probe_iter.cmp(candidate_iter)
        }
    ) {
        Ok(i) => ENCODINGS_SORTED_BY_NAME[i],
        Err(_) => panic!("Bogus encoding name"),
    }
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
    try_dst_set_len!(decoder.max_utf16_buffer_length(src.len()),
                     dst,
                     NS_ERROR_OUT_OF_MEMORY);
    // to_mut() shouldn't fail right after setting length.
    let (result, read, written, had_errors) = decoder.decode_to_utf16(src, dst.to_mut(), true);
    debug_assert_eq!(result, CoderResult::InputEmpty);
    debug_assert_eq!(read, src.len());
    debug_assert!(written <= dst.len());
    unsafe {
        if dst.fallible_set_length(written as u32).is_err() {
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }
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
    try_dst_set_len!(decoder.max_utf16_buffer_length(src.len()),
                     dst,
                     NS_ERROR_OUT_OF_MEMORY);
    // to_mut() shouldn't fail right after setting length.
    let (result, read, written) = decoder
        .decode_to_utf16_without_replacement(src, dst.to_mut(), true);
    match result {
        DecoderResult::InputEmpty => {
            debug_assert_eq!(read, src.len());
            debug_assert!(written <= dst.len());
            unsafe {
                if dst.fallible_set_length(written as u32).is_err() {
                    return NS_ERROR_OUT_OF_MEMORY;
                }
            }
            NS_OK
        }
        DecoderResult::Malformed(_, _) => {
            dst.truncate();
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
    try_dst_set_len!(encoder.max_buffer_length_from_utf16_if_no_unmappables(src.len()),
                     dst,
                     (NS_ERROR_OUT_OF_MEMORY, output_encoding));

    let mut total_read = 0;
    let mut total_written = 0;
    let mut total_had_errors = false;
    loop {
        let (result, read, written, had_errors) = encoder
            .encode_from_utf16(&src[total_read..],
                               &mut (dst.to_mut())[total_written..],
                               true);
        total_read += read;
        total_written += written;
        total_had_errors |= had_errors;
        match result {
            CoderResult::InputEmpty => {
                debug_assert_eq!(total_read, src.len());
                debug_assert!(total_written <= dst.len());
                unsafe {
                    if dst.fallible_set_length(total_written as u32).is_err() {
                        return (NS_ERROR_OUT_OF_MEMORY, output_encoding);
                    }
                }
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
                    // Let's round the allocation up in order to avoid repeated
                    // allocations. Using power-of-two as the approximation of
                    // available jemalloc buckets, since linking with
                    // malloc_good_size is messy.
                    if let Some(with_bookkeeping) = NS_CSTRING_OVERHEAD.checked_add(needed) {
                        let rounded = with_bookkeeping.next_power_of_two();
                        let unclowned = rounded - NS_CSTRING_OVERHEAD;
                        // XPCOM strings use uint32_t for length.
                        if unclowned <= ::std::u32::MAX as usize {
                            unsafe {
                                if dst.fallible_set_length(unclowned as u32).is_ok() {
                                    continue;
                                }
                            }
                        }
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
    let rounded_without_replacement =
        checked_next_power_of_two(checked_add(already_validated, decoder.max_utf8_buffer_length_without_replacement(bytes.len() - already_validated)));
    let with_replacement = checked_add(already_validated,
                                       decoder.max_utf8_buffer_length(bytes.len() -
                                                                      already_validated));
    try_dst_set_len!(checked_min(rounded_without_replacement, with_replacement),
                     dst,
                     NS_ERROR_OUT_OF_MEMORY);

    if already_validated != 0 {
        // to_mut() shouldn't fail right after setting length.
        &mut (dst.to_mut())[..already_validated].copy_from_slice(&bytes[..already_validated]);
    }
    let mut total_read = already_validated;
    let mut total_written = already_validated;
    let mut total_had_errors = false;
    loop {
        // to_mut() shouldn't fail right after setting length.
        let (result, read, written, had_errors) =
            decoder.decode_to_utf8(&bytes[total_read..],
                                   &mut (dst.to_mut())[total_written..],
                                   true);
        total_read += read;
        total_written += written;
        total_had_errors |= had_errors;
        match result {
            CoderResult::InputEmpty => {
                debug_assert_eq!(total_read, bytes.len());
                unsafe {
                    if dst.fallible_set_length(total_written as u32).is_err() {
                        return NS_ERROR_OUT_OF_MEMORY;
                    }
                }
                if total_had_errors {
                    return NS_OK_HAD_REPLACEMENTS;
                }
                return NS_OK;
            }
            CoderResult::OutputFull => {
                // Allocate for the worst case. That is, we should come
                // here at most once per invocation of this method.
                try_dst_set_len!(checked_add(total_written,
                                             decoder.max_utf8_buffer_length(bytes.len() -
                                                                            total_read)),
                                 dst,
                                 NS_ERROR_OUT_OF_MEMORY);
                continue;
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
    try_dst_set_len!(checked_add(valid_up_to,
                                 decoder.max_utf8_buffer_length_without_replacement(bytes.len() -
                                                                                    valid_up_to)),
                     dst,
                     NS_ERROR_OUT_OF_MEMORY);
    // to_mut() shouldn't fail right after setting length.
    let (result, read, written) = {
        let dest = dst.to_mut();
        dest[..valid_up_to].copy_from_slice(&bytes[..valid_up_to]);
        decoder
            .decode_to_utf8_without_replacement(&src[valid_up_to..], &mut dest[valid_up_to..], true)
    };
    match result {
        DecoderResult::InputEmpty => {
            debug_assert_eq!(valid_up_to + read, src.len());
            debug_assert!(valid_up_to + written <= dst.len());
            unsafe {
                if dst.fallible_set_length((valid_up_to + written) as u32)
                       .is_err() {
                    return NS_ERROR_OUT_OF_MEMORY;
                }
            }
            NS_OK
        }
        DecoderResult::Malformed(_, _) => {
            dst.truncate();
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
    try_dst_set_len!(checked_add(valid_up_to,
                    encoder.max_buffer_length_from_utf8_if_no_unmappables(trail.len())), dst, (NS_ERROR_OUT_OF_MEMORY, output_encoding));

    if valid_up_to != 0 {
        // to_mut() shouldn't fail right after setting length.
        &mut (dst.to_mut())[..valid_up_to].copy_from_slice(&bytes[..valid_up_to]);
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
                              &mut (dst.to_mut())[total_written..],
                              true);
        total_read += read;
        total_written += written;
        total_had_errors |= had_errors;
        match result {
            CoderResult::InputEmpty => {
                debug_assert_eq!(valid_up_to + total_read, src.len());
                debug_assert!(total_written <= dst.len());
                unsafe {
                    if dst.fallible_set_length(total_written as u32).is_err() {
                        return (NS_ERROR_OUT_OF_MEMORY, output_encoding);
                    }
                }
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
                    // Let's round the allocation up in order to avoid repeated
                    // allocations. Using power-of-two as the approximation of
                    // available jemalloc buckets, since linking with
                    // malloc_good_size is messy.
                    if let Some(with_bookkeeping) = NS_CSTRING_OVERHEAD.checked_add(needed) {
                        let rounded = with_bookkeeping.next_power_of_two();
                        let unclowned = rounded - NS_CSTRING_OVERHEAD;
                        // XPCOM strings use uint32_t for length.
                        if unclowned <= ::std::u32::MAX as usize {
                            unsafe {
                                if dst.fallible_set_length(unclowned as u32).is_ok() {
                                    continue;
                                }
                            }
                        }
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

#[inline(always)]
fn checked_next_power_of_two(opt: Option<usize>) -> Option<usize> {
    opt.map(|n| n.next_power_of_two())
}

#[inline(always)]
fn checked_min(one: Option<usize>, other: Option<usize>) -> Option<usize> {
    if let Some(a) = one {
        if let Some(b) = other {
            Some(::std::cmp::min(a, b))
        } else {
            Some(a)
        }
    } else {
        other
    }
}

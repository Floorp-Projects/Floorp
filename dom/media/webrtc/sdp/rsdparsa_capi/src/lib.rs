/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate libc;
extern crate nserror;
extern crate rsdparsa;

use std::ffi::CString;
use std::os::raw::c_char;
use std::ptr;

use libc::size_t;

use std::rc::Rc;

use std::convert::{TryFrom, TryInto};

use nserror::{nsresult, NS_ERROR_INVALID_ARG, NS_OK};
use rsdparsa::address::ExplicitlyTypedAddress;
use rsdparsa::anonymizer::{AnonymizingClone, StatefulSdpAnonymizer};
use rsdparsa::attribute_type::SdpAttribute;
use rsdparsa::error::SdpParserError;
use rsdparsa::media_type::{SdpMediaValue, SdpProtocolValue};
use rsdparsa::{SdpBandwidth, SdpSession, SdpTiming};

#[macro_use]
extern crate log;

pub mod attribute;
pub mod media_section;
pub mod network;
pub mod types;

use network::{
    get_bandwidth, origin_view_helper, RustAddressType, RustSdpConnection, RustSdpOrigin,
};
pub use types::{StringView, NULL_STRING};

#[no_mangle]
pub unsafe extern "C" fn parse_sdp(
    sdp: StringView,
    fail_on_warning: bool,
    session: *mut *const SdpSession,
    parser_error: *mut *const SdpParserError,
) -> nsresult {
    let sdp_str: String = match sdp.try_into() {
        Ok(string) => string,
        Err(boxed_error) => {
            *session = ptr::null();
            *parser_error = Box::into_raw(Box::new(SdpParserError::Sequence {
                message: format!("{}", boxed_error),
                line_number: 0,
            }));
            return NS_ERROR_INVALID_ARG;
        }
    };

    let parser_result = rsdparsa::parse_sdp(&sdp_str, fail_on_warning);
    match parser_result {
        Ok(mut parsed) => {
            *parser_error = match parsed.warnings.len() {
                0 => ptr::null(),
                _ => Box::into_raw(Box::new(parsed.warnings.remove(0))),
            };
            *session = Rc::into_raw(Rc::new(parsed));
            NS_OK
        }
        Err(e) => {
            *session = ptr::null();
            error!("Error parsing SDP in rust: {}", e);
            *parser_error = Box::into_raw(Box::new(e));
            NS_ERROR_INVALID_ARG
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn create_anonymized_sdp_clone(
    session: *const SdpSession,
) -> *const SdpSession {
    Rc::into_raw(Rc::new(
        (*session).masked_clone(&mut StatefulSdpAnonymizer::new()),
    ))
}

#[no_mangle]
pub unsafe extern "C" fn create_sdp_clone(session: *const SdpSession) -> *const SdpSession {
    Rc::into_raw(Rc::new((*session).clone()))
}

#[no_mangle]
pub unsafe extern "C" fn sdp_free_session(sdp_ptr: *mut SdpSession) {
    let sdp = Rc::from_raw(sdp_ptr);
    drop(sdp);
}

#[no_mangle]
pub unsafe extern "C" fn sdp_new_reference(session: *mut SdpSession) -> *const SdpSession {
    let original = Rc::from_raw(session);
    let ret = Rc::into_raw(Rc::clone(&original));
    Rc::into_raw(original); // So the original reference doesn't get dropped
    ret
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_error_line_num(parser_error: *mut SdpParserError) -> size_t {
    match *parser_error {
        SdpParserError::Line { line_number, .. }
        | SdpParserError::Unsupported { line_number, .. }
        | SdpParserError::Sequence { line_number, .. } => line_number,
    }
}

#[no_mangle]
// Callee must check that a nullptr is not returned
pub unsafe extern "C" fn sdp_get_error_message(parser_error: *mut SdpParserError) -> *mut c_char {
    let message = format!("{}", *parser_error);
    return match CString::new(message.as_str()) {
        Ok(c_char_ptr) => c_char_ptr.into_raw(),
        Err(_) => 0 as *mut c_char,
    };
}

#[no_mangle]
pub unsafe extern "C" fn sdp_free_error_message(message: *mut c_char) {
    if message != 0 as *mut c_char {
        let _tmp = CString::from_raw(message);
    }
}

#[no_mangle]
pub unsafe extern "C" fn sdp_free_error(parser_error: *mut SdpParserError) {
    let e = Box::from_raw(parser_error);
    drop(e);
}

#[no_mangle]
pub unsafe extern "C" fn get_version(session: *const SdpSession) -> u64 {
    (*session).get_version()
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_origin(session: *const SdpSession) -> RustSdpOrigin {
    origin_view_helper((*session).get_origin())
}

#[no_mangle]
pub unsafe extern "C" fn session_view(session: *const SdpSession) -> StringView {
    StringView::from((*session).get_session())
}

#[no_mangle]
pub unsafe extern "C" fn sdp_session_has_connection(session: *const SdpSession) -> bool {
    (*session).connection.is_some()
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_session_connection(
    session: *const SdpSession,
    connection: *mut RustSdpConnection,
) -> nsresult {
    match (*session).connection {
        Some(ref c) => {
            *connection = RustSdpConnection::from(c);
            NS_OK
        }
        None => NS_ERROR_INVALID_ARG,
    }
}

#[no_mangle]
pub unsafe extern "C" fn sdp_add_media_section(
    session: *mut SdpSession,
    media_type: u32,
    direction: u32,
    port: u16,
    protocol: u32,
    addr_type: u32,
    address: StringView,
) -> nsresult {
    let addr_type = match RustAddressType::try_from(addr_type) {
        Ok(a) => a.into(),
        Err(e) => {
            return e;
        }
    };
    let address_string: String = match address.try_into() {
        Ok(x) => x,
        Err(boxed_error) => {
            error!("Error while parsing string, description: {}", boxed_error);
            return NS_ERROR_INVALID_ARG;
        }
    };
    let address = match ExplicitlyTypedAddress::try_from((addr_type, address_string.as_str())) {
        Ok(a) => a,
        Err(_) => {
            return NS_ERROR_INVALID_ARG;
        }
    };

    let media_type = match media_type {
        0 => SdpMediaValue::Audio,       // MediaType::kAudio
        1 => SdpMediaValue::Video,       // MediaType::kVideo
        3 => SdpMediaValue::Application, // MediaType::kApplication
        _ => {
            return NS_ERROR_INVALID_ARG;
        }
    };
    let protocol = match protocol {
        20 => SdpProtocolValue::RtpSavpf,        // Protocol::kRtpSavpf
        21 => SdpProtocolValue::UdpTlsRtpSavp,   // Protocol::kUdpTlsRtpSavp
        22 => SdpProtocolValue::TcpDtlsRtpSavp,  // Protocol::kTcpDtlsRtpSavp
        24 => SdpProtocolValue::UdpTlsRtpSavpf,  // Protocol::kUdpTlsRtpSavpf
        25 => SdpProtocolValue::TcpDtlsRtpSavpf, // Protocol::kTcpTlsRtpSavpf
        37 => SdpProtocolValue::DtlsSctp,        // Protocol::kDtlsSctp
        38 => SdpProtocolValue::UdpDtlsSctp,     // Protocol::kUdpDtlsSctp
        39 => SdpProtocolValue::TcpDtlsSctp,     // Protocol::kTcpDtlsSctp
        _ => {
            return NS_ERROR_INVALID_ARG;
        }
    };
    let direction = match direction {
        1 => SdpAttribute::Sendonly,
        2 => SdpAttribute::Recvonly,
        3 => SdpAttribute::Sendrecv,
        _ => {
            return NS_ERROR_INVALID_ARG;
        }
    };

    match (*session).add_media(media_type, direction, port as u32, protocol, address) {
        Ok(_) => NS_OK,
        Err(_) => NS_ERROR_INVALID_ARG,
    }
}

#[repr(C)]
#[derive(Clone)]
pub struct RustSdpTiming {
    pub start: u64,
    pub stop: u64,
}

impl<'a> From<&'a SdpTiming> for RustSdpTiming {
    fn from(timing: &SdpTiming) -> Self {
        RustSdpTiming {
            start: timing.start,
            stop: timing.stop,
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn sdp_session_has_timing(session: *const SdpSession) -> bool {
    (*session).timing.is_some()
}

#[no_mangle]
pub unsafe extern "C" fn sdp_session_timing(
    session: *const SdpSession,
    timing: *mut RustSdpTiming,
) -> nsresult {
    match (*session).timing {
        Some(ref t) => {
            *timing = RustSdpTiming::from(t);
            NS_OK
        }
        None => NS_ERROR_INVALID_ARG,
    }
}

#[no_mangle]
pub unsafe extern "C" fn sdp_media_section_count(session: *const SdpSession) -> size_t {
    (*session).media.len()
}

#[no_mangle]
pub unsafe extern "C" fn get_sdp_bandwidth(
    session: *const SdpSession,
    bandwidth_type: *const c_char,
) -> u32 {
    get_bandwidth(&(*session).bandwidth, bandwidth_type)
}

#[no_mangle]
pub unsafe extern "C" fn sdp_get_session_bandwidth_vec(
    session: *const SdpSession,
) -> *const Vec<SdpBandwidth> {
    &(*session).bandwidth
}

#[no_mangle]
pub unsafe extern "C" fn get_sdp_session_attributes(
    session: *const SdpSession,
) -> *const Vec<SdpAttribute> {
    &(*session).attribute
}

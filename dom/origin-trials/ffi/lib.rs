/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

use origin_trial_token::{RawToken, Token, TokenValidationError, Usage};
use std::ffi::c_void;

#[repr(u8)]
pub enum OriginTrial {
    // NOTE(emilio): 0 is reserved for WebIDL usage.
    TestTrial = 1,
    OffscreenCanvas = 2,
    CoepCredentialless = 3,

    MAX,
}

impl OriginTrial {
    fn from_str(s: &str) -> Option<Self> {
        Some(match s {
            "TestTrial" => Self::TestTrial,
            "OffscreenCanvas" => Self::OffscreenCanvas,
            "CoepCredentialless" => Self::CoepCredentialless,
            _ => return None,
        })
    }
}

#[repr(u8)]
pub enum OriginTrialResult {
    Ok { trial: OriginTrial },
    BufferTooSmall,
    MismatchedPayloadSize { expected: usize, actual: usize },
    InvalidSignature,
    UnknownVersion,
    UnsupportedThirdPartyToken,
    UnexpectedUsageInNonThirdPartyToken,
    MalformedPayload,
    ExpiredToken,
    UnknownTrial,
    OriginMismatch,
}

impl OriginTrialResult {
    fn from_error(e: TokenValidationError) -> Self {
        match e {
            TokenValidationError::BufferTooSmall => OriginTrialResult::BufferTooSmall,
            TokenValidationError::MismatchedPayloadSize { expected, actual } => {
                OriginTrialResult::MismatchedPayloadSize { expected, actual }
            }
            TokenValidationError::InvalidSignature => OriginTrialResult::InvalidSignature,
            TokenValidationError::UnknownVersion => OriginTrialResult::UnknownVersion,
            TokenValidationError::UnsupportedThirdPartyToken => {
                OriginTrialResult::UnsupportedThirdPartyToken
            }
            TokenValidationError::UnexpectedUsageInNonThirdPartyToken => {
                OriginTrialResult::UnexpectedUsageInNonThirdPartyToken
            }
            TokenValidationError::MalformedPayload(..) => OriginTrialResult::MalformedPayload,
        }
    }
}

/// A struct that allows you to configure how validation on works, and pass
/// state to the signature verification.
#[repr(C)]
pub struct OriginTrialValidationParams {
    /// Verify a given signature against the signed data.
    pub verify_signature: extern "C" fn(
        signature: *const u8,
        signature_len: usize,
        data: *const u8,
        data_len: usize,
        user_data: *mut c_void,
    ) -> bool,

    /// Returns whether a given origin, which is passed as the first two
    /// arguments, and guaranteed to be valid UTF-8, passes the validation for a
    /// given invocation.
    pub matches_origin: extern "C" fn(
        origin: *const u8,
        len: usize,
        is_subdomain: bool,
        is_third_party: bool,
        is_usage_subset: bool,
        user_data: *mut c_void,
    ) -> bool,

    /// A pointer with user-supplied data that will be passed down to the
    /// other functions in this method.
    pub user_data: *mut c_void,
}

#[no_mangle]
pub unsafe extern "C" fn origin_trials_parse_and_validate_token(
    bytes: *const u8,
    len: usize,
    params: &OriginTrialValidationParams,
) -> OriginTrialResult {
    let slice = std::slice::from_raw_parts(bytes, len);
    let raw_token = match RawToken::from_buffer(slice) {
        Ok(token) => token,
        Err(e) => return OriginTrialResult::from_error(e),
    };

    // Verifying the token is usually more expensive than the early-outs here.
    let token = match Token::from_raw_token_unverified(raw_token) {
        Ok(token) => token,
        Err(e) => return OriginTrialResult::from_error(e),
    };

    if token.is_expired() {
        return OriginTrialResult::ExpiredToken;
    }

    let trial = match OriginTrial::from_str(token.feature()) {
        Some(t) => t,
        None => return OriginTrialResult::UnknownTrial,
    };

    let is_usage_subset = match token.usage {
        Usage::None => false,
        Usage::Subset => true,
    };

    if !(params.matches_origin)(
        token.origin.as_ptr(),
        token.origin.len(),
        token.is_subdomain,
        token.is_third_party,
        is_usage_subset,
        params.user_data,
    ) {
        return OriginTrialResult::OriginMismatch;
    }

    let valid_signature = raw_token.verify(|signature, data| {
        (params.verify_signature)(
            signature.as_ptr(),
            signature.len(),
            data.as_ptr(),
            data.len(),
            params.user_data,
        )
    });

    if !valid_signature {
        return OriginTrialResult::InvalidSignature;
    }

    OriginTrialResult::Ok { trial }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pub use base64::engine::general_purpose::*;
pub use base64::DecodeError;
use base64::Engine;

// Re-implement some of the 0.13 APIs on top of 0.21

pub fn decode<T: AsRef<[u8]>>(input: T) -> Result<Vec<u8>, DecodeError> {
    STANDARD.decode(input)
}

pub fn encode<T: AsRef<[u8]>>(input: T) -> String {
    STANDARD.encode(input)
}

pub fn decode_config<T: AsRef<[u8]>>(
    input: T,
    engine: GeneralPurpose,
) -> Result<Vec<u8>, DecodeError> {
    engine.decode(input)
}

pub fn encode_config<T: AsRef<[u8]>>(input: T, engine: GeneralPurpose) -> String {
    engine.encode(input)
}

pub fn encode_config_slice<T: AsRef<[u8]>>(
    input: T,
    engine: GeneralPurpose,
    output: &mut [u8],
) -> usize {
    engine
        .encode_slice(input, output)
        .expect("Output buffer too small")
}

pub fn encode_config_buf<T: AsRef<[u8]>>(input: T, engine: GeneralPurpose, buf: &mut String) {
    engine.encode_string(input, buf)
}

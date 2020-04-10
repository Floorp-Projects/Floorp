/* Copyright 2020 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#[no_mangle]
pub unsafe extern "C" fn wasm_text_to_binary(
    text: *const u16,
    text_len: usize,
    out_bytes: *mut *mut u8,
    out_bytes_len: *mut usize,
    out_error: *mut *mut u8,
    out_error_len: *mut usize,
) -> bool {
    let text_slice = std::slice::from_raw_parts(text, text_len);
    let text = String::from_utf16_lossy(text_slice);
    match wat::parse_str(&text) {
        Ok(bytes) => {
            let bytes_box = bytes.into_boxed_slice();
            let bytes_slice = Box::leak(bytes_box);
            out_bytes.write(bytes_slice.as_mut_ptr());
            out_bytes_len.write(bytes_slice.len());
            true
        }
        Err(error) => {
            let error = Box::leak(format!("{}\0", error).into_boxed_str());
            out_error.write(error.as_mut_ptr());
            out_error_len.write(error.len());
            false
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn wasm_code_offsets(
    bytes: *const u8,
    bytes_len: usize,
    out_offsets: *mut *mut u32,
    out_offsets_len: *mut usize,
) {
    fn code_offsets(bytes: &[u8]) -> Vec<u32> {
        use wasmparser::*;

        if bytes.is_empty() {
            return Vec::new();
        }

        let mut offsets = Vec::new();
        let mut parser = Parser::new(bytes);
        let mut next_input = ParserInput::Default;

        while !parser.eof() {
            let offset = parser.current_position();
            match parser.read_with_input(next_input) {
                ParserState::BeginSection { code, .. } if *code != SectionCode::Code => {
                    next_input = ParserInput::SkipSection;
                }
                ParserState::CodeOperator(..) => {
                    offsets.push(offset as u32);
                    next_input = ParserInput::Default
                }
                _ => next_input = ParserInput::Default,
            }
        }

        offsets
    }

    let bytes = std::slice::from_raw_parts(bytes, bytes_len);
    let offsets = code_offsets(bytes);

    if offsets.len() == 0 {
        out_offsets.write(std::ptr::null_mut());
        out_offsets_len.write(0);
    } else {
        let offsets_box = offsets.into_boxed_slice();
        let offsets_slice = Box::leak(offsets_box);
        out_offsets.write(offsets_slice.as_mut_ptr());
        out_offsets_len.write(offsets_slice.len());
    }
}

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

    match text_to_binary(&text) {
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

fn text_to_binary(text: &str) -> Result<Vec<u8>, wast::Error> {
    let mut lexer = wast::lexer::Lexer::new(text);
    // The 'names.wast' spec test has confusable unicode, so disable detection.
    // This protection is not very useful for a shell testing function anyways.
    lexer.allow_confusing_unicode(true);
    let buf = wast::parser::ParseBuffer::new_with_lexer(lexer)?;
    let mut ast = wast::parser::parse::<wast::Wat>(&buf)?;
    return ast.encode();
}

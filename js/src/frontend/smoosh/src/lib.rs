/* Copyright  Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0, or the MIT license,
 * (the "Licenses") at your option. You may not use this file except in
 * compliance with one of the Licenses. You may obtain copies of the
 * Licenses at:
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *    http://opensource.org/licenses/MIT
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the Licenses is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the Licenses for the specific language governing permissions and
 * limitations under the Licenses.
 */

use bumpalo;
use jsparagus::ast::types::Program;
use jsparagus::emitter::{emit, EmitError, EmitOptions, EmitResult};
use jsparagus::parser::{parse_module, parse_script, ParseError, ParseOptions};
use std::{mem, slice, str};

#[repr(C)]
pub struct CVec<T> {
    pub data: *mut T,
    pub len: usize,
    pub capacity: usize,
}

impl<T> CVec<T> {
    fn empty() -> CVec<T> {
        Self {
            data: std::ptr::null_mut(),
            len: 0,
            capacity: 0,
        }
    }

    fn from(mut v: Vec<T>) -> CVec<T> {
        let result = Self {
            data: v.as_mut_ptr(),
            len: v.len(),
            capacity: v.capacity(),
        };
        mem::forget(v);
        result
    }

    unsafe fn into(self) -> Vec<T> {
        Vec::from_raw_parts(self.data, self.len, self.capacity)
    }
}

#[repr(C)]
pub struct SmooshCompileOptions {
    no_script_rval: bool,
}

#[repr(C)]
pub struct SmooshResult {
    unimplemented: bool,
    error: CVec<u8>,
    bytecode: CVec<u8>,
    strings: CVec<CVec<u8>>,

    /// Line and column numbers for the first character of source.
    lineno: usize,
    column: usize,

    /// Offset of main entry point from code, after predef'ing prologue.
    main_offset: usize,

    /// Fixed frame slots.
    max_fixed_slots: u32,

    /// Maximum stack depth before any instruction.
    ///
    /// This value is a function of `bytecode`: there's only one correct value
    /// for a given script.
    maximum_stack_depth: u32,

    /// Index into the gcthings array of the body scope.
    body_scope_index: u32,

    /// Number of instructions in this script that have IC entries.
    ///
    /// A function of `bytecode`. See `JOF_IC`.
    num_ic_entries: u32,

    /// Number of instructions in this script that have JOF_TYPESET.
    num_type_sets: u32,

    /// `See BaseScript::ImmutableFlags`.
    strict: bool,
    bindings_accessed_dynamically: bool,
    has_call_site_obj: bool,
    is_for_eval: bool,
    is_module: bool,
    is_function: bool,
    has_non_syntactic_scope: bool,
    needs_function_environment_objects: bool,
    has_module_goal: bool,
}

enum SmooshError {
    GenericError(String),
    NotImplemented,
}

#[no_mangle]
pub unsafe extern "C" fn run_smoosh(
    text: *const u8,
    text_len: usize,
    options: &SmooshCompileOptions,
) -> SmooshResult {
    let text = str::from_utf8(slice::from_raw_parts(text, text_len)).expect("Invalid UTF8");
    match smoosh(text, options) {
        Ok(mut result) => SmooshResult {
            unimplemented: false,
            error: CVec::empty(),
            bytecode: CVec::from(result.bytecode),
            strings: CVec::from(
                result
                    .strings
                    .drain(..)
                    .map(|s| CVec::from(s.into_bytes()))
                    .collect(),
            ),
            lineno: result.lineno,
            column: result.column,
            main_offset: result.main_offset,
            max_fixed_slots: result.max_fixed_slots,
            maximum_stack_depth: result.maximum_stack_depth,
            body_scope_index: result.body_scope_index,
            num_ic_entries: result.num_ic_entries,
            num_type_sets: result.num_type_sets,
            strict: result.strict,
            bindings_accessed_dynamically: result.bindings_accessed_dynamically,
            has_call_site_obj: result.has_call_site_obj,
            is_for_eval: result.is_for_eval,
            is_module: result.is_module,
            is_function: result.is_function,
            has_non_syntactic_scope: result.has_non_syntactic_scope,
            needs_function_environment_objects: result.needs_function_environment_objects,
            has_module_goal: result.has_module_goal,
        },
        Err(SmooshError::GenericError(message)) => SmooshResult {
            unimplemented: false,
            error: CVec::from(format!("{}\0", message).into_bytes()),
            bytecode: CVec::empty(),
            strings: CVec::empty(),
            lineno: 0,
            column: 0,
            main_offset: 0,
            max_fixed_slots: 0,
            maximum_stack_depth: 0,
            body_scope_index: 0,
            num_ic_entries: 0,
            num_type_sets: 0,
            strict: false,
            bindings_accessed_dynamically: false,
            has_call_site_obj: false,
            is_for_eval: false,
            is_module: false,
            is_function: false,
            has_non_syntactic_scope: false,
            needs_function_environment_objects: false,
            has_module_goal: false,
        },
        Err(SmooshError::NotImplemented) => SmooshResult {
            unimplemented: true,
            error: CVec::empty(),
            bytecode: CVec::empty(),
            strings: CVec::empty(),
            lineno: 0,
            column: 0,
            main_offset: 0,
            max_fixed_slots: 0,
            maximum_stack_depth: 0,
            body_scope_index: 0,
            num_ic_entries: 0,
            num_type_sets: 0,
            strict: false,
            bindings_accessed_dynamically: false,
            has_call_site_obj: false,
            is_for_eval: false,
            is_module: false,
            is_function: false,
            has_non_syntactic_scope: false,
            needs_function_environment_objects: false,
            has_module_goal: false,
        },
    }
}

#[repr(C)]
pub struct SmooshParseResult {
    unimplemented: bool,
    error: CVec<u8>,
}

fn convert_parse_result<'alloc, T>(r: jsparagus::parser::Result<'alloc, T>) -> SmooshParseResult {
    match r {
        Ok(_) => SmooshParseResult {
            unimplemented: false,
            error: CVec::empty(),
        },
        Err(err) => match err {
            ParseError::NotImplemented(_) => {
                let message = err.message();
                SmooshParseResult {
                    unimplemented: true,
                    error: CVec::from(format!("{}\0", message).into_bytes()),
                }
            }
            _ => {
                let message = err.message();
                SmooshParseResult {
                    unimplemented: false,
                    error: CVec::from(format!("{}\0", message).into_bytes()),
                }
            }
        },
    }
}

#[no_mangle]
pub unsafe extern "C" fn test_parse_script(text: *const u8, text_len: usize) -> SmooshParseResult {
    let text = match str::from_utf8(slice::from_raw_parts(text, text_len)) {
        Ok(text) => text,
        Err(_) => {
            return SmooshParseResult {
                unimplemented: false,
                error: CVec::from("Invalid UTF-8\0".to_string().into_bytes()),
            };
        }
    };
    let allocator = bumpalo::Bump::new();
    let parse_options = ParseOptions::new();
    convert_parse_result(parse_script(&allocator, text, &parse_options))
}

#[no_mangle]
pub unsafe extern "C" fn test_parse_module(text: *const u8, text_len: usize) -> SmooshParseResult {
    let text = match str::from_utf8(slice::from_raw_parts(text, text_len)) {
        Ok(text) => text,
        Err(_) => {
            return SmooshParseResult {
                unimplemented: false,
                error: CVec::from("Invalid UTF-8\0".to_string().into_bytes()),
            };
        }
    };
    let allocator = bumpalo::Bump::new();
    let parse_options = ParseOptions::new();
    convert_parse_result(parse_module(&allocator, text, &parse_options))
}

#[no_mangle]
pub unsafe extern "C" fn free_smoosh_parse_result(result: SmooshParseResult) {
    let _ = result.error.into();
    //Vec::from_raw_parts(bytecode.data, bytecode.len, bytecode.capacity);
}

#[no_mangle]
pub unsafe extern "C" fn free_smoosh(result: SmooshResult) {
    let _ = result.bytecode.into();
    for v in result.strings.into() {
        let _ = v.into();
    }
    let _ = result.error.into();
    //Vec::from_raw_parts(bytecode.data, bytecode.len, bytecode.capacity);
}

fn smoosh(text: &str, options: &SmooshCompileOptions) -> Result<EmitResult, SmooshError> {
    let allocator = bumpalo::Bump::new();
    let parse_options = ParseOptions::new();
    let parse_result = match parse_script(&allocator, text, &parse_options) {
        Ok(result) => result,
        Err(err) => match err {
            ParseError::NotImplemented(_) => {
                println!("Unimplemented: {}", err.message());
                return Err(SmooshError::NotImplemented);
            }
            _ => {
                return Err(SmooshError::GenericError(err.message()));
            }
        },
    };

    let mut emit_options = EmitOptions::new();
    emit_options.no_script_rval = options.no_script_rval;
    match emit(&mut Program::Script(parse_result.unbox()), &emit_options) {
        Ok(result) => Ok(result),
        Err(EmitError::NotImplemented(message)) => {
            println!("Unimplemented: {}", message);
            return Err(SmooshError::NotImplemented);
        }
    }
}

#[cfg(test)]
mod tests {
    #[test]
    fn it_works() {
        assert_eq!(2 + 2, 4);
    }
}

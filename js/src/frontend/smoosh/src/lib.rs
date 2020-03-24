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
use env_logger;
use jsparagus::ast::source_atom_set::SourceAtomSet;
use jsparagus::ast::types::Program;
use jsparagus::emitter::{
    emit, BindingName, EmitError, EmitOptions, EmitResult, GCThing, ScopeData, ScopeNote,
};
use jsparagus::parser::{parse_module, parse_script, ParseError, ParseOptions};
use std::boxed::Box;
use std::cell::RefCell;
use std::os::raw::{c_char, c_void};
use std::rc::Rc;
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
pub enum SmooshGCThingKind {
    ScopeIndex,
}

#[repr(C)]
pub struct SmooshGCThing {
    kind: SmooshGCThingKind,
    scope_index: usize,
}

impl From<GCThing> for SmooshGCThing {
    fn from(item: GCThing) -> Self {
        match item {
            GCThing::Scope(index) => Self {
                kind: SmooshGCThingKind::ScopeIndex,
                scope_index: index.into(),
            },
        }
    }
}

#[repr(C)]
pub enum SmooshScopeDataKind {
    Global,
    Lexical,
}

#[repr(C)]
pub struct SmooshBindingName {
    name: usize,
    is_closed_over: bool,
    is_top_level_function: bool,
}

impl From<BindingName> for SmooshBindingName {
    fn from(info: BindingName) -> Self {
        Self {
            name: info.name.into(),
            is_closed_over: info.is_closed_over,
            is_top_level_function: info.is_top_level_function,
        }
    }
}

#[repr(C)]
pub struct SmooshScopeData {
    kind: SmooshScopeDataKind,
    bindings: CVec<SmooshBindingName>,
    let_start: usize,
    const_start: usize,
    enclosing: usize,
    first_frame_slot: u32,
}

impl From<ScopeData> for SmooshScopeData {
    fn from(data: ScopeData) -> Self {
        match data {
            ScopeData::Global(data) => Self {
                kind: SmooshScopeDataKind::Global,
                bindings: CVec::from(data.bindings.into_iter().map(|x| x.into()).collect()),
                let_start: data.let_start,
                const_start: data.const_start,
                enclosing: 0,
                first_frame_slot: 0,
            },
            ScopeData::Lexical(data) => Self {
                kind: SmooshScopeDataKind::Lexical,
                bindings: CVec::from(data.bindings.into_iter().map(|x| x.into()).collect()),
                let_start: 0,
                const_start: data.const_start,
                enclosing: data.enclosing.into(),
                first_frame_slot: data.first_frame_slot.into(),
            },
        }
    }
}

#[repr(C)]
pub struct SmooshScopeNote {
    index: u32,
    start: u32,
    length: u32,
    parent: u32,
}

impl From<ScopeNote> for SmooshScopeNote {
    fn from(note: ScopeNote) -> Self {
        let start = usize::from(note.start) as u32;
        let end = usize::from(note.end) as u32;
        let parent = match note.parent {
            Some(index) => usize::from(index) as u32,
            None => std::u32::MAX,
        };
        Self {
            index: usize::from(note.scope_index) as u32,
            start,
            length: end - start,
            parent,
        }
    }
}

#[repr(C)]
pub struct SmooshResult {
    unimplemented: bool,
    error: CVec<u8>,
    bytecode: CVec<u8>,
    atoms: CVec<usize>,
    gcthings: CVec<SmooshGCThing>,
    scopes: CVec<SmooshScopeData>,
    scope_notes: CVec<SmooshScopeNote>,

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

    all_atoms: *mut c_void,
    all_atoms_len: usize,
    allocator: *mut c_void,
}

impl SmooshResult {
    fn unimplemented() -> Self {
        Self::unimplemented_or_error(true, CVec::empty())
    }

    fn error(message: String) -> Self {
        let error = CVec::from(format!("{}\0", message).into_bytes());
        Self::unimplemented_or_error(false, error)
    }

    fn unimplemented_or_error(unimplemented: bool, error: CVec<u8>) -> Self {
        Self {
            unimplemented,
            error,
            bytecode: CVec::empty(),
            atoms: CVec::empty(),
            gcthings: CVec::empty(),
            scopes: CVec::empty(),
            scope_notes: CVec::empty(),
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

            all_atoms: std::ptr::null_mut(),
            all_atoms_len: 0,
            allocator: std::ptr::null_mut(),
        }
    }
}

enum SmooshError {
    GenericError(String),
    NotImplemented,
}

#[no_mangle]
pub unsafe extern "C" fn init_smoosh() {
    // Gecko might set a logger before we do, which is all fine; try to
    // initialize ours, and reset the FilterLevel env_logger::try_init might
    // have set to what it was in case of initialization failure
    let filter = log::max_level();
    match env_logger::try_init() {
        Ok(_) => {}
        Err(_) => {
            log::set_max_level(filter);
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn run_smoosh(
    text: *const u8,
    text_len: usize,
    options: &SmooshCompileOptions,
) -> SmooshResult {
    let text = str::from_utf8(slice::from_raw_parts(text, text_len)).expect("Invalid UTF8");
    let allocator = Box::new(bumpalo::Bump::new());
    match smoosh(&allocator, text, options) {
        Ok(result) => {
            let bytecode = CVec::from(result.bytecode);
            let atoms = CVec::from(result.atoms.into_iter().map(|s| s.into()).collect());
            let gcthings = CVec::from(result.gcthings.into_iter().map(|x| x.into()).collect());
            let scopes = CVec::from(result.scopes.into_iter().map(|x| x.into()).collect());
            let scope_notes =
                CVec::from(result.scope_notes.into_iter().map(|x| x.into()).collect());

            let lineno = result.lineno;
            let column = result.column;
            let main_offset = result.main_offset;
            let max_fixed_slots = result.max_fixed_slots.into();
            let maximum_stack_depth = result.maximum_stack_depth;
            let body_scope_index = result.body_scope_index;
            let num_ic_entries = result.num_ic_entries;
            let num_type_sets = result.num_type_sets;
            let strict = result.strict;
            let bindings_accessed_dynamically = result.bindings_accessed_dynamically;
            let has_call_site_obj = result.has_call_site_obj;
            let is_for_eval = result.is_for_eval;
            let is_module = result.is_module;
            let is_function = result.is_function;
            let has_non_syntactic_scope = result.has_non_syntactic_scope;
            let needs_function_environment_objects = result.needs_function_environment_objects;
            let has_module_goal = result.has_module_goal;

            let all_atoms_len = result.all_atoms.len();
            let all_atoms = Box::new(result.all_atoms);
            let raw_all_atoms = Box::into_raw(all_atoms);
            let opaque_all_atoms = raw_all_atoms as *mut c_void;

            let raw_allocator = Box::into_raw(allocator);
            let opaque_allocator = raw_allocator as *mut c_void;

            SmooshResult {
                unimplemented: false,
                error: CVec::empty(),
                bytecode,
                atoms,
                gcthings,
                scopes,
                scope_notes,
                lineno,
                column,
                main_offset,
                max_fixed_slots,
                maximum_stack_depth,
                body_scope_index,
                num_ic_entries,
                num_type_sets,
                strict,
                bindings_accessed_dynamically,
                has_call_site_obj,
                is_for_eval,
                is_module,
                is_function,
                has_non_syntactic_scope,
                needs_function_environment_objects,
                has_module_goal,

                all_atoms: opaque_all_atoms,
                all_atoms_len,
                allocator: opaque_allocator,
            }
        }
        Err(SmooshError::GenericError(message)) => SmooshResult::error(message),
        Err(SmooshError::NotImplemented) => SmooshResult::unimplemented(),
    }
}

#[repr(C)]
pub struct SmooshParseResult {
    unimplemented: bool,
    error: CVec<u8>,
}

fn convert_parse_result<'alloc, T>(r: jsparagus::parser::Result<T>) -> SmooshParseResult {
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
    let atoms = Rc::new(RefCell::new(SourceAtomSet::new()));
    convert_parse_result(parse_script(&allocator, text, &parse_options, atoms))
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
    let atoms = Rc::new(RefCell::new(SourceAtomSet::new()));
    convert_parse_result(parse_module(&allocator, text, &parse_options, atoms))
}

#[no_mangle]
pub unsafe extern "C" fn free_smoosh_parse_result(result: SmooshParseResult) {
    let _ = result.error.into();
}

#[no_mangle]
pub unsafe extern "C" fn smoosh_get_atom_at(result: SmooshResult, index: usize) -> *const c_char {
    let all_atoms = result.all_atoms as *const Vec<&str>;
    let atom = (*all_atoms)[index];
    atom.as_ptr() as *const c_char
}

#[no_mangle]
pub unsafe extern "C" fn smoosh_get_atom_len_at(result: SmooshResult, index: usize) -> usize {
    let all_atoms = result.all_atoms as *const Vec<&str>;
    let atom = (*all_atoms)[index];
    atom.len()
}

#[no_mangle]
pub unsafe extern "C" fn free_smoosh(result: SmooshResult) {
    let _ = result.error.into();
    let _ = result.bytecode.into();
    let _ = result.atoms.into();
    let _ = result.gcthings.into();
    let _ = result.scopes.into();
    let _ = result.scope_notes.into();
    //Vec::from_raw_parts(bytecode.data, bytecode.len, bytecode.capacity);

    if !result.all_atoms.is_null() {
        let _ = Box::from_raw(result.all_atoms as *mut Vec<&str>);
    }
    if !result.allocator.is_null() {
        let _ = Box::from_raw(result.allocator as *mut bumpalo::Bump);
    }
}

fn smoosh<'alloc>(
    allocator: &'alloc bumpalo::Bump,
    text: &'alloc str,
    options: &SmooshCompileOptions,
) -> Result<EmitResult<'alloc>, SmooshError> {
    let parse_options = ParseOptions::new();
    let atoms = Rc::new(RefCell::new(SourceAtomSet::new()));
    let parse_result = match parse_script(&allocator, text, &parse_options, atoms.clone()) {
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
    match emit(
        &mut Program::Script(parse_result.unbox()),
        &emit_options,
        atoms.replace(SourceAtomSet::new_uninitialized()),
    ) {
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

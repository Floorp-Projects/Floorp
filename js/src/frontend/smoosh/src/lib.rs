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
use jsparagus::ast::source_slice_list::SourceSliceList;
use jsparagus::ast::types::Program;
use jsparagus::emitter::{emit, EmitError, EmitOptions};
use jsparagus::parser::{parse_module, parse_script, ParseError, ParseOptions};
use jsparagus::stencil::gcthings::GCThing;
use jsparagus::stencil::regexp::RegExpItem;
use jsparagus::stencil::result::EmitResult;
use jsparagus::stencil::scope::{BindingName, ScopeData};
use jsparagus::stencil::scope_notes::ScopeNote;
use jsparagus::stencil::script::{ImmutableScriptData, ScriptStencil, SourceExtent};
use std::boxed::Box;
use std::cell::RefCell;
use std::collections::HashMap;
use std::convert::TryInto;
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

#[repr(C, u8)]
pub enum SmooshGCThing {
    Null,
    Atom(usize),
    Function(usize),
    Scope(usize),
    RegExp(usize),
}

fn convert_gcthing(item: GCThing, scope_index_map: &HashMap<usize, usize>) -> SmooshGCThing {
    match item {
        GCThing::Null => SmooshGCThing::Null,
        GCThing::Atom(index) => SmooshGCThing::Atom(index.into()),
        GCThing::Function(index) => SmooshGCThing::Function(index.into()),
        GCThing::RegExp(index) => SmooshGCThing::RegExp(index.into()),
        GCThing::Scope(index) => {
            let mapped_index = *scope_index_map
                .get(&index.into())
                .expect("Scope map should be populated");
            SmooshGCThing::Scope(mapped_index)
        }
    }
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
pub struct SmooshGlobalScopeData {
    bindings: CVec<SmooshBindingName>,
    let_start: usize,
    const_start: usize,
}

#[repr(C)]
pub struct SmooshVarScopeData {
    bindings: CVec<SmooshBindingName>,
    enclosing: usize,
    function_has_extensible_scope: bool,
    first_frame_slot: u32,
}

#[repr(C)]
pub struct SmooshLexicalScopeData {
    bindings: CVec<SmooshBindingName>,
    const_start: usize,
    enclosing: usize,
    first_frame_slot: u32,
}

#[repr(C)]
pub struct SmooshFunctionScopeData {
    bindings: CVec<COption<SmooshBindingName>>,
    has_parameter_exprs: bool,
    non_positional_formal_start: usize,
    var_start: usize,
    enclosing: usize,
    first_frame_slot: u32,
    function_index: usize,
    is_arrow: bool,
}

#[repr(C, u8)]
pub enum SmooshScopeData {
    Global(SmooshGlobalScopeData),
    Var(SmooshVarScopeData),
    Lexical(SmooshLexicalScopeData),
    Function(SmooshFunctionScopeData),
}

/// Convert single Scope data, resolving enclosing index with scope_index_map.
fn convert_scope(scope: ScopeData, scope_index_map: &mut HashMap<usize, usize>) -> SmooshScopeData {
    match scope {
        ScopeData::Alias(_) => panic!("alias should be handled in convert_scopes"),
        ScopeData::Global(data) => SmooshScopeData::Global(SmooshGlobalScopeData {
            bindings: CVec::from(data.base.bindings.into_iter().map(|x| x.into()).collect()),
            let_start: data.let_start,
            const_start: data.const_start,
        }),
        ScopeData::Var(data) => {
            let enclosing: usize = data.enclosing.into();
            SmooshScopeData::Var(SmooshVarScopeData {
                bindings: CVec::from(data.base.bindings.into_iter().map(|x| x.into()).collect()),
                enclosing: *scope_index_map
                    .get(&enclosing)
                    .expect("Alias target should be earlier index"),
                function_has_extensible_scope: data.function_has_extensible_scope,
                first_frame_slot: data.first_frame_slot.into(),
            })
        }
        ScopeData::Lexical(data) => {
            let enclosing: usize = data.enclosing.into();
            SmooshScopeData::Lexical(SmooshLexicalScopeData {
                bindings: CVec::from(data.base.bindings.into_iter().map(|x| x.into()).collect()),
                const_start: data.const_start,
                enclosing: *scope_index_map
                    .get(&enclosing)
                    .expect("Alias target should be earlier index"),
                first_frame_slot: data.first_frame_slot.into(),
            })
        }
        ScopeData::Function(data) => {
            let enclosing: usize = data.enclosing.into();
            SmooshScopeData::Function(SmooshFunctionScopeData {
                bindings: CVec::from(
                    data.base
                        .bindings
                        .into_iter()
                        .map(|x| COption::from(x.map(|x| x.into())))
                        .collect(),
                ),
                has_parameter_exprs: data.has_parameter_exprs,
                non_positional_formal_start: data.non_positional_formal_start,
                var_start: data.var_start,
                enclosing: *scope_index_map
                    .get(&enclosing)
                    .expect("Alias target should be earlier index"),
                first_frame_slot: data.first_frame_slot.into(),
                function_index: data.function_index.into(),
                is_arrow: data.is_arrow,
            })
        }
    }
}

/// Convert list of Scope data, removing aliases.
/// Also create a map between original index into index into result vector
/// without aliases.
fn convert_scopes(
    scopes: Vec<ScopeData>,
    scope_index_map: &mut HashMap<usize, usize>,
) -> CVec<SmooshScopeData> {
    let mut result = Vec::with_capacity(scopes.len());
    for (i, scope) in scopes.into_iter().enumerate() {
        if let ScopeData::Alias(index) = scope {
            let mapped_index = *scope_index_map
                .get(&index.into())
                .expect("Alias target should be earlier index");
            scope_index_map.insert(i, mapped_index);

            continue;
        }

        scope_index_map.insert(i, result.len());
        result.push(convert_scope(scope, scope_index_map))
    }

    CVec::from(result)
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
            index: usize::from(note.index) as u32,
            start,
            length: end - start,
            parent,
        }
    }
}

#[repr(C)]
pub struct SmooshRegExpItem {
    pattern: usize,
    global: bool,
    ignore_case: bool,
    multi_line: bool,
    dot_all: bool,
    sticky: bool,
    unicode: bool,
}

impl From<RegExpItem> for SmooshRegExpItem {
    fn from(data: RegExpItem) -> Self {
        Self {
            pattern: data.pattern.into(),
            global: data.global,
            ignore_case: data.ignore_case,
            multi_line: data.multi_line,
            dot_all: data.dot_all,
            sticky: data.sticky,
            unicode: data.unicode,
        }
    }
}

#[repr(C)]
pub struct SmooshImmutableScriptData {
    pub main_offset: u32,
    pub nfixed: u32,
    pub nslots: u32,
    pub body_scope_index: u32,
    pub num_ic_entries: u32,
    pub fun_length: u16,
    pub num_bytecode_type_sets: u32,
    pub bytecode: CVec<u8>,
    pub scope_notes: CVec<SmooshScopeNote>,
}

#[repr(C)]
pub struct SmooshSourceExtent {
    pub source_start: u32,
    pub source_end: u32,
    pub to_string_start: u32,
    pub to_string_end: u32,
    pub lineno: u32,
    pub column: u32,
}

#[repr(C, u8)]
pub enum COption<T> {
    Some(T),
    None,
}

impl<T> COption<T> {
    fn from(v: Option<T>) -> Self {
        match v {
            Option::Some(v) => COption::Some(v),
            Option::None => COption::None,
        }
    }
}

#[repr(C)]
pub struct SmooshScriptStencil {
    pub immutable_flags: u32,
    pub gcthings: CVec<SmooshGCThing>,
    pub immutable_script_data: COption<usize>,
    pub extent: SmooshSourceExtent,
    pub fun_name: COption<usize>,
    pub fun_nargs: u16,
    pub fun_flags: u16,
    pub lazy_function_enclosing_scope_index: COption<usize>,
    pub is_standalone_function: bool,
    pub was_function_emitted: bool,
    pub is_singleton_function: bool,
}

#[repr(C)]
pub struct SmooshResult {
    unimplemented: bool,
    error: CVec<u8>,

    scopes: CVec<SmooshScopeData>,
    regexps: CVec<SmooshRegExpItem>,

    top_level_script: SmooshScriptStencil,
    functions: CVec<SmooshScriptStencil>,
    script_data_list: CVec<SmooshImmutableScriptData>,

    all_atoms: *mut c_void,
    all_atoms_len: usize,
    slices: *mut c_void,
    slices_len: usize,
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

            scopes: CVec::empty(),
            regexps: CVec::empty(),

            top_level_script: SmooshScriptStencil {
                immutable_flags: 0,
                gcthings: CVec::empty(),
                immutable_script_data: COption::None,
                extent: SmooshSourceExtent {
                    source_start: 0,
                    source_end: 0,
                    to_string_start: 0,
                    to_string_end: 0,
                    lineno: 0,
                    column: 0,
                },
                fun_name: COption::None,
                fun_nargs: 0,
                fun_flags: 0,
                lazy_function_enclosing_scope_index: COption::None,
                is_standalone_function: false,
                was_function_emitted: false,
                is_singleton_function: false,
            },
            functions: CVec::empty(),
            script_data_list: CVec::empty(),

            all_atoms: std::ptr::null_mut(),
            all_atoms_len: 0,
            slices: std::ptr::null_mut(),
            slices_len: 0,
            allocator: std::ptr::null_mut(),
        }
    }
}

enum SmooshError {
    GenericError(String),
    NotImplemented,
}

#[no_mangle]
pub unsafe extern "C" fn smoosh_init() {
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

fn convert_extent(extent: SourceExtent) -> SmooshSourceExtent {
    SmooshSourceExtent {
        source_start: extent.source_start,
        source_end: extent.source_end,
        to_string_start: extent.to_string_start,
        to_string_end: extent.to_string_end,
        lineno: extent.lineno,
        column: extent.column,
    }
}

fn convert_script(
    script: ScriptStencil,
    scope_index_map: &HashMap<usize, usize>,
) -> SmooshScriptStencil {
    let immutable_flags = script.immutable_flags.into();

    let gcthings = CVec::from(
        script
            .gcthings
            .into_iter()
            .map(|x| convert_gcthing(x, scope_index_map))
            .collect(),
    );

    let immutable_script_data = COption::from(script.immutable_script_data.map(|n| n.into()));

    let extent = convert_extent(script.extent);

    let fun_name = COption::from(script.fun_name.map(|n| n.into()));
    let fun_nargs = script.fun_nargs;
    let fun_flags = script.fun_flags.into();

    let lazy_function_enclosing_scope_index =
        COption::from(script.lazy_function_enclosing_scope_index.map(|index| {
            *scope_index_map
                .get(&index.into())
                .expect("Alias target should be earlier index")
        }));

    let is_standalone_function = script.is_standalone_function;
    let was_function_emitted = script.was_function_emitted;
    let is_singleton_function = script.is_singleton_function;

    SmooshScriptStencil {
        immutable_flags,
        gcthings,
        immutable_script_data,
        extent,
        fun_name,
        fun_nargs,
        fun_flags,
        lazy_function_enclosing_scope_index,
        is_standalone_function,
        was_function_emitted,
        is_singleton_function,
    }
}

fn convert_script_data(script_data: ImmutableScriptData) -> SmooshImmutableScriptData {
    let main_offset = script_data.main_offset;
    let nfixed = script_data.nfixed.into();
    let nslots = script_data.nslots;
    let body_scope_index = script_data.body_scope_index;
    let num_ic_entries = script_data.num_ic_entries;
    let fun_length = script_data.fun_length;
    let num_bytecode_type_sets = script_data.num_bytecode_type_sets;

    let bytecode = CVec::from(script_data.bytecode);

    let scope_notes = CVec::from(
        script_data
            .scope_notes
            .into_iter()
            .map(|x| x.into())
            .collect(),
    );

    SmooshImmutableScriptData {
        main_offset,
        nfixed,
        nslots,
        body_scope_index,
        num_ic_entries,
        fun_length,
        num_bytecode_type_sets,
        bytecode,
        scope_notes,
    }
}

#[no_mangle]
pub unsafe extern "C" fn smoosh_run(
    text: *const u8,
    text_len: usize,
    options: &SmooshCompileOptions,
) -> SmooshResult {
    let text = str::from_utf8(slice::from_raw_parts(text, text_len)).expect("Invalid UTF8");
    let allocator = Box::new(bumpalo::Bump::new());
    match smoosh(&allocator, text, options) {
        Ok(result) => {
            let mut scope_index_map = HashMap::new();

            let scopes = convert_scopes(result.scopes, &mut scope_index_map);
            let regexps = CVec::from(result.regexps.into_iter().map(|x| x.into()).collect());

            let top_level_script = convert_script(result.top_level_script, &scope_index_map);

            let functions = CVec::from(
                result
                    .functions
                    .into_iter()
                    .map(|x| convert_script(x, &scope_index_map))
                    .collect(),
            );

            let script_data_list = CVec::from(
                result
                    .script_data_list
                    .into_iter()
                    .map(convert_script_data)
                    .collect(),
            );

            let all_atoms_len = result.atoms.len();
            let all_atoms = Box::new(result.atoms);
            let raw_all_atoms = Box::into_raw(all_atoms);
            let opaque_all_atoms = raw_all_atoms as *mut c_void;

            let slices_len = result.slices.len();
            let slices = Box::new(result.slices);
            let raw_slices = Box::into_raw(slices);
            let opaque_slices = raw_slices as *mut c_void;

            let raw_allocator = Box::into_raw(allocator);
            let opaque_allocator = raw_allocator as *mut c_void;

            SmooshResult {
                unimplemented: false,
                error: CVec::empty(),

                scopes,
                regexps,

                top_level_script,
                functions,
                script_data_list,

                all_atoms: opaque_all_atoms,
                all_atoms_len,
                slices: opaque_slices,
                slices_len,
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
        Err(err) => match *err {
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
pub unsafe extern "C" fn smoosh_test_parse_script(
    text: *const u8,
    text_len: usize,
) -> SmooshParseResult {
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
    let slices = Rc::new(RefCell::new(SourceSliceList::new()));
    convert_parse_result(parse_script(
        &allocator,
        text,
        &parse_options,
        atoms,
        slices,
    ))
}

#[no_mangle]
pub unsafe extern "C" fn smoosh_test_parse_module(
    text: *const u8,
    text_len: usize,
) -> SmooshParseResult {
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
    let slices = Rc::new(RefCell::new(SourceSliceList::new()));
    convert_parse_result(parse_module(
        &allocator,
        text,
        &parse_options,
        atoms,
        slices,
    ))
}

#[no_mangle]
pub unsafe extern "C" fn smoosh_free_parse_result(result: SmooshParseResult) {
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
pub unsafe extern "C" fn smoosh_get_slice_at(result: SmooshResult, index: usize) -> *const c_char {
    let slices = result.slices as *const Vec<&str>;
    let slice = (*slices)[index];
    slice.as_ptr() as *const c_char
}

#[no_mangle]
pub unsafe extern "C" fn smoosh_get_slice_len_at(result: SmooshResult, index: usize) -> usize {
    let slices = result.slices as *const Vec<&str>;
    let slice = (*slices)[index];
    slice.len()
}

unsafe fn free_script(script: SmooshScriptStencil) {
    let _ = script.gcthings.into();
}

unsafe fn free_script_data(script_data: SmooshImmutableScriptData) {
    let _ = script_data.bytecode.into();
    let _ = script_data.scope_notes.into();
}

#[no_mangle]
pub unsafe extern "C" fn smoosh_free(result: SmooshResult) {
    let _ = result.error.into();

    let _ = result.scopes.into();
    let _ = result.regexps.into();

    free_script(result.top_level_script);

    for fun in result.functions.into() {
        free_script(fun);
    }

    for script_data in result.script_data_list.into() {
        free_script_data(script_data);
    }

    if !result.all_atoms.is_null() {
        let _ = Box::from_raw(result.all_atoms as *mut Vec<&str>);
    }
    if !result.slices.is_null() {
        let _ = Box::from_raw(result.slices as *mut Vec<&str>);
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
    let slices = Rc::new(RefCell::new(SourceSliceList::new()));
    let text_length = text.len();

    let parse_result = match parse_script(
        &allocator,
        text,
        &parse_options,
        atoms.clone(),
        slices.clone(),
    ) {
        Ok(result) => result,
        Err(err) => match *err {
            ParseError::NotImplemented(_) => {
                println!("Unimplemented: {}", err.message());
                return Err(SmooshError::NotImplemented);
            }
            _ => {
                return Err(SmooshError::GenericError(err.message()));
            }
        },
    };

    let extent = SourceExtent::top_level_script(text_length.try_into().unwrap(), 1, 0);
    let mut emit_options = EmitOptions::new(extent);
    emit_options.no_script_rval = options.no_script_rval;
    let script = parse_result.unbox();
    match emit(
        allocator.alloc(Program::Script(script)),
        &emit_options,
        atoms.replace(SourceAtomSet::new_uninitialized()),
        slices.replace(SourceSliceList::new()),
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

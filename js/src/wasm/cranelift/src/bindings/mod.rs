/* Copyright 2018 Mozilla Foundation
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

// Safe wrappers to the low-level ABI.  This re-exports all types in low_level but none of the
// functions.

use std::{mem, slice};

use cranelift_codegen::binemit::CodeOffset;
use cranelift_codegen::cursor::FuncCursor;
use cranelift_codegen::entity::EntityRef;
use cranelift_codegen::ir::immediates::{Ieee32, Ieee64};
use cranelift_codegen::ir::{self, InstBuilder, SourceLoc};
use cranelift_codegen::isa;

use cranelift_wasm::{
    wasmparser, FuncIndex, GlobalIndex, SignatureIndex, TableIndex, TypeIndex, WasmResult,
};

use crate::compile;
use crate::utils::BasicError;
use crate::wasm2clif::REF_TYPE;

use self::low_level::*;

pub use self::low_level::BD_SymbolicAddress as SymbolicAddress;
pub use self::low_level::CraneliftCompiledFunc as CompiledFunc;
pub use self::low_level::CraneliftFuncCompileInput as FuncCompileInput;
pub use self::low_level::CraneliftMetadataEntry as MetadataEntry;
pub use self::low_level::CraneliftModuleEnvironment as LowLevelModuleEnvironment;
pub use self::low_level::CraneliftStaticEnvironment as StaticEnvironment;
pub use self::low_level::Trap;
pub use self::low_level::TypeIdDescKind;

mod low_level;

/// Converts a `TypeCode` into the equivalent Cranelift type, if it's a known type, or an error
/// otherwise.
#[inline]
fn typecode_to_type(type_code: TypeCode) -> WasmResult<Option<ir::Type>> {
    match type_code {
        TypeCode::I32 => Ok(Some(ir::types::I32)),
        TypeCode::I64 => Ok(Some(ir::types::I64)),
        TypeCode::F32 => Ok(Some(ir::types::F32)),
        TypeCode::F64 => Ok(Some(ir::types::F64)),
        TypeCode::V128 => Ok(Some(ir::types::I8X16)),
        TypeCode::FuncRef => Ok(Some(REF_TYPE)),
        TypeCode::ExternRef => Ok(Some(REF_TYPE)),
        TypeCode::BlockVoid => Ok(None),
        _ => Err(BasicError::new(format!("unknown type code: {:?}", type_code)).into()),
    }
}

/// Convert a non-void `TypeCode` into the equivalent Cranelift type.
#[inline]
pub(crate) fn typecode_to_nonvoid_type(type_code: TypeCode) -> WasmResult<ir::Type> {
    Ok(typecode_to_type(type_code)?.expect("unexpected void type"))
}

/// Convert a u32 into a `BD_SymbolicAddress`.
impl From<u32> for SymbolicAddress {
    fn from(x: u32) -> SymbolicAddress {
        assert!(x < SymbolicAddress::Limit as u32);
        unsafe { mem::transmute(x) }
    }
}

#[derive(Clone, Copy)]
pub struct GlobalDesc(*const low_level::GlobalDesc);

impl GlobalDesc {
    pub fn value_type(self) -> WasmResult<ir::Type> {
        let type_code = unsafe { low_level::global_type(self.0) };
        typecode_to_nonvoid_type(type_code)
    }

    pub fn is_constant(self) -> bool {
        unsafe { low_level::global_isConstant(self.0) }
    }

    pub fn is_mutable(self) -> bool {
        unsafe { low_level::global_isMutable(self.0) }
    }

    pub fn is_indirect(self) -> bool {
        unsafe { low_level::global_isIndirect(self.0) }
    }

    /// Insert an instruction at `pos` that materializes the constant value.
    pub fn emit_constant(self, pos: &mut FuncCursor) -> WasmResult<ir::Value> {
        unsafe {
            let v = low_level::global_constantValue(self.0);
            match v.t {
                TypeCode::I32 => Ok(pos.ins().iconst(ir::types::I32, i64::from(v.u.i32_))),
                TypeCode::I64 => Ok(pos.ins().iconst(ir::types::I64, v.u.i64_)),
                TypeCode::F32 => Ok(pos.ins().f32const(Ieee32::with_bits(v.u.i32_ as u32))),
                TypeCode::F64 => Ok(pos.ins().f64const(Ieee64::with_bits(v.u.i64_ as u64))),
                TypeCode::V128 => {
                    let c = pos
                        .func
                        .dfg
                        .constants
                        .insert(ir::ConstantData::from(&v.u.v128 as &[u8]));
                    Ok(pos.ins().vconst(ir::types::I8X16, c))
                }
                TypeCode::NullableRef | TypeCode::ExternRef | TypeCode::FuncRef => {
                    assert!(v.u.r as usize == 0);
                    Ok(pos.ins().null(REF_TYPE))
                }
                _ => Err(BasicError::new(format!("unexpected type: {}", v.t as u64)).into()),
            }
        }
    }

    /// Get the offset from the `WasmTlsReg` to the memory representing this global variable.
    pub fn tls_offset(self) -> usize {
        unsafe { low_level::global_tlsOffset(self.0) }
    }

    pub fn content_type(self) -> wasmparser::Type {
        typecode_to_parser_type(unsafe { low_level::global_type(self.0) })
    }
}

#[derive(Clone, Copy)]
pub struct TableDesc(*const low_level::TableDesc);

impl TableDesc {
    /// Get the offset from the `WasmTlsReg` to the `wasm::TableTls` representing this table.
    pub fn tls_offset(self) -> usize {
        unsafe { low_level::table_tlsOffset(self.0) }
    }

    pub fn element_type(self) -> wasmparser::Type {
        typecode_to_parser_type(unsafe { low_level::table_elementTypeCode(self.0) })
    }

    pub fn resizable_limits(self) -> wasmparser::ResizableLimits {
        let initial = unsafe { low_level::table_initialLimit(self.0) };
        let maximum = unsafe { low_level::table_initialLimit(self.0) };
        let maximum = if maximum == u32::max_value() {
            None
        } else {
            Some(maximum)
        };
        wasmparser::ResizableLimits { initial, maximum }
    }
}

#[derive(Clone)]
pub struct FuncType {
    ptr: *const low_level::FuncType,
    args: Vec<TypeCode>,
    results: Vec<TypeCode>,
}

impl FuncType {
    /// Creates a new FuncType, caching all the values it requires.
    pub(crate) fn new(ptr: *const low_level::FuncType) -> Self {
        let num_args = unsafe { low_level::funcType_numArgs(ptr) };
        let args = unsafe { slice::from_raw_parts(low_level::funcType_args(ptr), num_args) };
        let args = args
            .iter()
            .map(|val_type| unsafe { low_level::env_unpack(*val_type) })
            .collect();

        let num_results = unsafe { low_level::funcType_numResults(ptr) };
        let results =
            unsafe { slice::from_raw_parts(low_level::funcType_results(ptr), num_results) };
        let results = results
            .iter()
            .map(|val_type| unsafe { low_level::env_unpack(*val_type) })
            .collect();

        Self { ptr, args, results }
    }

    pub(crate) fn args(&self) -> &[TypeCode] {
        &self.args
    }
    pub(crate) fn results(&self) -> &[TypeCode] {
        &self.results
    }
}

#[derive(Clone)]
pub struct TypeIdDesc {
    ptr: *const low_level::TypeIdDesc,
}

impl TypeIdDesc {
    pub(crate) fn new(ptr: *const low_level::TypeIdDesc) -> Self {
        Self { ptr }
    }

    pub(crate) fn id_kind(&self) -> TypeIdDescKind {
        unsafe { low_level::funcType_idKind(self.ptr) }
    }
    pub(crate) fn id_immediate(&self) -> usize {
        unsafe { low_level::funcType_idImmediate(self.ptr) }
    }
    pub(crate) fn id_tls_offset(&self) -> usize {
        unsafe { low_level::funcType_idTlsOffset(self.ptr) }
    }
}

fn typecode_to_parser_type(ty: TypeCode) -> wasmparser::Type {
    match ty {
        TypeCode::I32 => wasmparser::Type::I32,
        TypeCode::I64 => wasmparser::Type::I64,
        TypeCode::F32 => wasmparser::Type::F32,
        TypeCode::F64 => wasmparser::Type::F64,
        TypeCode::V128 => wasmparser::Type::V128,
        TypeCode::FuncRef => wasmparser::Type::FuncRef,
        TypeCode::ExternRef => wasmparser::Type::ExternRef,
        TypeCode::BlockVoid => wasmparser::Type::EmptyBlockType,
        _ => panic!("unknown type code: {:?}", ty),
    }
}

impl wasmparser::WasmFuncType for FuncType {
    fn len_inputs(&self) -> usize {
        self.args.len()
    }
    fn len_outputs(&self) -> usize {
        self.results.len()
    }
    fn input_at(&self, at: u32) -> Option<wasmparser::Type> {
        self.args
            .get(at as usize)
            .map(|ty| typecode_to_parser_type(*ty))
    }
    fn output_at(&self, at: u32) -> Option<wasmparser::Type> {
        self.results
            .get(at as usize)
            .map(|ty| typecode_to_parser_type(*ty))
    }
}

/// Thin wrapper for the CraneliftModuleEnvironment structure.

pub struct ModuleEnvironment<'a> {
    env: &'a CraneliftModuleEnvironment,
    /// The `WasmModuleResources` trait requires us to return a borrow to a `FuncType`, so we
    /// eagerly construct these.
    types: Vec<FuncType>,
    /// Similar to `types`, we need to have a persistently-stored `FuncType` to return. The
    /// types in `func_sigs` are a subset of those in `types`, but we don't want to have to
    /// maintain an index from function to signature ID, so we store these directly.
    func_sigs: Vec<FuncType>,
}

impl<'a> ModuleEnvironment<'a> {
    pub(crate) fn new(env: &'a CraneliftModuleEnvironment) -> Self {
        let num_types = unsafe { low_level::env_num_types(env) };
        let mut types = Vec::with_capacity(num_types);
        for i in 0..num_types {
            let t = FuncType::new(unsafe { low_level::env_signature(env, i) });
            types.push(t);
        }
        let num_func_sigs = unsafe { low_level::env_num_funcs(env) };
        let mut func_sigs = Vec::with_capacity(num_func_sigs);
        for i in 0..num_func_sigs {
            let t = FuncType::new(unsafe { low_level::env_func_sig(env, i) });
            func_sigs.push(t);
        }
        Self {
            env,
            types,
            func_sigs,
        }
    }
    pub fn has_memory(&self) -> bool {
        unsafe { low_level::env_has_memory(self.env) }
    }
    pub fn uses_shared_memory(&self) -> bool {
        unsafe { low_level::env_uses_shared_memory(self.env) }
    }
    pub fn num_tables(&self) -> usize {
        unsafe { low_level::env_num_tables(self.env) }
    }
    pub fn num_types(&self) -> usize {
        self.types.len()
    }
    pub fn type_(&self, index: usize) -> FuncType {
        self.types[index].clone()
    }
    pub fn num_func_sigs(&self) -> usize {
        self.func_sigs.len()
    }
    pub fn func_sig(&self, func_index: FuncIndex) -> FuncType {
        self.func_sigs[func_index.index()].clone()
    }
    pub fn func_sig_index(&self, func_index: FuncIndex) -> SignatureIndex {
        SignatureIndex::new(unsafe { low_level::env_func_sig_index(self.env, func_index.index()) })
    }
    pub fn func_import_tls_offset(&self, func_index: FuncIndex) -> usize {
        unsafe { low_level::env_func_import_tls_offset(self.env, func_index.index()) }
    }
    pub fn func_is_import(&self, func_index: FuncIndex) -> bool {
        unsafe { low_level::env_func_is_import(self.env, func_index.index()) }
    }
    pub fn signature(&self, type_index: TypeIndex) -> FuncType {
        // This function takes `TypeIndex` rather than the `SignatureIndex` that one
        // might expect.  Why?  https://github.com/bytecodealliance/wasmtime/pull/2115
        // introduces two new types to the type section as viewed by Cranelift.  This is
        // in support of the module linking proposal.  So now a type index (for
        // Cranelift) can refer to a func, module, or instance type.  When the type index
        // refers to a func type, it can also be used to get the signature index which
        // can be used to get the ir::Signature for that func type.  For us, Cranelift is
        // only used with function types so we can just assume type index and signature
        // index are 1:1.  If and when we come to support the module linking proposal,
        // this will need to be revisited.
        FuncType::new(unsafe { low_level::env_signature(self.env, type_index.index()) })
    }
    pub fn signature_id(&self, type_index: TypeIndex) -> TypeIdDesc {
        TypeIdDesc::new(unsafe { low_level::env_signature_id(self.env, type_index.index()) })
    }
    pub fn table(&self, table_index: TableIndex) -> TableDesc {
        TableDesc(unsafe { low_level::env_table(self.env, table_index.index()) })
    }
    pub fn global(&self, global_index: GlobalIndex) -> GlobalDesc {
        GlobalDesc(unsafe { low_level::env_global(self.env, global_index.index()) })
    }
    pub fn min_memory_length(&self) -> u32 {
        self.env.min_memory_length
    }
    pub fn max_memory_length(&self) -> Option<u32> {
        let max = unsafe { low_level::env_max_memory(self.env) };
        if max == u32::max_value() {
            None
        } else {
            Some(max)
        }
    }
}

impl<'module> wasmparser::WasmModuleResources for ModuleEnvironment<'module> {
    type FuncType = FuncType;
    fn table_at(&self, at: u32) -> Option<wasmparser::TableType> {
        if (at as usize) < self.num_tables() {
            let desc = TableDesc(unsafe { low_level::env_table(self.env, at as usize) });
            let element_type = desc.element_type();
            let limits = desc.resizable_limits();
            Some(wasmparser::TableType {
                element_type,
                limits,
            })
        } else {
            None
        }
    }
    fn memory_at(&self, at: u32) -> Option<wasmparser::MemoryType> {
        if at == 0 {
            let has_memory = self.has_memory();
            if has_memory {
                let shared = self.uses_shared_memory();
                let initial = self.min_memory_length() as u32;
                let maximum = self.max_memory_length();
                Some(wasmparser::MemoryType::M32 {
                    limits: wasmparser::ResizableLimits { initial, maximum },
                    shared,
                })
            } else {
                None
            }
        } else {
            None
        }
    }
    fn global_at(&self, at: u32) -> Option<wasmparser::GlobalType> {
        let num_globals = unsafe { low_level::env_num_globals(self.env) };
        if (at as usize) < num_globals {
            let desc = self.global(GlobalIndex::new(at as usize));
            let mutable = desc.is_mutable();
            let content_type = desc.content_type();
            Some(wasmparser::GlobalType {
                mutable,
                content_type,
            })
        } else {
            None
        }
    }
    fn func_type_at(&self, type_idx: u32) -> Option<&Self::FuncType> {
        if (type_idx as usize) < self.types.len() {
            Some(&self.types[type_idx as usize])
        } else {
            None
        }
    }
    fn type_of_function(&self, func_idx: u32) -> Option<&Self::FuncType> {
        if (func_idx as usize) < self.func_sigs.len() {
            Some(&self.func_sigs[func_idx as usize])
        } else {
            None
        }
    }
    fn element_type_at(&self, at: u32) -> Option<wasmparser::Type> {
        let num_elems = self.element_count();
        if at < num_elems {
            let elem_type = unsafe { low_level::env_elem_typecode(self.env, at) };
            Some(typecode_to_parser_type(elem_type))
        } else {
            None
        }
    }
    fn element_count(&self) -> u32 {
        unsafe { low_level::env_num_elems(self.env) as u32 }
    }
    fn data_count(&self) -> u32 {
        unsafe { low_level::env_num_datas(self.env) as u32 }
    }
    fn is_function_referenced(&self, idx: u32) -> bool {
        unsafe { low_level::env_is_func_valid_for_ref(self.env, idx) }
    }
}

/// Extra methods for some C++ wrappers.

impl FuncCompileInput {
    pub fn bytecode(&self) -> &[u8] {
        unsafe { slice::from_raw_parts(self.bytecode, self.bytecode_size) }
    }

    pub fn stackmaps(&self) -> Stackmaps {
        Stackmaps(self.stackmaps)
    }
}

impl CompiledFunc {
    pub fn reset(&mut self, compiled_func: &compile::CompiledFunc) {
        self.num_metadata = compiled_func.metadata.len();
        self.metadatas = compiled_func.metadata.as_ptr();

        self.frame_pushed = compiled_func.frame_pushed as usize;
        self.contains_calls = compiled_func.contains_calls;

        self.code = compiled_func.code_buffer.as_ptr();
        self.code_size = compiled_func.code_size as usize;
        self.jumptables_size = compiled_func.jumptables_size as usize;
        self.rodata_size = compiled_func.rodata_size as usize;
        self.total_size = compiled_func.code_buffer.len();

        self.num_rodata_relocs = compiled_func.rodata_relocs.len();
        self.rodata_relocs = compiled_func.rodata_relocs.as_ptr();
    }
}

impl MetadataEntry {
    pub fn direct_call(code_offset: CodeOffset, srcloc: SourceLoc, func_index: FuncIndex) -> Self {
        Self {
            which: CraneliftMetadataEntry_Which_DirectCall,
            code_offset,
            module_bytecode_offset: srcloc.bits(),
            extra: func_index.index(),
        }
    }
    pub fn indirect_call(ret_addr: CodeOffset, srcloc: SourceLoc) -> Self {
        Self {
            which: CraneliftMetadataEntry_Which_IndirectCall,
            code_offset: ret_addr,
            module_bytecode_offset: srcloc.bits(),
            extra: 0,
        }
    }
    pub fn trap(code_offset: CodeOffset, srcloc: SourceLoc, which: Trap) -> Self {
        Self {
            which: CraneliftMetadataEntry_Which_Trap,
            code_offset,
            module_bytecode_offset: srcloc.bits(),
            extra: which as usize,
        }
    }
    pub fn symbolic_access(
        code_offset: CodeOffset,
        srcloc: SourceLoc,
        sym: SymbolicAddress,
    ) -> Self {
        Self {
            which: CraneliftMetadataEntry_Which_SymbolicAccess,
            code_offset,
            module_bytecode_offset: srcloc.bits(),
            extra: sym as usize,
        }
    }
}

impl StaticEnvironment {
    /// Returns the default calling convention on this machine.
    pub fn call_conv(&self) -> isa::CallConv {
        if self.platform_is_windows {
            unimplemented!("No FastCall variant of Baldrdash2020")
        } else {
            isa::CallConv::Baldrdash2020
        }
    }
}

pub struct Stackmaps(*mut self::low_level::BD_Stackmaps);

impl Stackmaps {
    pub fn add_stackmap(
        &mut self,
        inbound_args_size: u32,
        offset: CodeOffset,
        map: &cranelift_codegen::binemit::StackMap,
    ) {
        unsafe {
            let bitslice = map.as_slice();
            low_level::stackmaps_add(
                self.0,
                std::mem::transmute(bitslice.as_ptr()),
                map.mapped_words() as usize,
                inbound_args_size as usize,
                offset as usize,
            );
        }
    }
}

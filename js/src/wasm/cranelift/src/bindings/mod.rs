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
use cranelift_wasm::{FuncIndex, GlobalIndex, SignatureIndex, TableIndex, WasmError, WasmResult};

use smallvec::SmallVec;

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
pub use self::low_level::FuncTypeIdDescKind;
pub use self::low_level::Trap;

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
        TypeCode::NullRef => Ok(Some(REF_TYPE)),
        TypeCode::FuncRef => Ok(Some(REF_TYPE)),
        TypeCode::AnyRef => Ok(Some(REF_TYPE)),
        TypeCode::BlockVoid => Ok(None),
        _ => Err(BasicError::new(format!("unknown type code: {:?}", type_code)).into()),
    }
}

/// Convert a non-void `TypeCode` into the equivalent Cranelift type.
#[inline]
fn typecode_to_nonvoid_type(type_code: TypeCode) -> WasmResult<ir::Type> {
    Ok(typecode_to_type(type_code)?.expect("unexpected void type"))
}

/// Convert a `TypeCode` into the equivalent Cranelift type.
#[inline]
fn valtype_to_type(val_type: BD_ValType) -> WasmResult<ir::Type> {
    let type_code = unsafe { low_level::env_unpack(val_type) };
    typecode_to_nonvoid_type(type_code)
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

    pub fn is_indirect(self) -> bool {
        unsafe { low_level::global_isIndirect(self.0) }
    }

    /// Insert an instruction at `pos` that materialized the constant value.
    pub fn emit_constant(self, pos: &mut FuncCursor) -> WasmResult<ir::Value> {
        unsafe {
            let v = low_level::global_constantValue(self.0);
            match v.t {
                TypeCode::I32 => Ok(pos.ins().iconst(ir::types::I32, i64::from(v.u.i32))),
                TypeCode::I64 => Ok(pos.ins().iconst(ir::types::I64, v.u.i64)),
                TypeCode::F32 => Ok(pos.ins().f32const(Ieee32::with_bits(v.u.i32 as u32))),
                TypeCode::F64 => Ok(pos.ins().f64const(Ieee64::with_bits(v.u.i64 as u64))),
                TypeCode::OptRef | TypeCode::AnyRef | TypeCode::FuncRef | TypeCode::NullRef => {
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
}

#[derive(Clone, Copy)]
pub struct TableDesc(*const low_level::TableDesc);

impl TableDesc {
    /// Get the offset from the `WasmTlsReg` to the `wasm::TableTls` representing this table.
    pub fn tls_offset(self) -> usize {
        unsafe { low_level::table_tlsOffset(self.0) }
    }
}

#[derive(Clone, Copy)]
pub struct FuncTypeWithId(*const low_level::FuncTypeWithId);

impl FuncTypeWithId {
    pub fn args<'a>(self) -> WasmResult<SmallVec<[ir::Type; 4]>> {
        let num_args = unsafe { low_level::funcType_numArgs(self.0) };
        // The `funcType_args` callback crashes when there are no arguments. Also note that
        // `slice::from_raw_parts()` requires a non-null pointer for empty slices.
        // TODO: We should get all the parts of a signature in a single callback that returns a
        // struct.
        if num_args == 0 {
            Ok(SmallVec::new())
        } else {
            let args = unsafe { slice::from_raw_parts(low_level::funcType_args(self.0), num_args) };
            let mut ret = SmallVec::new();
            for &arg in args {
                ret.push(valtype_to_type(arg)?);
            }
            Ok(ret)
        }
    }

    pub fn results<'a>(self) -> WasmResult<Vec<ir::Type>> {
        let num_results = unsafe { low_level::funcType_numResults(self.0) };
        // The same comments as FuncTypeWithId::args apply here.
        if num_results == 0 {
            Ok(Vec::new())
        } else {
            let results =
                unsafe { slice::from_raw_parts(low_level::funcType_results(self.0), num_results) };
            let mut ret = Vec::new();
            for &result in results {
                ret.push(valtype_to_type(result)?);
            }
            Ok(ret)
        }
    }

    pub fn ret_type(self) -> WasmResult<Option<ir::Type>> {
        match self.results() {
            Ok(v) => match v.as_slice() {
                [] => Ok(None),
                [t] => Ok(Some(*t)),
                _ => Err(WasmError::Unsupported("multiple values".to_string())),
            },
            Err(e) => Err(e),
        }
    }

    pub fn id_kind(self) -> FuncTypeIdDescKind {
        unsafe { low_level::funcType_idKind(self.0) }
    }

    pub fn id_immediate(self) -> usize {
        unsafe { low_level::funcType_idImmediate(self.0) }
    }

    pub fn id_tls_offset(self) -> usize {
        unsafe { low_level::funcType_idTlsOffset(self.0) }
    }
}

/// Thin wrapper for the CraneliftModuleEnvironment structure.

#[derive(Clone, Copy)]
pub struct ModuleEnvironment<'a> {
    env: &'a CraneliftModuleEnvironment,
}

impl<'a> ModuleEnvironment<'a> {
    pub fn new(env: &'a CraneliftModuleEnvironment) -> Self {
        Self { env }
    }
    pub fn uses_shared_memory(&self) -> bool {
        unsafe { low_level::env_uses_shared_memory(self.env) }
    }
    pub fn function_signature(&self, func_index: FuncIndex) -> FuncTypeWithId {
        FuncTypeWithId(unsafe { low_level::env_function_signature(self.env, func_index.index()) })
    }
    pub fn func_import_tls_offset(&self, func_index: FuncIndex) -> usize {
        unsafe { low_level::env_func_import_tls_offset(self.env, func_index.index()) }
    }
    pub fn func_is_import(&self, func_index: FuncIndex) -> bool {
        unsafe { low_level::env_func_is_import(self.env, func_index.index()) }
    }
    pub fn signature(&self, sig_index: SignatureIndex) -> FuncTypeWithId {
        FuncTypeWithId(unsafe { low_level::env_signature(self.env, sig_index.index()) })
    }
    pub fn table(&self, table_index: TableIndex) -> TableDesc {
        TableDesc(unsafe { low_level::env_table(self.env, table_index.index()) })
    }
    pub fn global(&self, global_index: GlobalIndex) -> GlobalDesc {
        GlobalDesc(unsafe { low_level::env_global(self.env, global_index.index()) })
    }
    pub fn min_memory_length(&self) -> i64 {
        i64::from(self.env.min_memory_length)
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
            isa::CallConv::BaldrdashWindows
        } else {
            isa::CallConv::BaldrdashSystemV
        }
    }
}

pub struct Stackmaps(*mut self::low_level::BD_Stackmaps);

impl Stackmaps {
    pub fn add_stackmap(
        &mut self,
        stack_slots: &ir::StackSlots,
        offset: CodeOffset,
        map: cranelift_codegen::binemit::Stackmap,
    ) {
        unsafe {
            let bitslice = map.as_slice();
            low_level::stackmaps_add(
                self.0,
                std::mem::transmute(bitslice.as_ptr()),
                map.mapped_words() as usize,
                stack_slots.layout_info.unwrap().inbound_args_size as usize,
                offset as usize,
            );
        }
    }
}

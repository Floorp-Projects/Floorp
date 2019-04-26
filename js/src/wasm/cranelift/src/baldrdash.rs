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

// Safe wrappers to the low-level ABI.  This re-exports all types in
// baldrapi but none of the functions.

use cranelift_codegen::cursor::FuncCursor;
use cranelift_codegen::entity::EntityRef;
use cranelift_codegen::ir::immediates::{Ieee32, Ieee64};
use cranelift_codegen::ir::{self, InstBuilder};
use cranelift_wasm::{FuncIndex, GlobalIndex, SignatureIndex, TableIndex, WasmResult};

use std::mem;
use std::slice;

use baldrapi;
use baldrapi::BD_ValType as ValType;
use baldrapi::CraneliftModuleEnvironment;
use baldrapi::TypeCode;

use utils::BasicError;

pub use baldrapi::BD_SymbolicAddress as SymbolicAddress;
pub use baldrapi::CraneliftCompiledFunc as CompiledFunc;
pub use baldrapi::CraneliftFuncCompileInput as FuncCompileInput;
pub use baldrapi::CraneliftMetadataEntry as MetadataEntry;
pub use baldrapi::CraneliftStaticEnvironment as StaticEnvironment;
pub use baldrapi::FuncTypeIdDescKind;
pub use baldrapi::Trap;

/// Converts a `TypeCode` into the equivalent Cranelift type, if it's a known type, or an error
/// otherwise.
#[inline]
fn typecode_to_type(type_code: TypeCode) -> WasmResult<Option<ir::Type>> {
    match type_code {
        TypeCode::I32 => Ok(Some(ir::types::I32)),
        TypeCode::I64 => Ok(Some(ir::types::I64)),
        TypeCode::F32 => Ok(Some(ir::types::F32)),
        TypeCode::F64 => Ok(Some(ir::types::F64)),
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
fn valtype_to_type(val_type: ValType) -> WasmResult<ir::Type> {
    let type_code = unsafe { baldrapi::env_unpack(val_type) };
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
pub struct GlobalDesc(*const baldrapi::GlobalDesc);

impl GlobalDesc {
    pub fn value_type(self) -> WasmResult<ir::Type> {
        let type_code = unsafe { baldrapi::global_type(self.0) };
        typecode_to_nonvoid_type(type_code)
    }

    pub fn is_constant(self) -> bool {
        unsafe { baldrapi::global_isConstant(self.0) }
    }

    pub fn is_indirect(self) -> bool {
        unsafe { baldrapi::global_isIndirect(self.0) }
    }

    /// Insert an instruction at `pos` that materialized the constant value.
    pub fn emit_constant(self, pos: &mut FuncCursor) -> WasmResult<ir::Value> {
        unsafe {
            let v = baldrapi::global_constantValue(self.0);
            match v.t {
                TypeCode::I32 => Ok(pos.ins().iconst(ir::types::I32, i64::from(v.u.i32))),
                TypeCode::I64 => Ok(pos.ins().iconst(ir::types::I64, v.u.i64)),
                TypeCode::F32 => Ok(pos.ins().f32const(Ieee32::with_bits(v.u.i32 as u32))),
                TypeCode::F64 => Ok(pos.ins().f64const(Ieee64::with_bits(v.u.i64 as u64))),
                _ => Err(BasicError::new(format!("unexpected type: {}", v.t as u64)).into()),
            }
        }
    }

    /// Get the offset from the `WasmTlsReg` to the memory representing this global variable.
    pub fn tls_offset(self) -> usize {
        unsafe { baldrapi::global_tlsOffset(self.0) }
    }
}

#[derive(Clone, Copy)]
pub struct TableDesc(*const baldrapi::TableDesc);

impl TableDesc {
    /// Get the offset from the `WasmTlsReg` to the `wasm::TableTls` representing this table.
    pub fn tls_offset(self) -> usize {
        unsafe { baldrapi::table_tlsOffset(self.0) }
    }
}

#[derive(Clone, Copy)]
pub struct FuncTypeWithId(*const baldrapi::FuncTypeWithId);

impl FuncTypeWithId {
    pub fn args<'a>(self) -> WasmResult<Vec<ir::Type>> {
        let num_args = unsafe { baldrapi::funcType_numArgs(self.0) };
        // The `funcType_args` callback crashes when there are no arguments. Also note that
        // `slice::from_raw_parts()` requires a non-null pointer for empty slices.
        // TODO: We should get all the parts of a signature in a single callback that returns a
        // struct.
        if num_args == 0 {
            Ok(Vec::new())
        } else {
            let args = unsafe { slice::from_raw_parts(baldrapi::funcType_args(self.0), num_args) };
            let mut ret = Vec::new();
            for &arg in args {
                ret.push(valtype_to_type(arg)?);
            }
            Ok(ret)
        }
    }

    pub fn ret_type(self) -> WasmResult<Option<ir::Type>> {
        let type_code = unsafe { baldrapi::funcType_retType(self.0) };
        typecode_to_type(type_code)
    }

    pub fn id_kind(self) -> FuncTypeIdDescKind {
        unsafe { baldrapi::funcType_idKind(self.0) }
    }

    pub fn id_immediate(self) -> usize {
        unsafe { baldrapi::funcType_idImmediate(self.0) }
    }

    pub fn id_tls_offset(self) -> usize {
        unsafe { baldrapi::funcType_idTlsOffset(self.0) }
    }
}

/// Thin wrapper for the CraneliftModuleEnvironment structure.

pub struct ModuleEnvironment<'a> {
    env: &'a CraneliftModuleEnvironment,
}

impl<'a> ModuleEnvironment<'a> {
    pub fn new(env: &'a CraneliftModuleEnvironment) -> Self {
        Self { env }
    }
    pub fn function_signature(&self, func_index: FuncIndex) -> FuncTypeWithId {
        FuncTypeWithId(unsafe { baldrapi::env_function_signature(self.env, func_index.index()) })
    }
    pub fn func_import_tls_offset(&self, func_index: FuncIndex) -> usize {
        unsafe { baldrapi::env_func_import_tls_offset(self.env, func_index.index()) }
    }
    pub fn func_is_import(&self, func_index: FuncIndex) -> bool {
        unsafe { baldrapi::env_func_is_import(self.env, func_index.index()) }
    }
    pub fn signature(&self, sig_index: SignatureIndex) -> FuncTypeWithId {
        FuncTypeWithId(unsafe { baldrapi::env_signature(self.env, sig_index.index()) })
    }
    pub fn table(&self, table_index: TableIndex) -> TableDesc {
        TableDesc(unsafe { baldrapi::env_table(self.env, table_index.index()) })
    }
    pub fn global(&self, global_index: GlobalIndex) -> GlobalDesc {
        GlobalDesc(unsafe { baldrapi::env_global(self.env, global_index.index()) })
    }
    pub fn min_memory_length(&self) -> i64 {
        i64::from(self.env.min_memory_length)
    }
}

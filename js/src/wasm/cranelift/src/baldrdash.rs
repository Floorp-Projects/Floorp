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

// TODO: Should many u32 arguments and return values here really be
// usize, to be more conventional?

use baldrapi::CraneliftModuleEnvironment;
use cranelift_codegen::binemit::CodeOffset;
use cranelift_codegen::cursor::{Cursor, FuncCursor};
use cranelift_codegen::entity::EntityRef;
use cranelift_codegen::ir::immediates::{Ieee32, Ieee64};
use cranelift_codegen::ir::stackslot::StackSize;
use cranelift_codegen::ir::{self, InstBuilder};
use cranelift_codegen::{CodegenError, CodegenResult};
use cranelift_wasm::{FuncIndex, GlobalIndex, SignatureIndex, TableIndex};
use std::mem;
use std::slice;

use baldrapi;

pub use baldrapi::BD_SymbolicAddress as SymbolicAddress;
pub use baldrapi::BD_ValType as ValType;
pub use baldrapi::CraneliftCompiledFunc as CompiledFunc;
pub use baldrapi::CraneliftFuncCompileInput as FuncCompileInput;
pub use baldrapi::CraneliftMetadataEntry as MetadataEntry;
pub use baldrapi::CraneliftStaticEnvironment as StaticEnvironment;
pub use baldrapi::FuncTypeIdDescKind;
pub use baldrapi::Trap;
pub use baldrapi::TypeCode;

pub enum ConstantValue {
    I32(i32),
    I64(i64),
    F32(f32),
    F64(f64),
}

/// Convert a `TypeCode` into the equivalent Cranelift type.
///
/// We expect Cranelift's `VOID` type to go away in the future, so use `None` to represent a
/// function without a return value.
impl Into<Option<ir::Type>> for TypeCode {
    fn into(self) -> Option<ir::Type> {
        match self {
            TypeCode::I32 => Some(ir::types::I32),
            TypeCode::I64 => Some(ir::types::I64),
            TypeCode::F32 => Some(ir::types::F32),
            TypeCode::F64 => Some(ir::types::F64),
            TypeCode::BlockVoid => None,
            _ => panic!("unexpected type"),
        }
    }
}

/// Convert a non-void `TypeCode` into the equivalent Cranelift type.
impl Into<ir::Type> for TypeCode {
    fn into(self) -> ir::Type {
        match self.into() {
            Some(t) => t,
            None => panic!("unexpected void type"),
        }
    }
}

/// Convert a `TypeCode` into the equivalent Cranelift type.
impl Into<ir::Type> for ValType {
    fn into(self) -> ir::Type {
        unsafe { baldrapi::env_unpack(self) }.into()
    }
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
    pub fn value_type(self) -> TypeCode {
        unsafe { baldrapi::global_type(self.0) }
    }

    pub fn is_constant(self) -> bool {
        unsafe { baldrapi::global_isConstant(self.0) }
    }

    pub fn is_indirect(self) -> bool {
        unsafe { baldrapi::global_isIndirect(self.0) }
    }

    /// Insert an instruction at `pos` that materialized the constant value.
    pub fn emit_constant(self, pos: &mut FuncCursor) -> ir::Value {
        unsafe {
            let v = baldrapi::global_constantValue(self.0);
            // Note that the floating point constants below
            match v.t {
                TypeCode::I32 => pos.ins().iconst(ir::types::I32, v.u.i32 as i64),
                TypeCode::I64 => pos.ins().iconst(ir::types::I64, v.u.i64),
                TypeCode::F32 => pos.ins().f32const(Ieee32::with_bits(v.u.i32 as u32)),
                TypeCode::F64 => pos.ins().f64const(Ieee64::with_bits(v.u.i64 as u64)),
                _ => panic!("unexpected type"),
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

    /// Is this an external table?
    pub fn is_external(self) -> bool {
        unsafe { baldrapi::table_isExternal(self.0) }
    }
}

#[derive(Clone, Copy)]
pub struct FuncTypeWithId(*const baldrapi::FuncTypeWithId);

impl FuncTypeWithId {
    pub fn args<'a>(self) -> &'a [ValType] {
        unsafe {
            let num_args = baldrapi::funcType_numArgs(self.0);
            // The `funcType_args` callback crashes when there are no arguments. Also note that
            // `slice::from_raw_parts()` requires a non-null pointer for empty slices.
            // TODO: We should get all the parts of a signature in a single callback that returns a
            // struct.
            if num_args == 0 {
                &[]
            } else {
                slice::from_raw_parts(baldrapi::funcType_args(self.0), num_args)
            }
        }
    }

    pub fn ret_type(self) -> TypeCode {
        unsafe { baldrapi::funcType_retType(self.0) }
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
        FuncTypeWithId(unsafe { baldrapi::env_signature(self.env, sig_index) })
    }
    pub fn table(&self, table_index: TableIndex) -> TableDesc {
        TableDesc(unsafe { baldrapi::env_table(self.env, table_index) })
    }
    pub fn global(&self, global_index: GlobalIndex) -> GlobalDesc {
        GlobalDesc(unsafe { baldrapi::env_global(self.env, global_index) })
    }
    pub fn min_memory_length(&self) -> i64 {
        self.env.min_memory_length as i64
    }
}

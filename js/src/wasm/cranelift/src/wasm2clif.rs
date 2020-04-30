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

//! This module deals with the translation of WebAssembly binary functions to Cranelift IR.
//!
//! The code here deals with adapting the `cranelift_wasm` module to the specifics of BaldrMonkey's
//! internal data structures.

use std::collections::HashMap;

use cranelift_codegen::cursor::{Cursor, FuncCursor};
use cranelift_codegen::entity::{EntityRef, PrimaryMap, SecondaryMap};
use cranelift_codegen::ir;
use cranelift_codegen::ir::condcodes::IntCC;
use cranelift_codegen::ir::immediates::Offset32;
use cranelift_codegen::ir::InstBuilder;
use cranelift_codegen::isa::{CallConv, TargetFrontendConfig, TargetIsa};
use cranelift_codegen::packed_option::PackedOption;
use cranelift_wasm::{
    FuncEnvironment, FuncIndex, GlobalIndex, GlobalVariable, MemoryIndex, ReturnMode,
    SignatureIndex, TableIndex, TargetEnvironment, WasmError, WasmResult,
};

use crate::bindings::{self, GlobalDesc, SymbolicAddress};
use crate::compile::{symbolic_function_name, wasm_function_name};
use crate::isa::{platform::USES_HEAP_REG, POINTER_SIZE};

#[cfg(target_pointer_width = "64")]
pub const POINTER_TYPE: ir::Type = ir::types::I64;
#[cfg(target_pointer_width = "32")]
pub const POINTER_TYPE: ir::Type = ir::types::I32;

#[cfg(target_pointer_width = "64")]
pub const REF_TYPE: ir::Type = ir::types::R64;
#[cfg(target_pointer_width = "32")]
pub const REF_TYPE: ir::Type = ir::types::R32;

/// Convert a TlsData offset into a `Offset32` for a global decl.
fn offset32(offset: usize) -> ir::immediates::Offset32 {
    assert!(offset <= i32::max_value() as usize);
    (offset as i32).into()
}

/// Convert a usize offset into a `Imm64` for an iadd_imm.
fn imm64(offset: usize) -> ir::immediates::Imm64 {
    (offset as i64).into()
}

/// Initialize a `Signature` from a wasm signature.
fn init_sig_from_wsig(
    call_conv: CallConv,
    wsig: bindings::FuncTypeWithId,
) -> WasmResult<ir::Signature> {
    let mut sig = ir::Signature::new(call_conv);

    for arg in wsig.args()? {
        sig.params.push(ir::AbiParam::new(arg));
    }

    if let Some(ret_type) = wsig.ret_type()? {
        let ret = match ret_type {
            // Spidermonkey requires i32 returns to have their high 32 bits
            // zero so that it can directly box them.
            ir::types::I32 => ir::AbiParam::new(ret_type).uext(),
            _ => ir::AbiParam::new(ret_type),
        };
        sig.returns.push(ret);
    }

    // Add a VM context pointer argument.
    // This corresponds to SpiderMonkey's `WasmTlsReg` hidden argument.
    sig.params.push(ir::AbiParam::special(
        POINTER_TYPE,
        ir::ArgumentPurpose::VMContext,
    ));

    Ok(sig)
}

/// Initialize the signature `sig` to match the function with `index` in `env`.
pub fn init_sig(
    env: &bindings::ModuleEnvironment,
    call_conv: CallConv,
    func_index: FuncIndex,
) -> WasmResult<ir::Signature> {
    let wsig = env.function_signature(func_index);
    init_sig_from_wsig(call_conv, wsig)
}

/// An instance call may return a special value to indicate that the operation
/// failed and we need to trap. This indicates what kind of value to check for,
/// if any.
enum FailureMode {
    Infallible,
    /// The value returned by the function must be checked. internal_ret set to true indicates that
    /// the returned value is only used internally, and should not be passed back to wasm.
    NotZero {
        internal_ret: bool,
    },
    InvalidRef,
}

/// A description of builtin call to the `wasm::Instance`.
struct InstanceCall {
    address: SymbolicAddress,
    arguments: &'static [ir::Type],
    ret: Option<ir::Type>,
    failure_mode: FailureMode,
}

// The following are a list of the instance calls used to implement operations.

const FN_MEMORY_GROW: InstanceCall = InstanceCall {
    address: SymbolicAddress::MemoryGrow,
    arguments: &[ir::types::I32],
    ret: Some(ir::types::I32),
    failure_mode: FailureMode::Infallible,
};
const FN_MEMORY_SIZE: InstanceCall = InstanceCall {
    address: SymbolicAddress::MemorySize,
    arguments: &[],
    ret: Some(ir::types::I32),
    failure_mode: FailureMode::Infallible,
};
const FN_MEMORY_COPY: InstanceCall = InstanceCall {
    address: SymbolicAddress::MemoryCopy,
    arguments: &[ir::types::I32, ir::types::I32, ir::types::I32, POINTER_TYPE],
    ret: Some(ir::types::I32),
    failure_mode: FailureMode::NotZero { internal_ret: true },
};
const FN_MEMORY_COPY_SHARED: InstanceCall = InstanceCall {
    address: SymbolicAddress::MemoryCopyShared,
    arguments: &[ir::types::I32, ir::types::I32, ir::types::I32, POINTER_TYPE],
    ret: Some(ir::types::I32),
    failure_mode: FailureMode::NotZero { internal_ret: true },
};
const FN_MEMORY_FILL: InstanceCall = InstanceCall {
    address: SymbolicAddress::MemoryFill,
    arguments: &[ir::types::I32, ir::types::I32, ir::types::I32, POINTER_TYPE],
    ret: Some(ir::types::I32),
    failure_mode: FailureMode::NotZero { internal_ret: true },
};
const FN_MEMORY_FILL_SHARED: InstanceCall = InstanceCall {
    address: SymbolicAddress::MemoryFillShared,
    arguments: &[ir::types::I32, ir::types::I32, ir::types::I32, POINTER_TYPE],
    ret: Some(ir::types::I32),
    failure_mode: FailureMode::NotZero { internal_ret: true },
};
const FN_MEMORY_INIT: InstanceCall = InstanceCall {
    address: SymbolicAddress::MemoryInit,
    arguments: &[
        ir::types::I32,
        ir::types::I32,
        ir::types::I32,
        ir::types::I32,
    ],
    ret: Some(ir::types::I32),
    failure_mode: FailureMode::NotZero { internal_ret: true },
};
const FN_DATA_DROP: InstanceCall = InstanceCall {
    address: SymbolicAddress::DataDrop,
    arguments: &[ir::types::I32],
    ret: Some(ir::types::I32),
    failure_mode: FailureMode::NotZero { internal_ret: true },
};
const FN_TABLE_SIZE: InstanceCall = InstanceCall {
    address: SymbolicAddress::TableSize,
    arguments: &[ir::types::I32],
    ret: Some(ir::types::I32),
    failure_mode: FailureMode::Infallible,
};
const FN_TABLE_GROW: InstanceCall = InstanceCall {
    address: SymbolicAddress::TableGrow,
    arguments: &[REF_TYPE, ir::types::I32, ir::types::I32],
    ret: Some(ir::types::I32),
    failure_mode: FailureMode::Infallible,
};
const FN_TABLE_GET: InstanceCall = InstanceCall {
    address: SymbolicAddress::TableGet,
    arguments: &[ir::types::I32, ir::types::I32],
    ret: Some(REF_TYPE),
    failure_mode: FailureMode::InvalidRef,
};
const FN_TABLE_SET: InstanceCall = InstanceCall {
    address: SymbolicAddress::TableSet,
    arguments: &[ir::types::I32, REF_TYPE, ir::types::I32],
    ret: Some(ir::types::I32),
    failure_mode: FailureMode::NotZero { internal_ret: true },
};
const FN_TABLE_COPY: InstanceCall = InstanceCall {
    address: SymbolicAddress::TableCopy,
    arguments: &[
        ir::types::I32,
        ir::types::I32,
        ir::types::I32,
        ir::types::I32,
        ir::types::I32,
    ],
    ret: Some(ir::types::I32),
    failure_mode: FailureMode::NotZero { internal_ret: true },
};
const FN_TABLE_FILL: InstanceCall = InstanceCall {
    address: SymbolicAddress::TableFill,
    arguments: &[ir::types::I32, REF_TYPE, ir::types::I32, ir::types::I32],
    ret: Some(ir::types::I32),
    failure_mode: FailureMode::NotZero { internal_ret: true },
};
const FN_TABLE_INIT: InstanceCall = InstanceCall {
    address: SymbolicAddress::TableInit,
    arguments: &[
        ir::types::I32,
        ir::types::I32,
        ir::types::I32,
        ir::types::I32,
        ir::types::I32,
    ],
    ret: Some(ir::types::I32),
    failure_mode: FailureMode::NotZero { internal_ret: true },
};
const FN_ELEM_DROP: InstanceCall = InstanceCall {
    address: SymbolicAddress::ElemDrop,
    arguments: &[ir::types::I32],
    ret: Some(ir::types::I32),
    failure_mode: FailureMode::NotZero { internal_ret: true },
};
const FN_REF_FUNC: InstanceCall = InstanceCall {
    address: SymbolicAddress::RefFunc,
    arguments: &[ir::types::I32],
    ret: Some(REF_TYPE),
    failure_mode: FailureMode::InvalidRef,
};
const FN_PRE_BARRIER: InstanceCall = InstanceCall {
    address: SymbolicAddress::PreBarrier,
    arguments: &[POINTER_TYPE],
    ret: None,
    failure_mode: FailureMode::Infallible,
};
const FN_POST_BARRIER: InstanceCall = InstanceCall {
    address: SymbolicAddress::PostBarrier,
    arguments: &[POINTER_TYPE],
    ret: None,
    failure_mode: FailureMode::Infallible,
};

// Custom trap codes specific to this embedding

pub const TRAP_THROW_REPORTED: u16 = 1;

/// A translation context that implements `FuncEnvironment` for the specific Spidermonkey
/// translation bits.
pub struct TransEnv<'static_env, 'module_env> {
    env: bindings::ModuleEnvironment<'module_env>,
    static_env: &'static_env bindings::StaticEnvironment,

    target_frontend_config: TargetFrontendConfig,

    /// Information about the function pointer tables `self.env` knowns about. Indexed by table
    /// index.
    tables: PrimaryMap<TableIndex, TableInfo>,

    /// For those signatures whose ID is stored in a global, keep track of the globals we have
    /// created so far.
    ///
    /// Note that most signatures are of the immediate form, and we don't keep any records for
    /// those.
    ///
    /// The key to this table is the TLS offset returned by `sig_idTlsOffset()`.
    signatures: HashMap<i32, ir::GlobalValue>,

    /// Global variables containing `FuncImportTls` information about imported functions.
    /// This vector is indexed by a `FuncIndex`, taking advantage of the fact that WebAssembly
    /// imported functions are numbered starting from 0.
    ///
    /// Any `None` entries in this table are simply global variables that have not yet been created.
    func_gvs: SecondaryMap<FuncIndex, PackedOption<ir::GlobalValue>>,

    /// The `vmctx` global value.
    vmctx_gv: PackedOption<ir::GlobalValue>,

    /// Global variable representing the `TlsData::instance` field which points to the current
    /// instance.
    instance_gv: PackedOption<ir::GlobalValue>,

    /// Global variable representing the `TlsData::interrupt` field which points to the current
    /// interrupt flag.
    interrupt_gv: PackedOption<ir::GlobalValue>,

    /// Allocated `FuncRef` for symbolic addresses.
    /// See the `SymbolicAddress` enum in `baldrapi.h`.
    symbolic: [PackedOption<ir::FuncRef>; bindings::SymbolicAddress::Limit as usize],

    /// The address of the `cx` field in the `wasm::TlsData` struct.
    cx_addr: PackedOption<ir::GlobalValue>,

    /// The address of the `realm` field in the `wasm::TlsData` struct.
    realm_addr: PackedOption<ir::GlobalValue>,
}

impl<'static_env, 'module_env> TransEnv<'static_env, 'module_env> {
    pub fn new(
        isa: &dyn TargetIsa,
        env: bindings::ModuleEnvironment<'module_env>,
        static_env: &'static_env bindings::StaticEnvironment,
    ) -> Self {
        TransEnv {
            env,
            static_env,
            target_frontend_config: isa.frontend_config(),
            tables: PrimaryMap::new(),
            signatures: HashMap::new(),
            func_gvs: SecondaryMap::new(),
            vmctx_gv: None.into(),
            instance_gv: None.into(),
            interrupt_gv: None.into(),
            symbolic: [None.into(); bindings::SymbolicAddress::Limit as usize],
            cx_addr: None.into(),
            realm_addr: None.into(),
        }
    }

    pub fn clear(&mut self) {
        self.tables.clear();
        self.signatures.clear();
        self.func_gvs.clear();
        self.vmctx_gv = None.into();
        self.instance_gv = None.into();
        self.interrupt_gv = None.into();
        for entry in self.symbolic.iter_mut() {
            *entry = None.into();
        }
        self.cx_addr = None.into();
        self.realm_addr = None.into();
    }

    /// Get the `vmctx` global value.
    fn get_vmctx_gv(&mut self, func: &mut ir::Function) -> ir::GlobalValue {
        match self.vmctx_gv.expand() {
            Some(gv) => gv,
            None => {
                // We need to allocate the global variable.
                let gv = func.create_global_value(ir::GlobalValueData::VMContext);
                self.vmctx_gv = Some(gv).into();
                gv
            }
        }
    }

    /// Get information about `table`.
    /// Create it if necessary.
    fn get_table(&mut self, func: &mut ir::Function, table: TableIndex) -> TableInfo {
        // Allocate all tables up to the requested index.
        let vmctx = self.get_vmctx_gv(func);
        while self.tables.len() <= table.index() {
            let wtab = self.env.table(TableIndex::new(self.tables.len()));
            self.tables.push(TableInfo::new(wtab, func, vmctx));
        }
        self.tables[table].clone()
    }

    /// Get the global variable storing the ID of the given signature.
    fn sig_global(&mut self, func: &mut ir::Function, offset: usize) -> ir::GlobalValue {
        let vmctx = self.get_vmctx_gv(func);
        *self.signatures.entry(offset as i32).or_insert_with(|| {
            func.create_global_value(ir::GlobalValueData::IAddImm {
                base: vmctx,
                offset: imm64(offset),
                global_type: POINTER_TYPE,
            })
        })
    }

    /// Get the global variable storing the `FuncImportTls` struct for an imported function.
    fn func_import_global(&mut self, func: &mut ir::Function, index: FuncIndex) -> ir::GlobalValue {
        // See if we already allocated a global for this import.
        if let Some(gv) = self.func_gvs.get(index).and_then(|gv| gv.expand()) {
            return gv;
        }
        // We need to create a global variable for `import_index`.
        let vmctx = self.get_vmctx_gv(func);
        let gv = func.create_global_value(ir::GlobalValueData::IAddImm {
            base: vmctx,
            offset: imm64(self.env.func_import_tls_offset(index)),
            global_type: POINTER_TYPE,
        });
        // Save it for next time.
        self.func_gvs[index] = gv.into();
        gv
    }

    /// Generate code that loads the current instance pointer.
    fn load_instance(&mut self, pos: &mut FuncCursor) -> ir::Value {
        let gv = match self.instance_gv.expand() {
            Some(gv) => gv,
            None => {
                // We need to allocate the global variable.
                let vmctx = self.get_vmctx_gv(pos.func);
                let gv = pos.func.create_global_value(ir::GlobalValueData::IAddImm {
                    base: vmctx,
                    offset: imm64(self.static_env.instance_tls_offset),
                    global_type: POINTER_TYPE,
                });
                self.instance_gv = gv.into();
                gv
            }
        };
        let ga = pos.ins().global_value(POINTER_TYPE, gv);
        pos.ins().load(POINTER_TYPE, ir::MemFlags::trusted(), ga, 0)
    }

    /// Generate code that loads the current instance pointer.
    fn load_interrupt_flag(&mut self, pos: &mut FuncCursor) -> ir::Value {
        let gv = match self.interrupt_gv.expand() {
            Some(gv) => gv,
            None => {
                // We need to allocate the global variable.
                let vmctx = self.get_vmctx_gv(pos.func);
                let gv = pos.func.create_global_value(ir::GlobalValueData::IAddImm {
                    base: vmctx,
                    offset: imm64(self.static_env.interrupt_tls_offset),
                    global_type: POINTER_TYPE,
                });
                self.interrupt_gv = gv.into();
                gv
            }
        };
        let ga = pos.ins().global_value(POINTER_TYPE, gv);
        pos.ins()
            .load(ir::types::I32, ir::MemFlags::trusted(), ga, 0)
    }

    /// Get a `FuncRef` for the given symbolic address.
    /// Uses the closure to create the signature if necessary.
    fn symbolic_funcref<MKSIG: FnOnce() -> ir::Signature>(
        &mut self,
        func: &mut ir::Function,
        sym: bindings::SymbolicAddress,
        make_sig: MKSIG,
    ) -> (ir::FuncRef, ir::SigRef) {
        let symidx = sym as usize;
        if let Some(fnref) = self.symbolic[symidx].expand() {
            return (fnref, func.dfg.ext_funcs[fnref].signature);
        }

        // We need to allocate a signature and func-ref.
        let signature = func.import_signature(make_sig());
        let fnref = func.import_function(ir::ExtFuncData {
            signature,
            name: symbolic_function_name(sym),
            colocated: false,
        });

        self.symbolic[symidx] = fnref.into();
        (fnref, signature)
    }

    /// Update the JSContext's realm value. This is called after a call to restore the
    /// realm value, in case the call has used a different realm.
    fn switch_to_wasm_tls_realm(&mut self, pos: &mut FuncCursor) {
        if self.cx_addr.is_none() {
            let vmctx = self.get_vmctx_gv(&mut pos.func);
            self.cx_addr = pos
                .func
                .create_global_value(ir::GlobalValueData::IAddImm {
                    base: vmctx,
                    offset: imm64(self.static_env.cx_tls_offset),
                    global_type: POINTER_TYPE,
                })
                .into();
        }

        if self.realm_addr.is_none() {
            let vmctx = self.get_vmctx_gv(&mut pos.func);
            self.realm_addr = pos
                .func
                .create_global_value(ir::GlobalValueData::IAddImm {
                    base: vmctx,
                    offset: imm64(self.static_env.realm_tls_offset),
                    global_type: POINTER_TYPE,
                })
                .into();
        }

        let ptr = POINTER_TYPE;
        let flags = ir::MemFlags::trusted();
        let cx_addr_val = pos.ins().global_value(ptr, self.cx_addr.unwrap());
        let cx = pos.ins().load(ptr, flags, cx_addr_val, 0);
        let realm_addr_val = pos.ins().global_value(ptr, self.realm_addr.unwrap());
        let realm = pos.ins().load(ptr, flags, realm_addr_val, 0);
        pos.ins()
            .store(flags, realm, cx, offset32(self.static_env.realm_cx_offset));
    }

    /// Update the JSContext's realm value in preparation for making an indirect call through
    /// an external table.
    fn switch_to_indirect_callee_realm(&mut self, pos: &mut FuncCursor, vmctx: ir::Value) {
        let ptr = POINTER_TYPE;
        let flags = ir::MemFlags::trusted();
        let cx = pos
            .ins()
            .load(ptr, flags, vmctx, offset32(self.static_env.cx_tls_offset));
        let realm = pos.ins().load(
            ptr,
            flags,
            vmctx,
            offset32(self.static_env.realm_tls_offset),
        );
        pos.ins()
            .store(flags, realm, cx, offset32(self.static_env.realm_cx_offset));
    }

    /// Update the JSContext's realm value in preparation for making a call to an imported
    /// function.
    fn switch_to_import_realm(
        &mut self,
        pos: &mut FuncCursor,
        vmctx: ir::Value,
        gv_addr: ir::Value,
    ) {
        let ptr = POINTER_TYPE;
        let flags = ir::MemFlags::trusted();
        let cx = pos
            .ins()
            .load(ptr, flags, vmctx, offset32(self.static_env.cx_tls_offset));
        let realm = pos.ins().load(
            ptr,
            flags,
            gv_addr,
            offset32(self.static_env.realm_func_import_tls_offset),
        );
        pos.ins()
            .store(flags, realm, cx, offset32(self.static_env.realm_cx_offset));
    }

    fn load_pinned_reg(&self, pos: &mut FuncCursor, vmctx: ir::Value) {
        if USES_HEAP_REG {
            let heap_base = pos.ins().load(
                POINTER_TYPE,
                ir::MemFlags::trusted(),
                vmctx,
                self.static_env.memory_base_tls_offset as i32,
            );
            pos.ins().set_pinned_reg(heap_base);
        }
    }

    fn reload_tls_and_pinned_regs(&mut self, pos: &mut FuncCursor) {
        let vmctx_gv = self.get_vmctx_gv(&mut pos.func);
        let vmctx = pos.ins().global_value(POINTER_TYPE, vmctx_gv);
        self.load_pinned_reg(pos, vmctx);
    }

    fn instance_call(
        &mut self,
        pos: &mut FuncCursor,
        call: &InstanceCall,
        arguments: &[ir::Value],
    ) -> Option<ir::Value> {
        debug_assert!(call.arguments.len() == arguments.len());

        let call_conv = self.static_env.call_conv();
        let (fnref, sigref) = self.symbolic_funcref(pos.func, call.address, || {
            let mut sig = ir::Signature::new(call_conv);
            sig.params.push(ir::AbiParam::new(POINTER_TYPE));
            for argument in call.arguments {
                sig.params.push(ir::AbiParam::new(*argument));
            }
            sig.params.push(ir::AbiParam::special(
                POINTER_TYPE,
                ir::ArgumentPurpose::VMContext,
            ));
            if let Some(ret) = &call.ret {
                sig.returns.push(ir::AbiParam::new(*ret));
            }
            sig
        });

        let instance = self.load_instance(pos);
        let vmctx = pos
            .func
            .special_param(ir::ArgumentPurpose::VMContext)
            .expect("Missing vmctx arg");

        // We must use `func_addr` for symbolic references since the stubs can be far away, and the
        // C++ `SymbolicAccess` linker expects it.

        let func_addr = pos.ins().func_addr(POINTER_TYPE, fnref);
        let call_ins = pos.ins().call_indirect(sigref, func_addr, &[]);
        let mut built_arguments = pos.func.dfg[call_ins].take_value_list().unwrap();
        built_arguments.push(instance, &mut pos.func.dfg.value_lists);
        built_arguments.extend(arguments.iter().cloned(), &mut pos.func.dfg.value_lists);
        built_arguments.push(vmctx, &mut pos.func.dfg.value_lists);
        pos.func.dfg[call_ins].put_value_list(built_arguments);

        self.switch_to_wasm_tls_realm(pos);
        self.reload_tls_and_pinned_regs(pos);

        if call.ret.is_none() {
            return None;
        }

        let ret = pos.func.dfg.first_result(call_ins);
        match call.failure_mode {
            FailureMode::Infallible => Some(ret),
            FailureMode::NotZero { internal_ret } => {
                pos.ins()
                    .trapnz(ret, ir::TrapCode::User(TRAP_THROW_REPORTED));
                if internal_ret {
                    None
                } else {
                    Some(ret)
                }
            }
            FailureMode::InvalidRef => {
                let invalid = pos.ins().is_invalid(ret);
                pos.ins()
                    .trapnz(invalid, ir::TrapCode::User(TRAP_THROW_REPORTED));
                Some(ret)
            }
        }
    }

    fn global_address(
        &mut self,
        func: &mut ir::Function,
        global: &GlobalDesc,
    ) -> (ir::GlobalValue, Offset32) {
        assert!(!global.is_constant());

        // This is a global variable. Here we don't care if it is mutable or not.
        let vmctx_gv = self.get_vmctx_gv(func);
        let offset = global.tls_offset();

        // Some globals are represented as a pointer to the actual data, in which case we
        // must do an extra dereference to get to them.  Also, in that case, the pointer
        // itself is immutable, so we mark it `readonly` here to assist Cranelift in commoning
        // up what would otherwise be multiple adjacent reads of the value.
        if global.is_indirect() {
            let gv = func.create_global_value(ir::GlobalValueData::Load {
                base: vmctx_gv,
                offset: offset32(offset),
                global_type: POINTER_TYPE,
                readonly: true,
            });
            (gv, 0.into())
        } else {
            (vmctx_gv, offset32(offset))
        }
    }
}

impl<'static_env, 'module_env> TargetEnvironment for TransEnv<'static_env, 'module_env> {
    fn target_config(&self) -> TargetFrontendConfig {
        self.target_frontend_config
    }
    fn pointer_type(&self) -> ir::Type {
        POINTER_TYPE
    }
}

impl<'static_env, 'module_env> FuncEnvironment for TransEnv<'static_env, 'module_env> {
    fn make_global(
        &mut self,
        func: &mut ir::Function,
        index: GlobalIndex,
    ) -> WasmResult<GlobalVariable> {
        let global = self.env.global(index);
        if global.is_constant() {
            // Constant globals have a known value at compile time. We insert an instruction to
            // materialize the constant at the front of the entry block.
            let mut pos = FuncCursor::new(func);
            pos.next_block().expect("empty function");
            pos.next_inst();
            return Ok(GlobalVariable::Const(global.emit_constant(&mut pos)?));
        }

        match global.value_type()? {
            ir::types::R32 | ir::types::R64 => {
                return Ok(GlobalVariable::Custom);
            }
            _ => {
                let (base_gv, offset) = self.global_address(func, &global);
                let mem_ty = global.value_type()?;

                Ok(GlobalVariable::Memory {
                    gv: base_gv,
                    ty: mem_ty,
                    offset,
                })
            }
        }
    }

    fn make_heap(&mut self, func: &mut ir::Function, index: MemoryIndex) -> WasmResult<ir::Heap> {
        // Currently, Baldrdash doesn't support multiple memories.
        if index.index() != 0 {
            return Err(WasmError::Unsupported(
                "only one wasm memory supported".to_string(),
            ));
        }

        let vcmtx = self.get_vmctx_gv(func);

        let bound = self.static_env.static_memory_bound as u64;
        let is_static = bound > 0;

        // Get the `TlsData::memoryBase` field.
        let base = func.create_global_value(ir::GlobalValueData::Load {
            base: vcmtx,
            offset: offset32(0),
            global_type: POINTER_TYPE,
            readonly: is_static,
        });

        let style = if is_static {
            // We have a static heap.
            let bound = bound.into();
            ir::HeapStyle::Static { bound }
        } else {
            // Get the `TlsData::boundsCheckLimit` field.
            let bound_gv = func.create_global_value(ir::GlobalValueData::Load {
                base: vcmtx,
                offset: (POINTER_SIZE as i32).into(),
                global_type: ir::types::I32,
                readonly: false,
            });
            ir::HeapStyle::Dynamic { bound_gv }
        };

        let min_size = (self.env.min_memory_length() as u64).into();
        let offset_guard_size = (self.static_env.memory_guard_size as u64).into();

        Ok(func.create_heap(ir::HeapData {
            base,
            min_size,
            offset_guard_size,
            style,
            index_type: ir::types::I32,
        }))
    }

    fn make_indirect_sig(
        &mut self,
        func: &mut ir::Function,
        index: SignatureIndex,
    ) -> WasmResult<ir::SigRef> {
        let wsig = self.env.signature(index);
        let mut sigdata = init_sig_from_wsig(self.static_env.call_conv(), wsig)?;

        if wsig.id_kind() != bindings::FuncTypeIdDescKind::None {
            // A signature to be used for an indirect call also takes a signature id.
            sigdata.params.push(ir::AbiParam::special(
                POINTER_TYPE,
                ir::ArgumentPurpose::SignatureId,
            ));
        }

        Ok(func.import_signature(sigdata))
    }

    fn make_table(&mut self, func: &mut ir::Function, index: TableIndex) -> WasmResult<ir::Table> {
        let table_desc = self.get_table(func, index);

        // TODO we'd need a better way to synchronize the shape of GlobalDataDesc and these
        // offsets.
        let bound_gv = func.create_global_value(ir::GlobalValueData::Load {
            base: table_desc.global,
            offset: 0.into(),
            global_type: ir::types::I32,
            readonly: false,
        });

        let base_gv = func.create_global_value(ir::GlobalValueData::Load {
            base: table_desc.global,
            offset: offset32(POINTER_SIZE as usize),
            global_type: POINTER_TYPE,
            readonly: false,
        });

        Ok(func.create_table(ir::TableData {
            base_gv,
            min_size: 0.into(),
            bound_gv,
            element_size: (u64::from(self.pointer_bytes()) * 2).into(),
            index_type: ir::types::I32,
        }))
    }

    fn make_direct_func(
        &mut self,
        func: &mut ir::Function,
        index: FuncIndex,
    ) -> WasmResult<ir::FuncRef> {
        // Create a signature.
        let sigdata = init_sig(&self.env, self.static_env.call_conv(), index)?;
        let signature = func.import_signature(sigdata);

        Ok(func.import_function(ir::ExtFuncData {
            name: wasm_function_name(index),
            signature,
            colocated: true,
        }))
    }

    fn translate_call_indirect(
        &mut self,
        mut pos: FuncCursor,
        table_index: TableIndex,
        table: ir::Table,
        sig_index: SignatureIndex,
        sig_ref: ir::SigRef,
        callee: ir::Value,
        call_args: &[ir::Value],
    ) -> WasmResult<ir::Inst> {
        let wsig = self.env.signature(sig_index);

        let wtable = self.get_table(pos.func, table_index);

        // Follows `MacroAssembler::wasmCallIndirect`:

        // 1. Materialize the signature ID.
        let sigid_value = match wsig.id_kind() {
            bindings::FuncTypeIdDescKind::None => None,
            bindings::FuncTypeIdDescKind::Immediate => {
                // The signature is represented as an immediate pointer-sized value.
                let imm = wsig.id_immediate() as i64;
                Some(pos.ins().iconst(POINTER_TYPE, imm))
            }
            bindings::FuncTypeIdDescKind::Global => {
                let gv = self.sig_global(pos.func, wsig.id_tls_offset());
                let addr = pos.ins().global_value(POINTER_TYPE, gv);
                Some(
                    pos.ins()
                        .load(POINTER_TYPE, ir::MemFlags::trusted(), addr, 0),
                )
            }
        };

        // 2. Bounds check the callee against the table length.
        let (bound_gv, base_gv) = {
            let table_data = &pos.func.tables[table];
            (table_data.bound_gv, table_data.base_gv)
        };

        let tlength = pos.ins().global_value(ir::types::I32, bound_gv);

        let oob = pos
            .ins()
            .icmp(IntCC::UnsignedGreaterThanOrEqual, callee, tlength);
        pos.ins().trapnz(oob, ir::TrapCode::OutOfBounds);

        // 3. Load the wtable base pointer from a global.
        let tbase = pos.ins().global_value(POINTER_TYPE, base_gv);

        // 4. Load callee pointer from wtable.
        let callee_x = if POINTER_TYPE != ir::types::I32 {
            pos.ins().uextend(POINTER_TYPE, callee)
        } else {
            callee
        };
        let callee_scaled = pos.ins().imul_imm(callee_x, wtable.entry_size());

        let entry = pos.ins().iadd(tbase, callee_scaled);
        let callee_func = pos
            .ins()
            .load(POINTER_TYPE, ir::MemFlags::trusted(), entry, 0);

        // Check for a null callee.
        pos.ins()
            .trapz(callee_func, ir::TrapCode::IndirectCallToNull);

        // Handle external tables, set up environment.
        // A function table call could redirect execution to another module with a different realm,
        // so switch to this realm just in case.
        let callee_vmctx = pos.ins().load(
            POINTER_TYPE,
            ir::MemFlags::trusted(),
            entry,
            POINTER_SIZE as i32,
        );
        self.switch_to_indirect_callee_realm(&mut pos, callee_vmctx);
        self.load_pinned_reg(&mut pos, callee_vmctx);

        // First the wasm args.
        let mut args = ir::ValueList::default();
        args.push(callee_func, &mut pos.func.dfg.value_lists);
        args.extend(call_args.iter().cloned(), &mut pos.func.dfg.value_lists);
        args.push(callee_vmctx, &mut pos.func.dfg.value_lists);
        if let Some(sigid) = sigid_value {
            args.push(sigid, &mut pos.func.dfg.value_lists);
        }

        let call = pos
            .ins()
            .CallIndirect(ir::Opcode::CallIndirect, ir::types::INVALID, sig_ref, args)
            .0;

        self.switch_to_wasm_tls_realm(&mut pos);
        self.reload_tls_and_pinned_regs(&mut pos);

        Ok(call)
    }

    fn translate_call(
        &mut self,
        mut pos: FuncCursor,
        callee_index: FuncIndex,
        callee: ir::FuncRef,
        call_args: &[ir::Value],
    ) -> WasmResult<ir::Inst> {
        // First the wasm args.
        let mut args = ir::ValueList::default();
        args.extend(call_args.iter().cloned(), &mut pos.func.dfg.value_lists);

        // Is this an imported function in a different instance, or a local function?
        if self.env.func_is_import(callee_index) {
            // This is a call to an imported function. We need to load the callee address and vmctx
            // from the associated `FuncImportTls` struct in a global.
            let gv = self.func_import_global(pos.func, callee_index);
            let gv_addr = pos.ins().global_value(POINTER_TYPE, gv);

            // We need the first two pointer-sized fields from the `FuncImportTls` struct: `code`
            // and `tls`.
            let fit_code = pos
                .ins()
                .load(POINTER_TYPE, ir::MemFlags::trusted(), gv_addr, 0);
            let fit_tls = pos.ins().load(
                POINTER_TYPE,
                ir::MemFlags::trusted(),
                gv_addr,
                POINTER_SIZE as i32,
            );

            // Switch to the callee's realm.
            self.switch_to_import_realm(&mut pos, fit_tls, gv_addr);
            self.load_pinned_reg(&mut pos, fit_tls);

            // The `tls` field is the VM context pointer for the callee.
            args.push(fit_tls, &mut pos.func.dfg.value_lists);

            // Now make an indirect call to `fit_code`.
            // TODO: We don't need the `FuncRef` that was allocated for this callee since we're
            // using an indirect call. We would need to change the `FuncTranslator` interface to
            // deal.
            args.insert(0, fit_code, &mut pos.func.dfg.value_lists);
            let sig = pos.func.dfg.ext_funcs[callee].signature;
            let call = pos
                .ins()
                .CallIndirect(ir::Opcode::CallIndirect, ir::types::INVALID, sig, args)
                .0;
            self.switch_to_wasm_tls_realm(&mut pos);
            self.reload_tls_and_pinned_regs(&mut pos);
            Ok(call)
        } else {
            // This is a call to a local function.

            // Then we need to pass on the VM context pointer.
            let vmctx = pos
                .func
                .special_param(ir::ArgumentPurpose::VMContext)
                .expect("Missing vmctx arg");
            args.push(vmctx, &mut pos.func.dfg.value_lists);

            Ok(pos
                .ins()
                .Call(ir::Opcode::Call, ir::types::INVALID, callee, args)
                .0)
        }
    }

    fn translate_memory_grow(
        &mut self,
        mut pos: FuncCursor,
        _index: MemoryIndex,
        _heap: ir::Heap,
        val: ir::Value,
    ) -> WasmResult<ir::Value> {
        Ok(self
            .instance_call(&mut pos, &FN_MEMORY_GROW, &[val])
            .unwrap())
    }

    fn translate_memory_size(
        &mut self,
        mut pos: FuncCursor,
        _index: MemoryIndex,
        _heap: ir::Heap,
    ) -> WasmResult<ir::Value> {
        Ok(self.instance_call(&mut pos, &FN_MEMORY_SIZE, &[]).unwrap())
    }

    fn translate_memory_copy(
        &mut self,
        mut pos: FuncCursor,
        _index: MemoryIndex,
        heap: ir::Heap,
        dst: ir::Value,
        src: ir::Value,
        len: ir::Value,
    ) -> WasmResult<()> {
        let heap_gv = pos.func.heaps[heap].base;
        let mem_base = pos.ins().global_value(POINTER_TYPE, heap_gv);

        // We have a specialized version of `memory.copy` when we are using
        // shared memory or not.
        let ret = if self.env.uses_shared_memory() {
            self.instance_call(&mut pos, &FN_MEMORY_COPY_SHARED, &[dst, src, len, mem_base])
        } else {
            self.instance_call(&mut pos, &FN_MEMORY_COPY, &[dst, src, len, mem_base])
        };
        debug_assert!(ret.is_none());
        Ok(())
    }

    fn translate_memory_fill(
        &mut self,
        mut pos: FuncCursor,
        _index: MemoryIndex,
        heap: ir::Heap,
        dst: ir::Value,
        val: ir::Value,
        len: ir::Value,
    ) -> WasmResult<()> {
        let mem_base_gv = pos.func.heaps[heap].base;
        let mem_base = pos.ins().global_value(POINTER_TYPE, mem_base_gv);

        // We have a specialized version of `memory.fill` when we are using
        // shared memory or not.
        let ret = if self.env.uses_shared_memory() {
            self.instance_call(&mut pos, &FN_MEMORY_FILL_SHARED, &[dst, val, len, mem_base])
        } else {
            self.instance_call(&mut pos, &FN_MEMORY_FILL, &[dst, val, len, mem_base])
        };
        debug_assert!(ret.is_none());
        Ok(())
    }

    fn translate_memory_init(
        &mut self,
        mut pos: FuncCursor,
        _index: MemoryIndex,
        _heap: ir::Heap,
        seg_index: u32,
        dst: ir::Value,
        src: ir::Value,
        len: ir::Value,
    ) -> WasmResult<()> {
        let seg_index = pos.ins().iconst(ir::types::I32, seg_index as i64);
        let ret = self.instance_call(&mut pos, &FN_MEMORY_INIT, &[dst, src, len, seg_index]);
        debug_assert!(ret.is_none());
        Ok(())
    }

    fn translate_data_drop(&mut self, mut pos: FuncCursor, seg_index: u32) -> WasmResult<()> {
        let seg_index = pos.ins().iconst(ir::types::I32, seg_index as i64);
        let ret = self.instance_call(&mut pos, &FN_DATA_DROP, &[seg_index]);
        debug_assert!(ret.is_none());
        Ok(())
    }

    fn translate_table_size(
        &mut self,
        mut pos: FuncCursor,
        table_index: TableIndex,
        _table: ir::Table,
    ) -> WasmResult<ir::Value> {
        let table_index = pos.ins().iconst(ir::types::I32, table_index.index() as i64);
        Ok(self
            .instance_call(&mut pos, &FN_TABLE_SIZE, &[table_index])
            .unwrap())
    }

    fn translate_table_grow(
        &mut self,
        mut pos: FuncCursor,
        table_index: u32,
        delta: ir::Value,
        init_value: ir::Value,
    ) -> WasmResult<ir::Value> {
        let table_index = pos.ins().iconst(ir::types::I32, table_index as i64);
        Ok(self
            .instance_call(&mut pos, &FN_TABLE_GROW, &[init_value, delta, table_index])
            .unwrap())
    }

    fn translate_table_get(
        &mut self,
        mut pos: FuncCursor,
        table_index: u32,
        index: ir::Value,
    ) -> WasmResult<ir::Value> {
        let table_index = pos.ins().iconst(ir::types::I32, table_index as i64);
        Ok(self
            .instance_call(&mut pos, &FN_TABLE_GET, &[index, table_index])
            .unwrap())
    }

    fn translate_table_set(
        &mut self,
        mut pos: FuncCursor,
        table_index: u32,
        value: ir::Value,
        index: ir::Value,
    ) -> WasmResult<()> {
        let table_index = pos.ins().iconst(ir::types::I32, table_index as i64);
        self.instance_call(&mut pos, &FN_TABLE_SET, &[index, value, table_index]);
        Ok(())
    }

    fn translate_table_copy(
        &mut self,
        mut pos: FuncCursor,
        dst_table_index: TableIndex,
        _dst_table: ir::Table,
        src_table_index: TableIndex,
        _src_table: ir::Table,
        dst: ir::Value,
        src: ir::Value,
        len: ir::Value,
    ) -> WasmResult<()> {
        let dst_index = pos
            .ins()
            .iconst(ir::types::I32, dst_table_index.index() as i64);
        let src_index = pos
            .ins()
            .iconst(ir::types::I32, src_table_index.index() as i64);
        self.instance_call(
            &mut pos,
            &FN_TABLE_COPY,
            &[dst, src, len, dst_index, src_index],
        );
        Ok(())
    }

    fn translate_table_fill(
        &mut self,
        mut pos: FuncCursor,
        table_index: u32,
        dst: ir::Value,
        val: ir::Value,
        len: ir::Value,
    ) -> WasmResult<()> {
        let table_index = pos.ins().iconst(ir::types::I32, table_index as i64);
        self.instance_call(&mut pos, &FN_TABLE_FILL, &[dst, val, len, table_index]);
        Ok(())
    }

    fn translate_table_init(
        &mut self,
        mut pos: FuncCursor,
        seg_index: u32,
        table_index: TableIndex,
        _table: ir::Table,
        dst: ir::Value,
        src: ir::Value,
        len: ir::Value,
    ) -> WasmResult<()> {
        let seg_index = pos.ins().iconst(ir::types::I32, seg_index as i64);
        let table_index = pos.ins().iconst(ir::types::I32, table_index.index() as i64);
        let ret = self.instance_call(
            &mut pos,
            &FN_TABLE_INIT,
            &[dst, src, len, seg_index, table_index],
        );
        debug_assert!(ret.is_none());
        Ok(())
    }

    fn translate_elem_drop(&mut self, mut pos: FuncCursor, seg_index: u32) -> WasmResult<()> {
        let seg_index = pos.ins().iconst(ir::types::I32, seg_index as i64);
        let ret = self.instance_call(&mut pos, &FN_ELEM_DROP, &[seg_index]);
        debug_assert!(ret.is_none());
        Ok(())
    }

    fn translate_ref_func(
        &mut self,
        mut pos: FuncCursor,
        func_index: u32,
    ) -> WasmResult<ir::Value> {
        let func_index = pos.ins().iconst(ir::types::I32, func_index as i64);
        Ok(self
            .instance_call(&mut pos, &FN_REF_FUNC, &[func_index])
            .unwrap())
    }

    fn translate_custom_global_get(
        &mut self,
        mut pos: FuncCursor,
        global_index: GlobalIndex,
    ) -> WasmResult<ir::Value> {
        let global = self.env.global(global_index);
        let ty = global.value_type()?;
        debug_assert!(ty == ir::types::R32 || ty == ir::types::R64);

        let (base_gv, offset) = self.global_address(pos.func, &global);
        let addr = pos.ins().global_value(POINTER_TYPE, base_gv);
        let flags = ir::MemFlags::trusted();
        Ok(pos.ins().load(ty, flags, addr, offset))
    }

    fn translate_custom_global_set(
        &mut self,
        mut pos: FuncCursor,
        global_index: GlobalIndex,
        val: ir::Value,
    ) -> WasmResult<()> {
        let global = self.env.global(global_index);
        let ty = global.value_type()?;
        debug_assert!(ty == ir::types::R32 || ty == ir::types::R64);

        let (global_addr_gv, global_addr_offset) = self.global_address(pos.func, &global);
        let global_addr = pos.ins().global_value(POINTER_TYPE, global_addr_gv);
        let abs_global_addr = pos.ins().iadd_imm(
            global_addr,
            ir::immediates::Imm64::new(global_addr_offset.into()),
        );

        let res = self.instance_call(&mut pos, &FN_PRE_BARRIER, &[abs_global_addr]);
        debug_assert!(res.is_none());

        let flags = ir::MemFlags::trusted();
        pos.ins().store(flags, val, abs_global_addr, offset32(0));

        let res = self.instance_call(&mut pos, &FN_POST_BARRIER, &[abs_global_addr]);
        debug_assert!(res.is_none());

        Ok(())
    }

    fn translate_loop_header(&mut self, mut pos: FuncCursor) -> WasmResult<()> {
        let interrupt = self.load_interrupt_flag(&mut pos);
        pos.ins().trapnz(interrupt, ir::TrapCode::Interrupt);
        Ok(())
    }

    fn return_mode(&self) -> ReturnMode {
        // Since we're using SM's epilogue insertion code, we can only handle a single return
        // instruction at the end of the function.
        ReturnMode::FallthroughReturn
    }
}

/// Information about a function table.
#[derive(Clone)]
struct TableInfo {
    /// Global variable containing a `wasm::TableTls` struct with two fields:
    ///
    /// 0: Unsigned 32-bit table length.
    /// n: Pointer to table (n = sizeof(void*))
    pub global: ir::GlobalValue,
}

impl TableInfo {
    /// Create a TableInfo and its global variable in `func`.
    pub fn new(
        wtab: bindings::TableDesc,
        func: &mut ir::Function,
        vmctx: ir::GlobalValue,
    ) -> TableInfo {
        // Create the global variable.
        let offset = wtab.tls_offset();
        assert!(offset < i32::max_value() as usize);
        let offset = imm64(offset);
        let global = func.create_global_value(ir::GlobalValueData::IAddImm {
            base: vmctx,
            offset,
            global_type: POINTER_TYPE,
        });

        TableInfo { global }
    }

    /// Get the size in bytes of each table entry.
    pub fn entry_size(&self) -> i64 {
        // Each entry is an `wasm::FunctionTableElem` which consists of the code pointer and a new
        // VM context pointer.
        (POINTER_SIZE * 2) as i64
    }
}

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

use baldrdash as bd;
use compile::{symbolic_function_name, wasm_function_name};
use cranelift_codegen::cursor::{Cursor, FuncCursor};
use cranelift_codegen::entity::{EntityRef, PrimaryMap, SecondaryMap};
use cranelift_codegen::ir;
use cranelift_codegen::ir::condcodes::IntCC;
use cranelift_codegen::ir::InstBuilder;
use cranelift_codegen::isa::{CallConv, TargetFrontendConfig, TargetIsa};
use cranelift_codegen::packed_option::PackedOption;
use cranelift_wasm::{
    FuncEnvironment, FuncIndex, GlobalIndex, GlobalVariable, MemoryIndex, ReturnMode,
    SignatureIndex, TableIndex, WasmResult,
};
use std::collections::HashMap;

/// Get the integer type used for representing pointers on this platform.
fn native_pointer_type() -> ir::Type {
    if cfg!(target_pointer_width = "64") {
        ir::types::I64
    } else {
        ir::types::I32
    }
}

/// Number of bytes in a native pointer.
pub fn native_pointer_size() -> i32 {
    if cfg!(target_pointer_width = "64") {
        8
    } else {
        4
    }
}

/// Convert a TlsData offset into a `Offset32` for a global decl.
fn offset32(offset: usize) -> ir::immediates::Offset32 {
    assert!(offset <= i32::max_value() as usize);
    (offset as i32).into()
}

/// Convert a usize offset into a `Imm64` for an iadd_imm.
fn imm64(offset: usize) -> ir::immediates::Imm64 {
    (offset as i64).into()
}

/// Convert a usize offset into a `Uimm64`.
fn uimm64(offset: usize) -> ir::immediates::Uimm64 {
    (offset as u64).into()
}

/// Initialize a `Signature` from a wasm signature.
fn init_sig_from_wsig(sig: &mut ir::Signature, wsig: bd::FuncTypeWithId) -> WasmResult<()> {
    sig.clear(CallConv::Baldrdash);
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
        native_pointer_type(),
        ir::ArgumentPurpose::VMContext,
    ));

    Ok(())
}

/// Initialize the signature `sig` to match the function with `index` in `env`.
pub fn init_sig(
    sig: &mut ir::Signature,
    env: &bd::ModuleEnvironment,
    func_index: FuncIndex,
) -> WasmResult<bd::FuncTypeWithId> {
    let wsig = env.function_signature(func_index);
    init_sig_from_wsig(sig, wsig)?;
    Ok(wsig)
}

/// A `TargetIsa` and `ModuleEnvironment` joined so we can implement `FuncEnvironment`.
pub struct TransEnv<'a, 'b, 'c> {
    isa: &'a TargetIsa,
    env: &'b bd::ModuleEnvironment<'b>,
    static_env: &'c bd::StaticEnvironment,

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
    symbolic: [PackedOption<ir::FuncRef>; 2],

    /// The address of the `cx` field in the `wasm::TlsData` struct.
    cx_addr: PackedOption<ir::GlobalValue>,

    /// The address of the `realm` field in the `wasm::TlsData` struct.
    realm_addr: PackedOption<ir::GlobalValue>,
}

impl<'a, 'b, 'c> TransEnv<'a, 'b, 'c> {
    pub fn new(
        isa: &'a TargetIsa,
        env: &'b bd::ModuleEnvironment,
        static_env: &'c bd::StaticEnvironment,
    ) -> Self {
        TransEnv {
            isa,
            env,
            static_env,
            tables: PrimaryMap::new(),
            signatures: HashMap::new(),
            func_gvs: SecondaryMap::new(),
            vmctx_gv: None.into(),
            instance_gv: None.into(),
            interrupt_gv: None.into(),
            symbolic: [None.into(); 2],
            cx_addr: None.into(),
            realm_addr: None.into(),
        }
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
                global_type: native_pointer_type(),
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
            global_type: native_pointer_type(),
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
                    offset: imm64(self.static_env.instanceTlsOffset),
                    global_type: native_pointer_type(),
                });
                self.instance_gv = gv.into();
                gv
            }
        };
        let ga = pos.ins().global_value(native_pointer_type(), gv);
        pos.ins()
            .load(native_pointer_type(), ir::MemFlags::new(), ga, 0)
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
                    offset: imm64(self.static_env.interruptTlsOffset),
                    global_type: native_pointer_type(),
                });
                self.interrupt_gv = gv.into();
                gv
            }
        };
        let ga = pos.ins().global_value(native_pointer_type(), gv);
        pos.ins()
            .load(native_pointer_type(), ir::MemFlags::new(), ga, 0)
    }

    /// Get a `FuncRef` for the given symbolic address.
    /// Uses the closure to create the signature if necessary.
    fn symbolic_funcref<MKSIG: FnOnce() -> ir::Signature>(
        &mut self,
        func: &mut ir::Function,
        sym: bd::SymbolicAddress,
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
                    offset: imm64(self.static_env.cxTlsOffset),
                    global_type: native_pointer_type(),
                })
                .into();
        }
        if self.realm_addr.is_none() {
            let vmctx = self.get_vmctx_gv(&mut pos.func);
            self.realm_addr = pos
                .func
                .create_global_value(ir::GlobalValueData::IAddImm {
                    base: vmctx,
                    offset: imm64(self.static_env.realmTlsOffset),
                    global_type: native_pointer_type(),
                })
                .into();
        }

        let ptr = native_pointer_type();
        let mut flags = ir::MemFlags::new();
        flags.set_aligned();
        flags.set_notrap();

        let cx_addr_val = pos.ins().global_value(ptr, self.cx_addr.unwrap());
        let cx = pos.ins().load(ptr, flags, cx_addr_val, 0);
        let realm_addr_val = pos.ins().global_value(ptr, self.realm_addr.unwrap());
        let realm = pos.ins().load(ptr, flags, realm_addr_val, 0);
        pos.ins()
            .store(flags, realm, cx, offset32(self.static_env.realmCxOffset));
    }

    /// Update the JSContext's realm value in preparation for making an indirect call through
    /// an external table.
    fn switch_to_indirect_callee_realm(&mut self, pos: &mut FuncCursor, vmctx: ir::Value) {
        let ptr = native_pointer_type();
        let mut flags = ir::MemFlags::new();
        flags.set_aligned();
        flags.set_notrap();

        let cx = pos
            .ins()
            .load(ptr, flags, vmctx, offset32(self.static_env.cxTlsOffset));
        let realm = pos
            .ins()
            .load(ptr, flags, vmctx, offset32(self.static_env.realmTlsOffset));
        pos.ins()
            .store(flags, realm, cx, offset32(self.static_env.realmCxOffset));
    }

    /// Update the JSContext's realm value in preparation for making a call to an imported
    /// function.
    fn switch_to_import_realm(
        &mut self,
        pos: &mut FuncCursor,
        vmctx: ir::Value,
        gv_addr: ir::Value,
    ) {
        let ptr = native_pointer_type();
        let mut flags = ir::MemFlags::new();
        flags.set_aligned();
        flags.set_notrap();

        let cx = pos
            .ins()
            .load(ptr, flags, vmctx, offset32(self.static_env.cxTlsOffset));
        let realm = pos.ins().load(
            ptr,
            flags,
            gv_addr,
            offset32(self.static_env.realmFuncImportTlsOffset),
        );
        pos.ins()
            .store(flags, realm, cx, offset32(self.static_env.realmCxOffset));
    }
}

impl<'a, 'b, 'c> FuncEnvironment for TransEnv<'a, 'b, 'c> {
    fn target_config(&self) -> TargetFrontendConfig {
        self.isa.frontend_config()
    }

    fn pointer_type(&self) -> ir::Type {
        native_pointer_type()
    }

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
            pos.next_ebb().expect("empty function");
            pos.next_inst();
            return Ok(GlobalVariable::Const(global.emit_constant(&mut pos)?));
        }

        // This is a global variable. Here we don't care if it is mutable or not.
        let vmctx_gv = self.get_vmctx_gv(func);
        let offset = global.tls_offset();

        // Some globals are represented as a pointer to the actual data, in which case we
        // must do an extra dereference to get to them.
        let (base_gv, offset) = if global.is_indirect() {
            let gv = func.create_global_value(ir::GlobalValueData::Load {
                base: vmctx_gv,
                offset: offset32(offset),
                global_type: native_pointer_type(),
                readonly: false,
            });
            (gv, 0.into())
        } else {
            (vmctx_gv, offset32(offset))
        };

        let mem_ty = global.value_type()?;

        Ok(GlobalVariable::Memory {
            gv: base_gv,
            ty: mem_ty,
            offset,
        })
    }

    fn make_heap(&mut self, func: &mut ir::Function, index: MemoryIndex) -> WasmResult<ir::Heap> {
        assert_eq!(index.index(), 0, "Only one WebAssembly memory supported");
        // Get the address of the `TlsData::memoryBase` field.
        let base_addr = self.get_vmctx_gv(func);
        // Get the `TlsData::memoryBase` field. We assume this is never modified during execution
        // of the function.
        let base = func.create_global_value(ir::GlobalValueData::Load {
            base: base_addr,
            offset: offset32(0),
            global_type: native_pointer_type(),
            readonly: true,
        });
        let min_size = ir::immediates::Uimm64::new(self.env.min_memory_length() as u64);
        let guard_size = uimm64(self.static_env.memoryGuardSize);

        let bound = self.static_env.staticMemoryBound;
        let style = if bound > 0 {
            // We have a static heap.
            let bound = (bound as u64).into();
            ir::HeapStyle::Static { bound }
        } else {
            // Get the `TlsData::boundsCheckLimit` field.
            let bound_gv = func.create_global_value(ir::GlobalValueData::Load {
                base: base_addr,
                offset: native_pointer_size().into(),
                global_type: ir::types::I32,
                readonly: false,
            });
            ir::HeapStyle::Dynamic { bound_gv }
        };

        Ok(func.create_heap(ir::HeapData {
            base,
            min_size,
            offset_guard_size: guard_size,
            style,
            index_type: ir::types::I32,
        }))
    }

    fn make_indirect_sig(
        &mut self,
        func: &mut ir::Function,
        index: SignatureIndex,
    ) -> WasmResult<ir::SigRef> {
        let mut sigdata = ir::Signature::new(CallConv::Baldrdash);
        let wsig = self.env.signature(index);
        init_sig_from_wsig(&mut sigdata, wsig)?;

        if wsig.id_kind() != bd::FuncTypeIdDescKind::None {
            // A signature to be used for an indirect call also takes a signature id.
            sigdata.params.push(ir::AbiParam::special(
                native_pointer_type(),
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
            offset: offset32(native_pointer_size() as usize),
            global_type: native_pointer_type(),
            readonly: false,
        });

        Ok(func.create_table(ir::TableData {
            base_gv,
            min_size: ir::immediates::Uimm64::new(0),
            bound_gv,
            element_size: ir::immediates::Uimm64::new(u64::from(self.pointer_bytes()) * 2),
            index_type: ir::types::I32,
        }))
    }

    fn make_direct_func(
        &mut self,
        func: &mut ir::Function,
        index: FuncIndex,
    ) -> WasmResult<ir::FuncRef> {
        // Create a signature.
        let mut sigdata = ir::Signature::new(CallConv::Baldrdash);
        init_sig(&mut sigdata, &self.env, index)?;
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

        // Currently, WebAssembly doesn't support multiple tables. That may change.
        assert_eq!(table_index.index(), 0);
        let wtable = self.get_table(pos.func, table_index);

        // Follows `MacroAssembler::wasmCallIndirect`:

        // 1. Materialize the signature ID.
        let sigid_value = match wsig.id_kind() {
            bd::FuncTypeIdDescKind::None => None,
            bd::FuncTypeIdDescKind::Immediate => {
                // The signature is represented as an immediate pointer-sized value.
                let imm = wsig.id_immediate() as i64;
                Some(pos.ins().iconst(native_pointer_type(), imm))
            }
            bd::FuncTypeIdDescKind::Global => {
                let gv = self.sig_global(pos.func, wsig.id_tls_offset());
                let addr = pos.ins().global_value(native_pointer_type(), gv);
                Some(
                    pos.ins()
                        .load(native_pointer_type(), ir::MemFlags::new(), addr, 0),
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
        let tbase = pos.ins().global_value(native_pointer_type(), base_gv);

        // 4. Load callee pointer from wtable.
        let callee_x = if native_pointer_type() != ir::types::I32 {
            pos.ins().uextend(native_pointer_type(), callee)
        } else {
            callee
        };
        let callee_scaled = pos.ins().imul_imm(callee_x, wtable.entry_size());

        let entry = pos.ins().iadd(tbase, callee_scaled);
        let callee_func = pos
            .ins()
            .load(native_pointer_type(), ir::MemFlags::new(), entry, 0);

        // Check for a null callee.
        pos.ins()
            .trapz(callee_func, ir::TrapCode::IndirectCallToNull);

        // Handle external tables, set up environment.
        // A function table call could redirect execution to another module with a different realm,
        // so switch to this realm just in case.
        let vmctx = pos.ins().load(
            native_pointer_type(),
            ir::MemFlags::new(),
            entry,
            native_pointer_size(),
        );
        self.switch_to_indirect_callee_realm(&mut pos, vmctx);

        // First the wasm args.
        let mut args = ir::ValueList::default();
        args.push(callee_func, &mut pos.func.dfg.value_lists);
        args.extend(call_args.iter().cloned(), &mut pos.func.dfg.value_lists);
        args.push(vmctx, &mut pos.func.dfg.value_lists);
        if let Some(sigid) = sigid_value {
            args.push(sigid, &mut pos.func.dfg.value_lists);
        }

        let call = pos
            .ins()
            .CallIndirect(ir::Opcode::CallIndirect, ir::types::INVALID, sig_ref, args)
            .0;
        self.switch_to_wasm_tls_realm(&mut pos);
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
            let gv_addr = pos.ins().global_value(native_pointer_type(), gv);

            // We need the first two pointer-sized fields from the `FuncImportTls` struct: `code`
            // and `tls`.
            let fit_code = pos
                .ins()
                .load(native_pointer_type(), ir::MemFlags::new(), gv_addr, 0);
            let fit_tls = pos.ins().load(
                native_pointer_type(),
                ir::MemFlags::new(),
                gv_addr,
                native_pointer_size(),
            );

            // Switch to the callee's realm.
            self.switch_to_import_realm(&mut pos, fit_tls, gv_addr);

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
        // We emit a call to `uint32_t memoryGrow_i32(Instance* instance, uint32_t delta)` via a
        // stub.
        let (fnref, sigref) =
            self.symbolic_funcref(pos.func, bd::SymbolicAddress::MemoryGrow, || {
                let mut sig = ir::Signature::new(CallConv::Baldrdash);
                sig.params.push(ir::AbiParam::new(native_pointer_type()));
                sig.params.push(ir::AbiParam::new(ir::types::I32).uext());
                sig.params.push(ir::AbiParam::special(
                    native_pointer_type(),
                    ir::ArgumentPurpose::VMContext,
                ));
                sig.returns.push(ir::AbiParam::new(ir::types::I32).uext());
                sig
            });

        // Get the instance pointer needed by `memoryGrow_i32`.
        let instance = self.load_instance(&mut pos);
        let vmctx = pos
            .func
            .special_param(ir::ArgumentPurpose::VMContext)
            .expect("Missing vmctx arg");
        // We must use `func_addr` for symbolic references since the stubs can be far away, and the
        // C++ `SymbolicAccess` linker expects it.
        let addr = pos.ins().func_addr(native_pointer_type(), fnref);
        let call = pos
            .ins()
            .call_indirect(sigref, addr, &[instance, val, vmctx]);
        self.switch_to_wasm_tls_realm(&mut pos);
        Ok(pos.func.dfg.first_result(call))
    }

    fn translate_memory_size(
        &mut self,
        mut pos: FuncCursor,
        _index: MemoryIndex,
        _heap: ir::Heap,
    ) -> WasmResult<ir::Value> {
        // We emit a call to `uint32_t memorySize_i32(Instance* instance)` via a stub.
        let (fnref, sigref) =
            self.symbolic_funcref(pos.func, bd::SymbolicAddress::MemorySize, || {
                let mut sig = ir::Signature::new(CallConv::Baldrdash);
                sig.params.push(ir::AbiParam::new(native_pointer_type()));
                sig.params.push(ir::AbiParam::special(
                    native_pointer_type(),
                    ir::ArgumentPurpose::VMContext,
                ));
                sig.returns.push(ir::AbiParam::new(ir::types::I32).uext());
                sig
            });

        // Get the instance pointer needed by `memorySize_i32`.
        let instance = self.load_instance(&mut pos);
        let vmctx = pos
            .func
            .special_param(ir::ArgumentPurpose::VMContext)
            .expect("Missing vmctx arg");
        let addr = pos.ins().func_addr(native_pointer_type(), fnref);
        let call = pos.ins().call_indirect(sigref, addr, &[instance, vmctx]);
        self.switch_to_wasm_tls_realm(&mut pos);
        Ok(pos.func.dfg.first_result(call))
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
    pub fn new(wtab: bd::TableDesc, func: &mut ir::Function, vmctx: ir::GlobalValue) -> TableInfo {
        // Create the global variable.
        let offset = wtab.tls_offset();
        assert!(offset < i32::max_value() as usize);
        let offset = imm64(offset);
        let global = func.create_global_value(ir::GlobalValueData::IAddImm {
            base: vmctx,
            offset,
            global_type: native_pointer_type(),
        });

        TableInfo { global }
    }

    /// Get the size in bytes of each table entry.
    pub fn entry_size(&self) -> i64 {
        // Each entry is an `wasm::FunctionTableElem` which consists of the code pointer and a new
        // VM context pointer.
        i64::from(native_pointer_size()) * 2
    }
}

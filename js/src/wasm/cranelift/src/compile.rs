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

//! Cranelift WebAssembly function compiler.
//!
//! This module defines the `compile()` function which uses Cranelift to compile a single
//! WebAssembly function.

use std::fmt;
use std::mem;

use cranelift_codegen::binemit::{
    Addend, CodeInfo, CodeOffset, NullStackmapSink, NullTrapSink, Reloc, RelocSink,
};
use cranelift_codegen::entity::EntityRef;
use cranelift_codegen::ir::{self, constant::ConstantOffset, stackslot::StackSize};
use cranelift_codegen::isa::TargetIsa;
use cranelift_codegen::CodegenResult;
use cranelift_codegen::Context;
use cranelift_wasm::{FuncIndex, FuncTranslator, WasmResult};

use crate::bindings;
use crate::isa::make_isa;
use crate::utils::DashResult;
use crate::wasm2clif::{init_sig, native_pointer_size, TransEnv};

// Namespace for user-defined functions.
const USER_FUNCTION_NAMESPACE: u32 = 0;

// Namespace for builtins functions that are translated to symbolic accesses in Spidermonkey.
const SYMBOLIC_FUNCTION_NAMESPACE: u32 = 1;

/// The result of a function's compilation: code + metadata.
pub struct CompiledFunc {
    pub frame_pushed: StackSize,
    pub contains_calls: bool,
    pub metadata: Vec<bindings::MetadataEntry>,
    // rodata_relocs is Vec<CodeOffset>, but u32 is C++-friendlier
    pub rodata_relocs: Vec<u32>,
    // TODO(bbouvier) should just be a pointer into the masm buffer
    pub code_buffer: Vec<u8>,
    pub code_size: CodeOffset,
    pub jumptables_size: CodeOffset,
    pub rodata_size: CodeOffset,
}

impl CompiledFunc {
    fn new() -> Self {
        Self {
            frame_pushed: 0,
            contains_calls: false,
            metadata: vec![],
            rodata_relocs: vec![],
            code_buffer: vec![],
            code_size: 0,
            jumptables_size: 0,
            rodata_size: 0,
        }
    }

    fn reset(self: &mut Self, frame_pushed: StackSize, contains_calls: bool) {
        self.frame_pushed = frame_pushed;
        self.contains_calls = contains_calls;
        self.metadata.clear();
        self.rodata_relocs.clear();
        self.code_buffer.clear();
        self.code_size = 0;
        self.jumptables_size = 0;
        self.rodata_size = 0;
    }
}

/// A batch compiler holds on to data structures that can be recycled for multiple function
/// compilations.
pub struct BatchCompiler<'a, 'b> {
    static_environ: &'a bindings::StaticEnvironment,
    environ: bindings::ModuleEnvironment<'b>,
    isa: Box<dyn TargetIsa>,
    context: Context,
    trans: FuncTranslator,
    pub current_func: CompiledFunc,
}

impl<'a, 'b> BatchCompiler<'a, 'b> {
    pub fn new(
        static_environ: &'a bindings::StaticEnvironment,
        environ: bindings::ModuleEnvironment<'b>,
    ) -> DashResult<Self> {
        // TODO: The target ISA could be shared by multiple batch compilers across threads.
        Ok(BatchCompiler {
            static_environ,
            environ,
            isa: make_isa(static_environ)?,
            context: Context::new(),
            trans: FuncTranslator::new(),
            current_func: CompiledFunc::new(),
        })
    }

    pub fn compile(&mut self) -> CodegenResult<()> {
        let info = self.context.compile(&*self.isa)?;
        debug!("Optimized wasm function IR: {}", self);
        self.binemit(info)
    }

    /// Translate the WebAssembly code to Cranelift IR.
    pub fn translate_wasm(&mut self, func: &bindings::FuncCompileInput) -> WasmResult<()> {
        self.context.clear();

        let tenv = &mut TransEnv::new(&*self.isa, &self.environ, self.static_environ);

        // Set up the signature before translating the WebAssembly byte code.
        // The translator refers to it.
        let index = FuncIndex::new(func.index as usize);

        let new_sig = init_sig(&self.environ, self.static_environ.call_conv(), index)?;
        let new_func = ir::Function::with_name_signature(wasm_function_name(index), new_sig);
        self.context.func = self.trans.translate(
            func.bytecode(),
            func.offset_in_module as usize,
            new_func,
            tenv,
        )?;

        info!("Translated wasm function {}.", func.index);
        debug!("Translated wasm function IR: {}", self);
        Ok(())
    }

    /// Emit binary machine code to `emitter`.
    fn binemit(&mut self, info: CodeInfo) -> CodegenResult<()> {
        let total_size = info.total_size as usize;
        let frame_pushed = self.frame_pushed();
        let contains_calls = self.contains_calls();

        info!(
            "Emitting {} bytes, frame_pushed={}\n.",
            total_size, frame_pushed
        );

        self.current_func.reset(frame_pushed, contains_calls);

        // Generate metadata about function calls and traps now that the emitter knows where the
        // Cranelift code is going to end up.
        let mut metadata = mem::replace(&mut self.current_func.metadata, vec![]);
        self.emit_metadata(&mut metadata);
        mem::swap(&mut metadata, &mut self.current_func.metadata);

        // TODO: If we can get a pointer into `size` pre-allocated bytes of memory, we wouldn't
        // have to allocate and copy here.
        // TODO(bbouvier) try to get this pointer from the C++ caller, with an unlikely callback to
        // C++ if the remaining size is smaller than  needed.
        if self.current_func.code_buffer.len() < total_size {
            let current_size = self.current_func.code_buffer.len();
            // There's no way to do a proper uninitialized reserve, so first reserve and then
            // unsafely set the final size.
            self.current_func
                .code_buffer
                .reserve(total_size - current_size);
            unsafe { self.current_func.code_buffer.set_len(total_size) };
        }

        {
            let emit_env = &mut EmitEnv::new(
                &mut self.current_func.metadata,
                &mut self.current_func.rodata_relocs,
            );
            let mut trap_sink = NullTrapSink {};

            // TODO (bug 1574865) Support reference types and stackmaps in Baldrdash.
            let mut stackmap_sink = NullStackmapSink {};

            unsafe {
                let code_buffer = &mut self.current_func.code_buffer;
                self.context.emit_to_memory(
                    &*self.isa,
                    code_buffer.as_mut_ptr(),
                    emit_env,
                    &mut trap_sink,
                    &mut stackmap_sink,
                )
            };
        }

        self.current_func.code_size = info.code_size;
        self.current_func.jumptables_size = info.jumptables_size;
        self.current_func.rodata_size = info.rodata_size;

        Ok(())
    }

    /// Compute the `framePushed` argument to pass to `GenerateFunctionPrologue`. This is the
    /// number of frame bytes used by Cranelift, not counting the values pushed by the standard
    /// prologue generated by `GenerateFunctionPrologue`.
    fn frame_pushed(&self) -> StackSize {
        // Cranelift computes the total stack frame size including the pushed return address,
        // standard SM prologue pushes, and its own stack slots.
        let total = self.context.func.stack_slots.frame_size.expect("No frame");
        let sm_pushed = StackSize::from(self.isa.flags().baldrdash_prologue_words())
            * mem::size_of::<usize>() as StackSize;
        total
            .checked_sub(sm_pushed)
            .expect("SpiderMonkey prologue pushes not counted")
    }

    /// Determine whether the current function may contain calls.
    fn contains_calls(&self) -> bool {
        // Conservatively, just check to see if it contains any function
        // signatures which could be called.
        !self.context.func.dfg.signatures.is_empty()
    }

    #[cfg(feature = "cranelift_x86")]
    fn platform_specific_ignores_metadata(opcode: ir::Opcode) -> bool {
        match opcode {
            ir::Opcode::X86Sdivmodx | ir::Opcode::X86Udivmodx => true,
            _ => false,
        }
    }

    #[cfg(not(feature = "cranelift_x86"))]
    fn platform_specific_ignores_metadata(_opcode: ir::Opcode) -> bool {
        false
    }

    /// Emit metadata by scanning the compiled function before `emit_to_memory`.
    ///
    /// - All call sites need metadata: direct, indirect, symbolic.
    /// - All explicit traps must be registered.
    ///
    /// We don't get enough callbacks through the `RelocSink` trait to generate all the metadata we
    /// need.
    fn emit_metadata(&self, metadata: &mut Vec<bindings::MetadataEntry>) {
        let encinfo = self.isa.encoding_info();
        let func = &self.context.func;
        for ebb in func.layout.ebbs() {
            for (offset, inst, inst_size) in func.inst_offsets(ebb, &encinfo) {
                let opcode = func.dfg[inst].opcode();
                match opcode {
                    ir::Opcode::Call => self.call_metadata(metadata, inst, offset + inst_size),
                    ir::Opcode::CallIndirect => {
                        self.indirect_call_metadata(metadata, inst, offset + inst_size)
                    }
                    ir::Opcode::Trap | ir::Opcode::Trapif | ir::Opcode::Trapff => {
                        self.trap_metadata(metadata, inst, offset)
                    }
                    ir::Opcode::Load
                    | ir::Opcode::LoadComplex
                    | ir::Opcode::Uload8
                    | ir::Opcode::Uload8Complex
                    | ir::Opcode::Sload8
                    | ir::Opcode::Sload8Complex
                    | ir::Opcode::Uload16
                    | ir::Opcode::Uload16Complex
                    | ir::Opcode::Sload16
                    | ir::Opcode::Sload16Complex
                    | ir::Opcode::Uload32
                    | ir::Opcode::Uload32Complex
                    | ir::Opcode::Sload32
                    | ir::Opcode::Sload32Complex
                    | ir::Opcode::Store
                    | ir::Opcode::StoreComplex
                    | ir::Opcode::Istore8
                    | ir::Opcode::Istore8Complex
                    | ir::Opcode::Istore16
                    | ir::Opcode::Istore16Complex
                    | ir::Opcode::Istore32
                    | ir::Opcode::Istore32Complex => self.memory_metadata(metadata, inst, offset),

                    // Instructions that are not going to trap in our use, even though their opcode
                    // says they can.
                    ir::Opcode::Spill
                    | ir::Opcode::Fill
                    | ir::Opcode::FillNop
                    | ir::Opcode::JumpTableEntry => {}

                    _ if BatchCompiler::platform_specific_ignores_metadata(opcode) => {}

                    _ => {
                        debug_assert!(!opcode.is_call(), "Missed call opcode");
                        debug_assert!(
                            !opcode.can_trap(),
                            "Missed trap: {}",
                            func.dfg.display_inst(inst, Some(self.isa.as_ref()))
                        );
                        debug_assert!(
                            !opcode.can_load(),
                            "Missed load: {}",
                            func.dfg.display_inst(inst, Some(self.isa.as_ref()))
                        );
                        debug_assert!(
                            !opcode.can_store(),
                            "Missed store: {}",
                            func.dfg.display_inst(inst, Some(self.isa.as_ref()))
                        );
                    }
                }
            }
        }
    }

    fn srcloc(&self, inst: ir::Inst) -> ir::SourceLoc {
        let srcloc = self.context.func.srclocs[inst];
        debug_assert!(
            !srcloc.is_default(),
            "No source location on {}",
            self.context
                .func
                .dfg
                .display_inst(inst, Some(self.isa.as_ref()))
        );
        srcloc
    }

    /// Emit metadata for direct call `inst`.
    fn call_metadata(
        &self,
        metadata: &mut Vec<bindings::MetadataEntry>,
        inst: ir::Inst,
        ret_addr: CodeOffset,
    ) {
        let func = &self.context.func;

        // This is a direct call, so the callee should be a non-imported wasm
        // function. We register both the call site *and* the target for relocation.
        let callee = match func.dfg[inst] {
            ir::InstructionData::Call { func_ref, .. } => &func.dfg.ext_funcs[func_ref].name,
            _ => panic!("Bad format for call"),
        };

        let func_index = match *callee {
            ir::ExternalName::User {
                namespace: USER_FUNCTION_NAMESPACE,
                index,
            } => FuncIndex::new(index as usize),
            _ => panic!("Direct call to {} unsupported", callee),
        };

        metadata.push(bindings::MetadataEntry::direct_call(
            ret_addr,
            func_index,
            self.srcloc(inst),
        ));
    }

    /// Emit metadata for indirect call `inst`.
    fn indirect_call_metadata(
        &self,
        metadata: &mut Vec<bindings::MetadataEntry>,
        inst: ir::Inst,
        ret_addr: CodeOffset,
    ) {
        // A call_indirect instruction can represent either a table call or a far call to a runtime
        // function. The CallSiteDesc::Kind enum does distinguish between the two, but it is not
        // clear that the information is used anywhere. For now, we won't bother distinguishing
        // them, and mark all calls as `Kind::Dynamic`.
        //
        // If we do need to make a distinction in the future, it is probably easiest to add a
        // `call_far` instruction to Cranelift that encodes like an indirect call, but includes the
        // callee like a direct call.
        metadata.push(bindings::MetadataEntry::indirect_call(
            ret_addr,
            self.srcloc(inst),
        ));
    }

    fn trap_metadata(
        &self,
        metadata: &mut Vec<bindings::MetadataEntry>,
        inst: ir::Inst,
        offset: CodeOffset,
    ) {
        let func = &self.context.func;
        let (code, trap_offset) = match func.dfg[inst] {
            ir::InstructionData::Trap { code, .. } => (code, 0),
            ir::InstructionData::IntCondTrap { code, .. }
            | ir::InstructionData::FloatCondTrap { code, .. } => {
                // This instruction expands to a conditional branch over ud2 on Intel archs.
                // The actual trap happens on the ud2 instruction.
                (code, 2)
            }
            _ => panic!("Bad format for trap"),
        };

        // Translate the trap code into one of BaldrMonkey's trap codes.
        let bd_trap = match code {
            ir::TrapCode::StackOverflow => bindings::Trap::StackOverflow,
            ir::TrapCode::HeapOutOfBounds => bindings::Trap::OutOfBounds,
            ir::TrapCode::OutOfBounds => bindings::Trap::OutOfBounds,
            ir::TrapCode::TableOutOfBounds => bindings::Trap::OutOfBounds,
            ir::TrapCode::IndirectCallToNull => bindings::Trap::IndirectCallToNull,
            ir::TrapCode::BadSignature => bindings::Trap::IndirectCallBadSig,
            ir::TrapCode::IntegerOverflow => bindings::Trap::IntegerOverflow,
            ir::TrapCode::IntegerDivisionByZero => bindings::Trap::IntegerDivideByZero,
            ir::TrapCode::BadConversionToInteger => bindings::Trap::InvalidConversionToInteger,
            ir::TrapCode::Interrupt => bindings::Trap::CheckInterrupt,
            ir::TrapCode::UnreachableCodeReached => bindings::Trap::Unreachable,
            ir::TrapCode::User(_) => panic!("Uncovered trap code {}", code),
        };

        metadata.push(bindings::MetadataEntry::trap(
            offset + trap_offset,
            self.srcloc(inst),
            bd_trap,
        ));
    }

    fn memory_metadata(
        &self,
        metadata: &mut Vec<bindings::MetadataEntry>,
        inst: ir::Inst,
        offset: CodeOffset,
    ) {
        let func = &self.context.func;
        let memflags = match func.dfg[inst] {
            ir::InstructionData::Load { flags, .. }
            | ir::InstructionData::LoadComplex { flags, .. }
            | ir::InstructionData::Store { flags, .. }
            | ir::InstructionData::StoreComplex { flags, .. } => flags,
            _ => panic!("Bad format for memory access"),
        };

        // Some load/store instructions may be accessing VM data structures instead of the
        // WebAssembly heap. These are tagged with `notrap` since their trapping is not part of
        // the semantics, i.e. that would be a bug.
        if memflags.notrap() {
            return;
        }

        metadata.push(bindings::MetadataEntry::memory_access(
            offset,
            self.srcloc(inst),
        ));
    }
}

impl<'a, 'b> fmt::Display for BatchCompiler<'a, 'b> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self.context.func.display(self.isa.as_ref()))
    }
}

/// Create a Cranelift function name representing a WebAssembly function with `index`.
pub fn wasm_function_name(func: FuncIndex) -> ir::ExternalName {
    ir::ExternalName::User {
        namespace: USER_FUNCTION_NAMESPACE,
        index: func.index() as u32,
    }
}

/// Create a Cranelift function name representing a builtin function.
pub fn symbolic_function_name(sym: bindings::SymbolicAddress) -> ir::ExternalName {
    ir::ExternalName::User {
        namespace: SYMBOLIC_FUNCTION_NAMESPACE,
        index: sym as u32,
    }
}

/// References joined so we can implement `RelocSink`.
struct EmitEnv<'a> {
    metadata: &'a mut Vec<bindings::MetadataEntry>,
    rodata_relocs: &'a mut Vec<CodeOffset>,
}

impl<'a> EmitEnv<'a> {
    pub fn new(
        metadata: &'a mut Vec<bindings::MetadataEntry>,
        rodata_relocs: &'a mut Vec<CodeOffset>,
    ) -> EmitEnv<'a> {
        EmitEnv {
            metadata,
            rodata_relocs,
        }
    }
}

impl<'a> RelocSink for EmitEnv<'a> {
    fn reloc_ebb(&mut self, _offset: CodeOffset, _reloc: Reloc, _ebb_offset: CodeOffset) {
        unimplemented!();
    }

    fn reloc_external(
        &mut self,
        offset: CodeOffset,
        _reloc: Reloc,
        name: &ir::ExternalName,
        _addend: Addend,
    ) {
        // Decode the function name.
        match *name {
            ir::ExternalName::User {
                namespace: USER_FUNCTION_NAMESPACE,
                ..
            } => {
                // This is a direct function call handled by `call_metadata` above.
            }

            ir::ExternalName::User {
                namespace: SYMBOLIC_FUNCTION_NAMESPACE,
                index,
            } => {
                // This is a symbolic function reference encoded by `symbolic_function_name()`.
                let sym = index.into();

                // The symbolic access patch address points *after* the stored pointer.
                let offset = offset + native_pointer_size() as u32;
                self.metadata
                    .push(bindings::MetadataEntry::symbolic_access(offset, sym));
            }

            ir::ExternalName::LibCall(call) => {
                let sym = match call {
                    ir::LibCall::CeilF32 => bindings::SymbolicAddress::CeilF32,
                    ir::LibCall::CeilF64 => bindings::SymbolicAddress::CeilF64,
                    ir::LibCall::FloorF32 => bindings::SymbolicAddress::FloorF32,
                    ir::LibCall::FloorF64 => bindings::SymbolicAddress::FloorF64,
                    ir::LibCall::NearestF32 => bindings::SymbolicAddress::NearestF32,
                    ir::LibCall::NearestF64 => bindings::SymbolicAddress::NearestF64,
                    ir::LibCall::TruncF32 => bindings::SymbolicAddress::TruncF32,
                    ir::LibCall::TruncF64 => bindings::SymbolicAddress::TruncF64,
                    _ => {
                        panic!("Don't understand external {}", name);
                    }
                };

                // The symbolic access patch address points *after* the stored pointer.
                let offset = offset + native_pointer_size() as u32;
                self.metadata
                    .push(bindings::MetadataEntry::symbolic_access(offset, sym));
            }

            _ => {
                panic!("Don't understand external {}", name);
            }
        }
    }

    fn reloc_jt(&mut self, offset: CodeOffset, reloc: Reloc, _jt: ir::JumpTable) {
        match reloc {
            Reloc::X86PCRelRodata4 => {
                self.rodata_relocs.push(offset);
            }
            _ => {
                panic!("Unhandled/unexpected reloc type");
            }
        }
    }

    fn reloc_constant(
        &mut self,
        _offset: CodeOffset,
        _reloc: Reloc,
        _constant_pool_offset: ConstantOffset,
    ) {
        unimplemented!("constant pools NYI");
    }
}

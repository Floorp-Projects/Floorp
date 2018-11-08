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

use baldrdash as bd;
use cpu::make_isa;
use cranelift_codegen::binemit::{Addend, CodeOffset, NullTrapSink, Reloc, RelocSink, TrapSink};
use cranelift_codegen::cursor::{Cursor, FuncCursor};
use cranelift_codegen::entity::EntityRef;
use cranelift_codegen::ir::stackslot::StackSize;
use cranelift_codegen::ir::{InstBuilder, SourceLoc, TrapCode};
use cranelift_codegen::isa::TargetIsa;
use cranelift_codegen::settings::Flags;
use cranelift_codegen::{self, ir};
use cranelift_codegen::{CodegenError, CodegenResult};
use cranelift_wasm::{
    self, FuncIndex, GlobalIndex, MemoryIndex, SignatureIndex, TableIndex, WasmResult,
};
use std::fmt;
use std::mem;
use utils::DashResult;
use wasm2clif::{init_sig, native_pointer_size, TransEnv};

/// The result of a function's compilation: code + metadata.
pub struct CompiledFunc {
    pub frame_pushed: StackSize,
    pub contains_calls: bool,
    pub metadata: Vec<bd::MetadataEntry>,
    // TODO(bbouvier) should just be a pointer into the masm buffer
    pub code_buffer: Vec<u8>,
}

impl CompiledFunc {
    fn new() -> Self {
        Self {
            frame_pushed: 0,
            contains_calls: false,
            metadata: vec![],
            code_buffer: vec![],
        }
    }

    fn reset(self: &mut Self, frame_pushed: StackSize, contains_calls: bool) {
        self.frame_pushed = frame_pushed;
        self.contains_calls = contains_calls;
        self.metadata.clear();
        self.code_buffer.clear();
    }
}

/// A batch compiler holds on to data structures that can be recycled for multiple function
/// compilations.
pub struct BatchCompiler<'a, 'b> {
    static_environ: &'a bd::StaticEnvironment,
    environ: bd::ModuleEnvironment<'b>,
    isa: Box<TargetIsa>,
    context: cranelift_codegen::Context,
    trans: cranelift_wasm::FuncTranslator,
    pub current_func: CompiledFunc,
}

impl<'a, 'b> BatchCompiler<'a, 'b> {
    pub fn new(
        static_environ: &'a bd::StaticEnvironment,
        environ: bd::ModuleEnvironment<'b>,
    ) -> DashResult<Self> {
        // TODO: The target ISA could be shared by multiple batch compilers across threads.
        Ok(BatchCompiler {
            static_environ,
            environ,
            isa: make_isa(static_environ)?,
            context: cranelift_codegen::Context::new(),
            trans: cranelift_wasm::FuncTranslator::new(),
            current_func: CompiledFunc::new(),
        })
    }

    pub fn compile(&mut self) -> CodegenResult<()> {
        let size = self.context.compile(&*self.isa)?;
        self.binemit(size as usize)
    }

    /// Translate the WebAssembly code to Cranelift IR.
    pub fn translate_wasm(
        &mut self,
        func: &bd::FuncCompileInput,
    ) -> WasmResult<bd::FuncTypeWithId> {
        self.context.clear();

        // Set up the signature before translating the WebAssembly byte code.
        // The translator refers to it.
        let index = FuncIndex::new(func.index as usize);
        let wsig = init_sig(&mut self.context.func.signature, &self.environ, index);
        self.context.func.name = wasm_function_name(index);

        let tenv = &mut TransEnv::new(&*self.isa, &self.environ, self.static_environ);
        self.trans
            .translate(func.bytecode(), &mut self.context.func, tenv)?;

        info!("Translated wasm function {}.", func.index);
        debug!("Content: {}", self.context.func.display(&*self.isa));
        Ok(wsig)
    }

    /// Emit binary machine code to `emitter`.
    fn binemit(&mut self, size: usize) -> CodegenResult<()> {
        let frame_pushed = self.frame_pushed();
        let contains_calls = self.contains_calls();

        info!("Emitting {} bytes, frame_pushed={}\n.", size, frame_pushed);

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
        if self.current_func.code_buffer.len() < size {
            let current_size = self.current_func.code_buffer.len();
            // There's no way to do a proper uninitialized reserve, so first reserve and then
            // unsafely set the final size.
            self.current_func.code_buffer.reserve(size - current_size);
            unsafe { self.current_func.code_buffer.set_len(size) };
        }

        {
            let eenv = &mut EmitEnv::new(
                &self.context.func,
                &self.environ,
                &mut self.current_func.metadata,
            );
            let mut trap_sink = NullTrapSink {};
            unsafe {
                let code_buffer = &mut self.current_func.code_buffer;
                self.context.emit_to_memory(
                    &*self.isa,
                    code_buffer.as_mut_ptr(),
                    eenv,
                    &mut trap_sink,
                )
            };
        }

        Ok(())
    }

    /// Compute the `framePushed` argument to pass to `GenerateFunctionPrologue`. This is the
    /// number of frame bytes used by Cranelift, not counting the values pushed by the standard
    /// prologue generated by `GenerateFunctionPrologue`.
    fn frame_pushed(&self) -> StackSize {
        // Cranelift computes the total stack frame size including the pushed return address,
        // standard SM prologue pushes, and its own stack slots.
        let total = self.context.func.stack_slots.frame_size.expect("No frame");
        let sm_pushed = self.isa.flags().baldrdash_prologue_words() as StackSize
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

    /// Emit metadata by scanning the compiled function before `emit_to_memory`.
    ///
    /// - All call sites need metadata: direct, indirect, symbolic.
    /// - All explicit traps must be registered.
    ///
    /// We don't get enough callbacks through the `RelocSink` trait to generate all the metadata we
    /// need.
    fn emit_metadata(&self, metadata: &mut Vec<bd::MetadataEntry>) {
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
                    | ir::Opcode::X86Sdivmodx
                    | ir::Opcode::X86Udivmodx => {}

                    _ => {
                        assert!(!opcode.is_call(), "Missed call opcode");
                        assert!(
                            !opcode.can_trap(),
                            "Missed trap: {}",
                            func.dfg.display_inst(inst, Some(self.isa.as_ref()))
                        );
                        assert!(
                            !opcode.can_load(),
                            "Missed load: {}",
                            func.dfg.display_inst(inst, Some(self.isa.as_ref()))
                        );
                        assert!(
                            !opcode.can_store(),
                            "Missed store: {}",
                            func.dfg.display_inst(inst, Some(self.isa.as_ref()))
                        );
                    }
                }
            }
        }
    }

    /// Emit metadata for direct call `inst`.
    fn call_metadata(
        &self,
        metadata: &mut Vec<bd::MetadataEntry>,
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

        let func_index = match callee {
            &ir::ExternalName::User {
                namespace: 0,
                index,
            } => FuncIndex::new(index as usize),
            _ => panic!("Direct call to {} unsupported", callee),
        };

        let srcloc = func.srclocs[inst];
        assert!(
            !srcloc.is_default(),
            "No source location on {}",
            func.dfg.display_inst(inst, Some(self.isa.as_ref()))
        );

        metadata.push(bd::MetadataEntry::direct_call(ret_addr, func_index, srcloc));
    }

    /// Emit metadata for indirect call `inst`.
    fn indirect_call_metadata(
        &self,
        metadata: &mut Vec<bd::MetadataEntry>,
        inst: ir::Inst,
        ret_addr: CodeOffset,
    ) {
        let func = &self.context.func;

        // A call_indirect instruction can represent either a table call or a far call to a runtime
        // function. The CallSiteDesc::Kind enum does distinguish between the two, but it is not
        // clear that the information is used anywhere. For now, we won't bother distinguishing
        // them, and mark all calls as `Kind::Dynamic`.
        //
        // If we do need to make a distinction in the future, it is probably easiest to add a
        // `call_far` instruction to Cranelift that encodes like an indirect call, but includes the
        // callee like a direct call.

        let srcloc = func.srclocs[inst];
        assert!(
            !srcloc.is_default(),
            "No source location on {}",
            func.dfg.display_inst(inst, Some(self.isa.as_ref()))
        );

        metadata.push(bd::MetadataEntry::indirect_call(ret_addr, srcloc));
    }

    fn trap_metadata(
        &self,
        metadata: &mut Vec<bd::MetadataEntry>,
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
            ir::TrapCode::StackOverflow => bd::Trap::StackOverflow,
            ir::TrapCode::HeapOutOfBounds => bd::Trap::OutOfBounds,
            ir::TrapCode::OutOfBounds => bd::Trap::OutOfBounds,
            ir::TrapCode::TableOutOfBounds => bd::Trap::OutOfBounds,
            ir::TrapCode::IndirectCallToNull => bd::Trap::IndirectCallToNull,
            ir::TrapCode::BadSignature => bd::Trap::IndirectCallBadSig,
            ir::TrapCode::IntegerOverflow => bd::Trap::IntegerOverflow,
            ir::TrapCode::IntegerDivisionByZero => bd::Trap::IntegerDivideByZero,
            ir::TrapCode::BadConversionToInteger => bd::Trap::InvalidConversionToInteger,
            ir::TrapCode::Interrupt => bd::Trap::CheckInterrupt,
            ir::TrapCode::User(0) => bd::Trap::Unreachable,
            ir::TrapCode::User(_) => panic!("Uncovered trap code {}", code),
        };

        let srcloc = func.srclocs[inst];
        assert!(
            !srcloc.is_default(),
            "No source location on {}",
            func.dfg.display_inst(inst, Some(self.isa.as_ref()))
        );

        metadata.push(bd::MetadataEntry::trap(
            offset + trap_offset,
            srcloc,
            bd_trap,
        ));
    }

    fn memory_metadata(
        &self,
        metadata: &mut Vec<bd::MetadataEntry>,
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

        let srcloc = func.srclocs[inst];
        assert!(
            !srcloc.is_default(),
            "No source location on {}",
            func.dfg.display_inst(inst, Some(self.isa.as_ref()))
        );

        metadata.push(bd::MetadataEntry::memory_access(offset, srcloc));
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
        namespace: 0,
        index: func.index() as u32,
    }
}

/// Create a Cranelift function name representing a builtin function.
pub fn symbolic_function_name(sym: bd::SymbolicAddress) -> ir::ExternalName {
    ir::ExternalName::User {
        namespace: 1,
        index: sym as u32,
    }
}

/// References joined so we can implement `RelocSink`.
struct EmitEnv<'a, 'b, 'c> {
    func: &'a ir::Function,
    env: &'b bd::ModuleEnvironment<'b>,
    metadata: &'c mut Vec<bd::MetadataEntry>,
}

impl<'a, 'b, 'c> EmitEnv<'a, 'b, 'c> {
    pub fn new(
        func: &'a ir::Function,
        env: &'b bd::ModuleEnvironment,
        metadata: &'c mut Vec<bd::MetadataEntry>,
    ) -> EmitEnv<'a, 'b, 'c> {
        EmitEnv {
            func,
            env,
            metadata,
        }
    }
}

impl<'a, 'b, 'c> RelocSink for EmitEnv<'a, 'b, 'c> {
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
        match name {
            &ir::ExternalName::User {
                namespace: 0,
                index,
            } => {
                // This is a direct function call handled by `emit_metadata` above.
            }
            &ir::ExternalName::User {
                namespace: 1,
                index,
            } => {
                // This is a symbolic function reference encoded by `symbolic_function_name()`.
                let sym = index.into();
                // The symbolic access patch address points *after* the stored pointer.
                let offset = offset + native_pointer_size() as u32;
                self.metadata
                    .push(bd::MetadataEntry::symbolic_access(offset, sym));
            }
            &ir::ExternalName::LibCall(call) => {
                let sym = match call {
                    ir::LibCall::CeilF32 => bd::SymbolicAddress::CeilF32,
                    ir::LibCall::CeilF64 => bd::SymbolicAddress::CeilF64,
                    ir::LibCall::FloorF32 => bd::SymbolicAddress::FloorF32,
                    ir::LibCall::FloorF64 => bd::SymbolicAddress::FloorF64,
                    ir::LibCall::NearestF32 => bd::SymbolicAddress::NearestF32,
                    ir::LibCall::NearestF64 => bd::SymbolicAddress::NearestF64,
                    ir::LibCall::TruncF32 => bd::SymbolicAddress::TruncF32,
                    ir::LibCall::TruncF64 => bd::SymbolicAddress::TruncF64,
                    _ => {
                        panic!("Don't understand external {}", name);
                    }
                };
                // The symbolic access patch address points *after* the stored pointer.
                let offset = offset + native_pointer_size() as u32;
                self.metadata
                    .push(bd::MetadataEntry::symbolic_access(offset, sym));
            }
            _ => {
                panic!("Don't understand external {}", name);
            }
        }
    }

    fn reloc_jt(&mut self, _offset: CodeOffset, _reloc: Reloc, _jt: ir::JumpTable) {
        unimplemented!();
    }
}

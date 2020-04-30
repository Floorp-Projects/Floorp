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

use log::{debug, info};
use std::fmt;
use std::mem;

use cranelift_codegen::binemit::{
    Addend, CodeInfo, CodeOffset, NullStackmapSink, Reloc, RelocSink, Stackmap, TrapSink,
};
use cranelift_codegen::entity::EntityRef;
use cranelift_codegen::ir::{
    self, constant::ConstantOffset, stackslot::StackSize, ExternalName, JumpTable, SourceLoc,
    TrapCode,
};
use cranelift_codegen::isa::TargetIsa;
use cranelift_codegen::CodegenResult;
use cranelift_codegen::Context;
use cranelift_wasm::{FuncIndex, FuncTranslator, ModuleTranslationState, WasmResult};

use crate::bindings;
use crate::isa::make_isa;
use crate::utils::DashResult;
use crate::wasm2clif::{init_sig, TransEnv, TRAP_THROW_REPORTED};

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

    fn clear(&mut self) {
        self.frame_pushed = 0;
        self.contains_calls = false;
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
pub struct BatchCompiler<'static_env, 'module_env> {
    // Attributes that are constant accross multiple compilations.
    static_environ: &'static_env bindings::StaticEnvironment,
    environ: bindings::ModuleEnvironment<'module_env>,
    isa: Box<dyn TargetIsa>,

    // Stateless attributes.
    func_translator: FuncTranslator,
    dummy_module_state: ModuleTranslationState,

    // Mutable attributes.
    /// Cranelift overall context.
    context: Context,

    /// Temporary storage for trap relocations before they're moved back to the CompiledFunc.
    trap_relocs: Traps,

    /// The translation from wasm to clif environment.
    trans_env: TransEnv<'static_env, 'module_env>,

    /// Results of the current compilation.
    pub current_func: CompiledFunc,
}

impl<'static_env, 'module_env> BatchCompiler<'static_env, 'module_env> {
    pub fn new(
        static_environ: &'static_env bindings::StaticEnvironment,
        environ: bindings::ModuleEnvironment<'module_env>,
    ) -> DashResult<Self> {
        let isa = make_isa(static_environ)?;
        let trans_env = TransEnv::new(&*isa, environ, static_environ);
        Ok(BatchCompiler {
            static_environ,
            environ,
            isa,
            func_translator: FuncTranslator::new(),
            // TODO for Cranelift to support multi-value, feed it the real type section here.
            dummy_module_state: ModuleTranslationState::new(),
            context: Context::new(),
            trap_relocs: Traps::new(),
            trans_env,
            current_func: CompiledFunc::new(),
        })
    }

    /// Clears internal data structures.
    pub fn clear(&mut self) {
        self.context.clear();
        self.trap_relocs.clear();
        self.trans_env.clear();
        self.current_func.clear();
    }

    pub fn compile(&mut self, stackmaps: bindings::Stackmaps) -> CodegenResult<()> {
        let info = self.context.compile(&*self.isa)?;
        debug!("Optimized wasm function IR: {}", self);
        self.binemit(info, stackmaps)
    }

    /// Translate the WebAssembly code to Cranelift IR.
    pub fn translate_wasm(&mut self, func: &bindings::FuncCompileInput) -> WasmResult<()> {
        // Set up the signature before translating the WebAssembly byte code.
        // The translator refers to it.
        let index = FuncIndex::new(func.index as usize);

        self.context.func.signature =
            init_sig(&self.environ, self.static_environ.call_conv(), index)?;
        self.context.func.name = wasm_function_name(index);

        self.func_translator.translate(
            &self.dummy_module_state,
            func.bytecode(),
            func.offset_in_module as usize,
            &mut self.context.func,
            &mut self.trans_env,
        )?;

        info!("Translated wasm function {}.", func.index);
        debug!("Translated wasm function IR: {}", self);
        Ok(())
    }

    /// Emit binary machine code to `emitter`.
    fn binemit(&mut self, info: CodeInfo, stackmaps: bindings::Stackmaps) -> CodegenResult<()> {
        let total_size = info.total_size as usize;
        let frame_pushed = self.frame_pushed();
        let contains_calls = self.contains_calls();

        info!(
            "Emitting {} bytes, frame_pushed={}\n.",
            total_size, frame_pushed
        );

        self.current_func.frame_pushed = frame_pushed;
        self.current_func.contains_calls = contains_calls;

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
            let mut relocs = Relocations::new(
                &mut self.current_func.metadata,
                &mut self.current_func.rodata_relocs,
            );

            let code_buffer = &mut self.current_func.code_buffer;
            unsafe {
                self.context.emit_to_memory(
                    &*self.isa,
                    code_buffer.as_mut_ptr(),
                    &mut relocs,
                    &mut self.trap_relocs,
                    &mut NullStackmapSink {},
                )
            };

            self.current_func
                .metadata
                .append(&mut self.trap_relocs.metadata);
        }

        if self.static_environ.ref_types_enabled {
            if self.context.mach_compile_result.is_some() {
                // TODO Bug 1633721: new backend: support stackmaps.
                log::warn!("new isel backend doesn't support stackmaps yet");
            } else {
                self.emit_stackmaps(stackmaps);
            }
        }

        self.current_func.code_size = info.code_size;
        self.current_func.jumptables_size = info.jumptables_size;
        self.current_func.rodata_size = info.rodata_size;

        Ok(())
    }

    /// Iterate over each instruction to generate a stack map for each instruction that needs it.
    ///
    /// Note a stackmap is associated to the address of the next instruction following the actual
    /// instruction needing the stack map. This is because this is the only information
    /// Spidermonkey has access to when it looks up a stack map (during stack frame iteration).
    fn emit_stackmaps(&self, mut stackmaps: bindings::Stackmaps) {
        let encinfo = self.isa.encoding_info();
        let func = &self.context.func;
        let stack_slots = &func.stack_slots;
        for block in func.layout.blocks() {
            let mut pending_safepoint = None;
            for (offset, inst, inst_size) in func.inst_offsets(block, &encinfo) {
                if let Some(stackmap) = pending_safepoint.take() {
                    stackmaps.add_stackmap(stack_slots, offset + inst_size, stackmap);
                }
                if func.dfg[inst].opcode() == ir::Opcode::Safepoint {
                    let args = func.dfg.inst_args(inst);
                    let stackmap = Stackmap::from_values(&args, func, &*self.isa);
                    pending_safepoint = Some(stackmap);
                }
            }
            debug_assert!(pending_safepoint.is_none());
        }
    }

    /// Compute the `framePushed` argument to pass to `GenerateFunctionPrologue`. This is the
    /// number of frame bytes used by Cranelift, not counting the values pushed by the standard
    /// prologue generated by `GenerateFunctionPrologue`.
    fn frame_pushed(&self) -> StackSize {
        // Cranelift computes the total stack frame size including the pushed return address,
        // standard SM prologue pushes, and its own stack slots.
        let total = if let Some(result) = &self.context.mach_compile_result {
            result.frame_size
        } else {
            self.context
                .func
                .stack_slots
                .layout_info
                .expect("No frame")
                .frame_size
        };

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
}

impl<'static_env, 'module_env> fmt::Display for BatchCompiler<'static_env, 'module_env> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{}", self.context.func.display(self.isa.as_ref()))
    }
}

/// Create a Cranelift function name representing a WebAssembly function with `index`.
pub fn wasm_function_name(func: FuncIndex) -> ExternalName {
    ExternalName::User {
        namespace: USER_FUNCTION_NAMESPACE,
        index: func.index() as u32,
    }
}

/// Create a Cranelift function name representing a builtin function.
pub fn symbolic_function_name(sym: bindings::SymbolicAddress) -> ExternalName {
    ExternalName::User {
        namespace: SYMBOLIC_FUNCTION_NAMESPACE,
        index: sym as u32,
    }
}

struct Relocations<'a> {
    metadata: &'a mut Vec<bindings::MetadataEntry>,
    rodata_relocs: &'a mut Vec<CodeOffset>,
}

impl<'a> Relocations<'a> {
    fn new(
        metadata: &'a mut Vec<bindings::MetadataEntry>,
        rodata_relocs: &'a mut Vec<CodeOffset>,
    ) -> Self {
        Self {
            metadata,
            rodata_relocs,
        }
    }
}

impl<'a> RelocSink for Relocations<'a> {
    /// Add a relocation referencing a block at the current offset.
    fn reloc_block(&mut self, _at: CodeOffset, _reloc: Reloc, _block_offset: CodeOffset) {
        unimplemented!("block relocations NYI");
    }

    /// Add a relocation referencing an external symbol at the current offset.
    fn reloc_external(
        &mut self,
        at: CodeOffset,
        srcloc: SourceLoc,
        reloc: Reloc,
        name: &ExternalName,
        _addend: Addend,
    ) {
        debug_assert!(!srcloc.is_default());

        match *name {
            ExternalName::User {
                namespace: USER_FUNCTION_NAMESPACE,
                index,
            } => {
                // A simple function call to another wasm function.
                let func_index = FuncIndex::new(index as usize);

                // On x86, the Spidermonkey relocation must point to the next instruction.
                // Cranelift gives us the exact offset to the immediate, so fix it up by the
                // relocation's size.
                #[cfg(feature = "cranelift_x86")]
                let offset = at
                    + match reloc {
                        Reloc::X86CallPCRel4 => 4,
                        _ => unreachable!(),
                    };

                // Spidermonkey Aarch64 requires the relocation to point just after the start of
                // the actual relocation, for historical reasons.
                #[cfg(feature = "cranelift_arm64")]
                let offset = match reloc {
                    Reloc::Arm64Call => at + 4,
                    _ => unreachable!(),
                };

                #[cfg(not(any(feature = "cranelift_x86", feature = "cranelift_arm64")))]
                let offset = {
                    // Avoid warning about unused relocation.
                    let _reloc = reloc;
                    at
                };

                self.metadata.push(bindings::MetadataEntry::direct_call(
                    offset, srcloc, func_index,
                ));
            }

            ExternalName::User {
                namespace: SYMBOLIC_FUNCTION_NAMESPACE,
                index,
            } => {
                // This is a symbolic function reference encoded by `symbolic_function_name()`.
                let sym = index.into();

                // See comments about offsets in the User match arm above.

                #[cfg(feature = "cranelift_x86")]
                let offset = at
                    + match reloc {
                        Reloc::Abs8 => 8,
                        _ => unreachable!(),
                    };

                #[cfg(feature = "cranelift_arm64")]
                let offset = match reloc {
                    Reloc::Abs8 => at + 4,
                    _ => unreachable!(),
                };

                #[cfg(not(any(feature = "cranelift_x86", feature = "cranelift_arm64")))]
                let offset = at;

                self.metadata.push(bindings::MetadataEntry::symbolic_access(
                    offset, srcloc, sym,
                ));
            }

            ExternalName::LibCall(call) => {
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

                // The Spidermonkey relocation must point to the next instruction, on x86.
                #[cfg(feature = "cranelift_x86")]
                let offset = at
                    + match reloc {
                        Reloc::Abs8 => 8,
                        _ => unreachable!(),
                    };

                // Spidermonkey AArch64 doesn't expect a relocation offset, in this case.
                #[cfg(feature = "cranelift_arm64")]
                let offset = match reloc {
                    Reloc::Abs8 => at,
                    _ => unreachable!(),
                };

                #[cfg(not(any(feature = "cranelift_x86", feature = "cranelift_arm64")))]
                let offset = at;

                self.metadata.push(bindings::MetadataEntry::symbolic_access(
                    offset, srcloc, sym,
                ));
            }

            _ => {
                panic!("Don't understand external {}", name);
            }
        }
    }

    /// Add a relocation referencing a constant.
    fn reloc_constant(&mut self, _at: CodeOffset, _reloc: Reloc, _const_offset: ConstantOffset) {
        unimplemented!("constant pool relocations NYI");
    }

    /// Add a relocation referencing a jump table.
    fn reloc_jt(&mut self, at: CodeOffset, reloc: Reloc, _jt: JumpTable) {
        match reloc {
            Reloc::X86PCRelRodata4 => {
                self.rodata_relocs.push(at);
            }
            _ => {
                panic!("Unhandled/unexpected reloc type");
            }
        }
    }

    /// Track call sites information, giving us the return address offset.
    fn add_call_site(&mut self, opcode: ir::Opcode, ret_addr: CodeOffset, srcloc: SourceLoc) {
        // Direct calls need a plain relocation, so we don't need to handle them again.
        if opcode == ir::Opcode::CallIndirect {
            self.metadata
                .push(bindings::MetadataEntry::indirect_call(ret_addr, srcloc));
        }
    }
}

struct Traps {
    metadata: Vec<bindings::MetadataEntry>,
}

impl Traps {
    fn new() -> Self {
        Self {
            metadata: Vec::new(),
        }
    }
    fn clear(&mut self) {
        self.metadata.clear();
    }
}

impl TrapSink for Traps {
    /// Add trap information for a specific offset.
    fn trap(&mut self, trap_offset: CodeOffset, loc: SourceLoc, trap: TrapCode) {
        // Translate the trap code into one of BaldrMonkey's trap codes.
        use ir::TrapCode::*;
        let bd_trap = match trap {
            StackOverflow => {
                // Cranelift will give us trap information for every spill/push/call. But
                // Spidermonkey takes care of tracking stack overflows itself in the function
                // entries, so we don't have to.
                return;
            }
            HeapOutOfBounds | OutOfBounds | TableOutOfBounds => bindings::Trap::OutOfBounds,
            IndirectCallToNull => bindings::Trap::IndirectCallToNull,
            BadSignature => bindings::Trap::IndirectCallBadSig,
            IntegerOverflow => bindings::Trap::IntegerOverflow,
            IntegerDivisionByZero => bindings::Trap::IntegerDivideByZero,
            BadConversionToInteger => bindings::Trap::InvalidConversionToInteger,
            Interrupt => bindings::Trap::CheckInterrupt,
            UnreachableCodeReached => bindings::Trap::Unreachable,
            User(x) if x == TRAP_THROW_REPORTED => bindings::Trap::ThrowReported,
            User(_) => panic!("Uncovered trap code {}", trap),
        };

        debug_assert!(!loc.is_default());
        self.metadata
            .push(bindings::MetadataEntry::trap(trap_offset, loc, bd_trap));
    }
}

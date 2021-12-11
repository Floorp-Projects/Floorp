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
use std::rc::Rc;

use cranelift_codegen::binemit::{
    Addend, CodeInfo, CodeOffset, NullStackMapSink, Reloc, RelocSink, TrapSink,
};
use cranelift_codegen::entity::EntityRef;
use cranelift_codegen::ir::{
    self, constant::ConstantOffset, stackslot::StackSize, ExternalName, JumpTable, SourceLoc,
    TrapCode,
};
use cranelift_codegen::isa::TargetIsa;
use cranelift_codegen::machinst::MachStackMap;
use cranelift_codegen::CodegenResult;
use cranelift_codegen::Context;
use cranelift_wasm::wasmparser::{FuncValidator, WasmFeatures};
use cranelift_wasm::{FuncIndex, FuncTranslator, WasmResult};

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
    static_env: &'static_env bindings::StaticEnvironment,

    module_env: Rc<bindings::ModuleEnvironment<'module_env>>,

    isa: Box<dyn TargetIsa>,

    // Stateless attributes.
    func_translator: FuncTranslator,

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
        static_env: &'static_env bindings::StaticEnvironment,
        module_env: bindings::ModuleEnvironment<'module_env>,
    ) -> DashResult<Self> {
        let isa = make_isa(static_env)?;
        let module_env = Rc::new(module_env);
        let trans_env = TransEnv::new(&*isa, module_env.clone(), static_env);
        Ok(BatchCompiler {
            static_env,
            module_env,
            isa,
            func_translator: FuncTranslator::new(),
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
        debug!("=== BatchCompiler::compile: BEGIN ==============================");
        let info = self.context.compile(&*self.isa)?;
        let res = self.binemit(info, stackmaps);
        debug!("=== BatchCompiler::compile: END ================================");
        debug!("");
        res
    }

    /// Translate the WebAssembly code to Cranelift IR.
    pub fn translate_wasm(&mut self, func: &bindings::FuncCompileInput) -> WasmResult<()> {
        // Set up the signature before translating the WebAssembly byte code.
        // The translator refers to it.
        let index = FuncIndex::new(func.index as usize);

        self.context.func.signature =
            init_sig(&*self.module_env, self.static_env.call_conv(), index)?;
        self.context.func.name = wasm_function_name(index);

        let features = WasmFeatures {
            reference_types: self.static_env.ref_types_enabled,
            module_linking: false,
            simd: self.static_env.v128_enabled,
            multi_value: true,
            threads: self.static_env.threads_enabled,
            tail_call: false,
            bulk_memory: true,
            deterministic_only: true,
            memory64: false,
            multi_memory: false,
            exceptions: false,
        };
        let sig_index = self.module_env.func_sig_index(index);
        let mut validator =
            FuncValidator::new(sig_index.index() as u32, 0, &*self.module_env, &features)?;

        self.func_translator.translate(
            &mut validator,
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
            "Emitting {} bytes, frame_pushed={}.",
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
                    &mut NullStackMapSink {},
                )
            };

            self.current_func
                .metadata
                .append(&mut self.trap_relocs.metadata);
        }

        if self.static_env.ref_types_enabled {
            self.emit_stackmaps(stackmaps);
        }

        self.current_func.code_size = info.code_size;
        self.current_func.jumptables_size = info.jumptables_size;
        self.current_func.rodata_size = info.rodata_size;

        Ok(())
    }

    /// Iterate over safepoint information contained in the returned `MachBufferFinalized`.
    fn emit_stackmaps(&self, mut stackmaps: bindings::Stackmaps) {
        let mach_buf = &self.context.mach_compile_result.as_ref().unwrap().buffer;
        let mach_stackmaps = mach_buf.stack_maps();

        for &MachStackMap {
            offset_end,
            ref stack_map,
            ..
        } in mach_stackmaps
        {
            debug!(
                "Stack map at end-of-insn offset {}: {:?}",
                offset_end, stack_map
            );
            stackmaps.add_stackmap(/* inbound_args_size = */ 0, offset_end, stack_map);
        }
    }

    /// Compute the `framePushed` argument to pass to `GenerateFunctionPrologue`. This is the
    /// number of frame bytes used by Cranelift, not counting the values pushed by the standard
    /// prologue generated by `GenerateFunctionPrologue`.
    fn frame_pushed(&self) -> StackSize {
        // Cranelift computes the total stack frame size including the pushed return address,
        // standard SM prologue pushes, and its own stack slots.
        let total = self
            .context
            .mach_compile_result
            .as_ref()
            .expect("always use Mach backend")
            .frame_size;

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
            HeapOutOfBounds | TableOutOfBounds => bindings::Trap::OutOfBounds,
            HeapMisaligned => bindings::Trap::UnalignedAccess,
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

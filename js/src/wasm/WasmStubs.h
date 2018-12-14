/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Copyright 2015 Mozilla Foundation
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

#ifndef wasm_stubs_h
#define wasm_stubs_h

#include "wasm/WasmGenerator.h"

namespace js {
namespace wasm {

extern bool GenerateBuiltinThunk(jit::MacroAssembler& masm,
                                 jit::ABIFunctionType abiType,
                                 ExitReason exitReason, void* funcPtr,
                                 CallableOffsets* offsets);

extern bool GenerateImportFunctions(const ModuleEnvironment& env,
                                    const FuncImportVector& imports,
                                    CompiledCode* code);

extern bool GenerateStubs(const ModuleEnvironment& env,
                          const FuncImportVector& imports,
                          const FuncExportVector& exports, CompiledCode* code);

extern bool GenerateEntryStubs(jit::MacroAssembler& masm,
                               size_t funcExportIndex,
                               const FuncExport& funcExport,
                               const Maybe<jit::ImmPtr>& callee, bool isAsmJS,
                               HasGcTypes gcTypesConfigured,
                               CodeRangeVector* codeRanges);

extern void GenerateTrapExitMachineState(jit::MachineState* machine,
                                         size_t* numWords);

// A value that is written into the trap exit frame, which is useful for
// cross-checking during garbage collection.
static constexpr uintptr_t TrapExitDummyValue = 1337;

// And its offset, in words, down from the highest-addressed word of the trap
// exit frame.  The value is written into the frame using WasmPush.  In the
// case where WasmPush allocates more than one word, the value will therefore
// be written at the lowest-addressed word.
#ifdef JS_CODEGEN_ARM64
static constexpr size_t TrapExitDummyValueOffsetFromTop = 1;
#else
static constexpr size_t TrapExitDummyValueOffsetFromTop = 0;
#endif

// An argument that will end up on the stack according to the system ABI, to be
// passed to GenerateDirectCallFromJit. Since the direct JIT call creates its
// own frame, it is its responsibility to put stack arguments to their expected
// locations; so the caller of GenerateDirectCallFromJit can put them anywhere.

class JitCallStackArg {
 public:
  enum class Tag {
    Imm32,
    GPR,
    FPU,
    Address,
    Undefined,
  };

 private:
  Tag tag_;
  union U {
    int32_t imm32_;
    jit::Register gpr_;
    jit::FloatRegister fpu_;
    jit::Address addr_;
    U() {}
  } arg;

 public:
  JitCallStackArg() : tag_(Tag::Undefined) {}
  explicit JitCallStackArg(int32_t imm32) : tag_(Tag::Imm32) {
    arg.imm32_ = imm32;
  }
  explicit JitCallStackArg(jit::Register gpr) : tag_(Tag::GPR) {
    arg.gpr_ = gpr;
  }
  explicit JitCallStackArg(jit::FloatRegister fpu) : tag_(Tag::FPU) {
    new (&arg) jit::FloatRegister(fpu);
  }
  explicit JitCallStackArg(const jit::Address& addr) : tag_(Tag::Address) {
    new (&arg) jit::Address(addr);
  }

  Tag tag() const { return tag_; }
  int32_t imm32() const {
    MOZ_ASSERT(tag_ == Tag::Imm32);
    return arg.imm32_;
  }
  jit::Register gpr() const {
    MOZ_ASSERT(tag_ == Tag::GPR);
    return arg.gpr_;
  }
  jit::FloatRegister fpu() const {
    MOZ_ASSERT(tag_ == Tag::FPU);
    return arg.fpu_;
  }
  const jit::Address& addr() const {
    MOZ_ASSERT(tag_ == Tag::Address);
    return arg.addr_;
  }
};

using JitCallStackArgVector = Vector<JitCallStackArg, 4, SystemAllocPolicy>;

// Generates an inline wasm call (during jit compilation) to a specific wasm
// function (as specifed by the given FuncExport).
// This call doesn't go through a wasm entry, but rather creates its own
// inlined exit frame.
// Assumes:
// - all the registers have been preserved by the caller,
// - all arguments passed in registers have been set up at the expected
//   locations,
// - all arguments passed on stack slot are alive as defined by a corresponding
//   JitCallStackArg.

extern void GenerateDirectCallFromJit(
    jit::MacroAssembler& masm, const FuncExport& fe, const Instance& inst,
    const JitCallStackArgVector& stackArgs, bool profilingEnabled,
    bool wasmGcEnabled, jit::Register scratch, uint32_t* callOffset);

}  // namespace wasm
}  // namespace js

#endif  // wasm_stubs_h

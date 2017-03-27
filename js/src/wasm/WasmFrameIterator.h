/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright 2014 Mozilla Foundation
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

#ifndef wasm_frame_iterator_h
#define wasm_frame_iterator_h

#include "js/ProfilingFrameIterator.h"

class JSAtom;

namespace js {

class WasmActivation;
namespace jit { class MacroAssembler; }

namespace wasm {

class CallSite;
class Code;
class CodeRange;
class DebugFrame;
class Instance;
class SigIdDesc;
struct FuncOffsets;
struct CallableOffsets;
struct TrapOffset;

// Iterates over the frames of a single WasmActivation, called synchronously
// from C++ in the thread of the asm.js.
//
// The one exception is that this iterator may be called from the interrupt
// callback which may be called asynchronously from asm.js code; in this case,
// the backtrace may not be correct. That being said, we try our best printing
// an informative message to the user and at least the name of the innermost
// function stack frame.
class FrameIterator
{
  public:
    enum class Unwind { True, False };

  private:
    WasmActivation* activation_;
    const Code* code_;
    const CallSite* callsite_;
    const CodeRange* codeRange_;
    uint8_t* fp_;
    Unwind unwind_;
    bool missingFrameMessage_;

    void settle();

  public:
    explicit FrameIterator();
    explicit FrameIterator(WasmActivation* activation, Unwind unwind = Unwind::False);
    void operator++();
    bool done() const;
    const char* filename() const;
    const char16_t* displayURL() const;
    bool mutedErrors() const;
    JSAtom* functionDisplayAtom() const;
    unsigned lineOrBytecode() const;
    const CodeRange* codeRange() const { return codeRange_; }
    bool hasInstance() const;
    Instance* instance() const;
    bool debugEnabled() const;
    DebugFrame* debugFrame() const;
    const CallSite* debugTrapCallsite() const;
};

// An ExitReason describes the possible reasons for leaving compiled wasm code
// or the state of not having left compiled wasm code (ExitReason::None).
enum class ExitReason : uint32_t
{
    None,          // default state, the pc is in wasm code
    ImportJit,     // fast-path call directly into JIT code
    ImportInterp,  // slow-path call into C++ Invoke()
    Trap,          // call to trap handler for the trap in WasmActivation::trap
    DebugTrap      // call to debug trap handler
};

// Iterates over the frames of a single WasmActivation, given an
// asynchronously-interrupted thread's state.
class ProfilingFrameIterator
{
    const WasmActivation* activation_;
    const Code* code_;
    const CodeRange* codeRange_;
    void* callerFP_;
    void* callerPC_;
    void* stackAddress_;
    ExitReason exitReason_;

    void initFromExitFP();

  public:
    ProfilingFrameIterator();
    explicit ProfilingFrameIterator(const WasmActivation& activation);
    ProfilingFrameIterator(const WasmActivation& activation,
                           const JS::ProfilingFrameIterator::RegisterState& state);
    void operator++();
    bool done() const { return !codeRange_; }

    void* stackAddress() const { MOZ_ASSERT(!done()); return stackAddress_; }
    const char* label() const;
};

// Prologue/epilogue code generation

void
GenerateExitPrologue(jit::MacroAssembler& masm, unsigned framePushed, ExitReason reason,
                     CallableOffsets* offsets);
void
GenerateExitEpilogue(jit::MacroAssembler& masm, unsigned framePushed, ExitReason reason,
                     CallableOffsets* offsets);
void
GenerateFunctionPrologue(jit::MacroAssembler& masm, unsigned framePushed, const SigIdDesc& sigId,
                         FuncOffsets* offsets);
void
GenerateFunctionEpilogue(jit::MacroAssembler& masm, unsigned framePushed, FuncOffsets* offsets);

// Mark all instance objects live on the stack.

void
TraceActivations(JSContext* cx, const CooperatingContext& target, JSTracer* trc);

// Given a fault at pc with register fp, return the faulting instance if there
// is such a plausible instance, and otherwise null.

Instance*
LookupFaultingInstance(WasmActivation* activation, void* pc, void* fp);

} // namespace wasm
} // namespace js

#endif // wasm_frame_iterator_h

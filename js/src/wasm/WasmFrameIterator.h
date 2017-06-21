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
class DebugState;
class Instance;
class SigIdDesc;
struct Frame;
struct FuncOffsets;
struct CallableOffsets;

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
    Frame* fp_;
    Unwind unwind_;
    void** unwoundAddressOfReturnAddress_;

    void popFrame();

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
    Instance* instance() const;
    void** unwoundAddressOfReturnAddress() const;
    bool debugEnabled() const;
    DebugFrame* debugFrame() const;
    const CallSite* debugTrapCallsite() const;
};

enum class SymbolicAddress;

// An ExitReason describes the possible reasons for leaving compiled wasm
// code or the state of not having left compiled wasm code
// (ExitReason::None). It is either a known reason, or a enumeration to a native
// function that is used for better display in the profiler.
class ExitReason
{
    uint32_t payload_;

    ExitReason() {}

  public:
    enum class Fixed : uint32_t
    {
        None,          // default state, the pc is in wasm code
        ImportJit,     // fast-path call directly into JIT code
        ImportInterp,  // slow-path call into C++ Invoke()
        BuiltinNative, // fast-path call directly into native C++ code
        Trap,          // call to trap handler for the trap in WasmActivation::trap
        DebugTrap      // call to debug trap handler
    };

    MOZ_IMPLICIT ExitReason(Fixed exitReason)
      : payload_(0x0 | (uint32_t(exitReason) << 1))
    {
        MOZ_ASSERT(isFixed());
    }

    explicit ExitReason(SymbolicAddress sym)
      : payload_(0x1 | (uint32_t(sym) << 1))
    {
        MOZ_ASSERT(uint32_t(sym) <= (UINT32_MAX << 1), "packing constraints");
        MOZ_ASSERT(!isFixed());
    }

    static ExitReason Decode(uint32_t payload) {
        ExitReason reason;
        reason.payload_ = payload;
        return reason;
    }

    static ExitReason None() { return ExitReason(ExitReason::Fixed::None); }

    bool isFixed() const { return (payload_ & 0x1) == 0; }
    bool isNone() const { return isFixed() && fixed() == Fixed::None; }
    bool isNative() const { return !isFixed() || fixed() == Fixed::BuiltinNative; }

    uint32_t encode() const {
        return payload_;
    }
    Fixed fixed() const {
        MOZ_ASSERT(isFixed());
        return Fixed(payload_ >> 1);
    }
    SymbolicAddress symbolic() const {
        MOZ_ASSERT(!isFixed());
        return SymbolicAddress(payload_ >> 1);
    }
};

// Iterates over the frames of a single WasmActivation, given an
// asynchronously-interrupted thread's state.
class ProfilingFrameIterator
{
    const WasmActivation* activation_;
    const Code* code_;
    const CodeRange* codeRange_;
    Frame* callerFP_;
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

// If the innermost (active) Activation is a WasmActivation, return it.

WasmActivation*
ActivationIfInnermost(JSContext* cx);

// Return whether the given PC is in wasm code.

bool
InCompiledCode(void* pc);

// Describes register state and associated code at a given call frame.

struct UnwindState
{
    Frame* fp;
    void* pc;
    const Code* code;
    const CodeRange* codeRange;
    UnwindState() : fp(nullptr), pc(nullptr), code(nullptr), codeRange(nullptr) {}
};

typedef JS::ProfilingFrameIterator::RegisterState RegisterState;

// Ensures the register state at a call site is consistent: pc must be in the
// code range of the code described by fp. This prevents issues when using
// the values of pc/fp, especially at call sites boundaries, where the state
// hasn't fully transitioned from the caller's to the callee's.
//
// unwoundCaller is set to true if we were in a transitional state and had to
// rewind to the caller's frame instead of the current frame.
//
// Returns true if it was possible to get to a clear state, or false if the
// frame should be ignored.

bool
StartUnwinding(const WasmActivation& activation, const RegisterState& registers,
               UnwindState* unwindState, bool* unwoundCaller);

} // namespace wasm
} // namespace js

#endif // wasm_frame_iterator_h

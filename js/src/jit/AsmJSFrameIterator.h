/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_AsmJSFrameIterator_h
#define jit_AsmJSFrameIterator_h

#include "mozilla/NullPtr.h"

#include <stdint.h>

#include "js/ProfilingFrameIterator.h"

class JSAtom;
struct JSContext;

namespace js {

class AsmJSActivation;
class AsmJSModule;
struct AsmJSFunctionLabels;
namespace jit { class CallSite; class MacroAssembler; class Label; }

// Iterates over the frames of a single AsmJSActivation, called synchronously
// from C++ in the thread of the asm.js. The one exception is that this iterator
// may be called from the interrupt callback which may be called asynchronously
// from asm.js code; in this case, the backtrace may not be correct.
class AsmJSFrameIterator
{
    const AsmJSModule *module_;
    const jit::CallSite *callsite_;
    uint8_t *fp_;

    // Really, a const AsmJSModule::CodeRange*, but no forward declarations of
    // nested classes, so use void* to avoid pulling in all of AsmJSModule.h.
    const void *codeRange_;

    void settle();

  public:
    explicit AsmJSFrameIterator() : module_(nullptr) {}
    explicit AsmJSFrameIterator(const AsmJSActivation &activation);
    void operator++();
    bool done() const { return !fp_; }
    JSAtom *functionDisplayAtom() const;
    unsigned computeLine(uint32_t *column) const;
};

// List of reasons for execution leaving asm.js-generated code, stored in
// AsmJSActivation. The initial and default state is AsmJSNoExit. If AsmJSNoExit
// is observed when the pc isn't in asm.js code, execution must have been
// interrupted asynchronously (viz., by a exception/signal handler).
enum AsmJSExitReason
{
    AsmJSNoExit,
    AsmJSFFI,
    AsmJSInterrupt
};

// Iterates over the frames of a single AsmJSActivation, given an
// asynchrously-interrupted thread's state. If the activation's
// module is not in profiling mode, the activation is skipped.
class AsmJSProfilingFrameIterator
{
    const AsmJSModule *module_;
    uint8_t *callerFP_;
    void *callerPC_;
    AsmJSExitReason exitReason_;

    // Really, a const AsmJSModule::CodeRange*, but no forward declarations of
    // nested classes, so use void* to avoid pulling in all of AsmJSModule.h.
    const void *codeRange_;

    void initFromFP(const AsmJSActivation &activation);

  public:
    AsmJSProfilingFrameIterator() : codeRange_(nullptr) {}
    AsmJSProfilingFrameIterator(const AsmJSActivation &activation);
    AsmJSProfilingFrameIterator(const AsmJSActivation &activation,
                                const JS::ProfilingFrameIterator::RegisterState &state);
    void operator++();
    bool done() const { return !codeRange_; }

    typedef JS::ProfilingFrameIterator::Kind Kind;
    Kind kind() const;

    JSAtom *functionDisplayAtom() const;
    const char *functionFilename() const;
    unsigned functionLine() const;

    const char *nonFunctionDescription() const;
};

/******************************************************************************/
// Prologue/epilogue code generation.

void
GenerateAsmJSFunctionPrologue(jit::MacroAssembler &masm, unsigned framePushed,
                              AsmJSFunctionLabels *labels);
void
GenerateAsmJSFunctionEpilogue(jit::MacroAssembler &masm, unsigned framePushed,
                              AsmJSFunctionLabels *labels);
void
GenerateAsmJSStackOverflowExit(jit::MacroAssembler &masm, jit::Label *overflowExit,
                               jit::Label *throwLabel);

void
GenerateAsmJSEntryPrologue(jit::MacroAssembler &masm, jit::Label *begin);
void
GenerateAsmJSEntryEpilogue(jit::MacroAssembler &masm);

void
GenerateAsmJSExitPrologue(jit::MacroAssembler &masm, unsigned framePushed, AsmJSExitReason reason,
                          jit::Label *begin);
void
GenerateAsmJSExitEpilogue(jit::MacroAssembler &masm, unsigned framePushed, AsmJSExitReason reason,
                          jit::Label *profilingReturn);

} // namespace js

#endif // jit_AsmJSFrameIterator_h

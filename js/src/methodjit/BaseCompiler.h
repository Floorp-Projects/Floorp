/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined jsjaeger_compilerbase_h__ && defined JS_METHODJIT
#define jsjaeger_compilerbase_h__

#include "jscntxt.h"
#include "assembler/assembler/MacroAssembler.h"
#include "assembler/assembler/LinkBuffer.h"
#include "assembler/assembler/RepatchBuffer.h"
#include "assembler/jit/ExecutableAllocator.h"
#include <limits.h>

#if defined JS_CPU_ARM
# define POST_INST_OFFSET(__expr) ((__expr) - sizeof(ARMWord))
#else
# define POST_INST_OFFSET(__expr) (__expr)
#endif

namespace js {
namespace mjit {

struct MacroAssemblerTypedefs {
    typedef JSC::MacroAssembler::Label Label;
    typedef JSC::MacroAssembler::Imm32 Imm32;
    typedef JSC::MacroAssembler::ImmPtr ImmPtr;
    typedef JSC::MacroAssembler::RegisterID RegisterID;
    typedef JSC::MacroAssembler::FPRegisterID FPRegisterID;
    typedef JSC::MacroAssembler::Address Address;
    typedef JSC::MacroAssembler::BaseIndex BaseIndex;
    typedef JSC::MacroAssembler::AbsoluteAddress AbsoluteAddress;
    typedef JSC::MacroAssembler MacroAssembler;
    typedef JSC::MacroAssembler::Jump Jump;
    typedef JSC::MacroAssembler::JumpList JumpList;
    typedef JSC::MacroAssembler::Call Call;
    typedef JSC::MacroAssembler::DataLabelPtr DataLabelPtr;
    typedef JSC::MacroAssembler::DataLabel32 DataLabel32;
    typedef JSC::FunctionPtr FunctionPtr;
    typedef JSC::RepatchBuffer RepatchBuffer;
    typedef JSC::CodeLocationLabel CodeLocationLabel;
    typedef JSC::CodeLocationDataLabel32 CodeLocationDataLabel32;
    typedef JSC::CodeLocationDataLabelPtr CodeLocationDataLabelPtr;
    typedef JSC::CodeLocationJump CodeLocationJump;
    typedef JSC::CodeLocationCall CodeLocationCall;
    typedef JSC::CodeLocationInstruction CodeLocationInstruction;
    typedef JSC::ReturnAddressPtr ReturnAddressPtr;
    typedef JSC::MacroAssemblerCodePtr MacroAssemblerCodePtr;
    typedef JSC::JITCode JITCode;
#if defined JS_CPU_ARM
    typedef JSC::ARMWord ARMWord;
#endif
};

class BaseCompiler : public MacroAssemblerTypedefs
{
  protected:
    JSContext *cx;

  public:
    BaseCompiler() : cx(NULL)
    { }

    BaseCompiler(JSContext *cx) : cx(cx)
    { }
};

#ifdef JS_CPU_X64
inline bool
VerifyRange(void *start1, size_t size1, void *start2, size_t size2)
{
    uintptr_t end1 = uintptr_t(start1) + size1;
    uintptr_t end2 = uintptr_t(start2) + size2;

    uintptr_t lowest = Min(uintptr_t(start1), uintptr_t(start2));
    uintptr_t highest = Max(end1, end2);

    return (highest - lowest < INT_MAX);
}
#endif

// This class wraps JSC::LinkBuffer for Mozilla-specific memory handling.
// Every return |false| guarantees an OOM that has been correctly propagated,
// and should continue to propagate.
class LinkerHelper : public JSC::LinkBuffer
{
  protected:
    Assembler &masm;
#ifdef DEBUG
    bool verifiedRange;
#endif

  public:
    LinkerHelper(Assembler &masm, JSC::CodeKind kind) : JSC::LinkBuffer(kind)
        , masm(masm)
#ifdef DEBUG
        , verifiedRange(false)
#endif
    { }

    ~LinkerHelper() {
        JS_ASSERT(verifiedRange);
    }

    bool verifyRange(const JSC::JITCode &other) {
        markVerified();
#ifdef JS_CPU_X64
        return VerifyRange(m_code, m_size, other.start(), other.size());
#else
        return true;
#endif
    }

    bool verifyRange(JITChunk *chunk) {
        return verifyRange(JSC::JITCode(chunk->code.m_code.executableAddress(),
                                        chunk->code.m_size));
    }

    JSC::ExecutablePool *init(JSContext *cx) {
        // The pool is incref'd after this call, so it's necessary to release()
        // on any failure.
        JSC::ExecutableAllocator *allocator = &cx->runtime->execAlloc();
        allocator->setDestroyCallback(Probes::discardExecutableRegion);
        JSC::ExecutablePool *pool;
        m_code = executableAllocAndCopy(masm, allocator, &pool);
        if (!m_code) {
            markVerified();
            js_ReportOutOfMemory(cx);
            return NULL;
        }
        m_size = masm.size();   // must come after call to executableAllocAndCopy()!
        return pool;
    }

    JSC::CodeLocationLabel finalize(VMFrame &f) {
        masm.finalize(*this);
        JSC::CodeLocationLabel label = finalizeCodeAddendum();
        Probes::registerICCode(f.cx, f.chunk(), f.script(), f.pc(),
                               label.executableAddress(), masm.size());
        return label;
    }

    void maybeLink(MaybeJump jump, JSC::CodeLocationLabel label) {
        if (!jump.isSet())
            return;
        link(jump.get(), label);
    }

    size_t size() const {
        return m_size;
    }

  protected:
    void markVerified() {
#ifdef DEBUG
        verifiedRange = true;
#endif
    }
};

class NativeStubLinker : public LinkerHelper
{
  public:
#ifdef JS_CPU_X64
    typedef JSC::MacroAssembler::DataLabelPtr FinalJump;
#else
    typedef JSC::MacroAssembler::Jump FinalJump;
#endif

    NativeStubLinker(Assembler &masm, JITChunk *chunk, jsbytecode *pc, FinalJump done)
        : LinkerHelper(masm, JSC::JAEGER_CODE), chunk(chunk), pc(pc), done(done)
    {}

    bool init(JSContext *cx);

    void patchJump(JSC::CodeLocationLabel target) {
#ifdef JS_CPU_X64
        patch(done, target);
#else
        link(done, target);
#endif
    }

  private:
    JITChunk *chunk;
    jsbytecode *pc;
    FinalJump done;
};

bool
NativeStubEpilogue(VMFrame &f, Assembler &masm, NativeStubLinker::FinalJump *result,
                   int32_t initialFrameDepth, int32_t vpOffset,
                   MaybeRegisterID typeReg, MaybeRegisterID dataReg);

/*
 * On ARM, we periodically flush a constant pool into the instruction stream
 * where constants are found using PC-relative addressing. This is necessary
 * because the fixed-width instruction set doesn't support wide immediates.
 *
 * ICs perform repatching on the inline (fast) path by knowing small and
 * generally fixed code location offset values where the patchable instructions
 * live. Dumping a huge constant pool into the middle of an IC's inline path
 * makes the distance between emitted instructions potentially variable and/or
 * large, which makes the IC offsets invalid. We must reserve contiguous space
 * up front to prevent this from happening.
 */
#ifdef JS_CPU_ARM
template <size_t reservedSpace>
class AutoReserveICSpace {
    typedef Assembler::Label Label;

    Assembler           &masm;
    bool                didCheck;
    bool                *overflowSpace;
    int                 flushCount;

  public:
    AutoReserveICSpace(Assembler &masm, bool *overflowSpace)
        : masm(masm), didCheck(false), overflowSpace(overflowSpace)
    {
        masm.ensureSpace(reservedSpace);
        flushCount = masm.flushCount();
    }

    /* Allow manual IC space checks so that non-patchable code at the end of an IC section can be
     * free to use constant pools. */
    void check() {
        JS_ASSERT(!didCheck);
        didCheck = true;

        if (masm.flushCount() != flushCount)
            *overflowSpace = true;
    }

    ~AutoReserveICSpace() {
        /* Automatically check the IC space if we didn't already do it manually. */
        if (!didCheck) {
            check();
        }
    }
};

# define RESERVE_IC_SPACE(__masm)       AutoReserveICSpace<256> arics(__masm, &this->overflowICSpace)
# define CHECK_IC_SPACE()               arics.check()

/* The OOL path can need a lot of space because we save and restore a lot of registers. The actual
 * sequene varies. However, dumping the literal pool before an OOL block is probably a good idea
 * anyway, as we branch directly to the start of the block from the fast path. */
# define RESERVE_OOL_SPACE(__masm)      AutoReserveICSpace<2048> arics_ool(__masm, &this->overflowICSpace)

/* Allow the OOL patch to be checked before object destruction. Often, non-patchable epilogues or
 * rejoining sequences are emitted, and it isn't necessary to protect these from literal pools. */
# define CHECK_OOL_SPACE()              arics_ool.check()
#else
# define RESERVE_IC_SPACE(__masm)       /* Do nothing. */
# define CHECK_IC_SPACE()               /* Do nothing. */
# define RESERVE_OOL_SPACE(__masm)      /* Do nothing. */
# define CHECK_OOL_SPACE()              /* Do nothing. */
#endif

} /* namespace js */
} /* namespace mjit */

#endif

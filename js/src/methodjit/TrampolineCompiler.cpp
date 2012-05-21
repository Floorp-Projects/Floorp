/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TrampolineCompiler.h"
#include "StubCalls.h"
#include "assembler/assembler/LinkBuffer.h"
#include "assembler/jit/ExecutableAllocator.h"

namespace js {
namespace mjit {

#define CHECK_RESULT(x) if (!(x)) return false
#define COMPILE(which, pool, how) CHECK_RESULT(compileTrampoline(&(which), &pool, how))
#define RELEASE(which, pool) JS_BEGIN_MACRO \
    which = NULL;                           \
    if (pool)                               \
        pool->release();                    \
    pool = NULL;                            \
JS_END_MACRO

typedef JSC::MacroAssembler::Address Address;
typedef JSC::MacroAssembler::Label Label;
typedef JSC::MacroAssembler::Jump Jump;
typedef JSC::MacroAssembler::ImmPtr ImmPtr;
typedef JSC::MacroAssembler::Imm32 Imm32;
typedef JSC::MacroAssembler::Address Address;

bool
TrampolineCompiler::compile()
{
#ifdef JS_METHODJIT_SPEW
    JMCheckLogging();
#endif

    COMPILE(trampolines->forceReturn, trampolines->forceReturnPool, generateForceReturn);
#if (defined(JS_NO_FASTCALL) && defined(JS_CPU_X86)) || defined(_WIN64)
    COMPILE(trampolines->forceReturnFast, trampolines->forceReturnFastPool, generateForceReturnFast);
#endif

    return true;
}

void
TrampolineCompiler::release(Trampolines *tramps)
{
    RELEASE(tramps->forceReturn, tramps->forceReturnPool);
#if (defined(JS_NO_FASTCALL) && defined(JS_CPU_X86)) || defined(_WIN64)
    RELEASE(tramps->forceReturnFast, tramps->forceReturnFastPool);
#endif
}

bool
TrampolineCompiler::compileTrampoline(Trampolines::TrampolinePtr *where,
                                      JSC::ExecutablePool **poolp, TrampolineGenerator generator)
{
    Assembler masm;

    Label entry = masm.label();
    CHECK_RESULT(generator(masm));
    JS_ASSERT(entry.isSet());

    bool ok;
    JSC::LinkBuffer buffer(&masm, execAlloc, poolp, &ok, JSC::METHOD_CODE);
    if (!ok)
        return false;
    masm.finalize(buffer);
    uint8_t *result = (uint8_t*)buffer.finalizeCodeAddendum().dataLocation();
    *where = JS_DATA_TO_FUNC_PTR(Trampolines::TrampolinePtr, result + masm.distanceOf(entry));

    return true;
}

/*
 * This is shamelessly copied from emitReturn, but with several changes:
 * - There was always at least one inline call.
 * - We don't know if there are activation objects or a script with nesting
 *   state whose active frames need adjustment, so we always stub the epilogue.
 * - We don't know where we came from, so we don't know frame depth or PC.
 * - There is no stub buffer.
 */
bool
TrampolineCompiler::generateForceReturn(Assembler &masm)
{
    /* The JSStackFrame register may have been clobbered while returning, reload it. */
    masm.loadPtr(FrameAddress(VMFrame::offsetOfFp), JSFrameReg);

    /* Perform the frame epilogue. */
    masm.fallibleVMCall(true, JS_FUNC_TO_DATA_PTR(void *, stubs::AnyFrameEpilogue), NULL, NULL, 0);

    /* Store any known return value */
    masm.loadValueAsComponents(UndefinedValue(), JSReturnReg_Type, JSReturnReg_Data);
    Jump rvalClear = masm.branchTest32(Assembler::Zero,
                                       FrameFlagsAddress(), Imm32(StackFrame::HAS_RVAL));
    Address rvalAddress(JSFrameReg, StackFrame::offsetOfReturnValue());
    masm.loadValueAsComponents(rvalAddress, JSReturnReg_Type, JSReturnReg_Data);
    rvalClear.linkTo(masm.label(), &masm);

    /* Return to the caller */
    masm.loadPtr(Address(JSFrameReg, StackFrame::offsetOfNcode()), Registers::ReturnReg);
    masm.jump(Registers::ReturnReg);
    return true;
}

#if (defined(JS_NO_FASTCALL) && defined(JS_CPU_X86)) || defined(_WIN64)
bool
TrampolineCompiler::generateForceReturnFast(Assembler &masm)
{
#ifdef _WIN64
    masm.addPtr(Imm32(32), Registers::StackPointer);
#else
    // In case of no fast call, when we change the return address,
    // we need to make sure add esp by 8.
    masm.addPtr(Imm32(16), Registers::StackPointer);
#endif
    return generateForceReturn(masm);
}
#endif

} /* namespace mjit */
} /* namespace js */


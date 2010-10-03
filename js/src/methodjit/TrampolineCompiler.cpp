/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Jaegermonkey.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 *
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Andrew Drake <drakedevel@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "TrampolineCompiler.h"
#include "StubCalls.h"
#include "assembler/assembler/LinkBuffer.h"

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
TrampolineCompiler::compileTrampoline(Trampolines::TrampolinePtr *where, JSC::ExecutablePool **pool,
                                      TrampolineGenerator generator)
{
    Assembler masm;

    Label entry = masm.label();
    CHECK_RESULT(generator(masm));
    JS_ASSERT(entry.isValid());

    *pool = execPool->poolForSize(masm.size());
    if (!*pool)
        return false;

    JSC::LinkBuffer buffer(&masm, *pool);
    masm.finalize(buffer);
    uint8 *result = (uint8*)buffer.finalizeCodeAddendum().dataLocation();
    *where = JS_DATA_TO_FUNC_PTR(Trampolines::TrampolinePtr, result + masm.distanceOf(entry));

    return true;
}

/*
 * This is shamelessly copied from emitReturn, but with several changes:
 * - There was always at least one inline call.
 * - We don't know if there is a call object, so we always check.
 * - We don't know where we came from, so we don't know frame depth or PC.
 * - There is no stub buffer.
 */
bool
TrampolineCompiler::generateForceReturn(Assembler &masm)
{
    /* if (hasArgsObj() || hasCallObj()) stubs::PutActivationObjects() */
    Jump noActObjs = masm.branchTest32(Assembler::Zero, FrameFlagsAddress(),
                                       Imm32(JSFRAME_HAS_CALL_OBJ | JSFRAME_HAS_ARGS_OBJ));
    masm.stubCall(stubs::PutActivationObjects, NULL, 0);
    noActObjs.linkTo(masm.label(), &masm);

    /* Store any known return value */
    masm.loadValueAsComponents(UndefinedValue(), JSReturnReg_Type, JSReturnReg_Data);
    Jump rvalClear = masm.branchTest32(Assembler::Zero,
                                       FrameFlagsAddress(), Imm32(JSFRAME_HAS_RVAL));
    Address rvalAddress(JSFrameReg, JSStackFrame::offsetOfReturnValue());
    masm.loadValueAsComponents(rvalAddress, JSReturnReg_Type, JSReturnReg_Data);
    rvalClear.linkTo(masm.label(), &masm);

    /* Return to the caller */
    masm.loadPtr(Address(JSFrameReg, JSStackFrame::offsetOfncode()), Registers::ReturnReg);
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
    masm.addPtr(Imm32(8), Registers::StackPointer);
#endif
    return generateForceReturn(masm);
}
#endif

} /* namespace mjit */
} /* namespace js */


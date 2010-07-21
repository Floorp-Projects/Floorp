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

#if defined JS_METHODJIT

#include "TrampolineCompiler.h"
#include "StubCalls.h"
#include "assembler/assembler/LinkBuffer.h"

namespace js {
namespace mjit {

#define CHECK_RESULT(x) if (!(x)) return false
#define COMPILE(which, pool, how) CHECK_RESULT(compileTrampoline((void **)(&(which)), &pool, how))
#define RELEASE(which, pool) JS_BEGIN_MACRO \
    which = NULL;                           \
    if (pool)                               \
        pool->release();                    \
    pool = NULL;                            \
JS_END_MACRO

#ifdef JS_CPU_X86
static const JSC::MacroAssembler::RegisterID JSReturnReg_Type = JSC::X86Registers::ecx;
static const JSC::MacroAssembler::RegisterID JSReturnReg_Data = JSC::X86Registers::edx;
#elif defined(JS_CPU_ARM)
static const JSC::MacroAssembler::RegisterID JSReturnReg_Type = JSC::ARMRegisters::r2;
static const JSC::MacroAssembler::RegisterID JSReturnReg_Data = JSC::ARMRegisters::r1;
#endif

bool
TrampolineCompiler::compile()
{
#ifdef JS_METHODJIT_SPEW
    JMCheckLogging();
#endif

    COMPILE(trampolines->forceReturn, trampolines->forceReturnPool, generateForceReturn);

    return true;
}

void
TrampolineCompiler::release(Trampolines *tramps)
{
    RELEASE(tramps->forceReturn, tramps->forceReturnPool);
}

bool
TrampolineCompiler::compileTrampoline(void **where, JSC::ExecutablePool **pool,
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
    uint8 *result = (uint8*)buffer.finalizeCodeAddendum().dataLocation();
    masm.finalize(result);
    *where = result + masm.distanceOf(entry);

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
    /* if (!callobj) stubs::PutCallObject */
    Jump noCallObj = masm.branchPtr(Assembler::Equal,
                                    Address(JSFrameReg, offsetof(JSStackFrame, callobj)),
                                    ImmPtr(0));
    masm.stubCall(stubs::PutCallObject, NULL, 0);
    noCallObj.linkTo(masm.label(), &masm);

    /* if (arguments) stubs::PutArgsObject */
    Jump noArgsObj = masm.branchPtr(Assembler::Equal,
                                    Address(JSFrameReg, offsetof(JSStackFrame, argsobj)),
                                    ImmIntPtr(0));
    masm.stubCall(stubs::PutArgsObject, NULL, 0);
    noArgsObj.linkTo(masm.label(), &masm);

    /*
     * r = fp->down
     * a1 = f.cx
     * f.fp = r
     * cx->fp = r
     */
    masm.loadPtr(Address(JSFrameReg, offsetof(JSStackFrame, down)), Registers::ReturnReg);
    masm.loadPtr(FrameAddress(offsetof(VMFrame, cx)), Registers::ArgReg1);
    masm.storePtr(Registers::ReturnReg, FrameAddress(offsetof(VMFrame, fp)));
    masm.storePtr(Registers::ReturnReg, Address(Registers::ArgReg1, offsetof(JSContext, fp)));
    masm.subPtr(ImmIntPtr(1), FrameAddress(offsetof(VMFrame, inlineCallCount)));

    Address rval(JSFrameReg, offsetof(JSStackFrame, rval));
    masm.load32(masm.payloadOf(rval), JSReturnReg_Data);
    masm.load32(masm.tagOf(rval), JSReturnReg_Type);
    masm.move(Registers::ReturnReg, JSFrameReg);
    masm.loadPtr(Address(JSFrameReg, offsetof(JSStackFrame, ncode)), Registers::ReturnReg);
#ifdef DEBUG
    masm.storePtr(ImmPtr(JSStackFrame::sInvalidPC),
                  Address(JSFrameReg, offsetof(JSStackFrame, savedPC)));
#endif

#if defined(JS_CPU_ARM)
    masm.loadPtr(FrameAddress(offsetof(VMFrame, scriptedReturn)), JSC::ARMRegisters::lr);
#endif

    masm.ret();
    return true;
}

} /* mjit */
} /* js */

#endif /* JS_METHOJIT */


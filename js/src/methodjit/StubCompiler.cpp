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
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Brendan Eich <brendan@mozilla.org>
 *
 * Contributor(s):
 *   David Anderson <danderson@mozilla.com>
 *   David Mandelin <dmandelin@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "StubCompiler.h"
#include "Compiler.h"

using namespace js;
using namespace mjit;

StubCompiler::StubCompiler(JSContext *cx, mjit::Compiler &cc, FrameState &frame, JSScript *script)
  : cx(cx), cc(cc), frame(frame), script(script), exits(SystemAllocPolicy())
{
}

void
StubCompiler::linkExit(Jump j)
{
    /* :TODO: oom check */
    exits.append(ExitPatch(j, masm.label()));
}

void
StubCompiler::syncAndSpill()
{
    frame.sync(masm);
}

void *
StubCompiler::getCallTarget(void *fun)
{
#ifdef JS_CPU_ARM
    /*
     * Insert a veneer for ARM to allow it to catch exceptions. There is no
     * reliable way to determine the location of the return address on the
     * stack, so it cannot be hijacked.
     *
     * :TODO: It wouldn't surprise me if GCC always pushes LR first. In that
     * case, this looks like the x86-style call, and we can hijack the stack
     * slot accordingly, thus avoiding the cost of a veneer. This should be
     * investigated.
     */

    void *pfun = JS_FUNC_TO_DATA_PTR(void *, JaegerStubVeneer);

    /*
     * We put the real target address into IP, as this won't conflict with
     * the EABI argument-passing mechanism. Technically, this isn't ABI-
     * compliant.
     */
    masm.move(Imm32(intptr_t(fun)), ARMRegisters::ip);
#else
    /*
     * Architectures that push the return address to an easily-determined
     * location on the stack can hijack C++'s return mechanism by overwriting
     * that address, so a veneer is not required.
     */
    void *pfun = fun;
#endif
    return pfun;
}

typedef JSC::MacroAssembler::RegisterID RegisterID;
typedef JSC::MacroAssembler::ImmPtr ImmPtr;
typedef JSC::MacroAssembler::Imm32 Imm32;

/* Need a temp reg that is not ArgReg1. */
#if defined(JS_CPU_X86) || defined(JS_CPU_ARM)
static const RegisterID ClobberInCall = JSC::X86Registers::ecx;
#elif defined(JS_CPU_ARM)
static const RegisterID ClobberInCall = JSC::ARMRegisters::r2;
#endif

JS_STATIC_ASSERT(ClobberInCall != Registers::ArgReg1);

JSC::MacroAssembler::Call
StubCompiler::scall(void *ptr)
{
    void *pfun = getCallTarget(ptr);

    /* PC -> regs->pc :( */
    masm.storePtr(ImmPtr(cc.getPC()),
                  FrameAddress(offsetof(VMFrame, regs) + offsetof(JSFrameRegs, pc)));

    /* sp = fp + slots() + stackDepth */
    masm.addPtr(Imm32(sizeof(JSStackFrame) +
                (frame.stackDepth() + script->nfixed) * sizeof(jsval)),
                FrameState::FpReg, ClobberInCall);

    /* regs->sp = sp */
    masm.storePtr(ClobberInCall,
                  FrameAddress(offsetof(VMFrame, regs) + offsetof(JSFrameRegs, sp)));
    
    /* VMFrame -> ArgReg0 */
    masm.move(Assembler::stackPointerRegister, Registers::ArgReg0);

    return masm.call(pfun);
}


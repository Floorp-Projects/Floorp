/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Andrew Scheff <ascheff@mozilla.com>
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

#include "Assembler-x64.h"
#include "ion/IonCompartment.h"
#include "ion/IonLinker.h"

using namespace js::ion;
using namespace JSC;

/* This method generates a trampoline on x64 for a c++ function with
 * the following signature:
 *   JSBool blah(void *code, int argc, Value *argv, Value *vp)
 *   ...using standard x64 fastcall calling convention
 */
IonCode *
IonCompartment::generateEnterJIT(JSContext *cx)
{
    const Register reg_code = ArgReg0;
    const Register reg_argc = ArgReg1;
    const Register reg_argv = ArgReg2;
    const Register reg_vp   = ArgReg3;

    MacroAssembler masm(cx);

    // Save old stack frame pointer, set new stack frame pointer.
    masm.push(rbp);
    masm.mov(rsp, rbp);

    // Save non-volatile registers.
    masm.push(rbx);
    masm.push(r12);
    masm.push(r13);
    masm.push(r14);
    masm.push(r15);
#if defined(_WIN64)
    masm.push(rdi);
    masm.push(rsi);
#endif

    // Save arguments passed in registers needed after function call.
    masm.push(reg_vp);

    // Remember stack depth without padding and arguments.
    masm.mov(rsp, r14);

    // Remember number of bytes occupied by argument vector
    masm.mov(reg_argc, r13);
    masm.shll(Imm32(3), r13);

    // Guarantee 16-byte alignment.
    // We push argc, frame size, and return address.
    // The latter two are 16 bytes together, so we only consider argc.
    masm.mov(rsp, r12);
    masm.subq(r13, r12);
    masm.andl(Imm32(0xf), r12);
    masm.subq(r12, rsp);

    /***************************************************************
    Loop over argv vector, push arguments onto stack in reverse order
    ***************************************************************/

    // r13 still stores the number of bytes in the argument vector.
    masm.addq(reg_argv, r13); // r13 points above last argument.

    // while r13 > rdx, push arguments.
    {
        Label header, footer;
        masm.bind(&header);

        masm.cmpq(r13, reg_argv);
        masm.j(AssemblerX86Shared::BelowOrEqual, &footer);

        masm.subq(Imm32(8), r13);
        masm.push(Operand(r13, 0));
        masm.jmp(&header);

        masm.bind(&footer);
    }

    /*****************************************************************
    Push the number of bytes we've pushed so far on the stack and call
    *****************************************************************/
    masm.subq(rsp, r14);
    masm.push(r14);

    // Call function.
    masm.call(reg_code);

    // Pop arguments and padding from stack.
    masm.pop(r14);
    masm.addq(r14, rsp);

    /*****************************************************************
    Place return value where it belongs, pop all saved registers
    *****************************************************************/
    masm.pop(r12); // vp
    masm.movq(JSReturnReg, Operand(r12, 0));

    // Restore non-volatile registers.
#if defined(_WIN64)
    masm.pop(rsi);
    masm.pop(rdi);
#endif
    masm.pop(r15);
    masm.pop(r14);
    masm.pop(r13);
    masm.pop(r12);
    masm.pop(rbx);

    // Restore frame pointer and return.
    masm.pop(rbp);
    masm.movq(ImmWord(1), rax);
    masm.ret();

    Linker linker(masm);
    return linker.newCode(cx);
}


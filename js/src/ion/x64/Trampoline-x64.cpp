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
 *   void blah(void *code, int argc, Value *argv, Value *vp)
 *   ...using standard x64 fastcall calling convention
 */
IonCode *
IonCompartment::generateEnterJIT(JSContext *cx)
{
    Assembler masm;

    // Save old stack frame pointer, set new stack frame pointer.
    masm.push(rbp);
    masm.mov(rsp, rbp);

    // Save nonvolatile registers
    masm.push(rbx);
    masm.push(r12);
    masm.push(r13);
    masm.push(r14);
    masm.push(r15);
#if defined(_WIN64)
    masm.push(rdi);
    masm.push(rsi);
#endif

    // Save arguments passed in registers that we'll need after the function call
    masm.push(ArgReg4); // return value pointer

    // We need the stack to be 16 byte alligned when we jump into JIT code
    // We know that the stack is 8 byte alligned now (odd # of pushes so far),
    // so if we push an odd number of 8 byte words onto the stack we need to push
    // a word of padding so the stack will 16 byte alligned after the return address
    // is pushed. We push all args, then the number of bytes we pushed,
    // so if argc+1 is odd, then we need a word of padding
    masm.mov(ArgReg2, r12); // rsi has argc
    masm.andl(Imm32(1), Operand(r12)); //r12 has parity of argc
    masm.xorl(Imm32(1), Operand(r12)); //now r12 has inverse parity of argc (1 if argc is even)
    masm.shll(Imm32(3), r12); // final padding amount (either 8 or 0)

    masm.subq(r12, rsp); // put padding on the stack

    /***************************************************************
    Loop over argv vector, push arguments onto stack in reverse order
    ***************************************************************/
    masm.mov(ArgReg2, r13); // r13 = argc
    masm.subq(Imm32(1), r13);
    masm.shll(Imm32(3), r13); // r13 = 8(argc-1)
    masm.addq(ArgReg3, r13); // r13 now points to the last argument

    Label loopHeader, loopEnd;

    //while r13 >= rdx  -- while we are still pointing to arguments
    masm.bind(&loopHeader);

    masm.cmpq(r13, ArgReg3);
    masm.j(AssemblerX86Shared::LessThan, &loopEnd);

    masm.push(Operand(r13, 0)); // Push what r13 points to on the stack

    masm.subq(Imm32(8), r13); // Decrement r13 so that it points to the previous arg

    masm.jmp(&loopHeader); // loop footer
    masm.bind(&loopEnd);

    /*****************************************************************
    Push the number of bytes we've pushed so far on the stack and call
    *****************************************************************/
    masm.shll(Imm32(3), ArgReg2);
    masm.addq(ArgReg2, r12); // r12 = sizeof(padding) + sizeof(Value)*argc
    masm.push(r12); // Push on stack

    masm.call(Operand(ArgReg1)); // Call function

    masm.pop(r12); // Get the size of all args + padding we pushed on the stack
    masm.addq(r12, rsp); // Pop all those args and padding off of the stack

    /*****************************************************************
    Place return value where it belongs, pop all saved registers
    *****************************************************************/
    masm.pop(r12); // r12 is now a pointer to return value pointer vp
    masm.movq(JSReturnReg, Operand(r12, 0)); // Move the return value to memory

    // Restore nonvolatile registers
#if defined(_WIN64)
    masm.pop(rsi);
    masm.pop(rdi);
#endif
    masm.pop(r15);
    masm.pop(r14);
    masm.pop(r13);
    masm.pop(r12);
    masm.pop(rbx);

    // Restore frame pointer and return
    masm.pop(rbp);
    masm.ret();

    LinkerT<Assembler> linker(masm);
    return linker.newCode(cx);
}

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

#include "assembler/assembler/MacroAssembler.h"
#include "ion/IonCompartment.h"
#include "ion/IonLinker.h"

using namespace js::ion;

/* This method generates a trampoline on x86 for a c++ function with
 * the following signature:
 *   void blah(void *code, int argc, Value *argv, Value *vp)
 *   ...using standard cdecl calling convention
 */
IonCode *
IonCompartment::generateEnterJIT(JSContext *cx)
{
    MacroAssembler masm;

    // Save old stack frame pointer, set new stack fram pointer.
    masm.push(ebp);
    masm.movl(Operand(esp), ebp);

    // Save non-volatile registers
    masm.push(ebx);
    masm.push(esi);
    masm.push(edi);

    // eax <- 8*argc, eax is now the offset betwen argv and the last
    // parameter    --argc is in ebp + 12
    masm.movl(Operand(ebp, 12), eax);
    masm.shll(Imm32(3), eax);

    // We need to ensure that the stack is aligned on a 12-byte boundary, so
    // inside the JIT function the stack is 16-byte aligned. Our stack right
    // now might not be aligned on some platforms (win32, gcc) so we factor
    // this possibility in, and simulate what the new stack address would be.
    //   +argc * 8 for arguments
    //   +4 for pushing alignment
    //   +4 for pushing the return address
    masm.movl(esp, ecx);
    masm.subl(eax, ecx);
    masm.subl(Imm32(8), ecx);

    // ecx = ecx & 15, holds alignment.
    masm.andl(Imm32(15), ecx);
    masm.subl(ecx, esp);

    /***************************************************************
    Loop over argv vector, push arguments onto stack in reverse order
    ***************************************************************/

    // ebx = argv   --argv pointer is in ebp + 16
    masm.movl(Operand(ebp, 16), ebx);

    // eax = argv[8(argc)]  --eax now points one value past the last argument
    masm.addl(ebx, eax);

    // while (eax > ebx)  --while still looping through arguments
    {
        Label header, footer;
        masm.bind(&header);

        masm.cmpl(eax, ebx);
        masm.j(Assembler::BelowOrEqual, &footer);

        // eax -= 8  --move to previous argument
        masm.subl(Imm32(8), eax);

        // Push what eax points to on stack, a Value is 2 words
        masm.push(Operand(eax, 4));
        masm.push(Operand(eax, 0));

        masm.jmp(&header);
        masm.bind(&footer);
    }

    // Save the stack size so we can remove arguments and alignment after the
    // call.
    masm.movl(Operand(ebp, 12), eax);
    masm.shll(Imm32(3), eax);
    masm.addl(eax, ecx);
    masm.push(ecx);

    /***************************************************************
        Call passed-in code, get return value and fill in the
        passed in return value pointer
    ***************************************************************/
    // Call code  --code pointer is in ebp + 8
    masm.call(Operand(ebp, 8));

    // Pop arguments off the stack.
    // eax <- 8*argc (size of all arugments we pushed on the stack)
    masm.pop(eax);
    masm.addl(eax, esp);

    // |ebp| could have been clobbered by the inner function. For now, re-grab
    // |vp| directly off the stack:
    //
    //  +32 vp
    //  +28 argv
    //  +24 argc
    //  +20 code
    //  +16 <return>
    //  +12 ebp
    //  +8  ebx
    //  +4  esi
    //  +0  edi
    masm.movl(Operand(esp, 32), eax);
    masm.movl(JSReturnReg_Type, Operand(eax, 4));
    masm.movl(JSReturnReg_Data, Operand(eax, 0));

    /**************************************************************
        Return stack and registers to correct state
    **************************************************************/

    // Restore non-volatile registers
    masm.pop(edi);
    masm.pop(esi);
    masm.pop(ebx);

    // Restore old stack frame pointer
    masm.pop(ebp);
    masm.ret();

    Linker linker(masm);
    return linker.newCode(cx);
}


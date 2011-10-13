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
 *  Marty Rosenberg <mrosenberg@mozilla.com>
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
#include "ion/arm/MacroAssembler-arm.h"
using namespace js;
using namespace ion;
void
MacroAssemblerARM::convertInt32ToDouble(const Register &src, const FloatRegister &dest)
{
    // direct conversions aren't possible.
    as_vxfer(src, InvalidReg, VFPRegister(dest, VFPRegister::Single),
             CoreToFloat);
    as_vcvt(VFPRegister(dest, VFPRegister::Double),
            VFPRegister(dest, VFPRegister::Single));
}

bool
MacroAssemblerARM::alu_dbl(Register src1, Imm32 imm, Register dest, ALUOp op,
                           SetCond_ sc, Condition c)
{
    if ((sc == SetCond && ! condsAreSafe(op)) || !can_dbl(op)) {
        return false;
    }
    ALUOp interop = getDestVariant(op);
    Imm8::TwoImm8mData both = Imm8::encodeTwoImms(imm.value);
    if (both.fst.invalid) {
        return false;
    }
    // for the most part, there is no good reason to set the condition
    // codes for the first instruction.
    // we can do better things if the second instruction doesn't
    // have a dest, such as check for overflow by doing first operation
    // don't do second operation if first operation overflowed.
    // this preserves the overflow condition code.
    // unfortunately, it is horribly brittle.
    as_alu(ScratchRegister, src1, both.fst, interop, NoSetCond, c);
    as_alu(ScratchRegister, ScratchRegister, both.snd, op, sc, c);
    // we succeeded!
    return true;
}


void
MacroAssemblerARM::ma_alu(Register src1, Imm32 imm, Register dest,
                          ALUOp op,
                          SetCond_ sc, Condition c)
{
    // As it turns out, if you ask for a compare-like instruction
    // you *probably* want it to set condition codes.
    if (dest == InvalidReg) {
        JS_ASSERT(sc == SetCond);
    }

    // The operator gives us the ability to determine how
    // this can be used.
    Imm8 imm8 = Imm8(imm.value);
    // ONE INSTRUCTION:
    // If we can encode it using an imm8m, then do so.
    if (!imm8.invalid) {
        as_alu(dest, src1, imm8, op, sc, c);
        return;
    }
    // ONE INSTRUCTION, NEGATED:
    Imm32 negImm = imm;
    ALUOp negOp = ALUNeg(op, &negImm);
    Imm8 negImm8 = Imm8(negImm.value);
    if (negOp != op_invalid && !negImm8.invalid) {
        as_alu(dest, src1, negImm8, negOp, sc, c);
        return;
    }
    if (hasMOVWT()) {
        // If the operation is a move-a-like then we can try to use movw to
        // move the bits into the destination.  Otherwise, we'll need to
        // fall back on a multi-instruction format :(
        // movw/movt don't set condition codes, so don't hold your breath.
        if (sc == NoSetCond && (op == op_mov || op == op_mvn)) {
            // ARMv7 supports movw/movt. movw zero-extends
            // its 16 bit argument, so we can set the register
            // this way.
            // movt leaves the bottom 16 bits in tact, so
            // it is unsuitable to move a constant that
            if (op == op_mov && ((imm.value & ~ 0xffff) == 0)) {
                JS_ASSERT(src1 == InvalidReg);
                as_movw(dest, (uint16)imm.value, c);
                return;
            }
            // If they asked for a mvn rfoo, imm, where ~imm fits into 16 bits
            // then do it.
            if (op == op_mvn && (((~imm.value) & ~ 0xffff) == 0)) {
                JS_ASSERT(src1 == InvalidReg);
                as_movw(dest, (uint16)~imm.value, c);
                return;
            }
            // TODO: constant dedup may enable us to add dest, r0, 23 *if*
            // we are attempting to load a constant that looks similar to one
            // that already exists
            // If it can't be done with a single movw
            // then we *need* to use two instructions
            // since this must be some sort of a move operation, we can just use
            // a movw/movt pair and get the whole thing done in two moves.  This
            // does not work for ops like add, sinc we'd need to do
            // movw tmp; movt tmp; add dest, tmp, src1
            if (op == op_mvn)
                imm.value = ~imm.value;
            as_movw(dest, imm.value & 0xffff, c);
            as_movt(dest, (imm.value >> 16) & 0xffff, c);
            return;
        }
        // If we weren't doing a movalike, a 16 bit immediate
        // will require 2 instructions.  With the same amount of
        // space and (less)time, we can do two 8 bit operations, reusing
        // the dest register.  e.g.
        // movw tmp, 0xffff; add dest, src, tmp ror 4
        // vs.
        // add dest, src, 0xff0; add dest, dest, 0xf000000f
        // it turns out that there are some immediates that we miss with the
        // second approach.  A sample value is: add dest, src, 0x1fffe
        // this can be done by movw tmp, 0xffff; add dest, src, tmp lsl 1
        // since imm8m's only get even offsets, we cannot encode this.
        // I'll try to encode as two imm8's first, since they are faster.
        // Both operations should take 1 cycle, where as add dest, tmp ror 4
        // takes two cycles to execute.
    }
    // Either a) this isn't ARMv7 b) this isn't a move
    // start by attempting to generate a two instruction form.
    // Some things cannot be made into two-inst forms correctly.
    // namely, adds dest, src, 0xffff.
    // Since we want the condition codes (and don't know which ones will
    // be checked), we need to assume that the overflow flag will be checked
    // and add{,s} dest, src, 0xff00; add{,s} dest, dest, 0xff is not
    // guaranteed to set the overflow flag the same as the (theoretical)
    // one instruction variant.
    if (alu_dbl(src1, imm, dest, op, sc, c))
        return;
    // And try with its negative.
    if (negOp != op_invalid &&
        alu_dbl(src1, negImm, dest, negOp, sc, c))
        return;
    // Well, damn. We can use two 16 bit mov's, then do the op
    // or we can do a single load from a pool then op.
    if (hasMOVWT()) {
        // Try to load the immediate into a scratch register
        // then use that
        as_movw(ScratchRegister, imm.value & 0xffff, c);
        as_movt(ScratchRegister, (imm.value >> 16) & 0xffff, c);
    } else {
        JS_NOT_REACHED("non-ARMv7 loading of immediates NYI.");
    }
    as_alu(dest, src1, O2Reg(ScratchRegister), op, sc, c);
    // done!
}

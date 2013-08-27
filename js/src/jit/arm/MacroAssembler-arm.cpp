/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/arm/MacroAssembler-arm.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/MathAlgorithms.h"

#include "jit/Bailouts.h"
#include "jit/BaselineFrame.h"
#include "jit/MoveEmitter.h"

using namespace js;
using namespace jit;

using mozilla::Abs;

bool
isValueDTRDCandidate(ValueOperand &val)
{
    // In order to be used for a DTRD memory function, the two target registers
    // need to be a) Adjacent, with the tag larger than the payload, and
    // b) Aligned to a multiple of two.
    if ((val.typeReg().code() != (val.payloadReg().code() + 1)))
        return false;
    if ((val.payloadReg().code() & 1) != 0)
        return false;
    return true;
}

void
MacroAssemblerARM::convertBoolToInt32(Register source, Register dest)
{
    // Note that C++ bool is only 1 byte, so zero extend it to clear the
    // higher-order bits.
    ma_and(Imm32(0xff), source, dest);
}

void
MacroAssemblerARM::convertInt32ToDouble(const Register &src, const FloatRegister &dest_)
{
    // direct conversions aren't possible.
    VFPRegister dest = VFPRegister(dest_);
    as_vxfer(src, InvalidReg, dest.sintOverlay(),
             CoreToFloat);
    as_vcvt(dest, dest.sintOverlay());
}

void
MacroAssemblerARM::convertInt32ToDouble(const Address &src, FloatRegister dest)
{
    ma_ldr(Operand(src), ScratchRegister);
    convertInt32ToDouble(ScratchRegister, dest);
}

void
MacroAssemblerARM::convertUInt32ToDouble(const Register &src, const FloatRegister &dest_)
{
    // direct conversions aren't possible.
    VFPRegister dest = VFPRegister(dest_);
    as_vxfer(src, InvalidReg, dest.uintOverlay(),
             CoreToFloat);
    as_vcvt(dest, dest.uintOverlay());
}

void MacroAssemblerARM::convertDoubleToFloat(const FloatRegister &src, const FloatRegister &dest)
{
    as_vcvt(VFPRegister(dest).singleOverlay(), VFPRegister(src));
}

// there are two options for implementing emitTruncateDouble.
// 1) convert the floating point value to an integer, if it did not fit,
//        then it was clamped to INT_MIN/INT_MAX, and we can test it.
//        NOTE: if the value really was supposed to be INT_MAX / INT_MIN
//        then it will be wrong.
// 2) convert the floating point value to an integer, if it did not fit,
//        then it set one or two bits in the fpcsr.  Check those.
void
MacroAssemblerARM::branchTruncateDouble(const FloatRegister &src, const Register &dest, Label *fail)
{
    ma_vcvt_F64_I32(src, ScratchFloatReg);
    ma_vxfer(ScratchFloatReg, dest);
    ma_cmp(dest, Imm32(0x7fffffff));
    ma_cmp(dest, Imm32(0x80000000), Assembler::NotEqual);
    ma_b(fail, Assembler::Equal);
}

// Checks whether a double is representable as a 32-bit integer. If so, the
// integer is written to the output register. Otherwise, a bailout is taken to
// the given snapshot. This function overwrites the scratch float register.
void
MacroAssemblerARM::convertDoubleToInt32(const FloatRegister &src, const Register &dest, Label *fail, bool negativeZeroCheck)
{
    // convert the floating point value to an integer, if it did not fit,
    //     then when we convert it *back* to  a float, it will have a
    //     different value, which we can test.
    ma_vcvt_F64_I32(src, ScratchFloatReg);
    // move the value into the dest register.
    ma_vxfer(ScratchFloatReg, dest);
    ma_vcvt_I32_F64(ScratchFloatReg, ScratchFloatReg);
    ma_vcmp(src, ScratchFloatReg);
    as_vmrs(pc);
    ma_b(fail, Assembler::VFP_NotEqualOrUnordered);

    if (negativeZeroCheck) {
        ma_cmp(dest, Imm32(0));
        // Test and bail for -0.0, when integer result is 0
        // Move the top word of the double into the output reg, if it is non-zero,
        // then the original value was -0.0
        as_vxfer(dest, InvalidReg, src, FloatToCore, Assembler::Equal, 1);
        ma_cmp(dest, Imm32(0x80000000), Assembler::Equal);
        ma_b(fail, Assembler::Equal);
    }
}

void
MacroAssemblerARM::addDouble(FloatRegister src, FloatRegister dest)
{
    ma_vadd(dest, src, dest);
}

void
MacroAssemblerARM::subDouble(FloatRegister src, FloatRegister dest)
{
    ma_vsub(dest, src, dest);
}

void
MacroAssemblerARM::mulDouble(FloatRegister src, FloatRegister dest)
{
    ma_vmul(dest, src, dest);
}

void
MacroAssemblerARM::divDouble(FloatRegister src, FloatRegister dest)
{
    ma_vdiv(dest, src, dest);
}

void
MacroAssemblerARM::negateDouble(FloatRegister reg)
{
    ma_vneg(reg, reg);
}

void
MacroAssemblerARM::inc64(AbsoluteAddress dest)
{

    ma_strd(r0, r1, EDtrAddr(sp, EDtrOffImm(-8)), PreIndex);

    ma_mov(Imm32((int32_t)dest.addr), ScratchRegister);

    ma_ldrd(EDtrAddr(ScratchRegister, EDtrOffImm(0)), r0, r1);

    ma_add(Imm32(1), r0, SetCond);
    ma_adc(Imm32(0), r1, NoSetCond);

    ma_strd(r0, r1, EDtrAddr(ScratchRegister, EDtrOffImm(0)));

    ma_ldrd(EDtrAddr(sp, EDtrOffImm(8)), r0, r1, PostIndex);

}

bool
MacroAssemblerARM::alu_dbl(Register src1, Imm32 imm, Register dest, ALUOp op,
                           SetCond_ sc, Condition c)
{
    if ((sc == SetCond && ! condsAreSafe(op)) || !can_dbl(op))
        return false;
    ALUOp interop = getDestVariant(op);
    Imm8::TwoImm8mData both = Imm8::encodeTwoImms(imm.value);
    if (both.fst.invalid)
        return false;
    // for the most part, there is no good reason to set the condition
    // codes for the first instruction.
    // we can do better things if the second instruction doesn't
    // have a dest, such as check for overflow by doing first operation
    // don't do second operation if first operation overflowed.
    // this preserves the overflow condition code.
    // unfortunately, it is horribly brittle.
    as_alu(ScratchRegister, src1, both.fst, interop, NoSetCond, c);
    as_alu(dest, ScratchRegister, both.snd, op, sc, c);
    return true;
}


void
MacroAssemblerARM::ma_alu(Register src1, Imm32 imm, Register dest,
                          ALUOp op,
                          SetCond_ sc, Condition c)
{
    // As it turns out, if you ask for a compare-like instruction
    // you *probably* want it to set condition codes.
    if (dest == InvalidReg)
        JS_ASSERT(sc == SetCond);

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
    Register negDest;
    ALUOp negOp = ALUNeg(op, dest, &negImm, &negDest);
    Imm8 negImm8 = Imm8(negImm.value);
    // add r1, r2, -15 can be replaced with
    // sub r1, r2, 15
    // for bonus points, dest can be replaced (nearly always invalid => ScratchRegister)
    // This is useful if we wish to negate tst.  tst has an invalid (aka not used) dest,
    // but its negation is bic *requires* a dest.  We can accomodate, but it will need to clobber
    // *something*, and the scratch register isn't being used, so...
    if (negOp != op_invalid && !negImm8.invalid) {
        as_alu(negDest, src1, negImm8, negOp, sc, c);
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
                as_movw(dest, (uint16_t)imm.value, c);
                return;
            }

            // If they asked for a mvn rfoo, imm, where ~imm fits into 16 bits
            // then do it.
            if (op == op_mvn && (((~imm.value) & ~ 0xffff) == 0)) {
                JS_ASSERT(src1 == InvalidReg);
                as_movw(dest, (uint16_t)~imm.value, c);
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
        if ((imm.value >> 16) != 0)
            as_movt(ScratchRegister, (imm.value >> 16) & 0xffff, c);
    } else {
        // Going to have to use a load.  If the operation is a move, then just move it into the
        // destination register
        if (op == op_mov) {
            as_Imm32Pool(dest, imm.value, NULL, c);
            return;
        } else {
            // If this isn't just going into a register, then stick it in a temp, and then proceed.
            as_Imm32Pool(ScratchRegister, imm.value, NULL, c);
        }
    }
    as_alu(dest, src1, O2Reg(ScratchRegister), op, sc, c);
}

void
MacroAssemblerARM::ma_alu(Register src1, Operand op2, Register dest, ALUOp op,
            SetCond_ sc, Assembler::Condition c)
{
    JS_ASSERT(op2.getTag() == Operand::OP2);
    as_alu(dest, src1, op2.toOp2(), op, sc, c);
}

void
MacroAssemblerARM::ma_alu(Register src1, Operand2 op2, Register dest, ALUOp op, SetCond_ sc, Condition c)
{
    as_alu(dest, src1, op2, op, sc, c);
}

void
MacroAssemblerARM::ma_nop()
{
    as_nop();
}

Instruction *
NextInst(Instruction *i)
{
    if (i == NULL)
        return NULL;
    return i->next();
}

void
MacroAssemblerARM::ma_movPatchable(Imm32 imm_, Register dest,
                                   Assembler::Condition c, RelocStyle rs, Instruction *i)
{
    int32_t imm = imm_.value;
    if (i) {
        // Make sure the current instruction is not an artificial guard
        // inserted by the assembler buffer.
        // The InstructionIterator already does this and handles edge cases,
        // so, just asking an iterator for its current instruction should be
        // enough to make sure we don't accidentally inspect an artificial guard.
        i = InstructionIterator(i).cur();
    }
    switch(rs) {
      case L_MOVWT:
        as_movw(dest, Imm16(imm & 0xffff), c, i);
        // i can be NULL here.  that just means "insert in the next in sequence."
        // NextInst is special cased to not do anything when it is passed NULL, so two
        // consecutive instructions will be inserted.
        i = NextInst(i);
        as_movt(dest, Imm16(imm >> 16 & 0xffff), c, i);
        break;
      case L_LDR:
        if(i == NULL)
            as_Imm32Pool(dest, imm, NULL, c);
        else
            as_WritePoolEntry(i, c, imm);
        break;
    }
}

void
MacroAssemblerARM::ma_mov(Register src, Register dest,
            SetCond_ sc, Assembler::Condition c)
{
    if (sc == SetCond || dest != src)
        as_mov(dest, O2Reg(src), sc, c);
}

void
MacroAssemblerARM::ma_mov(Imm32 imm, Register dest,
                          SetCond_ sc, Assembler::Condition c)
{
    ma_alu(InvalidReg, imm, dest, op_mov, sc, c);
}

void
MacroAssemblerARM::ma_mov(ImmWord imm, Register dest,
                          SetCond_ sc, Assembler::Condition c)
{
    ma_alu(InvalidReg, Imm32(imm.value), dest, op_mov, sc, c);
}

void
MacroAssemblerARM::ma_mov(const ImmGCPtr &ptr, Register dest)
{
    // As opposed to x86/x64 version, the data relocation has to be executed
    // before to recover the pointer, and not after.
    writeDataRelocation(ptr);
    RelocStyle rs;
    if (hasMOVWT())
        rs = L_MOVWT;
    else
        rs = L_LDR;

    ma_movPatchable(Imm32(ptr.value), dest, Always, rs);
}

    // Shifts (just a move with a shifting op2)
void
MacroAssemblerARM::ma_lsl(Imm32 shift, Register src, Register dst)
{
    as_mov(dst, lsl(src, shift.value));
}
void
MacroAssemblerARM::ma_lsr(Imm32 shift, Register src, Register dst)
{
    as_mov(dst, lsr(src, shift.value));
}
void
MacroAssemblerARM::ma_asr(Imm32 shift, Register src, Register dst)
{
    as_mov(dst, asr(src, shift.value));
}
void
MacroAssemblerARM::ma_ror(Imm32 shift, Register src, Register dst)
{
    as_mov(dst, ror(src, shift.value));
}
void
MacroAssemblerARM::ma_rol(Imm32 shift, Register src, Register dst)
{
    as_mov(dst, rol(src, shift.value));
}
    // Shifts (just a move with a shifting op2)
void
MacroAssemblerARM::ma_lsl(Register shift, Register src, Register dst)
{
    as_mov(dst, lsl(src, shift));
}
void
MacroAssemblerARM::ma_lsr(Register shift, Register src, Register dst)
{
    as_mov(dst, lsr(src, shift));
}
void
MacroAssemblerARM::ma_asr(Register shift, Register src, Register dst)
{
    as_mov(dst, asr(src, shift));
}
void
MacroAssemblerARM::ma_ror(Register shift, Register src, Register dst)
{
    as_mov(dst, ror(src, shift));
}
void
MacroAssemblerARM::ma_rol(Register shift, Register src, Register dst)
{
    ma_rsb(shift, Imm32(32), ScratchRegister);
    as_mov(dst, ror(src, ScratchRegister));
}

    // Move not (dest <- ~src)

void
MacroAssemblerARM::ma_mvn(Imm32 imm, Register dest, SetCond_ sc, Assembler::Condition c)
{
    ma_alu(InvalidReg, imm, dest, op_mvn, sc, c);
}

void
MacroAssemblerARM::ma_mvn(Register src1, Register dest, SetCond_ sc, Assembler::Condition c)
{
    as_alu(dest, InvalidReg, O2Reg(src1), op_mvn, sc, c);
}

// Negate (dest <- -src), src is a register, rather than a general op2.
void
MacroAssemblerARM::ma_neg(Register src1, Register dest, SetCond_ sc, Assembler::Condition c)
{
    as_rsb(dest, src1, Imm8(0), sc, c);
}

// And.
void
MacroAssemblerARM::ma_and(Register src, Register dest, SetCond_ sc, Assembler::Condition c)
{
    ma_and(dest, src, dest);
}
void
MacroAssemblerARM::ma_and(Register src1, Register src2, Register dest,
                          SetCond_ sc, Assembler::Condition c)
{
    as_and(dest, src1, O2Reg(src2), sc, c);
}
void
MacroAssemblerARM::ma_and(Imm32 imm, Register dest, SetCond_ sc, Assembler::Condition c)
{
    ma_alu(dest, imm, dest, op_and, sc, c);
}
void
MacroAssemblerARM::ma_and(Imm32 imm, Register src1, Register dest,
                          SetCond_ sc, Assembler::Condition c)
{
    ma_alu(src1, imm, dest, op_and, sc, c);
}


// Bit clear (dest <- dest & ~imm) or (dest <- src1 & ~src2).
void
MacroAssemblerARM::ma_bic(Imm32 imm, Register dest, SetCond_ sc, Assembler::Condition c)
{
    ma_alu(dest, imm, dest, op_bic, sc, c);
}

// Exclusive or.
void
MacroAssemblerARM::ma_eor(Register src, Register dest, SetCond_ sc, Assembler::Condition c)
{
    ma_eor(dest, src, dest, sc, c);
}
void
MacroAssemblerARM::ma_eor(Register src1, Register src2, Register dest,
                          SetCond_ sc, Assembler::Condition c)
{
    as_eor(dest, src1, O2Reg(src2), sc, c);
}
void
MacroAssemblerARM::ma_eor(Imm32 imm, Register dest, SetCond_ sc, Assembler::Condition c)
{
    ma_alu(dest, imm, dest, op_eor, sc, c);
}
void
MacroAssemblerARM::ma_eor(Imm32 imm, Register src1, Register dest,
       SetCond_ sc, Assembler::Condition c)
{
    ma_alu(src1, imm, dest, op_eor, sc, c);
}

// Or.
void
MacroAssemblerARM::ma_orr(Register src, Register dest, SetCond_ sc, Assembler::Condition c)
{
    ma_orr(dest, src, dest, sc, c);
}
void
MacroAssemblerARM::ma_orr(Register src1, Register src2, Register dest,
                          SetCond_ sc, Assembler::Condition c)
{
    as_orr(dest, src1, O2Reg(src2), sc, c);
}
void
MacroAssemblerARM::ma_orr(Imm32 imm, Register dest, SetCond_ sc, Assembler::Condition c)
{
    ma_alu(dest, imm, dest, op_orr, sc, c);
}
void
MacroAssemblerARM::ma_orr(Imm32 imm, Register src1, Register dest,
                          SetCond_ sc, Assembler::Condition c)
{
    ma_alu(src1, imm, dest, op_orr, sc, c);
}

// Arithmetic-based ops.
// Add with carry.
void
MacroAssemblerARM::ma_adc(Imm32 imm, Register dest, SetCond_ sc, Condition c)
{
    ma_alu(dest, imm, dest, op_adc, sc, c);
}
void
MacroAssemblerARM::ma_adc(Register src, Register dest, SetCond_ sc, Condition c)
{
    as_alu(dest, dest, O2Reg(src), op_adc, sc, c);
}
void
MacroAssemblerARM::ma_adc(Register src1, Register src2, Register dest, SetCond_ sc, Condition c)
{
    as_alu(dest, src1, O2Reg(src2), op_adc, sc, c);
}

// Add.
void
MacroAssemblerARM::ma_add(Imm32 imm, Register dest, SetCond_ sc, Condition c)
{
    ma_alu(dest, imm, dest, op_add, sc, c);
}

void
MacroAssemblerARM::ma_add(Register src1, Register dest, SetCond_ sc, Condition c)
{
    ma_alu(dest, O2Reg(src1), dest, op_add, sc, c);
}
void
MacroAssemblerARM::ma_add(Register src1, Register src2, Register dest, SetCond_ sc, Condition c)
{
    as_alu(dest, src1, O2Reg(src2), op_add, sc, c);
}
void
MacroAssemblerARM::ma_add(Register src1, Operand op, Register dest, SetCond_ sc, Condition c)
{
    ma_alu(src1, op, dest, op_add, sc, c);
}
void
MacroAssemblerARM::ma_add(Register src1, Imm32 op, Register dest, SetCond_ sc, Condition c)
{
    ma_alu(src1, op, dest, op_add, sc, c);
}

// Subtract with carry.
void
MacroAssemblerARM::ma_sbc(Imm32 imm, Register dest, SetCond_ sc, Condition c)
{
    ma_alu(dest, imm, dest, op_sbc, sc, c);
}
void
MacroAssemblerARM::ma_sbc(Register src1, Register dest, SetCond_ sc, Condition c)
{
    as_alu(dest, dest, O2Reg(src1), op_sbc, sc, c);
}
void
MacroAssemblerARM::ma_sbc(Register src1, Register src2, Register dest, SetCond_ sc, Condition c)
{
    as_alu(dest, src1, O2Reg(src2), op_sbc, sc, c);
}

// Subtract.
void
MacroAssemblerARM::ma_sub(Imm32 imm, Register dest, SetCond_ sc, Condition c)
{
    ma_alu(dest, imm, dest, op_sub, sc, c);
}
void
MacroAssemblerARM::ma_sub(Register src1, Register dest, SetCond_ sc, Condition c)
{
    ma_alu(dest, Operand(src1), dest, op_sub, sc, c);
}
void
MacroAssemblerARM::ma_sub(Register src1, Register src2, Register dest, SetCond_ sc, Condition c)
{
    ma_alu(src1, Operand(src2), dest, op_sub, sc, c);
}
void
MacroAssemblerARM::ma_sub(Register src1, Operand op, Register dest, SetCond_ sc, Condition c)
{
    ma_alu(src1, op, dest, op_sub, sc, c);
}
void
MacroAssemblerARM::ma_sub(Register src1, Imm32 op, Register dest, SetCond_ sc, Condition c)
{
    ma_alu(src1, op, dest, op_sub, sc, c);
}

// Severse subtract.
void
MacroAssemblerARM::ma_rsb(Imm32 imm, Register dest, SetCond_ sc, Condition c)
{
    ma_alu(dest, imm, dest, op_rsb, sc, c);
}
void
MacroAssemblerARM::ma_rsb(Register src1, Register dest, SetCond_ sc, Condition c)
{
    as_alu(dest, dest, O2Reg(src1), op_add, sc, c);
}
void
MacroAssemblerARM::ma_rsb(Register src1, Register src2, Register dest, SetCond_ sc, Condition c)
{
    as_alu(dest, src1, O2Reg(src2), op_rsb, sc, c);
}
void
MacroAssemblerARM::ma_rsb(Register src1, Imm32 op2, Register dest, SetCond_ sc, Condition c)
{
    ma_alu(src1, op2, dest, op_rsb, sc, c);
}

// Reverse subtract with carry.
void
MacroAssemblerARM::ma_rsc(Imm32 imm, Register dest, SetCond_ sc, Condition c)
{
    ma_alu(dest, imm, dest, op_rsc, sc, c);
}
void
MacroAssemblerARM::ma_rsc(Register src1, Register dest, SetCond_ sc, Condition c)
{
    as_alu(dest, dest, O2Reg(src1), op_rsc, sc, c);
}
void
MacroAssemblerARM::ma_rsc(Register src1, Register src2, Register dest, SetCond_ sc, Condition c)
{
    as_alu(dest, src1, O2Reg(src2), op_rsc, sc, c);
}

// Compares/tests.
// Compare negative (sets condition codes as src1 + src2 would).
void
MacroAssemblerARM::ma_cmn(Register src1, Imm32 imm, Condition c)
{
    ma_alu(src1, imm, InvalidReg, op_cmn, SetCond, c);
}
void
MacroAssemblerARM::ma_cmn(Register src1, Register src2, Condition c)
{
    as_alu(InvalidReg, src2, O2Reg(src1), op_cmn, SetCond, c);
}
void
MacroAssemblerARM::ma_cmn(Register src1, Operand op, Condition c)
{
    MOZ_ASSUME_UNREACHABLE("Feature NYI");
}

// Compare (src - src2).
void
MacroAssemblerARM::ma_cmp(Register src1, Imm32 imm, Condition c)
{
    ma_alu(src1, imm, InvalidReg, op_cmp, SetCond, c);
}

void
MacroAssemblerARM::ma_cmp(Register src1, ImmWord ptr, Condition c)
{
    ma_cmp(src1, Imm32(ptr.value), c);
}

void
MacroAssemblerARM::ma_cmp(Register src1, ImmGCPtr ptr, Condition c)
{
    ma_mov(ptr, ScratchRegister);
    ma_cmp(src1, ScratchRegister, c);
}
void
MacroAssemblerARM::ma_cmp(Register src1, Operand op, Condition c)
{
    switch (op.getTag()) {
      case Operand::OP2:
        as_cmp(src1, op.toOp2(), c);
        break;
      case Operand::MEM:
        ma_ldr(op, ScratchRegister);
        as_cmp(src1, O2Reg(ScratchRegister), c);
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("trying to compare FP and integer registers");
    }
}
void
MacroAssemblerARM::ma_cmp(Register src1, Register src2, Condition c)
{
    as_cmp(src1, O2Reg(src2), c);
}

// Test for equality, (src1^src2).
void
MacroAssemblerARM::ma_teq(Register src1, Imm32 imm, Condition c)
{
    ma_alu(src1, imm, InvalidReg, op_teq, SetCond, c);
}
void
MacroAssemblerARM::ma_teq(Register src1, Register src2, Condition c)
{
    as_tst(src1, O2Reg(src2), c);
}
void
MacroAssemblerARM::ma_teq(Register src1, Operand op, Condition c)
{
    as_teq(src1, op.toOp2(), c);
}


// Test (src1 & src2).
void
MacroAssemblerARM::ma_tst(Register src1, Imm32 imm, Condition c)
{
    ma_alu(src1, imm, InvalidReg, op_tst, SetCond, c);
}
void
MacroAssemblerARM::ma_tst(Register src1, Register src2, Condition c)
{
    as_tst(src1, O2Reg(src2), c);
}
void
MacroAssemblerARM::ma_tst(Register src1, Operand op, Condition c)
{
    as_tst(src1, op.toOp2(), c);
}

void
MacroAssemblerARM::ma_mul(Register src1, Register src2, Register dest)
{
    as_mul(dest, src1, src2);
}
void
MacroAssemblerARM::ma_mul(Register src1, Imm32 imm, Register dest)
{

    ma_mov(imm, ScratchRegister);
    as_mul( dest, src1, ScratchRegister);
}

Assembler::Condition
MacroAssemblerARM::ma_check_mul(Register src1, Register src2, Register dest, Condition cond)
{
    // TODO: this operation is illegal on armv6 and earlier if src2 == ScratchRegister
    //       or src2 == dest.
    if (cond == Equal || cond == NotEqual) {
        as_smull(ScratchRegister, dest, src1, src2, SetCond);
        return cond;
    }
    
    if (cond == Overflow) {
        as_smull(ScratchRegister, dest, src1, src2);
        as_cmp(ScratchRegister, asr(dest, 31));
        return NotEqual;
    }

    MOZ_ASSUME_UNREACHABLE("Condition NYI");
}

Assembler::Condition
MacroAssemblerARM::ma_check_mul(Register src1, Imm32 imm, Register dest, Condition cond)
{
    ma_mov(imm, ScratchRegister);
    if (cond == Equal || cond == NotEqual) {
        as_smull(ScratchRegister, dest, ScratchRegister, src1, SetCond);
        return cond;
    }
    
    if (cond == Overflow) {
        as_smull(ScratchRegister, dest, ScratchRegister, src1);
        as_cmp(ScratchRegister, asr(dest, 31));
        return NotEqual;
    }

    MOZ_ASSUME_UNREACHABLE("Condition NYI");
}

void
MacroAssemblerARM::ma_mod_mask(Register src, Register dest, Register hold, int32_t shift)
{
    // MATH:
    // We wish to compute x % (1<<y) - 1 for a known constant, y.
    // first, let b = (1<<y) and C = (1<<y)-1, then think of the 32 bit dividend as
    // a number in base b, namely c_0*1 + c_1*b + c_2*b^2 ... c_n*b^n
    // now, since both addition and multiplication commute with modulus,
    // x % C == (c_0 + c_1*b + ... + c_n*b^n) % C ==
    // (c_0 % C) + (c_1%C) * (b % C) + (c_2 % C) * (b^2 % C)...
    // now, since b == C + 1, b % C == 1, and b^n % C == 1
    // this means that the whole thing simplifies to:
    // c_0 + c_1 + c_2 ... c_n % C
    // each c_n can easily be computed by a shift/bitextract, and the modulus can be maintained
    // by simply subtracting by C whenever the number gets over C.
    int32_t mask = (1 << shift) - 1;
    Label head;

    // hold holds -1 if the value was negative, 1 otherwise.
    // ScratchRegister holds the remaining bits that have not been processed
    // lr serves as a temporary location to store extracted bits into as well
    //    as holding the trial subtraction as a temp value
    // dest is the accumulator (and holds the final result)

    // move the whole value into the scratch register, setting the codition codes so
    // we can muck with them later
    as_mov(ScratchRegister, O2Reg(src), SetCond);
    // Zero out the dest.
    ma_mov(Imm32(0), dest);
    // Set the hold appropriately.
    ma_mov(Imm32(1), hold);
    ma_mov(Imm32(-1), hold, NoSetCond, Signed);
    ma_rsb(Imm32(0), ScratchRegister, SetCond, Signed);
    // Begin the main loop.
    bind(&head);

    // Extract the bottom bits into lr.
    ma_and(Imm32(mask), ScratchRegister, secondScratchReg_);
    // Add those bits to the accumulator.
    ma_add(secondScratchReg_, dest, dest);
    // Do a trial subtraction, this is the same operation as cmp, but we store the dest
    ma_sub(dest, Imm32(mask), secondScratchReg_, SetCond);
    // If (sum - C) > 0, store sum - C back into sum, thus performing a modulus.
    ma_mov(secondScratchReg_, dest, NoSetCond, Unsigned);
    // Get rid of the bits that we extracted before, and set the condition codes
    as_mov(ScratchRegister, lsr(ScratchRegister, shift), SetCond);
    // If the shift produced zero, finish, otherwise, continue in the loop.
    ma_b(&head, NonZero);
    // Check the hold to see if we need to negate the result.  Hold can only be 1 or -1,
    // so this will never set the 0 flag.
    ma_cmp(hold, Imm32(0));
    // If the hold was non-zero, negate the result to be in line with what JS wants
    // this will set the condition codes if we try to negate
    ma_rsb(Imm32(0), dest, SetCond, Signed);
    // Since the Zero flag is not set by the compare, we can *only* set the Zero flag
    // in the rsb, so Zero is set iff we negated zero (e.g. the result of the computation was -0.0).

}

void
MacroAssemblerARM::ma_smod(Register num, Register div, Register dest)
{
    as_sdiv(ScratchRegister, num, div);
    as_mls(dest, num, ScratchRegister, div);
}

void
MacroAssemblerARM::ma_umod(Register num, Register div, Register dest)
{
    as_udiv(ScratchRegister, num, div);
    as_mls(dest, num, ScratchRegister, div);
}

// division
void
MacroAssemblerARM::ma_sdiv(Register num, Register div, Register dest, Condition cond)
{
    as_sdiv(dest, num, div, cond);
}

void
MacroAssemblerARM::ma_udiv(Register num, Register div, Register dest, Condition cond)
{
    as_udiv(dest, num, div, cond);
}

// Memory.
// Shortcut for when we know we're transferring 32 bits of data.
void
MacroAssemblerARM::ma_dtr(LoadStore ls, Register rn, Imm32 offset, Register rt,
                          Index mode, Assembler::Condition cc)
{
    ma_dataTransferN(ls, 32, true, rn, offset, rt, mode, cc);
}

void
MacroAssemblerARM::ma_dtr(LoadStore ls, Register rn, Register rm, Register rt,
                          Index mode, Assembler::Condition cc)
{
    MOZ_ASSUME_UNREACHABLE("Feature NYI");
}

void
MacroAssemblerARM::ma_str(Register rt, DTRAddr addr, Index mode, Condition cc)
{
    as_dtr(IsStore, 32, mode, rt, addr, cc);
}

void
MacroAssemblerARM::ma_dtr(LoadStore ls, Register rt, const Operand &addr, Index mode, Condition cc)
{
    ma_dataTransferN(ls, 32, true,
                     Register::FromCode(addr.base()), Imm32(addr.disp()),
                     rt, mode, cc);
}

void
MacroAssemblerARM::ma_str(Register rt, const Operand &addr, Index mode, Condition cc)
{
    ma_dtr(IsStore, rt, addr, mode, cc);
}
void
MacroAssemblerARM::ma_strd(Register rt, DebugOnly<Register> rt2, EDtrAddr addr, Index mode, Condition cc)
{
    JS_ASSERT((rt.code() & 1) == 0);
    JS_ASSERT(rt2.value.code() == rt.code() + 1);
    as_extdtr(IsStore, 64, true, mode, rt, addr, cc);
}

void
MacroAssemblerARM::ma_ldr(DTRAddr addr, Register rt, Index mode, Condition cc)
{
    as_dtr(IsLoad, 32, mode, rt, addr, cc);
}
void
MacroAssemblerARM::ma_ldr(const Operand &addr, Register rt, Index mode, Condition cc)
{
    ma_dtr(IsLoad, rt, addr, mode, cc);
}

void
MacroAssemblerARM::ma_ldrb(DTRAddr addr, Register rt, Index mode, Condition cc)
{
    as_dtr(IsLoad, 8, mode, rt, addr, cc);
}

void
MacroAssemblerARM::ma_ldrsh(EDtrAddr addr, Register rt, Index mode, Condition cc)
{
    as_extdtr(IsLoad, 16, true, mode, rt, addr, cc);
}

void
MacroAssemblerARM::ma_ldrh(EDtrAddr addr, Register rt, Index mode, Condition cc)
{
    as_extdtr(IsLoad, 16, false, mode, rt, addr, cc);
}
void
MacroAssemblerARM::ma_ldrsb(EDtrAddr addr, Register rt, Index mode, Condition cc)
{
    as_extdtr(IsLoad, 8, true, mode, rt, addr, cc);
}
void
MacroAssemblerARM::ma_ldrd(EDtrAddr addr, Register rt, DebugOnly<Register> rt2,
                           Index mode, Condition cc)
{
    JS_ASSERT((rt.code() & 1) == 0);
    JS_ASSERT(rt2.value.code() == rt.code() + 1);
    as_extdtr(IsLoad, 64, true, mode, rt, addr, cc);
}
void
MacroAssemblerARM::ma_strh(Register rt, EDtrAddr addr, Index mode, Condition cc)
{
    as_extdtr(IsStore, 16, false, mode, rt, addr, cc);
}

void
MacroAssemblerARM::ma_strb(Register rt, DTRAddr addr, Index mode, Condition cc)
{
    as_dtr(IsStore, 8, mode, rt, addr, cc);
}

// Specialty for moving N bits of data, where n == 8,16,32,64.
BufferOffset
MacroAssemblerARM::ma_dataTransferN(LoadStore ls, int size, bool IsSigned,
                          Register rn, Register rm, Register rt,
                                    Index mode, Assembler::Condition cc, unsigned shiftAmount)
{
    if (size == 32 || (size == 8 && !IsSigned)) {
        return as_dtr(ls, size, mode, rt, DTRAddr(rn, DtrRegImmShift(rm, LSL, shiftAmount)), cc);
    } else {
        if (shiftAmount != 0) {
            JS_ASSERT(rn != ScratchRegister);
            JS_ASSERT(rt != ScratchRegister);
            ma_lsl(Imm32(shiftAmount), rm, ScratchRegister);
            rm = ScratchRegister;
        }
        return as_extdtr(ls, size, IsSigned, mode, rt, EDtrAddr(rn, EDtrOffReg(rm)), cc);
    }
}

BufferOffset
MacroAssemblerARM::ma_dataTransferN(LoadStore ls, int size, bool IsSigned,
                                    Register rn, Imm32 offset, Register rt,
                                    Index mode, Assembler::Condition cc)
{
    int off = offset.value;
    // we can encode this as a standard ldr... MAKE IT SO
    if (size == 32 || (size == 8 && !IsSigned) ) {
        if (off < 4096 && off > -4096) {
            // This encodes as a single instruction, Emulating mode's behavior
            // in a multi-instruction sequence is not necessary.
            return as_dtr(ls, size, mode, rt, DTRAddr(rn, DtrOffImm(off)), cc);
        }

        // We cannot encode this offset in a a single ldr. For mode == index,
        // try to encode it as |add scratch, base, imm; ldr dest, [scratch, +offset]|.
        // This does not wark for mode == PreIndex or mode == PostIndex.
        // PreIndex is simple, just do the add into the base register first, then do
        // a PreIndex'ed load. PostIndexed loads can be tricky.  Normally, doing the load with
        // an index of 0, then doing an add would work, but if the destination is the PC,
        // you don't get to execute the instruction after the branch, which will lead to
        // the base register not being updated correctly. Explicitly handle this case, without
        // doing anything fancy, then handle all of the other cases.

        // mode == Offset
        //  add   scratch, base, offset_hi
        //  ldr   dest, [scratch, +offset_lo]
        //
        // mode == PreIndex
        //  add   base, base, offset_hi
        //  ldr   dest, [base, +offset_lo]!
        //
        // mode == PostIndex, dest == pc
        //  ldr   scratch, [base]
        //  add   base, base, offset_hi
        //  add   base, base, offset_lo
        //  mov   dest, scratch
        // PostIndex with the pc as the destination needs to be handled
        // specially, since in the code below, the write into 'dest'
        // is going to alter the control flow, so the following instruction would
        // never get emitted.
        //
        // mode == PostIndex, dest != pc
        //  ldr   dest, [base], offset_lo
        //  add   base, base, offset_hi

        if (rt == pc && mode == PostIndex && ls == IsLoad) {
            ma_mov(rn, ScratchRegister);
            ma_alu(rn, offset, rn, op_add);
            return as_dtr(IsLoad, size, Offset, pc, DTRAddr(ScratchRegister, DtrOffImm(0)), cc);
        }

        int bottom = off & 0xfff;
        int neg_bottom = 0x1000 - bottom;
        // For a regular offset, base == ScratchRegister does what we want.  Modify the
        // scratch register, leaving the actual base unscathed.
        Register base = ScratchRegister;
        // For the preindex case, we want to just re-use rn as the base register, so when
        // the base register is updated *before* the load, rn is updated.
        if (mode == PreIndex)
            base = rn;
        JS_ASSERT(mode != PostIndex);
        // At this point, both off - bottom and off + neg_bottom will be reasonable-ish quantities.
        //
        // Note a neg_bottom of 0x1000 can not be encoded as an immediate negative offset in the
        // instruction and this occurs when bottom is zero, so this case is guarded against below.
        // 
        if (off < 0) {
            Operand2 sub_off = Imm8(-(off-bottom)); // sub_off = bottom - off
            if (!sub_off.invalid) {
                as_sub(ScratchRegister, rn, sub_off, NoSetCond, cc); // - sub_off = off - bottom
                return as_dtr(ls, size, Offset, rt, DTRAddr(ScratchRegister, DtrOffImm(bottom)), cc);
            }
            sub_off = Imm8(-(off+neg_bottom));// sub_off = -neg_bottom - off
            if (!sub_off.invalid && bottom != 0) {
                JS_ASSERT(neg_bottom < 0x1000);  // Guarded against by: bottom != 0
                as_sub(ScratchRegister, rn, sub_off, NoSetCond, cc); // - sub_off = neg_bottom + off
                return as_dtr(ls, size, Offset, rt, DTRAddr(ScratchRegister, DtrOffImm(-neg_bottom)), cc);
            }
        } else {
            Operand2 sub_off = Imm8(off-bottom); // sub_off = off - bottom
            if (!sub_off.invalid) {
                as_add(ScratchRegister, rn, sub_off, NoSetCond, cc); //  sub_off = off - bottom
                return as_dtr(ls, size, Offset, rt, DTRAddr(ScratchRegister, DtrOffImm(bottom)), cc);
            }
            sub_off = Imm8(off+neg_bottom);// sub_off = neg_bottom + off
            if (!sub_off.invalid && bottom != 0) {
                JS_ASSERT(neg_bottom < 0x1000);  // Guarded against by: bottom != 0
                as_add(ScratchRegister, rn, sub_off, NoSetCond,  cc); // sub_off = neg_bottom + off
                return as_dtr(ls, size, Offset, rt, DTRAddr(ScratchRegister, DtrOffImm(-neg_bottom)), cc);
            }
        }
        ma_mov(offset, ScratchRegister);
        return as_dtr(ls, size, mode, rt, DTRAddr(rn, DtrRegImmShift(ScratchRegister, LSL, 0)));
    } else {
        // should attempt to use the extended load/store instructions
        if (off < 256 && off > -256)
            return as_extdtr(ls, size, IsSigned, mode, rt, EDtrAddr(rn, EDtrOffImm(off)), cc);

        // We cannot encode this offset in a single extldr.  Try to encode it as
        // an add scratch, base, imm; extldr dest, [scratch, +offset].
        int bottom = off & 0xff;
        int neg_bottom = 0x100 - bottom;
        // At this point, both off - bottom and off + neg_bottom will be reasonable-ish quantities.
        //
        // Note a neg_bottom of 0x100 can not be encoded as an immediate negative offset in the
        // instruction and this occurs when bottom is zero, so this case is guarded against below.
        // 
        if (off < 0) {
            Operand2 sub_off = Imm8(-(off-bottom)); // sub_off = bottom - off
            if (!sub_off.invalid) {
                as_sub(ScratchRegister, rn, sub_off, NoSetCond, cc); // - sub_off = off - bottom
                return as_extdtr(ls, size, IsSigned, Offset, rt,
                                 EDtrAddr(ScratchRegister, EDtrOffImm(bottom)),
                                 cc);
            }
            sub_off = Imm8(-(off+neg_bottom));// sub_off = -neg_bottom - off
            if (!sub_off.invalid && bottom != 0) {
                JS_ASSERT(neg_bottom < 0x100);  // Guarded against by: bottom != 0
                as_sub(ScratchRegister, rn, sub_off, NoSetCond, cc); // - sub_off = neg_bottom + off
                return as_extdtr(ls, size, IsSigned, Offset, rt,
                                 EDtrAddr(ScratchRegister, EDtrOffImm(-neg_bottom)),
                                 cc);
            }
        } else {
            Operand2 sub_off = Imm8(off-bottom); // sub_off = off - bottom
            if (!sub_off.invalid) {
                as_add(ScratchRegister, rn, sub_off, NoSetCond, cc); //  sub_off = off - bottom
                return as_extdtr(ls, size, IsSigned, Offset, rt,
                                 EDtrAddr(ScratchRegister, EDtrOffImm(bottom)),
                                 cc);
            }
            sub_off = Imm8(off+neg_bottom);// sub_off = neg_bottom + off
            if (!sub_off.invalid && bottom != 0) {
                JS_ASSERT(neg_bottom < 0x100);  // Guarded against by: bottom != 0
                as_add(ScratchRegister, rn, sub_off, NoSetCond,  cc); // sub_off = neg_bottom + off
                return as_extdtr(ls, size, IsSigned, Offset, rt,
                                 EDtrAddr(ScratchRegister, EDtrOffImm(-neg_bottom)),
                                 cc);
            }
        }
        ma_mov(offset, ScratchRegister);
        return as_extdtr(ls, size, IsSigned, mode, rt, EDtrAddr(rn, EDtrOffReg(ScratchRegister)), cc);
    }
}

void
MacroAssemblerARM::ma_pop(Register r)
{
    ma_dtr(IsLoad, sp, Imm32(4), r, PostIndex);
    if (r == pc)
        m_buffer.markGuard();
}
void
MacroAssemblerARM::ma_push(Register r)
{
    // Pushing sp is not well defined: use two instructions.
    if (r == sp) {
        ma_mov(sp, ScratchRegister);
        r = ScratchRegister;
    }
    ma_dtr(IsStore, sp,Imm32(-4), r, PreIndex);
}

void
MacroAssemblerARM::ma_vpop(VFPRegister r)
{
    startFloatTransferM(IsLoad, sp, IA, WriteBack);
    transferFloatReg(r);
    finishFloatTransfer();
}
void
MacroAssemblerARM::ma_vpush(VFPRegister r)
{
    startFloatTransferM(IsStore, sp, DB, WriteBack);
    transferFloatReg(r);
    finishFloatTransfer();
}

// Branches when done from within arm-specific code.
void
MacroAssemblerARM::ma_b(Label *dest, Assembler::Condition c, bool isPatchable)
{
    as_b(dest, c, isPatchable);
}

void
MacroAssemblerARM::ma_bx(Register dest, Assembler::Condition c)
{
    as_bx(dest, c);
}

static Assembler::RelocBranchStyle
b_type()
{
    return Assembler::B_LDR;
}
void
MacroAssemblerARM::ma_b(void *target, Relocation::Kind reloc, Assembler::Condition c)
{
    // we know the absolute address of the target, but not our final
    // location (with relocating GC, we *can't* know our final location)
    // for now, I'm going to be conservative, and load this with an
    // absolute address
    uint32_t trg = (uint32_t)target;
    switch (b_type()) {
      case Assembler::B_MOVWT:
        as_movw(ScratchRegister, Imm16(trg & 0xffff), c);
        as_movt(ScratchRegister, Imm16(trg >> 16), c);
        // this is going to get the branch predictor pissed off.
        as_bx(ScratchRegister, c);
        break;
      case Assembler::B_LDR_BX:
        as_Imm32Pool(ScratchRegister, trg, NULL, c);
        as_bx(ScratchRegister, c);
        break;
      case Assembler::B_LDR:
        as_Imm32Pool(pc, trg, NULL, c);
        if (c == Always)
            m_buffer.markGuard();
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("Other methods of generating tracable jumps NYI");
    }
}

// This is almost NEVER necessary: we'll basically never be calling a label,
// except possibly in the crazy bailout-table case.
void
MacroAssemblerARM::ma_bl(Label *dest, Assembler::Condition c)
{
    as_bl(dest, c);
}

void
MacroAssemblerARM::ma_blx(Register reg, Assembler::Condition c)
{
    as_blx(reg, c);
}

// VFP/ALU
void
MacroAssemblerARM::ma_vadd(FloatRegister src1, FloatRegister src2, FloatRegister dst)
{
    as_vadd(VFPRegister(dst), VFPRegister(src1), VFPRegister(src2));
}

void
MacroAssemblerARM::ma_vsub(FloatRegister src1, FloatRegister src2, FloatRegister dst)
{
    as_vsub(VFPRegister(dst), VFPRegister(src1), VFPRegister(src2));
}

void
MacroAssemblerARM::ma_vmul(FloatRegister src1, FloatRegister src2, FloatRegister dst)
{
    as_vmul(VFPRegister(dst), VFPRegister(src1), VFPRegister(src2));
}

void
MacroAssemblerARM::ma_vdiv(FloatRegister src1, FloatRegister src2, FloatRegister dst)
{
    as_vdiv(VFPRegister(dst), VFPRegister(src1), VFPRegister(src2));
}

void
MacroAssemblerARM::ma_vmov(FloatRegister src, FloatRegister dest, Condition cc)
{
    as_vmov(dest, src, cc);
}

void
MacroAssemblerARM::ma_vneg(FloatRegister src, FloatRegister dest, Condition cc)
{
    as_vneg(dest, src, cc);
}

void
MacroAssemblerARM::ma_vabs(FloatRegister src, FloatRegister dest, Condition cc)
{
    as_vabs(dest, src, cc);
}

void
MacroAssemblerARM::ma_vsqrt(FloatRegister src, FloatRegister dest, Condition cc)
{
    as_vsqrt(dest, src, cc);
}

void
MacroAssemblerARM::ma_vimm(double value, FloatRegister dest, Condition cc)
{
    union DoublePun {
        struct {
#if defined(IS_LITTLE_ENDIAN)
            uint32_t lo, hi;
#else
            uint32_t hi, lo;
#endif
        } s;
        double d;
    } dpun;
    dpun.d = value;
    if (hasVFPv3()) {
        if (dpun.s.lo == 0) {
            if (dpun.s.hi == 0) {
                // To zero a register, load 1.0, then execute dN <- dN - dN
                VFPImm dblEnc(0x3FF00000);
                as_vimm(dest, dblEnc, cc);
                as_vsub(dest, dest, dest, cc);
                return;
            }

            VFPImm dblEnc(dpun.s.hi);
            if (dblEnc.isValid()) {
                as_vimm(dest, dblEnc, cc);
                return;
            }

        }
    }
    // Fall back to putting the value in a pool.
    as_FImm64Pool(dest, value, NULL, cc);
}

void
MacroAssemblerARM::ma_vcmp(FloatRegister src1, FloatRegister src2, Condition cc)
{
    as_vcmp(VFPRegister(src1), VFPRegister(src2), cc);
}
void
MacroAssemblerARM::ma_vcmpz(FloatRegister src1, Condition cc)
{
    as_vcmpz(VFPRegister(src1), cc);
}

void
MacroAssemblerARM::ma_vcvt_F64_I32(FloatRegister src, FloatRegister dest, Condition cc)
{
    as_vcvt(VFPRegister(dest).sintOverlay(), VFPRegister(src), false, cc);
}
void
MacroAssemblerARM::ma_vcvt_F64_U32(FloatRegister src, FloatRegister dest, Condition cc)
{
    as_vcvt(VFPRegister(dest).uintOverlay(), VFPRegister(src), false, cc);
}
void
MacroAssemblerARM::ma_vcvt_I32_F64(FloatRegister dest, FloatRegister src, Condition cc)
{
    as_vcvt(VFPRegister(dest), VFPRegister(src).sintOverlay(), false, cc);
}
void
MacroAssemblerARM::ma_vcvt_U32_F64(FloatRegister dest, FloatRegister src, Condition cc)
{
    as_vcvt(VFPRegister(dest), VFPRegister(src).uintOverlay(), false, cc);
}

void
MacroAssemblerARM::ma_vxfer(FloatRegister src, Register dest, Condition cc)
{
    as_vxfer(dest, InvalidReg, VFPRegister(src).singleOverlay(), FloatToCore, cc);
}

void
MacroAssemblerARM::ma_vxfer(FloatRegister src, Register dest1, Register dest2, Condition cc)
{
    as_vxfer(dest1, dest2, VFPRegister(src), FloatToCore, cc);
}

void
MacroAssemblerARM::ma_vxfer(Register src1, Register src2, FloatRegister dest, Condition cc)
{
    as_vxfer(src1, src2, VFPRegister(dest), CoreToFloat, cc);
}

void
MacroAssemblerARM::ma_vxfer(VFPRegister src, Register dest, Condition cc)
{
    as_vxfer(dest, InvalidReg, src, FloatToCore, cc);
}

void
MacroAssemblerARM::ma_vxfer(VFPRegister src, Register dest1, Register dest2, Condition cc)
{
    as_vxfer(dest1, dest2, src, FloatToCore, cc);
}

BufferOffset
MacroAssemblerARM::ma_vdtr(LoadStore ls, const Operand &addr, VFPRegister rt, Condition cc)
{
    int off = addr.disp();
    JS_ASSERT((off & 3) == 0);
    Register base = Register::FromCode(addr.base());
    if (off > -1024 && off < 1024)
        return as_vdtr(ls, rt, addr.toVFPAddr(), cc);

    // We cannot encode this offset in a a single ldr.  Try to encode it as
    // an add scratch, base, imm; ldr dest, [scratch, +offset].
    int bottom = off & (0xff << 2);
    int neg_bottom = (0x100 << 2) - bottom;
    // At this point, both off - bottom and off + neg_bottom will be reasonable-ish quantities.
    //
    // Note a neg_bottom of 0x400 can not be encoded as an immediate negative offset in the
    // instruction and this occurs when bottom is zero, so this case is guarded against below.
    // 
    if (off < 0) {
        Operand2 sub_off = Imm8(-(off-bottom)); // sub_off = bottom - off
        if (!sub_off.invalid) {
            as_sub(ScratchRegister, base, sub_off, NoSetCond, cc); // - sub_off = off - bottom
            return as_vdtr(ls, rt, VFPAddr(ScratchRegister, VFPOffImm(bottom)), cc);
        }
        sub_off = Imm8(-(off+neg_bottom));// sub_off = -neg_bottom - off
        if (!sub_off.invalid && bottom != 0) {
            JS_ASSERT(neg_bottom < 0x400);  // Guarded against by: bottom != 0
            as_sub(ScratchRegister, base, sub_off, NoSetCond, cc); // - sub_off = neg_bottom + off
            return as_vdtr(ls, rt, VFPAddr(ScratchRegister, VFPOffImm(-neg_bottom)), cc);
        }
    } else {
        Operand2 sub_off = Imm8(off-bottom); // sub_off = off - bottom
        if (!sub_off.invalid) {
            as_add(ScratchRegister, base, sub_off, NoSetCond, cc); //  sub_off = off - bottom
            return as_vdtr(ls, rt, VFPAddr(ScratchRegister, VFPOffImm(bottom)), cc);
        }
        sub_off = Imm8(off+neg_bottom);// sub_off = neg_bottom + off
        if (!sub_off.invalid && bottom != 0) {
            JS_ASSERT(neg_bottom < 0x400);  // Guarded against by: bottom != 0
            as_add(ScratchRegister, base, sub_off, NoSetCond,  cc); // sub_off = neg_bottom + off
            return as_vdtr(ls, rt, VFPAddr(ScratchRegister, VFPOffImm(-neg_bottom)), cc);
        }
    }
    ma_add(base, Imm32(off), ScratchRegister, NoSetCond, cc);
    return as_vdtr(ls, rt, VFPAddr(ScratchRegister, VFPOffImm(0)), cc);
}

BufferOffset
MacroAssemblerARM::ma_vldr(VFPAddr addr, VFPRegister dest, Condition cc)
{
    return as_vdtr(IsLoad, dest, addr, cc);
}
BufferOffset
MacroAssemblerARM::ma_vldr(const Operand &addr, VFPRegister dest, Condition cc)
{
    return ma_vdtr(IsLoad, addr, dest, cc);
}
BufferOffset
MacroAssemblerARM::ma_vldr(VFPRegister src, Register base, Register index, int32_t shift, Condition cc)
{
    as_add(ScratchRegister, base, lsl(index, shift), NoSetCond, cc);
    return ma_vldr(Operand(ScratchRegister, 0), src, cc);
}

BufferOffset
MacroAssemblerARM::ma_vstr(VFPRegister src, VFPAddr addr, Condition cc)
{
    return as_vdtr(IsStore, src, addr, cc);
}

BufferOffset
MacroAssemblerARM::ma_vstr(VFPRegister src, const Operand &addr, Condition cc)
{
    return ma_vdtr(IsStore, addr, src, cc);
}
BufferOffset
MacroAssemblerARM::ma_vstr(VFPRegister src, Register base, Register index, int32_t shift, Condition cc)
{
    as_add(ScratchRegister, base, lsl(index, shift), NoSetCond, cc);
    return ma_vstr(src, Operand(ScratchRegister, 0), cc);
}

bool
MacroAssemblerARMCompat::buildFakeExitFrame(const Register &scratch, uint32_t *offset)
{
    DebugOnly<uint32_t> initialDepth = framePushed();
    uint32_t descriptor = MakeFrameDescriptor(framePushed(), IonFrame_OptimizedJS);

    Push(Imm32(descriptor)); // descriptor_

    enterNoPool();
    DebugOnly<uint32_t> offsetBeforePush = currentOffset();
    Push(pc); // actually pushes $pc + 8.

    // Consume an additional 4 bytes. The start of the next instruction will
    // then be 8 bytes after the instruction for Push(pc); this offset can
    // therefore be fed to the safepoint.
    ma_nop();
    uint32_t pseudoReturnOffset = currentOffset();
    leaveNoPool();

    JS_ASSERT(framePushed() == initialDepth + IonExitFrameLayout::Size());
    JS_ASSERT(pseudoReturnOffset - offsetBeforePush == 8);

    *offset = pseudoReturnOffset;
    return true;
}

bool
MacroAssemblerARMCompat::buildOOLFakeExitFrame(void *fakeReturnAddr)
{
    DebugOnly<uint32_t> initialDepth = framePushed();
    uint32_t descriptor = MakeFrameDescriptor(framePushed(), IonFrame_OptimizedJS);

    Push(Imm32(descriptor)); // descriptor_

    enterNoPool();
    Push(Imm32((uint32_t) fakeReturnAddr));
    leaveNoPool();

    return true;
}

void
MacroAssemblerARMCompat::callWithExitFrame(IonCode *target)
{
    uint32_t descriptor = MakeFrameDescriptor(framePushed(), IonFrame_OptimizedJS);
    Push(Imm32(descriptor)); // descriptor

    addPendingJump(m_buffer.nextOffset(), target->raw(), Relocation::IONCODE);
    RelocStyle rs;
    if (hasMOVWT())
        rs = L_MOVWT;
    else
        rs = L_LDR;

    ma_movPatchable(Imm32((int) target->raw()), ScratchRegister, Always, rs);
    ma_callIonHalfPush(ScratchRegister);
}

void
MacroAssemblerARMCompat::callWithExitFrame(IonCode *target, Register dynStack)
{
    ma_add(Imm32(framePushed()), dynStack);
    makeFrameDescriptor(dynStack, IonFrame_OptimizedJS);
    Push(dynStack); // descriptor

    addPendingJump(m_buffer.nextOffset(), target->raw(), Relocation::IONCODE);
    RelocStyle rs;
    if (hasMOVWT())
        rs = L_MOVWT;
    else
        rs = L_LDR;

    ma_movPatchable(Imm32((int) target->raw()), ScratchRegister, Always, rs);
    ma_callIonHalfPush(ScratchRegister);
}

void
MacroAssemblerARMCompat::callIon(const Register &callee)
{
    JS_ASSERT((framePushed() & 3) == 0);
    if ((framePushed() & 7) == 4) {
        ma_callIonHalfPush(callee);
    } else {
        adjustFrame(sizeof(void*));
        ma_callIon(callee);
    }
}

void
MacroAssemblerARMCompat::reserveStack(uint32_t amount)
{
    if (amount)
        ma_sub(Imm32(amount), sp);
    adjustFrame(amount);
}
void
MacroAssemblerARMCompat::freeStack(uint32_t amount)
{
    JS_ASSERT(amount <= framePushed_);
    if (amount)
        ma_add(Imm32(amount), sp);
    adjustFrame(-amount);
}
void
MacroAssemblerARMCompat::freeStack(Register amount)
{
    ma_add(amount, sp);
}

void
MacroAssemblerARMCompat::add32(Register src, Register dest)
{
    ma_add(src, dest, SetCond);
}

void
MacroAssemblerARMCompat::add32(Imm32 imm, Register dest)
{
    ma_add(imm, dest, SetCond);
}

void
MacroAssemblerARMCompat::xor32(Imm32 imm, Register dest)
{
    ma_eor(imm, dest, SetCond);
}

void
MacroAssemblerARMCompat::add32(Imm32 imm, const Address &dest)
{
    load32(dest, ScratchRegister);
    ma_add(imm, ScratchRegister, SetCond);
    store32(ScratchRegister, dest);
}

void
MacroAssemblerARMCompat::sub32(Imm32 imm, Register dest)
{
    ma_sub(imm, dest, SetCond);
}

void
MacroAssemblerARMCompat::sub32(Register src, Register dest)
{
    ma_sub(src, dest, SetCond);
}

void
MacroAssemblerARMCompat::and32(Imm32 imm, Register dest)
{
    ma_and(imm, dest, SetCond);
}

void
MacroAssemblerARMCompat::addPtr(Register src, Register dest)
{
    ma_add(src, dest);
}

void
MacroAssemblerARMCompat::addPtr(const Address &src, Register dest)
{
    load32(src, ScratchRegister);
    ma_add(ScratchRegister, dest, SetCond);
}

void
MacroAssemblerARMCompat::and32(Imm32 imm, const Address &dest)
{
    load32(dest, ScratchRegister);
    ma_and(imm, ScratchRegister);
    store32(ScratchRegister, dest);
}

void
MacroAssemblerARMCompat::or32(Imm32 imm, const Address &dest)
{
    load32(dest, ScratchRegister);
    ma_orr(imm, ScratchRegister);
    store32(ScratchRegister, dest);
}

void
MacroAssemblerARMCompat::xorPtr(Imm32 imm, Register dest)
{
    ma_eor(imm, dest);
}

void
MacroAssemblerARMCompat::xorPtr(Register src, Register dest)
{
    ma_eor(src, dest);
}

void
MacroAssemblerARMCompat::orPtr(Imm32 imm, Register dest)
{
    ma_orr(imm, dest);
}

void
MacroAssemblerARMCompat::orPtr(Register src, Register dest)
{
    ma_orr(src, dest);
}

void
MacroAssemblerARMCompat::andPtr(Imm32 imm, Register dest)
{
    ma_and(imm, dest);
}

void
MacroAssemblerARMCompat::andPtr(Register src, Register dest)
{
    ma_and(src, dest);
}

void
MacroAssemblerARMCompat::move32(const Imm32 &imm, const Register &dest)
{
    ma_mov(imm, dest);
}
void
MacroAssemblerARMCompat::movePtr(const Register &src, const Register &dest)
{
    ma_mov(src, dest);
}
void
MacroAssemblerARMCompat::movePtr(const ImmWord &imm, const Register &dest)
{
    ma_mov(Imm32(imm.value), dest);
}
void
MacroAssemblerARMCompat::movePtr(const ImmGCPtr &imm, const Register &dest)
{
    ma_mov(imm, dest);
}
void
MacroAssemblerARMCompat::load8ZeroExtend(const Address &address, const Register &dest)
{
    ma_dataTransferN(IsLoad, 8, false, address.base, Imm32(address.offset), dest);
}

void
MacroAssemblerARMCompat::load8ZeroExtend(const BaseIndex &src, const Register &dest)
{
    Register base = src.base;
    uint32_t scale = Imm32::ShiftOf(src.scale).value;

    if (src.offset != 0) {
        ma_mov(base, ScratchRegister);
        base = ScratchRegister;
        ma_add(base, Imm32(src.offset), base);
    }
    ma_ldrb(DTRAddr(base, DtrRegImmShift(src.index, LSL, scale)), dest);

}

void
MacroAssemblerARMCompat::load8SignExtend(const Address &address, const Register &dest)
{
    ma_dataTransferN(IsLoad, 8, true, address.base, Imm32(address.offset), dest);
}

void
MacroAssemblerARMCompat::load8SignExtend(const BaseIndex &src, const Register &dest)
{
    Register index = src.index;

    // ARMv7 does not have LSL on an index register with an extended load.
    if (src.scale != TimesOne) {
        ma_lsl(Imm32::ShiftOf(src.scale), index, ScratchRegister);
        index = ScratchRegister;
    }

    if (src.offset != 0) {
        if (index != ScratchRegister) {
            ma_mov(index, ScratchRegister);
            index = ScratchRegister;
        }
        ma_add(Imm32(src.offset), index);
    }
    ma_ldrsb(EDtrAddr(src.base, EDtrOffReg(index)), dest);
}

void
MacroAssemblerARMCompat::load16ZeroExtend(const Address &address, const Register &dest)
{
    ma_dataTransferN(IsLoad, 16, false, address.base, Imm32(address.offset), dest);
}

void
MacroAssemblerARMCompat::load16ZeroExtend(const BaseIndex &src, const Register &dest)
{
    Register index = src.index;

    // ARMv7 does not have LSL on an index register with an extended load.
    if (src.scale != TimesOne) {
        ma_lsl(Imm32::ShiftOf(src.scale), index, ScratchRegister);
        index = ScratchRegister;
    }

    if (src.offset != 0) {
        if (index != ScratchRegister) {
            ma_mov(index, ScratchRegister);
            index = ScratchRegister;
        }
        ma_add(Imm32(src.offset), index);
    }
    ma_ldrh(EDtrAddr(src.base, EDtrOffReg(index)), dest);
}

void
MacroAssemblerARMCompat::load16SignExtend(const Address &address, const Register &dest)
{
    ma_dataTransferN(IsLoad, 16, true, address.base, Imm32(address.offset), dest);
}

void
MacroAssemblerARMCompat::load16SignExtend(const BaseIndex &src, const Register &dest)
{
    Register index = src.index;

    // We don't have LSL on index register yet.
    if (src.scale != TimesOne) {
        ma_lsl(Imm32::ShiftOf(src.scale), index, ScratchRegister);
        index = ScratchRegister;
    }

    if (src.offset != 0) {
        if (index != ScratchRegister) {
            ma_mov(index, ScratchRegister);
            index = ScratchRegister;
        }
        ma_add(Imm32(src.offset), index);
    }
    ma_ldrsh(EDtrAddr(src.base, EDtrOffReg(index)), dest);
}

void
MacroAssemblerARMCompat::load32(const Address &address, const Register &dest)
{
    loadPtr(address, dest);
}

void
MacroAssemblerARMCompat::load32(const BaseIndex &address, const Register &dest)
{
    loadPtr(address, dest);
}

void
MacroAssemblerARMCompat::load32(const AbsoluteAddress &address, const Register &dest)
{
    loadPtr(address, dest);
}
void
MacroAssemblerARMCompat::loadPtr(const Address &address, const Register &dest)
{
    ma_ldr(Operand(address), dest);
}

void
MacroAssemblerARMCompat::loadPtr(const BaseIndex &src, const Register &dest)
{
    Register base = src.base;
    uint32_t scale = Imm32::ShiftOf(src.scale).value;

    if (src.offset != 0) {
        ma_mov(base, ScratchRegister);
        base = ScratchRegister;
        ma_add(Imm32(src.offset), base);
    }
    ma_ldr(DTRAddr(base, DtrRegImmShift(src.index, LSL, scale)), dest);
}
void
MacroAssemblerARMCompat::loadPtr(const AbsoluteAddress &address, const Register &dest)
{
    movePtr(ImmWord(address.addr), ScratchRegister);
    loadPtr(Address(ScratchRegister, 0x0), dest);
}

Operand payloadOf(const Address &address) {
    return Operand(address.base, address.offset);
}
Operand tagOf(const Address &address) {
    return Operand(address.base, address.offset + 4);
}

void
MacroAssemblerARMCompat::loadPrivate(const Address &address, const Register &dest)
{
    ma_ldr(payloadOf(address), dest);
}

void
MacroAssemblerARMCompat::loadDouble(const Address &address, const FloatRegister &dest)
{
    ma_vldr(Operand(address), dest);
}

void
MacroAssemblerARMCompat::loadDouble(const BaseIndex &src, const FloatRegister &dest)
{
    // VFP instructions don't even support register Base + register Index modes, so
    // just add the index, then handle the offset like normal
    Register base = src.base;
    Register index = src.index;
    uint32_t scale = Imm32::ShiftOf(src.scale).value;
    int32_t offset = src.offset;
    as_add(ScratchRegister, base, lsl(index, scale));

    ma_vldr(Operand(ScratchRegister, offset), dest);
}

void
MacroAssemblerARMCompat::loadFloatAsDouble(const Address &address, const FloatRegister &dest)
{
    VFPRegister rt = dest;
    ma_vdtr(IsLoad, address, rt.singleOverlay());
    as_vcvt(rt, rt.singleOverlay());
}

void
MacroAssemblerARMCompat::loadFloatAsDouble(const BaseIndex &src, const FloatRegister &dest)
{
    // VFP instructions don't even support register Base + register Index modes, so
    // just add the index, then handle the offset like normal
    Register base = src.base;
    Register index = src.index;
    uint32_t scale = Imm32::ShiftOf(src.scale).value;
    int32_t offset = src.offset;
    VFPRegister rt = dest;
    as_add(ScratchRegister, base, lsl(index, scale));

    ma_vdtr(IsLoad, Operand(ScratchRegister, offset), rt.singleOverlay());
    as_vcvt(rt, rt.singleOverlay());
}

void
MacroAssemblerARMCompat::store8(const Imm32 &imm, const Address &address)
{
    ma_mov(imm, secondScratchReg_);
    store8(secondScratchReg_, address);
}

void
MacroAssemblerARMCompat::store8(const Register &src, const Address &address)
{
    ma_dataTransferN(IsStore, 8, false, address.base, Imm32(address.offset), src);
}

void
MacroAssemblerARMCompat::store8(const Imm32 &imm, const BaseIndex &dest)
{
    ma_mov(imm, secondScratchReg_);
    store8(secondScratchReg_, dest);
}

void
MacroAssemblerARMCompat::store8(const Register &src, const BaseIndex &dest)
{
    Register base = dest.base;
    uint32_t scale = Imm32::ShiftOf(dest.scale).value;

    if (dest.offset != 0) {
        ma_add(base, Imm32(dest.offset), ScratchRegister);
        base = ScratchRegister;
    }
    ma_strb(src, DTRAddr(base, DtrRegImmShift(dest.index, LSL, scale)));
}

void
MacroAssemblerARMCompat::store16(const Imm32 &imm, const Address &address)
{
    ma_mov(imm, secondScratchReg_);
    store16(secondScratchReg_, address);
}

void
MacroAssemblerARMCompat::store16(const Register &src, const Address &address)
{
    ma_dataTransferN(IsStore, 16, false, address.base, Imm32(address.offset), src);
}

void
MacroAssemblerARMCompat::store16(const Imm32 &imm, const BaseIndex &dest)
{
    ma_mov(imm, secondScratchReg_);
    store16(secondScratchReg_, dest);
}
void
MacroAssemblerARMCompat::store16(const Register &src, const BaseIndex &address)
{
    Register index = address.index;

    // We don't have LSL on index register yet.
    if (address.scale != TimesOne) {
        ma_lsl(Imm32::ShiftOf(address.scale), index, ScratchRegister);
        index = ScratchRegister;
    }

    if (address.offset != 0) {
        ma_add(index, Imm32(address.offset), ScratchRegister);
        index = ScratchRegister;
    }
    ma_strh(src, EDtrAddr(address.base, EDtrOffReg(index)));
}
void
MacroAssemblerARMCompat::store32(const Register &src, const AbsoluteAddress &address)
{
    storePtr(src, address);
}

void
MacroAssemblerARMCompat::store32(const Register &src, const Address &address)
{
    storePtr(src, address);
}

void
MacroAssemblerARMCompat::store32(const Imm32 &src, const Address &address)
{
    move32(src, secondScratchReg_);
    storePtr(secondScratchReg_, address);
}

void
MacroAssemblerARMCompat::store32(const Imm32 &imm, const BaseIndex &dest)
{
    ma_mov(imm, secondScratchReg_);
    store32(secondScratchReg_, dest);
}

void
MacroAssemblerARMCompat::store32(const Register &src, const BaseIndex &dest)
{
    Register base = dest.base;
    uint32_t scale = Imm32::ShiftOf(dest.scale).value;

    if (dest.offset != 0) {
        ma_add(base, Imm32(dest.offset), ScratchRegister);
        base = ScratchRegister;
    }
    ma_str(src, DTRAddr(base, DtrRegImmShift(dest.index, LSL, scale)));
}

void
MacroAssemblerARMCompat::storePtr(ImmWord imm, const Address &address)
{
    movePtr(imm, ScratchRegister);
    storePtr(ScratchRegister, address);
}

void
MacroAssemblerARMCompat::storePtr(ImmGCPtr imm, const Address &address)
{
    movePtr(imm, ScratchRegister);
    storePtr(ScratchRegister, address);
}

void
MacroAssemblerARMCompat::storePtr(Register src, const Address &address)
{
    ma_str(src, Operand(address));
}

void
MacroAssemblerARMCompat::storePtr(const Register &src, const AbsoluteAddress &dest)
{
    movePtr(ImmWord(dest.addr), ScratchRegister);
    storePtr(src, Address(ScratchRegister, 0x0));
}

void
MacroAssemblerARMCompat::cmp32(const Register &lhs, const Imm32 &rhs)
{
    JS_ASSERT(lhs != ScratchRegister);
    ma_cmp(lhs, rhs);
}

void
MacroAssemblerARMCompat::cmp32(const Operand &lhs, const Register &rhs)
{
    ma_cmp(lhs.toReg(), rhs);
}

void
MacroAssemblerARMCompat::cmp32(const Operand &lhs, const Imm32 &rhs)
{
    JS_ASSERT(lhs.toReg() != ScratchRegister);
    ma_cmp(lhs.toReg(), rhs);
}

void
MacroAssemblerARMCompat::cmp32(const Register &lhs, const Register &rhs)
{
    ma_cmp(lhs, rhs);
}

void
MacroAssemblerARMCompat::cmpPtr(const Register &lhs, const ImmWord &rhs)
{
    JS_ASSERT(lhs != ScratchRegister);
    ma_cmp(lhs, Imm32(rhs.value));
}

void
MacroAssemblerARMCompat::cmpPtr(const Register &lhs, const Register &rhs)
{
    ma_cmp(lhs, rhs);
}

void
MacroAssemblerARMCompat::cmpPtr(const Register &lhs, const ImmGCPtr &rhs)
{
    ma_cmp(lhs, rhs);
}

void
MacroAssemblerARMCompat::cmpPtr(const Address &lhs, const Register &rhs)
{
    loadPtr(lhs, ScratchRegister);
    cmpPtr(ScratchRegister, rhs);
}

void
MacroAssemblerARMCompat::cmpPtr(const Address &lhs, const ImmWord &rhs)
{
    loadPtr(lhs, secondScratchReg_);
    ma_cmp(secondScratchReg_, Imm32(rhs.value));
}

void
MacroAssemblerARMCompat::setStackArg(const Register &reg, uint32_t arg)
{
    ma_dataTransferN(IsStore, 32, true, sp, Imm32(arg * STACK_SLOT_SIZE), reg);

}

void
MacroAssemblerARMCompat::subPtr(Imm32 imm, const Register dest)
{
    ma_sub(imm, dest);
}

void
MacroAssemblerARMCompat::subPtr(const Address &addr, const Register dest)
{
    loadPtr(addr, ScratchRegister);
    ma_sub(ScratchRegister, dest);
}

void
MacroAssemblerARMCompat::subPtr(const Register &src, const Register &dest)
{
    ma_sub(src, dest);
}

void
MacroAssemblerARMCompat::addPtr(Imm32 imm, const Register dest)
{
    ma_add(imm, dest);
}

void
MacroAssemblerARMCompat::addPtr(Imm32 imm, const Address &dest)
{
    loadPtr(dest, ScratchRegister);
    addPtr(imm, ScratchRegister);
    storePtr(ScratchRegister, dest);
}

void
MacroAssemblerARMCompat::compareDouble(FloatRegister lhs, FloatRegister rhs)
{
    // Compare the doubles, setting vector status flags.
    if (rhs == InvalidFloatReg)
        ma_vcmpz(lhs);
    else
        ma_vcmp(lhs, rhs);

    // Move vector status bits to normal status flags.
    as_vmrs(pc);
}

void
MacroAssemblerARMCompat::branchDouble(DoubleCondition cond, const FloatRegister &lhs,
                                      const FloatRegister &rhs, Label *label)
{
    compareDouble(lhs, rhs);

    if (cond == DoubleNotEqual) {
        // Force the unordered cases not to jump.
        Label unordered;
        ma_b(&unordered, VFP_Unordered);
        ma_b(label, VFP_NotEqualOrUnordered);
        bind(&unordered);
        return;
    }

    if (cond == DoubleEqualOrUnordered) {
        ma_b(label, VFP_Unordered);
        ma_b(label, VFP_Equal);
        return;
    }

    ma_b(label, ConditionFromDoubleCondition(cond));
}

// higher level tag testing code
Operand ToPayload(Operand base) {
    return Operand(Register::FromCode(base.base()), base.disp());
}
Operand ToType(Operand base) {
    return Operand(Register::FromCode(base.base()), base.disp() + sizeof(void *));
}

Assembler::Condition
MacroAssemblerARMCompat::testInt32(Assembler::Condition cond, const ValueOperand &value)
{
    JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
    ma_cmp(value.typeReg(), ImmType(JSVAL_TYPE_INT32));
    return cond;
}

Assembler::Condition
MacroAssemblerARMCompat::testBoolean(Assembler::Condition cond, const ValueOperand &value)
{
    JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
    ma_cmp(value.typeReg(), ImmType(JSVAL_TYPE_BOOLEAN));
    return cond;
}
Assembler::Condition
MacroAssemblerARMCompat::testDouble(Assembler::Condition cond, const ValueOperand &value)
{
    JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
    Assembler::Condition actual = (cond == Equal) ? Below : AboveOrEqual;
    ma_cmp(value.typeReg(), ImmTag(JSVAL_TAG_CLEAR));
    return actual;
}

Assembler::Condition
MacroAssemblerARMCompat::testNull(Assembler::Condition cond, const ValueOperand &value)
{
    JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
    ma_cmp(value.typeReg(), ImmType(JSVAL_TYPE_NULL));
    return cond;
}

Assembler::Condition
MacroAssemblerARMCompat::testUndefined(Assembler::Condition cond, const ValueOperand &value)
{
    JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
    ma_cmp(value.typeReg(), ImmType(JSVAL_TYPE_UNDEFINED));
    return cond;
}

Assembler::Condition
MacroAssemblerARMCompat::testString(Assembler::Condition cond, const ValueOperand &value)
{
    return testString(cond, value.typeReg());
}

Assembler::Condition
MacroAssemblerARMCompat::testObject(Assembler::Condition cond, const ValueOperand &value)
{
    return testObject(cond, value.typeReg());
}

Assembler::Condition
MacroAssemblerARMCompat::testNumber(Assembler::Condition cond, const ValueOperand &value)
{
    return testNumber(cond, value.typeReg());
}

Assembler::Condition
MacroAssemblerARMCompat::testMagic(Assembler::Condition cond, const ValueOperand &value)
{
    return testMagic(cond, value.typeReg());
}

Assembler::Condition
MacroAssemblerARMCompat::testPrimitive(Assembler::Condition cond, const ValueOperand &value)
{
    return testPrimitive(cond, value.typeReg());
}

// Register-based tests.
Assembler::Condition
MacroAssemblerARMCompat::testInt32(Assembler::Condition cond, const Register &tag)
{
    JS_ASSERT(cond == Equal || cond == NotEqual);
    ma_cmp(tag, ImmTag(JSVAL_TAG_INT32));
    return cond;
}

Assembler::Condition
MacroAssemblerARMCompat::testBoolean(Assembler::Condition cond, const Register &tag)
{
    JS_ASSERT(cond == Equal || cond == NotEqual);
    ma_cmp(tag, ImmTag(JSVAL_TAG_BOOLEAN));
    return cond;
}

Assembler::Condition
MacroAssemblerARMCompat::testNull(Assembler::Condition cond, const Register &tag) {
    JS_ASSERT(cond == Equal || cond == NotEqual);
    ma_cmp(tag, ImmTag(JSVAL_TAG_NULL));
    return cond;
}

Assembler::Condition
MacroAssemblerARMCompat::testUndefined(Assembler::Condition cond, const Register &tag) {
    JS_ASSERT(cond == Equal || cond == NotEqual);
    ma_cmp(tag, ImmTag(JSVAL_TAG_UNDEFINED));
    return cond;
}

Assembler::Condition
MacroAssemblerARMCompat::testString(Assembler::Condition cond, const Register &tag) {
    JS_ASSERT(cond == Equal || cond == NotEqual);
    ma_cmp(tag, ImmTag(JSVAL_TAG_STRING));
    return cond;
}

Assembler::Condition
MacroAssemblerARMCompat::testObject(Assembler::Condition cond, const Register &tag)
{
    JS_ASSERT(cond == Equal || cond == NotEqual);
    ma_cmp(tag, ImmTag(JSVAL_TAG_OBJECT));
    return cond;
}

Assembler::Condition
MacroAssemblerARMCompat::testMagic(Assembler::Condition cond, const Register &tag)
{
    JS_ASSERT(cond == Equal || cond == NotEqual);
    ma_cmp(tag, ImmTag(JSVAL_TAG_MAGIC));
    return cond;
}

Assembler::Condition
MacroAssemblerARMCompat::testPrimitive(Assembler::Condition cond, const Register &tag)
{
    JS_ASSERT(cond == Equal || cond == NotEqual);
    ma_cmp(tag, ImmTag(JSVAL_UPPER_EXCL_TAG_OF_PRIMITIVE_SET));
    return cond == Equal ? Below : AboveOrEqual;
}

Assembler::Condition
MacroAssemblerARMCompat::testGCThing(Assembler::Condition cond, const Address &address)
{
    JS_ASSERT(cond == Equal || cond == NotEqual);
    extractTag(address, ScratchRegister);
    ma_cmp(ScratchRegister, ImmTag(JSVAL_LOWER_INCL_TAG_OF_GCTHING_SET));
    return cond == Equal ? AboveOrEqual : Below;
}

Assembler::Condition
MacroAssemblerARMCompat::testMagic(Assembler::Condition cond, const Address &address)
{
    JS_ASSERT(cond == Equal || cond == NotEqual);
    extractTag(address, ScratchRegister);
    ma_cmp(ScratchRegister, ImmTag(JSVAL_TAG_MAGIC));
    return cond;
}

Assembler::Condition
MacroAssemblerARMCompat::testInt32(Assembler::Condition cond, const Address &address)
{
    JS_ASSERT(cond == Equal || cond == NotEqual);
    extractTag(address, ScratchRegister);
    ma_cmp(ScratchRegister, ImmTag(JSVAL_TAG_INT32));
    return cond;
}

Assembler::Condition
MacroAssemblerARMCompat::testDouble(Assembler::Condition cond, const Address &address)
{
    JS_ASSERT(cond == Equal || cond == NotEqual);
    extractTag(address, ScratchRegister);
    return testDouble(cond, ScratchRegister);
}

Assembler::Condition
MacroAssemblerARMCompat::testDouble(Condition cond, const Register &tag)
{
    JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
    Condition actual = (cond == Equal) ? Below : AboveOrEqual;
    ma_cmp(tag, ImmTag(JSVAL_TAG_CLEAR));
    return actual;
}

Assembler::Condition
MacroAssemblerARMCompat::testNumber(Condition cond, const Register &tag)
{
    JS_ASSERT(cond == Equal || cond == NotEqual);
    ma_cmp(tag, ImmTag(JSVAL_UPPER_INCL_TAG_OF_NUMBER_SET));
    return cond == Equal ? BelowOrEqual : Above;
}

Assembler::Condition
MacroAssemblerARMCompat::testUndefined(Condition cond, const BaseIndex &src)
{
    JS_ASSERT(cond == Equal || cond == NotEqual);
    extractTag(src, ScratchRegister);
    ma_cmp(ScratchRegister, ImmTag(JSVAL_TAG_UNDEFINED));
    return cond;
}

Assembler::Condition
MacroAssemblerARMCompat::testNull(Condition cond, const BaseIndex &src)
{
    JS_ASSERT(cond == Equal || cond == NotEqual);
    extractTag(src, ScratchRegister);
    ma_cmp(ScratchRegister, ImmTag(JSVAL_TAG_NULL));
    return cond;
}

Assembler::Condition
MacroAssemblerARMCompat::testBoolean(Condition cond, const BaseIndex &src)
{
    JS_ASSERT(cond == Equal || cond == NotEqual);
    extractTag(src, ScratchRegister);
    ma_cmp(ScratchRegister, ImmTag(JSVAL_TAG_BOOLEAN));
    return cond;
}

Assembler::Condition
MacroAssemblerARMCompat::testString(Condition cond, const BaseIndex &src)
{
    JS_ASSERT(cond == Equal || cond == NotEqual);
    extractTag(src, ScratchRegister);
    ma_cmp(ScratchRegister, ImmTag(JSVAL_TAG_STRING));
    return cond;
}

Assembler::Condition
MacroAssemblerARMCompat::testInt32(Condition cond, const BaseIndex &src)
{
    JS_ASSERT(cond == Equal || cond == NotEqual);
    extractTag(src, ScratchRegister);
    ma_cmp(ScratchRegister, ImmTag(JSVAL_TAG_INT32));
    return cond;
}

Assembler::Condition
MacroAssemblerARMCompat::testObject(Condition cond, const BaseIndex &src)
{
    JS_ASSERT(cond == Equal || cond == NotEqual);
    extractTag(src, ScratchRegister);
    ma_cmp(ScratchRegister, ImmTag(JSVAL_TAG_OBJECT));
    return cond;
}

Assembler::Condition
MacroAssemblerARMCompat::testDouble(Condition cond, const BaseIndex &src)
{
    JS_ASSERT(cond == Equal || cond == NotEqual);
    Assembler::Condition actual = (cond == Equal) ? Below : AboveOrEqual;
    extractTag(src, ScratchRegister);
    ma_cmp(ScratchRegister, ImmTag(JSVAL_TAG_CLEAR));
    return actual;
}

Assembler::Condition
MacroAssemblerARMCompat::testMagic(Condition cond, const BaseIndex &address)
{
    JS_ASSERT(cond == Equal || cond == NotEqual);
    extractTag(address, ScratchRegister);
    ma_cmp(ScratchRegister, ImmTag(JSVAL_TAG_MAGIC));
    return cond;
}

Assembler::Condition
MacroAssemblerARMCompat::testGCThing(Condition cond, const BaseIndex &address)
{
    JS_ASSERT(cond == Equal || cond == NotEqual);
    extractTag(address, ScratchRegister);
    ma_cmp(ScratchRegister, ImmTag(JSVAL_LOWER_INCL_TAG_OF_GCTHING_SET));
    return cond == Equal ? AboveOrEqual : Below;
}

void
MacroAssemblerARMCompat::branchTestValue(Condition cond, const ValueOperand &value, const Value &v,
                                         Label *label)
{
    // If cond == NotEqual, branch when a.payload != b.payload || a.tag != b.tag.
    // If the payloads are equal, compare the tags. If the payloads are not equal,
    // short circuit true (NotEqual).
    //
    // If cand == Equal, branch when a.payload == b.payload && a.tag == b.tag.
    // If the payloads are equal, compare the tags. If the payloads are not equal,
    // short circuit false (NotEqual).
    jsval_layout jv = JSVAL_TO_IMPL(v);
    if (v.isMarkable())
        ma_cmp(value.payloadReg(), ImmGCPtr(reinterpret_cast<gc::Cell *>(v.toGCThing())));
    else
        ma_cmp(value.payloadReg(), Imm32(jv.s.payload.i32));
    ma_cmp(value.typeReg(), Imm32(jv.s.tag), Equal);
    ma_b(label, cond);
}

void
MacroAssemblerARMCompat::branchTestValue(Condition cond, const Address &valaddr,
                                         const ValueOperand &value, Label *label)
{
    JS_ASSERT(cond == Equal || cond == NotEqual);

    ma_ldr(tagOf(valaddr), ScratchRegister);
    branchPtr(cond, ScratchRegister, value.typeReg(), label);

    ma_ldr(payloadOf(valaddr), ScratchRegister);
    branchPtr(cond, ScratchRegister, value.payloadReg(), label);
}

// unboxing code
void
MacroAssemblerARMCompat::unboxInt32(const ValueOperand &operand, const Register &dest)
{
    ma_mov(operand.payloadReg(), dest);
}

void
MacroAssemblerARMCompat::unboxInt32(const Address &src, const Register &dest)
{
    ma_ldr(payloadOf(src), dest);
}

void
MacroAssemblerARMCompat::unboxBoolean(const ValueOperand &operand, const Register &dest)
{
    ma_mov(operand.payloadReg(), dest);
}

void
MacroAssemblerARMCompat::unboxBoolean(const Address &src, const Register &dest)
{
    ma_ldr(payloadOf(src), dest);
}

void
MacroAssemblerARMCompat::unboxDouble(const ValueOperand &operand, const FloatRegister &dest)
{
    JS_ASSERT(dest != ScratchFloatReg);
    as_vxfer(operand.payloadReg(), operand.typeReg(),
             VFPRegister(dest), CoreToFloat);
}

void
MacroAssemblerARMCompat::unboxDouble(const Address &src, const FloatRegister &dest)
{
    ma_vldr(Operand(src), dest);
}

void
MacroAssemblerARMCompat::unboxValue(const ValueOperand &src, AnyRegister dest)
{
    if (dest.isFloat()) {
        Label notInt32, end;
        branchTestInt32(Assembler::NotEqual, src, &notInt32);
        convertInt32ToDouble(src.payloadReg(), dest.fpu());
        ma_b(&end);
        bind(&notInt32);
        unboxDouble(src, dest.fpu());
        bind(&end);
    } else if (src.payloadReg() != dest.gpr()) {
        as_mov(dest.gpr(), O2Reg(src.payloadReg()));
    }
}

void
MacroAssemblerARMCompat::unboxPrivate(const ValueOperand &src, Register dest)
{
    ma_mov(src.payloadReg(), dest);
}

void
MacroAssemblerARMCompat::boxDouble(const FloatRegister &src, const ValueOperand &dest)
{
    as_vxfer(dest.payloadReg(), dest.typeReg(), VFPRegister(src), FloatToCore);
}

void
MacroAssemblerARMCompat::boxNonDouble(JSValueType type, const Register &src, const ValueOperand &dest) {
    if (src != dest.payloadReg())
        ma_mov(src, dest.payloadReg());
    ma_mov(ImmType(type), dest.typeReg());
}

void
MacroAssemblerARMCompat::boolValueToDouble(const ValueOperand &operand, const FloatRegister &dest)
{
    VFPRegister d = VFPRegister(dest);
    ma_vimm(1.0, dest);
    ma_cmp(operand.payloadReg(), Imm32(0));
    // If the source is 0, then subtract the dest from itself, producing 0.
    as_vsub(d, d, d, Equal);
}

void
MacroAssemblerARMCompat::int32ValueToDouble(const ValueOperand &operand, const FloatRegister &dest)
{
    // transfer the integral value to a floating point register
    VFPRegister vfpdest = VFPRegister(dest);
    as_vxfer(operand.payloadReg(), InvalidReg,
             vfpdest.sintOverlay(), CoreToFloat);
    // convert the value to a double.
    as_vcvt(vfpdest, vfpdest.sintOverlay());
}

void
MacroAssemblerARMCompat::loadInt32OrDouble(const Operand &src, const FloatRegister &dest)
{
    Label notInt32, end;
    // If it's an int, convert it to double.
    ma_ldr(ToType(src), ScratchRegister);
    branchTestInt32(Assembler::NotEqual, ScratchRegister, &notInt32);
    ma_ldr(ToPayload(src), ScratchRegister);
    convertInt32ToDouble(ScratchRegister, dest);
    ma_b(&end);

    // Not an int, just load as double.
    bind(&notInt32);
    ma_vldr(src, dest);
    bind(&end);
}

void
MacroAssemblerARMCompat::loadInt32OrDouble(Register base, Register index, const FloatRegister &dest, int32_t shift)
{
    Label notInt32, end;

    JS_STATIC_ASSERT(NUNBOX32_PAYLOAD_OFFSET == 0);

    // If it's an int, convert it to double.
    ma_alu(base, lsl(index, shift), ScratchRegister, op_add);

    // Since we only have one scratch register, we need to stomp over it with the tag
    ma_ldr(Address(ScratchRegister, NUNBOX32_TYPE_OFFSET), ScratchRegister);
    branchTestInt32(Assembler::NotEqual, ScratchRegister, &notInt32);

    // Implicitly requires NUNBOX32_PAYLOAD_OFFSET == 0: no offset provided
    ma_ldr(DTRAddr(base, DtrRegImmShift(index, LSL, shift)), ScratchRegister);
    convertInt32ToDouble(ScratchRegister, dest);
    ma_b(&end);

    // Not an int, just load as double.
    bind(&notInt32);
    // First, recompute the offset that had been stored in the scratch register
    // since the scratch register was overwritten loading in the type.
    ma_alu(base, lsl(index, shift), ScratchRegister, op_add);
    ma_vldr(Address(ScratchRegister, 0), dest);
    bind(&end);
}

void
MacroAssemblerARMCompat::loadConstantDouble(double dp, const FloatRegister &dest)
{
    as_FImm64Pool(dest, dp);
}

void
MacroAssemblerARMCompat::loadStaticDouble(const double *dp, const FloatRegister &dest)
{
    loadConstantDouble(*dp, dest);
}
    // treat the value as a boolean, and set condition codes accordingly

Assembler::Condition
MacroAssemblerARMCompat::testInt32Truthy(bool truthy, const ValueOperand &operand)
{
    ma_tst(operand.payloadReg(), operand.payloadReg());
    return truthy ? NonZero : Zero;
}

Assembler::Condition
MacroAssemblerARMCompat::testBooleanTruthy(bool truthy, const ValueOperand &operand)
{
    ma_tst(operand.payloadReg(), operand.payloadReg());
    return truthy ? NonZero : Zero;
}

Assembler::Condition
MacroAssemblerARMCompat::testDoubleTruthy(bool truthy, const FloatRegister &reg)
{
    as_vcmpz(VFPRegister(reg));
    as_vmrs(pc);
    as_cmp(r0, O2Reg(r0), Overflow);
    return truthy ? NonZero : Zero;
}

Register
MacroAssemblerARMCompat::extractObject(const Address &address, Register scratch)
{
    ma_ldr(payloadOf(address), scratch);
    return scratch;
}

Register
MacroAssemblerARMCompat::extractTag(const Address &address, Register scratch)
{
    ma_ldr(tagOf(address), scratch);
    return scratch;
}

Register
MacroAssemblerARMCompat::extractTag(const BaseIndex &address, Register scratch)
{
    ma_alu(address.base, lsl(address.index, address.scale), scratch, op_add, NoSetCond);
    return extractTag(Address(scratch, address.offset), scratch);
}

void
MacroAssemblerARMCompat::moveValue(const Value &val, Register type, Register data)
{
    jsval_layout jv = JSVAL_TO_IMPL(val);
    ma_mov(Imm32(jv.s.tag), type);
    if (val.isMarkable())
        ma_mov(ImmGCPtr(reinterpret_cast<gc::Cell *>(val.toGCThing())), data);
    else
        ma_mov(Imm32(jv.s.payload.i32), data);
}
void
MacroAssemblerARMCompat::moveValue(const Value &val, const ValueOperand &dest)
{
    moveValue(val, dest.typeReg(), dest.payloadReg());
}

/////////////////////////////////////////////////////////////////
// X86/X64-common (ARM too now) interface.
/////////////////////////////////////////////////////////////////
void
MacroAssemblerARMCompat::storeValue(ValueOperand val, Operand dst)
{
    ma_str(val.payloadReg(), ToPayload(dst));
    ma_str(val.typeReg(), ToType(dst));
}

void
MacroAssemblerARMCompat::storeValue(ValueOperand val, const BaseIndex &dest)
{
    if (isValueDTRDCandidate(val) && Abs(dest.offset) <= 255) {
        Register tmpIdx;
        if (dest.offset == 0) {
            if (dest.scale == TimesOne) {
                tmpIdx = dest.index;
            } else {
                ma_lsl(Imm32(dest.scale), dest.index, ScratchRegister);
                tmpIdx = ScratchRegister;
            }
            ma_strd(val.payloadReg(), val.typeReg(), EDtrAddr(dest.base, EDtrOffReg(tmpIdx)));
        } else {
            ma_alu(dest.base, lsl(dest.index, dest.scale), ScratchRegister, op_add);
            ma_strd(val.payloadReg(), val.typeReg(),
                    EDtrAddr(ScratchRegister, EDtrOffImm(dest.offset)));
        }
    } else {
        ma_alu(dest.base, lsl(dest.index, dest.scale), ScratchRegister, op_add);
        storeValue(val, Address(ScratchRegister, dest.offset));
    }
}

void
MacroAssemblerARMCompat::loadValue(const BaseIndex &addr, ValueOperand val)
{
    if (isValueDTRDCandidate(val) && Abs(addr.offset) <= 255) {
        Register tmpIdx;
        if (addr.offset == 0) {
            if (addr.scale == TimesOne) {
                tmpIdx = addr.index;
            } else {
                ma_lsl(Imm32(addr.scale), addr.index, ScratchRegister);
                tmpIdx = ScratchRegister;
            }
            ma_ldrd(EDtrAddr(addr.base, EDtrOffReg(tmpIdx)), val.payloadReg(), val.typeReg());
        } else {
            ma_alu(addr.base, lsl(addr.index, addr.scale), ScratchRegister, op_add);
            ma_ldrd(EDtrAddr(ScratchRegister, EDtrOffImm(addr.offset)),
                    val.payloadReg(), val.typeReg());
        }
    } else {
        ma_alu(addr.base, lsl(addr.index, addr.scale), ScratchRegister, op_add);
        loadValue(Address(ScratchRegister, addr.offset), val);
    }
}

void
MacroAssemblerARMCompat::loadValue(Address src, ValueOperand val)
{
    Operand srcOp = Operand(src);
    Operand payload = ToPayload(srcOp);
    Operand type = ToType(srcOp);
    // TODO: copy this code into a generic function that acts on all sequences of memory accesses
    if (isValueDTRDCandidate(val)) {
        // If the value we want is in two consecutive registers starting with an even register,
        // they can be combined as a single ldrd.
        int offset = srcOp.disp();
        if (offset < 256 && offset > -256) {
            ma_ldrd(EDtrAddr(Register::FromCode(srcOp.base()), EDtrOffImm(srcOp.disp())), val.payloadReg(), val.typeReg());
            return;
        }
    }
    // if the value is lower than the type, then we may be able to use an ldm instruction

    if (val.payloadReg().code() < val.typeReg().code()) {
        if (srcOp.disp() <= 4 && srcOp.disp() >= -8 && (srcOp.disp() & 3) == 0) {
            // turns out each of the 4 value -8, -4, 0, 4 corresponds exactly with one of
            // LDM{DB, DA, IA, IB}
            DTMMode mode;
            switch(srcOp.disp()) {
              case -8:
                mode = DB;
                break;
              case -4:
                mode = DA;
                break;
              case 0:
                mode = IA;
                break;
              case 4:
                mode = IB;
                break;
              default:
                MOZ_ASSUME_UNREACHABLE("Bogus Offset for LoadValue as DTM");
            }
            startDataTransferM(IsLoad, Register::FromCode(srcOp.base()), mode);
            transferReg(val.payloadReg());
            transferReg(val.typeReg());
            finishDataTransfer();
            return;
        }
    }
    // Ensure that loading the payload does not erase the pointer to the
    // Value in memory.
    if (Register::FromCode(type.base()) != val.payloadReg()) {
        ma_ldr(payload, val.payloadReg());
        ma_ldr(type, val.typeReg());
    } else {
        ma_ldr(type, val.typeReg());
        ma_ldr(payload, val.payloadReg());
    }
}

void
MacroAssemblerARMCompat::tagValue(JSValueType type, Register payload, ValueOperand dest)
{
    JS_ASSERT(payload != dest.typeReg());
    ma_mov(ImmType(type), dest.typeReg());
    if (payload != dest.payloadReg())
        ma_mov(payload, dest.payloadReg());
}

void
MacroAssemblerARMCompat::pushValue(ValueOperand val) {
    ma_push(val.typeReg());
    ma_push(val.payloadReg());
}
void
MacroAssemblerARMCompat::pushValue(const Address &addr)
{
    JS_ASSERT(addr.base != StackPointer);
    Operand srcOp = Operand(addr);
    Operand payload = ToPayload(srcOp);
    Operand type = ToType(srcOp);

    ma_ldr(type, ScratchRegister);
    ma_push(ScratchRegister);
    ma_ldr(payload, ScratchRegister);
    ma_push(ScratchRegister);
}

void
MacroAssemblerARMCompat::popValue(ValueOperand val) {
    ma_pop(val.payloadReg());
    ma_pop(val.typeReg());
}
void
MacroAssemblerARMCompat::storePayload(const Value &val, Operand dest)
{
    jsval_layout jv = JSVAL_TO_IMPL(val);
    if (val.isMarkable())
        ma_mov(ImmGCPtr((gc::Cell *)jv.s.payload.ptr), secondScratchReg_);
    else
        ma_mov(Imm32(jv.s.payload.i32), secondScratchReg_);
    ma_str(secondScratchReg_, ToPayload(dest));
}
void
MacroAssemblerARMCompat::storePayload(Register src, Operand dest)
{
    if (dest.getTag() == Operand::MEM) {
        ma_str(src, ToPayload(dest));
        return;
    }
    MOZ_ASSUME_UNREACHABLE("why do we do all of these things?");

}

void
MacroAssemblerARMCompat::storePayload(const Value &val, Register base, Register index, int32_t shift)
{
    jsval_layout jv = JSVAL_TO_IMPL(val);
    if (val.isMarkable())
        ma_mov(ImmGCPtr((gc::Cell *)jv.s.payload.ptr), ScratchRegister);
    else
        ma_mov(Imm32(jv.s.payload.i32), ScratchRegister);
    JS_STATIC_ASSERT(NUNBOX32_PAYLOAD_OFFSET == 0);
    // If NUNBOX32_PAYLOAD_OFFSET is not zero, the memory operand [base + index << shift + imm]
    // cannot be encoded into a single instruction, and cannot be integrated into the as_dtr call.
    as_dtr(IsStore, 32, Offset, ScratchRegister, DTRAddr(base, DtrRegImmShift(index, LSL, shift)));
}
void
MacroAssemblerARMCompat::storePayload(Register src, Register base, Register index, int32_t shift)
{
    JS_ASSERT((shift < 32) && (shift >= 0));
    // If NUNBOX32_PAYLOAD_OFFSET is not zero, the memory operand [base + index << shift + imm]
    // cannot be encoded into a single instruction, and cannot be integrated into the as_dtr call.
    JS_STATIC_ASSERT(NUNBOX32_PAYLOAD_OFFSET == 0);
    // Technically, shift > -32 can be handle by changing LSL to ASR, but should never come up,
    // and this is one less code path to get wrong.
    as_dtr(IsStore, 32, Offset, src, DTRAddr(base, DtrRegImmShift(index, LSL, shift)));
}

void
MacroAssemblerARMCompat::storeTypeTag(ImmTag tag, Operand dest) {
    if (dest.getTag() == Operand::MEM) {
        ma_mov(tag, secondScratchReg_);
        ma_str(secondScratchReg_, ToType(dest));
        return;
    }

    MOZ_ASSUME_UNREACHABLE("why do we do all of these things?");

}

void
MacroAssemblerARMCompat::storeTypeTag(ImmTag tag, Register base, Register index, int32_t shift) {
    JS_ASSERT(base != ScratchRegister);
    JS_ASSERT(index != ScratchRegister);
    // A value needs to be store a value int base + index << shift + 4.
    // Arm cannot handle this in a single operand, so a temp register is required.
    // However, the scratch register is presently in use to hold the immediate that
    // is being stored into said memory location. Work around this by modifying
    // the base so the valid [base + index << shift] format can be used, then
    // restore it.
    ma_add(base, Imm32(NUNBOX32_TYPE_OFFSET), base);
    ma_mov(tag, ScratchRegister);
    ma_str(ScratchRegister, DTRAddr(base, DtrRegImmShift(index, LSL, shift)));
    ma_sub(base, Imm32(NUNBOX32_TYPE_OFFSET), base);
}

void
MacroAssemblerARMCompat::linkExitFrame() {
    uint8_t *dest = ((uint8_t*)GetIonContext()->runtime) + offsetof(JSRuntime, mainThread.ionTop);
    movePtr(ImmWord(dest), ScratchRegister);
    ma_str(StackPointer, Operand(ScratchRegister, 0));
}

void
MacroAssemblerARMCompat::linkParallelExitFrame(const Register &pt)
{
    ma_str(StackPointer, Operand(pt, offsetof(PerThreadData, ionTop)));
}

// ARM says that all reads of pc will return 8 higher than the
// address of the currently executing instruction.  This means we are
// correctly storing the address of the instruction after the call
// in the register.
// Also ION is breaking the ARM EABI here (sort of). The ARM EABI
// says that a function call should move the pc into the link register,
// then branch to the function, and *sp is data that is owned by the caller,
// not the callee.  The ION ABI says *sp should be the address that
// we will return to when leaving this function
void
MacroAssemblerARM::ma_callIon(const Register r)
{
    // When the stack is 8 byte aligned,
    // we want to decrement sp by 8, and write pc+8 into the new sp.
    // when we return from this call, sp will be its present value minus 4.
    AutoForbidPools afp(this);
    as_dtr(IsStore, 32, PreIndex, pc, DTRAddr(sp, DtrOffImm(-8)));
    as_blx(r);
}
void
MacroAssemblerARM::ma_callIonNoPush(const Register r)
{
    // Since we just write the return address into the stack, which is
    // popped on return, the net effect is removing 4 bytes from the stack
    AutoForbidPools afp(this);
    as_dtr(IsStore, 32, Offset, pc, DTRAddr(sp, DtrOffImm(0)));
    as_blx(r);
}

void
MacroAssemblerARM::ma_callIonHalfPush(const Register r)
{
    // The stack is unaligned by 4 bytes.
    // We push the pc to the stack to align the stack before the call, when we
    // return the pc is poped and the stack is restored to its unaligned state.
    AutoForbidPools afp(this);
    ma_push(pc);
    as_blx(r);
}

void
MacroAssemblerARM::ma_call(void *dest)
{
    RelocStyle rs;
    if (hasMOVWT())
        rs = L_MOVWT;
    else
        rs = L_LDR;

    ma_movPatchable(Imm32((uint32_t) dest), CallReg, Always, rs);
    as_blx(CallReg);
}

void
MacroAssemblerARMCompat::breakpoint()
{
    as_bkpt();
}

void
MacroAssemblerARMCompat::ensureDouble(const ValueOperand &source, FloatRegister dest, Label *failure)
{
    Label isDouble, done;
    branchTestDouble(Assembler::Equal, source.typeReg(), &isDouble);
    branchTestInt32(Assembler::NotEqual, source.typeReg(), failure);

    convertInt32ToDouble(source.payloadReg(), dest);
    jump(&done);

    bind(&isDouble);
    unboxDouble(source, dest);

    bind(&done);
}

void
MacroAssemblerARMCompat::breakpoint(Condition cc)
{
    ma_ldr(DTRAddr(r12, DtrRegImmShift(r12, LSL, 0, IsDown)), r12, Offset, cc);
}

void
MacroAssemblerARMCompat::setupABICall(uint32_t args)
{
    JS_ASSERT(!inCall_);
    inCall_ = true;
    args_ = args;
    passedArgs_ = 0;
#ifdef JS_CPU_ARM_HARDFP
    usedIntSlots_ = 0;
    usedFloatSlots_ = 0;
    padding_ = 0;
#else
    usedSlots_ = 0;
#endif
    floatArgsInGPR[0] = MoveOperand();
    floatArgsInGPR[1] = MoveOperand();
    floatArgsInGPRValid[0] = false;
    floatArgsInGPRValid[1] = false;
}

void
MacroAssemblerARMCompat::setupAlignedABICall(uint32_t args)
{
    setupABICall(args);

    dynamicAlignment_ = false;
}

void
MacroAssemblerARMCompat::setupUnalignedABICall(uint32_t args, const Register &scratch)
{
    setupABICall(args);
    dynamicAlignment_ = true;

    ma_mov(sp, scratch);

    // Force sp to be aligned
    ma_and(Imm32(~(StackAlignment - 1)), sp, sp);
    ma_push(scratch);
}
#ifdef JS_CPU_ARM_HARDFP
void
MacroAssemblerARMCompat::passABIArg(const MoveOperand &from)
{
    MoveOperand to;
    ++passedArgs_;
    if (!enoughMemory_)
        return;
    if (from.isDouble()) {
        FloatRegister fr;
        if (GetFloatArgReg(usedIntSlots_, usedFloatSlots_, &fr)) {
            if (!from.isFloatReg() || from.floatReg() != fr) {
                enoughMemory_ = moveResolver_.addMove(from, MoveOperand(fr), Move::DOUBLE);
            }
            // else nothing to do; the value is in the right register already
        } else {
            // If (and only if) the integer registers have started spilling, do we
            // need to take the double register's alignment into account
            uint32_t disp = GetFloatArgStackDisp(usedIntSlots_, usedFloatSlots_, &padding_);
            enoughMemory_ = moveResolver_.addMove(from, MoveOperand(sp, disp), Move::DOUBLE);
        }
        usedFloatSlots_++;
    } else {
        Register r;
        if (GetIntArgReg(usedIntSlots_, usedFloatSlots_, &r)) {
            if (!from.isGeneralReg() || from.reg() != r) {
                enoughMemory_ = moveResolver_.addMove(from, MoveOperand(r), Move::GENERAL);
            }
            // else nothing to do; the value is in the right register already
        } else {
            uint32_t disp = GetIntArgStackDisp(usedIntSlots_, usedFloatSlots_, &padding_);
            enoughMemory_ = moveResolver_.addMove(from, MoveOperand(sp, disp), Move::GENERAL);
        }
        usedIntSlots_++;
    }

}

#else
void
MacroAssemblerARMCompat::passABIArg(const MoveOperand &from)
{
    MoveOperand to;
    uint32_t increment = 1;
    bool useResolver = true;
    ++passedArgs_;
    Move::Kind kind = Move::GENERAL;
    if (from.isDouble()) {
        // Double arguments need to be rounded up to the nearest doubleword
        // boundary, even if it is in a register!
        usedSlots_ = (usedSlots_ + 1) & ~1;
        increment = 2;
        kind = Move::DOUBLE;
    }

    Register destReg;
    MoveOperand dest;
    if (GetIntArgReg(usedSlots_, 0, &destReg)) {
        if (from.isDouble()) {
            floatArgsInGPR[destReg.code() >> 1] = from;
            floatArgsInGPRValid[destReg.code() >> 1] = true;
            useResolver = false;
        } else if (from.isGeneralReg() && from.reg() == destReg) {
            // No need to move anything
            useResolver = false;
        } else {
            dest = MoveOperand(destReg);
        }
    } else {
        uint32_t disp = GetArgStackDisp(usedSlots_);
        dest = MoveOperand(sp, disp);
    }

    if (useResolver)
        enoughMemory_ = enoughMemory_ && moveResolver_.addMove(from, dest, kind);
    usedSlots_ += increment;
}
#endif

void
MacroAssemblerARMCompat::passABIArg(const Register &reg)
{
    passABIArg(MoveOperand(reg));
}

void
MacroAssemblerARMCompat::passABIArg(const FloatRegister &freg)
{
    passABIArg(MoveOperand(freg));
}

void MacroAssemblerARMCompat::checkStackAlignment()
{
#ifdef DEBUG
    ma_tst(sp, Imm32(StackAlignment - 1));
    breakpoint(NonZero);
#endif
}

void
MacroAssemblerARMCompat::callWithABIPre(uint32_t *stackAdjust)
{
    JS_ASSERT(inCall_);
#ifdef JS_CPU_ARM_HARDFP
    *stackAdjust = ((usedIntSlots_ > NumIntArgRegs) ? usedIntSlots_ - NumIntArgRegs : 0) * STACK_SLOT_SIZE;
    *stackAdjust += 2*((usedFloatSlots_ > NumFloatArgRegs) ? usedFloatSlots_ - NumFloatArgRegs : 0) * STACK_SLOT_SIZE;
#else
    *stackAdjust = ((usedSlots_ > NumIntArgRegs) ? usedSlots_ - NumIntArgRegs : 0) * STACK_SLOT_SIZE;
#endif
    if (!dynamicAlignment_) {
        *stackAdjust += ComputeByteAlignment(framePushed_ + *stackAdjust, StackAlignment);
    } else {
        // STACK_SLOT_SIZE account for the saved stack pointer pushed by setupUnalignedABICall
        *stackAdjust += ComputeByteAlignment(*stackAdjust + STACK_SLOT_SIZE, StackAlignment);
    }

    reserveStack(*stackAdjust);

    // Position all arguments.
    {
        enoughMemory_ = enoughMemory_ && moveResolver_.resolve();
        if (!enoughMemory_)
            return;

        MoveEmitter emitter(*this);
        emitter.emit(moveResolver_);
        emitter.finish();
    }
    for (int i = 0; i < 2; i++) {
        if (floatArgsInGPRValid[i]) {
            MoveOperand from = floatArgsInGPR[i];
            Register to0 = Register::FromCode(i * 2), to1 = Register::FromCode(i * 2 + 1);

            if (from.isFloatReg()) {
                ma_vxfer(VFPRegister(from.floatReg()), to0, to1);
            } else {
                JS_ASSERT(from.isFloatAddress());
                // Note: We can safely use the MoveOperand's displacement here,
                // even if the base is SP: MoveEmitter::toOperand adjusts
                // SP-relative operands by the difference between the current
                // stack usage and stackAdjust, which emitter.finish() resets
                // to 0.
                //
                // Warning: if the offset isn't within [-255,+255] then this
                // will assert-fail (or, if non-debug, load the wrong words).
                // Nothing uses such an offset at the time of this writing.
                ma_ldrd(EDtrAddr(from.base(), EDtrOffImm(from.disp())), to0, to1);
            }
        }
    }
    checkStackAlignment();

    // Save the lr register if we need to preserve it.
    if (secondScratchReg_ != lr)
        ma_mov(lr, secondScratchReg_);
}

void
MacroAssemblerARMCompat::callWithABIPost(uint32_t stackAdjust, Result result)
{
    if (secondScratchReg_ != lr)
        ma_mov(secondScratchReg_, lr);

    if (result == DOUBLE) {
#ifdef JS_CPU_ARM_HARDFP
        as_vmov(ReturnFloatReg, d0);
#else
        // Move double from r0/r1 to ReturnFloatReg.
        as_vxfer(r0, r1, ReturnFloatReg, CoreToFloat);
#endif
    }

    freeStack(stackAdjust);

    if (dynamicAlignment_) {
        // x86 supports pop esp.  on arm, that isn't well defined, so just
        // do it manually
        as_dtr(IsLoad, 32, Offset, sp, DTRAddr(sp, DtrOffImm(0)));
    }

    JS_ASSERT(inCall_);
    inCall_ = false;
}

void
MacroAssemblerARMCompat::callWithABI(void *fun, Result result)
{
    uint32_t stackAdjust;
    callWithABIPre(&stackAdjust);
    ma_call(fun);
    callWithABIPost(stackAdjust, result);
}

void
MacroAssemblerARMCompat::callWithABI(const Address &fun, Result result)
{
    // Load the callee in r12, no instruction between the ldr and call
    // should clobber it. Note that we can't use fun.base because it may
    // be one of the IntArg registers clobbered before the call.
    ma_ldr(fun, r12);
    uint32_t stackAdjust;
    callWithABIPre(&stackAdjust);
    call(r12);
    callWithABIPost(stackAdjust, result);
}

void
MacroAssemblerARMCompat::handleFailureWithHandler(void *handler)
{
    // Reserve space for exception information.
    int size = (sizeof(ResumeFromException) + 7) & ~7;
    ma_sub(Imm32(size), sp);
    ma_mov(sp, r0);

    // Ask for an exception handler.
    setupUnalignedABICall(1, r1);
    passABIArg(r0);
    callWithABI(handler);

    IonCode *excTail = GetIonContext()->runtime->ionRuntime()->getExceptionTail();
    branch(excTail);
}

void
MacroAssemblerARMCompat::handleFailureWithHandlerTail()
{
    Label entryFrame;
    Label catch_;
    Label finally;
    Label return_;
    Label bailout;

    ma_ldr(Operand(sp, offsetof(ResumeFromException, kind)), r0);
    branch32(Assembler::Equal, r0, Imm32(ResumeFromException::RESUME_ENTRY_FRAME), &entryFrame);
    branch32(Assembler::Equal, r0, Imm32(ResumeFromException::RESUME_CATCH), &catch_);
    branch32(Assembler::Equal, r0, Imm32(ResumeFromException::RESUME_FINALLY), &finally);
    branch32(Assembler::Equal, r0, Imm32(ResumeFromException::RESUME_FORCED_RETURN), &return_);
    branch32(Assembler::Equal, r0, Imm32(ResumeFromException::RESUME_BAILOUT), &bailout);

    breakpoint(); // Invalid kind.

    // No exception handler. Load the error value, load the new stack pointer
    // and return from the entry frame.
    bind(&entryFrame);
    moveValue(MagicValue(JS_ION_ERROR), JSReturnOperand);
    ma_ldr(Operand(sp, offsetof(ResumeFromException, stackPointer)), sp);

    // We're going to be returning by the ion calling convention, which returns
    // by ??? (for now, I think ldr pc, [sp]!)
    as_dtr(IsLoad, 32, PostIndex, pc, DTRAddr(sp, DtrOffImm(4)));

    // If we found a catch handler, this must be a baseline frame. Restore state
    // and jump to the catch block.
    bind(&catch_);
    ma_ldr(Operand(sp, offsetof(ResumeFromException, target)), r0);
    ma_ldr(Operand(sp, offsetof(ResumeFromException, framePointer)), r11);
    ma_ldr(Operand(sp, offsetof(ResumeFromException, stackPointer)), sp);
    jump(r0);

    // If we found a finally block, this must be a baseline frame. Push
    // two values expected by JSOP_RETSUB: BooleanValue(true) and the
    // exception.
    bind(&finally);
    ValueOperand exception = ValueOperand(r1, r2);
    loadValue(Operand(sp, offsetof(ResumeFromException, exception)), exception);

    ma_ldr(Operand(sp, offsetof(ResumeFromException, target)), r0);
    ma_ldr(Operand(sp, offsetof(ResumeFromException, framePointer)), r11);
    ma_ldr(Operand(sp, offsetof(ResumeFromException, stackPointer)), sp);

    pushValue(BooleanValue(true));
    pushValue(exception);
    jump(r0);

    // Only used in debug mode. Return BaselineFrame->returnValue() to the caller.
    bind(&return_);
    ma_ldr(Operand(sp, offsetof(ResumeFromException, framePointer)), r11);
    ma_ldr(Operand(sp, offsetof(ResumeFromException, stackPointer)), sp);
    loadValue(Address(r11, BaselineFrame::reverseOffsetOfReturnValue()), JSReturnOperand);
    ma_mov(r11, sp);
    pop(r11);
    ret();

    // If we are bailing out to baseline to handle an exception, jump to
    // the bailout tail stub.
    bind(&bailout);
    ma_ldr(Operand(sp, offsetof(ResumeFromException, bailoutInfo)), r2);
    ma_mov(Imm32(BAILOUT_RETURN_OK), r0);
    ma_ldr(Operand(sp, offsetof(ResumeFromException, target)), r1);
    jump(r1);
}

Assembler::Condition
MacroAssemblerARMCompat::testStringTruthy(bool truthy, const ValueOperand &value)
{
    Register string = value.payloadReg();

    size_t mask = (0xFFFFFFFF << JSString::LENGTH_SHIFT);
    ma_dtr(IsLoad, string, Imm32(JSString::offsetOfLengthAndFlags()), ScratchRegister);
    // Bit clear into the scratch register. This is done because there is performs the operation
    // dest <- src1 & ~ src2. There is no instruction that does this without writing
    // the result somewhere, so the Scratch Register is sacrificed.
    ma_bic(Imm32(~mask), ScratchRegister, SetCond);
    return truthy ? Assembler::NonZero : Assembler::Zero;
}

void
MacroAssemblerARMCompat::enterOsr(Register calleeToken, Register code)
{
    push(Imm32(0)); // num actual arguments.
    push(calleeToken);
    push(Imm32(MakeFrameDescriptor(0, IonFrame_Osr)));
    ma_add(sp, Imm32(sizeof(uintptr_t)), sp);   // padding
    ma_callIonHalfPush(code);
    ma_sub(sp, Imm32(sizeof(uintptr_t) * 3), sp);
}


void
MacroAssemblerARMCompat::floor(FloatRegister input, Register output, Label *bail)
{
    Label handleZero;
    Label handleNeg;
    Label fin;
    compareDouble(input, InvalidFloatReg);
    ma_b(&handleZero, Assembler::Equal);
    ma_b(&handleNeg, Assembler::Signed);
    // NaN is always a bail condition, just bail directly.
    ma_b(bail, Assembler::Overflow);

    // The argument is a positive number, truncation is the path to glory;
    // Since it is known to be > 0.0, explicitly convert to a larger range,
    // then a value that rounds to INT_MAX is explicitly different from an
    // argument that clamps to INT_MAX
    ma_vcvt_F64_U32(input, ScratchFloatReg);
    ma_vxfer(VFPRegister(ScratchFloatReg).uintOverlay(), output);
    ma_mov(output, output, SetCond);
    ma_b(bail, Signed);
    ma_b(&fin);

    bind(&handleZero);
    // Move the top word of the double into the output reg, if it is non-zero,
    // then the original value was -0.0
    as_vxfer(output, InvalidReg, input, FloatToCore, Always, 1);
    ma_cmp(output, Imm32(0));
    ma_b(bail, NonZero);
    ma_b(&fin);

    bind(&handleNeg);
    // Negative case, negate, then start dancing
    ma_vneg(input, input);
    ma_vcvt_F64_U32(input, ScratchFloatReg);
    ma_vxfer(VFPRegister(ScratchFloatReg).uintOverlay(), output);
    ma_vcvt_U32_F64(ScratchFloatReg, ScratchFloatReg);
    compareDouble(ScratchFloatReg, input);
    ma_add(output, Imm32(1), output, NoSetCond, NotEqual);
    // Negate the output.  Since INT_MIN < -INT_MAX, even after adding 1,
    // the result will still be a negative number
    ma_rsb(output, Imm32(0), output, SetCond);
    // Flip the negated input back to its original value.
    ma_vneg(input, input);
    // If the result looks non-negative, then this value didn't actually fit into
    // the int range, and special handling is required.
    // zero is also caught by this case, but floor of a negative number
    // should never be zero.
    ma_b(bail, Unsigned);

    bind(&fin);
}

CodeOffsetLabel
MacroAssemblerARMCompat::toggledJump(Label *label)
{
    // Emit a B that can be toggled to a CMP. See ToggleToJmp(), ToggleToCmp().
    CodeOffsetLabel ret(nextOffset().getOffset());
    ma_b(label, Always, true);
    return ret;
}

CodeOffsetLabel
MacroAssemblerARMCompat::toggledCall(IonCode *target, bool enabled)
{
    BufferOffset bo = nextOffset();
    CodeOffsetLabel offset(bo.getOffset());
    addPendingJump(bo, target->raw(), Relocation::IONCODE);
    ma_movPatchable(Imm32(uint32_t(target->raw())), ScratchRegister, Always, hasMOVWT() ? L_MOVWT : L_LDR);
    if (enabled)
        ma_blx(ScratchRegister);
    else
        ma_nop();
    JS_ASSERT(nextOffset().getOffset() - offset.offset() == ToggledCallSize());
    return offset;
}

void
MacroAssemblerARMCompat::round(FloatRegister input, Register output, Label *bail, FloatRegister tmp)
{
    Label handleZero;
    Label handleNeg;
    Label fin;
    // Do a compare based on the original value, then do most other things based on the
    // shifted value.
    ma_vcmpz(input);
    // Adding 0.5 is technically incorrect!
    // We want to add 0.5 to negative numbers, and 0.49999999999999999 to positive numbers.
    ma_vimm(0.5, ScratchFloatReg);
    // Since we already know the sign bit, flip all numbers to be positive, stored in tmp.
    ma_vabs(input, tmp);
    // Add 0.5, storing the result into tmp.
    ma_vadd(ScratchFloatReg, tmp, tmp);
    as_vmrs(pc);
    ma_b(&handleZero, Assembler::Equal);
    ma_b(&handleNeg, Assembler::Signed);
    // NaN is always a bail condition, just bail directly.
    ma_b(bail, Assembler::Overflow);

    // The argument is a positive number, truncation is the path to glory;
    // Since it is known to be > 0.0, explicitly convert to a larger range,
    // then a value that rounds to INT_MAX is explicitly different from an
    // argument that clamps to INT_MAX
    ma_vcvt_F64_U32(tmp, ScratchFloatReg);
    ma_vxfer(VFPRegister(ScratchFloatReg).uintOverlay(), output);
    ma_mov(output, output, SetCond);
    ma_b(bail, Signed);
    ma_b(&fin);

    bind(&handleZero);
    // Move the top word of the double into the output reg, if it is non-zero,
    // then the original value was -0.0
    as_vxfer(output, InvalidReg, input, FloatToCore, Always, 1);
    ma_cmp(output, Imm32(0));
    ma_b(bail, NonZero);
    ma_b(&fin);

    bind(&handleNeg);
    // Negative case, negate, then start dancing.  This number may be positive, since we added 0.5
    ma_vcvt_F64_U32(tmp, ScratchFloatReg);
    ma_vxfer(VFPRegister(ScratchFloatReg).uintOverlay(), output);

    // -output is now a correctly rounded value, unless the original value was exactly
    // halfway between two integers, at which point, it has been rounded away from zero, when
    // it should be rounded towards \infty.
    ma_vcvt_U32_F64(ScratchFloatReg, ScratchFloatReg);
    compareDouble(ScratchFloatReg, tmp);
    ma_sub(output, Imm32(1), output, NoSetCond, Equal);
    // Negate the output.  Since INT_MIN < -INT_MAX, even after adding 1,
    // the result will still be a negative number
    ma_rsb(output, Imm32(0), output, SetCond);

    // If the result looks non-negative, then this value didn't actually fit into
    // the int range, and special handling is required, or it was zero, which means
    // the result is actually -0.0 which also requires special handling.
    ma_b(bail, Unsigned);

    bind(&fin);
}

CodeOffsetJump
MacroAssemblerARMCompat::jumpWithPatch(RepatchLabel *label, Condition cond)
{
    ARMBuffer::PoolEntry pe;
    BufferOffset bo = as_BranchPool(0xdeadbeef, label, &pe, cond);

    // Fill in a new CodeOffset with both the load and the
    // pool entry that the instruction loads from.
    CodeOffsetJump ret(bo.getOffset(), pe.encode());
    return ret;
}


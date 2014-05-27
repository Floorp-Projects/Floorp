/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef assembler_assembler_MacroAssemblerX86Common_h
#define assembler_assembler_MacroAssemblerX86Common_h

#include "assembler/wtf/Platform.h"

#if ENABLE_ASSEMBLER

#include "assembler/assembler/X86Assembler.h"
#include "assembler/assembler/AbstractMacroAssembler.h"

namespace JSC {

class MacroAssemblerX86Common : public AbstractMacroAssembler<X86Assembler> {
    static const int DoubleConditionBitInvert = 0x10;
    static const int DoubleConditionBitSpecial = 0x20;
    static const int DoubleConditionBits = DoubleConditionBitInvert | DoubleConditionBitSpecial;

protected:
#if WTF_CPU_X86_64
    static const X86Registers::RegisterID scratchRegister = X86Registers::r11;
#endif

public:

    enum Condition {
        Equal = X86Assembler::ConditionE,
        NotEqual = X86Assembler::ConditionNE,
        Above = X86Assembler::ConditionA,
        AboveOrEqual = X86Assembler::ConditionAE,
        Below = X86Assembler::ConditionB,
        BelowOrEqual = X86Assembler::ConditionBE,
        GreaterThan = X86Assembler::ConditionG,
        GreaterThanOrEqual = X86Assembler::ConditionGE,
        LessThan = X86Assembler::ConditionL,
        LessThanOrEqual = X86Assembler::ConditionLE,
        Overflow = X86Assembler::ConditionO,
        Signed = X86Assembler::ConditionS,
        Zero = X86Assembler::ConditionE,
        NonZero = X86Assembler::ConditionNE
    };

    enum DoubleCondition {
        // These conditions will only evaluate to true if the comparison is ordered - i.e. neither operand is NaN.
        DoubleEqual = X86Assembler::ConditionE | DoubleConditionBitSpecial,
        DoubleNotEqual = X86Assembler::ConditionNE,
        DoubleGreaterThan = X86Assembler::ConditionA,
        DoubleGreaterThanOrEqual = X86Assembler::ConditionAE,
        DoubleLessThan = X86Assembler::ConditionA | DoubleConditionBitInvert,
        DoubleLessThanOrEqual = X86Assembler::ConditionAE | DoubleConditionBitInvert,
        // If either operand is NaN, these conditions always evaluate to true.
        DoubleEqualOrUnordered = X86Assembler::ConditionE,
        DoubleNotEqualOrUnordered = X86Assembler::ConditionNE | DoubleConditionBitSpecial,
        DoubleGreaterThanOrUnordered = X86Assembler::ConditionB | DoubleConditionBitInvert,
        DoubleGreaterThanOrEqualOrUnordered = X86Assembler::ConditionBE | DoubleConditionBitInvert,
        DoubleLessThanOrUnordered = X86Assembler::ConditionB,
        DoubleLessThanOrEqualOrUnordered = X86Assembler::ConditionBE
    };
    static void staticAsserts() {
        COMPILE_ASSERT(
            !((X86Assembler::ConditionE | X86Assembler::ConditionNE | X86Assembler::ConditionA | X86Assembler::ConditionAE | X86Assembler::ConditionB | X86Assembler::ConditionBE) & DoubleConditionBits),
            DoubleConditionBits_should_not_interfere_with_X86Assembler_Condition_codes);
    }

    static const RegisterID stackPointerRegister = X86Registers::esp;

    static inline bool CanUse8Bit(RegisterID reg) {
        return !!((1 << reg) & ~((1 << X86Registers::esp) |
                                 (1 << X86Registers::edi) |
                                 (1 << X86Registers::esi) |
                                 (1 << X86Registers::ebp)));
    }

    // Integer arithmetic operations:
    //
    // Operations are typically two operand - operation(source, srcDst)
    // For many operations the source may be an Imm32, the srcDst operand
    // may often be a memory location (explictly described using an Address
    // object).

    void add32(RegisterID src, RegisterID dest)
    {
        m_assembler.addl_rr(src, dest);
    }

    void add32(TrustedImm32 imm, Address address)
    {
        m_assembler.addl_im(imm.m_value, address.offset, address.base);
    }

    void add32(TrustedImm32 imm, RegisterID dest)
    {
        m_assembler.addl_ir(imm.m_value, dest);
    }

    void add32(Address src, RegisterID dest)
    {
        m_assembler.addl_mr(src.offset, src.base, dest);
    }

    void add32(RegisterID src, Address dest)
    {
        m_assembler.addl_rm(src, dest.offset, dest.base);
    }

    void and32(RegisterID src, RegisterID dest)
    {
        m_assembler.andl_rr(src, dest);
    }

    void and32(Imm32 imm, RegisterID dest)
    {
        m_assembler.andl_ir(imm.m_value, dest);
    }

    void and32(RegisterID src, Address dest)
    {
        m_assembler.andl_rm(src, dest.offset, dest.base);
    }

    void and32(Address src, RegisterID dest)
    {
        m_assembler.andl_mr(src.offset, src.base, dest);
    }

    void and32(Imm32 imm, Address address)
    {
        m_assembler.andl_im(imm.m_value, address.offset, address.base);
    }

    void lshift32(Imm32 imm, RegisterID dest)
    {
        m_assembler.shll_i8r(imm.m_value, dest);
    }

    void lshift32(RegisterID shift_amount, RegisterID dest)
    {
        // On x86 we can only shift by ecx; if asked to shift by another register we'll
        // need rejig the shift amount into ecx first, and restore the registers afterwards.
        if (shift_amount != X86Registers::ecx) {
            swap(shift_amount, X86Registers::ecx);

            // E.g. transform "shll %eax, %eax" -> "xchgl %eax, %ecx; shll %ecx, %ecx; xchgl %eax, %ecx"
            if (dest == shift_amount)
                m_assembler.shll_CLr(X86Registers::ecx);
            // E.g. transform "shll %eax, %ecx" -> "xchgl %eax, %ecx; shll %ecx, %eax; xchgl %eax, %ecx"
            else if (dest == X86Registers::ecx)
                m_assembler.shll_CLr(shift_amount);
            // E.g. transform "shll %eax, %ebx" -> "xchgl %eax, %ecx; shll %ecx, %ebx; xchgl %eax, %ecx"
            else
                m_assembler.shll_CLr(dest);

            swap(shift_amount, X86Registers::ecx);
        } else
            m_assembler.shll_CLr(dest);
    }

    void mul32(RegisterID src, RegisterID dest)
    {
        m_assembler.imull_rr(src, dest);
    }

    void mul32(Address src, RegisterID dest)
    {
        m_assembler.imull_mr(src.offset, src.base, dest);
    }

    void mul32(Imm32 imm, RegisterID src, RegisterID dest)
    {
        m_assembler.imull_i32r(src, imm.m_value, dest);
    }

    void idiv(RegisterID reg)
    {
        m_assembler.cdq();
        m_assembler.idivl_r(reg);
    }

    void neg32(RegisterID srcDest)
    {
        m_assembler.negl_r(srcDest);
    }

    void neg32(Address srcDest)
    {
        m_assembler.negl_m(srcDest.offset, srcDest.base);
    }

    void not32(RegisterID srcDest)
    {
        m_assembler.notl_r(srcDest);
    }

    void not32(Address srcDest)
    {
        m_assembler.notl_m(srcDest.offset, srcDest.base);
    }

    void or32(RegisterID src, RegisterID dest)
    {
        m_assembler.orl_rr(src, dest);
    }

    void or32(TrustedImm32 imm, RegisterID dest)
    {
        m_assembler.orl_ir(imm.m_value, dest);
    }

    void or32(RegisterID src, Address dest)
    {
        m_assembler.orl_rm(src, dest.offset, dest.base);
    }

    void or32(Address src, RegisterID dest)
    {
        m_assembler.orl_mr(src.offset, src.base, dest);
    }

    void or32(TrustedImm32 imm, Address address)
    {
        m_assembler.orl_im(imm.m_value, address.offset, address.base);
    }

    void rshift32(RegisterID shift_amount, RegisterID dest)
    {
        // On x86 we can only shift by ecx; if asked to shift by another register we'll
        // need rejig the shift amount into ecx first, and restore the registers afterwards.
        if (shift_amount != X86Registers::ecx) {
            swap(shift_amount, X86Registers::ecx);

            // E.g. transform "shll %eax, %eax" -> "xchgl %eax, %ecx; shll %ecx, %ecx; xchgl %eax, %ecx"
            if (dest == shift_amount)
                m_assembler.sarl_CLr(X86Registers::ecx);
            // E.g. transform "shll %eax, %ecx" -> "xchgl %eax, %ecx; shll %ecx, %eax; xchgl %eax, %ecx"
            else if (dest == X86Registers::ecx)
                m_assembler.sarl_CLr(shift_amount);
            // E.g. transform "shll %eax, %ebx" -> "xchgl %eax, %ecx; shll %ecx, %ebx; xchgl %eax, %ecx"
            else
                m_assembler.sarl_CLr(dest);

            swap(shift_amount, X86Registers::ecx);
        } else
            m_assembler.sarl_CLr(dest);
    }

    void rshift32(Imm32 imm, RegisterID dest)
    {
        m_assembler.sarl_i8r(imm.m_value, dest);
    }

    void urshift32(RegisterID shift_amount, RegisterID dest)
    {
        // On x86 we can only shift by ecx; if asked to shift by another register we'll
        // need rejig the shift amount into ecx first, and restore the registers afterwards.
        if (shift_amount != X86Registers::ecx) {
            swap(shift_amount, X86Registers::ecx);

            // E.g. transform "shrl %eax, %eax" -> "xchgl %eax, %ecx; shrl %ecx, %ecx; xchgl %eax, %ecx"
            if (dest == shift_amount)
                m_assembler.shrl_CLr(X86Registers::ecx);
            // E.g. transform "shrl %eax, %ecx" -> "xchgl %eax, %ecx; shrl %ecx, %eax; xchgl %eax, %ecx"
            else if (dest == X86Registers::ecx)
                m_assembler.shrl_CLr(shift_amount);
            // E.g. transform "shrl %eax, %ebx" -> "xchgl %eax, %ecx; shrl %ecx, %ebx; xchgl %eax, %ecx"
            else
                m_assembler.shrl_CLr(dest);

            swap(shift_amount, X86Registers::ecx);
        } else
            m_assembler.shrl_CLr(dest);
    }

    void urshift32(Imm32 imm, RegisterID dest)
    {
        m_assembler.shrl_i8r(imm.m_value, dest);
    }

    void sub32(RegisterID src, RegisterID dest)
    {
        m_assembler.subl_rr(src, dest);
    }

    void sub32(TrustedImm32 imm, RegisterID dest)
    {
        m_assembler.subl_ir(imm.m_value, dest);
    }

    void sub32(TrustedImm32 imm, Address address)
    {
        m_assembler.subl_im(imm.m_value, address.offset, address.base);
    }

    void sub32(Address src, RegisterID dest)
    {
        m_assembler.subl_mr(src.offset, src.base, dest);
    }

    void sub32(RegisterID src, Address dest)
    {
        m_assembler.subl_rm(src, dest.offset, dest.base);
    }


    void xor32(RegisterID src, RegisterID dest)
    {
        m_assembler.xorl_rr(src, dest);
    }

    void xor32(TrustedImm32 imm, Address dest)
    {
        m_assembler.xorl_im(imm.m_value, dest.offset, dest.base);
    }

    void xor32(TrustedImm32 imm, RegisterID dest)
    {
        m_assembler.xorl_ir(imm.m_value, dest);
    }

    void xor32(RegisterID src, Address dest)
    {
        m_assembler.xorl_rm(src, dest.offset, dest.base);
    }

    void xor32(Address src, RegisterID dest)
    {
        m_assembler.xorl_mr(src.offset, src.base, dest);
    }

    void sqrtDouble(FPRegisterID src, FPRegisterID dst)
    {
        m_assembler.sqrtsd_rr(src, dst);
    }

    // Memory access operations:
    //
    // Loads are of the form load(address, destination) and stores of the form
    // store(source, address).  The source for a store may be an Imm32.  Address
    // operand objects to loads and store will be implicitly constructed if a
    // register is passed.

    void load32(ImplicitAddress address, RegisterID dest)
    {
        m_assembler.movl_mr(address.offset, address.base, dest);
    }

    void load32(BaseIndex address, RegisterID dest)
    {
        m_assembler.movl_mr(address.offset, address.base, address.index, address.scale, dest);
    }

    void load32WithUnalignedHalfWords(BaseIndex address, RegisterID dest)
    {
        load32(address, dest);
    }

    DataLabel32 load32WithAddressOffsetPatch(Address address, RegisterID dest)
    {
        m_assembler.movl_mr_disp32(address.offset, address.base, dest);
        return DataLabel32(this);
    }

    void store8(RegisterID src, Address address)
    {
        m_assembler.movb_rm(src, address.offset, address.base);
    }

    void store8(RegisterID src, BaseIndex address)
    {
        m_assembler.movb_rm(src, address.offset, address.base, address.index, address.scale);
    }

    void store16(RegisterID src, Address address)
    {
        m_assembler.movw_rm(src, address.offset, address.base);
    }

    void store16(RegisterID src, BaseIndex address)
    {
        m_assembler.movw_rm(src, address.offset, address.base, address.index, address.scale);
    }

    void load8(BaseIndex address, RegisterID dest)
    {
        load8ZeroExtend(address, dest);
    }

    void load8ZeroExtend(BaseIndex address, RegisterID dest)
    {
        m_assembler.movzbl_mr(address.offset, address.base, address.index, address.scale, dest);
    }

    void load8ZeroExtend(Address address, RegisterID dest)
    {
        m_assembler.movzbl_mr(address.offset, address.base, dest);
    }

    void load8SignExtend(BaseIndex address, RegisterID dest)
    {
        m_assembler.movsbl_mr(address.offset, address.base, address.index, address.scale, dest);
    }

    void load8SignExtend(Address address, RegisterID dest)
    {
        m_assembler.movsbl_mr(address.offset, address.base, dest);
    }

    void load16SignExtend(BaseIndex address, RegisterID dest)
    {
        m_assembler.movswl_mr(address.offset, address.base, address.index, address.scale, dest);
    }

    void load16SignExtend(Address address, RegisterID dest)
    {
        m_assembler.movswl_mr(address.offset, address.base, dest);
    }

    void load16(BaseIndex address, RegisterID dest)
    {
        m_assembler.movzwl_mr(address.offset, address.base, address.index, address.scale, dest);
    }

    void load16(Address address, RegisterID dest)
    {
        m_assembler.movzwl_mr(address.offset, address.base, dest);
    }

    void load16Unaligned(BaseIndex address, RegisterID dest)
    {
        load16(address, dest);
    }

    DataLabel32 store32WithAddressOffsetPatch(RegisterID src, Address address)
    {
        m_assembler.movl_rm_disp32(src, address.offset, address.base);
        return DataLabel32(this);
    }

    void store32(RegisterID src, ImplicitAddress address)
    {
        m_assembler.movl_rm(src, address.offset, address.base);
    }

    void store32(RegisterID src, BaseIndex address)
    {
        m_assembler.movl_rm(src, address.offset, address.base, address.index, address.scale);
    }

    void store32(TrustedImm32 imm, BaseIndex address)
    {
        m_assembler.movl_i32m(imm.m_value, address.offset, address.base, address.index, address.scale);
    }

    void store16(Imm32 imm, BaseIndex address)
    {
        m_assembler.movw_i16m(imm.m_value, address.offset, address.base, address.index, address.scale);
    }

    void store8(Imm32 imm, BaseIndex address)
    {
        m_assembler.movb_i8m(imm.m_value, address.offset, address.base, address.index, address.scale);
    }

    void store32(TrustedImm32 imm, ImplicitAddress address)
    {
        m_assembler.movl_i32m(imm.m_value, address.offset, address.base);
    }

    void store16(Imm32 imm, ImplicitAddress address)
    {
        m_assembler.movw_i16m(imm.m_value, address.offset, address.base);
    }

    void store8(Imm32 imm, ImplicitAddress address)
    {
        m_assembler.movb_i8m(imm.m_value, address.offset, address.base);
    }


    // Floating-point operation:
    //
    // Presently only supports SSE, not x87 floating point.

    void moveDouble(FPRegisterID src, FPRegisterID dest)
    {
        ASSERT(isSSE2Present());
        m_assembler.movsd_rr(src, dest);
    }

    void loadFloat(ImplicitAddress address, FPRegisterID dest)
    {
        ASSERT(isSSE2Present());
        m_assembler.movss_mr(address.offset, address.base, dest);
        m_assembler.cvtss2sd_rr(dest, dest);
    }

    void loadFloat(BaseIndex address, FPRegisterID dest)
    {
        ASSERT(isSSE2Present());
        m_assembler.movss_mr(address.offset, address.base, address.index, address.scale, dest);
        m_assembler.cvtss2sd_rr(dest, dest);
    }

    void convertDoubleToFloat(FPRegisterID src, FPRegisterID dest)
    {
        ASSERT(isSSE2Present());
        m_assembler.cvtsd2ss_rr(src, dest);
    }

    void loadDouble(ImplicitAddress address, FPRegisterID dest)
    {
        ASSERT(isSSE2Present());
        m_assembler.movsd_mr(address.offset, address.base, dest);
    }

    void loadDouble(BaseIndex address, FPRegisterID dest)
    {
        ASSERT(isSSE2Present());
        m_assembler.movsd_mr(address.offset, address.base, address.index, address.scale, dest);
    }

    void storeFloat(ImmDouble imm, Address address)
    {
        union {
            float f;
            uint32_t u32;
        } u;
        u.f = imm.u.d;
        store32(Imm32(u.u32), address);
    }

    void storeFloat(ImmDouble imm, BaseIndex address)
    {
        union {
            float f;
            uint32_t u32;
        } u;
        u.f = imm.u.d;
        store32(Imm32(u.u32), address);
    }

    void storeDouble(FPRegisterID src, ImplicitAddress address)
    {
        ASSERT(isSSE2Present());
        m_assembler.movsd_rm(src, address.offset, address.base);
    }

    void storeFloat(FPRegisterID src, ImplicitAddress address)
    {
        ASSERT(isSSE2Present());
        m_assembler.movss_rm(src, address.offset, address.base);
    }

    void storeDouble(FPRegisterID src, BaseIndex address)
    {
        ASSERT(isSSE2Present());
        m_assembler.movsd_rm(src, address.offset, address.base, address.index, address.scale);
    }

    void storeFloat(FPRegisterID src, BaseIndex address)
    {
        ASSERT(isSSE2Present());
        m_assembler.movss_rm(src, address.offset, address.base, address.index, address.scale);
    }

    void addDouble(FPRegisterID src, FPRegisterID dest)
    {
        ASSERT(isSSE2Present());
        m_assembler.addsd_rr(src, dest);
    }

    void addDouble(Address src, FPRegisterID dest)
    {
        ASSERT(isSSE2Present());
        m_assembler.addsd_mr(src.offset, src.base, dest);
    }

    void divDouble(FPRegisterID src, FPRegisterID dest)
    {
        ASSERT(isSSE2Present());
        m_assembler.divsd_rr(src, dest);
    }

    void divDouble(Address src, FPRegisterID dest)
    {
        ASSERT(isSSE2Present());
        m_assembler.divsd_mr(src.offset, src.base, dest);
    }

    void subDouble(FPRegisterID src, FPRegisterID dest)
    {
        ASSERT(isSSE2Present());
        m_assembler.subsd_rr(src, dest);
    }

    void subDouble(Address src, FPRegisterID dest)
    {
        ASSERT(isSSE2Present());
        m_assembler.subsd_mr(src.offset, src.base, dest);
    }

    void mulDouble(FPRegisterID src, FPRegisterID dest)
    {
        ASSERT(isSSE2Present());
        m_assembler.mulsd_rr(src, dest);
    }

    void mulDouble(Address src, FPRegisterID dest)
    {
        ASSERT(isSSE2Present());
        m_assembler.mulsd_mr(src.offset, src.base, dest);
    }

    void xorDouble(FPRegisterID src, FPRegisterID dest)
    {
        ASSERT(isSSE2Present());
        m_assembler.xorpd_rr(src, dest);
    }

    void andDouble(FPRegisterID src, FPRegisterID dest)
    {
        ASSERT(isSSE2Present());
        m_assembler.andpd_rr(src, dest);
    }

    void absDouble(FPRegisterID src, FPRegisterID dest)
    {
        ASSERT(isSSE2Present());
        /* Compile abs(x) as x & -x. */
        zeroDouble(dest);
        subDouble(src, dest);
        andDouble(src, dest);
    }

    void convertInt32ToDouble(RegisterID src, FPRegisterID dest)
    {
        ASSERT(isSSE2Present());
        m_assembler.cvtsi2sd_rr(src, dest);
    }

    void convertInt32ToDouble(Address src, FPRegisterID dest)
    {
        ASSERT(isSSE2Present());
        m_assembler.cvtsi2sd_mr(src.offset, src.base, dest);
    }

    Jump branchDouble(DoubleCondition cond, FPRegisterID left, FPRegisterID right)
    {
        ASSERT(isSSE2Present());

        if (cond & DoubleConditionBitInvert)
            m_assembler.ucomisd_rr(left, right);
        else
            m_assembler.ucomisd_rr(right, left);

        if (cond == DoubleEqual) {
            Jump isUnordered(m_assembler.jp());
            Jump result = Jump(m_assembler.je());
            isUnordered.link(this);
            return result;
        } else if (cond == DoubleNotEqualOrUnordered) {
            Jump isUnordered(m_assembler.jp());
            Jump isEqual(m_assembler.je());
            isUnordered.link(this);
            Jump result = jump();
            isEqual.link(this);
            return result;
        }

        ASSERT(!(cond & DoubleConditionBitSpecial));
        return Jump(m_assembler.jCC(static_cast<X86Assembler::Condition>(cond & ~DoubleConditionBits)));
    }

    // Truncates 'src' to an integer, and places the resulting 'dest'.
    // If the result is not representable as a 32 bit value, branch.
    // May also branch for some values that are representable in 32 bits
    // (specifically, in this case, INT_MIN).
    Jump branchTruncateDoubleToInt32(FPRegisterID src, RegisterID dest)
    {
        ASSERT(isSSE2Present());
        m_assembler.cvttsd2si_rr(src, dest);
        return branch32(Equal, dest, Imm32(0x80000000));
    }

    // Convert 'src' to an integer, and places the resulting 'dest'.
    // If the result is not representable as a 32 bit value, branch.
    // May also branch for some values that are representable in 32 bits
    // (specifically, in this case, 0).
    void branchConvertDoubleToInt32(FPRegisterID src, RegisterID dest, JumpList& failureCases, FPRegisterID fpTemp)
    {
        ASSERT(isSSE2Present());
        ASSERT(src != fpTemp);
        m_assembler.cvttsd2si_rr(src, dest);

        // If the result is zero, it might have been -0.0, and the double comparison won't catch this!
        failureCases.append(branchTest32(Zero, dest));

        // Convert the integer result back to float & compare to the original value - if not equal or unordered (NaN) then jump.
        convertInt32ToDouble(dest, fpTemp);
        m_assembler.ucomisd_rr(fpTemp, src);
        failureCases.append(Jump(m_assembler.jp()));
        failureCases.append(Jump(m_assembler.jne()));
    }

    void zeroDouble(FPRegisterID srcDest)
    {
        ASSERT(isSSE2Present());
        m_assembler.xorpd_rr(srcDest, srcDest);
    }


    // Stack manipulation operations:
    //
    // The ABI is assumed to provide a stack abstraction to memory,
    // containing machine word sized units of data.  Push and pop
    // operations add and remove a single register sized unit of data
    // to or from the stack.  Peek and poke operations read or write
    // values on the stack, without moving the current stack position.

    void pop(RegisterID dest)
    {
        m_assembler.pop_r(dest);
    }

    void push(RegisterID src)
    {
        m_assembler.push_r(src);
    }

    void push(Address address)
    {
        m_assembler.push_m(address.offset, address.base);
    }

    void push(Imm32 imm)
    {
        m_assembler.push_i32(imm.m_value);
    }


    // Register move operations:
    //
    // Move values in registers.

    void move(TrustedImm32 imm, RegisterID dest)
    {
        // Note: on 64-bit the Imm32 value is zero extended into the register, it
        // may be useful to have a separate version that sign extends the value?
        if (!imm.m_value)
            m_assembler.xorl_rr(dest, dest);
        else
            m_assembler.movl_i32r(imm.m_value, dest);
    }

#if WTF_CPU_X86_64
    void move(RegisterID src, RegisterID dest)
    {
        // Note: on 64-bit this is is a full register move; perhaps it would be
        // useful to have separate move32 & movePtr, with move32 zero extending?
        if (src != dest)
            m_assembler.movq_rr(src, dest);
    }

    void move(TrustedImmPtr imm, RegisterID dest)
    {
        m_assembler.movq_i64r(imm.asIntptr(), dest);
    }

    void swap(RegisterID reg1, RegisterID reg2)
    {
        // XCHG is extremely slow. Don't use XCHG.
        if (reg1 != reg2) {
            m_assembler.movq_rr(reg1, scratchRegister);
            m_assembler.movq_rr(reg2, reg1);
            m_assembler.movq_rr(scratchRegister, reg2);
        }
    }

    void signExtend32ToPtr(RegisterID src, RegisterID dest)
    {
        m_assembler.movsxd_rr(src, dest);
    }

    void zeroExtend32ToPtr(RegisterID src, RegisterID dest)
    {
        m_assembler.movl_rr(src, dest);
    }
#else
    void move(RegisterID src, RegisterID dest)
    {
        if (src != dest)
            m_assembler.movl_rr(src, dest);
    }

    void move(TrustedImmPtr imm, RegisterID dest)
    {
        m_assembler.movl_i32r(imm.asIntptr(), dest);
    }

    void swap(RegisterID reg1, RegisterID reg2)
    {
        if (reg1 != reg2)
            m_assembler.xchgl_rr(reg1, reg2);
    }

    void signExtend32ToPtr(RegisterID src, RegisterID dest)
    {
        move(src, dest);
    }

    void zeroExtend32ToPtr(RegisterID src, RegisterID dest)
    {
        move(src, dest);
    }
#endif


    // Forwards / external control flow operations:
    //
    // This set of jump and conditional branch operations return a Jump
    // object which may linked at a later point, allow forwards jump,
    // or jumps that will require external linkage (after the code has been
    // relocated).
    //
    // For branches, signed <, >, <= and >= are denoted as l, g, le, and ge
    // respecitvely, for unsigned comparisons the names b, a, be, and ae are
    // used (representing the names 'below' and 'above').
    //
    // Operands to the comparision are provided in the expected order, e.g.
    // jle32(reg1, Imm32(5)) will branch if the value held in reg1, when
    // treated as a signed 32bit value, is less than or equal to 5.
    //
    // jz and jnz test whether the first operand is equal to zero, and take
    // an optional second operand of a mask under which to perform the test.

public:
    Jump branch8(Condition cond, Address left, Imm32 right)
    {
        m_assembler.cmpb_im(right.m_value, left.offset, left.base);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branch32(Condition cond, RegisterID left, RegisterID right)
    {
        m_assembler.cmpl_rr(right, left);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branch32(Condition cond, RegisterID left, TrustedImm32 right)
    {
        if (((cond == Equal) || (cond == NotEqual)) && !right.m_value)
            m_assembler.testl_rr(left, left);
        else
            m_assembler.cmpl_ir(right.m_value, left);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    // Branch based on a 32-bit comparison, forcing the size of the
    // immediate operand to 32 bits in the native code stream to ensure that
    // the length of code emitted by this instruction is consistent.
    Jump branch32FixedLength(Condition cond, RegisterID left, TrustedImm32 right)
    {
        m_assembler.cmpl_ir_force32(right.m_value, left);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    // Branch and record a label after the comparison.
    Jump branch32WithPatch(Condition cond, RegisterID left, TrustedImm32 right, DataLabel32 &dataLabel)
    {
        // Always use cmpl, since the value is to be patched.
        m_assembler.cmpl_ir_force32(right.m_value, left);
        dataLabel = DataLabel32(this);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branch32WithPatch(Condition cond, Address left, TrustedImm32 right, DataLabel32 &dataLabel)
    {
        m_assembler.cmpl_im_force32(right.m_value, left.offset, left.base);
        dataLabel = DataLabel32(this);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branch32(Condition cond, RegisterID left, Address right)
    {
        m_assembler.cmpl_mr(right.offset, right.base, left);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branch32(Condition cond, Address left, RegisterID right)
    {
        m_assembler.cmpl_rm(right, left.offset, left.base);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branch32(Condition cond, Address left, TrustedImm32 right)
    {
        m_assembler.cmpl_im(right.m_value, left.offset, left.base);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branch32(Condition cond, BaseIndex left, TrustedImm32 right)
    {
        m_assembler.cmpl_im(right.m_value, left.offset, left.base, left.index, left.scale);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branch32WithUnalignedHalfWords(Condition cond, BaseIndex left, TrustedImm32 right)
    {
        return branch32(cond, left, right);
    }

    Jump branch16(Condition cond, BaseIndex left, RegisterID right)
    {
        m_assembler.cmpw_rm(right, left.offset, left.base, left.index, left.scale);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branch16(Condition cond, BaseIndex left, Imm32 right)
    {
        ASSERT(!(right.m_value & 0xFFFF0000));

        m_assembler.cmpw_im(right.m_value, left.offset, left.base, left.index, left.scale);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchTest32(Condition cond, RegisterID reg, RegisterID mask)
    {
        ASSERT((cond == Zero) || (cond == NonZero));
        m_assembler.testl_rr(reg, mask);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchTest32(Condition cond, RegisterID reg, Imm32 mask = Imm32(-1))
    {
        ASSERT((cond == Zero) || (cond == NonZero));
        // if we are only interested in the low seven bits, this can be tested with a testb
        if (mask.m_value == -1)
            m_assembler.testl_rr(reg, reg);
        else if (CanUse8Bit(reg) && (mask.m_value & ~0x7f) == 0)
            m_assembler.testb_i8r(mask.m_value, reg);
        else
            m_assembler.testl_i32r(mask.m_value, reg);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchTest32(Condition cond, Address address, Imm32 mask = Imm32(-1))
    {
        ASSERT((cond == Zero) || (cond == NonZero));
        if (mask.m_value == -1)
            m_assembler.cmpl_im(0, address.offset, address.base);
        else
            m_assembler.testl_i32m(mask.m_value, address.offset, address.base);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchTest32(Condition cond, BaseIndex address, Imm32 mask = Imm32(-1))
    {
        ASSERT((cond == Zero) || (cond == NonZero));
        if (mask.m_value == -1)
            m_assembler.cmpl_im(0, address.offset, address.base, address.index, address.scale);
        else
            m_assembler.testl_i32m(mask.m_value, address.offset, address.base, address.index, address.scale);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchTest8(Condition cond, Address address, Imm32 mask = Imm32(-1))
    {
        ASSERT((cond == Zero) || (cond == NonZero));
        if (mask.m_value == -1)
            m_assembler.cmpb_im(0, address.offset, address.base);
        else
            m_assembler.testb_im(mask.m_value, address.offset, address.base);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchTest8(Condition cond, BaseIndex address, Imm32 mask = Imm32(-1))
    {
        ASSERT((cond == Zero) || (cond == NonZero));
        if (mask.m_value == -1)
            m_assembler.cmpb_im(0, address.offset, address.base, address.index, address.scale);
        else
            m_assembler.testb_im(mask.m_value, address.offset, address.base, address.index, address.scale);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump jump()
    {
        return Jump(m_assembler.jmp());
    }

    void jump(RegisterID target)
    {
        m_assembler.jmp_r(target);
    }

    // Address is a memory location containing the address to jump to
    void jump(Address address)
    {
        m_assembler.jmp_m(address.offset, address.base);
    }

    void jump(BaseIndex address)
    {
        m_assembler.jmp_m(address.offset, address.base, address.index, address.scale);
    }

    // Arithmetic control flow operations:
    //
    // This set of conditional branch operations branch based
    // on the result of an arithmetic operation.  The operation
    // is performed as normal, storing the result.
    //
    // * jz operations branch if the result is zero.
    // * jo operations branch if the (signed) arithmetic
    //   operation caused an overflow to occur.

    Jump branchAdd32(Condition cond, RegisterID src, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
        add32(src, dest);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchAdd32(Condition cond, Imm32 imm, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
        add32(imm, dest);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchAdd32(Condition cond, Imm32 src, Address dest)
    {
        ASSERT((cond == Overflow) || (cond == Zero) || (cond == NonZero));
        add32(src, dest);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchAdd32(Condition cond, RegisterID src, Address dest)
    {
        ASSERT((cond == Overflow) || (cond == Zero) || (cond == NonZero));
        add32(src, dest);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchAdd32(Condition cond, Address src, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Zero) || (cond == NonZero));
        add32(src, dest);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchMul32(Condition cond, RegisterID src, RegisterID dest)
    {
        ASSERT(cond == Overflow);
        mul32(src, dest);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchMul32(Condition cond, Address src, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Zero) || (cond == NonZero));
        mul32(src, dest);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchMul32(Condition cond, Imm32 imm, RegisterID src, RegisterID dest)
    {
        ASSERT(cond == Overflow);
        mul32(imm, src, dest);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchSub32(Condition cond, RegisterID src, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
        sub32(src, dest);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchSub32(Condition cond, Imm32 imm, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
        sub32(imm, dest);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchSub32(Condition cond, Imm32 imm, Address dest)
    {
        ASSERT((cond == Overflow) || (cond == Zero) || (cond == NonZero));
        sub32(imm, dest);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchSub32(Condition cond, RegisterID src, Address dest)
    {
        ASSERT((cond == Overflow) || (cond == Zero) || (cond == NonZero));
        sub32(src, dest);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchSub32(Condition cond, Address src, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Zero) || (cond == NonZero));
        sub32(src, dest);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchNeg32(Condition cond, RegisterID srcDest)
    {
        ASSERT((cond == Overflow) || (cond == Zero) || (cond == NonZero));
        neg32(srcDest);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchOr32(Condition cond, RegisterID src, RegisterID dest)
    {
        ASSERT((cond == Signed) || (cond == Zero) || (cond == NonZero));
        or32(src, dest);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }


    // Miscellaneous operations:

    void breakpoint()
    {
        m_assembler.int3();
    }

    Call nearCall()
    {
        return Call(m_assembler.call(), Call::LinkableNear);
    }

    Call call(RegisterID target)
    {
        return Call(m_assembler.call(target), Call::None);
    }

    void call(Address address)
    {
        m_assembler.call_m(address.offset, address.base);
    }

    void ret()
    {
        m_assembler.ret();
    }

    void set8(Condition cond, RegisterID left, RegisterID right, RegisterID dest)
    {
        m_assembler.cmpl_rr(right, left);
        m_assembler.setCC_r(x86Condition(cond), dest);
    }

    void set8(Condition cond, Address left, RegisterID right, RegisterID dest)
    {
        m_assembler.cmpl_mr(left.offset, left.base, right);
        m_assembler.setCC_r(x86Condition(cond), dest);
    }

    void set8(Condition cond, RegisterID left, Imm32 right, RegisterID dest)
    {
        if (((cond == Equal) || (cond == NotEqual)) && !right.m_value)
            m_assembler.testl_rr(left, left);
        else
            m_assembler.cmpl_ir(right.m_value, left);
        m_assembler.setCC_r(x86Condition(cond), dest);
    }

    void set32(Condition cond, Address left, RegisterID right, RegisterID dest)
    {
        m_assembler.cmpl_rm(right, left.offset, left.base);
        m_assembler.setCC_r(x86Condition(cond), dest);
        m_assembler.movzbl_rr(dest, dest);
    }

    void set32(Condition cond, RegisterID left, Address right, RegisterID dest)
    {
        m_assembler.cmpl_mr(right.offset, right.base, left);
        m_assembler.setCC_r(x86Condition(cond), dest);
        m_assembler.movzbl_rr(dest, dest);
    }

    void set32(Condition cond, RegisterID left, RegisterID right, RegisterID dest)
    {
        m_assembler.cmpl_rr(right, left);
        m_assembler.setCC_r(x86Condition(cond), dest);
        m_assembler.movzbl_rr(dest, dest);
    }

    void set32(Condition cond, Address left, Imm32 right, RegisterID dest)
    {
        m_assembler.cmpl_im(right.m_value, left.offset, left.base);
        m_assembler.setCC_r(x86Condition(cond), dest);
        m_assembler.movzbl_rr(dest, dest);
    }

    void set32(Condition cond, RegisterID left, Imm32 right, RegisterID dest)
    {
        if (((cond == Equal) || (cond == NotEqual)) && !right.m_value)
            m_assembler.testl_rr(left, left);
        else
            m_assembler.cmpl_ir(right.m_value, left);
        m_assembler.setCC_r(x86Condition(cond), dest);
        m_assembler.movzbl_rr(dest, dest);
    }

    // FIXME:
    // The mask should be optional... paerhaps the argument order should be
    // dest-src, operations always have a dest? ... possibly not true, considering
    // asm ops like test, or pseudo ops like pop().

    void setTest8(Condition cond, Address address, Imm32 mask, RegisterID dest)
    {
        if (mask.m_value == -1)
            m_assembler.cmpb_im(0, address.offset, address.base);
        else
            m_assembler.testb_im(mask.m_value, address.offset, address.base);
        m_assembler.setCC_r(x86Condition(cond), dest);
        m_assembler.movzbl_rr(dest, dest);
    }

    void setTest32(Condition cond, Address address, Imm32 mask, RegisterID dest)
    {
        if (mask.m_value == -1)
            m_assembler.cmpl_im(0, address.offset, address.base);
        else
            m_assembler.testl_i32m(mask.m_value, address.offset, address.base);
        m_assembler.setCC_r(x86Condition(cond), dest);
        m_assembler.movzbl_rr(dest, dest);
    }

    // As the SSE's were introduced in order, the presence of a later SSE implies
    // the presence of an earlier SSE. For example, SSE4_2 support implies SSE2 support.
    enum SSECheckState {
        NotCheckedSSE = 0,
        NoSSE = 1,
        HasSSE = 2,
        HasSSE2 = 3,
        HasSSE3 = 4,
        HasSSSE3 = 5,
        HasSSE4_1 = 6,
        HasSSE4_2 = 7
    };

    static SSECheckState getSSEState()
    {
        if (s_sseCheckState == NotCheckedSSE) {
            MacroAssemblerX86Common::setSSECheckState();
        }
        // Only check once.
        ASSERT(s_sseCheckState != NotCheckedSSE);

        return s_sseCheckState;
    }

protected:
    X86Assembler::Condition x86Condition(Condition cond)
    {
        return static_cast<X86Assembler::Condition>(cond);
    }

private:
    friend class MacroAssemblerX86;

    static SSECheckState s_sseCheckState;

    static void setSSECheckState();

  public:
#if WTF_CPU_X86
    static bool isSSEPresent()
    {
#if defined(__SSE__) && !defined(DEBUG)
        return true;
#else
        if (s_sseCheckState == NotCheckedSSE) {
            setSSECheckState();
        }
        // Only check once.
        ASSERT(s_sseCheckState != NotCheckedSSE);

        return s_sseCheckState >= HasSSE;
#endif
    }

    static bool isSSE2Present()
    {
#if defined(__SSE2__) && !defined(DEBUG)
        return true;
#else
        if (s_sseCheckState == NotCheckedSSE) {
            setSSECheckState();
        }
        // Only check once.
        ASSERT(s_sseCheckState != NotCheckedSSE);

        return s_sseCheckState >= HasSSE2;
#endif
    }

#elif !defined(NDEBUG) // CPU(X86)

    // On x86-64 we should never be checking for SSE2 in a non-debug build,
    // but non debug add this method to keep the asserts above happy.
    static bool isSSE2Present()
    {
        return true;
    }

#endif
    static bool isSSE3Present()
    {
#if defined(__SSE3__) && !defined(DEBUG)
        return true;
#else
        if (s_sseCheckState == NotCheckedSSE) {
            setSSECheckState();
        }
        // Only check once.
        ASSERT(s_sseCheckState != NotCheckedSSE);

        return s_sseCheckState >= HasSSE3;
#endif
    }

    static bool isSSSE3Present()
    {
#if defined(__SSSE3__) && !defined(DEBUG)
        return true;
#else
        if (s_sseCheckState == NotCheckedSSE) {
            setSSECheckState();
        }
        // Only check once.
        ASSERT(s_sseCheckState != NotCheckedSSE);

        return s_sseCheckState >= HasSSSE3;
#endif
    }

    static bool isSSE41Present()
    {
#if defined(__SSE4_1__) && !defined(DEBUG)
        return true;
#else
        if (s_sseCheckState == NotCheckedSSE) {
            setSSECheckState();
        }
        // Only check once.
        ASSERT(s_sseCheckState != NotCheckedSSE);

        return s_sseCheckState >= HasSSE4_1;
#endif
    }

    static bool isSSE42Present()
    {
#if defined(__SSE4_2__) && !defined(DEBUG)
        return true;
#else
        if (s_sseCheckState == NotCheckedSSE) {
            setSSECheckState();
        }
        // Only check once.
        ASSERT(s_sseCheckState != NotCheckedSSE);

        return s_sseCheckState >= HasSSE4_2;
#endif
    }

  private:
#ifdef DEBUG
    static bool s_floatingPointDisabled;
    static bool s_SSE3Disabled;
    static bool s_SSE4Disabled;

  public:
    static void SetFloatingPointDisabled() {
        s_floatingPointDisabled = true;
    }
    static void SetSSE3Disabled() {
        s_SSE3Disabled = true;
    }
    static void SetSSE4Disabled() {
        s_SSE4Disabled = true;
    }
#endif
};

} // namespace JSC

#endif // ENABLE(ASSEMBLER)

#endif /* assembler_assembler_MacroAssemblerX86Common_h */

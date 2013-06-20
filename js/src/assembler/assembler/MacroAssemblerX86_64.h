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

#ifndef assembler_assembler_MacroAssemblerX86_64_h
#define assembler_assembler_MacroAssemblerX86_64_h

#include "mozilla/DebugOnly.h"

#include "assembler/wtf/Platform.h"

#if ENABLE_ASSEMBLER && WTF_CPU_X86_64

#include "MacroAssemblerX86Common.h"

#define REPTACH_OFFSET_CALL_R11 3

namespace JSC {

class MacroAssemblerX86_64 : public MacroAssemblerX86Common {
protected:
    static const intptr_t MinInt32 = 0xFFFFFFFF80000000;
    static const intptr_t MaxInt32 = 0x000000007FFFFFFF;

public:
    static const Scale ScalePtr = TimesEight;
    static const unsigned int TotalRegisters = 16;

    using MacroAssemblerX86Common::add32;
    using MacroAssemblerX86Common::and32;
    using MacroAssemblerX86Common::or32;
    using MacroAssemblerX86Common::sub32;
    using MacroAssemblerX86Common::load32;
    using MacroAssemblerX86Common::store32;
    using MacroAssemblerX86Common::call;
    using MacroAssemblerX86Common::loadDouble;
    using MacroAssemblerX86Common::storeDouble;
    using MacroAssemblerX86Common::convertInt32ToDouble;

    void add32(TrustedImm32 imm, AbsoluteAddress address)
    {
        move(ImmPtr(address.m_ptr), scratchRegister);
        add32(imm, Address(scratchRegister));
    }
    
    void and32(Imm32 imm, AbsoluteAddress address)
    {
        move(ImmPtr(address.m_ptr), scratchRegister);
        and32(imm, Address(scratchRegister));
    }
    
    void or32(TrustedImm32 imm, AbsoluteAddress address)
    {
        move(ImmPtr(address.m_ptr), scratchRegister);
        or32(imm, Address(scratchRegister));
    }

    void sub32(TrustedImm32 imm, AbsoluteAddress address)
    {
        move(ImmPtr(address.m_ptr), scratchRegister);
        sub32(imm, Address(scratchRegister));
    }

    void load32(const void* address, RegisterID dest)
    {
        if (dest == X86Registers::eax)
            m_assembler.movl_mEAX(address);
        else {
            move(ImmPtr(address), scratchRegister);
            load32(ImplicitAddress(scratchRegister), dest);
        }
    }

    DataLabelPtr loadDouble(const void* address, FPRegisterID dest)
    {
        DataLabelPtr label = moveWithPatch(ImmPtr(address), scratchRegister);
        loadDouble(scratchRegister, dest);
        return label;
    }

    void convertInt32ToDouble(AbsoluteAddress src, FPRegisterID dest)
    {
        move(Imm32(*static_cast<const int32_t*>(src.m_ptr)), scratchRegister);
        m_assembler.cvtsi2sd_rr(scratchRegister, dest);
    }

    void convertUInt32ToDouble(RegisterID srcDest, FPRegisterID dest)
    {
        zeroExtend32ToPtr(srcDest, srcDest);
        zeroDouble(dest); // break dependency chains
        m_assembler.cvtsq2sd_rr(srcDest, dest);
    }

    void store32(TrustedImm32 imm, void* address)
    {
        move(X86Registers::eax, scratchRegister);
        move(imm, X86Registers::eax);
        m_assembler.movl_EAXm(address);
        move(scratchRegister, X86Registers::eax);
    }

    Call call()
    {
        mozilla::DebugOnly<DataLabelPtr> label = moveWithPatch(ImmPtr(0), scratchRegister);
        Call result = Call(m_assembler.call(scratchRegister), Call::Linkable);
        ASSERT(differenceBetween(label, result) == REPTACH_OFFSET_CALL_R11);
        return result;
    }

    Call tailRecursiveCall()
    {
        mozilla::DebugOnly<DataLabelPtr> label = moveWithPatch(ImmPtr(0), scratchRegister);
        Jump newJump = Jump(m_assembler.jmp_r(scratchRegister));
        ASSERT(differenceBetween(label, newJump) == REPTACH_OFFSET_CALL_R11);
        return Call::fromTailJump(newJump);
    }

    Call makeTailRecursiveCall(Jump oldJump)
    {
        oldJump.link(this);
        mozilla::DebugOnly<DataLabelPtr> label = moveWithPatch(ImmPtr(0), scratchRegister);
        Jump newJump = Jump(m_assembler.jmp_r(scratchRegister));
        ASSERT(differenceBetween(label, newJump) == REPTACH_OFFSET_CALL_R11);
        return Call::fromTailJump(newJump);
    }


    void addPtr(RegisterID src, RegisterID dest)
    {
        m_assembler.addq_rr(src, dest);
    }

    void lea(BaseIndex address, RegisterID dest)
    {
        m_assembler.leaq_mr(address.offset, address.base, address.index, address.scale, dest);
    }

    void lea(Address address, RegisterID dest)
    {
        m_assembler.leaq_mr(address.offset, address.base, dest);
    }

    void addPtr(Imm32 imm, RegisterID srcDest)
    {
        m_assembler.addq_ir(imm.m_value, srcDest);
    }

    void addPtr(ImmPtr imm, RegisterID dest)
    {
        move(imm, scratchRegister);
        m_assembler.addq_rr(scratchRegister, dest);
    }

    void addPtr(Imm32 imm, RegisterID src, RegisterID dest)
    {
        m_assembler.leaq_mr(imm.m_value, src, dest);
    }

    void addPtr(Imm32 imm, Address address)
    {
        m_assembler.addq_im(imm.m_value, address.offset, address.base);
    }

    void addPtr(Imm32 imm, AbsoluteAddress address)
    {
        move(ImmPtr(address.m_ptr), scratchRegister);
        addPtr(imm, Address(scratchRegister));
    }
    
    void andPtr(RegisterID src, RegisterID dest)
    {
        m_assembler.andq_rr(src, dest);
    }

    void andPtr(Address src, RegisterID dest)
    {
        m_assembler.andq_mr(src.offset, src.base, dest);
    }

    void andPtr(Imm32 imm, RegisterID srcDest)
    {
        m_assembler.andq_ir(imm.m_value, srcDest);
    }

    void andPtr(ImmPtr imm, RegisterID srcDest)
    {
        intptr_t value = intptr_t(imm.m_value);

        // 32-bit immediates in 64-bit ALU ops are sign-extended.
        if (value >= MinInt32 && value <= MaxInt32) {
            andPtr(Imm32(int(value)), srcDest);
        } else {
            move(imm, scratchRegister);
            m_assembler.andq_rr(scratchRegister, srcDest);
        }
    }

    void negPtr(RegisterID srcDest)
    {
        m_assembler.negq_r(srcDest);
    }

    void notPtr(RegisterID srcDest)
    {
        m_assembler.notq_r(srcDest);
    }

    void orPtr(Address src, RegisterID dest)
    {
        m_assembler.orq_mr(src.offset, src.base, dest);
    }

    void orPtr(RegisterID src, RegisterID dest)
    {
        m_assembler.orq_rr(src, dest);
    }

    void orPtr(ImmPtr imm, RegisterID dest)
    {
        move(imm, scratchRegister);
        m_assembler.orq_rr(scratchRegister, dest);
    }

    void orPtr(Imm32 imm, RegisterID dest)
    {
        m_assembler.orq_ir(imm.m_value, dest);
    }

    void subPtr(RegisterID src, RegisterID dest)
    {
        m_assembler.subq_rr(src, dest);
    }
    
    void subPtr(Imm32 imm, RegisterID dest)
    {
        m_assembler.subq_ir(imm.m_value, dest);
    }
    
    void subPtr(ImmPtr imm, RegisterID dest)
    {
        move(imm, scratchRegister);
        m_assembler.subq_rr(scratchRegister, dest);
    }

    void xorPtr(RegisterID src, RegisterID dest)
    {
        m_assembler.xorq_rr(src, dest);
    }

    void xorPtr(Imm32 imm, RegisterID srcDest)
    {
        m_assembler.xorq_ir(imm.m_value, srcDest);
    }

    void rshiftPtr(Imm32 imm, RegisterID srcDest)
    {
        m_assembler.sarq_i8r(imm.m_value, srcDest);
    }

    void lshiftPtr(Imm32 imm, RegisterID srcDest)
    {
        m_assembler.shlq_i8r(imm.m_value, srcDest);
    }

    void loadPtr(ImplicitAddress address, RegisterID dest)
    {
        m_assembler.movq_mr(address.offset, address.base, dest);
    }

    void loadPtr(BaseIndex address, RegisterID dest)
    {
        m_assembler.movq_mr(address.offset, address.base, address.index, address.scale, dest);
    }

    void loadPtr(const void* address, RegisterID dest)
    {
        if (dest == X86Registers::eax)
            m_assembler.movq_mEAX(address);
        else {
            move(ImmPtr(address), scratchRegister);
            loadPtr(ImplicitAddress(scratchRegister), dest);
        }
    }

    DataLabel32 loadPtrWithAddressOffsetPatch(Address address, RegisterID dest)
    {
        m_assembler.movq_mr_disp32(address.offset, address.base, dest);
        return DataLabel32(this);
    }

    void storePtr(RegisterID src, ImplicitAddress address)
    {
        m_assembler.movq_rm(src, address.offset, address.base);
    }

    void storePtr(TrustedImmPtr imm, BaseIndex address)
    {
        intptr_t value = intptr_t(imm.m_value);

        // 32-bit immediates in 64-bit stores will be zero-extended, so check
        // if the value can fit in such a store.
        if (value >= 0 && value < intptr_t(0x7FFFFFFF)) {
            m_assembler.movq_i32m(int32_t(value), address.offset, address.base, address.index,
                                  address.scale);
        } else {
            move(imm, scratchRegister);
            storePtr(scratchRegister, address);
        }
    }

    void storePtr(RegisterID src, BaseIndex address)
    {
        m_assembler.movq_rm(src, address.offset, address.base, address.index, address.scale);
    }
    
    void storePtr(RegisterID src, void* address)
    {
        if (src == X86Registers::eax)
            m_assembler.movq_EAXm(address);
        else {
            move(ImmPtr(address), scratchRegister);
            storePtr(src, ImplicitAddress(scratchRegister));
        }
    }

    void storePtr(TrustedImmPtr imm, ImplicitAddress address)
    {
        intptr_t value = intptr_t(imm.m_value);

        // 32-bit immediates in 64-bit stores will be zero-extended, so check
        // if the value can fit in such a store.
        if (value >= 0 && value < intptr_t(0x7FFFFFFF)) {
            m_assembler.movq_i32m(int32_t(value), address.offset, address.base);
        } else {
            move(imm, scratchRegister);
            storePtr(scratchRegister, address);
        }
    }

    DataLabel32 storePtrWithAddressOffsetPatch(RegisterID src, Address address)
    {
        m_assembler.movq_rm_disp32(src, address.offset, address.base);
        return DataLabel32(this);
    }

    void move32(RegisterID src, RegisterID dest)
    {
        // upper 32bit will be 0
        m_assembler.movl_rr(src, dest);
    }

    void movePtrToDouble(RegisterID src, FPRegisterID dest)
    {
        m_assembler.movq_rr(src, dest);
    }

    void moveDoubleToPtr(FPRegisterID src, RegisterID dest)
    {
        m_assembler.movq_rr(src, dest);
    }

    void setPtr(Condition cond, RegisterID left, Imm32 right, RegisterID dest)
    {
        if (((cond == Equal) || (cond == NotEqual)) && !right.m_value)
            m_assembler.testq_rr(left, left);
        else
            m_assembler.cmpq_ir(right.m_value, left);
        m_assembler.setCC_r(x86Condition(cond), dest);
        m_assembler.movzbl_rr(dest, dest);
    }

    void setPtr(Condition cond, RegisterID left, RegisterID right, RegisterID dest)
    {
        m_assembler.cmpq_rr(right, left);
        m_assembler.setCC_r(x86Condition(cond), dest);
        m_assembler.movzbl_rr(dest, dest);
    }

    void setPtr(Condition cond, RegisterID left, ImmPtr right, RegisterID dest)	
    {
        move(right, scratchRegister);
        setPtr(cond, left, scratchRegister, dest);
    }

    Jump branchPtr(Condition cond, RegisterID left, RegisterID right)
    {
        m_assembler.cmpq_rr(right, left);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchPtr(Condition cond, RegisterID left, Imm32 right)
    {
        m_assembler.cmpq_ir(right.m_value, left);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchPtr(Condition cond, RegisterID left, ImmPtr right)
    {
        move(right, scratchRegister);
        return branchPtr(cond, left, scratchRegister);
    }

    Jump branchPtr(Condition cond, RegisterID left, Address right)
    {
        m_assembler.cmpq_mr(right.offset, right.base, left);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchPtr(Condition cond, AbsoluteAddress left, RegisterID right)
    {
        move(ImmPtr(left.m_ptr), scratchRegister);
        return branchPtr(cond, Address(scratchRegister), right);
    }

    Jump branchPtr(Condition cond, AbsoluteAddress left, ImmPtr right, RegisterID scratch)
    {
        move(ImmPtr(left.m_ptr), scratch);
        return branchPtr(cond, Address(scratch), right);
    }

    Jump branchPtr(Condition cond, Address left, RegisterID right)
    {
        m_assembler.cmpq_rm(right, left.offset, left.base);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchPtr(Condition cond, Address left, ImmPtr right)
    {
        move(right, scratchRegister);
        return branchPtr(cond, left, scratchRegister);
    }

    Jump branchTestPtr(Condition cond, RegisterID reg, RegisterID mask)
    {
        m_assembler.testq_rr(reg, mask);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchTestPtr(Condition cond, RegisterID reg, Imm32 mask = Imm32(-1))
    {
        // if we are only interested in the low seven bits, this can be tested with a testb
        if (mask.m_value == -1)
            m_assembler.testq_rr(reg, reg);
        else if ((mask.m_value & ~0x7f) == 0)
            m_assembler.testb_i8r(mask.m_value, reg);
        else
            m_assembler.testq_i32r(mask.m_value, reg);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchTestPtr(Condition cond, Address address, Imm32 mask = Imm32(-1))
    {
        if (mask.m_value == -1)
            m_assembler.cmpq_im(0, address.offset, address.base);
        else
            m_assembler.testq_i32m(mask.m_value, address.offset, address.base);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchTestPtr(Condition cond, BaseIndex address, Imm32 mask = Imm32(-1))
    {
        if (mask.m_value == -1)
            m_assembler.cmpq_im(0, address.offset, address.base, address.index, address.scale);
        else
            m_assembler.testq_i32m(mask.m_value, address.offset, address.base, address.index, address.scale);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }


    Jump branchAddPtr(Condition cond, RegisterID src, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Zero) || (cond == NonZero));
        addPtr(src, dest);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchSubPtr(Condition cond, Imm32 imm, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Zero) || (cond == NonZero));
        subPtr(imm, dest);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    DataLabelPtr moveWithPatch(TrustedImmPtr initialValue, RegisterID dest)
    {
        m_assembler.movq_i64r(initialValue.asIntptr(), dest);
        return DataLabelPtr(this);
    }

    Jump branchPtrWithPatch(Condition cond, RegisterID left, DataLabelPtr& dataLabel, ImmPtr initialRightValue = ImmPtr(0))
    {
        dataLabel = moveWithPatch(initialRightValue, scratchRegister);
        return branchPtr(cond, left, scratchRegister);
    }

    Jump branchPtrWithPatch(Condition cond, Address left, DataLabelPtr& dataLabel, ImmPtr initialRightValue = ImmPtr(0))
    {
        dataLabel = moveWithPatch(initialRightValue, scratchRegister);
        return branchPtr(cond, left, scratchRegister);
    }

    DataLabelPtr storePtrWithPatch(TrustedImmPtr initialValue, ImplicitAddress address)
    {
        DataLabelPtr label = moveWithPatch(initialValue, scratchRegister);
        storePtr(scratchRegister, address);
        return label;
    }

    using MacroAssemblerX86Common::branchTest8;
    Jump branchTest8(Condition cond, ExtendedAddress address, Imm32 mask = Imm32(-1))
    {
        ImmPtr addr(reinterpret_cast<void*>(address.offset));
        MacroAssemblerX86Common::move(addr, scratchRegister);
        return MacroAssemblerX86Common::branchTest8(cond, BaseIndex(scratchRegister, address.base, TimesOne), mask);
    }

    Label loadPtrWithPatchToLEA(Address address, RegisterID dest)
    {
        Label label(this);
        loadPtr(address, dest);
        return label;
    }

    void pushAllRegs()
    {
        for (int i = X86Registers::eax; i <= X86Registers::r15; i++)
            m_assembler.push_r((RegisterID)i);
    }

    void popAllRegs()
    {
        for (int i = X86Registers::r15; i >= X86Registers::eax; i--)
            m_assembler.pop_r((RegisterID)i);
    }

    void storeDouble(ImmDouble imm, Address address)
    {
        storePtr(ImmPtr(reinterpret_cast<void *>(imm.u.u64)), address);
    }

    void storeDouble(ImmDouble imm, BaseIndex address)
    {
        storePtr(ImmPtr(reinterpret_cast<void *>(imm.u.u64)), address);
    }

    bool supportsFloatingPoint() const { return true; }
    // See comment on MacroAssemblerARMv7::supportsFloatingPointTruncate()
    bool supportsFloatingPointTruncate() const { return true; }
    bool supportsFloatingPointSqrt() const { return true; }

private:
    friend class LinkBuffer;
    friend class RepatchBuffer;

    static void linkCall(void* code, Call call, FunctionPtr function)
    {
        if (!call.isFlagSet(Call::Near))
            X86Assembler::linkPointer(code, X86Assembler::labelFor(call.m_jmp, -REPTACH_OFFSET_CALL_R11), function.value());
        else
            X86Assembler::linkCall(code, call.m_jmp, function.value());
    }

    static void repatchCall(CodeLocationCall call, CodeLocationLabel destination)
    {
        X86Assembler::repatchPointer(call.dataLabelPtrAtOffset(-REPTACH_OFFSET_CALL_R11).dataLocation(), destination.executableAddress());
    }

    static void repatchCall(CodeLocationCall call, FunctionPtr destination)
    {
        X86Assembler::repatchPointer(call.dataLabelPtrAtOffset(-REPTACH_OFFSET_CALL_R11).dataLocation(), destination.executableAddress());
    }

};

} // namespace JSC

#endif // ENABLE(ASSEMBLER)

#endif /* assembler_assembler_MacroAssemblerX86_64_h */

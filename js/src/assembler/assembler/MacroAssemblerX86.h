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

#ifndef assembler_assembler_MacroAssemblerX86_h
#define assembler_assembler_MacroAssemblerX86_h

#include "assembler/wtf/Platform.h"

#if ENABLE_ASSEMBLER && WTF_CPU_X86

#include "assembler/assembler/MacroAssemblerX86Common.h"

namespace JSC {

class MacroAssemblerX86 : public MacroAssemblerX86Common {
public:
    MacroAssemblerX86()
    { }

    static const Scale ScalePtr = TimesFour;
    static const unsigned int TotalRegisters = 8;

    using MacroAssemblerX86Common::add32;
    using MacroAssemblerX86Common::and32;
    using MacroAssemblerX86Common::sub32;
    using MacroAssemblerX86Common::or32;
    using MacroAssemblerX86Common::load32;
    using MacroAssemblerX86Common::store32;
    using MacroAssemblerX86Common::branch32;
    using MacroAssemblerX86Common::call;
    using MacroAssemblerX86Common::loadDouble;
    using MacroAssemblerX86Common::storeDouble;
    using MacroAssemblerX86Common::convertInt32ToDouble;

    void add32(TrustedImm32 imm, RegisterID src, RegisterID dest)
    {
        m_assembler.leal_mr(imm.m_value, src, dest);
    }

    void lea(Address address, RegisterID dest)
    {
        m_assembler.leal_mr(address.offset, address.base, dest);
    }

    void lea(BaseIndex address, RegisterID dest)
    {
        m_assembler.leal_mr(address.offset, address.base, address.index, address.scale, dest);
    }

    void add32(Imm32 imm, AbsoluteAddress address)
    {
        m_assembler.addl_im(imm.m_value, address.m_ptr);
    }
    
    void addWithCarry32(Imm32 imm, AbsoluteAddress address)
    {
        m_assembler.adcl_im(imm.m_value, address.m_ptr);
    }
    
    void and32(Imm32 imm, AbsoluteAddress address)
    {
        m_assembler.andl_im(imm.m_value, address.m_ptr);
    }
    
    void or32(TrustedImm32 imm, AbsoluteAddress address)
    {
        m_assembler.orl_im(imm.m_value, address.m_ptr);
    }

    void sub32(TrustedImm32 imm, AbsoluteAddress address)
    {
        m_assembler.subl_im(imm.m_value, address.m_ptr);
    }

    void load32(void* address, RegisterID dest)
    {
        m_assembler.movl_mr(address, dest);
    }

    void storeDouble(ImmDouble imm, Address address)
    {
        store32(Imm32(imm.u.s.lsb), address);
        store32(Imm32(imm.u.s.msb), Address(address.base, address.offset + 4));
    }

    void storeDouble(ImmDouble imm, BaseIndex address)
    {
        store32(Imm32(imm.u.s.lsb), address);
        store32(Imm32(imm.u.s.msb),
                BaseIndex(address.base, address.index, address.scale, address.offset + 4));
    }

    DataLabelPtr loadDouble(const void* address, FPRegisterID dest)
    {
        ASSERT(isSSE2Present());
        m_assembler.movsd_mr(address, dest);
	return DataLabelPtr(this);
    }

    void convertInt32ToDouble(AbsoluteAddress src, FPRegisterID dest)
    {
        m_assembler.cvtsi2sd_mr(src.m_ptr, dest);
    }

    void convertUInt32ToDouble(RegisterID srcDest, FPRegisterID dest)
    {
        // Trick is from nanojit/Nativei386.cpp, asm_ui2d.
        static const double NegativeOne = 2147483648.0;

        // src is [0, 2^32-1]
        sub32(Imm32(0x80000000), srcDest);

        // Now src is [-2^31, 2^31-1] - int range, but not the same value.
        zeroDouble(dest);
        convertInt32ToDouble(srcDest, dest);

        // dest is now a double with the int range.
        // correct the double value by adding (0x80000000).
        move(ImmPtr(&NegativeOne), srcDest);
        addDouble(Address(srcDest), dest);
    }

    void store32(TrustedImm32 imm, void* address)
    {
        m_assembler.movl_i32m(imm.m_value, address);
    }

    void store32(RegisterID src, void* address)
    {
        m_assembler.movl_rm(src, address);
    }

    Jump branch32(Condition cond, AbsoluteAddress left, RegisterID right)
    {
        m_assembler.cmpl_rm(right, left.m_ptr);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branch32(Condition cond, AbsoluteAddress left, TrustedImm32 right)
    {
        m_assembler.cmpl_im(right.m_value, left.m_ptr);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Call call()
    {
        return Call(m_assembler.call(), Call::Linkable);
    }

    Call tailRecursiveCall()
    {
        return Call::fromTailJump(jump());
    }

    Call makeTailRecursiveCall(Jump oldJump)
    {
        return Call::fromTailJump(oldJump);
    }


    DataLabelPtr moveWithPatch(TrustedImmPtr initialValue, RegisterID dest)
    {
        m_assembler.movl_i32r(initialValue.asIntptr(), dest);
        return DataLabelPtr(this);
    }

    Jump branchPtrWithPatch(Condition cond, RegisterID left, DataLabelPtr& dataLabel, ImmPtr initialRightValue = ImmPtr(0))
    {
        m_assembler.cmpl_ir_force32(initialRightValue.asIntptr(), left);
        dataLabel = DataLabelPtr(this);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchPtrWithPatch(Condition cond, Address left, DataLabelPtr& dataLabel, ImmPtr initialRightValue = ImmPtr(0))
    {
        m_assembler.cmpl_im_force32(initialRightValue.asIntptr(), left.offset, left.base);
        dataLabel = DataLabelPtr(this);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    DataLabelPtr storePtrWithPatch(TrustedImmPtr initialValue, ImplicitAddress address)
    {
        m_assembler.movl_i32m(initialValue.asIntptr(), address.offset, address.base);
        return DataLabelPtr(this);
    }

    Label loadPtrWithPatchToLEA(Address address, RegisterID dest)
    {
        Label label(this);
        load32(address, dest);
        return label;
    }

    void pushAllRegs()
    {
        m_assembler.pusha();
    }

    void popAllRegs()
    {
        m_assembler.popa();
    }

    static bool supportsFloatingPoint() { return isSSE2Present(); }
    // See comment on MacroAssemblerARMv7::supportsFloatingPointTruncate()
    static bool supportsFloatingPointTruncate() { return isSSE2Present(); }
    static bool supportsFloatingPointSqrt() { return isSSE2Present(); }

private:
    friend class LinkBuffer;
    friend class RepatchBuffer;

    static void linkCall(void* code, Call call, FunctionPtr function)
    {
        X86Assembler::linkCall(code, call.m_jmp, function.value());
    }

    static void repatchCall(CodeLocationCall call, CodeLocationLabel destination)
    {
        X86Assembler::relinkCall(call.dataLocation(), destination.executableAddress());
    }

    static void repatchCall(CodeLocationCall call, FunctionPtr destination)
    {
        X86Assembler::relinkCall(call.dataLocation(), destination.executableAddress());
    }
};

} // namespace JSC

#endif // ENABLE(ASSEMBLER)

#endif /* assembler_assembler_MacroAssemblerX86_h */

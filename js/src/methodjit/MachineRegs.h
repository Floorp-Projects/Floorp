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

#if !defined jsjaeger_regstate_h__ && defined JS_METHODJIT
#define jsjaeger_regstate_h__

#include "jsbit.h"
#include "assembler/assembler/MacroAssembler.h"

namespace js {

namespace mjit {

struct Registers {

    typedef JSC::MacroAssembler::RegisterID RegisterID;

// TODO: Eliminate scratch register (requires rewriting register allocation mechanism)
#if defined(JS_CPU_X64)
    static const RegisterID TypeMaskReg = JSC::X86Registers::r13;
    static const RegisterID PayloadMaskReg = JSC::X86Registers::r14;
    static const RegisterID ValueReg = JSC::X86Registers::r15;
#endif

#if defined(JS_CPU_X86) || defined(JS_CPU_X64)
    static const RegisterID ReturnReg = JSC::X86Registers::eax;
# if defined(JS_CPU_X86) || defined(_MSC_VER)
    static const RegisterID ArgReg0 = JSC::X86Registers::ecx;
    static const RegisterID ArgReg1 = JSC::X86Registers::edx;
# else
    static const RegisterID ArgReg0 = JSC::X86Registers::edi;
    static const RegisterID ArgReg1 = JSC::X86Registers::esi;
# endif
#elif JS_CPU_ARM
    static const RegisterID ReturnReg = JSC::ARMRegisters::r0;
    static const RegisterID ArgReg0 = JSC::ARMRegisters::r0;
    static const RegisterID ArgReg1 = JSC::ARMRegisters::r1;
#endif

    static const RegisterID StackPointer = JSC::MacroAssembler::stackPointerRegister;

    static inline uint32 maskReg(RegisterID reg) {
        return (1 << reg);
    }

    static inline uint32 mask2Regs(RegisterID reg1, RegisterID reg2) {
        return maskReg(reg1) | maskReg(reg2);
    }

    static inline uint32 mask3Regs(RegisterID reg1, RegisterID reg2, RegisterID reg3) {
        return maskReg(reg1) | maskReg(reg2) | maskReg(reg3);
    }

#if defined(JS_CPU_X86) || defined(JS_CPU_X64)
    static const uint32 TempRegs =
          (1 << JSC::X86Registers::eax)
        | (1 << JSC::X86Registers::ecx)
        | (1 << JSC::X86Registers::edx)
# if defined(JS_CPU_X64)
        | (1 << JSC::X86Registers::r8)
        | (1 << JSC::X86Registers::r9)
        | (1 << JSC::X86Registers::r10)
#  if !defined(_MSC_VER)
        | (1 << JSC::X86Registers::esi)
        | (1 << JSC::X86Registers::edi)
#  endif
# endif
        ;

# if defined(JS_CPU_X64)
    static const uint32 SavedRegs =
        /* r11 is scratchRegister, used by JSC. */
          (1 << JSC::X86Registers::r12)
    // r13 is TypeMaskReg.
    // r14 is PayloadMaskReg.
    // r15 is ValueReg.
#  if defined(_MSC_VER)
        | (1 << JSC::X86Registers::esi)
        | (1 << JSC::X86Registers::edi)
#  endif
# else
    static const uint32 SavedRegs =
          (1 << JSC::X86Registers::esi)
        | (1 << JSC::X86Registers::edi)
# endif
        ;

    static const uint32 SingleByteRegs = (TempRegs | SavedRegs) &
        ~((1 << JSC::X86Registers::esi) |
          (1 << JSC::X86Registers::edi) |
          (1 << JSC::X86Registers::ebp) |
          (1 << JSC::X86Registers::esp));

#elif defined(JS_CPU_ARM)
    static const uint32 TempRegs =
          (1 << JSC::ARMRegisters::r0)
        | (1 << JSC::ARMRegisters::r1)
        | (1 << JSC::ARMRegisters::r2);
    // r3 is reserved as a scratch register for the assembler.

    static const uint32 SavedRegs =
          (1 << JSC::ARMRegisters::r4)
        | (1 << JSC::ARMRegisters::r5)
        | (1 << JSC::ARMRegisters::r6)
        | (1 << JSC::ARMRegisters::r7)
    // r8 is reserved as a scratch register for the assembler.
        | (1 << JSC::ARMRegisters::r9)
        | (1 << JSC::ARMRegisters::r10);
    // r11 is reserved for JSFrameReg.
    // r12 is IP, and is used for stub calls.
    // r13 is SP and must always point to VMFrame whilst in generated code.
    // r14 is LR and is used for return sequences.
    // r15 is PC (program counter).

    static const uint32 SingleByteRegs = TempRegs | SavedRegs;
#else
# error "Unsupported platform"
#endif

    static const uint32 AvailRegs = SavedRegs | TempRegs;

    Registers()
      : freeMask(AvailRegs)
    { }

    Registers(uint32 freeMask)
      : freeMask(freeMask)
    { }

    Registers(const Registers &other)
      : freeMask(other.freeMask)
    { }

    Registers & operator =(const Registers &other)
    {
        freeMask = other.freeMask;
        return *this;
    }

    void reset() {
        freeMask = AvailRegs;
    }

    bool empty() const {
        return !freeMask;
    }

    bool empty(uint32 mask) const {
        return !(freeMask & mask);
    }

    RegisterID peekReg() {
        JS_ASSERT(!empty());
        int ireg;
        JS_FLOOR_LOG2(ireg, freeMask);
        RegisterID reg = (RegisterID)ireg;
        return reg;
    }

    RegisterID takeAnyReg() {
        RegisterID reg = peekReg();
        takeReg(reg);
        return reg;
    }

    bool hasRegInMask(uint32 mask) const {
        Registers temp(freeMask & mask);
        return !temp.empty();
    }

    RegisterID takeRegInMask(uint32 mask) {
        Registers temp(freeMask & mask);
        RegisterID reg = temp.takeAnyReg();
        takeReg(reg);
        return reg;
    }

    bool hasReg(RegisterID reg) const {
        return !!(freeMask & (1 << reg));
    }

    void putRegUnchecked(RegisterID reg) {
        freeMask |= (1 << reg);
    }

    void putReg(RegisterID reg) {
        JS_ASSERT(!hasReg(reg));
        putRegUnchecked(reg);
    }

    void takeReg(RegisterID reg) {
        JS_ASSERT(hasReg(reg));
        freeMask &= ~(1 << reg);
    }

    bool operator ==(const Registers &other) {
        return freeMask == other.freeMask;
    }

    uint32 freeMask;
};


struct FPRegisters {

    typedef JSC::MacroAssembler::FPRegisterID FPRegisterID;

#if defined(JS_CPU_X86) || defined(JS_CPU_X64)
    static const uint32 TotalFPRegisters = 8;
    static const uint32 TempFPRegs =
          (1 << JSC::X86Registers::xmm0)
        | (1 << JSC::X86Registers::xmm1)
        | (1 << JSC::X86Registers::xmm2)
        | (1 << JSC::X86Registers::xmm3)
        | (1 << JSC::X86Registers::xmm4)
        | (1 << JSC::X86Registers::xmm5)
        | (1 << JSC::X86Registers::xmm6)
        | (1 << JSC::X86Registers::xmm7);
    /* FIXME: Temporary hack until FPRegister allocation exists. */
    static const FPRegisterID First  = JSC::X86Registers::xmm0;
    static const FPRegisterID Second = JSC::X86Registers::xmm1;
    static const FPRegisterID Temp0 = JSC::X86Registers::xmm2;
    static const FPRegisterID Temp1 = JSC::X86Registers::xmm3;
#elif defined(JS_CPU_ARM)
    static const uint32 TotalFPRegisters = 4;
    static const uint32 TempFPRegs = 
          (1 << JSC::ARMRegisters::d0)
        | (1 << JSC::ARMRegisters::d1)
        | (1 << JSC::ARMRegisters::d2)
        | (1 << JSC::ARMRegisters::d3);
    /* FIXME: Temporary hack until FPRegister allocation exists. */
    static const FPRegisterID First  = JSC::ARMRegisters::d0;
    static const FPRegisterID Second = JSC::ARMRegisters::d1;
    static const FPRegisterID Temp0 = JSC::ARMRegisters::d2;
    static const FPRegisterID Temp1 = JSC::ARMRegisters::d3;
#else
# error "Unsupported platform"
#endif

    static const uint32 AvailFPRegs = TempFPRegs;

    FPRegisters()
      : freeFPMask(AvailFPRegs)
    { }

    FPRegisters(uint32 freeFPMask)
      : freeFPMask(freeFPMask)
    { }

    FPRegisters(const FPRegisters &other)
      : freeFPMask(other.freeFPMask)
    { }

    FPRegisters & operator =(const FPRegisters &other)
    {
        freeFPMask = other.freeFPMask;
        return *this;
    }

    void reset() {
        freeFPMask = AvailFPRegs;
    }

    bool empty() const {
        return !freeFPMask;
    }

    bool empty(uint32 mask) const {
        return !(freeFPMask & mask);
    }

    FPRegisterID takeAnyReg() {
        JS_ASSERT(!empty());
        int ireg;
        JS_FLOOR_LOG2(ireg, freeFPMask);
        FPRegisterID reg = (FPRegisterID)ireg;
        takeReg(reg);
        return reg;
    }

    bool hasRegInMask(uint32 mask) const {
        FPRegisters temp(freeFPMask & mask);
        return !temp.empty();
    }

    FPRegisterID takeRegInMask(uint32 mask) {
        FPRegisters temp(freeFPMask & mask);
        FPRegisterID reg = temp.takeAnyReg();
        takeReg(reg);
        return reg;
    }

    bool hasReg(FPRegisterID fpreg) const {
        return !!(freeFPMask & (1 << fpreg));
    }

    void putRegUnchecked(FPRegisterID fpreg) {
        freeFPMask |= (1 << fpreg);
    }

    void putReg(FPRegisterID fpreg) {
        JS_ASSERT(!hasReg(fpreg));
        putRegUnchecked(fpreg);
    }

    void takeReg(FPRegisterID fpreg) {
        JS_ASSERT(hasReg(fpreg));
        freeFPMask &= ~(1 << fpreg);
    }

    bool operator ==(const FPRegisters &other) {
        return freeFPMask == other.freeFPMask;
    }

    uint32 freeFPMask;
};

} /* namespace mjit */

} /* namespace js */

#endif /* jsjaeger_regstate_h__ */


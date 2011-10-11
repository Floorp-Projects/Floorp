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

#include "mozilla/Util.h"

#include "assembler/assembler/MacroAssembler.h"

namespace js {

namespace mjit {

/* Common handling for both general purpose and floating point registers. */

struct AnyRegisterID {
    unsigned reg_;

    AnyRegisterID()
        : reg_((unsigned)-1)
    { pin(); }

    AnyRegisterID(const AnyRegisterID &o)
        : reg_(o.reg_)
    { pin(); }

    AnyRegisterID(JSC::MacroAssembler::RegisterID reg)
        : reg_((unsigned)reg)
    { pin(); }

    AnyRegisterID(JSC::MacroAssembler::FPRegisterID reg)
        : reg_(JSC::MacroAssembler::TotalRegisters + (unsigned)reg)
    { pin(); }

    static inline AnyRegisterID fromRaw(unsigned reg);

    inline JSC::MacroAssembler::RegisterID reg();
    inline JSC::MacroAssembler::FPRegisterID fpreg();

    bool isSet() { return reg_ != unsigned(-1); }
    bool isReg() { return reg_ < JSC::MacroAssembler::TotalRegisters; }
    bool isFPReg() { return isSet() && !isReg(); }

    inline const char * name();

  private:
    unsigned * pin() {
        /*
         * Workaround for apparent compiler bug in GCC 4.2. If GCC thinks that reg_
         * cannot escape then it compiles isReg() and other accesses to reg_ incorrectly.
         */
        static unsigned *v;
        v = &reg_;
        return v;
    }
};

struct Registers {

    /* General purpose registers. */

    static const uint32 TotalRegisters = JSC::MacroAssembler::TotalRegisters;

    enum CallConvention {
        NormalCall,
        FastCall
    };

    typedef JSC::MacroAssembler::RegisterID RegisterID;

    // Homed and scratch registers for working with Values on x64.
#if defined(JS_CPU_X64)
    static const RegisterID TypeMaskReg = JSC::X86Registers::r13;
    static const RegisterID PayloadMaskReg = JSC::X86Registers::r14;
    static const RegisterID ValueReg = JSC::X86Registers::r10;
    static const RegisterID ScratchReg = JSC::X86Registers::r11;
#endif

    // Register that homes the current JSStackFrame.
#if defined(JS_CPU_X86)
    static const RegisterID JSFrameReg = JSC::X86Registers::ebp;
#elif defined(JS_CPU_X64)
    static const RegisterID JSFrameReg = JSC::X86Registers::ebx;
#elif defined(JS_CPU_ARM)
    static const RegisterID JSFrameReg = JSC::ARMRegisters::r10;
#elif defined(JS_CPU_SPARC)
    static const RegisterID JSFrameReg = JSC::SparcRegisters::l0;
#endif

#if defined(JS_CPU_X86) || defined(JS_CPU_X64)
    static const RegisterID ReturnReg = JSC::X86Registers::eax;
# if defined(JS_CPU_X86) || defined(_WIN64)
    static const RegisterID ArgReg0 = JSC::X86Registers::ecx;
    static const RegisterID ArgReg1 = JSC::X86Registers::edx;
#  if defined(JS_CPU_X64)
    static const RegisterID ArgReg2 = JSC::X86Registers::r8;
    static const RegisterID ArgReg3 = JSC::X86Registers::r9;
#  endif
# else
    static const RegisterID ArgReg0 = JSC::X86Registers::edi;
    static const RegisterID ArgReg1 = JSC::X86Registers::esi;
    static const RegisterID ArgReg2 = JSC::X86Registers::edx;
    static const RegisterID ArgReg3 = JSC::X86Registers::ecx;
# endif
#elif JS_CPU_ARM
    static const RegisterID ReturnReg = JSC::ARMRegisters::r0;
    static const RegisterID ArgReg0 = JSC::ARMRegisters::r0;
    static const RegisterID ArgReg1 = JSC::ARMRegisters::r1;
    static const RegisterID ArgReg2 = JSC::ARMRegisters::r2;
#elif JS_CPU_SPARC
    static const RegisterID ReturnReg = JSC::SparcRegisters::o0;
    static const RegisterID ArgReg0 = JSC::SparcRegisters::o0;
    static const RegisterID ArgReg1 = JSC::SparcRegisters::o1;
    static const RegisterID ArgReg2 = JSC::SparcRegisters::o2;
    static const RegisterID ArgReg3 = JSC::SparcRegisters::o3;
    static const RegisterID ArgReg4 = JSC::SparcRegisters::o4;
    static const RegisterID ArgReg5 = JSC::SparcRegisters::o5;
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
# if defined(JS_CPU_X86)
        | (1 << JSC::X86Registers::ebx)
# endif
        | (1 << JSC::X86Registers::ecx)
        | (1 << JSC::X86Registers::edx)
# if defined(JS_CPU_X64)
        | (1 << JSC::X86Registers::r8)
        | (1 << JSC::X86Registers::r9)
#  if !defined(_WIN64)
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
        | (1 << JSC::X86Registers::r15)
#  if defined(_WIN64)
        | (1 << JSC::X86Registers::esi)
        | (1 << JSC::X86Registers::edi)
#  endif
# else
    static const uint32 SavedRegs =
          (1 << JSC::X86Registers::esi)
        | (1 << JSC::X86Registers::edi)
# endif
        ;

# if defined(JS_CPU_X86)
    static const uint32 SingleByteRegs = (TempRegs | SavedRegs) &
        ~((1 << JSC::X86Registers::esi) |
          (1 << JSC::X86Registers::edi) |
          (1 << JSC::X86Registers::ebp) |
          (1 << JSC::X86Registers::esp));
# elif defined(JS_CPU_X64)
    static const uint32 SingleByteRegs = TempRegs | SavedRegs;
# endif

#elif defined(JS_CPU_ARM)
    static const uint32 TempRegs =
          (1 << JSC::ARMRegisters::r0)
        | (1 << JSC::ARMRegisters::r1)
        | (1 << JSC::ARMRegisters::r2);
    // r3 is reserved as a scratch register for the assembler.
    // r12 is IP, and is used for stub calls.

    static const uint32 SavedRegs =
          (1 << JSC::ARMRegisters::r4)
        | (1 << JSC::ARMRegisters::r5)
        | (1 << JSC::ARMRegisters::r6)
        | (1 << JSC::ARMRegisters::r7)
    // r8 is reserved as a scratch register for the assembler.
        | (1 << JSC::ARMRegisters::r9);
    // r10 is reserved for JSFrameReg.
    // r13 is SP and must always point to VMFrame whilst in generated code.
    // r14 is LR and is used for return sequences.
    // r15 is PC (program counter).

    static const uint32 SingleByteRegs = TempRegs | SavedRegs;
#elif defined(JS_CPU_SPARC)
    static const uint32 TempRegs =
          (1 << JSC::SparcRegisters::o0)
        | (1 << JSC::SparcRegisters::o1)
        | (1 << JSC::SparcRegisters::o2)
        | (1 << JSC::SparcRegisters::o3)
        | (1 << JSC::SparcRegisters::o4)
        | (1 << JSC::SparcRegisters::o5);

    static const uint32 SavedRegs =
          (1 << JSC::SparcRegisters::l2)
        | (1 << JSC::SparcRegisters::l3)
        | (1 << JSC::SparcRegisters::l4)
        | (1 << JSC::SparcRegisters::l5)
        | (1 << JSC::SparcRegisters::l6)
        | (1 << JSC::SparcRegisters::l7);

    static const uint32 SingleByteRegs = TempRegs | SavedRegs;
#else
# error "Unsupported platform"
#endif

    static const uint32 AvailRegs = SavedRegs | TempRegs;

    static bool isAvail(RegisterID reg) {
        uint32 mask = maskReg(reg);
        return bool(mask & AvailRegs);
    }

    static bool isSaved(RegisterID reg) {
        uint32 mask = maskReg(reg);
        JS_ASSERT(mask & AvailRegs);
        return bool(mask & SavedRegs);
    }

    static inline uint32 numArgRegs(CallConvention convention) {
#if defined(JS_CPU_X86)
# if defined(JS_NO_FASTCALL)
        return 0;
# else
        return (convention == FastCall) ? 2 : 0;
# endif
#elif defined(JS_CPU_X64)
# ifdef _WIN64
        return 4;
# else
        return 6;
# endif
#elif defined(JS_CPU_ARM)
        return 4;
#elif defined(JS_CPU_SPARC)
        return 6;
#endif
    }

    static inline bool regForArg(CallConvention conv, uint32 i, RegisterID *reg) {
#if defined(JS_CPU_X86)
        static const RegisterID regs[] = {
            JSC::X86Registers::ecx,
            JSC::X86Registers::edx
        };

# if defined(JS_NO_FASTCALL)
        return false;
# else
        if (conv == NormalCall)
            return false;
# endif
#elif defined(JS_CPU_X64)
# ifdef _WIN64
        static const RegisterID regs[] = {
            JSC::X86Registers::ecx,
            JSC::X86Registers::edx,
            JSC::X86Registers::r8,
            JSC::X86Registers::r9
        };
# else
        static const RegisterID regs[] = {
            JSC::X86Registers::edi,
            JSC::X86Registers::esi,
            JSC::X86Registers::edx,
            JSC::X86Registers::ecx,
            JSC::X86Registers::r8,
            JSC::X86Registers::r9
        };
# endif
#elif defined(JS_CPU_ARM)
        static const RegisterID regs[] = {
            JSC::ARMRegisters::r0,
            JSC::ARMRegisters::r1,
            JSC::ARMRegisters::r2,
            JSC::ARMRegisters::r3
        };
#elif defined(JS_CPU_SPARC)
        static const RegisterID regs[] = {
            JSC::SparcRegisters::o0,
            JSC::SparcRegisters::o1,
            JSC::SparcRegisters::o2,
            JSC::SparcRegisters::o3,
            JSC::SparcRegisters::o4,
            JSC::SparcRegisters::o5
        };
#endif
        JS_ASSERT(numArgRegs(conv) == mozilla::ArrayLength(regs));
        if (i > mozilla::ArrayLength(regs))
            return false;
        *reg = regs[i];
        return true;
    }

    /* Floating point registers. */

    typedef JSC::MacroAssembler::FPRegisterID FPRegisterID;

#if defined(JS_CPU_X86) || defined(JS_CPU_X64)
#ifdef _WIN64
    /* xmm0-xmm5 are scratch register on Win64 ABI */
    static const uint32 TotalFPRegisters = 5;
    static const FPRegisterID FPConversionTemp = JSC::X86Registers::xmm5;
#else
    static const uint32 TotalFPRegisters = 7;
    static const FPRegisterID FPConversionTemp = JSC::X86Registers::xmm7;
#endif
    static const uint32 TempFPRegs = (
          (1 << JSC::X86Registers::xmm0)
        | (1 << JSC::X86Registers::xmm1)
        | (1 << JSC::X86Registers::xmm2)
        | (1 << JSC::X86Registers::xmm3)
        | (1 << JSC::X86Registers::xmm4)
#ifndef _WIN64
        | (1 << JSC::X86Registers::xmm5)
        | (1 << JSC::X86Registers::xmm6)
#endif
        ) << TotalRegisters;
#elif defined(JS_CPU_ARM)
    static const uint32 TotalFPRegisters = 3;
    static const uint32 TempFPRegs = (
          (1 << JSC::ARMRegisters::d0)
        | (1 << JSC::ARMRegisters::d1)
        | (1 << JSC::ARMRegisters::d2)
        ) << TotalRegisters;
    static const FPRegisterID FPConversionTemp = JSC::ARMRegisters::d3;
#elif defined(JS_CPU_SPARC)
    static const uint32 TotalFPRegisters = 8;
    static const uint32 TempFPRegs = (uint32)(
          (1 << JSC::SparcRegisters::f0)
        | (1 << JSC::SparcRegisters::f2)
        | (1 << JSC::SparcRegisters::f4)
        | (1 << JSC::SparcRegisters::f6)
        ) << TotalRegisters;
    static const FPRegisterID FPConversionTemp = JSC::SparcRegisters::f8;
#else
# error "Unsupported platform"
#endif

    /* Temp reg that can be clobbered when setting up a fallible fast or ABI call. */
#if defined(JS_CPU_X86) || defined(JS_CPU_X64)
    static const RegisterID ClobberInCall = JSC::X86Registers::ecx;
#elif defined(JS_CPU_ARM)
    static const RegisterID ClobberInCall = JSC::ARMRegisters::r2;
#elif defined(JS_CPU_SPARC)
    static const RegisterID ClobberInCall = JSC::SparcRegisters::l1;
#endif

    static const uint32 AvailFPRegs = TempFPRegs;

    static inline uint32 maskReg(FPRegisterID reg) {
        return (1 << reg) << TotalRegisters;
    }

    /* Common code. */

    static const uint32 TotalAnyRegisters = TotalRegisters + TotalFPRegisters;
    static const uint32 TempAnyRegs = TempRegs | TempFPRegs;
    static const uint32 AvailAnyRegs = AvailRegs | AvailFPRegs;

    static inline uint32 maskReg(AnyRegisterID reg) {
        return (1 << reg.reg_);
    }

    /* Get a register which is not live before a FASTCALL. */
    static inline RegisterID tempCallReg() {
        Registers regs(TempRegs);
        regs.takeReg(Registers::ArgReg0);
        regs.takeReg(Registers::ArgReg1);
        return regs.takeAnyReg().reg();
    }

    /* Get a register which is not live before a normal ABI call with at most four args. */
    static inline Registers tempCallRegMask() {
        Registers regs(AvailRegs);
#ifndef JS_CPU_X86
        regs.takeReg(ArgReg0);
        regs.takeReg(ArgReg1);
        regs.takeReg(ArgReg2);
#if defined(JS_CPU_SPARC) || defined(JS_CPU_X64)
        regs.takeReg(ArgReg3);
#endif
#endif
        return regs;
    }

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

    bool empty(uint32 mask) const {
        return !(freeMask & mask);
    }

    bool empty() const {
        return !freeMask;
    }

    AnyRegisterID peekReg(uint32 mask) {
        JS_ASSERT(!empty(mask));
        unsigned ireg;
        JS_FLOOR_LOG2(ireg, freeMask & mask);
        return AnyRegisterID::fromRaw(ireg);
    }

    AnyRegisterID peekReg() {
        return peekReg(freeMask);
    }

    AnyRegisterID takeAnyReg(uint32 mask) {
        AnyRegisterID reg = peekReg(mask);
        takeReg(reg);
        return reg;
    }

    AnyRegisterID takeAnyReg() {
        return takeAnyReg(freeMask);
    }

    bool hasReg(AnyRegisterID reg) const {
        return !!(freeMask & (1 << reg.reg_));
    }

    bool hasRegInMask(uint32 mask) const {
        return !!(freeMask & mask);
    }

    bool hasAllRegs(uint32 mask) const {
        return (freeMask & mask) == mask;
    }

    void putRegUnchecked(AnyRegisterID reg) {
        freeMask |= (1 << reg.reg_);
    }

    void putReg(AnyRegisterID reg) {
        JS_ASSERT(!hasReg(reg));
        putRegUnchecked(reg);
    }

    void takeReg(AnyRegisterID reg) {
        JS_ASSERT(hasReg(reg));
        takeRegUnchecked(reg);
    }

    void takeRegUnchecked(AnyRegisterID reg) {
        freeMask &= ~(1 << reg.reg_);
    }

    bool operator ==(const Registers &other) {
        return freeMask == other.freeMask;
    }

    uint32 freeMask;
};

static const JSC::MacroAssembler::RegisterID JSFrameReg = Registers::JSFrameReg;

AnyRegisterID
AnyRegisterID::fromRaw(unsigned reg_)
{
    JS_ASSERT(reg_ < Registers::TotalAnyRegisters);
    AnyRegisterID reg;
    reg.reg_ = reg_;
    return reg;
}

JSC::MacroAssembler::RegisterID
AnyRegisterID::reg()
{
    JS_ASSERT(reg_ < Registers::TotalRegisters);
    return (JSC::MacroAssembler::RegisterID) reg_;
}

JSC::MacroAssembler::FPRegisterID
AnyRegisterID::fpreg()
{
    JS_ASSERT(reg_ >= Registers::TotalRegisters &&
              reg_ < Registers::TotalAnyRegisters);
    return (JSC::MacroAssembler::FPRegisterID) (reg_ - Registers::TotalRegisters);
}

const char *
AnyRegisterID::name()
{
#if defined(JS_CPU_X86) || defined(JS_CPU_X64)
    return isReg() ? JSC::X86Registers::nameIReg(reg()) : JSC::X86Registers::nameFPReg(fpreg());
#elif defined(JS_CPU_ARM)
    return isReg() ? JSC::ARMAssembler::nameGpReg(reg()) : JSC::ARMAssembler::nameFpRegD(fpreg());
#else
    return "???";
#endif
}

} /* namespace mjit */

} /* namespace js */

#endif /* jsjaeger_regstate_h__ */


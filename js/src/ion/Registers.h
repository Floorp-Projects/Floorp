/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_cpu_registers_h__
#define jsion_cpu_registers_h__

#include "jsutil.h"
#include "IonTypes.h"
#if defined(JS_CPU_X86)
# include "x86/Architecture-x86.h"
#elif defined(JS_CPU_X64)
# include "x64/Architecture-x64.h"
#elif defined(JS_CPU_ARM)
# include "arm/Architecture-arm.h"
#endif
#include "FixedArityList.h"

// ARM defines the RegisterID within Architecture-arm.h
#if !defined(JS_CPU_ARM) && defined(JS_METHODJIT)
#include "assembler/assembler/MacroAssembler.h"
#endif

namespace js {
namespace ion {

struct Register {
    typedef Registers Codes;
    typedef Codes::Code Code;
    typedef js::ion::Registers::RegisterID RegisterID;
    Code code_;

    static Register FromCode(uint32_t i) {
        JS_ASSERT(i < Registers::Total);
        Register r = { (Registers::Code)i };
        return r;
    }
    Code code() const {
        JS_ASSERT((uint32_t)code_ < Registers::Total);
        return code_;
    }
    const char *name() const {
        return Registers::GetName(code());
    }
    bool operator ==(const Register &other) const {
        return code_ == other.code_;
    }
    bool operator !=(const Register &other) const {
        return code_ != other.code_;
    }
    bool volatile_() const {
        return !!((1 << code()) & Registers::VolatileMask);
    }
};

struct FloatRegister {
    typedef FloatRegisters Codes;
    typedef Codes::Code Code;

    Code code_;

    static FloatRegister FromCode(uint32_t i) {
        JS_ASSERT(i < FloatRegisters::Total);
        FloatRegister r = { (FloatRegisters::Code)i };
        return r;
    }
    Code code() const {
        JS_ASSERT((uint32_t)code_ < FloatRegisters::Total);
        return code_;
    }
    const char *name() const {
        return FloatRegisters::GetName(code());
    }
    bool operator ==(const FloatRegister &other) const {
        return code_ == other.code_;
    }
    bool operator !=(const FloatRegister &other) const {
        return code_ != other.code_;
    }
    bool volatile_() const {
        return !!((1 << code()) & FloatRegisters::VolatileMask);
    }
};

// Information needed to recover machine register state.
class MachineState
{
    FixedArityList<uintptr_t *, Registers::Total> regs_;
    FixedArityList<double *, FloatRegisters::Total> fpregs_;

  public:
    static MachineState FromBailout(uintptr_t regs[Registers::Total],
                                    double fpregs[FloatRegisters::Total]);

    void setRegisterLocation(Register reg, uintptr_t *up) {
        regs_[reg.code()] = up;
    }
    void setRegisterLocation(FloatRegister reg, double *dp) {
        fpregs_[reg.code()] = dp;
    }

    bool has(Register reg) const {
        return regs_[reg.code()] != NULL;
    }
    bool has(FloatRegister reg) const {
        return fpregs_[reg.code()] != NULL;
    }
    uintptr_t read(Register reg) const {
        return *regs_[reg.code()];
    }
    double read(FloatRegister reg) const {
        return *fpregs_[reg.code()];
    }
};

} // namespace ion
} // namespace js

#endif // jsion_cpu_registers_h__


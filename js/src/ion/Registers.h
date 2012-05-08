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
#ifndef JS_CPU_ARM
#include "assembler/assembler/MacroAssembler.h"
#endif

namespace js {
namespace ion {

struct Register {
    typedef Registers Codes;
    typedef Codes::Code Code;
    typedef js::ion::Registers::RegisterID RegisterID;
    Code code_;

    static Register FromCode(uint32 i) {
        JS_ASSERT(i < Registers::Total);
        Register r = { (Registers::Code)i };
        return r;
    }
    Code code() const {
        JS_ASSERT((uint32)code_ < Registers::Total);
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

    static FloatRegister FromCode(uint32 i) {
        JS_ASSERT(i < FloatRegisters::Total);
        FloatRegister r = { (FloatRegisters::Code)i };
        return r;
    }
    Code code() const {
        JS_ASSERT((uint32)code_ < FloatRegisters::Total);
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


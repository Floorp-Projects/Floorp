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

#ifndef jsion_cpu_assembler_h__
#define jsion_cpu_assembler_h__

#if defined(JS_CPU_X86)
# include "x86/Architecture-x86.h"
#elif defined(JS_CPU_X64)
# include "x64/Architecture-x64.h"
#elif defined(JS_CPU_ARM)
# include "arm/Architecture-ARM.h"
#endif

namespace js {
namespace ion {

struct Register {
    typedef RegisterCodes Codes;
    typedef Codes::Code Code;

    Code code_;

    static Register FromCode(uint32 i) {
        JS_ASSERT(i < RegisterCodes::Total);
        Register r = { (RegisterCodes::Code)i };
        return r;
    }
    Code code() const {
        JS_ASSERT(code_ < RegisterCodes::Total);
        return code_;
    }
    const char *name() const {
        return RegisterCodes::GetName(code());
    }
};

struct FloatRegister {
    typedef FloatRegisterCodes Codes;
    typedef Codes::Code Code;

    Code code_;

    static FloatRegister FromCode(uint32 i) {
        JS_ASSERT(i < FloatRegisterCodes::Total);
        FloatRegister r = { (FloatRegisterCodes::Code)i };
        return r;
    }
    Code code() const {
        JS_ASSERT(code_ < FloatRegisterCodes::Total);
        return code_;
    }
    const char *name() const {
        return FloatRegisterCodes::GetName(code());
    }
};

struct AnyRegister {
    union {
        RegisterCodes::Code gpr_;
        FloatRegisterCodes::Code fpu_;
    };
    bool isFloat_;

    AnyRegister()
    { }
    explicit AnyRegister(Register gpr) {
        gpr_ = gpr.code();
        isFloat_ = false;
    }
    explicit AnyRegister(FloatRegister fpu) {
        fpu_ = fpu.code();
        isFloat_ = true;
    }
    bool isFloat() const {
        return isFloat_;
    }
    Register gpr() const {
        JS_ASSERT(!isFloat());
        return Register::FromCode(gpr_);
    }
    FloatRegister fpu() const {
        JS_ASSERT(isFloat());
        return FloatRegister::FromCode(fpu_);
    }
    bool operator ==(const AnyRegister &other) {
        return isFloat()
               ? (other.isFloat() && fpu_ == other.fpu_)
               : (!other.isFloat() && gpr_ == other.gpr_);
    }
    bool operator !=(const AnyRegister &other) {
        return isFloat()
               ? (!other.isFloat() || fpu_ != other.fpu_)
               : (other.isFloat() || gpr_ != other.gpr_);
    }
};

template <typename T>
class TypedRegisterSet
{
    uint32 bits_;

    explicit TypedRegisterSet(uint32 bits)
      : bits_(bits)
    { }

  public:
    TypedRegisterSet() : bits_(0)
    { }
    static inline TypedRegisterSet All() {
        return TypedRegisterSet(T::Codes::AllocatableMask);
    }
    static inline TypedRegisterSet Intersect(const TypedRegisterSet &lhs,
                                             const TypedRegisterSet &rhs) {
        return TypedRegisterSet(lhs.bits_ & rhs.bits_);
    }
    static inline TypedRegisterSet Not(const TypedRegisterSet &in) {
        return TypedRegisterSet(~in.bits_ & T::Codes::AllocatableMask);
    }
    void intersect(TypedRegisterSet other) {
        bits_ &= ~other.bits_;
    }
    bool has(T reg) const {
        return !!(bits_ & (1 << reg.code()));
    }
    void addUnchecked(T reg) {
        bits_ |= (1 << reg.code());
    }
    void add(T reg) {
        JS_ASSERT(!has(reg));
        addUnchecked(reg);
    }
    bool empty() const {
        return !bits_;
    }
    void take(T reg) {
        JS_ASSERT(has(reg));
        bits_ &= ~(1 << reg.code());
    }
    T getAny() const {
        JS_ASSERT(!empty());
        int ireg;
        JS_FLOOR_LOG2(ireg, bits_);
        return T::FromCode(ireg);
    }
    T takeAny() {
        JS_ASSERT(!empty());
        T reg = getAny();
        take(reg);
        return reg;
    }
};

typedef TypedRegisterSet<Register> GeneralRegisterSet;
typedef TypedRegisterSet<FloatRegister> FloatRegisterSet;

class RegisterSet {
    GeneralRegisterSet gpr_;
    FloatRegisterSet fpu_;

  public:
    RegisterSet()
    { }
    RegisterSet(const GeneralRegisterSet &gpr, const FloatRegisterSet &fpu)
      : gpr_(gpr),
        fpu_(fpu)
    { }
    static inline RegisterSet All() {
        return RegisterSet(GeneralRegisterSet::All(), FloatRegisterSet::All());
    }
    static inline RegisterSet Intersect(const RegisterSet &lhs, const RegisterSet &rhs) {
        return RegisterSet(GeneralRegisterSet::Intersect(lhs.gpr_, rhs.gpr_),
                           FloatRegisterSet::Intersect(lhs.fpu_, rhs.fpu_));
    }
    static inline RegisterSet Not(const RegisterSet &in) {
        return RegisterSet(GeneralRegisterSet::Not(in.gpr_),
                           FloatRegisterSet::Not(in.fpu_));
    }
    bool has(Register reg) const {
        return gpr_.has(reg);
    }
    bool has(FloatRegister reg) const {
        return fpu_.has(reg);
    }
    bool has(AnyRegister reg) const {
        return reg.isFloat() ? has(reg.fpu()) : has(reg.gpr());
    }
    void add(Register reg) {
        gpr_.add(reg);
    }
    void add(FloatRegister reg) {
        fpu_.add(reg);
    }
    void add(const AnyRegister &any) {
        if (any.isFloat())
            add(any.fpu());
        else
            add(any.gpr());
    }
    void addUnchecked(Register reg) {
        gpr_.addUnchecked(reg);
    }
    void addUnchecked(FloatRegister reg) {
        fpu_.addUnchecked(reg);
    }
    void addUnchecked(const AnyRegister &any) {
        if (any.isFloat())
            addUnchecked(any.fpu());
        else
            addUnchecked(any.gpr());
    }
    bool empty(bool floats) const {
        return floats ? fpu_.empty() : gpr_.empty();
    }
    FloatRegister takeFloat() {
        return fpu_.takeAny();
    }
    Register takeGeneral() {
        return gpr_.takeAny();
    }
    void take(const AnyRegister &reg) {
        if (reg.isFloat())
            fpu_.take(reg.fpu());
        else
            gpr_.take(reg.gpr());
    }
    AnyRegister takeAny(bool isFloat) {
        if (isFloat)
            return AnyRegister(takeFloat());
        return AnyRegister(takeGeneral());
    }
};

} // namespace js
} // namespace ion

#if defined(JS_CPU_X86)
# include "x86/Assembler-x86.h"
# include "x86/StackAssignment-x86.h"
#elif defined(JS_CPU_X64)
# include "x64/Assembler-x64.h"
# include "x64/StackAssignment-x64.h"
#elif defined(JS_CPU_ARM)
# include "arm/Assembler-ARM.h"
# include "arm/StackAssignment-arm.h"
#endif

#endif // jsion_cpu_assembler_h__


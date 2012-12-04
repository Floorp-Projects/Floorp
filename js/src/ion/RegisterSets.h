/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_cpu_registersets_h__
#define jsion_cpu_registersets_h__

#include "Registers.h"
#include "TypeOracle.h"

namespace js {
namespace ion {

struct AnyRegister {
    typedef uint32_t Code;

    static const uint32_t Total = Registers::Total + FloatRegisters::Total;
    static const uint32_t Invalid = UINT_MAX;

    union {
        Registers::Code gpr_;
        FloatRegisters::Code fpu_;
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
    static AnyRegister FromCode(uint32_t i) {
        JS_ASSERT(i < Total);
        AnyRegister r;
        if (i < Registers::Total) {
            r.gpr_ = Register::Code(i);
            r.isFloat_ = false;
        } else {
            r.fpu_ = FloatRegister::Code(i - Registers::Total);
            r.isFloat_ = true;
        }
        return r;
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
    bool operator ==(const AnyRegister &other) const {
        return isFloat()
               ? (other.isFloat() && fpu_ == other.fpu_)
               : (!other.isFloat() && gpr_ == other.gpr_);
    }
    bool operator !=(const AnyRegister &other) const {
        return isFloat()
               ? (!other.isFloat() || fpu_ != other.fpu_)
               : (other.isFloat() || gpr_ != other.gpr_);
    }
    const char *name() const {
        return isFloat()
               ? FloatRegister::FromCode(fpu_).name()
               : Register::FromCode(gpr_).name();
    }
    const Code code() const {
        return isFloat()
               ? fpu_ + Registers::Total
               : gpr_;
    }
    bool volatile_() const {
        return isFloat() ? fpu().volatile_() : gpr().volatile_();
    }
};

// Registers to hold a boxed value. Uses one register on 64 bit
// platforms, two registers on 32 bit platforms.
class ValueOperand
{
#if defined(JS_NUNBOX32)
    Register type_;
    Register payload_;

  public:
    ValueOperand(Register type, Register payload)
      : type_(type), payload_(payload)
    { }

    Register typeReg() const {
        return type_;
    }
    Register payloadReg() const {
        return payload_;
    }

    Register scratchReg() const {
        return payloadReg();
    }

#elif defined(JS_PUNBOX64)
    Register value_;

  public:
    explicit ValueOperand(Register value)
      : value_(value)
    { }

    Register valueReg() const {
        return value_;
    }

    Register scratchReg() const {
        return valueReg();
    }

#endif

    ValueOperand() {}
};

// Registers to hold either either a typed or untyped value.
class TypedOrValueRegister
{
    // Type of value being stored.
    MIRType type_;

    // Space to hold either an AnyRegister or a ValueOperand.
    union U {
        mozilla::AlignedStorage2<AnyRegister> typed;
        mozilla::AlignedStorage2<ValueOperand> value;
    } data;

    AnyRegister &dataTyped() {
        JS_ASSERT(hasTyped());
        return *data.typed.addr();
    }
    ValueOperand &dataValue() {
        JS_ASSERT(hasValue());
        return *data.value.addr();
    }

  public:

    TypedOrValueRegister()
      : type_(MIRType_None)
    {}

    TypedOrValueRegister(MIRType type, AnyRegister reg)
      : type_(type)
    {
        dataTyped() = reg;
    }

    TypedOrValueRegister(ValueOperand value)
      : type_(MIRType_Value)
    {
        dataValue() = value;
    }

    MIRType type() const {
        return type_;
    }

    bool hasTyped() const {
        return type() != MIRType_None && type() != MIRType_Value;
    }

    bool hasValue() const {
        return type() == MIRType_Value;
    }

    AnyRegister typedReg() {
        return dataTyped();
    }

    ValueOperand valueReg() {
        return dataValue();
    }
};

// A constant value, or registers to hold a typed/untyped value.
class ConstantOrRegister
{
    // Whether a constant value is being stored.
    bool constant_;

    // Space to hold either a Value or a TypedOrValueRegister.
    union U {
        mozilla::AlignedStorage2<Value> constant;
        mozilla::AlignedStorage2<TypedOrValueRegister> reg;
    } data;

    Value &dataValue() {
        JS_ASSERT(constant());
        return *data.constant.addr();
    }
    TypedOrValueRegister &dataReg() {
        JS_ASSERT(!constant());
        return *data.reg.addr();
    }

  public:

    ConstantOrRegister()
    {}

    ConstantOrRegister(Value value)
      : constant_(true)
    {
        dataValue() = value;
    }

    ConstantOrRegister(TypedOrValueRegister reg)
      : constant_(false)
    {
        dataReg() = reg;
    }

    bool constant() {
        return constant_;
    }

    Value value() {
        return dataValue();
    }

    TypedOrValueRegister reg() {
        return dataReg();
    }
};

struct Int32Key {
    bool isRegister_;
    union {
        Register reg_;
        int32_t constant_;
    };

    explicit Int32Key(Register reg)
      : isRegister_(true), reg_(reg)
    { }

    explicit Int32Key(int32_t index)
      : isRegister_(false), constant_(index)
    { }

    inline void bumpConstant(int diff) {
        JS_ASSERT(!isRegister_);
        constant_ += diff;
    }
    inline Register reg() const {
        JS_ASSERT(isRegister_);
        return reg_;
    }
    inline int32_t constant() const {
        JS_ASSERT(!isRegister_);
        return constant_;
    }
    inline bool isRegister() const {
        return isRegister_;
    }
    inline bool isConstant() const {
        return !isRegister_;
    }
};

template <typename T>
class TypedRegisterSet
{
    uint32_t bits_;

  public:
    explicit TypedRegisterSet(uint32_t bits)
      : bits_(bits)
    { }

    TypedRegisterSet() : bits_(0)
    { }
    TypedRegisterSet(const TypedRegisterSet<T> &set) : bits_(set.bits_)
    { }

    static inline TypedRegisterSet All() {
        return TypedRegisterSet(T::Codes::AllocatableMask);
    }
    static inline TypedRegisterSet Intersect(const TypedRegisterSet &lhs,
                                             const TypedRegisterSet &rhs) {
        return TypedRegisterSet(lhs.bits_ & rhs.bits_);
    }
    static inline TypedRegisterSet Union(const TypedRegisterSet &lhs,
                                         const TypedRegisterSet &rhs) {
        return TypedRegisterSet(lhs.bits_ | rhs.bits_);
    }
    static inline TypedRegisterSet Not(const TypedRegisterSet &in) {
        return TypedRegisterSet(~in.bits_ & T::Codes::AllocatableMask);
    }
    static inline TypedRegisterSet VolatileNot(const TypedRegisterSet &in) {
        const uint32_t allocatableVolatile =
            T::Codes::AllocatableMask & T::Codes::VolatileMask;
        return TypedRegisterSet(~in.bits_ & allocatableVolatile);
    }
    static inline TypedRegisterSet Volatile() {
        return TypedRegisterSet(T::Codes::AllocatableMask & T::Codes::VolatileMask);
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
    // Determemine if some register are still allocated.  This function should
    // be used with the set of allocatable registers used for the initialization
    // of the current set.
    bool someAllocated(const TypedRegisterSet &allocatable) const {
        return allocatable.bits_ & ~bits_;
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
    void clear() {
        bits_ = 0;
    }
    uint32_t bits() const {
        return bits_;
    }
    uint32_t size() const {
        uint32_t sum2  = (bits_ & 0x55555555) + ((bits_ & 0xaaaaaaaa) >> 1);
        uint32_t sum4  = (sum2  & 0x33333333) + ((sum2  & 0xcccccccc) >> 2);
        uint32_t sum8  = (sum4  & 0x0f0f0f0f) + ((sum4  & 0xf0f0f0f0) >> 4);
        uint32_t sum16 = (sum8  & 0x00ff00ff) + ((sum8  & 0xff00ff00) >> 8);
        return sum16;
    }
    bool operator ==(const TypedRegisterSet<T> &other) const {
        return other.bits_ == bits_;
    }
};

typedef TypedRegisterSet<Register> GeneralRegisterSet;
typedef TypedRegisterSet<FloatRegister> FloatRegisterSet;

class AnyRegisterIterator;

class RegisterSet {
    GeneralRegisterSet gpr_;
    FloatRegisterSet fpu_;

    friend class AnyRegisterIterator;

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
    static inline RegisterSet Union(const RegisterSet &lhs, const RegisterSet &rhs) {
        return RegisterSet(GeneralRegisterSet::Union(lhs.gpr_, rhs.gpr_),
                           FloatRegisterSet::Union(lhs.fpu_, rhs.fpu_));
    }
    static inline RegisterSet Not(const RegisterSet &in) {
        return RegisterSet(GeneralRegisterSet::Not(in.gpr_),
                           FloatRegisterSet::Not(in.fpu_));
    }
    static inline RegisterSet VolatileNot(const RegisterSet &in) {
        return RegisterSet(GeneralRegisterSet::VolatileNot(in.gpr_),
                           FloatRegisterSet::VolatileNot(in.fpu_));
    }
    static inline RegisterSet Volatile() {
        return RegisterSet(GeneralRegisterSet::Volatile(), FloatRegisterSet::Volatile());
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
    void add(ValueOperand value) {
#if defined(JS_NUNBOX32)
        add(value.payloadReg());
        add(value.typeReg());
#elif defined(JS_PUNBOX64)
        add(value.valueReg());
#else
#error "Bad architecture"
#endif
    }
    void add(TypedOrValueRegister reg) {
        if (reg.hasValue())
            add(reg.valueReg());
        else if (reg.hasTyped())
            add(reg.typedReg());
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
    ValueOperand takeValueOperand() {
#if defined(JS_NUNBOX32)
        return ValueOperand(takeGeneral(), takeGeneral());
#elif defined(JS_PUNBOX64)
        return ValueOperand(takeGeneral());
#else
#error "Bad architecture"
#endif
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
    void clear() {
        gpr_.clear();
        fpu_.clear();
    }
    GeneralRegisterSet gprs() const {
        return gpr_;
    }
    FloatRegisterSet fpus() const {
        return fpu_;
    }
    bool operator ==(const RegisterSet &other) const {
        return other.gpr_ == gpr_ && other.fpu_ == fpu_;
    }

    void maybeTake(Register reg) {
        if (gpr_.has(reg))
            gpr_.take(reg);
    }
    void maybeTake(FloatRegister reg) {
        if (fpu_.has(reg))
            fpu_.take(reg);
    }
    void maybeTake(AnyRegister reg) {
        if (has(reg))
            take(reg);
    }
    void maybeTake(ValueOperand value) {
#if defined(JS_NUNBOX32)
        if (gpr_.has(value.typeReg()))
            gpr_.take(value.typeReg());
        if (gpr_.has(value.payloadReg()))
            gpr_.take(value.payloadReg());
#elif defined(JS_PUNBOX64)
        if (gpr_.has(value.valueReg()))
            gpr_.take(value.valueReg());
#else
#error "Bad architecture"
#endif
    }
    void maybeTake(TypedOrValueRegister reg) {
        if (reg.hasValue())
            maybeTake(reg.valueReg());
        else if (reg.hasTyped())
            maybeTake(reg.typedReg());
    }
};

template <typename T>
class TypedRegisterIterator
{
    TypedRegisterSet<T> regset_;

  public:
    TypedRegisterIterator(TypedRegisterSet<T> regset) : regset_(regset)
    { }
    TypedRegisterIterator(const TypedRegisterIterator &other) : regset_(other.regset_)
    { }

    bool more() const {
        return !regset_.empty();
    }
    TypedRegisterIterator<T> operator ++(int) {
        TypedRegisterIterator<T> old(*this);
        regset_.takeAny();
        return old;
    }
    T operator *() const {
        return regset_.getAny();
    }
};

typedef TypedRegisterIterator<Register> GeneralRegisterIterator;
typedef TypedRegisterIterator<FloatRegister> FloatRegisterIterator;

class AnyRegisterIterator
{
    GeneralRegisterIterator geniter_;
    FloatRegisterIterator floatiter_;

  public:
    AnyRegisterIterator()
      : geniter_(GeneralRegisterSet::All()), floatiter_(FloatRegisterSet::All())
    { }
    AnyRegisterIterator(GeneralRegisterSet genset, FloatRegisterSet floatset)
      : geniter_(genset), floatiter_(floatset)
    { }
    AnyRegisterIterator(const RegisterSet &set)
      : geniter_(set.gpr_), floatiter_(set.fpu_)
    { }
    AnyRegisterIterator(const AnyRegisterIterator &other)
      : geniter_(other.geniter_), floatiter_(other.floatiter_)
    { }
    bool more() const {
        return geniter_.more() || floatiter_.more();
    }
    AnyRegisterIterator operator ++(int) {
        AnyRegisterIterator old(*this);
        if (geniter_.more())
            geniter_++;
        else
            floatiter_++;
        return old;
    }
    AnyRegister operator *() const {
        if (geniter_.more())
            return AnyRegister(*geniter_);
        return AnyRegister(*floatiter_);
    }
};

} // namespace ion
} // namespace js

#endif // jsion_cpu_registersets_h__

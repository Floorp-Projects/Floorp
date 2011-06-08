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

#ifndef jsion_lir_h__
#define jsion_lir_h__

// This file declares the core data structures for LIR: storage allocations for
// inputs and outputs, as well as the interface instructions must conform to.

#include "jscntxt.h"
#include "IonAllocPolicy.h"
#include "InlineList.h"
#include "IonAssembler.h"
#include "FixedArityList.h"
#include "LOpcodes.h"

namespace js {
namespace ion {

class LUse;
class LGeneralReg;
class LFloatReg;
class LStackSlot;
class LArgument;
class LConstantIndex;

static const uint32 MAX_VIRTUAL_REGISTERS = (1 << 21) - 1;

#if defined(JS_NUNBOX32)
# define BOX_PIECES         2
#elif defined(JS_PUNBOX64)
# define BOX_PIECES         1
#else
# error "Unknown!"
#endif

// Represents storage for an operand. For constants, the pointer is tagged
// with a single bit, and the untagged pointer is a pointer to a Value.
class LAllocation
{
    uintptr_t bits_;

  protected:
    static const uintptr_t TAG_BIT = 1;
    static const uintptr_t TAG_SHIFT = 0;
    static const uintptr_t TAG_MASK = (1 << TAG_BIT) - 1;
    static const uintptr_t KIND_BITS = 3;
    static const uintptr_t KIND_SHIFT = TAG_SHIFT + TAG_BIT;
    static const uintptr_t KIND_MASK = (1 << KIND_BITS) - 1;
    static const uintptr_t DATA_BITS = (sizeof(uint32) * 8) - KIND_BITS;
    static const uintptr_t DATA_SHIFT = KIND_SHIFT + KIND_BITS;
    static const uintptr_t DATA_MASK = (1 << DATA_BITS) - 1;

  public:
    enum Kind {
        USE,            // Use of a virtual register, with physical allocation policy.
        CONSTANT_VALUE, // Constant js::Value.
        CONSTANT_INDEX, // Constant arbitrary index.
        GPR,            // General purpose register.
        FPU,            // Floating-point register.
        STACK_SLOT,     // 32-bit stack slot.
        DOUBLE_SLOT,    // 64-bit stack slot.
        ARGUMENT        // Argument slot.
    };

  protected:
    bool isTagged() const {
        return !!(bits_ & TAG_MASK);
    }
    Kind kind() const {
        if (isTagged())
            return CONSTANT_VALUE;
        return (Kind)((bits_ >> KIND_SHIFT) & KIND_MASK);
    }

    int32 data() const {
        return int32(bits_) >> DATA_SHIFT;
    }
    void setData(int32 data) {
        JS_ASSERT(int32(data) <= int32(DATA_MASK));
        bits_ &= ~(DATA_MASK << DATA_SHIFT);
        bits_ |= (data << DATA_SHIFT);
    }
    void setKindAndData(Kind kind, uint32 data) {
        JS_ASSERT(int32(data) <= int32(DATA_MASK));
        bits_ = (uint32(kind) << KIND_SHIFT) | data << DATA_SHIFT;
    }

    LAllocation(Kind kind, uint32 data) {
        setKindAndData(kind, data);
    }
    explicit LAllocation(Kind kind) {
        setKindAndData(kind, 0);
    }

  public:
    LAllocation() : bits_(0)
    { }

    // The value pointer must be rooted in MIR and have its low bit cleared.
    explicit LAllocation(const Value *vp) {
        bits_ = uintptr_t(vp);
        JS_ASSERT(!isTagged());
        bits_ |= TAG_MASK;
    }

    bool isUse() const {
        return kind() == USE;
    }
    bool isConstant() const {
        return kind() == CONSTANT_VALUE || kind() == CONSTANT_INDEX;
    }
    bool isValue() const {
        return kind() == CONSTANT_VALUE;
    }
    bool isGeneralReg() const {
        return kind() == GPR;
    }
    bool isFloatReg() const {
        return kind() == FPU;
    }
    bool isStackSlot() const {
        return kind() == STACK_SLOT || kind() == DOUBLE_SLOT;
    }
    bool isArgument() const {
        return kind() == ARGUMENT;
    }
    inline LUse *toUse();
    inline LGeneralReg *toGeneralReg();
    inline LFloatReg *toFloatReg();
    inline LStackSlot *toStackSlot();
    inline LArgument *toArgument();
    inline LConstantIndex *toConstantIndex();

    const Value *toConstant() const {
        JS_ASSERT(isConstant());
        return reinterpret_cast<const Value *>(bits_ & ~TAG_MASK);
    }
};

class LUse : public LAllocation
{
    static const uint32 POLICY_BITS = 2;
    static const uint32 POLICY_SHIFT = 0;
    static const uint32 POLICY_MASK = (1 << POLICY_BITS) - 1;
    static const uint32 FLAG_BITS = 1;
    static const uint32 FLAG_SHIFT = POLICY_SHIFT + POLICY_BITS;
    static const uint32 FLAG_MASK = (1 << FLAG_BITS) - 1;
    static const uint32 REG_BITS = 5;
    static const uint32 REG_SHIFT = FLAG_SHIFT + FLAG_BITS;
    static const uint32 REG_MASK = (1 << REG_BITS) - 1;

    // Virtual registers get the remaining 21 bits.
    static const uint32 VREG_BITS = DATA_BITS - (REG_SHIFT + REG_BITS);
    static const uint32 VREG_SHIFT = REG_SHIFT + REG_BITS;
    static const uint32 VREG_MASK = (1 << VREG_BITS) - 1;

  public:
    enum Policy {
        ANY,                // Register or stack slot.
        REGISTER,           // Must have a register.
        TEMPORARY,          // Temporary register, killed at the end.
        FIXED               // A specific register is required.
    };

    // The register is killed at the start, meaning it can be re-used as either
    // a temporary or an output. Only one input per register class can have
    // this on a single instruction.
    static const uint32 KILLED_AT_START = (1 << 0);

    void set(Policy policy, uint32 reg, uint32 flags) {
        JS_ASSERT(flags <= KILLED_AT_START);
        setKindAndData(USE, (policy << POLICY_SHIFT) |
                            (flags << FLAG_SHIFT) |
                            (reg << REG_SHIFT));
    }

  public:
    LUse(Policy policy, uint32 flags = 0) {
        set(policy, 0, flags);
    }
    LUse(Register reg, uint32 flags = 0) {
        set(FIXED, reg.code(), flags);
    }
    LUse(FloatRegister reg, uint32 flags = 0) {
        set(FIXED, reg.code(), flags);
    }

    void setVirtualRegister(uint32 index) {
        JS_STATIC_ASSERT(VREG_MASK <= MAX_VIRTUAL_REGISTERS);
        JS_ASSERT(index < VREG_MASK);

        uint32 old = data() & ~(VREG_MASK << VREG_SHIFT);
        setData(old | (index << VREG_SHIFT));
    }

    Policy policy() const {
        Policy policy = (Policy)((data() >> POLICY_SHIFT) & POLICY_MASK);
        return policy;
    }
    bool killedAtStart() const {
        return !!(flags() & KILLED_AT_START);
    }
    uint32 vreg() const {
        uint32 index = (data() >> VREG_SHIFT) & VREG_MASK;
        return index;
    }
    uint32 flags() const {
        return (data() >> FLAG_SHIFT) & FLAG_MASK;
    }
    uint32 registerCode() const {
        JS_ASSERT(policy() == FIXED);
        return (data() >> REG_SHIFT) & REG_MASK;
    }
};

class LGeneralReg : public LAllocation
{
  public:
    explicit LGeneralReg(Register reg)
      : LAllocation(GPR, reg.code())
    { }

    Register reg() const {
        return Register::FromCode(data());
    }
};

class LFloatReg : public LAllocation
{
  public:
    explicit LFloatReg(FloatRegister reg)
      : LAllocation(FPU, reg.code())
    { }

    FloatRegister reg() const {
        return FloatRegister::FromCode(data());
    }
};

// Arbitrary constant index.
class LConstantIndex : public LAllocation
{
  public:
    explicit LConstantIndex(uint32 index)
      : LAllocation(CONSTANT_INDEX, index)
    { }

    uint32 index() const {
        return data();
    }
};

// Stack slots are indexes into the stack, given that each slot is size
// STACK_SLOT_SIZE.
class LStackSlot : public LAllocation
{
  public:
    explicit LStackSlot(uint32 slot)
      : LAllocation(STACK_SLOT, slot)
    { }

    bool isDouble() const {
        return kind() == DOUBLE_SLOT;
    }

    uint32 slot() const {
        return data();
    }
};

// Arguments are reverse indexes into the stack, and like LStackSlot, each
// index is measured in increments of STACK_SLOT_SIZE.
class LArgument : public LAllocation
{
  public:
    explicit LArgument(int32 index)
      : LAllocation(ARGUMENT, index)
    { }

    int32 index() const {
        return data();
    }
};

// Represents storage for a definition.
class LDefinition
{
    // Bits containing policy, type, and virtual register.
    uint32 bits_;

    // Before register allocation, this optionally contains a fixed policy.
    // Current, preset fixed policies are limited to stack assignments.
    // Register allocation assigns this field to a physical policy.
    LAllocation output_;

    static const uint32 TYPE_BITS = 3;
    static const uint32 TYPE_SHIFT = 0;
    static const uint32 TYPE_MASK = (1 << TYPE_BITS) - 1;
    static const uint32 POLICY_BITS = 2;
    static const uint32 POLICY_SHIFT = TYPE_SHIFT + TYPE_BITS;
    static const uint32 POLICY_MASK = (1 << POLICY_BITS) - 1;

    static const uint32 VREG_BITS = (sizeof(bits_) * 8) - (POLICY_BITS + TYPE_BITS);
    static const uint32 VREG_SHIFT = POLICY_SHIFT + POLICY_BITS;
    static const uint32 VREG_MASK = (1 << VREG_BITS) - 1;

  public:
    // Note that definitions, by default, are always allocated a register,
    // unless the policy specifies that an input can be re-used and that input
    // is a stack slot.
    enum Policy {
        DEFAULT,                    // Register policy.
        CAN_REUSE_FIRST_INPUT,      // The first input can be re-used as the output.
        MUST_REUSE_FIRST_INPUT,     // The first input MUST be re-used as the output.
        PRESET                      // The policy is fixed, predetermined by the
                                    // allocation field.
    };

    enum Type {
        INTEGER,    // Generic, integer data (GPR).
        POINTER,    // Generic, pointer-width data (GPR).
        OBJECT,     // Pointer that may be collected as garbage (GPR).
        DOUBLE,     // 64-bit point value (FPU).
        TYPE,       // Type tag, for nunbox systems.
        PAYLOAD,    // Payload, for nunbox systems.
        BOX         // Joined box, for punbox systems.
    };

    void set(uint32 index, Type type, Policy policy) {
        JS_STATIC_ASSERT(MAX_VIRTUAL_REGISTERS <= VREG_MASK);
        bits_ = (index << VREG_SHIFT) | (policy << POLICY_SHIFT) | (type << TYPE_SHIFT);
    }

  public:
    LDefinition(uint32 index, Type type, Policy policy = DEFAULT) {
        set(index, type, policy);
    }

    LDefinition(Type type, Policy policy = DEFAULT) {
        set(0, type, policy);
    }

    LDefinition(Type type, const LAllocation &a)
      : output_(a)
    {
        set(0, type, PRESET);
    }

    LDefinition() : bits_(0)
    { }

    Policy policy() const {
        return (Policy)((bits_ >> POLICY_SHIFT) & POLICY_MASK);
    }
    Type type() const {
        return (Type)((bits_ >> TYPE_SHIFT) & TYPE_MASK);
    }
    uint32 vreg() const {
        return (bits_ >> VREG_SHIFT) & VREG_MASK;
    }
    const LAllocation &output() const {
        return output_;
    }

    void setVirtualRegister(uint32 index) {
        JS_ASSERT(index < VREG_MASK);
        bits_ &= ~(VREG_MASK << VREG_SHIFT);
        bits_ |= index << VREG_SHIFT;
    }
    void setOutput(const LAllocation &a) {
        output_ = a;
    }
};

// Forward declarations of LIR types.
#define LIROP(op) class L##op;
    LIR_OPCODE_LIST(LIROP)
#undef LIROP

class LInstruction : public TempObject,
                     public InlineListNode<LInstruction>
{
  public:
    enum Opcode {
#   define LIROP(name) LOp_##name,
        LIR_OPCODE_LIST(LIROP)
#   undef LIROP
        LOp_Invalid
    };

  public:
    virtual Opcode op() const = 0;

    // Returns the number of outputs of this instruction. If an output is
    // unallocated, it is an LDefinition, defining a virtual register.
    virtual size_t numDefs() const = 0;
    virtual LDefinition *getDef(size_t index) = 0;

    // Returns information about operands. Each unallocated operand is an LUse
    // with a non-TEMPORARY policy.
    virtual size_t numOperands() const = 0;
    virtual LAllocation *getOperand(size_t index) = 0;

    // Returns information about temporary registers needed. Each temporary
    // register is an LUse with a TEMPORARY policy, or a fixed register.
    virtual size_t numTemps() const = 0;
    virtual LAllocation *getTemp(size_t index) = 0;

  public:
    // Opcode testing and casts.
#   define LIROP(name)                                                      \
    bool is##name() const {                                                 \
        return op() == LOp_##name;                                          \
    }                                                                       \
    inline L##name *to##name();
    LIR_OPCODE_LIST(LIROP)
#   undef LIROP
};

template <size_t Defs, size_t Operands, size_t Temps>
class LInstructionHelper : public LInstruction
{
    FixedArityList<LDefinition, Defs> defs_;
    FixedArityList<LAllocation, Operands> operands_;
    FixedArityList<LAllocation, Temps> temps_;

  public:
    size_t numDefs() const {
        return Defs;
    }
    LDefinition *getDef(size_t index) {
        return &defs_[index];
    }
    size_t numOperands() const {
        return Operands;
    }
    LAllocation *getOperand(size_t index) {
        return &operands_[index];
    }
    size_t numTemps() const {
        return Temps;
    }
    LAllocation *getTemp(size_t index) {
        return &temps_[index];
    }

    void setDef(size_t index, const LDefinition &def) {
        defs_[index] = def;
    }
    void setOperand(size_t index, const LAllocation &a) {
        operands_[index] = a;
    }
};

LUse *LAllocation::toUse() {
    JS_ASSERT(isUse());
    return static_cast<LUse *>(this);
}
LGeneralReg *LAllocation::toGeneralReg() {
    JS_ASSERT(isGeneralReg());
    return static_cast<LGeneralReg *>(this);
}
LFloatReg *LAllocation::toFloatReg() {
    JS_ASSERT(isFloatReg());
    return static_cast<LFloatReg *>(this);
}
LStackSlot *LAllocation::toStackSlot() {
    JS_ASSERT(isStackSlot());
    return static_cast<LStackSlot *>(this);
}
LArgument *LAllocation::toArgument() {
    JS_ASSERT(isArgument());
    return static_cast<LArgument *>(this);
}
LConstantIndex *LAllocation::toConstantIndex() {
    JS_ASSERT(isConstant() && !isValue());
    return static_cast<LConstantIndex *>(this);
}

} // namespace ion
} // namespace js

#define LIR_HEADER(opcode)                                                  \
    Opcode op() const {                                                     \
        return LInstruction::LOp_##opcode;                                  \
    }

#include "LIR-Common.h"
#if defined(JS_CPU_X86)
# include "x86/LIR-x86.h"
#elif defined(JS_CPU_X64)
# include "x64/LIR-x64.h"
#elif defined(JS_CPU_ARM)
# include "arm/LIR-ARM.h"
#endif

#undef LIR_HEADER

#endif // jsion_lir_h__


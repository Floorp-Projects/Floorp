/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_TypePolicy_h
#define jit_TypePolicy_h

#include "jit/IonAllocPolicy.h"
#include "jit/IonTypes.h"

namespace js {
namespace jit {

class MInstruction;
class MDefinition;

// A type policy directs the type analysis phases, which insert conversion,
// boxing, unboxing, and type changes as necessary.
class TypePolicy
{
  public:
    // Analyze the inputs of the instruction and perform one of the following
    // actions for each input:
    //  * Nothing; the input already type-checks.
    //  * If untyped, optionally ask the input to try and specialize its value.
    //  * Replace the operand with a conversion instruction.
    //  * Insert an unconditional deoptimization (no conversion possible).
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *def) = 0;
};

struct TypeSpecializationData
{
  protected:
    // Specifies three levels of specialization:
    //  - < Value. This input is expected and required.
    //  - == None. This op should not be specialized.
    MIRType specialization_;

    MIRType thisTypeSpecialization() {
        return specialization_;
    }

  public:
    MIRType specialization() const {
        return specialization_;
    }
};

#define EMPTY_DATA_                                     \
    struct Data                                         \
    {                                                   \
        static TypePolicy *thisTypePolicy();            \
    }

#define INHERIT_DATA_(DATA_TYPE)                        \
    struct Data : public DATA_TYPE                      \
    {                                                   \
        static TypePolicy *thisTypePolicy();            \
    }

#define SPECIALIZATION_DATA_ INHERIT_DATA_(TypeSpecializationData)

class BoxInputsPolicy : public TypePolicy
{
  protected:
    static MDefinition *boxAt(TempAllocator &alloc, MInstruction *at, MDefinition *operand);

  public:
    EMPTY_DATA_;
    static MDefinition *alwaysBoxAt(TempAllocator &alloc, MInstruction *at, MDefinition *operand);
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *def);
};

class ArithPolicy : public BoxInputsPolicy
{
  public:
    SPECIALIZATION_DATA_;
    bool adjustInputs(TempAllocator &alloc, MInstruction *def);
};

class BitwisePolicy : public BoxInputsPolicy
{
  public:
    SPECIALIZATION_DATA_;
    bool adjustInputs(TempAllocator &alloc, MInstruction *def);
};

class ComparePolicy : public BoxInputsPolicy
{
  public:
    EMPTY_DATA_;
    bool adjustInputs(TempAllocator &alloc, MInstruction *def);
};

// Policy for MTest instructions.
class TestPolicy : public BoxInputsPolicy
{
  public:
    EMPTY_DATA_;
    bool adjustInputs(TempAllocator &alloc, MInstruction *ins);
};

class TypeBarrierPolicy : public BoxInputsPolicy
{
  public:
    EMPTY_DATA_;
    bool adjustInputs(TempAllocator &alloc, MInstruction *ins);
};

class CallPolicy : public BoxInputsPolicy
{
  public:
    EMPTY_DATA_;
    bool adjustInputs(TempAllocator &alloc, MInstruction *def);
};

// Policy for MPow. First operand Double; second Double or Int32.
class PowPolicy : public BoxInputsPolicy
{
  public:
    SPECIALIZATION_DATA_;
    bool adjustInputs(TempAllocator &alloc, MInstruction *ins);
};

// Expect a string for operand Op. If the input is a Value, it is unboxed.
template <unsigned Op>
class StringPolicy : public BoxInputsPolicy
{
  public:
    EMPTY_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
    bool adjustInputs(TempAllocator &alloc, MInstruction *def) {
        return staticAdjustInputs(alloc, def);
    }
};

// Expect a string for operand Op. Else a ToString instruction is inserted.
template <unsigned Op>
class ConvertToStringPolicy : public TypePolicy
{
  public:
    EMPTY_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
    bool adjustInputs(TempAllocator &alloc, MInstruction *def) {
        return staticAdjustInputs(alloc, def);
    }
};

// Expect an Int for operand Op. If the input is a Value, it is unboxed.
template <unsigned Op>
class IntPolicy : public BoxInputsPolicy
{
  public:
    EMPTY_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
    bool adjustInputs(TempAllocator &alloc, MInstruction *def) {
        return staticAdjustInputs(alloc, def);
    }
};

// Expect an Int for operand Op. Else a ToInt32 instruction is inserted.
template <unsigned Op>
class ConvertToInt32Policy : public BoxInputsPolicy
{
  public:
    EMPTY_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
    bool adjustInputs(TempAllocator &alloc, MInstruction *def) {
        return staticAdjustInputs(alloc, def);
    }
};

// Expect a double for operand Op. If the input is a Value, it is unboxed.
template <unsigned Op>
class DoublePolicy : public BoxInputsPolicy
{
  public:
    EMPTY_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
    bool adjustInputs(TempAllocator &alloc, MInstruction *def) {
        return staticAdjustInputs(alloc, def);
    }
};

// Expect a float32 for operand Op. If the input is a Value, it is unboxed.
template <unsigned Op>
class Float32Policy : public BoxInputsPolicy
{
  public:
    EMPTY_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
    bool adjustInputs(TempAllocator &alloc, MInstruction *def) {
        return staticAdjustInputs(alloc, def);
    }
};

// Expect a float32 OR a double for operand Op, but will prioritize Float32
// if the result type is set as such. If the input is a Value, it is unboxed.
template <unsigned Op>
class FloatingPointPolicy : public TypePolicy
{

  public:
    struct PolicyTypeData
    {
        MIRType policyType_;

        void setPolicyType(MIRType type) {
            policyType_ = type;
        }

      protected:
        MIRType &thisTypeSpecialization() {
            return policyType_;
        }
    };

    INHERIT_DATA_(PolicyTypeData);

    bool adjustInputs(TempAllocator &alloc, MInstruction *def);
};

template <unsigned Op>
class NoFloatPolicy : public TypePolicy
{
  public:
    EMPTY_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
    bool adjustInputs(TempAllocator &alloc, MInstruction *def) {
        return staticAdjustInputs(alloc, def);
    }
};

// Policy for guarding variadic instructions such as object / array state
// instructions.
template <unsigned FirstOp>
class NoFloatPolicyAfter : public TypePolicy
{
  public:
    EMPTY_DATA_;
    bool adjustInputs(TempAllocator &alloc, MInstruction *ins);
};

// Box objects or strings as an input to a ToDouble instruction.
class ToDoublePolicy : public BoxInputsPolicy
{
  public:
    EMPTY_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
    bool adjustInputs(TempAllocator &alloc, MInstruction *def) {
        return staticAdjustInputs(alloc, def);
    }
};

// Box objects, strings and undefined as input to a ToInt32 instruction.
class ToInt32Policy : public BoxInputsPolicy
{
  public:
    EMPTY_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
    bool adjustInputs(TempAllocator &alloc, MInstruction *def) {
        return staticAdjustInputs(alloc, def);
    }
};

// Box objects as input to a ToString instruction.
class ToStringPolicy : public BoxInputsPolicy
{
  public:
    EMPTY_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
    bool adjustInputs(TempAllocator &alloc, MInstruction *def) {
        return staticAdjustInputs(alloc, def);
    }
};

template <unsigned Op>
class ObjectPolicy : public BoxInputsPolicy
{
  public:
    EMPTY_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *ins);
    bool adjustInputs(TempAllocator &alloc, MInstruction *ins) {
        return staticAdjustInputs(alloc, ins);
    }
};

// Single-object input. If the input is a Value, it is unboxed. If it is
// a primitive, we use ValueToNonNullObject.
typedef ObjectPolicy<0> SingleObjectPolicy;

template <unsigned Op>
class BoxPolicy : public BoxInputsPolicy
{
  public:
    EMPTY_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *ins);
    bool adjustInputs(TempAllocator &alloc, MInstruction *ins) {
        return staticAdjustInputs(alloc, ins);
    }
};

// Boxes everything except inputs of type Type.
template <unsigned Op, MIRType Type>
class BoxExceptPolicy : public TypePolicy
{
  public:
    EMPTY_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *ins);
    bool adjustInputs(TempAllocator &alloc, MInstruction *ins) {
        return staticAdjustInputs(alloc, ins);
    }
};

// Combine multiple policies.
template <class Lhs, class Rhs>
class MixPolicy : public TypePolicy
{
  public:
    EMPTY_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *ins) {
        return Lhs::staticAdjustInputs(alloc, ins) && Rhs::staticAdjustInputs(alloc, ins);
    }
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *ins) {
        return staticAdjustInputs(alloc, ins);
    }
};

// Combine three policies.
template <class Policy1, class Policy2, class Policy3>
class Mix3Policy : public TypePolicy
{
  public:
    EMPTY_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *ins) {
        return Policy1::staticAdjustInputs(alloc, ins) &&
               Policy2::staticAdjustInputs(alloc, ins) &&
               Policy3::staticAdjustInputs(alloc, ins);
    }
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *ins) {
        return staticAdjustInputs(alloc, ins);
    }
};

class CallSetElementPolicy : public SingleObjectPolicy
{
  public:
    EMPTY_DATA_;
    bool adjustInputs(TempAllocator &alloc, MInstruction *def);
};

// First operand will be boxed to a Value (except for an object)
// Second operand (if specified) will forcefully be unboxed to an object
class InstanceOfPolicy : public TypePolicy
{
  public:
    EMPTY_DATA_;
    bool adjustInputs(TempAllocator &alloc, MInstruction *def);
};

class StoreTypedArrayPolicy : public BoxInputsPolicy
{
  protected:
    bool adjustValueInput(TempAllocator &alloc, MInstruction *ins, int arrayType, MDefinition *value, int valueOperand);

  public:
    EMPTY_DATA_;
    bool adjustInputs(TempAllocator &alloc, MInstruction *ins);
};

class StoreTypedArrayHolePolicy : public StoreTypedArrayPolicy
{
  public:
    EMPTY_DATA_;
    bool adjustInputs(TempAllocator &alloc, MInstruction *ins);
};

class StoreTypedArrayElementStaticPolicy : public StoreTypedArrayPolicy
{
  public:
    EMPTY_DATA_;
    bool adjustInputs(TempAllocator &alloc, MInstruction *ins);
};

// Accepts integers and doubles. Everything else is boxed.
class ClampPolicy : public BoxInputsPolicy
{
  public:
    EMPTY_DATA_;
    bool adjustInputs(TempAllocator &alloc, MInstruction *ins);
};

class FilterTypeSetPolicy : public BoxInputsPolicy
{
  public:
    EMPTY_DATA_;
    bool adjustInputs(TempAllocator &alloc, MInstruction *ins);
};

static inline bool
CoercesToDouble(MIRType type)
{
    if (type == MIRType_Undefined || IsFloatingPointType(type))
        return true;
    return false;
}

#undef SPECIALIZATION_DATA_
#undef INHERIT_DATA_
#undef EMPTY_DATA_

} // namespace jit
} // namespace js

#endif /* jit_TypePolicy_h */

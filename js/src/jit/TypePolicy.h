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

class BoxInputsPolicy : public TypePolicy
{
  protected:
    static MDefinition *boxAt(TempAllocator &alloc, MInstruction *at, MDefinition *operand);

  public:
    static MDefinition *alwaysBoxAt(TempAllocator &alloc, MInstruction *at, MDefinition *operand);
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *def);
};

class ArithPolicy : public BoxInputsPolicy
{
  protected:
    // Specifies three levels of specialization:
    //  - < Value. This input is expected and required.
    //  - == Any. Inputs are probably primitive.
    //  - == None. This op should not be specialized.
    MIRType specialization_;

  public:
    bool adjustInputs(TempAllocator &alloc, MInstruction *def);
};

class BitwisePolicy : public BoxInputsPolicy
{
  protected:
    // Specifies three levels of specialization:
    //  - < Value. This input is expected and required.
    //  - == Any. Inputs are probably primitive.
    //  - == None. This op should not be specialized.
    MIRType specialization_;

  public:
    bool adjustInputs(TempAllocator &alloc, MInstruction *def);

    MIRType specialization() const {
        return specialization_;
    }
};

class ComparePolicy : public BoxInputsPolicy
{
    bool adjustInputs(TempAllocator &alloc, MInstruction *def);
};

// Policy for MTest instructions.
class TestPolicy : public BoxInputsPolicy
{
  public:
    bool adjustInputs(TempAllocator &alloc, MInstruction *ins);
};

class TypeBarrierPolicy : public BoxInputsPolicy
{
  public:
    bool adjustInputs(TempAllocator &alloc, MInstruction *ins);
};

class CallPolicy : public BoxInputsPolicy
{
  public:
    bool adjustInputs(TempAllocator &alloc, MInstruction *def);
};

// Policy for MPow. First operand Double; second Double or Int32.
class PowPolicy : public BoxInputsPolicy
{
    MIRType specialization_;

  public:
    explicit PowPolicy(MIRType specialization)
      : specialization_(specialization)
    { }

    bool adjustInputs(TempAllocator &alloc, MInstruction *ins);
};

// Expect a string for operand Op. If the input is a Value, it is unboxed.
template <unsigned Op>
class StringPolicy : public BoxInputsPolicy
{
  public:
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
    MIRType policyType_;

  public:
    bool adjustInputs(TempAllocator &alloc, MInstruction *def) {
        if (policyType_ == MIRType_Double)
            return DoublePolicy<Op>::staticAdjustInputs(alloc, def);
        return Float32Policy<Op>::staticAdjustInputs(alloc, def);
    }
    void setPolicyType(MIRType type) {
        policyType_ = type;
    }
};

template <unsigned Op>
class NoFloatPolicy : public TypePolicy
{
  public:
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
    bool adjustInputs(TempAllocator &alloc, MInstruction *def) {
        return staticAdjustInputs(alloc, def);
    }
};

// Box objects or strings as an input to a ToDouble instruction.
class ToDoublePolicy : public BoxInputsPolicy
{
  public:
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
    bool adjustInputs(TempAllocator &alloc, MInstruction *def) {
        return staticAdjustInputs(alloc, def);
    }
};

// Box objects, strings and undefined as input to a ToInt32 instruction.
class ToInt32Policy : public BoxInputsPolicy
{
  public:
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
    bool adjustInputs(TempAllocator &alloc, MInstruction *def) {
        return staticAdjustInputs(alloc, def);
    }
};

// Box objects as input to a ToString instruction.
class ToStringPolicy : public BoxInputsPolicy
{
  public:
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
    bool adjustInputs(TempAllocator &alloc, MInstruction *def) {
        return staticAdjustInputs(alloc, def);
    }
};

template <unsigned Op>
class ObjectPolicy : public BoxInputsPolicy
{
  public:
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *ins);
    bool adjustInputs(TempAllocator &alloc, MInstruction *ins) {
        return staticAdjustInputs(alloc, ins);
    }
};

// Single-object input. If the input is a Value, it is unboxed. If it is
// a primitive, we use ValueToNonNullObject.
class SingleObjectPolicy : public ObjectPolicy<0>
{ };

template <unsigned Op>
class BoxPolicy : public BoxInputsPolicy
{
  public:
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
    bool adjustInputs(TempAllocator &alloc, MInstruction *def);
};

// First operand will be boxed to a Value (except for an object)
// Second operand (if specified) will forcefully be unboxed to an object
class InstanceOfPolicy : public TypePolicy
{
  public:
    bool adjustInputs(TempAllocator &alloc, MInstruction *def);
};

class StoreTypedArrayPolicy : public BoxInputsPolicy
{
  protected:
    bool adjustValueInput(TempAllocator &alloc, MInstruction *ins, int arrayType, MDefinition *value, int valueOperand);

  public:
    bool adjustInputs(TempAllocator &alloc, MInstruction *ins);
};

class StoreTypedArrayHolePolicy : public StoreTypedArrayPolicy
{
  public:
    bool adjustInputs(TempAllocator &alloc, MInstruction *ins);
};

class StoreTypedArrayElementStaticPolicy : public StoreTypedArrayPolicy
{
  public:
    bool adjustInputs(TempAllocator &alloc, MInstruction *ins);
};

// Accepts integers and doubles. Everything else is boxed.
class ClampPolicy : public BoxInputsPolicy
{
  public:
    bool adjustInputs(TempAllocator &alloc, MInstruction *ins);
};

class FilterTypeSetPolicy : public BoxInputsPolicy
{
  public:
    bool adjustInputs(TempAllocator &alloc, MInstruction *ins);
};

static inline bool
CoercesToDouble(MIRType type)
{
    if (type == MIRType_Undefined || IsFloatingPointType(type))
        return true;
    return false;
}


} // namespace jit
} // namespace js

#endif /* jit_TypePolicy_h */

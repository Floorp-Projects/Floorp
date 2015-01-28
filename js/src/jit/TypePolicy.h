/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_TypePolicy_h
#define jit_TypePolicy_h

#include "jit/IonTypes.h"
#include "jit/JitAllocPolicy.h"

namespace js {
namespace jit {

class MInstruction;
class MDefinition;

extern MDefinition *
AlwaysBoxAt(TempAllocator &alloc, MInstruction *at, MDefinition *operand);

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

class NoTypePolicy
{
  public:
    struct Data
    {
        static TypePolicy *thisTypePolicy() {
            return nullptr;
        }
    };
};

class BoxInputsPolicy MOZ_FINAL : public TypePolicy
{
  public:
    SPECIALIZATION_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *def) MOZ_OVERRIDE {
        return staticAdjustInputs(alloc, def);
    }
};

class ArithPolicy MOZ_FINAL : public TypePolicy
{
  public:
    SPECIALIZATION_DATA_;
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *def) MOZ_OVERRIDE;
};

class AllDoublePolicy MOZ_FINAL : public TypePolicy
{
  public:
    EMPTY_DATA_;
    bool adjustInputs(TempAllocator &alloc, MInstruction *def);
};

class BitwisePolicy MOZ_FINAL : public TypePolicy
{
  public:
    SPECIALIZATION_DATA_;
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *def) MOZ_OVERRIDE;
};

class ComparePolicy MOZ_FINAL : public TypePolicy
{
  public:
    EMPTY_DATA_;
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *def) MOZ_OVERRIDE;
};

// Policy for MTest instructions.
class TestPolicy MOZ_FINAL : public TypePolicy
{
  public:
    EMPTY_DATA_;
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *ins) MOZ_OVERRIDE;
};

class TypeBarrierPolicy MOZ_FINAL : public TypePolicy
{
  public:
    EMPTY_DATA_;
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *ins) MOZ_OVERRIDE;
};

class CallPolicy MOZ_FINAL : public TypePolicy
{
  public:
    EMPTY_DATA_;
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *def) MOZ_OVERRIDE;
};

// Policy for MPow. First operand Double; second Double or Int32.
class PowPolicy MOZ_FINAL : public TypePolicy
{
  public:
    SPECIALIZATION_DATA_;
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *ins) MOZ_OVERRIDE;
};

// Expect a string for operand Op. If the input is a Value, it is unboxed.
template <unsigned Op>
class StringPolicy MOZ_FINAL : public TypePolicy
{
  public:
    EMPTY_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *def) MOZ_OVERRIDE {
        return staticAdjustInputs(alloc, def);
    }
};

// Expect a string for operand Op. Else a ToString instruction is inserted.
template <unsigned Op>
class ConvertToStringPolicy MOZ_FINAL : public TypePolicy
{
  public:
    EMPTY_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *def) MOZ_OVERRIDE {
        return staticAdjustInputs(alloc, def);
    }
};

// Expect an Int for operand Op. If the input is a Value, it is unboxed.
template <unsigned Op>
class IntPolicy MOZ_FINAL : private TypePolicy
{
  public:
    EMPTY_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *def) MOZ_OVERRIDE {
        return staticAdjustInputs(alloc, def);
    }
};

// Expect an Int for operand Op. Else a ToInt32 instruction is inserted.
template <unsigned Op>
class ConvertToInt32Policy MOZ_FINAL : public TypePolicy
{
  public:
    EMPTY_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *def) MOZ_OVERRIDE {
        return staticAdjustInputs(alloc, def);
    }
};

// Expect a double for operand Op. If the input is a Value, it is unboxed.
template <unsigned Op>
class DoublePolicy MOZ_FINAL : public TypePolicy
{
  public:
    EMPTY_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *def) MOZ_OVERRIDE {
        return staticAdjustInputs(alloc, def);
    }
};

// Expect a float32 for operand Op. If the input is a Value, it is unboxed.
template <unsigned Op>
class Float32Policy MOZ_FINAL : public TypePolicy
{
  public:
    EMPTY_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *def) MOZ_OVERRIDE {
        return staticAdjustInputs(alloc, def);
    }
};

// Expect a float32 OR a double for operand Op, but will prioritize Float32
// if the result type is set as such. If the input is a Value, it is unboxed.
template <unsigned Op>
class FloatingPointPolicy MOZ_FINAL : public TypePolicy
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

    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *def) MOZ_OVERRIDE;
};

template <unsigned Op>
class NoFloatPolicy MOZ_FINAL : public TypePolicy
{
  public:
    EMPTY_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *def) MOZ_OVERRIDE {
        return staticAdjustInputs(alloc, def);
    }
};

// Policy for guarding variadic instructions such as object / array state
// instructions.
template <unsigned FirstOp>
class NoFloatPolicyAfter MOZ_FINAL : public TypePolicy
{
  public:
    EMPTY_DATA_;
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *ins) MOZ_OVERRIDE;
};

// Box objects or strings as an input to a ToDouble instruction.
class ToDoublePolicy MOZ_FINAL : public TypePolicy
{
  public:
    EMPTY_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *def) MOZ_OVERRIDE {
        return staticAdjustInputs(alloc, def);
    }
};

// Box objects, strings and undefined as input to a ToInt32 instruction.
class ToInt32Policy MOZ_FINAL : public TypePolicy
{
  public:
    EMPTY_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *def) MOZ_OVERRIDE {
        return staticAdjustInputs(alloc, def);
    }
};

// Box objects as input to a ToString instruction.
class ToStringPolicy MOZ_FINAL : public TypePolicy
{
  public:
    EMPTY_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *def) MOZ_OVERRIDE {
        return staticAdjustInputs(alloc, def);
    }
};

template <unsigned Op>
class ObjectPolicy MOZ_FINAL : public TypePolicy
{
  public:
    EMPTY_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *ins);
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *ins) MOZ_OVERRIDE {
        return staticAdjustInputs(alloc, ins);
    }
};

// Single-object input. If the input is a Value, it is unboxed. If it is
// a primitive, we use ValueToNonNullObject.
typedef ObjectPolicy<0> SingleObjectPolicy;

// Convert an operand to have a type identical to the scalar type of the
// returned type of the instruction.
template <unsigned Op>
class SimdScalarPolicy MOZ_FINAL : public TypePolicy
{
  public:
    EMPTY_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *def) MOZ_OVERRIDE {
        return staticAdjustInputs(alloc, def);
    }
};

// SIMD value-type policy, use the returned type of the instruction to determine
// how to unbox its operand.
template <unsigned Op>
class SimdSameAsReturnedTypePolicy MOZ_FINAL : public TypePolicy
{
  public:
    EMPTY_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *ins);
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *ins) MOZ_OVERRIDE {
        return staticAdjustInputs(alloc, ins);
    }
};

template <unsigned Op>
class BoxPolicy MOZ_FINAL : public TypePolicy
{
  public:
    EMPTY_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *ins);
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *ins) MOZ_OVERRIDE {
        return staticAdjustInputs(alloc, ins);
    }
};

// Boxes everything except inputs of type Type.
template <unsigned Op, MIRType Type>
class BoxExceptPolicy MOZ_FINAL : public TypePolicy
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
class MixPolicy MOZ_FINAL : public TypePolicy
{
  public:
    EMPTY_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *ins) {
        return Lhs::staticAdjustInputs(alloc, ins) && Rhs::staticAdjustInputs(alloc, ins);
    }
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *ins) MOZ_OVERRIDE {
        return staticAdjustInputs(alloc, ins);
    }
};

// Combine three policies.
template <class Policy1, class Policy2, class Policy3>
class Mix3Policy MOZ_FINAL : public TypePolicy
{
  public:
    EMPTY_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *ins) {
        return Policy1::staticAdjustInputs(alloc, ins) &&
               Policy2::staticAdjustInputs(alloc, ins) &&
               Policy3::staticAdjustInputs(alloc, ins);
    }
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *ins) MOZ_OVERRIDE {
        return staticAdjustInputs(alloc, ins);
    }
};

// Combine four policies.  (Missing variadic templates yet?)
template <class Policy1, class Policy2, class Policy3, class Policy4>
class Mix4Policy : public TypePolicy
{
  public:
    EMPTY_DATA_;
    static bool staticAdjustInputs(TempAllocator &alloc, MInstruction *ins) {
        return Policy1::staticAdjustInputs(alloc, ins) &&
               Policy2::staticAdjustInputs(alloc, ins) &&
               Policy3::staticAdjustInputs(alloc, ins) &&
               Policy4::staticAdjustInputs(alloc, ins);
    }
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *ins) MOZ_OVERRIDE {
        return staticAdjustInputs(alloc, ins);
    }
};

class CallSetElementPolicy MOZ_FINAL : public TypePolicy
{
  public:
    EMPTY_DATA_;
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *def) MOZ_OVERRIDE;
};

// First operand will be boxed to a Value (except for an object)
// Second operand (if specified) will forcefully be unboxed to an object
class InstanceOfPolicy MOZ_FINAL : public TypePolicy
{
  public:
    EMPTY_DATA_;
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *def) MOZ_OVERRIDE;
};

class StoreTypedArrayHolePolicy;
class StoreTypedArrayElementStaticPolicy;

class StoreTypedArrayPolicy : public TypePolicy
{
  private:
    static bool adjustValueInput(TempAllocator &alloc, MInstruction *ins, int arrayType, MDefinition *value, int valueOperand);

    friend class StoreTypedArrayHolePolicy;
    friend class StoreTypedArrayElementStaticPolicy;

  public:
    EMPTY_DATA_;
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *ins) MOZ_OVERRIDE;
};

class StoreTypedArrayHolePolicy MOZ_FINAL : public StoreTypedArrayPolicy
{
  public:
    EMPTY_DATA_;
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *ins) MOZ_OVERRIDE;
};

class StoreTypedArrayElementStaticPolicy MOZ_FINAL : public StoreTypedArrayPolicy
{
  public:
    EMPTY_DATA_;
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *ins) MOZ_OVERRIDE;
};

class StoreUnboxedObjectOrNullPolicy MOZ_FINAL : public TypePolicy
{
  public:
    EMPTY_DATA_;
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *def) MOZ_OVERRIDE;
};

// Accepts integers and doubles. Everything else is boxed.
class ClampPolicy MOZ_FINAL : public TypePolicy
{
  public:
    EMPTY_DATA_;
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *ins) MOZ_OVERRIDE;
};

class FilterTypeSetPolicy MOZ_FINAL : public TypePolicy
{
  public:
    EMPTY_DATA_;
    virtual bool adjustInputs(TempAllocator &alloc, MInstruction *ins) MOZ_OVERRIDE;
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

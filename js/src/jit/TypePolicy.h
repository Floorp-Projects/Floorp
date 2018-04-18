/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_TypePolicy_h
#define jit_TypePolicy_h

#include "mozilla/TypeTraits.h"

#include "jit/IonTypes.h"
#include "jit/JitAllocPolicy.h"

namespace js {
namespace jit {

class MInstruction;
class MDefinition;

extern MDefinition*
AlwaysBoxAt(TempAllocator& alloc, MInstruction* at, MDefinition* operand);

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
    virtual MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* def) const = 0;
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
        static const TypePolicy* thisTypePolicy();      \
    }

#define INHERIT_DATA_(DATA_TYPE)                        \
    struct Data : public DATA_TYPE                      \
    {                                                   \
        static const TypePolicy* thisTypePolicy();      \
    }

#define SPECIALIZATION_DATA_ INHERIT_DATA_(TypeSpecializationData)

class NoTypePolicy
{
  public:
    struct Data
    {
        static const TypePolicy* thisTypePolicy() {
            return nullptr;
        }
    };
};

class BoxInputsPolicy final : public TypePolicy
{
  public:
    constexpr BoxInputsPolicy() { }
    SPECIALIZATION_DATA_;
    static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc, MInstruction* def);
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* def) const override {
        return staticAdjustInputs(alloc, def);
    }
};

class ArithPolicy final : public TypePolicy
{
  public:
    constexpr ArithPolicy() { }
    SPECIALIZATION_DATA_;
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* def) const override;
};

class AllDoublePolicy final : public TypePolicy
{
  public:
    constexpr AllDoublePolicy() { }
    EMPTY_DATA_;
    static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc, MInstruction* def);
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* def) const override {
        return staticAdjustInputs(alloc, def);
    }
};

class BitwisePolicy final : public TypePolicy
{
  public:
    constexpr BitwisePolicy() { }
    SPECIALIZATION_DATA_;
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* def) const override;
};

class ComparePolicy final : public TypePolicy
{
  public:
    constexpr ComparePolicy() { }
    EMPTY_DATA_;
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* def) const override;
};

class SameValuePolicy final : public TypePolicy
{
  public:
    constexpr SameValuePolicy() { }
    EMPTY_DATA_;
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* def) const override;
};

// Policy for MTest instructions.
class TestPolicy final : public TypePolicy
{
  public:
    constexpr TestPolicy() { }
    EMPTY_DATA_;
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* ins) const override;
};

class TypeBarrierPolicy final : public TypePolicy
{
  public:
    constexpr TypeBarrierPolicy() { }
    EMPTY_DATA_;
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* ins) const override;
};

class CallPolicy final : public TypePolicy
{
  public:
    constexpr CallPolicy() { }
    EMPTY_DATA_;
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* def) const override;
};

// Policy for MPow. First operand Double; second Double or Int32.
class PowPolicy final : public TypePolicy
{
  public:
    constexpr PowPolicy() { }
    SPECIALIZATION_DATA_;
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* ins) const override;
};

// Expect a string for operand Op. If the input is a Value, it is unboxed.
template <unsigned Op>
class StringPolicy final : public TypePolicy
{
  public:
    constexpr StringPolicy() { }
    EMPTY_DATA_;
    static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc, MInstruction* def);
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* def) const override {
        return staticAdjustInputs(alloc, def);
    }
};

// Expect a string for operand Op. Else a ToString instruction is inserted.
template <unsigned Op>
class ConvertToStringPolicy final : public TypePolicy
{
  public:
    constexpr ConvertToStringPolicy() { }
    EMPTY_DATA_;
    static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc, MInstruction* def);
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* def) const override {
        return staticAdjustInputs(alloc, def);
    }
};

// Expect an Boolean for operand Op. If the input is a Value, it is unboxed.
template <unsigned Op>
class BooleanPolicy final : private TypePolicy
{
  public:
    constexpr BooleanPolicy() { }
    EMPTY_DATA_;
    static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc, MInstruction* def);
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* def) const override {
        return staticAdjustInputs(alloc, def);
    }
};

// Expects either an Int32 or a boxed Int32 for operand Op; may unbox if needed.
template <unsigned Op>
class UnboxedInt32Policy final : private TypePolicy
{
  public:
    constexpr UnboxedInt32Policy() { }
    EMPTY_DATA_;
    static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc, MInstruction* def);
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* def) const override {
        return staticAdjustInputs(alloc, def);
    }
};

// Expect an Int for operand Op. Else a ToInt32 instruction is inserted.
template <unsigned Op>
class ConvertToInt32Policy final : public TypePolicy
{
  public:
    constexpr ConvertToInt32Policy() { }
    EMPTY_DATA_;
    static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc, MInstruction* def);
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* def) const override {
        return staticAdjustInputs(alloc, def);
    }
};

// Expect an Int for operand Op. Else a TruncateToInt32 instruction is inserted.
template <unsigned Op>
class TruncateToInt32Policy final : public TypePolicy
{
  public:
    constexpr TruncateToInt32Policy() { }
    EMPTY_DATA_;
    static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc, MInstruction* def);
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* def) const override {
        return staticAdjustInputs(alloc, def);
    }
};

// Expect a double for operand Op. If the input is a Value, it is unboxed.
template <unsigned Op>
class DoublePolicy final : public TypePolicy
{
  public:
    constexpr DoublePolicy() { }
    EMPTY_DATA_;
    static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc, MInstruction* def);
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* def) const override {
        return staticAdjustInputs(alloc, def);
    }
};

// Expect a float32 for operand Op. If the input is a Value, it is unboxed.
template <unsigned Op>
class Float32Policy final : public TypePolicy
{
  public:
    constexpr Float32Policy() { }
    EMPTY_DATA_;
    static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc, MInstruction* def);
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* def) const override {
        return staticAdjustInputs(alloc, def);
    }
};

// Expect a float32 OR a double for operand Op, but will prioritize Float32
// if the result type is set as such. If the input is a Value, it is unboxed.
template <unsigned Op>
class FloatingPointPolicy final : public TypePolicy
{
  public:
    constexpr FloatingPointPolicy() { }
    SPECIALIZATION_DATA_;
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* def) const override;
};

template <unsigned Op>
class NoFloatPolicy final : public TypePolicy
{
  public:
    constexpr NoFloatPolicy() { }
    EMPTY_DATA_;
    static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc, MInstruction* def);
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* def) const override {
        return staticAdjustInputs(alloc, def);
    }
};

// Policy for guarding variadic instructions such as object / array state
// instructions.
template <unsigned FirstOp>
class NoFloatPolicyAfter final : public TypePolicy
{
  public:
    constexpr NoFloatPolicyAfter() { }
    EMPTY_DATA_;
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* ins) const override;
};

// Box objects or strings as an input to a ToDouble instruction.
class ToDoublePolicy final : public TypePolicy
{
  public:
    constexpr ToDoublePolicy() { }
    EMPTY_DATA_;
    static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc, MInstruction* def);
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* def) const override {
        return staticAdjustInputs(alloc, def);
    }
};

// Box objects, strings and undefined as input to a ToInt32 instruction.
class ToInt32Policy final : public TypePolicy
{
  public:
    constexpr ToInt32Policy() { }
    EMPTY_DATA_;
    static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc, MInstruction* def);
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* def) const override {
        return staticAdjustInputs(alloc, def);
    }
};

// Box objects as input to a ToString instruction.
class ToStringPolicy final : public TypePolicy
{
  public:
    constexpr ToStringPolicy() { }
    EMPTY_DATA_;
    static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc, MInstruction* def);
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* def) const override {
        return staticAdjustInputs(alloc, def);
    }
};

template <unsigned Op>
class ObjectPolicy final : public TypePolicy
{
  public:
    constexpr ObjectPolicy() { }
    EMPTY_DATA_;
    static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc, MInstruction* ins);
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* ins) const override {
        return staticAdjustInputs(alloc, ins);
    }
};

// Single-object input. If the input is a Value, it is unboxed. If it is
// a primitive, we use ValueToNonNullObject.
typedef ObjectPolicy<0> SingleObjectPolicy;

// Convert an operand to have a type identical to the scalar type of the
// returned type of the instruction.
template <unsigned Op>
class SimdScalarPolicy final : public TypePolicy
{
  public:
    constexpr SimdScalarPolicy() { }
    EMPTY_DATA_;
    static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc, MInstruction* def);
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* def) const override {
        return staticAdjustInputs(alloc, def);
    }
};

class SimdAllPolicy final : public TypePolicy
{
  public:
    constexpr SimdAllPolicy () { }
    SPECIALIZATION_DATA_;
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* ins) const override;
};

template <unsigned Op>
class SimdPolicy final : public TypePolicy
{
  public:
    constexpr SimdPolicy() { }
    SPECIALIZATION_DATA_;
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* ins) const override;
};

class SimdSelectPolicy final : public TypePolicy
{
  public:
    constexpr SimdSelectPolicy() { }
    SPECIALIZATION_DATA_;
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* ins) const override;
};

class SimdShufflePolicy final : public TypePolicy
{
  public:
    constexpr SimdShufflePolicy() { }
    SPECIALIZATION_DATA_;
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* ins) const override;
};

// SIMD value-type policy, use the returned type of the instruction to determine
// how to unbox its operand.
template <unsigned Op>
class SimdSameAsReturnedTypePolicy final : public TypePolicy
{
  public:
    constexpr SimdSameAsReturnedTypePolicy() { }
    EMPTY_DATA_;
    static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc, MInstruction* ins);
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* ins) const override {
        return staticAdjustInputs(alloc, ins);
    }
};

template <unsigned Op>
class BoxPolicy final : public TypePolicy
{
  public:
    constexpr BoxPolicy() { }
    EMPTY_DATA_;
    static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc, MInstruction* ins);
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* ins) const override {
        return staticAdjustInputs(alloc, ins);
    }
};

// Boxes everything except inputs of type Type.
template <unsigned Op, MIRType Type>
class BoxExceptPolicy final : public TypePolicy
{
  public:
    constexpr BoxExceptPolicy() { }
    EMPTY_DATA_;
    static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc, MInstruction* ins);
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* ins) const override {
        return staticAdjustInputs(alloc, ins);
    }
};

// Box if not a typical property id (string, symbol, int32).
template <unsigned Op>
class CacheIdPolicy final : public TypePolicy
{
  public:
    EMPTY_DATA_;
    static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc, MInstruction* ins);
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* ins) const override {
        return staticAdjustInputs(alloc, ins);
    }
};

// Combine multiple policies.
template <class... Policies>
class MixPolicy final : public TypePolicy
{
    template <class P>
    static bool staticAdjustInputsHelper(TempAllocator& alloc, MInstruction* ins) {
        return P::staticAdjustInputs(alloc, ins);
    }

    template <class P, class... Rest>
    static typename mozilla::EnableIf<(sizeof...(Rest) > 0), bool>::Type
    staticAdjustInputsHelper(TempAllocator& alloc, MInstruction* ins) {
        return P::staticAdjustInputs(alloc, ins) &&
               MixPolicy::staticAdjustInputsHelper<Rest...>(alloc, ins);
    }

  public:
    constexpr MixPolicy() { }
    EMPTY_DATA_;
    static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc, MInstruction* ins) {
        return MixPolicy::staticAdjustInputsHelper<Policies...>(alloc, ins);
    }
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* ins) const override {
        return staticAdjustInputs(alloc, ins);
    }
};

class CallSetElementPolicy final : public TypePolicy
{
  public:
    constexpr CallSetElementPolicy() { }
    EMPTY_DATA_;
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* def) const override;
};

// First operand will be boxed to a Value (except for an object)
// Second operand (if specified) will forcefully be unboxed to an object
class InstanceOfPolicy final : public TypePolicy
{
  public:
    constexpr InstanceOfPolicy() { }
    EMPTY_DATA_;
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* def) const override;
};

class StoreTypedArrayHolePolicy;

class StoreUnboxedScalarPolicy : public TypePolicy
{
  private:
    constexpr StoreUnboxedScalarPolicy() { }
    static MOZ_MUST_USE bool adjustValueInput(TempAllocator& alloc, MInstruction* ins,
                                              Scalar::Type arrayType, MDefinition* value,
                                              int valueOperand);

    friend class StoreTypedArrayHolePolicy;

  public:
    EMPTY_DATA_;
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* ins) const override;
};

class StoreTypedArrayHolePolicy final : public StoreUnboxedScalarPolicy
{
  public:
    constexpr StoreTypedArrayHolePolicy() { }
    EMPTY_DATA_;
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* ins) const override;
};

class StoreUnboxedObjectOrNullPolicy final : public TypePolicy
{
  public:
    constexpr StoreUnboxedObjectOrNullPolicy() { }
    EMPTY_DATA_;
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* def) const override;
};

class StoreUnboxedStringPolicy final : public TypePolicy
{
  public:
    constexpr StoreUnboxedStringPolicy() { }
    EMPTY_DATA_;
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* def) const override;
};

// Accepts integers and doubles. Everything else is boxed.
class ClampPolicy final : public TypePolicy
{
  public:
    constexpr ClampPolicy() { }
    EMPTY_DATA_;
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* ins) const override;
};

class FilterTypeSetPolicy final : public TypePolicy
{
  public:
    constexpr FilterTypeSetPolicy() { }
    EMPTY_DATA_;
    MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc, MInstruction* ins) const override;
};

#undef SPECIALIZATION_DATA_
#undef INHERIT_DATA_
#undef EMPTY_DATA_

} // namespace jit
} // namespace js

#endif /* jit_TypePolicy_h */

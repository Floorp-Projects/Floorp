/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
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

extern MDefinition* AlwaysBoxAt(TempAllocator& alloc, MInstruction* at,
                                MDefinition* operand);

// A type policy directs the type analysis phases, which insert conversion,
// boxing, unboxing, and type changes as necessary.
class TypePolicy {
 public:
  // Analyze the inputs of the instruction and perform one of the following
  // actions for each input:
  //  * Nothing; the input already type-checks.
  //  * If untyped, optionally ask the input to try and specialize its value.
  //  * Replace the operand with a conversion instruction.
  //  * Insert an unconditional deoptimization (no conversion possible).
  virtual MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                         MInstruction* def) const = 0;
};

struct TypeSpecializationData {
 protected:
  // Specifies three levels of specialization:
  //  - < Value. This input is expected and required.
  //  - == None. This op should not be specialized.
  MIRType specialization_;

  MIRType thisTypeSpecialization() { return specialization_; }

 public:
  MIRType specialization() const { return specialization_; }
};

#define EMPTY_DATA_                            \
  struct Data {                                \
    static const TypePolicy* thisTypePolicy(); \
  }

#define INHERIT_DATA_(DATA_TYPE)               \
  struct Data : public DATA_TYPE {             \
    static const TypePolicy* thisTypePolicy(); \
  }

#define SPECIALIZATION_DATA_ INHERIT_DATA_(TypeSpecializationData)

class NoTypePolicy {
 public:
  struct Data {
    static const TypePolicy* thisTypePolicy() { return nullptr; }
  };
};

class BoxInputsPolicy final : public TypePolicy {
 public:
  constexpr BoxInputsPolicy() = default;
  EMPTY_DATA_;
  static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc,
                                              MInstruction* def);
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* def) const override {
    return staticAdjustInputs(alloc, def);
  }
};

class ArithPolicy final : public TypePolicy {
 public:
  constexpr ArithPolicy() = default;
  EMPTY_DATA_;
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* def) const override;
};

class AllDoublePolicy final : public TypePolicy {
 public:
  constexpr AllDoublePolicy() = default;
  EMPTY_DATA_;
  static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc,
                                              MInstruction* def);
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* def) const override {
    return staticAdjustInputs(alloc, def);
  }
};

class BitwisePolicy final : public TypePolicy {
 public:
  constexpr BitwisePolicy() = default;
  SPECIALIZATION_DATA_;
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* def) const override;
};

class ComparePolicy final : public TypePolicy {
 public:
  constexpr ComparePolicy() = default;
  EMPTY_DATA_;
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* def) const override;
};

class SameValuePolicy final : public TypePolicy {
 public:
  constexpr SameValuePolicy() = default;
  EMPTY_DATA_;
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* def) const override;
};

// Policy for MTest instructions.
class TestPolicy final : public TypePolicy {
 public:
  constexpr TestPolicy() = default;
  EMPTY_DATA_;
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* ins) const override;
};

class TypeBarrierPolicy final : public TypePolicy {
 public:
  constexpr TypeBarrierPolicy() = default;
  EMPTY_DATA_;
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* ins) const override;
};

class CallPolicy final : public TypePolicy {
 public:
  constexpr CallPolicy() = default;
  EMPTY_DATA_;
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* def) const override;
};

// Policy for MPow. First operand Double; second Double or Int32.
class PowPolicy final : public TypePolicy {
 public:
  constexpr PowPolicy() = default;
  SPECIALIZATION_DATA_;
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* ins) const override;
};

// Policy for MSign. Operand is either Double or Int32.
class SignPolicy final : public TypePolicy {
 public:
  constexpr SignPolicy() = default;
  SPECIALIZATION_DATA_;
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* ins) const override;
};

// Expect a string for operand Op. If the input is a Value, it is unboxed.
template <unsigned Op>
class StringPolicy final : public TypePolicy {
 public:
  constexpr StringPolicy() = default;
  EMPTY_DATA_;
  static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc,
                                              MInstruction* def);
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* def) const override {
    return staticAdjustInputs(alloc, def);
  }
};

// Expect a string for operand Op. Else a ToString instruction is inserted.
template <unsigned Op>
class ConvertToStringPolicy final : public TypePolicy {
 public:
  constexpr ConvertToStringPolicy() = default;
  EMPTY_DATA_;
  static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc,
                                              MInstruction* def);
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* def) const override {
    return staticAdjustInputs(alloc, def);
  }
};

// Expect an Boolean for operand Op. If the input is a Value, it is unboxed.
template <unsigned Op>
class BooleanPolicy final : private TypePolicy {
 public:
  constexpr BooleanPolicy() = default;
  EMPTY_DATA_;
  static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc,
                                              MInstruction* def);
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* def) const override {
    return staticAdjustInputs(alloc, def);
  }
};

// Expects either an Int32 or a boxed Int32 for operand Op; may unbox if needed.
template <unsigned Op>
class UnboxedInt32Policy final : private TypePolicy {
 public:
  constexpr UnboxedInt32Policy() = default;
  EMPTY_DATA_;
  static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc,
                                              MInstruction* def);
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* def) const override {
    return staticAdjustInputs(alloc, def);
  }
};

// Expect an Int for operand Op. Else a ToInt32 instruction is inserted.
template <unsigned Op>
class ConvertToInt32Policy final : public TypePolicy {
 public:
  constexpr ConvertToInt32Policy() = default;
  EMPTY_DATA_;
  static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc,
                                              MInstruction* def);
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* def) const override {
    return staticAdjustInputs(alloc, def);
  }
};

// Expect an Int for operand Op. Else a TruncateToInt32 instruction is inserted.
template <unsigned Op>
class TruncateToInt32Policy final : public TypePolicy {
 public:
  constexpr TruncateToInt32Policy() = default;
  EMPTY_DATA_;
  static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc,
                                              MInstruction* def);
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* def) const override {
    return staticAdjustInputs(alloc, def);
  }
};

// Expect a double for operand Op. If the input is a Value, it is unboxed.
template <unsigned Op>
class DoublePolicy final : public TypePolicy {
 public:
  constexpr DoublePolicy() = default;
  EMPTY_DATA_;
  static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc,
                                              MInstruction* def);
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* def) const override {
    return staticAdjustInputs(alloc, def);
  }
};

// Expect a float32 for operand Op. If the input is a Value, it is unboxed.
template <unsigned Op>
class Float32Policy final : public TypePolicy {
 public:
  constexpr Float32Policy() = default;
  EMPTY_DATA_;
  static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc,
                                              MInstruction* def);
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* def) const override {
    return staticAdjustInputs(alloc, def);
  }
};

// Expect a float32 OR a double for operand Op, but will prioritize Float32
// if the result type is set as such. If the input is a Value, it is unboxed.
template <unsigned Op>
class FloatingPointPolicy final : public TypePolicy {
 public:
  constexpr FloatingPointPolicy() = default;
  SPECIALIZATION_DATA_;
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* def) const override;
};

template <unsigned Op>
class NoFloatPolicy final : public TypePolicy {
 public:
  constexpr NoFloatPolicy() = default;
  EMPTY_DATA_;
  static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc,
                                              MInstruction* def);
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* def) const override {
    return staticAdjustInputs(alloc, def);
  }
};

// Policy for guarding variadic instructions such as object / array state
// instructions.
template <unsigned FirstOp>
class NoFloatPolicyAfter final : public TypePolicy {
 public:
  constexpr NoFloatPolicyAfter() = default;
  EMPTY_DATA_;
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* ins) const override;
};

// Box objects or strings as an input to a ToDouble instruction.
class ToDoublePolicy final : public TypePolicy {
 public:
  constexpr ToDoublePolicy() = default;
  EMPTY_DATA_;
  static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc,
                                              MInstruction* def);
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* def) const override {
    return staticAdjustInputs(alloc, def);
  }
};

// Box objects, strings and undefined as input to a ToInt32 instruction.
class ToInt32Policy final : public TypePolicy {
 public:
  constexpr ToInt32Policy() = default;
  EMPTY_DATA_;
  static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc,
                                              MInstruction* def);
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* def) const override {
    return staticAdjustInputs(alloc, def);
  }
};

// Box objects as input to a ToString instruction.
class ToStringPolicy final : public TypePolicy {
 public:
  constexpr ToStringPolicy() = default;
  EMPTY_DATA_;
  static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc,
                                              MInstruction* def);
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* def) const override {
    return staticAdjustInputs(alloc, def);
  }
};

template <unsigned Op>
class ObjectPolicy final : public TypePolicy {
 public:
  constexpr ObjectPolicy() = default;
  EMPTY_DATA_;
  static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc,
                                              MInstruction* ins);
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* ins) const override {
    return staticAdjustInputs(alloc, ins);
  }
};

// Single-object input. If the input is a Value, it is unboxed. If it is
// a primitive, we use ValueToNonNullObject.
using SingleObjectPolicy = ObjectPolicy<0>;

template <unsigned Op>
class BoxPolicy final : public TypePolicy {
 public:
  constexpr BoxPolicy() = default;
  EMPTY_DATA_;
  static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc,
                                              MInstruction* ins);
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* ins) const override {
    return staticAdjustInputs(alloc, ins);
  }
};

// Boxes everything except inputs of type Type.
template <unsigned Op, MIRType Type>
class BoxExceptPolicy final : public TypePolicy {
 public:
  constexpr BoxExceptPolicy() = default;
  EMPTY_DATA_;
  static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc,
                                              MInstruction* ins);
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* ins) const override {
    return staticAdjustInputs(alloc, ins);
  }
};

// Box if not a typical property id (string, symbol, int32).
template <unsigned Op>
class CacheIdPolicy final : public TypePolicy {
 public:
  EMPTY_DATA_;
  static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc,
                                              MInstruction* ins);
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* ins) const override {
    return staticAdjustInputs(alloc, ins);
  }
};

// Combine multiple policies.
template <class... Policies>
class MixPolicy final : public TypePolicy {
 public:
  constexpr MixPolicy() = default;
  EMPTY_DATA_;
  static MOZ_MUST_USE bool staticAdjustInputs(TempAllocator& alloc,
                                              MInstruction* ins) {
    return (Policies::staticAdjustInputs(alloc, ins) && ...);
  }
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* ins) const override {
    return staticAdjustInputs(alloc, ins);
  }
};

class CallSetElementPolicy final : public TypePolicy {
 public:
  constexpr CallSetElementPolicy() = default;
  EMPTY_DATA_;
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* def) const override;
};

// First operand will be boxed to a Value (except for an object)
// Second operand (if specified) will forcefully be unboxed to an object
class InstanceOfPolicy final : public TypePolicy {
 public:
  constexpr InstanceOfPolicy() = default;
  EMPTY_DATA_;
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* def) const override;
};

class StoreTypedArrayHolePolicy;

class StoreUnboxedScalarPolicy : public TypePolicy {
 private:
  constexpr StoreUnboxedScalarPolicy() = default;
  static MOZ_MUST_USE bool adjustValueInput(TempAllocator& alloc,
                                            MInstruction* ins,
                                            Scalar::Type arrayType,
                                            MDefinition* value,
                                            int valueOperand);

  friend class StoreTypedArrayHolePolicy;

 public:
  EMPTY_DATA_;
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* ins) const override;
};

class StoreTypedArrayHolePolicy final : public StoreUnboxedScalarPolicy {
 public:
  constexpr StoreTypedArrayHolePolicy() = default;
  EMPTY_DATA_;
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* ins) const override;
};

// Accepts integers and doubles. Everything else is boxed.
class ClampPolicy final : public TypePolicy {
 public:
  constexpr ClampPolicy() = default;
  EMPTY_DATA_;
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* ins) const override;
};

class FilterTypeSetPolicy final : public TypePolicy {
 public:
  constexpr FilterTypeSetPolicy() = default;
  EMPTY_DATA_;
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* ins) const override;
};

// Policy for MTypedArrayIndexToInt32. Operand is either Double or Int32.
class TypedArrayIndexPolicy final : public TypePolicy {
 public:
  constexpr TypedArrayIndexPolicy() = default;
  SPECIALIZATION_DATA_;
  MOZ_MUST_USE bool adjustInputs(TempAllocator& alloc,
                                 MInstruction* def) const override;
};

#undef SPECIALIZATION_DATA_
#undef INHERIT_DATA_
#undef EMPTY_DATA_

}  // namespace jit
}  // namespace js

#endif /* jit_TypePolicy_h */

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_x64_LIR_x64_h
#define jit_x64_LIR_x64_h

namespace js {
namespace jit {

// Given an untyped input, guards on whether it's a specific type and returns
// the unboxed payload.
class LUnboxBase : public LInstructionHelper<1, 1, 0> {
 public:
  LUnboxBase(LNode::Opcode op, const LAllocation& input)
      : LInstructionHelper<1, 1, 0>(op) {
    setOperand(0, input);
  }

  static const size_t Input = 0;

  MUnbox* mir() const { return mir_->toUnbox(); }
};

class LUnbox : public LUnboxBase {
 public:
  LIR_HEADER(Unbox)

  explicit LUnbox(const LAllocation& input) : LUnboxBase(classOpcode, input) {}

  const char* extraName() const { return StringFromMIRType(mir()->type()); }
};

class LUnboxFloatingPoint : public LUnboxBase {
  MIRType type_;

 public:
  LIR_HEADER(UnboxFloatingPoint)

  LUnboxFloatingPoint(const LAllocation& input, MIRType type)
      : LUnboxBase(classOpcode, input), type_(type) {}

  MIRType type() const { return type_; }
  const char* extraName() const { return StringFromMIRType(type_); }
};

// Convert a 32-bit unsigned integer to a double.
class LWasmUint32ToDouble : public LInstructionHelper<1, 1, 0> {
 public:
  LIR_HEADER(WasmUint32ToDouble)

  explicit LWasmUint32ToDouble(const LAllocation& input)
      : LInstructionHelper(classOpcode) {
    setOperand(0, input);
  }
};

// Convert a 32-bit unsigned integer to a float32.
class LWasmUint32ToFloat32 : public LInstructionHelper<1, 1, 0> {
 public:
  LIR_HEADER(WasmUint32ToFloat32)

  explicit LWasmUint32ToFloat32(const LAllocation& input)
      : LInstructionHelper(classOpcode) {
    setOperand(0, input);
  }
};

class LDivOrModI64 : public LBinaryMath<1> {
 public:
  LIR_HEADER(DivOrModI64)

  LDivOrModI64(const LAllocation& lhs, const LAllocation& rhs,
               const LDefinition& temp)
      : LBinaryMath(classOpcode) {
    setOperand(0, lhs);
    setOperand(1, rhs);
    setTemp(0, temp);
  }

  const LDefinition* remainder() { return getTemp(0); }

  MBinaryArithInstruction* mir() const {
    MOZ_ASSERT(mir_->isDiv() || mir_->isMod());
    return static_cast<MBinaryArithInstruction*>(mir_);
  }
  bool canBeDivideByZero() const {
    if (mir_->isMod()) {
      return mir_->toMod()->canBeDivideByZero();
    }
    return mir_->toDiv()->canBeDivideByZero();
  }
  bool canBeNegativeOverflow() const {
    if (mir_->isMod()) {
      return mir_->toMod()->canBeNegativeDividend();
    }
    return mir_->toDiv()->canBeNegativeOverflow();
  }
  wasm::BytecodeOffset bytecodeOffset() const {
    MOZ_ASSERT(mir_->isDiv() || mir_->isMod());
    if (mir_->isMod()) {
      return mir_->toMod()->bytecodeOffset();
    }
    return mir_->toDiv()->bytecodeOffset();
  }
};

// This class performs a simple x86 'div', yielding either a quotient or
// remainder depending on whether this instruction is defined to output
// rax (quotient) or rdx (remainder).
class LUDivOrModI64 : public LBinaryMath<1> {
 public:
  LIR_HEADER(UDivOrModI64);

  LUDivOrModI64(const LAllocation& lhs, const LAllocation& rhs,
                const LDefinition& temp)
      : LBinaryMath(classOpcode) {
    setOperand(0, lhs);
    setOperand(1, rhs);
    setTemp(0, temp);
  }

  const LDefinition* remainder() { return getTemp(0); }

  const char* extraName() const {
    return mir()->isTruncated() ? "Truncated" : nullptr;
  }

  MBinaryArithInstruction* mir() const {
    MOZ_ASSERT(mir_->isDiv() || mir_->isMod());
    return static_cast<MBinaryArithInstruction*>(mir_);
  }

  bool canBeDivideByZero() const {
    if (mir_->isMod()) {
      return mir_->toMod()->canBeDivideByZero();
    }
    return mir_->toDiv()->canBeDivideByZero();
  }

  wasm::BytecodeOffset bytecodeOffset() const {
    MOZ_ASSERT(mir_->isDiv() || mir_->isMod());
    if (mir_->isMod()) {
      return mir_->toMod()->bytecodeOffset();
    }
    return mir_->toDiv()->bytecodeOffset();
  }
};

class LWasmTruncateToInt64 : public LInstructionHelper<1, 1, 1> {
 public:
  LIR_HEADER(WasmTruncateToInt64);

  LWasmTruncateToInt64(const LAllocation& in, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(0, in);
    setTemp(0, temp);
  }

  MWasmTruncateToInt64* mir() const { return mir_->toWasmTruncateToInt64(); }

  const LDefinition* temp() { return getTemp(0); }
};

// Wasm SIMD.
//
// These nodes are really x86-shared, but as some Masm APIs are not yet
// available on x86 we keep them here.

// Constant Simd128
class LSimd128 : public LInstructionHelper<1, 0, 0> {
  SimdConstant v_;

 public:
  LIR_HEADER(Simd128);

  explicit LSimd128(SimdConstant v) : LInstructionHelper(classOpcode), v_(v) {}

  const SimdConstant& getSimd128() const { return v_; }
};

// (v128, v128, v128) -> v128 effect-free operation.
// temp is FPR (and always in use).
class LWasmBitselectSimd128 : public LInstructionHelper<1, 3, 1> {
 public:
  LIR_HEADER(WasmBitselectSimd128)

  static constexpr uint32_t LhsDest = 0;
  static constexpr uint32_t Rhs = 1;
  static constexpr uint32_t Control = 2;

  LWasmBitselectSimd128(const LAllocation& lhs, const LAllocation& rhs,
                        const LAllocation& control, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(LhsDest, lhs);
    setOperand(Rhs, rhs);
    setOperand(Control, control);
    setTemp(0, temp);
  }

  const LAllocation* lhsDest() { return getOperand(LhsDest); }
  const LAllocation* rhs() { return getOperand(Rhs); }
  const LAllocation* control() { return getOperand(Control); }
  const LDefinition* temp() { return getTemp(0); }
};

// (v128, v128) -> v128 effect-free operations
// lhs and dest are the same.
// temps (if in use) are FPR.
class LWasmBinarySimd128 : public LInstructionHelper<1, 2, 2> {
 public:
  LIR_HEADER(WasmBinarySimd128)

  static constexpr uint32_t LhsDest = 0;
  static constexpr uint32_t Rhs = 1;

  LWasmBinarySimd128(const LAllocation& lhsDest, const LAllocation& rhs,
                     const LDefinition& temp0, const LDefinition& temp1)
      : LInstructionHelper(classOpcode) {
    setOperand(LhsDest, lhsDest);
    setOperand(Rhs, rhs);
    setTemp(0, temp0);
    setTemp(1, temp1);
  }

  const LAllocation* lhsDest() { return getOperand(LhsDest); }
  const LAllocation* rhs() { return getOperand(Rhs); }
  wasm::SimdOp simdOp() const { return mir_->toWasmBinarySimd128()->simdOp(); }
};

// (v128, v128) -> v128 effect-free operations for i64x2.mul
// lhs and dest are the same.
// one int64 temp.
class LWasmI64x2Mul : public LInstructionHelper<1, 2, INT64_PIECES> {
 public:
  LIR_HEADER(WasmI64x2Mul)

  static constexpr uint32_t LhsDest = 0;
  static constexpr uint32_t Rhs = 1;

  LWasmI64x2Mul(const LAllocation& lhsDest, const LAllocation& rhs,
                const LInt64Definition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(LhsDest, lhsDest);
    setOperand(Rhs, rhs);
    setInt64Temp(0, temp);
  }

  const LAllocation* lhsDest() { return getOperand(LhsDest); }
  const LAllocation* rhs() { return getOperand(Rhs); }
};

// (v128, i32) -> v128 effect-free variable-width shift operations
// lhs and dest are the same.
// temp0 is a GPR (if in use).
// temp1 is an FPR (if in use).
class LWasmVariableShiftSimd128 : public LInstructionHelper<1, 2, 2> {
 public:
  LIR_HEADER(WasmVariableShiftSimd128)

  static constexpr uint32_t LhsDest = 0;
  static constexpr uint32_t Rhs = 1;

  LWasmVariableShiftSimd128(const LAllocation& lhsDest, const LAllocation& rhs,
                            const LDefinition& temp0, const LDefinition& temp1)
      : LInstructionHelper(classOpcode) {
    setOperand(LhsDest, lhsDest);
    setOperand(Rhs, rhs);
    setTemp(0, temp0);
    setTemp(1, temp1);
  }

  const LAllocation* lhsDest() { return getOperand(LhsDest); }
  const LAllocation* rhs() { return getOperand(Rhs); }
  wasm::SimdOp simdOp() const { return mir_->toWasmShiftSimd128()->simdOp(); }
};

// (v128, v128, imm_simd) -> v128 effect-free operation.
// temp is FPR (and always in use).
class LWasmShuffleSimd128 : public LInstructionHelper<1, 2, 1> {
 public:
  LIR_HEADER(WasmShuffleSimd128)

  static constexpr uint32_t LhsDest = 0;
  static constexpr uint32_t Rhs = 1;

  LWasmShuffleSimd128(const LAllocation& lhsDest, const LAllocation& rhs,
                      const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(LhsDest, lhsDest);
    setOperand(Rhs, rhs);
    setTemp(0, temp);
  }

  const LAllocation* lhsDest() { return getOperand(LhsDest); }
  const LAllocation* rhs() { return getOperand(Rhs); }
  const SimdConstant control() {
    return mir_->toWasmShuffleSimd128()->control();
  }
  const LDefinition* temp() { return getTemp(0); }
};

class LWasmReplaceLaneSimd128 : public LInstructionHelper<1, 2, 0> {
 public:
  LIR_HEADER(WasmReplaceLaneSimd128)

  static constexpr uint32_t LhsDest = 0;
  static constexpr uint32_t Rhs = 1;

  LWasmReplaceLaneSimd128(const LAllocation& lhsDest, const LAllocation& rhs)
      : LInstructionHelper(classOpcode) {
    setOperand(LhsDest, lhsDest);
    setOperand(Rhs, rhs);
  }

  const LAllocation* lhsDest() { return getOperand(LhsDest); }
  const LAllocation* rhs() { return getOperand(Rhs); }
  uint32_t laneIndex() const {
    return mir_->toWasmReplaceLaneSimd128()->laneIndex();
  }
  wasm::SimdOp simdOp() const {
    return mir_->toWasmReplaceLaneSimd128()->simdOp();
  }
};

class LWasmReplaceInt64LaneSimd128
    : public LInstructionHelper<1, INT64_PIECES + 1, 0> {
 public:
  LIR_HEADER(WasmReplaceInt64LaneSimd128)

  static constexpr uint32_t LhsDest = 0;
  static constexpr uint32_t Rhs = 1;

  LWasmReplaceInt64LaneSimd128(const LAllocation& lhsDest,
                               const LInt64Allocation& rhs)
      : LInstructionHelper(classOpcode) {
    setOperand(LhsDest, lhsDest);
    setInt64Operand(Rhs, rhs);
  }

  const LAllocation* lhsDest() { return getOperand(LhsDest); }
  const LInt64Allocation rhs() { return getInt64Operand(Rhs); }
  uint32_t laneIndex() const {
    return mir_->toWasmReplaceLaneSimd128()->laneIndex();
  }
  wasm::SimdOp simdOp() const {
    return mir_->toWasmReplaceLaneSimd128()->simdOp();
  }
};

// (scalar) -> v128 effect-free operations, scalar != int64
class LWasmScalarToSimd128 : public LInstructionHelper<1, 1, 0> {
 public:
  LIR_HEADER(WasmScalarToSimd128)

  static constexpr uint32_t Src = 0;

  explicit LWasmScalarToSimd128(const LAllocation& src)
      : LInstructionHelper(classOpcode) {
    setOperand(Src, src);
  }

  const LAllocation* src() { return getOperand(Src); }
  wasm::SimdOp simdOp() const {
    return mir_->toWasmScalarToSimd128()->simdOp();
  }
};

// (int64) -> v128 effect-free operations
class LWasmInt64ToSimd128 : public LInstructionHelper<1, INT64_PIECES, 0> {
 public:
  LIR_HEADER(WasmInt64ToSimd128)

  static constexpr uint32_t Src = 0;

  explicit LWasmInt64ToSimd128(const LInt64Allocation& src)
      : LInstructionHelper(classOpcode) {
    setInt64Operand(Src, src);
  }

  const LInt64Allocation src() { return getInt64Operand(Src); }
  wasm::SimdOp simdOp() const {
    return mir_->toWasmScalarToSimd128()->simdOp();
  }
};

// (v128) -> v128 effect-free operations
// temp is FPR (if in use).
class LWasmUnarySimd128 : public LInstructionHelper<1, 1, 1> {
 public:
  LIR_HEADER(WasmUnarySimd128)

  static constexpr uint32_t Src = 0;

  LWasmUnarySimd128(const LAllocation& src, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setOperand(Src, src);
    setTemp(0, temp);
  }

  const LAllocation* src() { return getOperand(Src); }
  const LDefinition* temp() { return getTemp(0); }
  wasm::SimdOp simdOp() const { return mir_->toWasmUnarySimd128()->simdOp(); }
};

// (v128, imm) -> scalar effect-free operations.
class LWasmReduceSimd128 : public LInstructionHelper<1, 1, 0> {
 public:
  LIR_HEADER(WasmReduceSimd128)

  static constexpr uint32_t Src = 0;

  explicit LWasmReduceSimd128(const LAllocation& src)
      : LInstructionHelper(classOpcode) {
    setOperand(Src, src);
  }

  const LAllocation* src() { return getOperand(Src); }
  uint32_t imm() const { return mir_->toWasmReduceSimd128()->imm(); }
  wasm::SimdOp simdOp() const { return mir_->toWasmReduceSimd128()->simdOp(); }
};

// (v128, imm) -> i64 effect-free operations
class LWasmReduceSimd128ToInt64
    : public LInstructionHelper<INT64_PIECES, 1, 0> {
 public:
  LIR_HEADER(WasmReduceSimd128ToInt64)

  static constexpr uint32_t Src = 0;

  explicit LWasmReduceSimd128ToInt64(const LAllocation& src)
      : LInstructionHelper(classOpcode) {
    setOperand(Src, src);
  }

  const LAllocation* src() { return getOperand(Src); }
  uint32_t imm() const { return mir_->toWasmReduceSimd128()->imm(); }
  wasm::SimdOp simdOp() const { return mir_->toWasmReduceSimd128()->simdOp(); }
};

// End Wasm SIMD

}  // namespace jit
}  // namespace js

#endif /* jit_x64_LIR_x64_h */

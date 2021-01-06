/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_x86_shared_LIR_x86_shared_h
#define jit_x86_shared_LIR_x86_shared_h

namespace js {
namespace jit {

class LDivI : public LBinaryMath<1> {
 public:
  LIR_HEADER(DivI)

  LDivI(const LAllocation& lhs, const LAllocation& rhs, const LDefinition& temp)
      : LBinaryMath(classOpcode) {
    setOperand(0, lhs);
    setOperand(1, rhs);
    setTemp(0, temp);
  }

  const char* extraName() const {
    if (mir()->isTruncated()) {
      if (mir()->canBeNegativeZero()) {
        return mir()->canBeNegativeOverflow()
                   ? "Truncate_NegativeZero_NegativeOverflow"
                   : "Truncate_NegativeZero";
      }
      return mir()->canBeNegativeOverflow() ? "Truncate_NegativeOverflow"
                                            : "Truncate";
    }
    if (mir()->canBeNegativeZero()) {
      return mir()->canBeNegativeOverflow() ? "NegativeZero_NegativeOverflow"
                                            : "NegativeZero";
    }
    return mir()->canBeNegativeOverflow() ? "NegativeOverflow" : nullptr;
  }

  const LDefinition* remainder() { return getTemp(0); }
  MDiv* mir() const { return mir_->toDiv(); }
};

// Signed division by a power-of-two constant.
class LDivPowTwoI : public LBinaryMath<0> {
  const int32_t shift_;
  const bool negativeDivisor_;

 public:
  LIR_HEADER(DivPowTwoI)

  LDivPowTwoI(const LAllocation& lhs, const LAllocation& lhsCopy, int32_t shift,
              bool negativeDivisor)
      : LBinaryMath(classOpcode),
        shift_(shift),
        negativeDivisor_(negativeDivisor) {
    setOperand(0, lhs);
    setOperand(1, lhsCopy);
  }

  const LAllocation* numerator() { return getOperand(0); }
  const LAllocation* numeratorCopy() { return getOperand(1); }
  int32_t shift() const { return shift_; }
  bool negativeDivisor() const { return negativeDivisor_; }
  MDiv* mir() const { return mir_->toDiv(); }
};

class LDivOrModConstantI : public LInstructionHelper<1, 1, 1> {
  const int32_t denominator_;

 public:
  LIR_HEADER(DivOrModConstantI)

  LDivOrModConstantI(const LAllocation& lhs, int32_t denominator,
                     const LDefinition& temp)
      : LInstructionHelper(classOpcode), denominator_(denominator) {
    setOperand(0, lhs);
    setTemp(0, temp);
  }

  const LAllocation* numerator() { return getOperand(0); }
  int32_t denominator() const { return denominator_; }
  MBinaryArithInstruction* mir() const {
    MOZ_ASSERT(mir_->isDiv() || mir_->isMod());
    return static_cast<MBinaryArithInstruction*>(mir_);
  }
  bool canBeNegativeDividend() const {
    if (mir_->isMod()) {
      return mir_->toMod()->canBeNegativeDividend();
    }
    return mir_->toDiv()->canBeNegativeDividend();
  }
};

class LModI : public LBinaryMath<1> {
 public:
  LIR_HEADER(ModI)

  LModI(const LAllocation& lhs, const LAllocation& rhs, const LDefinition& temp)
      : LBinaryMath(classOpcode) {
    setOperand(0, lhs);
    setOperand(1, rhs);
    setTemp(0, temp);
  }

  const char* extraName() const {
    return mir()->isTruncated() ? "Truncated" : nullptr;
  }

  const LDefinition* remainder() { return getDef(0); }
  MMod* mir() const { return mir_->toMod(); }
};

// This class performs a simple x86 'div', yielding either a quotient or
// remainder depending on whether this instruction is defined to output eax
// (quotient) or edx (remainder).
class LUDivOrMod : public LBinaryMath<1> {
 public:
  LIR_HEADER(UDivOrMod);

  LUDivOrMod(const LAllocation& lhs, const LAllocation& rhs,
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

  bool trapOnError() const {
    if (mir_->isMod()) {
      return mir_->toMod()->trapOnError();
    }
    return mir_->toDiv()->trapOnError();
  }

  wasm::BytecodeOffset bytecodeOffset() const {
    if (mir_->isMod()) {
      return mir_->toMod()->bytecodeOffset();
    }
    return mir_->toDiv()->bytecodeOffset();
  }
};

class LUDivOrModConstant : public LInstructionHelper<1, 1, 1> {
  const uint32_t denominator_;

 public:
  LIR_HEADER(UDivOrModConstant)

  LUDivOrModConstant(const LAllocation& lhs, uint32_t denominator,
                     const LDefinition& temp)
      : LInstructionHelper(classOpcode), denominator_(denominator) {
    setOperand(0, lhs);
    setTemp(0, temp);
  }

  const LAllocation* numerator() { return getOperand(0); }
  uint32_t denominator() const { return denominator_; }
  MBinaryArithInstruction* mir() const {
    MOZ_ASSERT(mir_->isDiv() || mir_->isMod());
    return static_cast<MBinaryArithInstruction*>(mir_);
  }
  bool canBeNegativeDividend() const {
    if (mir_->isMod()) {
      return mir_->toMod()->canBeNegativeDividend();
    }
    return mir_->toDiv()->canBeNegativeDividend();
  }
  bool trapOnError() const {
    if (mir_->isMod()) {
      return mir_->toMod()->trapOnError();
    }
    return mir_->toDiv()->trapOnError();
  }
  wasm::BytecodeOffset bytecodeOffset() const {
    if (mir_->isMod()) {
      return mir_->toMod()->bytecodeOffset();
    }
    return mir_->toDiv()->bytecodeOffset();
  }
};

class LModPowTwoI : public LInstructionHelper<1, 1, 0> {
  const int32_t shift_;

 public:
  LIR_HEADER(ModPowTwoI)

  LModPowTwoI(const LAllocation& lhs, int32_t shift)
      : LInstructionHelper(classOpcode), shift_(shift) {
    setOperand(0, lhs);
  }

  int32_t shift() const { return shift_; }
  const LDefinition* remainder() { return getDef(0); }
  MMod* mir() const { return mir_->toMod(); }
};

// Takes a tableswitch with an integer to decide
class LTableSwitch : public LInstructionHelper<0, 1, 2> {
 public:
  LIR_HEADER(TableSwitch)

  LTableSwitch(const LAllocation& in, const LDefinition& inputCopy,
               const LDefinition& jumpTablePointer, MTableSwitch* ins)
      : LInstructionHelper(classOpcode) {
    setOperand(0, in);
    setTemp(0, inputCopy);
    setTemp(1, jumpTablePointer);
    setMir(ins);
  }

  MTableSwitch* mir() const { return mir_->toTableSwitch(); }

  const LAllocation* index() { return getOperand(0); }
  const LDefinition* tempInt() { return getTemp(0); }
  const LDefinition* tempPointer() { return getTemp(1); }
};

// Takes a tableswitch with a value to decide
class LTableSwitchV : public LInstructionHelper<0, BOX_PIECES, 3> {
 public:
  LIR_HEADER(TableSwitchV)

  LTableSwitchV(const LBoxAllocation& input, const LDefinition& inputCopy,
                const LDefinition& floatCopy,
                const LDefinition& jumpTablePointer, MTableSwitch* ins)
      : LInstructionHelper(classOpcode) {
    setBoxOperand(InputValue, input);
    setTemp(0, inputCopy);
    setTemp(1, floatCopy);
    setTemp(2, jumpTablePointer);
    setMir(ins);
  }

  MTableSwitch* mir() const { return mir_->toTableSwitch(); }

  static const size_t InputValue = 0;

  const LDefinition* tempInt() { return getTemp(0); }
  const LDefinition* tempFloat() { return getTemp(1); }
  const LDefinition* tempPointer() { return getTemp(2); }
};

class LMulI : public LBinaryMath<0, 1> {
 public:
  LIR_HEADER(MulI)

  LMulI(const LAllocation& lhs, const LAllocation& rhs,
        const LAllocation& lhsCopy)
      : LBinaryMath(classOpcode) {
    setOperand(0, lhs);
    setOperand(1, rhs);
    setOperand(2, lhsCopy);
  }

  const char* extraName() const {
    return (mir()->mode() == MMul::Integer)
               ? "Integer"
               : (mir()->canBeNegativeZero() ? "CanBeNegativeZero" : nullptr);
  }

  MMul* mir() const { return mir_->toMul(); }
  const LAllocation* lhsCopy() { return this->getOperand(2); }
};

class LInt64ToFloatingPoint : public LInstructionHelper<1, INT64_PIECES, 1> {
 public:
  LIR_HEADER(Int64ToFloatingPoint);

  LInt64ToFloatingPoint(const LInt64Allocation& in, const LDefinition& temp)
      : LInstructionHelper(classOpcode) {
    setInt64Operand(0, in);
    setTemp(0, temp);
  }

  MInt64ToFloatingPoint* mir() const { return mir_->toInt64ToFloatingPoint(); }

  const LDefinition* temp() { return getTemp(0); }
};

// Wasm SIMD.

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
// The op may differ from the MIR node's op.
class LWasmBinarySimd128 : public LInstructionHelper<1, 2, 2> {
  wasm::SimdOp op_;

 public:
  LIR_HEADER(WasmBinarySimd128)

  static constexpr uint32_t LhsDest = 0;
  static constexpr uint32_t Rhs = 1;

  LWasmBinarySimd128(wasm::SimdOp op, const LAllocation& lhsDest,
                     const LAllocation& rhs, const LDefinition& temp0,
                     const LDefinition& temp1)
      : LInstructionHelper(classOpcode), op_(op) {
    setOperand(LhsDest, lhsDest);
    setOperand(Rhs, rhs);
    setTemp(0, temp0);
    setTemp(1, temp1);
  }

  const LAllocation* lhsDest() { return getOperand(LhsDest); }
  const LAllocation* rhs() { return getOperand(Rhs); }
  wasm::SimdOp simdOp() const { return op_; }

  static bool SpecializeForConstantRhs(wasm::SimdOp op);
};

class LWasmBinarySimd128WithConstant : public LInstructionHelper<1, 1, 0> {
  SimdConstant rhs_;

 public:
  LIR_HEADER(WasmBinarySimd128WithConstant)

  static constexpr uint32_t LhsDest = 0;

  LWasmBinarySimd128WithConstant(const LAllocation& lhsDest,
                                 const SimdConstant& rhs)
      : LInstructionHelper(classOpcode), rhs_(rhs) {
    setOperand(LhsDest, lhsDest);
  }

  const LAllocation* lhsDest() { return getOperand(LhsDest); }
  const SimdConstant& rhs() { return rhs_; }
  wasm::SimdOp simdOp() const {
    return mir_->toWasmBinarySimd128WithConstant()->simdOp();
  }
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

// (v128, i32) -> v128 effect-free constant-width shift operations
class LWasmConstantShiftSimd128 : public LInstructionHelper<1, 1, 1> {
  int32_t shift_;

 public:
  LIR_HEADER(WasmConstantShiftSimd128)

  static constexpr uint32_t Src = 0;

  LWasmConstantShiftSimd128(const LAllocation& src, const LDefinition& temp,
                            int32_t shift)
      : LInstructionHelper(classOpcode), shift_(shift) {
    setOperand(Src, src);
    setTemp(0, temp);
  }

  const LAllocation* src() { return getOperand(Src); }
  const LDefinition* temp() { return getTemp(0); }
  int32_t shift() { return shift_; }
  wasm::SimdOp simdOp() const { return mir_->toWasmShiftSimd128()->simdOp(); }
};

// (v128, v128, imm_simd) -> v128 effect-free operation.
// temp is FPR (and always in use).
class LWasmShuffleSimd128 : public LInstructionHelper<1, 2, 1> {
 public:
  // Shuffle operations.
  enum Op {
    // Blend bytes.  control_ has the blend mask as an I8x16: 0 to select from
    // the lhs, -1 to select from the rhs.
    BLEND_8x16,

    // Blend words.  control_ has the blend mask as an I16x8: 0 to select from
    // the lhs, -1 to select from the rhs.
    BLEND_16x8,

    // Concat the lhs in front of the rhs and shift right by bytes, extracting
    // the low 16 bytes; control_[0] has the shift count.
    CONCAT_RIGHT_SHIFT_8x16,

    // Interleave dwords/qwords/words/bytes from high/low halves of operands.
    // The lhsDest gets the low-order item, then the rhs gets the next, then
    // the lhsDest the one after that, and so on.  control_ is garbage.
    INTERLEAVE_HIGH_8x16,
    INTERLEAVE_HIGH_16x8,
    INTERLEAVE_HIGH_32x4,
    INTERLEAVE_HIGH_64x2,
    INTERLEAVE_LOW_8x16,
    INTERLEAVE_LOW_16x8,
    INTERLEAVE_LOW_32x4,
    INTERLEAVE_LOW_64x2,

    // Fully general shuffle+blend.  control_ has the shuffle mask.
    SHUFFLE_BLEND_8x16,
  };

 private:
  Op op_;
  SimdConstant control_;

 public:
  LIR_HEADER(WasmShuffleSimd128)

  static constexpr uint32_t LhsDest = 0;
  static constexpr uint32_t Rhs = 1;

  LWasmShuffleSimd128(const LAllocation& lhsDest, const LAllocation& rhs,
                      const LDefinition& temp, Op op, SimdConstant control)
      : LInstructionHelper(classOpcode), op_(op), control_(control) {
    setOperand(LhsDest, lhsDest);
    setOperand(Rhs, rhs);
    setTemp(0, temp);
  }

  const LAllocation* lhsDest() { return getOperand(LhsDest); }
  const LAllocation* rhs() { return getOperand(Rhs); }
  const LDefinition* temp() { return getTemp(0); }
  Op op() { return op_; }
  SimdConstant control() { return control_; }
};

// (v128, imm_simd) -> v128 effect-free operation.
class LWasmPermuteSimd128 : public LInstructionHelper<1, 1, 0> {
 public:
  // Permutation operations.
  //
  // The "low-order" byte is in lane 0 of an 8x16 datum, the "high-order" byte
  // in lane 15.  The low-order byte is also the "rightmost".  In wasm, the
  // constant (v128.const i8x16 0 1 2 ... 15) has 0 in the low-order byte and 15
  // in the high-order byte.
  enum Op {
    // A single byte lane is copied into all the other byte lanes.  control_[0]
    // has the source lane.
    BROADCAST_8x16,

    // A single word lane is copied into all the other word lanes.  control_[0]
    // has the source lane.
    BROADCAST_16x8,

    // Copy input to output.
    MOVE,

    // control_ has bytes in range 0..15 s.t. control_[i] holds the source lane
    // for output lane i.
    PERMUTE_8x16,

    // control_ has int16s in range 0..7, as for 8x16.  In addition, the high
    // byte of control_[0] has flags detailing the operation, values taken
    // from the Perm16x8Action enum below.
    PERMUTE_16x8,

    // control_ has int32s in range 0..3, as for 8x16.
    PERMUTE_32x4,

    // control_[0] has the number of places to rotate by.
    ROTATE_RIGHT_8x16,

    // Zeroes are shifted into high-order bytes and low-order bytes are lost.
    // control_[0] has the number of places to shift by.
    SHIFT_RIGHT_8x16,

    // Zeroes are shifted into low-order bytes and high-order bytes are lost.
    // control_[0] has the number of places to shift by.
    SHIFT_LEFT_8x16,
  };

  enum Perm16x8Action {
    SWAP_QWORDS = 1,  // Swap qwords first
    PERM_LOW = 2,     // Permute low qword by control_[0..3]
    PERM_HIGH = 4     // Permute high qword by control_[4..7]
  };

 private:
  Op op_;
  SimdConstant control_;

 public:
  LIR_HEADER(WasmPermuteSimd128)

  static constexpr uint32_t Src = 0;

  LWasmPermuteSimd128(const LAllocation& src, Op op, SimdConstant control)
      : LInstructionHelper(classOpcode), op_(op), control_(control) {
    setOperand(Src, src);
  }

  const LAllocation* src() { return getOperand(Src); }
  Op op() { return op_; }
  SimdConstant control() { return control_; }
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

// (v128, onTrue, onFalse) test-and-branch operations.
class LWasmReduceAndBranchSimd128 : public LControlInstructionHelper<2, 1, 0> {
  wasm::SimdOp op_;

 public:
  LIR_HEADER(WasmReduceAndBranchSimd128)

  static constexpr uint32_t Src = 0;
  static constexpr uint32_t IfTrue = 0;
  static constexpr uint32_t IfFalse = 1;

  LWasmReduceAndBranchSimd128(const LAllocation& src, wasm::SimdOp op,
                              MBasicBlock* ifTrue, MBasicBlock* ifFalse)
      : LControlInstructionHelper(classOpcode), op_(op) {
    setOperand(Src, src);
    setSuccessor(IfTrue, ifTrue);
    setSuccessor(IfFalse, ifFalse);
  }

  const LAllocation* src() { return getOperand(Src); }
  wasm::SimdOp simdOp() const { return op_; }
  MBasicBlock* ifTrue() const { return getSuccessor(IfTrue); }
  MBasicBlock* ifFalse() const { return getSuccessor(IfFalse); }
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

#endif /* jit_x86_shared_LIR_x86_shared_h */

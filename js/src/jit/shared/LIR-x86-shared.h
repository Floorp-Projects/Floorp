/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_shared_LIR_x86_shared_h
#define jit_shared_LIR_x86_shared_h

namespace js {
namespace jit {

class LDivI : public LBinaryMath<1>
{
  public:
    LIR_HEADER(DivI)

    LDivI(const LAllocation &lhs, const LAllocation &rhs, const LDefinition &temp) {
        setOperand(0, lhs);
        setOperand(1, rhs);
        setTemp(0, temp);
    }

    const char *extraName() const {
        if (mir()->isTruncated()) {
            if (mir()->canBeNegativeZero()) {
                return mir()->canBeNegativeOverflow()
                       ? "Truncate_NegativeZero_NegativeOverflow"
                       : "Truncate_NegativeZero";
            }
            return mir()->canBeNegativeOverflow() ? "Truncate_NegativeOverflow" : "Truncate";
        }
        if (mir()->canBeNegativeZero())
            return mir()->canBeNegativeOverflow() ? "NegativeZero_NegativeOverflow" : "NegativeZero";
        return mir()->canBeNegativeOverflow() ? "NegativeOverflow" : nullptr;
    }

    const LDefinition *remainder() {
        return getTemp(0);
    }
    MDiv *mir() const {
        return mir_->toDiv();
    }
};

// Signed division by a power-of-two constant.
class LDivPowTwoI : public LBinaryMath<0>
{
    const int32_t shift_;

  public:
    LIR_HEADER(DivPowTwoI)

    LDivPowTwoI(const LAllocation &lhs, const LAllocation &lhsCopy, int32_t shift)
      : shift_(shift)
    {
        setOperand(0, lhs);
        setOperand(1, lhsCopy);
    }

    const LAllocation *numerator() {
        return getOperand(0);
    }
    const LAllocation *numeratorCopy() {
        return getOperand(1);
    }
    int32_t shift() const {
        return shift_;
    }
    MDiv *mir() const {
        return mir_->toDiv();
    }
};

// Division of a number by itself. Returns 1 unless the number is zero.
class LDivSelfI : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(DivSelfI)

    LDivSelfI(const LAllocation &op) {
        setOperand(0, op);
    }

    const LAllocation *op() {
        return getOperand(0);
    }

    MDiv *mir() const {
        return mir_->toDiv();
    }
};

class LModI : public LBinaryMath<1>
{
  public:
    LIR_HEADER(ModI)

    LModI(const LAllocation &lhs, const LAllocation &rhs, const LDefinition &temp) {
        setOperand(0, lhs);
        setOperand(1, rhs);
        setTemp(0, temp);
    }

    const char *extraName() const {
        return mir()->isTruncated() ? "Truncated" : nullptr;
    }

    const LDefinition *remainder() {
        return getDef(0);
    }
    MMod *mir() const {
        return mir_->toMod();
    }
};

// This class performs a simple x86 'div', yielding either a quotient or remainder depending on
// whether this instruction is defined to output eax (quotient) or edx (remainder).
class LUDivOrMod : public LBinaryMath<1>
{
  public:
    LIR_HEADER(UDivOrMod);

    LUDivOrMod(const LAllocation &lhs, const LAllocation &rhs, const LDefinition &temp) {
        setOperand(0, lhs);
        setOperand(1, rhs);
        setTemp(0, temp);
    }

    const LDefinition *remainder() {
        return getTemp(0);
    }

    const char *extraName() const {
        return mir()->isTruncated() ? "Truncated" : nullptr;
    }

    MBinaryArithInstruction *mir() const {
        JS_ASSERT(mir_->isDiv() || mir_->isMod());
        return static_cast<MBinaryArithInstruction *>(mir_);
    }

    bool canBeDivideByZero() const {
        if (mir_->isMod())
            return mir_->toMod()->canBeDivideByZero();
        return mir_->toDiv()->canBeDivideByZero();
    }
};

class LModPowTwoI : public LInstructionHelper<1,1,0>
{
    const int32_t shift_;

  public:
    LIR_HEADER(ModPowTwoI)

    LModPowTwoI(const LAllocation &lhs, int32_t shift)
      : shift_(shift)
    {
        setOperand(0, lhs);
    }

    int32_t shift() const {
        return shift_;
    }
    const LDefinition *remainder() {
        return getDef(0);
    }
    MMod *mir() const {
        return mir_->toMod();
    }
};

// Double raised to a half power.
class LPowHalfD : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(PowHalfD)
    LPowHalfD(const LAllocation &input) {
        setOperand(0, input);
    }

    const LAllocation *input() {
        return getOperand(0);
    }
    const LDefinition *output() {
        return getDef(0);
    }
    MPowHalf *mir() const {
        return mir_->toPowHalf();
    }
};

// Takes a tableswitch with an integer to decide
class LTableSwitch : public LInstructionHelper<0, 1, 2>
{
  public:
    LIR_HEADER(TableSwitch)

    LTableSwitch(const LAllocation &in, const LDefinition &inputCopy,
                 const LDefinition &jumpTablePointer, MTableSwitch *ins)
    {
        setOperand(0, in);
        setTemp(0, inputCopy);
        setTemp(1, jumpTablePointer);
        setMir(ins);
    }

    MTableSwitch *mir() const {
        return mir_->toTableSwitch();
    }

    const LAllocation *index() {
        return getOperand(0);
    }
    const LDefinition *tempInt() {
        return getTemp(0);
    }
    const LDefinition *tempPointer() {
        return getTemp(1);
    }
};

// Takes a tableswitch with a value to decide
class LTableSwitchV : public LInstructionHelper<0, BOX_PIECES, 3>
{
  public:
    LIR_HEADER(TableSwitchV)

    LTableSwitchV(const LDefinition &inputCopy, const LDefinition &floatCopy,
                  const LDefinition &jumpTablePointer, MTableSwitch *ins)
    {
        setTemp(0, inputCopy);
        setTemp(1, floatCopy);
        setTemp(2, jumpTablePointer);
        setMir(ins);
    }

    MTableSwitch *mir() const {
        return mir_->toTableSwitch();
    }

    static const size_t InputValue = 0;

    const LDefinition *tempInt() {
        return getTemp(0);
    }
    const LDefinition *tempFloat() {
        return getTemp(1);
    }
    const LDefinition *tempPointer() {
        return getTemp(2);
    }
};

class LGuardShape : public LInstructionHelper<0, 1, 0>
{
  public:
    LIR_HEADER(GuardShape)

    LGuardShape(const LAllocation &in) {
        setOperand(0, in);
    }
    const MGuardShape *mir() const {
        return mir_->toGuardShape();
    }
};

class LGuardObjectType : public LInstructionHelper<0, 1, 0>
{
  public:
    LIR_HEADER(GuardObjectType)

    LGuardObjectType(const LAllocation &in) {
        setOperand(0, in);
    }
    const MGuardObjectType *mir() const {
        return mir_->toGuardObjectType();
    }
};

class LInterruptCheck : public LInstructionHelper<0, 0, 0>
{
  public:
    LIR_HEADER(InterruptCheck)
};

class LMulI : public LBinaryMath<0, 1>
{
  public:
    LIR_HEADER(MulI)

    LMulI(const LAllocation &lhs, const LAllocation &rhs, const LAllocation &lhsCopy) {
        setOperand(0, lhs);
        setOperand(1, rhs);
        setOperand(2, lhsCopy);
    }

    const char *extraName() const {
        return (mir()->mode() == MMul::Integer)
               ? "Integer"
               : (mir()->canBeNegativeZero() ? "CanBeNegativeZero" : nullptr);
    }

    MMul *mir() const {
        return mir_->toMul();
    }
    const LAllocation *lhsCopy() {
        return this->getOperand(2);
    }
};

} // namespace jit
} // namespace js

#endif /* jit_shared_LIR_x86_shared_h */

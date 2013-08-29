/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_arm_LIR_arm_h
#define jit_arm_LIR_arm_h

namespace js {
namespace jit {

class LBox : public LInstructionHelper<2, 1, 0>
{
    MIRType type_;

  public:
    LIR_HEADER(Box);

    LBox(const LAllocation &in_payload, MIRType type)
      : type_(type)
    {
        setOperand(0, in_payload);
    }

    MIRType type() const {
        return type_;
    }
};

class LBoxDouble : public LInstructionHelper<2, 1, 1>
{
  public:
    LIR_HEADER(BoxDouble);

    LBoxDouble(const LAllocation &in, const LDefinition &temp) {
        setOperand(0, in);
        setTemp(0, temp);
    }
};

class LUnbox : public LInstructionHelper<1, 2, 0>
{
  public:
    LIR_HEADER(Unbox);

    MUnbox *mir() const {
        return mir_->toUnbox();
    }
    const LAllocation *payload() {
        return getOperand(0);
    }
    const LAllocation *type() {
        return getOperand(1);
    }
};

class LUnboxDouble : public LInstructionHelper<1, 2, 0>
{
  public:
    LIR_HEADER(UnboxDouble);

    static const size_t Input = 0;

    MUnbox *mir() const {
        return mir_->toUnbox();
    }
};

// Convert a 32-bit unsigned integer to a double.
class LUInt32ToDouble : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(UInt32ToDouble)

    LUInt32ToDouble(const LAllocation &input) {
        setOperand(0, input);
    }
};

class LDivI : public LBinaryMath<1>
{
  public:
    LIR_HEADER(DivI);

    LDivI(const LAllocation &lhs, const LAllocation &rhs,
          const LDefinition &temp) {
        setOperand(0, lhs);
        setOperand(1, rhs);
        setTemp(0, temp);
    }

    MDiv *mir() const {
        return mir_->toDiv();
    }
};

// LSoftDivI is a software divide for ARM cores that don't support a hardware
// divide instruction.
//
// It is implemented as a proper C function so it trashes r0, r1, r2 and r3.  The
// call also trashes lr, and has the ability to trash ip. The function also
// takes two arguments (dividend in r0, divisor in r1). The LInstruction gets
// encoded such that the divisor and dividend are passed in their apropriate
// registers, and are marked as copy so we can modify them (and the function
// will).  The other thre registers that can be trashed are marked as such. For
// the time being, the link register is not marked as trashed because we never
// allocate to the link register.
class LSoftDivI : public LBinaryMath<2>
{
  public:
    LIR_HEADER(SoftDivI);

    LSoftDivI(const LAllocation &lhs, const LAllocation &rhs,
              const LDefinition &temp1, const LDefinition &temp2) {
        setOperand(0, lhs);
        setOperand(1, rhs);
        setTemp(0, temp1);
        setTemp(1, temp2);
    }

    MDiv *mir() const {
        return mir_->toDiv();
    }
};

class LDivPowTwoI : public LInstructionHelper<1, 1, 0>
{
    const int32_t shift_;

  public:
    LIR_HEADER(DivPowTwoI)

    LDivPowTwoI(const LAllocation &lhs, int32_t shift)
      : shift_(shift)
    {
        setOperand(0, lhs);
    }

    const LAllocation *numerator() {
        return getOperand(0);
    }

    int32_t shift() {
        return shift_;
    }

    MDiv *mir() const {
        return mir_->toDiv();
    }
};

class LModI : public LBinaryMath<1>
{
  public:
    LIR_HEADER(ModI);

    LModI(const LAllocation &lhs, const LAllocation &rhs,
          const LDefinition &callTemp)
    {
        setOperand(0, lhs);
        setOperand(1, rhs);
        setTemp(0, callTemp);
    }

    MMod *mir() const {
        return mir_->toMod();
    }
};

class LSoftModI : public LBinaryMath<3>
{
  public:
    LIR_HEADER(SoftModI);

    LSoftModI(const LAllocation &lhs, const LAllocation &rhs,
              const LDefinition &temp1, const LDefinition &temp2,
              const LDefinition &callTemp)
    {
        setOperand(0, lhs);
        setOperand(1, rhs);
        setTemp(0, temp1);
        setTemp(1, temp2);
        setTemp(2, callTemp);
    }

    MMod *mir() const {
        return mir_->toMod();
    }
};

class LModPowTwoI : public LInstructionHelper<1, 1, 0>
{
    const int32_t shift_;

  public:
    LIR_HEADER(ModPowTwoI);
    int32_t shift()
    {
        return shift_;
    }

    LModPowTwoI(const LAllocation &lhs, int32_t shift)
      : shift_(shift)
    {
        setOperand(0, lhs);
    }

    MMod *mir() const {
        return mir_->toMod();
    }
};

class LModMaskI : public LInstructionHelper<1, 1, 1>
{
    const int32_t shift_;

  public:
    LIR_HEADER(ModMaskI);

    LModMaskI(const LAllocation &lhs, const LDefinition &temp1, int32_t shift)
      : shift_(shift)
    {
        setOperand(0, lhs);
        setTemp(0, temp1);
    }

    int32_t shift() const {
        return shift_;
    }

    MMod *mir() const {
        return mir_->toMod();
    }
};

class LPowHalfD : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(PowHalfD);
    LPowHalfD(const LAllocation &input) {
        setOperand(0, input);
    }

    const LAllocation *input() {
        return getOperand(0);
    }
    const LDefinition *output() {
        return getDef(0);
    }
};

// Takes a tableswitch with an integer to decide
class LTableSwitch : public LInstructionHelper<0, 1, 1>
{
  public:
    LIR_HEADER(TableSwitch);

    LTableSwitch(const LAllocation &in, const LDefinition &inputCopy, MTableSwitch *ins) {
        setOperand(0, in);
        setTemp(0, inputCopy);
        setMir(ins);
    }

    MTableSwitch *mir() const {
        return mir_->toTableSwitch();
    }

    const LAllocation *index() {
        return getOperand(0);
    }
    const LAllocation *tempInt() {
        return getTemp(0)->output();
    }
    // This is added to share the same CodeGenerator prefixes.
    const LAllocation *tempPointer() {
        return NULL;
    }
};

// Takes a tableswitch with an integer to decide
class LTableSwitchV : public LInstructionHelper<0, BOX_PIECES, 2>
{
  public:
    LIR_HEADER(TableSwitchV);

    LTableSwitchV(const LDefinition &inputCopy, const LDefinition &floatCopy,
                  MTableSwitch *ins)
    {
        setTemp(0, inputCopy);
        setTemp(1, floatCopy);
        setMir(ins);
    }

    MTableSwitch *mir() const {
        return mir_->toTableSwitch();
    }

    static const size_t InputValue = 0;

    const LAllocation *tempInt() {
        return getTemp(0)->output();
    }
    const LAllocation *tempFloat() {
        return getTemp(1)->output();
    }
    const LAllocation *tempPointer() {
        return NULL;
    }
};

class LGuardShape : public LInstructionHelper<0, 1, 1>
{
  public:
    LIR_HEADER(GuardShape);

    LGuardShape(const LAllocation &in, const LDefinition &temp) {
        setOperand(0, in);
        setTemp(0, temp);
    }
    const MGuardShape *mir() const {
        return mir_->toGuardShape();
    }
    const LAllocation *tempInt() {
        return getTemp(0)->output();
    }
};

class LGuardObjectType : public LInstructionHelper<0, 1, 1>
{
  public:
    LIR_HEADER(GuardObjectType);

    LGuardObjectType(const LAllocation &in, const LDefinition &temp) {
        setOperand(0, in);
        setTemp(0, temp);
    }
    const MGuardObjectType *mir() const {
        return mir_->toGuardObjectType();
    }
    const LAllocation *tempInt() {
        return getTemp(0)->output();
    }
};

class LInterruptCheck : public LInstructionHelper<0, 0, 0>
{
  public:
    LIR_HEADER(InterruptCheck);
};

class LMulI : public LBinaryMath<0>
{
  public:
    LIR_HEADER(MulI);

    MMul *mir() {
        return mir_->toMul();
    }
};

class LUDiv : public LBinaryMath<0>
{
  public:
    LIR_HEADER(UDiv);
};

class LUMod : public LBinaryMath<0>
{
  public:
    LIR_HEADER(UMod);
};

// This class performs a simple x86 'div', yielding either a quotient or remainder depending on
// whether this instruction is defined to output eax (quotient) or edx (remainder).
class LSoftUDivOrMod : public LBinaryMath<2>
{
  public:
    LIR_HEADER(SoftUDivOrMod);

    LSoftUDivOrMod(const LAllocation &lhs, const LAllocation &rhs, const LDefinition &temp1, const LDefinition &temp2) {
        setOperand(0, lhs);
        setOperand(1, rhs);
        setTemp(0, temp1);
        setTemp(1, temp2);
    }
    // this is incorrect, it is returned in r1, getTemp(0) is r2.
    const LDefinition *remainder() {
        return getTemp(0);
    }
};

class LAsmJSLoadFuncPtr : public LInstructionHelper<1, 1, 1>
{
  public:
    LIR_HEADER(AsmJSLoadFuncPtr);
    LAsmJSLoadFuncPtr(const LAllocation &index, const LDefinition &temp) {
        setOperand(0, index);
        setTemp(0, temp);
    }
    const MAsmJSLoadFuncPtr *mir() const {
        return mir_->toAsmJSLoadFuncPtr();
    }
    const LAllocation *index() {
        return getOperand(0);
    }
    const LDefinition *temp() {
        return getTemp(0);
    }
};

} // namespace jit
} // namespace js

#endif /* jit_arm_LIR_arm_h */

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

#ifndef jsion_lir_common_h__
#define jsion_lir_common_h__

#include "ion/shared/Assembler-shared.h"

// This file declares LIR instructions that are common to every platform.

namespace js {
namespace ion {

class LMove
{
    LAllocation *from_;
    LAllocation *to_;

  public:
    LMove(LAllocation *from, LAllocation *to)
      : from_(from),
        to_(to)
    { }

    LAllocation *from() {
        return from_;
    }
    const LAllocation *from() const {
        return from_;
    }
    LAllocation *to() {
        return to_;
    }
    const LAllocation *to() const {
        return to_;
    }
};

class LMoveGroup : public LInstructionHelper<0, 0, 0>
{
    js::Vector<LMove, 2, IonAllocPolicy> moves_;

  public:
    LIR_HEADER(MoveGroup);

    void printOperands(FILE *fp);
    bool add(LAllocation *from, LAllocation *to) {
        JS_ASSERT(*from != *to);
        return moves_.append(LMove(from, to));
    }
    size_t numMoves() const {
        return moves_.length();
    }
    const LMove &getMove(size_t i) const {
        return moves_[i];
    }
};

// Constant 32-bit integer.
class LInteger : public LInstructionHelper<1, 0, 0>
{
    int32 i32_;

  public:
    LIR_HEADER(Integer);

    LInteger(int32 i32) : i32_(i32)
    { }

    int32 getValue() const {
        return i32_;
    }
};

// Constant 64-bit pointer.
class LPointer : public LInstructionHelper<1, 0, 0>
{
    void *ptr_;

  public:
    LIR_HEADER(Pointer);

    LPointer(void *ptr) : ptr_(ptr)
    { }
};

// A constant Value.
class LValue : public LInstructionHelper<BOX_PIECES, 0, 0>
{
    Value v_;

  public:
    LIR_HEADER(Value);

    LValue(const Value &v) : v_(v)
    { }

    Value value() const {
        return v_;
    }
};

// Formal argument for a function, returning a box. Formal arguments are
// initially read from the stack.
class LParameter : public LInstructionHelper<BOX_PIECES, 0, 0>
{
  public:
    LIR_HEADER(Parameter);
};

// Jumps to the start of a basic block.
class LGoto : public LInstructionHelper<0, 0, 0>
{
    MBasicBlock *block_;

  public:
    LIR_HEADER(Goto);

    LGoto(MBasicBlock *block)
      : block_(block)
    { }

    MBasicBlock *target() const {
        return block_;
    }
};

// Takes a tableswitch with an integer to decide
class LTableSwitch : public LInstructionHelper<0, 1, 2>
{
    MTableSwitch *mir_;

  public:
    LIR_HEADER(TableSwitch);

    LTableSwitch(const LAllocation &in, const LDefinition &inputCopy,
                 const LDefinition &jumpTablePointer, MTableSwitch *mir)
      : mir_(mir)
    {
        setOperand(0, in);
        setTemp(0, inputCopy);
        setTemp(1, jumpTablePointer);
    }

    MTableSwitch *mir() const {
        return mir_;
    }
};

// Takes in either an integer or boolean input and tests it for truthiness.
class LTestIAndBranch : public LInstructionHelper<0, 1, 0>
{
    MBasicBlock *ifTrue_;
    MBasicBlock *ifFalse_;

  public:
    LIR_HEADER(TestIAndBranch);

    LTestIAndBranch(const LAllocation &in, MBasicBlock *ifTrue, MBasicBlock *ifFalse)
      : ifTrue_(ifTrue),
        ifFalse_(ifFalse)
    {
        setOperand(0, in);
    }

    MBasicBlock *ifTrue() const {
        return ifTrue_;
    }
    MBasicBlock *ifFalse() const {
        return ifFalse_;
    }
};

// Takes in either an integer or boolean input and tests it for truthiness.
class LTestDAndBranch : public LInstructionHelper<0, 1, 1>
{
    MBasicBlock *ifTrue;
    MBasicBlock *ifFalse;

  public:
    LIR_HEADER(TestDAndBranch);

    LTestDAndBranch(const LAllocation &in, const LDefinition &temp,
                    MBasicBlock *ifTrue, MBasicBlock *ifFalse)
      : ifTrue(ifTrue),
        ifFalse(ifFalse)
    {
        setOperand(0, in);
        setTemp(0, temp);
    }
};

// Takes in a boxed value and tests it for truthiness.
class LTestVAndBranch : public LInstructionHelper<0, BOX_PIECES, 0>
{
    MBasicBlock *ifTrue;
    MBasicBlock *ifFalse;

  public:
    LIR_HEADER(TestVAndBranch);

    LTestVAndBranch(MBasicBlock *ifTrue, MBasicBlock *ifFalse)
      : ifTrue(ifTrue),
        ifFalse(ifFalse)
    { }
};

class LCompareI : public LInstructionHelper<1, 2, 0>
{
    JSOp op_;

  public:
    LIR_HEADER(CompareI);
    LCompareI(JSOp op, const LAllocation &left, const LAllocation &right)
      : op_(op)
    {
        setOperand(0, left);
        setOperand(1, right);
    }
};

class LCompareD : public LInstructionHelper<1, 2, 0>
{
    JSOp op_;

  public:
    LIR_HEADER(CompareD);
    LCompareD(JSOp op, const LAllocation &left, const LAllocation &right)
      : op_(op)
    {
        setOperand(0, left);
        setOperand(1, right);
    }
};

class LCompareIAndBranch : public LInstructionHelper<0, 2, 0>
{
    JSOp op_;
    MBasicBlock *ifTrue_;
    MBasicBlock *ifFalse_;

  public:
    LIR_HEADER(CompareIAndBranch);
    LCompareIAndBranch(JSOp op, const LAllocation &left, const LAllocation &right,
                       MBasicBlock *ifTrue, MBasicBlock *ifFalse)
      : op_(op),
        ifTrue_(ifTrue),
        ifFalse_(ifFalse)
    {
        setOperand(0, left);
        setOperand(1, right);
    }

    MBasicBlock *ifTrue() const {
        return ifTrue_;
    }
    MBasicBlock *ifFalse() const {
        return ifFalse_;
    }
};

class LCompareDAndBranch : public LInstructionHelper<0, 2, 0>
{
    JSOp op_;
    MBasicBlock *ifTrue_;
    MBasicBlock *ifFalse_;

  public:
    LIR_HEADER(CompareDAndBranch);
    LCompareDAndBranch(JSOp op, const LAllocation &left, const LAllocation &right,
                       MBasicBlock *ifTrue, MBasicBlock *ifFalse)
      : op_(op),
        ifTrue_(ifTrue),
        ifFalse_(ifFalse)
    {
        setOperand(0, left);
        setOperand(1, right);
    }

    MBasicBlock *ifTrue() const {
        return ifTrue_;
    }
    MBasicBlock *ifFalse() const {
        return ifFalse_;
    }
};

// Bitwise not operation, takes a 32-bit integer as input and returning
// a 32-bit integer result as an output.
class LBitNot : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(BitNot);
};

// Binary bitwise operation, taking two 32-bit integers as inputs and returning
// a 32-bit integer result as an output.
class LBitOp : public LInstructionHelper<1, 2, 0>
{
    JSOp op_;

  public:
    LIR_HEADER(BitOp);

    LBitOp(JSOp op)
      : op_(op)
    { }

    JSOp bitop() {
        return op_;
    }
};

// Returns from the function being compiled (not used in inlined frames). The
// input must be a box.
class LReturn : public LInstructionHelper<0, BOX_PIECES, 0>
{
  public:
    LIR_HEADER(Return);
};

// Adds two integers, returning an integer value.
class LAddI : public LInstructionHelper<1, 2, 0>
{
  public:
    LIR_HEADER(AddI);
};

// Adds two integers, returning an integer value.
class LMulI : public LInstructionHelper<1, 2, 0>
{
    MMul *mir_;

  public:
    LIR_HEADER(MulI);

    LMulI(MMul *mir)
      : mir_(mir)
    { }

    MMul *mir() {
        return mir_;
    }
};

// Performs an add, sub, mul, or div on two double values.
class LMathD : public LInstructionHelper<1, 2, 0>
{
    JSOp jsop_;

  public:
    LIR_HEADER(MathD);

    LMathD(JSOp jsop)
      : jsop_(jsop)
    { }

    JSOp jsop() const {
        return jsop_;
    }
};

// Convert a 32-bit integer to a double.
class LInt32ToDouble : public LInstructionHelper<1, 1, 0>
{
  public:
    LIR_HEADER(Int32ToDouble);

    LInt32ToDouble(const LAllocation &input) {
        setOperand(0, input);
    }

    const LAllocation *input() {
        return getOperand(0);
    }
    const LDefinition *output() {
        return getDef(0);
    }
};

// Convert a value to a double.
class LValueToDouble : public LInstructionHelper<1, BOX_PIECES, 0>
{
  public:
    LIR_HEADER(ValueToDouble);

    static const size_t Input = 0;

    const LDefinition *output() {
        return getDef(0);
    }
};

// Convert a value to an int32.
//   Input: components of a Value
//   Output: 32-bit integer
//   
// This instruction requires a temporary float register.
class LValueToInt32 : public LInstructionHelper<1, BOX_PIECES, 1>
{
  public:
    LIR_HEADER(ValueToInt32);

    LValueToInt32(const LDefinition &temp) {
        setTemp(0, temp);
    }

    static const size_t Input = 0;

    const LDefinition *tempFloat() {
        return getTemp(0);
    }
    const LDefinition *output() {
        return getDef(0);
    }
};

class MPhi;

// Phi is a pseudo-instruction that emits no code, and is an annotation for the
// register allocator. Like its equivalent in MIR, phis are collected at the
// top of blocks and are meant to be executed in parallel, choosing the input
// corresponding to the predecessor taken in the control flow graph.
class LPhi : public LInstruction
{
    uint32 numInputs_;
    LAllocation *inputs_;
    LDefinition def_;

    bool init(MIRGenerator *gen);

    LPhi(MPhi *mir);

  public:
    LIR_HEADER(Phi);

    static LPhi *New(MIRGenerator *gen, MPhi *phi);

    size_t numDefs() const {
        return 1;
    }
    LDefinition *getDef(size_t index) {
        JS_ASSERT(index == 0);
        return &def_;
    }
    void setDef(size_t index, const LDefinition &def) {
        JS_ASSERT(index == 0);
        def_ = def;
    }
    size_t numOperands() const {
        return numInputs_;
    }
    LAllocation *getOperand(size_t index) {
        JS_ASSERT(index < numOperands());
        return &inputs_[index];
    }
    void setOperand(size_t index, const LAllocation &a) {
        JS_ASSERT(index < numOperands());
        inputs_[index] = a;
    }
    size_t numTemps() const {
        return 0;
    }
    LDefinition *getTemp(size_t index) {
        JS_NOT_REACHED("no temps");
        return NULL;
    }
    void setTemp(size_t index, const LDefinition &temp) {
        JS_NOT_REACHED("no temps");
    }

    virtual void printInfo(FILE *fp) {
        printOperands(fp);
    }
};

} // namespace ion
} // namespace js

#endif // jsion_lir_common_h__


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

#ifndef jsion_codegen_arm_h__
#define jsion_codegen_arm_h__

#include "Assembler-arm.h"
#include "ion/shared/CodeGenerator-shared.h"

namespace js {
namespace ion {

class OutOfLineBailout;

class CodeGeneratorARM : public CodeGeneratorShared
{
    friend class MoveResolverARM;
    CodeGeneratorARM *thisFromCtor() {return this;}
  protected:
    // Label for the common return path.
    HeapLabel *returnLabel_;
    HeapLabel *deoptLabel_;

    inline Operand ToOperand(const LAllocation &a) {
        if (a.isGeneralReg())
            return Operand(a.toGeneralReg()->reg());
        if (a.isFloatReg())
            return Operand(a.toFloatReg()->reg());
        return Operand(StackPointer, ToStackOffset(&a));
    }
    inline Operand ToOperand(const LAllocation *a) {
        return ToOperand(*a);
    }
    inline Operand ToOperand(const LDefinition *def) {
        return ToOperand(def->output());
    }

    MoveResolver::MoveOperand toMoveOperand(const LAllocation *a) const;

    bool bailoutIf(Assembler::Condition condition, LSnapshot *snapshot);

  protected:
    bool generatePrologue();
    bool generateEpilogue();
    bool generateOutOfLineCode();

    bool emitDoubleToInt32(const FloatRegister &src, const Register &dest, LSnapshot *snapshot);

  public:
    // Instruction visitors.
    virtual bool visitGoto(LGoto *jump);
    virtual bool visitAddI(LAddI *ins);
    virtual bool visitMulI(LMulI *ins);
    virtual bool visitBitNot(LBitNot *ins);
    virtual bool visitBitOp(LBitOp *ins);
    virtual bool visitMoveGroup(LMoveGroup *group);
    virtual bool visitInteger(LInteger *ins);
    virtual bool visitTestIAndBranch(LTestIAndBranch *test);
    virtual bool visitCompareI(LCompareI *comp);
    virtual bool visitCompareIAndBranch(LCompareIAndBranch *comp);
    virtual bool visitMathD(LMathD *math);
    virtual bool visitTableSwitch(LTableSwitch *ins);

    // Out of line visitors.
    bool visitOutOfLineBailout(OutOfLineBailout *ool);
private:
    class DeferredDouble : public TempObject
    {
        AbsoluteLabel label_;
        uint32 index_;

      public:
        DeferredDouble(uint32 index) : index_(index)
        { }

        AbsoluteLabel *label() {
            return &label_;
        }
        uint32 index() const {
            return index_;
        }
    };

  private:
    js::Vector<DeferredDouble *, 0, SystemAllocPolicy> deferredDoubles_;

  protected:
    ValueOperand ToValue(LInstruction *ins, size_t pos);

  protected:
    void linkAbsoluteLabels();

  public:
    CodeGeneratorARM(MIRGenerator *gen, LIRGraph &graph);

  public:
    bool visitBox(LBox *box);
    bool visitBoxDouble(LBoxDouble *box);
    bool visitUnbox(LUnbox *unbox);
    bool visitUnboxDouble(LUnboxDouble *ins);
    bool visitValue(LValue *value);
    bool visitReturn(LReturn *ret);
    bool visitDouble(LDouble *ins);
    bool visitCompareD(LCompareD *comp);
    bool visitCompareDAndBranch(LCompareDAndBranch *comp);
};

typedef CodeGeneratorARM CodeGeneratorSpecific;

// An out-of-line bailout thunk.
class OutOfLineBailout : public OutOfLineCodeBase<CodeGeneratorARM>
{
    LSnapshot *snapshot_;
    uint32 frameSize_;

  public:
    OutOfLineBailout(LSnapshot *snapshot, uint32 frameSize)
      : snapshot_(snapshot),
        frameSize_(frameSize)
    { }

    bool accept(CodeGeneratorARM *codegen);

    LSnapshot *snapshot() const {
        return snapshot_;
    }
};

} // ion
} // js

#endif // jsion_codegen_arm_h__


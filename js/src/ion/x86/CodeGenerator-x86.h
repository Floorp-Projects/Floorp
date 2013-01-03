/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_codegen_x86_h__
#define jsion_codegen_x86_h__

#include "Assembler-x86.h"
#include "ion/shared/CodeGenerator-x86-shared.h"

namespace js {
namespace ion {

class CodeGeneratorX86 : public CodeGeneratorX86Shared
{
    class DeferredDouble : public TempObject
    {
        AbsoluteLabel label_;
        uint32_t index_;

      public:
        DeferredDouble(uint32_t index) : index_(index)
        { }

        AbsoluteLabel *label() {
            return &label_;
        }
        uint32_t index() const {
            return index_;
        }
    };

  private:
    js::Vector<DeferredDouble *, 0, SystemAllocPolicy> deferredDoubles_;

    CodeGeneratorX86 *thisFromCtor() {
        return this;
    }

  protected:
    ValueOperand ToValue(LInstruction *ins, size_t pos);
    ValueOperand ToOutValue(LInstruction *ins);
    ValueOperand ToTempValue(LInstruction *ins, size_t pos);

    void storeElementTyped(const LAllocation *value, MIRType valueType, MIRType elementType,
                           const Register &elements, const LAllocation *index);

  protected:
    void linkAbsoluteLabels();

  public:
    CodeGeneratorX86(MIRGenerator *gen, LIRGraph *graph);

  public:
    bool visitBox(LBox *box);
    bool visitBoxDouble(LBoxDouble *box);
    bool visitUnbox(LUnbox *unbox);
    bool visitValue(LValue *value);
    bool visitOsrValue(LOsrValue *value);
    bool visitDouble(LDouble *ins);
    bool visitLoadSlotV(LLoadSlotV *load);
    bool visitLoadSlotT(LLoadSlotT *load);
    bool visitStoreSlotT(LStoreSlotT *store);
    bool visitLoadElementT(LLoadElementT *load);
    bool visitImplicitThis(LImplicitThis *lir);
    bool visitRecompileCheck(LRecompileCheck *lir);
    bool visitInterruptCheck(LInterruptCheck *lir);
    bool visitCompareB(LCompareB *lir);
    bool visitCompareBAndBranch(LCompareBAndBranch *lir);
    bool visitCompareV(LCompareV *lir);
    bool visitCompareVAndBranch(LCompareVAndBranch *lir);
};

typedef CodeGeneratorX86 CodeGeneratorSpecific;

} // namespace ion
} // namespace js

#endif // jsion_codegen_x86_h__


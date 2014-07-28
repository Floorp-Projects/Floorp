/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_none_CodeGenerator_none_h
#define jit_none_CodeGenerator_none_h

#include "jit/shared/CodeGenerator-shared.h"

namespace js {
namespace jit {

class CodeGeneratorNone : public CodeGeneratorShared
{
  public:
    NonAssertingLabel returnLabel_;

    CodeGeneratorNone(MIRGenerator *gen, LIRGraph *graph, MacroAssembler *masm)
      : CodeGeneratorShared(gen, graph, masm)
    {
        MOZ_CRASH();
    }

    template <typename T> inline Register ToOperand(T) { MOZ_CRASH(); }
    MoveOperand toMoveOperand(const LAllocation *) const { MOZ_CRASH(); }
    template <typename T1, typename T2>
    bool bailoutCmp32(Assembler::Condition, T1, T2, LSnapshot *) { MOZ_CRASH(); }
    template<typename T>
    bool bailoutTest32(Assembler::Condition, Register, T, LSnapshot *) { MOZ_CRASH(); }
    template <typename T1, typename T2>
    bool bailoutCmpPtr(Assembler::Condition, T1, T2, LSnapshot *) { MOZ_CRASH(); }
    bool bailoutTestPtr(Assembler::Condition, Register, Register, LSnapshot *) { MOZ_CRASH(); }
    bool bailoutIfFalseBool(Register, LSnapshot *) { MOZ_CRASH(); }
    bool bailoutFrom(Label *, LSnapshot *) { MOZ_CRASH(); }
    bool bailout(LSnapshot *) { MOZ_CRASH(); }
    bool bailoutIf(Assembler::Condition, LSnapshot *) { MOZ_CRASH(); }
    bool generatePrologue() { MOZ_CRASH(); }
    bool generateEpilogue() { MOZ_CRASH(); }
    bool generateOutOfLineCode() { MOZ_CRASH(); }
    void testNullEmitBranch(Assembler::Condition, ValueOperand, MBasicBlock *, MBasicBlock *) {
        MOZ_CRASH();
    }
    void testUndefinedEmitBranch(Assembler::Condition, ValueOperand, MBasicBlock *, MBasicBlock *) {
        MOZ_CRASH();
    }
    bool emitTableSwitchDispatch(MTableSwitch *, Register, Register) { MOZ_CRASH(); }
    ValueOperand ToValue(LInstruction *, size_t) { MOZ_CRASH(); }
    ValueOperand ToOutValue(LInstruction *) { MOZ_CRASH(); }
    ValueOperand ToTempValue(LInstruction *, size_t) { MOZ_CRASH(); }
    bool generateInvalidateEpilogue() { MOZ_CRASH(); }
    void postAsmJSCall(LAsmJSCall *) { MOZ_CRASH(); }
};

typedef CodeGeneratorNone CodeGeneratorSpecific;

} // namespace jit
} // namespace js

#endif /* jit_none_CodeGenerator_none_h */

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
#include "CodeGenerator-x64.h"
#include "ion/shared/CodeGenerator-shared-inl.h"
#include "ion/MIR.h"
#include "ion/MIRGraph.h"

using namespace js;
using namespace js::ion;

CodeGenerator::CodeGenerator(MIRGenerator *gen, LIRGraph &graph)
  : CodeGeneratorX86Shared(gen, graph),
    moveHelper(thisFromCtor())
{
}

bool
CodeGenerator::visitMoveGroup(LMoveGroup *group)
{
    if (!moveGroupResolver.resolve(group))
        return false;

    moveHelper.setup(group);

    for (size_t i = 0; i < moveGroupResolver.numMoves(); i++)
        moveHelper.emit(moveGroupResolver.getMove(i));

    moveHelper.finish();
    return true;
}

bool
CodeGenerator::visitValue(LValue *value)
{
    jsval_layout jv;
    jv.asBits = JSVAL_BITS(Jsvalify(value->value()));

    LDefinition *reg = value->getDef(0);

    if (value->value().isMarkable())
        masm.movq(ImmGCPtr(jv.asPtr), ToRegister(reg));
    else
        masm.movq(ImmWord(jv.asBits), ToRegister(reg));
    return true;
}

static inline JSValueShiftedTag
MIRTypeToShiftedTag(MIRType type)
{
    switch (type) {
      case MIRType_Int32:
        return JSVAL_SHIFTED_TAG_INT32;
      case MIRType_String:
        return JSVAL_SHIFTED_TAG_STRING;
      case MIRType_Boolean:
        return JSVAL_SHIFTED_TAG_BOOLEAN;
      case MIRType_Object:
        return JSVAL_SHIFTED_TAG_OBJECT;
      default:
        JS_NOT_REACHED("unexpected type");
        return JSVAL_SHIFTED_TAG_NULL;
    }
}

bool
CodeGenerator::visitBox(LBox *box)
{
    const LAllocation *in = box->getOperand(0);
    const LDefinition *result = box->getDef(0);

    if (box->type() != MIRType_Double) {
        JSValueShiftedTag tag = MIRTypeToShiftedTag(box->type());
        masm.movq(ImmWord(tag), ToRegister(result));
        masm.orq(ToOperand(in), ToRegister(result));
    } else {
        JS_NOT_REACHED("NYI");
    }
    return true;
}

bool
CodeGenerator::visitUnboxInteger(LUnboxInteger *unbox)
{
    const LAllocation *value = unbox->getOperand(0);
    const LDefinition *result = unbox->getDef(0);

    masm.movq(ToOperand(value), ToRegister(result));
    masm.shlq(Imm32(JSVAL_TAG_SHIFT), ToRegister(result));
    masm.cmpl(Imm32(JSVAL_TAG_INT32), ToOperand(result));
    masm.movl(ToOperand(value), ToRegister(result));

    return true;
}

bool
CodeGenerator::visitReturn(LReturn *ret)
{
#ifdef DEBUG
    LAllocation *result = ret->getOperand(0);
    JS_ASSERT(ToRegister(result) == JSReturnReg);
#endif
    // Don't emit a jump to the return label if this is the last block.
    if (current->mir()->id() + 1 != graph.numBlocks())
        masm.jmp(returnLabel_);
    return true;
}


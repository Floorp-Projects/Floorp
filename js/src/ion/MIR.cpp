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
#include "MIR.h"

using namespace js;
using namespace js::ion;

MIRGenerator::MIRGenerator(JSContext *cx, TempAllocator &temp, JSScript *script, JSFunction *fun)
  : cx(cx),
    pc(NULL),
    temp_(temp),
    script_(script),
    fun_(fun),
    blocks_(TempAllocPolicy(cx))
{
    nslots_ = script->nslots + (fun ? fun->nargs + 2 : 0);
}

void
MInstruction::unlinkUse(MOperand *prev, MOperand *use)
{
    JS_ASSERT(use->ins() == this);
    if (!prev) {
        JS_ASSERT(uses_ = use);
        uses_ = use->next();
    } else {
        JS_ASSERT(prev->next() == use);
        prev->next_ = use->next();
    }
}

void
MInstruction::replaceOperand(MOperand *prev, MOperand *use, MInstruction *ins)
{
    JS_ASSERT_IF(prev, prev->ins() == use->ins());

    use->ins()->unlinkUse(prev, use);
    use->setIns(ins);
    ins->linkUse(use);
}

MConstant *
MConstant::New(MIRGenerator *gen, const Value &v)
{
    return new (gen->temp()) MConstant(v);
}

MConstant::MConstant(const js::Value &vp)
  : value_(vp)
{
    setType(MIRTypeFromValue(vp));
    switch (type()) {
      case MIRType_Int32:
      case MIRType_Boolean:
      case MIRType_Magic:
        setRepresentation(Representation::Int32);
        break;
      case MIRType_Double:
        setRepresentation(Representation::Double);
        break;
      case MIRType_String:
      case MIRType_Object:
      case MIRType_Function:
        setRepresentation(Representation::Pointer);
        break;
      case MIRType_Undefined:
      case MIRType_Null:
        // These are special singletons that shouldn't be broken up.
        setRepresentation(Representation::Box);
        break;
      default:
        JS_NOT_REACHED("unexpected constant type");
    }
}

MParameter *
MParameter::New(MIRGenerator *gen, int32 index)
{
    JS_ASSERT(index >= CALLEE_SLOT && index < int32(gen->fun()->nargs));
    return new (gen->temp()) MParameter(index);
}

MCopy *
MCopy::New(MIRGenerator *gen, MInstruction *ins)
{
    if (!ins)
        return NULL;

    // Don't create nested copies.
    if (ins->isCopy())
        ins = ins->toCopy()->getOperand(0);

    MCopy *copy = new (gen->temp()) MCopy();
    if (!copy || !copy->init(gen, ins))
        return NULL;
    return copy;
}

MTest *
MTest::New(MIRGenerator *gen, MInstruction *ins, MBasicBlock *ifTrue, MBasicBlock *ifFalse)
{
    if (!ins || !ifTrue || !ifFalse)
        return NULL;
    MTest *test = new (gen->temp()) MTest(ifTrue, ifFalse);
    if (!test || !test->initOperand(gen, 0, ins))
        return NULL;
    return test;
}

MGoto *
MGoto::New(MIRGenerator *gen, MBasicBlock *target)
{
    return new (gen->temp()) MGoto(target);
}

MPhi *
MPhi::New(MIRGenerator *gen)
{
    return new (gen->temp()) MPhi(gen->cx);
}

bool
MPhi::addInput(MIRGenerator *gen, MInstruction *ins)
{
    for (size_t i = 0; i < inputs_.length(); i++) {
        if (getOperand(i) == ins)
            return true;
    }

    MOperand *operand = MOperand::New(gen, this, inputs_.length(), ins);
    if (!operand)
        return false;
    ins->linkUse(operand);

    return inputs_.append(operand);
}

MReturn *
MReturn::New(MIRGenerator *gen, MInstruction *ins)
{
    MReturn *ret = new (gen->temp()) MReturn();
    if (!ret || !ret->initOperand(gen, 0, ins))
        return NULL;
    return ret;
}

MBitAnd *
MBitAnd::New(MIRGenerator *gen, MInstruction *left, MInstruction *right)
{
    MBitAnd *ins = new (gen->temp()) MBitAnd();
    if (!ins || !ins->init(gen, left, right))
        return NULL;
    return ins;
}


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
 *   David Anderson <dvander@alliedmods.net>
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

#include "TypePolicy.h"
#include "MIR.h"
#include "MIRGraph.h"

using namespace js;
using namespace js::ion;

void
TypeAnalysis::preferType(MDefinition *def, MIRType type)
{
    if (def->type() != MIRType_Value)
        return;
    addPreferredType(def, type);
}

bool
BoxInputsPolicy::respecialize(MInstruction *ins)
{
    return false;
}

void
BoxInputsPolicy::specializeInputs(MInstruction *ins, TypeAnalysis *analysis)
{
}

bool
BoxInputsPolicy::adjustInputs(MInstruction *ins)
{
    for (size_t i = 0; i < ins->numOperands(); i++) {
        MDefinition *in = ins->getOperand(i);
        if (in->type() == MIRType_Value)
            continue;
        MBox *box = MBox::New(in);
        ins->block()->insertBefore(ins, box);
        ins->replaceOperand(i, box);
    }
    return true;
}

bool
BinaryArithPolicy::respecialize(MInstruction *ins)
{
    // If this operation is not specialized, we don't care.
    if (specialization_ == MIRType_None)
        return false;

    MDefinition *lhs = ins->getOperand(0);
    MDefinition *rhs = ins->getOperand(1);

    MIRType oldType = ins->type();

    // Check if any input would coerce to a double.
    if (CoercesToDouble(lhs->type()) || CoercesToDouble(rhs->type())) {
        if (ins->type() != MIRType_Double) {
            specialization_ = MIRType_Double;
            ins->setResultType(specialization_);
        }
    }

    return oldType != ins->type();
}

void
BinaryArithPolicy::specializeInputs(MInstruction *ins, TypeAnalysis *analysis)
{
    if (specialization_ == MIRType_None)
        return;

    analysis->preferType(ins->getOperand(0), ins->type());
    analysis->preferType(ins->getOperand(1), ins->type());
}

bool
BinaryArithPolicy::adjustInputs(MInstruction *ins)
{
    if (specialization_ == MIRType_None)
        return BoxInputsPolicy::adjustInputs(ins);

    JS_ASSERT(ins->type() == MIRType_Double || ins->type() == MIRType_Int32);

    for (size_t i = 0; i < 2; i++) {
        MDefinition *in = ins->getOperand(i);
        if (in->type() == ins->type())
            continue;

        MInstruction *replace;
        if (in->type() == MIRType_Value && specialization_ != MIRType_Any)
            replace = MUnbox::New(in, ins->type());
        else if (ins->type() == MIRType_Double)
            replace = MToDouble::New(in);
        else
            replace = MToInt32::New(in);

        ins->block()->insertBefore(ins, replace);
        ins->replaceOperand(i, replace);
    }

    return true;
}

bool
BitwisePolicy::respecialize(MInstruction *ins)
{
    return false;
}

void
BitwisePolicy::specializeInputs(MInstruction *ins, TypeAnalysis *analysis)
{
    if (specialization_ == MIRType_None)
        return;

    analysis->preferType(ins->getOperand(0), MIRType_Int32);
    analysis->preferType(ins->getOperand(1), MIRType_Int32);
}

bool
BitwisePolicy::adjustInputs(MInstruction *ins)
{
    if (specialization_ == MIRType_None)
        return BoxInputsPolicy::adjustInputs(ins);

    // This policy works for both unary and binary bitwise operations.
    for (size_t i = 0; i < ins->numOperands(); i++) {
        MDefinition *in = ins->getOperand(i);
        if (in->type() == ins->type())
            continue;

        MInstruction *replace;
        if (in->type() == MIRType_Value && specialization_ != MIRType_Any)
            replace = MUnbox::New(in, MIRType_Int32);
        else
            replace = MTruncateToInt32::New(in);

        ins->block()->insertBefore(ins, replace);
        ins->replaceOperand(i, replace);
    }

    return true;
}

bool
TableSwitchPolicy::respecialize(MInstruction *ins)
{
    // Has no outputs
    return false;
}

void
TableSwitchPolicy::specializeInputs(MInstruction *ins, TypeAnalysis *analysis)
{
    // We try to ask for the type Int32,
    // because this gives us the best code
    analysis->preferType(ins->getOperand(0), MIRType_Int32);
}

bool
TableSwitchPolicy::adjustInputs(MInstruction *ins)
{
    MDefinition *in = ins->getOperand(0);
    MInstruction *replace;

    // Tableswitch can consume all types, except:
    // - Double: try to convert to int32
    // - Value: unbox to int32
    switch (in->type()) {
        case MIRType_Value:
            replace = MUnbox::New(in, MIRType_Int32);
            break;
        case MIRType_Double:
            replace = MToInt32::New(in);
            break;
        default:
            return true;
    }
    
    ins->block()->insertBefore(ins, replace);
    ins->replaceOperand(0, replace);

    return true;
}

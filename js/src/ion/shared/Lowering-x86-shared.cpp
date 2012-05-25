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

#include "ion/MIR.h"
#include "ion/Lowering.h"
#include "ion/shared/Lowering-x86-shared.h"
#include "ion/shared/Lowering-shared-inl.h"
using namespace js;
using namespace js::ion;

bool
LIRGeneratorX86Shared::visitTableSwitch(MTableSwitch *tableswitch)
{
    MDefinition *opd = tableswitch->getOperand(0);

    // There should be at least 1 successor. The default case!
    JS_ASSERT(tableswitch->numSuccessors() > 0);

    // If there are no cases, the default case is always taken.
    if (tableswitch->numSuccessors() == 1)
        return add(new LGoto(tableswitch->getDefault()));

    // Case indices are numeric, so other types will always go to the default case.
    if (opd->type() != MIRType_Int32 && opd->type() != MIRType_Double)
        return add(new LGoto(tableswitch->getDefault()));

    // Return an LTableSwitch, capable of handling either an integer or
    // floating-point index.
    LAllocation index;
    LDefinition tempInt;
    if (opd->type() == MIRType_Int32) {
        index = useRegisterAtStart(opd);
        tempInt = tempCopy(opd, 0);
    } else {
        index = useRegister(opd);
        tempInt = temp(LDefinition::GENERAL);
    }
    return add(new LTableSwitch(index, tempInt, temp(LDefinition::GENERAL), tableswitch));
}

bool
LIRGeneratorX86Shared::visitRecompileCheck(MRecompileCheck *ins)
{
    LRecompileCheck *lir = new LRecompileCheck();
    return assignSnapshot(lir, Bailout_RecompileCheck) && add(lir);
}

bool
LIRGeneratorX86Shared::visitInterruptCheck(MInterruptCheck *ins)
{
    LInterruptCheck *lir = new LInterruptCheck();
    if (!add(lir))
        return false;
    if (!assignSafepoint(lir, ins))
        return false;
    return true;
}

bool
LIRGeneratorX86Shared::lowerMulI(MMul *mul, MDefinition *lhs, MDefinition *rhs)
{
    // Note: lhs is used twice, so that we can restore the original value for the
    // negative zero check.
    LMulI *lir = new LMulI(useRegisterAtStart(lhs), useOrConstant(rhs), useRegister(lhs));
    if (mul->fallible() && !assignSnapshot(lir))
        return false;
    return defineReuseInput(lir, mul, 0);
}

bool
LIRGeneratorX86Shared::lowerModI(MMod *mod)
{
    if (mod->rhs()->isConstant()) {
        int32 rhs = mod->rhs()->toConstant()->value().toInt32();
        int32 shift;
        JS_FLOOR_LOG2(shift, rhs);
        if (1 << shift == rhs) {
            LModPowTwoI *lir = new LModPowTwoI(useRegisterAtStart(mod->lhs()), shift);
            return assignSnapshot(lir) && defineReuseInput(lir, mod, 0);
        }
    }
    LModI *lir = new LModI(useFixed(mod->lhs(), eax), useRegister(mod->rhs()));
    return assignSnapshot(lir) && defineFixed(lir, mod, LAllocation(AnyRegister(edx)));
}

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
    return assignSnapshot(lir, Bailout_RecompileCheck) && add(lir, ins);
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
LIRGeneratorX86Shared::visitGuardShape(MGuardShape *ins)
{
    LGuardShape *guard = new LGuardShape(useRegister(ins->obj()));
    return assignSnapshot(guard, Bailout_Invalidate) && add(guard, ins);
}

bool
LIRGeneratorX86Shared::visitPowHalf(MPowHalf *ins)
{
    MDefinition *input = ins->input();
    JS_ASSERT(input->type() == MIRType_Double);
    LPowHalfD *lir = new LPowHalfD(useRegisterAtStart(input), temp());
    return defineReuseInput(lir, ins, 0);
}

bool
LIRGeneratorX86Shared::lowerMulI(MMul *mul, MDefinition *lhs, MDefinition *rhs)
{
    // Note: lhs is used twice, so that we can restore the original value for the
    // negative zero check.
    LMulI *lir = new LMulI(useRegisterAtStart(lhs), useOrConstant(rhs), use(lhs));
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

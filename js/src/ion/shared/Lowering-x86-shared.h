/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_ion_lowering_x86_shared_h__
#define jsion_ion_lowering_x86_shared_h__

#include "ion/shared/Lowering-shared.h"

namespace js {
namespace ion {

class LIRGeneratorX86Shared : public LIRGeneratorShared
{
  protected:
    LIRGeneratorX86Shared(MIRGenerator *gen, MIRGraph &graph, LIRGraph &lirGraph)
      : LIRGeneratorShared(gen, graph, lirGraph)
    {}

    LTableSwitch *newLTableSwitch(const LAllocation &in, const LDefinition &inputCopy,
                                  MTableSwitch *ins);
    LTableSwitchV *newLTableSwitchV(MTableSwitch *ins);

    bool visitInterruptCheck(MInterruptCheck *ins);
    bool visitGuardShapeOrType(MGuardShapeOrType *ins);
    bool visitPowHalf(MPowHalf *ins);
    bool visitConstant(MConstant *ins);
    bool visitAsmJSNeg(MAsmJSNeg *ins);
    bool visitAsmJSUDiv(MAsmJSUDiv *ins);
    bool visitAsmJSUMod(MAsmJSUMod *ins);
    bool lowerMulI(MMul *mul, MDefinition *lhs, MDefinition *rhs);
    bool lowerDivI(MDiv *div);
    bool lowerModI(MMod *mod);
    bool lowerUrshD(MUrsh *mir);
    bool lowerConstantDouble(double d, MInstruction *ins);
};

} // namespace ion
} // namespace js

#endif // jsion_ion_lowering_x86_shared_h__

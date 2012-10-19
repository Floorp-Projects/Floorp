/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
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

    bool visitTableSwitch(MTableSwitch *tableswitch);
    bool visitRecompileCheck(MRecompileCheck *ins);
    bool visitInterruptCheck(MInterruptCheck *ins);
    bool visitGuardShape(MGuardShape *ins);
    bool visitPowHalf(MPowHalf *ins);
    bool lowerMulI(MMul *mul, MDefinition *lhs, MDefinition *rhs);
    bool lowerModI(MMod *mod);
    bool lowerUrshD(MUrsh *mir);
};

} // namespace ion
} // namespace js

#endif // jsion_ion_lowering_x86_shared_h__

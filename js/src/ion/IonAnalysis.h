/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_ion_analysis_h__
#define jsion_ion_analysis_h__

// This file declares various analysis passes that operate on MIR.

#include "IonAllocPolicy.h"

namespace js {
namespace ion {

class MIRGenerator;
class MIRGraph;

bool
SplitCriticalEdges(MIRGenerator *gen, MIRGraph &graph);

bool
EliminatePhis(MIRGraph &graph);

bool
EliminateDeadCode(MIRGraph &graph);

bool
ApplyTypeInformation(MIRGraph &graph);

bool
RenumberBlocks(MIRGraph &graph);

bool
BuildDominatorTree(MIRGraph &graph);

bool
BuildPhiReverseMapping(MIRGraph &graph);

void
AssertGraphCoherency(MIRGraph &graph);

bool
EliminateRedundantBoundsChecks(MIRGraph &graph);

// Linear sum of term(s). For now the only linear sums which can be represented
// are 'n' or 'x + n' (for any computation x).
class MDefinition;

struct LinearSum
{
    MDefinition *term;
    int32 constant;

    LinearSum(MDefinition *term, int32 constant)
        : term(term), constant(constant)
    {}
};

LinearSum
ExtractLinearSum(MDefinition *ins);

} // namespace ion
} // namespace js

#endif // jsion_ion_analysis_h__


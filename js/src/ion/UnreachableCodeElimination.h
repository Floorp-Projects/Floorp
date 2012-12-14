/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_unreachable_code_elimination_h__
#define jsion_unreachable_code_elimination_h__

#include "MIR.h"
#include "MIRGraph.h"

namespace js {
namespace ion {

class MIRGraph;

class UnreachableCodeElimination
{
    MIRGenerator *mir_;
    MIRGraph &graph_;
    uint32_t marked_;
    bool redundantPhis_;

    bool prunePointlessBranchesAndMarkReachableBlocks();
    void removeUsesFromUnmarkedBlocks(MDefinition *instr);
    bool removeUnmarkedBlocksAndClearDominators();

  public:
    UnreachableCodeElimination(MIRGenerator *mir, MIRGraph &graph)
      : mir_(mir),
        graph_(graph),
        marked_(0),
        redundantPhis_(false)
    {}

    bool analyze();
};

} /* namespace ion */
} /* namespace js */

#endif

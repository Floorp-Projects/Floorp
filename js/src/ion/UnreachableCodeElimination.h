/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
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
    bool rerunAliasAnalysis_;

    bool prunePointlessBranchesAndMarkReachableBlocks();
    void checkDependencyAndRemoveUsesFromUnmarkedBlocks(MDefinition *instr);
    bool removeUnmarkedBlocksAndClearDominators();
    bool removeUnmarkedBlocksAndCleanup();

  public:
    UnreachableCodeElimination(MIRGenerator *mir, MIRGraph &graph)
      : mir_(mir),
        graph_(graph),
        marked_(0),
        redundantPhis_(false),
        rerunAliasAnalysis_(false)
    {}

    // Walks the graph and discovers what is reachable. Removes everything else.
    bool analyze();

    // Removes any blocks that are not marked.  Assumes that these blocks are not
    // reachable.  The parameter |marked| should be the number of blocks that
    // are marked.
    bool removeUnmarkedBlocks(size_t marked);
};

} /* namespace ion */
} /* namespace js */

#endif

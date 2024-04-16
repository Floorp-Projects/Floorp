/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/BranchHinting.h"

#include "jit/IonAnalysis.h"
#include "jit/JitSpewer.h"
#include "jit/MIRGenerator.h"
#include "jit/MIRGraph.h"

using namespace js;
using namespace js::jit;

// Implementation of the branch hinting proposal
// Some control instructions (if and br_if) can have a hint of the form
// Likely or unlikely. That means a specific branch will be likely/unlikely
// to be executed at runtime.

// In a first pass, we tag the basic blocks if we have a hint.
// In a Mir to Mir transformation, we read the hints and do something with it:
// - Unlikely blocks are pushed to the end of the function.
// Because of Ion's structure, we don't do that for blocks inside a loop.
// - TODO: do something for likely blocks.
// - TODO: register allocator can be tuned depending on the hints.
bool jit::BranchHinting(MIRGenerator* mir, MIRGraph& graph) {
  JitSpew(JitSpew_BranchHint, "Beginning BranchHinting pass");

  // Push to end all blocks marked as unlikely
  mozilla::Vector<MBasicBlock*, 0> toBeMoved;

  for (MBasicBlock* block : graph) {
    // Collect the unlikely blocks
    // Several possibilities:
    // - The unlikely block is in the middle of a loop: keep it there to not
    // break Ion assumptions.
    // - The unlikely block is not in a loop: push it to the end of the
    // function.
    if (block->branchHintingUnlikely() && block->loopDepth() == 0) {
      if (!toBeMoved.append(block)) {
        return false;
      }
    }
  }

  for (MBasicBlock* block : toBeMoved) {
#ifdef JS_JITSPEW
    JitSpew(JitSpew_BranchHint, "Pushing block%u to the end", block->id());
#endif
    graph.moveBlockToEnd(block);
  }

  if (!toBeMoved.empty()) {
    // Renumber blocks after moving them around.
    RenumberBlocks(graph);
  }

  return true;
}

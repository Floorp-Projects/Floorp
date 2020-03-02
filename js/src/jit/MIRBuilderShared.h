/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_MIRBuilderShared_h
#define jit_MIRBuilderShared_h

#include "mozilla/Maybe.h"

#include "ds/InlineTable.h"
#include "jit/MIRGraph.h"
#include "js/Vector.h"
#include "vm/Opcodes.h"

// Data structures and helper functions used by both IonBuilder and WarpBuilder.

namespace js {
namespace jit {

// PendingEdge is used whenever a block is terminated with a forward branch in
// the bytecode. When we reach the jump target we use this information to link
// the block to the jump target's block.
class PendingEdge {
 public:
  enum class Kind : uint8_t {
    // MTest true-successor.
    TestTrue,

    // MTest false-successor.
    TestFalse,

    // MGoto successor.
    Goto,

    // MGotoWithFake second successor.
    GotoWithFake,
  };

 private:
  MBasicBlock* block_;
  Kind kind_;
  JSOp testOp_ = JSOp::Undefined;

  PendingEdge(MBasicBlock* block, Kind kind, JSOp testOp = JSOp::Undefined)
      : block_(block), kind_(kind), testOp_(testOp) {}

 public:
  static PendingEdge NewTestTrue(MBasicBlock* block, JSOp op) {
    return PendingEdge(block, Kind::TestTrue, op);
  }
  static PendingEdge NewTestFalse(MBasicBlock* block, JSOp op) {
    return PendingEdge(block, Kind::TestFalse, op);
  }
  static PendingEdge NewGoto(MBasicBlock* block) {
    return PendingEdge(block, Kind::Goto);
  }
  static PendingEdge NewGotoWithFake(MBasicBlock* block) {
    return PendingEdge(block, Kind::GotoWithFake);
  }

  MBasicBlock* block() const { return block_; }
  Kind kind() const { return kind_; }

  JSOp testOp() const {
    MOZ_ASSERT(kind_ == Kind::TestTrue || kind_ == Kind::TestFalse);
    return testOp_;
  }
};

// Returns true iff the MTest added for |op| has a true-target corresponding
// with the join point in the bytecode.
inline bool TestTrueTargetIsJoinPoint(JSOp op) {
  switch (op) {
    case JSOp::IfNe:
    case JSOp::Or:
    case JSOp::Case:
      return true;

    case JSOp::IfEq:
    case JSOp::And:
    case JSOp::Coalesce:
      return false;

    default:
      MOZ_CRASH("Unexpected op");
  }
}

// PendingEdgesMap maps a bytecode instruction to a Vector of PendingEdges
// targeting it. We use InlineMap<> for this because most of the time there are
// only a few pending edges but there can be many when switch-statements are
// involved.
using PendingEdges = Vector<PendingEdge, 2, SystemAllocPolicy>;
using PendingEdgesMap =
    InlineMap<jsbytecode*, PendingEdges, 8, PointerHasher<jsbytecode*>,
              SystemAllocPolicy>;

// LoopState stores information about a loop that's being compiled to MIR.
class LoopState {
  MBasicBlock* header_ = nullptr;

 public:
  explicit LoopState(MBasicBlock* header) : header_(header) {}

  MBasicBlock* header() const { return header_; }
};
using LoopStateStack = Vector<LoopState, 4, JitAllocPolicy>;

}  // namespace jit
}  // namespace js

#endif /* jit_MIRBuilderShared_h */

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_MIRBuilderShared_h
#define jit_MIRBuilderShared_h

#include "mozilla/Maybe.h"

#include "ds/InlineTable.h"
#include "jit/MIRGenerator.h"
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

// Helper class to manage call state.
class MOZ_STACK_CLASS CallInfo {
  MDefinition* callee_;
  MDefinition* thisArg_;
  MDefinition* newTargetArg_;
  MDefinitionVector args_;
  // If non-empty, this corresponds to the stack prior any implicit inlining
  // such as before JSOp::FunApply.
  MDefinitionVector priorArgs_;

  bool constructing_;

  // True if the caller does not use the return value.
  bool ignoresReturnValue_;

  bool setter_;
  bool apply_;

 public:
  CallInfo(TempAllocator& alloc, jsbytecode* pc, bool constructing,
           bool ignoresReturnValue)
      : callee_(nullptr),
        thisArg_(nullptr),
        newTargetArg_(nullptr),
        args_(alloc),
        priorArgs_(alloc),
        constructing_(constructing),
        ignoresReturnValue_(ignoresReturnValue),
        setter_(false),
        apply_(JSOp(*pc) == JSOp::FunApply) {}

  MOZ_MUST_USE bool init(CallInfo& callInfo) {
    MOZ_ASSERT(constructing_ == callInfo.constructing());

    callee_ = callInfo.callee();
    thisArg_ = callInfo.thisArg();
    ignoresReturnValue_ = callInfo.ignoresReturnValue();

    if (constructing()) {
      newTargetArg_ = callInfo.getNewTarget();
    }

    if (!args_.appendAll(callInfo.argv())) {
      return false;
    }

    return true;
  }

  MOZ_MUST_USE bool init(MBasicBlock* current, uint32_t argc) {
    MOZ_ASSERT(args_.empty());

    // Get the arguments in the right order
    if (!args_.reserve(argc)) {
      return false;
    }

    if (constructing()) {
      setNewTarget(current->pop());
    }

    for (int32_t i = argc; i > 0; i--) {
      args_.infallibleAppend(current->peek(-i));
    }
    current->popn(argc);

    // Get |this| and |callee|
    setThis(current->pop());
    setCallee(current->pop());

    return true;
  }

  // Before doing any pop to the stack, capture whatever flows into the
  // instruction, such that we can restore it later.
  MOZ_MUST_USE bool savePriorCallStack(MIRGenerator* mir, MBasicBlock* current,
                                       size_t peekDepth);

  void popPriorCallStack(MBasicBlock* current) {
    if (priorArgs_.empty()) {
      popCallStack(current);
    } else {
      current->popn(priorArgs_.length());
    }
  }

  MOZ_MUST_USE bool pushPriorCallStack(MIRGenerator* mir,
                                       MBasicBlock* current) {
    if (priorArgs_.empty()) {
      return pushCallStack(mir, current);
    }
    for (MDefinition* def : priorArgs_) {
      current->push(def);
    }
    return true;
  }

  void popCallStack(MBasicBlock* current) { current->popn(numFormals()); }

  MOZ_MUST_USE bool pushCallStack(MIRGenerator* mir, MBasicBlock* current) {
    // Ensure sufficient space in the slots: needed for inlining from FunApply.
    if (apply_) {
      uint32_t depth = current->stackDepth() + numFormals();
      if (depth > current->nslots()) {
        if (!current->increaseSlots(depth - current->nslots())) {
          return false;
        }
      }
    }

    current->push(callee());
    current->push(thisArg());

    for (uint32_t i = 0; i < argc(); i++) {
      current->push(getArg(i));
    }

    if (constructing()) {
      current->push(getNewTarget());
    }

    return true;
  }

  uint32_t argc() const { return args_.length(); }
  uint32_t numFormals() const { return argc() + 2 + constructing(); }

  MOZ_MUST_USE bool setArgs(const MDefinitionVector& args) {
    MOZ_ASSERT(args_.empty());
    return args_.appendAll(args);
  }

  MDefinitionVector& argv() { return args_; }

  const MDefinitionVector& argv() const { return args_; }

  MDefinition* getArg(uint32_t i) const {
    MOZ_ASSERT(i < argc());
    return args_[i];
  }

  MDefinition* getArgWithDefault(uint32_t i, MDefinition* defaultValue) const {
    if (i < argc()) {
      return args_[i];
    }

    return defaultValue;
  }

  void setArg(uint32_t i, MDefinition* def) {
    MOZ_ASSERT(i < argc());
    args_[i] = def;
  }

  void removeArg(uint32_t i) { args_.erase(&args_[i]); }

  MDefinition* thisArg() const {
    MOZ_ASSERT(thisArg_);
    return thisArg_;
  }

  void setThis(MDefinition* thisArg) { thisArg_ = thisArg; }

  bool constructing() const { return constructing_; }

  bool ignoresReturnValue() const { return ignoresReturnValue_; }

  void setNewTarget(MDefinition* newTarget) {
    MOZ_ASSERT(constructing());
    newTargetArg_ = newTarget;
  }
  MDefinition* getNewTarget() const {
    MOZ_ASSERT(newTargetArg_);
    return newTargetArg_;
  }

  bool isSetter() const { return setter_; }
  void markAsSetter() { setter_ = true; }

  MDefinition* callee() const {
    MOZ_ASSERT(callee_);
    return callee_;
  }

  void setCallee(MDefinition* callee) { callee_ = callee; }

  template <typename Fun>
  void forEachCallOperand(Fun& f) {
    f(callee_);
    f(thisArg_);
    if (newTargetArg_) {
      f(newTargetArg_);
    }
    for (uint32_t i = 0; i < argc(); i++) {
      f(getArg(i));
    }
  }

  void setImplicitlyUsedUnchecked() {
    auto setFlag = [](MDefinition* def) { def->setImplicitlyUsedUnchecked(); };
    forEachCallOperand(setFlag);
  }
};

}  // namespace jit
}  // namespace js

#endif /* jit_MIRBuilderShared_h */

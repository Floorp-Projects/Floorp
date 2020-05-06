/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_WarpBuilder_h
#define jit_WarpBuilder_h

#include <initializer_list>

#include "jit/JitContext.h"
#include "jit/MIR.h"
#include "jit/MIRBuilderShared.h"
#include "jit/WarpSnapshot.h"
#include "vm/Opcodes.h"

namespace js {
namespace jit {

// JSOps not yet supported by WarpBuilder. See warning at the end of the list.
#define WARP_UNSUPPORTED_OPCODE_LIST(_)  \
  /* Intentionally not implemented */    \
  _(ForceInterpreter)                    \
  /* With */                             \
  _(EnterWith)                           \
  _(LeaveWith)                           \
  /* Eval */                             \
  _(Eval)                                \
  _(StrictEval)                          \
  _(SpreadEval)                          \
  _(StrictSpreadEval)                    \
  /* Super */                            \
  _(SetPropSuper)                        \
  _(SetElemSuper)                        \
  _(StrictSetPropSuper)                  \
  _(StrictSetElemSuper)                  \
  /* Environments (bug 1366470) */       \
  _(PushVarEnv)                          \
  /* Compound assignment */              \
  _(GetBoundName)                        \
  /* Generators / Async (bug 1317690) */ \
  _(IsGenClosing)                        \
  _(InitialYield)                        \
  _(Yield)                               \
  _(FinalYieldRval)                      \
  _(Resume)                              \
  _(ResumeKind)                          \
  _(CheckResumeKind)                     \
  _(AfterYield)                          \
  _(Await)                               \
  _(TrySkipAwait)                        \
  _(Generator)                           \
  _(AsyncAwait)                          \
  _(AsyncResolve)                        \
  /* Catch/finally */                    \
  _(Exception)                           \
  _(Finally)                             \
  _(Gosub)                               \
  _(Retsub)                              \
  /* Misc */                             \
  _(DelName)                             \
  _(GetRval)                             \
  _(SetIntrinsic)                        \
  _(ThrowMsg)
// === !! WARNING WARNING WARNING !! ===
// Do you really want to sacrifice performance by not implementing this
// operation in the optimizing compiler?

class MIRGenerator;
class MIRGraph;
class WarpSnapshot;

// WarpBuilder builds a MIR graph from WarpSnapshot. Unlike WarpOracle,
// WarpBuilder can run off-thread.
class MOZ_STACK_CLASS WarpBuilder {
  WarpSnapshot& snapshot_;
  MIRGenerator& mirGen_;
  MIRGraph& graph_;
  TempAllocator& alloc_;
  const CompileInfo& info_;
  JSScript* script_;
  MBasicBlock* current = nullptr;

  // Pointer to a WarpOpSnapshot or nullptr if we reached the end of the list.
  // Because bytecode is compiled from first to last instruction (and
  // WarpOpSnapshot is sorted the same way), the iterator always moves forward.
  const WarpOpSnapshot* opSnapshotIter_ = nullptr;

  // Note: we need both loopDepth_ and loopStack_.length(): once we support
  // inlining, loopDepth_ will be moved to a per-compilation data structure
  // (OuterWarpBuilder?) whereas loopStack_ and pendingEdges_ will be
  // builder-specific state.
  uint32_t loopDepth_ = 0;
  LoopStateStack loopStack_;
  PendingEdgesMap pendingEdges_;

  // Loop phis for iterators that need to be kept alive.
  // TODO: once we support inlining, this needs to be stored once per
  // compilation instead of builder.
  PhiVector iterators_;

  TempAllocator& alloc() { return alloc_; }
  MIRGraph& graph() { return graph_; }
  const CompileInfo& info() const { return info_; }
  WarpSnapshot& snapshot() const { return snapshot_; }

  BytecodeSite* newBytecodeSite(BytecodeLocation loc);

  const WarpOpSnapshot* getOpSnapshotImpl(BytecodeLocation loc);

  template <typename T>
  const T* getOpSnapshot(BytecodeLocation loc) {
    const WarpOpSnapshot* snapshot = getOpSnapshotImpl(loc);
    return snapshot ? snapshot->as<T>() : nullptr;
  }

  void initBlock(MBasicBlock* block);
  MOZ_MUST_USE bool startNewEntryBlock(size_t stackDepth, BytecodeLocation loc);
  MOZ_MUST_USE bool startNewBlock(MBasicBlock* predecessor,
                                  BytecodeLocation loc, size_t numToPop = 0);
  MOZ_MUST_USE bool startNewLoopHeaderBlock(BytecodeLocation loopHead);
  MOZ_MUST_USE bool startNewOsrPreHeaderBlock(BytecodeLocation loopHead);

  bool hasTerminatedBlock() const { return current == nullptr; }
  void setTerminatedBlock() { current = nullptr; }

  MOZ_MUST_USE bool addPendingEdge(const PendingEdge& edge,
                                   BytecodeLocation target);
  MOZ_MUST_USE bool buildForwardGoto(BytecodeLocation target);
  MOZ_MUST_USE bool buildBackedge();
  MOZ_MUST_USE bool buildTestBackedge(BytecodeLocation loc);

  MOZ_MUST_USE bool resumeAfter(MInstruction* ins, BytecodeLocation loc);

  MOZ_MUST_USE bool addIteratorLoopPhis(BytecodeLocation loopHead);

  MConstant* constant(const Value& v);
  void pushConstant(const Value& v);

  MOZ_MUST_USE bool buildPrologue();
  MOZ_MUST_USE bool buildBody();
  MOZ_MUST_USE bool buildEpilogue();

  MOZ_MUST_USE bool buildIC(BytecodeLocation loc, CacheKind kind,
                            std::initializer_list<MDefinition*> inputs);

  MOZ_MUST_USE bool buildEnvironmentChain();
  MInstruction* buildNamedLambdaEnv(MDefinition* callee, MDefinition* env,
                                    LexicalEnvironmentObject* templateObj);
  MInstruction* buildCallObject(MDefinition* callee, MDefinition* env,
                                CallObject* templateObj);
  MInstruction* buildLoadSlot(MDefinition* obj, uint32_t numFixedSlots,
                              uint32_t slot);

  MConstant* globalLexicalEnvConstant();
  MDefinition* getCallee();

  MOZ_MUST_USE bool buildUnaryOp(BytecodeLocation loc);
  MOZ_MUST_USE bool buildBinaryOp(BytecodeLocation loc);
  MOZ_MUST_USE bool buildCompareOp(BytecodeLocation loc);
  MOZ_MUST_USE bool buildTestOp(BytecodeLocation loc);
  MOZ_MUST_USE bool buildDefLexicalOp(BytecodeLocation loc);
  MOZ_MUST_USE bool buildCallOp(BytecodeLocation loc);

  MOZ_MUST_USE bool buildInitPropGetterSetterOp(BytecodeLocation loc);
  MOZ_MUST_USE bool buildInitElemGetterSetterOp(BytecodeLocation loc);

  void buildCopyLexicalEnvOp(bool copySlots);
  void buildCheckLexicalOp(BytecodeLocation loc);

  bool usesEnvironmentChain() const;
  MDefinition* walkEnvironmentChain(uint32_t numHops);

#define BUILD_OP(OP, ...) MOZ_MUST_USE bool build_##OP(BytecodeLocation loc);
  FOR_EACH_OPCODE(BUILD_OP)
#undef BUILD_OP

 public:
  WarpBuilder(WarpSnapshot& snapshot, MIRGenerator& mirGen);

  MOZ_MUST_USE bool build();
};

}  // namespace jit
}  // namespace js

#endif /* jit_WarpBuilder_h */

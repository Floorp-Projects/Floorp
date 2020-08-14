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
#include "jit/WarpBuilderShared.h"
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
  /* try-finally */                      \
  _(Finally)                             \
  _(Gosub)                               \
  _(Retsub)                              \
  /* Misc */                             \
  _(DelName)                             \
  _(GetRval)                             \
  _(SetIntrinsic)                        \
  _(ThrowMsg)                            \
  /* Private Fields */                   \
  _(InitLockedElem)                      \
  // === !! WARNING WARNING WARNING !! ===
  // Do you really want to sacrifice performance by not implementing this
  // operation in the optimizing compiler?

class MIRGenerator;
class MIRGraph;
class WarpSnapshot;

// Data that is shared across all WarpBuilders for a given compilation.
class MOZ_STACK_CLASS WarpCompilation {
  // The total loop depth, including loops in the caller while
  // compiling inlined functions.
  uint32_t loopDepth_ = 0;

  // Loop phis for iterators that need to be kept alive.
  PhiVector iterators_;

 public:
  explicit WarpCompilation(TempAllocator& alloc) : iterators_(alloc) {}

  uint32_t loopDepth() const { return loopDepth_; }
  void incLoopDepth() { loopDepth_++; }
  void decLoopDepth() {
    MOZ_ASSERT(loopDepth() > 0);
    loopDepth_--;
  }

  PhiVector* iterators() { return &iterators_; }
};

// WarpBuilder builds a MIR graph from WarpSnapshot. Unlike WarpOracle,
// WarpBuilder can run off-thread.
class MOZ_STACK_CLASS WarpBuilder : public WarpBuilderShared {
  WarpCompilation* warpCompilation_;
  MIRGraph& graph_;
  const CompileInfo& info_;
  const WarpScriptSnapshot* scriptSnapshot_;
  JSScript* script_;

  // Pointer to a WarpOpSnapshot or nullptr if we reached the end of the list.
  // Because bytecode is compiled from first to last instruction (and
  // WarpOpSnapshot is sorted the same way), the iterator always moves forward.
  const WarpOpSnapshot* opSnapshotIter_ = nullptr;

  // Note: loopStack_ is builder-specific. loopStack_.length is the
  // depth relative to the current script.  The overall loop depth is
  // stored in the WarpCompilation.
  LoopStateStack loopStack_;
  PendingEdgesMap pendingEdges_;

  // These are only initialized when building an inlined script.
  WarpBuilder* callerBuilder_ = nullptr;
  MResumePoint* callerResumePoint_ = nullptr;
  CallInfo* inlineCallInfo_ = nullptr;

  WarpCompilation* warpCompilation() const { return warpCompilation_; }
  MIRGraph& graph() { return graph_; }
  const CompileInfo& info() const { return info_; }
  const WarpScriptSnapshot* scriptSnapshot() const { return scriptSnapshot_; }

  uint32_t loopDepth() const { return warpCompilation_->loopDepth(); }
  void incLoopDepth() { warpCompilation_->incLoopDepth(); }
  void decLoopDepth() { warpCompilation_->decLoopDepth(); }
  PhiVector* iterators() { return warpCompilation_->iterators(); }

  WarpBuilder* callerBuilder() const { return callerBuilder_; }
  MResumePoint* callerResumePoint() const { return callerResumePoint_; }

  BytecodeSite* newBytecodeSite(BytecodeLocation loc);

  const WarpOpSnapshot* getOpSnapshotImpl(BytecodeLocation loc,
                                          WarpOpSnapshot::Kind kind);

  template <typename T>
  const T* getOpSnapshot(BytecodeLocation loc) {
    const WarpOpSnapshot* snapshot = getOpSnapshotImpl(loc, T::ThisKind);
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

  MOZ_MUST_USE bool addIteratorLoopPhis(BytecodeLocation loopHead);

  MOZ_MUST_USE bool buildPrologue();
  MOZ_MUST_USE bool buildBody();

  MOZ_MUST_USE bool buildInlinePrologue();

  MOZ_MUST_USE bool buildIC(BytecodeLocation loc, CacheKind kind,
                            std::initializer_list<MDefinition*> inputs);
  MOZ_MUST_USE bool buildBailoutForColdIC(BytecodeLocation loc, CacheKind kind);

  MOZ_MUST_USE bool buildEnvironmentChain();
  MInstruction* buildNamedLambdaEnv(MDefinition* callee, MDefinition* env,
                                    LexicalEnvironmentObject* templateObj);
  MInstruction* buildCallObject(MDefinition* callee, MDefinition* env,
                                CallObject* templateObj);
  MInstruction* buildLoadSlot(MDefinition* obj, uint32_t numFixedSlots,
                              uint32_t slot);

  MConstant* globalLexicalEnvConstant();
  MDefinition* getCallee();

  MDefinition* maybeGuardNotOptimizedArguments(MDefinition* def);

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

  MOZ_MUST_USE bool buildInlinedCall(BytecodeLocation loc,
                                     const WarpInlinedCall* snapshot,
                                     CallInfo& callInfo);

  MDefinition* patchInlinedReturns(CallInfo& callInfo, MIRGraphReturns& exits,
                                   MBasicBlock* returnBlock);
  MDefinition* patchInlinedReturn(CallInfo& callInfo, MBasicBlock* exit,
                                  MBasicBlock* returnBlock);

#define BUILD_OP(OP, ...) MOZ_MUST_USE bool build_##OP(BytecodeLocation loc);
  FOR_EACH_OPCODE(BUILD_OP)
#undef BUILD_OP

 public:
  WarpBuilder(WarpSnapshot& snapshot, MIRGenerator& mirGen,
              WarpCompilation* warpCompilation);
  WarpBuilder(WarpBuilder* caller, WarpScriptSnapshot* snapshot,
              CompileInfo& compileInfo, CallInfo* inlineCallInfo,
              MResumePoint* callerResumePoint);

  MOZ_MUST_USE bool build();
  MOZ_MUST_USE bool buildInline();

  CallInfo* inlineCallInfo() const { return inlineCallInfo_; }
};

}  // namespace jit
}  // namespace js

#endif /* jit_WarpBuilder_h */

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_WarpBuilder_h
#define jit_WarpBuilder_h

#include "jit/JitContext.h"
#include "jit/MIR.h"
#include "jit/MIRBuilderShared.h"

namespace js {
namespace jit {

// JS bytecode ops supported by WarpBuilder. Once we support most opcodes
// this should be replaced with the FOR_EACH_OPCODE macro.
#define WARP_OPCODE_LIST(_) \
  _(Nop)                    \
  _(NopDestructuring)       \
  _(TryDestructuring)       \
  _(Lineno)                 \
  _(DebugLeaveLexicalEnv)   \
  _(Undefined)              \
  _(Void)                   \
  _(Null)                   \
  _(Hole)                   \
  _(Uninitialized)          \
  _(IsConstructing)         \
  _(False)                  \
  _(True)                   \
  _(Zero)                   \
  _(One)                    \
  _(Int8)                   \
  _(Uint16)                 \
  _(Uint24)                 \
  _(Int32)                  \
  _(Double)                 \
  _(ResumeIndex)            \
  _(Pop)                    \
  _(PopN)                   \
  _(Dup)                    \
  _(Dup2)                   \
  _(DupAt)                  \
  _(Swap)                   \
  _(Pick)                   \
  _(Unpick)                 \
  _(GetLocal)               \
  _(SetLocal)               \
  _(InitLexical)            \
  _(GetArg)                 \
  _(SetArg)                 \
  _(ToNumeric)              \
  _(Inc)                    \
  _(Dec)                    \
  _(Neg)                    \
  _(BitNot)                 \
  _(Add)                    \
  _(Sub)                    \
  _(Mul)                    \
  _(Div)                    \
  _(Mod)                    \
  _(BitAnd)                 \
  _(BitOr)                  \
  _(BitXor)                 \
  _(Lsh)                    \
  _(Rsh)                    \
  _(Ursh)                   \
  _(Eq)                     \
  _(Ne)                     \
  _(Lt)                     \
  _(Le)                     \
  _(Gt)                     \
  _(Ge)                     \
  _(StrictEq)               \
  _(StrictNe)               \
  _(JumpTarget)             \
  _(LoopHead)               \
  _(IfEq)                   \
  _(IfNe)                   \
  _(And)                    \
  _(Or)                     \
  _(Case)                   \
  _(Default)                \
  _(Coalesce)               \
  _(Goto)                   \
  _(Return)                 \
  _(RetRval)

class MIRGenerator;
class MIRGraph;
class WarpSnapshot;

// WarpBuilder builds a MIR graph from WarpSnapshot. Unlike WarpOracle,
// WarpBuilder can run off-thread.
class MOZ_STACK_CLASS WarpBuilder {
  WarpSnapshot& input_;
  MIRGenerator& mirGen_;
  MIRGraph& graph_;
  TempAllocator& alloc_;
  const CompileInfo& info_;
  JSScript* script_;
  MBasicBlock* current = nullptr;

  // Note: we need both loopDepth_ and loopStack_.length(): once we support
  // inlining, loopDepth_ will be moved to a per-compilation data structure
  // (OuterWarpBuilder?) whereas loopStack_ and pendingEdges_ will be
  // builder-specific state.
  uint32_t loopDepth_ = 0;
  LoopStateStack loopStack_;
  PendingEdgesMap pendingEdges_;

  TempAllocator& alloc() { return alloc_; }
  MIRGraph& graph() { return graph_; }
  const CompileInfo& info() const { return info_; }
  WarpSnapshot& input() const { return input_; }

  BytecodeSite* newBytecodeSite(BytecodeLocation loc);

  void initBlock(MBasicBlock* block);
  MOZ_MUST_USE bool startNewEntryBlock(size_t stackDepth, BytecodeLocation loc);
  MOZ_MUST_USE bool startNewBlock(MBasicBlock* predecessor,
                                  BytecodeLocation loc, size_t numToPop = 0);
  MOZ_MUST_USE bool startNewLoopHeaderBlock(MBasicBlock* predecessor,
                                            BytecodeLocation loc);

  bool hasTerminatedBlock() const { return current == nullptr; }
  void setTerminatedBlock() { current = nullptr; }

  MOZ_MUST_USE bool addPendingEdge(const PendingEdge& edge,
                                   BytecodeLocation target);
  MOZ_MUST_USE bool buildForwardGoto(BytecodeLocation target);
  MOZ_MUST_USE bool buildBackedge();
  MOZ_MUST_USE bool buildTestBackedge(BytecodeLocation loc);

  MOZ_MUST_USE bool resumeAfter(MInstruction* ins, BytecodeLocation loc);

  MConstant* constant(const Value& v);
  void pushConstant(const Value& v);

  MOZ_MUST_USE bool buildPrologue();
  MOZ_MUST_USE bool buildBody();
  MOZ_MUST_USE bool buildEpilogue();

  MOZ_MUST_USE bool buildUnaryOp(BytecodeLocation loc);
  MOZ_MUST_USE bool buildBinaryOp(BytecodeLocation loc);
  MOZ_MUST_USE bool buildCompareOp(BytecodeLocation loc);
  MOZ_MUST_USE bool buildTestOp(BytecodeLocation loc);

#define BUILD_OP(OP) MOZ_MUST_USE bool build_##OP(BytecodeLocation loc);
  WARP_OPCODE_LIST(BUILD_OP)
#undef BUILD_OP

 public:
  WarpBuilder(WarpSnapshot& input, MIRGenerator& mirGen);

  MOZ_MUST_USE bool build();
};

}  // namespace jit
}  // namespace js

#endif /* jit_WarpBuilder_h */

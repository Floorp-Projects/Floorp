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
#include "jit/WarpOracle.h"

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
  _(BigInt)                 \
  _(String)                 \
  _(Symbol)                 \
  _(RegExp)                 \
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
  _(Pow)                    \
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
  _(DebugCheckSelfHosted)   \
  _(DynamicImport)          \
  _(Not)                    \
  _(ToString)               \
  _(DefVar)                 \
  _(DefLet)                 \
  _(DefConst)               \
  _(DefFun)                 \
  _(BindVar)                \
  _(MutateProto)            \
  _(Callee)                 \
  _(ClassConstructor)       \
  _(DerivedConstructor)     \
  _(ToAsyncIter)            \
  _(ToId)                   \
  _(Typeof)                 \
  _(TypeofExpr)             \
  _(Arguments)              \
  _(ObjWithProto)           \
  _(GetAliasedVar)          \
  _(SetAliasedVar)          \
  _(InitAliasedLexical)     \
  _(EnvCallee)              \
  _(Iter)                   \
  _(IterNext)               \
  _(MoreIter)               \
  _(EndIter)                \
  _(IsNoIter)               \
  _(Call)                   \
  _(CallIgnoresRv)          \
  _(CallIter)               \
  _(New)                    \
  _(SuperCall)              \
  _(FunctionThis)           \
  _(GlobalThis)             \
  _(GetName)                \
  _(GetGName)               \
  _(BindName)               \
  _(BindGName)              \
  _(GetProp)                \
  _(CallProp)               \
  _(Length)                 \
  _(GetElem)                \
  _(CallElem)               \
  _(SetProp)                \
  _(StrictSetProp)          \
  _(SetName)                \
  _(StrictSetName)          \
  _(SetGName)               \
  _(StrictSetGName)         \
  _(InitGLexical)           \
  _(SetElem)                \
  _(StrictSetElem)          \
  _(DelProp)                \
  _(StrictDelProp)          \
  _(DelElem)                \
  _(StrictDelElem)          \
  _(SetFunName)             \
  _(PushLexicalEnv)         \
  _(PopLexicalEnv)          \
  _(FreshenLexicalEnv)      \
  _(RecreateLexicalEnv)     \
  _(ImplicitThis)           \
  _(GImplicitThis)          \
  _(CheckClassHeritage)     \
  _(CheckThis)              \
  _(CheckThisReinit)        \
  _(CheckReturn)            \
  _(CheckLexical)           \
  _(CheckAliasedLexical)    \
  _(InitHomeObject)         \
  _(SuperBase)              \
  _(SuperFun)               \
  _(BuiltinProto)           \
  _(GetIntrinsic)           \
  _(ImportMeta)             \
  _(CallSiteObj)            \
  _(NewArray)               \
  _(NewArrayCopyOnWrite)    \
  _(NewObject)              \
  _(NewObjectWithGroup)     \
  _(NewInit)                \
  _(Object)                 \
  _(InitPropGetter)         \
  _(InitPropSetter)         \
  _(InitHiddenPropGetter)   \
  _(InitHiddenPropSetter)   \
  _(InitElemGetter)         \
  _(InitElemSetter)         \
  _(InitHiddenElemGetter)   \
  _(InitHiddenElemSetter)   \
  _(In)                     \
  _(HasOwn)                 \
  _(Instanceof)             \
  _(NewTarget)              \
  _(CheckIsObj)             \
  _(CheckIsCallable)        \
  _(CheckObjCoercible)      \
  _(GetImport)              \
  _(GetPropSuper)           \
  _(GetElemSuper)           \
  _(InitProp)               \
  _(InitLockedProp)         \
  _(InitHiddenProp)         \
  _(InitElem)               \
  _(InitHiddenElem)         \
  _(InitElemArray)          \
  _(InitElemInc)            \
  _(SetRval)                \
  _(Return)                 \
  _(RetRval)

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

  MOZ_MUST_USE bool buildGetNameOp(BytecodeLocation loc, MDefinition* env);
  MOZ_MUST_USE bool buildBindNameOp(BytecodeLocation loc, MDefinition* env);
  MOZ_MUST_USE bool buildGetPropOp(BytecodeLocation loc, MDefinition* val,
                                   MDefinition* id);
  MOZ_MUST_USE bool buildSetPropOp(BytecodeLocation loc, MDefinition* obj,
                                   MDefinition* id, MDefinition* val);
  MOZ_MUST_USE bool buildInitPropOp(BytecodeLocation loc, MDefinition* obj,
                                    MDefinition* id, MDefinition* val);
  MOZ_MUST_USE bool buildGetPropSuperOp(BytecodeLocation loc, MDefinition* obj,
                                        MDefinition* receiver, MDefinition* id);

  MOZ_MUST_USE bool buildInitPropGetterSetterOp(BytecodeLocation loc);
  MOZ_MUST_USE bool buildInitElemGetterSetterOp(BytecodeLocation loc);

  void buildCopyLexicalEnvOp(bool copySlots);
  void buildCheckLexicalOp(BytecodeLocation loc);

  bool usesEnvironmentChain() const;
  MDefinition* walkEnvironmentChain(uint32_t numHops);

#define BUILD_OP(OP) MOZ_MUST_USE bool build_##OP(BytecodeLocation loc);
  WARP_OPCODE_LIST(BUILD_OP)
#undef BUILD_OP

 public:
  WarpBuilder(WarpSnapshot& snapshot, MIRGenerator& mirGen);

  MOZ_MUST_USE bool build();
};

}  // namespace jit
}  // namespace js

#endif /* jit_WarpBuilder_h */

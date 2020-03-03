/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/WarpBuilder.h"

#include "jit/MIR.h"
#include "jit/MIRGenerator.h"
#include "jit/MIRGraph.h"
#include "jit/WarpOracle.h"

#include "jit/JitScript-inl.h"
#include "vm/BytecodeIterator-inl.h"
#include "vm/BytecodeLocation-inl.h"

using namespace js;
using namespace js::jit;

WarpBuilder::WarpBuilder(WarpSnapshot& input, MIRGenerator& mirGen)
    : input_(input),
      mirGen_(mirGen),
      graph_(mirGen.graph()),
      alloc_(mirGen.alloc()),
      info_(mirGen.outerInfo()),
      script_(input.script()->script()),
      loopStack_(alloc_) {}

MConstant* WarpBuilder::constant(const Value& v) {
  MOZ_ASSERT_IF(v.isString(), v.toString()->isAtom());
  MOZ_ASSERT_IF(v.isGCThing(), !IsInsideNursery(v.toGCThing()));

  MConstant* c = MConstant::New(alloc(), v);
  current->add(c);

  return c;
}

void WarpBuilder::pushConstant(const Value& v) {
  MConstant* c = constant(v);
  current->push(c);
}

BytecodeSite* WarpBuilder::newBytecodeSite(BytecodeLocation loc) {
  jsbytecode* pc = loc.toRawBytecode();
  MOZ_ASSERT(info().inlineScriptTree()->script()->containsPC(pc));
  return new (alloc()) BytecodeSite(info().inlineScriptTree(), pc);
}

void WarpBuilder::initBlock(MBasicBlock* block) {
  graph().addBlock(block);

  // TODO: set block hit count (for branch pruning pass)
  block->setLoopDepth(loopDepth_);

  current = block;
}

bool WarpBuilder::startNewBlock(MBasicBlock* predecessor, BytecodeLocation loc,
                                size_t numToPop) {
  MBasicBlock* block =
      MBasicBlock::NewPopN(graph(), info(), predecessor, newBytecodeSite(loc),
                           MBasicBlock::NORMAL, numToPop);
  if (!block) {
    return false;
  }

  initBlock(block);
  return true;
}

bool WarpBuilder::startNewEntryBlock(size_t stackDepth, BytecodeLocation loc) {
  MBasicBlock* block =
      MBasicBlock::New(graph(), stackDepth, info(), /* maybePred = */ nullptr,
                       newBytecodeSite(loc), MBasicBlock::NORMAL);
  if (!block) {
    return false;
  }

  initBlock(block);
  return true;
}

bool WarpBuilder::startNewLoopHeaderBlock(MBasicBlock* predecessor,
                                          BytecodeLocation loc) {
  MBasicBlock* header = MBasicBlock::NewPendingLoopHeader(
      graph(), info(), predecessor, newBytecodeSite(loc));
  if (!header) {
    return false;
  }

  initBlock(header);
  return loopStack_.emplaceBack(header);
}

bool WarpBuilder::addPendingEdge(const PendingEdge& edge,
                                 BytecodeLocation target) {
  jsbytecode* targetPC = target.toRawBytecode();
  PendingEdgesMap::AddPtr p = pendingEdges_.lookupForAdd(targetPC);
  if (p) {
    return p->value().append(edge);
  }

  PendingEdges edges;
  static_assert(PendingEdges::InlineLength >= 1,
                "Appending one element should be infallible");
  MOZ_ALWAYS_TRUE(edges.append(edge));

  return pendingEdges_.add(p, targetPC, std::move(edges));
}

bool WarpBuilder::resumeAfter(MInstruction* ins, BytecodeLocation loc) {
  MOZ_ASSERT(ins->isEffectful());

  MResumePoint* resumePoint = MResumePoint::New(
      alloc(), ins->block(), loc.toRawBytecode(), MResumePoint::ResumeAfter);
  if (!resumePoint) {
    return false;
  }

  ins->setResumePoint(resumePoint);
  return true;
}

bool WarpBuilder::build() {
  if (!buildPrologue()) {
    return false;
  }

  if (!buildBody()) {
    return false;
  }

  if (!buildEpilogue()) {
    return false;
  }

  MOZ_ASSERT(loopStack_.empty());
  MOZ_ASSERT(loopDepth_ == 0);

  return true;
}

bool WarpBuilder::buildPrologue() {
  BytecodeLocation startLoc(script_, script_->code());
  if (!startNewEntryBlock(info().firstStackSlot(), startLoc)) {
    return false;
  }

  if (info().funMaybeLazy()) {
    // Initialize |this|.
    MParameter* param =
        MParameter::New(alloc(), MParameter::THIS_SLOT, nullptr);
    current->add(param);
    current->initSlot(info().thisSlot(), param);

    // Initialize arguments.
    for (uint32_t i = 0; i < info().nargs(); i++) {
      MParameter* param = MParameter::New(alloc().fallible(), i, nullptr);
      if (!param) {
        return false;
      }
      current->add(param);
      current->initSlot(info().argSlotUnchecked(i), param);
    }
  }

  MConstant* undef = constant(UndefinedValue());

  // Initialize local slots.
  for (uint32_t i = 0; i < info().nlocals(); i++) {
    current->initSlot(info().localSlot(i), undef);
  }

  // Initialize the environment chain and return value slots.
  current->initSlot(info().environmentChainSlot(), undef);
  current->initSlot(info().returnValueSlot(), undef);

  current->add(MStart::New(alloc()));

  // TODO: environment chain initialization

  // Guard against over-recursion.
  MCheckOverRecursed* check = MCheckOverRecursed::New(alloc());
  current->add(check);

  return true;
}

bool WarpBuilder::buildBody() {
  for (BytecodeLocation loc : AllBytecodesIterable(script_)) {
    if (mirGen_.shouldCancel("WarpBuilder (opcode loop)")) {
      return false;
    }

    // Skip unreachable ops (for example code after a 'return' or 'throw') until
    // we get to the next jump target.
    if (hasTerminatedBlock()) {
      // Finish any "broken" loops with an unreachable backedge. For example:
      //
      //   do {
      //     ...
      //     return;
      //     ...
      //   } while (x);
      //
      // This loop never actually loops.
      if (loc.isBackedge() && !loopStack_.empty()) {
        BytecodeLocation loopHead(script_, loopStack_.back().header()->pc());
        if (loc.isBackedgeForLoophead(loopHead)) {
          MOZ_ASSERT(loopDepth_ > 0);
          loopDepth_--;
          loopStack_.popBack();
        }
      }
      if (!loc.isJumpTarget()) {
        continue;
      }
    }

    if (!alloc().ensureBallast()) {
      return false;
    }

    // TODO: port PoppedValueUseChecker from IonBuilder

    JSOp op = loc.getOp();

#define BUILD_OP(OP)                            \
  case JSOp::OP:                                \
    if (MOZ_UNLIKELY(!this->build_##OP(loc))) { \
      return false;                             \
    }                                           \
    break;
    switch (op) {
      WARP_OPCODE_LIST(BUILD_OP)
      default:
        // WarpOracle should have aborted compilation.
        MOZ_CRASH("Unexpected op");
    }
#undef BUILD_OP
  }

  return true;
}

bool WarpBuilder::buildEpilogue() { return true; }

bool WarpBuilder::build_Nop(BytecodeLocation) { return true; }

bool WarpBuilder::build_NopDestructuring(BytecodeLocation) { return true; }

bool WarpBuilder::build_TryDestructuring(BytecodeLocation) { return true; }

bool WarpBuilder::build_Lineno(BytecodeLocation) { return true; }

bool WarpBuilder::build_DebugLeaveLexicalEnv(BytecodeLocation) { return true; }

bool WarpBuilder::build_Undefined(BytecodeLocation) {
  // If this ever changes, change what JSOp::GImplicitThis does too.
  pushConstant(UndefinedValue());
  return true;
}

bool WarpBuilder::build_Void(BytecodeLocation) {
  current->pop();
  pushConstant(UndefinedValue());
  return true;
}

bool WarpBuilder::build_Null(BytecodeLocation) {
  pushConstant(NullValue());
  return true;
}

bool WarpBuilder::build_Hole(BytecodeLocation) {
  pushConstant(MagicValue(JS_ELEMENTS_HOLE));
  return true;
}

bool WarpBuilder::build_Uninitialized(BytecodeLocation) {
  pushConstant(MagicValue(JS_UNINITIALIZED_LEXICAL));
  return true;
}

bool WarpBuilder::build_IsConstructing(BytecodeLocation) {
  pushConstant(MagicValue(JS_IS_CONSTRUCTING));
  return true;
}

bool WarpBuilder::build_False(BytecodeLocation) {
  pushConstant(BooleanValue(false));
  return true;
}

bool WarpBuilder::build_True(BytecodeLocation) {
  pushConstant(BooleanValue(true));
  return true;
}

bool WarpBuilder::build_Pop(BytecodeLocation) {
  current->pop();
  // TODO: IonBuilder inserts a resume point in loops, re-evaluate this.
  return true;
}

bool WarpBuilder::build_PopN(BytecodeLocation loc) {
  for (uint32_t i = 0, n = loc.getPopCount(); i < n; i++) {
    current->pop();
  }
  return true;
}

bool WarpBuilder::build_Dup(BytecodeLocation) {
  current->pushSlot(current->stackDepth() - 1);
  return true;
}

bool WarpBuilder::build_Dup2(BytecodeLocation) {
  uint32_t lhsSlot = current->stackDepth() - 2;
  uint32_t rhsSlot = current->stackDepth() - 1;
  current->pushSlot(lhsSlot);
  current->pushSlot(rhsSlot);
  return true;
}

bool WarpBuilder::build_DupAt(BytecodeLocation loc) {
  current->pushSlot(current->stackDepth() - 1 - loc.getDupAtIndex());
  return true;
}

bool WarpBuilder::build_Swap(BytecodeLocation) {
  current->swapAt(-1);
  return true;
}

bool WarpBuilder::build_Pick(BytecodeLocation loc) {
  int32_t depth = -int32_t(loc.getPickDepth());
  current->pick(depth);
  return true;
}

bool WarpBuilder::build_Unpick(BytecodeLocation loc) {
  int32_t depth = -int32_t(loc.getUnpickDepth());
  current->unpick(depth);
  return true;
}

bool WarpBuilder::build_Zero(BytecodeLocation) {
  pushConstant(Int32Value(0));
  return true;
}

bool WarpBuilder::build_One(BytecodeLocation) {
  pushConstant(Int32Value(1));
  return true;
}

bool WarpBuilder::build_Int8(BytecodeLocation loc) {
  pushConstant(Int32Value(loc.getInt8()));
  return true;
}

bool WarpBuilder::build_Uint16(BytecodeLocation loc) {
  pushConstant(Int32Value(loc.getUint16()));
  return true;
}

bool WarpBuilder::build_Uint24(BytecodeLocation loc) {
  pushConstant(Int32Value(loc.getUint24()));
  return true;
}

bool WarpBuilder::build_Int32(BytecodeLocation loc) {
  pushConstant(Int32Value(loc.getInt32()));
  return true;
}

bool WarpBuilder::build_Double(BytecodeLocation loc) {
  pushConstant(loc.getInlineValue());
  return true;
}

bool WarpBuilder::build_ResumeIndex(BytecodeLocation loc) {
  pushConstant(Int32Value(loc.getResumeIndex()));
  return true;
}

bool WarpBuilder::build_Return(BytecodeLocation) {
  MDefinition* def = current->pop();

  MReturn* ret = MReturn::New(alloc(), def);
  current->end(ret);

  setTerminatedBlock();
  return true;
}

bool WarpBuilder::build_RetRval(BytecodeLocation) {
  MDefinition* rval;
  if (script_->noScriptRval()) {
    rval = constant(UndefinedValue());
  } else {
    rval = current->getSlot(info().returnValueSlot());
  }

  MReturn* ret = MReturn::New(alloc(), rval);
  current->end(ret);

  setTerminatedBlock();
  return true;
}

bool WarpBuilder::build_GetLocal(BytecodeLocation loc) {
  current->pushLocal(loc.local());
  return true;
}

bool WarpBuilder::build_SetLocal(BytecodeLocation loc) {
  current->setLocal(loc.local());
  return true;
}

bool WarpBuilder::build_InitLexical(BytecodeLocation loc) {
  current->setLocal(loc.local());
  return true;
}

bool WarpBuilder::build_GetArg(BytecodeLocation loc) {
  uint32_t arg = loc.arg();
  if (info().argsObjAliasesFormals()) {
    MDefinition* argsObj = current->argumentsObject();
    auto* getArg = MGetArgumentsObjectArg::New(alloc(), argsObj, arg);
    current->add(getArg);
    current->push(getArg);
  } else {
    current->pushArg(arg);
  }
  return true;
}

bool WarpBuilder::build_SetArg(BytecodeLocation loc) {
  MOZ_ASSERT(script_->jitScript()->modifiesArguments());

  uint32_t arg = loc.arg();
  MDefinition* val = current->peek(-1);

  if (!info().argumentsAliasesFormals()) {
    MOZ_ASSERT(!info().argsObjAliasesFormals());

    // |arguments| is never referenced within this function. No arguments object
    // is created in this case, so we don't need to worry about synchronizing
    // the argument values when writing to them.
    MOZ_ASSERT_IF(!info().hasArguments(), !info().needsArgsObj());

    // The arguments object doesn't map to the actual argument values, so we
    // also don't need to worry about synchronizing them.
    // Directly writing to a positional formal parameter is only possible when
    // the |arguments| contents are never observed, otherwise we can't
    // reconstruct the original parameter values when we access them through
    // |arguments[i]|. AnalyzeArgumentsUsage ensures this is handled correctly.
    MOZ_ASSERT_IF(info().hasArguments(), !info().hasMappedArgsObj());

    current->setArg(arg);
    return true;
  }

  MOZ_ASSERT(info().hasArguments() && info().hasMappedArgsObj(),
             "arguments aliases formals when an arguments binding is present "
             "and the arguments object is mapped");

  // TODO: double check corresponding IonBuilder code when supporting the
  // arguments analysis in WarpBuilder.

  MOZ_ASSERT(info().needsArgsObj(),
             "unexpected JSOp::SetArg with lazy arguments");

  MOZ_ASSERT(
      info().argsObjAliasesFormals(),
      "argsObjAliasesFormals() is true iff a mapped arguments object is used");

  // If an arguments object is in use, and it aliases formals, then all SetArgs
  // must go through the arguments object.
  MDefinition* argsObj = current->argumentsObject();
  current->add(MPostWriteBarrier::New(alloc(), argsObj, val));
  auto* ins = MSetArgumentsObjectArg::New(alloc(), argsObj, arg, val);
  current->add(ins);
  return resumeAfter(ins, loc);
}

bool WarpBuilder::build_ToNumeric(BytecodeLocation loc) {
  MDefinition* value = current->pop();
  MToNumeric* ins = MToNumeric::New(alloc(), value, /* types = */ nullptr);
  current->add(ins);
  current->push(ins);
  return resumeAfter(ins, loc);
}

bool WarpBuilder::buildUnaryOp(BytecodeLocation loc) {
  MDefinition* value = current->pop();
  MInstruction* ins = MUnaryCache::New(alloc(), value);
  current->add(ins);
  current->push(ins);
  return resumeAfter(ins, loc);
}

bool WarpBuilder::build_Inc(BytecodeLocation loc) { return buildUnaryOp(loc); }

bool WarpBuilder::build_Dec(BytecodeLocation loc) { return buildUnaryOp(loc); }

bool WarpBuilder::build_Neg(BytecodeLocation loc) { return buildUnaryOp(loc); }

bool WarpBuilder::build_BitNot(BytecodeLocation loc) {
  return buildUnaryOp(loc);
}

bool WarpBuilder::buildBinaryOp(BytecodeLocation loc) {
  MDefinition* right = current->pop();
  MDefinition* left = current->pop();
  MInstruction* ins = MBinaryCache::New(alloc(), left, right, MIRType::Value);
  current->add(ins);
  current->push(ins);
  return resumeAfter(ins, loc);
}

bool WarpBuilder::build_Add(BytecodeLocation loc) { return buildBinaryOp(loc); }

bool WarpBuilder::build_Sub(BytecodeLocation loc) { return buildBinaryOp(loc); }

bool WarpBuilder::build_Mul(BytecodeLocation loc) { return buildBinaryOp(loc); }

bool WarpBuilder::build_Div(BytecodeLocation loc) { return buildBinaryOp(loc); }

bool WarpBuilder::build_Mod(BytecodeLocation loc) { return buildBinaryOp(loc); }

bool WarpBuilder::build_BitAnd(BytecodeLocation loc) {
  return buildBinaryOp(loc);
}

bool WarpBuilder::build_BitOr(BytecodeLocation loc) {
  return buildBinaryOp(loc);
}

bool WarpBuilder::build_BitXor(BytecodeLocation loc) {
  return buildBinaryOp(loc);
}

bool WarpBuilder::build_Lsh(BytecodeLocation loc) { return buildBinaryOp(loc); }

bool WarpBuilder::build_Rsh(BytecodeLocation loc) { return buildBinaryOp(loc); }

bool WarpBuilder::build_Ursh(BytecodeLocation loc) {
  return buildBinaryOp(loc);
}

bool WarpBuilder::buildCompareOp(BytecodeLocation loc) {
  MDefinition* right = current->pop();
  MDefinition* left = current->pop();
  MInstruction* ins = MBinaryCache::New(alloc(), left, right, MIRType::Boolean);
  current->add(ins);
  current->push(ins);
  return resumeAfter(ins, loc);
}

bool WarpBuilder::build_Eq(BytecodeLocation loc) { return buildCompareOp(loc); }

bool WarpBuilder::build_Ne(BytecodeLocation loc) { return buildCompareOp(loc); }

bool WarpBuilder::build_Lt(BytecodeLocation loc) { return buildCompareOp(loc); }

bool WarpBuilder::build_Le(BytecodeLocation loc) { return buildCompareOp(loc); }

bool WarpBuilder::build_Gt(BytecodeLocation loc) { return buildCompareOp(loc); }

bool WarpBuilder::build_Ge(BytecodeLocation loc) { return buildCompareOp(loc); }

bool WarpBuilder::build_StrictEq(BytecodeLocation loc) {
  return buildCompareOp(loc);
}

bool WarpBuilder::build_StrictNe(BytecodeLocation loc) {
  return buildCompareOp(loc);
}

bool WarpBuilder::build_JumpTarget(BytecodeLocation loc) {
  PendingEdgesMap::Ptr p = pendingEdges_.lookup(loc.toRawBytecode());
  if (!p) {
    // No (reachable) jumps so this is just a no-op.
    return true;
  }

  PendingEdges edges(std::move(p->value()));
  pendingEdges_.remove(p);

  MOZ_ASSERT(!edges.empty());

  MBasicBlock* joinBlock = nullptr;

  // Create join block if there's fall-through from the previous bytecode op.
  if (!hasTerminatedBlock()) {
    MBasicBlock* pred = current;
    if (!startNewBlock(pred, loc)) {
      return false;
    }
    pred->end(MGoto::New(alloc(), current));
    joinBlock = current;
    setTerminatedBlock();
  }

  auto addEdge = [&](MBasicBlock* pred, size_t numToPop) -> bool {
    if (joinBlock) {
      MOZ_ASSERT(pred->stackDepth() - numToPop == joinBlock->stackDepth());
      return joinBlock->addPredecessorPopN(alloc(), pred, numToPop);
    }
    if (!startNewBlock(pred, loc, numToPop)) {
      return false;
    }
    joinBlock = current;
    setTerminatedBlock();
    return true;
  };

  // When a block is terminated with an MTest instruction we can end up with the
  // following triangle structure:
  //
  //        testBlock
  //         /    |
  //     block    |
  //         \    |
  //        joinBlock
  //
  // Although this is fine for correctness, the FoldTests pass is unable to
  // optimize this pattern. This matters for short-circuit operations
  // (JSOp::And, JSOp::Coalesce, etc).
  //
  // To fix these issues, we create an empty block to get a diamond structure:
  //
  //        testBlock
  //         /    |
  //     block  emptyBlock
  //         \    |
  //        joinBlock
  //
  // TODO: re-evaluate this. It would probably be better to fix FoldTests to
  // support the triangle pattern so that we can remove this. IonBuilder had
  // other concerns that don't apply to WarpBuilder.
  auto createEmptyBlockForTest = [&](MBasicBlock* pred, size_t successor,
                                     size_t numToPop) -> MBasicBlock* {
    MOZ_ASSERT(joinBlock);

    if (!startNewBlock(pred, loc, numToPop)) {
      return nullptr;
    }

    MBasicBlock* emptyBlock = current;
    MOZ_ASSERT(emptyBlock->stackDepth() == joinBlock->stackDepth());

    MTest* test = pred->lastIns()->toTest();
    test->initSuccessor(successor, emptyBlock);

    emptyBlock->end(MGoto::New(alloc(), joinBlock));
    setTerminatedBlock();

    return emptyBlock;
  };

  for (const PendingEdge& edge : edges) {
    MBasicBlock* source = edge.block();
    MControlInstruction* lastIns = source->lastIns();
    switch (edge.kind()) {
      case PendingEdge::Kind::TestTrue: {
        // JSOp::Case must pop the value when branching to the true-target.
        // If we create an empty block, we have to pop the value there instead
        // of as part of the emptyBlock -> joinBlock edge so stack depths match
        // the current depth.
        const size_t numToPop = (edge.testOp() == JSOp::Case) ? 1 : 0;

        const size_t successor = 0;  // true-branch
        if (joinBlock && TestTrueTargetIsJoinPoint(edge.testOp())) {
          MBasicBlock* pred =
              createEmptyBlockForTest(source, successor, numToPop);
          if (!pred || !addEdge(pred, /* numToPop = */ 0)) {
            return false;
          }
        } else {
          if (!addEdge(source, numToPop)) {
            return false;
          }
          lastIns->toTest()->initSuccessor(successor, joinBlock);
        }
        continue;
      }

      case PendingEdge::Kind::TestFalse: {
        const size_t numToPop = 0;
        const size_t successor = 1;  // false-branch
        if (joinBlock && !TestTrueTargetIsJoinPoint(edge.testOp())) {
          MBasicBlock* pred =
              createEmptyBlockForTest(source, successor, numToPop);
          if (!pred || !addEdge(pred, /* numToPop = */ 0)) {
            return false;
          }
        } else {
          if (!addEdge(source, numToPop)) {
            return false;
          }
          lastIns->toTest()->initSuccessor(successor, joinBlock);
        }
        continue;
      }

      case PendingEdge::Kind::Goto:
        if (!addEdge(source, /* numToPop = */ 0)) {
          return false;
        }
        lastIns->toGoto()->initSuccessor(0, joinBlock);
        continue;

      case PendingEdge::Kind::GotoWithFake:
        if (!addEdge(source, /* numToPop = */ 0)) {
          return false;
        }
        lastIns->toGotoWithFake()->initSuccessor(1, joinBlock);
        continue;
    }
    MOZ_CRASH("Invalid kind");
  }

  // Start traversing the join block. Make sure it comes after predecessor
  // blocks created by createEmptyBlockForTest.
  MOZ_ASSERT(hasTerminatedBlock());
  MOZ_ASSERT(joinBlock);
  graph().moveBlockToEnd(joinBlock);
  current = joinBlock;

  return true;
}

bool WarpBuilder::build_LoopHead(BytecodeLocation loc) {
  // All loops have the following bytecode structure:
  //
  //    LoopHead
  //    ...
  //    IfNe/Goto to LoopHead

  if (hasTerminatedBlock()) {
    // The whole loop is unreachable.
    return true;
  }

  // TODO: support OSR

  loopDepth_++;

  MBasicBlock* pred = current;
  if (!startNewLoopHeaderBlock(pred, loc)) {
    return false;
  }

  pred->end(MGoto::New(alloc(), current));

  // TODO: handle destructuring special case (IonBuilder::newPendingLoopHeader)

  MInterruptCheck* check = MInterruptCheck::New(alloc());
  current->add(check);

  // TODO: recompile check

  return true;
}

bool WarpBuilder::buildTestOp(BytecodeLocation loc) {
  if (loc.isBackedge()) {
    return buildTestBackedge(loc);
  }

  JSOp op = loc.getOp();
  BytecodeLocation target1 = loc.next();
  BytecodeLocation target2 = loc.getJumpTarget();

  if (TestTrueTargetIsJoinPoint(op)) {
    std::swap(target1, target2);
  }

  // JSOp::And and JSOp::Or inspect the top stack value but don't pop it.
  // Also note that JSOp::Case must pop a second value on the true-branch (the
  // input to the switch-statement). This conditional pop happens in
  // build_JumpTarget.
  bool mustKeepCondition = (op == JSOp::And || op == JSOp::Or);
  MDefinition* value = mustKeepCondition ? current->peek(-1) : current->pop();

  // If this op always branches to the same location we treat this as a
  // JSOp::Goto.
  if (target1 == target2) {
    value->setImplicitlyUsedUnchecked();
    return buildForwardGoto(target1);
  }

  MTest* test = MTest::New(alloc(), value, /* ifTrue = */ nullptr,
                           /* ifFalse = */ nullptr);
  current->end(test);

  if (!addPendingEdge(PendingEdge::NewTestTrue(current, op), target1)) {
    return false;
  }
  if (!addPendingEdge(PendingEdge::NewTestFalse(current, op), target2)) {
    return false;
  }

  setTerminatedBlock();
  return true;
}

bool WarpBuilder::buildTestBackedge(BytecodeLocation loc) {
  JSOp op = loc.getOp();
  MOZ_ASSERT(op == JSOp::IfNe);
  MOZ_ASSERT(loopDepth_ > 0);

  MDefinition* value = current->pop();

  BytecodeLocation loopHead = loc.getJumpTarget();
  MOZ_ASSERT(loopHead.is(JSOp::LoopHead));

  BytecodeLocation successor = loc.next();

  // We can finish the loop now. Use the loophead pc instead of the current pc
  // because the stack depth at the start of that op matches the current stack
  // depth (after popping our operand).
  MBasicBlock* pred = current;
  if (!startNewBlock(current, loopHead)) {
    return false;
  }

  pred->end(MTest::New(alloc(), value, /* ifTrue = */ current,
                       /* ifFalse = */ nullptr));

  if (!addPendingEdge(PendingEdge::NewTestFalse(pred, op), successor)) {
    return false;
  }

  return buildBackedge();
}

bool WarpBuilder::build_IfEq(BytecodeLocation loc) { return buildTestOp(loc); }

bool WarpBuilder::build_IfNe(BytecodeLocation loc) { return buildTestOp(loc); }

bool WarpBuilder::build_And(BytecodeLocation loc) { return buildTestOp(loc); }

bool WarpBuilder::build_Or(BytecodeLocation loc) { return buildTestOp(loc); }

bool WarpBuilder::build_Case(BytecodeLocation loc) { return buildTestOp(loc); }

bool WarpBuilder::build_Default(BytecodeLocation loc) {
  current->pop();
  return buildForwardGoto(loc.getJumpTarget());
}

bool WarpBuilder::build_Coalesce(BytecodeLocation loc) {
  BytecodeLocation target1 = loc.next();
  BytecodeLocation target2 = loc.getJumpTarget();
  MOZ_ASSERT(target2 > target1);

  MDefinition* value = current->peek(-1);

  MInstruction* isNullOrUndefined = MIsNullOrUndefined::New(alloc(), value);
  current->add(isNullOrUndefined);

  current->end(MTest::New(alloc(), isNullOrUndefined, /* ifTrue = */ nullptr,
                          /* ifFalse = */ nullptr));

  if (!addPendingEdge(PendingEdge::NewTestTrue(current, JSOp::Coalesce),
                      target1)) {
    return false;
  }
  if (!addPendingEdge(PendingEdge::NewTestFalse(current, JSOp::Coalesce),
                      target2)) {
    return false;
  }

  setTerminatedBlock();
  return true;
}

bool WarpBuilder::buildBackedge() {
  MOZ_ASSERT(loopDepth_ > 0);
  loopDepth_--;

  MBasicBlock* header = loopStack_.popCopy().header();
  current->end(MGoto::New(alloc(), header));

  AbortReason r = header->setBackedge(alloc(), current);
  if (r == AbortReason::NoAbort) {
    setTerminatedBlock();
    return true;
  }

  MOZ_ASSERT(r == AbortReason::Alloc);
  return false;
}

bool WarpBuilder::buildForwardGoto(BytecodeLocation target) {
  current->end(MGoto::New(alloc(), nullptr));

  if (!addPendingEdge(PendingEdge::NewGoto(current), target)) {
    return false;
  }

  setTerminatedBlock();
  return true;
}

bool WarpBuilder::build_Goto(BytecodeLocation loc) {
  if (loc.isBackedge()) {
    return buildBackedge();
  }

  return buildForwardGoto(loc.getJumpTarget());
}

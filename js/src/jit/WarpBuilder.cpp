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
      script_(input.script()->script()) {}

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

BytecodeSite* WarpBuilder::newBytecodeSite(jsbytecode* pc) {
  MOZ_ASSERT(info().inlineScriptTree()->script()->containsPC(pc));
  return new (alloc()) BytecodeSite(info().inlineScriptTree(), pc);
}

bool WarpBuilder::startNewBlock(size_t stackDepth, jsbytecode* pc,
                                MBasicBlock* maybePredecessor) {
  MOZ_ASSERT_IF(maybePredecessor, maybePredecessor->stackDepth() == stackDepth);

  MBasicBlock* block =
      MBasicBlock::New(graph(), stackDepth, info(), maybePredecessor,
                       newBytecodeSite(pc), MBasicBlock::NORMAL);
  if (!block) {
    return false;
  }

  graph().addBlock(block);

  // TODO: fix loop depth once we support loops
  // TODO: set block hit count (for branch pruning pass)
  block->setLoopDepth(0);

  current = block;
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

  return true;
}

bool WarpBuilder::buildPrologue() {
  if (!startNewBlock(info().firstStackSlot(), script_->code())) {
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

    if (hasTerminatedBlock()) {
      continue;
    }

    if (!alloc().ensureBallast()) {
      return false;
    }

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
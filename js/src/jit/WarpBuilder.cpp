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

  // Initialize the environment chain.
  MInstruction* env = MConstant::New(alloc(), UndefinedValue());
  current->add(env);
  current->initSlot(info().environmentChainSlot(), env);

  // Initialize the return value.
  MInstruction* returnValue = MConstant::New(alloc(), UndefinedValue());
  current->add(returnValue);
  current->initSlot(info().returnValueSlot(), returnValue);

  current->add(MStart::New(alloc()));

  // TODO: environment chain initialization
  // TODO: initialize local slots

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

bool WarpBuilder::build_Zero(BytecodeLocation) {
  pushConstant(Int32Value(0));
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

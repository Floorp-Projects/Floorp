/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/BytecodeControlStructures.h"

#include "frontend/BytecodeEmitter.h"
#include "frontend/EmitterScope.h"
#include "vm/Opcodes.h"

using namespace js;
using namespace js::frontend;

using mozilla::Maybe;

NestableControl::NestableControl(BytecodeEmitter* bce, StatementKind kind)
    : Nestable<NestableControl>(&bce->innermostNestableControl),
      kind_(kind),
      emitterScope_(bce->innermostEmitterScopeNoCheck()) {}

BreakableControl::BreakableControl(BytecodeEmitter* bce, StatementKind kind)
    : NestableControl(bce, kind) {
  MOZ_ASSERT(is<BreakableControl>());
}

bool BreakableControl::patchBreaks(BytecodeEmitter* bce) {
  return bce->emitJumpTargetAndPatch(breaks);
}

LabelControl::LabelControl(BytecodeEmitter* bce, JSAtom* label,
                           ptrdiff_t startOffset)
    : BreakableControl(bce, StatementKind::Label),
      label_(bce->cx, label),
      startOffset_(startOffset) {}

LoopControl::LoopControl(BytecodeEmitter* bce, StatementKind loopKind)
    : BreakableControl(bce, loopKind), tdzCache_(bce) {
  MOZ_ASSERT(is<LoopControl>());

  LoopControl* enclosingLoop = findNearest<LoopControl>(enclosing());

  stackDepth_ = bce->bytecodeSection().stackDepth();
  loopDepth_ = enclosingLoop ? enclosingLoop->loopDepth_ + 1 : 1;

  int loopSlots;
  if (loopKind == StatementKind::Spread) {
    // The iterator next method, the iterator, the result array, and
    // the current array index are on the stack.
    loopSlots = 4;
  } else if (loopKind == StatementKind::ForOfLoop) {
    // The iterator next method, the iterator, and the current value
    // are on the stack.
    loopSlots = 3;
  } else if (loopKind == StatementKind::ForInLoop) {
    // The iterator and the current value are on the stack.
    loopSlots = 2;
  } else {
    // No additional loop values are on the stack.
    loopSlots = 0;
  }

  MOZ_ASSERT(loopSlots <= stackDepth_);

  if (enclosingLoop) {
    canIonOsr_ = (enclosingLoop->canIonOsr_ &&
                  stackDepth_ == enclosingLoop->stackDepth_ + loopSlots);
  } else {
    canIonOsr_ = stackDepth_ == loopSlots;
  }
}

bool LoopControl::emitContinueTarget(BytecodeEmitter* bce) {
  if (!bce->emitJumpTarget(&continueTarget_)) {
    return false;
  }
  return true;
}

bool LoopControl::emitSpecialBreakForDone(BytecodeEmitter* bce) {
  // This doesn't pop stack values, nor handle any other controls.
  // Should be called on the toplevel of the loop.
  MOZ_ASSERT(bce->bytecodeSection().stackDepth() == stackDepth_);
  MOZ_ASSERT(bce->innermostNestableControl == this);

  if (!bce->newSrcNote(SRC_BREAK)) {
    return false;
  }
  if (!bce->emitJump(JSOP_GOTO, &breaks)) {
    return false;
  }

  return true;
}

bool LoopControl::emitEntryJump(BytecodeEmitter* bce) {
  if (!bce->emitJump(JSOP_GOTO, &entryJump_)) {
    return false;
  }
  return true;
}

bool LoopControl::emitLoopHead(BytecodeEmitter* bce,
                               const Maybe<uint32_t>& nextPos) {
  if (nextPos) {
    if (!bce->updateSourceCoordNotes(*nextPos)) {
      return false;
    }
  }

  head_ = {bce->bytecodeSection().offset()};
  ptrdiff_t off;
  if (!bce->emitJumpTargetOp(JSOP_LOOPHEAD, &off)) {
    return false;
  }

  return true;
}

bool LoopControl::emitLoopEntry(BytecodeEmitter* bce,
                                const Maybe<uint32_t>& nextPos) {
  if (nextPos) {
    if (!bce->updateSourceCoordNotes(*nextPos)) {
      return false;
    }
  }

  JumpTarget entry = {bce->bytecodeSection().offset()};
  bce->patchJumpsToTarget(entryJump_, entry);

  MOZ_ASSERT(loopDepth_ > 0);

  ptrdiff_t off;
  if (!bce->emitJumpTargetOp(JSOP_LOOPENTRY, &off)) {
    return false;
  }
  SetLoopEntryDepthHintAndFlags(bce->bytecodeSection().code(off), loopDepth_,
                                canIonOsr_);

  return true;
}

bool LoopControl::emitLoopEnd(BytecodeEmitter* bce, JSOp op) {
  JumpList beq;
  if (!bce->emitBackwardJump(op, head_, &beq, &breakTarget_)) {
    return false;
  }

  loopEndOffset_ = beq.offset;

  return true;
}

bool LoopControl::patchBreaksAndContinues(BytecodeEmitter* bce) {
  MOZ_ASSERT(continueTarget_.offset != -1);
  if (!patchBreaks(bce)) {
    return false;
  }
  bce->patchJumpsToTarget(continues, continueTarget_);
  return true;
}

TryFinallyControl::TryFinallyControl(BytecodeEmitter* bce, StatementKind kind)
    : NestableControl(bce, kind), emittingSubroutine_(false) {
  MOZ_ASSERT(is<TryFinallyControl>());
}

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/BytecodeControlStructures.h"

#include "frontend/BytecodeEmitter.h"   // BytecodeEmitter
#include "frontend/EmitterScope.h"      // EmitterScope
#include "frontend/ForOfLoopControl.h"  // ForOfLoopControl
#include "vm/Opcodes.h"                 // JSOp

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

LabelControl::LabelControl(BytecodeEmitter* bce, TaggedParserAtomIndex label,
                           BytecodeOffset startOffset)
    : BreakableControl(bce, StatementKind::Label),
      label_(label),
      startOffset_(startOffset) {}

LoopControl::LoopControl(BytecodeEmitter* bce, StatementKind loopKind)
    : BreakableControl(bce, loopKind), tdzCache_(bce) {
  MOZ_ASSERT(is<LoopControl>());

  LoopControl* enclosingLoop = findNearest<LoopControl>(enclosing());

  stackDepth_ = bce->bytecodeSection().stackDepth();
  loopDepth_ = enclosingLoop ? enclosingLoop->loopDepth_ + 1 : 1;
}

bool LoopControl::emitContinueTarget(BytecodeEmitter* bce) {
  // Note: this is always called after emitting the loop body so we must have
  // emitted all 'continues' by now.
  return bce->emitJumpTargetAndPatch(continues);
}

bool LoopControl::emitLoopHead(BytecodeEmitter* bce,
                               const Maybe<uint32_t>& nextPos) {
  // Insert a Nop if needed to ensure the script does not start with a
  // JSOp::LoopHead. This avoids JIT issues with prologue code + try notes
  // or OSR. See bug 1602390 and bug 1602681.
  if (bce->bytecodeSection().offset().toUint32() == 0) {
    if (!bce->emit1(JSOp::Nop)) {
      return false;
    }
  }

  if (nextPos) {
    if (!bce->updateSourceCoordNotes(*nextPos)) {
      return false;
    }
  }

  MOZ_ASSERT(loopDepth_ > 0);

  head_ = {bce->bytecodeSection().offset()};

  BytecodeOffset off;
  if (!bce->emitJumpTargetOp(JSOp::LoopHead, &off)) {
    return false;
  }
  SetLoopHeadDepthHint(bce->bytecodeSection().code(off), loopDepth_);

  return true;
}

bool LoopControl::emitLoopEnd(BytecodeEmitter* bce, JSOp op,
                              TryNoteKind tryNoteKind) {
  JumpList jump;
  if (!bce->emitJumpNoFallthrough(op, &jump)) {
    return false;
  }
  bce->patchJumpsToTarget(jump, head_);

  // Create a fallthrough for closing iterators, and as a target for break
  // statements.
  JumpTarget breakTarget;
  if (!bce->emitJumpTarget(&breakTarget)) {
    return false;
  }
  if (!patchBreaks(bce)) {
    return false;
  }
  if (!bce->addTryNote(tryNoteKind, bce->bytecodeSection().stackDepth(),
                       headOffset(), breakTarget.offset)) {
    return false;
  }
  return true;
}

TryFinallyControl::TryFinallyControl(BytecodeEmitter* bce, StatementKind kind)
    : NestableControl(bce, kind) {
  MOZ_ASSERT(is<TryFinallyControl>());
}

NonLocalExitControl::NonLocalExitControl(BytecodeEmitter* bce, Kind kind)
    : bce_(bce),
      savedScopeNoteIndex_(bce->bytecodeSection().scopeNoteList().length()),
      savedDepth_(bce->bytecodeSection().stackDepth()),
      openScopeNoteIndex_(bce->innermostEmitterScope()->noteIndex()),
      kind_(kind) {}

NonLocalExitControl::~NonLocalExitControl() {
  for (uint32_t n = savedScopeNoteIndex_;
       n < bce_->bytecodeSection().scopeNoteList().length(); n++) {
    bce_->bytecodeSection().scopeNoteList().recordEnd(
        n, bce_->bytecodeSection().offset());
  }
  bce_->bytecodeSection().setStackDepth(savedDepth_);
}

bool NonLocalExitControl::emitReturn(BytecodeOffset setRvalOffset) {
  MOZ_ASSERT(kind_ == Return);
  setRvalOffset_ = setRvalOffset;
  return emitNonLocalJump(nullptr);
}

bool NonLocalExitControl::leaveScope(EmitterScope* es) {
  if (!es->leave(bce_, /* nonLocal = */ true)) {
    return false;
  }

  // As we pop each scope due to the non-local jump, emit notes that
  // record the extent of the enclosing scope. These notes will have
  // their ends recorded in ~NonLocalExitControl().
  GCThingIndex enclosingScopeIndex = ScopeNote::NoScopeIndex;
  if (es->enclosingInFrame()) {
    enclosingScopeIndex = es->enclosingInFrame()->index();
  }
  if (!bce_->bytecodeSection().scopeNoteList().append(
          enclosingScopeIndex, bce_->bytecodeSection().offset(),
          openScopeNoteIndex_)) {
    return false;
  }
  openScopeNoteIndex_ = bce_->bytecodeSection().scopeNoteList().length() - 1;

  return true;
}

/*
 * Emit additional bytecode(s) for non-local jumps.
 */
bool NonLocalExitControl::emitNonLocalJump(NestableControl* target) {
  EmitterScope* es = bce_->innermostEmitterScope();
  int npops = 0;

  AutoCheckUnstableEmitterScope cues(bce_);

  // We emit IteratorClose bytecode inline. 'continue' statements do
  // not call IteratorClose for the loop they are continuing.
  bool emitIteratorCloseAtTarget = kind_ != Continue;

  auto flushPops = [&npops](BytecodeEmitter* bce) {
    if (npops && !bce->emitPopN(npops)) {
      return false;
    }
    npops = 0;
    return true;
  };

  // If we are closing multiple for-of loops, the resulting FOR_OF_ITERCLOSE
  // trynotes must be appropriately nested. Each FOR_OF_ITERCLOSE starts when
  // we close the corresponding for-of iterator, and continues until the
  // actual jump.
  Vector<BytecodeOffset, 4> forOfIterCloseScopeStarts(bce_->cx);

  // Walk the nestable control stack and patch jumps.
  for (NestableControl* control = bce_->innermostNestableControl;
       control != target; control = control->enclosing()) {
    // Walk the scope stack and leave the scopes we entered. Leaving a scope
    // may emit administrative ops like JSOp::PopLexicalEnv but never anything
    // that manipulates the stack.
    for (; es != control->emitterScope(); es = es->enclosingInFrame()) {
      if (!leaveScope(es)) {
        return false;
      }
    }

    switch (control->kind()) {
      case StatementKind::Finally: {
        TryFinallyControl& finallyControl = control->as<TryFinallyControl>();
        if (finallyControl.emittingSubroutine()) {
          /*
           * There's a [resume-index-or-exception, throwing] pair on
           * the stack that we need to pop. If the script is not a
           * noScriptRval script, we also need to pop the cached rval.
           */
          if (bce_->sc->noScriptRval()) {
            npops += 2;
          } else {
            npops += 3;
          }
        } else {
          if (!flushPops(bce_)) {
            return false;
          }
          if (!bce_->emitJumpToFinally(&finallyControl.finallyJumps_)) {
            return false;
          }
          finallyControl.setHasNonLocalJumps();
        }
        break;
      }

      case StatementKind::ForOfLoop: {
        if (!flushPops(bce_)) {
          return false;
        }
        BytecodeOffset tryNoteStart;
        ForOfLoopControl& loopinfo = control->as<ForOfLoopControl>();
        if (!loopinfo.emitPrepareForNonLocalJumpFromScope(
                bce_, *es,
                /* isTarget = */ false, &tryNoteStart)) {
          //      [stack] ...
          return false;
        }
        if (!forOfIterCloseScopeStarts.append(tryNoteStart)) {
          return false;
        }
        break;
      }

      case StatementKind::ForInLoop:
        if (!flushPops(bce_)) {
          return false;
        }

        // The iterator and the current value are on the stack.
        if (!bce_->emit1(JSOp::EndIter)) {
          //        [stack] ...
          return false;
        }
        break;

      default:
        break;
    }
  }

  if (!flushPops(bce_)) {
    return false;
  }

  if (target && emitIteratorCloseAtTarget && target->is<ForOfLoopControl>()) {
    BytecodeOffset tryNoteStart;
    ForOfLoopControl& loopinfo = target->as<ForOfLoopControl>();
    if (!loopinfo.emitPrepareForNonLocalJumpFromScope(bce_, *es,
                                                      /* isTarget = */ true,
                                                      &tryNoteStart)) {
      //            [stack] ... UNDEF UNDEF UNDEF
      return false;
    }
    if (!forOfIterCloseScopeStarts.append(tryNoteStart)) {
      return false;
    }
  }

  EmitterScope* targetEmitterScope =
      target ? target->emitterScope() : bce_->varEmitterScope;
  for (; es != targetEmitterScope; es = es->enclosingInFrame()) {
    if (!leaveScope(es)) {
      return false;
    }
  }

  // Close FOR_OF_ITERCLOSE trynotes.
  BytecodeOffset end = bce_->bytecodeSection().offset();
  for (BytecodeOffset start : forOfIterCloseScopeStarts) {
    if (!bce_->addTryNote(TryNoteKind::ForOfIterClose, 0, start, end)) {
      return false;
    }
  }

  switch (kind_) {
    case Continue: {
      LoopControl* loop = &target->as<LoopControl>();
      if (!bce_->emitJump(JSOp::Goto, &loop->continues)) {
        return false;
      }
      break;
    }
    case Break: {
      BreakableControl* breakable = &target->as<BreakableControl>();
      if (!bce_->emitJump(JSOp::Goto, &breakable->breaks)) {
        return false;
      }
      break;
    }
    case Return:
      MOZ_ASSERT(!target);
      if (!bce_->finishReturn(setRvalOffset_)) {
        return false;
      }
      break;
  }

  return true;
}

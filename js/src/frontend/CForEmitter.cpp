/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/CForEmitter.h"

#include "frontend/BytecodeEmitter.h"
#include "frontend/EmitterScope.h"
#include "frontend/SourceNotes.h"
#include "vm/Opcodes.h"
#include "vm/Scope.h"

using namespace js;
using namespace js::frontend;

using mozilla::Maybe;

CForEmitter::CForEmitter(BytecodeEmitter* bce,
                         const EmitterScope* headLexicalEmitterScopeForLet)
    : bce_(bce),
      headLexicalEmitterScopeForLet_(headLexicalEmitterScopeForLet) {}

bool CForEmitter::emitInit(const Maybe<uint32_t>& initPos) {
  MOZ_ASSERT(state_ == State::Start);

  loopInfo_.emplace(bce_, StatementKind::ForLoop);

  if (initPos) {
    if (!bce_->updateSourceCoordNotes(*initPos)) {
      return false;
    }
  }

#ifdef DEBUG
  state_ = State::Init;
#endif
  return true;
}

bool CForEmitter::emitBody(Cond cond, const Maybe<uint32_t>& bodyPos) {
  MOZ_ASSERT(state_ == State::Init);
  cond_ = cond;

  // ES 13.7.4.8 step 2. The initial freshening.
  //
  // If an initializer let-declaration may be captured during loop
  // iteration, the current scope has an environment.  If so, freshen the
  // current environment to expose distinct bindings for each loop
  // iteration.
  if (headLexicalEmitterScopeForLet_) {
    // The environment chain only includes an environment for the
    // for(;;) loop head's let-declaration *if* a scope binding is
    // captured, thus requiring a fresh environment each iteration. If
    // a lexical scope exists for the head, it must be the innermost
    // one. If that scope has closed-over bindings inducing an
    // environment, recreate the current environment.
    MOZ_ASSERT(headLexicalEmitterScopeForLet_ == bce_->innermostEmitterScope());
    MOZ_ASSERT(headLexicalEmitterScopeForLet_->scope(bce_)->kind() ==
               ScopeKind::Lexical);

    if (headLexicalEmitterScopeForLet_->hasEnvironment()) {
      if (!bce_->emit1(JSOP_FRESHENLEXICALENV)) {
        return false;
      }
    }
  }

  if (!bce_->newSrcNote(SRC_FOR, &noteIndex_)) {
    return false;
  }
  if (!bce_->emit1(JSOP_NOP)) {
    //              [stack]
    return false;
  }

  biasedTop_ = bce_->bytecodeSection().offset();

  if (cond_ == Cond::Present) {
    // Goto the loop condition, which branches back to iterate.
    if (!loopInfo_->emitEntryJump(bce_)) {
      return false;
    }
  }

  if (!loopInfo_->emitLoopHead(bce_, bodyPos)) {
    //              [stack]
    return false;
  }

  if (cond_ == Cond::Missing) {
    if (!loopInfo_->emitLoopEntry(bce_, bodyPos)) {
      //            [stack]
      return false;
    }
  }

  tdzCache_.emplace(bce_);

#ifdef DEBUG
  state_ = State::Body;
#endif
  return true;
}

bool CForEmitter::emitUpdate(Update update, const Maybe<uint32_t>& updatePos) {
  MOZ_ASSERT(state_ == State::Body);
  update_ = update;
  tdzCache_.reset();

  // Set loop and enclosing "update" offsets, for continue.  Note that we
  // continue to immediately *before* the block-freshening: continuing must
  // refresh the block.
  if (!loopInfo_->emitContinueTarget(bce_)) {
    return false;
  }

  // ES 13.7.4.8 step 3.e. The per-iteration freshening.
  if (headLexicalEmitterScopeForLet_) {
    MOZ_ASSERT(headLexicalEmitterScopeForLet_ == bce_->innermostEmitterScope());
    MOZ_ASSERT(headLexicalEmitterScopeForLet_->scope(bce_)->kind() ==
               ScopeKind::Lexical);

    if (headLexicalEmitterScopeForLet_->hasEnvironment()) {
      if (!bce_->emit1(JSOP_FRESHENLEXICALENV)) {
        return false;
      }
    }
  }

  // The update code may not be executed at all; it needs its own TDZ
  // cache.
  if (update_ == Update::Present) {
    tdzCache_.emplace(bce_);

    if (updatePos) {
      if (!bce_->updateSourceCoordNotes(*updatePos)) {
        return false;
      }
    }
  }

#ifdef DEBUG
  state_ = State::Update;
#endif
  return true;
}

bool CForEmitter::emitCond(const Maybe<uint32_t>& forPos,
                           const Maybe<uint32_t>& condPos,
                           const Maybe<uint32_t>& endPos) {
  MOZ_ASSERT(state_ == State::Update);

  if (update_ == Update::Present) {
    //              [stack] UPDATE

    if (!bce_->emit1(JSOP_POP)) {
      //            [stack]
      return false;
    }

    // Restore the absolute line number for source note readers.
    if (endPos) {
      uint32_t lineNum = bce_->parser->errorReporter().lineAt(*endPos);
      if (bce_->bytecodeSection().currentLine() != lineNum) {
        if (!bce_->newSrcNote2(SRC_SETLINE, ptrdiff_t(lineNum))) {
          return false;
        }
        bce_->bytecodeSection().setCurrentLine(lineNum);
      }
    }
  }

  if (update_ == Update::Present) {
    tdzCache_.reset();
  }

  condOffset_ = bce_->bytecodeSection().offset();

  if (cond_ == Cond::Present) {
    if (!loopInfo_->emitLoopEntry(bce_, condPos)) {
      //            [stack]
      return false;
    }
  } else if (update_ == Update::Missing) {
    // If there is no condition clause and no update clause, mark
    // the loop-ending "goto" with the location of the "for".
    // This ensures that the debugger will stop on each loop
    // iteration.
    if (forPos) {
      if (!bce_->updateSourceCoordNotes(*forPos)) {
        return false;
      }
    }
  }

#ifdef DEBUG
  state_ = State::Cond;
#endif
  return true;
}

bool CForEmitter::emitEnd() {
  MOZ_ASSERT(state_ == State::Cond);
  // Set the first note offset so we can find the loop condition.
  if (!bce_->setSrcNoteOffset(noteIndex_, SrcNote::For::CondOffset,
                              condOffset_ - biasedTop_)) {
    return false;
  }
  if (!bce_->setSrcNoteOffset(noteIndex_, SrcNote::For::UpdateOffset,
                              loopInfo_->continueTargetOffset() - biasedTop_)) {
    return false;
  }

  // If no loop condition, just emit a loop-closing jump.
  if (!loopInfo_->emitLoopEnd(bce_,
                              cond_ == Cond::Present ? JSOP_IFNE : JSOP_GOTO)) {
    //              [stack]
    return false;
  }

  // The third note offset helps us find the loop-closing jump.
  if (!bce_->setSrcNoteOffset(noteIndex_, SrcNote::For::BackJumpOffset,
                              loopInfo_->loopEndOffset() - biasedTop_))

  {
    return false;
  }

  if (!bce_->addTryNote(JSTRY_LOOP, bce_->bytecodeSection().stackDepth(),
                        loopInfo_->headOffset(),
                        loopInfo_->breakTargetOffset())) {
    return false;
  }

  if (!loopInfo_->patchBreaksAndContinues(bce_)) {
    //              [stack]
    return false;
  }

  loopInfo_.reset();

#ifdef DEBUG
  state_ = State::End;
#endif
  return true;
}

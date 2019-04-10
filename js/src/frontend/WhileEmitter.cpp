/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/WhileEmitter.h"

#include "frontend/BytecodeEmitter.h"
#include "frontend/SourceNotes.h"
#include "vm/Opcodes.h"

using namespace js;
using namespace js::frontend;

using mozilla::Maybe;

WhileEmitter::WhileEmitter(BytecodeEmitter* bce) : bce_(bce) {}

bool WhileEmitter::emitBody(const Maybe<uint32_t>& whilePos,
                            const Maybe<uint32_t>& bodyPos,
                            const Maybe<uint32_t>& endPos) {
  MOZ_ASSERT(state_ == State::Start);

  // Minimize bytecodes issued for one or more iterations by jumping to
  // the condition below the body and closing the loop if the condition
  // is true with a backward branch. For iteration count i:
  //
  //  i    test at the top                 test at the bottom
  //  =    ===============                 ==================
  //  0    ifeq-pass                       goto; ifne-fail
  //  1    ifeq-fail; goto; ifne-pass      goto; ifne-pass; ifne-fail
  //  2    2*(ifeq-fail; goto); ifeq-pass  goto; 2*ifne-pass; ifne-fail
  //  . . .
  //  N    N*(ifeq-fail; goto); ifeq-pass  goto; N*ifne-pass; ifne-fail

  // If we have a single-line while, like "while (x) ;", we want to
  // emit the line note before the initial goto, so that the
  // debugger sees a single entry point.  This way, if there is a
  // breakpoint on the line, it will only fire once; and "next"ing
  // will skip the whole loop.  However, for the multi-line case we
  // want to emit the line note after the initial goto, so that
  // "cont" stops on each iteration -- but without a stop before the
  // first iteration.
  if (whilePos && endPos &&
      bce_->parser->errorReporter().lineAt(*whilePos) ==
          bce_->parser->errorReporter().lineAt(*endPos)) {
    if (!bce_->updateSourceCoordNotes(*whilePos)) {
      return false;
    }
  }

  JumpTarget top = {-1};
  if (!bce_->emitJumpTarget(&top)) {
    return false;
  }

  loopInfo_.emplace(bce_, StatementKind::WhileLoop);
  loopInfo_->setContinueTarget(top.offset);

  if (!bce_->newSrcNote(SRC_WHILE, &noteIndex_)) {
    return false;
  }

  if (!loopInfo_->emitEntryJump(bce_)) {
    return false;
  }

  if (!loopInfo_->emitLoopHead(bce_, bodyPos)) {
    return false;
  }

  tdzCacheForBody_.emplace(bce_);

#ifdef DEBUG
  state_ = State::Body;
#endif
  return true;
}

bool WhileEmitter::emitCond(const Maybe<uint32_t>& condPos) {
  MOZ_ASSERT(state_ == State::Body);

  tdzCacheForBody_.reset();

  if (!loopInfo_->emitLoopEntry(bce_, condPos)) {
    return false;
  }

#ifdef DEBUG
  state_ = State::Cond;
#endif
  return true;
}

bool WhileEmitter::emitEnd() {
  MOZ_ASSERT(state_ == State::Cond);

  if (!loopInfo_->emitLoopEnd(bce_, JSOP_IFNE)) {
    return false;
  }

  if (!bce_->addTryNote(JSTRY_LOOP, bce_->bytecodeSection().stackDepth(),
                        loopInfo_->headOffset(),
                        loopInfo_->breakTargetOffset())) {
    return false;
  }

  if (!bce_->setSrcNoteOffset(noteIndex_, SrcNote::While::BackJumpOffset,
                              loopInfo_->loopEndOffsetFromEntryJump())) {
    return false;
  }

  if (!loopInfo_->patchBreaksAndContinues(bce_)) {
    return false;
  }

  loopInfo_.reset();

#ifdef DEBUG
  state_ = State::End;
#endif
  return true;
}

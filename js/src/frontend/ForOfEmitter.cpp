/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/ForOfEmitter.h"

#include "frontend/BytecodeEmitter.h"
#include "frontend/EmitterScope.h"
#include "frontend/IfEmitter.h"
#include "frontend/SourceNotes.h"
#include "vm/Opcodes.h"
#include "vm/Scope.h"

using namespace js;
using namespace js::frontend;

using mozilla::Maybe;
using mozilla::Nothing;

ForOfEmitter::ForOfEmitter(BytecodeEmitter* bce,
                           const EmitterScope* headLexicalEmitterScope,
                           bool allowSelfHostedIter, IteratorKind iterKind)
    : bce_(bce),
      allowSelfHostedIter_(allowSelfHostedIter),
      iterKind_(iterKind),
      headLexicalEmitterScope_(headLexicalEmitterScope) {}

bool ForOfEmitter::emitIterated() {
  MOZ_ASSERT(state_ == State::Start);

  // Evaluate the expression being iterated. The forHeadExpr should use a
  // distinct TDZCheckCache to evaluate since (abstractly) it runs in its
  // own LexicalEnvironment.
  tdzCacheForIteratedValue_.emplace(bce_);

#ifdef DEBUG
  state_ = State::Iterated;
#endif
  return true;
}

bool ForOfEmitter::emitInitialize(const Maybe<uint32_t>& forPos) {
  MOZ_ASSERT(state_ == State::Iterated);

  tdzCacheForIteratedValue_.reset();

  if (iterKind_ == IteratorKind::Async) {
    if (!bce_->emitAsyncIterator()) {
      //            [stack] NEXT ITER
      return false;
    }
  } else {
    if (!bce_->emitIterator()) {
      //            [stack] NEXT ITER
      return false;
    }
  }

  int32_t iterDepth = bce_->bytecodeSection().stackDepth();

  // For-of loops have the iterator next method, the iterator itself, and
  // the result.value on the stack.
  // Push an undefined to balance the stack.
  if (!bce_->emit1(JSOP_UNDEFINED)) {
    //              [stack] NEXT ITER UNDEF
    return false;
  }

  loopInfo_.emplace(bce_, iterDepth, allowSelfHostedIter_, iterKind_);

  // Annotate so IonMonkey can find the loop-closing jump.
  if (!bce_->newSrcNote(SRC_FOR_OF, &noteIndex_)) {
    return false;
  }

  if (!loopInfo_->emitEntryJump(bce_)) {
    //              [stack] NEXT ITER UNDEF
    return false;
  }

  if (!loopInfo_->emitLoopHead(bce_, Nothing())) {
    //              [stack] NEXT ITER UNDEF
    return false;
  }

  // If the loop had an escaping lexical declaration, replace the current
  // environment with an dead zoned one to implement TDZ semantics.
  if (headLexicalEmitterScope_) {
    // The environment chain only includes an environment for the for-of
    // loop head *if* a scope binding is captured, thereby requiring
    // recreation each iteration. If a lexical scope exists for the head,
    // it must be the innermost one. If that scope has closed-over
    // bindings inducing an environment, recreate the current environment.
    MOZ_ASSERT(headLexicalEmitterScope_ == bce_->innermostEmitterScope());
    MOZ_ASSERT(headLexicalEmitterScope_->scope(bce_)->kind() ==
               ScopeKind::Lexical);

    if (headLexicalEmitterScope_->hasEnvironment()) {
      if (!bce_->emit1(JSOP_RECREATELEXICALENV)) {
        //          [stack] NEXT ITER UNDEF
        return false;
      }
    }

    // For uncaptured bindings, put them back in TDZ.
    if (!headLexicalEmitterScope_->deadZoneFrameSlots(bce_)) {
      return false;
    }
  }

#ifdef DEBUG
  loopDepth_ = bce_->bytecodeSection().stackDepth();
#endif

  // Make sure this code is attributed to the "for".
  if (forPos) {
    if (!bce_->updateSourceCoordNotes(*forPos)) {
      return false;
    }
  }

  if (!bce_->emit1(JSOP_POP)) {
    //              [stack] NEXT ITER
    return false;
  }
  if (!bce_->emit1(JSOP_DUP2)) {
    //              [stack] NEXT ITER NEXT ITER
    return false;
  }

  if (!bce_->emitIteratorNext(forPos, iterKind_, allowSelfHostedIter_)) {
    //              [stack] NEXT ITER RESULT
    return false;
  }

  if (!bce_->emit1(JSOP_DUP)) {
    //              [stack] NEXT ITER RESULT RESULT
    return false;
  }
  if (!bce_->emitAtomOp(bce_->cx->names().done, JSOP_GETPROP)) {
    //              [stack] NEXT ITER RESULT DONE
    return false;
  }

  InternalIfEmitter ifDone(bce_);

  if (!ifDone.emitThen()) {
    //              [stack] NEXT ITER RESULT
    return false;
  }

  // Remove RESULT from the stack to release it.
  if (!bce_->emit1(JSOP_POP)) {
    //              [stack] NEXT ITER
    return false;
  }
  if (!bce_->emit1(JSOP_UNDEFINED)) {
    //              [stack] NEXT ITER UNDEF
    return false;
  }

  // If the iteration is done, leave loop here, instead of the branch at
  // the end of the loop.
  if (!loopInfo_->emitSpecialBreakForDone(bce_)) {
    //              [stack] NEXT ITER UNDEF
    return false;
  }

  if (!ifDone.emitEnd()) {
    //              [stack] NEXT ITER RESULT
    return false;
  }

  // Emit code to assign result.value to the iteration variable.
  //
  // Note that ES 13.7.5.13, step 5.c says getting result.value does not
  // call IteratorClose, so start JSTRY_ITERCLOSE after the GETPROP.
  if (!bce_->emitAtomOp(bce_->cx->names().value, JSOP_GETPROP)) {
    //              [stack] NEXT ITER VALUE
    return false;
  }

  if (!loopInfo_->emitBeginCodeNeedingIteratorClose(bce_)) {
    return false;
  }

#ifdef DEBUG
  state_ = State::Initialize;
#endif
  return true;
}

bool ForOfEmitter::emitBody() {
  MOZ_ASSERT(state_ == State::Initialize);

  MOZ_ASSERT(bce_->bytecodeSection().stackDepth() == loopDepth_,
             "the stack must be balanced around the initializing "
             "operation");

  // Remove VALUE from the stack to release it.
  if (!bce_->emit1(JSOP_POP)) {
    //              [stack] NEXT ITER
    return false;
  }
  if (!bce_->emit1(JSOP_UNDEFINED)) {
    //              [stack] NEXT ITER UNDEF
    return false;
  }

#ifdef DEBUG
  state_ = State::Body;
#endif
  return true;
}

bool ForOfEmitter::emitEnd(const Maybe<uint32_t>& iteratedPos) {
  MOZ_ASSERT(state_ == State::Body);

  MOZ_ASSERT(bce_->bytecodeSection().stackDepth() == loopDepth_,
             "the stack must be balanced around the for-of body");

  if (!loopInfo_->emitEndCodeNeedingIteratorClose(bce_)) {
    return false;
  }

  loopInfo_->setContinueTarget(bce_->bytecodeSection().offset());

  // We use the iterated value's position to attribute JSOP_LOOPENTRY,
  // which corresponds to the iteration protocol.
  // This is a bit misleading for 2nd and later iterations and might need
  // some fix (bug 1482003).
  if (!loopInfo_->emitLoopEntry(bce_, iteratedPos)) {
    return false;
  }

  if (!bce_->emit1(JSOP_FALSE)) {
    //              [stack] NEXT ITER UNDEF FALSE
    return false;
  }
  if (!loopInfo_->emitLoopEnd(bce_, JSOP_IFEQ)) {
    //              [stack] NEXT ITER UNDEF
    return false;
  }

  MOZ_ASSERT(bce_->bytecodeSection().stackDepth() == loopDepth_);

  // Let Ion know where the closing jump of this loop is.
  if (!bce_->setSrcNoteOffset(noteIndex_, SrcNote::ForOf::BackJumpOffset,
                              loopInfo_->loopEndOffsetFromEntryJump())) {
    return false;
  }

  if (!loopInfo_->patchBreaksAndContinues(bce_)) {
    return false;
  }

  if (!bce_->addTryNote(JSTRY_FOR_OF, bce_->bytecodeSection().stackDepth(),
                        loopInfo_->headOffset(),
                        loopInfo_->breakTargetOffset())) {
    return false;
  }

  if (!bce_->emitPopN(3)) {
    //              [stack]
    return false;
  }

  loopInfo_.reset();

#ifdef DEBUG
  state_ = State::End;
#endif
  return true;
}

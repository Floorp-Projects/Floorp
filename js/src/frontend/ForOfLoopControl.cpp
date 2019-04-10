/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/ForOfLoopControl.h"

#include "frontend/BytecodeEmitter.h"
#include "frontend/EmitterScope.h"
#include "frontend/IfEmitter.h"

using namespace js;
using namespace js::frontend;

ForOfLoopControl::ForOfLoopControl(BytecodeEmitter* bce, int32_t iterDepth,
                                   bool allowSelfHosted, IteratorKind iterKind)
    : LoopControl(bce, StatementKind::ForOfLoop),
      iterDepth_(iterDepth),
      numYieldsAtBeginCodeNeedingIterClose_(UINT32_MAX),
      allowSelfHosted_(allowSelfHosted),
      iterKind_(iterKind) {}

bool ForOfLoopControl::emitBeginCodeNeedingIteratorClose(BytecodeEmitter* bce) {
  tryCatch_.emplace(bce, TryEmitter::Kind::TryCatch,
                    TryEmitter::ControlKind::NonSyntactic);

  if (!tryCatch_->emitTry()) {
    return false;
  }

  MOZ_ASSERT(numYieldsAtBeginCodeNeedingIterClose_ == UINT32_MAX);
  numYieldsAtBeginCodeNeedingIterClose_ = bce->bytecodeSection().numYields();

  return true;
}

bool ForOfLoopControl::emitEndCodeNeedingIteratorClose(BytecodeEmitter* bce) {
  if (!tryCatch_->emitCatch()) {
    //              [stack] ITER ...
    return false;
  }

  if (!bce->emit1(JSOP_EXCEPTION)) {
    //              [stack] ITER ... EXCEPTION
    return false;
  }
  unsigned slotFromTop = bce->bytecodeSection().stackDepth() - iterDepth_;
  if (!bce->emitDupAt(slotFromTop)) {
    //              [stack] ITER ... EXCEPTION ITER
    return false;
  }

  // If ITER is undefined, it means the exception is thrown by
  // IteratorClose for non-local jump, and we should't perform
  // IteratorClose again here.
  if (!bce->emit1(JSOP_UNDEFINED)) {
    //              [stack] ITER ... EXCEPTION ITER UNDEF
    return false;
  }
  if (!bce->emit1(JSOP_STRICTNE)) {
    //              [stack] ITER ... EXCEPTION NE
    return false;
  }

  InternalIfEmitter ifIteratorIsNotClosed(bce);
  if (!ifIteratorIsNotClosed.emitThen()) {
    //              [stack] ITER ... EXCEPTION
    return false;
  }

  MOZ_ASSERT(slotFromTop ==
             unsigned(bce->bytecodeSection().stackDepth() - iterDepth_));
  if (!bce->emitDupAt(slotFromTop)) {
    //              [stack] ITER ... EXCEPTION ITER
    return false;
  }
  if (!emitIteratorCloseInInnermostScopeWithTryNote(bce,
                                                    CompletionKind::Throw)) {
    return false;  // ITER ... EXCEPTION
  }

  if (!ifIteratorIsNotClosed.emitEnd()) {
    //              [stack] ITER ... EXCEPTION
    return false;
  }

  if (!bce->emit1(JSOP_THROW)) {
    //              [stack] ITER ...
    return false;
  }

  // If any yields were emitted, then this for-of loop is inside a star
  // generator and must handle the case of Generator.return. Like in
  // yield*, it is handled with a finally block.
  uint32_t numYieldsEmitted = bce->bytecodeSection().numYields();
  if (numYieldsEmitted > numYieldsAtBeginCodeNeedingIterClose_) {
    if (!tryCatch_->emitFinally()) {
      return false;
    }

    InternalIfEmitter ifGeneratorClosing(bce);
    if (!bce->emit1(JSOP_ISGENCLOSING)) {
      //            [stack] ITER ... FTYPE FVALUE CLOSING
      return false;
    }
    if (!ifGeneratorClosing.emitThen()) {
      //            [stack] ITER ... FTYPE FVALUE
      return false;
    }
    if (!bce->emitDupAt(slotFromTop + 1)) {
      //            [stack] ITER ... FTYPE FVALUE ITER
      return false;
    }
    if (!emitIteratorCloseInInnermostScopeWithTryNote(bce,
                                                      CompletionKind::Normal)) {
      //            [stack] ITER ... FTYPE FVALUE
      return false;
    }
    if (!ifGeneratorClosing.emitEnd()) {
      //            [stack] ITER ... FTYPE FVALUE
      return false;
    }
  }

  if (!tryCatch_->emitEnd()) {
    return false;
  }

  tryCatch_.reset();
  numYieldsAtBeginCodeNeedingIterClose_ = UINT32_MAX;

  return true;
}

bool ForOfLoopControl::emitIteratorCloseInInnermostScopeWithTryNote(
    BytecodeEmitter* bce,
    CompletionKind completionKind /* = CompletionKind::Normal */) {
  ptrdiff_t start = bce->bytecodeSection().offset();
  if (!emitIteratorCloseInScope(bce, *bce->innermostEmitterScope(),
                                completionKind)) {
    return false;
  }
  ptrdiff_t end = bce->bytecodeSection().offset();
  return bce->addTryNote(JSTRY_FOR_OF_ITERCLOSE, 0, start, end);
}

bool ForOfLoopControl::emitIteratorCloseInScope(
    BytecodeEmitter* bce, EmitterScope& currentScope,
    CompletionKind completionKind /* = CompletionKind::Normal */) {
  return bce->emitIteratorCloseInScope(currentScope, iterKind_, completionKind,
                                       allowSelfHosted_);
}

// Since we're in the middle of emitting code that will leave
// |bce->innermostEmitterScope()|, passing the innermost emitter scope to
// emitIteratorCloseInScope and looking up .generator there would be very,
// very wrong.  We'd find .generator in the function environment, and we'd
// compute a NameLocation with the correct slot, but we'd compute a
// hop-count to the function environment that was too big.  At runtime we'd
// either crash, or we'd find a user-controllable value in that slot, and
// Very Bad Things would ensue as we reinterpreted that value as an
// iterator.
bool ForOfLoopControl::emitPrepareForNonLocalJumpFromScope(
    BytecodeEmitter* bce, EmitterScope& currentScope, bool isTarget,
    ptrdiff_t* tryNoteStart) {
  // Pop unnecessary value from the stack.  Effectively this means
  // leaving try-catch block.  However, the performing IteratorClose can
  // reach the depth for try-catch, and effectively re-enter the
  // try-catch block.
  if (!bce->emit1(JSOP_POP)) {
    //              [stack] NEXT ITER
    return false;
  }

  // Pop the iterator's next method.
  if (!bce->emit1(JSOP_SWAP)) {
    //              [stack] ITER NEXT
    return false;
  }
  if (!bce->emit1(JSOP_POP)) {
    //              [stack] ITER
    return false;
  }

  // Clear ITER slot on the stack to tell catch block to avoid performing
  // IteratorClose again.
  if (!bce->emit1(JSOP_UNDEFINED)) {
    //              [stack] ITER UNDEF
    return false;
  }
  if (!bce->emit1(JSOP_SWAP)) {
    //              [stack] UNDEF ITER
    return false;
  }

  *tryNoteStart = bce->bytecodeSection().offset();
  if (!emitIteratorCloseInScope(bce, currentScope, CompletionKind::Normal)) {
    //              [stack] UNDEF
    return false;
  }

  if (isTarget) {
    // At the level of the target block, there's bytecode after the
    // loop that will pop the next method, the iterator, and the
    // value, so push two undefineds to balance the stack.
    if (!bce->emit1(JSOP_UNDEFINED)) {
      //            [stack] UNDEF UNDEF
      return false;
    }
    if (!bce->emit1(JSOP_UNDEFINED)) {
      //            [stack] UNDEF UNDEF UNDEF
      return false;
    }
  } else {
    if (!bce->emit1(JSOP_POP)) {
      //            [stack]
      return false;
    }
  }

  return true;
}

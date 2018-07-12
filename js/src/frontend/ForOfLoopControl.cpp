/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/ForOfLoopControl.h"

#include "frontend/BytecodeEmitter.h"
#include "frontend/EmitterScope.h"
#include "frontend/IfEmitter.h"

using namespace js;
using namespace js::frontend;

ForOfLoopControl::ForOfLoopControl(BytecodeEmitter* bce, int32_t iterDepth, bool allowSelfHosted,
                                   IteratorKind iterKind)
  : LoopControl(bce, StatementKind::ForOfLoop),
    iterDepth_(iterDepth),
    numYieldsAtBeginCodeNeedingIterClose_(UINT32_MAX),
    allowSelfHosted_(allowSelfHosted),
    iterKind_(iterKind)
{}

bool
ForOfLoopControl::emitBeginCodeNeedingIteratorClose(BytecodeEmitter* bce)
{
    tryCatch_.emplace(bce, TryEmitter::Kind::TryCatch, TryEmitter::ControlKind::NonSyntactic);

    if (!tryCatch_->emitTry())
        return false;

    MOZ_ASSERT(numYieldsAtBeginCodeNeedingIterClose_ == UINT32_MAX);
    numYieldsAtBeginCodeNeedingIterClose_ = bce->yieldAndAwaitOffsetList.numYields;

    return true;
}

bool
ForOfLoopControl::emitEndCodeNeedingIteratorClose(BytecodeEmitter* bce)
{
    if (!tryCatch_->emitCatch())              // ITER ...
        return false;

    if (!bce->emit1(JSOP_EXCEPTION))          // ITER ... EXCEPTION
        return false;
    unsigned slotFromTop = bce->stackDepth - iterDepth_;
    if (!bce->emitDupAt(slotFromTop))         // ITER ... EXCEPTION ITER
        return false;

    // If ITER is undefined, it means the exception is thrown by
    // IteratorClose for non-local jump, and we should't perform
    // IteratorClose again here.
    if (!bce->emit1(JSOP_UNDEFINED))          // ITER ... EXCEPTION ITER UNDEF
        return false;
    if (!bce->emit1(JSOP_STRICTNE))           // ITER ... EXCEPTION NE
        return false;

    InternalIfEmitter ifIteratorIsNotClosed(bce);
    if (!ifIteratorIsNotClosed.emitThen())    // ITER ... EXCEPTION
        return false;

    MOZ_ASSERT(slotFromTop == unsigned(bce->stackDepth - iterDepth_));
    if (!bce->emitDupAt(slotFromTop))         // ITER ... EXCEPTION ITER
        return false;
    if (!emitIteratorCloseInInnermostScope(bce, CompletionKind::Throw))
        return false;                         // ITER ... EXCEPTION

    if (!ifIteratorIsNotClosed.emitEnd())     // ITER ... EXCEPTION
        return false;

    if (!bce->emit1(JSOP_THROW))              // ITER ...
        return false;

    // If any yields were emitted, then this for-of loop is inside a star
    // generator and must handle the case of Generator.return. Like in
    // yield*, it is handled with a finally block.
    uint32_t numYieldsEmitted = bce->yieldAndAwaitOffsetList.numYields;
    if (numYieldsEmitted > numYieldsAtBeginCodeNeedingIterClose_) {
        if (!tryCatch_->emitFinally())
            return false;

        InternalIfEmitter ifGeneratorClosing(bce);
        if (!bce->emit1(JSOP_ISGENCLOSING))   // ITER ... FTYPE FVALUE CLOSING
            return false;
        if (!ifGeneratorClosing.emitThen())   // ITER ... FTYPE FVALUE
            return false;
        if (!bce->emitDupAt(slotFromTop + 1)) // ITER ... FTYPE FVALUE ITER
            return false;
        if (!emitIteratorCloseInInnermostScope(bce, CompletionKind::Normal))
            return false;                     // ITER ... FTYPE FVALUE
        if (!ifGeneratorClosing.emitEnd())    // ITER ... FTYPE FVALUE
            return false;
    }

    if (!tryCatch_->emitEnd())
        return false;

    tryCatch_.reset();
    numYieldsAtBeginCodeNeedingIterClose_ = UINT32_MAX;

    return true;
}

bool
ForOfLoopControl::emitIteratorCloseInInnermostScope(BytecodeEmitter* bce,
                                                    CompletionKind completionKind /* = CompletionKind::Normal */)
{
    return emitIteratorCloseInScope(bce,  *bce->innermostEmitterScope(), completionKind);
}

bool
ForOfLoopControl::emitIteratorCloseInScope(BytecodeEmitter* bce,
                                           EmitterScope& currentScope,
                                           CompletionKind completionKind /* = CompletionKind::Normal */)
{
    ptrdiff_t start = bce->offset();
    if (!bce->emitIteratorCloseInScope(currentScope, iterKind_, completionKind,
                                       allowSelfHosted_))
    {
        return false;
    }
    ptrdiff_t end = bce->offset();
    return bce->tryNoteList.append(JSTRY_FOR_OF_ITERCLOSE, 0, start, end);
}

bool
ForOfLoopControl::emitPrepareForNonLocalJumpFromScope(BytecodeEmitter* bce,
                                                      EmitterScope& currentScope,
                                                      bool isTarget)
{
    // Pop unnecessary value from the stack.  Effectively this means
    // leaving try-catch block.  However, the performing IteratorClose can
    // reach the depth for try-catch, and effectively re-enter the
    // try-catch block.
    if (!bce->emit1(JSOP_POP))                        // NEXT ITER
        return false;

    // Pop the iterator's next method.
    if (!bce->emit1(JSOP_SWAP))                       // ITER NEXT
        return false;
    if (!bce->emit1(JSOP_POP))                        // ITER
        return false;

    // Clear ITER slot on the stack to tell catch block to avoid performing
    // IteratorClose again.
    if (!bce->emit1(JSOP_UNDEFINED))                  // ITER UNDEF
        return false;
    if (!bce->emit1(JSOP_SWAP))                       // UNDEF ITER
        return false;

    if (!emitIteratorCloseInScope(bce, currentScope, CompletionKind::Normal)) // UNDEF
        return false;

    if (isTarget) {
        // At the level of the target block, there's bytecode after the
        // loop that will pop the next method, the iterator, and the
        // value, so push two undefineds to balance the stack.
        if (!bce->emit1(JSOP_UNDEFINED))              // UNDEF UNDEF
            return false;
        if (!bce->emit1(JSOP_UNDEFINED))              // UNDEF UNDEF UNDEF
            return false;
    } else {
        if (!bce->emit1(JSOP_POP))                    //
            return false;
    }

    return true;
}

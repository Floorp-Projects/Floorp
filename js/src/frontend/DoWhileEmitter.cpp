/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/DoWhileEmitter.h"

#include "frontend/BytecodeEmitter.h"
#include "frontend/SourceNotes.h"
#include "vm/Opcodes.h"

using namespace js;
using namespace js::frontend;

using mozilla::Maybe;
using mozilla::Nothing;

DoWhileEmitter::DoWhileEmitter(BytecodeEmitter* bce)
  : bce_(bce)
{}

bool
DoWhileEmitter::emitBody(const Maybe<uint32_t>& doPos, const Maybe<uint32_t>& bodyPos)
{
    MOZ_ASSERT(state_ == State::Start);

    // Ensure that the column of the 'do' is set properly.
    if (doPos) {
        if (!bce_->updateSourceCoordNotes(*doPos))
            return false;
    }

    // Emit an annotated nop so IonBuilder can recognize the 'do' loop.
    if (!bce_->newSrcNote(SRC_WHILE, &noteIndex_))
        return false;
    if (!bce_->emit1(JSOP_NOP))
        return false;

    if (!bce_->newSrcNote(SRC_WHILE, &noteIndex2_))
        return false;

    loopInfo_.emplace(bce_, StatementKind::DoLoop);

    if (!loopInfo_->emitLoopHead(bce_, bodyPos))
        return false;

    if (!loopInfo_->emitLoopEntry(bce_, Nothing()))
        return false;

#ifdef DEBUG
    state_ = State::Body;
#endif
    return true;
}

bool
DoWhileEmitter::emitCond()
{
    MOZ_ASSERT(state_ == State::Body);

    if (!loopInfo_->emitContinueTarget(bce_))
        return false;

#ifdef DEBUG
    state_ = State::Cond;
#endif
    return true;
}

bool
DoWhileEmitter::emitEnd()
{
    MOZ_ASSERT(state_ == State::Cond);

    if (!loopInfo_->emitLoopEnd(bce_, JSOP_IFNE))
        return false;

    if (!bce_->tryNoteList.append(JSTRY_LOOP, bce_->stackDepth, loopInfo_->headOffset(),
                                  loopInfo_->breakTargetOffset()))
    {
        return false;
    }

    // Update the annotations with the update and back edge positions, for
    // IonBuilder.
    //
    // Be careful: We must set noteIndex2_ before_ noteIndex in case the
    // noteIndex_ note gets bigger.  Otherwise noteIndex2_ can point wrong
    // position.
    if (!bce_->setSrcNoteOffset(noteIndex2_, SrcNote::DoWhile2::BackJumpOffset,
                                loopInfo_->loopEndOffsetFromLoopHead()))
    {
        return false;
    }
    // +1 for NOP in emitBody.
    if (!bce_->setSrcNoteOffset(noteIndex_, SrcNote::DoWhile1::CondOffset,
                                loopInfo_->continueTargetOffsetFromLoopHead() + 1))
    {
        return false;
    }

    if (!loopInfo_->patchBreaksAndContinues(bce_))
        return false;

    loopInfo_.reset();

#ifdef DEBUG
    state_ = State::End;
#endif
    return true;
}

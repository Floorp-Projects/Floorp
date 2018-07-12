/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/IfEmitter.h"

#include "frontend/BytecodeEmitter.h"
#include "frontend/SourceNotes.h"
#include "vm/Opcodes.h"

using namespace js;
using namespace js::frontend;

IfEmitter::IfEmitter(BytecodeEmitter* bce, Kind kind)
  : bce_(bce),
    thenDepth_(0),
    kind_(kind)
#ifdef DEBUG
  , pushed_(0),
    calculatedPushed_(false),
    state_(State::Start)
#endif
{}

IfEmitter::IfEmitter(BytecodeEmitter* bce)
  : IfEmitter(bce, Kind::MayContainLexicalAccessInBranch)
{}

bool
IfEmitter::emitIfInternal(SrcNoteType type)
{
    MOZ_ASSERT_IF(state_ == State::ElseIf, tdzCache_.isSome());
    MOZ_ASSERT_IF(state_ != State::ElseIf, tdzCache_.isNothing());

    // The end of TDZCheckCache for cond for else-if.
    if (kind_ == Kind::MayContainLexicalAccessInBranch)
        tdzCache_.reset();

    // Emit an annotated branch-if-false around the then part.
    if (!bce_->newSrcNote(type))
        return false;
    if (!bce_->emitJump(JSOP_IFEQ, &jumpAroundThen_))
        return false;

    // To restore stack depth in else part, save depth of the then part.
#ifdef DEBUG
    // If DEBUG, this is also necessary to calculate |pushed_|.
    thenDepth_ = bce_->stackDepth;
#else
    if (type == SRC_COND || type == SRC_IF_ELSE)
        thenDepth_ = bce_->stackDepth;
#endif

    // Enclose then-branch with TDZCheckCache.
    if (kind_ == Kind::MayContainLexicalAccessInBranch)
        tdzCache_.emplace(bce_);

    return true;
}

void
IfEmitter::calculateOrCheckPushed()
{
#ifdef DEBUG
    if (!calculatedPushed_) {
        pushed_ = bce_->stackDepth - thenDepth_;
        calculatedPushed_ = true;
    } else {
        MOZ_ASSERT(pushed_ == bce_->stackDepth - thenDepth_);
    }
#endif
}

bool
IfEmitter::emitThen()
{
    MOZ_ASSERT(state_ == State::Start || state_ == State::ElseIf);
    if (!emitIfInternal(SRC_IF))
        return false;

#ifdef DEBUG
    state_ = State::Then;
#endif
    return true;
}

bool
IfEmitter::emitCond()
{
    MOZ_ASSERT(state_ == State::Start);
    if (!emitIfInternal(SRC_COND))
        return false;

#ifdef DEBUG
    state_ = State::Cond;
#endif
    return true;
}

bool
IfEmitter::emitThenElse()
{
    MOZ_ASSERT(state_ == State::Start || state_ == State::ElseIf);
    if (!emitIfInternal(SRC_IF_ELSE))
        return false;

#ifdef DEBUG
    state_ = State::ThenElse;
#endif
    return true;
}

bool
IfEmitter::emitElseInternal()
{
    calculateOrCheckPushed();

    // The end of TDZCheckCache for then-clause.
    if (kind_ == Kind::MayContainLexicalAccessInBranch) {
        MOZ_ASSERT(tdzCache_.isSome());
        tdzCache_.reset();
    }

    // Emit a jump from the end of our then part around the else part. The
    // patchJumpsToTarget call at the bottom of this function will fix up
    // the offset with jumpsAroundElse value.
    if (!bce_->emitJump(JSOP_GOTO, &jumpsAroundElse_))
        return false;

    // Ensure the branch-if-false comes here, then emit the else.
    if (!bce_->emitJumpTargetAndPatch(jumpAroundThen_))
        return false;

    // Clear jumpAroundThen_ offset, to tell emitEnd there was an else part.
    jumpAroundThen_ = JumpList();

    // Restore stack depth of the then part.
    bce_->stackDepth = thenDepth_;
#ifdef DEBUG
    state_ = State::Else;
#endif
    return true;
}

bool
IfEmitter::emitElse()
{
    MOZ_ASSERT(state_ == State::ThenElse || state_ == State::Cond);

    if (!emitElseInternal())
        return false;

    // Enclose else-branch with TDZCheckCache.
    if (kind_ == Kind::MayContainLexicalAccessInBranch)
        tdzCache_.emplace(bce_);

#ifdef DEBUG
    state_ = State::Else;
#endif
    return true;
}

bool
IfEmitter::emitElseIf()
{
    MOZ_ASSERT(state_ == State::ThenElse);

    if (!emitElseInternal())
        return false;

    // Enclose cond for else-if with TDZCheckCache.
    if (kind_ == Kind::MayContainLexicalAccessInBranch)
        tdzCache_.emplace(bce_);

#ifdef DEBUG
    state_ = State::ElseIf;
#endif
    return true;
}

bool
IfEmitter::emitEnd()
{
    MOZ_ASSERT(state_ == State::Then || state_ == State::Else);
    // If there was an else part for the last branch, jumpAroundThen_ is
    // already fixed up when emitting the else part.
    MOZ_ASSERT_IF(state_ == State::Then, jumpAroundThen_.offset != -1);
    MOZ_ASSERT_IF(state_ == State::Else, jumpAroundThen_.offset == -1);

    // The end of TDZCheckCache for then or else-clause.
    if (kind_ == Kind::MayContainLexicalAccessInBranch) {
        MOZ_ASSERT(tdzCache_.isSome());
        tdzCache_.reset();
    }

    calculateOrCheckPushed();

    if (jumpAroundThen_.offset != -1) {
        // No else part for the last branch, fixup the branch-if-false to
        // come here.
        if (!bce_->emitJumpTargetAndPatch(jumpAroundThen_))
            return false;
    }

    // Patch all the jumps around else parts.
    if (!bce_->emitJumpTargetAndPatch(jumpsAroundElse_))
        return false;

#ifdef DEBUG
    state_ = State::End;
#endif
    return true;
}

InternalIfEmitter::InternalIfEmitter(BytecodeEmitter* bce)
  : IfEmitter(bce, Kind::NoLexicalAccessInBranch)
{}

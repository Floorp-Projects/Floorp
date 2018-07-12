/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/BytecodeControlStructures.h"

#include "frontend/BytecodeEmitter.h"
#include "frontend/EmitterScope.h"

using namespace js;
using namespace js::frontend;

NestableControl::NestableControl(BytecodeEmitter* bce, StatementKind kind)
  : Nestable<NestableControl>(&bce->innermostNestableControl),
    kind_(kind),
    emitterScope_(bce->innermostEmitterScopeNoCheck())
{}

BreakableControl::BreakableControl(BytecodeEmitter* bce, StatementKind kind)
  : NestableControl(bce, kind)
{
    MOZ_ASSERT(is<BreakableControl>());
}

bool
BreakableControl::patchBreaks(BytecodeEmitter* bce)
{
    return bce->emitJumpTargetAndPatch(breaks);
}

LabelControl::LabelControl(BytecodeEmitter* bce, JSAtom* label, ptrdiff_t startOffset)
  : BreakableControl(bce, StatementKind::Label),
    label_(bce->cx, label),
    startOffset_(startOffset)
{}

LoopControl::LoopControl(BytecodeEmitter* bce, StatementKind loopKind)
  : BreakableControl(bce, loopKind),
    tdzCache_(bce),
    continueTarget({ -1 })
{
    MOZ_ASSERT(is<LoopControl>());

    LoopControl* enclosingLoop = findNearest<LoopControl>(enclosing());

    stackDepth_ = bce->stackDepth;
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

bool
LoopControl::emitSpecialBreakForDone(BytecodeEmitter* bce)
{
    // This doesn't pop stack values, nor handle any other controls.
    // Should be called on the toplevel of the loop.
    MOZ_ASSERT(bce->stackDepth == stackDepth_);
    MOZ_ASSERT(bce->innermostNestableControl == this);

    if (!bce->newSrcNote(SRC_BREAK))
        return false;
    if (!bce->emitJump(JSOP_GOTO, &breaks))
        return false;

    return true;
}

bool
LoopControl::patchBreaksAndContinues(BytecodeEmitter* bce)
{
    MOZ_ASSERT(continueTarget.offset != -1);
    if (!patchBreaks(bce))
        return false;
    bce->patchJumpsToTarget(continues, continueTarget);
    return true;
}

TryFinallyControl::TryFinallyControl(BytecodeEmitter* bce, StatementKind kind)
  : NestableControl(bce, kind),
    emittingSubroutine_(false)
{
    MOZ_ASSERT(is<TryFinallyControl>());
}

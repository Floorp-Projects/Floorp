/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_BytecodeControlStructures_h
#define frontend_BytecodeControlStructures_h

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"

#include <stddef.h>
#include <stdint.h>

#include "ds/Nestable.h"
#include "frontend/JumpList.h"
#include "frontend/SharedContext.h"
#include "frontend/TDZCheckCache.h"
#include "gc/Rooting.h"
#include "vm/BytecodeUtil.h"
#include "vm/StringType.h"

namespace js {
namespace frontend {

struct BytecodeEmitter;
class EmitterScope;

class NestableControl : public Nestable<NestableControl>
{
    StatementKind kind_;

    // The innermost scope when this was pushed.
    EmitterScope* emitterScope_;

  protected:
    NestableControl(BytecodeEmitter* bce, StatementKind kind);

  public:
    using Nestable<NestableControl>::enclosing;
    using Nestable<NestableControl>::findNearest;

    StatementKind kind() const {
        return kind_;
    }

    EmitterScope* emitterScope() const {
        return emitterScope_;
    }

    template <typename T> bool is() const;

    template <typename T> T& as() {
        MOZ_ASSERT(this->is<T>());
        return static_cast<T&>(*this);
    }
};

class BreakableControl : public NestableControl
{
  public:
    // Offset of the last break.
    JumpList breaks;

    BreakableControl(BytecodeEmitter* bce, StatementKind kind);

    MOZ_MUST_USE bool patchBreaks(BytecodeEmitter* bce);
};
template <>
inline bool
NestableControl::is<BreakableControl>() const
{
    return StatementKindIsUnlabeledBreakTarget(kind_) || kind_ == StatementKind::Label;
}

class LabelControl : public BreakableControl
{
    RootedAtom label_;

    // The code offset when this was pushed. Used for effectfulness checking.
    ptrdiff_t startOffset_;

  public:
    LabelControl(BytecodeEmitter* bce, JSAtom* label, ptrdiff_t startOffset);

    HandleAtom label() const {
        return label_;
    }

    ptrdiff_t startOffset() const {
        return startOffset_;
    }
};
template <>
inline bool
NestableControl::is<LabelControl>() const
{
    return kind_ == StatementKind::Label;
}

class LoopControl : public BreakableControl
{
    // Loops' children are emitted in dominance order, so they can always
    // have a TDZCheckCache.
    TDZCheckCache tdzCache_;

    // Here's the basic structure of a loop:
    //
    //     # Entry jump
    //     JSOP_GOTO entry
    //
    //   head:
    //     JSOP_LOOPHEAD
    //     {loop body after branch}
    //
    //   entry:
    //     JSOP_ENTRY
    //     {loop body before branch}
    //
    //     # Loop end, backward jump
    //     JSOP_GOTO/JSOP_IFNE head
    //
    //   breakTarget:
    //
    // `continueTarget` can be placed in arbitrary place by calling
    // `setContinueTarget` or `emitContinueTarget` (see comment above them for
    // more details).

    // The offset of backward jump at the end of loop.
    ptrdiff_t loopEndOffset_ = -1;

    // The jump into JSOP_LOOPENTRY.
    JumpList entryJump_;

    // The bytecode offset of JSOP_LOOPHEAD.
    JumpTarget head_ = { -1 };

    // The target of break statement jumps.
    JumpTarget breakTarget_ = { -1 };

    // The target of continue statement jumps, e.g., the update portion of a
    // for(;;) loop.
    JumpTarget continueTarget_ = { -1 };

    // Stack depth when this loop was pushed on the control stack.
    int32_t stackDepth_;

    // The loop nesting depth. Used as a hint to Ion.
    uint32_t loopDepth_;

    // Can we OSR into Ion from here? True unless there is non-loop state on the stack.
    bool canIonOsr_;

  public:
    // Offset of the last continue in the loop.
    JumpList continues;

    LoopControl(BytecodeEmitter* bce, StatementKind loopKind);

    ptrdiff_t headOffset() const {
        return head_.offset;
    }
    ptrdiff_t loopEndOffset() const {
        return loopEndOffset_;
    }
    ptrdiff_t breakTargetOffset() const {
        return breakTarget_.offset;
    }
    ptrdiff_t continueTargetOffset() const {
        return continueTarget_.offset;
    }

    // The offset of the backward jump at the loop end from the loop's top, in
    // case there was an entry jump.
    ptrdiff_t loopEndOffsetFromEntryJump() const {
        return loopEndOffset_ - entryJump_.offset;
    }

    // The offset of the backward jump at the loop end from the loop's top, in
    // case there was no entry jump.
    ptrdiff_t loopEndOffsetFromLoopHead() const {
        return loopEndOffset_ - head_.offset;
    }

    // The offset of the continue target from the loop's top, in case there was
    // no entry jump.
    ptrdiff_t continueTargetOffsetFromLoopHead() const {
        return continueTarget_.offset - head_.offset;
    }

    // A continue target can be specified by the following 2 ways:
    //   * Use the existing JUMPTARGET by calling `setContinueTarget` with
    //     the offset of the JUMPTARGET
    //   * Generate a new JUMPTARGETby calling `emitContinueTarget`
    void setContinueTarget(ptrdiff_t offset) {
        continueTarget_.offset = offset;
    }
    MOZ_MUST_USE bool emitContinueTarget(BytecodeEmitter* bce);

    // Emit a jump to break target from the top level of the loop.
    MOZ_MUST_USE bool emitSpecialBreakForDone(BytecodeEmitter* bce);

    MOZ_MUST_USE bool emitEntryJump(BytecodeEmitter* bce);

    // `nextPos` is the offset in the source code for the character that
    // corresponds to the next instruction after JSOP_LOOPHEAD.
    // Can be Nothing() if not available.
    MOZ_MUST_USE bool emitLoopHead(BytecodeEmitter* bce,
                                   const mozilla::Maybe<uint32_t>& nextPos);

    // `nextPos` is the offset in the source code for the character that
    // corresponds to the next instruction after JSOP_LOOPENTRY.
    // Can be Nothing() if not available.
    MOZ_MUST_USE bool emitLoopEntry(BytecodeEmitter* bce,
                                    const mozilla::Maybe<uint32_t>& nextPos);

    MOZ_MUST_USE bool emitLoopEnd(BytecodeEmitter* bce, JSOp op);
    MOZ_MUST_USE bool patchBreaksAndContinues(BytecodeEmitter* bce);
};
template <>
inline bool
NestableControl::is<LoopControl>() const
{
    return StatementKindIsLoop(kind_);
}

class TryFinallyControl : public NestableControl
{
    bool emittingSubroutine_;

  public:
    // The subroutine when emitting a finally block.
    JumpList gosubs;

    TryFinallyControl(BytecodeEmitter* bce, StatementKind kind);

    void setEmittingSubroutine() {
        emittingSubroutine_ = true;
    }

    bool emittingSubroutine() const {
        return emittingSubroutine_;
    }
};
template <>
inline bool
NestableControl::is<TryFinallyControl>() const
{
    return kind_ == StatementKind::Try || kind_ == StatementKind::Finally;
}

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_BytecodeControlStructures_h */

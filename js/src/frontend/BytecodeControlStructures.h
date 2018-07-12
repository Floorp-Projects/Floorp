/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_BytecodeControlStructures_h
#define frontend_BytecodeControlStructures_h

#include "mozilla/Attributes.h"

#include <stddef.h>
#include <stdint.h>

#include "ds/Nestable.h"
#include "frontend/JumpList.h"
#include "frontend/SharedContext.h"
#include "frontend/TDZCheckCache.h"
#include "gc/Rooting.h"
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

    // Stack depth when this loop was pushed on the control stack.
    int32_t stackDepth_;

    // The loop nesting depth. Used as a hint to Ion.
    uint32_t loopDepth_;

    // Can we OSR into Ion from here? True unless there is non-loop state on the stack.
    bool canIonOsr_;

  public:
    // The target of continue statement jumps, e.g., the update portion of a
    // for(;;) loop.
    JumpTarget continueTarget;

    // Offset of the last continue in the loop.
    JumpList continues;

    LoopControl(BytecodeEmitter* bce, StatementKind loopKind);

    uint32_t loopDepth() const {
        return loopDepth_;
    }

    bool canIonOsr() const {
        return canIonOsr_;
    }

    MOZ_MUST_USE bool emitSpecialBreakForDone(BytecodeEmitter* bce);
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

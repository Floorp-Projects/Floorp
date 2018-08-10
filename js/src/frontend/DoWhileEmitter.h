/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_DoWhileEmitter_h
#define frontend_DoWhileEmitter_h

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"

#include <stdint.h>

#include "frontend/BytecodeControlStructures.h"

namespace js {
namespace frontend {

struct BytecodeEmitter;

// Class for emitting bytecode for do-while loop.
//
// Usage: (check for the return value is omitted for simplicity)
//
//   `do body while (cond);`
//     DoWhileEmitter doWhile(this);
//     doWhile.emitBody(Some(offset_of_do), Some(offset_of_body));
//     emit(body);
//     doWhile.emitCond();
//     emit(cond);
//     doWhile.emitEnd();
//
class MOZ_STACK_CLASS DoWhileEmitter
{
    BytecodeEmitter* bce_;

    // The source note indices for 2 SRC_WHILE's.
    // FIXME: Add SRC_DO_WHILE (bug 1477896).
    unsigned noteIndex_ = 0;
    unsigned noteIndex2_ = 0;

    mozilla::Maybe<LoopControl> loopInfo_;

#ifdef DEBUG
    // The state of this emitter.
    //
    // +-------+ emitBody +------+ emitCond +------+ emitEnd  +-----+
    // | Start |--------->| Body |--------->| Cond |--------->| End |
    // +-------+          +------+          +------+          +-----+
    enum class State {
        // The initial state.
        Start,

        // After calling emitBody.
        Body,

        // After calling emitCond.
        Cond,

        // After calling emitEnd.
        End
    };
    State state_ = State::Start;
#endif

  public:
    explicit DoWhileEmitter(BytecodeEmitter* bce);

    // Parameters are the offset in the source code for each character below:
    //
    //   do { ... } while ( x < 20 );
    //   ^  ^
    //   |  |
    //   |  bodyPos
    //   |
    //   doPos
    //
    // Can be Nothing() if not available.
    MOZ_MUST_USE bool emitBody(const mozilla::Maybe<uint32_t>& doPos,
                               const mozilla::Maybe<uint32_t>& bodyPos);
    MOZ_MUST_USE bool emitCond();
    MOZ_MUST_USE bool emitEnd();
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_DoWhileEmitter_h */

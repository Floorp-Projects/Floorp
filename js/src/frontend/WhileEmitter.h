/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_WhileEmitter_h
#define frontend_WhileEmitter_h

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"

#include <stdint.h>

#include "frontend/BytecodeControlStructures.h"
#include "frontend/TDZCheckCache.h"

namespace js {
namespace frontend {

struct BytecodeEmitter;

// Class for emitting bytecode for while loop.
//
// Usage: (check for the return value is omitted for simplicity)
//
//   `while (cond) body`
//     WhileEmitter wh(this);
//     wh.emitBody(Some(offset_of_while),
//                 Some(offset_of_body),
//                 Some(offset_of_end));
//     emit(body);
//     wh.emitCond(Some(offset_of_cond));
//     emit(cond);
//     wh.emitEnd();
//
class MOZ_STACK_CLASS WhileEmitter
{
    BytecodeEmitter* bce_;

    // The source note index for SRC_WHILE.
    unsigned noteIndex_ = 0;

    mozilla::Maybe<LoopControl> loopInfo_;

    // Cache for the loop body, which is enclosed by the cache in `loopInfo_`,
    // which is effectively for the loop condition.
    mozilla::Maybe<TDZCheckCache> tdzCacheForBody_;

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
    explicit WhileEmitter(BytecodeEmitter* bce);

    // Parameters are the offset in the source code for each character below:
    //
    //   while ( x < 20 ) { ... }
    //   ^       ^        ^     ^
    //   |       |        |     |
    //   |       |        |     endPos_
    //   |       |        |
    //   |       |        bodyPos_
    //   |       |
    //   |       condPos_
    //   |
    //   whilePos_
    //
    // Can be Nothing() if not available.
    MOZ_MUST_USE bool emitBody(const mozilla::Maybe<uint32_t>& whilePos,
                               const mozilla::Maybe<uint32_t>& bodyPos,
                               const mozilla::Maybe<uint32_t>& endPos);
    MOZ_MUST_USE bool emitCond(const mozilla::Maybe<uint32_t>& condPos);
    MOZ_MUST_USE bool emitEnd();
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_WhileEmitter_h */

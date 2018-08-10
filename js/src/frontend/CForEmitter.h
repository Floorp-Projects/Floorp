/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_CForEmitter_h
#define frontend_CForEmitter_h

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"

#include <stddef.h>
#include <stdint.h>

#include "frontend/BytecodeControlStructures.h"
#include "frontend/TDZCheckCache.h"

namespace js {
namespace frontend {

struct BytecodeEmitter;
class EmitterScope;

// Class for emitting bytecode for c-style for block.
//
// Usage: (check for the return value is omitted for simplicity)
//
//   `for (init; cond; update) body`
//     CForEmitter cfor(this, headLexicalEmitterScopeForLet or nullptr);
//     cfor.emitInit(Some(offset_of_init));
//     emit(init); // without pushing value
//     cfor.emitBody(CForEmitter::Cond::Present, Some(offset_of_body));
//     emit(body);
//     cfor.emitUpdate(CForEmitter::Update::Present, Some(offset_of_update)));
//     emit(update);
//     cfor.emitCond(Some(offset_of_for),
//                   Some(offset_of_cond),
//                   Some(offset_of_end));
//     emit(cond);
//     cfor.emitEnd();
//
//   `for (;;) body`
//     CForEmitter cfor(this, nullptr);
//     cfor.emitInit(Nothing());
//     cfor.emitBody(CForEmitter::Cond::Missing, Some(offset_of_body));
//     emit(body);
//     cfor.emitUpdate(CForEmitter::Update::Missing, Nothing());
//     cfor.emitCond(Some(offset_of_for),
//                   Nothing(),
//                   Some(offset_of_end));
//     cfor.emitEnd();
//
class MOZ_STACK_CLASS CForEmitter
{
    // Basic structure of the bytecode (not complete).
    //
    // If `cond` is not empty:
    //     {init}
    //     JSOP_GOTO entry
    //   loop:
    //     {body}
    //     {update}
    //   entry:
    //     {cond}
    //     JSOP_IFNE loop
    //
    // If `cond` is empty:
    //     {init}
    //   loop:
    //     {body}
    //     {update}
    //     JSOP_GOTO loop
    //
  public:
    enum class Cond {
        Missing,
        Present
    };
    enum class Update {
        Missing,
        Present
    };

  private:
    BytecodeEmitter* bce_;

    // The source note index for SRC_FOR.
    unsigned noteIndex_ = 0;

    // The bytecode offset of loop condition.
    // Not the bytecode offset of loop condition expression itself.
    ptrdiff_t condOffset_ = 0;

    // The base bytecode offset used by SRC_FOR.
    ptrdiff_t biasedTop_ = 0;

    // Whether the c-style for loop has `cond` and `update`.
    Cond cond_ = Cond::Missing;
    Update update_ = Update::Missing;

    mozilla::Maybe<LoopControl> loopInfo_;

    // The lexical scope to be freshened for each iteration.
    // See the comment in `emitBody` for more details.
    //
    // ### Scope freshening
    //
    // Each iteration of a `for (let V...)` loop creates a fresh loop variable
    // binding for V, even if the loop is a C-style `for(;;)` loop:
    //
    //     var funcs = [];
    //     for (let i = 0; i < 2; i++)
    //         funcs.push(function() { return i; });
    //     assertEq(funcs[0](), 0);  // the two closures capture...
    //     assertEq(funcs[1](), 1);  // ...two different `i` bindings
    //
    // This is implemented by "freshening" the implicit block -- changing the
    // scope chain to a fresh clone of the instantaneous block object -- each
    // iteration, just before evaluating the "update" in for(;;) loops.
    //
    // ECMAScript doesn't freshen in `for (const ...;;)`.  Lack of freshening
    // isn't directly observable in-language because `const`s can't be mutated,
    // but it *can* be observed in the Debugger API.
    const EmitterScope* headLexicalEmitterScopeForLet_;

    mozilla::Maybe<TDZCheckCache> tdzCache_;

#ifdef DEBUG
    // The state of this emitter.
    //
    // +-------+ emitInit +------+ emitBody +------+ emitUpdate +--------+
    // | Start |--------->| Init |--------->| Body |----------->| Update |-+
    // +-------+          +------+          +------+            +--------+ |
    //                                                                     |
    //                                   +---------------------------------+
    //                                   |
    //                                   | emitCond +------+ emitEnd +-----+
    //                                   +--------->| Cond |-------->| End |
    //                                              +------+         +-----+
    enum class State {
        // The initial state.
        Start,

        // After calling emitInit.
        Init,

        // After calling emitBody.
        Body,

        // After calling emitUpdate.
        Update,

        // After calling emitCond.
        Cond,

        // After calling emitEnd.
        End
    };
    State state_ = State::Start;
#endif

  public:
    CForEmitter(BytecodeEmitter* bce, const EmitterScope* headLexicalEmitterScopeForLet);

    // Parameters are the offset in the source code for each character below:
    //
    //   for ( x = 10 ; x < 20 ; x ++ ) { f(x); }
    //   ^     ^        ^        ^      ^       ^
    //   |     |        |        |      |       |
    //   |     |        |        |      |       endPos
    //   |     |        |        |      |
    //   |     |        |        |      bodyPos
    //   |     |        |        |
    //   |     |        |        updatePos
    //   |     |        |
    //   |     |        condPos
    //   |     |
    //   |     initPos
    //   |
    //   forPos
    //
    // Can be Nothing() if not available.
    MOZ_MUST_USE bool emitInit(const mozilla::Maybe<uint32_t>& initPos);
    MOZ_MUST_USE bool emitBody(Cond cond, const mozilla::Maybe<uint32_t>& bodyPos);
    MOZ_MUST_USE bool emitUpdate(Update update, const mozilla::Maybe<uint32_t>& updatePos);
    MOZ_MUST_USE bool emitCond(const mozilla::Maybe<uint32_t>& forPos,
                               const mozilla::Maybe<uint32_t>& condPos,
                               const mozilla::Maybe<uint32_t>& endPos);
    MOZ_MUST_USE bool emitEnd();
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_CForEmitter_h */

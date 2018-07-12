/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_TryEmitter_h
#define frontend_TryEmitter_h

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"

#include <stddef.h>
#include <stdint.h>

#include "frontend/BytecodeControlStructures.h"
#include "frontend/JumpList.h"
#include "frontend/TDZCheckCache.h"

namespace js {
namespace frontend {

struct BytecodeEmitter;

// Class for emitting bytecode for blocks like try-catch-finally.
//
// Usage: (check for the return value is omitted for simplicity)
//
//   `try { try_block } catch (ex) { catch_block }`
//     TryEmitter tryCatch(this, TryEmitter::Kind::TryCatch,
//                         TryEmitter::ControlKind::Syntactic);
//     tryCatch.emitTry();
//     emit(try_block);
//     tryCatch.emitCatch();
//     emit(ex and catch_block); // use JSOP_EXCEPTION to get exception
//     tryCatch.emitEnd();
//
//   `try { try_block } finally { finally_block }`
//     TryEmitter tryCatch(this, TryEmitter::Kind::TryFinally,
//                         TryEmitter::ControlKind::Syntactic);
//     tryCatch.emitTry();
//     emit(try_block);
//     // finally_pos: The "{" character's position in the source code text.
//     tryCatch.emitFinally(Some(finally_pos));
//     emit(finally_block);
//     tryCatch.emitEnd();
//
//   `try { try_block } catch (ex) {catch_block} finally { finally_block }`
//     TryEmitter tryCatch(this, TryEmitter::Kind::TryCatchFinally,
//                         TryEmitter::ControlKind::Syntactic);
//     tryCatch.emitTry();
//     emit(try_block);
//     tryCatch.emitCatch();
//     emit(ex and catch_block);
//     tryCatch.emitFinally(Some(finally_pos));
//     emit(finally_block);
//     tryCatch.emitEnd();
//
class MOZ_STACK_CLASS TryEmitter
{
  public:
    enum class Kind {
        TryCatch,
        TryCatchFinally,
        TryFinally
    };

    // Syntactic try-catch-finally and internally used non-syntactic
    // try-catch-finally behave differently for 2 points.
    //
    // The first one is whether TryFinallyControl is used or not.
    // See the comment for `controlInfo_`.
    //
    // The second one is whether the catch and finally blocks handle the frame's
    // return value.  For syntactic try-catch-finally, the bytecode marked with
    // "*" are emitted to clear return value with `undefined` before the catch
    // block and the finally block, and also to save/restore the return value
    // before/after the finally block.
    //
    //     JSOP_TRY
    //
    //     try_body...
    //
    //     JSOP_GOSUB finally
    //     JSOP_JUMPTARGET
    //     JSOP_GOTO end:
    //
    //   catch:
    //     JSOP_JUMPTARGET
    //   * JSOP_UNDEFINED
    //   * JSOP_SETRVAL
    //
    //     catch_body...
    //
    //     JSOP_GOSUB finally
    //     JSOP_JUMPTARGET
    //     JSOP_GOTO end
    //
    //   finally:
    //     JSOP_JUMPTARGET
    //   * JSOP_GETRVAL
    //   * JSOP_UNDEFINED
    //   * JSOP_SETRVAL
    //
    //     finally_body...
    //
    //   * JSOP_SETRVAL
    //     JSOP_NOP
    //
    //   end:
    //     JSOP_JUMPTARGET
    //
    // For syntactic try-catch-finally, Syntactic should be used.
    // For non-syntactic try-catch-finally, NonSyntactic should be used.
    enum class ControlKind {
        Syntactic,
        NonSyntactic
    };

  private:
    BytecodeEmitter* bce_;
    Kind kind_;
    ControlKind controlKind_;

    // Track jumps-over-catches and gosubs-to-finally for later fixup.
    //
    // When a finally block is active, non-local jumps (including
    // jumps-over-catches) result in a GOSUB being written into the bytecode
    // stream and fixed-up later.
    //
    // For non-syntactic try-catch-finally, all that handling is skipped.
    // The non-syntactic try-catch-finally must:
    //   * have only one catch block
    //   * have JSOP_GOTO at the end of catch-block
    //   * have no non-local-jump
    //   * don't use finally block for normal completion of try-block and
    //     catch-block
    //
    // Additionally, a finally block may be emitted for non-syntactic
    // try-catch-finally, even if the kind is TryCatch, because GOSUBs are not
    // emitted.
    mozilla::Maybe<TryFinallyControl> controlInfo_;

    // The stack depth before emitting JSOP_TRY.
    int depth_;

    // The source note index for SRC_TRY.
    unsigned noteIndex_;

    // The offset after JSOP_TRY.
    ptrdiff_t tryStart_;

    // JSOP_JUMPTARGET after the entire try-catch-finally block.
    JumpList catchAndFinallyJump_;

    // The offset of JSOP_GOTO at the end of the try block.
    JumpTarget tryEnd_;

    // The offset of JSOP_JUMPTARGET at the beginning of the finally block.
    JumpTarget finallyStart_;

#ifdef DEBUG
    // The state of this emitter.
    //
    // +-------+ emitTry +-----+   emitCatch +-------+      emitEnd  +-----+
    // | Start |-------->| Try |-+---------->| Catch |-+->+--------->| End |
    // +-------+         +-----+ |           +-------+ |  ^          +-----+
    //                           |                     |  |
    //                           |  +------------------+  +----+
    //                           |  |                          |
    //                           |  v emitFinally +---------+  |
    //                           +->+------------>| Finally |--+
    //                                            +---------+
    enum class State {
        // The initial state.
        Start,

        // After calling emitTry.
        Try,

        // After calling emitCatch.
        Catch,

        // After calling emitFinally.
        Finally,

        // After calling emitEnd.
        End
    };
    State state_;
#endif

    bool hasCatch() const {
        return kind_ == Kind::TryCatch || kind_ == Kind::TryCatchFinally;
    }
    bool hasFinally() const {
        return kind_ == Kind::TryCatchFinally || kind_ == Kind::TryFinally;
    }

  public:
    TryEmitter(BytecodeEmitter* bce, Kind kind, ControlKind controlKind);

    // Emits JSOP_GOTO to the end of try-catch-finally.
    // Used in `yield*`.
    MOZ_MUST_USE bool emitJumpOverCatchAndFinally();

    MOZ_MUST_USE bool emitTry();
    MOZ_MUST_USE bool emitCatch();

    // If `finallyPos` is specified, it's an offset of the finally block's
    // "{" character in the source code text, to improve line:column number in
    // the error reporting.
    // For non-syntactic try-catch-finally, `finallyPos` can be omitted.
    MOZ_MUST_USE bool emitFinally(const mozilla::Maybe<uint32_t>& finallyPos = mozilla::Nothing());

    MOZ_MUST_USE bool emitEnd();

  private:
    MOZ_MUST_USE bool emitTryEnd();
    MOZ_MUST_USE bool emitCatchEnd();
    MOZ_MUST_USE bool emitFinallyEnd();
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_TryEmitter_h */

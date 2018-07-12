/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_IfEmitter_h
#define frontend_IfEmitter_h

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"

#include <stdint.h>

#include "frontend/JumpList.h"
#include "frontend/SourceNotes.h"
#include "frontend/TDZCheckCache.h"

namespace js {
namespace frontend {

struct BytecodeEmitter;

// Class for emitting bytecode for blocks like if-then-else.
//
// This class can be used to emit single if-then-else block, or cascading
// else-if blocks.
//
// Usage: (check for the return value is omitted for simplicity)
//
//   `if (cond) then_block`
//     IfEmitter ifThen(this);
//     emit(cond);
//     ifThen.emitThen();
//     emit(then_block);
//     ifThen.emitEnd();
//
//   `if (cond) then_block else else_block`
//     IfEmitter ifThenElse(this);
//     emit(cond);
//     ifThenElse.emitThenElse();
//     emit(then_block);
//     ifThenElse.emitElse();
//     emit(else_block);
//     ifThenElse.emitEnd();
//
//   `if (c1) b1 else if (c2) b2 else if (c3) b3 else b4`
//     IfEmitter ifThenElse(this);
//     emit(c1);
//     ifThenElse.emitThenElse();
//     emit(b1);
//     ifThenElse.emitElseIf();
//     emit(c2);
//     ifThenElse.emitThenElse();
//     emit(b2);
//     ifThenElse.emitElseIf();
//     emit(c3);
//     ifThenElse.emitThenElse();
//     emit(b3);
//     ifThenElse.emitElse();
//     emit(b4);
//     ifThenElse.emitEnd();
//
//   `cond ? then_expr : else_expr`
//     IfEmitter condElse(this);
//     emit(cond);
//     condElse.emitCond();
//     emit(then_block);
//     condElse.emitElse();
//     emit(else_block);
//     condElse.emitEnd();
//
class MOZ_STACK_CLASS IfEmitter
{
  public:
    // Whether the then-clause, the else-clause, or else-if condition may
    // contain declaration or access to lexical variables, which means they
    // should have their own TDZCheckCache.  Basically TDZCheckCache should be
    // created for each basic block, which then-clause, else-clause, and
    // else-if condition are, but for internally used branches which are
    // known not to touch lexical variables we can skip creating TDZCheckCache
    // for them.
    //
    // See the comment for TDZCheckCache class for more details.
    enum class Kind {
        // For syntactic branches (if, if-else, and conditional expression),
        // which basically may contain declaration or accesses to lexical
        // variables inside then-clause, else-clause, and else-if condition.
        MayContainLexicalAccessInBranch,

        // For internally used branches which don't touch lexical variables
        // inside then-clause, else-clause, nor else-if condition.
        NoLexicalAccessInBranch
    };

  private:
    BytecodeEmitter* bce_;

    // Jump around the then clause, to the beginning of the else clause.
    JumpList jumpAroundThen_;

    // Jump around the else clause, to the end of the entire branch.
    JumpList jumpsAroundElse_;

    // The stack depth before emitting the then block.
    // Used for restoring stack depth before emitting the else block.
    // Also used for assertion to make sure then and else blocks pushed the
    // same number of values.
    int32_t thenDepth_;

    Kind kind_;
    mozilla::Maybe<TDZCheckCache> tdzCache_;

#ifdef DEBUG
    // The number of values pushed in the then and else blocks.
    int32_t pushed_;
    bool calculatedPushed_;

    // The state of this emitter.
    //
    // +-------+   emitCond +------+ emitElse +------+        emitEnd +-----+
    // | Start |-+--------->| Cond |--------->| Else |------>+------->| End |
    // +-------+ |          +------+          +------+       ^        +-----+
    //           |                                           |
    //           v emitThen +------+                         |
    //        +->+--------->| Then |------------------------>+
    //        ^  |          +------+                         ^
    //        |  |                                           |
    //        |  |                                           +---+
    //        |  |                                               |
    //        |  | emitThenElse +----------+   emitElse +------+ |
    //        |  +------------->| ThenElse |-+--------->| Else |-+
    //        |                 +----------+ |          +------+
    //        |                              |
    //        |                              | emitElseIf +--------+
    //        |                              +----------->| ElseIf |-+
    //        |                                           +--------+ |
    //        |                                                      |
    //        +------------------------------------------------------+
    enum class State {
        // The initial state.
        Start,

        // After calling emitThen.
        Then,

        // After calling emitCond.
        Cond,

        // After calling emitThenElse.
        ThenElse,

        // After calling emitElse.
        Else,

        // After calling emitElseIf.
        ElseIf,

        // After calling emitEnd.
        End
    };
    State state_;
#endif

  protected:
    // For InternalIfEmitter.
    IfEmitter(BytecodeEmitter* bce, Kind kind);

  public:
    explicit IfEmitter(BytecodeEmitter* bce);

    MOZ_MUST_USE bool emitThen();
    MOZ_MUST_USE bool emitCond();
    MOZ_MUST_USE bool emitThenElse();

    MOZ_MUST_USE bool emitElse();
    MOZ_MUST_USE bool emitElseIf();

    MOZ_MUST_USE bool emitEnd();

#ifdef DEBUG
    // Returns the number of values pushed onto the value stack inside
    // `then_block` and `else_block`.
    // Can be used in assertion after emitting if-then-else.
    int32_t pushed() const {
        return pushed_;
    }

    // Returns the number of values popped onto the value stack inside
    // `then_block` and `else_block`.
    // Can be used in assertion after emitting if-then-else.
    int32_t popped() const {
        return -pushed_;
    }
#endif

  private:
    MOZ_MUST_USE bool emitIfInternal(SrcNoteType type);
    void calculateOrCheckPushed();
    MOZ_MUST_USE bool emitElseInternal();
};

// Class for emitting bytecode for blocks like if-then-else which doesn't touch
// lexical variables.
//
// See the comments above NoLexicalAccessInBranch for more details when to use
// this instead of IfEmitter.
class MOZ_STACK_CLASS InternalIfEmitter : public IfEmitter
{
  public:
    explicit InternalIfEmitter(BytecodeEmitter* bce);
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_IfEmitter_h */

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_SwitchEmitter_h
#define frontend_SwitchEmitter_h

#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"

#include <stddef.h>
#include <stdint.h>

#include "frontend/BytecodeControlStructures.h"
#include "frontend/EmitterScope.h"
#include "frontend/JumpList.h"
#include "frontend/TDZCheckCache.h"
#include "gc/Rooting.h"
#include "js/AllocPolicy.h"
#include "js/Value.h"
#include "js/Vector.h"
#include "vm/Scope.h"

namespace js {
namespace frontend {

struct BytecodeEmitter;

// Class for emitting bytecode for switch-case-default block.
//
// Usage: (check for the return value is omitted for simplicity)
//
//   `switch (discriminant) { case c1_expr: c1_body; }`
//     SwitchEmitter se(this);
//     se.emitDiscriminant(Some(offset_of_switch));
//     emit(discriminant);
//
//     se.validateCaseCount(1);
//     se.emitCond();
//
//     emit(c1_expr);
//     se.emitCaseJump();
//
//     se.emitCaseBody();
//     emit(c1_body);
//
//     se.emitEnd();
//
//   `switch (discriminant) { case c1_expr: c1_body; case c2_expr: c2_body;
//                            default: def_body; }`
//     SwitchEmitter se(this);
//     se.emitDiscriminant(Some(offset_of_switch));
//     emit(discriminant);
//
//     se.validateCaseCount(2);
//     se.emitCond();
//
//     emit(c1_expr);
//     se.emitCaseJump();
//
//     emit(c2_expr);
//     se.emitCaseJump();
//
//     se.emitCaseBody();
//     emit(c1_body);
//
//     se.emitCaseBody();
//     emit(c2_body);
//
//     se.emitDefaultBody();
//     emit(def_body);
//
//     se.emitEnd();
//
//   `switch (discriminant) { case c1_expr: c1_body; case c2_expr: c2_body; }`
//   with Table Switch
//     SwitchEmitter::TableGenerator tableGen(this);
//     tableGen.addNumber(c1_expr_value);
//     tableGen.addNumber(c2_expr_value);
//     tableGen.finish(2);
//
//     // If `!tableGen.isValid()` here, `emitCond` should be used instead.
//
//     SwitchEmitter se(this);
//     se.emitDiscriminant(Some(offset_of_switch));
//     emit(discriminant);
//     se.validateCaseCount(2);
//     se.emitTable(tableGen);
//
//     se.emitCaseBody(c1_expr_value, tableGen);
//     emit(c1_body);
//
//     se.emitCaseBody(c2_expr_value, tableGen);
//     emit(c2_body);
//
//     se.emitEnd();
//
//   `switch (discriminant) { case c1_expr: c1_body; case c2_expr: c2_body;
//                            default: def_body; }`
//   with Table Switch
//     SwitchEmitter::TableGenerator tableGen(bce);
//     tableGen.addNumber(c1_expr_value);
//     tableGen.addNumber(c2_expr_value);
//     tableGen.finish(2);
//
//     // If `!tableGen.isValid()` here, `emitCond` should be used instead.
//
//     SwitchEmitter se(this);
//     se.emitDiscriminant(Some(offset_of_switch));
//     emit(discriminant);
//     se.validateCaseCount(2);
//     se.emitTable(tableGen);
//
//     se.emitCaseBody(c1_expr_value, tableGen);
//     emit(c1_body);
//
//     se.emitCaseBody(c2_expr_value, tableGen);
//     emit(c2_body);
//
//     se.emitDefaultBody();
//     emit(def_body);
//
//     se.emitEnd();
//
//   `switch (discriminant) { case c1_expr: c1_body; }`
//   in case c1_body contains lexical bindings
//     SwitchEmitter se(this);
//     se.emitDiscriminant(Some(offset_of_switch));
//     emit(discriminant);
//
//     se.validateCaseCount(1);
//
//     se.emitLexical(bindings);
//
//     se.emitCond();
//
//     emit(c1_expr);
//     se.emitCaseJump();
//
//     se.emitCaseBody();
//     emit(c1_body);
//
//     se.emitEnd();
//
//   `switch (discriminant) { case c1_expr: c1_body; }`
//   in case c1_body contains hosted functions
//     SwitchEmitter se(this);
//     se.emitDiscriminant(Some(offset_of_switch));
//     emit(discriminant);
//
//     se.validateCaseCount(1);
//
//     se.emitLexical(bindings);
//     emit(hosted functions);
//
//     se.emitCond();
//
//     emit(c1_expr);
//     se.emitCaseJump();
//
//     se.emitCaseBody();
//     emit(c1_body);
//
//     se.emitEnd();
//
class MOZ_STACK_CLASS SwitchEmitter
{
    // Bytecode for each case.
    //
    // Cond Switch
    //     {discriminant}
    //     JSOP_CONDSWITCH
    //
    //     {c1_expr}
    //     JSOP_CASE c1
    //
    //     JSOP_JUMPTARGET
    //     {c2_expr}
    //     JSOP_CASE c2
    //
    //     ...
    //
    //     JSOP_JUMPTARGET
    //     JSOP_DEFAULT default
    //
    //   c1:
    //     JSOP_JUMPTARGET
    //     {c1_body}
    //     JSOP_GOTO end
    //
    //   c2:
    //     JSOP_JUMPTARGET
    //     {c2_body}
    //     JSOP_GOTO end
    //
    //   default:
    //   end:
    //     JSOP_JUMPTARGET
    //
    // Table Switch
    //     {discriminant}
    //     JSOP_TABLESWITCH c1, c2, ...
    //
    //   c1:
    //     JSOP_JUMPTARGET
    //     {c1_body}
    //     JSOP_GOTO end
    //
    //   c2:
    //     JSOP_JUMPTARGET
    //     {c2_body}
    //     JSOP_GOTO end
    //
    //   ...
    //
    //   end:
    //     JSOP_JUMPTARGET

  public:
    enum class Kind {
        Table,
        Cond
    };

    // Class for generating optimized table switch data.
    class MOZ_STACK_CLASS TableGenerator
    {
        BytecodeEmitter* bce_;

        // Bit array for given numbers.
        mozilla::Maybe<js::Vector<size_t, 128, SystemAllocPolicy>> intmap_;

        // The length of the intmap_.
        int32_t intmapBitLength_ = 0;

        // The length of the table.
        uint32_t tableLength_ = 0;

        // The lower and higher bounds of the table.
        int32_t low_ = JSVAL_INT_MAX, high_ = JSVAL_INT_MIN;

        // Whether the table is still valid.
        bool valid_= true;

#ifdef DEBUG
        bool finished_ = false;
#endif

      public:
        explicit TableGenerator(BytecodeEmitter* bce)
          : bce_(bce)
        {}

        void setInvalid() {
            valid_ = false;
        }
        MOZ_MUST_USE bool isValid() const {
            return valid_;
        }
        MOZ_MUST_USE bool isInvalid() const {
            return !valid_;
        }

        // Add the given number to the table.  The number is the value of
        // `expr` for `case expr:` syntax.
        MOZ_MUST_USE bool addNumber(int32_t caseValue);

        // Finish generating the table.
        // `caseCount` should be the number of cases in the switch statement,
        // excluding the default case.
        void finish(uint32_t caseCount);

      private:
        friend SwitchEmitter;

        // The following methods can be used only after calling `finish`.

        // Returns the lower bound of the added numbers.
        int32_t low() const {
            MOZ_ASSERT(finished_);
            return low_;
        }

        // Returns the higher bound of the numbers.
        int32_t high() const {
            MOZ_ASSERT(finished_);
            return high_;
        }

        // Returns the index in SwitchEmitter.caseOffsets_ for table switch.
        uint32_t toCaseIndex(int32_t caseValue) const;

        // Returns the length of the table.
        // This method can be called only if `isValid()` is true.
        uint32_t tableLength() const;
    };

  private:
    BytecodeEmitter* bce_;

    // `kind_` should be set to the correct value in emitCond/emitTable.
    Kind kind_ = Kind::Cond;

    // True if there's explicit default case.
    bool hasDefault_ = false;

    // The source note index for SRC_CONDSWITCH.
    unsigned noteIndex_ = 0;

    // Source note index of the previous SRC_NEXTCASE.
    unsigned caseNoteIndex_ = 0;

    // The number of cases in the switch statement, excluding the default case.
    uint32_t caseCount_ = 0;

    // Internal index for case jump and case body, used by cond switch.
    uint32_t caseIndex_ = 0;

    // Bytecode offset after emitting `discriminant`.
    ptrdiff_t top_ = 0;

    // Bytecode offset of the previous JSOP_CASE.
    ptrdiff_t lastCaseOffset_ = 0;

    // Bytecode offset of the JSOP_JUMPTARGET for default body.
    JumpTarget defaultJumpTargetOffset_ = { -1 };

    // Bytecode offset of the JSOP_DEFAULT.
    JumpList condSwitchDefaultOffset_;

    // Instantiated when there's lexical scope for entire switch.
    mozilla::Maybe<TDZCheckCache> tdzCacheLexical_;
    mozilla::Maybe<EmitterScope> emitterScope_;

    // Instantiated while emitting case expression and case/default body.
    mozilla::Maybe<TDZCheckCache> tdzCacheCaseAndBody_;

    // Control for switch.
    mozilla::Maybe<BreakableControl> controlInfo_;

    mozilla::Maybe<uint32_t> switchPos_;

    // Cond Switch:
    //   Offset of each JSOP_CASE.
    // Table Switch:
    //   Offset of each JSOP_JUMPTARGET for case.
    js::Vector<ptrdiff_t, 32, SystemAllocPolicy> caseOffsets_;

    // The state of this emitter.
    //
    // +-------+ emitDiscriminant +--------------+
    // | Start |----------------->| Discriminant |-+
    // +-------+                  +--------------+ |
    //                                             |
    // +-------------------------------------------+
    // |
    // |                              validateCaseCount +-----------+
    // +->+------------------------>+------------------>| CaseCount |-+
    //    |                         ^                   +-----------+ |
    //    | emitLexical +---------+ |                                 |
    //    +------------>| Lexical |-+                                 |
    //                  +---------+                                   |
    //                                                                |
    // +--------------------------------------------------------------+
    // |
    // | emitTable +-------+
    // +---------->| Table |---------------------------->+-+
    // |           +-------+                             ^ |
    // |                                                 | |
    // | emitCond  +------+                              | |
    // +---------->| Cond |-+------------------------>+->+ |
    //             +------+ |                         ^    |
    //                      |                         |    |
    //                      |    emitCase +------+    |    |
    //                      +->+--------->| Case |->+-+    |
    //                         ^          +------+  |      |
    //                         |                    |      |
    //                         +--------------------+      |
    //                                                     |
    // +---------------------------------------------------+
    // |
    // |                                              emitEnd +-----+
    // +-+----------------------------------------->+-------->| End |
    //   |                                          ^         +-----+
    //   |      emitCaseBody    +----------+        |
    //   +->+-+---------------->| CaseBody |--->+-+-+
    //      ^ |                 +----------+    ^ |
    //      | |                                 | |
    //      | | emitDefaultBody +-------------+ | |
    //      | +---------------->| DefaultBody |-+ |
    //      |                   +-------------+   |
    //      |                                     |
    //      +-------------------------------------+
    //
    enum class State {
        // The initial state.
        Start,

        // After calling emitDiscriminant.
        Discriminant,

        // After calling validateCaseCount.
        CaseCount,

        // After calling emitLexical.
        Lexical,

        // After calling emitCond.
        Cond,

        // After calling emitTable.
        Table,

        // After calling emitCase.
        Case,

        // After calling emitCaseBody.
        CaseBody,

        // After calling emitDefaultBody.
        DefaultBody,

        // After calling emitEnd.
        End
    };
    State state_ = State::Start;

  public:
    explicit SwitchEmitter(BytecodeEmitter* bce);

    // `switchPos` is the offset in the source code for the character below:
    //
    //   switch ( cond ) { ... }
    //   ^
    //   |
    //   switchPos
    //
    // Can be Nothing() if not available.
    MOZ_MUST_USE bool emitDiscriminant(const mozilla::Maybe<uint32_t>& switchPos);

    // `caseCount` should be the number of cases in the switch statement,
    // excluding the default case.
    MOZ_MUST_USE bool validateCaseCount(uint32_t caseCount);

    // `bindings` is a lexical scope for the entire switch, in case there's
    // let/const effectively directly under case or default blocks.
    MOZ_MUST_USE bool emitLexical(Handle<LexicalScope::Data*> bindings);

    MOZ_MUST_USE bool emitCond();
    MOZ_MUST_USE bool emitTable(const TableGenerator& tableGen);

    MOZ_MUST_USE bool emitCaseJump();

    MOZ_MUST_USE bool emitCaseBody();
    MOZ_MUST_USE bool emitCaseBody(int32_t caseValue, const TableGenerator& tableGen);
    MOZ_MUST_USE bool emitDefaultBody();
    MOZ_MUST_USE bool emitEnd();

  private:
    MOZ_MUST_USE bool emitCaseOrDefaultJump(uint32_t caseIndex, bool isDefault);
    MOZ_MUST_USE bool emitImplicitDefault();
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_SwitchEmitter_h */

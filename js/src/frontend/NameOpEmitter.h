/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_NameOpEmitter_h
#define frontend_NameOpEmitter_h

#include "mozilla/Attributes.h"

#include <stdint.h>

#include "frontend/NameAnalysisTypes.h"
#include "js/TypeDecls.h"

namespace js {
namespace frontend {

struct BytecodeEmitter;

// Class for emitting bytecode for name operation.
//
// Usage: (check for the return value is omitted for simplicity)
//
//   `name;`
//     NameOpEmitter noe(this, atom_of_name
//                       ElemOpEmitter::Kind::Get);
//     noe.emitGet();
//
//   `name();`
//     this is handled in CallOrNewEmitter
//
//   `name++;`
//     NameOpEmitter noe(this, atom_of_name
//                       ElemOpEmitter::Kind::PostIncrement);
//     noe.emitIncDec();
//
//   `name = 10;`
//     NameOpEmitter noe(this, atom_of_name
//                       ElemOpEmitter::Kind::SimpleAssignment);
//     noe.prepareForRhs();
//     emit(10);
//     noe.emitAssignment();
//
//   `name += 10;`
//     NameOpEmitter noe(this, atom_of_name
//                       ElemOpEmitter::Kind::CompoundAssignment);
//     noe.prepareForRhs();
//     emit(10);
//     emit_add_op_here();
//     noe.emitAssignment();
//
//   `name = 10;` part of `let name = 10;`
//     NameOpEmitter noe(this, atom_of_name
//                       ElemOpEmitter::Kind::Initialize);
//     noe.prepareForRhs();
//     emit(10);
//     noe.emitAssignment();
//
class MOZ_STACK_CLASS NameOpEmitter {
 public:
  enum class Kind {
    Get,
    Call,
    PostIncrement,
    PreIncrement,
    PostDecrement,
    PreDecrement,
    SimpleAssignment,
    CompoundAssignment,
    Initialize
  };

 private:
  BytecodeEmitter* bce_;

  Kind kind_;

  bool emittedBindOp_ = false;

  Handle<JSAtom*> name_;

  uint32_t atomIndex_;

  NameLocation loc_;

#ifdef DEBUG
  // The state of this emitter.
  //
  //               [Get]
  //               [Call]
  // +-------+      emitGet +-----+
  // | Start |-+-+--------->| Get |
  // +-------+   |          +-----+
  //             |
  //             | [PostIncrement]
  //             | [PreIncrement]
  //             | [PostDecrement]
  //             | [PreDecrement]
  //             |   emitIncDec +--------+
  //             +------------->| IncDec |
  //             |              +--------+
  //             |
  //             | [SimpleAssignment]
  //             |                        prepareForRhs +-----+
  //             +--------------------->+-------------->| Rhs |-+
  //             |                      ^               +-----+ |
  //             |                      |                       |
  //             |                      |    +------------------+
  //             | [CompoundAssignment] |    |
  //             |   emitGet +-----+    |    | emitAssignment +------------+
  //             +---------->| Get |----+    + -------------->| Assignment |
  //                         +-----+                          +------------+
  enum class State {
    // The initial state.
    Start,

    // After calling emitGet.
    Get,

    // After calling emitIncDec.
    IncDec,

    // After calling prepareForRhs.
    Rhs,

    // After calling emitAssignment.
    Assignment,
  };
  State state_ = State::Start;
#endif

 public:
  NameOpEmitter(BytecodeEmitter* bce, Handle<JSAtom*> name, Kind kind);
  NameOpEmitter(BytecodeEmitter* bce, Handle<JSAtom*> name,
                const NameLocation& loc, Kind kind);

 private:
  MOZ_MUST_USE bool isCall() const { return kind_ == Kind::Call; }

  MOZ_MUST_USE bool isSimpleAssignment() const {
    return kind_ == Kind::SimpleAssignment;
  }

  MOZ_MUST_USE bool isCompoundAssignment() const {
    return kind_ == Kind::CompoundAssignment;
  }

  MOZ_MUST_USE bool isIncDec() const { return isPostIncDec() || isPreIncDec(); }

  MOZ_MUST_USE bool isPostIncDec() const {
    return kind_ == Kind::PostIncrement || kind_ == Kind::PostDecrement;
  }

  MOZ_MUST_USE bool isPreIncDec() const {
    return kind_ == Kind::PreIncrement || kind_ == Kind::PreDecrement;
  }

  MOZ_MUST_USE bool isInc() const {
    return kind_ == Kind::PostIncrement || kind_ == Kind::PreIncrement;
  }

  MOZ_MUST_USE bool isInitialize() const { return kind_ == Kind::Initialize; }

 public:
  MOZ_MUST_USE bool emittedBindOp() const { return emittedBindOp_; }

  MOZ_MUST_USE const NameLocation& loc() const { return loc_; }

  MOZ_MUST_USE bool emitGet();
  MOZ_MUST_USE bool prepareForRhs();
  MOZ_MUST_USE bool emitAssignment();
  MOZ_MUST_USE bool emitIncDec();
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_NameOpEmitter_h */

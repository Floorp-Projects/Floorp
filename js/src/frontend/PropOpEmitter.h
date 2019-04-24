/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_PropOpEmitter_h
#define frontend_PropOpEmitter_h

#include "mozilla/Attributes.h"

#include <stdint.h>

#include "js/TypeDecls.h"

namespace js {
namespace frontend {

struct BytecodeEmitter;

// Class for emitting bytecode for property operation.
//
// Usage: (check for the return value is omitted for simplicity)
//
//   `obj.prop;`
//     PropOpEmitter poe(this,
//                       PropOpEmitter::Kind::Get,
//                       PropOpEmitter::ObjKind::Other);
//     poe.prepareForObj();
//     emit(obj);
//     poe.emitGet(atom_of_prop);
//
//   `super.prop;`
//     PropOpEmitter poe(this,
//                       PropOpEmitter::Kind::Get,
//                       PropOpEmitter::ObjKind::Super);
//     poe.prepareForObj();
//     emit(obj);
//     poe.emitGet(atom_of_prop);
//
//   `obj.prop();`
//     PropOpEmitter poe(this,
//                       PropOpEmitter::Kind::Call,
//                       PropOpEmitter::ObjKind::Other);
//     poe.prepareForObj();
//     emit(obj);
//     poe.emitGet(atom_of_prop);
//     emit_call_here();
//
//   `new obj.prop();`
//     PropOpEmitter poe(this,
//                       PropOpEmitter::Kind::Call,
//                       PropOpEmitter::ObjKind::Other);
//     poe.prepareForObj();
//     emit(obj);
//     poe.emitGet(atom_of_prop);
//     emit_call_here();
//
//   `delete obj.prop;`
//     PropOpEmitter poe(this,
//                       PropOpEmitter::Kind::Delete,
//                       PropOpEmitter::ObjKind::Other);
//     poe.prepareForObj();
//     emit(obj);
//     poe.emitDelete(atom_of_prop);
//
//   `delete super.prop;`
//     PropOpEmitter poe(this,
//                       PropOpEmitter::Kind::Delete,
//                       PropOpEmitter::ObjKind::Other);
//     poe.emitDelete(atom_of_prop);
//
//   `obj.prop++;`
//     PropOpEmitter poe(this,
//                       PropOpEmitter::Kind::PostIncrement,
//                       PropOpEmitter::ObjKind::Other);
//     poe.prepareForObj();
//     emit(obj);
//     poe.emitIncDec(atom_of_prop);
//
//   `obj.prop = value;`
//     PropOpEmitter poe(this,
//                       PropOpEmitter::Kind::SimpleAssignment,
//                       PropOpEmitter::ObjKind::Other);
//     poe.prepareForObj();
//     emit(obj);
//     poe.prepareForRhs();
//     emit(value);
//     poe.emitAssignment(atom_of_prop);
//
//   `obj.prop += value;`
//     PropOpEmitter poe(this,
//                       PropOpEmitter::Kind::CompoundAssignment,
//                       PropOpEmitter::ObjKind::Other);
//     poe.prepareForObj();
//     emit(obj);
//     poe.emitGet(atom_of_prop);
//     poe.prepareForRhs();
//     emit(value);
//     emit_add_op_here();
//     poe.emitAssignment(nullptr); // nullptr for CompoundAssignment
//
class MOZ_STACK_CLASS PropOpEmitter {
 public:
  enum class Kind {
    Get,
    Call,
    Set,
    Delete,
    PostIncrement,
    PreIncrement,
    PostDecrement,
    PreDecrement,
    SimpleAssignment,
    PropInit,
    CompoundAssignment
  };
  enum class ObjKind { Super, Other };

 private:
  BytecodeEmitter* bce_;

  Kind kind_;
  ObjKind objKind_;

  // The index for the property name's atom.
  uint32_t propAtomIndex_ = 0;

  // Whether the property name is `length` or not.
  bool isLength_ = false;

#ifdef DEBUG
  // The state of this emitter.
  //
  //             skipObjAndRhs
  //           +----------------------------+
  //           |                            |
  // +-------+ | prepareForObj +-----+      |
  // | Start |-+-------------->| Obj |-+    |
  // +-------+                 +-----+ |    |
  //                                   |    |
  // +---------------------------------+    |
  // |                                      |
  // |                                      |
  // | [Get]                                |
  // | [Call]                               |
  // |   emitGet +-----+                    |
  // +---------->| Get |                    |
  // |           +-----+                    |
  // |                                      |
  // | [Delete]                             |
  // |   emitDelete +--------+              |
  // +------------->| Delete |              |
  // |              +--------+              |
  // |                                      |
  // | [PostIncrement]                      |
  // | [PreIncrement]                       |
  // | [PostDecrement]                      |
  // | [PreDecrement]                       |
  // |   emitIncDec +--------+              |
  // +------------->| IncDec |              |
  // |              +--------+              |
  // |                                      |
  // | [SimpleAssignment]                   |
  // | [PropInit]                           |
  // |                        prepareForRhs |  +-----+
  // +--------------------->+-------------->+->| Rhs |-+
  // |                      ^                  +-----+ |
  // |                      |                          |
  // |                      |                +---------+
  // | [CompoundAssignment] |                |
  // |   emitGet +-----+    |                | emitAssignment +------------+
  // +---------->| Get |----+                + -------------->| Assignment |
  //             +-----+                                      +------------+
  enum class State {
    // The initial state.
    Start,

    // After calling prepareForObj.
    Obj,

    // After calling emitGet.
    Get,

    // After calling emitDelete.
    Delete,

    // After calling emitIncDec.
    IncDec,

    // After calling prepareForRhs or skipObjAndRhs.
    Rhs,

    // After calling emitAssignment.
    Assignment,
  };
  State state_ = State::Start;
#endif

 public:
  PropOpEmitter(BytecodeEmitter* bce, Kind kind, ObjKind objKind);

 private:
  MOZ_MUST_USE bool isCall() const { return kind_ == Kind::Call; }

  MOZ_MUST_USE bool isSuper() const { return objKind_ == ObjKind::Super; }

  MOZ_MUST_USE bool isSimpleAssignment() const {
    return kind_ == Kind::SimpleAssignment;
  }

  MOZ_MUST_USE bool isPropInit() const { return kind_ == Kind::PropInit; }

  MOZ_MUST_USE bool isDelete() const { return kind_ == Kind::Delete; }

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

  MOZ_MUST_USE bool prepareAtomIndex(JSAtom* prop);

 public:
  MOZ_MUST_USE bool prepareForObj();

  MOZ_MUST_USE bool emitGet(JSAtom* prop);

  MOZ_MUST_USE bool prepareForRhs();
  MOZ_MUST_USE bool skipObjAndRhs();

  MOZ_MUST_USE bool emitDelete(JSAtom* prop);

  // `prop` can be nullptr for CompoundAssignment.
  MOZ_MUST_USE bool emitAssignment(JSAtom* prop);

  MOZ_MUST_USE bool emitIncDec(JSAtom* prop);
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_PropOpEmitter_h */

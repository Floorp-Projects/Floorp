/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_ElemOpEmitter_h
#define frontend_ElemOpEmitter_h

#include "mozilla/Attributes.h"

namespace js {
namespace frontend {

struct BytecodeEmitter;

// Class for emitting bytecode for element operation.
//
// Usage: (check for the return value is omitted for simplicity)
//
//   `obj[key];`
//     ElemOpEmitter eoe(this,
//                       ElemOpEmitter::Kind::Get,
//                       ElemOpEmitter::ObjKind::Other);
//     eoe.prepareForObj();
//     emit(obj);
//     eoe.prepareForKey();
//     emit(key);
//     eoe.emitGet();
//
//   `super[key];`
//     ElemOpEmitter eoe(this,
//                       ElemOpEmitter::Kind::Get,
//                       ElemOpEmitter::ObjKind::Super);
//     eoe.prepareForObj();
//     emit(this_for_super);
//     eoe.prepareForKey();
//     emit(key);
//     eoe.emitGet();
//
//   `obj[key]();`
//     ElemOpEmitter eoe(this,
//                       ElemOpEmitter::Kind::Call,
//                       ElemOpEmitter::ObjKind::Other);
//     eoe.prepareForObj();
//     emit(obj);
//     eoe.prepareForKey();
//     emit(key);
//     eoe.emitGet();
//     emit_call_here();
//
//   `new obj[key]();`
//     ElemOpEmitter eoe(this,
//                       ElemOpEmitter::Kind::Call,
//                       ElemOpEmitter::ObjKind::Other);
//     eoe.prepareForObj();
//     emit(obj);
//     eoe.prepareForKey();
//     emit(key);
//     eoe.emitGet();
//     emit_call_here();
//
//   `delete obj[key];`
//     ElemOpEmitter eoe(this,
//                       ElemOpEmitter::Kind::Delete,
//                       ElemOpEmitter::ObjKind::Other);
//     eoe.prepareForObj();
//     emit(obj);
//     eoe.prepareForKey();
//     emit(key);
//     eoe.emitDelete();
//
//   `delete super[key];`
//     ElemOpEmitter eoe(this,
//                       ElemOpEmitter::Kind::Delete,
//                       ElemOpEmitter::ObjKind::Super);
//     eoe.prepareForObj();
//     emit(this_for_super);
//     eoe.prepareForKey();
//     emit(key);
//     eoe.emitDelete();
//
//   `obj[key]++;`
//     ElemOpEmitter eoe(this,
//                       ElemOpEmitter::Kind::PostIncrement,
//                       ElemOpEmitter::ObjKind::Other);
//     eoe.prepareForObj();
//     emit(obj);
//     eoe.prepareForKey();
//     emit(key);
//     eoe.emitIncDec();
//
//   `obj[key] = value;`
//     ElemOpEmitter eoe(this,
//                       ElemOpEmitter::Kind::SimpleAssignment,
//                       ElemOpEmitter::ObjKind::Other);
//     eoe.prepareForObj();
//     emit(obj);
//     eoe.prepareForKey();
//     emit(key);
//     eoe.prepareForRhs();
//     emit(value);
//     eoe.emitAssignment();
//
//   `obj[key] += value;`
//     ElemOpEmitter eoe(this,
//                       ElemOpEmitter::Kind::CompoundAssignment,
//                       ElemOpEmitter::ObjKind::Other);
//     eoe.prepareForObj();
//     emit(obj);
//     eoe.prepareForKey();
//     emit(key);
//     eoe.emitGet();
//     eoe.prepareForRhs();
//     emit(value);
//     emit_add_op_here();
//     eoe.emitAssignment();
//
class MOZ_STACK_CLASS ElemOpEmitter {
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

#ifdef DEBUG
  // The state of this emitter.
  //
  //             skipObjAndKeyAndRhs
  //           +------------------------------------------------+
  //           |                                                |
  // +-------+ | prepareForObj +-----+ prepareForKey +-----+    |
  // | Start |-+-------------->| Obj |-------------->| Key |-+  |
  // +-------+                 +-----+               +-----+ |  |
  //                                                         |  |
  // +-------------------------------------------------------+  |
  // |                                                          |
  // |                                                          |
  // |                                                          |
  // | [Get]                                                    |
  // | [Call]                                                   |
  // |   emitGet +-----+                                        |
  // +---------->| Get |                                        |
  // |           +-----+                                        |
  // |                                                          |
  // | [Delete]                                                 |
  // |   emitDelete +--------+                                  |
  // +------------->| Delete |                                  |
  // |              +--------+                                  |
  // |                                                          |
  // | [PostIncrement]                                          |
  // | [PreIncrement]                                           |
  // | [PostDecrement]                                          |
  // | [PreDecrement]                                           |
  // |   emitIncDec +--------+                                  |
  // +------------->| IncDec |                                  |
  // |              +--------+                                  |
  // |                                      +-------------------+
  // | [SimpleAssignment]                   |
  // | [PropInit]                           |
  // |                        prepareForRhs v  +-----+
  // +--------------------->+-------------->+->| Rhs |-+
  // |                      ^                  +-----+ |
  // |                      |                          |
  // |                      |            +-------------+
  // | [CompoundAssignment] |            |
  // |   emitGet +-----+    |            | emitAssignment +------------+
  // +---------->| Get |----+            +--------------->| Assignment |
  //             +-----+                                  +------------+
  enum class State {
    // The initial state.
    Start,

    // After calling prepareForObj.
    Obj,

    // After calling emitKey.
    Key,

    // After calling emitGet.
    Get,

    // After calling emitDelete.
    Delete,

    // After calling emitIncDec.
    IncDec,

    // After calling prepareForRhs or skipObjAndKeyAndRhs.
    Rhs,

    // After calling emitAssignment.
    Assignment,
  };
  State state_ = State::Start;
#endif

 public:
  ElemOpEmitter(BytecodeEmitter* bce, Kind kind, ObjKind objKind);

 private:
  MOZ_MUST_USE bool isCall() const { return kind_ == Kind::Call; }

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

  MOZ_MUST_USE bool isSuper() const { return objKind_ == ObjKind::Super; }

 public:
  MOZ_MUST_USE bool prepareForObj();
  MOZ_MUST_USE bool prepareForKey();

  MOZ_MUST_USE bool emitGet();

  MOZ_MUST_USE bool prepareForRhs();
  MOZ_MUST_USE bool skipObjAndKeyAndRhs();

  MOZ_MUST_USE bool emitDelete();

  MOZ_MUST_USE bool emitAssignment();

  MOZ_MUST_USE bool emitIncDec();
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_ElemOpEmitter_h */

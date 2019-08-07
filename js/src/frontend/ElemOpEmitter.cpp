/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/ElemOpEmitter.h"

#include "frontend/BytecodeEmitter.h"
#include "frontend/SharedContext.h"
#include "vm/Opcodes.h"

using namespace js;
using namespace js::frontend;

ElemOpEmitter::ElemOpEmitter(BytecodeEmitter* bce, Kind kind, ObjKind objKind)
    : bce_(bce), kind_(kind), objKind_(objKind) {}

bool ElemOpEmitter::prepareForObj() {
  MOZ_ASSERT(state_ == State::Start);

#ifdef DEBUG
  state_ = State::Obj;
#endif
  return true;
}

bool ElemOpEmitter::prepareForKey() {
  MOZ_ASSERT(state_ == State::Obj);

  if (!isSuper() && isIncDec()) {
    if (!bce_->emit1(JSOP_CHECKOBJCOERCIBLE)) {
      //            [stack] OBJ
      return false;
    }
  }
  if (isCall()) {
    if (!bce_->emit1(JSOP_DUP)) {
      //            [stack] # if Super
      //            [stack] THIS THIS
      //            [stack] # otherwise
      //            [stack] OBJ OBJ
      return false;
    }
  }

#ifdef DEBUG
  state_ = State::Key;
#endif
  return true;
}

bool ElemOpEmitter::emitGet() {
  MOZ_ASSERT(state_ == State::Key);

  if (isIncDec() || isCompoundAssignment()) {
    if (!bce_->emit1(JSOP_TOID)) {
      //            [stack] # if Super
      //            [stack] THIS KEY
      //            [stack] # otherwise
      //            [stack] OBJ KEY
      return false;
    }
  }
  if (isSuper()) {
    if (!bce_->emitSuperBase()) {
      //            [stack] THIS? THIS KEY SUPERBASE
      return false;
    }
  }
  if (isIncDec() || isCompoundAssignment()) {
    if (isSuper()) {
      if (!bce_->emitDupAt(2, 3)) {
        //          [stack] THIS KEY SUPERBASE THIS KEY SUPERBASE
        return false;
      }
    } else {
      if (!bce_->emit1(JSOP_DUP2)) {
        //          [stack] OBJ KEY OBJ KEY
        return false;
      }
    }
  }

  JSOp op;
  if (isSuper()) {
    op = JSOP_GETELEM_SUPER;
  } else if (isCall()) {
    op = JSOP_CALLELEM;
  } else {
    op = JSOP_GETELEM;
  }
  if (!bce_->emitElemOpBase(op, ShouldInstrument::Yes)) {
    //              [stack] # if Get
    //              [stack] ELEM
    //              [stack] # if Call
    //              [stack] THIS ELEM
    //              [stack] # if Inc/Dec/Assignment, with Super
    //              [stack] THIS KEY SUPERBASE ELEM
    //              [stack] # if Inc/Dec/Assignment, other
    //              [stack] OBJ KEY ELEM
    return false;
  }
  if (isCall()) {
    if (!bce_->emit1(JSOP_SWAP)) {
      //            [stack] ELEM THIS
      return false;
    }
  }

#ifdef DEBUG
  state_ = State::Get;
#endif
  return true;
}

bool ElemOpEmitter::prepareForRhs() {
  MOZ_ASSERT(isSimpleAssignment() || isPropInit() || isCompoundAssignment());
  MOZ_ASSERT_IF(isSimpleAssignment() || isPropInit(), state_ == State::Key);
  MOZ_ASSERT_IF(isCompoundAssignment(), state_ == State::Get);

  if (isSimpleAssignment() || isPropInit()) {
    // For CompoundAssignment, SUPERBASE is already emitted by emitGet.
    if (isSuper()) {
      if (!bce_->emitSuperBase()) {
        //          [stack] THIS KEY SUPERBASE
        return false;
      }
    }
  }

#ifdef DEBUG
  state_ = State::Rhs;
#endif
  return true;
}

bool ElemOpEmitter::skipObjAndKeyAndRhs() {
  MOZ_ASSERT(state_ == State::Start);
  MOZ_ASSERT(isSimpleAssignment() || isPropInit());

#ifdef DEBUG
  state_ = State::Rhs;
#endif
  return true;
}

bool ElemOpEmitter::emitDelete() {
  MOZ_ASSERT(state_ == State::Key);
  MOZ_ASSERT(isDelete());

  if (isSuper()) {
    if (!bce_->emit1(JSOP_TOID)) {
      //            [stack] THIS KEY
      return false;
    }
    if (!bce_->emitSuperBase()) {
      //            [stack] THIS KEY SUPERBASE
      return false;
    }

    // Unconditionally throw when attempting to delete a super-reference.
    if (!bce_->emitUint16Operand(JSOP_THROWMSG, JSMSG_CANT_DELETE_SUPER)) {
      //            [stack] THIS KEY SUPERBASE
      return false;
    }

    // Another wrinkle: Balance the stack from the emitter's point of view.
    // Execution will not reach here, as the last bytecode threw.
    if (!bce_->emitPopN(2)) {
      //            [stack] THIS
      return false;
    }
  } else {
    JSOp op = bce_->sc->strict() ? JSOP_STRICTDELELEM : JSOP_DELELEM;
    if (!bce_->emitElemOpBase(op)) {
      // SUCCEEDED
      return false;
    }
  }

#ifdef DEBUG
  state_ = State::Delete;
#endif
  return true;
}

bool ElemOpEmitter::emitAssignment() {
  MOZ_ASSERT(isSimpleAssignment() || isPropInit() || isCompoundAssignment());
  MOZ_ASSERT(state_ == State::Rhs);

  MOZ_ASSERT_IF(isPropInit(), !isSuper());

  JSOp setOp =
      isPropInit()
          ? JSOP_INITELEM
          : isSuper() ? bce_->sc->strict() ? JSOP_STRICTSETELEM_SUPER
                                           : JSOP_SETELEM_SUPER
                      : bce_->sc->strict() ? JSOP_STRICTSETELEM : JSOP_SETELEM;
  if (!bce_->emitElemOpBase(setOp, ShouldInstrument::Yes)) {
    //              [stack] ELEM
    return false;
  }

#ifdef DEBUG
  state_ = State::Assignment;
#endif
  return true;
}

bool ElemOpEmitter::emitIncDec() {
  MOZ_ASSERT(state_ == State::Key);
  MOZ_ASSERT(isIncDec());

  if (!emitGet()) {
    //              [stack] ... ELEM
    return false;
  }

  MOZ_ASSERT(state_ == State::Get);

  JSOp incOp = isInc() ? JSOP_INC : JSOP_DEC;
  if (!bce_->emit1(JSOP_TONUMERIC)) {
    //              [stack] ... N
    return false;
  }
  if (isPostIncDec()) {
    if (!bce_->emit1(JSOP_DUP)) {
      //            [stack] ... N? N
      return false;
    }
  }
  if (!bce_->emit1(incOp)) {
    //              [stack] ... N? N+1
    return false;
  }
  if (isPostIncDec()) {
    if (isSuper()) {
      //            [stack] THIS KEY OBJ N N+1

      if (!bce_->emit2(JSOP_PICK, 4)) {
        //          [stack] KEY SUPERBASE N N+1 THIS
        return false;
      }
      if (!bce_->emit2(JSOP_PICK, 4)) {
        //          [stack] SUPERBASE N N+1 THIS KEY
        return false;
      }
      if (!bce_->emit2(JSOP_PICK, 4)) {
        //          [stack] N N+1 THIS KEY SUPERBASE
        return false;
      }
      if (!bce_->emit2(JSOP_PICK, 3)) {
        //          [stack] N THIS KEY SUPERBASE N+1
        return false;
      }
    } else {
      //            [stack] OBJ KEY N N+1

      if (!bce_->emit2(JSOP_PICK, 3)) {
        //          [stack] KEY N N+1 OBJ
        return false;
      }
      if (!bce_->emit2(JSOP_PICK, 3)) {
        //          [stack] N N+1 OBJ KEY
        return false;
      }
      if (!bce_->emit2(JSOP_PICK, 2)) {
        //          [stack] N OBJ KEY N+1
        return false;
      }
    }
  }

  JSOp setOp =
      isSuper()
          ? (bce_->sc->strict() ? JSOP_STRICTSETELEM_SUPER : JSOP_SETELEM_SUPER)
          : (bce_->sc->strict() ? JSOP_STRICTSETELEM : JSOP_SETELEM);
  if (!bce_->emitElemOpBase(setOp, ShouldInstrument::Yes)) {
    //              [stack] N? N+1
    return false;
  }
  if (isPostIncDec()) {
    if (!bce_->emit1(JSOP_POP)) {
      //            [stack] N
      return false;
    }
  }

#ifdef DEBUG
  state_ = State::IncDec;
#endif
  return true;
}

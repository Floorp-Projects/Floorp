/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/PropOpEmitter.h"

#include "frontend/BytecodeEmitter.h"
#include "frontend/SharedContext.h"
#include "vm/Opcodes.h"
#include "vm/StringType.h"

using namespace js;
using namespace js::frontend;

PropOpEmitter::PropOpEmitter(BytecodeEmitter* bce, Kind kind, ObjKind objKind)
    : bce_(bce), kind_(kind), objKind_(objKind) {}

bool PropOpEmitter::prepareAtomIndex(JSAtom* prop) {
  if (!bce_->makeAtomIndex(prop, &propAtomIndex_)) {
    return false;
  }
  isLength_ = prop == bce_->cx->names().length;

  return true;
}

bool PropOpEmitter::prepareForObj() {
  MOZ_ASSERT(state_ == State::Start);

#ifdef DEBUG
  state_ = State::Obj;
#endif
  return true;
}

bool PropOpEmitter::emitGet(JSAtom* prop) {
  MOZ_ASSERT(state_ == State::Obj);

  if (!prepareAtomIndex(prop)) {
    return false;
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
  if (isSuper()) {
    if (!bce_->emitSuperBase()) {
      //            [stack] THIS? THIS SUPERBASE
      return false;
    }
  }
  if (isIncDec() || isCompoundAssignment()) {
    if (isSuper()) {
      if (!bce_->emit1(JSOP_DUP2)) {
        //          [stack] THIS SUPERBASE THIS SUPERBASE
        return false;
      }
    } else {
      if (!bce_->emit1(JSOP_DUP)) {
        //          [stack] OBJ OBJ
        return false;
      }
    }
  }

  JSOp op;
  if (isSuper()) {
    op = JSOP_GETPROP_SUPER;
  } else if (isCall()) {
    op = JSOP_CALLPROP;
  } else {
    op = isLength_ ? JSOP_LENGTH : JSOP_GETPROP;
  }
  if (!bce_->emitAtomOp(propAtomIndex_, op)) {
    //              [stack] # if Get
    //              [stack] PROP
    //              [stack] # if Call
    //              [stack] THIS PROP
    //              [stack] # if Inc/Dec/Compound, Super]
    //              [stack] THIS SUPERBASE PROP
    //              [stack] # if Inc/Dec/Compound, other
    //              [stack] OBJ PROP
    return false;
  }
  if (isCall()) {
    if (!bce_->emit1(JSOP_SWAP)) {
      //            [stack] PROP THIS
      return false;
    }
  }

#ifdef DEBUG
  state_ = State::Get;
#endif
  return true;
}

bool PropOpEmitter::prepareForRhs() {
  MOZ_ASSERT(isSimpleAssignment() || isPropInit() || isCompoundAssignment());
  MOZ_ASSERT_IF(isSimpleAssignment() || isPropInit(), state_ == State::Obj);
  MOZ_ASSERT_IF(isCompoundAssignment(), state_ == State::Get);

  if (isSimpleAssignment() || isPropInit()) {
    // For CompoundAssignment, SUPERBASE is already emitted by emitGet.
    if (isSuper()) {
      if (!bce_->emitSuperBase()) {
        //          [stack] THIS SUPERBASE
        return false;
      }
    }
  }

#ifdef DEBUG
  state_ = State::Rhs;
#endif
  return true;
}

bool PropOpEmitter::skipObjAndRhs() {
  MOZ_ASSERT(state_ == State::Start);
  MOZ_ASSERT(isSimpleAssignment() || isPropInit());

#ifdef DEBUG
  state_ = State::Rhs;
#endif
  return true;
}

bool PropOpEmitter::emitDelete(JSAtom* prop) {
  MOZ_ASSERT_IF(!isSuper(), state_ == State::Obj);
  MOZ_ASSERT_IF(isSuper(), state_ == State::Start);
  MOZ_ASSERT(isDelete());

  if (!prepareAtomIndex(prop)) {
    return false;
  }
  if (isSuper()) {
    if (!bce_->emitSuperBase()) {
      //            [stack] THIS SUPERBASE
      return false;
    }

    // Unconditionally throw when attempting to delete a super-reference.
    if (!bce_->emitUint16Operand(JSOP_THROWMSG, JSMSG_CANT_DELETE_SUPER)) {
      //            [stack] THIS SUPERBASE
      return false;
    }

    // Another wrinkle: Balance the stack from the emitter's point of view.
    // Execution will not reach here, as the last bytecode threw.
    if (!bce_->emit1(JSOP_POP)) {
      //            [stack] THIS
      return false;
    }
  } else {
    JSOp op = bce_->sc->strict() ? JSOP_STRICTDELPROP : JSOP_DELPROP;
    if (!bce_->emitAtomOp(propAtomIndex_, op)) {
      //            [stack] SUCCEEDED
      return false;
    }
  }

#ifdef DEBUG
  state_ = State::Delete;
#endif
  return true;
}

bool PropOpEmitter::emitAssignment(JSAtom* prop) {
  MOZ_ASSERT(isSimpleAssignment() || isPropInit() || isCompoundAssignment());
  MOZ_ASSERT(state_ == State::Rhs);

  if (isSimpleAssignment() || isPropInit()) {
    if (!prepareAtomIndex(prop)) {
      return false;
    }
  }

  MOZ_ASSERT_IF(isPropInit(), !isSuper());
  JSOp setOp =
      isPropInit()
          ? JSOP_INITPROP
          : isSuper() ? bce_->sc->strict() ? JSOP_STRICTSETPROP_SUPER
                                           : JSOP_SETPROP_SUPER
                      : bce_->sc->strict() ? JSOP_STRICTSETPROP : JSOP_SETPROP;
  if (!bce_->emitAtomOp(propAtomIndex_, setOp)) {
    //              [stack] VAL
    return false;
  }

#ifdef DEBUG
  state_ = State::Assignment;
#endif
  return true;
}

bool PropOpEmitter::emitIncDec(JSAtom* prop) {
  MOZ_ASSERT(state_ == State::Obj);
  MOZ_ASSERT(isIncDec());

  if (!emitGet(prop)) {
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
      //            [stack] .. N N
      return false;
    }
  }
  if (!bce_->emit1(incOp)) {
    //              [stack] ... N? N+1
    return false;
  }
  if (isPostIncDec()) {
    if (isSuper()) {
      //            [stack] THIS OBJ N N+1
      if (!bce_->emit2(JSOP_PICK, 3)) {
        //          [stack] OBJ N N+1 THIS
        return false;
      }
      if (!bce_->emit1(JSOP_SWAP)) {
        //          [stack] OBJ N THIS N+1
        return false;
      }
      if (!bce_->emit2(JSOP_PICK, 3)) {
        //          [stack] N THIS N+1 OBJ
        return false;
      }
      if (!bce_->emit1(JSOP_SWAP)) {
        //          [stack] N THIS OBJ N+1
        return false;
      }
    } else {
      //            [stack] OBJ N N+1
      if (!bce_->emit2(JSOP_PICK, 2)) {
        //          [stack] N N+1 OBJ
        return false;
      }
      if (!bce_->emit1(JSOP_SWAP)) {
        //          [stack] N OBJ N+1
        return false;
      }
    }
  }

  JSOp setOp =
      isSuper()
          ? bce_->sc->strict() ? JSOP_STRICTSETPROP_SUPER : JSOP_SETPROP_SUPER
          : bce_->sc->strict() ? JSOP_STRICTSETPROP : JSOP_SETPROP;
  if (!bce_->emitAtomOp(propAtomIndex_, setOp)) {
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

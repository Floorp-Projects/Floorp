/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
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
  : bce_(bce),
    kind_(kind),
    objKind_(objKind)
{}

bool
ElemOpEmitter::prepareForObj()
{
    MOZ_ASSERT(state_ == State::Start);

#ifdef DEBUG
    state_ = State::Obj;
#endif
    return true;
}

bool
ElemOpEmitter::prepareForKey()
{
    MOZ_ASSERT(state_ == State::Obj);

    if (!isSuper() && isIncDec()) {
        if (!bce_->emit1(JSOP_CHECKOBJCOERCIBLE)) {   // OBJ
            return false;
        }
    }
    if (isCall()) {
        if (!bce_->emit1(JSOP_DUP)) {                 // [Super]
            //                                        // THIS THIS
            //                                        // [Other]
            //                                        // OBJ OBJ
            return false;
        }
    }

#ifdef DEBUG
    state_ = State::Key;
#endif
    return true;
}

bool
ElemOpEmitter::emitGet()
{
    MOZ_ASSERT(state_ == State::Key);

    if (isIncDec() || isCompoundAssignment()) {
        if (!bce_->emit1(JSOP_TOID)) {                // [Super]
            //                                        // THIS KEY
            //                                        // [Other]
            //                                        // OBJ KEY
            return false;
        }
    }
    if (isSuper()) {
        if (!bce_->emit1(JSOP_SUPERBASE)) {           // THIS? THIS KEY SUPERBASE
            return false;
        }
    }
    if (isIncDec() || isCompoundAssignment()) {
        if (isSuper()) {
            // There's no such thing as JSOP_DUP3, so we have to be creative.
            // Note that pushing things again is no fewer JSOps.
            if (!bce_->emitDupAt(2)) {                // THIS KEY SUPERBASE THIS
                return false;
            }
            if (!bce_->emitDupAt(2)) {                // THIS KEY SUPERBASE THIS KEY
                return false;
            }
            if (!bce_->emitDupAt(2)) {                // THIS KEY SUPERBASE THIS KEY SUPERBASE
                return false;
            }
        } else {
            if (!bce_->emit1(JSOP_DUP2)) {            // OBJ KEY OBJ KEY
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
    if (!bce_->emitElemOpBase(op)) {                  // [Get]
        //                                            // ELEM
        //                                            // [Call]
        //                                            // THIS ELEM
        //                                            // [Inc/Dec/Assignment,
        //                                            //  Super]
        //                                            // THIS KEY SUPERBASE ELEM
        //                                            // [Inc/Dec/Assignment,
        //                                            //  Other]
        //                                            // OBJ KEY ELEM
        return false;
    }
    if (isCall()) {
        if (!bce_->emit1(JSOP_SWAP)) {                // ELEM THIS
            return false;
        }
    }

#ifdef DEBUG
    state_ = State::Get;
#endif
    return true;
}

bool
ElemOpEmitter::prepareForRhs()
{
    MOZ_ASSERT(isSimpleAssignment() || isCompoundAssignment());
    MOZ_ASSERT_IF(isSimpleAssignment(), state_ == State::Key);
    MOZ_ASSERT_IF(isCompoundAssignment(), state_ == State::Get);

    if (isSimpleAssignment()) {
        // For CompoundAssignment, SUPERBASE is already emitted by emitGet.
        if (isSuper()) {
            if (!bce_->emit1(JSOP_SUPERBASE)) {           // THIS KEY SUPERBASE
                return false;
            }
        }
    }

#ifdef DEBUG
    state_ = State::Rhs;
#endif
    return true;
}

bool
ElemOpEmitter::skipObjAndKeyAndRhs()
{
    MOZ_ASSERT(state_ == State::Start);
    MOZ_ASSERT(isSimpleAssignment());

#ifdef DEBUG
    state_ = State::Rhs;
#endif
    return true;
}

bool
ElemOpEmitter::emitDelete()
{
    MOZ_ASSERT(state_ == State::Key);
    MOZ_ASSERT(isDelete());

    if (isSuper()) {
        if (!bce_->emit1(JSOP_TOID)) {                // THIS KEY
            return false;
        }
        if (!bce_->emit1(JSOP_SUPERBASE)) {           // THIS KEY SUPERBASE
            return false;
        }

        // Unconditionally throw when attempting to delete a super-reference.
        if (!bce_->emitUint16Operand(JSOP_THROWMSG, JSMSG_CANT_DELETE_SUPER)) {
            return false;                             // THIS KEY SUPERBASE
        }

        // Another wrinkle: Balance the stack from the emitter's point of view.
        // Execution will not reach here, as the last bytecode threw.
        if (!bce_->emitPopN(2)) {                     // THIS
            return false;
        }
    } else {
        JSOp op = bce_->sc->strict() ? JSOP_STRICTDELELEM : JSOP_DELELEM;
        if (!bce_->emitElemOpBase(op)){              // SUCCEEDED
            return false;
        }
    }

#ifdef DEBUG
    state_ = State::Delete;
#endif
    return true;
}

bool
ElemOpEmitter::emitAssignment()
{
    MOZ_ASSERT(isSimpleAssignment() || isCompoundAssignment());
    MOZ_ASSERT(state_ == State::Rhs);

    JSOp setOp = isSuper()
                 ? bce_->sc->strict() ? JSOP_STRICTSETELEM_SUPER : JSOP_SETELEM_SUPER
                 : bce_->sc->strict() ? JSOP_STRICTSETELEM : JSOP_SETELEM;
    if (!bce_->emitElemOpBase(setOp)) {               // ELEM
        return false;
    }

#ifdef DEBUG
    state_ = State::Assignment;
#endif
    return true;
}

bool
ElemOpEmitter::emitIncDec()
{
    MOZ_ASSERT(state_ == State::Key);
    MOZ_ASSERT(isIncDec());

    if (!emitGet()) {                                 // ... ELEM
        return false;
    }

    MOZ_ASSERT(state_ == State::Get);

    JSOp binOp = isInc() ? JSOP_ADD : JSOP_SUB;
    if (!bce_->emit1(JSOP_POS)) {                     // ... N
        return false;
    }
    if (isPostIncDec()) {
        if (!bce_->emit1(JSOP_DUP)) {                 // ... N? N
            return false;
        }
    }
    if (!bce_->emit1(JSOP_ONE)) {                     // ... N? N 1
        return false;
    }
    if (!bce_->emit1(binOp)) {                        // ... N? N+1
        return false;
    }
    if (isPostIncDec()) {
        if (isSuper()) {                              // THIS KEY OBJ N N+1
            if (!bce_->emit2(JSOP_PICK, 4)) {         // KEY SUPERBASE N N+1 THIS
                return false;
            }
            if (!bce_->emit2(JSOP_PICK, 4)) {         // SUPERBASE N N+1 THIS KEY
                return false;
            }
            if (!bce_->emit2(JSOP_PICK, 4)) {         // N N+1 THIS KEY SUPERBASE
                return false;
            }
            if (!bce_->emit2(JSOP_PICK, 3)) {         // N THIS KEY SUPERBASE N+1
                return false;
            }
        } else {                                      // OBJ KEY N N+1
            if (!bce_->emit2(JSOP_PICK, 3)) {         // KEY N N+1 OBJ
                return false;
            }
            if (!bce_->emit2(JSOP_PICK, 3)) {         // N N+1 OBJ KEY
                return false;
            }
            if (!bce_->emit2(JSOP_PICK, 2)) {         // N OBJ KEY N+1
                return false;
            }
        }
    }

    JSOp setOp = isSuper()
                 ? (bce_->sc->strict() ? JSOP_STRICTSETELEM_SUPER : JSOP_SETELEM_SUPER)
                 : (bce_->sc->strict() ? JSOP_STRICTSETELEM : JSOP_SETELEM);
    if (!bce_->emitElemOpBase(setOp)) {               // N? N+1
        return false;
    }
    if (isPostIncDec()) {
        if (!bce_->emit1(JSOP_POP)) {                 // N
            return false;
        }
    }

#ifdef DEBUG
    state_ = State::IncDec;
#endif
    return true;
}

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
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
  : bce_(bce),
    kind_(kind),
    objKind_(objKind)
{}

bool
PropOpEmitter::prepareAtomIndex(JSAtom* prop)
{
    if (!bce_->makeAtomIndex(prop, &propAtomIndex_)) {
        return false;
    }
    isLength_ = prop == bce_->cx->names().length;

    return true;
}

bool
PropOpEmitter::prepareForObj()
{
    MOZ_ASSERT(state_ == State::Start);

#ifdef DEBUG
    state_ = State::Obj;
#endif
    return true;
}

bool
PropOpEmitter::emitGet(JSAtom* prop)
{
    MOZ_ASSERT(state_ == State::Obj);

    if (!prepareAtomIndex(prop)) {
        return false;
    }
    if (isCall()) {
        if (!bce_->emit1(JSOP_DUP)) {                 // [Super]
            //                                        // THIS THIS
            //                                        // [Other]
            //                                        // OBJ OBJ
            return false;
        }
    }
    if (isSuper()) {
        if (!bce_->emit1(JSOP_SUPERBASE)) {           // THIS? THIS SUPERBASE
            return false;
        }
    }
    if (isIncDec() || isCompoundAssignment()) {
        if (isSuper()) {
            if (!bce_->emit1(JSOP_DUP2)) {            // THIS SUPERBASE THIS SUPERBASE
                return false;
            }
        } else {
            if (!bce_->emit1(JSOP_DUP)) {             // OBJ OBJ
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
    if (!bce_->emitAtomOp(propAtomIndex_, op)) {      // [Get]
        //                                            // PROP
        //                                            // [Call]
        //                                            // THIS PROP
        //                                            // [Inc/Dec/Compound,
        //                                            //  Super]
        //                                            // THIS SUPERBASE PROP
        //                                            // [Inc/Dec/Compound,
        //                                            //  Other]
        //                                            // OBJ PROP
        return false;
    }
    if (isCall()) {
        if (!bce_->emit1(JSOP_SWAP)) {                // PROP THIS
            return false;
        }
    }

#ifdef DEBUG
    state_ = State::Get;
#endif
    return true;
}

bool
PropOpEmitter::prepareForRhs()
{
    MOZ_ASSERT(isSimpleAssignment() || isCompoundAssignment());
    MOZ_ASSERT_IF(isSimpleAssignment(), state_ == State::Obj);
    MOZ_ASSERT_IF(isCompoundAssignment(), state_ == State::Get);

    if (isSimpleAssignment()) {
        // For CompoundAssignment, SUPERBASE is already emitted by emitGet.
        if (isSuper()) {
            if (!bce_->emit1(JSOP_SUPERBASE)) {       // THIS SUPERBASE
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
PropOpEmitter::skipObjAndRhs()
{
    MOZ_ASSERT(state_ == State::Start);
    MOZ_ASSERT(isSimpleAssignment());

#ifdef DEBUG
    state_ = State::Rhs;
#endif
    return true;
}

bool
PropOpEmitter::emitDelete(JSAtom* prop)
{
    MOZ_ASSERT_IF(!isSuper(), state_ == State::Obj);
    MOZ_ASSERT_IF(isSuper(), state_ == State::Start);
    MOZ_ASSERT(isDelete());

    if (!prepareAtomIndex(prop)) {
        return false;
    }
    if (isSuper()) {
        if (!bce_->emit1(JSOP_SUPERBASE)) {           // THIS SUPERBASE
            return false;
        }

        // Unconditionally throw when attempting to delete a super-reference.
        if (!bce_->emitUint16Operand(JSOP_THROWMSG, JSMSG_CANT_DELETE_SUPER)) {
            return false;                             // THIS SUPERBASE
        }

        // Another wrinkle: Balance the stack from the emitter's point of view.
        // Execution will not reach here, as the last bytecode threw.
        if (!bce_->emit1(JSOP_POP)) {                 // THIS
            return false;
        }
    } else {
        JSOp op = bce_->sc->strict() ? JSOP_STRICTDELPROP : JSOP_DELPROP;
        if (!bce_->emitAtomOp(propAtomIndex_, op)) {  // SUCCEEDED
            return false;
        }
    }

#ifdef DEBUG
    state_ = State::Delete;
#endif
    return true;
}

bool
PropOpEmitter::emitAssignment(JSAtom* prop)
{
    MOZ_ASSERT(isSimpleAssignment() || isCompoundAssignment());
    MOZ_ASSERT(state_ == State::Rhs);

    if (isSimpleAssignment()) {
        if (!prepareAtomIndex(prop)) {
            return false;
        }
    }

    JSOp setOp = isSuper()
                 ? bce_->sc->strict() ? JSOP_STRICTSETPROP_SUPER : JSOP_SETPROP_SUPER
                 : bce_->sc->strict() ? JSOP_STRICTSETPROP : JSOP_SETPROP;
    if (!bce_->emitAtomOp(propAtomIndex_, setOp)) {   // VAL
        return false;
    }

#ifdef DEBUG
    state_ = State::Assignment;
#endif
    return true;
}

bool
PropOpEmitter::emitIncDec(JSAtom* prop)
{
    MOZ_ASSERT(state_ == State::Obj);
    MOZ_ASSERT(isIncDec());

    if (!emitGet(prop)) {
        return false;
    }

    MOZ_ASSERT(state_ == State::Get);

    JSOp binOp = isInc() ? JSOP_ADD : JSOP_SUB;

    if (!bce_->emit1(JSOP_POS)) {                     // ... N
        return false;
    }
    if (isPostIncDec()) {
        if (!bce_->emit1(JSOP_DUP)) {                 // ... N N
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
        if (isSuper()) {                              // THIS OBJ N N+1
            if (!bce_->emit2(JSOP_PICK, 3)) {         // OBJ N N+1 THIS
                return false;
            }
            if (!bce_->emit1(JSOP_SWAP)) {            // OBJ N THIS N+1
                return false;
            }
            if (!bce_->emit2(JSOP_PICK, 3)) {         // N THIS N+1 OBJ
                return false;
            }
            if (!bce_->emit1(JSOP_SWAP)) {            // N THIS OBJ N+1
                return false;
            }
        } else {                                      // OBJ N N+1
            if (!bce_->emit2(JSOP_PICK, 2)) {         // N N+1 OBJ
                return false;
            }
            if (!bce_->emit1(JSOP_SWAP)) {            // N OBJ N+1
                return false;
            }
        }
    }

    JSOp setOp = isSuper()
                 ? bce_->sc->strict() ? JSOP_STRICTSETPROP_SUPER : JSOP_SETPROP_SUPER
                 : bce_->sc->strict() ? JSOP_STRICTSETPROP : JSOP_SETPROP;
    if (!bce_->emitAtomOp(propAtomIndex_, setOp)) {   // N? N+1
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

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/NameOpEmitter.h"

#include "frontend/BytecodeEmitter.h"
#include "frontend/SharedContext.h"
#include "frontend/TDZCheckCache.h"
#include "vm/Opcodes.h"
#include "vm/Scope.h"
#include "vm/StringType.h"

using namespace js;
using namespace js::frontend;

NameOpEmitter::NameOpEmitter(BytecodeEmitter* bce, JSAtom* name, Kind kind)
  : bce_(bce),
    kind_(kind),
    name_(bce_->cx, name),
    loc_(bce_->lookupName(name_))
{}

NameOpEmitter::NameOpEmitter(BytecodeEmitter* bce, JSAtom* name, const NameLocation& loc,
                             Kind kind)
  : bce_(bce),
    kind_(kind),
    name_(bce_->cx, name),
    loc_(loc)
{}

bool
NameOpEmitter::emitGet()
{
    MOZ_ASSERT(state_ == State::Start);

    switch (loc_.kind()) {
      case NameLocation::Kind::Dynamic:
        if (!bce_->emitAtomOp(name_, JSOP_GETNAME)) { // VAL
            return false;
        }
        break;
      case NameLocation::Kind::Global:
        if (!bce_->emitAtomOp(name_, JSOP_GETGNAME)) {// VAL
            return false;
        }
        break;
      case NameLocation::Kind::Intrinsic:
        if (!bce_->emitAtomOp(name_, JSOP_GETINTRINSIC)) {
            return false;                             // VAL
        }
        break;
      case NameLocation::Kind::NamedLambdaCallee:
        if (!bce_->emit1(JSOP_CALLEE)) {              // VAL
            return false;
        }
        break;
      case NameLocation::Kind::Import:
        if (!bce_->emitAtomOp(name_, JSOP_GETIMPORT)) {
            return false;                             // VAL
        }
        break;
      case NameLocation::Kind::ArgumentSlot:
        if (!bce_->emitArgOp(JSOP_GETARG, loc_.argumentSlot())) {
            return false;                             // VAL
        }
        break;
      case NameLocation::Kind::FrameSlot:
        if (loc_.isLexical()) {
            if (!bce_->emitTDZCheckIfNeeded(name_, loc_)) {
                return false;
            }
        }
        if (!bce_->emitLocalOp(JSOP_GETLOCAL, loc_.frameSlot())) {
            return false;                             // VAL
        }
        break;
      case NameLocation::Kind::EnvironmentCoordinate:
        if (loc_.isLexical()) {
            if (!bce_->emitTDZCheckIfNeeded(name_, loc_)) {
                return false;
            }
        }
        if (!bce_->emitEnvCoordOp(JSOP_GETALIASEDVAR, loc_.environmentCoordinate())) {
            return false;                             // VAL
        }
        break;
      case NameLocation::Kind::DynamicAnnexBVar:
        MOZ_CRASH("Synthesized vars for Annex B.3.3 should only be used in initialization");
    }

    if (isCall()) {
        switch (loc_.kind()) {
          case NameLocation::Kind::Dynamic: {
            JSOp thisOp = bce_->needsImplicitThis() ? JSOP_IMPLICITTHIS : JSOP_GIMPLICITTHIS;
            if (!bce_->emitAtomOp(name_, thisOp)) {   // CALLEE THIS
                return false;
            }
            break;
          }
          case NameLocation::Kind::Global:
            if (!bce_->emitAtomOp(name_, JSOP_GIMPLICITTHIS)) {
                return false;                         // CALLEE THIS
            }
            break;
          case NameLocation::Kind::Intrinsic:
          case NameLocation::Kind::NamedLambdaCallee:
          case NameLocation::Kind::Import:
          case NameLocation::Kind::ArgumentSlot:
          case NameLocation::Kind::FrameSlot:
          case NameLocation::Kind::EnvironmentCoordinate:
            if (!bce_->emit1(JSOP_UNDEFINED)) {       // CALLEE UNDEF
                return false;
            }
            break;
          case NameLocation::Kind::DynamicAnnexBVar:
            MOZ_CRASH("Synthesized vars for Annex B.3.3 should only be used in initialization");
        }
    }

#ifdef DEBUG
    state_ = State::Get;
#endif
    return true;
}

bool
NameOpEmitter::prepareForRhs()
{
    MOZ_ASSERT(state_ == State::Start);

    switch (loc_.kind()) {
      case NameLocation::Kind::Dynamic:
      case NameLocation::Kind::Import:
      case NameLocation::Kind::DynamicAnnexBVar:
        if (!bce_->makeAtomIndex(name_, &atomIndex_)) {
            return false;
        }
        if (loc_.kind() == NameLocation::Kind::DynamicAnnexBVar) {
            // Annex B vars always go on the nearest variable environment,
            // even if lexical environments in between contain same-named
            // bindings.
            if (!bce_->emit1(JSOP_BINDVAR)) {         // ENV
                return false;
            }
        } else {
            if (!bce_->emitIndexOp(JSOP_BINDNAME, atomIndex_)) {
                return false;                         // ENV
            }
        }
        emittedBindOp_ = true;
        break;
      case NameLocation::Kind::Global:
        if (!bce_->makeAtomIndex(name_, &atomIndex_)) {
            return false;
        }
        if (loc_.isLexical() && isInitialize()) {
            // INITGLEXICAL always gets the global lexical scope. It doesn't
            // need a BINDGNAME.
            MOZ_ASSERT(bce_->innermostScope()->is<GlobalScope>());
        } else {
            if (!bce_->emitIndexOp(JSOP_BINDGNAME, atomIndex_)) {
                return false;                         // ENV
            }
            emittedBindOp_ = true;
        }
        break;
      case NameLocation::Kind::Intrinsic:
        break;
      case NameLocation::Kind::NamedLambdaCallee:
        break;
      case NameLocation::Kind::ArgumentSlot: {
        // If we assign to a positional formal parameter and the arguments
        // object is unmapped (strict mode or function with
        // default/rest/destructing args), parameters do not alias
        // arguments[i], and to make the arguments object reflect initial
        // parameter values prior to any mutation we create it eagerly
        // whenever parameters are (or might, in the case of calls to eval)
        // assigned.
        FunctionBox* funbox = bce_->sc->asFunctionBox();
        if (funbox->argumentsHasLocalBinding() && !funbox->hasMappedArgsObj()) {
            funbox->setDefinitelyNeedsArgsObj();
        }
        break;
      }
      case NameLocation::Kind::FrameSlot:
        break;
      case NameLocation::Kind::EnvironmentCoordinate:
        break;
    }

    // For compound assignments, first get the LHS value, then emit
    // the RHS and the op.
    if (isCompoundAssignment() || isIncDec()) {
        if (loc_.kind() == NameLocation::Kind::Dynamic) {
            // For dynamic accesses we need to emit GETBOUNDNAME instead of
            // GETNAME for correctness: looking up @@unscopables on the
            // environment chain (due to 'with' environments) must only happen
            // once.
            //
            // GETBOUNDNAME uses the environment already pushed on the stack
            // from the earlier BINDNAME.
            if (!bce_->emit1(JSOP_DUP)) {             // ENV ENV
                return false;
            }
            if (!bce_->emitAtomOp(name_, JSOP_GETBOUNDNAME)) {
                return false;                         // ENV V
            }
        } else {
            if (!emitGet()) {                         // ENV? V
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
NameOpEmitter::emitAssignment()
{
    MOZ_ASSERT(state_ == State::Rhs);

    switch (loc_.kind()) {
      case NameLocation::Kind::Dynamic:
      case NameLocation::Kind::Import:
      case NameLocation::Kind::DynamicAnnexBVar:
        if (!bce_->emitIndexOp(bce_->strictifySetNameOp(JSOP_SETNAME), atomIndex_)) {
            return false;
        }
        break;
      case NameLocation::Kind::Global: {
        JSOp op;
        if (emittedBindOp_) {
            op = bce_->strictifySetNameOp(JSOP_SETGNAME);
        } else {
            op = JSOP_INITGLEXICAL;
        }
        if (!bce_->emitIndexOp(op, atomIndex_)) {
            return false;
        }
        break;
      }
      case NameLocation::Kind::Intrinsic:
        if (!bce_->emitAtomOp(name_, JSOP_SETINTRINSIC)) {
            return false;
        }
        break;
      case NameLocation::Kind::NamedLambdaCallee:
        // Assigning to the named lambda is a no-op in sloppy mode but
        // throws in strict mode.
        if (bce_->sc->strict()) {
            if (!bce_->emit1(JSOP_THROWSETCALLEE)) {
                return false;
            }
        }
        break;
      case NameLocation::Kind::ArgumentSlot:
        if (!bce_->emitArgOp(JSOP_SETARG, loc_.argumentSlot())) {
            return false;
        }
        break;
      case NameLocation::Kind::FrameSlot: {
        JSOp op = JSOP_SETLOCAL;
        if (loc_.isLexical()) {
            if (isInitialize()) {
                op = JSOP_INITLEXICAL;
            } else {
                if (loc_.isConst()) {
                    op = JSOP_THROWSETCONST;
                }

                if (!bce_->emitTDZCheckIfNeeded(name_, loc_)) {
                    return false;
                }
            }
        }
        if (!bce_->emitLocalOp(op, loc_.frameSlot())) {
            return false;
        }
        if (op == JSOP_INITLEXICAL) {
            if (!bce_->innermostTDZCheckCache->noteTDZCheck(bce_, name_, DontCheckTDZ)) {
                return false;
            }
        }
        break;
      }
      case NameLocation::Kind::EnvironmentCoordinate: {
        JSOp op = JSOP_SETALIASEDVAR;
        if (loc_.isLexical()) {
            if (isInitialize()) {
                op = JSOP_INITALIASEDLEXICAL;
            } else {
                if (loc_.isConst()) {
                    op = JSOP_THROWSETALIASEDCONST;
                }

                if (!bce_->emitTDZCheckIfNeeded(name_, loc_)) {
                    return false;
                }
            }
        }
        if (loc_.bindingKind() == BindingKind::NamedLambdaCallee) {
            // Assigning to the named lambda is a no-op in sloppy mode and throws
            // in strict mode.
            op = JSOP_THROWSETALIASEDCONST;
            if (bce_->sc->strict()) {
                if (!bce_->emitEnvCoordOp(op, loc_.environmentCoordinate())) {
                    return false;
                }
            }
        } else {
            if (!bce_->emitEnvCoordOp(op, loc_.environmentCoordinate())) {
                return false;
            }
        }
        if (op == JSOP_INITALIASEDLEXICAL) {
            if (!bce_->innermostTDZCheckCache->noteTDZCheck(bce_, name_, DontCheckTDZ)) {
                return false;
            }
        }
        break;
      }
    }

#ifdef DEBUG
    state_ = State::Assignment;
#endif
    return true;
}

bool
NameOpEmitter::emitIncDec()
{
    MOZ_ASSERT(state_ == State::Start);

    JSOp binOp = isInc() ? JSOP_ADD : JSOP_SUB;
    if (!prepareForRhs()) {                           // ENV? V
        return false;
    }
    if (!bce_->emit1(JSOP_POS)) {                     // ENV? N
        return false;
    }
    if (isPostIncDec()) {
        if (!bce_->emit1(JSOP_DUP)) {                 // ENV? N? N
            return false;
        }
    }
    if (!bce_->emit1(JSOP_ONE)) {                     // ENV? N? N 1
        return false;
    }
    if (!bce_->emit1(binOp)) {                        // ENV? N? N+1
        return false;
    }
    if (isPostIncDec() && emittedBindOp()) {
        if (!bce_->emit2(JSOP_PICK, 2)) {             // N? N+1 ENV?
            return false;
        }
        if (!bce_->emit1(JSOP_SWAP)) {                // N? ENV? N+1
            return false;
        }
    }
    if (!emitAssignment()) {                          // N? N+1
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

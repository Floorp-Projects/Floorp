/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_move_resolver_x86_shared_h__
#define jsion_move_resolver_x86_shared_h__

#include "ion/MoveResolver.h"
#include "ion/IonMacroAssembler.h"

namespace js {
namespace ion {

class CodeGenerator;

class MoveEmitterX86
{
    typedef MoveResolver::Move Move;
    typedef MoveResolver::MoveOperand MoveOperand;

    bool inCycle_;
    MacroAssemblerSpecific &masm;

    // Original stack push value.
    uint32_t pushedAtStart_;

    // These store stack offsets to spill locations, snapshotting
    // codegen->framePushed_ at the time they were allocated. They are -1 if no
    // stack space has been allocated for that particular spill.
    int32_t pushedAtCycle_;
    int32_t pushedAtSpill_;

    // Register that is available for temporary use. It may be assigned
    // InvalidReg. If no corresponding spill space has been assigned,
    // then this register do not need to be spilled.
    Register spilledReg_;

    void assertDone();
    Register tempReg();
    Operand cycleSlot() const;
    Operand spillSlot() const;
    Operand toOperand(const MoveOperand &operand) const;

    void emitMove(const MoveOperand &from, const MoveOperand &to);
    void emitDoubleMove(const MoveOperand &from, const MoveOperand &to);
    void breakCycle(const MoveOperand &from, const MoveOperand &to, Move::Kind kind);
    void completeCycle(const MoveOperand &from, const MoveOperand &to, Move::Kind kind);
    void emit(const Move &move);

  public:
    MoveEmitterX86(MacroAssemblerSpecific &masm);
    ~MoveEmitterX86();
    void emit(const MoveResolver &moves);
    void finish();
};

typedef MoveEmitterX86 MoveEmitter;

} // ion
} // js

#endif // jsion_move_resolver_x86_shared_h__


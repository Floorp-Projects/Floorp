/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined jsjaeger_imm_sync_h__ && defined JS_METHODJIT && defined JS_NUNBOX32
#define jsjaeger_imm_sync_h__

#include "methodjit/MachineRegs.h"
#include "methodjit/FrameEntry.h"
#include "CodeGenIncludes.h"

namespace js {
namespace mjit {

class FrameState;

/*
 * This is a structure nestled within the FrameState used for safely syncing
 * registers to memory during transitions from the fast path into a slow path
 * stub call. During this process, the frame itself is immutable, and we may
 * run out of registers needed to remat copies.
 *
 * This structure maintains a mapping of the tracker used to perform ad-hoc
 * register allocation.
 */
class ImmutableSync
{
    typedef JSC::MacroAssembler::RegisterID RegisterID;
    typedef JSC::MacroAssembler::Address Address;

    struct SyncEntry {
        /*
         * NB: clobbered and sync mean the same thing: the register associated
         * in the FrameEntry is no longer valid, and has been written back.
         *
         * They are separated for readability.
         */
        uint32_t generation;
        bool dataClobbered;
        bool typeClobbered;
        bool hasDataReg;
        bool hasTypeReg;
        bool learnedType;
        RegisterID dataReg;
        RegisterID typeReg;
        JSValueType type;

        void reset(uint32_t gen) {
            dataClobbered = false;
            typeClobbered = false;
            hasDataReg = false;
            hasTypeReg = false;
            learnedType = false;
            generation = gen;
        }
    };

  public:
    ImmutableSync();
    ~ImmutableSync();
    bool init(JSContext *cx, const FrameState &frame, uint32_t nentries);

    void reset(Assembler *masm, Registers avail, FrameEntry *top, FrameEntry *bottom);
    void sync(FrameEntry *fe);

  private:
    void syncCopy(FrameEntry *fe);
    void syncNormal(FrameEntry *fe);
    RegisterID ensureDataReg(FrameEntry *fe, SyncEntry &e);
    RegisterID ensureTypeReg(FrameEntry *fe, SyncEntry &e);

    RegisterID allocReg();
    void freeReg(RegisterID reg);

    /* To be called only by allocReg. */
    RegisterID doAllocReg();

    inline SyncEntry &entryFor(FrameEntry *fe);

    bool shouldSyncType(FrameEntry *fe, SyncEntry &e);
    bool shouldSyncData(FrameEntry *fe, SyncEntry &e);

  private:
    JSContext *cx;
    SyncEntry *entries;
    const FrameState *frame;
    uint32_t nentries;
    Registers avail;
    Assembler *masm;
    SyncEntry *regs[Assembler::TotalRegisters];
    FrameEntry *top;
    FrameEntry *bottom;
    uint32_t generation;
};

} /* namespace mjit */
} /* namespace js */

#endif /* jsjaeger_imm_sync_h__ */


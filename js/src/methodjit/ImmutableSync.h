/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Brendan Eich <brendan@mozilla.org>
 *
 * Contributor(s):
 *   David Anderson <danderson@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#if !defined jsjaeger_imm_sync_h__ && defined JS_METHODJIT
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
        bool dataSynced;
        bool typeSynced;
        bool dataClobbered;
        bool typeClobbered;
        RegisterID dataReg;
        RegisterID typeReg;
        bool hasDataReg;
        bool hasTypeReg;
        bool learnedType;
        JSValueType type;
    };

  public:
    ImmutableSync(JSContext *cx, const FrameState &frame);
    ~ImmutableSync();
    bool init(uint32 nentries);

    void reset(Assembler *masm, Registers avail, uint32 n,
               FrameEntry *bottom);
    void sync(FrameEntry *fe);

  private:
    void syncCopy(FrameEntry *fe);
    void syncNormal(FrameEntry *fe);
    RegisterID ensureDataReg(FrameEntry *fe, SyncEntry &e);
    RegisterID ensureTypeReg(FrameEntry *fe, SyncEntry &e);
    RegisterID allocReg();

    inline SyncEntry &entryFor(FrameEntry *fe);

    bool shouldSyncType(FrameEntry *fe, SyncEntry &e);
    bool shouldSyncData(FrameEntry *fe, SyncEntry &e);

  private:
    JSContext *cx;
    SyncEntry *entries;
    const FrameState &frame;
    uint32 nentries;
    Registers avail;
    Assembler *masm;
    SyncEntry *regs[Assembler::TotalRegisters];
    FrameEntry *bottom;
};

} /* namespace mjit */
} /* namespace js */

#endif /* jsjaeger_imm_sync_h__ */


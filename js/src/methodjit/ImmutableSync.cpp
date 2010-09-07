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
#include "FrameEntry.h"
#include "FrameState.h"
#include "FrameState-inl.h"
#include "ImmutableSync.h"

using namespace js;
using namespace js::mjit;

ImmutableSync::ImmutableSync(JSContext *cx, const FrameState &frame)
  : cx(cx), entries(NULL), frame(frame)
{
}

ImmutableSync::~ImmutableSync()
{
    cx->free(entries);
}

bool
ImmutableSync::init(uint32 nentries)
{
    entries = (SyncEntry *)cx->malloc(sizeof(SyncEntry) * nentries);
    return !!entries;
}

void
ImmutableSync::reset(Assembler *masm, Registers avail, uint32 n,
                     FrameEntry *bottom)
{
    this->avail = avail;
    this->nentries = n;
    this->masm = masm;
    this->bottom = bottom;
    memset(entries, 0, sizeof(SyncEntry) * nentries);
    memset(regs, 0, sizeof(regs));
}

JSC::MacroAssembler::RegisterID
ImmutableSync::allocReg()
{
    if (!avail.empty())
        return avail.takeAnyReg();

    uint32 lastResort = FrameState::InvalidIndex;
    uint32 evictFromFrame = FrameState::InvalidIndex;

    /* Find something to evict. */
    for (uint32 i = 0; i < JSC::MacroAssembler::TotalRegisters; i++) {
        RegisterID reg = RegisterID(i);
        if (!(Registers::maskReg(reg) & Registers::AvailRegs))
            continue;

        lastResort = 0;

        if (!regs[i]) {
            /* If the frame does not own this register, take it! */
            FrameEntry *fe = frame.regstate[i].fe;
            if (!fe)
                return reg;

            /*
             * The Reifier does not own this register, but the frame does.
             * This must mean that we've not yet processed this entry, and
             * that it's data has not been clobbered.
             */
            JS_ASSERT(fe->trackerIndex() < nentries);

            evictFromFrame = i;

            /*
             * If not copied, we can sync and not have to load again later.
             * That's about as good as it gets, so just break out now.
             */
            if (!fe->isCopied())
                break;
        }
    }

    if (evictFromFrame != FrameState::InvalidIndex) {
        FrameEntry *fe = frame.regstate[evictFromFrame].fe;
        SyncEntry &e = entryFor(fe);
        if (frame.regstate[evictFromFrame].type == RematInfo::TYPE) {
            JS_ASSERT(!e.typeClobbered);
            e.typeSynced = true;
            e.typeClobbered = true;
            masm->storeTypeTag(fe->type.reg(), frame.addressOf(fe));
        } else {
            JS_ASSERT(!e.dataClobbered);
            e.dataSynced = true;
            e.dataClobbered = true;
            masm->storePayload(fe->data.reg(), frame.addressOf(fe));
        }
        return RegisterID(evictFromFrame);
    }

    JS_ASSERT(lastResort != FrameState::InvalidIndex);
    JS_ASSERT(regs[lastResort]);

    SyncEntry *e = regs[lastResort];
    RegisterID reg = RegisterID(lastResort);
    if (e->hasDataReg && e->dataReg == reg) {
        e->hasDataReg = false;
    } else if (e->hasTypeReg && e->typeReg == reg) {
        e->hasTypeReg = false;
    } else {
        JS_NOT_REACHED("no way");
    }

    return reg;
}

inline ImmutableSync::SyncEntry &
ImmutableSync::entryFor(FrameEntry *fe)
{
    JS_ASSERT(fe->trackerIndex() < nentries);
    return entries[fe->trackerIndex()];
}

void
ImmutableSync::sync(FrameEntry *fe)
{
    JS_ASSERT(nentries);
    if (fe->isCopy())
        syncCopy(fe);
    else
        syncNormal(fe);
    nentries--;
}

bool
ImmutableSync::shouldSyncType(FrameEntry *fe, SyncEntry &e)
{
    if (fe->type.inRegister() && !e.typeClobbered)
        return true;
    if (e.hasTypeReg)
        return true;
    return frame.inTryBlock || fe >= bottom;
}

bool
ImmutableSync::shouldSyncData(FrameEntry *fe, SyncEntry &e)
{
    if (fe->data.inRegister() && !e.dataClobbered)
        return true;
    if (e.hasDataReg)
        return true;
    return frame.inTryBlock || fe >= bottom;
}

JSC::MacroAssembler::RegisterID
ImmutableSync::ensureTypeReg(FrameEntry *fe, SyncEntry &e)
{
    if (fe->type.inRegister() && !e.typeClobbered)
        return fe->type.reg();
    if (e.hasTypeReg)
        return e.typeReg;
    e.typeReg = allocReg();
    e.hasTypeReg = true;
    regs[e.typeReg] = &e;
    masm->loadTypeTag(frame.addressOf(fe), e.typeReg);
    return e.typeReg;
}

JSC::MacroAssembler::RegisterID
ImmutableSync::ensureDataReg(FrameEntry *fe, SyncEntry &e)
{
    if (fe->data.inRegister() && !e.dataClobbered)
        return fe->data.reg();
    if (e.hasDataReg)
        return e.dataReg;
    e.dataReg = allocReg();
    e.hasDataReg = true;
    regs[e.dataReg] = &e;
    masm->loadPayload(frame.addressOf(fe), e.dataReg);
    return e.dataReg;
}

void
ImmutableSync::syncCopy(FrameEntry *fe)
{
    if (!frame.inTryBlock && fe < bottom)
        return;

    FrameEntry *backing = fe->copyOf();
    SyncEntry &e = entryFor(backing);

    JS_ASSERT(!backing->isConstant());

    Address addr = frame.addressOf(fe);

    if (fe->isTypeKnown() && !e.learnedType) {
        e.learnedType = true;
        e.type = fe->getKnownType();
    }

    if (!fe->data.synced())
        masm->storePayload(ensureDataReg(backing, e), addr);

    if (!fe->type.synced()) {
        if (e.learnedType)
            masm->storeTypeTag(ImmType(e.type), addr);
        else
            masm->storeTypeTag(ensureTypeReg(backing, e), addr);
    }
}

void
ImmutableSync::syncNormal(FrameEntry *fe)
{
    SyncEntry &e = entryFor(fe);

    Address addr = frame.addressOf(fe);

    if (fe->isTypeKnown()) {
        e.learnedType = true;
        e.type = fe->getKnownType();
    }

    if (!fe->data.synced() && !e.dataSynced && shouldSyncData(fe, e)) {
        if (fe->isConstant()) {
            masm->storeValue(fe->getValue(), addr);
            return;
        }
        masm->storePayload(ensureDataReg(fe, e), addr);
    }

    if (!fe->type.synced() && !e.typeSynced && shouldSyncType(fe, e)) {
        if (e.learnedType)
            masm->storeTypeTag(ImmType(e.type), addr);
        else
            masm->storeTypeTag(ensureTypeReg(fe, e), addr);
    }

    if (e.hasDataReg) {
        avail.putReg(e.dataReg);
        regs[e.dataReg] = NULL;
    } else if (!e.dataClobbered && fe->data.inRegister() && frame.regstate[fe->data.reg()].fe) {
        avail.putReg(fe->data.reg());
    }

    if (e.hasTypeReg) {
        avail.putReg(e.typeReg);
        regs[e.typeReg] = NULL;
    } else if (!e.typeClobbered && fe->type.inRegister() && frame.regstate[fe->type.reg()].fe) {
        avail.putReg(fe->type.reg());
    }
}


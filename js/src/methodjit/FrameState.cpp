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
#include "jscntxt.h"
#include "FrameState.h"
#include "FrameState-inl.h"

using namespace js;
using namespace js::mjit;

/* Because of Value alignment */
JS_STATIC_ASSERT(sizeof(FrameEntry) % 8 == 0);

FrameState::FrameState(JSContext *cx, JSScript *script, Assembler &masm)
  : cx(cx), script(script), masm(masm), entries(NULL), reifier(cx, *this),
    inTryBlock(false)
{
}

FrameState::~FrameState()
{
    cx->free(entries);
}

bool
FrameState::init(uint32 nargs)
{
    this->nargs = nargs;

    uint32 nslots = script->nslots + nargs;
    if (!nslots) {
        sp = spBase = locals = args = NULL;
        return true;
    }

    eval = script->usesEval || cx->compartment->debugMode;

    size_t totalBytes = sizeof(FrameEntry) * nslots +         // entries[]
                        sizeof(FrameEntry *) * nslots +       // tracker.entries
                        (eval ? 0 : sizeof(uint32) * nslots); // closedVars[]

    uint8 *cursor = (uint8 *)cx->calloc(totalBytes);
    if (!cursor)
        return false;

    if (!reifier.init(nslots))
        return false;

    entries = (FrameEntry *)cursor;
    cursor += sizeof(FrameEntry) * nslots;

    args = entries;
    locals = args + nargs;
    spBase = locals + script->nfixed;
    sp = spBase;

    tracker.entries = (FrameEntry **)cursor;
    cursor += sizeof(FrameEntry *) * nslots;

    if (!eval && nslots) {
        escaping = (uint32 *)cursor;
        cursor += sizeof(uint32) * nslots;
    }

    JS_ASSERT(reinterpret_cast<uint8 *>(entries) + totalBytes == cursor);

    return true;
}

void
FrameState::takeReg(RegisterID reg)
{
    if (freeRegs.hasReg(reg)) {
        freeRegs.takeReg(reg);
        JS_ASSERT(!regstate[reg].usedBy());
    } else {
        JS_ASSERT(regstate[reg].fe());
        evictReg(reg);
        regstate[reg].forget();
    }
}

void
FrameState::evictReg(RegisterID reg)
{
    FrameEntry *fe = regstate[reg].fe();

    if (regstate[reg].type() == RematInfo::TYPE) {
        if (!fe->type.synced()) {
            syncType(fe, addressOf(fe), masm);
            fe->type.sync();
        }
        fe->type.setMemory();
    } else {
        if (!fe->data.synced()) {
            syncData(fe, addressOf(fe), masm);
            fe->data.sync();
        }
        fe->data.setMemory();
    }
}

JSC::MacroAssembler::RegisterID
FrameState::evictSomeReg(uint32 mask)
{
#ifdef DEBUG
    bool fallbackSet = false;
#endif
    RegisterID fallback = Registers::ReturnReg;

    for (uint32 i = 0; i < JSC::MacroAssembler::TotalRegisters; i++) {
        RegisterID reg = RegisterID(i);

        /* Register is not allocatable, don't bother.  */
        if (!(Registers::maskReg(reg) & mask))
            continue;

        /* Register is not owned by the FrameState. */
        FrameEntry *fe = regstate[i].fe();
        if (!fe)
            continue;

        /* Try to find a candidate... that doesn't need spilling. */
#ifdef DEBUG
        fallbackSet = true;
#endif
        fallback = reg;

        if (regstate[i].type() == RematInfo::TYPE && fe->type.synced()) {
            fe->type.setMemory();
            return fallback;
        }
        if (regstate[i].type() == RematInfo::DATA && fe->data.synced()) {
            fe->data.setMemory();
            return fallback;
        }
    }

    JS_ASSERT(fallbackSet);

    evictReg(fallback);
    return fallback;
}


void
FrameState::syncAndForgetEverything()
{
    syncAndKill(Registers(Registers::AvailRegs), Uses(frameDepth()));
    forgetEverything();
}

void
FrameState::resetInternalState()
{
    for (uint32 i = 0; i < tracker.nentries; i++)
        tracker[i]->untrack();

    tracker.reset();
    freeRegs.reset();
}

void
FrameState::discardFrame()
{
    resetInternalState();

    memset(regstate, 0, sizeof(regstate));
}

void
FrameState::forgetEverything()
{
    resetInternalState();

#ifdef DEBUG
    for (uint32 i = 0; i < JSC::MacroAssembler::TotalRegisters; i++) {
        JS_ASSERT(!regstate[i].usedBy());
    }
#endif
}

void
FrameState::storeTo(FrameEntry *fe, Address address, bool popped)
{
    if (fe->isConstant()) {
        masm.storeValue(fe->getValue(), address);
        return;
    }

    if (fe->isCopy())
        fe = fe->copyOf();

    /* Cannot clobber the address's register. */
    JS_ASSERT(!freeRegs.hasReg(address.base));

    /* If loading from memory, ensure destination differs. */
    JS_ASSERT_IF((fe->type.inMemory() || fe->data.inMemory()),
                 addressOf(fe).base != address.base ||
                 addressOf(fe).offset != address.offset);

#if defined JS_PUNBOX64
    if (fe->type.inMemory() && fe->data.inMemory()) {
        /* Future optimization: track that the Value is in a register. */
        RegisterID vreg = Registers::ValueReg;
        masm.loadPtr(addressOf(fe), vreg);
        masm.storePtr(vreg, address);
        return;
    }

    /* Get a register for the payload. */
    bool wasInRegister = fe->data.inRegister();
    MaybeRegisterID dreg;

    if (fe->data.inRegister()) {
        dreg = fe->data.reg();
    } else {
        JS_ASSERT(fe->data.inMemory());
        dreg = popped ? allocReg() : allocReg(fe, RematInfo::DATA);
        masm.loadPayload(addressOf(fe), dreg.reg());
        if (!popped)
            fe->data.setRegister(dreg.reg());
    }
    
    /* Store the Value. */
    if (fe->type.inRegister()) {
        masm.storeValueFromComponents(fe->type.reg(), dreg.reg(), address);
    } else if (fe->isTypeKnown()) {
        masm.storeValueFromComponents(ImmType(fe->getKnownType()), dreg.reg(), address);
    } else {
        pinReg(dreg.reg());
        JS_ASSERT(fe->type.inMemory());
        RegisterID treg = popped ? allocReg() : allocReg(fe, RematInfo::TYPE);
        masm.loadTypeTag(addressOf(fe), treg);
        masm.storeValueFromComponents(treg, dreg.reg(), address);
        if (popped)
            freeReg(treg);
        else
            fe->type.setRegister(treg);
        unpinReg(dreg.reg());
    }

    /* If register is untracked, free it. */
    if (!wasInRegister && popped)
        freeReg(dreg.reg());

#elif defined JS_NUNBOX32

    if (fe->data.inRegister()) {
        masm.storePayload(fe->data.reg(), address);
    } else {
        JS_ASSERT(fe->data.inMemory());
        RegisterID reg = popped ? allocReg() : allocReg(fe, RematInfo::DATA);
        masm.loadPayload(addressOf(fe), reg);
        masm.storePayload(reg, address);
        if (popped)
            freeReg(reg);
        else
            fe->data.setRegister(reg);
    }

    if (fe->isTypeKnown()) {
        masm.storeTypeTag(ImmType(fe->getKnownType()), address);
    } else if (fe->type.inRegister()) {
        masm.storeTypeTag(fe->type.reg(), address);
    } else {
        JS_ASSERT(fe->type.inMemory());
        RegisterID reg = popped ? allocReg() : allocReg(fe, RematInfo::TYPE);
        masm.loadTypeTag(addressOf(fe), reg);
        masm.storeTypeTag(reg, address);
        if (popped)
            freeReg(reg);
        else
            fe->type.setRegister(reg);
    }
#endif
}

#ifdef DEBUG
void
FrameState::assertValidRegisterState() const
{
    Registers checkedFreeRegs;

    for (uint32 i = 0; i < tracker.nentries; i++) {
        FrameEntry *fe = tracker[i];
        if (fe >= sp)
            continue;

        JS_ASSERT(i == fe->trackerIndex());
        JS_ASSERT_IF(fe->isCopy(),
                     fe->trackerIndex() > fe->copyOf()->trackerIndex());
        JS_ASSERT_IF(fe->isCopy(), fe > fe->copyOf());
        JS_ASSERT_IF(fe->isCopy(), !fe->type.inRegister() && !fe->data.inRegister());
        JS_ASSERT_IF(fe->isCopy(), fe->copyOf() < sp);
        JS_ASSERT_IF(fe->isCopy(), fe->copyOf()->isCopied());

        if (fe->isCopy())
            continue;
        if (fe->type.inRegister()) {
            checkedFreeRegs.takeReg(fe->type.reg());
            JS_ASSERT(regstate[fe->type.reg()].fe() == fe);
        }
        if (fe->data.inRegister()) {
            checkedFreeRegs.takeReg(fe->data.reg());
            JS_ASSERT(regstate[fe->data.reg()].fe() == fe);
        }
    }

    JS_ASSERT(checkedFreeRegs == freeRegs);

    for (uint32 i = 0; i < JSC::MacroAssembler::TotalRegisters; i++) {
        JS_ASSERT(!regstate[i].isPinned());
        JS_ASSERT_IF(regstate[i].fe(), !freeRegs.hasReg(RegisterID(i)));
        JS_ASSERT_IF(regstate[i].fe(), regstate[i].fe()->isTracked());
    }
}
#endif

void
FrameState::syncFancy(Assembler &masm, Registers avail, FrameEntry *resumeAt,
                      FrameEntry *bottom) const
{
    reifier.reset(&masm, avail, resumeAt, bottom);

    for (FrameEntry *fe = resumeAt; fe >= bottom; fe--) {
        if (!fe->isTracked())
            continue;

        reifier.sync(fe);
    }
}

void
FrameState::sync(Assembler &masm, Uses uses) const
{
    if (!entries)
        return;

    /* Sync all registers up-front. */
    for (uint32 i = 0; i < JSC::MacroAssembler::TotalRegisters; i++) {
        RegisterID reg = RegisterID(i);
        FrameEntry *fe = regstate[reg].usedBy();
        if (!fe)
            continue;

        JS_ASSERT(fe->isTracked());

        if (regstate[reg].type() == RematInfo::DATA) {
            JS_ASSERT(fe->data.reg() == reg);
            if (!fe->data.synced())
                syncData(fe, addressOf(fe), masm);
        } else {
            JS_ASSERT(fe->type.reg() == reg);
            if (!fe->type.synced())
                syncType(fe, addressOf(fe), masm);
        }
    }

    /*
     * Keep track of free registers using a bitmask. If we have to drop into
     * syncFancy(), then this mask will help avoid eviction.
     */
    Registers avail(freeRegs);
    Registers temp(Registers::TempRegs);

    FrameEntry *bottom = sp - uses.nuses;

    for (FrameEntry *fe = sp - 1; fe >= bottom; fe--) {
        if (!fe->isTracked())
            continue;

        Address address = addressOf(fe);

        if (!fe->isCopy()) {
            /*
             * If this |fe| has registers, track them as available. They've
             * already been synced. Otherwise, see if a constant needs to be
             * synced.
             */
            if (fe->data.inRegister()) {
                avail.putReg(fe->data.reg());
            } else if (!fe->data.synced()) {
                syncData(fe, address, masm);
                if (fe->isConstant())
                    continue;
            }

            if (fe->type.inRegister())
                avail.putReg(fe->type.reg());
            else if (!fe->type.synced())
                syncType(fe, address, masm);
        } else {
            FrameEntry *backing = fe->copyOf();
            JS_ASSERT(backing != fe);
            JS_ASSERT(!backing->isConstant() && !fe->isConstant());

            /*
             * If the copy is backed by something not in a register, fall back
             * to a slower sync algorithm.
             */
            if ((!fe->type.synced() && !backing->type.inRegister()) ||
                (!fe->data.synced() && !backing->data.inRegister())) {
                syncFancy(masm, avail, fe, bottom);
                return;
            }

            if (!fe->type.synced()) {
                /* :TODO: we can do better, the type is learned for all copies. */
                if (fe->isTypeKnown()) {
                    //JS_ASSERT(fe->getTypeTag() == backing->getTypeTag());
                    masm.storeTypeTag(ImmType(fe->getKnownType()), address);
                } else {
                    masm.storeTypeTag(backing->type.reg(), address);
                }
            }

            if (!fe->data.synced())
                masm.storePayload(backing->data.reg(), address);
        }
    }
}

void
FrameState::syncAndKill(Registers kill, Uses uses, Uses ignore)
{
    FrameEntry *spStop = sp - ignore.nuses;

    /* Sync all kill-registers up-front. */
    Registers search(kill.freeMask & ~freeRegs.freeMask);
    while (!search.empty()) {
        RegisterID reg = search.takeAnyReg();
        FrameEntry *fe = regstate[reg].usedBy();
        if (!fe || fe >= spStop)
            continue;

        JS_ASSERT(fe->isTracked());

        if (regstate[reg].type() == RematInfo::DATA) {
            JS_ASSERT(fe->data.reg() == reg);
            if (!fe->data.synced()) {
                syncData(fe, addressOf(fe), masm);
                fe->data.sync();
            }
        } else {
            JS_ASSERT(fe->type.reg() == reg);
            if (!fe->type.synced()) {
                syncType(fe, addressOf(fe), masm);
                fe->type.sync();
            }
        }
    }

    uint32 maxvisits = tracker.nentries;
    FrameEntry *bottom = sp - uses.nuses;

    for (FrameEntry *fe = sp - 1; fe >= bottom && maxvisits; fe--) {
        if (!fe->isTracked())
            continue;

        maxvisits--;

        if (fe >= spStop)
            continue;

        Address address = addressOf(fe);
        FrameEntry *backing = fe;

        if (fe->isCopy())
            backing = fe->copyOf();

        if (!fe->data.synced()) {
            if (backing != fe && backing->data.inMemory())
                tempRegForData(backing);
            syncData(backing, address, masm);
            fe->data.sync();
            if (fe->isConstant() && !fe->type.synced()) {
                fe->type.sync();
            } else if (fe->data.inRegister() && kill.hasReg(fe->data.reg())) {
                forgetReg(fe->data.reg());
                fe->data.setMemory();
            }
        }
        if (!fe->type.synced()) {
            if (backing != fe && backing->type.inMemory())
                tempRegForType(backing);
            syncType(backing, address, masm);
            fe->type.sync();
            if (fe->type.inRegister() && kill.hasReg(fe->type.reg())) {
                forgetReg(fe->type.reg());
                fe->type.setMemory();
            }
        }
    }

    /*
     * Anything still alive at this point is guaranteed to be synced. However,
     * it is necessary to evict temporary registers.
     */
    search = Registers(kill.freeMask & ~freeRegs.freeMask);
    while (!search.empty()) {
        RegisterID reg = search.takeAnyReg();
        FrameEntry *fe = regstate[reg].usedBy();
        if (!fe || fe >= spStop)
            continue;

        JS_ASSERT(fe->isTracked());

        if (regstate[reg].type() == RematInfo::DATA) {
            JS_ASSERT(fe->data.reg() == reg);
            JS_ASSERT(fe->data.synced());
            fe->data.setMemory();
        } else {
            JS_ASSERT(fe->type.reg() == reg);
            JS_ASSERT(fe->type.synced());
            fe->type.setMemory();
        }

        forgetReg(reg);
    }
}

void
FrameState::merge(Assembler &masm, Changes changes) const
{
    Registers search(Registers::AvailRegs & ~freeRegs.freeMask);

    while (!search.empty()) {
        RegisterID reg = search.peekReg();
        FrameEntry *fe = regstate[reg].usedBy();

        if (!fe) {
            search.takeReg(reg);
            continue;
        }

        if (fe->data.inRegister() && fe->type.inRegister()) {
            search.takeReg(fe->data.reg());
            search.takeReg(fe->type.reg());
            masm.loadValueAsComponents(addressOf(fe), fe->type.reg(), fe->data.reg());
        } else {
            if (fe->data.inRegister()) {
                search.takeReg(fe->data.reg());
                masm.loadPayload(addressOf(fe), fe->data.reg());
            }
            if (fe->type.inRegister()) {
                search.takeReg(fe->type.reg());
                masm.loadTypeTag(addressOf(fe), fe->type.reg());
            }
        }
    }
}

JSC::MacroAssembler::RegisterID
FrameState::copyDataIntoReg(FrameEntry *fe)
{
    return copyDataIntoReg(this->masm, fe);
}

void
FrameState::copyDataIntoReg(FrameEntry *fe, RegisterID hint)
{
    JS_ASSERT(!fe->data.isConstant());

    if (fe->isCopy())
        fe = fe->copyOf();

    if (!fe->data.inRegister())
        tempRegForData(fe);

    RegisterID reg = fe->data.reg();
    if (reg == hint) {
        if (freeRegs.empty()) {
            if (!fe->data.synced())
                syncData(fe, addressOf(fe), masm);
            fe->data.setMemory();
        } else {
            reg = allocReg();
            masm.move(hint, reg);
            fe->data.setRegister(reg);
            regstate[reg].associate(regstate[hint].fe(), RematInfo::DATA);
        }
        regstate[hint].forget();
    } else {
        pinReg(reg);
        takeReg(hint);
        unpinReg(reg);
        masm.move(reg, hint);
    }
}

JSC::MacroAssembler::RegisterID
FrameState::copyDataIntoReg(Assembler &masm, FrameEntry *fe)
{
    JS_ASSERT(!fe->data.isConstant());

    if (fe->isCopy())
        fe = fe->copyOf();

    if (fe->data.inRegister()) {
        RegisterID reg = fe->data.reg();
        if (freeRegs.empty()) {
            if (!fe->data.synced())
                syncData(fe, addressOf(fe), masm);
            fe->data.setMemory();
            regstate[reg].forget();
        } else {
            RegisterID newReg = allocReg();
            masm.move(reg, newReg);
            reg = newReg;
        }
        return reg;
    }

    RegisterID reg = allocReg();

    if (!freeRegs.empty())
        masm.move(tempRegForData(fe), reg);
    else
        masm.loadPayload(addressOf(fe),reg);

    return reg;
}

JSC::MacroAssembler::RegisterID
FrameState::copyTypeIntoReg(FrameEntry *fe)
{
    JS_ASSERT(!fe->type.isConstant());

    if (fe->isCopy())
        fe = fe->copyOf();

    if (fe->type.inRegister()) {
        RegisterID reg = fe->type.reg();
        if (freeRegs.empty()) {
            if (!fe->type.synced())
                syncType(fe, addressOf(fe), masm);
            fe->type.setMemory();
            regstate[reg].forget();
        } else {
            RegisterID newReg = allocReg();
            masm.move(reg, newReg);
            reg = newReg;
        }
        return reg;
    }

    RegisterID reg = allocReg();

    if (!freeRegs.empty())
        masm.move(tempRegForType(fe), reg);
    else
        masm.loadTypeTag(addressOf(fe), reg);

    return reg;
}

JSC::MacroAssembler::RegisterID
FrameState::copyInt32ConstantIntoReg(FrameEntry *fe)
{
    return copyInt32ConstantIntoReg(masm, fe);
}

JSC::MacroAssembler::RegisterID
FrameState::copyInt32ConstantIntoReg(Assembler &masm, FrameEntry *fe)
{
    JS_ASSERT(fe->data.isConstant());

    if (fe->isCopy())
        fe = fe->copyOf();

    RegisterID reg = allocReg();
    masm.move(Imm32(fe->getValue().toInt32()), reg);
    return reg;
}

JSC::MacroAssembler::FPRegisterID
FrameState::copyEntryIntoFPReg(FrameEntry *fe, FPRegisterID fpreg)
{
    return copyEntryIntoFPReg(this->masm, fe, fpreg);
}

JSC::MacroAssembler::FPRegisterID
FrameState::copyEntryIntoFPReg(Assembler &masm, FrameEntry *fe, FPRegisterID fpreg)
{
    if (fe->isCopy())
        fe = fe->copyOf();

    /* The entry must be synced to memory. */
    if (fe->data.isConstant()) {
        if (!fe->data.synced())
            syncData(fe, addressOf(fe), masm);
        if (!fe->type.synced())
            syncType(fe, addressOf(fe), masm);
    } else {
        if (fe->data.inRegister() && !fe->data.synced())
            syncData(fe, addressOf(fe), masm);
        if (fe->type.inRegister() && !fe->type.synced())
            syncType(fe, addressOf(fe), masm);
    }

    masm.loadDouble(addressOf(fe), fpreg);
    return fpreg;
}

JSC::MacroAssembler::RegisterID
FrameState::ownRegForType(FrameEntry *fe)
{
    JS_ASSERT(!fe->type.isConstant());

    RegisterID reg;
    if (fe->isCopy()) {
        /* For now, just do an extra move. The reg must be mutable. */
        FrameEntry *backing = fe->copyOf();
        if (!backing->type.inRegister()) {
            JS_ASSERT(backing->type.inMemory());
            tempRegForType(backing);
        }

        if (freeRegs.empty()) {
            /* For now... just steal the register that already exists. */
            if (!backing->type.synced())
                syncType(backing, addressOf(backing), masm);
            reg = backing->type.reg();
            backing->type.setMemory();
            regstate[reg].forget();
        } else {
            reg = allocReg();
            masm.move(backing->type.reg(), reg);
        }
        return reg;
    }

    if (fe->type.inRegister()) {
        reg = fe->type.reg();

        /* Remove ownership of this register. */
        JS_ASSERT(regstate[reg].fe() == fe);
        JS_ASSERT(regstate[reg].type() == RematInfo::TYPE);
        regstate[reg].forget();
        fe->type.invalidate();
    } else {
        JS_ASSERT(fe->type.inMemory());
        reg = allocReg();
        masm.loadTypeTag(addressOf(fe), reg);
    }
    return reg;
}

JSC::MacroAssembler::RegisterID
FrameState::ownRegForData(FrameEntry *fe)
{
    JS_ASSERT(!fe->data.isConstant());

    RegisterID reg;
    if (fe->isCopy()) {
        /* For now, just do an extra move. The reg must be mutable. */
        FrameEntry *backing = fe->copyOf();
        if (!backing->data.inRegister()) {
            JS_ASSERT(backing->data.inMemory());
            tempRegForData(backing);
        }

        if (freeRegs.empty()) {
            /* For now... just steal the register that already exists. */
            if (!backing->data.synced())
                syncData(backing, addressOf(backing), masm);
            reg = backing->data.reg();
            backing->data.setMemory();
            regstate[reg].forget();
        } else {
            reg = allocReg();
            masm.move(backing->data.reg(), reg);
        }
        return reg;
    }

    if (fe->isCopied()) {
        FrameEntry *copy = uncopy(fe);
        if (fe->isCopied()) {
            fe->type.invalidate();
            fe->data.invalidate();
            return copyDataIntoReg(copy);
        }
    }
    
    if (fe->data.inRegister()) {
        reg = fe->data.reg();
        /* Remove ownership of this register. */
        JS_ASSERT(regstate[reg].fe() == fe);
        JS_ASSERT(regstate[reg].type() == RematInfo::DATA);
        regstate[reg].forget();
        fe->data.invalidate();
    } else {
        JS_ASSERT(fe->data.inMemory());
        reg = allocReg();
        masm.loadPayload(addressOf(fe), reg);
    }
    return reg;
}

void
FrameState::pushCopyOf(uint32 index)
{
    FrameEntry *backing = entryFor(index);
    FrameEntry *fe = rawPush();
    fe->resetUnsynced();
    if (backing->isConstant()) {
        fe->setConstant(Jsvalify(backing->getValue()));
    } else {
        if (backing->isTypeKnown())
            fe->setType(backing->getKnownType());
        else
            fe->type.invalidate();
        fe->isNumber = backing->isNumber;
        fe->data.invalidate();
        if (backing->isCopy()) {
            backing = backing->copyOf();
            fe->setCopyOf(backing);
        } else {
            fe->setCopyOf(backing);
            backing->setCopied();
        }

        /* Maintain tracker ordering guarantees for copies. */
        JS_ASSERT(backing->isCopied());
        if (fe->trackerIndex() < backing->trackerIndex())
            swapInTracker(fe, backing);
    }
}

FrameEntry *
FrameState::walkTrackerForUncopy(FrameEntry *original)
{
    uint32 firstCopy = InvalidIndex;
    FrameEntry *bestFe = NULL;
    uint32 ncopies = 0;
    for (uint32 i = original->trackerIndex() + 1; i < tracker.nentries; i++) {
        FrameEntry *fe = tracker[i];
        if (fe >= sp)
            continue;
        if (fe->isCopy() && fe->copyOf() == original) {
            if (firstCopy == InvalidIndex) {
                firstCopy = i;
                bestFe = fe;
            } else if (fe < bestFe) {
                bestFe = fe;
            }
            ncopies++;
        }
    }

    if (!ncopies) {
        JS_ASSERT(firstCopy == InvalidIndex);
        JS_ASSERT(!bestFe);
        return NULL;
    }

    JS_ASSERT(firstCopy != InvalidIndex);
    JS_ASSERT(bestFe);
    JS_ASSERT(bestFe > original);

    /* Mark all extra copies as copies of the new backing index. */
    bestFe->setCopyOf(NULL);
    if (ncopies > 1) {
        bestFe->setCopied();
        for (uint32 i = firstCopy; i < tracker.nentries; i++) {
            FrameEntry *other = tracker[i];
            if (other >= sp || other == bestFe)
                continue;

            /* The original must be tracked before copies. */
            JS_ASSERT(other != original);

            if (!other->isCopy() || other->copyOf() != original)
                continue;

            other->setCopyOf(bestFe);

            /*
             * This is safe even though we're mutating during iteration. There
             * are two cases. The first is that both indexes are <= i, and :.
             * will never be observed. The other case is we're placing the
             * other FE such that it will be observed later. Luckily, copyOf()
             * will return != original, so nothing will happen.
             */
            if (other->trackerIndex() < bestFe->trackerIndex())
                swapInTracker(bestFe, other);
        }
    } else {
        bestFe->setNotCopied();
    }

    return bestFe;
}

FrameEntry *
FrameState::walkFrameForUncopy(FrameEntry *original)
{
    FrameEntry *bestFe = NULL;
    uint32 ncopies = 0;

    /* It's only necessary to visit as many FEs are being tracked. */
    uint32 maxvisits = tracker.nentries;

    for (FrameEntry *fe = original + 1; fe < sp && maxvisits; fe++) {
        if (!fe->isTracked())
            continue;

        maxvisits--;

        if (fe->isCopy() && fe->copyOf() == original) {
            if (!bestFe) {
                bestFe = fe;
                bestFe->setCopyOf(NULL);
            } else {
                fe->setCopyOf(bestFe);
                if (fe->trackerIndex() < bestFe->trackerIndex())
                    swapInTracker(bestFe, fe);
            }
            ncopies++;
        }
    }

    if (ncopies)
        bestFe->setCopied();

    return bestFe;
}

FrameEntry *
FrameState::uncopy(FrameEntry *original)
{
    JS_ASSERT(original->isCopied());

    /*
     * Copies have three critical invariants:
     *  1) The backing store precedes all copies in the tracker.
     *  2) The backing store precedes all copies in the FrameState.
     *  3) The backing store of a copy cannot be popped from the stack
     *     while the copy is still live.
     *
     * Maintaining this invariant iteratively is kind of hard, so we choose
     * the "lowest" copy in the frame up-front.
     *
     * For example, if the stack is:
     *    [A, B, C, D]
     * And the tracker has:
     *    [A, D, C, B]
     *
     * If B, C, and D are copies of A - we will walk the tracker to the end
     * and select B, not D (see bug 583684).
     *
     * Note: |tracker.nentries <= (nslots + nargs)|. However, this walk is
     * sub-optimal if |tracker.nentries - original->trackerIndex() > sp - original|.
     * With large scripts this may be a problem worth investigating. Note that
     * the tracker is walked twice, so we multiply by 2 for pessimism.
     */
    FrameEntry *fe;
    if ((tracker.nentries - original->trackerIndex()) * 2 > uint32(sp - original))
        fe = walkFrameForUncopy(original);
    else
        fe = walkTrackerForUncopy(original);
    if (!fe) {
        original->setNotCopied();
        return NULL;
    }

    /*
     * Switch the new backing store to the old backing store. During
     * this process we also necessarily make sure the copy can be
     * synced.
     */
    if (!original->isTypeKnown()) {
        /*
         * If the copy is unsynced, and the original is in memory,
         * give the original a register. We do this below too; it's
         * okay if it's spilled.
         */
        if (original->type.inMemory() && !fe->type.synced())
            tempRegForType(original);
        fe->type.inherit(original->type);
        if (fe->type.inRegister())
            regstate[fe->type.reg()].reassociate(fe);
    } else {
        JS_ASSERT(fe->isTypeKnown());
        JS_ASSERT(fe->getKnownType() == original->getKnownType());
    }
    if (original->data.inMemory() && !fe->data.synced())
        tempRegForData(original);
    fe->data.inherit(original->data);
    if (fe->data.inRegister())
        regstate[fe->data.reg()].reassociate(fe);

    return fe;
}

void
FrameState::storeLocal(uint32 n, bool popGuaranteed, bool typeChange)
{
    FrameEntry *local = getLocal(n);

    storeTop(local, popGuaranteed, typeChange);

    bool closed = eval || escaping[n];
    if (closed || inTryBlock) {
        /* Ensure that the local variable remains synced. */
        if (local->isCopy()) {
            FrameEntry *backing = local->copyOf();
            if (!local->data.synced()) {
                if (backing->data.inMemory())
                    tempRegForData(backing);
                syncData(backing, addressOf(local), masm);
            }
            if (!local->type.synced()) {
                if (backing->type.inMemory())
                    tempRegForType(backing);
                syncType(backing, addressOf(local), masm);
            }
        } else if (local->isConstant()) {
            if (!local->data.synced())
                syncData(local, addressOf(local), masm);
        } else {
            if (!local->data.synced()) {
                syncData(local, addressOf(local), masm);
                local->data.sync();
            }
            if (!local->type.synced()) {
                syncType(local, addressOf(local), masm);
                local->type.sync();
            }
            if (closed)
                forgetEntry(local);
        }

        if (closed)
            local->resetSynced();
    }
}

void
FrameState::forgetEntry(FrameEntry *fe)
{
    if (fe->isCopied()) {
        uncopy(fe);
        if (!fe->isCopied())
            forgetAllRegs(fe);
    } else {
        forgetAllRegs(fe);
    }
}

void
FrameState::storeTop(FrameEntry *target, bool popGuaranteed, bool typeChange)
{
    bool wasSynced = target->type.synced();
    /* Detect something like (x = x) which is a no-op. */
    FrameEntry *top = peek(-1);
    if (top->isCopy() && top->copyOf() == target) {
        JS_ASSERT(target->isCopied());
        return;
    }

    /* Completely invalidate the local variable. */
    forgetEntry(target);
    target->resetUnsynced();

    /* Constants are easy to propagate. */
    if (top->isConstant()) {
        target->setCopyOf(NULL);
        target->setNotCopied();
        target->setConstant(Jsvalify(top->getValue()));
        return;
    }

    /*
     * When dealing with copies, there are three important invariants:
     *
     * 1) The backing store precedes all copies in the tracker.
     * 2) The backing store precedes all copies in the FrameState.
     * 2) The backing store of a local is never a stack slot, UNLESS the local
     *    variable itself is a stack slot (blocks) that precedes the stack
     *    slot.
     *
     * If the top is a copy, and the second condition holds true, the local
     * can be rewritten as a copy of the original backing slot. If the first
     * condition does not hold, force it to hold by swapping in-place.
     */
    FrameEntry *backing = top;
    bool copied = false;
    if (top->isCopy()) {
        backing = top->copyOf();
        JS_ASSERT(backing->trackerIndex() < top->trackerIndex());

        if (backing < target) {
            /* local.idx < backing.idx means local cannot be a copy yet */
            if (target->trackerIndex() < backing->trackerIndex())
                swapInTracker(backing, target);
            target->setNotCopied();
            target->setCopyOf(backing);
            if (backing->isTypeKnown())
                target->setType(backing->getKnownType());
            else
                target->type.invalidate();
            target->data.invalidate();
            target->isNumber = backing->isNumber;
            return;
        }

        /*
         * If control flow lands here, then there was a bytecode sequence like
         *
         *  ENTERBLOCK 2
         *  GETLOCAL 1
         *  SETLOCAL 0
         *
         * The problem is slot N can't be backed by M if M could be popped
         * before N. We want a guarantee that when we pop M, even if it was
         * copied, it has no outstanding copies.
         * 
         * Because of |let| expressions, it's kind of hard to really know
         * whether a region on the stack will be popped all at once. Bleh!
         *
         * This should be rare except in browser code (and maybe even then),
         * but even so there's a quick workaround. We take all copies of the
         * backing fe, and redirect them to be copies of the destination.
         */
        for (uint32 i = backing->trackerIndex() + 1; i < tracker.nentries; i++) {
            FrameEntry *fe = tracker[i];
            if (fe >= sp)
                continue;
            if (fe->isCopy() && fe->copyOf() == backing) {
                fe->setCopyOf(target);
                copied = true;
            }
        }
    }
    backing->setNotCopied();
    
    /*
     * This is valid from the top->isCopy() path because we're guaranteed a
     * consistent ordering - all copies of |backing| are tracked after 
     * |backing|. Transitively, only one swap is needed.
     */
    if (backing->trackerIndex() < target->trackerIndex())
        swapInTracker(backing, target);

    /*
     * Move the backing store down - we spill registers here, but we could be
     * smarter and re-use the type reg.
     */
    RegisterID reg = tempRegForData(backing);
    target->data.setRegister(reg);
    regstate[reg].reassociate(target);

    if (typeChange) {
        if (backing->isTypeKnown()) {
            target->setType(backing->getKnownType());
        } else {
            RegisterID reg = tempRegForType(backing);
            target->type.setRegister(reg);
            regstate[reg].reassociate(target);
        }
    } else {
        if (!wasSynced)
            masm.storeTypeTag(ImmType(backing->getKnownType()), addressOf(target));
        target->type.setMemory();
    }

    if (!backing->isTypeKnown())
        backing->type.invalidate();
    backing->data.invalidate();
    backing->setCopyOf(target);
    backing->isNumber = target->isNumber;

    JS_ASSERT(top->copyOf() == target);

    /*
     * Right now, |backing| is a copy of |target| (note the reversal), but
     * |target| is not marked as copied. This is an optimization so uncopy()
     * may avoid frame traversal.
     *
     * There are two cases where we must set the copy bit, however:
     *  - The fixup phase redirected more copies to |target|.
     *  - An immediate pop is not guaranteed.
     */
    if (copied || !popGuaranteed)
        target->setCopied();
}

void
FrameState::shimmy(uint32 n)
{
    JS_ASSERT(sp - n >= spBase);
    int32 depth = 0 - int32(n);
    storeTop(&sp[depth - 1], true);
    popn(n);
}

void
FrameState::shift(int32 n)
{
    JS_ASSERT(n < 0);
    JS_ASSERT(sp + n - 1 >= spBase);
    storeTop(&sp[n - 1], true);
    pop();
}

void
FrameState::pinEntry(FrameEntry *fe, ValueRemat &vr)
{
    vr.isDataSynced = fe->data.synced();
    vr.isTypeSynced = fe->type.synced();
    if (fe->isConstant()) {
        vr.isConstant = true;
        vr.u.v = Jsvalify(fe->getValue());
    } else {
        vr.isConstant = false;
        vr.u.s.isTypeKnown = fe->isTypeKnown();
        if (vr.u.s.isTypeKnown) {
            vr.u.s.type.knownType = fe->getKnownType();
        } else {
            vr.u.s.type.reg = tempRegForType(fe);
            pinReg(vr.u.s.type.reg);
        }
        vr.u.s.data = tempRegForData(fe);
        pinReg(vr.u.s.data);
    }
}

void
FrameState::unpinEntry(const ValueRemat &vr)
{
    if (!vr.isConstant) {
        if (!vr.u.s.isTypeKnown)
            unpinReg(vr.u.s.type.reg);
        unpinReg(vr.u.s.data);
    }
}

void
FrameState::syncEntry(Assembler &masm, FrameEntry *fe, const ValueRemat &vr)
{
#if defined JS_PUNBOX64
    if (!vr.isDataSynced || !vr.isTypeSynced)
        masm.storeValue(vr, addressOf(fe));
#elif defined JS_NUNBOX32
    if (vr.isConstant) {
        if (!vr.isDataSynced || !vr.isTypeSynced)
            masm.storeValue(Valueify(vr.u.v), addressOf(fe));
    } else {
        if (!vr.isDataSynced)
            masm.storePayload(vr.u.s.data, addressOf(fe));
        if (!vr.isTypeSynced) {
            if (vr.u.s.isTypeKnown)
                masm.storeTypeTag(ImmType(vr.u.s.type.knownType), addressOf(fe));
            else
                masm.storeTypeTag(vr.u.s.type.reg, addressOf(fe));
        }
    }
#endif
}

static inline bool
AllocHelper(RematInfo &info, MaybeRegisterID &maybe)
{
    if (info.inRegister()) {
        maybe = info.reg();
        return true;
    }
    return false;
}

void
FrameState::allocForSameBinary(FrameEntry *fe, JSOp op, BinaryAlloc &alloc)
{
    if (!fe->isTypeKnown()) {
        alloc.lhsType = tempRegForType(fe);
        pinReg(alloc.lhsType.reg());
    }

    alloc.lhsData = tempRegForData(fe);

    if (!freeRegs.empty()) {
        alloc.result = allocReg();
        masm.move(alloc.lhsData.reg(), alloc.result);
        alloc.lhsNeedsRemat = false;
    } else {
        alloc.result = alloc.lhsData.reg();
        takeReg(alloc.result);
        alloc.lhsNeedsRemat = true;
    }

    if (alloc.lhsType.isSet())
        unpinReg(alloc.lhsType.reg());
}

void
FrameState::ensureFullRegs(FrameEntry *fe)
{
    FrameEntry *backing = fe;
    if (fe->isCopy())
        backing = fe->copyOf();

    if (!fe->type.inMemory()) {
        if (fe->data.inRegister())
            return;
        if (fe->type.inRegister())
            pinReg(fe->type.reg());
        if (fe->data.inMemory())
            tempRegForData(fe);
        if (fe->type.inRegister())
            unpinReg(fe->type.reg());
    } else if (!fe->data.inMemory()) {
        if (fe->type.inRegister())
            return;
        if (fe->data.inRegister())
            pinReg(fe->data.reg());
        if (fe->type.inMemory())
            tempRegForType(fe);
        if (fe->data.inRegister())
            unpinReg(fe->data.reg());
    }
}

void
FrameState::allocForBinary(FrameEntry *lhs, FrameEntry *rhs, JSOp op, BinaryAlloc &alloc,
                           bool needsResult)
{
    FrameEntry *backingLeft = lhs;
    FrameEntry *backingRight = rhs;

    if (backingLeft->isCopy())
        backingLeft = backingLeft->copyOf();
    if (backingRight->isCopy())
        backingRight = backingRight->copyOf();

    /*
     * For each remat piece of both FEs, if a register is assigned, get it now
     * and pin it. This is safe - constants and known types will be avoided.
     */
    if (AllocHelper(backingLeft->type, alloc.lhsType))
        pinReg(alloc.lhsType.reg());
    if (AllocHelper(backingLeft->data, alloc.lhsData))
        pinReg(alloc.lhsData.reg());
    if (AllocHelper(backingRight->type, alloc.rhsType))
        pinReg(alloc.rhsType.reg());
    if (AllocHelper(backingRight->data, alloc.rhsData))
        pinReg(alloc.rhsData.reg());

    /* For each type without a register, give it a register if needed. */
    if (!alloc.lhsType.isSet() && backingLeft->type.inMemory()) {
        alloc.lhsType = tempRegForType(lhs);
        pinReg(alloc.lhsType.reg());
    }
    if (!alloc.rhsType.isSet() && backingRight->type.inMemory()) {
        alloc.rhsType = tempRegForType(rhs);
        pinReg(alloc.rhsType.reg());
    }

    bool commu;
    switch (op) {
      case JSOP_EQ:
      case JSOP_GT:
      case JSOP_GE:
      case JSOP_LT:
      case JSOP_LE:
        /* fall through */
      case JSOP_ADD:
      case JSOP_MUL:
      case JSOP_SUB:
        commu = true;
        break;

      case JSOP_DIV:
        commu = false;
        break;

      default:
        JS_NOT_REACHED("unknown op");
        return;
    }

    /*
     * Data is a little more complicated. If the op is MUL, not all CPUs
     * have multiplication on immediates, so a register is needed. Also,
     * if the op is not commutative, the LHS _must_ be in a register.
     */
    JS_ASSERT_IF(lhs->isConstant(), !rhs->isConstant());
    JS_ASSERT_IF(rhs->isConstant(), !lhs->isConstant());

    if (!alloc.lhsData.isSet()) {
        if (backingLeft->data.inMemory()) {
            alloc.lhsData = tempRegForData(lhs);
            pinReg(alloc.lhsData.reg());
        } else if (op == JSOP_MUL || !commu) {
            JS_ASSERT(lhs->isConstant());
            alloc.lhsData = allocReg();
            alloc.extraFree = alloc.lhsData;
            masm.move(Imm32(lhs->getValue().toInt32()), alloc.lhsData.reg());
        }
    }
    if (!alloc.rhsData.isSet()) {
        if (backingRight->data.inMemory()) {
            alloc.rhsData = tempRegForData(rhs);
            pinReg(alloc.rhsData.reg());
        } else if (op == JSOP_MUL) {
            JS_ASSERT(rhs->isConstant());
            alloc.rhsData = allocReg();
            alloc.extraFree = alloc.rhsData;
            masm.move(Imm32(rhs->getValue().toInt32()), alloc.rhsData.reg());
        }
    }

    alloc.lhsNeedsRemat = false;
    alloc.rhsNeedsRemat = false;

    if (!needsResult)
        goto skip;

    /*
     * Now a result register is needed. It must contain a mutable copy of the
     * LHS. For commutative operations, we can opt to use the RHS instead. At
     * this point, if for some reason either must be in a register, that has
     * already been guaranteed at this point.
     */
    if (!freeRegs.empty()) {
        /* Free reg - just grab it. */
        alloc.result = allocReg();
        if (!alloc.lhsData.isSet()) {
            JS_ASSERT(alloc.rhsData.isSet());
            JS_ASSERT(commu);
            masm.move(alloc.rhsData.reg(), alloc.result);
            alloc.resultHasRhs = true;
        } else {
            masm.move(alloc.lhsData.reg(), alloc.result);
            alloc.resultHasRhs = false;
        }
    } else {
        /*
         * No free regs. Find a good candidate to re-use. Best candidates don't
         * require syncs on the inline path.
         */
        bool leftInReg = backingLeft->data.inRegister();
        bool rightInReg = backingRight->data.inRegister();
        bool leftSynced = backingLeft->data.synced();
        bool rightSynced = backingRight->data.synced();
        if (!commu || (leftInReg && (leftSynced || (!rightInReg || !rightSynced)))) {
            JS_ASSERT(backingLeft->data.inRegister() || !commu);
            JS_ASSERT_IF(backingLeft->data.inRegister(),
                         backingLeft->data.reg() == alloc.lhsData.reg());
            if (backingLeft->data.inRegister()) {
                alloc.result = backingLeft->data.reg();
                unpinReg(alloc.result);
                takeReg(alloc.result);
                alloc.lhsNeedsRemat = true;
            } else {
                /* For now, just spill... */
                alloc.result = allocReg();
                masm.move(alloc.lhsData.reg(), alloc.result);
            }
            alloc.resultHasRhs = false;
        } else {
            JS_ASSERT(commu);
            JS_ASSERT(!leftInReg || (rightInReg && rightSynced));
            alloc.result = backingRight->data.reg();
            unpinReg(alloc.result);
            takeReg(alloc.result);
            alloc.resultHasRhs = true;
            alloc.rhsNeedsRemat = true;
        }
    }

  skip:
    /* Unpin everything that was pinned. */
    if (backingLeft->type.inRegister())
        unpinReg(backingLeft->type.reg());
    if (backingRight->type.inRegister())
        unpinReg(backingRight->type.reg());
    if (backingLeft->data.inRegister())
        unpinReg(backingLeft->data.reg());
    if (backingRight->data.inRegister())
        unpinReg(backingRight->data.reg());
}


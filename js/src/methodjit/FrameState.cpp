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

FrameState::FrameState(JSContext *cx, JSScript *script, Assembler &masm)
  : cx(cx), script(script), masm(masm), entries(NULL)
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
        sp = spBase = locals = args = base = NULL;
        return true;
    }

    uint8 *cursor = (uint8 *)cx->malloc(sizeof(FrameEntry) * nslots +       // entries[]
                                        sizeof(FrameEntry *) * nslots +     // base[]
                                        sizeof(uint32 *) * nslots           // tracker.entries[]
                                        );
    if (!cursor)
        return false;

    entries = (FrameEntry *)cursor;
    cursor += sizeof(FrameEntry) * nslots;

    base = (FrameEntry **)cursor;
    args = base;
    locals = base + nargs;
    spBase = locals + script->nfixed;
    sp = spBase;
    memset(base, 0, sizeof(FrameEntry *) * nslots);
    cursor += sizeof(FrameEntry *) * nslots;

    tracker.entries = (uint32 *)cursor;

    return true;
}

void
FrameState::takeReg(RegisterID reg)
{
    if (freeRegs.hasReg(reg)) {
        freeRegs.takeReg(reg);
        return;
    }

    evictReg(reg);
    regstate[reg].fe = NULL;
}

void
FrameState::evictReg(RegisterID reg)
{
    FrameEntry *fe = regstate[reg].fe;

    if (regstate[reg].type == RematInfo::TYPE) {
        syncType(fe, addressOf(fe), masm);
        fe->type.sync();
        fe->type.setMemory();
    } else {
        syncData(fe, addressOf(fe), masm);
        fe->data.sync();
        fe->data.setMemory();
    }
}

JSC::MacroAssembler::RegisterID
FrameState::evictSomething(uint32 mask)
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
        FrameEntry *fe = regstate[i].fe;
        if (!fe)
            continue;

        /* Try to find a candidate... that doesn't need spilling. */
#ifdef DEBUG
        fallbackSet = true;
#endif
        fallback = reg;

        if (regstate[i].type == RematInfo::TYPE && fe->type.synced()) {
            fe->type.setMemory();
            return fallback;
        }
        if (regstate[i].type == RematInfo::DATA && fe->data.synced()) {
            fe->data.setMemory();
            return fallback;
        }
    }

    JS_ASSERT(fallbackSet);

    evictReg(fallback);
    return fallback;
}

void
FrameState::forgetEverything()
{
    sync(masm);

    for (uint32 i = 0; i < tracker.nentries; i++)
        base[tracker[i]] = NULL;

    tracker.reset();
    freeRegs.reset();
}

void
FrameState::storeTo(FrameEntry *fe, Address address, bool popped)
{
    if (fe->isConstant()) {
        masm.storeValue(fe->getValue(), address);
        return;
    }

    if (fe->isCopy())
        fe = entryFor(fe->copyOf());

    /* Cannot clobber the address's register. */
    JS_ASSERT(!freeRegs.hasReg(address.base));

    if (fe->data.inRegister()) {
        masm.storeData32(fe->data.reg(), address);
    } else {
        JS_ASSERT(fe->data.inMemory());
        RegisterID reg = popped ? alloc() : alloc(fe, RematInfo::DATA, true);
        masm.loadData32(addressOf(fe), reg);
        masm.storeData32(reg, address);
        if (popped)
            freeReg(reg);
        else
            fe->data.setRegister(reg);
    }

    if (fe->isTypeKnown()) {
        masm.storeTypeTag(ImmTag(fe->getTypeTag()), address);
    } else if (fe->type.inRegister()) {
        masm.storeTypeTag(fe->type.reg(), address);
    } else {
        JS_ASSERT(fe->type.inMemory());
        RegisterID reg = popped ? alloc() : alloc(fe, RematInfo::TYPE, true);
        masm.loadTypeTag(addressOf(fe), reg);
        masm.storeTypeTag(reg, address);
        if (popped)
            freeReg(reg);
        else
            fe->type.setRegister(reg);
    }
}

#ifdef DEBUG
void
FrameState::assertValidRegisterState() const
{
    Registers checkedFreeRegs;

    uint32 tos = uint32(sp - base);

    for (uint32 i = 0; i < tracker.nentries; i++) {
        uint32 index = tracker[i];

        if (index >= tos)
            continue;

        FrameEntry *fe = entryFor(index);
        JS_ASSERT_IF(fe->isCopy(), !fe->type.inRegister() && !fe->data.inRegister());

        if (fe->isCopy())
            continue;
        if (fe->type.inRegister()) {
            checkedFreeRegs.takeReg(fe->type.reg());
            JS_ASSERT(regstate[fe->type.reg()].fe == fe);
        }
        if (fe->data.inRegister()) {
            checkedFreeRegs.takeReg(fe->data.reg());
            JS_ASSERT(regstate[fe->data.reg()].fe == fe);
        }
    }

    JS_ASSERT(checkedFreeRegs == freeRegs);
}
#endif

void
FrameState::sync(Assembler &masm) const
{
    Registers avail(freeRegs);

    for (uint32 i = 0; i < tracker.nentries; i++) {
        uint32 index = tracker[i];

        if (index >= tos())
            continue;

        FrameEntry *fe = entryFor(index);
        Address address = addressOf(fe);

        if (!fe->isCopy()) {
            /* Keep track of registers that can be clobbered. */
            if (fe->data.inRegister())
                avail.putReg(fe->data.reg());
            if (fe->type.inRegister())
                avail.putReg(fe->type.reg());

            /* Sync. */
            if (!fe->data.synced()) {
                syncData(fe, address, masm);
                if (fe->isConstant())
                    continue;
            }
            if (!fe->type.synced())
                syncType(fe, addressOf(fe), masm);
        } else {
            FrameEntry *backing = entryFor(fe->copyOf());
            JS_ASSERT(backing != fe);
            JS_ASSERT(!backing->isConstant() && !fe->isConstant());

            RegisterID reg;
            bool allocd = false;
            if (backing->type.inMemory() || backing->data.inMemory()) {
                /* :TODO: this is not a valid assumption. */
                JS_ASSERT(!avail.empty());

                /*
                 * It's not valid to alter the state of an fe here, not yet. So
                 * we don't bother optimizing copies of the same local variable
                 * if each one needs a load from memory.
                 */
                reg = avail.takeAnyReg();
                allocd = true;
            }

            if (!fe->type.synced()) {
                if (backing->isTypeKnown()) {
                    JS_ASSERT(fe->getTypeTag() == backing->getTypeTag());
                    masm.storeTypeTag(ImmTag(backing->getTypeTag()), address);
                } else {
                    RegisterID r;
                    if (backing->type.inRegister()) {
                        r = backing->type.reg();
                    } else {
                        masm.loadTypeTag(addressOf(backing), reg);
                        r = reg;
                    }
                    masm.storeTypeTag(r, address);
                }
            }

            if (!fe->data.synced()) {
                RegisterID r;
                if (backing->data.inRegister()) {
                    r = backing->data.reg();
                } else {
                    masm.loadData32(addressOf(backing), reg);
                    r = reg;
                }
                masm.storeData32(r, address);
            }

            if (allocd)
                avail.putReg(reg);
        }
    }
}

void
FrameState::syncAndKill(uint32 mask)
{
    Registers kill(mask);

    /* Backwards, so we can allocate registers to backing slots better. */
    for (uint32 i = tracker.nentries - 1; i < tracker.nentries; i--) {
        uint32 index = tracker[i];

        if (index >= tos())
            continue;

        FrameEntry *fe = entryFor(index);
        Address address = addressOf(fe);
        FrameEntry *backing = fe;
        if (fe->isCopy())
            backing = entryFor(fe->copyOf());

        if (!fe->data.synced()) {
            if (backing != fe && backing->data.inMemory())
                tempRegForData(backing);
            syncData(backing, address, masm);
            fe->data.sync();
            if (fe->isConstant() && !fe->type.synced())
                fe->type.sync();
        }
        if (fe->data.inRegister() && kill.hasReg(fe->data.reg())) {
            JS_ASSERT(backing == fe);
            JS_ASSERT(fe->data.synced());
            forgetReg(fe->data.reg());
            fe->data.setMemory();
        }
        if (!fe->type.synced()) {
            if (backing != fe && backing->type.inMemory())
                tempRegForType(backing);
            syncType(backing, address, masm);
            fe->type.sync();
        }
        if (fe->type.inRegister() && kill.hasReg(fe->type.reg())) {
            JS_ASSERT(backing == fe);
            JS_ASSERT(fe->type.synced());
            forgetReg(fe->type.reg());
            fe->type.setMemory();
        }
    }
}

void
FrameState::merge(Assembler &masm, uint32 iVD) const
{
    for (uint32 i = 0; i < tracker.nentries; i++) {
        uint32 index = tracker[i];

        if (index >= tos())
            continue;

        FrameEntry *fe = entryFor(index);
        JS_ASSERT(!fe->isCopy());

        if (fe->data.inRegister())
            masm.loadData32(addressOf(fe), fe->data.reg());
        if (fe->type.inRegister())
            masm.loadTypeTag(addressOf(fe), fe->type.reg());
    }
}

JSC::MacroAssembler::RegisterID
FrameState::copyData(FrameEntry *fe)
{
    JS_ASSERT(!fe->data.isConstant());
    JS_ASSERT(!fe->isCopy());

    if (fe->data.inRegister()) {
        RegisterID reg = fe->data.reg();
        if (freeRegs.empty()) {
            if (!fe->data.synced())
                syncData(fe, addressOf(fe), masm);
            fe->data.setMemory();
            regstate[reg].fe = NULL;
        } else {
            RegisterID newReg = alloc();
            masm.move(reg, newReg);
            reg = newReg;
        }
        return reg;
    }

    RegisterID reg = alloc();

    if (!freeRegs.empty())
        masm.move(tempRegForData(fe), reg);
    else
        masm.loadData32(addressOf(fe),reg);

    return reg;
}

JSC::MacroAssembler::RegisterID
FrameState::ownRegForData(FrameEntry *fe)
{
    JS_ASSERT(!fe->data.isConstant());

    RegisterID reg;
    if (fe->isCopy()) {
        /* For now, just do an extra move. The reg must be mutable. */
        FrameEntry *backing = entryFor(fe->copyOf());
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
            moveOwnership(reg, NULL);
        } else {
            reg = alloc();
            masm.move(backing->data.reg(), reg);
        }
    } else if (fe->data.inRegister()) {
        reg = fe->data.reg();
        /* Remove ownership of this register. */
        JS_ASSERT(regstate[reg].fe == fe);
        JS_ASSERT(regstate[reg].type == RematInfo::DATA);
        regstate[reg].fe = NULL;
        fe->data.invalidate();
    } else {
        JS_ASSERT(fe->data.inMemory());
        reg = alloc();
        masm.loadData32(addressOf(fe), reg);
    }
    return reg;
}

void
FrameState::pushLocal(uint32 n)
{
    FrameEntry *localFe = getLocal(n);
    FrameEntry *fe = rawPush();
    fe->resetUnsynced();
    if (localFe->isConstant()) {
        fe->setConstant(Jsvalify(localFe->getValue()));
    } else {
        if (localFe->isTypeKnown())
            fe->setTypeTag(localFe->getTypeTag());
        else
            fe->type.invalidate();
        fe->data.invalidate();
        fe->setCopyOf(localIndex(n));
        localFe->setCopied();
    }
}

void
FrameState::storeLocal(FrameEntry *fe, uint32 n)
{
    FrameEntry *localFe = getLocal(n);

    if (fe->isCopy()) {
        FrameEntry *backing = entryFor(fe->copyOf());

        /* x == x is a no-op. */
        if (fe == backing)
            return;

        fe = backing;
    }

    if (localFe->isCopied()) {
        /* Find any entry being backed by the local fe, and rewrite it. */
        for (uint32 i = 0; i < tracker.nentries; i++) {
            uint32 index = tracker[i];
            FrameEntry *other = entryFor(index);
            if (other->isCopy() && other->copyOf() == localIndex(n)) {
                /* This is the first copy -- it's now the new backing index. */
                JS_NOT_REACHED("hello, plz fix this now");
            }
        }
    }

    localFe->resetUnsynced();

    if (fe->isConstant()) {
        /* Propagate constants. */
        localFe->setConstant(Jsvalify(fe->getValue()));
    } else {
        RegisterID reg;

        /* First, copy everything to the local fe. */
        if (fe->isTypeKnown()) {
            localFe->setTypeTag(fe->getTypeTag());
        } else {
            if (!fe->type.inRegister()) {
                reg = tempRegForType(fe);
                JS_ASSERT(reg == fe->type.reg());
            } else {
                reg = fe->type.reg();
            }
            localFe->type.setRegister(reg);
            moveOwnership(reg, localFe);
            fe->type.invalidate();
            JS_ASSERT(regstate[reg].type == RematInfo::TYPE);
        }
        if (!fe->data.inRegister()) {
            reg = tempRegForData(fe);
            JS_ASSERT(reg == fe->data.reg());
        } else {
            reg = fe->data.reg();
        }
        localFe->data.setRegister(reg);
        fe->data.invalidate();
        moveOwnership(reg, localFe);
        JS_ASSERT(regstate[reg].type == RematInfo::DATA);

        localFe->setCopied();
        fe->setCopyOf(localIndex(n));
    }
}

void
FrameState::popAfterSet()
{
    FrameEntry *top = peek(-1);
    FrameEntry *down = peek(-2);

    if (down->type.inRegister())
        forgetReg(down->type.reg());
    if (down->data.inRegister())
        forgetReg(down->data.reg());

    if (top->isConstant()) {
        down->setConstant(Jsvalify(top->getValue()));
    } else {
        down->type.unsync();
        if (top->isTypeKnown()) {
            down->setTypeTag(top->getTypeTag());
        } else {
            down->type.inherit(top->type);
            if (top->type.inRegister())
                moveOwnership(top->type.reg(), down);
        }
        down->data.unsync();
        down->data.inherit(top->data);
        if (top->data.inRegister())
            moveOwnership(top->data.reg(), down);
    }

    /* It is now okay to just decrement sp. */
    sp--;
}


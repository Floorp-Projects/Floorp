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

using namespace js;
using namespace js::mjit;

FrameState::FrameState(JSContext *cx, JSScript *script, Assembler &masm)
  : cx(cx), script(script), masm(masm)
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
    if (!nslots)
        return true;

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

FrameEntry *
FrameState::addToTracker(uint32 index)
{
    JS_ASSERT(!base[index]);
    tracker.add(index);
    base[index] = &entries[index];
    return base[index];
}

FrameEntry *
FrameState::peek(int32 depth)
{
    JS_ASSERT(depth < 0);
    JS_ASSERT(sp + depth >= spBase);
    FrameEntry *fe = sp[depth];
    if (!fe) {
        fe = addToTracker(indexOf(depth));
        fe->resetSynced();
    }
    return fe;
}

void
FrameState::popn(uint32 n)
{
    for (uint32 i = 0; i < n; i++)
        pop();
}

JSC::MacroAssembler::RegisterID
FrameState::allocReg()
{
    return alloc();
}

JSC::MacroAssembler::RegisterID
FrameState::alloc()
{
    if (freeRegs.empty())
        evictSomething();
    RegisterID reg = freeRegs.takeAnyReg();
    regstate[reg].fe = NULL;
    return reg;
}

JSC::MacroAssembler::RegisterID
FrameState::alloc(FrameEntry *fe, RematInfo::RematType type, bool weak)
{
    if (freeRegs.empty())
        evictSomething();
    RegisterID reg = freeRegs.takeAnyReg();
    regstate[reg] = RegisterState(fe, type, weak);
    return reg;
}

void
FrameState::evictSomething()
{
    JS_NOT_REACHED("NYI");
}

void
FrameState::pop()
{
    JS_ASSERT(sp > spBase);

    FrameEntry *fe = *--sp;
    if (!fe)
        return;

    if (fe->type.inRegister())
        forgetReg(fe->type.reg());
    if (fe->data.inRegister())
        forgetReg(fe->data.reg());
}

void
FrameState::freeReg(RegisterID reg)
{
    JS_ASSERT(!regstate[reg].fe);
    forgetReg(reg);
}

void
FrameState::forgetReg(RegisterID reg)
{
    freeRegs.putReg(reg);
}

void
FrameState::forgetEverything(uint32 newStackDepth)
{
    forgetEverything();
    sp = spBase + newStackDepth;
}

void
FrameState::forgetEverything()
{
    for (uint32 i = 0; i < tracker.nentries; i++) {
        uint32 index = tracker[i];
        base[index] = NULL;

        if (index >= tos())
            continue;

        FrameEntry *fe = &entries[index];
        if (!fe->data.synced())
            syncData(fe, masm);
        if (!fe->type.synced())
            syncType(fe, masm);
    }

    tracker.reset();
    freeRegs.reset();
}

FrameEntry *
FrameState::rawPush()
{
    sp++;

    if (FrameEntry *fe = sp[-1])
        return fe;

    return addToTracker(&sp[-1] - base);
}

void
FrameState::push(const Value &v)
{
    FrameEntry *fe = rawPush();
    fe->setConstant(Jsvalify(v));
}

void
FrameState::pushSynced()
{
    sp++;

    if (FrameEntry *fe = sp[-1])
        fe->resetSynced();
}

void
FrameState::pushSyncedType(uint32 tag)
{
    FrameEntry *fe = rawPush();

    fe->type.unsync();
    fe->setTypeTag(tag);
    fe->data.setMemory();
}

void
FrameState::push(Address address)
{
    FrameEntry *fe = rawPush();

    /* :XXX: X64 */
    fe->resetUnsynced();

    RegisterID reg = alloc(fe, RematInfo::DATA, true);
    masm.loadData32(addressOf(fe), reg);
    fe->data.setRegister(reg);
    
    reg = alloc(fe, RematInfo::TYPE, true);
    masm.loadTypeTag(addressOf(fe), reg);
    fe->type.setRegister(reg);
}

void
FrameState::pushTypedPayload(uint32 tag, RegisterID payload)
{
    JS_ASSERT(!freeRegs.hasReg(payload));
    JS_ASSERT(!regstate[payload].fe);

    FrameEntry *fe = rawPush();

    fe->resetUnsynced();
    fe->setTypeTag(tag);
    fe->data.setRegister(payload);
    regstate[payload] = RegisterState(fe, RematInfo::DATA, true);
}

void
FrameState::storeTo(FrameEntry *fe, Address address, bool popped)
{
    if (fe->isConstant()) {
        masm.storeValue(fe->getValue(), address);
        return;
    }

    if (fe->data.inRegister()) {
        masm.storeData32(fe->data.reg(), addressOf(fe));
    } else {
        RegisterID reg = popped ? alloc() : alloc(fe, RematInfo::DATA, true);
        masm.loadData32(addressOf(fe), reg);
        masm.storeData32(reg, addressOf(fe));
        if (popped)
            freeReg(reg);
        else
            fe->data.setRegister(reg);
    }

    if (fe->isTypeKnown()) {
        masm.storeTypeTag(Imm32(fe->getTypeTag()), addressOf(fe));
    } else if (fe->type.inRegister()) {
        masm.storeTypeTag(fe->type.reg(), addressOf(fe));
    } else {
        RegisterID reg = popped ? alloc() : alloc(fe, RematInfo::TYPE, true);
        masm.loadTypeTag(addressOf(fe), reg);
        masm.storeTypeTag(reg, addressOf(fe));
        if (popped)
            freeReg(reg);
        else
            fe->type.setRegister(reg);
    }
}

JSC::MacroAssembler::RegisterID
FrameState::tempRegForType(FrameEntry *fe)
{
    JS_ASSERT(!fe->type.isConstant());

    if (fe->type.inRegister())
        return fe->type.reg();

    /* :XXX: X64 */

    RegisterID reg = alloc(fe, RematInfo::TYPE, true);
    masm.loadTypeTag(addressOf(fe), reg);
    return reg;
}

void
FrameState::assertValidRegisterState() const
{
#ifdef DEBUG
    Registers checkedFreeRegs;

    uint32 tos = uint32(sp - base);

    for (uint32 i = 0; i < tracker.nentries; i++) {
        uint32 index = tracker[i];

        if (index >= tos)
            continue;

        JS_ASSERT(base[index]);
        FrameEntry *fe = &entries[index];
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
#endif
}

void
FrameState::syncType(const FrameEntry *fe, Assembler &masm) const
{
    JS_ASSERT(!fe->type.synced());
    JS_ASSERT(fe->type.inRegister() || fe->type.isConstant());

    if (fe->type.isConstant()) {
        JS_ASSERT(fe->isTypeKnown());
        masm.storeTypeTag(Imm32(fe->getTypeTag()), addressOf(fe));
    } else {
        masm.storeTypeTag(fe->type.reg(), addressOf(fe));
    }
}

void
FrameState::syncData(const FrameEntry *fe, Assembler &masm) const
{
    JS_ASSERT(!fe->data.synced());
    JS_ASSERT(fe->data.inRegister() || fe->data.isConstant());

    if (fe->data.isConstant()) {
       if (!fe->type.synced())
           masm.storeValue(fe->getValue(), addressOf(fe));
       else
           masm.storeData32(Imm32(fe->getPayload32()), addressOf(fe));
    } else {
        masm.storeData32(fe->data.reg(), addressOf(fe));
    }
}

void
FrameState::syncAndKill(uint32 mask)
{
    Registers kill(mask);

    for (uint32 i = 0; i < tracker.nentries; i++) {
        uint32 index = tracker[i];

        if (index >= tos())
            continue;

        FrameEntry *fe = &entries[index];
        if (!fe->data.synced()) {
            syncData(fe, masm);
            fe->data.sync();
            if (fe->isConstant())
                fe->type.sync();
        }
        if (fe->data.inRegister() && kill.hasReg(fe->data.reg())) {
            JS_ASSERT(fe->data.synced());
            forgetReg(fe->data.reg());
            fe->data.setMemory();
        }
        if (!fe->type.synced()) {
            syncType(fe, masm);
            fe->type.sync();
        }
        if (fe->type.inRegister() && kill.hasReg(fe->type.reg())) {
            JS_ASSERT(fe->type.synced());
            forgetReg(fe->type.reg());
            fe->type.setMemory();
        }
    }
}


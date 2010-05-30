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
#if !defined jsjaeger_framestate_inl_h__ && defined JS_METHODJIT
#define jsjaeger_framestate_inl_h__

namespace js {
namespace mjit {

inline FrameEntry *
FrameState::addToTracker(uint32 index)
{
    JS_ASSERT(!base[index]);
    tracker.add(index);
    base[index] = &entries[index];
    return base[index];
}

inline FrameEntry *
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

inline void
FrameState::popn(uint32 n)
{
    for (uint32 i = 0; i < n; i++)
        pop();
}

inline JSC::MacroAssembler::RegisterID
FrameState::allocReg()
{
    return alloc();
}

inline JSC::MacroAssembler::RegisterID
FrameState::alloc()
{
    if (freeRegs.empty())
        evictSomething();
    RegisterID reg = freeRegs.takeAnyReg();
    regstate[reg].fe = NULL;
    return reg;
}

inline JSC::MacroAssembler::RegisterID
FrameState::alloc(FrameEntry *fe, RematInfo::RematType type, bool weak)
{
    if (freeRegs.empty())
        evictSomething();
    RegisterID reg = freeRegs.takeAnyReg();
    regstate[reg] = RegisterState(fe, type, weak);
    return reg;
}

inline void
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

inline void
FrameState::freeReg(RegisterID reg)
{
    JS_ASSERT(!regstate[reg].fe);
    forgetReg(reg);
}

inline void
FrameState::forgetReg(RegisterID reg)
{
    freeRegs.putReg(reg);
}

inline void
FrameState::forgetEverything(uint32 newStackDepth)
{
    forgetEverything();
    sp = spBase + newStackDepth;
}

inline FrameEntry *
FrameState::rawPush()
{
    sp++;

    if (FrameEntry *fe = sp[-1])
        return fe;

    return addToTracker(&sp[-1] - base);
}

inline void
FrameState::push(const Value &v)
{
    FrameEntry *fe = rawPush();
    fe->setConstant(Jsvalify(v));
}

inline void
FrameState::pushSynced()
{
    sp++;

    if (FrameEntry *fe = sp[-1])
        fe->resetSynced();
}

inline void
FrameState::pushSyncedType(uint32 tag)
{
    FrameEntry *fe = rawPush();

    fe->type.unsync();
    fe->setTypeTag(tag);
    fe->data.setMemory();
}

inline void
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

inline void
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

inline JSC::MacroAssembler::RegisterID
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

inline void
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

inline void
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

} /* namspace mjit */
} /* namspace js */

#endif /* include */


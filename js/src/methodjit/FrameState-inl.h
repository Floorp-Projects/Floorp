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

inline void
FrameState::addToTracker(FrameEntry *fe)
{
    JS_ASSERT(!fe->isTracked());
    fe->track(tracker.nentries);
    tracker.add(fe);
    JS_ASSERT(tracker.nentries <= script->nslots);
}

inline FrameEntry *
FrameState::peek(int32 depth)
{
    JS_ASSERT(depth < 0);
    JS_ASSERT(sp + depth >= spBase);
    FrameEntry *fe = &sp[depth];
    if (!fe->isTracked()) {
        addToTracker(fe);
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

inline bool
FrameState::haveSameBacking(FrameEntry *lhs, FrameEntry *rhs)
{
    if (lhs->isCopy())
        lhs = lhs->copyOf();
    if (rhs->isCopy())
        rhs = rhs->copyOf();
    return lhs == rhs;
}

inline JSC::MacroAssembler::RegisterID
FrameState::allocReg()
{
    RegisterID reg;
    if (!freeRegs.empty()) {
        reg = freeRegs.takeAnyReg();
    } else {
        reg = evictSomeReg();
        regstate[reg].forget();
    }

    return reg;
}

inline JSC::MacroAssembler::RegisterID
FrameState::allocReg(uint32 mask)
{
    RegisterID reg;
    if (freeRegs.hasRegInMask(mask)) {
        reg = freeRegs.takeRegInMask(mask);
    } else {
        reg = evictSomeReg(mask);
        regstate[reg].forget();
    }

    return reg;
}

inline JSC::MacroAssembler::RegisterID
FrameState::allocReg(FrameEntry *fe, RematInfo::RematType type)
{
    RegisterID reg;
    if (!freeRegs.empty()) {
        reg = freeRegs.takeAnyReg();
    } else {
        reg = evictSomeReg();
        regstate[reg].forget();
    }

    regstate[reg].associate(fe, type);

    return reg;
}

inline void
FrameState::emitLoadTypeTag(FrameEntry *fe, RegisterID reg) const
{
    emitLoadTypeTag(this->masm, fe, reg);
}

inline void
FrameState::emitLoadTypeTag(Assembler &masm, FrameEntry *fe, RegisterID reg) const
{
    if (fe->isCopy())
        fe = fe->copyOf();
    masm.loadTypeTag(addressOf(fe), reg);
}

inline void
FrameState::convertInt32ToDouble(Assembler &masm, FrameEntry *fe, FPRegisterID fpreg) const
{
    JS_ASSERT(!fe->isConstant());

    if (fe->isCopy())
        fe = fe->copyOf();
    
    if (fe->data.inRegister())
        masm.convertInt32ToDouble(fe->data.reg(), fpreg);
    else
        masm.convertInt32ToDouble(addressOf(fe), fpreg);
}

inline bool
FrameState::peekTypeInRegister(FrameEntry *fe) const
{
    if (fe->isCopy())
        fe = fe->copyOf();
    return fe->type.inRegister();
}

inline void
FrameState::pop()
{
    JS_ASSERT(sp > spBase);

    FrameEntry *fe = --sp;
    if (!fe->isTracked())
        return;

    forgetAllRegs(fe);
}

inline void
FrameState::freeReg(RegisterID reg)
{
    JS_ASSERT(!regstate[reg].usedBy());

    freeRegs.putReg(reg);
}

inline void
FrameState::forgetReg(RegisterID reg)
{
    /*
     * Important: Do not touch the fe here. We can peephole optimize away
     * loads and stores by re-using the contents of old FEs.
     */
    JS_ASSERT_IF(regstate[reg].fe(), !regstate[reg].fe()->isCopy());

    if (!regstate[reg].isPinned()) {
        regstate[reg].forget();
        freeRegs.putReg(reg);
    }
}

inline void
FrameState::syncAndForgetEverything(uint32 newStackDepth)
{
    syncAndForgetEverything();
    sp = spBase + newStackDepth;
}

inline FrameEntry *
FrameState::rawPush()
{
    JS_ASSERT(unsigned(sp - entries) < nargs + script->nslots);

    if (!sp->isTracked())
        addToTracker(sp);

    return sp++;
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
    if (sp->isTracked())
        sp->resetSynced();
    sp++;
}

inline void
FrameState::pushSyncedType(JSValueType type)
{
    FrameEntry *fe = rawPush();

    fe->resetSynced();
    fe->setType(type);
}

inline void
FrameState::pushSynced(JSValueType type, RegisterID reg)
{
    FrameEntry *fe = rawPush();

    fe->resetUnsynced();
    fe->type.sync();
    fe->data.sync();
    fe->setType(type);
    fe->data.setRegister(reg);
    regstate[reg].associate(fe, RematInfo::DATA);
}

inline void
FrameState::push(Address address)
{
    FrameEntry *fe = rawPush();

    /* :XXX: X64 */
    fe->resetUnsynced();

    /* Prevent us from clobbering this reg. */
    bool free = freeRegs.hasReg(address.base);
    if (free)
        freeRegs.takeReg(address.base);

    RegisterID dreg = allocReg(fe, RematInfo::DATA);
    masm.loadPayload(address, dreg);
    fe->data.setRegister(dreg);

    /* Now it's safe to grab this register again. */
    if (free)
        freeRegs.putReg(address.base);

    RegisterID treg = allocReg(fe, RematInfo::TYPE);
    masm.loadTypeTag(address, treg);
    fe->type.setRegister(treg);
}

inline void
FrameState::pushRegs(RegisterID type, RegisterID data)
{
    JS_ASSERT(!freeRegs.hasReg(type) && !freeRegs.hasReg(data));

    FrameEntry *fe = rawPush();

    fe->resetUnsynced();
    fe->type.setRegister(type);
    fe->data.setRegister(data);
    regstate[type].associate(fe, RematInfo::TYPE);
    regstate[data].associate(fe, RematInfo::DATA);
}

inline void
FrameState::pushTypedPayload(JSValueType type, RegisterID payload)
{
    JS_ASSERT(!freeRegs.hasReg(payload));

    FrameEntry *fe = rawPush();

    fe->resetUnsynced();
    fe->setType(type);
    fe->data.setRegister(payload);
    regstate[payload].associate(fe, RematInfo::DATA);
}

inline void
FrameState::pushNumber(MaybeRegisterID payload, bool asInt32)
{
    JS_ASSERT_IF(payload.isSet(), !freeRegs.hasReg(payload.reg()));

    FrameEntry *fe = rawPush();
    fe->clear();

    JS_ASSERT(!fe->isNumber);

    if (asInt32) {
        if (!fe->type.synced())
            masm.storeTypeTag(ImmType(JSVAL_TYPE_INT32), addressOf(fe));
        fe->type.setMemory();
    } else {
        fe->type.setMemory();
    }

    fe->isNumber = true;
    if (payload.isSet()) {
        fe->data.unsync();
        fe->data.setRegister(payload.reg());
        regstate[payload.reg()].associate(fe, RematInfo::DATA);
    } else {
        fe->data.setMemory();
    }
}

inline void
FrameState::pushInt32(RegisterID payload)
{
    FrameEntry *fe = rawPush();
    fe->clear();
    JS_ASSERT(!fe->isNumber);

    masm.storeTypeTag(ImmType(JSVAL_TYPE_INT32), addressOf(fe));
    fe->type.setMemory();

    fe->isNumber = true;
    fe->data.unsync();
    fe->data.setRegister(payload);
    regstate[payload].associate(fe, RematInfo::DATA);
}

inline void
FrameState::pushUntypedPayload(JSValueType type, RegisterID payload)
{
    JS_ASSERT(!freeRegs.hasReg(payload));

    FrameEntry *fe = rawPush();

    fe->clear();

    masm.storeTypeTag(ImmType(type), addressOf(fe));

    /* The forceful type sync will assert otherwise. */
#ifdef DEBUG
    fe->type.unsync();
#endif
    fe->type.setMemory();
    fe->data.unsync();
    fe->setNotCopied();
    fe->setCopyOf(NULL);
    fe->data.setRegister(payload);
    regstate[payload].associate(fe, RematInfo::DATA);
}

inline JSC::MacroAssembler::RegisterID
FrameState::tempRegForType(FrameEntry *fe, RegisterID fallback)
{
    JS_ASSERT(!regstate[fallback].fe());
    if (fe->isCopy())
        fe = fe->copyOf();

    JS_ASSERT(!fe->type.isConstant());

    if (fe->type.inRegister())
        return fe->type.reg();

    /* :XXX: X86 */

    masm.loadTypeTag(addressOf(fe), fallback);
    return fallback;
}


inline JSC::MacroAssembler::RegisterID
FrameState::tempRegForType(FrameEntry *fe)
{
    if (fe->isCopy())
        fe = fe->copyOf();

    JS_ASSERT(!fe->type.isConstant());

    if (fe->type.inRegister())
        return fe->type.reg();

    /* :XXX: X86 */

    RegisterID reg = allocReg(fe, RematInfo::TYPE);
    masm.loadTypeTag(addressOf(fe), reg);
    fe->type.setRegister(reg);
    return reg;
}

inline JSC::MacroAssembler::RegisterID
FrameState::tempRegForData(FrameEntry *fe)
{
    JS_ASSERT(!fe->data.isConstant());

    if (fe->isCopy())
        fe = fe->copyOf();

    if (fe->data.inRegister())
        return fe->data.reg();

    RegisterID reg = allocReg(fe, RematInfo::DATA);
    masm.loadPayload(addressOf(fe), reg);
    fe->data.setRegister(reg);
    return reg;
}

inline JSC::MacroAssembler::RegisterID
FrameState::tempRegInMaskForData(FrameEntry *fe, uint32 mask)
{
    JS_ASSERT(!fe->data.isConstant());

    if (fe->isCopy())
        fe = fe->copyOf();

    RegisterID reg;
    if (fe->data.inRegister()) {
        RegisterID old = fe->data.reg();
        if (Registers::maskReg(old) & mask)
            return old;

        /* Keep the old register pinned. */
        regstate[old].forget();
        reg = allocReg(mask);
        masm.move(old, reg);
        freeReg(old);
    } else {
        reg = allocReg(mask);
        masm.loadPayload(addressOf(fe), reg);
    }
    regstate[reg].associate(fe, RematInfo::DATA);
    fe->data.setRegister(reg);
    return reg;
}

inline JSC::MacroAssembler::RegisterID
FrameState::tempRegForData(FrameEntry *fe, RegisterID reg, Assembler &masm) const
{
    JS_ASSERT(!fe->data.isConstant());

    if (fe->isCopy())
        fe = fe->copyOf();

    if (fe->data.inRegister()) {
        JS_ASSERT(fe->data.reg() != reg);
        return fe->data.reg();
    } else {
        masm.loadPayload(addressOf(fe), reg);
        return reg;
    }
}

inline bool
FrameState::shouldAvoidTypeRemat(FrameEntry *fe)
{
    return fe->type.inMemory();
}

inline bool
FrameState::shouldAvoidDataRemat(FrameEntry *fe)
{
    return fe->data.inMemory();
}

inline void
FrameState::syncType(const FrameEntry *fe, Address to, Assembler &masm) const
{
    JS_ASSERT_IF(fe->type.synced(),
                 fe->isCopied() && addressOf(fe).offset != to.offset);
    JS_ASSERT(fe->type.inRegister() || fe->type.isConstant());

    if (fe->type.isConstant()) {
        JS_ASSERT(fe->isTypeKnown());
        masm.storeTypeTag(ImmType(fe->getKnownType()), to);
    } else {
        masm.storeTypeTag(fe->type.reg(), to);
    }
}

inline void
FrameState::syncData(const FrameEntry *fe, Address to, Assembler &masm) const
{
    JS_ASSERT_IF(addressOf(fe).base == to.base &&
                 addressOf(fe).offset == to.offset,
                 !fe->data.synced());
    JS_ASSERT(fe->data.inRegister() || fe->data.isConstant());

    if (fe->data.isConstant()) {
        if (!fe->type.synced())
            masm.storeValue(fe->getValue(), to);
        else
#if defined JS_NUNBOX32
            masm.storePayload(Imm32(fe->getPayload32()), to);
#elif defined JS_PUNBOX64
            masm.storePayload(Imm64(fe->getPayload64()), to);
#endif
    } else {
        masm.storePayload(fe->data.reg(), to);
    }
}

inline void
FrameState::forgetType(FrameEntry *fe)
{
    JS_ASSERT(fe->isTypeKnown() && !fe->type.synced());
    syncType(fe, addressOf(fe), masm);
    fe->type.setMemory();
}

inline void
FrameState::learnType(FrameEntry *fe, JSValueType type)
{
    if (fe->type.inRegister())
        forgetReg(fe->type.reg());
#ifdef DEBUG
    fe->isNumber = false;
#endif
    fe->setType(type);
}

inline JSC::MacroAssembler::Address
FrameState::addressOf(const FrameEntry *fe) const
{
    uint32 index = (fe - entries);
    JS_ASSERT(index >= nargs);
    index -= nargs;
    return Address(JSFrameReg, sizeof(JSStackFrame) + sizeof(Value) * index);
}

inline JSC::MacroAssembler::Address
FrameState::addressForDataRemat(const FrameEntry *fe) const
{
    if (fe->isCopy() && !fe->data.synced())
        fe = fe->copyOf();
    JS_ASSERT(fe->data.synced());
    return addressOf(fe);
}

inline JSC::MacroAssembler::Jump
FrameState::testNull(Assembler::Condition cond, FrameEntry *fe)
{
    JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
    if (shouldAvoidTypeRemat(fe))
        return masm.testNull(cond, addressOf(fe));
    return masm.testNull(cond, tempRegForType(fe));
}

inline JSC::MacroAssembler::Jump
FrameState::testInt32(Assembler::Condition cond, FrameEntry *fe)
{
    JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
    if (shouldAvoidTypeRemat(fe))
        return masm.testInt32(cond, addressOf(fe));
    return masm.testInt32(cond, tempRegForType(fe));
}

inline JSC::MacroAssembler::Jump
FrameState::testPrimitive(Assembler::Condition cond, FrameEntry *fe)
{
    JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
    if (shouldAvoidTypeRemat(fe))
        return masm.testPrimitive(cond, addressOf(fe));
    return masm.testPrimitive(cond, tempRegForType(fe));
}

inline JSC::MacroAssembler::Jump
FrameState::testObject(Assembler::Condition cond, FrameEntry *fe)
{
    JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
    if (shouldAvoidTypeRemat(fe))
        return masm.testObject(cond, addressOf(fe));
    return masm.testObject(cond, tempRegForType(fe));
}

inline JSC::MacroAssembler::Jump
FrameState::testDouble(Assembler::Condition cond, FrameEntry *fe)
{
    JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
    if (shouldAvoidTypeRemat(fe))
        return masm.testDouble(cond, addressOf(fe));
    return masm.testDouble(cond, tempRegForType(fe));
}

inline JSC::MacroAssembler::Jump
FrameState::testBoolean(Assembler::Condition cond, FrameEntry *fe)
{
    JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
    if (shouldAvoidTypeRemat(fe))
        return masm.testBoolean(cond, addressOf(fe));
    return masm.testBoolean(cond, tempRegForType(fe));
}

inline JSC::MacroAssembler::Jump
FrameState::testString(Assembler::Condition cond, FrameEntry *fe)
{
    JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
    if (shouldAvoidTypeRemat(fe))
        return masm.testString(cond, addressOf(fe));
    return masm.testString(cond, tempRegForType(fe));
}

inline FrameEntry *
FrameState::getLocal(uint32 slot)
{
    uint32 index = nargs + slot;
    FrameEntry *fe = &entries[index];
    if (!fe->isTracked()) {
        addToTracker(fe);
        fe->resetSynced();
    }
    return fe;
}

inline void
FrameState::pinReg(RegisterID reg)
{
    regstate[reg].pin();
}

inline void
FrameState::unpinReg(RegisterID reg)
{
    regstate[reg].unpin();
}

inline void
FrameState::unpinKilledReg(RegisterID reg)
{
    regstate[reg].unpinUnsafe();
    freeRegs.putReg(reg);
}

inline void
FrameState::forgetAllRegs(FrameEntry *fe)
{
    if (fe->type.inRegister())
        forgetReg(fe->type.reg());
    if (fe->data.inRegister())
        forgetReg(fe->data.reg());
}

inline void
FrameState::swapInTracker(FrameEntry *lhs, FrameEntry *rhs)
{
    uint32 li = lhs->trackerIndex();
    uint32 ri = rhs->trackerIndex();
    JS_ASSERT(tracker[li] == lhs);
    JS_ASSERT(tracker[ri] == rhs);
    tracker.entries[ri] = lhs;
    tracker.entries[li] = rhs;
    lhs->index_ = ri;
    rhs->index_ = li;
}

inline uint32
FrameState::localIndex(uint32 n)
{
    return nargs + n;
}

inline void
FrameState::dup()
{
    dupAt(-1);
}

inline void
FrameState::dup2()
{
    FrameEntry *lhs = peek(-2);
    FrameEntry *rhs = peek(-1);
    pushCopyOf(indexOfFe(lhs));
    pushCopyOf(indexOfFe(rhs));
}

inline void
FrameState::dupAt(int32 n)
{
    JS_ASSERT(n < 0);
    FrameEntry *fe = peek(n);
    pushCopyOf(indexOfFe(fe));
}

inline void
FrameState::pushLocal(uint32 n)
{
    if (!eval && !escaping[n]) {
        pushCopyOf(indexOfFe(getLocal(n)));
    } else {
#ifdef DEBUG
        /*
         * We really want to assert on local variables, but in the presence of
         * SETLOCAL equivocation of stack slots, and let expressions, just
         * weakly assert on the fixed local vars.
         */
        FrameEntry *fe = &locals[n];
        if (fe->isTracked() && n < script->nfixed) {
            JS_ASSERT(fe->type.inMemory());
            JS_ASSERT(fe->data.inMemory());
        }
#endif
        push(Address(JSFrameReg, sizeof(JSStackFrame) + n * sizeof(Value)));
    }
}

inline void
FrameState::leaveBlock(uint32 n)
{
    popn(n);
}

inline void
FrameState::enterBlock(uint32 n)
{
    /* expect that tracker has 0 entries, for now. */
    JS_ASSERT(!tracker.nentries);
    JS_ASSERT(uint32(sp + n - locals) <= script->nslots);

    sp += n;
}

inline void
FrameState::eviscerate(FrameEntry *fe)
{
    forgetAllRegs(fe);
    fe->type.invalidate();
    fe->data.invalidate();
    fe->setNotCopied();
    fe->setCopyOf(NULL);
}

inline bool
FrameState::addEscaping(uint32 local)
{
    if (!eval) {
        uint32 already = escaping[local];
        escaping[local] = 1;
        return !already;
    }
    return false;
}

inline StateRemat
FrameState::dataRematInfo(const FrameEntry *fe) const
{
    if (fe->isCopy())
        fe = fe->copyOf();
    StateRemat remat;
    if (fe->data.inRegister()) {
        remat.reg = fe->data.reg();
        remat.inReg = true;
    } else {
        JS_ASSERT(fe->data.synced());
        remat.offset = addressOf(fe).offset;
        remat.inReg = false;
    }
    return remat;
}

inline void
FrameState::giveOwnRegs(FrameEntry *fe)
{
    JS_ASSERT(!fe->isConstant());
    JS_ASSERT(fe == peek(-1));

    if (!fe->isCopy())
        return;

    RegisterID data = copyDataIntoReg(fe);
    if (fe->isTypeKnown()) {
        JSValueType type = fe->getKnownType();
        pop();
        pushTypedPayload(type, data);
    } else {
        RegisterID type = copyTypeIntoReg(fe);
        pop();
        pushRegs(type, data);
    }
}

inline void
FrameState::loadDouble(RegisterID t, RegisterID d, FrameEntry *fe, FPRegisterID fpReg,
                       Assembler &masm) const
{
#ifdef JS_CPU_X86
    masm.fastLoadDouble(d, t, fpReg);
#else
    loadDouble(fe, fpReg, masm);
#endif
}

inline bool
FrameState::tryFastDoubleLoad(FrameEntry *fe, FPRegisterID fpReg, Assembler &masm) const
{
#ifdef JS_CPU_X86
    if (fe->type.inRegister() && fe->data.inRegister()) {
        masm.fastLoadDouble(fe->data.reg(), fe->type.reg(), fpReg);
        return true;
    }
#endif
    return false;
}

inline void
FrameState::loadDouble(FrameEntry *fe, FPRegisterID fpReg, Assembler &masm) const
{
    if (fe->isCopy()) {
        FrameEntry *backing = fe->copyOf();
        if (tryFastDoubleLoad(fe, fpReg, masm))
            return;
        if (backing->isCachedNumber() || (backing->type.synced() && backing->data.synced())) {
            masm.loadDouble(addressOf(backing), fpReg);
            return;
        }
        fe = backing;
    }

    if (tryFastDoubleLoad(fe, fpReg, masm))
        return;

    if ((fe->type.synced() && fe->data.synced()) || fe->isCachedNumber()) {
        masm.loadDouble(addressOf(fe), fpReg);
        return;
    }

    Address address = addressOf(fe);
    do {
        if (!fe->data.synced()) {
            syncData(fe, address, masm);
            if (fe->isConstant())
                break;
        }
        if (!fe->type.synced())
            syncType(fe, address, masm);
    } while (0);

    masm.loadDouble(address, fpReg);
}

} /* namspace mjit */
} /* namspace js */

#endif /* include */


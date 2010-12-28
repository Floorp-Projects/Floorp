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
    JS_ASSERT(tracker.nentries <= feLimit());
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

inline AnyRegisterID
FrameState::allocReg(uint32 mask)
{
    if (freeRegs.hasRegInMask(mask)) {
        AnyRegisterID reg = freeRegs.takeAnyReg(mask);
        clearLoopReg(reg);
        return reg;
    }

    AnyRegisterID reg = evictSomeReg(mask);
    regstate(reg).forget();
    return reg;
}

inline JSC::MacroAssembler::RegisterID
FrameState::allocReg()
{
    return allocReg(Registers::AvailRegs).reg();
}

inline JSC::MacroAssembler::FPRegisterID
FrameState::allocFPReg()
{
    return allocReg(Registers::AvailFPRegs).fpreg();
}

inline AnyRegisterID
FrameState::allocAndLoadReg(FrameEntry *fe, bool fp, RematInfo::RematType type)
{
    AnyRegisterID reg;
    uint32 mask = fp ? (uint32) Registers::AvailFPRegs : (uint32) Registers::AvailRegs;

    /*
     * Decide whether to retroactively mark a register as holding the entry
     * at the start of the current loop. We can do this if (a) the register has
     * not been touched since the start of the loop (it is in loopRegs), and (b)
     * the entry has also not been written to or already had a loop register
     * assigned.
     */
    if (freeRegs.hasRegInMask(loopRegs.freeMask & mask) && type == RematInfo::DATA &&
        (fe == this_ || isArg(fe) || isLocal(fe)) && fe->lastLoop < activeLoop->head) {
        reg = freeRegs.takeAnyReg(loopRegs.freeMask & mask);
        setLoopReg(reg, fe);
        return reg;
    }

    if (!freeRegs.empty(mask)) {
        reg = freeRegs.takeAnyReg(mask);
        clearLoopReg(reg);
    } else {
        reg = evictSomeReg(mask);
        regstate(reg).forget();
    }

    if (fp)
        masm.loadDouble(addressOf(fe), reg.fpreg());
    else if (type == RematInfo::TYPE)
        masm.loadTypeTag(addressOf(fe), reg.reg());
    else
        masm.loadPayload(addressOf(fe), reg.reg());

    regstate(reg).associate(fe, type);
    return reg;
}

inline void
FrameState::clearLoopReg(AnyRegisterID reg)
{
    JS_ASSERT(loopRegs.hasReg(reg) == (activeLoop && activeLoop->alloc->loop(reg)));
    if (loopRegs.hasReg(reg)) {
        loopRegs.takeReg(reg);
        activeLoop->alloc->setUnassigned(reg);
        JaegerSpew(JSpew_Regalloc, "clearing loop register %s\n", reg.name());
    }
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
FrameState::freeReg(AnyRegisterID reg)
{
    JS_ASSERT(!regstate(reg).usedBy());

    freeRegs.putReg(reg);
}

inline void
FrameState::forgetReg(AnyRegisterID reg)
{
    /*
     * Important: Do not touch the fe here. We can peephole optimize away
     * loads and stores by re-using the contents of old FEs.
     */
    JS_ASSERT_IF(regstate(reg).fe(), !regstate(reg).fe()->isCopy());

    if (!regstate(reg).isPinned()) {
        regstate(reg).forget();
        freeRegs.putReg(reg);
    }
}

inline FrameEntry *
FrameState::rawPush()
{
    JS_ASSERT(unsigned(sp - entries) < feLimit());

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
FrameState::pushSynced(JSValueType type, types::TypeSet *typeSet)
{
    FrameEntry *fe = rawPush();

    fe->resetSynced();
    if (type != JSVAL_TYPE_UNKNOWN) {
        fe->setType(type, typeSet);
        if (type == JSVAL_TYPE_DOUBLE)
            masm.ensureInMemoryDouble(addressOf(fe));
    }
}

inline void
FrameState::pushSynced(JSValueType type, RegisterID reg, types::TypeSet *typeSet)
{
    FrameEntry *fe = rawPush();

    fe->resetUnsynced();
    fe->type.sync();
    fe->data.sync();
    fe->setType(type, typeSet);
    fe->data.setRegister(reg);
    regstate(reg).associate(fe, RematInfo::DATA);
}

inline void
FrameState::push(Address address, JSValueType knownType, types::TypeSet *typeSet)
{
    if (knownType == JSVAL_TYPE_DOUBLE) {
        FPRegisterID fpreg = allocFPReg();
        masm.moveInt32OrDouble(address, fpreg);
        pushDouble(fpreg);
        return;
    }

#ifdef JS_PUNBOX64
    // It's okay if either of these clobbers address.base, since we guarantee
    // eviction will not physically clobber. It's also safe, on x64, for
    // loadValueAsComponents() to take either type or data regs as address.base.
    RegisterID typeReg = allocReg();
    RegisterID dataReg = allocReg();
    masm.loadValueAsComponents(address, typeReg, dataReg);
#elif JS_NUNBOX32
    // Prevent us from clobbering this reg.
    bool free = freeRegs.hasReg(address.base);
    if (free)
        freeRegs.takeReg(address.base);

    if (knownType != JSVAL_TYPE_UNKNOWN) {
        RegisterID dataReg = allocReg();
        if (free)
            freeRegs.putReg(address.base);
        masm.loadPayload(address, dataReg);
        pushTypedPayload(knownType, dataReg, typeSet);
        return;
    }

    RegisterID typeReg = allocReg();

    masm.loadTypeTag(address, typeReg);

    // Allow re-use of the base register. This could avoid a spill, and
    // is safe because the following allocReg() won't actually emit any
    // writes to the register.
    if (free)
        freeRegs.putReg(address.base);

    RegisterID dataReg = allocReg();
    masm.loadPayload(address, dataReg);
#endif

    pushRegs(typeReg, dataReg, knownType, typeSet);
}

inline JSC::MacroAssembler::FPRegisterID
FrameState::pushRegs(RegisterID type, RegisterID data, JSValueType knownType, types::TypeSet *typeSet)
{
    JS_ASSERT(!freeRegs.hasReg(type) && !freeRegs.hasReg(data));

    if (knownType == JSVAL_TYPE_UNKNOWN) {
        FrameEntry *fe = rawPush();
        fe->resetUnsynced();
        fe->type.setRegister(type);
        fe->data.setRegister(data);
        regstate(type).associate(fe, RematInfo::TYPE);
        regstate(data).associate(fe, RematInfo::DATA);
        return Registers::FPConversionTemp;
    }

    if (knownType == JSVAL_TYPE_DOUBLE) {
        FPRegisterID fpreg = allocFPReg();
        masm.moveInt32OrDouble(data, type, addressOf(sp), fpreg);
        pushDouble(fpreg);
        freeReg(type);
        freeReg(data);
        return fpreg;
    }

    freeReg(type);
    pushTypedPayload(knownType, data, typeSet);
    return Registers::FPConversionTemp;
}

inline void
FrameState::pushTypedPayload(JSValueType type, RegisterID payload, types::TypeSet *typeSet)
{
    JS_ASSERT(type != JSVAL_TYPE_DOUBLE);
    JS_ASSERT(!freeRegs.hasReg(payload));

    FrameEntry *fe = rawPush();

    fe->resetUnsynced();
    fe->setType(type, typeSet);
    fe->data.setRegister(payload);
    regstate(payload).associate(fe, RematInfo::DATA);
}

inline void
FrameState::pushNumber(RegisterID payload, bool asInt32)
{
    JS_ASSERT(!freeRegs.hasReg(payload));

    FrameEntry *fe = rawPush();
    fe->clear();

    if (asInt32) {
        if (!fe->type.synced())
            masm.storeTypeTag(ImmType(JSVAL_TYPE_INT32), addressOf(fe));
        fe->type.setMemory();
    } else {
        fe->type.setMemory();
    }

    fe->data.unsync();
    fe->data.setRegister(payload);
    regstate(payload).associate(fe, RematInfo::DATA);
}

inline void
FrameState::pushInt32(RegisterID payload)
{
    FrameEntry *fe = rawPush();
    fe->clear();

    masm.storeTypeTag(ImmType(JSVAL_TYPE_INT32), addressOf(fe));
    fe->type.setMemory();

    fe->data.unsync();
    fe->data.setRegister(payload);
    regstate(payload).associate(fe, RematInfo::DATA);
}

inline void
FrameState::pushInitializerObject(RegisterID payload, bool array, JSObject *baseobj)
{
    pushTypedPayload(JSVAL_TYPE_OBJECT, payload, NULL);

    FrameEntry *fe = peek(-1);
    fe->initArray = array;
    fe->initObject = baseobj;
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
    regstate(payload).associate(fe, RematInfo::DATA);
}

inline JSC::MacroAssembler::RegisterID
FrameState::tempRegForType(FrameEntry *fe, RegisterID fallback)
{
    JS_ASSERT(!regstate(fallback).fe());
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

    RegisterID reg = allocAndLoadReg(fe, false, RematInfo::TYPE).reg();
    fe->type.setRegister(reg);
    return reg;
}

inline JSC::MacroAssembler::RegisterID
FrameState::tempRegForData(FrameEntry *fe)
{
    JS_ASSERT(!fe->data.isConstant());
    JS_ASSERT(!fe->isType(JSVAL_TYPE_DOUBLE));

    if (fe->isCopy())
        fe = fe->copyOf();

    if (fe->data.inRegister())
        return fe->data.reg();

    RegisterID reg = allocAndLoadReg(fe, false, RematInfo::DATA).reg();
    fe->data.setRegister(reg);
    return reg;
}

inline JSC::MacroAssembler::FPRegisterID
FrameState::tempFPRegForData(FrameEntry *fe)
{
    JS_ASSERT(!fe->data.isConstant());
    JS_ASSERT(fe->isType(JSVAL_TYPE_DOUBLE));

    if (fe->isCopy())
        fe = fe->copyOf();

    if (fe->data.inFPRegister())
        return fe->data.fpreg();

    FPRegisterID reg = allocAndLoadReg(fe, true, RematInfo::DATA).fpreg();
    fe->data.setFPRegister(reg);
    return reg;
}

inline JSC::MacroAssembler::RegisterID
FrameState::tempRegInMaskForData(FrameEntry *fe, uint32 mask)
{
    JS_ASSERT(!fe->data.isConstant());
    JS_ASSERT(!fe->isType(JSVAL_TYPE_DOUBLE));
    JS_ASSERT(!(mask & ~Registers::AvailRegs));

    if (fe->isCopy())
        fe = fe->copyOf();

    RegisterID reg;
    if (fe->data.inRegister()) {
        RegisterID old = fe->data.reg();
        if (Registers::maskReg(old) & mask)
            return old;

        /* Keep the old register pinned. */
        regstate(old).forget();
        reg = allocReg(mask).reg();
        masm.move(old, reg);
        freeReg(old);
    } else {
        reg = allocReg(mask).reg();
        masm.loadPayload(addressOf(fe), reg);
    }
    regstate(reg).associate(fe, RematInfo::DATA);
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
FrameState::ensureFeSynced(const FrameEntry *fe, Assembler &masm) const
{
    Address to = addressOf(fe);
    const FrameEntry *backing = fe;
    if (fe->isCopy())
        backing = fe->copyOf();

    if (backing->isType(JSVAL_TYPE_DOUBLE)) {
        if (fe->data.synced()) {
            /* Entries representing known doubles can't be partially synced. */
            JS_ASSERT(fe->type.synced());
            return;
        }
        if (backing->isConstant()) {
            masm.storeValue(backing->getValue(), to);
        } else if (backing->data.inFPRegister()) {
            masm.storeDouble(backing->data.fpreg(), to);
        } else {
            /* Use a temporary so the entry can be synced without allocating a register. */
            JS_ASSERT(backing->data.inMemory() && backing != fe);
            masm.loadDouble(addressOf(backing), Registers::FPConversionTemp);
            masm.storeDouble(Registers::FPConversionTemp, to);
        }
        return;
    }

#if defined JS_PUNBOX64
    /* If we can, sync the type and data in one go. */
    if (!fe->data.synced() && !fe->type.synced()) {
        if (backing->isConstant())
            masm.storeValue(backing->getValue(), to);
        else if (backing->isTypeKnown())
            masm.storeValueFromComponents(ImmType(backing->getKnownType()), backing->data.reg(), to);
        else
            masm.storeValueFromComponents(backing->type.reg(), backing->data.reg(), to);
        return;
    }
#endif

    /* 
     * On x86_64, only one of the following two calls will have output,
     * and a load will only occur if necessary.
     */
    ensureDataSynced(fe, masm);
    ensureTypeSynced(fe, masm);
}

inline void
FrameState::ensureTypeSynced(const FrameEntry *fe, Assembler &masm) const
{
    if (fe->type.synced())
        return;

    Address to = addressOf(fe);
    const FrameEntry *backing = fe;
    if (fe->isCopy())
        backing = fe->copyOf();

#if defined JS_PUNBOX64
    /* Attempt to store the entire Value, to prevent a load. */
    if (backing->isConstant()) {
        masm.storeValue(backing->getValue(), to);
        return;
    }

    if (backing->data.inRegister()) {
        RegisterID dreg = backing->data.reg();
        if (backing->isTypeKnown())
            masm.storeValueFromComponents(ImmType(backing->getKnownType()), dreg, to);
        else
            masm.storeValueFromComponents(backing->type.reg(), dreg, to);
        return;
    }
#endif

    /* Store a double's type bits, even though !isTypeKnown(). */
    if (backing->isConstant())
        masm.storeTypeTag(ImmTag(backing->getKnownTag()), to);
    else if (backing->isTypeKnown())
        masm.storeTypeTag(ImmType(backing->getKnownType()), to); 
    else
        masm.storeTypeTag(backing->type.reg(), to);
}

inline void
FrameState::ensureDataSynced(const FrameEntry *fe, Assembler &masm) const
{
    if (fe->data.synced())
        return;

    Address to = addressOf(fe);
    const FrameEntry *backing = fe;
    if (fe->isCopy())
        backing = fe->copyOf();

#if defined JS_PUNBOX64
    if (backing->isConstant())
        masm.storeValue(backing->getValue(), to);
    else if (backing->isTypeKnown())
        masm.storeValueFromComponents(ImmType(backing->getKnownType()), backing->data.reg(), to);
    else if (backing->type.inRegister())
        masm.storeValueFromComponents(backing->type.reg(), backing->data.reg(), to);
    else
        masm.storePayload(backing->data.reg(), to);
#elif defined JS_NUNBOX32
    if (backing->isConstant())
        masm.storePayload(ImmPayload(backing->getPayload()), to);
    else
        masm.storePayload(backing->data.reg(), to);
#endif
}

inline void
FrameState::syncFe(FrameEntry *fe)
{
    if (fe->type.synced() && fe->data.synced())
        return;

    FrameEntry *backing = fe;
    if (fe->isCopy())
        backing = fe->copyOf();

    if (backing->isType(JSVAL_TYPE_DOUBLE)) {
        if (!backing->isConstant())
            tempFPRegForData(backing);
        ensureFeSynced(fe, masm);

        if (!fe->type.synced())
            fe->type.sync();
        if (!fe->data.synced())
            fe->data.sync();
        return;
    }

    bool needTypeReg = !fe->type.synced() && backing->type.inMemory();
    bool needDataReg = !fe->data.synced() && backing->data.inMemory();

#if defined JS_NUNBOX32
    /* Determine an ordering that won't spill known regs. */
    if (needTypeReg && !needDataReg) {
        syncData(fe);
        syncType(fe);
    } else {
        syncType(fe);
        syncData(fe);
    }
#elif defined JS_PUNBOX64
    if (JS_UNLIKELY(needTypeReg && needDataReg)) {
        /* Memory-to-memory moves can only occur for copies backed by memory. */
        JS_ASSERT(backing != fe);

        /* Use ValueReg to do a whole-Value mem-to-mem move. */
        masm.loadValue(addressOf(backing), Registers::ValueReg);
        masm.storeValue(Registers::ValueReg, addressOf(fe));
    } else {
        /* Store in case unpinning is necessary. */
        MaybeRegisterID pairReg;

        /* Get a register if necessary, without clobbering its pair. */
        if (needTypeReg) {
            if (backing->data.inRegister() && !regstate(backing->data.reg()).isPinned()) {
                pairReg = backing->data.reg();
                pinReg(backing->data.reg());
            }
            tempRegForType(backing);
        } else if (needDataReg) {
            if (backing->type.inRegister() && !regstate(backing->type.reg()).isPinned()) {
                pairReg = backing->type.reg();
                pinReg(backing->type.reg());
            }
            tempRegForData(backing);
        }

        ensureFeSynced(fe, masm);

        if (pairReg.isSet())
            unpinReg(pairReg.reg());
    }

    if (!fe->type.synced())
        fe->type.sync();
    if (!fe->data.synced())
        fe->data.sync();
#endif
}

inline void
FrameState::syncAndForgetFe(FrameEntry *fe)
{
    syncFe(fe);
    forgetAllRegs(fe);
    fe->type.setMemory();
    fe->data.setMemory();
}

inline void
FrameState::syncType(FrameEntry *fe)
{
    JS_ASSERT(!fe->isType(JSVAL_TYPE_DOUBLE));

    FrameEntry *backing = fe;
    if (fe->isCopy())
        backing = fe->copyOf();

    if (!fe->type.synced() && backing->type.inMemory())
        tempRegForType(backing);

    ensureTypeSynced(fe, masm);

    if (!fe->type.synced())
        fe->type.sync();
}

inline void
FrameState::syncData(FrameEntry *fe)
{
    JS_ASSERT(!fe->isType(JSVAL_TYPE_DOUBLE));

    FrameEntry *backing = fe;
    if (fe->isCopy())
        backing = fe->copyOf();

    if (!fe->data.synced() && backing->data.inMemory())
        tempRegForData(backing);

    ensureDataSynced(fe, masm);

    if (!fe->data.synced())
        fe->data.sync();
}

inline void
FrameState::forgetType(FrameEntry *fe)
{
    /*
     * The type may have been forgotten with an intervening storeLocal in the
     * presence of eval or closed variables. For defense in depth and to make
     * callers' lives simpler, bail out if the type is not known.
     */
    if (!fe->isTypeKnown())
        return;

    /*
     * Likewise, storeLocal() may have set this FE, with a known type,
     * to be a copy of another FE, which has an unknown type.
     * Just forget the type, since the backing is used in all cases.
     */
    if (fe->isCopy()) {
        fe->type.invalidate();
        return;
    }

    ensureTypeSynced(fe, masm);
    fe->type.setMemory();
}

inline void
FrameState::learnType(FrameEntry *fe, JSValueType type, bool unsync)
{
    JS_ASSERT(!fe->isType(JSVAL_TYPE_DOUBLE));
    if (fe->type.inRegister())
        forgetReg(fe->type.reg());
    fe->setType(type, NULL);
    if (unsync)
        fe->type.unsync();
}

inline void
FrameState::learnType(FrameEntry *fe, JSValueType type, RegisterID data)
{
    /* The copied bit may be set on an entry, but there should not be any actual copies. */
    JS_ASSERT_IF(fe->isCopied(), !isEntryCopied(fe));

    forgetAllRegs(fe);
    fe->copy = NULL;

    fe->type.setConstant();
    fe->knownType = type;
    fe->typeSet = NULL;

    fe->data.setRegister(data);
    regstate(data).associate(fe, RematInfo::DATA);

    fe->data.unsync();
    fe->type.unsync();
}

inline JSC::MacroAssembler::Address
FrameState::addressOf(const FrameEntry *fe) const
{
    int32 frameOffset = 0;
    if (fe >= locals)
        frameOffset = JSStackFrame::offsetOfFixed(uint32(fe - locals));
    else if (fe >= args)
        frameOffset = JSStackFrame::offsetOfFormalArg(fun, uint32(fe - args));
    else if (fe == this_)
        frameOffset = JSStackFrame::offsetOfThis(fun);
    else if (fe == callee_)
        frameOffset = JSStackFrame::offsetOfCallee(fun);
    JS_ASSERT(frameOffset);
    return Address(JSFrameReg, frameOffset);
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
FrameState::testUndefined(Assembler::Condition cond, FrameEntry *fe)
{
    JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
    if (shouldAvoidTypeRemat(fe))
        return masm.testUndefined(cond, addressOf(fe));
    return masm.testUndefined(cond, tempRegForType(fe));
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
FrameState::getOrTrack(uint32 index)
{
    FrameEntry *fe = &entries[index];
    if (!fe->isTracked()) {
        addToTracker(fe);
        fe->resetSynced();
    }
    return fe;
}

inline FrameEntry *
FrameState::getLocal(uint32 slot)
{
    JS_ASSERT(slot < script->nslots);
    return getOrTrack(uint32(&locals[slot] - entries));
}

inline FrameEntry *
FrameState::getArg(uint32 slot)
{
    JS_ASSERT(slot < nargs);
    return getOrTrack(uint32(&args[slot] - entries));
}

inline FrameEntry *
FrameState::getThis()
{
    return getOrTrack(uint32(this_ - entries));
}

inline FrameEntry *
FrameState::getCallee()
{
    // Callee can only be used in function code, and it's always an object.
    JS_ASSERT(fun);
    if (!callee_->isTracked()) {
        addToTracker(callee_);
        callee_->resetSynced();
        callee_->setType(JSVAL_TYPE_OBJECT, NULL);
    }
    return callee_;
}

inline void
FrameState::unpinKilledReg(RegisterID reg)
{
    regstate(reg).unpinUnsafe();
    freeRegs.putReg(reg);
}

inline void
FrameState::forgetAllRegs(FrameEntry *fe)
{
    if (fe->type.inRegister())
        forgetReg(fe->type.reg());
    if (fe->data.inRegister())
        forgetReg(fe->data.reg());
    if (fe->data.inFPRegister())
        forgetReg(fe->data.fpreg());
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
FrameState::syncAt(int32 n)
{
    JS_ASSERT(n < 0);
    FrameEntry *fe = peek(n);
    syncFe(fe);
}

inline void
FrameState::pushLocal(uint32 n, JSValueType knownType, types::TypeSet *typeSet)
{
    FrameEntry *fe = getLocal(n);
    if (!isClosedVar(n)) {
        pushCopyOf(indexOfFe(fe));
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
        push(addressOf(fe), knownType, typeSet);
    }
}

inline void
FrameState::pushArg(uint32 n, JSValueType knownType, types::TypeSet *typeSet)
{
    FrameEntry *fe = getArg(n);
    if (!isClosedArg(n)) {
        pushCopyOf(indexOfFe(fe));
    } else {
#ifdef DEBUG
        FrameEntry *fe = &args[n];
        if (fe->isTracked())
            JS_ASSERT(fe->data.inMemory());
#endif
        push(addressOf(fe), knownType, typeSet);
    }
}

inline void
FrameState::pushCallee()
{
    FrameEntry *fe = getCallee();
    pushCopyOf(indexOfFe(fe));
}

inline void
FrameState::pushThis()
{
    FrameEntry *fe = getThis();
    pushCopyOf(indexOfFe(fe));
}

void
FrameState::learnThisIsObject()
{
    // This is safe, albeit hacky. This is only called from the compiler,
    // and only on the first use of |this| inside a basic block. Thus,
    // there are no copies of |this| anywhere.
    learnType(this_, JSVAL_TYPE_OBJECT);
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

    if (!eval)
        memset(&closedVars[uint32(sp - locals)], 0, n * sizeof(*closedVars));
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

inline void
FrameState::setClosedVar(uint32 slot)
{
    if (!eval)
        closedVars[slot] = true;
}

inline void
FrameState::setClosedArg(uint32 slot)
{
    if (!eval && !usesArguments)
        closedArgs[slot] = true;
}

inline StateRemat
FrameState::dataRematInfo(const FrameEntry *fe) const
{
    if (fe->isCopy())
        fe = fe->copyOf();

    if (fe->data.inRegister())
        return StateRemat::FromRegister(fe->data.reg());

    JS_ASSERT(fe->data.synced());
    return StateRemat::FromAddress(addressOf(fe));
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
        types::TypeSet *typeSet = fe->getTypeSet();
        pop();
        pushTypedPayload(type, data, typeSet);
    } else {
        RegisterID type = copyTypeIntoReg(fe);
        pop();
        pushRegs(type, data, JSVAL_TYPE_UNKNOWN, NULL);
    }
}

inline void
FrameState::loadDouble(RegisterID t, RegisterID d, FrameEntry *fe, FPRegisterID fpreg,
                       Assembler &masm) const
{
#ifdef JS_CPU_X86
    masm.fastLoadDouble(d, t, fpreg);
#else
    loadDouble(fe, fpreg, masm);
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
        fe = backing;
    }

    if (tryFastDoubleLoad(fe, fpReg, masm))
        return;

    ensureFeSynced(fe, masm);
    masm.loadDouble(addressOf(fe), fpReg);
}

inline bool
FrameState::isClosedVar(uint32 slot) const
{
    return eval || closedVars[slot];
}

inline bool
FrameState::isClosedArg(uint32 slot) const
{
    return eval || usesArguments || closedArgs[slot];
}

class PinRegAcrossSyncAndKill
{
    typedef JSC::MacroAssembler::RegisterID RegisterID;
    FrameState &frame;
    MaybeRegisterID maybeReg;
  public:
    PinRegAcrossSyncAndKill(FrameState &frame, RegisterID reg)
      : frame(frame), maybeReg(reg)
    {
        frame.pinReg(reg);
    }
    PinRegAcrossSyncAndKill(FrameState &frame, MaybeRegisterID maybeReg)
      : frame(frame), maybeReg(maybeReg)
    {
        if (maybeReg.isSet())
            frame.pinReg(maybeReg.reg());
    }
    ~PinRegAcrossSyncAndKill() {
        if (maybeReg.isSet())
            frame.unpinKilledReg(maybeReg.reg());
    }
};

} /* namespace mjit */
} /* namespace js */

#endif /* include */


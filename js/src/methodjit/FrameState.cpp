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
#include "StubCompiler.h"

using namespace js;
using namespace js::mjit;
using namespace js::analyze;

/* Because of Value alignment */
JS_STATIC_ASSERT(sizeof(FrameEntry) % 8 == 0);

FrameState::FrameState(JSContext *cx, mjit::Compiler &cc,
                       Assembler &masm, StubCompiler &stubcc)
  : cx(cx),
    masm(masm), cc(cc), stubcc(stubcc),
    a(NULL), script(NULL), entries(NULL),
    callee_(NULL), this_(NULL), args(NULL), locals(NULL),
    spBase(NULL), sp(NULL), PC(NULL),
    loop(NULL), inTryBlock(false)
{
}

FrameState::~FrameState()
{
    while (a) {
        ActiveFrame *parent = a->parent;
        a->script->analysis(cx)->clearAllocations();
#if defined JS_NUNBOX32
        a->reifier.~ImmutableSync();
#endif
        cx->free_(a);
        a = parent;
    }
}

void
FrameState::getUnsyncedEntries(uint32 *pdepth, Vector<UnsyncedEntry> *unsyncedEntries)
{
    *pdepth = totalDepth() + VALUES_PER_STACK_FRAME;

    /* Mark all unsynced entries in the frame. */
    for (uint32 i = 0; i < a->tracker.nentries; i++) {
        FrameEntry *fe = a->tracker[i];
        if (fe >= sp)
            continue;
        if (fe->type.synced() && fe->data.synced())
            continue;
        if (fe->inlined)
            continue;

        UnsyncedEntry entry;
        PodZero(&entry);

        entry.offset = frameOffset(fe);

        if (fe->isCopy()) {
            FrameEntry *nfe = fe->copyOf();
            entry.copy = true;
            entry.u.copiedOffset = frameOffset(nfe);
        } else if (fe->isConstant()) {
            entry.constant = true;
            entry.u.value = fe->getValue();
        } else if (fe->isTypeKnown() && !fe->isType(JSVAL_TYPE_DOUBLE) && !fe->type.synced()) {
            entry.knownType = true;
            entry.u.type = fe->getKnownType();
        } else {
            /*
             * All the unsynced portions of this entry are in registers. When
             * making a call from within an inline frame, these will be synced
             * beforehand.
             */
            continue;
        }

        unsyncedEntries->append(entry);
    }
}

bool
FrameState::pushActiveFrame(JSScript *script, uint32 argc)
{
    uint32 depth = a ? totalDepth() : 0;
    uint32 nentries = feLimit(script);

    size_t totalBytes = sizeof(ActiveFrame) +
                        sizeof(FrameEntry) * nentries +              // entries[]
                        sizeof(FrameEntry *) * nentries +            // tracker.entries
                        sizeof(StackEntryExtra) * script->nslots;    // extraArray

    uint8 *cursor = (uint8 *)cx->calloc_(totalBytes);
    if (!cursor)
        return false;

    ActiveFrame *newa = (ActiveFrame *) cursor;
    cursor += sizeof(ActiveFrame);

#if defined JS_NUNBOX32
    if (!newa->reifier.init(cx, *this, nentries)) {
        cx->free_(newa);
        return false;
    }
#endif

    newa->parent = a;
    newa->parentPC = PC;
    newa->parentSP = sp;
    newa->parentArgc = argc;
    newa->script = script;
    newa->freeRegs = Registers(Registers::AvailAnyRegs);

    newa->entries = (FrameEntry *)cursor;
    cursor += sizeof(FrameEntry) * nentries;

    newa->callee_ = newa->entries;
    newa->this_ = newa->entries + 1;
    newa->args = newa->entries + 2;
    newa->locals = newa->args + (script->fun ? script->fun->nargs : 0);

    newa->tracker.entries = (FrameEntry **)cursor;
    cursor += sizeof(FrameEntry *) * nentries;

    newa->extraArray = (StackEntryExtra *)cursor;
    cursor += sizeof(StackEntryExtra) * script->nslots;

    JS_ASSERT(reinterpret_cast<uint8 *>(newa) + totalBytes == cursor);

    this->a = newa;
    updateActiveFrame();

    if (a->parent && script->analysis(cx)->inlineable(argc)) {
        a->depth = depth + VALUES_PER_STACK_FRAME;

        /* Mark all registers which are in use by the parent or its own parent. */
        a->parentRegs = 0;
        Registers regs(Registers::AvailAnyRegs);
        while (!regs.empty()) {
            AnyRegisterID reg = regs.takeAnyReg();
            if (a->parent->parentRegs.hasReg(reg) || !a->parent->freeRegs.hasReg(reg))
                a->parentRegs.putReg(reg);
        }

        JS_ASSERT(argc == script->fun->nargs);

        syncInlinedEntry(getCallee(), a->parentSP - (argc + 2));
        syncInlinedEntry(getThis(), a->parentSP - (argc + 1));
        for (unsigned i = 0; i < argc; i++)
            syncInlinedEntry(getArg(i), a->parentSP - (argc - i));
    }

    return true;
}

void
FrameState::syncInlinedEntry(FrameEntry *fe, const FrameEntry *parent)
{
    /*
     * Fill in the initial state of an entry in this inlined frame that
     * corresponds to an entry in the caller's frame.
     */

    /*
     * Make sure the initial sync state of the inlined entries matches the
     * parent. These inlined entries will never unsync (since they are never
     * modified) and will be marked as synced as necessary. Note that this
     * follows any copies in the parent to get the eventual backing of the
     * argument --- the slot we compute using getAddress. Syncing of the
     * argument slots themselves is handled by the parent's unsyncedSlots.
     */
    JS_ASSERT(fe->type.synced() && fe->data.synced());
    parent = parent->backing();
    if (!parent->type.synced())
        fe->type.unsync();
    if (!parent->data.synced())
        fe->data.unsync();

    fe->inlined = true;

    if (parent->isConstant()) {
        fe->setConstant(Jsvalify(parent->getValue()));
        return;
    }

    if (parent->isCopy())
        parent = parent->copyOf();

    if (parent->isTypeKnown())
        fe->setType(parent->getKnownType());

    if (parent->type.inRegister())
        associateReg(fe, RematInfo::TYPE, parent->type.reg());
    if (parent->data.inRegister())
        associateReg(fe, RematInfo::DATA, parent->data.reg());
    if (parent->data.inFPRegister())
        associateReg(fe, RematInfo::DATA, parent->data.fpreg());
}

void
FrameState::associateReg(FrameEntry *fe, RematInfo::RematType type, AnyRegisterID reg)
{
    a->freeRegs.takeReg(reg);

    if (type == RematInfo::TYPE)
        fe->type.setRegister(reg.reg());
    else if (reg.isReg())
        fe->data.setRegister(reg.reg());
    else
        fe->data.setFPRegister(reg.fpreg());
    regstate(reg).associate(fe, type);
}

void
FrameState::popActiveFrame()
{
    jsbytecode *parentPC = a->parentPC;
    FrameEntry *parentSP = a->parentSP;
    ActiveFrame *parent = a->parent;

    analysis->clearAllocations();

#if defined JS_NUNBOX32
    a->reifier.~ImmutableSync();
#endif
    cx->free_(a);

    a = parent;
    updateActiveFrame();
    PC = parentPC;
    sp = parentSP;
}

void
FrameState::updateActiveFrame()
{
    script = a->script;
    analysis = script->analysis(cx);
    entries = a->entries;
    callee_ = a->callee_;
    this_ = a->this_;
    args = a->args;
    locals = a->locals;
    spBase = locals + script->nfixed;
    sp = spBase;
    temporaries = locals + script->nslots;
    temporariesTop = temporaries;
}

void
FrameState::discardLocalRegisters()
{
    /* Discard all local registers, without syncing. Must be followed by a discardFrame. */
    a->freeRegs = Registers::AvailAnyRegs;
}

void
FrameState::evictInlineModifiedRegisters(Registers regs)
{
    JS_ASSERT(cx->typeInferenceEnabled());
    a->parentRegs.freeMask &= ~regs.freeMask;

    while (!regs.empty()) {
        AnyRegisterID reg = regs.takeAnyReg();
        if (a->freeRegs.hasReg(reg))
            continue;

        FrameEntry *fe = regstate(reg).fe();
        JS_ASSERT(fe);
        if (regstate(reg).type() == RematInfo::TYPE) {
            if (!fe->type.synced())
                fe->type.sync();
            fe->type.setMemory();
        } else {
            if (!fe->data.synced())
                fe->data.sync();
            if (fe->isType(JSVAL_TYPE_DOUBLE) && !fe->type.synced())
                fe->type.sync();
            fe->data.setMemory();
        }

        regstate(reg).forget();
        a->freeRegs.putReg(reg);
    }
}

void
FrameState::tryCopyRegister(FrameEntry *fe, FrameEntry *callStart)
{
    JS_ASSERT(cx->typeInferenceEnabled());
    JS_ASSERT(!fe->isCopied() || !isEntryCopied(fe));

    if (!fe->isCopy())
        return;

    /*
     * Uncopy the entry if it shares a backing with any other entry used
     * in the impending call. We want to ensure that within inline calls each
     * entry has its own set of registers.
     */

    FrameEntry *uncopyfe = NULL;
    for (FrameEntry *nfe = callStart; !uncopyfe && nfe < fe; nfe++) {
        if (!nfe->isTracked())
            continue;
        if (nfe->backing() == fe->copyOf())
            uncopyfe = nfe;
    }

    if (uncopyfe) {
        JSValueType type = fe->isTypeKnown() ? fe->getKnownType() : JSVAL_TYPE_UNKNOWN;
        if (type == JSVAL_TYPE_UNKNOWN)
            syncType(fe);
        fe->resetUnsynced();
        if (type == JSVAL_TYPE_UNKNOWN) {
            fe->type.sync();
            fe->type.setMemory();
        } else {
            fe->setType(type);
        }
        if (type == JSVAL_TYPE_DOUBLE) {
            FPRegisterID fpreg = allocFPReg();
            masm.moveDouble(tempFPRegForData(uncopyfe), fpreg);
            fe->data.setFPRegister(fpreg);
            regstate(fpreg).associate(fe, RematInfo::DATA);
        } else {
            RegisterID reg = allocReg();
            masm.move(tempRegForData(uncopyfe), reg);
            fe->data.setRegister(reg);
            regstate(reg).associate(fe, RematInfo::DATA);
        }
    } else {
        /* Try to put the entry in a register. */
        fe = fe->copyOf();
        if (fe->isType(JSVAL_TYPE_DOUBLE))
            tempFPRegForData(fe);
        else
            tempRegForData(fe);
    }
}

Registers
FrameState::getTemporaryCallRegisters(FrameEntry *callStart) const
{
    JS_ASSERT(cx->typeInferenceEnabled());

    /*
     * Get the registers in use for entries which will be popped once the
     * call at callStart finishes.
     */
    Registers regs(Registers::AvailAnyRegs & ~a->freeRegs.freeMask);
    Registers result = 0;
    while (!regs.empty()) {
        AnyRegisterID reg = regs.takeAnyReg();
        FrameEntry *fe = regstate(reg).usedBy();
        JS_ASSERT(fe);

        if (fe >= callStart)
            result.putReg(reg);
    }

    return result;
}

void
FrameState::takeReg(AnyRegisterID reg)
{
    modifyReg(reg);
    if (a->freeRegs.hasReg(reg)) {
        a->freeRegs.takeReg(reg);
        JS_ASSERT(!regstate(reg).usedBy());
    } else {
        JS_ASSERT(regstate(reg).fe());
        evictReg(reg);
        regstate(reg).forget();
    }
}

#ifdef DEBUG
const char *
FrameState::entryName(const FrameEntry *fe) const
{
    if (fe == this_)
        return "'this'";
    if (fe == callee_)
        return "callee";

    static char bufs[4][50];
    static unsigned which = 0;
    which = (which + 1) & 3;
    char *buf = bufs[which];

    if (isArg(fe))
        JS_snprintf(buf, 50, "arg%d", fe - args);
    else if (isLocal(fe))
        JS_snprintf(buf, 50, "local%d", fe - locals);
    else if (isTemporary(fe))
        JS_snprintf(buf, 50, "temp%d", fe - temporaries);
    else
        JS_snprintf(buf, 50, "slot%d", fe - spBase);
    return buf;
}
#endif

void
FrameState::evictReg(AnyRegisterID reg)
{
    FrameEntry *fe = regstate(reg).fe();

    JaegerSpew(JSpew_Regalloc, "evicting %s from %s\n", entryName(fe), reg.name());

    if (regstate(reg).type() == RematInfo::TYPE) {
        syncType(fe);
        fe->type.setMemory();
    } else if (reg.isReg()) {
        syncData(fe);
        fe->data.setMemory();
    } else {
        syncFe(fe);
        fe->data.setMemory();
    }
}

inline Lifetime *
FrameState::variableLive(FrameEntry *fe, jsbytecode *pc) const
{
    JS_ASSERT(cx->typeInferenceEnabled());
    JS_ASSERT(fe < spBase && fe != callee_);

    uint32 offset = pc - script->code;
    return analysis->liveness(indexOfFe(fe)).live(offset);
}

bool
FrameState::isEntryCopied(FrameEntry *fe) const
{
    /*
     * :TODO: It would be better for fe->isCopied() to mean 'is actually copied'
     * rather than 'might have copies', removing the need for this walk.
     */
    JS_ASSERT(fe->isCopied());

    for (uint32 i = fe->trackerIndex() + 1; i < a->tracker.nentries; i++) {
        FrameEntry *nfe = a->tracker[i];
        if (!deadEntry(nfe) && nfe->isCopy() && nfe->copyOf() == fe)
            return true;
    }

    return false;
}

AnyRegisterID
FrameState::bestEvictReg(uint32 mask, bool includePinned) const
{
    JS_ASSERT(cx->typeInferenceEnabled());

    /* Must be looking for a specific type of register. */
    JS_ASSERT((mask & Registers::AvailRegs) != (mask & Registers::AvailFPRegs));

    AnyRegisterID fallback;
    uint32 fallbackOffset = uint32(-1);

    JaegerSpew(JSpew_Regalloc, "picking best register to evict:\n");

    for (uint32 i = 0; i < Registers::TotalAnyRegisters; i++) {
        AnyRegisterID reg = AnyRegisterID::fromRaw(i);

        /* Register is not allocatable, don't bother.  */
        if (!(Registers::maskReg(reg) & mask))
            continue;

        /* Register is not owned by the FrameState. */
        FrameEntry *fe = includePinned ? regstate(reg).usedBy() : regstate(reg).fe();
        if (!fe)
            continue;

        /*
         * Liveness is not tracked for the callee or for stack slot frame entries.
         * The callee is evicted as early as needed, stack slots are evicted as
         * late as possible. :XXX: This is unfortunate if the stack slot lives
         * a long time (especially if it gets spilled anyways when we hit a branch).
         */

        if (fe == callee_) {
            JS_ASSERT(fe->inlined || (fe->data.synced() && fe->type.synced()));
            JaegerSpew(JSpew_Regalloc, "result: %s is callee\n", reg.name());
            return reg;
        }

        if (fe >= spBase && !isTemporary(fe)) {
            if (!fallback.isSet()) {
                fallback = reg;
                fallbackOffset = 0;
            }
            JaegerSpew(JSpew_Regalloc, "    %s is on stack\n", reg.name());
            continue;
        }

        /*
         * Prioritize keeping copied entries in registers. This is necessary
         * for correctness with eviction of dead variables below, as testing
         * variableLive does not consider any copies of the variable.
         */
        if (fe->isCopied() && isEntryCopied(fe)) {
            if (!fallback.isSet()) {
                fallback = reg;
                fallbackOffset = 0;
            }
            JaegerSpew(JSpew_Regalloc, "    %s has copies\n", reg.name());
            continue;
        }

        if (isTemporary(fe)) {
            /*
             * All temporaries we currently generate are for loop invariants,
             * which we treat as being live everywhere within the loop.
             */
            JS_ASSERT(loop);
            if (!fallback.isSet() || loop->backedgeOffset() > fallbackOffset) {
                fallback = reg;
                fallbackOffset = loop->backedgeOffset();
            }
            JaegerSpew(JSpew_Regalloc, "    %s is a loop temporary\n", reg.name());
            continue;
        }

        /*
         * Any register for an entry dead at this bytecode is fine to evict.
         * We require an entry to be live at the bytecode which kills it.
         * This ensures that if multiple registers are used for the entry
         * (i.e. type and payload), we do not haphazardly evict the first
         * one when allocating the second one.
         */
        Lifetime *lifetime = variableLive(fe, PC);
        if (!lifetime) {
            /*
             * Mark the entry as synced to avoid emitting a store, we don't need
             * to keep this value around.
             */
            if (!fe->data.synced())
                fe->data.sync();
            if (!fe->type.synced())
                fe->type.sync();
            JaegerSpew(JSpew_Regalloc, "result: %s (%s) is dead\n", entryName(fe), reg.name());
            return reg;
        }

        /*
         * Evict variables which are only live in future loop iterations, and are
         * not carried around the loop in a register.
         */
        JS_ASSERT_IF(lifetime->loopTail, loop);
        if (lifetime->loopTail && !loop->carriesLoopReg(fe)) {
            JaegerSpew(JSpew_Regalloc, "result: %s (%s) only live in later iterations\n",
                       entryName(fe), reg.name());
            return reg;
        }

        JaegerSpew(JSpew_Regalloc, "    %s (%s): %u\n", entryName(fe), reg.name(), lifetime->end);

        /*
         * The best live register to evict is the one that will be live for the
         * longest time. This may need tweaking for variables that are used in
         * many places throughout their lifetime. Note that we don't pay attention
         * to whether the register is synced or not --- it is more efficient to
         * have things in registers when they're needed than to emit some extra
         * writes for things that won't be used again for a while.
         */

        if (!fallback.isSet() || lifetime->end > fallbackOffset) {
            fallback = reg;
            fallbackOffset = lifetime->end;
        }
    }

    JS_ASSERT(fallback.isSet());

    JaegerSpew(JSpew_Regalloc, "result %s\n", fallback.name());
    return fallback;
}

AnyRegisterID
FrameState::evictSomeReg(uint32 mask)
{
    if (cx->typeInferenceEnabled()) {
        AnyRegisterID reg = bestEvictReg(mask, false);
        evictReg(reg);
        return reg;
    }

    /* With inference disabled, only general purpose registers are managed. */
    JS_ASSERT((mask & ~Registers::AvailRegs) == 0);

    MaybeRegisterID fallback;

    for (uint32 i = 0; i < JSC::MacroAssembler::TotalRegisters; i++) {
        RegisterID reg = RegisterID(i);

        /* Register is not allocatable, don't bother.  */
        if (!(Registers::maskReg(reg) & mask))
            continue;

        /* Register is not owned by the FrameState. */
        FrameEntry *fe = regstate(reg).fe();
        if (!fe)
            continue;

        /* Try to find a candidate... that doesn't need spilling. */
        fallback = reg;

        if (regstate(reg).type() == RematInfo::TYPE && fe->type.synced()) {
            fe->type.setMemory();
            return fallback.reg();
        }
        if (regstate(reg).type() == RematInfo::DATA && fe->data.synced()) {
            fe->data.setMemory();
            return fallback.reg();
        }
    }

    evictReg(fallback.reg());
    return fallback.reg();
}

void
FrameState::resetInternalState()
{
    for (uint32 i = 0; i < a->tracker.nentries; i++)
        a->tracker[i]->untrack();

    a->tracker.reset();
    a->freeRegs = Registers(Registers::AvailAnyRegs);
}

void
FrameState::discardFrame()
{
    resetInternalState();
    PodArrayZero(a->regstate_);
}

void
FrameState::forgetEverything()
{
    resetInternalState();

#ifdef DEBUG
    for (uint32 i = 0; i < Registers::TotalAnyRegisters; i++) {
        AnyRegisterID reg = AnyRegisterID::fromRaw(i);
        JS_ASSERT(!regstate(reg).usedBy());
    }
#endif
}

#ifdef DEBUG
void
FrameState::dumpAllocation(RegisterAllocation *alloc)
{
    JS_ASSERT(cx->typeInferenceEnabled());
    for (unsigned i = 0; i < Registers::TotalAnyRegisters; i++) {
        AnyRegisterID reg = AnyRegisterID::fromRaw(i);
        if (alloc->assigned(reg)) {
            printf(" (%s: %s%s)", reg.name(), entryName(entries + alloc->slot(reg)),
                   alloc->synced(reg) ? "" : " unsynced");
        }
    }
    Registers regs = alloc->getParentRegs();
    while (!regs.empty()) {
        AnyRegisterID reg = regs.takeAnyReg();
        printf(" (%s: parent)", reg.name());
    }
    printf("\n");
}
#endif

RegisterAllocation *
FrameState::computeAllocation(jsbytecode *target)
{
    JS_ASSERT(cx->typeInferenceEnabled());
    RegisterAllocation *alloc = ArenaNew<RegisterAllocation>(cx->compartment->pool, false);
    if (!alloc)
        return NULL;

    if (analysis->getCode(target).exceptionEntry || analysis->getCode(target).switchTarget ||
        JSOp(*target) == JSOP_TRAP) {
        /* State must be synced at exception and switch targets, and at traps. */
#ifdef DEBUG
        if (IsJaegerSpewChannelActive(JSpew_Regalloc)) {
            JaegerSpew(JSpew_Regalloc, "allocation at %u:", target - script->code);
            dumpAllocation(alloc);
        }
#endif
        return alloc;
    }

    alloc->setParentRegs(a->parentRegs);

    /*
     * The allocation to use at the target consists of all non-stack entries
     * currently in registers which are live at the target.
     */
    Registers regs = Registers::AvailRegs;
    while (!regs.empty()) {
        AnyRegisterID reg = regs.takeAnyReg();
        if (a->freeRegs.hasReg(reg) || regstate(reg).type() == RematInfo::TYPE)
            continue;
        FrameEntry *fe = regstate(reg).fe();
        if (fe == callee_)
            continue;
        if (fe < spBase && !variableLive(fe, target))
            continue;
        if (fe >= spBase && !isTemporary(fe))
            continue;
        if (isTemporary(fe) && uint32(target - script->code) > loop->backedgeOffset())
            continue;
        alloc->set(reg, indexOfFe(fe), fe->data.synced());
    }

#ifdef DEBUG
    if (IsJaegerSpewChannelActive(JSpew_Regalloc)) {
        JaegerSpew(JSpew_Regalloc, "allocation at %u:", target - script->code);
        dumpAllocation(alloc);
    }
#endif

    return alloc;
}

void
FrameState::relocateReg(AnyRegisterID reg, RegisterAllocation *alloc, Uses uses)
{
    JS_ASSERT(cx->typeInferenceEnabled());

    /*
     * The reg needs to be freed to make room for a variable carried across
     * a branch. Either evict its entry, or try to move it to a different
     * register if it is needed to test the branch condition. :XXX: could also
     * watch for variables which are carried across the branch but are in a
     * the register for a different carried entry, we just spill these for now.
     */
    JS_ASSERT(!a->freeRegs.hasReg(reg));

    for (unsigned i = 0; i < uses.nuses; i++) {
        FrameEntry *fe = peek(-1 - i);
        if (fe->isCopy())
            fe = fe->copyOf();
        if (reg.isReg() && fe->data.inRegister() && fe->data.reg() == reg.reg()) {
            pinReg(reg);
            RegisterID nreg = allocReg();
            unpinReg(reg);

            JaegerSpew(JSpew_Regalloc, "relocating %s\n", reg.name());

            masm.move(reg.reg(), nreg);
            regstate(reg).forget();
            regstate(nreg).associate(fe, RematInfo::DATA);
            fe->data.setRegister(nreg);
            a->freeRegs.putReg(reg);
            return;
        }
    }

    JaegerSpew(JSpew_Regalloc, "could not relocate %s\n", reg.name());

    takeReg(reg);
    a->freeRegs.putReg(reg);
}

bool
FrameState::syncForBranch(jsbytecode *target, Uses uses)
{
    /* There should be no unowned or pinned registers. */
#ifdef DEBUG
    Registers checkRegs(Registers::AvailAnyRegs);
    while (!checkRegs.empty()) {
        AnyRegisterID reg = checkRegs.takeAnyReg();
        JS_ASSERT_IF(!a->freeRegs.hasReg(reg), regstate(reg).fe());
    }
#endif

    if (!cx->typeInferenceEnabled()) {
        syncAndForgetEverything();
        return true;
    }

    Registers regs = 0;

    RegisterAllocation *&alloc = analysis->getAllocation(target);
    if (!alloc) {
        alloc = computeAllocation(target);
        if (!alloc)
            return false;
    }

    /*
     * First pass. Sync all entries which will not be carried in a register,
     * and uncopy everything except values used in the branch.
     */

    for (uint32 i = a->tracker.nentries - 1; i < a->tracker.nentries; i--) {
        FrameEntry *fe = a->tracker[i];

        if (deadEntry(fe, uses.nuses)) {
            /* No need to sync, this will get popped before branching. */
            continue;
        }

        /* Force syncs for locals which are dead at the current PC. */
        if (isLocal(fe) && !analysis->slotEscapes(indexOfFe(fe))) {
            Lifetime *lifetime = variableLive(fe, PC);
            if (!lifetime) {
                if (!fe->data.synced())
                    fe->data.sync();
                if (!fe->type.synced())
                    fe->type.sync();
            }
        }

        unsigned index = indexOfFe(fe);
        if (!fe->isCopy() && alloc->hasAnyReg(index)) {
            /* Types are always synced, except for known doubles. */
            if (!fe->isType(JSVAL_TYPE_DOUBLE))
                syncType(fe);
        } else {
            syncFe(fe);
            if (fe->isCopy())
                fe->resetSynced();
        }
    }

    syncParentRegistersInMask(masm, a->parentRegs.freeMask & ~alloc->getParentRegs().freeMask, true);

    /*
     * Second pass. Move entries carried in registers to the right register
     * provided no value used in the branch is evicted. After this pass,
     * everything will either be in the right register or will be in memory.
     */

    regs = Registers(Registers::AvailAnyRegs);
    while (!regs.empty()) {
        AnyRegisterID reg = regs.takeAnyReg();
        if (!alloc->assigned(reg))
            continue;
        FrameEntry *fe = getOrTrack(alloc->slot(reg));
        JS_ASSERT(!fe->isCopy());

        JS_ASSERT_IF(!fe->isType(JSVAL_TYPE_DOUBLE), fe->type.synced());
        if (!fe->data.synced() && alloc->synced(reg))
            syncFe(fe);

        if (fe->dataInRegister(reg))
            continue;

        if (!a->freeRegs.hasReg(reg))
            relocateReg(reg, alloc, uses);

        /*
         * It is possible that the fe is known to be a double currently but is not
         * known to be a double at the join point (it may have non-double values
         * assigned elsewhere in the script). It is *not* possible for the fe to
         * be a non-double currently but a double at the join point --- the Compiler
         * must have called fixDoubleTypes before branching.
         */
        if (reg.isReg() && fe->isType(JSVAL_TYPE_DOUBLE)) {
            syncFe(fe);
            forgetAllRegs(fe);
            fe->resetSynced();
        }
        JS_ASSERT_IF(!reg.isReg(), fe->isType(JSVAL_TYPE_DOUBLE));

        if (reg.isReg()) {
            RegisterID nreg = reg.reg();
            if (fe->data.inMemory()) {
                masm.loadPayload(addressOf(fe), nreg);
            } else if (fe->isConstant()) {
                masm.loadValuePayload(fe->getValue(), nreg);
            } else {
                JS_ASSERT(fe->data.inRegister() && fe->data.reg() != nreg);
                masm.move(fe->data.reg(), nreg);
                a->freeRegs.putReg(fe->data.reg());
                regstate(fe->data.reg()).forget();
            }
            fe->data.setRegister(nreg);
        } else {
            FPRegisterID nreg = reg.fpreg();
            if (fe->data.inMemory()) {
                masm.loadDouble(addressOf(fe), nreg);
            } else if (fe->isConstant()) {
                masm.slowLoadConstantDouble(fe->getValue().toDouble(), nreg);
            } else {
                JS_ASSERT(fe->data.inFPRegister() && fe->data.fpreg() != nreg);
                masm.moveDouble(fe->data.fpreg(), nreg);
                a->freeRegs.putReg(fe->data.fpreg());
                regstate(fe->data.fpreg()).forget();
            }
            fe->data.setFPRegister(nreg);
        }

        a->freeRegs.takeReg(reg);
        regstate(reg).associate(fe, RematInfo::DATA);

        /*
         * If this register is also a parent register at the branch target,
         * we are restoring a parent register we previously evicted.
         */
        if (alloc->getParentRegs().hasReg(reg))
            a->parentRegs.putReg(reg);
    }

    /* Restore any parent registers needed at the branch, evicting those still in use. */
    Registers parents(alloc->getParentRegs().freeMask & ~a->parentRegs.freeMask);
    while (!parents.empty()) {
        AnyRegisterID reg = parents.takeAnyReg();
        if (!a->freeRegs.hasReg(reg))
            relocateReg(reg, alloc, uses);
        a->parentRegs.putReg(reg);
        restoreParentRegister(masm, reg);
    }

    return true;
}

bool
FrameState::discardForJoin(jsbytecode *target, uint32 stackDepth)
{
    if (!cx->typeInferenceEnabled()) {
        resetInternalState();
        PodArrayZero(a->regstate_);
        sp = spBase + stackDepth;
        return true;
    }

    RegisterAllocation *&alloc = analysis->getAllocation(target);

    if (!alloc) {
        /*
         * This shows up for loop entries which are not reachable from the
         * loop head, and for exception, switch target and trap safe points.
         */
        alloc = ArenaNew<RegisterAllocation>(cx->compartment->pool, false);
        if (!alloc)
            return false;
    }

    resetInternalState();
    PodArrayZero(a->regstate_);

    a->parentRegs = alloc->getParentRegs();

    Registers regs(Registers::AvailAnyRegs);
    while (!regs.empty()) {
        AnyRegisterID reg = regs.takeAnyReg();
        if (!alloc->assigned(reg))
            continue;
        FrameEntry *fe = getOrTrack(alloc->slot(reg));

        a->freeRegs.takeReg(reg);

        /*
         * We can't look at the type of the fe as we haven't restored analysis types yet,
         * but if this is an FP reg it will be set to double type.
         */
        if (reg.isReg()) {
            fe->data.setRegister(reg.reg());
        } else {
            fe->setType(JSVAL_TYPE_DOUBLE);
            fe->data.setFPRegister(reg.fpreg());
        }

        regstate(reg).associate(fe, RematInfo::DATA);
        if (!alloc->synced(reg))
            fe->data.unsync();
    }

    sp = spBase + stackDepth;

    for (unsigned i = 0; i < stackDepth; i++)
        a->extraArray[i].reset();

    return true;
}

bool
FrameState::consistentRegisters(jsbytecode *target)
{
    if (!cx->typeInferenceEnabled()) {
        JS_ASSERT(a->freeRegs.freeMask == Registers::AvailAnyRegs);
        return true;
    }

    /*
     * Before calling this, either the entire state should have been synced or
     * syncForBranch should have been called. These will ensure that any FE
     * which is not consistent with the target's register state has already
     * been synced, and no stores will need to be issued by prepareForJump.
     */
    RegisterAllocation *alloc = analysis->getAllocation(target);
    JS_ASSERT(alloc);

    Registers regs(Registers::AvailAnyRegs);
    while (!regs.empty()) {
        AnyRegisterID reg = regs.takeAnyReg();
        if (alloc->assigned(reg)) {
            FrameEntry *needed = getOrTrack(alloc->slot(reg));
            if (!a->freeRegs.hasReg(reg)) {
                FrameEntry *fe = regstate(reg).fe();
                if (fe != needed)
                    return false;
            } else {
                return false;
            }
        }
    }

    if (!a->parentRegs.hasAllRegs(alloc->getParentRegs().freeMask))
        return false;

    return true;
}

void
FrameState::prepareForJump(jsbytecode *target, Assembler &masm, bool synced)
{
    if (!cx->typeInferenceEnabled())
        return;

    JS_ASSERT_IF(!synced, !consistentRegisters(target));

    RegisterAllocation *alloc = analysis->getAllocation(target);
    JS_ASSERT(alloc);

    Registers regs = 0;

    regs = Registers(Registers::AvailAnyRegs);
    while (!regs.empty()) {
        AnyRegisterID reg = regs.takeAnyReg();
        if (!alloc->assigned(reg))
            continue;

        const FrameEntry *fe = getOrTrack(alloc->slot(reg));
        if (synced || !fe->backing()->dataInRegister(reg)) {
            JS_ASSERT_IF(!synced, fe->data.synced());
            if (reg.isReg())
                masm.loadPayload(addressOf(fe), reg.reg());
            else
                masm.loadDouble(addressOf(fe), reg.fpreg());
        }
    }

    regs = Registers(alloc->getParentRegs());
    while (!regs.empty()) {
        AnyRegisterID reg = regs.takeAnyReg();
        if (synced || !a->parentRegs.hasReg(reg))
            restoreParentRegister(masm, reg);
    }
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

    JS_ASSERT(!a->freeRegs.hasReg(address.base));

    /* If loading from memory, ensure destination differs. */
    JS_ASSERT_IF((fe->type.inMemory() || fe->data.inMemory()),
                 addressOf(fe).base != address.base ||
                 addressOf(fe).offset != address.offset);

    if (fe->data.inFPRegister()) {
        masm.storeDouble(fe->data.fpreg(), address);
        return;
    }

    if (fe->isType(JSVAL_TYPE_DOUBLE)) {
        JS_ASSERT(fe->data.inMemory());
        masm.loadDouble(addressOf(fe), Registers::FPConversionTemp);
        masm.storeDouble(Registers::FPConversionTemp, address);
        return;
    }

    /* Don't clobber the address's register. */
    bool pinAddressReg = !!regstate(address.base).fe();
    if (pinAddressReg)
        pinReg(address.base);

#if defined JS_PUNBOX64
    if (fe->type.inMemory() && fe->data.inMemory()) {
        /* Future optimization: track that the Value is in a register. */
        RegisterID vreg = Registers::ValueReg;
        masm.loadPtr(addressOf(fe), vreg);
        masm.storePtr(vreg, address);
        if (pinAddressReg)
            unpinReg(address.base);
        return;
    }

    JS_ASSERT(!fe->isType(JSVAL_TYPE_DOUBLE));

    /*
     * If dreg is obtained via allocReg(), then calling
     * pinReg() trips an assertion. But in all other cases,
     * calling pinReg() is necessary in the fe->type.inMemory() path.
     * Remember whether pinReg() can be safely called.
     */
    bool canPinDreg = true;
    bool wasInRegister = fe->data.inRegister();

    /* Get a register for the payload. */
    MaybeRegisterID dreg;
    if (fe->data.inRegister()) {
        dreg = fe->data.reg();
    } else {
        JS_ASSERT(fe->data.inMemory());
        if (popped) {
            dreg = allocReg();
            masm.loadPayload(addressOf(fe), dreg.reg());
            canPinDreg = false;
        } else {
            dreg = allocAndLoadReg(fe, false, RematInfo::DATA).reg();
            fe->data.setRegister(dreg.reg());
        }
    }
    
    /* Store the Value. */
    if (fe->type.inRegister()) {
        masm.storeValueFromComponents(fe->type.reg(), dreg.reg(), address);
    } else if (fe->isTypeKnown()) {
        masm.storeValueFromComponents(ImmType(fe->getKnownType()), dreg.reg(), address);
    } else {
        JS_ASSERT(fe->type.inMemory());
        if (canPinDreg)
            pinReg(dreg.reg());

        RegisterID treg;
        if (popped) {
            treg = allocReg();
            masm.loadTypeTag(addressOf(fe), treg);
        } else {
            treg = allocAndLoadReg(fe, false, RematInfo::TYPE).reg();
        }
        masm.storeValueFromComponents(treg, dreg.reg(), address);

        if (popped)
            freeReg(treg);
        else
            fe->type.setRegister(treg);

        if (canPinDreg)
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
        RegisterID reg;
        if (popped) {
            reg = allocReg();
            masm.loadPayload(addressOf(fe), reg);
        } else {
            reg = allocAndLoadReg(fe, false, RematInfo::DATA).reg();
        }
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
        RegisterID reg;
        if (popped) {
            reg = allocReg();
            masm.loadTypeTag(addressOf(fe), reg);
        } else {
            reg = allocAndLoadReg(fe, false, RematInfo::TYPE).reg();
        }
        masm.storeTypeTag(reg, address);
        if (popped)
            freeReg(reg);
        else
            fe->type.setRegister(reg);
    }
#endif
    if (pinAddressReg)
        unpinReg(address.base);
}

void
FrameState::loadThisForReturn(RegisterID typeReg, RegisterID dataReg, RegisterID tempReg)
{
    return loadForReturn(getThis(), typeReg, dataReg, tempReg);
}

void FrameState::loadForReturn(FrameEntry *fe, RegisterID typeReg, RegisterID dataReg, RegisterID tempReg)
{
    JS_ASSERT(dataReg != typeReg && dataReg != tempReg && typeReg != tempReg);

    if (fe->isConstant()) {
        masm.loadValueAsComponents(fe->getValue(), typeReg, dataReg);
        return;
    }

    if (fe->isType(JSVAL_TYPE_DOUBLE)) {
        FPRegisterID fpreg = tempFPRegForData(fe);
        masm.breakDouble(fpreg, typeReg, dataReg);
        return;
    }

    if (fe->isCopy())
        fe = fe->copyOf();

    MaybeRegisterID maybeType = maybePinType(fe);
    MaybeRegisterID maybeData = maybePinData(fe);

    if (fe->isTypeKnown()) {
        // If the data is in memory, or in the wrong reg, load/move it.
        if (!maybeData.isSet())
            masm.loadPayload(addressOf(fe), dataReg);
        else if (maybeData.reg() != dataReg)
            masm.move(maybeData.reg(), dataReg);
        masm.move(ImmType(fe->getKnownType()), typeReg);
        return;
    }

    // If both halves of the value are in memory, make this easier and load
    // both pieces into their respective registers.
    if (fe->type.inMemory() && fe->data.inMemory()) {
        masm.loadValueAsComponents(addressOf(fe), typeReg, dataReg);
        return;
    }

    // Now, we should be guaranteed that at least one part is in a register.
    JS_ASSERT(maybeType.isSet() || maybeData.isSet());

    // Make sure we have two registers while making sure not clobber either half.
    // Here we are allowed to mess up the FrameState invariants, because this
    // is specialized code for a path that is about to discard the entire frame.
    if (!maybeType.isSet()) {
        JS_ASSERT(maybeData.isSet());
        if (maybeData.reg() != typeReg)
            maybeType = typeReg;
        else
            maybeType = tempReg;
        masm.loadTypeTag(addressOf(fe), maybeType.reg());
    } else if (!maybeData.isSet()) {
        JS_ASSERT(maybeType.isSet());
        if (maybeType.reg() != dataReg)
            maybeData = dataReg;
        else
            maybeData = tempReg;
        masm.loadPayload(addressOf(fe), maybeData.reg());
    }

    RegisterID type = maybeType.reg();
    RegisterID data = maybeData.reg();

    if (data == typeReg && type == dataReg) {
        masm.move(type, tempReg);
        masm.move(data, dataReg);
        masm.move(tempReg, typeReg);
    } else if (data != dataReg) {
        if (type == typeReg) {
            masm.move(data, dataReg);
        } else if (type != dataReg) {
            masm.move(data, dataReg);
            if (type != typeReg)
                masm.move(type, typeReg);
        } else {
            JS_ASSERT(data != typeReg);
            masm.move(type, typeReg);
            masm.move(data, dataReg);
        }
    } else if (type != typeReg) {
        masm.move(type, typeReg);
    }
}

#ifdef DEBUG
void
FrameState::assertValidRegisterState() const
{
    Registers checkedFreeRegs(Registers::AvailAnyRegs);

    for (uint32 i = 0; i < a->tracker.nentries; i++) {
        FrameEntry *fe = a->tracker[i];
        if (deadEntry(fe))
            continue;

        JS_ASSERT(i == fe->trackerIndex());

        if (fe->isCopy()) {
            JS_ASSERT_IF(!fe->copyOf()->temporary, fe > fe->copyOf());
            JS_ASSERT(fe->trackerIndex() > fe->copyOf()->trackerIndex());
            JS_ASSERT(!deadEntry(fe->copyOf()));
            JS_ASSERT(fe->copyOf()->isCopied());
            continue;
        }

        if (fe->type.inRegister()) {
            checkedFreeRegs.takeReg(fe->type.reg());
            JS_ASSERT(regstate(fe->type.reg()).fe() == fe);
            JS_ASSERT(!fe->isType(JSVAL_TYPE_DOUBLE));
        }
        if (fe->data.inRegister()) {
            checkedFreeRegs.takeReg(fe->data.reg());
            JS_ASSERT(regstate(fe->data.reg()).fe() == fe);
            JS_ASSERT(!fe->isType(JSVAL_TYPE_DOUBLE));
        }
        if (fe->data.inFPRegister()) {
            JS_ASSERT(fe->isType(JSVAL_TYPE_DOUBLE));
            checkedFreeRegs.takeReg(fe->data.fpreg());
            JS_ASSERT(regstate(fe->data.fpreg()).fe() == fe);
        }
    }

    JS_ASSERT(checkedFreeRegs == a->freeRegs);

    for (uint32 i = 0; i < Registers::TotalRegisters; i++) {
        AnyRegisterID reg = (RegisterID) i;
        JS_ASSERT(!regstate(reg).isPinned());
        JS_ASSERT_IF(regstate(reg).fe(), !a->freeRegs.hasReg(reg));
        JS_ASSERT_IF(regstate(reg).fe(), regstate(reg).fe()->isTracked());
    }

    for (uint32 i = 0; i < Registers::TotalFPRegisters; i++) {
        AnyRegisterID reg = (FPRegisterID) i;
        JS_ASSERT(!regstate(reg).isPinned());
        JS_ASSERT_IF(regstate(reg).fe(), !a->freeRegs.hasReg(reg));
        JS_ASSERT_IF(regstate(reg).fe(), regstate(reg).fe()->isTracked());
        JS_ASSERT_IF(regstate(reg).fe(), regstate(reg).type() == RematInfo::DATA);
    }
}
#endif

#if defined JS_NUNBOX32
void
FrameState::syncFancy(Assembler &masm, Registers avail, FrameEntry *resumeAt,
                      FrameEntry *bottom) const
{
    a->reifier.reset(&masm, avail, resumeAt, bottom);

    for (FrameEntry *fe = resumeAt; fe >= bottom; fe--) {
        if (!fe->isTracked())
            continue;

        a->reifier.sync(fe);
    }
}
#endif

void
FrameState::syncParentRegister(Assembler &masm, AnyRegisterID reg) const
{
    ActiveFrame *which = a->parent;
    while (which->freeRegs.hasReg(reg))
        which = which->parent;

    FrameEntry *fe = which->regstate(reg).usedBy();
    Address address = addressOf(fe, which);

    if (reg.isReg() && fe->type.inRegister() && fe->type.reg() == reg.reg()) {
        if (!fe->type.synced())
            masm.storeTypeTag(reg.reg(), address);
    } else if (reg.isReg()) {
        JS_ASSERT(fe->data.inRegister() && fe->data.reg() == reg.reg());
        if (!fe->data.synced())
            masm.storePayload(reg.reg(), address);
    } else {
        JS_ASSERT(fe->data.inFPRegister() && fe->data.fpreg() == reg.fpreg());
        if (!fe->data.synced())
            masm.storeDouble(reg.fpreg(), address);
    }
}

void
FrameState::syncParentRegistersInMask(Assembler &masm, uint32 mask, bool update) const
{
    JS_ASSERT((a->parentRegs.freeMask & mask) == mask);

    Registers parents(mask);
    while (!parents.empty()) {
        AnyRegisterID reg = parents.takeAnyReg();
        if (update)
            a->parentRegs.takeReg(reg);
        syncParentRegister(masm, reg);
    }
}

void
FrameState::sync(Assembler &masm, Uses uses) const
{
    syncParentRegistersInMask(masm, a->parentRegs.freeMask, false);

    if (!entries)
        return;

    /* Sync all registers up-front. */
    Registers allRegs(Registers::AvailAnyRegs);
    while (!allRegs.empty()) {
        AnyRegisterID reg = allRegs.takeAnyReg();
        FrameEntry *fe = regstate(reg).usedBy();
        if (!fe)
            continue;

        JS_ASSERT(fe->isTracked());

#if defined JS_PUNBOX64
        /* Sync entire FE to prevent loads. */
        ensureFeSynced(fe, masm);

        /* Take the other register in the pair, if one exists. */
        if (regstate(reg).type() == RematInfo::DATA && fe->type.inRegister())
            allRegs.takeReg(fe->type.reg());
        else if (regstate(reg).type() == RematInfo::TYPE && fe->data.inRegister())
            allRegs.takeReg(fe->data.reg());
#elif defined JS_NUNBOX32
        /* Sync register if unsynced. */
        if (fe->isType(JSVAL_TYPE_DOUBLE)) {
            ensureFeSynced(fe, masm);
        } else if (regstate(reg).type() == RematInfo::DATA) {
            JS_ASSERT(fe->data.reg() == reg.reg());
            ensureDataSynced(fe, masm);
        } else {
            JS_ASSERT(fe->type.reg() == reg.reg());
            ensureTypeSynced(fe, masm);
        }
#endif
    }

    /*
     * Keep track of free registers using a bitmask. If we have to drop into
     * syncFancy(), then this mask will help avoid eviction.
     */
    Registers avail(a->freeRegs.freeMask & Registers::AvailRegs);
    Registers temp(Registers::TempAnyRegs);

    FrameEntry *bottom = cx->typeInferenceEnabled() ? entries : sp - uses.nuses;

    for (FrameEntry *fe = sp - 1; fe >= bottom; fe--) {
        if (!fe->isTracked())
            continue;

        if (fe->isType(JSVAL_TYPE_DOUBLE)) {
            /* Copies of in-memory doubles can be synced without spilling. */
            ensureFeSynced(fe, masm);
            continue;
        }

        FrameEntry *backing = fe;

        if (!fe->isCopy()) {
            if (fe->data.inRegister())
                avail.putReg(fe->data.reg());
            if (fe->type.inRegister())
                avail.putReg(fe->type.reg());
        } else {
            backing = fe->copyOf();
            JS_ASSERT(!backing->isConstant() && !fe->isConstant());

#if defined JS_PUNBOX64
            if ((!fe->type.synced() && backing->type.inMemory()) ||
                (!fe->data.synced() && backing->data.inMemory())) {
    
                RegisterID syncReg = Registers::ValueReg;

                /* Load the entire Value into syncReg. */
                if (backing->type.synced() && backing->data.synced()) {
                    masm.loadValue(addressOf(backing), syncReg);
                } else if (backing->type.inMemory()) {
                    masm.loadTypeTag(addressOf(backing), syncReg);
                    masm.orPtr(backing->data.reg(), syncReg);
                } else {
                    JS_ASSERT(backing->data.inMemory());
                    masm.loadPayload(addressOf(backing), syncReg);
                    if (backing->isTypeKnown())
                        masm.orPtr(ImmType(backing->getKnownType()), syncReg);
                    else
                        masm.orPtr(backing->type.reg(), syncReg);
                }

                masm.storeValue(syncReg, addressOf(fe));
                continue;
            }
#elif defined JS_NUNBOX32
            /* Fall back to a slower sync algorithm if load required. */
            if ((!fe->type.synced() && backing->type.inMemory()) ||
                (!fe->data.synced() && backing->data.inMemory())) {
                syncFancy(masm, avail, fe, bottom);
                return;
            }
#endif
        }

        bool copy = fe->isCopy();

        /* If a part still needs syncing, it is either a copy or constant. */
#if defined JS_PUNBOX64
        /* All register-backed FEs have been entirely synced up-front. */
        if (copy || (!fe->type.inRegister() && !fe->data.inRegister()))
            ensureFeSynced(fe, masm);
#elif defined JS_NUNBOX32
        /* All components held in registers have been already synced. */
        if (copy || !fe->data.inRegister())
            ensureDataSynced(fe, masm);
        if (copy || !fe->type.inRegister())
            ensureTypeSynced(fe, masm);
#endif
    }
}

void
FrameState::syncAndKill(Registers kill, Uses uses, Uses ignore)
{
    syncParentRegistersInMask(masm, a->parentRegs.freeMask, true);
    JS_ASSERT(a->parentRegs.empty());

    if (loop) {
        /*
         * Drop any remaining loop registers so we don't do any more after-the-fact
         * allocation of the initial register state.
         */
        loop->clearLoopRegisters();
    }

    /* Sync all kill-registers up-front. */
    Registers search(kill.freeMask & ~a->freeRegs.freeMask);
    while (!search.empty()) {
        AnyRegisterID reg = search.takeAnyReg();
        FrameEntry *fe = regstate(reg).usedBy();
        if (!fe || deadEntry(fe, ignore.nuses))
            continue;

        JS_ASSERT(fe->isTracked());

#if defined JS_PUNBOX64
        /* Don't use syncFe(), since that may clobber more registers. */
        ensureFeSynced(fe, masm);

        if (!fe->type.synced())
            fe->type.sync();
        if (!fe->data.synced())
            fe->data.sync();

        /* Take the other register in the pair, if one exists. */
        if (regstate(reg).type() == RematInfo::DATA) {
            if (!fe->isType(JSVAL_TYPE_DOUBLE)) {
                JS_ASSERT(fe->data.reg() == reg.reg());
                if (fe->type.inRegister() && search.hasReg(fe->type.reg()))
                    search.takeReg(fe->type.reg());
            }
        } else {
            JS_ASSERT(fe->type.reg() == reg.reg());
            if (fe->data.inRegister() && search.hasReg(fe->data.reg()))
                search.takeReg(fe->data.reg());
        }
#elif defined JS_NUNBOX32
        /* Sync this register. */
        if (fe->isType(JSVAL_TYPE_DOUBLE)) {
            syncFe(fe);
        } else if (regstate(reg).type() == RematInfo::DATA) {
            JS_ASSERT(fe->data.reg() == reg.reg());
            syncData(fe);
        } else {
            JS_ASSERT(fe->type.reg() == reg.reg());
            syncType(fe);
        }
#endif
    }

    uint32 maxvisits = a->tracker.nentries;
    FrameEntry *bottom = cx->typeInferenceEnabled() ? entries : sp - uses.nuses;

    for (FrameEntry *fe = sp - 1; fe >= bottom && maxvisits; fe--) {
        if (!fe->isTracked())
            continue;

        maxvisits--;

        if (deadEntry(fe, ignore.nuses))
            continue;

        syncFe(fe);

        if (fe->isCopy())
            continue;

        /* Forget registers. */
        if (fe->data.inRegister() && !regstate(fe->data.reg()).isPinned()) {
            forgetReg(fe->data.reg());
            fe->data.setMemory();
        }
        if (fe->data.inFPRegister() && !regstate(fe->data.fpreg()).isPinned()) {
            forgetReg(fe->data.fpreg());
            fe->data.setMemory();
        }
        if (fe->type.inRegister() && !regstate(fe->type.reg()).isPinned()) {
            forgetReg(fe->type.reg());
            fe->type.setMemory();
        }
    }

    /*
     * Anything still alive at this point is guaranteed to be synced. However,
     * it is necessary to evict temporary registers.
     */
    search = Registers(kill.freeMask & ~a->freeRegs.freeMask);
    while (!search.empty()) {
        AnyRegisterID reg = search.takeAnyReg();
        FrameEntry *fe = regstate(reg).usedBy();
        if (!fe || deadEntry(fe, ignore.nuses))
            continue;

        JS_ASSERT(fe->isTracked() && !fe->isType(JSVAL_TYPE_DOUBLE));

        if (regstate(reg).type() == RematInfo::DATA) {
            JS_ASSERT(fe->data.reg() == reg.reg());
            JS_ASSERT(fe->data.synced());
            fe->data.setMemory();
        } else {
            JS_ASSERT(fe->type.reg() == reg.reg());
            JS_ASSERT(fe->type.synced());
            fe->type.setMemory();
        }

        forgetReg(reg);
    }
}

void
FrameState::restoreParentRegister(Assembler &masm, AnyRegisterID reg) const
{
    ActiveFrame *which = a->parent;
    while (which->freeRegs.hasReg(reg))
        which = which->parent;

    FrameEntry *fe = which->regstate(reg).usedBy();
    Address address = addressOf(fe, which);

    if (reg.isReg() && fe->type.inRegister() && fe->type.reg() == reg.reg()) {
        masm.loadTypeTag(address, reg.reg());
    } else if (reg.isReg()) {
        JS_ASSERT(fe->data.inRegister() && fe->data.reg() == reg.reg());
        masm.loadPayload(address, reg.reg());
    } else {
        JS_ASSERT(fe->data.inFPRegister() && fe->data.fpreg() == reg.fpreg());
        masm.loadDouble(address, reg.fpreg());
    }
}

void
FrameState::restoreParentRegistersInMask(Assembler &masm, uint32 mask, bool update) const
{
    JS_ASSERT_IF(update, (a->parentRegs.freeMask & mask) == 0);

    Registers parents(mask);
    while (!parents.empty()) {
        AnyRegisterID reg = parents.takeAnyReg();
        if (update) {
            JS_ASSERT(a->freeRegs.hasReg(reg));
            a->parentRegs.putReg(reg);
        }
        restoreParentRegister(masm, reg);
    }
}

void
FrameState::merge(Assembler &masm, Changes changes) const
{
    /*
     * Note: this should only be called by StubCompiler::rejoin, which will notify
     * this FrameState about the jump to patch up in case a new loop register is
     * allocated later.
     */

    restoreParentRegistersInMask(masm, a->parentRegs.freeMask, false);

    /*
     * For any changed values we are merging back which we consider to be doubles,
     * ensure they actually are doubles.  They must be doubles or ints, but we
     * do not require stub paths to always generate a double when needed.
     * :FIXME: we check this on OOL stub calls, but not inline stub calls.
     */
    for (unsigned i = 0; i < changes.nchanges; i++) {
        FrameEntry *fe = sp - 1 - i;
        if (fe->isTracked() && fe->isType(JSVAL_TYPE_DOUBLE))
            masm.ensureInMemoryDouble(addressOf(fe));
    }

    uint32 mask = Registers::AvailAnyRegs & ~a->freeRegs.freeMask;
    Registers search(mask);

    while (!search.empty(mask)) {
        AnyRegisterID reg = search.peekReg(mask);
        FrameEntry *fe = regstate(reg).usedBy();

        if (!fe) {
            search.takeReg(reg);
            continue;
        }

        if (fe->isType(JSVAL_TYPE_DOUBLE)) {
            JS_ASSERT(fe->data.fpreg() == reg.fpreg());
            search.takeReg(fe->data.fpreg());
            masm.loadDouble(addressOf(fe), fe->data.fpreg());
        } else if (fe->data.inRegister() && fe->type.inRegister()) {
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
    JS_ASSERT(!fe->isConstant());
    JS_ASSERT(!fe->isType(JSVAL_TYPE_DOUBLE));

    if (fe->isCopy())
        fe = fe->copyOf();

    if (!fe->data.inRegister())
        tempRegForData(fe);

    RegisterID reg = fe->data.reg();
    if (reg == hint) {
        if (a->freeRegs.empty(Registers::AvailRegs)) {
            ensureDataSynced(fe, masm);
            fe->data.setMemory();
        } else {
            reg = allocReg();
            masm.move(hint, reg);
            fe->data.setRegister(reg);
            regstate(reg).associate(regstate(hint).fe(), RematInfo::DATA);
        }
        regstate(hint).forget();
    } else {
        pinReg(reg);
        takeReg(hint);
        unpinReg(reg);
        masm.move(reg, hint);
    }

    modifyReg(hint);
}

JSC::MacroAssembler::RegisterID
FrameState::copyDataIntoReg(Assembler &masm, FrameEntry *fe)
{
    JS_ASSERT(!fe->isConstant());

    if (fe->isCopy())
        fe = fe->copyOf();

    if (fe->data.inRegister()) {
        RegisterID reg = fe->data.reg();
        if (a->freeRegs.empty(Registers::AvailRegs)) {
            ensureDataSynced(fe, masm);
            fe->data.setMemory();
            regstate(reg).forget();
            modifyReg(reg);
        } else {
            RegisterID newReg = allocReg();
            masm.move(reg, newReg);
            reg = newReg;
        }
        return reg;
    }

    RegisterID reg = allocReg();

    if (!a->freeRegs.empty(Registers::AvailRegs))
        masm.move(tempRegForData(fe), reg);
    else
        masm.loadPayload(addressOf(fe),reg);

    return reg;
}

JSC::MacroAssembler::RegisterID
FrameState::copyTypeIntoReg(FrameEntry *fe)
{
    if (fe->isCopy())
        fe = fe->copyOf();

    JS_ASSERT(!fe->type.isConstant());

    if (fe->type.inRegister()) {
        RegisterID reg = fe->type.reg();
        if (a->freeRegs.empty(Registers::AvailRegs)) {
            ensureTypeSynced(fe, masm);
            fe->type.setMemory();
            regstate(reg).forget();
            modifyReg(reg);
        } else {
            RegisterID newReg = allocReg();
            masm.move(reg, newReg);
            reg = newReg;
        }
        return reg;
    }

    RegisterID reg = allocReg();

    if (!a->freeRegs.empty(Registers::AvailRegs))
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

JSC::MacroAssembler::RegisterID
FrameState::ownRegForType(FrameEntry *fe)
{
    JS_ASSERT(!fe->isTypeKnown());

    RegisterID reg;
    if (fe->isCopy()) {
        /* For now, just do an extra move. The reg must be mutable. */
        FrameEntry *backing = fe->copyOf();
        if (!backing->type.inRegister()) {
            JS_ASSERT(backing->type.inMemory());
            tempRegForType(backing);
        }

        if (a->freeRegs.empty(Registers::AvailRegs)) {
            /* For now... just steal the register that already exists. */
            ensureTypeSynced(backing, masm);
            reg = backing->type.reg();
            backing->type.setMemory();
            regstate(reg).forget();
            modifyReg(reg);
        } else {
            reg = allocReg();
            masm.move(backing->type.reg(), reg);
        }
        return reg;
    }

    if (fe->type.inRegister()) {
        reg = fe->type.reg();

        /* Remove ownership of this register. */
        JS_ASSERT(regstate(reg).fe() == fe);
        JS_ASSERT(regstate(reg).type() == RematInfo::TYPE);
        regstate(reg).forget();
        fe->type.setMemory();
        modifyReg(reg);
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
    JS_ASSERT(!fe->isConstant());
    JS_ASSERT(!fe->isType(JSVAL_TYPE_DOUBLE));

    RegisterID reg;
    if (fe->isCopy()) {
        /* For now, just do an extra move. The reg must be mutable. */
        FrameEntry *backing = fe->copyOf();
        if (!backing->data.inRegister()) {
            JS_ASSERT(backing->data.inMemory());
            tempRegForData(backing);
        }

        if (a->freeRegs.empty(Registers::AvailRegs)) {
            /* For now... just steal the register that already exists. */
            ensureDataSynced(backing, masm);
            reg = backing->data.reg();
            backing->data.setMemory();
            regstate(reg).forget();
            modifyReg(reg);
        } else {
            reg = allocReg();
            masm.move(backing->data.reg(), reg);
        }
        return reg;
    }

    if (fe->isCopied()) {
        FrameEntry *copy = uncopy(fe);
        if (fe->isCopied()) {
            fe->resetSynced();
            return copyDataIntoReg(copy);
        }
    }

    if (fe->data.inRegister()) {
        reg = fe->data.reg();
        /* Remove ownership of this register. */
        JS_ASSERT(regstate(reg).fe() == fe);
        JS_ASSERT(regstate(reg).type() == RematInfo::DATA);
        regstate(reg).forget();
        fe->data.setMemory();
        modifyReg(reg);
    } else {
        JS_ASSERT(fe->data.inMemory());
        reg = allocReg();
        masm.loadPayload(addressOf(fe), reg);
    }
    return reg;
}

void
FrameState::discardFe(FrameEntry *fe)
{
    forgetEntry(fe);
    fe->type.setMemory();
    fe->data.setMemory();
    fe->clear();
}

void
FrameState::pushDouble(FPRegisterID fpreg)
{
    FrameEntry *fe = rawPush();
    fe->resetUnsynced();
    fe->setType(JSVAL_TYPE_DOUBLE);
    fe->data.setFPRegister(fpreg);
    regstate(fpreg).associate(fe, RematInfo::DATA);
}

void
FrameState::pushDouble(Address address)
{
    FPRegisterID fpreg = allocFPReg();
    masm.loadDouble(address, fpreg);
    pushDouble(fpreg);
}

void
FrameState::ensureDouble(FrameEntry *fe)
{
    if (fe->isType(JSVAL_TYPE_DOUBLE))
        return;

    if (fe->isConstant()) {
        JS_ASSERT(fe->getValue().isInt32());
        Value newValue = DoubleValue(double(fe->getValue().toInt32()));
        fe->setConstant(Jsvalify(newValue));
        return;
    }

    FrameEntry *backing = fe;
    if (fe->isCopy()) {
        /* Forget this entry is a copy.  We are converting this entry, not the backing. */
        backing = fe->copyOf();
        fe->clear();
    } else if (fe->isCopied()) {
        /* Sync and forget any copies of this entry. */
        for (uint32 i = fe->trackerIndex() + 1; i < a->tracker.nentries; i++) {
            FrameEntry *nfe = a->tracker[i];
            if (!deadEntry(nfe) && nfe->isCopy() && nfe->copyOf() == fe) {
                syncFe(nfe);
                nfe->resetSynced();
            }
        }
    }

    FPRegisterID fpreg = allocFPReg();

    if (backing->isType(JSVAL_TYPE_INT32)) {
        RegisterID data = tempRegForData(backing);
        masm.convertInt32ToDouble(data, fpreg);
    } else {
        syncFe(backing);
        masm.moveInt32OrDouble(addressOf(backing), fpreg);
    }

    if (fe == backing)
        forgetAllRegs(fe);
    fe->resetUnsynced();
    fe->setType(JSVAL_TYPE_DOUBLE);
    fe->data.setFPRegister(fpreg);
    regstate(fpreg).associate(fe, RematInfo::DATA);

    fe->data.unsync();
    fe->type.unsync();
}

void
FrameState::ensureInteger(FrameEntry *fe)
{
    /*
     * This method is used to revert a previous ensureDouble call made for a
     * branch. The entry is definitely a double, and has had no copies made.
     */

    if (fe->isConstant()) {
        Value newValue = Int32Value(int32(fe->getValue().toDouble()));
        fe->setConstant(Jsvalify(newValue));
        return;
    }

    JS_ASSERT(!fe->isCopy() && !fe->isCopied());

    if (!fe->isType(JSVAL_TYPE_DOUBLE)) {
        /*
         * A normal register may have been allocated after calling
         * syncAndForgetEverything.
         */
        if (fe->data.inRegister()) {
            syncFe(fe);
            forgetReg(fe->data.reg());
            fe->data.setMemory();
        }
        learnType(fe, JSVAL_TYPE_DOUBLE, false);
    }

    RegisterID reg = allocReg();
    FPRegisterID fpreg = tempFPRegForData(fe);
    Jump j = masm.branchTruncateDoubleToInt32(fpreg, reg);
    j.linkTo(masm.label(), &masm);

    forgetAllRegs(fe);
    fe->resetUnsynced();
    fe->setType(JSVAL_TYPE_INT32);
    fe->data.setRegister(reg);
    regstate(reg).associate(fe, RematInfo::DATA);

    fe->data.unsync();
    fe->type.unsync();
}

void
FrameState::ensureInMemoryDoubles(Assembler &masm)
{
    JS_ASSERT(!a->parent);
    for (uint32 i = 0; i < a->tracker.nentries; i++) {
        FrameEntry *fe = a->tracker[i];
        if (!deadEntry(fe) && fe->isType(JSVAL_TYPE_DOUBLE) &&
            !fe->isCopy() && !fe->isConstant()) {
            masm.ensureInMemoryDouble(addressOf(fe));
        }
    }
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
        fe->type.invalidate();
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
    /* Temporary entries are immutable and should never be uncopied. */
    JS_ASSERT(!isTemporary(original));

    uint32 firstCopy = InvalidIndex;
    FrameEntry *bestFe = NULL;
    uint32 ncopies = 0;
    for (uint32 i = original->trackerIndex() + 1; i < a->tracker.nentries; i++) {
        FrameEntry *fe = a->tracker[i];
        if (deadEntry(fe))
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
        for (uint32 i = firstCopy; i < a->tracker.nentries; i++) {
            FrameEntry *other = a->tracker[i];
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
    uint32 maxvisits = a->tracker.nentries;

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
    if ((a->tracker.nentries - original->trackerIndex()) * 2 > uint32(sp - original))
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
            regstate(fe->type.reg()).reassociate(fe);
    } else {
        fe->setType(original->getKnownType());
    }
    if (original->isType(JSVAL_TYPE_DOUBLE)) {
        if (original->data.inMemory() && !fe->data.synced())
            tempFPRegForData(original);
        fe->data.inherit(original->data);
        if (fe->data.inFPRegister())
            regstate(fe->data.fpreg()).reassociate(fe);
    } else {
        if (fe->type.inRegister())
            pinReg(fe->type.reg());
        if (original->data.inMemory() && !fe->data.synced())
            tempRegForData(original);
        if (fe->type.inRegister())
            unpinReg(fe->type.reg());
        fe->data.inherit(original->data);
        if (fe->data.inRegister())
            regstate(fe->data.reg()).reassociate(fe);
    }

    return fe;
}

bool
FrameState::hasOnlyCopy(FrameEntry *backing, FrameEntry *fe)
{
    JS_ASSERT(backing->isCopied() && fe->copyOf() == backing);

    for (uint32 i = backing->trackerIndex() + 1; i < a->tracker.nentries; i++) {
        FrameEntry *nfe = a->tracker[i];
        if (nfe != fe && !deadEntry(nfe) && nfe->isCopy() && nfe->copyOf() == backing)
            return false;
    }

    return true;
}

void
FrameState::separateBinaryEntries(FrameEntry *lhs, FrameEntry *rhs)
{
    JS_ASSERT(lhs == sp - 2 && rhs == sp - 1);
    if (rhs->isCopy() && rhs->copyOf() == lhs) {
        syncAndForgetFe(rhs);
        syncAndForgetFe(lhs);
        uncopy(lhs);
    }
}

void
FrameState::storeLocal(uint32 n, bool popGuaranteed, bool fixedType)
{
    FrameEntry *local = getLocal(n);

    if (analysis->slotEscapes(indexOfFe(local))) {
        JS_ASSERT(local->data.inMemory());
        storeTo(peek(-1), addressOf(local), popGuaranteed);
        return;
    }

    storeTop(local, popGuaranteed);

    if (loop)
        local->lastLoop = loop->headOffset();

    if (inTryBlock)
        syncFe(local);
}

void
FrameState::storeArg(uint32 n, bool popGuaranteed)
{
    // Note that args are always immediately synced, because they can be
    // aliased (but not written to) via f.arguments.
    FrameEntry *arg = getArg(n);

    if (analysis->slotEscapes(indexOfFe(arg))) {
        JS_ASSERT(arg->data.inMemory());
        storeTo(peek(-1), addressOf(arg), popGuaranteed);
        return;
    }

    storeTop(arg, popGuaranteed);

    if (loop)
        arg->lastLoop = loop->headOffset();

    syncFe(arg);
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

    if (fe >= spBase && fe < sp)
        a->extraArray[fe - spBase].reset();
}

void
FrameState::storeTop(FrameEntry *target, bool popGuaranteed)
{
    JS_ASSERT(!isTemporary(target));

    /* Detect something like (x = x) which is a no-op. */
    FrameEntry *top = peek(-1);
    if (top->isCopy() && top->copyOf() == target) {
        JS_ASSERT(target->isCopied());
        return;
    }

    /*
     * If this is overwriting a known non-double type with another value of the
     * same type, then make sure we keep the type marked as synced after doing
     * the copy.
     */
    bool wasSynced = target->type.synced();
    JSValueType oldType = target->isTypeKnown() ? target->getKnownType() : JSVAL_TYPE_UNKNOWN;
    bool trySyncType = wasSynced && oldType != JSVAL_TYPE_UNKNOWN && oldType != JSVAL_TYPE_DOUBLE;

    /* Completely invalidate the local variable. */
    forgetEntry(target);
    target->resetUnsynced();

    /* Constants are easy to propagate. */
    if (top->isConstant()) {
        target->setCopyOf(NULL);
        target->setNotCopied();
        target->setConstant(Jsvalify(top->getValue()));
        if (trySyncType && target->isType(oldType))
            target->type.sync();
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
            if (trySyncType && target->isType(oldType))
                target->type.sync();
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
        for (uint32 i = backing->trackerIndex() + 1; i < a->tracker.nentries; i++) {
            FrameEntry *fe = a->tracker[i];
            if (deadEntry(fe))
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

    if (backing->isType(JSVAL_TYPE_DOUBLE)) {
        FPRegisterID fpreg = tempFPRegForData(backing);
        target->setType(JSVAL_TYPE_DOUBLE);
        target->data.setFPRegister(fpreg);
        regstate(fpreg).reassociate(target);
    } else {
        /*
         * Move the backing store down - we spill registers here, but we could be
         * smarter and re-use the type reg. If we need registers for both the type
         * and data in the backing, make sure we keep the other components pinned.
         * There is nothing else to keep us from evicting the backing's registers.
         */
        if (backing->type.inRegister())
            pinReg(backing->type.reg());
        RegisterID reg = tempRegForData(backing);
        if (backing->type.inRegister())
            unpinReg(backing->type.reg());
        target->data.setRegister(reg);
        regstate(reg).reassociate(target);

        if (backing->isTypeKnown()) {
            target->setType(backing->getKnownType());
        } else {
            pinReg(reg);
            RegisterID typeReg = tempRegForType(backing);
            unpinReg(reg);
            target->type.setRegister(typeReg);
            regstate(typeReg).reassociate(target);
        }
    }

    backing->setCopyOf(target);
    JS_ASSERT(top->copyOf() == target);

    if (trySyncType && target->isType(oldType))
        target->type.sync();

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
    storeTop(peek(depth - 1), true);
    popn(n);
}

void
FrameState::shift(int32 n)
{
    JS_ASSERT(n < 0);
    JS_ASSERT(sp + n - 1 >= spBase);
    storeTop(peek(n - 1), true);
    pop();
}

void
FrameState::forgetKnownDouble(FrameEntry *fe)
{
    /*
     * Forget all information indicating fe is a double, so we can use GPRs for its
     * contents.  We currently need to do this in order to use the entry in MICs/PICs
     * or to construct its ValueRemat. :FIXME: this needs to get fixed.
     */
    JS_ASSERT(!fe->isConstant() && fe->isType(JSVAL_TYPE_DOUBLE));

    RegisterID typeReg = allocReg();
    RegisterID dataReg = allocReg();

    /* Copy into a different FP register, as breakDouble can modify fpreg. */
    FPRegisterID fpreg = allocFPReg();
    masm.moveDouble(tempFPRegForData(fe), fpreg);
    masm.breakDouble(fpreg, typeReg, dataReg);

    forgetAllRegs(fe);
    fe->resetUnsynced();
    fe->clear();

    regstate(typeReg).associate(fe, RematInfo::TYPE);
    regstate(dataReg).associate(fe, RematInfo::DATA);
    fe->type.setRegister(typeReg);
    fe->data.setRegister(dataReg);
    freeReg(fpreg);
}

void
FrameState::pinEntry(FrameEntry *fe, ValueRemat &vr)
{
    if (fe->isConstant()) {
        vr = ValueRemat::FromConstant(fe->getValue());
    } else {
        if (fe->isType(JSVAL_TYPE_DOUBLE))
            forgetKnownDouble(fe);

        // Pin the type register so it can't spill.
        MaybeRegisterID maybePinnedType = maybePinType(fe);

        // Get and pin the data register.
        RegisterID dataReg = tempRegForData(fe);
        pinReg(dataReg);

        if (fe->isTypeKnown()) {
            vr = ValueRemat::FromKnownType(fe->getKnownType(), dataReg);
        } else {
            // The type might not be loaded yet, so unpin for simplicity.
            maybeUnpinReg(maybePinnedType);

            vr = ValueRemat::FromRegisters(tempRegForType(fe), dataReg);
            pinReg(vr.typeReg());
        }
    }

    // Set these bits last, since allocation could have caused a sync.
    vr.isDataSynced = fe->data.synced();
    vr.isTypeSynced = fe->type.synced();
}

void
FrameState::unpinEntry(const ValueRemat &vr)
{
    if (!vr.isConstant()) {
        if (!vr.isTypeKnown())
            unpinReg(vr.typeReg());
        unpinReg(vr.dataReg());
    }
}

void
FrameState::ensureValueSynced(Assembler &masm, FrameEntry *fe, const ValueRemat &vr)
{
#if defined JS_PUNBOX64
    if (!vr.isDataSynced || !vr.isTypeSynced)
        masm.storeValue(vr, addressOf(fe));
#elif defined JS_NUNBOX32
    if (vr.isConstant()) {
        if (!vr.isDataSynced || !vr.isTypeSynced)
            masm.storeValue(vr.value(), addressOf(fe));
    } else {
        if (!vr.isDataSynced)
            masm.storePayload(vr.dataReg(), addressOf(fe));
        if (!vr.isTypeSynced) {
            if (vr.isTypeKnown())
                masm.storeTypeTag(ImmType(vr.knownType()), addressOf(fe));
            else
                masm.storeTypeTag(vr.typeReg(), addressOf(fe));
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
    alloc.rhsNeedsRemat = false;

    if (!fe->isTypeKnown()) {
        alloc.lhsType = tempRegForType(fe);
        pinReg(alloc.lhsType.reg());
    }

    alloc.lhsData = tempRegForData(fe);

    if (!a->freeRegs.empty(Registers::AvailRegs)) {
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

    alloc.lhsFP = alloc.rhsFP = allocFPReg();
}

void
FrameState::ensureFullRegs(FrameEntry *fe, MaybeRegisterID *type, MaybeRegisterID *data)
{
    fe = fe->isCopy() ? fe->copyOf() : fe;

    JS_ASSERT(!data->isSet() && !type->isSet());
    if (!fe->type.inMemory()) {
        if (fe->type.inRegister())
            *type = fe->type.reg();
        if (fe->data.isConstant())
            return;
        if (fe->data.inRegister()) {
            *data = fe->data.reg();
            return;
        }
        if (fe->type.inRegister())
            pinReg(fe->type.reg());
        *data = tempRegForData(fe);
        if (fe->type.inRegister())
            unpinReg(fe->type.reg());
    } else if (!fe->data.inMemory()) {
        if (fe->data.inRegister())
            *data = fe->data.reg();
        if (fe->type.isConstant())
            return;
        if (fe->type.inRegister()) {
            *type = fe->type.reg();
            return;
        }
        if (fe->data.inRegister())
            pinReg(fe->data.reg());
        *type = tempRegForType(fe);
        if (fe->data.inRegister())
            unpinReg(fe->data.reg());
    } else {
        *data = tempRegForData(fe);
        pinReg(data->reg());
        *type = tempRegForType(fe);
        unpinReg(data->reg());
    }
}

inline bool
FrameState::binaryEntryLive(FrameEntry *fe) const
{
    /*
     * Compute whether fe is live after the binary operation performed at the current
     * bytecode. This is similar to variableLive except that it returns false for the
     * top two stack entries and special cases LOCALINC/ARGINC and friends, which fuse
     * a binary operation before writing over the local/arg.
     */
    JS_ASSERT(cx->typeInferenceEnabled());

    if (deadEntry(fe, 2))
        return false;

    switch (JSOp(*PC)) {
      case JSOP_INCLOCAL:
      case JSOP_DECLOCAL:
      case JSOP_LOCALINC:
      case JSOP_LOCALDEC:
        if (fe - locals == (int) GET_SLOTNO(PC))
            return false;
      case JSOP_INCARG:
      case JSOP_DECARG:
      case JSOP_ARGINC:
      case JSOP_ARGDEC:
        if (fe - args == (int) GET_SLOTNO(PC))
            return false;
      default:;
    }

    JS_ASSERT(fe != callee_);

    /* Caller must check that no copies are invalidated by rewriting the entry. */
    return fe >= spBase || variableLive(fe, PC);
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

    /*
     * Allocate floating point registers.  These are temporaries with no pre-existing data;
     * floating point registers are only allocated for known doubles, and BinaryAlloc is not
     * used for such operations.
     */
    JS_ASSERT(!backingLeft->isType(JSVAL_TYPE_DOUBLE));
    JS_ASSERT(!backingRight->isType(JSVAL_TYPE_DOUBLE));
    alloc.lhsFP = allocFPReg();
    alloc.rhsFP = allocFPReg();

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
    alloc.resultHasRhs = false;
    alloc.undoResult = false;

    if (!needsResult)
        goto skip;

    /*
     * Now a result register is needed. It must contain a mutable copy of the
     * LHS. For commutative operations, we can opt to use the RHS instead. At
     * this point, if for some reason either must be in a register, that has
     * already been guaranteed at this point.
     */

    /*
     * Try to reuse operand registers without syncing for ADD and constant SUB,
     * so long as the backing for the operand is dead.
     */
    if (cx->typeInferenceEnabled() &&
        backingLeft->data.inRegister() && !binaryEntryLive(backingLeft) &&
        (op == JSOP_ADD || (op == JSOP_SUB && backingRight->isConstant())) &&
        (lhs == backingLeft || hasOnlyCopy(backingLeft, lhs))) {
        alloc.result = backingLeft->data.reg();
        alloc.undoResult = true;
        alloc.resultHasRhs = false;
        goto skip;
    }

    if (!a->freeRegs.empty(Registers::AvailRegs)) {
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
    } else if (cx->typeInferenceEnabled()) {
        /* No free regs. Evict a register or reuse one of the operands. */
        bool leftInReg = backingLeft->data.inRegister();
        bool rightInReg = backingRight->data.inRegister();

        /* If the LHS/RHS types are in registers, don't use them for the result. */
        uint32 mask = Registers::AvailRegs;
        if (backingLeft->type.inRegister())
            mask &= ~Registers::maskReg(backingLeft->type.reg());
        if (backingRight->type.inRegister())
            mask &= ~Registers::maskReg(backingRight->type.reg());

        RegisterID result = bestEvictReg(mask, true).reg();
        if (!commu && rightInReg && backingRight->data.reg() == result) {
            /* Can't put the result in the RHS for non-commutative operations. */
            alloc.result = allocReg();
            masm.move(alloc.lhsData.reg(), alloc.result);
        } else {
            alloc.result = result;
            if (leftInReg && result == backingLeft->data.reg()) {
                alloc.lhsNeedsRemat = true;
                unpinReg(result);
                takeReg(result);
            } else if (rightInReg && result == backingRight->data.reg()) {
                alloc.rhsNeedsRemat = true;
                alloc.resultHasRhs = true;
                unpinReg(result);
                takeReg(result);
            } else {
                JS_ASSERT(!regstate(result).isPinned());
                takeReg(result);
                if (leftInReg) {
                    masm.move(alloc.lhsData.reg(), result);
                } else {
                    masm.move(alloc.rhsData.reg(), result);
                    alloc.resultHasRhs = true;
                }
            }
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

void
FrameState::rematBinary(FrameEntry *lhs, FrameEntry *rhs, const BinaryAlloc &alloc, Assembler &masm)
{
    if (alloc.rhsNeedsRemat)
        masm.loadPayload(addressForDataRemat(rhs), alloc.rhsData.reg());
    if (alloc.lhsNeedsRemat)
        masm.loadPayload(addressForDataRemat(lhs), alloc.lhsData.reg());
}

MaybeRegisterID
FrameState::maybePinData(FrameEntry *fe)
{
    fe = fe->isCopy() ? fe->copyOf() : fe;
    if (fe->data.inRegister()) {
        pinReg(fe->data.reg());
        return fe->data.reg();
    }
    return MaybeRegisterID();
}

MaybeRegisterID
FrameState::maybePinType(FrameEntry *fe)
{
    fe = fe->isCopy() ? fe->copyOf() : fe;
    if (fe->type.inRegister()) {
        pinReg(fe->type.reg());
        return fe->type.reg();
    }
    return MaybeRegisterID();
}

void
FrameState::maybeUnpinReg(MaybeRegisterID reg)
{
    if (reg.isSet())
        unpinReg(reg.reg());
}

uint32
FrameState::allocTemporary()
{
    if (temporariesTop == temporaries + TEMPORARY_LIMIT)
        return uint32(-1);
    FrameEntry *fe = temporariesTop++;
    fe->lastLoop = 0;
    fe->temporary = true;
    return fe - temporaries;
}

void
FrameState::clearTemporaries()
{
    for (FrameEntry *fe = temporaries; fe < temporariesTop; fe++) {
        if (!fe->isTracked())
            continue;
        forgetAllRegs(fe);
        fe->resetSynced();
    }

    temporariesTop = temporaries;
}

Vector<TemporaryCopy> *
FrameState::getTemporaryCopies()
{
    Vector<TemporaryCopy> *res = NULL;

    for (FrameEntry *fe = temporaries; fe < temporariesTop; fe++) {
        if (!fe->isTracked())
            continue;
        if (fe->isCopied()) {
            for (uint32 i = fe->trackerIndex() + 1; i < a->tracker.nentries; i++) {
                FrameEntry *nfe = a->tracker[i];
                if (!deadEntry(nfe) && nfe->isCopy() && nfe->copyOf() == fe) {
                    if (!res)
                        res = cx->new_< Vector<TemporaryCopy> >(cx);
                    res->append(TemporaryCopy(addressOf(nfe), addressOf(fe)));  /* :XXX: handle OOM */
                }
            }
        }
    }

    return res;
}

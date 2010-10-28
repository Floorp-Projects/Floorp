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

#if !defined jsjaeger_framestate_h__ && defined JS_METHODJIT
#define jsjaeger_framestate_h__

#include "jsapi.h"
#include "methodjit/MachineRegs.h"
#include "methodjit/FrameEntry.h"
#include "CodeGenIncludes.h"
#include "ImmutableSync.h"
#include "jscompartment.h"

namespace js {
namespace mjit {

struct Uses {
    explicit Uses(uint32 nuses)
      : nuses(nuses)
    { }
    uint32 nuses;
};

struct Changes {
    explicit Changes(uint32 nchanges)
      : nchanges(nchanges)
    { }
    uint32 nchanges;
};

/*
 * The FrameState keeps track of values on the frame during compilation.
 * The compiler can query FrameState for information about arguments, locals,
 * and stack slots (all hereby referred to as "slots"). Slot information can
 * be requested in constant time. For each slot there is a FrameEntry *. If
 * this is non-NULL, it contains valid information and can be returned.
 *
 * The register allocator keeps track of registers as being in one of two
 * states. These are:
 *
 * 1) Unowned. Some code in the compiler is working on a register.
 * 2) Owned. The FrameState owns the register, and may spill it at any time.
 *
 * ------------------ Implementation Details ------------------
 * 
 * Observations:
 *
 * 1) We totally blow away known information quite often; branches, merge points.
 * 2) Every time we need a slow call, we must sync everything.
 * 3) Efficient side-exits need to quickly deltize state snapshots.
 * 4) Syncing is limited to constants and registers.
 * 5) Once a value is tracked, there is no reason to "forget" it until #1.
 * 
 * With these in mind, we want to make sure that the compiler doesn't degrade
 * badly as functions get larger.
 *
 * If the FE is NULL, a new one is allocated, initialized, and stored. They
 * are allocated from a pool such that (fe - pool) can be used to compute
 * the slot's Address.
 *
 * We keep a side vector of all tracked FrameEntry * to quickly generate
 * memory stores and clear the tracker.
 *
 * It is still possible to get really bad behavior with a very large script
 * that doesn't have branches or calls. That's okay, having this code in
 * minimizes damage and lets us introduce a hard cut-off point.
 */
class FrameState
{
    friend class ImmutableSync;

    typedef JSC::MacroAssembler::RegisterID RegisterID;
    typedef JSC::MacroAssembler::FPRegisterID FPRegisterID;
    typedef JSC::MacroAssembler::Address Address;
    typedef JSC::MacroAssembler::Jump Jump;
    typedef JSC::MacroAssembler::Imm32 Imm32;

    static const uint32 InvalidIndex = 0xFFFFFFFF;

    struct Tracker {
        Tracker()
          : entries(NULL), nentries(0)
        { }

        void add(FrameEntry *fe) {
            entries[nentries++] = fe;
        }

        void reset() {
            nentries = 0;
        }

        FrameEntry * operator [](uint32 n) const {
            JS_ASSERT(n < nentries);
            return entries[n];
        }

        FrameEntry **entries;
        uint32 nentries;
    };

    /*
     * Some RegisterState invariants.
     *
     *  If |fe| is non-NULL, |save| is NULL.
     *  If |save| is non-NULL, |fe| is NULL.
     *  That is, both |fe| and |save| cannot be non-NULL.
     *
     *  If either |fe| or |save| is non-NULL, the register is not in freeRegs.
     *  If both |fe| and |save| are NULL, the register is either in freeRegs,
     *  or owned by the compiler.
     */
    struct RegisterState {
        RegisterState() : fe_(NULL), save_(NULL)
        { }

        RegisterState(FrameEntry *fe, RematInfo::RematType type)
          : fe_(fe), save_(NULL), type_(type)
        {
            JS_ASSERT(!save_);
        }

        bool isPinned() const {
            assertConsistency();
            return !!save_;
        }

        void assertConsistency() const {
            JS_ASSERT_IF(fe_, !save_);
            JS_ASSERT_IF(save_, !fe_);
        }

        FrameEntry *fe() const {
            assertConsistency();
            return fe_;
        }

        RematInfo::RematType type() const {
            assertConsistency();
            return type_;
        }

        FrameEntry *usedBy() const {
            if (fe_)
                return fe_;
            return save_;
        }

        void associate(FrameEntry *fe, RematInfo::RematType type) {
            JS_ASSERT(!fe_);
            JS_ASSERT(!save_);

            fe_ = fe;
            type_ = type;
            JS_ASSERT(!save_);
        }

        /* Change ownership. */
        void reassociate(FrameEntry *fe) {
            assertConsistency();
            JS_ASSERT(fe);

            fe_ = fe;
        }

        /* Unassociate this register from the FE. */
        void forget() {
            JS_ASSERT(fe_);
            fe_ = NULL;
            JS_ASSERT(!save_);
        }

        void pin() {
            JS_ASSERT(fe_ != NULL);
            assertConsistency();
            save_ = fe_;
            fe_ = NULL;
        }

        void unpin() {
            JS_ASSERT(save_ != NULL);
            assertConsistency();
            fe_ = save_;
            save_ = NULL;
        }

        void unpinUnsafe() {
            assertConsistency();
            save_ = NULL;
        }

      private:
        /* FrameEntry owning this register, or NULL if not owned by a frame. */
        FrameEntry *fe_;

        /* Hack - simplifies register allocation for pairs. */
        FrameEntry *save_;
        
        /* Part of the FrameEntry that owns the FE. */
        RematInfo::RematType type_;
    };

  public:
    FrameState(JSContext *cx, JSScript *script, Assembler &masm);
    ~FrameState();
    bool init(uint32 nargs);

    /*
     * Pushes a synced slot.
     */
    inline void pushSynced();

    /*
     * Pushes a slot that has a known, synced type and payload.
     */
    inline void pushSyncedType(JSValueType type);

    /*
     * Pushes a slot that has a known, synced type and payload.
     */
    inline void pushSynced(JSValueType type, RegisterID reg);

    /*
     * Pushes a constant value.
     */
    inline void push(const Value &v);

    /*
     * Loads a value from memory and pushes it.
     */
    inline void push(Address address);

    /*
     * Pushes a known type and allocated payload onto the operation stack.
     */
    inline void pushTypedPayload(JSValueType type, RegisterID payload);

    /*
     * Pushes a type register and data register pair.
     */
    inline void pushRegs(RegisterID type, RegisterID data);

    /*
     * Pushes a known type and allocated payload onto the operation stack.
     * This must be used when the type is known, but cannot be propagated
     * because it is not known to be correct at a slow-path merge point.
     *
     * The caller guarantees that the tag was a fast-path check; that is,
     * the value it replaces on the stack had the same tag if the fast-path
     * was taken.
     */
    inline void pushUntypedPayload(JSValueType type, RegisterID payload);

    /*
     * Pushes a number onto the operation stack.
     *
     * If asInt32 is set to true, then the FS will attempt to optimize
     * syncing the type as int32. Only use this parameter when the fast-path
     * guaranteed that the stack slot was guarded to be an int32 originally.
     *
     * For example, checking LHS and RHS as ints guarantees that if the LHS
     * was synced, then popping both and pushing a maybe-int32 does not need
     * to be synced.
     */
    inline void pushNumber(MaybeRegisterID payload, bool asInt32 = false);

    /*
     * Pushes an int32 onto the operation stack. This is a specialized version
     * of pushNumber. The caller must guarantee that (a) an int32 is to be 
     * pushed on the inline path, and (b) if any slow path pushes a double,
     * the slow path also stores the double to memory.
     */
    inline void pushInt32(RegisterID payload);

    /*
     * Pops a value off the operation stack, freeing any of its resources.
     */
    inline void pop();

    /*
     * Pops a number of values off the operation stack, freeing any of their
     * resources.
     */
    inline void popn(uint32 n);

    /*
     * Returns true iff lhs and rhs are copies of the same FrameEntry.
     */
    inline bool haveSameBacking(FrameEntry *lhs, FrameEntry *rhs);

    /*
     * Temporarily increase and decrease local variable depth.
     */
    inline void enterBlock(uint32 n);
    inline void leaveBlock(uint32 n);

    /*
     * Pushes a copy of a local variable.
     */
    void pushLocal(uint32 n);

    /*
     * Allocates a temporary register for a FrameEntry's type. The register
     * can be spilled or clobbered by the frame. The compiler may only operate
     * on it temporarily, and must take care not to clobber it.
     */
    inline RegisterID tempRegForType(FrameEntry *fe);

    /*
     * Try to use a register already allocated for fe's type, but if one
     * is not already available, use fallback.
     *
     * Note: this does NOT change fe's type-register remat info. It's supposed
     * to be a super lightweight/transparent operation.
     */
    inline RegisterID tempRegForType(FrameEntry *fe, RegisterID fallback);

    /*
     * Returns a register that is guaranteed to contain the frame entry's
     * data payload. The compiler may not modify the contents of the register.
     * The compiler should NOT explicitly free it.
     */
    inline RegisterID tempRegForData(FrameEntry *fe);

    /*
     * Same as above, except register must match identically.
     */
    inline RegisterID tempRegInMaskForData(FrameEntry *fe, uint32 mask);

    /*
     * Same as above, except loads into reg (using masm) if the entry does not
     * already have a register, and does not change the frame state in doing so.
     */
    inline RegisterID tempRegForData(FrameEntry *fe, RegisterID reg, Assembler &masm) const;

    /*
     * Convert an integer to a double without applying
     * additional Register pressure.
     */
    inline void convertInt32ToDouble(Assembler &masm, FrameEntry *fe,
                                     FPRegisterID fpreg) const;

    /*
     * Dive into a FrameEntry and check whether it's in a register.
     */
    inline bool peekTypeInRegister(FrameEntry *fe) const;

    /*
     * Allocates a register for a FrameEntry's data, such that the compiler
     * can modify it in-place.
     *
     * The caller guarantees the FrameEntry will not be observed again. This
     * allows the compiler to avoid spilling. Only call this if the FE is
     * going to be popped before stubcc joins/guards or the end of the current
     * opcode.
     */
    RegisterID ownRegForData(FrameEntry *fe);

    /*
     * Allocates a register for a FrameEntry's type, such that the compiler
     * can modify it in-place.
     *
     * The caller guarantees the FrameEntry will not be observed again. This
     * allows the compiler to avoid spilling. Only call this if the FE is
     * going to be popped before stubcc joins/guards or the end of the current
     * opcode.
     */
    RegisterID ownRegForType(FrameEntry *fe);

    /*
     * Allocates a register for a FrameEntry's data, such that the compiler
     * can modify it in-place. The actual FE is not modified.
     */
    RegisterID copyDataIntoReg(FrameEntry *fe);
    void copyDataIntoReg(FrameEntry *fe, RegisterID exact);
    RegisterID copyDataIntoReg(Assembler &masm, FrameEntry *fe);

    /*
     * Allocates a FPRegister for a FrameEntry, such that the compiler
     * can modify it in-place. The FrameState is not modified.
     */
    FPRegisterID copyEntryIntoFPReg(FrameEntry *fe, FPRegisterID fpreg);
    FPRegisterID copyEntryIntoFPReg(Assembler &masm, FrameEntry *fe,
                                    FPRegisterID fpreg);

    /*
     * Allocates a register for a FrameEntry's type, such that the compiler
     * can modify it in-place. The actual FE is not modified.
     */
    RegisterID copyTypeIntoReg(FrameEntry *fe);

    /*
     * Returns a register that contains the constant Int32 value of the
     * frame entry's data payload.
     * Since the register is not bound to a FrameEntry,
     * it MUST be explicitly freed with freeReg().
     */
    RegisterID copyInt32ConstantIntoReg(FrameEntry *fe);
    RegisterID copyInt32ConstantIntoReg(Assembler &masm, FrameEntry *fe);

    /*
     * Gets registers for the components of fe where needed,
     * pins them and stores into vr.
     */
    void pinEntry(FrameEntry *fe, ValueRemat &vr);

    /* Unpins registers from a call to pinEntry. */
    void unpinEntry(const ValueRemat &vr);

    /* Syncs fe to memory, given its state as constructed by a call to pinEntry. */
    void ensureValueSynced(Assembler &masm, FrameEntry *fe, const ValueRemat &vr);

    struct BinaryAlloc {
        MaybeRegisterID lhsType;
        MaybeRegisterID lhsData;
        MaybeRegisterID rhsType;
        MaybeRegisterID rhsData;
        MaybeRegisterID extraFree;
        RegisterID result;  // mutable result reg
        bool resultHasRhs;  // whether the result has the RHS instead of the LHS
        bool lhsNeedsRemat; // whether LHS needs memory remat
        bool rhsNeedsRemat; // whether RHS needs memory remat
    };

    /*
     * Ensures that the two given FrameEntries have registers for both their
     * type and data. The register allocations are returned in a struct.
     *
     * One mutable register is allocated as well, holding the LHS payload. If
     * this would cause a spill that could be avoided by using a mutable RHS,
     * and the operation is commutative, then the resultHasRhs is set to true.
     */
    void allocForBinary(FrameEntry *lhs, FrameEntry *rhs, JSOp op, BinaryAlloc &alloc,
                        bool resultNeeded = true);

    /* Ensures that an FE has both type and data remat'd in registers. */
    void ensureFullRegs(FrameEntry *fe, MaybeRegisterID *typeReg, MaybeRegisterID *dataReg);

    /*
     * Similar to allocForBinary, except works when the LHS and RHS have the
     * same backing FE. Only a reduced subset of BinaryAlloc is used:
     *   lhsType
     *   lhsData
     *   result
     *   lhsNeedsRemat
     */
    void allocForSameBinary(FrameEntry *fe, JSOp op, BinaryAlloc &alloc);

    /* Loads an FE into an fp reg. */
    inline void loadDouble(FrameEntry *fe, FPRegisterID fpReg, Assembler &masm) const;

    /*
     * Slightly more specialized version when more precise register
     * information is known.
     */
    inline void loadDouble(RegisterID type, RegisterID data, FrameEntry *fe, FPRegisterID fpReg,
                           Assembler &masm) const;

    /*
     * Types don't always have to be in registers, sometimes the compiler
     * can use addresses and avoid spilling. If this FrameEntry has a synced
     * address and no register, this returns true.
     */
    inline bool shouldAvoidTypeRemat(FrameEntry *fe);

    /*
     * Payloads don't always have to be in registers, sometimes the compiler
     * can use addresses and avoid spilling. If this FrameEntry has a synced
     * address and no register, this returns true.
     */
    inline bool shouldAvoidDataRemat(FrameEntry *fe);

    /*
     * Frees a temporary register. If this register is being tracked, then it
     * is not spilled; the backing data becomes invalidated!
     */
    inline void freeReg(RegisterID reg);

    /*
     * Allocates a register. If none are free, one may be spilled from the
     * tracker. If there are none available for spilling in the tracker,
     * then this is considered a compiler bug and an assert will fire.
     */
    inline RegisterID allocReg();

    /*
     * Allocates a register, except using a mask.
     */
    inline RegisterID allocReg(uint32 mask);

    /*
     * Allocates a specific register, evicting it if it's not avaliable.
     */
    void takeReg(RegisterID reg);

    /*
     * Returns a FrameEntry * for a slot on the operation stack.
     */
    inline FrameEntry *peek(int32 depth);

    /*
     * Fully stores a FrameEntry at an arbitrary address. popHint specifies
     * how hard the register allocator should try to keep the FE in registers.
     */
    void storeTo(FrameEntry *fe, Address address, bool popHint = false);

    /*
     * Fully stores a FrameEntry into two arbitrary registers. tempReg may be
     * used as a temporary.
     */
    void loadForReturn(FrameEntry *fe, RegisterID typeReg, RegisterID dataReg, RegisterID tempReg);

    /*
     * Stores the top stack slot back to a slot.
     */
    void storeLocal(uint32 n, bool popGuaranteed = false, bool typeChange = true);
    void storeTop(FrameEntry *target, bool popGuaranteed = false, bool typeChange = true);

    /*
     * Restores state from a slow path.
     */
    void merge(Assembler &masm, Changes changes) const;

    /*
     * Writes unsynced stores to an arbitrary buffer.
     */
    void sync(Assembler &masm, Uses uses) const;

    /*
     * Syncs all outstanding stores to memory and possibly kills regs in the
     * process.  The top [ignored..uses-1] frame entries will be synced.
     */
    void syncAndKill(Registers kill, Uses uses, Uses ignored);
    void syncAndKill(Registers kill, Uses uses) { syncAndKill(kill, uses, Uses(0)); }

    /* Syncs and kills everything. */
    void syncAndKillEverything() {
        syncAndKill(Registers(Registers::AvailRegs), Uses(frameDepth()));
    }

    /*
     * Clear all tracker entries, syncing all outstanding stores in the process.
     * The stack depth is in case some merge points' edges did not immediately
     * precede the current instruction.
     */
    inline void syncAndForgetEverything(uint32 newStackDepth);

    /*
     * Same as above, except the stack depth is not changed. This is used for
     * branching opcodes.
     */
    void syncAndForgetEverything();

    /*
     * Throw away the entire frame state, without syncing anything.
     * This can only be called after a syncAndKill() against all registers.
     */
    void forgetEverything();

    /*
     * Discard the entire framestate forcefully.
     */
    void discardFrame();

    /*
     * Mark an existing slot with a type.
     */
    inline void learnType(FrameEntry *fe, JSValueType type);

    /*
     * Forget a type, syncing in the process.
     */
    inline void forgetType(FrameEntry *fe);

    /*
     * Discards a FrameEntry, tricking the FS into thinking it's synced.
     */
    void discardFe(FrameEntry *fe);

    /*
     * Helper function. Tests if a slot's type is null. Condition must
     * be Equal or NotEqual.
     */
    inline Jump testNull(Assembler::Condition cond, FrameEntry *fe);

    /*
     * Helper function. Tests if a slot's type is undefined. Condition must
     * be Equal or NotEqual.
     */
    inline Jump testUndefined(Assembler::Condition cond, FrameEntry *fe);

    /*
     * Helper function. Tests if a slot's type is an integer. Condition must
     * be Equal or NotEqual.
     */
    inline Jump testInt32(Assembler::Condition cond, FrameEntry *fe);

    /*
     * Helper function. Tests if a slot's type is a double. Condition must
     * be Equal or Not Equal.
     */
    inline Jump testDouble(Assembler::Condition cond, FrameEntry *fe);

    /*
     * Helper function. Tests if a slot's type is a boolean. Condition must
     * be Equal or NotEqual.
     */
    inline Jump testBoolean(Assembler::Condition cond, FrameEntry *fe);

    /*
     * Helper function. Tests if a slot's type is a string. Condition must
     * be Equal or NotEqual.
     */
    inline Jump testString(Assembler::Condition cond, FrameEntry *fe);

    /*
     * Helper function. Tests if a slot's type is a non-funobj. Condition must
     * be Equal or NotEqual.
     */
    inline Jump testObject(Assembler::Condition cond, FrameEntry *fe);

    /*
     * Helper function. Tests if a slot's type is primitive. Condition must
     * be Equal or NotEqual.
     */
    inline Jump testPrimitive(Assembler::Condition cond, FrameEntry *fe);

    /*
     * Marks a register such that it cannot be spilled by the register
     * allocator. Any pinned registers must be unpinned at the end of the op,
     * no matter what. In addition, pinReg() can only be used on registers
     * which are associated with FrameEntries.
     */
    inline void pinReg(RegisterID reg);

    /*
     * Unpins a previously pinned register.
     */
    inline void unpinReg(RegisterID reg);

    /*
     * Same as unpinReg(), but does not restore the FrameEntry.
     */
    inline void unpinKilledReg(RegisterID reg);

    /* Pins a data or type register if one exists. */
    MaybeRegisterID maybePinData(FrameEntry *fe);
    MaybeRegisterID maybePinType(FrameEntry *fe);
    void maybeUnpinReg(MaybeRegisterID reg);

    /*
     * Dups the top item on the stack.
     */
    inline void dup();

    /*
     * Dups the top 2 items on the stack.
     */
    inline void dup2();

    /*
     * Dups an item n-deep in the stack. n must be < 0
     */
    inline void dupAt(int32 n);

    /*
     * If the frameentry is a copy, give it its own registers.
     * This may only be called on the topmost fe.
     */
    inline void giveOwnRegs(FrameEntry *fe);

    /*
     * Returns the current stack depth of the frame.
     */
    uint32 stackDepth() const { return sp - spBase; }
    uint32 frameDepth() const { return stackDepth() + script->nfixed; }

#ifdef DEBUG
    void assertValidRegisterState() const;
#endif

    Address addressOf(const FrameEntry *fe) const;
    Address addressForDataRemat(const FrameEntry *fe) const;

    inline StateRemat dataRematInfo(const FrameEntry *fe) const;

    /*
     * This is similar to freeReg(ownRegForData(fe)) - except no movement takes place.
     * The fe is simply invalidated as if it were popped. This can be used to free
     * registers in the working area of the stack. Obviously, this can only be called
     * in infallible code that will pop these entries soon after.
     */
    inline void eviscerate(FrameEntry *fe);

    /*
     * Moves the top of the stack down N slots, popping each item above it.
     * Caller guarantees the slots below have been observed and eviscerated.
     */
    void shimmy(uint32 n);

    /*
     * Stores the top item on the stack to a stack slot, count down from the
     * current stack depth. For example, to move the top (-1) to -3, you would
     * call shift(-2).
     */
    void shift(int32 n);

    /*
     * Notifies the frame of a slot that can escape.
     */
    inline void setClosedVar(uint32 slot);

    inline void setInTryBlock(bool inTryBlock) {
        this->inTryBlock = inTryBlock;
    }

  private:
    inline RegisterID allocReg(FrameEntry *fe, RematInfo::RematType type);
    inline void forgetReg(RegisterID reg);
    RegisterID evictSomeReg(uint32 mask);
    void evictReg(RegisterID reg);
    inline FrameEntry *rawPush();
    inline void addToTracker(FrameEntry *fe);

    /* Guarantee sync, but do not set any sync flag. */
    inline void ensureFeSynced(const FrameEntry *fe, Assembler &masm) const;
    inline void ensureTypeSynced(const FrameEntry *fe, Assembler &masm) const;
    inline void ensureDataSynced(const FrameEntry *fe, Assembler &masm) const;

    /* Guarantee sync, even if register allocation is required, and set sync. */
    inline void syncFe(FrameEntry *fe);
    inline void syncType(FrameEntry *fe);
    inline void syncData(FrameEntry *fe);

    inline FrameEntry *getLocal(uint32 slot);
    inline void forgetAllRegs(FrameEntry *fe);
    inline void swapInTracker(FrameEntry *lhs, FrameEntry *rhs);
    inline uint32 localIndex(uint32 n);
    void pushCopyOf(uint32 index);
#if defined JS_NUNBOX32
    void syncFancy(Assembler &masm, Registers avail, FrameEntry *resumeAt,
                   FrameEntry *bottom) const;
#endif
    inline bool tryFastDoubleLoad(FrameEntry *fe, FPRegisterID fpReg, Assembler &masm) const;
    void resetInternalState();

    /*
     * "Uncopies" the backing store of a FrameEntry that has been copied. The
     * original FrameEntry is not invalidated; this is the responsibility of
     * the caller. The caller can check isCopied() to see if the registers
     * were moved to a copy.
     *
     * Later addition: uncopy() returns the first copy found.
     */
    FrameEntry *uncopy(FrameEntry *original);
    FrameEntry *walkTrackerForUncopy(FrameEntry *original);
    FrameEntry *walkFrameForUncopy(FrameEntry *original);

    /*
     * All registers in the FE are forgotten. If it is copied, it is uncopied
     * beforehand.
     */
    void forgetEntry(FrameEntry *fe);

    FrameEntry *entryFor(uint32 index) const {
        JS_ASSERT(entries[index].isTracked());
        return &entries[index];
    }

    RegisterID evictSomeReg() {
        return evictSomeReg(Registers::AvailRegs);
    }

    uint32 indexOf(int32 depth) {
        return uint32((sp + depth) - entries);
    }

    uint32 indexOfFe(FrameEntry *fe) const {
        return uint32(fe - entries);
    }

    inline bool isClosedVar(uint32 slot);

  private:
    JSContext *cx;
    JSScript *script;
    uint32 nargs;
    Assembler &masm;

    /* All allocated registers. */
    Registers freeRegs;

    /* Cache of FrameEntry objects. */
    FrameEntry *entries;

    /* Base pointer for arguments. */
    FrameEntry *args;

    /* Base pointer for local variables. */
    FrameEntry *locals;

    /* Base pointer for the stack. */
    FrameEntry *spBase;

    /* Dynamic stack pointer. */
    FrameEntry *sp;

    /* Vector of tracked slot indexes. */
    Tracker tracker;

    /*
     * Register ownership state. This can't be used alone; to find whether an
     * entry is active, you must check the allocated registers.
     */
    RegisterState regstate[Assembler::TotalRegisters];

#if defined JS_NUNBOX32
    mutable ImmutableSync reifier;
#endif

    JSPackedBool *closedVars;
    bool eval;
    bool inTryBlock;
};

} /* namespace mjit */
} /* namespace js */

#endif /* jsjaeger_framestate_h__ */


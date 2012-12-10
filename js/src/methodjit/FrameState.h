/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined jsjaeger_framestate_h__ && defined JS_METHODJIT
#define jsjaeger_framestate_h__

#include "jsanalyze.h"
#include "jsapi.h"
#include "methodjit/MachineRegs.h"
#include "methodjit/FrameEntry.h"
#include "CodeGenIncludes.h"
#include "ImmutableSync.h"
#include "jscompartment.h"

namespace js {
namespace mjit {

struct Uses {
    explicit Uses(uint32_t nuses)
      : nuses(nuses)
    { }
    uint32_t nuses;
};

struct Changes {
    explicit Changes(uint32_t nchanges)
      : nchanges(nchanges)
    { }
    uint32_t nchanges;
};

struct TemporaryCopy {
    TemporaryCopy(JSC::MacroAssembler::Address copy, JSC::MacroAssembler::Address temporary)
        : copy(copy), temporary(temporary)
    {}
    JSC::MacroAssembler::Address copy;
    JSC::MacroAssembler::Address temporary;
};

class StubCompiler;
class LoopState;

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
 * 1) Every time we need a slow call, we must sync everything.
 * 2) Efficient side-exits need to quickly deltize state snapshots.
 * 3) Syncing is limited to constants and registers.
 * 4) Entries are not forgotten unless they are entirely in memory and are
 *    not constants or copies.
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
    typedef JSC::MacroAssembler::AbsoluteAddress AbsoluteAddress;
    typedef JSC::MacroAssembler::Jump Jump;
    typedef JSC::MacroAssembler::Imm32 Imm32;
    typedef JSC::MacroAssembler::ImmPtr ImmPtr;

    static const uint32_t InvalidIndex = 0xFFFFFFFF;

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

        FrameEntry * operator [](uint32_t n) const {
            JS_ASSERT(n < nentries);
            return entries[n];
        }

        FrameEntry **entries;
        uint32_t nentries;
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

    struct ActiveFrame;

    FrameState *thisFromCtor() { return this; }
  public:
    FrameState(JSContext *cx, Compiler &cc, Assembler &masm, StubCompiler &stubcc);
    ~FrameState();

    /*
     * Pushes a synced slot that may have a known type.
     */
    inline void pushSynced(JSValueType knownType);

    /*
     * Pushes a slot that has a known, synced type and payload.
     */
    inline void pushSynced(JSValueType knownType, RegisterID reg);

    /*
     * Pushes a constant value.
     */
    inline void push(const Value &v);

    /*
     * Loads a value from memory and pushes it. If reuseBase is set, the
     * Compiler owns the register and it should be reused if possible.
     */
    inline void push(Address address, JSValueType knownType, bool reuseBase = false);

    /*
     * Loads a word from memory and pushes it. If reuseBase is set, the
     * Compiler owns the register and it should be reused if possible.
     * It takes an address and loads/pushes an unboxed word of a given non-double type.
     */
    inline void pushWord(Address address, JSValueType knownType, bool reuseBase = false);

    /* Loads a value from memory into a register pair, returning the register. */
    inline void loadIntoRegisters(Address address, bool reuseBase,
                                  RegisterID *ptypeReg, RegisterID *pdataReg);

    /*
     * Pushes a known type and allocated payload onto the operation stack.
     */
    inline void pushTypedPayload(JSValueType type, RegisterID payload);

    /*
     * Clobbers a stack entry with a type register and data register pair,
     * converting to the specified known type if necessary.  If the type is
     * JSVAL_TYPE_DOUBLE, the registers are converted into a floating point
     * register, which is returned.
     */
    inline FPRegisterID storeRegs(int32_t depth, RegisterID type, RegisterID data,
                                  JSValueType knownType);
    inline FPRegisterID pushRegs(RegisterID type, RegisterID data, JSValueType knownType);

    /*
     * Load an address into a frame entry in registers. For handling call paths
     * where merge() would otherwise reload from the wrong address.
     */
    inline void reloadEntry(Assembler &masm, Address address, FrameEntry *fe);

    /* Push a value which is definitely a double. */
    void pushDouble(FPRegisterID fpreg);
    void pushDouble(Address address);

    /* Ensure that fe is definitely a double. It must already be either int or double. */
    void ensureDouble(FrameEntry *fe);

    /* Revert an entry just converted to double by ensureDouble. */
    void ensureInteger(FrameEntry *fe);

    /*
     * Emit code to masm ensuring that all in memory slots thought to be
     * doubles are in fact doubles.
     */
    void ensureInMemoryDoubles(Assembler &masm);

    /* Forget that fe is definitely a double. */
    void forgetKnownDouble(FrameEntry *fe);

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
     * Pushes a value onto the operation stack. This must be used when the
     * value is known, but its type cannot be propagated because it is not
     * known to be correct at a slow-path merge point.
     */
    inline void pushUntypedValue(const Value &value);

    /*
     * Pushes a number onto the operation stack.
     *
     * If asInt32 is set to true, then the FS will attempt to optimize
     * syncing the type as int32_t. Only use this parameter when the fast-path
     * guaranteed that the stack slot was guarded to be an int32_t originally.
     *
     * For example, checking LHS and RHS as ints guarantees that if the LHS
     * was synced, then popping both and pushing a maybe-int32_t does not need
     * to be synced.
     */
    inline void pushNumber(RegisterID payload, bool asInt32 = false);

    /*
     * Pushes an int32_t onto the operation stack. This is a specialized version
     * of pushNumber. The caller must guarantee that (a) an int32_t is to be
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
    inline void popn(uint32_t n);

    /*
     * Returns true iff lhs and rhs are copies of the same FrameEntry.
     */
    inline bool haveSameBacking(FrameEntry *lhs, FrameEntry *rhs);

    /* If the rhs to a binary operation directly copies the lhs, uncopy the lhs. */
    void separateBinaryEntries(FrameEntry *lhs, FrameEntry *rhs);

    /*
     * Temporarily increase and decrease local variable depth.
     */
    inline void enterBlock(uint32_t n);
    inline void leaveBlock(uint32_t n);

    // Pushes a copy of a slot (formal argument, local variable, or stack slot)
    // onto the operation stack.
    void pushLocal(uint32_t n);
    void pushArg(uint32_t n);
    void pushCallee();
    void pushThis();
    void pushCopyOf(FrameEntry *fe);
    inline void setThis(RegisterID reg);
    inline void syncThis();
    inline void learnThisIsObject(bool unsync = true);

    inline FrameEntry *getStack(uint32_t slot);
    inline FrameEntry *getLocal(uint32_t slot);
    inline FrameEntry *getArg(uint32_t slot);
    inline FrameEntry *getSlotEntry(uint32_t slot);

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

    inline void loadTypeIntoReg(const FrameEntry *fe, RegisterID reg);
    inline void loadDataIntoReg(const FrameEntry *fe, RegisterID reg);

    /*
     * Returns a register that is guaranteed to contain the frame entry's
     * data payload. The compiler may not modify the contents of the register.
     * The compiler should NOT explicitly free it.
     */
    inline RegisterID tempRegForData(FrameEntry *fe);
    inline FPRegisterID tempFPRegForData(FrameEntry *fe);

    /*
     * Same as above, except register must match identically.
     */
    inline AnyRegisterID tempRegInMaskForData(FrameEntry *fe, uint32_t mask);

    /*
     * Same as above, except loads into reg (using masm) if the entry does not
     * already have a register, and does not change the frame state in doing so.
     */
    inline RegisterID tempRegForData(FrameEntry *fe, RegisterID reg, Assembler &masm) const;

    /*
     * For opcodes which expect to operate on an object, forget the entry if it
     * is either a known constant or a non-object. This simplifies path
     * generation in the Compiler for such unusual cases.
     */
    inline void forgetMismatchedObject(FrameEntry *fe);

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
     * Gets registers for the components of fe where needed, pins them and
     * stores into vr. If breakDouble is set, vr is guaranteed not to be a
     * floating point register.
     */
    void pinEntry(FrameEntry *fe, ValueRemat &vr, bool breakDouble = true);

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
        FPRegisterID lhsFP; // mutable scratch floating point reg
        FPRegisterID rhsFP; // mutable scratch floating point reg
        bool resultHasRhs;  // whether the result has the RHS instead of the LHS
        bool lhsNeedsRemat; // whether LHS needs memory remat
        bool rhsNeedsRemat; // whether RHS needs memory remat
        bool undoResult;    // whether to remat LHS/RHS by undoing operation
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

    /*
     * After the result register in a BinaryAlloc has been clobbered, rematerialize
     * the left or right side if necessary to restore the original values.
     */
    void rematBinary(FrameEntry *lhs, FrameEntry *rhs, const BinaryAlloc &alloc, Assembler &masm);

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
    inline void freeReg(AnyRegisterID reg);

    /*
     * Allocates a register. If none are free, one may be spilled from the
     * tracker. If there are none available for spilling in the tracker,
     * then this is considered a compiler bug and an assert will fire.
     */
    inline RegisterID allocReg();
    inline FPRegisterID allocFPReg();

    /*
     * Allocates a register, except using a mask.
     */
    inline AnyRegisterID allocReg(uint32_t mask);

    /*
     * Allocates a specific register, evicting it if it's not avaliable.
     */
    void takeReg(AnyRegisterID reg);

    /*
     * Returns a FrameEntry * for a slot on the operation stack.
     */
    inline FrameEntry *peek(int32_t depth);

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
    void loadThisForReturn(RegisterID typeReg, RegisterID dataReg, RegisterID tempReg);

    /* Stores the top stack slot back to a local or slot. */
    void storeLocal(uint32_t n, bool popGuaranteed = false);
    void storeArg(uint32_t n, bool popGuaranteed = false);
    void storeTop(FrameEntry *target);

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
    void syncAndKill(Uses uses) { syncAndKill(Registers(Registers::AvailAnyRegs), uses, Uses(0)); }

    /* Syncs and kills everything. */
    void syncAndKillEverything() {
        syncAndKill(Registers(Registers::AvailAnyRegs), Uses(frameSlots()));
    }

    /*
     * Throw away the entire frame state, without syncing anything.
     * This can only be called after a syncAndKill() against all registers.
     */
    void forgetEverything();

    void syncAndForgetEverything()
    {
        syncAndKillEverything();
        forgetEverything();
    }

    /*
     * Discard the entire framestate forcefully.
     */
    void discardFrame();

    /*
     * Make a copy of the current frame state, and restore from that snapshot.
     * The stack depth must match between the snapshot and restore points.
     */
    FrameEntry *snapshotState();
    void restoreFromSnapshot(FrameEntry *snapshot);

    /*
     * Tries to sync and shuffle registers in accordance with the register state
     * at target, constructing that state if necessary. Forgets all constants and
     * copies, and nothing can be pinned. Keeps the top Uses in registers; if Uses
     * is non-zero the state may not actually be consistent with target.
     */
    bool syncForBranch(jsbytecode *target, Uses uses);
    void syncForAllocation(RegisterAllocation *alloc, bool inlineReturn, Uses uses);

    /* Discards the current frame state and updates to a new register allocation. */
    bool discardForJoin(RegisterAllocation *&alloc, uint32_t stackDepth);

    RegisterAllocation * computeAllocation(jsbytecode *target);

    /* Return whether the register state is consistent with that at target. */
    bool consistentRegisters(jsbytecode *target);

    /*
     * Load all registers to update from either the current register state (if synced
     * is unset) or a synced state (if synced is set) to target.
     */
    void prepareForJump(jsbytecode *target, Assembler &masm, bool synced);

    /*
     * Mark an existing slot with a type. unsync indicates whether type is already synced.
     * Do not call this on entries which might be copied.
     */
    inline void learnType(FrameEntry *fe, JSValueType type, bool unsync = true);
    inline void learnType(FrameEntry *fe, JSValueType type, RegisterID payload);

    /*
     * Forget a type, syncing in the process.
     */
    inline void forgetType(FrameEntry *fe);

    /*
     * Discards a FrameEntry, tricking the FS into thinking it's synced.
     */
    void discardFe(FrameEntry *fe);

    /* Compiler-owned metadata about stack entries, reset on push/pop/copy. */
    struct StackEntryExtra {
        bool initArray;
        JSObject *initObject;
        types::StackTypeSet *types;
        JSAtom *name;
        void reset() { PodZero(this); }
    };
    StackEntryExtra& extra(const FrameEntry *fe) {
        JS_ASSERT(fe >= a->args && fe < a->sp);
        return extraArray[fe - entries];
    }
    StackEntryExtra& extra(uint32_t slot) { return extra(entries + slot); }

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

    inline Jump testGCThing(FrameEntry *fe);

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
    inline void pinReg(AnyRegisterID reg) { regstate(reg).pin(); }

    /*
     * Unpins a previously pinned register.
     */
    inline void unpinReg(AnyRegisterID reg) { regstate(reg).unpin(); }

    /*
     * Same as unpinReg(), but does not restore the FrameEntry.
     */
    inline void unpinKilledReg(AnyRegisterID reg);

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
    inline void dupAt(int32_t n);

    /*
     * Syncs an item n-deep in the stack.
     */
    inline void syncAt(int32_t n);

    /*
     * If the frameentry is a copy, give it its own registers.
     * This may only be called on the topmost fe.
     */
    inline void giveOwnRegs(FrameEntry *fe);

    uint32_t stackDepth() const { return a->sp - a->spBase; }

    /*
     * The stack depth of the current frame plus any locals and space
     * for inlined frames, i.e. the difference between the end of the
     * current fp and sp.
     */
    uint32_t totalDepth() const { return a->depth + a->script->nfixed + stackDepth(); }

    // Returns the number of entries in the frame, that is:
    //   2 for callee, this +
    //   nargs +
    //   nfixed +
    //   currently pushed stack slots
    uint32_t frameSlots() const { return uint32_t(a->sp - a->callee_); }

#ifdef DEBUG
    void assertValidRegisterState() const;
#else
    inline void assertValidRegisterState() const {}
#endif

    // Return an address, relative to the StackFrame, that represents where
    // this FrameEntry is stored in memory. Note that this is its canonical
    // address, not its backing store. There is no guarantee that the memory
    // is coherent.
    Address addressOf(const FrameEntry *fe) const;
    Address addressOf(uint32_t slot) const { return addressOf(a->callee_ + slot); }

    Address addressOfTop() const { return addressOf(a->sp); }

    // Returns an address, relative to the StackFrame, that represents where
    // this FrameEntry is backed in memory. This is not necessarily its
    // canonical address, but the address for which the payload has been synced
    // to memory. The caller guarantees that the payload has been synced.
    Address addressForDataRemat(const FrameEntry *fe) const;

    // Inside an inline frame, the address for the return value in the caller.
    Address addressForInlineReturn();

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
    void shimmy(uint32_t n);

    /*
     * Stores the top item on the stack to a stack slot, count down from the
     * current stack depth. For example, to move the top (-1) to -3, you would
     * call shift(-2).
     */
    void shift(int32_t n);

    /* Swaps the top two items on the stack. Requires two temp slots. */
    void swap();

    inline void setInTryBlock(bool inTryBlock) {
        this->inTryBlock = inTryBlock;
    }

    inline uint32_t regsInUse() const { return Registers::AvailRegs & ~freeRegs.freeMask; }

    void setPC(jsbytecode *PC) { a->PC = PC; }
    void setLoop(LoopState *loop) { this->loop = loop; }

    void pruneDeadEntries();

    bool pushActiveFrame(JSScript *script, uint32_t argc);
    void popActiveFrame();

    uint32_t entrySlot(const FrameEntry *fe) const {
        return frameSlot(a, fe);
    }

    uint32_t outerSlot(const FrameEntry *fe) const {
        ActiveFrame *na = a;
        while (na->parent) { na = na->parent; }
        return frameSlot(na, fe);
    }

    bool isOuterSlot(const FrameEntry *fe) const {
        if (isTemporary(fe))
            return true;
        ActiveFrame *na = a;
        while (na->parent) { na = na->parent; }
        return fe < na->spBase && fe != na->callee_;
    }

#ifdef DEBUG
    const char * entryName(const FrameEntry *fe) const;
    void dumpAllocation(RegisterAllocation *alloc);
#else
    const char * entryName(const FrameEntry *fe) const { return NULL; }
#endif
    const char * entryName(uint32_t slot) { return entryName(entries + slot); }

    /* Maximum number of analysis temporaries the FrameState can track. */
    static const uint32_t TEMPORARY_LIMIT = 10;

    uint32_t allocTemporary();  /* -1 if limit reached. */
    void clearTemporaries();
    inline FrameEntry *getTemporary(uint32_t which);

    /*
     * Return NULL or a new vector with all current copies of temporaries,
     * excluding those about to be popped per 'uses'.
     */
    Vector<TemporaryCopy> *getTemporaryCopies(Uses uses);

    inline void syncAndForgetFe(FrameEntry *fe, bool markSynced = false);
    inline void forgetLoopReg(FrameEntry *fe);

  private:
    inline AnyRegisterID allocAndLoadReg(FrameEntry *fe, bool fp, RematInfo::RematType type);
    inline void forgetReg(AnyRegisterID reg);
    AnyRegisterID evictSomeReg(uint32_t mask);
    void evictReg(AnyRegisterID reg);
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

    /* For a frame entry whose value is dead, mark as synced. */
    inline void fakeSync(FrameEntry *fe);

    inline FrameEntry *getCallee();
    inline FrameEntry *getThis();
    inline FrameEntry *getOrTrack(uint32_t index);

    inline void forgetAllRegs(FrameEntry *fe);
    inline void swapInTracker(FrameEntry *lhs, FrameEntry *rhs);
#if defined JS_NUNBOX32
    void syncFancy(Assembler &masm, Registers avail, int trackerIndex) const;
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

    /* Whether fe is the only copy of backing. */
    bool hasOnlyCopy(FrameEntry *backing, FrameEntry *fe);

    /*
     * All registers in the FE are forgotten. If it is copied, it is uncopied
     * beforehand.
     */
    void forgetEntry(FrameEntry *fe);

    /* Stack and temporary entries whose contents should be disregarded. */
    bool deadEntry(const FrameEntry *fe, unsigned uses = 0) const {
        return (fe >= (a->sp - uses) && fe < temporaries) || fe >= temporariesTop;
    }

    RegisterState & regstate(AnyRegisterID reg) {
        JS_ASSERT(reg.reg_ < Registers::TotalAnyRegisters);
        return regstate_[reg.reg_];
    }

    const RegisterState & regstate(AnyRegisterID reg) const {
        JS_ASSERT(reg.reg_ < Registers::TotalAnyRegisters);
        return regstate_[reg.reg_];
    }

    AnyRegisterID bestEvictReg(uint32_t mask, bool includePinned) const;
    void evictDeadEntries(bool includePinned);

    inline analyze::Lifetime * variableLive(FrameEntry *fe, jsbytecode *pc) const;
    inline bool binaryEntryLive(FrameEntry *fe) const;
    void relocateReg(AnyRegisterID reg, RegisterAllocation *alloc, Uses uses);

    bool isThis(const FrameEntry *fe) const {
        return fe == a->this_;
    }

    inline bool isConstructorThis(const FrameEntry *fe) const;

    bool isArg(const FrameEntry *fe) const {
        return a->script->function() && fe >= a->args && fe - a->args < a->script->function()->nargs;
    }

    bool isLocal(const FrameEntry *fe) const {
        return fe >= a->locals && fe - a->locals < a->script->nfixed;
    }

    bool isTemporary(const FrameEntry *fe) const {
        JS_ASSERT_IF(fe >= temporaries, fe < temporariesTop);
        return fe >= temporaries;
    }

    int32_t frameOffset(const FrameEntry *fe, ActiveFrame *a) const;
    Address addressOf(const FrameEntry *fe, ActiveFrame *a) const;
    uint32_t frameSlot(ActiveFrame *a, const FrameEntry *fe) const;

    void associateReg(FrameEntry *fe, RematInfo::RematType type, AnyRegisterID reg);

    inline void modifyReg(AnyRegisterID reg);

    MaybeJump guardArrayLengthBase(FrameEntry *obj, Int32Key key);

  private:
    JSContext *cx;
    Assembler &masm;
    Compiler &cc;
    StubCompiler &stubcc;

    /* State for the active stack frame. */

    struct ActiveFrame {
        ActiveFrame() { PodZero(this); }

        ActiveFrame *parent;

        /* Number of values between the start of the outer frame and the start of this frame. */
        uint32_t depth;

        JSScript *script;
        jsbytecode *PC;
        analyze::ScriptAnalysis *analysis;

        /* Indexes into the main FrameEntry buffer of entries for this frame. */
        FrameEntry *callee_;
        FrameEntry *this_;
        FrameEntry *args;
        FrameEntry *locals;
        FrameEntry *spBase;
        FrameEntry *sp;
    };
    ActiveFrame *a;

    /* Common buffer of frame entries. */
    FrameEntry *entries;
    uint32_t nentries;

    /* Compiler-owned metadata for stack contents. */
    StackEntryExtra *extraArray;

    /* Vector of tracked slot indexes. */
    Tracker tracker;

#if defined JS_NUNBOX32
    mutable ImmutableSync reifier;
#endif

    /*
     * Register ownership state. This can't be used alone; to find whether an
     * entry is active, you must check the allocated registers.
     */
    RegisterState regstate_[Registers::TotalAnyRegisters];

    /* All unallocated registers. */
    Registers freeRegs;

    /* Stack of active loops. */
    LoopState *loop;

    /*
     * Track state for analysis temporaries. The meaning of these temporaries
     * is opaque to the frame state, which just tracks where they are stored.
     */
    FrameEntry *temporaries;
    FrameEntry *temporariesTop;

    bool inTryBlock;
};

/*
 * Register allocation overview. We want to allocate registers at the same time
 * as we emit code, in a single forward pass over the script. This is good both
 * for compilation speed and for design simplicity; we allocate registers for
 * variables and temporaries as the compiler needs them. To get a good allocation,
 * however, we need knowledge of which variables will be used in the future and
 * in what order --- we must prioritize keeping variables in registers which
 * will be used soon, and evict variables after they are no longer needed.
 * We get this from the analyze::LifetimeScript analysis, an initial backwards
 * pass over the script.
 *
 * Combining a backwards lifetime pass with a forward allocation pass in this
 * way produces a Linear-scan register allocator. These can generate code at
 * a speed close to that produced by a graph coloring register allocator,
 * at a fraction of the compilation time.
 */

/* Register allocation to use at a join point. */
struct RegisterAllocation {
  private:
    typedef JSC::MacroAssembler::RegisterID RegisterID;
    typedef JSC::MacroAssembler::FPRegisterID FPRegisterID;

    /* Entry for an unassigned register at the join point. */
    static const uint32_t UNASSIGNED_REGISTER = UINT32_MAX;

    /*
     * In the body of a loop, entry for an unassigned register that has not been
     * used since the start of the loop. We do not finalize the register state
     * at the start of a loop body until after generating code for the entire loop,
     * so we can decide on which variables to carry around the loop after seeing
     * them accessed early on in the body.
     */
    static const uint32_t LOOP_REGISTER = uint32_t(-2);

    /*
     * Assignment of registers to payloads. Type tags are always in memory,
     * except for known doubles in FP registers. These are indexes into the
     * frame's entries[] buffer, not slots.
     */
    uint32_t regstate_[Registers::TotalAnyRegisters];

    /* Mask for regstate entries indicating if the slot is synced. */
    static const uint32_t SYNCED = 0x80000000;

    uint32_t & regstate(AnyRegisterID reg) {
        JS_ASSERT(reg.reg_ < Registers::TotalAnyRegisters);
        return regstate_[reg.reg_];
    }

  public:
    RegisterAllocation(bool forLoop)
    {
        uint32_t entry = forLoop ? (uint32_t) LOOP_REGISTER : (uint32_t) UNASSIGNED_REGISTER;
        for (unsigned i = 0; i < Registers::TotalAnyRegisters; i++) {
            AnyRegisterID reg = AnyRegisterID::fromRaw(i);
            bool avail = Registers::maskReg(reg) & Registers::AvailAnyRegs;
            regstate_[i] = avail ? entry : UNASSIGNED_REGISTER;
        }
    }

    bool assigned(AnyRegisterID reg) {
        return regstate(reg) != UNASSIGNED_REGISTER && regstate(reg) != LOOP_REGISTER;
    }

    bool loop(AnyRegisterID reg) {
        return regstate(reg) == LOOP_REGISTER;
    }

    bool synced(AnyRegisterID reg) {
        JS_ASSERT(assigned(reg));
        return regstate(reg) & SYNCED;
    }

    uint32_t index(AnyRegisterID reg) {
        JS_ASSERT(assigned(reg));
        return regstate(reg) & ~SYNCED;
    }

    void set(AnyRegisterID reg, uint32_t index, bool synced) {
        JS_ASSERT(index != LOOP_REGISTER && index != UNASSIGNED_REGISTER);
        regstate(reg) = index | (synced ? SYNCED : 0);
    }

    void setUnassigned(AnyRegisterID reg) {
        regstate(reg) = UNASSIGNED_REGISTER;
    }

    bool synced() {
        for (unsigned i = 0; i < Registers::TotalAnyRegisters; i++) {
            if (assigned(AnyRegisterID::fromRaw(i)))
                return false;
        }
        return true;
    }

    void clearLoops() {
        for (unsigned i = 0; i < Registers::TotalAnyRegisters; i++) {
            AnyRegisterID reg = AnyRegisterID::fromRaw(i);
            if (loop(reg))
                setUnassigned(reg);
        }
    }

    bool hasAnyReg(uint32_t n) {
        for (unsigned i = 0; i < Registers::TotalAnyRegisters; i++) {
            AnyRegisterID reg = AnyRegisterID::fromRaw(i);
            if (assigned(reg) && index(reg) == n)
                return true;
        }
        return false;
    }
};

class AutoPreserveAcrossSyncAndKill;

} /* namespace mjit */
} /* namespace js */

#endif /* jsjaeger_framestate_h__ */


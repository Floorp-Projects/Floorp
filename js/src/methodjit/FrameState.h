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

namespace js {
namespace mjit {

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
    typedef JSC::MacroAssembler::RegisterID RegisterID;
    typedef JSC::MacroAssembler::Address Address;
    typedef JSC::MacroAssembler::Jump Jump;
    typedef JSC::MacroAssembler::Imm32 Imm32;

    struct Tracker {
        Tracker()
          : entries(NULL), nentries(0)
        { }

        void add(uint32 index) {
            entries[nentries++] = index;
        }

        void reset() {
            nentries = 0;
        }

        uint32 operator [](uint32 n) const {
            return entries[n];
        }

        uint32 *entries;
        uint32 nentries;
    };

    struct RegisterState {
        RegisterState()
        { }

        RegisterState(FrameEntry *fe, RematInfo::RematType type, bool weak)
          : fe(fe), type(type), weak(weak)
        { }

        /* FrameEntry owning this register, or NULL if not owned by a frame. */
        FrameEntry *fe;
        
        /* Part of the FrameEntry that owns the FE. */
        RematInfo::RematType type;

        /* Weak means it is easily spillable. */
        bool weak;
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
    inline void pushSyncedType(JSValueMask32 tag);

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
    inline void pushTypedPayload(JSValueMask32 tag, RegisterID payload);

    /*
     * Pushes a known type and allocated payload onto the operation stack.
     * This must be used when the type is known, but cannot be propagated
     * because it is not known to be correct at a slow-path merge point.
     */
    inline void pushUntypedPayload(JSValueMask32 tag, RegisterID payload);

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
     * Allocates a temporary register for a FrameEntry's type. The register
     * can be spilled or clobbered by the frame. The compiler may only operate
     * on it temporarily, and must take care not to clobber it.
     */
    inline RegisterID tempRegForType(FrameEntry *fe);

    /*
     * Returns a register that is guaranteed to contain the frame entry's
     * data payload. The compiler may not modify the contents of the register,
     * though it may explicitly free it.
     */
    inline RegisterID tempRegForData(FrameEntry *fe);

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
     * Allocates a register for a FrameEntry's data, such that the compiler
     * can modify it in-place. The actual FE is not modified.
     */
    RegisterID copyData(FrameEntry *fe);

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
     * Returns a FrameEntry * for a slot on the operation stack.
     */
    inline FrameEntry *peek(int32 depth);

    /*
     * Fully stores a FrameEntry at an arbitrary address. popHint specifies
     * how hard the register allocator should try to keep the FE in registers.
     */
    void storeTo(FrameEntry *fe, Address address, bool popHint);

    /*
     * Restores state from a slow path.
     */
    void merge(Assembler &masm, uint32 ivD) const;

    /*
     * Writes unsynced stores to an arbitrary buffer.
     */
    void sync(Assembler &masm) const;

    /*
     * Syncs all outstanding stores to memory and possibly kills regs in the
     * process.
     */
    void syncAndKill(uint32 mask); 

    /*
     * Clear all tracker entries, syncing all outstanding stores in the process.
     * The stack depth is in case some merge points' edges did not immediately
     * precede the current instruction.
     */
    inline void forgetEverything(uint32 newStackDepth);

    /*
     * Same as above, except the stack depth is not changed. This is used for
     * branching opcodes.
     */
    void forgetEverything();

    /*
     * Mark an existing slot with a type.
     */
    inline void learnType(FrameEntry *fe, JSValueMask32 tag);

    /*
     * Helper function. Tests if a slot's type is an integer. Condition should
     * be Equal or NotEqual.
     */
    inline Jump testInt32(Assembler::Condition cond, FrameEntry *fe);

    /*
     * Returns the current stack depth of the frame.
     */
    uint32 stackDepth() const { return sp - spBase; }
    uint32 tos() const { return sp - base; }

#ifdef DEBUG
    void assertValidRegisterState() const;
#endif

    Address addressOf(const FrameEntry *fe) const;

  private:
    inline RegisterID alloc();
    inline RegisterID alloc(FrameEntry *fe, RematInfo::RematType type, bool weak);
    inline void forgetReg(RegisterID reg);
    RegisterID evictSomething();
    inline FrameEntry *rawPush();
    inline FrameEntry *addToTracker(uint32 index);
    inline void syncType(const FrameEntry *fe, Assembler &masm) const;
    inline void syncData(const FrameEntry *fe, Assembler &masm) const;

    uint32 indexOf(int32 depth) {
        return uint32((sp + depth) - base);
    }

  private:
    JSContext *cx;
    JSScript *script;
    uint32 nargs;
    Assembler &masm;

    /* All allocated registers. */
    Registers freeRegs;

    /* Cache of FrameEntry objects. */
    FrameEntry *entries;

    /* Base pointer of the FrameEntry vector. */
    FrameEntry **base;

    /* Base pointer for arguments. */
    FrameEntry **args;

    /* Base pointer for local variables. */
    FrameEntry **locals;

    /* Base pointer for the stack. */
    FrameEntry **spBase;

    /* Dynamic stack pointer. */
    FrameEntry **sp;

    /* Vector of tracked slot indexes. */
    Tracker tracker;

    /*
     * Register ownership state. This can't be used alone; to find whether an
     * entry is active, you must check the allocated registers.
     */
    RegisterState regstate[Assembler::TotalRegisters];
};

} /* namespace mjit */
} /* namespace js */

#endif /* jsjaeger_framestate_h__ */


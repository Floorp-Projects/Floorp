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
 *   David Mandelin <dmandelin@mozilla.com>
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

#if !defined jsjaeger_baseassembler_h__ && defined JS_METHODJIT
#define jsjaeger_baseassembler_h__

#include "jscntxt.h"
#include "jstl.h"
#include "assembler/assembler/MacroAssemblerCodeRef.h"
#include "assembler/assembler/MacroAssembler.h"
#include "assembler/assembler/LinkBuffer.h"
#include "assembler/moco/MocoStubs.h"
#include "methodjit/MethodJIT.h"
#include "methodjit/MachineRegs.h"
#include "CodeGenIncludes.h"
#include "jsobjinlines.h"
#include "jsscopeinlines.h"

namespace js {
namespace mjit {

class MaybeRegisterID {
    typedef JSC::MacroAssembler::RegisterID RegisterID;

  public:
    MaybeRegisterID()
      : reg_(Registers::ReturnReg), set(false)
    { }

    MaybeRegisterID(RegisterID reg)
      : reg_(reg), set(true)
    { }

    inline RegisterID reg() const { JS_ASSERT(set); return reg_; }
    inline void setReg(const RegisterID r) { reg_ = r; set = true; }
    inline bool isSet() const { return set; }

    MaybeRegisterID & operator =(const MaybeRegisterID &other) {
        set = other.set;
        reg_ = other.reg_;
        return *this;
    }

    MaybeRegisterID & operator =(RegisterID r) {
        setReg(r);
        return *this;
    }

  private:
    RegisterID reg_;
    bool set;
};

// Represents an int32 property name in generated code, which must be either
// a RegisterID or a constant value.
struct Int32Key {
    typedef JSC::MacroAssembler::RegisterID RegisterID;

    MaybeRegisterID reg_;
    int32 index_;

    Int32Key() : index_(0) { }

    static Int32Key FromRegister(RegisterID reg) {
        Int32Key key;
        key.reg_ = reg;
        return key;
    }
    static Int32Key FromConstant(int32 index) {
        Int32Key key;
        key.index_ = index;
        return key;
    }

    int32 index() const {
        JS_ASSERT(!reg_.isSet());
        return index_;
    }

    RegisterID reg() const { return reg_.reg(); }
    bool isConstant() const { return !reg_.isSet(); }
};

class MaybeJump {
    typedef JSC::MacroAssembler::Jump Jump;
  public:
    MaybeJump()
      : set(false)
    { }

    inline Jump getJump() const { JS_ASSERT(set); return jump; }
    inline Jump get() const { JS_ASSERT(set); return jump; }
    inline void setJump(const Jump &j) { jump = j; set = true; }
    inline bool isSet() const { return set; }

    inline MaybeJump &operator=(Jump j) { setJump(j); return *this; }

  private:
    Jump jump;
    bool set;
};

struct FrameAddress : JSC::MacroAssembler::Address
{
    FrameAddress(int32 offset)
      : Address(JSC::MacroAssembler::stackPointerRegister, offset)
    { }
};

struct ImmIntPtr : public JSC::MacroAssembler::ImmPtr
{
    ImmIntPtr(intptr_t val)
      : ImmPtr(reinterpret_cast<void*>(val))
    { }
};

struct StackMarker {
    uint32 base;
    uint32 bytes;

    StackMarker(uint32 base, uint32 bytes)
      : base(base), bytes(bytes)
    { }
};

class Assembler : public ValueAssembler
{
    struct CallPatch {
        CallPatch(Call cl, void *fun)
          : call(cl), fun(fun)
        { }

        Call call;
        JSC::FunctionPtr fun;
    };

    /* Need a temp reg that is not ArgReg1. */
#if defined(JS_CPU_X86) || defined(JS_CPU_X64)
    static const RegisterID ClobberInCall = JSC::X86Registers::ecx;
#elif defined(JS_CPU_ARM)
    static const RegisterID ClobberInCall = JSC::ARMRegisters::r2;
#endif

    /* :TODO: OOM */
    Label startLabel;
    Vector<CallPatch, 64, SystemAllocPolicy> callPatches;

    // Registers that can be clobbered during a call sequence.
    Registers   availInCall;

    // Extra number of bytes that can be used for storing structs/references
    // across calls.
    uint32      extraStackSpace;

    // Calling convention used by the currently in-progress call.
    Registers::CallConvention callConvention;

    // Amount of stack space reserved for the currently in-progress call. This
    // includes alignment and parameters.
    uint32      stackAdjust;

    // Debug flag to make sure calls do not nest.
#ifdef DEBUG
    bool        callIsAligned;
#endif

  public:
    Assembler()
      : callPatches(SystemAllocPolicy()),
        extraStackSpace(0),
        stackAdjust(0)
#ifdef DEBUG
        , callIsAligned(false)
#endif
    {
        startLabel = label();
    }

    /* Total number of floating-point registers. */
    static const uint32 TotalFPRegisters = FPRegisters::TotalFPRegisters;

    /* Register pair storing returned type/data for calls. */
#if defined(JS_CPU_X86) || defined(JS_CPU_X64)
static const JSC::MacroAssembler::RegisterID JSReturnReg_Type  = JSC::X86Registers::ecx;
static const JSC::MacroAssembler::RegisterID JSReturnReg_Data  = JSC::X86Registers::edx;
static const JSC::MacroAssembler::RegisterID JSParamReg_Argc   = JSC::X86Registers::ecx;
#elif defined(JS_CPU_ARM)
static const JSC::MacroAssembler::RegisterID JSReturnReg_Type  = JSC::ARMRegisters::r2;
static const JSC::MacroAssembler::RegisterID JSReturnReg_Data  = JSC::ARMRegisters::r1;
static const JSC::MacroAssembler::RegisterID JSParamReg_Argc   = JSC::ARMRegisters::r1;
#endif

    size_t distanceOf(Label l) {
        return differenceBetween(startLabel, l);
    }

    void load32FromImm(void *ptr, RegisterID reg) {
        load32(ptr, reg);
    }

    void loadShape(RegisterID obj, RegisterID shape) {
        load32(Address(obj, offsetof(JSObject, objShape)), shape);
    }

    Jump guardShape(RegisterID objReg, JSObject *obj) {
        return branch32(NotEqual, Address(objReg, offsetof(JSObject, objShape)),
                        Imm32(obj->shape()));
    }

    Jump testFunction(Condition cond, RegisterID fun) {
        return branchPtr(cond, Address(fun, offsetof(JSObject, clasp)),
                         ImmPtr(&js_FunctionClass));
    }

    /*
     * Finds and returns the address of a known object and slot.
     */
    Address objSlotRef(JSObject *obj, RegisterID reg, uint32 slot) {
        move(ImmPtr(&obj->slots), reg);
        loadPtr(reg, reg);
        return Address(reg, slot * sizeof(Value));
    }

#ifdef JS_CPU_X86
    void idiv(RegisterID reg) {
        m_assembler.cdq();
        m_assembler.idivl_r(reg);
    }
#endif

    /* Prepare for a call that might THROW. */
    void *getFallibleCallTarget(void *fun) {
#ifdef JS_CPU_ARM
        /*
         * Insert a veneer for ARM to allow it to catch exceptions. There is no
         * reliable way to determine the location of the return address on the
         * stack, so a typical C(++) return address cannot be hijacked.
         *
         * We put the real target address into IP, as this won't conflict with
         * the EABI argument-passing mechanism. JaegerStubVeneer is responsible
         * for calling 'fun' (in IP) and catching exceptions.
         *
         * Note that we must use 'moveWithPatch' here, rather than 'move',
         * because 'move' might try to optimize the constant load, and we need a
         * consistent code sequence for patching.
         */
        moveWithPatch(Imm32(intptr_t(fun)), JSC::ARMRegisters::ip);

        return JS_FUNC_TO_DATA_PTR(void *, JaegerStubVeneer);
#else
        /*
         * Architectures that push the return address to an easily-determined
         * location on the stack can hijack C++'s return mechanism by overwriting
         * that address, so a veneer is not required.
         */
        return fun;
#endif
    }

    static inline uint32 align(uint32 bytes, uint32 alignment) {
        return (alignment - (bytes % alignment)) % alignment;
    }

    // Specifies extra stack space that is available across a call, for storing
    // large parameters (structs) or returning values via references. All extra
    // stack space must be reserved up-front, and is aligned on an 8-byte
    // boundary.
    //
    // Returns an offset that can be used to index into this stack 
    StackMarker allocStack(uint32 bytes, uint32 alignment = 4) {
        bytes += align(bytes + extraStackSpace, alignment);
        subPtr(Imm32(bytes), stackPointerRegister);
        extraStackSpace += bytes;
        return StackMarker(extraStackSpace, bytes);
    }

    // Similar to allocStack(), but combines it with a push().
    void saveReg(RegisterID reg) {
        push(reg);
        extraStackSpace += sizeof(void *);
    }

    // Similar to freeStack(), but combines it with a pop().
    void restoreReg(RegisterID reg) {
        JS_ASSERT(extraStackSpace >= sizeof(void *));
        extraStackSpace -= sizeof(void *);
        pop(reg);
    }

    static const uint32 StackAlignment = 16;

    static inline uint32 alignForCall(uint32 stackBytes) {
#if defined(JS_CPU_X86) || defined(JS_CPU_X64)
        // If StackAlignment is a power of two, % is just two shifts.
        // 16 - (x % 16) gives alignment, extra % 16 handles total == 0.
        return align(stackBytes, StackAlignment);
#else
        return 0;
#endif
    }

    // Some platforms require stack manipulation before making stub calls.
    // When using THROW/V, the return address is replaced, meaning the
    // stack de-adjustment will not have occured. JaegerThrowpoline accounts
    // for this. For stub calls, which are always invoked as if they use
    // two parameters, the stack adjustment is constant.
    //
    // When using callWithABI() manually, for example via an IC, it might
    // be necessary to jump directly to JaegerThrowpoline. In this case,
    // the constant is provided here in order to appropriately adjust the
    // stack.
#ifdef _WIN64
    static const uint32 ReturnStackAdjustment = 32;
#elif defined(JS_CPU_X86) && defined(JS_NO_FASTCALL)
    static const uint32 ReturnStackAdjustment = 16;
#else
    static const uint32 ReturnStackAdjustment = 0;
#endif

    void throwInJIT() {
        if (ReturnStackAdjustment)
            subPtr(Imm32(ReturnStackAdjustment), stackPointerRegister);
        move(ImmPtr(JS_FUNC_TO_DATA_PTR(void *, JaegerThrowpoline)), Registers::ReturnReg);
        jump(Registers::ReturnReg);
    }

    // Windows x64 requires extra space in between calls.
#ifdef _WIN64
    static const uint32 ShadowStackSpace = 32;
#else
    static const uint32 ShadowStackSpace = 0;
#endif

    // Prepare the stack for a call sequence. This must be called AFTER all
    // volatile regs have been saved, and BEFORE pushArg() is used. The stack
    // is assumed to be aligned to 16-bytes plus any pushes that occured via
    // saveRegs().
    //
    // During a call sequence all registers are "owned" by the Assembler.
    // Attempts to perform loads, nested calls, or anything that can clobber
    // a register, is asking for breaking on some platform or some situation.
    // Be careful to limit to storeArg() during setupABICall.
    void setupABICall(Registers::CallConvention convention, uint32 generalArgs) {
        JS_ASSERT(!callIsAligned);

        uint32 numArgRegs = Registers::numArgRegs(convention);
        uint32 pushCount = (generalArgs > numArgRegs)
                           ? generalArgs - numArgRegs
                           : 0;

        // Assume all temporary regs are available to clobber.
        availInCall = Registers::TempRegs;

        // Find the total number of bytes the stack will have been adjusted by,
        // in order to compute alignment.
        uint32 total = (pushCount * sizeof(void *)) +
                       extraStackSpace;

        stackAdjust = (pushCount * sizeof(void *)) +
                      alignForCall(total);

#ifdef _WIN64
        // Windows x64 ABI requires 32 bytes of "shadow space" for the callee
        // to spill its parameters.
        stackAdjust += ShadowStackSpace;
#endif

        if (stackAdjust)
            subPtr(Imm32(stackAdjust), stackPointerRegister);

        callConvention = convention;
#ifdef DEBUG
        callIsAligned = true;
#endif
    }

    // Computes an interior pointer into VMFrame during a call.
    Address vmFrameOffset(uint32 offs) {
        return Address(stackPointerRegister, stackAdjust + extraStackSpace + offs);
    }

    // Get an Address to the extra space already allocated before the call.
    Address addressOfExtra(const StackMarker &marker) {
        // Stack looks like this:
        //   extraStackSpace
        //   stackAdjust
        // To get to the requested offset into extraStackSpace, we can walk
        // up to the top of the extra stack space, then subtract |offs|.
        //
        // Note that it's not required we're in a call - stackAdjust can be 0.
        JS_ASSERT(marker.base <= extraStackSpace);
        return Address(stackPointerRegister, stackAdjust + extraStackSpace - marker.base);
    }

    // This is an internal function only for use inside a setupABICall(),
    // callWithABI() sequence, and only for arguments known to fit in
    // registers.
    Address addressOfArg(uint32 i) {
        uint32 numArgRegs = Registers::numArgRegs(callConvention);
        JS_ASSERT(i >= numArgRegs);

        // Note that shadow space is for the callee to spill, and thus it must
        // be skipped when writing its arguments.
        int32 spOffset = ((i - numArgRegs) * sizeof(void *)) + ShadowStackSpace;
        return Address(stackPointerRegister, spOffset);
    }

    // Push an argument for a call.
    void storeArg(uint32 i, RegisterID reg) {
        JS_ASSERT(callIsAligned);
        RegisterID to;
        if (Registers::regForArg(callConvention, i, &to)) {
            if (reg != to)
                move(reg, to);
            availInCall.takeRegUnchecked(to);
        } else {
            storePtr(reg, addressOfArg(i));
        }
    }

    // This variant can clobber temporary registers. However, it will NOT
    // clobber any registers that have already been set via storeArg().
    void storeArg(uint32 i, Address address) {
        JS_ASSERT(callIsAligned);
        RegisterID to;
        if (Registers::regForArg(callConvention, i, &to)) {
            loadPtr(address, to);
            availInCall.takeRegUnchecked(to);
        } else if (!availInCall.empty()) {
            // Memory-to-memory, and there is a temporary register free.
            RegisterID reg = availInCall.takeAnyReg();
            loadPtr(address, reg);
            storeArg(i, reg);
            availInCall.putReg(reg);
        } else {
            // Memory-to-memory, but no temporary registers are free.
            // This shouldn't happen on any platforms, because
            // (TempRegs) Union (ArgRegs) != 0
            JS_NOT_REACHED("too much reg pressure");
        }
    }

    // This variant can clobber temporary registers. However, it will NOT
    // clobber any registers that have already been set via storeArg().
    void storeArgAddr(uint32 i, Address address) {
        JS_ASSERT(callIsAligned);
        RegisterID to;
        if (Registers::regForArg(callConvention, i, &to)) {
            lea(address, to);
            availInCall.takeRegUnchecked(to);
        } else if (!availInCall.empty()) {
            // Memory-to-memory, and there is a temporary register free.
            RegisterID reg = availInCall.takeAnyReg();
            lea(address, reg);
            storeArg(i, reg);
            availInCall.putReg(reg);
        } else {
            // Memory-to-memory, but no temporary registers are free.
            // This shouldn't happen on any platforms, because
            // (TempRegs) Union (ArgRegs) != 0
            JS_NOT_REACHED("too much reg pressure");
        }
    }

    void storeArg(uint32 i, Imm32 imm) {
        JS_ASSERT(callIsAligned);
        RegisterID to;
        if (Registers::regForArg(callConvention, i, &to)) {
            move(imm, to);
            availInCall.takeRegUnchecked(to);
        } else {
            store32(imm, addressOfArg(i));
        }
    }

    // High-level call helper, given an optional function pointer and a
    // calling convention. setupABICall() must have been called beforehand,
    // as well as each numbered argument stored with storeArg().
    //
    // After callWithABI(), the call state is reset, so a new call may begin.
    Call callWithABI(void *fun, bool canThrow) {
        // [Bug 614953]: This can only be made conditional once the ARM back-end
        // is able to distinguish and patch both call sequences. Other
        // architecutres are unaffected regardless.
        //if (canThrow) {
            // Some platforms (such as ARM) require a call veneer if the target
            // might THROW. For other platforms, getFallibleCallTarget does
            // nothing.
            fun = getFallibleCallTarget(fun);
        //}

        JS_ASSERT(callIsAligned);

        Call cl = call();
        callPatches.append(CallPatch(cl, fun));

        if (stackAdjust)
            addPtr(Imm32(stackAdjust), stackPointerRegister);

        stackAdjust = 0;

#ifdef DEBUG
        callIsAligned = false;
#endif
        return cl;
    }

    // Frees stack space allocated by allocStack().
    void freeStack(const StackMarker &mark) {
        JS_ASSERT(!callIsAligned);
        JS_ASSERT(mark.bytes <= extraStackSpace);

        extraStackSpace -= mark.bytes;
        addPtr(Imm32(mark.bytes), stackPointerRegister);
    }

    // Wrap AbstractMacroAssembler::getLinkerCallReturnOffset which is protected.
    unsigned callReturnOffset(Call call) {
        return getLinkerCallReturnOffset(call);
    }


#define STUB_CALL_TYPE(type)                                                \
    Call callWithVMFrame(type stub, jsbytecode *pc, uint32 fd) {            \
        return fallibleVMCall(JS_FUNC_TO_DATA_PTR(void *, stub), pc, fd);   \
    }

    STUB_CALL_TYPE(JSObjStub);
    STUB_CALL_TYPE(VoidPtrStubUInt32);
    STUB_CALL_TYPE(VoidStubUInt32);
    STUB_CALL_TYPE(VoidStub);

#undef STUB_CALL_TYPE

    void setupInfallibleVMFrame(int32 frameDepth) {
        // |frameDepth < 0| implies ic::SplatApplyArgs has been called which
        // means regs.sp has already been set in the VMFrame.
        if (frameDepth >= 0) {
            // sp = fp->slots() + frameDepth
            // regs->sp = sp
            addPtr(Imm32(sizeof(JSStackFrame) + frameDepth * sizeof(jsval)),
                   JSFrameReg,
                   ClobberInCall);
            storePtr(ClobberInCall, FrameAddress(offsetof(VMFrame, regs.sp)));
        }

        // The JIT has moved Arg1 already, and we've guaranteed to not clobber
        // it. Move ArgReg0 into place now. setupFallibleVMFrame will not
        // clobber it either.
        move(MacroAssembler::stackPointerRegister, Registers::ArgReg0);
    }

    void setupFallibleVMFrame(jsbytecode *pc, int32 frameDepth) {
        setupInfallibleVMFrame(frameDepth);

        /* regs->fp = fp */
        storePtr(JSFrameReg, FrameAddress(offsetof(VMFrame, regs.fp)));

        /* PC -> regs->pc :( */
        storePtr(ImmPtr(pc),
                 FrameAddress(offsetof(VMFrame, regs) + offsetof(JSFrameRegs, pc)));
    }

    // An infallible VM call is a stub call (taking a VMFrame & and one
    // optional parameter) that does not need |pc| and |fp| updated, since
    // the call is guaranteed to not fail. However, |sp| is always coherent.
    Call infallibleVMCall(void *ptr, int32 frameDepth) {
        setupInfallibleVMFrame(frameDepth);
        return wrapVMCall(ptr);
    }

    // A fallible VM call is a stub call (taking a VMFrame & and one optional
    // parameter) that needs the entire VMFrame to be coherent, meaning that
    // |pc| and |fp| are guaranteed to be up-to-date.
    Call fallibleVMCall(void *ptr, jsbytecode *pc, int32 frameDepth) {
        setupFallibleVMFrame(pc, frameDepth);
        return wrapVMCall(ptr);
    }

    Call wrapVMCall(void *ptr) {
        JS_ASSERT(!callIsAligned);

        // Every stub call has at most two arguments.
        setupABICall(Registers::FastCall, 2);

        // On x86, if JS_NO_FASTCALL is present, these will result in actual
        // pushes to the stack, which the caller will clean up. Otherwise,
        // they'll be ignored because the registers fit into the calling
        // sequence.
        storeArg(0, Registers::ArgReg0);
        storeArg(1, Registers::ArgReg1);

        // [Bug 614953]: The second argument, 'canThrow', can be set to 'false'
        // for infallibleVMCall invocations. However, this changes the call
        // sequence on ARM, and the ARM repatcher cannot currently distinguish
        // between the two sequences. The argument does not affect the code
        // generated by x86 or amd64.
        return callWithABI(ptr, true);
    }

    void finalize(JSC::LinkBuffer &linker) {
        for (size_t i = 0; i < callPatches.length(); i++) {
            CallPatch &patch = callPatches[i];
            linker.link(patch.call, JSC::FunctionPtr(patch.fun));
        }
    }

    struct FastArrayLoadFails {
        Jump rangeCheck;
        Jump holeCheck;
    };

    Jump guardArrayCapacity(RegisterID objReg, const Int32Key &key) {
        Address capacity(objReg, offsetof(JSObject, capacity));
        if (key.isConstant()) {
            JS_ASSERT(key.index() >= 0);
            return branch32(BelowOrEqual, payloadOf(capacity), Imm32(key.index()));
        }
        return branch32(BelowOrEqual, payloadOf(capacity), key.reg());
    }

    // Load a jsval from an array slot, given a key. |objReg| is clobbered.
    FastArrayLoadFails fastArrayLoad(RegisterID objReg, const Int32Key &key,
                                     RegisterID typeReg, RegisterID dataReg) {
        JS_ASSERT(objReg != typeReg);

        FastArrayLoadFails fails;
        fails.rangeCheck = guardArrayCapacity(objReg, key);

        RegisterID dslotsReg = objReg;
        loadPtr(Address(objReg, offsetof(JSObject, slots)), dslotsReg);

        // Load the slot out of the array.
        if (key.isConstant()) {
            Address slot(objReg, key.index() * sizeof(Value));
            fails.holeCheck = fastArrayLoadSlot(slot, typeReg, dataReg);
        } else {
            BaseIndex slot(objReg, key.reg(), JSVAL_SCALE);
            fails.holeCheck = fastArrayLoadSlot(slot, typeReg, dataReg);
        }

        return fails;
    }

    void loadObjClass(RegisterID objReg, RegisterID destReg) {
        loadPtr(Address(objReg, offsetof(JSObject, clasp)), destReg);
    }

    Jump testClass(Condition cond, RegisterID claspReg, js::Class *clasp) {
        return branchPtr(cond, claspReg, ImmPtr(clasp));
    }

    Jump testObjClass(Condition cond, RegisterID objReg, js::Class *clasp) {
        return branchPtr(cond, Address(objReg, offsetof(JSObject, clasp)), ImmPtr(clasp));
    }

    void rematPayload(const StateRemat &remat, RegisterID reg) {
        if (remat.inMemory())
            loadPayload(remat.address(), reg);
        else
            move(remat.reg(), reg);
    }

    void loadDynamicSlot(RegisterID objReg, uint32 slot,
                         RegisterID typeReg, RegisterID dataReg) {
        loadPtr(Address(objReg, offsetof(JSObject, slots)), dataReg);
        loadValueAsComponents(Address(dataReg, slot * sizeof(Value)), typeReg, dataReg);
    }

    void loadObjProp(JSObject *obj, RegisterID objReg,
                     const js::Shape *shape,
                     RegisterID typeReg, RegisterID dataReg)
    {
        if (shape->isMethod())
            loadValueAsComponents(ObjectValue(shape->methodObject()), typeReg, dataReg);
        else if (obj->hasSlotsArray())
            loadDynamicSlot(objReg, shape->slot, typeReg, dataReg);
        else
            loadInlineSlot(objReg, shape->slot, typeReg, dataReg);
    }

    static uint32 maskAddress(Address address) {
        return Registers::maskReg(address.base);
    }

    static uint32 maskAddress(BaseIndex address) {
        return Registers::maskReg(address.base) |
               Registers::maskReg(address.index);
    }
};

/* Return f<true> if the script is strict mode code, f<false> otherwise. */
#define STRICT_VARIANT(f)                                                     \
    (FunctionTemplateConditional(script->strictModeCode,                      \
                                 f<true>, f<false>))

/* Save some typing. */
static const JSC::MacroAssembler::RegisterID JSReturnReg_Type = Assembler::JSReturnReg_Type;
static const JSC::MacroAssembler::RegisterID JSReturnReg_Data = Assembler::JSReturnReg_Data;
static const JSC::MacroAssembler::RegisterID JSParamReg_Argc  = Assembler::JSParamReg_Argc;

struct FrameFlagsAddress : JSC::MacroAssembler::Address
{
    FrameFlagsAddress()
      : Address(JSFrameReg, JSStackFrame::offsetOfFlags())
    {}
};

class PreserveRegisters {
    typedef JSC::MacroAssembler::RegisterID RegisterID;

    Assembler   &masm;
    uint32      count;
    RegisterID  regs[JSC::MacroAssembler::TotalRegisters];

  public:
    PreserveRegisters(Assembler &masm) : masm(masm), count(0) { }
    ~PreserveRegisters() { JS_ASSERT(!count); }

    void preserve(Registers mask) {
        JS_ASSERT(!count);

        while (!mask.empty()) {
            RegisterID reg = mask.takeAnyReg();
            regs[count++] = reg;
            masm.saveReg(reg);
        }
    }

    void restore() {
        while (count)
            masm.restoreReg(regs[--count]);
    }
};

} /* namespace mjit */
} /* namespace js */

#endif


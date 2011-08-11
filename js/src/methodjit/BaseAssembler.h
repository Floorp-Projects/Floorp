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

    struct DoublePatch {
        double d;
        DataLabelPtr label;
    };

    /* Need a temp reg that is not ArgReg1. */
#if defined(JS_CPU_X86) || defined(JS_CPU_X64)
    static const RegisterID ClobberInCall = JSC::X86Registers::ecx;
#elif defined(JS_CPU_ARM)
    static const RegisterID ClobberInCall = JSC::ARMRegisters::r2;
#elif defined(JS_CPU_SPARC)
    static const RegisterID ClobberInCall = JSC::SparcRegisters::l1;
#endif

    /* :TODO: OOM */
    Label startLabel;
    Vector<CallPatch, 64, SystemAllocPolicy> callPatches;
    Vector<DoublePatch, 16, SystemAllocPolicy> doublePatches;

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
        availInCall(0),
        extraStackSpace(0),
        stackAdjust(0)
#ifdef DEBUG
        , callIsAligned(false)
#endif
    {
        startLabel = label();
    }

    /* Register pair storing returned type/data for calls. */
#if defined(JS_CPU_X86) || defined(JS_CPU_X64)
static const JSC::MacroAssembler::RegisterID JSReturnReg_Type  = JSC::X86Registers::edi;
static const JSC::MacroAssembler::RegisterID JSReturnReg_Data  = JSC::X86Registers::esi;
static const JSC::MacroAssembler::RegisterID JSParamReg_Argc   = JSC::X86Registers::ecx;
#elif defined(JS_CPU_ARM)
static const JSC::MacroAssembler::RegisterID JSReturnReg_Type  = JSC::ARMRegisters::r5;
static const JSC::MacroAssembler::RegisterID JSReturnReg_Data  = JSC::ARMRegisters::r4;
static const JSC::MacroAssembler::RegisterID JSParamReg_Argc   = JSC::ARMRegisters::r1;
#elif defined(JS_CPU_SPARC)
static const JSC::MacroAssembler::RegisterID JSReturnReg_Type = JSC::SparcRegisters::l2;
static const JSC::MacroAssembler::RegisterID JSReturnReg_Data = JSC::SparcRegisters::l3;
static const JSC::MacroAssembler::RegisterID JSParamReg_Argc  = JSC::SparcRegisters::i2;
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
        move(ImmPtr(obj), reg);
        if (obj->isFixedSlot(slot)) {
            return Address(reg, JSObject::getFixedSlotOffset(slot));
        } else {
            loadPtr(Address(reg, JSObject::offsetOfSlots()), reg);
            return Address(reg, obj->dynamicSlotIndex(slot) * sizeof(Value));
        }
    }

#ifdef JS_CPU_X86
    void idiv(RegisterID reg) {
        m_assembler.cdq();
        m_assembler.idivl_r(reg);
    }

    void fastLoadDouble(RegisterID lo, RegisterID hi, FPRegisterID fpReg) {
        JS_ASSERT(fpReg != Registers::FPConversionTemp);
        if (MacroAssemblerX86Common::getSSEState() >= HasSSE4_1) {
            m_assembler.movd_rr(lo, fpReg);
            m_assembler.pinsrd_rr(hi, fpReg);
        } else {
            m_assembler.movd_rr(lo, fpReg);
            m_assembler.movd_rr(hi, Registers::FPConversionTemp);
            m_assembler.unpcklps_rr(Registers::FPConversionTemp, fpReg);
        }
    }
#endif

    /*
     * Move a register pair which may indicate either an int32 or double into fpreg,
     * converting to double in the int32 case.
     */
    void moveInt32OrDouble(RegisterID data, RegisterID type, Address address, FPRegisterID fpreg)
    {
#ifdef JS_CPU_X86
        fastLoadDouble(data, type, fpreg);
        Jump notInteger = testInt32(Assembler::NotEqual, type);
        convertInt32ToDouble(data, fpreg);
        notInteger.linkTo(label(), this);
#else
        Jump notInteger = testInt32(Assembler::NotEqual, type);
        convertInt32ToDouble(data, fpreg);
        Jump fallthrough = jump();
        notInteger.linkTo(label(), this);

        /* Store the components, then read it back out as a double. */
        storeValueFromComponents(type, data, address);
        loadDouble(address, fpreg);

        fallthrough.linkTo(label(), this);
#endif
    }

    /*
     * Move a memory address which contains either an int32 or double into fpreg,
     * converting to double in the int32 case.
     */
    template <typename T>
    void moveInt32OrDouble(T address, FPRegisterID fpreg)
    {
        Jump notInteger = testInt32(Assembler::NotEqual, address);
        convertInt32ToDouble(payloadOf(address), fpreg);
        Jump fallthrough = jump();
        notInteger.linkTo(label(), this);
        loadDouble(address, fpreg);
        fallthrough.linkTo(label(), this);
    }

    /* Ensure that an in-memory address is definitely a double. */
    void ensureInMemoryDouble(Address address)
    {
        Jump notInteger = testInt32(Assembler::NotEqual, address);
        convertInt32ToDouble(payloadOf(address), Registers::FPConversionTemp);
        storeDouble(Registers::FPConversionTemp, address);
        notInteger.linkTo(label(), this);
    }

    void negateDouble(FPRegisterID fpreg)
    {
#if defined JS_CPU_X86 || defined JS_CPU_X64
        static const uint64 DoubleNegMask = 0x8000000000000000ULL;
        loadDouble(&DoubleNegMask, Registers::FPConversionTemp);
        xorDouble(Registers::FPConversionTemp, fpreg);
#elif defined JS_CPU_ARM || defined JS_CPU_SPARC
        negDouble(fpreg, fpreg);
#endif
    }

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
#elif defined(JS_CPU_SPARC)
    static const uint32 ShadowStackSpace = 92;
#else
    static const uint32 ShadowStackSpace = 0;
#endif

#if defined(JS_CPU_SPARC)
    static const uint32 BaseStackSpace = 104;
#else
    static const uint32 BaseStackSpace = 0;
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
        return Address(stackPointerRegister, BaseStackSpace + stackAdjust + extraStackSpace - marker.base);
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
            RegisterID reg = availInCall.takeAnyReg().reg();
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
            RegisterID reg = availInCall.takeAnyReg().reg();
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


#define STUB_CALL_TYPE(type)                                                             \
    Call callWithVMFrame(bool inlining, type stub, jsbytecode *pc,                       \
                         DataLabelPtr *pinlined, uint32 fd) {                            \
        return fallibleVMCall(inlining, JS_FUNC_TO_DATA_PTR(void *, stub),               \
                              pc, pinlined, fd);                                         \
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
            addPtr(Imm32(sizeof(StackFrame) + frameDepth * sizeof(jsval)),
                   JSFrameReg,
                   ClobberInCall);
            storePtr(ClobberInCall, FrameAddress(offsetof(VMFrame, regs.sp)));
        }

        // The JIT has moved Arg1 already, and we've guaranteed to not clobber
        // it. Move ArgReg0 into place now. setupFallibleVMFrame will not
        // clobber it either.
        move(MacroAssembler::stackPointerRegister, Registers::ArgReg0);
    }

    void setupFallibleVMFrame(bool inlining, jsbytecode *pc,
                              DataLabelPtr *pinlined, int32 frameDepth) {
        setupInfallibleVMFrame(frameDepth);

        /* regs->fp = fp */
        storePtr(JSFrameReg, FrameAddress(VMFrame::offsetOfFp));

        /* PC -> regs->pc :( */
        storePtr(ImmPtr(pc), FrameAddress(offsetof(VMFrame, regs.pc)));

        if (inlining) {
            /* inlined -> regs->inlined :( */
            DataLabelPtr ptr = storePtrWithPatch(ImmPtr(NULL),
                                                 FrameAddress(VMFrame::offsetOfInlined));
            if (pinlined)
                *pinlined = ptr;
        }

        restoreStackBase();
    }

    void restoreStackBase() {
#if defined(JS_CPU_X86)
        /*
         * We use the %ebp base stack pointer on x86 to store the JSStackFrame.
         * Restore this before calling so that debuggers can construct a
         * coherent stack if we crash outside of JIT code.
         */
        JS_STATIC_ASSERT(JSFrameReg == JSC::X86Registers::ebp);
        move(JSC::X86Registers::esp, JSFrameReg);
        addPtr(Imm32(VMFrame::STACK_BASE_DIFFERENCE), JSFrameReg);
#endif
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
    // |pc|, |inlined| and |fp| are guaranteed to be up-to-date.
    Call fallibleVMCall(bool inlining, void *ptr, jsbytecode *pc,
                        DataLabelPtr *pinlined, int32 frameDepth) {
        setupFallibleVMFrame(inlining, pc, pinlined, frameDepth);
        Call call = wrapVMCall(ptr);

        // Restore the frame pointer from the VM.
        loadPtr(FrameAddress(VMFrame::offsetOfFp), JSFrameReg);

        return call;
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

    // Constant doubles can't be directly moved into a register, we need to put
    // them in memory and load them back with.
    void slowLoadConstantDouble(double d, FPRegisterID fpreg) {
        DoublePatch patch;
        patch.d = d;
        patch.label = loadDouble(NULL, fpreg);
        doublePatches.append(patch);
    }

    size_t numDoubles() { return doublePatches.length(); }

    void finalize(JSC::LinkBuffer &linker, double *doubleVec = NULL) {
        for (size_t i = 0; i < callPatches.length(); i++) {
            CallPatch &patch = callPatches[i];
            linker.link(patch.call, JSC::FunctionPtr(patch.fun));
        }
        for (size_t i = 0; i < doublePatches.length(); i++) {
            DoublePatch &patch = doublePatches[i];
            doubleVec[i] = patch.d;
            linker.patch(patch.label, &doubleVec[i]);
        }
    }

    struct FastArrayLoadFails {
        Jump rangeCheck;
        Jump holeCheck;
    };

    // Guard an array's capacity, length or initialized length.
    Jump guardArrayExtent(uint32 offset, RegisterID objReg, const Int32Key &key, Condition cond) {
        Address initlen(objReg, offset);
        if (key.isConstant()) {
            JS_ASSERT(key.index() >= 0);
            return branch32(cond, initlen, Imm32(key.index()));
        }
        return branch32(cond, initlen, key.reg());
    }

    // Load a jsval from an array slot, given a key. |objReg| is clobbered.
    FastArrayLoadFails fastArrayLoad(RegisterID objReg, const Int32Key &key,
                                     RegisterID typeReg, RegisterID dataReg) {
        JS_ASSERT(objReg != typeReg);

        FastArrayLoadFails fails;
        fails.rangeCheck = guardArrayExtent(offsetof(JSObject, initializedLength),
                                            objReg, key, BelowOrEqual);

        RegisterID dslotsReg = objReg;
        loadPtr(Address(objReg, JSObject::offsetOfSlots()), dslotsReg);

        // Load the slot out of the array.
        if (key.isConstant()) {
            Address slot(objReg, key.index() * sizeof(Value));
            fails.holeCheck = fastArrayLoadSlot(slot, true, typeReg, dataReg);
        } else {
            BaseIndex slot(objReg, key.reg(), JSVAL_SCALE);
            fails.holeCheck = fastArrayLoadSlot(slot, true, typeReg, dataReg);
        }

        return fails;
    }

    void storeKey(const Int32Key &key, Address address) {
        if (key.isConstant())
            store32(Imm32(key.index()), address);
        else
            store32(key.reg(), address);
    }

    void bumpKey(Int32Key &key, int32 delta) {
        if (key.isConstant())
            key.index_ += delta;
        else
            add32(Imm32(delta), key.reg());
    }

    void loadFrameActuals(JSFunction *fun, RegisterID reg) {
        /* Bias for the case where there was an arguments overflow. */
        load32(Address(JSFrameReg, StackFrame::offsetOfArgs()), reg);
        add32(Imm32(fun->nargs + 2), reg);
        Jump overflowArgs = branchTest32(Assembler::NonZero,
                                         Address(JSFrameReg, StackFrame::offsetOfFlags()),
                                         Imm32(StackFrame::OVERFLOW_ARGS));
        move(Imm32(fun->nargs), reg);
        overflowArgs.linkTo(label(), this);
        lshiftPtr(Imm32(3), reg);
        negPtr(reg);
        addPtr(JSFrameReg, reg);
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

    void branchValue(Condition cond, RegisterID reg, int32 value, RegisterID result)
    {
        if (Registers::maskReg(result) & Registers::SingleByteRegs) {
            set32(cond, reg, Imm32(value), result);
        } else {
            Jump j = branch32(cond, reg, Imm32(value));
            move(Imm32(0), result);
            Jump skip = jump();
            j.linkTo(label(), this);
            move(Imm32(1), result);
            skip.linkTo(label(), this);
        }
    }

    void branchValue(Condition cond, RegisterID lreg, RegisterID rreg, RegisterID result)
    {
        if (Registers::maskReg(result) & Registers::SingleByteRegs) {
            set32(cond, lreg, rreg, result);
        } else {
            Jump j = branch32(cond, lreg, rreg);
            move(Imm32(0), result);
            Jump skip = jump();
            j.linkTo(label(), this);
            move(Imm32(1), result);
            skip.linkTo(label(), this);
        }
    }

    void rematPayload(const StateRemat &remat, RegisterID reg) {
        if (remat.inMemory())
            loadPayload(remat.address(), reg);
        else
            move(remat.reg(), reg);
    }

    void loadDynamicSlot(RegisterID objReg, uint32 index,
                         RegisterID typeReg, RegisterID dataReg) {
        loadPtr(Address(objReg, JSObject::offsetOfSlots()), dataReg);
        loadValueAsComponents(Address(dataReg, index * sizeof(Value)), typeReg, dataReg);
    }

    void loadObjProp(JSObject *obj, RegisterID objReg,
                     const js::Shape *shape,
                     RegisterID typeReg, RegisterID dataReg)
    {
        if (shape->isMethod())
            loadValueAsComponents(ObjectValue(shape->methodObject()), typeReg, dataReg);
        else if (obj->isFixedSlot(shape->slot))
            loadInlineSlot(objReg, shape->slot, typeReg, dataReg);
        else
            loadDynamicSlot(objReg, obj->dynamicSlotIndex(shape->slot), typeReg, dataReg);
    }

#ifdef JS_METHODJIT_TYPED_ARRAY
    // Load a value from a typed array's packed data vector into dataReg.
    // This function expects the following combinations of typeReg, dataReg and tempReg:
    // 1) for all INT arrays other than UINT32:
    //    - dataReg is a GP-register
    //    - typeReg is optional
    //    - tempReg is not set
    // 2) for UINT32:
    //    - dataReg is either a FP-register or a GP-register
    //    - typeReg is set if dataReg is a GP-register
    //    - tempReg is set if dataReg is a FP-register
    // 3) for FLOAT32 and FLOAT64:
    //    - dataReg is either a FP-register or a GP-register
    //    - typeReg is set if dataReg is a GP-register
    //    - tempReg is not set
    template <typename T>
    void loadFromTypedArray(int atype, T address, MaybeRegisterID typeReg,
                            AnyRegisterID dataReg, MaybeRegisterID tempReg)
    {
        // If dataReg is an FP-register we don't use typeReg.
        JS_ASSERT_IF(dataReg.isFPReg(), !typeReg.isSet());

        // We only need tempReg for Uint32Array and only if dataReg is an FP-register.
        JS_ASSERT_IF(atype != js::TypedArray::TYPE_UINT32 || dataReg.isReg(), !tempReg.isSet());

        switch (atype) {
          case js::TypedArray::TYPE_INT8:
            load8SignExtend(address, dataReg.reg());
            if (typeReg.isSet())
                move(ImmType(JSVAL_TYPE_INT32), typeReg.reg());
            break;
          case js::TypedArray::TYPE_UINT8:
          case js::TypedArray::TYPE_UINT8_CLAMPED:
            load8ZeroExtend(address, dataReg.reg());
            if (typeReg.isSet())
                move(ImmType(JSVAL_TYPE_INT32), typeReg.reg());
            break;
          case js::TypedArray::TYPE_INT16:
            load16SignExtend(address, dataReg.reg());
            if (typeReg.isSet())
                move(ImmType(JSVAL_TYPE_INT32), typeReg.reg());
            break;
          case js::TypedArray::TYPE_UINT16:
            load16(address, dataReg.reg());
            if (typeReg.isSet())
                move(ImmType(JSVAL_TYPE_INT32), typeReg.reg());
            break;
          case js::TypedArray::TYPE_INT32:
            load32(address, dataReg.reg());
            if (typeReg.isSet())
                move(ImmType(JSVAL_TYPE_INT32), typeReg.reg());
            break;
          case js::TypedArray::TYPE_UINT32:
          {
            // For Uint32Array the result is either int32 or double.
            // If dataReg is a GP-register, load a double or int32 into dataReg/typeReg.
            // If dataReg is a FP-register, load the value as double.
            if (dataReg.isReg()) {
                load32(address, dataReg.reg());
                move(ImmType(JSVAL_TYPE_INT32), typeReg.reg());
                Jump safeInt = branch32(Assembler::Below, dataReg.reg(), Imm32(0x80000000));
                convertUInt32ToDouble(dataReg.reg(), Registers::FPConversionTemp);
                breakDouble(Registers::FPConversionTemp, typeReg.reg(), dataReg.reg());
                safeInt.linkTo(label(), this);
            } else {
                load32(address, tempReg.reg());
                convertUInt32ToDouble(tempReg.reg(), dataReg.fpreg());
            }
            break;
          }
          case js::TypedArray::TYPE_FLOAT32:
          case js::TypedArray::TYPE_FLOAT64:
          {
            FPRegisterID fpreg = dataReg.isReg()
                               ? Registers::FPConversionTemp
                               : dataReg.fpreg();
            if (atype == js::TypedArray::TYPE_FLOAT32)
                loadFloat(address, fpreg);
            else
                loadDouble(address, fpreg);
            // Make sure NaN gets canonicalized. If dataReg is not an FP-register
            // we have to use loadStaticDouble as we were probably called from an
            // IC and we can't use slowLoadConstantDouble.
            Jump notNaN = branchDouble(Assembler::DoubleEqual, fpreg, fpreg);
            if (dataReg.isReg())
                loadStaticDouble(&js_NaN, Registers::FPConversionTemp, dataReg.reg());
            else
                slowLoadConstantDouble(js_NaN, fpreg);
            notNaN.linkTo(label(), this);
            if (dataReg.isReg())
                breakDouble(Registers::FPConversionTemp, typeReg.reg(), dataReg.reg());
            break;
          }
        }
    }

    void loadFromTypedArray(int atype, RegisterID objReg, Int32Key key,
                            MaybeRegisterID typeReg, AnyRegisterID dataReg,
                            MaybeRegisterID tempReg)
    {
        int shift = TypedArray::slotWidth(atype);

        if (key.isConstant()) {
            Address addr(objReg, key.index() * shift);
            loadFromTypedArray(atype, addr, typeReg, dataReg, tempReg);
        } else {
            Assembler::Scale scale = Assembler::TimesOne;
            switch (shift) {
              case 2:
                scale = Assembler::TimesTwo;
                break;
              case 4:
                scale = Assembler::TimesFour;
                break;
              case 8:
                scale = Assembler::TimesEight;
                break;
            }
            BaseIndex addr(objReg, key.reg(), scale);
            loadFromTypedArray(atype, addr, typeReg, dataReg, tempReg);
        }
    }

    template <typename S, typename T>
    void storeToTypedIntArray(int atype, S src, T address)
    {
        switch (atype) {
          case js::TypedArray::TYPE_INT8:
          case js::TypedArray::TYPE_UINT8:
          case js::TypedArray::TYPE_UINT8_CLAMPED:
            store8(src, address);
            break;
          case js::TypedArray::TYPE_INT16:
          case js::TypedArray::TYPE_UINT16:
            store16(src, address);
            break;
          case js::TypedArray::TYPE_INT32:
          case js::TypedArray::TYPE_UINT32:
            store32(src, address);
            break;
          default:
            JS_NOT_REACHED("unknown int array type");
        }
    }

    template <typename S, typename T>
    void storeToTypedFloatArray(int atype, S src, T address)
    {
        if (atype == js::TypedArray::TYPE_FLOAT32)
            storeFloat(src, address);
        else
            storeDouble(src, address);
    }

    template <typename T>
    void storeToTypedArray(int atype, ValueRemat vr, T address)
    {
        if (atype == js::TypedArray::TYPE_FLOAT32 || atype == js::TypedArray::TYPE_FLOAT64) {
            if (vr.isConstant())
                storeToTypedFloatArray(atype, ImmDouble(vr.value().toDouble()), address);
            else
                storeToTypedFloatArray(atype, vr.fpReg(), address);
        } else {
            if (vr.isConstant())
                storeToTypedIntArray(atype, Imm32(vr.value().toInt32()), address);
            else
                storeToTypedIntArray(atype, vr.dataReg(), address);
        }
    }

    void storeToTypedArray(int atype, RegisterID objReg, Int32Key key, ValueRemat vr)
    {
        int shift = TypedArray::slotWidth(atype);
        if (key.isConstant()) {
            Address addr(objReg, key.index() * shift);
            storeToTypedArray(atype, vr, addr);
        } else {
            Assembler::Scale scale = Assembler::TimesOne;
            switch (shift) {
            case 2:
                scale = Assembler::TimesTwo;
                break;
            case 4:
                scale = Assembler::TimesFour;
                break;
            case 8:
                scale = Assembler::TimesEight;
                break;
            }
            BaseIndex addr(objReg, key.reg(), scale);
            storeToTypedArray(atype, vr, addr);
        }
    }

    void clampInt32ToUint8(RegisterID reg)
    {
        Jump j = branch32(Assembler::GreaterThanOrEqual, reg, Imm32(0));
        move(Imm32(0), reg);
        Jump done = jump();
        j.linkTo(label(), this);
        j = branch32(Assembler::LessThanOrEqual, reg, Imm32(255));
        move(Imm32(255), reg);
        j.linkTo(label(), this);
        done.linkTo(label(), this);
    }

    // Inline version of js_TypedArray_uint8_clamp_double.
    void clampDoubleToUint8(FPRegisterID fpReg, FPRegisterID fpTemp, RegisterID reg)
    {
        JS_ASSERT(fpTemp != Registers::FPConversionTemp);

        // <= 0 or NaN ==> 0
        zeroDouble(fpTemp);
        Jump positive = branchDouble(Assembler::DoubleGreaterThan, fpReg, fpTemp);
        move(Imm32(0), reg);
        Jump done1 = jump();

        // Add 0.5 and truncate.
        positive.linkTo(label(), this);
        slowLoadConstantDouble(0.5, fpTemp);
        addDouble(fpReg, fpTemp);
        Jump notInt = branchTruncateDoubleToInt32(fpTemp, reg);

        // > 255 ==> 255
        Jump inRange = branch32(Assembler::BelowOrEqual, reg, Imm32(255));
        notInt.linkTo(label(), this);
        move(Imm32(255), reg);
        Jump done2 = jump();

        // Check if we had a tie.
        inRange.linkTo(label(), this);
        convertInt32ToDouble(reg, Registers::FPConversionTemp);
        Jump done3 = branchDouble(Assembler::DoubleNotEqual, fpTemp, Registers::FPConversionTemp);

        // It was a tie. Mask out the ones bit to get an even value.
        // See js_TypedArray_uint8_clamp_double for the reasoning behind this.
        and32(Imm32(~1), reg);

        done1.linkTo(label(), this);
        done2.linkTo(label(), this);
        done3.linkTo(label(), this);
    }
#endif /* JS_METHODJIT_TYPED_ARRAY */

    Address objPropAddress(JSObject *obj, RegisterID objReg, uint32 slot)
    {
        if (obj->isFixedSlot(slot))
            return Address(objReg, JSObject::getFixedSlotOffset(slot));
        loadPtr(Address(objReg, JSObject::offsetOfSlots()), objReg);
        return Address(objReg, obj->dynamicSlotIndex(slot) * sizeof(Value));
    }

    static uint32 maskAddress(Address address) {
        return Registers::maskReg(address.base);
    }

    static uint32 maskAddress(BaseIndex address) {
        return Registers::maskReg(address.base) |
               Registers::maskReg(address.index);
    }

    /*
     * Generate code testing whether an in memory value at address has a type
     * in the specified set. Updates mismatches with any failure jumps. Assumes
     * no data registers are live.
     */
    bool generateTypeCheck(JSContext *cx, Address address,
                           types::TypeSet *types, Vector<Jump> *mismatches)
    {
        if (types->unknown())
            return true;

        Vector<Jump> matches(cx);

        if (types->hasType(types::Type::DoubleType())) {
            /* Type sets containing double also contain int. */
            if (!matches.append(testNumber(Assembler::Equal, address)))
                return false;
        } else if (types->hasType(types::Type::Int32Type())) {
            if (!matches.append(testInt32(Assembler::Equal, address)))
                return false;
        }

        if (types->hasType(types::Type::UndefinedType())) {
            if (!matches.append(testUndefined(Assembler::Equal, address)))
                return false;
        }

        if (types->hasType(types::Type::BooleanType())) {
            if (!matches.append(testBoolean(Assembler::Equal, address)))
                return false;
        }

        if (types->hasType(types::Type::StringType())) {
            if (!matches.append(testString(Assembler::Equal, address)))
                return false;
        }

        if (types->hasType(types::Type::NullType())) {
            if (!matches.append(testNull(Assembler::Equal, address)))
                return false;
        }

        unsigned count = 0;
        if (types->hasType(types::Type::AnyObjectType())) {
            if (!matches.append(testObject(Assembler::Equal, address)))
                return false;
        } else {
            count = types->getObjectCount();
        }

        if (count != 0) {
            if (!mismatches->append(testObject(Assembler::NotEqual, address)))
                return false;
            Registers tempRegs(Registers::AvailRegs);
            RegisterID reg = tempRegs.takeAnyReg().reg();

            loadPayload(address, reg);

            for (unsigned i = 0; i < count; i++) {
                JSObject *object = types->getSingleObject(i);
                if (object) {
                    if (!matches.append(branchPtr(Assembler::Equal, reg, ImmPtr(object))))
                        return false;
                }
            }

            loadPtr(Address(reg, JSObject::offsetOfType()), reg);

            for (unsigned i = 0; i < count; i++) {
                types::TypeObject *object = types->getTypeObject(i);
                if (object) {
                    if (!matches.append(branchPtr(Assembler::Equal, reg, ImmPtr(object))))
                        return false;
                }
            }
        }

        if (!mismatches->append(jump()))
            return false;

        for (unsigned i = 0; i < matches.length(); i++)
            matches[i].linkTo(label(), this);

        return true;
    }

    /*
     * Get a free object for the specified GC kind in compartment, writing it
     * to result and filling it in according to templateObject. Returns a jump
     * taken if a free thing was not retrieved.
     */
    Jump getNewObject(JSContext *cx, RegisterID result, JSObject *templateObject)
    {
        unsigned thingKind = templateObject->arenaHeader()->getThingKind();

        JS_ASSERT(thingKind >= gc::FINALIZE_OBJECT0 && thingKind <= gc::FINALIZE_OBJECT_LAST);
        size_t thingSize = gc::GCThingSizeMap[thingKind];

        JS_ASSERT(cx->typeInferenceEnabled());
        JS_ASSERT(!templateObject->hasSlotsArray());

#ifdef JS_GC_ZEAL
        if (cx->runtime->needZealousGC())
            return jump();
#endif

        /*
         * Inline FreeLists::getNext. Only the case where the current freelist
         * span is not empty is handled.
         */
        gc::FreeSpan *list = &cx->compartment->freeLists.lists[thingKind];

        loadPtr(&list->start, result);
        Jump jump = branchPtr(Assembler::Equal, AbsoluteAddress(&list->end), result);

        addPtr(Imm32(thingSize), result);
        storePtr(result, &list->start);

        /*
         * Fill in the blank object. Order doesn't matter here, from here
         * everything is infallible. Note that this bakes GC thing pointers
         * into the code without explicitly pinning them. With type inference
         * enabled, JIT code is collected on GC except when analysis or
         * compilation is active, in which case type objects won't be collected
         * but other things may be. The shape held by templateObject *must* be
         * pinned against GC either by the script or by some type object.
         */

        /*
         * Write out the slots pointer before readjusting the result register,
         * as for dense arrays this is relative to the JSObject itself.
         */
        if (templateObject->isDenseArray()) {
            JS_ASSERT(!templateObject->initializedLength);
            addPtr(Imm32(-thingSize + sizeof(JSObject)), result);
            storePtr(result, Address(result, -sizeof(JSObject) + JSObject::offsetOfSlots()));
            addPtr(Imm32(-(int)sizeof(JSObject)), result);
        } else {
            JS_ASSERT(!templateObject->newType);
            addPtr(Imm32(-thingSize), result);
            storePtr(ImmPtr(NULL), Address(result, JSObject::offsetOfSlots()));
        }

        storePtr(ImmPtr(templateObject->lastProp), Address(result, offsetof(JSObject, lastProp)));
        storePtr(ImmPtr(templateObject->clasp), Address(result, offsetof(JSObject, clasp)));
        store32(Imm32(templateObject->flags), Address(result, offsetof(JSObject, flags)));
        store32(Imm32(templateObject->objShape), Address(result, offsetof(JSObject, objShape)));
        storePtr(ImmPtr(templateObject->newType), Address(result, offsetof(JSObject, newType)));
        storePtr(ImmPtr(templateObject->parent), Address(result, offsetof(JSObject, parent)));
        storePtr(ImmPtr(templateObject->privateData), Address(result, offsetof(JSObject, privateData)));
        storePtr(ImmPtr((void *) templateObject->capacity), Address(result, offsetof(JSObject, capacity)));
        storePtr(ImmPtr(templateObject->gctype()), Address(result, JSObject::offsetOfType()));

        /* Fixed slots of non-array objects are required to be initialized. */
        if (!templateObject->isDenseArray()) {
            for (unsigned i = 0; i < templateObject->numFixedSlots(); i++)
                storeValue(UndefinedValue(), Address(result, JSObject::getFixedSlotOffset(i)));
        }

        return jump;
    }

    /* Add the value stored in 'value' to the accumulator 'counter'. */
    void addCounter(const double *value, double *counter, RegisterID scratch)
    {
        loadDouble(value, Registers::FPConversionTemp);
        move(ImmPtr(counter), scratch);
        addDouble(Address(scratch), Registers::FPConversionTemp);
        storeDouble(Registers::FPConversionTemp, Address(scratch));
    }

    /* Bump the stub call count for script/pc if they are being counted. */
    void bumpStubCounter(JSScript *script, jsbytecode *pc, RegisterID scratch)
    {
        if (script->pcCounters) {
            double *counter = &script->pcCounters.get(JSPCCounters::METHODJIT_STUBS, pc - script->code);
            addCounter(&oneDouble, counter, scratch);
        }
    }

    static const double oneDouble;
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
      : Address(JSFrameReg, StackFrame::offsetOfFlags())
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
            RegisterID reg = mask.takeAnyReg().reg();
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


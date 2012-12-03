/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_macro_assembler_h__
#define jsion_macro_assembler_h__

#if defined(JS_CPU_X86)
# include "ion/x86/MacroAssembler-x86.h"
#elif defined(JS_CPU_X64)
# include "ion/x64/MacroAssembler-x64.h"
#elif defined(JS_CPU_ARM)
# include "ion/arm/MacroAssembler-arm.h"
#endif
#include "ion/IonCompartment.h"
#include "ion/IonInstrumentation.h"
#include "ion/TypeOracle.h"

#include "jsscope.h"
#include "jstypedarray.h"
#include "jscompartment.h"

namespace js {
namespace ion {

// The public entrypoint for emitting assembly. Note that a MacroAssembler can
// use cx->lifoAlloc, so take care not to interleave masm use with other
// lifoAlloc use if one will be destroyed before the other.
class MacroAssembler : public MacroAssemblerSpecific
{
    MacroAssembler *thisFromCtor() {
        return this;
    }

  public:
    class AutoRooter : public AutoGCRooter
    {
        MacroAssembler *masm_;

      public:
        AutoRooter(JSContext *cx, MacroAssembler *masm)
          : AutoGCRooter(cx, IONMASM),
            masm_(masm)
        { }

        MacroAssembler *masm() const {
            return masm_;
        }
    };

    mozilla::Maybe<AutoRooter> autoRooter_;
    mozilla::Maybe<IonContext> ionContext_;
    mozilla::Maybe<AutoIonContextAlloc> alloc_;
    bool enoughMemory_;

  private:
    // This field is used to manage profiling instrumentation output. If
    // provided and enabled, then instrumentation will be emitted around call
    // sites. The IonInstrumentation instance is hosted inside of
    // CodeGeneratorShared and is the manager of when instrumentation is
    // actually emitted or not. If NULL, then no instrumentation is emitted.
    IonInstrumentation *sps_;

  public:
    // If instrumentation should be emitted, then the sps parameter should be
    // provided, but otherwise it can be safely omitted to prevent all
    // instrumentation from being emitted.
    MacroAssembler(IonInstrumentation *sps = NULL)
      : enoughMemory_(true),
        sps_(sps)
    {
        JSContext *cx = GetIonContext()->cx;
        if (cx)
            constructRoot(cx);

        if (!GetIonContext()->temp)
            alloc_.construct(cx);
#ifdef JS_CPU_ARM
        m_buffer.id = GetIonContext()->getNextAssemblerId();
#endif
    }

    // This constructor should only be used when there is no IonContext active
    // (for example, Trampoline-$(ARCH).cpp).
    MacroAssembler(JSContext *cx)
      : enoughMemory_(true),
        sps_(NULL) // no need for instrumentation in trampolines and such
    {
        constructRoot(cx);
        ionContext_.construct(cx, cx->compartment, (js::ion::TempAllocator *)NULL);
        alloc_.construct(cx);
#ifdef JS_CPU_ARM
        m_buffer.id = GetIonContext()->getNextAssemblerId();
#endif
    }

    void constructRoot(JSContext *cx) {
        autoRooter_.construct(cx, this);
    }

    MoveResolver &moveResolver() {
        return moveResolver_;
    }

    size_t instructionsSize() const {
        return size();
    }

    void reportMemory(bool success) {
        enoughMemory_ &= success;
    }
    bool oom() const {
        return !enoughMemory_ || MacroAssemblerSpecific::oom();
    }

    // Emits a test of a value against all types in a TypeSet. A scratch
    // register is required.
    template <typename T>
    void guardTypeSet(const T &address, const types::TypeSet *types, Register scratch,
                      Label *mismatched);

    void loadObjShape(Register objReg, Register dest) {
        loadPtr(Address(objReg, JSObject::offsetOfShape()), dest);
    }
    void loadBaseShape(Register objReg, Register dest) {
        loadPtr(Address(objReg, JSObject::offsetOfShape()), dest);

        loadPtr(Address(dest, Shape::offsetOfBase()), dest);
    }
    void loadBaseShapeClass(Register baseShapeReg, Register dest) {
        loadPtr(Address(baseShapeReg, BaseShape::offsetOfClass()), dest);
    }
    void loadObjClass(Register objReg, Register dest) {
        loadBaseShape(objReg, dest);
        loadBaseShapeClass(dest, dest);
    }
    void branchTestObjClass(Condition cond, Register obj, Register scratch, js::Class *clasp,
                            Label *label) {
        loadBaseShape(obj, scratch);
        branchPtr(cond, Address(scratch, BaseShape::offsetOfClass()), ImmWord(clasp), label);
    }
    void branchTestObjShape(Condition cond, Register obj, const Shape *shape, Label *label) {
        branchPtr(cond, Address(obj, JSObject::offsetOfShape()), ImmGCPtr(shape), label);
    }

    void loadObjPrivate(Register obj, uint32_t nfixed, Register dest) {
        loadPtr(Address(obj, JSObject::getPrivateDataOffset(nfixed)), dest);
    }

    void loadObjProto(Register obj, Register dest) {
        loadPtr(Address(obj, JSObject::offsetOfType()), dest);
        loadPtr(Address(dest, offsetof(types::TypeObject, proto)), dest);
    }

    void loadStringLength(Register str, Register dest) {
        loadPtr(Address(str, JSString::offsetOfLengthAndFlags()), dest);
        rshiftPtr(Imm32(JSString::LENGTH_SHIFT), dest);
    }

    void loadJSContext(const Register &dest) {
        movePtr(ImmWord(GetIonContext()->compartment->rt), dest);
        loadPtr(Address(dest, offsetof(JSRuntime, ionJSContext)), dest);
    }
    void loadIonActivation(const Register &dest) {
        movePtr(ImmWord(GetIonContext()->compartment->rt), dest);
        loadPtr(Address(dest, offsetof(JSRuntime, ionActivation)), dest);
    }

    template<typename T>
    void loadTypedOrValue(const T &src, TypedOrValueRegister dest) {
        if (dest.hasValue())
            loadValue(src, dest.valueReg());
        else
            loadUnboxedValue(src, dest.type(), dest.typedReg());
    }

    template<typename T>
    void loadElementTypedOrValue(const T &src, TypedOrValueRegister dest, bool holeCheck,
                                 Label *hole) {
        if (dest.hasValue()) {
            loadValue(src, dest.valueReg());
            if (holeCheck)
                branchTestMagic(Assembler::Equal, dest.valueReg(), hole);
        } else {
            if (holeCheck)
                branchTestMagic(Assembler::Equal, src, hole);
            loadUnboxedValue(src, dest.type(), dest.typedReg());
        }
    }

    template <typename T>
    void storeTypedOrValue(TypedOrValueRegister src, const T &dest) {
        if (src.hasValue())
            storeValue(src.valueReg(), dest);
        else if (src.type() == MIRType_Double)
            storeDouble(src.typedReg().fpu(), dest);
        else
            storeValue(ValueTypeFromMIRType(src.type()), src.typedReg().gpr(), dest);
    }

    template <typename T>
    void storeConstantOrRegister(ConstantOrRegister src, const T &dest) {
        if (src.constant())
            storeValue(src.value(), dest);
        else
            storeTypedOrValue(src.reg(), dest);
    }

    void storeCallResult(Register reg) {
        if (reg != ReturnReg)
            mov(ReturnReg, reg);
    }

    void storeCallResultValue(AnyRegister dest) {
#if defined(JS_NUNBOX32)
        unboxValue(ValueOperand(JSReturnReg_Type, JSReturnReg_Data), dest);
#elif defined(JS_PUNBOX64)
        unboxValue(ValueOperand(JSReturnReg), dest);
#else
#error "Bad architecture"
#endif
    }

    void storeCallResultValue(ValueOperand dest) {
#if defined(JS_NUNBOX32)
        // reshuffle the return registers used for a call result to store into
        // dest, using ReturnReg as a scratch register if necessary. This must
        // only be called after returning from a call, at a point when the
        // return register is not live. XXX would be better to allow wrappers
        // to store the return value to different places.
        if (dest.typeReg() == JSReturnReg_Data) {
            if (dest.payloadReg() == JSReturnReg_Type) {
                // swap the two registers.
                mov(JSReturnReg_Type, ReturnReg);
                mov(JSReturnReg_Data, JSReturnReg_Type);
                mov(ReturnReg, JSReturnReg_Data);
            } else {
                mov(JSReturnReg_Data, dest.payloadReg());
                mov(JSReturnReg_Type, dest.typeReg());
            }
        } else {
            mov(JSReturnReg_Type, dest.typeReg());
            mov(JSReturnReg_Data, dest.payloadReg());
        }
#elif defined(JS_PUNBOX64)
        if (dest.valueReg() != JSReturnReg)
            movq(JSReturnReg, dest.valueReg());
#else
#error "Bad architecture"
#endif
    }

    void storeCallResultValue(TypedOrValueRegister dest) {
        if (dest.hasValue())
            storeCallResultValue(dest.valueReg());
        else
            storeCallResultValue(dest.typedReg());
    }

    void PushRegsInMask(RegisterSet set);
    void PushRegsInMask(GeneralRegisterSet set) {
        PushRegsInMask(RegisterSet(set, FloatRegisterSet()));
    }
    void PopRegsInMask(RegisterSet set) {
        PopRegsInMaskIgnore(set, RegisterSet());
    }
    void PopRegsInMask(GeneralRegisterSet set) {
        PopRegsInMask(RegisterSet(set, FloatRegisterSet()));
    }
    void PopRegsInMaskIgnore(RegisterSet set, RegisterSet ignore);

    void branchTestValueTruthy(const ValueOperand &value, Label *ifTrue, FloatRegister fr);

    void branchIfFunctionHasNoScript(Register fun, Label *label) {
        // 16-bit loads are slow and unaligned 32-bit loads may be too so
        // perform an aligned 32-bit load and adjust the bitmask accordingly.
        JS_STATIC_ASSERT(offsetof(JSFunction, nargs) % sizeof(uint32_t) == 0);
        JS_STATIC_ASSERT(offsetof(JSFunction, flags) == offsetof(JSFunction, nargs) + 2);
        JS_STATIC_ASSERT(IS_LITTLE_ENDIAN);
        Address address(fun, offsetof(JSFunction, nargs));
        uint32_t bit = JSFunction::INTERPRETED << 16;
        branchTest32(Assembler::Zero, address, Imm32(bit), label);
    }

    using MacroAssemblerSpecific::Push;

    void Push(jsid id, Register scratchReg) {
        if (JSID_IS_GCTHING(id)) {
            // If we're pushing a gcthing, then we can't just push the tagged jsid
            // value since the GC won't have any idea that the push instruction
            // carries a reference to a gcthing.  Need to unpack the pointer,
            // push it using ImmGCPtr, and then rematerialize the id at runtime.

            // double-checking this here to ensure we don't lose sync
            // with implementation of JSID_IS_GCTHING.
            if (JSID_IS_OBJECT(id)) {
                JSObject *obj = JSID_TO_OBJECT(id);
                movePtr(ImmGCPtr(obj), scratchReg);
                JS_ASSERT(((size_t)obj & JSID_TYPE_MASK) == 0);
                orPtr(Imm32(JSID_TYPE_OBJECT), scratchReg);
                Push(scratchReg);
            } else {
                JSString *str = JSID_TO_STRING(id);
                JS_ASSERT(((size_t)str & JSID_TYPE_MASK) == 0);
                JS_ASSERT(JSID_TYPE_STRING == 0x0);
                Push(ImmGCPtr(str));
            }
        } else {
            size_t idbits = JSID_BITS(id);
            Push(ImmWord(idbits));
        }
    }

    void Push(TypedOrValueRegister v) {
        if (v.hasValue())
            Push(v.valueReg());
        else if (v.type() == MIRType_Double)
            Push(v.typedReg().fpu());
        else
            Push(ValueTypeFromMIRType(v.type()), v.typedReg().gpr());
    }

    void Push(ConstantOrRegister v) {
        if (v.constant())
            Push(v.value());
        else
            Push(v.reg());
    }

    void Push(const ValueOperand &val) {
        pushValue(val);
        framePushed_ += sizeof(Value);
    }

    void Push(const Value &val) {
        pushValue(val);
        framePushed_ += sizeof(Value);
    }

    void Push(JSValueType type, Register reg) {
        pushValue(type, reg);
        framePushed_ += sizeof(Value);
    }

    void adjustStack(int amount) {
        if (amount > 0)
            freeStack(amount);
        else if (amount < 0)
            reserveStack(-amount);
    }

    void bumpKey(Int32Key *key, int diff) {
        if (key->isRegister())
            add32(Imm32(diff), key->reg());
        else
            key->bumpConstant(diff);
    }

    void storeKey(const Int32Key &key, const Address &dest) {
        if (key.isRegister())
            store32(key.reg(), dest);
        else
            store32(Imm32(key.constant()), dest);
    }

    template<typename T>
    void branchKey(Condition cond, const T &length, const Int32Key &key, Label *label) {
        if (key.isRegister())
            branch32(cond, length, key.reg(), label);
        else
            branch32(cond, length, Imm32(key.constant()), label);
    }

    void branchTestNeedsBarrier(Condition cond, const Register &scratch, Label *label) {
        JS_ASSERT(cond == Zero || cond == NonZero);
        JSCompartment *comp = GetIonContext()->compartment;
        movePtr(ImmWord(comp), scratch);
        Address needsBarrierAddr(scratch, JSCompartment::OffsetOfNeedsBarrier());
        branchTest32(cond, needsBarrierAddr, Imm32(0x1), label);
    }

    template <typename T>
    void callPreBarrier(const T &address, MIRType type) {
        JS_ASSERT(type == MIRType_Value ||
                  type == MIRType_String ||
                  type == MIRType_Object ||
                  type == MIRType_Shape);
        Label done;

        if (type == MIRType_Value)
            branchTestGCThing(Assembler::NotEqual, address, &done);

        Push(PreBarrierReg);
        computeEffectiveAddress(address, PreBarrierReg);

        JSCompartment *compartment = GetIonContext()->compartment;
        IonCode *preBarrier = (type == MIRType_Shape)
                              ? compartment->ionCompartment()->shapePreBarrier()
                              : compartment->ionCompartment()->valuePreBarrier();

        call(preBarrier);
        Pop(PreBarrierReg);

        bind(&done);
    }

    template <typename T>
    CodeOffsetLabel patchableCallPreBarrier(const T &address, MIRType type) {
        JS_ASSERT(type == MIRType_Value || type == MIRType_String || type == MIRType_Object);

        Label done;

        // All barriers are off by default.
        // They are enabled if necessary at the end of CodeGenerator::generate().
        CodeOffsetLabel nopJump = toggledJump(&done);

        callPreBarrier(address, type);
        jump(&done);

        align(8);
        bind(&done);
        return nopJump;
    }

    template<typename T>
    void loadFromTypedArray(int arrayType, const T &src, AnyRegister dest, Register temp, Label *fail);

    template<typename T>
    void loadFromTypedArray(int arrayType, const T &src, const ValueOperand &dest, bool allowDouble,
                            Label *fail);

    template<typename S, typename T>
    void storeToTypedIntArray(int arrayType, const S &value, const T &dest) {
        switch (arrayType) {
          case TypedArray::TYPE_INT8:
          case TypedArray::TYPE_UINT8:
          case TypedArray::TYPE_UINT8_CLAMPED:
            store8(value, dest);
            break;
          case TypedArray::TYPE_INT16:
          case TypedArray::TYPE_UINT16:
            store16(value, dest);
            break;
          case TypedArray::TYPE_INT32:
          case TypedArray::TYPE_UINT32:
            store32(value, dest);
            break;
          default:
            JS_NOT_REACHED("Invalid typed array type");
            break;
        }
    }

    template<typename S, typename T>
    void storeToTypedFloatArray(int arrayType, const S &value, const T &dest) {
        switch (arrayType) {
          case TypedArray::TYPE_FLOAT32:
            convertDoubleToFloat(value, ScratchFloatReg);
            storeFloat(ScratchFloatReg, dest);
            break;
          case TypedArray::TYPE_FLOAT64:
            storeDouble(value, dest);
            break;
          default:
            JS_NOT_REACHED("Invalid typed array type");
            break;
        }
    }

    // Inline version of js_TypedArray_uint8_clamp_double.
    // This function clobbers the input register.
    void clampDoubleToUint8(FloatRegister input, Register output);

    // Inline allocation.
    void newGCThing(const Register &result, JSObject *templateObject, Label *fail);
    void initGCThing(const Register &obj, JSObject *templateObject);

    // If the IonCode that created this assembler needs to transition into the VM,
    // we want to store the IonCode on the stack in order to mark it during a GC.
    // This is a reference to a patch location where the IonCode* will be written.
  private:
    CodeOffsetLabel exitCodePatch_;

  public:
    void enterExitFrame(const VMFunction *f = NULL) {
        linkExitFrame();
        // Push the ioncode. (Bailout or VM wrapper)
        exitCodePatch_ = PushWithPatch(ImmWord(-1));
        // Push VMFunction pointer, to mark arguments.
        Push(ImmWord(f));
    }
    void enterFakeExitFrame(IonCode *codeVal = NULL) {
        linkExitFrame();
        Push(ImmWord(uintptr_t(codeVal)));
        Push(ImmWord(uintptr_t(NULL)));
    }

    void leaveExitFrame() {
        freeStack(IonExitFooterFrame::Size());
    }

    void link(IonCode *code) {
        JS_ASSERT(!oom());
        // If this code can transition to C++ code and witness a GC, then we need to store
        // the IonCode onto the stack in order to GC it correctly.  exitCodePatch should
        // be unset if the code never needed to push its IonCode*.
        if (exitCodePatch_.offset() != 0) {
            patchDataWithValueCheck(CodeLocationLabel(code, exitCodePatch_),
                                    ImmWord(uintptr_t(code)),
                                    ImmWord(uintptr_t(-1)));
        }

    }

    // Given a js::StackFrame in OsrFrameReg, performs inline on-stack
    // replacement. The stack frame must be at a valid OSR entry-point.
    void performOsr();

    // Checks if an OSR frame is the previous frame, and if so, removes it.
    void maybeRemoveOsrFrame(Register scratch);

    // Generates code used to complete a bailout.
    void generateBailoutTail(Register scratch);

    // These functions exist as small wrappers around sites where execution can
    // leave the currently running stream of instructions. They exist so that
    // instrumentation may be put in place around them if necessary and the
    // instrumentation is enabled. For the functions that return a uint32_t,
    // they are returning the offset of the assembler just after the call has
    // been made so that a safepoint can be made at that location.

    void callWithABI(void *fun, Result result = GENERAL) {
        leaveSPSFrame();
        MacroAssemblerSpecific::callWithABI(fun, result);
        reenterSPSFrame();
    }

    void handleException() {
        // Re-entry code is irrelevant because the exception will leave the
        // running function and never come back
        if (sps_)
            sps_->skipNextReenter();
        leaveSPSFrame();
        MacroAssemblerSpecific::handleException();
        // Doesn't actually emit code, but balances the leave()
        if (sps_)
            sps_->reenter(*this, InvalidReg);
    }

    // see above comment for what is returned
    uint32_t callIon(const Register &callee) {
        leaveSPSFrame();
        MacroAssemblerSpecific::callIon(callee);
        uint32_t ret = currentOffset();
        reenterSPSFrame();
        return ret;
    }

    // see above comment for what is returned
    uint32_t callWithExitFrame(IonCode *target) {
        leaveSPSFrame();
        MacroAssemblerSpecific::callWithExitFrame(target);
        uint32_t ret = currentOffset();
        reenterSPSFrame();
        return ret;
    }

    // see above comment for what is returned
    uint32_t callWithExitFrame(IonCode *target, Register dynStack) {
        leaveSPSFrame();
        MacroAssemblerSpecific::callWithExitFrame(target, dynStack);
        uint32_t ret = currentOffset();
        reenterSPSFrame();
        return ret;
    }

  private:
    // These two functions are helpers used around call sites throughout the
    // assembler. They are called from the above call wrappers to emit the
    // necessary instrumentation.
    void leaveSPSFrame() {
        if (!sps_ || !sps_->enabled())
            return;
        // No registers are guaranteed to be available, so push/pop a register
        // so we can use one
        push(CallTempReg0);
        sps_->leave(*this, CallTempReg0);
        pop(CallTempReg0);
    }

    void reenterSPSFrame() {
        if (!sps_ || !sps_->enabled())
            return;
        // Attempt to use a now-free register within a given set, but if the
        // architecture being built doesn't have an available register, resort
        // to push/pop
        GeneralRegisterSet regs(Registers::TempMask & ~Registers::JSCallMask &
                                                      ~Registers::CallMask);
        if (regs.empty()) {
            push(CallTempReg0);
            sps_->reenter(*this, CallTempReg0);
            pop(CallTempReg0);
        } else {
            sps_->reenter(*this, regs.getAny());
        }
    }

    void spsProfileEntryAddress(SPSProfiler *p, int offset, Register temp,
                                Label *full)
    {
        movePtr(ImmWord(p->sizePointer()), temp);
        load32(Address(temp, 0), temp);
        if (offset != 0)
            add32(Imm32(offset), temp);
        branch32(Assembler::GreaterThanOrEqual, temp, Imm32(p->maxSize()), full);

        // 4 * sizeof(void*) * idx = idx << (2 + log(sizeof(void*)))
        JS_STATIC_ASSERT(sizeof(ProfileEntry) == 4 * sizeof(void*));
        lshiftPtr(Imm32(2 + (sizeof(void*) == 4 ? 2 : 3)), temp);
        addPtr(ImmWord(p->stack()), temp);
    }

  public:

    // These functions are needed by the IonInstrumentation interface defined in
    // vm/SPSProfiler.h.  They will modify the pseudostack provided to SPS to
    // perform the actual instrumentation.

    void spsUpdatePCIdx(SPSProfiler *p, int32_t idx, Register temp) {
        Label stackFull;
        spsProfileEntryAddress(p, -1, temp, &stackFull);
        store32(Imm32(idx), Address(temp, ProfileEntry::offsetOfPCIdx()));
        bind(&stackFull);
    }

    void spsPushFrame(SPSProfiler *p, const char *str, JSScript *s, Register temp) {
        Label stackFull;
        spsProfileEntryAddress(p, 0, temp, &stackFull);

        storePtr(ImmWord(str),    Address(temp, ProfileEntry::offsetOfString()));
        storePtr(ImmGCPtr(s),     Address(temp, ProfileEntry::offsetOfScript()));
        storePtr(ImmWord((void*) NULL),
                 Address(temp, ProfileEntry::offsetOfStackAddress()));
        store32(Imm32(ProfileEntry::NullPCIndex),
                Address(temp, ProfileEntry::offsetOfPCIdx()));

        /* Always increment the stack size, whether or not we actually pushed. */
        bind(&stackFull);
        movePtr(ImmWord(p->sizePointer()), temp);
        add32(Imm32(1), Address(temp, 0));
    }

    void spsPopFrame(SPSProfiler *p, Register temp) {
        movePtr(ImmWord(p->sizePointer()), temp);
        add32(Imm32(-1), Address(temp, 0));
    }

    void printf(const char *output);
    void printf(const char *output, Register value);
};

static inline Assembler::Condition
JSOpToCondition(JSOp op)
{
    switch (op) {
      case JSOP_EQ:
      case JSOP_STRICTEQ:
        return Assembler::Equal;
      case JSOP_NE:
      case JSOP_STRICTNE:
        return Assembler::NotEqual;
      case JSOP_LT:
        return Assembler::LessThan;
      case JSOP_LE:
        return Assembler::LessThanOrEqual;
      case JSOP_GT:
        return Assembler::GreaterThan;
      case JSOP_GE:
        return Assembler::GreaterThanOrEqual;
      default:
        JS_NOT_REACHED("Unrecognized comparison operation");
        return Assembler::Equal;
    }
}

static inline Assembler::DoubleCondition
JSOpToDoubleCondition(JSOp op)
{
    switch (op) {
      case JSOP_EQ:
      case JSOP_STRICTEQ:
        return Assembler::DoubleEqual;
      case JSOP_NE:
      case JSOP_STRICTNE:
        return Assembler::DoubleNotEqualOrUnordered;
      case JSOP_LT:
        return Assembler::DoubleLessThan;
      case JSOP_LE:
        return Assembler::DoubleLessThanOrEqual;
      case JSOP_GT:
        return Assembler::DoubleGreaterThan;
      case JSOP_GE:
        return Assembler::DoubleGreaterThanOrEqual;
      default:
        JS_NOT_REACHED("Unexpected comparison operation");
        return Assembler::DoubleEqual;
    }
}

} // namespace ion
} // namespace js

#endif // jsion_macro_assembler_h__


/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsinfer.h"
#include "jsinferinlines.h"
#include "IonMacroAssembler.h"
#include "gc/Root.h"
#include "Bailouts.h"

using namespace js;
using namespace js::ion;

template <typename T> void
MacroAssembler::guardTypeSet(const T &address, const types::TypeSet *types,
                             Register scratch, Label *mismatched)
{
    JS_ASSERT(!types->unknown());

    Label matched;
    Register tag = extractTag(address, scratch);

    if (types->hasType(types::Type::DoubleType())) {
        // The double type also implies Int32.
        JS_ASSERT(types->hasType(types::Type::Int32Type()));
        branchTestNumber(Equal, tag, &matched);
    } else if (types->hasType(types::Type::Int32Type())) {
        branchTestInt32(Equal, tag, &matched);
    }

    if (types->hasType(types::Type::UndefinedType()))
        branchTestUndefined(Equal, tag, &matched);
    if (types->hasType(types::Type::BooleanType()))
        branchTestBoolean(Equal, tag, &matched);
    if (types->hasType(types::Type::StringType()))
        branchTestString(Equal, tag, &matched);
    if (types->hasType(types::Type::NullType()))
        branchTestNull(Equal, tag, &matched);

    if (types->hasType(types::Type::AnyObjectType())) {
        branchTestObject(Equal, tag, &matched);
    } else if (types->getObjectCount()) {
        branchTestObject(NotEqual, tag, mismatched);
        Register obj = extractObject(address, scratch);

        unsigned count = types->getObjectCount();
        for (unsigned i = 0; i < count; i++) {
            if (JSObject *object = types->getSingleObject(i))
                branchPtr(Equal, obj, ImmGCPtr(object), &matched);
        }

        loadPtr(Address(obj, JSObject::offsetOfType()), scratch);

        for (unsigned i = 0; i < count; i++) {
            if (types::TypeObject *object = types->getTypeObject(i))
                branchPtr(Equal, scratch, ImmGCPtr(object), &matched);
        }
    }

    jump(mismatched);
    bind(&matched);
}

template void MacroAssembler::guardTypeSet(const Address &address, const types::TypeSet *types,
                                           Register scratch, Label *mismatched);
template void MacroAssembler::guardTypeSet(const ValueOperand &value, const types::TypeSet *types,
                                           Register scratch, Label *mismatched);

void
MacroAssembler::PushRegsInMask(RegisterSet set)
{
    size_t diff = set.gprs().size() * STACK_SLOT_SIZE +
                  set.fpus().size() * sizeof(double);

    reserveStack(diff);

    for (GeneralRegisterIterator iter(set.gprs()); iter.more(); iter++) {
        diff -= STACK_SLOT_SIZE;
        storePtr(*iter, Address(StackPointer, diff));
    }
    for (FloatRegisterIterator iter(set.fpus()); iter.more(); iter++) {
        diff -= sizeof(double);
        storeDouble(*iter, Address(StackPointer, diff));
    }
}

void
MacroAssembler::PopRegsInMaskIgnore(RegisterSet set, RegisterSet ignore)
{
    size_t diff = set.gprs().size() * STACK_SLOT_SIZE +
                  set.fpus().size() * sizeof(double);
    size_t reserved = diff;

    for (GeneralRegisterIterator iter(set.gprs()); iter.more(); iter++) {
        diff -= STACK_SLOT_SIZE;
        if (!ignore.has(*iter))
            loadPtr(Address(StackPointer, diff), *iter);
    }
    for (FloatRegisterIterator iter(set.fpus()); iter.more(); iter++) {
        diff -= sizeof(double);
        if (!ignore.has(*iter))
            loadDouble(Address(StackPointer, diff), *iter);
    }

    freeStack(reserved);
}

template<typename T>
void
MacroAssembler::loadFromTypedArray(int arrayType, const T &src, AnyRegister dest, Register temp,
                                   Label *fail)
{
    switch (arrayType) {
      case TypedArray::TYPE_INT8:
        load8SignExtend(src, dest.gpr());
        break;
      case TypedArray::TYPE_UINT8:
      case TypedArray::TYPE_UINT8_CLAMPED:
        load8ZeroExtend(src, dest.gpr());
        break;
      case TypedArray::TYPE_INT16:
        load16SignExtend(src, dest.gpr());
        break;
      case TypedArray::TYPE_UINT16:
        load16ZeroExtend(src, dest.gpr());
        break;
      case TypedArray::TYPE_INT32:
        load32(src, dest.gpr());
        break;
      case TypedArray::TYPE_UINT32:
        if (dest.isFloat()) {
            load32(src, temp);
            convertUInt32ToDouble(temp, dest.fpu());
        } else {
            load32(src, dest.gpr());
            test32(dest.gpr(), dest.gpr());
            j(Assembler::Signed, fail);
        }
        break;
      case TypedArray::TYPE_FLOAT32:
      case TypedArray::TYPE_FLOAT64:
      {
        if (arrayType == js::TypedArray::TYPE_FLOAT32)
            loadFloatAsDouble(src, dest.fpu());
        else
            loadDouble(src, dest.fpu());

        // Make sure NaN gets canonicalized.
        Label notNaN;
        branchDouble(DoubleOrdered, dest.fpu(), dest.fpu(), &notNaN);
        {
            loadStaticDouble(&js_NaN, dest.fpu());
        }
        bind(&notNaN);
        break;
      }
      default:
        JS_NOT_REACHED("Invalid typed array type");
        break;
    }
}

template void MacroAssembler::loadFromTypedArray(int arrayType, const Address &src, AnyRegister dest,
                                                 Register temp, Label *fail);
template void MacroAssembler::loadFromTypedArray(int arrayType, const BaseIndex &src, AnyRegister dest,
                                                 Register temp, Label *fail);

template<typename T>
void
MacroAssembler::loadFromTypedArray(int arrayType, const T &src, const ValueOperand &dest,
                                   bool allowDouble, Label *fail)
{
    switch (arrayType) {
      case TypedArray::TYPE_INT8:
      case TypedArray::TYPE_UINT8:
      case TypedArray::TYPE_UINT8_CLAMPED:
      case TypedArray::TYPE_INT16:
      case TypedArray::TYPE_UINT16:
      case TypedArray::TYPE_INT32:
        loadFromTypedArray(arrayType, src, AnyRegister(dest.scratchReg()), InvalidReg, NULL);
        tagValue(JSVAL_TYPE_INT32, dest.scratchReg(), dest);
        break;
      case TypedArray::TYPE_UINT32:
        load32(src, dest.scratchReg());
        test32(dest.scratchReg(), dest.scratchReg());
        if (allowDouble) {
            // If the value fits in an int32, store an int32 type tag.
            // Else, convert the value to double and box it.
            Label done, isDouble;
            j(Assembler::Signed, &isDouble);
            {
                tagValue(JSVAL_TYPE_INT32, dest.scratchReg(), dest);
                jump(&done);
            }
            bind(&isDouble);
            {
                convertUInt32ToDouble(dest.scratchReg(), ScratchFloatReg);
                boxDouble(ScratchFloatReg, dest);
            }
            bind(&done);
        } else {
            // Bailout if the value does not fit in an int32.
            j(Assembler::Signed, fail);
            tagValue(JSVAL_TYPE_INT32, dest.scratchReg(), dest);
        }
        break;
      case TypedArray::TYPE_FLOAT32:
      case TypedArray::TYPE_FLOAT64:
        loadFromTypedArray(arrayType, src, AnyRegister(ScratchFloatReg), dest.scratchReg(), NULL);
        boxDouble(ScratchFloatReg, dest);
        break;
      default:
        JS_NOT_REACHED("Invalid typed array type");
        break;
    }
}

template void MacroAssembler::loadFromTypedArray(int arrayType, const Address &src, const ValueOperand &dest,
                                                 bool allowDouble, Label *fail);
template void MacroAssembler::loadFromTypedArray(int arrayType, const BaseIndex &src, const ValueOperand &dest,
                                                 bool allowDouble, Label *fail);

// Note: this function clobbers the input register.
void
MacroAssembler::clampDoubleToUint8(FloatRegister input, Register output)
{
    JS_ASSERT(input != ScratchFloatReg);
#ifdef JS_CPU_ARM
    Label notSplit;
    ma_vimm(0.5, ScratchFloatReg);
    ma_vadd(input, ScratchFloatReg, ScratchFloatReg);
    // Convert the double into an unsigned fixed point value with 24 bits of
    // precision. The resulting number will look like 0xII.DDDDDD
    as_vcvtFixed(ScratchFloatReg, false, 24, true);
    // Move the fixed point value into an integer register
    as_vxfer(output, InvalidReg, ScratchFloatReg, FloatToCore);
    // See if this value *might* have been an exact integer after adding 0.5
    // This tests the 1/2 through 1/16,777,216th places, but 0.5 needs to be tested out to
    // the 1/140,737,488,355,328th place.
    ma_tst(output, Imm32(0x00ffffff));
    // convert to a uint8 by shifting out all of the fraction bits
    ma_lsr(Imm32(24), output, output);
    // If any of the bottom 24 bits were non-zero, then we're good, since this number
    // can't be exactly XX.0
    ma_b(&notSplit, NonZero);
    as_vxfer(ScratchRegister, InvalidReg, input, FloatToCore);
    ma_cmp(ScratchRegister, Imm32(0));
    // If the lower 32 bits of the double were 0, then this was an exact number,
    // and it should be even.
    ma_bic(Imm32(1), output, NoSetCond, Zero);
    bind(&notSplit);
#else

    Label positive, done;

    // <= 0 or NaN --> 0
    zeroDouble(ScratchFloatReg);
    branchDouble(DoubleGreaterThan, input, ScratchFloatReg, &positive);
    {
        move32(Imm32(0), output);
        jump(&done);
    }

    bind(&positive);

    // Add 0.5 and truncate.
    static const double DoubleHalf = 0.5;
    loadStaticDouble(&DoubleHalf, ScratchFloatReg);
    addDouble(ScratchFloatReg, input);

    Label outOfRange;
    branchTruncateDouble(input, output, &outOfRange);
    branch32(Assembler::Above, output, Imm32(255), &outOfRange);
    {
        // Check if we had a tie.
        convertInt32ToDouble(output, ScratchFloatReg);
        branchDouble(DoubleNotEqual, input, ScratchFloatReg, &done);

        // It was a tie. Mask out the ones bit to get an even value.
        // See also js_TypedArray_uint8_clamp_double.
        and32(Imm32(~1), output);
        jump(&done);
    }

    // > 255 --> 255
    bind(&outOfRange);
    {
        move32(Imm32(255), output);
    }

    bind(&done);
#endif
}

void
MacroAssembler::newGCThing(const Register &result,
                           JSObject *templateObject, Label *fail)
{
    // Inlined equivalent of js::gc::NewGCThing() without failure case handling.

    gc::AllocKind allocKind = templateObject->getAllocKind();
    JS_ASSERT(allocKind >= gc::FINALIZE_OBJECT0 && allocKind <= gc::FINALIZE_OBJECT_LAST);
    int thingSize = (int)gc::Arena::thingSize(allocKind);

    JS_ASSERT(!templateObject->hasDynamicElements());

    JSCompartment *compartment = GetIonContext()->compartment;

#ifdef JS_GC_ZEAL
    // Don't execute the inline path if gcZeal is active.
    movePtr(ImmWord(compartment->rt), result);
    loadPtr(Address(result, offsetof(JSRuntime, gcZeal_)), result);
    branch32(Assembler::NotEqual, result, Imm32(0), fail);
#endif

    // Inline FreeSpan::allocate.
    // There is always exactly one FreeSpan per allocKind per JSCompartment.
    // If a FreeSpan is replaced, its members are updated in the freeLists table,
    // which the code below always re-reads.
    gc::FreeSpan *list = const_cast<gc::FreeSpan *>
                         (compartment->arenas.getFreeList(allocKind));
    loadPtr(AbsoluteAddress(&list->first), result);
    branchPtr(Assembler::BelowOrEqual, AbsoluteAddress(&list->last), result, fail);

    addPtr(Imm32(thingSize), result);
    storePtr(result, AbsoluteAddress(&list->first));
    subPtr(Imm32(thingSize), result);
}

void
MacroAssembler::initGCThing(const Register &obj, JSObject *templateObject)
{
    // Fast initialization of an empty object returned by NewGCThing().

    storePtr(ImmGCPtr(templateObject->lastProperty()), Address(obj, JSObject::offsetOfShape()));
    storePtr(ImmGCPtr(templateObject->type()), Address(obj, JSObject::offsetOfType()));
    storePtr(ImmWord((void *)NULL), Address(obj, JSObject::offsetOfSlots()));

    if (templateObject->isDenseArray()) {
        JS_ASSERT(!templateObject->getDenseArrayInitializedLength());

        int elementsOffset = JSObject::offsetOfFixedElements();

        addPtr(Imm32(elementsOffset), obj);
        storePtr(obj, Address(obj, -elementsOffset + JSObject::offsetOfElements()));
        addPtr(Imm32(-elementsOffset), obj);

        // Fill in the elements header.
        store32(Imm32(templateObject->getDenseArrayCapacity()),
                Address(obj, elementsOffset + ObjectElements::offsetOfCapacity()));
        store32(Imm32(templateObject->getDenseArrayInitializedLength()),
                Address(obj, elementsOffset + ObjectElements::offsetOfInitializedLength()));
        store32(Imm32(templateObject->getArrayLength()),
                Address(obj, elementsOffset + ObjectElements::offsetOfLength()));
    } else {
        storePtr(ImmWord(emptyObjectElements), Address(obj, JSObject::offsetOfElements()));

        // Fixed slots of non-array objects are required to be initialized.
        // Use the values currently in the template object.
        size_t nslots = Min(templateObject->numFixedSlots(), templateObject->slotSpan());
        for (unsigned i = 0; i < nslots; i++) {
            storeValue(templateObject->getFixedSlot(i),
                       Address(obj, JSObject::getFixedSlotOffset(i)));
        }
    }

    if (templateObject->hasPrivate()) {
        uint32_t nfixed = templateObject->numFixedSlots();
        storePtr(ImmWord(templateObject->getPrivate()),
                 Address(obj, JSObject::getPrivateDataOffset(nfixed)));
    }
}

void
MacroAssembler::maybeRemoveOsrFrame(Register scratch)
{
    // Before we link an exit frame, check for an OSR frame, which is
    // indicative of working inside an existing bailout. In this case, remove
    // the OSR frame, so we don't explode the stack with repeated bailouts.
    Label osrRemoved;
    loadPtr(Address(StackPointer, IonCommonFrameLayout::offsetOfDescriptor()), scratch);
    and32(Imm32(FRAMETYPE_MASK), scratch);
    branch32(Assembler::NotEqual, scratch, Imm32(IonFrame_Osr), &osrRemoved);
    addPtr(Imm32(sizeof(IonOsrFrameLayout)), StackPointer);
    bind(&osrRemoved);
}

void
MacroAssembler::performOsr()
{
    GeneralRegisterSet regs = GeneralRegisterSet::All();
    if (FramePointer != InvalidReg && sps_ && sps_->enabled())
        regs.take(FramePointer);

    // This register must be fixed as it's used in the Osr prologue.
    regs.take(OsrFrameReg);

    // Remove any existing OSR frame so we don't create one per bailout.
    maybeRemoveOsrFrame(regs.getAny());

    const Register script = regs.takeAny();
    const Register calleeToken = regs.takeAny();

    // Grab fp.exec
    loadPtr(Address(OsrFrameReg, StackFrame::offsetOfExec()), script);
    mov(script, calleeToken);

    Label isFunction, performOsr;
    branchTest32(Assembler::NonZero,
                 Address(OsrFrameReg, StackFrame::offsetOfFlags()),
                 Imm32(StackFrame::FUNCTION),
                 &isFunction);

    {
        // Not a function - just tag the calleeToken now.
        orPtr(Imm32(CalleeToken_Script), calleeToken);
        jump(&performOsr);
    }

    bind(&isFunction);
    {
        // Function - create the callee token, then get the script.
        orPtr(Imm32(CalleeToken_Function), calleeToken);
        loadPtr(Address(script, JSFunction::offsetOfNativeOrScript()), script);
    }

    bind(&performOsr);

    const Register ionScript = regs.takeAny();
    const Register osrEntry = regs.takeAny();

    loadPtr(Address(script, offsetof(JSScript, ion)), ionScript);
    load32(Address(ionScript, IonScript::offsetOfOsrEntryOffset()), osrEntry);

    // Get ionScript->method->code, and scoot to the osrEntry.
    const Register code = ionScript;
    loadPtr(Address(ionScript, IonScript::offsetOfMethod()), code);
    loadPtr(Address(code, IonCode::offsetOfCode()), code);
    addPtr(osrEntry, code);

    // To simplify stack handling, we create an intermediate OSR frame, that
    // looks like a JS frame with no argv.
    enterOsr(calleeToken, code);
    ret();
}

void
MacroAssembler::generateBailoutTail(Register scratch)
{
    enterExitFrame();

    Label reflow;
    Label interpret;
    Label exception;
    Label osr;
    Label recompile;
    Label boundscheck;
    Label overrecursed;
    Label invalidate;

    // The return value from Bailout is tagged as:
    // - 0x0: done (thunk to interpreter)
    // - 0x1: error (handle exception)
    // - 0x2: reflow args
    // - 0x3: reflow barrier
    // - 0x4: monitor types
    // - 0x5: recompile to inline calls
    // - 0x6: bounds check failure
    // - 0x7: force invalidation
    // - 0x8: overrecursed
    // - 0x9: cached shape guard failure

    branch32(LessThan, ReturnReg, Imm32(BAILOUT_RETURN_FATAL_ERROR), &interpret);
    branch32(Equal, ReturnReg, Imm32(BAILOUT_RETURN_FATAL_ERROR), &exception);

    branch32(LessThan, ReturnReg, Imm32(BAILOUT_RETURN_RECOMPILE_CHECK), &reflow);
    branch32(Equal, ReturnReg, Imm32(BAILOUT_RETURN_RECOMPILE_CHECK), &recompile);

    branch32(Equal, ReturnReg, Imm32(BAILOUT_RETURN_BOUNDS_CHECK), &boundscheck);
    branch32(Equal, ReturnReg, Imm32(BAILOUT_RETURN_OVERRECURSED), &overrecursed);
    branch32(Equal, ReturnReg, Imm32(BAILOUT_RETURN_SHAPE_GUARD), &invalidate);

    // Fall-through: cached shape guard failure.
    {
        setupUnalignedABICall(0, scratch);
        callWithABI(JS_FUNC_TO_DATA_PTR(void *, CachedShapeGuardFailure));

        branchTest32(Zero, ReturnReg, ReturnReg, &exception);
        jump(&interpret);
    }

    // Force invalidation.
    bind(&invalidate);
    {
        setupUnalignedABICall(0, scratch);
        callWithABI(JS_FUNC_TO_DATA_PTR(void *, ShapeGuardFailure));

        branchTest32(Zero, ReturnReg, ReturnReg, &exception);
        jump(&interpret);
    }

    // Bounds-check failure.
    bind(&boundscheck);
    {
        setupUnalignedABICall(0, scratch);
        callWithABI(JS_FUNC_TO_DATA_PTR(void *, BoundsCheckFailure));

        branchTest32(Zero, ReturnReg, ReturnReg, &exception);
        jump(&interpret);
    }

    // Recompile to inline calls.
    bind(&recompile);
    {
        setupUnalignedABICall(0, scratch);
        callWithABI(JS_FUNC_TO_DATA_PTR(void *, RecompileForInlining));

        branchTest32(Zero, ReturnReg, ReturnReg, &exception);
        jump(&interpret);
    }

    // Reflow types.
    bind(&reflow);
    {
        setupUnalignedABICall(1, scratch);
        passABIArg(ReturnReg);
        callWithABI(JS_FUNC_TO_DATA_PTR(void *, ReflowTypeInfo));

        branchTest32(Zero, ReturnReg, ReturnReg, &exception);
        jump(&interpret);
    }

    // Throw an over-recursion error.
    bind(&overrecursed);
    {
        loadJSContext(ReturnReg);
        setupUnalignedABICall(1, scratch);
        passABIArg(ReturnReg);
        callWithABI(JS_FUNC_TO_DATA_PTR(void *, js_ReportOverRecursed));
        jump(&exception);
    }

    bind(&interpret);
    {
        // Reserve space for Interpret() to store a Value.
        subPtr(Imm32(sizeof(Value)), StackPointer);
        mov(StackPointer, ReturnReg);

        // Call out to the interpreter.
        setupUnalignedABICall(1, scratch);
        passABIArg(ReturnReg);
        callWithABI(JS_FUNC_TO_DATA_PTR(void *, ThunkToInterpreter));

        // Load the value the interpreter returned.
        popValue(JSReturnOperand);

        // Check for an exception.
        JS_STATIC_ASSERT(!Interpret_Error);
        branchTest32(Zero, ReturnReg, ReturnReg, &exception);

        // Remove the exitCode pointer from the stack.
        leaveExitFrame();

        branch32(Equal, ReturnReg, Imm32(Interpret_OSR), &osr);

        // Return to the caller.
        ret();
    }

    bind(&osr);
    {
        unboxPrivate(JSReturnOperand, OsrFrameReg);
        performOsr();
    }

    bind(&exception);
    {
        handleException();
    }
}

void printf0_(const char *output) {
    printf("%s", output);
}

void
MacroAssembler::printf(const char *output)
{
    RegisterSet regs = RegisterSet::Volatile();
    PushRegsInMask(regs);

    Register temp = regs.takeGeneral();

    setupUnalignedABICall(1, temp);
    movePtr(ImmWord(output), temp);
    passABIArg(temp);
    callWithABI(JS_FUNC_TO_DATA_PTR(void *, printf0_));

    PopRegsInMask(RegisterSet::Volatile());
}

void printf1_(const char *output, uintptr_t value) {
    char *line = JS_sprintf_append(NULL, output, value);
    printf("%s", line);
    js_free(line);
}

void
MacroAssembler::printf(const char *output, Register value)
{
    RegisterSet regs = RegisterSet::Volatile();
    PushRegsInMask(regs);

    regs.maybeTake(value);

    Register temp = regs.takeGeneral();

    setupUnalignedABICall(2, temp);
    movePtr(ImmWord(output), temp);
    passABIArg(temp);
    passABIArg(value);
    callWithABI(JS_FUNC_TO_DATA_PTR(void *, printf1_));

    PopRegsInMask(RegisterSet::Volatile());
}

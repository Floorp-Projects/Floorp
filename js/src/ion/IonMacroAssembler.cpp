/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ion/IonMacroAssembler.h"

#include "jsinfer.h"

#include "ion/AsmJS.h"
#include "ion/Bailouts.h"
#include "ion/BaselineFrame.h"
#include "ion/BaselineIC.h"
#include "ion/BaselineJIT.h"
#include "ion/BaselineRegisters.h"
#include "ion/IonMacroAssembler.h"
#include "ion/MIR.h"
#include "js/RootingAPI.h"
#include "vm/ForkJoin.h"

#include "jsgcinlines.h"
#include "jsinferinlines.h"

#include "vm/Shape-inl.h"

using namespace js;
using namespace js::ion;

// Emulate a TypeSet logic from a Type object to avoid duplicating the guard
// logic.
class TypeWrapper {
    types::Type t_;

  public:
    TypeWrapper(types::Type t) : t_(t) {}

    inline bool unknown() const {
        return t_.isUnknown();
    }
    inline bool hasType(types::Type t) const {
        if (t == types::Type::Int32Type())
            return t == t_ || t_ == types::Type::DoubleType();
        return t == t_;
    }
    inline unsigned getObjectCount() const {
        if (t_.isAnyObject() || t_.isUnknown() || !t_.isObject())
            return 0;
        return 1;
    }
    inline JSObject *getSingleObject(unsigned) const {
        if (t_.isSingleObject())
            return t_.singleObject();
        return NULL;
    }
    inline types::TypeObject *getTypeObject(unsigned) const {
        if (t_.isTypeObject())
            return t_.typeObject();
        return NULL;
    }
};

template <typename Source, typename TypeSet> void
MacroAssembler::guardTypeSet(const Source &address, const TypeSet *types,
                             Register scratch, Label *matched, Label *miss)
{
    JS_ASSERT(!types->unknown());

    Register tag = extractTag(address, scratch);

    if (types->hasType(types::Type::DoubleType())) {
        // The double type also implies Int32.
        JS_ASSERT(types->hasType(types::Type::Int32Type()));
        branchTestNumber(Equal, tag, matched);
    } else if (types->hasType(types::Type::Int32Type())) {
        branchTestInt32(Equal, tag, matched);
    }

    if (types->hasType(types::Type::UndefinedType()))
        branchTestUndefined(Equal, tag, matched);
    if (types->hasType(types::Type::BooleanType()))
        branchTestBoolean(Equal, tag, matched);
    if (types->hasType(types::Type::StringType()))
        branchTestString(Equal, tag, matched);
    if (types->hasType(types::Type::NullType()))
        branchTestNull(Equal, tag, matched);
    if (types->hasType(types::Type::MagicArgType()))
        branchTestMagic(Equal, tag, matched);

    if (types->hasType(types::Type::AnyObjectType())) {
        branchTestObject(Equal, tag, matched);
    } else if (types->getObjectCount()) {
        JS_ASSERT(scratch != InvalidReg);
        branchTestObject(NotEqual, tag, miss);
        Register obj = extractObject(address, scratch);

        unsigned count = types->getObjectCount();
        for (unsigned i = 0; i < count; i++) {
            if (JSObject *object = types->getSingleObject(i))
                branchPtr(Equal, obj, ImmGCPtr(object), matched);
        }

        loadPtr(Address(obj, JSObject::offsetOfType()), scratch);

        for (unsigned i = 0; i < count; i++) {
            if (types::TypeObject *object = types->getTypeObject(i))
                branchPtr(Equal, scratch, ImmGCPtr(object), matched);
        }
    }
}

template <typename Source> void
MacroAssembler::guardType(const Source &address, types::Type type,
                          Register scratch, Label *matched, Label *miss)
{
    TypeWrapper wrapper(type);
    guardTypeSet(address, &wrapper, scratch, matched, miss);
}

template void MacroAssembler::guardTypeSet(const Address &address, const types::StackTypeSet *types,
                                           Register scratch, Label *matched, Label *miss);
template void MacroAssembler::guardTypeSet(const ValueOperand &value, const types::StackTypeSet *types,
                                           Register scratch, Label *matched, Label *miss);

template void MacroAssembler::guardTypeSet(const Address &address, const types::TypeSet *types,
                                           Register scratch, Label *matched, Label *miss);
template void MacroAssembler::guardTypeSet(const ValueOperand &value, const types::TypeSet *types,
                                           Register scratch, Label *matched, Label *miss);

template void MacroAssembler::guardTypeSet(const Address &address, const TypeWrapper *types,
                                           Register scratch, Label *matched, Label *miss);
template void MacroAssembler::guardTypeSet(const ValueOperand &value, const TypeWrapper *types,
                                           Register scratch, Label *matched, Label *miss);

template void MacroAssembler::guardType(const Address &address, types::Type type,
                                        Register scratch, Label *matched, Label *miss);
template void MacroAssembler::guardType(const ValueOperand &value, types::Type type,
                                        Register scratch, Label *matched, Label *miss);

void
MacroAssembler::PushRegsInMask(RegisterSet set)
{
    int32_t diffF = set.fpus().size() * sizeof(double);
    int32_t diffG = set.gprs().size() * STACK_SLOT_SIZE;

#if defined(JS_CPU_X86) || defined(JS_CPU_X64)
    // On x86, always use push to push the integer registers, as it's fast
    // on modern hardware and it's a small instruction.
    for (GeneralRegisterBackwardIterator iter(set.gprs()); iter.more(); iter++) {
        diffG -= STACK_SLOT_SIZE;
        Push(*iter);
    }
#elif defined(JS_CPU_ARM)
    if (set.gprs().size() > 1) {
        adjustFrame(diffG);
        startDataTransferM(IsStore, StackPointer, DB, WriteBack);
        for (GeneralRegisterBackwardIterator iter(set.gprs()); iter.more(); iter++) {
            diffG -= STACK_SLOT_SIZE;
            transferReg(*iter);
        }
        finishDataTransfer();
    } else {
        reserveStack(diffG);
        for (GeneralRegisterBackwardIterator iter(set.gprs()); iter.more(); iter++) {
            diffG -= STACK_SLOT_SIZE;
            storePtr(*iter, Address(StackPointer, diffG));
        }
    }
#else
    reserveStack(diffG);
    for (GeneralRegisterBackwardIterator iter(set.gprs()); iter.more(); iter++) {
        diffG -= STACK_SLOT_SIZE;
        storePtr(*iter, Address(StackPointer, diffG));
    }
#endif
    JS_ASSERT(diffG == 0);

#ifdef JS_CPU_ARM
    adjustFrame(diffF);
    diffF += transferMultipleByRuns(set.fpus(), IsStore, StackPointer, DB);
#else
    reserveStack(diffF);
    for (FloatRegisterBackwardIterator iter(set.fpus()); iter.more(); iter++) {
        diffF -= sizeof(double);
        storeDouble(*iter, Address(StackPointer, diffF));
    }
#endif
    JS_ASSERT(diffF == 0);
}

void
MacroAssembler::PopRegsInMaskIgnore(RegisterSet set, RegisterSet ignore)
{
    int32_t diffG = set.gprs().size() * STACK_SLOT_SIZE;
    int32_t diffF = set.fpus().size() * sizeof(double);
    const int32_t reservedG = diffG;
    const int32_t reservedF = diffF;

#ifdef JS_CPU_ARM
    // ARM can load multiple registers at once, but only if we want back all
    // the registers we previously saved to the stack.
    if (ignore.empty(true)) {
        diffF -= transferMultipleByRuns(set.fpus(), IsLoad, StackPointer, IA);
        adjustFrame(-reservedF);
    } else
#endif
    {
        for (FloatRegisterBackwardIterator iter(set.fpus()); iter.more(); iter++) {
            diffF -= sizeof(double);
            if (!ignore.has(*iter))
                loadDouble(Address(StackPointer, diffF), *iter);
        }
        freeStack(reservedF);
    }
    JS_ASSERT(diffF == 0);

#if defined(JS_CPU_X86) || defined(JS_CPU_X64)
    // On x86, use pop to pop the integer registers, if we're not going to
    // ignore any slots, as it's fast on modern hardware and it's a small
    // instruction.
    if (ignore.empty(false)) {
        for (GeneralRegisterForwardIterator iter(set.gprs()); iter.more(); iter++) {
            diffG -= STACK_SLOT_SIZE;
            Pop(*iter);
        }
    } else
#endif
#ifdef JS_CPU_ARM
    if (set.gprs().size() > 1 && ignore.empty(false)) {
        startDataTransferM(IsLoad, StackPointer, IA, WriteBack);
        for (GeneralRegisterBackwardIterator iter(set.gprs()); iter.more(); iter++) {
            diffG -= STACK_SLOT_SIZE;
            transferReg(*iter);
        }
        finishDataTransfer();
        adjustFrame(-reservedG);
    } else
#endif
    {
        for (GeneralRegisterBackwardIterator iter(set.gprs()); iter.more(); iter++) {
            diffG -= STACK_SLOT_SIZE;
            if (!ignore.has(*iter))
                loadPtr(Address(StackPointer, diffG), *iter);
        }
        freeStack(reservedG);
    }
    JS_ASSERT(diffG == 0);
}

void
MacroAssembler::branchNurseryPtr(Condition cond, const Address &ptr1, const ImmMaybeNurseryPtr &ptr2,
                                 Label *label)
{
#ifdef JSGC_GENERATIONAL
    if (ptr2.value && gc::IsInsideNursery(GetIonContext()->cx->runtime(), (void *)ptr2.value))
        embedsNurseryPointers_ = true;
#endif
    branchPtr(cond, ptr1, ptr2, label);
}

void
MacroAssembler::moveNurseryPtr(const ImmMaybeNurseryPtr &ptr, const Register &reg)
{
#ifdef JSGC_GENERATIONAL
    if (ptr.value && gc::IsInsideNursery(GetIonContext()->cx->runtime(), (void *)ptr.value))
        embedsNurseryPointers_ = true;
#endif
    movePtr(ptr, reg);
}

template<typename T>
void
MacroAssembler::loadFromTypedArray(int arrayType, const T &src, AnyRegister dest, Register temp,
                                   Label *fail)
{
    switch (arrayType) {
      case TypedArrayObject::TYPE_INT8:
        load8SignExtend(src, dest.gpr());
        break;
      case TypedArrayObject::TYPE_UINT8:
      case TypedArrayObject::TYPE_UINT8_CLAMPED:
        load8ZeroExtend(src, dest.gpr());
        break;
      case TypedArrayObject::TYPE_INT16:
        load16SignExtend(src, dest.gpr());
        break;
      case TypedArrayObject::TYPE_UINT16:
        load16ZeroExtend(src, dest.gpr());
        break;
      case TypedArrayObject::TYPE_INT32:
        load32(src, dest.gpr());
        break;
      case TypedArrayObject::TYPE_UINT32:
        if (dest.isFloat()) {
            load32(src, temp);
            convertUInt32ToDouble(temp, dest.fpu());
        } else {
            load32(src, dest.gpr());
            test32(dest.gpr(), dest.gpr());
            j(Assembler::Signed, fail);
        }
        break;
      case TypedArrayObject::TYPE_FLOAT32:
      case TypedArrayObject::TYPE_FLOAT64:
        if (arrayType == TypedArrayObject::TYPE_FLOAT32)
            loadFloatAsDouble(src, dest.fpu());
        else
            loadDouble(src, dest.fpu());
        canonicalizeDouble(dest.fpu());
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("Invalid typed array type");
    }
}

template void MacroAssembler::loadFromTypedArray(int arrayType, const Address &src, AnyRegister dest,
                                                 Register temp, Label *fail);
template void MacroAssembler::loadFromTypedArray(int arrayType, const BaseIndex &src, AnyRegister dest,
                                                 Register temp, Label *fail);

template<typename T>
void
MacroAssembler::loadFromTypedArray(int arrayType, const T &src, const ValueOperand &dest,
                                   bool allowDouble, Register temp, Label *fail)
{
    switch (arrayType) {
      case TypedArrayObject::TYPE_INT8:
      case TypedArrayObject::TYPE_UINT8:
      case TypedArrayObject::TYPE_UINT8_CLAMPED:
      case TypedArrayObject::TYPE_INT16:
      case TypedArrayObject::TYPE_UINT16:
      case TypedArrayObject::TYPE_INT32:
        loadFromTypedArray(arrayType, src, AnyRegister(dest.scratchReg()), InvalidReg, NULL);
        tagValue(JSVAL_TYPE_INT32, dest.scratchReg(), dest);
        break;
      case TypedArrayObject::TYPE_UINT32:
        // Don't clobber dest when we could fail, instead use temp.
        load32(src, temp);
        test32(temp, temp);
        if (allowDouble) {
            // If the value fits in an int32, store an int32 type tag.
            // Else, convert the value to double and box it.
            Label done, isDouble;
            j(Assembler::Signed, &isDouble);
            {
                tagValue(JSVAL_TYPE_INT32, temp, dest);
                jump(&done);
            }
            bind(&isDouble);
            {
                convertUInt32ToDouble(temp, ScratchFloatReg);
                boxDouble(ScratchFloatReg, dest);
            }
            bind(&done);
        } else {
            // Bailout if the value does not fit in an int32.
            j(Assembler::Signed, fail);
            tagValue(JSVAL_TYPE_INT32, temp, dest);
        }
        break;
      case TypedArrayObject::TYPE_FLOAT32:
      case TypedArrayObject::TYPE_FLOAT64:
        loadFromTypedArray(arrayType, src, AnyRegister(ScratchFloatReg), dest.scratchReg(), NULL);
        boxDouble(ScratchFloatReg, dest);
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("Invalid typed array type");
    }
}

template void MacroAssembler::loadFromTypedArray(int arrayType, const Address &src, const ValueOperand &dest,
                                                 bool allowDouble, Register temp, Label *fail);
template void MacroAssembler::loadFromTypedArray(int arrayType, const BaseIndex &src, const ValueOperand &dest,
                                                 bool allowDouble, Register temp, Label *fail);

// Note: this function clobbers the input register.
void
MacroAssembler::clampDoubleToUint8(FloatRegister input, Register output)
{
    JS_ASSERT(input != ScratchFloatReg);
#ifdef JS_CPU_ARM
    ma_vimm(0.5, ScratchFloatReg);
    if (hasVFPv3()) {
        Label notSplit;
        ma_vadd(input, ScratchFloatReg, ScratchFloatReg);
        // Convert the double into an unsigned fixed point value with 24 bits of
        // precision. The resulting number will look like 0xII.DDDDDD
        as_vcvtFixed(ScratchFloatReg, false, 24, true);
        // Move the fixed point value into an integer register
        as_vxfer(output, InvalidReg, ScratchFloatReg, FloatToCore);
        // see if this value *might* have been an exact integer after adding 0.5
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

    } else {
        Label outOfRange;
        ma_vcmpz(input);
        // do the add, in place so we can reference it later
        ma_vadd(input, ScratchFloatReg, input);
        // do the conversion to an integer.
        as_vcvt(VFPRegister(ScratchFloatReg).uintOverlay(), VFPRegister(input));
        // copy the converted value out
        as_vxfer(output, InvalidReg, ScratchFloatReg, FloatToCore);
        as_vmrs(pc);
        ma_b(&outOfRange, Overflow);
        ma_cmp(output, Imm32(0xff));
        ma_mov(Imm32(0xff), output, NoSetCond, Above);
        ma_b(&outOfRange, Above);
        // convert it back to see if we got the same value back
        as_vcvt(ScratchFloatReg, VFPRegister(ScratchFloatReg).uintOverlay());
        // do the check
        as_vcmp(ScratchFloatReg, input);
        as_vmrs(pc);
        ma_bic(Imm32(1), output, NoSetCond, Zero);
        bind(&outOfRange);
    }
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
MacroAssembler::newGCThing(const Register &result, gc::AllocKind allocKind, Label *fail)
{
    // Inlined equivalent of js::gc::NewGCThing() without failure case handling.

    int thingSize = int(gc::Arena::thingSize(allocKind));

    Zone *zone = GetIonContext()->compartment->zone();

#ifdef JS_GC_ZEAL
    // Don't execute the inline path if gcZeal is active.
    movePtr(ImmWord(zone->rt), result);
    loadPtr(Address(result, offsetof(JSRuntime, gcZeal_)), result);
    branch32(Assembler::NotEqual, result, Imm32(0), fail);
#endif

    // Don't execute the inline path if the compartment has an object metadata callback,
    // as the metadata to use for the object may vary between executions of the op.
    if (GetIonContext()->compartment->objectMetadataCallback)
        jump(fail);

#ifdef JSGC_GENERATIONAL
    Nursery &nursery = zone->rt->gcNursery;
    if (nursery.isEnabled() && allocKind <= gc::FINALIZE_OBJECT_LAST) {
        // Inline Nursery::allocate. No explicit check for nursery.isEnabled()
        // is needed, as the comparison with the nursery's end will always fail
        // in such cases.
        loadPtr(AbsoluteAddress(nursery.addressOfPosition()), result);
        addPtr(Imm32(thingSize), result);
        branchPtr(Assembler::BelowOrEqual, AbsoluteAddress(nursery.addressOfCurrentEnd()), result, fail);
        storePtr(result, AbsoluteAddress(nursery.addressOfPosition()));
        subPtr(Imm32(thingSize), result);
        return;
    }
#endif // JSGC_GENERATIONAL

    // Inline FreeSpan::allocate.
    // There is always exactly one FreeSpan per allocKind per JSCompartment.
    // If a FreeSpan is replaced, its members are updated in the freeLists table,
    // which the code below always re-reads.
    gc::FreeSpan *list = const_cast<gc::FreeSpan *>
                         (zone->allocator.arenas.getFreeList(allocKind));
    loadPtr(AbsoluteAddress(&list->first), result);
    branchPtr(Assembler::BelowOrEqual, AbsoluteAddress(&list->last), result, fail);

    addPtr(Imm32(thingSize), result);
    storePtr(result, AbsoluteAddress(&list->first));
    subPtr(Imm32(thingSize), result);
}

void
MacroAssembler::newGCThing(const Register &result, JSObject *templateObject, Label *fail)
{
    gc::AllocKind allocKind = templateObject->tenuredGetAllocKind();
    JS_ASSERT(allocKind >= gc::FINALIZE_OBJECT0 && allocKind <= gc::FINALIZE_OBJECT_LAST);
    JS_ASSERT(!templateObject->hasDynamicElements());

    newGCThing(result, allocKind, fail);
}

void
MacroAssembler::newGCString(const Register &result, Label *fail)
{
    newGCThing(result, js::gc::FINALIZE_STRING, fail);
}

void
MacroAssembler::newGCShortString(const Register &result, Label *fail)
{
    newGCThing(result, js::gc::FINALIZE_SHORT_STRING, fail);
}

void
MacroAssembler::parNewGCThing(const Register &result,
                              const Register &threadContextReg,
                              const Register &tempReg1,
                              const Register &tempReg2,
                              gc::AllocKind allocKind,
                              Label *fail)
{
    // Similar to ::newGCThing(), except that it allocates from a
    // custom Allocator in the ForkJoinSlice*, rather than being
    // hardcoded to the compartment allocator.  This requires two
    // temporary registers.
    //
    // Subtle: I wanted to reuse `result` for one of the temporaries,
    // but the register allocator was assigning it to the same
    // register as `threadContextReg`.  Then we overwrite that
    // register which messed up the OOL code.

    uint32_t thingSize = (uint32_t)gc::Arena::thingSize(allocKind);

    // Load the allocator:
    // tempReg1 = (Allocator*) forkJoinSlice->allocator()
    loadPtr(Address(threadContextReg, ThreadSafeContext::offsetOfAllocator()),
            tempReg1);

    // Get a pointer to the relevant free list:
    // tempReg1 = (FreeSpan*) &tempReg1->arenas.freeLists[(allocKind)]
    uint32_t offset = (offsetof(Allocator, arenas) +
                       js::gc::ArenaLists::getFreeListOffset(allocKind));
    addPtr(Imm32(offset), tempReg1);

    // Load first item on the list
    // tempReg2 = tempReg1->first
    loadPtr(Address(tempReg1, offsetof(gc::FreeSpan, first)), tempReg2);

    // Check whether list is empty
    // if tempReg1->last <= tempReg2, fail
    branchPtr(Assembler::BelowOrEqual,
              Address(tempReg1, offsetof(gc::FreeSpan, last)),
              tempReg2,
              fail);

    // If not, take first and advance pointer by thingSize bytes.
    // result = tempReg2;
    // tempReg2 += thingSize;
    movePtr(tempReg2, result);
    addPtr(Imm32(thingSize), tempReg2);

    // Update `first`
    // tempReg1->first = tempReg2;
    storePtr(tempReg2, Address(tempReg1, offsetof(gc::FreeSpan, first)));
}

void
MacroAssembler::parNewGCThing(const Register &result,
                              const Register &threadContextReg,
                              const Register &tempReg1,
                              const Register &tempReg2,
                              JSObject *templateObject,
                              Label *fail)
{
    gc::AllocKind allocKind = templateObject->tenuredGetAllocKind();
    JS_ASSERT(allocKind >= gc::FINALIZE_OBJECT0 && allocKind <= gc::FINALIZE_OBJECT_LAST);
    JS_ASSERT(!templateObject->hasDynamicElements());

    parNewGCThing(result, threadContextReg, tempReg1, tempReg2, allocKind, fail);
}

void
MacroAssembler::parNewGCString(const Register &result,
                               const Register &threadContextReg,
                               const Register &tempReg1,
                               const Register &tempReg2,
                               Label *fail)
{
    parNewGCThing(result, threadContextReg, tempReg1, tempReg2, js::gc::FINALIZE_STRING, fail);
}

void
MacroAssembler::parNewGCShortString(const Register &result,
                                    const Register &threadContextReg,
                                    const Register &tempReg1,
                                    const Register &tempReg2,
                                    Label *fail)
{
    parNewGCThing(result, threadContextReg, tempReg1, tempReg2, js::gc::FINALIZE_SHORT_STRING, fail);
}

void
MacroAssembler::initGCThing(const Register &obj, JSObject *templateObject)
{
    // Fast initialization of an empty object returned by NewGCThing().

    storePtr(ImmGCPtr(templateObject->lastProperty()), Address(obj, JSObject::offsetOfShape()));
    storePtr(ImmGCPtr(templateObject->type()), Address(obj, JSObject::offsetOfType()));
    storePtr(ImmWord((void *)NULL), Address(obj, JSObject::offsetOfSlots()));

    if (templateObject->is<ArrayObject>()) {
        JS_ASSERT(!templateObject->getDenseInitializedLength());

        int elementsOffset = JSObject::offsetOfFixedElements();

        addPtr(Imm32(elementsOffset), obj);
        storePtr(obj, Address(obj, -elementsOffset + JSObject::offsetOfElements()));
        addPtr(Imm32(-elementsOffset), obj);

        // Fill in the elements header.
        store32(Imm32(templateObject->getDenseCapacity()),
                Address(obj, elementsOffset + ObjectElements::offsetOfCapacity()));
        store32(Imm32(templateObject->getDenseInitializedLength()),
                Address(obj, elementsOffset + ObjectElements::offsetOfInitializedLength()));
        store32(Imm32(templateObject->as<ArrayObject>().length()),
                Address(obj, elementsOffset + ObjectElements::offsetOfLength()));
        store32(Imm32(templateObject->shouldConvertDoubleElements()
                      ? ObjectElements::CONVERT_DOUBLE_ELEMENTS
                      : 0),
                Address(obj, elementsOffset + ObjectElements::offsetOfFlags()));
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
MacroAssembler::compareStrings(JSOp op, Register left, Register right, Register result,
                               Register temp, Label *fail)
{
    JS_ASSERT(IsEqualityOp(op));

    Label done;
    Label notPointerEqual;
    // Fast path for identical strings.
    branchPtr(Assembler::NotEqual, left, right, &notPointerEqual);
    move32(Imm32(op == JSOP_EQ || op == JSOP_STRICTEQ), result);
    jump(&done);

    bind(&notPointerEqual);
    loadPtr(Address(left, JSString::offsetOfLengthAndFlags()), result);
    loadPtr(Address(right, JSString::offsetOfLengthAndFlags()), temp);

    Label notAtom;
    // Optimize the equality operation to a pointer compare for two atoms.
    Imm32 atomBit(JSString::ATOM_BIT);
    branchTest32(Assembler::Zero, result, atomBit, &notAtom);
    branchTest32(Assembler::Zero, temp, atomBit, &notAtom);

    cmpPtr(left, right);
    emitSet(JSOpToCondition(MCompare::Compare_String, op), result);
    jump(&done);

    bind(&notAtom);
    // Strings of different length can never be equal.
    rshiftPtr(Imm32(JSString::LENGTH_SHIFT), result);
    rshiftPtr(Imm32(JSString::LENGTH_SHIFT), temp);
    branchPtr(Assembler::Equal, result, temp, fail);
    move32(Imm32(op == JSOP_NE || op == JSOP_STRICTNE), result);

    bind(&done);
}

void
MacroAssembler::parCheckInterruptFlags(const Register &tempReg,
                                       Label *fail)
{
    JSCompartment *compartment = GetIonContext()->compartment;

    void *interrupt = (void*)&compartment->rt->interrupt;
    movePtr(ImmWord(interrupt), tempReg);
    load32(Address(tempReg, 0), tempReg);
    branchTest32(Assembler::NonZero, tempReg, tempReg, fail);
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

        // Skip the or-ing of CalleeToken_Function into calleeToken since it is zero.
        JS_ASSERT(CalleeToken_Function == 0);

        loadPtr(Address(script, JSFunction::offsetOfNativeOrScript()), script);
    }

    bind(&performOsr);

    const Register ionScript = regs.takeAny();
    const Register osrEntry = regs.takeAny();

    loadPtr(Address(script, JSScript::offsetOfIonScript()), ionScript);
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

static void
ReportOverRecursed(JSContext *cx)
{
    js_ReportOverRecursed(cx);
}

void
MacroAssembler::generateBailoutTail(Register scratch, Register bailoutInfo)
{
    enterExitFrame();

    Label exception;
    Label baseline;

    // The return value from Bailout is tagged as:
    // - 0x0: done (enter baseline)
    // - 0x1: error (handle exception)
    // - 0x2: overrecursed

    branch32(Equal, ReturnReg, Imm32(BAILOUT_RETURN_OK), &baseline);
    branch32(Equal, ReturnReg, Imm32(BAILOUT_RETURN_FATAL_ERROR), &exception);

    // Fall-through: overrecursed.
    {
        loadJSContext(ReturnReg);
        setupUnalignedABICall(1, scratch);
        passABIArg(ReturnReg);
        callWithABI(JS_FUNC_TO_DATA_PTR(void *, ReportOverRecursed));
        jump(&exception);
    }

    bind(&exception);
    {
        handleException();
    }

    bind(&baseline);
    {
        // Prepare a register set for use in this case.
        GeneralRegisterSet regs(GeneralRegisterSet::All());
        JS_ASSERT(!regs.has(BaselineStackReg));
        regs.take(bailoutInfo);

        // Reset SP to the point where clobbering starts.
        loadPtr(Address(bailoutInfo, offsetof(BaselineBailoutInfo, incomingStack)),
                BaselineStackReg);

        Register copyCur = regs.takeAny();
        Register copyEnd = regs.takeAny();
        Register temp = regs.takeAny();

        // Copy data onto stack.
        loadPtr(Address(bailoutInfo, offsetof(BaselineBailoutInfo, copyStackTop)), copyCur);
        loadPtr(Address(bailoutInfo, offsetof(BaselineBailoutInfo, copyStackBottom)), copyEnd);
        {
            Label copyLoop;
            Label endOfCopy;
            bind(&copyLoop);
            branchPtr(Assembler::BelowOrEqual, copyCur, copyEnd, &endOfCopy);
            subPtr(Imm32(4), copyCur);
            subPtr(Imm32(4), BaselineStackReg);
            load32(Address(copyCur, 0), temp);
            store32(temp, Address(BaselineStackReg, 0));
            jump(&copyLoop);
            bind(&endOfCopy);
        }

        // Enter exit frame for the FinishBailoutToBaseline call.
        loadPtr(Address(bailoutInfo, offsetof(BaselineBailoutInfo, resumeFramePtr)), temp);
        load32(Address(temp, BaselineFrame::reverseOffsetOfFrameSize()), temp);
        makeFrameDescriptor(temp, IonFrame_BaselineJS);
        push(temp);
        loadPtr(Address(bailoutInfo, offsetof(BaselineBailoutInfo, resumeAddr)), temp);
        push(temp);
        enterFakeExitFrame();

        // If monitorStub is non-null, handle resumeAddr appropriately.
        Label noMonitor;
        Label done;
        branchPtr(Assembler::Equal,
                  Address(bailoutInfo, offsetof(BaselineBailoutInfo, monitorStub)),
                  ImmWord((void*) 0),
                  &noMonitor);

        //
        // Resuming into a monitoring stub chain.
        //
        {
            // Save needed values onto stack temporarily.
            pushValue(Address(bailoutInfo, offsetof(BaselineBailoutInfo, valueR0)));
            loadPtr(Address(bailoutInfo, offsetof(BaselineBailoutInfo, resumeFramePtr)), temp);
            push(temp);
            loadPtr(Address(bailoutInfo, offsetof(BaselineBailoutInfo, resumeAddr)), temp);
            push(temp);
            loadPtr(Address(bailoutInfo, offsetof(BaselineBailoutInfo, monitorStub)), temp);
            push(temp);

            // Call a stub to free allocated memory and create arguments objects.
            setupUnalignedABICall(1, temp);
            passABIArg(bailoutInfo);
            callWithABI(JS_FUNC_TO_DATA_PTR(void *, FinishBailoutToBaseline));
            branchTest32(Zero, ReturnReg, ReturnReg, &exception);

            // Restore values where they need to be and resume execution.
            GeneralRegisterSet enterMonRegs(GeneralRegisterSet::All());
            enterMonRegs.take(R0);
            enterMonRegs.take(BaselineStubReg);
            enterMonRegs.take(BaselineFrameReg);
            enterMonRegs.takeUnchecked(BaselineTailCallReg);
            Register jitcodeReg = enterMonRegs.takeAny();

            pop(BaselineStubReg);
            pop(BaselineTailCallReg);
            pop(BaselineFrameReg);
            popValue(R0);

            // Discard exit frame.
            addPtr(Imm32(IonExitFrameLayout::SizeWithFooter()), StackPointer);

            loadPtr(Address(BaselineStubReg, ICStub::offsetOfStubCode()), jitcodeReg);
#if defined(JS_CPU_X86) || defined(JS_CPU_X64)
            push(BaselineTailCallReg);
#endif
            jump(jitcodeReg);
        }

        //
        // Resuming into main jitcode.
        //
        bind(&noMonitor);
        {
            // Save needed values onto stack temporarily.
            pushValue(Address(bailoutInfo, offsetof(BaselineBailoutInfo, valueR0)));
            pushValue(Address(bailoutInfo, offsetof(BaselineBailoutInfo, valueR1)));
            loadPtr(Address(bailoutInfo, offsetof(BaselineBailoutInfo, resumeFramePtr)), temp);
            push(temp);
            loadPtr(Address(bailoutInfo, offsetof(BaselineBailoutInfo, resumeAddr)), temp);
            push(temp);

            // Call a stub to free allocated memory and create arguments objects.
            setupUnalignedABICall(1, temp);
            passABIArg(bailoutInfo);
            callWithABI(JS_FUNC_TO_DATA_PTR(void *, FinishBailoutToBaseline));
            branchTest32(Zero, ReturnReg, ReturnReg, &exception);

            // Restore values where they need to be and resume execution.
            GeneralRegisterSet enterRegs(GeneralRegisterSet::All());
            enterRegs.take(R0);
            enterRegs.take(R1);
            enterRegs.take(BaselineFrameReg);
            Register jitcodeReg = enterRegs.takeAny();

            pop(jitcodeReg);
            pop(BaselineFrameReg);
            popValue(R1);
            popValue(R0);

            // Discard exit frame.
            addPtr(Imm32(IonExitFrameLayout::SizeWithFooter()), StackPointer);

            jump(jitcodeReg);
        }
    }
}

void
MacroAssembler::loadBaselineOrIonRaw(Register script, Register dest, ExecutionMode mode,
                                     Label *failure)
{
    if (mode == SequentialExecution) {
        loadPtr(Address(script, JSScript::offsetOfBaselineOrIonRaw()), dest);
        if (failure)
            branchTestPtr(Assembler::Zero, dest, dest, failure);
    } else {
        loadPtr(Address(script, JSScript::offsetOfParallelIonScript()), dest);
        if (failure)
            branchPtr(Assembler::BelowOrEqual, dest, ImmWord(ION_COMPILING_SCRIPT), failure);
        loadPtr(Address(dest, IonScript::offsetOfMethod()), dest);
        loadPtr(Address(dest, IonCode::offsetOfCode()), dest);
    }
}

void
MacroAssembler::loadBaselineOrIonNoArgCheck(Register script, Register dest, ExecutionMode mode,
                                            Label *failure)
{
    if (mode == SequentialExecution) {
        loadPtr(Address(script, JSScript::offsetOfBaselineOrIonSkipArgCheck()), dest);
        if (failure)
            branchTestPtr(Assembler::Zero, dest, dest, failure);
    } else {
        // Find second register to get the offset to skip argument check
        Register offset = script;
        if (script == dest) {
            GeneralRegisterSet regs(GeneralRegisterSet::All());
            regs.take(dest);
            offset = regs.takeAny();
        }

        loadPtr(Address(script, JSScript::offsetOfParallelIonScript()), dest);
        if (failure)
            branchPtr(Assembler::BelowOrEqual, dest, ImmWord(ION_COMPILING_SCRIPT), failure);

        Push(offset);
        load32(Address(script, IonScript::offsetOfSkipArgCheckEntryOffset()), offset);

        loadPtr(Address(dest, IonScript::offsetOfMethod()), dest);
        loadPtr(Address(dest, IonCode::offsetOfCode()), dest);
        addPtr(offset, dest);

        Pop(offset);
    }
}

void
MacroAssembler::loadBaselineFramePtr(Register framePtr, Register dest)
{
    movePtr(framePtr, dest);
    subPtr(Imm32(BaselineFrame::Size()), dest);
}

void
MacroAssembler::loadForkJoinSlice(Register slice, Register scratch)
{
    // Load the current ForkJoinSlice *. If we need a parallel exit frame,
    // chances are we are about to do something very slow anyways, so just
    // call ParForkJoinSlice again instead of using the cached version.
    setupUnalignedABICall(0, scratch);
    callWithABI(JS_FUNC_TO_DATA_PTR(void *, ParForkJoinSlice));
    if (ReturnReg != slice)
        movePtr(ReturnReg, slice);
}

void
MacroAssembler::loadContext(Register cxReg, Register scratch, ExecutionMode executionMode)
{
    switch (executionMode) {
      case SequentialExecution:
        // The scratch register is not used for sequential execution.
        loadJSContext(cxReg);
        break;
      case ParallelExecution:
        loadForkJoinSlice(cxReg, scratch);
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("No such execution mode");
    }
}

void
MacroAssembler::enterParallelExitFrameAndLoadSlice(const VMFunction *f, Register slice,
                                                   Register scratch)
{
    loadForkJoinSlice(slice, scratch);
    // Load the PerThreadData from from the slice.
    loadPtr(Address(slice, offsetof(ForkJoinSlice, perThreadData)), scratch);
    linkParallelExitFrame(scratch);
    // Push the ioncode.
    exitCodePatch_ = PushWithPatch(ImmWord(-1));
    // Push the VMFunction pointer, to mark arguments.
    Push(ImmWord(f));
}

void
MacroAssembler::enterFakeParallelExitFrame(Register slice, Register scratch,
                                           IonCode *codeVal)
{
    // Load the PerThreadData from from the slice.
    loadPtr(Address(slice, offsetof(ForkJoinSlice, perThreadData)), scratch);
    linkParallelExitFrame(scratch);
    Push(ImmWord(uintptr_t(codeVal)));
    Push(ImmWord(uintptr_t(NULL)));
}

void
MacroAssembler::enterExitFrameAndLoadContext(const VMFunction *f, Register cxReg, Register scratch,
                                             ExecutionMode executionMode)
{
    switch (executionMode) {
      case SequentialExecution:
        // The scratch register is not used for sequential execution.
        enterExitFrame(f);
        loadJSContext(cxReg);
        break;
      case ParallelExecution:
        enterParallelExitFrameAndLoadSlice(f, cxReg, scratch);
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("No such execution mode");
    }
}

void
MacroAssembler::enterFakeExitFrame(Register cxReg, Register scratch,
                                   ExecutionMode executionMode,
                                   IonCode *codeVal)
{
    switch (executionMode) {
      case SequentialExecution:
        // The cx and scratch registers are not used for sequential execution.
        enterFakeExitFrame(codeVal);
        break;
      case ParallelExecution:
        enterFakeParallelExitFrame(cxReg, scratch, codeVal);
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("No such execution mode");
    }
}

void
MacroAssembler::handleFailure(ExecutionMode executionMode)
{
    // Re-entry code is irrelevant because the exception will leave the
    // running function and never come back
    if (sps_)
        sps_->skipNextReenter();
    leaveSPSFrame();

    void *handler;
    switch (executionMode) {
      case SequentialExecution:
        handler = JS_FUNC_TO_DATA_PTR(void *, ion::HandleException);
        break;
      case ParallelExecution:
        handler = JS_FUNC_TO_DATA_PTR(void *, ion::HandleParallelFailure);
        break;
      default:
        MOZ_ASSUME_UNREACHABLE("No such execution mode");
    }
    MacroAssemblerSpecific::handleFailureWithHandler(handler);

    // Doesn't actually emit code, but balances the leave()
    if (sps_)
        sps_->reenter(*this, InvalidReg);
}

void
MacroAssembler::pushCalleeToken(Register callee, ExecutionMode mode)
{
    // Tag and push a callee, then clear the tag after pushing. This is needed
    // if we dereference the callee pointer after pushing it as part of a
    // frame.
    tagCallee(callee, mode);
    push(callee);
    clearCalleeTag(callee, mode);
}

void
MacroAssembler::PushCalleeToken(Register callee, ExecutionMode mode)
{
    tagCallee(callee, mode);
    Push(callee);
    clearCalleeTag(callee, mode);
}

void
MacroAssembler::tagCallee(Register callee, ExecutionMode mode)
{
    switch (mode) {
      case SequentialExecution:
        // CalleeToken_Function is untagged, so we don't need to do anything.
        return;
      case ParallelExecution:
        orPtr(Imm32(CalleeToken_ParallelFunction), callee);
        return;
      default:
        MOZ_ASSUME_UNREACHABLE("No such execution mode");
    }
}

void
MacroAssembler::clearCalleeTag(Register callee, ExecutionMode mode)
{
    switch (mode) {
      case SequentialExecution:
        // CalleeToken_Function is untagged, so we don't need to do anything.
        return;
      case ParallelExecution:
        andPtr(Imm32(~0x3), callee);
        return;
      default:
        MOZ_ASSUME_UNREACHABLE("No such execution mode");
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

#if JS_TRACE_LOGGING
void
MacroAssembler::tracelogStart(JSScript *script)
{
    void (&TraceLogStart)(TraceLogging*, TraceLogging::Type, JSScript*) = TraceLog;
    RegisterSet regs = RegisterSet::Volatile();
    PushRegsInMask(regs);

    Register temp = regs.takeGeneral();
    Register logger = regs.takeGeneral();
    Register type = regs.takeGeneral();
    Register rscript = regs.takeGeneral();

    setupUnalignedABICall(3, temp);
    movePtr(ImmWord((void *)TraceLogging::defaultLogger()), logger);
    passABIArg(logger);
    move32(Imm32(TraceLogging::SCRIPT_START), type);
    passABIArg(type);
    movePtr(ImmGCPtr(script), rscript);
    passABIArg(rscript);
    callWithABI(JS_FUNC_TO_DATA_PTR(void *, TraceLogStart));

    PopRegsInMask(RegisterSet::Volatile());
}

void
MacroAssembler::tracelogStop()
{
    void (&TraceLogStop)(TraceLogging*, TraceLogging::Type) = TraceLog;
    RegisterSet regs = RegisterSet::Volatile();
    PushRegsInMask(regs);

    Register temp = regs.takeGeneral();
    Register logger = regs.takeGeneral();
    Register type = regs.takeGeneral();

    setupUnalignedABICall(2, temp);
    movePtr(ImmWord((void *)TraceLogging::defaultLogger()), logger);
    passABIArg(logger);
    move32(Imm32(TraceLogging::SCRIPT_STOP), type);
    passABIArg(type);
    callWithABI(JS_FUNC_TO_DATA_PTR(void *, TraceLogStop));

    PopRegsInMask(RegisterSet::Volatile());
}

void
MacroAssembler::tracelogLog(TraceLogging::Type type)
{
    void (&TraceLogStop)(TraceLogging*, TraceLogging::Type) = TraceLog;
    RegisterSet regs = RegisterSet::Volatile();
    PushRegsInMask(regs);

    Register temp = regs.takeGeneral();
    Register logger = regs.takeGeneral();
    Register rtype = regs.takeGeneral();

    setupUnalignedABICall(2, temp);
    movePtr(ImmWord((void *)TraceLogging::defaultLogger()), logger);
    passABIArg(logger);
    move32(Imm32(type), rtype);
    passABIArg(rtype);
    callWithABI(JS_FUNC_TO_DATA_PTR(void *, TraceLogStop));

    PopRegsInMask(RegisterSet::Volatile());
}
#endif

void
MacroAssembler::convertInt32ValueToDouble(const Address &address, Register scratch, Label *done)
{
    branchTestInt32(Assembler::NotEqual, address, done);
    unboxInt32(address, scratch);
    convertInt32ToDouble(scratch, ScratchFloatReg);
    storeDouble(ScratchFloatReg, address);
}

static const double DoubleZero = 0.0;

void
MacroAssembler::convertValueToDouble(ValueOperand value, FloatRegister output, Label *fail)
{
    Register tag = splitTagForTest(value);

    Label isDouble, isInt32, isBool, isNull, done;

    branchTestDouble(Assembler::Equal, tag, &isDouble);
    branchTestInt32(Assembler::Equal, tag, &isInt32);
    branchTestBoolean(Assembler::Equal, tag, &isBool);
    branchTestNull(Assembler::Equal, tag, &isNull);
    branchTestUndefined(Assembler::NotEqual, tag, fail);

    // fall-through: undefined
    loadStaticDouble(&js_NaN, output);
    jump(&done);

    bind(&isNull);
    loadStaticDouble(&DoubleZero, output);
    jump(&done);

    bind(&isBool);
    boolValueToDouble(value, output);
    jump(&done);

    bind(&isInt32);
    int32ValueToDouble(value, output);
    jump(&done);

    bind(&isDouble);
    unboxDouble(value, output);
    bind(&done);
}

void
MacroAssembler::convertValueToInt32(ValueOperand value, FloatRegister temp,
                                    Register output, Label *fail)
{
    Register tag = splitTagForTest(value);

    Label done, simple, isInt32, isBool, isDouble;

    branchTestInt32(Assembler::Equal, tag, &isInt32);
    branchTestBoolean(Assembler::Equal, tag, &isBool);
    branchTestDouble(Assembler::Equal, tag, &isDouble);
    branchTestNull(Assembler::NotEqual, tag, fail);

    // The value is null - just emit 0.
    mov(Imm32(0), output);
    jump(&done);

    // Try converting double into integer
    bind(&isDouble);
    unboxDouble(value, temp);
    convertDoubleToInt32(temp, output, fail, /* -0 check */ false);
    jump(&done);

    // Just unbox a bool, the result is 0 or 1.
    bind(&isBool);
    unboxBoolean(value, output);
    jump(&done);

    // Integers can be unboxed.
    bind(&isInt32);
    unboxInt32(value, output);

    bind(&done);
}

void
MacroAssembler::PushEmptyRooted(VMFunction::RootType rootType)
{
    switch (rootType) {
      case VMFunction::RootNone:
        MOZ_ASSUME_UNREACHABLE("Handle must have root type");
      case VMFunction::RootObject:
      case VMFunction::RootString:
      case VMFunction::RootPropertyName:
      case VMFunction::RootFunction:
      case VMFunction::RootCell:
        Push(ImmWord((void *)NULL));
        break;
      case VMFunction::RootValue:
        Push(UndefinedValue());
        break;
    }
}

void
MacroAssembler::popRooted(VMFunction::RootType rootType, Register cellReg,
                          const ValueOperand &valueReg)
{
    switch (rootType) {
      case VMFunction::RootNone:
        MOZ_ASSUME_UNREACHABLE("Handle must have root type");
      case VMFunction::RootObject:
      case VMFunction::RootString:
      case VMFunction::RootPropertyName:
      case VMFunction::RootFunction:
      case VMFunction::RootCell:
        Pop(cellReg);
        break;
      case VMFunction::RootValue:
        Pop(valueReg);
        break;
    }
}

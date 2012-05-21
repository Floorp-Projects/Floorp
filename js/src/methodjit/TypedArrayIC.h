/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_typedarray_ic_h___
#define js_typedarray_ic_h___

#include "jscntxt.h"
#include "jstypedarray.h"

#include "vm/NumericConversions.h"

#include "jsnuminlines.h"
#include "jstypedarrayinlines.h"

namespace js {
namespace mjit {

#ifdef JS_METHODJIT_TYPED_ARRAY

typedef JSC::MacroAssembler::RegisterID RegisterID;
typedef JSC::MacroAssembler::FPRegisterID FPRegisterID;
typedef JSC::MacroAssembler::Jump Jump;
typedef JSC::MacroAssembler::Imm32 Imm32;
typedef JSC::MacroAssembler::ImmDouble ImmDouble;

static inline bool
ConstantFoldForFloatArray(JSContext *cx, ValueRemat *vr)
{
    if (!vr->isTypeKnown())
        return true;

    // Objects and undefined coerce to NaN, which coerces to 0.
    // Null converts to 0.
    if (vr->knownType() == JSVAL_TYPE_OBJECT ||
        vr->knownType() == JSVAL_TYPE_UNDEFINED) {
        *vr = ValueRemat::FromConstant(DoubleValue(js_NaN));
        return true;
    }
    if (vr->knownType() == JSVAL_TYPE_NULL) {
        *vr = ValueRemat::FromConstant(DoubleValue(0));
        return true;
    }

    if (!vr->isConstant())
        return true;

    if (vr->knownType() == JSVAL_TYPE_DOUBLE)
        return true;

    double d = 0;
    Value v = vr->value();
    if (v.isString()) {
        if (!StringToNumberType<double>(cx, v.toString(), &d))
            return false;
    } else if (v.isBoolean()) {
        d = v.toBoolean() ? 1 : 0;
    } else if (v.isInt32()) {
        d = v.toInt32();
    } else {
        JS_NOT_REACHED("unknown constant type");
    }
    *vr = ValueRemat::FromConstant(DoubleValue(d));
    return true;
}

static inline bool
ConstantFoldForIntArray(JSContext *cx, JSObject *tarray, ValueRemat *vr)
{
    if (!vr->isTypeKnown())
        return true;

    // Objects and undefined coerce to NaN, which coerces to 0.
    // Null converts to 0.
    if (vr->knownType() == JSVAL_TYPE_OBJECT ||
        vr->knownType() == JSVAL_TYPE_UNDEFINED ||
        vr->knownType() == JSVAL_TYPE_NULL) {
        *vr = ValueRemat::FromConstant(Int32Value(0));
        return true;
    }

    if (!vr->isConstant())
        return true;

    // Convert from string to double first (see bug 624483).
    Value v = vr->value();
    if (v.isString()) {
        double d;
        if (!StringToNumberType<double>(cx, v.toString(), &d))
            return false;
        v.setNumber(d);
    }

    int32_t i32 = 0;
    if (v.isDouble()) {
        i32 = (TypedArray::getType(tarray) == js::TypedArray::TYPE_UINT8_CLAMPED)
              ? ClampDoubleToUint8(v.toDouble())
              : ToInt32(v.toDouble());
    } else if (v.isInt32()) {
        i32 = v.toInt32();
        if (TypedArray::getType(tarray) == js::TypedArray::TYPE_UINT8_CLAMPED)
            i32 = ClampIntForUint8Array(i32);
    } else if (v.isBoolean()) {
        i32 = v.toBoolean() ? 1 : 0;
    } else {
        JS_NOT_REACHED("unknown constant type");
    }

    *vr = ValueRemat::FromConstant(Int32Value(i32));

    return true;
}

// Generate code that will ensure a dynamically typed value, pinned in |vr|,
// can be stored in an integer typed array. If any sort of conversion is
// required, |dataReg| will be clobbered by a new value. |saveMask| is
// used to ensure that |dataReg| (and volatile registers) are preserved
// across any conversion process.
static void
GenConversionForIntArray(Assembler &masm, JSObject *tarray, const ValueRemat &vr,
                         uint32_t saveMask)
{
    if (vr.isConstant()) {
        // Constants are always folded to ints up-front.
        JS_ASSERT(vr.knownType() == JSVAL_TYPE_INT32);
        return;
    }

    if (!vr.isTypeKnown() || vr.knownType() != JSVAL_TYPE_INT32) {
        // If a conversion is necessary, save registers now.
        MaybeJump checkInt32;
        if (!vr.isTypeKnown())
            checkInt32 = masm.testInt32(Assembler::Equal, vr.typeReg());

        // Store the value to convert.
        StackMarker vp = masm.allocStack(sizeof(Value), sizeof(double));
        masm.storeValue(vr, masm.addressOfExtra(vp));

        // Preserve volatile registers.
        PreserveRegisters saveForCall(masm);
        saveForCall.preserve(saveMask & Registers::TempRegs);

        masm.setupABICall(Registers::FastCall, 2);
        masm.storeArg(0, masm.vmFrameOffset(offsetof(VMFrame, cx)));
        masm.storeArgAddr(1, masm.addressOfExtra(vp));

        typedef int32_t (JS_FASTCALL *Int32CxVp)(JSContext *, Value *);
        Int32CxVp stub;
        if (TypedArray::getType(tarray) == js::TypedArray::TYPE_UINT8_CLAMPED)
            stub = stubs::ConvertToTypedInt<true>;
        else
            stub = stubs::ConvertToTypedInt<false>;
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, stub), false);
        if (vr.dataReg() != Registers::ReturnReg)
            masm.move(Registers::ReturnReg, vr.dataReg());

        saveForCall.restore();
        masm.freeStack(vp);

        if (checkInt32.isSet())
            checkInt32.get().linkTo(masm.label(), &masm);
    }

    // Performing clamping, if needed.
    if (TypedArray::getType(tarray) == js::TypedArray::TYPE_UINT8_CLAMPED)
        masm.clampInt32ToUint8(vr.dataReg());
}

// Generate code that will ensure a dynamically typed value, pinned in |vr|,
// can be stored in an integer typed array.  saveMask| is used to ensure that
// |dataReg| (and volatile registers) are preserved across any conversion
// process.
//
// Constants are left untouched. Any other value is placed into destReg.
static void
GenConversionForFloatArray(Assembler &masm, JSObject *tarray, const ValueRemat &vr,
                           FPRegisterID destReg, uint32_t saveMask)
{
    if (vr.isConstant()) {
        // Constants are always folded to doubles up-front.
        JS_ASSERT(vr.knownType() == JSVAL_TYPE_DOUBLE);
        return;
    }

    // Fast-path, if the value is a double, skip converting.
    MaybeJump isDouble;
    if (!vr.isTypeKnown())
        isDouble = masm.testDouble(Assembler::Equal, vr.typeReg());

    // If the value is an integer, inline the conversion.
    MaybeJump skip1, skip2;
    if (!vr.isTypeKnown() || vr.knownType() == JSVAL_TYPE_INT32) {
        MaybeJump isNotInt32;
        if (!vr.isTypeKnown())
            isNotInt32 = masm.testInt32(Assembler::NotEqual, vr.typeReg());
        masm.convertInt32ToDouble(vr.dataReg(), destReg);
        if (isNotInt32.isSet()) {
            skip1 = masm.jump();
            isNotInt32.get().linkTo(masm.label(), &masm);
        }
    }

    // Generate a generic conversion call, if not known to be int32_t or double.
    if (!vr.isTypeKnown() ||
        (vr.knownType() != JSVAL_TYPE_INT32 &&
         vr.knownType() != JSVAL_TYPE_DOUBLE)) {
        // Store this value, which is also an outparam.
        StackMarker vp = masm.allocStack(sizeof(Value), sizeof(double));
        masm.storeValue(vr, masm.addressOfExtra(vp));

        // Preserve volatile registers, and make the call.
        PreserveRegisters saveForCall(masm);
        saveForCall.preserve(saveMask & Registers::TempRegs);
        masm.setupABICall(Registers::FastCall, 2);
        masm.storeArg(0, masm.vmFrameOffset(offsetof(VMFrame, cx)));
        masm.storeArgAddr(1, masm.addressOfExtra(vp));
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, stubs::ConvertToTypedFloat), false);
        saveForCall.restore();

        // Load the value from the outparam, then pop the stack.
        masm.loadDouble(masm.addressOfExtra(vp), destReg);
        masm.freeStack(vp);
        skip2 = masm.jump();
    }

    if (isDouble.isSet())
        isDouble.get().linkTo(masm.label(), &masm);

    // If it's possible the value was already a double, load it directly
    // from registers (the known type is distinct from typeReg, which has
    // 32-bits of the 64-bit double).
    if (!vr.isTypeKnown() || vr.knownType() == JSVAL_TYPE_DOUBLE)
        masm.fastLoadDouble(vr.dataReg(), vr.typeReg(), destReg);

    // At this point, all loads into xmm1 are complete.
    if (skip1.isSet())
        skip1.get().linkTo(masm.label(), &masm);
    if (skip2.isSet())
        skip2.get().linkTo(masm.label(), &masm);

    if (TypedArray::getType(tarray) == js::TypedArray::TYPE_FLOAT32)
        masm.convertDoubleToFloat(destReg, destReg);
}

template <typename T>
static bool
StoreToTypedArray(JSContext *cx, Assembler &masm, JSObject *tarray, T address,
                  const ValueRemat &vrIn, uint32_t saveMask)
{
    ValueRemat vr = vrIn;

    uint32_t type = TypedArray::getType(tarray);
    switch (type) {
      case js::TypedArray::TYPE_INT8:
      case js::TypedArray::TYPE_UINT8:
      case js::TypedArray::TYPE_UINT8_CLAMPED:
      case js::TypedArray::TYPE_INT16:
      case js::TypedArray::TYPE_UINT16:
      case js::TypedArray::TYPE_INT32:
      case js::TypedArray::TYPE_UINT32:
      {
        if (!ConstantFoldForIntArray(cx, tarray, &vr))
            return false;

        PreserveRegisters saveRHS(masm);
        PreserveRegisters saveLHS(masm);

        // There are three tricky situations to handle:
        //   (1) The RHS needs conversion. saveMask will be stomped, and
        //       the RHS may need to be stomped.
        //   (2) The RHS may need to be clamped, which clobbers it.
        //   (3) The RHS may need to be in a single-byte register.
        //
        // In all of these cases, we try to find a free register that can be
        // used to mutate the RHS. Failing that, we evict an existing volatile
        // register.
        //
        // Note that we are careful to preserve the RHS before saving registers
        // for the conversion call. This is because the object and key may be
        // in temporary registers, and we want to restore those without killing
        // the mutated RHS.
        bool singleByte = (type == js::TypedArray::TYPE_INT8 ||
                           type == js::TypedArray::TYPE_UINT8 ||
                           type == js::TypedArray::TYPE_UINT8_CLAMPED);
        bool mayNeedConversion = (!vr.isTypeKnown() || vr.knownType() != JSVAL_TYPE_INT32);
        bool mayNeedClamping = !vr.isConstant() && (type == js::TypedArray::TYPE_UINT8_CLAMPED);
        bool needsSingleByteReg = singleByte &&
                                  !vr.isConstant() &&
                                  !(Registers::SingleByteRegs & Registers::maskReg(vr.dataReg()));
        bool rhsIsMutable = !vr.isConstant() && !(saveMask & Registers::maskReg(vr.dataReg()));

        if (((mayNeedConversion || mayNeedClamping) && !rhsIsMutable) || needsSingleByteReg) {
            // First attempt to find a free temporary register that:
            //   - is compatible with the RHS constraints
            //   - won't clobber the key, object, or RHS type regs
            //   - is temporary, but
            //   - is not in saveMask, which contains live volatile registers.
            uint32_t allowMask = Registers::AvailRegs;
            if (singleByte)
                allowMask &= Registers::SingleByteRegs;

            // Create a mask of registers we absolutely cannot clobber.
            uint32_t pinned = Assembler::maskAddress(address);
            if (!vr.isTypeKnown())
                pinned |= Registers::maskReg(vr.typeReg());

            Registers avail = allowMask & ~(pinned | saveMask);

            RegisterID newReg;
            if (!avail.empty()) {
                newReg = avail.takeAnyReg().reg();
            } else {
                // If no registers meet the ideal set, relax a constraint and spill.
                avail = allowMask & ~pinned;

                if (!avail.empty()) {
                    newReg = avail.takeAnyReg().reg();
                    saveRHS.preserve(Registers::maskReg(newReg));
                } else {
                    // Oh no! *All* single byte registers are pinned. This
                    // sucks. We'll swap the type and data registers in |vr|
                    // and unswap them later.

                    // If |vr|'s registers are part of the address, swapping is
                    // going to cause problems during the store.
                    uint32_t vrRegs = Registers::mask2Regs(vr.dataReg(), vr.typeReg());
                    uint32_t lhsMask = vrRegs & Assembler::maskAddress(address);

                    // We'll also need to save any of the registers which won't
                    // be restored via |lhsMask| above.
                    uint32_t rhsMask = vrRegs & ~lhsMask;

                    // Push them, but get the order right. We'll pop LHS first.
                    saveRHS.preserve(rhsMask);
                    saveLHS.preserve(lhsMask);

                    // Don't store/restore registers if we dont have to.
                    saveMask &= ~lhsMask;

                    // Actually perform the swap.
                    masm.swap(vr.typeReg(), vr.dataReg());
                    vr = ValueRemat::FromRegisters(vr.dataReg(), vr.typeReg());
                    newReg = vr.dataReg();
                }

                // Now, make sure the new register is not in the saveMask,
                // so it won't get restored right after the call.
                saveMask &= ~Registers::maskReg(newReg);
            }

            if (vr.dataReg() != newReg)
                masm.move(vr.dataReg(), newReg);

            // Update |vr|.
            if (vr.isTypeKnown())
                vr = ValueRemat::FromKnownType(vr.knownType(), newReg);
            else
                vr = ValueRemat::FromRegisters(vr.typeReg(), newReg);
        }

        GenConversionForIntArray(masm, tarray, vr, saveMask);

        // Restore the registers in |address|. |GenConversionForIntArray| won't
        // restore them because we told it not to by fiddling with |saveMask|.
        saveLHS.restore();

        if (vr.isConstant())
            masm.storeToTypedIntArray(type, Imm32(vr.value().toInt32()), address);
        else
            masm.storeToTypedIntArray(type, vr.dataReg(), address);

        // Note that this will finish restoring the damage from the
        // earlier register swap.
        saveRHS.restore();
        break;
      }

      case js::TypedArray::TYPE_FLOAT32:
      case js::TypedArray::TYPE_FLOAT64: {
        /*
         * Use a temporary for conversion. Inference is disabled, so no FP
         * registers are live.
         */
        Registers regs(Registers::TempFPRegs);
        FPRegisterID temp = regs.takeAnyReg().fpreg();

        if (!ConstantFoldForFloatArray(cx, &vr))
            return false;
        GenConversionForFloatArray(masm, tarray, vr, temp, saveMask);
        if (vr.isConstant())
            masm.storeToTypedFloatArray(type, ImmDouble(vr.value().toDouble()), address);
        else
            masm.storeToTypedFloatArray(type, temp, address);
        break;
      }
    }

    return true;
}

#endif /* JS_METHODJIT_TYPED_ARRAY */

} /* namespace mjit */
} /* namespace js */

#endif /* js_typedarray_ic_h___ */


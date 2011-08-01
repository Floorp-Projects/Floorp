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

#ifndef js_typedarray_ic_h___
#define js_typedarray_ic_h___

#include "jscntxt.h"
#include "jstypedarray.h"

#include "jsnuminlines.h"
#include "jstypedarrayinlines.h"

namespace js {
namespace mjit {

#ifdef JS_POLYIC_TYPED_ARRAY

typedef JSC::MacroAssembler::RegisterID RegisterID;
typedef JSC::MacroAssembler::FPRegisterID FPRegisterID;
typedef JSC::MacroAssembler::Jump Jump;
typedef JSC::MacroAssembler::Imm32 Imm32;
typedef JSC::MacroAssembler::ImmDouble ImmDouble;

template <typename T>
static void
LoadFromTypedArray(Assembler &masm, JSObject *tarray, T address,
                   RegisterID typeReg, RegisterID dataReg)
{
    switch (TypedArray::getType(tarray)) {
      case js::TypedArray::TYPE_INT8:
        masm.load8SignExtend(address, dataReg);
        masm.move(ImmType(JSVAL_TYPE_INT32), typeReg);
        break;
      case js::TypedArray::TYPE_UINT8:
      case js::TypedArray::TYPE_UINT8_CLAMPED:
        masm.load8ZeroExtend(address, dataReg);
        masm.move(ImmType(JSVAL_TYPE_INT32), typeReg);
        break;
      case js::TypedArray::TYPE_INT16:
        masm.load16SignExtend(address, dataReg);
        masm.move(ImmType(JSVAL_TYPE_INT32), typeReg);
        break;
      case js::TypedArray::TYPE_UINT16:
        masm.load16(address, dataReg);
        masm.move(ImmType(JSVAL_TYPE_INT32), typeReg);
        break;
      case js::TypedArray::TYPE_INT32:
        masm.load32(address, dataReg);
        masm.move(ImmType(JSVAL_TYPE_INT32), typeReg);
        break;
      case js::TypedArray::TYPE_UINT32:
      {
        masm.load32(address, dataReg);
        masm.move(ImmType(JSVAL_TYPE_INT32), typeReg);
        Jump safeInt = masm.branch32(Assembler::Below, dataReg, Imm32(0x80000000));
        masm.convertUInt32ToDouble(dataReg, FPRegisters::First);
        masm.breakDouble(FPRegisters::First, typeReg, dataReg);
        safeInt.linkTo(masm.label(), &masm);
        break;
      }
      case js::TypedArray::TYPE_FLOAT32:
      case js::TypedArray::TYPE_FLOAT64:
      {
        if (TypedArray::getType(tarray) == js::TypedArray::TYPE_FLOAT32)
            masm.loadFloat(address, FPRegisters::First);
        else
            masm.loadDouble(address, FPRegisters::First);
        // Make sure NaN gets canonicalized.
        Jump notNaN = masm.branchDouble(Assembler::DoubleEqual,
                                        FPRegisters::First,
                                        FPRegisters::First);
        masm.loadStaticDouble(&js_NaN, FPRegisters::First, dataReg);
        notNaN.linkTo(masm.label(), &masm);
        masm.breakDouble(FPRegisters::First, typeReg, dataReg);
        break;
      }
    }
}

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

    jsdouble d = 0;
    Value v = vr->value();
    if (v.isString()) {
        if (!StringToNumberType<jsdouble>(cx, v.toString(), &d))
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

static inline int32
ClampIntForUint8Array(int32 x)
{
    if (x < 0)
        return 0;
    if (x > 255)
        return 255;
    return x;
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

    int32 i32 = 0;
    if (v.isDouble()) {
        i32 = (TypedArray::getType(tarray) == js::TypedArray::TYPE_UINT8_CLAMPED)
              ? js_TypedArray_uint8_clamp_double(v.toDouble())
              : js_DoubleToECMAInt32(v.toDouble());
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

template <typename S, typename T>
static void
StoreToIntArray(Assembler &masm, JSObject *tarray, S src, T address)
{
    switch (TypedArray::getType(tarray)) {
      case js::TypedArray::TYPE_INT8:
      case js::TypedArray::TYPE_UINT8:
      case js::TypedArray::TYPE_UINT8_CLAMPED:
        masm.store8(src, address);
        break;
      case js::TypedArray::TYPE_INT16:
      case js::TypedArray::TYPE_UINT16:
        masm.store16(src, address);
        break;
      case js::TypedArray::TYPE_INT32:
      case js::TypedArray::TYPE_UINT32:
        masm.store32(src, address);
        break;
      default:
        JS_NOT_REACHED("unknown int array type");
    }
}

template <typename S, typename T>
static void
StoreToFloatArray(Assembler &masm, JSObject *tarray, S src, T address)
{
    if (TypedArray::getType(tarray) == js::TypedArray::TYPE_FLOAT32)
        masm.storeFloat(src, address);
    else
        masm.storeDouble(src, address);
}

// Generate code that will ensure a dynamically typed value, pinned in |vr|,
// can be stored in an integer typed array. If any sort of conversion is
// required, |dataReg| will be clobbered by a new value. |saveMask| is
// used to ensure that |dataReg| (and volatile registers) are preserved
// across any conversion process.
static void
GenConversionForIntArray(Assembler &masm, JSObject *tarray, const ValueRemat &vr,
                         uint32 saveMask)
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

        typedef int32 (JS_FASTCALL *Int32CxVp)(JSContext *, Value *);
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
    if (TypedArray::getType(tarray) == js::TypedArray::TYPE_UINT8_CLAMPED) {
        //     cmp dr, 0
        //     jge _min
        //     mov dr, 0
        //     jump _done
        // _min:
        //     cmp dr, 255
        //     jle _done
        //     mov dr, 255
        // _done:
        //
        Jump j = masm.branch32(Assembler::GreaterThanOrEqual, vr.dataReg(), Imm32(0));
        masm.move(Imm32(0), vr.dataReg());
        Jump done = masm.jump();
        j.linkTo(masm.label(), &masm);
        j = masm.branch32(Assembler::LessThanOrEqual, vr.dataReg(), Imm32(255));
        masm.move(Imm32(255), vr.dataReg());
        j.linkTo(masm.label(), &masm);
        done.linkTo(masm.label(), &masm);
    }
}

// Generate code that will ensure a dynamically typed value, pinned in |vr|,
// can be stored in an integer typed array.  saveMask| is used to ensure that
// |dataReg| (and volatile registers) are preserved across any conversion
// process.
//
// Constants are left untouched. Any other value is placed into
// FPRegisters::First.
static void
GenConversionForFloatArray(Assembler &masm, JSObject *tarray, const ValueRemat &vr,
                           FPRegisterID destReg, uint32 saveMask)
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

    // Generate a generic conversion call, if not known to be int32 or double.
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
                  const ValueRemat &vrIn, uint32 saveMask)
{
    ValueRemat vr = vrIn;

    switch (TypedArray::getType(tarray)) {
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
        bool singleByte = (TypedArray::getType(tarray) == js::TypedArray::TYPE_INT8 ||
                           TypedArray::getType(tarray) == js::TypedArray::TYPE_UINT8 ||
                           TypedArray::getType(tarray) == js::TypedArray::TYPE_UINT8_CLAMPED);
        bool mayNeedConversion = (!vr.isTypeKnown() || vr.knownType() != JSVAL_TYPE_INT32);
        bool mayNeedClamping = !vr.isConstant() && (TypedArray::getType(tarray) == js::TypedArray::TYPE_UINT8_CLAMPED);
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
            uint32 allowMask = Registers::AvailRegs;
            if (singleByte)
                allowMask &= Registers::SingleByteRegs;

            // Create a mask of registers we absolutely cannot clobber.
            uint32 pinned = Assembler::maskAddress(address);
            if (!vr.isTypeKnown())
                pinned |= Registers::maskReg(vr.typeReg());

            Registers avail = allowMask & ~(pinned | saveMask);

            RegisterID newReg;
            if (!avail.empty()) {
                newReg = avail.takeAnyReg();
            } else {
                // If no registers meet the ideal set, relax a constraint and spill.
                avail = allowMask & ~pinned;

                if (!avail.empty()) {
                    newReg = avail.takeAnyReg();
                    saveRHS.preserve(Registers::maskReg(newReg));
                } else {
                    // Oh no! *All* single byte registers are pinned. This
                    // sucks. We'll swap the type and data registers in |vr|
                    // and unswap them later.

                    // If |vr|'s registers are part of the address, swapping is
                    // going to cause problems during the store.
                    uint32 vrRegs = Registers::mask2Regs(vr.dataReg(), vr.typeReg());
                    uint32 lhsMask = vrRegs & Assembler::maskAddress(address);

                    // We'll also need to save any of the registers which won't
                    // be restored via |lhsMask| above.
                    uint32 rhsMask = vrRegs & ~lhsMask;

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
            StoreToIntArray(masm, tarray, Imm32(vr.value().toInt32()), address);
        else
            StoreToIntArray(masm, tarray, vr.dataReg(), address);

        // Note that this will finish restoring the damage from the
        // earlier register swap.
        saveRHS.restore();
        break;
      }

      case js::TypedArray::TYPE_FLOAT32:
      case js::TypedArray::TYPE_FLOAT64:
        if (!ConstantFoldForFloatArray(cx, &vr))
            return false;
        GenConversionForFloatArray(masm, tarray, vr, FPRegisters::First, saveMask);
        if (vr.isConstant())
            StoreToFloatArray(masm, tarray, ImmDouble(vr.value().toDouble()), address);
        else
            StoreToFloatArray(masm, tarray, FPRegisters::First, address);
        break;
    }

    return true;
}

#endif // defined(JS_POLYIC) && (defined JS_CPU_X86 || defined JS_CPU_X64)

} /* namespace mjit */
} /* namespace js */

#endif /* js_typedarray_ic_h___ */


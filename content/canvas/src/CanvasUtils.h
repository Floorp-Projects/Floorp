/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CANVASUTILS_H_
#define _CANVASUTILS_H_

#include "mozilla/CheckedInt.h"

class nsIPrincipal;

namespace mozilla {

namespace gfx {
class Matrix;
}

namespace dom {
class HTMLCanvasElement;
}

namespace CanvasUtils {


// Check that the rectangle [x,y,w,h] is a subrectangle of [0,0,realWidth,realHeight]

inline bool CheckSaneSubrectSize(int32_t x, int32_t y, int32_t w, int32_t h,
                            int32_t realWidth, int32_t realHeight) {
    CheckedInt32 checked_xmost  = CheckedInt32(x) + w;
    CheckedInt32 checked_ymost  = CheckedInt32(y) + h;

    return w >= 0 && h >= 0 && x >= 0 && y >= 0 &&
        checked_xmost.isValid() &&
        checked_xmost.value() <= realWidth &&
        checked_ymost.isValid() &&
        checked_ymost.value() <= realHeight;
}

// Flag aCanvasElement as write-only if drawing an image with aPrincipal
// onto it would make it such.

void DoDrawImageSecurityCheck(dom::HTMLCanvasElement *aCanvasElement,
                              nsIPrincipal *aPrincipal,
                              bool forceWriteOnly,
                              bool CORSUsed);

// Make a double out of |v|, treating undefined values as 0.0 (for
// the sake of sparse arrays).  Return true iff coercion
// succeeded.
bool CoerceDouble(JS::Value v, double* d);

    /* Float validation stuff */
#define VALIDATE(_f)  if (!NS_finite(_f)) return false

inline bool FloatValidate (double f1) {
    VALIDATE(f1);
    return true;
}

inline bool FloatValidate (double f1, double f2) {
    VALIDATE(f1); VALIDATE(f2);
    return true;
}

inline bool FloatValidate (double f1, double f2, double f3) {
    VALIDATE(f1); VALIDATE(f2); VALIDATE(f3);
    return true;
}

inline bool FloatValidate (double f1, double f2, double f3, double f4) {
    VALIDATE(f1); VALIDATE(f2); VALIDATE(f3); VALIDATE(f4);
    return true;
}

inline bool FloatValidate (double f1, double f2, double f3, double f4, double f5) {
    VALIDATE(f1); VALIDATE(f2); VALIDATE(f3); VALIDATE(f4); VALIDATE(f5);
    return true;
}

inline bool FloatValidate (double f1, double f2, double f3, double f4, double f5, double f6) {
    VALIDATE(f1); VALIDATE(f2); VALIDATE(f3); VALIDATE(f4); VALIDATE(f5); VALIDATE(f6);
    return true;
}

#undef VALIDATE

template<typename T>
nsresult
JSValToDashArray(JSContext* cx, const JS::Value& val,
                 FallibleTArray<T>& dashArray);

template<typename T>
JS::Value
DashArrayToJSVal(FallibleTArray<T>& dashArray,
                 JSContext* cx, mozilla::ErrorResult& rv);

template<typename T>
nsresult
JSValToDashArray(JSContext* cx, const JS::Value& patternArray,
                 FallibleTArray<T>& dashes)
{
    // The cap is pretty arbitrary.  16k should be enough for
    // anybody...
    static const uint32_t MAX_NUM_DASHES = 1 << 14;

    if (!JSVAL_IS_PRIMITIVE(patternArray)) {
        JS::Rooted<JSObject*> obj(cx, JSVAL_TO_OBJECT(patternArray));
        uint32_t length;
        if (!JS_GetArrayLength(cx, obj, &length)) {
            // Not an array-like thing
            return NS_ERROR_INVALID_ARG;
        } else if (length > MAX_NUM_DASHES) {
            // Too many dashes in the pattern
            return NS_ERROR_ILLEGAL_VALUE;
        }

        bool haveNonzeroElement = false;
        for (uint32_t i = 0; i < length; ++i) {
            JS::Rooted<JS::Value> elt(cx);
            double d;
            if (!JS_GetElement(cx, obj, i, elt.address())) {
                return NS_ERROR_FAILURE;
            }
            if (!(CoerceDouble(elt, &d) &&
                  FloatValidate(d) &&
                  d >= 0.0)) {
                // Pattern elements must be finite "numbers" >= 0.
                return NS_ERROR_INVALID_ARG;
            } else if (d > 0.0) {
                haveNonzeroElement = true;
            }
            if (!dashes.AppendElement(d)) {
                return NS_ERROR_OUT_OF_MEMORY;
            }
        }

        if (dashes.Length() > 0 && !haveNonzeroElement) {
            // An all-zero pattern makes no sense.
            return NS_ERROR_ILLEGAL_VALUE;
        }
    } else if (!(JSVAL_IS_VOID(patternArray) || JSVAL_IS_NULL(patternArray))) {
        // undefined and null mean "reset to no dash".  Any other
        // random garbage is a type error.
        return NS_ERROR_INVALID_ARG;
    }

    return NS_OK;
}

template<typename T>
JS::Value
DashArrayToJSVal(FallibleTArray<T>& dashes,
                 JSContext* cx, mozilla::ErrorResult& rv)
{
    if (dashes.IsEmpty()) {
        return JSVAL_NULL;
    }
    JS::Rooted<JSObject*> obj(cx,
        JS_NewArrayObject(cx, dashes.Length(), nullptr));
    if (!obj) {
        rv.Throw(NS_ERROR_OUT_OF_MEMORY);
        return JSVAL_NULL;
    }
    for (uint32_t i = 0; i < dashes.Length(); ++i) {
        double d = dashes[i];
        JS::Value elt = DOUBLE_TO_JSVAL(d);
        if (!JS_DefineElement(cx, obj, i, elt, nullptr, nullptr, 0)) {
            rv.Throw(NS_ERROR_FAILURE);
            return JSVAL_NULL;
        }
    }
    return OBJECT_TO_JSVAL(obj);
}

}
}

#endif /* _CANVASUTILS_H_ */

/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CANVASUTILS_H_
#define _CANVASUTILS_H_

#include "prtypes.h"

#include "mozilla/CheckedInt.h"

class nsHTMLCanvasElement;
class nsIPrincipal;

namespace mozilla {

namespace gfx {
class Matrix;
}

namespace CanvasUtils {

using namespace gfx;

// Check that the rectangle [x,y,w,h] is a subrectangle of [0,0,realWidth,realHeight]

inline bool CheckSaneSubrectSize(PRInt32 x, PRInt32 y, PRInt32 w, PRInt32 h,
                            PRInt32 realWidth, PRInt32 realHeight) {
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

void DoDrawImageSecurityCheck(nsHTMLCanvasElement *aCanvasElement,
                              nsIPrincipal *aPrincipal,
                              bool forceWriteOnly,
                              bool CORSUsed);

// Make a double out of |v|, treating undefined values as 0.0 (for
// the sake of sparse arrays).  Return true iff coercion
// succeeded.
bool CoerceDouble(jsval v, double* d);

// Return true iff the conversion succeeded, false otherwise.  *rv is
// the value to return to script if this returns false.
bool JSValToMatrix(JSContext* cx, const jsval& val,
                   gfxMatrix* matrix, nsresult* rv);
bool JSValToMatrix(JSContext* cx, const jsval& val,
                   Matrix* matrix, nsresult* rv);

nsresult MatrixToJSVal(const gfxMatrix& matrix,
                       JSContext* cx, jsval* val);
nsresult MatrixToJSVal(const Matrix& matrix,
                       JSContext* cx, jsval* val);

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
JSValToDashArray(JSContext* cx, const jsval& val,
                 FallibleTArray<T>& dashArray);

template<typename T>
nsresult
DashArrayToJSVal(FallibleTArray<T>& dashArray,
                 JSContext* cx, jsval* val);

template<typename T>
nsresult
JSValToDashArray(JSContext* cx, const jsval& patternArray,
                 FallibleTArray<T>& dashes)
{
    // The cap is pretty arbitrary.  16k should be enough for
    // anybody...
    static const uint32_t MAX_NUM_DASHES = 1 << 14;

    if (!JSVAL_IS_PRIMITIVE(patternArray)) {
        JSObject* obj = JSVAL_TO_OBJECT(patternArray);
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
            jsval elt;
            double d;
            if (!JS_GetElement(cx, obj, i, &elt)) {
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
nsresult
DashArrayToJSVal(FallibleTArray<T>& dashes,
                 JSContext* cx, jsval* val)
{
    if (dashes.IsEmpty()) {
        *val = JSVAL_NULL;
    } else {
        JSObject* obj = JS_NewArrayObject(cx, dashes.Length(), nsnull);
        if (!obj) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
        for (PRUint32 i = 0; i < dashes.Length(); ++i) {
            double d = dashes[i];
            jsval elt = DOUBLE_TO_JSVAL(d);
            if (!JS_SetElement(cx, obj, i, &elt)) {
                return NS_ERROR_FAILURE;
            }
        }
        *val = OBJECT_TO_JSVAL(obj);
    }
    return NS_OK;
}

}
}

#endif /* _CANVASUTILS_H_ */

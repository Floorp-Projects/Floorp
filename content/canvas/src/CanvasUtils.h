/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Vladimir Vukicevic <vladimir@pobox.com>
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef _CANVASUTILS_H_
#define _CANVASUTILS_H_

#include "prtypes.h"

#include "CheckedInt.h"

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
        checked_xmost.valid() &&
        checked_xmost.value() <= realWidth &&
        checked_ymost.valid() &&
        checked_ymost.value() <= realHeight;
}

// Flag aCanvasElement as write-only if drawing an image with aPrincipal
// onto it would make it such.

void DoDrawImageSecurityCheck(nsHTMLCanvasElement *aCanvasElement,
                              nsIPrincipal *aPrincipal,
                              bool forceWriteOnly,
                              bool CORSUsed);

void LogMessage (const nsCString& errorString);
void LogMessagef (const char *fmt, ...);

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
#define VALIDATE(_f)  if (!NS_finite(_f)) return PR_FALSE

inline bool FloatValidate (double f1) {
    VALIDATE(f1);
    return PR_TRUE;
}

inline bool FloatValidate (double f1, double f2) {
    VALIDATE(f1); VALIDATE(f2);
    return PR_TRUE;
}

inline bool FloatValidate (double f1, double f2, double f3) {
    VALIDATE(f1); VALIDATE(f2); VALIDATE(f3);
    return PR_TRUE;
}

inline bool FloatValidate (double f1, double f2, double f3, double f4) {
    VALIDATE(f1); VALIDATE(f2); VALIDATE(f3); VALIDATE(f4);
    return PR_TRUE;
}

inline bool FloatValidate (double f1, double f2, double f3, double f4, double f5) {
    VALIDATE(f1); VALIDATE(f2); VALIDATE(f3); VALIDATE(f4); VALIDATE(f5);
    return PR_TRUE;
}

inline bool FloatValidate (double f1, double f2, double f3, double f4, double f5, double f6) {
    VALIDATE(f1); VALIDATE(f2); VALIDATE(f3); VALIDATE(f4); VALIDATE(f5); VALIDATE(f6);
    return PR_TRUE;
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
    static const jsuint MAX_NUM_DASHES = 1 << 14;

    if (!JSVAL_IS_PRIMITIVE(patternArray)) {
        JSObject* obj = JSVAL_TO_OBJECT(patternArray);
        jsuint length;
        if (!JS_GetArrayLength(cx, obj, &length)) {
            // Not an array-like thing
            return NS_ERROR_INVALID_ARG;
        } else if (length > MAX_NUM_DASHES) {
            // Too many dashes in the pattern
            return NS_ERROR_ILLEGAL_VALUE;
        }

        bool haveNonzeroElement = false;
        for (uint32 i = 0; i < length; ++i) {
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

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

#include <stdlib.h>
#include <stdarg.h>

#include "prmem.h"
#include "prprf.h"

#include "nsIServiceManager.h"

#include "nsIConsoleService.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIDOMCanvasRenderingContext2D.h"
#include "nsICanvasRenderingContextInternal.h"
#include "nsHTMLCanvasElement.h"
#include "nsIPrincipal.h"
#include "nsINode.h"

#include "nsGfxCIID.h"

#include "nsTArray.h"

#include "CanvasUtils.h"
#include "mozilla/gfx/Matrix.h"

namespace mozilla {
namespace CanvasUtils {

void
DoDrawImageSecurityCheck(nsHTMLCanvasElement *aCanvasElement,
                         nsIPrincipal *aPrincipal,
                         bool forceWriteOnly,
                         bool CORSUsed)
{
    NS_PRECONDITION(aPrincipal, "Must have a principal here");

    // Callers should ensure that mCanvasElement is non-null before calling this
    if (!aCanvasElement) {
        NS_WARNING("DoDrawImageSecurityCheck called without canvas element!");
        return;
    }

    if (aCanvasElement->IsWriteOnly())
        return;

    // If we explicitly set WriteOnly just do it and get out
    if (forceWriteOnly) {
        aCanvasElement->SetWriteOnly();
        return;
    }

    // No need to do a security check if the image used CORS for the load
    if (CORSUsed)
        return;

    bool subsumes;
    nsresult rv =
        aCanvasElement->NodePrincipal()->Subsumes(aPrincipal, &subsumes);

    if (NS_SUCCEEDED(rv) && subsumes) {
        // This canvas has access to that image anyway
        return;
    }

    aCanvasElement->SetWriteOnly();
}

void
LogMessage (const nsCString& errorString)
{
    nsCOMPtr<nsIConsoleService> console(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
    if (!console)
        return;

    console->LogStringMessage(NS_ConvertUTF8toUTF16(errorString).get());
    fprintf(stderr, "%s\n", errorString.get());
}

void
LogMessagef (const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char buf[256];

    nsCOMPtr<nsIConsoleService> console(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
    if (console) {
        PR_vsnprintf(buf, 256, fmt, ap);
        console->LogStringMessage(NS_ConvertUTF8toUTF16(nsDependentCString(buf)).get());
        fprintf(stderr, "%s\n", buf);
    }

    va_end(ap);
}

bool
CoerceDouble(jsval v, double* d)
{
    if (JSVAL_IS_DOUBLE(v)) {
        *d = JSVAL_TO_DOUBLE(v);
    } else if (JSVAL_IS_INT(v)) {
        *d = double(JSVAL_TO_INT(v));
    } else if (JSVAL_IS_VOID(v)) {
        *d = 0.0;
    } else {
        return false;
    }
    return true;
}

template<size_t N>
static bool
JSValToMatrixElts(JSContext* cx, const jsval& val,
                  double* (&elts)[N], nsresult* rv)
{
    JSObject* obj;
    jsuint length;

    if (JSVAL_IS_PRIMITIVE(val) ||
        !(obj = JSVAL_TO_OBJECT(val)) ||
        !JS_GetArrayLength(cx, obj, &length) ||
        N != length) {
        // Not an array-like thing or wrong size
        *rv = NS_ERROR_INVALID_ARG;
        return false;
    }

    for (PRUint32 i = 0; i < N; ++i) {
        jsval elt;
        double d;
        if (!JS_GetElement(cx, obj, i, &elt)) {
            *rv = NS_ERROR_FAILURE;
            return false;
        }
        if (!CoerceDouble(elt, &d)) {
            *rv = NS_ERROR_INVALID_ARG;
            return false;
        }
        if (!FloatValidate(d)) {
            // This is weird, but it's the behavior of SetTransform()
            *rv = NS_OK;
            return false;
        }
        *elts[i] = d;
    }

    *rv = NS_OK;
    return true;
}

bool
JSValToMatrix(JSContext* cx, const jsval& val, gfxMatrix* matrix, nsresult* rv)
{
    double* elts[] = { &matrix->xx, &matrix->yx, &matrix->xy, &matrix->yy,
                       &matrix->x0, &matrix->y0 };
    return JSValToMatrixElts(cx, val, elts, rv);
}

bool
JSValToMatrix(JSContext* cx, const jsval& val, Matrix* matrix, nsresult* rv)
{
    gfxMatrix m;
    if (!JSValToMatrix(cx, val, &m, rv))
        return false;
    *matrix = Matrix(Float(m.xx), Float(m.yx), Float(m.xy), Float(m.yy),
                     Float(m.x0), Float(m.y0));
    return true;
}

template<size_t N>
static nsresult
MatrixEltsToJSVal(/*const*/ jsval (&elts)[N], JSContext* cx, jsval* val)
{
    JSObject* obj = JS_NewArrayObject(cx, N, elts);
    if  (!obj) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    *val = OBJECT_TO_JSVAL(obj);

    return NS_OK;
}

nsresult
MatrixToJSVal(const gfxMatrix& matrix, JSContext* cx, jsval* val)
{
    jsval elts[] = {
        DOUBLE_TO_JSVAL(matrix.xx), DOUBLE_TO_JSVAL(matrix.yx),
        DOUBLE_TO_JSVAL(matrix.xy), DOUBLE_TO_JSVAL(matrix.yy),
        DOUBLE_TO_JSVAL(matrix.x0), DOUBLE_TO_JSVAL(matrix.y0)
    };
    return MatrixEltsToJSVal(elts, cx, val);
}

nsresult
MatrixToJSVal(const Matrix& matrix, JSContext* cx, jsval* val)
{
    jsval elts[] = {
        DOUBLE_TO_JSVAL(matrix._11), DOUBLE_TO_JSVAL(matrix._12),
        DOUBLE_TO_JSVAL(matrix._21), DOUBLE_TO_JSVAL(matrix._22),
        DOUBLE_TO_JSVAL(matrix._31), DOUBLE_TO_JSVAL(matrix._32)
    };
    return MatrixEltsToJSVal(elts, cx, val);
}

} // namespace CanvasUtils
} // namespace mozilla

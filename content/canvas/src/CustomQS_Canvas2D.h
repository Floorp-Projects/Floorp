/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Gecko code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com> (original author)
 *   Ms2ger <ms2ger@gmail.com>
 *   Yury <async.processingjs@yahoo.com>
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

#include "nsDOMError.h"
#include "nsIDOMCanvasRenderingContext2D.h"
#include "CheckedInt.h"
#include "nsMathUtils.h"

typedef NS_STDCALL_FUNCPROTO(nsresult, CanvasStyleSetterType, nsIDOMCanvasRenderingContext2D,
                             SetStrokeStyle_multi, (const nsAString &, nsISupports *));
typedef NS_STDCALL_FUNCPROTO(nsresult, CanvasStyleGetterType, nsIDOMCanvasRenderingContext2D,
                             GetStrokeStyle_multi, (nsAString &, nsISupports **, PRInt32 *));

static JSBool
Canvas2D_SetStyleHelper(JSContext *cx, JSObject *obj, jsid id, jsval *vp,
                        CanvasStyleSetterType setfunc)
{
    XPC_QS_ASSERT_CONTEXT_OK(cx);
    nsIDOMCanvasRenderingContext2D *self;
    xpc_qsSelfRef selfref;
    js::AutoValueRooter tvr(cx);
    if (!xpc_qsUnwrapThis(cx, obj, nsnull, &self, &selfref.ptr, tvr.jsval_addr(), nsnull))
        return JS_FALSE;

    nsresult rv = NS_OK;
    if (JSVAL_IS_STRING(*vp)) {
        xpc_qsDOMString arg0(cx, *vp, vp,
                             xpc_qsDOMString::eDefaultNullBehavior,
                             xpc_qsDOMString::eDefaultUndefinedBehavior);
        if (!arg0.IsValid())
            return JS_FALSE;

        rv = (self->*setfunc)(arg0, nsnull);
    } else {
        nsISupports *arg0;
        xpc_qsSelfRef arg0ref;
        rv = xpc_qsUnwrapArg<nsISupports>(cx, *vp, &arg0, &arg0ref.ptr, vp);
        if (NS_FAILED(rv)) {
            xpc_qsThrowBadSetterValue(cx, rv, JSVAL_TO_OBJECT(*tvr.jsval_addr()), id);
            return JS_FALSE;
        }

        nsString voidStr;
        voidStr.SetIsVoid(PR_TRUE);

        rv = (self->*setfunc)(voidStr, arg0);
    }

    if (NS_FAILED(rv))
        return xpc_qsThrowGetterSetterFailed(cx, rv, JSVAL_TO_OBJECT(*tvr.jsval_addr()), id);

    return JS_TRUE;
}

static JSBool
Canvas2D_GetStyleHelper(JSContext *cx, JSObject *obj, jsid id, jsval *vp,
                        CanvasStyleGetterType getfunc)
{
    XPC_QS_ASSERT_CONTEXT_OK(cx);
    nsIDOMCanvasRenderingContext2D *self;
    xpc_qsSelfRef selfref;
    XPCLazyCallContext lccx(JS_CALLER, cx, obj);
    if (!xpc_qsUnwrapThis(cx, obj, nsnull, &self, &selfref.ptr, vp, &lccx))
        return JS_FALSE;
    nsresult rv;

    nsString resultString;
    nsCOMPtr<nsISupports> resultInterface;
    PRInt32 resultType;
    rv = (self->*getfunc)(resultString, getter_AddRefs(resultInterface), &resultType);
    if (NS_FAILED(rv))
        return xpc_qsThrowGetterSetterFailed(cx, rv, JSVAL_TO_OBJECT(*vp), id);

    switch (resultType) {
    case nsIDOMCanvasRenderingContext2D::CMG_STYLE_STRING:
        return xpc_qsStringToJsval(cx, resultString, vp);

    case nsIDOMCanvasRenderingContext2D::CMG_STYLE_PATTERN:
    {
        qsObjectHelper helper(resultInterface,
                              xpc_qsGetWrapperCache(resultInterface));
        return xpc_qsXPCOMObjectToJsval(lccx, helper,
                                        &NS_GET_IID(nsIDOMCanvasPattern),
                                        &interfaces[k_nsIDOMCanvasPattern], vp);
    }
    case nsIDOMCanvasRenderingContext2D::CMG_STYLE_GRADIENT:
    {
        qsObjectHelper helper(resultInterface,
                              xpc_qsGetWrapperCache(resultInterface));
        return xpc_qsXPCOMObjectToJsval(lccx, helper,
                                        &NS_GET_IID(nsIDOMCanvasGradient),
                                        &interfaces[k_nsIDOMCanvasGradient], vp);
    }
    default:
        return xpc_qsThrowGetterSetterFailed(cx, NS_ERROR_FAILURE, JSVAL_TO_OBJECT(*vp), id);
    }
}

static JSBool
nsIDOMCanvasRenderingContext2D_SetStrokeStyle(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
    return Canvas2D_SetStyleHelper(cx, obj, id, vp, &nsIDOMCanvasRenderingContext2D::SetStrokeStyle_multi);
}

static JSBool
nsIDOMCanvasRenderingContext2D_GetStrokeStyle(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    return Canvas2D_GetStyleHelper(cx, obj, id, vp, &nsIDOMCanvasRenderingContext2D::GetStrokeStyle_multi);
}

static JSBool
nsIDOMCanvasRenderingContext2D_SetFillStyle(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
    return Canvas2D_SetStyleHelper(cx, obj, id, vp, &nsIDOMCanvasRenderingContext2D::SetFillStyle_multi);
}

static JSBool
nsIDOMCanvasRenderingContext2D_GetFillStyle(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    return Canvas2D_GetStyleHelper(cx, obj, id, vp, &nsIDOMCanvasRenderingContext2D::GetFillStyle_multi);
}

static bool
CreateImageData(JSContext* cx,
                uint32 w,
                uint32 h,
                nsIDOMCanvasRenderingContext2D* self,
                int32 x,
                int32 y,
                jsval* vp)
{
    using mozilla::CheckedInt;

    if (w == 0)
        w = 1;
    if (h == 0)
        h = 1;

    CheckedInt<uint32> len = CheckedInt<uint32>(w) * h * 4;
    if (!len.valid()) {
        return xpc_qsThrow(cx, NS_ERROR_DOM_INDEX_SIZE_ERR);
    }

    // Create the fast typed array; it's initialized to 0 by default.
    JSObject* darray =
      js_CreateTypedArray(cx, js::TypedArray::TYPE_UINT8_CLAMPED, len.value());
    js::AutoObjectRooter rd(cx, darray);
    if (!darray) {
        return false;
    }

    if (self) {
        JSObject *tdest = js::TypedArray::getTypedArray(darray);

        // make the call
        nsresult rv =
            self->GetImageData_explicit(x, y, w, h,
                                        static_cast<PRUint8*>(JS_GetTypedArrayData(tdest)),
                                        JS_GetTypedArrayByteLength(tdest));
        if (NS_FAILED(rv)) {
            return xpc_qsThrowMethodFailed(cx, rv, vp);
        }
    }

    // Do JS_NewObject after CreateTypedArray, so that gc will get
    // triggered here if necessary
    JSObject* result = JS_NewObject(cx, NULL, NULL, NULL);
    js::AutoObjectRooter rr(cx, result);
    if (!result) {
        return false;
    }

    if (!JS_DefineProperty(cx, result, "width", INT_TO_JSVAL(w), NULL, NULL,
                           JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT) ||
        !JS_DefineProperty(cx, result, "height", INT_TO_JSVAL(h), NULL, NULL,
                           JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT) ||
        !JS_DefineProperty(cx, result, "data", OBJECT_TO_JSVAL(darray), NULL, NULL,
                           JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT)) {
        return false;
    }

    *vp = OBJECT_TO_JSVAL(result);
    return true;
}

static bool
GetImageDataDimensions(JSContext *cx, JSObject *dataObject, uint32 *width, uint32 *height)
{
    jsval temp;
    int32 wi, hi;
    
    // Need to check that dataObject is ImageData object. That's hard for the moment 
    // because they're just vanilla objects in our implementation.
    // Let's guess, if the object has valid width and height then it's suitable
    // for this operation.
    if (!JS_GetProperty(cx, dataObject, "width", &temp) ||
        !JS_ValueToECMAInt32(cx, temp, &wi))
        return false;

    if (!JS_GetProperty(cx, dataObject, "height", &temp) ||
        !JS_ValueToECMAInt32(cx, temp, &hi))
        return false;

    if (wi <= 0 || hi <= 0)
        return xpc_qsThrow(cx, NS_ERROR_DOM_INDEX_SIZE_ERR);

    *width = (uint32)wi;
    *height = (uint32)hi;
    return true;
}

static JSBool
nsIDOMCanvasRenderingContext2D_CreateImageData(JSContext *cx, uintN argc, jsval *vp)
{
    XPC_QS_ASSERT_CONTEXT_OK(cx);

    /* Note: this doesn't need JS_THIS_OBJECT */

    if (argc < 1)
        return xpc_qsThrow(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);

    jsval *argv = JS_ARGV(cx, vp);

    if (argc == 1) {
        // The specification asks to throw NOT_SUPPORTED if first argument is NULL,
        // An object is expected, so throw an exception for all primitives.
        if (JSVAL_IS_PRIMITIVE(argv[0]))
            return xpc_qsThrow(cx, NS_ERROR_DOM_NOT_SUPPORTED_ERR);

        JSObject *dataObject = JSVAL_TO_OBJECT(argv[0]);

        uint32 data_width, data_height;
        if (!GetImageDataDimensions(cx, dataObject, &data_width, &data_height))
            return false;

        return CreateImageData(cx, data_width, data_height, NULL, 0, 0, vp);
    }

    jsdouble width, height;
    if (!JS_ValueToNumber(cx, argv[0], &width) ||
        !JS_ValueToNumber(cx, argv[1], &height))
        return false;

    if (!NS_finite(width) || !NS_finite(height))
        return xpc_qsThrow(cx, NS_ERROR_DOM_NOT_SUPPORTED_ERR);

    if (!width || !height)
        return xpc_qsThrow(cx, NS_ERROR_DOM_INDEX_SIZE_ERR);

    int32 wi = JS_DoubleToInt32(width);
    int32 hi = JS_DoubleToInt32(height);

    uint32 w = NS_ABS(wi);
    uint32 h = NS_ABS(hi);
    return CreateImageData(cx, w, h, NULL, 0, 0, vp);
}

static JSBool
nsIDOMCanvasRenderingContext2D_GetImageData(JSContext *cx, uintN argc, jsval *vp)
{
    XPC_QS_ASSERT_CONTEXT_OK(cx);

    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return JS_FALSE;

    nsIDOMCanvasRenderingContext2D *self;
    xpc_qsSelfRef selfref;
    js::AutoValueRooter tvr(cx);
    if (!xpc_qsUnwrapThis(cx, obj, nsnull, &self, &selfref.ptr, tvr.jsval_addr(), nsnull))
        return JS_FALSE;

    if (argc < 4)
        return xpc_qsThrow(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);

    jsval *argv = JS_ARGV(cx, vp);

    jsdouble xd, yd, width, height;
    if (!JS_ValueToNumber(cx, argv[0], &xd) ||
        !JS_ValueToNumber(cx, argv[1], &yd) ||
        !JS_ValueToNumber(cx, argv[2], &width) ||
        !JS_ValueToNumber(cx, argv[3], &height))
        return false;

    if (!NS_finite(xd) || !NS_finite(yd) ||
        !NS_finite(width) || !NS_finite(height))
        return xpc_qsThrow(cx, NS_ERROR_DOM_NOT_SUPPORTED_ERR);

    if (!width || !height)
        return xpc_qsThrow(cx, NS_ERROR_DOM_INDEX_SIZE_ERR);

    int32 x = JS_DoubleToInt32(xd);
    int32 y = JS_DoubleToInt32(yd);
    int32 wi = JS_DoubleToInt32(width);
    int32 hi = JS_DoubleToInt32(height);

    // Handle negative width and height by flipping the rectangle over in the
    // relevant direction.
    uint32 w, h;
    if (width < 0) {
        w = -wi;
        x -= w;
    } else {
        w = wi;
    }
    if (height < 0) {
        h = -hi;
        y -= h;
    } else {
        h = hi;
    }
    return CreateImageData(cx, w, h, self, x, y, vp);
}

static JSBool
nsIDOMCanvasRenderingContext2D_PutImageData(JSContext *cx, uintN argc, jsval *vp)
{
    XPC_QS_ASSERT_CONTEXT_OK(cx);

    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return JS_FALSE;

    nsresult rv;

    nsIDOMCanvasRenderingContext2D *self;
    xpc_qsSelfRef selfref;
    js::AutoValueRooter tvr(cx);
    if (!xpc_qsUnwrapThis(cx, obj, nsnull, &self, &selfref.ptr, tvr.jsval_addr(), nsnull))
        return JS_FALSE;

    if (argc < 3)
        return xpc_qsThrow(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);

    jsval *argv = JS_ARGV(cx, vp);

    if (JSVAL_IS_PRIMITIVE(argv[0]))
        return xpc_qsThrow(cx, NS_ERROR_DOM_TYPE_MISMATCH_ERR);

    JSObject *dataObject = JSVAL_TO_OBJECT(argv[0]);
    int32 x, y;
    if (!JS_ValueToECMAInt32(cx, argv[1], &x) ||
        !JS_ValueToECMAInt32(cx, argv[2], &y))
        return JS_FALSE;

    uint32 w, h;
    JSObject *darray;

    // grab width, height, and the dense array from the dataObject
    js::AutoValueRooter tv(cx);

    if (!GetImageDataDimensions(cx, dataObject, &w, &h))
        return JS_FALSE;

    // the optional dirty rect
    bool hasDirtyRect = false;
    int32 dirtyX = 0,
          dirtyY = 0,
          dirtyWidth = w,
          dirtyHeight = h;

    if (argc >= 7) {
        if (!JS_ValueToECMAInt32(cx, argv[3], &dirtyX) ||
            !JS_ValueToECMAInt32(cx, argv[4], &dirtyY) ||
            !JS_ValueToECMAInt32(cx, argv[5], &dirtyWidth) ||
            !JS_ValueToECMAInt32(cx, argv[6], &dirtyHeight))
            return JS_FALSE;

        hasDirtyRect = PR_TRUE;
    }

    if (!JS_GetProperty(cx, dataObject, "data", tv.jsval_addr()))
        return JS_FALSE;

    if (JSVAL_IS_PRIMITIVE(tv.jsval_value()))
        return xpc_qsThrow(cx, NS_ERROR_DOM_TYPE_MISMATCH_ERR);

    darray = JSVAL_TO_OBJECT(tv.jsval_value());

    js::AutoValueRooter tsrc_tvr(cx);

    JSObject *tsrc = NULL;
    if (darray->getClass() == &js::TypedArray::fastClasses[js::TypedArray::TYPE_UINT8] ||
        darray->getClass() == &js::TypedArray::fastClasses[js::TypedArray::TYPE_UINT8_CLAMPED])
    {
        tsrc = js::TypedArray::getTypedArray(darray);
    } else if (JS_IsArrayObject(cx, darray) || js_IsTypedArray(darray)) {
        // ugh, this isn't a uint8 typed array, someone made their own object; convert it to a typed array
        JSObject *nobj = js_CreateTypedArrayWithArray(cx, js::TypedArray::TYPE_UINT8, darray);
        if (!nobj)
            return JS_FALSE;

        *tsrc_tvr.jsval_addr() = OBJECT_TO_JSVAL(nobj);
        tsrc = js::TypedArray::getTypedArray(nobj);
    } else {
        // yeah, no.
        return xpc_qsThrow(cx, NS_ERROR_DOM_TYPE_MISMATCH_ERR);
    }

    // make the call
    rv = self->PutImageData_explicit(x, y, w, h, (PRUint8*) JS_GetTypedArrayData(tsrc), JS_GetTypedArrayByteLength(tsrc), hasDirtyRect, dirtyX, dirtyY, dirtyWidth, dirtyHeight);
    if (NS_FAILED(rv))
        return xpc_qsThrowMethodFailed(cx, rv, vp);

    *vp = JSVAL_VOID;
    return JS_TRUE;
}

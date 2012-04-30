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
#include "CustomQS_Canvas.h"

#include "jsapi.h"
#include "jsfriendapi.h"

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
    JS::AutoValueRooter tvr(cx);
    if (!xpc_qsUnwrapThis(cx, obj, &self, &selfref.ptr, tvr.jsval_addr(), nsnull))
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

        rv = (self->*setfunc)(NullString(), arg0);
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
    if (!xpc_qsUnwrapThis(cx, obj, &self, &selfref.ptr, vp, &lccx))
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
        return xpc::StringToJsval(cx, resultString, vp);

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
CreateImageData(JSContext* cx, JSObject* obj, uint32_t w, uint32_t h, jsval* vp)
{
    using mozilla::CheckedInt;

    if (w == 0)
        w = 1;
    if (h == 0)
        h = 1;

    CheckedInt<uint32_t> len = CheckedInt<uint32_t>(w) * h * 4;
    if (!len.valid()) {
        return xpc_qsThrow(cx, NS_ERROR_DOM_INDEX_SIZE_ERR);
    }

    // Create the fast typed array; it's initialized to 0 by default.
    JSObject* darray = JS_NewUint8ClampedArray(cx, len.value());
    JS::AutoObjectRooter rd(cx, darray);
    if (!darray) {
        return false;
    }

    XPCLazyCallContext lccx(JS_CALLER, cx, obj);
    const nsIID *iid = &NS_GET_IID(nsIDOMImageData);
    nsRefPtr<mozilla::dom::ImageData> imageData =
        new mozilla::dom::ImageData(w, h, *darray);
    qsObjectHelper helper(imageData, NULL);
    return xpc_qsXPCOMObjectToJsval(lccx, helper, iid,
                                    &interfaces[k_nsIDOMImageData], vp);
}

static JSBool
nsIDOMCanvasRenderingContext2D_CreateImageData(JSContext *cx, unsigned argc, jsval *vp)
{
    XPC_QS_ASSERT_CONTEXT_OK(cx);

    JSObject* obj = JS_THIS_OBJECT(cx, vp);
    if (!obj) {
        return false;
    }

    if (argc < 1)
        return xpc_qsThrow(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);

    jsval *argv = JS_ARGV(cx, vp);

    if (argc == 1) {
        uint32_t data_width, data_height;
        JS::Anchor<JSObject*> darray;
        if (!GetImageData(cx, argv[0], &data_width, &data_height, &darray)) {
            return false;
        }

        return CreateImageData(cx, obj, data_width, data_height, vp);
    }

    double width, height;
    if (!JS_ValueToNumber(cx, argv[0], &width) ||
        !JS_ValueToNumber(cx, argv[1], &height))
        return false;

    if (!NS_finite(width) || !NS_finite(height))
        return xpc_qsThrow(cx, NS_ERROR_DOM_NOT_SUPPORTED_ERR);

    if (!width || !height)
        return xpc_qsThrow(cx, NS_ERROR_DOM_INDEX_SIZE_ERR);

    int32_t wi = JS_DoubleToInt32(width);
    int32_t hi = JS_DoubleToInt32(height);

    uint32_t w = NS_ABS(wi);
    uint32_t h = NS_ABS(hi);
    return CreateImageData(cx, obj, w, h, vp);
}

static JSBool
nsIDOMCanvasRenderingContext2D_PutImageData(JSContext *cx, unsigned argc, jsval *vp)
{
    XPC_QS_ASSERT_CONTEXT_OK(cx);

    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return JS_FALSE;

    nsresult rv;

    nsIDOMCanvasRenderingContext2D *self;
    xpc_qsSelfRef selfref;
    JS::AutoValueRooter tvr(cx);
    if (!xpc_qsUnwrapThis(cx, obj, &self, &selfref.ptr, tvr.jsval_addr(), nsnull))
        return JS_FALSE;

    if (argc < 3)
        return xpc_qsThrow(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);

    jsval *argv = JS_ARGV(cx, vp);

    uint32_t w, h;
    JS::Anchor<JSObject*> darray;
    if (!GetImageData(cx, argv[0], &w, &h, &darray)) {
        return false;
    }

    double xd, yd;
    if (!JS_ValueToNumber(cx, argv[1], &xd) ||
        !JS_ValueToNumber(cx, argv[2], &yd)) {
        return false;
    }

    if (!NS_finite(xd) || !NS_finite(yd)) {
        return xpc_qsThrow(cx, NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    }

    int32_t x = JS_DoubleToInt32(xd);
    int32_t y = JS_DoubleToInt32(yd);

    // the optional dirty rect
    bool hasDirtyRect = false;
    int32_t dirtyX = 0,
            dirtyY = 0,
            dirtyWidth = w,
            dirtyHeight = h;

    if (argc >= 7) {
        double dx, dy, dw, dh;
        if (!JS_ValueToNumber(cx, argv[3], &dx) ||
            !JS_ValueToNumber(cx, argv[4], &dy) ||
            !JS_ValueToNumber(cx, argv[5], &dw) ||
            !JS_ValueToNumber(cx, argv[6], &dh)) {
            return false;
        }

        if (!NS_finite(dx) || !NS_finite(dy) ||
            !NS_finite(dw) || !NS_finite(dh)) {
            return xpc_qsThrow(cx, NS_ERROR_DOM_NOT_SUPPORTED_ERR);
        }

        dirtyX = JS_DoubleToInt32(dx);
        dirtyY = JS_DoubleToInt32(dy);
        dirtyWidth = JS_DoubleToInt32(dw);
        dirtyHeight = JS_DoubleToInt32(dh);

        hasDirtyRect = true;
    }

    JS::AutoValueRooter tsrc_tvr(cx);

    JSObject * tsrc = NULL;
    if (JS_IsInt8Array(darray.get(), cx) ||
        JS_IsUint8Array(darray.get(), cx) ||
        JS_IsUint8ClampedArray(darray.get(), cx))
    {
        tsrc = darray.get();
    } else if (JS_IsTypedArrayObject(darray.get(), cx) || JS_IsArrayObject(cx, darray.get())) {
        // ugh, this isn't a uint8 typed array, someone made their own object; convert it to a typed array
        JSObject *nobj = JS_NewUint8ClampedArrayFromArray(cx, darray.get());
        if (!nobj)
            return JS_FALSE;

        *tsrc_tvr.jsval_addr() = OBJECT_TO_JSVAL(nobj);
        tsrc = nobj;
    } else {
        // yeah, no.
        return xpc_qsThrow(cx, NS_ERROR_DOM_TYPE_MISMATCH_ERR);
    }

    // make the call
    MOZ_ASSERT(JS_IsTypedArrayObject(tsrc, cx));
    PRUint8* data = reinterpret_cast<PRUint8*>(JS_GetArrayBufferViewData(tsrc, cx));
    uint32_t byteLength = JS_GetTypedArrayByteLength(tsrc, cx);
    rv = self->PutImageData_explicit(x, y, w, h, data, byteLength, hasDirtyRect, dirtyX, dirtyY, dirtyWidth, dirtyHeight);
    if (NS_FAILED(rv))
        return xpc_qsThrowMethodFailed(cx, rv, vp);

    *vp = JSVAL_VOID;
    return JS_TRUE;
}

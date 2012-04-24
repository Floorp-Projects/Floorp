/* -*- Mode: c++; c-basic-offset: 4; tab-width: 40; indent-tabs-mode: nil; -*- */
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
 *   Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com> (original author)
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

/*
 * Intended to be #included in dom_quickstubs.cpp via qsconf!
 */

#include "jsapi.h"
#include "jsfriendapi.h"
#include "CustomQS_Canvas.h"

#define GET_INT32_ARG(var, index) \
  int32_t var; \
  do { \
    if (!JS_ValueToECMAInt32(cx, argv[index], &(var))) \
      return JS_FALSE; \
  } while (0)

#define GET_UINT32_ARG(var, index) \
  uint32_t var; \
  do { \
    if (!JS_ValueToECMAUint32(cx, argv[index], &(var))) \
      return JS_FALSE; \
  } while (0)

class CallTexImage2D
{
private:
    nsIDOMWebGLRenderingContext* self;
    WebGLenum target;
    WebGLint level;
    WebGLenum internalformat;
    WebGLenum format;
    WebGLenum type;

public:
    explicit CallTexImage2D(nsIDOMWebGLRenderingContext* aSelf,
                            WebGLenum aTarget,
                            WebGLint aLevel,
                            WebGLenum aInternalformat,
                            WebGLenum aFormat,
                            WebGLenum aType)
        : self(aSelf)
        , target(aTarget)
        , level(aLevel)
        , internalformat(aInternalformat)
        , format(aFormat)
        , type(aType)
    {}

    nsresult DoCallForImageData(WebGLsizei width, WebGLsizei height,
                                JSObject* pixels, JSContext *cx)
    {
        return self->TexImage2D_imageData(target, level, internalformat, width,
                                          height, 0, format, type, pixels, cx);
    }
    nsresult DoCallForElement(mozilla::dom::Element* elt)
    {
        return self->TexImage2D_dom(target, level, internalformat, format, type,
                                    elt);
    }
};

class CallTexSubImage2D
{
private:
    nsIDOMWebGLRenderingContext* self;
    WebGLenum target;
    WebGLint level;
    WebGLint xoffset;
    WebGLint yoffset;
    WebGLenum format;
    WebGLenum type;

public:
    explicit CallTexSubImage2D(nsIDOMWebGLRenderingContext* aSelf,
                               WebGLenum aTarget,
                               WebGLint aLevel,
                               WebGLint aXoffset,
                               WebGLint aYoffset,
                               WebGLenum aFormat,
                               WebGLenum aType)

        : self(aSelf)
        , target(aTarget)
        , level(aLevel)
        , xoffset(aXoffset)
        , yoffset(aYoffset)
        , format(aFormat)
        , type(aType)
    {}

    nsresult DoCallForImageData(WebGLsizei width, WebGLsizei height,
                                JSObject* pixels, JSContext *cx)
    {
        return self->TexSubImage2D_imageData(target, level, xoffset, yoffset,
                                             width, height, format, type,
                                             pixels, cx);
    }
    nsresult DoCallForElement(mozilla::dom::Element* elt)
    {
        return self->TexSubImage2D_dom(target, level, xoffset, yoffset, format,
                                       type, elt);
    }
};

template<class T>
static bool
TexImage2DImageDataOrElement(JSContext* cx, T& self, JS::Value* object)
{
    MOZ_ASSERT(object && object->isObject());

    nsGenericElement* elt;
    xpc_qsSelfRef eltRef;
    if (NS_SUCCEEDED(xpc_qsUnwrapArg<nsGenericElement>(
            cx, *object, &elt, &eltRef.ptr, object))) {
        nsresult rv = self.DoCallForElement(elt);
        return NS_SUCCEEDED(rv) || xpc_qsThrow(cx, rv);
    }

    // Failed to interpret object as an Element, now try to interpret it as
    // ImageData.
    uint32_t int_width, int_height;
    JS::Anchor<JSObject*> obj_data;
    if (!GetImageData(cx, *object, &int_width, &int_height, &obj_data)) {
        return false;
    }
    if (!JS_IsTypedArrayObject(obj_data.get(), cx)) {
        return xpc_qsThrow(cx, NS_ERROR_FAILURE);
    }

    nsresult rv = self.DoCallForImageData(int_width, int_height, obj_data.get(), cx);
    return NS_SUCCEEDED(rv) || xpc_qsThrow(cx, rv);
}

/*
 * TexImage2D takes:
 *    TexImage2D(uint, int, uint, int, int, int, uint, uint, ArrayBufferView)
 *    TexImage2D(uint, int, uint, uint, uint, nsIDOMElement)
 *    TexImage2D(uint, int, uint, uint, uint, ImageData)
 */
static JSBool
nsIDOMWebGLRenderingContext_TexImage2D(JSContext *cx, unsigned argc, jsval *vp)
{
    XPC_QS_ASSERT_CONTEXT_OK(cx);
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return JS_FALSE;

    nsresult rv;

    nsIDOMWebGLRenderingContext *self;
    xpc_qsSelfRef selfref;
    JS::AutoValueRooter tvr(cx);
    if (!xpc_qsUnwrapThis(cx, obj, &self, &selfref.ptr, tvr.jsval_addr(), nsnull))
        return JS_FALSE;

    if (argc < 6 || argc == 7 || argc == 8)
        return xpc_qsThrow(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);

    jsval *argv = JS_ARGV(cx, vp);

    // arguments common to all cases
    GET_UINT32_ARG(argv0, 0);
    GET_INT32_ARG(argv1, 1);
    GET_UINT32_ARG(argv2, 2);

    if (argc > 5 && !JSVAL_IS_PRIMITIVE(argv[5])) {
        // implement the variants taking a DOMElement as argv[5]
        GET_UINT32_ARG(argv3, 3);
        GET_UINT32_ARG(argv4, 4);

        CallTexImage2D selfCaller(self, argv0, argv1, argv2, argv3, argv4);
        if (!TexImage2DImageDataOrElement(cx, selfCaller, argv + 5)) {
            return false;
        }
        rv = NS_OK;
    } else if (argc > 8 && JSVAL_IS_OBJECT(argv[8])) {
        // here, we allow null !
        // implement the variants taking a buffer/array as argv[8]
        GET_INT32_ARG(argv3, 3);
        GET_INT32_ARG(argv4, 4);
        GET_INT32_ARG(argv5, 5);
        GET_UINT32_ARG(argv6, 6);
        GET_UINT32_ARG(argv7, 7);

        JSObject *argv8 = JSVAL_TO_OBJECT(argv[8]);

        // then try to grab either a js::TypedArray, or null
        if (argv8 == nsnull) {
            rv = self->TexImage2D_array(argv0, argv1, argv2, argv3,
                                        argv4, argv5, argv6, argv7,
                                        nsnull, cx);
        } else if (JS_IsTypedArrayObject(argv8, cx)) {
            rv = self->TexImage2D_array(argv0, argv1, argv2, argv3,
                                        argv4, argv5, argv6, argv7,
                                        argv8, cx);
        } else {
            xpc_qsThrowBadArg(cx, NS_ERROR_FAILURE, vp, 8);
            return JS_FALSE;
        }
    } else {
        xpc_qsThrow(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);
        return JS_FALSE;
    }

    if (NS_FAILED(rv))
        return xpc_qsThrowMethodFailed(cx, rv, vp);

    *vp = JSVAL_VOID;
    return JS_TRUE;
}

/* TexSubImage2D takes:
 *    TexSubImage2D(uint, int, int, int, int, int, uint, uint, ArrayBufferView)
 *    TexSubImage2D(uint, int, int, int, uint, uint, nsIDOMElement)
 *    TexSubImage2D(uint, int, int, int, uint, uint, ImageData)
 */
static JSBool
nsIDOMWebGLRenderingContext_TexSubImage2D(JSContext *cx, unsigned argc, jsval *vp)
{
    XPC_QS_ASSERT_CONTEXT_OK(cx);
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return JS_FALSE;

    nsresult rv;

    nsIDOMWebGLRenderingContext *self;
    xpc_qsSelfRef selfref;
    JS::AutoValueRooter tvr(cx);
    if (!xpc_qsUnwrapThis(cx, obj, &self, &selfref.ptr, tvr.jsval_addr(), nsnull))
        return JS_FALSE;

    if (argc < 7 || argc == 8)
        return xpc_qsThrow(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);

    jsval *argv = JS_ARGV(cx, vp);

    // arguments common to all cases
    GET_UINT32_ARG(argv0, 0);
    GET_INT32_ARG(argv1, 1);
    GET_INT32_ARG(argv2, 2);
    GET_INT32_ARG(argv3, 3);

    if (argc > 6 && !JSVAL_IS_PRIMITIVE(argv[6])) {
        // implement the variants taking a DOMElement or an ImageData as argv[6]
        GET_UINT32_ARG(argv4, 4);
        GET_UINT32_ARG(argv5, 5);

        CallTexSubImage2D selfCaller(self, argv0, argv1, argv2, argv3, argv4, argv5);
        if (!TexImage2DImageDataOrElement(cx, selfCaller, argv + 6)) {
            return false;
        }
        rv = NS_OK;
    } else if (argc > 8 && !JSVAL_IS_PRIMITIVE(argv[8])) {
        // implement the variants taking a buffer/array as argv[8]
        GET_INT32_ARG(argv4, 4);
        GET_INT32_ARG(argv5, 5);
        GET_UINT32_ARG(argv6, 6);
        GET_UINT32_ARG(argv7, 7);

        JSObject *argv8 = JSVAL_TO_OBJECT(argv[8]);
        // try to grab a js::TypedArray
        if (JS_IsTypedArrayObject(argv8, cx)) {
            rv = self->TexSubImage2D_array(argv0, argv1, argv2, argv3,
                                           argv4, argv5, argv6, argv7,
                                           argv8, cx);
        } else {
            xpc_qsThrowBadArg(cx, NS_ERROR_FAILURE, vp, 8);
            return JS_FALSE;
        }
    } else {
        xpc_qsThrow(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);
        return JS_FALSE;
    }

    if (NS_FAILED(rv))
        return xpc_qsThrowMethodFailed(cx, rv, vp);

    *vp = JSVAL_VOID;
    return JS_TRUE;
}

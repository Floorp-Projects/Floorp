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

#include "jstypedarray.h"

#define GET_INT32_ARG(var, index) \
  int32 var; \
  do { \
    if (!JS_ValueToECMAInt32(cx, argv[index], &(var))) \
      return JS_FALSE; \
  } while (0)

#define GET_UINT32_ARG(var, index) \
  uint32 var; \
  do { \
    if (!JS_ValueToECMAUint32(cx, argv[index], &(var))) \
      return JS_FALSE; \
  } while (0)

#define GET_OPTIONAL_UINT32_ARG(var, index) \
  uint32 var = 0; \
  do { \
    if (argc > index) \
      if (!JS_ValueToECMAUint32(cx, argv[index], &(var))) \
        return JS_FALSE; \
  } while (0)


static inline bool
helper_isInt32Array(JSObject *obj) {
    return obj->getClass() == &js::TypedArray::fastClasses[js::TypedArray::TYPE_INT32];
}

static inline bool
helper_isFloat32Array(JSObject *obj) {
    return obj->getClass() == &js::TypedArray::fastClasses[js::TypedArray::TYPE_FLOAT32];
}

/*
 * BufferData takes:
 *    BufferData (int, int, int)
 *    BufferData_buf (int, js::ArrayBuffer *, int)
 *    BufferData_array (int, js::TypedArray *, int)
 */
static JSBool
nsICanvasRenderingContextWebGL_BufferData(JSContext *cx, uintN argc, jsval *vp)
{
    XPC_QS_ASSERT_CONTEXT_OK(cx);
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return JS_FALSE;

    nsICanvasRenderingContextWebGL *self;
    xpc_qsSelfRef selfref;
    js::AutoValueRooter tvr(cx);
    if (!xpc_qsUnwrapThis(cx, obj, nsnull, &self, &selfref.ptr, tvr.jsval_addr(), nsnull))
        return JS_FALSE;

    if (argc < 3)
        return xpc_qsThrow(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);

    jsval *argv = JS_ARGV(cx, vp);

    int32 target;
    js::TypedArray *wa = 0;
    js::ArrayBuffer *wb = 0;
    int32 size;
    int32 usage;

    if (!JS_ValueToECMAInt32(cx, argv[0], &target))
        return JS_FALSE;
    if (!JS_ValueToECMAInt32(cx, argv[2], &usage))
        return JS_FALSE;

    if (!JSVAL_IS_PRIMITIVE(argv[1])) {

        JSObject *arg2 = JSVAL_TO_OBJECT(argv[1]);
        if (js_IsArrayBuffer(arg2)) {
            wb = js::ArrayBuffer::fromJSObject(arg2);
        } else if (js_IsTypedArray(arg2)) {
            wa = js::TypedArray::fromJSObject(arg2);
        }
    }

    if (!wa && !wb &&
        !JS_ValueToECMAInt32(cx, argv[1], &size))
    {
        return JS_FALSE;
    }

    nsresult rv;

    if (wa)
        rv = self->BufferData_array(target, wa, usage);
    else if (wb)
        rv = self->BufferData_buf(target, wb, usage);
    else
        rv = self->BufferData_size(target, size, usage);

    if (NS_FAILED(rv))
        return xpc_qsThrowMethodFailed(cx, rv, vp);

    *vp = JSVAL_VOID;
    return JS_TRUE;
}

/*
 * BufferSubData takes:
 *    BufferSubData (int, int, js::ArrayBuffer *)
 *    BufferSubData_array (int, int, js::TypedArray *)
 */
static JSBool
nsICanvasRenderingContextWebGL_BufferSubData(JSContext *cx, uintN argc, jsval *vp)
{
    XPC_QS_ASSERT_CONTEXT_OK(cx);
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return JS_FALSE;

    nsICanvasRenderingContextWebGL *self;
    xpc_qsSelfRef selfref;
    js::AutoValueRooter tvr(cx);
    if (!xpc_qsUnwrapThis(cx, obj, nsnull, &self, &selfref.ptr, tvr.jsval_addr(), nsnull))
        return JS_FALSE;

    if (argc < 3)
        return xpc_qsThrow(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);

    jsval *argv = JS_ARGV(cx, vp);

    int32 target;
    int32 offset;
    js::TypedArray *wa = 0;
    js::ArrayBuffer *wb = 0;

    if (!JS_ValueToECMAInt32(cx, argv[0], &target))
        return JS_FALSE;
    if (!JS_ValueToECMAInt32(cx, argv[1], &offset))
        return JS_FALSE;

    if (JSVAL_IS_PRIMITIVE(argv[2])) {
        xpc_qsThrowBadArg(cx, NS_ERROR_FAILURE, vp, 2);
        return JS_FALSE;
    }

    JSObject *arg3 = JSVAL_TO_OBJECT(argv[2]);
    if (js_IsArrayBuffer(arg3)) {
        wb = js::ArrayBuffer::fromJSObject(arg3);
    } else if (js_IsTypedArray(arg3)) {
        wa = js::TypedArray::fromJSObject(arg3);
    } else {
        xpc_qsThrowBadArg(cx, NS_ERROR_FAILURE, vp, 2);
        return JS_FALSE;
    }

    nsresult rv;

    if (wa) {
        rv = self->BufferSubData_array(target, offset, wa);
    } else if (wb) {
        rv = self->BufferSubData_buf(target, offset, wb);
    } else {
        xpc_qsThrowBadArg(cx, NS_ERROR_FAILURE, vp, 2);
        return JS_FALSE;
    }

    if (NS_FAILED(rv))
        return xpc_qsThrowMethodFailed(cx, rv, vp);

    *vp = JSVAL_VOID;
    return JS_TRUE;
}

/*
 * ReadPixels takes:
 *    ReadPixels(int, int, int, int, uint, uint, ArrayBufferView)
 */
static JSBool
nsICanvasRenderingContextWebGL_ReadPixels(JSContext *cx, uintN argc, jsval *vp)
{
    XPC_QS_ASSERT_CONTEXT_OK(cx);
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return JS_FALSE;

    nsresult rv;

    nsICanvasRenderingContextWebGL *self;
    xpc_qsSelfRef selfref;
    js::AutoValueRooter tvr(cx);
    if (!xpc_qsUnwrapThis(cx, obj, nsnull, &self, &selfref.ptr, tvr.jsval_addr(), nsnull))
        return JS_FALSE;

    if (argc < 7)
        return xpc_qsThrow(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);

    jsval *argv = JS_ARGV(cx, vp);

    // arguments common to all cases
    GET_INT32_ARG(argv0, 0);
    GET_INT32_ARG(argv1, 1);
    GET_INT32_ARG(argv2, 2);
    GET_INT32_ARG(argv3, 3);
    GET_UINT32_ARG(argv4, 4);
    GET_UINT32_ARG(argv5, 5);

    if (argc == 7 &&
        !JSVAL_IS_PRIMITIVE(argv[6]))
    {
        JSObject *argv6 = JSVAL_TO_OBJECT(argv[6]);
        if (js_IsArrayBuffer(argv6)) {
            rv = self->ReadPixels_buf(argv0, argv1, argv2, argv3,
                                      argv4, argv5, js::ArrayBuffer::fromJSObject(argv6));
        } else if (js_IsTypedArray(argv6)) {
            rv = self->ReadPixels_array(argv0, argv1, argv2, argv3,
                                        argv4, argv5,
                                        js::TypedArray::fromJSObject(argv6));
        } else {
            xpc_qsThrowBadArg(cx, NS_ERROR_FAILURE, vp, 6);
            return JS_FALSE;
        }
    } else {
        xpc_qsThrow(cx, NS_ERROR_FAILURE);
        return JS_FALSE;
    }

    if (NS_FAILED(rv))
        return xpc_qsThrowMethodFailed(cx, rv, vp);

    *vp = JSVAL_VOID;
    return JS_TRUE;
}


/*
 * TexImage2D takes:
 *    TexImage2D(uint, int, uint, int, int, int, uint, uint, ArrayBufferView)
 *    TexImage2D(uint, int, uint, uint, uint, nsIDOMElement)
 *    TexImage2D(uint, int, uint, uint, uint, ImageData)
 */
static JSBool
nsICanvasRenderingContextWebGL_TexImage2D(JSContext *cx, uintN argc, jsval *vp)
{
    XPC_QS_ASSERT_CONTEXT_OK(cx);
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return JS_FALSE;

    nsresult rv;

    nsICanvasRenderingContextWebGL *self;
    xpc_qsSelfRef selfref;
    js::AutoValueRooter tvr(cx);
    if (!xpc_qsUnwrapThis(cx, obj, nsnull, &self, &selfref.ptr, tvr.jsval_addr(), nsnull))
        return JS_FALSE;

    if (argc < 6 || argc == 7 || argc == 8)
        return xpc_qsThrow(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);

    jsval *argv = JS_ARGV(cx, vp);

    // arguments common to all cases
    GET_UINT32_ARG(argv0, 0);
    GET_INT32_ARG(argv1, 1);

    if (argc > 5 &&
        !JSVAL_IS_PRIMITIVE(argv[5]))
    {
        // implement the variants taking a DOMElement as argv[5]
        GET_UINT32_ARG(argv2, 2);
        GET_UINT32_ARG(argv3, 3);
        GET_UINT32_ARG(argv4, 4);

        nsIDOMElement *elt;
        xpc_qsSelfRef eltRef;
        rv = xpc_qsUnwrapArg<nsIDOMElement>(cx, argv[5], &elt, &eltRef.ptr, &argv[5]);
        if (NS_FAILED(rv)) return JS_FALSE;

        rv = self->TexImage2D_dom(argv0, argv1, argv2, argv3, argv4, elt);

        if (NS_FAILED(rv)) {
            // failed to interprete argv[5] as a DOMElement, now try to interprete it as ImageData
            JSObject *argv5 = JSVAL_TO_OBJECT(argv[5]);

            jsval js_width, js_height, js_data;
            JS_GetProperty(cx, argv5, "width", &js_width);
            JS_GetProperty(cx, argv5, "height", &js_height);
            JS_GetProperty(cx, argv5, "data", &js_data);
            if (js_width  == JSVAL_VOID ||
                js_height == JSVAL_VOID ||
                js_data   == JSVAL_VOID)
            {
                xpc_qsThrowBadArg(cx, NS_ERROR_FAILURE, vp, 5);
                return JS_FALSE;
            }
            int32 int_width, int_height;
            JSObject *obj_data = JSVAL_TO_OBJECT(js_data);
            if (!JS_ValueToECMAInt32(cx, js_width, &int_width) ||
                !JS_ValueToECMAInt32(cx, js_height, &int_height))
            {
                return JS_FALSE;
            }
            if (!js_IsTypedArray(obj_data))
            {
                xpc_qsThrowBadArg(cx, NS_ERROR_FAILURE, vp, 5);
                return JS_FALSE;
            }
            rv = self->TexImage2D_array(argv0, argv1, argv2,
                                        int_width, int_height, 0,
                                        argv3, argv4, js::TypedArray::fromJSObject(obj_data));
        }
    } else if (argc > 8 &&
               JSVAL_IS_OBJECT(argv[8])) // here, we allow null !
    {
        // implement the variants taking a buffer/array as argv[8]
        GET_UINT32_ARG(argv2, 2);
        GET_INT32_ARG(argv3, 3);
        GET_INT32_ARG(argv4, 4);
        GET_INT32_ARG(argv5, 5);
        GET_UINT32_ARG(argv6, 6);
        GET_UINT32_ARG(argv7, 7);

        JSObject *argv8 = JSVAL_TO_OBJECT(argv[8]);

        // then try to grab either a js::ArrayBuffer, js::TypedArray, or null
        if (argv8 == nsnull) {
            rv = self->TexImage2D_buf(argv0, argv1, argv2, argv3,
                                      argv4, argv5, argv6, argv7,
                                      nsnull);
        } else if (js_IsArrayBuffer(argv8)) {
            rv = self->TexImage2D_buf(argv0, argv1, argv2, argv3,
                                      argv4, argv5, argv6, argv7,
                                      js::ArrayBuffer::fromJSObject(argv8));
        } else if (js_IsTypedArray(argv8)) {
            rv = self->TexImage2D_array(argv0, argv1, argv2, argv3,
                                        argv4, argv5, argv6, argv7,
                                        js::TypedArray::fromJSObject(argv8));
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
nsICanvasRenderingContextWebGL_TexSubImage2D(JSContext *cx, uintN argc, jsval *vp)
{
    XPC_QS_ASSERT_CONTEXT_OK(cx);
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return JS_FALSE;

    nsresult rv;

    nsICanvasRenderingContextWebGL *self;
    xpc_qsSelfRef selfref;
    js::AutoValueRooter tvr(cx);
    if (!xpc_qsUnwrapThis(cx, obj, nsnull, &self, &selfref.ptr, tvr.jsval_addr(), nsnull))
        return JS_FALSE;

    if (argc < 7 || argc == 8)
        return xpc_qsThrow(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);

    jsval *argv = JS_ARGV(cx, vp);

    // arguments common to all cases
    GET_UINT32_ARG(argv0, 0);
    GET_INT32_ARG(argv1, 1);
    GET_INT32_ARG(argv2, 2);
    GET_INT32_ARG(argv3, 3);

    if (argc > 6 &&
        !JSVAL_IS_PRIMITIVE(argv[6]))
    {
        // implement the variants taking a DOMElement as argv[6]
        GET_UINT32_ARG(argv4, 4);
        GET_UINT32_ARG(argv5, 5);

        nsIDOMElement *elt;
        xpc_qsSelfRef eltRef;
        rv = xpc_qsUnwrapArg<nsIDOMElement>(cx, argv[6], &elt, &eltRef.ptr, &argv[6]);
        if (NS_FAILED(rv)) return JS_FALSE;

        rv = self->TexSubImage2D_dom(argv0, argv1, argv2, argv3, argv4, argv5, elt);

        if (NS_FAILED(rv)) {
            // failed to interprete argv[6] as a DOMElement, now try to interprete it as ImageData
            JSObject *argv6 = JSVAL_TO_OBJECT(argv[6]);
            jsval js_width, js_height, js_data;
            JS_GetProperty(cx, argv6, "width", &js_width);
            JS_GetProperty(cx, argv6, "height", &js_height);
            JS_GetProperty(cx, argv6, "data", &js_data);
            if (js_width  == JSVAL_VOID ||
                js_height == JSVAL_VOID ||
                js_data   == JSVAL_VOID)
            {
                xpc_qsThrowBadArg(cx, NS_ERROR_FAILURE, vp, 6);
                return JS_FALSE;
            }
            int32 int_width, int_height;
            JSObject *obj_data = JSVAL_TO_OBJECT(js_data);
            if (!JS_ValueToECMAInt32(cx, js_width, &int_width) ||
                !JS_ValueToECMAInt32(cx, js_height, &int_height))
            {
                return JS_FALSE;
            }
            if (!js_IsTypedArray(obj_data))
            {
                xpc_qsThrowBadArg(cx, NS_ERROR_FAILURE, vp, 6);
                return JS_FALSE;
            }
            rv = self->TexSubImage2D_array(argv0, argv1, argv2, argv3,
                                           int_width, int_height,
                                           argv4, argv5,
                                           js::TypedArray::fromJSObject(obj_data));
        }
    } else if (argc > 8 &&
               !JSVAL_IS_PRIMITIVE(argv[8]))
    {
        // implement the variants taking a buffer/array as argv[8]
        GET_INT32_ARG(argv4, 4);
        GET_INT32_ARG(argv5, 5);
        GET_UINT32_ARG(argv6, 6);
        GET_UINT32_ARG(argv7, 7);

        JSObject *argv8 = JSVAL_TO_OBJECT(argv[8]);
        // try to grab either a js::ArrayBuffer or js::TypedArray
        if (js_IsArrayBuffer(argv8)) {
            rv = self->TexSubImage2D_buf(argv0, argv1, argv2, argv3,
                                         argv4, argv5, argv6, argv7,
                                         js::ArrayBuffer::fromJSObject(argv8));
        } else if (js_IsTypedArray(argv8)) {
            rv = self->TexSubImage2D_array(argv0, argv1, argv2, argv3,
                                           argv4, argv5, argv6, argv7,
                                           js::TypedArray::fromJSObject(argv8));
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

/* NOTE: There is a TN version of this below, update it as well */
static inline JSBool
helper_nsICanvasRenderingContextWebGL_Uniform_x_iv(JSContext *cx, uintN argc, jsval *vp, int nElements)
{
    XPC_QS_ASSERT_CONTEXT_OK(cx);
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return JS_FALSE;

    nsresult rv;

    nsICanvasRenderingContextWebGL *self;
    xpc_qsSelfRef selfref;
    js::AutoValueRooter tvr(cx);
    if (!xpc_qsUnwrapThis(cx, obj, nsnull, &self, &selfref.ptr, tvr.jsval_addr(), nsnull))
        return JS_FALSE;

    if (argc < 2)
        return xpc_qsThrow(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);

    jsval *argv = JS_ARGV(cx, vp);

    nsIWebGLUniformLocation *location;
    xpc_qsSelfRef location_selfref;
    rv = xpc_qsUnwrapArg(cx, argv[0], &location, &location_selfref.ptr, &argv[0]);
    if (NS_FAILED(rv)) {
        xpc_qsThrowBadArg(cx, rv, vp, 0);
        return JS_FALSE;
    }

    if (JSVAL_IS_PRIMITIVE(argv[1])) {
        xpc_qsThrowBadArg(cx, NS_ERROR_FAILURE, vp, 1);
        return JS_FALSE;
    }

    JSObject *arg1 = JSVAL_TO_OBJECT(argv[1]);

    js::AutoValueRooter obj_tvr(cx);

    js::TypedArray *wa = 0;

    if (helper_isInt32Array(arg1)) {
        wa = js::TypedArray::fromJSObject(arg1);
    }  else if (JS_IsArrayObject(cx, arg1)) {
        JSObject *nobj = js_CreateTypedArrayWithArray(cx, js::TypedArray::TYPE_INT32, arg1);
        if (!nobj) {
            // XXX this will likely return a strange error message if it goes wrong
            return JS_FALSE;
        }

        *obj_tvr.jsval_addr() = OBJECT_TO_JSVAL(nobj);
        wa = js::TypedArray::fromJSObject(nobj);
    } else {
        xpc_qsThrowBadArg(cx, NS_ERROR_FAILURE, vp, 1);
        return JS_FALSE;
    }

    if (nElements == 1) {
        rv = self->Uniform1iv_array(location, wa);
    } else if (nElements == 2) {
        rv = self->Uniform2iv_array(location, wa);
    } else if (nElements == 3) {
        rv = self->Uniform3iv_array(location, wa);
    } else if (nElements == 4) {
        rv = self->Uniform4iv_array(location, wa);
    }

    if (NS_FAILED(rv))
        return xpc_qsThrowMethodFailed(cx, rv, vp);

    *vp = JSVAL_VOID;
    return JS_TRUE;
}

/* NOTE: There is a TN version of this below, update it as well */
static inline JSBool
helper_nsICanvasRenderingContextWebGL_Uniform_x_fv(JSContext *cx, uintN argc, jsval *vp, int nElements)
{
    XPC_QS_ASSERT_CONTEXT_OK(cx);
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return JS_FALSE;

    nsresult rv;

    nsICanvasRenderingContextWebGL *self;
    xpc_qsSelfRef selfref;
    js::AutoValueRooter tvr(cx);
    if (!xpc_qsUnwrapThis(cx, obj, nsnull, &self, &selfref.ptr, tvr.jsval_addr(), nsnull))
        return JS_FALSE;

    if (argc < 2)
        return xpc_qsThrow(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);

    jsval *argv = JS_ARGV(cx, vp);

    nsIWebGLUniformLocation *location;
    xpc_qsSelfRef location_selfref;
    rv = xpc_qsUnwrapArg(cx, argv[0], &location, &location_selfref.ptr, &argv[0]);
    if (NS_FAILED(rv)) {
        xpc_qsThrowBadArg(cx, rv, vp, 0);
        return JS_FALSE;
    }

    if (JSVAL_IS_PRIMITIVE(argv[1])) {
        xpc_qsThrowBadArg(cx, NS_ERROR_FAILURE, vp, 1);
        return JS_FALSE;
    }

    JSObject *arg1 = JSVAL_TO_OBJECT(argv[1]);

    js::AutoValueRooter obj_tvr(cx);

    js::TypedArray *wa = 0;

    if (helper_isFloat32Array(arg1)) {
        wa = js::TypedArray::fromJSObject(arg1);
    }  else if (JS_IsArrayObject(cx, arg1)) {
        JSObject *nobj = js_CreateTypedArrayWithArray(cx, js::TypedArray::TYPE_FLOAT32, arg1);
        if (!nobj) {
            // XXX this will likely return a strange error message if it goes wrong
            return JS_FALSE;
        }

        *obj_tvr.jsval_addr() = OBJECT_TO_JSVAL(nobj);
        wa = js::TypedArray::fromJSObject(nobj);
    } else {
        xpc_qsThrowBadArg(cx, NS_ERROR_FAILURE, vp, 1);
        return JS_FALSE;
    }

    if (nElements == 1) {
        rv = self->Uniform1fv_array(location, wa);
    } else if (nElements == 2) {
        rv = self->Uniform2fv_array(location, wa);
    } else if (nElements == 3) {
        rv = self->Uniform3fv_array(location, wa);
    } else if (nElements == 4) {
        rv = self->Uniform4fv_array(location, wa);
    }

    if (NS_FAILED(rv))
        return xpc_qsThrowMethodFailed(cx, rv, vp);

    *vp = JSVAL_VOID;
    return JS_TRUE;
}

/* NOTE: There is a TN version of this below, update it as well */
static inline JSBool
helper_nsICanvasRenderingContextWebGL_UniformMatrix_x_fv(JSContext *cx, uintN argc, jsval *vp, int nElements)
{
    XPC_QS_ASSERT_CONTEXT_OK(cx);
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return JS_FALSE;

    nsresult rv;

    nsICanvasRenderingContextWebGL *self;
    xpc_qsSelfRef selfref;
    js::AutoValueRooter tvr(cx);
    if (!xpc_qsUnwrapThis(cx, obj, nsnull, &self, &selfref.ptr, tvr.jsval_addr(), nsnull))
        return JS_FALSE;

    if (argc < 3)
        return xpc_qsThrow(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);

    jsval *argv = JS_ARGV(cx, vp);

    nsIWebGLUniformLocation *location;
    xpc_qsSelfRef location_selfref;
    rv = xpc_qsUnwrapArg(cx, argv[0], &location, &location_selfref.ptr, &argv[0]);
    if (NS_FAILED(rv)) {
        xpc_qsThrowBadArg(cx, rv, vp, 0);
        return JS_FALSE;
    }

    int32 transpose;
    if (!JS_ValueToECMAInt32(cx, argv[1], &transpose))
        return JS_FALSE;

    if (JSVAL_IS_PRIMITIVE(argv[2])) {
        xpc_qsThrowBadArg(cx, NS_ERROR_FAILURE, vp, 2);
        return JS_FALSE;
    }

    JSObject *arg2 = JSVAL_TO_OBJECT(argv[2]);

    js::AutoValueRooter obj_tvr(cx);

    js::TypedArray *wa = 0;

    if (helper_isFloat32Array(arg2)) {
        wa = js::TypedArray::fromJSObject(arg2);
    }  else if (JS_IsArrayObject(cx, arg2)) {
        JSObject *nobj = js_CreateTypedArrayWithArray(cx, js::TypedArray::TYPE_FLOAT32, arg2);
        if (!nobj) {
            // XXX this will likely return a strange error message if it goes wrong
            return JS_FALSE;
        }

        *obj_tvr.jsval_addr() = OBJECT_TO_JSVAL(nobj);
        wa = js::TypedArray::fromJSObject(nobj);
    } else {
        xpc_qsThrowBadArg(cx, NS_ERROR_FAILURE, vp, 2);
        return JS_FALSE;
    }

    if (nElements == 2) {
        rv = self->UniformMatrix2fv_array(location, transpose ? 1 : 0, wa);
    } else if (nElements == 3) {
        rv = self->UniformMatrix3fv_array(location, transpose ? 1 : 0, wa);
    } else if (nElements == 4) {
        rv = self->UniformMatrix4fv_array(location, transpose ? 1 : 0, wa);
    }

    if (NS_FAILED(rv))
        return xpc_qsThrowMethodFailed(cx, rv, vp);

    *vp = JSVAL_VOID;
    return JS_TRUE;
}

static inline JSBool
helper_nsICanvasRenderingContextWebGL_VertexAttrib_x_fv(JSContext *cx, uintN argc, jsval *vp, int nElements)
{
    XPC_QS_ASSERT_CONTEXT_OK(cx);
    JSObject *obj = JS_THIS_OBJECT(cx, vp);
    if (!obj)
        return JS_FALSE;

    nsresult rv;

    nsICanvasRenderingContextWebGL *self;
    xpc_qsSelfRef selfref;
    js::AutoValueRooter tvr(cx);
    if (!xpc_qsUnwrapThis(cx, obj, nsnull, &self, &selfref.ptr, tvr.jsval_addr(), nsnull))
        return JS_FALSE;

    if (argc < 2)
        return xpc_qsThrow(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);

    jsval *argv = JS_ARGV(cx, vp);

    uint32 location;
    if (!JS_ValueToECMAUint32(cx, argv[0], &location))
        return JS_FALSE;

    if (JSVAL_IS_PRIMITIVE(argv[1])) {
        xpc_qsThrowBadArg(cx, NS_ERROR_FAILURE, vp, 1);
        return JS_FALSE;
    }

    JSObject *arg1 = JSVAL_TO_OBJECT(argv[1]);

    js::AutoValueRooter obj_tvr(cx);

    js::TypedArray *wa = 0;

    if (helper_isFloat32Array(arg1)) {
        wa = js::TypedArray::fromJSObject(arg1);
    }  else if (JS_IsArrayObject(cx, arg1)) {
        JSObject *nobj = js_CreateTypedArrayWithArray(cx, js::TypedArray::TYPE_FLOAT32, arg1);
        if (!nobj) {
            // XXX this will likely return a strange error message if it goes wrong
            return JS_FALSE;
        }

        *obj_tvr.jsval_addr() = OBJECT_TO_JSVAL(nobj);
        wa = js::TypedArray::fromJSObject(nobj);
    } else {
        xpc_qsThrowBadArg(cx, NS_ERROR_FAILURE, vp, 1);
        return JS_FALSE;
    }

    if (nElements == 1) {
        rv = self->VertexAttrib1fv_array(location, wa);
    } else if (nElements == 2) {
        rv = self->VertexAttrib2fv_array(location, wa);
    } else if (nElements == 3) {
        rv = self->VertexAttrib3fv_array(location, wa);
    } else if (nElements == 4) {
        rv = self->VertexAttrib4fv_array(location, wa);
    }

    if (NS_FAILED(rv))
        return xpc_qsThrowMethodFailed(cx, rv, vp);

    *vp = JSVAL_VOID;
    return JS_TRUE;
}

static JSBool
nsICanvasRenderingContextWebGL_Uniform1iv(JSContext *cx, uintN argc, jsval *vp)
{
    return helper_nsICanvasRenderingContextWebGL_Uniform_x_iv(cx, argc, vp, 1);
}

static JSBool
nsICanvasRenderingContextWebGL_Uniform2iv(JSContext *cx, uintN argc, jsval *vp)
{
    return helper_nsICanvasRenderingContextWebGL_Uniform_x_iv(cx, argc, vp, 2);
}

static JSBool
nsICanvasRenderingContextWebGL_Uniform3iv(JSContext *cx, uintN argc, jsval *vp)
{
    return helper_nsICanvasRenderingContextWebGL_Uniform_x_iv(cx, argc, vp, 3);
}

static JSBool
nsICanvasRenderingContextWebGL_Uniform4iv(JSContext *cx, uintN argc, jsval *vp)
{
    return helper_nsICanvasRenderingContextWebGL_Uniform_x_iv(cx, argc, vp, 4);
}

static JSBool
nsICanvasRenderingContextWebGL_Uniform1fv(JSContext *cx, uintN argc, jsval *vp)
{
    return helper_nsICanvasRenderingContextWebGL_Uniform_x_fv(cx, argc, vp, 1);
}

static JSBool
nsICanvasRenderingContextWebGL_Uniform2fv(JSContext *cx, uintN argc, jsval *vp)
{
    return helper_nsICanvasRenderingContextWebGL_Uniform_x_fv(cx, argc, vp, 2);
}

static JSBool
nsICanvasRenderingContextWebGL_Uniform3fv(JSContext *cx, uintN argc, jsval *vp)
{
    return helper_nsICanvasRenderingContextWebGL_Uniform_x_fv(cx, argc, vp, 3);
}

static JSBool
nsICanvasRenderingContextWebGL_Uniform4fv(JSContext *cx, uintN argc, jsval *vp)
{
    return helper_nsICanvasRenderingContextWebGL_Uniform_x_fv(cx, argc, vp, 4);
}

static JSBool
nsICanvasRenderingContextWebGL_UniformMatrix2fv(JSContext *cx, uintN argc, jsval *vp)
{
    return helper_nsICanvasRenderingContextWebGL_UniformMatrix_x_fv(cx, argc, vp, 2);
}

static JSBool
nsICanvasRenderingContextWebGL_UniformMatrix3fv(JSContext *cx, uintN argc, jsval *vp)
{
    return helper_nsICanvasRenderingContextWebGL_UniformMatrix_x_fv(cx, argc, vp, 3);
}

static JSBool
nsICanvasRenderingContextWebGL_UniformMatrix4fv(JSContext *cx, uintN argc, jsval *vp)
{
    return helper_nsICanvasRenderingContextWebGL_UniformMatrix_x_fv(cx, argc, vp, 4);
}

static JSBool
nsICanvasRenderingContextWebGL_VertexAttrib1fv(JSContext *cx, uintN argc, jsval *vp)
{
    return helper_nsICanvasRenderingContextWebGL_VertexAttrib_x_fv(cx, argc, vp, 1);
}

static JSBool
nsICanvasRenderingContextWebGL_VertexAttrib2fv(JSContext *cx, uintN argc, jsval *vp)
{
    return helper_nsICanvasRenderingContextWebGL_VertexAttrib_x_fv(cx, argc, vp, 2);
}

static JSBool
nsICanvasRenderingContextWebGL_VertexAttrib3fv(JSContext *cx, uintN argc, jsval *vp)
{
    return helper_nsICanvasRenderingContextWebGL_VertexAttrib_x_fv(cx, argc, vp, 3);
}

static JSBool
nsICanvasRenderingContextWebGL_VertexAttrib4fv(JSContext *cx, uintN argc, jsval *vp)
{
    return helper_nsICanvasRenderingContextWebGL_VertexAttrib_x_fv(cx, argc, vp, 4);
}

#ifdef JS_TRACER

static inline void FASTCALL
helper_nsICanvasRenderingContextWebGL_Uniform_x_iv_tn(JSContext *cx, JSObject *obj, JSObject *locationobj,
                                                      JSObject *arg, int nElements)
{
    XPC_QS_ASSERT_CONTEXT_OK(cx);

    nsICanvasRenderingContextWebGL *self;
    xpc_qsSelfRef selfref;
    xpc_qsArgValArray<3> vp(cx);
    if (!xpc_qsUnwrapThis(cx, obj, nsnull, &self, &selfref.ptr, &vp.array[0], nsnull)) {
        js_SetTraceableNativeFailed(cx);
        return;
    }

    if (!arg) {
        xpc_qsThrowMethodFailedWithDetails(cx, NS_ERROR_FAILURE, "nsICanvasRenderingContextWebGL", "uniformNiv");
        js_SetTraceableNativeFailed(cx);
    }

    js::AutoValueRooter obj_tvr(cx);

    nsIWebGLUniformLocation *location;
    xpc_qsSelfRef location_selfref;
    nsresult rv_convert_arg0
        = xpc_qsUnwrapThis(cx, locationobj, nsnull, &location, &location_selfref.ptr, &vp.array[1], nsnull);
    if (NS_FAILED(rv_convert_arg0)) {
        js_SetTraceableNativeFailed(cx);
        return;
    }

    js::TypedArray *wa = 0;

    if (helper_isInt32Array(arg)) {
        wa = js::TypedArray::fromJSObject(arg);
    }  else if (JS_IsArrayObject(cx, arg)) {
        JSObject *nobj = js_CreateTypedArrayWithArray(cx, js::TypedArray::TYPE_INT32, arg);
        if (!nobj) {
            // XXX this will likely return a strange error message if it goes wrong
            js_SetTraceableNativeFailed(cx);
            return;
        }

        *obj_tvr.jsval_addr() = OBJECT_TO_JSVAL(nobj);
        wa = js::TypedArray::fromJSObject(nobj);
    } else {
        xpc_qsThrowMethodFailedWithDetails(cx, NS_ERROR_FAILURE, "nsICanvasRenderingContextWebGL", "uniformNiv");
        js_SetTraceableNativeFailed(cx);
        return;
    }

    nsresult rv;

    if (nElements == 1) {
        rv = self->Uniform1iv_array(location, wa);
    } else if (nElements == 2) {
        rv = self->Uniform2iv_array(location, wa);
    } else if (nElements == 3) {
        rv = self->Uniform3iv_array(location, wa);
    } else if (nElements == 4) {
        rv = self->Uniform4iv_array(location, wa);
    }

    if (NS_FAILED(rv)) {
        xpc_qsThrowMethodFailedWithDetails(cx, rv, "nsICanvasRenderingContextWebGL", "uniformNiv");
        js_SetTraceableNativeFailed(cx);
    }
}

static inline void FASTCALL
helper_nsICanvasRenderingContextWebGL_Uniform_x_fv_tn(JSContext *cx, JSObject *obj, JSObject *locationobj,
                                                      JSObject *arg, int nElements)
{
    XPC_QS_ASSERT_CONTEXT_OK(cx);

    nsICanvasRenderingContextWebGL *self;
    xpc_qsSelfRef selfref;
    xpc_qsArgValArray<3> vp(cx);
    if (!xpc_qsUnwrapThis(cx, obj, nsnull, &self, &selfref.ptr, &vp.array[0], nsnull)) {
        js_SetTraceableNativeFailed(cx);
        return;
    }

    if (!arg) {
        xpc_qsThrowMethodFailedWithDetails(cx, NS_ERROR_FAILURE, "nsICanvasRenderingContextWebGL", "uniformNfv");
        js_SetTraceableNativeFailed(cx);
    }

    js::AutoValueRooter obj_tvr(cx);

    nsIWebGLUniformLocation *location;
    xpc_qsSelfRef location_selfref;
    nsresult rv_convert_arg0
        = xpc_qsUnwrapThis(cx, locationobj, nsnull, &location, &location_selfref.ptr, &vp.array[1], nsnull);
    if (NS_FAILED(rv_convert_arg0)) {
        js_SetTraceableNativeFailed(cx);
        return;
    }

    js::TypedArray *wa = 0;

    if (helper_isFloat32Array(arg)) {
        wa = js::TypedArray::fromJSObject(arg);
    }  else if (JS_IsArrayObject(cx, arg)) {
        JSObject *nobj = js_CreateTypedArrayWithArray(cx, js::TypedArray::TYPE_FLOAT32, arg);
        if (!nobj) {
            // XXX this will likely return a strange error message if it goes wrong
            js_SetTraceableNativeFailed(cx);
            return;
        }

        *obj_tvr.jsval_addr() = OBJECT_TO_JSVAL(nobj);
        wa = js::TypedArray::fromJSObject(nobj);
    } else {
        xpc_qsThrowMethodFailedWithDetails(cx, NS_ERROR_FAILURE, "nsICanvasRenderingContextWebGL", "uniformNfv");
        js_SetTraceableNativeFailed(cx);
        return;
    }

    nsresult rv;

    if (nElements == 1) {
        rv = self->Uniform1fv_array(location, wa);
    } else if (nElements == 2) {
        rv = self->Uniform2fv_array(location, wa);
    } else if (nElements == 3) {
        rv = self->Uniform3fv_array(location, wa);
    } else if (nElements == 4) {
        rv = self->Uniform4fv_array(location, wa);
    }

    if (NS_FAILED(rv)) {
        xpc_qsThrowMethodFailedWithDetails(cx, rv, "nsICanvasRenderingContextWebGL", "uniformNfv");
        js_SetTraceableNativeFailed(cx);
    }

    return;
}

static inline void FASTCALL
helper_nsICanvasRenderingContextWebGL_UniformMatrix_x_fv_tn(JSContext *cx, JSObject *obj, JSObject *locationobj,
                                                            JSBool transpose, JSObject *arg, int nElements)
{
    XPC_QS_ASSERT_CONTEXT_OK(cx);

    nsICanvasRenderingContextWebGL *self;
    xpc_qsSelfRef selfref;
    xpc_qsArgValArray<4> vp(cx);
    if (!xpc_qsUnwrapThis(cx, obj, nsnull, &self, &selfref.ptr, &vp.array[0], nsnull)) {
        js_SetTraceableNativeFailed(cx);
        return;
    }

    if (!arg) {
        xpc_qsThrowMethodFailedWithDetails(cx, NS_ERROR_FAILURE, "nsICanvasRenderingContextWebGL", "uniformMatrixNfv");
        js_SetTraceableNativeFailed(cx);
    }

    js::AutoValueRooter obj_tvr(cx);

    nsIWebGLUniformLocation *location;
    xpc_qsSelfRef location_selfref;
    nsresult rv_convert_arg0
        = xpc_qsUnwrapThis(cx, locationobj, nsnull, &location, &location_selfref.ptr, &vp.array[1], nsnull);
    if (NS_FAILED(rv_convert_arg0)) {
        js_SetTraceableNativeFailed(cx);
        return;
    }

    js::TypedArray *wa = 0;

    if (helper_isFloat32Array(arg)) {
        wa = js::TypedArray::fromJSObject(arg);
    }  else if (JS_IsArrayObject(cx, arg)) {
        JSObject *nobj = js_CreateTypedArrayWithArray(cx, js::TypedArray::TYPE_FLOAT32, arg);
        if (!nobj) {
            // XXX this will likely return a strange error message if it goes wrong
            js_SetTraceableNativeFailed(cx);
            return;
        }

        *obj_tvr.jsval_addr() = OBJECT_TO_JSVAL(nobj);
        wa = js::TypedArray::fromJSObject(nobj);
    } else {
        xpc_qsThrowMethodFailedWithDetails(cx, NS_ERROR_FAILURE, "nsICanvasRenderingContextWebGL", "uniformMatrixNfv");
        js_SetTraceableNativeFailed(cx);
        return;
    }

    nsresult rv;
    if (nElements == 2) {
        rv = self->UniformMatrix2fv_array(location, transpose, wa);
    } else if (nElements == 3) {
        rv = self->UniformMatrix3fv_array(location, transpose, wa);
    } else if (nElements == 4) {
        rv = self->UniformMatrix4fv_array(location, transpose, wa);
    }

    if (NS_FAILED(rv)) {
        xpc_qsThrowMethodFailedWithDetails(cx, rv, "nsICanvasRenderingContextWebGL", "uniformMatrixNfv");
        js_SetTraceableNativeFailed(cx);
    }
}

// FIXME This should return void, not uint32
//       (waiting for https://bugzilla.mozilla.org/show_bug.cgi?id=572798)
static uint32 FASTCALL
nsICanvasRenderingContextWebGL_Uniform1iv_tn(JSContext *cx, JSObject *obj, JSObject *location, JSObject *arg)
{
    helper_nsICanvasRenderingContextWebGL_Uniform_x_iv_tn(cx, obj, location, arg, 1);
    return 0;
}

JS_DEFINE_TRCINFO_1(nsICanvasRenderingContextWebGL_Uniform1iv,
    (4, (static, UINT32_FAIL, nsICanvasRenderingContextWebGL_Uniform1iv_tn, CONTEXT, THIS, OBJECT, OBJECT, 0, nanojit::ACCSET_STORE_ANY)))

// FIXME This should return void, not uint32
//       (waiting for https://bugzilla.mozilla.org/show_bug.cgi?id=572798)
static uint32 FASTCALL
nsICanvasRenderingContextWebGL_Uniform2iv_tn(JSContext *cx, JSObject *obj, JSObject *location, JSObject *arg)
{
    helper_nsICanvasRenderingContextWebGL_Uniform_x_iv_tn(cx, obj, location, arg, 2);
    return 0;
}

JS_DEFINE_TRCINFO_1(nsICanvasRenderingContextWebGL_Uniform2iv,
    (4, (static, UINT32_FAIL, nsICanvasRenderingContextWebGL_Uniform2iv_tn, CONTEXT, THIS, OBJECT, OBJECT, 0, nanojit::ACCSET_STORE_ANY)))

// FIXME This should return void, not uint32
//       (waiting for https://bugzilla.mozilla.org/show_bug.cgi?id=572798)
static uint32 FASTCALL
nsICanvasRenderingContextWebGL_Uniform3iv_tn(JSContext *cx, JSObject *obj, JSObject *location, JSObject *arg)
{
    helper_nsICanvasRenderingContextWebGL_Uniform_x_iv_tn(cx, obj, location, arg, 3);
    return 0;
}

JS_DEFINE_TRCINFO_1(nsICanvasRenderingContextWebGL_Uniform3iv,
    (4, (static, UINT32_FAIL, nsICanvasRenderingContextWebGL_Uniform3iv_tn, CONTEXT, THIS, OBJECT, OBJECT, 0, nanojit::ACCSET_STORE_ANY)))

// FIXME This should return void, not uint32
//       (waiting for https://bugzilla.mozilla.org/show_bug.cgi?id=572798)
static uint32 FASTCALL
nsICanvasRenderingContextWebGL_Uniform4iv_tn(JSContext *cx, JSObject *obj, JSObject *location, JSObject *arg)
{
    helper_nsICanvasRenderingContextWebGL_Uniform_x_iv_tn(cx, obj, location, arg, 4);
    return 0;
}

JS_DEFINE_TRCINFO_1(nsICanvasRenderingContextWebGL_Uniform4iv,
    (4, (static, UINT32_FAIL, nsICanvasRenderingContextWebGL_Uniform4iv_tn, CONTEXT, THIS, OBJECT, OBJECT, 0, nanojit::ACCSET_STORE_ANY)))

// FIXME This should return void, not uint32
//       (waiting for https://bugzilla.mozilla.org/show_bug.cgi?id=572798)
static uint32 FASTCALL
nsICanvasRenderingContextWebGL_Uniform1fv_tn(JSContext *cx, JSObject *obj, JSObject *location, JSObject *arg)
{
    helper_nsICanvasRenderingContextWebGL_Uniform_x_fv_tn(cx, obj, location, arg, 1);
    return 0;
}

JS_DEFINE_TRCINFO_1(nsICanvasRenderingContextWebGL_Uniform1fv,
    (4, (static, UINT32_FAIL, nsICanvasRenderingContextWebGL_Uniform1fv_tn, CONTEXT, THIS, OBJECT, OBJECT, 0, nanojit::ACCSET_STORE_ANY)))

// FIXME This should return void, not uint32
//       (waiting for https://bugzilla.mozilla.org/show_bug.cgi?id=572798)
static uint32 FASTCALL
nsICanvasRenderingContextWebGL_Uniform2fv_tn(JSContext *cx, JSObject *obj, JSObject *location, JSObject *arg)
{
    helper_nsICanvasRenderingContextWebGL_Uniform_x_fv_tn(cx, obj, location, arg, 2);
    return 0;
}

JS_DEFINE_TRCINFO_1(nsICanvasRenderingContextWebGL_Uniform2fv,
    (4, (static, UINT32_FAIL, nsICanvasRenderingContextWebGL_Uniform2fv_tn, CONTEXT, THIS, OBJECT, OBJECT, 0, nanojit::ACCSET_STORE_ANY)))

// FIXME This should return void, not uint32
//       (waiting for https://bugzilla.mozilla.org/show_bug.cgi?id=572798)
static uint32 FASTCALL
nsICanvasRenderingContextWebGL_Uniform3fv_tn(JSContext *cx, JSObject *obj, JSObject *location, JSObject *arg)
{
    helper_nsICanvasRenderingContextWebGL_Uniform_x_fv_tn(cx, obj, location, arg, 3);
    return 0;
}

JS_DEFINE_TRCINFO_1(nsICanvasRenderingContextWebGL_Uniform3fv,
    (4, (static, UINT32_FAIL, nsICanvasRenderingContextWebGL_Uniform3fv_tn, CONTEXT, THIS, OBJECT, OBJECT, 0, nanojit::ACCSET_STORE_ANY)))

// FIXME This should return void, not uint32
//       (waiting for https://bugzilla.mozilla.org/show_bug.cgi?id=572798)
static uint32 FASTCALL
nsICanvasRenderingContextWebGL_Uniform4fv_tn(JSContext *cx, JSObject *obj, JSObject *location, JSObject *arg)
{
    helper_nsICanvasRenderingContextWebGL_Uniform_x_fv_tn(cx, obj, location, arg, 4);
    return 0;
}

JS_DEFINE_TRCINFO_1(nsICanvasRenderingContextWebGL_Uniform4fv,
    (4, (static, UINT32_FAIL, nsICanvasRenderingContextWebGL_Uniform4fv_tn, CONTEXT, THIS, OBJECT, OBJECT, 0, nanojit::ACCSET_STORE_ANY)))

// FIXME This should return void, not uint32
//       (waiting for https://bugzilla.mozilla.org/show_bug.cgi?id=572798)
static uint32 FASTCALL
nsICanvasRenderingContextWebGL_UniformMatrix2fv_tn(JSContext *cx, JSObject *obj, JSObject *loc, JSBool transpose, JSObject *arg)
{
    helper_nsICanvasRenderingContextWebGL_UniformMatrix_x_fv_tn(cx, obj, loc, transpose, arg, 2);
    return 0;
}

JS_DEFINE_TRCINFO_1(nsICanvasRenderingContextWebGL_UniformMatrix2fv,
    (5, (static, UINT32_FAIL, nsICanvasRenderingContextWebGL_UniformMatrix2fv_tn, CONTEXT, THIS, OBJECT, BOOL, OBJECT, 0, nanojit::ACCSET_STORE_ANY)))

// FIXME This should return void, not uint32
//       (waiting for https://bugzilla.mozilla.org/show_bug.cgi?id=572798)
static uint32 FASTCALL
nsICanvasRenderingContextWebGL_UniformMatrix3fv_tn(JSContext *cx, JSObject *obj, JSObject *loc, JSBool transpose, JSObject *arg)
{
    helper_nsICanvasRenderingContextWebGL_UniformMatrix_x_fv_tn(cx, obj, loc, transpose, arg, 3);
    return 0;
}

JS_DEFINE_TRCINFO_1(nsICanvasRenderingContextWebGL_UniformMatrix3fv,
    (5, (static, UINT32_FAIL, nsICanvasRenderingContextWebGL_UniformMatrix3fv_tn, CONTEXT, THIS, OBJECT, BOOL, OBJECT, 0, nanojit::ACCSET_STORE_ANY)))

// FIXME This should return void, not uint32
//       (waiting for https://bugzilla.mozilla.org/show_bug.cgi?id=572798)
static uint32 FASTCALL
nsICanvasRenderingContextWebGL_UniformMatrix4fv_tn(JSContext *cx, JSObject *obj, JSObject *loc, JSBool transpose, JSObject *arg)
{
    helper_nsICanvasRenderingContextWebGL_UniformMatrix_x_fv_tn(cx, obj, loc, transpose, arg, 4);
    return 0;
}

JS_DEFINE_TRCINFO_1(nsICanvasRenderingContextWebGL_UniformMatrix4fv,
    (5, (static, UINT32_FAIL, nsICanvasRenderingContextWebGL_UniformMatrix4fv_tn, CONTEXT, THIS, OBJECT, BOOL, OBJECT, 0, nanojit::ACCSET_STORE_ANY)))

#endif /* JS_TRACER */

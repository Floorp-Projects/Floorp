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
    JSAutoTempValueRooter tvr(cx);
    if (!xpc_qsUnwrapThis(cx, obj, nsnull, &self, &selfref.ptr, tvr.addr(), nsnull))
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

    if (JSVAL_IS_OBJECT(argv[1])) {
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
    JSAutoTempValueRooter tvr(cx);
    if (!xpc_qsUnwrapThis(cx, obj, nsnull, &self, &selfref.ptr, tvr.addr(), nsnull))
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

    if (!JSVAL_IS_OBJECT(argv[2])) {
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
 * TexImage2D takes:
 *    TexImage2D(int, int, int, int, int, int, int, int, WebGLArray)
 *    TexImage2D(int, int, int, int, int, int, int, int, WebGLArrayBuffer)
 *    TexImage2D(int, int, nsIDOMElement, [bool[, bool]])
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
    JSAutoTempValueRooter tvr(cx);
    if (!xpc_qsUnwrapThis(cx, obj, nsnull, &self, &selfref.ptr, tvr.addr(), nsnull))
        return JS_FALSE;

    if (argc < 3)
        return xpc_qsThrow(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);

    jsval *argv = JS_ARGV(cx, vp);

    int32 intargs[8];

    // convert the first two args, they must be ints
    for (jsuint i = 0; i < 2; ++i) {
        if (!JS_ValueToECMAInt32(cx, argv[i], &intargs[i]))
            return JS_FALSE;
    }

    if (JSVAL_IS_OBJECT(argv[2])) {
        // try to make this a nsIDOMElement
        nsIDOMElement *elt;
        xpc_qsSelfRef eltRef;
        rv = xpc_qsUnwrapArg<nsIDOMElement>(cx, argv[2], &elt, &eltRef.ptr, &argv[2]);
        if (NS_SUCCEEDED(rv)) {
            intargs[3] = 0;
            intargs[4] = 0;

            // convert args 4 and 5 if present, default to 0
            for (jsuint i = 3; i < 5 && i < argc; ++i) {
                if (!JS_ValueToECMAInt32(cx, argv[i], &intargs[i]))
                    return JS_FALSE;
            }

            rv = self->TexImage2D_dom(intargs[0], intargs[1], elt, (GLboolean) intargs[3], (GLboolean) intargs[4]);
            if (NS_FAILED(rv))
                return xpc_qsThrowMethodFailed(cx, rv, vp);
            return JS_TRUE;
        }
    }

    // didn't succeed? convert the rest of the int args
    for (jsuint i = 2; i < 8; ++i) {
        if (!JS_ValueToECMAInt32(cx, argv[i], &intargs[i]))
            return JS_FALSE;
    }

    if (!JSVAL_IS_OBJECT(argv[8])) {
        xpc_qsThrowBadArg(cx, NS_ERROR_FAILURE, vp, 8);
        return JS_FALSE;
    }

    // then try to grab either a js::ArrayBuffer or js::TypedArray
    js::TypedArray *wa = 0;
    js::ArrayBuffer *wb = 0;

    JSObject *arg9 = JSVAL_TO_OBJECT(argv[8]);
    if (js_IsArrayBuffer(arg9)) {
        wb = js::ArrayBuffer::fromJSObject(arg9);
    } else if (js_IsTypedArray(arg9)) {
        wa = js::TypedArray::fromJSObject(arg9);
    } else {
        xpc_qsThrowBadArg(cx, NS_ERROR_FAILURE, vp, 8);
        return JS_FALSE;
    }

    if (wa) {
        rv = self->TexImage2D_array(intargs[0], intargs[1], intargs[2], intargs[3],
                                    intargs[4], intargs[5], intargs[6], intargs[7],
                                    wa);
    } else if (wb) {
        rv = self->TexImage2D_buf(intargs[0], intargs[1], intargs[2], intargs[3],
                                  intargs[4], intargs[5], intargs[6], intargs[7],
                                  wb);
    } else {
        xpc_qsThrowBadArg(cx, NS_ERROR_FAILURE, vp, 8);
        return JS_FALSE;
    }

    if (NS_FAILED(rv))
        return xpc_qsThrowMethodFailed(cx, rv, vp);

    *vp = JSVAL_VOID;
    return JS_TRUE;
}

/*
 * TexSubImage2D takes:
 *    TexSubImage2D(int, int, int, int, int, int, int, int, WebGLArray)
 *    TexSubImage2D(int, int, int, int, int, int, int, int, WebGLArrayBuffer)
 *    TexSubImage2D(int, int, int, int, int, int, nsIDOMElement, [bool[, bool]])
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
    JSAutoTempValueRooter tvr(cx);
    if (!xpc_qsUnwrapThis(cx, obj, nsnull, &self, &selfref.ptr, tvr.addr(), nsnull))
        return JS_FALSE;

    if (argc < 7 || (argc > 7 && argc < 9))
        return xpc_qsThrow(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);

    jsval *argv = JS_ARGV(cx, vp);

    int32 intargs[8];

    // convert the first six args, they must be ints
    for (jsuint i = 0; i < 6; ++i) {
        if (!JS_ValueToECMAInt32(cx, argv[i], &intargs[i]))
            return JS_FALSE;
    }

    if (JSVAL_IS_OBJECT(argv[6])) {
        // try to make this a nsIDOMElement
        nsIDOMElement *elt;
        xpc_qsSelfRef eltRef;

        // these are two optinal args, default to 0
        intargs[7] = 0;
        intargs[8] = 0;

        // convert args 7 and 8 if present
        for (jsuint i = 7; i < 9 && i < argc; ++i) {
            if (!JS_ValueToECMAInt32(cx, argv[i], &intargs[i]))
                return JS_FALSE;
        }

        rv = xpc_qsUnwrapArg<nsIDOMElement>(cx, argv[2], &elt, &eltRef.ptr, &argv[2]);
        if (NS_SUCCEEDED(rv)) {
            rv = self->TexSubImage2D_dom(intargs[0], intargs[1], intargs[2],
                                         intargs[2], intargs[4], intargs[5],
                                         elt, (GLboolean) intargs[7], (GLboolean) intargs[8]);
            if (NS_FAILED(rv))
                return xpc_qsThrowMethodFailed(cx, rv, vp);
            return JS_TRUE;
        }
    }

    // didn't succeed? convert the rest of the int args
    for (jsuint i = 6; i < 8; ++i) {
        if (!JS_ValueToECMAInt32(cx, argv[i], &intargs[i]))
            return JS_FALSE;
    }

    if (!JSVAL_IS_OBJECT(argv[8])) {
        xpc_qsThrowBadArg(cx, NS_ERROR_FAILURE, vp, 8);
        return JS_FALSE;
    }

    // then try to grab either a js::ArrayBuffer or js::TypedArray
    js::TypedArray *wa = 0;
    js::ArrayBuffer *wb = 0;

    JSObject *arg9 = JSVAL_TO_OBJECT(argv[8]);
    if (js_IsArrayBuffer(arg9)) {
        wb = js::ArrayBuffer::fromJSObject(arg9);
    } else if (js_IsTypedArray(arg9)) {
        wa = js::TypedArray::fromJSObject(arg9);
    } else {
        xpc_qsThrowBadArg(cx, NS_ERROR_FAILURE, vp, 8);
        return JS_FALSE;
    }

    if (wa) {
        rv = self->TexSubImage2D_array(intargs[0], intargs[1], intargs[2], intargs[3],
                                       intargs[4], intargs[5], intargs[6], intargs[7],
                                       wa);
    } else if (wb) {
        rv = self->TexSubImage2D_buf(intargs[0], intargs[1], intargs[2], intargs[3],
                                     intargs[4], intargs[5], intargs[6], intargs[7],
                                     wb);
    } else {
        xpc_qsThrowBadArg(cx, NS_ERROR_FAILURE, vp, 8);
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
    JSAutoTempValueRooter tvr(cx);
    if (!xpc_qsUnwrapThis(cx, obj, nsnull, &self, &selfref.ptr, tvr.addr(), nsnull))
        return JS_FALSE;

    if (argc < 2)
        return xpc_qsThrow(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);

    jsval *argv = JS_ARGV(cx, vp);

    uint32 location;
    if (!JS_ValueToECMAUint32(cx, argv[0], &location))
        return JS_FALSE;

    if (!JSVAL_IS_OBJECT(argv[1])) {
        xpc_qsThrowBadArg(cx, NS_ERROR_FAILURE, vp, 1);
        return JS_FALSE;
    }

    JSObject *arg1 = JSVAL_TO_OBJECT(argv[1]);

    JSAutoTempValueRooter obj_tvr(cx);

    js::TypedArray *wa = 0;

    if (helper_isInt32Array(arg1)) {
        wa = js::TypedArray::fromJSObject(arg1);
    }  else if (JS_IsArrayObject(cx, arg1)) {
        JSObject *nobj = js_CreateTypedArrayWithArray(cx, js::TypedArray::TYPE_INT32, arg1);
        if (!nobj) {
            // XXX this will likely return a strange error message if it goes wrong
            return JS_FALSE;
        }

        *obj_tvr.addr() = OBJECT_TO_JSVAL(nobj);
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
    JSAutoTempValueRooter tvr(cx);
    if (!xpc_qsUnwrapThis(cx, obj, nsnull, &self, &selfref.ptr, tvr.addr(), nsnull))
        return JS_FALSE;

    if (argc < 2)
        return xpc_qsThrow(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);

    jsval *argv = JS_ARGV(cx, vp);

    uint32 location;
    if (!JS_ValueToECMAUint32(cx, argv[0], &location))
        return JS_FALSE;

    if (!JSVAL_IS_OBJECT(argv[1])) {
        xpc_qsThrowBadArg(cx, NS_ERROR_FAILURE, vp, 1);
        return JS_FALSE;
    }

    JSObject *arg1 = JSVAL_TO_OBJECT(argv[1]);

    JSAutoTempValueRooter obj_tvr(cx);

    js::TypedArray *wa = 0;

    if (helper_isFloat32Array(arg1)) {
        wa = js::TypedArray::fromJSObject(arg1);
    }  else if (JS_IsArrayObject(cx, arg1)) {
        JSObject *nobj = js_CreateTypedArrayWithArray(cx, js::TypedArray::TYPE_FLOAT32, arg1);
        if (!nobj) {
            // XXX this will likely return a strange error message if it goes wrong
            return JS_FALSE;
        }

        *obj_tvr.addr() = OBJECT_TO_JSVAL(nobj);
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
    JSAutoTempValueRooter tvr(cx);
    if (!xpc_qsUnwrapThis(cx, obj, nsnull, &self, &selfref.ptr, tvr.addr(), nsnull))
        return JS_FALSE;

    if (argc < 3)
        return xpc_qsThrow(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);

    jsval *argv = JS_ARGV(cx, vp);

    uint32 location;
    int32 transpose;
    if (!JS_ValueToECMAUint32(cx, argv[0], &location))
        return JS_FALSE;

    if (!JS_ValueToECMAInt32(cx, argv[1], &transpose))
        return JS_FALSE;

    if (!JSVAL_IS_OBJECT(argv[2])) {
        xpc_qsThrowBadArg(cx, NS_ERROR_FAILURE, vp, 2);
        return JS_FALSE;
    }

    JSObject *arg2 = JSVAL_TO_OBJECT(argv[2]);

    JSAutoTempValueRooter obj_tvr(cx);

    js::TypedArray *wa = 0;

    if (helper_isFloat32Array(arg2)) {
        wa = js::TypedArray::fromJSObject(arg2);
    }  else if (JS_IsArrayObject(cx, arg2)) {
        JSObject *nobj = js_CreateTypedArrayWithArray(cx, js::TypedArray::TYPE_FLOAT32, arg2);
        if (!nobj) {
            // XXX this will likely return a strange error message if it goes wrong
            return JS_FALSE;
        }

        *obj_tvr.addr() = OBJECT_TO_JSVAL(nobj);
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
    JSAutoTempValueRooter tvr(cx);
    if (!xpc_qsUnwrapThis(cx, obj, nsnull, &self, &selfref.ptr, tvr.addr(), nsnull))
        return JS_FALSE;

    if (argc < 2)
        return xpc_qsThrow(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);

    jsval *argv = JS_ARGV(cx, vp);

    uint32 location;
    if (!JS_ValueToECMAUint32(cx, argv[0], &location))
        return JS_FALSE;

    if (!JSVAL_IS_OBJECT(argv[1])) {
        xpc_qsThrowBadArg(cx, NS_ERROR_FAILURE, vp, 1);
        return JS_FALSE;
    }

    JSObject *arg1 = JSVAL_TO_OBJECT(argv[1]);

    JSAutoTempValueRooter obj_tvr(cx);

    js::TypedArray *wa = 0;

    if (helper_isFloat32Array(arg1)) {
        wa = js::TypedArray::fromJSObject(arg1);
    }  else if (JS_IsArrayObject(cx, arg1)) {
        JSObject *nobj = js_CreateTypedArrayWithArray(cx, js::TypedArray::TYPE_FLOAT32, arg1);
        if (!nobj) {
            // XXX this will likely return a strange error message if it goes wrong
            return JS_FALSE;
        }

        *obj_tvr.addr() = OBJECT_TO_JSVAL(nobj);
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

static inline jsval FASTCALL
helper_nsICanvasRenderingContextWebGL_Uniform_x_iv_tn(JSContext *cx, JSObject *obj, uint32 location, JSObject *arg, int nElements)
{
    XPC_QS_ASSERT_CONTEXT_OK(cx);

    nsICanvasRenderingContextWebGL *self;
    xpc_qsSelfRef selfref;
    xpc_qsArgValArray<3> vp(cx);
    if (!xpc_qsUnwrapThis(cx, obj, nsnull, &self, &selfref.ptr, &vp.array[0], nsnull)) {
        js_SetTraceableNativeFailed(cx);
        return JSVAL_VOID;
    }

    JSAutoTempValueRooter obj_tvr(cx);

    js::TypedArray *wa = 0;

    if (helper_isInt32Array(arg)) {
        wa = js::TypedArray::fromJSObject(arg);
    }  else if (JS_IsArrayObject(cx, arg)) {
        JSObject *nobj = js_CreateTypedArrayWithArray(cx, js::TypedArray::TYPE_INT32, arg);
        if (!nobj) {
            // XXX this will likely return a strange error message if it goes wrong
            js_SetTraceableNativeFailed(cx);
            return JSVAL_VOID;
        }

        *obj_tvr.addr() = OBJECT_TO_JSVAL(nobj);
        wa = js::TypedArray::fromJSObject(nobj);
    } else {
        xpc_qsThrowMethodFailedWithDetails(cx, NS_ERROR_FAILURE, "nsICanvasRenderingContextWebGL", "uniformNiv");
        js_SetTraceableNativeFailed(cx);
        return JSVAL_VOID;
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

    return JSVAL_VOID;
}

static inline jsval FASTCALL
helper_nsICanvasRenderingContextWebGL_Uniform_x_fv_tn(JSContext *cx, JSObject *obj, uint32 location, JSObject *arg, int nElements)
{
    XPC_QS_ASSERT_CONTEXT_OK(cx);

    nsICanvasRenderingContextWebGL *self;
    xpc_qsSelfRef selfref;
    xpc_qsArgValArray<3> vp(cx);
    if (!xpc_qsUnwrapThis(cx, obj, nsnull, &self, &selfref.ptr, &vp.array[0], nsnull)) {
        js_SetTraceableNativeFailed(cx);
        return JSVAL_VOID;
    }

    JSAutoTempValueRooter obj_tvr(cx);

    js::TypedArray *wa = 0;

    if (helper_isFloat32Array(arg)) {
        wa = js::TypedArray::fromJSObject(arg);
    }  else if (JS_IsArrayObject(cx, arg)) {
        JSObject *nobj = js_CreateTypedArrayWithArray(cx, js::TypedArray::TYPE_FLOAT32, arg);
        if (!nobj) {
            // XXX this will likely return a strange error message if it goes wrong
            js_SetTraceableNativeFailed(cx);
            return JSVAL_VOID;
        }

        *obj_tvr.addr() = OBJECT_TO_JSVAL(nobj);
        wa = js::TypedArray::fromJSObject(nobj);
    } else {
        xpc_qsThrowMethodFailedWithDetails(cx, NS_ERROR_FAILURE, "nsICanvasRenderingContextWebGL", "uniformNfv");
        js_SetTraceableNativeFailed(cx);
        return JSVAL_VOID;
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

    return JSVAL_VOID;
}

static inline jsval FASTCALL
helper_nsICanvasRenderingContextWebGL_UniformMatrix_x_fv_tn(JSContext *cx, JSObject *obj, uint32 location, JSBool transpose, JSObject *arg, int nElements)
{
    XPC_QS_ASSERT_CONTEXT_OK(cx);

    nsICanvasRenderingContextWebGL *self;
    xpc_qsSelfRef selfref;
    xpc_qsArgValArray<4> vp(cx);
    if (!xpc_qsUnwrapThis(cx, obj, nsnull, &self, &selfref.ptr, &vp.array[0], nsnull)) {
        js_SetTraceableNativeFailed(cx);
        return JSVAL_VOID;
    }

    JSAutoTempValueRooter obj_tvr(cx);

    js::TypedArray *wa = 0;

    if (helper_isFloat32Array(arg)) {
        wa = js::TypedArray::fromJSObject(arg);
    }  else if (JS_IsArrayObject(cx, arg)) {
        JSObject *nobj = js_CreateTypedArrayWithArray(cx, js::TypedArray::TYPE_FLOAT32, arg);
        if (!nobj) {
            // XXX this will likely return a strange error message if it goes wrong
            js_SetTraceableNativeFailed(cx);
            return JSVAL_VOID;
        }

        *obj_tvr.addr() = OBJECT_TO_JSVAL(nobj);
        wa = js::TypedArray::fromJSObject(nobj);
    } else {
        xpc_qsThrowMethodFailedWithDetails(cx, NS_ERROR_FAILURE, "nsICanvasRenderingContextWebGL", "uniformMatrixNfv");
        js_SetTraceableNativeFailed(cx);
        return JSVAL_VOID;
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

    return JSVAL_VOID;
}

static jsval FASTCALL
nsICanvasRenderingContextWebGL_Uniform1iv_tn(JSContext *cx, JSObject *obj, uint32 location, JSObject *arg)
{
    return helper_nsICanvasRenderingContextWebGL_Uniform_x_iv_tn(cx, obj, location, arg, 1);
}

JS_DEFINE_TRCINFO_1(nsICanvasRenderingContextWebGL_Uniform1iv,
    (4, (static, JSVAL_FAIL, nsICanvasRenderingContextWebGL_Uniform1iv_tn, CONTEXT, THIS, UINT32, OBJECT, 0, 0)))

static jsval FASTCALL
nsICanvasRenderingContextWebGL_Uniform2iv_tn(JSContext *cx, JSObject *obj, uint32 location, JSObject *arg)
{
    return helper_nsICanvasRenderingContextWebGL_Uniform_x_iv_tn(cx, obj, location, arg, 2);
}

JS_DEFINE_TRCINFO_1(nsICanvasRenderingContextWebGL_Uniform2iv,
    (4, (static, JSVAL_FAIL, nsICanvasRenderingContextWebGL_Uniform2iv_tn, CONTEXT, THIS, UINT32, OBJECT, 0, 0)))

static jsval FASTCALL
nsICanvasRenderingContextWebGL_Uniform3iv_tn(JSContext *cx, JSObject *obj, uint32 location, JSObject *arg)
{
    return helper_nsICanvasRenderingContextWebGL_Uniform_x_iv_tn(cx, obj, location, arg, 3);
}

JS_DEFINE_TRCINFO_1(nsICanvasRenderingContextWebGL_Uniform3iv,
    (4, (static, JSVAL_FAIL, nsICanvasRenderingContextWebGL_Uniform3iv_tn, CONTEXT, THIS, UINT32, OBJECT, 0, 0)))

static jsval FASTCALL
nsICanvasRenderingContextWebGL_Uniform4iv_tn(JSContext *cx, JSObject *obj, uint32 location, JSObject *arg)
{
    return helper_nsICanvasRenderingContextWebGL_Uniform_x_iv_tn(cx, obj, location, arg, 4);
}

JS_DEFINE_TRCINFO_1(nsICanvasRenderingContextWebGL_Uniform4iv,
    (4, (static, JSVAL_FAIL, nsICanvasRenderingContextWebGL_Uniform4iv_tn, CONTEXT, THIS, UINT32, OBJECT, 0, 0)))

static jsval FASTCALL
nsICanvasRenderingContextWebGL_Uniform1fv_tn(JSContext *cx, JSObject *obj, uint32 location, JSObject *arg)
{
    return helper_nsICanvasRenderingContextWebGL_Uniform_x_fv_tn(cx, obj, location, arg, 1);
}

JS_DEFINE_TRCINFO_1(nsICanvasRenderingContextWebGL_Uniform1fv,
    (4, (static, JSVAL_FAIL, nsICanvasRenderingContextWebGL_Uniform1fv_tn, CONTEXT, THIS, UINT32, OBJECT, 0, 0)))

static jsval FASTCALL
nsICanvasRenderingContextWebGL_Uniform2fv_tn(JSContext *cx, JSObject *obj, uint32 location, JSObject *arg)
{
    return helper_nsICanvasRenderingContextWebGL_Uniform_x_fv_tn(cx, obj, location, arg, 2);
}

JS_DEFINE_TRCINFO_1(nsICanvasRenderingContextWebGL_Uniform2fv,
    (4, (static, JSVAL_FAIL, nsICanvasRenderingContextWebGL_Uniform2fv_tn, CONTEXT, THIS, UINT32, OBJECT, 0, 0)))

static jsval FASTCALL
nsICanvasRenderingContextWebGL_Uniform3fv_tn(JSContext *cx, JSObject *obj, uint32 location, JSObject *arg)
{
    return helper_nsICanvasRenderingContextWebGL_Uniform_x_fv_tn(cx, obj, location, arg, 3);
}

JS_DEFINE_TRCINFO_1(nsICanvasRenderingContextWebGL_Uniform3fv,
    (4, (static, JSVAL_FAIL, nsICanvasRenderingContextWebGL_Uniform3fv_tn, CONTEXT, THIS, UINT32, OBJECT, 0, 0)))

static jsval FASTCALL
nsICanvasRenderingContextWebGL_Uniform4fv_tn(JSContext *cx, JSObject *obj, uint32 location, JSObject *arg)
{
    return helper_nsICanvasRenderingContextWebGL_Uniform_x_fv_tn(cx, obj, location, arg, 4);
}

JS_DEFINE_TRCINFO_1(nsICanvasRenderingContextWebGL_Uniform4fv,
    (4, (static, JSVAL_FAIL, nsICanvasRenderingContextWebGL_Uniform4fv_tn, CONTEXT, THIS, UINT32, OBJECT, 0, 0)))

static jsval FASTCALL
nsICanvasRenderingContextWebGL_UniformMatrix2fv_tn(JSContext *cx, JSObject *obj, uint32 loc, JSBool transpose, JSObject *arg)
{
    return helper_nsICanvasRenderingContextWebGL_UniformMatrix_x_fv_tn(cx, obj, loc, transpose, arg, 2);
}

JS_DEFINE_TRCINFO_1(nsICanvasRenderingContextWebGL_UniformMatrix2fv,
    (5, (static, JSVAL_FAIL, nsICanvasRenderingContextWebGL_UniformMatrix2fv_tn, CONTEXT, THIS, UINT32, BOOL, OBJECT, 0, 0)))

static jsval FASTCALL
nsICanvasRenderingContextWebGL_UniformMatrix3fv_tn(JSContext *cx, JSObject *obj, uint32 loc, JSBool transpose, JSObject *arg)
{
    return helper_nsICanvasRenderingContextWebGL_UniformMatrix_x_fv_tn(cx, obj, loc, transpose, arg, 3);
}

JS_DEFINE_TRCINFO_1(nsICanvasRenderingContextWebGL_UniformMatrix3fv,
    (5, (static, JSVAL_FAIL, nsICanvasRenderingContextWebGL_UniformMatrix3fv_tn, CONTEXT, THIS, UINT32, BOOL, OBJECT, 0, 0)))

static jsval FASTCALL
nsICanvasRenderingContextWebGL_UniformMatrix4fv_tn(JSContext *cx, JSObject *obj, uint32 loc, JSBool transpose, JSObject *arg)
{
    return helper_nsICanvasRenderingContextWebGL_UniformMatrix_x_fv_tn(cx, obj, loc, transpose, arg, 4);
}

JS_DEFINE_TRCINFO_1(nsICanvasRenderingContextWebGL_UniformMatrix4fv,
    (5, (static, JSVAL_FAIL, nsICanvasRenderingContextWebGL_UniformMatrix4fv_tn, CONTEXT, THIS, UINT32, BOOL, OBJECT, 0, 0)))

#endif /* JS_TRACER */

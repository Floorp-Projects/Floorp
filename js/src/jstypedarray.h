/* -*- Mode: c++; c-basic-offset: 4; tab-width: 40; indent-tabs-mode: nil -*- */
/* vim: set ts=40 sw=4 et tw=99: */
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
 * The Original Code is Mozilla WebGL impl
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#ifndef jstypedarray_h
#define jstypedarray_h

#include "jsapi.h"
#include "jsvalue.h"

typedef struct JSProperty JSProperty;

namespace js {

/*
 * ArrayBuffer
 *
 * This class holds the underlying raw buffer that the TypedArray
 * subclasses access.  It can be created explicitly and passed to a
 * TypedArray subclass, or can be created implicitly by constructing a
 * TypedArray with a size.
 */
struct JS_FRIEND_API(ArrayBuffer) {
    static Class jsclass;
    static JSPropertySpec jsprops[];

    static JSBool prop_getByteLength(JSContext *cx, JSObject *obj, jsid id, Value *vp);
    static void class_finalize(JSContext *cx, JSObject *obj);

    static JSBool class_constructor(JSContext *cx, uintN argc, Value *vp);

    static bool create(JSContext *cx, uintN argc, Value *argv, Value *rval);

    static ArrayBuffer *fromJSObject(JSObject *obj);

    ArrayBuffer()
        : data(0), byteLength()
    {
    }

    ~ArrayBuffer();

    bool allocateStorage(JSContext *cx, uint32 bytes);
    void freeStorage(JSContext *cx);

    void *offsetData(uint32 offs) {
        return (void*) (((intptr_t)data) + offs);
    }

    void *data;
    uint32 byteLength;
};

/*
 * TypedArray
 *
 * The non-templated base class for the specific typed implementations.
 * This class holds all the member variables that are used by
 * the subclasses.
 */

struct JS_FRIEND_API(TypedArray) {
    enum {
        TYPE_INT8 = 0,
        TYPE_UINT8,
        TYPE_INT16,
        TYPE_UINT16,
        TYPE_INT32,
        TYPE_UINT32,
        TYPE_FLOAT32,
        TYPE_FLOAT64,

        /*
         * Special type that's a uint8, but assignments are clamped to 0 .. 255.
         * Treat the raw data type as a uint8.
         */
        TYPE_UINT8_CLAMPED,

        TYPE_MAX
    };

    // and MUST NOT be used to construct new objects.
    static Class fastClasses[TYPE_MAX];

    // These are the slow/original classes, used
    // fo constructing new objects
    static Class slowClasses[TYPE_MAX];

    static JSPropertySpec jsprops[];

    static TypedArray *fromJSObject(JSObject *obj);

    static JSBool prop_getBuffer(JSContext *cx, JSObject *obj, jsid id, Value *vp);
    static JSBool prop_getByteOffset(JSContext *cx, JSObject *obj, jsid id, Value *vp);
    static JSBool prop_getByteLength(JSContext *cx, JSObject *obj, jsid id, Value *vp);
    static JSBool prop_getLength(JSContext *cx, JSObject *obj, jsid id, Value *vp);

    static JSBool obj_lookupProperty(JSContext *cx, JSObject *obj, jsid id,
                                     JSObject **objp, JSProperty **propp);

    static void obj_trace(JSTracer *trc, JSObject *obj);

    static JSBool obj_getAttributes(JSContext *cx, JSObject *obj, jsid id, uintN *attrsp);

    static JSBool obj_setAttributes(JSContext *cx, JSObject *obj, jsid id, uintN *attrsp);

    static int32 lengthOffset() { return offsetof(TypedArray, length); }
    static int32 dataOffset() { return offsetof(TypedArray, data); }
    static int32 typeOffset() { return offsetof(TypedArray, type); }

  public:
    TypedArray() : buffer(0) { }

    bool isArrayIndex(JSContext *cx, jsid id, jsuint *ip = NULL);
    bool valid() { return buffer != 0; }

    ArrayBuffer *buffer;
    JSObject *bufferJS;
    uint32 byteOffset;
    uint32 byteLength;
    uint32 length;
    uint32 type;

    void *data;
};

} // namespace js

/* Friend API methods */

JS_FRIEND_API(JSObject *)
js_InitTypedArrayClasses(JSContext *cx, JSObject *obj);

JS_FRIEND_API(JSBool)
js_IsTypedArray(JSObject *obj);

JS_FRIEND_API(JSBool)
js_IsArrayBuffer(JSObject *obj);

JS_FRIEND_API(JSObject *)
js_CreateArrayBuffer(JSContext *cx, jsuint nbytes);

/*
 * Create a new typed array of type atype (one of the TypedArray
 * enumerant values above), with nelements elements.
 */
JS_FRIEND_API(JSObject *)
js_CreateTypedArray(JSContext *cx, jsint atype, jsuint nelements);

/*
 * Create a new typed array of type atype (one of the TypedArray
 * enumerant values above), and copy in values from the given JSObject,
 * which must either be a typed array or an array-like object.
 */
JS_FRIEND_API(JSObject *)
js_CreateTypedArrayWithArray(JSContext *cx, jsint atype, JSObject *arrayArg);

/*
 * Create a new typed array of type atype (one of the TypedArray
 * enumerant values above), using a given ArrayBuffer for storage.
 * The byteoffset and length values are optional; if -1 is passed, an
 * offset of 0 and enough elements to use up the remainder of the byte
 * array are used as the default values.
 */
JS_FRIEND_API(JSObject *)
js_CreateTypedArrayWithBuffer(JSContext *cx, jsint atype, JSObject *bufArg,
                              jsint byteoffset, jsint length);

/*
 * Reparent a typed array to a new scope. This should only be used to reparent
 * a typed array that does not share its underlying ArrayBuffer with another
 * typed array to avoid having a parent mismatch with the other typed array and
 * its ArrayBuffer.
 */
JS_FRIEND_API(JSBool)
js_ReparentTypedArrayToScope(JSContext *cx, JSObject *obj, JSObject *scope);

#endif /* jstypedarray_h */

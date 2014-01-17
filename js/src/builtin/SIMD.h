/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_SIMD_h
#define builtin_SIMD_h

#include "jsapi.h"
#include "jsobj.h"
#include "builtin/TypeRepresentation.h"
#include "vm/GlobalObject.h"

/*
 * JS SIMD functions.
 * Spec matching polyfill:
 * https://github.com/johnmccutchan/ecmascript_simd/blob/master/src/ecmascript_simd.js
 */

namespace js {

class SIMDObject : public JSObject
{
  public:
    static const Class class_;
    static JSObject* initClass(JSContext *cx, Handle<GlobalObject *> global);
    static bool toString(JSContext *cx, unsigned int argc, jsval *vp);
};

// These classes exist for use with templates below.

struct Float32x4 {
    typedef float Elem;
    static const int32_t lanes = 4;
    static const X4TypeRepresentation::Type type =
        X4TypeRepresentation::TYPE_FLOAT32;

    static JSObject &GetTypeObject(GlobalObject &global) {
        return global.float32x4TypeObject();
    }
    static Elem toType(Elem a) {
        return a;
    }
    static void toType2(JSContext *cx, JS::Handle<JS::Value> v, Elem *out) {
        *out = v.toNumber();
    }
    static void setReturn(CallArgs &args, float value) {
        args.rval().setDouble(JS::CanonicalizeNaN(value));
    }
};

struct Int32x4 {
    typedef int32_t Elem;
    static const int32_t lanes = 4;
    static const X4TypeRepresentation::Type type =
        X4TypeRepresentation::TYPE_INT32;

    static JSObject &GetTypeObject(GlobalObject &global) {
        return global.int32x4TypeObject();
    }
    static Elem toType(Elem a) {
        return ToInt32(a);
    }
    static void toType2(JSContext *cx, JS::Handle<JS::Value> v, Elem *out) {
        ToInt32(cx,v,out);
    }
    static void setReturn(CallArgs &args, int32_t value) {
        args.rval().setInt32(value);
    }
};

template<typename V>
JSObject *Create(JSContext *cx, typename V::Elem *data);

}  /* namespace js */

JSObject *
js_InitSIMDClass(JSContext *cx, js::HandleObject obj);

#endif /* builtin_SIMD_h */

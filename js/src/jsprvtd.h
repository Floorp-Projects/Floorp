/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsprvtd_h
#define jsprvtd_h

/*
 * JS private typename definitions.
 */

#include "jsapi.h"

/* Scalar typedefs. */
typedef uint8_t     jsbytecode;
typedef uint8_t     jssrcnote;
typedef uintptr_t   jsatomid;

/* String typedefs. */
class JSAtom;

namespace js {

class PropertyName;
class Shape;

typedef JSNative             Native;
typedef JSParallelNative     ParallelNative;
typedef JSThreadSafeNative   ThreadSafeNative;
typedef JSPropertyOp         PropertyOp;
typedef JSStrictPropertyOp   StrictPropertyOp;
typedef JSPropertyDescriptor PropertyDescriptor;

namespace types { struct TypeObject; }

typedef JS::Handle<Shape*>             HandleShape;
typedef JS::Handle<types::TypeObject*> HandleTypeObject;
typedef JS::Handle<JSAtom*>            HandleAtom;
typedef JS::Handle<PropertyName*>      HandlePropertyName;

typedef JS::MutableHandle<Shape*>      MutableHandleShape;
typedef JS::MutableHandle<JSAtom*>     MutableHandleAtom;

typedef JS::Rooted<Shape*>             RootedShape;
typedef JS::Rooted<types::TypeObject*> RootedTypeObject;
typedef JS::Rooted<JSAtom*>            RootedAtom;
typedef JS::Rooted<PropertyName*>      RootedPropertyName;

enum XDRMode {
    XDR_ENCODE,
    XDR_DECODE
};

struct IdValuePair
{
    jsid id;
    Value value;

    IdValuePair() {}
    IdValuePair(jsid idArg)
      : id(idArg), value(UndefinedValue())
    {}
};

} /* namespace js */

/* js::ObjectOps function pointer typedefs. */

/*
 * A generic type for functions mapping an object to another object, or null
 * if an error or exception was thrown on cx.
 */
typedef JSObject *
(* JSObjectOp)(JSContext *cx, JS::Handle<JSObject*> obj);

/* Signature for class initialization ops. */
typedef JSObject *
(* JSClassInitializerOp)(JSContext *cx, JS::HandleObject obj);

/*
 * Hook that creates an iterator object for a given object. Returns the
 * iterator object or null if an error or exception was thrown on cx.
 */
typedef JSObject *
(* JSIteratorOp)(JSContext *cx, JS::HandleObject obj, bool keysonly);


#endif /* jsprvtd_h */

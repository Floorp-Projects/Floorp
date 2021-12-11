/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_Object_h
#define builtin_Object_h

#include "js/Value.h"
#include "vm/NativeObject.h"

namespace js {

class PlainObject;

// Object constructor native. Exposed only so the JIT can know its address.
[[nodiscard]] bool obj_construct(JSContext* cx, unsigned argc, JS::Value* vp);

PlainObject* ObjectCreateImpl(JSContext* cx, HandleObject proto,
                              NewObjectKind newKind = GenericObject);

PlainObject* ObjectCreateWithTemplate(JSContext* cx,
                                      Handle<PlainObject*> templateObj);

// Object methods exposed so they can be installed in the self-hosting global.
[[nodiscard]] bool obj_propertyIsEnumerable(JSContext* cx, unsigned argc,
                                            Value* vp);

[[nodiscard]] bool obj_isPrototypeOf(JSContext* cx, unsigned argc, Value* vp);

[[nodiscard]] bool obj_create(JSContext* cx, unsigned argc, JS::Value* vp);

[[nodiscard]] bool obj_is(JSContext* cx, unsigned argc, JS::Value* vp);

[[nodiscard]] bool obj_toString(JSContext* cx, unsigned argc, JS::Value* vp);

[[nodiscard]] bool obj_setProto(JSContext* cx, unsigned argc, JS::Value* vp);

JSString* ObjectClassToString(JSContext* cx, JSObject* obj);

[[nodiscard]] bool GetOwnPropertyKeys(JSContext* cx, HandleObject obj,
                                      unsigned flags,
                                      JS::MutableHandleValue rval);

// Exposed for SelfHosting.cpp
[[nodiscard]] bool GetOwnPropertyDescriptorToArray(JSContext* cx, unsigned argc,
                                                   JS::Value* vp);

/*
 * Like IdToValue, but convert int jsids to strings. This is used when
 * exposing a jsid to script for Object.getOwnProperty{Names,Symbols}
 * or scriptable proxy traps.
 */
[[nodiscard]] bool IdToStringOrSymbol(JSContext* cx, JS::HandleId id,
                                      JS::MutableHandleValue result);

// Object.prototype.toSource. Function.prototype.toSource and uneval use this.
JSString* ObjectToSource(JSContext* cx, JS::HandleObject obj);

} /* namespace js */

#endif /* builtin_Object_h */

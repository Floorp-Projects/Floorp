/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_Object_h
#define builtin_Object_h

#include "jsapi.h"

namespace JS { class Value; }

namespace js {

extern const JSFunctionSpec object_methods[];
extern const JSPropertySpec object_properties[];
extern const JSFunctionSpec object_static_methods[];

// Object constructor native. Exposed only so the JIT can know its address.
bool
obj_construct(JSContext *cx, unsigned argc, JS::Value *vp);

#if JS_HAS_TOSOURCE
// Object.prototype.toSource. Function.prototype.toSource and uneval use this.
JSString *
ObjectToSource(JSContext *cx, JS::HandleObject obj);
#endif // JS_HAS_TOSOURCE

extern bool
WatchHandler(JSContext *cx, JSObject *obj, jsid id, JS::Value old,
             JS::Value *nvp, void *closure);

} /* namespace js */

#endif /* builtin_Object_h */

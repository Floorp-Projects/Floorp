/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Ways to get various per-Realm objects. All the getters declared in this
 * header operate on the Realm corresponding to the current compartment on the
 * JSContext.
 */

#ifndef js_Realm_h
#define js_Realm_h

#include "jstypes.h"

struct JSContext;
class JSObject;

namespace JS {

extern JS_PUBLIC_API(JSObject*)
GetRealmObjectPrototype(JSContext* cx);

extern JS_PUBLIC_API(JSObject*)
GetRealmFunctionPrototype(JSContext* cx);

extern JS_PUBLIC_API(JSObject*)
GetRealmArrayPrototype(JSContext* cx);

extern JS_PUBLIC_API(JSObject*)
GetRealmErrorPrototype(JSContext* cx);

extern JS_PUBLIC_API(JSObject*)
GetRealmIteratorPrototype(JSContext* cx);

} // namespace JS

#endif // js_Realm_h



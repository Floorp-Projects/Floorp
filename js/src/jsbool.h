/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsbool_h___
#define jsbool_h___
/*
 * JS boolean interface.
 */

#include "jsapi.h"
#include "jsobj.h"

extern JSObject *
js_InitBooleanClass(JSContext *cx, JSObject *obj);

extern JSString *
js_BooleanToString(JSContext *cx, JSBool b);

namespace js {

inline bool
BooleanGetPrimitiveValue(JSContext *cx, JSObject &obj, Value *vp);

} /* namespace js */

extern JSBool
js_ValueToBoolean(const js::Value &v);

#endif /* jsbool_h___ */

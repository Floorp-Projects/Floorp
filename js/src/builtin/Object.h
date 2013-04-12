/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Object_h___
#define Object_h___

#include "jsobj.h"

namespace js {

extern JSFunctionSpec object_methods[];
extern JSFunctionSpec object_static_methods[];

/* Object constructor native. Exposed only so the JIT can know its address. */
extern JSBool
obj_construct(JSContext *cx, unsigned argc, js::Value *vp);

extern JSString *
obj_toStringHelper(JSContext *cx, HandleObject obj);

} /* namespace js */

#endif

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_Object_h
#define builtin_Object_h

#include "jsobj.h"

namespace js {

extern const JSFunctionSpec object_methods[];
extern const JSFunctionSpec object_static_methods[];

/* Object constructor native. Exposed only so the JIT can know its address. */
extern bool
obj_construct(JSContext *cx, unsigned argc, js::Value *vp);

} /* namespace js */

#endif /* builtin_Object_h */

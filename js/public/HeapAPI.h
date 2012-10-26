/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_heap_api_h___
#define js_heap_api_h___

#include "gc/Heap.h"

namespace js {

static inline JSCompartment *
GetGCThingCompartment(void *thing)
{
    JS_ASSERT(thing);
    return reinterpret_cast<gc::Cell *>(thing)->compartment();
}

static inline JSCompartment *
GetObjectCompartment(JSObject *obj)
{
    return GetGCThingCompartment(obj);
}

} /* namespace js */

#endif /* js_heap_api_h___ */

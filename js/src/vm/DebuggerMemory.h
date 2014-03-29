/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_DebuggerMemory_h
#define vm_DebuggerMemory_h

#include "jsapi.h"
#include "jscntxt.h"
#include "jsobj.h"
#include "js/Class.h"
#include "js/Value.h"

namespace js {

class DebuggerMemory : public JSObject {
    enum {
        JSSLOT_DEBUGGER_MEMORY_COUNT
    };

public:

    static bool construct(JSContext *cx, unsigned argc, Value *vp);

    static const Class          class_;
    static const JSPropertySpec properties[];
    static const JSFunctionSpec methods[];
};

} /* namespace js */

#endif /* vm_DebuggerMemory_h */

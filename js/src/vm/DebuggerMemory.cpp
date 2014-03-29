/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/DebuggerMemory.h"

namespace js {

/* static */ bool
DebuggerMemory::construct(JSContext *cx, unsigned argc, Value *vp)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_NO_CONSTRUCTOR,
                         "Debugger.Memory");
    return false;
}

/* static */ const Class DebuggerMemory::class_ = {
    "Memory",
    JSCLASS_HAS_PRIVATE | JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_RESERVED_SLOTS(JSSLOT_DEBUGGER_MEMORY_COUNT),

    JS_PropertyStub,       // addProperty
    JS_DeletePropertyStub, // delProperty
    JS_PropertyStub,       // getProperty
    JS_StrictPropertyStub, // setProperty
    JS_EnumerateStub,      // enumerate
    JS_ResolveStub,        // resolve
    JS_ConvertStub,        // convert

    nullptr, // finalize
    nullptr, // call
    nullptr, // hasInstance
    nullptr, // construct
    nullptr  // trace
};

/* static */ const JSPropertySpec DebuggerMemory::properties[] = {
    JS_PS_END
};

/* static */ const JSFunctionSpec DebuggerMemory::methods[] = {
    JS_FS_END
};

} /* namespace js */

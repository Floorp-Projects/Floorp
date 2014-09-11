/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// OSObject.h - os object for exposing posix system calls in the JS shell

#include "shell/OSObject.h"

#include <stdlib.h>

using namespace JS;

static bool
os_getenv(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() < 1) {
        JS_ReportError(cx, "os.getenv requires 1 argument");
        return false;
    }
    RootedString key(cx, ToString(cx, args[0]));
    if (!key)
        return false;
    JSAutoByteString keyBytes;
    if (!keyBytes.encodeUtf8(cx, key))
        return false;

    if (const char *valueBytes = getenv(keyBytes.ptr())) {
        RootedString value(cx, JS_NewStringCopyZ(cx, valueBytes));
        if (!value)
            return false;
        args.rval().setString(value);
    } else {
        args.rval().setUndefined();
    }
    return true;
}

static const JSFunctionSpec os_functions[] = {
    JS_FS("getenv", os_getenv, 1, 0),
    JS_FS_END
};

bool
js::DefineOS(JSContext *cx, HandleObject global)
{
    RootedObject obj(cx, JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr()));
    return obj &&
           JS_DefineFunctions(cx, obj, os_functions) &&
           JS_DefineProperty(cx, global, "os", obj, 0);
}

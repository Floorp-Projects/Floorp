/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// jsshell.cpp - Utilities for the JS shell

#include "shell/jsshell.h"

#include "jsapi.h"
#include "jsfriendapi.h"

#include "vm/StringBuffer.h"

using namespace JS;

namespace js {
namespace shell {

bool
GenerateInterfaceHelp(JSContext* cx, HandleObject obj, const char* name)
{
    AutoIdVector idv(cx);
    if (!GetPropertyKeys(cx, obj, JSITER_OWNONLY | JSITER_HIDDEN, &idv))
        return false;

    StringBuffer buf(cx);
    if (!buf.append(' '))
        return false;

    for (size_t i = 0; i < idv.length(); i++) {
        RootedValue v(cx);
        RootedId id(cx, idv[i]);
        if (!JS_GetPropertyById(cx, obj, id, &v))
            return false;
        if (!v.isObject())
            continue;
        bool hasHelp = false;
        RootedObject prop(cx, &v.toObject());
        if (!JS_GetProperty(cx, prop, "usage", &v))
            return false;
        if (v.isString())
            hasHelp = true;
        if (!JS_GetProperty(cx, prop, "help", &v))
            return false;
        if (v.isString())
            hasHelp = true;
        if (hasHelp) {
            if (!buf.append(' ') ||
                !buf.append(name, strlen(name)) ||
                !buf.append('.') ||
                !buf.append(JSID_TO_FLAT_STRING(id)))
            {
                return false;
            }
        }
    }

    RootedString s(cx, buf.finishString());
    if (!s || !JS_DefineProperty(cx, obj, "help", s, 0))
        return false;

    if (!buf.append(name, strlen(name)) || !buf.append(" - interface object", 20))
        return false;
    s = buf.finishString();
    if (!s || !JS_DefineProperty(cx, obj, "usage", s, 0))
        return false;

    return true;
}

} // namespace shell
} // namespace js

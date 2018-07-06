/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// jsshell.cpp - Utilities for the JS shell

#include "shell/jsshell.h"

#include "mozilla/Sprintf.h"

#include "jsapi.h"
#include "jsfriendapi.h"

#include "util/StringBuffer.h"

using namespace JS;

namespace js {
namespace shell {

// Generate 'usage' and 'help' properties for the given object.
// JS_DefineFunctionsWithHelp will define individual function objects with both
// of those properties (eg getpid.usage = "getpid()" and getpid.help = "return
// the process id"). This function will generate strings for an "interface
// object", eg os.file, which contains some number of functions.
//
// .usage will be set to "<name> - interface object".
//
// .help will be set to a newline-separated list of functions that have either
// 'help' or 'usage' properties. Functions are described with their usage
// strings, if they have them, else with just their names.
//
bool
GenerateInterfaceHelp(JSContext* cx, HandleObject obj, const char* name)
{
    AutoIdVector idv(cx);
    if (!GetPropertyKeys(cx, obj, JSITER_OWNONLY | JSITER_HIDDEN, &idv))
        return false;

    StringBuffer buf(cx);
    int numEntries = 0;
    for (size_t i = 0; i < idv.length(); i++) {
        RootedId id(cx, idv[i]);
        RootedValue v(cx);
        if (!JS_GetPropertyById(cx, obj, id, &v))
            return false;
        if (!v.isObject())
            continue;
        RootedObject prop(cx, &v.toObject());

        RootedValue usage(cx);
        RootedValue help(cx);
        if (!JS_GetProperty(cx, prop, "usage", &usage))
            return false;
        if (!JS_GetProperty(cx, prop, "help", &help))
            return false;
        if (!usage.isString() && !help.isString())
            continue;

        if (numEntries && !buf.append("\n"))
            return false;
        numEntries++;

        if (!buf.append("  ", 2))
            return false;

        if (!buf.append(usage.isString() ? usage.toString() : JSID_TO_FLAT_STRING(id)))
            return false;
    }

    RootedString s(cx, buf.finishString());
    if (!s || !JS_DefineProperty(cx, obj, "help", s, 0))
        return false;

    buf.clear();
    if (!buf.append(name, strlen(name)) || !buf.append(" - interface object with ", 25))
        return false;
    char cbuf[100];
    SprintfLiteral(cbuf, "%d %s", numEntries, numEntries == 1 ? "entry" : "entries");
    if (!buf.append(cbuf, strlen(cbuf)))
        return false;
    s = buf.finishString();
    if (!s || !JS_DefineProperty(cx, obj, "usage", s, 0))
        return false;

    return true;
}

bool
CreateAlias(JSContext* cx, const char* dstName, JS::HandleObject namespaceObj, const char* srcName)
{
    RootedObject global(cx, JS::GetNonCCWObjectGlobal(namespaceObj));

    RootedValue val(cx);
    if (!JS_GetProperty(cx, namespaceObj, srcName, &val))
        return false;

    if (!val.isObject()) {
        JS_ReportErrorASCII(cx, "attempted to alias nonexistent function");
        return false;
    }
    
    RootedObject function(cx, &val.toObject());
    if (!JS_DefineProperty(cx, global, dstName, function, 0))
        return false;

    return true;
}

} // namespace shell
} // namespace js

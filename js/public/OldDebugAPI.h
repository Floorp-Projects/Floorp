/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_OldDebugAPI_h
#define js_OldDebugAPI_h

/*
 * JS debugger API.
 */

#include "mozilla/NullPtr.h"

#include "jsapi.h"
#include "jsbytecode.h"

#include "js/CallArgs.h"
#include "js/TypeDecls.h"

typedef enum JSTrapStatus {
    JSTRAP_ERROR,
    JSTRAP_CONTINUE,
    JSTRAP_RETURN,
    JSTRAP_THROW,
    JSTRAP_LIMIT
} JSTrapStatus;

/************************************************************************/

extern JS_PUBLIC_API(JSScript *)
JS_GetFunctionScript(JSContext *cx, JS::HandleFunction fun);

/************************************************************************/

extern JS_PUBLIC_API(const char *)
JS_GetScriptFilename(JSScript *script);

extern JS_PUBLIC_API(unsigned)
JS_GetScriptBaseLineNumber(JSContext *cx, JSScript *script);

#endif /* js_OldDebugAPI_h */

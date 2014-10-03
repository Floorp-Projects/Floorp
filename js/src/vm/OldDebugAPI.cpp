/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS debugging API.
 */

#include "js/OldDebugAPI.h"

#include <string.h>

#include "jscntxt.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsprf.h"
#include "jsscript.h"
#include "jsstr.h"
#include "jstypes.h"

#include "frontend/SourceNotes.h"
#include "vm/Debugger.h"
#include "vm/Shape.h"

#include "jsatominlines.h"
#include "jsinferinlines.h"
#include "jsscriptinlines.h"

#include "vm/Debugger-inl.h"
#include "vm/Interpreter-inl.h"
#include "vm/Stack-inl.h"

using namespace js;
using namespace js::gc;

using mozilla::PodZero;

JS_PUBLIC_API(JSScript *)
JS_GetFunctionScript(JSContext *cx, HandleFunction fun)
{
    if (fun->isNative())
        return nullptr;
    if (fun->isInterpretedLazy()) {
        AutoCompartment funCompartment(cx, fun);
        JSScript *script = fun->getOrCreateScript(cx);
        if (!script)
            MOZ_CRASH();
        return script;
    }
    return fun->nonLazyScript();
}

/************************************************************************/

JS_PUBLIC_API(const char *)
JS_GetScriptFilename(JSScript *script)
{
    // This is called from ThreadStackHelper which can be called from another
    // thread or inside a signal hander, so we need to be careful in case a
    // copmacting GC is currently moving things around.
    return script->maybeForwardedFilename();
}

JS_PUBLIC_API(const char16_t *)
JS_GetScriptSourceMap(JSContext *cx, JSScript *script)
{
    ScriptSource *source = script->scriptSource();
    MOZ_ASSERT(source);
    return source->hasSourceMapURL() ? source->sourceMapURL() : nullptr;
}

JS_PUBLIC_API(unsigned)
JS_GetScriptBaseLineNumber(JSContext *cx, JSScript *script)
{
    return script->lineno();
}

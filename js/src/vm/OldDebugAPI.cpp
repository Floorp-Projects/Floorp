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

JSTrapStatus
js::ScriptDebugPrologue(JSContext *cx, AbstractFramePtr frame, jsbytecode *pc)
{
    MOZ_ASSERT_IF(frame.isInterpreterFrame(), frame.asInterpreterFrame() == cx->interpreterFrame());

    RootedValue rval(cx);
    JSTrapStatus status = Debugger::onEnterFrame(cx, frame, &rval);
    switch (status) {
      case JSTRAP_CONTINUE:
        break;
      case JSTRAP_THROW:
        cx->setPendingException(rval);
        break;
      case JSTRAP_ERROR:
        cx->clearPendingException();
        break;
      case JSTRAP_RETURN:
        frame.setReturnValue(rval);
        break;
      default:
        MOZ_CRASH("bad Debugger::onEnterFrame JSTrapStatus value");
    }
    return status;
}

bool
js::ScriptDebugEpilogue(JSContext *cx, AbstractFramePtr frame, jsbytecode *pc, bool okArg)
{
    MOZ_ASSERT_IF(frame.isInterpreterFrame(), frame.asInterpreterFrame() == cx->interpreterFrame());

    bool ok = okArg;

    return Debugger::onLeaveFrame(cx, frame, ok);
}

JSTrapStatus
js::DebugExceptionUnwind(JSContext *cx, AbstractFramePtr frame, jsbytecode *pc)
{
    MOZ_ASSERT(cx->compartment()->debugMode());

    /* Call debugger throw hook if set. */
    RootedValue rval(cx);
    JSTrapStatus status = Debugger::onExceptionUnwind(cx, &rval);

    switch (status) {
      case JSTRAP_ERROR:
        cx->clearPendingException();
        break;

      case JSTRAP_RETURN:
        cx->clearPendingException();
        frame.setReturnValue(rval);
        break;

      case JSTRAP_THROW:
        cx->setPendingException(rval);
        break;

      case JSTRAP_CONTINUE:
        break;

      default:
        MOZ_CRASH("Invalid trap status");
    }

    return status;
}

/************************************************************************/

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
    return script->filename();
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

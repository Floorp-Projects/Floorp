/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Probes_inl_h__
#define Probes_inl_h__

#include "vm/Probes.h"

#include "jscntxt.h"
#include "jsobj.h"
#include "jsscript.h"

#include "vm/Stack-inl.h"

namespace js {

/*
 * Many probe handlers are implemented inline for minimal performance impact,
 * especially important when no backends are enabled.
 */

inline bool
Probes::callTrackingActive(JSContext *cx)
{
#ifdef INCLUDE_MOZILLA_DTRACE
    if (JAVASCRIPT_FUNCTION_ENTRY_ENABLED() || JAVASCRIPT_FUNCTION_RETURN_ENABLED())
        return true;
#endif
#ifdef MOZ_TRACE_JSCALLS
    if (cx->functionCallback)
        return true;
#endif
    return false;
}

inline bool
Probes::wantNativeAddressInfo(JSContext *cx)
{
    return (cx->reportGranularity >= JITREPORT_GRANULARITY_FUNCTION &&
            JITGranularityRequested(cx) >= JITREPORT_GRANULARITY_FUNCTION);
}

inline bool
Probes::enterScript(JSContext *cx, JSScript *script, JSFunction *maybeFun,
                    StackFrame *fp)
{
    bool ok = true;
#ifdef INCLUDE_MOZILLA_DTRACE
    if (JAVASCRIPT_FUNCTION_ENTRY_ENABLED())
        DTraceEnterJSFun(cx, maybeFun, script);
#endif
#ifdef MOZ_TRACE_JSCALLS
    cx->doFunctionCallback(maybeFun, script, 1);
#endif

    JSRuntime *rt = cx->runtime;
    if (rt->spsProfiler.enabled()) {
        rt->spsProfiler.enter(cx, script, maybeFun);
        JS_ASSERT_IF(!fp->isGeneratorFrame(), !fp->hasPushedSPSFrame());
        fp->setPushedSPSFrame();
    }

    return ok;
}

inline bool
Probes::exitScript(JSContext *cx, JSScript *script, JSFunction *maybeFun,
                   AbstractFramePtr fp)
{
    bool ok = true;

#ifdef INCLUDE_MOZILLA_DTRACE
    if (JAVASCRIPT_FUNCTION_RETURN_ENABLED())
        DTraceExitJSFun(cx, maybeFun, script);
#endif
#ifdef MOZ_TRACE_JSCALLS
    cx->doFunctionCallback(maybeFun, script, 0);
#endif

    JSRuntime *rt = cx->runtime;
    /*
     * Coming from IonMonkey, the fp might not be known (fp == NULL), but
     * IonMonkey will only call exitScript() when absolutely necessary, so it is
     * guaranteed that fp->hasPushedSPSFrame() would have been true
     */
    if ((!fp && rt->spsProfiler.enabled()) || (fp && fp.hasPushedSPSFrame()))
        rt->spsProfiler.exit(cx, script, maybeFun);
    return ok;
}

inline bool
Probes::exitScript(JSContext *cx, JSScript *script, JSFunction *maybeFun,
                   StackFrame *fp)
{
    return Probes::exitScript(cx, script, maybeFun, fp ? AbstractFramePtr(fp) : AbstractFramePtr());
}

inline bool
Probes::startExecution(JSScript *script)
{
    bool ok = true;

#ifdef INCLUDE_MOZILLA_DTRACE
    if (JAVASCRIPT_EXECUTE_START_ENABLED())
        JAVASCRIPT_EXECUTE_START((script->filename() ? (char *)script->filename() : nullName),
                                 script->lineno);
#endif

    return ok;
}

inline bool
Probes::stopExecution(JSScript *script)
{
    bool ok = true;

#ifdef INCLUDE_MOZILLA_DTRACE
    if (JAVASCRIPT_EXECUTE_DONE_ENABLED())
        JAVASCRIPT_EXECUTE_DONE((script->filename() ? (char *)script->filename() : nullName),
                                script->lineno);
#endif

    return ok;
}

} /* namespace js */
 
#endif  // Probes_inl_h__

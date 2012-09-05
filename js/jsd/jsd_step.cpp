/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JavaScript Debugging support - Stepping support
 */

#include "jsd.h"

/*
* #define JSD_TRACE 1
*/

#ifdef JSD_TRACE

static char*
_indentSpaces(int i)
{
#define MAX_INDENT 63
    static char* p = NULL;
    if(!p)
    {
        p = calloc(1, MAX_INDENT+1);
        if(!p) return "";
        memset(p, ' ', MAX_INDENT);
    }
    if(i > MAX_INDENT) return p;
    return p + MAX_INDENT-i;
}

static void
_interpreterTrace(JSDContext* jsdc, JSContext *cx, JSStackFrame *fp,
                  JSBool before)
{
    JSDScript* jsdscript = NULL;
    JSScript * script;
    static indent = 0;
    JSString* funName = NULL;

    script = JS_GetFrameScript(cx, fp);
    if(script)
    {
        JSD_LOCK_SCRIPTS(jsdc);
        jsdscript = jsd_FindOrCreateJSDScript(jsdc, cx, script, fp);
        JSD_UNLOCK_SCRIPTS(jsdc);
        if(jsdscript)
            funName = JSD_GetScriptFunctionId(jsdc, jsdscript);
    }

    if(before)
        printf("%sentering ", _indentSpaces(indent++));
    else
        printf("%sleaving ", _indentSpaces(--indent));

    if (!funName)
        printf("TOP_LEVEL");
    else
        JS_FileEscapedString(stdout, funName, 0);

    if(before)
    {
        jsval thisVal;

        printf("%s this: ", JS_IsConstructorFrame(cx, fp) ? "constructing":"");

        if (JS_GetFrameThis(cx, fp, &thisVal))
            printf("0x%0llx", (uintptr_t) thisVal);
        else
            puts("<unavailable>");
    }
    printf("\n");
    JS_ASSERT(indent >= 0);
}
#endif

JSBool
_callHook(JSDContext *jsdc, JSContext *cx, JSStackFrame *fp, JSBool before,
          unsigned type, JSD_CallHookProc hook, void *hookData)
{
    JSDScript*        jsdscript;
    JSScript*         jsscript;
    JSBool            hookresult = JS_TRUE;
    
    if (!jsdc || !jsdc->inited)
        return JS_FALSE;

    if (!hook && !(jsdc->flags & JSD_COLLECT_PROFILE_DATA))
    {
        /* no hook to call, no profile data needs to be collected,
         * so there is nothing to do here.
         */
        return hookresult;
    }
    
    if (before && JS_IsConstructorFrame(cx, fp)) {
        jsval newObj;
        if (!JS_GetFrameThis(cx, fp, &newObj))
            return JS_FALSE;
        jsd_Constructing(jsdc, cx, JSVAL_TO_OBJECT(newObj), fp);
    }

    jsscript = JS_GetFrameScript(cx, fp);
    if (jsscript)
    {
        JSD_LOCK_SCRIPTS(jsdc);
        jsdscript = jsd_FindOrCreateJSDScript(jsdc, cx, jsscript, fp);
        JSD_UNLOCK_SCRIPTS(jsdc);
    
        if (jsdscript)
        {
            if (JSD_IS_PROFILE_ENABLED(jsdc, jsdscript))
            {
                JSDProfileData *pdata;
                pdata = jsd_GetScriptProfileData (jsdc, jsdscript);
                if (pdata)
                {
                    if (before)
                    {
                        if (!pdata->lastCallStart)
                        {
                            int64_t now;
                            JSDProfileData *callerpdata;
                            
                            /* Get the time just the once, for consistency. */
                            now = JS_Now();
                            /* This contains a pointer to the profile data for
                             * the caller of this function. */
                            callerpdata = jsdc->callingFunctionPData;
                            if (callerpdata)
                            {
                                int64_t ll_delta;
                                pdata->caller = callerpdata;
                                /* We need to 'stop' the timer for the caller.
                                 * Use time since last return if appropriate. */
                                ll_delta = jsdc->lastReturnTime
                                           ? now - jsdc->lastReturnTime
                                           : now - callerpdata->lastCallStart;
                                callerpdata->runningTime += ll_delta;
                            }
                            /* We're the new current function, and no return
                             * has happened yet. */
                            jsdc->callingFunctionPData = pdata;
                            jsdc->lastReturnTime = 0;
                            /* This function has no running time (just been
                             * called!), and we'll need the call start time. */
                            pdata->runningTime = 0;
                            pdata->lastCallStart = now;
                        } else {
                            if (++pdata->recurseDepth > pdata->maxRecurseDepth)
                                pdata->maxRecurseDepth = pdata->recurseDepth;
                        }
                        /* make sure we're called for the return too. */
                        hookresult = JS_TRUE;
                    } else if (!pdata->recurseDepth && pdata->lastCallStart) {
                        int64_t now, ll_delta;
                        double delta;
                        now = JS_Now();
                        ll_delta = now - pdata->lastCallStart;
                        delta = ll_delta;
                        delta /= 1000.0;
                        pdata->totalExecutionTime += delta;
                        /* minExecutionTime starts as 0, so we need to overwrite
                         * it on the first call always. */
                        if ((0 == pdata->callCount) ||
                            delta < pdata->minExecutionTime)
                        {
                            pdata->minExecutionTime = delta;
                        }
                        if (delta > pdata->maxExecutionTime)
                            pdata->maxExecutionTime = delta;
                        
                        /* If we last returned from a function (as opposed to
                         * having last entered this function), we need to inc.
                         * the running total by the time delta since the last
                         * return, and use the running total instead of the
                         * delta calculated above. */
                        if (jsdc->lastReturnTime)
                        {
                            /* Add last chunk to running time, and use total
                             * running time as 'delta'. */
                            ll_delta = now - jsdc->lastReturnTime;
                            pdata->runningTime += ll_delta;
                            delta = pdata->runningTime;
                            delta /= 1000.0;
                        }
                        
                        pdata->totalOwnExecutionTime += delta;
                        /* See minExecutionTime comment above. */
                        if ((0 == pdata->callCount) ||
                            delta < pdata->minOwnExecutionTime)
                        {
                            pdata->minOwnExecutionTime = delta;
                        }
                        if (delta > pdata->maxOwnExecutionTime)
                            pdata->maxOwnExecutionTime = delta;
                        
                        /* Current function is now our caller. */
                        jsdc->callingFunctionPData = pdata->caller;
                        /* No hanging pointers, please. */
                        pdata->caller = NULL;
                        /* Mark the time we returned, and indicate this
                         * function is no longer running. */
                        jsdc->lastReturnTime = now;
                        pdata->lastCallStart = 0;
                        ++pdata->callCount;
                    } else if (pdata->recurseDepth) {
                        --pdata->recurseDepth;
                        ++pdata->callCount;
                    }
                }
                if (hook)
                    jsd_CallCallHook (jsdc, cx, type, hook, hookData);
            } else {
                if (hook)
                    hookresult = 
                        jsd_CallCallHook (jsdc, cx, type, hook, hookData);
                else
                    hookresult = JS_TRUE;
            }
        }
    }

#ifdef JSD_TRACE
    _interpreterTrace(jsdc, cx, fp, before);
    return JS_TRUE;
#else
    return hookresult;
#endif

}

void *
jsd_FunctionCallHook(JSContext *cx, JSStackFrame *fp, JSBool before,
                     JSBool *ok, void *closure)
{
    JSDContext*       jsdc;
    JSD_CallHookProc  hook;
    void*             hookData;

    jsdc = (JSDContext*) closure;
    
    /* local in case jsdc->functionHook gets cleared on another thread */
    JSD_LOCK();
    hook     = jsdc->functionHook;
    hookData = jsdc->functionHookData;
    JSD_UNLOCK();
    
    if (_callHook (jsdc, cx, fp, before,
                   (before) ? JSD_HOOK_FUNCTION_CALL : JSD_HOOK_FUNCTION_RETURN,
                   hook, hookData))
    {
        return closure;
    }
    
    return NULL;
}

void *
jsd_TopLevelCallHook(JSContext *cx, JSStackFrame *fp, JSBool before,
                     JSBool *ok, void *closure)
{
    JSDContext*       jsdc;
    JSD_CallHookProc  hook;
    void*             hookData;

    jsdc = (JSDContext*) closure;

    /* local in case jsdc->toplevelHook gets cleared on another thread */
    JSD_LOCK();
    hook     = jsdc->toplevelHook;
    hookData = jsdc->toplevelHookData;
    JSD_UNLOCK();
    
    if (_callHook (jsdc, cx, fp, before,
                   (before) ? JSD_HOOK_TOPLEVEL_START : JSD_HOOK_TOPLEVEL_END,
                   hook, hookData))
    {
        return closure;
    }
    
    return NULL;
    
}

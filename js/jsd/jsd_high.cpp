/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JavaScript Debugging support - 'High Level' functions
 */

#include "jsd.h"

/***************************************************************************/

/* XXX not 'static' because of old Mac CodeWarrior bug */ 
JSCList _jsd_context_list = JS_INIT_STATIC_CLIST(&_jsd_context_list);

/* these are used to connect JSD_SetUserCallbacks() with JSD_DebuggerOn() */
static JSD_UserCallbacks _callbacks;
static void*             _user = NULL; 
static JSRuntime*        _jsrt = NULL;

#ifdef JSD_HAS_DANGEROUS_THREAD
static void* _dangerousThread = NULL;
#endif

#ifdef JSD_THREADSAFE
JSDStaticLock* _jsd_global_lock = NULL;
#endif

#ifdef DEBUG
void JSD_ASSERT_VALID_CONTEXT(JSDContext* jsdc)
{
    JS_ASSERT(jsdc->inited);
    JS_ASSERT(jsdc->jsrt);
    JS_ASSERT(jsdc->dumbContext);
    JS_ASSERT(jsdc->glob);
}
#endif

/***************************************************************************/
/* xpconnect related utility functions implemented in jsd_xpc.cpp */

extern void
global_finalize(JSFreeOp* fop, JSObject* obj);

extern JSObject*
CreateJSDGlobal(JSContext *cx, JSClass *clasp);

/***************************************************************************/


static JSClass global_class = {
    "JSDGlobal", JSCLASS_GLOBAL_FLAGS |
    JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
    JS_PropertyStub,  JS_DeletePropertyStub,  JS_PropertyStub,  JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   global_finalize
};

static JSBool
_validateUserCallbacks(JSD_UserCallbacks* callbacks)
{
    return !callbacks ||
           (callbacks->size && callbacks->size <= sizeof(JSD_UserCallbacks));
}    

static JSDContext*
_newJSDContext(JSRuntime*         jsrt, 
               JSD_UserCallbacks* callbacks, 
               void*              user,
               JSObject*          scopeobj)
{
    JSDContext* jsdc = NULL;
    JSCompartment *oldCompartment = NULL;
    JSBool ok;

    if( ! jsrt )
        return NULL;

    if( ! _validateUserCallbacks(callbacks) )
        return NULL;

    jsdc = (JSDContext*) calloc(1, sizeof(JSDContext));
    if( ! jsdc )
        goto label_newJSDContext_failure;

    if( ! JSD_INIT_LOCKS(jsdc) )
        goto label_newJSDContext_failure;

    JS_INIT_CLIST(&jsdc->links);

    jsdc->jsrt = jsrt;

    if( callbacks )
        memcpy(&jsdc->userCallbacks, callbacks, callbacks->size);
    
    jsdc->user = user;

#ifdef JSD_HAS_DANGEROUS_THREAD
    jsdc->dangerousThread = _dangerousThread;
#endif

    JS_INIT_CLIST(&jsdc->threadsStates);
    JS_INIT_CLIST(&jsdc->sources);
    JS_INIT_CLIST(&jsdc->removedSources);

    jsdc->sourceAlterCount = 1;

    if( ! jsd_CreateAtomTable(jsdc) )
        goto label_newJSDContext_failure;

    if( ! jsd_InitObjectManager(jsdc) )
        goto label_newJSDContext_failure;

    if( ! jsd_InitScriptManager(jsdc) )
        goto label_newJSDContext_failure;

    jsdc->dumbContext = JS_NewContext(jsdc->jsrt, 256);
    if( ! jsdc->dumbContext )
        goto label_newJSDContext_failure;

    JS_BeginRequest(jsdc->dumbContext);
    JS_SetOptions(jsdc->dumbContext, JS_GetOptions(jsdc->dumbContext));

    jsdc->glob = CreateJSDGlobal(jsdc->dumbContext, &global_class);

    if( ! jsdc->glob )
        goto label_newJSDContext_failure;

    oldCompartment = JS_EnterCompartment(jsdc->dumbContext, jsdc->glob);

    if ( ! JS_AddNamedObjectRoot(jsdc->dumbContext, &jsdc->glob, "JSD context global") )
        goto label_newJSDContext_failure;

    ok = JS_InitStandardClasses(jsdc->dumbContext, jsdc->glob);

    JS_LeaveCompartment(jsdc->dumbContext, oldCompartment);
    if( ! ok )
        goto label_newJSDContext_failure;

    JS_EndRequest(jsdc->dumbContext);

    jsdc->data = NULL;
    jsdc->inited = JS_TRUE;

    JSD_LOCK();
    JS_INSERT_LINK(&jsdc->links, &_jsd_context_list);
    JSD_UNLOCK();

    return jsdc;

label_newJSDContext_failure:
    if( jsdc ) {
        if ( jsdc->dumbContext && jsdc->glob )
            JS_RemoveObjectRootRT(JS_GetRuntime(jsdc->dumbContext), &jsdc->glob);
        jsd_DestroyObjectManager(jsdc);
        jsd_DestroyAtomTable(jsdc);
        if( jsdc->dumbContext )
            JS_EndRequest(jsdc->dumbContext);
        free(jsdc);
    }
    return NULL;
}

static void
_destroyJSDContext(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);

    JSD_LOCK();
    JS_REMOVE_LINK(&jsdc->links);
    JSD_UNLOCK();

    if ( jsdc->dumbContext && jsdc->glob ) {
        JS_RemoveObjectRootRT(JS_GetRuntime(jsdc->dumbContext), &jsdc->glob);
    }
    jsd_DestroyObjectManager(jsdc);
    jsd_DestroyAtomTable(jsdc);

    jsdc->inited = JS_FALSE;

    /*
    * We should free jsdc here, but we let it leak in case there are any 
    * asynchronous hooks calling into the system using it as a handle
    *
    * XXX we also leak the locks
    */
    JS_DestroyContext(jsdc->dumbContext);
    jsdc->dumbContext = NULL;
}

/***************************************************************************/

JSDContext*
jsd_DebuggerOnForUser(JSRuntime*         jsrt, 
                      JSD_UserCallbacks* callbacks, 
                      void*              user,
                      JSObject*          scopeobj)
{
    JSDContext* jsdc;

    jsdc = _newJSDContext(jsrt, callbacks, user, scopeobj);
    if( ! jsdc )
        return NULL;

    /*
     * Set hooks here.  The new/destroy script hooks are on even when
     * the debugger is paused.  The destroy hook so we'll clean up
     * internal data structures when scripts are destroyed, and the
     * newscript hook for backwards compatibility for now.  We'd like
     * to stop doing that.
     */
    JS_SetNewScriptHookProc(jsdc->jsrt, jsd_NewScriptHookProc, jsdc);
    JS_SetDestroyScriptHookProc(jsdc->jsrt, jsd_DestroyScriptHookProc, jsdc);
    jsd_DebuggerUnpause(jsdc);
#ifdef LIVEWIRE
    LWDBG_SetNewScriptHookProc(jsd_NewScriptHookProc, jsdc);
#endif
    if( jsdc->userCallbacks.setContext )
        jsdc->userCallbacks.setContext(jsdc, jsdc->user);
    return jsdc;
}

JSDContext*
jsd_DebuggerOn(void)
{
    JS_ASSERT(_jsrt);
    JS_ASSERT(_validateUserCallbacks(&_callbacks));
    return jsd_DebuggerOnForUser(_jsrt, &_callbacks, _user, NULL);
}

void
jsd_DebuggerOff(JSDContext* jsdc)
{
    jsd_DebuggerPause(jsdc, JS_TRUE);
    /* clear hooks here */
    JS_SetNewScriptHookProc(jsdc->jsrt, NULL, NULL);
    JS_SetDestroyScriptHookProc(jsdc->jsrt, NULL, NULL);
#ifdef LIVEWIRE
    LWDBG_SetNewScriptHookProc(NULL,NULL);
#endif

    /* clean up */
    JSD_LockScriptSubsystem(jsdc);
    jsd_DestroyScriptManager(jsdc);
    JSD_UnlockScriptSubsystem(jsdc);
    jsd_DestroyAllSources(jsdc);
    
    _destroyJSDContext(jsdc);

    if( jsdc->userCallbacks.setContext )
        jsdc->userCallbacks.setContext(NULL, jsdc->user);
}

void
jsd_DebuggerPause(JSDContext* jsdc, JSBool forceAllHooksOff)
{
    JS_SetDebuggerHandler(jsdc->jsrt, NULL, NULL);
    if (forceAllHooksOff || !(jsdc->flags & JSD_COLLECT_PROFILE_DATA)) {
        JS_SetExecuteHook(jsdc->jsrt, NULL, NULL);
        JS_SetCallHook(jsdc->jsrt, NULL, NULL);
    }
    JS_SetThrowHook(jsdc->jsrt, NULL, NULL);
    JS_SetDebugErrorHook(jsdc->jsrt, NULL, NULL);
}

static JSBool
jsd_DebugErrorHook(JSContext *cx, const char *message,
                   JSErrorReport *report, void *closure);

void
jsd_DebuggerUnpause(JSDContext* jsdc)
{
    JS_SetDebuggerHandler(jsdc->jsrt, jsd_DebuggerHandler, jsdc);
    JS_SetExecuteHook(jsdc->jsrt, jsd_TopLevelCallHook, jsdc);
    JS_SetCallHook(jsdc->jsrt, jsd_FunctionCallHook, jsdc);
    JS_SetThrowHook(jsdc->jsrt, jsd_ThrowHandler, jsdc);
    JS_SetDebugErrorHook(jsdc->jsrt, jsd_DebugErrorHook, jsdc);
}

void
jsd_SetUserCallbacks(JSRuntime* jsrt, JSD_UserCallbacks* callbacks, void* user)
{
    _jsrt = jsrt;
    _user = user;

#ifdef JSD_HAS_DANGEROUS_THREAD
    _dangerousThread = JSD_CURRENT_THREAD();
#endif

    if( callbacks )
        memcpy(&_callbacks, callbacks, sizeof(JSD_UserCallbacks));
    else
        memset(&_callbacks, 0 , sizeof(JSD_UserCallbacks));
}

void*
jsd_SetContextPrivate(JSDContext* jsdc, void *data)
{
    jsdc->data = data;
    return data;
}

void*
jsd_GetContextPrivate(JSDContext* jsdc)
{
    return jsdc->data;
}

void
jsd_ClearAllProfileData(JSDContext* jsdc)
{
    JSDScript *current;
    
    JSD_LOCK_SCRIPTS(jsdc);
    current = (JSDScript *)jsdc->scripts.next;
    while (current != (JSDScript *)&jsdc->scripts)
    {
        jsd_ClearScriptProfileData(jsdc, current);
        current = (JSDScript *)current->links.next;
    }

    JSD_UNLOCK_SCRIPTS(jsdc);
}

JSDContext*
jsd_JSDContextForJSContext(JSContext* context)
{
    JSDContext* iter;
    JSDContext* jsdc = NULL;
    JSRuntime*  runtime = JS_GetRuntime(context);

    JSD_LOCK();
    for( iter = (JSDContext*)_jsd_context_list.next;
         iter != (JSDContext*)&_jsd_context_list;
         iter = (JSDContext*)iter->links.next )
    {
        if( runtime == iter->jsrt )
        {
            jsdc = iter;
            break;
        }
    }
    JSD_UNLOCK();
    return jsdc;
}    

static JSBool
jsd_DebugErrorHook(JSContext *cx, const char *message,
                   JSErrorReport *report, void *closure)
{
    JSDContext* jsdc = (JSDContext*) closure;
    JSD_ErrorReporter errorReporter;
    void*             errorReporterData;
    
    if( ! jsdc )
    {
        JS_ASSERT(0);
        return JS_TRUE;
    }
    if( JSD_IS_DANGEROUS_THREAD(jsdc) )
        return JS_TRUE;

    /* local in case hook gets cleared on another thread */
    JSD_LOCK();
    errorReporter     = jsdc->errorReporter;
    errorReporterData = jsdc->errorReporterData;
    JSD_UNLOCK();

    if(!errorReporter)
        return JS_TRUE;

    switch(errorReporter(jsdc, cx, message, report, errorReporterData))
    {
        case JSD_ERROR_REPORTER_PASS_ALONG:
            return JS_TRUE;
        case JSD_ERROR_REPORTER_RETURN:
            return JS_FALSE;
        case JSD_ERROR_REPORTER_DEBUG:
        {
            jsval rval;
            JSD_ExecutionHookProc   hook;
            void*                   hookData;

            /* local in case hook gets cleared on another thread */
            JSD_LOCK();
            hook = jsdc->debugBreakHook;
            hookData = jsdc->debugBreakHookData;
            JSD_UNLOCK();

            jsd_CallExecutionHook(jsdc, cx, JSD_HOOK_DEBUG_REQUESTED,
                                  hook, hookData, &rval);
            /* XXX Should make this dependent on ExecutionHook retval */
            return JS_TRUE;
        }
        case JSD_ERROR_REPORTER_CLEAR_RETURN:
            if(report && JSREPORT_IS_EXCEPTION(report->flags))
                JS_ClearPendingException(cx);
            return JS_FALSE;
        default:
            JS_ASSERT(0);
            break;
    }
    return JS_TRUE;
}

JSBool
jsd_SetErrorReporter(JSDContext*       jsdc, 
                     JSD_ErrorReporter reporter, 
                     void*             callerdata)
{
    JSD_LOCK();
    jsdc->errorReporter = reporter;
    jsdc->errorReporterData = callerdata;
    JSD_UNLOCK();
    return JS_TRUE;
}

JSBool
jsd_GetErrorReporter(JSDContext*        jsdc, 
                     JSD_ErrorReporter* reporter, 
                     void**             callerdata)
{
    JSD_LOCK();
    if( reporter )
        *reporter = jsdc->errorReporter;
    if( callerdata )
        *callerdata = jsdc->errorReporterData;
    JSD_UNLOCK();
    return JS_TRUE;
}

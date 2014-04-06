/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JavaScript Debugging support - 'High Level' functions
 */

#include "jsd.h"
#include "nsCxPusher.h"

using mozilla::AutoSafeJSContext;

/***************************************************************************/

/* XXX not 'static' because of old Mac CodeWarrior bug */ 
JSCList _jsd_context_list = JS_INIT_STATIC_CLIST(&_jsd_context_list);

/* these are used to connect JSD_SetUserCallbacks() with JSD_DebuggerOn() */
static JSD_UserCallbacks _callbacks;
static void*             _user = nullptr; 
static JSRuntime*        _jsrt = nullptr;

#ifdef JSD_HAS_DANGEROUS_THREAD
static void* _dangerousThread = nullptr;
#endif

#ifdef JSD_THREADSAFE
JSDStaticLock* _jsd_global_lock = nullptr;
#endif

#ifdef DEBUG
void JSD_ASSERT_VALID_CONTEXT(JSDContext* jsdc)
{
    MOZ_ASSERT(jsdc->inited);
    MOZ_ASSERT(jsdc->jsrt);
    MOZ_ASSERT(jsdc->glob);
}
#endif

/***************************************************************************/
/* xpconnect related utility functions implemented in jsd_xpc.cpp */

extern void
global_finalize(JSFreeOp* fop, JSObject* obj);

extern JSObject*
CreateJSDGlobal(JSContext *cx, const JSClass *clasp);

/***************************************************************************/


static const JSClass global_class = {
    "JSDGlobal", JSCLASS_GLOBAL_FLAGS |
    JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
    JS_PropertyStub,  JS_DeletePropertyStub,  JS_PropertyStub,  JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   global_finalize,
    nullptr, nullptr, nullptr,
    JS_GlobalObjectTraceHook
};

static bool
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
    JSDContext* jsdc = nullptr;
    bool ok = true;
    AutoSafeJSContext cx;

    if( ! jsrt )
        return nullptr;

    if( ! _validateUserCallbacks(callbacks) )
        return nullptr;

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


    jsdc->glob = CreateJSDGlobal(cx, &global_class);

    if( ! jsdc->glob )
        goto label_newJSDContext_failure;

    {
        JSAutoCompartment ac(cx, jsdc->glob);
        ok = JS_AddNamedObjectRoot(cx, &jsdc->glob, "JSD context global") &&
             JS_InitStandardClasses(cx, JS::HandleObject::fromMarkedLocation(&jsdc->glob));
    }
    if( ! ok )
        goto label_newJSDContext_failure;

    jsdc->data = nullptr;
    jsdc->inited = true;

    JSD_LOCK();
    JS_INSERT_LINK(&jsdc->links, &_jsd_context_list);
    JSD_UNLOCK();

    return jsdc;

label_newJSDContext_failure:
    if( jsdc ) {
        if ( jsdc->glob )
            JS_RemoveObjectRootRT(JS_GetRuntime(cx), &jsdc->glob);
        jsd_DestroyObjectManager(jsdc);
        jsd_DestroyAtomTable(jsdc);
        free(jsdc);
    }
    return nullptr;
}

static void
_destroyJSDContext(JSDContext* jsdc)
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);

    JSD_LOCK();
    JS_REMOVE_LINK(&jsdc->links);
    JSD_UNLOCK();

    if ( jsdc->glob ) {
        JS_RemoveObjectRootRT(jsdc->jsrt, &jsdc->glob);
    }
    jsd_DestroyObjectManager(jsdc);
    jsd_DestroyAtomTable(jsdc);

    jsdc->inited = false;

    /*
    * We should free jsdc here, but we let it leak in case there are any 
    * asynchronous hooks calling into the system using it as a handle
    *
    * XXX we also leak the locks
    */
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
        return nullptr;

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

    if( jsdc->userCallbacks.setContext )
        jsdc->userCallbacks.setContext(jsdc, jsdc->user);
    return jsdc;
}

JSDContext*
jsd_DebuggerOn(void)
{
    MOZ_ASSERT(_jsrt);
    MOZ_ASSERT(_validateUserCallbacks(&_callbacks));
    return jsd_DebuggerOnForUser(_jsrt, &_callbacks, _user, nullptr);
}

void
jsd_DebuggerOff(JSDContext* jsdc)
{
    jsd_DebuggerPause(jsdc, true);
    /* clear hooks here */
    JS_SetNewScriptHookProc(jsdc->jsrt, nullptr, nullptr);
    JS_SetDestroyScriptHookProc(jsdc->jsrt, nullptr, nullptr);

    /* clean up */
    JSD_LockScriptSubsystem(jsdc);
    jsd_DestroyScriptManager(jsdc);
    JSD_UnlockScriptSubsystem(jsdc);
    jsd_DestroyAllSources(jsdc);
    
    _destroyJSDContext(jsdc);

    if( jsdc->userCallbacks.setContext )
        jsdc->userCallbacks.setContext(nullptr, jsdc->user);
}

void
jsd_DebuggerPause(JSDContext* jsdc, bool forceAllHooksOff)
{
    JS_SetDebuggerHandler(jsdc->jsrt, nullptr, nullptr);
    if (forceAllHooksOff || !(jsdc->flags & JSD_COLLECT_PROFILE_DATA)) {
        JS_SetExecuteHook(jsdc->jsrt, nullptr, nullptr);
        JS_SetCallHook(jsdc->jsrt, nullptr, nullptr);
    }
    JS_SetThrowHook(jsdc->jsrt, nullptr, nullptr);
    JS_SetDebugErrorHook(jsdc->jsrt, nullptr, nullptr);
}

static bool
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
    JSDContext* jsdc = nullptr;
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

static bool
jsd_DebugErrorHook(JSContext *cx, const char *message,
                   JSErrorReport *report, void *closure)
{
    JSDContext* jsdc = (JSDContext*) closure;
    JSD_ErrorReporter errorReporter;
    void*             errorReporterData;
    
    if( ! jsdc )
    {
        MOZ_ASSERT(0);
        return true;
    }
    if( JSD_IS_DANGEROUS_THREAD(jsdc) )
        return true;

    /* local in case hook gets cleared on another thread */
    JSD_LOCK();
    errorReporter     = jsdc->errorReporter;
    errorReporterData = jsdc->errorReporterData;
    JSD_UNLOCK();

    if(!errorReporter)
        return true;

    switch(errorReporter(jsdc, cx, message, report, errorReporterData))
    {
        case JSD_ERROR_REPORTER_PASS_ALONG:
            return true;
        case JSD_ERROR_REPORTER_RETURN:
            return false;
        case JSD_ERROR_REPORTER_DEBUG:
        {
            JS::RootedValue rval(cx);
            JSD_ExecutionHookProc   hook;
            void*                   hookData;

            /* local in case hook gets cleared on another thread */
            JSD_LOCK();
            hook = jsdc->debugBreakHook;
            hookData = jsdc->debugBreakHookData;
            JSD_UNLOCK();

            jsd_CallExecutionHook(jsdc, cx, JSD_HOOK_DEBUG_REQUESTED,
                                  hook, hookData, rval.address());
            /* XXX Should make this dependent on ExecutionHook retval */
            return true;
        }
        case JSD_ERROR_REPORTER_CLEAR_RETURN:
            if(report && JSREPORT_IS_EXCEPTION(report->flags))
                JS_ClearPendingException(cx);
            return false;
        default:
            MOZ_ASSERT(0);
            break;
    }
    return true;
}

bool
jsd_SetErrorReporter(JSDContext*       jsdc, 
                     JSD_ErrorReporter reporter, 
                     void*             callerdata)
{
    JSD_LOCK();
    jsdc->errorReporter = reporter;
    jsdc->errorReporterData = callerdata;
    JSD_UNLOCK();
    return true;
}

bool
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
    return true;
}

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
** JavaScript Debugger Navigator API - 'High Level' functions
*/

#include "jsd.h"

/***************************************************************************/

/* use a global context for now (avoid direct references to it!) */
static JSDContext _static_context;

/* these are global now, they transcend our concept of JSDContext...*/

static JSD_UserCallbacks _callbacks;
static void* _user; 
static JSTaskState* _jstaskstate;
static PRThread * _dangerousThread1;

#ifdef DEBUG
void JSD_ASSERT_VALID_CONTEXT( JSDContext* jsdc )
{
    PR_ASSERT( jsdc == &_static_context );
    PR_ASSERT( jsdc->inited );
    PR_ASSERT( jsdc->jstaskstate );
    PR_ASSERT( jsdc->jscontexts );
}
#endif

static PRHashNumber
_hash_root(const void *key)
{
    PRHashNumber num = (PRHashNumber) key;
    return num >> 2;
}

static JSClass global_class = {
    "JSDGlobal", 0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
};

static JSDContext*
NewJSDContext(void)
{
    JSDContext* jsdc = &_static_context;
    
    if( jsdc->inited )
        return NULL;

    if( ! _jstaskstate )
        return NULL;

    memset( jsdc, 0, sizeof(JSDContext) );
    jsdc->jstaskstate = _jstaskstate;

    PR_INIT_CLIST(&jsdc->threadsStates);

    jsdc->dumbContext = JS_NewContext( _jstaskstate, 256 );
    if( ! jsdc->dumbContext )
        return NULL;

    jsdc->glob = JS_NewObject(jsdc->dumbContext, &global_class, NULL, NULL);
    if( ! jsdc->glob )
        return NULL;

    if( ! JS_InitStandardClasses(jsdc->dumbContext, jsdc->glob) )
        return NULL;

    jsdc->jscontexts = PR_NewHashTable(256, _hash_root,
                                       PR_CompareValues, PR_CompareValues,
                                       NULL, NULL);
    if( ! jsdc->jscontexts )
        return NULL;

    jsdc->inited = JS_TRUE;
    return jsdc;
}

PR_STATIC_CALLBACK(PRIntn)
_hash_entry_zapper(PRHashEntry *he, PRIntn i, void *arg)
{
    PR_FREEIF(he->value);
    he->value = NULL;
    he->key   = NULL;
    return HT_ENUMERATE_NEXT;
}

static void
DestroyJSDContext( JSDContext* jsdc )
{
    JSD_ASSERT_VALID_CONTEXT(jsdc);
    if( jsdc->jscontexts )
    {
        PR_HashTableEnumerateEntries(jsdc->jscontexts, _hash_entry_zapper, NULL);
        PR_HashTableDestroy(jsdc->jscontexts);
    }
    jsdc->inited = JS_FALSE;
}

JSDContext*
jsd_GetDefaultJSDContext(void)
{
    JSDContext* jsdc = &_static_context;
    if( ! jsdc->inited )
        return NULL;
    return jsdc;
}

/***************************************************************************/


JSDContext*
jsd_DebuggerOn(void)
{
    JSDContext* jsdc = NewJSDContext();
    JSContext* iter = NULL;
    JSContext* cx;

    if( ! jsdc )
        return NULL;
    
    /* set hooks here */
    JS_SetNewScriptHookProc( jsdc->jstaskstate, jsd_NewScriptHookProc, jsdc );
    JS_SetDestroyScriptHookProc( jsdc->jstaskstate, jsd_DestroyScriptHookProc, jsdc );

    /* enumerate contexts for JSTaskState and add them to our table */
    while( NULL != (cx = JS_ContextIterator(jsdc->jstaskstate, &iter)) )
        jsd_JSContextUsed( jsdc, cx );

    if( _callbacks.setContext )
        _callbacks.setContext( jsdc, _user );
    
    return jsdc;
}


void
jsd_DebuggerOff(JSDContext* jsdc)
{
    /* clear hooks here */
    JS_SetNewScriptHookProc( jsdc->jstaskstate, NULL, NULL );
    JS_SetDestroyScriptHookProc( jsdc->jstaskstate, NULL, NULL );

    /* clean up */
    jsd_DestroyAllJSDScripts( jsdc );
    jsd_DestroyAllSources( jsdc );
    
    DestroyJSDContext( jsdc );

    if( _callbacks.setContext )
        _callbacks.setContext( NULL, _user );
}

void
jsd_SetUserCallbacks(JSTaskState* jstaskstate, JSD_UserCallbacks* callbacks, void* user)
{
    _jstaskstate = jstaskstate;
    _user = user;
    _dangerousThread1 = PR_CurrentThread();

    if( callbacks )
        memcpy( &_callbacks, callbacks, sizeof(JSD_UserCallbacks) );
    else
        memset( &_callbacks, 0 , sizeof(JSD_UserCallbacks) );

    if( _callbacks.setContext && _static_context.inited ) 
        _callbacks.setContext( &_static_context, _user );
}

JSDContextWrapper*
jsd_JSDContextWrapperForJSContext( JSDContext* jsdc, JSContext* context )
{
    return (JSDContextWrapper*) PR_HashTableLookup(jsdc->jscontexts, context);
}

PR_STATIC_CALLBACK(void)
jsd_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
    JSDContextWrapper* wrapper;
    JSDContext* jsdc;
    PRUintn action = JSD_ERROR_REPORTER_PASS_ALONG;

    jsdc = jsd_GetDefaultJSDContext();
    if( ! jsdc )
    {
        PR_ASSERT(0);
        return;
    }

    wrapper = jsd_JSDContextWrapperForJSContext( jsdc, cx );
    if( ! wrapper )
    {
        PR_ASSERT(0);
        return;
    }

    if( jsdc->errorReporter && ! jsd_IsCurrentThreadDangerous() )
        action = jsdc->errorReporter(jsdc, cx, message, report, 
                                     jsdc->errorReporterData);

    switch(action)
    {
        case JSD_ERROR_REPORTER_PASS_ALONG:
            if( wrapper->originalErrorReporter )
                wrapper->originalErrorReporter(cx, message, report);
            break;
        case JSD_ERROR_REPORTER_RETURN:    
            break;
        case JSD_ERROR_REPORTER_DEBUG:
        {
            JSDThreadState* jsdthreadstate;

            if( ! jsdc->debugBreakHook )
                return;

            jsdthreadstate = jsd_NewThreadState(jsdc,cx);
            if( jsdthreadstate )
            {
                (*jsdc->debugBreakHook)(jsdc, jsdthreadstate, 
                                        JSD_HOOK_DEBUG_REQUESTED, 
                                        jsdc->debugBreakHookData );

                jsd_DestroyThreadState(jsdc, jsdthreadstate);
            }
        }
        default:;
    }
}

void 
jsd_JSContextUsed( JSDContext* jsdc, JSContext* context )
{
    JSDContextWrapper* wrapper;

    wrapper = jsd_JSDContextWrapperForJSContext(jsdc, context);
    if( wrapper )
    {
        /* error reporters are sometimes overwritten by other code... */
        JSErrorReporter oldrep = JS_SetErrorReporter(context, jsd_ErrorReporter);
        if( jsd_ErrorReporter != oldrep )
            wrapper->originalErrorReporter = oldrep;
        return;
    }

    /* else... */
    wrapper = PR_NEWZAP(JSDContextWrapper);
    if( ! wrapper )
        return;

    if( ! PR_HashTableAdd(jsdc->jscontexts, context, wrapper ) )
    {
        PR_FREEIF(wrapper);
        return;
    }

    wrapper->context = context;
    wrapper->jsdc    = jsdc;

    /* add our error reporter */
    wrapper->originalErrorReporter = JS_SetErrorReporter(context, jsd_ErrorReporter);

    /* add our printer */
    /* add our loader */
}

JSD_ErrorReporter
jsd_SetErrorReporter( JSDContext* jsdc, JSD_ErrorReporter reporter, void* callerdata)
{
    JSD_ErrorReporter old = jsdc->errorReporter;

    jsdc->errorReporter = reporter;
    jsdc->errorReporterData =  callerdata;
    return old;
}

JSBool
jsd_IsCurrentThreadDangerous()
{
    return PR_CurrentThread() == _dangerousThread1;
}

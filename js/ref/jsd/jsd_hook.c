/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * JavaScript Debugger API - Hook support
 */

#include "jsd.h"

JSTrapStatus PR_CALLBACK
jsd_InterruptHandler(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval,
                     void *closure)
{
    uintN hookanswer = JSD_HOOK_RETURN_CONTINUE;
    JSDThreadState* jsdthreadstate;
    JSDScript*      jsdscript;
    JSDContext*     jsdc = (JSDContext*) closure;

    if( ! jsdc || ! jsdc->inited )
        return JSTRAP_CONTINUE;
    
    if( JSD_IS_DANGEROUS_THREAD(jsdc) )
        return JSTRAP_CONTINUE;

    jsd_JSContextUsed(jsdc, cx);

    JSD_LOCK_SCRIPTS(jsdc);
    jsdscript = jsd_FindJSDScript(jsdc, script);
    JSD_UNLOCK_SCRIPTS(jsdc);
    if( ! jsdscript )
        return JSTRAP_CONTINUE;

#ifdef LIVEWIRE
    if( ! jsdlw_UserCodeAtPC(jsdc, jsdscript, (pruword)pc) )
        return JSTRAP_CONTINUE;
#endif

    jsdthreadstate = jsd_NewThreadState(jsdc,cx);
    if( jsdthreadstate )
    {
        JSD_ExecutionHookProc hook;
        void*                 hookData;

        /* local in case jsdc->interruptHook gets cleared on another thread */

        JSD_LOCK();
        hook     = jsdc->interruptHook;
        hookData = jsdc->interruptHookData;
        JSD_UNLOCK();

        if(hook)
            hookanswer = hook(jsdc, jsdthreadstate, 
                              JSD_HOOK_INTERRUPTED, hookData);

        jsd_DestroyThreadState(jsdc, jsdthreadstate);
    }

    *rval = 0;           /* XXX extend this */

    if( JSD_HOOK_RETURN_ABORT == hookanswer )
        return JSTRAP_ERROR;

    return JSTRAP_CONTINUE; /* XXX extend this */
}

JSBool
jsd_SetInterruptHook(JSDContext*           jsdc, 
                     JSD_ExecutionHookProc hook,
                     void*                 callerdata)
{
    JSD_LOCK();
    jsdc->interruptHookData  = callerdata;
    jsdc->interruptHook      = hook;
    JS_SetInterrupt(jsdc->jstaskstate, jsd_InterruptHandler, (void*) jsdc);
    JSD_UNLOCK();

    return JS_TRUE;
}

JSBool
jsd_ClearInterruptHook(JSDContext* jsdc) 
{
    JSD_LOCK();
    JS_ClearInterrupt(jsdc->jstaskstate, NULL, NULL );
    jsdc->interruptHook      = NULL;
    JSD_UNLOCK();

    return JS_TRUE;
}

JSBool
jsd_SetDebugBreakHook(JSDContext*           jsdc, 
                      JSD_ExecutionHookProc hook,
                      void*                 callerdata)
{
    JSD_LOCK();
    jsdc->debugBreakHookData  = callerdata;
    jsdc->debugBreakHook      = hook;
    JSD_UNLOCK();

    return JS_TRUE;
}

JSBool
jsd_ClearDebugBreakHook(JSDContext* jsdc) 
{
    JSD_LOCK();
    jsdc->debugBreakHook      = NULL;
    JSD_UNLOCK();

    return JS_TRUE;
}

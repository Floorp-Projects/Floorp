/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
** JavaScript Debugger Navigator API - Hook support
*/

#include "jsd.h"

JSTrapStatus PR_CALLBACK
jsd_InterruptHandler(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval,
                     void *closure)
{
    PRUintn hookanswer = JSD_HOOK_RETURN_CONTINUE;
    JSDThreadState* jsdthreadstate;
    JSDScript*      jsdscript;
    JSDContext*     jsdc = (JSDContext*) closure;

    if( jsd_IsCurrentThreadDangerous() )
        return JSTRAP_CONTINUE;

    if( ! jsdc || ! jsdc->inited )
        return JSTRAP_CONTINUE;
    
    jsd_JSContextUsed(jsdc, cx);
    
    if( ! jsdc->interruptHook )
        return JSTRAP_CONTINUE;

    jsdscript = jsd_FindJSDScript(jsdc, script);
    if( ! jsdscript )
        return JSTRAP_CONTINUE;

    jsdthreadstate = jsd_NewThreadState(jsdc,cx);
    if( jsdthreadstate )
    {
        hookanswer =
            (*jsdc->interruptHook)(jsdc, jsdthreadstate, 
                                   JSD_HOOK_INTERRUPTED, 
                                   jsdc->interruptHookData );

        jsd_DestroyThreadState(jsdc, jsdthreadstate);
    }

    *rval = NULL;           /* XXX fix this!!! */

    if( JSD_HOOK_RETURN_ABORT == hookanswer )
        return JSTRAP_ERROR;

    return JSTRAP_CONTINUE; /* XXX fix this!!! */
}

JSBool
jsd_SetInterruptHook(JSDContext*           jsdc, 
                     JSD_ExecutionHookProc hook,
                     void*                 callerdata)
{
    jsdc->interruptHook      = hook;
    jsdc->interruptHookData  = callerdata;

    return JS_SetInterrupt(jsdc->jstaskstate, jsd_InterruptHandler, (void*) jsdc);
}

JSBool
jsd_ClearInterruptHook(JSDContext* jsdc) 
{
    jsdc->interruptHook      = NULL;
    return JS_ClearInterrupt(jsdc->jstaskstate, NULL, NULL );
}

JSBool
jsd_SetDebugBreakHook(JSDContext*           jsdc, 
                      JSD_ExecutionHookProc hook,
                      void*                 callerdata)
{
    jsdc->debugBreakHook      = hook;
    jsdc->debugBreakHookData  = callerdata;
    return JS_TRUE;
}

JSBool
jsd_ClearDebugBreakHook(JSDContext* jsdc) 
{
    jsdc->debugBreakHook      = NULL;
    return JS_TRUE;
}

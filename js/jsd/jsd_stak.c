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
** JavaScript Debugger Navigator API - Call Stack support
*/

#include "jsd.h"
#ifdef NSPR20
#ifdef XP_MAC
#include "prpriv.h"
#else
#include "private/prpriv.h"
#endif
#endif

#ifdef DEBUG
void JSD_ASSERT_VALID_THREAD_STATE( JSDThreadState* jsdthreadstate )
{
    PR_ASSERT(jsdthreadstate);
    PR_ASSERT(jsdthreadstate->stackDepth > 0);
}

void JSD_ASSERT_VALID_STACK_FRAME( JSDStackFrameInfo* jsdframe )
{
    PR_ASSERT(jsdframe);
    PR_ASSERT(jsdframe->jsdthreadstate);
}
#endif

static JSDStackFrameInfo* 
AddNewFrame( JSDContext*        jsdc,
             JSDThreadState*    jsdthreadstate,
             JSScript*          script,
             prword_t           pc,
             JSObject*          object,
             JSObject*          thisp,
             JSStackFrame*      fp )
{
    JSDStackFrameInfo* jsdframe;
    JSDScript*         jsdscript;

    jsdscript = jsd_FindJSDScript(jsdc, script);
    if( ! jsdscript )
        return NULL;

    jsdframe = PR_NEWZAP(JSDStackFrameInfo);
    if( ! jsdframe )
        return NULL;

    jsdframe->jsdthreadstate = jsdthreadstate;
    jsdframe->jsdscript      = jsdscript     ;
    jsdframe->pc             = pc            ;
    jsdframe->object         = object        ;
    jsdframe->thisp          = thisp         ;
    jsdframe->fp             = fp            ;

    PR_APPEND_LINK(&jsdframe->links, &jsdthreadstate->stack);
    jsdthreadstate->stackDepth++;

    return jsdframe;
}

static void
DestroyFrame(JSDStackFrameInfo* jsdframe)
{
    /* kill any alloc'd objects in frame here... */

    PR_FREEIF(jsdframe);
}


JSDThreadState*
jsd_NewThreadState(JSDContext* jsdc, JSContext *cx )
{
    JSDThreadState* jsdthreadstate;
    JSStackFrame *  iter = NULL;
    JSStackFrame *  fp;

    jsdthreadstate = PR_NEWZAP(JSDThreadState);
    if( ! jsdthreadstate )
        return NULL;

    jsdthreadstate->context = cx;
    jsdthreadstate->thread = PR_CurrentThread();
    PR_INIT_CLIST(&jsdthreadstate->stack);
    jsdthreadstate->stackDepth = 0;

    while( NULL != (fp = JS_FrameIterator(cx, &iter)) )
    {
        JSScript* script = JS_GetFrameScript(cx, fp);
        prword_t  pc = (prword_t) JS_GetFramePC(cx, fp);

        if( ! script || ! pc || JS_IsNativeFrame(cx, fp) )
            continue;

        AddNewFrame( jsdc, jsdthreadstate, script, pc,
                     JS_GetFrameObject(cx, fp),
                     JS_GetFrameThis(cx, fp), fp );
    }

    jsd_LockThreadsStates(jsdc);
    PR_APPEND_LINK(&jsdthreadstate->links, &jsdc->threadsStates);
    jsd_UnlockThreadStates(jsdc);

    return jsdthreadstate;
}

void
jsd_DestroyThreadState(JSDContext* jsdc, JSDThreadState* jsdthreadstate)
{
    JSDStackFrameInfo* jsdframe;
    PRCList* list;
    int64 i;

#ifndef NSPR20
    LL_I2L(i,100);
#endif

    /* we need to wait if someone else is using the threadstate */

    jsd_LockThreadsStates(jsdc);
    while( jsdthreadstate->wait )
    {
        jsd_UnlockThreadStates(jsdc);
#ifndef NSPR20
        PR_Sleep(i);
#else
        PR_Sleep(PR_MicrosecondsToInterval(100));
#endif
        jsd_LockThreadsStates(jsdc);
    }
    PR_REMOVE_LINK(&jsdthreadstate->links);
    jsd_UnlockThreadStates(jsdc);


    list = &jsdthreadstate->stack;
    while( (JSDStackFrameInfo*)list != (jsdframe = (JSDStackFrameInfo*)list->next) )
    {
        PR_REMOVE_LINK(&jsdframe->links);
        DestroyFrame(jsdframe);
    }
    PR_FREEIF(jsdthreadstate);
}

PRUintn
jsd_GetCountOfStackFrames(JSDContext* jsdc, JSDThreadState* jsdthreadstate)
{
    PRUintn count = 0;

    jsd_LockThreadsStates(jsdc);

    if( jsd_IsValidThreadState(jsdc, jsdthreadstate) )
        count = jsdthreadstate->stackDepth;

    jsd_UnlockThreadStates(jsdc);

    return count;
}

JSDStackFrameInfo*
jsd_GetStackFrame(JSDContext* jsdc, JSDThreadState* jsdthreadstate)
{
    JSDStackFrameInfo* jsdframe = NULL;

    jsd_LockThreadsStates(jsdc);

    if( jsd_IsValidThreadState(jsdc, jsdthreadstate) )
        jsdframe = (JSDStackFrameInfo*) PR_LIST_HEAD(&jsdthreadstate->stack);

    jsd_UnlockThreadStates(jsdc);

    return jsdframe;
}

JSDStackFrameInfo*
jsd_GetCallingStackFrame(JSDContext* jsdc, 
                         JSDThreadState* jsdthreadstate,
                         JSDStackFrameInfo* jsdframe)
{
    JSDStackFrameInfo* nextjsdframe = NULL;

    jsd_LockThreadsStates(jsdc);

    if( jsd_IsValidFrameInThreadState(jsdc, jsdthreadstate, jsdframe) )
        if( PR_LIST_HEAD(&jsdframe->links) != &jsdframe->jsdthreadstate->stack )
            nextjsdframe = (JSDStackFrameInfo*) PR_LIST_HEAD(&jsdframe->links);

    jsd_UnlockThreadStates(jsdc);

    return nextjsdframe;
}

JSDScript*
jsd_GetScriptForStackFrame(JSDContext* jsdc, 
                           JSDThreadState* jsdthreadstate,
                           JSDStackFrameInfo* jsdframe)
{
    JSDScript* jsdscript = NULL;

    jsd_LockThreadsStates(jsdc);

    if( jsd_IsValidFrameInThreadState(jsdc, jsdthreadstate, jsdframe) )
        jsdscript = jsdframe->jsdscript;

    jsd_UnlockThreadStates(jsdc);

    return jsdscript;
}

prword_t
jsd_GetPCForStackFrame(JSDContext* jsdc, 
                       JSDThreadState* jsdthreadstate,
                       JSDStackFrameInfo* jsdframe)
{
    prword_t pc = 0;

    jsd_LockThreadsStates(jsdc);

    if( jsd_IsValidFrameInThreadState(jsdc, jsdthreadstate, jsdframe) )
        pc = jsdframe->pc;

    jsd_UnlockThreadStates(jsdc);

    return pc;
}

JSBool
jsd_EvaluateScriptInStackFrame(JSDContext* jsdc, 
                               JSDThreadState* jsdthreadstate,
                               JSDStackFrameInfo* jsdframe,
                               const char *bytes, PRUintn length,
                               const char *filename, PRUintn lineno, jsval *rval)
{
    JSBool retval;

    jsd_LockThreadsStates(jsdc);

    if( ! jsd_IsValidFrameInThreadState(jsdc, jsdthreadstate, jsdframe) )
    {
        jsd_UnlockThreadStates(jsdc);
        return JS_FALSE;
    }
    jsdthreadstate->wait++ ;
    jsd_UnlockThreadStates(jsdc);

    retval = JS_EvaluateInStackFrame(jsdthreadstate->context, jsdframe->fp, 
                                     bytes, length, filename, lineno, rval);

    jsd_LockThreadsStates(jsdc);
    jsdthreadstate->wait-- ;
    jsd_UnlockThreadStates(jsdc);

    return retval;
}

JSString*
jsd_ValToStringInStackFrame(JSDContext* jsdc, 
                            JSDThreadState* jsdthreadstate,
                            JSDStackFrameInfo* jsdframe,
                            jsval val)
{
    JSString* jsstr = NULL;

    jsd_LockThreadsStates(jsdc);

    if( ! jsd_IsValidFrameInThreadState(jsdc, jsdthreadstate, jsdframe) )
    {
        jsd_UnlockThreadStates(jsdc);
        return NULL;
    }
    jsdthreadstate->wait++ ;
    jsd_UnlockThreadStates(jsdc);

    jsstr = JS_ValueToString(jsdthreadstate->context, val);

    jsd_LockThreadsStates(jsdc);
    jsdthreadstate->wait-- ;
    jsd_UnlockThreadStates(jsdc);

    return jsstr;
}


/***************************************************************************/

#ifndef JSD_SIMULATION
static PRMonitor *jsd_threadstate_mon = NULL; 
#endif /* JSD_SIMULATION */

void
jsd_LockThreadsStates(JSDContext* jsdc)
{
#ifndef JSD_SIMULATION
    if (jsd_threadstate_mon == NULL)
        jsd_threadstate_mon = PR_NewNamedMonitor("jsd-threadstate-monitor");

    PR_EnterMonitor(jsd_threadstate_mon);
#endif /* JSD_SIMULATION */
}

void
jsd_UnlockThreadStates(JSDContext* jsdc)
{
#ifndef JSD_SIMULATION
    PR_ExitMonitor(jsd_threadstate_mon);
#endif /* JSD_SIMULATION */
}

JSBool
jsd_ThreadStatesIsLocked(JSDContext* jsdc)
{
#ifndef JSD_SIMULATION
    return jsd_threadstate_mon && PR_InMonitor(jsd_threadstate_mon);
#else
    return JS_TRUE;
#endif /* JSD_SIMULATION */
}

JSBool
jsd_IsValidThreadState(JSDContext*        jsdc, 
                       JSDThreadState*    jsdthreadstate )
{
    JSDThreadState *cur;

    PR_ASSERT( jsd_ThreadStatesIsLocked(jsdc) );

    for (cur = (JSDThreadState*)jsdc->threadsStates.next;
         cur != (JSDThreadState*)&jsdc->threadsStates;
         cur = (JSDThreadState*)cur->links.next ) 
    {
        if( cur == jsdthreadstate )
            return JS_TRUE;
    }
    return JS_FALSE;
}    

JSBool
jsd_IsValidFrameInThreadState(JSDContext*        jsdc, 
                              JSDThreadState*    jsdthreadstate,
                              JSDStackFrameInfo* jsdframe )
{
    PR_ASSERT( jsd_ThreadStatesIsLocked(jsdc) );

    if( ! jsd_IsValidThreadState(jsdc, jsdthreadstate) )
        return JS_FALSE;
    if( jsdframe->jsdthreadstate != jsdthreadstate )
        return JS_FALSE;

    JSD_ASSERT_VALID_THREAD_STATE(jsdthreadstate);
    JSD_ASSERT_VALID_STACK_FRAME(jsdframe);
    
    return JS_TRUE;
}

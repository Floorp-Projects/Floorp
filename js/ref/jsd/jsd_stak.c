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
 * JavaScript Debugger API - Call Stack support
 */

#include "jsd.h"

#ifdef DEBUG
void JSD_ASSERT_VALID_THREAD_STATE(JSDThreadState* jsdthreadstate)
{
    PR_ASSERT(jsdthreadstate);
    PR_ASSERT(jsdthreadstate->stackDepth > 0);
}

void JSD_ASSERT_VALID_STACK_FRAME(JSDStackFrameInfo* jsdframe)
{
    PR_ASSERT(jsdframe);
    PR_ASSERT(jsdframe->jsdthreadstate);
}
#endif

static JSDStackFrameInfo* 
_addNewFrame(JSDContext*        jsdc,
             JSDThreadState*    jsdthreadstate,
             JSScript*          script,
             pruword            pc,
             JSObject*          object,
             JSObject*          thisp,
             JSStackFrame*      fp)
{
    JSDStackFrameInfo* jsdframe;
    JSDScript*         jsdscript;

    JSD_LOCK_SCRIPTS(jsdc);
    jsdscript = jsd_FindJSDScript(jsdc, script);
    JSD_UNLOCK_SCRIPTS(jsdc);
    if( ! jsdscript )
        return NULL;

    jsdframe = (JSDStackFrameInfo*) calloc(1, sizeof(JSDStackFrameInfo));
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
_destroyFrame(JSDStackFrameInfo* jsdframe)
{
    /* kill any alloc'd objects in frame here... */

    if( jsdframe )
        free(jsdframe);
}


JSDThreadState*
jsd_NewThreadState(JSDContext* jsdc, JSContext *cx )
{
    JSDThreadState* jsdthreadstate;
    JSStackFrame *  iter = NULL;
    JSStackFrame *  fp;

    jsdthreadstate = (JSDThreadState*)calloc(1, sizeof(JSDThreadState));
    if( ! jsdthreadstate )
        return NULL;

    jsdthreadstate->context = cx;
    jsdthreadstate->thread = JSD_CURRENT_THREAD();
    PR_INIT_CLIST(&jsdthreadstate->stack);
    jsdthreadstate->stackDepth = 0;

    while( NULL != (fp = JS_FrameIterator(cx, &iter)) )
    {
        JSScript* script = JS_GetFrameScript(cx, fp);
        pruword  pc = (pruword) JS_GetFramePC(cx, fp);

        if( ! script || ! pc || JS_IsNativeFrame(cx, fp) )
            continue;

        _addNewFrame( jsdc, jsdthreadstate, script, pc,
                     JS_GetFrameObject(cx, fp),
                     JS_GetFrameThis(cx, fp), fp );
    }
    
    /* if there is no stack, then this threadstate can not be constructed */
    if( 0 == jsdthreadstate->stackDepth )
    {
        free(jsdthreadstate);
        return NULL;
    }

    JSD_LOCK_THREADSTATES(jsdc);
    PR_APPEND_LINK(&jsdthreadstate->links, &jsdc->threadsStates);
    JSD_UNLOCK_THREADSTATES(jsdc);

    return jsdthreadstate;
}

void
jsd_DestroyThreadState(JSDContext* jsdc, JSDThreadState* jsdthreadstate)
{
    JSDStackFrameInfo* jsdframe;
    PRCList* list;

    JSD_ASSERT_VALID_THREAD_STATE(jsdthreadstate);
    PR_ASSERT(JSD_CURRENT_THREAD() == jsdthreadstate->thread);

    JSD_LOCK_THREADSTATES(jsdc);
    PR_REMOVE_LINK(&jsdthreadstate->links);
    JSD_UNLOCK_THREADSTATES(jsdc);

    list = &jsdthreadstate->stack;
    while( (JSDStackFrameInfo*)list != (jsdframe = (JSDStackFrameInfo*)list->next) )
    {
        PR_REMOVE_LINK(&jsdframe->links);
        _destroyFrame(jsdframe);
    }
    free(jsdthreadstate);
}

uintN
jsd_GetCountOfStackFrames(JSDContext* jsdc, JSDThreadState* jsdthreadstate)
{
    uintN count = 0;

    JSD_LOCK_THREADSTATES(jsdc);

    if( jsd_IsValidThreadState(jsdc, jsdthreadstate) )
        count = jsdthreadstate->stackDepth;

    JSD_UNLOCK_THREADSTATES(jsdc);

    return count;
}

JSDStackFrameInfo*
jsd_GetStackFrame(JSDContext* jsdc, JSDThreadState* jsdthreadstate)
{
    JSDStackFrameInfo* jsdframe = NULL;

    JSD_LOCK_THREADSTATES(jsdc);

    if( jsd_IsValidThreadState(jsdc, jsdthreadstate) )
        jsdframe = (JSDStackFrameInfo*) PR_LIST_HEAD(&jsdthreadstate->stack);
    JSD_UNLOCK_THREADSTATES(jsdc);

    return jsdframe;
}

JSDStackFrameInfo*
jsd_GetCallingStackFrame(JSDContext* jsdc, 
                         JSDThreadState* jsdthreadstate,
                         JSDStackFrameInfo* jsdframe)
{
    JSDStackFrameInfo* nextjsdframe = NULL;

    JSD_LOCK_THREADSTATES(jsdc);

    if( jsd_IsValidFrameInThreadState(jsdc, jsdthreadstate, jsdframe) )
        if( PR_LIST_HEAD(&jsdframe->links) != &jsdframe->jsdthreadstate->stack )
            nextjsdframe = (JSDStackFrameInfo*) PR_LIST_HEAD(&jsdframe->links);

    JSD_UNLOCK_THREADSTATES(jsdc);

    return nextjsdframe;
}

JSDScript*
jsd_GetScriptForStackFrame(JSDContext* jsdc, 
                           JSDThreadState* jsdthreadstate,
                           JSDStackFrameInfo* jsdframe)
{
    JSDScript* jsdscript = NULL;

    JSD_LOCK_THREADSTATES(jsdc);

    if( jsd_IsValidFrameInThreadState(jsdc, jsdthreadstate, jsdframe) )
        jsdscript = jsdframe->jsdscript;

    JSD_UNLOCK_THREADSTATES(jsdc);

    return jsdscript;
}

pruword
jsd_GetPCForStackFrame(JSDContext* jsdc, 
                       JSDThreadState* jsdthreadstate,
                       JSDStackFrameInfo* jsdframe)
{
    pruword pc = 0;

    JSD_LOCK_THREADSTATES(jsdc);

    if( jsd_IsValidFrameInThreadState(jsdc, jsdthreadstate, jsdframe) )
        pc = jsdframe->pc;

    JSD_UNLOCK_THREADSTATES(jsdc);

    return pc;
}

JSBool
jsd_EvaluateScriptInStackFrame(JSDContext* jsdc, 
                               JSDThreadState* jsdthreadstate,
                               JSDStackFrameInfo* jsdframe,
                               const char *bytes, uintN length,
                               const char *filename, uintN lineno, jsval *rval)
{
    JSBool retval;
    JSBool valid;

    PR_ASSERT(JSD_CURRENT_THREAD() == jsdthreadstate->thread);

    JSD_LOCK_THREADSTATES(jsdc);
    valid = jsd_IsValidFrameInThreadState(jsdc, jsdthreadstate, jsdframe);
    JSD_UNLOCK_THREADSTATES(jsdc);

    if( ! valid )
        return JS_FALSE;

    jsd_StartingEvalUsingFilename(jsdc, filename);
    retval = JS_EvaluateInStackFrame(jsdthreadstate->context, jsdframe->fp, 
                                     bytes, length, filename, lineno, rval);
    jsd_FinishedEvalUsingFilename(jsdc, filename);

    return retval;
}

JSString*
jsd_ValToStringInStackFrame(JSDContext* jsdc, 
                            JSDThreadState* jsdthreadstate,
                            JSDStackFrameInfo* jsdframe,
                            jsval val)
{
    JSBool valid;

    JSD_LOCK_THREADSTATES(jsdc);
    valid = jsd_IsValidFrameInThreadState(jsdc, jsdthreadstate, jsdframe);
    JSD_UNLOCK_THREADSTATES(jsdc);

    if( ! valid )
        return NULL;

    return JS_ValueToString(jsdthreadstate->context, val);
}

JSBool
jsd_IsValidThreadState(JSDContext*        jsdc, 
                       JSDThreadState*    jsdthreadstate)
{
    JSDThreadState *cur;

    PR_ASSERT( JSD_THREADSTATES_LOCKED(jsdc) );

    for( cur = (JSDThreadState*)jsdc->threadsStates.next;
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
                              JSDStackFrameInfo* jsdframe)
{
    PR_ASSERT(JSD_THREADSTATES_LOCKED(jsdc));

    if( ! jsd_IsValidThreadState(jsdc, jsdthreadstate) )
        return JS_FALSE;
    if( jsdframe->jsdthreadstate != jsdthreadstate )
        return JS_FALSE;

    JSD_ASSERT_VALID_THREAD_STATE(jsdthreadstate);
    JSD_ASSERT_VALID_STACK_FRAME(jsdframe);
    
    return JS_TRUE;
}

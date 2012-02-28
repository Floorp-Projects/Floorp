/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * JavaScript Debugging support - Call stack support
 */

#include "jsd.h"
#include "jsfriendapi.h"

#ifdef DEBUG
void JSD_ASSERT_VALID_THREAD_STATE(JSDThreadState* jsdthreadstate)
{
    JS_ASSERT(jsdthreadstate);
    JS_ASSERT(jsdthreadstate->stackDepth > 0);
}

void JSD_ASSERT_VALID_STACK_FRAME(JSDStackFrameInfo* jsdframe)
{
    JS_ASSERT(jsdframe);
    JS_ASSERT(jsdframe->jsdthreadstate);
}
#endif

static JSDStackFrameInfo* 
_addNewFrame(JSDContext*        jsdc,
             JSDThreadState*    jsdthreadstate,
             JSScript*          script,
             uintptr_t          pc,
             JSStackFrame*      fp)
{
    JSDStackFrameInfo* jsdframe;
    JSDScript*         jsdscript = NULL;

    if (JS_IsScriptFrame(jsdthreadstate->context, fp))
    {
        JSD_LOCK_SCRIPTS(jsdc);
        jsdscript = jsd_FindJSDScript(jsdc, script);
        JSD_UNLOCK_SCRIPTS(jsdc);
        if (!jsdscript || (jsdc->flags & JSD_HIDE_DISABLED_FRAMES &&
                           !JSD_IS_DEBUG_ENABLED(jsdc, jsdscript)))
        {
            return NULL;
        }

        if (!JSD_IS_DEBUG_ENABLED(jsdc, jsdscript))
            jsdthreadstate->flags |= TS_HAS_DISABLED_FRAME;
    }
    
    jsdframe = (JSDStackFrameInfo*) calloc(1, sizeof(JSDStackFrameInfo));
    if( ! jsdframe )
        return NULL;

    jsdframe->jsdthreadstate = jsdthreadstate;
    jsdframe->jsdscript      = jsdscript;
    jsdframe->pc             = pc;
    jsdframe->fp             = fp;

    JS_APPEND_LINK(&jsdframe->links, &jsdthreadstate->stack);
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
    JS_INIT_CLIST(&jsdthreadstate->stack);
    jsdthreadstate->stackDepth = 0;

    JS_BeginRequest(jsdthreadstate->context);
    while( NULL != (fp = JS_FrameIterator(cx, &iter)) )
    {
        JSScript* script = JS_GetFrameScript(cx, fp);
        uintptr_t  pc = (uintptr_t) JS_GetFramePC(cx, fp);
        jsval dummyThis;

        /*
         * don't construct a JSDStackFrame for dummy frames (those without a
         * |this| object, or native frames, if JSD_INCLUDE_NATIVE_FRAMES
         * isn't set.
         */
        if (JS_GetFrameThis(cx, fp, &dummyThis) &&
            ((jsdc->flags & JSD_INCLUDE_NATIVE_FRAMES) ||
             JS_IsScriptFrame(cx, fp)))
        {
            JSDStackFrameInfo *frame;

            frame = _addNewFrame( jsdc, jsdthreadstate, script, pc, fp );

            if ((jsdthreadstate->stackDepth == 0 && !frame) ||
                (jsdthreadstate->stackDepth == 1 && frame &&
                 frame->jsdscript && !JSD_IS_DEBUG_ENABLED(jsdc, frame->jsdscript)))
            {
                /*
                 * if we failed to create the first frame, or the top frame
                 * is not enabled for debugging, fail the entire thread state.
                 */
                JS_INIT_CLIST(&jsdthreadstate->links);
                JS_EndRequest(jsdthreadstate->context);
                jsd_DestroyThreadState(jsdc, jsdthreadstate);
                return NULL;
            }
        }
    }
    JS_EndRequest(jsdthreadstate->context);

    if (jsdthreadstate->stackDepth == 0)
    {
        free(jsdthreadstate);
        return NULL;
    }
    
    JSD_LOCK_THREADSTATES(jsdc);
    JS_APPEND_LINK(&jsdthreadstate->links, &jsdc->threadsStates);
    JSD_UNLOCK_THREADSTATES(jsdc);

    return jsdthreadstate;
}

void
jsd_DestroyThreadState(JSDContext* jsdc, JSDThreadState* jsdthreadstate)
{
    JSDStackFrameInfo* jsdframe;
    JSCList* list;

    JS_ASSERT(jsdthreadstate);
    JS_ASSERT(JSD_CURRENT_THREAD() == jsdthreadstate->thread);

    JSD_LOCK_THREADSTATES(jsdc);
    JS_REMOVE_LINK(&jsdthreadstate->links);
    JSD_UNLOCK_THREADSTATES(jsdc);

    list = &jsdthreadstate->stack;
    while( (JSDStackFrameInfo*)list != (jsdframe = (JSDStackFrameInfo*)list->next) )
    {
        JS_REMOVE_LINK(&jsdframe->links);
        _destroyFrame(jsdframe);
    }
    free(jsdthreadstate);
}

unsigned
jsd_GetCountOfStackFrames(JSDContext* jsdc, JSDThreadState* jsdthreadstate)
{
    unsigned count = 0;

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
        jsdframe = (JSDStackFrameInfo*) JS_LIST_HEAD(&jsdthreadstate->stack);
    JSD_UNLOCK_THREADSTATES(jsdc);

    return jsdframe;
}

JSContext *
jsd_GetJSContext (JSDContext* jsdc, JSDThreadState* jsdthreadstate)
{
    JSContext* cx = NULL;

    JSD_LOCK_THREADSTATES(jsdc);
    if( jsd_IsValidThreadState(jsdc, jsdthreadstate) )
        cx = jsdthreadstate->context;
    JSD_UNLOCK_THREADSTATES(jsdc);

    return cx;
}
    
JSDStackFrameInfo*
jsd_GetCallingStackFrame(JSDContext* jsdc, 
                         JSDThreadState* jsdthreadstate,
                         JSDStackFrameInfo* jsdframe)
{
    JSDStackFrameInfo* nextjsdframe = NULL;

    JSD_LOCK_THREADSTATES(jsdc);

    if( jsd_IsValidFrameInThreadState(jsdc, jsdthreadstate, jsdframe) )
        if( JS_LIST_HEAD(&jsdframe->links) != &jsdframe->jsdthreadstate->stack )
            nextjsdframe = (JSDStackFrameInfo*) JS_LIST_HEAD(&jsdframe->links);

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

uintptr_t
jsd_GetPCForStackFrame(JSDContext* jsdc, 
                       JSDThreadState* jsdthreadstate,
                       JSDStackFrameInfo* jsdframe)
{
    uintptr_t pc = 0;

    JSD_LOCK_THREADSTATES(jsdc);

    if( jsd_IsValidFrameInThreadState(jsdc, jsdthreadstate, jsdframe) )
        pc = jsdframe->pc;

    JSD_UNLOCK_THREADSTATES(jsdc);

    return pc;
}

JSDValue*
jsd_GetCallObjectForStackFrame(JSDContext* jsdc, 
                               JSDThreadState* jsdthreadstate,
                               JSDStackFrameInfo* jsdframe)
{
    JSObject* obj;
    JSDValue* jsdval = NULL;

    JSD_LOCK_THREADSTATES(jsdc);

    if( jsd_IsValidFrameInThreadState(jsdc, jsdthreadstate, jsdframe) )
    {
        obj = JS_GetFrameCallObject(jsdthreadstate->context, jsdframe->fp); 
        if(obj)                                                             
            jsdval = JSD_NewValue(jsdc, OBJECT_TO_JSVAL(obj));              
    }

    JSD_UNLOCK_THREADSTATES(jsdc);

    return jsdval;
}

JSDValue*
jsd_GetScopeChainForStackFrame(JSDContext* jsdc, 
                               JSDThreadState* jsdthreadstate,
                               JSDStackFrameInfo* jsdframe)
{
    JSObject* obj;
    JSDValue* jsdval = NULL;

    JSD_LOCK_THREADSTATES(jsdc);

    if( jsd_IsValidFrameInThreadState(jsdc, jsdthreadstate, jsdframe) )
    {
        JS_BeginRequest(jsdthreadstate->context);
        obj = JS_GetFrameScopeChain(jsdthreadstate->context, jsdframe->fp); 
        JS_EndRequest(jsdthreadstate->context);
        if(obj)                                                             
            jsdval = JSD_NewValue(jsdc, OBJECT_TO_JSVAL(obj));              
    }

    JSD_UNLOCK_THREADSTATES(jsdc);

    return jsdval;
}

JSDValue*
jsd_GetThisForStackFrame(JSDContext* jsdc, 
                         JSDThreadState* jsdthreadstate,
                         JSDStackFrameInfo* jsdframe)
{
    JSObject* obj;
    JSDValue* jsdval = NULL;
    JSD_LOCK_THREADSTATES(jsdc);

    if( jsd_IsValidFrameInThreadState(jsdc, jsdthreadstate, jsdframe) )
    {
        JSBool ok;
        jsval thisval;
        JS_BeginRequest(jsdthreadstate->context);
        ok = JS_GetFrameThis(jsdthreadstate->context, jsdframe->fp, &thisval);
        JS_EndRequest(jsdthreadstate->context);
        if(ok)
            jsdval = JSD_NewValue(jsdc, thisval);
    }

    JSD_UNLOCK_THREADSTATES(jsdc);
    return jsdval;
}

JSString*
jsd_GetIdForStackFrame(JSDContext* jsdc, 
                       JSDThreadState* jsdthreadstate,
                       JSDStackFrameInfo* jsdframe)
{
    JSString *rv = NULL;
    
    JSD_LOCK_THREADSTATES(jsdc);
    
    if( jsd_IsValidFrameInThreadState(jsdc, jsdthreadstate, jsdframe) )
    {
        JSFunction *fun = JS_GetFrameFunction (jsdthreadstate->context,
                                               jsdframe->fp);
        if( fun )
        {
            rv = JS_GetFunctionId (fun);

            /*
             * For compatibility we return "anonymous", not an empty string
             * here.
             */
            if( !rv )
                rv = JS_GetAnonymousString(jsdc->jsrt);
        }
    }
    
    JSD_UNLOCK_THREADSTATES(jsdc);
    return rv;
}

JSBool
jsd_IsStackFrameDebugger(JSDContext* jsdc, 
                         JSDThreadState* jsdthreadstate,
                         JSDStackFrameInfo* jsdframe)
{
    JSBool rv = JS_TRUE;
    JSD_LOCK_THREADSTATES(jsdc);

    if( jsd_IsValidFrameInThreadState(jsdc, jsdthreadstate, jsdframe) )
    {
        rv = JS_IsDebuggerFrame(jsdthreadstate->context, jsdframe->fp);
    }

    JSD_UNLOCK_THREADSTATES(jsdc);
    return rv;
}

JSBool
jsd_IsStackFrameConstructing(JSDContext* jsdc, 
                             JSDThreadState* jsdthreadstate,
                             JSDStackFrameInfo* jsdframe)
{
    JSBool rv = JS_TRUE;
    JSD_LOCK_THREADSTATES(jsdc);

    if( jsd_IsValidFrameInThreadState(jsdc, jsdthreadstate, jsdframe) )
    {
        rv = JS_IsConstructorFrame(jsdthreadstate->context, jsdframe->fp);
    }

    JSD_UNLOCK_THREADSTATES(jsdc);
    return rv;
}

JSBool
jsd_EvaluateUCScriptInStackFrame(JSDContext* jsdc, 
                                 JSDThreadState* jsdthreadstate,
                                 JSDStackFrameInfo* jsdframe,
                                 const jschar *bytes, unsigned length,
                                 const char *filename, unsigned lineno,
                                 JSBool eatExceptions, jsval *rval)
{
    JSBool retval;
    JSBool valid;
    JSExceptionState* exceptionState = NULL;
    JSContext* cx;

    JS_ASSERT(JSD_CURRENT_THREAD() == jsdthreadstate->thread);

    JSD_LOCK_THREADSTATES(jsdc);
    valid = jsd_IsValidFrameInThreadState(jsdc, jsdthreadstate, jsdframe);
    JSD_UNLOCK_THREADSTATES(jsdc);

    if( ! valid )
        return JS_FALSE;

    cx = jsdthreadstate->context;
    JS_ASSERT(cx);

    if (eatExceptions)
        exceptionState = JS_SaveExceptionState(cx);
    JS_ClearPendingException(cx);
    jsd_StartingEvalUsingFilename(jsdc, filename);
    retval = JS_EvaluateUCInStackFrame(cx, jsdframe->fp, bytes, length, 
                                       filename, lineno, rval);
    jsd_FinishedEvalUsingFilename(jsdc, filename);
    if (eatExceptions)
        JS_RestoreExceptionState(cx, exceptionState);

    return retval;
}

JSBool
jsd_EvaluateScriptInStackFrame(JSDContext* jsdc, 
                               JSDThreadState* jsdthreadstate,
                               JSDStackFrameInfo* jsdframe,
                               const char *bytes, unsigned length,
                               const char *filename, unsigned lineno,
                               JSBool eatExceptions, jsval *rval)
{
    JSBool retval;
    JSBool valid;
    JSExceptionState* exceptionState = NULL;
    JSContext *cx;

    JS_ASSERT(JSD_CURRENT_THREAD() == jsdthreadstate->thread);

    JSD_LOCK_THREADSTATES(jsdc);
    valid = jsd_IsValidFrameInThreadState(jsdc, jsdthreadstate, jsdframe);
    JSD_UNLOCK_THREADSTATES(jsdc);

    if (!valid)
        return JS_FALSE;

    cx = jsdthreadstate->context;
    JS_ASSERT(cx);

    if (eatExceptions)
        exceptionState = JS_SaveExceptionState(cx);
    JS_ClearPendingException(cx);
    jsd_StartingEvalUsingFilename(jsdc, filename);
    retval = JS_EvaluateInStackFrame(cx, jsdframe->fp, bytes, length,
                                     filename, lineno, rval);
    jsd_FinishedEvalUsingFilename(jsdc, filename);
    if (eatExceptions)
        JS_RestoreExceptionState(cx, exceptionState);

    return retval;
}

JSString*
jsd_ValToStringInStackFrame(JSDContext* jsdc, 
                            JSDThreadState* jsdthreadstate,
                            JSDStackFrameInfo* jsdframe,
                            jsval val)
{
    JSBool valid;
    JSString* retval;
    JSExceptionState* exceptionState;
    JSContext* cx;

    JSD_LOCK_THREADSTATES(jsdc);
    valid = jsd_IsValidFrameInThreadState(jsdc, jsdthreadstate, jsdframe);
    JSD_UNLOCK_THREADSTATES(jsdc);

    if( ! valid )
        return NULL;

    cx = jsdthreadstate->context;
    JS_ASSERT(cx);

    exceptionState = JS_SaveExceptionState(cx);
    retval = JS_ValueToString(cx, val);
    JS_RestoreExceptionState(cx, exceptionState);

    return retval;
}

JSBool
jsd_IsValidThreadState(JSDContext*        jsdc, 
                       JSDThreadState*    jsdthreadstate)
{
    JSDThreadState *cur;

    JS_ASSERT( JSD_THREADSTATES_LOCKED(jsdc) );

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
    JS_ASSERT(JSD_THREADSTATES_LOCKED(jsdc));

    if( ! jsd_IsValidThreadState(jsdc, jsdthreadstate) )
        return JS_FALSE;
    if( jsdframe->jsdthreadstate != jsdthreadstate )
        return JS_FALSE;

    JSD_ASSERT_VALID_THREAD_STATE(jsdthreadstate);
    JSD_ASSERT_VALID_STACK_FRAME(jsdframe);
    
    return JS_TRUE;
}

static JSContext*
_getContextForThreadState(JSDContext* jsdc, JSDThreadState* jsdthreadstate)
{
    JSBool valid;
    JSD_LOCK_THREADSTATES(jsdc);
    valid = jsd_IsValidThreadState(jsdc, jsdthreadstate);
    JSD_UNLOCK_THREADSTATES(jsdc);
    if( valid )
        return jsdthreadstate->context;
    return NULL;
}        

JSDValue*
jsd_GetException(JSDContext* jsdc, JSDThreadState* jsdthreadstate)
{
    JSContext* cx;
    jsval val;

    if(!(cx = _getContextForThreadState(jsdc, jsdthreadstate)))
        return NULL;

    if(JS_GetPendingException(cx, &val))
        return jsd_NewValue(jsdc, val);
    return NULL;
}        

JSBool
jsd_SetException(JSDContext* jsdc, JSDThreadState* jsdthreadstate, 
                 JSDValue* jsdval)
{
    JSContext* cx;

    if(!(cx = _getContextForThreadState(jsdc, jsdthreadstate)))
        return JS_FALSE;

    if(jsdval)
        JS_SetPendingException(cx, JSD_GetValueWrappedJSVal(jsdc, jsdval));
    else
        JS_ClearPendingException(cx);
    return JS_TRUE;
}


/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License. 
 *
 * The Original Code is mozilla.org code
 * 
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation
 * Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 *
 * Contributor(s):
 *   Robert Ginda, <rginda@netscape.com>
 *
 */

#include "jsd_xpc.h"

#include "nsIXPConnect.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsString.h"
#include "nsMemory.h"
#include "jsdebug.h"
#include "prmem.h"

/* XXX this stuff is used by NestEventLoop, a temporary hack to be refactored
 * later */
#include "nsWidgetsCID.h"
#include "nsIAppShell.h"
#include "nsIJSContextStack.h"

#define ASSERT_VALID_SCRIPT { if (!mValid) return NS_ERROR_NOT_AVAILABLE; }

static JSBool
jsds_GCCallbackProc (JSContext *cx, JSGCStatus status);

/*******************************************************************************
 * global vars
 *******************************************************************************/

static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

const char jsdServiceContractID[] = "@mozilla.org/js/jsd/debugger-service;1";

#ifdef DEBUG_verbose
PRUint32 gScriptCount = 0;
PRUint32 gValueCount  = 0;
#endif

static jsdService   *gJsds       = 0;
static JSGCCallback  gLastGCProc = jsds_GCCallbackProc;
static JSGCStatus    gGCStatus   = JSGC_END;
static struct DeadScript {
    PRCList     links;
    JSDContext *jsdc;
    jsdIScript *script;
} *gDeadScripts = 0;

/*******************************************************************************
 * c callbacks
 *******************************************************************************/

static JSBool
jsds_GCCallbackProc (JSContext *cx, JSGCStatus status)
{
    gGCStatus = status;
#ifdef DEBUG
    printf ("new gc status is %i\n", status);
#endif
    if (status == JSGC_END && gDeadScripts) {
        nsCOMPtr<jsdIScriptHook> hook = 0;   
        gJsds->GetScriptHook (getter_AddRefs(hook));
        if (hook)
        {
            DeadScript *ds;
            do {
#ifdef DEBUG_verbose  
                printf ("calling script delete hook.\n");
#endif
                ds = gDeadScripts;
            
                /* tell the user this script has been destroyed */
                hook->OnScriptDestroyed (ds->script);
                /* get next deleted script */
                gDeadScripts = NS_REINTERPRET_CAST(DeadScript *,
                                                   PR_NEXT_LINK(&ds->links));
                /* take ourselves out of the circular list */
                PR_REMOVE_LINK(&ds->links);
                /* addref came from the FromPtr call in jsds_ScriptHookProc */
                NS_RELEASE(ds->script);
                /* free the struct! */
                PR_Free(ds);
            } while (&gDeadScripts->links != &ds->links);
            /* keep going until we catch up with our tail */
        }
        gDeadScripts = 0;
    }
    
    if (gLastGCProc)
        return gLastGCProc (cx, status);
    
    return JS_TRUE;
}

static PRUint32
jsds_ExecutionHookProc (JSDContext* jsdc, JSDThreadState* jsdthreadstate,
                        uintN type, void* callerdata, jsval* rval)
{
    nsCOMPtr<jsdIExecutionHook> hook(0);
    PRUint32 hook_rv = JSD_HOOK_RETURN_CONTINUE;
    jsdIValue *js_rv = 0;    

    switch (type)
    {
        case JSD_HOOK_INTERRUPTED:
            gJsds->GetInterruptHook(getter_AddRefs(hook));
            break;
        case JSD_HOOK_DEBUG_REQUESTED:
            gJsds->GetErrorHook(getter_AddRefs(hook));
            break;
        case JSD_HOOK_DEBUGGER_KEYWORD:
            gJsds->GetDebuggerHook(getter_AddRefs(hook));
            break;
        case JSD_HOOK_BREAKPOINT:
            gJsds->GetBreakpointHook(getter_AddRefs(hook));
            break;
        case JSD_HOOK_THROW:
        {
            gJsds->GetThrowHook(getter_AddRefs(hook));
            if (hook) {
                JSDValue *jsdv = JSD_GetException (jsdc, jsdthreadstate);
                js_rv = jsdValue::FromPtr (jsdc, jsdv);
            }
            break;
        }
        default:
            NS_ASSERTION (0, "Unknown hook type.");
    }

    if (!hook)
        return NS_OK;
    
    JSDStackFrameInfo *frame = JSD_GetStackFrame (jsdc, jsdthreadstate);
    hook->OnExecute (jsdStackFrame::FromPtr(jsdc, jsdthreadstate, frame),
                     type, &js_rv, &hook_rv);

    if (hook_rv == JSD_HOOK_RETURN_RET_WITH_VAL ||
        hook_rv == JSD_HOOK_RETURN_THROW_WITH_VAL)
    {
        JSDValue *jsdv;
        js_rv->GetJSDValue (&jsdv);
        *rval = JSD_GetValueWrappedJSVal(jsdc, jsdv);
    }
    
    NS_IF_RELEASE(js_rv);
    return hook_rv;
}

static void
jsds_ScriptHookProc (JSDContext* jsdc, JSDScript* jsdscript, JSBool creating,
                     void* callerdata)
{
    if (creating) {
        jsdIScriptHook *hook = 0;
        
        gJsds->GetScriptHook (&hook);
        if (!hook)
            return;
            
        nsCOMPtr<jsdIScript> script = 
            getter_AddRefs(jsdScript::FromPtr(jsdc, jsdscript));
        hook->OnScriptCreated (script);
    } else {
#ifdef DEBUG_verbose  
        printf ("script deleted.\n");
#endif
        jsdIScript *jsdis = jsdScript::FromPtr(jsdc, jsdscript);
        /* the initial addref is owned by the DeadScript record */
        jsdis->Invalidate();

        if (gGCStatus == JSGC_END) {
            /* if GC *isn't* running, we can tell the user about the script
             * delete now. */
            nsCOMPtr<jsdIScriptHook> hook = 0;   
            gJsds->GetScriptHook (getter_AddRefs(hook));
            if (hook) {
#ifdef DEBUG_verbose
                printf ("calling script delete hook immediatly.\n");
#endif
                hook->OnScriptDestroyed (jsdis);
            }
        } else {
            /* if a GC *is* running, we've got to wait until it's done before
             * we can execute any JS, so we queue the notification in a PRCList
             * until GC tells us it's done. See jsds_GCCallbackProc(). */
            DeadScript *ds = PR_NEW(DeadScript);
            if (!ds) {
                NS_RELEASE(jsdis);
                return; /* NS_ERROR_OUT_OF_MEMORY */
            }
        
            ds->jsdc = jsdc;
            ds->script = jsdis;
        
            if (gDeadScripts)
                /* if the queue exists, add to it */
                PR_APPEND_LINK(&ds->links, &gDeadScripts->links);
            else {
                /* otherwise create the queue */
                PR_INIT_CLIST(&ds->links);
                gDeadScripts = ds;
            }
        }
    }
    
            
}

/*******************************************************************************
 * reflected jsd data structures
 *******************************************************************************/

/* Contexts */
/*
NS_IMPL_THREADSAFE_ISUPPORTS1(jsdContext, jsdIContext); 

NS_IMETHODIMP
jsdContext::GetJSDContext(JSDContext **_rval)
{
    *_rval = mCx;
    return NS_OK;
}
*/

/* Objects */
NS_IMPL_THREADSAFE_ISUPPORTS1(jsdObject, jsdIObject); 

NS_IMETHODIMP
jsdObject::GetJSDContext(JSDContext **_rval)
{
    *_rval = mCx;
    return NS_OK;
}

NS_IMETHODIMP
jsdObject::GetJSDObject(JSDObject **_rval)
{
    *_rval = mObject;
    return NS_OK;
}

NS_IMETHODIMP
jsdObject::GetCreatorURL(char **_rval)
{
    *_rval = nsCString(JSD_GetObjectNewURL(mCx, mObject)).ToNewCString();
    return NS_OK;
}

NS_IMETHODIMP
jsdObject::GetCreatorLine(PRUint32 *_rval)
{
    *_rval = JSD_GetObjectNewLineNumber(mCx, mObject);
    return NS_OK;
}

NS_IMETHODIMP
jsdObject::GetConstructorURL(char **_rval)
{
    *_rval = nsCString(JSD_GetObjectConstructorURL(mCx, mObject)).ToNewCString();
    return NS_OK;
}

NS_IMETHODIMP
jsdObject::GetConstructorLine(PRUint32 *_rval)
{
    *_rval = JSD_GetObjectConstructorLineNumber(mCx, mObject);
    return NS_OK;
}

NS_IMETHODIMP
jsdObject::GetValue(jsdIValue **_rval)
{
    JSDValue *jsdv = JSD_GetValueForObject (mCx, mObject);
    
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    return NS_OK;
}

/* PC */
NS_IMPL_THREADSAFE_ISUPPORTS1(jsdPC, jsdIPC); 

NS_IMETHODIMP
jsdPC::GetPc(jsuword *_rval)
{
    *_rval = mPC;
    return NS_OK;
}

/* Properties */
NS_IMPL_THREADSAFE_ISUPPORTS1(jsdProperty, jsdIProperty); 

NS_IMETHODIMP
jsdProperty::GetJSDContext(JSDContext **_rval)
{
    *_rval = mCx;
    return NS_OK;
}

NS_IMETHODIMP
jsdProperty::GetJSDProperty(JSDProperty **_rval)
{
    *_rval = mProperty;
    return NS_OK;
}

NS_IMETHODIMP
jsdProperty::GetAlias(jsdIValue **_rval)
{
    JSDValue *jsdv = JSD_GetPropertyValue (mCx, mProperty);
    
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    return NS_OK;
}

NS_IMETHODIMP
jsdProperty::GetFlags(PRUint32 *_rval)
{
    *_rval = JSD_GetPropertyFlags (mCx, mProperty);
    return NS_OK;
}

NS_IMETHODIMP
jsdProperty::GetName(jsdIValue **_rval)
{
    JSDValue *jsdv = JSD_GetPropertyName (mCx, mProperty);
    
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    return NS_OK;
}

NS_IMETHODIMP
jsdProperty::GetValue(jsdIValue **_rval)
{
    JSDValue *jsdv = JSD_GetPropertyValue (mCx, mProperty);
    
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    return NS_OK;
}

NS_IMETHODIMP
jsdProperty::GetVarArgSlot(PRUint32 *_rval)
{
    *_rval = JSD_GetPropertyVarArgSlot (mCx, mProperty);
    return NS_OK;
}

/* Scripts */
NS_IMPL_THREADSAFE_ISUPPORTS1(jsdScript, jsdIScript); 

NS_IMETHODIMP
jsdScript::GetJSDContext(JSDContext **_rval)
{
    ASSERT_VALID_SCRIPT;
    *_rval = mCx;
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetJSDScript(JSDScript **_rval)
{
    ASSERT_VALID_SCRIPT;
    *_rval = mScript;
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::Invalidate()
{
    ASSERT_VALID_SCRIPT;
    
    /* release the addref we do in FromPtr */
    jsdIScript *script = NS_STATIC_CAST(jsdIScript *,
                                        JSD_GetScriptPrivate(mScript));
    NS_ASSERTION (script == this, "That's not my script!");
    NS_RELEASE(script);
    return NS_OK;
}
    
NS_IMETHODIMP
jsdScript::GetIsActive(PRBool *_rval)
{
    ASSERT_VALID_SCRIPT;
    JSD_LockScriptSubsystem(mCx);
    *_rval = JSD_IsActiveScript(mCx, mScript);
    JSD_UnlockScriptSubsystem(mCx);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetFileName(char **_rval)
{
    ASSERT_VALID_SCRIPT;
    JSD_LockScriptSubsystem(mCx);
    *_rval = nsCString(JSD_GetScriptFilename(mCx, mScript)).ToNewCString();
    JSD_UnlockScriptSubsystem(mCx);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetFunctionName(char **_rval)
{
    ASSERT_VALID_SCRIPT;
    JSD_LockScriptSubsystem(mCx);
    *_rval = nsCString(JSD_GetScriptFunctionName(mCx, mScript)).ToNewCString();
    JSD_UnlockScriptSubsystem(mCx);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetBaseLineNumber(PRUint32 *_rval)
{
    ASSERT_VALID_SCRIPT;
    JSD_LockScriptSubsystem(mCx);
    *_rval = JSD_GetScriptBaseLineNumber(mCx, mScript);
    JSD_UnlockScriptSubsystem(mCx);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetLineExtent(PRUint32 *_rval)
{
    ASSERT_VALID_SCRIPT;
    JSD_LockScriptSubsystem(mCx);
    *_rval = JSD_GetScriptLineExtent(mCx, mScript);
    JSD_UnlockScriptSubsystem(mCx);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::PcToLine(jsdIPC *aPC, PRUint32 *_rval)
{
    ASSERT_VALID_SCRIPT;
    jsuword pc;
    aPC->GetPc(&pc);
    *_rval = JSD_GetClosestLine (mCx, mScript, pc);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::LineToPc(PRUint32 aLine, jsdIPC **_rval)
{
    ASSERT_VALID_SCRIPT;
    jsuword pc = JSD_GetClosestPC (mCx, mScript, aLine);
    *_rval = jsdPC::FromPtr (pc);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::SetBreakpoint(jsdIPC *aPC)
{
    ASSERT_VALID_SCRIPT;
    jsuword pc;
    aPC->GetPc (&pc);
    
    JSD_SetExecutionHook (mCx, mScript, pc, jsds_ExecutionHookProc,
                          NS_REINTERPRET_CAST(void *, PRIVATE_TO_JSVAL(NULL)));
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::ClearBreakpoint(jsdIPC *aPC)
{
    ASSERT_VALID_SCRIPT;
    
    if (!aPC)
        return NS_ERROR_INVALID_ARG;
    
    jsuword pc;
    aPC->GetPc (&pc);
    
    JSD_ClearExecutionHook (mCx, mScript, pc);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::ClearAllBreakpoints()
{
    ASSERT_VALID_SCRIPT;
    JSD_LockScriptSubsystem(mCx);
    JSD_ClearAllExecutionHooksForScript (mCx, mScript);
    JSD_UnlockScriptSubsystem(mCx);
    return NS_OK;
}

/* Stack Frames */
NS_IMPL_THREADSAFE_ISUPPORTS1(jsdStackFrame, jsdIStackFrame); 

NS_IMETHODIMP
jsdStackFrame::GetJSDContext(JSDContext **_rval)
{
    *_rval = mCx;
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetJSDThreadState(JSDThreadState **_rval)
{
    *_rval = mThreadState;
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetJSDStackFrameInfo(JSDStackFrameInfo **_rval)
{
    *_rval = mStackFrameInfo;
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetCallingFrame(jsdIStackFrame **_rval)
{
    JSDStackFrameInfo *sfi = JSD_GetCallingStackFrame (mCx, mThreadState,
                                                       mStackFrameInfo);
    *_rval = jsdStackFrame::FromPtr (mCx, mThreadState, sfi);
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetScript(jsdIScript **_rval)
{
    JSDScript *script = JSD_GetScriptForStackFrame (mCx, mThreadState,
                                                    mStackFrameInfo);
    *_rval = jsdScript::FromPtr (mCx, script);
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetPc(jsdIPC **_rval)
{
    jsuword pc;
    pc = JSD_GetPCForStackFrame (mCx, mThreadState, mStackFrameInfo);
    *_rval = jsdPC::FromPtr (pc);
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetLine(PRUint32 *_rval)
{
    JSDScript *script = JSD_GetScriptForStackFrame (mCx, mThreadState,
                                                    mStackFrameInfo);
    jsuword pc = JSD_GetPCForStackFrame (mCx, mThreadState, mStackFrameInfo);
    *_rval = JSD_GetClosestLine (mCx, script, pc);
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetCallee(jsdIValue **_rval)
{
    JSDValue *jsdv = JSD_GetCallObjectForStackFrame (mCx, mThreadState,
                                                     mStackFrameInfo);
    
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetScope(jsdIValue **_rval)
{
    JSDValue *jsdv = JSD_GetScopeChainForStackFrame (mCx, mThreadState,
                                                     mStackFrameInfo);
    
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetThisValue(jsdIValue **_rval)
{
    JSDValue *jsdv = JSD_GetThisForStackFrame (mCx, mThreadState,
                                               mStackFrameInfo);
    
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    return NS_OK;
}


NS_IMETHODIMP
jsdStackFrame::Eval (const nsAReadableString &bytes, const char *fileName,
                     PRUint32 line, jsdIValue **_rval)
{
    jsval jv;

    const nsSharedBufferHandle<PRUnichar> *h = bytes.GetSharedBufferHandle();
    const jschar *char_bytes = NS_REINTERPRET_CAST(const jschar *,
                                                   h->DataStart());

    if (!JSD_EvaluateUCScriptInStackFrame (mCx, mThreadState, mStackFrameInfo,
                                           char_bytes, bytes.Length(), fileName,
                                           line, &jv))
        return NS_ERROR_FAILURE;
    
    JSDValue *jsdv = JSD_NewValue (mCx, jv);
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    return NS_OK;
}        

/* Values */
NS_IMPL_THREADSAFE_ISUPPORTS1(jsdValue, jsdIValue);

NS_IMETHODIMP
jsdValue::GetJSDContext(JSDContext **_rval)
{
    *_rval = mCx;
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetJSDValue (JSDValue **_rval)
{
    *_rval = mValue;
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetIsNative (PRBool *_rval)
{
    *_rval = JSD_IsValueNative (mCx, mValue);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetIsNumber (PRBool *_rval)
{
    *_rval = JSD_IsValueNumber (mCx, mValue);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetIsPrimitive (PRBool *_rval)
{
    *_rval = JSD_IsValuePrimitive (mCx, mValue);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetJsType (PRUint32 *_rval)
{
    /* XXX surely this can be done better. */
    if (JSD_IsValueBoolean(mCx, mValue))
        *_rval = TYPE_BOOLEAN;
    else if (JSD_IsValueDouble(mCx, mValue))
        *_rval = TYPE_DOUBLE;
    else if (JSD_IsValueInt(mCx, mValue))
        *_rval = TYPE_INT;
    else if (JSD_IsValueFunction(mCx, mValue))
        *_rval = TYPE_FUNCTION;
    else if (JSD_IsValueNull(mCx, mValue))
        *_rval = TYPE_NULL;
    else if (JSD_IsValueObject(mCx, mValue))
        *_rval = TYPE_OBJECT;
    else if (JSD_IsValueString(mCx, mValue))
        *_rval = TYPE_STRING;
    else if (JSD_IsValueVoid(mCx, mValue))
        *_rval = TYPE_VOID;
    else
        *_rval = TYPE_UNKNOWN;    

    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetJsPrototype (jsdIValue **_rval)
{
    JSDValue *jsdv = JSD_GetValuePrototype (mCx, mValue);
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetJsParent (jsdIValue **_rval)
{
    JSDValue *jsdv = JSD_GetValueParent (mCx, mValue);
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetJsClassName(char **_rval)
{
    *_rval = nsCString(JSD_GetValueClassName(mCx, mValue)).ToNewCString();
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetJsConstructor (jsdIValue **_rval)
{
    JSDValue *jsdv = JSD_GetValueConstructor (mCx, mValue);
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetJsFunctionName(char **_rval)
{
    *_rval = nsCString(JSD_GetValueFunctionName(mCx, mValue)).ToNewCString();
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetBooleanValue(PRBool *_rval)
{
    *_rval = JSD_GetValueBoolean (mCx, mValue);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetDoubleValue(double *_rval)
{
    _rval = JSD_GetValueDouble (mCx, mValue);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetIntValue(PRInt32 *_rval)
{
    *_rval = JSD_GetValueInt (mCx, mValue);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetObjectValue(jsdIObject **_rval)
{
    JSDObject *obj;
    obj = JSD_GetObjectForValue (mCx, mValue);
    *_rval = jsdObject::FromPtr (mCx, obj);
    return NS_OK;
}
    
NS_IMETHODIMP
jsdValue::GetStringValue(char **_rval)
{
    JSString *jstr_val = JSD_GetValueString(mCx, mValue);
    *_rval = nsCString(JS_GetStringBytes(jstr_val)).ToNewCString();
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetPropertyCount (PRInt32 *_rval)
{
    if (JSD_IsValueObject(mCx, mValue))
        *_rval = JSD_GetCountOfProperties (mCx, mValue);
    else
        *_rval = -1;
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetProperties (jsdIProperty ***propArray, PRUint32 *length)
{
    if (!JSD_IsValueObject(mCx, mValue)) {
        *length = 0;
        *propArray = 0;
        return NS_OK;
    }

    jsdIProperty **pa_temp;
    PRUint32 prop_count = JSD_GetCountOfProperties (mCx, mValue);
    
    pa_temp = NS_STATIC_CAST(jsdIProperty **,
                             nsMemory::Alloc(sizeof (jsdIProperty *) * 
                                             prop_count));

    PRUint32     i    = 0;
    JSDProperty *iter = NULL;
    JSDProperty *prop;
    while ((prop = JSD_IterateProperties (mCx, mValue, &iter))) {
        pa_temp[i] = jsdProperty::FromPtr (mCx, prop);
        ++i;
    }
    
    NS_ASSERTION (prop_count == i, "property count mismatch");    

    /* if caller doesn't care about length, don't bother telling them */
    *propArray = pa_temp;
    if (length)
        *length = prop_count;
    
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetProperty (const char *name, jsdIProperty **_rval)
{
    JSContext *cx = JSD_GetDefaultJSContext (mCx);
    /* not rooting this */
    JSString *jstr_name = JS_NewStringCopyZ (cx, name);

    JSDProperty *prop = JSD_GetValueProperty (mCx, mValue, jstr_name);
    
    *_rval = jsdProperty::FromPtr (mCx, prop);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::Refresh()
{
    JSD_RefreshValue (mCx, mValue);
    return NS_OK;
}

/******************************************************************************
 * debugger service implementation
 ******************************************************************************/
NS_IMPL_THREADSAFE_ISUPPORTS1(jsdService, jsdIDebuggerService); 

jsdService::jsdService() : mOn(PR_FALSE), mNestedLoopLevel(0), mCx(0),
                           mBreakpointHook(0), mErrorHook(0),
                           mDebuggerHook(0), mInterruptHook(0), mScriptHook(0),
                           mThrowHook(0)
{
    NS_INIT_ISUPPORTS();
}

NS_IMETHODIMP
jsdService::On (void)
{
    nsresult  rv;

    /* get JS things from the CallContext */
    nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID());
    if (!xpc) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIXPCNativeCallContext> cc;
    rv = xpc->GetCurrentNativeCallContext(getter_AddRefs(cc));
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    JSContext *cx;
    rv = cc->GetJSContext (&cx);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
    
    JSRuntime *rt = JS_GetRuntime (cx);

    if (gLastGCProc == jsds_GCCallbackProc)
        /* condition indicates that the callback proc has not been set yet */
        gLastGCProc = JS_SetGCCallbackRT (rt, jsds_GCCallbackProc);
        
    mCx = JSD_DebuggerOnForUser (rt, NULL, NULL);
    if (!mCx)
        return NS_ERROR_FAILURE;
    
    mOn = PR_TRUE;
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::Off (void)
{
    JSD_DebuggerOff (mCx);
    mOn = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
jsdService::ClearAllBreakpoints (void)
{
    JSD_LockScriptSubsystem(mCx);
    JSD_ClearAllExecutionHooks (mCx);
    JSD_UnlockScriptSubsystem(mCx);
    return NS_OK;
}

NS_IMETHODIMP
jsdService::EnterNestedEventLoop (PRUint32 *_rval)
{
    nsCOMPtr<nsIAppShell> appShell(do_CreateInstance(kAppShellCID));
    NS_ENSURE_TRUE(appShell, NS_ERROR_FAILURE);

    appShell->Create(0, nsnull);
    appShell->Spinup();
    // Store locally so it doesn't die on us

    nsCOMPtr<nsIJSContextStack> 
        stack(do_GetService("@mozilla.org/js/xpc/ContextStack;1"));
    nsresult rv = NS_OK;
    PRUint32 nestLevel = ++mNestedLoopLevel;
    
    if(stack && NS_SUCCEEDED(stack->Push(nsnull)))
    {
        while(NS_SUCCEEDED(rv) && mNestedLoopLevel >= nestLevel)
        {
            void* data;
            PRBool isRealEvent;
            //PRBool processEvent;
            
            rv = appShell->GetNativeEvent(isRealEvent, data);
            if(NS_SUCCEEDED(rv))
            {
                appShell->DispatchNativeEvent(isRealEvent, data);
            }
        }
        JSContext* cx;
        stack->Pop(&cx);
        NS_ASSERTION(cx == nsnull, "JSContextStack mismatch");
    }
    else
        rv = NS_ERROR_FAILURE;
    
    appShell->Spindown();

    NS_ASSERTION (mNestedLoopLevel <= nestLevel,
                  "nested event didn't unwind properly");
    if (mNestedLoopLevel == nestLevel)
        --mNestedLoopLevel;

    *_rval = mNestedLoopLevel;
    return rv;
}

NS_IMETHODIMP
jsdService::ExitNestedEventLoop (PRUint32 *_rval)
{
    if (mNestedLoopLevel > 0)
        --mNestedLoopLevel;
    else
        return NS_ERROR_FAILURE;

    *_rval = mNestedLoopLevel;    
    return NS_OK;
}    

/* hook attribute get/set functions */

NS_IMETHODIMP
jsdService::SetBreakpointHook (jsdIExecutionHook *aHook)
{    
    mBreakpointHook = aHook;
    return NS_OK;
}

NS_IMETHODIMP
jsdService::GetBreakpointHook (jsdIExecutionHook **aHook)
{   
    *aHook = mBreakpointHook;
    NS_IF_ADDREF(*aHook);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::SetErrorHook (jsdIExecutionHook *aHook)
{    
    mErrorHook = aHook;
    if (aHook)
        JSD_SetDebugBreakHook (mCx, jsds_ExecutionHookProc, NULL);
    else
        JSD_ClearDebugBreakHook (mCx);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::GetErrorHook (jsdIExecutionHook **aHook)
{   
    *aHook = mErrorHook;
    NS_IF_ADDREF(*aHook);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::SetDebuggerHook (jsdIExecutionHook *aHook)
{    
    mDebuggerHook = aHook;
    if (aHook)
        JSD_SetDebuggerHook (mCx, jsds_ExecutionHookProc, NULL);
    else
        JSD_ClearDebuggerHook (mCx);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::GetDebuggerHook (jsdIExecutionHook **aHook)
{   
    *aHook = mDebuggerHook;
    NS_IF_ADDREF(*aHook);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::SetInterruptHook (jsdIExecutionHook *aHook)
{    
    mInterruptHook = aHook;
    if (aHook)
        JSD_SetInterruptHook (mCx, jsds_ExecutionHookProc, NULL);
    else
        JSD_ClearInterruptHook (mCx);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::GetInterruptHook (jsdIExecutionHook **aHook)
{   
    *aHook = mInterruptHook;
    NS_IF_ADDREF(*aHook);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::SetScriptHook (jsdIScriptHook *aHook)
{    
    mScriptHook = aHook;
    if (aHook)
        JSD_SetScriptHook (mCx, jsds_ScriptHookProc, NULL);
    else
        JSD_SetScriptHook (mCx, NULL, NULL);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::GetScriptHook (jsdIScriptHook **aHook)
{   
    *aHook = mScriptHook;
    NS_IF_ADDREF(*aHook);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::SetThrowHook (jsdIExecutionHook *aHook)
{    
    mThrowHook = aHook;
    if (aHook)
        JSD_SetThrowHook (mCx, jsds_ExecutionHookProc, NULL);
    else
        JSD_ClearThrowHook (mCx);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::GetThrowHook (jsdIExecutionHook **aHook)
{   
    *aHook = mThrowHook;
    NS_IF_ADDREF(*aHook);
    
    return NS_OK;
}

jsdService *
jsdService::GetService ()
{
    if (!gJsds)
        gJsds = new jsdService();
        
    NS_IF_ADDREF(gJsds);
    return gJsds;
}

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(jsdService, jsdService::GetService);

static nsModuleComponentInfo components[] = {
    {"JSDService", JSDSERVICE_CID, jsdServiceContractID, jsdServiceConstructor}
};

NS_IMPL_NSGETMODULE("JavaScript Debugger", components);


/* graveyard */

#if 0
/* Thread States */
NS_IMPL_THREADSAFE_ISUPPORTS1(jsdThreadState, jsdIThreadState); 

NS_IMETHODIMP
jsdThreadState::GetJSDContext(JSDContext **_rval)
{
    *_rval = mCx;
    return NS_OK;
}

NS_IMETHODIMP
jsdThreadState::GetJSDThreadState(JSDThreadState **_rval)
{
    *_rval = mThreadState;
    return NS_OK;
}

NS_IMETHODIMP
jsdThreadState::GetFrameCount (PRUint32 *_rval)
{
    *_rval = JSD_GetCountOfStackFrames (mCx, mThreadState);
    return NS_OK;
}

NS_IMETHODIMP
jsdThreadState::GetTopFrame (jsdIStackFrame **_rval)
{
    JSDStackFrameInfo *sfi = JSD_GetStackFrame (mCx, mThreadState);
    
    *_rval = jsdStackFrame::FromPtr (mCx, mThreadState, sfi);
    return NS_OK;
}

NS_IMETHODIMP
jsdThreadState::GetPendingException(jsdIValue **_rval)
{
    JSDValue *jsdv = JSD_GetException (mCx, mThreadState);
    
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    return NS_OK;
}

NS_IMETHODIMP
jsdThreadState::SetPendingException(jsdIValue *aException)
{
    JSDValue *jsdv;
    
    nsresult rv = aException->GetJSDValue (&jsdv);
    if (NS_FAILED(rv))
        return NS_ERROR_FAILURE;
    
    if (!JSD_SetException (mCx, mThreadState, jsdv))
        return NS_ERROR_FAILURE;

    return NS_OK;
}

#endif

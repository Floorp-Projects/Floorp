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

static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

const char jsdServiceContractID[] = "@mozilla.org/js/jsd/debugger-service;1";

/*******************************************************************************
 * reflected jsd data structures
 *******************************************************************************/

/* Contexts */
NS_IMPL_THREADSAFE_ISUPPORTS1(jsdContext, jsdIContext); 

NS_IMETHODIMP
jsdContext::GetJSDContext(JSDContext **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = mCx;
    return NS_OK;
}

/* Objects */
NS_IMPL_THREADSAFE_ISUPPORTS1(jsdObject, jsdIObject); 

NS_IMETHODIMP
jsdObject::GetJSDContext(JSDContext **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = mCx;
    return NS_OK;
}

NS_IMETHODIMP
jsdObject::GetJSDObject(JSDObject **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = mObject;
    return NS_OK;
}

/* Properties */
NS_IMPL_THREADSAFE_ISUPPORTS1(jsdProperty, jsdIProperty); 

NS_IMETHODIMP
jsdProperty::GetJSDContext(JSDContext **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = mCx;
    return NS_OK;
}

NS_IMETHODIMP
jsdProperty::GetJSDProperty(JSDProperty **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = mProperty;
    return NS_OK;
}

NS_IMETHODIMP
jsdProperty::GetAlias(jsdIValue **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    JSDValue *jsdv = JSD_GetPropertyValue (mCx, mProperty);
    
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    NS_IF_ADDREF(*_rval);
    return NS_OK;
}

NS_IMETHODIMP
jsdProperty::GetFlags(PRUint32 *_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = JSD_GetPropertyFlags (mCx, mProperty);
    return NS_OK;
}

NS_IMETHODIMP
jsdProperty::GetName(jsdIValue **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    JSDValue *jsdv = JSD_GetPropertyName (mCx, mProperty);
    
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    NS_IF_ADDREF(*_rval);
    return NS_OK;
}

NS_IMETHODIMP
jsdProperty::GetValue(jsdIValue **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    JSDValue *jsdv = JSD_GetPropertyValue (mCx, mProperty);
    
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    NS_IF_ADDREF(*_rval);
    return NS_OK;
}

NS_IMETHODIMP
jsdProperty::GetVarArgSlot(PRUint32 *_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = JSD_GetPropertyVarArgSlot (mCx, mProperty);
    return NS_OK;
}

/* Scripts */
NS_IMPL_THREADSAFE_ISUPPORTS1(jsdScript, jsdIScript); 

NS_IMETHODIMP
jsdScript::GetJSDContext(JSDContext **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = mCx;
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetJSDScript(JSDScript **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = mScript;
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetIsActive(PRBool *_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    JSD_LockScriptSubsystem(mCx);
    *_rval = JSD_IsActiveScript(mCx, mScript);
    JSD_UnlockScriptSubsystem(mCx);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetFileName(char **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    JSD_LockScriptSubsystem(mCx);
    *_rval = nsCString(JSD_GetScriptFilename(mCx, mScript)).ToNewCString();
    JSD_UnlockScriptSubsystem(mCx);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetFunctionName(char **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    JSD_LockScriptSubsystem(mCx);
    *_rval = nsCString(JSD_GetScriptFunctionName(mCx, mScript)).ToNewCString();
    JSD_UnlockScriptSubsystem(mCx);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetBaseLineNumber(PRUint32 *_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    JSD_LockScriptSubsystem(mCx);
    *_rval = JSD_GetScriptBaseLineNumber(mCx, mScript);
    JSD_UnlockScriptSubsystem(mCx);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetLineExtent(PRUint32 *_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    JSD_LockScriptSubsystem(mCx);
    *_rval = JSD_GetScriptLineExtent(mCx, mScript);
    JSD_UnlockScriptSubsystem(mCx);
    return NS_OK;
}

/* Stack Frames */
NS_IMPL_THREADSAFE_ISUPPORTS1(jsdStackFrame, jsdIStackFrame); 

NS_IMETHODIMP
jsdStackFrame::GetJSDContext(JSDContext **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = mCx;
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetJSDThreadState(JSDThreadState **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = mThreadState;
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetJSDStackFrameInfo(JSDStackFrameInfo **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = mStackFrameInfo;
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetCallingFrame(jsdIStackFrame **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    JSDStackFrameInfo *sfi = JSD_GetCallingStackFrame (mCx, mThreadState,
                                                       mStackFrameInfo);
    *_rval = jsdStackFrame::FromPtr (mCx, mThreadState, sfi);
    NS_IF_ADDREF(*_rval);
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetScript(jsdIScript **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;

    JSDScript *script = JSD_GetScriptForStackFrame (mCx, mThreadState,
                                                    mStackFrameInfo);
    *_rval = jsdScript::FromPtr (mCx, script);
    NS_IF_ADDREF(*_rval);
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetPc(PRUint32 *_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = JSD_GetPCForStackFrame (mCx, mThreadState, mStackFrameInfo);
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetLine(PRUint32 *_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    JSDScript *script = JSD_GetScriptForStackFrame (mCx, mThreadState,
                                                    mStackFrameInfo);
    jsuword pc = JSD_GetPCForStackFrame (mCx, mThreadState, mStackFrameInfo);
    *_rval = JSD_GetClosestLine (mCx, script, pc);
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::PcToLine(PRUint32 aPc, PRUint32 *_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    JSDScript *script = JSD_GetScriptForStackFrame (mCx, mThreadState,
                                                    mStackFrameInfo);
    *_rval = JSD_GetClosestLine (mCx, script, aPc);
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::LineToPc(PRUint32 aLine, PRUint32 *_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    JSDScript *script = JSD_GetScriptForStackFrame (mCx, mThreadState,
                                                    mStackFrameInfo);
    *_rval = JSD_GetClosestPC (mCx, script, aLine);
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetCallee(jsdIValue **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    JSDValue *jsdv = JSD_GetCallObjectForStackFrame (mCx, mThreadState,
                                                     mStackFrameInfo);
    
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    NS_IF_ADDREF(*_rval);
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetScope(jsdIValue **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    JSDValue *jsdv = JSD_GetScopeChainForStackFrame (mCx, mThreadState,
                                                     mStackFrameInfo);
    
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    NS_IF_ADDREF(*_rval);
    return NS_OK;
}

NS_IMETHODIMP
jsdStackFrame::GetThisValue(jsdIValue **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    JSDValue *jsdv = JSD_GetThisForStackFrame (mCx, mThreadState,
                                               mStackFrameInfo);
    
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    NS_IF_ADDREF(*_rval);
    return NS_OK;
}


NS_IMETHODIMP
jsdStackFrame::Eval (const nsAReadableString &bytes, const char *fileName,
                     PRUint32 line, jsdIValue **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;

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
    NS_IF_ADDREF(*_rval);
    return NS_OK;
}        

/* Thread States */
NS_IMPL_THREADSAFE_ISUPPORTS1(jsdThreadState, jsdIThreadState); 

NS_IMETHODIMP
jsdThreadState::GetJSDContext(JSDContext **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = mCx;
    return NS_OK;
}

NS_IMETHODIMP
jsdThreadState::GetJSDThreadState(JSDThreadState **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = mThreadState;
    return NS_OK;
}

NS_IMETHODIMP
jsdThreadState::GetFrameCount (PRUint32 *_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = JSD_GetCountOfStackFrames (mCx, mThreadState);
    return NS_OK;
}

NS_IMETHODIMP
jsdThreadState::GetTopFrame (jsdIStackFrame **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    JSDStackFrameInfo *sfi = JSD_GetStackFrame (mCx, mThreadState);
    
    *_rval = jsdStackFrame::FromPtr (mCx, mThreadState, sfi);
    NS_IF_ADDREF (*_rval);
    return NS_OK;
}

NS_IMETHODIMP
jsdThreadState::GetPendingException(jsdIValue **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    JSDValue *jsdv = JSD_GetException (mCx, mThreadState);
    
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    NS_IF_ADDREF(*_rval);
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

/* Thread States */
NS_IMPL_THREADSAFE_ISUPPORTS1(jsdValue, jsdIValue);

NS_IMETHODIMP
jsdValue::GetJSDContext(JSDContext **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = mCx;
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetJSDValue (JSDValue **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = mValue;
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetIsNative (PRBool *_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = JSD_IsValueNative (mCx, mValue);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetIsNumber (PRBool *_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = JSD_IsValueNumber (mCx, mValue);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetIsPrimitive (PRBool *_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = JSD_IsValuePrimitive (mCx, mValue);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetJsType (PRUint32 *_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;

    /* XXX surely this can be done better. */
    if (JSD_IsValueDouble(mCx, mValue))
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
    if (!_rval)
        return NS_ERROR_NULL_POINTER;

    JSDValue *jsdv = JSD_GetValuePrototype (mCx, mValue);
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    NS_IF_ADDREF(*_rval);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetJsParent (jsdIValue **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;

    JSDValue *jsdv = JSD_GetValueParent (mCx, mValue);
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    NS_IF_ADDREF(*_rval);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetJsClassName(char **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = nsCString(JSD_GetValueClassName(mCx, mValue)).ToNewCString();
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetJsConstructor (jsdIValue **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;

    JSDValue *jsdv = JSD_GetValueConstructor (mCx, mValue);
    *_rval = jsdValue::FromPtr (mCx, jsdv);
    NS_IF_ADDREF(*_rval);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetJsFunctionName(char **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = nsCString(JSD_GetValueFunctionName(mCx, mValue)).ToNewCString();
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetBooleanValue(PRBool *_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = JSD_GetValueBoolean (mCx, mValue);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetDoubleValue(double *_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    _rval = JSD_GetValueDouble (mCx, mValue);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetIntValue(PRInt32 *_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = JSD_GetValueInt (mCx, mValue);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetObjectValue(jsdIObject **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    JSDObject *obj;
    obj = JSD_GetObjectForValue (mCx, mValue);
    *_rval = jsdObject::FromPtr (mCx, obj);
    NS_IF_ADDREF(*_rval);
    return NS_OK;
}
    
NS_IMETHODIMP
jsdValue::GetStringValue(char **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    JSString *jstr_val = JSD_GetValueString(mCx, mValue);
    *_rval = nsCString(JS_GetStringBytes(jstr_val)).ToNewCString();
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetPropertyCount (PRUint32 *_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;

    *_rval = JSD_GetCountOfProperties (mCx, mValue);
    return NS_OK;
}

NS_IMETHODIMP
jsdValue::GetProperties (jsdIProperty ***propArray, PRUint32 *length)
{
    if (!propArray)
        return NS_ERROR_NULL_POINTER;
    
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
    NS_IF_ADDREF (*_rval);
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

static void
jsds_SetContextProc (JSDContext* jsdc, void* user)
{
    printf ("jsds_SetContextProc: called.\n");
}

static PRUint32
jsds_ExecutionHookProc (JSDContext* jsdc, JSDThreadState* jsdthreadstate,
                        uintN type, void* callerdata, jsval* rval)
{
    NS_PRECONDITION (callerdata, "no callerdata for jsds_ExecutionHookProc.");
    
    jsdIExecutionHook *hook = NS_STATIC_CAST(jsdIExecutionHook *, callerdata);
    jsdIValue *js_rv = 0;
    
    PRUint32 hook_rv = JSD_HOOK_RETURN_CONTINUE;
    
    hook->OnExecute (jsdContext::FromPtr(jsdc),
                     jsdThreadState::FromPtr(jsdc, jsdthreadstate),
                     type, &js_rv, &hook_rv);
    return hook_rv;
}

static void
jsds_ScriptHookProc (JSDContext* jsdc, JSDScript* jsdscript, JSBool creating,
                     void* callerdata)
{
    if (!callerdata)
        return;
    
    jsdIScriptHook *hook = NS_STATIC_CAST(jsdIScriptHook *, callerdata);
    hook->OnScriptLoaded (jsdContext::FromPtr(jsdc),
                          jsdScript::FromPtr(jsdc, jsdscript),
                          creating ? PR_TRUE : PR_FALSE);
}

NS_IMETHODIMP
jsdService::Init (void)
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
    
    mJSrt = JS_GetRuntime (cx);
    JSD_UserCallbacks jsd_uc;
    jsd_uc.size = sizeof(JSD_UserCallbacks);
    jsd_uc.setContext = jsds_SetContextProc;
    
    mJSDcx = JSD_DebuggerOnForUser (mJSrt, &jsd_uc, NULL);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::EnterNestedEventLoop (PRUint32 *_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
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
    if (!_rval)
        return NS_ERROR_NULL_POINTER;

    if (mNestedLoopLevel > 0)
        --mNestedLoopLevel;
    else
        return NS_ERROR_FAILURE;

    *_rval = mNestedLoopLevel;    
    return NS_OK;
}    

/* hook attribute get/set functions */

NS_IMETHODIMP
jsdService::SetDebugBreakHook (jsdIExecutionHook *aHook)
{    
    mDebugBreakHook = aHook;
    if (aHook)
        JSD_SetDebugBreakHook (mJSDcx, jsds_ExecutionHookProc,
                               NS_STATIC_CAST(void *, aHook));
    else
        JSD_ClearDebugBreakHook (mJSDcx);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::GetDebugBreakHook (jsdIExecutionHook **aHook)
{   
    if (!aHook)
        return NS_ERROR_NULL_POINTER;

    *aHook = mDebugBreakHook;
    NS_IF_ADDREF(*aHook);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::SetDebuggerHook (jsdIExecutionHook *aHook)
{    
    mDebuggerHook = aHook;
    if (aHook)
        JSD_SetDebuggerHook (mJSDcx, jsds_ExecutionHookProc,
                             NS_STATIC_CAST(void *, aHook));
    else
        JSD_ClearDebuggerHook (mJSDcx);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::GetDebuggerHook (jsdIExecutionHook **aHook)
{   
    if (!aHook)
        return NS_ERROR_NULL_POINTER;

    *aHook = mDebuggerHook;
    NS_IF_ADDREF(*aHook);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::SetInterruptHook (jsdIExecutionHook *aHook)
{    
    mInterruptHook = aHook;
    if (aHook)
        JSD_SetInterruptHook (mJSDcx, jsds_ExecutionHookProc,
                              NS_STATIC_CAST(void *, aHook));
    else
        JSD_ClearInterruptHook (mJSDcx);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::GetInterruptHook (jsdIExecutionHook **aHook)
{   
    if (!aHook)
        return NS_ERROR_NULL_POINTER;

    *aHook = mInterruptHook;
    NS_IF_ADDREF(*aHook);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::SetScriptHook (jsdIScriptHook *aHook)
{    
    mScriptHook = aHook;
    if (aHook)
        JSD_SetScriptHook (mJSDcx, jsds_ScriptHookProc,
                           NS_STATIC_CAST(void *, aHook));
    else
        JSD_SetScriptHook (mJSDcx, NULL, NULL);
    
    return NS_OK;
}

NS_IMETHODIMP
jsdService::GetScriptHook (jsdIScriptHook **aHook)
{   
    if (!aHook)
        return NS_ERROR_NULL_POINTER;

    *aHook = mScriptHook;
    NS_IF_ADDREF(*aHook);
    
    return NS_OK;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(jsdService);

static nsModuleComponentInfo components[] = {
    { "JSDService", JSDSERVICE_CID,
      jsdServiceContractID,
      jsdServiceConstructor},
};

NS_IMPL_NSGETMODULE("JavaScript Debugger", components);

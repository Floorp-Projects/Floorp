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
 *
 */

#include "jsd_xpc.h"

#include "nsIXPConnect.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsString.h"
#include "jsdebug.h"
#include "prmem.h"

const char jsdServiceContractID[] = "@mozilla.org/js/jsd/debugger-service;1";

/*******************************************************************************
 * reflected jsd data structures
 *******************************************************************************/

NS_IMPL_THREADSAFE_ISUPPORTS1(jsdContext, jsdIContext); 

NS_IMETHODIMP
jsdContext::GetJSDContext(JSDContext **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = mCx;
    return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(jsdScript, jsdIScript); 

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
    
    *_rval = JSD_IsActiveScript(mCx, mScript);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetFileName(PRUnichar **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = nsCString(JSD_GetScriptFilename(mCx, mScript)).ToNewUnicode();
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetFunctionName(PRUnichar **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = nsCString(JSD_GetScriptFunctionName(mCx, mScript)).ToNewUnicode();
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetBaseLineNumber(PRUint32 *_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = JSD_GetScriptBaseLineNumber(mCx, mScript);
    return NS_OK;
}

NS_IMETHODIMP
jsdScript::GetLineExtent(PRUint32 *_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = JSD_GetScriptLineExtent(mCx, mScript);
    return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(jsdThreadState, jsdIThreadState); 

NS_IMETHODIMP
jsdThreadState::GetJSDThreadState(JSDThreadState **_rval)
{
    if (!_rval)
        return NS_ERROR_NULL_POINTER;
    
    *_rval = mThreadState;
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
    nsISupports *is_rv = 0;
    
    PRUint32 hook_rv = JSD_HOOK_RETURN_CONTINUE;
    
    hook->OnExecute (jsdContext::FromPtr(jsdc),
                     jsdThreadState::FromPtr(jsdthreadstate),
                     type, &is_rv, &hook_rv);
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

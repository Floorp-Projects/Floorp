/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
 * This file is part of the Java-vendor-neutral implementation of LiveConnect
 *
 * It contains the implementation providing nsIFactory XP-COM interface.
 *
 */


#include <stdlib.h>
#include <string.h>
#include "prtypes.h"
#include "prprf.h"
#include "prlog.h"

#include "jsj_private.h"
#include "jsjava.h"

#include "jscntxt.h"        /* For js_ReportErrorAgain().*/

#include "netscape_javascript_JSObject.h"   /* javah-generated headers */
#include "nsISecurityContext.h"
#include "nsIServiceManager.h"
#include "nsIJSContextStack.h"

PR_BEGIN_EXTERN_C

/* A captured JavaScript error, created when JS_ReportError() is called while
   running JavaScript code that is itself called from Java. */
struct CapturedJSError {
    char *              message;
    JSErrorReport       report;         /* Line # of error, etc. */
    jthrowable          java_exception; /* Java exception, error, or null */
    CapturedJSError *   next;                   /* Next oldest captured JS error */
};
PR_END_EXTERN_C

#include "nsCLiveconnect.h"

#include "jsinterp.h"  // XXX private API so we can auto-push a JSStackFrame
#include "nsIScriptSecurityManager.h"
#include "nsIPrincipal.h"
#include "nsNetUtil.h"
#include "nsISecurityContext.h"
#include "prmem.h"

/***************************************************************************/
// A class to put on the stack to manage JS contexts when we are entering JS.
// This pushes and pops the given context
// with the nsThreadJSContextStack service as this object goes into and out
// of scope. It is optimized to not push/pop the cx if it is already on top
// of the stack. We need to push the JSContext when we enter JS because the
// JS security manager looks on the context stack for permissions information.

class AutoPushJSContext
{
public:
    AutoPushJSContext(nsISupports* aSecuritySupports,
                      JSContext *cx);

    ~AutoPushJSContext();

    nsresult ResultOfPush() { return mPushResult; }

private:
    nsCOMPtr<nsIJSContextStack> mContextStack;
    JSContext*                  mContext;
    JSStackFrame                mFrame;
    nsresult                    mPushResult;
};

AutoPushJSContext::AutoPushJSContext(nsISupports* aSecuritySupports,
                                     JSContext *cx) 
                                     : mContext(cx), mPushResult(NS_OK)
{
    nsCOMPtr<nsIJSContextStack> contextStack =
        do_GetService("@mozilla.org/js/xpc/ContextStack;1");

    JSContext* currentCX;
    if(contextStack &&
       // Don't push if the current context is already on the stack.
       (NS_FAILED(contextStack->Peek(&currentCX)) ||
        cx != currentCX) )
    {
        if (NS_SUCCEEDED(contextStack->Push(cx)))
        {
            // Leave the reference in mContextStack to
            // indicate that we need to pop it in our dtor.
            mContextStack.swap(contextStack);
        }
    }

    nsCOMPtr<nsIScriptSecurityManager> secMan(
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &mPushResult));

    if (NS_FAILED(mPushResult))
        return;

    nsCOMPtr<nsIPrincipal> principal;
    mPushResult = secMan->GetPrincipalFromContext(cx, getter_AddRefs(principal));

    if (NS_FAILED(mPushResult))
    {
        JS_ReportError(cx, "failed to get a principal");
        return;
    }

    // See if JavaScript is enabled for the current window
    PRBool jsEnabled = PR_FALSE;
    mPushResult = secMan->CanExecuteScripts(cx, principal, &jsEnabled);
    if (!jsEnabled)
        mPushResult = NS_ERROR_FAILURE;

    memset(&mFrame, 0, sizeof(mFrame));

    if (NS_SUCCEEDED(mPushResult))
    {
        // See if there are any scripts on the stack.
        // If not, we need to add a dummy frame with a principal.

        PRBool hasScript = PR_FALSE;
        JSStackFrame* tempFP = cx->fp;
        while (tempFP)
        {
            if (tempFP->script)
            {
                hasScript = PR_TRUE;
                break;
            }
            tempFP = tempFP->down;
        };

        if (!hasScript)
        {
            JSPrincipals* jsprinc;
            principal->GetJSPrincipals(cx, &jsprinc);

            mFrame.script = JS_CompileScriptForPrincipals(cx, JS_GetGlobalObject(cx),
                                                          jsprinc, "", 0, "", 1);
            JSPRINCIPALS_DROP(cx, jsprinc);

            if (mFrame.script)
            {
                mFrame.down = cx->fp;
                cx->fp = &mFrame;
            }
            else
                mPushResult = NS_ERROR_OUT_OF_MEMORY;
        }
    }
}

AutoPushJSContext::~AutoPushJSContext()
{
    if (mContextStack)
        mContextStack->Pop(nsnull);

    if (mFrame.script)
        mContext->fp = mFrame.down;

}


////////////////////////////////////////////////////////////////////////////
// from nsISupports and AggregatedQueryInterface:

// Thes macro expands to the aggregated query interface scheme.

NS_IMPL_AGGREGATED(nsCLiveconnect)
NS_INTERFACE_MAP_BEGIN_AGGREGATED(nsCLiveconnect)
    NS_INTERFACE_MAP_ENTRY(nsILiveconnect)
NS_INTERFACE_MAP_END



////////////////////////////////////////////////////////////////////////////
// from nsILiveconnect:

/**
 * get member of a Native JSObject for a given name.
 *
 * @param jEnv       - JNIEnv on which the call is being made.
 * @param obj        - A Native JS Object.
 * @param name       - Name of a member.
 * @param pjobj      - return parameter as a java object representing 
 *                     the member. If it is a basic data type it is converted to
 *                     a corresponding java type. If it is a NJSObject, then it is
 *                     wrapped up as java wrapper netscape.javascript.JSObject.
 */
NS_METHOD	
nsCLiveconnect::GetMember(JNIEnv *jEnv, lcjsobject obj, const jchar *name, jsize length, void* principalsArray[], 
                     int numPrincipals, nsISupports *securitySupports, jobject *pjobj)
{
    if(jEnv == NULL || obj == 0)
    {
       return NS_ERROR_FAILURE;
    }

    JSJavaThreadState *jsj_env        = NULL;
    JSObjectHandle    *handle         = (JSObjectHandle*)obj;
    JSObject          *js_obj         = handle->js_obj;
    JSContext         *cx             = NULL;
    jobject            member         = NULL;
    jsval              js_val;
    int                dummy_cost     = 0;
    JSBool             dummy_bool     = PR_FALSE;
    JSErrorReporter    saved_state    = NULL;

    jsj_env = jsj_enter_js(jEnv, mJavaClient, NULL, &cx, NULL, &saved_state, principalsArray, numPrincipals, securitySupports);
    if (!jsj_env)
        return NS_ERROR_FAILURE;

    AutoPushJSContext autopush(securitySupports, cx);
    if (NS_FAILED(autopush.ResultOfPush()))
        goto done;

    if (!name) {
        JS_ReportError(cx, "illegal null member name");
        member = NULL;
        goto done;
    }
    
    if (!JS_GetUCProperty(cx, js_obj, name, length, &js_val))
        goto done;

    jsj_ConvertJSValueToJavaObject(cx, jEnv, js_val, jsj_get_jlObject_descriptor(cx, jEnv),
                                   &dummy_cost, &member, &dummy_bool);

done:
    if (!jsj_exit_js(cx, jsj_env, saved_state))
        return NS_ERROR_FAILURE;
    
    *pjobj = member;

    return NS_OK;
}


/**
 * get member of a Native JSObject for a given index.
 *
 * @param jEnv       - JNIEnv on which the call is being made.
 * @param obj        - A Native JS Object.
 * @param index      - Index of a member.
 * @param pjobj      - return parameter as a java object representing 
 *                     the member. 
 */
NS_METHOD	
nsCLiveconnect::GetSlot(JNIEnv *jEnv, lcjsobject obj, jint slot, void* principalsArray[], 
                     int numPrincipals, nsISupports *securitySupports,  jobject *pjobj)
{
    if(jEnv == NULL || obj == 0)
    {
       return NS_ERROR_FAILURE;
    }

    JSJavaThreadState *jsj_env        = NULL;
    JSObjectHandle    *handle         = (JSObjectHandle*)obj;
    JSObject          *js_obj         = handle->js_obj;
    JSContext         *cx             = NULL;
    jobject            member         = NULL;
    jsval              js_val;
    int                dummy_cost     = 0;
    JSBool             dummy_bool     = PR_FALSE;
    JSErrorReporter    saved_state    = NULL;

    jsj_env = jsj_enter_js(jEnv, mJavaClient, NULL, &cx, NULL, &saved_state, principalsArray, numPrincipals, securitySupports);
    if (!jsj_env)
       return NS_ERROR_FAILURE;

    AutoPushJSContext autopush(securitySupports, cx);
    if (NS_FAILED(autopush.ResultOfPush()))
        goto done;
    
    // =-= sudu: check to see if slot can be passed in as is.
    //           Should it be converted to a jsint?
    if (!JS_GetElement(cx, js_obj, slot, &js_val))
        goto done;
    if (!jsj_ConvertJSValueToJavaObject(cx, jEnv, js_val, jsj_get_jlObject_descriptor(cx, jEnv),
                                        &dummy_cost, &member, &dummy_bool))
        goto done;

done:
    if (!jsj_exit_js(cx, jsj_env, saved_state))
       return NS_ERROR_FAILURE;
    
    *pjobj = member;
    return NS_OK;
}

/**
 * set member of a Native JSObject for a given name.
 *
 * @param jEnv       - JNIEnv on which the call is being made.
 * @param obj        - A Native JS Object.
 * @param name       - Name of a member.
 * @param jobj       - Value to set. If this is a basic data type, it is converted
 *                     using standard JNI calls but if it is a wrapper to a JSObject
 *                     then a internal mapping is consulted to convert to a NJSObject.
 */
NS_METHOD	
nsCLiveconnect::SetMember(JNIEnv *jEnv, lcjsobject obj, const jchar *name, jsize length, jobject java_obj, void* principalsArray[], 
                     int numPrincipals, nsISupports *securitySupports)
{
    if(jEnv == NULL || obj == 0)
    {
       return NS_ERROR_FAILURE;
    }

    JSJavaThreadState *jsj_env        = NULL;
    JSObjectHandle    *handle         = (JSObjectHandle*)obj;
    JSObject          *js_obj         = handle->js_obj;
    JSContext         *cx             = NULL;
    jsval              js_val;
    JSErrorReporter    saved_state    = NULL;

    jsj_env = jsj_enter_js(jEnv, mJavaClient, NULL, &cx, NULL, &saved_state, principalsArray, numPrincipals, securitySupports);
    if (!jsj_env)
        return NS_ERROR_FAILURE;

    AutoPushJSContext autopush(securitySupports, cx);
    if (NS_FAILED(autopush.ResultOfPush()))
        goto done;
    
    if (!name) {
        JS_ReportError(cx, "illegal null member name");
        goto done;
    }

    if (!jsj_ConvertJavaObjectToJSValue(cx, jEnv, java_obj, &js_val))
        goto done;

    JS_SetUCProperty(cx, js_obj, name, length, &js_val);

done:
    jsj_exit_js(cx, jsj_env, saved_state);
   return NS_OK;
}


/**
 * set member of a Native JSObject for a given index.
 *
 * @param jEnv       - JNIEnv on which the call is being made.
 * @param obj        - A Native JS Object.
 * @param index      - Index of a member.
 * @param jobj       - Value to set. If this is a basic data type, it is converted
 *                     using standard JNI calls but if it is a wrapper to a JSObject
 *                     then a internal mapping is consulted to convert to a NJSObject.
 */
NS_METHOD	
nsCLiveconnect::SetSlot(JNIEnv *jEnv, lcjsobject obj, jint slot, jobject java_obj,  void* principalsArray[], 
                     int numPrincipals, nsISupports *securitySupports)
{
    if(jEnv == NULL || obj == 0)
    {
       return NS_ERROR_FAILURE;
    }

    JSJavaThreadState *jsj_env        = NULL;
    JSObjectHandle    *handle         = (JSObjectHandle*)obj;
    JSObject          *js_obj         = handle->js_obj;
    JSContext         *cx             = NULL;
    jsval              js_val;
    JSErrorReporter    saved_state    = NULL;

    jsj_env = jsj_enter_js(jEnv, mJavaClient, NULL, &cx, NULL, &saved_state, principalsArray, numPrincipals, securitySupports);
    if (!jsj_env)
        return NS_ERROR_FAILURE;

    AutoPushJSContext autopush(securitySupports, cx);
    if (NS_FAILED(autopush.ResultOfPush()))
        goto done;
    
    if (!jsj_ConvertJavaObjectToJSValue(cx, jEnv, java_obj, &js_val))
        goto done;
    JS_SetElement(cx, js_obj, slot, &js_val);

done:
    jsj_exit_js(cx, jsj_env, saved_state);
    return NS_OK;
}


/**
 * remove member of a Native JSObject for a given name.
 *
 * @param jEnv       - JNIEnv on which the call is being made.
 * @param obj        - A Native JS Object.
 * @param name       - Name of a member.
 */
NS_METHOD	
nsCLiveconnect::RemoveMember(JNIEnv *jEnv, lcjsobject obj, const jchar *name, jsize length,  void* principalsArray[], 
                             int numPrincipals, nsISupports *securitySupports)
{
    if(jEnv == NULL || obj == 0)
    {
       return NS_ERROR_FAILURE;
    }

    JSJavaThreadState *jsj_env        = NULL;
    JSObjectHandle    *handle         = (JSObjectHandle*)obj;
    JSObject          *js_obj         = handle->js_obj;
    JSContext         *cx             = NULL;
    jsval              js_val;
    JSErrorReporter    saved_state    = NULL;

    jsj_env = jsj_enter_js(jEnv, mJavaClient, NULL, &cx, NULL, &saved_state, principalsArray, numPrincipals, securitySupports);
    if (!jsj_env)
        return NS_ERROR_FAILURE;

    AutoPushJSContext autopush(securitySupports, cx);
    if (NS_FAILED(autopush.ResultOfPush()))
        goto done;
    
    if (!name) {
        JS_ReportError(cx, "illegal null member name");
        goto done;
    }

    JS_DeleteUCProperty2(cx, js_obj, name, length, &js_val);

done:
    jsj_exit_js(cx, jsj_env, saved_state);
    return NS_OK;
}


/**
 * call a method of Native JSObject. 
 *
 * @param jEnv       - JNIEnv on which the call is being made.
 * @param obj        - A Native JS Object.
 * @param name       - Name of a method.
 * @param jobjArr    - Array of jobjects representing parameters of method being caled.
 * @param pjobj      - return value.
 */
NS_METHOD	
nsCLiveconnect::Call(JNIEnv *jEnv, lcjsobject obj, const jchar *name, jsize length, jobjectArray java_args, void* principalsArray[], 
                     int numPrincipals, nsISupports *securitySupports, jobject *pjobj)
{
    if(jEnv == NULL || obj == 0)
    {
       return NS_ERROR_FAILURE;
    }

    int                i              = 0;
    int                argc           = 0;
    int                arg_num        = 0;
    jsval             *argv           = 0;
    JSJavaThreadState *jsj_env        = NULL;
    JSObjectHandle    *handle         = (JSObjectHandle*)obj;
    JSObject          *js_obj         = handle->js_obj;
    JSContext         *cx             = NULL;
    jsval              js_val;
    jsval              function_val   = 0;
    int                dummy_cost     = 0;
    JSBool             dummy_bool     = PR_FALSE;
    JSErrorReporter    saved_state    = NULL;
    jobject            result         = NULL;

    jsj_env = jsj_enter_js(jEnv, mJavaClient, NULL, &cx, NULL, &saved_state, principalsArray, numPrincipals, securitySupports);
    if (!jsj_env)
        return NS_ERROR_FAILURE;

    result = NULL;
    AutoPushJSContext autopush(securitySupports, cx);
    if (NS_FAILED(autopush.ResultOfPush()))
        goto done;
    
    if (!name) {
        JS_ReportError(cx, "illegal null JavaScript function name");
        goto done;
    }

    /* Allocate space for JS arguments */
    argc = java_args ? jEnv->GetArrayLength(java_args) : 0;
    if (argc) {
        argv = (jsval*)JS_malloc(cx, argc * sizeof(jsval));
        if (!argv)
            goto done;
    } else {
        argv = 0;
    }

    /* Convert arguments from Java to JS values */
    for (arg_num = 0; arg_num < argc; arg_num++) {
        jobject arg = jEnv->GetObjectArrayElement(java_args, arg_num);
        JSBool ret = jsj_ConvertJavaObjectToJSValue(cx, jEnv, arg, &argv[arg_num]);
		
        jEnv->DeleteLocalRef(arg);
        if (!ret)
            goto cleanup_argv;
        JS_AddRoot(cx, &argv[arg_num]);
    }

    if (!JS_GetUCProperty(cx, js_obj, name, length, &function_val))
        goto cleanup_argv;

    if (!JS_CallFunctionValue(cx, js_obj, function_val, argc, argv, &js_val))
        goto cleanup_argv;

    jsj_ConvertJSValueToJavaObject(cx, jEnv, js_val, jsj_get_jlObject_descriptor(cx, jEnv),
                                   &dummy_cost, &result, &dummy_bool);

cleanup_argv:
    if (argv) {
        for (i = 0; i < arg_num; i++)
            JS_RemoveRoot(cx, &argv[i]);
        JS_free(cx, argv);
    }

done:
    if (!jsj_exit_js(cx, jsj_env, saved_state))
        return NS_ERROR_FAILURE;
    
    *pjobj = result;

    return NS_OK;
}

NS_METHOD	
nsCLiveconnect::Eval(JNIEnv *jEnv, lcjsobject obj, const jchar *script, jsize length, void* principalsArray[], 
                     int numPrincipals, nsISupports *securitySupports, jobject *pjobj)
{
    if(jEnv == NULL || obj == 0)
    {
       return NS_ERROR_FAILURE;
    }

    JSJavaThreadState *jsj_env        = NULL;
    JSObjectHandle    *handle         = (JSObjectHandle*)obj;
    JSObject          *js_obj         = handle->js_obj;
    JSContext         *cx             = NULL;
    jsval              js_val;
    int                dummy_cost     = 0;
    JSBool             dummy_bool     = PR_FALSE;
    JSErrorReporter    saved_state    = NULL;
    jobject            result         = NULL;
	const char		  *codebase       = NULL;
    JSPrincipals      *principals     = NULL;
    JSBool             eval_succeeded = PR_FALSE;

    jsj_env = jsj_enter_js(jEnv, mJavaClient, NULL, &cx, NULL, &saved_state, principalsArray, numPrincipals, securitySupports);
    if (!jsj_env)
       return NS_ERROR_FAILURE;

    result = NULL;
    AutoPushJSContext autopush(securitySupports, cx);
    if (NS_FAILED(autopush.ResultOfPush()))
        goto done;
    
    if (!script) {
        JS_ReportError(cx, "illegal null string eval argument");
        goto done;
    }

    /* Set up security stuff */
    if (JSJ_callbacks && JSJ_callbacks->get_JSPrincipals_from_java_caller)
        principals = JSJ_callbacks->get_JSPrincipals_from_java_caller(jEnv, cx, principalsArray, numPrincipals, securitySupports);
    codebase = principals ? principals->codebase : NULL;

    /* Have the JS engine evaluate the unicode string */
    eval_succeeded = JS_EvaluateUCScriptForPrincipals(cx, js_obj, principals,
                                                      script, length,
                                                      codebase, 0, &js_val);

    if (!eval_succeeded)
        goto done;

    /* Convert result to a subclass of java.lang.Object */
    jsj_ConvertJSValueToJavaObject(cx, jEnv, js_val, jsj_get_jlObject_descriptor(cx, jEnv),
                                   &dummy_cost, &result, &dummy_bool);

done:
    if (principals)
        JSPRINCIPALS_DROP(cx, principals);
    if (!jsj_exit_js(cx, jsj_env, saved_state))
       return NS_ERROR_FAILURE;
    
    *pjobj = result;
    return NS_OK;
}


/**
 * Get the window object for a plugin instance.
 *
 * @param jEnv               - JNIEnv on which the call is being made.
 * @param pJavaObject        - Either a jobject or a pointer to a plugin instance 
 *                             representing the java object.
 * @param pjobj              - return value. This is a native js object 
 *                             representing the window object of a frame 
 *                             in which a applet/bean resides.
 */
NS_METHOD	
nsCLiveconnect::GetWindow(JNIEnv *jEnv, void *pJavaObject,  void* principalsArray[], 
                     int numPrincipals, nsISupports *securitySupports, lcjsobject *pobj)
{
    if(jEnv == NULL || JSJ_callbacks == NULL)
    {
       return NS_ERROR_FAILURE;
    }

    // associate this Java client with this LiveConnect connection.
    mJavaClient = pJavaObject;

    char              *err_msg        = NULL;
    JSContext         *cx             = NULL;
    JSObject          *js_obj         = NULL;
    JSErrorReporter    saved_state    = NULL;
    JSJavaThreadState *jsj_env        = NULL;
    JSObjectHandle    *handle         = NULL;

    jsj_env = jsj_enter_js(jEnv, mJavaClient, NULL, &cx, NULL, &saved_state, principalsArray, numPrincipals, securitySupports);
    if (!jsj_env)
       return NS_ERROR_FAILURE;

    err_msg = NULL;
    AutoPushJSContext autopush(securitySupports, cx);
    if (NS_FAILED(autopush.ResultOfPush()))
        goto done;
    
    js_obj = JSJ_callbacks->map_java_object_to_js_object(jEnv, mJavaClient, &err_msg);
    if (!js_obj) {
        if (err_msg) {
            JS_ReportError(cx, err_msg);
            free(err_msg);
        }
        goto done;
    }
#ifdef PRESERVE_JSOBJECT_IDENTITY
    *pobj = (jint)js_obj;
#else
	/* FIXME:  to handle PRESERVE_JSOBJECT_IDENTITY case, this needs to
    just return a raw JSObject reference. FIXME !!! */
    /* Create a tiny stub object to act as the GC root that points to the
       JSObject from its netscape.javascript.JSObject counterpart. */
    handle = (JSObjectHandle*)JS_malloc(cx, sizeof(JSObjectHandle));
    if (handle != NULL)
    {
      handle->js_obj = js_obj;
      handle->rt = JS_GetRuntime(cx);
    }
    *pobj = (lcjsobject)handle;
    /* FIXME:  what if the window is explicitly disposed of, how do we
       notify Java? */
#endif
done:
    if (!jsj_exit_js(cx, jsj_env, saved_state))
       return NS_ERROR_FAILURE;

    return NS_OK;
}

/**
 * Get the window object for a plugin instance.
 *
 * @param jEnv       - JNIEnv on which the call is being made.
 * @param obj        - A Native JS Object.
 */
NS_METHOD	
nsCLiveconnect::FinalizeJSObject(JNIEnv *jEnv, lcjsobject obj)
{
    if(jEnv == NULL || obj == 0)
    {
       return NS_ERROR_FAILURE;
    }

    JSObjectHandle    *handle         = (JSObjectHandle *)obj;
    
    if (!handle)
        return NS_ERROR_NULL_POINTER;
    JS_RemoveRootRT(handle->rt, &handle->js_obj);
    free(handle);
    return NS_OK;
}


NS_METHOD	
nsCLiveconnect::ToString(JNIEnv *jEnv, lcjsobject obj, jstring *pjstring)
{
    if(jEnv == NULL || obj == 0)
    {
       return NS_ERROR_FAILURE;
    }

    JSJavaThreadState *jsj_env        = NULL;
    JSObjectHandle    *handle         = (JSObjectHandle*)obj;
    JSObject          *js_obj         = handle->js_obj;
    JSContext         *cx             = NULL;
    JSErrorReporter    saved_state    = NULL;
    jstring            result         = NULL;
    JSString          *jsstr          = NULL;

    jsj_env = jsj_enter_js(jEnv, mJavaClient, NULL, &cx, NULL, &saved_state, NULL, 0, NULL );
    if (!jsj_env)
       return NS_ERROR_FAILURE;

    result = NULL;
    AutoPushJSContext autopush(nsnull, cx);
    if (NS_FAILED(autopush.ResultOfPush()))
        return NS_ERROR_FAILURE;

    jsstr = JS_ValueToString(cx, OBJECT_TO_JSVAL(js_obj));
    if (jsstr)
        result = jsj_ConvertJSStringToJavaString(cx, jEnv, jsstr);
    if (!result)
        result = jEnv->NewStringUTF("*JavaObject*");

    if (!jsj_exit_js(cx, jsj_env, saved_state))
        return NS_ERROR_FAILURE;
    *pjstring = result;     
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////////
// from nsCLiveconnect:

nsCLiveconnect::nsCLiveconnect(nsISupports *aOuter)
    :   mJavaClient(NULL)
{
    NS_INIT_AGGREGATED(aOuter);
#ifdef PRESERVE_JSOBJECT_IDENTITY
    jsj_init_js_obj_reflections_table();
#endif
}

nsCLiveconnect::~nsCLiveconnect()
{
}

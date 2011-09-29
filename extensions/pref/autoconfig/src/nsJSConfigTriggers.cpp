/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Mitesh Shah <mitesh@netscape.com>
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

#ifdef MOZ_LOGGING
// sorry, this has to be before the pre-compiled header
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif
#include "jsapi.h"
#include "nsIXPCSecurityManager.h"
#include "nsIXPConnect.h"
#include "nsIJSRuntimeService.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsString.h"
#include "nsIPrefService.h"
#include "nsIJSContextStack.h"
#include "nspr.h"

extern PRLogModuleInfo *MCD;

// Security Manager for new XPCONNECT enabled JS Context
// Right now it allows all access

class AutoConfigSecMan : public nsIXPCSecurityManager
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIXPCSECURITYMANAGER
    AutoConfigSecMan();
};

NS_IMPL_ISUPPORTS1(AutoConfigSecMan, nsIXPCSecurityManager)

AutoConfigSecMan::AutoConfigSecMan()
{
}

NS_IMETHODIMP
AutoConfigSecMan::CanCreateWrapper(JSContext *aJSContext, const nsIID & aIID, 
                                  nsISupports *aObj, nsIClassInfo *aClassInfo, 
                                  void **aPolicy)
{
    return NS_OK;
}

NS_IMETHODIMP
AutoConfigSecMan::CanCreateInstance(JSContext *aJSContext, const nsCID & aCID)
{
    return NS_OK;
}

NS_IMETHODIMP
AutoConfigSecMan::CanGetService(JSContext *aJSContext, const nsCID & aCID)
{
    return NS_OK;
}

NS_IMETHODIMP 
AutoConfigSecMan::CanAccess(PRUint32 aAction, 
                            nsAXPCNativeCallContext *aCallContext, 
                            JSContext *aJSContext, JSObject *aJSObject, 
                            nsISupports *aObj, nsIClassInfo *aClassInfo, 
                            jsid aName, void **aPolicy)
{
    return NS_OK;
}

//*****************************************************************************

static  JSContext *autoconfig_cx = nsnull;
static  JSObject *autoconfig_glob;

static JSClass global_class = {
    "autoconfig_global", JSCLASS_GLOBAL_FLAGS,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   nsnull
};

static void
autoConfigErrorReporter(JSContext *cx, const char *message, 
                        JSErrorReport *report)
{
    NS_ERROR(message);
    PR_LOG(MCD, PR_LOG_DEBUG, ("JS error in js from MCD server: %s\n", message));
} 

nsresult CentralizedAdminPrefManagerInit()
{
    nsresult rv;
    JSRuntime *rt;

    // If autoconfig_cx already created, no need to create it again
    if (autoconfig_cx) 
        return NS_OK;

    // We need the XPCONNECT service
    nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID(), &rv);
    if (NS_FAILED(rv)) {
        return rv;
    }

    // Get the JS RunTime
    nsCOMPtr<nsIJSRuntimeService> rtsvc = 
        do_GetService("@mozilla.org/js/xpc/RuntimeService;1", &rv);
    if (NS_SUCCEEDED(rv))
        rv = rtsvc->GetRuntime(&rt);

    if (NS_FAILED(rv)) {
        NS_ERROR("Couldn't get JS RunTime");
        return rv;
    }

    // Create a new JS context for autoconfig JS script
    autoconfig_cx = JS_NewContext(rt, 1024);
    if (!autoconfig_cx)
        return NS_ERROR_OUT_OF_MEMORY;

    JSAutoRequest ar(autoconfig_cx);

    JS_SetErrorReporter(autoconfig_cx, autoConfigErrorReporter);

    // Create a new Security Manger and set it for the new JS context
    nsCOMPtr<nsIXPCSecurityManager> secman =
        static_cast<nsIXPCSecurityManager*>(new AutoConfigSecMan());
    xpc->SetSecurityManagerForJSContext(autoconfig_cx, secman, 0);

    autoconfig_glob = JS_NewCompartmentAndGlobalObject(autoconfig_cx, &global_class, NULL);
    if (autoconfig_glob) {
        JSAutoEnterCompartment ac;
        if(!ac.enter(autoconfig_cx, autoconfig_glob))
            return NS_ERROR_FAILURE;
        if (JS_InitStandardClasses(autoconfig_cx, autoconfig_glob)) {
            // XPCONNECT enable this JS context
            rv = xpc->InitClasses(autoconfig_cx, autoconfig_glob);
            if (NS_SUCCEEDED(rv)) 
                return NS_OK;
        }
    }

    // failue exit... clean up the JS context
    JS_DestroyContext(autoconfig_cx);
    autoconfig_cx = nsnull;
    return NS_ERROR_FAILURE;
}

nsresult CentralizedAdminPrefManagerFinish()
{
    if (autoconfig_cx)
        JS_DestroyContext(autoconfig_cx);
    return NS_OK;
}

nsresult EvaluateAdminConfigScript(const char *js_buffer, size_t length,
                                   const char *filename, bool bGlobalContext, 
                                   bool bCallbacks, bool skipFirstLine)
{
    JSBool ok;

    if (skipFirstLine) {
        /* In order to protect the privacy of the JavaScript preferences file 
         * from loading by the browser, we make the first line unparseable
         * by JavaScript. We must skip that line here before executing 
         * the JavaScript code.
         */
        unsigned int i = 0;
        while (i < length) {
            char c = js_buffer[i++];
            if (c == '\r') {
                if (js_buffer[i] == '\n')
                    i++;
                break;
            }
            if (c == '\n')
                break;
        }

        length -= i;
        js_buffer += i;
    }

    nsresult rv;
    nsCOMPtr<nsIJSContextStack> cxstack = 
        do_GetService("@mozilla.org/js/xpc/ContextStack;1");
    rv = cxstack->Push(autoconfig_cx);
    if (NS_FAILED(rv)) {
        NS_ERROR("coudn't push the context on the stack");
        return rv;
    }

    JS_BeginRequest(autoconfig_cx);
    ok = JS_EvaluateScript(autoconfig_cx, autoconfig_glob,
                           js_buffer, length, filename, 0, nsnull);
    JS_EndRequest(autoconfig_cx);

    JS_MaybeGC(autoconfig_cx);

    JSContext *cx;
    cxstack->Pop(&cx);
    NS_ASSERTION(cx == autoconfig_cx, "AutoConfig JS contexts didn't match");

    if (ok)
        return NS_OK;
    return NS_ERROR_FAILURE;
}


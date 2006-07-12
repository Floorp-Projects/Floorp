/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike Shaver <shaver@mozilla.org>
 *   John Bandhauer <jband@netscape.com>
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

#include "xpcprivate.h"

#ifndef XPCONNECT_STANDALONE
NS_IMPL_THREADSAFE_ISUPPORTS2(BackstagePass, nsIScriptObjectPrincipal, nsIXPCScriptable)
#else
NS_IMPL_THREADSAFE_ISUPPORTS1(BackstagePass, nsIXPCScriptable)
#endif

// The nsIXPCScriptable map declaration that will generate stubs for us...
#define XPC_MAP_CLASSNAME           BackstagePass
#define XPC_MAP_QUOTED_CLASSNAME   "BackstagePass"
#define                             XPC_MAP_WANT_NEWRESOLVE
#define XPC_MAP_FLAGS       nsIXPCScriptable::USE_JSSTUB_FOR_ADDPROPERTY   | \
                            nsIXPCScriptable::USE_JSSTUB_FOR_DELPROPERTY   | \
                            nsIXPCScriptable::USE_JSSTUB_FOR_SETPROPERTY   | \
                            nsIXPCScriptable::DONT_ENUM_STATIC_PROPS       | \
                            nsIXPCScriptable::DONT_ENUM_QUERY_INTERFACE    | \
                            nsIXPCScriptable::DONT_REFLECT_INTERFACE_NAMES
#include "xpc_map_end.h" /* This will #undef the above */

/* PRBool newResolve (in nsIXPConnectWrappedNative wrapper, in JSContextPtr cx, in JSObjectPtr obj, in JSVal id, in PRUint32 flags, out JSObjectPtr objp); */
NS_IMETHODIMP
BackstagePass::NewResolve(nsIXPConnectWrappedNative *wrapper,
                          JSContext * cx, JSObject * obj,
                          jsval id, PRUint32 flags, 
                          JSObject * *objp, PRBool *_retval)
{
    JSBool resolved;

    *_retval = JS_ResolveStandardClass(cx, obj, id, &resolved);
    if(*_retval && resolved)
        *objp = obj;
    return NS_OK;
}

/*
 * This object holds state that we don't want to lose!
 *
 * The plan is that once created this object never goes away. We do an
 * intentional extra addref at construction to keep it around even if no one
 * is using it.
 */

nsJSRuntimeServiceImpl::nsJSRuntimeServiceImpl() :
    mRuntime(0)
{
}

nsJSRuntimeServiceImpl::~nsJSRuntimeServiceImpl() {
    if(mRuntime)
    {
        JS_DestroyRuntime(mRuntime);
        JS_ShutDown();
#ifdef DEBUG_shaver_off
        fprintf(stderr, "nJRSI: destroyed runtime %p\n", (void *)mRuntime);
#endif
    }
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsJSRuntimeServiceImpl,
                              nsIJSRuntimeService,
                              nsISupportsWeakReference)

nsJSRuntimeServiceImpl*
nsJSRuntimeServiceImpl::gJSRuntimeService = nsnull;

nsJSRuntimeServiceImpl*
nsJSRuntimeServiceImpl::GetSingleton()
{
    if(!gJSRuntimeService)
    {
        gJSRuntimeService = new nsJSRuntimeServiceImpl();
        // hold an extra reference to lock it down
        NS_IF_ADDREF(gJSRuntimeService);

    }
    NS_IF_ADDREF(gJSRuntimeService);

    return gJSRuntimeService;
}

void
nsJSRuntimeServiceImpl::FreeSingleton()
{
    NS_IF_RELEASE(gJSRuntimeService);
}

const uint32 gGCSize = 32L * 1024L * 1024L; /* pref? */

/* attribute JSRuntime runtime; */
NS_IMETHODIMP
nsJSRuntimeServiceImpl::GetRuntime(JSRuntime **runtime)
{
    if(!runtime)
        return NS_ERROR_NULL_POINTER;

    if(!mRuntime)
    {
        mRuntime = JS_NewRuntime(gGCSize);
        if(!mRuntime)
            return NS_ERROR_OUT_OF_MEMORY;

        // Unconstrain the runtime's threshold on nominal heap size, to avoid
        // triggering GC too often if operating continuously near an arbitrary
        // finite threshold (0xffffffff is infinity for uint32 parameters).
        // This leaves the maximum-JS_malloc-bytes threshold still in effect
        // to cause period, and we hope hygienic, last-ditch GCs from within
        // the GC's allocator.
        JS_SetGCParameter(mRuntime, JSGC_MAX_BYTES, 0xffffffff);
    }
    *runtime = mRuntime;
    return NS_OK;
}

/* attribute nsIXPCScriptable backstagePass; */
NS_IMETHODIMP
nsJSRuntimeServiceImpl::GetBackstagePass(nsIXPCScriptable **bsp)
{
    if(!mBackstagePass) {
#ifndef XPCONNECT_STANDALONE
        nsCOMPtr<nsIPrincipal> sysprin;
        nsCOMPtr<nsIScriptSecurityManager> secman = 
            do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);
        if(!secman)
            return NS_ERROR_NOT_AVAILABLE;
        if(NS_FAILED(secman->GetSystemPrincipal(getter_AddRefs(sysprin))))
            return NS_ERROR_NOT_AVAILABLE;
        
        mBackstagePass = new BackstagePass(sysprin);
#else
        mBackstagePass = new BackstagePass();
#endif
        if(!mBackstagePass)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(*bsp = mBackstagePass);
    return NS_OK;
}

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   John Bandhauer <jband@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/* Call Context class for calls into native code. */

#include "xpcprivate.h"

nsXPCNativeCallContext::nsXPCNativeCallContext()
    :   mData(nsnull)
{
    NS_INIT_ISUPPORTS();
}

nsXPCNativeCallContext::~nsXPCNativeCallContext()
{
    // empty...
}        

NS_IMPL_QUERY_INTERFACE1(nsXPCNativeCallContext, nsIXPCNativeCallContext)

// This is not a refcounted object. We keep one alive per XPCContext

NS_IMETHODIMP_(nsrefcnt)
nsXPCNativeCallContext::AddRef(void)
{
    return 1;
}        

NS_IMETHODIMP_(nsrefcnt)
nsXPCNativeCallContext::Release(void)
{
    return 1;
}        

NS_IMETHODIMP 
nsXPCNativeCallContext::GetCallee(nsISupports** calleep)
{
    NS_ENSURE_ARG_POINTER(calleep);
    if(NS_WARN_IF_FALSE(mData,"no active native call")) 
        return NS_ERROR_FAILURE;
    NS_IF_ADDREF(mData->callee);
    *calleep = mData->callee;
    return NS_OK;
}        

NS_IMETHODIMP 
nsXPCNativeCallContext::GetCalleeMethodIndex(uint16* indexp)
{
    NS_ENSURE_ARG_POINTER(indexp);
    if(NS_WARN_IF_FALSE(mData,"no active native call")) 
        return NS_ERROR_FAILURE;
    *indexp = mData->index;
    return NS_OK;
}        

NS_IMETHODIMP
nsXPCNativeCallContext::GetCalleeWrapper(nsIXPConnectWrappedNative** wrapperp)
{
    NS_ENSURE_ARG_POINTER(wrapperp);
    if(NS_WARN_IF_FALSE(mData,"no active native call")) 
        return NS_ERROR_FAILURE;
    NS_IF_ADDREF(mData->wrapper);
    *wrapperp = mData->wrapper;
    return NS_OK;
}        

NS_IMETHODIMP 
nsXPCNativeCallContext::GetJSContext(JSContext** cxp)
{
    NS_ENSURE_ARG_POINTER(cxp);
    if(NS_WARN_IF_FALSE(mData,"no active native call")) 
        return NS_ERROR_FAILURE;
    *cxp = mData->cx;
    return NS_OK;
}        

NS_IMETHODIMP 
nsXPCNativeCallContext::GetArgc(PRUint32* argcp)
{
    NS_ENSURE_ARG_POINTER(argcp);
    if(NS_WARN_IF_FALSE(mData,"no active native call")) 
        return NS_ERROR_FAILURE;
    *argcp = mData->argc;
    return NS_OK;
}        

NS_IMETHODIMP 
nsXPCNativeCallContext::GetArgvPtr(jsval** argvp)
{
    NS_ENSURE_ARG_POINTER(argvp);
    if(NS_WARN_IF_FALSE(mData,"no active native call")) 
        return NS_ERROR_FAILURE;
    *argvp = mData->argv;
    return NS_OK;
}        

NS_IMETHODIMP 
nsXPCNativeCallContext::GetRetValPtr(jsval** retvalp)
{
    NS_ENSURE_ARG_POINTER(retvalp);
    if(NS_WARN_IF_FALSE(mData,"no active native call")) 
        return NS_ERROR_FAILURE;
    *retvalp = mData->retvalp;
    return NS_OK;
}        

NS_IMETHODIMP 
nsXPCNativeCallContext::SetExceptionWasThrown(JSBool threw)
{
    if(NS_WARN_IF_FALSE(mData,"no active native call")) 
        return NS_ERROR_FAILURE;
    mData->threw = threw;
    return NS_OK;
}        

NS_IMETHODIMP 
nsXPCNativeCallContext::GetExceptionWasThrown(JSBool* threwp)
{
    NS_ENSURE_ARG_POINTER(threwp);
    if(NS_WARN_IF_FALSE(mData,"no active native call")) 
        return NS_ERROR_FAILURE;
    *threwp = mData->threw;
    return NS_OK;
}        

NS_IMETHODIMP 
nsXPCNativeCallContext::SetReturnValueWasSet(JSBool retvalSet)
{
    if(NS_WARN_IF_FALSE(mData,"no active native call")) 
        return NS_ERROR_FAILURE;
    mData->retvalSet = retvalSet;
    return NS_OK;
}        

NS_IMETHODIMP 
nsXPCNativeCallContext::GetReturnValueWasSet(JSBool* retvalSet)
{
    NS_ENSURE_ARG_POINTER(retvalSet);
    if(NS_WARN_IF_FALSE(mData,"no active native call")) 
        return NS_ERROR_FAILURE;
    *retvalSet = mData->retvalSet;
    return NS_OK;
}        

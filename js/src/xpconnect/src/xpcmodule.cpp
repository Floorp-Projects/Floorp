/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/* Module level methods. */

#include "xpcprivate.h"

/***************************************************************************/

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kGenericFactoryCID, NS_GENERICFACTORY_CID);
static NS_DEFINE_CID(kJSID_CID,  NS_JS_ID_CID);
static NS_DEFINE_CID(kXPCException_CID,  NS_XPCEXCEPTION_CID);
static NS_DEFINE_CID(kXPConnect_CID, NS_XPCONNECT_CID);
static NS_DEFINE_CID(kXPCThreadJSContextStack_CID, NS_XPC_THREAD_JSCONTEXT_STACK_CID);

/********************************************/

NS_GENERIC_FACTORY_CONSTRUCTOR(nsJSID)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsXPCException)

static NS_IMETHODIMP
Construct_nsXPConnect(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsresult rv;
    nsISupports *obj;

    if(!aResult)
    {
        rv = NS_ERROR_NULL_POINTER;
        goto done;
    }
    *aResult = NULL;
    if(aOuter)
    {
        rv = NS_ERROR_NO_AGGREGATION;
        goto done;
    }

    obj = nsXPConnect::GetXPConnect();

    if(!obj)
    {
        rv = NS_ERROR_OUT_OF_MEMORY;
        goto done;
    }

    rv = obj->QueryInterface(aIID, aResult);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to find correct interface");
    NS_RELEASE(obj);
 done:
    return rv;
}

static NS_IMETHODIMP
Construct_nsXPCThreadJSContextStack(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsresult rv;
    nsISupports *obj;

    if(!aResult)
    {
        rv = NS_ERROR_NULL_POINTER;
        goto done;
    }
    *aResult = NULL;
    if(aOuter)
    {
        rv = NS_ERROR_NO_AGGREGATION;
        goto done;
    }

    obj = nsXPCThreadJSContextStackImpl::GetSingleton();

    if(!obj)
    {
        rv = NS_ERROR_OUT_OF_MEMORY;
        goto done;
    }

    rv = obj->QueryInterface(aIID, aResult);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to find correct interface");
    NS_RELEASE(obj);
 done:
    return rv;
}

/********************************************/

extern "C" PR_IMPLEMENT(nsresult)
NSGetFactory(nsISupports* aServMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
    nsresult rv;
    NS_ASSERTION(aFactory != nsnull, "bad factory pointer");

    NS_WITH_SERVICE1(nsIComponentManager, compMgr,
                     aServMgr, kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsIGenericFactory* factory;
    rv = compMgr->CreateInstance(kGenericFactoryCID, nsnull,
                                 nsIGenericFactory::GetIID(),
                                 (void**)&factory);
    if (NS_FAILED(rv)) return rv;

    // add more factories as 'if else's below...

    if(aClass.Equals(kJSID_CID))
        rv = factory->SetConstructor(nsJSIDConstructor);
    else if(aClass.Equals(kXPConnect_CID))
        rv = factory->SetConstructor(Construct_nsXPConnect);
    else if(aClass.Equals(kXPCThreadJSContextStack_CID))
        rv = factory->SetConstructor(Construct_nsXPCThreadJSContextStack);
    else if(aClass.Equals(kXPCException_CID))
        rv = factory->SetConstructor(nsXPCExceptionConstructor);
    else
    {
        NS_ASSERTION(0, "incorrectly registered");
        rv = NS_ERROR_NO_INTERFACE;
    }

    if (NS_FAILED(rv)) {
        NS_RELEASE(factory);
        return rv;
    }
    *aFactory = factory;
    return NS_OK;
}

/***************************************************************************/

extern "C" XPC_PUBLIC_API(PRBool)
NSCanUnload(nsISupports* aServMgr)
{
  return PR_FALSE;
}

extern "C" XPC_PUBLIC_API(nsresult)
NSRegisterSelf(nsISupports* aServMgr, const char *aPath)
{
    nsresult rv;
#ifdef DEBUG
    printf("*** Register XPConnect\n");
#endif
    NS_WITH_SERVICE1(nsIComponentManager, compMgr,
                     aServMgr, kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->RegisterComponent(kJSID_CID,
                                    "nsIJSID","nsID",
                                    aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->RegisterComponent(kXPConnect_CID,
                                    "nsIXPConnect","nsIXPConnect",
                                    aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->RegisterComponent(kXPCThreadJSContextStack_CID,
                                    "nsThreadJSContextStack","nsThreadJSContextStack",
                                    aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->RegisterComponent(kXPCException_CID,
                                    "nsXPCException","nsXPCException",
                                    aPath, PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    return rv;
}

extern "C" XPC_PUBLIC_API(nsresult)
NSUnregisterSelf(nsISupports* aServMgr, const char *aPath)
{
    nsresult rv;
#ifdef DEBUG
    printf("*** Unregister XPConnect\n");
#endif
    NS_WITH_SERVICE1(nsIComponentManager, compMgr,
                     aServMgr, kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->UnregisterComponent(kJSID_CID, aPath);
    rv = compMgr->UnregisterComponent(kXPConnect_CID, aPath);
    rv = compMgr->UnregisterComponent(kXPCThreadJSContextStack_CID, aPath);
    rv = compMgr->UnregisterComponent(kXPCException_CID, aPath);

    return rv;
}

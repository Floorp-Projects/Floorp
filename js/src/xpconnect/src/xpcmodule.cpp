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

/* Module level methods. */

#include "xpcprivate.h"

/***************************************************************************/

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kJSIID_CID, NS_JS_IID_CID);
static NS_DEFINE_CID(kJSCID_CID, NS_JS_CID_CID);
static NS_DEFINE_CID(kXPConnect_CID, NS_XPCONNECT_CID);

class nsXPCFactory : public nsIFactory
{
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD CreateInstance(nsISupports *aOuter,
                              REFNSIID aIID,
                              void **aResult);

    NS_IMETHOD LockFactory(PRBool aLock);

    nsXPCFactory(const nsCID &aCID);
    virtual ~nsXPCFactory();
private:
    nsCID      mCID;
};

nsXPCFactory::nsXPCFactory(const nsCID &aCID)
    : mCID(aCID)
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

nsXPCFactory::~nsXPCFactory()
{
//    NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
}

NS_IMETHODIMP
nsXPCFactory::QueryInterface(const nsIID &aIID, void **aResult)
{
    if(aResult == NULL)
        return NS_ERROR_NULL_POINTER;

    if(aIID.Equals(nsISupports::GetIID()) ||
       aIID.Equals(nsIFactory::GetIID())  ||
       aIID.Equals(nsXPCFactory::GetIID()))
    {
        *aResult = (void*)this;
    }
    else
    {
        *aResult = NULL;
        return NS_NOINTERFACE;
    }

    NS_ADDREF_THIS();
    return NS_OK;
}

NS_IMPL_ADDREF(nsXPCFactory)
NS_IMPL_RELEASE(nsXPCFactory)

NS_IMETHODIMP
nsXPCFactory::CreateInstance(nsISupports *aOuter,
                             const nsIID &aIID,
                             void **aResult)
{
    nsresult res = NS_OK;

    if(aResult == NULL)
        return NS_ERROR_NULL_POINTER;

    *aResult = NULL;

    if(aOuter)
        return NS_NOINTERFACE;

    nsISupports *inst = nsnull;

    // ClassID check happens here
    // Whenever you add a new class that supports an interface, plug it in here

    // ADD NEW CLASSES HERE
    if (mCID.Equals(kJSIID_CID))
    {
        inst = new nsJSIID;
    }
    else if (mCID.Equals(kJSCID_CID))
    {
        inst = new nsJSCID;
    }
    else if (mCID.Equals(kXPConnect_CID))
    {
        inst = nsXPConnect::GetXPConnect();    
    }
    else
        return NS_NOINTERFACE;

    if(inst)
    {
        res = inst->QueryInterface(aIID, aResult);
        NS_RELEASE(inst);
    }
    else
        res = NS_ERROR_OUT_OF_MEMORY;

    return res;
}

NS_IMETHODIMP
nsXPCFactory::LockFactory(PRBool aLock)
{
    return NS_OK;
}

/***************************************************************************/

#if defined(XP_MAC) && defined(MAC_STATIC)
extern "C" XPC_PUBLIC_API(nsresult)
NSGetFactory_XPCONNECT_DLL(nsISupports* servMgr,
                     const nsCID &aClass,
                     const char *aClassName,
                     const char *aProgID,
                     nsIFactory **aFactory)
#else
extern "C" XPC_PUBLIC_API(nsresult)
NSGetFactory(nsISupports* servMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
#endif
{
    static nsXPCFactory iid_factory(kJSIID_CID);
    static nsXPCFactory cid_factory(kJSCID_CID);
    static nsXPCFactory xpc_factory(kXPConnect_CID);

    if(!aFactory)
        return NS_ERROR_NULL_POINTER;

    if(aClass.Equals(kJSIID_CID))
    {
        iid_factory.AddRef();
        *aFactory = &iid_factory;
        return NS_OK;
    }
    if(aClass.Equals(kJSCID_CID))
    {
        cid_factory.AddRef();
        *aFactory = &cid_factory;
        return NS_OK;
    }
    if(aClass.Equals(kXPConnect_CID))
    {
        xpc_factory.AddRef();
        *aFactory = &xpc_factory;
        return NS_OK;
    }

    return NS_ERROR_NO_INTERFACE;
}


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

    rv = compMgr->RegisterComponent(kJSIID_CID,
                                    "nsIJSIID","nsIID",
                                    aPath, PR_TRUE, PR_TRUE);

    rv = compMgr->RegisterComponent(kJSCID_CID,
                                    "nsIJSCID","nsCID",
                                    aPath, PR_TRUE, PR_TRUE);

    rv = compMgr->RegisterComponent(kXPConnect_CID,
                                    "nsIXPConnect","nsIXPConnect",
                                    aPath, PR_TRUE, PR_TRUE);
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

    rv = compMgr->UnregisterComponent(kJSIID_CID, aPath);
    rv = compMgr->UnregisterComponent(kJSCID_CID, aPath);
    rv = compMgr->UnregisterComponent(kXPConnect_CID, aPath);

    return rv;
}        

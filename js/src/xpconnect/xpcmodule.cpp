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

class nsJSIIDFactory : public nsIFactory
{
public:
    static REFNSCID GetCID() {static nsID cid = NS_JS_IID_CID; return cid;}

    NS_DECL_ISUPPORTS;
    NS_IMETHOD CreateInstance(nsISupports *aOuter,
                              REFNSIID aIID,
                              void **aResult);

    NS_IMETHOD LockFactory(PRBool aLock);

  nsJSIIDFactory();
  virtual ~nsJSIIDFactory();
};

/********************************************/

static NS_DEFINE_IID(kJSIIDFactoryIID, NS_IFACTORY_IID);
NS_IMPL_ISUPPORTS(nsJSIIDFactory, kJSIIDFactoryIID);

NS_IMETHODIMP
nsJSIIDFactory::CreateInstance(nsISupports *aOuter,
                                 REFNSIID aIID,
                                 void **aResult)
{
    if(!aResult)
        return NS_ERROR_NULL_POINTER;

    *aResult = NULL;

    if(aOuter)
        return NS_ERROR_NO_INTERFACE;

    if(aIID.Equals(nsISupports::GetIID()) ||
       aIID.Equals(nsJSIID::GetIID()))
    {
        *aResult = new nsJSIID;
    }

    return *aResult ? NS_OK : NS_ERROR_NO_INTERFACE;
}

NS_IMETHODIMP
nsJSIIDFactory::LockFactory(PRBool aLock)
{
    return NS_OK;
}

nsJSIIDFactory::nsJSIIDFactory()
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

nsJSIIDFactory::~nsJSIIDFactory() {}

/***************************************************************************/
class nsJSCIDFactory : public nsIFactory
{
public:
    static REFNSCID GetCID() {static nsID cid = NS_JS_CID_CID; return cid;}

    NS_DECL_ISUPPORTS;
    NS_IMETHOD CreateInstance(nsISupports *aOuter,
                              REFNSIID aIID,
                              void **aResult);

    NS_IMETHOD LockFactory(PRBool aLock);

  nsJSCIDFactory();
  virtual ~nsJSCIDFactory();
};

/********************************************/

static NS_DEFINE_IID(kJSCIDFactoryIID, NS_IFACTORY_IID);
NS_IMPL_ISUPPORTS(nsJSCIDFactory, kJSCIDFactoryIID);

NS_IMETHODIMP
nsJSCIDFactory::CreateInstance(nsISupports *aOuter,
                                 REFNSIID aIID,
                                 void **aResult)
{
    if(!aResult)
        return NS_ERROR_NULL_POINTER;

    *aResult = NULL;

    if(aOuter)
        return NS_ERROR_NO_INTERFACE;

    if(aIID.Equals(nsISupports::GetIID()) ||
       aIID.Equals(nsJSCID::GetIID()))
    {
        *aResult = new nsJSCID;
    }

    return *aResult ? NS_OK : NS_ERROR_NO_INTERFACE;
}

NS_IMETHODIMP
nsJSCIDFactory::LockFactory(PRBool aLock)
{
    return NS_OK;
}

nsJSCIDFactory::nsJSCIDFactory()
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

nsJSCIDFactory::~nsJSCIDFactory() {}

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
    static nsJSIIDFactory iid_factory;
    static nsJSCIDFactory cid_factory;

    if(!aFactory)
        return NS_ERROR_NULL_POINTER;

    if(aClass.Equals(nsJSIIDFactory::GetCID()))
    {
        iid_factory.AddRef();
        *aFactory = &iid_factory;
        return NS_OK;
    }
    if(aClass.Equals(nsJSCIDFactory::GetCID()))
    {
        cid_factory.AddRef();
        *aFactory = &cid_factory;
        return NS_OK;
    }

    return NS_ERROR_NO_INTERFACE;
}

#ifdef XP_PC
#define XPCONNECT_DLL  "xpc3250.dll"
#else
#ifdef XP_MAC
#define XPCONNECT_DLL  "XPCONNECT_DLL"
#else
#define XPCONNECT_DLL  "libxpconnect.so"
#endif
#endif

void
xpc_RegisterSelf()
{
    nsComponentManager::RegisterComponent(nsJSIIDFactory::GetCID(),
                                          "nsIJSIID",
                                          "nsIID",
                                          XPCONNECT_DLL,
                                          PR_FALSE, PR_TRUE);

    nsComponentManager::RegisterComponent(nsJSCIDFactory::GetCID(),
                                          "nsIJSCID",
                                          "nsCID",
                                          XPCONNECT_DLL,
                                          PR_FALSE, PR_TRUE);
}

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Communications Corporation.	Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIWalletService.h"
#include "nsIServiceManager.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID,  NS_IFACTORY_IID);
static NS_DEFINE_CID(kWalletServiceCID, NS_WALLETSERVICE_CID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

//#if defined(HAS_C_PLUS_PLUS_CASTS)
//#define NS_STATIC_CAST(__type, __ptr)	   static_cast<__type>(__ptr)
//#else
//#define NS_STATIC_CAST(__type, __ptr)	   ((__type)(__ptr))
//#endif

// factory functions
nsresult NS_NewWalletService(nsIWalletService** result);

class WalletFactoryImpl : public nsIFactory
{
public:
    WalletFactoryImpl(const nsCID &aClass);

    // nsISupports methods
    NS_DECL_ISUPPORTS

    // nsIFactory methods
    NS_IMETHOD CreateInstance(nsISupports *aOuter,
			      const nsIID &aIID,
			      void **aResult);

    NS_IMETHOD LockFactory(PRBool aLock);

protected:
    virtual ~WalletFactoryImpl();

private:
    nsCID     mClassID;
};

////////////////////////////////////////////////////////////////////////

WalletFactoryImpl::WalletFactoryImpl(const nsCID &aClass)
{
    NS_INIT_REFCNT();
    mClassID = aClass;
}

WalletFactoryImpl::~WalletFactoryImpl()
{
    NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
}

NS_IMETHODIMP
WalletFactoryImpl::QueryInterface(const nsIID &aIID,
				      void **aResult)
{
    if (! aResult)
	return NS_ERROR_NULL_POINTER;

    // Always NULL result, in case of failure
    *aResult = nsnull;

    if (aIID.Equals(kISupportsIID)) {
	*aResult = NS_STATIC_CAST(nsISupports*, this);
	AddRef();
	return NS_OK;
    } else if (aIID.Equals(kIFactoryIID)) {
        *aResult = NS_STATIC_CAST(nsIFactory*, this);
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(WalletFactoryImpl);
NS_IMPL_RELEASE(WalletFactoryImpl);

NS_IMETHODIMP
WalletFactoryImpl::CreateInstance(nsISupports *aOuter,
			     const nsIID &aIID,
			     void **aResult)
{
    if (! aResult)
	return NS_ERROR_NULL_POINTER;

    if (aOuter)
	return NS_ERROR_NO_AGGREGATION;

    *aResult = nsnull;

    nsresult rv;
    nsISupports *inst = nsnull;
    if (mClassID.Equals(kWalletServiceCID)) {
	if (NS_FAILED(rv = NS_NewWalletService((nsIWalletService**) &inst)))
	    return rv;
    }
    else {
        return NS_ERROR_NO_INTERFACE;
    }

    if (NS_FAILED(rv = inst->QueryInterface(aIID, aResult))) {
        // We didn't get the right interface, so clean up
        delete inst;
    }

	NS_IF_RELEASE(inst);
    return rv;
}

nsresult WalletFactoryImpl::LockFactory(PRBool aLock)
{
    // Not implemented in simplest case.
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////



// return the proper factory to the caller
extern "C" PR_IMPLEMENT(nsresult)
NSGetFactory(nsISupports* serviceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
    if (! aFactory)
	return NS_ERROR_NULL_POINTER;

    WalletFactoryImpl* factory = new WalletFactoryImpl(aClass);
    if (factory == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(factory);
    *aFactory = factory;
    return NS_OK;
}

extern "C" PR_IMPLEMENT(nsresult)
NSRegisterSelf(nsISupports* aServMgr , const char* aPath)
{
    nsresult rv;

    nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
    if (NS_FAILED(rv)) return rv;

    nsIComponentManager* compMgr;
    rv = servMgr->GetService(kComponentManagerCID,
                             nsIComponentManager::GetIID(),
                             (nsISupports**)&compMgr);
    if (NS_FAILED(rv)) return rv;

    // register wallet service
    rv = compMgr->RegisterComponent(kWalletServiceCID,
                                    NS_WALLETSERVICE_CLASSNAME,
                                    NS_WALLETSERVICE_PROGID,
                                    aPath, PR_TRUE, PR_TRUE);

    
    (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
    return rv;
}


extern "C" PR_IMPLEMENT(nsresult)
NSUnregisterSelf(nsISupports* aServMgr , const char* aPath)
{
    nsresult rv;

    nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
    if (NS_FAILED(rv)) return rv;

    nsIComponentManager* compMgr;
    rv = servMgr->GetService(kComponentManagerCID,
                             nsIComponentManager::GetIID(),
                             (nsISupports**)&compMgr);
    if (NS_FAILED(rv)) return rv;

    // unregister wallet component
    rv = compMgr->UnregisterComponent(kWalletServiceCID, aPath);
    
    (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
    return rv;
}


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
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*

  A sample of XPConnect. This file contains the XPCOM factory the
  creates for SignonViewerImpl objects.

*/

#include "nsCOMPtr.h"
#include "nscore.h"
#include "nsISignonViewer.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsXPComFactory.h"

static NS_DEFINE_IID(kISupportsIID,        NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID,         NS_IFACTORY_IID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kSignonViewerCID,            NS_SIGNONVIEWER_CID);

class SignonViewerFactoryImpl : public nsIFactory
{
public:
    SignonViewerFactoryImpl(const nsCID &aClass, const char* className, const char* progID);

    // nsISupports methods
    NS_DECL_ISUPPORTS

    // nsIFactory methods
    NS_IMETHOD CreateInstance(nsISupports *aOuter,
                              const nsIID &aIID,
                              void **aResult);

    NS_IMETHOD LockFactory(PRBool aLock);

protected:
    virtual ~SignonViewerFactoryImpl();

protected:
    nsCID       mClassID;
    const char* mClassName;
    const char* mProgID;
};

////////////////////////////////////////////////////////////////////////

SignonViewerFactoryImpl::SignonViewerFactoryImpl(const nsCID &aClass, 
                                   const char* className,
                                   const char* progID)
    : mClassID(aClass), mClassName(className), mProgID(progID)
{
    NS_INIT_REFCNT();
}

SignonViewerFactoryImpl::~SignonViewerFactoryImpl()
{
    NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
}

NS_IMETHODIMP
SignonViewerFactoryImpl::QueryInterface(const nsIID &aIID, void **aResult)
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

NS_IMPL_ADDREF(SignonViewerFactoryImpl);
NS_IMPL_RELEASE(SignonViewerFactoryImpl);

NS_IMETHODIMP
SignonViewerFactoryImpl::CreateInstance(nsISupports *aOuter,
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
    if (mClassID.Equals(kSignonViewerCID)) {
        if (NS_FAILED(rv = NS_NewSignonViewer((nsISignonViewer**) &inst)))
            return rv;
    }
    else {
        return NS_ERROR_NO_INTERFACE;
    }

    if (! inst)
        return NS_ERROR_OUT_OF_MEMORY;

    if (NS_FAILED(rv = inst->QueryInterface(aIID, aResult))) {
        // We didn't get the right interface.
        NS_ERROR("didn't support the interface you wanted");
    }

    NS_IF_RELEASE(inst);
    return rv;
}

nsresult SignonViewerFactoryImpl::LockFactory(PRBool aLock)
{
    // Not implemented in simplest case.
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////



// return the proper factory to the caller
extern "C" PR_IMPLEMENT(nsresult)
NSGetFactory(nsISupports* aServMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
    if (! aFactory)
        return NS_ERROR_NULL_POINTER;

    SignonViewerFactoryImpl* factory = new SignonViewerFactoryImpl(aClass, aClassName, aProgID);
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

    NS_WITH_SERVICE(nsIComponentManager, compMgr, kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->RegisterComponent(kSignonViewerCID,
                                    "SignonViewer World Component",
                                    "component://netscape/signonviewer/signonviewer-world",
                                    aPath, PR_TRUE, PR_TRUE);

    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


extern "C" PR_IMPLEMENT(nsresult)
NSUnregisterSelf(nsISupports* aServMgr, const char* aPath)
{
    nsresult rv;

    nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
    if (NS_FAILED(rv)) return rv;

    NS_WITH_SERVICE(nsIComponentManager, compMgr, kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->UnregisterComponent(kSignonViewerCID, aPath);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

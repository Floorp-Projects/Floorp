/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License. 
 *
 * The Original Code is Basic Socket Library
 *
 * The Initial Developer of the Original Code is New Dimensions Consulting,
 * Inc. Portions created by New Dimensions Consulting, Inc. Copyright (C) 1999
 * New Dimenstions Consulting, Inc. All Rights Reserved.
 *
 *
 * Contributor(s):
 *  Robert Ginda, rginda@ndcico.com, original author
 */

/*
 * This is a blatent ripoff of nsSampleFactory.cpp
 */

#include "nsCOMPtr.h"
#include "nscore.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsXPComFactory.h"
#include "bsIConnection.h"

static NS_DEFINE_IID(kISupportsIID,        NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID,         NS_IFACTORY_IID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kConnectionCID,       BS_CONNECTION_CID);

class bsConnectionFactory : public nsIFactory
{
public:
    bsConnectionFactory (const nsCID &aClass, const char* className,
                         const char* progID);

    // nsISupports methods
    NS_DECL_ISUPPORTS

    // nsIFactory methods
    NS_IMETHOD CreateInstance(nsISupports *aOuter,
                              const nsIID &aIID,
                              void **aResult);

    NS_IMETHOD LockFactory(PRBool aLock);

protected:
    virtual ~bsConnectionFactory();

protected:
    nsCID       mClassID;
    const char* mClassName;
    const char* mProgID;
};

////////////////////////////////////////////////////////////////////////

bsConnectionFactory::bsConnectionFactory(const nsCID &aClass, 
                                   const char* className,
                                   const char* progID)
    : mClassID(aClass), mClassName(className), mProgID(progID)
{
    NS_INIT_REFCNT();
}

bsConnectionFactory::~bsConnectionFactory()
{
    NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
}

NS_IMETHODIMP
bsConnectionFactory::QueryInterface(const nsIID &aIID, void **aResult)
{
    if (!aResult)
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

NS_IMPL_ADDREF(bsConnectionFactory);
NS_IMPL_RELEASE(bsConnectionFactory);

NS_IMETHODIMP
bsConnectionFactory::CreateInstance(nsISupports *aOuter,
                                 const nsIID &aIID,
                                 void **aResult)
{
    if (!aResult)
        return NS_ERROR_NULL_POINTER;

    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    *aResult = nsnull;

    nsresult rv;

    nsISupports *inst = nsnull;
    if (mClassID.Equals(kConnectionCID)) {
        if (NS_FAILED(rv = BS_NewConnection((bsIConnection**) &inst)))
            return rv;
    }
    else {
        return NS_ERROR_NO_INTERFACE;
    }

    if (!inst)
        return NS_ERROR_OUT_OF_MEMORY;

    if (NS_FAILED(rv = inst->QueryInterface(aIID, aResult))) {
        // We didn't get the right interface.
        NS_ERROR("didn't support the interface you wanted");
    }

    NS_IF_RELEASE(inst);
    return rv;
}

nsresult bsConnectionFactory::LockFactory(PRBool aLock)
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
    if (!aFactory)
        return NS_ERROR_NULL_POINTER;

    bsConnectionFactory* factory = new bsConnectionFactory(aClass,
                                                           aClassName, aProgID);
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

    /*
    nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
    if (NS_FAILED(rv)) return rv;
    */
    
    NS_WITH_SERVICE(nsIComponentManager, compMgr, kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->RegisterComponent(kConnectionCID,
                                    BS_CONNECTION_CLASSNAME,
                                    BS_CONNECTION_PROGID,
                                    aPath, PR_TRUE, PR_FALSE);

    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


extern "C" PR_IMPLEMENT(nsresult)
NSUnregisterSelf(nsISupports* aServMgr, const char* aPath)
{
    nsresult rv;

    /*
    nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
    if (NS_FAILED(rv)) return rv;
    */
    
    NS_WITH_SERVICE(nsIComponentManager, compMgr, kComponentManagerCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = compMgr->UnregisterComponent(kConnectionCID, aPath);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

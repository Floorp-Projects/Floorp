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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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

/* Module level methods. */

#include "xpcprivate.h"

/***************************************************************************/

static NS_DEFINE_CID(kJSID_CID,  NS_JS_ID_CID);
static NS_DEFINE_CID(kXPCException_CID,  NS_XPCEXCEPTION_CID);
static NS_DEFINE_CID(kXPConnect_CID, NS_XPCONNECT_CID);
static NS_DEFINE_CID(kXPCThreadJSContextStack_CID, NS_XPC_THREAD_JSCONTEXT_STACK_CID);
static NS_DEFINE_CID(kJSRuntime_CID, NS_JS_RUNTIME_SERVICE_CID);

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
    *aResult = nsnull;
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
    *aResult = nsnull;
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

static NS_IMETHODIMP
Construct_nsJSRuntimeService(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    nsresult rv;
    nsISupports *obj;

    if(!aResult)
        return NS_ERROR_NULL_POINTER;
    *aResult = nsnull;
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    obj = nsJSRuntimeServiceImpl::GetSingleton();

    if (!obj)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = obj->QueryInterface(aIID, aResult);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to find correct interface");
    NS_RELEASE(obj);
    return rv;
}

/********************************************/

// Module implementation for the xpconnect library

class nsXPConnectModule : public nsIModule
{
public:
    nsXPConnectModule();
    virtual ~nsXPConnectModule();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIMODULE

protected:
    nsresult Initialize();

    void Shutdown();

    PRBool mInitialized;
    nsCOMPtr<nsIGenericFactory> mJSIDFactory;
    nsCOMPtr<nsIGenericFactory> mXPConnectFactory;
    nsCOMPtr<nsIGenericFactory> mXPCThreadJSContextStackFactory;
    nsCOMPtr<nsIGenericFactory> mXPCExceptionFactory;
    nsCOMPtr<nsIGenericFactory> mJSRuntimeServiceFactory;
};

static NS_DEFINE_IID(kIModuleIID, NS_IMODULE_IID);

nsXPConnectModule::nsXPConnectModule()
    : mInitialized(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsXPConnectModule::~nsXPConnectModule()
{
    Shutdown();
}

NS_IMPL_ISUPPORTS(nsXPConnectModule, kIModuleIID)

// Perform our one-time intialization for this module
nsresult
nsXPConnectModule::Initialize()
{
    if (mInitialized) {
        return NS_OK;
    }
    mInitialized = PR_TRUE;
    return NS_OK;
}

// Shutdown this module, releasing all of the module resources
void
nsXPConnectModule::Shutdown()
{
    // Release the factory objects
    mJSIDFactory = nsnull;
    mXPConnectFactory = nsnull;
    mXPCThreadJSContextStackFactory = nsnull;
    mXPCExceptionFactory = nsnull;
    mJSRuntimeServiceFactory = nsnull;

    // Release our singletons
    nsXPCThreadJSContextStackImpl::FreeSingleton();
    nsJSRuntimeServiceImpl::FreeSingleton();
    nsXPConnect::ReleaseXPConnectSingleton();
}

// Create a factory object for creating instances of aClass.
NS_IMETHODIMP
nsXPConnectModule::GetClassObject(nsIComponentManager *aCompMgr,
                                  const nsCID& aClass,
                                  const nsIID& aIID,
                                  void** r_classObj)
{
    nsresult rv;

    // Defensive programming: Initialize *r_classObj in case of error below
    if (!r_classObj) {
        return NS_ERROR_INVALID_POINTER;
    }
    *r_classObj = NULL;

    // Do one-time-only initialization if necessary
    if (!mInitialized) {
        rv = Initialize();
        if (NS_FAILED(rv)) {
            // Initialization failed! yikes!
            return rv;
        }
    }

    // Choose the appropriate factory, based on the desired instance
    // class type (aClass).
    nsCOMPtr<nsIGenericFactory> fact;
    if (aClass.Equals(kJSID_CID)) {
        if (!mJSIDFactory.get()) {
            rv = NS_NewGenericFactory(getter_AddRefs(mJSIDFactory),
                                      nsJSIDConstructor);
        }
        fact = mJSIDFactory;
    } else if(aClass.Equals(kXPConnect_CID)) {
        if (!mXPConnectFactory.get()) {
            rv = NS_NewGenericFactory(getter_AddRefs(mXPConnectFactory),
                                      Construct_nsXPConnect);
        }
        fact = mXPConnectFactory;
    } else if(aClass.Equals(kXPCThreadJSContextStack_CID)) {
        if (!mXPCThreadJSContextStackFactory.get()) {
            rv = NS_NewGenericFactory(getter_AddRefs(mXPCThreadJSContextStackFactory),
                                      Construct_nsXPCThreadJSContextStack);
        }
        fact = mXPCThreadJSContextStackFactory;
    } else if(aClass.Equals(kXPCException_CID)) {
        if (!mXPCExceptionFactory.get()) {
            rv = NS_NewGenericFactory(getter_AddRefs(mXPCExceptionFactory),
                                      nsXPCExceptionConstructor);
        }
        fact = mXPCExceptionFactory;
    } else if(aClass.Equals(kJSRuntime_CID)) {
        if (!mJSRuntimeServiceFactory.get()) {
            rv = NS_NewGenericFactory(getter_AddRefs(mJSRuntimeServiceFactory),
                                      Construct_nsJSRuntimeService);
        }
        fact = mJSRuntimeServiceFactory;
    }
    else {
        rv = NS_ERROR_FACTORY_NOT_REGISTERED;
#ifdef DEBUG
        char* cs = aClass.ToString();
        printf("+++ nsXPConnectModule: unable to create factory for %s\n", cs);
        nsCRT::free(cs);
#endif
    }

    if (fact) {
        rv = fact->QueryInterface(aIID, r_classObj);
    }

    return rv;
}

//----------------------------------------

struct Components {
    const char* mDescription;
    const nsID* mCID;
    const char* mProgID;
};

// The list of components we register
// XXX These progids look pretty bogus!
static Components gComponents[] = {
    { "nsIJSID", &kJSID_CID,
      "nsID", },
    { "nsIXPConnect", &kXPConnect_CID,
      "nsIXPConnect", },
    { "nsThreadJSContextStack", &kXPCThreadJSContextStack_CID,
      "nsThreadJSContextStack", },
    { "nsXPCException", &kXPCException_CID,
      "nsXPCException", },
    { "JS Runtime Service", &kJSRuntime_CID,
      "nsJSRuntimeService", },
};
#define NUM_COMPONENTS (sizeof(gComponents) / sizeof(gComponents[0]))

NS_IMETHODIMP
nsXPConnectModule::RegisterSelf(nsIComponentManager *aCompMgr,
                                nsIFileSpec* aPath,
                                const char* registryLocation,
                                const char* componentType)
{
    nsresult rv = NS_OK;

#ifdef DEBUG
    printf("*** Registering xpconnect components\n");
#endif

    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        rv = aCompMgr->RegisterComponentSpec(*cp->mCID, cp->mDescription,
                                             cp->mProgID, aPath, PR_TRUE,
                                             PR_TRUE);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsXPConnectModule: unable to register %s component => %x\n",
                   cp->mDescription, rv);
#endif
            break;
        }
        cp++;
    }

    return rv;
}

NS_IMETHODIMP
nsXPConnectModule::UnregisterSelf(nsIComponentManager* aCompMgr,
                                  nsIFileSpec* aPath,
                                  const char* registryLocation)
{
#ifdef DEBUG
    printf("*** Unregistering xpconnect components\n");
#endif
    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        nsresult rv = aCompMgr->UnregisterComponentSpec(*cp->mCID, aPath);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsXPConnectModule: unable to unregister %s component => %x\n",
                   cp->mDescription, rv);
#endif
        }
        cp++;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXPConnectModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
    if (!okToUnload) {
        return NS_ERROR_INVALID_POINTER;
    }
    *okToUnload = PR_FALSE;
    return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------

static nsXPConnectModule *gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFileSpec* location,
                                          nsIModule** return_cobj)
{
    nsresult rv = NS_OK;

    NS_ASSERTION(return_cobj, "Null argument");
    NS_ASSERTION(gModule == NULL, "nsXPConnectModule: Module already created.");

    // Create an initialize the layout module instance
    nsXPConnectModule *m = new nsXPConnectModule();
    if (!m) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // Increase refcnt and store away nsIModule interface to m in return_cobj
    rv = m->QueryInterface(NS_GET_IID(nsIModule), (void**)return_cobj);
    if (NS_FAILED(rv)) {
        delete m;
        m = nsnull;
    }
    gModule = m;                  // WARNING: Weak Reference
    return rv;
}

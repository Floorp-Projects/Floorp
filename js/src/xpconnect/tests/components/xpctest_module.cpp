/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

/* module registration and factory code. */

#include "xpctest_private.h"
#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kGenericFactoryCID, NS_GENERICFACTORY_CID);

// Module implementation
class nsXPCTestModule : public nsIModule
{
public:
    nsXPCTestModule();
    virtual ~nsXPCTestModule();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIMODULE

protected:
    nsresult Initialize();

    void Shutdown();

    PRBool mInitialized;
    nsCOMPtr<nsIGenericFactory> mEchoFactory;
    nsCOMPtr<nsIGenericFactory> mChildFactory;
    nsCOMPtr<nsIGenericFactory> mNoisyFactory;
    nsCOMPtr<nsIGenericFactory> mStringTestFactory;
    nsCOMPtr<nsIGenericFactory> mOverloadedFactory;
    nsCOMPtr<nsIGenericFactory> mXPCTestObjectReadOnlyFactory;
    nsCOMPtr<nsIGenericFactory> mXPCTestObjectReadWriteFactory;
    nsCOMPtr<nsIGenericFactory> mXPCTestInFactory;
    nsCOMPtr<nsIGenericFactory> mXPCTestOutFactory;
    nsCOMPtr<nsIGenericFactory> mXPCTestInOutFactory;
    nsCOMPtr<nsIGenericFactory> mXPCTestCallJSFactory;
    nsCOMPtr<nsIGenericFactory> mXPCTestConstFactory;
#if (0)
    nsCOMPtr<nsIGenericFactory> mXPCTestContructWithArgsFactory;
    nsCOMPtr<nsIGenericFactory> mXPCTestScriptableFactory;
#endif /* 0 */
    nsCOMPtr<nsIGenericFactory> mXPCTestParentOneFactory;
    nsCOMPtr<nsIGenericFactory> mXPCTestParentTwoFactory;
    nsCOMPtr<nsIGenericFactory> mXPCTestChild2Factory;
    nsCOMPtr<nsIGenericFactory> mXPCTestChild3Factory;
    nsCOMPtr<nsIGenericFactory> mXPCTestChild4Factory;
    nsCOMPtr<nsIGenericFactory> mXPCTestChild5Factory;
    nsCOMPtr<nsIGenericFactory> mArrayTestFactory;
};

//----------------------------------------------------------------------

// Functions used to create new instances of a given object by the
// generic factory.


nsXPCTestModule::nsXPCTestModule()
    : mInitialized(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsXPCTestModule::~nsXPCTestModule()
{
    Shutdown();
}

NS_IMPL_ISUPPORTS(nsXPCTestModule, NS_GET_IID(nsIModule))

// Perform our one-time intialization for this module
nsresult
nsXPCTestModule::Initialize()
{
    if (mInitialized) {
        return NS_OK;
    }
    mInitialized = PR_TRUE;
    return NS_OK;
}

// Shutdown this module, releasing all of the module resources
void
nsXPCTestModule::Shutdown()
{
    // Release the factory objects
    mEchoFactory = nsnull;
    mChildFactory = nsnull;
    mNoisyFactory = nsnull;
    mStringTestFactory = nsnull;
    mOverloadedFactory = nsnull;
    mXPCTestObjectReadOnlyFactory = nsnull;
    mXPCTestObjectReadWriteFactory = nsnull;
    mXPCTestInFactory = nsnull;
    mXPCTestOutFactory = nsnull;
    mXPCTestInOutFactory = nsnull;
    mXPCTestCallJSFactory = nsnull;
    mXPCTestConstFactory = nsnull;
#if (0)
    mXPCTestContructWithArgsFactory = nsnull;
    mXPCTestScriptableFactory = nsnull;
#endif /* 0 */
    mXPCTestParentOneFactory = nsnull;
    mXPCTestParentTwoFactory = nsnull;
    mXPCTestChild2Factory = nsnull;
    mXPCTestChild3Factory = nsnull;
    mXPCTestChild4Factory = nsnull;
    mXPCTestChild5Factory = nsnull;
    mArrayTestFactory = nsnull;
}

// Create a factory object for creating instances of aClass.
NS_IMETHODIMP
nsXPCTestModule::GetClassObject(nsIComponentManager *aCompMgr,
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
    if (aClass.Equals(xpctest::GetEchoCID())) {
        if (!mEchoFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mEchoFactory),
                                      xpctest::ConstructEcho);
        }
        fact = mEchoFactory;
    }
    else if (aClass.Equals(xpctest::GetChildCID())) {
        if (!mChildFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mChildFactory),
                                      xpctest::ConstructChild);
        }
        fact = mChildFactory;
    }
    else if (aClass.Equals(xpctest::GetNoisyCID())) {
        if (!mNoisyFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mNoisyFactory),
                                      xpctest::ConstructNoisy);
        }
        fact = mNoisyFactory;
    }
    else if (aClass.Equals(xpctest::GetStringTestCID())) {
        if (!mStringTestFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mStringTestFactory),
                                      xpctest::ConstructStringTest);
        }
        fact = mStringTestFactory;
    }
    else if (aClass.Equals(xpctest::GetOverloadedCID())) {
        if (!mOverloadedFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mOverloadedFactory),
                                      xpctest::ConstructOverloaded);
        }
        fact = mOverloadedFactory;
    }
    else if (aClass.Equals(xpctest::GetXPCTestObjectReadOnlyCID())) {
        if (!mXPCTestObjectReadOnlyFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mXPCTestObjectReadOnlyFactory),
                                      xpctest::ConstructXPCTestObjectReadOnly);
        }
        fact = mXPCTestObjectReadOnlyFactory;
    }
    else if (aClass.Equals(xpctest::GetXPCTestObjectReadWriteCID())) {
        if (!mXPCTestObjectReadWriteFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mXPCTestObjectReadWriteFactory),
                                      xpctest::ConstructXPCTestObjectReadWrite);
        }
        fact = mXPCTestObjectReadWriteFactory;
    }
    else if (aClass.Equals(xpctest::GetXPCTestInCID())) {
        if (!mXPCTestInFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mXPCTestInFactory),
                                      xpctest::ConstructXPCTestIn);
        }
        fact = mXPCTestInFactory;
    }
    else if (aClass.Equals(xpctest::GetXPCTestOutCID())) {
        if (!mXPCTestOutFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mXPCTestOutFactory),
                                      xpctest::ConstructXPCTestOut);
        }
        fact = mXPCTestOutFactory;
    }
    else if (aClass.Equals(xpctest::GetXPCTestInOutCID())) {
        if (!mXPCTestInOutFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mXPCTestInOutFactory),
                                      xpctest::ConstructXPCTestInOut);
        }
        fact = mXPCTestInOutFactory;
    }
    else if (aClass.Equals(xpctest::GetXPCTestCallJSCID())) {
        if (!mXPCTestCallJSFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mXPCTestCallJSFactory),
                                      xpctest::ConstructXPCTestCallJS);
        }
        fact = mXPCTestCallJSFactory;
    }
    else if (aClass.Equals(xpctest::GetXPCTestConstCID())) {
        if (!mXPCTestConstFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mXPCTestConstFactory),
                                      xpctest::ConstructXPCTestConst);
        }
        fact = mXPCTestConstFactory;
    }
#if (0)
    else if (aClass.Equals(xpctest::GetXPCTestConstructWithArgsCID())) {
        if (!mXPCTestConstructWithArgsFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mXPCTestConstructWithArgsFactory),
                                      xpctest::ConstructXPCTestConstructWithArgs);
        }
        fact = mXPCTestConstructWithArgsFactory;
    }
    else if (aClass.Equals(xpctest::GetXPCTestScriptableCID())) {
        if (!mXPCTestScriptableFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mXPCTestScriptableFactory),
                                      xpctest::ConstructXPCTestScriptable);
        }
        fact = mXPCTestScriptableFactory;
    }
#endif /* 0 */
    else if (aClass.Equals(xpctest::GetXPCTestParentOneCID())) {
        if (!mXPCTestParentOneFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mXPCTestParentOneFactory),
                                      xpctest::ConstructXPCTestParentOne);
        }
        fact = mXPCTestParentOneFactory;
    }
    else if (aClass.Equals(xpctest::GetXPCTestParentTwoCID())) {
        if (!mXPCTestParentTwoFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mXPCTestParentTwoFactory),
                                      xpctest::ConstructXPCTestParentTwo);
        }
        fact = mXPCTestParentTwoFactory;
    }
    else if (aClass.Equals(xpctest::GetXPCTestChild2CID())) {
        if (!mXPCTestChild2Factory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mXPCTestChild2Factory),
                                      xpctest::ConstructXPCTestChild2);
        }
        fact = mXPCTestChild2Factory;
    }
    else if (aClass.Equals(xpctest::GetXPCTestChild3CID())) {
        if (!mXPCTestChild3Factory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mXPCTestChild3Factory),
                                      xpctest::ConstructXPCTestChild3);
        }
        fact = mXPCTestChild3Factory;
    }
    else if (aClass.Equals(xpctest::GetXPCTestChild4CID())) {
        if (!mXPCTestChild4Factory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mXPCTestChild4Factory),
                                      xpctest::ConstructXPCTestChild4);
        }
        fact = mXPCTestChild4Factory;
    }
    else if (aClass.Equals(xpctest::GetXPCTestChild5CID())) {
        if (!mXPCTestChild5Factory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mXPCTestChild5Factory),
                                      xpctest::ConstructXPCTestChild5);
        }
        fact = mXPCTestChild5Factory;
    }
    else if (aClass.Equals(xpctest::GetArrayTestCID())) {
        if (!mArrayTestFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mArrayTestFactory),
                                      xpctest::ConstructArrayTest);
        }
        fact = mArrayTestFactory;
    }
    else {
        rv = NS_ERROR_FACTORY_NOT_REGISTERED;
#ifdef DEBUG
        char* cs = aClass.ToString();
        printf("+++ nsXPCTestModule: unable to create factory for %s\n", cs);
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
static Components gComponents[] = {
    { "nsEcho", &xpctest::GetEchoCID(),
      "nsEcho", },
    { "nsChild", &xpctest::GetChildCID(),
      "nsChild", },
    { "nsNoisy", &xpctest::GetNoisyCID(),
      "nsNoisy", },
    { "nsStringTest", &xpctest::GetStringTestCID(),
      "nsStringTest", },
    { "nsOverloaded", &xpctest::GetOverloadedCID(),
      "nsOverloaded", },
    { "xpcTestObjectReadOnly", &xpctest::GetXPCTestObjectReadOnlyCID(),
      "xpcTestObjectReadOnly", },
    { "xpcTestObjectReadWrite", &xpctest::GetXPCTestObjectReadWriteCID(),
      "xpcTestObjectReadWrite", },
    { "xpcTestIn", &xpctest::GetXPCTestInCID(),
      "xpcTestIn", },
    { "xpcTestOut", &xpctest::GetXPCTestOutCID(),
      "xpcTestOut", },
    { "xpcTestInOut", &xpctest::GetXPCTestInOutCID(),
      "xpcTestInOut", },
    { "xpcTestCallJS", &xpctest::GetXPCTestCallJSCID(),
      "xpcTestCallJS", },
    { "xpcTestConst", &xpctest::GetXPCTestConstCID(),
      "xpcTestConst", },
#if 0
    { "xpcTestConstructArgs", &xpctest::GetXPCTestConstructWithArgsCID(),
      "xpcTestConstructArgs", },
    { "xpcTestScriptable", &xpctest::GetXPCTestScriptableCID(),
      "xpcTestScriptable", },
#endif /* 0 */
    { "xpcTestParentOne", &xpctest::GetXPCTestParentOneCID(),
      "xpcTestParentOne", },
    { "xpcTestParentTwo", &xpctest::GetXPCTestParentTwoCID(),
      "xpcTestParentTwo", },
    { "xpcTestChild2", &xpctest::GetXPCTestChild2CID(),
      "xpcTestChild2", },
    { "xpcTestChild3", &xpctest::GetXPCTestChild3CID(),
      "xpcTestChild3", },
    { "xpcTestChild4", &xpctest::GetXPCTestChild4CID(),
      "xpcTestChild4", },
    { "xpcTestChild5", &xpctest::GetXPCTestChild5CID(),
      "xpcTestChild5", },
    { "nsArrayTest", &xpctest::GetArrayTestCID(),
      "nsArrayTest", },
};
#define NUM_COMPONENTS (sizeof(gComponents) / sizeof(gComponents[0]))

NS_IMETHODIMP
nsXPCTestModule::RegisterSelf(nsIComponentManager *aCompMgr,
                             nsIFileSpec* aPath,
                             const char* registryLocation,
                             const char* componentType)
{
    nsresult rv = NS_OK;

#ifdef DEBUG
    printf("*** Registering XPCTest components\n");
#endif

    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        rv = aCompMgr->RegisterComponentSpec(*cp->mCID, cp->mDescription,
                                             cp->mProgID, aPath, PR_TRUE,
                                             PR_TRUE);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsXPCTestModule: unable to register %s component => %x\n",
                   cp->mDescription, rv);
#endif
            break;
        }
        cp++;
    }

    return rv;
}

NS_IMETHODIMP
nsXPCTestModule::UnregisterSelf(nsIComponentManager* aCompMgr,
                               nsIFileSpec* aPath,
                               const char* registryLocation)
{
#ifdef DEBUG
    printf("*** Unregistering XPCTest components\n");
#endif
    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        nsresult rv = aCompMgr->UnregisterComponentSpec(*cp->mCID, aPath);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsXPCTestModule: unable to unregister %s component => %x\n",
                   cp->mDescription, rv);
#endif
        }
        cp++;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXPCTestModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
    if (!okToUnload) {
        return NS_ERROR_INVALID_POINTER;
    }
    *okToUnload = PR_FALSE;
    return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------

static nsXPCTestModule *gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFileSpec* location,
                                          nsIModule** return_cobj)
{
    nsresult rv = NS_OK;

    NS_ENSURE_ARG_POINTER(return_cobj);
    NS_ENSURE_FALSE(gModule, NS_ERROR_FAILURE);

    // Create and initialize the module instance
    nsXPCTestModule *m = new nsXPCTestModule();
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

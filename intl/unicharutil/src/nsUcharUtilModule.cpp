/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsUnicharUtilCIID.h"
#include "nsCaseConversionImp2.h"
#include "nsHankakuToZenkakuCID.h"
#include "nsTextTransformFactory.h"
#include "nsICaseConversion.h"
#include "nsEntityConverter.h"
#include "nsSaveAsCharset.h"
#include "nsUUDll.h"

PRInt32 g_InstanceCount = 0;
PRInt32 g_LockCount = 0;

static NS_DEFINE_CID(kUnicharUtilCID,      NS_UNICHARUTIL_CID);
static NS_DEFINE_CID(kHankakuToZenkakuCID, NS_HANKAKUTOZENKAKU_CID);
static NS_DEFINE_CID(kEntityConverterCID,  NS_ENTITYCONVERTER_CID);
static NS_DEFINE_CID(kSaveAsCharsetCID,    NS_SAVEASCHARSET_CID);

// Module implementation
class nsUcharUtilModule : public nsIModule
{
public:
    nsUcharUtilModule();
    virtual ~nsUcharUtilModule();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIMODULE

protected:
    nsresult Initialize();

    void Shutdown();

    PRBool mInitialized;
    nsCOMPtr<nsIGenericFactory> mHankakuToZenkakuFactory;
    nsCOMPtr<nsIGenericFactory> mUnicharUtilFactory;
    nsCOMPtr<nsIGenericFactory> mEntityConverterFactory;
    nsCOMPtr<nsIGenericFactory> mSaveAsCharsetFactory;
};


//----------------------------------------------------------------------

// Functions used to create new instances of a given object by the
// generic factory.

#define MAKE_CTOR(_name)                                             \
static NS_IMETHODIMP                                                 \
CreateNew##_name(nsISupports* aOuter, REFNSIID aIID, void **aResult) \
{                                                                    \
    if (!aResult) {                                                  \
        return NS_ERROR_INVALID_POINTER;                             \
    }                                                                \
    if (aOuter) {                                                    \
        *aResult = nsnull;                                           \
        return NS_ERROR_NO_AGGREGATION;                              \
    }                                                                \
    nsISupports* inst;                                               \
    nsresult rv = NS_New##_name(&inst);                              \
    if (NS_FAILED(rv)) {                                             \
        *aResult = nsnull;                                           \
        return rv;                                                   \
    }                                                                \
    rv = inst->QueryInterface(aIID, aResult);                        \
    if (NS_FAILED(rv)) {                                             \
        *aResult = nsnull;                                           \
    }                                                                \
    NS_RELEASE(inst);                /* get rid of extra refcnt */   \
    return rv;                                                       \
}


MAKE_CTOR(CaseConversion)
MAKE_CTOR(HankakuToZenkaku)
MAKE_CTOR(EntityConverter)
MAKE_CTOR(SaveAsCharset)

//----------------------------------------------------------------------

static NS_DEFINE_IID(kIModuleIID, NS_IMODULE_IID);

nsUcharUtilModule::nsUcharUtilModule()
    : mInitialized(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsUcharUtilModule::~nsUcharUtilModule()
{
    Shutdown();
}

NS_IMPL_ISUPPORTS(nsUcharUtilModule, kIModuleIID)

// Perform our one-time intialization for this module
nsresult
nsUcharUtilModule::Initialize()
{
    if (mInitialized) {
        return NS_OK;
    }
    mInitialized = PR_TRUE;
    return NS_OK;
}

// Shutdown this module, releasing all of the module resources
void
nsUcharUtilModule::Shutdown()
{
    // Release the factory object
    mHankakuToZenkakuFactory = nsnull;
    mUnicharUtilFactory = nsnull;
    mEntityConverterFactory = nsnull;
    mSaveAsCharsetFactory = nsnull;
}

// Create a factory object for creating instances of aClass.
NS_IMETHODIMP
nsUcharUtilModule::GetClassObject(nsIComponentManager *aCompMgr,
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
    if (aClass.Equals(kUnicharUtilCID)) {
        if (!mUnicharUtilFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mUnicharUtilFactory),
                                      CreateNewCaseConversion);
        }
        fact = mUnicharUtilFactory;
    }
    else
    if (aClass.Equals(kEntityConverterCID)) {
        if (!mEntityConverterFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mEntityConverterFactory),
                                      CreateNewEntityConverter);
        }
        fact = mEntityConverterFactory;
    }
    else
    if (aClass.Equals(kSaveAsCharsetCID)) {
        if (!mSaveAsCharsetFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mSaveAsCharsetFactory),
                                      CreateNewSaveAsCharset);
        }
        fact = mSaveAsCharsetFactory;
    }
    else
    if (aClass.Equals(kHankakuToZenkakuCID)) {
        if(!mHankakuToZenkakuFactory) {
            rv = NS_NewGenericFactory(getter_AddRefs(mHankakuToZenkakuFactory),
                                      CreateNewHankakuToZenkaku);
        }
        fact = mHankakuToZenkakuFactory;
    }
    else {
        rv = NS_ERROR_FACTORY_NOT_REGISTERED;
#ifdef DEBUG
        char* cs = aClass.ToString();
        printf("+++ nsUcharUtilModule: unable to create factory for %s\n", cs);
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
    { "Unichar Utility", &kUnicharUtilCID,
      NS_UNICHARUTIL_PROGID, },
    { "Unicode To Entity Converter", &kEntityConverterCID,
      NS_ENTITYCONVERTER_PROGID, },
    { "Unicode To Charset Converter", &kSaveAsCharsetCID,
      NS_SAVEASCHARSET_PROGID, },
    { "Japanese Hankaku To Zenkaku", &kHankakuToZenkakuCID,
      NS_HANKAKUTOZENKAKU_PROGID, },
};
#define NUM_COMPONENTS (sizeof(gComponents) / sizeof(gComponents[0]))

NS_IMETHODIMP
nsUcharUtilModule::RegisterSelf(nsIComponentManager *aCompMgr,
                             nsIFileSpec* aPath,
                             const char* registryLocation,
                             const char* componentType)
{
    nsresult rv = NS_OK;

#ifdef DEBUG
    printf("*** Registering UcharUtil components\n");
#endif

    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        rv = aCompMgr->RegisterComponentSpec(*cp->mCID, cp->mDescription,
                                             cp->mProgID, aPath, PR_TRUE,
                                             PR_TRUE);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsUcharUtilModule: unable to register %s component => %x\n",
                   cp->mDescription, rv);
#endif
            break;
        }
        cp++;
    }

    return rv;
}

NS_IMETHODIMP
nsUcharUtilModule::UnregisterSelf(nsIComponentManager* aCompMgr,
                               nsIFileSpec* aPath,
                               const char* registryLocation)
{
#ifdef DEBUG
    printf("*** Unregistering UcharUtil components\n");
#endif
    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        nsresult rv = aCompMgr->UnregisterComponentSpec(*cp->mCID, aPath);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsUcharUtilModule: unable to unregister %s component => %x\n",
                   cp->mDescription, rv);
#endif
        }
        cp++;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsUcharUtilModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
    if (!okToUnload) {
        return NS_ERROR_INVALID_POINTER;
    }
    *okToUnload = (g_InstanceCount == 0) && (g_LockCount == 0);
    return NS_OK;
}

//----------------------------------------------------------------------

static nsUcharUtilModule *gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFileSpec* location,
                                          nsIModule** return_cobj)
{
    nsresult rv = NS_OK;

    NS_ASSERTION(return_cobj, "Null argument");
    NS_ASSERTION(gModule == NULL, "nsUcharUtilModule: Module already created.");

    // Create and initialize the module instance
    nsUcharUtilModule *m = new nsUcharUtilModule();
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

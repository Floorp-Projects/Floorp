/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsIWalletPreview.h"
#include "nsISignonViewer.h"
#include "nsICookieViewer.h"
#include "nsIWalletEditor.h"

static NS_DEFINE_CID(kWalletPreviewCID,           NS_WALLETPREVIEW_CID);
static NS_DEFINE_CID(kSignonViewerCID,            NS_SIGNONVIEWER_CID);
static NS_DEFINE_CID(kCookieViewerCID,            NS_COOKIEVIEWER_CID);
static NS_DEFINE_CID(kWalletEditorCID,            NS_WALLETEDITOR_CID);


// Module implementation
class nsWalletViewerModule : public nsIModule
{
public:
    nsWalletViewerModule();
    virtual ~nsWalletViewerModule();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIMODULE

protected:
    nsresult Initialize();

    void Shutdown();

    PRBool mInitialized;
    nsCOMPtr<nsIGenericFactory> mWalletPreviewFactory;
    nsCOMPtr<nsIGenericFactory> mSignonViewerFactory;
    nsCOMPtr<nsIGenericFactory> mCookieViewerFactory;
    nsCOMPtr<nsIGenericFactory> mWalletEditorFactory;
};

//----------------------------------------------------------------------

// Functions used to create new instances of a given object by the
// generic factory.

static NS_IMETHODIMP                 
CreateNewWalletPreview(nsISupports* aOuter, REFNSIID aIID, void **aResult) 
{
    if (!aResult) {                                                  
        return NS_ERROR_INVALID_POINTER;                             
    }
    if (aOuter) {                                                    
        *aResult = nsnull;                                           
        return NS_ERROR_NO_AGGREGATION;                              
    }   
    nsIWalletPreview* inst = nsnull;
    nsresult rv = NS_NewWalletPreview(&inst);
    if (NS_FAILED(rv)) {                                             
        *aResult = nsnull;                                           
        return rv;                                                   
    } 
    rv = inst->QueryInterface(aIID, aResult);
    if (NS_FAILED(rv)) {                                             
        *aResult = nsnull;                                           
    }                                                                
    NS_RELEASE(inst);             /* get rid of extra refcnt */      
    return rv;              
}

static NS_IMETHODIMP                 
CreateNewSignonViewer(nsISupports* aOuter, REFNSIID aIID, void **aResult) 
{
    if (!aResult) {                                                  
        return NS_ERROR_INVALID_POINTER;                             
    }
    if (aOuter) {                                                    
        *aResult = nsnull;                                           
        return NS_ERROR_NO_AGGREGATION;                              
    }   
    nsISignonViewer* inst = nsnull;
    nsresult rv = NS_NewSignonViewer(&inst);
    if (NS_FAILED(rv)) {                                             
        *aResult = nsnull;                                           
        return rv;                                                   
    } 
    rv = inst->QueryInterface(aIID, aResult);
    if (NS_FAILED(rv)) {                                             
        *aResult = nsnull;                                           
    }                                                                
    NS_RELEASE(inst);             /* get rid of extra refcnt */      
    return rv;              
}

static NS_IMETHODIMP                 
CreateNewCookieViewer(nsISupports* aOuter, REFNSIID aIID, void **aResult) 
{
    if (!aResult) {                                                  
        return NS_ERROR_INVALID_POINTER;                             
    }
    if (aOuter) {                                                    
        *aResult = nsnull;                                           
        return NS_ERROR_NO_AGGREGATION;                              
    }   
    nsICookieViewer* inst = nsnull;
    nsresult rv = NS_NewCookieViewer(&inst);
    if (NS_FAILED(rv)) {                                             
        *aResult = nsnull;                                           
        return rv;                                                   
    } 
    rv = inst->QueryInterface(aIID, aResult);
    if (NS_FAILED(rv)) {                                             
        *aResult = nsnull;                                           
    }                                                                
    NS_RELEASE(inst);             /* get rid of extra refcnt */      
    return rv;              
}

static NS_IMETHODIMP                 
CreateNewWalletEditor(nsISupports* aOuter, REFNSIID aIID, void **aResult) 
{
    if (!aResult) {                                                  
        return NS_ERROR_INVALID_POINTER;                             
    }
    if (aOuter) {                                                    
        *aResult = nsnull;                                           
        return NS_ERROR_NO_AGGREGATION;                              
    }   
    nsIWalletEditor* inst = nsnull;
    nsresult rv = NS_NewWalletEditor(&inst);
    if (NS_FAILED(rv)) {                                             
        *aResult = nsnull;                                           
        return rv;                                                   
    } 
    rv = inst->QueryInterface(aIID, aResult);
    if (NS_FAILED(rv)) {                                             
        *aResult = nsnull;                                           
    }                                                                
    NS_RELEASE(inst);             /* get rid of extra refcnt */      
    return rv;              
}


//----------------------------------------------------------------------

nsWalletViewerModule::nsWalletViewerModule()
    : mInitialized(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsWalletViewerModule::~nsWalletViewerModule()
{
    Shutdown();
}

NS_IMPL_ISUPPORTS(nsWalletViewerModule, NS_GET_IID(nsIModule))

// Perform our one-time intialization for this module
nsresult
nsWalletViewerModule::Initialize()
{
    if (mInitialized) {
        return NS_OK;
    }
    mInitialized = PR_TRUE;
    return NS_OK;
}

// Shutdown this module, releasing all of the module resources
void
nsWalletViewerModule::Shutdown()
{
    // Release the factory objects
    mWalletPreviewFactory = nsnull;
    mSignonViewerFactory = nsnull;
    mCookieViewerFactory = nsnull;
    mWalletEditorFactory = nsnull;
}

// Create a factory object for creating instances of aClass.
NS_IMETHODIMP
nsWalletViewerModule::GetClassObject(nsIComponentManager *aCompMgr,
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
    if (aClass.Equals(kWalletPreviewCID)) {
        if (!mWalletPreviewFactory) {
            // Create and save away the factory object for creating
            // new instances of WalletPreview. This way if we are called
            // again for the factory, we won't need to create a new
            // one.
            rv = NS_NewGenericFactory(getter_AddRefs(mWalletPreviewFactory),
                                      CreateNewWalletPreview);
        }
        fact = mWalletPreviewFactory;
    }
    else if (aClass.Equals(kSignonViewerCID)) {
        if (!mSignonViewerFactory) {
            // Create and save away the factory object for creating
            // new instances of SignonViewer. This way if we are called
            // again for the factory, we won't need to create a new
            // one.
            rv = NS_NewGenericFactory(getter_AddRefs(mSignonViewerFactory),
                                      CreateNewSignonViewer);
        }
        fact = mSignonViewerFactory;
    }
    else if (aClass.Equals(kCookieViewerCID)) {
        if (!mCookieViewerFactory) {
            // Create and save away the factory object for creating
            // new instances of CookieViewer. This way if we are called
            // again for the factory, we won't need to create a new
            // one.
            rv = NS_NewGenericFactory(getter_AddRefs(mCookieViewerFactory),
                                      CreateNewCookieViewer);
        }
        fact = mCookieViewerFactory;
    }
    else if (aClass.Equals(kWalletEditorCID)) {
        if (!mWalletEditorFactory) {
            // Create and save away the factory object for creating
            // new instances of WalletEditor. This way if we are called
            // again for the factory, we won't need to create a new
            // one.
            rv = NS_NewGenericFactory(getter_AddRefs(mWalletEditorFactory),
                                      CreateNewWalletEditor);
        }
        fact = mWalletEditorFactory;
    }
    else {
        rv = NS_ERROR_FACTORY_NOT_REGISTERED;
#ifdef DEBUG
        char* cs = aClass.ToString();
        printf("+++ nsWalletViewerModule: unable to create factory for %s\n", cs);
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
    { "WalletPreview World Component", &kWalletPreviewCID,
      "component://netscape/walletpreview/walletpreview-world", },
    { "SignonViewer World Component", &kSignonViewerCID,
      "component://netscape/signonviewer/signonviewer-world", },
    { "CookieViewer World Component", &kCookieViewerCID,
      "component://netscape/cookieviewer/cookieviewer-world", },
    { "WalletEditor World Component", &kWalletEditorCID,
      "component://netscape/walleteditor/walleteditor-world", },
};
#define NUM_COMPONENTS (sizeof(gComponents) / sizeof(gComponents[0]))

NS_IMETHODIMP
nsWalletViewerModule::RegisterSelf(nsIComponentManager *aCompMgr,
                             nsIFileSpec* aPath,
                             const char* registryLocation,
                             const char* componentType)
{
    nsresult rv = NS_OK;

#ifdef DEBUG
    printf("*** Registering walletviewer components\n");
#endif

    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        rv = aCompMgr->RegisterComponentSpec(*cp->mCID, cp->mDescription,
                                             cp->mProgID, aPath, PR_TRUE,
                                             PR_TRUE);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsWalletViewerModule: unable to register %s component => %x\n",
                   cp->mDescription, rv);
#endif
            break;
        }
        cp++;
    }

    return rv;
}

NS_IMETHODIMP
nsWalletViewerModule::UnregisterSelf(nsIComponentManager* aCompMgr,
                               nsIFileSpec* aPath,
                               const char* registryLocation)
{
#ifdef DEBUG
    printf("*** Unregistering walletviewer components\n");
#endif
    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        nsresult rv = aCompMgr->UnregisterComponentSpec(*cp->mCID, aPath);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsWalletViewerModule: unable to unregister %s component => %x\n",
                   cp->mDescription, rv);
#endif
        }
        cp++;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWalletViewerModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
    if (!okToUnload) {
        return NS_ERROR_INVALID_POINTER;
    }
    *okToUnload = PR_FALSE;
    return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------

static nsWalletViewerModule *gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFileSpec* location,
                                          nsIModule** return_cobj)
{
    nsresult rv = NS_OK;

    NS_ENSURE_ARG_POINTER(return_cobj);
    NS_ENSURE_FALSE(gModule, NS_ERROR_FAILURE);

    // Create and initialize the module instance
    nsWalletViewerModule *m = new nsWalletViewerModule();
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

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://wwwt.mozilla.org/NPL/
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


#include "nsCOMPtr.h"

#include "nsIServiceManager.h"
#include "nsIComponentManager.h"

#include "nsEditorCID.h"

#include "nsEditorShell.h"		// for the CID

#include "nsEditor.h"				// for gInstanceCount
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#ifdef ENABLE_EDITOR_API_LOG
#include "nsHTMLEditorLog.h"
#endif // ENABLE_EDITOR_API_LOG

static NS_DEFINE_CID(kHTMLEditorCID,        NS_HTMLEDITOR_CID);
static NS_DEFINE_CID(kEditorShellCID,       NS_EDITORSHELL_CID);


// Module implementation
class nsEditorModule : public nsIModule
{
public:
    nsEditorModule();
    virtual ~nsEditorModule();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIMODULE

protected:
    nsresult Initialize();

    void Shutdown();

    PRBool mInitialized;
    nsCOMPtr<nsIGenericFactory> mEditorFactory;
    nsCOMPtr<nsIGenericFactory> mEditorShellFactory;
};



//----------------------------------------------------------------------

nsEditorModule::nsEditorModule()
    : mInitialized(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsEditorModule::~nsEditorModule()
{
    Shutdown();
}

NS_IMPL_ISUPPORTS(nsEditorModule, NS_GET_IID(nsIModule))

// Perform our one-time intialization for this module
nsresult
nsEditorModule::Initialize()
{
    if (mInitialized) {
        return NS_OK;
    }
    mInitialized = PR_TRUE;
    return NS_OK;
}

// Shutdown this module, releasing all of the module resources
void
nsEditorModule::Shutdown()
{
    // Release the factory objects
    mEditorFactory = nsnull;
    mEditorShellFactory = nsnull;
}


static NS_IMETHODIMP                 
CreateNewEditor(nsISupports* aOuter, REFNSIID aIID, void **aResult) 
{
    if (!aResult) {                                                  
        return NS_ERROR_INVALID_POINTER;                             
    }
    if (aOuter) {                                                    
        *aResult = nsnull;                                           
        return NS_ERROR_NO_AGGREGATION;                              
    }   

	// XXX Fix me.
    // XXX Need to cast to interface first to avoid "ambiguous conversion..." error
    // XXX because of multiple nsISupports in the class hierarchy
	nsISupports *inst;
#ifdef ENABLE_EDITOR_API_LOG
    inst = (nsISupports *)(nsIHTMLEditor*)new nsHTMLEditorLog();
#else
    inst = (nsISupports *)(nsIHTMLEditor*)new nsHTMLEditor();
#endif // ENABLE_EDITOR_API_LOG
    if (inst == NULL) {                                             
        *aResult = nsnull;                                           
        return NS_ERROR_OUT_OF_MEMORY;                                                   
    } 

	NS_ADDREF(inst);
    nsresult rv = inst->QueryInterface(aIID, aResult);
    if (NS_FAILED(rv)) {                                             
        *aResult = nsnull;                                           
    }                                                                
    NS_RELEASE(inst);             /* get rid of extra refcnt */      

    return rv;
}


static NS_IMETHODIMP                 
CreateNewEditorShell(nsISupports* aOuter, REFNSIID aIID, void **aResult) 
{
    if (!aResult) {                                                  
        return NS_ERROR_INVALID_POINTER;                             
    }
    if (aOuter) {                                                    
        *aResult = nsnull;                                           
        return NS_ERROR_NO_AGGREGATION;                              
    }

    nsIEditorShell* inst = nsnull;
    nsresult rv = NS_NewEditorShell(&inst);
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


// Create a factory object for creating instances of aClass.
NS_IMETHODIMP
nsEditorModule::GetClassObject(nsIComponentManager *aCompMgr,
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
    if (aClass.Equals(kHTMLEditorCID)) {
        if (!mEditorFactory) {
            // Create and save away the factory object for creating
            // new instances of EditorFactory. This way if we are called
            // again for the factory, we won't need to create a new
            // one.
            rv = NS_NewGenericFactory(getter_AddRefs(mEditorFactory),
                                      CreateNewEditor);
        }
        fact = mEditorFactory;
    }
    else if (aClass.Equals(kEditorShellCID)) {
        if (!mEditorShellFactory) {
            // Create and save away the factory object for creating
            // new instances of EditorShellFactory. This way if we are called
            // again for the factory, we won't need to create a new
            // one.
            rv = NS_NewGenericFactory(getter_AddRefs(mEditorShellFactory),
                                      CreateNewEditorShell);
        }
        fact = mEditorShellFactory;
    }
    else {
        rv = NS_ERROR_FACTORY_NOT_REGISTERED;
#ifdef DEBUG
        char* cs = aClass.ToString();
        printf("+++ nsEditorModule: unable to create factory for %s\n", cs);
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
    { "HTML Editor", &kHTMLEditorCID,
      "component://netscape/editor/htmleditor", },
    { "Editor Shell Component", &kEditorShellCID,
      "component://netscape/editor/editorshell", },
    { "Editor Shell Spell Checker", &kEditorShellCID,
      "component://netscape/editor/editorspellcheck", },
};
#define NUM_COMPONENTS (sizeof(gComponents) / sizeof(gComponents[0]))

NS_IMETHODIMP
nsEditorModule::RegisterSelf(nsIComponentManager *aCompMgr,
                             nsIFileSpec* aPath,
                             const char* registryLocation,
                             const char* componentType)
{
    nsresult rv = NS_OK;

#ifdef DEBUG
    printf("*** Registering Editor components\n");
#endif

    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        rv = aCompMgr->RegisterComponentSpec(*cp->mCID, cp->mDescription,
                                             cp->mProgID, aPath, PR_TRUE,
                                             PR_TRUE);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsEditorModule: unable to register %s component => %x\n",
                   cp->mDescription, rv);
#endif
            break;
        }
        cp++;
    }

    return rv;
}

NS_IMETHODIMP
nsEditorModule::UnregisterSelf(nsIComponentManager* aCompMgr,
                               nsIFileSpec* aPath,
                               const char* registryLocation)
{
#ifdef DEBUG
    printf("*** Unregistering Editor components\n");
#endif
    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) {
        nsresult rv = aCompMgr->UnregisterComponentSpec(*cp->mCID, aPath);
        if (NS_FAILED(rv)) {
#ifdef DEBUG
            printf("nsEditorModule: unable to unregister %s component => %x\n",
                   cp->mDescription, rv);
#endif
        }
        cp++;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsEditorModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
    if (!okToUnload) {
        return NS_ERROR_INVALID_POINTER;
    }
    // we might want to check nsEditor::gInstanceCount == 0 and count
    // factory locks here one day.
    *okToUnload = PR_FALSE;
    return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------

static nsEditorModule *gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFileSpec* location,
                                          nsIModule** return_cobj)
{
    nsresult rv = NS_OK;

    NS_ENSURE_ARG_POINTER(return_cobj);
    NS_ENSURE_FALSE(gModule, NS_ERROR_FAILURE);

    // Create and initialize the module instance
    nsEditorModule *m = new nsEditorModule();
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

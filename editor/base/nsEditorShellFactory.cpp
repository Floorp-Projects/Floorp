/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */


#include "nsEditorShellFactory.h"
#include "nsEditorShell.h"
#include "nsEditor.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsXPComFactory.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

static NS_DEFINE_IID(kEditorShellCID,      NS_EDITORSHELL_CID);

/////////////////////////////////////////////////////////////////////////
// nsEditorShellFactoryImpl
/////////////////////////////////////////////////////////////////////////

nsEditorShellFactoryImpl::nsEditorShellFactoryImpl(const nsCID &aClass, 
                                   const char* className,
                                   const char* progID)
    : mClassID(aClass), mClassName(className), mProgID(progID)

{
    NS_INIT_REFCNT();
}

nsEditorShellFactoryImpl::~nsEditorShellFactoryImpl(void)
{
    NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
}


NS_IMETHODIMP 
nsEditorShellFactoryImpl::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
    if (aInstancePtr == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

    // Always NULL result, in case of failure
    *aInstancePtr = NULL;

    if ( aIID.Equals(kISupportsIID) )
    {
        *aInstancePtr = NS_STATIC_CAST(nsISupports*, this);
    }
    else if ( aIID.Equals(kIFactoryIID) )
    {
        *aInstancePtr = NS_STATIC_CAST(nsIFactory*, this);
    }

    if (*aInstancePtr == NULL)
    {
        return NS_ERROR_NO_INTERFACE;
    }

    AddRef();
    return NS_OK;
}



NS_IMPL_ADDREF(nsEditorShellFactoryImpl)
NS_IMPL_RELEASE(nsEditorShellFactoryImpl)


NS_IMETHODIMP
nsEditorShellFactoryImpl::CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (!aResult)
        return NS_ERROR_NULL_POINTER;

    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    *aResult = NULL;

    nsresult rv;

    nsISupports *inst = nsnull;
    if (mClassID.Equals(kEditorShellCID)) {
        if (NS_FAILED(rv = NS_NewEditorShell((nsIEditorShell**) &inst)))
            return rv;
    }
    else {
        return NS_ERROR_NO_INTERFACE;
    }

    if (NS_FAILED(rv = inst->QueryInterface(aIID, aResult))) {
        // We didn't get the right interface.
        NS_ERROR("didn't support the interface you wanted");
    }

    NS_IF_RELEASE(inst);
    return rv;
}

NS_IMETHODIMP
nsEditorShellFactoryImpl::LockFactory(PRBool aLock)
{
    // Not implemented in simplest case.
    return NS_OK;
}


nsresult
GetEditorShellFactory(nsIFactory **aFactory, const nsCID &aClass, const char *aClassName, const char *aProgID)
{
  PR_EnterMonitor(GetEditorMonitor());

  nsEditorShellFactoryImpl* factory = new nsEditorShellFactoryImpl(aClass, aClassName, aProgID);
  if (!factory)
    return NS_ERROR_OUT_OF_MEMORY;
  nsCOMPtr<nsIFactory> pNSIFactory (do_QueryInterface(factory));
  if (!pNSIFactory)
    return NS_ERROR_NO_INTERFACE;

  nsresult result = pNSIFactory->QueryInterface(kIFactoryIID,
                                                (void **)aFactory);
  PR_ExitMonitor(GetEditorMonitor());
  return result;
}


//#define EDITOR_SHELL_STANDALONE

#if EDITOR_SHELL_STANDALONE

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

    nsEditorShellFactoryImpl* factory = new nsEditorShellFactoryImpl(aClass, aClassName, aProgID);
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

    rv = compMgr->RegisterComponent(kEditorAppCoreCID,
                                    "Editor Shell Component",
                                    "component://netscape/editor/editorshell",
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

    rv = compMgr->UnregisterComponent(kEditorAppCoreCID, aPath);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

#endif

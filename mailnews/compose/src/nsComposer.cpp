/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsComposer.h"
#include "nsComposerNameSet.h"
#include "nsIScriptNameSetRegistry.h"

#include "nsDOMCID.h"

static NS_DEFINE_IID(kCScriptNameSetRegistryCID, NS_SCRIPT_NAMESET_REGISTRY_CID);

class nsComposerBootstrap : public nsIAppShellService {
  
public:
  nsComposerBootstrap(nsIServiceManager *serviceManager);
  virtual ~nsComposerBootstrap();
  
  NS_DECL_ISUPPORTS
  
  // nsIAppShellService 
  // Initialize() is the only one we care about
  NS_IMETHOD Initialize();

private:
  nsIServiceManager *mServiceManager;

  
private:
  NS_IMETHOD Run(void) { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD GetNativeEvent(void *& aEvent,
                            nsIWebShellWindow* aWidget,
                            PRBool &aIsInWindow,
                            PRBool &aIsMouseEvent)
    { return NS_ERROR_NOT_IMPLEMENTED; }
  
  NS_IMETHOD DispatchNativeEvent(void * aEvent)
    { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD Shutdown(void)
    { return NS_ERROR_NOT_IMPLEMENTED; }

  NS_IMETHOD CreateTopLevelWindow(nsIWebShellWindow * aParent,
                                  nsIURL* aUrl, 
                                  nsString& aControllerIID,
                                  nsIWebShellWindow*& aResult, nsIStreamObserver* anObserver,
                                  nsIXULWindowCallbacks *aCallbacks,
                                  PRInt32 aInitialWidth, PRInt32 aInitialHeight)
    { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD CreateDialogWindow(  nsIWebShellWindow * aParent,
                                  nsIURL* aUrl, 
                                  nsString& aControllerIID,
                                  nsIWebShellWindow*& aResult,
                                  nsIStreamObserver* anObserver,
                                  nsIXULWindowCallbacks *aCallbacks,
                                  PRInt32 aInitialWidth, PRInt32 aInitialHeight)
    { return NS_ERROR_NOT_IMPLEMENTED; }
  
  NS_IMETHOD CloseTopLevelWindow(nsIWebShellWindow* aWindow)
    { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD RegisterTopLevelWindow(nsIWebShellWindow* aWindow)
    { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD UnregisterTopLevelWindow(nsIWebShellWindow* aWindow)
    { return NS_ERROR_NOT_IMPLEMENTED; }

};

NS_IMPL_ISUPPORTS(nsComposerBootstrap, nsIAppShellService::GetIID())

nsComposerBootstrap::nsComposerBootstrap(nsIServiceManager *serviceManager)
  : mServiceManager(serviceManager)
{
  NS_INIT_REFCNT();
  
  if (mServiceManager) NS_ADDREF(mServiceManager);
}

nsComposerBootstrap::~nsComposerBootstrap()
{
  NS_IF_RELEASE(mServiceManager);

}

nsresult
nsComposerBootstrap::Initialize()
{
  nsresult rv;

  printf("Composer has been bootstrapped!\n");
  nsIScriptNameSetRegistry* registry;
  rv = nsServiceManager::GetService(kCScriptNameSetRegistryCID, 
                                    nsIScriptNameSetRegistry::GetIID(),
                                    (nsISupports**)&registry);
  if (NS_FAILED(rv))
    return rv;
  nsComposerNameSet* nameSet = new nsComposerNameSet();
  if (nameSet == nsnull)
    rv = NS_ERROR_OUT_OF_MEMORY;
  else
    rv = registry->AddExternalNameSet(nameSet);
  (void)nsServiceManager::ReleaseService(kCScriptNameSetRegistryCID, 
                                         registry);
  return rv;
}


nsresult
NS_NewComposerBootstrap(nsIAppShellService **msgboot,
                         nsIServiceManager *serviceManager)
{
  if (!msgboot) return NS_ERROR_NULL_POINTER;
  if (!serviceManager) return NS_ERROR_NULL_POINTER;
  
  nsComposerBootstrap *bootstrap =
    new nsComposerBootstrap(serviceManager);

  if (!bootstrap) return NS_ERROR_OUT_OF_MEMORY;

  
  return bootstrap->QueryInterface(nsIAppShellService::GetIID(),
                                   (void **)msgboot);

}

// nsComposer implementation

class nsComposer : public nsIComposer {

public:
    NS_DECL_ISUPPORTS;
};

NS_IMPL_ISUPPORTS(nsComposer, nsIComposer::GetIID())


nsresult
NS_NewComposer(nsIComposer **msg)
{
  if (!msg) return NS_ERROR_NULL_POINTER;
  nsComposer *composer = 
    new nsComposer();
  if (!composer) return NS_ERROR_OUT_OF_MEMORY;
  return composer->QueryInterface(nsIComposer::GetIID(),
                                   (void**)&msg);
}


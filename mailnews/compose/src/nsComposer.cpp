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
#include "nsIAppShellComponent.h"

#include "nsDOMCID.h"

static NS_DEFINE_IID(kCScriptNameSetRegistryCID, NS_SCRIPT_NAMESET_REGISTRY_CID);

class nsComposerBootstrap : public nsIAppShellComponent {
  
public:
  nsComposerBootstrap(nsIServiceManager *serviceManager);
  virtual ~nsComposerBootstrap();
  
  NS_DECL_ISUPPORTS

  NS_DECL_IAPPSHELLCOMPONENT

private:
  nsIServiceManager *mServiceManager;

};

NS_IMPL_ISUPPORTS(nsComposerBootstrap, nsIAppShellComponent::GetIID())

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
nsComposerBootstrap::Initialize(nsIAppShellService*,
                                nsICmdLineService*)
{
  nsresult rv;

  printf("Composer has been bootstrapped!\n");
  NS_WITH_SERVICE(nsIScriptNameSetRegistry, registry, kCScriptNameSetRegistryCID, &rv); 

  if (NS_FAILED(rv)) return rv;

  nsComposerNameSet* nameSet = new nsComposerNameSet();
  if (nameSet == nsnull)
    rv = NS_ERROR_OUT_OF_MEMORY;
  else
    rv = registry->AddExternalNameSet(nameSet);

  return rv;
}

nsresult
nsComposerBootstrap::Shutdown()
{
  return NS_OK;
}

nsresult
NS_NewComposerBootstrap(const nsIID &aIID, void **msgboot,
                         nsIServiceManager *serviceManager)
{
  if (!msgboot) return NS_ERROR_NULL_POINTER;
  if (!serviceManager) return NS_ERROR_NULL_POINTER;
  
  nsComposerBootstrap *bootstrap =
    new nsComposerBootstrap(serviceManager);

  if (!bootstrap) return NS_ERROR_OUT_OF_MEMORY;

  
  return bootstrap->QueryInterface(aIID, msgboot);

}

// nsComposer implementation

class nsComposer : public nsIComposer {

public:
    NS_DECL_ISUPPORTS
};

NS_IMPL_ISUPPORTS(nsComposer, nsIComposer::GetIID())


nsresult
NS_NewComposer(const nsIID &aIID, void **msg)
{
  if (!msg) return NS_ERROR_NULL_POINTER;
  nsComposer *composer = 
    new nsComposer();
  if (!composer) return NS_ERROR_OUT_OF_MEMORY;
  return composer->QueryInterface(aIID, msg);
}


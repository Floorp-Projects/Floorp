/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsURLProperties.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsIURL.h"
#include "nsINetService.h"

static NS_DEFINE_IID(kINetServiceIID, NS_INETSERVICE_IID);
static NS_DEFINE_IID(kIPersistentPropertiesIID, NS_IPERSISTENTPROPERTIES_IID);
static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);

nsURLProperties::nsURLProperties(nsString& aUrl)
{
  mDelegate = nsnull; 
  nsresult res = NS_OK;

  nsINetService* pNetService = nsnull;
  if(NS_SUCCEEDED(res)) 
     res = nsServiceManager::GetService( kNetServiceCID, 
                                         kINetServiceIID, 
                                         (nsISupports**) &pNetService);

  nsIURL* url = nsnull;
  if(NS_SUCCEEDED(res)) 
    res = pNetService->CreateURL(&url, aUrl, nsnull, nsnull, nsnull);
 
  nsIInputStream* in=nsnull;
  if(NS_SUCCEEDED(res)) 
    res = pNetService->OpenBlockingStream(url, nsnull, &in);
    
  
  if(NS_SUCCEEDED(res))
    res = nsComponentManager::CreateInstance(kPersistentPropertiesCID, NULL,
                                         kIPersistentPropertiesIID, 
                                         (void**)&mDelegate);

  if(NS_SUCCEEDED(res))
     res = mDelegate->Load(in);

  if(NS_FAILED(res)) {
    NS_IF_RELEASE(mDelegate);
    mDelegate=nsnull;
  }

  if(pNetService) 
     res = nsServiceManager::ReleaseService( kNetServiceCID, 
                                             pNetService);
  NS_IF_RELEASE(in);
}

nsURLProperties::~nsURLProperties()
{
  NS_IF_RELEASE(mDelegate);
}

NS_IMETHODIMP nsURLProperties::Get(const nsString& aKey, nsString& oValue)
{
  if(mDelegate)
     return mDelegate->GetProperty(aKey, oValue);
  else 
     return NS_ERROR_FAILURE;
}

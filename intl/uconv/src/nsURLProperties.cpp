/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsURLProperties.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsIURL.h"
#include "nsNetUtil.h"

static NS_DEFINE_IID(kIPersistentPropertiesIID, NS_IPERSISTENTPROPERTIES_IID);
static NS_DEFINE_IID(kIOServiceCID, NS_IOSERVICE_CID);

nsIIOService*   nsURLProperties::gIOService = nsnull;
nsrefcnt        nsURLProperties::gRefCnt = 0;

nsURLProperties::nsURLProperties(nsString& aUrl)
{
  mDelegate = nsnull; 
  nsresult res = NS_OK;
  nsIURI* url = nsnull;
  nsIInputStream* in = nsnull;

  if (gRefCnt == 0) {
    res = nsServiceManager::GetService(kIOServiceCID,
                                      NS_GET_IID(nsIIOService),
                                      (nsISupports**)&gIOService);
    if (NS_FAILED(res)) return;
    gRefCnt++;
  }

  nsCAutoString aUrlCString;
  aUrlCString.AssignWithConversion(aUrl);
  res = gIOService->NewURI(aUrlCString.get(), nsnull, &url);
  if (NS_FAILED(res)) return;

  res = NS_OpenURI(&in, url);
  NS_RELEASE(url);
  if (NS_FAILED(res)) return;

  if(NS_SUCCEEDED(res))
    res = nsComponentManager::CreateInstance(kPersistentPropertiesCID, NULL,
                                             kIPersistentPropertiesIID, 
                                             (void**)&mDelegate);

  if(NS_SUCCEEDED(res)) {
     if(in) {
       res = mDelegate->Load(in);
     }
     else {
       res = NS_ERROR_FAILURE;
     }
  }

  if(NS_FAILED(res)) {
    NS_IF_RELEASE(mDelegate);
    mDelegate=nsnull;
  }
  NS_IF_RELEASE(in);
}

nsURLProperties::~nsURLProperties()
{
  NS_IF_RELEASE(mDelegate);
  if (--gRefCnt == 0) {
    nsServiceManager::ReleaseService(kIOServiceCID, gIOService);
    gIOService = nsnull;
  }
}

NS_IMETHODIMP nsURLProperties::Get(const nsString& aKey, nsString& oValue)
{
  if(mDelegate)
     return mDelegate->GetStringProperty(aKey, oValue);
  else 
     return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsURLProperties::DidLoad(PRBool &oDidLoad)
{
  oDidLoad = (mDelegate!=nsnull);
  return NS_OK;
}

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
#include "msgCore.h"
#include "prprf.h"
#include "prmem.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIStringBundle.h"
#include "nsMsgComposeStringBundle.h"
#include "nsIServiceManager.h"
#include "nsIURI.h"

static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

#define COMPOSE_BE_URL       "chrome://messenger/locale/messengercompose/composeMsgs.properties"

nsComposeStringService::nsComposeStringService()
{
  NS_INIT_ISUPPORTS();
}

nsComposeStringService::~nsComposeStringService()
{}

NS_IMPL_THREADSAFE_ADDREF(nsComposeStringService);
NS_IMPL_THREADSAFE_RELEASE(nsComposeStringService);

NS_INTERFACE_MAP_BEGIN(nsComposeStringService)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIMsgStringService)
   NS_INTERFACE_MAP_ENTRY(nsIMsgStringService)
NS_INTERFACE_MAP_END

NS_IMETHODIMP 
nsComposeStringService::GetStringByID(PRInt32 aStringID, PRUnichar ** aString)
{
  nsresult rv = NS_OK;
  
  if (!mComposeStringBundle)
    rv = InitializeStringBundle();

  NS_ENSURE_TRUE(mComposeStringBundle, NS_ERROR_UNEXPECTED);
  if (NS_IS_MSG_ERROR(aStringID))
    aStringID = NS_ERROR_GET_CODE(aStringID);
  NS_ENSURE_SUCCESS(mComposeStringBundle->GetStringFromID(aStringID, aString), NS_ERROR_UNEXPECTED);

  return rv;
}

NS_IMETHODIMP
nsComposeStringService::GetBundle(nsIStringBundle **aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  nsresult rv = NS_OK;
  if (!mComposeStringBundle)
    rv = InitializeStringBundle();
  NS_ENSURE_SUCCESS(rv, rv);

  *aResult = mComposeStringBundle;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}


nsresult
nsComposeStringService::InitializeStringBundle()
{
  nsCOMPtr<nsIStringBundleService> stringService = do_GetService(kStringBundleServiceCID);
  NS_ENSURE_TRUE(stringService, NS_ERROR_FAILURE);

  NS_ENSURE_SUCCESS(stringService->CreateBundle(COMPOSE_BE_URL, nsnull, getter_AddRefs(mComposeStringBundle)), 
                    NS_ERROR_FAILURE);
  return NS_OK;
}


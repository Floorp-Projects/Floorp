/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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

  NS_ENSURE_SUCCESS(stringService->CreateBundle(COMPOSE_BE_URL, getter_AddRefs(mComposeStringBundle)), 
                    NS_ERROR_FAILURE);
  return NS_OK;
}


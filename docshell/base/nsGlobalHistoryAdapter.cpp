/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla gecko code.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg <bsmedberg@covad.net>
 * Portions created by the Initial Developer are Copyright (C) 2004
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsGlobalHistoryAdapter.h"

#include "nsDocShellCID.h"
#include "nsIServiceManagerUtils.h"
#include "nsIComponentRegistrar.h"
#include "nsGlobalHistory2Adapter.h"
#include "nsIURI.h"
#include "nsString.h"

nsresult
nsGlobalHistoryAdapter::Create(nsISupports *aOuter,
                               REFNSIID aIID,
                               void **aResult)
{
  nsresult rv;

  if (aOuter) {
    rv = NS_ERROR_NO_AGGREGATION;
    return rv;
  }

  nsGlobalHistoryAdapter* adapter = new nsGlobalHistoryAdapter();
  if (!adapter) {
    rv = NS_ERROR_OUT_OF_MEMORY;
    return rv;
  }

  NS_ADDREF(adapter);
  rv = adapter->Init();
  if (NS_SUCCEEDED(rv)) {
    rv = adapter->QueryInterface(aIID, aResult);
  }
  NS_RELEASE(adapter);

  return rv;
}

nsresult
nsGlobalHistoryAdapter::RegisterSelf(nsIComponentManager* aCompMgr,
                                     nsIFile* aPath,
                                     const char* aLoaderStr,
                                     const char* aType,
                                     const nsModuleComponentInfo *aInfo)
{
  nsresult rv;
  PRBool registered;
  nsCOMPtr<nsIComponentRegistrar> compReg( do_QueryInterface(aCompMgr) );
  if (!compReg) {
    rv = NS_ERROR_UNEXPECTED;
    return rv;
  }

  rv = compReg->IsContractIDRegistered(NS_GLOBALHISTORY2_CONTRACTID, &registered);
  if (NS_FAILED(rv)) return rv;

  // If the embedder has already registered the contractID, we don't want to
  // register ourself. Ideally the component manager would handle this for us.
  if (registered) {
    rv = NS_OK;
    return rv;
  }

  return compReg->RegisterFactoryLocation(GetCID(),
                                          "nsGlobalHistoryAdapter",
                                          NS_GLOBALHISTORY2_CONTRACTID,
                                          aPath, aLoaderStr, aType);
}                                     

nsGlobalHistoryAdapter::nsGlobalHistoryAdapter()
{ }

nsGlobalHistoryAdapter::~nsGlobalHistoryAdapter()
{ }

nsresult
nsGlobalHistoryAdapter::Init()
{
  nsresult rv;

  nsCOMPtr<nsIComponentRegistrar> compReg;
  rv = NS_GetComponentRegistrar(getter_AddRefs(compReg));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCID *cid;
  rv = compReg->ContractIDToCID(NS_GLOBALHISTORY_CONTRACTID, &cid);
  if (NS_FAILED(rv)) {
    rv = NS_ERROR_FACTORY_NOT_REGISTERED;
    return rv;
  }

  if (cid->Equals(nsGlobalHistory2Adapter::GetCID())) {
    rv = NS_ERROR_FACTORY_NOT_REGISTERED;
    return rv;
  }

  NS_WARNING("Using nsIGlobalHistory2->nsIGlobalHistory adapter.");
  mHistory = do_GetService(NS_GLOBALHISTORY_CONTRACTID, &rv);
  return rv;
}

NS_IMPL_ISUPPORTS1(nsGlobalHistoryAdapter, nsIGlobalHistory2)

NS_IMETHODIMP
nsGlobalHistoryAdapter::AddURI(nsIURI* aURI, PRBool aRedirect,
                               PRBool aToplevel, nsIURI* aReferrer)
{
  NS_ENSURE_ARG_POINTER(aURI);
  nsresult rv;

  // The model is really if we don't know differently then add which basically
  // means we are supposed to try all the things we know not to allow in and
  // then if we don't bail go on and allow it in.  But here lets compare
  // against the most common case we know to allow in and go on and say yes
  // to it.

  PRBool isHTTP = PR_FALSE;
  PRBool isHTTPS = PR_FALSE;

  NS_ENSURE_SUCCESS(rv = aURI->SchemeIs("http", &isHTTP), rv);
  NS_ENSURE_SUCCESS(rv = aURI->SchemeIs("https", &isHTTPS), rv);

  if (!isHTTP && !isHTTPS) {
    PRBool isAbout, isImap, isNews, isMailbox, isViewSource, isChrome, isData;

    rv = aURI->SchemeIs("about", &isAbout);
    rv |= aURI->SchemeIs("imap", &isImap);
    rv |= aURI->SchemeIs("news", &isNews);
    rv |= aURI->SchemeIs("mailbox", &isMailbox);
    rv |= aURI->SchemeIs("view-source", &isViewSource);
    rv |= aURI->SchemeIs("chrome", &isChrome);
    rv |= aURI->SchemeIs("data", &isData);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    if (isAbout || isImap || isNews || isMailbox || isViewSource || isChrome || isData) {
      return NS_OK;
    }
  }

  nsCAutoString spec;
  rv = aURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  return mHistory->AddPage(spec.get());
}

NS_IMETHODIMP
nsGlobalHistoryAdapter::IsVisited(nsIURI* aURI, PRBool* aRetval)
{
  NS_ENSURE_ARG_POINTER(aURI);

  nsCAutoString spec;
  nsresult rv = aURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  return mHistory->IsVisited(spec.get(), aRetval);
}

NS_IMETHODIMP
nsGlobalHistoryAdapter::SetPageTitle(nsIURI* aURI, const nsAString& aTitle)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

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

#include "nsGlobalHistory2Adapter.h"

#include "nsDocShellCID.h"
#include "nsIServiceManagerUtils.h"
#include "nsIComponentRegistrar.h"
#include "nsGlobalHistoryAdapter.h"
#include "nsIURI.h"
#include "nsString.h"
#include "nsNetUtil.h"

// we should really have a nsIGenericFactory macro to make this a static
// member function

nsresult
nsGlobalHistory2Adapter::Create(nsISupports *aOuter,
                                REFNSIID aIID,
                                void **aResult)
{
  nsresult rv;

  if (aOuter) {
    rv = NS_ERROR_NO_AGGREGATION;
    return rv;
  }

  nsGlobalHistory2Adapter* adapter = new nsGlobalHistory2Adapter();
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
nsGlobalHistory2Adapter::RegisterSelf(nsIComponentManager* aCompMgr,
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

  rv = compReg->IsContractIDRegistered(NS_GLOBALHISTORY_CONTRACTID, &registered);
  if (NS_FAILED(rv)) return rv;

  // If the embedder has already registered the contractID, we don't want to
  // register ourself. Ideally the component manager would handle this for us.
  if (registered) {
    rv = NS_OK;
    return rv;
  }

  return compReg->RegisterFactoryLocation(GetCID(),
                                          "nsGlobalHistory2Adapter",
                                          NS_GLOBALHISTORY_CONTRACTID,
                                          aPath, aLoaderStr, aType);
}                                     

nsGlobalHistory2Adapter::nsGlobalHistory2Adapter()
{ }

nsGlobalHistory2Adapter::~nsGlobalHistory2Adapter()
{ }

nsresult
nsGlobalHistory2Adapter::Init()
{
  nsresult rv;

  nsCOMPtr<nsIComponentRegistrar> compReg;
  rv = NS_GetComponentRegistrar(getter_AddRefs(compReg));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCID *cid;
  rv = compReg->ContractIDToCID(NS_GLOBALHISTORY2_CONTRACTID, &cid);
  if (NS_FAILED(rv)) {
    rv = NS_ERROR_FACTORY_NOT_REGISTERED;
    return rv;
  }

  if (cid->Equals(nsGlobalHistoryAdapter::GetCID())) {
    rv = NS_ERROR_FACTORY_NOT_REGISTERED;
    return rv;
  }

  NS_WARNING("Using nsIGlobalHistory->nsIGlobalHistory2 adapter.");
  mHistory = do_GetService(NS_GLOBALHISTORY2_CONTRACTID, &rv);
  return rv;
}

NS_IMPL_ISUPPORTS1(nsGlobalHistory2Adapter, nsIGlobalHistory)

NS_IMETHODIMP
nsGlobalHistory2Adapter::AddPage(const char* aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);
  NS_ENSURE_TRUE(*aURI, NS_ERROR_ILLEGAL_VALUE);

  nsresult rv;

  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), nsDependentCString(aURI));

  if (NS_SUCCEEDED(rv)) {
    rv = mHistory->AddURI(uri, PR_FALSE, PR_FALSE, nsnull);
  }

  return rv;
}

NS_IMETHODIMP
nsGlobalHistory2Adapter::IsVisited(const char* aURI, PRBool* aRetval)
{
  NS_ENSURE_ARG_POINTER(aURI);
  nsresult rv;

  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), nsDependentCString(aURI));

  if (NS_SUCCEEDED(rv)) {
    rv = mHistory->IsVisited(uri, aRetval);
  }

  return rv;
}

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsINodeInfo.h"
#include "nsIServiceManager.h"
#include "nsIXTFElement.h"
#include "nsIXTFElementFactory.h"
#include "nsIXTFService.h"
#include "nsInterfaceHashtable.h"
#include "nsString.h"
#include "nsXTFElementWrapper.h"


////////////////////////////////////////////////////////////////////////
// nsXTFService class 
class nsXTFService : public nsIXTFService
{
protected:
  friend nsresult NS_NewXTFService(nsIXTFService** aResult);
  
  nsXTFService();

public:
  // nsISupports interface
  NS_DECL_ISUPPORTS

  // nsIXTFService interface
  nsresult CreateElement(nsIContent** aResult,
                         already_AddRefed<nsINodeInfo> aNodeInfo);

private:
  nsInterfaceHashtable<nsUint32HashKey, nsIXTFElementFactory> mFactoryHash;
};

//----------------------------------------------------------------------
// implementation:

nsXTFService::nsXTFService()
{
  mFactoryHash.Init(); // XXX this can fail. move to Init()
}

nsresult
NS_NewXTFService(nsIXTFService** aResult)
{
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (! aResult)
    return NS_ERROR_NULL_POINTER;

  nsXTFService* result = new nsXTFService();
  if (! result)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(result);
  *aResult = result;
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ISUPPORTS1(nsXTFService, nsIXTFService)

//----------------------------------------------------------------------
// nsIXTFService methods

nsresult
nsXTFService::CreateElement(nsIContent** aResult,
                            already_AddRefed<nsINodeInfo> aNodeInfo)
{
  nsCOMPtr<nsIXTFElementFactory> factory;

  // Check if we have an xtf factory for the given namespaceid in our cache:
  if (!mFactoryHash.Get(aNodeInfo.get()->NamespaceID(), getter_AddRefs(factory))) {
    // No. See if there is one registered with the component manager:
    nsCAutoString xtf_contract_id(NS_XTF_ELEMENT_FACTORY_CONTRACTID_PREFIX);
    nsAutoString uri;
    aNodeInfo.get()->GetNamespaceURI(uri);
    AppendUTF16toUTF8(uri, xtf_contract_id);
#ifdef DEBUG_xtf_verbose
    printf("Testing for XTF factory at %s\n", xtf_contract_id.get());
#endif
    factory = do_GetService(xtf_contract_id.get());
    if (factory) {
#ifdef DEBUG
      printf("We've got an XTF factory: %s \n", xtf_contract_id.get());
#endif
      // Put into hash:
      mFactoryHash.Put(aNodeInfo.get()->NamespaceID(), factory);
    }
  }
  if (!factory) return NS_ERROR_FAILURE;

  // We have an xtf factory. Now try to create an element for the given tag name:
  nsCOMPtr<nsIXTFElement> elem;
  nsAutoString tagName;
  aNodeInfo.get()->GetName(tagName);
  factory->CreateElement(tagName, getter_AddRefs(elem));
  if (!elem) return NS_ERROR_FAILURE;
  
  // We've got an xtf element. Create a wrapper for it:
  return NS_NewXTFElementWrapper(elem, aNodeInfo, aResult);
}


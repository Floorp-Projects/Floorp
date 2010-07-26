/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Mozilla XTF project.
 *
 * The Initial Developer of the Original Code is
 * Alex Fritze.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex@croczilla.com> (original author)
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


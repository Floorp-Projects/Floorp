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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

#include "nsCOMPtr.h"
#include "nsIXBLService.h"
#include "nsIInputStream.h"
#include "nsINameSpaceManager.h"
#include "nsHashtable.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsXPIDLString.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsNetUtil.h"
#include "plstr.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsIDocument.h"
#include "nsIXMLContentSink.h"
#include "nsLayoutCID.h"
#include "nsXMLDocument.h"
#include "nsHTMLAtoms.h"
#include "nsSupportsArray.h"
#include "nsITextContent.h"

#include "nsIXBLBinding.h"

// Static IIDs/CIDs. Try to minimize these.
static NS_DEFINE_CID(kNameSpaceManagerCID,        NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kXMLDocumentCID,             NS_XMLDOCUMENT_CID);
static NS_DEFINE_CID(kParserCID,                  NS_PARSER_IID); // XXX What's up with this???

class nsBindingManager : public nsIBindingManager
{
  NS_DECL_ISUPPORTS

public:
  nsBindingManager();
  virtual ~nsBindingManager();

  NS_IMETHOD GetBinding(nsIContent* aContent, nsIXBLBinding** aResult);
  NS_IMETHOD SetBinding(nsIContent* aContent, nsIXBLBinding* aBinding);

  NS_IMETHOD ResolveTag(nsIContent* aContent, PRInt32* aNameSpaceID, nsIAtom** aResult);

  NS_IMETHOD GetInsertionPoint(nsIContent* aParent, nsIContent* aChild, nsIContent** aResult);
  NS_IMETHOD GetSingleInsertionPoint(nsIContent* aParent, nsIContent** aResult, 
                                     PRBool* aMultipleInsertionPoints);

  NS_IMETHOD AddLayeredBinding(nsIContent* aContent, const nsString& aURL);
  NS_IMETHOD RemoveLayeredBinding(nsIContent* aContent, const nsString& aURL);

  NS_IMETHOD AddToAttachedQueue(nsIXBLBinding* aBinding);
  NS_IMETHOD ClearAttachedQueue();
  NS_IMETHOD ProcessAttachedQueue();

// MEMBER VARIABLES
protected: 
  nsSupportsHashtable* mBindingTable;
  nsSupportsHashtable* mDocumentTable;
  nsCOMPtr<nsISupportsArray> mAttachedQueue;
};


// Implementation /////////////////////////////////////////////////////////////////

// Static member variable initialization

// Implement our nsISupports methods
NS_IMPL_ISUPPORTS1(nsBindingManager, nsIBindingManager)

// Constructors/Destructors
nsBindingManager::nsBindingManager(void)
{
  NS_INIT_REFCNT();

  mBindingTable = nsnull;
  mDocumentTable = nsnull;
  mAttachedQueue = nsnull;
}

nsBindingManager::~nsBindingManager(void)
{
  delete mBindingTable;
  delete mDocumentTable;
}

NS_IMETHODIMP
nsBindingManager::GetBinding(nsIContent* aContent, nsIXBLBinding** aResult) 
{ 
  *aResult = nsnull;
  if (mBindingTable) {
    nsISupportsKey key(aContent);
    nsCOMPtr<nsIXBLBinding> binding;
    binding = dont_AddRef(NS_STATIC_CAST(nsIXBLBinding*, mBindingTable->Get(&key)));
    if (binding) {
      *aResult = binding;
      NS_ADDREF(*aResult);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::SetBinding(nsIContent* aContent, nsIXBLBinding* aBinding )
{
  if (!mBindingTable)
    mBindingTable = new nsSupportsHashtable;

  nsISupportsKey key(aContent);
  if (aBinding) {
    mBindingTable->Put (&key, aBinding);
  }
  else
    mBindingTable->Remove(&key);

  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::ResolveTag(nsIContent* aContent, PRInt32* aNameSpaceID, nsIAtom** aResult)
{
  nsCOMPtr<nsIXBLBinding> binding;
  GetBinding(aContent, getter_AddRefs(binding));
  
  if (binding) {
    nsCOMPtr<nsIAtom> tag;
    binding->GetBaseTag(aNameSpaceID, getter_AddRefs(tag));
    if (tag) {
      *aResult = tag;
      NS_ADDREF(*aResult);
      return NS_OK;
    }
  }

  aContent->GetNameSpaceID(*aNameSpaceID);
  return aContent->GetTag(*aResult);
}

NS_IMETHODIMP
nsBindingManager::GetInsertionPoint(nsIContent* aParent, nsIContent* aChild, nsIContent** aResult)
{
  nsCOMPtr<nsIXBLBinding> binding;
  GetBinding(aParent, getter_AddRefs(binding));
  
  if (binding)
    return binding->GetInsertionPoint(aChild, aResult);
  
  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::GetSingleInsertionPoint(nsIContent* aParent, nsIContent** aResult,
                                          PRBool* aMultipleInsertionPoints)
{
  nsCOMPtr<nsIXBLBinding> binding;
  GetBinding(aParent, getter_AddRefs(binding));
  
  if (binding)
    return binding->GetSingleInsertionPoint( aResult, aMultipleInsertionPoints);
  
  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::AddLayeredBinding(nsIContent* aContent, const nsString& aURL)
{
  // First we need to load our binding.
  nsresult rv;
  NS_WITH_SERVICE(nsIXBLService, xblService, "component://netscape/xbl", &rv);
  if (!xblService)
    return rv;

  // Load the bindings.
  nsCOMPtr<nsIXBLBinding> binding;
  xblService->LoadBindings(aContent, aURL, PR_TRUE, getter_AddRefs(binding));
  if (binding) {
    AddToAttachedQueue(binding);
    ProcessAttachedQueue();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::RemoveLayeredBinding(nsIContent* aContent, const nsString& aURL)
{
  /*
  nsCOMPtr<nsIXBLBinding> binding;
  GetBinding(aParent, getter_AddRefs(binding));
  
  nsCOMPtr<nsIXBLBinding> prevBinding;
    
  while (binding) {
    nsCOMPtr<nsIXBLBinding> nextBinding;
    binding->GetBaseBinding(getter_AddRefs(nextBinding));

    PRBool style;
    binding->IsStyleBinding(&style);
    if (!style) {
       // Remove only our binding.
      if (prevBinding) {
        prevBinding->SetBaseBinding(nextBinding);

        // XXX Unhooking the binding should kill event handlers and
        // fix up the prototype chain.
        // e.g., binding->UnhookEventHandlers(); 
        //       binding->FixupPrototypeChain();
        // or maybe just binding->Unhook();

      }
      else SetBinding(aContent, nextBinding);
    }

    prevBinding = binding;
    binding = nextBinding;
  }
*/
  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::AddToAttachedQueue(nsIXBLBinding* aBinding)
{
  if (!mAttachedQueue)
    NS_NewISupportsArray(getter_AddRefs(mAttachedQueue)); // This call addrefs the array.

  mAttachedQueue->AppendElement(aBinding);

  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::ClearAttachedQueue()
{
  if (mAttachedQueue)
    mAttachedQueue->Clear();
  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::ProcessAttachedQueue()
{
  if (!mAttachedQueue)
    return NS_OK;

  PRUint32 count;
  mAttachedQueue->Count(&count);
  for (PRUint32 i = 0; i < count; i++) {
    nsCOMPtr<nsIXBLBinding> binding;
    mAttachedQueue->GetElementAt(i, getter_AddRefs(binding));
    binding->ExecuteAttachedHandler();
  }

  ClearAttachedQueue();
  return NS_OK;
}

// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewBindingManager(nsIBindingManager** aResult)
{
  *aResult = new nsBindingManager;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}


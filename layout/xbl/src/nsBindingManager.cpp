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
#include "nsIStreamListener.h"
#include "nsIStyleRuleSupplier.h"

#include "nsIXBLBinding.h"
#include "nsIXBLDocumentInfo.h"
#include "nsIXBLBindingAttachedHandler.h"

#include "nsIStyleSheet.h"
#include "nsIHTMLStyleSheet.h"
#include "nsIHTMLCSSStyleSheet.h"
#include "nsIHTMLContentContainer.h"

#include "nsIStyleRuleProcessor.h"
#include "nsIStyleSet.h"
#include "nsIXBLPrototypeHandler.h"

// Static IIDs/CIDs. Try to minimize these.
static NS_DEFINE_CID(kNameSpaceManagerCID,        NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kXMLDocumentCID,             NS_XMLDOCUMENT_CID);
static NS_DEFINE_CID(kParserCID,                  NS_PARSER_IID); // XXX What's up with this???

class nsXBLDocumentInfo : public nsIXBLDocumentInfo
{
public:
  NS_DECL_ISUPPORTS
  
  nsXBLDocumentInfo(nsIDocument* aDocument);
  virtual ~nsXBLDocumentInfo();
  
  NS_IMETHOD GetDocument(nsIDocument** aResult) { *aResult = mDocument; NS_IF_ADDREF(*aResult); return NS_OK; };
  NS_IMETHOD GetRuleProcessors(nsISupportsArray** aResult);
  
  NS_IMETHOD GetScriptAccess(PRBool* aResult) { *aResult = mScriptAccess; return NS_OK; };
  NS_IMETHOD SetScriptAccess(PRBool aAccess) { mScriptAccess = aAccess; return NS_OK; };

  NS_IMETHOD GetPrototypeHandler(const nsCString& aRef, nsIXBLPrototypeHandler** aResult);
  NS_IMETHOD SetPrototypeHandler(const nsCString& aRef, nsIXBLPrototypeHandler* aHandler);

private:
  nsCOMPtr<nsIDocument> mDocument;
  nsCOMPtr<nsISupportsArray> mRuleProcessors;
  PRBool mScriptAccess;
  nsSupportsHashtable* mHandlerTable;
};

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsXBLDocumentInfo, nsIXBLDocumentInfo)

nsXBLDocumentInfo::nsXBLDocumentInfo(nsIDocument* aDocument)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
  mDocument = aDocument;
  mScriptAccess = PR_TRUE;
  mHandlerTable = nsnull;
}

nsXBLDocumentInfo::~nsXBLDocumentInfo()
{
  /* destructor code */
  delete mHandlerTable;
}

NS_IMETHODIMP
nsXBLDocumentInfo::GetRuleProcessors(nsISupportsArray** aResult)
{
  if (!mRuleProcessors) {
    // Gather the rule processors.
    PRInt32 count = mDocument->GetNumberOfStyleSheets();
    if (count > 2) {
      nsCOMPtr<nsIHTMLContentContainer> container(do_QueryInterface(mDocument));
      nsCOMPtr<nsIHTMLCSSStyleSheet> inlineSheet;
      container->GetInlineStyleSheet(getter_AddRefs(inlineSheet));
      nsCOMPtr<nsIHTMLStyleSheet> attrSheet;
      container->GetAttributeStyleSheet(getter_AddRefs(attrSheet));
      nsCOMPtr<nsIStyleSheet> inlineCSS(do_QueryInterface(inlineSheet));
      nsCOMPtr<nsIStyleSheet> attrCSS(do_QueryInterface(attrSheet));
      NS_NewISupportsArray(getter_AddRefs(mRuleProcessors));
      nsCOMPtr<nsIStyleRuleProcessor> prevProcessor;
      for (PRInt32 i = 0; i < count; i++) {
        nsCOMPtr<nsIStyleSheet> sheet = getter_AddRefs(mDocument->GetStyleSheetAt(i));
        if (sheet == inlineCSS || sheet == attrCSS)
          continue;

        nsCOMPtr<nsIStyleRuleProcessor> processor;
        sheet->GetStyleRuleProcessor(*getter_AddRefs(processor), prevProcessor);
        if (processor != prevProcessor) {
          mRuleProcessors->AppendElement(processor);
          prevProcessor = processor;
        }
      }
    }
  }
  
  *aResult = mRuleProcessors;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXBLDocumentInfo::GetPrototypeHandler(const nsCString& aRef, nsIXBLPrototypeHandler** aResult)
{
  *aResult = nsnull;
  if (!mHandlerTable)
    return NS_OK;

  nsCStringKey key(aRef);
  *aResult = NS_STATIC_CAST(nsIXBLPrototypeHandler*, mHandlerTable->Get(&key)); // Addref happens here.

  return NS_OK;
}

NS_IMETHODIMP
nsXBLDocumentInfo::SetPrototypeHandler(const nsCString& aRef, nsIXBLPrototypeHandler* aHandler)
{
  if (!mHandlerTable)
    mHandlerTable = new nsSupportsHashtable();

  nsCStringKey key(aRef);
  mHandlerTable->Put(&key, aHandler);

  return NS_OK;
}

nsresult NS_NewXBLDocumentInfo(nsIDocument* aDocument, nsIXBLDocumentInfo** aResult)
{
  *aResult = new nsXBLDocumentInfo(aDocument);
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////

class nsBindingManager : public nsIBindingManager, public nsIStyleRuleSupplier
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

  NS_IMETHOD AddLayeredBinding(nsIContent* aContent, const nsAReadableString& aURL);
  NS_IMETHOD RemoveLayeredBinding(nsIContent* aContent, const nsAReadableString& aURL);
  NS_IMETHOD LoadBindingDocument(nsIDocument* aBoundDoc, const nsAReadableString& aURL,
                                 nsIDocument** aResult);

  NS_IMETHOD AddToAttachedQueue(nsIXBLBinding* aBinding);
  NS_IMETHOD AddHandlerToAttachedQueue(nsIXBLBindingAttachedHandler* aHandler);
  NS_IMETHOD ClearAttachedQueue();
  NS_IMETHOD ProcessAttachedQueue();

  NS_IMETHOD ExecuteDetachedHandlers();

  NS_IMETHOD PutXBLDocumentInfo(nsIXBLDocumentInfo* aDocumentInfo);
  NS_IMETHOD GetXBLDocumentInfo(const nsCString& aURL, nsIXBLDocumentInfo** aResult);

  NS_IMETHOD PutLoadingDocListener(const nsCString& aURL, nsIStreamListener* aListener);
  NS_IMETHOD GetLoadingDocListener(const nsCString& aURL, nsIStreamListener** aResult);
  NS_IMETHOD RemoveLoadingDocListener(const nsCString& aURL);

  NS_IMETHOD InheritsStyle(nsIContent* aContent, PRBool* aResult);
  NS_IMETHOD FlushChromeBindings();

  // nsIStyleRuleSupplier
  NS_IMETHOD UseDocumentRules(nsIContent* aContent, PRBool* aResult);
  NS_IMETHOD WalkRules(nsIStyleSet* aStyleSet, 
                       nsISupportsArrayEnumFunc aFunc, void* aData,
                       nsIContent* aContent);

protected:
  void GetEnclosingScope(nsIContent* aContent, nsIContent** aParent);
  void GetOutermostStyleScope(nsIContent* aContent, nsIContent** aParent);

  void WalkRules(nsISupportsArrayEnumFunc aFunc, void* aData,
                 nsIContent* aParent, nsIContent* aCurrContent);

// MEMBER VARIABLES
protected: 
  nsSupportsHashtable* mBindingTable;
  nsSupportsHashtable* mDocumentTable;
  nsSupportsHashtable* mLoadingDocTable;

  nsCOMPtr<nsISupportsArray> mAttachedQueue;
};

// Implementation /////////////////////////////////////////////////////////////////

// Static member variable initialization

// Implement our nsISupports methods
NS_IMPL_ISUPPORTS2(nsBindingManager, nsIBindingManager, nsIStyleRuleSupplier)

// Constructors/Destructors
nsBindingManager::nsBindingManager(void)
{
  NS_INIT_REFCNT();

  mBindingTable = nsnull;
  
  mDocumentTable = nsnull;
  mLoadingDocTable = nsnull;

  mAttachedQueue = nsnull;
}

nsBindingManager::~nsBindingManager(void)
{
  delete mBindingTable;
  delete mDocumentTable;
  delete mLoadingDocTable;
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

  nsCOMPtr<nsISupports> old = getter_AddRefs(mBindingTable->Get(&key));
  if (old && aBinding)
    NS_ERROR("Binding already installed!");

  if (aBinding) {
    mBindingTable->Put(&key, aBinding);
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
nsBindingManager::AddLayeredBinding(nsIContent* aContent, const nsAReadableString& aURL)
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
nsBindingManager::RemoveLayeredBinding(nsIContent* aContent, const nsAReadableString& aURL)
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
nsBindingManager::LoadBindingDocument(nsIDocument* aBoundDoc, const nsAReadableString& aURL,
                                      nsIDocument** aResult)
{
  // First we need to load our binding.
  *aResult = nsnull;
  nsresult rv;
  NS_WITH_SERVICE(nsIXBLService, xblService, "component://netscape/xbl", &rv);
  if (!xblService)
    return rv;

  // Load the binding doc.
  nsCString url; url.AssignWithConversion((const PRUnichar*)nsPromiseFlatString(aURL));
  nsCOMPtr<nsIXBLDocumentInfo> info;
  xblService->LoadBindingDocumentInfo(nsnull, aBoundDoc, url, nsCAutoString(), PR_TRUE, getter_AddRefs(info));
  if (!info)
    return NS_ERROR_FAILURE;

  info->GetDocument(aResult); // Addref happens here.
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
nsBindingManager::AddHandlerToAttachedQueue(nsIXBLBindingAttachedHandler* aBinding)
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
    nsCOMPtr<nsISupports> supp;
    mAttachedQueue->GetElementAt(0, getter_AddRefs(supp));
    mAttachedQueue->RemoveElementAt(0);

    nsCOMPtr<nsIXBLBinding> binding(do_QueryInterface(supp));
    if (binding)
      binding->ExecuteAttachedHandler();
    else {
      nsCOMPtr<nsIXBLBindingAttachedHandler> handler(do_QueryInterface(supp));
      if (handler)
        handler->OnBindingAttached();
    }
  }

  ClearAttachedQueue();
  return NS_OK;
}

PRBool PR_CALLBACK ExecuteDetachedHandler(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsIXBLBinding* binding = (nsIXBLBinding*)aData;
  binding->ExecuteDetachedHandler();
  return PR_TRUE;
}


NS_IMETHODIMP
nsBindingManager::ExecuteDetachedHandlers()
{
  // Walk our hashtable of bindings.
  if (mBindingTable)
    mBindingTable->Enumerate(ExecuteDetachedHandler);
  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::PutXBLDocumentInfo(nsIXBLDocumentInfo* aDocumentInfo)
{
  if (!mDocumentTable)
    mDocumentTable = new nsSupportsHashtable();

  nsCOMPtr<nsIDocument> doc;
  aDocumentInfo->GetDocument(getter_AddRefs(doc));

  nsCOMPtr<nsIURI> uri(getter_AddRefs(doc->GetDocumentURL()));
  nsXPIDLCString str;
  uri->GetSpec(getter_Copies(str));
  
  nsCStringKey key((const char*)str);
  mDocumentTable->Put(&key, aDocumentInfo);
  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::GetXBLDocumentInfo(const nsCString& aURL, nsIXBLDocumentInfo** aResult)
{
  *aResult = nsnull;
  if (!mDocumentTable)
    return NS_OK;

  nsCStringKey key(aURL);
  *aResult = NS_STATIC_CAST(nsIXBLDocumentInfo*, mDocumentTable->Get(&key)); // Addref happens here.

  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::PutLoadingDocListener(const nsCString& aURL, nsIStreamListener* aListener)
{
  if (!mLoadingDocTable)
    mLoadingDocTable = new nsSupportsHashtable();

  nsCStringKey key(aURL);
  mLoadingDocTable->Put(&key, aListener);

  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::GetLoadingDocListener(const nsCString& aURL, nsIStreamListener** aResult)
{
  *aResult = nsnull;
  if (!mLoadingDocTable)
    return NS_OK;

  nsCStringKey key(aURL);
  *aResult = NS_STATIC_CAST(nsIStreamListener*, mLoadingDocTable->Get(&key)); // Addref happens here.
  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::RemoveLoadingDocListener(const nsCString& aURL)
{
  if (!mLoadingDocTable)
    return NS_OK;

  nsCStringKey key(aURL);
  mLoadingDocTable->Remove(&key);

  return NS_OK;
}

PRBool PR_CALLBACK MarkForDeath(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsIXBLBinding* binding = (nsIXBLBinding*)aData;
  nsCAutoString docURI;
  binding->GetDocURI(docURI);
  if (!docURI.CompareWithConversion("chrome", PR_FALSE, 6))
    binding->MarkForDeath();
  return PR_TRUE;
}

NS_IMETHODIMP
nsBindingManager::FlushChromeBindings()
{
  mBindingTable->Enumerate(MarkForDeath);
  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::InheritsStyle(nsIContent* aContent, PRBool* aResult)
{
  // Get our enclosing parent.
  *aResult = PR_TRUE;
  nsCOMPtr<nsIContent> parent;
  GetEnclosingScope(aContent, getter_AddRefs(parent));
  if (parent) {
    // See if the parent is our parent.
    nsCOMPtr<nsIContent> ourParent;
    aContent->GetParent(*getter_AddRefs(ourParent));
    if (ourParent == parent) {
      // Yes. Check the binding and see if it wants to allow us
      // to inherit styles.
      nsCOMPtr<nsIXBLBinding> binding;
      GetBinding(parent, getter_AddRefs(binding));
      if (binding)
        binding->InheritsStyle(aResult);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsBindingManager::UseDocumentRules(nsIContent* aContent, PRBool* aResult)
{
  if (!aContent)
    return NS_OK;

  nsCOMPtr<nsIContent> parent;
  GetOutermostStyleScope(aContent, getter_AddRefs(parent));
  *aResult = !parent;
  return NS_OK;
}

void
nsBindingManager::GetEnclosingScope(nsIContent* aContent,
                                    nsIContent** aParent)
{
  // Look up the enclosing parent.
  aContent->GetBindingParent(aParent);
}

void
nsBindingManager::GetOutermostStyleScope(nsIContent* aContent,
                                         nsIContent** aParent)
{
  nsCOMPtr<nsIContent> parent;
  GetEnclosingScope(aContent, getter_AddRefs(parent));
  while (parent) {
    PRBool inheritsStyle = PR_TRUE;
    nsCOMPtr<nsIXBLBinding> binding;
    GetBinding(parent, getter_AddRefs(binding));
    if (binding) {
      binding->InheritsStyle(&inheritsStyle);
    }
    if (!inheritsStyle)
      break;
    nsCOMPtr<nsIContent> child = parent;
    GetEnclosingScope(child, getter_AddRefs(parent));
    if (parent == child)
      break; // The scrollbar case only is deliberately hacked to return itself
             // (see GetBindingParent in nsXULElement.cpp).
  }
  *aParent = parent;
  NS_IF_ADDREF(*aParent);
}

void
nsBindingManager::WalkRules(nsISupportsArrayEnumFunc aFunc, void* aData,
                            nsIContent* aParent, nsIContent* aCurrContent)
{
  nsCOMPtr<nsIXBLBinding> binding;
  GetBinding(aCurrContent, getter_AddRefs(binding));
  if (binding) {
    binding->WalkRules(aFunc, aData);
  }
  if (aParent != aCurrContent) {
    nsCOMPtr<nsIContent> par;
    GetEnclosingScope(aCurrContent, getter_AddRefs(par));
    if (par)
      WalkRules(aFunc, aData, aParent, par);
  }
}

NS_IMETHODIMP
nsBindingManager::WalkRules(nsIStyleSet* aStyleSet,
                            nsISupportsArrayEnumFunc aFunc, void* aData,
                            nsIContent* aContent)
{
  if (!aContent)
    return NS_OK;

  nsCOMPtr<nsIContent> parent;
  GetOutermostStyleScope(aContent, getter_AddRefs(parent));

  WalkRules(aFunc, aData, parent, aContent);

  if (parent) {
    // We cut ourselves off, but we still need to walk the document's attribute sheet
    // so that inline style continues to work on anonymous content.
    nsCOMPtr<nsIDocument> document;
    aContent->GetDocument(*getter_AddRefs(document));
    nsCOMPtr<nsIHTMLContentContainer> container(do_QueryInterface(document));
    nsCOMPtr<nsIHTMLCSSStyleSheet> inlineSheet;
    container->GetInlineStyleSheet(getter_AddRefs(inlineSheet));  
    nsCOMPtr<nsIStyleRuleProcessor> inlineCSS(do_QueryInterface(inlineSheet));
    (*aFunc)((nsISupports*)(inlineCSS.get()), aData);
  }
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


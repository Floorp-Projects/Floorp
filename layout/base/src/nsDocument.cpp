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
 *   L. David Baron <dbaron@fas.harvard.edu>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
#include "plstr.h"

#include "nsCOMPtr.h"
#include "nsIInterfaceRequestor.h"
#include "nsDocument.h"
#include "nsIArena.h"
#include "nsIURL.h"
#include "nsILoadGroup.h"
#include "nsIChannel.h"
#include "nsString.h"
#include "nsIContent.h"
#include "nsIDocumentObserver.h"
#include "nsIStyleSet.h"
#include "nsIStyleSheet.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIDocumentObserver.h"
#include "nsIEventListenerManager.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptEventListener.h"
#include "nsIDOMEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIEventStateManager.h"
#include "nsContentList.h"
#include "nsIObserver.h"

#include "nsIDOMEventListener.h"
#include "nsIDOMFormListener.h"
#include "nsIDOMPaintListener.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseMotionListener.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMFocusListener.h"
#include "nsIDOMLoadListener.h"

#include "nsIDOMStyleSheet.h"
#include "nsIDOMStyleSheetList.h"
#include "nsDOMAttribute.h"
#include "nsDOMCID.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsIDOMDOMImplementation.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMAbstractView.h"
#include "nsIDOMDocumentXBL.h"
#include "nsIDOMNavigator.h"
#include "nsGenericElement.h"

#include "nsICSSStyleSheet.h"

#include "nsITextContent.h"
#include "nsIDocumentEncoder.h"
#include "nsIHTMLContentSink.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsIFileStreams.h"

#include "nsRange.h"
#include "nsIDOMText.h"
#include "nsIDOMComment.h"
#include "nsDOMDocumentType.h"

#include "nsINameSpaceManager.h"
#include "nsIServiceManager.h"

#include "nsLayoutAtoms.h"
#include "nsLayoutCID.h"
#include "nsIDOMRange.h"
#include "nsIEnumerator.h"
#include "nsDOMError.h"
#include "nsIScrollableView.h"
#include "nsIFrame.h"
#include "nsLayoutUtils.h"
#include "nsNodeInfoManager.h"
#include "nsIXBLService.h"

#include "nsNetUtil.h"     // for NS_MakeAbsoluteURI

#include "nsIScriptSecurityManager.h"
#include "nsIAggregatePrincipal.h"
#include "nsIPrivateDOMImplementation.h"

#include "nsIInterfaceRequestor.h"
#include "nsIDOMWindowInternal.h"

#include "nsIDOMElement.h"

#include "nsIBoxObject.h"
#include "nsPIBoxObject.h"
#include "nsXULAtoms.h"

static NS_DEFINE_CID(kDOMScriptObjectFactoryCID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);

static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);

#include "nsILineBreakerFactory.h"
#include "nsIWordBreakerFactory.h"
#include "nsLWBrkCIID.h"

#include "nsIHTMLDocument.h"

class nsDOMStyleSheetList : public nsIDOMStyleSheetList,
                            public nsIScriptObjectOwner,
                            public nsIDocumentObserver
{
public:
  nsDOMStyleSheetList(nsIDocument *aDocument);
  virtual ~nsDOMStyleSheetList();

  NS_DECL_ISUPPORTS
  NS_DECL_IDOMSTYLESHEETLIST
  
  NS_IMETHOD BeginUpdate(nsIDocument *aDocument) { return NS_OK; }
  NS_IMETHOD EndUpdate(nsIDocument *aDocument) { return NS_OK; }
  NS_IMETHOD BeginLoad(nsIDocument *aDocument) { return NS_OK; }
  NS_IMETHOD EndLoad(nsIDocument *aDocument) { return NS_OK; }
  NS_IMETHOD BeginReflow(nsIDocument *aDocument,
			                   nsIPresShell* aShell) { return NS_OK; }
  NS_IMETHOD EndReflow(nsIDocument *aDocument,
		                   nsIPresShell* aShell) { return NS_OK; } 
  NS_IMETHOD ContentChanged(nsIDocument *aDocument,
			                      nsIContent* aContent,
                            nsISupports* aSubContent) { return NS_OK; }
  NS_IMETHOD ContentStatesChanged(nsIDocument* aDocument,
                                  nsIContent* aContent1,
                                  nsIContent* aContent2) { return NS_OK; }
  NS_IMETHOD AttributeChanged(nsIDocument *aDocument,
                              nsIContent*  aContent,
                              PRInt32      aNameSpaceID,
                              nsIAtom*     aAttribute,
                              PRInt32      aHint) { return NS_OK; }
  NS_IMETHOD ContentAppended(nsIDocument *aDocument,
			                       nsIContent* aContainer,
                             PRInt32     aNewIndexInContainer) 
                             { return NS_OK; }
  NS_IMETHOD ContentInserted(nsIDocument *aDocument,
			                       nsIContent* aContainer,
                             nsIContent* aChild,
                             PRInt32 aIndexInContainer) { return NS_OK; }
  NS_IMETHOD ContentReplaced(nsIDocument *aDocument,
			                       nsIContent* aContainer,
                             nsIContent* aOldChild,
                             nsIContent* aNewChild,
                             PRInt32 aIndexInContainer) { return NS_OK; }
  NS_IMETHOD ContentRemoved(nsIDocument *aDocument,
                            nsIContent* aContainer,
                            nsIContent* aChild,
                            PRInt32 aIndexInContainer) { return NS_OK; }
  NS_IMETHOD StyleSheetAdded(nsIDocument *aDocument,
                             nsIStyleSheet* aStyleSheet);
  NS_IMETHOD StyleSheetRemoved(nsIDocument *aDocument,
                               nsIStyleSheet* aStyleSheet);
  NS_IMETHOD StyleSheetDisabledStateChanged(nsIDocument *aDocument,
                                        nsIStyleSheet* aStyleSheet,
                                        PRBool aDisabled) { return NS_OK; }
  NS_IMETHOD StyleRuleChanged(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule,
                              PRInt32 aHint) { return NS_OK; }
  NS_IMETHOD StyleRuleAdded(nsIDocument *aDocument,
                            nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule) { return NS_OK; }
  NS_IMETHOD StyleRuleRemoved(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule) { return NS_OK; }
  NS_IMETHOD DocumentWillBeDestroyed(nsIDocument *aDocument);

  // nsIScriptObjectOwner interface
  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void* aScriptObject);

protected:
  PRInt32       mLength;
  nsIDocument*  mDocument;
  void*         mScriptObject;
};

nsDOMStyleSheetList::nsDOMStyleSheetList(nsIDocument *aDocument)
{
  NS_INIT_REFCNT();
  mLength = -1;
  // Not reference counted to avoid circular references.
  // The document will tell us when its going away.
  mDocument = aDocument;
  mDocument->AddObserver(this);
  mScriptObject = nsnull;
}

nsDOMStyleSheetList::~nsDOMStyleSheetList()
{
  if (nsnull != mDocument) {
    mDocument->RemoveObserver(this);
  }
  mDocument = nsnull;
}

NS_IMPL_ADDREF(nsDOMStyleSheetList)
NS_IMPL_RELEASE(nsDOMStyleSheetList)

NS_INTERFACE_MAP_BEGIN(nsDOMStyleSheetList)
   NS_INTERFACE_MAP_ENTRY(nsIDOMStyleSheetList)
   NS_INTERFACE_MAP_ENTRY(nsIScriptObjectOwner)
   NS_INTERFACE_MAP_ENTRY(nsIDocumentObserver)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMStyleSheetList)
NS_INTERFACE_MAP_END

NS_IMETHODIMP    
nsDOMStyleSheetList::GetLength(PRUint32* aLength)
{
  if (nsnull != mDocument) {
    // XXX Find the number and then cache it. We'll use the 
    // observer notification to figure out if new ones have
    // been added or removed.
    if (-1 == mLength) {
      PRUint32 count = 0;
      PRInt32 i, imax = mDocument->GetNumberOfStyleSheets();
      
      for (i = 0; i < imax; i++) {
        nsCOMPtr<nsIStyleSheet> sheet(dont_AddRef(mDocument->GetStyleSheetAt(i)));
        if (!sheet)
          continue;
        nsCOMPtr<nsIDOMStyleSheet> domss(do_QueryInterface(sheet));

        if (domss) {
          count++;
        }
      }
      mLength = count;
    }
    *aLength = mLength;
  }
  else {
    *aLength = 0;
  }
  
  return NS_OK;
}

NS_IMETHODIMP    
nsDOMStyleSheetList::Item(PRUint32 aIndex, nsIDOMStyleSheet** aReturn)
{
  *aReturn = nsnull;
  if (nsnull != mDocument) {
    PRUint32 count = 0;
    PRInt32 i, imax = mDocument->GetNumberOfStyleSheets();
  
    // XXX Not particularly efficient, but does anyone care?
    for (i = 0; (i < imax) && (nsnull == *aReturn); i++) {
      nsCOMPtr<nsIStyleSheet> sheet(dont_AddRef(mDocument->GetStyleSheetAt(i)));
      if (!sheet)
        continue;
      nsCOMPtr<nsIDOMStyleSheet> domss(do_QueryInterface(sheet));

      if (domss) {
        if (count++ == aIndex) {
          *aReturn = domss;
          NS_IF_ADDREF(*aReturn);
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsDOMStyleSheetList::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;

  if (nsnull == mScriptObject) {
    nsISupports *supports = (nsISupports *)(nsIDOMStyleSheetList *)this;
    nsISupports *parent = (nsISupports *)mDocument;

    // XXX Should be done through factory
    res = NS_NewScriptStyleSheetList(aContext, 
                                     supports,
                                     parent,
                                     (void**)&mScriptObject);
  }
  *aScriptObject = mScriptObject;

  return res;
}

NS_IMETHODIMP 
nsDOMStyleSheetList::SetScriptObject(void* aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

NS_IMETHODIMP 
nsDOMStyleSheetList::StyleSheetAdded(nsIDocument *aDocument,
                                     nsIStyleSheet* aStyleSheet)
{
  if (-1 != mLength) {
    nsCOMPtr<nsIDOMStyleSheet> domss(do_QueryInterface(aStyleSheet));
    if (domss) {
      mLength++;
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP 
nsDOMStyleSheetList::StyleSheetRemoved(nsIDocument *aDocument,
                                       nsIStyleSheet* aStyleSheet)
{
  if (-1 != mLength) {
    nsCOMPtr<nsIDOMStyleSheet> domss(do_QueryInterface(aStyleSheet));
    if (domss) {
      mLength--;
    }
  }
  return NS_OK;  
}

NS_IMETHODIMP
nsDOMStyleSheetList::DocumentWillBeDestroyed(nsIDocument *aDocument)
{
  if (nsnull != mDocument) {
    aDocument->RemoveObserver(this);
    mDocument = nsnull;
  }
  
  return NS_OK;
}

// ==================================================================
// =
// ==================================================================

class nsDOMImplementation : public nsIDOMDOMImplementation,
                            public nsIScriptObjectOwner,
                            public nsIPrivateDOMImplementation
{
public:
  nsDOMImplementation(nsIURI* aBaseURI = nsnull);
  virtual ~nsDOMImplementation();

  NS_DECL_ISUPPORTS
  
  // nsIDOMDOMImplementation
  NS_IMETHOD    HasFeature(const nsAReadableString& aFeature, 
                           const nsAReadableString& aVersion, 
                           PRBool* aReturn);
  NS_IMETHOD    CreateDocumentType(const nsAReadableString& aQualifiedName,
                                   const nsAReadableString& aPublicId,
                                   const nsAReadableString& aSystemId,
                                   nsIDOMDocumentType** aReturn);

  NS_IMETHOD    CreateDocument(const nsAReadableString& aNamespaceURI,
                               const nsAReadableString& aQualifiedName,
                               nsIDOMDocumentType* aDoctype,
                               nsIDOMDocument** aReturn);

  // nsIScriptObjectOwner
  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void *aScriptObject);

  //nsIPrivateDOMImplementation
  NS_IMETHOD Init(nsIURI* aBaseURI);

protected:
  void *mScriptObject;
  nsCOMPtr<nsIURI> mBaseURI;
};


NS_LAYOUT nsresult
NS_NewDOMImplementation(nsIDOMDOMImplementation** aInstancePtrResult)
{
  nsDOMImplementation* domImpl = new nsDOMImplementation();
  if (domImpl == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  return domImpl->QueryInterface(NS_GET_IID(nsIDOMDOMImplementation), (void**) aInstancePtrResult);
}

nsDOMImplementation::nsDOMImplementation(nsIURI* aBaseURI)
{
  NS_INIT_REFCNT();
  mScriptObject = nsnull;
  mBaseURI = aBaseURI;
}

nsDOMImplementation::~nsDOMImplementation()
{
}

NS_IMPL_ISUPPORTS4(nsDOMImplementation, nsIDOMDOMImplementation, nsIPrivateDOMImplementation, nsIScriptObjectOwner, nsIDOMDOMImplementation)


NS_IMETHODIMP    
nsDOMImplementation::HasFeature(const nsAReadableString& aFeature, 
                                const nsAReadableString& aVersion, 
                                PRBool* aReturn)
{
  return nsGenericElement::InternalIsSupported(aFeature, aVersion, aReturn);
}

NS_IMETHODIMP
nsDOMImplementation::CreateDocumentType(const nsAReadableString& aQualifiedName,
                                        const nsAReadableString& aPublicId, 
                                        const nsAReadableString& aSystemId, 
                                        nsIDOMDocumentType** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  return NS_NewDOMDocumentType(aReturn, aQualifiedName, nsnull, nsnull,
                               aPublicId, aSystemId, nsString());
}

NS_IMETHODIMP
nsDOMImplementation::CreateDocument(const nsAReadableString& aNamespaceURI, 
                                    const nsAReadableString& aQualifiedName, 
                                    nsIDOMDocumentType* aDoctype, 
                                    nsIDOMDocument** aReturn)
{  
  NS_ENSURE_ARG_POINTER(aReturn);    
  
  *aReturn = nsnull;

  return NS_NewDOMDocument(aReturn, aNamespaceURI, aQualifiedName, aDoctype,
                           mBaseURI);
}

NS_IMETHODIMP 
nsDOMImplementation::GetScriptObject(nsIScriptContext *aContext, 
                                     void** aScriptObject)
{
  nsresult result = NS_OK;

  if (!mScriptObject) {
    nsCOMPtr<nsIDOMScriptObjectFactory> factory(do_GetService(kDOMScriptObjectFactoryCID, &result));

    if (NS_OK == result) {
      nsCOMPtr<nsIScriptGlobalObject> global(dont_AddRef(aContext->GetGlobalObject()));

      result = factory->NewScriptDOMImplementation(aContext, (nsISupports*)(nsIDOMDOMImplementation*)this,
                                                   global, &mScriptObject);
    }
  }
  
  *aScriptObject = mScriptObject;
  return result;
}

NS_IMETHODIMP 
nsDOMImplementation::SetScriptObject(void *aScriptObject)
{
  mScriptObject = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMImplementation::Init(nsIURI* aBaseURI)
{
  mBaseURI = aBaseURI;
  return NS_OK;
}

// ==================================================================
// =
// ==================================================================

MOZ_DECL_CTOR_COUNTER(nsDocumentChildNodes)

nsDocumentChildNodes::nsDocumentChildNodes(nsIDocument* aDocument)
{
  MOZ_COUNT_CTOR(nsDocumentChildNodes);

  // We don't reference count our document reference (to avoid circular
  // references). We'll be told when the document goes away.
  mDocument = aDocument;
}
 
nsDocumentChildNodes::~nsDocumentChildNodes()
{
  MOZ_COUNT_DTOR(nsDocumentChildNodes);
}

NS_IMETHODIMP
nsDocumentChildNodes::GetLength(PRUint32* aLength)
{
  if (nsnull != mDocument) {
    PRInt32 count;
    mDocument->GetChildCount(count);
    *aLength = (PRUint32)count;
  }
  else {
    *aLength = 0;
  }
  
  return NS_OK;
}

NS_IMETHODIMP    
nsDocumentChildNodes::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  nsresult result = NS_OK;
  nsCOMPtr<nsIContent> content;

  *aReturn = nsnull;
  if (nsnull != mDocument) {
    result = mDocument->ChildAt(aIndex, *getter_AddRefs(content));
    if ((NS_OK == result) && (nsnull != content)) {
      result = content->QueryInterface(NS_GET_IID(nsIDOMNode), (void**)aReturn);
    }
  }

  return result;
}

void 
nsDocumentChildNodes::DropReference()
{
  mDocument = nsnull;
}

// ==================================================================
// =
// ==================================================================

nsDocument::nsDocument()
{
  NS_INIT_REFCNT();

  mArena = nsnull;
  mDocumentTitle = nsnull;
  mDocumentURL = nsnull;
  mCharacterSet.AssignWithConversion("ISO-8859-1");
  mParentDocument = nsnull;
  mRootContent = nsnull;
  mScriptObject = nsnull;
  mListenerManager = nsnull;
  mInDestructor = PR_FALSE;
  mDOMStyleSheets = nsnull;
  mNameSpaceManager = nsnull;
  mHeaderData = nsnull;
  mChildNodes = nsnull;
  mModCount = 0;
  mPrincipal = nsnull;
  mNextContentID = NS_CONTENT_ID_COUNTER_BASE;
  mDTD = 0;
  mBoxObjectTable = nsnull;
  Init();/* XXX */
}

nsDocument::~nsDocument()
{
  // XXX Inform any remaining observers that we are going away.
  // Note that this currently contradicts the rule that all
  // observers must hold on to live references to the document.
  // This notification will occur only after the reference has
  // been dropped.
  mInDestructor = PR_TRUE;
  PRInt32 index;
  for (index = 0; index < mObservers.Count(); index++) {
    nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers.ElementAt(index);
    observer->DocumentWillBeDestroyed(this);
    if (observer != (nsIDocumentObserver*)mObservers.ElementAt(index)) {
      index--;
    }
  }

  if (nsnull != mDocumentTitle) {
    delete mDocumentTitle;
    mDocumentTitle = nsnull;
  }
  NS_IF_RELEASE(mDocumentURL);
  NS_IF_RELEASE(mPrincipal);
  mDocumentLoadGroup = null_nsCOMPtr();

  mParentDocument = nsnull;

  // Delete references to sub-documents
  index = mSubDocuments.Count();
  while (--index >= 0) {
    nsIDocument* subdoc = (nsIDocument*) mSubDocuments.ElementAt(index);
    NS_RELEASE(subdoc);
  }


  mRootContent = nsnull;
  mChildren->Clear();

  // Delete references to style sheets
  index = mStyleSheets.Count();
  while (--index >= 0) {
    nsIStyleSheet* sheet = (nsIStyleSheet*) mStyleSheets.ElementAt(index);
    sheet->SetOwningDocument(nsnull);
    NS_RELEASE(sheet);
  }

  if (nsnull != mChildNodes) {
    mChildNodes->DropReference();
    NS_RELEASE(mChildNodes);
  }

  NS_IF_RELEASE(mArena);
  NS_IF_RELEASE(mListenerManager);
  NS_IF_RELEASE(mDOMStyleSheets);
  NS_IF_RELEASE(mNameSpaceManager);
  if (nsnull != mHeaderData) {
    delete mHeaderData;
    mHeaderData = nsnull;
  }

  NS_IF_RELEASE(mDTD);
  	
  delete mBoxObjectTable;
}

NS_INTERFACE_MAP_BEGIN(nsDocument)
  NS_INTERFACE_MAP_ENTRY(nsIDocument)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDocument)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSDocument)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDocumentEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDocumentStyle)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDocumentView)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDocumentXBL)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectOwner)
  NS_INTERFACE_MAP_ENTRY(nsIJSScriptObject)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventReceiver)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDiskDocument)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDocument)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsDocument)
NS_IMPL_RELEASE(nsDocument)

nsresult nsDocument::Init()
{
  if (mNameSpaceManager) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  nsresult rv;

  rv = NS_NewISupportsArray(getter_AddRefs(mChildren));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewHeapArena(&mArena, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewNameSpaceManager(&mNameSpaceManager);
  NS_ENSURE_SUCCESS(rv, rv);

  mNodeInfoManager = new nsNodeInfoManager();
  NS_ENSURE_TRUE(mNodeInfoManager, NS_ERROR_OUT_OF_MEMORY);

  mNodeInfoManager->Init(mNameSpaceManager);

  return rv;
}

nsIArena* nsDocument::GetArena()
{
  if (nsnull != mArena) {
    NS_ADDREF(mArena);
  }
  return mArena;
}

nsresult
nsDocument::Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup)
{
  nsresult rv = NS_OK;

  if (nsnull != mDocumentTitle) {
    delete mDocumentTitle;
    mDocumentTitle = nsnull;
  }
  NS_IF_RELEASE(mDocumentURL);
  NS_IF_RELEASE(mPrincipal);
  mDocumentLoadGroup = null_nsCOMPtr();

  // Delete references to sub-documents
  PRInt32 index = mSubDocuments.Count();
  while (--index >= 0) {
    nsIDocument* subdoc = (nsIDocument*) mSubDocuments.ElementAt(index);
    NS_RELEASE(subdoc);
  }

  mRootContent = nsnull;
  PRUint32 count, i;
  mChildren->Count(&count);
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsIContent> content(dont_AddRef(NS_STATIC_CAST(nsIContent*,mChildren->ElementAt(i))));
    content->SetDocument(nsnull, PR_TRUE, PR_TRUE);
    ContentRemoved(nsnull, content, index);
  }
  mChildren->Clear();

  // Delete references to style sheets
  index = mStyleSheets.Count();
  while (--index >= 0) {
    nsIStyleSheet* sheet = (nsIStyleSheet*) mStyleSheets.ElementAt(index);
    sheet->SetOwningDocument(nsnull);

    PRInt32 pscount = mPresShells.Count();
    PRInt32 psindex;
    for (psindex = 0; psindex < pscount; psindex++) {
      nsIPresShell* shell = (nsIPresShell*)mPresShells.ElementAt(psindex);
      nsCOMPtr<nsIStyleSet> set;
      if (NS_SUCCEEDED(shell->GetStyleSet(getter_AddRefs(set)))) {
        if (set) {
          set->RemoveDocStyleSheet(sheet);
        }
      }
    }

    // XXX Tell observers?

    NS_RELEASE(sheet);
  }
  mStyleSheets.Clear();

  NS_IF_RELEASE(mListenerManager);
  NS_IF_RELEASE(mDOMStyleSheets);

  NS_IF_RELEASE(mNameSpaceManager);

  if (aChannel) {

      nsCOMPtr<nsIURI> uri;
      (void) aChannel->GetOriginalURI(getter_AddRefs(uri));

      PRBool isChrome = PR_FALSE;
      PRBool isRes = PR_FALSE;
      (void)uri->SchemeIs(nsIURI::CHROME, &isChrome);
      (void)uri->SchemeIs(nsIURI::RESOURCE, &isRes);

      if (isChrome || isRes)
            (void)aChannel->GetOriginalURI(&mDocumentURL);
      else
            (void)aChannel->GetURI(&mDocumentURL);

    nsCOMPtr<nsISupports> owner;
    aChannel->GetOwner(getter_AddRefs(owner));
    if (owner)
      owner->QueryInterface(NS_GET_IID(nsIPrincipal), (void**)&mPrincipal);
  }

  if (aLoadGroup) {
    mDocumentLoadGroup = getter_AddRefs(NS_GetWeakReference(aLoadGroup));
    // there was an assertion here that aLoadGroup was not null.  This is no longer valid
    // nsWebShell::SetDocument does not create a load group, and it works just fine.
  }

  if (NS_OK == rv) {
    rv = NS_NewNameSpaceManager(&mNameSpaceManager);
  }

  return rv;
}

nsresult 
nsDocument::SetDocumentURL(nsIURI* aURI)
{
  NS_IF_RELEASE(mDocumentURL);
  mDocumentURL = aURI;
  NS_IF_ADDREF(mDocumentURL);

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::StartDocumentLoad(const char* aCommand,
                              nsIChannel* aChannel,
                              nsILoadGroup* aLoadGroup,
                              nsISupports* aContainer,
                              nsIStreamListener **aDocListener,
                              PRBool aReset)
{
  nsresult rv = NS_OK;
  if (aReset)
    rv = Reset(aChannel, aLoadGroup);
  return rv;
}

NS_IMETHODIMP 
nsDocument::StopDocumentLoad()
{
  return NS_OK;
}

const nsString* nsDocument::GetDocumentTitle() const
{
  return mDocumentTitle;
}

nsIURI* nsDocument::GetDocumentURL() const
{
  nsIURI* url = mDocumentURL;
  NS_IF_ADDREF(url);
  return url;
}

NS_IMETHODIMP
nsDocument::GetPrincipal(nsIPrincipal **aPrincipal)
{
  if (!mPrincipal) {
    nsresult rv;
    NS_WITH_SERVICE(nsIScriptSecurityManager, securityManager, 
                    NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) 
        return rv;
    if (NS_FAILED(rv = securityManager->GetCodebasePrincipal(mDocumentURL, 
                                                             &mPrincipal)))
        return rv;
  }

  if(aPrincipal)
  {
    *aPrincipal = mPrincipal;
    NS_ADDREF(*aPrincipal);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::AddPrincipal(nsIPrincipal *aNewPrincipal)
{
  nsresult rv;
  if (!mPrincipal)
    GetPrincipal(nsnull);

  nsCOMPtr<nsIAggregatePrincipal> agg(do_QueryInterface(mPrincipal, &rv));
  if (NS_SUCCEEDED(rv)) {
    rv = agg->Intersect(aNewPrincipal);
    if (NS_FAILED(rv))
      return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsDocument::GetContentType(nsAWritableString& aContentType) const
{
  // Must be implemented by derived class.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocument::GetDocumentLoadGroup(nsILoadGroup **aGroup) const
{
  nsCOMPtr<nsILoadGroup> group(do_QueryReferent(mDocumentLoadGroup));

  *aGroup = group;
  NS_IF_ADDREF(*aGroup);
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetBaseURL(nsIURI*& aURL) const
{
  aURL = mDocumentURL;
  NS_IF_ADDREF(aURL);
  return NS_OK;
}

NS_IMETHODIMP nsDocument::GetDocumentCharacterSet(nsAWritableString& oCharSetID) 
{
  oCharSetID = mCharacterSet;
  return NS_OK;
}

NS_IMETHODIMP nsDocument::SetDocumentCharacterSet(const nsAReadableString& aCharSetID)
{
  if (!mCharacterSet.Equals(aCharSetID)) {
    mCharacterSet = aCharSetID;
    PRInt32 n = mCharSetObservers.Count();
    for (PRInt32 i = 0; i < n; i++) {
      nsIObserver* observer = (nsIObserver*) mCharSetObservers.ElementAt(i);
      observer->Observe((nsIDocument*) this, NS_LITERAL_STRING("charset").get(),
                        nsPromiseFlatString(aCharSetID).get());
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsDocument::AddCharSetObserver(nsIObserver* aObserver)
{
  NS_ENSURE_ARG_POINTER(aObserver);
  NS_ENSURE_TRUE(mCharSetObservers.AppendElement(aObserver), NS_ERROR_FAILURE);
  return NS_OK;
}

NS_IMETHODIMP nsDocument::RemoveCharSetObserver(nsIObserver* aObserver)
{
  NS_ENSURE_ARG_POINTER(aObserver);
  NS_ENSURE_TRUE(mCharSetObservers.RemoveElement(aObserver), NS_ERROR_FAILURE);
  return NS_OK;
}

NS_IMETHODIMP nsDocument::GetLineBreaker(nsILineBreaker** aResult) 
{
  if (!mLineBreaker) {
    // no line breaker, find a default one
    nsresult result;
    nsCOMPtr<nsILineBreakerFactory> lbf(do_GetService(NS_LWBRK_CONTRACTID, &result));

    if (NS_SUCCEEDED(result)) {
      nsAutoString lbarg;
      lbf->GetBreaker(lbarg, getter_AddRefs(mLineBreaker));
    }
  }
  *aResult = mLineBreaker;
  NS_IF_ADDREF(*aResult);
  return NS_OK; // XXX we should do error handling here
}

NS_IMETHODIMP nsDocument::SetLineBreaker(nsILineBreaker* aLineBreaker) 
{
  mLineBreaker = aLineBreaker;
  return NS_OK;
}

NS_IMETHODIMP nsDocument::GetWordBreaker(nsIWordBreaker** aResult) 
{
  if (!mWordBreaker) {
    // no word breaker, find a default one
    nsresult result;
    nsCOMPtr<nsIWordBreakerFactory> wbf(do_GetService(NS_LWBRK_CONTRACTID, &result));

    if (NS_SUCCEEDED(result)) {
      nsAutoString wbarg;
      wbf->GetBreaker(wbarg, getter_AddRefs(mWordBreaker));
    }
  }
  *aResult = mWordBreaker;
  NS_IF_ADDREF(*aResult);
  return NS_OK; // XXX we should do error handling here
}

NS_IMETHODIMP nsDocument::SetWordBreaker(nsIWordBreaker* aWordBreaker) 
{
  mWordBreaker = aWordBreaker;
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetHeaderData(nsIAtom* aHeaderField, nsAWritableString& aData) const
{
  aData.Truncate();
  const nsDocHeaderData* data = mHeaderData;
  while (nsnull != data) {
    if (data->mField == aHeaderField) {
      aData = data->mData;
      break;
    }
    data = data->mNext;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::SetHeaderData(nsIAtom* aHeaderField, const nsAReadableString& aData)
{
  if (nsnull != aHeaderField) {
    if (nsnull == mHeaderData) {
      if (0 < aData.Length()) { // don't bother storing empty string
        mHeaderData = new nsDocHeaderData(aHeaderField, aData);
      }
    }
    else {
      nsDocHeaderData* data = mHeaderData;
      nsDocHeaderData** lastPtr = &mHeaderData;
      do {  // look for existing and replace
        if (data->mField == aHeaderField) {
          if (0 < aData.Length()) {
            data->mData.Assign(aData);
          }
          else {  // don't store empty string
            (*lastPtr)->mNext = data->mNext;
            data->mNext = nsnull;
            delete data;
          }
          return NS_OK;
        }
        lastPtr = &(data->mNext);
        data = data->mNext;
      } while (nsnull != data);
      // didn't find, append
      if (0 < aData.Length()) {
        *lastPtr = new nsDocHeaderData(aHeaderField, aData);
      }
    }
    return NS_OK;
  }
  return NS_ERROR_NULL_POINTER;
}

#if 0
// XXX Temp hack: moved to nsMarkupDocument
NS_IMETHODIMP
nsDocument::CreateShell(nsIPresContext* aContext,
                        nsIViewManager* aViewManager,
                        nsIStyleSet* aStyleSet,
                        nsIPresShell** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIPresShell* shell;
  nsresult rv = NS_NewPresShell(&shell);
  if (NS_OK != rv) {
    return rv;
  }

  if (NS_OK != shell->Init(this, aContext, aViewManager, aStyleSet)) {
    NS_RELEASE(shell);
    return rv;
  }

  // Note: we don't hold a ref to the shell (it holds a ref to us)
  mPresShells.AppendElement(shell);
  *aInstancePtrResult = shell;
  return NS_OK;
}
#endif

PRBool nsDocument::DeleteShell(nsIPresShell* aShell)
{
  return mPresShells.RemoveElement(aShell);
}

PRInt32 nsDocument::GetNumberOfShells()
{
  return mPresShells.Count();
}

nsIPresShell* nsDocument::GetShellAt(PRInt32 aIndex)
{
  nsIPresShell* shell = (nsIPresShell*) mPresShells.ElementAt(aIndex);
  if (nsnull != shell) {
    NS_ADDREF(shell);
  }
  return shell;
}

nsIDocument* nsDocument::GetParentDocument()
{
  if (nsnull != mParentDocument) {
    NS_ADDREF(mParentDocument);
  }
  return mParentDocument;
}

/**
 * Note that we do *not* AddRef our parent because that would
 * create a circular reference.
 */
void nsDocument::SetParentDocument(nsIDocument* aParent)
{
  mParentDocument = aParent;
}

void nsDocument::AddSubDocument(nsIDocument* aSubDoc)
{
  NS_ADDREF(aSubDoc);
  mSubDocuments.AppendElement(aSubDoc);
}

PRInt32 nsDocument::GetNumberOfSubDocuments()
{
  return mSubDocuments.Count();
}

nsIDocument* nsDocument::GetSubDocumentAt(PRInt32 aIndex)
{
  nsIDocument* doc = (nsIDocument*) mSubDocuments.ElementAt(aIndex);
  if (nsnull != doc) {
    NS_ADDREF(doc);
  }
  return doc;
}

nsIContent* nsDocument::GetRootContent()
{
  if (nsnull != mRootContent) {
    NS_ADDREF(mRootContent);
  }
  return mRootContent;
}

void nsDocument::SetRootContent(nsIContent* aRoot)
{
  if (mRootContent) {
    PRInt32 index = mChildren->IndexOf(mRootContent);
    if (aRoot) {
      mChildren->ReplaceElementAt(aRoot, index);
    } else {
      mChildren->RemoveElementAt(index);
    }
  } else if (aRoot) {
    mChildren->AppendElement(aRoot);
  }

  mRootContent = aRoot;
}

NS_IMETHODIMP 
nsDocument::ChildAt(PRInt32 aIndex, nsIContent*& aResult) const
{
  nsCOMPtr<nsIContent> content( dont_AddRef(NS_STATIC_CAST(nsIContent*, mChildren->ElementAt(aIndex))) );
  NS_IF_ADDREF(aResult = content);
  return NS_OK;
}

NS_IMETHODIMP 
nsDocument::IndexOf(nsIContent* aPossibleChild, PRInt32& aIndex) const
{
  aIndex = mChildren->IndexOf(aPossibleChild);
  return NS_OK;
}

NS_IMETHODIMP 
nsDocument::GetChildCount(PRInt32& aCount)
{
  PRUint32 count;
  mChildren->Count(&count);
  aCount = NS_STATIC_CAST(PRInt32, count);
  return NS_OK;
}

PRInt32 nsDocument::GetNumberOfStyleSheets()
{
  return mStyleSheets.Count();
}

nsIStyleSheet* nsDocument::GetStyleSheetAt(PRInt32 aIndex)
{
  nsIStyleSheet* sheet = (nsIStyleSheet*)mStyleSheets.ElementAt(aIndex);
  NS_IF_ADDREF(sheet);
  return sheet;
}

PRInt32 nsDocument::GetIndexOfStyleSheet(nsIStyleSheet* aSheet)
{
  return mStyleSheets.IndexOf(aSheet);
}

void nsDocument::InternalAddStyleSheet(nsIStyleSheet* aSheet)  // subclass hook for sheet ordering
{
  mStyleSheets.AppendElement(aSheet);
}

void nsDocument::AddStyleSheetToStyleSets(nsIStyleSheet* aSheet)
{
  PRInt32 count = mPresShells.Count();
  PRInt32 index;
  for (index = 0; index < count; index++) {
    nsIPresShell* shell = (nsIPresShell*)mPresShells.ElementAt(index);
    nsCOMPtr<nsIStyleSet> set;
    if (NS_SUCCEEDED(shell->GetStyleSet(getter_AddRefs(set)))) {
      if (set) {
        set->AddDocStyleSheet(aSheet, this);
      }
    }
  }
}

void nsDocument::AddStyleSheet(nsIStyleSheet* aSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");
  InternalAddStyleSheet(aSheet);
  NS_ADDREF(aSheet);
  aSheet->SetOwningDocument(this);

  PRBool enabled = PR_TRUE;
  aSheet->GetEnabled(enabled);

  if (enabled) {
    AddStyleSheetToStyleSets(aSheet);

    // XXX should observers be notified for disabled sheets??? I think not, but I could be wrong
    for (PRInt32 index = 0; index < mObservers.Count(); index++) {
      nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers.ElementAt(index);
      observer->StyleSheetAdded(this, aSheet);
      if (observer != (nsIDocumentObserver*)mObservers.ElementAt(index)) {
        index--;
      }
    }
  }
}

void nsDocument::RemoveStyleSheetFromStyleSets(nsIStyleSheet* aSheet)
{
  PRInt32 count = mPresShells.Count();
  PRInt32 index;
  for (index = 0; index < count; index++) {
    nsIPresShell* shell = (nsIPresShell*)mPresShells.ElementAt(index);
    nsCOMPtr<nsIStyleSet> set;
    if (NS_SUCCEEDED(shell->GetStyleSet(getter_AddRefs(set)))) {
      if (set) {
        set->RemoveDocStyleSheet(aSheet);
      }
    }
  }
}

void nsDocument::RemoveStyleSheet(nsIStyleSheet* aSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");
  mStyleSheets.RemoveElement(aSheet);
  
  PRBool enabled = PR_TRUE;
  aSheet->GetEnabled(enabled);

  if (enabled) {
    RemoveStyleSheetFromStyleSets(aSheet);

    // XXX should observers be notified for disabled sheets??? I think not, but I could be wrong
    for (PRInt32 index = 0; index < mObservers.Count(); index++) {
      nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers.ElementAt(index);
      observer->StyleSheetRemoved(this, aSheet);
      if (observer != (nsIDocumentObserver*)mObservers.ElementAt(index)) {
        index--;
      }
    }
  }

  aSheet->SetOwningDocument(nsnull);
  NS_RELEASE(aSheet);
}

NS_IMETHODIMP
nsDocument::UpdateStyleSheets(nsISupportsArray* aOldSheets, nsISupportsArray* aNewSheets)
{
  PRUint32 oldCount;
  aOldSheets->Count(&oldCount);
  nsCOMPtr<nsIStyleSheet> sheet;
  PRUint32 i;
  for (i = 0; i < oldCount; i++) {
    nsCOMPtr<nsISupports> supp;
    aOldSheets->GetElementAt(i, getter_AddRefs(supp));
    sheet = do_QueryInterface(supp);
    if (sheet) {
      mStyleSheets.RemoveElement(sheet);
      PRBool enabled = PR_TRUE;
      sheet->GetEnabled(enabled);
      if (enabled) {
        RemoveStyleSheetFromStyleSets(sheet);
      }

      sheet->SetOwningDocument(nsnull);
      nsIStyleSheet* sheetPtr = sheet.get();
      NS_RELEASE(sheetPtr);
    }
  }

  PRUint32 newCount;
  aNewSheets->Count(&newCount);
  for (i = 0; i < newCount; i++) {
    nsCOMPtr<nsISupports> supp;
    aNewSheets->GetElementAt(i, getter_AddRefs(supp));
    sheet = do_QueryInterface(supp);
    if (sheet) {
      InternalAddStyleSheet(sheet);
      nsIStyleSheet* sheetPtr = sheet;
      NS_ADDREF(sheetPtr);
      sheet->SetOwningDocument(this);

      PRBool enabled = PR_TRUE;
      sheet->GetEnabled(enabled);
      if (enabled) {
        AddStyleSheetToStyleSets(sheet);
        sheet->SetOwningDocument(nsnull);
      }
    }
  }

  for (PRInt32 index = 0; index < mObservers.Count(); index++) {
    nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers.ElementAt(index);
    observer->StyleSheetRemoved(this, sheet);
    if (observer != (nsIDocumentObserver*)mObservers.ElementAt(index)) {
      index--;
    }
  }

  return NS_OK;
}


void 
nsDocument::InternalInsertStyleSheetAt(nsIStyleSheet* aSheet, PRInt32 aIndex)
{ // subclass hook for sheet ordering
  mStyleSheets.InsertElementAt(aSheet, aIndex);
}

NS_IMETHODIMP
nsDocument::InsertStyleSheetAt(nsIStyleSheet* aSheet, PRInt32 aIndex, PRBool aNotify)
{
  NS_PRECONDITION(nsnull != aSheet, "null ptr");
  InternalInsertStyleSheetAt(aSheet, aIndex);

  NS_ADDREF(aSheet);
  aSheet->SetOwningDocument(this);

  PRBool enabled = PR_TRUE;
  aSheet->GetEnabled(enabled);

  PRInt32 count;
  PRInt32 index;
  if (enabled) {
    count = mPresShells.Count();
    for (index = 0; index < count; index++) {
      nsIPresShell* shell = (nsIPresShell*)mPresShells.ElementAt(index);
      nsCOMPtr<nsIStyleSet> set;
      shell->GetStyleSet(getter_AddRefs(set));
      if (set) {
        set->AddDocStyleSheet(aSheet, this);
      }
    }
  }
  if (aNotify) {  // notify here even if disabled, there may have been others that weren't notified
    for (index = 0; index < mObservers.Count(); index++) {
      nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers.ElementAt(index);
      observer->StyleSheetAdded(this, aSheet);
      if (observer != (nsIDocumentObserver*)mObservers.ElementAt(index)) {
        index--;
      }
    }
  }
  return NS_OK;
}


void nsDocument::SetStyleSheetDisabledState(nsIStyleSheet* aSheet,
                                            PRBool aDisabled)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");
  PRInt32 index = mStyleSheets.IndexOf((void *)aSheet);
  PRInt32 count;
  // If we're actually in the document style sheet list
  if (-1 != index) {
    count = mPresShells.Count();
    for (index = 0; index < count; index++) {
      nsIPresShell* shell = (nsIPresShell*)mPresShells.ElementAt(index);
      nsCOMPtr<nsIStyleSet> set;
      if (NS_SUCCEEDED(shell->GetStyleSet(getter_AddRefs(set)))) {
        if (set) {
          if (aDisabled) {
            set->RemoveDocStyleSheet(aSheet);
          }
          else {
            set->AddDocStyleSheet(aSheet, this);
          }
        }
      }
    }
  }  

  for (index = 0; index < mObservers.Count(); index++) {
    nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers.ElementAt(index);
    observer->StyleSheetDisabledStateChanged(this, aSheet, aDisabled);
    if (observer != (nsIDocumentObserver*)mObservers.ElementAt(index)) {
      index--;
    }
  }
}

NS_IMETHODIMP 
nsDocument::GetScriptGlobalObject(nsIScriptGlobalObject** aScriptGlobalObject)
{
   NS_ENSURE_ARG_POINTER(aScriptGlobalObject);

   *aScriptGlobalObject = mScriptGlobalObject;
   NS_IF_ADDREF(*aScriptGlobalObject);
   return NS_OK;
}

NS_IMETHODIMP 
nsDocument::SetScriptGlobalObject(nsIScriptGlobalObject *aScriptGlobalObject)
{
  // XXX HACK ALERT! If the script context owner is null, the document
  // will soon be going away. So tell our content that to lose its
  // reference to the document. This has to be done before we
  // actually set the script context owner to null so that the
  // content elements can remove references to their script objects.
  if (!aScriptGlobalObject) {
    PRUint32 ucount, index;
    mChildren->Count(&ucount);
    for (index = 0; index < ucount; index++) {
      nsCOMPtr<nsIContent> content(dont_AddRef(NS_STATIC_CAST(nsIContent*,mChildren->ElementAt(index))));
      content->SetDocument(nsnull, PR_TRUE, PR_TRUE);
    }

    // Propagate the out-of-band notification to each PresShell's
    // anonymous content as well. This ensures that there aren't any
    // accidental script references left in anonymous content keeping
    // the document alive. (While not strictly necessary -- the
    // PresShell owns us -- it's tidy.)
    PRInt32 count;
    for (count = mPresShells.Count() - 1; count >= 0; --count) {
      nsIPresShell* shell = NS_STATIC_CAST(nsIPresShell*, mPresShells[count]);
      if (! shell)
        continue;

      shell->ReleaseAnonymousContent();
    }
  }

  mScriptGlobalObject = aScriptGlobalObject;
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetNameSpaceManager(nsINameSpaceManager*& aManager)
{
  aManager = mNameSpaceManager;
  NS_IF_ADDREF(aManager);
  return NS_OK;
}


// Note: We don't hold a reference to the document observer; we assume
// that it has a live reference to the document.
void nsDocument::AddObserver(nsIDocumentObserver* aObserver)
{
  // XXX Make sure the observer isn't already in the list
  if (mObservers.IndexOf(aObserver) == -1) {
    mObservers.AppendElement(aObserver);
  }
}

PRBool nsDocument::RemoveObserver(nsIDocumentObserver* aObserver)
{
  // If we're in the process of destroying the document (and we're
  // informing the observers of the destruction), don't remove the
  // observers from the list. This is not a big deal, since we
  // don't hold a live reference to the observers.
  if (!mInDestructor)
    return mObservers.RemoveElement(aObserver);
  else
    return (mObservers.IndexOf(aObserver) != -1);
}

NS_IMETHODIMP 
nsDocument::BeginUpdate()
{
  PRInt32 i;
  // Get new value of count for every iteration in case
  // observers remove themselves during the loop.
  for (i = 0; i < mObservers.Count(); i++) {
    nsIDocumentObserver* observer = (nsIDocumentObserver*) mObservers[i];
    observer->BeginUpdate(this);
    // Make sure that the observer didn't remove itself during the
    // notification. If it did, update our index and count.
    if (observer != (nsIDocumentObserver*)mObservers[i]) {
      i--;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsDocument::EndUpdate()
{
  PRInt32 i;
  // Get new value of count for every iteration in case
  // observers remove themselves during the loop.
  for (i = 0; i < mObservers.Count(); i++) {
    nsIDocumentObserver* observer = (nsIDocumentObserver*) mObservers[i];
    observer->EndUpdate(this);
    // Make sure that the observer didn't remove itself during the
    // notification. If it did, update our index and count.
    if (observer != (nsIDocumentObserver*)mObservers[i]) {
      i--;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::BeginLoad()
{
  PRInt32 i;
  // Get new value of count for every iteration in case
  // observers remove themselves during the loop.
  for (i = 0; i < mObservers.Count(); i++) {
    nsIDocumentObserver* observer = (nsIDocumentObserver*) mObservers[i];
    observer->BeginLoad(this);
    // Make sure that the observer didn't remove itself during the
    // notification. If it did, update our index and count.
    if (observer != (nsIDocumentObserver*)mObservers[i]) {
      i--;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::EndLoad()
{
  PRInt32 i;
  // Get new value of count for every iteration in case
  // observers remove themselves during the loop.
  for (i = 0; i < mObservers.Count(); i++) {
    nsIDocumentObserver* observer = (nsIDocumentObserver*) mObservers[i];
    observer->EndLoad(this);
    // Make sure that the observer didn't remove itself during the
    // notification. If it did, update our index and count.
    if (observer != (nsIDocumentObserver*)mObservers[i]) {
      i--;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::ContentChanged(nsIContent* aContent,
                           nsISupports* aSubContent)
{
  PRInt32 i;
  // Get new value of count for every iteration in case
  // observers remove themselves during the loop.
  for (i = 0; i < mObservers.Count(); i++) {
    nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
    observer->ContentChanged(this, aContent, aSubContent);
    // Make sure that the observer didn't remove itself during the
    // notification. If it did, update our index and count.
    if (observer != (nsIDocumentObserver*)mObservers[i]) {
      i--;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::ContentStatesChanged(nsIContent* aContent1,
                                 nsIContent* aContent2)
{
  PRInt32 i;
  // Get new value of count for every iteration in case
  // observers remove themselves during the loop.
  for (i = 0; i < mObservers.Count(); i++) {
    nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
    observer->ContentStatesChanged(this, aContent1, aContent2);
    // Make sure that the observer didn't remove itself during the
    // notification. If it did, update our index and count.
    if (observer != (nsIDocumentObserver*)mObservers[i]) {
      i--;
    }
  }
  return NS_OK;
}


NS_IMETHODIMP
nsDocument::ContentAppended(nsIContent* aContainer,
                            PRInt32 aNewIndexInContainer)
{
  PRInt32 i;
  // Get new value of count for every iteration in case
  // observers remove themselves during the loop.
  for (i = 0; i < mObservers.Count(); i++) {
    nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
    observer->ContentAppended(this, aContainer, aNewIndexInContainer);
    // Make sure that the observer didn't remove itself during the
    // notification. If it did, update our index and count.
    if (observer != (nsIDocumentObserver*)mObservers[i]) {
      i--;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::ContentInserted(nsIContent* aContainer,
                            nsIContent* aChild,
                            PRInt32 aIndexInContainer)
{
  PRInt32 i;
  // Get new value of count for every iteration in case
  // observers remove themselves during the loop.
  for (i = 0; i < mObservers.Count(); i++) {
    nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
    observer->ContentInserted(this, aContainer, aChild, aIndexInContainer);
    // Make sure that the observer didn't remove itself during the
    // notification. If it did, update our index and count.
    if (observer != (nsIDocumentObserver*)mObservers[i]) {
      i--;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::ContentReplaced(nsIContent* aContainer,
                            nsIContent* aOldChild,
                            nsIContent* aNewChild,
                            PRInt32 aIndexInContainer)
{
  PRInt32 i;
  // Get new value of count for every iteration in case
  // observers remove themselves during the loop.
  for (i = 0; i < mObservers.Count(); i++) {
    nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
    observer->ContentReplaced(this, aContainer, aOldChild, aNewChild,
                              aIndexInContainer);
    // Make sure that the observer didn't remove itself during the
    // notification. If it did, update our index and count.
    if (observer != (nsIDocumentObserver*)mObservers[i]) {
      i--;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::ContentRemoved(nsIContent* aContainer,
                           nsIContent* aChild,
                           PRInt32 aIndexInContainer)
{
  PRInt32 i;
  // Get new value of count for every iteration in case
  // observers remove themselves during the loop.
  for (i = 0; i < mObservers.Count(); i++) {
    nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
    observer->ContentRemoved(this, aContainer, 
                             aChild, aIndexInContainer);
    // Make sure that the observer didn't remove itself during the
    // notification. If it did, update our index and count.
    if (observer != (nsIDocumentObserver*)mObservers[i]) {
      i--;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::AttributeChanged(nsIContent* aChild,
                             PRInt32 aNameSpaceID,
                             nsIAtom* aAttribute,
                             PRInt32 aHint)
{
  PRInt32 i;
  nsresult result = NS_OK;
  // Get new value of count for every iteration in case
  // observers remove themselves during the loop.
  for (i = 0; i < mObservers.Count(); i++) {
    nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
    nsresult rv = observer->AttributeChanged(this, aChild, aNameSpaceID, aAttribute, aHint);
    if (NS_FAILED(rv) && NS_SUCCEEDED(result))
      result = rv;
    // Make sure that the observer didn't remove itself during the
    // notification. If it did, update our index and count.
    if (observer != (nsIDocumentObserver*)mObservers[i]) {
      i--;
    }
  }
  return result;
}


NS_IMETHODIMP
nsDocument::StyleRuleChanged(nsIStyleSheet* aStyleSheet, nsIStyleRule* aStyleRule,
                             PRInt32 aHint)
{
  PRInt32 i;
  // Get new value of count for every iteration in case
  // observers remove themselves during the loop.
  for (i = 0; i < mObservers.Count(); i++) {
    nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
    observer->BeginUpdate(this);
    observer->StyleRuleChanged(this, aStyleSheet, aStyleRule, aHint);
    // Make sure that the observer didn't remove itself during the
    // notification. If it did, update our index and count.
    if (observer != (nsIDocumentObserver*)mObservers[i]) {
      i--;
    }
    else {
      observer->EndUpdate(this);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::StyleRuleAdded(nsIStyleSheet* aStyleSheet, nsIStyleRule* aStyleRule)
{
  PRInt32 i;
  // Get new value of count for every iteration in case
  // observers remove themselves during the loop.
  for (i = 0; i < mObservers.Count(); i++) {
    nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
    observer->BeginUpdate(this);
    observer->StyleRuleAdded(this, aStyleSheet, aStyleRule);
    // Make sure that the observer didn't remove itself during the
    // notification. If it did, update our index and count.
    if (observer != (nsIDocumentObserver*)mObservers[i]) {
      i--;
    }
    else {
      observer->EndUpdate(this);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::StyleRuleRemoved(nsIStyleSheet* aStyleSheet, nsIStyleRule* aStyleRule)
{
  PRInt32 i;
  // Get new value of count for every iteration in case
  // observers remove themselves during the loop.
  for (i = 0; i < mObservers.Count(); i++) {
    nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
    observer->BeginUpdate(this);
    observer->StyleRuleRemoved(this, aStyleSheet, aStyleRule);
    // Make sure that the observer didn't remove itself during the
    // notification. If it did, update our index and count.
    if (observer != (nsIDocumentObserver*)mObservers[i]) {
      i--;
    }
    else {
      observer->EndUpdate(this);
    }
  }
  return NS_OK;
}


nsresult nsDocument::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;
  nsCOMPtr<nsIScriptGlobalObject> global;

  if (nsnull == mScriptObject) {
    // XXX We make the (possibly erroneous) assumption that the first
    // presentation shell represents the "primary view" of the document
    // and that the JS parent chain should incorporate just that view.
    // This is done for lack of a better model when we have multiple
    // views.
    nsIPresShell* shell = (nsIPresShell*) mPresShells.ElementAt(0);
    if (shell) {
      nsCOMPtr<nsIPresContext> cx;
      shell->GetPresContext(getter_AddRefs(cx));
      nsCOMPtr<nsISupports> container;
      
      res = cx->GetContainer(getter_AddRefs(container));
      if (NS_SUCCEEDED(res) && container) {
        global = do_GetInterface(container);
      }
    }
    // XXX If we can't find a view, parent to the calling context's
    // global object. This may not be right either, but we need
    // something.
    else {
      global = getter_AddRefs(aContext->GetGlobalObject());
    }
    
    if (NS_SUCCEEDED(res)) {
      res = NS_NewScriptDocument(aContext, 
                                 (nsISupports *)(nsIDOMDocument *)this, 
                                 (nsISupports *)global, 
                                 (void**)&mScriptObject);
    }
  }
  
  *aScriptObject = mScriptObject;

  return res;
}

nsresult nsDocument::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}


//
// nsIDOMDocument interface
//
NS_IMETHODIMP    
nsDocument::GetDoctype(nsIDOMDocumentType** aDoctype)
{
  NS_ENSURE_ARG_POINTER(aDoctype);

  *aDoctype = nsnull;
  PRUint32 i, count;
  mChildren->Count(&count);
  nsCOMPtr<nsIDOMNode> rootContentNode( do_QueryInterface(mRootContent) );
  nsCOMPtr<nsIDOMNode> node;

  for (i = 0; i < count; i++) {
    mChildren->QueryElementAt(i, NS_GET_IID(nsIDOMNode), getter_AddRefs(node));

    NS_ASSERTION(node, "null element of mChildren");

    // doctype can't be after the root
    // XXX Do we really want to enforce this when we don't enforce
    // anything else?
    if (node == rootContentNode)
      return NS_OK;

    if (node) {
      PRUint16 nodeType;

      node->GetNodeType(&nodeType);

      if (nodeType == nsIDOMNode::DOCUMENT_TYPE_NODE) {
        return node->QueryInterface(NS_GET_IID(nsIDOMDocumentType),
                                    (void **)aDoctype);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsDocument::GetImplementation(nsIDOMDOMImplementation** aImplementation)
{
  // For now, create a new implementation every time. This shouldn't
  // be a high bandwidth operation
  nsDOMImplementation* impl = new nsDOMImplementation(mDocumentURL);
  if (nsnull == impl) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return impl->QueryInterface(NS_GET_IID(nsIDOMDOMImplementation), (void**)aImplementation);
}

NS_IMETHODIMP    
nsDocument::GetDocumentElement(nsIDOMElement** aDocumentElement)
{
  if (nsnull == aDocumentElement) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult res = NS_OK;

  if (nsnull != mRootContent) {
    res = mRootContent->QueryInterface(NS_GET_IID(nsIDOMElement), (void**)aDocumentElement);
    NS_ASSERTION(NS_OK == res, "Must be a DOM Element");
  } else {
    *aDocumentElement = nsnull;
  }
  
  return res;
}

NS_IMETHODIMP    
nsDocument::CreateElement(const nsAReadableString& aTagName, 
                          nsIDOMElement** aReturn)
{
  // Should be implemented by subclass
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocument::CreateTextNode(const nsAReadableString& aData, nsIDOMText** aReturn)
{
  nsCOMPtr<nsIContent> text;
  nsresult rv = NS_NewTextNode(getter_AddRefs(text));

  if (NS_OK == rv) {
    rv = text->QueryInterface(NS_GET_IID(nsIDOMText), (void**)aReturn);
    (*aReturn)->AppendData(aData);
  }

  return rv;
}

NS_IMETHODIMP    
nsDocument::CreateDocumentFragment(nsIDOMDocumentFragment** aReturn)
{
  return NS_NewDocumentFragment(aReturn, this);
}

NS_IMETHODIMP    
nsDocument::CreateComment(const nsAReadableString& aData, nsIDOMComment** aReturn)
{
  nsCOMPtr<nsIContent> comment;
  nsresult rv = NS_NewCommentNode(getter_AddRefs(comment));

  if (NS_OK == rv) {
    rv = comment->QueryInterface(NS_GET_IID(nsIDOMComment), (void**)aReturn);
    (*aReturn)->AppendData(aData);
  }

  return rv;
}

NS_IMETHODIMP 
nsDocument::CreateCDATASection(const nsAReadableString& aData, nsIDOMCDATASection** aReturn)
{
  // Should be implemented by subclass
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::CreateProcessingInstruction(const nsAReadableString& aTarget, 
                                        const nsAReadableString& aData, 
                                        nsIDOMProcessingInstruction** aReturn)
{
  // Should be implemented by subclass
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::CreateAttribute(const nsAReadableString& aName, 
                            nsIDOMAttr** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  NS_ENSURE_TRUE(mNodeInfoManager, NS_ERROR_NOT_INITIALIZED);

  nsAutoString value;
  nsDOMAttribute* attribute;

  nsCOMPtr<nsINodeInfo> nodeInfo;
  nsresult rv = mNodeInfoManager->GetNodeInfo(aName, nsnull, kNameSpaceID_None,
                                              *getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv); 
 
  attribute = new nsDOMAttribute(nsnull, nodeInfo, value);
  NS_ENSURE_TRUE(attribute, NS_ERROR_OUT_OF_MEMORY); 

  return attribute->QueryInterface(NS_GET_IID(nsIDOMAttr), (void**)aReturn);
}

NS_IMETHODIMP    
nsDocument::CreateEntityReference(const nsAReadableString& aName, 
                                  nsIDOMEntityReference** aReturn)
{
  // Should be implemented by subclass
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::GetElementsByTagName(const nsAReadableString& aTagname, 
                                 nsIDOMNodeList** aReturn)
{
  nsCOMPtr<nsIAtom> nameAtom(dont_AddRef(NS_NewAtom(aTagname)));

  nsContentList* list = new nsContentList(this, nameAtom, kNameSpaceID_Unknown);
  NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);

  return list->QueryInterface(NS_GET_IID(nsIDOMNodeList), (void **)aReturn);
}

NS_IMETHODIMP    
nsDocument::GetElementsByTagNameNS(const nsAReadableString& aNamespaceURI,
                                   const nsAReadableString& aLocalName,
                                   nsIDOMNodeList** aReturn)
{

  PRInt32 nameSpaceId = kNameSpaceID_Unknown;

  nsContentList* list = nsnull;

  if (!aNamespaceURI.Equals(NS_LITERAL_STRING("*"))) {
    mNameSpaceManager->GetNameSpaceID(aNamespaceURI, nameSpaceId);

    if (nameSpaceId == kNameSpaceID_Unknown) {
      // Unkonwn namespace means no matches, we create an empty list...
      list = new nsContentList(this, nsnull, kNameSpaceID_None);
      NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);
    }
  }

  if (!list) {
    nsCOMPtr<nsIAtom> nameAtom(dont_AddRef(NS_NewAtom(aLocalName)));
    list = new nsContentList(this, nameAtom, nameSpaceId);
    NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);
  }

  return list->QueryInterface(NS_GET_IID(nsIDOMNodeList), (void **)aReturn);
}

NS_IMETHODIMP    
nsDocument::GetStyleSheets(nsIDOMStyleSheetList** aStyleSheets)
{
  if (nsnull == mDOMStyleSheets) {
    mDOMStyleSheets = new nsDOMStyleSheetList(this);
    if (nsnull == mDOMStyleSheets) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mDOMStyleSheets);
  }

  *aStyleSheets = mDOMStyleSheets;
  NS_ADDREF(mDOMStyleSheets);

  return NS_OK;
}

NS_IMETHODIMP    
nsDocument::GetCharacterSet(nsAWritableString& aCharacterSet)
{
  return GetDocumentCharacterSet(aCharacterSet);
}

NS_IMETHODIMP
nsDocument::ImportNode(nsIDOMNode* aImportedNode,
                       PRBool aDeep,
                       nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG(aImportedNode);
  NS_ENSURE_ARG_POINTER(aReturn);

  return aImportedNode->CloneNode(aDeep, aReturn);
}

NS_IMETHODIMP
nsDocument::AddBinding(nsIDOMElement* aContent, const nsAReadableString& aURL)
{
  nsCOMPtr<nsIBindingManager> bm;
  GetBindingManager(getter_AddRefs(bm));
  nsCOMPtr<nsIContent> content(do_QueryInterface(aContent));

  return bm->AddLayeredBinding(content, aURL);
}

NS_IMETHODIMP
nsDocument::RemoveBinding(nsIDOMElement* aContent, const nsAReadableString& aURL)
{
  if (mBindingManager) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(aContent));
    return mBindingManager->RemoveLayeredBinding(content, aURL);
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDocument::LoadBindingDocument(const nsAReadableString& aURL, nsIDOMDocument** aResult)
{
  if (mBindingManager) {
    nsCOMPtr<nsIDocument> doc;
    mBindingManager->LoadBindingDocument(this, aURL, getter_AddRefs(doc));
    nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(doc));
    *aResult = domDoc;
    NS_IF_ADDREF(*aResult);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDocument::GetBindingParent(nsIDOMNode* aNode, nsIDOMElement** aResult)
{
  *aResult = nsnull;
  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
  if (!content)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> result;
  content->GetBindingParent(getter_AddRefs(result));
  nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(result));
  *aResult = elt;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

static nsresult
GetElementByAttribute(nsIContent* aContent, 
                      nsIAtom* aAttrName,
                      const nsAReadableString& aAttrValue,
                      PRBool aUniversalMatch,
                      nsIDOMElement** aResult)
{
  nsAutoString value;
  nsresult rv = aContent->GetAttribute(kNameSpaceID_None, aAttrName, value);
  if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
    if (aUniversalMatch || value.Equals(aAttrValue))
      return aContent->QueryInterface(NS_GET_IID(nsIDOMElement), (void**)aResult);
  }
  
  PRInt32 childCount;
  aContent->ChildCount(childCount);

  for (PRInt32 i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIContent> current;
    current->ChildAt(i, *getter_AddRefs(current));

    GetElementByAttribute(current, aAttrName, aAttrValue, aUniversalMatch, aResult);

    if (*aResult)
      return NS_OK;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetAnonymousElementByAttribute(nsIDOMElement* aElement,
                                           const nsAReadableString& aAttrName, 
                                           const nsAReadableString& aAttrValue, 
                                           nsIDOMElement** aResult)
{
  *aResult = nsnull;

  nsCOMPtr<nsIDOMNodeList> nodeList;
  GetAnonymousNodes(aElement, getter_AddRefs(nodeList));
  
  if (!nodeList) 
    return NS_OK;

  nsCOMPtr<nsIAtom> attribute = getter_AddRefs(NS_NewAtom(aAttrName));

  PRUint32 length;
  nodeList->GetLength(&length);

  PRBool universalMatch = aAttrValue.Equals(NS_LITERAL_STRING("*"));

  for (PRUint32 i = 0; i < length; ++i) {
    nsCOMPtr<nsIDOMNode> current;
    nodeList->Item(i, getter_AddRefs(current));
    
    nsCOMPtr<nsIContent> content(do_QueryInterface(current));

    GetElementByAttribute(content, attribute, aAttrValue, universalMatch, aResult);
    if (*aResult)
      return NS_OK;
  }

  return NS_OK;
}


NS_IMETHODIMP
nsDocument::GetAnonymousNodes(nsIDOMElement* aElement,
                              nsIDOMNodeList** aResult)
{
  *aResult = nsnull;
  if (mBindingManager) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(aElement));
    return mBindingManager->GetAnonymousNodesFor(content, aResult);
  }
  return NS_OK;
}

NS_IMETHODIMP    
nsDocument::GetLocation(jsval* aLocation)
{
  if (mScriptGlobalObject) {
    nsCOMPtr<nsIDOMWindowInternal> window(do_QueryInterface(mScriptGlobalObject));
    if(window) {
      return window->GetLocation(aLocation);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP    
nsDocument::SetLocation(jsval aLocation)
{
  if (mScriptGlobalObject) {
    nsCOMPtr<nsIDOMWindowInternal> window(do_QueryInterface(mScriptGlobalObject));
    if(window) {
      return window->SetLocation(aLocation);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP    
nsDocument::CreateRange(nsIDOMRange** aReturn)
{
  return NS_NewRange(aReturn);
}


NS_IMETHODIMP
nsDocument::GetDefaultView(nsIDOMAbstractView** aDefaultView)
{
  NS_ENSURE_ARG_POINTER(aDefaultView);
  *aDefaultView = nsnull;

  nsIPresShell *shell = NS_STATIC_CAST(nsIPresShell *,
                                       mPresShells.ElementAt(0));
  NS_ENSURE_TRUE(shell, NS_OK);

  nsCOMPtr<nsIPresContext> ctx;
  nsresult rv = shell->GetPresContext(getter_AddRefs(ctx));
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && ctx, rv);

  nsCOMPtr<nsISupports> container;
  rv = ctx->GetContainer(getter_AddRefs(container));
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && container, rv);

  nsCOMPtr<nsIInterfaceRequestor> ifrq(do_QueryInterface(container));
  NS_ENSURE_TRUE(ifrq, NS_OK);

  nsCOMPtr<nsIDOMWindowInternal> window;
  ifrq->GetInterface(NS_GET_IID(nsIDOMWindowInternal), getter_AddRefs(window));
  NS_ENSURE_TRUE(window, NS_OK);

  window->QueryInterface(NS_GET_IID(nsIDOMAbstractView),
                         (void **)aDefaultView);

  return NS_OK;
}


NS_IMETHODIMP
nsDocument::Load (const nsAReadableString& aUrl)
{
  // Should be implemented by subclass
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::GetPlugins(nsIDOMPluginArray** aPlugins)
{
  NS_ENSURE_ARG_POINTER(aPlugins);
  *aPlugins = nsnull;

  // XXX Could also get this through mScriptGlobalObject
  nsIPresShell *shell = NS_STATIC_CAST(nsIPresShell *,
                                       mPresShells.ElementAt(0));
  NS_ENSURE_TRUE(shell, NS_OK);

  nsCOMPtr<nsIPresContext> ctx;
  nsresult rv = shell->GetPresContext(getter_AddRefs(ctx));
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && ctx, rv);

  nsCOMPtr<nsISupports> container;
  rv = ctx->GetContainer(getter_AddRefs(container));
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && container, rv);

  nsCOMPtr<nsIInterfaceRequestor> ifrq(do_QueryInterface(container));
  NS_ENSURE_TRUE(ifrq, NS_OK);

  nsCOMPtr<nsIDOMWindowInternal> window;
  ifrq->GetInterface(NS_GET_IID(nsIDOMWindowInternal), getter_AddRefs(window));
  NS_ENSURE_TRUE(window, NS_OK);

  nsCOMPtr<nsIDOMNavigator> navigator;
  window->GetNavigator(getter_AddRefs(navigator));
  NS_ENSURE_TRUE(navigator, NS_OK);

  return navigator->GetPlugins(aPlugins);
}

NS_IMETHODIMP
nsDocument::GetBoxObjectFor(nsIDOMElement* aElement, nsIBoxObject** aResult)
{
  nsresult rv;

  *aResult = nsnull;

  if (!mBoxObjectTable)
    mBoxObjectTable = new nsSupportsHashtable;
  else {
    nsISupportsKey key(aElement);
    nsCOMPtr<nsISupports> supports(dont_AddRef(NS_STATIC_CAST(nsISupports*, mBoxObjectTable->Get(&key))));
    nsCOMPtr<nsIBoxObject> boxObject(do_QueryInterface(supports));
    if (boxObject) {
      *aResult = boxObject;
      NS_ADDREF(*aResult);
      return NS_OK;
    }
  }

  nsCOMPtr<nsIPresShell> shell(getter_AddRefs(GetShellAt(0)));
  if (!shell)
    return NS_ERROR_FAILURE;

  PRInt32 namespaceID;
  nsCOMPtr<nsIAtom> tag;
  NS_WITH_SERVICE(nsIXBLService, xblService, "@mozilla.org/xbl;1", &rv);
  nsCOMPtr<nsIContent> content(do_QueryInterface(aElement));
  xblService->ResolveTag(content, &namespaceID, getter_AddRefs(tag));
  
  nsCAutoString contractID("@mozilla.org/layout/xul-boxobject");
  if (namespaceID == nsXULAtoms::nameSpaceID) {
    if (tag.get() == nsXULAtoms::browser)
      contractID += "-browser";
    else if (tag.get() == nsXULAtoms::editor)
      contractID += "-editor";
    else if (tag.get() == nsXULAtoms::iframe)
      contractID += "-iframe";
    else if (tag.get() == nsXULAtoms::menu)
      contractID += "-menu";
    else if (tag.get() == nsXULAtoms::popupset)
      contractID += "-popupset";
    else if (tag.get() == nsXULAtoms::tree)
      contractID += "-tree";
    else if (tag.get() == nsXULAtoms::scrollbox)
      contractID += "-scrollbox";
  }
  contractID += ";1";
  
  nsCOMPtr<nsIBoxObject> boxObject(do_CreateInstance(contractID.get()));
  if (!boxObject) 
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsPIBoxObject> privateBox(do_QueryInterface(boxObject));
  if (NS_FAILED(rv = privateBox->Init(content, shell)))
    return rv;

  SetBoxObjectFor(aElement, boxObject);

  *aResult = boxObject;
  NS_ADDREF(*aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::SetBoxObjectFor(nsIDOMElement* aElement, nsIBoxObject* aBoxObject)
{
  if (!mBoxObjectTable) {
    if (!aBoxObject) 
      return NS_OK;
    mBoxObjectTable = new nsSupportsHashtable(12);
  }

  nsISupportsKey key(aElement);

  if (aBoxObject)
    mBoxObjectTable->Put(&key, aBoxObject);
  else {
    nsCOMPtr<nsISupports> supp;
    mBoxObjectTable->Remove(&key, getter_AddRefs(supp));
    nsCOMPtr<nsPIBoxObject> boxObject(do_QueryInterface(supp));
    if (boxObject)
      boxObject->SetDocument(nsnull);
  }

  return NS_OK;
}

//
// nsIDOMNode methods
//
NS_IMETHODIMP    
nsDocument::GetNodeName(nsAWritableString& aNodeName)
{
  aNodeName.Assign(NS_LITERAL_STRING("#document"));
  return NS_OK;
}

NS_IMETHODIMP    
nsDocument::GetNodeValue(nsAWritableString& aNodeValue)
{
  aNodeValue.Truncate();
  return NS_OK;
}

NS_IMETHODIMP    
nsDocument::SetNodeValue(const nsAReadableString& aNodeValue)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP    
nsDocument::GetNodeType(PRUint16* aNodeType)
{
  *aNodeType = nsIDOMNode::DOCUMENT_NODE;
  return NS_OK;
}

NS_IMETHODIMP    
nsDocument::GetParentNode(nsIDOMNode** aParentNode) 
{
  *aParentNode = nsnull;
  return NS_OK;
}

NS_IMETHODIMP    
nsDocument::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
  if (nsnull == mChildNodes) {
    mChildNodes = new nsDocumentChildNodes(this);
    if (nsnull == mChildNodes) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mChildNodes);
  }

  return mChildNodes->QueryInterface(NS_GET_IID(nsIDOMNodeList), (void**)aChildNodes);
}

NS_IMETHODIMP    
nsDocument::HasChildNodes(PRBool* aHasChildNodes)
{
  NS_ENSURE_ARG(aHasChildNodes);

  PRUint32 count;
  mChildren->Count(&count);
  *aHasChildNodes = (count != 0);

  return NS_OK;
}

NS_IMETHODIMP    
nsDocument::HasAttributes(PRBool* aHasAttributes)
{
  NS_ENSURE_ARG(aHasAttributes);

  *aHasAttributes = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP    
nsDocument::GetFirstChild(nsIDOMNode** aFirstChild)
{
  nsresult result = NS_OK;

  PRUint32 count;
  mChildren->Count(&count);
  if (count) {
    result = mChildren->QueryElementAt(0, NS_GET_IID(nsIDOMNode),
                                     NS_REINTERPRET_CAST(void**, aFirstChild));
  } else {
    *aFirstChild = nsnull;
  }

  return result;
}

NS_IMETHODIMP    
nsDocument::GetLastChild(nsIDOMNode** aLastChild)
{
  nsresult result = NS_OK;

  PRUint32 count;
  mChildren->Count(&count);
  if (count) {
    result = mChildren->QueryElementAt(count-1, NS_GET_IID(nsIDOMNode),
                                     NS_REINTERPRET_CAST(void**, aLastChild));
  } else {
    *aLastChild = nsnull;
  }

  return result;
}

NS_IMETHODIMP    
nsDocument::GetPreviousSibling(nsIDOMNode** aPreviousSibling)
{
  *aPreviousSibling = nsnull;
  return NS_OK;
}

NS_IMETHODIMP    
nsDocument::GetNextSibling(nsIDOMNode** aNextSibling)
{
  *aNextSibling = nsnull;
  return NS_OK;
}

NS_IMETHODIMP    
nsDocument::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
  *aAttributes = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetNamespaceURI(nsAWritableString& aNamespaceURI)
{ 
  aNamespaceURI.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetPrefix(nsAWritableString& aPrefix)
{
  aPrefix.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::SetPrefix(const nsAReadableString& aPrefix)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsDocument::GetLocalName(nsAWritableString& aLocalName)
{
  aLocalName.Truncate();
  return NS_OK;
}

NS_IMETHODIMP    
nsDocument::InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn)
{
  NS_ASSERTION(nsnull != aNewChild, "null ptr");
  PRInt32 index;
  PRUint16 nodeType;

  *aReturn = nsnull; // Do we need to do this?

  if (nsnull == aNewChild) {
    return NS_ERROR_NULL_POINTER;
  }

  // If it's a child type we can't handle (per DOM spec), or if it's an
  // element and we already have a root (our addition to DOM spec), throw
  // HIERARCHY_REQUEST_ERR.
  aNewChild->GetNodeType(&nodeType);
  if (((COMMENT_NODE != nodeType) &&
       (TEXT_NODE != nodeType) &&
       (PROCESSING_INSTRUCTION_NODE != nodeType) &&
       (DOCUMENT_TYPE_NODE != nodeType) &&
       (ELEMENT_NODE != nodeType)) ||
      ((ELEMENT_NODE == nodeType) && mRootContent)){
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }

  nsCOMPtr<nsIContent> content( do_QueryInterface(aNewChild) );
  if (!content) {
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }

  if (!aRefChild) {
    PRUint32 count;
    mChildren->Count(&count);
    index = count;
    mChildren->AppendElement(content);
  }
  else {
    nsCOMPtr<nsIContent> refContent( do_QueryInterface(aRefChild) );

    if (!refContent) {
      return NS_ERROR_DOM_NOT_FOUND_ERR;
    }

    index = mChildren->IndexOf(refContent);
    if (index != -1) {
      mChildren->InsertElementAt(content, index);
    } else {
      // couldn't find refChild
      return NS_ERROR_DOM_NOT_FOUND_ERR;
    }
  }

  // If we get here, we've succesfully inserted content into the
  // index-th spot in mChildren.
  if (ELEMENT_NODE == nodeType)
    mRootContent = content;
  ContentInserted(nsnull, content, index);

  content->SetDocument(this, PR_TRUE, PR_TRUE);
  *aReturn = aNewChild;
  NS_ADDREF(aNewChild);

  return NS_OK;
}

NS_IMETHODIMP    
nsDocument::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  NS_ASSERTION(((nsnull != aNewChild) && (nsnull != aOldChild)), "null ptr");
  nsresult result = NS_OK;
  PRInt32 index;
  PRUint16 nodeType;
  
  *aReturn = nsnull; // is this necessary?

  if ((nsnull == aNewChild) || (nsnull == aOldChild)) {
    return NS_ERROR_NULL_POINTER;
  }

  aNewChild->GetNodeType(&nodeType);

  if ((COMMENT_NODE != nodeType) &&
      (TEXT_NODE != nodeType) &&
      (PROCESSING_INSTRUCTION_NODE != nodeType) &&
      (DOCUMENT_TYPE_NODE != nodeType) &&
      (ELEMENT_NODE != nodeType)) {
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }

  nsCOMPtr<nsIContent> content( do_QueryInterface(aNewChild) );
  nsCOMPtr<nsIContent> refContent( do_QueryInterface(aOldChild) );
  if (!content || !refContent) {
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }

  if ((ELEMENT_NODE == nodeType) &&
      mRootContent &&
      (mRootContent != refContent.get()))
  {
    // Caller attempted to add a second element as a child.
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }
  
  index = mChildren->IndexOf(refContent);
  if (-1 == index) {
    // The reference child is not a child of the document.
    return NS_ERROR_DOM_NOT_FOUND_ERR;
  }

  refContent->SetDocument(nsnull, PR_TRUE, PR_TRUE);
  ContentRemoved(nsnull, refContent, index);

  mChildren->ReplaceElementAt(content, index);
  // This is OK because we checked above.
  if (ELEMENT_NODE == nodeType)
    mRootContent = content;

  ContentInserted(nsnull, content, index);
  content->SetDocument(this, PR_TRUE, PR_TRUE);
  *aReturn = aOldChild;
  NS_ADDREF(aOldChild);

  return result;
}

NS_IMETHODIMP    
nsDocument::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  NS_ASSERTION(nsnull != aOldChild, "null ptr");
  
  *aReturn = nsnull; // do we need to do this?

  if (nsnull == aOldChild) {
    return NS_ERROR_NULL_POINTER;
  }

  nsCOMPtr<nsIContent> content( do_QueryInterface(aOldChild) );
  if (!content) {
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }

  PRInt32 index = mChildren->IndexOf(content);
  if (-1 == index) {
    return NS_ERROR_DOM_NOT_FOUND_ERR;
  }

  ContentRemoved(nsnull, content, index);

  mChildren->RemoveElementAt(index);
  if (content.get() == mRootContent)
    mRootContent = nsnull;

  content->SetDocument(nsnull, PR_TRUE, PR_TRUE);

  *aReturn = aOldChild;
  NS_ADDREF(aOldChild);
  
  return NS_OK;
}

NS_IMETHODIMP    
nsDocument::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
  return InsertBefore(aNewChild, nsnull, aReturn);
}

NS_IMETHODIMP    
nsDocument::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  // XXX should be implemented by subclass
  *aReturn = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::Normalize()
{
  // XXX Not completely correct, since you can still have unnormalized
  // text nodes as immediate children of the document.
  if (mRootContent) {
    nsCOMPtr<nsIDOMNode> node(do_QueryInterface(mRootContent));

    if (node) {
      return node->Normalize();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::IsSupported(const nsAReadableString& aFeature,
                        const nsAReadableString& aVersion,
                        PRBool* aReturn)
{
  return nsGenericElement::InternalIsSupported(aFeature, aVersion, aReturn);
}

NS_IMETHODIMP    
nsDocument::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
  *aOwnerDocument = nsnull;
  return NS_OK;
}

nsresult nsDocument::GetListenerManager(nsIEventListenerManager **aInstancePtrResult)
{
  if (nsnull != mListenerManager) {
    return mListenerManager->QueryInterface(NS_GET_IID(nsIEventListenerManager), (void**) aInstancePtrResult);
  }
  if (NS_OK == GetNewListenerManager(aInstancePtrResult)) {
    mListenerManager = *aInstancePtrResult;
    NS_ADDREF(mListenerManager);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult nsDocument::GetNewListenerManager(nsIEventListenerManager **aInstancePtrResult)
{
  return NS_NewEventListenerManager(aInstancePtrResult);
} 

nsresult nsDocument::HandleEvent(nsIDOMEvent *aEvent)
{
  return DispatchEvent(aEvent);
} 

nsresult nsDocument::HandleDOMEvent(nsIPresContext* aPresContext, 
                                    nsEvent* aEvent, 
                                    nsIDOMEvent** aDOMEvent,
                                    PRUint32 aFlags,
                                    nsEventStatus* aEventStatus)
{
  nsresult mRet = NS_OK;

  nsIDOMEvent* domEvent = nsnull;

  if (NS_EVENT_FLAG_INIT & aFlags) {
    if (!aDOMEvent) {
      aDOMEvent = &domEvent;
    }
    aEvent->flags = aFlags;
    aFlags &= ~(NS_EVENT_FLAG_CANT_BUBBLE | NS_EVENT_FLAG_CANT_CANCEL);
  }
  
  //Capturing stage
  if (NS_EVENT_FLAG_BUBBLE != aFlags && nsnull != mScriptGlobalObject) {
    mScriptGlobalObject->HandleDOMEvent(aPresContext, aEvent, aDOMEvent, NS_EVENT_FLAG_CAPTURE, aEventStatus);
  }
  
  //Local handling stage
  if (mListenerManager && !(aEvent->flags & NS_EVENT_FLAG_STOP_DISPATCH) &&
      !(NS_EVENT_FLAG_BUBBLE & aFlags && NS_EVENT_FLAG_CANT_BUBBLE & aEvent->flags)) {
    aEvent->flags |= aFlags;
    mListenerManager->HandleEvent(aPresContext, aEvent, aDOMEvent, this, aFlags, aEventStatus);
    aEvent->flags &= ~aFlags;
  }

  //Bubbling stage
  if (NS_EVENT_FLAG_CAPTURE != aFlags && nsnull != mScriptGlobalObject) {
    mScriptGlobalObject->HandleDOMEvent(aPresContext, aEvent, aDOMEvent, NS_EVENT_FLAG_BUBBLE, aEventStatus);
  }

  if (NS_EVENT_FLAG_INIT & aFlags) {
    // We're leaving the DOM event loop so if we created a DOM event, release here.
    if (nsnull != *aDOMEvent) {
      nsrefcnt rc;
      NS_RELEASE2(*aDOMEvent, rc);
      if (0 != rc) {
      //Okay, so someone in the DOM loop (a listener, JS object) still has a ref to the DOM Event but
      //the internal data hasn't been malloc'd.  Force a copy of the data here so the DOM Event is still valid.
        nsIPrivateDOMEvent *mPrivateEvent;
        if (NS_OK == (*aDOMEvent)->QueryInterface(NS_GET_IID(nsIPrivateDOMEvent), (void**)&mPrivateEvent)) {
          mPrivateEvent->DuplicatePrivateData();
          NS_RELEASE(mPrivateEvent);
        }
      }
    }
    aDOMEvent = nsnull;
  }

  return mRet;
}

nsresult nsDocument::AddEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID)
{
  nsCOMPtr<nsIEventListenerManager> manager;

  GetListenerManager(getter_AddRefs(manager));
  if (manager) {
    manager->AddEventListenerByIID(aListener, aIID, NS_EVENT_FLAG_BUBBLE);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult nsDocument::RemoveEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID)
{
  if (nsnull != mListenerManager) {
    mListenerManager->RemoveEventListenerByIID(aListener, aIID, NS_EVENT_FLAG_BUBBLE);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult nsDocument::AddEventListener(const nsAReadableString& aType, nsIDOMEventListener* aListener, 
                                      PRBool aUseCapture)
{
  nsCOMPtr<nsIEventListenerManager> manager;

  GetListenerManager(getter_AddRefs(manager));
  if (manager) {
    PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

    manager->AddEventListenerByType(aListener, aType, flags);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult nsDocument::RemoveEventListener(const nsAReadableString& aType, nsIDOMEventListener* aListener, 
                                         PRBool aUseCapture)
{
  if (nsnull != mListenerManager) {
    PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

    mListenerManager->RemoveEventListenerByType(aListener, aType, flags);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDocument::DispatchEvent(nsIDOMEvent* aEvent)
{
  // Obtain a presentation context
  PRInt32 count = GetNumberOfShells();
  if (count == 0)
    return NS_OK;

  nsCOMPtr<nsIPresShell> shell(getter_AddRefs(GetShellAt(0)));
  
  // Retrieve the context
  nsCOMPtr<nsIPresContext> presContext;
  shell->GetPresContext(getter_AddRefs(presContext));

  nsCOMPtr<nsIEventStateManager> esm;
  if (NS_SUCCEEDED(presContext->GetEventStateManager(getter_AddRefs(esm)))) {
    return esm->DispatchNewEvent((nsISupports *)(nsIDOMDocument *)this, aEvent);
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDocument::CreateEvent(const nsAReadableString& aEventType, nsIDOMEvent** aReturn)
{
  // Obtain a presentation context
  PRInt32 count = GetNumberOfShells();
  if (count == 0)
    return NS_OK;

  nsCOMPtr<nsIPresShell> shell(getter_AddRefs(GetShellAt(0)));
  
  // Retrieve the context
  nsCOMPtr<nsIPresContext> presContext;
  shell->GetPresContext(getter_AddRefs(presContext));

  if (presContext) {
    nsCOMPtr<nsIEventListenerManager> manager;
    GetListenerManager(getter_AddRefs(manager));
    if (manager) {
      return manager->CreateEvent(presContext, nsnull, aEventType, aReturn);
    }
  }

  return NS_ERROR_FAILURE;
}

PRBool    nsDocument::AddProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp)
{
  return PR_TRUE;
}

PRBool    nsDocument::DeleteProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp)
{
  return PR_TRUE;
}

PRBool    nsDocument::GetProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp)
{
  return PR_TRUE;
}

PRBool    nsDocument::SetProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp)
{
  PRBool result = PR_TRUE;

  if (JS_TypeOfValue(aContext, *aVp) == JSTYPE_FUNCTION && JSVAL_IS_STRING(aID)) {
    const PRUnichar* str = NS_REINTERPRET_CAST(const PRUnichar *, JS_GetStringChars(JS_ValueToString(aContext, aID)));

    if (str && str[0] == 'o' && str[1] == 'n' && str[2]) {
      PRBool iidFound = PR_TRUE;
      nsIID theIID;
      nsCOMPtr<nsIAtom> atom(dont_AddRef(NS_NewAtom(str)));

      if (atom.get() == nsLayoutAtoms::onmousedown || atom.get() == nsLayoutAtoms::onmouseup || atom.get() ==  nsLayoutAtoms::onclick ||
         atom.get() == nsLayoutAtoms::onmouseover || atom.get() == nsLayoutAtoms::onmouseout) {
        theIID = NS_GET_IID(nsIDOMMouseListener);
      }
      else if (atom.get() == nsLayoutAtoms::onkeydown || atom.get() == nsLayoutAtoms::onkeyup || atom.get() == nsLayoutAtoms::onkeypress) {
        theIID = NS_GET_IID(nsIDOMKeyListener);
      }
      else if (atom.get() == nsLayoutAtoms::onmousemove) {
        theIID = NS_GET_IID(nsIDOMMouseMotionListener);
      }
      else if (atom.get() == nsLayoutAtoms::onfocus || atom.get() == nsLayoutAtoms::onblur) {
        theIID = NS_GET_IID(nsIDOMFocusListener);
      }
      else if (atom.get() == nsLayoutAtoms::onsubmit || atom.get() == nsLayoutAtoms::onreset || atom.get() == nsLayoutAtoms::onchange ||
               atom.get() == nsLayoutAtoms::onselect) {
        theIID = NS_GET_IID(nsIDOMFormListener);
      }
      else if (atom.get() == nsLayoutAtoms::onload || atom.get() == nsLayoutAtoms::onunload || atom.get() == nsLayoutAtoms::onabort ||
               atom.get() == nsLayoutAtoms::onerror) {
        theIID = NS_GET_IID(nsIDOMLoadListener);
      }
      else if (atom.get() == nsLayoutAtoms::onpaint) {
        theIID = NS_GET_IID(nsIDOMPaintListener);
      }
      else {
        iidFound = PR_FALSE;
      }

      if (iidFound) {
        nsCOMPtr<nsIEventListenerManager> manager;
        GetListenerManager(getter_AddRefs(manager));

        if (manager) {
          nsCOMPtr<nsIScriptContext> scriptContext;
          nsresult rv = nsLayoutUtils::GetStaticScriptContext(aContext, NS_REINTERPRET_CAST(JSObject*, mScriptObject),
                                                              getter_AddRefs(scriptContext));
          if (NS_SUCCEEDED(rv) && scriptContext) {
            rv = manager->RegisterScriptEventListener(scriptContext, this, atom, theIID);
          }
          if (NS_FAILED(rv))
            result = PR_FALSE;
        }
      }
    }
  }

  return result;
}

PRBool    nsDocument::EnumerateProperty(JSContext *aContext, JSObject *aObj)
{
  return PR_TRUE;
}

PRBool    nsDocument::Resolve(JSContext *aContext, JSObject *aObj, jsval aID,
                              PRBool *aDidDefineProperty)
{
  *aDidDefineProperty = PR_FALSE;

  return PR_TRUE;
}

PRBool    nsDocument::Convert(JSContext *aContext, JSObject *aObj, jsval aID)
{
  return PR_TRUE;
}

void      nsDocument::Finalize(JSContext *aContext, JSObject *aObj)
{
}

/**
  * Finds text in content
 */
NS_IMETHODIMP nsDocument::FindNext(const nsAReadableString& aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound)
{
  aIsFound = PR_FALSE;
  return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsDocument::InitDiskDocument(nsIFile *aFile)
{
  // aFile may be nsnull here
  mFileSpec = nsnull;   // delete if we have one
 
  if (aFile)
  {
    // we clone the nsIFile here, rather than just holding onto a ref,
    // in case the caller does something to aFile later
    nsresult rv = aFile->Clone(getter_AddRefs(mFileSpec));
    if (NS_FAILED(rv)) return rv;
  }
  
  mModCount = 0;
	return NS_OK;
}



NS_IMETHODIMP
nsDocument::SaveFile( nsIFile*          aFile,
                      PRBool            aReplaceExisting,
                      PRBool            aSaveCopy,
                      const PRUnichar*  aFileType,     // MIME type of file to save
                      const PRUnichar*  aFileCharset,
                      PRUint32          aSaveFlags,
                      PRUint32          aWrapColumn)
{
  NS_ENSURE_ARG_POINTER(aFile);
  NS_ENSURE_ARG_POINTER(aFileType);
  NS_ENSURE_ARG_POINTER(aFileCharset);
    
  nsresult  rv = NS_OK;

  // if we're not replacing an existing file but the file
  // exists, somethine is wrong
  PRBool  fileExists;
  rv = aFile->Exists(&fileExists);
  if (NS_FAILED(rv)) return rv;
  
  if (!aReplaceExisting && fileExists)
    return NS_ERROR_FAILURE;				// where are the file I/O errors?
  
  
  nsCOMPtr<nsIFileOutputStream> outputStream(do_CreateInstance(NS_LOCALFILEOUTPUTSTREAM_CONTRACTID, &rv));
  if (NS_FAILED(rv)) return rv;
  
  rv = outputStream->Init(aFile, -1, -1);
  if (NS_FAILED(rv)) return rv;
    
  // Get a document encoder instance
  nsCAutoString contractID(NS_DOC_ENCODER_CONTRACTID_BASE);
  contractID.AppendWithConversion(aFileType);
  
  nsCOMPtr<nsIDocumentEncoder> encoder(do_CreateInstance(contractID.get(), &rv));
  if (NS_FAILED(rv))
    return rv;
  
  nsAutoString fileType(aFileType);   // sucky copy
  rv = encoder->Init(this, fileType, aSaveFlags);
  if (NS_FAILED(rv))
    return rv;

  if (aSaveFlags & nsIDocumentEncoder::OutputWrap)
    encoder->SetWrapColumn(aWrapColumn);

  nsAutoString charsetStr(aFileCharset);
  if (charsetStr.Length() == 0)
  {
	  rv = GetDocumentCharacterSet(charsetStr);
	  if(NS_FAILED(rv)) {
	     charsetStr.AssignWithConversion("ISO-8859-1"); 
	  }
  }
  encoder->SetCharset(charsetStr);

  rv = encoder->EncodeToStream(outputStream);

  if (NS_SUCCEEDED(rv))
  {
    // if everything went OK and we're not just saving off a copy,
    // store the new fileSpec in the doc
    if (!aSaveCopy)
    {
      // we clone the nsIFile here, rather than just holding onto a ref,
      // in case the caller does something to aFile later
      rv = aFile->Clone(getter_AddRefs(mFileSpec));

      if (NS_SUCCEEDED(rv))
      {
        // and mark the document as clean
        ResetModificationCount();
      }
    }
  }
  
  return rv;
}

NS_IMETHODIMP
nsDocument::GetFileSpec(nsIFile * *aFileSpec)
{
  NS_ENSURE_ARG_POINTER(aFileSpec);
  
  if (!mFileSpec)
    return NS_ERROR_NOT_INITIALIZED;
    
  NS_IF_ADDREF(*aFileSpec = mFileSpec);
  return NS_OK;
}

NS_IMETHODIMP 
nsDocument::FlushPendingNotifications()
{
  PRInt32 i, count = mPresShells.Count();

  for (i = 0; i < count; i++) {
    nsIPresShell* shell = NS_STATIC_CAST(nsIPresShell*, mPresShells[i]);
    if (shell) {
      shell->FlushPendingNotifications();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetAndIncrementContentID(PRInt32* aID)
{
  *aID = mNextContentID++;
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetBindingManager(nsIBindingManager** aResult)
{
  nsresult rv;
  if (!mBindingManager) {
    mBindingManager = do_CreateInstance("@mozilla.org/xbl/binding-manager;1", &rv);
    if (NS_FAILED(rv))
      return NS_ERROR_FAILURE;
  }

  *aResult = mBindingManager;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}


NS_IMETHODIMP
nsDocument::GetModificationCount(PRInt32 *outModCount)
{
  if (!outModCount)
  	return NS_ERROR_NULL_POINTER;
  	
 *outModCount = mModCount;
 return NS_OK;
}


NS_IMETHODIMP
nsDocument::ResetModificationCount()
{
  mModCount = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::IncrementModificationCount(PRInt32 aNumMods)
{
  mModCount += aNumMods;
  //NS_ASSERTION(mModCount >= 0, "Modification count went negative");
  return NS_OK;
}


NS_IMETHODIMP
nsDocument::GetNodeInfoManager(nsINodeInfoManager*& aNodeInfoManager)
{
  NS_ENSURE_TRUE(mNodeInfoManager, NS_ERROR_NOT_INITIALIZED);

  aNodeInfoManager = mNodeInfoManager;
  NS_ADDREF(aNodeInfoManager);

  return NS_OK;
}


//
// FindContent does a depth-first search from aStartNode
// and returns the first of aTest1 or aTest2 which it finds.
// I think.
//
nsIContent* nsDocument::FindContent(const nsIContent* aStartNode,
                                    const nsIContent* aTest1, 
                                    const nsIContent* aTest2) const
{
  PRInt32       count;
  aStartNode->ChildCount(count);
  PRInt32       index;

  for(index = 0; index < count;index++)
  {
    nsIContent* child;
    aStartNode->ChildAt(index, child);
    nsIContent* content = FindContent(child,aTest1,aTest2);
    if (content != nsnull) {
      NS_IF_RELEASE(child);
      return content;
    }
    if (child == aTest1 || child == aTest2) {
      NS_IF_RELEASE(content);
      return child;
    }
    NS_IF_RELEASE(child);
    NS_IF_RELEASE(content);
  }
  return nsnull;
}


NS_IMETHODIMP    
nsDocument::GetDTD(nsIDTD** aDTD) const
{
  if (!aDTD)
    return NS_ERROR_INVALID_ARG;
  if (!mDTD)
  {
    nsCOMPtr<nsIDOMDocumentType> doctype;
    // Wish for mutable:
    nsresult rv = NS_CONST_CAST(nsDocument* , this)->GetDoctype(getter_AddRefs(doctype));
    if (NS_FAILED(rv)) return rv;
    if (!doctype) return NS_ERROR_FAILURE;
    nsAutoString doctypename;
    rv = doctype->GetName(doctypename);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIParser> parser( do_CreateInstance(kCParserCID, &rv) );
    if (NS_FAILED(rv)) return rv;
    if (!parser) return NS_ERROR_FAILURE;

    nsIDTD* dtd = 0;
    rv = parser->CreateCompatibleDTD(&dtd, &doctypename, eViewNormal,
                                     0, eDTDMode_unknown);
    if (NS_FAILED(rv)) return rv;
    if (!dtd) return NS_ERROR_FAILURE;

    // Wish again for mutable:
    NS_CONST_CAST(nsDocument* , this)->mDTD = dtd;
  }

  NS_ADDREF(mDTD);
  *aDTD = mDTD;
  return NS_OK;
}



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
#include "nsIDocumentObserver.h"
#include "nsEventListenerManager.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptEventListener.h"
#include "nsDOMEvent.h"
#include "nsDOMEventsIIDs.h"
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
//#include "nsIXIFConverter.h"
#include "nsIHTMLContentSink.h"
//#include "nsHTMLContentSinkStream.h"
//#include "nsHTMLToTXTSinkStream.h"
//#include "nsXIFDTD.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsFileSpec.h"
#include "nsFileStream.h"

#include "nsRange.h"
#include "nsIDOMText.h"
#include "nsIDOMComment.h"
#include "nsDOMDocumentType.h"

#include "nsINameSpaceManager.h"
#include "nsIServiceManager.h"

#include "nsLayoutAtoms.h"
#include "nsLayoutCID.h"
#include "nsIDOMSelection.h"
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
#include "nsIDOMWindow.h"

#include "nsIDOMElement.h"
#include "nsIAnonymousContentCreator.h"

static NS_DEFINE_CID(kXIFConverterCID, NS_XIFCONVERTER_CID);
static NS_DEFINE_CID(kCRangeCID, NS_RANGE_CID);
static NS_DEFINE_CID(kDOMScriptObjectFactoryCID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);

#include "nsILineBreakerFactory.h"
#include "nsIWordBreakerFactory.h"
#include "nsLWBrkCIID.h"
static NS_DEFINE_CID(kLWBrkCID, NS_LWBRK_CID);

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
  NS_IMETHOD    HasFeature(const nsString& aFeature, 
                           const nsString& aVersion, 
                           PRBool* aReturn);
  NS_IMETHOD    CreateDocumentType(const nsString& aQualifiedName,
                                   const nsString& aPublicId,
                                   const nsString& aSystemId,
                                   nsIDOMDocumentType** aReturn);

  NS_IMETHOD    CreateDocument(const nsString& aNamespaceURI,
                               const nsString& aQualifiedName,
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
nsDOMImplementation::HasFeature(const nsString& aFeature, 
                                const nsString& aVersion, 
                                PRBool* aReturn)
{
  return nsGenericElement::InternalSupports(aFeature, aVersion, aReturn);
}

NS_IMETHODIMP
nsDOMImplementation::CreateDocumentType(const nsString& aQualifiedName,
                                        const nsString& aPublicId, 
                                        const nsString& aSystemId, 
                                        nsIDOMDocumentType** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  return NS_NewDOMDocumentType(aReturn, aQualifiedName, nsnull, nsnull,
                               aPublicId, aSystemId, nsAutoString());
}

NS_IMETHODIMP
nsDOMImplementation::CreateDocument(const nsString& aNamespaceURI, 
                                    const nsString& aQualifiedName, 
                                    nsIDOMDocumentType* aDoctype, 
                                    nsIDOMDocument** aReturn)
{  
  NS_ENSURE_ARG_POINTER(aReturn);    
  
  nsresult rv = NS_OK;
  *aReturn = nsnull;

  NS_NewDOMDocument(aReturn, aNamespaceURI, aQualifiedName, aDoctype, mBaseURI);

  return rv;
}

NS_IMETHODIMP 
nsDOMImplementation::GetScriptObject(nsIScriptContext *aContext, 
                                     void** aScriptObject)
{
  nsresult result = NS_OK;

  if (nsnull == mScriptObject) {
    NS_WITH_SERVICE(nsIDOMScriptObjectFactory, factory, 
                    kDOMScriptObjectFactoryCID, &result);

    if (NS_OK == result) {
      nsIScriptGlobalObject *global = aContext->GetGlobalObject();

      result = factory->NewScriptDOMImplementation(aContext, (nsISupports*)(nsIDOMDOMImplementation*)this,
                                                   global, &mScriptObject);
      NS_RELEASE(global);
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

MOZ_DECL_CTOR_COUNTER(nsDocumentChildNodes);

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
  nsIContent* content = nsnull;

  *aReturn = nsnull;
  if (nsnull != mDocument) {
    result = mDocument->ChildAt(aIndex, content);
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

MOZ_DECL_CTOR_COUNTER(nsAnonymousContentList);

nsAnonymousContentList::nsAnonymousContentList(nsISupportsArray* aElements)
{
  MOZ_COUNT_CTOR(nsAnonymousContentList);

  // We don't reference count our Anonymous reference (to avoid circular
  // references). We'll be told when the Anonymous goes away.
  mElements = aElements;
  NS_IF_ADDREF(mElements);
}
 
nsAnonymousContentList::~nsAnonymousContentList()
{
  MOZ_COUNT_DTOR(nsAnonymousContentList);
  NS_IF_RELEASE(mElements);
}

NS_IMETHODIMP
nsAnonymousContentList::GetLength(PRUint32* aLength)
{
  NS_ASSERTION(aLength != nsnull, "null ptr");
  if (! aLength)
      return NS_ERROR_NULL_POINTER;

  PRUint32 cnt;
  nsresult rv = mElements->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  *aLength = cnt;
  return NS_OK;
}

NS_IMETHODIMP    
nsAnonymousContentList::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  PRUint32 cnt;
  nsresult rv = mElements->Count(&cnt);
  if (NS_FAILED(rv)) return rv;

  if (aIndex >= (PRUint32) cnt)
      return NS_ERROR_INVALID_ARG;

  // Cast is okay because we're in a closed system.
  *aReturn = (nsIDOMNode*) mElements->ElementAt(aIndex);
  return NS_OK;
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
  mDisplaySelection = PR_FALSE;
  mInDestructor = PR_FALSE;
  mDOMStyleSheets = nsnull;
  mNameSpaceManager = nsnull;
  mHeaderData = nsnull;
  mLineBreaker = nsnull;
  mProlog = nsnull;
  mEpilog = nsnull;
  mChildNodes = nsnull;
  mWordBreaker = nsnull;
  mModCount = 0;
  mFileSpec = nsnull;
  mPrincipal = nsnull;
  mNextContentID = NS_CONTENT_ID_COUNTER_BASE;
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
  PRInt32 index, count;
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

  NS_IF_RELEASE(mRootContent);

  // Delete references to style sheets
  index = mStyleSheets.Count();
  while (--index >= 0) {
    nsIStyleSheet* sheet = (nsIStyleSheet*) mStyleSheets.ElementAt(index);
    sheet->SetOwningDocument(nsnull);
    NS_RELEASE(sheet);
  }

  nsIContent* content;
  if (nsnull != mProlog) {
    count = mProlog->Count();
    for (index = 0; index < count; index++) {
      content = (nsIContent*)mProlog->ElementAt(index);
      NS_RELEASE(content);
    }
    delete mProlog;
  }
  if (nsnull != mEpilog) {
    count = mEpilog->Count();
    for (index = 0; index < count; index++) {
      content = (nsIContent*)mEpilog->ElementAt(index);
      NS_RELEASE(content);
    }
    delete mEpilog;
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
  NS_IF_RELEASE(mLineBreaker);
  NS_IF_RELEASE(mWordBreaker);
  
	delete mFileSpec;
	
}

nsresult nsDocument::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(NS_GET_IID(nsIDocument))) {
    nsIDocument* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMDocument))) {
    nsIDOMDocument* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMNSDocument))) {
    nsIDOMNSDocument* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMDocumentEvent))) {
    nsIDOMDocumentEvent* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMDocumentStyle))) {
    nsIDOMDocumentStyle* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMDocumentView))) {
    nsIDOMDocumentView* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMDocumentXBL))) {
    nsIDOMDocumentView* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIScriptObjectOwner))) {
    nsIScriptObjectOwner* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIJSScriptObject))) {
    nsIJSScriptObject* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIScriptObjectPrincipal))) {
    nsIScriptObjectPrincipal* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMEventReceiver))) {
    nsIDOMEventReceiver* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMEventTarget))) {
    nsIDOMEventTarget* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMNode))) {
    nsIDOMNode* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIDiskDocument))) {
    nsIDiskDocument* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsISupportsWeakReference))) {
    nsISupportsWeakReference* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsISupports))) {
    nsIDocument* tmp = this;
    nsISupports* tmp2 = tmp;
    *aInstancePtr = (void*) tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsDocument)
NS_IMPL_RELEASE(nsDocument)

nsresult nsDocument::Init()
{
  if (mNameSpaceManager) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  nsresult rv = NS_NewHeapArena(&mArena, nsnull);

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

  nsIContent* content;
  PRInt32 count;
  if (nsnull != mProlog) {
    count = mProlog->Count();
    for (index = 0; index < count; index++) {
      content = (nsIContent*)mProlog->ElementAt(index);
      content->SetDocument(nsnull, PR_TRUE, PR_TRUE);
      ContentRemoved(nsnull, content, index);
      NS_RELEASE(content);
    }
    delete mProlog;
    mProlog = nsnull;
  }
  if (nsnull != mRootContent) {
    // Ensure that document is nsnull to allow validity checks on content
    mRootContent->SetDocument(nsnull, PR_TRUE, PR_TRUE);
    ContentRemoved(nsnull, mRootContent, 0);
    NS_IF_RELEASE(mRootContent);
  }
  if (nsnull != mEpilog) {
    count = mEpilog->Count();
    for (index = 0; index < count; index++) {
      content = (nsIContent*)mEpilog->ElementAt(index);
      content->SetDocument(nsnull, PR_TRUE, PR_TRUE);
      ContentRemoved(nsnull, content, index);
      NS_RELEASE(content);
    }
    delete mEpilog;
    mEpilog = nsnull;
  }

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
    (void)aChannel->GetOriginalURI(&mDocumentURL);
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
                    NS_SCRIPTSECURITYMANAGER_PROGID, &rv);
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

  nsCOMPtr<nsIAggregatePrincipal> agg =
    do_QueryInterface(mPrincipal, &rv);
  if (NS_SUCCEEDED(rv))
  {
    rv = agg->Intersect(aNewPrincipal);
    if (NS_FAILED(rv)) return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsDocument::GetContentType(nsString& aContentType) const
{
  // Must be implemented by derived class.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocument::GetDocumentLoadGroup(nsILoadGroup **aGroup) const
{
  nsCOMPtr<nsILoadGroup> group = do_QueryReferent(mDocumentLoadGroup);

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

NS_IMETHODIMP nsDocument::GetDocumentCharacterSet(nsString& oCharSetID) 
{
  oCharSetID = mCharacterSet;
  return NS_OK;
}

NS_IMETHODIMP nsDocument::SetDocumentCharacterSet(const nsString& aCharSetID)
{
  if (mCharacterSet != aCharSetID) {
    mCharacterSet = aCharSetID;
    nsAutoString charSetTopic;
    charSetTopic.AssignWithConversion("charset");
    PRInt32 n = mCharSetObservers.Count();
    for (PRInt32 i = 0; i < n; i++) {
      nsIObserver* observer = (nsIObserver*) mCharSetObservers.ElementAt(i);
      observer->Observe((nsIDocument*) this, charSetTopic.GetUnicode(),
                        aCharSetID.GetUnicode());
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
  if(nsnull == mLineBreaker ) {
     // no line breaker, find a default one
     nsILineBreakerFactory *lf;
     nsresult result;
     result = nsServiceManager::GetService(kLWBrkCID,
                                          NS_GET_IID(nsILineBreakerFactory),
                                          (nsISupports **)&lf);
     if (NS_SUCCEEDED(result)) {
      nsILineBreaker *lb = nsnull ;
      nsAutoString lbarg;
      result = lf->GetBreaker(lbarg, &lb);
      if(NS_SUCCEEDED(result)) {
         mLineBreaker = lb;
      }
      result = nsServiceManager::ReleaseService(kLWBrkCID, lf);
     }
  }
  *aResult = mLineBreaker;
  NS_IF_ADDREF(mLineBreaker);
  return NS_OK; // XXX we should do error handling here
}
NS_IMETHODIMP nsDocument::SetLineBreaker(nsILineBreaker* aLineBreaker) 
{
  NS_IF_RELEASE(mLineBreaker);
  mLineBreaker = aLineBreaker;
  NS_IF_ADDREF(mLineBreaker);
  return NS_OK;
}
NS_IMETHODIMP nsDocument::GetWordBreaker(nsIWordBreaker** aResult) 
{
  if(nsnull == mWordBreaker ) {
     // no line breaker, find a default one
     nsIWordBreakerFactory *lf;
     nsresult result;
     result = nsServiceManager::GetService(kLWBrkCID,
                                          NS_GET_IID(nsIWordBreakerFactory),
                                          (nsISupports **)&lf);
     if (NS_SUCCEEDED(result)) {
      nsIWordBreaker *lb = nsnull ;
      nsAutoString lbarg;
      result = lf->GetBreaker(lbarg, &lb);
      if(NS_SUCCEEDED(result)) {
         mWordBreaker = lb;
      }
      result = nsServiceManager::ReleaseService(kLWBrkCID, lf);
     }
  }
  *aResult = mWordBreaker;
  NS_IF_ADDREF(mWordBreaker);
  return NS_OK; // XXX we should do error handling here
}
NS_IMETHODIMP nsDocument::SetWordBreaker(nsIWordBreaker* aWordBreaker) 
{
  NS_IF_RELEASE(mWordBreaker);
  mWordBreaker = aWordBreaker;
  NS_IF_ADDREF(mWordBreaker);
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetHeaderData(nsIAtom* aHeaderField, nsString& aData) const
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
nsDocument::SetHeaderData(nsIAtom* aHeaderField, const nsString& aData)
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
  NS_IF_RELEASE(mRootContent);
  if (nsnull != aRoot) {
    mRootContent = aRoot;
    NS_ADDREF(aRoot);
  }
}

NS_IMETHODIMP 
nsDocument::AppendToProlog(nsIContent* aContent)
{
  if (nsnull == mProlog) {
    mProlog = new nsVoidArray();
  }

  mProlog->AppendElement((void *)aContent);
  NS_ADDREF(aContent);

  return NS_OK;
}

NS_IMETHODIMP 
nsDocument::AppendToEpilog(nsIContent* aContent)
{
  if (nsnull == mEpilog) {
    mEpilog = new nsVoidArray();
  }

  mEpilog->AppendElement((void *)aContent);
  NS_ADDREF(aContent);

  return NS_OK;
}

NS_IMETHODIMP 
nsDocument::ChildAt(PRInt32 aIndex, nsIContent*& aResult) const
{
  nsIContent* content = nsnull;
  PRInt32 prolog = 0;

  if (nsnull != mProlog) {
    prolog = mProlog->Count();
    if (aIndex < prolog) {
      // It's in the prolog
      content = (nsIContent*)mProlog->ElementAt(aIndex);
    }
  }
  
  if ((aIndex == prolog) && mRootContent) {
    // It's the document element
    content = mRootContent;
  }
  else {
    prolog += mRootContent?1:0;
    if ((aIndex >= prolog) && (nsnull != mEpilog)) {
      // It's in the epilog
      content = (nsIContent*)mEpilog->ElementAt(aIndex-prolog);
    }
  }

  NS_IF_ADDREF(content);
  aResult = content;

  return NS_OK;
}

NS_IMETHODIMP 
nsDocument::IndexOf(nsIContent* aPossibleChild, PRInt32& aIndex) const
{
  PRInt32 index = -1;
  PRInt32 prolog = 0;

  if (nsnull != mProlog) {
    index = mProlog->IndexOf(aPossibleChild);
    prolog = mProlog->Count();
  }

  if (-1 == index) {
    if (aPossibleChild == mRootContent) {
      index = prolog;
    }
    else if (nsnull != mEpilog) {
      index = mEpilog->IndexOf(aPossibleChild);
      if (-1 != index) {
        index += (prolog+1);
      }
    }
  }
  
  aIndex = index;
  
  return NS_OK;
}

NS_IMETHODIMP 
nsDocument::GetChildCount(PRInt32& aCount)
{
  aCount = 0;
  if (nsnull != mRootContent) {
    aCount++;
  }
  if (nsnull != mProlog) {
    aCount += mProlog->Count();
  }
  if (nsnull != mEpilog) {
    aCount += mEpilog->Count();
  }

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
    PRInt32 count, index;
    nsIContent *content;
    if (mProlog) {
      count = mProlog->Count();
      for (index = 0; index < count; index++) {
        content = (nsIContent*)mProlog->ElementAt(index);
        content->SetDocument(nsnull, PR_TRUE, PR_TRUE);
      }
    }
    if (mRootContent) {
      mRootContent->SetDocument(nsnull, PR_TRUE, PR_TRUE);
    }
    if (mEpilog) {
      count = mEpilog->Count();
      for (index = 0; index < count; index++) {
        content = (nsIContent*)mEpilog->ElementAt(index);
        content->SetDocument(nsnull, PR_TRUE, PR_TRUE);
      }
    }

    // XXX You thought that was a hack?  Let's unroot the scrollbars
    // around the root element.  The frame that created the anonymous
    // content for them isn't the primary frame for any element, so
    // we do it here.  This tries not to have too much knowledge of
    // how scrollbars are implemented today by actually checking all
    // the frames up to the top.
    nsCOMPtr<nsIPresShell> shell( dont_AddRef(GetShellAt(0)) );
    if (shell) {
      nsIFrame *kidFrame = nsnull;
      shell->GetPrimaryFrameFor(mRootContent, &kidFrame);
      while (kidFrame) {
        // XXX Don't release a frame!
        nsCOMPtr<nsIAnonymousContentCreator> acc( do_QueryInterface(kidFrame) );
        if (acc) {
          acc->SetDocumentForAnonymousContent(nsnull, PR_TRUE, PR_TRUE);
        }
        nsIFrame *parentFrame;
        kidFrame->GetParent(&parentFrame);
        kidFrame = parentFrame;
      }
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

  if (mProlog) {
    PRInt32 i, count = mProlog->Count();

    for (i = 0; i < count; i++) {
      nsIContent* content = (nsIContent *)mProlog->ElementAt(i);

      if (!content)
        continue;

      nsCOMPtr<nsIDOMNode> node(do_QueryInterface(content));

      if (node) {
        PRUint16 nodeType;

        node->GetNodeType(&nodeType);

        if (nodeType == nsIDOMNode::DOCUMENT_TYPE_NODE) {
          return node->QueryInterface(NS_GET_IID(nsIDOMDocumentType),
                                      (void **)aDoctype);
        }
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
nsDocument::CreateElement(const nsString& aTagName, 
                          nsIDOMElement** aReturn)
{
  // Should be implemented by subclass
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDocument::CreateTextNode(const nsString& aData, nsIDOMText** aReturn)
{
  nsIContent* text = nsnull;
  nsresult        rv = NS_NewTextNode(&text);

  if (NS_OK == rv) {
    rv = text->QueryInterface(NS_GET_IID(nsIDOMText), (void**)aReturn);
    (*aReturn)->AppendData(aData);
    NS_RELEASE(text);
  }

  return rv;
}

NS_IMETHODIMP    
nsDocument::CreateDocumentFragment(nsIDOMDocumentFragment** aReturn)
{
  return NS_NewDocumentFragment(aReturn, this);
}

NS_IMETHODIMP    
nsDocument::CreateComment(const nsString& aData, nsIDOMComment** aReturn)
{
  nsIContent* comment = nsnull;
  nsresult        rv = NS_NewCommentNode(&comment);

  if (NS_OK == rv) {
    rv = comment->QueryInterface(NS_GET_IID(nsIDOMComment), (void**)aReturn);
    (*aReturn)->AppendData(aData);
    NS_RELEASE(comment);
  }

  return rv;
}

NS_IMETHODIMP 
nsDocument::CreateCDATASection(const nsString& aData, nsIDOMCDATASection** aReturn)
{
  // Should be implemented by subclass
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::CreateProcessingInstruction(const nsString& aTarget, 
                                        const nsString& aData, 
                                        nsIDOMProcessingInstruction** aReturn)
{
  // Should be implemented by subclass
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::CreateAttribute(const nsString& aName, 
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
nsDocument::CreateEntityReference(const nsString& aName, 
                                  nsIDOMEntityReference** aReturn)
{
  // Should be implemented by subclass
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::GetElementsByTagName(const nsString& aTagname, 
                                 nsIDOMNodeList** aReturn)
{
  nsCOMPtr<nsIAtom> nameAtom = dont_AddRef(NS_NewAtom(aTagname));

  nsContentList* list = new nsContentList(this, nameAtom, kNameSpaceID_Unknown);
  NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);

  return list->QueryInterface(NS_GET_IID(nsIDOMNodeList), (void **)aReturn);
}

NS_IMETHODIMP    
nsDocument::GetElementsByTagNameNS(const nsString& aNamespaceURI,
                                   const nsString& aLocalName,
                                   nsIDOMNodeList** aReturn)
{
  nsCOMPtr<nsIAtom> nameAtom;
  PRInt32 nameSpaceId = kNameSpaceID_Unknown;

  nameAtom = dont_AddRef(NS_NewAtom(aLocalName));

  nsContentList* list = nsnull;

  if (!aNamespaceURI.EqualsWithConversion("*")) {
    mNameSpaceManager->GetNameSpaceID(aNamespaceURI, nameSpaceId);

    if (nameSpaceId == kNameSpaceID_Unknown) {
      // Unkonwn namespace means no matches, we create an empty list...
      list = new nsContentList(this, nsnull, kNameSpaceID_None);
      NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);
    }
  }

  if (!list) {
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
nsDocument::GetCharacterSet(nsString& aCharacterSet)
{
  return GetDocumentCharacterSet(aCharacterSet);
}

NS_IMETHODIMP    
nsDocument::CreateElementWithNameSpace(const nsString& aTagName, 
                                       const nsString& aNameSpace, 
                                       nsIDOMElement** aReturn)
{
  *aReturn = nsnull;
  return NS_ERROR_NOT_IMPLEMENTED;
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
nsDocument::AddBinding(nsIDOMElement* aContent, const nsString& aURL)
{
  nsCOMPtr<nsIBindingManager> bm;
  GetBindingManager(getter_AddRefs(bm));
  nsCOMPtr<nsIContent> content(do_QueryInterface(aContent));

  return bm->AddLayeredBinding(content, aURL);
}

NS_IMETHODIMP
nsDocument::RemoveBinding(nsIDOMElement* aContent, const nsString& aURL)
{
  if (mBindingManager) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(aContent));
    return mBindingManager->RemoveLayeredBinding(content, aURL);
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDocument::LoadBindingDocument(const nsString& aURL)
{
  if (mBindingManager)
    return mBindingManager->LoadBindingDocument(this, aURL);
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDocument::GetAnonymousNodes(nsIDOMElement* aElement, nsIDOMNodeList** aResult)
{
  nsresult rv;
  *aResult = nsnull;

  // Use the XBL service to get a content list.
  NS_WITH_SERVICE(nsIXBLService, xblService, "component://netscape/xbl", &rv);
  if (!xblService)
    return rv;

  // Retrieve the anonymous content that we should build.
  nsCOMPtr<nsISupportsArray> anonymousItems;
  nsCOMPtr<nsIContent> dummyElt;
  nsCOMPtr<nsIContent> element(do_QueryInterface(aElement));
  if (!element)
    return rv;

  PRBool dummy;
  xblService->GetContentList(element, getter_AddRefs(anonymousItems), 
                             getter_AddRefs(dummyElt), &dummy);
  
  if (!anonymousItems)
    return NS_OK;

  nsCOMPtr<nsISupportsArray> elements;
  NS_NewISupportsArray(getter_AddRefs(elements));

  PRUint32 count = 0;
  anonymousItems->Count(&count);

  for (PRUint32 i=0; i < count; i++)
  {
    // get our child's content and set its parent to our content
    nsCOMPtr<nsISupports> node;
    anonymousItems->GetElementAt(i,getter_AddRefs(node));

    nsCOMPtr<nsIDOMNode> content(do_QueryInterface(node));
    
    if (content)
      elements->AppendElement(content);
  }

  nsAnonymousContentList* elts = new nsAnonymousContentList(elements);
  NS_IF_ADDREF(elts);
  *aResult = elts;

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

  nsCOMPtr<nsIDOMWindow> window;
  ifrq->GetInterface(NS_GET_IID(nsIDOMWindow), getter_AddRefs(window));
  NS_ENSURE_TRUE(window, NS_OK);

  window->QueryInterface(NS_GET_IID(nsIDOMAbstractView),
                         (void **)aDefaultView);

  return NS_OK;
}


NS_IMETHODIMP
nsDocument::Load (const nsString& aUrl)
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

  nsCOMPtr<nsIDOMWindow> window;
  ifrq->GetInterface(NS_GET_IID(nsIDOMWindow), getter_AddRefs(window));
  NS_ENSURE_TRUE(window, NS_OK);

  nsCOMPtr<nsIDOMNavigator> navigator;
  window->GetNavigator(getter_AddRefs(navigator));
  NS_ENSURE_TRUE(navigator, NS_OK);

  return navigator->GetPlugins(aPlugins);
}

//
// nsIDOMNode methods
//
NS_IMETHODIMP    
nsDocument::GetNodeName(nsString& aNodeName)
{
  aNodeName.AssignWithConversion("#document");
  return NS_OK;
}

NS_IMETHODIMP    
nsDocument::GetNodeValue(nsString& aNodeValue)
{
  aNodeValue.Truncate();
  return NS_OK;
}

NS_IMETHODIMP    
nsDocument::SetNodeValue(const nsString& aNodeValue)
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

  *aHasChildNodes =  (((nsnull != mProlog) && (0 != mProlog->Count())) ||
                      (nsnull != mRootContent) ||
                      ((nsnull != mEpilog) && (0 != mEpilog->Count())));
  return NS_OK;
}

NS_IMETHODIMP    
nsDocument::GetFirstChild(nsIDOMNode** aFirstChild)
{
  nsresult result = NS_OK;

  *aFirstChild = nsnull;
  if ((nsnull != mProlog) && (0 != mProlog->Count())) {
    nsIContent* content;
    content = (nsIContent *)mProlog->ElementAt(0);

    if (nsnull != content) {
      result = content->QueryInterface(NS_GET_IID(nsIDOMNode), (void**)aFirstChild);
    }
  }
  else {
    nsIDOMElement* element;
    result = GetDocumentElement(&element);
    if ((NS_OK == result) && element) {
      result = element->QueryInterface(NS_GET_IID(nsIDOMNode), (void**)aFirstChild);
      NS_RELEASE(element);
    }
  }

  return result;
}

NS_IMETHODIMP    
nsDocument::GetLastChild(nsIDOMNode** aLastChild)
{
  nsresult result = NS_OK;

  if ((nsnull != mEpilog) && (0 != mEpilog->Count())) {
    nsIContent* content;
    content = (nsIContent *)mEpilog->ElementAt(mEpilog->Count()-1);
    if (nsnull != content) {
      result = content->QueryInterface(NS_GET_IID(nsIDOMNode), (void**)aLastChild);
    }
  }
  else {
    nsIDOMElement* element;
    result = GetDocumentElement(&element);
    if ((NS_OK == result) && element) {
      result = element->QueryInterface(NS_GET_IID(nsIDOMNode), (void**)aLastChild);
      NS_RELEASE(element);
    }
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
nsDocument::GetNamespaceURI(nsString& aNamespaceURI)
{ 
  aNamespaceURI.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::GetPrefix(nsString& aPrefix)
{
  aPrefix.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::SetPrefix(const nsString& aPrefix)
{
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsDocument::GetLocalName(nsString& aLocalName)
{
  aLocalName.Truncate();
  return NS_OK;
}

NS_IMETHODIMP    
nsDocument::InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn)
{
  NS_ASSERTION(nsnull != aNewChild, "null ptr");
  nsresult result = NS_OK;
  PRInt32 index;
  PRUint16 nodeType;
  nsIContent *content, *refContent = nsnull;

  if (nsnull == aNewChild) {
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

  result = aNewChild->QueryInterface(NS_GET_IID(nsIContent), (void**)&content);
  if (NS_OK != result) {
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }

  if (ELEMENT_NODE == nodeType) {
    if (mRootContent) {
      return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
    }
    else {
      SetRootContent(content);
      ContentInserted(nsnull, content, 0);
    }
  }
  else if (nsnull == aRefChild) {
    if ((!mProlog || (mProlog && mProlog->Count())) && mRootContent) {
      AppendToEpilog(content);
    } else if (nodeType != ELEMENT_NODE) {
      AppendToProlog(content);
    } else {
      return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
    }
  }
  else {
    result = aRefChild->QueryInterface(NS_GET_IID(nsIContent), (void**)&refContent);
    if (NS_OK != result) {
      NS_RELEASE(content);
      return NS_ERROR_DOM_NOT_FOUND_ERR;
    }

    if ((nsnull != mProlog) && (0 != mProlog->Count())) {
      index = mProlog->IndexOf(refContent);
      if (-1 != index) {
        mProlog->InsertElementAt(content, index);
        NS_ADDREF(content);
      }
    }

    if (refContent == mRootContent) {
      AppendToProlog(content);
    }
    else if ((nsnull != mEpilog) && (0 != mEpilog->Count())) {
      index = mEpilog->IndexOf(refContent);
      if (-1 != index) {
        mEpilog->InsertElementAt(content, index);
        NS_ADDREF(content);
      }
    }
    NS_RELEASE(refContent);
  }

  if (NS_OK == result) {
    content->SetDocument(this, PR_TRUE, PR_TRUE);
    *aReturn = aNewChild;
    NS_ADDREF(aNewChild);
  }
  else {
    *aReturn = nsnull;
  }

  NS_RELEASE(content);  

  return result;
}

NS_IMETHODIMP    
nsDocument::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  NS_ASSERTION(((nsnull != aNewChild) && (nsnull != aOldChild)), "null ptr");
  nsresult result = NS_OK;
  PRInt32 index;
  PRUint16 nodeType;
  nsIContent *content, *refContent;
  PRBool found = PR_FALSE;
  
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

  result = aNewChild->QueryInterface(NS_GET_IID(nsIContent), (void**)&content);
  if (NS_OK != result) {
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }

  result = aOldChild->QueryInterface(NS_GET_IID(nsIContent), (void**)&refContent);
  if (NS_OK != result) {
    NS_RELEASE(content);
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }

  if ((nsnull != mProlog) && (0 != mProlog->Count())) {
    index = mProlog->IndexOf(refContent);
    if (-1 != index) {
      nsIContent* oldContent;
      oldContent = (nsIContent*)mProlog->ElementAt(index);
      oldContent->SetDocument(nsnull, PR_TRUE, PR_TRUE);
      ContentRemoved(nsnull, oldContent, index);
      NS_RELEASE(oldContent);
      mProlog->ReplaceElementAt(content, index);
      ContentInserted(nsnull, content, index);
      NS_ADDREF(content);
      found = PR_TRUE;
    }
  }

  if (!found && (refContent == mRootContent)) {
    if (ELEMENT_NODE == nodeType) {
      // Out with the old
      ContentRemoved(nsnull, mRootContent, mProlog->Count());
      
      // In with the new
      SetRootContent(content);
      ContentInserted(nsnull, content, mProlog->Count());
      found = PR_TRUE;
    }
    else {
      NS_RELEASE(refContent);
      NS_RELEASE(content);
      return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
    }
  }
  else if (!found && (nsnull != mEpilog) && (0 != mEpilog->Count())) {
    index = mEpilog->IndexOf(refContent);
    if (-1 != index) {
      nsIContent* oldContent;
      oldContent = (nsIContent*)mEpilog->ElementAt(index);
      ContentRemoved(nsnull, oldContent, mProlog->Count()+(mRootContent?1:0)+index);
      NS_RELEASE(oldContent);
      mEpilog->ReplaceElementAt(content, index);
      ContentInserted(nsnull, content, mProlog->Count()+(mRootContent?1:0)+index);
      NS_ADDREF(content);
      found = PR_TRUE;
    }
  }

  if (found) {
    content->SetDocument(this, PR_TRUE, PR_TRUE);
    refContent->SetDocument(nsnull, PR_TRUE, PR_TRUE);
    *aReturn = aOldChild;
    NS_ADDREF(aOldChild);
  }
  else {
    *aReturn = nsnull;
    result = NS_ERROR_DOM_NOT_FOUND_ERR;
  }

  NS_RELEASE(content);
  NS_RELEASE(refContent);

  return result;
}

NS_IMETHODIMP    
nsDocument::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  NS_ASSERTION(nsnull != aOldChild, "null ptr");
  nsresult result = NS_OK;
  PRInt32 index;
  nsIContent *content;
  PRBool found = PR_FALSE;
  
  if (nsnull == aOldChild) {
    return NS_ERROR_NULL_POINTER;
  }

  result = aOldChild->QueryInterface(NS_GET_IID(nsIContent), (void**)&content);
  if (NS_OK != result) {
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }

  if ((nsnull != mProlog) && (0 != mProlog->Count())) {
    index = mProlog->IndexOf(content);
    if (-1 != index) {
      // Don't drop reference count since we're going
      // to return this element anyway.
      ContentRemoved(nsnull, content, index);
      mProlog->RemoveElementAt(index);
      found = PR_TRUE;
    }
  }

  if (!found && (content == mRootContent)) {
    // Out with the old
    ContentRemoved(nsnull, mRootContent, mProlog->Count());
    NS_ADDREF(mRootContent); // for return, since next line releases
    SetRootContent(nsnull);
    found = PR_TRUE;
  }
  else if (!found && (nsnull != mEpilog) && (0 != mEpilog->Count())) {
    index = mEpilog->IndexOf(content);
    if (-1 != index) {
      // Don't drop reference count since we're going
      // to return this element anyway.
      ContentRemoved(nsnull, content, mProlog->Count()+(mRootContent?1:0)+index);
      mEpilog->RemoveElementAt(index);
      found = PR_TRUE;
    }
  }

  if (found) {
    content->SetDocument(nsnull, PR_TRUE, PR_TRUE);
    // The refcount return was AddRef'd / not released above.
    *aReturn = aOldChild;
    result = NS_OK;
  }
  else {
    *aReturn = nsnull;
    result = NS_ERROR_DOM_NOT_FOUND_ERR;
  }

  NS_RELEASE(content);
  
  return result;
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
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(mRootContent);

    if (node) {
      return node->Normalize();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::Supports(const nsString& aFeature, const nsString& aVersion,
                     PRBool* aReturn)
{
  return nsGenericElement::InternalSupports(aFeature, aVersion, aReturn);
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
  nsIEventListenerManager *manager;

  if (NS_OK == GetListenerManager(&manager)) {
    manager->AddEventListenerByIID(aListener, aIID, NS_EVENT_FLAG_BUBBLE);
    NS_RELEASE(manager);
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

nsresult nsDocument::AddEventListener(const nsString& aType, nsIDOMEventListener* aListener, 
                                      PRBool aUseCapture)
{
  nsIEventListenerManager *manager;

  if (NS_OK == GetListenerManager(&manager)) {
    PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

    manager->AddEventListenerByType(aListener, aType, flags);
    NS_RELEASE(manager);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult nsDocument::RemoveEventListener(const nsString& aType, nsIDOMEventListener* aListener, 
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

  nsCOMPtr<nsIPresShell> shell = getter_AddRefs(GetShellAt(0));
  
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
nsDocument::CreateEvent(const nsString& aEventType, nsIDOMEvent** aReturn)
{
  // Obtain a presentation context
  PRInt32 count = GetNumberOfShells();
  if (count == 0)
    return NS_OK;

  nsCOMPtr<nsIPresShell> shell = getter_AddRefs(GetShellAt(0));
  
  // Retrieve the context
  nsCOMPtr<nsIPresContext> presContext;
  shell->GetPresContext(getter_AddRefs(presContext));

  if (presContext) {
    nsCOMPtr<nsIEventListenerManager> lm;
    if (NS_SUCCEEDED(GetListenerManager(getter_AddRefs(lm)))) {
      return lm->CreateEvent(presContext, nsnull, aEventType, aReturn);
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
  PRBool result = PR_TRUE;

  if (JSVAL_IS_STRING(aID) && 
      PL_strcmp("location", JS_GetStringBytes(JS_ValueToString(aContext, aID))) == 0) {
    if (mScriptGlobalObject) {
      nsCOMPtr<nsIJSScriptObject> window(do_QueryInterface(mScriptGlobalObject));
      if(window) {
        result = window->GetProperty(aContext, aObj, aID, aVp);
      }
      else {
        result = PR_FALSE;
      }
    }
  }

  return result;
}

PRBool    nsDocument::SetProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp)
{
  PRBool result = PR_TRUE;

  if (JS_TypeOfValue(aContext, *aVp) == JSTYPE_FUNCTION && JSVAL_IS_STRING(aID)) {
    nsAutoString mPropName, mPrefix;
    mPropName.Assign(NS_REINTERPRET_CAST(const PRUnichar*, JS_GetStringChars(JS_ValueToString(aContext, aID))));
    if (mPropName.Length() > 2)
      mPrefix.Assign(mPropName.GetUnicode(), 2);
    if (mPrefix.EqualsWithConversion("on")) {
      nsCOMPtr<nsIAtom> atom = getter_AddRefs(NS_NewAtom(mPropName));
      nsIEventListenerManager *mManager = nsnull;

      if (atom.get() == nsLayoutAtoms::onmousedown || atom.get() == nsLayoutAtoms::onmouseup || atom.get() ==  nsLayoutAtoms::onclick ||
         atom.get() == nsLayoutAtoms::onmouseover || atom.get() == nsLayoutAtoms::onmouseout) {
        if (NS_OK == GetListenerManager(&mManager)) {
          nsCOMPtr<nsIScriptContext> mScriptCX;
          if(NS_FAILED(nsLayoutUtils::GetStaticScriptContext(aContext, (JSObject*)mScriptObject, getter_AddRefs(mScriptCX))) ||
             NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this, atom, NS_GET_IID(nsIDOMMouseListener))) {
            NS_RELEASE(mManager);
            return PR_FALSE;
          }
        }
      }
      else if (atom.get() == nsLayoutAtoms::onkeydown || atom.get() == nsLayoutAtoms::onkeyup || atom.get() == nsLayoutAtoms::onkeypress) {
        if (NS_OK == GetListenerManager(&mManager)) {
          nsCOMPtr<nsIScriptContext> mScriptCX;
          if(NS_FAILED(nsLayoutUtils::GetStaticScriptContext(aContext, (JSObject*)mScriptObject, getter_AddRefs(mScriptCX))) ||
             NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this, atom, NS_GET_IID(nsIDOMKeyListener))) {
            NS_RELEASE(mManager);
            return PR_FALSE;
          }
        }
      }
      else if (atom.get() == nsLayoutAtoms::onmousemove) {
        if (NS_OK == GetListenerManager(&mManager)) {
          nsCOMPtr<nsIScriptContext> mScriptCX;
          if(NS_FAILED(nsLayoutUtils::GetStaticScriptContext(aContext, (JSObject*)mScriptObject, getter_AddRefs(mScriptCX))) ||
             NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this, atom, NS_GET_IID(nsIDOMMouseMotionListener))) {
            NS_RELEASE(mManager);
            return PR_FALSE;
          }
        }
      }
      else if (atom.get() == nsLayoutAtoms::onfocus || atom.get() == nsLayoutAtoms::onblur) {
        if (NS_OK == GetListenerManager(&mManager)) {
          nsCOMPtr<nsIScriptContext> mScriptCX;
          if(NS_FAILED(nsLayoutUtils::GetStaticScriptContext(aContext, (JSObject*)mScriptObject, getter_AddRefs(mScriptCX))) ||
             NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this, atom, NS_GET_IID(nsIDOMFocusListener))) {
            NS_RELEASE(mManager);
            return PR_FALSE;
          }
        }
      }
      else if (atom.get() == nsLayoutAtoms::onsubmit || atom.get() == nsLayoutAtoms::onreset || atom.get() == nsLayoutAtoms::onchange ||
               atom.get() == nsLayoutAtoms::onselect) {
        if (NS_OK == GetListenerManager(&mManager)) {
          nsCOMPtr<nsIScriptContext> mScriptCX;
          if(NS_FAILED(nsLayoutUtils::GetStaticScriptContext(aContext, (JSObject*)mScriptObject, getter_AddRefs(mScriptCX))) ||
             NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this, atom, NS_GET_IID(nsIDOMFormListener))) {
            NS_RELEASE(mManager);
            return PR_FALSE;
          }
        }
      }
      else if (atom.get() == nsLayoutAtoms::onload || atom.get() == nsLayoutAtoms::onunload || atom.get() == nsLayoutAtoms::onabort ||
               atom.get() == nsLayoutAtoms::onerror) {
        if (NS_OK == GetListenerManager(&mManager)) {
          nsCOMPtr<nsIScriptContext> mScriptCX;
          if(NS_FAILED(nsLayoutUtils::GetStaticScriptContext(aContext, (JSObject*)mScriptObject, getter_AddRefs(mScriptCX))) ||
             NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this, atom, NS_GET_IID(nsIDOMLoadListener))) {
            NS_RELEASE(mManager);
            return PR_FALSE;
          }
        }
      }
      else if (atom.get() == nsLayoutAtoms::onpaint) {
        if (NS_OK == GetListenerManager(&mManager)) {
          nsCOMPtr<nsIScriptContext> mScriptCX;
          if(NS_FAILED(nsLayoutUtils::GetStaticScriptContext(aContext, (JSObject*)mScriptObject, getter_AddRefs(mScriptCX))) ||
             NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this,
                                                             atom, NS_GET_IID(nsIDOMPaintListener))) {
            NS_RELEASE(mManager);
            return PR_FALSE;
          }
        }
      }
      NS_IF_RELEASE(mManager);
    }
  }
  else if (JSVAL_IS_STRING(aID) && 
      PL_strcmp("location", JS_GetStringBytes(JS_ValueToString(aContext, aID))) == 0) {
    if (mScriptGlobalObject) {
      nsCOMPtr<nsIJSScriptObject> window(do_QueryInterface(mScriptGlobalObject));
      if(window) {
        result = window->SetProperty(aContext, aObj, aID, aVp);
      }
      else {
        result = PR_FALSE;
      }
    }
  }

  return result;
}

PRBool    nsDocument::EnumerateProperty(JSContext *aContext, JSObject *aObj)
{
  return PR_TRUE;
}

PRBool    nsDocument::Resolve(JSContext *aContext, JSObject *aObj, jsval aID)
{
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
NS_IMETHODIMP nsDocument::FindNext(const nsString &aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound)
{
  aIsFound = PR_FALSE;
  return NS_ERROR_FAILURE;
}



void nsDocument::BeginConvertToXIF(nsIXIFConverter *aConverter, nsIDOMNode* aNode)
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
  PRBool      isSynthetic = PR_TRUE;

  // Begin Conversion
  if (content) 
  {
    content->IsSynthetic(isSynthetic);
    if (PR_FALSE == isSynthetic)
    {
      content->BeginConvertToXIF(aConverter);
      content->ConvertContentToXIF(aConverter);
    }
  }
}

void nsDocument::ConvertChildrenToXIF(nsIXIFConverter * aConverter, nsIDOMNode* aNode)
{
  // Iterate through the children, convertion child nodes
  nsresult result = NS_OK;
  nsCOMPtr<nsIDOMNode> child;
  result = aNode->GetFirstChild(getter_AddRefs(child));
    
  while ((result == NS_OK) && (child != nsnull))
  { 
    nsCOMPtr<nsIDOMNode> temp(child);
    result=ToXIF(aConverter,child);    
    result = temp->GetNextSibling(getter_AddRefs(child));
  }
}

void nsDocument::FinishConvertToXIF(nsIXIFConverter* aConverter, nsIDOMNode* aNode)
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
  PRBool      isSynthetic = PR_TRUE;

  if (content) 
  {
    content->IsSynthetic(isSynthetic);
    if (PR_FALSE == isSynthetic)
      content->FinishConvertToXIF(aConverter);
  }
}


NS_IMETHODIMP
nsDocument::ToXIF(nsIXIFConverter* aConverter, nsIDOMNode* aNode)
{
  nsresult result=NS_OK;
  nsCOMPtr<nsIDOMSelection> sel;
  aConverter->GetSelection(getter_AddRefs(sel));
  if (sel)
  {
    nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));

    if (NS_SUCCEEDED(result) && content)
    {
      PRBool  isInSelection = IsInSelection(sel,content);

      if (isInSelection == PR_TRUE)
      {
        BeginConvertToXIF(aConverter,aNode);
        ConvertChildrenToXIF(aConverter,aNode);
        FinishConvertToXIF(aConverter,aNode);
      }
      else
      {
        ConvertChildrenToXIF(aConverter,aNode);
      }
    }
  }
  else
  {
    BeginConvertToXIF(aConverter,aNode);
    ConvertChildrenToXIF(aConverter,aNode);
    FinishConvertToXIF(aConverter,aNode);
  }
  return result;
} 

NS_IMETHODIMP
nsDocument::CreateXIF(nsString & aBuffer, nsIDOMSelection* aSelection)
{
    nsresult result=NS_OK;

    nsCOMPtr<nsIXIFConverter> converter;
    nsComponentManager::CreateInstance(kXIFConverterCID,
                             nsnull,
                             NS_GET_IID(nsIXIFConverter),
                             getter_AddRefs(converter));
    NS_ENSURE_TRUE(converter,NS_ERROR_FAILURE);
    converter->Init(aBuffer);

    if (aSelection)
      converter->SetSelection(aSelection);

    converter->AddStartTag( NS_ConvertToString("section") , PR_TRUE); 
    converter->AddStartTag( NS_ConvertToString("section_head") , PR_TRUE);

    converter->BeginStartTag( NS_ConvertToString("document_info") );
    converter->AddAttribute(NS_ConvertToString("charset"),mCharacterSet);
    nsCOMPtr<nsIURI> uri (getter_AddRefs(GetDocumentURL()));
    if (uri)
    {
      char* spec = 0;
      if (NS_SUCCEEDED(uri->GetSpec(&spec)) && spec)
      {
        converter->AddAttribute(NS_ConvertToString("uri"), NS_ConvertToString(spec));
        Recycle(spec);
      }
    }
    converter->FinishStartTag(NS_ConvertToString("document_info"),PR_TRUE,PR_TRUE);

    converter->AddEndTag(NS_ConvertToString("section_head"), PR_TRUE, PR_TRUE);
    converter->AddStartTag(NS_ConvertToString("section_body"), PR_TRUE);

    nsCOMPtr<nsIDOMDocumentType> doctype;
    GetDoctype(getter_AddRefs(doctype));
    if(doctype) {
      nsAutoString tmpStr, docTypeStr;

      doctype->GetName(tmpStr);

      if (tmpStr.Length()) {
        docTypeStr.AppendWithConversion("DOCTYPE ");
        docTypeStr.Append(tmpStr);

        doctype->GetPublicId(tmpStr);
        if (tmpStr.Length()) {
          docTypeStr.AppendWithConversion(" PUBLIC \"");
          docTypeStr.Append(tmpStr);
          docTypeStr.AppendWithConversion('"');
        }

        doctype->GetSystemId(tmpStr);
        if (tmpStr.Length()) {
          docTypeStr.AppendWithConversion(" SYSTEM \"");
          docTypeStr.Append(tmpStr);
          docTypeStr.AppendWithConversion('"');
        }

        doctype->GetInternalSubset(tmpStr);
        if (tmpStr.Length()) {
          docTypeStr.AppendWithConversion(" [\n");
          docTypeStr.Append(tmpStr);
          docTypeStr.AppendWithConversion("\n]");
        }
      }
      if (docTypeStr.Length())
        converter->AddMarkupDeclaration(docTypeStr);
    }

    nsCOMPtr<nsIDOMElement> rootElement;
    if (aSelection)
    {
      PRInt32 rangeCount;
      if (NS_SUCCEEDED(aSelection->GetRangeCount(&rangeCount)) && rangeCount == 1) //getter_AddRefs(node));
      {
        nsCOMPtr<nsIDOMNode> anchor;
        nsCOMPtr<nsIDOMNode> focus;
        if (NS_SUCCEEDED(aSelection->GetAnchorNode(getter_AddRefs(anchor))))
        {
          if (NS_SUCCEEDED(aSelection->GetFocusNode(getter_AddRefs(focus))))
          {
            //check to see if these are text nodes if so there is still a chance at optimization
            //from checking their respective parents
            if (focus.get() != anchor.get()) 
            {
              nsCOMPtr<nsIDOMText> domText(do_QueryInterface(focus));
              if (domText)
              {
                nsCOMPtr<nsIDOMNode> parent;
                result = focus->GetParentNode(getter_AddRefs(parent));
                if (NS_SUCCEEDED(result) && parent)
                  focus = parent;
              }
              domText = do_QueryInterface(anchor);
              if (domText)
              {
                nsCOMPtr<nsIDOMNode> parent;
                result = anchor->GetParentNode(getter_AddRefs(parent));
                if (NS_SUCCEEDED(result) && parent)
                  anchor = parent;
              }
            }//end parent checking

            if (focus.get() == anchor.get())
            {
              rootElement = do_QueryInterface(focus);//set root to top of selection
              if (!rootElement)//maybe its a text node since both are the same. both parents are the same. pick one
              {
                nsCOMPtr<nsIDOMNode> parent;
                anchor->GetParentNode(getter_AddRefs(parent));
                rootElement = do_QueryInterface(parent);//set root to top of selection
              }
            }
          }
        }
      }
    }
    if (!rootElement)
      result=GetDocumentElement(getter_AddRefs(rootElement));
    if (NS_SUCCEEDED(result) && rootElement)
    {  
  #if 1
      result=ToXIF(converter,rootElement);
  #else
     if(NS_SUCCEEDED(rv)) {
        // Make a content iterator over the selection:
        nsCOMPtr<nsIContentIterator> iter;
        result = nsComponentManager::CreateInstance(kCContentIteratorCID, nsnull,
                                                          NS_GET_IID(nsIContentIterator), 
                                                          getter_AddRefs(iter));
        if ((NS_SUCCEEDED(result)) && iter)
        {
          nsCOMPtr<nsIContent> rootContent (do_QueryInterface(root));
          if (rootContent)
          {
            iter->Init(rootContent);
            // loop through the content iterator for each content node
            while (NS_COMFALSE == iter->IsDone())
            {
              nsCOMPtr<nsIContent> content;
              res = iter->CurrentNode(getter_AddRefs(content));
              if (NS_FAILED(res))
                break;
              //content->BeginConvertToXIF(converter);
              content->ConvertContentToXIF(converter);
              //content->FinishConvertToXIF(converter);
    #if 0
              nsCOMPtr<nsIDOMNode> node (do_QueryInterface(content));
              if (node)
                ToXIF(converter, node);
    #endif
              iter->Next();
            }
          }
        }
     }
    #endif
     }
  converter->AddEndTag(NS_ConvertToString("section_body"), PR_TRUE, PR_TRUE);
  converter->AddEndTag(NS_ConvertToString("section"), PR_TRUE, PR_TRUE);
  return result;
}

static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID);
static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);

#if 0
nsresult
nsDocument::OutputDocumentAs(nsIOutputStream* aStream,
                             nsIDOMSelection* selection,
                             EOutputFormat aOutputFormat,
                             const nsString& aCharset,
                             PRUint32 aFlags)
{
  nsresult  rv = NS_OK;
  
  nsAutoString charsetStr; charsetStr.Assign(aCharset);
  if (charsetStr.Length() == 0)
  {
	  rv = GetDocumentCharacterSet(charsetStr);
	  if(NS_FAILED(rv)) {
	     charsetStr.AssignWithConversion("ISO-8859-1"); 
	  }
  }

  nsCOMPtr<nsIParser>  parser;
  rv = nsComponentManager::CreateInstance(kCParserCID, 
                                             nsnull, 
                                             kCParserIID, 
                                             getter_AddRefs(parser));
  if (NS_SUCCEEDED(rv))
  {
 	  nsString buffer;
	  rv=CreateXIF(buffer, selection);			// if selection is null, ignores the selection

    if(NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIHTMLContentSink> sink;

      switch (aOutputFormat)
      {
      case eOutputText:
        rv = NS_New_HTMLToTXT_SinkStream(getter_AddRefs(sink), aStream, &charsetStr, 0);
        break;
      case eOutputHTML:
        rv = NS_New_HTML_ContentSinkStream(getter_AddRefs(sink), aStream, &charsetStr, 0);
        break;
      default:
        rv = NS_ERROR_INVALID_ARG;
      }

      if (NS_SUCCEEDED(rv) && sink)
      {
        parser->SetContentSink(sink);
        parser->SetDocumentCharset(charsetStr, kCharsetFromPreviousLoading);
        nsCOMPtr<nsIDTD>  dtd;
        rv = NS_NewXIFDTD(getter_AddRefs(dtd));
        if (NS_SUCCEEDED(rv) && dtd)
        {
          parser->RegisterDTD(dtd);
          parser->Parse(buffer, 0, NS_ConvertToString("text/xif"), PR_FALSE, PR_TRUE);           
        }
      }
    }
  }
  return rv;
}
#endif

NS_IMETHODIMP
nsDocument::InitDiskDocument(nsFileSpec* aFileSpec)
{
  mFileSpec = nsnull;
 
  if (aFileSpec)
  {
    mFileSpec = new nsFileSpec(*aFileSpec);
    if (!mFileSpec)
      return NS_ERROR_OUT_OF_MEMORY;
  }
  
  mModCount = 0;
	return NS_OK;
}



NS_IMETHODIMP
nsDocument::SaveFile(nsFileSpec*     aFileSpec,
                     PRBool          aReplaceExisting,
                     PRBool          aSaveCopy,
                     const nsString& aFormatType,
                     const nsString& aSaveCharset,
                     PRUint32        aFlags)
{
  if (!aFileSpec)
    return NS_ERROR_NULL_POINTER;
    
  nsresult  rv = NS_OK;

  // if we're not replacing an existing file but the file
  // exists, somethine is wrong
  if (!aReplaceExisting && aFileSpec->Exists())
    return NS_ERROR_FAILURE;				// where are the file I/O errors?
  
  nsOutputFileStream		stream(*aFileSpec);
  // if the stream didn't open, something went wrong
  if (!stream.is_open())
    return NS_BASE_STREAM_CLOSED;

  // Get a document encoder instance:
  nsCOMPtr<nsIDocumentEncoder> encoder;
  char* progid = (char *)nsMemory::Alloc(strlen(NS_DOC_ENCODER_PROGID_BASE)
                                            + aFormatType.Length() + 1);
  if (! progid)
    return NS_ERROR_OUT_OF_MEMORY;
  strcpy(progid, NS_DOC_ENCODER_PROGID_BASE);
  char* type = aFormatType.ToNewCString();
  strcat(progid, type);
  nsCRT::free(type);
  rv = nsComponentManager::CreateInstance(progid,
                                          nsnull,
                                          NS_GET_IID(nsIDocumentEncoder),
                                          getter_AddRefs(encoder));
  nsCRT::free(progid);
  if (NS_FAILED(rv))
    return rv;

  rv = encoder->Init(this, aFormatType, aFlags);
  if (NS_FAILED(rv))
    return rv;

  nsAutoString charsetStr(aSaveCharset);
  if (charsetStr.Length() == 0)
  {
	  rv = GetDocumentCharacterSet(charsetStr);
	  if(NS_FAILED(rv)) {
	     charsetStr.AssignWithConversion("ISO-8859-1"); 
	  }
  }
  encoder->SetCharset(charsetStr);

  rv = encoder->EncodeToStream(stream.GetIStream());

  if (NS_SUCCEEDED(rv))
  {
    // if everything went OK and we're not just saving off a copy,
    // store the new fileSpec in the doc
    if (!aSaveCopy)
    {
      delete mFileSpec;
      mFileSpec = new nsFileSpec(*aFileSpec);
      if (!mFileSpec)
        return NS_ERROR_OUT_OF_MEMORY;
      
      // and mark the document as clean
      ResetModCount();
    }
  }
  
  return rv;
}

NS_IMETHODIMP
nsDocument::GetFileSpec(nsFileSpec& aFileSpec)
{
  if (mFileSpec)
  {
    aFileSpec = *mFileSpec;
    return NS_OK;
  }
  
  return NS_ERROR_NOT_INITIALIZED;
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
    mBindingManager = do_CreateInstance("component://netscape/xbl/binding-manager", &rv);
    if (NS_FAILED(rv))
      return NS_ERROR_FAILURE;
  }

  *aResult = mBindingManager;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}


NS_IMETHODIMP
nsDocument::GetModCount(PRInt32 *outModCount)
{
  if (!outModCount)
  	return NS_ERROR_NULL_POINTER;
  	
 *outModCount = mModCount;
 return NS_OK;
}


NS_IMETHODIMP
nsDocument::ResetModCount()
{
  mModCount = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::IncrementModCount(PRInt32 aNumMods)
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


/**
 *  Determines if the content is found within the selection
 *  
 *  @update  gpk 1/8/99
 *  @param   param -- description
 *  @param   param -- description
 *  @return  PR_TRUE if the content is found within the selection
 */
PRBool
nsDocument::IsInSelection(nsIDOMSelection* aSelection, const nsIContent* aContent) const
{
  PRBool aYes = PR_FALSE;
  nsCOMPtr<nsIDOMNode> node (do_QueryInterface((nsIContent *) aContent));
  aSelection->ContainsNode(node, PR_FALSE, &aYes);
  return aYes;
}

nsIContent* nsDocument::GetPrevContent(const nsIContent *aContent) const
{
  nsIContent* result = nsnull;
 
  // Look at previous sibling

  if (nsnull != aContent)
  {
    nsIContent* parent;
    aContent->GetParent(parent);
    if (parent != nsnull && parent != mRootContent)
    {
      PRInt32  index;
      parent->IndexOf((nsIContent*)aContent, index);
      if (index > 0)
        parent->ChildAt(index-1, result);
      else
        result = GetPrevContent(parent);
    }
    NS_IF_RELEASE(parent);
  }
  return result;
}

nsIContent* nsDocument::GetNextContent(const nsIContent *aContent) const
{
  nsIContent* result = nsnull;
   
  if (nsnull != aContent)
  {
    // Look at next sibling
    nsIContent* parent;
    aContent->GetParent(parent);
    if (parent != nsnull && parent != mRootContent)
    {
      PRInt32     index;
      parent->IndexOf((nsIContent*)aContent, index);
      PRInt32     count;
      parent->ChildCount(count);
      if (index+1 < count) {
        parent->ChildAt(index+1, result);
        // Get first child down the tree
        for (;;) {
          PRInt32 n;
          result->ChildCount(n);
          if (n <= 0) {
            break;
          }
          nsIContent * old = result;
          old->ChildAt(0, result);
          NS_RELEASE(old);
          result->ChildCount(n);
        }
      } else {
        result = GetNextContent(parent);
      }
    }
    NS_IF_RELEASE(parent);
  }
  return result;
}



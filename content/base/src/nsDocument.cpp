/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "plstr.h"

#include "nsCOMPtr.h"
#include "nsDocument.h"
#include "nsIArena.h"
#include "nsIURL.h"
#include "nsIURLGroup.h"
#include "nsString.h"
#include "nsIContent.h"
#include "nsIDocumentObserver.h"
#include "nsIStyleSet.h"
#include "nsIStyleSheet.h"
#include "nsIPresShell.h"
#include "nsIDocumentObserver.h"
#include "nsEventListenerManager.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContextOwner.h"
#include "nsIScriptEventListener.h"
#include "nsDOMEvent.h"
#include "nsDOMEventsIIDs.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIEventStateManager.h"
#include "nsContentList.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMStyleSheet.h"
#include "nsIDOMStyleSheetCollection.h"
#include "nsDOMAttribute.h"
#include "nsDOMCID.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsIDOMDOMImplementation.h"
#include "nsGenericElement.h"

#include "nsCSSPropIDs.h"
#include "nsCSSProps.h"
#include "nsICSSStyleSheet.h"
#include "nsICSSStyleRule.h"
#include "nsICSSDeclaration.h"

#include "nsITextContent.h"
#include "nsXIFConverter.h"


#include "nsIDOMText.h"
#include "nsIDOMComment.h"

#include "nsINameSpaceManager.h"
#include "nsIServiceManager.h"

#include "nsLayoutCID.h"
#include "nsIDOMSelection.h"
#include "nsIDOMRange.h"
#include "nsIEnumerator.h"

static NS_DEFINE_IID(kIDOMTextIID, NS_IDOMTEXT_IID);
static NS_DEFINE_IID(kIDOMCommentIID, NS_IDOMCOMMENT_IID);
static NS_DEFINE_IID(kIDocumentIID, NS_IDOCUMENT_IID);

#include "nsIDOMElement.h"

static NS_DEFINE_IID(kIDOMDocumentIID, NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIDOMNSDocumentIID, NS_IDOMNSDOCUMENT_IID);
static NS_DEFINE_IID(kIDOMNodeListIID, NS_IDOMNODELIST_IID);
static NS_DEFINE_IID(kIDOMAttrIID, NS_IDOMATTR_IID);
static NS_DEFINE_IID(kIScriptEventListenerIID, NS_ISCRIPTEVENTLISTENER_IID);
static NS_DEFINE_IID(kIDOMEventCapturerIID, NS_IDOMEVENTCAPTURER_IID);
static NS_DEFINE_IID(kIPrivateDOMEventIID, NS_IPRIVATEDOMEVENT_IID);
static NS_DEFINE_IID(kIEventListenerManagerIID, NS_IEVENTLISTENERMANAGER_IID);
static NS_DEFINE_IID(kIPostDataIID, NS_IPOSTDATA_IID);
static NS_DEFINE_IID(kIDOMStyleSheetCollectionIID, NS_IDOMSTYLESHEETCOLLECTION_IID);
static NS_DEFINE_IID(kIDOMStyleSheetIID, NS_IDOMSTYLESHEET_IID);
static NS_DEFINE_IID(kIDOMDOMImplementationIID, NS_IDOMDOMIMPLEMENTATION_IID);
static NS_DEFINE_IID(kIDocumentObserverIID, NS_IDOCUMENT_OBSERVER_IID);
static NS_DEFINE_IID(kICSSStyleSheetIID, NS_ICSS_STYLE_SHEET_IID);
static NS_DEFINE_IID(kCRangeCID, NS_RANGE_CID);
static NS_DEFINE_IID(kIDOMRange, NS_IDOMRANGE_IID);
static NS_DEFINE_IID(kCRangeListCID, NS_RANGELIST_CID);
static NS_DEFINE_IID(kIEnumeratorIID, NS_IENUMERATOR_IID);
static NS_DEFINE_IID(kIDOMScriptObjectFactoryIID, NS_IDOM_SCRIPT_OBJECT_FACTORY_IID);
static NS_DEFINE_IID(kDOMScriptObjectFactoryCID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);


#include "nsILineBreakerFactory.h"
#include "nsLWBrkCIID.h"
static NS_DEFINE_IID(kLWBrkCID, NS_LWBRK_CID);
static NS_DEFINE_IID(kILineBreakerFactoryIID, NS_ILINEBREAKERFACTORY_IID);

class nsDOMStyleSheetCollection : public nsIDOMStyleSheetCollection,
                                  public nsIScriptObjectOwner,
                                  public nsIDocumentObserver
{
public:
  nsDOMStyleSheetCollection(nsIDocument *aDocument);
  virtual ~nsDOMStyleSheetCollection();

  NS_DECL_ISUPPORTS
  NS_DECL_IDOMSTYLESHEETCOLLECTION
  
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
  NS_IMETHOD ContentStateChanged(nsIDocument* aDocument,
                                 nsIContent* aContent) { return NS_OK; }
  NS_IMETHOD AttributeChanged(nsIDocument *aDocument,
                              nsIContent*  aContent,
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

nsDOMStyleSheetCollection::nsDOMStyleSheetCollection(nsIDocument *aDocument)
{
  NS_INIT_REFCNT();
  mLength = -1;
  // Not reference counted to avoid circular references.
  // The document will tell us when its going away.
  mDocument = aDocument;
  mDocument->AddObserver(this);
  mScriptObject = nsnull;
}

nsDOMStyleSheetCollection::~nsDOMStyleSheetCollection()
{
  if (nsnull != mDocument) {
    mDocument->RemoveObserver(this);
  }
  mDocument = nsnull;
}

NS_IMPL_ADDREF(nsDOMStyleSheetCollection)
NS_IMPL_RELEASE(nsDOMStyleSheetCollection)

nsresult 
nsDOMStyleSheetCollection::QueryInterface(REFNSIID aIID, void** aInstancePtrResult)
{
  if (NULL == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  if (aIID.Equals(kIDOMStyleSheetCollectionIID)) {
    nsIDOMStyleSheetCollection *tmp = this;
    *aInstancePtrResult = (void*) tmp;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    nsIScriptObjectOwner *tmp = this;
    *aInstancePtrResult = (void*) tmp;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIDocumentObserverIID)) {
    nsIDocumentObserver *tmp = this;
    *aInstancePtrResult = (void*) tmp;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    nsIDOMStyleSheetCollection *tmp = this;
    nsISupports *tmp2 = tmp;
    *aInstancePtrResult = (void*) tmp2;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP    
nsDOMStyleSheetCollection::GetLength(PRUint32* aLength)
{
  if (nsnull != mDocument) {
    // XXX Find the number and then cache it. We'll use the 
    // observer notification to figure out if new ones have
    // been added or removed.
    if (-1 == mLength) {
      PRUint32 count = 0;
      PRInt32 i, imax = mDocument->GetNumberOfStyleSheets();
      
      for (i = 0; i < imax; i++) {
        nsIStyleSheet *sheet = mDocument->GetStyleSheetAt(i);
        nsIDOMStyleSheet *domss;

        if (NS_OK == sheet->QueryInterface(kIDOMStyleSheetIID, (void **)&domss)) {
          count++;
          NS_RELEASE(domss);
        }

        NS_RELEASE(sheet);
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
nsDOMStyleSheetCollection::Item(PRUint32 aIndex, nsIDOMStyleSheet** aReturn)
{
  *aReturn = nsnull;
  if (nsnull != mDocument) {
    PRUint32 count = 0;
    PRInt32 i, imax = mDocument->GetNumberOfStyleSheets();
  
    // XXX Not particularly efficient, but does anyone care?
    for (i = 0; (i < imax) && (nsnull == *aReturn); i++) {
      nsIStyleSheet *sheet = mDocument->GetStyleSheetAt(i);
      nsIDOMStyleSheet *domss;
      
      if (NS_OK == sheet->QueryInterface(kIDOMStyleSheetIID, (void **)&domss)) {
        if (count++ == aIndex) {
          *aReturn = domss;
          NS_ADDREF(domss);
        }
        NS_RELEASE(domss);
      }
      
      NS_RELEASE(sheet);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsDOMStyleSheetCollection::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;

  if (nsnull == mScriptObject) {
    nsISupports *supports = (nsISupports *)(nsIDOMStyleSheetCollection *)this;
    nsISupports *parent = (nsISupports *)mDocument;

    // XXX Should be done through factory
    res = NS_NewScriptStyleSheetCollection(aContext, 
                                           supports,
                                           parent,
                                           (void**)&mScriptObject);
  }
  *aScriptObject = mScriptObject;

  return res;
}

NS_IMETHODIMP 
nsDOMStyleSheetCollection::SetScriptObject(void* aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

NS_IMETHODIMP 
nsDOMStyleSheetCollection::StyleSheetAdded(nsIDocument *aDocument,
                                           nsIStyleSheet* aStyleSheet)
{
  if (-1 != mLength) {
    nsIDOMStyleSheet *domss;
    if (NS_OK == aStyleSheet->QueryInterface(kIDOMStyleSheetIID, (void **)&domss)) {
      mLength++;
      NS_RELEASE(domss);
    }
  }
  
  return NS_OK;
}

NS_IMETHODIMP 
nsDOMStyleSheetCollection::StyleSheetRemoved(nsIDocument *aDocument,
                                             nsIStyleSheet* aStyleSheet)
{
  if (-1 != mLength) {
    nsIDOMStyleSheet *domss;
    if (NS_OK == aStyleSheet->QueryInterface(kIDOMStyleSheetIID, (void **)&domss)) {
      mLength--;
      NS_RELEASE(domss);
    }
  }
  
  return NS_OK;  
}

NS_IMETHODIMP
nsDOMStyleSheetCollection::DocumentWillBeDestroyed(nsIDocument *aDocument)
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
                            public nsIScriptObjectOwner
{
public:
  nsDOMImplementation();
  virtual ~nsDOMImplementation();

  NS_DECL_ISUPPORTS
  
  NS_IMETHOD    HasFeature(const nsString& aFeature, 
                           const nsString& aVersion, 
                           PRBool* aReturn);

  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void *aScriptObject);

protected:
  void *mScriptObject;
};

nsDOMImplementation::nsDOMImplementation()
{
  NS_INIT_REFCNT();
  mScriptObject = nsnull;
}

nsDOMImplementation::~nsDOMImplementation()
{
}

NS_IMPL_ADDREF(nsDOMImplementation)
NS_IMPL_RELEASE(nsDOMImplementation)

nsresult 
nsDOMImplementation::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIDOMDOMImplementationIID)) {
    nsIDOMDOMImplementation* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    nsIScriptObjectOwner* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    nsIDOMDOMImplementation* tmp = this;
    nsISupports* tmp2 = tmp;
    *aInstancePtr = (void*) tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP    
nsDOMImplementation::HasFeature(const nsString& aFeature, 
                                const nsString& aVersion, 
                                PRBool* aReturn)
{
  // XXX Currently this is hardcoded. In the future, we should
  // probably figure out some of this by querying the registry??
  if (aFeature.EqualsIgnoreCase("HTML") ||
      aFeature.EqualsIgnoreCase("XML")) {
    *aReturn = PR_TRUE;
  }
  else {
    *aReturn = PR_FALSE;
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsDOMImplementation::GetScriptObject(nsIScriptContext *aContext, 
                                     void** aScriptObject)
{
  nsresult result = NS_OK;

  if (nsnull == mScriptObject) {
    nsIDOMScriptObjectFactory *factory;

    result = nsServiceManager::GetService(kDOMScriptObjectFactoryCID,
                                          kIDOMScriptObjectFactoryIID,
                                          (nsISupports **)&factory);
    if (NS_OK == result) {
      nsIScriptGlobalObject *global = aContext->GetGlobalObject();

      result = factory->NewScriptDOMImplementation(aContext, (nsISupports*)(nsIDOMDOMImplementation*)this,
                                                   global, &mScriptObject);
      NS_RELEASE(global);
      NS_RELEASE(factory);
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

// ==================================================================
// =
// ==================================================================

NS_LAYOUT nsresult
NS_NewPostData(PRBool aIsFile, char* aData, 
               nsIPostData** aInstancePtrResult)
{
  nsresult rv = NS_OK;

  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  *aInstancePtrResult = new nsPostData(aIsFile, aData);
  if (nsnull != *aInstancePtrResult) {
    NS_ADDREF(*aInstancePtrResult);
  } else {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  return rv;
}


nsPostData::nsPostData(PRBool aIsFile, char* aData)
{
  NS_INIT_REFCNT();

  mData    = nsnull;
  mDataLen = 0;
  mIsFile  = aIsFile;

  if (aData) {
    mDataLen = PL_strlen(aData);
    mData = aData;
  }
}

nsPostData::~nsPostData()
{
  if (nsnull != mData) {
    delete [] mData;
    mData = nsnull;
  }
}


/*
 * Implementation of ISupports methods...
 */
NS_IMPL_ISUPPORTS(nsPostData,kIPostDataIID);

PRBool nsPostData::IsFile() 
{ 
  return mIsFile;
}

const char* nsPostData::GetData()
{
  return mData;
}

PRInt32 nsPostData::GetDataLength()
{
  return mDataLen;
}




nsDocument::nsDocument()
{
  NS_INIT_REFCNT();

  mArena = nsnull;
  mDocumentTitle = nsnull;
  mDocumentURL = nsnull;
  mDocumentURLGroup = nsnull;
  mCharacterSet = nsnull;
  mParentDocument = nsnull;
  mRootContent = nsnull;
  mScriptObject = nsnull;
  mScriptContextOwner = nsnull;
  mListenerManager = nsnull;
  mDisplaySelection = PR_FALSE;
  mInDestructor = PR_FALSE;
  mDOMStyleSheets = nsnull;
  mNameSpaceManager = nsnull;
  mHeaderData = nsnull;
  mLineBreaker = nsnull;

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
  }

  if (nsnull != mDocumentTitle) {
    delete mDocumentTitle;
    mDocumentTitle = nsnull;
  }
  NS_IF_RELEASE(mDocumentURL);
  NS_IF_RELEASE(mDocumentURLGroup);

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

  NS_IF_RELEASE(mArena);
  NS_IF_RELEASE(mScriptContextOwner);
  NS_IF_RELEASE(mListenerManager);
  NS_IF_RELEASE(mDOMStyleSheets);
  NS_IF_RELEASE(mNameSpaceManager);
  if (nsnull != mHeaderData) {
    delete mHeaderData;
    mHeaderData = nsnull;
  }
  NS_IF_RELEASE(mLineBreaker);
}

nsresult nsDocument::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIDocumentIID)) {
    nsIDocument* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMDocumentIID)) {
    nsIDOMDocument* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMNSDocumentIID)) {
    nsIDOMNSDocument* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    nsIScriptObjectOwner* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIJSScriptObjectIID)) {
    nsIJSScriptObject* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMEventCapturerIID)) {
    nsIDOMEventCapturer* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMEventReceiverIID)) {
    nsIDOMEventReceiver* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMNodeIID)) {
    nsIDOMNode* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  if (aIID.Equals(kISupportsIID)) {
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
  nsresult rv = NS_NewHeapArena(&mArena, nsnull);

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
nsDocument::Reset(nsIURL *aURL)
{
  nsresult rv = NS_OK;

  if (nsnull != mDocumentTitle) {
    delete mDocumentTitle;
    mDocumentTitle = nsnull;
  }
  NS_IF_RELEASE(mDocumentURL);
  NS_IF_RELEASE(mDocumentURLGroup);

  // Delete references to sub-documents
  PRInt32 index = mSubDocuments.Count();
  while (--index >= 0) {
    nsIDocument* subdoc = (nsIDocument*) mSubDocuments.ElementAt(index);
    NS_RELEASE(subdoc);
  }

  if (nsnull != mRootContent) {
    // Ensure that document is nsnull to allow validity checks on content
    mRootContent->SetDocument(nsnull, PR_TRUE);
    ContentRemoved(nsnull, mRootContent, 0);
    NS_IF_RELEASE(mRootContent);
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

  mDocumentURL = aURL;
  if (nsnull != aURL) {
    NS_ADDREF(aURL);

    rv = aURL->GetURLGroup(&mDocumentURLGroup);
  }

  if (NS_OK == rv) {
    rv = NS_NewNameSpaceManager(&mNameSpaceManager);
  }

  return rv;
}

nsresult
nsDocument::StartDocumentLoad(nsIURL *aURL, 
                              nsIContentViewerContainer* aContainer,
                              nsIStreamListener **aDocListener,
                              const char* aCommand)
{
  return Reset(aURL);
}

const nsString* nsDocument::GetDocumentTitle() const
{
  return mDocumentTitle;
}

nsIURL* nsDocument::GetDocumentURL() const
{
  NS_IF_ADDREF(mDocumentURL);
  return mDocumentURL;
}

nsIURLGroup* nsDocument::GetDocumentURLGroup() const
{
  NS_IF_ADDREF(mDocumentURLGroup);
  return mDocumentURLGroup;
}

NS_IMETHODIMP
nsDocument::GetBaseURL(nsIURL*& aURL) const
{
  aURL = mDocumentURL;
  NS_IF_ADDREF(mDocumentURL);
  return NS_OK;
}

nsString* nsDocument::GetDocumentCharacterSet() const
{
  return mCharacterSet;
}

void nsDocument::SetDocumentCharacterSet(nsString* aCharSetID)
{
  mCharacterSet = aCharSetID;
}

NS_IMETHODIMP nsDocument::GetLineBreaker(nsILineBreaker** aResult) 
{
  if(nsnull == mLineBreaker ) {
     // no line breaker, find a default one
     nsILineBreakerFactory *lf;
     nsresult result;
     result = nsServiceManager::GetService(kLWBrkCID,
                                          kILineBreakerFactoryIID,
                                          (nsISupports **)&lf);
     if (NS_SUCCEEDED(result)) {
      nsILineBreaker *lb = nsnull ;
      nsAutoString lbarg("");
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
            data->mData = aData;
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
nsresult nsDocument::CreateShell(nsIPresContext* aContext,
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

void nsDocument::AddStyleSheet(nsIStyleSheet* aSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");
  InternalAddStyleSheet(aSheet);
  NS_ADDREF(aSheet);
  aSheet->SetOwningDocument(this);

  PRBool enabled = PR_TRUE;
  aSheet->GetEnabled(enabled);

  if (enabled) {
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

    // XXX should observers be notified for disabled sheets??? I think not, but I could be wrong
    for (index = 0; index < mObservers.Count(); index++) {
      nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers.ElementAt(index);
      observer->StyleSheetAdded(this, aSheet);
    }
  }
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
  }
}

nsIScriptContextOwner *nsDocument::GetScriptContextOwner()
{
  if (nsnull != mScriptContextOwner) {
    NS_ADDREF(mScriptContextOwner);
  }
  
  return mScriptContextOwner;
}

void nsDocument::SetScriptContextOwner(nsIScriptContextOwner *aScriptContextOwner)
{
  // XXX HACK ALERT! If the script context owner is null, the document
  // will soon be going away. So tell our content that to lose its
  // reference to the document. This has to be done before we
  // actually set the script context owner to null so that the
  // content elements can remove references to their script objects.
  if ((nsnull == aScriptContextOwner) && (nsnull != mRootContent)) {
    mRootContent->SetDocument(nsnull, PR_TRUE);
  }

  if (nsnull != mScriptContextOwner) {
    NS_RELEASE(mScriptContextOwner);
  }
  
  mScriptContextOwner = aScriptContextOwner;
  
  if (nsnull != mScriptContextOwner) {
    NS_ADDREF(mScriptContextOwner);
  }
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
nsDocument::ContentStateChanged(nsIContent* aContent)
{
  PRInt32 i;
  // Get new value of count for every iteration in case
  // observers remove themselves during the loop.
  for (i = 0; i < mObservers.Count(); i++) {
    nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
    observer->ContentStateChanged(this, aContent);
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
                             nsIAtom* aAttribute,
                             PRInt32 aHint)
{
  PRInt32 i;
  // Get new value of count for every iteration in case
  // observers remove themselves during the loop.
  for (i = 0; i < mObservers.Count(); i++) {
    nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
    observer->AttributeChanged(this, aChild, aAttribute, aHint);
    // Make sure that the observer didn't remove itself during the
    // notification. If it did, update our index and count.
    if (observer != (nsIDocumentObserver*)mObservers[i]) {
      i--;
    }
  }
  return NS_OK;
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
    observer->StyleRuleChanged(this, aStyleSheet, aStyleRule, aHint);
    // Make sure that the observer didn't remove itself during the
    // notification. If it did, update our index and count.
    if (observer != (nsIDocumentObserver*)mObservers[i]) {
      i--;
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
    observer->StyleRuleAdded(this, aStyleSheet, aStyleRule);
    // Make sure that the observer didn't remove itself during the
    // notification. If it did, update our index and count.
    if (observer != (nsIDocumentObserver*)mObservers[i]) {
      i--;
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
    observer->StyleRuleRemoved(this, aStyleSheet, aStyleRule);
    // Make sure that the observer didn't remove itself during the
    // notification. If it did, update our index and count.
    if (observer != (nsIDocumentObserver*)mObservers[i]) {
      i--;
    }
  }
  return NS_OK;
}


nsresult nsDocument::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;
  nsIScriptGlobalObject *global = aContext->GetGlobalObject();

  if (nsnull == mScriptObject) {
    res = NS_NewScriptDocument(aContext, (nsISupports *)(nsIDOMDocument *)this, global, (void**)&mScriptObject);
  }
  *aScriptObject = mScriptObject;

  NS_RELEASE(global);
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
  // Should be implemented by subclass
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::GetImplementation(nsIDOMDOMImplementation** aImplementation)
{
  // For now, create a new implementation every time. This shouldn't
  // be a high bandwidth operation
  nsDOMImplementation* impl = new nsDOMImplementation();
  if (nsnull == impl) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return impl->QueryInterface(kIDOMDOMImplementationIID, (void**)aImplementation);
}

NS_IMETHODIMP    
nsDocument::GetDocumentElement(nsIDOMElement** aDocumentElement)
{
  if (nsnull == aDocumentElement) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult res = NS_ERROR_FAILURE;

  if (nsnull != mRootContent) {
    res = mRootContent->QueryInterface(kIDOMElementIID, (void**)aDocumentElement);
    NS_ASSERTION(NS_OK == res, "Must be a DOM Element");
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
    rv = text->QueryInterface(kIDOMTextIID, (void**)aReturn);
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
nsDocument::CreateComment(const nsString& aData, nsIDOMComment** aReturn)
{
  nsIContent* comment = nsnull;
  nsresult        rv = NS_NewCommentNode(&comment);

  if (NS_OK == rv) {
    rv = comment->QueryInterface(kIDOMCommentIID, (void**)aReturn);
    (*aReturn)->AppendData(aData);
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
  nsAutoString value;
  nsDOMAttribute* attribute;
  
  value.Truncate();
  attribute = new nsDOMAttribute(nsnull, aName, value);
  if (nsnull == attribute) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return attribute->QueryInterface(kIDOMAttrIID, (void**)aReturn);
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
  nsIAtom* nameAtom;
  PRInt32 nameSpaceId;
  nsresult result = NS_OK;

  if (nsnull != mRootContent) {
    result = mRootContent->ParseAttributeString(aTagname, nameAtom,
                                                nameSpaceId);
    if (NS_OK != result) {
      return result;
    }
  }
  else {
    nameAtom = NS_NewAtom(aTagname);
    nameSpaceId = kNameSpaceID_None;
  }

  nsContentList* list = new nsContentList(this, nameAtom, nameSpaceId);
  NS_IF_RELEASE(nameAtom);
  if (nsnull == list) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return list->QueryInterface(kIDOMNodeListIID, (void **)aReturn);
}

NS_IMETHODIMP    
nsDocument::GetStyleSheets(nsIDOMStyleSheetCollection** aStyleSheets)
{
  if (nsnull == mDOMStyleSheets) {
    mDOMStyleSheets = new nsDOMStyleSheetCollection(this);
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
nsDocument::CreateElementWithNameSpace(const nsString& aTagName, 
                                       const nsString& aNameSpace, 
                                       nsIDOMElement** aReturn)
{
  return NS_OK;
}

//
// nsIDOMNode methods
//
NS_IMETHODIMP    
nsDocument::GetNodeName(nsString& aNodeName)
{
  aNodeName.SetString("#document");
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
  return NS_OK;
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
  // Should be implemented by subclass
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::HasChildNodes(PRBool* aHasChildNodes)
{
  // Should be implemented by subclass
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::GetFirstChild(nsIDOMNode** aFirstChild)
{
  // Should be implemented by subclass
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::GetLastChild(nsIDOMNode** aLastChild)
{
  // Should be implemented by subclass
  return NS_ERROR_NOT_IMPLEMENTED;
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
nsDocument::InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn)
{
  // Should be implemented by subclass
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  // Should be implemented by subclass
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  // Should be implemented by subclass
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
  // Should be implemented by subclass
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  // We don't allow cloning of a document
  *aReturn = nsnull;
  return NS_OK;
}


NS_IMETHODIMP    
nsDocument::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
  return QueryInterface(kIDOMDocumentIID, (void **)aOwnerDocument);
}

nsresult nsDocument::GetListenerManager(nsIEventListenerManager **aInstancePtrResult)
{
  if (nsnull != mListenerManager) {
    return mListenerManager->QueryInterface(kIEventListenerManagerIID, (void**) aInstancePtrResult);;
  }
  if (NS_OK == NS_NewEventListenerManager(aInstancePtrResult)) {
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

nsresult nsDocument::HandleDOMEvent(nsIPresContext& aPresContext, 
                                    nsEvent* aEvent, 
                                    nsIDOMEvent** aDOMEvent,
                                    PRUint32 aFlags,
                                    nsEventStatus& aEventStatus)
{
  nsresult mRet = NS_OK;
  nsIDOMEvent* mDOMEvent = nsnull;

  if (DOM_EVENT_INIT == aFlags) {
    aDOMEvent = &mDOMEvent;
  }
  
  //Capturing stage
  /*if (mEventCapturer) {
    mEventCapturer->HandleDOMEvent(aPresContext, aEvent, aDOMEvent, aFlags, aEventStatus);
  }*/
  
  //Local handling stage
  if (nsnull != mListenerManager) {
    mListenerManager->HandleEvent(aPresContext, aEvent, aDOMEvent, aEventStatus);
  }

  //Bubbling stage
  if (DOM_EVENT_CAPTURE != aFlags && nsnull != mScriptContextOwner) {
    nsIScriptGlobalObject* mGlobal;
    if (NS_OK == mScriptContextOwner->GetScriptGlobalObject(&mGlobal)) {
      mGlobal->HandleDOMEvent(aPresContext, aEvent, aDOMEvent, DOM_EVENT_BUBBLE, aEventStatus);
      NS_RELEASE(mGlobal);
    }
  }

  /*Need to go to window here*/

  if (DOM_EVENT_INIT == aFlags) {
    // We're leaving the DOM event loop so if we created a DOM event, release here.
    if (nsnull != *aDOMEvent) {
      nsrefcnt rc;
      NS_RELEASE2(*aDOMEvent, rc);
      if (0 != rc) {
      //Okay, so someone in the DOM loop (a listener, JS object) still has a ref to the DOM Event but
      //the internal data hasn't been malloc'd.  Force a copy of the data here so the DOM Event is still valid.
        nsIPrivateDOMEvent *mPrivateEvent;
        if (NS_OK == (*aDOMEvent)->QueryInterface(kIPrivateDOMEventIID, (void**)&mPrivateEvent)) {
          mPrivateEvent->DuplicatePrivateData();
          NS_RELEASE(mPrivateEvent);
        }
      }
    }
    aDOMEvent = nsnull;
  }

  return mRet;
}

nsresult nsDocument::AddEventListener(nsIDOMEventListener *aListener, const nsIID& aIID)
{
  nsIEventListenerManager *mManager;

  if (NS_OK == GetListenerManager(&mManager)) {
    mManager->AddEventListener(aListener, aIID);
    NS_RELEASE(mManager);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult nsDocument::RemoveEventListener(nsIDOMEventListener *aListener, const nsIID& aIID)
{
  if (nsnull != mListenerManager) {
    mListenerManager->RemoveEventListener(aListener, aIID);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult nsDocument::CaptureEvent(const nsString& aType)
{
  nsIEventListenerManager *mManager;

  if (NS_OK == GetListenerManager(&mManager)) {
    //mManager->CaptureEvent(aListener);
    NS_RELEASE(mManager);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult nsDocument::ReleaseEvent(const nsString& aType)
{
  if (nsnull != mListenerManager) {
    //mListenerManager->ReleaseEvent(aListener);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

PRBool    nsDocument::AddProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
  return PR_TRUE;
}

PRBool    nsDocument::DeleteProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
  return PR_TRUE;
}

PRBool    nsDocument::GetProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
  PRBool result = PR_TRUE;

  if (JSVAL_IS_STRING(aID) && 
      PL_strcmp("location", JS_GetStringBytes(JS_ValueToString(aContext, aID))) == 0) {
    if (nsnull != mScriptContextOwner) {
      nsIScriptGlobalObject *global;
      mScriptContextOwner->GetScriptGlobalObject(&global);
      if (nsnull != global) {
        nsIJSScriptObject *window;
        if (NS_OK == global->QueryInterface(kIJSScriptObjectIID, (void **)&window)) {
          result = window->GetProperty(aContext, aID, aVp);
          NS_RELEASE(window);
        }
        else {
          result = PR_FALSE;
        }
        NS_RELEASE(global);
      }
    }
  }

  return result;
}

PRBool    nsDocument::SetProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
  PRBool result = PR_TRUE;

  if (JS_TypeOfValue(aContext, *aVp) == JSTYPE_FUNCTION && JSVAL_IS_STRING(aID)) {
    nsAutoString mPropName, mPrefix;
    mPropName.SetString(JS_GetStringChars(JS_ValueToString(aContext, aID)));
    mPrefix.SetString(mPropName, 2);
    if (mPrefix == "on") {
      nsIEventListenerManager *mManager = nsnull;

      if (mPropName == "onmousedown" || mPropName == "onmouseup" || mPropName ==  "onclick" ||
         mPropName == "onmouseover" || mPropName == "onmouseout") {
        if (NS_OK == GetListenerManager(&mManager)) {
          nsIScriptContext *mScriptCX = (nsIScriptContext *)JS_GetContextPrivate(aContext);
          if (NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this, kIDOMMouseListenerIID)) {
            NS_RELEASE(mManager);
            return PR_FALSE;
          }
        }
      }
      else if (mPropName == "onkeydown" || mPropName == "onkeyup" || mPropName == "onkeypress") {
        if (NS_OK == GetListenerManager(&mManager)) {
          nsIScriptContext *mScriptCX = (nsIScriptContext *)JS_GetContextPrivate(aContext);
          if (NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this, kIDOMKeyListenerIID)) {
            NS_RELEASE(mManager);
            return PR_FALSE;
          }
        }
      }
      else if (mPropName == "onmousemove") {
        if (NS_OK == GetListenerManager(&mManager)) {
          nsIScriptContext *mScriptCX = (nsIScriptContext *)JS_GetContextPrivate(aContext);
          if (NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this, kIDOMMouseMotionListenerIID)) {
            NS_RELEASE(mManager);
            return PR_FALSE;
          }
        }
      }
      else if (mPropName == "onfocus" || mPropName == "onblur") {
        if (NS_OK == GetListenerManager(&mManager)) {
          nsIScriptContext *mScriptCX = (nsIScriptContext *)JS_GetContextPrivate(aContext);
          if (NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this, kIDOMFocusListenerIID)) {
            NS_RELEASE(mManager);
            return PR_FALSE;
          }
        }
      }
      else if (mPropName == "onsubmit" || mPropName == "onreset" || mPropName == "onchange") {
        if (NS_OK == GetListenerManager(&mManager)) {
          nsIScriptContext *mScriptCX = (nsIScriptContext *)JS_GetContextPrivate(aContext);
          if (NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this, kIDOMFormListenerIID)) {
            NS_RELEASE(mManager);
            return PR_FALSE;
          }
        }
      }
      else if (mPropName == "onload" || mPropName == "onunload" || mPropName == "onabort" ||
               mPropName == "onerror") {
        if (NS_OK == GetListenerManager(&mManager)) {
          nsIScriptContext *mScriptCX = (nsIScriptContext *)JS_GetContextPrivate(aContext);
          if (NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this, kIDOMLoadListenerIID)) {
            NS_RELEASE(mManager);
            return PR_FALSE;
          }
        }
      }
      else if (mPropName == "onpaint") {
        if (NS_OK == GetListenerManager(&mManager)) {
          nsIScriptContext *mScriptCX = (nsIScriptContext *)
            JS_GetContextPrivate(aContext);
          if (NS_OK != mManager->RegisterScriptEventListener(mScriptCX, this,
                                                      kIDOMPaintListenerIID)) {
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
    if (nsnull != mScriptContextOwner) {
      nsIScriptGlobalObject *global;
      mScriptContextOwner->GetScriptGlobalObject(&global);
      if (nsnull != global) {
        nsIJSScriptObject *window;
        if (NS_OK == global->QueryInterface(kIJSScriptObjectIID, (void **)&window)) {
          result = window->SetProperty(aContext, aID, aVp);
          NS_RELEASE(window);
        }
        else {
          result = PR_FALSE;
        }
        NS_RELEASE(global);
      }
    }
  }

  return result;
}

PRBool    nsDocument::EnumerateProperty(JSContext *aContext)
{
  return PR_TRUE;
}

PRBool    nsDocument::Resolve(JSContext *aContext, jsval aID)
{
  return PR_TRUE;
}

PRBool    nsDocument::Convert(JSContext *aContext, jsval aID)
{
  return PR_TRUE;
}

void      nsDocument::Finalize(JSContext *aContext)
{
}

/**
  * Returns the Selection Object
 */
NS_IMETHODIMP nsDocument::GetSelection(nsIDOMSelection ** aSelection) {
  if (!aSelection)
    return NS_ERROR_NULL_POINTER;
  return NS_ERROR_FAILURE;
}

/**
  * Selects all the Content
 */
NS_IMETHODIMP nsDocument::SelectAll() {

  nsIContent * start = nsnull;
  nsIContent * end   = nsnull;
  nsIContent * body  = nsnull;

  nsString bodyStr("BODY");
  PRInt32 i, n;
  mRootContent->ChildCount(n);
  for (i=0;i<n;i++) {
    nsIContent * child;
    mRootContent->ChildAt(i, child);
    PRBool isSynthetic;
    child->IsSynthetic(isSynthetic);
    if (!isSynthetic) {
      nsIAtom * atom;
      child->GetTag(atom);
      if (bodyStr.EqualsIgnoreCase(atom)) {
        body = child;
        break;
      }
      NS_IF_RELEASE(atom);
    }
    NS_RELEASE(child);
  }

  if (body == nsnull) {
    return NS_ERROR_FAILURE;
  }

  start = body;
  // Find Very first Piece of Content
  NS_ADDREF(start); //to balance release below
  for (;;) {
    start->ChildCount(n);
    if (n <= 0) {
      break;
    }
    nsIContent * child = start;
    child->ChildAt(0, start);
    NS_RELEASE(child);
  }

  end = body;
  NS_ADDREF(end); //to balance release below
  // Last piece of Content
  for (;;) {
    end->ChildCount(n);
    if (n <= 0) {
      break;
    }
    nsIContent * child = end;
    child->ChildAt(n-1, end);
    NS_RELEASE(child);
  }

  NS_RELEASE(body);
  SetDisplaySelection(PR_TRUE);

  return NS_OK;
}

/**
  * Finds text in content
 */
NS_IMETHODIMP nsDocument::FindNext(const nsString &aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound)
{
  aIsFound = PR_FALSE;
  return NS_ERROR_FAILURE;
}



void nsDocument::BeginConvertToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode)
{
  nsIContent* content = nsnull;
  nsresult    isContent = aNode->QueryInterface(kIContentIID, (void**)&content);
  PRBool      isSynthetic = PR_TRUE;

  // Begin Conversion
  if (NS_OK == isContent) 
  {
    content->IsSynthetic(isSynthetic);
    if (PR_FALSE == isSynthetic)
    {
      content->BeginConvertToXIF(aConverter);
      content->ConvertContentToXIF(aConverter);
    }
    NS_RELEASE(content);
  }
}

void nsDocument::ConvertChildrenToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode)
{
  // Iterate through the children, convertion child nodes
  nsresult result = NS_OK;
  nsIDOMNode* child = nsnull;
  result = aNode->GetFirstChild(&child);
    
  while ((result == NS_OK) && (child != nsnull))
  { 
    nsIDOMNode* temp = child;
    ToXIF(aConverter,child);    
    result = child->GetNextSibling(&child);
    NS_RELEASE(temp);
  }
}

void nsDocument::FinishConvertToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode)
{
  nsIContent* content = nsnull;
  nsresult    isContent = aNode->QueryInterface(kIContentIID, (void**)&content);
  PRBool      isSynthetic = PR_TRUE;

  if (NS_OK == isContent) 
  {
    content->IsSynthetic(isSynthetic);
    if (PR_FALSE == isSynthetic)
      content->FinishConvertToXIF(aConverter);
    NS_RELEASE(content);
  }
}


void nsDocument::ToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode)
{
  nsIDOMSelection* sel = aConverter.GetSelection();

  if (sel != nsnull)
  {
    nsIContent* content = nsnull;
    nsresult    isContent = aNode->QueryInterface(kIContentIID, (void**)&content);

    if (isContent != nsnull)
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
      NS_RELEASE(content);
    }
  }
  else
  {
    BeginConvertToXIF(aConverter,aNode);
    ConvertChildrenToXIF(aConverter,aNode);
    FinishConvertToXIF(aConverter,aNode);
  }
}

void nsDocument::CreateXIF(nsString & aBuffer, nsIDOMSelection* aSelection)
{
  
  nsXIFConverter  converter(aBuffer);
  // call the function

  converter.SetSelection(aSelection);

  converter.AddStartTag("section");
  
  converter.AddStartTag("section_head");
  converter.AddEndTag("section_head");

  converter.AddStartTag("section_body");

  nsIDOMElement* root = nsnull;
  if (NS_OK == GetDocumentElement(&root)) 
  {  
    ToXIF(converter,root);
    NS_RELEASE(root);
  }
  converter.AddEndTag("section_body");

  converter.AddEndTag("section");

  converter.Write();
  
}


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

PRBool nsDocument::IsInRange(const nsIContent *aStartContent, const nsIContent* aEndContent, const nsIContent* aContent) const
{
  PRBool  result;

  if (aStartContent == aEndContent) 
  {
    return PRBool(aContent == aStartContent);
  }
  else if (aStartContent == aContent || aEndContent == aContent)
  {
    result = PR_TRUE;
  }
  else
  {
    result = IsBefore(aStartContent,aContent);
    if (result == PR_TRUE)
      result = IsBefore(aContent,aEndContent);
  }
  return result;

}


/**
 *  Determines if the content is found within the selection
 *  
 *  @update  gpk 1/8/99
 *  @param   param -- description
 *  @param   param -- description
 *  @return  PR_TRUE if the content is found within the selection
 */
PRBool nsDocument::IsInSelection(nsIDOMSelection* aSelection, const nsIContent* aContent) const
{
  PRBool          result = PR_FALSE;
  
  
  if (aSelection != nsnull) {
    //traverses through an iterator to see if the acontent is in the ranges
    nsIEnumerator *enumerator;
    if (NS_SUCCEEDED(aSelection->QueryInterface(kIEnumeratorIID, (void **)&enumerator))) 
    {
      for (enumerator->First();NS_OK != enumerator->IsDone() ; enumerator->Next()) {
        nsIDOMRange* range = nsnull;
        if (NS_SUCCEEDED(enumerator->CurrentItem((nsISupports**)&range)))
        {
          nsIDOMNode* startNode = nsnull;
          nsIDOMNode* endNode = nsnull;

          range->GetStartParent(&startNode); 
          range->GetEndParent(&endNode);
          
          if (startNode && endNode)
          {
            nsIContent* start;
            nsIContent* end;

            if (NS_SUCCEEDED(startNode->QueryInterface(kIContentIID, (void **)&start)) &&
                NS_SUCCEEDED(endNode->QueryInterface(kIContentIID, (void **)&end)) )
            {
              result = IsInRange(start,end,aContent);
            }
            NS_IF_RELEASE(start);
            NS_IF_RELEASE(end);
          }
          NS_IF_RELEASE(startNode);
          NS_IF_RELEASE(endNode);
          NS_RELEASE(range);
        }
        if (result) break;
      }
      NS_IF_RELEASE(enumerator);
    }
  }
  return result;
}



PRBool nsDocument::IsBefore(const nsIContent *aNewContent, const nsIContent* aCurrentContent) const
{

  PRBool result = PR_FALSE;

  if (nsnull != aNewContent && nsnull != aCurrentContent && aNewContent != aCurrentContent)
  {
    nsIContent* test = FindContent(mRootContent,aNewContent,aCurrentContent);
    if (test == aNewContent)
      result = PR_TRUE;
    NS_RELEASE(test);
  }
  return result;
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

void nsDocument::SetDisplaySelection(PRBool aToggle)
{
  mDisplaySelection = aToggle;
}

PRBool nsDocument::GetDisplaySelection() const
{
  return mDisplaySelection;
}

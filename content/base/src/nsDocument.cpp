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

#include "nsDocument.h"
#include "nsIArena.h"
#include "nsIURL.h"
#include "nsString.h"
#include "nsIContent.h"
#include "nsIStyleSet.h"
#include "nsIStyleSheet.h"
#include "nsIPresShell.h"
#include "nsIDocumentObserver.h"
#include "nsEventListenerManager.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContextOwner.h"
#include "nsIParser.h"


#include "nsCSSPropIDs.h"
#include "nsCSSProps.h"
#include "nsICSSStyleSheet.h"
#include "nsICSSStyleRule.h"
#include "nsICSSDeclaration.h"
#include "nsIHTMLCSSStyleSheet.h"

#include "nsHTMLValue.h"
#include "nsXIFConverter.h"

#include "nsSelection.h"
#include "nsIDOMText.h"
static NS_DEFINE_IID(kIDOMTextIID, NS_IDOMTEXT_IID);

static NS_DEFINE_IID(kIDocumentIID, NS_IDOCUMENT_IID);

#include "nsIDOMElement.h"

static NS_DEFINE_IID(kIDOMDocumentIID, NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIContentIID, NS_ICONTENT_IID);
static NS_DEFINE_IID(kIDOMElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIDOMEventCapturerIID, NS_IDOMEVENTCAPTURER_IID);
static NS_DEFINE_IID(kIDOMEventReceiverIID, NS_IDOMEVENTRECEIVER_IID);
static NS_DEFINE_IID(kIEventListenerManagerIID, NS_IEVENTLISTENERMANAGER_IID);
static NS_DEFINE_IID(kIPostDataIID, NS_IPOSTDATA_IID);
static NS_DEFINE_IID(kICSSStyleSheetIID, NS_ICSS_STYLE_SHEET_IID);


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
  mCharacterSet = eCharSetID_IsoLatin1;
  mParentDocument = nsnull;
  mRootContent = nsnull;
  mScriptObject = nsnull;
  mScriptContextOwner = nsnull;
  mListenerManager = nsnull;
  mParser = nsnull;
 
  if (NS_OK != NS_NewSelection(&mSelection)) {
    printf("*************** Error: nsDocument::nsDocument - Creation of Selection failed!\n");
  }

  Init();/* XXX */
}

nsDocument::~nsDocument()
{
  if (nsnull != mDocumentTitle) {
    delete mDocumentTitle;
    mDocumentTitle = nsnull;
  }
  NS_IF_RELEASE(mDocumentURL);

  mParentDocument = nsnull;

  // Delete references to sub-documents
  PRInt32 index = mSubDocuments.Count();
  while (--index >= 0) {
    nsIDocument* subdoc = (nsIDocument*) mSubDocuments.ElementAt(index);
    NS_RELEASE(subdoc);
  }

  NS_IF_RELEASE(mRootContent);
  // Delete references to style sheets
  index = mStyleSheets.Count();
  while (--index >= 0) {
    nsIStyleSheet* sheet = (nsIStyleSheet*) mStyleSheets.ElementAt(index);
    NS_RELEASE(sheet);
  }

  NS_IF_RELEASE(mArena);
  NS_IF_RELEASE(mSelection);
  NS_IF_RELEASE(mScriptContextOwner);
  NS_IF_RELEASE(mParser);
}

nsresult nsDocument::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIDocumentIID)) {
    *aInstancePtr = (void*)(nsIDocument*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMDocumentIID)) {
    *aInstancePtr = (void*)(nsIDOMDocument*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    *aInstancePtr = (void*)(nsIScriptObjectOwner*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMEventCapturerIID)) {
    *aInstancePtr = (void*)(nsIDOMEventCapturer*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMEventReceiverIID)) {
    *aInstancePtr = (void*)(nsIDOMEventReceiver*)this;
    AddRef();
    return NS_OK;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)(nsIDocument*)this;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsDocument)
NS_IMPL_RELEASE(nsDocument)

nsresult nsDocument::Init()
{
  nsresult rv = NS_NewHeapArena(&mArena, nsnull);
  if (NS_OK != rv) {
    return rv;
  }
  return NS_OK;
}

nsIArena* nsDocument::GetArena()
{
  if (nsnull != mArena) {
    NS_ADDREF(mArena);
  }
  return mArena;
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

nsCharSetID nsDocument::GetDocumentCharacterSet() const
{
  return mCharacterSet;
}

void nsDocument::SetDocumentCharacterSet(nsCharSetID aCharSetID)
{
  mCharacterSet = aCharSetID;
}

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

void nsDocument::AddStyleSheetToSet(nsIStyleSheet* aSheet, nsIStyleSet* aSet)
{
  aSet->AppendDocStyleSheet(aSheet);
}

void nsDocument::AddStyleSheet(nsIStyleSheet* aSheet)
{
  NS_PRECONDITION(nsnull != aSheet, "null arg");
  mStyleSheets.AppendElement(aSheet);
  NS_ADDREF(aSheet);

  PRInt32 count = mPresShells.Count();
  PRInt32 index;
  for (index = 0; index < count; index++) {
    nsIPresShell* shell = (nsIPresShell*)mPresShells.ElementAt(index);
    nsIStyleSet* set = shell->GetStyleSet();
    if (nsnull != set) {
      AddStyleSheetToSet(aSheet, set);
      NS_RELEASE(set);
    }
  }

  count = mObservers.Count();
  for (index = 0; index < count; index++) {
    nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers.ElementAt(index);
    observer->StyleSheetAdded(aSheet);
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
  if (nsnull != mScriptContextOwner) {
    NS_RELEASE(mScriptContextOwner);
  }
  
  mScriptContextOwner = aScriptContextOwner;
  
  if (nsnull != mScriptContextOwner) {
    NS_ADDREF(mScriptContextOwner);
  }
}

nsIParser *nsDocument::GetParser()
{
  NS_IF_ADDREF(mParser);
  
  return mParser;
}

void nsDocument::SetParser(nsIParser *aParser)
{
  NS_IF_RELEASE(mParser);

  mParser = aParser;
  
  NS_IF_ADDREF(mParser);
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
  return mObservers.RemoveElement(aObserver);
}

NS_IMETHODIMP
nsDocument::BeginLoad()
{
  PRInt32 i, count = mObservers.Count();
  for (i = 0; i < count; i++) {
    nsIDocumentObserver* observer = (nsIDocumentObserver*) mObservers[i];
    observer->BeginLoad();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDocument::EndLoad()
{
  PRInt32 i, count = mObservers.Count();
  for (i = 0; i < count; i++) {
    nsIDocumentObserver* observer = (nsIDocumentObserver*) mObservers[i];
    observer->EndLoad();
  }
  return NS_OK;
}

void nsDocument::ContentChanged(nsIContent* aContent,
                                nsISupports* aSubContent)
{
  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; i++) {
    nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
    observer->ContentChanged(aContent, aSubContent);
  }
}

void nsDocument::ContentAppended(nsIContent* aContainer)
{
  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; i++) {
    nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
    observer->ContentAppended(aContainer);
  }
}

void nsDocument::ContentInserted(nsIContent* aContainer,
                                 nsIContent* aChild,
                                 PRInt32 aIndexInContainer)
{
  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; i++) {
    nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
    observer->ContentInserted(aContainer, aChild, aIndexInContainer);
  }
}

void nsDocument::ContentReplaced(nsIContent* aContainer,
                                 nsIContent* aOldChild,
                                 nsIContent* aNewChild,
                                 PRInt32 aIndexInContainer)
{
  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; i++) {
    nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
    observer->ContentReplaced(aContainer, aOldChild, aNewChild,
                              aIndexInContainer);
  }
}

void nsDocument::ContentWillBeRemoved(nsIContent* aContainer,
                                      nsIContent* aChild,
                                      PRInt32 aIndexInContainer)
{
  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; i++) {
    nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
    observer->ContentWillBeRemoved(aContainer, aChild, aIndexInContainer);
  }
}

void nsDocument::ContentHasBeenRemoved(nsIContent* aContainer,
                                       nsIContent* aChild,
                                       PRInt32 aIndexInContainer)
{
  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; i++) {
    nsIDocumentObserver*  observer = (nsIDocumentObserver*)mObservers[i];
    observer->ContentHasBeenRemoved(aContainer, aChild, aIndexInContainer);
  }
}

nsresult nsDocument::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;
  nsIScriptGlobalObject *global = aContext->GetGlobalObject();

  if (nsnull == mScriptObject) {
    res = NS_NewScriptDocument(aContext, this, global, (void**)&mScriptObject);
  }
  *aScriptObject = mScriptObject;

  NS_RELEASE(global);
  return res;
}

nsresult nsDocument::ResetScriptObject()
{
  mScriptObject = nsnull;
  return NS_OK;
}


//
// nsIDOMDocument interface
//
NS_IMETHODIMP
nsDocument::GetMasterDoc(nsIDOMDocument **aDocument)
{
  AddRef();
  *aDocument = (nsIDOMDocument*)this;
  return NS_OK;
}

NS_IMETHODIMP    
nsDocument::GetDocumentType(nsIDOMDocumentType** aDocumentType)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::GetProlog(nsIDOMNodeList** aProlog)
{
  *aProlog = nsnull;
  return NS_OK;
}

NS_IMETHODIMP    
nsDocument::GetEpilog(nsIDOMNodeList** aEpilog)
{
  *aEpilog = nsnull;
  return NS_OK;
}

NS_IMETHODIMP    
nsDocument::GetDocumentElement(nsIDOMElement** aDocumentElement)
{
  nsresult res = NS_ERROR_FAILURE;

  if (nsnull != mRootContent) {
    res = mRootContent->QueryInterface(kIDOMElementIID, (void**)aDocumentElement);
    NS_ASSERTION(NS_OK == res, "Must be a DOM Element");
  }
  
  return res;
}

#if 0
NS_IMETHODIMP
nsDocument::SetDocumentElement(nsIDOMElement *aElement)
{
  NS_PRECONDITION(nsnull != aElement, "null arg");
  nsIContent* content;
  
  nsresult res = aElement->QueryInterface(kIContentIID, (void**)&content);
  if (NS_OK == res) {
    SetRootContent(content);
    NS_RELEASE(content);
  }
  else NS_ASSERTION(0, "Must be an nsIContent"); // nsIContent not supported. Who are you?

  return res;
}
#endif

NS_IMETHODIMP    
nsDocument::CreateElement(const nsString& aTagName, 
                              nsIDOMNamedNodeMap* aAttributes, 
                              nsIDOMElement** aReturn)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}
 
NS_IMETHODIMP
nsDocument::CreateTextNode(const nsString& aData, nsIDOMText** aReturn)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::CreateDocumentFragment(nsIDOMDocumentFragment** aReturn)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::CreateComment(const nsString& aData, nsIDOMComment** aReturn)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::CreateProcessingInstruction(const nsString& aTarget, 
                                        const nsString& aData, 
                                        nsIDOMProcessingInstruction** aReturn)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::CreateAttribute(const nsString& aName, 
                            nsIDOMNode* aValue, 
                            nsIDOMAttribute** aReturn)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::GetElementsByTagName(const nsString& aTagname, 
                                 nsIDOMNodeList** aReturn)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

//
// nsIDOMNode methods
//
NS_IMETHODIMP    
nsDocument::GetNodeName(nsString& aNodeName)
{
  // XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::GetNodeValue(nsString& aNodeValue)
{
  // XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::SetNodeValue(const nsString& aNodeValue)
{
  // XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::GetNodeType(PRInt32* aNodeType)
{
  // XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::GetParentNode(nsIDOMNode** aParentNode)
{
  // XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
  // XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::GetHasChildNodes(PRBool* aHasChildNodes)
{
  // XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::GetFirstChild(nsIDOMNode** aFirstChild)
{
  // XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::GetLastChild(nsIDOMNode** aLastChild)
{
  // XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::GetPreviousSibling(nsIDOMNode** aPreviousSibling)
{
  // XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::GetNextSibling(nsIDOMNode** aNextSibling)
{
  // XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
  // XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn)
{
  // XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  // XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  // XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::CloneNode(nsIDOMNode** aReturn)
{
  // XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP    
nsDocument::Equals(nsIDOMNode* aNode, PRBool aDeep, PRBool* aReturn)
{
  // XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsDocument::GetListenerManager(nsIEventListenerManager **aInstancePtrResult)
{
  if (nsnull != mListenerManager) {
    return mListenerManager->QueryInterface(kIEventListenerManagerIID, (void**) aInstancePtrResult);;
  }
  else {
    nsIEventListenerManager* l = new nsEventListenerManager();

    if (nsnull == l) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    
    if (NS_OK == l->QueryInterface(kIEventListenerManagerIID, (void**) aInstancePtrResult)) {
      mListenerManager = l;
      return NS_OK;
    }

    return NS_ERROR_FAILURE;
  }
}

nsresult nsDocument::HandleDOMEvent(nsIPresContext& aPresContext, 
                                 nsGUIEvent* aEvent, 
                                 nsIDOMEvent* aDOMEvent,
                                 nsEventStatus& aEventStatus)
{
  //Capturing stage
  
  //Local handling stage
  if (nsnull != mListenerManager) {
    mListenerManager->HandleEvent(aPresContext, aEvent, aDOMEvent, aEventStatus);
  }

  //Bubbling stage
  /*Need to go to window here*/

  return NS_OK;
}

nsresult nsDocument::AddEventListener(nsIDOMEventListener *aListener, const nsIID& aIID)
{
  nsIEventListenerManager *manager;

  if (NS_OK == GetListenerManager(&manager)) {
    manager->AddEventListener(aListener, aIID);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult nsDocument::RemoveEventListener(nsIDOMEventListener *aListener, const nsIID& aIID)
{
  nsIEventListenerManager *manager;

  if (NS_OK == GetListenerManager(&manager)) {
    manager->RemoveEventListener(aListener, aIID);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult nsDocument::CaptureEvent(nsIDOMEventListener *aListener)
{
  nsIEventListenerManager *manager;

  if (NS_OK == GetListenerManager(&manager)) {
    manager->CaptureEvent(aListener);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult nsDocument::ReleaseEvent(nsIDOMEventListener *aListener)
{
  nsIEventListenerManager *manager;

  if (NS_OK == GetListenerManager(&manager)) {
    manager->ReleaseEvent(aListener);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}



/**
  * Returns the Selection Object
 */
nsISelection * nsDocument::GetSelection() {
  if (mSelection != nsnull) {
    NS_ADDREF(mSelection);
  }
  return mSelection;
}
 
/**
  * Selects all the Content
 */
void nsDocument::SelectAll() {
  nsIContent * start = mRootContent;
  nsIContent * end   = mRootContent;

  // Find Very first Piece of Content
  while (start->ChildCount() > 0) {
    start = start->ChildAt(0);
  }

  // Last piece of Content
  PRInt32 count = end->ChildCount();
  while (count > 0) {
    end = end->ChildAt(count-1);
    count = end->ChildCount();
  }
  nsSelectionRange * range    = mSelection->GetRange();
  nsSelectionPoint * startPnt = range->GetStartPoint();
  nsSelectionPoint * endPnt   = range->GetEndPoint();
  startPnt->SetPoint(start, -1, PR_TRUE);
  endPnt->SetPoint(end, -1, PR_FALSE);

}

void nsDocument::TraverseTree(nsString   & aText,  
                              nsIContent * aContent, 
                              nsIContent * aStart, 
                              nsIContent * aEnd, 
                              PRBool     & aInRange) {
  
  if (aContent == aStart) {
    aInRange = PR_TRUE;
  } 

  PRBool addReturn = PR_FALSE;
  if (aInRange) {
    nsIDOMText * textContent;
    nsresult status = aContent->QueryInterface(kIDOMTextIID, (void**) &textContent);
    if (NS_OK == status) {
      nsString text;
      textContent->GetData(text);
      aText.Append(text);
      //NS_IF_RELEASE(textContent);
    } else {
      nsIAtom * atom = aContent->GetTag();
      if (atom != nsnull) {
        nsString str;
        atom->ToString(str);
        //char * s = str.ToNewCString();
        if (str.Equals("P") || 
            str.Equals("OL") || 
            str.Equals("LI") || 
            str.Equals("TD") || 
            str.Equals("H1") || 
            str.Equals("H2") || 
            str.Equals("H3") || 
            str.Equals("H4") || 
            str.Equals("H5") || 
            str.Equals("H6") || 
            str.Equals("TABLE")) {
          addReturn = PR_TRUE;
        }
        //printf("[%s]\n", s);
        //delete s;
      }
    }
  }

  for (PRInt32 i=0;i<aContent->ChildCount();i++) {
    TraverseTree(aText, aContent->ChildAt(i), aStart, aEnd, aInRange);
  }
  if (addReturn) {
    aText.Append("\n");
  }
  if (aContent == aEnd) {
    aInRange = PR_FALSE;
  } 

}

void nsDocument::GetSelectionText(nsString & aText) {
  nsSelectionRange * range    = mSelection->GetRange();
  nsSelectionPoint * startPnt = range->GetStartPoint();
  nsSelectionPoint * endPnt   = range->GetEndPoint();

  PRBool inRange = PR_FALSE;
  TraverseTree(aText, mRootContent, startPnt->GetContent(), endPnt->GetContent(), inRange);
}


void nsDocument::CSSSelectorToXIF(nsXIFConverter& aConverter, nsCSSSelector& aSelector)
{
  nsString s;

  nsCSSSelector* next = aSelector.mNext;

  if (nsnull != next)
    CSSSelectorToXIF(aConverter,*next);

  aConverter.BeginCSSSelector();
 
  if (aSelector.mTag != nsnull)
  {
    aSelector.mTag->ToString(s);
    aConverter.AddCSSTag(s);
  }

  if (aSelector.mID != nsnull)
  {
    aSelector.mID->ToString(s);
    aConverter.AddCSSID(s);
  }
  
  if (aSelector.mClass != nsnull)
  {
    aSelector.mClass->ToString(s);
    aConverter.AddCSSClass(s);
  }
  
  if (aSelector.mPseudoClass != nsnull)
  {
    aSelector.mPseudoClass->ToString(s);
    aConverter.AddCSSPsuedoClass(s);
  }
  aConverter.EndCSSSelector();

}


void nsDocument::CSSDeclarationToXIF(nsXIFConverter& aConverter, nsICSSDeclaration& aDeclaration)
{
  PRInt32    propId;
  nsCSSValue value;
  nsString   name;
  nsString   str;



  aConverter.BeginCSSDeclarationList();
  for (propId = 0; propId < PROP_MAX; propId++)
  {
    switch(propId)
    {
      case PROP_BACKGROUND:
      case PROP_BORDER:
      case PROP_CLIP:
      case PROP_FONT:
      case PROP_LIST_STYLE:
      case PROP_MARGIN:
      case PROP_PADDING:
      case PROP_BACKGROUND_POSITION:
      case PROP_BORDER_TOP:
      case PROP_BORDER_RIGHT:
      case PROP_BORDER_BOTTOM:
      case PROP_BORDER_LEFT:
      case PROP_BORDER_COLOR:
      case PROP_BORDER_STYLE:
      case PROP_BORDER_WIDTH:
        break;

      default:
        aDeclaration.GetValue(propId,value);
        if (value.GetUnit() != eHTMLUnit_Null)
        {
          aConverter.BeginCSSDeclaration();
          name = nsCSSProps::kNameTable[propId].name;
          value.ToCSSString(str,propId);
          aConverter.AddCSSDeclaration(name,str);
          aConverter.EndCSSDeclaration();
        }
    }
  }
  aConverter.EndCSSDeclarationList();
}


void nsDocument::StyleSheetsToXIF(nsXIFConverter& aConverter)
{
 
  PRInt32     count = GetNumberOfStyleSheets();
  nsIURL&      docURL = *mDocumentURL;

  for (PRInt32 index = 0; index < count; index++)
  {
    nsIStyleSheet*          sheet = GetStyleSheetAt(index);
    nsICSSStyleSheet*       cssSheet = nsnull;
    
    if (sheet != nsnull)
    {
      nsIURL& sheetURL = *sheet->GetURL();
      
      if (!(sheetURL == docURL))
        break;
      
      nsresult  isCss = sheet->QueryInterface(kICSSStyleSheetIID, (void**)&cssSheet);
      if ((isCss == NS_OK) && (cssSheet != nsnull))
      {
        PRInt32           ruleCount = cssSheet->StyleRuleCount();
        PRInt32           ruleIndex;
        nsICSSStyleRule*  rule = nsnull;

        aConverter.BeginCSSStyleSheet();
        for (ruleIndex = 0; ruleIndex < ruleCount; ruleIndex++)
        {
          if (NS_OK == cssSheet->GetStyleRuleAt(ruleIndex, rule))
          {
            aConverter.BeginCSSRule();

              if (nsnull != rule)
              {
                nsCSSSelector* selector = rule->FirstSelector();
          
                if (nsnull != selector)
                  CSSSelectorToXIF(aConverter,*selector);
  
                nsICSSDeclaration* declaration = rule->GetDeclaration();
                if (nsnull != declaration)
                  CSSDeclarationToXIF(aConverter,*declaration);

                NS_IF_RELEASE(declaration);
                NS_IF_RELEASE(rule);
              } // ruleAt

            aConverter.EndCSSRule();
          } // for loop
        }
        aConverter.EndCSSStyleSheet();
        NS_RELEASE(cssSheet);
      } // css_sheet
      NS_RELEASE(sheet);
    } // sheet
  }
}



void nsDocument::ToXIF(nsXIFConverter& aConverter, nsIDOMNode* aNode)
{
  nsIContent* content = nsnull;
  nsresult isContent = aNode->QueryInterface(kIContentIID, (void**)&content);
  nsIDOMElement* element = nsnull;
  nsresult isElement = aNode->QueryInterface(kIDOMElementIID, (void**)&element);
  PRBool isSynthetic = PR_TRUE;

  // Begin Conversion
  if (NS_OK == isContent) 
  {
    content->IsSynthetic(isSynthetic);
    if (PR_FALSE == isSynthetic)
    {
      content->BeginConvertToXIF(aConverter);
      content->DoConvertToXIF(aConverter);
    }
  }
  
  
  // Iterate through the children, convertion child nodes
  nsresult result = NS_OK;
  nsIDOMNode* node = nsnull;
  result = aNode->GetFirstChild(&node);
    
  while ((result == NS_OK) && (node != nsnull))
  { 
    nsIDOMNode* temp = node;
    ToXIF(aConverter,node);
    result = node->GetNextSibling(&node);
    NS_RELEASE(temp);
  }

  if (NS_OK == isContent && PR_FALSE == isSynthetic)
  {
    nsIAtom* tag = content->GetTag();
    if (tag != nsnull)
    {
      if (tag != nsnull) 
      {
        nsString str;
        tag->ToString(str);
        if (str.EqualsIgnoreCase("Head"))
          StyleSheetsToXIF(aConverter);
      }
    }
  }

  if (NS_OK == isContent) 
  {
    if (PR_FALSE == isSynthetic)
      content->FinishConvertToXIF(aConverter);
    NS_RELEASE(content);
  }
  if (NS_OK == isElement)
  {
    NS_RELEASE(element);
  }
}

void nsDocument::ToXIF(nsString & aBuffer, PRBool aUseSelection)
{
  
  nsXIFConverter  converter(aBuffer);
  // call the function

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

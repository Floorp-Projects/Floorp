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

NS_LAYOUT nsresult
NS_NewPostData(nsIPostData* aPostData, nsIPostData** aInstancePtrResult)
{
  *aInstancePtrResult = new nsPostData(aPostData);
  return NS_OK;
}

nsPostData::nsPostData(nsIPostData* aPostData) 
{
  mIsFile = PR_FALSE;
  mData = nsnull;
  if (aPostData) {
    mIsFile = aPostData->IsFile();
    const char* data = aPostData->GetData();
    if (data) {
      PRInt32 len = strlen(data);
      mData = new char[len+1];
      strcpy(mData, data);
    }
  }
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
  mListenerManager = nsnull;
 
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

// Note: We don't hold a reference to the document observer; we assume
// that it has a live reference to the document.
void nsDocument::AddObserver(nsIDocumentObserver* aObserver)
{
  mObservers.AppendElement(aObserver);
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

nsresult nsDocument::GetScriptObject(JSContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;

  if (nsnull == mScriptObject) {
    res = NS_NewScriptDocument(aContext, this, nsnull, (JSObject**)&mScriptObject);
  }
  *aScriptObject = mScriptObject;

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
nsresult nsDocument::GetNodeType(PRInt32 *aType)
{
  *aType = nsIDOMNode::DOCUMENT;
  return NS_OK;
}

nsresult nsDocument::GetParentNode(nsIDOMNode **aNode)
{
  return NS_ERROR_FAILURE;
}

nsresult nsDocument::GetChildNodes(nsIDOMNodeIterator **aIterator)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsDocument::HasChildNodes()
{
  if (nsnull != mRootContent) {
    return NS_OK;
  }
  else {
    return NS_ERROR_FAILURE;
  }
}

nsresult nsDocument::GetFirstChild(nsIDOMNode **aNode)
{
  if (nsnull != mRootContent) {
    nsresult res = mRootContent->QueryInterface(kIDOMNodeIID, (void**)aNode);
    NS_ASSERTION(NS_OK == res, "Must be a DOM Node");
    return res;
  }
  
  return NS_ERROR_FAILURE;
}

nsresult nsDocument::GetPreviousSibling(nsIDOMNode **aNode)
{
  // no siblings
  return NS_ERROR_FAILURE;
}

nsresult nsDocument::GetNextSibling(nsIDOMNode **aNode)
{
  // no siblings
  return NS_ERROR_FAILURE;
}

nsresult nsDocument::InsertBefore(nsIDOMNode *newChild, nsIDOMNode *refChild)
{
  // a document has only one child
  return NS_ERROR_FAILURE;
}

nsresult nsDocument::ReplaceChild(nsIDOMNode *newChild, nsIDOMNode *oldChild)
{
  NS_PRECONDITION(nsnull != newChild && nsnull != oldChild, "null arg");
  nsIContent* content;
  
  nsresult res = oldChild->QueryInterface(kIContentIID, (void**)&content);
  if (NS_OK == res) {
    // check that we are replacing the root content
    if (content == mRootContent) {
      nsIContent* newContent;
      res = newChild->QueryInterface(kIContentIID, (void**)&newContent);
      if (NS_OK == res) {
        SetRootContent(newContent);
        NS_RELEASE(newContent);
      }
      else NS_ASSERTION(0, "Must be an nsIContent"); // nsIContent not supported. Who are you?
    }

    NS_RELEASE(content);
  }
  else NS_ASSERTION(0, "Must be an nsIContent"); // nsIContent not supported. Who are you?

  return res;
}

nsresult nsDocument::RemoveChild(nsIDOMNode *oldChild)
{
  NS_PRECONDITION(nsnull != oldChild, "null arg");
  nsIContent* content;
  
  nsresult res = oldChild->QueryInterface(kIContentIID, (void**)&content);
  if (NS_OK == res) {
    if (content == mRootContent) {
      NS_RELEASE(mRootContent);
      mRootContent = nsnull;
    }
  }

  return res;
}

nsresult nsDocument::GetMasterDoc(nsIDOMDocument **aDocument)
{
  AddRef();
  *aDocument = (nsIDOMDocument*)this;
  return NS_OK;
}

nsresult nsDocument::SetMasterDoc(nsIDOMDocument *aDocument)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsDocument::GetDocumentType(nsIDOMNode **aDocType)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsDocument::SetDocumentType(nsIDOMNode *aNode)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsDocument::GetDocumentElement(nsIDOMElement **aElement)        
{
  nsresult res = NS_ERROR_FAILURE;

  if (nsnull != mRootContent) {
    res = mRootContent->QueryInterface(kIDOMElementIID, (void**)aElement);
    NS_ASSERTION(NS_OK == res, "Must be a DOM Element");
  }
  
  return res;
}

nsresult nsDocument::SetDocumentElement(nsIDOMElement *aElement)
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

nsresult nsDocument::GetDocumentContext(nsIDOMDocumentContext **aDocContext)       
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsDocument::SetDocumentContext(nsIDOMDocumentContext *aContext)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsDocument::CreateDocumentContext(nsIDOMDocumentContext **aDocContext)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsDocument::CreateElement(nsString &aTagName, 
                                   nsIDOMAttributeList *aAttributes, 
                                   nsIDOMElement **aElement)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}
  
nsresult nsDocument::CreateTextNode(nsString &aData, nsIDOMText** aTextNode)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsDocument::CreateComment(nsString &aData, nsIDOMComment **aComment)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsDocument::CreatePI(nsString &aName, nsString &aData, nsIDOMPI **aPI)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsDocument::CreateAttribute(nsString &aName, 
                                      nsIDOMNode *value, 
                                      nsIDOMAttribute **aAttribute)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsDocument::CreateAttributeList(nsIDOMAttributeList **aAttributesList)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsDocument::CreateTreeIterator(nsIDOMNode **aNode, nsIDOMTreeIterator **aTreeIterator)
{
  //XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsDocument::GetElementsByTagName(nsString &aTagname, nsIDOMNodeIterator **aIterator)
{
  //XXX TBI
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
                                 nsEventStatus& aEventStatus)
{
  //Capturing stage
  
  //Local handling stage
  if (nsnull != mListenerManager) {
    mListenerManager->HandleEvent(aPresContext, aEvent, aEventStatus);
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

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
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDocumentObserver.h"
#include "nsIStyleSet.h"
#include "nsICSSStyleSheet.h" // XXX for UA sheet loading hack, can this go away please?
#include "nsIStyleContext.h"
#include "nsFrame.h"
#include "nsIReflowCommand.h"
#include "nsIViewManager.h"
#include "nsCRT.h"
#include "plhash.h"
#include "prlog.h"
#include "prthread.h"
#include "prinrval.h"
#include "nsVoidArray.h"
#include "nsIPref.h"
#include "nsIViewObserver.h"
#include "nsContainerFrame.h"
#include "nsHTMLIIDs.h"
#include "nsIDeviceContext.h"
#include "nsIEventStateManager.h"
#include "nsDOMEvent.h"
#include "nsHTMLParts.h"
#include "nsIDOMSelection.h"
#include "nsLayoutCID.h"
#include "nsIDOMRange.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsHTMLAtoms.h"
#include "nsCOMPtr.h"
#include "nsIEventQueueService.h"
#include "nsXPComCIID.h"
#include "nsIServiceManager.h"
#include "nsICaret.h"
#include "nsCaretProperties.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIXMLDocument.h"
#include "nsIScrollableView.h"
#include "nsIDOMSelectionListener.h"
#include "nsISelectionMgr.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsHTMLContentSinkStream.h"
#include "nsHTMLToTXTSinkStream.h"
#include "nsXIFDTD.h"
#include "nsIFrameSelection.h"


static PRBool gsNoisyRefs = PR_FALSE;
#undef NOISY

// comment out to hide caret
#define SHOW_CARET

static PLHashNumber
HashKey(nsIFrame* key)
{
  return (PLHashNumber) key;
}

static PRIntn
CompareKeys(nsIFrame* key1, nsIFrame* key2)
{
  return key1 == key2;
}

class FrameHashTable {
public:
  FrameHashTable();
  ~FrameHashTable();

  void* Get(nsIFrame* aKey);
  void* Put(nsIFrame* aKey, void* aValue);
  void* Remove(nsIFrame* aKey);

protected:
  PLHashTable* mTable;
};

FrameHashTable::FrameHashTable()
{
  mTable = PL_NewHashTable(8, (PLHashFunction) HashKey,
                           (PLHashComparator) CompareKeys,
                           (PLHashComparator) nsnull,
                           nsnull, nsnull);
}

FrameHashTable::~FrameHashTable()
{
  PL_HashTableDestroy(mTable);
}

/**
 * Get the data associated with a frame.
 */
void*
FrameHashTable::Get(nsIFrame* aKey)
{
  PRInt32 hashCode = (PRInt32) aKey;
  PLHashEntry** hep = PL_HashTableRawLookup(mTable, hashCode, aKey);
  PLHashEntry* he = *hep;
  if (nsnull != he) {
    return he->value;
  }
  return nsnull;
}

/**
 * Create an association between a frame and some data. This call
 * returns an old association if there was one (or nsnull if there
 * wasn't).
 */
void*
FrameHashTable::Put(nsIFrame* aKey, void* aData)
{
  PRInt32 hashCode = (PRInt32) aKey;
  PLHashEntry** hep = PL_HashTableRawLookup(mTable, hashCode, aKey);
  PLHashEntry* he = *hep;
  if (nsnull != he) {
    void* oldValue = he->value;
    he->value = aData;
    return oldValue;
  }
  PL_HashTableRawAdd(mTable, hep, hashCode, aKey, aData);
  return nsnull;
}

/**
 * Remove an association between a frame and it's data. This returns
 * the old associated data.
 */
void*
FrameHashTable::Remove(nsIFrame* aKey)
{
  PRInt32 hashCode = (PRInt32) aKey;
  PLHashEntry** hep = PL_HashTableRawLookup(mTable, hashCode, aKey);
  PLHashEntry* he = *hep;
  void* oldValue = nsnull;
  if (nsnull != he) {
    oldValue = he->value;
    PL_HashTableRawRemove(mTable, hep, he);
  }
  return oldValue;
}

//----------------------------------------------------------------------

// Class IID's
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kRangeListCID, NS_RANGELIST_CID);
static NS_DEFINE_IID(kCRangeCID, NS_RANGE_CID);

// IID's
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIPresShellIID, NS_IPRESSHELL_IID);
static NS_DEFINE_IID(kIDocumentObserverIID, NS_IDOCUMENT_OBSERVER_IID);
static NS_DEFINE_IID(kIViewObserverIID, NS_IVIEWOBSERVER_IID);
static NS_DEFINE_IID(kIFrameSelectionIID, NS_IFRAMESELECTION_IID);
static NS_DEFINE_IID(kIDOMSelectionIID, NS_IDOMSELECTION_IID);
static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDOMRangeIID, NS_IDOMRANGE_IID);
static NS_DEFINE_IID(kIDOMDocumentIID, NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIFocusTrackerIID, NS_IFOCUSTRACKER_IID);
static NS_DEFINE_IID(kIDomSelectionListenerIID, NS_IDOMSELECTIONLISTENER_IID);
static NS_DEFINE_IID(kIEventQueueServiceIID,  NS_IEVENTQUEUESERVICE_IID);
static NS_DEFINE_IID(kICaretIID, NS_ICARET_IID);
static NS_DEFINE_IID(kICaretID,  NS_ICARET_IID);
static NS_DEFINE_IID(kIDOMHTMLDocumentIID, NS_IDOMHTMLDOCUMENT_IID);
static NS_DEFINE_IID(kIXMLDocumentIID, NS_IXMLDOCUMENT_IID);
static NS_DEFINE_IID(kIContentIID, NS_ICONTENT_IID);
static NS_DEFINE_IID(kIScrollableViewIID, NS_ISCROLLABLEVIEW_IID);

class PresShell : public nsIPresShell, public nsIViewObserver,
                  private nsIDocumentObserver, public nsIFocusTracker,
                  public nsIDOMSelectionListener
{
public:
  PresShell();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDocumentObserver
  NS_IMETHOD BeginUpdate(nsIDocument *aDocument);
  NS_IMETHOD EndUpdate(nsIDocument *aDocument);
  NS_IMETHOD BeginLoad(nsIDocument *aDocument);
  NS_IMETHOD EndLoad(nsIDocument *aDocument);
  NS_IMETHOD BeginReflow(nsIDocument *aDocument, nsIPresShell* aShell);
  NS_IMETHOD EndReflow(nsIDocument *aDocument, nsIPresShell* aShell);
  NS_IMETHOD ContentChanged(nsIDocument *aDocument,
                            nsIContent* aContent,
                            nsISupports* aSubContent);
  NS_IMETHOD ContentStateChanged(nsIDocument* aDocument,
                                 nsIContent* aContent);
  NS_IMETHOD AttributeChanged(nsIDocument *aDocument,
                              nsIContent*  aContent,
                              nsIAtom*     aAttribute,
                              PRInt32      aHint);
  NS_IMETHOD ContentAppended(nsIDocument *aDocument,
                             nsIContent* aContainer,
                             PRInt32     aNewIndexInContainer);
  NS_IMETHOD ContentInserted(nsIDocument *aDocument,
                             nsIContent* aContainer,
                             nsIContent* aChild,
                             PRInt32 aIndexInContainer);
  NS_IMETHOD ContentReplaced(nsIDocument *aDocument,
                             nsIContent* aContainer,
                             nsIContent* aOldChild,
                             nsIContent* aNewChild,
                             PRInt32 aIndexInContainer);
  NS_IMETHOD ContentRemoved(nsIDocument *aDocument,
                            nsIContent* aContainer,
                            nsIContent* aChild,
                            PRInt32 aIndexInContainer);
  NS_IMETHOD StyleSheetAdded(nsIDocument *aDocument,
                             nsIStyleSheet* aStyleSheet);
  NS_IMETHOD StyleSheetRemoved(nsIDocument *aDocument,
                               nsIStyleSheet* aStyleSheet);
  NS_IMETHOD StyleSheetDisabledStateChanged(nsIDocument *aDocument,
                                            nsIStyleSheet* aStyleSheet,
                                            PRBool aDisabled);
  NS_IMETHOD StyleRuleChanged(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule,
                              PRInt32 aHint);
  NS_IMETHOD StyleRuleAdded(nsIDocument *aDocument,
                            nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule);
  NS_IMETHOD StyleRuleRemoved(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule);
  NS_IMETHOD DocumentWillBeDestroyed(nsIDocument *aDocument);

  // nsIPresShell
  NS_IMETHOD Init(nsIDocument* aDocument,
                  nsIPresContext* aPresContext,
                  nsIViewManager* aViewManager,
                  nsIStyleSet* aStyleSet);
  NS_IMETHOD GetDocument(nsIDocument** aResult);
  NS_IMETHOD GetPresContext(nsIPresContext** aResult);
  NS_IMETHOD GetViewManager(nsIViewManager** aResult);
  NS_IMETHOD GetStyleSet(nsIStyleSet** aResult);
  NS_IMETHOD GetActiveAlternateStyleSheet(nsString& aSheetTitle);
  NS_IMETHOD SelectAlternateStyleSheet(const nsString& aSheetTitle);
  NS_IMETHOD ListAlternateStyleSheets(nsStringArray& aTitleList);
  NS_IMETHOD GetSelection(nsIDOMSelection** aSelection);
  NS_IMETHOD EnterReflowLock();
  NS_IMETHOD ExitReflowLock();
  NS_IMETHOD BeginObservingDocument();
  NS_IMETHOD EndObservingDocument();
  NS_IMETHOD InitialReflow(nscoord aWidth, nscoord aHeight);
  NS_IMETHOD ResizeReflow(nscoord aWidth, nscoord aHeight);
  NS_IMETHOD StyleChangeReflow();
  NS_IMETHOD GetRootFrame(nsIFrame** aFrame) const;
  NS_IMETHOD GetPageSequenceFrame(nsIPageSequenceFrame** aResult) const;
  NS_IMETHOD GetPrimaryFrameFor(nsIContent* aContent,
                                nsIFrame**  aPrimaryFrame) const;
  NS_IMETHOD GetStyleContextFor(nsIFrame*         aFrame,
                                nsIStyleContext** aStyleContext) const;
  NS_IMETHOD GetLayoutObjectFor(nsIContent*   aContent,
                                nsISupports** aResult) const;
  NS_IMETHOD GetPlaceholderFrameFor(nsIFrame*  aFrame,
                                    nsIFrame** aPlaceholderFrame) const;
  NS_IMETHOD SetPlaceholderFrameFor(nsIFrame* aFrame,
                                    nsIFrame* aPlaceholderFrame);
  NS_IMETHOD AppendReflowCommand(nsIReflowCommand* aReflowCommand);
  NS_IMETHOD ProcessReflowCommands();
  NS_IMETHOD ClearFrameRefs(nsIFrame* aFrame);
  NS_IMETHOD CreateRenderingContext(nsIFrame *aFrame,
                                    nsIRenderingContext** aContext);
  NS_IMETHOD CantRenderReplacedElement(nsIPresContext* aPresContext,
                                       nsIFrame*       aFrame);
  NS_IMETHOD GoToAnchor(const nsString& aAnchorName) const;

  NS_IMETHOD ScrollFrameIntoView(nsIFrame *aFrame,
                                 PRInt32   aVOffsetPercent, 
                                 PRUint32  aVFlags,
                                 PRInt32   aHOffsetPercent, 
                                 PRUint32  aHFlags) const;
  NS_IMETHOD DoCopy(nsISelectionMgr* aSelectionMgr);

  //nsIViewObserver interface

  NS_IMETHOD Paint(nsIView *aView,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect);
  NS_IMETHOD HandleEvent(nsIView*        aView,
                         nsGUIEvent*     aEvent,
                         nsEventStatus&  aEventStatus);
  NS_IMETHOD Scrolled(nsIView *aView);
  NS_IMETHOD ResizeReflow(nsIView *aView, nscoord aWidth, nscoord aHeight);

  //nsIFocusTracker interface
  NS_IMETHOD SetFocus(nsIFrame *aFrame, nsIFrame *aAnchorFrame);

  NS_IMETHOD GetFocus(nsIFrame **aFrame, nsIFrame **aAnchorFrame);

  NS_IMETHOD ScrollFrameIntoView(nsIFrame *aFrame);
  // caret handling
  NS_IMETHOD GetCaret(nsICaret **outCaret);
  NS_IMETHOD RefreshCaret();

  // nsIDOMSelectionListener interface
  NS_IMETHOD NotifySelectionChanged();

  // implementation
  void HandleCantRenderReplacedElementEvent(nsIFrame* aFrame);

protected:
  virtual ~PresShell();

  nsresult ReconstructFrames(void);

  // turn the caret on and off.
  nsresult EnableCaret();
  nsresult DisableCaret();
    
#ifdef NS_DEBUG
  void VerifyIncrementalReflow();
  PRBool mInVerifyReflow;
#endif

  nsIDocument* mDocument;
  nsIPresContext* mPresContext;
  nsIStyleSet* mStyleSet;
  nsIFrame* mRootFrame;
  nsIViewManager* mViewManager;
  PRUint32 mUpdateCount;
  nsVoidArray mReflowCommands;
  PRUint32 mReflowLockCount;
  PRBool mIsDestroying;
  nsIFrame* mCurrentEventFrame;
  nsIFrame* mFocusEventFrame; //keeps track of which frame has focus. 
  nsIFrame* mAnchorEventFrame; //keeps track of which frame has focus. 
  
  nsCOMPtr<nsIFrameSelection>   mSelection;
  nsCOMPtr<nsICaret>            mCaret;
  PRBool                        mScrollingEnabled; //used to disable programmable scrolling from outside
  FrameHashTable*               mPlaceholderMap;
private:
  //helper funcs for disabing autoscrolling
  void DisableScrolling(){mScrollingEnabled = PR_FALSE;}
  void EnableScrolling(){mScrollingEnabled = PR_TRUE;}
  PRBool IsScrollingEnabled(){return mScrollingEnabled;}
};

#ifdef NS_DEBUG
/**
 * Note: the log module is created during library initialization which
 * means that you cannot perform logging before then.
 */
static PRLogModuleInfo* gLogModule;
#endif

static PRBool gVerifyReflow = PRBool(0x55);
static PRBool gVerifyReflowAll;

NS_LAYOUT PRBool
nsIPresShell::GetVerifyReflowEnable()
{
#ifdef NS_DEBUG
  if (gVerifyReflow == PRBool(0x55)) {
    gLogModule = PR_NewLogModule("verifyreflow");
    gVerifyReflow = 0 != gLogModule->level;
    if (gLogModule->level > 1) {
      gVerifyReflowAll = PR_TRUE;
    }
    printf("Note: verifyreflow is %sabled",
           gVerifyReflow ? "en" : "dis");
    if (gVerifyReflowAll) {
      printf(" (diff all enabled)\n");
    }
    else {
      printf("\n");
    }
  }
#endif
  return gVerifyReflow;
}

NS_LAYOUT void
nsIPresShell::SetVerifyReflowEnable(PRBool aEnabled)
{
  gVerifyReflow = aEnabled;
}

//----------------------------------------------------------------------

NS_LAYOUT nsresult
NS_NewPresShell(nsIPresShell** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  PresShell* it = new PresShell();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIPresShellIID, (void **) aInstancePtrResult);
}

PresShell::PresShell()
{
  //XXX joki 11/17 - temporary event hack.
  mIsDestroying = PR_FALSE;
  EnableScrolling();
}

#ifdef NS_DEBUG
// for debugging only
nsrefcnt PresShell::AddRef(void)
{
  if (gsNoisyRefs) {
    printf("PresShell: AddRef: %p, cnt = %d \n",this, mRefCnt+1);
  }
  return ++mRefCnt;
}

// for debugging only
nsrefcnt PresShell::Release(void)
{
  if (gsNoisyRefs) {
    printf("PresShell Release: %p, cnt = %d \n",this, mRefCnt-1);
  }
  if (--mRefCnt == 0) {
    if (gsNoisyRefs) {
      printf("PresShell Delete: %p, \n",this);
    }
    delete this;
    return 0;
  }
  return mRefCnt;
}
#else
NS_IMPL_ADDREF(PresShell)
NS_IMPL_RELEASE(PresShell)
#endif

nsresult
PresShell::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (aIID.Equals(kIPresShellIID)) {
    nsIPresShell* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDocumentObserverIID)) {
    nsIDocumentObserver* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIViewObserverIID)) {
    nsIViewObserver* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIFocusTrackerIID)) {
    nsIFocusTracker* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDomSelectionListenerIID)) {
    nsIDOMSelectionListener* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    nsIPresShell* tmp = this;
    nsISupports* tmp2 = tmp;
    *aInstancePtr = (void*) tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

PresShell::~PresShell()
{
  mRefCnt = 99;/* XXX hack! get around re-entrancy bugs */
  mIsDestroying = PR_TRUE;
  if (nsnull != mRootFrame) {
    mRootFrame->DeleteFrame(*mPresContext);
  }
  NS_IF_RELEASE(mViewManager);
  //Release mPresContext after mViewManager
  NS_IF_RELEASE(mPresContext);
  NS_IF_RELEASE(mStyleSet);
  if (nsnull != mDocument) {
    mDocument->DeleteShell(this);
    NS_RELEASE(mDocument);
  }
  mRefCnt = 0;
  delete mPlaceholderMap;
}

/**
 * Initialize the presentation shell. Create view manager and style
 * manager.
 */
nsresult
PresShell::Init(nsIDocument* aDocument,
                nsIPresContext* aPresContext,
                nsIViewManager* aViewManager,
                nsIStyleSet* aStyleSet)
{
  NS_PRECONDITION(nsnull != aDocument, "null ptr");
  NS_PRECONDITION(nsnull != aPresContext, "null ptr");
  NS_PRECONDITION(nsnull != aViewManager, "null ptr");
  if ((nsnull == aDocument) || (nsnull == aPresContext) ||
      (nsnull == aViewManager)) {
    return NS_ERROR_NULL_POINTER;
  }
  if (nsnull != mDocument) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  mDocument = aDocument;
  NS_ADDREF(aDocument);
  mViewManager = aViewManager;
  NS_ADDREF(mViewManager);

  //doesn't add a ref since we own it... MMP
  mViewManager->SetViewObserver((nsIViewObserver *)this);

  // Bind the context to the presentation shell.
  mPresContext = aPresContext;
  NS_ADDREF(aPresContext);
  aPresContext->SetShell(this);

  mStyleSet = aStyleSet;
  NS_ADDREF(aStyleSet);

  nsCOMPtr<nsIDOMSelection>domselection;
  nsresult result = nsComponentManager::CreateInstance(kRangeListCID, nsnull,
                                                 kIDOMSelectionIID,
                                                 getter_AddRefs(domselection));
  if (!NS_SUCCEEDED(result))
    return result;
  result = domselection->QueryInterface(kIFrameSelectionIID,
                                        getter_AddRefs(mSelection));
  if (!NS_SUCCEEDED(result))
    return result;
  domselection->AddSelectionListener(this);//possible circular reference
  // XXX This code causes the document object (and the entire content model) to be leaked...
#if 0
  nsCOMPtr<nsIDOMRange>range;
  if (NS_SUCCEEDED(nsComponentManager::CreateInstance(kCRangeCID, nsnull, kIDOMRangeIID, getter_AddRefs(range)))){ //create an irange
    nsCOMPtr<nsIDocument>doc(GetDocument());
    nsCOMPtr<nsIDOMDocument>domDoc(doc);
    if (domDoc){
      nsCOMPtr<nsIDOMElement> domElement;
      if (NS_SUCCEEDED(domDoc->GetDocumentElement(getter_AddRefs(domElement)))) {//get the first element from the dom
        nsCOMPtr<nsIDOMNode>domNode(domElement);
        if (domNode) {//get the node interface for the range object
          range->SetStart(domNode,0);
          range->SetEnd(domNode,0);
          nsCOMPtr<nsISupports>rangeISupports(range);
          if (rangeISupports) {
            selection->AddItem(rangeISupports);
          }
        }
      }
    }
  }
 
#endif
  
  // Important: this has to happen after the selection has been set up
#ifdef SHOW_CARET
  nsCaretProperties  *caretProperties = NewCaretProperties();
  
  // make the caret
  nsresult  err = NS_NewCaret(getter_AddRefs(mCaret));
  if (NS_SUCCEEDED(err))
    mCaret->Init(this, caretProperties);
  
  delete caretProperties;
  caretProperties = nsnull;
#endif  

  return NS_OK;
}

NS_IMETHODIMP
PresShell::EnterReflowLock()
{
  ++mReflowLockCount;
  return NS_OK;
}

NS_IMETHODIMP
PresShell::ExitReflowLock()
{
  PRUint32 newReflowLockCount = mReflowLockCount - 1;
  if (newReflowLockCount == 0) {
    ProcessReflowCommands();
  }
  mReflowLockCount = newReflowLockCount;
  return NS_OK;
}

NS_IMETHODIMP
PresShell::GetDocument(nsIDocument** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mDocument;
  NS_IF_ADDREF(mDocument);
  return NS_OK;
}

NS_IMETHODIMP
PresShell::GetPresContext(nsIPresContext** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mPresContext;
  NS_IF_ADDREF(mPresContext);
  return NS_OK;
}

NS_IMETHODIMP
PresShell::GetViewManager(nsIViewManager** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mViewManager;
  NS_IF_ADDREF(mViewManager);
  return NS_OK;
}

NS_IMETHODIMP
PresShell::GetStyleSet(nsIStyleSet** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mStyleSet;
  NS_IF_ADDREF(mStyleSet);
  return NS_OK;
}

NS_IMETHODIMP
PresShell::GetActiveAlternateStyleSheet(nsString& aSheetTitle)
{ // first non-html sheet in style set that has title
  if (nsnull != mStyleSet) {
    PRInt32 count = mStyleSet->GetNumberOfDocStyleSheets();
    PRInt32 index;
    nsAutoString textHtml("text/html");
    for (index = 0; index < count; index++) {
      nsIStyleSheet* sheet = mStyleSet->GetDocStyleSheetAt(index);
      if (nsnull != sheet) {
        nsAutoString type;
        sheet->GetType(type);
        if (PR_FALSE == type.Equals(textHtml)) {
          nsAutoString title;
          sheet->GetTitle(title);
          if (0 < title.Length()) {
            aSheetTitle = title;
            index = count;  // stop looking
          }
        }
        NS_RELEASE(sheet);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
PresShell::SelectAlternateStyleSheet(const nsString& aSheetTitle)
{
  if ((nsnull != mDocument) && (nsnull != mStyleSet)) {
    PRInt32 count = mDocument->GetNumberOfStyleSheets();
    PRInt32 index;
    nsAutoString textHtml("text/html");
    for (index = 0; index < count; index++) {
      nsIStyleSheet* sheet = mDocument->GetStyleSheetAt(index);
      if (nsnull != sheet) {
        nsAutoString type;
        sheet->GetType(type);
        if (PR_FALSE == type.Equals(textHtml)) {
          nsAutoString  title;
          sheet->GetTitle(title);
          if (0 < title.Length()) {
            if (title.EqualsIgnoreCase(aSheetTitle)) {
              mStyleSet->AddDocStyleSheet(sheet, mDocument);
            }
            else {
              mStyleSet->RemoveDocStyleSheet(sheet);
            }
          }
        }
        NS_RELEASE(sheet);
      }
    }
    ReconstructFrames();
  }
  return NS_OK;
}

NS_IMETHODIMP
PresShell::ListAlternateStyleSheets(nsStringArray& aTitleList)
{
  if (nsnull != mDocument) {
    PRInt32 count = mDocument->GetNumberOfStyleSheets();
    PRInt32 index;
    nsAutoString textHtml("text/html");
    for (index = 0; index < count; index++) {
      nsIStyleSheet* sheet = mDocument->GetStyleSheetAt(index);
      if (nsnull != sheet) {
        nsAutoString type;
        sheet->GetType(type);
        if (PR_FALSE == type.Equals(textHtml)) {
          nsAutoString  title;
          sheet->GetTitle(title);
          if (0 < title.Length()) {
            if (-1 == aTitleList.IndexOfIgnoreCase(title)) {
              aTitleList.AppendString(title);
            }
          }
        }
        NS_RELEASE(sheet);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
PresShell::GetSelection(nsIDOMSelection **aSelection)
{
  if (!aSelection || !mSelection)
    return NS_ERROR_NULL_POINTER;
  return mSelection->QueryInterface(kIDOMSelectionIID,(void **)aSelection);
}



// Make shell be a document observer
NS_IMETHODIMP
PresShell::BeginObservingDocument()
{
  if (nsnull != mDocument) {
    mDocument->AddObserver(this);
  }
  return NS_OK;
}

// Make shell stop being a document observer
NS_IMETHODIMP
PresShell::EndObservingDocument()
{
  if (nsnull != mDocument) {
    mDocument->RemoveObserver(this);
  }
  if (mSelection){
    nsCOMPtr<nsIDOMSelection> domselection;
    nsresult result;
    domselection = do_QueryInterface(mSelection , &result);
    if (NS_FAILED(result))
      return result;
    if (!domselection)
      return NS_ERROR_UNEXPECTED;
    domselection->RemoveSelectionListener(this);
  }
  return NS_OK;
}

NS_IMETHODIMP
PresShell::InitialReflow(nscoord aWidth, nscoord aHeight)
{
  nsIContent* root = nsnull;

  DisableCaret();
  EnterReflowLock();

  if (nsnull != mPresContext) {
    nsRect r(0, 0, aWidth, aHeight);
    mPresContext->SetVisibleArea(r);
  }

  if (nsnull != mDocument) {
    root = mDocument->GetRootContent();
  }

  if (nsnull != root) {
    if (nsnull == mRootFrame) {
      // Have style sheet processor construct a frame for the
      // precursors to the root content object's frame
      mStyleSet->ConstructRootFrame(mPresContext, root, mRootFrame);
    }

    // Have the style sheet processor construct frame for the root
    // content object down
    mStyleSet->ContentInserted(mPresContext, nsnull, root, 0);
    NS_RELEASE(root);
  }

  if (nsnull != mRootFrame) {
    // Kick off a top-down reflow
    NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
                 ("enter nsPresShell::InitialReflow: %d,%d", aWidth, aHeight));
#ifdef NS_DEBUG
    if (nsIFrame::GetVerifyTreeEnable()) {
      mRootFrame->VerifyTree();
    }
#endif
    nsRect                bounds;
    mPresContext->GetVisibleArea(bounds);
    nsSize                maxSize(bounds.width, bounds.height);
    nsHTMLReflowMetrics   desiredSize(nsnull);
    nsReflowStatus        status;
    nsIHTMLReflow*        htmlReflow;
    nsIRenderingContext*  rcx = nsnull;

    CreateRenderingContext(mRootFrame, &rcx);

    nsHTMLReflowState reflowState(*mPresContext, mRootFrame,
                                  eReflowReason_Initial, rcx, maxSize);

    if (NS_OK == mRootFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow)) {
      htmlReflow->Reflow(*mPresContext, desiredSize, reflowState, status);
      mRootFrame->SizeTo(desiredSize.width, desiredSize.height);
#ifdef NS_DEBUG
      if (nsIFrame::GetVerifyTreeEnable()) {
        mRootFrame->VerifyTree();
      }
#endif
    }
    NS_IF_RELEASE(rcx);
    NS_FRAME_LOG(NS_FRAME_TRACE_CALLS, ("exit nsPresShell::InitialReflow"));
  }

  ExitReflowLock();
  EnableCaret();

  return NS_OK; //XXX this needs to be real. MMP
}

NS_IMETHODIMP
PresShell::ResizeReflow(nscoord aWidth, nscoord aHeight)
{
  DisableCaret();
  EnterReflowLock();

  if (nsnull != mPresContext) {
    nsRect r(0, 0, aWidth, aHeight);
    mPresContext->SetVisibleArea(r);
  }

  // If we don't have a root frame yet, that means we haven't had our initial
  // reflow...
  if (nsnull != mRootFrame) {
    // Kick off a top-down reflow
    NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
                 ("enter nsPresShell::ResizeReflow: %d,%d", aWidth, aHeight));
#ifdef NS_DEBUG
    if (nsIFrame::GetVerifyTreeEnable()) {
      mRootFrame->VerifyTree();
    }
#endif
    nsRect                bounds;
    mPresContext->GetVisibleArea(bounds);
    nsSize                maxSize(bounds.width, bounds.height);
    nsHTMLReflowMetrics   desiredSize(nsnull);
    nsReflowStatus        status;
    nsIHTMLReflow*        htmlReflow;
    nsIRenderingContext*  rcx = nsnull;

    CreateRenderingContext(mRootFrame, &rcx);

    nsHTMLReflowState reflowState(*mPresContext, mRootFrame,
                                  eReflowReason_Resize, rcx, maxSize);

    if (NS_OK == mRootFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow)) {
      htmlReflow->Reflow(*mPresContext, desiredSize, reflowState, status);
      mRootFrame->SizeTo(desiredSize.width, desiredSize.height);
#ifdef NS_DEBUG
      if (nsIFrame::GetVerifyTreeEnable()) {
        mRootFrame->VerifyTree();
      }
#endif
    }
    NS_IF_RELEASE(rcx);
    NS_FRAME_LOG(NS_FRAME_TRACE_CALLS, ("exit nsPresShell::ResizeReflow"));

    if (mSelection){
      DisableScrolling();
      mSelection->ResetSelection(this, mRootFrame);
      EnableScrolling();
    }

    // XXX if debugging then we should assert that the cache is empty
  } else {
#ifdef NOISY
    printf("PresShell::ResizeReflow: null root frame\n");
#endif
  }
  ExitReflowLock();
  EnableCaret();
  
  return NS_OK; //XXX this needs to be real. MMP
}

//it is ok to pass null, it will simply ignore that parameter.
//if necessary we can add a clear focus, but I dont think it is a big
//deal.
NS_IMETHODIMP
PresShell::SetFocus(nsIFrame *aFrame, nsIFrame *aAnchorFrame){
  if (aFrame)
    mFocusEventFrame = aFrame;
  if (aAnchorFrame)
    mAnchorEventFrame = aAnchorFrame;
  return NS_OK;
}

NS_IMETHODIMP
PresShell::GetFocus(nsIFrame **aFrame, nsIFrame **aAnchorFrame){
  if (!aFrame || !aAnchorFrame) 
    return NS_ERROR_NULL_POINTER;
  *aFrame = mFocusEventFrame;
  *aAnchorFrame = mAnchorEventFrame; 
  return NS_OK;
}

NS_IMETHODIMP PresShell::ScrollFrameIntoView(nsIFrame *aFrame){
  if (!aFrame)
    return NS_ERROR_NULL_POINTER;
  if (IsScrollingEnabled())
    return ScrollFrameIntoView(aFrame, 
           0, NS_PRESSHELL_SCROLL_TOP |NS_PRESSHELL_SCROLL_ANYWHERE,
           0, NS_PRESSHELL_SCROLL_LEFT|NS_PRESSHELL_SCROLL_ANYWHERE);
  return NS_OK;
}


NS_IMETHODIMP PresShell::GetCaret(nsICaret **outCaret)
{
  if (!outCaret || !mCaret)
    return NS_ERROR_NULL_POINTER;
  return mCaret->QueryInterface(kICaretIID,(void **)outCaret);
}

NS_IMETHODIMP PresShell::RefreshCaret()
{
  if (mCaret)
  	mCaret->Refresh();

  return NS_OK;
}

/*implementation of the nsIDOMSelectionListener
  it will invoke the resetselection to update the presentation shell's frames
*/
NS_IMETHODIMP PresShell::NotifySelectionChanged()
{
  if (!mSelection)
    return NS_ERROR_NULL_POINTER;
  mSelection->ResetSelection(this, mRootFrame);
  return NS_ERROR_NULL_POINTER;
}

nsresult PresShell::DisableCaret()
{
	if (mCaret)
		return mCaret->SetCaretVisible(PR_FALSE);
	return NS_OK;
}

nsresult PresShell::EnableCaret()
{
	if (mCaret)
		return mCaret->SetCaretVisible(PR_TRUE);
	return NS_OK;
}

NS_IMETHODIMP
PresShell::StyleChangeReflow()
{
  EnterReflowLock();

  if (nsnull != mRootFrame) {
    // Kick off a top-down reflow
    NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
                 ("enter nsPresShell::StyleChangeReflow"));
#ifdef NS_DEBUG
    if (nsIFrame::GetVerifyTreeEnable()) {
      mRootFrame->VerifyTree();
    }
#endif
    nsRect                bounds;
    mPresContext->GetVisibleArea(bounds);
    nsSize                maxSize(bounds.width, bounds.height);
    nsHTMLReflowMetrics   desiredSize(nsnull);
    nsReflowStatus        status;
    nsIHTMLReflow*        htmlReflow;
    nsIRenderingContext*  rcx = nsnull;

    CreateRenderingContext(mRootFrame, &rcx);

    // XXX We should be using eReflowReason_StyleChange
    nsHTMLReflowState reflowState(*mPresContext, mRootFrame,
                                  eReflowReason_Resize, rcx, maxSize);

    if (NS_OK == mRootFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow)) {
      htmlReflow->Reflow(*mPresContext, desiredSize, reflowState, status);
      mRootFrame->SizeTo(desiredSize.width, desiredSize.height);
#ifdef NS_DEBUG
      if (nsIFrame::GetVerifyTreeEnable()) {
        mRootFrame->VerifyTree();
      }
#endif
    }
    NS_IF_RELEASE(rcx);
    NS_FRAME_LOG(NS_FRAME_TRACE_CALLS, ("exit nsPresShell::StyleChangeReflow"));
  }

  ExitReflowLock();

  return NS_OK; //XXX this needs to be real. MMP
}

NS_IMETHODIMP
PresShell::GetRootFrame(nsIFrame** aResult) const
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mRootFrame;
  return NS_OK;
}

NS_IMETHODIMP
PresShell::GetPageSequenceFrame(nsIPageSequenceFrame** aResult) const
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIFrame*             child;
  nsIPageSequenceFrame* pageSequence;

  // The page sequence frame should be either the immediate child or
  // its child
  mRootFrame->FirstChild(nsnull, &child);
  if (nsnull != child) {
    if (NS_SUCCEEDED(child->QueryInterface(kIPageSequenceFrameIID, (void**)&pageSequence))) {
      *aResult = pageSequence;
      return NS_OK;
    }
  
    child->FirstChild(nsnull, &child);
    if (nsnull != child) {
      if (NS_SUCCEEDED(child->QueryInterface(kIPageSequenceFrameIID, (void**)&pageSequence))) {
        *aResult = pageSequence;
        return NS_OK;
      }
    }
  }
  *aResult = nsnull;

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PresShell::BeginUpdate(nsIDocument *aDocument)
{
  mUpdateCount++;
  return NS_OK;
}

NS_IMETHODIMP
PresShell::EndUpdate(nsIDocument *aDocument)
{
  NS_PRECONDITION(0 != mUpdateCount, "too many EndUpdate's");
  if (--mUpdateCount == 0) {
    // XXX do something here
  }
  return NS_OK;
}

NS_IMETHODIMP
PresShell::BeginLoad(nsIDocument *aDocument)
{
  return NS_OK;
}

NS_IMETHODIMP
PresShell::EndLoad(nsIDocument *aDocument)
{
  return NS_OK;
}

NS_IMETHODIMP
PresShell::BeginReflow(nsIDocument *aDocument, nsIPresShell* aShell)
{
  return NS_OK;
}

NS_IMETHODIMP
PresShell::EndReflow(nsIDocument *aDocument, nsIPresShell* aShell)
{
  return NS_OK;
}

NS_IMETHODIMP
PresShell::AppendReflowCommand(nsIReflowCommand* aReflowCommand)
{
#ifdef NS_DEBUG
  if (mInVerifyReflow) {
    return NS_OK;
  }
#endif
  NS_ADDREF(aReflowCommand);
  return mReflowCommands.AppendElement(aReflowCommand);
}

NS_IMETHODIMP
PresShell::ProcessReflowCommands()
{
  if (0 != mReflowCommands.Count()) {
    nsHTMLReflowMetrics   desiredSize(nsnull);
    nsIRenderingContext*  rcx;

    CreateRenderingContext(mRootFrame, &rcx);

    while (0 != mReflowCommands.Count()) {
      nsIReflowCommand* rc = (nsIReflowCommand*) mReflowCommands.ElementAt(0);
      mReflowCommands.RemoveElementAt(0);

      // Dispatch the reflow command
      nsSize          maxSize;
      mRootFrame->GetSize(maxSize);
#ifdef NS_DEBUG
      nsIReflowCommand::ReflowType type;
      rc->GetType(type);
      NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
         ("PresShell::ProcessReflowCommands: begin reflow command type=%d",
          type));
#endif
      rc->Dispatch(*mPresContext, desiredSize, maxSize, *rcx);
      NS_RELEASE(rc);
      NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
         ("PresShell::ProcessReflowCommands: end reflow command"));
    }
    NS_IF_RELEASE(rcx);

    // Place and size the root frame
    mRootFrame->SizeTo(desiredSize.width, desiredSize.height);

#ifdef NS_DEBUG
    if (nsIFrame::GetVerifyTreeEnable()) {
      mRootFrame->VerifyTree();
    }
    if (GetVerifyReflowEnable()) {
      // First synchronously render what we have so far so that we can
      // see it.
      if (gVerifyReflowAll) {
        printf("Before verify-reflow\n");
        nsIView* rootView;
        mViewManager->GetRootView(rootView);
        mViewManager->UpdateView(rootView, nsnull, NS_VMREFRESH_IMMEDIATE);
        PR_Sleep(PR_SecondsToInterval(3));
      }

      mInVerifyReflow = PR_TRUE;
      VerifyIncrementalReflow();
      mInVerifyReflow = PR_FALSE;
      if (gVerifyReflowAll) {
        printf("After verify-reflow\n");
      }

      if (0 != mReflowCommands.Count()) {
        printf("XXX yikes!\n");
      }
    }
#endif
  }

  return NS_OK;
}

NS_IMETHODIMP
PresShell::ClearFrameRefs(nsIFrame* aFrame)
{
  nsIEventStateManager *manager;
  if (NS_OK == mPresContext->GetEventStateManager(&manager)) {
    manager->ClearFrameRefs(aFrame);
    NS_RELEASE(manager);
  }
  if (aFrame == mCurrentEventFrame) {
    mCurrentEventFrame = nsnull;
  }
  if (aFrame == mFocusEventFrame) {
    mFocusEventFrame = nsnull;
  }
  if (aFrame == mAnchorEventFrame) {
    mAnchorEventFrame = nsnull;
  }
  return NS_OK;
}

NS_IMETHODIMP
PresShell::CreateRenderingContext(nsIFrame *aFrame,
                                  nsIRenderingContext** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIWidget *widget = nsnull;
  nsIView   *view = nsnull;
  nsPoint   pt;
  nsresult  rv;

  aFrame->GetView(&view);

  if (nsnull == view)
    aFrame->GetOffsetFromView(pt, &view);

  while (nsnull != view)
  {
    view->GetWidget(widget);

    if (nsnull != widget)
    {
      NS_RELEASE(widget);
      break;
    }

    view->GetParent(view);
  }

  nsCOMPtr<nsIDeviceContext> dx;

  nsIRenderingContext* result = nsnull;
  rv = mPresContext->GetDeviceContext(getter_AddRefs(dx));
  if (NS_SUCCEEDED(rv) && dx) {
    if (nsnull != view) {
      rv = dx->CreateRenderingContext(view, result);
    }
    else {
      rv = dx->CreateRenderingContext(result);
    }
  }
  *aResult = result;

  return rv;
}

void
PresShell::HandleCantRenderReplacedElementEvent(nsIFrame* aFrame)
{
  // Double-check that we haven't deleted the frame hierarchy
  // XXX If we stay with this model we approach, then we need to observe
  // aFrame and if it's deleted null out the pointer in the PL event struct
  if (nsnull != mRootFrame) {
    mStyleSet->CantRenderReplacedElement(mPresContext, aFrame);
    ProcessReflowCommands();
  }
}

struct CantRenderReplacedElementEvent : public PLEvent {
  CantRenderReplacedElementEvent(PresShell* aShell, nsIFrame* aFrame);
  ~CantRenderReplacedElementEvent();

  PresShell* mShell;
  nsIFrame*  mFrame;
};

static void PR_CALLBACK
HandlePLEvent(CantRenderReplacedElementEvent* aEvent)
{
  aEvent->mShell->HandleCantRenderReplacedElementEvent(aEvent->mFrame);
}

static void PR_CALLBACK
DestroyPLEvent(CantRenderReplacedElementEvent* aEvent)
{
  delete aEvent;
}

CantRenderReplacedElementEvent::CantRenderReplacedElementEvent(PresShell* aShell,
                                                               nsIFrame*  aFrame)
{
  mShell = aShell;
  NS_ADDREF(mShell);
  mFrame = aFrame;
  PL_InitEvent(this, nsnull, (PLHandleEventProc)::HandlePLEvent,
               (PLDestroyEventProc)::DestroyPLEvent);
}

CantRenderReplacedElementEvent::~CantRenderReplacedElementEvent()
{
  NS_RELEASE(mShell);
}

NS_IMETHODIMP
PresShell::CantRenderReplacedElement(nsIPresContext* aPresContext,
                                     nsIFrame*       aFrame)
{
#ifdef _WIN32
  nsIEventQueueService* eventService;
  nsresult              rv;
   
  // Notify the style set, but post the notification so it doesn't happen
  // now
  rv = nsServiceManager::GetService(kEventQueueServiceCID,
                                    kIEventQueueServiceIID,
                                    (nsISupports **)&eventService);
  if (NS_SUCCEEDED(rv)) {
    PLEventQueue* eventQueue;
    rv = eventService->GetThreadEventQueue(PR_GetCurrentThread(), 
                                           &eventQueue);
    nsServiceManager::ReleaseService(kEventQueueServiceCID, eventService);

    if (nsnull != eventQueue) {
      CantRenderReplacedElementEvent* ev;

      ev = new CantRenderReplacedElementEvent(this, aFrame);
      PL_PostEvent(eventQueue, ev);
    }
  }
  return rv;
#else
  return NS_OK;
#endif
}

NS_IMETHODIMP
PresShell::GoToAnchor(const nsString& aAnchorName) const
{
  nsCOMPtr<nsIDOMHTMLDocument> htmlDoc;
  nsCOMPtr<nsIXMLDocument> xmlDoc;
  nsresult                     rv;
  nsCOMPtr<nsIContent>  content;

  if (NS_SUCCEEDED(mDocument->QueryInterface(kIDOMHTMLDocumentIID,
                                             getter_AddRefs(htmlDoc)))) {    
    nsCOMPtr<nsIDOMElement> element;

    // Find the element with the specified id
    rv = htmlDoc->GetElementById(aAnchorName, getter_AddRefs(element));
    if (NS_SUCCEEDED(rv)) {
      // Get the nsIContent interface, because that's what we need to
      // get the primary frame
      rv = element->QueryInterface(kIContentIID, getter_AddRefs(content));
    }
  }
  else if (NS_SUCCEEDED(mDocument->QueryInterface(kIXMLDocumentIID,
                                                  getter_AddRefs(xmlDoc)))) {
    rv = xmlDoc->GetContentById(aAnchorName,  getter_AddRefs(content));
  }

  if (NS_SUCCEEDED(rv)) {
    nsIFrame* frame;
    
    // Get the primary frame
    if (NS_SUCCEEDED(GetPrimaryFrameFor(content, &frame))) {
      rv = ScrollFrameIntoView(frame, NS_PRESSHELL_SCROLL_TOP, 0, NS_PRESSHELL_SCROLL_LEFT, 0);
    }
  } else {
    rv = NS_ERROR_FAILURE;
  }

  return rv;
}

// XXX: all arguments except aFrame are currently ignored!
NS_IMETHODIMP
PresShell::ScrollFrameIntoView(nsIFrame *aFrame,
                               PRInt32   aVOffsetPercent, 
                               PRUint32  aVFlags,
                               PRInt32   aHOffsetPercent, 
                               PRUint32  aHFlags) const
{
  nsresult rv = NS_ERROR_UNEXPECTED;
  if (!aFrame)
    return NS_ERROR_NULL_POINTER;
  if (nsnull != mViewManager) {
    nsIScrollableView* scrollingView;
    mViewManager->GetRootScrollableView(&scrollingView);

    if (scrollingView) {
      // Determine the offset for the given frame relative to the
      // scrolled view
      nsIView*  scrolledView;
      nsPoint   offset;
      nsIView*  view;
          
      scrollingView->GetScrolledView(scrolledView);
      aFrame->GetOffsetFromView(offset, &view);
      if (aVFlags & NS_PRESSHELL_SCROLL_ANYWHERE)
      { // if we just care that the frame is somewhere in the view port, 
        // check to see if it is already visible
        nsRect frameRect (offset, nsSize(1,1));   // XXX: this is biased toward showing the top of the frame
        nsRect viewBounds;            // the bounds of the view containing the frame
        view->GetBounds(viewBounds);
        // walk up the view hierarchy looking for the scrolling view
        nsIView *parentView;          
        view->GetParent(parentView);
        nsRect bounds;
        nscoord visHeight=0;
        while (parentView)
        {
          nsIScrollableView *parentScrollableView=nsnull;
          nsresult result = parentView->QueryInterface(kIScrollableViewIID, (void**)&parentScrollableView);
          if (NS_SUCCEEDED(result))
          {
            if (parentScrollableView==scrollingView)
            { // found the scrolling view, get it's height
              parentView->GetBounds(bounds);
              visHeight = bounds.height;
              break;
            }
          }
          parentView->GetParent(parentView);
        }
        if (0!=visHeight)
        { // there was a scrolling view, calc the visible rect
          nsRect visArea(viewBounds.x, -(viewBounds.y), viewBounds.width, visHeight);
          // is the frame within the visible rect?
          PRBool alreadyVisible = visArea.Intersects(frameRect);
          if (PR_FALSE==alreadyVisible) { // if not, scroll it into view
            scrollingView->ScrollTo(0, offset.y, NS_VMREFRESH_IMMEDIATE);
          }
        }
      }
      else
      {
        // XXX If view != scrolledView, then there is a scrolled frame,
        // e.g., a DIV with 'overflow' of 'scroll', somewhere in the middle,
        // or maybe an absolutely positioned element that has a view. We
        // need to handle these cases...
        scrollingView->ScrollTo(0, offset.y, NS_VMREFRESH_IMMEDIATE);
      }
      rv = NS_OK;
    }
  }
  return rv;
}

NS_IMETHODIMP
PresShell::DoCopy(nsISelectionMgr* aSelectionMgr)
{
  nsCOMPtr<nsIDocument> doc;
  GetDocument(getter_AddRefs(doc));
  if (doc) {
    nsString buffer;
    
    nsIDOMSelection* sel = nsnull;
    GetSelection(&sel);
      
    if (sel != nsnull)
      doc->CreateXIF(buffer,sel);
    NS_IF_RELEASE(sel);

    nsIParser* parser;

    static NS_DEFINE_IID(kCParserIID, NS_IPARSER_IID);
    static NS_DEFINE_IID(kCParserCID, NS_PARSER_IID);

    nsresult rv = nsComponentManager::CreateInstance(kCParserCID, 
                                               nsnull, 
                                               kCParserIID, 
                                               (void **)&parser);

    if (NS_OK != rv)
      return rv;

    nsIHTMLContentSink* sink = nsnull;
	

//  rv = NS_New_HTML_ContentSinkStream(&sink,PR_FALSE,PR_FALSE);
//  Changed to do plain text only for Dogfood -- gpk 3/14/99
    rv = NS_New_HTMLToTXT_SinkStream(&sink);
    


    ostream* copyStream;
    rv = aSelectionMgr->GetCopyOStream(&copyStream);
    if (!NS_SUCCEEDED(rv))
    {
      NS_IF_RELEASE(sink);
      return rv;
    }

//  Changed to do plain text only for Dogfood -- gpk 3/14/99
//  ((nsHTMLContentSinkStream*)sink)->SetOutputStream(*copyStream);
    ((nsHTMLToTXTSinkStream*)sink)->SetOutputStream(*copyStream);

    if (NS_OK == rv) {
      parser->SetContentSink(sink);
	  
      nsIDTD* dtd = nsnull;
      rv = NS_NewXIFDTD(&dtd);
      if (NS_OK == rv) 
      {
        parser->RegisterDTD(dtd);
        //dtd->SetContentSink(sink);
        //dtd->SetParser(parser);
        parser->Parse(buffer, 0, "text/xif",PR_FALSE,PR_TRUE);           
      }
      NS_IF_RELEASE(dtd);

      aSelectionMgr->CopyToClipboard();
    }
    NS_IF_RELEASE(sink);
    NS_RELEASE(parser);
  }
  return NS_OK;
}

NS_IMETHODIMP
PresShell::ContentChanged(nsIDocument *aDocument,
                          nsIContent*  aContent,
                          nsISupports* aSubContent)
{
  NS_PRECONDITION(nsnull != mRootFrame, "null root frame");

  EnterReflowLock();
  nsresult rv = mStyleSet->ContentChanged(mPresContext, aContent, aSubContent);
  ExitReflowLock();
  if (mSelection)
    mSelection->ResetSelection(this, mRootFrame);

  return rv;
}

NS_IMETHODIMP
PresShell::ContentStateChanged(nsIDocument* aDocument,
                               nsIContent* aContent)
{
  NS_PRECONDITION(nsnull != mRootFrame, "null root frame");

  EnterReflowLock();
  nsresult rv = mStyleSet->ContentStateChanged(mPresContext, aContent);
  ExitReflowLock();
  if (mSelection)
    mSelection->ResetSelection(this, mRootFrame);

  return rv;
}


NS_IMETHODIMP
PresShell::AttributeChanged(nsIDocument *aDocument,
                            nsIContent*  aContent,
                            nsIAtom*     aAttribute,
                            PRInt32      aHint)
{
  NS_PRECONDITION(nsnull != mRootFrame, "null root frame");

  EnterReflowLock();
  nsresult rv = mStyleSet->AttributeChanged(mPresContext, aContent, aAttribute, aHint);
  ExitReflowLock();
  return rv;
}

NS_IMETHODIMP
PresShell::ContentAppended(nsIDocument *aDocument,
                           nsIContent* aContainer,
                           PRInt32     aNewIndexInContainer)
{
  EnterReflowLock();
  nsresult  rv = mStyleSet->ContentAppended(mPresContext, aContainer, aNewIndexInContainer);
  ExitReflowLock();
  return rv;
}

NS_IMETHODIMP
PresShell::ContentInserted(nsIDocument* aDocument,
                           nsIContent*  aContainer,
                           nsIContent*  aChild,
                           PRInt32      aIndexInContainer)
{
  EnterReflowLock();
  nsresult  rv = mStyleSet->ContentInserted(mPresContext, aContainer, aChild, aIndexInContainer);
  ExitReflowLock();
  return rv;
}

NS_IMETHODIMP
PresShell::ContentReplaced(nsIDocument* aDocument,
                           nsIContent*  aContainer,
                           nsIContent*  aOldChild,
                           nsIContent*  aNewChild,
                           PRInt32      aIndexInContainer)
{
  EnterReflowLock();
  nsresult  rv = mStyleSet->ContentReplaced(mPresContext, aContainer, aOldChild,
                                            aNewChild, aIndexInContainer);
  ExitReflowLock();
  return rv;
}

NS_IMETHODIMP
PresShell::ContentRemoved(nsIDocument *aDocument,
                          nsIContent* aContainer,
                          nsIContent* aChild,
                          PRInt32     aIndexInContainer)
{
  EnterReflowLock();
  nsresult  rv = mStyleSet->ContentRemoved(mPresContext, aContainer,
                                           aChild, aIndexInContainer);
  ExitReflowLock();
  return rv;
}

nsresult
PresShell::ReconstructFrames(void)
{
  nsresult rv = NS_OK;
          
  EnterReflowLock();
  rv = mStyleSet->ReconstructDocElementHierarchy(mPresContext);
  ExitReflowLock();

  return rv;
}

NS_IMETHODIMP
PresShell::StyleSheetAdded(nsIDocument *aDocument,
                           nsIStyleSheet* aStyleSheet)
{
  return ReconstructFrames();
}

NS_IMETHODIMP 
PresShell::StyleSheetRemoved(nsIDocument *aDocument,
                             nsIStyleSheet* aStyleSheet)
{
  return ReconstructFrames();
}

NS_IMETHODIMP
PresShell::StyleSheetDisabledStateChanged(nsIDocument *aDocument,
                                          nsIStyleSheet* aStyleSheet,
                                          PRBool aDisabled)
{
  return ReconstructFrames();
}

NS_IMETHODIMP
PresShell::StyleRuleChanged(nsIDocument *aDocument,
                            nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule,
                            PRInt32 aHint) 
{
  EnterReflowLock();
  nsresult  rv = mStyleSet->StyleRuleChanged(mPresContext, aStyleSheet,
                                             aStyleRule, aHint);
  ExitReflowLock();
  return rv;
}

NS_IMETHODIMP
PresShell::StyleRuleAdded(nsIDocument *aDocument,
                          nsIStyleSheet* aStyleSheet,
                          nsIStyleRule* aStyleRule) 
{ 
  EnterReflowLock();
  nsresult  rv = mStyleSet->StyleRuleAdded(mPresContext, aStyleSheet, aStyleRule);
  ExitReflowLock();
  // XXX For now reconstruct everything
  return ReconstructFrames();
}

NS_IMETHODIMP
PresShell::StyleRuleRemoved(nsIDocument *aDocument,
                            nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule) 
{ 
  EnterReflowLock();
  nsresult  rv = mStyleSet->StyleRuleRemoved(mPresContext, aStyleSheet, aStyleRule);
  ExitReflowLock();
  // XXX For now reconstruct everything
  return ReconstructFrames();
}


NS_IMETHODIMP
PresShell::DocumentWillBeDestroyed(nsIDocument *aDocument)
{
  return NS_OK;
}

static nsIFrame*
FindFrameWithContent(nsIFrame* aFrame, nsIContent* aContent)
{
  nsIContent* frameContent;
   
  aFrame->GetContent(&frameContent);
  if (frameContent == aContent) {
    nsIStyleContext*  styleContext;
    nsIAtom*          pseudoTag;
    PRBool            isPlaceholder = PR_FALSE;

    // If it's a placeholder frame, then ignore it and keep looking for the
    // primary frame
    aFrame->GetStyleContext(&styleContext);
    styleContext->GetPseudoType(pseudoTag);
    if (pseudoTag == nsHTMLAtoms::placeholderPseudo) {
      isPlaceholder = PR_TRUE;
    }
    NS_RELEASE(styleContext);
    NS_IF_RELEASE(pseudoTag);

    if (!isPlaceholder) {
      NS_IF_RELEASE(frameContent);
      return aFrame;
    }
  }
  NS_IF_RELEASE(frameContent);

  // Search for the frame in each child list that aFrame supports
  nsIAtom* listName = nsnull;
  PRInt32 listIndex = 0;
  do {
    nsIFrame* kid;
    aFrame->FirstChild(listName, &kid);
    while (nsnull != kid) {
      nsIFrame* result = FindFrameWithContent(kid, aContent);
      if (nsnull != result) {
        NS_IF_RELEASE(listName);
        return result;
      }
      kid->GetNextSibling(&kid);
    }
    NS_IF_RELEASE(listName);
    aFrame->GetAdditionalChildListName(listIndex++, &listName);
  } while(nsnull != listName);

  return nsnull;
}

NS_IMETHODIMP
PresShell::GetPrimaryFrameFor(nsIContent* aContent,
                              nsIFrame** aResult) const
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  // For the time being do a brute force depth-first search of
  // the frame tree
  *aResult = ::FindFrameWithContent(mRootFrame, aContent);
  return NS_OK;
}

NS_IMETHODIMP 
PresShell::GetStyleContextFor(nsIFrame*         aFrame,
                              nsIStyleContext** aStyleContext) const
{
  if (!aFrame || !aStyleContext) {
    return NS_ERROR_NULL_POINTER;
  }
  return (aFrame->GetStyleContext(aStyleContext));
}

NS_IMETHODIMP
PresShell::GetLayoutObjectFor(nsIContent*   aContent,
                              nsISupports** aResult) const 
{
  nsresult result = NS_ERROR_NULL_POINTER;
  if ((nsnull!=aResult) && (nsnull!=aContent))
  {
    *aResult = nsnull;
    nsIFrame *primaryFrame=nsnull;
    result = GetPrimaryFrameFor(aContent, &primaryFrame);
    if ((NS_SUCCEEDED(result)) && (nsnull!=primaryFrame))
    {
      result = primaryFrame->QueryInterface(kISupportsIID, (void**)aResult);
    }
  }
  return result;
}
  

NS_IMETHODIMP
PresShell::GetPlaceholderFrameFor(nsIFrame*  aFrame,
                                  nsIFrame** aResult) const
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  NS_PRECONDITION(nsnull != aFrame, "no frame");
  if ((nsnull == aResult) || (nsnull == aFrame)) {
    return NS_ERROR_NULL_POINTER;
  }

  if (nsnull == mPlaceholderMap) {
    *aResult = nsnull;
  } else {
    *aResult = (nsIFrame*) mPlaceholderMap->Get(aFrame);
  }

  return NS_OK;
}

NS_IMETHODIMP
PresShell::SetPlaceholderFrameFor(nsIFrame* aFrame,
                                  nsIFrame* aPlaceholderFrame)
{
  NS_PRECONDITION(nsnull != aFrame, "no frame");

  if (nsnull == mPlaceholderMap) {
    mPlaceholderMap = new FrameHashTable;
    if (nsnull == mPlaceholderMap) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  if (nsnull == aPlaceholderFrame) {
    mPlaceholderMap->Remove(aFrame);
  } else {
    mPlaceholderMap->Put(aFrame, (void*)aPlaceholderFrame);
  }
  return NS_OK;
}

//nsIViewObserver

NS_IMETHODIMP
PresShell::Paint(nsIView              *aView,
                 nsIRenderingContext& aRenderingContext,
                 const nsRect&        aDirtyRect)
{
  void*     clientData;
  nsIFrame* frame;
  nsresult  rv = NS_OK;

  NS_ASSERTION(!(nsnull == aView), "null view");

  aView->GetClientData(clientData);
  frame = (nsIFrame *)clientData;

  if (nsnull != frame) {
    rv = frame->Paint(*mPresContext, aRenderingContext, aDirtyRect,
                      eFramePaintLayer_Underlay);
    rv = frame->Paint(*mPresContext, aRenderingContext, aDirtyRect,
                      eFramePaintLayer_Content);
    rv = frame->Paint(*mPresContext, aRenderingContext, aDirtyRect,
                      eFramePaintLayer_Overlay);
#ifdef NS_DEBUG
    // Draw a border around the frame
    if (nsIFrame::GetShowFrameBorders()) {
      nsRect r;
      frame->GetRect(r);
      aRenderingContext.SetColor(NS_RGB(0,0,255));
      aRenderingContext.DrawRect(0, 0, r.width, r.height);
    }
#endif
  }
  return rv;
}

NS_IMETHODIMP
PresShell::HandleEvent(nsIView         *aView,
                       nsGUIEvent*     aEvent,
                       nsEventStatus&  aEventStatus)
{
  void*     clientData;
  nsIFrame* frame;
  nsresult  rv = NS_OK;
  
  NS_ASSERTION(!(nsnull == aView), "null view");

  if (mIsDestroying || mReflowLockCount > 0) {
    return NS_OK;
  }

  aView->GetClientData(clientData);
  frame = (nsIFrame *)clientData;

  if (nsnull != frame) {
    if (mSelection && mFocusEventFrame && aEvent->eventStructType == NS_KEY_EVENT)
    {
      mSelection->EnableFrameNotification(PR_FALSE);
      mSelection->HandleKeyEvent((nsIFocusTracker *)this, aEvent);
      mSelection->EnableFrameNotification(PR_TRUE); //prevents secondary reset selection called since
      //we are a listener now.
    }
    frame->GetFrameForPoint(aEvent->point, &mCurrentEventFrame);
    if (nsnull != mCurrentEventFrame) {
      //Once we have the targetFrame, handle the event in this order
      nsIEventStateManager *manager;
      if (NS_OK == mPresContext->GetEventStateManager(&manager)) {
        //1. Give event to event manager for pre event state changes and generation of synthetic events.
        rv = manager->PreHandleEvent(*mPresContext, aEvent, mCurrentEventFrame, aEventStatus);

        //2. Give event to the DOM for third party and JS use.
        if (nsnull != mCurrentEventFrame && NS_OK == rv) {
          nsIContent* targetContent;
          if (NS_OK == mCurrentEventFrame->GetContent(&targetContent) && nsnull != targetContent) {
            rv = targetContent->HandleDOMEvent(*mPresContext, (nsEvent*)aEvent, nsnull, 
                                               DOM_EVENT_INIT, aEventStatus);
            NS_RELEASE(targetContent);
          }

          //3. Give event to the Frames for browser default processing.
          // XXX The event isn't translated into the local coordinate space
          // of the frame...
          if (nsnull != mCurrentEventFrame && NS_OK == rv) {
            rv = mCurrentEventFrame->HandleEvent(*mPresContext, aEvent, aEventStatus);

            //4. Give event to event manager for post event state changes and generation of synthetic events.
            if (nsnull != mCurrentEventFrame && NS_OK == rv) {
              rv = manager->PostHandleEvent(*mPresContext, aEvent, mCurrentEventFrame, aEventStatus);
            }
          }
        }
        NS_RELEASE(manager);
      }
    }
  }
  else {
    rv = NS_OK;
  }

  return rv;
}

NS_IMETHODIMP
PresShell::Scrolled(nsIView *aView)
{
  void*     clientData;
  nsIFrame* frame;
  nsresult  rv;
  
  NS_ASSERTION(!(nsnull == aView), "null view");

  aView->GetClientData(clientData);
  frame = (nsIFrame *)clientData;

  if (nsnull != frame)
    rv = frame->Scrolled(aView);
  else
    rv = NS_OK;

  return rv;
}

NS_IMETHODIMP
PresShell::ResizeReflow(nsIView *aView, nscoord aWidth, nscoord aHeight)
{
  return ResizeReflow(aWidth, aHeight);
}

#ifdef NS_DEBUG
#include "nsViewsCID.h"
#include "nsWidgetsCID.h"
#include "nsIScrollableView.h"
#include "nsIDeviceContext.h"
#include "nsIURL.h"

static NS_DEFINE_IID(kViewManagerCID, NS_VIEW_MANAGER_CID);
static NS_DEFINE_IID(kIViewManagerIID, NS_IVIEWMANAGER_IID);
static NS_DEFINE_IID(kScrollingViewCID, NS_SCROLLING_VIEW_CID);
static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);
static NS_DEFINE_IID(kScrollViewIID, NS_ISCROLLABLEVIEW_IID);
static NS_DEFINE_IID(kWidgetCID, NS_CHILD_CID);

static void
LogVerifyMessage(nsIFrame* k1, nsIFrame* k2, const char* aMsg)
{
  printf("verifyreflow: ");
  nsAutoString name;
  if (nsnull != k1) {
    k1->GetFrameName(name);
  }
  else {
    name = "(null)";
  }
  fputs(name, stdout);

  printf(" != ");

  if (nsnull != k2) {
    k2->GetFrameName(name);
  }
  else {
    name = "(null)";
  }
  fputs(name, stdout);

  printf(" %s", aMsg);
}

static void
LogVerifyMessage(nsIFrame* k1, nsIFrame* k2, const char* aMsg,
                 const nsRect& r1, const nsRect& r2)
{
  printf("verifyreflow: ");
  nsAutoString name;
  k1->GetFrameName(name);
  fputs(name, stdout);
  stdout << r1;

  printf(" != ");

  k2->GetFrameName(name);
  fputs(name, stdout);
  stdout << r2;

  printf(" %s\n", aMsg);
  if (gVerifyReflowAll) {
    k1->List(stdout, 1);
    k2->List(stdout, 1);
  }
}

static void
CompareTrees(nsIFrame* aA, nsIFrame* aB)
{
  PRBool whoops = PR_FALSE;
  nsIAtom* listName = nsnull;
  PRInt32 listIndex = 0;
  do {
    nsIFrame* k1, *k2;
    aA->FirstChild(listName, &k1);
    aB->FirstChild(listName, &k2);
    PRInt32 l1 = nsContainerFrame::LengthOf(k1);
    PRInt32 l2 = nsContainerFrame::LengthOf(k2);
    if (l1 != l2) {
      LogVerifyMessage(k1, k2, "child counts don't match: ");
      printf("%d != %d\n", l1, l2);
      if (!gVerifyReflowAll) {
        break;
      }
    }

    nsRect r1, r2;
    nsIView* v1, *v2;
    nsIWidget* w1, *w2;
    for (;;) {
      if (((nsnull == k1) && (nsnull != k2)) ||
          ((nsnull != k1) && (nsnull == k2))) {
        LogVerifyMessage(k1, k2, "child lists are different\n");
        whoops = PR_TRUE;
        break;
      }
      else if (nsnull != k1) {
        // Verify that the frames are the same size
        k1->GetRect(r1);
        k2->GetRect(r2);
        if (r1 != r2) {
          LogVerifyMessage(k1, k2, "(frame rects)", r1, r2);
          whoops = PR_TRUE;
        }

        // Make sure either both have views or neither have views; if they
        // do have views, make sure the views are the same size. If the
        // views have widgets, make sure they both do or neither does. If
        // they do, make sure the widgets are the same size.
        k1->GetView(&v1);
        k2->GetView(&v2);
        if (((nsnull == v1) && (nsnull != v2)) ||
            ((nsnull != v1) && (nsnull == v2))) {
          LogVerifyMessage(k1, k2, "child views are not matched\n");
          whoops = PR_TRUE;
        }
        else if (nsnull != v1) {
          v1->GetBounds(r1);
          v2->GetBounds(r2);
          if (r1 != r2) {
            LogVerifyMessage(k1, k2, "(view rects)", r1, r2);
            whoops = PR_TRUE;
          }

          v1->GetWidget(w1);
          v2->GetWidget(w2);
          if (((nsnull == w1) && (nsnull != w2)) ||
              ((nsnull != w1) && (nsnull == w2))) {
            LogVerifyMessage(k1, k2, "child widgets are not matched\n");
            whoops = PR_TRUE;
          }
          else if (nsnull != w1) {
            w1->GetBounds(r1);
            w2->GetBounds(r2);
            if (r1 != r2) {
              LogVerifyMessage(k1, k2, "(widget rects)", r1, r2);
              whoops = PR_TRUE;
            }
          }
        }
        if (whoops && !gVerifyReflowAll) {
          break;
        }

        // Compare the sub-trees too
        CompareTrees(k1, k2);

        // Advance to next sibling
        k1->GetNextSibling(&k1);
        k2->GetNextSibling(&k2);
      }
      else {
        break;
      }
    }
    if (whoops && !gVerifyReflowAll) {
      break;
    }
    NS_IF_RELEASE(listName);

    nsIAtom* listName1;
    nsIAtom* listName2;
    aA->GetAdditionalChildListName(listIndex, &listName1);
    aB->GetAdditionalChildListName(listIndex, &listName2);
    listIndex++;
    if (listName1 != listName2) {
      LogVerifyMessage(k1, k2, "child list names are not matched: ");
      nsAutoString tmp;
      if (nsnull != listName1) {
        listName1->ToString(tmp);
        fputs(tmp, stdout);
      }
      else
        fputs("(null)", stdout);
      printf(" != ");
      if (nsnull != listName2) {
        listName2->ToString(tmp);
        fputs(tmp, stdout);
      }
      else
        fputs("(null)", stdout);
      printf("\n");
      NS_IF_RELEASE(listName1);
      NS_IF_RELEASE(listName2);
      break;
    }
    NS_IF_RELEASE(listName2);
    listName = listName1;
  } while (listName != nsnull);
}

// After an incremental reflow, we verify the correctness by doing a
// full reflow into a fresh frame tree.
void
PresShell::VerifyIncrementalReflow()
{
  // All the stuff we are creating that needs releasing
  nsIPresContext* cx;
  nsIViewManager* vm;
  nsIView* view;
  nsIPresShell* sh;

  // Create a presentation context to view the new frame tree
  nsresult rv;
  PRBool isPaginated = PR_FALSE;
  mPresContext->IsPaginated(&isPaginated);
  if (isPaginated) {
    rv = NS_NewPrintPreviewContext(&cx);
  }
  else {
    rv = NS_NewGalleyContext(&cx);
  }
  NS_ASSERTION(NS_OK == rv, "failed to create presentation context");
  nsCOMPtr<nsIDeviceContext> dc;
  mPresContext->GetDeviceContext(getter_AddRefs(dc));
  nsCOMPtr<nsIPref> prefs; 
  mPresContext->GetPrefs(getter_AddRefs(prefs));
  cx->Init(dc, prefs);

  // Get our scrolling preference
  nsScrollPreference scrolling;
  nsIView* rootView;
  mViewManager->GetRootView(rootView);
  nsIScrollableView* scrollView;
  rv = rootView->QueryInterface(kScrollViewIID, (void**)&scrollView);
  if (NS_OK == rv) {
    scrollView->GetScrollPreference(scrolling);
  }
  nsIWidget* rootWidget;
  rootView->GetWidget(rootWidget);
  void* nativeParentWidget = rootWidget->GetNativeData(NS_NATIVE_WIDGET);

  // Create a new view manager.
  rv = nsComponentManager::CreateInstance(kViewManagerCID, nsnull, kIViewManagerIID,
                                    (void**) &vm);
  if ((NS_OK != rv) || (NS_OK != vm->Init(dc))) {
    NS_ASSERTION(NS_OK == rv, "failed to create view manager");
  }

  vm->SetViewObserver((nsIViewObserver *)this);

  // Create a child window of the parent that is our "root view/window"
  // Create a view
  nsRect tbounds;
  mPresContext->GetVisibleArea(tbounds);
  rv = nsComponentManager::CreateInstance(kScrollingViewCID, nsnull, kIViewIID,
                                    (void **) &view);
  if ((NS_OK != rv) || (NS_OK != view->Init(vm, tbounds, nsnull))) {
    NS_ASSERTION(NS_OK == rv, "failed to create scroll view");
  }

  //now create the widget for the view
  rv = view->CreateWidget(kWidgetCID, nsnull, nativeParentWidget);
  if (NS_OK != rv) {
    NS_ASSERTION(NS_OK == rv, "failed to create scroll view widget");
  }

  rv = view->QueryInterface(kScrollViewIID, (void**)&scrollView);
  if (NS_OK == rv) {
    scrollView->CreateScrollControls(nativeParentWidget);
    scrollView->SetScrollPreference(scrolling);
  }
  else {
    NS_ASSERTION(0, "invalid scrolling view");
  }

  // Setup hierarchical relationship in view manager
  vm->SetRootView(view);

  // Make the new presentation context the same size as our
  // presentation context.
  nsRect r;
  mPresContext->GetVisibleArea(r);
  cx->SetVisibleArea(r);
  // Create a new presentation shell to view the document. Use the
  // exact same style information that this document has.
  rv = mDocument->CreateShell(cx, vm, mStyleSet, &sh);
  NS_ASSERTION(NS_OK == rv, "failed to create presentation shell");
  sh->InitialReflow(r.width, r.height);

  // Now that the document has been reflowed, use its frame tree to
  // compare against our frame tree.
  nsIFrame* root1;
  nsIFrame* root2;
  GetRootFrame(&root1);
  sh->GetRootFrame(&root2);
  CompareTrees(root1, root2);

  NS_RELEASE(vm);
  NS_RELEASE(cx);
  NS_RELEASE(sh);
}
#endif

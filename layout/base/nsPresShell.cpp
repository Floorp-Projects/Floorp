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
#include "nsVoidArray.h"
#include "nsIPref.h"
#include "nsIViewObserver.h"
#include "nsContainerFrame.h"
#include "nsHTMLIIDs.h"
#include "nsIDeviceContext.h"
#include "nsIEventStateManager.h"
#include "nsDOMEvent.h"
#include "nsHTMLParts.h"
#include "nsISelection.h"
#include "nsLayoutCID.h"

static PRBool gsNoisyRefs = PR_FALSE;
#undef NOISY

#if 0
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
  // XXX if debugging then we should assert that the table is empty
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
#endif

//----------------------------------------------------------------------

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIPresShellIID, NS_IPRESSHELL_IID);
static NS_DEFINE_IID(kIDocumentObserverIID, NS_IDOCUMENT_OBSERVER_IID);
static NS_DEFINE_IID(kIViewObserverIID, NS_IVIEWOBSERVER_IID);
static NS_DEFINE_IID(kRangeListCID, NS_RANGELIST_CID);
static NS_DEFINE_IID(kISelectionIID, NS_ISELECTION_IID);

class PresShell : public nsIPresShell, public nsIViewObserver,
                  private nsIDocumentObserver

{
public:
  PresShell();

  void* operator new(size_t sz) {
    void* rv = new char[sz];
    nsCRT::zero(rv, sz);
    return rv;
  }

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
  virtual nsIDocument* GetDocument();
  virtual nsIPresContext* GetPresContext();
  virtual nsIViewManager* GetViewManager();
  virtual nsIStyleSet* GetStyleSet();
  virtual nsresult SetFocus(nsIFrame *aFrame);
  virtual nsresult GetSelection(nsISelection **aSelection);
  NS_IMETHOD EnterReflowLock();
  NS_IMETHOD ExitReflowLock();
  virtual void BeginObservingDocument();
  virtual void EndObservingDocument();
  NS_IMETHOD InitialReflow(nscoord aWidth, nscoord aHeight);
  NS_IMETHOD ResizeReflow(nscoord aWidth, nscoord aHeight);
  NS_IMETHOD StyleChangeReflow();
  virtual nsIFrame* GetRootFrame();
  NS_IMETHOD GetPageSequenceFrame(nsIPageSequenceFrame*& aPageSequenceFrame);
  virtual nsIFrame* FindFrameWithContent(nsIContent* aContent);
  virtual void AppendReflowCommand(nsIReflowCommand* aReflowCommand);
  virtual void ProcessReflowCommands();
  virtual void ClearFrameRefs(nsIFrame*);
  NS_IMETHOD CreateRenderingContext(nsIFrame *aFrame, nsIRenderingContext *&aContext);

  //nsIViewObserver interface

  NS_IMETHOD Paint(nsIView *aView,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect);
  NS_IMETHOD HandleEvent(nsIView*        aView,
                         nsGUIEvent*     aEvent,
                         nsEventStatus&  aEventStatus);
  NS_IMETHOD Scrolled(nsIView *aView);
  NS_IMETHOD ResizeReflow(nsIView *aView, nscoord aWidth, nscoord aHeight);

protected:
  ~PresShell();

  nsresult ReconstructFrames(void);

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
  nsISelection *mSelection;
};

#ifdef NS_DEBUG
/**
 * Note: the log module is created during library initialization which
 * means that you cannot perform logging before then.
 */
static PRLogModuleInfo* gLogModule = PR_NewLogModule("verifyreflow");
#endif

static PRBool gVerifyReflow = PRBool(0x55);
static PRBool gVerifyReflowAll;

NS_LAYOUT PRBool
nsIPresShell::GetVerifyReflowEnable()
{
#ifdef NS_DEBUG
  if (gVerifyReflow == PRBool(0x55)) {
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
  nsRepository::CreateInstance(kRangeListCID, nsnull, kISelectionIID, (void **) &mSelection);
}

#ifdef NS_DEBUG
// for debugging only
nsrefcnt PresShell::AddRef(void)
{
  if (gsNoisyRefs) printf("PresShell: AddRef: %x, cnt = %d \n",this, mRefCnt+1);
  return ++mRefCnt;
}

// for debugging only
nsrefcnt PresShell::Release(void)
{
  if (gsNoisyRefs==PR_TRUE) printf("PresShell Release: %x, cnt = %d \n",this, mRefCnt-1);
  if (--mRefCnt == 0) {
    if (gsNoisyRefs==PR_TRUE) printf("PresShell Delete: %x, \n",this);
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
  NS_IF_RELEASE(mSelection);
  mRefCnt = 0;
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

  return NS_OK;
}

NS_METHOD
PresShell::EnterReflowLock()
{
  ++mReflowLockCount;
  return NS_OK;
}

NS_METHOD
PresShell::ExitReflowLock()
{
  PRUint32 newReflowLockCount = mReflowLockCount - 1;
  if (newReflowLockCount == 0) {
    ProcessReflowCommands();
  }
  mReflowLockCount = newReflowLockCount;
  return NS_OK;
}

nsIDocument*
PresShell::GetDocument()
{
  NS_IF_ADDREF(mDocument);
  return mDocument;
}

nsIPresContext*
PresShell::GetPresContext()
{
  NS_IF_ADDREF(mPresContext);
  return mPresContext;
}

nsIViewManager*
PresShell::GetViewManager()
{
  NS_IF_ADDREF(mViewManager);
  return mViewManager;
}

nsIStyleSet*
PresShell::GetStyleSet()
{
  NS_IF_ADDREF(mStyleSet);
  return mStyleSet;
}

nsresult 
PresShell::SetFocus(nsIFrame *aFrame)
{
  if (!aFrame)
    return NS_ERROR_NULL_POINTER;
  mFocusEventFrame = aFrame;
  return NS_OK;
}



nsresult
PresShell::GetSelection(nsISelection **aSelection)
{
  if (!aSelection || !mSelection)
    return NS_ERROR_NULL_POINTER;
  return mSelection->QueryInterface(kISelectionIID,(void **)aSelection);
}



// Make shell be a document observer
void
PresShell::BeginObservingDocument()
{
  if (nsnull != mDocument) {
    mDocument->AddObserver(this);
  }
}

// Make shell stop being a document observer
void
PresShell::EndObservingDocument()
{
  if (nsnull != mDocument) {
    mDocument->RemoveObserver(this);
  }
}

NS_IMETHODIMP
PresShell::InitialReflow(nscoord aWidth, nscoord aHeight)
{
  NS_PRECONDITION(nsnull == mRootFrame, "unexpected root frame");

  EnterReflowLock();

  if (nsnull != mPresContext) {
    nsRect r(0, 0, aWidth, aHeight);
    mPresContext->SetVisibleArea(r);
  }

  if (nsnull == mRootFrame) {
    if (nsnull != mDocument) {
      nsIContent* root = mDocument->GetRootContent();
      if (nsnull != root) {
        // Have style sheet processor construct a frame for the
        // root content object
        mStyleSet->ConstructFrame(mPresContext, root, nsnull, mRootFrame);
        NS_RELEASE(root);
      }
    }
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

    CreateRenderingContext(mRootFrame, rcx);

    nsHTMLReflowState reflowState(*mPresContext, mRootFrame,
                                  eReflowReason_Initial, maxSize, rcx);

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

  return NS_OK; //XXX this needs to be real. MMP
}

NS_IMETHODIMP
PresShell::ResizeReflow(nscoord aWidth, nscoord aHeight)
{
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

    CreateRenderingContext(mRootFrame, rcx);

    nsHTMLReflowState reflowState(*mPresContext, mRootFrame,
                                  eReflowReason_Resize, maxSize, rcx);

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

    // XXX if debugging then we should assert that the cache is empty
  } else {
#ifdef NOISY
    printf("PresShell::ResizeReflow: null root frame\n");
#endif
  }

  ExitReflowLock();

  return NS_OK; //XXX this needs to be real. MMP
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

    CreateRenderingContext(mRootFrame, rcx);

    // XXX We should be using eReflowReason_StyleChange
    nsHTMLReflowState reflowState(*mPresContext, mRootFrame,
                                  eReflowReason_Resize, maxSize, rcx);

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

nsIFrame*
PresShell::GetRootFrame()
{
  return mRootFrame;
}

NS_IMETHODIMP
PresShell::GetPageSequenceFrame(nsIPageSequenceFrame*& aPageSequenceFrame)
{
  nsIFrame*             child;
  nsIPageSequenceFrame* pageSequence;

  // The page sequence frame should be either the immediate child or
  // its child
  mRootFrame->FirstChild(nsnull, child);
  if (nsnull != child) {
    if (NS_SUCCEEDED(child->QueryInterface(kIPageSequenceFrameIID, (void**)&pageSequence))) {
      aPageSequenceFrame = pageSequence;
      return NS_OK;
    }
  
    child->FirstChild(nsnull, child);
    if (nsnull != child) {
      if (NS_SUCCEEDED(child->QueryInterface(kIPageSequenceFrameIID, (void**)&pageSequence))) {
        aPageSequenceFrame = pageSequence;
        return NS_OK;
      }
    }
  }

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

void
PresShell::AppendReflowCommand(nsIReflowCommand* aReflowCommand)
{
#ifdef NS_DEBUG
  if (mInVerifyReflow) {
    return;
  }
#endif
  mReflowCommands.AppendElement(aReflowCommand);
  NS_ADDREF(aReflowCommand);
}

void
PresShell::ProcessReflowCommands()
{
  if (0 != mReflowCommands.Count()) {
    nsHTMLReflowMetrics   desiredSize(nsnull);
    nsIRenderingContext*  rcx;

    CreateRenderingContext(mRootFrame, rcx);

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
      mInVerifyReflow = PR_TRUE;
      VerifyIncrementalReflow();
      mInVerifyReflow = PR_FALSE;

      if (0 != mReflowCommands.Count()) {
        printf("XXX yikes!\n");
      }
    }
#endif
  }
}

void
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
}

NS_IMETHODIMP PresShell :: CreateRenderingContext(nsIFrame *aFrame,
                                                  nsIRenderingContext *&aContext)
{
  nsIWidget *widget = nsnull;
  nsIView   *view = nsnull;
  nsPoint   pt;
  nsresult  rv;

  aFrame->GetView(view);

  if (nsnull == view)
    aFrame->GetOffsetFromView(pt, view);

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

  nsIDeviceContext  *dx;

  dx = mPresContext->GetDeviceContext();

  if (nsnull != view)
    rv = dx->CreateRenderingContext(view, aContext);
  else
    rv = dx->CreateRenderingContext(aContext);

  NS_RELEASE(dx);

  return rv;
}

#ifdef NS_DEBUG
static char*
ContentTag(nsIContent* aContent, PRIntn aSlot)
{
  static char buf0[100], buf1[100], buf2[100];
  static char* bufs[] = { buf0, buf1, buf2 };
  char* buf = bufs[aSlot];
  nsIAtom* atom;
  aContent->GetTag(atom);
  if (nsnull != atom) {
    nsAutoString tmp;
    atom->ToString(tmp);
    tmp.ToCString(buf, 100);
  }
  else {
    buf[0] = 0;
  }
  return buf;
}
#endif

NS_IMETHODIMP
PresShell::ContentChanged(nsIDocument *aDocument,
                          nsIContent*  aContent,
                          nsISupports* aSubContent)
{
  NS_PRECONDITION(nsnull != mRootFrame, "null root frame");

  EnterReflowLock();
  nsresult rv = mStyleSet->ContentChanged(mPresContext, aContent, aSubContent);
  ExitReflowLock();
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
  if (nsnull != mRootFrame) {
    nsIFrame* childFrame;
    rv = mRootFrame->FirstChild(nsnull, childFrame);
    
    if (nsnull != mDocument) {
      nsIContent* root = mDocument->GetRootContent();
      if (nsnull != root) {
        
        EnterReflowLock();
        rv = mStyleSet->ReconstructFrames(mPresContext, root,
                                          mRootFrame, childFrame);
        ExitReflowLock();
        NS_RELEASE(root);
      }
    }
  }
  return rv;
}

NS_IMETHODIMP
PresShell::StyleSheetAdded(nsIDocument *aDocument,
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
  return rv;
}

NS_IMETHODIMP
PresShell::StyleRuleRemoved(nsIDocument *aDocument,
                            nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule) 
{ 
  EnterReflowLock();
  nsresult  rv = mStyleSet->StyleRuleRemoved(mPresContext, aStyleSheet, aStyleRule);
  ExitReflowLock();
  return rv;
}


NS_IMETHODIMP
PresShell::DocumentWillBeDestroyed(nsIDocument *aDocument)
{
  return NS_OK;
}

static PRBool
IsZeroSizedFrame(nsIFrame *aFrame)
{
  nsSize size;
  aFrame->GetSize(size);
  return ((0 == size.width) && (0 == size.height));
}

static nsIFrame*
FindFrameWithContent(nsIFrame* aFrame, nsIContent* aContent)
{
  nsIContent* frameContent;
   
  aFrame->GetContent(frameContent);
  if (frameContent == aContent) {
    // XXX Sleazy hack to check whether this is a placeholder frame.
    // If it is, we skip it and go on to (hopefully) find the 
    // absolutely positioned frame.
    const nsStylePosition*  position;

    aFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)position);
    if ((NS_STYLE_POSITION_ABSOLUTE != position->mPosition) ||
        !IsZeroSizedFrame(aFrame)) {
      NS_RELEASE(frameContent);
      return aFrame;
    }
  }
  NS_IF_RELEASE(frameContent);

  // Search for the frame in each child list that aFrame supports
  nsIAtom* listName = nsnull;
  PRInt32 listIndex = 0;
  do {
    nsIFrame* kid;
    aFrame->FirstChild(listName, kid);
    while (nsnull != kid) {
      nsIFrame* result = FindFrameWithContent(kid, aContent);
      if (nsnull != result) {
        NS_IF_RELEASE(listName);
        return result;
      }
      kid->GetNextSibling(kid);
    }
    NS_IF_RELEASE(listName);
    aFrame->GetAdditionalChildListName(listIndex++, listName);
  } while(nsnull != listName);

  return nsnull;
}

nsIFrame*
PresShell::FindFrameWithContent(nsIContent* aContent)
{
  // For the time being do a brute force depth-first search of
  // the frame tree
  return ::FindFrameWithContent(mRootFrame, aContent);
}

//nsIViewObserver

NS_IMETHODIMP PresShell :: Paint(nsIView              *aView,
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
    rv = frame->Paint(*mPresContext, aRenderingContext, aDirtyRect);
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

NS_IMETHODIMP PresShell :: HandleEvent(nsIView         *aView,
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
      mSelection->HandleKeyEvent(aEvent, mFocusEventFrame);
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
          if (NS_OK == mCurrentEventFrame->GetContent(targetContent) && nsnull != targetContent) {
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

NS_IMETHODIMP PresShell :: Scrolled(nsIView *aView)
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

NS_IMETHODIMP PresShell :: ResizeReflow(nsIView *aView, nscoord aWidth, nscoord aHeight)
{
  return ResizeReflow(aWidth, aHeight);
}

#ifdef NS_DEBUG
#include "nsViewsCID.h"
#include "nsWidgetsCID.h"
#include "nsIScrollableView.h"
#include "nsIDeviceContext.h"
#include "nsIURL.h"
//#include "nsICSSParser.h"
//#include "nsIStyleSheet.h"

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
}

static void
CompareTrees(nsIFrame* aA, nsIFrame* aB)
{
  PRBool whoops = PR_FALSE;
  nsIAtom* listName = nsnull;
  PRInt32 listIndex = 0;
  do {
    nsIFrame* k1, *k2;
    aA->FirstChild(listName, k1);
    aB->FirstChild(listName, k2);
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
        k1->GetView(v1);
        k2->GetView(v2);
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
        k1->GetNextSibling(k1);
        k2->GetNextSibling(k2);
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
    aA->GetAdditionalChildListName(listIndex, listName1);
    aB->GetAdditionalChildListName(listIndex, listName2);
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
  if (mPresContext->IsPaginated()) {
    rv = NS_NewPrintPreviewContext(&cx);
  }
  else {
    rv = NS_NewGalleyContext(&cx);
  }
  NS_ASSERTION(NS_OK == rv, "failed to create presentation context");
  nsIDeviceContext* dc = mPresContext->GetDeviceContext();
  nsIPref* prefs; 
  mPresContext->GetPrefs(prefs);
  cx->Init(dc, prefs);
  NS_IF_RELEASE(prefs);

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
  rv = nsRepository::CreateInstance(kViewManagerCID, nsnull, kIViewManagerIID,
                                    (void**) &vm);
  if ((NS_OK != rv) || (NS_OK != vm->Init(dc))) {
    NS_ASSERTION(NS_OK == rv, "failed to create view manager");
  }

  NS_RELEASE(dc);

  vm->SetViewObserver((nsIViewObserver *)this);

  // Create a child window of the parent that is our "root view/window"
  // Create a view
  nsRect tbounds;
  mPresContext->GetVisibleArea(tbounds);
  rv = nsRepository::CreateInstance(kScrollingViewCID, nsnull, kIViewIID,
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
  CompareTrees(GetRootFrame(), sh->GetRootFrame());

  NS_RELEASE(vm);
  NS_RELEASE(cx);
  NS_RELEASE(sh);
}
#endif

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
  NS_IMETHOD EnterReflowLock();
  NS_IMETHOD ExitReflowLock();
  virtual void BeginObservingDocument();
  virtual void EndObservingDocument();
  NS_IMETHOD InitialReflow(nscoord aWidth, nscoord aHeight);
  NS_IMETHOD ResizeReflow(nscoord aWidth, nscoord aHeight);
  virtual nsIFrame* GetRootFrame();
  virtual nsIFrame* FindFrameWithContent(nsIContent* aContent);
  virtual void AppendReflowCommand(nsIReflowCommand* aReflowCommand);
  virtual void ProcessReflowCommands();
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

#ifdef NS_DEBUG
  void VerifyIncrementalReflow();
#endif

  nsIDocument* mDocument;
  nsIPresContext* mPresContext;
  nsIStyleSet* mStyleSet;
  nsIFrame* mRootFrame;
  nsIViewManager* mViewManager;
  PRUint32 mUpdateCount;
  nsVoidArray mReflowCommands;
  PRUint32 mReflowLockCount;
};

#ifdef NS_DEBUG
/**
 * Note: the log module is created during library initialization which
 * means that you cannot perform logging before then.
 */
static PRLogModuleInfo* gLogModule = PR_NewLogModule("verifyreflow");
#endif

static PRBool gVerifyReflow = PRBool(0x55);

NS_LAYOUT PRBool
nsIPresShell::GetVerifyReflowEnable()
{
#ifdef NS_DEBUG
  if (gVerifyReflow == PRBool(0x55)) {
    gVerifyReflow = 0 != gLogModule->level;
    printf("Note: verifyreflow is %sabled\n",
           gVerifyReflow ? "en" : "dis");
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

nsIFrame*
PresShell::GetRootFrame()
{
  return mRootFrame;
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

    // Place and size the root frame
    mRootFrame->SizeTo(desiredSize.width, desiredSize.height);
#ifdef NS_DEBUG
    if (nsIFrame::GetVerifyTreeEnable()) {
      mRootFrame->VerifyTree();
    }
    if (GetVerifyReflowEnable()) {
      VerifyIncrementalReflow();
    }
#endif
    NS_IF_RELEASE(rcx);
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

  if (nsnull != view)
  {
    nsIDeviceContext  *dx;

    dx = mPresContext->GetDeviceContext();
    rv = dx->CreateRenderingContext(view, aContext);
    NS_RELEASE(dx);
  }
  else
  {
    rv = NS_ERROR_FAILURE;
    aContext = nsnull;
  }

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

NS_IMETHODIMP
PresShell::StyleSheetAdded(nsIDocument *aDocument,
                           nsIStyleSheet* aStyleSheet)
{
  return NS_OK;
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

    aFrame->GetStyleData(eStyleStruct_Position, (nsStyleStruct*&)position);
    if ((NS_STYLE_POSITION_ABSOLUTE != position->mPosition) ||
        !IsZeroSizedFrame(aFrame)) {
      NS_RELEASE(frameContent);
      return aFrame;
    }
  }
  NS_RELEASE(frameContent);

  aFrame->FirstChild(aFrame);
  while (aFrame) {
    nsIFrame* result = FindFrameWithContent(aFrame, aContent);

    if (result) {
      return result;
    }

    aFrame->GetNextSibling(aFrame);
  }

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
  nsresult  rv;

  NS_ASSERTION(!(nsnull == aView), "null view");

  aView->GetClientData(clientData);
  frame = (nsIFrame *)clientData;

  if (nsnull != frame)
    rv = frame->Paint(*mPresContext, aRenderingContext, aDirtyRect);
  else
    rv = NS_OK;

  return rv;
}

NS_IMETHODIMP PresShell :: HandleEvent(nsIView         *aView,
                                       nsGUIEvent*     aEvent,
                                       nsEventStatus&  aEventStatus)
{
  void*     clientData;
  nsIFrame* frame;
  nsresult  rv;
  
  NS_ASSERTION(!(nsnull == aView), "null view");

  aView->GetClientData(clientData);
  frame = (nsIFrame *)clientData;

  if (nsnull != frame)
    rv = frame->HandleEvent(*mPresContext, aEvent, aEventStatus);
  else
    rv = NS_OK;

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
#include "nsICSSParser.h"
#include "nsIStyleSheet.h"

static NS_DEFINE_IID(kViewManagerCID, NS_VIEW_MANAGER_CID);
static NS_DEFINE_IID(kIViewManagerIID, NS_IVIEWMANAGER_IID);
static NS_DEFINE_IID(kScrollingViewCID, NS_SCROLLING_VIEW_CID);
static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);
static NS_DEFINE_IID(kScrollViewIID, NS_ISCROLLABLEVIEW_IID);
static NS_DEFINE_IID(kWidgetCID, NS_CHILD_CID);

static void
ShowDiffs(nsIFrame* k1, nsIFrame* k2, const nsRect& r1, const nsRect& r2)
{
  printf("verifyreflow: ");
  k1->ListTag(stdout);
  printf(" ");
  stdout << r1;
  printf(" != ");
  k2->ListTag(stdout);
  printf(" ");
  stdout << r2;
  printf("\n");
}

static void
CompareTrees(nsIFrame* aA, nsIFrame* aB)
{
  nsIFrame* k1, *k2;
  aA->FirstChild(k1);
  aB->FirstChild(k2);
  NS_ASSERTION(nsContainerFrame::LengthOf(k1) == nsContainerFrame::LengthOf(k2),
               "child counts don't match");

  nsRect r1, r2;
  nsIView* v1, *v2;
  nsIWidget* w1, *w2;
  for (;;) {
    if (nsnull == k1) {
      NS_ASSERTION(nsnull == k2, "child lists are different");
      break;
    }
    NS_ASSERTION(nsnull != k2, "child lists are different");

    // Verify that the frames are the same size
    k1->GetRect(r1);
    k2->GetRect(r2);
    if (r1 != r2) {
      ShowDiffs(k1, k2, r1, r2);
    }
    else {
      // Make sure either both have views or neither have views; if they
      // do have views, make sure the views are the same size. If the
      // views have widgets, make sure they both do or neither does. If
      // they do, make sure the widgets are the same size.
      k1->GetView(v1);
      k2->GetView(v2);
      if (nsnull != v1) {
        NS_ASSERTION(nsnull != v2, "child views are not matched");
        v1->GetBounds(r1);
        v2->GetBounds(r2);
        NS_ASSERTION(r1 == r2, "child views are different sizes");

        v1->GetWidget(w1);
        v2->GetWidget(w2);
        if (nsnull != w1) {
          NS_ASSERTION(nsnull != w2, "child widgets are not matched");
          w1->GetBounds(r1);
          w2->GetBounds(r2);
          NS_ASSERTION(r1 == r2, "child widgets are different sizes");
        }
        else {
          NS_ASSERTION(nsnull == w2, "child widgets are not matched");
        }
      }
      else {
        NS_ASSERTION(nsnull == v2, "child views are not matched");
      }

      // Compare the sub-trees too
      CompareTrees(k1, k2);
    }

    // Advance to next sibling
    k1->GetNextSibling(k1);
    k2->GetNextSibling(k2);
  }
}

// XXX: copy of nsWebWidget's ua.css loading code!!!
#define UA_CSS_URL "resource:/res/ua.css"

static nsIStyleSheet* gUAStyleSheet;

static nsresult
InitUAStyleSheet()
{
  nsresult rv = NS_OK;

  if (nsnull == gUAStyleSheet) {  // snarf one
    nsIURL* uaURL;
    rv = NS_NewURL(&uaURL, nsnull, UA_CSS_URL); // XXX this bites, fix it
    if (NS_OK == rv) {
      // Get an input stream from the url
      PRInt32 ec;
      nsIInputStream* in = uaURL->Open(&ec);
      if (nsnull != in) {
        // Translate the input using the argument character set id into unicode
        nsIUnicharInputStream* uin;
        rv = NS_NewConverterStream(&uin, nsnull, in);
        if (NS_OK == rv) {
          // Create parser and set it up to process the input file
          nsICSSParser* css;
          rv = NS_NewCSSParser(&css);
          if (NS_OK == rv) {
            // Parse the input and produce a style set
            // XXX note: we are ignoring rv until the error code stuff in the
            // input routines is converted to use nsresult's
            css->Parse(uin, uaURL, gUAStyleSheet);
            NS_RELEASE(css);
          }
          NS_RELEASE(uin);
        }
        NS_RELEASE(in);
      }
      else {
//        printf("open of %s failed: error=%x\n", UA_CSS_URL, ec);
        rv = NS_ERROR_ILLEGAL_VALUE;  // XXX need a better error code here
      }

      NS_RELEASE(uaURL);
    }
  }
  return rv;
}

static nsresult
CreateStyleSet(nsIDocument* aDocument, nsIStyleSet** aStyleSet)
{
  nsresult rv = InitUAStyleSheet();
  if (NS_OK != rv) {
    NS_WARNING("unable to load UA style sheet");
  }

  rv = NS_NewStyleSet(aStyleSet);
  if (NS_OK == rv) {
    PRInt32 index = aDocument->GetNumberOfStyleSheets();
    while (0 < index--) {
      // NOTE: turn the order around for the set
      nsIStyleSheet* sheet = aDocument->GetStyleSheetAt(index);
      (*aStyleSet)->AppendDocStyleSheet(sheet);
      NS_RELEASE(sheet);
    }
    // XXX this is just wrong, the UA style sheet should be owned by the UA
    // for that matter, the style set should be created by the UA too
    if (nsnull != gUAStyleSheet) {
      (*aStyleSet)->AppendBackstopStyleSheet(gUAStyleSheet);
    }
  }
  return rv;
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
  nsIStyleSet* ss;

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

  rv = CreateStyleSet(mDocument, &ss);
  NS_ASSERTION(NS_OK == rv, "failed to create style set");

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
//  tbounds *= mPresContext->GetPixelsToTwips();
  rv = nsRepository::CreateInstance(kScrollingViewCID, nsnull, kIViewIID,
                                    (void **) &view);
  if ((NS_OK != rv) || (NS_OK != view->Init(vm, tbounds, nsnull, &kWidgetCID,
                                            nsnull, nativeParentWidget))) {
    NS_ASSERTION(NS_OK == rv, "failed to create scroll view");
  }
  rv = view->QueryInterface(kScrollViewIID, (void**)&scrollView);
  if (NS_OK == rv) {
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

  // Create a new presentation shell to view the document
  rv = mDocument->CreateShell(cx, vm, ss, &sh);
  NS_ASSERTION(NS_OK == rv, "failed to create presentation shell");
  sh->ResizeReflow(r.width, r.height);

  // Now that the document has been reflowed, use its frame tree to
  // compare against our frame tree.
  CompareTrees(GetRootFrame(), sh->GetRootFrame());

  NS_RELEASE(vm);

  NS_RELEASE(cx);
  NS_RELEASE(sh);

  NS_RELEASE(ss);
}
#endif

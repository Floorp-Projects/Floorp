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
#include "nsIContentDelegate.h"
#include "nsIDocument.h"
#include "nsIDocumentObserver.h"
#include "nsIStyleSet.h"
#include "nsIStyleContext.h"
#include "nsFrame.h"
#include "nsReflowCommand.h"
#include "nsIViewManager.h"
#include "nsCRT.h"
#include "plhash.h"
#include "prlog.h"

#undef NOISY

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

//----------------------------------------------------------------------

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIPresShellIID, NS_IPRESSHELL_IID);
static NS_DEFINE_IID(kIDocumentObserverIID, NS_IDOCUMENT_OBSERVER_IID);

static PRLogModuleInfo* gShellLogModuleInfo;

class PresShell : public nsIPresShell, nsIDocumentObserver {
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
  NS_IMETHOD SetTitle(const nsString& aTitle);
  NS_IMETHOD BeginUpdate();
  NS_IMETHOD EndUpdate();
  NS_IMETHOD BeginLoad();
  NS_IMETHOD EndLoad();
  NS_IMETHOD BeginReflow(nsIPresShell* aShell);
  NS_IMETHOD EndReflow(nsIPresShell* aShell);
  NS_IMETHOD ContentChanged(nsIContent* aContent,
                            nsISupports* aSubContent);
  NS_IMETHOD ContentAppended(nsIContent* aContainer);
  NS_IMETHOD ContentInserted(nsIContent* aContainer,
                             nsIContent* aChild,
                             PRInt32 aIndexInContainer);
  NS_IMETHOD ContentReplaced(nsIContent* aContainer,
                             nsIContent* aOldChild,
                             nsIContent* aNewChild,
                             PRInt32 aIndexInContainer);
  NS_IMETHOD ContentWillBeRemoved(nsIContent* aContainer,
                                  nsIContent* aChild,
                                  PRInt32 aIndexInContainer);
  NS_IMETHOD ContentHasBeenRemoved(nsIContent* aContainer,
                                   nsIContent* aChild,
                                   PRInt32 aIndexInContainer);
  NS_IMETHOD StyleSheetAdded(nsIStyleSheet* aStyleSheet);

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
  virtual void ResizeReflow(nscoord aWidth, nscoord aHeight);
  virtual nsIFrame* GetRootFrame();
  virtual nsIFrame* FindFrameWithContent(nsIContent* aContent);
  virtual void AppendReflowCommand(nsReflowCommand* aReflowCommand);
  virtual void ProcessReflowCommands();
  virtual void PutCachedData(nsIFrame* aKeyFrame, void* aData);
  virtual void* GetCachedData(nsIFrame* aKeyFrame);
  virtual void* RemoveCachedData(nsIFrame* aKeyFrame);

protected:
  ~PresShell();

  void InitFrameVerifyTreeLogModuleInfo();

  FrameHashTable* mCache;
  nsIDocument* mDocument;
  nsIPresContext* mPresContext;
  nsIStyleSet* mStyleSet;
  nsIFrame* mRootFrame;
  nsIViewManager* mViewManager;
  PRUint32 mUpdateCount;
  nsVoidArray mReflowCommands;
  PRUint32 mReflowLockCount;
};

//----------------------------------------------------------------------

PresShell::PresShell()
{
#ifdef NS_DEBUG
  gShellLogModuleInfo = PR_NewLogModule("shell");
#endif
}

nsrefcnt
PresShell::AddRef(void)
{
  return ++mRefCnt;
}

nsrefcnt
PresShell::Release(void)
{
  NS_PRECONDITION(0 != mRefCnt, "bad refcnt");
  if (--mRefCnt == 0) {
    delete this;
    return 0;
  }
  return mRefCnt;
}

nsresult
PresShell::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (aIID.Equals(kIPresShellIID)) {
    *aInstancePtr = (void*) ((nsIPresShell*) this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIDocumentObserverIID)) {
    *aInstancePtr = (void*) ((nsIDocumentObserver*) this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*) ((nsISupports*) ((nsIPresShell*)this));
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

PresShell::~PresShell()
{
  delete mCache;

  if (nsnull != mDocument) {
    mDocument->DeleteShell(this);
    NS_RELEASE(mDocument);
  }
  if (nsnull != mRootFrame) {
    mRootFrame->DeleteFrame();
  }
  NS_IF_RELEASE(mPresContext);
  NS_IF_RELEASE(mViewManager);
  NS_IF_RELEASE(mStyleSet);
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

  mCache = new FrameHashTable();

  mDocument = aDocument;
  NS_ADDREF(aDocument);
  mViewManager = aViewManager;
  NS_ADDREF(mViewManager);

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

void
PresShell::ResizeReflow(nscoord aWidth, nscoord aHeight)
{
  EnterReflowLock();

  if (nsnull != mPresContext) {
    nsRect r(0, 0, aWidth, aHeight);
    mPresContext->SetVisibleArea(r);
  }

  nsReflowReason  reflowReason = eReflowReason_Resize;

  if (nsnull == mRootFrame) {
    if (nsnull != mDocument) {
      nsIContent* root = mDocument->GetRootContent();
      if (nsnull != root) {
        nsIContentDelegate* cd = root->GetDelegate(mPresContext);
        if (nsnull != cd) {
          nsIStyleContext* rootSC =
            mPresContext->ResolveStyleContextFor(root, nsnull);
          nsresult rv = cd->CreateFrame(mPresContext, root, nsnull,
                                        rootSC, mRootFrame);
          NS_RELEASE(rootSC);
          NS_RELEASE(cd);
          reflowReason = eReflowReason_Initial;

          // Bind root frame to root view (and root window)
          nsIView* rootView = mViewManager->GetRootView();
          mRootFrame->SetView(rootView);
          NS_RELEASE(rootView);
        }
        NS_RELEASE(root);
      }
    }
  }

  if (nsnull != mRootFrame) {
    // Kick off a top-down reflow
    NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
                 ("enter nsPresShell::ResizeReflow: %d,%d", aWidth, aHeight));
#ifdef NS_DEBUG
    if (nsIFrame::GetVerifyTreeEnable()) {
      mRootFrame->VerifyTree();
    }
#endif
    nsRect          bounds;
    mPresContext->GetVisibleArea(bounds);
    nsSize          maxSize(bounds.width, bounds.height);
    nsReflowMetrics desiredSize(nsnull);
    nsReflowStatus  status;
    nsReflowState   reflowState(mRootFrame, reflowReason, maxSize);

    mRootFrame->Reflow(mPresContext, desiredSize, reflowState, status);
    mRootFrame->SizeTo(desiredSize.width, desiredSize.height);
#ifdef NS_DEBUG
    if (nsIFrame::GetVerifyTreeEnable()) {
      mRootFrame->VerifyTree();
    }
#endif
    NS_FRAME_LOG(NS_FRAME_TRACE_CALLS, ("exit nsPresShell::ResizeReflow"));

    // XXX if debugging then we should assert that the cache is empty
  } else {
#ifdef NOISY
    printf("PresShell::ResizeReflow: null root frame\n");
#endif
  }

  ExitReflowLock();
}

nsIFrame*
PresShell::GetRootFrame()
{
  return mRootFrame;
}

NS_METHOD
PresShell::SetTitle(const nsString& aTitle)
{
  if (nsnull != mPresContext) {
    nsISupports* container;
    if (NS_OK == mPresContext->GetContainer(&container)) {
      if (nsnull != container) {
        nsIDocumentObserver* docob;
        if (NS_OK == container->QueryInterface(kIDocumentObserverIID,
                                               (void**) &docob)) {
          docob->SetTitle(aTitle);
          NS_RELEASE(docob);
        }
        NS_RELEASE(container);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
PresShell::BeginUpdate()
{
  mUpdateCount++;
  return NS_OK;
}

NS_IMETHODIMP
PresShell::EndUpdate()
{
  NS_PRECONDITION(0 != mUpdateCount, "too many EndUpdate's");
  if (--mUpdateCount == 0) {
    // XXX do something here
  }
  return NS_OK;
}

NS_IMETHODIMP
PresShell::BeginLoad()
{
  return NS_OK;
}

NS_IMETHODIMP
PresShell::EndLoad()
{
  return NS_OK;
}

NS_IMETHODIMP
PresShell::BeginReflow(nsIPresShell* aShell)
{
  return NS_OK;
}

NS_IMETHODIMP
PresShell::EndReflow(nsIPresShell* aShell)
{
  return NS_OK;
}

void
PresShell::AppendReflowCommand(nsReflowCommand* aReflowCommand)
{
  mReflowCommands.AppendElement(aReflowCommand);
}

void
PresShell::ProcessReflowCommands()
{
  if (0 != mReflowCommands.Count()) {
    nsReflowMetrics desiredSize(nsnull);

    while (0 != mReflowCommands.Count()) {
      nsReflowCommand* rc = (nsReflowCommand*) mReflowCommands.ElementAt(0);
      mReflowCommands.RemoveElementAt(0);

      // Dispatch the reflow command
      nsSize          maxSize;
      
      mRootFrame->GetSize(maxSize);
      PR_LOG(gShellLogModuleInfo, PR_LOG_DEBUG,
             ("PresShell::ProcessReflowCommands: reflow command type=%d",
              rc->GetType()));
      rc->Dispatch(desiredSize, maxSize);
      delete rc;
    }

    // Place and size the root frame
    mRootFrame->SizeTo(desiredSize.width, desiredSize.height);
#ifdef NS_DEBUG
    if (nsIFrame::GetVerifyTreeEnable()) {
      mRootFrame->VerifyTree();
    }
#endif
  }
}

#ifdef NS_DEBUG
static char*
ContentTag(nsIContent* aContent, PRIntn aSlot)
{
  static char buf0[100], buf1[100], buf2[100];
  static char* bufs[] = { buf0, buf1, buf2 };
  char* buf = bufs[aSlot];
  nsIAtom* atom = aContent->GetTag();
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
PresShell::ContentChanged(nsIContent*  aContent,
                          nsISupports* aSubContent)
{
  NS_PRECONDITION(nsnull != mRootFrame, "null root frame");

  EnterReflowLock();

  // Notify the first frame that maps the content. It will generate a reflow
  // command
  nsIFrame* frame = FindFrameWithContent(aContent);
  NS_PRECONDITION(nsnull != frame, "null frame");

  PR_LOG(gShellLogModuleInfo, PR_LOG_DEBUG,
         ("PresShell::ContentChanged: content=%p[%s] subcontent=%p frame=%p",
          aContent, ContentTag(aContent, 0),
          aSubContent, frame));
  frame->ContentChanged(this, mPresContext, aContent, aSubContent);

  ExitReflowLock();
  return NS_OK;
}

NS_IMETHODIMP
PresShell::ContentAppended(nsIContent* aContainer)
{
  NS_PRECONDITION(nsnull != mRootFrame, "null root frame");

  EnterReflowLock();

  nsIContent* parentContainer = aContainer;
  while (nsnull != parentContainer) {
    nsIFrame* frame = FindFrameWithContent(parentContainer);
    if (nsnull != frame) {
      PR_LOG(gShellLogModuleInfo, PR_LOG_DEBUG,
             ("PresShell::ContentAppended: container=%p[%s] frame=%p",
              aContainer, ContentTag(aContainer, 0), frame));
      frame->ContentAppended(this, mPresContext, aContainer);
      break;
    }
    parentContainer = parentContainer->GetParent();
  }

  ExitReflowLock();
  return NS_OK;
}

NS_IMETHODIMP
PresShell::ContentInserted(nsIContent* aContainer,
                           nsIContent* aChild,
                           PRInt32     aIndexInContainer)
{
  NS_PRECONDITION(nsnull != mRootFrame, "null root frame");

  EnterReflowLock();

  nsIFrame* frame = FindFrameWithContent(aContainer);
  NS_PRECONDITION(nsnull != frame, "null frame");
  PR_LOG(gShellLogModuleInfo, PR_LOG_DEBUG,
     ("PresShell::ContentInserted: container=%p[%s] child=%p[%s][%d] frame=%p",
      aContainer, ContentTag(aContainer, 0),
      aChild, ContentTag(aChild, 1), aIndexInContainer,
      frame));
  frame->ContentInserted(this, mPresContext, aContainer, aChild,
                         aIndexInContainer);

  ExitReflowLock();
  return NS_OK;
}

NS_IMETHODIMP
PresShell::ContentReplaced(nsIContent* aContainer,
                           nsIContent* aOldChild,
                           nsIContent* aNewChild,
                           PRInt32     aIndexInContainer)
{
  NS_PRECONDITION(nsnull != mRootFrame, "null root frame");

  EnterReflowLock();

  nsIFrame* frame = FindFrameWithContent(aContainer);
  NS_PRECONDITION(nsnull != frame, "null frame");
  PR_LOG(gShellLogModuleInfo, PR_LOG_DEBUG,
     ("PresShell::ContentReplaced: container=%p[%s] oldChild=%p[%s][%d] newChild=%p[%s] frame=%p",
      aContainer, ContentTag(aContainer, 0),
      aOldChild, ContentTag(aOldChild, 1), aIndexInContainer,
      aNewChild, ContentTag(aNewChild, 2), frame));
  frame->ContentReplaced(this, mPresContext, aContainer, aOldChild,
                         aNewChild, aIndexInContainer);

  ExitReflowLock();
  return NS_OK;
}

// XXX keep this?
NS_IMETHODIMP
PresShell::ContentWillBeRemoved(nsIContent* aContainer,
                                nsIContent* aChild,
                                PRInt32     aIndexInContainer)
{
  PR_LOG(gShellLogModuleInfo, PR_LOG_DEBUG,
     ("PresShell::ContentWillBeRemoved: container=%p[%s] child=%p[%s][%d]",
      aContainer, ContentTag(aContainer, 0),
      aChild, ContentTag(aChild, 1), aIndexInContainer));
  return NS_OK;
}

NS_IMETHODIMP
PresShell::ContentHasBeenRemoved(nsIContent* aContainer,
                                 nsIContent* aChild,
                                 PRInt32     aIndexInContainer)
{
  NS_PRECONDITION(nsnull != mRootFrame, "null root frame");

  nsIFrame* frame = FindFrameWithContent(aContainer);
  NS_PRECONDITION(nsnull != frame, "null frame");
  PR_LOG(gShellLogModuleInfo, PR_LOG_DEBUG,
     ("PresShell::ContentHasBeenRemoved: container=%p child=%p[%d] frame=%p",
      aContainer, aChild, aIndexInContainer, frame));
  frame->ContentDeleted(this, mPresContext, aContainer, aChild, aIndexInContainer);
  ProcessReflowCommands();
  return NS_OK;
}

NS_IMETHODIMP
PresShell::StyleSheetAdded(nsIStyleSheet* aStyleSheet)
{
  return NS_OK;
}

static nsIFrame*
FindFrameWithContent(nsIFrame* aFrame, nsIContent* aContent)
{
  nsIContent* frameContent;
   
  aFrame->GetContent(frameContent);
  if (frameContent == aContent) {
    NS_RELEASE(frameContent);
    return aFrame;
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

void
PresShell::PutCachedData(nsIFrame* aKeyFrame, void* aData)
{
  void* oldData = mCache->Put(aKeyFrame, aData);
  NS_ASSERTION(nsnull == oldData, "bad cached data usage");
}

void*
PresShell::GetCachedData(nsIFrame* aKeyFrame)
{
  return mCache->Get(aKeyFrame);
}

void*
PresShell::RemoveCachedData(nsIFrame* aKeyFrame)
{
  return mCache->Remove(aKeyFrame);
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

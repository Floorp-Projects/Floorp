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
#include "nsIStyleSet.h"
#include "nsIStyleContext.h"
#include "nsFrame.h"
#include "nsReflowCommand.h"
#include "nsIViewManager.h"
#include "nsIView.h"
#include "nsCRT.h"
#include "plhash.h"

#undef NOISY
#define TEMPORARY_CONTENT_APPENDED_HACK

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
static NS_DEFINE_IID(kIDocumentObserverIID, NS_IDOCUMENTOBSERVER_IID);

class PresShell : public nsIPresShell {
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
  virtual void BeginUpdate();
  virtual void EndUpdate();
  virtual void ContentChanged(nsIContent* aContent,
                              nsISupports* aSubContent);
  virtual void ContentAppended(nsIContent* aContainer);
  virtual void ContentInserted(nsIContent* aContainer,
                               nsIContent* aChild,
                               PRInt32 aIndexInContainer);
  virtual void ContentReplaced(nsIContent* aContainer,
                               nsIContent* aOldChild,
                               nsIContent* aNewChild,
                               PRInt32 aIndexInContainer);
  virtual void ContentWillBeRemoved(nsIContent* aContainer,
                                    nsIContent* aChild,
                                    PRInt32 aIndexInContainer);
  virtual void ContentHasBeenRemoved(nsIContent* aContainer,
                                     nsIContent* aChild,
                                     PRInt32 aIndexInContainer);
  virtual void StyleSheetAdded(nsIStyleSheet* aStyleSheet);

  // nsIPresShell
  virtual nsresult Init(nsIDocument* aDocument,
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
#ifdef TEMPORARY_CONTENT_APPENDED_HACK
  PRUint32 mResizeReflows;
#endif
};

//----------------------------------------------------------------------

PresShell::PresShell()
{
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
  printf("enter reflow lock\n");
  ++mReflowLockCount;
  return NS_OK;
}

NS_METHOD
PresShell::ExitReflowLock()
{
  printf("exit reflow lock\n");
  PRUint32 newReflowLockCount = mReflowLockCount - 1;
  if (newReflowLockCount == 0) {
#ifdef TEMPORARY_CONTENT_APPENDED_HACK
    if (0 != mResizeReflows) {
      mResizeReflows = 0;
      nsRect r;
      mRootFrame->GetRect(r);
      ResizeReflow(r.width, r.height);
    }
#endif
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
  if (nsnull != mPresContext) {
    nsRect r(0, 0, aWidth, aHeight);
    mPresContext->SetVisibleArea(r);
  }

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
    nsRect          bounds = mPresContext->GetVisibleArea();
    nsSize          maxSize(bounds.width, bounds.height);
    nsReflowMetrics desiredSize;
    nsReflowStatus  status;
    mRootFrame->ResizeReflow(mPresContext, desiredSize, maxSize,
                             nsnull, status);
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

void
PresShell::BeginUpdate()
{
  mUpdateCount++;
}

void
PresShell::EndUpdate()
{
  NS_PRECONDITION(0 != mUpdateCount, "too many EndUpdate's");
  if (--mUpdateCount == 0) {
    // XXX do something here
  }
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
    nsReflowMetrics desiredSize;

    while (0 != mReflowCommands.Count()) {
      nsReflowCommand* rc = (nsReflowCommand*) mReflowCommands.ElementAt(0);
      mReflowCommands.RemoveElementAt(0);

      // Dispatch the reflow command
      nsSize          maxSize;
      
      mRootFrame->GetSize(maxSize);
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

void
PresShell::ContentChanged(nsIContent* aContent,
                          nsISupports* aSubContent)
{
  NS_PRECONDITION(nsnull != mRootFrame, "null root frame");
  NS_PRECONDITION(0 != mReflowLockCount, "unlocked reflow");

#ifdef TEMPORARY_CONTENT_APPENDED_HACK
  mResizeReflows++;
#else
  // Notify the first frame that maps the content. It will generate a reflow
  // command
  nsIFrame* frame = FindFrameWithContent(aContent);
  NS_PRECONDITION(nsnull != frame, "null frame");
  frame->ContentChanged(this, mPresContext, aContent, aSubContent);

  if (1 == mReflowLockCount) {
    ProcessReflowCommands();
  }
#endif
}

void
PresShell::ContentAppended(nsIContent* aContainer)
{
  NS_PRECONDITION(nsnull != mRootFrame, "null root frame");

  nsIContent* parentContainer = aContainer;
  while (nsnull != parentContainer) {
    nsIFrame* frame = FindFrameWithContent(parentContainer);
    if (nsnull != frame) {
      frame->ContentAppended(this, mPresContext, aContainer);
      ProcessReflowCommands();
      break;
    }
    parentContainer = parentContainer->GetParent();
  }
}

void
PresShell::ContentInserted(nsIContent* aContainer,
                           nsIContent* aChild,
                           PRInt32     aIndexInContainer)
{
  NS_PRECONDITION(nsnull != mRootFrame, "null root frame");

  nsIFrame* frame = FindFrameWithContent(aContainer);
  NS_PRECONDITION(nsnull != frame, "null frame");
  frame->ContentInserted(this, mPresContext, aContainer, aChild, aIndexInContainer);
  ProcessReflowCommands();
}

void
PresShell::ContentReplaced(nsIContent* aContainer,
                           nsIContent* aOldChild,
                           nsIContent* aNewChild,
                           PRInt32     aIndexInContainer)
{
  NS_PRECONDITION(nsnull != mRootFrame, "null root frame");

  nsIFrame* frame = FindFrameWithContent(aContainer);
  NS_PRECONDITION(nsnull != frame, "null frame");
  frame->ContentReplaced(this, mPresContext, aContainer, aOldChild,
    aNewChild, aIndexInContainer);
  ProcessReflowCommands();
}

// XXX keep this?
void
PresShell::ContentWillBeRemoved(nsIContent* aContainer,
                                nsIContent* aChild,
                                PRInt32     aIndexInContainer)
{
}

void
PresShell::ContentHasBeenRemoved(nsIContent* aContainer,
                                 nsIContent* aChild,
                                 PRInt32     aIndexInContainer)
{
  NS_PRECONDITION(nsnull != mRootFrame, "null root frame");

  nsIFrame* frame = FindFrameWithContent(aContainer);
  NS_PRECONDITION(nsnull != frame, "null frame");
  frame->ContentDeleted(this, mPresContext, aContainer, aChild, aIndexInContainer);
  ProcessReflowCommands();
}

void
PresShell::StyleSheetAdded(nsIStyleSheet* aStyleSheet)
{
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

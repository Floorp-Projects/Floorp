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
 *   Steve Clark <buster@netscape.com>
 *   Håkan Waara <hwaara@chello.se>
 *   Dan Rosen <dr@netscape.com>
 *   Daniel Glazman <glazman@netscape.com>
 *
 *   IBM Corporation
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 05/03/2000   IBM Corp.       Observer events for reflow states
 */ 

#define PL_ARENA_CONST_ALIGN_MASK 3
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDocumentObserver.h"
#include "nsIStyleSet.h"
#include "nsICSSStyleSheet.h" // XXX for UA sheet loading hack, can this go away please?
#include "nsIDOMCSSStyleSheet.h"  // for Pref-related rule management (bugs 22963,20760,31816)
#include "nsINameSpaceManager.h"  // for Pref-related rule management (bugs 22963,20760,31816)
#include "nsIServiceManager.h"
#include "nsFrame.h"
#include "nsHTMLReflowCommand.h"
#include "nsIViewManager.h"
#include "nsCRT.h"
#include "prlog.h"
#include "prmem.h"
#include "prinrval.h"
#include "nsVoidArray.h"
#include "nsHashtable.h"
#include "nsIPref.h"
#include "nsIViewObserver.h"
#include "nsContainerFrame.h"
#include "nsIDeviceContext.h"
#include "nsIEventStateManager.h"
#include "nsIDOMEvent.h"
#include "nsGUIEvent.h"
#include "nsHTMLParts.h"
#include "nsISelection.h"
#include "nsISelectionController.h"
#include "nsReflowPath.h"
#include "nsLayoutCID.h"
#include "nsLayoutAtoms.h"
#include "nsIDOMRange.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMElement.h"
#include "nsHTMLAtoms.h"
#include "nsCSSPseudoElements.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsWeakReference.h"
#include "nsIPageSequenceFrame.h"
#include "nsICaret.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIXMLDocument.h"
#include "nsIDOMXMLDocument.h"
#include "nsIScrollableView.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsIFrameSelection.h"
#include "nsIDOMNSHTMLInputElement.h" //optimization for ::DoXXX commands
#include "nsIDOMNSHTMLTextAreaElement.h"
#include "nsViewsCID.h"
#include "nsIFrameManager.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsILayoutHistoryState.h"
#include "nsIScrollPositionListener.h"
#include "nsICompositeListener.h"
#include "nsILineIterator.h" // for ScrollFrameIntoView
#include "nsTimer.h"
#include "nsWeakPtr.h"
#include "plarena.h"
#include "nsIObserverService.h" // for reflow observation
#include "nsIDocShell.h"        // for reflow observation
#include "nsIDOMRange.h"
#ifdef MOZ_PERF_METRICS
#include "nsITimeRecorder.h"
#endif
#ifdef NS_DEBUG
#include "nsIFrameDebug.h"
#endif
  // for |#ifdef DEBUG| code
#include "nsSpaceManager.h"
#include "prenv.h"

#ifdef MOZ_REFLOW_PERF_DSP
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#endif

#include "nsIReflowCallback.h"

#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindowInternal.h"
#include "nsPIDOMWindow.h"
#include "nsIFocusController.h"
#include "nsIPrintPreviewContext.h"

// Drag & Drop, Clipboard
#include "nsWidgetsCID.h"
#include "nsIClipboard.h"
#include "nsIClipboardHelper.h"
#include "nsIDocShellTreeItem.h"
#include "nsIURI.h"
#include "nsIEventQueue.h"
#include "nsIEventQueueService.h"
#include "nsIScrollableFrame.h"
#include "prtime.h"
#include "prlong.h"
#include "nsIDragService.h"
#include "nsCopySupport.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLLinkElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsITimer.h"
#include "nsITimerInternal.h"

// For style data reconstruction
#include "nsStyleChangeList.h"
#include "nsIStyleFrameConstruction.h"
#include "nsINameSpaceManager.h"
#include "nsIBindingManager.h"
#include "nsIMenuFrame.h"
#include "nsITreeBoxObject.h"
#include "nsIXBLBinding.h"
#include "nsPlaceholderFrame.h"

// Dummy layout request
#include "nsDummyLayoutRequest.h"

// Content viewer interfaces
#include "nsIContentViewer.h"
#include "nsIDocumentViewer.h"

#ifdef IBMBIDI
#include "nsIBidiKeyboard.h"
#endif // IBMBIDI

#include "nsContentCID.h"
static NS_DEFINE_CID(kCSSStyleSheetCID, NS_CSS_STYLESHEET_CID);
static NS_DEFINE_CID(kStyleSetCID, NS_STYLESET_CID);
static NS_DEFINE_IID(kRangeCID,     NS_RANGE_CID);
static NS_DEFINE_CID(kPrintPreviewContextCID,  NS_PRINT_PREVIEW_CONTEXT_CID);

// convert a color value to a string, in the CSS format #RRGGBB
// *  - initially created for bugs 31816, 20760, 22963
static void ColorToString(nscolor aColor, nsAutoString &aString);


// local management of the style watch:
//  aCtlValue should be set to all actions desired (bitwise OR'd together)
//  aStyleSet cannot be null
// NOTE: implementation is noop unless MOZ_PERF_METRICS is defined
static nsresult CtlStyleWatch(PRUint32 aCtlValue, nsIStyleSet *aStyleSet);
#define kStyleWatchEnable  1
#define kStyleWatchDisable 2
#define kStyleWatchPrint   4
#define kStyleWatchStart   8
#define kStyleWatchStop    16
#define kStyleWatchReset   32

// Class ID's
static NS_DEFINE_CID(kFrameSelectionCID, NS_FRAMESELECTION_CID);
static NS_DEFINE_CID(kEventQueueServiceCID,   NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kViewCID, NS_VIEW_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

#undef NOISY

// ----------------------------------------------------------------------

#ifdef NS_DEBUG
// Set the environment variable GECKO_VERIFY_REFLOW_FLAGS to one or
// more of the following flags (comma separated) for handy debug
// output.
static PRUint32 gVerifyReflowFlags;

struct VerifyReflowFlags {
  char*    name;
  PRUint32 bit;
};

static VerifyReflowFlags gFlags[] = {
  { "verify",                VERIFY_REFLOW_ON },
  { "reflow",                VERIFY_REFLOW_NOISY },
  { "all",                   VERIFY_REFLOW_ALL },
  { "list-commands",         VERIFY_REFLOW_DUMP_COMMANDS },
  { "noisy-commands",        VERIFY_REFLOW_NOISY_RC },
  { "really-noisy-commands", VERIFY_REFLOW_REALLY_NOISY_RC },
  { "space-manager",         VERIFY_REFLOW_INCLUDE_SPACE_MANAGER },
  { "resize",                VERIFY_REFLOW_DURING_RESIZE_REFLOW },
};

#define NUM_VERIFY_REFLOW_FLAGS (sizeof(gFlags) / sizeof(gFlags[0]))

static void
ShowVerifyReflowFlags()
{
  printf("Here are the available GECKO_VERIFY_REFLOW_FLAGS:\n");
  VerifyReflowFlags* flag = gFlags;
  VerifyReflowFlags* limit = gFlags + NUM_VERIFY_REFLOW_FLAGS;
  while (flag < limit) {
    printf("  %s\n", flag->name);
    ++flag;
  }
  printf("Note: GECKO_VERIFY_REFLOW_FLAGS is a comma separated list of flag\n");
  printf("names (no whitespace)\n");
}
#endif

//========================================================================
//========================================================================
//========================================================================
#ifdef MOZ_REFLOW_PERF
class ReflowCountMgr;

static const char * kGrandTotalsStr = "Grand Totals";
#define NUM_REFLOW_TYPES 5

// Counting Class
class ReflowCounter {
public:
  ReflowCounter(ReflowCountMgr * aMgr = nsnull);
  ~ReflowCounter();

  void ClearTotals();
  void DisplayTotals(const char * aStr);
  void DisplayDiffTotals(const char * aStr);
  void DisplayHTMLTotals(const char * aStr);

  void Add(nsReflowReason aType)                  { mTotals[aType]++;         }
  void Add(nsReflowReason aType, PRUint32 aTotal) { mTotals[aType] += aTotal; }

  void CalcDiffInTotals();
  void SetTotalsCache();

  void SetMgr(ReflowCountMgr * aMgr) { mMgr = aMgr; }

  PRUint32 GetTotalByType(nsReflowReason aType) { if (aType >= eReflowReason_Initial && aType <= eReflowReason_Dirty) return mTotals[aType]; else return 0; }
  
protected:
  void DisplayTotals(PRUint32 * aArray, const char * aTitle);
  void DisplayHTMLTotals(PRUint32 * aArray, const char * aTitle);

  PRUint32 mTotals[NUM_REFLOW_TYPES];
  PRUint32 mCacheTotals[NUM_REFLOW_TYPES];

  ReflowCountMgr * mMgr; // weak reference (don't delete)
};

// Counting Class
class IndiReflowCounter {
public:
  IndiReflowCounter(ReflowCountMgr * aMgr = nsnull)
    : mFrame(nsnull),
      mCount(0),
      mMgr(aMgr),
      mCounter(aMgr),
      mHasBeenOutput(PR_FALSE)
    {}
  virtual ~IndiReflowCounter() {}

  nsAutoString mName;
  nsIFrame *   mFrame;   // weak reference (don't delete)
  PRInt32      mCount;

  ReflowCountMgr * mMgr; // weak reference (don't delete)

  ReflowCounter mCounter;
  PRBool        mHasBeenOutput;

};

//--------------------
// Manager Class
//--------------------
class ReflowCountMgr {
public:
  ReflowCountMgr();
  virtual ~ReflowCountMgr();

  void ClearTotals();
  void ClearGrandTotals();
  void DisplayTotals(const char * aStr);
  void DisplayHTMLTotals(const char * aStr);
  void DisplayDiffsInTotals(const char * aStr);

  void Add(const char * aName, nsReflowReason aType, nsIFrame * aFrame);
  ReflowCounter * LookUp(const char * aName);

  void PaintCount(const char * aName, nsIRenderingContext* aRenderingContext, nsIPresContext* aPresContext, nsIFrame * aFrame, PRUint32 aColor);

  FILE * GetOutFile() { return mFD; }

  PLHashTable * GetIndiFrameHT() { return mIndiFrameCounts; }

  void SetPresContext(nsIPresContext * aPresContext) { mPresContext = aPresContext; } // weak reference
  void SetPresShell(nsIPresShell* aPresShell) { mPresShell= aPresShell; } // weak reference

  void SetDumpFrameCounts(PRBool aVal)         { mDumpFrameCounts = aVal; }
  void SetDumpFrameByFrameCounts(PRBool aVal)  { mDumpFrameByFrameCounts = aVal; }
  void SetPaintFrameCounts(PRBool aVal)        { mPaintFrameByFrameCounts = aVal; }

protected:
  void DisplayTotals(PRUint32 * aArray, PRUint32 * aDupArray, char * aTitle);
  void DisplayHTMLTotals(PRUint32 * aArray, PRUint32 * aDupArray, char * aTitle);

  PR_STATIC_CALLBACK(PRIntn) RemoveItems(PLHashEntry *he, PRIntn i, void *arg);
  PR_STATIC_CALLBACK(PRIntn) RemoveIndiItems(PLHashEntry *he, PRIntn i, void *arg);
  void CleanUp();

  // stdout Output Methods
  PR_STATIC_CALLBACK(PRIntn) DoSingleTotal(PLHashEntry *he, PRIntn i, void *arg);
  PR_STATIC_CALLBACK(PRIntn) DoSingleIndi(PLHashEntry *he, PRIntn i, void *arg);

  void DoGrandTotals();
  void DoIndiTotalsTree();

  // HTML Output Methods
  PR_STATIC_CALLBACK(PRIntn) DoSingleHTMLTotal(PLHashEntry *he, PRIntn i, void *arg);
  void DoGrandHTMLTotals();

  // Zero Out the Totals
  PR_STATIC_CALLBACK(PRIntn) DoClearTotals(PLHashEntry *he, PRIntn i, void *arg);

  // Displays the Diff Totals
  PR_STATIC_CALLBACK(PRIntn) DoDisplayDiffTotals(PLHashEntry *he, PRIntn i, void *arg);

  PLHashTable * mCounts;
  PLHashTable * mIndiFrameCounts;
  FILE * mFD;
  
  PRBool mDumpFrameCounts;
  PRBool mDumpFrameByFrameCounts;
  PRBool mPaintFrameByFrameCounts;

  PRBool mCycledOnce;

  // Root Frame for Individual Tracking
  nsIPresContext * mPresContext;
  nsIPresShell*    mPresShell;

  // ReflowCountMgr gReflowCountMgr;
};
#endif
//========================================================================

// comment out to hide caret
#define SHOW_CARET

// The upper bound on the amount of time to spend reflowing, in
// microseconds.  When this bound is exceeded and reflow commands are
// still queued up, a reflow event is posted.  The idea is for reflow
// to not hog the processor beyond the time specifed in
// gMaxRCProcessingTime.  This data member is initialized from the
// layout.reflow.timeslice pref.
#define NS_MAX_REFLOW_TIME    1000000
static PRInt32 gMaxRCProcessingTime = -1;

// Set to true to enable async reflow during document load.
// This flag is initialized from the layout.reflow.async.duringDocLoad pref.
static PRBool gAsyncReflowDuringDocLoad = PR_FALSE;

// Largest chunk size we recycle
static const size_t gMaxRecycledSize = 400;

#define MARK_INCREMENT 50
#define BLOCK_INCREMENT 4044 /* a bit under 4096, for malloc overhead */

/**A block of memory that the stack will 
 * chop up and hand out
 */
struct StackBlock {
   
   // a block of memory.  Note that this must be first so that it will
   // be aligned.
   char mBlock[BLOCK_INCREMENT];

   // another block of memory that would only be created
   // if our stack overflowed. Yes we have the ability
   // to grow on a stack overflow
   StackBlock* mNext;

   StackBlock() : mNext(nsnull) { }
   ~StackBlock() { }
};

/* we hold an array of marks. A push pushes a mark on the stack
 * a pop pops it off.
 */
struct StackMark {
   // the block of memory we are currently handing out chunks of
   StackBlock* mBlock;
   
   // our current position in the memory
   size_t mPos;
};


/* A stack arena allows a stack based interface to a block of memory.
 * It should be used when you need to allocate some temporary memory that
 * you will immediately return.
 */
class StackArena {
public:
  StackArena();
  ~StackArena();

  // Memory management functions
  nsresult  Allocate(size_t aSize, void** aResult);
  nsresult  Push();
  nsresult  Pop();

private:
  // our current position in memory
  size_t mPos;

  // a list of memory block. Usually there is only one
  // but if we overrun our stack size we can get more memory.
  StackBlock* mBlocks;

  // the current block of memory we are passing our chucks of
  StackBlock* mCurBlock;

  // our stack of mark where push has been called
  StackMark* mMarks;

  // the current top of the the mark list
  PRUint32 mStackTop;

  // the size of the mark array
  PRUint32 mMarkLength;
};



StackArena::StackArena()
{
  // allocate the marks array
  mMarkLength = MARK_INCREMENT;
  mMarks = new StackMark[mMarkLength];

  // allocate our stack memory
  mBlocks = new StackBlock();
  mCurBlock = mBlocks;

  mStackTop = 0;
  mPos = 0;
}

StackArena::~StackArena()
{
  // free up our data
  delete[] mMarks;
  while(mBlocks)
  {
    StackBlock* toDelete = mBlocks;
    mBlocks = mBlocks->mNext;
    delete toDelete;
  }
} 

nsresult
StackArena::Push()
{
  // if the we overrun our mark array. Resize it.
  if (mStackTop + 1 >= mMarkLength)
  {
    StackMark* oldMarks = mMarks;
    PRUint32 oldLength = mMarkLength;
    mMarkLength += MARK_INCREMENT;
    mMarks = new StackMark[mMarkLength];
    memcpy(mMarks, oldMarks, sizeof(StackMark)*oldLength);

    delete[] oldMarks;
  }

  // set a mark at the top
  mMarks[mStackTop].mBlock = mCurBlock;
  mMarks[mStackTop].mPos = mPos;

  mStackTop++;

  return NS_OK;
}

nsresult
StackArena::Allocate(size_t aSize, void** aResult)
{
  NS_ASSERTION(mStackTop > 0, "Error allocate called before push!!!");

  // make sure we are aligned. Beard said 8 was safer then 4. 
  // Round size to multiple of 8
  aSize = PR_ROUNDUP(aSize, 8);

  // if the size makes the stack overflow. Grab another block for the stack
  if (mPos + aSize >= BLOCK_INCREMENT)
  {
    NS_ASSERTION(aSize <= BLOCK_INCREMENT,"Requested memory is greater that our block size!!");
    if (mCurBlock->mNext == nsnull)
      mCurBlock->mNext = new StackBlock();

    mCurBlock =  mCurBlock->mNext;
    mPos = 0;
  }

  // return the chunk they need.
  *aResult = mCurBlock->mBlock + mPos;
  mPos += aSize;

  return NS_OK;
}

nsresult
StackArena::Pop()
{
  // pop off the mark
  NS_ASSERTION(mStackTop > 0, "Error Pop called 1 too many times");
  mStackTop--;

#ifdef DEBUG
  // Mark the "freed" memory with 0xdd to help with debugging of memory
  // allocation problems.
  {
    StackBlock *block = mMarks[mStackTop].mBlock, *block_end = mCurBlock;
    size_t pos = mMarks[mStackTop].mPos;
    for (; block != block_end; block = block->mNext, pos = 0) {
      memset(block->mBlock + pos, 0xdd, sizeof(block->mBlock) - pos);
    }
    memset(block->mBlock + pos, 0xdd, mPos - pos);
  }
#endif

  mCurBlock = mMarks[mStackTop].mBlock;
  mPos      = mMarks[mStackTop].mPos;

  return NS_OK;
}

// Uncomment this to disable the frame arena.
//#define DEBUG_TRACEMALLOC_FRAMEARENA 1

// Memory is allocated 4-byte aligned. We have recyclers for chunks up to
// 200 bytes
class FrameArena {
public:
  FrameArena(PRUint32 aArenaSize = 4096);
  ~FrameArena();

  // Memory management functions
  nsresult  AllocateFrame(size_t aSize, void** aResult);
  nsresult  FreeFrame(size_t aSize, void* aPtr);

private:
#if !defined(DEBUG_TRACEMALLOC_FRAMEARENA)
  // Underlying arena pool
  PLArenaPool mPool;

  // The recycler array is sparse with the indices being multiples of 4,
  // i.e., 0, 4, 8, 12, 16, 20, ...
  void*       mRecyclers[gMaxRecycledSize >> 2];
#endif
};

FrameArena::FrameArena(PRUint32 aArenaSize)
{
#if !defined(DEBUG_TRACEMALLOC_FRAMEARENA)
  // Initialize the arena pool
  PL_INIT_ARENA_POOL(&mPool, "FrameArena", aArenaSize);

  // Zero out the recyclers array
  memset(mRecyclers, 0, sizeof(mRecyclers));
#endif
}

FrameArena::~FrameArena()
{
#if !defined(DEBUG_TRACEMALLOC_FRAMEARENA)
  // Free the arena in the pool and finish using it
  PL_FinishArenaPool(&mPool);
#endif
} 

nsresult
FrameArena::AllocateFrame(size_t aSize, void** aResult)
{
#if defined(DEBUG_TRACEMALLOC_FRAMEARENA)
  *aResult = PR_Malloc(aSize);
#else
  void* result = nsnull;
  
  // Ensure we have correct alignment for pointers.  Important for Tru64
  aSize = PR_ROUNDUP(aSize, sizeof(void*));

  // Check recyclers first
  if (aSize < gMaxRecycledSize) {
    const int   index = aSize >> 2;

    result = mRecyclers[index];
    if (result) {
      // Need to move to the next object
      void* next = *((void**)result);
      mRecyclers[index] = next;
    }
  }

  if (!result) {
    // Allocate a new chunk from the arena
    PL_ARENA_ALLOCATE(result, &mPool, aSize);
  }

  *aResult = result;
#endif
  return NS_OK;
}

nsresult
FrameArena::FreeFrame(size_t aSize, void* aPtr)
{
#ifdef DEBUG
  // Mark the memory with 0xdd in DEBUG builds so that there will be
  // problems if someone tries to access memory that they've freed.
  memset(aPtr, 0xdd, aSize);
#endif
#if defined(DEBUG_TRACEMALLOC_FRAMEARENA)
  PR_Free(aPtr);
#else
  // Ensure we have correct alignment for pointers.  Important for Tru64
  aSize = PR_ROUNDUP(aSize, sizeof(void*));

  // See if it's a size that we recycle
  if (aSize < gMaxRecycledSize) {
    const int   index = aSize >> 2;
    void*       currentTop = mRecyclers[index];
    mRecyclers[index] = aPtr;
    *((void**)aPtr) = currentTop;
  }
#endif
  return NS_OK;
}

class PresShellViewEventListener : public nsIScrollPositionListener,
                                   public nsICompositeListener
{
public:
  PresShellViewEventListener();
  virtual ~PresShellViewEventListener();

  NS_DECL_ISUPPORTS

  // nsIScrollPositionListener methods
  NS_IMETHOD ScrollPositionWillChange(nsIScrollableView *aView, nscoord aX, nscoord aY);
  NS_IMETHOD ScrollPositionDidChange(nsIScrollableView *aView, nscoord aX, nscoord aY);

  // nsICompositeListener methods
	NS_IMETHOD WillRefreshRegion(nsIViewManager *aViewManager,
                               nsIView *aView,
                               nsIRenderingContext *aContext,
                               nsIRegion *aRegion,
                               PRUint32 aUpdateFlags);

	NS_IMETHOD DidRefreshRegion(nsIViewManager *aViewManager,
                              nsIView *aView,
                              nsIRenderingContext *aContext,
                              nsIRegion *aRegion,
                              PRUint32 aUpdateFlags);

	NS_IMETHOD WillRefreshRect(nsIViewManager *aViewManager,
                             nsIView *aView,
                             nsIRenderingContext *aContext,
                             const nsRect *aRect,
                             PRUint32 aUpdateFlags);

	NS_IMETHOD DidRefreshRect(nsIViewManager *aViewManager,
                            nsIView *aView,
                            nsIRenderingContext *aContext,
                            const nsRect *aRect,
                            PRUint32 aUpdateFlags);

  nsresult SetPresShell(nsIPresShell *aPresShell);

private:

  nsresult HideCaret();
  nsresult RestoreCaretVisibility();

  nsIPresShell *mPresShell;
  PRBool        mWasVisible;
  PRInt32       mCallCount;
};

struct nsDOMEventRequest 
{
  nsIContent* content;
  nsEvent* event;
  nsDOMEventRequest* next;
};

struct nsAttributeChangeRequest 
{
  nsIContent* content;
  PRInt32 nameSpaceID;
  nsIAtom* name;
  nsAutoString value;
  PRBool notify;
  nsAttributeChangeType type;
  nsAttributeChangeRequest* next;
};

struct nsCallbackEventRequest
{
  nsIReflowCallback* callback;
  nsCallbackEventRequest* next;
};


PRInt32 nsDummyLayoutRequest::gRefCnt;
nsIURI* nsDummyLayoutRequest::gURI;

NS_IMPL_ADDREF(nsDummyLayoutRequest);
NS_IMPL_RELEASE(nsDummyLayoutRequest);
NS_IMPL_QUERY_INTERFACE2(nsDummyLayoutRequest, nsIRequest, nsIChannel);

nsresult
nsDummyLayoutRequest::Create(nsIRequest** aResult, nsIPresShell* aPresShell)
{
  nsDummyLayoutRequest* request = new nsDummyLayoutRequest(aPresShell);
  if (!request)
      return NS_ERROR_OUT_OF_MEMORY;

  return request->QueryInterface(NS_GET_IID(nsIRequest), (void**) aResult); 
}


nsDummyLayoutRequest::nsDummyLayoutRequest(nsIPresShell* aPresShell)
{
  NS_INIT_ISUPPORTS();

  if (gRefCnt++ == 0) {
      nsresult rv;
      rv = NS_NewURI(&gURI, "about:layout-dummy-request", nsnull);
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create about:layout-dummy-request");
  }

  mPresShell = getter_AddRefs(NS_GetWeakReference(aPresShell));
}


nsDummyLayoutRequest::~nsDummyLayoutRequest()
{
  if (--gRefCnt == 0) {
      NS_IF_RELEASE(gURI);
  }
}

NS_IMETHODIMP
nsDummyLayoutRequest::Cancel(nsresult status)
{
  // Cancel layout
  nsresult rv = NS_OK;
  nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShell);
  if (presShell) {
    rv = presShell->CancelAllReflowCommands();
  }
  return rv;
}

// ----------------------------------------------------------------------------

/**
 * Used to build and maintain the incremental reflow tree, and
 * dispatch incremental reflows to individual reflow roots.
 */
class IncrementalReflow
{
public:
  ~IncrementalReflow();

  /**
   * Add a reflow command to the set of commands that are to be
   * dispatched in the incremental reflow. Returns `true' if
   * successful, `false' if the command could not be added (and thus,
   * should be re-scheduled).
   */
  PRBool
  AddCommand(nsIPresContext      *aPresContext,
             nsHTMLReflowCommand *aCommand);

  /**
   * Dispatch the incremental reflow.
   */
  void
  Dispatch(nsIPresContext      *aPresContext,
           nsHTMLReflowMetrics &aDesiredSize,
           const nsSize        &aMaxSize,
           nsIRenderingContext &aRendContext);

#ifdef NS_DEBUG
  /**
   * Dump the incremental reflow state.
   */
  void
  Dump(nsIPresContext *aPresContext) const;
#endif

protected:
  /**
   * The set of incremental reflow roots.
   */
  nsAutoVoidArray mRoots;
};

IncrementalReflow::~IncrementalReflow()
{
  for (PRInt32 i = mRoots.Count() - 1; i >= 0; --i)
    delete NS_STATIC_CAST(nsReflowPath *, mRoots[i]);
}

void
IncrementalReflow::Dispatch(nsIPresContext      *aPresContext,
                            nsHTMLReflowMetrics &aDesiredSize,
                            const nsSize        &aMaxSize,
                            nsIRenderingContext &aRendContext)
{
  for (PRInt32 i = mRoots.Count() - 1; i >= 0; --i) {
    // Send an incremental reflow notification to the first frame in the
    // path.
    nsReflowPath *path = NS_STATIC_CAST(nsReflowPath *, mRoots[i]);
    nsIFrame *first = path->mFrame;

    nsCOMPtr<nsIPresShell> shell;
    aPresContext->GetShell(getter_AddRefs(shell));

    nsIFrame* root;
    shell->GetRootFrame(&root);

    first->WillReflow(aPresContext);
    nsContainerFrame::PositionFrameView(aPresContext, first);

    // If the first frame in the path is the root of the frame
    // hierarchy, then use all the available space. If it's simply a
    // `reflow root', then use the first frame's size as the available
    // space.
    nsSize size;
    if (first == root)
      size = aMaxSize;
    else
      first->GetSize(size);

    nsHTMLReflowState reflowState(aPresContext, first, path,
                                  &aRendContext, size);

    nsReflowStatus status;
    first->Reflow(aPresContext, aDesiredSize, reflowState, status);

    // If an incremental reflow is initiated at a frame other than the
    // root frame, then its desired size had better not change!
    NS_ASSERTION(first == root ||
                 (aDesiredSize.width == size.width && aDesiredSize.height == size.height),
                 "non-root frame's desired size changed during an incremental reflow");

    first->SizeTo(aPresContext, aDesiredSize.width, aDesiredSize.height);

    nsIView* view;
    first->GetView(aPresContext, &view);
    if (view)
      nsContainerFrame::SyncFrameViewAfterReflow(aPresContext, first, view, nsnull);

    first->DidReflow(aPresContext, nsnull, NS_FRAME_REFLOW_FINISHED);
  }
}

PRBool
IncrementalReflow::AddCommand(nsIPresContext      *aPresContext,
                              nsHTMLReflowCommand *aCommand)
{
  nsIFrame *frame;
  aCommand->GetTarget(frame);
  NS_ASSERTION(frame != nsnull, "reflow command with no target");

  // Construct the reflow path by walking up the through the frames'
  // parent chain until we reach either a `reflow root' or the root
  // frame in the frame hierarchy.
  nsAutoVoidArray path;
  nsFrameState state;
  do {
    path.AppendElement(frame);
    frame->GetFrameState(&state);
  } while (!(state & NS_FRAME_REFLOW_ROOT) &&
           (frame->GetParent(&frame), frame != nsnull));

  // Pop off the root, add it to the set if it's not there already.
  PRInt32 lastIndex = path.Count() - 1;
  nsIFrame *rootFrame = NS_STATIC_CAST(nsIFrame *, path[lastIndex]);
  path.RemoveElementAt(lastIndex);

  nsReflowPath *root = nsnull;

  PRInt32 i;
  for (i = mRoots.Count() - 1; i >= 0; --i) {
    nsReflowPath *path = NS_STATIC_CAST(nsReflowPath *, mRoots[i]);
    if (path->mFrame == rootFrame) {
      root = path;
      break;
    }
  }

  if (! root) {
    root = new nsReflowPath(rootFrame);
    if (! root)
      return NS_ERROR_OUT_OF_MEMORY;

    root->mReflowCommand = nsnull;
    mRoots.AppendElement(root);
  }

  // Now walk the path from the root to the leaf, adding to the reflow
  // tree as necessary.
  nsReflowPath *target = root;
  for (i = path.Count() - 1; i >= 0; --i) {
    nsIFrame *frame = NS_STATIC_CAST(nsIFrame *, path[i]);
    target = target->EnsureSubtreeFor(frame);

    // Out of memory. Ugh.
    if (! target)
      return PR_FALSE;
  }

  // Place the reflow command in the leaf, if one isn't there already.
  if (target->mReflowCommand) {
    // XXXwaterson it's probably possible to have some notion of
    // `promotion' here that would avoid any re-queuing; for example,
    // promote a dirty reflow to a style changed. For now, let's punt
    // and not worry about it.
#ifdef NS_DEBUG
    if (gVerifyReflowFlags & VERIFY_REFLOW_NOISY_RC)
      printf("requeuing command %p because %p was already scheduled "
             "for the same frame",
             (void*)aCommand, (void*)target->mReflowCommand);
#endif

    return PR_FALSE;
  }

  target->mReflowCommand = aCommand;
  return PR_TRUE;
}


#ifdef NS_DEBUG
void
IncrementalReflow::Dump(nsIPresContext *aPresContext) const
{
  for (PRInt32 i = mRoots.Count() - 1; i >= 0; --i)
    NS_STATIC_CAST(nsReflowPath *, mRoots[i])->Dump(aPresContext, stdout, 0);
}
#endif

// ----------------------------------------------------------------------------

class PresShell : public nsIPresShell, public nsIViewObserver,
                  private nsIDocumentObserver, public nsIFocusTracker,
                  public nsISelectionController,
                  public nsSupportsWeakReference
{
public:
  PresShell();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIPresShell
  NS_IMETHOD Init(nsIDocument* aDocument,
                  nsIPresContext* aPresContext,
                  nsIViewManager* aViewManager,
                  nsIStyleSet* aStyleSet,
                  nsCompatibility aCompatMode);
  NS_IMETHOD Destroy();

  NS_IMETHOD AllocateFrame(size_t aSize, void** aResult);
  NS_IMETHOD FreeFrame(size_t aSize, void* aFreeChunk);

  // Dynamic stack memory allocation
  NS_IMETHOD PushStackMemory();
  NS_IMETHOD PopStackMemory();
  NS_IMETHOD AllocateStackMemory(size_t aSize, void** aResult);

  NS_IMETHOD GetDocument(nsIDocument** aResult);
  NS_IMETHOD GetPresContext(nsIPresContext** aResult);
  NS_IMETHOD GetViewManager(nsIViewManager** aResult);
  NS_IMETHOD GetStyleSet(nsIStyleSet** aResult);
  NS_IMETHOD GetActiveAlternateStyleSheet(nsString& aSheetTitle);
  NS_IMETHOD SelectAlternateStyleSheet(const nsString& aSheetTitle);
  NS_IMETHOD ListAlternateStyleSheets(nsStringArray& aTitleList);
  NS_IMETHOD ReconstructStyleData(PRBool aRebuildRuleTree);
  NS_IMETHOD SetPreferenceStyleRules(PRBool aForceReflow);
  NS_IMETHOD EnablePrefStyleRules(PRBool aEnable, PRUint8 aPrefType=0xFF);
  NS_IMETHOD ArePrefStyleRulesEnabled(PRBool& aEnabled);

  NS_IMETHOD GetSelection(SelectionType aType, nsISelection** aSelection);

  NS_IMETHOD SetDisplaySelection(PRInt16 aToggle);
  NS_IMETHOD GetDisplaySelection(PRInt16 *aToggle);
  NS_IMETHOD ScrollSelectionIntoView(SelectionType aType, SelectionRegion aRegion, PRBool aIsSynchronous);
  NS_IMETHOD RepaintSelection(SelectionType aType);
  NS_IMETHOD GetFrameSelection(nsIFrameSelection** aSelection);  

  NS_IMETHOD BeginObservingDocument();
  NS_IMETHOD EndObservingDocument();
  NS_IMETHOD GetDidInitialReflow(PRBool *aDidInitialReflow);
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
  NS_IMETHOD AppendReflowCommand(nsHTMLReflowCommand* aReflowCommand);
  NS_IMETHOD CancelReflowCommand(nsIFrame*                     aTargetFrame, 
                                 nsReflowType* aCmdType);  
  NS_IMETHOD CancelReflowCommandInternal(nsIFrame*     aTargetFrame, 
                                         nsReflowType* aCmdType,
                                         PRBool        aProcessDummyLayoutRequest = PR_TRUE);  
  NS_IMETHOD CancelAllReflowCommands();
  NS_IMETHOD IsSafeToFlush(PRBool& aIsSafeToFlush);
  NS_IMETHOD FlushPendingNotifications(PRBool aUpdateViews);

  /**
   * Recreates the frames for a node
   */
  NS_IMETHOD RecreateFramesFor(nsIContent* aContent);

  /**
   * Post a request to handle a DOM event after Reflow has finished.
   */
  NS_IMETHOD PostDOMEvent(nsIContent* aContent, nsEvent* aEvent);

  /**
   * Post a request to set and attribute after reflow has finished.
   */
  NS_IMETHOD PostAttributeChange(nsIContent* aContent,
                                 PRInt32 aNameSpaceID, 
                                 nsIAtom* aName,
                                 const nsString& aValue,
                                 PRBool aNotify,
                                 nsAttributeChangeType aType);

  /**
   * Post a callback that should be handled after reflow has finished.
   */
  NS_IMETHOD PostReflowCallback(nsIReflowCallback* aCallback);
  NS_IMETHOD CancelReflowCallback(nsIReflowCallback* aCallback);

  /**
   * Reflow batching
   */   
  NS_IMETHOD BeginReflowBatching();
  NS_IMETHOD EndReflowBatching(PRBool aFlushPendingReflows);  
  NS_IMETHOD GetReflowBatchingStatus(PRBool* aBatch);
  
  NS_IMETHOD ClearFrameRefs(nsIFrame* aFrame);
  NS_IMETHOD CreateRenderingContext(nsIFrame *aFrame,
                                    nsIRenderingContext** aContext);
  NS_IMETHOD CantRenderReplacedElement(nsIPresContext* aPresContext,
                                       nsIFrame*       aFrame);
  NS_IMETHOD GoToAnchor(const nsAString& aAnchorName);

  NS_IMETHOD ScrollFrameIntoView(nsIFrame *aFrame,
                                 PRIntn   aVPercent, 
                                 PRIntn   aHPercent) const;

  NS_IMETHOD SetIgnoreFrameDestruction(PRBool aIgnore);
  NS_IMETHOD NotifyDestroyingFrame(nsIFrame* aFrame);
  
  NS_IMETHOD GetFrameManager(nsIFrameManager** aFrameManager) const;

  NS_IMETHOD DoCopy();
  NS_IMETHOD GetLinkLocation(nsIDOMNode* aNode, nsAString& aLocationString);
  NS_IMETHOD GetImageLocation(nsIDOMNode* aNode, nsAString& aLocationString);
  NS_IMETHOD DoCopyImageContents(nsIDOMNode* aNode);

  NS_IMETHOD CaptureHistoryState(nsILayoutHistoryState** aLayoutHistoryState, PRBool aLeavingPage);
  NS_IMETHOD GetHistoryState(nsILayoutHistoryState** aLayoutHistoryState);
  NS_IMETHOD SetHistoryState(nsILayoutHistoryState* aLayoutHistoryState);

  NS_IMETHOD GetGeneratedContentIterator(nsIContent*          aContent,
                                         GeneratedContentType aType,
                                         nsIContentIterator** aIterator) const;
 
  NS_IMETHOD SetAnonymousContentFor(nsIContent* aContent, nsISupportsArray* aAnonymousElements);
  NS_IMETHOD GetAnonymousContentFor(nsIContent* aContent, nsISupportsArray** aAnonymousElements);
  NS_IMETHOD ReleaseAnonymousContent();

  NS_IMETHOD IsPaintingSuppressed(PRBool* aResult);
  NS_IMETHOD UnsuppressPainting();
  
  NS_IMETHOD DisableThemeSupport();
  virtual PRBool IsThemeSupportEnabled();

  NS_IMETHOD HandleEventWithTarget(nsEvent* aEvent, nsIFrame* aFrame, nsIContent* aContent, PRUint32 aFlags, nsEventStatus* aStatus);
  NS_IMETHOD GetEventTargetFrame(nsIFrame** aFrame);

  NS_IMETHOD IsReflowLocked(PRBool* aIsLocked);  
#ifdef IBMBIDI
  NS_IMETHOD SetCaretBidiLevel(PRUint8 aLevel);
  NS_IMETHOD GetCaretBidiLevel(PRUint8 *aOutLevel);
  NS_IMETHOD UndefineCaretBidiLevel();
  NS_IMETHOD BidiStyleChangeReflow();
#endif

  //nsIViewObserver interface

  NS_IMETHOD Paint(nsIView *aView,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect);
  NS_IMETHOD HandleEvent(nsIView*        aView,
                         nsGUIEvent*     aEvent,
                         nsEventStatus*  aEventStatus,
  											 PRBool          aForceHandle,
                         PRBool&         aHandled);
  NS_IMETHOD HandleDOMEventWithTarget(nsIContent* aTargetContent,
                            nsEvent* aEvent,
                            nsEventStatus* aStatus);
  NS_IMETHOD Scrolled(nsIView *aView);
  NS_IMETHOD ResizeReflow(nsIView *aView, nscoord aWidth, nscoord aHeight);

  //nsIFocusTracker interface
  NS_IMETHOD ScrollFrameIntoView(nsIFrame *aFrame);
  // caret handling
  NS_IMETHOD GetCaret(nsICaret **aOutCaret);
  NS_IMETHOD SetCaretEnabled(PRBool aInEnable);
  NS_IMETHOD SetCaretWidth(PRInt16 twips);
  NS_IMETHOD SetCaretReadOnly(PRBool aReadOnly);
  NS_IMETHOD GetCaretEnabled(PRBool *aOutEnabled);

  NS_IMETHOD SetSelectionFlags(PRInt16 aInEnable);
  NS_IMETHOD GetSelectionFlags(PRInt16 *aOutEnable);

  // nsISelectionController

  NS_IMETHOD CharacterMove(PRBool aForward, PRBool aExtend);
  NS_IMETHOD WordMove(PRBool aForward, PRBool aExtend);
  NS_IMETHOD LineMove(PRBool aForward, PRBool aExtend);
  NS_IMETHOD IntraLineMove(PRBool aForward, PRBool aExtend);
  NS_IMETHOD PageMove(PRBool aForward, PRBool aExtend);
  NS_IMETHOD ScrollPage(PRBool aForward);
  NS_IMETHOD ScrollLine(PRBool aForward);
  NS_IMETHOD ScrollHorizontal(PRBool aLeft);
  NS_IMETHOD CompleteScroll(PRBool aForward);
  NS_IMETHOD CompleteMove(PRBool aForward, PRBool aExtend);
  NS_IMETHOD SelectAll();
  NS_IMETHOD CheckVisibility(nsIDOMNode *node, PRInt16 startOffset, PRInt16 EndOffset, PRBool *_retval);

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
  NS_IMETHOD ContentStatesChanged(nsIDocument* aDocument,
                                  nsIContent* aContent1,
                                  nsIContent* aContent2,
                                  PRInt32 aStateMask);
  NS_IMETHOD AttributeChanged(nsIDocument *aDocument,
                              nsIContent*  aContent,
                              PRInt32      aNameSpaceID,
                              nsIAtom*     aAttribute,
                              PRInt32      aModType, 
                              nsChangeHint aHint);
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
                              nsChangeHint aHint);
  NS_IMETHOD StyleRuleAdded(nsIDocument *aDocument,
                            nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule);
  NS_IMETHOD StyleRuleRemoved(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule);
  NS_IMETHOD DocumentWillBeDestroyed(nsIDocument *aDocument);

#ifdef MOZ_REFLOW_PERF
  NS_IMETHOD DumpReflows();
  NS_IMETHOD CountReflows(const char * aName, PRUint32 aType, nsIFrame * aFrame);
  NS_IMETHOD PaintCount(const char * aName, nsIRenderingContext* aRenderingContext, nsIPresContext* aPresContext, nsIFrame * aFrame, PRUint32 aColor);

  NS_IMETHOD SetPaintFrameCount(PRBool aOn);
  
#endif

#ifdef PR_LOGGING
  static PRLogModuleInfo* gLog;
#endif

protected:
  virtual ~PresShell();

  void HandlePostedDOMEvents();
  void HandlePostedAttributeChanges();
  void HandlePostedReflowCallbacks();

  void UnsuppressAndInvalidate();
  
  /** notify all external reflow observers that reflow of type "aData" is about
    * to begin.
    */
  nsresult NotifyReflowObservers(const char *aData);

  nsresult ReflowCommandAdded(nsHTMLReflowCommand* aRC);
  nsresult ReflowCommandRemoved(nsHTMLReflowCommand* aRC);

  // This method should be called after a reflow commands have been
  // removed from the queue, but after the state in the presshell is
  // such that it's safe to flush (i.e. mIsReflowing == PR_FALSE)
  // If we are not reflowing and ther are no load-crated reflow commands, then
  // the dummyLayoutRequest is removed
  void DoneRemovingReflowCommands();

  nsresult AddDummyLayoutRequest(void);
  nsresult RemoveDummyLayoutRequest(void);

  nsresult ReconstructFrames(void);
  nsresult CloneStyleSet(nsIStyleSet* aSet, nsIStyleSet** aResult);
  nsresult WillCauseReflow();
  nsresult DidCauseReflow();
  nsresult ProcessReflowCommands(PRBool aInterruptible);
  nsresult ClearReflowEventStatus();  
  void     PostReflowEvent();
  PRBool   AlreadyInQueue(nsHTMLReflowCommand* aReflowCommand,
                          nsVoidArray&         aQueue);
  
  friend struct ReflowEvent;

    // utility to determine if we're in the middle of a drag
  PRBool IsDragInProgress ( ) const ;

  PRBool	mCaretEnabled;
#ifdef IBMBIDI
  PRUint8   mBidiLevel; // The Bidi level of the cursor
  nsCOMPtr<nsIBidiKeyboard> mBidiKeyboard;
#endif // IBMBIDI
#ifdef NS_DEBUG
  PRBool VerifyIncrementalReflow();
  PRBool mInVerifyReflow;
#endif

    /**
    * methods that manage rules that are used to implement the associated preferences
    *  - initially created for bugs 31816, 20760, 22963
    */
  nsresult ClearPreferenceStyleRules(void);
  nsresult CreatePreferenceStyleSheet(void);
  nsresult SetPrefColorRules(void);
  nsresult SetPrefLinkRules(void);
  nsresult SetPrefFocusRules(void);

  nsresult SelectRange(nsIDOMRange *aRange);

  // IMPORTANT: The ownership implicit in the following member variables has been 
  // explicitly checked and set using nsCOMPtr for owning pointers and raw COM interface 
  // pointers for weak (ie, non owning) references. If you add any members to this
  // class, please make the ownership explicit (pinkerton, scc).

  // these are the same Document and PresContext owned by the DocViewer.
  // we must share ownership.
  nsCOMPtr<nsIDocument>     mDocument;
  nsCOMPtr<nsIPresContext>  mPresContext;
  nsCOMPtr<nsIStyleSet>     mStyleSet;
  nsICSSStyleSheet*         mPrefStyleSheet; // mStyleSet owns it but we maintaina ref, may be null
  nsIViewManager*           mViewManager;   // [WEAK] docViewer owns it so I don't have to
  nsWeakPtr                 mHistoryState; // [WEAK] session history owns this
  PRUint32                  mUpdateCount;
  // normal reflow commands
  nsVoidArray               mReflowCommands; 

  PRPackedBool mEnablePrefStyleSheet;
  PRPackedBool mDocumentLoading;
  PRPackedBool mIsReflowing;
  PRPackedBool mIsDestroying;

  PRPackedBool mDidInitialReflow;
  PRPackedBool mIgnoreFrameDestruction;

  nsIFrame*   mCurrentEventFrame;
  nsIContent* mCurrentEventContent;
  nsVoidArray mCurrentEventFrameStack;
  nsVoidArray mCurrentEventContentStack;
  nsSupportsHashtable* mAnonymousContentTable;

#ifdef NS_DEBUG
  nsRect mCurrentTargetRect;
  nsIView* mCurrentTargetView;
#endif
  
  nsCOMPtr<nsIFrameSelection>   mSelection;
  nsCOMPtr<nsICaret>            mCaret;
  PRInt16                       mSelectionFlags;
  PRPackedBool                  mScrollingEnabled; //used to disable programmable scrolling from outside
  PRPackedBool                  mBatchReflows;  // When set to true, the pres shell batches reflow commands.  
  nsIFrameManager*              mFrameManager;  // we hold a reference
  PresShellViewEventListener    *mViewEventListener;
  nsCOMPtr<nsIEventQueueService> mEventQueueService;
  nsCOMPtr<nsIEventQueue>       mReflowEventQueue;
  FrameArena                    mFrameArena;
  StackArena*                   mStackArena;
  nsCOMPtr<nsIObserverService>  mObserverService; // Observer service for reflow events
  nsCOMPtr<nsIDragService>      mDragService;
  PRInt32                       mRCCreatedDuringLoad; // Counter to keep track of reflow commands created during doc
  nsCOMPtr<nsIRequest>          mDummyLayoutRequest;

  // used for list of posted events and attribute changes. To be done
  // after reflow.
  nsDOMEventRequest* mFirstDOMEventRequest;
  nsDOMEventRequest* mLastDOMEventRequest;
  nsAttributeChangeRequest* mFirstAttributeRequest;
  nsAttributeChangeRequest* mLastAttributeRequest;
  nsCallbackEventRequest* mFirstCallbackEventRequest;
  nsCallbackEventRequest* mLastCallbackEventRequest;

  PRPackedBool      mIsThemeSupportDisabled;  // Whether or not form controls should use nsITheme in this shell.

  PRPackedBool      mIsDocumentGone;      // We've been disconnected from the document.
  PRPackedBool      mPaintingSuppressed;  // For all documents we initially lock down painting.
                                          // We will refuse to paint the document until either
                                          // (a) our timer fires or (b) all frames are constructed.
  PRPackedBool      mShouldUnsuppressPainting;  // Indicates that it is safe to unlock painting once all pending
                                                // reflows have been processed.
  nsCOMPtr<nsITimer> mPaintSuppressionTimer; // This timer controls painting suppression.  Until it fires
                                             // or all frames are constructed, we won't paint anything but
                                             // our <body> background and scrollbars.
#define PAINTLOCK_EVENT_DELAY 1200 // 1200 ms.  This is actually pref-controlled, but we use this
                                   // value if we fail to get the pref for any reason.

  static void sPaintSuppressionCallback(nsITimer* aTimer, void* aPresShell); // A callback for the timer.

  MOZ_TIMER_DECLARE(mReflowWatch)  // Used for measuring time spent in reflow
  MOZ_TIMER_DECLARE(mFrameCreationWatch)  // Used for measuring time spent in frame creation 

#ifdef MOZ_REFLOW_PERF
  ReflowCountMgr * mReflowCountMgr;
#endif

private:

  void FreeDynamicStack();

  //helper funcs for disabing autoscrolling
  void DisableScrolling(){mScrollingEnabled = PR_FALSE;}
  void EnableScrolling(){mScrollingEnabled = PR_TRUE;}
  PRBool IsScrollingEnabled(){return mScrollingEnabled;}

  //helper funcs for event handling
  nsIFrame* GetCurrentEventFrame();
  void PushCurrentEventInfo(nsIFrame* aFrame, nsIContent* aContent);
  void PopCurrentEventInfo();
  nsresult HandleEventInternal(nsEvent* aEvent, nsIView* aView, PRUint32 aFlags, nsEventStatus *aStatus);

  //help funcs for resize events
  void CreateResizeEventTimer();
  void KillResizeEventTimer();
  void FireResizeEvent();
  static void sResizeEventCallback(nsITimer* aTimer, void* aPresShell) ;
  nsCOMPtr<nsITimer> mResizeEventTimer;
};

#ifdef PR_LOGGING
PRLogModuleInfo* PresShell::gLog;
#endif

#ifdef NS_DEBUG
static void
VerifyStyleTree(nsIPresContext* aPresContext, nsIFrameManager* aFrameManager)
{
  if (aFrameManager && nsIFrameDebug::GetVerifyStyleTreeEnable()) {
    nsIFrame* rootFrame;

    aFrameManager->GetRootFrame(&rootFrame);
    aFrameManager->DebugVerifyStyleTree(aPresContext, rootFrame);
  }
}
#define VERIFY_STYLE_TREE VerifyStyleTree(mPresContext, mFrameManager)
#else
#define VERIFY_STYLE_TREE
#endif

static PRBool gVerifyReflowEnabled;

PRBool
nsIPresShell::GetVerifyReflowEnable()
{
#ifdef NS_DEBUG
  static PRBool firstTime = PR_TRUE;
  if (firstTime) {
    firstTime = PR_FALSE;
    char* flags = PR_GetEnv("GECKO_VERIFY_REFLOW_FLAGS");
    if (flags) {
      PRBool error = PR_FALSE;

      for (;;) {
        char* comma = PL_strchr(flags, ',');
        if (comma)
          *comma = '\0';

        PRBool found = PR_FALSE;
        VerifyReflowFlags* flag = gFlags;
        VerifyReflowFlags* limit = gFlags + NUM_VERIFY_REFLOW_FLAGS;
        while (flag < limit) {
          if (PL_strcasecmp(flag->name, flags) == 0) {
            gVerifyReflowFlags |= flag->bit;
            found = PR_TRUE;
            break;
          }
          ++flag;
        }

        if (! found)
          error = PR_TRUE;

        if (! comma)
          break;

        *comma = ',';
        flags = comma + 1;
      }

      if (error)
        ShowVerifyReflowFlags();
    }

    if (VERIFY_REFLOW_ON & gVerifyReflowFlags) {
      gVerifyReflowEnabled = PR_TRUE;
    }
    printf("Note: verifyreflow is %sabled",
           gVerifyReflowEnabled ? "en" : "dis");
    if (VERIFY_REFLOW_NOISY & gVerifyReflowFlags) {
      printf(" (noisy)");
    }
    if (VERIFY_REFLOW_ALL & gVerifyReflowFlags) {
      printf(" (all)");
    }
    if (VERIFY_REFLOW_DUMP_COMMANDS & gVerifyReflowFlags) {
      printf(" (show reflow commands)");
    }
    if (VERIFY_REFLOW_NOISY_RC & gVerifyReflowFlags) {
      printf(" (noisy reflow commands)");
      if (VERIFY_REFLOW_REALLY_NOISY_RC & gVerifyReflowFlags) {
        printf(" (REALLY noisy reflow commands)");
      }
    }
    printf("\n");
  }
#endif
  return gVerifyReflowEnabled;
}

void
nsIPresShell::SetVerifyReflowEnable(PRBool aEnabled)
{
  gVerifyReflowEnabled = aEnabled;
}

PRInt32
nsIPresShell::GetVerifyReflowFlags()
{
#ifdef NS_DEBUG
  return gVerifyReflowFlags;
#else
  return 0;
#endif
}

//----------------------------------------------------------------------

nsresult
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
  return it->QueryInterface(NS_GET_IID(nsIPresShell),
                            (void **) aInstancePtrResult);
}

PresShell::PresShell():
#ifdef IBMBIDI
  mBidiLevel(BIDI_LEVEL_UNDEFINED),
#endif
  mEnablePrefStyleSheet(PR_TRUE),
  mScrollingEnabled(PR_TRUE)
{
  NS_INIT_ISUPPORTS();
#ifdef MOZ_REFLOW_PERF
  mReflowCountMgr = new ReflowCountMgr();
  mReflowCountMgr->SetPresContext(mPresContext);
  mReflowCountMgr->SetPresShell(this);
#endif
#ifdef IBMBIDI
  mBidiLevel = BIDI_LEVEL_UNDEFINED;
#endif
#ifdef PR_LOGGING
  if (! gLog)
    gLog = PR_NewLogModule("PresShell");
#endif
  mSelectionFlags = nsISelectionDisplay::DISPLAY_TEXT | nsISelectionDisplay::DISPLAY_IMAGES;
  mIsThemeSupportDisabled = PR_FALSE;
}

NS_IMPL_ADDREF(PresShell)
NS_IMPL_RELEASE(PresShell)

nsresult
PresShell::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (!aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aIID.Equals(NS_GET_IID(nsIPresShell))) {
    nsIPresShell* tmp = this;
    *aInstancePtr = (void*) tmp;
  } else if (aIID.Equals(NS_GET_IID(nsIDocumentObserver))) {
    nsIDocumentObserver* tmp = this;
    *aInstancePtr = (void*) tmp;
  } else if (aIID.Equals(NS_GET_IID(nsIViewObserver))) {
    nsIViewObserver* tmp = this;
    *aInstancePtr = (void*) tmp;
  } else if (aIID.Equals(NS_GET_IID(nsIFocusTracker))) {
    nsIFocusTracker* tmp = this;
    *aInstancePtr = (void*) tmp;
  } else if (aIID.Equals(NS_GET_IID(nsISelectionController))) {
    nsISelectionController* tmp = this;
    *aInstancePtr = (void*) tmp;
  } else if (aIID.Equals(NS_GET_IID(nsISelectionDisplay))) {
    nsISelectionController* tmp = this;
    *aInstancePtr = (void*) tmp;
  } else if (aIID.Equals(NS_GET_IID(nsISupportsWeakReference))) {
    nsISupportsWeakReference* tmp = this;
    *aInstancePtr = (void*) tmp;
  } else if (aIID.Equals(NS_GET_IID(nsISupports))) {
    nsIPresShell* tmp = this;
    nsISupports* tmp2 = tmp;
    *aInstancePtr = (void*) tmp2;
  } else {
    *aInstancePtr = nsnull;

    return NS_NOINTERFACE;
  }

  NS_ADDREF_THIS();

  return NS_OK;
}

PresShell::~PresShell()
{
  if (mStyleSet) {
    NS_NOTREACHED("Someone did not call nsIPresShell::destroy");
    Destroy();
  }

  NS_ASSERTION(mCurrentEventContentStack.Count() == 0,
               "Huh, event content left on the stack in pres shell dtor!");

  NS_IF_RELEASE(mCurrentEventContent);

  // if we allocated any stack memory free it.
  FreeDynamicStack();
}

/**
 * Initialize the presentation shell. Create view manager and style
 * manager.
 */
NS_IMETHODIMP
PresShell::Init(nsIDocument* aDocument,
                nsIPresContext* aPresContext,
                nsIViewManager* aViewManager,
                nsIStyleSet* aStyleSet,
                nsCompatibility aCompatMode)
{
  NS_PRECONDITION(nsnull != aDocument, "null ptr");
  NS_PRECONDITION(nsnull != aPresContext, "null ptr");
  NS_PRECONDITION(nsnull != aViewManager, "null ptr");

  if ((nsnull == aDocument) || (nsnull == aPresContext) ||
      (nsnull == aViewManager)) {
    return NS_ERROR_NULL_POINTER;
  }
  if (mDocument) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  mDocument = dont_QueryInterface(aDocument);
  mViewManager = aViewManager;

  // The document viewer owns both view manager and pres shell.
  mViewManager->SetViewObserver(this);

  // Bind the context to the presentation shell.
  mPresContext = aPresContext;
  aPresContext->SetShell(this);

  mStyleSet = aStyleSet;

  // Set the compatibility mode after attaching the pres context and
  // style set, but before creating any frames.
  mPresContext->SetCompatibilityMode(aCompatMode);

  // setup the preference style rules (no forced reflow), and do it
  // before creating any frames.
  SetPreferenceStyleRules(PR_FALSE);

  mHistoryState = nsnull;

  nsresult result = nsComponentManager::CreateInstance(kFrameSelectionCID, nsnull,
                                                 NS_GET_IID(nsIFrameSelection),
                                                 getter_AddRefs(mSelection));
  if (NS_FAILED(result))
    return result;

  // Create and initialize the frame manager
  result = NS_NewFrameManager(&mFrameManager);
  if (NS_FAILED(result)) {
    return result;
  }
  result = mFrameManager->Init(this, mStyleSet);
  if (NS_FAILED(result)) {
    return result;
  }

  result = mSelection->Init((nsIFocusTracker *) this, nsnull);
  if (NS_FAILED(result))
    return result;
  // Important: this has to happen after the selection has been set up
#ifdef SHOW_CARET
  // make the caret
  nsresult  err = NS_NewCaret(getter_AddRefs(mCaret));
  if (NS_SUCCEEDED(err))
  {
    mCaret->Init(this);
  }

  //SetCaretEnabled(PR_TRUE);			// make it show in browser windows
#endif  
//set up selection to be displayed in document
  nsCOMPtr<nsISupports> container;
  result = aPresContext->GetContainer(getter_AddRefs(container));
  if (NS_SUCCEEDED(result) && container) {
    nsCOMPtr<nsIDocShellTreeItem> docShell(do_QueryInterface(container, &result));
    if (NS_SUCCEEDED(result) && docShell){
      PRInt32 docShellType;
      result = docShell->GetItemType(&docShellType);
      if (NS_SUCCEEDED(result)){
        if (nsIDocShellTreeItem::typeContent == docShellType){
          SetDisplaySelection(nsISelectionController::SELECTION_DISABLED);
        }
      }      
    }
  }
  
  mEventQueueService = do_GetService(kEventQueueServiceCID, &result);

  if (!mEventQueueService) {
    NS_WARNING("couldn't get event queue service");
    return NS_ERROR_FAILURE;
  }

  if (gMaxRCProcessingTime == -1) {
    // First, set the defaults
    gMaxRCProcessingTime = NS_MAX_REFLOW_TIME;
    gAsyncReflowDuringDocLoad = PR_TRUE;

    // Get the prefs service
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID, &result));
    if (NS_SUCCEEDED(result)) {
      prefs->GetIntPref("layout.reflow.timeslice", &gMaxRCProcessingTime);
      prefs->GetBoolPref("layout.reflow.async.duringDocLoad", &gAsyncReflowDuringDocLoad);
    }
  }

  // cache the observation service
  mObserverService = do_GetService("@mozilla.org/observer-service;1",
                                   &result);
  if (NS_FAILED(result)) {
    return result;
  }

  // cache the drag service so we can check it during reflows
  mDragService = do_GetService("@mozilla.org/widget/dragservice;1");

#ifdef IBMBIDI
  mBidiKeyboard = do_GetService("@mozilla.org/widget/bidikeyboard;1");
#endif

#ifdef MOZ_REFLOW_PERF
    // Get the prefs service
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID, &result));
    if (NS_SUCCEEDED(result)) {
      if (mReflowCountMgr != nsnull) {
        PRBool paintFrameCounts       = PR_FALSE;
        PRBool dumpFrameCounts        = PR_FALSE;
        PRBool dumpFrameByFrameCounts = PR_FALSE;
        prefs->GetBoolPref("layout.reflow.showframecounts", &paintFrameCounts);
        prefs->GetBoolPref("layout.reflow.dumpframecounts", &dumpFrameCounts);
        prefs->GetBoolPref("layout.reflow.dumpframebyframecounts", &dumpFrameByFrameCounts);

        mReflowCountMgr->SetDumpFrameCounts(dumpFrameCounts);
        mReflowCountMgr->SetDumpFrameByFrameCounts(dumpFrameByFrameCounts);
        mReflowCountMgr->SetPaintFrameCounts(paintFrameCounts);
      }
    }
#endif

  return NS_OK;
}

NS_IMETHODIMP
PresShell::Destroy()
{
#ifdef MOZ_REFLOW_PERF
  DumpReflows();
  if (mReflowCountMgr) {
    delete mReflowCountMgr;
    mReflowCountMgr = nsnull;
  }
#endif

  // If our paint suppression timer is still active, kill it.
  if (mPaintSuppressionTimer) {
    mPaintSuppressionTimer->Cancel();
    mPaintSuppressionTimer = nsnull;
  }

  // release our pref style sheet, if we have one still
  ClearPreferenceStyleRules();

  // free our table of anonymous content
  ReleaseAnonymousContent();

  mIsDestroying = PR_TRUE;

  // Clobber weak leaks in case of re-entrancy during tear down
  mHistoryState = nsnull;

  // We can't release all the event content in
  // mCurrentEventContentStack here since there might be code on the
  // stack that will release the event content too. Double release
  // bad!

  // The frames will be torn down, so remove them from the current
  // event frame stack (since they'd be dangling references if we'd
  // leave them in) and null out the mCurrentEventFrame pointer as
  // well.

  mCurrentEventFrame = nsnull;

  PRInt32 i, count = mCurrentEventFrameStack.Count();
  for (i = 0; i < count; i++) {
    mCurrentEventFrameStack.ReplaceElementAt(nsnull, i);
  }

  if (mViewManager) {
    // Disable paints during tear down of the frame tree
    mViewManager->DisableRefresh();
    // Clear the view manager's weak pointer back to |this| in case it
    // was leaked.
    mViewManager->SetViewObserver(nsnull);
    mViewManager = nsnull;
  }

  // This shell must be removed from the document before the frame
  // hierarchy is torn down to avoid finding deleted frames through
  // this presshell while the frames are being torn down
  if (mDocument) {
    mDocument->DeleteShell(this);
  }

  // Destroy the frame manager. This will destroy the frame hierarchy
  if (mFrameManager) {
    mFrameManager->Destroy();
    NS_RELEASE(mFrameManager);
  }

  // Let the style set do its cleanup.
  mStyleSet->Shutdown();
  mStyleSet = nsnull;

  // We hold a reference to the pres context, and it holds a weak link back
  // to us. To avoid the pres context having a dangling reference, set its 
  // pres shell to NULL
  if (mPresContext) {
    mPresContext->SetShell(nsnull);
  }

  if (mViewEventListener) {
    mViewEventListener->SetPresShell((nsIPresShell*)nsnull);
    NS_RELEASE(mViewEventListener);
  }

  // Revoke pending reflow events
  mReflowEventQueue = nsnull;
  nsCOMPtr<nsIEventQueue> eventQueue;
  mEventQueueService->GetSpecialEventQueue(nsIEventQueueService::UI_THREAD_EVENT_QUEUE,
                                           getter_AddRefs(eventQueue));
  eventQueue->RevokeEvents(this);

  CancelAllReflowCommands();
  KillResizeEventTimer();

  return NS_OK;
}

                  // Dynamic stack memory allocation
NS_IMETHODIMP
PresShell::PushStackMemory()
{
  if (!mStackArena) {
    mStackArena = new StackArena();
    if (!mStackArena)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  return mStackArena->Push();
}

NS_IMETHODIMP
PresShell::PopStackMemory()
{
  NS_ENSURE_TRUE(mStackArena, NS_ERROR_UNEXPECTED);

  return mStackArena->Pop();
}

NS_IMETHODIMP
PresShell::AllocateStackMemory(size_t aSize, void** aResult)
{
  if (!mStackArena) {
    mStackArena = new StackArena();
    if (!mStackArena)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  return mStackArena->Allocate(aSize, aResult);
}

void
PresShell::FreeDynamicStack()
{
  if (mStackArena) {
    delete mStackArena;
    mStackArena = nsnull;
  }
}
 

NS_IMETHODIMP
PresShell::FreeFrame(size_t aSize, void* aPtr)
{
  mFrameArena.FreeFrame(aSize, aPtr);
  return NS_OK;
}

NS_IMETHODIMP
PresShell::AllocateFrame(size_t aSize, void** aResult)
{
  return mFrameArena.AllocateFrame(aSize, aResult);
}

NS_IMETHODIMP
PresShell::GetDocument(nsIDocument** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mDocument;
  NS_IF_ADDREF(*aResult);
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
  NS_IF_ADDREF(*aResult);
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
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
PresShell::GetActiveAlternateStyleSheet(nsString& aSheetTitle)
{ // first non-html sheet in style set that has title
  if (mStyleSet) {
    PRInt32 count = mStyleSet->GetNumberOfDocStyleSheets();
    PRInt32 index;
    NS_NAMED_LITERAL_STRING(textHtml, "text/html");
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
  if (mDocument && mStyleSet) {
    PRInt32 count = 0;
    mDocument->GetNumberOfStyleSheets(&count);
    PRInt32 index;
    NS_NAMED_LITERAL_STRING(textHtml,"text/html");
    for (index = 0; index < count; index++) {
      nsCOMPtr<nsIStyleSheet> sheet;
      mDocument->GetStyleSheetAt(index, getter_AddRefs(sheet));
      if (sheet) {
        nsAutoString type;
        sheet->GetType(type);
        if (PR_FALSE == type.Equals(textHtml)) {
          nsAutoString  title;
          sheet->GetTitle(title);
          if (0 < title.Length()) {
            if (title.Equals(aSheetTitle)) {
              mStyleSet->AddDocStyleSheet(sheet, mDocument);
            }
            else {
              mStyleSet->RemoveDocStyleSheet(sheet);
            }
          }
        }
      }
    }
    // We don't need to rebuild the
    // rule tree, since no rule nodes have been rendered invalid by the
    // addition of new rule content.  They can simply lurk in the tree
    // until the stylesheet is enabled once more.
    return ReconstructStyleData(PR_FALSE);
  }
  return NS_OK;
}

NS_IMETHODIMP
PresShell::ListAlternateStyleSheets(nsStringArray& aTitleList)
{
  if (mDocument) {
    PRInt32 count = 0;
    mDocument->GetNumberOfStyleSheets(&count);
    PRInt32 index;
    NS_NAMED_LITERAL_STRING(textHtml,"text/html");
    for (index = 0; index < count; index++) {
      nsCOMPtr<nsIStyleSheet> sheet;
      mDocument->GetStyleSheetAt(index, getter_AddRefs(sheet));
      if (sheet) {
        nsAutoString type;
        sheet->GetType(type);
        if (PR_FALSE == type.Equals(textHtml)) {
          nsAutoString  title;
          sheet->GetTitle(title);
          if (0 < title.Length()) {
            if (-1 == aTitleList.IndexOf(title)) {
              aTitleList.AppendString(title);
            }
          }
        }
      }
    }
  }
  return NS_OK;
}


NS_IMETHODIMP 
PresShell::EnablePrefStyleRules(PRBool aEnable, PRUint8 aPrefType/*=0xFF*/)
{
  nsresult result = NS_OK;

  // capture change in state
  PRBool bChanging = (mEnablePrefStyleSheet != aEnable) ? PR_TRUE : PR_FALSE;
  // set to desired state
  mEnablePrefStyleSheet = aEnable;

#ifdef DEBUG_attinasi
  printf("PrefStyleSheet %s %s\n",
    mEnablePrefStyleSheet ? "ENABLED" : "DISABLED",
    bChanging ? "(state toggled)" : "(state unchanged)");
#endif

  // deal with changing state
  if(bChanging){
    switch (mEnablePrefStyleSheet){
    case PR_TRUE:
      // was off, now on, so create the rules
      result = SetPreferenceStyleRules(PR_TRUE);
      break;
    default :
      // was on, now off, so clear the rules
      result = ClearPreferenceStyleRules();
      break;
    }
  }
  return result;
}

NS_IMETHODIMP
PresShell::ArePrefStyleRulesEnabled(PRBool& aEnabled)
{
  aEnabled = mEnablePrefStyleSheet;
  return NS_OK;
}

NS_IMETHODIMP
PresShell::SetPreferenceStyleRules(PRBool aForceReflow)
{
  if (!mDocument) {
    return NS_ERROR_NULL_POINTER;
  }

  nsCOMPtr<nsIScriptGlobalObject> globalObj;
	mDocument->GetScriptGlobalObject(getter_AddRefs(globalObj));

  // If the document doesn't have a global object there's no need to
  // notify its presshell about changes to preferences since the
  // document is in a state where it doesn't matter any more (see
  // DocumentViewerImpl::Close()).

  if (!globalObj) {
    return NS_ERROR_NULL_POINTER;
  } 

  NS_PRECONDITION(mPresContext, "presContext cannot be null");
  if (mPresContext) {
    nsresult result = NS_OK;

    // zeroth, make sure this feature is enabled
    // XXX: may get more granularity later 
    //      (i.e. each pref may be controlled independently)
    if (!mEnablePrefStyleSheet) {
#ifdef DEBUG_attinasi
      printf("PrefStyleSheet disabled\n");
#endif
      return PR_TRUE;
    }

    // first, make sure this is not a chrome shell 
    nsCOMPtr<nsISupports> container;
    result = mPresContext->GetContainer(getter_AddRefs(container));
    if (NS_SUCCEEDED(result) && container) {
      nsCOMPtr<nsIDocShellTreeItem> docShell(do_QueryInterface(container, &result));
      if (NS_SUCCEEDED(result) && docShell){
        PRInt32 docShellType;
        result = docShell->GetItemType(&docShellType);
        if (NS_SUCCEEDED(result)){
          if (nsIDocShellTreeItem::typeChrome == docShellType){
            return NS_OK;
          }
        }      
      }
    }
    if (NS_SUCCEEDED(result)) {
    
#ifdef DEBUG_attinasi
      printf("Setting Preference Style Rules:\n");
#endif
      // if here, we need to create rules for the prefs
      // - this includes the background-color, the text-color,
      //   the link color, the visited link color and the link-underlining
    
      // first clear any exising rules
      result = ClearPreferenceStyleRules();
      
      // now do the color rules
      if (NS_SUCCEEDED(result)) {
        result = SetPrefColorRules();
      }

      // now the link rules (must come after the color rules, or links will not be correct color!)
      // XXX - when there is both an override and agent pref stylesheet this won't matter,
      //       as the color rules will be overrides and the links rules will be agent
      if (NS_SUCCEEDED(result)) {
        result = SetPrefLinkRules();
      }
      if (NS_SUCCEEDED(result)) {
        result = SetPrefFocusRules();
      }
 

      // update the styleset now that we are done inserting our rules
      if (NS_SUCCEEDED(result)) {
        if (mStyleSet) {
          mStyleSet->NotifyStyleSheetStateChanged(PR_FALSE);
        }
      }
    }
#ifdef DEBUG_attinasi
    printf( "Preference Style Rules set: error=%ld\n", (long)result);
#endif    

    if (aForceReflow){
#ifdef NS_DEBUG
      printf( "*** Forcing reframe! ***\n");
#endif
      // this is harsh, but without it the new colors don't appear on the current page
      // Fortunately, it only happens when the prefs change, a rare event.
      // XXX - determine why the normal PresContext::RemapStyleAndReflow doesn't cut it
      // XXXldb FIX THIS!
      ReconstructFrames();
    }

    return result;

  }

  return NS_ERROR_NULL_POINTER;
}

nsresult PresShell::ClearPreferenceStyleRules(void)
{
  nsresult result = NS_OK;
  if (mPrefStyleSheet) {
    NS_ASSERTION(mStyleSet, "null styleset entirely unexpected!");
    if (mStyleSet) {
      // remove the sheet from the styleset: 
      // - note that we have to check for success by comparing the count before and after...
#ifdef NS_DEBUG
      PRInt32 numBefore = mStyleSet->GetNumberOfUserStyleSheets();
      NS_ASSERTION(numBefore > 0, "no user stylesheets in styleset, but we have one!");
#endif
      mStyleSet->RemoveUserStyleSheet(mPrefStyleSheet);

#ifdef DEBUG_attinasi
      NS_ASSERTION((numBefore - 1) == mStyleSet->GetNumberOfUserStyleSheets(),
                   "Pref stylesheet was not removed");
      printf("PrefStyleSheet removed\n");
#endif
      // clear the sheet pointer: it is strictly historical now
      NS_IF_RELEASE(mPrefStyleSheet);
    }
  }
  return result;
}

nsresult PresShell::CreatePreferenceStyleSheet(void)
{
  NS_ASSERTION(mPrefStyleSheet==nsnull, "prefStyleSheet already exists");
  nsresult result = NS_OK;

  result = nsComponentManager::CreateInstance(kCSSStyleSheetCID,nsnull,NS_GET_IID(nsICSSStyleSheet),(void**)&mPrefStyleSheet);
  if (NS_SUCCEEDED(result)) {
    NS_ASSERTION(mPrefStyleSheet, "null but no error");
    nsCOMPtr<nsIURI> uri;
    result = NS_NewURI(getter_AddRefs(uri), "about:PreferenceStyleSheet", nsnull);
    if (NS_SUCCEEDED(result)) {
      NS_ASSERTION(uri, "null but no error");
      result = mPrefStyleSheet->Init(uri);
      if (NS_SUCCEEDED(result)) {
        mPrefStyleSheet->SetDefaultNameSpaceID(kNameSpaceID_XHTML);
        mStyleSet->InsertUserStyleSheetBefore(mPrefStyleSheet, nsnull);
      }
    }
  } else {
    result = NS_ERROR_OUT_OF_MEMORY;
  }

#ifdef DEBUG_attinasi
  printf("CreatePrefStyleSheet completed: error=%ld\n",(long)result);
#endif

  return result;
}

nsresult PresShell::SetPrefColorRules(void)
{
  NS_ASSERTION(mPresContext,"null prescontext not allowed");
  if (mPresContext) {
    nsresult result = NS_OK;
    PRBool useDocColors = PR_TRUE;

    // see if we need to create the rules first
    if (NS_SUCCEEDED(mPresContext->GetCachedBoolPref(kPresContext_UseDocumentColors, useDocColors))) {
      if (!useDocColors) {

#ifdef DEBUG_attinasi
        printf(" - Creating rules for document colors\n");
#endif

        // OK, not using document colors, so we have to force the user's colors via style rules
        if (!mPrefStyleSheet) {
          result = CreatePreferenceStyleSheet();
        }
        if (NS_SUCCEEDED(result)) {
          NS_ASSERTION(mPrefStyleSheet, "prefstylesheet should not be null");

          nscolor bgColor;
          nscolor textColor;
          result = mPresContext->GetDefaultColor(&textColor);
          if (NS_SUCCEEDED(result)) {
            result = mPresContext->GetDefaultBackgroundColor(&bgColor);
          }
          if (NS_SUCCEEDED(result)) {
            // get the DOM interface to the stylesheet
            nsCOMPtr<nsIDOMCSSStyleSheet> sheet(do_QueryInterface(mPrefStyleSheet,&result));
            if (NS_SUCCEEDED(result)) {
              PRUint32 index = 0;
              nsAutoString strColor, strBackgroundColor;

              // create a rule for background and foreground color and
              // add it to the style sheet              
              // - the rule is !important so it overrides all but author
              //   important rules (when put into an agent stylesheet) and 
              //   all (even author important) when put into an override stylesheet

              ///////////////////////////////////////////////////////////////
              // - default colors: ':root {color:#RRGGBB !important;
              //                           background: #RRGGBB !important;}'
              ColorToString(textColor,strColor);
              ColorToString(bgColor,strBackgroundColor);
              result = sheet->InsertRule(NS_LITERAL_STRING(":root {color:") +
                                         strColor +
                                         NS_LITERAL_STRING(" !important; ") +
                                         NS_LITERAL_STRING("background:") +
                                         strBackgroundColor +
                                         NS_LITERAL_STRING(" !important; }"),
                                         0,&index);
              NS_ENSURE_SUCCESS(result, result);

              ///////////////////////////////////////////////////////////////
              // - everything else inherits the color, and has transparent background
              result = sheet->InsertRule(NS_LITERAL_STRING("* {color: inherit !important; background: transparent !important;} "),
                                         0,&index);
            }
          }
        }
      }
    }
    return result;
  } else {
    return NS_ERROR_FAILURE;
  }
}

nsresult PresShell::SetPrefLinkRules(void)
{
  NS_ASSERTION(mPresContext,"null prescontext not allowed");
  if (mPresContext) {
    nsresult result = NS_OK;

    if (!mPrefStyleSheet) {
      result = CreatePreferenceStyleSheet();
    }
    if (NS_SUCCEEDED(result)) {
      NS_ASSERTION(mPrefStyleSheet, "prefstylesheet should not be null");

      // get the DOM interface to the stylesheet
      nsCOMPtr<nsIDOMCSSStyleSheet> sheet(do_QueryInterface(mPrefStyleSheet,&result));
      if (NS_SUCCEEDED(result)) {

#ifdef DEBUG_attinasi
        printf(" - Creating rules for link and visited colors\n");
#endif

        // support default link colors: 
        //   this means the link colors need to be overridable, 
        //   which they are if we put them in the agent stylesheet,
        //   though if using an override sheet this will cause authors grief still
        //   In the agent stylesheet, they are !important when we are ignoring document colors
        //
        // XXX: Do active links and visited links get another color?
        //      They are red in the html.css rules, but there is no pref for their color.
        
        nscolor linkColor, visitedColor;
        result = mPresContext->GetDefaultLinkColor(&linkColor);
        if (NS_SUCCEEDED(result)) {
          result = mPresContext->GetDefaultVisitedLinkColor(&visitedColor);
        }
        if (NS_SUCCEEDED(result)) {
          // insert a rule to make links the preferred color
          PRUint32 index = 0;
          nsAutoString strColor;
          PRBool useDocColors = PR_TRUE;

          // see if we need to create the rules first
          mPresContext->GetCachedBoolPref(kPresContext_UseDocumentColors, useDocColors);

          ///////////////////////////////////////////////////////////////
          // - links: '*:link {color: #RRGGBB [!important];}'
          ColorToString(linkColor,strColor);
          NS_NAMED_LITERAL_STRING(notImportantStr, "}");
          NS_NAMED_LITERAL_STRING(importantStr, "!important}");
          const nsAString& ruleClose = useDocColors ? notImportantStr : importantStr;
          result = sheet->InsertRule(NS_LITERAL_STRING("*:link{color:") +
                                     strColor +
                                     ruleClose,
                                     0,&index);
          NS_ENSURE_SUCCESS(result, result);
            
          ///////////////////////////////////////////////////////////////
          // - visited links '*:visited {color: #RRGGBB [!important];}'
          ColorToString(visitedColor,strColor);
          // insert the rule
          result = sheet->InsertRule(NS_LITERAL_STRING("*:visited{color:") +
                                     strColor +
                                     ruleClose,
                                     0,&index);
            
          ///////////////////////////////////////////////////////////////
          // - active links '*:-moz-any-link {color: red [!important];}'
          // This has to be here (i.e. we can't just rely on the rule in the UA stylesheet)
          // because this entire stylesheet is at the user level (not UA level) and so
          // the two rules above override the rule in the UA stylesheet regardless of the
          // weights of the respective rules.
          result = sheet->InsertRule(NS_LITERAL_STRING("*:-moz-any-link:active{color:red") +
                                     ruleClose,
                                     0,&index);
        }

        if (NS_SUCCEEDED(result)) {
          PRBool underlineLinks = PR_TRUE;
          result = mPresContext->GetCachedBoolPref(kPresContext_UnderlineLinks,underlineLinks);
          if (NS_SUCCEEDED(result)) {
            // create a rule for underline: on or off
            PRUint32 index = 0;
            nsAutoString strRule;
            if (underlineLinks) {
              // create a rule to make underlining happen
              //  ':link, :visited {text-decoration:[underline|none];}'
              // no need for important, we want these to be overridable
              // NOTE: these must go in the agent stylesheet or they cannot be
              //       overridden by authors
  #ifdef DEBUG_attinasi
              printf (" - Creating rules for enabling link underlines\n");
  #endif
              // make a rule to make text-decoration: underline happen for links
              strRule.Append(NS_LITERAL_STRING("*:-moz-any-link{text-decoration:underline}"));
            } else {
  #ifdef DEBUG_attinasi
              printf (" - Creating rules for disabling link underlines\n");
  #endif
              // make a rule to make text-decoration: none happen for links
              strRule.Append(NS_LITERAL_STRING("*:-moz-any-link{text-decoration:none}"));
            }

            // ...now insert the rule
            result = sheet->InsertRule(strRule,0,&index);          
          }
        }
      }
    }
    return result;
  } else {
    return NS_ERROR_FAILURE;
  }
}

nsresult PresShell::SetPrefFocusRules(void)
{
  NS_ASSERTION(mPresContext,"null prescontext not allowed");
  nsresult result = NS_OK;

  if (!mPresContext)
    result = NS_ERROR_FAILURE;

  if (NS_SUCCEEDED(result) && !mPrefStyleSheet)
    result = CreatePreferenceStyleSheet();

  if (NS_SUCCEEDED(result)) {
    NS_ASSERTION(mPrefStyleSheet, "prefstylesheet should not be null");

    // get the DOM interface to the stylesheet
    nsCOMPtr<nsIDOMCSSStyleSheet> sheet(do_QueryInterface(mPrefStyleSheet,&result));
    if (NS_SUCCEEDED(result)) {
      PRBool useFocusColors;
      mPresContext->GetUseFocusColors(useFocusColors);
      nscolor focusBackground, focusText;
      result = mPresContext->GetFocusBackgroundColor(&focusBackground);
      nsresult result2 = mPresContext->GetFocusTextColor(&focusText);
      if (useFocusColors && NS_SUCCEEDED(result) && NS_SUCCEEDED(result2)) {
        // insert a rule to make focus the preferred color
        PRUint32 index = 0;
        nsAutoString strRule, strColor;

        ///////////////////////////////////////////////////////////////
        // - focus: '*:focus
        ColorToString(focusText,strColor);
        strRule.Append(NS_LITERAL_STRING("*:focus,*:focus>font {color: "));
        strRule.Append(strColor);
        strRule.Append(NS_LITERAL_STRING(" !important; background-color: "));
        ColorToString(focusBackground,strColor);
        strRule.Append(strColor);
        strRule.Append(NS_LITERAL_STRING(" !important; } "));
        // insert the rules
        result = sheet->InsertRule(strRule,0,&index);
      }
      PRUint8 focusRingWidth = 1;
      result = mPresContext->GetFocusRingWidth(&focusRingWidth);
      PRBool focusRingOnAnything;
      mPresContext->GetFocusRingOnAnything(focusRingOnAnything);

      if ((NS_SUCCEEDED(result) && focusRingWidth != 1 && focusRingWidth <= 4 ) || focusRingOnAnything) {
        PRUint32 index = 0;
        nsAutoString strRule;
        if (!focusRingOnAnything)
          strRule.Append(NS_LITERAL_STRING(":link:focus, :visited"));    // If we only want focus rings on the normal things like links
        strRule.Append(NS_LITERAL_STRING(":focus {-moz-outline: "));     // For example 3px dotted WindowText (maximum 4)
        strRule.AppendInt(focusRingWidth);
        strRule.Append(NS_LITERAL_STRING("px dotted WindowText !important; } "));     // For example 3px dotted WindowText
        // insert the rules
        result = sheet->InsertRule(strRule,0,&index);
        NS_ENSURE_SUCCESS(result, result);
        if (focusRingWidth != 1) {
          // If the focus ring width is different from the default, fix buttons with rings
          strRule.Assign(NS_LITERAL_STRING("button:-moz-focus-inner, input[type=\"reset\"]:-moz-focus-inner,"));
          strRule.Append(NS_LITERAL_STRING("input[type=\"button\"]:-moz-focus-inner, "));
          strRule.Append(NS_LITERAL_STRING("input[type=\"submit\"]:-moz-focus-inner { padding: 1px 2px 1px 2px; border: "));
          strRule.AppendInt(focusRingWidth);
          strRule.Append(NS_LITERAL_STRING("px dotted transparent !important; } "));
          result = sheet->InsertRule(strRule,0,&index);
          NS_ENSURE_SUCCESS(result, result);
          
          strRule.Assign(NS_LITERAL_STRING("button:focus:-moz-focus-inner, input[type=\"reset\"]:focus:-moz-focus-inner,"));
          strRule.Append(NS_LITERAL_STRING("input[type=\"button\"]:focus:-moz-focus-inner, input[type=\"submit\"]:focus:-moz-focus-inner {"));
          strRule.Append(NS_LITERAL_STRING("border-color: ButtonText !important; }"));
          result = sheet->InsertRule(strRule,0,&index);
        }
      }
    }
  }
  return result;
}


NS_IMETHODIMP
PresShell::SetDisplaySelection(PRInt16 aToggle)
{
  return mSelection->SetDisplaySelection(aToggle);
}

NS_IMETHODIMP
PresShell::GetDisplaySelection(PRInt16 *aToggle)
{
  return mSelection->GetDisplaySelection(aToggle);
}

NS_IMETHODIMP
PresShell::GetSelection(SelectionType aType, nsISelection **aSelection)
{
  if (!aSelection || !mSelection)
    return NS_ERROR_NULL_POINTER;
  return mSelection->GetSelection(aType, aSelection);
}

NS_IMETHODIMP
PresShell::ScrollSelectionIntoView(SelectionType aType, SelectionRegion aRegion, PRBool aIsSynchronous)
{
  if (!mSelection)
    return NS_ERROR_NULL_POINTER;

  return mSelection->ScrollSelectionIntoView(aType, aRegion, aIsSynchronous);
}

NS_IMETHODIMP
PresShell::RepaintSelection(SelectionType aType)
{
  if (!mSelection)
    return NS_ERROR_NULL_POINTER;

  return mSelection->RepaintSelection(mPresContext, aType);
}

NS_IMETHODIMP
PresShell::GetFrameSelection(nsIFrameSelection** aSelection)
{
  if (!aSelection || !mSelection)
    return NS_ERROR_NULL_POINTER;
  *aSelection = mSelection;
  (*aSelection)->AddRef();
  return NS_OK;
}


// Make shell be a document observer
NS_IMETHODIMP
PresShell::BeginObservingDocument()
{
  if (mDocument) {
    mDocument->AddObserver(this);
  }
  return NS_OK;
}

// Make shell stop being a document observer
NS_IMETHODIMP
PresShell::EndObservingDocument()
{
  mIsDocumentGone = PR_TRUE;
  if (mDocument) {
    mDocument->RemoveObserver(this);
  }
  if (mSelection){
    nsCOMPtr<nsISelection> domselection;
    nsresult result;
    result = mSelection->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domselection));
    if (NS_FAILED(result))
      return result;
    if (!domselection)
      return NS_ERROR_UNEXPECTED;
    mSelection->ShutDown();
  }

  return NS_OK;
}

#ifdef DEBUG_kipp
char* nsPresShell_ReflowStackPointerTop;
#endif

static void CheckForFocus(nsPIDOMWindow* aOurWindow, nsIFocusController* aFocusController, nsIDocument* aDocument)
{
  // Now that we have a root frame, set focus in to the presshell, but
  // only do this if our window is currently focused
  // Restore focus if we're the active window or a parent of a previously
  // active window.
  if (aFocusController) {
    nsCOMPtr<nsIDOMWindowInternal> focusedWindow;
    aFocusController->GetFocusedWindow(getter_AddRefs(focusedWindow));
    
    // See if the command dispatcher is holding on to an orphan window.
    // This happens when you move from an inner frame to an outer frame
    // (e.g., _parent, _top)
    if (focusedWindow) {
      nsCOMPtr<nsIDOMDocument> domDoc;
      focusedWindow->GetDocument(getter_AddRefs(domDoc));
      if (!domDoc) {
        // We're pointing to garbage. Go ahead and let this
        // presshell take the focus.
        focusedWindow = do_QueryInterface(aOurWindow);
        aFocusController->SetFocusedWindow(focusedWindow);
      }
    }

    nsCOMPtr<nsIDOMWindowInternal> domWindow = do_QueryInterface(aOurWindow);
    if (domWindow == focusedWindow) {
      PRBool active;
      aFocusController->GetActive(&active);
      if(active) {
        // We need to restore focus to the window.
        domWindow->Focus();
      }
    }
  }
}

nsresult
PresShell::NotifyReflowObservers(const char *aData)
{
  if (!aData) { return NS_ERROR_NULL_POINTER; }

  nsresult               result = NS_OK;
  nsCOMPtr<nsISupports>  pContainer;
  nsCOMPtr<nsIDocShell>  pDocShell;

  result = mPresContext->GetContainer( getter_AddRefs( pContainer ) );

  if (NS_SUCCEEDED( result ) && pContainer) {
    pDocShell = do_QueryInterface( pContainer,
                                  &result );

    if (NS_SUCCEEDED( result ) && pDocShell && mObserverService) {
      result = mObserverService->NotifyObservers( pDocShell,
                                                  NS_PRESSHELL_REFLOW_TOPIC,
                                                  NS_ConvertASCIItoUCS2(aData).get() );
      // notice that we don't really care what the observer service returns
    }
  }
  return NS_OK;
}

static nsresult
GetRootScrollFrame(nsIPresContext* aPresContext, nsIFrame* aRootFrame, nsIFrame** aScrollFrame) {

  // Frames: viewport->scroll->scrollport (Gfx) or viewport->scroll (Native)
  // Types:  viewport->scroll->sroll               viewport->scroll

  // Ensure root frame is a viewport frame
  *aScrollFrame = nsnull;
  nsIFrame* theFrame = nsnull;
  if (aRootFrame) {
    nsCOMPtr<nsIAtom> fType;
    aRootFrame->GetFrameType(getter_AddRefs(fType));
    if (fType && (nsLayoutAtoms::viewportFrame == fType.get())) {

      // If child is scrollframe keep it (native)
      aRootFrame->FirstChild(aPresContext, nsnull, &theFrame);
      if (theFrame) {
        theFrame->GetFrameType(getter_AddRefs(fType));
        if (nsLayoutAtoms::scrollFrame == fType.get()) {
          *aScrollFrame = theFrame;

          // If the first child of that is scrollframe, use it instead (gfx)
          theFrame->FirstChild(aPresContext, nsnull, &theFrame);
          if (theFrame) {
            theFrame->GetFrameType(getter_AddRefs(fType));
            if (nsLayoutAtoms::scrollFrame == fType.get()) {
              *aScrollFrame = theFrame;
            }
          }
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
PresShell::GetDidInitialReflow(PRBool *aDidInitialReflow)
{
  if (!aDidInitialReflow)
    return NS_ERROR_FAILURE;

  *aDidInitialReflow = mDidInitialReflow;

  return NS_OK;
}

NS_IMETHODIMP
PresShell::InitialReflow(nscoord aWidth, nscoord aHeight)
{
  nsCOMPtr<nsIContent> root;
  mDidInitialReflow = PR_TRUE;

#ifdef NS_DEBUG
  if (VERIFY_REFLOW_NOISY_RC & gVerifyReflowFlags) {
    nsCOMPtr<nsIURI> uri;
    if (mDocument) {
      mDocument->GetDocumentURL(getter_AddRefs(uri));
      if (uri) {
        nsCAutoString url;
        uri->GetSpec(url);
        printf("*** PresShell::InitialReflow (this=%p, url='%s')\n", (void*)this, url.get());
      }
    }
  }
#endif

  // notice that we ignore the result
  NotifyReflowObservers(NS_PRESSHELL_INITIAL_REFLOW);
  mCaret->EraseCaret();
  //StCaretHider  caretHider(mCaret);			// stack-based class hides caret until dtor.
  
  WillCauseReflow();

  if (mPresContext) {
    nsRect r(0, 0, aWidth, aHeight);
    mPresContext->SetVisibleArea(r);
  }

  if (mDocument) {
    mDocument->GetRootContent(getter_AddRefs(root));
  }

  // Get the root frame from the frame manager
  nsIFrame* rootFrame;
  mFrameManager->GetRootFrame(&rootFrame);
  
  if (root) {
    MOZ_TIMER_DEBUGLOG(("Reset and start: Frame Creation: PresShell::InitialReflow(), this=%p\n",
                        (void*)this));
    MOZ_TIMER_RESET(mFrameCreationWatch);
    MOZ_TIMER_START(mFrameCreationWatch);
    CtlStyleWatch(kStyleWatchEnable,mStyleSet);

    if (!rootFrame) {
      // Have style sheet processor construct a frame for the
      // precursors to the root content object's frame
      mStyleSet->ConstructRootFrame(mPresContext, root, rootFrame);
      mFrameManager->SetRootFrame(rootFrame);
    }

    // Have the style sheet processor construct frame for the root
    // content object down
    mStyleSet->ContentInserted(mPresContext, nsnull, root, 0);
    VERIFY_STYLE_TREE;
    MOZ_TIMER_DEBUGLOG(("Stop: Frame Creation: PresShell::InitialReflow(), this=%p\n",
                        (void*)this));
    MOZ_TIMER_STOP(mFrameCreationWatch);
    CtlStyleWatch(kStyleWatchDisable,mStyleSet);
  }

  if (rootFrame) {
    MOZ_TIMER_DEBUGLOG(("Reset and start: Reflow: PresShell::InitialReflow(), this=%p\n",
                        (void*)this));
    MOZ_TIMER_RESET(mReflowWatch);
    MOZ_TIMER_START(mReflowWatch);
    // Kick off a top-down reflow
    NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
                 ("enter nsPresShell::InitialReflow: %d,%d", aWidth, aHeight));
#ifdef NS_DEBUG
    if (nsIFrameDebug::GetVerifyTreeEnable()) {
      nsIFrameDebug*  frameDebug;

      if (NS_SUCCEEDED(rootFrame->QueryInterface(NS_GET_IID(nsIFrameDebug),
                                                (void**)&frameDebug))) {
        frameDebug->VerifyTree();
      }
    }
#endif
#ifdef DEBUG_kipp
    nsPresShell_ReflowStackPointerTop = (char*) &aWidth;
#endif
    nsRect                bounds;
    mPresContext->GetVisibleArea(bounds);
    nsSize                maxSize(bounds.width, bounds.height);
    nsHTMLReflowMetrics   desiredSize(nsnull);
    nsReflowStatus        status;
    nsIRenderingContext*  rcx = nsnull;

    nsresult rv=CreateRenderingContext(rootFrame, &rcx);
	if (NS_FAILED(rv)) return rv;

    mIsReflowing = PR_TRUE;

    nsHTMLReflowState reflowState(mPresContext, rootFrame,
                                  eReflowReason_Initial, rcx, maxSize);
    rootFrame->WillReflow(mPresContext);
    nsContainerFrame::PositionFrameView(mPresContext, rootFrame);
    rootFrame->Reflow(mPresContext, desiredSize, reflowState, status);
    rootFrame->SizeTo(mPresContext, desiredSize.width, desiredSize.height);
    mPresContext->SetVisibleArea(nsRect(0,0,desiredSize.width,desiredSize.height));

    nsIView* view;
    rootFrame->GetView(mPresContext, &view);
    if (view) {
      nsContainerFrame::SyncFrameViewAfterReflow(mPresContext, rootFrame, view,
                                                 nsnull);
    }
    rootFrame->DidReflow(mPresContext, nsnull, NS_FRAME_REFLOW_FINISHED);
      
#ifdef NS_DEBUG
    if (nsIFrameDebug::GetVerifyTreeEnable()) {
      nsIFrameDebug*  frameDebug;

      if (NS_SUCCEEDED(rootFrame->QueryInterface(NS_GET_IID(nsIFrameDebug),
                                                 (void**)&frameDebug))) {
        frameDebug->VerifyTree();
      }
    }
#endif
    VERIFY_STYLE_TREE;
    NS_IF_RELEASE(rcx);
    NS_FRAME_LOG(NS_FRAME_TRACE_CALLS, ("exit nsPresShell::InitialReflow"));
    MOZ_TIMER_DEBUGLOG(("Stop: Reflow: PresShell::InitialReflow(), this=%p\n", (void*)this));
    MOZ_TIMER_STOP(mReflowWatch);

    mIsReflowing = PR_FALSE;
  }

  DidCauseReflow();

  if (mViewManager && mCaret && !mViewEventListener) {
    nsIScrollableView* scrollingView = nsnull;
    mViewManager->GetRootScrollableView(&scrollingView);

    if (scrollingView) {
      mViewEventListener = new PresShellViewEventListener;

      if (!mViewEventListener)
        return NS_ERROR_OUT_OF_MEMORY;

      NS_ADDREF(mViewEventListener);
      mViewEventListener->SetPresShell(this);
      scrollingView->AddScrollPositionListener((nsIScrollPositionListener *)mViewEventListener);
      mViewManager->AddCompositeListener((nsICompositeListener *)mViewEventListener);
    }
  }

  // For printing, we just immediately unsuppress.
  PRBool isPaginated = PR_FALSE;
  mPresContext->IsPaginated(&isPaginated);
  if (!isPaginated) {
    // Kick off a one-shot timer based off our pref value.  When this timer
    // fires, if painting is still locked down, then we will go ahead and
    // trigger a full invalidate and allow painting to proceed normally.
    mPaintingSuppressed = PR_TRUE;
    mPaintSuppressionTimer = do_CreateInstance("@mozilla.org/timer;1");
    if (!mPaintSuppressionTimer)
      // Uh-oh.  We must be out of memory.  No point in keeping painting locked down.
      mPaintingSuppressed = PR_FALSE;
    else {
      // Initialize the timer.
      PRInt32 delay = PAINTLOCK_EVENT_DELAY; // Use this value if we fail to get the pref value.
      nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID));
      if (prefs)
        prefs->GetIntPref("nglayout.initialpaint.delay", &delay);

      nsCOMPtr<nsITimerInternal> ti = do_QueryInterface(mPaintSuppressionTimer);
      ti->SetIdle(PR_FALSE);

      mPaintSuppressionTimer->InitWithFuncCallback(sPaintSuppressionCallback, this, delay, 
                                                   nsITimer::TYPE_ONE_SHOT);
    }
  }

  return NS_OK; //XXX this needs to be real. MMP
}

void
PresShell::sPaintSuppressionCallback(nsITimer *aTimer, void* aPresShell)
{
  PresShell* self = NS_STATIC_CAST(PresShell*, aPresShell);
  if (self)
    self->UnsuppressPainting();
}

NS_IMETHODIMP
PresShell::ResizeReflow(nscoord aWidth, nscoord aHeight)
{
  PRBool firstReflow = PR_FALSE;

    // notice that we ignore the result
  NotifyReflowObservers(NS_PRESSHELL_RESIZE_REFLOW);
  mViewManager->CacheWidgetChanges(PR_TRUE);

  mCaret->EraseCaret();
  //StCaretHider  caretHider(mCaret);			// stack-based class hides caret until dtor.
  WillCauseReflow();

  if (mPresContext) {
    nsRect r(0, 0, aWidth, aHeight);
    mPresContext->SetVisibleArea(r);
  }

  // If we don't have a root frame yet, that means we haven't had our initial
  // reflow...
  nsIFrame* rootFrame;
  mFrameManager->GetRootFrame(&rootFrame);
  if (rootFrame) {
    // Kick off a top-down reflow
    NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
                 ("enter nsPresShell::ResizeReflow: %d,%d", aWidth, aHeight));
#ifdef NS_DEBUG
    if (nsIFrameDebug::GetVerifyTreeEnable()) {
      nsIFrameDebug*  frameDebug;

      if (NS_SUCCEEDED(rootFrame->QueryInterface(NS_GET_IID(nsIFrameDebug),
                                                 (void**)&frameDebug))) {
        frameDebug->VerifyTree();
      }
    }
#endif
#ifdef DEBUG_kipp
    nsPresShell_ReflowStackPointerTop = (char*) &aWidth;
#endif
    nsRect                bounds;
    mPresContext->GetVisibleArea(bounds);
    nsSize                maxSize(bounds.width, bounds.height);
    nsHTMLReflowMetrics   desiredSize(nsnull);
    nsReflowStatus        status;
    nsIRenderingContext*  rcx = nsnull;

    nsresult rv=CreateRenderingContext(rootFrame, &rcx);
    if (NS_FAILED(rv)) return rv;

    nsHTMLReflowState reflowState(mPresContext, rootFrame,
                                  eReflowReason_Resize, rcx, maxSize);

    rootFrame->WillReflow(mPresContext);
    nsContainerFrame::PositionFrameView(mPresContext, rootFrame);
    rootFrame->Reflow(mPresContext, desiredSize, reflowState, status);
    rootFrame->SizeTo(mPresContext, desiredSize.width, desiredSize.height);
    mPresContext->SetVisibleArea(nsRect(0,0,desiredSize.width,desiredSize.height));

    nsIView* view;
    rootFrame->GetView(mPresContext, &view);
    if (view) {
      nsContainerFrame::SyncFrameViewAfterReflow(mPresContext, rootFrame, view,
                                                 nsnull);
    }
    rootFrame->DidReflow(mPresContext, nsnull, NS_FRAME_REFLOW_FINISHED);
#ifdef NS_DEBUG
    if (nsIFrameDebug::GetVerifyTreeEnable()) {
      nsIFrameDebug*  frameDebug;

      if (NS_SUCCEEDED(rootFrame->QueryInterface(NS_GET_IID(nsIFrameDebug),
                                                 (void**)&frameDebug))) {
        frameDebug->VerifyTree();
      }
    }
#endif
    VERIFY_STYLE_TREE;
    NS_IF_RELEASE(rcx);
    NS_FRAME_LOG(NS_FRAME_TRACE_CALLS, ("exit nsPresShell::ResizeReflow"));

    // XXX if debugging then we should assert that the cache is empty
  } else {
    firstReflow = PR_TRUE;
#ifdef NOISY
    printf("PresShell::ResizeReflow: null root frame\n");
#endif
  }
  DidCauseReflow();
  // if the proper flag is set, VerifyReflow now
#ifdef NS_DEBUG
  if (GetVerifyReflowEnable() && (VERIFY_REFLOW_DURING_RESIZE_REFLOW & gVerifyReflowFlags))
  {
    mInVerifyReflow = PR_TRUE;
    /*PRBool ok = */VerifyIncrementalReflow();
    mInVerifyReflow = PR_FALSE;
  }
#endif
  
  mViewManager->CacheWidgetChanges(PR_FALSE);
  HandlePostedDOMEvents();
  HandlePostedAttributeChanges();
  HandlePostedReflowCallbacks();

  if (!firstReflow) {
    //Set resize event timer
    CreateResizeEventTimer();
  }
  
  return NS_OK; //XXX this needs to be real. MMP
}

#define RESIZE_EVENT_DELAY 200

void
PresShell::CreateResizeEventTimer ()
{
  KillResizeEventTimer();

  if (mIsDocumentGone)
    return;

  mResizeEventTimer = do_CreateInstance("@mozilla.org/timer;1");
  if (mResizeEventTimer) {
    mResizeEventTimer->InitWithFuncCallback(sResizeEventCallback, this, RESIZE_EVENT_DELAY, 
                                            nsITimer::TYPE_ONE_SHOT);
  }
}

void
PresShell::KillResizeEventTimer()
{
  if(mResizeEventTimer) {
    mResizeEventTimer->Cancel();
    mResizeEventTimer = nsnull;
  }
}

void
PresShell::sResizeEventCallback(nsITimer *aTimer, void* aPresShell)
{
  PresShell* self = NS_STATIC_CAST(PresShell*, aPresShell);
  if (self) {
    self->FireResizeEvent();  
  }
}

void
PresShell::FireResizeEvent()
{
  if (mIsDocumentGone)
    return;

  //Send resize event from here.
  nsEvent event;
  nsEventStatus status = nsEventStatus_eIgnore;
  event.eventStructType = NS_EVENT;
  event.message = NS_RESIZE_EVENT;
  event.time = 0;

  nsCOMPtr<nsIScriptGlobalObject> globalObj;
	mDocument->GetScriptGlobalObject(getter_AddRefs(globalObj));
  if (globalObj) {
    globalObj->HandleDOMEvent(mPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
  }
}

NS_IMETHODIMP
PresShell::ScrollFrameIntoView(nsIFrame *aFrame)
{
  if (!aFrame)
    return NS_ERROR_NULL_POINTER;
  
  if (IsScrollingEnabled())
    return ScrollFrameIntoView(aFrame, NS_PRESSHELL_SCROLL_ANYWHERE,
                               NS_PRESSHELL_SCROLL_ANYWHERE);
  return NS_OK;
}

NS_IMETHODIMP
PresShell::SetIgnoreFrameDestruction(PRBool aIgnore)
{
  mIgnoreFrameDestruction = aIgnore;
  return NS_OK;
}

NS_IMETHODIMP
PresShell::NotifyDestroyingFrame(nsIFrame* aFrame)
{
  if (!mIgnoreFrameDestruction) {
    // Cancel any pending reflow commands targeted at this frame
    CancelReflowCommandInternal(aFrame, nsnull);

    // Notify the frame manager
    if (mFrameManager) {
      mFrameManager->NotifyDestroyingFrame(aFrame);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
PresShell::GetFrameManager(nsIFrameManager** aFrameManager) const
{
  *aFrameManager = mFrameManager;
  NS_IF_ADDREF(mFrameManager);
  return NS_OK;
}

NS_IMETHODIMP PresShell::GetCaret(nsICaret **outCaret)
{
  if (!outCaret || !mCaret)
    return NS_ERROR_NULL_POINTER;
  return mCaret->QueryInterface(NS_GET_IID(nsICaret), (void **)outCaret);
}

NS_IMETHODIMP PresShell::SetCaretEnabled(PRBool aInEnable)
{
  nsresult result = NS_OK;
  PRBool   oldEnabled = mCaretEnabled;
	
  mCaretEnabled = aInEnable;
	
  if (mCaret && (mCaretEnabled != oldEnabled))
  {
/*  Don't change the caret's selection here! This was an evil side-effect of SetCaretEnabled()
    nsCOMPtr<nsIDOMSelection> domSel;
    if (NS_SUCCEEDED(GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSel))) && domSel)
      mCaret->SetCaretDOMSelection(domSel);
*/
    result = mCaret->SetCaretVisible(mCaretEnabled);
  }
	
  return result;
}

NS_IMETHODIMP PresShell::SetCaretWidth(PRInt16 pixels)
{
  return mCaret->SetCaretWidth(pixels);
}

NS_IMETHODIMP PresShell::SetCaretReadOnly(PRBool aReadOnly)
{
  return mCaret->SetCaretReadOnly(aReadOnly);
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP PresShell::GetCaretEnabled(PRBool *aOutEnabled)
{
  if (!aOutEnabled) { return NS_ERROR_INVALID_ARG; }
  *aOutEnabled = mCaretEnabled;
  return NS_OK;
}

NS_IMETHODIMP PresShell::SetSelectionFlags(PRInt16 aInEnable)
{
  mSelectionFlags = aInEnable;
  return NS_OK;
}

NS_IMETHODIMP PresShell::GetSelectionFlags(PRInt16 *aOutEnable)
{
  if (!aOutEnable)
    return NS_ERROR_INVALID_ARG;
  *aOutEnable = mSelectionFlags;
  return NS_OK;
}

//implementation of nsISelectionController

NS_IMETHODIMP 
PresShell::CharacterMove(PRBool aForward, PRBool aExtend)
{
  return mSelection->CharacterMove(aForward, aExtend);  
}

NS_IMETHODIMP 
PresShell::WordMove(PRBool aForward, PRBool aExtend)
{
  return mSelection->WordMove(aForward, aExtend);  
}

NS_IMETHODIMP 
PresShell::LineMove(PRBool aForward, PRBool aExtend)
{
  nsresult result = mSelection->LineMove(aForward, aExtend);  
// if we cant go down/up any more we must then move caret completely to 
// end/beginning respectively.
  if (NS_FAILED(result)) 
    result = CompleteMove(aForward,aExtend);
  return result;
}

NS_IMETHODIMP 
PresShell::IntraLineMove(PRBool aForward, PRBool aExtend)
{
  return mSelection->IntraLineMove(aForward, aExtend);  
}



NS_IMETHODIMP 
PresShell::PageMove(PRBool aForward, PRBool aExtend)
{
  nsresult result;
  nsCOMPtr<nsIViewManager> viewManager;
  nsIScrollableView *scrollableView;
  result = GetViewManager(getter_AddRefs(viewManager));
  if (NS_FAILED(result)) 
    return result;
  if (!viewManager) 
    return NS_ERROR_UNEXPECTED;
  result = viewManager->GetRootScrollableView(&scrollableView);
  if (NS_FAILED(result)) 
    return result;
  if (!scrollableView) 
    return NS_ERROR_UNEXPECTED;
  nsIView *scrolledView;
  result = scrollableView->GetScrolledView(scrolledView);
  mSelection->CommonPageMove(aForward, aExtend, scrollableView, mSelection);
  // do ScrollSelectionIntoView()
  return ScrollSelectionIntoView(nsISelectionController::SELECTION_NORMAL, nsISelectionController::SELECTION_FOCUS_REGION, PR_TRUE);
}



NS_IMETHODIMP 
PresShell::ScrollPage(PRBool aForward)
{
  nsCOMPtr<nsIViewManager> viewManager;
  nsresult result = GetViewManager(getter_AddRefs(viewManager));
  if (NS_SUCCEEDED(result) && viewManager)
  {
    nsIScrollableView *scrollView;
    result = viewManager->GetRootScrollableView(&scrollView);
    if (NS_SUCCEEDED(result) && scrollView)
    {
      scrollView->ScrollByPages(aForward ? 1 : -1);
    }
  }
  return result;
}

NS_IMETHODIMP
PresShell::ScrollLine(PRBool aForward)
{
  nsCOMPtr<nsIViewManager> viewManager;
  nsresult result = GetViewManager(getter_AddRefs(viewManager));
  if (NS_SUCCEEDED(result) && viewManager)
  {
    nsIScrollableView *scrollView;
    result = viewManager->GetRootScrollableView(&scrollView);
    if (NS_SUCCEEDED(result) && scrollView)
    {
#ifdef MOZ_WIDGET_COCOA
      // Emulate the Mac IE behavior of scrolling a minimum of 2 lines
      // rather than 1.  This vastly improves scrolling speed.
      scrollView->ScrollByLines(0, aForward ? 2 : -2);
#else
      scrollView->ScrollByLines(0, aForward ? 1 : -1);
#endif
      
//NEW FOR LINES    
      // force the update to happen now, otherwise multiple scrolls can
      // occur before the update is processed. (bug #7354)

    // I'd use Composite here, but it doesn't always work.
      // vm->Composite();
      viewManager->ForceUpdate();
    }
  }

  return result;
}

NS_IMETHODIMP
PresShell::ScrollHorizontal(PRBool aLeft)
{
  nsCOMPtr<nsIViewManager> viewManager;
  nsresult result = GetViewManager(getter_AddRefs(viewManager));
  if (NS_SUCCEEDED(result) && viewManager)
  {
    nsIScrollableView *scrollView;
    result = viewManager->GetRootScrollableView(&scrollView);
    if (NS_SUCCEEDED(result) && scrollView)
    {
      scrollView->ScrollByLines(aLeft ? -1 : 1, 0);
//NEW FOR LINES    
      // force the update to happen now, otherwise multiple scrolls can
      // occur before the update is processed. (bug #7354)

    // I'd use Composite here, but it doesn't always work.
      // vm->Composite();
      viewManager->ForceUpdate();
    }
  }

  return result;
}

NS_IMETHODIMP
PresShell::CompleteScroll(PRBool aForward)
{
  nsCOMPtr<nsIViewManager> viewManager;
  nsresult result = GetViewManager(getter_AddRefs(viewManager));
  if (NS_SUCCEEDED(result) && viewManager)
  {
    nsIScrollableView *scrollView;
    result = viewManager->GetRootScrollableView(&scrollView);
    if (NS_SUCCEEDED(result) && scrollView)
    {
      scrollView->ScrollByWhole(!aForward);//TRUE = top, aForward TRUE=bottom
    }
  }
  return result;
}

NS_IMETHODIMP
PresShell::CompleteMove(PRBool aForward, PRBool aExtend)
{
  nsIScrollableView *scrollableView;
  if (!mViewManager) 
    return NS_ERROR_UNEXPECTED;
  nsresult result = mViewManager->GetRootScrollableView(&scrollableView);
  if (NS_FAILED(result)) 
    return result;
  if (!scrollableView) 
    return NS_ERROR_UNEXPECTED;
  nsIView *scrolledView;
  result = scrollableView->GetScrolledView(scrolledView);
  // get a frame
  void *clientData;
  scrolledView->GetClientData(clientData);
  nsIFrame *frame = (nsIFrame *)clientData;
  if (!frame)
    return NS_ERROR_FAILURE;
  //we need to get to the area frame.
  nsCOMPtr<nsIAtom> frameType; 
  do 
  {
    frame->GetFrameType(getter_AddRefs(frameType));
    if (frameType != nsLayoutAtoms::areaFrame)
    {
      result = frame->FirstChild(mPresContext, nsnull, &frame);
      if (NS_FAILED(result) || !frame)
        break;
    }
  }while(frameType != nsLayoutAtoms::areaFrame);
  
  if (!frame)
    return NS_ERROR_FAILURE; //could not find an area frame.

  PRInt8  outsideLimit = -1;//search from beginning
  nsPeekOffsetStruct pos;
  pos.mAmount = eSelectLine;
  pos.mTracker = this;
  pos.mContentOffset = 0;
  pos.mContentOffsetEnd = 0;
  pos.mScrollViewStop = PR_FALSE;//dont stop on scrolled views.
  if (aForward)
  {
    outsideLimit = 1;//search from end
    nsRect rect;
    frame->GetRect(rect);
    pos.mDesiredX = rect.width * 2;//search way off to right of line
    pos.mDirection = eDirPrevious; //seach backwards from the end
  }
  else
  {
    pos.mDesiredX = -1; //start before line
    pos.mDirection = eDirNext; //search forwards from before beginning
  }
  
  do
  {
    result = nsFrame::GetNextPrevLineFromeBlockFrame(mPresContext,
                                &pos,
                                frame, 
                                0, //irrelavent since we set outsidelimit 
                                outsideLimit
                                );
    if (NS_COMFALSE == result) //NS_COMFALSE should ALSO break
      break;
    if (NS_OK != result || !pos.mResultFrame ) 
      return result?result:NS_ERROR_FAILURE;
    nsCOMPtr<nsILineIteratorNavigator> newIt; 
    //check to see if this is ANOTHER blockframe inside the other one if so then call into its lines
    result = pos.mResultFrame->QueryInterface(NS_GET_IID(nsILineIteratorNavigator),getter_AddRefs(newIt));
    if (NS_SUCCEEDED(result) && newIt)
      frame = pos.mResultFrame;
  }
  while (NS_SUCCEEDED(result));//end 'do'
  
  mSelection->HandleClick(pos.mResultContent ,pos.mContentOffset ,pos.mContentOffsetEnd ,aExtend, PR_FALSE, pos.mPreferLeft);
  return ScrollSelectionIntoView(nsISelectionController::SELECTION_NORMAL, nsISelectionController::SELECTION_FOCUS_REGION, PR_TRUE);
}

NS_IMETHODIMP 
PresShell::SelectAll()
{
  return mSelection->SelectAll();
}

NS_IMETHODIMP
PresShell::CheckVisibility(nsIDOMNode *node, PRInt16 startOffset, PRInt16 EndOffset, PRBool *_retval)
{
  if (!node || startOffset>EndOffset || !_retval || startOffset<0 || EndOffset<0)
    return NS_ERROR_INVALID_ARG;
  *_retval = PR_FALSE; //initialize return parameter
  nsCOMPtr<nsIContent> content(do_QueryInterface(node));
  if (!content)
    return NS_ERROR_FAILURE;
  nsIFrame *frame;
  nsresult result = GetPrimaryFrameFor(content,&frame);
  if (NS_FAILED(result)) //failure is taken as a no.
    return result;
  if (!frame) //no frame to look at so it must not be visible
    return NS_OK;  
  //start process now to go through all frames to find startOffset. then check chars after that to see 
  //if anything until EndOffset is visible.
  PRBool finished = PR_FALSE;
  frame->CheckVisibility(mPresContext,startOffset,EndOffset,PR_TRUE,&finished, _retval);
  return NS_OK;//dont worry about other return val
}

//end implementations nsISelectionController



NS_IMETHODIMP
PresShell::StyleChangeReflow()
{

  // notify any presshell observers about the reflow.
  // notice that we ignore the result
  NotifyReflowObservers(NS_PRESSHELL_STYLE_CHANGE_REFLOW);

  WillCauseReflow();

  nsIFrame* rootFrame;
  mFrameManager->GetRootFrame(&rootFrame);
  if (rootFrame) {
    // Kick off a top-down reflow
    NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
                 ("enter nsPresShell::StyleChangeReflow"));
#ifdef NS_DEBUG
    if (nsIFrameDebug::GetVerifyTreeEnable()) {
      nsIFrameDebug*  frameDebug;

      if (NS_SUCCEEDED(rootFrame->QueryInterface(NS_GET_IID(nsIFrameDebug),
                                                 (void**)&frameDebug))) {
        frameDebug->VerifyTree();
      }
    }
#endif
    nsRect                bounds;
    mPresContext->GetVisibleArea(bounds);
    nsSize                maxSize(bounds.width, bounds.height);
    nsHTMLReflowMetrics   desiredSize(nsnull);
    nsReflowStatus        status;
    nsIRenderingContext*  rcx = nsnull;

    nsresult rv=CreateRenderingContext(rootFrame, &rcx);
	if (NS_FAILED(rv)) return rv;

    nsHTMLReflowState reflowState(mPresContext, rootFrame,
                                  eReflowReason_StyleChange, rcx, maxSize);

    rootFrame->WillReflow(mPresContext);
    nsContainerFrame::PositionFrameView(mPresContext, rootFrame);
    rootFrame->Reflow(mPresContext, desiredSize, reflowState, status);
    rootFrame->SizeTo(mPresContext, desiredSize.width, desiredSize.height);
    mPresContext->SetVisibleArea(nsRect(0,0,desiredSize.width,desiredSize.height));
    nsIView* view;
    rootFrame->GetView(mPresContext, &view);
    if (view) {
      nsContainerFrame::SyncFrameViewAfterReflow(mPresContext, rootFrame, view,
                                                 nsnull);
    }
    rootFrame->DidReflow(mPresContext, nsnull, NS_FRAME_REFLOW_FINISHED);
#ifdef NS_DEBUG
    if (nsIFrameDebug::GetVerifyTreeEnable()) {
      nsIFrameDebug*  frameDebug;

      if (NS_SUCCEEDED(rootFrame->QueryInterface(NS_GET_IID(nsIFrameDebug),
                                                 (void**)&frameDebug))) {
        frameDebug->VerifyTree();
      }
    }
#endif
    VERIFY_STYLE_TREE;
    NS_IF_RELEASE(rcx);
    NS_FRAME_LOG(NS_FRAME_TRACE_CALLS, ("exit nsPresShell::StyleChangeReflow"));
  }

  DidCauseReflow();

  return NS_OK; //XXX this needs to be real. MMP
}

NS_IMETHODIMP
PresShell::GetRootFrame(nsIFrame** aResult) const
{
  return mFrameManager->GetRootFrame(aResult);
}

NS_IMETHODIMP
PresShell::GetPageSequenceFrame(nsIPageSequenceFrame** aResult) const
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIFrame*             rootFrame;
  nsIFrame*             child;
  nsIPageSequenceFrame* pageSequence = nsnull;

  // The page sequence frame is the child of the rootFrame
  mFrameManager->GetRootFrame(&rootFrame);
  rootFrame->FirstChild(mPresContext, nsnull, &child);

  if (nsnull != child) {

      // but the child could be wrapped in a scrollframe so lets check
      nsIScrollableFrame* scrollable = nsnull;
      nsresult rv = child->QueryInterface(NS_GET_IID(nsIScrollableFrame),
                                          (void **)&scrollable);
      if (NS_SUCCEEDED(rv) && (nsnull != scrollable)) {
          // if it is then get the scrolled frame
          scrollable->GetScrolledFrame(nsnull, child);
      } else {
        nsCOMPtr<nsIPrintPreviewContext> ppContext = do_QueryInterface(mPresContext);
        if (ppContext) {
          child->FirstChild(mPresContext, nsnull, &child);
        }
      }

      // make sure the child is a pageSequence
      rv = child->QueryInterface(NS_GET_IID(nsIPageSequenceFrame),
                                 (void**)&pageSequence);
      NS_ASSERTION(NS_SUCCEEDED(rv),"Error: Could not find pageSequence!");

      *aResult = pageSequence;
      return NS_OK;
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
#ifdef MOZ_PERF_METRICS
  // Reset style resolution stopwatch maintained by style set
  MOZ_TIMER_DEBUGLOG(("Reset: Style Resolution: PresShell::BeginLoad(), this=%p\n", (void*)this));
  CtlStyleWatch(kStyleWatchReset,mStyleSet);
#endif  
  mDocumentLoading = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
PresShell::EndLoad(nsIDocument *aDocument)
{

  // Restore frame state for the root scroll frame
  nsIFrame* rootFrame = nsnull;
  GetRootFrame(&rootFrame);
  nsCOMPtr<nsILayoutHistoryState> historyState = do_QueryReferent(mHistoryState);
  if (rootFrame && historyState) {
    nsIFrame* scrollFrame = nsnull;
    GetRootScrollFrame(mPresContext, rootFrame, &scrollFrame);
    if (scrollFrame) {
      mFrameManager->RestoreFrameStateFor(mPresContext, scrollFrame, historyState, nsIStatefulFrame::eDocumentScrollState);
    }
  }

#ifdef MOZ_PERF_METRICS
  // Dump reflow, style resolution and frame construction times here.
  MOZ_TIMER_DEBUGLOG(("Stop: Reflow: PresShell::EndLoad(), this=%p\n", this));
  MOZ_TIMER_STOP(mReflowWatch);
  MOZ_TIMER_LOG(("Reflow time (this=%p): ", this));
  MOZ_TIMER_PRINT(mReflowWatch);  

  MOZ_TIMER_DEBUGLOG(("Stop: Frame Creation: PresShell::EndLoad(), this=%p\n", this));
  MOZ_TIMER_STOP(mFrameCreationWatch);
  MOZ_TIMER_LOG(("Frame construction plus style resolution time (this=%p): ", this));
  MOZ_TIMER_PRINT(mFrameCreationWatch);

  // Print style resolution stopwatch maintained by style set
  MOZ_TIMER_DEBUGLOG(("Stop: Style Resolution: PresShell::EndLoad(), this=%p\n", this));
  CtlStyleWatch(kStyleWatchStop|kStyleWatchDisable, mStyleSet);
  MOZ_TIMER_LOG(("Style resolution time (this=%p): ", this));
  CtlStyleWatch(kStyleWatchPrint, mStyleSet);
#endif  
  mDocumentLoading = PR_FALSE;
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


// aReflowCommand is considered to be already in the queue if the
// frame it targets is targeted by a pre-existing reflow command in
// the queue.
PRBool
PresShell::AlreadyInQueue(nsHTMLReflowCommand* aReflowCommand,
                          nsVoidArray&         aQueue)
{
  PRInt32 i, n = aQueue.Count();
  nsIFrame* targetFrame;
  PRBool inQueue = PR_FALSE;    

  if (NS_SUCCEEDED(aReflowCommand->GetTarget(targetFrame))) {
    // Iterate over the reflow commands and compare the targeted frames.
    for (i = 0; i < n; i++) {
      nsHTMLReflowCommand* rc = (nsHTMLReflowCommand*) aQueue.ElementAt(i);
      if (rc) {
        nsIFrame* targetOfQueuedRC;
        if (NS_SUCCEEDED(rc->GetTarget(targetOfQueuedRC))) {
          nsReflowType RCType;
          nsReflowType queuedRCType;
          aReflowCommand->GetType(RCType);
          rc->GetType(queuedRCType);
          if (targetFrame == targetOfQueuedRC &&
            RCType == queuedRCType) {
            nsCOMPtr<nsIAtom> CLName, queuedCLName;
            aReflowCommand->GetChildListName(*getter_AddRefs(CLName));
            rc->GetChildListName(*getter_AddRefs(queuedCLName));
            if (CLName == queuedCLName) {
#ifdef DEBUG
              if (VERIFY_REFLOW_NOISY_RC & gVerifyReflowFlags) {
                printf("*** PresShell::AlreadyInQueue(): Discarding reflow command: this=%p\n", (void*)this);
                aReflowCommand->List(stdout);
              }
#endif
              inQueue = PR_TRUE;
              break;
            }
          } 
        }
      }
    }
  }

  return inQueue;
}

NS_IMETHODIMP
PresShell::AppendReflowCommand(nsHTMLReflowCommand* aReflowCommand)
{
  // If we've not yet done the initial reflow, then don't bother
  // enqueuing a reflow command yet.
  if (! mDidInitialReflow)
    return NS_OK;

#ifdef DEBUG
  //printf("gShellCounter: %d\n", gShellCounter++);
  if (mInVerifyReflow) {
    return NS_OK;
  }  
  if (VERIFY_REFLOW_NOISY_RC & gVerifyReflowFlags) {
    printf("\nPresShell@%p: adding reflow command\n", (void*)this);
    aReflowCommand->List(stdout);
    if (VERIFY_REFLOW_REALLY_NOISY_RC & gVerifyReflowFlags) {
      printf("Current content model:\n");
      nsCOMPtr<nsIContent> rootContent;
      mDocument->GetRootContent(getter_AddRefs(rootContent));
      if (rootContent) {
        rootContent->List(stdout, 0);
      }
    }
  }  
#endif

  // Add the reflow command to the queue
  nsresult rv = NS_OK;
  if (!AlreadyInQueue(aReflowCommand, mReflowCommands)) {
    rv = (mReflowCommands.AppendElement(aReflowCommand) ? NS_OK : NS_ERROR_OUT_OF_MEMORY);
    ReflowCommandAdded(aReflowCommand);
  }
  else {
    // We're not going to process this reflow command.
    delete aReflowCommand;
  }

  // For async reflow during doc load, post a reflow event if we are not batching reflow commands.
  // For sync reflow during doc load, post a reflow event if we are not batching reflow commands
  // and the document is not loading.
  if ((gAsyncReflowDuringDocLoad && !mBatchReflows) ||
      (!gAsyncReflowDuringDocLoad && !mBatchReflows && !mDocumentLoading)) {
    // If we're in the middle of a drag, process it right away (needed for mac,
    // might as well do it on all platforms just to keep the code paths the same).
    if ( !IsDragInProgress() )
      PostReflowEvent();
  }

  return rv;
}


//
// IsDragInProgress
//
// Ask the drag service if we're in the middle of a drag
//
PRBool
PresShell :: IsDragInProgress ( ) const
{
  PRBool dragInProgress = PR_FALSE;
  if ( mDragService ) {
    nsCOMPtr<nsIDragSession> session;
    mDragService->GetCurrentSession ( getter_AddRefs(session) );
    if ( session )
      dragInProgress = PR_TRUE;
  }
  
  return dragInProgress;

} // IsDragInProgress


NS_IMETHODIMP
PresShell::CancelReflowCommandInternal(nsIFrame*     aTargetFrame, 
                                       nsReflowType* aCmdType,
                                       PRBool        aProcessDummyLayoutRequest)
{
  PRInt32 i, n = mReflowCommands.Count();
  for (i = 0; i < n; i++) {
    nsHTMLReflowCommand* rc = (nsHTMLReflowCommand*) mReflowCommands.ElementAt(i);
    if (rc) {
      nsIFrame* target;      
      if (NS_SUCCEEDED(rc->GetTarget(target))) {
        if (target == aTargetFrame) {
          if (aCmdType != NULL) {
            // If aCmdType is specified, only remove reflow commands
            // of that type
            nsReflowType type;
            if (NS_SUCCEEDED(rc->GetType(type))) {
              if (type != *aCmdType)
                continue;
            }
          }
#ifdef DEBUG
          if (VERIFY_REFLOW_NOISY_RC & gVerifyReflowFlags) {
            printf("PresShell: removing rc=%p for frame ", (void*)rc);
            nsFrame::ListTag(stdout, aTargetFrame);
            printf("\n");
          }
#endif
          mReflowCommands.RemoveElementAt(i);
          ReflowCommandRemoved(rc);
          delete rc;
          n--;
          i--;
          continue;
        }
      }
    }
  }

  if (aProcessDummyLayoutRequest) {
    DoneRemovingReflowCommands();
  }

  return NS_OK;
}

NS_IMETHODIMP
PresShell::CancelReflowCommand(nsIFrame*     aTargetFrame, 
                               nsReflowType* aCmdType)
{
  return CancelReflowCommandInternal(aTargetFrame, aCmdType);
}


NS_IMETHODIMP
PresShell::CancelAllReflowCommands()
{
  PRInt32 n = mReflowCommands.Count();
  nsHTMLReflowCommand* rc;
  PRInt32 i;
  for (i = 0; i < n; i++) {
    rc = NS_STATIC_CAST(nsHTMLReflowCommand*, mReflowCommands.ElementAt(i));
    ReflowCommandRemoved(rc);
    delete rc;
  }
  NS_ASSERTION(n == mReflowCommands.Count(),"reflow command list changed during cancel!");
  mReflowCommands.Clear();

  DoneRemovingReflowCommands();

  return NS_OK;
}

NS_IMETHODIMP
PresShell::RecreateFramesFor(nsIContent* aContent)
{
  NS_ENSURE_TRUE(mPresContext, NS_ERROR_FAILURE);

  nsCOMPtr<nsIStyleSet> styleSet;
  nsresult rv = GetStyleSet(getter_AddRefs(styleSet));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStyleFrameConstruction> frameConstruction;
  rv = styleSet->GetStyleFrameConstruction(getter_AddRefs(frameConstruction));
  NS_ENSURE_SUCCESS(rv, rv);

  // Don't call RecreateFramesForContent since that is not exported and we want
  // to keep the number of entrypoints down.

  nsStyleChangeList changeList;
  changeList.AppendChange(nsnull, aContent, nsChangeHint_ReconstructFrame);
  return frameConstruction->ProcessRestyledFrames(changeList, mPresContext);
}

NS_IMETHODIMP
PresShell::ClearFrameRefs(nsIFrame* aFrame)
{
  nsIEventStateManager *manager;
  if (NS_OK == mPresContext->GetEventStateManager(&manager)) {
    manager->ClearFrameRefs(aFrame);
    NS_RELEASE(manager);
  }
  
  if (mCaret) {
  	mCaret->ClearFrameRefs(aFrame);
  }
  
  if (aFrame == mCurrentEventFrame) {
    aFrame->GetContent(&mCurrentEventContent);
    mCurrentEventFrame = nsnull;
  }

  for (int i=0; i<mCurrentEventFrameStack.Count(); i++) {
    if (aFrame == (nsIFrame*)mCurrentEventFrameStack.ElementAt(i)) {
      //One of our stack frames was deleted.  Get its content so that when we
      //pop it we can still get its new frame from its content
      nsIContent *currentEventContent;
      aFrame->GetContent(&currentEventContent);
      mCurrentEventContentStack.ReplaceElementAt((void*)currentEventContent, i);
      mCurrentEventFrameStack.ReplaceElementAt(nsnull, i);
    }
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

  nsIView   *view = nsnull;
  nsPoint   pt;
  nsresult  rv;

  aFrame->GetView(mPresContext, &view);

  if (nsnull == view)
    aFrame->GetOffsetFromView(mPresContext, pt, &view);

  nsCOMPtr<nsIWidget> widget;
  if (nsnull != view) {
    nsCOMPtr<nsIViewManager> vm;
    view->GetViewManager(*getter_AddRefs(vm));
    vm->GetWidgetForView(view, getter_AddRefs(widget));
  }

  nsCOMPtr<nsIDeviceContext> dx;

  nsIRenderingContext* result = nsnull;
  rv = mPresContext->GetDeviceContext(getter_AddRefs(dx));
  if (NS_SUCCEEDED(rv) && dx) {
    if (nsnull != widget) {
      rv = dx->CreateRenderingContext(widget.get(), result);
    }
    else {
      rv = dx->CreateRenderingContext(result);
    }
  }
  *aResult = result;

  return rv;
}

NS_IMETHODIMP
PresShell::CantRenderReplacedElement(nsIPresContext* aPresContext,
                                     nsIFrame*       aFrame)
{
  if (mFrameManager) {
    return mFrameManager->CantRenderReplacedElement(aPresContext, aFrame);
  }

  return NS_OK;
}

NS_IMETHODIMP
PresShell::GoToAnchor(const nsAString& aAnchorName)
{
  nsCOMPtr<nsIDOMDocument> doc = do_QueryInterface(mDocument);
  nsCOMPtr<nsIDOMHTMLDocument> htmlDoc = do_QueryInterface(mDocument);
  nsresult rv = NS_OK;
  nsCOMPtr<nsIContent> content;

  // Search for an element with a matching "id" attribute
  if (doc) {    
    nsCOMPtr<nsIDOMElement> element;
    rv = doc->GetElementById(aAnchorName, getter_AddRefs(element));
    if (NS_SUCCEEDED(rv) && element) {
      // Get the nsIContent interface, because that's what we need to
      // get the primary frame
      content = do_QueryInterface(element);
    }
  }

  // Search for an anchor element with a matching "name" attribute
  if (!content && htmlDoc) {
    nsCOMPtr<nsIDOMNodeList> list;
    // Find a matching list of named nodes
    rv = htmlDoc->GetElementsByName(aAnchorName, getter_AddRefs(list));
    if (NS_SUCCEEDED(rv) && list) {
      PRUint32 count;
      PRUint32 i;
      list->GetLength(&count);
      // Loop through the named nodes looking for the first anchor
      for (i = 0; i < count; i++) {
        nsCOMPtr<nsIDOMNode> node;
        rv = list->Item(i, getter_AddRefs(node));
        if (NS_FAILED(rv)) {
          break;
        }
        // Ensure it's an anchor element
        nsCOMPtr<nsIDOMElement> element = do_QueryInterface(node);
        nsAutoString tagName;
        if (element && NS_SUCCEEDED(element->GetTagName(tagName))) {
          ToLowerCase(tagName);
          if (tagName.Equals(NS_LITERAL_STRING("a"))) {
            content = do_QueryInterface(element);
            break;
          }
        }
      }
    }
  }

  // Search for anchor in the HTML namespace with a matching name
  if (!content && !htmlDoc)
  {
    nsCOMPtr<nsIDOMNodeList> list;
    NS_NAMED_LITERAL_STRING(nameSpace, "http://www.w3.org/1999/xhtml");
    // Get the list of anchor elements
    rv = doc->GetElementsByTagNameNS(nameSpace, NS_LITERAL_STRING("a"), getter_AddRefs(list));
    if (NS_SUCCEEDED(rv) && list) {
      PRUint32 count;
      PRUint32 i;
      list->GetLength(&count);
      // Loop through the named nodes looking for the first anchor
      for (i = 0; i < count; i++) {
        nsCOMPtr<nsIDOMNode> node;
        rv = list->Item(i, getter_AddRefs(node));
        if (NS_FAILED(rv)) {
          break;
        }
        // Compare the name attribute
        nsCOMPtr<nsIDOMElement> element = do_QueryInterface(node);
        nsAutoString value;
        if (element && NS_SUCCEEDED(element->GetAttribute(NS_LITERAL_STRING("name"), value))) {
          if (value.Equals(aAnchorName)) {
            content = do_QueryInterface(element);
            break;
          }
        }
      }
    }
  }

  // Finally try FIXptr
  nsCOMPtr<nsIDOMRange> jumpToRange;
  if (!content) {
    nsCOMPtr<nsIDOMXMLDocument> xmldoc = do_QueryInterface(mDocument);
    if (xmldoc) {
      xmldoc->EvaluateFIXptr(aAnchorName,getter_AddRefs(jumpToRange));
      if (jumpToRange) {
        nsCOMPtr<nsIDOMNode> node;
        jumpToRange->GetStartContainer(getter_AddRefs(node));
        if (node) {
          node->QueryInterface(NS_GET_IID(nsIContent),getter_AddRefs(content));
        }
      }
    }
  }
 
  if (content) {
    nsIFrame* frame = nsnull;

    // Get the primary frame
    if (NS_SUCCEEDED(GetPrimaryFrameFor(content, &frame)) && frame) {
      rv = ScrollFrameIntoView(frame, NS_PRESSHELL_SCROLL_TOP,
                               NS_PRESSHELL_SCROLL_ANYWHERE);

      if (NS_SUCCEEDED(rv)) {
        // Should we select the target?
        // This action is controlled by a preference: the default is to not select.
        PRBool selectAnchor = PR_FALSE;
        nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID,&rv));
        if (NS_SUCCEEDED(rv) && prefs) {
          prefs->GetBoolPref("layout.selectanchor",&selectAnchor);
        }
        // Even if select anchor pref is false, we must still move the caret there.
        // That way tabbing will start from the new location
        if (!jumpToRange) {
          jumpToRange = do_CreateInstance(kRangeCID);
          nsCOMPtr<nsIDOMNode> node(do_QueryInterface(content));
          if (jumpToRange && node) 
            jumpToRange->SelectNode(node);
        }
        if (jumpToRange) {
          if (!selectAnchor)
            jumpToRange->Collapse(PR_TRUE);
          SelectRange(jumpToRange);
        }
        
        nsCOMPtr<nsIEventStateManager> esm;
        if (NS_SUCCEEDED(mPresContext->GetEventStateManager(getter_AddRefs(esm))) && esm) {
          PRBool isSelectionWithFocus;
          esm->MoveFocusToCaret(PR_TRUE, &isSelectionWithFocus);
        }
      }
    }
  } else {
    rv = NS_ERROR_FAILURE; //changed to NS_OK in quirks mode if ScrollTo is called
    
    // Scroll to the top/left if the anchor can not be
    // found and it is labelled top (quirks mode only). @see bug 80784
    nsCompatibility compatMode;
    mPresContext->GetCompatibilityMode(&compatMode);
   
    if ((NS_LossyConvertUCS2toASCII(aAnchorName).EqualsIgnoreCase("top")) &&
        (compatMode == eCompatibility_NavQuirks) && 
        (mViewManager)) { 
      // Get the viewport scroller
      nsIScrollableView* scrollingView;
      mViewManager->GetRootScrollableView(&scrollingView);
      if (scrollingView) {
        // Scroll to the top of the page
        scrollingView->ScrollTo(0, 0, NS_VMREFRESH_IMMEDIATE);
        rv = NS_OK;
      }
    }
  }

  return rv;
}

nsresult
PresShell::SelectRange(nsIDOMRange *aRange)
{
  nsCOMPtr<nsISelection> sel;
  nsresult rv = GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(sel));
  if (NS_SUCCEEDED(rv) && sel) {
    sel->RemoveAllRanges();
    sel->AddRange(aRange);
  }

  return rv;
}

/**
 * This function takes a scrolling view, a rect, and a scroll position and
 * attempts to scroll that rect to that position in that view.  The rect
 * should be in the coordinate system of the _scrolled_ view.
 */
static void ScrollViewToShowRect(nsIScrollableView* aScrollingView,
                                 nsRect &           aRect,
                                 PRIntn             aVPercent,
                                 PRIntn             aHPercent)
{
  // Determine the visible rect in the scrolling view's coordinate space.
  // The size of the visible area is the clip view size
  const nsIView*  clipView;
  nsRect          visibleRect;

  aScrollingView->GetClipView(&clipView);
  clipView->GetBounds(visibleRect); // get width and height
  aScrollingView->GetScrollPosition(visibleRect.x, visibleRect.y);

  // The actual scroll offsets
  nscoord scrollOffsetX = visibleRect.x;
  nscoord scrollOffsetY = visibleRect.y;

  nscoord lineHeight;
  aScrollingView->GetLineHeight(&lineHeight);
  
  // See how the rect should be positioned vertically
  if (NS_PRESSHELL_SCROLL_ANYWHERE == aVPercent) {
    // The caller doesn't care where the frame is positioned vertically,
    // so long as it's fully visible
    if (aRect.y < visibleRect.y) {
      // Scroll up so the frame's top edge is visible
      scrollOffsetY = aRect.y;
    } else if (aRect.YMost() > visibleRect.YMost()) {
      // Scroll down so the frame's bottom edge is visible. Make sure the
      // frame's top edge is still visible
      scrollOffsetY += aRect.YMost() - visibleRect.YMost();
      if (scrollOffsetY > aRect.y) {
        scrollOffsetY = aRect.y;
      }
    }
  } else if (NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE == aVPercent) {
    // Scroll only if no part of the frame is visible in this view
    if (aRect.YMost() - lineHeight < visibleRect.y) {
      // Scroll up so the frame's top edge is visible
      scrollOffsetY = aRect.y;
    }  else if (aRect.y + lineHeight > visibleRect.YMost()) {
      // Scroll down so the frame's bottom edge is visible. Make sure the
      // frame's top edge is still visible
      scrollOffsetY += aRect.YMost() - visibleRect.YMost();
      if (scrollOffsetY > aRect.y) {
        scrollOffsetY = aRect.y;
      }
    }
  } else {
    // Align the frame edge according to the specified percentage
    nscoord frameAlignY =
      NSToCoordRound(aRect.y + aRect.height * (aVPercent / 100.0));
    scrollOffsetY =
      NSToCoordRound(frameAlignY - visibleRect.height * (aVPercent / 100.0));
  }

  // See how the frame should be positioned horizontally
  if (NS_PRESSHELL_SCROLL_ANYWHERE == aHPercent) {
    // The caller doesn't care where the frame is positioned horizontally,
    // so long as it's fully visible
    if (aRect.x < visibleRect.x) {
      // Scroll left so the frame's left edge is visible
      scrollOffsetX = aRect.x;
    } else if (aRect.XMost() > visibleRect.XMost()) {
      // Scroll right so the frame's right edge is visible. Make sure the
      // frame's left edge is still visible
      scrollOffsetX += aRect.XMost() - visibleRect.XMost();
      if (scrollOffsetX > aRect.x) {
        scrollOffsetX = aRect.x;
      }
    }
  } else if (NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE == aHPercent) {
    // Scroll only if no part of the frame is visible in this view
    // XXXbz using the line height here is odd, but there are no
    // natural dimensions to use here, really....
    if (aRect.XMost() - lineHeight < visibleRect.x) {
      // Scroll left so the frame's left edge is visible
      scrollOffsetX = aRect.x;
    }  else if (aRect.x + lineHeight > visibleRect.XMost()) {
      // Scroll right so the frame's right edge is visible. Make sure the
      // frame's left edge is still visible
      scrollOffsetX += aRect.XMost() - visibleRect.XMost();
      if (scrollOffsetX > aRect.x) {
        scrollOffsetX = aRect.x;
      }
    }
  } else {
    // Align the frame edge according to the specified percentage
    nscoord frameAlignX =
      NSToCoordRound(aRect.x + aRect.width * (aHPercent / 100.0));
    scrollOffsetX =
      NSToCoordRound(frameAlignX - visibleRect.width * (aHPercent / 100.0));
  }

  aScrollingView->ScrollTo(scrollOffsetX, scrollOffsetY,
                           NS_VMREFRESH_IMMEDIATE);
}

NS_IMETHODIMP
PresShell::ScrollFrameIntoView(nsIFrame *aFrame,
                               PRIntn   aVPercent, 
                               PRIntn   aHPercent) const
{
  if (!aFrame) {
    return NS_ERROR_NULL_POINTER;
  }

  // Before we scroll the frame into view, ask the command dispatcher
  // if we're resetting focus because a window just got an activate
  // event. If we are, we do not want to scroll the frame into view.
  // Example: The user clicks on an anchor, and then deactivates the 
  // window. When they reactivate the window, the expected behavior
  // is not for the anchor link to scroll back into view. That is what
  // this check is preventing.
  // XXX: The dependency on the command dispatcher needs to be fixed.
  nsCOMPtr<nsIContent> content;
  aFrame->GetContent(getter_AddRefs(content));
  if(content) {
    nsCOMPtr<nsIDocument> document;
    content->GetDocument(*getter_AddRefs(document));
    if(document){
      nsCOMPtr<nsIFocusController> focusController;
	    nsCOMPtr<nsIScriptGlobalObject> ourGlobal;
	    document->GetScriptGlobalObject(getter_AddRefs(ourGlobal));
      nsCOMPtr<nsPIDOMWindow> ourWindow = do_QueryInterface(ourGlobal);
      if(ourWindow) {
        ourWindow->GetRootFocusController(getter_AddRefs(focusController));
        if (focusController) {
          PRBool dontScroll;
          focusController->GetSuppressFocusScroll(&dontScroll);
          if(dontScroll)
            return NS_OK;
        }
      }
    }
  }

  // This is a two-step process.
  // Step 1: Find the bounds of the rect we want to scroll into view.  For
  //         example, for an inline frame we want to scroll in the whole line.
  // Step 2: Walk the views that are parents of the frame and scroll them
  //         appropriately.
  
  nsRect  frameBounds;
  aFrame->GetRect(frameBounds);
  nsPoint offset;
  nsIView* closestView;
  aFrame->GetOffsetFromView(mPresContext, offset, &closestView);
  frameBounds.MoveTo(offset);

  // If this is an inline frame, we need to change the top of the
  // bounds to include the whole line.
  nsCOMPtr<nsIAtom> frameType;
  nsIFrame *prevFrame = aFrame;
  nsIFrame *frame = aFrame;

  while (frame && (frame->GetFrameType(getter_AddRefs(frameType)),
                   frameType == nsLayoutAtoms::inlineFrame)) {
    prevFrame = frame;
    prevFrame->GetParent(&frame);
  }

  if (frame != aFrame &&
      frame &&
      frameType == nsLayoutAtoms::blockFrame) {
    // find the line containing aFrame and increase the top of |offset|.
    nsCOMPtr<nsILineIterator> lines( do_QueryInterface(frame) );

    if (lines) {
      PRInt32 index = -1;
      lines->FindLineContaining(prevFrame, &index);
      if (index >= 0) {
        nsIFrame *trash1;
        PRInt32 trash2;
        nsRect lineBounds;
        PRUint32 trash3;

        if (NS_SUCCEEDED(lines->GetLine(index, &trash1, &trash2,
                                        lineBounds, &trash3))) {
          nsPoint blockOffset;
          nsIView* blockView;
          frame->GetOffsetFromView(mPresContext, blockOffset, &blockView);

          if (blockView == closestView) {
            // XXX If views not equal, this is hard.  Do we want to bother?
            nscoord newoffset = lineBounds.y + blockOffset.y;

            if (newoffset < frameBounds.y)
              frameBounds.y = newoffset;
          }
        }
      }
    }
  }

#ifdef DEBUG
  if (closestView) {
    nsIScrollableView* _testView = nsnull;
    CallQueryInterface(closestView, &_testView);
    NS_ASSERTION(!_testView,
                 "What happened to the scrolled view?  "
                 "The frame should not be directly in the scrolling view!");
  }
#endif
  
  // Walk up the view hierarchy.  Make sure to add the view's position
  // _after_ we get the parent and see whether it's scrollable.  We want to
  // make sure to get the scrolled view's position after it has been scrolled.
  nsIScrollableView* scrollingView = nsnull;
  while (closestView) {
    nsIView* parent;
    closestView->GetParent(parent);
    if (parent) {
      CallQueryInterface(parent, &scrollingView);
      if (scrollingView) {
        ScrollViewToShowRect(scrollingView, frameBounds, aVPercent, aHPercent);
      }
    }
    nscoord x, y;
    closestView->GetPosition(&x, &y);
    frameBounds.MoveBy(x, y);
    closestView = parent;
  }

  return NS_OK;
}

// GetLinkLocation: copy link location to clipboard
NS_IMETHODIMP PresShell::GetLinkLocation(nsIDOMNode* aNode, nsAString& aLocationString)
{
#ifdef DEBUG_dr
  printf("dr :: PresShell::GetLinkLocation\n");
#endif

  NS_ENSURE_ARG_POINTER(aNode);
  nsresult rv;
  nsAutoString anchorText;
  static char strippedChars[] = {'\t','\r','\n'};

  // are we an anchor?
  nsCOMPtr<nsIDOMHTMLAnchorElement> anchor(do_QueryInterface(aNode));
  nsCOMPtr<nsIDOMHTMLAreaElement> area;
  nsCOMPtr<nsIDOMHTMLLinkElement> link;
  nsAutoString xlinkType;
  if (anchor) {
    rv = anchor->GetHref(anchorText);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // area?
    area = do_QueryInterface(aNode);
    if (area) {
      rv = area->GetHref(anchorText);
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      // link?
      link = do_QueryInterface(aNode);
      if (link) {
        rv = link->GetHref(anchorText);
        NS_ENSURE_SUCCESS(rv, rv);
      } else {
        // Xlink?
        nsCOMPtr<nsIDOMElement> element(do_QueryInterface(aNode));
        if (element) {
          NS_NAMED_LITERAL_STRING(xlinkNS,"http://www.w3.org/1999/xlink");
          element->GetAttributeNS(xlinkNS,NS_LITERAL_STRING("type"),xlinkType);
          if (xlinkType.Equals(NS_LITERAL_STRING("simple"))) {
            element->GetAttributeNS(xlinkNS,NS_LITERAL_STRING("href"),anchorText);
            if (!anchorText.IsEmpty()) {
              // Resolve the full URI using baseURI property

              nsAutoString base;
              nsCOMPtr<nsIDOM3Node> node(do_QueryInterface(aNode,&rv));
              NS_ENSURE_SUCCESS(rv, rv);
              node->GetBaseURI(base);

              nsCOMPtr<nsIIOService>
                ios(do_GetService("@mozilla.org/network/io-service;1", &rv));
              NS_ENSURE_SUCCESS(rv, rv);

              nsCOMPtr<nsIURI> baseURI;
              rv = ios->NewURI(NS_ConvertUCS2toUTF8(base),nsnull,nsnull,getter_AddRefs(baseURI));
              NS_ENSURE_SUCCESS(rv, rv);

              nsCAutoString spec;
              rv = baseURI->Resolve(NS_ConvertUCS2toUTF8(anchorText),spec);
              NS_ENSURE_SUCCESS(rv, rv);

              anchorText = NS_ConvertUTF8toUCS2(spec);
            }
          }
        }
      }
    }
  }

  if (anchor || area || link || xlinkType.Equals(NS_LITERAL_STRING("simple"))) {
    //Remove all the '\t', '\r' and '\n' from 'anchorText'
    anchorText.StripChars(strippedChars);

    aLocationString = anchorText;

    return NS_OK;
  }

  // if no link, fail.
  return NS_ERROR_FAILURE;
}

// GetImageLocation: copy image location to clipboard
NS_IMETHODIMP PresShell::GetImageLocation(nsIDOMNode* aNode, nsAString& aLocationString)
{
#ifdef DEBUG_dr
  printf("dr :: PresShell::GetImageLocation\n");
#endif

  NS_ENSURE_ARG_POINTER(aNode);
  nsresult rv;

  // are we an image?
  nsCOMPtr<nsIDOMHTMLImageElement> img(do_QueryInterface(aNode, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  return img->GetSrc(aLocationString);
}

// DoCopyImageContents: copy image contents to clipboard
NS_IMETHODIMP PresShell::DoCopyImageContents(nsIDOMNode* aNode)
{
  // XXX dr: platform-specific widget code works on windows and mac.
  // when linux copy image contents works, this should get written
  // and hooked up to the front end, similarly to cmd_copyImageLocation.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
PresShell::DoCopy()
{
  nsCOMPtr<nsIDocument> doc;
  GetDocument(getter_AddRefs(doc));
  if (!doc) return NS_ERROR_FAILURE;

  nsresult rv;
  nsCOMPtr<nsISelection> sel;
  nsCOMPtr<nsIEventStateManager> manager;
  nsCOMPtr<nsIContent> content;
  rv = mPresContext->GetEventStateManager(getter_AddRefs(manager));
  if (NS_FAILED(rv)) 
    return rv;
  if (!manager) 
    return NS_ERROR_FAILURE;

  rv = manager->GetFocusedContent(getter_AddRefs(content));
  if (NS_SUCCEEDED(rv) && content)
  {
    //check to see if we need to get selection from frame
    //optimization that MAY need to be expanded as more things implement their own "selection"
    nsCOMPtr<nsIDOMNSHTMLInputElement>    htmlInputElement(do_QueryInterface(content));
    nsCOMPtr<nsIDOMNSHTMLTextAreaElement> htmlTextAreaElement(do_QueryInterface(content));
    if (htmlInputElement || htmlTextAreaElement)
    {
      nsIFrame *htmlInputFrame;
      rv = GetPrimaryFrameFor(content, &htmlInputFrame);
      if (NS_FAILED(rv)) 
        return rv;
      if (!htmlInputFrame) 
        return NS_ERROR_FAILURE;

      nsCOMPtr<nsISelectionController> selCon;
      rv = htmlInputFrame->GetSelectionController(mPresContext,getter_AddRefs(selCon));
      if (NS_FAILED(rv)) 
        return rv;
      if (!selCon) 
        return NS_ERROR_FAILURE;

      rv = selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(sel));
    }
  }
  if (!sel) //get selection from this PresShell
    rv = GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(sel));
    
  if (NS_FAILED(rv)) 
    return rv;
  if (!sel) 
    return NS_ERROR_FAILURE;

  // Now we have the selection.  Make sure it's nonzero:
  PRBool isCollapsed;
  sel->GetIsCollapsed(&isCollapsed);
  if (isCollapsed)
    return NS_OK;

  // call the copy code
  rv = nsCopySupport::HTMLCopy(sel, doc, nsIClipboard::kGlobalClipboard);
  if (NS_FAILED(rv))
    return rv;

  // Now that we have copied, update the Paste menu item
  nsCOMPtr<nsIScriptGlobalObject> globalObject;
  doc->GetScriptGlobalObject(getter_AddRefs(globalObject));  
  nsCOMPtr<nsIDOMWindowInternal> domWindow = do_QueryInterface(globalObject);
  if (domWindow)
  {
    domWindow->UpdateCommands(NS_LITERAL_STRING("clipboard"));
  }
  
  return NS_OK;
}

NS_IMETHODIMP
PresShell::CaptureHistoryState(nsILayoutHistoryState** aState, PRBool aLeavingPage)
{
  nsresult rv = NS_OK;

  NS_PRECONDITION(nsnull != aState, "null state pointer");

  nsCOMPtr<nsILayoutHistoryState> historyState = do_QueryReferent(mHistoryState);
  if (!historyState) {
    // Create the document state object
    rv = NS_NewLayoutHistoryState(getter_AddRefs(historyState));
  
    if (NS_FAILED(rv)) { 
      *aState = nsnull;
      return rv;
    }    

    mHistoryState = getter_AddRefs(NS_GetWeakReference(historyState));
  }

  *aState = historyState;
  NS_IF_ADDREF(*aState);
  
  // Capture frame state for the entire frame hierarchy
  nsIFrame* rootFrame = nsnull;
  rv = GetRootFrame(&rootFrame);
  if (NS_FAILED(rv) || nsnull == rootFrame) return rv;
  // Capture frame state for the root scroll frame
  // Don't capture state when first creating doc element heirarchy
  // As the scroll position is 0 and this will cause us to loose
  // our previously saved place!
  if (aLeavingPage) {
    nsIFrame* scrollFrame = nsnull;
    rv = GetRootScrollFrame(mPresContext, rootFrame, &scrollFrame);
    if (scrollFrame) {
      rv = mFrameManager->CaptureFrameStateFor(mPresContext, scrollFrame, historyState, nsIStatefulFrame::eDocumentScrollState);
    }
  }


  rv = mFrameManager->CaptureFrameState(mPresContext, rootFrame, historyState);  
 
  return rv;
}

NS_IMETHODIMP
PresShell::GetHistoryState(nsILayoutHistoryState** aState)
{
  nsCOMPtr<nsILayoutHistoryState> historyState = do_QueryReferent(mHistoryState);
  *aState = historyState;
  NS_IF_ADDREF(*aState);
  return NS_OK;
}

NS_IMETHODIMP
PresShell::SetHistoryState(nsILayoutHistoryState* aLayoutHistoryState)
{
  mHistoryState = getter_AddRefs(NS_GetWeakReference(aLayoutHistoryState));
  return NS_OK;
}
  
static PRBool
IsGeneratedContentFrame(nsIFrame* aFrame)
{
  nsFrameState  frameState;

  aFrame->GetFrameState(&frameState);
  return (frameState & NS_FRAME_GENERATED_CONTENT) != 0;
}

static PRBool
IsPseudoFrame(nsIFrame* aFrame, nsIContent* aParentContent)
{
  nsCOMPtr<nsIContent>  content;

  aFrame->GetContent(getter_AddRefs(content));
  return content.get() == aParentContent;
}

static nsIFrame*
GetFirstChildFrame(nsIPresContext* aPresContext,
                   nsIFrame*       aFrame,
                   nsIContent*     aContent)
{
  nsIFrame* childFrame;

  // Get the first child frame
  aFrame->FirstChild(aPresContext, nsnull, &childFrame);

  // If the child frame is a pseudo-frame, then return its first child.
  // Note that the frame we create for the generated content is also a
  // pseudo-frame and so don't drill down in that case
  if (childFrame && IsPseudoFrame(childFrame, aContent) &&
      !IsGeneratedContentFrame(childFrame)) {
    return GetFirstChildFrame(aPresContext, childFrame, aContent);
  }

  return childFrame;
}

static nsIFrame*
GetLastChildFrame(nsIPresContext* aPresContext,
                  nsIFrame*       aFrame,
                  nsIContent*     aContent)
{
  NS_PRECONDITION(aFrame, "NULL frame pointer");

  // Get the last in flow frame
  nsIFrame* lastInFlow;
  do {
    lastInFlow = aFrame;
    lastInFlow->GetNextInFlow(&aFrame);
  } while (aFrame);

  // Get the last child frame
  nsIFrame* firstChildFrame;
  lastInFlow->FirstChild(aPresContext, nsnull, &firstChildFrame);
  if (firstChildFrame) {
    nsFrameList frameList(firstChildFrame);
    nsIFrame*   lastChildFrame = frameList.LastChild();

    NS_ASSERTION(lastChildFrame, "unexpected error");

    // Get the frame's first-in-flow. This matters in case the frame has
    // been continuted across multiple lines
    while (PR_TRUE) {
      nsIFrame* prevInFlow;
      lastChildFrame->GetPrevInFlow(&prevInFlow);
      if (prevInFlow) {
        lastChildFrame = prevInFlow;
      } else {
        break;
      }
    }

    // If the last child frame is a pseudo-frame, then return its last child.
    // Note that the frame we create for the generated content is also a
    // pseudo-frame and so don't drill down in that case
    if (lastChildFrame && IsPseudoFrame(lastChildFrame, aContent) &&
        !IsGeneratedContentFrame(lastChildFrame)) {
      return GetLastChildFrame(aPresContext, lastChildFrame, aContent);
    }

    return lastChildFrame;
  }

  return nsnull;
}

NS_IMETHODIMP
PresShell::GetGeneratedContentIterator(nsIContent*          aContent,
                                       GeneratedContentType aType,
                                       nsIContentIterator** aIterator) const
{
  nsIFrame* primaryFrame;
  nsresult  rv = NS_OK;

  // Initialize OUT parameter
  *aIterator = nsnull;

  // Get the primary frame associated with the content object
  GetPrimaryFrameFor(aContent, &primaryFrame);
  if (primaryFrame) {
    // See whether it's a request for the before or after generated content
    if (Before == aType) {
      // The most efficient thing to do is to get the first child frame,
      // and see if it is associated with generated content
      nsIFrame* firstChildFrame = GetFirstChildFrame(mPresContext, primaryFrame, aContent);
      if (firstChildFrame && IsGeneratedContentFrame(firstChildFrame)) {
        // Create an iterator
        rv = NS_NewFrameContentIterator(mPresContext, firstChildFrame, aIterator);
      }
      
    } else {
      // Avoid finding the last child frame unless we need to. Instead probe
      // for the existence of the pseudo-element
      nsCOMPtr<nsIStyleContext> styleContext;
      nsCOMPtr<nsIStyleContext> pseudoStyleContext;
      
      primaryFrame->GetStyleContext(getter_AddRefs(styleContext));
      mPresContext->ProbePseudoStyleContextFor(aContent,
                                               nsCSSPseudoElements::after,
                                               styleContext,
                                               getter_AddRefs(pseudoStyleContext));
      if (pseudoStyleContext) {
        nsIFrame* lastChildFrame = GetLastChildFrame(mPresContext, primaryFrame, aContent);
        if (lastChildFrame)
        { // it is now legal for GetLastChildFrame to return null.  see bug 52307 (a regression from bug 18754)
          // in the case of a null child frame, we treat the frame as having no "after" style
          // the "before" handler above already does this check
          NS_ASSERTION(IsGeneratedContentFrame(lastChildFrame),
                       "can't find generated content frame");
          // Create an iterator
          rv = NS_NewFrameContentIterator(mPresContext, lastChildFrame, aIterator);
        }
      }
    }
  }

  return rv;
}


NS_IMETHODIMP
PresShell::SetAnonymousContentFor(nsIContent* aContent, nsISupportsArray* aAnonymousElements)
{
  NS_PRECONDITION(aContent != nsnull, "null ptr");
  if (! aContent)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(aAnonymousElements != nsnull, "null ptr");
  if (! aAnonymousElements)
    return NS_ERROR_NULL_POINTER;

  if (! mAnonymousContentTable) {
    mAnonymousContentTable = new nsSupportsHashtable;
    if (! mAnonymousContentTable)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  nsISupportsKey key(aContent);

  nsCOMPtr<nsISupportsArray> oldAnonymousElements =
    getter_AddRefs(NS_STATIC_CAST(nsISupportsArray*, mAnonymousContentTable->Get(&key)));

  if (oldAnonymousElements) {
    // If we're trying to set anonymous content for an element that
    // already had anonymous content, then we need to be sure to clean
    // up after the old content. (This can happen, for example, when a
    // reframe occurs.)
    PRUint32 count;
    oldAnonymousElements->Count(&count);

    while (PRInt32(--count) >= 0) {
      nsCOMPtr<nsISupports> isupports( getter_AddRefs(oldAnonymousElements->ElementAt(count)) );
      nsCOMPtr<nsIContent> content( do_QueryInterface(isupports) );
      NS_ASSERTION(content != nsnull, "not an nsIContent");
      if (! content)
        continue;

      content->SetDocument(nsnull, PR_TRUE, PR_TRUE);
    }
  }

  mAnonymousContentTable->Put(&key, aAnonymousElements);
  return NS_OK;
}


NS_IMETHODIMP
PresShell::GetAnonymousContentFor(nsIContent* aContent, nsISupportsArray** aAnonymousElements)
{
  if (! mAnonymousContentTable) {
    *aAnonymousElements = nsnull;
    return NS_OK;
  }

  nsISupportsKey key(aContent);
  *aAnonymousElements =
    NS_REINTERPRET_CAST(nsISupportsArray*, mAnonymousContentTable->Get(&key)); // addrefs

  return NS_OK;
}


static PRBool PR_CALLBACK
ClearDocumentEnumerator(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsISupportsArray* anonymousElements =
    NS_STATIC_CAST(nsISupportsArray*, aData);

  PRUint32 count;
  anonymousElements->Count(&count);
  while (PRInt32(--count) >= 0) {
    nsCOMPtr<nsISupports> isupports( getter_AddRefs(anonymousElements->ElementAt(count)) );
    nsCOMPtr<nsIContent> content( do_QueryInterface(isupports) );
    NS_ASSERTION(content != nsnull, "not an nsIContent");
    if (! content)
      continue;

    content->SetDocument(nsnull, PR_TRUE, PR_TRUE);
  }

  return PR_TRUE;
}


NS_IMETHODIMP
PresShell::ReleaseAnonymousContent()
{
  if (mAnonymousContentTable) {
    mAnonymousContentTable->Enumerate(ClearDocumentEnumerator);
    delete mAnonymousContentTable;
    mAnonymousContentTable = nsnull;
  }
  return NS_OK;
}

NS_IMETHODIMP
PresShell::IsPaintingSuppressed(PRBool* aResult)
{
  *aResult = mPaintingSuppressed;
  return NS_OK;
}

void
PresShell::UnsuppressAndInvalidate()
{
  nsCOMPtr<nsIScriptGlobalObject> globalObject;
  mDocument->GetScriptGlobalObject(getter_AddRefs(globalObject));  
  nsCOMPtr<nsPIDOMWindow> ourWindow = do_QueryInterface(globalObject);
  nsCOMPtr<nsIFocusController> focusController;
  if (ourWindow)
    ourWindow->GetRootFocusController(getter_AddRefs(focusController));
  if (focusController)
    // Suppress focus.  The act of tearing down the old content viewer
    // causes us to blur incorrectly.
    focusController->SetSuppressFocus(PR_TRUE, "PresShell suppression on Web page loads");

  nsCOMPtr<nsISupports> container;
  mPresContext->GetContainer(getter_AddRefs(container));
  if (container) {
    nsCOMPtr<nsIDocShell> cvc(do_QueryInterface(container));
    if (cvc) {
      nsCOMPtr<nsIContentViewer> cv;
      cvc->GetContentViewer(getter_AddRefs(cv));
      if (cv) {
        nsCOMPtr<nsIPresShell> kungFuDeathGrip(this);
        cv->Show();
        // Calling |Show| may destroy us.  Not sure why yet, but it's
        // a smoketest blocker.
        if (mIsDestroying)
          return;
      }
    }
  }

  NS_ASSERTION(mFrameManager, "frameManager is already gone");
  if (mFrameManager) {
    mPaintingSuppressed = PR_FALSE;
    nsIFrame* rootFrame;
    mFrameManager->GetRootFrame(&rootFrame);
    if (rootFrame) {
      nsRect rect;
      rootFrame->GetRect(rect);
      if (!rect.IsEmpty()) {
        ((nsFrame*)rootFrame)->Invalidate(mPresContext, rect, PR_FALSE);
      }
    }
  }

  if (ourWindow)
    CheckForFocus(ourWindow, focusController, mDocument);

  if (focusController) // Unsuppress now that we've shown the new window and focused it.
    focusController->SetSuppressFocus(PR_FALSE, "PresShell suppression on Web page loads");
}

NS_IMETHODIMP
PresShell::UnsuppressPainting()
{
  if (mPaintSuppressionTimer) {
    mPaintSuppressionTimer->Cancel();
    mPaintSuppressionTimer = nsnull;
  }

  if (mIsDocumentGone || !mPaintingSuppressed)
    return NS_OK;

  // If we have reflows pending, just wait until we process
  // the reflows and get all the frames where we want them
  // before actually unlocking the painting.  Otherwise
  // go ahead and unlock now.
  if (mReflowCommands.Count() > 0)
    mShouldUnsuppressPainting = PR_TRUE;
  else
    UnsuppressAndInvalidate();
  return NS_OK;
}

NS_IMETHODIMP
PresShell::DisableThemeSupport()
{
  // Doesn't have to be dynamic.  Just set the bool.
  mIsThemeSupportDisabled = PR_TRUE;
  return NS_OK;
}

PRBool 
PresShell::IsThemeSupportEnabled()
{
  return !mIsThemeSupportDisabled;
}

// Post a request to handle an arbitrary callback after reflow has finished.
NS_IMETHODIMP
PresShell::PostReflowCallback(nsIReflowCallback* aCallback)
{
  nsCallbackEventRequest* request = nsnull;
  void* result = nsnull;
  AllocateFrame(sizeof(nsCallbackEventRequest), &result);
  request = (nsCallbackEventRequest*)result;

  request->callback = aCallback;
  NS_ADDREF(aCallback);
  request->next = nsnull;

  if (mLastCallbackEventRequest) {
    mLastCallbackEventRequest = mLastCallbackEventRequest->next = request;
  } else {
    mFirstCallbackEventRequest = request;
    mLastCallbackEventRequest = request;
  }
 
  return NS_OK;
}

NS_IMETHODIMP
PresShell::CancelReflowCallback(nsIReflowCallback* aCallback)
{
   nsCallbackEventRequest* before = nsnull;
   nsCallbackEventRequest* node = mFirstCallbackEventRequest;
   while(node)
   {
      nsIReflowCallback* callback = node->callback;

      if (callback == aCallback) 
      {
        nsCallbackEventRequest* toFree = node;
        if (node == mFirstCallbackEventRequest) {
           mFirstCallbackEventRequest = node->next;
           node = mFirstCallbackEventRequest;
           before = nsnull;
        } else {
           node = node->next;
           before->next = node;
        }

        FreeFrame(sizeof(nsCallbackEventRequest), toFree);
        NS_RELEASE(callback);
      } else {
        before = node;
        node = node->next;
      }
   }

   return NS_OK;
}

/**
* Post a request to handle a DOM event after Reflow has finished.
* The event must have been created with the "new" operator.
*/
NS_IMETHODIMP
PresShell::PostDOMEvent(nsIContent* aContent, nsEvent* aEvent)
{
 // ok we have a list of events to handle. Queue them up and handle them
 // after we finish reflow.
  nsDOMEventRequest* request = nsnull;
  void* result = nsnull;
  AllocateFrame(sizeof(nsDOMEventRequest), &result);
  request = (nsDOMEventRequest*)result;

  request->content = aContent;
  NS_ADDREF(aContent);

  request->event = aEvent;
  request->next = nsnull;

  if (mLastDOMEventRequest) {
    mLastDOMEventRequest = mLastDOMEventRequest->next = request;
  } else {
    mFirstDOMEventRequest = request;
    mLastDOMEventRequest = request;
  }
 
  return NS_OK;
}


/**
 * Post a request to set and attribute after reflow has finished.
 */
NS_IMETHODIMP
PresShell::PostAttributeChange(nsIContent* aContent,
                               PRInt32 aNameSpaceID, 
                               nsIAtom* aName,
                               const nsString& aValue,
                               PRBool aNotify,
                               nsAttributeChangeType aType)
{
 // ok we have a list of events to handle. Queue them up and handle them
 // after we finish reflow.
  nsAttributeChangeRequest* request = nsnull;

  void* result = nsnull;
  AllocateFrame(sizeof(nsAttributeChangeRequest), &result);
  request = (nsAttributeChangeRequest*)result;

  request->content = aContent;
  NS_ADDREF(aContent);

  request->nameSpaceID = aNameSpaceID;
  request->name = aName;
  request->value = aValue;
  request->notify = aNotify;
  request->type = aType;
  request->next = nsnull;

  if (mLastAttributeRequest) {
    mLastAttributeRequest = mLastAttributeRequest->next = request;
  } else {
    mFirstAttributeRequest = request;
    mLastAttributeRequest = request;
  }

  return NS_OK;
}


void
PresShell::HandlePostedReflowCallbacks()
{
   PRBool shouldFlush = PR_FALSE;
   nsCallbackEventRequest* node = mFirstCallbackEventRequest;
   while(node)
   {
      nsIReflowCallback* callback = node->callback;
      nsCallbackEventRequest* toFree = node;
      node = node->next;
      mFirstCallbackEventRequest = node;
      FreeFrame(sizeof(nsCallbackEventRequest), toFree);
	  if (callback)
        callback->ReflowFinished(this, &shouldFlush);
      NS_IF_RELEASE(callback);
   }

   mFirstCallbackEventRequest = mLastCallbackEventRequest = nsnull;

   if (shouldFlush)
     FlushPendingNotifications(PR_FALSE);
}

void
PresShell::HandlePostedDOMEvents()
{
   while(mFirstDOMEventRequest)
   {
      /* pull the node from the event request list. Be prepared for reentrant access to the list
         from within HandleDOMEvent and its callees! */
      nsDOMEventRequest* node = mFirstDOMEventRequest;
      nsEventStatus status = nsEventStatus_eIgnore;

      mFirstDOMEventRequest = node->next;
      if (nsnull == mFirstDOMEventRequest) {
        mLastDOMEventRequest = nsnull;
      }

      node->content->HandleDOMEvent(mPresContext, node->event, nsnull, NS_EVENT_FLAG_INIT, &status);
      NS_RELEASE(node->content);
      delete node->event;
      node->nsDOMEventRequest::~nsDOMEventRequest(); // doesn't do anything, but just in case
      FreeFrame(sizeof(nsDOMEventRequest), node);
   }
}

void
PresShell::HandlePostedAttributeChanges()
{
   while(mFirstAttributeRequest)
   {
      /* pull the node from the request list. Be prepared for reentrant access to the list
         from within SetAttribute/UnsetAttribute and its callees! */
      nsAttributeChangeRequest* node = mFirstAttributeRequest;

      mFirstAttributeRequest = node->next;
      if (nsnull == mFirstAttributeRequest) {
        mLastAttributeRequest = nsnull;
      }

      if (node->type == eChangeType_Set)
         node->content->SetAttr(node->nameSpaceID, node->name, node->value, node->notify);   
      else
         node->content->UnsetAttr(node->nameSpaceID, node->name, node->notify);   

      NS_RELEASE(node->content);
      node->nsAttributeChangeRequest::~nsAttributeChangeRequest();
      FreeFrame(sizeof(nsAttributeChangeRequest), node);
   }
}

NS_IMETHODIMP 
PresShell::IsSafeToFlush(PRBool& aIsSafeToFlush)
{
  aIsSafeToFlush = PR_TRUE;

  if (mIsReflowing) {
    // Not safe if we are reflowing
    aIsSafeToFlush = PR_FALSE;
  } else {
    // Not safe if we are painting
    nsCOMPtr<nsIViewManager> viewManager;
    nsresult rv = GetViewManager(getter_AddRefs(viewManager));
    if (NS_SUCCEEDED(rv) && (nsnull != viewManager)) {
      PRBool isPainting = PR_FALSE;
      viewManager->IsPainting(isPainting);
      if (isPainting) {
        aIsSafeToFlush = PR_FALSE;
      }
    }
  }
  return NS_OK;
}


NS_IMETHODIMP 
PresShell::FlushPendingNotifications(PRBool aUpdateViews)
{
  PRBool isSafeToFlush;
  IsSafeToFlush(isSafeToFlush);

  if (isSafeToFlush) {
    if (aUpdateViews && mViewManager) {
      mViewManager->BeginUpdateViewBatch();
    }

    ProcessReflowCommands(PR_FALSE);

    if (aUpdateViews && mViewManager) {
      mViewManager->EndUpdateViewBatch(NS_VMREFRESH_IMMEDIATE);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
PresShell::IsReflowLocked(PRBool* aIsReflowLocked) 
{
  *aIsReflowLocked = mIsReflowing;
  return NS_OK;
}

NS_IMETHODIMP 
PresShell::BeginReflowBatching()
{  
  mBatchReflows = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP 
PresShell::EndReflowBatching(PRBool aFlushPendingReflows)
{  
  nsresult rv = NS_OK;
  mBatchReflows = PR_FALSE;
  if (aFlushPendingReflows) {
    rv = FlushPendingNotifications(PR_FALSE);
  }
  else {
    PostReflowEvent();
  }
  return rv;
}


NS_IMETHODIMP
PresShell::GetReflowBatchingStatus(PRBool* aIsBatching)
{
  *aIsBatching = mBatchReflows;
  return NS_OK;
}

NS_IMETHODIMP
PresShell::ContentChanged(nsIDocument *aDocument,
                          nsIContent*  aContent,
                          nsISupports* aSubContent)
{
  WillCauseReflow();
  nsresult rv = mStyleSet->ContentChanged(mPresContext, aContent, aSubContent);
  VERIFY_STYLE_TREE;
  DidCauseReflow();

  return rv;
}

NS_IMETHODIMP
PresShell::ContentStatesChanged(nsIDocument* aDocument,
                                nsIContent* aContent1,
                                nsIContent* aContent2,
                                PRInt32 aStateMask)
{
  WillCauseReflow();
  nsresult rv = mStyleSet->ContentStatesChanged(mPresContext, aContent1,
                                                aContent2, aStateMask);
  VERIFY_STYLE_TREE;
  DidCauseReflow();

  return rv;
}


NS_IMETHODIMP
PresShell::AttributeChanged(nsIDocument *aDocument,
                            nsIContent*  aContent,
                            PRInt32      aNameSpaceID,
                            nsIAtom*     aAttribute,
                            PRInt32      aModType, 
                            nsChangeHint aHint)
{
  nsresult rv = NS_OK;
  // XXXwaterson it might be more elegant to wait until after the
  // initial reflow to begin observing the document. That would
  // squelch any other inappropriate notifications as well.
  if (mDidInitialReflow) {
    WillCauseReflow();
    rv = mStyleSet->AttributeChanged(mPresContext, aContent, aNameSpaceID, aAttribute, aModType, aHint);
    VERIFY_STYLE_TREE;
    DidCauseReflow();
  }
  return rv;
}

NS_IMETHODIMP
PresShell::ContentAppended(nsIDocument *aDocument,
                           nsIContent* aContainer,
                           PRInt32     aNewIndexInContainer)
{
  WillCauseReflow();
  MOZ_TIMER_DEBUGLOG(("Start: Frame Creation: PresShell::ContentAppended(), this=%p\n", this));
  MOZ_TIMER_START(mFrameCreationWatch);
  CtlStyleWatch(kStyleWatchEnable,mStyleSet);

  nsresult  rv = mStyleSet->ContentAppended(mPresContext, aContainer, aNewIndexInContainer);
  VERIFY_STYLE_TREE;

  MOZ_TIMER_DEBUGLOG(("Stop: Frame Creation: PresShell::ContentAppended(), this=%p\n", this));
  MOZ_TIMER_STOP(mFrameCreationWatch);
  CtlStyleWatch(kStyleWatchDisable,mStyleSet);
  DidCauseReflow();
  return rv;
}

NS_IMETHODIMP
PresShell::ContentInserted(nsIDocument* aDocument,
                           nsIContent*  aContainer,
                           nsIContent*  aChild,
                           PRInt32      aIndexInContainer)
{
  WillCauseReflow();
  nsresult  rv = mStyleSet->ContentInserted(mPresContext, aContainer, aChild, aIndexInContainer);
  VERIFY_STYLE_TREE;
  DidCauseReflow();
  return rv;
}

NS_IMETHODIMP
PresShell::ContentReplaced(nsIDocument* aDocument,
                           nsIContent*  aContainer,
                           nsIContent*  aOldChild,
                           nsIContent*  aNewChild,
                           PRInt32      aIndexInContainer)
{
  // Notify the ESM that the content has been removed, so that
  // it can clean up any state related to the content.
  nsCOMPtr<nsIEventStateManager> esm;
  mPresContext->GetEventStateManager(getter_AddRefs(esm));
  if (esm)
    esm->ContentRemoved(aOldChild);

  WillCauseReflow();
  nsresult  rv = mStyleSet->ContentReplaced(mPresContext, aContainer, aOldChild,
                                            aNewChild, aIndexInContainer);
  VERIFY_STYLE_TREE;
  DidCauseReflow();
  return rv;
}

NS_IMETHODIMP
PresShell::ContentRemoved(nsIDocument *aDocument,
                          nsIContent* aContainer,
                          nsIContent* aChild,
                          PRInt32     aIndexInContainer)
{
  // Notify the ESM that the content has been removed, so that
  // it can clean up any state related to the content.
  nsCOMPtr<nsIEventStateManager> esm;
  mPresContext->GetEventStateManager(getter_AddRefs(esm));
  if (esm)
    esm->ContentRemoved(aChild);

  WillCauseReflow();
  nsresult  rv = mStyleSet->ContentRemoved(mPresContext, aContainer,
                                           aChild, aIndexInContainer);

  // If we have no root content node at this point, be sure to reset
  // mDidInitialReflow to PR_FALSE, this will allow InitialReflow()
  // to be called again should a new root node be inserted for this
  // presShell. (Bug 167355)

  if (mDocument) {
    nsCOMPtr<nsIContent> rootContent;
    mDocument->GetRootContent(getter_AddRefs(rootContent));

    if (!rootContent) {
      mDidInitialReflow = PR_FALSE;
    }
  }

  VERIFY_STYLE_TREE;
  DidCauseReflow();
  return rv;
}

nsresult
PresShell::ReconstructFrames(void)
{
  nsresult rv = NS_OK;
          
  WillCauseReflow();
  rv = mStyleSet->ReconstructDocElementHierarchy(mPresContext);
  VERIFY_STYLE_TREE;
  DidCauseReflow();

  return rv;
}

/*
 * It's better to add stuff to the |DidSetStyleContext| method of the
 * relevant frames than adding it here.  These methods should (ideally,
 * anyway) go away.
 */

// Return value says whether to walk children.
typedef PRBool (* PR_CALLBACK frameWalkerFn)(nsIFrame *aFrame, void *aClosure);
   
PR_STATIC_CALLBACK(PRBool)
BuildFramechangeList(nsIFrame *aFrame, void *aClosure)
{
  nsStyleChangeList *changeList = NS_STATIC_CAST(nsStyleChangeList*, aClosure);

  // Ok, get our binding information.
  const nsStyleDisplay* oldDisplay;
  aFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)oldDisplay);
  if (!oldDisplay->mBinding.IsEmpty()) {
    // We had a binding.
    nsCOMPtr<nsIContent> content;
    aFrame->GetContent(getter_AddRefs(content));
    nsCOMPtr<nsIDocument> doc;
    content->GetDocument(*getter_AddRefs(doc));
    if (doc) {
      nsCOMPtr<nsIBindingManager> bm;
      doc->GetBindingManager(getter_AddRefs(bm));
      nsCOMPtr<nsIXBLBinding> binding;
      bm->GetBinding(content, getter_AddRefs(binding));
      PRBool marked = PR_FALSE;
      binding->MarkedForDeath(&marked);
      if (marked) {
        // Add in a change to process, thus ensuring this binding gets rebuilt.
        changeList->AppendChange(aFrame, content, NS_STYLE_HINT_FRAMECHANGE);
        return PR_FALSE;
      }
    }
  }

  return PR_TRUE;
}

PR_STATIC_CALLBACK(PRBool)
ReResolveMenusAndTrees(nsIFrame *aFrame, void *aClosure)
{
  // Trees have a special style cache that needs to be flushed when
  // the theme changes.
  nsCOMPtr<nsITreeBoxObject> treeBox(do_QueryInterface(aFrame));
  if (treeBox)
    treeBox->ClearStyleAndImageCaches();

  // We deliberately don't re-resolve style on a menu's popup
  // sub-content, since doing so slows menus to a crawl.  That means we
  // have to special-case them on a skin switch, and ensure that the
  // popup frames just get destroyed completely.
  nsCOMPtr<nsIMenuFrame> menuFrame(do_QueryInterface(aFrame));
  if (menuFrame) {
    menuFrame->UngenerateMenu();  
    menuFrame->OpenMenu(PR_FALSE);
  }
  return PR_TRUE;
}

static void
WalkFramesThroughPlaceholders(nsIPresContext *aPresContext, nsIFrame *aFrame,
                              frameWalkerFn aFunc, void *aClosure)
{
  PRBool walkChildren = (*aFunc)(aFrame, aClosure);
  if (!walkChildren)
    return;

  PRInt32 listIndex = 0;
  nsCOMPtr<nsIAtom> childList;

  do {
    nsIFrame *child = nsnull;
    aFrame->FirstChild(aPresContext, childList, &child);
    while (child) {
      nsFrameState  state;
      child->GetFrameState(&state);
      if (!(state & NS_FRAME_OUT_OF_FLOW)) {
        // only do frames that are in flow
        nsCOMPtr<nsIAtom> frameType;
        child->GetFrameType(getter_AddRefs(frameType));
        if (nsLayoutAtoms::placeholderFrame == frameType) { // placeholder
          // get out of flow frame and recur there
          nsIFrame* outOfFlowFrame =
              NS_STATIC_CAST(nsPlaceholderFrame*, child)->GetOutOfFlowFrame();
          NS_ASSERTION(outOfFlowFrame, "no out-of-flow frame");
          WalkFramesThroughPlaceholders(aPresContext, outOfFlowFrame,
                                        aFunc, aClosure);
        }
        else
          WalkFramesThroughPlaceholders(aPresContext, child, aFunc, aClosure);
      }
      child->GetNextSibling(&child);
    }

    aFrame->GetAdditionalChildListName(listIndex++, getter_AddRefs(childList));
  } while (childList);
}

NS_IMETHODIMP
PresShell::ReconstructStyleData(PRBool aRebuildRuleTree)
{
  nsIFrame* rootFrame;
  GetRootFrame(&rootFrame);
  if (!rootFrame)
    return NS_OK;

  nsCOMPtr<nsIStyleSet> set;
  GetStyleSet(getter_AddRefs(set));
  if (!set)
    return NS_OK;

  nsCOMPtr<nsIStyleFrameConstruction> cssFrameConstructor;
  set->GetStyleFrameConstruction(getter_AddRefs(cssFrameConstructor));
  if (!cssFrameConstructor)
    return NS_OK;

  nsCOMPtr<nsIFrameManager> frameManager;
  GetFrameManager(getter_AddRefs(frameManager));
  
  // Now handle some of our more problematic widgets (and also deal with
  // skin XBL changing).
  nsStyleChangeList changeList;
  if (aRebuildRuleTree) {
    // Handle those widgets that have special style contexts cached
    // that will point to invalid rule nodes following a rule tree
    // reconstruction.
    WalkFramesThroughPlaceholders(mPresContext, rootFrame,
                                  &BuildFramechangeList, &changeList);
    cssFrameConstructor->ProcessRestyledFrames(changeList, mPresContext);
    changeList.Clear();

    // Now do a complete re-resolve of our style tree.
    set->BeginRuleTreeReconstruct();
  }

  nsChangeHint frameChange = NS_STYLE_HINT_NONE;
  frameManager->ComputeStyleChangeFor(mPresContext, rootFrame, 
                                      kNameSpaceID_Unknown, nsnull,
                                      changeList, NS_STYLE_HINT_NONE, frameChange);

  if (frameChange & nsChangeHint_ReconstructDoc)
    set->ReconstructDocElementHierarchy(mPresContext);
  else {
    cssFrameConstructor->ProcessRestyledFrames(changeList, mPresContext);
    if (aRebuildRuleTree) {
      GetRootFrame(&rootFrame);
      WalkFramesThroughPlaceholders(mPresContext, rootFrame,
                                    &ReResolveMenusAndTrees, nsnull);
    }
  }

  if (aRebuildRuleTree)
    set->EndRuleTreeReconstruct();
  return NS_OK;
}

NS_IMETHODIMP
PresShell::StyleSheetAdded(nsIDocument *aDocument,
                           nsIStyleSheet* aStyleSheet)
{
  // If style information is being added, we don't need to rebuild the
  // rule tree, since no rule nodes have been rendered invalid by the
  // addition of new rule content.
  return ReconstructStyleData(PR_FALSE);
}

NS_IMETHODIMP 
PresShell::StyleSheetRemoved(nsIDocument *aDocument,
                             nsIStyleSheet* aStyleSheet)
{
  // XXXdwh We'd like to be able to use ReconstructStyleData in all the
  // other style sheet calls, but it doesn't quite work because of HTML tables.
  return ReconstructStyleData(PR_TRUE);
}

NS_IMETHODIMP
PresShell::StyleSheetDisabledStateChanged(nsIDocument *aDocument,
                                          nsIStyleSheet* aStyleSheet,
                                          PRBool aDisabled)
{
  // first notify the style set that a sheet's state has changed
  if (mStyleSet) {
    nsresult rv = mStyleSet->NotifyStyleSheetStateChanged(!aDisabled);
    if (NS_FAILED(rv))
      return rv;
  }

  // We don't need to rebuild the
  // rule tree, since no rule nodes have been rendered invalid by the
  // addition of new rule content.  They can simply lurk in the tree
  // until the stylesheet is enabled once more.
  return ReconstructStyleData(PR_FALSE);
}

NS_IMETHODIMP
PresShell::StyleRuleChanged(nsIDocument *aDocument,
                            nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule,
                            nsChangeHint aHint) 
{
  WillCauseReflow();
  nsresult  rv = mStyleSet->StyleRuleChanged(mPresContext, aStyleSheet,
                                             aStyleRule, aHint);
  VERIFY_STYLE_TREE;
  DidCauseReflow();
  return rv;
}

NS_IMETHODIMP
PresShell::StyleRuleAdded(nsIDocument *aDocument,
                          nsIStyleSheet* aStyleSheet,
                          nsIStyleRule* aStyleRule) 
{ 
  WillCauseReflow();
  nsresult rv = mStyleSet->StyleRuleAdded(mPresContext, aStyleSheet,
                                          aStyleRule);
  VERIFY_STYLE_TREE;
  DidCauseReflow();
  if (NS_FAILED(rv)) {
    return rv;
  }

  // We don't need to rebuild the
  // rule tree, since no rule nodes have been rendered invalid by the
  // addition of new rule content.  
  return ReconstructStyleData(PR_FALSE);
}

NS_IMETHODIMP
PresShell::StyleRuleRemoved(nsIDocument *aDocument,
                            nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule) 
{ 
  WillCauseReflow();
  nsresult  rv = mStyleSet->StyleRuleRemoved(mPresContext, aStyleSheet,
                                             aStyleRule);
  VERIFY_STYLE_TREE;
  DidCauseReflow();
  if (NS_FAILED(rv)) {
    return rv;
  }
  // XXX For now reconstruct everything
  return ReconstructFrames();
}

NS_IMETHODIMP
PresShell::DocumentWillBeDestroyed(nsIDocument *aDocument)
{
  return NS_OK;
}

NS_IMETHODIMP
PresShell::GetPrimaryFrameFor(nsIContent* aContent,
                              nsIFrame**  aResult) const
{
  nsresult  rv;

  if (mFrameManager) {
    rv = mFrameManager->GetPrimaryFrameFor(aContent, aResult);

  } else {
    *aResult = nsnull;
    rv = NS_OK;
  }

  return rv;
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
      result = primaryFrame->QueryInterface(NS_GET_IID(nsISupports),
                                            (void**)aResult);
    }
  }
  return result;
}

NS_IMETHODIMP
PresShell::GetPlaceholderFrameFor(nsIFrame*  aFrame,
                                  nsIFrame** aResult) const
{
  if (!mFrameManager) {
    *aResult = nsnull;
    return NS_OK;
  }
  return mFrameManager->GetPlaceholderFrameFor(aFrame, aResult);
}

#ifdef IBMBIDI
NS_IMETHODIMP
PresShell::SetCaretBidiLevel(PRUint8 aLevel)
{
  // If the current level is undefined, we have just inserted new text.
  // In this case, we don't want to reset the keyboard language
  PRBool afterInsert = mBidiLevel & BIDI_LEVEL_UNDEFINED;
  mBidiLevel = aLevel;

//  PRBool parityChange = ((mBidiLevel ^ aLevel) & 1); // is the parity of the new level different from the current level?
//  if (parityChange)                                  // if so, change the keyboard language
  if (mBidiKeyboard && !afterInsert)
    mBidiKeyboard->SetLangFromBidiLevel(aLevel);
  return NS_OK;
}

NS_IMETHODIMP
PresShell::GetCaretBidiLevel(PRUint8 *aOutLevel)
{
  if (!aOutLevel) { return NS_ERROR_INVALID_ARG; }
  *aOutLevel = mBidiLevel;
  return NS_OK;    
}

NS_IMETHODIMP
PresShell::UndefineCaretBidiLevel()
{
  mBidiLevel |= BIDI_LEVEL_UNDEFINED;
  return NS_OK;
}

NS_IMETHODIMP
PresShell::BidiStyleChangeReflow()
{
  // Have the root frame's style context remap its style
  nsIFrame* rootFrame;
  mFrameManager->GetRootFrame(&rootFrame);
  if (rootFrame) {
    mStyleSet->ClearStyleData(mPresContext, nsnull, nsnull);
    ReconstructFrames();
  }
  return NS_OK;
}
#endif // IBMBIDI

//nsIViewObserver

// Return TRUE if any clipping is to be done.
static PRBool ComputeClipRect(nsIFrame* aFrame, nsRect& aResult) {
  const nsStyleDisplay* display;
  aFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&) display);
  
  // 'clip' only applies to absolutely positioned elements, and is
  // relative to the element's border edge. 'clip' applies to the entire
  // element: border, padding, and content areas, and even scrollbars if
  // there are any.
  if (display->IsAbsolutelyPositioned() && (display->mClipFlags & NS_STYLE_CLIP_RECT)) {
    nsSize  size;

    // Start with the 'auto' values and then factor in user specified values
    aFrame->GetSize(size);
    nsRect  clipRect(0, 0, size.width, size.height);

    if (display->mClipFlags & NS_STYLE_CLIP_RECT) {
      if (0 == (NS_STYLE_CLIP_TOP_AUTO & display->mClipFlags)) {
        clipRect.y = display->mClip.y;
      }
      if (0 == (NS_STYLE_CLIP_LEFT_AUTO & display->mClipFlags)) {
        clipRect.x = display->mClip.x;
      }
      if (0 == (NS_STYLE_CLIP_RIGHT_AUTO & display->mClipFlags)) {
        clipRect.width = display->mClip.width;
      }
      if (0 == (NS_STYLE_CLIP_BOTTOM_AUTO & display->mClipFlags)) {
        clipRect.height = display->mClip.height;
      }
    }

    aResult = clipRect;

    return PR_TRUE;
  }

  return PR_FALSE;
}

// If the element is absolutely positioned and has a specified clip rect
// then it pushes the current rendering context and sets the clip rect.
// Returns PR_TRUE if the clip rect is set and PR_FALSE otherwise
static PRBool
SetClipRect(nsIRenderingContext& aRenderingContext, nsIFrame* aFrame)
{
  nsRect clipRect;

  if (ComputeClipRect(aFrame, clipRect)) {
    // Set updated clip-rect into the rendering context
    PRBool clipState;

    aRenderingContext.PushState();
    aRenderingContext.SetClipRect(clipRect, nsClipCombine_kIntersect, clipState);
    return PR_TRUE;
  }

  return PR_FALSE;
}

static PRBool
InClipRect(nsIFrame* aFrame, nsPoint& aEventPoint)
{
  nsRect clipRect;

  if (ComputeClipRect(aFrame, clipRect)) {
    return clipRect.Contains(aEventPoint);
  } else {
    return PR_TRUE;
  }
}

NS_IMETHODIMP
PresShell::Paint(nsIView              *aView,
                 nsIRenderingContext& aRenderingContext,
                 const nsRect&        aDirtyRect)
{
  void*     clientData;
  nsIFrame* frame;
  nsresult  rv = NS_OK;

  if (mIsDestroying) {
    NS_ASSERTION(PR_FALSE, "A paint message was dispatched to a destroyed PresShell");
    return NS_OK;
  }

  NS_ASSERTION(!(nsnull == aView), "null view");

  aView->GetClientData(clientData);
  frame = (nsIFrame *)clientData;
     
  if (nsnull != frame)
  {
    mCaret->EraseCaret();
    //StCaretHider  caretHider(mCaret);			// stack-based class hides caret until dtor.

    // If the frame is absolutely positioned, then the 'clip' property
    // applies
    PRBool  setClipRect = SetClipRect(aRenderingContext, frame);

    rv = frame->Paint(mPresContext, aRenderingContext, aDirtyRect,
                      NS_FRAME_PAINT_LAYER_BACKGROUND);
    rv = frame->Paint(mPresContext, aRenderingContext, aDirtyRect,
                      NS_FRAME_PAINT_LAYER_FLOATERS);
    rv = frame->Paint(mPresContext, aRenderingContext, aDirtyRect,
                      NS_FRAME_PAINT_LAYER_FOREGROUND);
                      
    if (setClipRect) {
      PRBool clipState;
      aRenderingContext.PopState(clipState);
    }

#ifdef NS_DEBUG
    // Draw a border around the frame
    if (nsIFrameDebug::GetShowFrameBorders()) {
      nsRect r;
      frame->GetRect(r);
      aRenderingContext.SetColor(NS_RGB(0,0,255));
      aRenderingContext.DrawRect(0, 0, r.width, r.height);
    }
    // Draw a border around the current event target
    if ((nsIFrameDebug::GetShowEventTargetFrameBorder()) && (aView == mCurrentTargetView)) {
      aRenderingContext.SetColor(NS_RGB(128,0,128));
      aRenderingContext.DrawRect(mCurrentTargetRect.x, mCurrentTargetRect.y, mCurrentTargetRect.width, mCurrentTargetRect.height);
    }
#endif
  }

  return rv;
}

nsIFrame*
PresShell::GetCurrentEventFrame()
{
  if (!mCurrentEventFrame && mCurrentEventContent) {
    // Make sure the content still has a document reference. If not,
    // then we assume it is no longer in the content tree and the
    // frame shouldn't get an event, nor should we even assume its
    // safe to try and find the frame.
    nsCOMPtr<nsIDocument> doc;
    nsresult result = mCurrentEventContent->GetDocument(*getter_AddRefs(doc));
    if (NS_SUCCEEDED(result) && doc) {
      GetPrimaryFrameFor(mCurrentEventContent, &mCurrentEventFrame);
    }
  }

  return mCurrentEventFrame;
}

NS_IMETHODIMP
PresShell::GetEventTargetFrame(nsIFrame** aFrame)
{
	*aFrame = GetCurrentEventFrame();
	return NS_OK;
}

void
PresShell::PushCurrentEventInfo(nsIFrame* aFrame, nsIContent* aContent)
{
  if (mCurrentEventFrame || mCurrentEventContent) {
    mCurrentEventFrameStack.InsertElementAt((void*)mCurrentEventFrame, 0);
    mCurrentEventContentStack.InsertElementAt((void*)mCurrentEventContent, 0);
  }
  mCurrentEventFrame = aFrame;
  mCurrentEventContent = aContent;
  NS_IF_ADDREF(aContent);
}

void
PresShell::PopCurrentEventInfo()
{
  mCurrentEventFrame = nsnull;
  NS_IF_RELEASE(mCurrentEventContent);

  if (0 != mCurrentEventFrameStack.Count()) {
    mCurrentEventFrame = (nsIFrame*)mCurrentEventFrameStack.ElementAt(0);
    mCurrentEventFrameStack.RemoveElementAt(0);
    mCurrentEventContent = (nsIContent*)mCurrentEventContentStack.ElementAt(0);
    mCurrentEventContentStack.RemoveElementAt(0);
  }
}

NS_IMETHODIMP
PresShell::HandleEvent(nsIView         *aView,
                       nsGUIEvent*     aEvent,
                       nsEventStatus*  aEventStatus,
											 PRBool          aForceHandle,
                       PRBool&         aHandled)
{
  void*     clientData;
  nsIFrame* frame;
  nsresult  rv = NS_OK;
  
  NS_ASSERTION(!(nsnull == aView), "null view");

  aHandled = PR_TRUE;

  if (mIsDestroying || mIsReflowing) {
    return NS_OK;
  }

#ifdef ACCESSIBILITY
  if (aEvent->eventStructType == NS_ACCESSIBLE_EVENT)
    return HandleEventInternal(aEvent, aView, NS_EVENT_FLAG_INIT, aEventStatus);
#endif

  // Check for a theme change up front, since the frame type is irrelevant
  if (aEvent->message == NS_THEMECHANGED && mPresContext)
    return mPresContext->ThemeChanged();

  // Check for a system color change up front, since the frame type is irrelevenat
  if ((aEvent->message == NS_SYSCOLORCHANGED) && mPresContext) {
    nsIViewManager *vm;
    if ((NS_SUCCEEDED(GetViewManager(&vm))) && vm) {
      // Only dispatch system color change when the message originates from
      // from the root views widget. This is necessary to prevent us from 
      // dispatching the SysColorChanged notification for each child window 
      // which may be redundant.
      nsIView *view;
      vm->GetRootView(view);
      if (view == aView) {
        aHandled = PR_TRUE;
        *aEventStatus = nsEventStatus_eConsumeDoDefault;
        return mPresContext->SysColorChanged();
      }
    }
    return NS_OK;
  }


  // We really don't want to just drop a focus event on the floor here,
  // because the widget-level focus change will have already happened
  // by the time we get the event.  Rather than dropping it, manually
  // update the focus controller here.

  if (aEvent->message == NS_GOTFOCUS && !mDidInitialReflow && mDocument) {
    nsCOMPtr<nsIScriptGlobalObject> sgo;
    mDocument->GetScriptGlobalObject(getter_AddRefs(sgo));
    if (sgo) {
      nsCOMPtr<nsPIDOMWindow> pWindow = do_QueryInterface(sgo);
      if (pWindow) {
        nsCOMPtr<nsIFocusController> controller;
        pWindow->GetRootFocusController(getter_AddRefs(controller));
        if (controller) {
          nsCOMPtr<nsIDOMWindowInternal> domWin = do_QueryInterface(sgo);
          controller->SetFocusedWindow(domWin);
          controller->SetFocusedElement(nsnull);
        }
      }
    }
  }

  aView->GetClientData(clientData);
  frame = (nsIFrame *)clientData;

/*  if (mSelection && aEvent->eventStructType == NS_KEY_EVENT)
  {//KEY HANDLERS WILL GET RID OF THIS 
    if (mSelectionFlags && NS_SUCCEEDED(mSelection->HandleKeyEvent(mPresContext, aEvent)))
    {  
      return NS_OK;
    }
  }
*/
  if (nsnull != frame) {
    PushCurrentEventInfo(nsnull, nsnull);

    nsIEventStateManager *manager;
    if (NS_OK == mPresContext->GetEventStateManager(&manager)) {
      //change 6-01-00 mjudge,ftang adding ime as an event that needs focused element
      if (NS_IS_KEY_EVENT(aEvent) || NS_IS_IME_EVENT(aEvent) || aEvent->message == NS_CONTEXTMENU_KEY) { 
        //Key events go to the focused frame, not point based.
        manager->GetFocusedContent(&mCurrentEventContent);
        if (mCurrentEventContent)
          GetPrimaryFrameFor(mCurrentEventContent, &mCurrentEventFrame);
        else {
#if defined(MOZ_X11)
          if (NS_IS_IME_EVENT(aEvent)) {
            // bug 52416
            // Lookup region (candidate window) of UNIX IME grabs
            // input focus from Mozilla but wants to send IME event
            // to redraw pre-edit (composed) string
            // If Mozilla does not have input focus and event is IME,
            // sends IME event to pre-focused element
            nsCOMPtr<nsIScriptGlobalObject> ourGlobal;
            mDocument->GetScriptGlobalObject(getter_AddRefs(ourGlobal));
            nsCOMPtr<nsPIDOMWindow> ourWindow = do_QueryInterface(ourGlobal);
            if (ourWindow) {
              nsCOMPtr<nsIFocusController> focusController;
              ourWindow->GetRootFocusController(getter_AddRefs(focusController));
              if (focusController) {
                PRBool active = PR_FALSE;
                // check input focus is in Mozilla
                focusController->GetActive(&active);
                if (!active) {
                  // if not, search for pre-focused element
                  nsCOMPtr<nsIDOMElement> focusedElement;
                  focusController->GetFocusedElement(getter_AddRefs(focusedElement));
                  if (focusedElement) {
                    // get mCurrentEventContent from focusedElement
                    CallQueryInterface(focusedElement, &mCurrentEventContent);
                  }
                }
              }
            }
          }

          if (!mCurrentEventContent) {
            // fallback, call existing codes
            mDocument->GetRootContent(&mCurrentEventContent);
          }
#else /* defined(MOZ_X11) */

          // XXX This is the way key events seem to work?  Why?????
          // They spend time doing calls to GetFrameForPoint with the
          // point as (0,0) (or sometimes something else).

          // XXX If this code is really going to stay, it should
          // probably go into a separate function, because its just
          // a duplicate of the code a few lines below:

          // This is because we want to give the point in the same
          // coordinates as the frame's own Rect, so mRect.Contains(aPoint)
          // works.  However, this point is relative to the frame's rect, so
          // we need to add on the origin of the rect.
/*          nsPoint eventPoint;
          frame->GetOrigin(eventPoint);
          eventPoint += aEvent->point;
          rv = frame->GetFrameForPoint(mPresContext, eventPoint, NS_FRAME_PAINT_LAYER_FOREGROUND, &mCurrentEventFrame);
          if (rv != NS_OK) {
            rv = frame->GetFrameForPoint(mPresContext, eventPoint, NS_FRAME_PAINT_LAYER_FLOATERS, &mCurrentEventFrame);
            if (rv != NS_OK) {
              rv = frame->GetFrameForPoint(mPresContext, eventPoint, NS_FRAME_PAINT_LAYER_BACKGROUND, &mCurrentEventFrame);
              if (rv != NS_OK) {
#ifdef XP_MAC*/
                // On the Mac it is possible to be running with no windows open, only the native menu bar.
                // In this situation, we need to handle key board events but there are no frames, so
                // we set mCurrentEventContent and that works itself out in HandleEventInternal.
                mDocument->GetRootContent(&mCurrentEventContent);
#endif /* defined(MOZ_X11) */
                mCurrentEventFrame = nsnull;
/*#else
								if (aForceHandle) {
									mCurrentEventFrame = frame;
								}
								else {
									mCurrentEventFrame = nsnull;
								}
                aHandled = PR_FALSE;
#endif
                rv = NS_OK;
              }
            }
          }
          */
        }
      }
      else if (!InClipRect(frame, aEvent->point)) {
        // we only check for the clip rect on this frame ... all frames with clip have views so any
        // viewless children of this frame cannot have clip. Furthermore if the event is not in the clip
        // for this frame, then none of the children can get it either.
        if (aForceHandle) {
          mCurrentEventFrame = frame;
        }
        else {
          mCurrentEventFrame = nsnull;
        }
        aHandled = PR_FALSE;
        rv = NS_OK;
      } else {
        // aEvent->point is relative to aView's upper left corner. We need
        // a point that is in the same coordinate system as frame's rect
        // so that the frame->mRect.Contains(aPoint) calls in 
        // GetFrameForPoint() work. The assumption here is that frame->GetView()
        // will return aView, and frame's parent view is aView's parent.

        nsPoint eventPoint;
        frame->GetOrigin(eventPoint);
        eventPoint += aEvent->point;

        nsPoint originOffset;
        nsIView *view = nsnull;
        frame->GetOriginToViewOffset(mPresContext, originOffset, &view);

#ifdef DEBUG_kin
        NS_ASSERTION(view == aView, "view != aView");
#endif // DEBUG_kin

        if (view == aView)
          eventPoint -= originOffset;

        rv = frame->GetFrameForPoint(mPresContext, eventPoint, NS_FRAME_PAINT_LAYER_FOREGROUND, &mCurrentEventFrame);
        if (rv != NS_OK) {
          rv = frame->GetFrameForPoint(mPresContext, eventPoint, NS_FRAME_PAINT_LAYER_FLOATERS, &mCurrentEventFrame);
          if (rv != NS_OK) {
            rv = frame->GetFrameForPoint(mPresContext, eventPoint, NS_FRAME_PAINT_LAYER_BACKGROUND, &mCurrentEventFrame);
            if (rv != NS_OK) {
							if (aForceHandle) {
								mCurrentEventFrame = frame;
							}
							else {
								mCurrentEventFrame = nsnull;
							}
              aHandled = PR_FALSE;
              rv = NS_OK;
            }
          }
        }
      }
      if (GetCurrentEventFrame()) {
        rv = HandleEventInternal(aEvent, aView, NS_EVENT_FLAG_INIT, aEventStatus);
      }
      NS_RELEASE(manager);
    }
#ifdef NS_DEBUG
    if ((nsIFrameDebug::GetShowEventTargetFrameBorder()) && (GetCurrentEventFrame())) {
      nsIView *oldView = mCurrentTargetView;
      nsPoint offset(0,0);
      nsRect oldTargetRect(mCurrentTargetRect);
      mCurrentEventFrame->GetRect(mCurrentTargetRect);
      mCurrentEventFrame->GetView(mPresContext, &mCurrentTargetView);
      if ( ! mCurrentTargetView ) {
        mCurrentEventFrame->GetOffsetFromView(mPresContext, offset, &mCurrentTargetView);
      }
      if (mCurrentTargetView) {
        mCurrentTargetRect.x = offset.x;
        mCurrentTargetRect.y = offset.y;
        // use aView or mCurrentTargetView??
        if ( (mCurrentTargetRect != oldTargetRect) || (mCurrentTargetView != oldView)) {
          nsIViewManager *vm;
          if ((NS_OK == GetViewManager(&vm)) && vm) {
            vm->UpdateView(mCurrentTargetView,mCurrentTargetRect,0);
            if (oldView)
              vm->UpdateView(oldView,oldTargetRect,0);
            NS_IF_RELEASE(vm);
          }
        }
      }
    }
#endif
    PopCurrentEventInfo();
  }
  else {
    aHandled = PR_FALSE;
    rv = NS_OK;
  }

  return rv;
}

NS_IMETHODIMP
PresShell::HandleEventWithTarget(nsEvent* aEvent, nsIFrame* aFrame, nsIContent* aContent, PRUint32 aFlags, nsEventStatus* aStatus)
{
  nsresult ret;

  PushCurrentEventInfo(aFrame, aContent);
  ret = HandleEventInternal(aEvent, nsnull, aFlags, aStatus);
  PopCurrentEventInfo();
  return NS_OK;
}

nsresult
PresShell::HandleEventInternal(nsEvent* aEvent, nsIView *aView, PRUint32 aFlags, nsEventStatus* aStatus)
{
#ifdef ACCESSIBILITY
  if (aEvent->eventStructType == NS_ACCESSIBLE_EVENT)
  {
    void*     clientData;
    aView->GetClientData(clientData);
    nsIFrame* frame = (nsIFrame *)clientData;
    if (!frame)
      return NS_ERROR_FAILURE;
    return frame->HandleEvent(mPresContext, (nsGUIEvent*)aEvent, aStatus);
  }
#endif

  nsresult rv = NS_OK;

  nsIEventStateManager *manager;
  if (NS_OK == mPresContext->GetEventStateManager(&manager) && GetCurrentEventFrame()) {
    //1. Give event to event manager for pre event state changes and generation of synthetic events.
    rv = manager->PreHandleEvent(mPresContext, aEvent, mCurrentEventFrame, aStatus, aView);

    //2. Give event to the DOM for third party and JS use.
    if ((GetCurrentEventFrame()) && NS_OK == rv) {
      if (mCurrentEventContent) {
        rv = mCurrentEventContent->HandleDOMEvent(mPresContext, aEvent, nsnull, 
                                           aFlags, aStatus);
      }
      else {
        nsIContent* targetContent;
        if (NS_OK == mCurrentEventFrame->GetContentForEvent(mPresContext, aEvent, &targetContent) && nsnull != targetContent) {
          rv = targetContent->HandleDOMEvent(mPresContext, aEvent, nsnull, 
                                             aFlags, aStatus);
          NS_RELEASE(targetContent);
        }
      }

      //Continue with second dispatch to system event handlers.
      // Need to null check mCurrentEventContent and mCurrentEventFrame
      // since the previous dispatch could have nuked them.
      if (mCurrentEventContent) {
        rv = mCurrentEventContent->HandleDOMEvent(mPresContext, aEvent, nsnull, 
                                           aFlags | NS_EVENT_FLAG_SYSTEM_EVENT, aStatus);
      }
      else if (mCurrentEventFrame) {
        nsIContent* targetContent;
        if (NS_OK == mCurrentEventFrame->GetContentForEvent(mPresContext, aEvent, &targetContent) && nsnull != targetContent) {
          rv = targetContent->HandleDOMEvent(mPresContext, aEvent, nsnull, 
                                             aFlags | NS_EVENT_FLAG_SYSTEM_EVENT, aStatus);
          NS_RELEASE(targetContent);
        }
      }

      //3. Give event to the Frames for browser default processing.
      // XXX The event isn't translated into the local coordinate space
      // of the frame...
      if (GetCurrentEventFrame() && NS_OK == rv && aEvent->eventStructType != NS_EVENT) {
        rv = mCurrentEventFrame->HandleEvent(mPresContext, (nsGUIEvent*)aEvent, aStatus);
      }

      //4. Give event to event manager for post event state changes and generation of synthetic events.
      if ((GetCurrentEventFrame()) && NS_OK == rv) {
        rv = manager->PostHandleEvent(mPresContext, aEvent, mCurrentEventFrame, aStatus, aView);
      }
    }
    NS_RELEASE(manager);
  }
  return rv;
}

// Dispatch event to content only (NOT full processing)
// See also HandleEventWithTarget which does full event processing.
NS_IMETHODIMP
PresShell::HandleDOMEventWithTarget(nsIContent* aTargetContent, nsEvent* aEvent, nsEventStatus* aStatus)
{
  nsresult ret;

  PushCurrentEventInfo(nsnull, aTargetContent);

  // Bug 41013: Check if the event should be dispatched to content.
  // It's possible that we are in the middle of destroying the window
  // and the js context is out of date. This check detects the case
  // that caused a crash in bug 41013, but there may be a better way
  // to handle this situation!
  nsCOMPtr<nsISupports> container;
  ret = mPresContext->GetContainer(getter_AddRefs(container));
  if (NS_SUCCEEDED(ret) && container) {

    // Dispatch event to content
    ret = aTargetContent->HandleDOMEvent(mPresContext, aEvent, nsnull, NS_EVENT_FLAG_INIT, aStatus);
  }

  PopCurrentEventInfo();
  return NS_OK;
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

//--------------------------------------------------------
// Start of protected and private methods on the PresShell
//--------------------------------------------------------

//-------------- Begin Reflow Event Definition ------------------------

struct ReflowEvent : public PLEvent {
  ReflowEvent(nsIPresShell* aPresShell);
  ~ReflowEvent() { }

  void HandleEvent() {    
    nsCOMPtr<nsIPresShell> presShell = do_QueryReferent(mPresShell);
    if (presShell) {
#ifdef DEBUG
      if (VERIFY_REFLOW_NOISY_RC & gVerifyReflowFlags) {
         printf("\n*** Handling reflow event: PresShell=%p, event=%p\n", (void*)presShell.get(), (void*)this);
      }
#endif
      // XXX Statically cast the pres shell pointer so that we can call
      // protected methods on the pres shell. (the ReflowEvent struct
      // is a friend of the PresShell class)
      PresShell* ps = NS_REINTERPRET_CAST(PresShell*, presShell.get());
      PRBool isBatching;
      ps->ClearReflowEventStatus();
      ps->GetReflowBatchingStatus(&isBatching);
      if (!isBatching) {
        nsCOMPtr<nsIViewManager> viewManager;
        // Set a kung fu death grip on the view manager associated with the pres shell
        // before processing that pres shell's reflow commands.  Fixes bug 54868.
        presShell->GetViewManager(getter_AddRefs(viewManager));
        ps->ProcessReflowCommands(PR_TRUE);

        // Now, explicitly release the pres shell before the view manager
        presShell = nsnull;
        viewManager = nsnull;
      }
    }
    else
      mPresShell = 0;
  }
  
  nsWeakPtr mPresShell;
};

static void PR_CALLBACK HandlePLEvent(ReflowEvent* aEvent)
{
  aEvent->HandleEvent();
}

static void PR_CALLBACK DestroyPLEvent(ReflowEvent* aEvent)
{
  delete aEvent;
}


ReflowEvent::ReflowEvent(nsIPresShell* aPresShell)
{
  NS_ASSERTION(aPresShell, "Null parameters!");  

  mPresShell = getter_AddRefs(NS_GetWeakReference(aPresShell));

  PL_InitEvent(this, aPresShell,
               (PLHandleEventProc) ::HandlePLEvent,
               (PLDestroyEventProc) ::DestroyPLEvent);  
}

//-------------- End Reflow Event Definition ---------------------------


void
PresShell::PostReflowEvent()
{
  nsCOMPtr<nsIEventQueue> eventQueue;
  mEventQueueService->GetSpecialEventQueue(nsIEventQueueService::UI_THREAD_EVENT_QUEUE,
                                           getter_AddRefs(eventQueue));

  if (eventQueue != mReflowEventQueue &&
      !mIsReflowing && mReflowCommands.Count() > 0) {
    ReflowEvent* ev = new ReflowEvent(NS_STATIC_CAST(nsIPresShell*, this));
    eventQueue->PostEvent(ev);
    mReflowEventQueue = eventQueue;
#ifdef DEBUG
    if (VERIFY_REFLOW_NOISY_RC & gVerifyReflowFlags) {
      printf("\n*** PresShell::PostReflowEvent(), this=%p, event=%p\n", (void*)this, (void*)ev);
    }
#endif    
  }
}

nsresult
PresShell::WillCauseReflow()
{
  mViewManager->CacheWidgetChanges(PR_TRUE);  
  return NS_OK;
}

nsresult
PresShell::DidCauseReflow()
{
  if (mViewManager) {
    mViewManager->CacheWidgetChanges(PR_FALSE);
  }

  // We may have had more reflow commands appended to the queue during
  // our reflow.  Make sure these get processed at some point.
  if (!gAsyncReflowDuringDocLoad && mDocumentLoading) {
    FlushPendingNotifications(PR_FALSE);
  } else {
    PostReflowEvent();
  }

  return NS_OK;
}

nsresult
PresShell::ProcessReflowCommands(PRBool aInterruptible)
{
  MOZ_TIMER_DEBUGLOG(("Start: Reflow: PresShell::ProcessReflowCommands(), this=%p\n", this));
  MOZ_TIMER_START(mReflowWatch);  

  if (0 != mReflowCommands.Count()) {
    nsHTMLReflowMetrics   desiredSize(nsnull);
    nsIRenderingContext*  rcx;
    nsIFrame*             rootFrame;        
    mFrameManager->GetRootFrame(&rootFrame);
    nsSize          maxSize;
    rootFrame->GetSize(maxSize);

    nsresult rv=CreateRenderingContext(rootFrame, &rcx);
	  if (NS_FAILED(rv)) return rv;

#ifdef DEBUG
    if (GetVerifyReflowEnable()) {
      if (VERIFY_REFLOW_ALL & gVerifyReflowFlags) {
        printf("ProcessReflowCommands: begin incremental reflow\n");
      }
    }
    if (VERIFY_REFLOW_DUMP_COMMANDS & gVerifyReflowFlags) {   
      PRInt32 i, n = mReflowCommands.Count();
      printf("\nPresShell::ProcessReflowCommands: this=%p, count=%d\n", (void*)this, n);
      for (i = 0; i < n; i++) {
        nsHTMLReflowCommand* rc = (nsHTMLReflowCommand*)
          mReflowCommands.ElementAt(i);
        rc->List(stdout);
      }
    }
#endif

    // If reflow is interruptible, then make a note of our deadline.
    PRIntervalTime deadline;
    if (aInterruptible)
      deadline = PR_IntervalNow() + PR_MicrosecondsToInterval(gMaxRCProcessingTime);

    mIsReflowing = PR_TRUE;

    do {
      // Coalesce the reflow commands into a tree.
      IncrementalReflow reflow;
      for (PRInt32 i = mReflowCommands.Count() - 1; i >= 0; --i) {
        nsHTMLReflowCommand *command =
          NS_STATIC_CAST(nsHTMLReflowCommand *, mReflowCommands[i]);

        if (reflow.AddCommand(mPresContext, command)) {
          // Remove the command from the queue.
          mReflowCommands.RemoveElementAt(i);
          ReflowCommandRemoved(command);
        }
        else {
          // The reflow command couldn't be added to the tree; leave
          // it in the queue, and we'll handle it next time.
#ifdef DEBUG
          printf("WARNING: Couldn't add reflow command, so splitting.\n");
#endif
        }
      }

#ifdef DEBUG
      if (VERIFY_REFLOW_NOISY_RC & gVerifyReflowFlags) {
        printf("Incremental reflow tree:\n");
        reflow.Dump(mPresContext);
      }
#endif

      // Dispatch an incremental reflow.
      reflow.Dispatch(mPresContext, desiredSize, maxSize, *rcx);

      // Keep going until we're out of reflow commands, or we've run
      // past our deadline.
    } while (mReflowCommands.Count() &&
             (!aInterruptible || PR_IntervalNow() < deadline));

    // XXXwaterson for interruptible reflow, examine the tree and
    // re-enqueue any unflowed reflow targets.

    mIsReflowing = PR_FALSE;

    NS_IF_RELEASE(rcx);

    // If any new reflow commands were enqueued during the reflow,
    // schedule another reflow event to process them.
    if (mReflowCommands.Count())
      PostReflowEvent();
    
#ifdef DEBUG
    if (VERIFY_REFLOW_DUMP_COMMANDS & gVerifyReflowFlags) {
      printf("\nPresShell::ProcessReflowCommands() finished: this=%p\n", (void*)this);
    }

    if (nsIFrameDebug::GetVerifyTreeEnable()) {
      nsIFrameDebug*  frameDebug;

      if (NS_SUCCEEDED(rootFrame->QueryInterface(NS_GET_IID(nsIFrameDebug),
                                                 (void**)&frameDebug))) {
        frameDebug->VerifyTree();
      }
    }
    if (GetVerifyReflowEnable()) {
      // First synchronously render what we have so far so that we can
      // see it.
      nsIView* rootView;
      mViewManager->GetRootView(rootView);
      mViewManager->UpdateView(rootView, NS_VMREFRESH_IMMEDIATE);

      mInVerifyReflow = PR_TRUE;
      PRBool ok = VerifyIncrementalReflow();
      mInVerifyReflow = PR_FALSE;
      if (VERIFY_REFLOW_ALL & gVerifyReflowFlags) {
        printf("ProcessReflowCommands: finished (%s)\n",
               ok ? "ok" : "failed");
      }

      if (0 != mReflowCommands.Count()) {
        printf("XXX yikes! reflow commands queued during verify-reflow\n");
      }
    }
#endif

    // If there are no more reflow commands in the queue, we'll want
    // to remove the ``dummy request''.
    DoneRemovingReflowCommands();
  }
  
  MOZ_TIMER_DEBUGLOG(("Stop: Reflow: PresShell::ProcessReflowCommands(), this=%p\n", this));
  MOZ_TIMER_STOP(mReflowWatch);  

  HandlePostedDOMEvents();
  HandlePostedAttributeChanges();
  HandlePostedReflowCallbacks();

  if (mShouldUnsuppressPainting && mReflowCommands.Count() == 0) {
    // We only unlock if we're out of reflows.  It's pointless
    // to unlock if reflows are still pending, since reflows
    // are just going to thrash the frames around some more.  By
    // waiting we avoid an overeager "jitter" effect.
    mShouldUnsuppressPainting = PR_FALSE;
    UnsuppressAndInvalidate();
  }

  return NS_OK;
}

nsresult
PresShell::ClearReflowEventStatus()
{
  mReflowEventQueue = nsnull;
  return NS_OK;
}

nsresult
PresShell::CloneStyleSet(nsIStyleSet* aSet, nsIStyleSet** aResult)
{
  nsresult rv;
  nsCOMPtr<nsIStyleSet> clone(do_CreateInstance(kStyleSetCID,&rv));
  if (NS_FAILED(rv)) {
    return rv;
  }

  PRInt32 i, n;
  n = aSet->GetNumberOfOverrideStyleSheets();
  for (i = 0; i < n; i++) {
    nsIStyleSheet* ss;
    ss = aSet->GetOverrideStyleSheetAt(i);
    if (nsnull != ss) {
      clone->AppendOverrideStyleSheet(ss);
      NS_RELEASE(ss);
    }
  }

  n = aSet->GetNumberOfDocStyleSheets();
  for (i = 0; i < n; i++) {
    nsIStyleSheet* ss;
    ss = aSet->GetDocStyleSheetAt(i);
    if (nsnull != ss) {
      clone->AddDocStyleSheet(ss, mDocument);
      NS_RELEASE(ss);
    }
  }
  n = aSet->GetNumberOfUserStyleSheets();
  for (i = 0; i < n; i++) {
    nsIStyleSheet* ss;
    ss = aSet->GetUserStyleSheetAt(i);
    if (nsnull != ss) {
      clone->AppendUserStyleSheet(ss);
      NS_RELEASE(ss);
    }
  }

  n = aSet->GetNumberOfAgentStyleSheets();
  for (i = 0; i < n; i++) {
    nsIStyleSheet* ss;
    ss = aSet->GetAgentStyleSheetAt(i);
    if (nsnull != ss) {
      clone->AppendAgentStyleSheet(ss);
      NS_RELEASE(ss);
    }
  }
  *aResult = clone.get();
  NS_ADDREF(*aResult);
  return NS_OK;
}


nsresult
PresShell::ReflowCommandAdded(nsHTMLReflowCommand* aRC)
{

  if (gAsyncReflowDuringDocLoad) {
    NS_PRECONDITION(mRCCreatedDuringLoad >= 0, "PresShell's reflow command queue is in a bad state.");
    if (mDocumentLoading) {
      PRInt32 flags;
      aRC->GetFlags(&flags);
      flags |= NS_RC_CREATED_DURING_DOCUMENT_LOAD;
      aRC->SetFlags(flags);    
      mRCCreatedDuringLoad++;

#ifdef PR_LOGGING
      if (PR_LOG_TEST(gLog, PR_LOG_DEBUG)) {
        nsIFrame* target;
        aRC->GetTarget(target);

        nsCOMPtr<nsIAtom> type;
        target->GetFrameType(getter_AddRefs(type));
        NS_ASSERTION(type, "frame didn't override GetFrameType()");

        nsAutoString typeStr(NS_LITERAL_STRING("unknown"));
        if (type)
          type->ToString(typeStr);

        PR_LOG(gLog, PR_LOG_DEBUG,
               ("presshell=%p, ReflowCommandAdded(%p) target=%p[%s] mRCCreatedDuringLoad=%d\n",
                this, aRC, target, NS_ConvertUCS2toUTF8(typeStr).get(), mRCCreatedDuringLoad));
      }
#endif

      if (!mDummyLayoutRequest) {
        AddDummyLayoutRequest();
      }
    }
  }
  return NS_OK;
}

nsresult
PresShell::ReflowCommandRemoved(nsHTMLReflowCommand* aRC)
{
  if (gAsyncReflowDuringDocLoad) {
    NS_PRECONDITION(mRCCreatedDuringLoad >= 0, "PresShell's reflow command queue is in a bad state.");  
    PRInt32 flags;
    aRC->GetFlags(&flags);
    if (flags & NS_RC_CREATED_DURING_DOCUMENT_LOAD) {
      mRCCreatedDuringLoad--;

      PR_LOG(gLog, PR_LOG_DEBUG,
             ("presshell=%p, ReflowCommandRemoved(%p) mRCCreatedDuringLoad=%d\n",
              this, aRC, mRCCreatedDuringLoad));
    }
  }
  return NS_OK;
}

void
PresShell::DoneRemovingReflowCommands()
{
  if (mRCCreatedDuringLoad == 0 && mDummyLayoutRequest && !mIsReflowing) {
    RemoveDummyLayoutRequest();
  }
}

nsresult
PresShell::AddDummyLayoutRequest(void)
{ 
  nsresult rv = NS_OK;

  if (gAsyncReflowDuringDocLoad && !mIsReflowing) {
    rv = nsDummyLayoutRequest::Create(getter_AddRefs(mDummyLayoutRequest), this);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsILoadGroup> loadGroup;
    if (mDocument) {
      rv = mDocument->GetDocumentLoadGroup(getter_AddRefs(loadGroup));
      if (NS_FAILED(rv)) return rv;
    }

    if (loadGroup) {
      rv = mDummyLayoutRequest->SetLoadGroup(loadGroup);
      if (NS_FAILED(rv)) return rv;
      rv = loadGroup->AddRequest(mDummyLayoutRequest, nsnull);
      if (NS_FAILED(rv)) return rv;

      PR_LOG(gLog, PR_LOG_ALWAYS,
             ("presshell=%p, Added dummy layout request %p", this, mDummyLayoutRequest.get()));
    }
  }
  return rv;
}

nsresult
PresShell::RemoveDummyLayoutRequest(void)
{
  nsresult rv = NS_OK;

  if (gAsyncReflowDuringDocLoad) {
    nsCOMPtr<nsILoadGroup> loadGroup;
    if (mDocument) {
      rv = mDocument->GetDocumentLoadGroup(getter_AddRefs(loadGroup));
      if (NS_FAILED(rv)) return rv;
    }

    if (loadGroup && mDummyLayoutRequest) {
      rv = loadGroup->RemoveRequest(mDummyLayoutRequest, nsnull, NS_OK);
      if (NS_FAILED(rv)) return rv;

      PR_LOG(gLog, PR_LOG_ALWAYS,
             ("presshell=%p, Removed dummy layout request %p", this, mDummyLayoutRequest.get()));

      mDummyLayoutRequest = nsnull;
    }
  }
  return rv;
}

//------------------------------------------------------
// End of protected and private methods on the PresShell
//------------------------------------------------------

// Start of DEBUG only code

#ifdef NS_DEBUG
#include "nsViewsCID.h"
#include "nsWidgetsCID.h"
#include "nsIScrollableView.h"
#include "nsIDeviceContext.h"
#include "nsIURL.h"
#include "nsILinkHandler.h"

static NS_DEFINE_CID(kViewManagerCID, NS_VIEW_MANAGER_CID);
static NS_DEFINE_CID(kScrollingViewCID, NS_SCROLLING_VIEW_CID);
static NS_DEFINE_CID(kWidgetCID, NS_CHILD_CID);

static void
LogVerifyMessage(nsIFrame* k1, nsIFrame* k2, const char* aMsg)
{
  printf("verifyreflow: ");
  nsAutoString name;
  if (nsnull != k1) {
    nsIFrameDebug*  frameDebug;

    if (NS_SUCCEEDED(k1->QueryInterface(NS_GET_IID(nsIFrameDebug),
                                        (void**)&frameDebug))) {
     frameDebug->GetFrameName(name);
    }
  }
  else {
    name.Assign(NS_LITERAL_STRING("(null)"));
  }
  fputs(NS_LossyConvertUCS2toASCII(name).get(), stdout);

  printf(" != ");

  if (nsnull != k2) {
    nsIFrameDebug*  frameDebug;

    if (NS_SUCCEEDED(k2->QueryInterface(NS_GET_IID(nsIFrameDebug),
                                        (void**)&frameDebug))) {
      frameDebug->GetFrameName(name);
    }
  }
  else {
    name.Assign(NS_LITERAL_STRING("(null)"));
  }
  fputs(NS_LossyConvertUCS2toASCII(name).get(), stdout);

  printf(" %s", aMsg);
}

static void
LogVerifyMessage(nsIFrame* k1, nsIFrame* k2, const char* aMsg,
                 const nsRect& r1, const nsRect& r2)
{
  printf("VerifyReflow Error:\n");
  nsAutoString name;
  nsIFrameDebug*  frameDebug;

  if (NS_SUCCEEDED(k1->QueryInterface(NS_GET_IID(nsIFrameDebug),
                                      (void**)&frameDebug))) {
    fprintf(stdout, "  ");
    frameDebug->GetFrameName(name);
    fputs(NS_LossyConvertUCS2toASCII(name).get(), stdout);
    fprintf(stdout, " %p ", (void*)k1);
  }
  printf("{%d, %d, %d, %d}", r1.x, r1.y, r1.width, r1.height);

  printf(" != \n");

  if (NS_SUCCEEDED(k2->QueryInterface(NS_GET_IID(nsIFrameDebug),
                                      (void**)&frameDebug))) {
    fprintf(stdout, "  ");
    frameDebug->GetFrameName(name);
    fputs(NS_LossyConvertUCS2toASCII(name).get(), stdout);
    fprintf(stdout, " %p ", (void*)k2);
  }
  printf("{%d, %d, %d, %d}\n", r2.x, r2.y, r2.width, r2.height);

  printf("  %s\n", aMsg);
}

static PRBool
CompareTrees(nsIPresContext* aFirstPresContext, nsIFrame* aFirstFrame, 
             nsIPresContext* aSecondPresContext, nsIFrame* aSecondFrame)
{
  if (!aFirstPresContext || !aFirstFrame || !aSecondPresContext || !aSecondFrame)
    return PR_TRUE;
  PRBool ok = PR_TRUE;
  nsIAtom* listName = nsnull;
  PRInt32 listIndex = 0;
  do {
    nsIFrame* k1, *k2;
    aFirstFrame->FirstChild(aFirstPresContext, listName, &k1);
    aSecondFrame->FirstChild(aSecondPresContext, listName, &k2);
    PRInt32 l1 = nsContainerFrame::LengthOf(k1);
    PRInt32 l2 = nsContainerFrame::LengthOf(k2);
    if (l1 != l2) {
      ok = PR_FALSE;
      LogVerifyMessage(k1, k2, "child counts don't match: ");
      printf("%d != %d\n", l1, l2);
      if (0 == (VERIFY_REFLOW_ALL & gVerifyReflowFlags)) {
        break;
      }
    }

    nsRect r1, r2;
    nsIView* v1, *v2;
    nsCOMPtr<nsIWidget> w1;
    nsCOMPtr<nsIWidget> w2;
    for (;;) {
      if (((nsnull == k1) && (nsnull != k2)) ||
          ((nsnull != k1) && (nsnull == k2))) {
        ok = PR_FALSE;
        LogVerifyMessage(k1, k2, "child lists are different\n");
        break;
      }
      else if (nsnull != k1) {
        // Verify that the frames are the same size
        k1->GetRect(r1);
        k2->GetRect(r2);
        if (r1 != r2) {
          ok = PR_FALSE;
          LogVerifyMessage(k1, k2, "(frame rects)", r1, r2);
        }

        // Make sure either both have views or neither have views; if they
        // do have views, make sure the views are the same size. If the
        // views have widgets, make sure they both do or neither does. If
        // they do, make sure the widgets are the same size.
        k1->GetView(aFirstPresContext, &v1);
        k2->GetView(aSecondPresContext, &v2);
        if (((nsnull == v1) && (nsnull != v2)) ||
            ((nsnull != v1) && (nsnull == v2))) {
          ok = PR_FALSE;
          LogVerifyMessage(k1, k2, "child views are not matched\n");
        }
        else if (nsnull != v1) {
          v1->GetBounds(r1);
          v2->GetBounds(r2);
          if (r1 != r2) {
            LogVerifyMessage(k1, k2, "(view rects)", r1, r2);
          }

          v1->GetWidget(*getter_AddRefs(w1));
          v2->GetWidget(*getter_AddRefs(w2));
          if (((nsnull == w1) && (nsnull != w2)) ||
              ((nsnull != w1) && (nsnull == w2))) {
            ok = PR_FALSE;
            LogVerifyMessage(k1, k2, "child widgets are not matched\n");
          }
          else if (nsnull != w1) {
            w1->GetBounds(r1);
            w2->GetBounds(r2);
            if (r1 != r2) {
              LogVerifyMessage(k1, k2, "(widget rects)", r1, r2);
            }
          }
        }
        if (!ok && (0 == (VERIFY_REFLOW_ALL & gVerifyReflowFlags))) {
          break;
        }

        // verify that neither frame has a space manager,
        // or they both do and the space managers are equivalent
        nsCOMPtr<nsIFrameManager>fm1;
        nsCOMPtr<nsIPresShell> ps1;
        nsSpaceManager *sm1;
        aFirstPresContext->GetShell(getter_AddRefs(ps1));
        NS_ASSERTION(ps1, "no pres shell for primary tree!");
        ps1->GetFrameManager(getter_AddRefs(fm1));
        NS_ASSERTION(fm1, "no frame manager for primary tree!");
        fm1->GetFrameProperty((nsIFrame*)k1, nsLayoutAtoms::spaceManagerProperty,
                                     0, (void **)&sm1);
        // look at the test frame
        nsCOMPtr<nsIFrameManager>fm2;
        nsCOMPtr<nsIPresShell> ps2;
        nsSpaceManager *sm2;
        aSecondPresContext->GetShell(getter_AddRefs(ps2));
        NS_ASSERTION(ps2, "no pres shell for test tree!");
        ps2->GetFrameManager(getter_AddRefs(fm2));
        NS_ASSERTION(fm2, "no frame manager for test tree!");
        fm2->GetFrameProperty((nsIFrame*)k2, nsLayoutAtoms::spaceManagerProperty,
                                     0, (void **)&sm2);
        // now compare the space managers
        if (((nsnull == sm1) && (nsnull != sm2)) ||
            ((nsnull != sm1) && (nsnull == sm2))) {   // one is null, and the other is not
          ok = PR_FALSE;
          LogVerifyMessage(k1, k2, "space managers are not matched\n");
        }
        else if (sm1 && sm2) {  // both are not null, compare them
          // first, compare yMost
          nscoord yMost1, yMost2;
          nsresult smresult = sm1->YMost(yMost1);
          if (NS_ERROR_ABORT != smresult)
          {
            NS_ASSERTION(NS_SUCCEEDED(smresult), "bad result");
            smresult = sm2->YMost(yMost2);
            NS_ASSERTION(NS_SUCCEEDED(smresult), "bad result");
            if (yMost1 != yMost2) {
              LogVerifyMessage(k1, k2, "yMost of space managers differs\n");
            }
            // now compare bands by sampling
            PRInt32 yIncrement = yMost1/100;
            if (0==yIncrement) {
              yIncrement = 1;   // guarantee we make progress in the loop below
            }
            nscoord yOffset = 0;
            for ( ; ok && yOffset < yMost1; yOffset += yIncrement)
            {
              nscoord small=5, large=100;
              nsBandData band1, band2;
              nsBandTrapezoid trap1[20], trap2[20];
              band1.mSize = band2.mSize = 20;
              band1.mTrapezoids = trap1;  
              band2.mTrapezoids = trap2;
              sm1->GetBandData(yOffset, nsSize(small,small), band1);
              sm2->GetBandData(yOffset, nsSize(small,small), band2);
              if (band1.mCount != band2.mCount) 
              { // count mismatch, stop comparing
                LogVerifyMessage(k1, k2, "band.mCount of space managers differs\n");
                printf("count1= %d, count2=%d, yOffset = %d, size=%d\n",
                        band1.mCount, band2.mCount, yOffset, small);
                ok = PR_FALSE;
                      
              }
              else   // band counts match, compare individual traps
              { 
                PRInt32 trapIndex=0;
                for ( ;trapIndex<band1.mCount; trapIndex++)
                {
                  PRBool match = (trap1[trapIndex].EqualGeometry(trap2[trapIndex])) && 
                    trap1[trapIndex].mState == trap2[trapIndex].mState;
                  if (!match)
                  {
                    LogVerifyMessage(k1, k2, "band.mTrapezoids of space managers differs\n");
                    printf ("index %d\n", trapIndex);
                  }
                }
              }
              // test the larger maxSize
              sm1->GetBandData(yOffset, nsSize(large,large), band1);
              sm2->GetBandData(yOffset, nsSize(large,large), band2);
              if (band1.mCount != band2.mCount) 
              { // count mismatch, stop comparing
                LogVerifyMessage(k1, k2, "band.mCount of space managers differs\n");
                printf("count1= %d, count2=%d, yOffset = %d, size=%d\n",
                        band1.mCount, band2.mCount, yOffset, small);
                ok = PR_FALSE;
                      
              }
              else   // band counts match, compare individual traps
              { 
                PRInt32 trapIndex=0;
                for ( ; trapIndex<band1.mCount; trapIndex++)
                {
                  PRBool match = (trap1[trapIndex].EqualGeometry(trap2[trapIndex])) && 
                    trap1[trapIndex].mState == trap2[trapIndex].mState;
                  if (!match)
                  {
                    LogVerifyMessage(k1, k2, "band.mTrapezoids of space managers differs\n");
                    printf ("index %d\n", trapIndex);
                  }
                }
              }
            }
          }
        }




        // Compare the sub-trees too
        if (!CompareTrees(aFirstPresContext, k1, aSecondPresContext, k2)) {
          ok = PR_FALSE;
          if (0 == (VERIFY_REFLOW_ALL & gVerifyReflowFlags)) {
            break;
          }
        }

        // Advance to next sibling
        k1->GetNextSibling(&k1);
        k2->GetNextSibling(&k2);
      }
      else {
        break;
      }
    }
    if (!ok && (0 == (VERIFY_REFLOW_ALL & gVerifyReflowFlags))) {
      break;
    }
    NS_IF_RELEASE(listName);

    nsIAtom* listName1;
    nsIAtom* listName2;
    aFirstFrame->GetAdditionalChildListName(listIndex, &listName1);
    aSecondFrame->GetAdditionalChildListName(listIndex, &listName2);
    listIndex++;
    if (listName1 != listName2) {
      if (0 == (VERIFY_REFLOW_ALL & gVerifyReflowFlags)) {
        ok = PR_FALSE;
      }
      LogVerifyMessage(k1, k2, "child list names are not matched: ");
      nsAutoString tmp;
      if (nsnull != listName1) {
        listName1->ToString(tmp);
        fputs(NS_LossyConvertUCS2toASCII(tmp).get(), stdout);
      }
      else
        fputs("(null)", stdout);
      printf(" != ");
      if (nsnull != listName2) {
        listName2->ToString(tmp);
        fputs(NS_LossyConvertUCS2toASCII(tmp).get(), stdout);
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
  } while (ok && (listName != nsnull));

  return ok;
}
#endif

#if 0
static nsIFrame*
FindTopFrame(nsIFrame* aRoot)
{
  if (nsnull != aRoot) {
    nsIContent* content;
    aRoot->GetContent(&content);
    if (nsnull != content) {
      nsIAtom* tag;
      content->GetTag(tag);
      if (nsnull != tag) {
        NS_RELEASE(tag);
        NS_RELEASE(content);
        return aRoot;
      }
      NS_RELEASE(content);
    }

    // Try one of the children
    nsIFrame* kid;
    aRoot->FirstChild(nsnull, &kid);
    while (nsnull != kid) {
      nsIFrame* result = FindTopFrame(kid);
      if (nsnull != result) {
        return result;
      }
      kid->GetNextSibling(&kid);
    }
  }
  return nsnull;
}
#endif


#ifdef DEBUG
// After an incremental reflow, we verify the correctness by doing a
// full reflow into a fresh frame tree.
PRBool
PresShell::VerifyIncrementalReflow()
{
   if (VERIFY_REFLOW_NOISY & gVerifyReflowFlags) {
     printf("Building Verification Tree...\n");
   }
  // All the stuff we are creating that needs releasing
  nsIPresContext* cx;
  nsIViewManager* vm;
  nsIPresShell* sh;

  // Create a presentation context to view the new frame tree
  nsresult rv;
  PRBool isPaginated = PR_FALSE;
  mPresContext->IsPaginated(&isPaginated);
  if (isPaginated) {
    nsCOMPtr<nsIPrintPreviewContext> ppx = do_CreateInstance(kPrintPreviewContextCID, &rv);
    if (NS_SUCCEEDED(rv)) {
      ppx->QueryInterface(NS_GET_IID(nsIPresContext),(void**)&cx);
    }
  }
  else {
    rv = NS_NewGalleyContext(&cx);
  }

  nsISupports* container;
  if (NS_SUCCEEDED(mPresContext->GetContainer(&container)) &&
      (nsnull != container)) {
    cx->SetContainer(container);
    nsILinkHandler* lh;
    if (NS_SUCCEEDED(container->QueryInterface(NS_GET_IID(nsILinkHandler),
                                               (void**)&lh))) {
      cx->SetLinkHandler(lh);
      NS_RELEASE(lh);
    }
    NS_RELEASE(container);
  }

  NS_ASSERTION(NS_OK == rv, "failed to create presentation context");
  nsCOMPtr<nsIDeviceContext> dc;
  mPresContext->GetDeviceContext(getter_AddRefs(dc));
  cx->Init(dc);

  // Get our scrolling preference
  nsScrollPreference scrolling;
  nsIView* rootView;
  mViewManager->GetRootView(rootView);
  nsIScrollableView* scrollView;
  rv = rootView->QueryInterface(NS_GET_IID(nsIScrollableView),
                                (void**)&scrollView);
  if (NS_OK == rv) {
    scrollView->GetScrollPreference(scrolling);
  }
  nsCOMPtr<nsIWidget> rootWidget;
  rootView->GetWidget(*getter_AddRefs(rootWidget));
  void* nativeParentWidget = rootWidget->GetNativeData(NS_NATIVE_WIDGET);

  // Create a new view manager.
  rv = nsComponentManager::CreateInstance(kViewManagerCID, nsnull,
                                          NS_GET_IID(nsIViewManager),
                                          (void**) &vm);
  if (NS_FAILED(rv)) {
    NS_ASSERTION(NS_OK == rv, "failed to create view manager");
  }
  rv = vm->Init(dc);
  if (NS_FAILED(rv)) {
    NS_ASSERTION(NS_OK == rv, "failed to init view manager");
  }

  // Create a child window of the parent that is our "root view/window"
  // Create a view
  nsRect tbounds;
  mPresContext->GetVisibleArea(tbounds);
  nsIView* view;
  rv = nsComponentManager::CreateInstance(kViewCID, nsnull,
                                          NS_GET_IID(nsIView),
                                          (void **) &view);
  if (NS_FAILED(rv)) {
    NS_ASSERTION(NS_OK == rv, "failed to create scroll view");
  }
  rv = view->Init(vm, tbounds, nsnull);
  if (NS_FAILED(rv)) {
    NS_ASSERTION(NS_OK == rv, "failed to init scroll view");
  }

  //now create the widget for the view
  rv = view->CreateWidget(kWidgetCID, nsnull, nativeParentWidget);
  if (NS_OK != rv) {
    NS_ASSERTION(NS_OK == rv, "failed to create scroll view widget");
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
  nsCOMPtr<nsIStyleSet> newSet;
  rv = CloneStyleSet(mStyleSet, getter_AddRefs(newSet));
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to clone style set");
  rv = mDocument->CreateShell(cx, vm, newSet, &sh);
  sh->SetVerifyReflowEnable(PR_FALSE); // turn off verify reflow while we're reflowing the test frame tree
  NS_ASSERTION(NS_OK == rv, "failed to create presentation shell");
  vm->SetViewObserver((nsIViewObserver *)((PresShell*)sh));
  sh->InitialReflow(r.width, r.height);
  sh->SetVerifyReflowEnable(PR_TRUE);  // turn on verify reflow again now that we're done reflowing the test frame tree
  if (VERIFY_REFLOW_NOISY & gVerifyReflowFlags) {
     printf("Verification Tree built, comparing...\n");
  }

  // Now that the document has been reflowed, use its frame tree to
  // compare against our frame tree.
  nsIFrame* root1;
  GetRootFrame(&root1);
  nsIFrame* root2;
  sh->GetRootFrame(&root2);
  PRBool ok = CompareTrees(mPresContext, root1, cx, root2);
  if (!ok && (VERIFY_REFLOW_NOISY & gVerifyReflowFlags)) {
    printf("Verify reflow failed, primary tree:\n");
    nsIFrameDebug*  frameDebug;

    if (NS_SUCCEEDED(root1->QueryInterface(NS_GET_IID(nsIFrameDebug),
                                           (void**)&frameDebug))) {
      frameDebug->List(mPresContext, stdout, 0);
    }
    printf("Verification tree:\n");
    if (NS_SUCCEEDED(root2->QueryInterface(NS_GET_IID(nsIFrameDebug),
                                           (void**)&frameDebug))) {
      frameDebug->List(mPresContext, stdout, 0);
    }
  }

  cx->SetContainer(nsnull);
  NS_RELEASE(cx);
  sh->EndObservingDocument();
  NS_RELEASE(sh);
  NS_RELEASE(vm);
  if (VERIFY_REFLOW_NOISY & gVerifyReflowFlags) {
    printf("Finished Verifying Reflow...\n");
  }

  return ok;
}
#endif

// PresShellViewEventListener

NS_IMPL_ISUPPORTS2(PresShellViewEventListener, nsIScrollPositionListener, nsICompositeListener)

PresShellViewEventListener::PresShellViewEventListener()
{
  NS_INIT_ISUPPORTS();
  mPresShell  = 0;
  mWasVisible = PR_FALSE;
  mCallCount  = 0;
}

PresShellViewEventListener::~PresShellViewEventListener()
{
  mPresShell  = 0;
}

nsresult
PresShellViewEventListener::SetPresShell(nsIPresShell *aPresShell)
{
  mPresShell = aPresShell;
  return NS_OK;
}

nsresult
PresShellViewEventListener::HideCaret()
{
  nsresult result = NS_OK;

  if (mPresShell && 0 == mCallCount)
  {
    nsCOMPtr<nsISelectionController> selCon = do_QueryInterface(mPresShell);
    if (selCon)
    {
      result = selCon->GetCaretEnabled(&mWasVisible);

      if (NS_SUCCEEDED(result) && mWasVisible)
        result = selCon->SetCaretEnabled(PR_FALSE);
    }
  }

  ++mCallCount;

  return result;
}

nsresult
PresShellViewEventListener::RestoreCaretVisibility()
{
  nsresult result = NS_OK;

  --mCallCount;

  if (mPresShell && 0 == mCallCount && mWasVisible)
  {
    nsCOMPtr<nsISelectionController> selCon = do_QueryInterface(mPresShell);
    if (selCon)
      result = selCon->SetCaretEnabled(PR_TRUE);
  }

  return result;
}

NS_IMETHODIMP
PresShellViewEventListener::ScrollPositionWillChange(nsIScrollableView *aView, nscoord aX, nscoord aY)
{
  return HideCaret();
}

NS_IMETHODIMP
PresShellViewEventListener::ScrollPositionDidChange(nsIScrollableView *aView, nscoord aX, nscoord aY)
{
  return RestoreCaretVisibility();
}

NS_IMETHODIMP
PresShellViewEventListener::WillRefreshRegion(nsIViewManager *aViewManager,
                                     nsIView *aView,
                                     nsIRenderingContext *aContext,
                                     nsIRegion *aRegion,
                                     PRUint32 aUpdateFlags)
{
  return HideCaret();
}

NS_IMETHODIMP
PresShellViewEventListener::DidRefreshRegion(nsIViewManager *aViewManager,
                                    nsIView *aView,
                                    nsIRenderingContext *aContext,
                                    nsIRegion *aRegion,
                                    PRUint32 aUpdateFlags)
{
  return RestoreCaretVisibility();
}

NS_IMETHODIMP
PresShellViewEventListener::WillRefreshRect(nsIViewManager *aViewManager,
                                   nsIView *aView,
                                   nsIRenderingContext *aContext,
                                   const nsRect *aRect,
                                   PRUint32 aUpdateFlags)
{
  return HideCaret();
}

NS_IMETHODIMP
PresShellViewEventListener::DidRefreshRect(nsIViewManager *aViewManager,
                                  nsIView *aView,
                                  nsIRenderingContext *aContext,
                                  const nsRect *aRect,
                                  PRUint32 aUpdateFlags)
{
  return RestoreCaretVisibility();
}

// Enable, Disable and Print, Start, Stop and/or Reset the StyleSet watch
/*static*/
nsresult CtlStyleWatch(PRUint32 aCtlValue, nsIStyleSet *aStyleSet)
{
  NS_ASSERTION(aStyleSet!=nsnull,"aStyleSet cannot be null in CtlStyleWatch");
  nsresult rv = NS_OK;
#ifdef MOZ_PERF_METRICS
  if (aStyleSet != nsnull){
    nsCOMPtr<nsITimeRecorder> watch = do_QueryInterface(aStyleSet, &rv);
    if (NS_SUCCEEDED(rv) && watch) {
      if (aCtlValue & kStyleWatchEnable){
        watch->EnableTimer(NS_TIMER_STYLE_RESOLUTION);
      }
      if (aCtlValue & kStyleWatchDisable){
        watch->DisableTimer(NS_TIMER_STYLE_RESOLUTION);
      }
      if (aCtlValue & kStyleWatchPrint){
        watch->PrintTimer(NS_TIMER_STYLE_RESOLUTION);
      }
      if (aCtlValue & kStyleWatchStart){
        watch->StartTimer(NS_TIMER_STYLE_RESOLUTION);
      }
      if (aCtlValue & kStyleWatchStop){
        watch->StopTimer(NS_TIMER_STYLE_RESOLUTION);
      }
      if (aCtlValue & kStyleWatchReset){
        watch->ResetTimer(NS_TIMER_STYLE_RESOLUTION);
      }
    }
  }
#endif
  return rv;  
}


//=============================================================
//=============================================================
//-- Debug Reflow Counts
//=============================================================
//=============================================================
#ifdef MOZ_REFLOW_PERF
//-------------------------------------------------------------
NS_IMETHODIMP
PresShell::DumpReflows()
{
  if (mReflowCountMgr) {
    nsCAutoString uriStr;
    if (mDocument) {
      nsCOMPtr<nsIURI> uri;
      mDocument->GetDocumentURL(getter_AddRefs(uri));
      if (uri) {
        uri->GetPath(uriStr);
      }
    }
    mReflowCountMgr->DisplayTotals(uriStr.get());
    mReflowCountMgr->DisplayHTMLTotals(uriStr.get());
    mReflowCountMgr->DisplayDiffsInTotals("Differences");
  }
  return NS_OK;
}

//-------------------------------------------------------------
NS_IMETHODIMP
PresShell::CountReflows(const char * aName, PRUint32 aType, nsIFrame * aFrame)
{
  if (mReflowCountMgr) {
    mReflowCountMgr->Add(aName, (nsReflowReason)aType, aFrame);
  }

  return NS_OK;
}

//-------------------------------------------------------------
NS_IMETHODIMP
PresShell::PaintCount(const char * aName, nsIRenderingContext* aRenderingContext, nsIPresContext* aPresContext, nsIFrame * aFrame, PRUint32 aColor)
{
  if (mReflowCountMgr) {
    mReflowCountMgr->PaintCount(aName, aRenderingContext, aPresContext, aFrame, aColor);
  }
  return NS_OK;
}

//-------------------------------------------------------------
NS_IMETHODIMP
PresShell::SetPaintFrameCount(PRBool aPaintFrameCounts)
{ 
  if (mReflowCountMgr) {
    mReflowCountMgr->SetPaintFrameCounts(aPaintFrameCounts);
  }
  return NS_OK; 
}

//------------------------------------------------------------------
//-- Reflow Counter Classes Impls
//------------------------------------------------------------------

//------------------------------------------------------------------
ReflowCounter::ReflowCounter(ReflowCountMgr * aMgr) :
  mMgr(aMgr)
{
  ClearTotals();
  SetTotalsCache();
}

//------------------------------------------------------------------
ReflowCounter::~ReflowCounter()
{
  
}

//------------------------------------------------------------------
void ReflowCounter::ClearTotals()
{
  for (PRUint32 i=0;i<NUM_REFLOW_TYPES;i++) {
    mTotals[i] = 0;
  }
}

//------------------------------------------------------------------
void ReflowCounter::SetTotalsCache()
{
  for (PRUint32 i=0;i<NUM_REFLOW_TYPES;i++) {
    mCacheTotals[i] = mTotals[i];
  }
}

//------------------------------------------------------------------
void ReflowCounter::CalcDiffInTotals()
{
  for (PRUint32 i=0;i<NUM_REFLOW_TYPES;i++) {
    mCacheTotals[i] = mTotals[i] - mCacheTotals[i];
  }
}

//------------------------------------------------------------------
void ReflowCounter::DisplayTotals(const char * aStr)
{
  DisplayTotals(mTotals, aStr?aStr:"Totals");
}

//------------------------------------------------------------------
void ReflowCounter::DisplayDiffTotals(const char * aStr)
{
  DisplayTotals(mCacheTotals, aStr?aStr:"Diff Totals");
}

//------------------------------------------------------------------
void ReflowCounter::DisplayHTMLTotals(const char * aStr)
{
  DisplayHTMLTotals(mTotals, aStr?aStr:"Totals");
}

//------------------------------------------------------------------
void ReflowCounter::DisplayTotals(PRUint32 * aArray, const char * aTitle)
{
  // figure total
  PRUint32 total = 0;
  PRUint32 i;
  for (i=0;i<NUM_REFLOW_TYPES;i++) {
    total += aArray[i];
  }

  if (total == 0) {
    return;
  }
  ReflowCounter * gTots = (ReflowCounter *)mMgr->LookUp(kGrandTotalsStr);

  printf("%25s\t", aTitle);
  for (i=0;i<NUM_REFLOW_TYPES;i++) {
    printf("%d\t", aArray[i]);
    if (gTots != this &&  aArray[i] > 0) {
      gTots->Add((nsReflowReason)i, aArray[i]);
    }
  }
  printf("%d\n", total);
}

//------------------------------------------------------------------
void ReflowCounter::DisplayHTMLTotals(PRUint32 * aArray, const char * aTitle)
{
  // figure total
  PRUint32 total = 0;
  PRUint32 i;
  for (i=0;i<NUM_REFLOW_TYPES;i++) {
    total += aArray[i];
  }

  if (total == 0) {
    return;
  }

  ReflowCounter * gTots = (ReflowCounter *)mMgr->LookUp(kGrandTotalsStr);
  FILE * fd = mMgr->GetOutFile();
  if (!fd) {
    return;
  }

  fprintf(fd, "<tr><td><center>%s</center></td>", aTitle);
  for (i=0;i<NUM_REFLOW_TYPES;i++) {
    fprintf(fd, "<td><center>");
    if (aArray[i]) {
      fprintf(fd, "%d", aArray[i]);
    } else {
      fprintf(fd, "&nbsp;");
    }
    fprintf(fd, "</center></td>");
    if (gTots != this &&  aArray[i] > 0) {
      gTots->Add((nsReflowReason)i, aArray[i]);
    }
  }
  fprintf(fd, "<td><center>%d</center></td></tr>\n", total);
}

//------------------------------------------------------------------
//-- ReflowCountMgr
//------------------------------------------------------------------
ReflowCountMgr::ReflowCountMgr()
{
  mCounts = PL_NewHashTable(10, PL_HashString, PL_CompareStrings, 
                                PL_CompareValues, nsnull, nsnull);
  mIndiFrameCounts = PL_NewHashTable(10, PL_HashString, PL_CompareStrings, 
                                     PL_CompareValues, nsnull, nsnull);
  mCycledOnce              = PR_FALSE;
  mDumpFrameCounts         = PR_FALSE;
  mDumpFrameByFrameCounts  = PR_FALSE;
  mPaintFrameByFrameCounts = PR_FALSE;
}

//------------------------------------------------------------------
ReflowCountMgr::~ReflowCountMgr()
{
  CleanUp();
}

//------------------------------------------------------------------
ReflowCounter * ReflowCountMgr::LookUp(const char * aName)
{
  if (nsnull != mCounts) {
    ReflowCounter * counter = (ReflowCounter *)PL_HashTableLookup(mCounts, aName);
    return counter;
  }
  return nsnull;

}

//------------------------------------------------------------------
void ReflowCountMgr::Add(const char * aName, nsReflowReason aType, nsIFrame * aFrame)
{
  NS_ASSERTION(aName != nsnull, "Name shouldn't be null!");

  if (mDumpFrameCounts && nsnull != mCounts) {
    ReflowCounter * counter = (ReflowCounter *)PL_HashTableLookup(mCounts, aName);
    if (counter == nsnull) {
      counter = new ReflowCounter(this);
      NS_ASSERTION(counter != nsnull, "null ptr");
      char * name = nsCRT::strdup(aName);
      NS_ASSERTION(name != nsnull, "null ptr");
      PL_HashTableAdd(mCounts, name, counter);
    }
    counter->Add(aType);
  }

  if ((mDumpFrameByFrameCounts || mPaintFrameByFrameCounts) && 
      nsnull != mIndiFrameCounts && 
      aFrame != nsnull) {
    char * key = new char[16];
    sprintf(key, "%p", (void*)aFrame);
    IndiReflowCounter * counter = (IndiReflowCounter *)PL_HashTableLookup(mIndiFrameCounts, key);
    if (counter == nsnull) {
      counter = new IndiReflowCounter(this);
      NS_ASSERTION(counter != nsnull, "null ptr");
      counter->mFrame = aFrame;
      counter->mName.AssignWithConversion(aName);
      PL_HashTableAdd(mIndiFrameCounts, key, counter);
    }
    // this eliminates extra counts from super classes
    if (counter != nsnull && counter->mName.EqualsWithConversion(aName)) {
      counter->mCount++;
      counter->mCounter.Add(aType, 1);
    }
  }
}

//------------------------------------------------------------------
void ReflowCountMgr::PaintCount(const char *    aName, 
                                nsIRenderingContext* aRenderingContext, 
                                nsIPresContext* aPresContext, 
                                nsIFrame*       aFrame, 
                                PRUint32        aColor)
{
  if (mPaintFrameByFrameCounts && 
      nsnull != mIndiFrameCounts && 
      aFrame != nsnull) {
    char * key = new char[16];
    sprintf(key, "%p", (void*)aFrame);
    IndiReflowCounter * counter = (IndiReflowCounter *)PL_HashTableLookup(mIndiFrameCounts, key);
    if (counter != nsnull && counter->mName.EqualsWithConversion(aName)) {
      aRenderingContext->PushState();
      nsFont font("Times", NS_FONT_STYLE_NORMAL,NS_FONT_VARIANT_NORMAL,
						       NS_FONT_WEIGHT_NORMAL,0,NSIntPointsToTwips(8));

      nsCOMPtr<nsIFontMetrics> fm;
      aPresContext->GetMetricsFor(font, getter_AddRefs(fm));
      aRenderingContext->SetFont(fm);
      char buf[16];
      sprintf(buf, "%d", counter->mCount);
      nscoord x = 0, y;
      nscoord width, height;
      aRenderingContext->GetWidth((char*)buf, width);
      fm->GetHeight(height);
      fm->GetMaxAscent(y);

      nsRect r;
      aFrame->GetRect(r);

      PRUint32 color;
      PRUint32 color2;
      if (aColor != 0) {
        color  = aColor;
        color2 = NS_RGB(0,0,0);
      } else {
        PRUint8 rc,gc,bc = 0;
        if (counter->mCount < 5) {
          rc = 255;
          gc = 255;
        } else if ( counter->mCount < 11) {
          gc = 255;
        } else {
          rc = 255;
        }
        color  = NS_RGB(rc,gc,bc);
        color2 = NS_RGB(rc/2,gc/2,bc/2);
      }

      nsRect rect(0,0, width+15, height+15);
      aRenderingContext->SetColor(NS_RGB(0,0,0));
      aRenderingContext->FillRect(rect);
      aRenderingContext->SetColor(color2);
      aRenderingContext->DrawString(buf, strlen(buf), x+15,y+15);
      aRenderingContext->SetColor(color);
      aRenderingContext->DrawString(buf, strlen(buf), x,y);

      PRBool clipEmpty;
      aRenderingContext->PopState(clipEmpty);
    }
  }
}

//------------------------------------------------------------------
PRIntn ReflowCountMgr::RemoveItems(PLHashEntry *he, PRIntn i, void *arg)
{
  char *str = (char *)he->key;
  ReflowCounter * counter = (ReflowCounter *)he->value;
  delete counter;
  delete [] str;

  return HT_ENUMERATE_REMOVE;
}

//------------------------------------------------------------------
PRIntn ReflowCountMgr::RemoveIndiItems(PLHashEntry *he, PRIntn i, void *arg)
{
  char *str = (char *)he->key;
  IndiReflowCounter * counter = (IndiReflowCounter *)he->value;
  delete counter;
  delete [] str;

  return HT_ENUMERATE_REMOVE;
}

//------------------------------------------------------------------
void ReflowCountMgr::CleanUp()
{
  if (nsnull != mCounts) {
    PL_HashTableEnumerateEntries(mCounts, RemoveItems, nsnull);
    PL_HashTableDestroy(mCounts);
    mCounts = nsnull;
  }

  if (nsnull != mIndiFrameCounts) {
    PL_HashTableEnumerateEntries(mIndiFrameCounts, RemoveIndiItems, nsnull);
    PL_HashTableDestroy(mIndiFrameCounts);
    mIndiFrameCounts = nsnull;
  }
}

//------------------------------------------------------------------
PRIntn ReflowCountMgr::DoSingleTotal(PLHashEntry *he, PRIntn i, void *arg)
{
  char *str = (char *)he->key;
  ReflowCounter * counter = (ReflowCounter *)he->value;

  counter->DisplayTotals(str);

  return HT_ENUMERATE_NEXT;
}

//------------------------------------------------------------------
void ReflowCountMgr::DoGrandTotals()
{
  if (nsnull != mCounts) {
    ReflowCounter * gTots = (ReflowCounter *)PL_HashTableLookup(mCounts, kGrandTotalsStr);
    if (gTots == nsnull) {
      gTots = new ReflowCounter(this);
      PL_HashTableAdd(mCounts, nsCRT::strdup(kGrandTotalsStr), gTots);
    } else {
      gTots->ClearTotals();
    }

    static const char * title[] = {"Init", "Incrm", "Resze", "Style", "Dirty", "Total"};
    printf("\t\t\t");
    PRUint32 i;
    for (i=0;i<NUM_REFLOW_TYPES+1;i++) {
      printf("\t%s", title[i]);
    }
    printf("\n");
    for (i=0;i<78;i++) {
      printf("-");
    }
    printf("\n");
    PL_HashTableEnumerateEntries(mCounts, DoSingleTotal, this);
  }
}

static void RecurseIndiTotals(nsIPresContext* aPresContext, 
                              PLHashTable *   aHT, 
                              nsIFrame *      aParentFrame,
                              PRInt32         aLevel)
{
  if (aParentFrame == nsnull) {
    return;
  }

  char key[16];
  sprintf(key, "%p", (void*)aParentFrame);
  IndiReflowCounter * counter = (IndiReflowCounter *)PL_HashTableLookup(aHT, key);
  if (counter) {
    counter->mHasBeenOutput = PR_TRUE;
    char * name = ToNewCString(counter->mName);
    for (PRInt32 i=0;i<aLevel;i++) printf(" ");
    printf("%s - %p   [%d][", name, (void*)aParentFrame, counter->mCount);
    for (PRInt32 inx=0;inx<5;inx++) {
      if (inx != 0) printf(",");
      printf("%d", counter->mCounter.GetTotalByType(nsReflowReason(inx)));
    }
    printf("]\n");
    nsMemory::Free(name);
  }

  nsIFrame * child;
  aParentFrame->FirstChild(aPresContext, nsnull, &child);
  while (child) {
    RecurseIndiTotals(aPresContext, aHT, child, aLevel+1);
    child->GetNextSibling(&child);
  }

}

//------------------------------------------------------------------
PRIntn ReflowCountMgr::DoSingleIndi(PLHashEntry *he, PRIntn i, void *arg)
{
  IndiReflowCounter * counter = (IndiReflowCounter *)he->value;
  if (counter && !counter->mHasBeenOutput) {
    char * name = ToNewCString(counter->mName);
    printf("%s - %p   [%d][", name, (void*)counter->mFrame, counter->mCount);
    for (PRInt32 inx=0;inx<5;inx++) {
      if (inx != 0) printf(",");
      printf("%d", counter->mCounter.GetTotalByType(nsReflowReason(inx)));
    }
    printf("]\n");
    nsMemory::Free(name);
  }
  return HT_ENUMERATE_NEXT;
}

//------------------------------------------------------------------
void ReflowCountMgr::DoIndiTotalsTree()
{
  if (nsnull != mCounts) {
    printf("\n------------------------------------------------\n");
    printf("-- Individual Frame Counts\n");
    printf("------------------------------------------------\n");

    if (mPresShell) {
      nsIFrame * rootFrame;
      mPresShell->GetRootFrame(&rootFrame);
      RecurseIndiTotals(mPresContext, mIndiFrameCounts, rootFrame, 0);
      printf("------------------------------------------------\n");
      printf("-- Individual Counts of Frames not in Root Tree\n");
      printf("------------------------------------------------\n");
      PL_HashTableEnumerateEntries(mIndiFrameCounts, DoSingleIndi, this);
    }
  }
}

//------------------------------------------------------------------
PRIntn ReflowCountMgr::DoSingleHTMLTotal(PLHashEntry *he, PRIntn i, void *arg)
{
  char *str = (char *)he->key;
  ReflowCounter * counter = (ReflowCounter *)he->value;

  counter->DisplayHTMLTotals(str);

  return HT_ENUMERATE_NEXT;
}

//------------------------------------------------------------------
void ReflowCountMgr::DoGrandHTMLTotals()
{
  if (nsnull != mCounts) {
    ReflowCounter * gTots = (ReflowCounter *)PL_HashTableLookup(mCounts, kGrandTotalsStr);
    if (gTots == nsnull) {
      gTots = new ReflowCounter(this);
      PL_HashTableAdd(mCounts, nsCRT::strdup(kGrandTotalsStr), gTots);
    } else {
      gTots->ClearTotals();
    }

    static const char * title[] = {"Class", "Init", "Incrm", "Resze", "Style", "Dirty", "Total"};
    fprintf(mFD, "<tr>");
    PRUint32 i;
    for (i=0;i<NUM_REFLOW_TYPES+2;i++) {
      fprintf(mFD, "<td><center><b>%s<b></center></td>", title[i]);
    }
    fprintf(mFD, "</tr>\n");
    PL_HashTableEnumerateEntries(mCounts, DoSingleHTMLTotal, this);
  }
}

//------------------------------------
void ReflowCountMgr::DisplayTotals(const char * aStr)
{
#ifdef DEBUG_rods
  printf("%s\n", aStr?aStr:"No name");
#endif
  if (mDumpFrameCounts) {
    DoGrandTotals();
  }
  if (mDumpFrameByFrameCounts) {
    DoIndiTotalsTree();
  }

}
//------------------------------------
void ReflowCountMgr::DisplayHTMLTotals(const char * aStr)
{
#ifdef WIN32x // XXX NOT XP!
  char name[1024];
  
  char * sptr = strrchr(aStr, '/');
  if (sptr) {
    sptr++;
    strcpy(name, sptr);
    char * eptr = strrchr(name, '.');
    if (eptr) {
      *eptr = 0;
    }
    strcat(name, "_stats.html");
  }
  mFD = fopen(name, "w");
  if (mFD) {
    fprintf(mFD, "<html><head><title>Reflow Stats</title></head><body>\n");
    const char * title = aStr?aStr:"No name";
    fprintf(mFD, "<center><b>%s</b><br><table border=1 style=\"background-color:#e0e0e0\">", title);
    DoGrandHTMLTotals();
    fprintf(mFD, "</center></table>\n");
    fprintf(mFD, "</body></html>\n");
    fclose(mFD);
    mFD = nsnull;
  }
#endif // not XP!
}

//------------------------------------------------------------------
PRIntn ReflowCountMgr::DoClearTotals(PLHashEntry *he, PRIntn i, void *arg)
{
  ReflowCounter * counter = (ReflowCounter *)he->value;
  counter->ClearTotals();

  return HT_ENUMERATE_NEXT;
}

//------------------------------------------------------------------
void ReflowCountMgr::ClearTotals()
{
  PL_HashTableEnumerateEntries(mCounts, DoClearTotals, this);
}

//------------------------------------------------------------------
void ReflowCountMgr::ClearGrandTotals()
{
  if (nsnull != mCounts) {
    ReflowCounter * gTots = (ReflowCounter *)PL_HashTableLookup(mCounts, kGrandTotalsStr);
    if (gTots == nsnull) {
      gTots = new ReflowCounter(this);
      PL_HashTableAdd(mCounts, nsCRT::strdup(kGrandTotalsStr), gTots);
    } else {
      gTots->ClearTotals();
      gTots->SetTotalsCache();
    }
  }
}

//------------------------------------------------------------------
PRIntn ReflowCountMgr::DoDisplayDiffTotals(PLHashEntry *he, PRIntn i, void *arg)
{
  PRBool cycledOnce = (arg != 0);

  char *str = (char *)he->key;
  ReflowCounter * counter = (ReflowCounter *)he->value;

  if (cycledOnce) {
    counter->CalcDiffInTotals();
    counter->DisplayDiffTotals(str);
  }
  counter->SetTotalsCache();

  return HT_ENUMERATE_NEXT;
}

//------------------------------------------------------------------
void ReflowCountMgr::DisplayDiffsInTotals(const char * aStr)
{
  if (mCycledOnce) {
    printf("Differences\n");
    for (PRInt32 i=0;i<78;i++) {
      printf("-");
    }
    printf("\n");
    ClearGrandTotals();
  }
  PL_HashTableEnumerateEntries(mCounts, DoDisplayDiffTotals, (void *)mCycledOnce);

  mCycledOnce = PR_TRUE;
}

#endif // MOZ_REFLOW_PERF

// make a color string like #RRGGBB
void ColorToString(nscolor aColor, nsAutoString &aString)
{
  nsAutoString tmp;
  aString.SetLength(0);

  // #
  aString.Append(NS_LITERAL_STRING("#"));
  // RR
  tmp.AppendInt((PRInt32)NS_GET_R(aColor), 16);
  if (tmp.Length() < 2) tmp.AppendInt(0,16);
  aString.Append(tmp);
  tmp.SetLength(0);
  // GG
  tmp.AppendInt((PRInt32)NS_GET_G(aColor), 16);
  if (tmp.Length() < 2) tmp.AppendInt(0,16);
  aString.Append(tmp);
  tmp.SetLength(0);
  // BB
  tmp.AppendInt((PRInt32)NS_GET_B(aColor), 16);
  if (tmp.Length() < 2) tmp.AppendInt(0,16);
  aString.Append(tmp);
}



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
#include "nsISpaceManager.h"
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
#include "nsIReflowCommand.h"
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
#include "nsHTMLIIDs.h"
#include "nsIDeviceContext.h"
#include "nsIEventStateManager.h"
#include "nsIDOMEvent.h"
#include "nsGUIEvent.h"
#include "nsHTMLParts.h"
#include "nsISelection.h"
#include "nsISelectionController.h"
#include "nsLayoutCID.h"
#include "nsLayoutAtoms.h"
#include "nsIDOMRange.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMElement.h"
#include "nsHTMLAtoms.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsWeakReference.h"
#include "nsIPageSequenceFrame.h"
#include "nsICaret.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIXMLDocument.h"
#include "nsIScrollableView.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsIFrameSelection.h"
#include "nsIDOMNSHTMLInputElement.h" //optimization for ::DoXXX commands
#include "nsIDOMNSHTMLTextAreaElement.h"
#include "nsViewsCID.h"
#include "nsIFrameManager.h"
#include "nsISupportsPrimitives.h"
#include "nsILayoutHistoryState.h"
#include "nsIScrollPositionListener.h"
#include "nsICompositeListener.h"
#include "nsILineIterator.h" // for ScrollFrameIntoView
#include "nsTimer.h"
#include "nsWeakPtr.h"
#include "plarena.h"
#include "nsCSSAtoms.h"
#include "nsIObserverService.h" // for reflow observation
#include "nsIDocShell.h"        // for reflow observation
#include "nsIDOMRange.h"
#ifdef MOZ_PERF_METRICS
#include "nsITimeRecorder.h"
#endif
#ifdef NS_DEBUG
#include "nsIFrameDebug.h"
#endif
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

// Dummy layout request
#include "nsIChannel.h"
#include "nsILoadGroup.h"
#include "nsNetUtil.h"

// Content viewer interfaces
#include "nsIContentViewer.h"
#include "nsIDocumentViewer.h"

// SubShell map
#include "nsDST.h"

#ifdef IBMBIDI
#include "nsIBidiKeyboard.h"
#endif // IBMBIDI

#include "nsContentCID.h"
static NS_DEFINE_CID(kCSSStyleSheetCID, NS_CSS_STYLESHEET_CID);
static NS_DEFINE_CID(kStyleSetCID, NS_STYLESET_CID);
static NS_DEFINE_IID(kRangeCID,     NS_RANGE_CID);

// supporting bugs 31816, 20760, 22963
// define USE_OVERRIDE to put prefs in as an override stylesheet
// otherwise they go in as a Backstop stylesheets
//  - OVERRIDE is better for text and bg colors, but bad for link colors,
//    so eventually, we should probably have a backstop and an override and 
//    put the link colors in the backstop and the text and bg colors in the override,
//    but using the backstop stylesheet with !important rules solves 95% of the 
//    problem and should suffice for RTM
//
// XXX: use backstop stylesheet of link colors and link underline, 
//      user override stylesheet for forcing background and text colors, post RTM
//
// #define PREFS_USE_OVERRIDE

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
static NS_DEFINE_CID(kCRangeCID, NS_RANGE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID,   NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kViewCID, NS_VIEW_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

// Drag & Drop, Clipboard Support
static NS_DEFINE_CID(kCClipboardCID,           NS_CLIPBOARD_CID);
static NS_DEFINE_CID(kCTransferableCID,        NS_TRANSFERABLE_CID);
static NS_DEFINE_CID(kHTMLConverterCID,        NS_HTMLFORMATCONVERTER_CID);

#undef NOISY

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
  IndiReflowCounter(ReflowCountMgr * aMgr = nsnull):mMgr(aMgr),mCounter(aMgr),mFrame(nsnull), mCount(0), mHasBeenOutput(PR_FALSE) {}
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

// The upper bound on the amount of time to spend reflowing.  When this bound is exceeded
// and reflow commands are still queued up, a reflow event is posted.  The idea is for reflow
// to not hog the processor beyond the time specifed in gMaxRCProcessingTime.
// This data member is initialized from the layout.reflow.timeslice pref.
#define NS_MAX_REFLOW_TIME    1000000
static PRInt32 gMaxRCProcessingTime = -1;

// Set to true to enable async reflow during document load.
// This flag is initialized from the layout.reflow.async.duringDocLoad pref.
static PRBool gAsyncReflowDuringDocLoad = PR_FALSE;

// Largest chunk size we recycle
static const size_t gMaxRecycledSize = 400;

#define MARK_INCREMENT 50
#define BLOCK_INCREMENT 2048

/**A block of memory that the stack will 
 * chop up and hand out
 */
struct StackBlock {
   
   // a block of memory
   void* mBlock;

   // another block of memory that would only be created
   // if our stack overflowed. Yes we have the ability
   // to grow on a stack overflow
   StackBlock* mNext;
   StackBlock()
   {
      mBlock = PR_Malloc(BLOCK_INCREMENT);
      mNext = nsnull;
   }

   ~StackBlock()
   {
     PR_Free(mBlock);
   }
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
    nsCRT::memcpy(mMarks, oldMarks, sizeof(StackMark)*oldLength);

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
  *aResult = ((char*)mCurBlock->mBlock) + mPos;
  mPos += aSize;

  return NS_OK;
}

nsresult
StackArena::Pop()
{
  // pop off the mark
  NS_ASSERTION(mStackTop > 0, "Error Pop called 1 too many times");
  mStackTop--;
  mCurBlock = mMarks[mStackTop].mBlock;
  mPos      = mMarks[mStackTop].mPos;

  return NS_OK;
}


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
  // Underlying arena pool
  PLArenaPool mPool;

  // The recycler array is sparse with the indices being multiples of 4,
  // i.e., 0, 4, 8, 12, 16, 20, ...
  void*       mRecyclers[gMaxRecycledSize >> 2];
};

FrameArena::FrameArena(PRUint32 aArenaSize)
{
  // Initialize the arena pool
  PL_INIT_ARENA_POOL(&mPool, "FrameArena", aArenaSize);

  // Zero out the recyclers array
  nsCRT::memset(mRecyclers, 0, sizeof(mRecyclers));
}

FrameArena::~FrameArena()
{
  // Free the arena in the pool and finish using it
  PL_FinishArenaPool(&mPool);
} 

nsresult
FrameArena::AllocateFrame(size_t aSize, void** aResult)
{
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
  return NS_OK;
}

nsresult
FrameArena::FreeFrame(size_t aSize, void* aPtr)
{
  // Ensure we have correct alignment for pointers.  Important for Tru64
  aSize = PR_ROUNDUP(aSize, sizeof(void*));

  // See if it's a size that we recycle
  if (aSize < gMaxRecycledSize) {
    const int   index = aSize >> 2;
    void*       currentTop = mRecyclers[index];
    mRecyclers[index] = aPtr;
    *((void**)aPtr) = currentTop;
  }

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

//----------------------------------------------------------------------
//
// DummyLayoutRequest
//
//   This is a dummy request implementation that we add to the document's load
//   group. It ensures that EndDocumentLoad() in the docshell doesn't fire
//   before we've finished all of layout.
//

class DummyLayoutRequest : public nsIChannel
{
protected:
  DummyLayoutRequest(nsIPresShell* aPresShell);
  virtual ~DummyLayoutRequest();

  static PRInt32 gRefCnt;
  static nsIURI* gURI;

  nsCOMPtr<nsILoadGroup> mLoadGroup;
  nsWeakPtr mPresShell;

public:
  static nsresult
  Create(nsIRequest** aResult, nsIPresShell* aPresShell);

  NS_DECL_ISUPPORTS

	// nsIRequest
  NS_IMETHOD GetName(PRUnichar* *result) { 
    *result = ToNewUnicode(NS_LITERAL_STRING("about:layout-dummy-request"));
    return NS_OK;
  }
  NS_IMETHOD IsPending(PRBool *_retval) { *_retval = PR_TRUE; return NS_OK; }
  NS_IMETHOD GetStatus(nsresult *status) { *status = NS_OK; return NS_OK; } 
  NS_IMETHOD Cancel(nsresult status);
  NS_IMETHOD Suspend(void) { return NS_OK; }
  NS_IMETHOD Resume(void)  { return NS_OK; }

 	// nsIChannel
  NS_IMETHOD GetOriginalURI(nsIURI* *aOriginalURI) { *aOriginalURI = gURI; NS_ADDREF(*aOriginalURI); return NS_OK; }
  NS_IMETHOD SetOriginalURI(nsIURI* aOriginalURI) { gURI = aOriginalURI; NS_ADDREF(gURI); return NS_OK; }
  NS_IMETHOD GetURI(nsIURI* *aURI) { *aURI = gURI; NS_ADDREF(*aURI); return NS_OK; }
  NS_IMETHOD SetURI(nsIURI* aURI) { gURI = aURI; NS_ADDREF(gURI); return NS_OK; }
  NS_IMETHOD Open(nsIInputStream **_retval) { *_retval = nsnull; return NS_OK; }
  NS_IMETHOD AsyncOpen(nsIStreamListener *listener, nsISupports *ctxt) { return NS_OK; }
  NS_IMETHOD GetLoadFlags(nsLoadFlags *aLoadFlags) { *aLoadFlags = nsIRequest::LOAD_NORMAL; return NS_OK; }
  NS_IMETHOD SetLoadFlags(nsLoadFlags aLoadFlags) { return NS_OK; }
  NS_IMETHOD GetOwner(nsISupports * *aOwner) { *aOwner = nsnull; return NS_OK; }
  NS_IMETHOD SetOwner(nsISupports * aOwner) { return NS_OK; }
  NS_IMETHOD GetLoadGroup(nsILoadGroup * *aLoadGroup) { *aLoadGroup = mLoadGroup; NS_IF_ADDREF(*aLoadGroup); return NS_OK; }
  NS_IMETHOD SetLoadGroup(nsILoadGroup * aLoadGroup) { mLoadGroup = aLoadGroup; return NS_OK; }
  NS_IMETHOD GetNotificationCallbacks(nsIInterfaceRequestor * *aNotificationCallbacks) { *aNotificationCallbacks = nsnull; return NS_OK; }
  NS_IMETHOD SetNotificationCallbacks(nsIInterfaceRequestor * aNotificationCallbacks) { return NS_OK; }
  NS_IMETHOD GetSecurityInfo(nsISupports * *aSecurityInfo) { *aSecurityInfo = nsnull; return NS_OK; } 
  NS_IMETHOD GetContentType(char * *aContentType) { *aContentType = nsnull; return NS_OK; } 
  NS_IMETHOD SetContentType(const char * aContentType) { return NS_OK; } 
  NS_IMETHOD GetContentLength(PRInt32 *aContentLength) { return NS_OK; }
  NS_IMETHOD SetContentLength(PRInt32 aContentLength) { return NS_OK; }

};

PRInt32 DummyLayoutRequest::gRefCnt;
nsIURI* DummyLayoutRequest::gURI;

NS_IMPL_ADDREF(DummyLayoutRequest);
NS_IMPL_RELEASE(DummyLayoutRequest);
NS_IMPL_QUERY_INTERFACE2(DummyLayoutRequest, nsIRequest, nsIChannel);

nsresult
DummyLayoutRequest::Create(nsIRequest** aResult, nsIPresShell* aPresShell)
{
  DummyLayoutRequest* request = new DummyLayoutRequest(aPresShell);
  if (!request)
      return NS_ERROR_OUT_OF_MEMORY;

  return request->QueryInterface(NS_GET_IID(nsIRequest), (void**) aResult); 
}


DummyLayoutRequest::DummyLayoutRequest(nsIPresShell* aPresShell)
{
  NS_INIT_REFCNT();

  if (gRefCnt++ == 0) {
      nsresult rv;
      rv = NS_NewURI(&gURI, "about:layout-dummy-request", nsnull);
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create about:layout-dummy-request");
  }

  mPresShell = getter_AddRefs(NS_GetWeakReference(aPresShell));
}


DummyLayoutRequest::~DummyLayoutRequest()
{
  if (--gRefCnt == 0) {
      NS_IF_RELEASE(gURI);
  }
}

NS_IMETHODIMP
DummyLayoutRequest::Cancel(nsresult status)
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
//   A generic nsDSTNodeFunctor to look for a specific value in the map

class nsSearchEnumerator : public nsDSTNodeFunctor
{
public:
  nsSearchEnumerator(void* aTargetValue)
    : mTargetValue(aTargetValue) { 
    mResultKey = nsnull;
  }
  
  void operator() (void* aKey, void* aValue) {
    if (aValue == mTargetValue)
      mResultKey = aKey;
  }
  
  void* GetResult() { return mResultKey; }

private:
  void* mResultKey;
  void* mTargetValue;
};

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
                  nsIStyleSet* aStyleSet);
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
  NS_IMETHOD SetPreferenceStyleRules(PRBool aForceReflow);
  NS_IMETHOD EnablePrefStyleRules(PRBool aEnable, PRUint8 aPrefType=0xFF);
  NS_IMETHOD ArePrefStyleRulesEnabled(PRBool& aEnabled);

  NS_IMETHOD SetDisplaySelection(PRInt16 aToggle);
  NS_IMETHOD GetDisplaySelection(PRInt16 *aToggle);
  NS_IMETHOD GetSelection(SelectionType aType, nsISelection** aSelection);
  NS_IMETHOD ScrollSelectionIntoView(SelectionType aType, SelectionRegion aRegion);
  NS_IMETHOD RepaintSelection(SelectionType aType);
  NS_IMETHOD GetFrameSelection(nsIFrameSelection** aSelection);  

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
  NS_IMETHOD GetSubShellFor(nsIContent*   aContent,
                            nsISupports** aResult) const;
  NS_IMETHOD SetSubShellFor(nsIContent*  aContent,
                            nsISupports* aSubShell);
  NS_IMETHOD FindContentForShell(nsISupports* aSubShell,
                                 nsIContent** aContent) const;
  NS_IMETHOD GetPlaceholderFrameFor(nsIFrame*  aFrame,
                                    nsIFrame** aPlaceholderFrame) const;
  NS_IMETHOD AppendReflowCommand(nsIReflowCommand* aReflowCommand);
  NS_IMETHOD AppendReflowCommandInternal(nsIReflowCommand* aReflowCommand,
                                         nsVoidArray&      aQueue);   
  NS_IMETHOD CancelReflowCommand(nsIFrame*                     aTargetFrame, 
                                 nsIReflowCommand::ReflowType* aCmdType);  
  NS_IMETHOD CancelReflowCommandInternal(nsIFrame*                     aTargetFrame, 
                                         nsIReflowCommand::ReflowType* aCmdType,
                                         nsVoidArray&                  aQueue,
                                         PRBool                        aProcessDummyLayoutRequest = PR_TRUE);  
  NS_IMETHOD CancelAllReflowCommands();
  NS_IMETHOD IsSafeToFlush(PRBool& aIsSafeToFlush);
  NS_IMETHOD FlushPendingNotifications(PRBool aUpdateViews);

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
  NS_IMETHOD GoToAnchor(const nsString& aAnchorName);

  NS_IMETHOD ScrollFrameIntoView(nsIFrame *aFrame,
                                 PRIntn   aVPercent, 
                                 PRIntn   aHPercent) const;

  NS_IMETHOD NotifyDestroyingFrame(nsIFrame* aFrame);
  
  NS_IMETHOD GetFrameManager(nsIFrameManager** aFrameManager) const;

  NS_IMETHOD DoCopy();
  NS_IMETHOD DoCopyLinkLocation(nsIDOMNode* aNode);
  NS_IMETHOD DoCopyImageLocation(nsIDOMNode* aNode);
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

  NS_IMETHOD SetDisplayNonTextSelection(PRBool aInEnable);
  NS_IMETHOD GetDisplayNonTextSelection(PRBool *aOutEnable);

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
                                  nsIContent* aContent2);
  NS_IMETHOD AttributeChanged(nsIDocument *aDocument,
                              nsIContent*  aContent,
                              PRInt32      aNameSpaceID,
                              nsIAtom*     aAttribute,
                              PRInt32      aModType, 
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

  NS_IMETHOD SendInterruptNotificationTo(nsIFrame*                   aFrame,
                                         nsIPresShell::InterruptType aInterruptType);
  NS_IMETHOD CancelInterruptNotificationTo(nsIFrame*                   aFrame,
                                           nsIPresShell::InterruptType aInterruptType);
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

  nsresult ReflowCommandAdded(nsIReflowCommand* aRC);
  nsresult ReflowCommandRemoved(nsIReflowCommand* aRC);

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
  void     ProcessReflowCommand(nsVoidArray&         aQueue,
                                PRBool               aAccumulateTime,
                                nsHTMLReflowMetrics& aDesiredSize,
                                nsSize&              aMaxSize,
                                nsIRenderingContext& aRenderingContext);
  nsresult ProcessReflowCommands(PRBool aInterruptible);
  nsresult GetReflowEventStatus(PRBool* aPending);
  nsresult SetReflowEventStatus(PRBool aPending);  
  void     PostReflowEvent();
  PRBool   AlreadyInQueue(nsIReflowCommand* aReflowCommand,
                          nsVoidArray&      aQueue);
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

  nsresult SelectContent(nsIContent *aContent);

  // IMPORTANT: The ownership implicit in the following member variables has been 
  // explicitly checked and set using nsCOMPtr for owning pointers and raw COM interface 
  // pointers for weak (ie, non owning) references. If you add any members to this
  // class, please make the ownership explicit (pinkerton, scc).

  // these are the same Document and PresContext owned by the DocViewer.
  // we must share ownership.
  nsCOMPtr<nsIDocument> mDocument;
  nsCOMPtr<nsIPresContext> mPresContext;
  nsCOMPtr<nsIStyleSet> mStyleSet;
  nsICSSStyleSheet* mPrefStyleSheet; // mStyleSet owns it but we maintaina ref, may be null
  PRPackedBool mEnablePrefStyleSheet;
  nsIViewManager* mViewManager;   // [WEAK] docViewer owns it so I don't have to
  nsWeakPtr mHistoryState; // [WEAK] session history owns this
  PRUint32 mUpdateCount;
  // normal reflow commands
  nsVoidArray mReflowCommands; 
  // reflow commands targeted at each aFrame who calls SendInterruptNotificationTo(aFrame, Timeout);
  nsVoidArray mTimeoutReflowCommands; 

  PRPackedBool mDocumentLoading;
  PRPackedBool mIsReflowing;
  PRPackedBool mIsDestroying;
  PRPackedBool mDidInitialReflow;
  nsIFrame* mCurrentEventFrame;
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
  PRBool                        mDisplayNonTextSelection;
  PRBool                        mScrollingEnabled; //used to disable programmable scrolling from outside
  nsIFrameManager*              mFrameManager;  // we hold a reference
  PresShellViewEventListener    *mViewEventListener;
  PRBool                        mPendingReflowEvent;
  nsCOMPtr<nsIEventQueue>       mEventQueue;
  FrameArena                    mFrameArena;
  StackArena*                   mStackArena;
  PRInt32                       mAccumulatedReflowTime;  // Time spent in reflow command processing so far  
  PRPackedBool                  mBatchReflows;  // When set to true, the pres shell batches reflow commands.  
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

  PRBool mIsDocumentGone; // We've been disconnected from the document.
  PRBool mPaintingSuppressed; // For all documents we initially lock down painting.
                              // We will refuse to paint the document until either
                              // (a) our timer fires or (b) all frames are constructed.
  PRBool mShouldUnsuppressPainting; // Indicates that it is safe to unlock painting once all pending
                                    // reflows have been processed.
  nsCOMPtr<nsITimer> mPaintSuppressionTimer; // This timer controls painting suppression.  Until it fires
                                             // or all frames are constructed, we won't paint anything but
                                             // our <body> background and scrollbars.
#define PAINTLOCK_EVENT_DELAY 1200 // 1200 ms.  This is actually pref-controlled, but we use this
                                   // value if we fail to get the pref for any reason.

  static void sPaintSuppressionCallback(nsITimer* aTimer, void* aPresShell); // A callback for the timer.

  // subshell map
  nsDST*            mSubShellMap;  // map of content/subshell pairs
  nsDST::NodeArena* mDSTNodeArena; // weak link. DST owns (mSubShellMap object)

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

PresShell::PresShell():mAnonymousContentTable(nsnull),
                       mStackArena(nsnull),
                       mFirstDOMEventRequest(nsnull),
                       mLastDOMEventRequest(nsnull),
                       mFirstAttributeRequest(nsnull),
                       mLastAttributeRequest(nsnull),
                       mFirstCallbackEventRequest(nsnull),
                       mLastCallbackEventRequest(nsnull),
                       mPaintingSuppressed(PR_FALSE),
                       mShouldUnsuppressPainting(PR_FALSE)
{
  NS_INIT_REFCNT();
  mIsDocumentGone = PR_FALSE;
  mIsDestroying = PR_FALSE;
  mDidInitialReflow = PR_FALSE;
  mCaretEnabled = PR_FALSE;
  mDisplayNonTextSelection = PR_FALSE;
  mCurrentEventContent = nsnull;
  mCurrentEventFrame = nsnull;
  EnableScrolling();
#ifdef NS_DEBUG
  mCurrentTargetView = nsnull;
#endif
  mPendingReflowEvent = PR_FALSE;    
  mBatchReflows = PR_FALSE;
  mDocumentLoading = PR_FALSE;
  mSubShellMap = nsnull;
  mRCCreatedDuringLoad = 0;
  mDummyLayoutRequest = nsnull;
  mPrefStyleSheet = nsnull;
  mEnablePrefStyleSheet = PR_TRUE;

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
                nsIStyleSet* aStyleSet)
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

  //doesn't add a ref since we own it... MMP
  mViewManager->SetViewObserver((nsIViewObserver *)this);

  // Bind the context to the presentation shell.
  mPresContext = dont_QueryInterface(aPresContext);
  aPresContext->SetShell(this);

  mStyleSet = aStyleSet;

  mHistoryState = nsnull;

  nsresult result = nsComponentManager::CreateInstance(kFrameSelectionCID, nsnull,
                                                 NS_GET_IID(nsIFrameSelection),
                                                 getter_AddRefs(mSelection));
  if (!NS_SUCCEEDED(result))
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
  if (!NS_SUCCEEDED(result))
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
  
  // Cache the event queue of the current UI thread
  nsCOMPtr<nsIEventQueueService> eventService = 
           do_GetService(kEventQueueServiceCID, &result);
  if (NS_SUCCEEDED(result))                    // XXX this implies that the UI is the current thread.
    result = eventService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(mEventQueue));
  

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
  // setup the preference style rules up (no forced reflow)
  SetPreferenceStyleRules(PR_FALSE);

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

  // kill subshell map, if any.  It holds only weak references
  if (mSubShellMap)
  {
    delete mSubShellMap;
    mSubShellMap = nsnull;
  }

  // release current event content and any content on event stack
  NS_IF_RELEASE(mCurrentEventContent);

  PRInt32 i, count = mCurrentEventContentStack.Count();
  nsIContent* currentEventContent;
  for (i = 0; i < count; i++) {
    currentEventContent = (nsIContent*)mCurrentEventContentStack.ElementAt(i);
    NS_IF_RELEASE(currentEventContent);
  }

  if (mViewManager) {
    // Disable paints during tear down of the frame tree
    mViewManager->DisableRefresh();
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
  if (mPendingReflowEvent) {
    mPendingReflowEvent = PR_FALSE;
    mEventQueue->RevokeEvents(this);
  }

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
  if (!mStackArena) {
    mStackArena = new StackArena();
    if (!mStackArena)
      return NS_ERROR_OUT_OF_MEMORY;
  }

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
    ReconstructFrames();
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
      // XXX - when there is both an override and backstop pref stylesheet this won't matter,
      //       as the color rules will be overrides and the links rules will be backstop
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
      ReconstructFrames();
    }

    return result;

  } else {
    return NS_ERROR_NULL_POINTER;
  }
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
 #ifdef PREFS_USE_OVERRIDE
      PRInt32 numBefore = mStyleSet->GetNumberOfOverrideStyleSheets();
 #else
      PRInt32 numBefore = mStyleSet->GetNumberOfBackstopStyleSheets();
 #endif
      NS_ASSERTION(numBefore > 0, "no override stylesheets in styleset, but we have one!");
#endif

 #ifdef PREFS_USE_OVERRIDE
      mStyleSet->RemoveOverrideStyleSheet(mPrefStyleSheet);
 #else 
      mStyleSet->RemoveBackstopStyleSheet(mPrefStyleSheet);
 #endif

#ifdef DEBUG_attinasi
 #ifdef PREFS_USE_OVERRIDE
      NS_ASSERTION((numBefore - 1) == mStyleSet->GetNumberOfOverrideStyleSheets(),
                   "Pref stylesheet was not removed");
 #else
      NS_ASSERTION((numBefore - 1) == mStyleSet->GetNumberOfBackstopStyleSheets(),
                   "Pref stylesheet was not removed");
 #endif
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
        mPrefStyleSheet->SetDefaultNameSpaceID(kNameSpaceID_HTML);
 #ifdef PREFS_USE_OVERRIDE
        mStyleSet->AppendOverrideStyleSheet(mPrefStyleSheet);
 #else
        mStyleSet->AppendBackstopStyleSheet(mPrefStyleSheet);
 #endif
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
              //   important rules (when put into a backstop stylesheet) and 
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
        //   which they are if we put them in the backstop stylesheet,
        //   though if using an override sheet this will cause authors grief still
        //   In the backstop stylesheet, they are !important when we are ignoring document colors
        //
        // XXX: do active links and visited links get another color?
        //      They were red in the html.css rules
        
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
          // - links: '*:link, *:link:active {color: #RRGGBB [!important];}'
          ColorToString(linkColor,strColor);
          NS_NAMED_LITERAL_STRING(notImportantStr, ";} ");
          NS_NAMED_LITERAL_STRING(importantStr, " !important;} ");
          nsAString& ruleClose = useDocColors ? notImportantStr : importantStr;
          result = sheet->InsertRule(NS_LITERAL_STRING("*:link, *:link:active {color:") +
                                     strColor +
                                     ruleClose,
                                     0,&index);
          NS_ENSURE_SUCCESS(result, result);
            
          ///////////////////////////////////////////////////////////////
          // - visited links '*:visited, *:visited:active {color: #RRGGBB [!important];}'
          ColorToString(visitedColor,strColor);
          // insert the rule
          result = sheet->InsertRule(NS_LITERAL_STRING("*:visited, *:visited:active {color:") +
                                     strColor +
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
              // NOTE: these must go in the backstop stylesheet or they cannot be
              //       overridden by authors
  #ifdef DEBUG_attinasi
              printf (" - Creating rules for enabling link underlines\n");
  #endif
              // make a rule to make text-decoration: underline happen for links
              strRule.Append(NS_LITERAL_STRING(":link, :visited {text-decoration:underline;}"));
            } else {
  #ifdef DEBUG_attinasi
              printf (" - Creating rules for disabling link underlines\n");
  #endif
              // make a rule to make text-decoration: none happen for links
              strRule.Append(NS_LITERAL_STRING(":link, :visited {text-decoration:none;}"));
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
PresShell::ScrollSelectionIntoView(SelectionType aType, SelectionRegion aRegion)
{
  if (!mSelection)
    return NS_ERROR_NULL_POINTER;

  return mSelection->ScrollSelectionIntoView(aType, aRegion);
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
        char* url = nsnull;
        uri->GetSpec(&url);
        printf("*** PresShell::InitialReflow (this=%p, url='%s')\n", (void*)this, url);
        Recycle(url);
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
    MOZ_TIMER_DEBUGLOG(("Reset and start: Frame Creation: PresShell::InitialReflow(), this=%p\n", this));
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
    MOZ_TIMER_DEBUGLOG(("Stop: Frame Creation: PresShell::InitialReflow(), this=%p\n", this));
    MOZ_TIMER_STOP(mFrameCreationWatch);
    CtlStyleWatch(kStyleWatchDisable,mStyleSet);
  }

  if (rootFrame) {
    nsCOMPtr<nsILayoutHistoryState> historyState = do_QueryReferent(mHistoryState);
    if (historyState) {
      mFrameManager->RestoreFrameState(mPresContext, rootFrame, historyState);
    }

    MOZ_TIMER_DEBUGLOG(("Reset and start: Reflow: PresShell::InitialReflow(), this=%p\n", this));
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
    rootFrame->DidReflow(mPresContext, NS_FRAME_REFLOW_FINISHED);
      
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
    MOZ_TIMER_DEBUGLOG(("Stop: Reflow: PresShell::InitialReflow(), this=%p\n", this));
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
      mPaintSuppressionTimer->Init(sPaintSuppressionCallback, this, delay, NS_PRIORITY_HIGHEST);
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
    rootFrame->DidReflow(mPresContext, NS_FRAME_REFLOW_FINISHED);
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
    mResizeEventTimer->Init(sResizeEventCallback, this, RESIZE_EVENT_DELAY, NS_PRIORITY_HIGH);  
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
PresShell::ScrollFrameIntoView(nsIFrame *aFrame){
  if (!aFrame)
    return NS_ERROR_NULL_POINTER;
  
  // Before we scroll the frame into view, ask the command dispatcher
  // if we're resetting focus because a window just got an activate
  // event. If we are, we do not want to scroll the frame into view.
  // Example: The user clicks on an anchor, and then deactivates the 
  // window. When they reactivate the window, the expected behavior
  // is not for the anchor link to scroll back into view. That is what
  // this check is preventing.
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
    
  if (IsScrollingEnabled())
    return ScrollFrameIntoView(aFrame, NS_PRESSHELL_SCROLL_ANYWHERE,
                               NS_PRESSHELL_SCROLL_ANYWHERE);
  return NS_OK;
}

NS_IMETHODIMP
PresShell::NotifyDestroyingFrame(nsIFrame* aFrame)
{
  // Cancel any pending reflow commands targeted at this frame
  CancelReflowCommandInternal(aFrame, nsnull, mReflowCommands);
  CancelReflowCommandInternal(aFrame, nsnull, mTimeoutReflowCommands);


  // Notify the frame manager
  if (mFrameManager) {
    mFrameManager->NotifyDestroyingFrame(aFrame);
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

NS_IMETHODIMP PresShell::SetDisplayNonTextSelection(PRBool aInEnable)
{
  mDisplayNonTextSelection = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP PresShell::GetDisplayNonTextSelection(PRBool *aOutEnable)
{
  if (!aOutEnable)
    return NS_ERROR_INVALID_ARG;
  *aOutEnable = mDisplayNonTextSelection;
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
#if 1
  return ScrollPage(aForward);
#else

  nsCOMPtr<nsIViewManager> viewManager;
  nsresult result = GetViewManager(getter_AddRefs(viewManager));
  if (NS_SUCCEEDED(result) && viewManager)
  {
    nsIScrollableView *scrollView;
    result = viewManager->GetRootScrollableView(&scrollView);
    if (NS_SUCCEEDED(result) && scrollView)
    {
      
    }
  }
  return result;
#endif //0
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
      scrollView->ScrollByLines(0, aForward ? 1 : -1);
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
  nsCOMPtr<nsIDocument> document;
  if (NS_FAILED(GetDocument(getter_AddRefs(document))) || !document)
    return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIDOMNodeList>nodeList; 
  NS_NAMED_LITERAL_STRING(bodyTag, "body");

  nsCOMPtr<nsIDOMDocument> doc = do_QueryInterface(document);
  if (!doc) 
    return NS_ERROR_FAILURE;
  nsresult result = doc->GetElementsByTagName(bodyTag, getter_AddRefs(nodeList));
  if (NS_FAILED(result) ||!nodeList)
    return result?result:NS_ERROR_FAILURE;


  PRUint32 count; 
  nodeList->GetLength(&count);

  if (count < 1)
    return NS_ERROR_FAILURE;

  // Use the first body node in the list:
  nsCOMPtr<nsIDOMNode> node;
  result = nodeList->Item(0, getter_AddRefs(node)); 
  if (NS_SUCCEEDED(result) && node)
  {
    //return node->QueryInterface(NS_GET_IID(nsIDOMElement), (void **)aBodyElement);
    // Is above equivalent to this:
    nsCOMPtr<nsIDOMElement> bodyElement = do_QueryInterface(node);
    if (bodyElement)
    {
      nsCOMPtr<nsIContent> bodyContent = do_QueryInterface(bodyElement);
      if (bodyContent)
      {
        nsIFrame *frame = nsnull;
        result = GetPrimaryFrameFor(bodyContent, &frame);
        if (frame)
        {
          PRInt32 offset;
          PRInt32 offsetend;
          PRBool  beginFrameContent;
          PRInt8  outsideLimit = -1;//search from beginning
          nsPeekOffsetStruct pos;
          pos.mAmount = eSelectLine;
          pos.mTracker = this;
          pos.mContentOffset = 0;
          pos.mContentOffsetEnd = 0;
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
          
          result = mSelection->HandleClick(pos.mResultContent ,pos.mContentOffset ,pos.mContentOffsetEnd ,aExtend, PR_FALSE, pos.mPreferLeft);
        }
        // if we got this far, attempt to scroll no matter what the above result is
        CompleteScroll(aForward);
      }
    }
  }
  return result;
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
    rootFrame->DidReflow(mPresContext, NS_FRAME_REFLOW_FINISHED);
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
  MOZ_TIMER_DEBUGLOG(("Reset: Style Resolution: PresShell::BeginLoad(), this=%p\n", this));
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
PresShell::AlreadyInQueue(nsIReflowCommand* aReflowCommand,
                          nsVoidArray&      aQueue)
{
  PRInt32 i, n = aQueue.Count();
  nsIFrame* targetFrame;
  PRBool inQueue = PR_FALSE;    

  if (NS_SUCCEEDED(aReflowCommand->GetTarget(targetFrame))) {
    // Iterate over the reflow commands and compare the targeted frames.
    for (i = 0; i < n; i++) {
      nsIReflowCommand* rc = (nsIReflowCommand*) aQueue.ElementAt(i);
      if (rc) {
        nsIFrame* targetOfQueuedRC;
        if (NS_SUCCEEDED(rc->GetTarget(targetOfQueuedRC))) {
          nsIReflowCommand::ReflowType RCType;
          nsIReflowCommand::ReflowType queuedRCType;
          aReflowCommand->GetType(RCType);
          rc->GetType(queuedRCType);
          if (targetFrame == targetOfQueuedRC &&
            RCType == queuedRCType) {            
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

  return inQueue;
}

void
NotifyAncestorFramesOfReflowCommand(nsIPresShell*     aPresShell,
                                    nsIReflowCommand* aRC,
                                    PRBool            aCommandAdded)
{
  if (aRC) {
    nsIFrame* target;
    aRC->GetTarget(target);
    if (target) {
      nsIFrame* ancestor;
      target->GetParent(&ancestor);
      while(ancestor) {
        ancestor->ReflowCommandNotify(aPresShell, aRC, aCommandAdded);
        ancestor->GetParent(&ancestor);
      }
    }
  }
}

NS_IMETHODIMP
PresShell::AppendReflowCommandInternal(nsIReflowCommand* aReflowCommand,
                                       nsVoidArray&      aQueue)
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
  // don't check for duplicates on the timeout queue - it is responsiblity of frames
  // who call SendInterruptNotificationTo to make sure there are no duplicates
  if ((&aQueue == &mTimeoutReflowCommands) ||
      ((&aQueue == &mReflowCommands) && !AlreadyInQueue(aReflowCommand, aQueue))) {
    NS_ADDREF(aReflowCommand);
    rv = (aQueue.AppendElement(aReflowCommand) ? NS_OK : NS_ERROR_OUT_OF_MEMORY);
    ReflowCommandAdded(aReflowCommand);
    NotifyAncestorFramesOfReflowCommand(this, aReflowCommand, PR_TRUE);
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

NS_IMETHODIMP
PresShell::AppendReflowCommand(nsIReflowCommand* aReflowCommand)
{
  return AppendReflowCommandInternal(aReflowCommand, mReflowCommands);
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
PresShell::CancelReflowCommandInternal(nsIFrame*                     aTargetFrame, 
                                       nsIReflowCommand::ReflowType* aCmdType,
                                       nsVoidArray&                  aQueue,
                                       PRBool                        aProcessDummyLayoutRequest)
{
  PRInt32 i, n = aQueue.Count();
  for (i = 0; i < n; i++) {
    nsIReflowCommand* rc = (nsIReflowCommand*) aQueue.ElementAt(i);
    if (rc) {
      nsIFrame* target;      
      if (NS_SUCCEEDED(rc->GetTarget(target))) {
        if (target == aTargetFrame) {
          if (aCmdType != NULL) {
            // If aCmdType is specified, only remove reflow commands
            // of that type
            nsIReflowCommand::ReflowType type;
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
          aQueue.RemoveElementAt(i);
          ReflowCommandRemoved(rc);
          NotifyAncestorFramesOfReflowCommand(this, rc, PR_FALSE);
          NS_RELEASE(rc);
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
PresShell::CancelReflowCommand(nsIFrame*                     aTargetFrame, 
                               nsIReflowCommand::ReflowType* aCmdType)
{
  return CancelReflowCommandInternal(aTargetFrame, aCmdType, mReflowCommands);
}


NS_IMETHODIMP
PresShell::CancelAllReflowCommands()
{
  PRInt32 n = mReflowCommands.Count();
  nsIReflowCommand* rc;
  PRInt32 i;
  for (i = 0; i < n; i++) {
    rc = NS_STATIC_CAST(nsIReflowCommand*, mReflowCommands.ElementAt(0));
    mReflowCommands.RemoveElementAt(0);
    ReflowCommandRemoved(rc);
    NS_RELEASE(rc);
  }

  n = mTimeoutReflowCommands.Count();
  for (i = 0; i < n; i++) {
    rc = NS_STATIC_CAST(nsIReflowCommand*, mTimeoutReflowCommands.ElementAt(0));
    mTimeoutReflowCommands.RemoveElementAt(0);
    ReflowCommandRemoved(rc);
    NS_RELEASE(rc);
  }

  DoneRemovingReflowCommands();

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
      rv = dx->CreateRenderingContext(widget, result);
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
PresShell::GoToAnchor(const nsString& aAnchorName)
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
          tagName.ToLowerCase();
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
          if (value.EqualsWithConversion(aAnchorName)) {
            content = do_QueryInterface(element);
            break;
          }
        }
      }
    }
  }


  if (content) {
    nsIFrame* frame;

    // Get the primary frame
    if (NS_SUCCEEDED(GetPrimaryFrameFor(content, &frame))) {
      rv = ScrollFrameIntoView(frame, NS_PRESSHELL_SCROLL_TOP,
                               NS_PRESSHELL_SCROLL_ANYWHERE);

      if (NS_SUCCEEDED(rv)) {
        // Should we select the target?
        // This action is controlled by a preference: the default is to not select.
        nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID,&rv));
        if (NS_SUCCEEDED(rv) && prefs) {
          PRBool selectAnchor;
          nsresult rvPref;
          rvPref = prefs->GetBoolPref("layout.selectanchor",&selectAnchor);
          if (NS_SUCCEEDED(rvPref) && selectAnchor) {
            rv = SelectContent(content);
          }
        }
      }
    }
  } else {
    rv = NS_ERROR_FAILURE; //changed to NS_OK in quirks mode if ScrollTo is called
    
    // Scroll to the top/left if the anchor can not be
    // found and it is labelled top (quirks mode only). @see bug 80784
    nsCompatibility compatMode;
    mPresContext->GetCompatibilityMode(&compatMode);
   
    if ((aAnchorName.EqualsIgnoreCase("top")) &&
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
PresShell::SelectContent(nsIContent *aContent)
{
  nsresult rv;
  nsCOMPtr<nsISelection> sel;
  rv = GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(sel));
  if (NS_SUCCEEDED(rv) && sel) {
    nsCOMPtr<nsIDOMRange> range(do_CreateInstance(kRangeCID,&rv));
    nsCOMPtr<nsIDOMNode> node(do_QueryInterface(aContent));
    if (NS_SUCCEEDED(rv) && node) {
      rv = range->SelectNode(node);
      if (NS_SUCCEEDED(rv)) {
        sel->RemoveAllRanges();
        rv = sel->AddRange(range);
      }
    }
  }

  return rv;
}

NS_IMETHODIMP
PresShell::ScrollFrameIntoView(nsIFrame *aFrame,
                               PRIntn   aVPercent, 
                               PRIntn   aHPercent) const
{
  nsresult rv = NS_OK;
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

  if (mViewManager) {
    // Get the viewport scroller
    nsIScrollableView* scrollingView;
    mViewManager->GetRootScrollableView(&scrollingView);

    if (scrollingView) {
      nsIView*  scrolledView;
      nsPoint   offset;
      nsIView*  closestView;
          
      // Determine the offset from aFrame to the scrolled view. We do that by
      // getting the offset from its closest view and then walking up
      scrollingView->GetScrolledView(scrolledView);
      aFrame->GetOffsetFromView(mPresContext, offset, &closestView);

      // If this is an inline frame, we need to change the top of the
      // offset to include the whole line.
      nsCOMPtr<nsIAtom> frameType;
      nsIFrame *prevFrame = aFrame;
      nsIFrame *frame = aFrame;
      while (frame && (frame->GetFrameType(getter_AddRefs(frameType)),
                       frameType.get() == nsLayoutAtoms::inlineFrame)) {
        prevFrame = frame;
        prevFrame->GetParent(&frame);
      }
      if (frame != aFrame &&
          frame &&
          frameType.get() == nsLayoutAtoms::blockFrame) {
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
                if (newoffset < offset.y) offset.y = newoffset;
              }
            }
          }
        }
      }

      // XXX Deal with the case where there is a scrolled element, e.g., a
      // DIV in the middle...
      while ((closestView != nsnull) && (closestView != scrolledView)) {
        nscoord x, y;

        // Update the offset
        closestView->GetPosition(&x, &y);
        offset.MoveBy(x, y);

        // Get its parent view
        closestView->GetParent(closestView);
      }

      // Determine the visible rect in the scrolled view's coordinate space.
      // The size of the visible area is the clip view size
      const nsIView*  clipView;
      nsRect          visibleRect;

      scrollingView->GetScrollPosition(visibleRect.x, visibleRect.y);
      scrollingView->GetClipView(&clipView);
      clipView->GetDimensions(&visibleRect.width, &visibleRect.height);

      // The actual scroll offsets
      nscoord scrollOffsetX = visibleRect.x;
      nscoord scrollOffsetY = visibleRect.y;

      // The frame's bounds in the coordinate space of the scrolled frame
      nsRect  frameBounds;
      aFrame->GetRect(frameBounds);
      frameBounds.x = offset.x;
      frameBounds.y = offset.y;

      // See how the frame should be positioned vertically
      if (NS_PRESSHELL_SCROLL_ANYWHERE == aVPercent) {
        // The caller doesn't care where the frame is positioned vertically,
        // so long as it's fully visible
        if (frameBounds.y < visibleRect.y) {
          // Scroll up so the frame's top edge is visible
          scrollOffsetY = frameBounds.y;
        } else if (frameBounds.YMost() > visibleRect.YMost()) {
          // Scroll down so the frame's bottom edge is visible. Make sure the
          // frame's top edge is still visible
          scrollOffsetY += frameBounds.YMost() - visibleRect.YMost();
          if (scrollOffsetY > frameBounds.y) {
            scrollOffsetY = frameBounds.y;
          }
        }
      } else if (NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE == aVPercent) {
        // Scroll only if no part of the frame is visible in this view
        if (frameBounds.YMost() < visibleRect.y) {
          // Scroll up so the frame's top edge is visible
          scrollOffsetY = frameBounds.y;
        }  else if (frameBounds.y > visibleRect.YMost()) {
          // Scroll down so the frame's bottom edge is visible. Make sure the
          // frame's top edge is still visible
          scrollOffsetY += frameBounds.YMost() - visibleRect.YMost();
          if (scrollOffsetY > frameBounds.y) {
            scrollOffsetY = frameBounds.y;
          }
         }      
      } else {
        // Align the frame edge according to the specified percentage
        nscoord frameAlignY = frameBounds.y + (frameBounds.height * aVPercent) / 100;
        scrollOffsetY = frameAlignY - (visibleRect.height * aVPercent) / 100;
      }

      // See how the frame should be positioned horizontally
      if (NS_PRESSHELL_SCROLL_ANYWHERE == aHPercent) {
        // The caller doesn't care where the frame is positioned horizontally,
        // so long as it's fully visible
        if (frameBounds.x < visibleRect.x) {
          // Scroll left so the frame's left edge is visible
          scrollOffsetX = frameBounds.x;
        } else if (frameBounds.XMost() > visibleRect.XMost()) {
          // Scroll right so the frame's right edge is visible. Make sure the
          // frame's left edge is still visible
          scrollOffsetX += frameBounds.XMost() - visibleRect.XMost();
          if (scrollOffsetX > frameBounds.x) {
            scrollOffsetX = frameBounds.x;
          }
        }
      } else if (NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE == aHPercent) {
        // Scroll only if no part of the frame is visible in this view
        if (frameBounds.XMost() < visibleRect.x) {
          // Scroll left so the frame's left edge is visible
          scrollOffsetX = frameBounds.x;
        }  else if (frameBounds.x > visibleRect.XMost()) {
          // Scroll right so the frame's right edge is visible. Make sure the
          // frame's left edge is still visible
          scrollOffsetX += frameBounds.XMost() - visibleRect.XMost();
          if (scrollOffsetX > frameBounds.x) {
            scrollOffsetX = frameBounds.x;
          }
        }           
      } else {
        // Align the frame edge according to the specified percentage
        nscoord frameAlignX = frameBounds.x + (frameBounds.width * aHPercent) / 100;
        scrollOffsetX = frameAlignX - (visibleRect.width * aHPercent) / 100;
      }
      scrollingView->ScrollTo(scrollOffsetX, scrollOffsetY, NS_VMREFRESH_IMMEDIATE);
    }
  }
  return rv;
}

// DoCopyLinkLocation: copy link location to clipboard
NS_IMETHODIMP PresShell::DoCopyLinkLocation(nsIDOMNode* aNode)
{
#ifdef DEBUG_dr
  printf("dr :: PresShell::DoCopyLinkLocation\n");
#endif

  NS_ENSURE_ARG_POINTER(aNode);
  nsresult rv;
  nsAutoString anchorText;

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
              rv = ios->NewURI(NS_ConvertUCS2toUTF8(base).get(),nsnull,getter_AddRefs(baseURI));
              NS_ENSURE_SUCCESS(rv, rv);
      
              nsXPIDLCString spec;
              rv = baseURI->Resolve(NS_ConvertUCS2toUTF8(anchorText).get(),getter_Copies(spec));
              NS_ENSURE_SUCCESS(rv, rv);

              anchorText.AssignWithConversion(spec.get());
            }
          }
        }
      }
    }
  }

  if (anchor || area || link || xlinkType.Equals(NS_LITERAL_STRING("simple"))) {
    // get the clipboard helper
    nsCOMPtr<nsIClipboardHelper>
      clipboard(do_GetService("@mozilla.org/widget/clipboardhelper;1", &rv));
    NS_ENSURE_SUCCESS(rv, rv);

    // copy the href onto the clipboard
    return clipboard->CopyString(anchorText);
  }

  // if no link, fail.
  return NS_ERROR_FAILURE;
}

// DoCopyImageLocation: copy image location to clipboard
NS_IMETHODIMP PresShell::DoCopyImageLocation(nsIDOMNode* aNode)
{
#ifdef DEBUG_dr
  printf("dr :: PresShell::DoCopyImageLocation\n");
#endif

  NS_ENSURE_ARG_POINTER(aNode);
  nsresult rv;

  // are we an image?
  nsCOMPtr<nsIDOMHTMLImageElement> img(do_QueryInterface(aNode, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  if (img) {
    // if so, get the src
    nsAutoString srcText;
    rv = img->GetSrc(srcText);
    NS_ENSURE_SUCCESS(rv, rv);

    // get the clipboard helper
    nsCOMPtr<nsIClipboardHelper>
      clipboard(do_GetService("@mozilla.org/widget/clipboardhelper;1", &rv));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(clipboard, NS_ERROR_FAILURE);

    // copy the src onto the clipboard
    return clipboard->CopyString(srcText);
  }

  // if no image, fail.
  return NS_ERROR_FAILURE;
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
    domWindow->UpdateCommands(NS_ConvertASCIItoUCS2("clipboard"));
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
  while (aFrame)  {
    lastInFlow = aFrame;
    lastInFlow->GetNextInFlow(&aFrame);
  }

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
      mPresContext->ProbePseudoStyleContextFor(aContent, nsCSSAtoms::afterPseudo,
                                               styleContext, PR_FALSE,
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

  mPaintingSuppressed = PR_FALSE;
  nsIFrame* rootFrame;
  mFrameManager->GetRootFrame(&rootFrame);
  if (rootFrame) {
    nsRect rect;
    rootFrame->GetRect(rect);
    ((nsFrame*)rootFrame)->Invalidate(mPresContext, rect, PR_FALSE);
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
                                nsIContent* aContent2)
{
  WillCauseReflow();
  nsresult rv = mStyleSet->ContentStatesChanged(mPresContext, aContent1, aContent2);
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
                            PRInt32      aHint)
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

  nsCOMPtr<nsILayoutHistoryState> historyState = do_QueryReferent(mHistoryState);
  if (NS_SUCCEEDED(rv) && historyState) {
    // If history state has been set by session history, ask the frame manager 
    // to restore frame state for the frame hierarchy created for the chunk of
    // content that just came in.
    // That is the frames with numbers after aNewIndexInContainer.
    PRInt32 count = 0;
    aContainer->ChildCount(count);

    PRInt32 i;
    nsCOMPtr<nsIContent> newChild;
    for (i = aNewIndexInContainer; i < count; ++i) {
      aContainer->ChildAt(i, *getter_AddRefs(newChild));
      if (!newChild) {
        // We should never get here.
        NS_ERROR("Got a null child when restoring state!");
        continue;
      }
      nsIFrame* frame;
      rv = GetPrimaryFrameFor(newChild, &frame);
      if (NS_SUCCEEDED(rv) && nsnull != frame)
        mFrameManager->RestoreFrameState(mPresContext, frame, historyState);
    }
  }

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
  WillCauseReflow();
  nsresult  rv = mStyleSet->ContentRemoved(mPresContext, aContainer,
                                           aChild, aIndexInContainer);
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
  nsresult rv = NS_OK;

  // first notify the style set that a sheet's state has changed
  if (mStyleSet) {
    rv = mStyleSet->NotifyStyleSheetStateChanged(aDisabled ? PR_FALSE : PR_TRUE);
  }
  if (NS_FAILED(rv)) return rv;

  if (aDisabled) {
    // If the stylesheet is disabled, remove existing BodyFixupRule for
    // bug 88681
    rv = mStyleSet->RemoveBodyFixupRule(aDocument);
    if (NS_FAILED(rv)) return rv;
  }
  // rebuild the frame-world
  return ReconstructFrames();
}

NS_IMETHODIMP
PresShell::StyleRuleChanged(nsIDocument *aDocument,
                            nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule,
                            PRInt32 aHint) 
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
  // XXX For now reconstruct everything
  return ReconstructFrames();
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
PresShell::GetSubShellFor(nsIContent*   aContent,
                          nsISupports** aResult) const
{
  NS_ENSURE_ARG_POINTER(aContent);
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nsnull;

  if (mSubShellMap) {
    mSubShellMap->Search(aContent, 0, (void**)aResult);
    if (*aResult) {
      NS_ADDREF(*aResult);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
PresShell::SetSubShellFor(nsIContent*  aContent,
                          nsISupports* aSubShell)
{
  NS_ENSURE_ARG_POINTER(aContent);

  // If aSubShell is NULL, then remove the mapping
  if (!aSubShell) {
    if (mSubShellMap) {
      mSubShellMap->Remove(aContent);
    }
  } else {
    // Create a new DST if necessary
    if (!mSubShellMap) {
      if (!mDSTNodeArena) {
        mDSTNodeArena = nsDST::NewMemoryArena();
        if (!mDSTNodeArena) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
      }
      mSubShellMap = new nsDST(mDSTNodeArena);
      if (!mSubShellMap) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }

    // Add a mapping to the hash table
    mSubShellMap->Insert(aContent, (void*)aSubShell, nsnull);
  }
  return NS_OK;
}
  
NS_IMETHODIMP
PresShell::FindContentForShell(nsISupports* aSubShell,
                               nsIContent** aContent) const
{
  NS_ENSURE_ARG_POINTER(aSubShell);
  NS_ENSURE_ARG_POINTER(aContent);
  *aContent = nsnull;
  
  if (!mSubShellMap)
    return NS_OK;
  
  nsSearchEnumerator enumerator(NS_STATIC_CAST(void*, aSubShell));
  mSubShellMap->Enumerate(enumerator);
  *aContent = NS_STATIC_CAST(nsIContent*, enumerator.GetResult());
  NS_IF_ADDREF(*aContent);

  return NS_OK;
}

NS_IMETHODIMP
PresShell::GetPlaceholderFrameFor(nsIFrame*  aFrame,
                                  nsIFrame** aResult) const
{
  nsresult  rv;

  if (mFrameManager) {
    rv = mFrameManager->GetPlaceholderFrameFor(aFrame, aResult);

  } else {
    *aResult = nsnull;
    rv = NS_OK;
  }

  return rv;
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

// If the element is absolutely positioned and has a specified clip rect
// then it pushes the current rendering context and sets the clip rect.
// Returns PR_TRUE if the clip rect is set and PR_FALSE otherwise
static PRBool
SetClipRect(nsIRenderingContext& aRenderingContext, nsIFrame* aFrame)
{
  PRBool clipState;
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

    // Set updated clip-rect into the rendering context
    aRenderingContext.PushState();
    aRenderingContext.SetClipRect(clipRect, nsClipCombine_kIntersect, clipState);
    return PR_TRUE;
  }

  return PR_FALSE;
}

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


  aView->GetClientData(clientData);
  frame = (nsIFrame *)clientData;

/*  if (mSelection && aEvent->eventStructType == NS_KEY_EVENT)
  {//KEY HANDLERS WILL GET RID OF THIS 
    if (mDisplayNonTextSelection && NS_SUCCEEDED(mSelection->HandleKeyEvent(mPresContext, aEvent)))
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
      else {
        // This is because we want to give the point in the same
        // coordinates as the frame's own Rect, so mRect.Contains(aPoint)
        // works.  However, this point is relative to the frame's rect, so
        // we need to add on the origin of the rect.
        nsPoint eventPoint;
        frame->GetOrigin(eventPoint);
        eventPoint += aEvent->point;
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
      ps->SetReflowEventStatus(PR_FALSE);
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
  if (!mPendingReflowEvent && !mIsReflowing && mReflowCommands.Count() > 0) {
    ReflowEvent* ev = new ReflowEvent(NS_STATIC_CAST(nsIPresShell*, this));
    mEventQueue->PostEvent(ev);
    mPendingReflowEvent = PR_TRUE;
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

  if (!gAsyncReflowDuringDocLoad && mDocumentLoading) {
    FlushPendingNotifications(PR_FALSE);
  }

  return NS_OK;
}

void
PresShell::ProcessReflowCommand(nsVoidArray&         aQueue,
                                PRBool               aAccumulateTime,
                                nsHTMLReflowMetrics& aDesiredSize,
                                nsSize&              aMaxSize,
                                nsIRenderingContext& aRenderingContext)
{
  // Use RemoveElementAt in case the reflowcommand dispatches a
  // new one during its execution.
  nsIReflowCommand* rc = (nsIReflowCommand*)aQueue.ElementAt(0);
  aQueue.RemoveElementAt(0);

  // Dispatch the reflow command
  PRTime beforeReflow, afterReflow;
  if (aAccumulateTime) 
    beforeReflow = PR_Now();
  rc->Dispatch(mPresContext, aDesiredSize, aMaxSize, aRenderingContext); // dispatch the reflow command
  if (aAccumulateTime) 
    afterReflow = PR_Now();

  ReflowCommandRemoved(rc);
  NS_RELEASE(rc);
  VERIFY_STYLE_TREE;

  if (aAccumulateTime) {
    PRInt64 totalTime, diff;
    LL_SUB(diff, afterReflow, beforeReflow);

    LL_I2L(totalTime, mAccumulatedReflowTime);
    LL_ADD(totalTime, totalTime, diff);
    LL_L2I(mAccumulatedReflowTime, totalTime);
  }
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
        nsIReflowCommand* rc = (nsIReflowCommand*)
          mReflowCommands.ElementAt(i);
        rc->List(stdout);
      }
    }
#endif
    
    mIsReflowing = PR_TRUE;
    PRInt64 maxTime;
    while (0 != mReflowCommands.Count()) {
      ProcessReflowCommand(mReflowCommands, aInterruptible, desiredSize, maxSize, *rcx);
      if (aInterruptible) {
        LL_I2L(maxTime, gMaxRCProcessingTime);
        PRInt64 temp;
        LL_I2L(temp, mAccumulatedReflowTime);
        if (LL_CMP(temp, >, maxTime))
          break;          
      }
    }
    mIsReflowing = PR_FALSE;

    if (aInterruptible) {
      // process the timeout reflow commands completely
      // printf("timeout reflows=%d \n", mTimeoutReflowCommands.Count());
      while (0 != mTimeoutReflowCommands.Count()) {
        ProcessReflowCommand(mTimeoutReflowCommands, PR_TRUE, desiredSize, maxSize, *rcx);
      }
      if (mReflowCommands.Count() > 0) { // Reflow Commands are still queued up.
        // Schedule a reflow event to handle the remaining reflow commands asynchronously.
        PostReflowEvent();
      }
#ifdef DEBUG
      if (VERIFY_REFLOW_DUMP_COMMANDS & gVerifyReflowFlags) {        
        printf("Time spent in PresShell::ProcessReflowCommands(), this=%p, time=%d micro seconds\n", 
               (void*)this, mAccumulatedReflowTime);
      }
#endif
      mAccumulatedReflowTime = 0;
    }
    NS_IF_RELEASE(rcx);
    
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
PresShell::SendInterruptNotificationTo(nsIFrame*                   aFrame,
                                       nsIPresShell::InterruptType aInterruptType)
{
  // create a reflow command targeted at aFrame
  nsIReflowCommand* reflowCmd;
  nsresult          rv;

  // Target the reflow comamnd at aFrame
  rv = NS_NewHTMLReflowCommand(&reflowCmd, aFrame,
                               nsIReflowCommand::Timeout);
  if (NS_SUCCEEDED(rv)) {
    // Add the reflow command
    AppendReflowCommandInternal(reflowCmd, mTimeoutReflowCommands);
    NS_RELEASE(reflowCmd);
  }
  return rv;
}

nsresult
PresShell::CancelInterruptNotificationTo(nsIFrame*                   aFrame,
                                         nsIPresShell::InterruptType aInterruptType)

{
  // cancel the reflow command without affecting the dummy layout request
  return CancelReflowCommandInternal(aFrame, nsnull, mTimeoutReflowCommands, PR_FALSE);
}

nsresult
PresShell::GetReflowEventStatus(PRBool* aPending)
{
  if (aPending)
    *aPending = mPendingReflowEvent;
  return NS_OK;
}

nsresult
PresShell::SetReflowEventStatus(PRBool aPending)
{
  mPendingReflowEvent = aPending;
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

  n = aSet->GetNumberOfBackstopStyleSheets();
  for (i = 0; i < n; i++) {
    nsIStyleSheet* ss;
    ss = aSet->GetBackstopStyleSheetAt(i);
    if (nsnull != ss) {
      clone->AppendBackstopStyleSheet(ss);
      NS_RELEASE(ss);
    }
  }
  *aResult = clone.get();
  NS_ADDREF(*aResult);
  return NS_OK;
}


nsresult
PresShell::ReflowCommandAdded(nsIReflowCommand* aRC)
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
PresShell::ReflowCommandRemoved(nsIReflowCommand* aRC)
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
    rv = DummyLayoutRequest::Create(getter_AddRefs(mDummyLayoutRequest), this);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsILoadGroup> loadGroup;
    if (mDocument) {
      rv = mDocument->GetDocumentLoadGroup(getter_AddRefs(loadGroup));
      if (NS_FAILED(rv)) return rv;
    }

    if (loadGroup) {
      nsCOMPtr<nsIChannel> channel = do_QueryInterface(mDummyLayoutRequest);
      rv = channel->SetLoadGroup(loadGroup);
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
    nsIWidget* w1, *w2;
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

          v1->GetWidget(w1);
          v2->GetWidget(w2);
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
        nsISpaceManager *sm1; // note, no ref counting here
        aFirstPresContext->GetShell(getter_AddRefs(ps1));
        NS_ASSERTION(ps1, "no pres shell for primary tree!");
        ps1->GetFrameManager(getter_AddRefs(fm1));
        NS_ASSERTION(fm1, "no frame manager for primary tree!");
        fm1->GetFrameProperty((nsIFrame*)k1, nsLayoutAtoms::spaceManagerProperty,
                                     0, (void **)&sm1);
        // look at the test frame
        nsCOMPtr<nsIFrameManager>fm2;
        nsCOMPtr<nsIPresShell> ps2;
        nsISpaceManager *sm2; // note, no ref counting here
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
    rv = NS_NewPrintPreviewContext(&cx);
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
  nsIWidget* rootWidget;
  rootView->GetWidget(rootWidget);
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
    char * uriStr = nsnull;
    if (mDocument) {
      nsCOMPtr<nsIURI> uri;
      mDocument->GetDocumentURL(getter_AddRefs(uri));
      if (uri) {
        uri->GetPath(&uriStr);
      }
    }
    mReflowCountMgr->DisplayTotals(uriStr);
    mReflowCountMgr->DisplayHTMLTotals(uriStr);
    mReflowCountMgr->DisplayDiffsInTotals("Differences");
    if (uriStr) nsCRT::free(uriStr);
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
    sprintf(key, "%p", aFrame);
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
    sprintf(key, "%p", aFrame);
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
      nscoord width, height;
      aRenderingContext->GetWidth((char*)buf, width);
      fm->GetHeight(height);

      nsRect r;
      aFrame->GetRect(r);

      nscoord x = 0;
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
      aRenderingContext->DrawString(buf, strlen(buf), x+15,15);
      aRenderingContext->SetColor(color);
      aRenderingContext->DrawString(buf, strlen(buf), x,0);

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
  sprintf(key, "%p", aParentFrame);
  IndiReflowCounter * counter = (IndiReflowCounter *)PL_HashTableLookup(aHT, key);
  if (counter) {
    counter->mHasBeenOutput = PR_TRUE;
    char * name = ToNewCString(counter->mName);
    for (PRInt32 i=0;i<aLevel;i++) printf(" ");
    printf("%s - %p   [%d][", name, aParentFrame, counter->mCount);
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
  char *str = (char *)he->key;
  IndiReflowCounter * counter = (IndiReflowCounter *)he->value;
  if (counter && !counter->mHasBeenOutput) {
    char * name = ToNewCString(counter->mName);
    printf("%s - %p   [%d][", name, counter->mFrame, counter->mCount);
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



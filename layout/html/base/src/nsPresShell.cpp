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
 * Steve Clark (buster@netscape.com)
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
#include "nsIStyleContext.h"
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
#include "nsDOMEvent.h"
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
#include "nsWeakReference.h"
#include "nsIPageSequenceFrame.h"
#include "nsICaret.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIXMLDocument.h"
#include "nsIScrollableView.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsHTMLContentSinkStream.h"
#include "nsIFrameSelection.h"
#include "nsIDOMNSHTMLInputElement.h" //optimization for ::DoXXX commands
#include "nsIDOMNSHTMLTextAreaElement.h"
#include "nsViewsCID.h"
#include "nsIFrameManager.h"
#include "nsISupportsPrimitives.h"
#include "nsILayoutHistoryState.h"
#include "nsIScrollPositionListener.h"
#include "nsICompositeListener.h"
#include "nsTimer.h"
#include "nsWeakPtr.h"
#include "plarena.h"
#include "nsCSSAtoms.h"
#include "nsIObserverService.h" // for reflow observation
#include "nsIDocShell.h"        // for reflow observation
#ifdef MOZ_PERF_METRICS
#include "nsITimeRecorder.h"
#endif
#ifdef NS_DEBUG
#include "nsIFrameDebug.h"
#endif

#include "nsIReflowCallback.h"

#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindowInternal.h"
#include "nsPIDOMWindow.h"
#ifdef INCLUDE_XUL
#include "nsIDOMXULDocument.h"
#include "nsIDOMXULCommandDispatcher.h"
#endif // INCLUDE_XUL

// Drag & Drop, Clipboard
#include "nsWidgetsCID.h"
#include "nsIClipboard.h"
#include "nsITransferable.h"
#include "nsIFormatConverter.h"
#include "nsIDocShellTreeItem.h"
#include "nsIURI.h"
#include "nsIEventQueue.h"
#include "nsIEventQueueService.h"
#include "nsIScrollableFrame.h"
#include "prtime.h"
#include "prlong.h"
#include "nsIDocumentEncoder.h"
#include "nsIDragService.h"

// Dummy layout request
#include "nsIChannel.h"
#include "nsILoadGroup.h"
#include "nsNetUtil.h"

// SubShell map
#include "nsDST.h"

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

// private clipboard data flavors for html copy, used by editor when pasting
#define kHTMLContext   "text/_moz_htmlcontext"
#define kHTMLInfo      "text/_moz_htmlinfo"

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
#ifdef MOZ_REFLOW_PERF
class ReflowCountMgr;

static const char * kGrandTotalsStr = "Grand Totals";
#define NUM_REFLOW_TYPES 5

// Counting Class
class ReflowCounter {
public:
  ReflowCounter() { mMgr = nsnull; }
  ReflowCounter(ReflowCountMgr * aMgr);
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

protected:
  void DisplayTotals(PRUint32 * aArray, const char * aTitle);
  void DisplayHTMLTotals(PRUint32 * aArray, const char * aTitle);

  PRUint32 mTotals[NUM_REFLOW_TYPES];
  PRUint32 mCacheTotals[NUM_REFLOW_TYPES];

  ReflowCountMgr * mMgr;
};

// Manager Class
class ReflowCountMgr {
public:
  ReflowCountMgr();
  //static ReflowCountMgr * GetInstance() { return &gReflowCountMgr; } 

  ~ReflowCountMgr();

  void ClearTotals();
  void ClearGrandTotals();
  void DisplayTotals(const char * aStr);
  void DisplayHTMLTotals(const char * aStr);
  void DisplayDiffsInTotals(const char * aStr);

  void Add(const char * aName, nsReflowReason aType);
  ReflowCounter * LookUp(const char * aName);

  FILE * GetOutFile() { return mFD; }

protected:
  void DisplayTotals(PRUint32 * aArray, PRUint32 * aDupArray, char * aTitle);
  void DisplayHTMLTotals(PRUint32 * aArray, PRUint32 * aDupArray, char * aTitle);

  static PRIntn RemoveItems(PLHashEntry *he, PRIntn i, void *arg);
  void CleanUp();

  // stdout Output Methods
  static PRIntn DoSingleTotal(PLHashEntry *he, PRIntn i, void *arg);
  void DoGrandTotals();

  // HTML Output Methods
  static PRIntn DoSingleHTMLTotal(PLHashEntry *he, PRIntn i, void *arg);
  void DoGrandHTMLTotals();

  // Zero Out the Totals
  static PRIntn DoClearTotals(PLHashEntry *he, PRIntn i, void *arg);

  // Displays the Diff Totals
  static PRIntn DoDisplayDiffTotals(PLHashEntry *he, PRIntn i, void *arg);

  PLHashTable * mCounts;
  FILE * mFD;

  PRBool mCycledOnce;

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
  
  // Round size to multiple of 4
  aSize = PR_ROUNDUP(aSize, 4);

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
  // Round size to multiple of 4
  aSize = PR_ROUNDUP(aSize, 4);

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
  Create(nsIChannel** aResult, nsIPresShell* aPresShell);

  NS_DECL_ISUPPORTS

	// nsIRequest
  NS_IMETHOD GetName(PRUnichar* *result) { 
    NS_NOTREACHED("DummyLayoutRequest::GetName");
    return NS_ERROR_NOT_IMPLEMENTED;
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
  NS_IMETHOD OpenInputStream(nsIInputStream **_retval) { *_retval = nsnull; return NS_OK; }
	NS_IMETHOD OpenOutputStream(nsIOutputStream **_retval) { *_retval = nsnull; return NS_OK; }
	NS_IMETHOD AsyncOpen(nsIStreamObserver *observer, nsISupports *ctxt) { return NS_OK; }
	NS_IMETHOD AsyncRead(nsIStreamListener *listener, nsISupports *ctxt) { return NS_OK; }
	NS_IMETHOD AsyncWrite(nsIInputStream *fromStream, nsIStreamObserver *observer, nsISupports *ctxt) { return NS_OK; }
	NS_IMETHOD GetLoadAttributes(nsLoadFlags *aLoadAttributes) { *aLoadAttributes = nsIChannel::LOAD_NORMAL; return NS_OK; }
  NS_IMETHOD SetLoadAttributes(nsLoadFlags aLoadAttributes) { return NS_OK; }
	NS_IMETHOD GetContentType(char * *aContentType) { *aContentType = nsnull; return NS_OK; }
  NS_IMETHOD SetContentType(const char *aContentType) { return NS_OK; }
	NS_IMETHOD GetContentLength(PRInt32 *aContentLength) { *aContentLength = 0; return NS_OK; }
  NS_IMETHOD SetContentLength(PRInt32 aContentLength) { NS_NOTREACHED("SetContentLength"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD GetTransferOffset(PRUint32 *aTransferOffset) { NS_NOTREACHED("GetTransferOffset"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD SetTransferOffset(PRUint32 aTransferOffset) { NS_NOTREACHED("SetTransferOffset"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD GetTransferCount(PRInt32 *aTransferCount) { NS_NOTREACHED("GetTransferCount"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD SetTransferCount(PRInt32 aTransferCount) { NS_NOTREACHED("SetTransferCount"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD GetBufferSegmentSize(PRUint32 *aBufferSegmentSize) { NS_NOTREACHED("GetBufferSegmentSize"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD SetBufferSegmentSize(PRUint32 aBufferSegmentSize) { NS_NOTREACHED("SetBufferSegmentSize"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD GetBufferMaxSize(PRUint32 *aBufferMaxSize) { NS_NOTREACHED("GetBufferMaxSize"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD SetBufferMaxSize(PRUint32 aBufferMaxSize) { NS_NOTREACHED("SetBufferMaxSize"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD GetLocalFile(nsIFile* *result) { NS_NOTREACHED("GetLocalFile"); return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD GetPipeliningAllowed(PRBool *aPipeliningAllowed) { *aPipeliningAllowed = PR_FALSE; return NS_OK; }
  NS_IMETHOD SetPipeliningAllowed(PRBool aPipeliningAllowed) { NS_NOTREACHED("SetPipeliningAllowed"); return NS_ERROR_NOT_IMPLEMENTED; }
	NS_IMETHOD GetOwner(nsISupports * *aOwner) { *aOwner = nsnull; return NS_OK; }
	NS_IMETHOD SetOwner(nsISupports * aOwner) { return NS_OK; }
	NS_IMETHOD GetLoadGroup(nsILoadGroup * *aLoadGroup) { *aLoadGroup = mLoadGroup; NS_IF_ADDREF(*aLoadGroup); return NS_OK; }
	NS_IMETHOD SetLoadGroup(nsILoadGroup * aLoadGroup) { mLoadGroup = aLoadGroup; return NS_OK; }
	NS_IMETHOD GetNotificationCallbacks(nsIInterfaceRequestor * *aNotificationCallbacks) { *aNotificationCallbacks = nsnull; return NS_OK; }
	NS_IMETHOD SetNotificationCallbacks(nsIInterfaceRequestor * aNotificationCallbacks) { return NS_OK; }
  NS_IMETHOD GetSecurityInfo(nsISupports **info) {*info = nsnull; return NS_OK;}
};

PRInt32 DummyLayoutRequest::gRefCnt;
nsIURI* DummyLayoutRequest::gURI;

NS_IMPL_ADDREF(DummyLayoutRequest);
NS_IMPL_RELEASE(DummyLayoutRequest);
NS_IMPL_QUERY_INTERFACE2(DummyLayoutRequest, nsIRequest, nsIChannel);

nsresult
DummyLayoutRequest::Create(nsIChannel** aResult, nsIPresShell* aPresShell)
{
  DummyLayoutRequest* request = new DummyLayoutRequest(aPresShell);
  if (!request)
      return NS_ERROR_OUT_OF_MEMORY;

  *aResult = request;
  NS_ADDREF(*aResult);
  return NS_OK;
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
  NS_IMETHOD SetPreferenceStyleRules(PRBool aForceRefrlow);
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
  NS_IMETHOD GetPlaceholderFrameFor(nsIFrame*  aFrame,
                                    nsIFrame** aPlaceholderFrame) const;
  NS_IMETHOD AppendReflowCommand(nsIReflowCommand* aReflowCommand);
  NS_IMETHOD CancelReflowCommand(nsIFrame* aTargetFrame, nsIReflowCommand::ReflowType* aCmdType);  
  NS_IMETHOD CancelAllReflowCommands();
  NS_IMETHOD FlushPendingNotifications();

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
  NS_IMETHOD GoToAnchor(const nsString& aAnchorName) const;

  NS_IMETHOD ScrollFrameIntoView(nsIFrame *aFrame,
                                 PRIntn   aVPercent, 
                                 PRIntn   aHPercent) const;

  NS_IMETHOD NotifyDestroyingFrame(nsIFrame* aFrame);
  
  NS_IMETHOD GetFrameManager(nsIFrameManager** aFrameManager) const;

  NS_IMETHOD DoCopy();

  NS_IMETHOD CaptureHistoryState(nsILayoutHistoryState** aLayoutHistoryState, PRBool aLeavingPage);
  NS_IMETHOD GetHistoryState(nsILayoutHistoryState** aLayoutHistoryState);
  NS_IMETHOD SetHistoryState(nsILayoutHistoryState* aLayoutHistoryState);

  NS_IMETHOD GetGeneratedContentIterator(nsIContent*          aContent,
                                         GeneratedContentType aType,
                                         nsIContentIterator** aIterator) const;
 
  NS_IMETHOD SetAnonymousContentFor(nsIContent* aContent, nsISupportsArray* aAnonymousElements);
  NS_IMETHOD GetAnonymousContentFor(nsIContent* aContent, nsISupportsArray** aAnonymousElements);
  NS_IMETHOD ReleaseAnonymousContent();

  NS_IMETHOD HandleEventWithTarget(nsEvent* aEvent, nsIFrame* aFrame, nsIContent* aContent, PRUint32 aFlags, nsEventStatus* aStatus);
  NS_IMETHOD GetEventTargetFrame(nsIFrame** aFrame);

  NS_IMETHOD IsReflowLocked(PRBool* aIsLocked);  

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

#ifdef MOZ_REFLOW_PERF
  NS_IMETHOD ClearTotals();
  NS_IMETHOD DumpReflows();
  NS_IMETHOD CountReflows(const char * aName, PRUint32 aType);
#endif

protected:
  virtual ~PresShell();

  void HandlePostedDOMEvents();
  void HandlePostedAttributeChanges();
  void HandlePostedReflowCallbacks();

  /** notify all external reflow observers that reflow of type "aData" is about
    * to begin.
    */
  nsresult NotifyReflowObservers(const char *aData);

  nsresult ReflowCommandAdded(nsIReflowCommand* aRC);
  nsresult ReflowCommandRemoved(nsIReflowCommand* aRC);

  nsresult AddDummyLayoutRequest(void);
  nsresult RemoveDummyLayoutRequest(void);

  nsresult ReconstructFrames(void);
  nsresult CloneStyleSet(nsIStyleSet* aSet, nsIStyleSet** aResult);
  nsresult WillCauseReflow();
  nsresult DidCauseReflow();
  nsresult ProcessReflowCommands(PRBool aInterruptible);
  nsresult GetReflowEventStatus(PRBool* aPending);
  nsresult SetReflowEventStatus(PRBool aPending);  
  void     PostReflowEvent();
  PRBool   AlreadyInQueue(nsIReflowCommand* aReflowCommand);
  friend struct ReflowEvent;

    // utility to determine if we're in the middle of a drag
  PRBool IsDragInProgress ( ) const ;

  PRBool	mCaretEnabled;
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
  nsILayoutHistoryState* mHistoryState; // [WEAK] session history owns this
  PRUint32 mUpdateCount;
  nsVoidArray mReflowCommands;
  PRPackedBool mDocumentLoading;
  PRPackedBool mIsReflowing;
  PRPackedBool mIsDestroying;
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
  nsCOMPtr<nsIChannel>          mDummyLayoutRequest;

  // used for list of posted events and attribute changes. To be done
  // after reflow.
  nsDOMEventRequest* mFirstDOMEventRequest;
  nsDOMEventRequest* mLastDOMEventRequest;
  nsAttributeChangeRequest* mFirstAttributeRequest;
  nsAttributeChangeRequest* mLastAttributeRequest;
  nsCallbackEventRequest* mFirstCallbackEventRequest;
  nsCallbackEventRequest* mLastCallbackEventRequest;

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
};

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
/**
 * Note: the log module is created during library initialization which
 * means that you cannot perform logging before then.
 */
static PRLogModuleInfo* gLogModule;
static PRUint32 gVerifyReflowFlags;
#endif

static PRBool gVerifyReflowEnabled;

NS_LAYOUT PRBool
nsIPresShell::GetVerifyReflowEnable()
{
#ifdef NS_DEBUG
  static PRBool firstTime = PR_TRUE;
  if (firstTime) {
    firstTime = PR_FALSE;
    gLogModule = PR_NewLogModule("verifyreflow");
    gVerifyReflowFlags = gLogModule->level;
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

NS_LAYOUT void
nsIPresShell::SetVerifyReflowEnable(PRBool aEnabled)
{
  gVerifyReflowEnabled = aEnabled;
}

NS_LAYOUT PRInt32
nsIPresShell::GetVerifyReflowFlags()
{
#ifdef NS_DEBUG
  return gVerifyReflowFlags;
#else
  return 0;
#endif
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
  return it->QueryInterface(NS_GET_IID(nsIPresShell),
                            (void **) aInstancePtrResult);
}

PresShell::PresShell():mStackArena(nsnull),
                       mFirstDOMEventRequest(nsnull),
                       mLastDOMEventRequest(nsnull),
                       mFirstAttributeRequest(nsnull),
                       mLastAttributeRequest(nsnull),
                       mFirstCallbackEventRequest(nsnull),
                       mLastCallbackEventRequest(nsnull),
                       mAnonymousContentTable(nsnull)
{
  NS_INIT_REFCNT();
  mIsDestroying = PR_FALSE;
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
#ifdef MOZ_REFLOW_PERF
  DumpReflows();
  if (mReflowCountMgr) {
    delete mReflowCountMgr;
    mReflowCountMgr = nsnull;
  }
#endif

  // release our pref style sheet, if we have one still
  ClearPreferenceStyleRules();

  // if we allocated any stack memory free it.
  FreeDynamicStack();

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

  // Destroy the frame manager. This will destroy the frame hierarchy
  NS_IF_RELEASE(mFrameManager);

  if (mDocument) {
    mDocument->DeleteShell(this);
  }

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

  mStyleSet = dont_QueryInterface(aStyleSet);

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
          SetDisplaySelection(nsISelectionController::SELECTION_ON);
        }
      }      
    }
  }
  
  // Cache the event queue of the current UI thread
  NS_WITH_SERVICE(nsIEventQueueService, eventService, kEventQueueServiceCID, &result);
  if (NS_SUCCEEDED(result))                    // XXX this implies that the UI is the current thread.
    result = eventService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(mEventQueue));
  

  if (gMaxRCProcessingTime == -1) {
    // First, set the defaults
    gMaxRCProcessingTime = NS_MAX_REFLOW_TIME;
    gAsyncReflowDuringDocLoad = PR_TRUE;

    // Get the prefs service
    NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &result);
    if (NS_SUCCEEDED(result)) {
      prefs->GetIntPref("layout.reflow.timeslice", &gMaxRCProcessingTime);
      prefs->GetBoolPref("layout.reflow.async.duringDocLoad", &gAsyncReflowDuringDocLoad);
    }
  }

  // cache the observation service
  mObserverService = do_GetService(NS_OBSERVERSERVICE_CONTRACTID,
                                   &result);
  if (NS_FAILED(result)) {
    return result;
  }

  // cache the drag service so we can check it during reflows
  mDragService = do_GetService("@mozilla.org/widget/dragservice;1");

  // setup the preference style rules up (no forced reflow)
  SetPreferenceStyleRules(PR_FALSE);

  return NS_OK;
}

                  // Dynamic stack memory allocation
NS_IMETHODIMP
PresShell::PushStackMemory()
{
  if (nsnull == mStackArena)
     mStackArena = new StackArena();
   
  return mStackArena->Push();
}

NS_IMETHODIMP
PresShell::PopStackMemory()
{
  if (nsnull == mStackArena)
     mStackArena = new StackArena();

  return mStackArena->Pop();
}

NS_IMETHODIMP
PresShell::AllocateStackMemory(size_t aSize, void** aResult)
{
  if (nsnull == mStackArena)
     mStackArena = new StackArena();

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
    nsAutoString textHtml; textHtml.AssignWithConversion("text/html");
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
    PRInt32 count = mDocument->GetNumberOfStyleSheets();
    PRInt32 index;
    nsAutoString textHtml; textHtml.AssignWithConversion("text/html");
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
  if (mDocument) {
    PRInt32 count = mDocument->GetNumberOfStyleSheets();
    PRInt32 index;
    nsAutoString textHtml; textHtml.AssignWithConversion("text/html");
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
PresShell::EnablePrefStyleRules(PRBool aEnable, PRUint8 aPrefType/*=0xFF*/)
{
  nsresult result = NS_OK;

  // capture change in state
  PRBool bChanging = (mEnablePrefStyleSheet != aEnable) ? PR_TRUE : PR_FALSE;
  // set to desired state
  mEnablePrefStyleSheet = aEnable;

#ifdef NS_DEBUG
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
#ifdef NS_DEBUG
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
    
#ifdef NS_DEBUG
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

      // update the styleset now that we are done inserting our rules
      if (NS_SUCCEEDED(result)) {
        if (mStyleSet) {
          mStyleSet->NotifyStyleSheetStateChanged(PR_FALSE);
        }
      }
    }
#ifdef NS_DEBUG
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

#ifdef NS_DEBUG
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

  result = NS_NewCSSStyleSheet(&mPrefStyleSheet);
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

#ifdef NS_DEBUG
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

#ifdef NS_DEBUG
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
              nsAutoString strRule,strColor;

              // create a rule for each color and add it to the style sheet
              // - the rules are !important so they override all but author
              //   important rules (when put into a backstop stylesheet) and 
              //   all (even author important) when put into an override stylesheet

              ///////////////////////////////////////////////////////////////
              // - default color: '* {color:#RRGGBB !important;}'
              ColorToString(textColor,strColor);
              strRule.Append(NS_LITERAL_STRING("* {color:"));
              strRule.Append(strColor);
              strRule.Append(NS_LITERAL_STRING(" !important;} "));

              ///////////////////////////////////////////////////////////////
              // - default background color: '* {background-color:#RRGGBB !important;}'
              ColorToString(bgColor,strColor);
              strRule.Append(NS_LITERAL_STRING("* {background-color:"));
              strRule.Append(strColor);
              strRule.Append(NS_LITERAL_STRING(" !important;} "));
              
              // now insert the rule
              result = sheet->InsertRule(strRule,0,&index);
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

#ifdef NS_DEBUG
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
          nsAutoString strRule, strColor;
          PRBool useDocColors = PR_TRUE;

          // see if we need to create the rules first
          mPresContext->GetCachedBoolPref(kPresContext_UseDocumentColors, useDocColors);

          ///////////////////////////////////////////////////////////////
          // - links: '*:link, *:link:active {color: #RRGGBB [!important];}'
          ColorToString(linkColor,strColor);
          strRule.Append(NS_LITERAL_STRING("*:link, *:link:active {color:"));
          strRule.Append(strColor);
          if (!useDocColors) {
            strRule.Append(NS_LITERAL_STRING(" !important;} "));
          } else {
            strRule.Append(NS_LITERAL_STRING(";} "));
          }
 
          ///////////////////////////////////////////////////////////////
          // - visited links '*:visited, *:visited:active {color: #RRGGBB [!important];}'
          ColorToString(visitedColor,strColor);
          strRule.Append(NS_LITERAL_STRING("*:visited, *:visited:active {color:"));
          strRule.Append(strColor);
          if (!useDocColors) {
            strRule.Append(NS_LITERAL_STRING(" !important;} "));
          } else {
            strRule.Append(NS_LITERAL_STRING(";} "));
          }

          // insert the rules
          result = sheet->InsertRule(strRule,0,&index);
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
  #ifdef NS_DEBUG
              printf (" - Creating rules for enabling link underlines\n");
  #endif
              // make a rule to make text-decoration: underline happen for links
              strRule.Append(NS_LITERAL_STRING(":link, :visited {text-decoration:underline;}"));
            } else {
  #ifdef NS_DEBUG
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

#ifdef INCLUDE_XUL
static void CheckForFocus(nsIDocument* aDocument)
{
  // Now that we have a root frame, set focus in to the presshell, but
  // only do this if our window is currently focused
  // Restore focus if we're the active window or a parent of a previously
  // active window.
  // XXX The XUL dependency will go away when the focus tracking portion
  // of the command dispatcher moves into the embedding layer.
  nsCOMPtr<nsIScriptGlobalObject> globalObject;
  aDocument->GetScriptGlobalObject(getter_AddRefs(globalObject));  
  nsCOMPtr<nsIDOMWindowInternal> rootWindow;
  nsCOMPtr<nsPIDOMWindow> ourWindow = do_QueryInterface(globalObject);
  ourWindow->GetPrivateRoot(getter_AddRefs(rootWindow));
  nsCOMPtr<nsIDOMDocument> rootDocument;
  rootWindow->GetDocument(getter_AddRefs(rootDocument));

  nsCOMPtr<nsIDOMXULCommandDispatcher> commandDispatcher;
  nsCOMPtr<nsIDOMXULDocument> xulDoc = do_QueryInterface(rootDocument);
  if (xulDoc) {
    // See if we have a command dispatcher attached.
    xulDoc->GetCommandDispatcher(getter_AddRefs(commandDispatcher));
    if (commandDispatcher) {
      // Suppress the command dispatcher.
      commandDispatcher->SetSuppressFocus(PR_TRUE);
      nsCOMPtr<nsIDOMWindowInternal> focusedWindow;
      commandDispatcher->GetFocusedWindow(getter_AddRefs(focusedWindow));
      
      // See if the command dispatcher is holding on to an orphan window.
      // This happens when you move from an inner frame to an outer frame
      // (e.g., _parent, _top)
      if (focusedWindow) {
        nsCOMPtr<nsIDOMDocument> domDoc;
        focusedWindow->GetDocument(getter_AddRefs(domDoc));
        if (!domDoc) {
          // We're pointing to garbage. Go ahead and let this
          // presshell take the focus.
          focusedWindow = do_QueryInterface(ourWindow);
          commandDispatcher->SetFocusedWindow(focusedWindow);
        }
      }

      nsCOMPtr<nsIDOMWindowInternal> domWindow = do_QueryInterface(ourWindow);
      if (domWindow == focusedWindow) {
        PRBool active;
        commandDispatcher->GetActive(&active);
        commandDispatcher->SetFocusedElement(nsnull);
        if(active) {
          // We need to restore focus and make sure we null
          // out the focused element.
          domWindow->Focus();
        }
      }
      commandDispatcher->SetSuppressFocus(PR_FALSE);
    }
  }
}
#endif


nsresult
PresShell::NotifyReflowObservers(const char *aData)
{
  if (!aData) { return NS_ERROR_NULL_POINTER; }

  nsresult               result = NS_OK;
  nsCOMPtr<nsISupports>  pContainer;
  nsCOMPtr<nsIDocShell>  pDocShell;
  nsAutoString           sTopic,
                         sData;


  result = mPresContext->GetContainer( getter_AddRefs( pContainer ) );

  if (NS_SUCCEEDED( result ) && pContainer) {
    pDocShell = do_QueryInterface( pContainer,
                                  &result );

    if (NS_SUCCEEDED( result ) && pDocShell && mObserverService) {
      sTopic.AssignWithConversion( NS_PRESSHELL_REFLOW_TOPIC );
      sData.AssignWithConversion( aData );
      result = mObserverService->Notify( pDocShell,
                                         sTopic.GetUnicode( ),
                                         sData.GetUnicode( ) );
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
  nsIContent* root = nsnull;

#ifdef NS_DEBUG
  if (VERIFY_REFLOW_NOISY_RC & gVerifyReflowFlags) {
    nsCOMPtr<nsIURI> uri;
    if (mDocument) {
      uri = dont_AddRef(mDocument->GetDocumentURL());
      if (uri) {
        char* url = nsnull;
        uri->GetSpec(&url);
        printf("*** PresShell::InitialReflow (this=%p, url='%s')\n", this, url);
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
    root = mDocument->GetRootContent();
  }

  // Get the root frame from the frame manager
  nsIFrame* rootFrame;
  mFrameManager->GetRootFrame(&rootFrame);
  
  if (nsnull != root) {
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

#ifdef INCLUDE_XUL
    CheckForFocus(mDocument);
#endif

    // Have the style sheet processor construct frame for the root
    // content object down
    mStyleSet->ContentInserted(mPresContext, nsnull, root, 0);
    NS_RELEASE(root);
    VERIFY_STYLE_TREE;
    MOZ_TIMER_DEBUGLOG(("Stop: Frame Creation: PresShell::InitialReflow(), this=%p\n", this));
    MOZ_TIMER_STOP(mFrameCreationWatch);
    CtlStyleWatch(kStyleWatchDisable,mStyleSet);
  }

  if (rootFrame) {
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
    nsIView*          view;

    rootFrame->WillReflow(mPresContext);
    rootFrame->GetView(mPresContext, &view);
    if (view) {
      nsContainerFrame::PositionFrameView(mPresContext, rootFrame, view);
    }
    rootFrame->Reflow(mPresContext, desiredSize, reflowState, status);
    rootFrame->SizeTo(mPresContext, desiredSize.width, desiredSize.height);
    mPresContext->SetVisibleArea(nsRect(0,0,desiredSize.width,desiredSize.height));
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

  return NS_OK; //XXX this needs to be real. MMP
}

NS_IMETHODIMP
PresShell::ResizeReflow(nscoord aWidth, nscoord aHeight)
{
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
    nsIView*          view;

    rootFrame->WillReflow(mPresContext);
    rootFrame->GetView(mPresContext, &view);
    if (view) {
      nsContainerFrame::PositionFrameView(mPresContext, rootFrame, view);
    }
    rootFrame->Reflow(mPresContext, desiredSize, reflowState, status);
    rootFrame->SizeTo(mPresContext, desiredSize.width, desiredSize.height);
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

  //Send resize event from here.
  nsEvent event;
  nsEventStatus status;
  event.eventStructType = NS_EVENT;
  event.message = NS_RESIZE_EVENT;
  event.time = 0;

  nsCOMPtr<nsIScriptGlobalObject> globalObj;
	mDocument->GetScriptGlobalObject(getter_AddRefs(globalObj));
  globalObj->HandleDOMEvent(mPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
  
  return NS_OK; //XXX this needs to be real. MMP
}

NS_IMETHODIMP
PresShell::ScrollFrameIntoView(nsIFrame *aFrame){
  if (!aFrame)
    return NS_ERROR_NULL_POINTER;

#ifdef INCLUDE_XUL    
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
      nsCOMPtr<nsIDOMXULCommandDispatcher> commandDispatcher;
	  nsCOMPtr<nsIScriptGlobalObject> ourGlobal;
	  document->GetScriptGlobalObject(getter_AddRefs(ourGlobal));
      nsCOMPtr<nsIDOMWindowInternal> rootWindow;
      nsCOMPtr<nsPIDOMWindow> ourWindow = do_QueryInterface(ourGlobal);
      if(ourWindow) {
        ourWindow->GetPrivateRoot(getter_AddRefs(rootWindow));
        if(rootWindow) {
          nsCOMPtr<nsIDOMDocument> rootDocument;
          rootWindow->GetDocument(getter_AddRefs(rootDocument));

          nsCOMPtr<nsIDOMXULDocument> xulDoc = do_QueryInterface(rootDocument);
          if (xulDoc) {
            // See if we have a command dispatcher attached.
            xulDoc->GetCommandDispatcher(getter_AddRefs(commandDispatcher));
            if (commandDispatcher) {
              PRBool dontScroll;
              commandDispatcher->GetSuppressFocusScroll(&dontScroll);
              if(dontScroll)
                return NS_OK;
			}
		  }
		}
	  }
    }
  }
#endif    
  if (IsScrollingEnabled())
    return ScrollFrameIntoView(aFrame, NS_PRESSHELL_SCROLL_ANYWHERE,
                               NS_PRESSHELL_SCROLL_ANYWHERE);
  return NS_OK;
}

NS_IMETHODIMP
PresShell::NotifyDestroyingFrame(nsIFrame* aFrame)
{
  // Cancel any pending reflow commands targeted at this frame
  CancelReflowCommand(aFrame, nsnull);

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
  return mSelection->LineMove(aForward, aExtend);  
}

NS_IMETHODIMP 
PresShell::IntraLineMove(PRBool aForward, PRBool aExtend)
{
  return mSelection->IntraLineMove(aForward, aExtend);  
}

NS_IMETHODIMP 
PresShell::PageMove(PRBool aForward, PRBool aExtend)
{
  return ScrollPage(aForward);
#if 0

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
  nsAutoString bodyTag; bodyTag.AssignWithConversion("body"); 

  nsCOMPtr<nsIDOMDocument> doc = do_QueryInterface(document);
  if (!doc) 
    return NS_ERROR_FAILURE;
  nsresult result = doc->GetElementsByTagName(bodyTag, getter_AddRefs(nodeList));

  if (NS_FAILED(result) || !nodeList)
    return result?result:NS_ERROR_NULL_POINTER;

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
        PRInt32 offset = 0;
        if (aForward)
        {
          bodyContent->ChildCount(offset);
        }
        result = mSelection->HandleClick(bodyContent,offset,offset,aExtend, PR_FALSE,aExtend);
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
    nsIView*          view;

    rootFrame->WillReflow(mPresContext);
    rootFrame->GetView(mPresContext, &view);
    if (view) {
      nsContainerFrame::PositionFrameView(mPresContext, rootFrame, view);
    }
    rootFrame->Reflow(mPresContext, desiredSize, reflowState, status);
    rootFrame->SizeTo(mPresContext, desiredSize.width, desiredSize.height);
    mPresContext->SetVisibleArea(nsRect(0,0,desiredSize.width,desiredSize.height));
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
  if (rootFrame && mHistoryState) {
    nsIFrame* scrollFrame = nsnull;
    GetRootScrollFrame(mPresContext, rootFrame, &scrollFrame);
    if (scrollFrame) {
      mFrameManager->RestoreFrameStateFor(mPresContext, scrollFrame, mHistoryState, nsIStatefulFrame::eDocumentScrollState);
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
PresShell::AlreadyInQueue(nsIReflowCommand* aReflowCommand)
{
  PRInt32 i, n = mReflowCommands.Count();
  nsIFrame* targetFrame;
  PRBool inQueue = PR_FALSE;    

  if (NS_SUCCEEDED(aReflowCommand->GetTarget(targetFrame))) {
    // Iterate over the reflow commands and compare the targeted frames.
    for (i = 0; i < n; i++) {
      nsIReflowCommand* rc = (nsIReflowCommand*) mReflowCommands.ElementAt(i);
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
              printf("*** PresShell::AlreadyInQueue(): Discarding reflow command: this=%p\n", this);
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

NS_IMETHODIMP
PresShell::AppendReflowCommand(nsIReflowCommand* aReflowCommand)
{
#ifdef DEBUG
  //printf("gShellCounter: %d\n", gShellCounter++);
  if (mInVerifyReflow) {
    return NS_OK;
  }  
  if (VERIFY_REFLOW_NOISY_RC & gVerifyReflowFlags) {
    printf("\nPresShell@%p: adding reflow command\n", this);
    aReflowCommand->List(stdout);
    if (VERIFY_REFLOW_REALLY_NOISY_RC & gVerifyReflowFlags) {
      printf("Current content model:\n");
      nsCOMPtr<nsIContent> rootContent;
      rootContent = getter_AddRefs(mDocument->GetRootContent());
      if (rootContent) {
        rootContent->List(stdout, 0);
      }
    }
  }  
#endif

  // Add the reflow command to the queue
  nsresult rv = NS_OK;
  if (!AlreadyInQueue(aReflowCommand)) {
    NS_ADDREF(aReflowCommand);
    rv = (mReflowCommands.AppendElement(aReflowCommand) ? NS_OK : NS_ERROR_OUT_OF_MEMORY);
    ReflowCommandAdded(aReflowCommand);
  }

  // For async reflow during doc load, post a reflow event if we are not batching reflow commands.
  // For sync reflow during doc load, post a reflow event if we are not batching reflow commands
  // and the document is not loading.
  if ((gAsyncReflowDuringDocLoad && !mBatchReflows) ||
      (!gAsyncReflowDuringDocLoad && !mBatchReflows && !mDocumentLoading)) {
    // If we're in the middle of a drag, process it right away (needed for mac,
    // might as well do it on all platforms just to keep the code paths the same).
    if ( IsDragInProgress() )
      FlushPendingNotifications();
    else
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
PresShell::CancelReflowCommand(nsIFrame* aTargetFrame, nsIReflowCommand::ReflowType* aCmdType)
{
  PRInt32 i, n = mReflowCommands.Count();
  for (i = 0; i < n; i++) {
    nsIReflowCommand* rc = (nsIReflowCommand*) mReflowCommands.ElementAt(i);
    if (rc) {
      nsIFrame* target;      
      if (NS_SUCCEEDED(rc->GetTarget(target))) {
        if (target == aTargetFrame) {
          if (aCmdType != NULL) {
            // If aCmdType is specified, only remove reflow commands of that type
            nsIReflowCommand::ReflowType type;
            if (NS_SUCCEEDED(rc->GetType(type))) {
              if (type != *aCmdType)
                continue;
            }
          }
#ifdef DEBUG
          if (VERIFY_REFLOW_NOISY_RC & gVerifyReflowFlags) {
            printf("PresShell: removing rc=%p for frame ", rc);
            nsFrame::ListTag(stdout, aTargetFrame);
            printf("\n");
          }
#endif
          mReflowCommands.RemoveElementAt(i);
          ReflowCommandRemoved(rc);
          NS_RELEASE(rc);
          n--;
          i--;
          continue;
        }
      }
    }
  }
  return NS_OK;
}


NS_IMETHODIMP
PresShell::CancelAllReflowCommands()
{
  PRInt32 n = mReflowCommands.Count();
  nsIReflowCommand* rc;
  for (PRInt32 i = 0; i < n; i++) {
    rc = NS_STATIC_CAST(nsIReflowCommand*, mReflowCommands.ElementAt(0));
    mReflowCommands.RemoveElementAt(0);
    ReflowCommandRemoved(rc);
    NS_RELEASE(rc);
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
PresShell::GoToAnchor(const nsString& aAnchorName) const
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
          if (tagName.EqualsWithConversion("a")) {
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
    }
  } else {
    rv = NS_ERROR_FAILURE;
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
#ifdef INCLUDE_XUL
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
      nsCOMPtr<nsIDOMXULCommandDispatcher> commandDispatcher;
	  nsCOMPtr<nsIScriptGlobalObject> ourGlobal;
	  document->GetScriptGlobalObject(getter_AddRefs(ourGlobal));
      nsCOMPtr<nsIDOMWindowInternal> rootWindow;
      nsCOMPtr<nsPIDOMWindow> ourWindow = do_QueryInterface(ourGlobal);
      if(ourWindow) {
        ourWindow->GetPrivateRoot(getter_AddRefs(rootWindow));
        if(rootWindow) {
          nsCOMPtr<nsIDOMDocument> rootDocument;
          rootWindow->GetDocument(getter_AddRefs(rootDocument));

          nsCOMPtr<nsIDOMXULDocument> xulDoc = do_QueryInterface(rootDocument);
          if (xulDoc) {
            // See if we have a command dispatcher attached.
            xulDoc->GetCommandDispatcher(getter_AddRefs(commandDispatcher));
            if (commandDispatcher) {
              PRBool dontScroll;
              commandDispatcher->GetSuppressFocusScroll(&dontScroll);
              if(dontScroll)
                return NS_OK;
			}
		  }
		}
	  }
    }
  }
#endif
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
      if (scrollOffsetX != 0 || scrollOffsetY != 0) {
        scrollingView->ScrollTo(scrollOffsetX, scrollOffsetY, NS_VMREFRESH_IMMEDIATE);
      }
    }
  }
  return rv;
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
  if (NS_FAILED(rv) || !manager)
    return rv?rv:NS_ERROR_FAILURE;
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
      if (NS_FAILED(rv) || !htmlInputFrame)
        return rv?rv:NS_ERROR_FAILURE;
      nsCOMPtr<nsISelectionController> selCon;
      rv = htmlInputFrame->GetSelectionController(mPresContext,getter_AddRefs(selCon));
      if (NS_FAILED(rv) || !selCon)
        return rv?rv:NS_ERROR_FAILURE;
      rv = selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(sel));
    }
  }
  if (!sel) //get selection from this PresShell
    rv = GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(sel));
    
  if (NS_FAILED(rv) || !sel)
    return rv?rv:NS_ERROR_FAILURE;

  // Now we have the selection.  Make sure it's nonzero:
  PRBool isCollapsed;
  sel->GetIsCollapsed(&isCollapsed);
  if (isCollapsed)
    return NS_OK;

  nsCOMPtr<nsIDocumentEncoder> docEncoder;

  docEncoder = do_CreateInstance(NS_HTMLCOPY_ENCODER_CONTRACTID);
  NS_ENSURE_TRUE(docEncoder, NS_ERROR_FAILURE);

  docEncoder->Init(doc, NS_LITERAL_STRING("text/html"), 0);
  docEncoder->SetSelection(sel);

  nsAutoString buffer, parents, info;
  
  rv = docEncoder->EncodeToStringWithContext(buffer, parents, info);
  if (NS_FAILED(rv)) 
    return rv;
  
  // Get the Clipboard
  NS_WITH_SERVICE(nsIClipboard, clipboard, kCClipboardCID, &rv);
  if (NS_FAILED(rv)) 
    return rv;

  if ( clipboard ) 
  {
    // Create a transferable for putting data on the Clipboard
    nsCOMPtr<nsITransferable> trans;
    rv = nsComponentManager::CreateInstance(kCTransferableCID, nsnull, 
                                            NS_GET_IID(nsITransferable), 
                                            getter_AddRefs(trans));
    if ( trans ) 
    {
      // set up the data converter
      nsCOMPtr<nsIFormatConverter> htmlConverter = do_CreateInstance(kHTMLConverterCID);
      NS_ENSURE_TRUE(htmlConverter, NS_ERROR_FAILURE);
      trans->SetConverter(htmlConverter);
      
      // Add the html DataFlavor to the transferable
      trans->AddDataFlavor(kHTMLMime);
      // Add the htmlcontext DataFlavor to the transferable
      trans->AddDataFlavor(kHTMLContext);
      // Add the htmlinfo DataFlavor to the transferable
      trans->AddDataFlavor(kHTMLInfo);
      
      // get wStrings to hold clip data
      nsCOMPtr<nsISupportsWString> dataWrapper, contextWrapper, infoWrapper;
      dataWrapper = do_CreateInstance(NS_SUPPORTS_WSTRING_CONTRACTID);
      NS_ENSURE_TRUE(dataWrapper, NS_ERROR_FAILURE);
      contextWrapper = do_CreateInstance(NS_SUPPORTS_WSTRING_CONTRACTID);
      NS_ENSURE_TRUE(contextWrapper, NS_ERROR_FAILURE);
      infoWrapper = do_CreateInstance(NS_SUPPORTS_WSTRING_CONTRACTID);
      NS_ENSURE_TRUE(infoWrapper, NS_ERROR_FAILURE);

      // populate the strings
      dataWrapper->SetData ( NS_CONST_CAST(PRUnichar*,buffer.GetUnicode()) );
      contextWrapper->SetData ( NS_CONST_CAST(PRUnichar*,parents.GetUnicode()) );
      infoWrapper->SetData ( NS_CONST_CAST(PRUnichar*,info.GetUnicode()) );
      
      // QI the data object an |nsISupports| so that when the transferable holds
      // onto it, it will addref the correct interface.
      nsCOMPtr<nsISupports> genericDataObj ( do_QueryInterface(dataWrapper) );
      trans->SetTransferData(kHTMLMime, genericDataObj, buffer.Length()*2);
      genericDataObj = do_QueryInterface(contextWrapper);
      trans->SetTransferData(kHTMLContext, genericDataObj, parents.Length()*2);
      genericDataObj = do_QueryInterface(infoWrapper);
      trans->SetTransferData(kHTMLInfo, genericDataObj, info.Length()*2);

      // put the transferable on the clipboard
      clipboard->SetData(trans, nsnull, nsIClipboard::kGlobalClipboard);
    }
  }
  
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

  if (!mHistoryState) {
    // Create the document state object
    rv = NS_NewLayoutHistoryState(aState); // This addrefs
  
    if (NS_FAILED(rv)) { 
      *aState = nsnull;
      return rv;
    }    

    mHistoryState = *aState;
  }
  else {
    *aState = mHistoryState;
    NS_IF_ADDREF(mHistoryState);
  }
  
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
      rv = mFrameManager->CaptureFrameStateFor(mPresContext, scrollFrame, mHistoryState, nsIStatefulFrame::eDocumentScrollState);
    }
  }


  rv = mFrameManager->CaptureFrameState(mPresContext, rootFrame, mHistoryState);  
 
  return rv;
}

NS_IMETHODIMP
PresShell::GetHistoryState(nsILayoutHistoryState** aState)
{
  NS_IF_ADDREF(mHistoryState);
  *aState = mHistoryState;
  return NS_OK;
}

NS_IMETHODIMP
PresShell::SetHistoryState(nsILayoutHistoryState* aLayoutHistoryState)
{
  mHistoryState = aLayoutHistoryState;
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
        rv = NS_NewGeneratedContentIterator(mPresContext, firstChildFrame, aIterator);
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
          rv = NS_NewGeneratedContentIterator(mPresContext, lastChildFrame, aIterator);
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
      callback->ReflowFinished(this, &shouldFlush);
      NS_RELEASE(callback);
   }

   mFirstCallbackEventRequest = mLastCallbackEventRequest = nsnull;

   if (shouldFlush)
     FlushPendingNotifications();
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
         node->content->SetAttribute(node->nameSpaceID, node->name, node->value, node->notify);   
      else
         node->content->UnsetAttribute(node->nameSpaceID, node->name, node->notify);   

      NS_RELEASE(node->content);
      node->nsAttributeChangeRequest::~nsAttributeChangeRequest();
      FreeFrame(sizeof(nsAttributeChangeRequest), node);
   }
}

NS_IMETHODIMP 
PresShell::FlushPendingNotifications()
{
  if (!mIsReflowing) {
    ProcessReflowCommands(PR_FALSE);
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
    rv = FlushPendingNotifications();
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
                            PRInt32      aHint)
{
  WillCauseReflow();
  nsresult rv = mStyleSet->AttributeChanged(mPresContext, aContent, aNameSpaceID, aAttribute, aHint);
  VERIFY_STYLE_TREE;
  DidCauseReflow();
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

  if (NS_SUCCEEDED(rv) && nsnull != mHistoryState) {
    // If history state has been set by session history, ask the frame manager 
    // to restore frame state for the frame hierarchy created for the chunk of
    // content that just came in.
    nsIFrame* frame;
    rv = GetPrimaryFrameFor(aContainer, &frame);
    if (NS_SUCCEEDED(rv) && nsnull != frame)
      mFrameManager->RestoreFrameState(mPresContext, frame, mHistoryState);
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
  const nsStylePosition* position;
  aFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&) position);

  // 'clip' only applies to absolutely positioned elements, and is
  // relative to the element's border edge. 'clip' applies to the entire
  // element: border, padding, and content areas, and even scrollbars if
  // there are any.
  if (position->IsAbsolutelyPositioned() && (display->mClipFlags & NS_STYLE_CLIP_RECT)) {
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
      if (NS_IS_KEY_EVENT(aEvent) || NS_IS_IME_EVENT(aEvent)) { 
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
                mCurrentEventContent = mDocument->GetRootContent();
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
         printf("\n*** Handling reflow event: PresShell=%p, event=%p\n", presShell.get(), this);
      }
#endif
      // XXX Statically cast the pres shell pointer so that we can call
      // protected methods on the pres shell. (the ReflowEvent struct
      // is a friend of the PresShell class)
      PresShell* ps = NS_REINTERPRET_CAST(PresShell*, presShell.get());
      PRBool isBatching;
      ps->SetReflowEventStatus(PR_FALSE);
      ps->GetReflowBatchingStatus(&isBatching);
      if (!isBatching) ps->ProcessReflowCommands(PR_TRUE);
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
      printf("\n*** PresShell::PostReflowEvent(), this=%p, event=%p\n", this, ev);
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
  mViewManager->CacheWidgetChanges(PR_FALSE);

  if (!gAsyncReflowDuringDocLoad && mDocumentLoading) {
    FlushPendingNotifications();
  }

  return NS_OK;
}

nsresult
PresShell::ProcessReflowCommands(PRBool aInterruptible)
{
  MOZ_TIMER_DEBUGLOG(("Start: Reflow: PresShell::ProcessReflowCommands(), this=%p\n", this));
  MOZ_TIMER_START(mReflowWatch);  
  PRTime beforeReflow, afterReflow;
  PRInt64 diff;    

  if (0 != mReflowCommands.Count()) {
    nsHTMLReflowMetrics   desiredSize(nsnull);
    nsIRenderingContext*  rcx;
    nsIFrame*             rootFrame;        


    mFrameManager->GetRootFrame(&rootFrame);
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
      printf("\nPresShell::ProcessReflowCommands: this=%p, count=%d\n", this, n);
      for (i = 0; i < n; i++) {
        nsIReflowCommand* rc = (nsIReflowCommand*)
          mReflowCommands.ElementAt(i);
        rc->List(stdout);
      }
    }
#endif
    
    mIsReflowing = PR_TRUE;
    while (0 != mReflowCommands.Count()) {
      // Use RemoveElementAt in case the reflowcommand dispatches a
      // new one during its execution.
      nsIReflowCommand* rc = (nsIReflowCommand*) mReflowCommands.ElementAt(0);
      mReflowCommands.RemoveElementAt(0);

      // Dispatch the reflow command
      nsSize          maxSize;
      rootFrame->GetSize(maxSize);
      if (aInterruptible) beforeReflow = PR_Now();
      rc->Dispatch(mPresContext, desiredSize, maxSize, *rcx); // dispatch the reflow command
      if (aInterruptible) afterReflow = PR_Now();
      ReflowCommandRemoved(rc);
      NS_RELEASE(rc);
      VERIFY_STYLE_TREE;

      if (aInterruptible) {
        PRInt64 totalTime;
        PRInt64 maxTime;
        LL_SUB(diff, afterReflow, beforeReflow);

        LL_I2L(totalTime, mAccumulatedReflowTime);
        LL_ADD(totalTime, totalTime, diff);
        LL_L2I(mAccumulatedReflowTime, totalTime);

        LL_I2L(maxTime, gMaxRCProcessingTime);
        if (LL_CMP(totalTime, >, maxTime))
          break;          
      }
    }
    NS_IF_RELEASE(rcx);
    mIsReflowing = PR_FALSE;

    if (aInterruptible) {
      if (mReflowCommands.Count() > 0) {
        // Reflow Commands are still queued up.
        // Schedule a reflow event to handle them asynchronously.
        PostReflowEvent();
      }
#ifdef DEBUG
      if (VERIFY_REFLOW_DUMP_COMMANDS & gVerifyReflowFlags) {        
        printf("Time spent in PresShell::ProcessReflowCommands(), this=%p, time=%d micro seconds\n", 
          this, mAccumulatedReflowTime);
      }
#endif
      mAccumulatedReflowTime = 0;
    }
    
#ifdef DEBUG
    if (VERIFY_REFLOW_DUMP_COMMANDS & gVerifyReflowFlags) {
      printf("\nPresShell::ProcessReflowCommands() finished: this=%p\n", this);
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
  }
  
  MOZ_TIMER_DEBUGLOG(("Stop: Reflow: PresShell::ProcessReflowCommands(), this=%p\n", this));
  MOZ_TIMER_STOP(mReflowWatch);  

  HandlePostedDOMEvents();
  HandlePostedAttributeChanges();
  HandlePostedReflowCallbacks();
  return NS_OK;
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
  nsIStyleSet* clone;
  nsresult rv = NS_NewStyleSet(&clone);
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
  *aResult = clone;
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

      if (!mDummyLayoutRequest) {
        AddDummyLayoutRequest();
      }

  #ifdef DEBUG_nisheeth
      printf("presshell=%p, mRCCreatedDuringLoad=%d\n", this, mRCCreatedDuringLoad);
  #endif
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
  #ifdef DEBUG_nisheeth
      printf("presshell=%p, mRCCreatedDuringLoad=%d\n", this, mRCCreatedDuringLoad);
  #endif
    }

    if (mRCCreatedDuringLoad == 0 && !mDocumentLoading && mDummyLayoutRequest)
      RemoveDummyLayoutRequest();
  }
  return NS_OK;
}


nsresult
PresShell::AddDummyLayoutRequest(void)
{ 
  nsresult rv = NS_OK;

  if (gAsyncReflowDuringDocLoad) {
    rv = DummyLayoutRequest::Create(getter_AddRefs(mDummyLayoutRequest), this);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsILoadGroup> loadGroup;
    if (mDocument) {
      rv = mDocument->GetDocumentLoadGroup(getter_AddRefs(loadGroup));
      if (NS_FAILED(rv)) return rv;
    }

    if (loadGroup) {
      rv = mDummyLayoutRequest->SetLoadGroup(loadGroup);
      if (NS_FAILED(rv)) return rv;

      rv = loadGroup->AddChannel(mDummyLayoutRequest, nsnull);
      if (NS_FAILED(rv)) return rv;
    }

#ifdef DEBUG_nisheeth
    printf("presshell=%p, Added dummy layout request.\n", this);
#endif
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
      rv = loadGroup->RemoveChannel(mDummyLayoutRequest, nsnull, NS_OK, nsnull);
      if (NS_FAILED(rv)) return rv;

      mDummyLayoutRequest = nsnull;
    }

#ifdef DEBUG_nisheeth
    printf("presshell=%p, Removed dummy layout request.\n", this);
#endif
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
    name.AssignWithConversion("(null)");
  }
  fputs(name, stdout);

  printf(" != ");

  if (nsnull != k2) {
    nsIFrameDebug*  frameDebug;

    if (NS_SUCCEEDED(k2->QueryInterface(NS_GET_IID(nsIFrameDebug),
                                        (void**)&frameDebug))) {
      frameDebug->GetFrameName(name);
    }
  }
  else {
    name.AssignWithConversion("(null)");
  }
  fputs(name, stdout);

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
    fputs(name, stdout);
    fprintf(stdout, " %p ", k1);
  }
  printf("{%d, %d, %d, %d}", r1.x, r1.y, r1.width, r1.height);

  printf(" != \n");

  if (NS_SUCCEEDED(k2->QueryInterface(NS_GET_IID(nsIFrameDebug),
                                      (void**)&frameDebug))) {
    fprintf(stdout, "  ");
    frameDebug->GetFrameName(name);
    fputs(name, stdout);
    fprintf(stdout, " %p ", k2);
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

  cx->Stop();
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


//-------------------------------------------------------------
//-- Reflow counts
//-------------------------------------------------------------
#ifdef MOZ_REFLOW_PERF
//-------------------------------------------------------------
NS_IMETHODIMP
PresShell::ClearTotals()
{
  return NS_OK;
}

//-------------------------------------------------------------
NS_IMETHODIMP
PresShell::DumpReflows()
{
  if (mReflowCountMgr) {
    char * uriStr = nsnull;
    if (mDocument) {
      nsIURI * uri = mDocument->GetDocumentURL();
      if (uri) {
        uri->GetPath(&uriStr);
        NS_RELEASE(uri);
      }
    }
    mReflowCountMgr->DisplayTotals(uriStr);
    mReflowCountMgr->DisplayHTMLTotals(uriStr);
    mReflowCountMgr->DisplayDiffsInTotals("Differences");
    if (uriStr) delete [] uriStr;
  }
  return NS_OK;
}

//-------------------------------------------------------------
NS_IMETHODIMP
PresShell::CountReflows(const char * aName, PRUint32 aType)
{
  if (mReflowCountMgr) {
    //mReflowCountMgr->Add(aName, (nsReflowReason)aType);
  }
  return NS_OK;
}

//------------------------------------------------------------------
//-- Reflow Counter Classes Impls
//------------------------------------------------------------------
//ReflowCountMgr ReflowCountMgr::gReflowCountMgr;

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
  //DisplayTotals(mTotals, "Grand Totals");
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
ReflowCountMgr::ReflowCountMgr()
{
  mCounts = PL_NewHashTable(10, PL_HashString, PL_CompareStrings, 
                                PL_CompareValues, nsnull, nsnull);
  mCycledOnce = PR_FALSE;
}

//------------------------------------------------------------------
ReflowCountMgr::~ReflowCountMgr()
{
  //if (GetInstance() == this) {
  //  DoGrandTotals();
  //}
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
void ReflowCountMgr::Add(const char * aName, nsReflowReason aType)
{
  if (nsnull != mCounts) {
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
void ReflowCountMgr::CleanUp()
{
  if (nsnull != mCounts) {
    PL_HashTableEnumerateEntries(mCounts, RemoveItems, nsnull);
    PL_HashTableDestroy(mCounts);
    mCounts = nsnull;
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
  printf("%s\n", aStr?aStr:"No name");
  DoGrandTotals();

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
  PRBool cycledOnce = (PRBool)arg;

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

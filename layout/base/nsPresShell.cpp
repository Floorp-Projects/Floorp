/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Steve Clark <buster@netscape.com>
 *   Håkan Waara <hwaara@chello.se>
 *   Dan Rosen <dr@netscape.com>
 *   Daniel Glazman <glazman@netscape.com>
 *   Mats Palmgren <matspal@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
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

/* a presentation of a document, part 2 */

#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIContent.h"
#include "mozilla/dom/Element.h"
#include "nsIDocument.h"
#include "nsIDOMXULDocument.h"
#include "nsStubDocumentObserver.h"
#include "nsStyleSet.h"
#include "nsCSSStyleSheet.h" // XXX for UA sheet loading hack, can this go away please?
#include "nsIDOMCSSStyleSheet.h"  // for Pref-related rule management (bugs 22963,20760,31816)
#include "nsINameSpaceManager.h"  // for Pref-related rule management (bugs 22963,20760,31816)
#include "nsIServiceManager.h"
#include "nsFrame.h"
#include "nsIViewManager.h"
#include "nsCRT.h"
#include "nsCRTGlue.h"
#include "prlog.h"
#include "prmem.h"
#include "prprf.h"
#include "prinrval.h"
#include "nsTArray.h"
#include "nsCOMArray.h"
#include "nsHashtable.h"
#include "nsIViewObserver.h"
#include "nsContainerFrame.h"
#include "nsIDeviceContext.h"
#include "nsEventStateManager.h"
#include "nsDOMEvent.h"
#include "nsGUIEvent.h"
#include "nsHTMLParts.h"
#include "nsContentUtils.h"
#include "nsISelection.h"
#include "nsISelectionController.h"
#include "nsISelectionPrivate.h"
#include "nsLayoutCID.h"
#include "nsGkAtoms.h"
#include "nsIDOMRange.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOM3Node.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMElement.h"
#include "nsRange.h"
#include "nsCSSPseudoElements.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsWeakReference.h"
#include "nsIPageSequenceFrame.h"
#include "nsCaret.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIXPointer.h"
#include "nsIDOMXMLDocument.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsFrameSelection.h"
#include "nsIDOMNSHTMLTextAreaElement.h"
#include "nsViewsCID.h"
#include "nsPresArena.h"
#include "nsFrameManager.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsILayoutHistoryState.h"
#include "nsILineIterator.h" // for ScrollContentIntoView
#include "nsWeakPtr.h"
#include "pldhash.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsIDocShell.h"        // for reflow observation
#include "nsIBaseWindow.h"
#include "nsLayoutErrors.h"
#include "nsLayoutUtils.h"
#include "nsCSSRendering.h"
  // for |#ifdef DEBUG| code
#include "prenv.h"
#include "nsIAttribute.h"
#include "nsIGlobalHistory2.h"
#include "nsDisplayList.h"
#include "nsIRegion.h"
#include "nsRegion.h"

#ifdef MOZ_REFLOW_PERF
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#endif

#include "nsIReflowCallback.h"

#include "nsPIDOMWindow.h"
#include "nsFocusManager.h"
#include "nsIPluginInstance.h"
#include "nsIObjectFrame.h"
#include "nsIObjectLoadingContent.h"
#include "nsNetUtil.h"
#include "nsEventDispatcher.h"
#include "nsThreadUtils.h"
#include "nsStyleSheetService.h"
#include "gfxImageSurface.h"
#include "gfxContext.h"
#ifdef MOZ_MEDIA
#include "nsHTMLMediaElement.h"
#endif
#ifdef MOZ_SMIL
#include "nsSMILAnimationController.h"
#endif

#include "nsRefreshDriver.h"

// Drag & Drop, Clipboard
#include "nsWidgetsCID.h"
#include "nsIClipboard.h"
#include "nsIClipboardHelper.h"
#include "nsIDocShellTreeItem.h"
#include "nsIURI.h"
#include "nsIScrollableFrame.h"
#include "prtime.h"
#include "prlong.h"
#include "nsIDragService.h"
#include "nsCopySupport.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLLinkElement.h"
#include "nsITimer.h"
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#include "nsAccessible.h"
#endif

// For style data reconstruction
#include "nsStyleChangeList.h"
#include "nsCSSFrameConstructor.h"
#ifdef MOZ_XUL
#include "nsMenuFrame.h"
#include "nsTreeBodyFrame.h"
#include "nsIBoxObject.h"
#include "nsITreeBoxObject.h"
#include "nsMenuPopupFrame.h"
#include "nsITreeColumns.h"
#include "nsIDOMXULMultSelectCntrlEl.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsIDOMXULMenuListElement.h"

#endif
#include "nsPlaceholderFrame.h"
#include "nsCanvasFrame.h"

// Content viewer interfaces
#include "nsIContentViewer.h"
#include "imgIEncoder.h"
#include "gfxPlatform.h"

#include "mozilla/FunctionTimer.h"

#include "Layers.h"

#ifdef NS_FUNCTION_TIMER
#define NS_TIME_FUNCTION_DECLARE_DOCURL                \
  nsCAutoString docURL__("N/A");                       \
  nsIURI *uri__ = mDocument->GetDocumentURI();         \
  if (uri__) uri__->GetSpec(docURL__);
#define NS_TIME_FUNCTION_WITH_DOCURL                   \
  NS_TIME_FUNCTION_DECLARE_DOCURL                      \
  NS_TIME_FUNCTION_MIN_FMT(1.0,                        \
     "%s (line %d) (document: %s)", MOZ_FUNCTION_NAME, \
     __LINE__, docURL__.get())
#else
#define NS_TIME_FUNCTION_WITH_DOCURL do{} while(0)
#endif

#include "nsContentCID.h"
static NS_DEFINE_IID(kRangeCID,     NS_RANGE_CID);

/* for NS_MEMORY_REPORTER_IMPLEMENT */
#include "nsIMemoryReporter.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layers;

PRBool nsIPresShell::gIsAccessibilityActive = PR_FALSE;
CapturingContentInfo nsIPresShell::gCaptureInfo;
nsIContent* nsIPresShell::gKeyDownTarget;

// convert a color value to a string, in the CSS format #RRGGBB
// *  - initially created for bugs 31816, 20760, 22963
static void ColorToString(nscolor aColor, nsAutoString &aString);

// Class ID's
static NS_DEFINE_CID(kFrameSelectionCID, NS_FRAMESELECTION_CID);

// RangePaintInfo is used to paint ranges to offscreen buffers
struct RangePaintInfo {
  nsCOMPtr<nsIRange> mRange;
  nsDisplayListBuilder mBuilder;
  nsDisplayList mList;

  // offset of builder's reference frame to the root frame
  nsPoint mRootOffset;

  RangePaintInfo(nsIRange* aRange, nsIFrame* aFrame)
    : mRange(aRange), mBuilder(aFrame, PR_FALSE, PR_FALSE)
  {
    MOZ_COUNT_CTOR(RangePaintInfo);
  }

  ~RangePaintInfo()
  {
    mList.DeleteAll();
    MOZ_COUNT_DTOR(RangePaintInfo);
  }
};

#undef NOISY

// ----------------------------------------------------------------------

#ifdef NS_DEBUG
// Set the environment variable GECKO_VERIFY_REFLOW_FLAGS to one or
// more of the following flags (comma separated) for handy debug
// output.
static PRUint32 gVerifyReflowFlags;

struct VerifyReflowFlags {
  const char*    name;
  PRUint32 bit;
};

static const VerifyReflowFlags gFlags[] = {
  { "verify",                VERIFY_REFLOW_ON },
  { "reflow",                VERIFY_REFLOW_NOISY },
  { "all",                   VERIFY_REFLOW_ALL },
  { "list-commands",         VERIFY_REFLOW_DUMP_COMMANDS },
  { "noisy-commands",        VERIFY_REFLOW_NOISY_RC },
  { "really-noisy-commands", VERIFY_REFLOW_REALLY_NOISY_RC },
  { "resize",                VERIFY_REFLOW_DURING_RESIZE_REFLOW },
};

#define NUM_VERIFY_REFLOW_FLAGS (sizeof(gFlags) / sizeof(gFlags[0]))

static void
ShowVerifyReflowFlags()
{
  printf("Here are the available GECKO_VERIFY_REFLOW_FLAGS:\n");
  const VerifyReflowFlags* flag = gFlags;
  const VerifyReflowFlags* limit = gFlags + NUM_VERIFY_REFLOW_FLAGS;
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

static const char kGrandTotalsStr[] = "Grand Totals";

// Counting Class
class ReflowCounter {
public:
  ReflowCounter(ReflowCountMgr * aMgr = nsnull);
  ~ReflowCounter();

  void ClearTotals();
  void DisplayTotals(const char * aStr);
  void DisplayDiffTotals(const char * aStr);
  void DisplayHTMLTotals(const char * aStr);

  void Add()                { mTotal++;         }
  void Add(PRUint32 aTotal) { mTotal += aTotal; }

  void CalcDiffInTotals();
  void SetTotalsCache();

  void SetMgr(ReflowCountMgr * aMgr) { mMgr = aMgr; }

  PRUint32 GetTotal() { return mTotal; }
  
protected:
  void DisplayTotals(PRUint32 aTotal, const char * aTitle);
  void DisplayHTMLTotals(PRUint32 aTotal, const char * aTitle);

  PRUint32 mTotal;
  PRUint32 mCacheTotal;

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

  void Add(const char * aName, nsIFrame * aFrame);
  ReflowCounter * LookUp(const char * aName);

  void PaintCount(const char * aName, nsIRenderingContext* aRenderingContext, nsPresContext* aPresContext, nsIFrame * aFrame, PRUint32 aColor);

  FILE * GetOutFile() { return mFD; }

  PLHashTable * GetIndiFrameHT() { return mIndiFrameCounts; }

  void SetPresContext(nsPresContext * aPresContext) { mPresContext = aPresContext; } // weak reference
  void SetPresShell(nsIPresShell* aPresShell) { mPresShell= aPresShell; } // weak reference

  void SetDumpFrameCounts(PRBool aVal)         { mDumpFrameCounts = aVal; }
  void SetDumpFrameByFrameCounts(PRBool aVal)  { mDumpFrameByFrameCounts = aVal; }
  void SetPaintFrameCounts(PRBool aVal)        { mPaintFrameByFrameCounts = aVal; }

  PRBool IsPaintingFrameCounts() { return mPaintFrameByFrameCounts; }

protected:
  void DisplayTotals(PRUint32 aTotal, PRUint32 * aDupArray, char * aTitle);
  void DisplayHTMLTotals(PRUint32 aTotal, PRUint32 * aDupArray, char * aTitle);

  static PRIntn RemoveItems(PLHashEntry *he, PRIntn i, void *arg);
  static PRIntn RemoveIndiItems(PLHashEntry *he, PRIntn i, void *arg);
  void CleanUp();

  // stdout Output Methods
  static PRIntn DoSingleTotal(PLHashEntry *he, PRIntn i, void *arg);
  static PRIntn DoSingleIndi(PLHashEntry *he, PRIntn i, void *arg);

  void DoGrandTotals();
  void DoIndiTotalsTree();

  // HTML Output Methods
  static PRIntn DoSingleHTMLTotal(PLHashEntry *he, PRIntn i, void *arg);
  void DoGrandHTMLTotals();

  // Zero Out the Totals
  static PRIntn DoClearTotals(PLHashEntry *he, PRIntn i, void *arg);

  // Displays the Diff Totals
  static PRIntn DoDisplayDiffTotals(PLHashEntry *he, PRIntn i, void *arg);

  PLHashTable * mCounts;
  PLHashTable * mIndiFrameCounts;
  FILE * mFD;
  
  PRBool mDumpFrameCounts;
  PRBool mDumpFrameByFrameCounts;
  PRBool mPaintFrameByFrameCounts;

  PRBool mCycledOnce;

  // Root Frame for Individual Tracking
  nsPresContext * mPresContext;
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

  nsresult Init() { return mBlocks ? NS_OK : NS_ERROR_OUT_OF_MEMORY; }

  // Memory management functions
  void* Allocate(size_t aSize);
  void Push();
  void Pop();

  PRUint32 Size() {
    PRUint32 result = 0;
    StackBlock *block = mBlocks;
    while (block) {
      result += sizeof(StackBlock);
      block = block->mNext;
    }
    return result;
  }

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

  // the current top of the mark list
  PRUint32 mStackTop;

  // the size of the mark array
  PRUint32 mMarkLength;
};



StackArena::StackArena()
{
  mMarkLength = 0;
  mMarks = nsnull;

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

void
StackArena::Push()
{
  // Resize the mark array if we overrun it.  Failure to allocate the
  // mark array is not fatal; we just won't free to that mark.  This
  // allows callers not to worry about error checking.
  if (mStackTop >= mMarkLength)
  {
    PRUint32 newLength = mStackTop + MARK_INCREMENT;
    StackMark* newMarks = new StackMark[newLength];
    if (newMarks) {
      if (mMarkLength)
        memcpy(newMarks, mMarks, sizeof(StackMark)*mMarkLength);
      // Fill in any marks that we couldn't allocate during a prior call
      // to Push().
      for (; mMarkLength < mStackTop; ++mMarkLength) {
        NS_NOTREACHED("should only hit this on out-of-memory");
        newMarks[mMarkLength].mBlock = mCurBlock;
        newMarks[mMarkLength].mPos = mPos;
      }
      delete [] mMarks;
      mMarks = newMarks;
      mMarkLength = newLength;
    }
  }

  // set a mark at the top (if we can)
  NS_ASSERTION(mStackTop < mMarkLength, "out of memory");
  if (mStackTop < mMarkLength) {
    mMarks[mStackTop].mBlock = mCurBlock;
    mMarks[mStackTop].mPos = mPos;
  }

  mStackTop++;
}

void*
StackArena::Allocate(size_t aSize)
{
  NS_ASSERTION(mStackTop > 0, "Allocate called without Push");

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
  void *result = mCurBlock->mBlock + mPos;
  mPos += aSize;

  return result;
}

void
StackArena::Pop()
{
  // pop off the mark
  NS_ASSERTION(mStackTop > 0, "unmatched pop");
  mStackTop--;

  if (mStackTop >= mMarkLength) {
    // We couldn't allocate the marks array at the time of the push, so
    // we don't know where we're freeing to.
    NS_NOTREACHED("out of memory");
    if (mStackTop == 0) {
      // But we do know if we've completely pushed the stack.
      mCurBlock = mBlocks;
      mPos = 0;
    }
    return;
  }

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
}

struct nsCallbackEventRequest
{
  nsIReflowCallback* callback;
  nsCallbackEventRequest* next;
};

// ----------------------------------------------------------------------------
#define ASSERT_REFLOW_SCHEDULED_STATE()                                       \
  NS_ASSERTION(mReflowScheduled ==                                            \
                 GetPresContext()->RefreshDriver()->                          \
                   IsLayoutFlushObserver(this), "Unexpected state")

class nsPresShellEventCB;
class nsAutoCauseReflowNotifier;

class PresShell : public nsIPresShell, public nsIViewObserver,
                  public nsStubDocumentObserver,
                  public nsISelectionController, public nsIObserver,
                  public nsSupportsWeakReference
{
public:
  PresShell();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIPresShell
  virtual NS_HIDDEN_(nsresult) Init(nsIDocument* aDocument,
                                   nsPresContext* aPresContext,
                                   nsIViewManager* aViewManager,
                                   nsStyleSet* aStyleSet,
                                   nsCompatibility aCompatMode);
  virtual NS_HIDDEN_(void) Destroy();

  virtual NS_HIDDEN_(void*) AllocateFrame(nsQueryFrame::FrameIID aCode,
                                          size_t aSize);
  virtual NS_HIDDEN_(void)  FreeFrame(nsQueryFrame::FrameIID aCode,
                                      void* aChunk);

  virtual NS_HIDDEN_(void*) AllocateMisc(size_t aSize);
  virtual NS_HIDDEN_(void)  FreeMisc(size_t aSize, void* aChunk);

  // Dynamic stack memory allocation
  virtual NS_HIDDEN_(void) PushStackMemory();
  virtual NS_HIDDEN_(void) PopStackMemory();
  virtual NS_HIDDEN_(void*) AllocateStackMemory(size_t aSize);

  virtual NS_HIDDEN_(nsresult) SetPreferenceStyleRules(PRBool aForceReflow);

  NS_IMETHOD GetSelection(SelectionType aType, nsISelection** aSelection);
  virtual nsISelection* GetCurrentSelection(SelectionType aType);

  NS_IMETHOD SetDisplaySelection(PRInt16 aToggle);
  NS_IMETHOD GetDisplaySelection(PRInt16 *aToggle);
  NS_IMETHOD ScrollSelectionIntoView(SelectionType aType, SelectionRegion aRegion, PRBool aIsSynchronous);
  NS_IMETHOD RepaintSelection(SelectionType aType);

  virtual NS_HIDDEN_(void) BeginObservingDocument();
  virtual NS_HIDDEN_(void) EndObservingDocument();
  virtual NS_HIDDEN_(nsresult) InitialReflow(nscoord aWidth, nscoord aHeight);
  virtual NS_HIDDEN_(nsresult) ResizeReflow(nscoord aWidth, nscoord aHeight);
  virtual NS_HIDDEN_(void) StyleChangeReflow();
  virtual NS_HIDDEN_(nsIPageSequenceFrame*) GetPageSequenceFrame() const;
  virtual NS_HIDDEN_(nsIFrame*) GetRealPrimaryFrameFor(nsIContent* aContent) const;

  virtual NS_HIDDEN_(nsIFrame*) GetPlaceholderFrameFor(nsIFrame* aFrame) const;
  virtual NS_HIDDEN_(void) FrameNeedsReflow(nsIFrame *aFrame, IntrinsicDirty aIntrinsicDirty,
                                            nsFrameState aBitToAdd);
  virtual NS_HIDDEN_(void) FrameNeedsToContinueReflow(nsIFrame *aFrame);
  virtual NS_HIDDEN_(void) CancelAllPendingReflows();
  virtual NS_HIDDEN_(PRBool) IsSafeToFlush() const;
  virtual NS_HIDDEN_(void) FlushPendingNotifications(mozFlushType aType);

  /**
   * Recreates the frames for a node
   */
  virtual NS_HIDDEN_(nsresult) RecreateFramesFor(nsIContent* aContent);

  /**
   * Post a callback that should be handled after reflow has finished.
   */
  virtual NS_HIDDEN_(nsresult) PostReflowCallback(nsIReflowCallback* aCallback);
  virtual NS_HIDDEN_(void) CancelReflowCallback(nsIReflowCallback* aCallback);

  virtual NS_HIDDEN_(void) ClearFrameRefs(nsIFrame* aFrame);
  virtual NS_HIDDEN_(already_AddRefed<nsIRenderingContext>) GetReferenceRenderingContext();
  virtual NS_HIDDEN_(nsresult) GoToAnchor(const nsAString& aAnchorName, PRBool aScroll);
  virtual NS_HIDDEN_(nsresult) ScrollToAnchor();

  virtual NS_HIDDEN_(nsresult) ScrollContentIntoView(nsIContent* aContent,
                                                     PRIntn      aVPercent,
                                                     PRIntn      aHPercent);
  virtual PRBool ScrollFrameRectIntoView(nsIFrame*     aFrame,
                                         const nsRect& aRect,
                                         PRIntn        aVPercent,
                                         PRIntn        aHPercent,
                                         PRUint32      aFlags);
  virtual nsRectVisibility GetRectVisibility(nsIFrame *aFrame,
                                             const nsRect &aRect,
                                             nscoord aMinTwips) const;

  virtual NS_HIDDEN_(void) SetIgnoreFrameDestruction(PRBool aIgnore);
  virtual NS_HIDDEN_(void) NotifyDestroyingFrame(nsIFrame* aFrame);

  virtual NS_HIDDEN_(nsresult) GetLinkLocation(nsIDOMNode* aNode, nsAString& aLocationString) const;

  virtual NS_HIDDEN_(nsresult) CaptureHistoryState(nsILayoutHistoryState** aLayoutHistoryState, PRBool aLeavingPage);

  virtual NS_HIDDEN_(void) UnsuppressPainting();

  virtual nsresult GetAgentStyleSheets(nsCOMArray<nsIStyleSheet>& aSheets);
  virtual nsresult SetAgentStyleSheets(const nsCOMArray<nsIStyleSheet>& aSheets);

  virtual nsresult AddOverrideStyleSheet(nsIStyleSheet *aSheet);
  virtual nsresult RemoveOverrideStyleSheet(nsIStyleSheet *aSheet);

  virtual NS_HIDDEN_(nsresult) HandleEventWithTarget(nsEvent* aEvent, nsIFrame* aFrame,
                                                     nsIContent* aContent,
                                                     nsEventStatus* aStatus);
  virtual NS_HIDDEN_(nsIFrame*) GetEventTargetFrame();
  virtual NS_HIDDEN_(already_AddRefed<nsIContent>) GetEventTargetContent(nsEvent* aEvent);


  virtual nsresult ReconstructFrames(void);
  virtual void Freeze();
  virtual void Thaw();
  virtual void FireOrClearDelayedEvents(PRBool aFireEvents);

  virtual nsIFrame* GetFrameForPoint(nsIFrame* aFrame, nsPoint aPt);

  virtual NS_HIDDEN_(nsresult) RenderDocument(const nsRect& aRect, PRUint32 aFlags,
                                              nscolor aBackgroundColor,
                                              gfxContext* aThebesContext);

  virtual already_AddRefed<gfxASurface> RenderNode(nsIDOMNode* aNode,
                                                   nsIntRegion* aRegion,
                                                   nsIntPoint& aPoint,
                                                   nsIntRect* aScreenRect);

  virtual already_AddRefed<gfxASurface> RenderSelection(nsISelection* aSelection,
                                                        nsIntPoint& aPoint,
                                                        nsIntRect* aScreenRect);

  virtual already_AddRefed<nsPIDOMWindow> GetRootWindow();

  virtual LayerManager* GetLayerManager();

  //nsIViewObserver interface

  NS_IMETHOD Paint(nsIView* aDisplayRoot,
                   nsIView* aViewToPaint,
                   nsIWidget* aWidget,
                   const nsRegion& aDirtyRegion,
                   const nsIntRegion& aIntDirtyRegion,
                   PRBool aPaintDefaultBackground,
                   PRBool aWillSendDidPaint);
  NS_IMETHOD HandleEvent(nsIView*        aView,
                         nsGUIEvent*     aEvent,
                         nsEventStatus*  aEventStatus);
  virtual NS_HIDDEN_(nsresult) HandleDOMEventWithTarget(nsIContent* aTargetContent,
                                                        nsEvent* aEvent,
                                                        nsEventStatus* aStatus);
  virtual NS_HIDDEN_(nsresult) HandleDOMEventWithTarget(nsIContent* aTargetContent,
                                                        nsIDOMEvent* aEvent,
                                                        nsEventStatus* aStatus);
  NS_IMETHOD ResizeReflow(nsIView *aView, nscoord aWidth, nscoord aHeight);
  NS_IMETHOD_(PRBool) ShouldIgnoreInvalidation();
  NS_IMETHOD_(void) WillPaint(PRBool aWillSendDidPaint);
  NS_IMETHOD_(void) DidPaint();
  NS_IMETHOD_(void) DispatchSynthMouseMove(nsGUIEvent *aEvent,
                                           PRBool aFlushOnHoverChange);
  NS_IMETHOD_(void) ClearMouseCapture(nsIView* aView);

  // caret handling
  virtual NS_HIDDEN_(already_AddRefed<nsCaret>) GetCaret() const;
  virtual NS_HIDDEN_(void) MaybeInvalidateCaretPosition();
  NS_IMETHOD SetCaretEnabled(PRBool aInEnable);
  NS_IMETHOD SetCaretReadOnly(PRBool aReadOnly);
  NS_IMETHOD GetCaretEnabled(PRBool *aOutEnabled);
  NS_IMETHOD SetCaretVisibilityDuringSelection(PRBool aVisibility);
  NS_IMETHOD GetCaretVisible(PRBool *_retval);
  virtual void SetCaret(nsCaret *aNewCaret);
  virtual void RestoreCaret();

  NS_IMETHOD SetSelectionFlags(PRInt16 aInEnable);
  NS_IMETHOD GetSelectionFlags(PRInt16 *aOutEnable);

  // nsISelectionController

  NS_IMETHOD CharacterMove(PRBool aForward, PRBool aExtend);
  NS_IMETHOD CharacterExtendForDelete();
  NS_IMETHOD CharacterExtendForBackspace();
  NS_IMETHOD WordMove(PRBool aForward, PRBool aExtend);
  NS_IMETHOD WordExtendForDelete(PRBool aForward);
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
  virtual void BeginUpdate(nsIDocument* aDocument, nsUpdateType aUpdateType);
  virtual void EndUpdate(nsIDocument* aDocument, nsUpdateType aUpdateType);
  virtual void BeginLoad(nsIDocument* aDocument);
  virtual void EndLoad(nsIDocument* aDocument);
  virtual void ContentStatesChanged(nsIDocument* aDocument,
                                    nsIContent* aContent1,
                                    nsIContent* aContent2,
                                    PRInt32 aStateMask);
  virtual void DocumentStatesChanged(nsIDocument* aDocument,
                                     PRInt32 aStateMask);
  virtual void StyleSheetAdded(nsIDocument* aDocument,
                               nsIStyleSheet* aStyleSheet,
                               PRBool aDocumentSheet);
  virtual void StyleSheetRemoved(nsIDocument* aDocument,
                                 nsIStyleSheet* aStyleSheet,
                                 PRBool aDocumentSheet);
  virtual void StyleSheetApplicableStateChanged(nsIDocument* aDocument,
                                                nsIStyleSheet* aStyleSheet,
                                                PRBool aApplicable);
  virtual void StyleRuleChanged(nsIDocument* aDocument,
                                nsIStyleSheet* aStyleSheet,
                                nsIStyleRule* aOldStyleRule,
                                nsIStyleRule* aNewStyleRule);
  virtual void StyleRuleAdded(nsIDocument* aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule);
  virtual void StyleRuleRemoved(nsIDocument* aDocument,
                                nsIStyleSheet* aStyleSheet,
                                nsIStyleRule* aStyleRule);

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTEWILLCHANGE
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  NS_DECL_NSIOBSERVER

#ifdef MOZ_REFLOW_PERF
  virtual NS_HIDDEN_(void) DumpReflows();
  virtual NS_HIDDEN_(void) CountReflows(const char * aName, nsIFrame * aFrame);
  virtual NS_HIDDEN_(void) PaintCount(const char * aName,
                                      nsIRenderingContext* aRenderingContext,
                                      nsPresContext* aPresContext,
                                      nsIFrame * aFrame,
                                      PRUint32 aColor);
  virtual NS_HIDDEN_(void) SetPaintFrameCount(PRBool aOn);
  virtual PRBool IsPaintingFrameCounts();
#endif

#ifdef DEBUG
  virtual void ListStyleContexts(nsIFrame *aRootFrame, FILE *out,
                                 PRInt32 aIndent = 0);

  virtual void ListStyleSheets(FILE *out, PRInt32 aIndent = 0);
  virtual void VerifyStyleTree();
#endif

#ifdef PR_LOGGING
  static PRLogModuleInfo* gLog;
#endif

  virtual NS_HIDDEN_(void) DisableNonTestMouseEvents(PRBool aDisable);

  virtual void UpdateCanvasBackground();

  virtual nsresult AddCanvasBackgroundColorItem(nsDisplayListBuilder& aBuilder,
                                                nsDisplayList& aList,
                                                nsIFrame* aFrame,
                                                const nsRect& aBounds,
                                                nscolor aBackstopColor,
                                                PRBool aForceDraw);

  virtual nsresult AddPrintPreviewBackgroundItem(nsDisplayListBuilder& aBuilder,
                                                 nsDisplayList& aList,
                                                 nsIFrame* aFrame,
                                                 const nsRect& aBounds);

  virtual nscolor ComputeBackstopColor(nsIView* aDisplayRoot);

  virtual NS_HIDDEN_(nsresult) SetIsActive(PRBool aIsActive);

protected:
  virtual ~PresShell();

  void HandlePostedReflowCallbacks(PRBool aInterruptible);
  void CancelPostedReflowCallbacks();

  void UnsuppressAndInvalidate();

  void WillCauseReflow() {
    nsContentUtils::AddScriptBlocker();
    ++mChangeNestCount;
  }
  nsresult DidCauseReflow();
  friend class nsAutoCauseReflowNotifier;

  void     WillDoReflow();
  void     DidDoReflow(PRBool aInterruptible);
  // ProcessReflowCommands returns whether we processed all our dirty roots
  // without interruptions.
  PRBool   ProcessReflowCommands(PRBool aInterruptible);
  // MaybeScheduleReflow checks if posting a reflow is needed, then checks if
  // the last reflow was interrupted. In the interrupted case ScheduleReflow is
  // called off a timer, otherwise it is called directly.
  void     MaybeScheduleReflow();
  // Actually schedules a reflow.  This should only be called by
  // MaybeScheduleReflow and the reflow timer ScheduleReflowOffTimer
  // sets up.
  void     ScheduleReflow();

  // DoReflow returns whether the reflow finished without interruption
  PRBool DoReflow(nsIFrame* aFrame, PRBool aInterruptible);
#ifdef DEBUG
  void DoVerifyReflow();
  void VerifyHasDirtyRootAncestor(nsIFrame* aFrame);
#endif

  // Helper for ScrollContentIntoView
  void DoScrollContentIntoView(nsIContent* aContent,
                               PRIntn      aVPercent,
                               PRIntn      aHPercent);

  friend class nsPresShellEventCB;

  PRBool mCaretEnabled;
#ifdef NS_DEBUG
  nsresult CloneStyleSet(nsStyleSet* aSet, nsStyleSet** aResult);
  PRBool VerifyIncrementalReflow();
  PRBool mInVerifyReflow;
  void ShowEventTargetDebug();
#endif

    /**
    * methods that manage rules that are used to implement the associated preferences
    *  - initially created for bugs 31816, 20760, 22963
    */
  nsresult ClearPreferenceStyleRules(void);
  nsresult CreatePreferenceStyleSheet(void);
  nsresult SetPrefLinkRules(void);
  nsresult SetPrefFocusRules(void);
  nsresult SetPrefNoScriptRule();
  nsresult SetPrefNoFramesRule(void);

  // methods for painting a range to an offscreen buffer

  // given a display list, clip the items within the list to
  // the range
  nsRect ClipListToRange(nsDisplayListBuilder *aBuilder,
                         nsDisplayList* aList,
                         nsIRange* aRange);

  // create a RangePaintInfo for the range aRange containing the
  // display list needed to paint the range to a surface
  RangePaintInfo* CreateRangePaintInfo(nsIDOMRange* aRange,
                                       nsRect& aSurfaceRect,
                                       PRBool aForPrimarySelection);

  /*
   * Paint the items to a new surface and return it.
   *
   * aSelection - selection being painted, if any
   * aRegion - clip region, if any
   * aArea - area that the surface occupies, relative to the root frame
   * aPoint - reference point, typically the mouse position
   * aScreenRect - [out] set to the area of the screen the painted area should
   *               be displayed at
   */
  already_AddRefed<gfxASurface>
  PaintRangePaintInfo(nsTArray<nsAutoPtr<RangePaintInfo> >* aItems,
                      nsISelection* aSelection,
                      nsIntRegion* aRegion,
                      nsRect aArea,
                      nsIntPoint& aPoint,
                      nsIntRect* aScreenRect);

  /**
   * Methods to handle changes to user and UA sheet lists that we get
   * notified about.
   */
  void AddUserSheet(nsISupports* aSheet);
  void AddAgentSheet(nsISupports* aSheet);
  void RemoveSheet(nsStyleSet::sheetType aType, nsISupports* aSheet);

  // Hide a view if it is a popup
  void HideViewIfPopup(nsIView* aView);

  // Utility method to restore the root scrollframe state
  void RestoreRootScrollPosition();

  void MaybeReleaseCapturingContent()
  {
    nsCOMPtr<nsFrameSelection> frameSelection = FrameSelection();
    if (frameSelection) {
      frameSelection->SetMouseDownState(PR_FALSE);
    }
    if (gCaptureInfo.mContent &&
        gCaptureInfo.mContent->GetOwnerDoc() == mDocument) {
      SetCapturingContent(nsnull, 0);
    }
  }

  nsRefPtr<nsCSSStyleSheet> mPrefStyleSheet; // mStyleSet owns it but we
                                             // maintain a ref, may be null
#ifdef DEBUG
  PRUint32                  mUpdateCount;
#endif
  // reflow roots that need to be reflowed, as both a queue and a hashtable
  nsTArray<nsIFrame*> mDirtyRoots;

  PRPackedBool mDocumentLoading;

  PRPackedBool mIgnoreFrameDestruction;
  PRPackedBool mHaveShutDown;

  // This is used to protect ourselves from triggering reflow while in the
  // middle of frame construction and the like... it really shouldn't be
  // needed, one hopes, but it is for now.
  PRUint32  mChangeNestCount;
  
  nsIFrame*   mCurrentEventFrame;
  nsCOMPtr<nsIContent> mCurrentEventContent;
  nsTArray<nsIFrame*> mCurrentEventFrameStack;
  nsCOMArray<nsIContent> mCurrentEventContentStack;

  nsCOMPtr<nsIContent>          mLastAnchorScrolledTo;
  nscoord                       mLastAnchorScrollPositionY;
  nsRefPtr<nsCaret>             mCaret;
  nsRefPtr<nsCaret>             mOriginalCaret;
  nsPresArena                   mFrameArena;
  StackArena                    mStackArena;
  nsCOMPtr<nsIDragService>      mDragService;
  
#ifdef DEBUG
  // The reflow root under which we're currently reflowing.  Null when
  // not in reflow.
  nsIFrame* mCurrentReflowRoot;
#endif

  // Set of frames that we should mark with NS_FRAME_HAS_DIRTY_CHILDREN after
  // we finish reflowing mCurrentReflowRoot.
  nsTHashtable< nsPtrHashKey<nsIFrame> > mFramesToDirty;

  // Information needed to properly handle scrolling content into view if the
  // pre-scroll reflow flush can be interrupted.  mContentToScrollTo is
  // non-null between the initial scroll attempt and the first time we finish
  // processing all our dirty roots.  mContentScrollVPosition and
  // mContentScrollHPosition are only used when it's non-null.
  nsCOMPtr<nsIContent> mContentToScrollTo;
  PRIntn mContentScrollVPosition;
  PRIntn mContentScrollHPosition;

  class nsDelayedEvent
  {
  public:
    virtual ~nsDelayedEvent() {};
    virtual void Dispatch(PresShell* aShell) {}
  };

  class nsDelayedInputEvent : public nsDelayedEvent
  {
  public:
    virtual void Dispatch(PresShell* aShell)
    {
      if (mEvent && mEvent->widget) {
        nsCOMPtr<nsIWidget> w = mEvent->widget;
        nsEventStatus status;
        w->DispatchEvent(mEvent, status);
      }
    }

  protected:
    void Init(nsInputEvent* aEvent)
    {
      mEvent->time = aEvent->time;
      mEvent->refPoint = aEvent->refPoint;
      mEvent->isShift = aEvent->isShift;
      mEvent->isControl = aEvent->isControl;
      mEvent->isAlt = aEvent->isAlt;
      mEvent->isMeta = aEvent->isMeta;
    }

    nsDelayedInputEvent()
    : nsDelayedEvent(), mEvent(nsnull) {}

    nsInputEvent* mEvent;
  };

  class nsDelayedMouseEvent : public nsDelayedInputEvent
  {
  public:
    nsDelayedMouseEvent(nsMouseEvent* aEvent) : nsDelayedInputEvent()
    {
      mEvent = new nsMouseEvent(NS_IS_TRUSTED_EVENT(aEvent),
                                aEvent->message,
                                aEvent->widget,
                                aEvent->reason,
                                aEvent->context);
      if (mEvent) {
        Init(aEvent);
        static_cast<nsMouseEvent*>(mEvent)->clickCount = aEvent->clickCount;
      }
    }

    virtual ~nsDelayedMouseEvent()
    {
      delete static_cast<nsMouseEvent*>(mEvent);
    }
  };

  class nsDelayedKeyEvent : public nsDelayedInputEvent
  {
  public:
    nsDelayedKeyEvent(nsKeyEvent* aEvent) : nsDelayedInputEvent()
    {
      mEvent = new nsKeyEvent(NS_IS_TRUSTED_EVENT(aEvent),
                              aEvent->message,
                              aEvent->widget);
      if (mEvent) {
        Init(aEvent);
        static_cast<nsKeyEvent*>(mEvent)->keyCode = aEvent->keyCode;
        static_cast<nsKeyEvent*>(mEvent)->charCode = aEvent->charCode;
        static_cast<nsKeyEvent*>(mEvent)->alternativeCharCodes =
          aEvent->alternativeCharCodes;
        static_cast<nsKeyEvent*>(mEvent)->isChar = aEvent->isChar;
      }
    }

    virtual ~nsDelayedKeyEvent()
    {
      delete static_cast<nsKeyEvent*>(mEvent);
    }
  };

  PRPackedBool                         mNoDelayedMouseEvents;
  PRPackedBool                         mNoDelayedKeyEvents;
  nsTArray<nsAutoPtr<nsDelayedEvent> > mDelayedEvents;

  nsCallbackEventRequest* mFirstCallbackEventRequest;
  nsCallbackEventRequest* mLastCallbackEventRequest;

  PRPackedBool      mIsDocumentGone;      // We've been disconnected from the document.
                                          // We will refuse to paint the document until either
                                          // (a) our timer fires or (b) all frames are constructed.
  PRPackedBool      mShouldUnsuppressPainting;  // Indicates that it is safe to unlock painting once all pending
                                                // reflows have been processed.
  nsCOMPtr<nsITimer> mPaintSuppressionTimer; // This timer controls painting suppression.  Until it fires
                                             // or all frames are constructed, we won't paint anything but
                                             // our <body> background and scrollbars.
#define PAINTLOCK_EVENT_DELAY 250 // 250ms.  This is actually
                                  // pref-controlled, but we use this
                                  // value if we fail to get the pref
                                  // for any reason.

  static void sPaintSuppressionCallback(nsITimer* aTimer, void* aPresShell); // A callback for the timer.

  // At least on Win32 and Mac after interupting a reflow we need to post
  // the resume reflow event off a timer to avoid event starvation because
  // posted messages are processed before other messages when the modal
  // moving/sizing loop is running, see bug 491700 for details.
  nsCOMPtr<nsITimer> mReflowContinueTimer;
  static void sReflowContinueCallback(nsITimer* aTimer, void* aPresShell);
  PRBool ScheduleReflowOffTimer();
  
#ifdef MOZ_REFLOW_PERF
  ReflowCountMgr * mReflowCountMgr;
#endif

  static PRBool sDisableNonTestMouseEvents;

  // false if a check should be done for key/ime events that should be
  // retargeted to the currently focused presshell
  static PRBool sDontRetargetEvents;

private:

  PRBool InZombieDocument(nsIContent *aContent);
  already_AddRefed<nsIPresShell> GetParentPresShell();
  nsresult RetargetEventToParent(nsGUIEvent* aEvent,
                                 nsEventStatus*  aEventStatus);

  //helper funcs for event handling
protected:
  //protected because nsPresShellEventCB needs this.
  nsIFrame* GetCurrentEventFrame();
private:
  void PushCurrentEventInfo(nsIFrame* aFrame, nsIContent* aContent);
  void PopCurrentEventInfo();
  nsresult HandleEventInternal(nsEvent* aEvent, nsIView* aView,
                               nsEventStatus *aStatus);
  nsresult HandlePositionedEvent(nsIView*       aView,
                                 nsIFrame*      aTargetFrame,
                                 nsGUIEvent*    aEvent,
                                 nsEventStatus* aEventStatus);
  // This returns the focused DOM window under our top level window.
  //  I.e., when we are deactive, this returns the *last* focused DOM window.
  already_AddRefed<nsPIDOMWindow> GetFocusedDOMWindowInOurWindow();

  /*
   * This and the next two helper methods are used to target and position the
   * context menu when the keyboard shortcut is used to open it.
   *
   * If another menu is open, the context menu is opened relative to the
   * active menuitem within the menu, or the menu itself if no item is active.
   * Otherwise, if the caret is visible, the menu is opened near the caret.
   * Otherwise, if a selectable list such as a listbox is focused, the
   * current item within the menu is opened relative to this item.
   * Otherwise, the context menu is opened at the topleft corner of the
   * view.
   *
   * Returns true if the context menu event should fire and false if it should
   * not.
   */
  PRBool AdjustContextMenuKeyEvent(nsMouseEvent* aEvent);

  // 
  PRBool PrepareToUseCaretPosition(nsIWidget* aEventWidget, nsIntPoint& aTargetPt);

  // Get the selected item and coordinates in device pixels relative to root
  // document's root view for element, first ensuring the element is onscreen
  void GetCurrentItemAndPositionForElement(nsIDOMElement *aCurrentEl,
                                           nsIContent **aTargetToUse,
                                           nsIntPoint& aTargetPt,
                                           nsIWidget *aRootWidget);

  void FireResizeEvent();
  static void AsyncResizeEventCallback(nsITimer* aTimer, void* aPresShell);
  nsRevocableEventPtr<nsRunnableMethod<PresShell> > mResizeEvent;
  nsCOMPtr<nsITimer> mAsyncResizeEventTimer;
  PRPackedBool mAsyncResizeTimerIsActive;
  PRPackedBool mInResize;

private:
#ifdef DEBUG
  // Ensure that every allocation from the PresArena is eventually freed.
  PRUint32 mPresArenaAllocCount;
#endif

public:

  PRUint32 EstimateMemoryUsed() {
    PRUint32 result = 0;

    result += sizeof(PresShell);
    result += mStackArena.Size();
    result += mFrameArena.Size();

    return result;
  }

  static PLDHashOperator LiveShellSizeEnumerator(PresShellPtrKey *aEntry,
                                                 void *userArg)
  {
    PresShell *aShell = static_cast<PresShell*>(aEntry->GetKey());
    PRUint32 *val = (PRUint32*)userArg;
    *val += aShell->EstimateMemoryUsed();
    *val += aShell->mPresContext->EstimateMemoryUsed();
    return PL_DHASH_NEXT;
  }

  static PLDHashOperator LiveShellBidiSizeEnumerator(PresShellPtrKey *aEntry,
                                                     void *userArg)
  {
    PresShell *aShell = static_cast<PresShell*>(aEntry->GetKey());
    PRUint32 *val = (PRUint32*)userArg;
    *val += aShell->mPresContext->GetBidiMemoryUsed();
    return PL_DHASH_NEXT;
  }

  static PRUint32
  EstimateShellsMemory(nsTHashtable<PresShellPtrKey>::Enumerator aEnumerator)
  {
    PRUint32 result = 0;
    sLiveShells->EnumerateEntries(aEnumerator, &result);
    return result;
  }
                  
                                  
  static PRInt64 SizeOfLayoutMemoryReporter(void *) {
    return EstimateShellsMemory(LiveShellSizeEnumerator);
  }

  static PRInt64 SizeOfBidiMemoryReporter(void *) {
    return EstimateShellsMemory(LiveShellBidiSizeEnumerator);
  }

protected:
  void QueryIsActive();
  nsresult UpdateImageLockingState();
};

class nsAutoCauseReflowNotifier
{
public:
  nsAutoCauseReflowNotifier(PresShell* aShell)
    : mShell(aShell)
  {
    mShell->WillCauseReflow();
  }
  ~nsAutoCauseReflowNotifier()
  {
    // This check should not be needed. Currently the only place that seem
    // to need it is the code that deals with bug 337586.
    if (!mShell->mHaveShutDown) {
      mShell->DidCauseReflow();
    }
    else {
      nsContentUtils::RemoveScriptBlocker();
    }
  }

  PresShell* mShell;
};

class NS_STACK_CLASS nsPresShellEventCB : public nsDispatchingCallback
{
public:
  nsPresShellEventCB(PresShell* aPresShell) : mPresShell(aPresShell) {}

  virtual void HandleEvent(nsEventChainPostVisitor& aVisitor)
  {
    if (aVisitor.mPresContext && aVisitor.mEvent->eventStructType != NS_EVENT) {
      nsIFrame* frame = mPresShell->GetCurrentEventFrame();
      if (frame) {
        frame->HandleEvent(aVisitor.mPresContext,
                           (nsGUIEvent*) aVisitor.mEvent,
                           &aVisitor.mEventStatus);
      }
    }
  }

  nsRefPtr<PresShell> mPresShell;
};

PRBool PresShell::sDisableNonTestMouseEvents = PR_FALSE;
PRBool PresShell::sDontRetargetEvents = PR_FALSE;

#ifdef PR_LOGGING
PRLogModuleInfo* PresShell::gLog;
#endif

#ifdef NS_DEBUG
static void
VerifyStyleTree(nsPresContext* aPresContext, nsFrameManager* aFrameManager)
{
  if (nsFrame::GetVerifyStyleTreeEnable()) {
    nsIFrame* rootFrame = aFrameManager->GetRootFrame();
    aFrameManager->DebugVerifyStyleTree(rootFrame);
  }
}
#define VERIFY_STYLE_TREE ::VerifyStyleTree(mPresContext, FrameManager())
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
        const VerifyReflowFlags* flag = gFlags;
        const VerifyReflowFlags* limit = gFlags + NUM_VERIFY_REFLOW_FLAGS;
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

      printf("Note: verifyreflow is enabled");
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
  }
#endif
  return gVerifyReflowEnabled;
}

void
nsIPresShell::SetVerifyReflowEnable(PRBool aEnabled)
{
  gVerifyReflowEnabled = aEnabled;
}

/* virtual */ void
nsIPresShell::AddWeakFrameExternal(nsWeakFrame* aWeakFrame)
{
  AddWeakFrameInternal(aWeakFrame);
}

void
nsIPresShell::AddWeakFrameInternal(nsWeakFrame* aWeakFrame)
{
  if (aWeakFrame->GetFrame()) {
    aWeakFrame->GetFrame()->AddStateBits(NS_FRAME_EXTERNAL_REFERENCE);
  }
  aWeakFrame->SetPreviousWeakFrame(mWeakFrames);
  mWeakFrames = aWeakFrame;
}

/* virtual */ void
nsIPresShell::RemoveWeakFrameExternal(nsWeakFrame* aWeakFrame)
{
  RemoveWeakFrameInternal(aWeakFrame);
}

void
nsIPresShell::RemoveWeakFrameInternal(nsWeakFrame* aWeakFrame)
{
  if (mWeakFrames == aWeakFrame) {
    mWeakFrames = aWeakFrame->GetPreviousWeakFrame();
    return;
  }
  nsWeakFrame* nextWeak = mWeakFrames;
  while (nextWeak && nextWeak->GetPreviousWeakFrame() != aWeakFrame) {
    nextWeak = nextWeak->GetPreviousWeakFrame();
  }
  if (nextWeak) {
    nextWeak->SetPreviousWeakFrame(aWeakFrame->GetPreviousWeakFrame());
  }
}

already_AddRefed<nsFrameSelection>
nsIPresShell::FrameSelection()
{
  NS_IF_ADDREF(mSelection);
  return mSelection;
}

//----------------------------------------------------------------------

nsresult
NS_NewPresShell(nsIPresShell** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");

  if (!aInstancePtrResult)
    return NS_ERROR_NULL_POINTER;

  *aInstancePtrResult = new PresShell();
  if (!*aInstancePtrResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aInstancePtrResult);
  return NS_OK;
}

nsTHashtable<PresShell::PresShellPtrKey> *nsIPresShell::sLiveShells = 0;

NS_MEMORY_REPORTER_IMPLEMENT(LayoutPresShell,
                             "layout/all",
                             "Memory in use by layout PresShell, PresContext, and other related areas.",
                             PresShell::SizeOfLayoutMemoryReporter,
                             nsnull)

NS_MEMORY_REPORTER_IMPLEMENT(LayoutBidi,
                             "layout/bidi",
                             "Memory in use by layout Bidi processor.",
                             PresShell::SizeOfBidiMemoryReporter,
                             nsnull)

PresShell::PresShell()
{
  mSelection = nsnull;
#ifdef MOZ_REFLOW_PERF
  mReflowCountMgr = new ReflowCountMgr();
  mReflowCountMgr->SetPresContext(mPresContext);
  mReflowCountMgr->SetPresShell(this);
#endif
#ifdef PR_LOGGING
  if (! gLog)
    gLog = PR_NewLogModule("PresShell");
#endif
  mSelectionFlags = nsISelectionDisplay::DISPLAY_TEXT | nsISelectionDisplay::DISPLAY_IMAGES;
  mIsThemeSupportDisabled = PR_FALSE;
  mIsActive = PR_TRUE;
  mFrozen = PR_FALSE;
#ifdef DEBUG
  mPresArenaAllocCount = 0;
#endif

  static bool registeredReporter = false;
  if (!registeredReporter) {
    NS_RegisterMemoryReporter(new NS_MEMORY_REPORTER_NAME(LayoutPresShell));
    NS_RegisterMemoryReporter(new NS_MEMORY_REPORTER_NAME(LayoutBidi));
    registeredReporter = true;
  }

  new (this) nsFrameManager();

  sLiveShells->PutEntry(this);
}

NS_IMPL_ISUPPORTS8(PresShell, nsIPresShell, nsIDocumentObserver,
                   nsIViewObserver, nsISelectionController,
                   nsISelectionDisplay, nsIObserver, nsISupportsWeakReference,
                   nsIMutationObserver)

PresShell::~PresShell()
{
  sLiveShells->RemoveEntry(this);

  if (!mHaveShutDown) {
    NS_NOTREACHED("Someone did not call nsIPresShell::destroy");
    Destroy();
  }

  NS_ASSERTION(mCurrentEventContentStack.Count() == 0,
               "Huh, event content left on the stack in pres shell dtor!");
  NS_ASSERTION(mFirstCallbackEventRequest == nsnull &&
               mLastCallbackEventRequest == nsnull,
               "post-reflow queues not empty.  This means we're leaking");

#ifdef DEBUG
  NS_ASSERTION(mPresArenaAllocCount == 0,
               "Some pres arena objects were not freed");
#endif

  delete mStyleSet;
  delete mFrameConstructor;

  mCurrentEventContent = nsnull;

  NS_IF_RELEASE(mPresContext);
  NS_IF_RELEASE(mDocument);
  NS_IF_RELEASE(mSelection);
}

/**
 * Initialize the presentation shell. Create view manager and style
 * manager.
 */
nsresult
PresShell::Init(nsIDocument* aDocument,
                nsPresContext* aPresContext,
                nsIViewManager* aViewManager,
                nsStyleSet* aStyleSet,
                nsCompatibility aCompatMode)
{
  NS_TIME_FUNCTION_MIN(1.0);

  NS_PRECONDITION(nsnull != aDocument, "null ptr");
  NS_PRECONDITION(nsnull != aPresContext, "null ptr");
  NS_PRECONDITION(nsnull != aViewManager, "null ptr");
  nsresult result;

  if ((nsnull == aDocument) || (nsnull == aPresContext) ||
      (nsnull == aViewManager)) {
    return NS_ERROR_NULL_POINTER;
  }
  if (mDocument) {
    NS_WARNING("PresShell double init'ed");
    return NS_ERROR_ALREADY_INITIALIZED;
  }
  result = mStackArena.Init();
  NS_ENSURE_SUCCESS(result, result);

  if (!mFramesToDirty.Init()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mDocument = aDocument;
  NS_ADDREF(mDocument);
  mViewManager = aViewManager;

  // Create our frame constructor.
  mFrameConstructor = new nsCSSFrameConstructor(mDocument, this);
  NS_ENSURE_TRUE(mFrameConstructor, NS_ERROR_OUT_OF_MEMORY);

  // The document viewer owns both view manager and pres shell.
  mViewManager->SetViewObserver(this);

  // Bind the context to the presentation shell.
  mPresContext = aPresContext;
  NS_ADDREF(mPresContext);
  aPresContext->SetShell(this);

  // Now we can initialize the style set.
  result = aStyleSet->Init(aPresContext);
  NS_ENSURE_SUCCESS(result, result);

  // From this point on, any time we return an error we need to make
  // sure to null out mStyleSet first, since an error return from this
  // method will cause the caller to delete the style set, so we don't
  // want to delete it in our destructor.
  mStyleSet = aStyleSet;

  // Notify our prescontext that it now has a compatibility mode.  Note that
  // this MUST happen after we set up our style set but before we create any
  // frames.
  mPresContext->CompatibilityModeChanged();

  // setup the preference style rules (no forced reflow), and do it
  // before creating any frames.
  SetPreferenceStyleRules(PR_FALSE);

  result = CallCreateInstance(kFrameSelectionCID, &mSelection);
  if (NS_FAILED(result)) {
    mStyleSet = nsnull;
    return result;
  }

  // Create and initialize the frame manager
  result = FrameManager()->Init(this, mStyleSet);
  if (NS_FAILED(result)) {
    NS_WARNING("Frame manager initialization failed");
    mStyleSet = nsnull;
    return result;
  }

  mSelection->Init(this, nsnull);

  // Important: this has to happen after the selection has been set up
#ifdef SHOW_CARET
  // make the caret
  nsresult  err = NS_NewCaret(getter_AddRefs(mCaret));
  if (NS_SUCCEEDED(err))
  {
    mCaret->Init(this);
    mOriginalCaret = mCaret;
  }

  //SetCaretEnabled(PR_TRUE);       // make it show in browser windows
#endif  
  //set up selection to be displayed in document
  // Don't enable selection for print media
  nsPresContext::nsPresContextType type = aPresContext->Type();
  if (type != nsPresContext::eContext_PrintPreview &&
      type != nsPresContext::eContext_Print)
    SetDisplaySelection(nsISelectionController::SELECTION_DISABLED);
  
  if (gMaxRCProcessingTime == -1) {
    gMaxRCProcessingTime =
      nsContentUtils::GetIntPref("layout.reflow.timeslice",
                                 NS_MAX_REFLOW_TIME);
  }

  {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->AddObserver(this, "agent-sheet-added", PR_FALSE);
      os->AddObserver(this, "user-sheet-added", PR_FALSE);
      os->AddObserver(this, "agent-sheet-removed", PR_FALSE);
      os->AddObserver(this, "user-sheet-removed", PR_FALSE);
#ifdef MOZ_XUL
      os->AddObserver(this, "chrome-flush-skin-caches", PR_FALSE);
#endif
#ifdef ACCESSIBILITY
      os->AddObserver(this, "a11y-init-or-shutdown", PR_FALSE);
#endif
    }
  }

  // cache the drag service so we can check it during reflows
  mDragService = do_GetService("@mozilla.org/widget/dragservice;1");

#ifdef MOZ_REFLOW_PERF
    if (mReflowCountMgr) {
      PRBool paintFrameCounts =
        nsContentUtils::GetBoolPref("layout.reflow.showframecounts");

      PRBool dumpFrameCounts =
        nsContentUtils::GetBoolPref("layout.reflow.dumpframecounts");

      PRBool dumpFrameByFrameCounts =
        nsContentUtils::GetBoolPref("layout.reflow.dumpframebyframecounts");

      mReflowCountMgr->SetDumpFrameCounts(dumpFrameCounts);
      mReflowCountMgr->SetDumpFrameByFrameCounts(dumpFrameByFrameCounts);
      mReflowCountMgr->SetPaintFrameCounts(paintFrameCounts);
    }
#endif

#ifdef MOZ_SMIL
  if (mDocument->HasAnimationController()) {
    nsSMILAnimationController* animCtrl = mDocument->GetAnimationController();
    if (!animCtrl->IsPaused()) {
      animCtrl->StartSampling(GetPresContext()->RefreshDriver());
    }
  }
#endif // MOZ_SMIL

  // Get our activeness from the docShell.
  QueryIsActive();

  return NS_OK;
}

void
PresShell::Destroy()
{
  NS_TIME_FUNCTION_MIN(1.0);

  NS_ASSERTION(!nsContentUtils::IsSafeToRunScript(),
    "destroy called on presshell while scripts not blocked");

#ifdef MOZ_REFLOW_PERF
  DumpReflows();
  if (mReflowCountMgr) {
    delete mReflowCountMgr;
    mReflowCountMgr = nsnull;
  }
#endif

  if (mHaveShutDown)
    return;

#ifdef ACCESSIBILITY
  if (gIsAccessibilityActive) {
    nsCOMPtr<nsIAccessibilityService> accService =
      do_GetService("@mozilla.org/accessibilityService;1");
    if (accService) {
      accService->PresShellDestroyed(this);
    }
  }
#endif // ACCESSIBILITY

  MaybeReleaseCapturingContent();

  if (gKeyDownTarget && gKeyDownTarget->GetOwnerDoc() == mDocument) {
    NS_RELEASE(gKeyDownTarget);
  }

  mContentToScrollTo = nsnull;

  if (mPresContext) {
    // We need to notify the destroying the nsPresContext to ESM for
    // suppressing to use from ESM.
    mPresContext->EventStateManager()->NotifyDestroyPresContext(mPresContext);
  }

  {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->RemoveObserver(this, "agent-sheet-added");
      os->RemoveObserver(this, "user-sheet-added");
      os->RemoveObserver(this, "agent-sheet-removed");
      os->RemoveObserver(this, "user-sheet-removed");
#ifdef MOZ_XUL
      os->RemoveObserver(this, "chrome-flush-skin-caches");
#endif
#ifdef ACCESSIBILITY
      os->RemoveObserver(this, "a11y-init-or-shutdown");
#endif
    }
  }

  // If our paint suppression timer is still active, kill it.
  if (mPaintSuppressionTimer) {
    mPaintSuppressionTimer->Cancel();
    mPaintSuppressionTimer = nsnull;
  }

  // Same for our reflow continuation timer
  if (mReflowContinueTimer) {
    mReflowContinueTimer->Cancel();
    mReflowContinueTimer = nsnull;
  }

  if (mCaret) {
    mCaret->Terminate();
    mCaret = nsnull;
  }

  if (mSelection) {
    mSelection->DisconnectFromPresShell();
  }

  // release our pref style sheet, if we have one still
  ClearPreferenceStyleRules();

  mIsDestroying = PR_TRUE;

  // We can't release all the event content in
  // mCurrentEventContentStack here since there might be code on the
  // stack that will release the event content too. Double release
  // bad!

  // The frames will be torn down, so remove them from the current
  // event frame stack (since they'd be dangling references if we'd
  // leave them in) and null out the mCurrentEventFrame pointer as
  // well.

  mCurrentEventFrame = nsnull;

  PRInt32 i, count = mCurrentEventFrameStack.Length();
  for (i = 0; i < count; i++) {
    mCurrentEventFrameStack[i] = nsnull;
  }

  mFramesToDirty.Clear();

  if (mViewManager) {
    // Clear the view manager's weak pointer back to |this| in case it
    // was leaked.
    mViewManager->SetViewObserver(nsnull);
    mViewManager = nsnull;
  }

  mStyleSet->BeginShutdown(mPresContext);
  nsRefreshDriver* rd = GetPresContext()->RefreshDriver();

  // This shell must be removed from the document before the frame
  // hierarchy is torn down to avoid finding deleted frames through
  // this presshell while the frames are being torn down
  if (mDocument) {
    NS_ASSERTION(mDocument->GetShell() == this, "Wrong shell?");
    mDocument->DeleteShell();

#ifdef MOZ_SMIL
    if (mDocument->HasAnimationController()) {
      mDocument->GetAnimationController()->StopSampling(rd);
    }
#endif // MOZ_SMIL
  }

  // Revoke any pending events.  We need to do this and cancel pending reflows
  // before we destroy the frame manager, since apparently frame destruction
  // sometimes spins the event queue when plug-ins are involved(!).
  rd->RemoveLayoutFlushObserver(this);
  mResizeEvent.Revoke();
  if (mAsyncResizeTimerIsActive) {
    mAsyncResizeEventTimer->Cancel();
    mAsyncResizeTimerIsActive = PR_FALSE;
  }

  CancelAllPendingReflows();
  CancelPostedReflowCallbacks();

  // Destroy the frame manager. This will destroy the frame hierarchy
  mFrameConstructor->WillDestroyFrameTree();
  FrameManager()->Destroy();

  // Destroy all frame properties (whose destruction was suppressed
  // while destroying the frame tree, but which might contain more
  // frames within the properties.
  if (mPresContext) {
    // Clear out the prescontext's property table -- since our frame tree is
    // now dead, we shouldn't be looking up any more properties in that table.
    // We want to do this before we call SetShell() on the prescontext, so
    // property destructors can usefully call GetPresShell() on the
    // prescontext.
    mPresContext->PropertyTable()->DeleteAll();
  }


  NS_WARN_IF_FALSE(!mWeakFrames, "Weak frames alive after destroying FrameManager");
  while (mWeakFrames) {
    mWeakFrames->Clear(this);
  }

  // Let the style set do its cleanup.
  mStyleSet->Shutdown(mPresContext);

  if (mPresContext) {
    // We hold a reference to the pres context, and it holds a weak link back
    // to us. To avoid the pres context having a dangling reference, set its 
    // pres shell to NULL
    mPresContext->SetShell(nsnull);

    // Clear the link handler (weak reference) as well
    mPresContext->SetLinkHandler(nsnull);
  }

  mHaveShutDown = PR_TRUE;
}

                  // Dynamic stack memory allocation
/* virtual */ void
PresShell::PushStackMemory()
{
  mStackArena.Push();
}

/* virtual */ void
PresShell::PopStackMemory()
{
  mStackArena.Pop();
}

/* virtual */ void*
PresShell::AllocateStackMemory(size_t aSize)
{
  return mStackArena.Allocate(aSize);
}

void
PresShell::FreeFrame(nsQueryFrame::FrameIID aCode, void* aPtr)
{
#ifdef DEBUG
  mPresArenaAllocCount--;
#endif
  if (PRESARENA_MUST_FREE_DURING_DESTROY || !mIsDestroying)
    mFrameArena.FreeByCode(aCode, aPtr);
}

void*
PresShell::AllocateFrame(nsQueryFrame::FrameIID aCode, size_t aSize)
{
#ifdef DEBUG
  mPresArenaAllocCount++;
#endif
  void* result = mFrameArena.AllocateByCode(aCode, aSize);

  if (result) {
    memset(result, 0, aSize);
  }
  return result;
}

void
PresShell::FreeMisc(size_t aSize, void* aPtr)
{
#ifdef DEBUG
  mPresArenaAllocCount--;
#endif
  if (PRESARENA_MUST_FREE_DURING_DESTROY || !mIsDestroying)
    mFrameArena.FreeBySize(aSize, aPtr);
}

void*
PresShell::AllocateMisc(size_t aSize)
{
#ifdef DEBUG
  mPresArenaAllocCount++;
#endif
  return mFrameArena.AllocateBySize(aSize);
}

void
nsIPresShell::SetAuthorStyleDisabled(PRBool aStyleDisabled)
{
  if (aStyleDisabled != mStyleSet->GetAuthorStyleDisabled()) {
    mStyleSet->SetAuthorStyleDisabled(aStyleDisabled);
    ReconstructStyleData();
  }
}

PRBool
nsIPresShell::GetAuthorStyleDisabled() const
{
  return mStyleSet->GetAuthorStyleDisabled();
}

nsresult
PresShell::SetPreferenceStyleRules(PRBool aForceReflow)
{
  NS_TIME_FUNCTION_MIN(1.0);

  if (!mDocument) {
    return NS_ERROR_NULL_POINTER;
  }

  nsPIDOMWindow *window = mDocument->GetWindow();

  // If the document doesn't have a window there's no need to notify
  // its presshell about changes to preferences since the document is
  // in a state where it doesn't matter any more (see
  // DocumentViewerImpl::Close()).

  if (!window) {
    return NS_ERROR_NULL_POINTER;
  } 

  NS_PRECONDITION(mPresContext, "presContext cannot be null");
  if (mPresContext) {
    // first, make sure this is not a chrome shell 
    if (nsContentUtils::IsInChromeDocshell(mDocument)) {
      return NS_OK;
    }

#ifdef DEBUG_attinasi
    printf("Setting Preference Style Rules:\n");
#endif
    // if here, we need to create rules for the prefs
    // - this includes the background-color, the text-color,
    //   the link color, the visited link color and the link-underlining
    
    // first clear any exising rules
    nsresult result = ClearPreferenceStyleRules();
      
    // now the link rules (must come after the color rules, or links will not be correct color!)
    // XXX - when there is both an override and agent pref stylesheet this won't matter,
    //       as the color rules will be overrides and the links rules will be agent
    if (NS_SUCCEEDED(result)) {
      result = SetPrefLinkRules();
    }
    if (NS_SUCCEEDED(result)) {
      result = SetPrefFocusRules();
    }
    if (NS_SUCCEEDED(result)) {
      result = SetPrefNoScriptRule();
    }
    if (NS_SUCCEEDED(result)) {
      result = SetPrefNoFramesRule();
    }
#ifdef DEBUG_attinasi
    printf( "Preference Style Rules set: error=%ld\n", (long)result);
#endif

    // Note that this method never needs to force any calculation; the caller
    // will recalculate style if needed

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
      PRInt32 numBefore = mStyleSet->SheetCount(nsStyleSet::eUserSheet);
      NS_ASSERTION(numBefore > 0, "no user stylesheets in styleset, but we have one!");
#endif
      mStyleSet->RemoveStyleSheet(nsStyleSet::eUserSheet, mPrefStyleSheet);

#ifdef DEBUG_attinasi
      NS_ASSERTION((numBefore - 1) == mStyleSet->GetNumberOfUserStyleSheets(),
                   "Pref stylesheet was not removed");
      printf("PrefStyleSheet removed\n");
#endif
      // clear the sheet pointer: it is strictly historical now
      mPrefStyleSheet = nsnull;
    }
  }
  return result;
}

nsresult PresShell::CreatePreferenceStyleSheet(void)
{
  NS_TIME_FUNCTION_MIN(1.0);

  NS_ASSERTION(!mPrefStyleSheet, "prefStyleSheet already exists");
  nsresult result = NS_NewCSSStyleSheet(getter_AddRefs(mPrefStyleSheet));
  if (NS_SUCCEEDED(result)) {
    NS_ASSERTION(mPrefStyleSheet, "null but no error");
    nsCOMPtr<nsIURI> uri;
    result = NS_NewURI(getter_AddRefs(uri), "about:PreferenceStyleSheet", nsnull);
    if (NS_SUCCEEDED(result)) {
      NS_ASSERTION(uri, "null but no error");
      mPrefStyleSheet->SetURIs(uri, uri, uri);
      mPrefStyleSheet->SetComplete();
      PRUint32 index;
      result =
        mPrefStyleSheet->InsertRuleInternal(NS_LITERAL_STRING("@namespace url(http://www.w3.org/1999/xhtml);"),
                                            0, &index);
      if (NS_SUCCEEDED(result)) {
        mStyleSet->AppendStyleSheet(nsStyleSet::eUserSheet, mPrefStyleSheet);
      }
    }
  }

#ifdef DEBUG_attinasi
  printf("CreatePrefStyleSheet completed: error=%ld\n",(long)result);
#endif

  if (NS_FAILED(result)) {
    mPrefStyleSheet = nsnull;
  }

  return result;
}

// XXX We want these after the @namespace rule.  Does order matter
// for these rules, or can we call nsICSSStyleRule::StyleRuleCount()
// and just "append"?
static PRUint32 sInsertPrefSheetRulesAt = 1;

nsresult
PresShell::SetPrefNoScriptRule()
{
  NS_TIME_FUNCTION_MIN(1.0);

  nsresult rv = NS_OK;

  // also handle the case where print is done from print preview
  // see bug #342439 for more details
  nsIDocument* doc = mDocument;
  if (mPresContext->Type() == nsPresContext::eContext_PrintPreview ||
      mPresContext->Type() == nsPresContext::eContext_Print) {
    while (doc->GetOriginalDocument()) {
      doc = doc->GetOriginalDocument();
    }
  }

  PRBool scriptEnabled = doc->IsScriptEnabled();
  if (scriptEnabled) {
    if (!mPrefStyleSheet) {
      rv = CreatePreferenceStyleSheet();
      NS_ENSURE_SUCCESS(rv, rv);
    }

    PRUint32 index = 0;
    mPrefStyleSheet->
      InsertRuleInternal(NS_LITERAL_STRING("noscript{display:none!important}"),
                         sInsertPrefSheetRulesAt, &index);
  }

  return rv;
}

nsresult PresShell::SetPrefNoFramesRule(void)
{
  NS_TIME_FUNCTION_MIN(1.0);

  NS_ASSERTION(mPresContext,"null prescontext not allowed");
  if (!mPresContext) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = NS_OK;
  
  if (!mPrefStyleSheet) {
    rv = CreatePreferenceStyleSheet();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  
  NS_ASSERTION(mPrefStyleSheet, "prefstylesheet should not be null");
  
  PRBool allowSubframes = PR_TRUE;
  nsCOMPtr<nsISupports> container = mPresContext->GetContainer();     
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(container));
  if (docShell) {
    docShell->GetAllowSubframes(&allowSubframes);
  }
  if (!allowSubframes) {
    PRUint32 index = 0;
    rv = mPrefStyleSheet->
      InsertRuleInternal(NS_LITERAL_STRING("noframes{display:block}"),
                         sInsertPrefSheetRulesAt, &index);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mPrefStyleSheet->
      InsertRuleInternal(NS_LITERAL_STRING("frame, frameset, iframe {display:none!important}"),
                         sInsertPrefSheetRulesAt, &index);
  }
  return rv;
}
  
nsresult PresShell::SetPrefLinkRules(void)
{
  NS_TIME_FUNCTION_MIN(1.0);

  NS_ASSERTION(mPresContext,"null prescontext not allowed");
  if (!mPresContext) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = NS_OK;
  
  if (!mPrefStyleSheet) {
    rv = CreatePreferenceStyleSheet();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  
  NS_ASSERTION(mPrefStyleSheet, "prefstylesheet should not be null");
  
  // support default link colors: 
  //   this means the link colors need to be overridable, 
  //   which they are if we put them in the agent stylesheet,
  //   though if using an override sheet this will cause authors grief still
  //   In the agent stylesheet, they are !important when we are ignoring document colors
  
  nscolor linkColor(mPresContext->DefaultLinkColor());
  nscolor activeColor(mPresContext->DefaultActiveLinkColor());
  nscolor visitedColor(mPresContext->DefaultVisitedLinkColor());
  
  NS_NAMED_LITERAL_STRING(ruleClose, "}");
  PRUint32 index = 0;
  nsAutoString strColor;

  // insert a rule to color links: '*|*:link {color: #RRGGBB [!important];}'
  ColorToString(linkColor, strColor);
  rv = mPrefStyleSheet->
    InsertRuleInternal(NS_LITERAL_STRING("*|*:link{color:") +
                       strColor + ruleClose,
                       sInsertPrefSheetRulesAt, &index);
  NS_ENSURE_SUCCESS(rv, rv);

  // - visited links: '*|*:visited {color: #RRGGBB [!important];}'
  ColorToString(visitedColor, strColor);
  rv = mPrefStyleSheet->
    InsertRuleInternal(NS_LITERAL_STRING("*|*:visited{color:") +
                       strColor + ruleClose,
                       sInsertPrefSheetRulesAt, &index);
  NS_ENSURE_SUCCESS(rv, rv);

  // - active links: '*|*:-moz-any-link:active {color: #RRGGBB [!important];}'
  ColorToString(activeColor, strColor);
  rv = mPrefStyleSheet->
    InsertRuleInternal(NS_LITERAL_STRING("*|*:-moz-any-link:active{color:") +
                       strColor + ruleClose,
                       sInsertPrefSheetRulesAt, &index);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool underlineLinks =
    mPresContext->GetCachedBoolPref(kPresContext_UnderlineLinks);

  if (underlineLinks) {
    // create a rule to make underlining happen
    //  '*|*:-moz-any-link {text-decoration:[underline|none];}'
    // no need for important, we want these to be overridable
    // NOTE: these must go in the agent stylesheet or they cannot be
    //       overridden by authors
    rv = mPrefStyleSheet->
      InsertRuleInternal(NS_LITERAL_STRING("*|*:-moz-any-link{text-decoration:underline}"),
                         sInsertPrefSheetRulesAt, &index);
  } else {
    rv = mPrefStyleSheet->
      InsertRuleInternal(NS_LITERAL_STRING("*|*:-moz-any-link{text-decoration:none}"),
                         sInsertPrefSheetRulesAt, &index);
  }

  return rv;          
}

nsresult PresShell::SetPrefFocusRules(void)
{
  NS_TIME_FUNCTION_MIN(1.0);

  NS_ASSERTION(mPresContext,"null prescontext not allowed");
  nsresult result = NS_OK;

  if (!mPresContext)
    result = NS_ERROR_FAILURE;

  if (NS_SUCCEEDED(result) && !mPrefStyleSheet)
    result = CreatePreferenceStyleSheet();

  if (NS_SUCCEEDED(result)) {
    NS_ASSERTION(mPrefStyleSheet, "prefstylesheet should not be null");

    if (mPresContext->GetUseFocusColors()) {
      nscolor focusBackground(mPresContext->FocusBackgroundColor());
      nscolor focusText(mPresContext->FocusTextColor());

      // insert a rule to make focus the preferred color
      PRUint32 index = 0;
      nsAutoString strRule, strColor;

      ///////////////////////////////////////////////////////////////
      // - focus: '*:focus
      ColorToString(focusText,strColor);
      strRule.AppendLiteral("*:focus,*:focus>font {color: ");
      strRule.Append(strColor);
      strRule.AppendLiteral(" !important; background-color: ");
      ColorToString(focusBackground,strColor);
      strRule.Append(strColor);
      strRule.AppendLiteral(" !important; } ");
      // insert the rules
      result = mPrefStyleSheet->
        InsertRuleInternal(strRule, sInsertPrefSheetRulesAt, &index);
    }
    PRUint8 focusRingWidth = mPresContext->FocusRingWidth();
    PRBool focusRingOnAnything = mPresContext->GetFocusRingOnAnything();
    PRUint8 focusRingStyle = mPresContext->GetFocusRingStyle();

    if ((NS_SUCCEEDED(result) && focusRingWidth != 1 && focusRingWidth <= 4 ) || focusRingOnAnything) {
      PRUint32 index = 0;
      nsAutoString strRule;
      if (!focusRingOnAnything)
        strRule.AppendLiteral("*|*:link:focus, *|*:visited");    // If we only want focus rings on the normal things like links
      strRule.AppendLiteral(":focus {outline: ");     // For example 3px dotted WindowText (maximum 4)
      strRule.AppendInt(focusRingWidth);
      if (focusRingStyle == 0) // solid
        strRule.AppendLiteral("px solid -moz-mac-focusring !important; -moz-outline-radius: 3px; outline-offset: 1px; } ");
      else // dotted
        strRule.AppendLiteral("px dotted WindowText !important; } ");
      // insert the rules
      result = mPrefStyleSheet->
        InsertRuleInternal(strRule, sInsertPrefSheetRulesAt, &index);
      NS_ENSURE_SUCCESS(result, result);
      if (focusRingWidth != 1) {
        // If the focus ring width is different from the default, fix buttons with rings
        strRule.AssignLiteral("button::-moz-focus-inner, input[type=\"reset\"]::-moz-focus-inner,");
        strRule.AppendLiteral("input[type=\"button\"]::-moz-focus-inner, ");
        strRule.AppendLiteral("input[type=\"submit\"]::-moz-focus-inner { padding: 1px 2px 1px 2px; border: ");
        strRule.AppendInt(focusRingWidth);
        if (focusRingStyle == 0) // solid
          strRule.AppendLiteral("px solid transparent !important; } ");
        else
          strRule.AppendLiteral("px dotted transparent !important; } ");
        result = mPrefStyleSheet->
          InsertRuleInternal(strRule, sInsertPrefSheetRulesAt, &index);
        NS_ENSURE_SUCCESS(result, result);
          
        strRule.AssignLiteral("button:focus::-moz-focus-inner, input[type=\"reset\"]:focus::-moz-focus-inner,");
        strRule.AppendLiteral("input[type=\"button\"]:focus::-moz-focus-inner, input[type=\"submit\"]:focus::-moz-focus-inner {");
        strRule.AppendLiteral("border-color: ButtonText !important; }");
        result = mPrefStyleSheet->
          InsertRuleInternal(strRule, sInsertPrefSheetRulesAt, &index);
      }
    }
  }
  return result;
}

void
PresShell::AddUserSheet(nsISupports* aSheet)
{
  // Make sure this does what DocumentViewerImpl::CreateStyleSet does wrt
  // ordering. We want this new sheet to come after all the existing stylesheet
  // service sheets, but before other user sheets; see nsIStyleSheetService.idl
  // for the ordering.  Just remove and readd all the nsStyleSheetService
  // sheets.
  nsCOMPtr<nsIStyleSheetService> dummy =
    do_GetService(NS_STYLESHEETSERVICE_CONTRACTID);

  mStyleSet->BeginUpdate();
  
  nsStyleSheetService *sheetService = nsStyleSheetService::gInstance;
  nsCOMArray<nsIStyleSheet> & userSheets = *sheetService->UserStyleSheets();
  PRInt32 i;
  // Iterate forwards when removing so the searches for RemoveStyleSheet are as
  // short as possible.
  for (i = 0; i < userSheets.Count(); ++i) {
    mStyleSet->RemoveStyleSheet(nsStyleSet::eUserSheet, userSheets[i]);
  }

  // Now iterate backwards, so that the order of userSheets will be the same as
  // the order of sheets from it in the style set.
  for (i = userSheets.Count() - 1; i >= 0; --i) {
    mStyleSet->PrependStyleSheet(nsStyleSet::eUserSheet, userSheets[i]);
  }

  mStyleSet->EndUpdate();

  ReconstructStyleData();
}

void
PresShell::AddAgentSheet(nsISupports* aSheet)
{
  // Make sure this does what DocumentViewerImpl::CreateStyleSet does
  // wrt ordering.
  nsCOMPtr<nsIStyleSheet> sheet = do_QueryInterface(aSheet);
  if (!sheet) {
    return;
  }

  mStyleSet->AppendStyleSheet(nsStyleSet::eAgentSheet, sheet);
  ReconstructStyleData();
}

void
PresShell::RemoveSheet(nsStyleSet::sheetType aType, nsISupports* aSheet)
{
  nsCOMPtr<nsIStyleSheet> sheet = do_QueryInterface(aSheet);
  if (!sheet) {
    return;
  }

  mStyleSet->RemoveStyleSheet(aType, sheet);
  ReconstructStyleData();
}

NS_IMETHODIMP
PresShell::SetDisplaySelection(PRInt16 aToggle)
{
  mSelection->SetDisplaySelection(aToggle);
  return NS_OK;
}

NS_IMETHODIMP
PresShell::GetDisplaySelection(PRInt16 *aToggle)
{
  *aToggle = mSelection->GetDisplaySelection();
  return NS_OK;
}

NS_IMETHODIMP
PresShell::GetSelection(SelectionType aType, nsISelection **aSelection)
{
  if (!aSelection || !mSelection)
    return NS_ERROR_NULL_POINTER;

  *aSelection = mSelection->GetSelection(aType);

  if (!(*aSelection))
    return NS_ERROR_INVALID_ARG;

  NS_ADDREF(*aSelection);

  return NS_OK;
}

nsISelection*
PresShell::GetCurrentSelection(SelectionType aType)
{
  if (!mSelection)
    return nsnull;

  return mSelection->GetSelection(aType);
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

  return mSelection->RepaintSelection(aType);
}

// Make shell be a document observer
void
PresShell::BeginObservingDocument()
{
  if (mDocument && !mIsDestroying) {
    mDocument->AddObserver(this);
    if (mIsDocumentGone) {
      NS_WARNING("Adding a presshell that was disconnected from the document "
                 "as a document observer?  Sounds wrong...");
      mIsDocumentGone = PR_FALSE;
    }
  }
}

// Make shell stop being a document observer
void
PresShell::EndObservingDocument()
{
  // XXXbz do we need to tell the frame constructor that the document
  // is gone, perhaps?  Except for printing it's NOT gone, sometimes.
  mIsDocumentGone = PR_TRUE;
  if (mDocument) {
    mDocument->RemoveObserver(this);
  }
}

#ifdef DEBUG_kipp
char* nsPresShell_ReflowStackPointerTop;
#endif

nsresult
PresShell::InitialReflow(nscoord aWidth, nscoord aHeight)
{
  if (mIsDestroying) {
    return NS_OK;
  }

  if (!mDocument) {
    // Nothing to do
    return NS_OK;
  }

  NS_TIME_FUNCTION_WITH_DOCURL;

  NS_ASSERTION(!mDidInitialReflow, "Why are we being called?");

  nsCOMPtr<nsIPresShell> kungFuDeathGrip(this);
  mDidInitialReflow = PR_TRUE;

#ifdef NS_DEBUG
  if (VERIFY_REFLOW_NOISY_RC & gVerifyReflowFlags) {
    if (mDocument) {
      nsIURI *uri = mDocument->GetDocumentURI();
      if (uri) {
        nsCAutoString url;
        uri->GetSpec(url);
        printf("*** PresShell::InitialReflow (this=%p, url='%s')\n", (void*)this, url.get());
      }
    }
  }
#endif

  if (mCaret)
    mCaret->EraseCaret();

  // XXX Do a full invalidate at the beginning so that invalidates along
  // the way don't have region accumulation issues?

  mPresContext->SetVisibleArea(nsRect(0, 0, aWidth, aHeight));

  // Get the root frame from the frame manager
  // XXXbz it would be nice to move this somewhere else... like frame manager
  // Init(), say.  But we need to make sure our views are all set up by the
  // time we do this!
  nsIFrame* rootFrame = FrameManager()->GetRootFrame();
  NS_ASSERTION(!rootFrame, "How did that happen, exactly?");
  if (!rootFrame) {
    nsAutoScriptBlocker scriptBlocker;
    mFrameConstructor->BeginUpdate();
    mFrameConstructor->ConstructRootFrame(&rootFrame);
    FrameManager()->SetRootFrame(rootFrame);
    mFrameConstructor->EndUpdate();
  }

  NS_ENSURE_STATE(!mHaveShutDown);

  if (!rootFrame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  Element *root = mDocument->GetRootElement();

  if (root) {
    {
      nsAutoCauseReflowNotifier reflowNotifier(this);
      mFrameConstructor->BeginUpdate();

      // Have the style sheet processor construct frame for the root
      // content object down
      mFrameConstructor->ContentInserted(nsnull, root, nsnull, PR_FALSE);
      VERIFY_STYLE_TREE;

      // Something in mFrameConstructor->ContentInserted may have caused
      // Destroy() to get called, bug 337586.
      NS_ENSURE_STATE(!mHaveShutDown);

      mFrameConstructor->EndUpdate();
    }

    // nsAutoScriptBlocker going out of scope may have killed us too
    NS_ENSURE_STATE(!mHaveShutDown);

    // Run the XBL binding constructors for any new frames we've constructed
    mDocument->BindingManager()->ProcessAttachedQueue();

    NS_TIME_FUNCTION_MARK("XBL binding constructors fired");

    // Constructors may have killed us too
    NS_ENSURE_STATE(!mHaveShutDown);

    // Now flush out pending restyles before we actually reflow, in
    // case XBL constructors changed styles somewhere.
    {
      nsAutoScriptBlocker scriptBlocker;
      mFrameConstructor->CreateNeededFrames();
      mFrameConstructor->ProcessPendingRestyles();
    }

    // And that might have run _more_ XBL constructors
    NS_ENSURE_STATE(!mHaveShutDown);
  }

  NS_ASSERTION(rootFrame, "How did that happen?");

  // Note: Because the frame just got created, it has the NS_FRAME_IS_DIRTY
  // bit set.  Unset it so that FrameNeedsReflow() will work right.
  NS_ASSERTION(!mDirtyRoots.Contains(rootFrame),
               "Why is the root in mDirtyRoots already?");

  rootFrame->RemoveStateBits(NS_FRAME_IS_DIRTY |
                             NS_FRAME_HAS_DIRTY_CHILDREN);
  FrameNeedsReflow(rootFrame, eResize, NS_FRAME_IS_DIRTY);

  NS_ASSERTION(mDirtyRoots.Contains(rootFrame),
               "Should be in mDirtyRoots now");
  NS_ASSERTION(mReflowScheduled, "Why no reflow scheduled?");

  // Restore our root scroll position now if we're getting here after EndLoad
  // got called, since this is our one chance to do it.  Note that we need not
  // have reflowed for this to work; when the scrollframe is finally reflowed
  // it'll puick up the position we store in it here.
  if (!mDocumentLoading) {
    RestoreRootScrollPosition();
  }

  // For printing, we just immediately unsuppress.
  if (!mPresContext->IsPaginated()) {
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

      // Default to PAINTLOCK_EVENT_DELAY if we can't get the pref value.
      PRInt32 delay =
        nsContentUtils::GetIntPref("nglayout.initialpaint.delay",
                                   PAINTLOCK_EVENT_DELAY);

      mPaintSuppressionTimer->InitWithFuncCallback(sPaintSuppressionCallback,
                                                   this, delay, 
                                                   nsITimer::TYPE_ONE_SHOT);
    }
  }

  return NS_OK; //XXX this needs to be real. MMP
}

void
PresShell::sPaintSuppressionCallback(nsITimer *aTimer, void* aPresShell)
{
  nsRefPtr<PresShell> self = static_cast<PresShell*>(aPresShell);
  if (self)
    self->UnsuppressPainting();
}

void
PresShell::AsyncResizeEventCallback(nsITimer* aTimer, void* aPresShell)
{
  static_cast<PresShell*>(aPresShell)->FireResizeEvent();
}

nsresult
PresShell::ResizeReflow(nscoord aWidth, nscoord aHeight)
{
  NS_PRECONDITION(!mIsReflowing, "Shouldn't be in reflow here!");
  NS_PRECONDITION(aWidth != NS_UNCONSTRAINEDSIZE,
                  "shouldn't use unconstrained widths anymore");
  
  // If we don't have a root frame yet, that means we haven't had our initial
  // reflow... If that's the case, and aWidth or aHeight is unconstrained,
  // ignore them altogether.
  nsIFrame* rootFrame = FrameManager()->GetRootFrame();

  if (!rootFrame && aHeight == NS_UNCONSTRAINEDSIZE) {
    // We can't do the work needed for SizeToContent without a root
    // frame, and we want to return before setting the visible area.
    return NS_ERROR_NOT_AVAILABLE;
  }

  mPresContext->SetVisibleArea(nsRect(0, 0, aWidth, aHeight));

  // There isn't anything useful we can do if the initial reflow hasn't happened
  if (!rootFrame)
    return NS_OK;

  NS_ASSERTION(mViewManager, "Must have view manager");
  nsCOMPtr<nsIViewManager> viewManagerDeathGrip = mViewManager;
  // Take this ref after viewManager so it'll make sure to go away first
  nsCOMPtr<nsIPresShell> kungFuDeathGrip(this);
  if (!GetPresContext()->SupressingResizeReflow())
  {
    nsIViewManager::UpdateViewBatch batch(mViewManager);

    // Have to make sure that the content notifications are flushed before we
    // start messing with the frame model; otherwise we can get content doubling.
    mDocument->FlushPendingNotifications(Flush_ContentAndNotify);

    // Make sure style is up to date
    {
      nsAutoScriptBlocker scriptBlocker;
      mFrameConstructor->CreateNeededFrames();
      mFrameConstructor->ProcessPendingRestyles();
    }

    if (!mIsDestroying) {
      // XXX Do a full invalidate at the beginning so that invalidates along
      // the way don't have region accumulation issues?

      {
        nsAutoCauseReflowNotifier crNotifier(this);
        WillDoReflow();

        // Kick off a top-down reflow
        AUTO_LAYOUT_PHASE_ENTRY_POINT(GetPresContext(), Reflow);

        mDirtyRoots.RemoveElement(rootFrame);
        DoReflow(rootFrame, PR_TRUE);
      }

      DidDoReflow(PR_TRUE);
    }

    batch.EndUpdateViewBatch(NS_VMREFRESH_NO_SYNC);
  }

  if (aHeight == NS_UNCONSTRAINEDSIZE) {
    mPresContext->SetVisibleArea(
      nsRect(0, 0, aWidth, rootFrame->GetRect().height));
  }

  if (!mIsDestroying && !mResizeEvent.IsPending() &&
      !mAsyncResizeTimerIsActive) {
    if (mInResize) {
      if (!mAsyncResizeEventTimer) {
        mAsyncResizeEventTimer = do_CreateInstance("@mozilla.org/timer;1");
      }
      if (mAsyncResizeEventTimer) {
        mAsyncResizeTimerIsActive = PR_TRUE;
        mAsyncResizeEventTimer->InitWithFuncCallback(AsyncResizeEventCallback,
                                                     this, 15,
                                                     nsITimer::TYPE_ONE_SHOT);
      }
    } else {
      nsRefPtr<nsRunnableMethod<PresShell> > resizeEvent =
        NS_NewRunnableMethod(this, &PresShell::FireResizeEvent);
      if (NS_SUCCEEDED(NS_DispatchToCurrentThread(resizeEvent))) {
        mResizeEvent = resizeEvent;
      }
    }
  }

  return NS_OK; //XXX this needs to be real. MMP
}

void
PresShell::FireResizeEvent()
{
  if (mAsyncResizeTimerIsActive) {
    mAsyncResizeTimerIsActive = PR_FALSE;
    mAsyncResizeEventTimer->Cancel();
  }
  mResizeEvent.Revoke();

  if (mIsDocumentGone)
    return;

  //Send resize event from here.
  nsEvent event(PR_TRUE, NS_RESIZE_EVENT);
  nsEventStatus status = nsEventStatus_eIgnore;

  nsPIDOMWindow *window = mDocument->GetWindow();
  if (window) {
    nsCOMPtr<nsIPresShell> kungFuDeathGrip(this);
    mInResize = PR_TRUE;
    nsEventDispatcher::Dispatch(window, mPresContext, &event, nsnull, &status);
    mInResize = PR_FALSE;
  }
}

void
PresShell::SetIgnoreFrameDestruction(PRBool aIgnore)
{
  mIgnoreFrameDestruction = aIgnore;
}

void
PresShell::NotifyDestroyingFrame(nsIFrame* aFrame)
{
  NS_TIME_FUNCTION_MIN(1.0);

  mPresContext->ForgetUpdatePluginGeometryFrame(aFrame);

  if (!mIgnoreFrameDestruction) {
    mPresContext->StopImagesFor(aFrame);

    mFrameConstructor->NotifyDestroyingFrame(aFrame);

    for (PRInt32 idx = mDirtyRoots.Length(); idx; ) {
      --idx;
      if (mDirtyRoots[idx] == aFrame) {
        mDirtyRoots.RemoveElementAt(idx);
      }
    }

    // Notify the frame manager
    FrameManager()->NotifyDestroyingFrame(aFrame);

    // Remove frame properties
    mPresContext->NotifyDestroyingFrame(aFrame);

    if (aFrame == mCurrentEventFrame) {
      mCurrentEventContent = aFrame->GetContent();
      mCurrentEventFrame = nsnull;
    }

  #ifdef NS_DEBUG
    if (aFrame == mDrawEventTargetFrame) {
      mDrawEventTargetFrame = nsnull;
    }
  #endif

    for (unsigned int i=0; i < mCurrentEventFrameStack.Length(); i++) {
      if (aFrame == mCurrentEventFrameStack.ElementAt(i)) {
        //One of our stack frames was deleted.  Get its content so that when we
        //pop it we can still get its new frame from its content
        nsIContent *currentEventContent = aFrame->GetContent();
        mCurrentEventContentStack.ReplaceObjectAt(currentEventContent, i);
        mCurrentEventFrameStack[i] = nsnull;
      }
    }
  
    mFramesToDirty.RemoveEntry(aFrame);
  }
}

already_AddRefed<nsCaret> PresShell::GetCaret() const
{
  nsCaret* caret = mCaret;
  NS_IF_ADDREF(caret);
  return caret;
}

void PresShell::MaybeInvalidateCaretPosition()
{
  if (mCaret) {
    mCaret->InvalidateOutsideCaret();
  }
}

void PresShell::SetCaret(nsCaret *aNewCaret)
{
  mCaret = aNewCaret;
}

void PresShell::RestoreCaret()
{
  mCaret = mOriginalCaret;
}

NS_IMETHODIMP PresShell::SetCaretEnabled(PRBool aInEnable)
{
  PRBool oldEnabled = mCaretEnabled;

  mCaretEnabled = aInEnable;

  if (mCaret && (mCaretEnabled != oldEnabled))
  {
/*  Don't change the caret's selection here! This was an evil side-effect of SetCaretEnabled()
    nsCOMPtr<nsIDOMSelection> domSel;
    if (NS_SUCCEEDED(GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSel))) && domSel)
      mCaret->SetCaretDOMSelection(domSel);
*/
    mCaret->SetCaretVisible(mCaretEnabled);
  }

  return NS_OK;
}

NS_IMETHODIMP PresShell::SetCaretReadOnly(PRBool aReadOnly)
{
  if (mCaret)
    mCaret->SetCaretReadOnly(aReadOnly);
  return NS_OK;
}

NS_IMETHODIMP PresShell::GetCaretEnabled(PRBool *aOutEnabled)
{
  NS_ENSURE_ARG_POINTER(aOutEnabled);
  *aOutEnabled = mCaretEnabled;
  return NS_OK;
}

NS_IMETHODIMP PresShell::SetCaretVisibilityDuringSelection(PRBool aVisibility)
{
  if (mCaret)
    mCaret->SetVisibilityDuringSelection(aVisibility);
  return NS_OK;
}

NS_IMETHODIMP PresShell::GetCaretVisible(PRBool *aOutIsVisible)
{
  *aOutIsVisible = PR_FALSE;
  if (mCaret) {
    nsresult rv = mCaret->GetCaretVisible(aOutIsVisible);
    NS_ENSURE_SUCCESS(rv,rv);
  }
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
PresShell::CharacterExtendForDelete()
{
  return mSelection->CharacterExtendForDelete();
}

NS_IMETHODIMP
PresShell::CharacterExtendForBackspace()
{
  return mSelection->CharacterExtendForBackspace();
}

NS_IMETHODIMP 
PresShell::WordMove(PRBool aForward, PRBool aExtend)
{
  return mSelection->WordMove(aForward, aExtend);  
}

NS_IMETHODIMP 
PresShell::WordExtendForDelete(PRBool aForward)
{
  return mSelection->WordExtendForDelete(aForward);  
}

NS_IMETHODIMP 
PresShell::LineMove(PRBool aForward, PRBool aExtend)
{
  nsresult result = mSelection->LineMove(aForward, aExtend);  
// if we can't go down/up any more we must then move caret completely to 
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
  nsIScrollableFrame *scrollableFrame =
    GetFrameToScrollAsScrollable(nsIPresShell::eVertical);
  if (!scrollableFrame)
    return NS_OK;

  mSelection->CommonPageMove(aForward, aExtend, scrollableFrame);
  // After ScrollSelectionIntoView(), the pending notifications might be
  // flushed and PresShell/PresContext/Frames may be dead. See bug 418470.
  return ScrollSelectionIntoView(nsISelectionController::SELECTION_NORMAL, nsISelectionController::SELECTION_FOCUS_REGION, PR_TRUE);
}



NS_IMETHODIMP 
PresShell::ScrollPage(PRBool aForward)
{
  nsIScrollableFrame* scrollFrame =
    GetFrameToScrollAsScrollable(nsIPresShell::eVertical);
  if (scrollFrame) {
    scrollFrame->ScrollBy(nsIntPoint(0, aForward ? 1 : -1),
                          nsIScrollableFrame::PAGES,
                          nsIScrollableFrame::SMOOTH);
  }
  return NS_OK;
}

NS_IMETHODIMP
PresShell::ScrollLine(PRBool aForward)
{
  nsIScrollableFrame* scrollFrame =
    GetFrameToScrollAsScrollable(nsIPresShell::eVertical);
  if (scrollFrame) {
    PRInt32 lineCount = 1;
#ifdef MOZ_WIDGET_COCOA
    // Emulate the Mac IE behavior of scrolling a minimum of 2 lines
    // rather than 1.  This vastly improves scrolling speed.
    lineCount = 2;
#endif
    scrollFrame->ScrollBy(nsIntPoint(0, aForward ? lineCount : -lineCount),
                          nsIScrollableFrame::LINES,
                          nsIScrollableFrame::SMOOTH);
      
//NEW FOR LINES    
    // force the update to happen now, otherwise multiple scrolls can
    // occur before the update is processed. (bug #7354)

  // I'd use Composite here, but it doesn't always work.
    // vm->Composite();
    nsIViewManager* viewManager = GetViewManager();
    if (viewManager) {
      viewManager->ForceUpdate();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
PresShell::ScrollHorizontal(PRBool aLeft)
{
  nsIScrollableFrame* scrollFrame =
    GetFrameToScrollAsScrollable(nsIPresShell::eHorizontal);
  if (scrollFrame) {
    scrollFrame->ScrollBy(nsIntPoint(aLeft ? -1 : 1, 0),
                          nsIScrollableFrame::LINES,
                          nsIScrollableFrame::SMOOTH);
//NEW FOR LINES    
    // force the update to happen now, otherwise multiple scrolls can
    // occur before the update is processed. (bug #7354)

  // I'd use Composite here, but it doesn't always work.
    // vm->Composite();
    nsIViewManager* viewManager = GetViewManager();
    if (viewManager) {
      viewManager->ForceUpdate();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
PresShell::CompleteScroll(PRBool aForward)
{
  nsIScrollableFrame* scrollFrame =
    GetFrameToScrollAsScrollable(nsIPresShell::eVertical);
  if (scrollFrame) {
    scrollFrame->ScrollBy(nsIntPoint(0, aForward ? 1 : -1),
                          nsIScrollableFrame::WHOLE,
                          nsIScrollableFrame::INSTANT);
  }
  return NS_OK;
}

NS_IMETHODIMP
PresShell::CompleteMove(PRBool aForward, PRBool aExtend)
{
  // Beware! This may flush notifications via synchronous
  // ScrollSelectionIntoView.
  nsIContent* limiter = mSelection->GetAncestorLimiter();
  nsIFrame* frame = limiter ? limiter->GetPrimaryFrame()
                            : FrameConstructor()->GetRootElementFrame();
  if (!frame)
    return NS_ERROR_FAILURE;
  nsPeekOffsetStruct pos = frame->GetExtremeCaretPosition(!aForward);
  mSelection->HandleClick(pos.mResultContent, pos.mContentOffset,
                          pos.mContentOffset, aExtend, PR_FALSE, aForward);
  if (limiter) {
    // HandleClick resets ancestorLimiter, so set it again.
    mSelection->SetAncestorLimiter(limiter);
  }
    
  // After ScrollSelectionIntoView(), the pending notifications might be
  // flushed and PresShell/PresContext/Frames may be dead. See bug 418470.
  return ScrollSelectionIntoView(nsISelectionController::SELECTION_NORMAL, 
                                 nsISelectionController::SELECTION_FOCUS_REGION,
                                 PR_TRUE);
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
  nsIFrame *frame = content->GetPrimaryFrame();
  if (!frame) //no frame to look at so it must not be visible
    return NS_OK;  
  //start process now to go through all frames to find startOffset. then check chars after that to see 
  //if anything until EndOffset is visible.
  PRBool finished = PR_FALSE;
  frame->CheckVisibility(mPresContext,startOffset,EndOffset,PR_TRUE,&finished, _retval);
  return NS_OK;//dont worry about other return val
}

//end implementations nsISelectionController


void
PresShell::StyleChangeReflow()
{
  nsIFrame* rootFrame = FrameManager()->GetRootFrame();
  // At the moment at least, we don't have a root frame before the initial
  // reflow; it's safe to just ignore the request in that case
  if (!rootFrame)
    return;

  FrameNeedsReflow(rootFrame, eStyleChange, NS_FRAME_IS_DIRTY);
}

nsIFrame*
nsIPresShell::GetRootFrameExternal() const
{
  return FrameManager()->GetRootFrame();
}

nsIFrame*
nsIPresShell::GetRootScrollFrame() const
{
  nsIFrame* rootFrame = FrameManager()->GetRootFrame();
  // Ensure root frame is a viewport frame
  if (!rootFrame || nsGkAtoms::viewportFrame != rootFrame->GetType())
    return nsnull;
  nsIFrame* theFrame = rootFrame->GetFirstChild(nsnull);
  if (!theFrame || nsGkAtoms::scrollFrame != theFrame->GetType())
    return nsnull;
  return theFrame;
}

nsIScrollableFrame*
nsIPresShell::GetRootScrollFrameAsScrollable() const
{
  nsIFrame* frame = GetRootScrollFrame();
  if (!frame)
    return nsnull;
  nsIScrollableFrame* scrollableFrame = do_QueryFrame(frame);
  NS_ASSERTION(scrollableFrame,
               "All scroll frames must implement nsIScrollableFrame");
  return scrollableFrame;
}

nsIScrollableFrame*
nsIPresShell::GetRootScrollFrameAsScrollableExternal() const
{
  return GetRootScrollFrameAsScrollable();
}

nsIPageSequenceFrame*
PresShell::GetPageSequenceFrame() const
{
  nsIFrame* frame = mFrameConstructor->GetPageSequenceFrame();
  return do_QueryFrame(frame);
}

nsIFrame*
PresShell::GetFrameForPoint(nsIFrame* aFrame, nsPoint aPt)
{
  return nsLayoutUtils::GetFrameForPoint(aFrame, aPt);
}

void
PresShell::BeginUpdate(nsIDocument *aDocument, nsUpdateType aUpdateType)
{
#ifdef DEBUG
  mUpdateCount++;
#endif
  mFrameConstructor->BeginUpdate();

  if (aUpdateType & UPDATE_STYLE)
    mStyleSet->BeginUpdate();
}

void
PresShell::EndUpdate(nsIDocument *aDocument, nsUpdateType aUpdateType)
{
#ifdef DEBUG
  NS_PRECONDITION(0 != mUpdateCount, "too many EndUpdate's");
  --mUpdateCount;
#endif

  if (aUpdateType & UPDATE_STYLE) {
    mStyleSet->EndUpdate();
    if (mStylesHaveChanged)
      ReconstructStyleData();
  }

  mFrameConstructor->EndUpdate();
}

void
PresShell::RestoreRootScrollPosition()
{
  // Restore frame state for the root scroll frame
  nsCOMPtr<nsILayoutHistoryState> historyState =
    mDocument->GetLayoutHistoryState();
  // Make sure we don't reenter reflow via the sync paint that happens while
  // we're scrolling to our restored position.  Entering reflow for the
  // scrollable frame will cause it to reenter ScrollToRestoredPosition(), and
  // it'll get all confused.
  nsAutoScriptBlocker scriptBlocker;
  ++mChangeNestCount;

  if (historyState) {
    nsIFrame* scrollFrame = GetRootScrollFrame();
    if (scrollFrame) {
      nsIScrollableFrame* scrollableFrame = do_QueryFrame(scrollFrame);
      if (scrollableFrame) {
        FrameManager()->RestoreFrameStateFor(scrollFrame, historyState,
                                             nsIStatefulFrame::eDocumentScrollState);
        scrollableFrame->ScrollToRestoredPosition();
      }
    }
  }

  --mChangeNestCount;
}

void
PresShell::BeginLoad(nsIDocument *aDocument)
{  
  mDocumentLoading = PR_TRUE;
}

void
PresShell::EndLoad(nsIDocument *aDocument)
{
  NS_PRECONDITION(aDocument == mDocument, "Wrong document");
  
  RestoreRootScrollPosition();
  
  mDocumentLoading = PR_FALSE;
}

#ifdef DEBUG
void
PresShell::VerifyHasDirtyRootAncestor(nsIFrame* aFrame)
{
  // XXXbz due to bug 372769, can't actually assert anything here...
  return;
  
  // XXXbz shouldn't need this part; remove it once FrameNeedsReflow
  // handles the root frame correctly.
  if (!aFrame->GetParent()) {
    return;
  }
        
  // Make sure that there is a reflow root ancestor of |aFrame| that's
  // in mDirtyRoots already.
  while (aFrame && (aFrame->GetStateBits() & NS_FRAME_HAS_DIRTY_CHILDREN)) {
    if (((aFrame->GetStateBits() & NS_FRAME_REFLOW_ROOT) ||
         !aFrame->GetParent()) &&
        mDirtyRoots.Contains(aFrame)) {
      return;
    }

    aFrame = aFrame->GetParent();
  }
  NS_NOTREACHED("Frame has dirty bits set but isn't scheduled to be "
                "reflowed?");
}
#endif

void
PresShell::FrameNeedsReflow(nsIFrame *aFrame, IntrinsicDirty aIntrinsicDirty,
                            nsFrameState aBitToAdd)
{
#ifdef NS_FUNCTION_TIMER
  NS_TIME_FUNCTION_DECLARE_DOCURL;
  nsCAutoString frameType__("N/A");
  nsIAtom *atomType__ = aFrame ? aFrame->GetType() : nsnull;
  if (atomType__) atomType__->ToUTF8String(frameType__);
  NS_TIME_FUNCTION_MIN_FMT(1.0, "%s (line %d) (document: %s, frame type: %s)", MOZ_FUNCTION_NAME,
                           __LINE__, docURL__.get(), frameType__.get());
#endif

  NS_PRECONDITION(aBitToAdd == NS_FRAME_IS_DIRTY ||
                  aBitToAdd == NS_FRAME_HAS_DIRTY_CHILDREN,
                  "Unexpected bits being added");
  NS_PRECONDITION(aIntrinsicDirty != eStyleChange ||
                  aBitToAdd == NS_FRAME_IS_DIRTY,
                  "bits don't correspond to style change reason");

  NS_ASSERTION(!mIsReflowing, "can't mark frame dirty during reflow");

  // If we've not yet done the initial reflow, then don't bother
  // enqueuing a reflow command yet.
  if (! mDidInitialReflow)
    return;

  // If we're already destroying, don't bother with this either.
  if (mIsDestroying)
    return;

#ifdef DEBUG
  //printf("gShellCounter: %d\n", gShellCounter++);
  if (mInVerifyReflow)
    return;

  if (VERIFY_REFLOW_NOISY_RC & gVerifyReflowFlags) {
    printf("\nPresShell@%p: frame %p needs reflow\n", (void*)this, (void*)aFrame);
    if (VERIFY_REFLOW_REALLY_NOISY_RC & gVerifyReflowFlags) {
      printf("Current content model:\n");
      Element *rootElement = mDocument->GetRootElement();
      if (rootElement) {
        rootElement->List(stdout, 0);
      }
    }
  }  
#endif

  nsAutoTArray<nsIFrame*, 4> subtrees;
  subtrees.AppendElement(aFrame);

  do {
    nsIFrame *subtreeRoot = subtrees.ElementAt(subtrees.Length() - 1);
    subtrees.RemoveElementAt(subtrees.Length() - 1);

    // Grab |wasDirty| now so we can go ahead and update the bits on
    // subtreeRoot.
    PRBool wasDirty = NS_SUBTREE_DIRTY(subtreeRoot);
    subtreeRoot->AddStateBits(aBitToAdd);

    // Now if subtreeRoot is a reflow root we can cut off this reflow at it if
    // the bit being added is NS_FRAME_HAS_DIRTY_CHILDREN.
    PRBool targetFrameDirty = (aBitToAdd == NS_FRAME_IS_DIRTY);

#define FRAME_IS_REFLOW_ROOT(_f)                   \
  ((_f->GetStateBits() & NS_FRAME_REFLOW_ROOT) &&  \
   (_f != subtreeRoot || !targetFrameDirty))


    // Mark the intrinsic widths as dirty on the frame, all of its ancestors,
    // and all of its descendants, if needed:

    if (aIntrinsicDirty != eResize) {
      // Mark argument and all ancestors dirty. (Unless we hit a reflow
      // root that should contain the reflow.  That root could be
      // subtreeRoot itself if it's not dirty, or it could be some
      // ancestor of subtreeRoot.)
      for (nsIFrame *a = subtreeRoot;
           a && !FRAME_IS_REFLOW_ROOT(a);
           a = a->GetParent())
        a->MarkIntrinsicWidthsDirty();
    }

    if (aIntrinsicDirty == eStyleChange) {
      // Mark all descendants dirty (using an nsTArray stack rather than
      // recursion).
      nsAutoTArray<nsIFrame*, 32> stack;
      stack.AppendElement(subtreeRoot);

      do {
        nsIFrame *f = stack.ElementAt(stack.Length() - 1);
        stack.RemoveElementAt(stack.Length() - 1);

        if (f->GetType() == nsGkAtoms::placeholderFrame) {
          nsIFrame *oof = nsPlaceholderFrame::GetRealFrameForPlaceholder(f);
          if (!nsLayoutUtils::IsProperAncestorFrame(subtreeRoot, oof)) {
            // We have another distinct subtree we need to mark.
            subtrees.AppendElement(oof);
          }
        }

        PRInt32 childListIndex = 0;
        nsIAtom *childListName;
        do {
          childListName = f->GetAdditionalChildListName(childListIndex++);
          for (nsIFrame *kid = f->GetFirstChild(childListName); kid;
               kid = kid->GetNextSibling()) {
            kid->MarkIntrinsicWidthsDirty();
            stack.AppendElement(kid);
          }
        } while (childListName);
      } while (stack.Length() != 0);
    }

    // Set NS_FRAME_HAS_DIRTY_CHILDREN bits (via nsIFrame::ChildIsDirty)
    // up the tree until we reach either a frame that's already dirty or
    // a reflow root.
    nsIFrame *f = subtreeRoot;
    for (;;) {
      if (FRAME_IS_REFLOW_ROOT(f) || !f->GetParent()) {
        // we've hit a reflow root or the root frame
        if (!wasDirty) {
          mDirtyRoots.AppendElement(f);
        }
#ifdef DEBUG
        else {
          VerifyHasDirtyRootAncestor(f);
        }
#endif
        
        break;
      }

      nsIFrame *child = f;
      f = f->GetParent();
      wasDirty = NS_SUBTREE_DIRTY(f);
      f->ChildIsDirty(child);
      NS_ASSERTION(f->GetStateBits() & NS_FRAME_HAS_DIRTY_CHILDREN,
                   "ChildIsDirty didn't do its job");
      if (wasDirty) {
        // This frame was already marked dirty.
#ifdef DEBUG
        VerifyHasDirtyRootAncestor(f);
#endif
        break;
      }
    }
  } while (subtrees.Length() != 0);

  MaybeScheduleReflow();
}

void
PresShell::FrameNeedsToContinueReflow(nsIFrame *aFrame)
{
  NS_ASSERTION(mIsReflowing, "Must be in reflow when marking path dirty.");  
  NS_PRECONDITION(mCurrentReflowRoot, "Must have a current reflow root here");
  NS_ASSERTION(aFrame == mCurrentReflowRoot ||
               nsLayoutUtils::IsProperAncestorFrame(mCurrentReflowRoot, aFrame),
               "Frame passed in is not the descendant of mCurrentReflowRoot");
  NS_ASSERTION(aFrame->GetStateBits() & NS_FRAME_IN_REFLOW,
               "Frame passed in not in reflow?");

  mFramesToDirty.PutEntry(aFrame);
}

nsIScrollableFrame*
nsIPresShell::GetFrameToScrollAsScrollable(
                nsIPresShell::ScrollDirection aDirection)
{
  nsIScrollableFrame* scrollFrame = nsnull;

  nsCOMPtr<nsIContent> focusedContent;
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm && mDocument) {
    nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(mDocument->GetWindow());

    nsCOMPtr<nsIDOMElement> focusedElement;
    fm->GetFocusedElementForWindow(window, PR_FALSE, nsnull, getter_AddRefs(focusedElement));
    focusedContent = do_QueryInterface(focusedElement);
  }
  if (!focusedContent && mSelection) {
    nsISelection* domSelection = mSelection->
      GetSelection(nsISelectionController::SELECTION_NORMAL);
    if (domSelection) {
      nsCOMPtr<nsIDOMNode> focusedNode;
      domSelection->GetFocusNode(getter_AddRefs(focusedNode));
      focusedContent = do_QueryInterface(focusedNode);
    }
  }
  if (focusedContent) {
    nsIFrame* startFrame = focusedContent->GetPrimaryFrame();
    if (startFrame) {
      scrollFrame = startFrame->GetScrollTargetFrame();
      if (scrollFrame) {
        startFrame = scrollFrame->GetScrolledFrame();
      }
      if (aDirection == nsIPresShell::eEither) {
        scrollFrame =
          nsLayoutUtils::GetNearestScrollableFrame(startFrame);
      } else {
        scrollFrame =
          nsLayoutUtils::GetNearestScrollableFrameForDirection(startFrame,
            aDirection == eVertical ? nsLayoutUtils::eVertical :
                                      nsLayoutUtils::eHorizontal);
      }
    }
  }
  if (!scrollFrame) {
    scrollFrame = GetRootScrollFrameAsScrollable();
  }
  return scrollFrame;
}

void
PresShell::CancelAllPendingReflows()
{
  mDirtyRoots.Clear();

  if (mReflowScheduled) {
    GetPresContext()->RefreshDriver()->RemoveLayoutFlushObserver(this);
    mReflowScheduled = PR_FALSE;
  }

  ASSERT_REFLOW_SCHEDULED_STATE();
}

#ifdef ACCESSIBILITY
void nsIPresShell::InvalidateAccessibleSubtree(nsIContent *aContent)
{
  if (gIsAccessibilityActive) {
    nsCOMPtr<nsIAccessibilityService> accService = 
      do_GetService("@mozilla.org/accessibilityService;1");
    if (accService) {
      accService->InvalidateSubtreeFor(this, aContent,
                                       nsIAccessibilityService::FRAME_SIGNIFICANT_CHANGE);
    }
  }
}
#endif

nsresult
PresShell::RecreateFramesFor(nsIContent* aContent)
{
  NS_TIME_FUNCTION_MIN(1.0);

  NS_ENSURE_TRUE(mPresContext, NS_ERROR_FAILURE);
  if (!mDidInitialReflow) {
    // Nothing to do here.  In fact, if we proceed and aContent is the
    // root we will crash.
    return NS_OK;
  }

  // Don't call RecreateFramesForContent since that is not exported and we want
  // to keep the number of entrypoints down.

  NS_ASSERTION(mViewManager, "Should have view manager");
  nsIViewManager::UpdateViewBatch batch(mViewManager);

  // Have to make sure that the content notifications are flushed before we
  // start messing with the frame model; otherwise we can get content doubling.
  mDocument->FlushPendingNotifications(Flush_ContentAndNotify);

  nsAutoScriptBlocker scriptBlocker;

  nsStyleChangeList changeList;
  changeList.AppendChange(nsnull, aContent, nsChangeHint_ReconstructFrame);

  // Mark ourselves as not safe to flush while we're doing frame construction.
  ++mChangeNestCount;
  nsresult rv = mFrameConstructor->ProcessRestyledFrames(changeList);
  --mChangeNestCount;
  
  batch.EndUpdateViewBatch(NS_VMREFRESH_NO_SYNC);
#ifdef ACCESSIBILITY
  InvalidateAccessibleSubtree(aContent);
#endif
  return rv;
}

void
nsIPresShell::PostRecreateFramesFor(Element* aElement)
{
  FrameConstructor()->PostRestyleEvent(aElement, nsRestyleHint(0),
                                       nsChangeHint_ReconstructFrame);
}

void
nsIPresShell::RestyleForAnimation(Element* aElement, nsRestyleHint aHint)
{
  FrameConstructor()->PostAnimationRestyleEvent(aElement, aHint,
                                                NS_STYLE_HINT_NONE);
}

void
PresShell::ClearFrameRefs(nsIFrame* aFrame)
{
  mPresContext->EventStateManager()->ClearFrameRefs(aFrame);

  nsWeakFrame* weakFrame = mWeakFrames;
  while (weakFrame) {
    nsWeakFrame* prev = weakFrame->GetPreviousWeakFrame();
    if (weakFrame->GetFrame() == aFrame) {
      // This removes weakFrame from mWeakFrames.
      weakFrame->Clear(this);
    }
    weakFrame = prev;
  }
}

already_AddRefed<nsIRenderingContext>
PresShell::GetReferenceRenderingContext()
{
  NS_TIME_FUNCTION_MIN(1.0);

  nsIDeviceContext* devCtx = mPresContext->DeviceContext();
  nsRefPtr<nsIRenderingContext> rc;
  if (mPresContext->IsScreen()) {
    devCtx->CreateRenderingContextInstance(*getter_AddRefs(rc));
    if (rc) {
      rc->Init(devCtx, gfxPlatform::GetPlatform()->ScreenReferenceSurface());
    }
  } else {
    devCtx->CreateRenderingContext(*getter_AddRefs(rc));
  }
  return rc.forget();
}

nsresult
PresShell::GoToAnchor(const nsAString& aAnchorName, PRBool aScroll)
{
  if (!mDocument) {
    return NS_ERROR_FAILURE;
  }
  
  // Hold a reference to the ESM in case event dispatch tears us down.
  nsCOMPtr<nsIEventStateManager> esm = mPresContext->EventStateManager();

  if (aAnchorName.IsEmpty()) {
    NS_ASSERTION(!aScroll, "can't scroll to empty anchor name");
    esm->SetContentState(nsnull, NS_EVENT_STATE_URLTARGET);
    return NS_OK;
  }

  nsCOMPtr<nsIDOMHTMLDocument> htmlDoc = do_QueryInterface(mDocument);
  nsresult rv = NS_OK;
  nsCOMPtr<nsIContent> content;

  // Search for an element with a matching "id" attribute
  if (mDocument) {    
    content = mDocument->GetElementById(aAnchorName);
  }

  // Search for an anchor element with a matching "name" attribute
  if (!content && htmlDoc) {
    nsCOMPtr<nsIDOMNodeList> list;
    // Find a matching list of named nodes
    rv = htmlDoc->GetElementsByName(aAnchorName, getter_AddRefs(list));
    if (NS_SUCCEEDED(rv) && list) {
      PRUint32 i;
      // Loop through the named nodes looking for the first anchor
      for (i = 0; PR_TRUE; i++) {
        nsCOMPtr<nsIDOMNode> node;
        rv = list->Item(i, getter_AddRefs(node));
        if (!node) {  // End of list
          break;
        }
        // Ensure it's an anchor element
        content = do_QueryInterface(node);
        if (content) {
          if (content->Tag() == nsGkAtoms::a && content->IsHTML()) {
            break;
          }
          content = nsnull;
        }
      }
    }
  }

  // Search for anchor in the HTML namespace with a matching name
  if (!content && !htmlDoc)
  {
    nsCOMPtr<nsIDOMDocument> doc = do_QueryInterface(mDocument);
    nsCOMPtr<nsIDOMNodeList> list;
    NS_NAMED_LITERAL_STRING(nameSpace, "http://www.w3.org/1999/xhtml");
    // Get the list of anchor elements
    rv = doc->GetElementsByTagNameNS(nameSpace, NS_LITERAL_STRING("a"), getter_AddRefs(list));
    if (NS_SUCCEEDED(rv) && list) {
      PRUint32 i;
      // Loop through the named nodes looking for the first anchor
      for (i = 0; PR_TRUE; i++) {
        nsCOMPtr<nsIDOMNode> node;
        rv = list->Item(i, getter_AddRefs(node));
        if (!node) { // End of list
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

  nsCOMPtr<nsIDOMRange> jumpToRange;
  nsCOMPtr<nsIXPointerResult> xpointerResult;
  if (!content) {
    nsCOMPtr<nsIDOMXMLDocument> xmldoc = do_QueryInterface(mDocument);
    if (xmldoc) {
      // Try XPointer
      xmldoc->EvaluateXPointer(aAnchorName, getter_AddRefs(xpointerResult));
      if (xpointerResult) {
        xpointerResult->Item(0, getter_AddRefs(jumpToRange));
        if (!jumpToRange) {
          // We know it was an XPointer, so there is no point in
          // trying any other pointer types, let's just return
          // an error.
          return NS_ERROR_FAILURE;
        }
      }

      // Finally try FIXptr
      if (!jumpToRange) {
        xmldoc->EvaluateFIXptr(aAnchorName,getter_AddRefs(jumpToRange));
      }

      if (jumpToRange) {
        nsCOMPtr<nsIDOMNode> node;
        jumpToRange->GetStartContainer(getter_AddRefs(node));
        if (node) {
          PRUint16 nodeType;
          node->GetNodeType(&nodeType);
          PRInt32 offset = -1;
          jumpToRange->GetStartOffset(&offset);
          switch (nodeType) {
            case nsIDOMNode::ATTRIBUTE_NODE:
            {
              // XXX Assuming jumping to the ownerElement is the sanest action.
              nsCOMPtr<nsIAttribute> attr = do_QueryInterface(node);
              content = attr->GetContent();
              break;
            }
            case nsIDOMNode::DOCUMENT_NODE:
            {
              if (offset >= 0) {
                nsCOMPtr<nsIDocument> document = do_QueryInterface(node);
                content = document->GetChildAt(offset);
              }
              break;
            }
            case nsIDOMNode::DOCUMENT_FRAGMENT_NODE:
            case nsIDOMNode::ELEMENT_NODE:
            case nsIDOMNode::ENTITY_REFERENCE_NODE:
            {
              if (offset >= 0) {
                nsCOMPtr<nsIContent> parent = do_QueryInterface(node);
                content = parent->GetChildAt(offset);
              }
              break;
            }
            case nsIDOMNode::CDATA_SECTION_NODE:
            case nsIDOMNode::COMMENT_NODE:
            case nsIDOMNode::TEXT_NODE:
            case nsIDOMNode::PROCESSING_INSTRUCTION_NODE:
            {
              // XXX This should scroll to a specific position in the text.
              content = do_QueryInterface(node);
              break;
            }
          }
        }
      }
    }
  }

  esm->SetContentState(content, NS_EVENT_STATE_URLTARGET);

#ifdef ACCESSIBILITY
  nsIContent *anchorTarget = content;
#endif

  if (content) {
    if (aScroll) {
      rv = ScrollContentIntoView(content, NS_PRESSHELL_SCROLL_TOP,
                                 NS_PRESSHELL_SCROLL_ANYWHERE);
      NS_ENSURE_SUCCESS(rv, rv);

      nsIScrollableFrame* rootScroll = GetRootScrollFrameAsScrollable();
      if (rootScroll) {
        mLastAnchorScrolledTo = content;
        mLastAnchorScrollPositionY = rootScroll->GetScrollPosition().y;
      }
    }

    // Should we select the target? This action is controlled by a
    // preference: the default is to not select.
    PRBool selectAnchor = nsContentUtils::GetBoolPref("layout.selectanchor");

    // Even if select anchor pref is false, we must still move the
    // caret there. That way tabbing will start from the new
    // location
    if (!jumpToRange) {
      jumpToRange = do_CreateInstance(kRangeCID);
      if (jumpToRange) {
        while (content && content->GetChildCount() > 0) {
          content = content->GetChildAt(0);
        }
        nsCOMPtr<nsIDOMNode> node(do_QueryInterface(content));
        NS_ASSERTION(node, "No nsIDOMNode for descendant of anchor");
        jumpToRange->SelectNodeContents(node);
      }
    }
    if (jumpToRange) {
      // Select the anchor
      nsISelection* sel = mSelection->
        GetSelection(nsISelectionController::SELECTION_NORMAL);
      if (sel) {
        sel->RemoveAllRanges();
        sel->AddRange(jumpToRange);
        if (!selectAnchor) {
          // Use a caret (collapsed selection) at the start of the anchor
          sel->CollapseToStart();
        }
      }  

      if (selectAnchor && xpointerResult) {
        // Select the rest (if any) of the ranges in XPointerResult
        PRUint32 count, i;
        xpointerResult->GetLength(&count);
        for (i = 1; i < count; i++) { // jumpToRange is i = 0
          nsCOMPtr<nsIDOMRange> range;
          xpointerResult->Item(i, getter_AddRefs(range));
          sel->AddRange(range);
        }
      }
      // Selection is at anchor.
      // Now focus the document itself if focus is on an element within it.
      nsPIDOMWindow *win = mDocument->GetWindow();

      nsIFocusManager* fm = nsFocusManager::GetFocusManager();
      if (fm && win) {
        nsCOMPtr<nsIDOMWindow> focusedWindow;
        fm->GetFocusedWindow(getter_AddRefs(focusedWindow));
        if (SameCOMIdentity(win, focusedWindow))
          fm->ClearFocus(focusedWindow);
      }
    }
  } else {
    rv = NS_ERROR_FAILURE; //changed to NS_OK in quirks mode if ScrollTo is called

    // Scroll to the top/left if the anchor can not be
    // found and it is labelled top (quirks mode only). @see bug 80784
    if ((NS_LossyConvertUTF16toASCII(aAnchorName).LowerCaseEqualsLiteral("top")) &&
        (mPresContext->CompatibilityMode() == eCompatibility_NavQuirks)) {
      rv = NS_OK;
      nsIScrollableFrame* sf = GetRootScrollFrameAsScrollable();
      // Check |aScroll| after setting |rv| so we set |rv| to the same
      // thing whether or not |aScroll| is true.
      if (aScroll && sf) {
        // Scroll to the top of the page
        sf->ScrollTo(nsPoint(0, 0), nsIScrollableFrame::INSTANT);
      }
    }
  }

#ifdef ACCESSIBILITY
  if (anchorTarget && gIsAccessibilityActive) {
    nsCOMPtr<nsIAccessibilityService> accService = 
      do_GetService("@mozilla.org/accessibilityService;1");
    if (accService)
      accService->NotifyOfAnchorJumpTo(anchorTarget);
  }
#endif

  return rv;
}

nsresult
PresShell::ScrollToAnchor()
{
  if (!mLastAnchorScrolledTo)
    return NS_OK;

  NS_ASSERTION(mDidInitialReflow, "should have done initial reflow by now");

  nsIScrollableFrame* rootScroll = GetRootScrollFrameAsScrollable();
  if (!rootScroll ||
      mLastAnchorScrollPositionY != rootScroll->GetScrollPosition().y)
    return NS_OK;

  nsresult rv = ScrollContentIntoView(mLastAnchorScrolledTo, NS_PRESSHELL_SCROLL_TOP,
                                      NS_PRESSHELL_SCROLL_ANYWHERE);
  mLastAnchorScrolledTo = nsnull;
  return rv;
}

/*
 * Helper (per-continuation) for ScrollContentIntoView.
 *
 * @param aContainerFrame [in] the frame which aRect is relative to
 * @param aFrame [in] Frame whose bounds should be unioned
 * @param aUseWholeLineHeightForInlines [in] if true, then for inline frames
 * we should include the top of the line in the added rectangle
 * @param aRect [inout] rect into which its bounds should be unioned
 * @param aHaveRect [inout] whether aRect contains data yet
 */
static void
AccumulateFrameBounds(nsIFrame* aContainerFrame,
                      nsIFrame* aFrame,
                      PRBool aUseWholeLineHeightForInlines,
                      nsRect& aRect,
                      PRBool& aHaveRect)
{
  nsRect frameBounds = aFrame->GetRect() +
    aFrame->GetParent()->GetOffsetTo(aContainerFrame);

  // If this is an inline frame and either the bounds height is 0 (quirks
  // layout model) or aUseWholeLineHeightForInlines is set, we need to
  // change the top of the bounds to include the whole line.
  if (frameBounds.height == 0 || aUseWholeLineHeightForInlines) {
    nsIAtom* frameType = NULL;
    nsIFrame *prevFrame = aFrame;
    nsIFrame *f = aFrame;

    while (f &&
           (frameType = f->GetType()) == nsGkAtoms::inlineFrame) {
      prevFrame = f;
      f = prevFrame->GetParent();
    }

    if (f != aFrame &&
        f &&
        frameType == nsGkAtoms::blockFrame) {
      // find the line containing aFrame and increase the top of |offset|.
      nsAutoLineIterator lines = f->GetLineIterator();
      if (lines) {
        PRInt32 index = lines->FindLineContaining(prevFrame);
        if (index >= 0) {
          nsIFrame *trash1;
          PRInt32 trash2;
          nsRect lineBounds;
          PRUint32 trash3;

          if (NS_SUCCEEDED(lines->GetLine(index, &trash1, &trash2,
                                          lineBounds, &trash3))) {
            lineBounds += f->GetOffsetTo(aContainerFrame);
            if (lineBounds.y < frameBounds.y) {
              frameBounds.height = frameBounds.YMost() - lineBounds.y;
              frameBounds.y = lineBounds.y;
            }
          }
        }
      }
    }
  }

  if (aHaveRect) {
    // We can't use nsRect::UnionRect since it drops empty rects on
    // the floor, and we need to include them.  (Thus we need
    // aHaveRect to know when to drop the initial value on the floor.)
    aRect.UnionRectIncludeEmpty(aRect, frameBounds);
  } else {
    aHaveRect = PR_TRUE;
    aRect = frameBounds;
  }
}

/**
 * This function takes a scrollable frame, a rect in the coordinate system
 * of the scrolled frame, and a desired percentage-based scroll
 * position and attempts to scroll the rect to that position in the
 * scrollport.
 * 
 * This needs to work even if aRect has a width or height of zero.
 */
static void ScrollToShowRect(nsIScrollableFrame* aScrollFrame,
                             const nsRect&       aRect,
                             PRIntn              aVPercent,
                             PRIntn              aHPercent,
                             PRUint32            aFlags)
{
  nsPoint scrollPt = aScrollFrame->GetScrollPosition();
  nsRect visibleRect(scrollPt, aScrollFrame->GetScrollPortRect().Size());
  nsSize lineSize = aScrollFrame->GetLineScrollAmount();
  nsPresContext::ScrollbarStyles ss = aScrollFrame->GetScrollbarStyles();

  if ((aFlags & nsIPresShell::SCROLL_OVERFLOW_HIDDEN) ||
      ss.mVertical != NS_STYLE_OVERFLOW_HIDDEN) {
    // See how the rect should be positioned vertically
    if (NS_PRESSHELL_SCROLL_ANYWHERE == aVPercent ||
        (NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE == aVPercent &&
         aRect.height < lineSize.height)) {
      // The caller doesn't care where the frame is positioned vertically,
      // so long as it's fully visible
      if (aRect.y < visibleRect.y) {
        // Scroll up so the frame's top edge is visible
        scrollPt.y = aRect.y;
      } else if (aRect.YMost() > visibleRect.YMost()) {
        // Scroll down so the frame's bottom edge is visible. Make sure the
        // frame's top edge is still visible
        scrollPt.y += aRect.YMost() - visibleRect.YMost();
        if (scrollPt.y > aRect.y) {
          scrollPt.y = aRect.y;
        }
      }
    } else if (NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE == aVPercent) {
      // Scroll only if no part of the frame is visible in this view
      if (aRect.YMost() - lineSize.height < visibleRect.y) {
        // Scroll up so the frame's top edge is visible
        scrollPt.y = aRect.y;
      }  else if (aRect.y + lineSize.height > visibleRect.YMost()) {
        // Scroll down so the frame's bottom edge is visible. Make sure the
        // frame's top edge is still visible
        scrollPt.y += aRect.YMost() - visibleRect.YMost();
        if (scrollPt.y > aRect.y) {
          scrollPt.y = aRect.y;
        }
      }
    } else {
      // Align the frame edge according to the specified percentage
      nscoord frameAlignY =
        NSToCoordRound(aRect.y + aRect.height * (aVPercent / 100.0f));
      scrollPt.y =
        NSToCoordRound(frameAlignY - visibleRect.height * (aVPercent / 100.0f));
    }
  }

  if ((aFlags & nsIPresShell::SCROLL_OVERFLOW_HIDDEN) ||
      ss.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN) {
    // See how the frame should be positioned horizontally
    if (NS_PRESSHELL_SCROLL_ANYWHERE == aHPercent ||
        (NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE == aHPercent &&
         aRect.width < lineSize.width)) {
      // The caller doesn't care where the frame is positioned horizontally,
      // so long as it's fully visible
      if (aRect.x < visibleRect.x) {
        // Scroll left so the frame's left edge is visible
        scrollPt.x = aRect.x;
      } else if (aRect.XMost() > visibleRect.XMost()) {
        // Scroll right so the frame's right edge is visible. Make sure the
        // frame's left edge is still visible
        scrollPt.x += aRect.XMost() - visibleRect.XMost();
        if (scrollPt.x > aRect.x) {
          scrollPt.x = aRect.x;
        }
      }
    } else if (NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE == aHPercent) {
      // Scroll only if no part of the frame is visible in this view
      if (aRect.XMost() - lineSize.width < visibleRect.x) {
        // Scroll left so the frame's left edge is visible
        scrollPt.x = aRect.x;
      }  else if (aRect.x + lineSize.width > visibleRect.XMost()) {
        // Scroll right so the frame's right edge is visible. Make sure the
        // frame's left edge is still visible
        scrollPt.x += aRect.XMost() - visibleRect.XMost();
        if (scrollPt.x > aRect.x) {
          scrollPt.x = aRect.x;
        }
      }
    } else {
      // Align the frame edge according to the specified percentage
      nscoord frameAlignX =
        NSToCoordRound(aRect.x + (aRect.width) * (aHPercent / 100.0f));
      scrollPt.x =
        NSToCoordRound(frameAlignX - visibleRect.width * (aHPercent / 100.0f));
    }
  }

  aScrollFrame->ScrollTo(scrollPt, nsIScrollableFrame::INSTANT);
}

nsresult
PresShell::ScrollContentIntoView(nsIContent* aContent,
                                 PRIntn      aVPercent,
                                 PRIntn      aHPercent)
{
  nsCOMPtr<nsIContent> content = aContent; // Keep content alive while flushing.
  NS_ENSURE_TRUE(content, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIDocument> currentDoc = content->GetCurrentDoc();
  NS_ENSURE_STATE(currentDoc);

  NS_ASSERTION(mDidInitialReflow, "should have done initial reflow by now");

  mContentToScrollTo = aContent;
  mContentScrollVPosition = aVPercent;
  mContentScrollHPosition = aHPercent;

  // Flush layout and attempt to scroll in the process.
  currentDoc->FlushPendingNotifications(Flush_InterruptibleLayout);

  // If mContentToScrollTo is non-null, that means we interrupted the reflow
  // (or suppressed it altogether because we're suppressing interruptible
  // flushes right now) and won't necessarily get the position correct, but do
  // a best-effort scroll here.  The other option would be to do this inside
  // FlushPendingNotifications, but I'm not sure the repeated scrolling that
  // could trigger if reflows keep getting interrupted would be more desirable
  // than a single best-effort scroll followed by one final scroll on the first
  // completed reflow.
  if (mContentToScrollTo) {
    DoScrollContentIntoView(content, aVPercent, aHPercent);
  }
  return NS_OK;
}

void
PresShell::DoScrollContentIntoView(nsIContent* aContent,
                                   PRIntn      aVPercent,
                                   PRIntn      aHPercent)
{
  NS_ASSERTION(mDidInitialReflow, "should have done initial reflow by now");

  nsIFrame* frame = aContent->GetPrimaryFrame();
  if (!frame) {
    mContentToScrollTo = nsnull;
    return;
  }

  if (frame->GetStateBits() & NS_FRAME_FIRST_REFLOW) {
    // The reflow flush before this scroll got interrupted, and this frame's
    // coords and size are all zero, and it has no content showing anyway.
    // Don't bother scrolling to it.  We'll try again when we finish up layout.
    return;
  }

  nsIFrame* container =
    nsLayoutUtils::GetClosestFrameOfType(frame, nsGkAtoms::scrollFrame);
  if (!container) {
    // nothing can be scrolled
    return;
  }

  // This is a two-step process.
  // Step 1: Find the bounds of the rect we want to scroll into view.  For
  //         example, for an inline frame we may want to scroll in the whole
  //         line, or we may want to scroll multiple lines into view.
  // Step 2: Walk container frame and its ancestors and scroll them
  //         appropriately.
  // frameBounds is relative to container. We're assuming
  // that scrollframes don't split so every continuation of frame will
  // be a descendant of container. (Things would still mostly work
  // even if that assumption was false.)
  nsRect frameBounds;
  PRBool haveRect = PR_FALSE;
  PRBool useWholeLineHeightForInlines = aVPercent != NS_PRESSHELL_SCROLL_ANYWHERE;
  do {
    AccumulateFrameBounds(container, frame, useWholeLineHeightForInlines,
                          frameBounds, haveRect);
  } while ((frame = frame->GetNextContinuation()));

  ScrollFrameRectIntoView(container, frameBounds, aVPercent, aHPercent,
                          SCROLL_OVERFLOW_HIDDEN);
}

PRBool
PresShell::ScrollFrameRectIntoView(nsIFrame*     aFrame,
                                   const nsRect& aRect,
                                   PRIntn        aVPercent,
                                   PRIntn        aHPercent,
                                   PRUint32      aFlags)
{
  PRBool didScroll = PR_FALSE;
  // This function needs to work even if rect has a width or height of 0.
  nsRect rect = aRect;
  nsIFrame* container = aFrame;
  // Walk up the frame hierarchy scrolling the rect into view and
  // keeping rect relative to container
  do {
    nsIScrollableFrame* sf = do_QueryFrame(container);
    if (sf) {
      nsPoint oldPosition = sf->GetScrollPosition();
      ScrollToShowRect(sf, rect - sf->GetScrolledFrame()->GetPosition(),
                       aVPercent, aHPercent, aFlags);
      nsPoint newPosition = sf->GetScrollPosition();
      // If the scroll position increased, that means our content moved up,
      // so our rect's offset should decrease
      rect += oldPosition - newPosition;

      if (oldPosition != newPosition) {
        didScroll = PR_TRUE;
      }

      // only scroll one container when this flag is set
      if (aFlags & SCROLL_FIRST_ANCESTOR_ONLY) {
        break;
      }

      nsRect scrollPort = sf->GetScrollPortRect();
      if (rect.XMost() < scrollPort.x ||
          rect.x > scrollPort.XMost() ||
          rect.YMost() < scrollPort.y ||
          rect.y > scrollPort.YMost()) {
        // We tried to show the rectangle, but none of it is visible,
        // not even an edge.
        // Stop trying to scroll ancestors into view.
        break;
      }

      // Restrict rect to the area that is actually visible through
      // the scrollport. We don't want to try to scroll some clipped-out
      // part of 'rect' into view in some ancestor.
      rect.IntersectRect(rect, sf->GetScrollPortRect());
    }
    rect += container->GetPosition();
    nsIFrame* parent = container->GetParent();
    if (!parent) {
      nsPoint extraOffset(0,0);
      parent = nsLayoutUtils::GetCrossDocParentFrame(container, &extraOffset);
      if (parent) {
        PRInt32 APD = container->PresContext()->AppUnitsPerDevPixel();        
        PRInt32 parentAPD = parent->PresContext()->AppUnitsPerDevPixel();
        rect = rect.ConvertAppUnitsRoundOut(APD, parentAPD);
        rect += extraOffset;
      }
    }
    container = parent;
  } while (container);

  return didScroll;
}

nsRectVisibility
PresShell::GetRectVisibility(nsIFrame* aFrame,
                             const nsRect &aRect,
                             nscoord aMinTwips) const
{
  NS_ASSERTION(aFrame->PresContext() == GetPresContext(),
               "prescontext mismatch?");
  nsIFrame* rootFrame = FrameManager()->GetRootFrame();
  NS_ASSERTION(rootFrame,
               "How can someone have a frame for this presshell when there's no root?");
  nsIScrollableFrame* sf = GetRootScrollFrameAsScrollable();
  nsRect scrollPortRect;
  if (sf) {
    scrollPortRect = sf->GetScrollPortRect();
    nsIFrame* f = do_QueryFrame(sf);
    scrollPortRect += f->GetOffsetTo(rootFrame);
  } else {
    scrollPortRect = nsRect(nsPoint(0,0), rootFrame->GetSize());
  }

  nsRect r = aRect + aFrame->GetOffsetTo(rootFrame);
  // If aRect is entirely visible then we don't need to ensure that
  // at least aMinTwips of it is visible
  if (scrollPortRect.Contains(r))
    return nsRectVisibility_kVisible;

  nsRect insetRect = scrollPortRect;
  insetRect.Deflate(aMinTwips, aMinTwips);
  if (r.YMost() <= insetRect.y)
    return nsRectVisibility_kAboveViewport;
  if (r.y >= insetRect.YMost())
    return nsRectVisibility_kBelowViewport;
  if (r.XMost() <= insetRect.x)
    return nsRectVisibility_kLeftOfViewport;
  if (r.x >= insetRect.XMost())
    return nsRectVisibility_kRightOfViewport;

  return nsRectVisibility_kVisible;
}

// GetLinkLocation: copy link location to clipboard
nsresult PresShell::GetLinkLocation(nsIDOMNode* aNode, nsAString& aLocationString) const
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
          if (xlinkType.EqualsLiteral("simple")) {
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
              rv = ios->NewURI(NS_ConvertUTF16toUTF8(base),nsnull,nsnull,getter_AddRefs(baseURI));
              NS_ENSURE_SUCCESS(rv, rv);

              nsCAutoString spec;
              rv = baseURI->Resolve(NS_ConvertUTF16toUTF8(anchorText),spec);
              NS_ENSURE_SUCCESS(rv, rv);

              CopyUTF8toUTF16(spec, anchorText);
            }
          }
        }
      }
    }
  }

  if (anchor || area || link || xlinkType.EqualsLiteral("simple")) {
    //Remove all the '\t', '\r' and '\n' from 'anchorText'
    anchorText.StripChars(strippedChars);

    aLocationString = anchorText;

    return NS_OK;
  }

  // if no link, fail.
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP_(void)
PresShell::DispatchSynthMouseMove(nsGUIEvent *aEvent,
                                  PRBool aFlushOnHoverChange)
{
  PRUint32 hoverGenerationBefore = mFrameConstructor->GetHoverGeneration();
  nsEventStatus status;
  nsIView* targetView = nsIView::GetViewFor(aEvent->widget);
  targetView->GetViewManager()->DispatchEvent(aEvent, targetView, &status);
  if (aFlushOnHoverChange &&
      hoverGenerationBefore != mFrameConstructor->GetHoverGeneration()) {
    // Flush so that the resulting reflow happens now so that our caller
    // can suppress any synthesized mouse moves caused by that reflow.
    FlushPendingNotifications(Flush_Layout);
  }
}

NS_IMETHODIMP_(void)
PresShell::ClearMouseCapture(nsIView* aView)
{
  if (gCaptureInfo.mContent) {
    if (aView) {
      // if a view was specified, ensure that the captured content is within
      // this view.
      nsIFrame* frame = gCaptureInfo.mContent->GetPrimaryFrame();
      if (frame) {
        nsIView* view = frame->GetClosestView();
        // if there is no view, capturing won't be handled any more, so
        // just release the capture.
        if (view) {
          do {
            if (view == aView) {
              NS_RELEASE(gCaptureInfo.mContent);
              // the view containing the captured content likely disappeared so
              // disable capture for now.
              gCaptureInfo.mAllowed = PR_FALSE;
              break;
            }

            view = view->GetParent();
          } while (view);
          // return if the view wasn't found
          return;
        }
      }
    }

    NS_RELEASE(gCaptureInfo.mContent);
  }

  // disable mouse capture until the next mousedown as a dialog has opened
  // or a drag has started. Otherwise, someone could start capture during
  // the modal dialog or drag.
  gCaptureInfo.mAllowed = PR_FALSE;
}

nsresult
PresShell::CaptureHistoryState(nsILayoutHistoryState** aState, PRBool aLeavingPage)
{
  NS_TIME_FUNCTION_MIN(1.0);

  nsresult rv = NS_OK;

  NS_PRECONDITION(nsnull != aState, "null state pointer");

  // We actually have to mess with the docshell here, since we want to
  // store the state back in it.
  // XXXbz this isn't really right, since this is being called in the
  // content viewer's Hide() method...  by that point the docshell's
  // state could be wrong.  We should sort out a better ownership
  // model for the layout history state.
  nsCOMPtr<nsISupports> container = mPresContext->GetContainer();
  if (!container)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(container));
  if (!docShell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsILayoutHistoryState> historyState;
  docShell->GetLayoutHistoryState(getter_AddRefs(historyState));
  if (!historyState) {
    // Create the document state object
    rv = NS_NewLayoutHistoryState(getter_AddRefs(historyState));
  
    if (NS_FAILED(rv)) { 
      *aState = nsnull;
      return rv;
    }    

    docShell->SetLayoutHistoryState(historyState);
  }

  *aState = historyState;
  NS_IF_ADDREF(*aState);
  
  // Capture frame state for the entire frame hierarchy
  nsIFrame* rootFrame = FrameManager()->GetRootFrame();
  if (!rootFrame) return NS_OK;
  // Capture frame state for the root scroll frame
  // Don't capture state when first creating doc element hierarchy
  // As the scroll position is 0 and this will cause us to lose
  // our previously saved place!
  if (aLeavingPage) {
    nsIFrame* scrollFrame = GetRootScrollFrame();
    if (scrollFrame) {
      FrameManager()->CaptureFrameStateFor(scrollFrame, historyState,
                                           nsIStatefulFrame::eDocumentScrollState);
    }
  }

  FrameManager()->CaptureFrameState(rootFrame, historyState);  
 
  return NS_OK;
}

void
PresShell::UnsuppressAndInvalidate()
{
  // Note: We ignore the EnsureVisible check for resource documents, because
  // they won't have a docshell, so they'll always fail EnsureVisible.
  if ((!mDocument->IsResourceDoc() && !mPresContext->EnsureVisible()) ||
      mHaveShutDown) {
    // No point; we're about to be torn down anyway.
    return;
  }
  
  mPaintingSuppressed = PR_FALSE;
  nsIFrame* rootFrame = FrameManager()->GetRootFrame();
  if (rootFrame) {
    // let's assume that outline on a root frame is not supported
    nsRect rect(nsPoint(0, 0), rootFrame->GetSize());
    rootFrame->Invalidate(rect);

    if (mCaretEnabled && mCaret) {
      mCaret->CheckCaretDrawingState();
    }

    nsRootPresContext* rootPC = mPresContext->GetRootPresContext();
    if (rootPC) {
      rootPC->RequestUpdatePluginGeometry(rootFrame);
    }
  }

  // now that painting is unsuppressed, focus may be set on the document
  nsPIDOMWindow *win = mDocument->GetWindow();
  if (win)
    win->SetReadyForFocus();

  if (!mHaveShutDown && mViewManager)
    mViewManager->SynthesizeMouseMove(PR_FALSE);
}

void
PresShell::UnsuppressPainting()
{
  if (mPaintSuppressionTimer) {
    mPaintSuppressionTimer->Cancel();
    mPaintSuppressionTimer = nsnull;
  }

  if (mIsDocumentGone || !mPaintingSuppressed)
    return;

  // If we have reflows pending, just wait until we process
  // the reflows and get all the frames where we want them
  // before actually unlocking the painting.  Otherwise
  // go ahead and unlock now.
  if (mDirtyRoots.Length() > 0)
    mShouldUnsuppressPainting = PR_TRUE;
  else
    UnsuppressAndInvalidate();
}

// Post a request to handle an arbitrary callback after reflow has finished.
nsresult
PresShell::PostReflowCallback(nsIReflowCallback* aCallback)
{
  void* result = AllocateMisc(sizeof(nsCallbackEventRequest));
  if (NS_UNLIKELY(!result)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsCallbackEventRequest* request = (nsCallbackEventRequest*)result;

  request->callback = aCallback;
  request->next = nsnull;

  if (mLastCallbackEventRequest) {
    mLastCallbackEventRequest = mLastCallbackEventRequest->next = request;
  } else {
    mFirstCallbackEventRequest = request;
    mLastCallbackEventRequest = request;
  }
 
  return NS_OK;
}

void
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
          node = node->next;
          mFirstCallbackEventRequest = node;
          NS_ASSERTION(before == nsnull, "impossible");
        } else {
          node = node->next;
          before->next = node;
        }

        if (toFree == mLastCallbackEventRequest) {
          mLastCallbackEventRequest = before;
        }

        FreeMisc(sizeof(nsCallbackEventRequest), toFree);
      } else {
        before = node;
        node = node->next;
      }
   }
}

void
PresShell::CancelPostedReflowCallbacks()
{
  while (mFirstCallbackEventRequest) {
    nsCallbackEventRequest* node = mFirstCallbackEventRequest;
    mFirstCallbackEventRequest = node->next;
    if (!mFirstCallbackEventRequest) {
      mLastCallbackEventRequest = nsnull;
    }
    nsIReflowCallback* callback = node->callback;
    FreeMisc(sizeof(nsCallbackEventRequest), node);
    if (callback) {
      callback->ReflowCallbackCanceled();
    }
  }
}

void
PresShell::HandlePostedReflowCallbacks(PRBool aInterruptible)
{
   PRBool shouldFlush = PR_FALSE;

   while (mFirstCallbackEventRequest) {
     nsCallbackEventRequest* node = mFirstCallbackEventRequest;
     mFirstCallbackEventRequest = node->next;
     if (!mFirstCallbackEventRequest) {
       mLastCallbackEventRequest = nsnull;
     }
     nsIReflowCallback* callback = node->callback;
     FreeMisc(sizeof(nsCallbackEventRequest), node);
     if (callback) {
       if (callback->ReflowFinished()) {
         shouldFlush = PR_TRUE;
       }
     }
   }

   mozFlushType flushType =
     aInterruptible ? Flush_InterruptibleLayout : Flush_Layout;
   if (shouldFlush)
     FlushPendingNotifications(flushType);
}

PRBool
PresShell::IsSafeToFlush() const
{
  // Not safe if we are reflowing or in the middle of frame construction
  PRBool isSafeToFlush = !mIsReflowing &&
                         !mChangeNestCount;

  if (isSafeToFlush) {
    // Not safe if we are painting
    nsIViewManager* viewManager = GetViewManager();
    if (viewManager) {
      PRBool isPainting = PR_FALSE;
      viewManager->IsPainting(isPainting);
      if (isPainting) {
        isSafeToFlush = PR_FALSE;
      }
    }
  }

  return isSafeToFlush;
}


void
PresShell::FlushPendingNotifications(mozFlushType aType)
{
#ifdef NS_FUNCTION_TIMER
  NS_TIME_FUNCTION_DECLARE_DOCURL;
  static const char *flushTypeNames[] = {
    "Flush_Content",
    "Flush_ContentAndNotify",
    "Flush_Styles",
    "Flush_InterruptibleLayout",
    "Flush_Layout",
    "Flush_Display"
  };
  NS_TIME_FUNCTION_MIN_FMT(1.0, "%s (line %d) (document: %s, type: %s)", MOZ_FUNCTION_NAME,
                           __LINE__, docURL__.get(), flushTypeNames[aType - 1]);
#endif

  NS_ASSERTION(aType >= Flush_Frames, "Why did we get called?");

  PRBool isSafeToFlush = IsSafeToFlush();

  // If layout could possibly trigger scripts, then it's only safe to flush if
  // it's safe to run script.
  if (mDocument->GetScriptGlobalObject()) {
    isSafeToFlush = isSafeToFlush && nsContentUtils::IsSafeToRunScript();
  }

  NS_ASSERTION(!isSafeToFlush || mViewManager, "Must have view manager");
  // Make sure the view manager stays alive while batching view updates.
  nsCOMPtr<nsIViewManager> viewManagerDeathGrip = mViewManager;
  if (isSafeToFlush && mViewManager) {
    // Processing pending notifications can kill us, and some callers only
    // hold weak refs when calling FlushPendingNotifications().  :(
    nsCOMPtr<nsIPresShell> kungFuDeathGrip(this);

    if (mResizeEvent.IsPending()) {
      FireResizeEvent();
      if (mIsDestroying) {
        return;
      }
    }

    // Style reresolves not in conjunction with reflows can't cause
    // painting or geometry changes, so don't bother with view update
    // batching if we only have style reresolve
    nsIViewManager::UpdateViewBatch batch(mViewManager);

    // We need to make sure external resource documents are flushed too (for
    // example, svg filters that reference a filter in an external document
    // need the frames in the external document to be constructed for the
    // filter to work). We only need external resources to be flushed when the
    // main document is flushing >= Flush_Frames, so we flush external
    // resources here instead of nsDocument::FlushPendingNotifications.
    mDocument->FlushExternalResources(aType);

    // Force flushing of any pending content notifications that might have
    // queued up while our event was pending.  That will ensure that we don't
    // construct frames for content right now that's still waiting to be
    // notified on,
    mDocument->FlushPendingNotifications(Flush_ContentAndNotify);

    // Process pending restyles, since any flush of the presshell wants
    // up-to-date style data.
    if (!mIsDestroying) {
      mViewManager->FlushDelayedResize(PR_FALSE);
      mPresContext->FlushPendingMediaFeatureValuesChanged();

      // Flush any pending update of the user font set, since that could
      // cause style changes (for updating ex/ch units, and to cause a
      // reflow).
      mPresContext->FlushUserFontSet();

#ifdef MOZ_SMIL
      // Flush any requested SMIL samples.
      if (mDocument->HasAnimationController()) {
        mDocument->GetAnimationController()->FlushResampleRequests();
      }
#endif // MOZ_SMIL

      nsAutoScriptBlocker scriptBlocker;
      mFrameConstructor->CreateNeededFrames();
      mFrameConstructor->ProcessPendingRestyles();
    }

    // Process whatever XBL constructors those restyles queued up.  This
    // ensures that onload doesn't fire too early and that we won't do extra
    // reflows after those constructors run.
    if (!mIsDestroying) {
      mDocument->BindingManager()->ProcessAttachedQueue();
    }

    // Now those constructors might have posted restyle events.  At the same
    // time, we still need up-to-date style data.  In particular, reflow
    // depends on style being completely up to date.  If it's not, then style
    // context reparenting, which can happen during reflow, might suddenly pick
    // up the new rules and we'll end up with frames whose style doesn't match
    // the frame type.
    if (!mIsDestroying) {
      nsAutoScriptBlocker scriptBlocker;
      mFrameConstructor->CreateNeededFrames();
      mFrameConstructor->ProcessPendingRestyles();
    }


    // There might be more pending constructors now, but we're not going to
    // worry about them.  They can't be triggered during reflow, so we should
    // be good.

    if (aType >= (mSuppressInterruptibleReflows ? Flush_Layout : Flush_InterruptibleLayout) &&
        !mIsDestroying) {
      mFrameConstructor->RecalcQuotesAndCounters();
      mViewManager->FlushDelayedResize(PR_TRUE);
      if (ProcessReflowCommands(aType < Flush_Layout) && mContentToScrollTo) {
        // We didn't get interrupted.  Go ahead and scroll to our content
        DoScrollContentIntoView(mContentToScrollTo, mContentScrollVPosition,
                                mContentScrollHPosition);
        mContentToScrollTo = nsnull;
      }
    }

    if (aType >= Flush_Layout) {
      // Flush plugin geometry. Don't flush plugin geometry for
      // interruptible layouts, since WillPaint does an interruptible
      // layout.
      nsRootPresContext* rootPresContext = mPresContext->GetRootPresContext();
      if (rootPresContext) {
        rootPresContext->UpdatePluginGeometry();
      }
    }

    PRUint32 updateFlags = NS_VMREFRESH_NO_SYNC;
    if (aType >= Flush_Display) {
      // Flushing paints, so perform the invalidates and drawing
      // immediately
      updateFlags = NS_VMREFRESH_IMMEDIATE;
    }
    batch.EndUpdateViewBatch(updateFlags);
  }
}

void
PresShell::CharacterDataChanged(nsIDocument *aDocument,
                                nsIContent*  aContent,
                                CharacterDataChangeInfo* aInfo)
{
  NS_PRECONDITION(!mIsDocumentGone, "Unexpected CharacterDataChanged");
  NS_PRECONDITION(aDocument == mDocument, "Unexpected aDocument");

  nsAutoCauseReflowNotifier crNotifier(this);

  if (mCaret) {
    // Invalidate the caret's current location before we call into the frame
    // constructor. It is important to do this now, and not wait until the
    // resulting reflow, because this call causes continuation frames of the
    // text frame the caret is in to forget what part of the content they
    // refer to, making it hard for them to return the correct continuation
    // frame to the caret.
    mCaret->InvalidateOutsideCaret();
  }

  // Call this here so it only happens for real content mutations and
  // not cases when the frame constructor calls its own methods to force
  // frame reconstruction.
  nsIContent *container = aContent->GetParent();
  PRUint32 selectorFlags =
    container ? (container->GetFlags() & NODE_ALL_SELECTOR_FLAGS) : 0;
  if (selectorFlags != 0 && !aContent->IsRootOfAnonymousSubtree()) {
    Element* element = container->AsElement();
    if (aInfo->mAppend && !aContent->GetNextSibling())
      mFrameConstructor->RestyleForAppend(element, aContent);
    else
      mFrameConstructor->RestyleForInsertOrChange(element, aContent);
  }

  mFrameConstructor->CharacterDataChanged(aContent, aInfo);
  VERIFY_STYLE_TREE;
}

void
PresShell::ContentStatesChanged(nsIDocument* aDocument,
                                nsIContent* aContent1,
                                nsIContent* aContent2,
                                PRInt32 aStateMask)
{
  NS_PRECONDITION(!mIsDocumentGone, "Unexpected ContentStatesChanged");
  NS_PRECONDITION(aDocument == mDocument, "Unexpected aDocument");

  if (mDidInitialReflow) {
    nsAutoCauseReflowNotifier crNotifier(this);
    mFrameConstructor->ContentStatesChanged(aContent1, aContent2, aStateMask);
    VERIFY_STYLE_TREE;
  }
}

void
PresShell::DocumentStatesChanged(nsIDocument* aDocument,
                                 PRInt32 aStateMask)
{
  NS_PRECONDITION(!mIsDocumentGone, "Unexpected DocumentStatesChanged");
  NS_PRECONDITION(aDocument == mDocument, "Unexpected aDocument");

  if (mDidInitialReflow &&
      mStyleSet->HasDocumentStateDependentStyle(mPresContext,
                                                mDocument->GetRootElement(),
                                                aStateMask)) {
    mFrameConstructor->PostRestyleEvent(mDocument->GetRootElement(),
                                        eRestyle_Subtree, NS_STYLE_HINT_NONE);
    VERIFY_STYLE_TREE;
  }

  if (aStateMask & NS_DOCUMENT_STATE_WINDOW_INACTIVE) {
    nsIFrame* root = FrameManager()->GetRootFrame();
    if (root) {
      // It's a display root. So, invalidate the layer contents of
      // everything we can find. We need to do this because the contents
      // of controls etc can depend on whether the window is active,
      // and when a window becomes (in)active it just gets repainted
      // and we don't specifically invalidate each affected control.
      nsIWidget* widget = root->GetNearestWidget();
      if (widget) {
        LayerManager* layerManager = widget->GetLayerManager();
        if (layerManager) {
          FrameLayerBuilder::InvalidateAllThebesLayerContents(layerManager);
        }
      }
    }
  }
}

void
PresShell::AttributeWillChange(nsIDocument* aDocument,
                               Element*     aElement,
                               PRInt32      aNameSpaceID,
                               nsIAtom*     aAttribute,
                               PRInt32      aModType)
{
  NS_PRECONDITION(!mIsDocumentGone, "Unexpected AttributeWillChange");
  NS_PRECONDITION(aDocument == mDocument, "Unexpected aDocument");

  // XXXwaterson it might be more elegant to wait until after the
  // initial reflow to begin observing the document. That would
  // squelch any other inappropriate notifications as well.
  if (mDidInitialReflow) {
    nsAutoCauseReflowNotifier crNotifier(this);
    mFrameConstructor->AttributeWillChange(aElement, aNameSpaceID,
                                           aAttribute, aModType);
    VERIFY_STYLE_TREE;
  }
}

void
PresShell::AttributeChanged(nsIDocument* aDocument,
                            Element*     aElement,
                            PRInt32      aNameSpaceID,
                            nsIAtom*     aAttribute,
                            PRInt32      aModType)
{
  NS_PRECONDITION(!mIsDocumentGone, "Unexpected AttributeChanged");
  NS_PRECONDITION(aDocument == mDocument, "Unexpected aDocument");

  // XXXwaterson it might be more elegant to wait until after the
  // initial reflow to begin observing the document. That would
  // squelch any other inappropriate notifications as well.
  if (mDidInitialReflow) {
    nsAutoCauseReflowNotifier crNotifier(this);
    mFrameConstructor->AttributeChanged(aElement, aNameSpaceID,
                                        aAttribute, aModType);
    VERIFY_STYLE_TREE;
  }
}

void
PresShell::ContentAppended(nsIDocument *aDocument,
                           nsIContent* aContainer,
                           nsIContent* aFirstNewContent,
                           PRInt32     aNewIndexInContainer)
{
  NS_PRECONDITION(!mIsDocumentGone, "Unexpected ContentAppended");
  NS_PRECONDITION(aDocument == mDocument, "Unexpected aDocument");
  NS_PRECONDITION(aContainer, "must have container");
  
  if (!mDidInitialReflow) {
    return;
  }
  
  nsAutoCauseReflowNotifier crNotifier(this);

  // Call this here so it only happens for real content mutations and
  // not cases when the frame constructor calls its own methods to force
  // frame reconstruction.
  mFrameConstructor->RestyleForAppend(aContainer->AsElement(), aFirstNewContent);

  mFrameConstructor->ContentAppended(aContainer, aFirstNewContent, PR_TRUE);
  VERIFY_STYLE_TREE;
}

void
PresShell::ContentInserted(nsIDocument* aDocument,
                           nsIContent*  aContainer,
                           nsIContent*  aChild,
                           PRInt32      aIndexInContainer)
{
  NS_PRECONDITION(!mIsDocumentGone, "Unexpected ContentInserted");
  NS_PRECONDITION(aDocument == mDocument, "Unexpected aDocument");

  if (!mDidInitialReflow) {
    return;
  }
  
  nsAutoCauseReflowNotifier crNotifier(this);

  // Call this here so it only happens for real content mutations and
  // not cases when the frame constructor calls its own methods to force
  // frame reconstruction.
  if (aContainer)
    mFrameConstructor->RestyleForInsertOrChange(aContainer->AsElement(), aChild);

  mFrameConstructor->ContentInserted(aContainer, aChild, nsnull, PR_TRUE);
  VERIFY_STYLE_TREE;
}

void
PresShell::ContentRemoved(nsIDocument *aDocument,
                          nsIContent* aContainer,
                          nsIContent* aChild,
                          PRInt32     aIndexInContainer,
                          nsIContent* aPreviousSibling)
{
  NS_PRECONDITION(!mIsDocumentGone, "Unexpected ContentRemoved");
  NS_PRECONDITION(aDocument == mDocument, "Unexpected aDocument");

  // Make sure that the caret doesn't leave a turd where the child used to be.
  if (mCaret) {
    mCaret->InvalidateOutsideCaret();
  }

  // Notify the ESM that the content has been removed, so that
  // it can clean up any state related to the content.
  mPresContext->EventStateManager()->ContentRemoved(aDocument, aChild);

  nsAutoCauseReflowNotifier crNotifier(this);

  // Call this here so it only happens for real content mutations and
  // not cases when the frame constructor calls its own methods to force
  // frame reconstruction.
  nsIContent* oldNextSibling;
  if (aContainer) {
    oldNextSibling = aContainer->GetChildAt(aIndexInContainer);
  } else {
    oldNextSibling = nsnull;
  }
  
  if (aContainer)
    mFrameConstructor->RestyleForRemove(aContainer->AsElement(), aChild,
                                        oldNextSibling);

  PRBool didReconstruct;
  mFrameConstructor->ContentRemoved(aContainer, aChild, oldNextSibling,
                                    nsCSSFrameConstructor::REMOVE_CONTENT,
                                    &didReconstruct);

  VERIFY_STYLE_TREE;
}

nsresult
PresShell::ReconstructFrames(void)
{
  nsCOMPtr<nsIPresShell> kungFuDeathGrip(this);

  // Have to make sure that the content notifications are flushed before we
  // start messing with the frame model; otherwise we can get content doubling.
  mDocument->FlushPendingNotifications(Flush_ContentAndNotify);

  nsAutoCauseReflowNotifier crNotifier(this);
  mFrameConstructor->BeginUpdate();
  nsresult rv = mFrameConstructor->ReconstructDocElementHierarchy();
  VERIFY_STYLE_TREE;
  mFrameConstructor->EndUpdate();

  return rv;
}

void
nsIPresShell::ReconstructStyleDataInternal()
{
  mStylesHaveChanged = PR_FALSE;

  if (mIsDestroying) {
    // We don't want to mess with restyles at this point
    return;
  }

  if (mPresContext) {
    mPresContext->RebuildUserFontSet();
  }

  Element* root = mDocument->GetRootElement();
  if (!mDidInitialReflow) {
    // Nothing to do here, since we have no frames yet
    return;
  }

  if (!root) {
    // No content to restyle
    return;
  }
  
  mFrameConstructor->PostRestyleEvent(root, eRestyle_Subtree, NS_STYLE_HINT_NONE);

#ifdef ACCESSIBILITY
  InvalidateAccessibleSubtree(nsnull);
#endif
}

void
nsIPresShell::ReconstructStyleDataExternal()
{
  ReconstructStyleDataInternal();
}

void
PresShell::StyleSheetAdded(nsIDocument *aDocument,
                           nsIStyleSheet* aStyleSheet,
                           PRBool aDocumentSheet)
{
  // We only care when enabled sheets are added
  NS_PRECONDITION(aStyleSheet, "Must have a style sheet!");

  if (aStyleSheet->IsApplicable() && aStyleSheet->HasRules()) {
    mStylesHaveChanged = PR_TRUE;
  }
}

void 
PresShell::StyleSheetRemoved(nsIDocument *aDocument,
                             nsIStyleSheet* aStyleSheet,
                             PRBool aDocumentSheet)
{
  // We only care when enabled sheets are removed
  NS_PRECONDITION(aStyleSheet, "Must have a style sheet!");

  if (aStyleSheet->IsApplicable() && aStyleSheet->HasRules()) {
    mStylesHaveChanged = PR_TRUE;
  }
}

void
PresShell::StyleSheetApplicableStateChanged(nsIDocument *aDocument,
                                            nsIStyleSheet* aStyleSheet,
                                            PRBool aApplicable)
{
  if (aStyleSheet->HasRules()) {
    mStylesHaveChanged = PR_TRUE;
  }
}

void
PresShell::StyleRuleChanged(nsIDocument *aDocument,
                            nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aOldStyleRule,
                            nsIStyleRule* aNewStyleRule)
{
  mStylesHaveChanged = PR_TRUE;
}

void
PresShell::StyleRuleAdded(nsIDocument *aDocument,
                          nsIStyleSheet* aStyleSheet,
                          nsIStyleRule* aStyleRule) 
{
  mStylesHaveChanged = PR_TRUE;
}

void
PresShell::StyleRuleRemoved(nsIDocument *aDocument,
                            nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule) 
{
  mStylesHaveChanged = PR_TRUE;
}

nsIFrame*
PresShell::GetRealPrimaryFrameFor(nsIContent* aContent) const
{
  if (aContent->GetDocument() != GetDocument()) {
    return nsnull;
  }
  nsIFrame *primaryFrame = aContent->GetPrimaryFrame();
  if (!primaryFrame)
    return nsnull;
  return nsPlaceholderFrame::GetRealFrameFor(primaryFrame);
}

nsIFrame*
PresShell::GetPlaceholderFrameFor(nsIFrame* aFrame) const
{
  return FrameManager()->GetPlaceholderFrameFor(aFrame);
}

nsresult
PresShell::RenderDocument(const nsRect& aRect, PRUint32 aFlags,
                          nscolor aBackgroundColor,
                          gfxContext* aThebesContext)
{
  NS_TIME_FUNCTION_WITH_DOCURL;

  NS_ENSURE_TRUE(!(aFlags & RENDER_IS_UNTRUSTED), NS_ERROR_NOT_IMPLEMENTED);

  // Set up the rectangle as the path in aThebesContext
  gfxRect r(0, 0,
            nsPresContext::AppUnitsToFloatCSSPixels(aRect.width),
            nsPresContext::AppUnitsToFloatCSSPixels(aRect.height));
  aThebesContext->NewPath();
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
  aThebesContext->Rectangle(r, PR_TRUE);
#else
  aThebesContext->Rectangle(r);
#endif

  nsIFrame* rootFrame = FrameManager()->GetRootFrame();
  if (!rootFrame) {
    // Nothing to paint, just fill the rect
    aThebesContext->SetColor(gfxRGBA(aBackgroundColor));
    aThebesContext->Fill();
    return NS_OK;
  }

  gfxContextAutoSaveRestore save(aThebesContext);

  gfxContext::GraphicsOperator oldOperator = aThebesContext->CurrentOperator();
  if (oldOperator == gfxContext::OPERATOR_OVER) {
    // Clip to the destination rectangle before we push the group,
    // to limit the size of the temporary surface
    aThebesContext->Clip();
  }

  // we want the window to be composited as a single image using
  // whatever operator was set; set OPERATOR_OVER here, which is
  // either already the case, or overrides the operator in a group.
  // the original operator will be present when we PopGroup.
  // we can avoid using a temporary surface if we're using OPERATOR_OVER
  // and our background color has no alpha (so we'll be compositing on top
  // of a fully opaque solid color region)
  PRBool needsGroup = NS_GET_A(aBackgroundColor) < 0xff ||
    oldOperator != gfxContext::OPERATOR_OVER;

  if (needsGroup) {
    aThebesContext->PushGroup(NS_GET_A(aBackgroundColor) == 0xff ?
                              gfxASurface::CONTENT_COLOR :
                              gfxASurface::CONTENT_COLOR_ALPHA);
    aThebesContext->Save();

    if (oldOperator != gfxContext::OPERATOR_OVER) {
      // Clip now while we paint to the temporary surface. For
      // non-source-bounded operators (e.g., SOURCE), we need to do clip
      // here after we've pushed the group, so that eventually popping
      // the group and painting it will be able to clear the entire
      // destination surface.
      aThebesContext->Clip();
      aThebesContext->SetOperator(gfxContext::OPERATOR_OVER);
    }
  }

  aThebesContext->Translate(gfxPoint(-nsPresContext::AppUnitsToFloatCSSPixels(aRect.x),
                                     -nsPresContext::AppUnitsToFloatCSSPixels(aRect.y)));

  nsIDeviceContext* devCtx = mPresContext->DeviceContext();
  gfxFloat scale = gfxFloat(devCtx->AppUnitsPerDevPixel())/nsPresContext::AppUnitsPerCSSPixel();
  aThebesContext->Scale(scale, scale);

  // Since canvas APIs use floats to set up their matrices, we may have
  // some slight inaccuracy here. Adjust matrix components that are
  // integers up to the accuracy of floats to be those integers.
  aThebesContext->NudgeCurrentMatrixToIntegers();

  nsCOMPtr<nsIRenderingContext> rc;
  devCtx->CreateRenderingContextInstance(*getter_AddRefs(rc));
  rc->Init(devCtx, aThebesContext);

  PRUint32 flags = nsLayoutUtils::PAINT_IGNORE_SUPPRESSION;
  if (!(aFlags & RENDER_ASYNC_DECODE_IMAGES)) {
    flags |= nsLayoutUtils::PAINT_SYNC_DECODE_IMAGES;
  }
  if (aFlags & RENDER_USE_WIDGET_LAYERS) {
    // We only support using widget layers on display root's with widgets.
    nsIView* view = rootFrame->GetView();
    if (view && view->GetWidget() &&
        nsLayoutUtils::GetDisplayRootFrame(rootFrame) == rootFrame) {
      flags |= nsLayoutUtils::PAINT_WIDGET_LAYERS;
    }
  }
  if (!(aFlags & RENDER_CARET)) {
    flags |= nsLayoutUtils::PAINT_HIDE_CARET;
  }
  if (aFlags & RENDER_IGNORE_VIEWPORT_SCROLLING) {
    flags |= nsLayoutUtils::PAINT_IGNORE_VIEWPORT_SCROLLING;
  }
  nsLayoutUtils::PaintFrame(rc, rootFrame, nsRegion(aRect),
                            aBackgroundColor, flags);

  // if we had to use a group, paint it to the destination now
  if (needsGroup) {
    aThebesContext->Restore();
    aThebesContext->PopGroupToSource();
    aThebesContext->Paint();
  }

  return NS_OK;
}

/*
 * Clip the display list aList to a range. Returns the clipped
 * rectangle surrounding the range.
 */
nsRect
PresShell::ClipListToRange(nsDisplayListBuilder *aBuilder,
                           nsDisplayList* aList,
                           nsIRange* aRange)
{
  NS_TIME_FUNCTION_WITH_DOCURL;

  // iterate though the display items and add up the bounding boxes of each.
  // This will allow the total area of the frames within the range to be
  // determined. To do this, remove an item from the bottom of the list, check
  // whether it should be part of the range, and if so, append it to the top
  // of the temporary list tmpList. If the item is a text frame at the end of
  // the selection range, wrap it in an nsDisplayClip to clip the display to
  // the portion of the text frame that is part of the selection. Then, append
  // the wrapper to the top of the list. Otherwise, just delete the item and
  // don't append it.
  nsRect surfaceRect;
  nsDisplayList tmpList;

  nsDisplayItem* i;
  while ((i = aList->RemoveBottom())) {
    // itemToInsert indiciates the item that should be inserted into the
    // temporary list. If null, no item should be inserted.
    nsDisplayItem* itemToInsert = nsnull;
    nsIFrame* frame = i->GetUnderlyingFrame();
    if (frame) {
      nsIContent* content = frame->GetContent();
      if (content) {
        PRBool atStart = (content == aRange->GetStartParent());
        PRBool atEnd = (content == aRange->GetEndParent());
        if ((atStart || atEnd) && frame->GetType() == nsGkAtoms::textFrame) {
          PRInt32 frameStartOffset, frameEndOffset;
          frame->GetOffsets(frameStartOffset, frameEndOffset);

          PRInt32 hilightStart =
            atStart ? NS_MAX(aRange->StartOffset(), frameStartOffset) : frameStartOffset;
          PRInt32 hilightEnd =
            atEnd ? NS_MIN(aRange->EndOffset(), frameEndOffset) : frameEndOffset;
          if (hilightStart < hilightEnd) {
            // determine the location of the start and end edges of the range.
            nsPoint startPoint, endPoint;
            frame->GetPointFromOffset(hilightStart, &startPoint);
            frame->GetPointFromOffset(hilightEnd, &endPoint);

            // the clip rectangle is determined by taking the the start and
            // end points of the range, offset from the reference frame.
            // Because of rtl, the end point may be to the left of the
            // start point, so x is set to the lowest value
            nsRect textRect(aBuilder->ToReferenceFrame(frame), frame->GetSize());
            nscoord x = NS_MIN(startPoint.x, endPoint.x);
            textRect.x += x;
            textRect.width = NS_MAX(startPoint.x, endPoint.x) - x;
            surfaceRect.UnionRect(surfaceRect, textRect);

            // wrap the item in an nsDisplayClip so that it can be clipped to
            // the selection. If the allocation fails, fall through and delete
            // the item below.
            itemToInsert = new (aBuilder)
                nsDisplayClip(aBuilder, frame, i, textRect);
          }
        }
        // Don't try to descend into subdocuments.
        // If this ever changes we'd need to add handling for subdocuments with
        // different zoom levels.
        else if (content->GetCurrentDoc() ==
                   aRange->GetStartParent()->GetCurrentDoc()) {
          // if the node is within the range, append it to the temporary list
          PRBool before, after;
          nsresult rv =
            nsRange::CompareNodeToRange(content, aRange, &before, &after);
          if (NS_SUCCEEDED(rv) && !before && !after) {
            itemToInsert = i;
            surfaceRect.UnionRect(surfaceRect, i->GetBounds(aBuilder));
          }
        }
      }
    }

    // insert the item into the list if necessary. If the item has a child
    // list, insert that as well
    nsDisplayList* sublist = i->GetList();
    if (itemToInsert || sublist) {
      tmpList.AppendToTop(itemToInsert ? itemToInsert : i);
      // if the item is a list, iterate over it as well
      if (sublist)
        surfaceRect.UnionRect(surfaceRect,
          ClipListToRange(aBuilder, sublist, aRange));
    }
    else {
      // otherwise, just delete the item and don't readd it to the list
      i->~nsDisplayItem();
    }
  }

  // now add all the items back onto the original list again
  aList->AppendToTop(&tmpList);

  return surfaceRect;
}

#ifdef DEBUG
#include <stdio.h>

static PRBool gDumpRangePaintList = PR_FALSE;
#endif

RangePaintInfo*
PresShell::CreateRangePaintInfo(nsIDOMRange* aRange,
                                nsRect& aSurfaceRect,
                                PRBool aForPrimarySelection)
{
  NS_TIME_FUNCTION_WITH_DOCURL;

  RangePaintInfo* info = nsnull;

  nsCOMPtr<nsIRange> range = do_QueryInterface(aRange);
  if (!range)
    return nsnull;

  nsIFrame* ancestorFrame;
  nsIFrame* rootFrame = GetRootFrame();

  // If the start or end of the range is the document, just use the root
  // frame, otherwise get the common ancestor of the two endpoints of the
  // range.
  nsINode* startParent = range->GetStartParent();
  nsINode* endParent = range->GetEndParent();
  nsIDocument* doc = startParent->GetCurrentDoc();
  if (startParent == doc || endParent == doc) {
    ancestorFrame = rootFrame;
  }
  else {
    nsINode* ancestor = nsContentUtils::GetCommonAncestor(startParent, endParent);
    NS_ASSERTION(!ancestor || ancestor->IsNodeOfType(nsINode::eCONTENT),
                 "common ancestor is not content");
    if (!ancestor || !ancestor->IsNodeOfType(nsINode::eCONTENT))
      return nsnull;

    nsIContent* ancestorContent = static_cast<nsIContent*>(ancestor);
    ancestorFrame = ancestorContent->GetPrimaryFrame();

    // use the nearest ancestor frame that includes all continuations as the
    // root for building the display list
    while (ancestorFrame &&
           nsLayoutUtils::GetNextContinuationOrSpecialSibling(ancestorFrame))
      ancestorFrame = ancestorFrame->GetParent();
  }

  if (!ancestorFrame)
    return nsnull;

  info = new RangePaintInfo(range, ancestorFrame);
  if (!info)
    return nsnull;

  nsRect ancestorRect = ancestorFrame->GetOverflowRect();

  // get a display list containing the range
  if (aForPrimarySelection) {
    info->mBuilder.SetSelectedFramesOnly();
  }
  info->mBuilder.EnterPresShell(ancestorFrame, ancestorRect);
  ancestorFrame->BuildDisplayListForStackingContext(&info->mBuilder,
                                                    ancestorRect, &info->mList);
  info->mBuilder.LeavePresShell(ancestorFrame, ancestorRect);

#ifdef DEBUG
  if (gDumpRangePaintList) {
    fprintf(stderr, "CreateRangePaintInfo --- before ClipListToRange:\n");
    nsFrame::PrintDisplayList(&(info->mBuilder), info->mList);
  }
#endif

  nsRect rangeRect = ClipListToRange(&info->mBuilder, &info->mList, range);

#ifdef DEBUG
  if (gDumpRangePaintList) {
    fprintf(stderr, "CreateRangePaintInfo --- after ClipListToRange:\n");
    nsFrame::PrintDisplayList(&(info->mBuilder), info->mList);
  }
#endif

  // determine the offset of the reference frame for the display list
  // to the root frame. This will allow the coordinates used when painting
  // to all be offset from the same point
  info->mRootOffset = ancestorFrame->GetOffsetTo(rootFrame);
  rangeRect.MoveBy(info->mRootOffset);
  aSurfaceRect.UnionRect(aSurfaceRect, rangeRect);

  return info;
}

already_AddRefed<gfxASurface>
PresShell::PaintRangePaintInfo(nsTArray<nsAutoPtr<RangePaintInfo> >* aItems,
                               nsISelection* aSelection,
                               nsIntRegion* aRegion,
                               nsRect aArea,
                               nsIntPoint& aPoint,
                               nsIntRect* aScreenRect)
{
  NS_TIME_FUNCTION_WITH_DOCURL;

  nsPresContext* pc = GetPresContext();
  if (!pc || aArea.width == 0 || aArea.height == 0)
    return nsnull;

  nsIDeviceContext* deviceContext = pc->DeviceContext();

  // use the rectangle to create the surface
  nsIntRect pixelArea = aArea.ToOutsidePixels(pc->AppUnitsPerDevPixel());

  // if the area of the image is larger than the maximum area, scale it down
  float scale = 0.0;
  nsIntRect rootScreenRect =
    GetRootFrame()->GetScreenRectInAppUnits().ToNearestPixels(
      pc->AppUnitsPerDevPixel());

  // if the image is larger in one or both directions than half the size of
  // the available screen area, scale the image down to that size.
  nsRect maxSize;
  deviceContext->GetClientRect(maxSize);
  nscoord maxWidth = pc->AppUnitsToDevPixels(maxSize.width >> 1);
  nscoord maxHeight = pc->AppUnitsToDevPixels(maxSize.height >> 1);
  PRBool resize = (pixelArea.width > maxWidth || pixelArea.height > maxHeight);
  if (resize) {
    scale = 1.0;
    // divide the maximum size by the image size in both directions. Whichever
    // direction produces the smallest result determines how much should be
    // scaled.
    if (pixelArea.width > maxWidth)
      scale = NS_MIN(scale, float(maxWidth) / pixelArea.width);
    if (pixelArea.height > maxHeight)
      scale = NS_MIN(scale, float(maxHeight) / pixelArea.height);

    pixelArea.width = NSToIntFloor(float(pixelArea.width) * scale);
    pixelArea.height = NSToIntFloor(float(pixelArea.height) * scale);

    // adjust the screen position based on the rescaled size
    nscoord left = rootScreenRect.x + pixelArea.x;
    nscoord top = rootScreenRect.y + pixelArea.y;
    aScreenRect->x = NSToIntFloor(aPoint.x - float(aPoint.x - left) * scale);
    aScreenRect->y = NSToIntFloor(aPoint.y - float(aPoint.y - top) * scale);
  }
  else {
    // move aScreenRect to the position of the surface in screen coordinates
    aScreenRect->MoveTo(rootScreenRect.x + pixelArea.x, rootScreenRect.y + pixelArea.y);
  }
  aScreenRect->width = pixelArea.width;
  aScreenRect->height = pixelArea.height;

  gfxImageSurface* surface =
    new gfxImageSurface(gfxIntSize(pixelArea.width, pixelArea.height),
                        gfxImageSurface::ImageFormatARGB32);
  if (!surface || surface->CairoStatus()) {
    delete surface;
    return nsnull;
  }

  // clear the image
  gfxContext context(surface);
  context.SetOperator(gfxContext::OPERATOR_CLEAR);
  context.Rectangle(gfxRect(0, 0, pixelArea.width, pixelArea.height));
  context.Fill();

  nsCOMPtr<nsIRenderingContext> rc;
  deviceContext->CreateRenderingContextInstance(*getter_AddRefs(rc));
  rc->Init(deviceContext, surface);

  if (aRegion) {
    // Convert aRegion from CSS pixels to dev pixels
    nsIntRegion region =
      aRegion->ToAppUnits(nsPresContext::AppUnitsPerCSSPixel())
        .ToOutsidePixels(pc->AppUnitsPerDevPixel());
    rc->SetClipRegion(region, nsClipCombine_kReplace);
  }

  if (resize)
    rc->Scale(scale, scale);

  // translate so that points are relative to the surface area
  rc->Translate(-aArea.x, -aArea.y);

  // temporarily hide the selection so that text is drawn normally. If a
  // selection is being rendered, use that, otherwise use the presshell's
  // selection.
  nsCOMPtr<nsFrameSelection> frameSelection;
  if (aSelection) {
    nsCOMPtr<nsISelectionPrivate> selpriv = do_QueryInterface(aSelection);
    selpriv->GetFrameSelection(getter_AddRefs(frameSelection));
  }
  else {
    frameSelection = FrameSelection();
  }
  PRInt16 oldDisplaySelection = frameSelection->GetDisplaySelection();
  frameSelection->SetDisplaySelection(nsISelectionController::SELECTION_HIDDEN);

  // next, paint each range in the selection
  PRInt32 count = aItems->Length();
  for (PRInt32 i = 0; i < count; i++) {
    RangePaintInfo* rangeInfo = (*aItems)[i];
    // the display lists paint relative to the offset from the reference
    // frame, so translate the rendering context
    nsIRenderingContext::AutoPushTranslation
      translate(rc, rangeInfo->mRootOffset.x, rangeInfo->mRootOffset.y);

    aArea.MoveBy(-rangeInfo->mRootOffset.x, -rangeInfo->mRootOffset.y);
    nsRegion visible(aArea);
    rangeInfo->mList.ComputeVisibilityForRoot(&rangeInfo->mBuilder, &visible);
    rangeInfo->mList.PaintRoot(&rangeInfo->mBuilder, rc, nsDisplayList::PAINT_DEFAULT);
    aArea.MoveBy(rangeInfo->mRootOffset.x, rangeInfo->mRootOffset.y);
  }

  // restore the old selection display state
  frameSelection->SetDisplaySelection(oldDisplaySelection);

  NS_ADDREF(surface);
  return surface;
}

already_AddRefed<gfxASurface>
PresShell::RenderNode(nsIDOMNode* aNode,
                      nsIntRegion* aRegion,
                      nsIntPoint& aPoint,
                      nsIntRect* aScreenRect)
{
  // area will hold the size of the surface needed to draw the node, measured
  // from the root frame.
  nsRect area;
  nsTArray<nsAutoPtr<RangePaintInfo> > rangeItems;

  // nothing to draw if the node isn't in a document
  nsCOMPtr<nsINode> node = do_QueryInterface(aNode);
  if (!node->IsInDoc())
    return nsnull;
  
  nsCOMPtr<nsIDOMRange> range;
  NS_NewRange(getter_AddRefs(range));
  if (NS_FAILED(range->SelectNode(aNode)))
    return nsnull;

  RangePaintInfo* info = CreateRangePaintInfo(range, area, PR_FALSE);
  if (info && !rangeItems.AppendElement(info)) {
    delete info;
    return nsnull;
  }

  if (aRegion) {
    // combine the area with the supplied region
    nsIntRect rrectPixels = aRegion->GetBounds();

    nsRect rrect = rrectPixels.ToAppUnits(nsPresContext::AppUnitsPerCSSPixel());
    area.IntersectRect(area, rrect);
    
    nsPresContext* pc = GetPresContext();
    if (!pc)
      return nsnull;

    // move the region so that it is offset from the topleft corner of the surface
    aRegion->MoveBy(-pc->AppUnitsToDevPixels(area.x),
                    -pc->AppUnitsToDevPixels(area.y));
  }

  return PaintRangePaintInfo(&rangeItems, nsnull, aRegion, area, aPoint,
                             aScreenRect);
}

already_AddRefed<gfxASurface>
PresShell::RenderSelection(nsISelection* aSelection,
                           nsIntPoint& aPoint,
                           nsIntRect* aScreenRect)
{
  // area will hold the size of the surface needed to draw the selection,
  // measured from the root frame.
  nsRect area;
  nsTArray<nsAutoPtr<RangePaintInfo> > rangeItems;

  // iterate over each range and collect them into the rangeItems array.
  // This is done so that the size of selection can be determined so as
  // to allocate a surface area
  PRInt32 numRanges;
  aSelection->GetRangeCount(&numRanges);
  NS_ASSERTION(numRanges > 0, "RenderSelection called with no selection");

  for (PRInt32 r = 0; r < numRanges; r++)
  {
    nsCOMPtr<nsIDOMRange> range;
    aSelection->GetRangeAt(r, getter_AddRefs(range));

    RangePaintInfo* info = CreateRangePaintInfo(range, area, PR_TRUE);
    if (info && !rangeItems.AppendElement(info)) {
      delete info;
      return nsnull;
    }
  }

  return PaintRangePaintInfo(&rangeItems, aSelection, nsnull, area, aPoint,
                             aScreenRect);
}

nsresult
PresShell::AddPrintPreviewBackgroundItem(nsDisplayListBuilder& aBuilder,
                                         nsDisplayList&        aList,
                                         nsIFrame*             aFrame,
                                         const nsRect&         aBounds)
{
  return aList.AppendNewToBottom(new (&aBuilder)
    nsDisplaySolidColor(&aBuilder, aFrame, aBounds, NS_RGB(115, 115, 115)));
}

static PRBool
AddCanvasBackgroundColor(const nsDisplayList& aList, nsIFrame* aCanvasFrame,
                         nscolor aColor)
{
  for (nsDisplayItem* i = aList.GetBottom(); i; i = i->GetAbove()) {
    if (i->GetUnderlyingFrame() == aCanvasFrame &&
        i->GetType() == nsDisplayItem::TYPE_CANVAS_BACKGROUND) {
      nsDisplayCanvasBackground* bg = static_cast<nsDisplayCanvasBackground*>(i);
      bg->SetExtraBackgroundColor(aColor);
      return PR_TRUE;
    }
    nsDisplayList* sublist = i->GetList();
    if (sublist && AddCanvasBackgroundColor(*sublist, aCanvasFrame, aColor))
      return PR_TRUE;
  }
  return PR_FALSE;
}

nsresult PresShell::AddCanvasBackgroundColorItem(nsDisplayListBuilder& aBuilder,
                                                 nsDisplayList&        aList,
                                                 nsIFrame*             aFrame,
                                                 const nsRect&         aBounds,
                                                 nscolor               aBackstopColor,
                                                 PRBool                aForceDraw)
{
  // We don't want to add an item for the canvas background color if the frame
  // (sub)tree we are painting doesn't include any canvas frames. There isn't
  // an easy way to check this directly, but if we check if the root of the
  // (sub)tree we are painting is a canvas frame that should cover us in all
  // cases (it will usually be a viewport frame when we have a canvas frame in
  // the (sub)tree).
  if (!aForceDraw && !nsCSSRendering::IsCanvasFrame(aFrame))
    return NS_OK;

  nscolor bgcolor = NS_ComposeColors(aBackstopColor, mCanvasBackgroundColor);

  // To make layers work better, we want to avoid having a big non-scrolled 
  // color background behind a scrolled transparent background. Instead,
  // we'll try to move the color background into the scrolled content
  // by making nsDisplayCanvasBackground paint it.
  if (!aFrame->GetParent()) {
    nsIScrollableFrame* sf =
      aFrame->PresContext()->PresShell()->GetRootScrollFrameAsScrollable();
    if (sf) {
      nsCanvasFrame* canvasFrame = do_QueryFrame(sf->GetScrolledFrame());
      if (canvasFrame && canvasFrame->IsVisibleForPainting(&aBuilder)) {
        if (AddCanvasBackgroundColor(aList, canvasFrame, bgcolor))
          return NS_OK;
      }
    }
  }

  return aList.AppendNewToBottom(
      new (&aBuilder) nsDisplaySolidColor(&aBuilder, aFrame, aBounds, bgcolor));
}

static PRBool IsTransparentContainerElement(nsPresContext* aPresContext)
{
  nsCOMPtr<nsISupports> container = aPresContext->GetContainerInternal();
  nsCOMPtr<nsIDocShellTreeItem> docShellItem = do_QueryInterface(container);
  nsCOMPtr<nsPIDOMWindow> pwin(do_GetInterface(docShellItem));
  if (!pwin)
    return PR_FALSE;
  nsCOMPtr<nsIContent> containerElement =
    do_QueryInterface(pwin->GetFrameElementInternal());
  return containerElement &&
         containerElement->HasAttr(kNameSpaceID_None, nsGkAtoms::transparent);
}

void PresShell::UpdateCanvasBackground()
{
  // If we have a frame tree and it has style information that
  // specifies the background color of the canvas, update our local
  // cache of that color.
  nsIFrame* rootStyleFrame = FrameConstructor()->GetRootElementStyleFrame();
  if (rootStyleFrame) {
    nsStyleContext* bgStyle =
      nsCSSRendering::FindRootFrameBackground(rootStyleFrame);
    // XXX We should really be passing the canvasframe, not the root element
    // style frame but we don't have access to the canvasframe here. It isn't
    // a problem because only a few frames can return something other than true
    // and none of them would be a canvas frame or root element style frame.
    mCanvasBackgroundColor =
      nsCSSRendering::DetermineBackgroundColor(mPresContext, bgStyle,
                                               rootStyleFrame);
    if (GetPresContext()->IsRootContentDocument() &&
        !IsTransparentContainerElement(mPresContext)) {
      mCanvasBackgroundColor =
        NS_ComposeColors(mPresContext->DefaultBackgroundColor(), mCanvasBackgroundColor);
    }
  }

  // If the root element of the document (ie html) has style 'display: none'
  // then the document's background color does not get drawn; cache the
  // color we actually draw.
  if (!FrameConstructor()->GetRootElementFrame()) {
    mCanvasBackgroundColor = mPresContext->DefaultBackgroundColor();
  }
}

nscolor PresShell::ComputeBackstopColor(nsIView* aDisplayRoot)
{
  nsIWidget* widget = aDisplayRoot->GetWidget();
  if (widget && widget->GetTransparencyMode() != eTransparencyOpaque) {
    // Within a transparent widget, so the backstop color must be
    // totally transparent.
    return NS_RGBA(0,0,0,0);
  }
  // Within an opaque widget (or no widget at all), so the backstop
  // color must be totally opaque. The user's default background
  // as reported by the prescontext is guaranteed to be opaque.
  return GetPresContext()->DefaultBackgroundColor();
}

struct PaintParams {
  nsIFrame* mFrame;
  nsPoint mOffsetToWidget;
  const nsRegion* mDirtyRegion;
  nscolor mBackgroundColor;
};

LayerManager* PresShell::GetLayerManager()
{
  NS_ASSERTION(mViewManager, "Should have view manager");

  nsIView* rootView;
  if (NS_SUCCEEDED(mViewManager->GetRootView(rootView)) && rootView) {
    if (nsIWidget* widget = rootView->GetWidget()) {
      return widget->GetLayerManager();
    }
  }
  return nsnull;
}

static void DrawThebesLayer(ThebesLayer* aLayer,
                            gfxContext* aContext,
                            const nsIntRegion& aRegionToDraw,
                            const nsIntRegion& aRegionToInvalidate,
                            void* aCallbackData)
{
  PaintParams* params = static_cast<PaintParams*>(aCallbackData);
  nsIFrame* frame = params->mFrame;
  if (frame) {
    // We're drawing into a child window.
    nsIDeviceContext* devCtx = frame->PresContext()->DeviceContext();
    nsCOMPtr<nsIRenderingContext> rc;
    nsresult rv = devCtx->CreateRenderingContextInstance(*getter_AddRefs(rc));
    if (NS_SUCCEEDED(rv)) {
      rc->Init(devCtx, aContext);
      nsIRenderingContext::AutoPushTranslation
        push(rc, params->mOffsetToWidget.x, params->mOffsetToWidget.y);
      nsLayoutUtils::PaintFrame(rc, frame, *params->mDirtyRegion,
                                params->mBackgroundColor,
                                nsLayoutUtils::PAINT_WIDGET_LAYERS);
    }
  } else {
    aContext->NewPath();
    aContext->SetColor(gfxRGBA(params->mBackgroundColor));
    nsIntRect dirtyRect = aRegionToDraw.GetBounds();
    aContext->Rectangle(
      gfxRect(dirtyRect.x, dirtyRect.y, dirtyRect.width, dirtyRect.height));
    aContext->Fill();
  }
}

NS_IMETHODIMP
PresShell::Paint(nsIView*           aDisplayRoot,
                 nsIView*           aViewToPaint,
                 nsIWidget*         aWidgetToPaint,
                 const nsRegion&    aDirtyRegion,
                 const nsIntRegion& aIntDirtyRegion,
                 PRBool             aPaintDefaultBackground,
                 PRBool             aWillSendDidPaint)
{
#ifdef NS_FUNCTION_TIMER
  NS_TIME_FUNCTION_DECLARE_DOCURL;
  const nsRect& bounds__ = aDirtyRegion.GetBounds();
  NS_TIME_FUNCTION_MIN_FMT(1.0, "%s (line %d) (document: %s, dirty rect: (<%f, %f>, <%f, %f>)",
                           MOZ_FUNCTION_NAME, __LINE__, docURL__.get(),
                           NSCoordToFloat(bounds__.x),
                           NSCoordToFloat(bounds__.y),
                           NSCoordToFloat(bounds__.XMost()),
                           NSCoordToFloat(bounds__.YMost()));
#endif

  nsPresContext* presContext = GetPresContext();
  AUTO_LAYOUT_PHASE_ENTRY_POINT(presContext, Paint);

  NS_ASSERTION(!mIsDestroying, "painting a destroyed PresShell");
  NS_ASSERTION(aDisplayRoot, "null view");
  NS_ASSERTION(aViewToPaint, "null view");
  NS_ASSERTION(aWidgetToPaint, "Can't paint without a widget");

  nscolor bgcolor = ComputeBackstopColor(aDisplayRoot);

  nsIFrame* frame = aPaintDefaultBackground
      ? nsnull : static_cast<nsIFrame*>(aDisplayRoot->GetClientData());

  if (frame && aViewToPaint == aDisplayRoot) {
    // Defer invalidates that are triggered during painting, and discard
    // invalidates of areas that are already being repainted.
    // The layer system can trigger invalidates during painting
    // (see FrameLayerBuilder).
    frame->BeginDeferringInvalidatesForDisplayRoot(aDirtyRegion);

    // We can paint directly into the widget using its layer manager.
    // When we get rid of child widgets, this will be the only path we
    // need. (aPaintDefaultBackground will never be needed since the
    // chrome can always paint a default background.)
    nsLayoutUtils::PaintFrame(nsnull, frame, aDirtyRegion, bgcolor,
                              nsLayoutUtils::PAINT_WIDGET_LAYERS);

    frame->EndDeferringInvalidatesForDisplayRoot();
    return NS_OK;
  }

  if (frame) {
    // Defer invalidates that are triggered during painting, and discard
    // invalidates of areas that are already being repainted.
    frame->BeginDeferringInvalidatesForDisplayRoot(aDirtyRegion);
  }

  LayerManager* layerManager = aWidgetToPaint->GetLayerManager();
  NS_ASSERTION(layerManager, "Must be in paint event");

  layerManager->BeginTransaction();
  nsRefPtr<ThebesLayer> root = layerManager->CreateThebesLayer();
  if (root) {
    root->SetVisibleRegion(aIntDirtyRegion);
    layerManager->SetRoot(root);
  }
  if (!frame) {
    bgcolor = NS_ComposeColors(bgcolor, mCanvasBackgroundColor);
  }
  PaintParams params =
    { frame,
      aDisplayRoot->GetOffsetToWidget(aWidgetToPaint),
      &aDirtyRegion,
      bgcolor };
  layerManager->EndTransaction(DrawThebesLayer, &params);

  if (frame) {
    frame->EndDeferringInvalidatesForDisplayRoot();
  }
  return NS_OK;
}

// static
void
nsIPresShell::SetCapturingContent(nsIContent* aContent, PRUint8 aFlags)
{
  NS_IF_RELEASE(gCaptureInfo.mContent);

  // only set capturing content if allowed or the CAPTURE_IGNOREALLOWED flag
  // is used
  if ((aFlags & CAPTURE_IGNOREALLOWED) || gCaptureInfo.mAllowed) {
    if (aContent) {
      NS_ADDREF(gCaptureInfo.mContent = aContent);
    }
    gCaptureInfo.mRetargetToElement = (aFlags & CAPTURE_RETARGETTOELEMENT) != 0;
    gCaptureInfo.mPreventDrag = (aFlags & CAPTURE_PREVENTDRAG) != 0;
  }
}

nsIFrame*
PresShell::GetCurrentEventFrame()
{
  if (NS_UNLIKELY(mIsDestroying)) {
    return nsnull;
  }
    
  if (!mCurrentEventFrame && mCurrentEventContent) {
    // Make sure the content still has a document reference. If not,
    // then we assume it is no longer in the content tree and the
    // frame shouldn't get an event, nor should we even assume its
    // safe to try and find the frame.
    if (mCurrentEventContent->GetDocument()) {
      mCurrentEventFrame = mCurrentEventContent->GetPrimaryFrame();
    }
  }

  return mCurrentEventFrame;
}

nsIFrame*
PresShell::GetEventTargetFrame()
{
  return GetCurrentEventFrame();
}

already_AddRefed<nsIContent>
PresShell::GetEventTargetContent(nsEvent* aEvent)
{
  nsIContent* content = nsnull;

  if (mCurrentEventContent) {
    content = mCurrentEventContent;
    NS_IF_ADDREF(content);
  } else {
    nsIFrame* currentEventFrame = GetCurrentEventFrame();
    if (currentEventFrame) {
      currentEventFrame->GetContentForEvent(mPresContext, aEvent, &content);
    } else {
      content = nsnull;
    }
  }
  return content;
}

void
PresShell::PushCurrentEventInfo(nsIFrame* aFrame, nsIContent* aContent)
{
  if (mCurrentEventFrame || mCurrentEventContent) {
    mCurrentEventFrameStack.InsertElementAt(0, mCurrentEventFrame);
    mCurrentEventContentStack.InsertObjectAt(mCurrentEventContent, 0);
  }
  mCurrentEventFrame = aFrame;
  mCurrentEventContent = aContent;
}

void
PresShell::PopCurrentEventInfo()
{
  mCurrentEventFrame = nsnull;
  mCurrentEventContent = nsnull;

  if (0 != mCurrentEventFrameStack.Length()) {
    mCurrentEventFrame = mCurrentEventFrameStack.ElementAt(0);
    mCurrentEventFrameStack.RemoveElementAt(0);
    mCurrentEventContent = mCurrentEventContentStack.ObjectAt(0);
    mCurrentEventContentStack.RemoveObjectAt(0);
  }
}

PRBool PresShell::InZombieDocument(nsIContent *aContent)
{
  // If a content node points to a null document, or the document is not
  // attached to a window, then it is possibly in a zombie document,
  // about to be replaced by a newly loading document.
  // Such documents cannot handle DOM events.
  // It might actually be in a node not attached to any document,
  // in which case there is not parent presshell to retarget it to.
  nsIDocument *doc = aContent->GetDocument();
  return !doc || !doc->GetWindow();
}

already_AddRefed<nsPIDOMWindow>
PresShell::GetRootWindow()
{
  nsCOMPtr<nsPIDOMWindow> window =
    do_QueryInterface(mDocument->GetWindow());
  if (window) {
    nsCOMPtr<nsPIDOMWindow> rootWindow = window->GetPrivateRoot();
    NS_ASSERTION(rootWindow, "nsPIDOMWindow::GetPrivateRoot() returns NULL");
    return rootWindow.forget();
  }

  // If we don't have DOM window, we're zombie, we should find the root window
  // with our parent shell.
  nsCOMPtr<nsIPresShell> parent = GetParentPresShell();
  NS_ENSURE_TRUE(parent, nsnull);
  return parent->GetRootWindow();
}

already_AddRefed<nsIPresShell>
PresShell::GetParentPresShell()
{
  NS_ENSURE_TRUE(mPresContext, nsnull);
  nsCOMPtr<nsISupports> container = mPresContext->GetContainer();
  if (!container) {
    container = do_QueryReferent(mForwardingContainer);
  }

  // Now, find the parent pres shell and send the event there
  nsCOMPtr<nsIDocShellTreeItem> treeItem = do_QueryInterface(container);
  // Might have gone away, or never been around to start with
  NS_ENSURE_TRUE(treeItem, nsnull);

  nsCOMPtr<nsIDocShellTreeItem> parentTreeItem;
  treeItem->GetParent(getter_AddRefs(parentTreeItem));
  nsCOMPtr<nsIDocShell> parentDocShell = do_QueryInterface(parentTreeItem);
  NS_ENSURE_TRUE(parentDocShell && treeItem != parentTreeItem, nsnull);

  nsIPresShell* parentPresShell = nsnull;
  parentDocShell->GetPresShell(&parentPresShell);
  return parentPresShell;
}

nsresult
PresShell::RetargetEventToParent(nsGUIEvent*     aEvent,
                                 nsEventStatus*  aEventStatus)
{
  // Send this events straight up to the parent pres shell.
  // We do this for keystroke events in zombie documents or if either a frame
  // or a root content is not present.
  // That way at least the UI key bindings can work.

  nsCOMPtr<nsIPresShell> kungFuDeathGrip(this);
  nsCOMPtr<nsIPresShell> parentPresShell = GetParentPresShell();
  NS_ENSURE_TRUE(parentPresShell, NS_ERROR_FAILURE);
  nsCOMPtr<nsIViewObserver> parentViewObserver = 
    do_QueryInterface(parentPresShell);
  if (!parentViewObserver) {
    return NS_ERROR_FAILURE;
  }

  // Fake the event as though it'ss from the parent pres shell's root view.
  nsIView *parentRootView;
  parentPresShell->GetViewManager()->GetRootView(parentRootView);
  
  sDontRetargetEvents = PR_TRUE;
  nsresult rv = parentViewObserver->HandleEvent(parentRootView, aEvent, aEventStatus);
  sDontRetargetEvents = PR_FALSE;
  return rv;
}

void
PresShell::DisableNonTestMouseEvents(PRBool aDisable)
{
  sDisableNonTestMouseEvents = aDisable;
}

already_AddRefed<nsPIDOMWindow>
PresShell::GetFocusedDOMWindowInOurWindow()
{
  nsCOMPtr<nsPIDOMWindow> rootWindow = GetRootWindow();
  NS_ENSURE_TRUE(rootWindow, nsnull);
  nsPIDOMWindow* focusedWindow;
  nsFocusManager::GetFocusedDescendant(rootWindow, PR_TRUE, &focusedWindow);
  return focusedWindow;
}

NS_IMETHODIMP
PresShell::HandleEvent(nsIView         *aView,
                       nsGUIEvent*     aEvent,
                       nsEventStatus*  aEventStatus)
{
  NS_ASSERTION(aView, "null view");

  if (mIsDestroying || !nsContentUtils::IsSafeToRunScript() ||
      (sDisableNonTestMouseEvents && NS_IS_MOUSE_EVENT(aEvent) &&
       !(aEvent->flags & NS_EVENT_FLAG_SYNTHETIC_TEST_EVENT))) {
    return NS_OK;
  }

  NS_TIME_FUNCTION_MIN(1.0);

#ifdef ACCESSIBILITY
  if (aEvent->eventStructType == NS_ACCESSIBLE_EVENT) {
    // Accessibility events come through OS requests and not from scripts,
    // so it is safe to handle here
    return HandleEventInternal(aEvent, aView, aEventStatus);
  }
#endif

  nsIContent* capturingContent =
    NS_IS_MOUSE_EVENT(aEvent) ? GetCapturingContent() : nsnull;

  nsCOMPtr<nsIDocument> retargetEventDoc;
  if (!sDontRetargetEvents) {
    // key and IME related events should not cross top level window boundary.
    // Basically, such input events should be fired only on focused widget.
    // However, some IMEs might need to clean up composition after focused
    // window is deactivated.  And also some tests on MozMill want to test key
    // handling on deactivated window because MozMill window can be activated
    // during tests.  So, there is no merit the events should be redirected to
    // active window.  So, the events should be handled on the last focused
    // content in the last focused DOM window in same top level window.
    // Note, if no DOM window has been focused yet, we can discard the events.
    if (NS_IsEventTargetedAtFocusedWindow(aEvent)) {
      nsCOMPtr<nsPIDOMWindow> window = GetFocusedDOMWindowInOurWindow();
      // No DOM window in same top level window has not been focused yet,
      // discard the events.
      if (!window) {
        return NS_OK;
      }

      retargetEventDoc = do_QueryInterface(window->GetExtantDocument());
      if (!retargetEventDoc)
        return NS_OK;
    } else if (capturingContent) {
      // if the mouse is being captured then retarget the mouse event at the
      // document that is being captured.
      retargetEventDoc = capturingContent->GetCurrentDoc();
    }

    if (retargetEventDoc) {
      nsIPresShell* presShell = retargetEventDoc->GetShell();
      if (!presShell)
        return NS_OK;

      if (presShell != this) {
        nsCOMPtr<nsIViewObserver> viewObserver = do_QueryInterface(presShell);
        if (!viewObserver)
          return NS_ERROR_FAILURE;

        nsIView *view;
        presShell->GetViewManager()->GetRootView(view);
        sDontRetargetEvents = PR_TRUE;
        nsresult rv = viewObserver->HandleEvent(view, aEvent, aEventStatus);
        sDontRetargetEvents = PR_FALSE;
        return rv;
      }
    }
  }

  // Check for a theme change up front, since the frame type is irrelevant
  if (aEvent->message == NS_THEMECHANGED && mPresContext) {
    mPresContext->ThemeChanged();
    return NS_OK;
  }

  if (aEvent->message == NS_UISTATECHANGED && mDocument) {
    nsPIDOMWindow* win = mDocument->GetWindow();
    if (win) {
      nsUIStateChangeEvent* event = (nsUIStateChangeEvent*)aEvent;
      win->SetKeyboardIndicators(event->showAccelerators, event->showFocusRings);
    }
    return NS_OK;
  }

  // Check for a system color change up front, since the frame type is
  // irrelevant
  if ((aEvent->message == NS_SYSCOLORCHANGED) && mPresContext) {
    nsIViewManager* vm = GetViewManager();
    if (vm) {
      // Only dispatch system color change when the message originates from
      // from the root views widget. This is necessary to prevent us from 
      // dispatching the SysColorChanged notification for each child window 
      // which may be redundant.
      nsIView *view;
      vm->GetRootView(view);
      if (view == aView) {
        *aEventStatus = nsEventStatus_eConsumeDoDefault;
        mPresContext->SysColorChanged();
        return NS_OK;
      }
    }
    return NS_OK;
  }

  if (aEvent->eventStructType == NS_KEY_EVENT &&
      mDocument && mDocument->EventHandlingSuppressed()) {
    if (aEvent->message == NS_KEY_DOWN) {
      mNoDelayedKeyEvents = PR_TRUE;
    } else if (!mNoDelayedKeyEvents) {
      nsDelayedEvent* event =
        new nsDelayedKeyEvent(static_cast<nsKeyEvent*>(aEvent));
      if (event && !mDelayedEvents.AppendElement(event)) {
        delete event;
      }
    }
    return NS_OK;
  }

  nsIFrame* frame = static_cast<nsIFrame*>(aView->GetClientData());
  PRBool dispatchUsingCoordinates = NS_IsEventUsingCoordinates(aEvent);

  // if this event has no frame, we need to retarget it at a parent
  // view that has a frame.
  if (!frame &&
      (dispatchUsingCoordinates || NS_IS_KEY_EVENT(aEvent) ||
       NS_IS_IME_RELATED_EVENT(aEvent) || NS_IS_NON_RETARGETED_PLUGIN_EVENT(aEvent) ||
       aEvent->message == NS_PLUGIN_ACTIVATE)) {
    nsIView* targetView = aView;
    while (targetView && !targetView->GetClientData()) {
      targetView = targetView->GetParent();
    }
    
    if (targetView) {
      aView = targetView;
      frame = static_cast<nsIFrame*>(aView->GetClientData());
    }
  }

  if (dispatchUsingCoordinates) {
    NS_WARN_IF_FALSE(frame, "Nothing to handle this event!");
    if (!frame)
      return NS_OK;

    nsPresContext* framePresContext = frame->PresContext();
    nsPresContext* rootPresContext = framePresContext->GetRootPresContext();
    NS_ASSERTION(rootPresContext == mPresContext->GetRootPresContext(),
                 "How did we end up outside the connected prescontext/viewmanager hierarchy?"); 
    // If we aren't starting our event dispatch from the root frame of the root prescontext,
    // then someone must be capturing the mouse. In that case we don't want to search the popup
    // list.
    if (framePresContext == rootPresContext &&
        frame == FrameManager()->GetRootFrame()) {
      nsIFrame* popupFrame =
        nsLayoutUtils::GetPopupFrameForEventCoordinates(rootPresContext, aEvent);
      // If the popupFrame is an ancestor of the 'frame', the frame should
      // handle the event, otherwise, the popup should handle it.
      if (popupFrame &&
          !nsContentUtils::ContentIsCrossDocDescendantOf(
             framePresContext->GetPresShell()->GetDocument(),
             popupFrame->GetContent())) {
        frame = popupFrame;
      }
    }

    PRBool captureRetarget = PR_FALSE;
    if (capturingContent) {
      // If a capture is active, determine if the docshell is visible. If not,
      // clear the capture and target the mouse event normally instead. This
      // would occur if the mouse button is held down while a tab change occurs.
      // If the docshell is visible, look for a scrolling container.
      PRBool vis;
      nsCOMPtr<nsISupports> supports = mPresContext->GetContainer();
      nsCOMPtr<nsIBaseWindow> baseWin(do_QueryInterface(supports));
      if (baseWin && NS_SUCCEEDED(baseWin->GetVisibility(&vis)) && vis) {
        captureRetarget = gCaptureInfo.mRetargetToElement;
        if (!captureRetarget) {
          // A check was already done above to ensure that capturingContent is
          // in this presshell.
          NS_ASSERTION(capturingContent->GetCurrentDoc() == GetDocument(),
                       "Unexpected document");
          nsIFrame* captureFrame = capturingContent->GetPrimaryFrame();
          if (captureFrame) {
            if (capturingContent->Tag() == nsGkAtoms::select &&
                capturingContent->IsHTML()) {
              // a dropdown <select> has a child in its selectPopupList and we should
              // capture on that instead.
              nsIFrame* childFrame = captureFrame->GetChildList(nsGkAtoms::selectPopupList).FirstChild();
              if (childFrame) {
                captureFrame = childFrame;
              }
            }

            // scrollable frames should use the scrolling container as
            // the root instead of the document
            nsIScrollableFrame* scrollFrame = do_QueryFrame(captureFrame);
            if (scrollFrame) {
              frame = scrollFrame->GetScrolledFrame();
            }
          }
        }
      }
      else {
        ClearMouseCapture(nsnull);
        capturingContent = nsnull;
      }
    }

    // Get the frame at the event point. However, don't do this if we're
    // capturing and retargeting the event because the captured frame will
    // be used instead below.
    if (!captureRetarget) {
      nsPoint eventPoint
          = nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, frame);
      {
        PRBool ignoreRootScrollFrame = PR_FALSE;
        if (aEvent->eventStructType == NS_MOUSE_EVENT) {
          ignoreRootScrollFrame = static_cast<nsMouseEvent*>(aEvent)->ignoreRootScrollFrame;
        }
        nsIFrame* target = nsLayoutUtils::GetFrameForPoint(frame, eventPoint,
                                                           PR_FALSE, ignoreRootScrollFrame);
        if (target) {
          frame = target;
        }
      }
    }

    // if a node is capturing the mouse, check if the event needs to be
    // retargeted at the capturing content instead. This will be the case when
    // capture retargeting is being used, no frame was found or the frame's
    // content is not a descendant of the capturing content.
    if (capturingContent &&
        (gCaptureInfo.mRetargetToElement || !frame->GetContent() ||
         !nsContentUtils::ContentIsCrossDocDescendantOf(frame->GetContent(),
                                                        capturingContent))) {
      // A check was already done above to ensure that capturingContent is
      // in this presshell.
      NS_ASSERTION(capturingContent->GetCurrentDoc() == GetDocument(),
                   "Unexpected document");
      nsIFrame* capturingFrame = capturingContent->GetPrimaryFrame();
      if (capturingFrame) {
        frame = capturingFrame;
        aView = frame->GetClosestView();
      }
    }

    // Suppress mouse event if it's being targeted at an element inside
    // a document which needs events suppressed
    if (aEvent->eventStructType == NS_MOUSE_EVENT &&
        frame->PresContext()->Document()->EventHandlingSuppressed()) {
      if (aEvent->message == NS_MOUSE_BUTTON_DOWN) {
        mNoDelayedMouseEvents = PR_TRUE;
      } else if (!mNoDelayedMouseEvents && aEvent->message == NS_MOUSE_BUTTON_UP) {
        nsDelayedEvent* event =
          new nsDelayedMouseEvent(static_cast<nsMouseEvent*>(aEvent));
        if (!mDelayedEvents.AppendElement(event)) {
          delete event;
        }
      }

      return NS_OK;
    }

    PresShell* shell =
        static_cast<PresShell*>(frame->PresContext()->PresShell());
    if (shell != this) {
      // Handle the event in the correct shell.
      // Prevent deletion until we're done with event handling (bug 336582).
      nsCOMPtr<nsIPresShell> kungFuDeathGrip(shell);
      nsIView* subshellRootView;
      shell->GetViewManager()->GetRootView(subshellRootView);
      // We pass the subshell's root view as the view to start from. This is
      // the only correct alternative; if the event was captured then it
      // must have been captured by us or some ancestor shell and we
      // now ask the subshell to dispatch it normally.
      return shell->HandlePositionedEvent(subshellRootView, frame,
                                          aEvent, aEventStatus);
    }

    return HandlePositionedEvent(aView, frame, aEvent, aEventStatus);
  }
  
  nsresult rv = NS_OK;
  
  if (frame) {
    PushCurrentEventInfo(nsnull, nsnull);

    // key and IME related events go to the focused frame in this DOM window.
    if (NS_IsEventTargetedAtFocusedContent(aEvent)) {
      NS_ASSERTION(mDocument, "mDocument is null");
      nsCOMPtr<nsPIDOMWindow> window =
        do_QueryInterface(mDocument->GetWindow());
      nsCOMPtr<nsPIDOMWindow> focusedWindow;
      mCurrentEventContent =
        nsFocusManager::GetFocusedDescendant(window, PR_FALSE,
                                             getter_AddRefs(focusedWindow));

      // otherwise, if there is no focused content or the focused content has
      // no frame, just use the root content. This ensures that key events
      // still get sent to the window properly if nothing is focused or if a
      // frame goes away while it is focused.
      if (!mCurrentEventContent || !GetCurrentEventFrame())
        mCurrentEventContent = mDocument->GetRootElement();

      if (aEvent->message == NS_KEY_DOWN) {
        NS_IF_RELEASE(gKeyDownTarget);
        NS_IF_ADDREF(gKeyDownTarget = mCurrentEventContent);
      }
      else if ((aEvent->message == NS_KEY_PRESS || aEvent->message == NS_KEY_UP) &&
               gKeyDownTarget) {
        // If a different element is now focused for the keypress/keyup event
        // than what was focused during the keydown event, check if the new
        // focused element is not in a chrome document any more, and if so,
        // retarget the event back at the keydown target. This prevents a
        // content area from grabbing the focus from chrome in-between key
        // events.
        if (mCurrentEventContent &&
            nsContentUtils::IsChromeDoc(gKeyDownTarget->GetCurrentDoc()) &&
            !nsContentUtils::IsChromeDoc(mCurrentEventContent->GetCurrentDoc())) {
          mCurrentEventContent = gKeyDownTarget;
        }

        if (aEvent->message == NS_KEY_UP) {
          NS_RELEASE(gKeyDownTarget);
        }
      }

      mCurrentEventFrame = nsnull;
        
      if (!mCurrentEventContent || !GetCurrentEventFrame() ||
          InZombieDocument(mCurrentEventContent)) {
        rv = RetargetEventToParent(aEvent, aEventStatus);
        PopCurrentEventInfo();
        return rv;
      }
    } else {
      mCurrentEventFrame = frame;
    }
    if (GetCurrentEventFrame()) {
      rv = HandleEventInternal(aEvent, aView, aEventStatus);
    }
  
#ifdef NS_DEBUG
    ShowEventTargetDebug();
#endif
    PopCurrentEventInfo();
  } else {
    // Activation events need to be dispatched even if no frame was found, since
    // we don't want the focus to be out of sync.

    if (!NS_EVENT_NEEDS_FRAME(aEvent)) {
      mCurrentEventFrame = nsnull;
      return HandleEventInternal(aEvent, aView, aEventStatus);
    }
    else if (NS_IS_KEY_EVENT(aEvent)) {
      // Keypress events in new blank tabs should not be completely thrown away.
      // Retarget them -- the parent chrome shell might make use of them.
      return RetargetEventToParent(aEvent, aEventStatus);
    }
  }

  return rv;
}

#ifdef NS_DEBUG
void
PresShell::ShowEventTargetDebug()
{
  if (nsFrame::GetShowEventTargetFrameBorder() &&
      GetCurrentEventFrame()) {
    if (mDrawEventTargetFrame) {
      mDrawEventTargetFrame->Invalidate(
          nsRect(nsPoint(0, 0), mDrawEventTargetFrame->GetSize()));
    }

    mDrawEventTargetFrame = mCurrentEventFrame;
    mDrawEventTargetFrame->Invalidate(
        nsRect(nsPoint(0, 0), mDrawEventTargetFrame->GetSize()));
  }
}
#endif

nsresult
PresShell::HandlePositionedEvent(nsIView*       aView,
                                 nsIFrame*      aTargetFrame,
                                 nsGUIEvent*    aEvent,
                                 nsEventStatus* aEventStatus)
{
  nsresult rv = NS_OK;
  
  PushCurrentEventInfo(nsnull, nsnull);
  
  mCurrentEventFrame = aTargetFrame;

  if (mCurrentEventFrame) {
    nsCOMPtr<nsIContent> targetElement;
    mCurrentEventFrame->GetContentForEvent(mPresContext, aEvent,
                                           getter_AddRefs(targetElement));

    // If there is no content for this frame, target it anyway.  Some
    // frames can be targeted but do not have content, particularly
    // windows with scrolling off.
    if (targetElement) {
      // Bug 103055, bug 185889: mouse events apply to *elements*, not all
      // nodes.  Thus we get the nearest element parent here.
      // XXX we leave the frame the same even if we find an element
      // parent, so that the text frame will receive the event (selection
      // and friends are the ones who care about that anyway)
      //
      // We use weak pointers because during this tight loop, the node
      // will *not* go away.  And this happens on every mousemove.
      while (targetElement && !targetElement->IsElement()) {
        targetElement = targetElement->GetParent();
      }

      // If we found an element, target it.  Otherwise, target *nothing*.
      if (!targetElement) {
        mCurrentEventContent = nsnull;
        mCurrentEventFrame = nsnull;
      } else if (targetElement != mCurrentEventContent) {
        mCurrentEventContent = targetElement;
      }
    }
  }

  if (GetCurrentEventFrame()) {
    rv = HandleEventInternal(aEvent, aView, aEventStatus);
  }

#ifdef NS_DEBUG
  ShowEventTargetDebug();
#endif
  PopCurrentEventInfo();
  return rv;
}

nsresult
PresShell::HandleEventWithTarget(nsEvent* aEvent, nsIFrame* aFrame,
                                 nsIContent* aContent, nsEventStatus* aStatus)
{
  PushCurrentEventInfo(aFrame, aContent);
  nsresult rv = HandleEventInternal(aEvent, nsnull, aStatus);
  PopCurrentEventInfo();
  return rv;
}

static inline PRBool
IsSynthesizedMouseEvent(nsEvent* aEvent)
{
  return aEvent->eventStructType == NS_MOUSE_EVENT &&
         static_cast<nsMouseEvent*>(aEvent)->reason != nsMouseEvent::eReal;
}

static PRBool CanHandleContextMenuEvent(nsMouseEvent* aMouseEvent,
                                        nsIFrame* aFrame)
{
#if defined(XP_MACOSX) && defined(MOZ_XUL)
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm) {
    nsIFrame* popupFrame = pm->GetTopPopup(ePopupTypeMenu);
    if (popupFrame) {
      // context menus should not be opened while another menu is open on Mac,
      // so return false so that the event is not fired.
      if (aMouseEvent->context == nsMouseEvent::eContextMenuKey) {
        return PR_FALSE;
      } else if (aMouseEvent->widget) {
         nsWindowType windowType;
         aMouseEvent->widget->GetWindowType(windowType);
         if (windowType == eWindowType_popup) {
           for (nsIFrame* current = aFrame; current;
                current = nsLayoutUtils::GetCrossDocParentFrame(current)) {
             if (current->GetType() == nsGkAtoms::menuPopupFrame) {
               return PR_FALSE;
             }
           }
         }
      }
    }
  }
#endif
  return PR_TRUE;
}

nsresult
PresShell::HandleEventInternal(nsEvent* aEvent, nsIView *aView,
                               nsEventStatus* aStatus)
{
  NS_TIME_FUNCTION_MIN(1.0);

#ifdef ACCESSIBILITY
  if (aEvent->eventStructType == NS_ACCESSIBLE_EVENT)
  {
    nsAccessibleEvent *accEvent = static_cast<nsAccessibleEvent*>(aEvent);
    accEvent->mAccessible = nsnull;

    nsCOMPtr<nsIAccessibilityService> accService =
      do_GetService("@mozilla.org/accessibilityService;1");
    if (accService) {
      nsCOMPtr<nsISupports> container = mPresContext->GetContainer();
      if (!container) {
        // This presshell is not active. This often happens when a
        // preshell is being held onto for fastback.
        return NS_OK;
      }

      accEvent->mAccessible = accService->GetAccessibleInShell(mDocument, this);

      // Ensure this is set in case a11y was activated before any
      // nsPresShells existed to observe "a11y-init-or-shutdown" topic
      gIsAccessibilityActive = PR_TRUE;
      return NS_OK;
    }
  }
#endif

  nsCOMPtr<nsIEventStateManager> manager = mPresContext->EventStateManager();
  nsresult rv = NS_OK;

  if (!NS_EVENT_NEEDS_FRAME(aEvent) || GetCurrentEventFrame()) {
    PRBool isHandlingUserInput = PR_FALSE;

    if (NS_IS_TRUSTED_EVENT(aEvent)) {
      switch (aEvent->message) {
      case NS_MOUSE_BUTTON_DOWN:
      case NS_MOUSE_BUTTON_UP:
      case NS_KEY_PRESS:
      case NS_KEY_DOWN:
      case NS_KEY_UP:
        isHandlingUserInput = PR_TRUE;
        break;
      case NS_DRAGDROP_DROP:
        nsCOMPtr<nsIDragSession> session = nsContentUtils::GetDragSession();
        if (session) {
          PRBool onlyChromeDrop = PR_FALSE;
          session->GetOnlyChromeDrop(&onlyChromeDrop);
          if (onlyChromeDrop) {
            aEvent->flags |= NS_EVENT_FLAG_ONLY_CHROME_DISPATCH;
          }
        }
        break;
      }
    }

    if (aEvent->message == NS_CONTEXTMENU) {
      nsMouseEvent* me = static_cast<nsMouseEvent*>(aEvent);
      if (!CanHandleContextMenuEvent(me, GetCurrentEventFrame())) {
        return NS_OK;
      }
      if (me->context == nsMouseEvent::eContextMenuKey &&
          !AdjustContextMenuKeyEvent(me)) {
        return NS_OK;
      }
    }                                

    nsAutoHandlingUserInputStatePusher userInpStatePusher(isHandlingUserInput,
                                                          aEvent->message == NS_MOUSE_BUTTON_DOWN);

    nsAutoPopupStatePusher popupStatePusher(nsDOMEvent::GetEventPopupControlState(aEvent));

    // FIXME. If the event was reused, we need to clear the old target,
    // bug 329430
    aEvent->target = nsnull;

    nsWeakView weakView(aView);
    // 1. Give event to event manager for pre event state changes and
    //    generation of synthetic events.
    rv = manager->PreHandleEvent(mPresContext, aEvent, mCurrentEventFrame,
                                 aStatus, aView);

    // 2. Give event to the DOM for third party and JS use.
    if (GetCurrentEventFrame() && NS_SUCCEEDED(rv)) {
      PRBool wasHandlingKeyBoardEvent =
        nsContentUtils::IsHandlingKeyBoardEvent();
      if (aEvent->eventStructType == NS_KEY_EVENT) {
        nsContentUtils::SetIsHandlingKeyBoardEvent(PR_TRUE);
      }
      // We want synthesized mouse moves to cause mouseover and mouseout
      // DOM events (PreHandleEvent above), but not mousemove DOM events.
      // Synthesized button up events also do not cause DOM events
      // because they do not have a reliable refPoint.
      if (!IsSynthesizedMouseEvent(aEvent)) {
        nsPresShellEventCB eventCB(this);
        if (mCurrentEventContent) {
          nsEventDispatcher::Dispatch(mCurrentEventContent, mPresContext,
                                      aEvent, nsnull, aStatus, &eventCB);
        }
        else {
          nsCOMPtr<nsIContent> targetContent;
          rv = mCurrentEventFrame->GetContentForEvent(mPresContext, aEvent,
                                                      getter_AddRefs(targetContent));
          if (NS_SUCCEEDED(rv) && targetContent) {
            nsEventDispatcher::Dispatch(targetContent, mPresContext, aEvent,
                                        nsnull, aStatus, &eventCB);
          } else if (mDocument) {
            nsEventDispatcher::Dispatch(mDocument, mPresContext, aEvent,
                                        nsnull, aStatus, nsnull);
          }
        }
      }

      nsContentUtils::SetIsHandlingKeyBoardEvent(wasHandlingKeyBoardEvent);

      // 3. Give event to event manager for post event state changes and
      //    generation of synthetic events.
      if (!mIsDestroying && NS_SUCCEEDED(rv)) {
        rv = manager->PostHandleEvent(mPresContext, aEvent,
                                      GetCurrentEventFrame(), aStatus,
                                      weakView.GetView());
      }
    }

    if (aEvent->message == NS_MOUSE_BUTTON_UP) {
      // reset the capturing content now that the mouse button is up
      SetCapturingContent(nsnull, 0);
    }
  }
  return rv;
}

// Dispatch event to content only (NOT full processing)
// See also HandleEventWithTarget which does full event processing.
nsresult
PresShell::HandleDOMEventWithTarget(nsIContent* aTargetContent, nsEvent* aEvent,
                                    nsEventStatus* aStatus)
{
  nsresult rv = NS_OK;

  PushCurrentEventInfo(nsnull, aTargetContent);

  // Bug 41013: Check if the event should be dispatched to content.
  // It's possible that we are in the middle of destroying the window
  // and the js context is out of date. This check detects the case
  // that caused a crash in bug 41013, but there may be a better way
  // to handle this situation!
  nsCOMPtr<nsISupports> container = mPresContext->GetContainer();
  if (container) {

    // Dispatch event to content
    rv = nsEventDispatcher::Dispatch(aTargetContent, mPresContext, aEvent, nsnull,
                                     aStatus);
  }

  PopCurrentEventInfo();
  return rv;
}

// See the method above.
nsresult
PresShell::HandleDOMEventWithTarget(nsIContent* aTargetContent,
                                    nsIDOMEvent* aEvent,
                                    nsEventStatus* aStatus)
{
  nsresult rv = NS_OK;

  PushCurrentEventInfo(nsnull, aTargetContent);
  nsCOMPtr<nsISupports> container = mPresContext->GetContainer();
  if (container) {
    rv = nsEventDispatcher::DispatchDOMEvent(aTargetContent, nsnull, aEvent,
                                             mPresContext, aStatus);
  }

  PopCurrentEventInfo();
  return rv;
}

PRBool
PresShell::AdjustContextMenuKeyEvent(nsMouseEvent* aEvent)
{
#ifdef MOZ_XUL
  // if a menu is open, open the context menu relative to the active item on the menu. 
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm) {
    nsIFrame* popupFrame = pm->GetTopPopup(ePopupTypeMenu);
    if (popupFrame) {
      nsIFrame* itemFrame = 
        (static_cast<nsMenuPopupFrame *>(popupFrame))->GetCurrentMenuItem();
      if (!itemFrame)
        itemFrame = popupFrame;

      nsCOMPtr<nsIWidget> widget = popupFrame->GetNearestWidget();
      aEvent->widget = widget;
      nsIntPoint widgetPoint = widget->WidgetToScreenOffset();
      aEvent->refPoint = itemFrame->GetScreenRect().BottomLeft() - widgetPoint;

      mCurrentEventContent = itemFrame->GetContent();
      mCurrentEventFrame = itemFrame;

      return PR_TRUE;
    }
  }
#endif

  // If we're here because of the key-equiv for showing context menus, we
  // have to twiddle with the NS event to make sure the context menu comes
  // up in the upper left of the relevant content area before we create
  // the DOM event. Since we never call InitMouseEvent() on the event, 
  // the client X/Y will be 0,0. We can make use of that if the widget is null.
  // Use the root view manager's widget since it's most likely to have one,
  // and the coordinates returned by GetCurrentItemAndPositionForElement
  // are relative to the widget of the root of the root view manager.
  nsRootPresContext* rootPC = mPresContext->GetRootPresContext();
  aEvent->refPoint.x = 0;
  aEvent->refPoint.y = 0;
  if (rootPC) {
    rootPC->PresShell()->GetViewManager()->
      GetRootWidget(getter_AddRefs(aEvent->widget));

    if (aEvent->widget) {
      // default the refpoint to the topleft of our document
      nsPoint offset(0, 0);
      nsIFrame* rootFrame = FrameManager()->GetRootFrame();
      if (rootFrame) {
        nsIView* view = rootFrame->GetClosestView(&offset);
        offset += view->GetOffsetToWidget(aEvent->widget);
        aEvent->refPoint =
          offset.ToNearestPixels(mPresContext->AppUnitsPerDevPixel());
      }
    }
  } else {
    aEvent->widget = nsnull;
  }

  // see if we should use the caret position for the popup
  nsIntPoint caretPoint;
  // Beware! This may flush notifications via synchronous
  // ScrollSelectionIntoView.
  if (PrepareToUseCaretPosition(aEvent->widget, caretPoint)) {
    // caret position is good
    aEvent->refPoint = caretPoint;
    return PR_TRUE;
  }

  // If we're here because of the key-equiv for showing context menus, we
  // have to reset the event target to the currently focused element. Get it
  // from the focus controller.
  nsCOMPtr<nsIDOMElement> currentFocus;
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm)
    fm->GetFocusedElement(getter_AddRefs(currentFocus));

  // Reset event coordinates relative to focused frame in view
  if (currentFocus) {
    nsCOMPtr<nsIContent> currentPointElement;
    GetCurrentItemAndPositionForElement(currentFocus,
                                        getter_AddRefs(currentPointElement),
                                        aEvent->refPoint,
                                        aEvent->widget);
    if (currentPointElement) {
      mCurrentEventContent = currentPointElement;
      mCurrentEventFrame = nsnull;
      GetCurrentEventFrame();
    }
  }

  return PR_TRUE;
}

// PresShell::PrepareToUseCaretPosition
//
//    This checks to see if we should use the caret position for popup context
//    menus. Returns true if the caret position should be used, and the
//    coordinates of that position is returned in aTargetPt. This function
//    will also scroll the window as needed to make the caret visible.
//
//    The event widget should be the widget that generated the event, and
//    whose coordinate system the resulting event's refPoint should be
//    relative to.  The returned point is in device pixels realtive to the
//    widget passed in.
PRBool
PresShell::PrepareToUseCaretPosition(nsIWidget* aEventWidget, nsIntPoint& aTargetPt)
{
  nsresult rv;

  // check caret visibility
  nsRefPtr<nsCaret> caret = GetCaret();
  NS_ENSURE_TRUE(caret, PR_FALSE);

  PRBool caretVisible = PR_FALSE;
  rv = caret->GetCaretVisible(&caretVisible);
  if (NS_FAILED(rv) || ! caretVisible)
    return PR_FALSE;

  // caret selection, this is a temporary weak reference, so no refcounting is 
  // needed
  nsISelection* domSelection = caret->GetCaretDOMSelection();
  NS_ENSURE_TRUE(domSelection, PR_FALSE);

  // since the match could be an anonymous textnode inside a
  // <textarea> or text <input>, we need to get the outer frame
  // note: frames are not refcounted
  nsIFrame* frame = nsnull; // may be NULL
  nsCOMPtr<nsIDOMNode> node;
  rv = domSelection->GetFocusNode(getter_AddRefs(node));
  NS_ENSURE_SUCCESS(rv, PR_FALSE);
  NS_ENSURE_TRUE(node, PR_FALSE);
  nsCOMPtr<nsIContent> content(do_QueryInterface(node));
  if (content) {
    nsIContent* nonNative = content->FindFirstNonNativeAnonymous();
    content = nonNative;
  }

  if (content) {
    // It seems like ScrollSelectionIntoView should be enough, but it's
    // not. The problem is that scrolling the selection into view when it is
    // below the current viewport will align the top line of the frame exactly
    // with the bottom of the window. This is fine, BUT, the popup event causes
    // the control to be re-focused which does this exact call to
    // ScrollContentIntoView, which has a one-pixel disagreement of whether the
    // frame is actually in view. The result is that the frame is aligned with
    // the top of the window, but the menu is still at the bottom.
    //
    // Doing this call first forces the frame to be in view, eliminating the
    // problem. The only difference in the result is that if your cursor is in
    // an edit box below the current view, you'll get the edit box aligned with
    // the top of the window. This is arguably better behavior anyway.
    rv = ScrollContentIntoView(content, NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE,
                                        NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);
    frame = content->GetPrimaryFrame();
    NS_WARN_IF_FALSE(frame, "No frame for focused content?");
  }

  // Actually scroll the selection (ie caret) into view. Note that this must
  // be synchronous since we will be checking the caret position on the screen.
  //
  // Be easy about errors, and just don't scroll in those cases. Better to have
  // the correct menu at a weird place than the wrong menu.
  // After ScrollSelectionIntoView(), the pending notifications might be
  // flushed and PresShell/PresContext/Frames may be dead. See bug 418470.
  nsCOMPtr<nsISelectionController> selCon;
  if (frame)
    frame->GetSelectionController(GetPresContext(), getter_AddRefs(selCon));
  else
    selCon = static_cast<nsISelectionController *>(this);
  if (selCon) {
    rv = selCon->ScrollSelectionIntoView(nsISelectionController::SELECTION_NORMAL,
                   nsISelectionController::SELECTION_FOCUS_REGION, PR_TRUE);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);
  }

  nsPresContext* presContext = GetPresContext();

  // get caret position relative to the closest view
  nsRect caretCoords;
  nsIFrame* caretFrame = caret->GetGeometry(domSelection, &caretCoords);
  if (!caretFrame)
    return PR_FALSE;
  nsPoint viewOffset;
  nsIView* view = caretFrame->GetClosestView(&viewOffset);
  if (!view)
    return PR_FALSE;
  // and then get the caret coords relative to the event widget
  if (aEventWidget) {
    viewOffset += view->GetOffsetToWidget(aEventWidget);
  }
  caretCoords.MoveBy(viewOffset);

  // caret coordinates are in app units, convert to pixels
  aTargetPt.x =
    presContext->AppUnitsToDevPixels(caretCoords.x + caretCoords.width);
  aTargetPt.y =
    presContext->AppUnitsToDevPixels(caretCoords.y + caretCoords.height);

  // make sure rounding doesn't return a pixel which is outside the caret
  // (e.g. one line lower)
  aTargetPt.y -= 1;

  return PR_TRUE;
}

void
PresShell::GetCurrentItemAndPositionForElement(nsIDOMElement *aCurrentEl,
                                               nsIContent** aTargetToUse,
                                               nsIntPoint& aTargetPt,
                                               nsIWidget *aRootWidget)
{
  nsCOMPtr<nsIContent> focusedContent(do_QueryInterface(aCurrentEl));
  ScrollContentIntoView(focusedContent, NS_PRESSHELL_SCROLL_ANYWHERE,
                                        NS_PRESSHELL_SCROLL_ANYWHERE);

  nsPresContext* presContext = GetPresContext();

  PRBool istree = PR_FALSE, checkLineHeight = PR_TRUE;
  nscoord extraTreeY = 0;

#ifdef MOZ_XUL
  // Set the position to just underneath the current item for multi-select
  // lists or just underneath the selected item for single-select lists. If
  // the element is not a list, or there is no selection, leave the position
  // as is.
  nsCOMPtr<nsIDOMXULSelectControlItemElement> item;
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelect =
    do_QueryInterface(aCurrentEl);
  if (multiSelect) {
    checkLineHeight = PR_FALSE;
    
    PRInt32 currentIndex;
    multiSelect->GetCurrentIndex(&currentIndex);
    if (currentIndex >= 0) {
      nsCOMPtr<nsIDOMXULElement> xulElement(do_QueryInterface(aCurrentEl));
      if (xulElement) {
        nsCOMPtr<nsIBoxObject> box;
        xulElement->GetBoxObject(getter_AddRefs(box));
        nsCOMPtr<nsITreeBoxObject> treeBox(do_QueryInterface(box));
        // Tree view special case (tree items have no frames)
        // Get the focused row and add its coordinates, which are already in pixels
        // XXX Boris, should we create a new interface so that this doesn't
        // need to know about trees? Something like nsINodelessChildCreator which
        // could provide the current focus coordinates?
        if (treeBox) {
          treeBox->EnsureRowIsVisible(currentIndex);
          PRInt32 firstVisibleRow, rowHeight;
          treeBox->GetFirstVisibleRow(&firstVisibleRow);
          treeBox->GetRowHeight(&rowHeight);

          extraTreeY += presContext->CSSPixelsToAppUnits(
                          (currentIndex - firstVisibleRow + 1) * rowHeight);
          istree = PR_TRUE;

          nsCOMPtr<nsITreeColumns> cols;
          treeBox->GetColumns(getter_AddRefs(cols));
          if (cols) {
            nsCOMPtr<nsITreeColumn> col;
            cols->GetFirstColumn(getter_AddRefs(col));
            if (col) {
              nsCOMPtr<nsIDOMElement> colElement;
              col->GetElement(getter_AddRefs(colElement));
              nsCOMPtr<nsIContent> colContent(do_QueryInterface(colElement));
              if (colContent) {
                nsIFrame* frame = colContent->GetPrimaryFrame();
                if (frame) {
                  extraTreeY += frame->GetSize().height;
                }
              }
            }
          }
        }
        else {
          multiSelect->GetCurrentItem(getter_AddRefs(item));
        }
      }
    }
  }
  else {
    // don't check menulists as the selected item will be inside a popup.
    nsCOMPtr<nsIDOMXULMenuListElement> menulist = do_QueryInterface(aCurrentEl);
    if (!menulist) {
      nsCOMPtr<nsIDOMXULSelectControlElement> select =
        do_QueryInterface(aCurrentEl);
      if (select) {
        checkLineHeight = PR_FALSE;
        select->GetSelectedItem(getter_AddRefs(item));
      }
    }
  }

  if (item)
    focusedContent = do_QueryInterface(item);
#endif

  nsIFrame *frame = focusedContent->GetPrimaryFrame();
  if (frame) {
    NS_ASSERTION(frame->PresContext() == GetPresContext(),
      "handling event for focused content that is not in our document?");

    nsPoint frameOrigin(0, 0);

    // Get the frame's origin within its view
    nsIView *view = frame->GetClosestView(&frameOrigin);
    NS_ASSERTION(view, "No view for frame");

    // View's origin relative the widget
    if (aRootWidget) {
      frameOrigin += view->GetOffsetToWidget(aRootWidget);
    }

    // Start context menu down and to the right from top left of frame
    // use the lineheight. This is a good distance to move the context
    // menu away from the top left corner of the frame. If we always 
    // used the frame height, the context menu could end up far away,
    // for example when we're focused on linked images.
    // On the other hand, we want to use the frame height if it's less
    // than the current line height, so that the context menu appears
    // associated with the correct frame.
    nscoord extra = 0;
    if (!istree) {
      extra = frame->GetSize().height;
      if (checkLineHeight) {
        nsIScrollableFrame *scrollFrame =
          nsLayoutUtils::GetNearestScrollableFrame(frame);
        if (scrollFrame) {
          nsSize scrollAmount = scrollFrame->GetLineScrollAmount();
          nsIFrame* f = do_QueryFrame(scrollFrame);
          PRInt32 APD = presContext->AppUnitsPerDevPixel();
          PRInt32 scrollAPD = f->PresContext()->AppUnitsPerDevPixel();
          scrollAmount = scrollAmount.ConvertAppUnits(scrollAPD, APD);
          if (extra > scrollAmount.height) {
            extra = scrollAmount.height;
          }
        }
      }
    }

    aTargetPt.x = presContext->AppUnitsToDevPixels(frameOrigin.x);
    aTargetPt.y = presContext->AppUnitsToDevPixels(
                    frameOrigin.y + extra + extraTreeY);
  }

  NS_IF_ADDREF(*aTargetToUse = focusedContent);
}

NS_IMETHODIMP
PresShell::ResizeReflow(nsIView *aView, nscoord aWidth, nscoord aHeight)
{
  return ResizeReflow(aWidth, aHeight);
}

NS_IMETHODIMP_(PRBool)
PresShell::ShouldIgnoreInvalidation()
{
  return mPaintingSuppressed || !mIsActive;
}

NS_IMETHODIMP_(void)
PresShell::WillPaint(PRBool aWillSendDidPaint)
{
  // Don't bother doing anything if some viewmanager in our tree is
  // painting while we still have painting suppressed.
  if (mPaintingSuppressed) {
    return;
  }

  if (!aWillSendDidPaint) {
    nsRootPresContext* rootPresContext = mPresContext->GetRootPresContext();
    if (!rootPresContext) {
      return;
    }
    if (rootPresContext == mPresContext) {
      rootPresContext->UpdatePluginGeometry();
    }
  }

  // Process reflows, if we have them, to reduce flicker due to invalidates and
  // reflow being interspersed.  Note that we _do_ allow this to be
  // interruptible; if we can't do all the reflows it's better to flicker a bit
  // than to freeze up.
  FlushPendingNotifications(Flush_InterruptibleLayout);
}

NS_IMETHODIMP_(void)
PresShell::DidPaint()
{
  nsRootPresContext* rootPresContext = mPresContext->GetRootPresContext();
  if (!rootPresContext) {
    return;
  }
  if (rootPresContext == mPresContext) {
    rootPresContext->UpdatePluginGeometry();
  }
}

nsresult
PresShell::GetAgentStyleSheets(nsCOMArray<nsIStyleSheet>& aSheets)
{
  aSheets.Clear();
  PRInt32 sheetCount = mStyleSet->SheetCount(nsStyleSet::eAgentSheet);

  for (PRInt32 i = 0; i < sheetCount; ++i) {
    nsIStyleSheet *sheet = mStyleSet->StyleSheetAt(nsStyleSet::eAgentSheet, i);
    if (!aSheets.AppendObject(sheet))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

nsresult
PresShell::SetAgentStyleSheets(const nsCOMArray<nsIStyleSheet>& aSheets)
{
  return mStyleSet->ReplaceSheets(nsStyleSet::eAgentSheet, aSheets);
}

nsresult
PresShell::AddOverrideStyleSheet(nsIStyleSheet *aSheet)
{
  return mStyleSet->PrependStyleSheet(nsStyleSet::eOverrideSheet, aSheet);
}

nsresult
PresShell::RemoveOverrideStyleSheet(nsIStyleSheet *aSheet)
{
  return mStyleSet->RemoveStyleSheet(nsStyleSet::eOverrideSheet, aSheet);
}

static void
FreezeElement(nsIContent *aContent, void * /* unused */)
{
  nsIFrame *frame = aContent->GetPrimaryFrame();
  nsIObjectFrame *objectFrame = do_QueryFrame(frame);
  if (objectFrame) {
    objectFrame->StopPlugin();
  }
}

static PRBool
FreezeSubDocument(nsIDocument *aDocument, void *aData)
{
  nsIPresShell *shell = aDocument->GetShell();
  if (shell)
    shell->Freeze();

  return PR_TRUE;
}

void
PresShell::Freeze()
{
  MaybeReleaseCapturingContent();

  mDocument->EnumerateFreezableElements(FreezeElement, nsnull);

  if (mCaret)
    mCaret->SetCaretVisible(PR_FALSE);

  mPaintingSuppressed = PR_TRUE;

  if (mDocument)
    mDocument->EnumerateSubDocuments(FreezeSubDocument, nsnull);

  nsPresContext* presContext = GetPresContext();
  if (presContext &&
      presContext->RefreshDriver()->PresContext() == presContext) {
    presContext->RefreshDriver()->Freeze();
  }

  mFrozen = PR_TRUE;
  UpdateImageLockingState();
}

void
PresShell::FireOrClearDelayedEvents(PRBool aFireEvents)
{
  mNoDelayedMouseEvents = PR_FALSE;
  mNoDelayedKeyEvents = PR_FALSE;
  if (!aFireEvents) {
    mDelayedEvents.Clear();
    return;
  }

  if (mDocument) {
    nsCOMPtr<nsIDocument> doc = mDocument;
    while (!mIsDestroying && mDelayedEvents.Length() &&
           !doc->EventHandlingSuppressed()) {
      nsAutoPtr<nsDelayedEvent> ev(mDelayedEvents[0].forget());
      mDelayedEvents.RemoveElementAt(0);
      ev->Dispatch(this);
    }
    if (!doc->EventHandlingSuppressed()) {
      mDelayedEvents.Clear();
    }
  }
}

static void
ThawElement(nsIContent *aContent, void *aShell)
{
  nsCOMPtr<nsIObjectLoadingContent> objlc(do_QueryInterface(aContent));
  if (objlc) {
    nsCOMPtr<nsIPluginInstance> inst;
    objlc->EnsureInstantiation(getter_AddRefs(inst));
  }
}

static PRBool
ThawSubDocument(nsIDocument *aDocument, void *aData)
{
  nsIPresShell *shell = aDocument->GetShell();
  if (shell)
    shell->Thaw();

  return PR_TRUE;
}

void
PresShell::Thaw()
{
  nsPresContext* presContext = GetPresContext();
  if (presContext &&
      presContext->RefreshDriver()->PresContext() == presContext) {
    presContext->RefreshDriver()->Thaw();
  }

  mDocument->EnumerateFreezableElements(ThawElement, this);

  if (mDocument)
    mDocument->EnumerateSubDocuments(ThawSubDocument, nsnull);

  // Get the activeness of our presshell, as this might have changed
  // while we were in the bfcache
  QueryIsActive();

  // We're now unfrozen
  mFrozen = PR_FALSE;
  UpdateImageLockingState();

  UnsuppressPainting();
}

//--------------------------------------------------------
// Start of protected and private methods on the PresShell
//--------------------------------------------------------

void
PresShell::MaybeScheduleReflow()
{
  ASSERT_REFLOW_SCHEDULED_STATE();
  if (mReflowScheduled || mIsDestroying || mIsReflowing ||
      mDirtyRoots.Length() == 0)
    return;

  if (!mPresContext->HasPendingInterrupt() || !ScheduleReflowOffTimer()) {
    ScheduleReflow();
  }

  ASSERT_REFLOW_SCHEDULED_STATE();
}

void
PresShell::ScheduleReflow()
{
  NS_PRECONDITION(!mReflowScheduled, "Why are we trying to schedule a reflow?");
  ASSERT_REFLOW_SCHEDULED_STATE();

  if (GetPresContext()->RefreshDriver()->AddLayoutFlushObserver(this)) {
    mReflowScheduled = PR_TRUE;
  }

  ASSERT_REFLOW_SCHEDULED_STATE();
}

nsresult
PresShell::DidCauseReflow()
{
  NS_ASSERTION(mChangeNestCount != 0, "Unexpected call to DidCauseReflow()");
  --mChangeNestCount;
  nsContentUtils::RemoveScriptBlocker();

  return NS_OK;
}

void
PresShell::WillDoReflow()
{
  // We just reflowed, tell the caret that its frame might have moved.
  // XXXbz that comment makes no sense
  if (mCaret) {
    mCaret->InvalidateOutsideCaret();
    mCaret->UpdateCaretPosition();
  }

  mPresContext->FlushUserFontSet();

  mFrameConstructor->BeginUpdate();
}

void
PresShell::DidDoReflow(PRBool aInterruptible)
{
  mFrameConstructor->EndUpdate();
  
  HandlePostedReflowCallbacks(aInterruptible);
  // Null-check mViewManager in case this happens during Destroy.  See
  // bugs 244435 and 238546.
  if (!mPaintingSuppressed && mViewManager)
    mViewManager->SynthesizeMouseMove(PR_FALSE);
  if (mCaret) {
    // Update the caret's position now to account for any changes created by
    // the reflow.
    mCaret->InvalidateOutsideCaret();
    mCaret->UpdateCaretPosition();
  }
}

static PLDHashOperator
MarkFramesDirtyToRoot(nsPtrHashKey<nsIFrame>* p, void* closure)
{
  nsIFrame* target = static_cast<nsIFrame*>(closure);
  for (nsIFrame* f = p->GetKey(); f && !NS_SUBTREE_DIRTY(f);
       f = f->GetParent()) {
    f->AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);

    if (f == target) {
      break;
    }
  }

  return PL_DHASH_NEXT;
}

void
PresShell::sReflowContinueCallback(nsITimer* aTimer, void* aPresShell)
{
  nsRefPtr<PresShell> self = static_cast<PresShell*>(aPresShell);

  NS_PRECONDITION(aTimer == self->mReflowContinueTimer, "Unexpected timer");
  self->mReflowContinueTimer = nsnull;
  self->ScheduleReflow();
}

PRBool
PresShell::ScheduleReflowOffTimer()
{
  NS_PRECONDITION(!mReflowScheduled, "Shouldn't get here");
  ASSERT_REFLOW_SCHEDULED_STATE();

  if (!mReflowContinueTimer) {
    mReflowContinueTimer = do_CreateInstance("@mozilla.org/timer;1");
    if (!mReflowContinueTimer ||
        NS_FAILED(mReflowContinueTimer->
                    InitWithFuncCallback(sReflowContinueCallback, this, 30,
                                         nsITimer::TYPE_ONE_SHOT))) {
      return PR_FALSE;
    }
  }
  return PR_TRUE;
}

PRBool
PresShell::DoReflow(nsIFrame* target, PRBool aInterruptible)
{
  NS_TIME_FUNCTION_WITH_DOCURL;

  if (mReflowContinueTimer) {
    mReflowContinueTimer->Cancel();
    mReflowContinueTimer = nsnull;
  }

  nsIFrame* rootFrame = FrameManager()->GetRootFrame();

  nsCOMPtr<nsIRenderingContext> rcx = GetReferenceRenderingContext();
  if (!rcx) {
    NS_NOTREACHED("CreateRenderingContext failure");
    return PR_FALSE;
  }

#ifdef DEBUG
  mCurrentReflowRoot = target;
#endif

  target->WillReflow(mPresContext);

  // If the target frame is the root of the frame hierarchy, then
  // use all the available space. If it's simply a `reflow root',
  // then use the target frame's size as the available space.
  nsSize size;
  if (target == rootFrame) {
     size = mPresContext->GetVisibleArea().Size();

     // target->GetRect() has the old size of the frame,
     // mPresContext->GetVisibleArea() has the new size.
     target->InvalidateRectDifference(mPresContext->GetVisibleArea(),
                                      target->GetRect());
  } else {
     size = target->GetSize();
  }

  NS_ASSERTION(!target->GetNextInFlow() && !target->GetPrevInFlow(),
               "reflow roots should never split");

  // Don't pass size directly to the reflow state, since a
  // constrained height implies page/column breaking.
  nsSize reflowSize(size.width, NS_UNCONSTRAINEDSIZE);
  nsHTMLReflowState reflowState(mPresContext, target, rcx, reflowSize);

  // fix the computed height
  NS_ASSERTION(reflowState.mComputedMargin == nsMargin(0, 0, 0, 0),
               "reflow state should not set margin for reflow roots");
  if (size.height != NS_UNCONSTRAINEDSIZE) {
    nscoord computedHeight =
      size.height - reflowState.mComputedBorderPadding.TopBottom();
    computedHeight = NS_MAX(computedHeight, 0);
    reflowState.SetComputedHeight(computedHeight);
  }
  NS_ASSERTION(reflowState.ComputedWidth() ==
                 size.width -
                   reflowState.mComputedBorderPadding.LeftRight(),
               "reflow state computed incorrect width");

  mPresContext->ReflowStarted(aInterruptible);
  mIsReflowing = PR_TRUE;

  nsReflowStatus status;
  nsHTMLReflowMetrics desiredSize;
  target->Reflow(mPresContext, desiredSize, reflowState, status);

  // If an incremental reflow is initiated at a frame other than the
  // root frame, then its desired size had better not change!  If it's
  // initiated at the root, then the size better not change unless its
  // height was unconstrained to start with.
  NS_ASSERTION((target == rootFrame && size.height == NS_UNCONSTRAINEDSIZE) ||
               (desiredSize.width == size.width &&
                desiredSize.height == size.height),
               "non-root frame's desired size changed during an "
               "incremental reflow");
  NS_ASSERTION(desiredSize.mOverflowArea ==
                 nsRect(nsPoint(0, 0),
                        nsSize(desiredSize.width, desiredSize.height)),
               "reflow roots must not have visible overflow");
  NS_ASSERTION(status == NS_FRAME_COMPLETE,
               "reflow roots should never split");

  target->SetSize(nsSize(desiredSize.width, desiredSize.height));

  nsContainerFrame::SyncFrameViewAfterReflow(mPresContext, target,
                                             target->GetView(),
                                             &desiredSize.mOverflowArea);
  nsContainerFrame::SyncWindowProperties(mPresContext, target,
                                         target->GetView());

  target->DidReflow(mPresContext, nsnull, NS_FRAME_REFLOW_FINISHED);
  if (target == rootFrame && size.height == NS_UNCONSTRAINEDSIZE) {
    mPresContext->SetVisibleArea(nsRect(0, 0, desiredSize.width,
                                        desiredSize.height));
  }

#ifdef DEBUG
  mCurrentReflowRoot = nsnull;
#endif

  NS_ASSERTION(mPresContext->HasPendingInterrupt() ||
               mFramesToDirty.Count() == 0,
               "Why do we need to dirty anything if not interrupted?");

  mIsReflowing = PR_FALSE;
  PRBool interrupted = mPresContext->HasPendingInterrupt();
  if (interrupted) {
    // Make sure target gets reflowed again.
    mFramesToDirty.EnumerateEntries(&MarkFramesDirtyToRoot, target);
    NS_ASSERTION(NS_SUBTREE_DIRTY(target), "Why is the target not dirty?");
    mDirtyRoots.AppendElement(target);

    // Clear mFramesToDirty after we've done the NS_SUBTREE_DIRTY(target)
    // assertion so that if it fails it's easier to see what's going on.
#ifdef NOISY_INTERRUPTIBLE_REFLOW
    printf("mFramesToDirty.Count() == %u\n", mFramesToDirty.Count());
#endif /* NOISY_INTERRUPTIBLE_REFLOW */
    mFramesToDirty.Clear();

    // Any FlushPendingNotifications with interruptible reflows
    // should be suppressed now. We don't want to do extra reflow work
    // before our reflow event happens.
    mSuppressInterruptibleReflows = PR_TRUE;
    MaybeScheduleReflow();
  }

  nsRootPresContext* rootPC = mPresContext->GetRootPresContext();
  if (rootPC) {
    rootPC->RequestUpdatePluginGeometry(target);
  }

  return !interrupted;
}

#ifdef DEBUG
void
PresShell::DoVerifyReflow()
{
  if (GetVerifyReflowEnable()) {
    // First synchronously render what we have so far so that we can
    // see it.
    nsIView* rootView;
    mViewManager->GetRootView(rootView);
    mViewManager->UpdateView(rootView, NS_VMREFRESH_IMMEDIATE);

    FlushPendingNotifications(Flush_Layout);
    mInVerifyReflow = PR_TRUE;
    PRBool ok = VerifyIncrementalReflow();
    mInVerifyReflow = PR_FALSE;
    if (VERIFY_REFLOW_ALL & gVerifyReflowFlags) {
      printf("ProcessReflowCommands: finished (%s)\n",
             ok ? "ok" : "failed");
    }

    if (0 != mDirtyRoots.Length()) {
      printf("XXX yikes! reflow commands queued during verify-reflow\n");
    }
  }
}
#endif

PRBool
PresShell::ProcessReflowCommands(PRBool aInterruptible)
{
  NS_TIME_FUNCTION_WITH_DOCURL;

  PRBool interrupted = PR_FALSE;
  if (0 != mDirtyRoots.Length()) {

#ifdef DEBUG
    if (VERIFY_REFLOW_DUMP_COMMANDS & gVerifyReflowFlags) {
      printf("ProcessReflowCommands: begin incremental reflow\n");
    }
#endif

    // If reflow is interruptible, then make a note of our deadline.
    const PRIntervalTime deadline = aInterruptible
        ? PR_IntervalNow() + PR_MicrosecondsToInterval(gMaxRCProcessingTime)
        : (PRIntervalTime)0;

    // Scope for the reflow entry point
    {
      nsAutoScriptBlocker scriptBlocker;
      WillDoReflow();
      AUTO_LAYOUT_PHASE_ENTRY_POINT(GetPresContext(), Reflow);

      do {
        // Send an incremental reflow notification to the target frame.
        PRInt32 idx = mDirtyRoots.Length() - 1;
        nsIFrame *target = mDirtyRoots[idx];
        mDirtyRoots.RemoveElementAt(idx);

        if (!NS_SUBTREE_DIRTY(target)) {
          // It's not dirty anymore, which probably means the notification
          // was posted in the middle of a reflow (perhaps with a reflow
          // root in the middle).  Don't do anything.
          continue;
        }

        interrupted = !DoReflow(target, aInterruptible);

        // Keep going until we're out of reflow commands, or we've run
        // past our deadline, or we're interrupted.
      } while (!interrupted && mDirtyRoots.Length() &&
               (!aInterruptible || PR_IntervalNow() < deadline));

      interrupted = mDirtyRoots.Length() != 0;
    }

    // Exiting the scriptblocker might have killed us
    if (!mIsDestroying) {
      DidDoReflow(aInterruptible);
    }

    // DidDoReflow might have killed us
    if (!mIsDestroying) {
#ifdef DEBUG
      if (VERIFY_REFLOW_DUMP_COMMANDS & gVerifyReflowFlags) {
        printf("\nPresShell::ProcessReflowCommands() finished: this=%p\n",
               (void*)this);
      }
      DoVerifyReflow();
#endif

      // If any new reflow commands were enqueued during the reflow, schedule
      // another reflow event to process them.  Note that we want to do this
      // after DidDoReflow(), since that method can change whether there are
      // dirty roots around by flushing, and there's no point in posting a
      // reflow event just to have the flush revoke it.
      if (mDirtyRoots.Length())
        MaybeScheduleReflow();
    }
  }

  if (!mIsDestroying && mShouldUnsuppressPainting &&
      mDirtyRoots.Length() == 0) {
    // We only unlock if we're out of reflows.  It's pointless
    // to unlock if reflows are still pending, since reflows
    // are just going to thrash the frames around some more.  By
    // waiting we avoid an overeager "jitter" effect.
    mShouldUnsuppressPainting = PR_FALSE;
    UnsuppressAndInvalidate();
  }

  return !interrupted;
}

#ifdef MOZ_XUL
/*
 * It's better to add stuff to the |DidSetStyleContext| method of the
 * relevant frames than adding it here.  These methods should (ideally,
 * anyway) go away.
 */

// Return value says whether to walk children.
typedef PRBool (* frameWalkerFn)(nsIFrame *aFrame, void *aClosure);
   
static PRBool
ReResolveMenusAndTrees(nsIFrame *aFrame, void *aClosure)
{
  // Trees have a special style cache that needs to be flushed when
  // the theme changes.
  nsTreeBodyFrame *treeBody = do_QueryFrame(aFrame);
  if (treeBody)
    treeBody->ClearStyleAndImageCaches();

  // We deliberately don't re-resolve style on a menu's popup
  // sub-content, since doing so slows menus to a crawl.  That means we
  // have to special-case them on a skin switch, and ensure that the
  // popup frames just get destroyed completely.
  if (aFrame && aFrame->GetType() == nsGkAtoms::menuFrame)
    (static_cast<nsMenuFrame *>(aFrame))->CloseMenu(PR_TRUE);
  return PR_TRUE;
}

static PRBool
ReframeImageBoxes(nsIFrame *aFrame, void *aClosure)
{
  nsStyleChangeList *list = static_cast<nsStyleChangeList*>(aClosure);
  if (aFrame->GetType() == nsGkAtoms::imageBoxFrame) {
    list->AppendChange(aFrame, aFrame->GetContent(),
                       NS_STYLE_HINT_FRAMECHANGE);
    return PR_FALSE; // don't walk descendants
  }
  return PR_TRUE; // walk descendants
}

static void
WalkFramesThroughPlaceholders(nsPresContext *aPresContext, nsIFrame *aFrame,
                              frameWalkerFn aFunc, void *aClosure)
{
  PRBool walkChildren = (*aFunc)(aFrame, aClosure);
  if (!walkChildren)
    return;

  PRInt32 listIndex = 0;
  nsIAtom* childList = nsnull;

  do {
    nsIFrame *child = aFrame->GetFirstChild(childList);
    while (child) {
      if (!(child->GetStateBits() & NS_FRAME_OUT_OF_FLOW)) {
        // only do frames that are in flow, and recur through the
        // out-of-flows of placeholders.
        WalkFramesThroughPlaceholders(aPresContext,
                                      nsPlaceholderFrame::GetRealFrameFor(child),
                                      aFunc, aClosure);
      }
      child = child->GetNextSibling();
    }

    childList = aFrame->GetAdditionalChildListName(listIndex++);
  } while (childList);
}
#endif

NS_IMETHODIMP
PresShell::Observe(nsISupports* aSubject, 
                   const char* aTopic,
                   const PRUnichar* aData)
{
#ifdef MOZ_XUL
  if (!nsCRT::strcmp(aTopic, "chrome-flush-skin-caches")) {
    nsIFrame *rootFrame = FrameManager()->GetRootFrame();
    // Need to null-check because "chrome-flush-skin-caches" can happen
    // at interesting times during startup.
    if (rootFrame) {
      NS_ASSERTION(mViewManager, "View manager must exist");
      nsIViewManager::UpdateViewBatch batch(mViewManager);

      nsWeakFrame weakRoot(rootFrame);
      // Have to make sure that the content notifications are flushed before we
      // start messing with the frame model; otherwise we can get content doubling.
      mDocument->FlushPendingNotifications(Flush_ContentAndNotify);

      if (weakRoot.IsAlive()) {
        WalkFramesThroughPlaceholders(mPresContext, rootFrame,
                                      &ReResolveMenusAndTrees, nsnull);

        // Because "chrome:" URL equality is messy, reframe image box
        // frames (hack!).
        nsStyleChangeList changeList;
        WalkFramesThroughPlaceholders(mPresContext, rootFrame,
                                      ReframeImageBoxes, &changeList);
        // Mark ourselves as not safe to flush while we're doing frame
        // construction.
        {
          nsAutoScriptBlocker scriptBlocker;
          ++mChangeNestCount;
          mFrameConstructor->ProcessRestyledFrames(changeList);
          --mChangeNestCount;
        }
      }
      batch.EndUpdateViewBatch(NS_VMREFRESH_NO_SYNC);
#ifdef ACCESSIBILITY
      InvalidateAccessibleSubtree(nsnull);
#endif
    }
    return NS_OK;
  }
#endif

  if (!nsCRT::strcmp(aTopic, "agent-sheet-added") && mStyleSet) {
    AddAgentSheet(aSubject);
    return NS_OK;
  }

  if (!nsCRT::strcmp(aTopic, "user-sheet-added") && mStyleSet) {
    AddUserSheet(aSubject);
    return NS_OK;
  }

  if (!nsCRT::strcmp(aTopic, "agent-sheet-removed") && mStyleSet) {
    RemoveSheet(nsStyleSet::eAgentSheet, aSubject);
    return NS_OK;
  }

  if (!nsCRT::strcmp(aTopic, "user-sheet-removed") && mStyleSet) {
    RemoveSheet(nsStyleSet::eUserSheet, aSubject);
    return NS_OK;
  }

#ifdef ACCESSIBILITY
  if (!nsCRT::strcmp(aTopic, "a11y-init-or-shutdown")) {
    gIsAccessibilityActive = aData && *aData == '1';
  }
#endif
  NS_WARNING("unrecognized topic in PresShell::Observe");
  return NS_ERROR_FAILURE;
}

PRBool
nsIPresShell::AddRefreshObserverInternal(nsARefreshObserver* aObserver,
                                         mozFlushType aFlushType)
{
  return GetPresContext()->RefreshDriver()->
    AddRefreshObserver(aObserver, aFlushType);
}

/* virtual */ PRBool
nsIPresShell::AddRefreshObserverExternal(nsARefreshObserver* aObserver,
                                         mozFlushType aFlushType)
{
  return AddRefreshObserverInternal(aObserver, aFlushType);
}

PRBool
nsIPresShell::RemoveRefreshObserverInternal(nsARefreshObserver* aObserver,
                                            mozFlushType aFlushType)
{
  return GetPresContext()->RefreshDriver()->
    RemoveRefreshObserver(aObserver, aFlushType);
}

/* virtual */ PRBool
nsIPresShell::RemoveRefreshObserverExternal(nsARefreshObserver* aObserver,
                                            mozFlushType aFlushType)
{
  return RemoveRefreshObserverInternal(aObserver, aFlushType);
}

//------------------------------------------------------
// End of protected and private methods on the PresShell
//------------------------------------------------------

// Start of DEBUG only code

#ifdef NS_DEBUG
#include "nsViewsCID.h"
#include "nsWidgetsCID.h"
#include "nsIDeviceContext.h"
#include "nsIURL.h"
#include "nsILinkHandler.h"

static NS_DEFINE_CID(kViewManagerCID, NS_VIEW_MANAGER_CID);

static void
LogVerifyMessage(nsIFrame* k1, nsIFrame* k2, const char* aMsg)
{
  nsAutoString n1, n2;
  if (k1) {
    k1->GetFrameName(n1);
  } else {
    n1.Assign(NS_LITERAL_STRING("(null)"));
  }

  if (k2) {
    k2->GetFrameName(n2);
  } else {
    n2.Assign(NS_LITERAL_STRING("(null)"));
  }

  printf("verifyreflow: %s %p != %s %p  %s\n",
         NS_LossyConvertUTF16toASCII(n1).get(), (void*)k1,
         NS_LossyConvertUTF16toASCII(n2).get(), (void*)k2, aMsg);
}

static void
LogVerifyMessage(nsIFrame* k1, nsIFrame* k2, const char* aMsg,
                 const nsRect& r1, const nsRect& r2)
{
  printf("VerifyReflow Error:\n");
  nsAutoString name;

  if (k1) {
    k1->GetFrameName(name);
    printf("  %s %p ", NS_LossyConvertUTF16toASCII(name).get(), (void*)k1);
  }
  printf("{%d, %d, %d, %d} != \n", r1.x, r1.y, r1.width, r1.height);

  if (k2) {
    k2->GetFrameName(name);
    printf("  %s %p ", NS_LossyConvertUTF16toASCII(name).get(), (void*)k2);
  }
  printf("{%d, %d, %d, %d}\n  %s\n",
         r2.x, r2.y, r2.width, r2.height, aMsg);
}

static void
LogVerifyMessage(nsIFrame* k1, nsIFrame* k2, const char* aMsg,
                 const nsIntRect& r1, const nsIntRect& r2)
{
  printf("VerifyReflow Error:\n");
  nsAutoString name;

  if (k1) {
    k1->GetFrameName(name);
    printf("  %s %p ", NS_LossyConvertUTF16toASCII(name).get(), (void*)k1);
  }
  printf("{%d, %d, %d, %d} != \n", r1.x, r1.y, r1.width, r1.height);

  if (k2) {
    k2->GetFrameName(name);
    printf("  %s %p ", NS_LossyConvertUTF16toASCII(name).get(), (void*)k2);
  }
  printf("{%d, %d, %d, %d}\n  %s\n",
         r2.x, r2.y, r2.width, r2.height, aMsg);
}

static PRBool
CompareTrees(nsPresContext* aFirstPresContext, nsIFrame* aFirstFrame, 
             nsPresContext* aSecondPresContext, nsIFrame* aSecondFrame)
{
  if (!aFirstPresContext || !aFirstFrame || !aSecondPresContext || !aSecondFrame)
    return PR_TRUE;
  // XXX Evil hack to reduce false positives; I can't seem to figure
  // out how to flush scrollbar changes correctly
  //if (aFirstFrame->GetType() == nsGkAtoms::scrollbarFrame)
  //  return PR_TRUE;
  PRBool ok = PR_TRUE;
  nsIAtom* listName = nsnull;
  PRInt32 listIndex = 0;
  do {
    const nsFrameList& kids1 = aFirstFrame->GetChildList(listName);
    const nsFrameList& kids2 = aSecondFrame->GetChildList(listName);
    PRInt32 l1 = kids1.GetLength();
    PRInt32 l2 = kids2.GetLength();;
    if (l1 != l2) {
      ok = PR_FALSE;
      LogVerifyMessage(kids1.FirstChild(), kids2.FirstChild(),
                       "child counts don't match: ");
      printf("%d != %d\n", l1, l2);
      if (0 == (VERIFY_REFLOW_ALL & gVerifyReflowFlags)) {
        break;
      }
    }

    nsIntRect r1, r2;
    nsIView* v1, *v2;
    for (nsFrameList::Enumerator e1(kids1), e2(kids2);
         ;
         e1.Next(), e2.Next()) {
      nsIFrame* k1 = e1.get();
      nsIFrame* k2 = e2.get();
      if (((nsnull == k1) && (nsnull != k2)) ||
          ((nsnull != k1) && (nsnull == k2))) {
        ok = PR_FALSE;
        LogVerifyMessage(k1, k2, "child lists are different\n");
        break;
      }
      else if (nsnull != k1) {
        // Verify that the frames are the same size
        if (k1->GetRect() != k2->GetRect()) {
          ok = PR_FALSE;
          LogVerifyMessage(k1, k2, "(frame rects)", k1->GetRect(), k2->GetRect());
        }

        // Make sure either both have views or neither have views; if they
        // do have views, make sure the views are the same size. If the
        // views have widgets, make sure they both do or neither does. If
        // they do, make sure the widgets are the same size.
        v1 = k1->GetView();
        v2 = k2->GetView();
        if (((nsnull == v1) && (nsnull != v2)) ||
            ((nsnull != v1) && (nsnull == v2))) {
          ok = PR_FALSE;
          LogVerifyMessage(k1, k2, "child views are not matched\n");
        }
        else if (nsnull != v1) {
          if (v1->GetBounds() != v2->GetBounds()) {
            LogVerifyMessage(k1, k2, "(view rects)", v1->GetBounds(), v2->GetBounds());
          }

          nsIWidget* w1 = v1->GetWidget();
          nsIWidget* w2 = v2->GetWidget();
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

        // XXX Should perhaps compare their float managers.

        // Compare the sub-trees too
        if (!CompareTrees(aFirstPresContext, k1, aSecondPresContext, k2)) {
          ok = PR_FALSE;
          if (0 == (VERIFY_REFLOW_ALL & gVerifyReflowFlags)) {
            break;
          }
        }
      }
      else {
        break;
      }
    }
    if (!ok && (0 == (VERIFY_REFLOW_ALL & gVerifyReflowFlags))) {
      break;
    }

    nsIAtom* listName1 = aFirstFrame->GetAdditionalChildListName(listIndex);
    nsIAtom* listName2 = aSecondFrame->GetAdditionalChildListName(listIndex);
    listIndex++;
    if (listName1 != listName2) {
      if (0 == (VERIFY_REFLOW_ALL & gVerifyReflowFlags)) {
        ok = PR_FALSE;
      }
      LogVerifyMessage(kids1.FirstChild(), kids2.FirstChild(),
                       "child list names are not matched: ");
      nsAutoString tmp;
      if (nsnull != listName1) {
        listName1->ToString(tmp);
        fputs(NS_LossyConvertUTF16toASCII(tmp).get(), stdout);
      }
      else
        fputs("(null)", stdout);
      printf(" != ");
      if (nsnull != listName2) {
        listName2->ToString(tmp);
        fputs(NS_LossyConvertUTF16toASCII(tmp).get(), stdout);
      }
      else
        fputs("(null)", stdout);
      printf("\n");
      break;
    }
    listName = listName1;
  } while (ok && (listName != nsnull));

  return ok;
}
#endif

#if 0
static nsIFrame*
FindTopFrame(nsIFrame* aRoot)
{
  if (aRoot) {
    nsIContent* content = aRoot->GetContent();
    if (content) {
      nsIAtom* tag;
      content->GetTag(tag);
      if (nsnull != tag) {
        NS_RELEASE(tag);
        return aRoot;
      }
    }

    // Try one of the children
    nsIFrame* kid = aRoot->GetFirstChild(nsnull);
    while (nsnull != kid) {
      nsIFrame* result = FindTopFrame(kid);
      if (nsnull != result) {
        return result;
      }
      kid = kid->GetNextSibling();
    }
  }
  return nsnull;
}
#endif


#ifdef DEBUG

nsresult
PresShell::CloneStyleSet(nsStyleSet* aSet, nsStyleSet** aResult)
{
  nsStyleSet *clone = new nsStyleSet();
  if (!clone) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  PRInt32 i, n = aSet->SheetCount(nsStyleSet::eOverrideSheet);
  for (i = 0; i < n; i++) {
    nsIStyleSheet* ss = aSet->StyleSheetAt(nsStyleSet::eOverrideSheet, i);
    if (ss)
      clone->AppendStyleSheet(nsStyleSet::eOverrideSheet, ss);
  }

  // The document expects to insert document stylesheets itself
#if 0
  n = aSet->SheetCount(nsStyleSet::eDocSheet);
  for (i = 0; i < n; i++) {
    nsIStyleSheet* ss = aSet->StyleSheetAt(nsStyleSet::eDocSheet, i);
    if (ss)
      clone->AddDocStyleSheet(ss, mDocument);
  }
#endif

  n = aSet->SheetCount(nsStyleSet::eUserSheet);
  for (i = 0; i < n; i++) {
    nsIStyleSheet* ss = aSet->StyleSheetAt(nsStyleSet::eUserSheet, i);
    if (ss)
      clone->AppendStyleSheet(nsStyleSet::eUserSheet, ss);
  }

  n = aSet->SheetCount(nsStyleSet::eAgentSheet);
  for (i = 0; i < n; i++) {
    nsIStyleSheet* ss = aSet->StyleSheetAt(nsStyleSet::eAgentSheet, i);
    if (ss)
      clone->AppendStyleSheet(nsStyleSet::eAgentSheet, ss);
  }
  *aResult = clone;
  return NS_OK;
}

#ifdef DEBUG_Eli
static nsresult
DumpToPNG(nsIPresShell* shell, nsAString& name) {
  PRInt32 width=1000, height=1000;
  nsRect r(0, 0, shell->GetPresContext()->DevPixelsToAppUnits(width),
                 shell->GetPresContext()->DevPixelsToAppUnits(height));

  nsRefPtr<gfxImageSurface> imgSurface =
     new gfxImageSurface(gfxIntSize(width, height),
                         gfxImageSurface::ImageFormatARGB32);
  NS_ENSURE_TRUE(imgSurface, NS_ERROR_OUT_OF_MEMORY);

  nsRefPtr<gfxContext> imgContext = new gfxContext(imgSurface);

  nsRefPtr<gfxASurface> surface = 
    gfxPlatform::GetPlatform()->
    CreateOffscreenSurface(gfxIntSize(width, height),
      gfxASurface::ImageFormatARGB32);
  NS_ENSURE_TRUE(surface, NS_ERROR_OUT_OF_MEMORY);

  nsRefPtr<gfxContext> context = new gfxContext(surface);
  NS_ENSURE_TRUE(context, NS_ERROR_OUT_OF_MEMORY);

  shell->RenderDocument(r, 0, NS_RGB(255, 255, 0), context);

  imgContext->DrawSurface(surface, gfxSize(width, height));

  nsCOMPtr<imgIEncoder> encoder = do_CreateInstance("@mozilla.org/image/encoder;2?type=image/png");
  NS_ENSURE_TRUE(encoder, NS_ERROR_FAILURE);
  encoder->InitFromData(imgSurface->Data(), imgSurface->Stride() * height,
                        width, height, imgSurface->Stride(),
                        imgIEncoder::INPUT_FORMAT_HOSTARGB, EmptyString());

  // XXX not sure if this is the right way to write to a file
  nsCOMPtr<nsILocalFile> file = do_CreateInstance("@mozilla.org/file/local;1");
  NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);
  rv = file->InitWithPath(name);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 length;
  encoder->Available(&length);

  nsCOMPtr<nsIOutputStream> outputStream;
  rv = NS_NewLocalFileOutputStream(getter_AddRefs(outputStream), file);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIOutputStream> bufferedOutputStream;
  rv = NS_NewBufferedOutputStream(getter_AddRefs(bufferedOutputStream),
                                  outputStream, length);

  PRUint32 numWritten;
  rv = bufferedOutputStream->WriteFrom(encoder, length, &numWritten);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}
#endif

// After an incremental reflow, we verify the correctness by doing a
// full reflow into a fresh frame tree.
PRBool
PresShell::VerifyIncrementalReflow()
{
   if (VERIFY_REFLOW_NOISY & gVerifyReflowFlags) {
     printf("Building Verification Tree...\n");
   }

  // Create a presentation context to view the new frame tree
  nsRefPtr<nsPresContext> cx =
       new nsRootPresContext(mDocument, mPresContext->IsPaginated() ?
                                        nsPresContext::eContext_PrintPreview :
                                        nsPresContext::eContext_Galley);
  NS_ENSURE_TRUE(cx, PR_FALSE);

  nsIDeviceContext *dc = mPresContext->DeviceContext();
  nsresult rv = cx->Init(dc);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  // Get our scrolling preference
  nsIView* rootView;
  mViewManager->GetRootView(rootView);
  NS_ENSURE_TRUE(rootView->HasWidget(), PR_FALSE);
  nsIWidget* parentWidget = rootView->GetWidget();

  // Create a new view manager.
  nsCOMPtr<nsIViewManager> vm = do_CreateInstance(kViewManagerCID);
  NS_ENSURE_TRUE(vm, PR_FALSE);
  rv = vm->Init(dc);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  // Create a child window of the parent that is our "root view/window"
  // Create a view
  nsRect tbounds = mPresContext->GetVisibleArea();
  nsIView* view = vm->CreateView(tbounds, nsnull);
  NS_ENSURE_TRUE(view, PR_FALSE);

  //now create the widget for the view
  rv = view->CreateWidgetForParent(parentWidget, nsnull, PR_TRUE);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  // Setup hierarchical relationship in view manager
  vm->SetRootView(view);

  // Make the new presentation context the same size as our
  // presentation context.
  nsRect r = mPresContext->GetVisibleArea();
  cx->SetVisibleArea(r);

  // Create a new presentation shell to view the document. Use the
  // exact same style information that this document has.
  nsAutoPtr<nsStyleSet> newSet;
  rv = CloneStyleSet(mStyleSet, getter_Transfers(newSet));
  NS_ENSURE_SUCCESS(rv, PR_FALSE);
  nsCOMPtr<nsIPresShell> sh;
  rv = mDocument->CreateShell(cx, vm, newSet, getter_AddRefs(sh));
  NS_ENSURE_SUCCESS(rv, PR_FALSE);
  newSet.forget();
  // Note that after we create the shell, we must make sure to destroy it
  sh->SetVerifyReflowEnable(PR_FALSE); // turn off verify reflow while we're reflowing the test frame tree
  vm->SetViewObserver((nsIViewObserver *)((PresShell*)sh.get()));
  {
    nsAutoCauseReflowNotifier crNotifier(this);
    sh->InitialReflow(r.width, r.height);
  }
  mDocument->BindingManager()->ProcessAttachedQueue();
  sh->FlushPendingNotifications(Flush_Layout);
  sh->SetVerifyReflowEnable(PR_TRUE);  // turn on verify reflow again now that we're done reflowing the test frame tree
  // Force the non-primary presshell to unsuppress; it doesn't want to normally
  // because it thinks it's hidden
  ((PresShell*)sh.get())->mPaintingSuppressed = PR_FALSE;
  if (VERIFY_REFLOW_NOISY & gVerifyReflowFlags) {
     printf("Verification Tree built, comparing...\n");
  }

  // Now that the document has been reflowed, use its frame tree to
  // compare against our frame tree.
  nsIFrame* root1 = FrameManager()->GetRootFrame();
  nsIFrame* root2 = sh->FrameManager()->GetRootFrame();
  PRBool ok = CompareTrees(mPresContext, root1, cx, root2);
  if (!ok && (VERIFY_REFLOW_NOISY & gVerifyReflowFlags)) {
    printf("Verify reflow failed, primary tree:\n");
    root1->List(stdout, 0);
    printf("Verification tree:\n");
    root2->List(stdout, 0);
  }

#ifdef DEBUG_Eli
  // Sample code for dumping page to png
  // XXX Needs to be made more flexible
  if (!ok) {
    nsString stra;
    static int num = 0;
    stra.AppendLiteral("C:\\mozilla\\mozilla\\debug\\filea");
    stra.AppendInt(num);
    stra.AppendLiteral(".png");
    DumpToPNG(sh, stra);
    nsString strb;
    strb.AppendLiteral("C:\\mozilla\\mozilla\\debug\\fileb");
    strb.AppendInt(num);
    strb.AppendLiteral(".png");
    DumpToPNG(this, strb);
    ++num;
  }
#endif

  sh->EndObservingDocument();
  sh->Destroy();
  if (VERIFY_REFLOW_NOISY & gVerifyReflowFlags) {
    printf("Finished Verifying Reflow...\n");
  }

  return ok;
}

// Layout debugging hooks
void
PresShell::ListStyleContexts(nsIFrame *aRootFrame, FILE *out, PRInt32 aIndent)
{
  nsStyleContext *sc = aRootFrame->GetStyleContext();
  if (sc)
    sc->List(out, aIndent);
}

void
PresShell::ListStyleSheets(FILE *out, PRInt32 aIndent)
{
  PRInt32 sheetCount = mStyleSet->SheetCount(nsStyleSet::eDocSheet);
  for (PRInt32 i = 0; i < sheetCount; ++i) {
    mStyleSet->StyleSheetAt(nsStyleSet::eDocSheet, i)->List(out, aIndent);
    fputs("\n", out);
  }
}

void
PresShell::VerifyStyleTree()
{
  VERIFY_STYLE_TREE;
}
#endif

//=============================================================
//=============================================================
//-- Debug Reflow Counts
//=============================================================
//=============================================================
#ifdef MOZ_REFLOW_PERF
//-------------------------------------------------------------
void
PresShell::DumpReflows()
{
  if (mReflowCountMgr) {
    nsCAutoString uriStr;
    if (mDocument) {
      nsIURI *uri = mDocument->GetDocumentURI();
      if (uri) {
        uri->GetPath(uriStr);
      }
    }
    mReflowCountMgr->DisplayTotals(uriStr.get());
    mReflowCountMgr->DisplayHTMLTotals(uriStr.get());
    mReflowCountMgr->DisplayDiffsInTotals("Differences");
  }
}

//-------------------------------------------------------------
void
PresShell::CountReflows(const char * aName, nsIFrame * aFrame)
{
  if (mReflowCountMgr) {
    mReflowCountMgr->Add(aName, aFrame);
  }
}

//-------------------------------------------------------------
void
PresShell::PaintCount(const char * aName,
                      nsIRenderingContext* aRenderingContext,
                      nsPresContext* aPresContext,
                      nsIFrame * aFrame,
                      PRUint32 aColor)
{
  if (mReflowCountMgr) {
    mReflowCountMgr->PaintCount(aName, aRenderingContext, aPresContext, aFrame, aColor);
  }
}

//-------------------------------------------------------------
void
PresShell::SetPaintFrameCount(PRBool aPaintFrameCounts)
{
  if (mReflowCountMgr) {
    mReflowCountMgr->SetPaintFrameCounts(aPaintFrameCounts);
  }
}

PRBool
PresShell::IsPaintingFrameCounts()
{
  if (mReflowCountMgr)
    return mReflowCountMgr->IsPaintingFrameCounts();
  return PR_FALSE;
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
  mTotal = 0;
}

//------------------------------------------------------------------
void ReflowCounter::SetTotalsCache()
{
  mCacheTotal = mTotal;
}

//------------------------------------------------------------------
void ReflowCounter::CalcDiffInTotals()
{
  mCacheTotal = mTotal - mCacheTotal;
}

//------------------------------------------------------------------
void ReflowCounter::DisplayTotals(const char * aStr)
{
  DisplayTotals(mTotal, aStr?aStr:"Totals");
}

//------------------------------------------------------------------
void ReflowCounter::DisplayDiffTotals(const char * aStr)
{
  DisplayTotals(mCacheTotal, aStr?aStr:"Diff Totals");
}

//------------------------------------------------------------------
void ReflowCounter::DisplayHTMLTotals(const char * aStr)
{
  DisplayHTMLTotals(mTotal, aStr?aStr:"Totals");
}

//------------------------------------------------------------------
void ReflowCounter::DisplayTotals(PRUint32 aTotal, const char * aTitle)
{
  // figure total
  if (aTotal == 0) {
    return;
  }
  ReflowCounter * gTots = (ReflowCounter *)mMgr->LookUp(kGrandTotalsStr);

  printf("%25s\t", aTitle);
  printf("%d\t", aTotal);
  if (gTots != this && aTotal > 0) {
    gTots->Add(aTotal);
  }
}

//------------------------------------------------------------------
void ReflowCounter::DisplayHTMLTotals(PRUint32 aTotal, const char * aTitle)
{
  if (aTotal == 0) {
    return;
  }

  ReflowCounter * gTots = (ReflowCounter *)mMgr->LookUp(kGrandTotalsStr);
  FILE * fd = mMgr->GetOutFile();
  if (!fd) {
    return;
  }

  fprintf(fd, "<tr><td><center>%s</center></td>", aTitle);
  fprintf(fd, "<td><center>%d</center></td></tr>\n", aTotal);

  if (gTots != this && aTotal > 0) {
    gTots->Add(aTotal);
  }
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
void ReflowCountMgr::Add(const char * aName, nsIFrame * aFrame)
{
  NS_ASSERTION(aName != nsnull, "Name shouldn't be null!");

  if (mDumpFrameCounts && nsnull != mCounts) {
    ReflowCounter * counter = (ReflowCounter *)PL_HashTableLookup(mCounts, aName);
    if (counter == nsnull) {
      counter = new ReflowCounter(this);
      NS_ASSERTION(counter != nsnull, "null ptr");
      char * name = NS_strdup(aName);
      NS_ASSERTION(name != nsnull, "null ptr");
      PL_HashTableAdd(mCounts, name, counter);
    }
    counter->Add();
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
      counter->mName.AssignASCII(aName);
      PL_HashTableAdd(mIndiFrameCounts, key, counter);
    }
    // this eliminates extra counts from super classes
    if (counter != nsnull && counter->mName.EqualsASCII(aName)) {
      counter->mCount++;
      counter->mCounter.Add(1);
    }
  }
}

//------------------------------------------------------------------
void ReflowCountMgr::PaintCount(const char *    aName, 
                                nsIRenderingContext* aRenderingContext, 
                                nsPresContext* aPresContext, 
                                nsIFrame*       aFrame, 
                                PRUint32        aColor)
{
  if (mPaintFrameByFrameCounts && 
      nsnull != mIndiFrameCounts && 
      aFrame != nsnull) {
    char * key = new char[16];
    sprintf(key, "%p", (void*)aFrame);
    IndiReflowCounter * counter = (IndiReflowCounter *)PL_HashTableLookup(mIndiFrameCounts, key);
    if (counter != nsnull && counter->mName.EqualsASCII(aName)) {
      aRenderingContext->PushState();
      nsFont font("Times", NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
                  NS_FONT_WEIGHT_NORMAL, NS_FONT_STRETCH_NORMAL, 0,
                  nsPresContext::CSSPixelsToAppUnits(11));

      nsCOMPtr<nsIFontMetrics> fm = aPresContext->GetMetricsFor(font);
      aRenderingContext->SetFont(fm);
      char buf[16];
      sprintf(buf, "%d", counter->mCount);
      nscoord x = 0, y;
      nscoord width, height;
      aRenderingContext->SetTextRunRTL(PR_FALSE);
      aRenderingContext->GetWidth((char*)buf, width);
      fm->GetHeight(height);
      fm->GetMaxAscent(y);

      PRUint32 color;
      PRUint32 color2;
      if (aColor != 0) {
        color  = aColor;
        color2 = NS_RGB(0,0,0);
      } else {
        PRUint8 rc = 0, gc = 0, bc = 0;
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

      aRenderingContext->PopState();
    }
  }
}

//------------------------------------------------------------------
PRIntn ReflowCountMgr::RemoveItems(PLHashEntry *he, PRIntn i, void *arg)
{
  char *str = (char *)he->key;
  ReflowCounter * counter = (ReflowCounter *)he->value;
  delete counter;
  NS_Free(str);

  return HT_ENUMERATE_REMOVE;
}

//------------------------------------------------------------------
PRIntn ReflowCountMgr::RemoveIndiItems(PLHashEntry *he, PRIntn i, void *arg)
{
  char *str = (char *)he->key;
  IndiReflowCounter * counter = (IndiReflowCounter *)he->value;
  delete counter;
  NS_Free(str);

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
      PL_HashTableAdd(mCounts, NS_strdup(kGrandTotalsStr), gTots);
    } else {
      gTots->ClearTotals();
    }

    printf("\t\t\t\tTotal\n");
    for (PRUint32 i=0;i<78;i++) {
      printf("-");
    }
    printf("\n");
    PL_HashTableEnumerateEntries(mCounts, DoSingleTotal, this);
  }
}

static void RecurseIndiTotals(nsPresContext* aPresContext, 
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
    printf("%d", counter->mCounter.GetTotal());
    printf("]\n");
    nsMemory::Free(name);
  }

  nsIFrame* child = aParentFrame->GetFirstChild(nsnull);
  while (child) {
    RecurseIndiTotals(aPresContext, aHT, child, aLevel+1);
    child = child->GetNextSibling();
  }

}

//------------------------------------------------------------------
PRIntn ReflowCountMgr::DoSingleIndi(PLHashEntry *he, PRIntn i, void *arg)
{
  IndiReflowCounter * counter = (IndiReflowCounter *)he->value;
  if (counter && !counter->mHasBeenOutput) {
    char * name = ToNewCString(counter->mName);
    printf("%s - %p   [%d][", name, (void*)counter->mFrame, counter->mCount);
    printf("%d", counter->mCounter.GetTotal());
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
      nsIFrame * rootFrame = mPresShell->FrameManager()->GetRootFrame();
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
      PL_HashTableAdd(mCounts, NS_strdup(kGrandTotalsStr), gTots);
    } else {
      gTots->ClearTotals();
    }

    static const char * title[] = {"Class", "Reflows"};
    fprintf(mFD, "<tr>");
    for (PRUint32 i=0; i < NS_ARRAY_LENGTH(title); i++) {
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
      PL_HashTableAdd(mCounts, NS_strdup(kGrandTotalsStr), gTots);
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
  char buf[8];

  PR_snprintf(buf, sizeof(buf), "#%02x%02x%02x",
              NS_GET_R(aColor), NS_GET_G(aColor), NS_GET_B(aColor));
  CopyASCIItoUTF16(buf, aString);
}

nsIFrame* nsIPresShell::GetAbsoluteContainingBlock(nsIFrame *aFrame)
{
  return FrameConstructor()->GetAbsoluteContainingBlock(aFrame);
}

void nsIPresShell::InitializeStatics()
{
  NS_ASSERTION(sLiveShells == nsnull, "InitializeStatics called multiple times!");
  sLiveShells = new nsTHashtable<PresShellPtrKey>();
  sLiveShells->Init();
}

void nsIPresShell::ReleaseStatics()
{
  NS_ASSERTION(sLiveShells, "ReleaseStatics called without Initialize!");
  delete sLiveShells;
  sLiveShells = nsnull;
}

// Asks our docshell whether we're active.
void PresShell::QueryIsActive()
{
  nsCOMPtr<nsISupports> container = mPresContext->GetContainer();
  nsCOMPtr<nsIDocShell> docshell(do_QueryInterface(container));
  if (docshell) {
    PRBool isActive;
    nsresult rv = docshell->GetIsActive(&isActive);
    if (NS_SUCCEEDED(rv))
      SetIsActive(isActive);
  }
}

nsresult
PresShell::SetIsActive(PRBool aIsActive)
{
  mIsActive = aIsActive;
  nsPresContext* presContext = GetPresContext();
  if (presContext &&
      presContext->RefreshDriver()->PresContext() == presContext) {
    presContext->RefreshDriver()->SetThrottled(!mIsActive);
  }
  return UpdateImageLockingState();
}

/*
 * Determines the current image locking state. Called when one of the
 * dependent factors changes.
 */
nsresult
PresShell::UpdateImageLockingState()
{
  // We're locked if we're both thawed and active.
  return mDocument->SetImageLockingState(!mFrozen && mIsActive);
}

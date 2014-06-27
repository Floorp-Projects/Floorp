/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
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

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif
#include "prlog.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/CSSStyleSheet.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/EventStates.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/Likely.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"
#include <algorithm>

#ifdef XP_WIN
#include "winuser.h"
#endif

#include "nsPresShell.h"
#include "nsPresContext.h"
#include "nsIContent.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h" // for Event::GetEventPopupControlState()
#include "mozilla/dom/ShadowRoot.h"
#include "mozilla/dom/PointerEvent.h"
#include "nsIDocument.h"
#include "nsAnimationManager.h"
#include "nsNameSpaceManager.h"  // for Pref-related rule management (bugs 22963,20760,31816)
#include "nsFrame.h"
#include "FrameLayerBuilder.h"
#include "nsViewManager.h"
#include "nsView.h"
#include "nsCRTGlue.h"
#include "prprf.h"
#include "prinrval.h"
#include "nsTArray.h"
#include "nsCOMArray.h"
#include "nsContainerFrame.h"
#include "nsISelection.h"
#include "mozilla/dom/Selection.h"
#include "nsGkAtoms.h"
#include "nsIDOMRange.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMElement.h"
#include "nsRange.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsReadableUtils.h"
#include "nsIPageSequenceFrame.h"
#include "nsCaret.h"
#include "TouchCaret.h"
#include "SelectionCarets.h"
#include "nsIDOMHTMLDocument.h"
#include "nsFrameManager.h"
#include "nsXPCOM.h"
#include "nsILayoutHistoryState.h"
#include "nsILineIterator.h" // for ScrollContentIntoView
#include "pldhash.h"
#include "mozilla/dom/Touch.h"
#include "mozilla/dom/PointerEventBinding.h"
#include "nsIObserverService.h"
#include "nsDocShell.h"        // for reflow observation
#include "nsIBaseWindow.h"
#include "nsError.h"
#include "nsLayoutUtils.h"
#include "nsViewportInfo.h"
#include "nsCSSRendering.h"
  // for |#ifdef DEBUG| code
#include "prenv.h"
#include "nsDisplayList.h"
#include "nsRegion.h"
#include "nsRenderingContext.h"
#include "nsAutoLayoutPhase.h"
#ifdef MOZ_REFLOW_PERF
#include "nsFontMetrics.h"
#endif
#include "PositionedEventTargeting.h"

#include "nsIReflowCallback.h"

#include "nsPIDOMWindow.h"
#include "nsFocusManager.h"
#include "nsIObjectFrame.h"
#include "nsIObjectLoadingContent.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "nsStyleSheetService.h"
#include "gfxImageSurface.h"
#include "gfxContext.h"
#include "gfxUtils.h"
#include "nsSMILAnimationController.h"
#include "SVGContentUtils.h"
#include "nsSVGEffects.h"
#include "SVGFragmentIdentifier.h"
#include "nsArenaMemoryStats.h"

#include "nsPerformance.h"
#include "nsRefreshDriver.h"
#include "nsDOMNavigationTiming.h"

// Drag & Drop, Clipboard
#include "nsIDocShellTreeItem.h"
#include "nsIURI.h"
#include "nsIScrollableFrame.h"
#include "nsITimer.h"
#ifdef ACCESSIBILITY
#include "nsAccessibilityService.h"
#include "mozilla/a11y/DocAccessible.h"
#ifdef DEBUG
#include "mozilla/a11y/Logging.h"
#endif
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

#include "GeckoProfiler.h"
#include "gfxPlatform.h"
#include "Layers.h"
#include "LayerTreeInvalidation.h"
#include "mozilla/css/ImageLoader.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"
#include "nsCanvasFrame.h"
#include "nsIImageLoadingContent.h"
#include "nsIScreen.h"
#include "nsIScreenManager.h"
#include "nsPlaceholderFrame.h"
#include "nsTransitionManager.h"
#include "ChildIterator.h"
#include "RestyleManager.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDragSession.h"
#include "nsIFrameInlines.h"
#include "mozilla/gfx/2D.h"
#include "nsSubDocumentFrame.h"

#ifdef ANDROID
#include "nsIDocShellTreeOwner.h"
#endif

#ifdef MOZ_TASK_TRACER
#include "GeckoTaskTracer.h"
using namespace mozilla::tasktracer;
#endif

#define ANCHOR_SCROLL_FLAGS \
  (nsIPresShell::SCROLL_OVERFLOW_HIDDEN | nsIPresShell::SCROLL_NO_PARENT_FRAMES)

using namespace mozilla;
using namespace mozilla::css;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::layers;
using namespace mozilla::gfx;
using namespace mozilla::layout;

CapturingContentInfo nsIPresShell::gCaptureInfo =
  { false /* mAllowed */, false /* mPointerLock */, false /* mRetargetToElement */,
    false /* mPreventDrag */, nullptr /* mContent */ };
nsIContent* nsIPresShell::gKeyDownTarget;
nsRefPtrHashtable<nsUint32HashKey, dom::Touch>* nsIPresShell::gCaptureTouchList;
nsRefPtrHashtable<nsUint32HashKey, nsIContent>* nsIPresShell::gPointerCaptureList;
nsClassHashtable<nsUint32HashKey, nsIPresShell::PointerInfo>* nsIPresShell::gActivePointersIds;
bool nsIPresShell::gPreventMouseEvents = false;

// convert a color value to a string, in the CSS format #RRGGBB
// *  - initially created for bugs 31816, 20760, 22963
static void ColorToString(nscolor aColor, nsAutoString &aString);

// RangePaintInfo is used to paint ranges to offscreen buffers
struct RangePaintInfo {
  nsRefPtr<nsRange> mRange;
  nsDisplayListBuilder mBuilder;
  nsDisplayList mList;

  // offset of builder's reference frame to the root frame
  nsPoint mRootOffset;

  RangePaintInfo(nsRange* aRange, nsIFrame* aFrame)
    : mRange(aRange), mBuilder(aFrame, nsDisplayListBuilder::PAINTING, false)
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

#ifdef DEBUG
// Set the environment variable GECKO_VERIFY_REFLOW_FLAGS to one or
// more of the following flags (comma separated) for handy debug
// output.
static uint32_t gVerifyReflowFlags;

struct VerifyReflowFlags {
  const char*    name;
  uint32_t bit;
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
  ReflowCounter(ReflowCountMgr * aMgr = nullptr);
  ~ReflowCounter();

  void ClearTotals();
  void DisplayTotals(const char * aStr);
  void DisplayDiffTotals(const char * aStr);
  void DisplayHTMLTotals(const char * aStr);

  void Add()                { mTotal++;         }
  void Add(uint32_t aTotal) { mTotal += aTotal; }

  void CalcDiffInTotals();
  void SetTotalsCache();

  void SetMgr(ReflowCountMgr * aMgr) { mMgr = aMgr; }

  uint32_t GetTotal() { return mTotal; }

protected:
  void DisplayTotals(uint32_t aTotal, const char * aTitle);
  void DisplayHTMLTotals(uint32_t aTotal, const char * aTitle);

  uint32_t mTotal;
  uint32_t mCacheTotal;

  ReflowCountMgr * mMgr; // weak reference (don't delete)
};

// Counting Class
class IndiReflowCounter {
public:
  IndiReflowCounter(ReflowCountMgr * aMgr = nullptr)
    : mFrame(nullptr),
      mCount(0),
      mMgr(aMgr),
      mCounter(aMgr),
      mHasBeenOutput(false)
    {}
  virtual ~IndiReflowCounter() {}

  nsAutoString mName;
  nsIFrame *   mFrame;   // weak reference (don't delete)
  int32_t      mCount;

  ReflowCountMgr * mMgr; // weak reference (don't delete)

  ReflowCounter mCounter;
  bool          mHasBeenOutput;

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

  void PaintCount(const char *aName, nsRenderingContext* aRenderingContext,
                  nsPresContext *aPresContext, nsIFrame *aFrame,
                  const nsPoint &aOffset, uint32_t aColor);

  FILE * GetOutFile() { return mFD; }

  PLHashTable * GetIndiFrameHT() { return mIndiFrameCounts; }

  void SetPresContext(nsPresContext * aPresContext) { mPresContext = aPresContext; } // weak reference
  void SetPresShell(nsIPresShell* aPresShell) { mPresShell= aPresShell; } // weak reference

  void SetDumpFrameCounts(bool aVal)         { mDumpFrameCounts = aVal; }
  void SetDumpFrameByFrameCounts(bool aVal)  { mDumpFrameByFrameCounts = aVal; }
  void SetPaintFrameCounts(bool aVal)        { mPaintFrameByFrameCounts = aVal; }

  bool IsPaintingFrameCounts() { return mPaintFrameByFrameCounts; }

protected:
  void DisplayTotals(uint32_t aTotal, uint32_t * aDupArray, char * aTitle);
  void DisplayHTMLTotals(uint32_t aTotal, uint32_t * aDupArray, char * aTitle);

  static int RemoveItems(PLHashEntry *he, int i, void *arg);
  static int RemoveIndiItems(PLHashEntry *he, int i, void *arg);
  void CleanUp();

  // stdout Output Methods
  static int DoSingleTotal(PLHashEntry *he, int i, void *arg);
  static int DoSingleIndi(PLHashEntry *he, int i, void *arg);

  void DoGrandTotals();
  void DoIndiTotalsTree();

  // HTML Output Methods
  static int DoSingleHTMLTotal(PLHashEntry *he, int i, void *arg);
  void DoGrandHTMLTotals();

  // Zero Out the Totals
  static int DoClearTotals(PLHashEntry *he, int i, void *arg);

  // Displays the Diff Totals
  static int DoDisplayDiffTotals(PLHashEntry *he, int i, void *arg);

  PLHashTable * mCounts;
  PLHashTable * mIndiFrameCounts;
  FILE * mFD;

  bool mDumpFrameCounts;
  bool mDumpFrameByFrameCounts;
  bool mPaintFrameByFrameCounts;

  bool mCycledOnce;

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
static int32_t gMaxRCProcessingTime = -1;

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

class MOZ_STACK_CLASS nsPresShellEventCB : public EventDispatchingCallback
{
public:
  nsPresShellEventCB(PresShell* aPresShell) : mPresShell(aPresShell) {}

  virtual void HandleEvent(EventChainPostVisitor& aVisitor) MOZ_OVERRIDE
  {
    if (aVisitor.mPresContext && aVisitor.mEvent->eventStructType != NS_EVENT) {
      if (aVisitor.mEvent->message == NS_MOUSE_BUTTON_DOWN ||
          aVisitor.mEvent->message == NS_MOUSE_BUTTON_UP) {
        // Mouse-up and mouse-down events call nsFrame::HandlePress/Release
        // which call GetContentOffsetsFromPoint which requires up-to-date layout.
        // Bring layout up-to-date now so that GetCurrentEventFrame() below
        // will return a real frame and we don't have to worry about
        // destroying it by flushing later.
        mPresShell->FlushPendingNotifications(Flush_Layout);
      } else if (aVisitor.mEvent->message == NS_WHEEL_WHEEL &&
                 aVisitor.mEventStatus != nsEventStatus_eConsumeNoDefault) {
        nsIFrame* frame = mPresShell->GetCurrentEventFrame();
        if (frame) {
          // chrome (including addons) should be able to know if content
          // handles both D3E "wheel" event and legacy mouse scroll events.
          // We should dispatch legacy mouse events before dispatching the
          // "wheel" event into system group.
          nsRefPtr<EventStateManager> esm =
            aVisitor.mPresContext->EventStateManager();
          esm->DispatchLegacyMouseScrollEvents(frame,
                                               aVisitor.mEvent->AsWheelEvent(),
                                               &aVisitor.mEventStatus);
        }
      }
      nsIFrame* frame = mPresShell->GetCurrentEventFrame();
      if (!frame &&
          (aVisitor.mEvent->message == NS_MOUSE_BUTTON_UP ||
           aVisitor.mEvent->message == NS_TOUCH_END)) {
        // Redirect BUTTON_UP and TOUCH_END events to the root frame to ensure
        // that capturing is released.
        frame = mPresShell->GetRootFrame();
      }
      if (frame) {
        frame->HandleEvent(aVisitor.mPresContext,
                           aVisitor.mEvent->AsGUIEvent(),
                           &aVisitor.mEventStatus);
      }
    }
  }

  nsRefPtr<PresShell> mPresShell;
};

class nsBeforeFirstPaintDispatcher : public nsRunnable
{
public:
  nsBeforeFirstPaintDispatcher(nsIDocument* aDocument)
  : mDocument(aDocument) {}

  // Fires the "before-first-paint" event so that interested parties (right now, the
  // mobile browser) are aware of it.
  NS_IMETHOD Run() MOZ_OVERRIDE
  {
    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
    if (observerService) {
      observerService->NotifyObservers(mDocument, "before-first-paint",
                                       nullptr);
    }
    return NS_OK;
  }

private:
  nsCOMPtr<nsIDocument> mDocument;
};

bool PresShell::sDisableNonTestMouseEvents = false;

#ifdef PR_LOGGING
PRLogModuleInfo* PresShell::gLog;
#endif

#ifdef DEBUG
static void
VerifyStyleTree(nsPresContext* aPresContext, nsFrameManager* aFrameManager)
{
  if (nsFrame::GetVerifyStyleTreeEnable()) {
    nsIFrame* rootFrame = aFrameManager->GetRootFrame();
    aPresContext->RestyleManager()->DebugVerifyStyleTree(rootFrame);
  }
}
#define VERIFY_STYLE_TREE ::VerifyStyleTree(mPresContext, mFrameConstructor)
#else
#define VERIFY_STYLE_TREE
#endif

static bool gVerifyReflowEnabled;

bool
nsIPresShell::GetVerifyReflowEnable()
{
#ifdef DEBUG
  static bool firstTime = true;
  if (firstTime) {
    firstTime = false;
    char* flags = PR_GetEnv("GECKO_VERIFY_REFLOW_FLAGS");
    if (flags) {
      bool error = false;

      for (;;) {
        char* comma = PL_strchr(flags, ',');
        if (comma)
          *comma = '\0';

        bool found = false;
        const VerifyReflowFlags* flag = gFlags;
        const VerifyReflowFlags* limit = gFlags + NUM_VERIFY_REFLOW_FLAGS;
        while (flag < limit) {
          if (PL_strcasecmp(flag->name, flags) == 0) {
            gVerifyReflowFlags |= flag->bit;
            found = true;
            break;
          }
          ++flag;
        }

        if (! found)
          error = true;

        if (! comma)
          break;

        *comma = ',';
        flags = comma + 1;
      }

      if (error)
        ShowVerifyReflowFlags();
    }

    if (VERIFY_REFLOW_ON & gVerifyReflowFlags) {
      gVerifyReflowEnabled = true;

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
PresShell::AddInvalidateHiddenPresShellObserver(nsRefreshDriver *aDriver)
{
  if (!mHiddenInvalidationObserverRefreshDriver && !mIsDestroying && !mHaveShutDown) {
    aDriver->AddPresShellToInvalidateIfHidden(this);
    mHiddenInvalidationObserverRefreshDriver = aDriver;
  }
}

void
nsIPresShell::InvalidatePresShellIfHidden()
{
  if (!IsVisible() && mPresContext) {
    mPresContext->NotifyInvalidation(0);
  }
  mHiddenInvalidationObserverRefreshDriver = nullptr;
}

void
nsIPresShell::CancelInvalidatePresShellIfHidden()
{
  if (mHiddenInvalidationObserverRefreshDriver) {
    mHiddenInvalidationObserverRefreshDriver->RemovePresShellToInvalidateIfHidden(this);
    mHiddenInvalidationObserverRefreshDriver = nullptr;
  }
}

void
nsIPresShell::SetVerifyReflowEnable(bool aEnabled)
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
  nsRefPtr<nsFrameSelection> ret = mSelection;
  return ret.forget();
}

//----------------------------------------------------------------------

static bool sSynthMouseMove = true;
static uint32_t sNextPresShellId;
static bool sPointerEventEnabled = true;
static bool sTouchCaretEnabled = false;
static bool sSelectionCaretEnabled = false;

/* static */ bool
PresShell::TouchCaretPrefEnabled()
{
  static bool initialized = false;
  if (!initialized) {
    Preferences::AddBoolVarCache(&sTouchCaretEnabled, "touchcaret.enabled");
    initialized = true;
  }
  return sTouchCaretEnabled;
}

/* static */ bool
PresShell::SelectionCaretPrefEnabled()
{
  static bool initialized = false;
  if (!initialized) {
    Preferences::AddBoolVarCache(&sSelectionCaretEnabled, "selectioncaret.enabled");
    initialized = true;
  }
  return sSelectionCaretEnabled;
}

PresShell::PresShell()
  : mMouseLocation(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE)
{
  mSelection = nullptr;
#ifdef MOZ_REFLOW_PERF
  mReflowCountMgr = new ReflowCountMgr();
  mReflowCountMgr->SetPresContext(mPresContext);
  mReflowCountMgr->SetPresShell(this);
#endif
#ifdef PR_LOGGING
  mLoadBegin = TimeStamp::Now();
  if (!gLog) {
    gLog = PR_NewLogModule("PresShell");
  }
#endif
  mSelectionFlags = nsISelectionDisplay::DISPLAY_TEXT | nsISelectionDisplay::DISPLAY_IMAGES;
  mIsThemeSupportDisabled = false;
  mIsActive = true;
  // FIXME/bug 735029: find a better solution to this problem
#ifdef MOZ_ANDROID_OMTC
  // The java pan/zoom code uses this to mean approximately "request a
  // reset of pan/zoom state" which doesn't necessarily correspond
  // with the first paint of content.
  mIsFirstPaint = false;
#else
  mIsFirstPaint = true;
#endif
  mPresShellId = sNextPresShellId++;
  mFrozen = false;
#ifdef DEBUG
  mPresArenaAllocCount = 0;
#endif
  mRenderFlags = 0;
  mXResolution = 1.0;
  mYResolution = 1.0;
  mViewportOverridden = false;

  mScrollPositionClampingScrollPortSizeSet = false;

  mMaxLineBoxWidth = 0;

  static bool addedSynthMouseMove = false;
  if (!addedSynthMouseMove) {
    Preferences::AddBoolVarCache(&sSynthMouseMove,
                                 "layout.reflow.synthMouseMove", true);
    addedSynthMouseMove = true;
  }
  static bool addedPointerEventEnabled = false;
  if (!addedPointerEventEnabled) {
    Preferences::AddBoolVarCache(&sPointerEventEnabled,
                                 "dom.w3c_pointer_events.enabled", true);
    addedPointerEventEnabled = true;
  }

  mPaintingIsFrozen = false;
}

NS_IMPL_ISUPPORTS(PresShell, nsIPresShell, nsIDocumentObserver,
                  nsISelectionController,
                  nsISelectionDisplay, nsIObserver, nsISupportsWeakReference,
                  nsIMutationObserver)

PresShell::~PresShell()
{
  if (!mHaveShutDown) {
    NS_NOTREACHED("Someone did not call nsIPresShell::destroy");
    Destroy();
  }

  NS_ASSERTION(mCurrentEventContentStack.Count() == 0,
               "Huh, event content left on the stack in pres shell dtor!");
  NS_ASSERTION(mFirstCallbackEventRequest == nullptr &&
               mLastCallbackEventRequest == nullptr,
               "post-reflow queues not empty.  This means we're leaking");

  // Verify that if painting was frozen, but we're being removed from the tree,
  // that we now re-enable painting on our refresh driver, since it may need to
  // be re-used by another presentation.
  if (mPaintingIsFrozen) {
    mPresContext->RefreshDriver()->Thaw();
  }

#ifdef DEBUG
  MOZ_ASSERT(mPresArenaAllocCount == 0,
             "Some pres arena objects were not freed");
#endif

  delete mStyleSet;
  delete mFrameConstructor;

  mCurrentEventContent = nullptr;

  NS_IF_RELEASE(mPresContext);
  NS_IF_RELEASE(mDocument);
  NS_IF_RELEASE(mSelection);
}

/**
 * Initialize the presentation shell. Create view manager and style
 * manager.
 * Note this can't be merged into our constructor because caret initialization
 * calls AddRef() on us.
 */
void
PresShell::Init(nsIDocument* aDocument,
                nsPresContext* aPresContext,
                nsViewManager* aViewManager,
                nsStyleSet* aStyleSet,
                nsCompatibility aCompatMode)
{
  NS_PRECONDITION(aDocument, "null ptr");
  NS_PRECONDITION(aPresContext, "null ptr");
  NS_PRECONDITION(aViewManager, "null ptr");
  NS_PRECONDITION(!mDocument, "already initialized");

  if (!aDocument || !aPresContext || !aViewManager || mDocument) {
    return;
  }

  mDocument = aDocument;
  NS_ADDREF(mDocument);
  mViewManager = aViewManager;

  // Create our frame constructor.
  mFrameConstructor = new nsCSSFrameConstructor(mDocument, this, aStyleSet);

  mFrameManager = mFrameConstructor;

  // The document viewer owns both view manager and pres shell.
  mViewManager->SetPresShell(this);

  // Bind the context to the presentation shell.
  mPresContext = aPresContext;
  NS_ADDREF(mPresContext);
  aPresContext->SetShell(this);

  // Now we can initialize the style set.
  aStyleSet->Init(aPresContext);
  mStyleSet = aStyleSet;

  // Notify our prescontext that it now has a compatibility mode.  Note that
  // this MUST happen after we set up our style set but before we create any
  // frames.
  mPresContext->CompatibilityModeChanged();

  // setup the preference style rules (no forced reflow), and do it
  // before creating any frames.
  SetPreferenceStyleRules(false);

  if (TouchCaretPrefEnabled()) {
    // Create touch caret handle
    mTouchCaret = new TouchCaret(this);
  }

  if (SelectionCaretPrefEnabled()) {
    // Create selection caret handle
    mSelectionCarets = new SelectionCarets(this);
  }


  NS_ADDREF(mSelection = new nsFrameSelection());

  mSelection->Init(this, nullptr);

  // Important: this has to happen after the selection has been set up
#ifdef SHOW_CARET
  // make the caret
  mCaret = new nsCaret();
  mCaret->Init(this);
  mOriginalCaret = mCaret;

  //SetCaretEnabled(true);       // make it show in browser windows
#endif
  //set up selection to be displayed in document
  // Don't enable selection for print media
  nsPresContext::nsPresContextType type = aPresContext->Type();
  if (type != nsPresContext::eContext_PrintPreview &&
      type != nsPresContext::eContext_Print)
    SetDisplaySelection(nsISelectionController::SELECTION_DISABLED);

  if (gMaxRCProcessingTime == -1) {
    gMaxRCProcessingTime =
      Preferences::GetInt("layout.reflow.timeslice", NS_MAX_REFLOW_TIME);
  }

  {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->AddObserver(this, "agent-sheet-added", false);
      os->AddObserver(this, "user-sheet-added", false);
      os->AddObserver(this, "author-sheet-added", false);
      os->AddObserver(this, "agent-sheet-removed", false);
      os->AddObserver(this, "user-sheet-removed", false);
      os->AddObserver(this, "author-sheet-removed", false);
#ifdef MOZ_XUL
      os->AddObserver(this, "chrome-flush-skin-caches", false);
#endif
    }
  }

#ifdef MOZ_REFLOW_PERF
    if (mReflowCountMgr) {
      bool paintFrameCounts =
        Preferences::GetBool("layout.reflow.showframecounts");

      bool dumpFrameCounts =
        Preferences::GetBool("layout.reflow.dumpframecounts");

      bool dumpFrameByFrameCounts =
        Preferences::GetBool("layout.reflow.dumpframebyframecounts");

      mReflowCountMgr->SetDumpFrameCounts(dumpFrameCounts);
      mReflowCountMgr->SetDumpFrameByFrameCounts(dumpFrameByFrameCounts);
      mReflowCountMgr->SetPaintFrameCounts(paintFrameCounts);
    }
#endif

  if (mDocument->HasAnimationController()) {
    nsSMILAnimationController* animCtrl = mDocument->GetAnimationController();
    animCtrl->NotifyRefreshDriverCreated(GetPresContext()->RefreshDriver());
  }

  // Get our activeness from the docShell.
  QueryIsActive();

  // Setup our font inflation preferences.
  SetupFontInflation();
}

#ifdef PR_LOGGING
enum TextPerfLogType {
  eLog_reflow,
  eLog_loaddone,
  eLog_totals
};

static void
LogTextPerfStats(gfxTextPerfMetrics* aTextPerf,
                 PresShell* aPresShell,
                 const gfxTextPerfMetrics::TextCounts& aCounts,
                 float aTime, TextPerfLogType aLogType, const char* aURL)
{
  char prefix[256];

  switch (aLogType) {
    case eLog_reflow:
      sprintf(prefix, "(textperf-reflow) %p time-ms: %7.0f", aPresShell, aTime);
      break;
    case eLog_loaddone:
      sprintf(prefix, "(textperf-loaddone) %p time-ms: %7.0f", aPresShell, aTime);
      break;
    default:
      MOZ_ASSERT(aLogType == eLog_totals, "unknown textperf log type");
      sprintf(prefix, "(textperf-totals) %p", aPresShell);
  }

  PRLogModuleInfo* tpLog = gfxPlatform::GetLog(eGfxLog_textperf);

  // ignore XUL contexts unless at debug level
  PRLogModuleLevel logLevel = PR_LOG_WARNING;
  if (aCounts.numContentTextRuns == 0) {
    logLevel = PR_LOG_DEBUG;
  }

  double hitRatio = 0.0;
  uint32_t lookups = aCounts.wordCacheHit + aCounts.wordCacheMiss;
  if (lookups) {
    hitRatio = double(aCounts.wordCacheHit) / double(lookups);
  }

  if (aLogType == eLog_loaddone) {
    PR_LOG(tpLog, logLevel,
           ("%s reflow: %d chars: %d "
            "[%s] "
            "content-textruns: %d chrome-textruns: %d "
            "max-textrun-len: %d "
            "word-cache-lookups: %d word-cache-hit-ratio: %4.3f "
            "word-cache-space: %d word-cache-long: %d "
            "pref-fallbacks: %d system-fallbacks: %d "
            "textruns-const: %d textruns-destr: %d "
            "cumulative-textruns-destr: %d\n",
            prefix, aTextPerf->reflowCount, aCounts.numChars,
            (aURL ? aURL : ""),
            aCounts.numContentTextRuns, aCounts.numChromeTextRuns,
            aCounts.maxTextRunLen,
            lookups, hitRatio,
            aCounts.wordCacheSpaceRules, aCounts.wordCacheLong,
            aCounts.fallbackPrefs, aCounts.fallbackSystem,
            aCounts.textrunConst, aCounts.textrunDestr,
            aTextPerf->cumulative.textrunDestr));
  } else {
    PR_LOG(tpLog, logLevel,
           ("%s reflow: %d chars: %d "
            "content-textruns: %d chrome-textruns: %d "
            "max-textrun-len: %d "
            "word-cache-lookups: %d word-cache-hit-ratio: %4.3f "
            "word-cache-space: %d word-cache-long: %d "
            "pref-fallbacks: %d system-fallbacks: %d "
            "textruns-const: %d textruns-destr: %d "
            "cumulative-textruns-destr: %d\n",
            prefix, aTextPerf->reflowCount, aCounts.numChars,
            aCounts.numContentTextRuns, aCounts.numChromeTextRuns,
            aCounts.maxTextRunLen,
            lookups, hitRatio,
            aCounts.wordCacheSpaceRules, aCounts.wordCacheLong,
            aCounts.fallbackPrefs, aCounts.fallbackSystem,
            aCounts.textrunConst, aCounts.textrunDestr,
            aTextPerf->cumulative.textrunDestr));
  }
}
#endif

void
PresShell::Destroy()
{
  NS_ASSERTION(!nsContentUtils::IsSafeToRunScript(),
    "destroy called on presshell while scripts not blocked");

  // dump out cumulative text perf metrics
#ifdef PR_LOGGING
  gfxTextPerfMetrics* tp;
  if (mPresContext && (tp = mPresContext->GetTextPerfMetrics())) {
    tp->Accumulate();
    if (tp->cumulative.numChars > 0) {
      LogTextPerfStats(tp, this, tp->cumulative, 0.0, eLog_totals, nullptr);
    }
  }
#endif

#ifdef MOZ_REFLOW_PERF
  DumpReflows();
  if (mReflowCountMgr) {
    delete mReflowCountMgr;
    mReflowCountMgr = nullptr;
  }
#endif

  if (mHaveShutDown)
    return;

#ifdef ACCESSIBILITY
  if (mDocAccessible) {
#ifdef DEBUG
    if (a11y::logging::IsEnabled(a11y::logging::eDocDestroy))
      a11y::logging::DocDestroy("presshell destroyed", mDocument);
#endif

    mDocAccessible->Shutdown();
    mDocAccessible = nullptr;
  }
#endif // ACCESSIBILITY

  MaybeReleaseCapturingContent();

  if (gKeyDownTarget && gKeyDownTarget->OwnerDoc() == mDocument) {
    NS_RELEASE(gKeyDownTarget);
  }

  if (mContentToScrollTo) {
    mContentToScrollTo->DeleteProperty(nsGkAtoms::scrolling);
    mContentToScrollTo = nullptr;
  }

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
      os->RemoveObserver(this, "author-sheet-added");
      os->RemoveObserver(this, "agent-sheet-removed");
      os->RemoveObserver(this, "user-sheet-removed");
      os->RemoveObserver(this, "author-sheet-removed");
#ifdef MOZ_XUL
      os->RemoveObserver(this, "chrome-flush-skin-caches");
#endif
    }
  }

  // If our paint suppression timer is still active, kill it.
  if (mPaintSuppressionTimer) {
    mPaintSuppressionTimer->Cancel();
    mPaintSuppressionTimer = nullptr;
  }

  // Same for our reflow continuation timer
  if (mReflowContinueTimer) {
    mReflowContinueTimer->Cancel();
    mReflowContinueTimer = nullptr;
  }

  if (mDelayedPaintTimer) {
    mDelayedPaintTimer->Cancel();
    mDelayedPaintTimer = nullptr;
  }

  mSynthMouseMoveEvent.Revoke();

  mUpdateImageVisibilityEvent.Revoke();

  ClearVisibleImagesList();

  if (mCaret) {
    mCaret->Terminate();
    mCaret = nullptr;
  }

  if (mSelection) {
    mSelection->DisconnectFromPresShell();
  }

  if (mTouchCaret) {
    mTouchCaret->Terminate();
    mTouchCaret = nullptr;
  }

  if (mSelectionCarets) {
    mSelectionCarets->Terminate();
    mSelectionCarets = nullptr;
  }

  // release our pref style sheet, if we have one still
  ClearPreferenceStyleRules();

  mIsDestroying = true;

  // We can't release all the event content in
  // mCurrentEventContentStack here since there might be code on the
  // stack that will release the event content too. Double release
  // bad!

  // The frames will be torn down, so remove them from the current
  // event frame stack (since they'd be dangling references if we'd
  // leave them in) and null out the mCurrentEventFrame pointer as
  // well.

  mCurrentEventFrame = nullptr;

  int32_t i, count = mCurrentEventFrameStack.Length();
  for (i = 0; i < count; i++) {
    mCurrentEventFrameStack[i] = nullptr;
  }

  mFramesToDirty.Clear();

  if (mViewManager) {
    // Clear the view manager's weak pointer back to |this| in case it
    // was leaked.
    mViewManager->SetPresShell(nullptr);
    mViewManager = nullptr;
  }

  mStyleSet->BeginShutdown(mPresContext);
  nsRefreshDriver* rd = GetPresContext()->RefreshDriver();

  // This shell must be removed from the document before the frame
  // hierarchy is torn down to avoid finding deleted frames through
  // this presshell while the frames are being torn down
  if (mDocument) {
    NS_ASSERTION(mDocument->GetShell() == this, "Wrong shell?");
    mDocument->DeleteShell();

    if (mDocument->HasAnimationController()) {
      mDocument->GetAnimationController()->NotifyRefreshDriverDestroying(rd);
    }
  }

  // Revoke any pending events.  We need to do this and cancel pending reflows
  // before we destroy the frame manager, since apparently frame destruction
  // sometimes spins the event queue when plug-ins are involved(!).
  rd->RemoveLayoutFlushObserver(this);
  if (mHiddenInvalidationObserverRefreshDriver) {
    mHiddenInvalidationObserverRefreshDriver->RemovePresShellToInvalidateIfHidden(this);
  }

  if (rd->PresContext() == GetPresContext()) {
    rd->RevokeViewManagerFlush();
  }

  mResizeEvent.Revoke();
  if (mAsyncResizeTimerIsActive) {
    mAsyncResizeEventTimer->Cancel();
    mAsyncResizeTimerIsActive = false;
  }

  CancelAllPendingReflows();
  CancelPostedReflowCallbacks();

  // Destroy the frame manager. This will destroy the frame hierarchy
  mFrameConstructor->WillDestroyFrameTree();

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
    // pres shell to nullptr
    mPresContext->SetShell(nullptr);

    // Clear the link handler (weak reference) as well
    mPresContext->SetLinkHandler(nullptr);
  }

  mHaveShutDown = true;

  EvictTouches();
}

void
PresShell::MakeZombie()
{
  mIsZombie = true;
  CancelAllPendingReflows();
}

void
nsIPresShell::SetAuthorStyleDisabled(bool aStyleDisabled)
{
  if (aStyleDisabled != mStyleSet->GetAuthorStyleDisabled()) {
    mStyleSet->SetAuthorStyleDisabled(aStyleDisabled);
    ReconstructStyleData();

    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
    if (observerService) {
      observerService->NotifyObservers(mDocument,
                                       "author-style-disabled-changed",
                                       nullptr);
    }
  }
}

bool
nsIPresShell::GetAuthorStyleDisabled() const
{
  return mStyleSet->GetAuthorStyleDisabled();
}

nsresult
PresShell::SetPreferenceStyleRules(bool aForceReflow)
{
  if (!mDocument) {
    return NS_ERROR_NULL_POINTER;
  }

  nsPIDOMWindow *window = mDocument->GetWindow();

  // If the document doesn't have a window there's no need to notify
  // its presshell about changes to preferences since the document is
  // in a state where it doesn't matter any more (see
  // nsDocumentViewer::Close()).

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
#ifdef DEBUG
      int32_t numBefore = mStyleSet->SheetCount(nsStyleSet::eUserSheet);
      NS_ASSERTION(numBefore > 0, "no user stylesheets in styleset, but we have one!");
#endif
      mStyleSet->RemoveStyleSheet(nsStyleSet::eUserSheet, mPrefStyleSheet);

#ifdef DEBUG_attinasi
      NS_ASSERTION((numBefore - 1) == mStyleSet->GetNumberOfUserStyleSheets(),
                   "Pref stylesheet was not removed");
      printf("PrefStyleSheet removed\n");
#endif
      // clear the sheet pointer: it is strictly historical now
      mPrefStyleSheet = nullptr;
    }
  }
  return result;
}

nsresult
PresShell::CreatePreferenceStyleSheet()
{
  NS_ASSERTION(!mPrefStyleSheet, "prefStyleSheet already exists");
  mPrefStyleSheet = new CSSStyleSheet(CORS_NONE);
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), "about:PreferenceStyleSheet", nullptr);
  if (NS_FAILED(rv)) {
    mPrefStyleSheet = nullptr;
    return rv;
  }
  NS_ASSERTION(uri, "null but no error");
  mPrefStyleSheet->SetURIs(uri, uri, uri);
  mPrefStyleSheet->SetComplete();
  uint32_t index;
  rv =
    mPrefStyleSheet->InsertRuleInternal(NS_LITERAL_STRING("@namespace svg url(http://www.w3.org/2000/svg);"),
                                        0, &index);
  if (NS_FAILED(rv)) {
    mPrefStyleSheet = nullptr;
    return rv;
  }
  rv =
    mPrefStyleSheet->InsertRuleInternal(NS_LITERAL_STRING("@namespace url(http://www.w3.org/1999/xhtml);"),
                                        0, &index);
  if (NS_FAILED(rv)) {
    mPrefStyleSheet = nullptr;
    return rv;
  }

  mStyleSet->AppendStyleSheet(nsStyleSet::eUserSheet, mPrefStyleSheet);
  return NS_OK;
}

// XXX We want these after the @namespace rules.  Does order matter
// for these rules, or can we call StyleRule::StyleRuleCount()
// and just "append"?
static uint32_t sInsertPrefSheetRulesAt = 2;

nsresult
PresShell::SetPrefNoScriptRule()
{
  nsresult rv = NS_OK;

  // also handle the case where print is done from print preview
  // see bug #342439 for more details
  nsIDocument* doc = mDocument;
  if (doc->IsStaticDocument()) {
    doc = doc->GetOriginalDocument();
  }

  bool scriptEnabled = doc->IsScriptEnabled();
  if (scriptEnabled) {
    if (!mPrefStyleSheet) {
      rv = CreatePreferenceStyleSheet();
      NS_ENSURE_SUCCESS(rv, rv);
    }

    uint32_t index = 0;
    mPrefStyleSheet->
      InsertRuleInternal(NS_LITERAL_STRING("noscript{display:none!important}"),
                         sInsertPrefSheetRulesAt, &index);
  }

  return rv;
}

nsresult PresShell::SetPrefNoFramesRule(void)
{
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

  bool allowSubframes = true;
  nsCOMPtr<nsIDocShell> docShell(mPresContext->GetDocShell());
  if (docShell) {
    docShell->GetAllowSubframes(&allowSubframes);
  }
  if (!allowSubframes) {
    uint32_t index = 0;
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
  uint32_t index = 0;
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

  bool underlineLinks =
    mPresContext->GetCachedBoolPref(kPresContext_UnderlineLinks);

  if (underlineLinks) {
    // create a rule to make underlining happen
    //  '*|*:-moz-any-link {text-decoration:[underline|none];}'
    // no need for important, we want these to be overridable
    // NOTE: these must go in the agent stylesheet or they cannot be
    //       overridden by authors
    rv = mPrefStyleSheet->
      InsertRuleInternal(NS_LITERAL_STRING("*|*:-moz-any-link:not(svg|a){text-decoration:underline}"),
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
      uint32_t index = 0;
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
    uint8_t focusRingWidth = mPresContext->FocusRingWidth();
    bool focusRingOnAnything = mPresContext->GetFocusRingOnAnything();
    uint8_t focusRingStyle = mPresContext->GetFocusRingStyle();

    if ((NS_SUCCEEDED(result) && focusRingWidth != 1 && focusRingWidth <= 4 ) || focusRingOnAnything) {
      uint32_t index = 0;
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
  // Make sure this does what nsDocumentViewer::CreateStyleSet does wrt
  // ordering. We want this new sheet to come after all the existing stylesheet
  // service sheets, but before other user sheets; see nsIStyleSheetService.idl
  // for the ordering.  Just remove and readd all the nsStyleSheetService
  // sheets.
  nsCOMPtr<nsIStyleSheetService> dummy =
    do_GetService(NS_STYLESHEETSERVICE_CONTRACTID);

  mStyleSet->BeginUpdate();

  nsStyleSheetService *sheetService = nsStyleSheetService::gInstance;
  nsCOMArray<nsIStyleSheet> & userSheets = *sheetService->UserStyleSheets();
  int32_t i;
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
  // Make sure this does what nsDocumentViewer::CreateStyleSet does
  // wrt ordering.
  nsCOMPtr<nsIStyleSheet> sheet = do_QueryInterface(aSheet);
  if (!sheet) {
    return;
  }

  mStyleSet->AppendStyleSheet(nsStyleSet::eAgentSheet, sheet);
  ReconstructStyleData();
}

void
PresShell::AddAuthorSheet(nsISupports* aSheet)
{
  nsCOMPtr<nsIStyleSheet> sheet = do_QueryInterface(aSheet);
  if (!sheet) {
    return;
  }

  // Document specific "additional" Author sheets should be stronger than the ones
  // added with the StyleSheetService.
  nsIStyleSheet* firstAuthorSheet = mDocument->FirstAdditionalAuthorSheet();
  if (firstAuthorSheet) {
    mStyleSet->InsertStyleSheetBefore(nsStyleSet::eDocSheet, sheet, firstAuthorSheet);
  } else {
    mStyleSet->AppendStyleSheet(nsStyleSet::eDocSheet, sheet);
  }

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
PresShell::SetDisplaySelection(int16_t aToggle)
{
  mSelection->SetDisplaySelection(aToggle);
  return NS_OK;
}

NS_IMETHODIMP
PresShell::GetDisplaySelection(int16_t *aToggle)
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

Selection*
PresShell::GetCurrentSelection(SelectionType aType)
{
  if (!mSelection)
    return nullptr;

  return mSelection->GetSelection(aType);
}

NS_IMETHODIMP
PresShell::ScrollSelectionIntoView(SelectionType aType, SelectionRegion aRegion,
                                   int16_t aFlags)
{
  if (!mSelection)
    return NS_ERROR_NULL_POINTER;

  return mSelection->ScrollSelectionIntoView(aType, aRegion, aFlags);
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
      mIsDocumentGone = false;
    }
  }
}

// Make shell stop being a document observer
void
PresShell::EndObservingDocument()
{
  // XXXbz do we need to tell the frame constructor that the document
  // is gone, perhaps?  Except for printing it's NOT gone, sometimes.
  mIsDocumentGone = true;
  if (mDocument) {
    mDocument->RemoveObserver(this);
  }
}

#ifdef DEBUG_kipp
char* nsPresShell_ReflowStackPointerTop;
#endif

nsresult
PresShell::Initialize(nscoord aWidth, nscoord aHeight)
{
  if (mIsDestroying) {
    return NS_OK;
  }

  if (!mDocument) {
    // Nothing to do
    return NS_OK;
  }

  mozilla::TimeStamp timerStart = mozilla::TimeStamp::Now();

  NS_ASSERTION(!mDidInitialize, "Why are we being called?");

  nsCOMPtr<nsIPresShell> kungFuDeathGrip(this);
  mDidInitialize = true;

#ifdef DEBUG
  if (VERIFY_REFLOW_NOISY_RC & gVerifyReflowFlags) {
    if (mDocument) {
      nsIURI *uri = mDocument->GetDocumentURI();
      if (uri) {
        nsAutoCString url;
        uri->GetSpec(url);
        printf("*** PresShell::Initialize (this=%p, url='%s')\n", (void*)this, url.get());
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
  nsIFrame* rootFrame = mFrameConstructor->GetRootFrame();
  NS_ASSERTION(!rootFrame, "How did that happen, exactly?");
  if (!rootFrame) {
    nsAutoScriptBlocker scriptBlocker;
    mFrameConstructor->BeginUpdate();
    rootFrame = mFrameConstructor->ConstructRootFrame();
    mFrameConstructor->SetRootFrame(rootFrame);
    mFrameConstructor->EndUpdate();
  }

  NS_ENSURE_STATE(!mHaveShutDown);

  if (!rootFrame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (nsIFrame* f = rootFrame; f; f = nsLayoutUtils::GetCrossDocParentFrame(f)) {
    if (f->GetStateBits() & NS_FRAME_NO_COMPONENT_ALPHA) {
      f->InvalidateFrameSubtree();
      f->RemoveStateBits(NS_FRAME_NO_COMPONENT_ALPHA);
    }
  }

  Element *root = mDocument->GetRootElement();

  if (root) {
    {
      nsAutoCauseReflowNotifier reflowNotifier(this);
      mFrameConstructor->BeginUpdate();

      // Have the style sheet processor construct frame for the root
      // content object down
      mFrameConstructor->ContentInserted(nullptr, root, nullptr, false);
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

    // Constructors may have killed us too
    NS_ENSURE_STATE(!mHaveShutDown);

    // Now flush out pending restyles before we actually reflow, in
    // case XBL constructors changed styles somewhere.
    {
      nsAutoScriptBlocker scriptBlocker;
      mPresContext->RestyleManager()->ProcessPendingRestyles();
    }

    // And that might have run _more_ XBL constructors
    NS_ENSURE_STATE(!mHaveShutDown);
  }

  NS_ASSERTION(rootFrame, "How did that happen?");

  // Note: when the frame was created above it had the NS_FRAME_IS_DIRTY bit
  // set, but XBL processing could have caused a reflow which clears it.
  if (MOZ_LIKELY(rootFrame->GetStateBits() & NS_FRAME_IS_DIRTY)) {
    // Unset the DIRTY bits so that FrameNeedsReflow() will work right.
    rootFrame->RemoveStateBits(NS_FRAME_IS_DIRTY |
                               NS_FRAME_HAS_DIRTY_CHILDREN);
    NS_ASSERTION(!mDirtyRoots.Contains(rootFrame),
                 "Why is the root in mDirtyRoots already?");
    FrameNeedsReflow(rootFrame, eResize, NS_FRAME_IS_DIRTY);
    NS_ASSERTION(mDirtyRoots.Contains(rootFrame),
                 "Should be in mDirtyRoots now");
    NS_ASSERTION(mReflowScheduled, "Why no reflow scheduled?");
  }

  // Restore our root scroll position now if we're getting here after EndLoad
  // got called, since this is our one chance to do it.  Note that we need not
  // have reflowed for this to work; when the scrollframe is finally reflowed
  // it'll pick up the position we store in it here.
  if (!mDocumentLoading) {
    RestoreRootScrollPosition();
  }

  // For printing, we just immediately unsuppress.
  if (!mPresContext->IsPaginated()) {
    // Kick off a one-shot timer based off our pref value.  When this timer
    // fires, if painting is still locked down, then we will go ahead and
    // trigger a full invalidate and allow painting to proceed normally.
    mPaintingSuppressed = true;
    // Don't suppress painting if the document isn't loading.
    nsIDocument::ReadyState readyState = mDocument->GetReadyStateEnum();
    if (readyState != nsIDocument::READYSTATE_COMPLETE) {
      mPaintSuppressionTimer = do_CreateInstance("@mozilla.org/timer;1");
    }
    if (!mPaintSuppressionTimer) {
      mPaintingSuppressed = false;
    } else {
      // Initialize the timer.

      // Default to PAINTLOCK_EVENT_DELAY if we can't get the pref value.
      int32_t delay =
        Preferences::GetInt("nglayout.initialpaint.delay",
                            PAINTLOCK_EVENT_DELAY);

      mPaintSuppressionTimer->InitWithFuncCallback(sPaintSuppressionCallback,
                                                   this, delay,
                                                   nsITimer::TYPE_ONE_SHOT);
    }
  }

  if (root && root->IsXUL()) {
    mozilla::Telemetry::AccumulateTimeDelta(Telemetry::XUL_INITIAL_FRAME_CONSTRUCTION,
                                            timerStart);
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
PresShell::ResizeReflowOverride(nscoord aWidth, nscoord aHeight)
{
  mViewportOverridden = true;
  return ResizeReflowIgnoreOverride(aWidth, aHeight);
}

nsresult
PresShell::ResizeReflow(nscoord aWidth, nscoord aHeight)
{
  if (mViewportOverridden) {
    // The viewport has been overridden, and this reflow request
    // didn't ask to ignore the override.  Pretend it didn't happen.
    return NS_OK;
  }
  return ResizeReflowIgnoreOverride(aWidth, aHeight);
}

nsresult
PresShell::ResizeReflowIgnoreOverride(nscoord aWidth, nscoord aHeight)
{
  NS_PRECONDITION(!mIsReflowing, "Shouldn't be in reflow here!");
  NS_PRECONDITION(aWidth != NS_UNCONSTRAINEDSIZE,
                  "shouldn't use unconstrained widths anymore");

  // If we don't have a root frame yet, that means we haven't had our initial
  // reflow... If that's the case, and aWidth or aHeight is unconstrained,
  // ignore them altogether.
  nsIFrame* rootFrame = mFrameConstructor->GetRootFrame();
  if (!rootFrame && aHeight == NS_UNCONSTRAINEDSIZE) {
    // We can't do the work needed for SizeToContent without a root
    // frame, and we want to return before setting the visible area.
    return NS_ERROR_NOT_AVAILABLE;
  }

  mPresContext->SetVisibleArea(nsRect(0, 0, aWidth, aHeight));

  // There isn't anything useful we can do if the initial reflow hasn't happened.
  if (!rootFrame) {
    return NS_OK;
  }

  nsRefPtr<nsViewManager> viewManagerDeathGrip = mViewManager;
  // Take this ref after viewManager so it'll make sure to go away first.
  nsCOMPtr<nsIPresShell> kungFuDeathGrip(this);

  if (!GetPresContext()->SupressingResizeReflow()) {
    // Have to make sure that the content notifications are flushed before we
    // start messing with the frame model; otherwise we can get content doubling.
    mDocument->FlushPendingNotifications(Flush_ContentAndNotify);

    // Make sure style is up to date
    {
      nsAutoScriptBlocker scriptBlocker;
      mPresContext->RestyleManager()->ProcessPendingRestyles();
    }

    rootFrame = mFrameConstructor->GetRootFrame();
    if (!mIsDestroying && rootFrame) {
      // XXX Do a full invalidate at the beginning so that invalidates along
      // the way don't have region accumulation issues?

      {
        nsAutoCauseReflowNotifier crNotifier(this);
        WillDoReflow();

        // Kick off a top-down reflow
        AUTO_LAYOUT_PHASE_ENTRY_POINT(GetPresContext(), Reflow);
        nsViewManager::AutoDisableRefresh refreshBlocker(mViewManager);

        mDirtyRoots.RemoveElement(rootFrame);
        DoReflow(rootFrame, true);
      }

      DidDoReflow(true, false);
    }
  }

  rootFrame = mFrameConstructor->GetRootFrame();
  if (aHeight == NS_UNCONSTRAINEDSIZE && rootFrame) {
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
        mAsyncResizeTimerIsActive = true;
        mAsyncResizeEventTimer->InitWithFuncCallback(AsyncResizeEventCallback,
                                                     this, 15,
                                                     nsITimer::TYPE_ONE_SHOT);
      }
    } else {
      nsRefPtr<nsRunnableMethod<PresShell> > resizeEvent =
        NS_NewRunnableMethod(this, &PresShell::FireResizeEvent);
      if (NS_SUCCEEDED(NS_DispatchToCurrentThread(resizeEvent))) {
        mResizeEvent = resizeEvent;
        mDocument->SetNeedStyleFlush();
      }
    }
  }

  return NS_OK; //XXX this needs to be real. MMP
}

void
PresShell::FireResizeEvent()
{
  if (mAsyncResizeTimerIsActive) {
    mAsyncResizeTimerIsActive = false;
    mAsyncResizeEventTimer->Cancel();
  }
  mResizeEvent.Revoke();

  if (mIsDocumentGone)
    return;

  //Send resize event from here.
  WidgetEvent event(true, NS_RESIZE_EVENT);
  nsEventStatus status = nsEventStatus_eIgnore;

  nsPIDOMWindow *window = mDocument->GetWindow();
  if (window) {
    nsCOMPtr<nsIPresShell> kungFuDeathGrip(this);
    mInResize = true;
    EventDispatcher::Dispatch(window, mPresContext, &event, nullptr, &status);
    mInResize = false;
  }
}

void
PresShell::SetIgnoreFrameDestruction(bool aIgnore)
{
  if (mDocument) {
    // We need to tell the ImageLoader to drop all its references to frames
    // because they're about to go away and it won't get notifications of that.
    mDocument->StyleImageLoader()->ClearFrames();
  }
  mIgnoreFrameDestruction = aIgnore;
}

void
PresShell::NotifyDestroyingFrame(nsIFrame* aFrame)
{
  if (!mIgnoreFrameDestruction) {
    mDocument->StyleImageLoader()->DropRequestsForFrame(aFrame);

    mFrameConstructor->NotifyDestroyingFrame(aFrame);

    for (int32_t idx = mDirtyRoots.Length(); idx; ) {
      --idx;
      if (mDirtyRoots[idx] == aFrame) {
        mDirtyRoots.RemoveElementAt(idx);
      }
    }

    // Remove frame properties
    mPresContext->NotifyDestroyingFrame(aFrame);

    if (aFrame == mCurrentEventFrame) {
      mCurrentEventContent = aFrame->GetContent();
      mCurrentEventFrame = nullptr;
    }

  #ifdef DEBUG
    if (aFrame == mDrawEventTargetFrame) {
      mDrawEventTargetFrame = nullptr;
    }
  #endif

    for (unsigned int i=0; i < mCurrentEventFrameStack.Length(); i++) {
      if (aFrame == mCurrentEventFrameStack.ElementAt(i)) {
        //One of our stack frames was deleted.  Get its content so that when we
        //pop it we can still get its new frame from its content
        nsIContent *currentEventContent = aFrame->GetContent();
        mCurrentEventContentStack.ReplaceObjectAt(currentEventContent, i);
        mCurrentEventFrameStack[i] = nullptr;
      }
    }

    mFramesToDirty.RemoveEntry(aFrame);
  } else {
    // We must delete this property in situ so that its destructor removes the
    // frame from FrameLayerBuilder::DisplayItemData::mFrameList -- otherwise
    // the DisplayItemData destructor will use the destroyed frame when it
    // tries to remove it from the (array) value of this property.
    mPresContext->PropertyTable()->
      Delete(aFrame, FrameLayerBuilder::LayerManagerDataProperty());
  }
}

already_AddRefed<nsCaret> PresShell::GetCaret() const
{
  nsRefPtr<nsCaret> caret = mCaret;
  return caret.forget();
}

// TouchCaret
already_AddRefed<TouchCaret> PresShell::GetTouchCaret() const
{
  nsRefPtr<TouchCaret> touchCaret = mTouchCaret;
  return touchCaret.forget();
}

already_AddRefed<SelectionCarets> PresShell::GetSelectionCarets() const
{
  nsRefPtr<SelectionCarets> selectionCaret = mSelectionCarets;
  return selectionCaret.forget();
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

NS_IMETHODIMP PresShell::SetCaretEnabled(bool aInEnable)
{
  bool oldEnabled = mCaretEnabled;

  mCaretEnabled = aInEnable;

  if (mCaretEnabled != oldEnabled)
  {
/*  Don't change the caret's selection here! This was an evil side-effect of SetCaretEnabled()
    nsCOMPtr<nsIDOMSelection> domSel;
    if (NS_SUCCEEDED(GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSel))) && domSel)
      mCaret->SetCaretDOMSelection(domSel);
*/

    MOZ_ASSERT(mCaret || mTouchCaret);
    if (mCaret) {
      mCaret->SetCaretVisible(mCaretEnabled);
    }
    if (mTouchCaret) {
      mTouchCaret->UpdateTouchCaret(mCaretEnabled);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP PresShell::SetCaretReadOnly(bool aReadOnly)
{
  if (mCaret)
    mCaret->SetCaretReadOnly(aReadOnly);
  return NS_OK;
}

NS_IMETHODIMP PresShell::GetCaretEnabled(bool *aOutEnabled)
{
  NS_ENSURE_ARG_POINTER(aOutEnabled);
  *aOutEnabled = mCaretEnabled;
  return NS_OK;
}

NS_IMETHODIMP PresShell::SetCaretVisibilityDuringSelection(bool aVisibility)
{
  if (mCaret)
    mCaret->SetVisibilityDuringSelection(aVisibility);
  return NS_OK;
}

NS_IMETHODIMP PresShell::GetCaretVisible(bool *aOutIsVisible)
{
  *aOutIsVisible = false;
  if (mCaret) {
    nsresult rv = mCaret->GetCaretVisible(aOutIsVisible);
    NS_ENSURE_SUCCESS(rv,rv);
  }
  return NS_OK;
}

NS_IMETHODIMP PresShell::SetSelectionFlags(int16_t aInEnable)
{
  mSelectionFlags = aInEnable;
  return NS_OK;
}

NS_IMETHODIMP PresShell::GetSelectionFlags(int16_t *aOutEnable)
{
  if (!aOutEnable)
    return NS_ERROR_INVALID_ARG;
  *aOutEnable = mSelectionFlags;
  return NS_OK;
}

//implementation of nsISelectionController

NS_IMETHODIMP
PresShell::CharacterMove(bool aForward, bool aExtend)
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
PresShell::WordMove(bool aForward, bool aExtend)
{
  nsresult result = mSelection->WordMove(aForward, aExtend);
// if we can't go down/up any more we must then move caret completely to
// end/beginning respectively.
  if (NS_FAILED(result))
    result = CompleteMove(aForward, aExtend);
  return result;
}

NS_IMETHODIMP
PresShell::WordExtendForDelete(bool aForward)
{
  return mSelection->WordExtendForDelete(aForward);
}

NS_IMETHODIMP
PresShell::LineMove(bool aForward, bool aExtend)
{
  nsresult result = mSelection->LineMove(aForward, aExtend);
// if we can't go down/up any more we must then move caret completely to
// end/beginning respectively.
  if (NS_FAILED(result))
    result = CompleteMove(aForward,aExtend);
  return result;
}

NS_IMETHODIMP
PresShell::IntraLineMove(bool aForward, bool aExtend)
{
  return mSelection->IntraLineMove(aForward, aExtend);
}



NS_IMETHODIMP
PresShell::PageMove(bool aForward, bool aExtend)
{
  nsIScrollableFrame *scrollableFrame =
    GetFrameToScrollAsScrollable(nsIPresShell::eVertical);
  if (!scrollableFrame)
    return NS_OK;

  mSelection->CommonPageMove(aForward, aExtend, scrollableFrame);
  // After ScrollSelectionIntoView(), the pending notifications might be
  // flushed and PresShell/PresContext/Frames may be dead. See bug 418470.
  return ScrollSelectionIntoView(nsISelectionController::SELECTION_NORMAL,
                                 nsISelectionController::SELECTION_FOCUS_REGION,
                                 nsISelectionController::SCROLL_SYNCHRONOUS);
}



NS_IMETHODIMP
PresShell::ScrollPage(bool aForward)
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
PresShell::ScrollLine(bool aForward)
{
  nsIScrollableFrame* scrollFrame =
    GetFrameToScrollAsScrollable(nsIPresShell::eVertical);
  if (scrollFrame) {
    int32_t lineCount = Preferences::GetInt("toolkit.scrollbox.verticalScrollDistance",
                                            NS_DEFAULT_VERTICAL_SCROLL_DISTANCE);
    scrollFrame->ScrollBy(nsIntPoint(0, aForward ? lineCount : -lineCount),
                          nsIScrollableFrame::LINES,
                          nsIScrollableFrame::SMOOTH);
  }
  return NS_OK;
}

NS_IMETHODIMP
PresShell::ScrollCharacter(bool aRight)
{
  nsIScrollableFrame* scrollFrame =
    GetFrameToScrollAsScrollable(nsIPresShell::eHorizontal);
  if (scrollFrame) {
    int32_t h = Preferences::GetInt("toolkit.scrollbox.horizontalScrollDistance",
                                    NS_DEFAULT_HORIZONTAL_SCROLL_DISTANCE);
    scrollFrame->ScrollBy(nsIntPoint(aRight ? h : -h, 0),
                          nsIScrollableFrame::LINES,
                          nsIScrollableFrame::SMOOTH);
  }
  return NS_OK;
}

NS_IMETHODIMP
PresShell::CompleteScroll(bool aForward)
{
  nsIScrollableFrame* scrollFrame =
    GetFrameToScrollAsScrollable(nsIPresShell::eVertical);
  if (scrollFrame) {
    scrollFrame->ScrollBy(nsIntPoint(0, aForward ? 1 : -1),
                          nsIScrollableFrame::WHOLE,
                          nsIScrollableFrame::SMOOTH);
  }
  return NS_OK;
}

NS_IMETHODIMP
PresShell::CompleteMove(bool aForward, bool aExtend)
{
  // Beware! This may flush notifications via synchronous
  // ScrollSelectionIntoView.
  nsIContent* limiter = mSelection->GetAncestorLimiter();
  nsIFrame* frame = limiter ? limiter->GetPrimaryFrame()
                            : FrameConstructor()->GetRootElementFrame();
  if (!frame)
    return NS_ERROR_FAILURE;
  nsIFrame::CaretPosition pos =
    frame->GetExtremeCaretPosition(!aForward);
  mSelection->HandleClick(pos.mResultContent, pos.mContentOffset,
                          pos.mContentOffset, aExtend, false, aForward);
  if (limiter) {
    // HandleClick resets ancestorLimiter, so set it again.
    mSelection->SetAncestorLimiter(limiter);
  }

  // After ScrollSelectionIntoView(), the pending notifications might be
  // flushed and PresShell/PresContext/Frames may be dead. See bug 418470.
  return ScrollSelectionIntoView(nsISelectionController::SELECTION_NORMAL,
                                 nsISelectionController::SELECTION_FOCUS_REGION,
                                 nsISelectionController::SCROLL_SYNCHRONOUS);
}

NS_IMETHODIMP
PresShell::SelectAll()
{
  return mSelection->SelectAll();
}

static void
DoCheckVisibility(nsPresContext* aPresContext,
                  nsIContent* aNode,
                  int16_t aStartOffset,
                  int16_t aEndOffset,
                  bool* aRetval)
{
  nsIFrame* frame = aNode->GetPrimaryFrame();
  if (!frame) {
    // No frame to look at so it must not be visible.
    return;
  }

  // Start process now to go through all frames to find startOffset. Then check
  // chars after that to see if anything until EndOffset is visible.
  bool finished = false;
  frame->CheckVisibility(aPresContext, aStartOffset, aEndOffset, true,
                         &finished, aRetval);
  // Don't worry about other return value.
}

NS_IMETHODIMP
PresShell::CheckVisibility(nsIDOMNode *node, int16_t startOffset, int16_t EndOffset, bool *_retval)
{
  if (!node || startOffset>EndOffset || !_retval || startOffset<0 || EndOffset<0)
    return NS_ERROR_INVALID_ARG;
  *_retval = false; //initialize return parameter
  nsCOMPtr<nsIContent> content(do_QueryInterface(node));
  if (!content)
    return NS_ERROR_FAILURE;

  DoCheckVisibility(mPresContext, content, startOffset, EndOffset, _retval);
  return NS_OK;
}

nsresult
PresShell::CheckVisibilityContent(nsIContent* aNode, int16_t aStartOffset,
                                  int16_t aEndOffset, bool* aRetval)
{
  if (!aNode || aStartOffset > aEndOffset || !aRetval ||
      aStartOffset < 0 || aEndOffset < 0) {
    return NS_ERROR_INVALID_ARG;
  }

  *aRetval = false;
  DoCheckVisibility(mPresContext, aNode, aStartOffset, aEndOffset, aRetval);
  return NS_OK;
}

//end implementations nsISelectionController

nsIFrame*
nsIPresShell::GetRootFrameExternal() const
{
  return mFrameConstructor->GetRootFrame();
}

nsIFrame*
nsIPresShell::GetRootScrollFrame() const
{
  nsIFrame* rootFrame = mFrameConstructor->GetRootFrame();
  // Ensure root frame is a viewport frame
  if (!rootFrame || nsGkAtoms::viewportFrame != rootFrame->GetType())
    return nullptr;
  nsIFrame* theFrame = rootFrame->GetFirstPrincipalChild();
  if (!theFrame || nsGkAtoms::scrollFrame != theFrame->GetType())
    return nullptr;
  return theFrame;
}

nsIScrollableFrame*
nsIPresShell::GetRootScrollFrameAsScrollable() const
{
  nsIFrame* frame = GetRootScrollFrame();
  if (!frame)
    return nullptr;
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

nsCanvasFrame*
PresShell::GetCanvasFrame() const
{
  nsIFrame* frame = mFrameConstructor->GetDocElementContainingBlock();
  return do_QueryFrame(frame);
}

Element*
PresShell::GetTouchCaretElement() const
{
  return GetCanvasFrame() ? GetCanvasFrame()->GetTouchCaretElement() : nullptr;
}

void
PresShell::SetMayHaveTouchCaret(bool aSet)
{
  if (!mPresContext) {
    return;
  }

  if (!mPresContext->IsRoot()) {
    nsIPresShell* rootPresShell = GetRootPresShell();
    if (rootPresShell) {
      rootPresShell->SetMayHaveTouchCaret(aSet);
    }
    return;
  }

  nsIDocument* document = GetDocument();
  if (document) {
    nsPIDOMWindow* innerWin = document->GetInnerWindow();
    if (innerWin) {
      innerWin->SetMayHaveTouchCaret(aSet);
    }
  }
}

bool
PresShell::MayHaveTouchCaret()
{
  if (!mPresContext) {
    return false;
  }

  if (!mPresContext->IsRoot()) {
    nsIPresShell* rootPresShell = GetRootPresShell();
    return rootPresShell ? rootPresShell->MayHaveTouchCaret() : false;
  }

  nsIDocument* document = GetDocument();
  if (document) {
    nsPIDOMWindow* innerWin = document->GetInnerWindow();
    if (innerWin) {
      return innerWin->MayHaveTouchCaret();
    }
  }
  return false;
}

Element*
PresShell::GetSelectionCaretsStartElement() const
{
  return GetCanvasFrame() ? GetCanvasFrame()->GetSelectionCaretsStartElement() : nullptr;
}

Element*
PresShell::GetSelectionCaretsEndElement() const
{
  return GetCanvasFrame() ? GetCanvasFrame()->GetSelectionCaretsEndElement() : nullptr;
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
    if (mStylesHaveChanged || !mChangedScopeStyleRoots.IsEmpty())
      ReconstructStyleData();
  }

  mFrameConstructor->EndUpdate();
}

void
PresShell::RestoreRootScrollPosition()
{
  nsIScrollableFrame* scrollableFrame = GetRootScrollFrameAsScrollable();
  if (scrollableFrame) {
    scrollableFrame->ScrollToRestoredPosition();
  }
}

void
PresShell::BeginLoad(nsIDocument *aDocument)
{
  mDocumentLoading = true;

#ifdef PR_LOGGING
  gfxTextPerfMetrics *tp = nullptr;
  if (mPresContext) {
    tp = mPresContext->GetTextPerfMetrics();
  }

  bool shouldLog = gLog && PR_LOG_TEST(gLog, PR_LOG_DEBUG);
  if (shouldLog || tp) {
    mLoadBegin = TimeStamp::Now();
  }

  if (shouldLog) {
    nsIURI* uri = mDocument->GetDocumentURI();
    nsAutoCString spec;
    if (uri) {
      uri->GetSpec(spec);
    }
    PR_LOG(gLog, PR_LOG_DEBUG,
           ("(presshell) %p load begin [%s]\n",
            this, spec.get()));
  }
#endif
}

void
PresShell::EndLoad(nsIDocument *aDocument)
{
  NS_PRECONDITION(aDocument == mDocument, "Wrong document");

  RestoreRootScrollPosition();

  mDocumentLoading = false;
}

void
PresShell::LoadComplete()
{
#ifdef PR_LOGGING
  gfxTextPerfMetrics *tp = nullptr;
  if (mPresContext) {
    tp = mPresContext->GetTextPerfMetrics();
  }

  // log load
  bool shouldLog = gLog && PR_LOG_TEST(gLog, PR_LOG_DEBUG);
  if (shouldLog || tp) {
    TimeDuration loadTime = TimeStamp::Now() - mLoadBegin;
    nsIURI* uri = mDocument->GetDocumentURI();
    nsAutoCString spec;
    if (uri) {
      uri->GetSpec(spec);
    }
    if (shouldLog) {
      PR_LOG(gLog, PR_LOG_DEBUG,
             ("(presshell) %p load done time-ms: %9.2f [%s]\n",
              this, loadTime.ToMilliseconds(), spec.get()));
    }
    if (tp) {
      tp->Accumulate();
      if (tp->cumulative.numChars > 0) {
        LogTextPerfStats(tp, this, tp->cumulative, loadTime.ToMilliseconds(),
                         eLog_loaddone, spec.get());
      }
    }
  }
#endif
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
  NS_PRECONDITION(aBitToAdd == NS_FRAME_IS_DIRTY ||
                  aBitToAdd == NS_FRAME_HAS_DIRTY_CHILDREN,
                  "Unexpected bits being added");
  NS_PRECONDITION(aIntrinsicDirty != eStyleChange ||
                  aBitToAdd == NS_FRAME_IS_DIRTY,
                  "bits don't correspond to style change reason");

  NS_ASSERTION(!mIsReflowing, "can't mark frame dirty during reflow");

  // If we've not yet done the initial reflow, then don't bother
  // enqueuing a reflow command yet.
  if (! mDidInitialize)
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
    bool wasDirty = NS_SUBTREE_DIRTY(subtreeRoot);
    subtreeRoot->AddStateBits(aBitToAdd);

    // Now if subtreeRoot is a reflow root we can cut off this reflow at it if
    // the bit being added is NS_FRAME_HAS_DIRTY_CHILDREN.
    bool targetFrameDirty = (aBitToAdd == NS_FRAME_IS_DIRTY);

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
      // Note that nsHTMLReflowState::InitResizeFlags has some similar
      // code; see comments there for how and why it differs.
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

        nsIFrame::ChildListIterator lists(f);
        for (; !lists.IsDone(); lists.Next()) {
          nsFrameList::Enumerator childFrames(lists.CurrentList());
          for (; !childFrames.AtEnd(); childFrames.Next()) {
            nsIFrame* kid = childFrames.get();
            kid->MarkIntrinsicWidthsDirty();
            stack.AppendElement(kid);
          }
        }
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
          mDocument->SetNeedLayoutFlush();
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
  nsIScrollableFrame* scrollFrame = nullptr;

  nsCOMPtr<nsIContent> focusedContent;
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm && mDocument) {
    nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(mDocument->GetWindow());

    nsCOMPtr<nsIDOMElement> focusedElement;
    fm->GetFocusedElementForWindow(window, false, nullptr, getter_AddRefs(focusedElement));
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
    mReflowScheduled = false;
  }

  ASSERT_REFLOW_SCHEDULED_STATE();
}

nsresult
PresShell::RecreateFramesFor(nsIContent* aContent)
{
  NS_ENSURE_TRUE(mPresContext, NS_ERROR_FAILURE);
  if (!mDidInitialize) {
    // Nothing to do here.  In fact, if we proceed and aContent is the
    // root we will crash.
    return NS_OK;
  }

  // Don't call RecreateFramesForContent since that is not exported and we want
  // to keep the number of entrypoints down.

  NS_ASSERTION(mViewManager, "Should have view manager");

  // Have to make sure that the content notifications are flushed before we
  // start messing with the frame model; otherwise we can get content doubling.
  mDocument->FlushPendingNotifications(Flush_ContentAndNotify);

  nsAutoScriptBlocker scriptBlocker;

  nsStyleChangeList changeList;
  changeList.AppendChange(nullptr, aContent, nsChangeHint_ReconstructFrame);

  // Mark ourselves as not safe to flush while we're doing frame construction.
  ++mChangeNestCount;
  RestyleManager* restyleManager = mPresContext->RestyleManager();
  nsresult rv = restyleManager->ProcessRestyledFrames(changeList);
  restyleManager->FlushOverflowChangedTracker();
  --mChangeNestCount;

  return rv;
}

void
nsIPresShell::PostRecreateFramesFor(Element* aElement)
{
  mPresContext->RestyleManager()->PostRestyleEvent(aElement, nsRestyleHint(0),
                                                   nsChangeHint_ReconstructFrame);
}

void
nsIPresShell::RestyleForAnimation(Element* aElement, nsRestyleHint aHint)
{
  mPresContext->RestyleManager()->PostAnimationRestyleEvent(aElement, aHint,
                                                            NS_STYLE_HINT_NONE);
}

void
nsIPresShell::SetForwardingContainer(const WeakPtr<nsDocShell> &aContainer)
{
  mForwardingContainer = aContainer;
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

already_AddRefed<nsRenderingContext>
PresShell::CreateReferenceRenderingContext()
{
  nsDeviceContext* devCtx = mPresContext->DeviceContext();
  nsRefPtr<nsRenderingContext> rc;
  if (mPresContext->IsScreen()) {
    rc = new nsRenderingContext();
    rc->Init(devCtx, gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget());
  } else {
    rc = devCtx->CreateRenderingContext();
  }

  MOZ_ASSERT(rc, "shouldn't break promise to return non-null");
  return rc.forget();
}

nsresult
PresShell::GoToAnchor(const nsAString& aAnchorName, bool aScroll)
{
  if (!mDocument) {
    return NS_ERROR_FAILURE;
  }

  const Element *root = mDocument->GetRootElement();
  if (root && root->IsSVG(nsGkAtoms::svg)) {
    // We need to execute this even if there is an empty anchor name
    // so that any existing SVG fragment identifier effect is removed
    if (SVGFragmentIdentifier::ProcessFragmentIdentifier(mDocument, aAnchorName)) {
      return NS_OK;
    }
  }

  // Hold a reference to the ESM in case event dispatch tears us down.
  nsRefPtr<EventStateManager> esm = mPresContext->EventStateManager();

  if (aAnchorName.IsEmpty()) {
    NS_ASSERTION(!aScroll, "can't scroll to empty anchor name");
    esm->SetContentState(nullptr, NS_EVENT_STATE_URLTARGET);
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
      uint32_t i;
      // Loop through the named nodes looking for the first anchor
      for (i = 0; true; i++) {
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
          content = nullptr;
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
      uint32_t i;
      // Loop through the named nodes looking for the first anchor
      for (i = 0; true; i++) {
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

  esm->SetContentState(content, NS_EVENT_STATE_URLTARGET);

#ifdef ACCESSIBILITY
  nsIContent *anchorTarget = content;
#endif

  nsIScrollableFrame* rootScroll = GetRootScrollFrameAsScrollable();
  if (rootScroll && rootScroll->DidHistoryRestore()) {
    // Scroll position restored from history trumps scrolling to anchor.
    aScroll = false;
    rootScroll->ClearDidHistoryRestore();
  }

  if (content) {
    if (aScroll) {
      rv = ScrollContentIntoView(content,
                                 ScrollAxis(SCROLL_TOP, SCROLL_ALWAYS),
                                 ScrollAxis(),
                                 ANCHOR_SCROLL_FLAGS);
      NS_ENSURE_SUCCESS(rv, rv);

      nsIScrollableFrame* rootScroll = GetRootScrollFrameAsScrollable();
      if (rootScroll) {
        mLastAnchorScrolledTo = content;
        mLastAnchorScrollPositionY = rootScroll->GetScrollPosition().y;
      }
    }

    // Should we select the target? This action is controlled by a
    // preference: the default is to not select.
    bool selectAnchor = Preferences::GetBool("layout.selectanchor");

    // Even if select anchor pref is false, we must still move the
    // caret there. That way tabbing will start from the new
    // location
    nsRefPtr<nsIDOMRange> jumpToRange = new nsRange(mDocument);
    while (content && content->GetFirstChild()) {
      content = content->GetFirstChild();
    }
    nsCOMPtr<nsIDOMNode> node(do_QueryInterface(content));
    NS_ASSERTION(node, "No nsIDOMNode for descendant of anchor");
    jumpToRange->SelectNodeContents(node);
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
    // Selection is at anchor.
    // Now focus the document itself if focus is on an element within it.
    nsPIDOMWindow *win = mDocument->GetWindow();

    nsIFocusManager* fm = nsFocusManager::GetFocusManager();
    if (fm && win) {
      nsCOMPtr<nsIDOMWindow> focusedWindow;
      fm->GetFocusedWindow(getter_AddRefs(focusedWindow));
      if (SameCOMIdentity(win, focusedWindow)) {
        fm->ClearFocus(focusedWindow);
      }
    }

    // If the target is an animation element, activate the animation
    if (content->IsNodeOfType(nsINode::eANIMATION)) {
      SVGContentUtils::ActivateByHyperlink(content.get());
    }
  } else {
    rv = NS_ERROR_FAILURE;
    NS_NAMED_LITERAL_STRING(top, "top");
    if (nsContentUtils::EqualsIgnoreASCIICase(aAnchorName, top)) {
      // Scroll to the top/left if aAnchorName is "top" and there is no element
      // with such a name or id.
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
  if (anchorTarget) {
    nsAccessibilityService* accService = AccService();
    if (accService)
      accService->NotifyOfAnchorJumpTo(anchorTarget);
  }
#endif

  return rv;
}

nsresult
PresShell::ScrollToAnchor()
{
  if (!mLastAnchorScrolledTo) {
    return NS_OK;
  }
  NS_ASSERTION(mDidInitialize, "should have done initial reflow by now");

  nsIScrollableFrame* rootScroll = GetRootScrollFrameAsScrollable();
  if (!rootScroll ||
      mLastAnchorScrollPositionY != rootScroll->GetScrollPosition().y) {
    return NS_OK;
  }
  nsresult rv = ScrollContentIntoView(mLastAnchorScrolledTo,
                                      ScrollAxis(SCROLL_TOP, SCROLL_ALWAYS),
                                      ScrollAxis(),
                                      ANCHOR_SCROLL_FLAGS);
  mLastAnchorScrolledTo = nullptr;
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
 * @param aPrevBlock [inout] the block aLines is a line iterator for
 * @param aLines [inout] the line iterator we're using
 * @param aCurLine [inout] the line to start looking from in this iterator
 */
static void
AccumulateFrameBounds(nsIFrame* aContainerFrame,
                      nsIFrame* aFrame,
                      bool aUseWholeLineHeightForInlines,
                      nsRect& aRect,
                      bool& aHaveRect,
                      nsIFrame*& aPrevBlock,
                      nsAutoLineIterator& aLines,
                      int32_t& aCurLine)
{
  nsIFrame* frame = aFrame;
  nsRect frameBounds = nsRect(nsPoint(0, 0), aFrame->GetSize());

  // If this is an inline frame and either the bounds height is 0 (quirks
  // layout model) or aUseWholeLineHeightForInlines is set, we need to
  // change the top of the bounds to include the whole line.
  if (frameBounds.height == 0 || aUseWholeLineHeightForInlines) {
    nsIFrame *prevFrame = aFrame;
    nsIFrame *f = aFrame;

    while (f && f->IsFrameOfType(nsIFrame::eLineParticipant) &&
           !f->IsTransformed() && !f->IsPositioned()) {
      prevFrame = f;
      f = prevFrame->GetParent();
    }

    if (f != aFrame &&
        f &&
        f->GetType() == nsGkAtoms::blockFrame) {
      // find the line containing aFrame and increase the top of |offset|.
      if (f != aPrevBlock) {
        aLines = f->GetLineIterator();
        aPrevBlock = f;
        aCurLine = 0;
      }
      if (aLines) {
        int32_t index = aLines->FindLineContaining(prevFrame, aCurLine);
        if (index >= 0) {
          aCurLine = index;
          nsIFrame *trash1;
          int32_t trash2;
          nsRect lineBounds;
          uint32_t trash3;

          if (NS_SUCCEEDED(aLines->GetLine(index, &trash1, &trash2,
                                           lineBounds, &trash3))) {
            frameBounds += frame->GetOffsetTo(f);
            frame = f;
            if (lineBounds.y < frameBounds.y) {
              frameBounds.height = frameBounds.YMost() - lineBounds.y;
              frameBounds.y = lineBounds.y;
            }
          }
        }
      }
    }
  }

  nsRect transformedBounds = nsLayoutUtils::TransformFrameRectToAncestor(frame,
    frameBounds, aContainerFrame);

  if (aHaveRect) {
    // We can't use nsRect::UnionRect since it drops empty rects on
    // the floor, and we need to include them.  (Thus we need
    // aHaveRect to know when to drop the initial value on the floor.)
    aRect.UnionRectEdges(aRect, transformedBounds);
  } else {
    aHaveRect = true;
    aRect = transformedBounds;
  }
}

static bool
ComputeNeedToScroll(nsIPresShell::WhenToScroll aWhenToScroll,
                    nscoord                    aLineSize,
                    nscoord                    aRectMin,
                    nscoord                    aRectMax,
                    nscoord                    aViewMin,
                    nscoord                    aViewMax) {
  // See how the rect should be positioned vertically
  if (nsIPresShell::SCROLL_ALWAYS == aWhenToScroll) {
    // The caller wants the frame as visible as possible
    return true;
  } else if (nsIPresShell::SCROLL_IF_NOT_VISIBLE == aWhenToScroll) {
    // Scroll only if no part of the frame is visible in this view
    return aRectMax - aLineSize <= aViewMin ||
           aRectMin + aLineSize >= aViewMax;
  } else if (nsIPresShell::SCROLL_IF_NOT_FULLY_VISIBLE == aWhenToScroll) {
    // Scroll only if part of the frame is hidden and more can fit in view
    return !(aRectMin >= aViewMin && aRectMax <= aViewMax) &&
      std::min(aViewMax, aRectMax) - std::max(aRectMin, aViewMin) < aViewMax - aViewMin;
  }
  return false;
}

static nscoord
ComputeWhereToScroll(int16_t aWhereToScroll,
                     nscoord aOriginalCoord,
                     nscoord aRectMin,
                     nscoord aRectMax,
                     nscoord aViewMin,
                     nscoord aViewMax,
                     nscoord* aRangeMin,
                     nscoord* aRangeMax) {
  nscoord resultCoord = aOriginalCoord;
  // Allow the scroll operation to land anywhere that
  // makes the whole rectangle visible.
  if (nsIPresShell::SCROLL_MINIMUM == aWhereToScroll) {
    if (aRectMin < aViewMin) {
      // Scroll up so the frame's top edge is visible
      resultCoord = aRectMin;
    } else if (aRectMax > aViewMax) {
      // Scroll down so the frame's bottom edge is visible. Make sure the
      // frame's top edge is still visible
      resultCoord = aOriginalCoord + aRectMax - aViewMax;
      if (resultCoord > aRectMin) {
        resultCoord = aRectMin;
      }
    }
  } else {
    nscoord frameAlignCoord =
      NSToCoordRound(aRectMin + (aRectMax - aRectMin) * (aWhereToScroll / 100.0f));
    resultCoord =  NSToCoordRound(frameAlignCoord - (aViewMax - aViewMin) * (
                                  aWhereToScroll / 100.0f));
  }
  nscoord scrollPortLength = aViewMax - aViewMin;
  // Force the scroll range to extend to include resultCoord.
  *aRangeMin = std::min(resultCoord, aRectMax - scrollPortLength);
  *aRangeMax = std::max(resultCoord, aRectMin);
  return resultCoord;
}

/**
 * This function takes a scrollable frame, a rect in the coordinate system
 * of the scrolled frame, and a desired percentage-based scroll
 * position and attempts to scroll the rect to that position in the
 * scrollport.
 *
 * This needs to work even if aRect has a width or height of zero.
 */
static void ScrollToShowRect(nsIFrame*                aFrame,
                             nsIScrollableFrame*      aFrameAsScrollable,
                             const nsRect&            aRect,
                             nsIPresShell::ScrollAxis aVertical,
                             nsIPresShell::ScrollAxis aHorizontal,
                             uint32_t                 aFlags)
{
  nsPoint scrollPt = aFrameAsScrollable->GetScrollPosition();
  nsRect visibleRect(scrollPt,
                     aFrameAsScrollable->GetScrollPositionClampingScrollPortSize());

  // If this is the root scroll frame, make sure to take into account the
  // content document fixed position margins. When set, these indicate that
  // chrome is obscuring the viewport.
  nsRect targetRect(aRect);
  nsIPresShell *presShell = aFrame->PresContext()->PresShell();
  if (aFrameAsScrollable == presShell->GetRootScrollFrameAsScrollable()) {
    targetRect.Inflate(presShell->GetContentDocumentFixedPositionMargins());
  }

  nsSize lineSize;
  // Don't call GetLineScrollAmount unless we actually need it. Not only
  // does this save time, but it's not safe to call GetLineScrollAmount
  // during reflow (because it depends on font size inflation and doesn't
  // use the in-reflow-safe font-size inflation path). If we did call it,
  // it would assert and possible give the wrong result.
  if (aVertical.mWhenToScroll == nsIPresShell::SCROLL_IF_NOT_VISIBLE ||
      aHorizontal.mWhenToScroll == nsIPresShell::SCROLL_IF_NOT_VISIBLE) {
    lineSize = aFrameAsScrollable->GetLineScrollAmount();
  }
  ScrollbarStyles ss = aFrameAsScrollable->GetScrollbarStyles();
  nsRect allowedRange(scrollPt, nsSize(0, 0));
  bool needToScroll = false;
  uint32_t directions = aFrameAsScrollable->GetPerceivedScrollingDirections();

  if (((aFlags & nsIPresShell::SCROLL_OVERFLOW_HIDDEN) ||
       ss.mVertical != NS_STYLE_OVERFLOW_HIDDEN) &&
      (!aVertical.mOnlyIfPerceivedScrollableDirection ||
       (directions & nsIScrollableFrame::VERTICAL))) {

    if (ComputeNeedToScroll(aVertical.mWhenToScroll,
                            lineSize.height,
                            targetRect.y,
                            targetRect.YMost(),
                            visibleRect.y,
                            visibleRect.YMost())) {
      nscoord maxHeight;
      scrollPt.y = ComputeWhereToScroll(aVertical.mWhereToScroll,
                                        scrollPt.y,
                                        targetRect.y,
                                        targetRect.YMost(),
                                        visibleRect.y,
                                        visibleRect.YMost(),
                                        &allowedRange.y, &maxHeight);
      allowedRange.height = maxHeight - allowedRange.y;
      needToScroll = true;
    }
  }

  if (((aFlags & nsIPresShell::SCROLL_OVERFLOW_HIDDEN) ||
       ss.mHorizontal != NS_STYLE_OVERFLOW_HIDDEN) &&
      (!aHorizontal.mOnlyIfPerceivedScrollableDirection ||
       (directions & nsIScrollableFrame::HORIZONTAL))) {

    if (ComputeNeedToScroll(aHorizontal.mWhenToScroll,
                            lineSize.width,
                            targetRect.x,
                            targetRect.XMost(),
                            visibleRect.x,
                            visibleRect.XMost())) {
      nscoord maxWidth;
      scrollPt.x = ComputeWhereToScroll(aHorizontal.mWhereToScroll,
                                        scrollPt.x,
                                        targetRect.x,
                                        targetRect.XMost(),
                                        visibleRect.x,
                                        visibleRect.XMost(),
                                        &allowedRange.x, &maxWidth);
      allowedRange.width = maxWidth - allowedRange.x;
      needToScroll = true;
    }
  }

  // If we don't need to scroll, then don't try since it might cancel
  // a current smooth scroll operation.
  if (needToScroll) {
    aFrameAsScrollable->ScrollTo(scrollPt, nsIScrollableFrame::INSTANT, &allowedRange);
  }
}

nsresult
PresShell::ScrollContentIntoView(nsIContent*              aContent,
                                 nsIPresShell::ScrollAxis aVertical,
                                 nsIPresShell::ScrollAxis aHorizontal,
                                 uint32_t                 aFlags)
{
  NS_ENSURE_TRUE(aContent, NS_ERROR_NULL_POINTER);
  nsCOMPtr<nsIDocument> currentDoc = aContent->GetCurrentDoc();
  NS_ENSURE_STATE(currentDoc);

  NS_ASSERTION(mDidInitialize, "should have done initial reflow by now");

  if (mContentToScrollTo) {
    mContentToScrollTo->DeleteProperty(nsGkAtoms::scrolling);
  }
  mContentToScrollTo = aContent;
  ScrollIntoViewData* data = new ScrollIntoViewData();
  data->mContentScrollVAxis = aVertical;
  data->mContentScrollHAxis = aHorizontal;
  data->mContentToScrollToFlags = aFlags;
  if (NS_FAILED(mContentToScrollTo->SetProperty(nsGkAtoms::scrolling, data,
                                                nsINode::DeleteProperty<PresShell::ScrollIntoViewData>))) {
    mContentToScrollTo = nullptr;
  }

  // Flush layout and attempt to scroll in the process.
  currentDoc->SetNeedLayoutFlush();
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
    DoScrollContentIntoView();
  }
  return NS_OK;
}

void
PresShell::DoScrollContentIntoView()
{
  NS_ASSERTION(mDidInitialize, "should have done initial reflow by now");

  nsIFrame* frame = mContentToScrollTo->GetPrimaryFrame();
  if (!frame) {
    mContentToScrollTo->DeleteProperty(nsGkAtoms::scrolling);
    mContentToScrollTo = nullptr;
    return;
  }

  if (frame->GetStateBits() & NS_FRAME_FIRST_REFLOW) {
    // The reflow flush before this scroll got interrupted, and this frame's
    // coords and size are all zero, and it has no content showing anyway.
    // Don't bother scrolling to it.  We'll try again when we finish up layout.
    return;
  }

  // Make sure we skip 'frame' ... if it's scrollable, we should use its
  // scrollable ancestor as the container.
  nsIFrame* container =
    nsLayoutUtils::GetClosestFrameOfType(frame->GetParent(), nsGkAtoms::scrollFrame);
  if (!container) {
    // nothing can be scrolled
    return;
  }

  ScrollIntoViewData* data = static_cast<ScrollIntoViewData*>(
    mContentToScrollTo->GetProperty(nsGkAtoms::scrolling));
  if (MOZ_UNLIKELY(!data)) {
    mContentToScrollTo = nullptr;
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
  bool haveRect = false;
  bool useWholeLineHeightForInlines =
    data->mContentScrollVAxis.mWhenToScroll != nsIPresShell::SCROLL_IF_NOT_FULLY_VISIBLE;
  // Reuse the same line iterator across calls to AccumulateFrameBounds.  We set
  // it every time we detect a new block (stored in prevBlock).
  nsIFrame* prevBlock = nullptr;
  nsAutoLineIterator lines;
  // The last line we found a continuation on in |lines|.  We assume that later
  // continuations cannot come on earlier lines.
  int32_t curLine = 0;
  do {
    AccumulateFrameBounds(container, frame, useWholeLineHeightForInlines,
                          frameBounds, haveRect, prevBlock, lines, curLine);
  } while ((frame = frame->GetNextContinuation()));

  ScrollFrameRectIntoView(container, frameBounds, data->mContentScrollVAxis,
                          data->mContentScrollHAxis,
                          data->mContentToScrollToFlags);
}

bool
PresShell::ScrollFrameRectIntoView(nsIFrame*                aFrame,
                                   const nsRect&            aRect,
                                   nsIPresShell::ScrollAxis aVertical,
                                   nsIPresShell::ScrollAxis aHorizontal,
                                   uint32_t                 aFlags)
{
  bool didScroll = false;
  // This function needs to work even if rect has a width or height of 0.
  nsRect rect = aRect;
  nsIFrame* container = aFrame;
  // Walk up the frame hierarchy scrolling the rect into view and
  // keeping rect relative to container
  do {
    nsIScrollableFrame* sf = do_QueryFrame(container);
    if (sf) {
      nsPoint oldPosition = sf->GetScrollPosition();
      nsRect targetRect = rect;
      if (container->StyleDisplay()->mOverflowClipBox ==
            NS_STYLE_OVERFLOW_CLIP_BOX_CONTENT_BOX) {
        nsMargin padding = container->GetUsedPadding();
        targetRect.Inflate(padding);
      }
      ScrollToShowRect(container, sf, targetRect - sf->GetScrolledFrame()->GetPosition(),
                       aVertical, aHorizontal, aFlags);
      nsPoint newPosition = sf->GetScrollPosition();
      // If the scroll position increased, that means our content moved up,
      // so our rect's offset should decrease
      rect += oldPosition - newPosition;

      if (oldPosition != newPosition) {
        didScroll = true;
      }

      // only scroll one container when this flag is set
      if (aFlags & nsIPresShell::SCROLL_FIRST_ANCESTOR_ONLY) {
        break;
      }
    }
    nsIFrame* parent;
    if (container->IsTransformed()) {
      container->GetTransformMatrix(nullptr, &parent);
      rect = nsLayoutUtils::TransformFrameRectToAncestor(container, rect, parent);
    } else {
      rect += container->GetPosition();
      parent = container->GetParent();
    }
    if (!parent && !(aFlags & nsIPresShell::SCROLL_NO_PARENT_FRAMES)) {
      nsPoint extraOffset(0,0);
      parent = nsLayoutUtils::GetCrossDocParentFrame(container, &extraOffset);
      if (parent) {
        int32_t APD = container->PresContext()->AppUnitsPerDevPixel();
        int32_t parentAPD = parent->PresContext()->AppUnitsPerDevPixel();
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
  nsIFrame* rootFrame = mFrameConstructor->GetRootFrame();
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

class PaintTimerCallBack MOZ_FINAL : public nsITimerCallback
{
public:
  PaintTimerCallBack(PresShell* aShell) : mShell(aShell) {}

  NS_DECL_ISUPPORTS

  NS_IMETHODIMP Notify(nsITimer* aTimer) MOZ_FINAL
  {
    mShell->SetNextPaintCompressed();
    mShell->AddInvalidateHiddenPresShellObserver(mShell->GetPresContext()->RefreshDriver());
    mShell->ScheduleViewManagerFlush();
    return NS_OK;
  }

private:
  ~PaintTimerCallBack() {}

  PresShell* mShell;
};

NS_IMPL_ISUPPORTS(PaintTimerCallBack, nsITimerCallback)

void
PresShell::ScheduleViewManagerFlush(PaintType aType)
{
  if (aType == PAINT_DELAYED_COMPRESS) {
    // Delay paint for 1 second.
    static const uint32_t kPaintDelayPeriod = 1000;
    if (!mDelayedPaintTimer) {
      mDelayedPaintTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
      nsRefPtr<PaintTimerCallBack> cb = new PaintTimerCallBack(this);
      mDelayedPaintTimer->InitWithCallback(cb, kPaintDelayPeriod, nsITimer::TYPE_ONE_SHOT);
    }
    return;
  }

  nsPresContext* presContext = GetPresContext();
  if (presContext) {
    presContext->RefreshDriver()->ScheduleViewManagerFlush();
  }
  if (mDocument) {
    mDocument->SetNeedLayoutFlush();
  }
}

void
PresShell::DispatchSynthMouseMove(WidgetGUIEvent* aEvent,
                                  bool aFlushOnHoverChange)
{
  RestyleManager* restyleManager = mPresContext->RestyleManager();
  uint32_t hoverGenerationBefore = restyleManager->GetHoverGeneration();
  nsEventStatus status;
  nsView* targetView = nsView::GetViewFor(aEvent->widget);
  if (!targetView)
    return;
  targetView->GetViewManager()->DispatchEvent(aEvent, targetView, &status);
  if (MOZ_UNLIKELY(mIsDestroying)) {
    return;
  }
  if (aFlushOnHoverChange &&
      hoverGenerationBefore != restyleManager->GetHoverGeneration()) {
    // Flush so that the resulting reflow happens now so that our caller
    // can suppress any synthesized mouse moves caused by that reflow.
    FlushPendingNotifications(Flush_Layout);
  }
}

void
PresShell::ClearMouseCaptureOnView(nsView* aView)
{
  if (gCaptureInfo.mContent) {
    if (aView) {
      // if a view was specified, ensure that the captured content is within
      // this view.
      nsIFrame* frame = gCaptureInfo.mContent->GetPrimaryFrame();
      if (frame) {
        nsView* view = frame->GetClosestView();
        // if there is no view, capturing won't be handled any more, so
        // just release the capture.
        if (view) {
          do {
            if (view == aView) {
              NS_RELEASE(gCaptureInfo.mContent);
              // the view containing the captured content likely disappeared so
              // disable capture for now.
              gCaptureInfo.mAllowed = false;
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
  gCaptureInfo.mAllowed = false;
}

void
nsIPresShell::ClearMouseCapture(nsIFrame* aFrame)
{
  if (!gCaptureInfo.mContent) {
    gCaptureInfo.mAllowed = false;
    return;
  }

  // null frame argument means clear the capture
  if (!aFrame) {
    NS_RELEASE(gCaptureInfo.mContent);
    gCaptureInfo.mAllowed = false;
    return;
  }

  nsIFrame* capturingFrame = gCaptureInfo.mContent->GetPrimaryFrame();
  if (!capturingFrame) {
    NS_RELEASE(gCaptureInfo.mContent);
    gCaptureInfo.mAllowed = false;
    return;
  }

  if (nsLayoutUtils::IsAncestorFrameCrossDoc(aFrame, capturingFrame)) {
    NS_RELEASE(gCaptureInfo.mContent);
    gCaptureInfo.mAllowed = false;
  }
}

nsresult
PresShell::CaptureHistoryState(nsILayoutHistoryState** aState)
{
  NS_PRECONDITION(nullptr != aState, "null state pointer");

  // We actually have to mess with the docshell here, since we want to
  // store the state back in it.
  // XXXbz this isn't really right, since this is being called in the
  // content viewer's Hide() method...  by that point the docshell's
  // state could be wrong.  We should sort out a better ownership
  // model for the layout history state.
  nsCOMPtr<nsIDocShell> docShell(mPresContext->GetDocShell());
  if (!docShell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsILayoutHistoryState> historyState;
  docShell->GetLayoutHistoryState(getter_AddRefs(historyState));
  if (!historyState) {
    // Create the document state object
    historyState = NS_NewLayoutHistoryState();
    docShell->SetLayoutHistoryState(historyState);
  }

  *aState = historyState;
  NS_IF_ADDREF(*aState);

  // Capture frame state for the entire frame hierarchy
  nsIFrame* rootFrame = mFrameConstructor->GetRootFrame();
  if (!rootFrame) return NS_OK;

  mFrameConstructor->CaptureFrameState(rootFrame, historyState);

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

  if (!mDocument->IsResourceDoc()) {
    // Notify observers that a new page is about to be drawn. Execute this
    // as soon as it is safe to run JS, which is guaranteed to be before we
    // go back to the event loop and actually draw the page.
    nsContentUtils::AddScriptRunner(new nsBeforeFirstPaintDispatcher(mDocument));
  }

  mPaintingSuppressed = false;
  nsIFrame* rootFrame = mFrameConstructor->GetRootFrame();
  if (rootFrame) {
    // let's assume that outline on a root frame is not supported
    rootFrame->InvalidateFrame();

    if (mCaretEnabled && mCaret) {
      mCaret->CheckCaretDrawingState();
    }
  }

  // now that painting is unsuppressed, focus may be set on the document
  nsPIDOMWindow *win = mDocument->GetWindow();
  if (win)
    win->SetReadyForFocus();

  if (!mHaveShutDown) {
    SynthesizeMouseMove(false);
    ScheduleImageVisibilityUpdate();
  }
}

void
PresShell::UnsuppressPainting()
{
  if (mPaintSuppressionTimer) {
    mPaintSuppressionTimer->Cancel();
    mPaintSuppressionTimer = nullptr;
  }

  if (mIsDocumentGone || !mPaintingSuppressed)
    return;

  // If we have reflows pending, just wait until we process
  // the reflows and get all the frames where we want them
  // before actually unlocking the painting.  Otherwise
  // go ahead and unlock now.
  if (!mDirtyRoots.IsEmpty())
    mShouldUnsuppressPainting = true;
  else
    UnsuppressAndInvalidate();
}

// Post a request to handle an arbitrary callback after reflow has finished.
nsresult
PresShell::PostReflowCallback(nsIReflowCallback* aCallback)
{
  void* result = AllocateMisc(sizeof(nsCallbackEventRequest));
  nsCallbackEventRequest* request = (nsCallbackEventRequest*)result;

  request->callback = aCallback;
  request->next = nullptr;

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
   nsCallbackEventRequest* before = nullptr;
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
          NS_ASSERTION(before == nullptr, "impossible");
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
      mLastCallbackEventRequest = nullptr;
    }
    nsIReflowCallback* callback = node->callback;
    FreeMisc(sizeof(nsCallbackEventRequest), node);
    if (callback) {
      callback->ReflowCallbackCanceled();
    }
  }
}

void
PresShell::HandlePostedReflowCallbacks(bool aInterruptible)
{
   bool shouldFlush = false;

   while (mFirstCallbackEventRequest) {
     nsCallbackEventRequest* node = mFirstCallbackEventRequest;
     mFirstCallbackEventRequest = node->next;
     if (!mFirstCallbackEventRequest) {
       mLastCallbackEventRequest = nullptr;
     }
     nsIReflowCallback* callback = node->callback;
     FreeMisc(sizeof(nsCallbackEventRequest), node);
     if (callback) {
       if (callback->ReflowFinished()) {
         shouldFlush = true;
       }
     }
   }

   mozFlushType flushType =
     aInterruptible ? Flush_InterruptibleLayout : Flush_Layout;
   if (shouldFlush && !mIsDestroying) {
     FlushPendingNotifications(flushType);
   }
}

bool
PresShell::IsSafeToFlush() const
{
  // Not safe if we are reflowing or in the middle of frame construction
  bool isSafeToFlush = !mIsReflowing &&
                         !mChangeNestCount;

  if (isSafeToFlush) {
    // Not safe if we are painting
    nsViewManager* viewManager = GetViewManager();
    if (viewManager) {
      bool isPainting = false;
      viewManager->IsPainting(isPainting);
      if (isPainting) {
        isSafeToFlush = false;
      }
    }
  }

  return isSafeToFlush;
}


void
PresShell::FlushPendingNotifications(mozFlushType aType)
{
  // by default, flush animations if aType >= Flush_Style
  mozilla::ChangesToFlush flush(aType, aType >= Flush_Style);
  FlushPendingNotifications(flush);
}

void
PresShell::FlushPendingNotifications(mozilla::ChangesToFlush aFlush)
{
  if (mIsZombie) {
    return;
  }

  /**
   * VERY IMPORTANT: If you add some sort of new flushing to this
   * method, make sure to add the relevant SetNeedLayoutFlush or
   * SetNeedStyleFlush calls on the document.
   */
  mozFlushType flushType = aFlush.mFlushType;

#ifdef MOZ_ENABLE_PROFILER_SPS
  static const char flushTypeNames[][20] = {
    "Content",
    "ContentAndNotify",
    "Style",
    "InterruptibleLayout",
    "Layout",
    "Display"
  };

  // Make sure that we don't miss things added to mozFlushType!
  MOZ_ASSERT(static_cast<uint32_t>(flushType) <= ArrayLength(flushTypeNames));

  PROFILER_LABEL_PRINTF("PresShell", "Flush",
    js::ProfileEntry::Category::GRAPHICS, "(Flush_%s)", flushTypeNames[flushType - 1]);
#endif

#ifdef ACCESSIBILITY
#ifdef DEBUG
  nsAccessibilityService* accService = GetAccService();
  if (accService) {
    NS_ASSERTION(!accService->IsProcessingRefreshDriverNotification(),
                 "Flush during accessible tree update!");
  }
#endif
#endif

  NS_ASSERTION(flushType >= Flush_Frames, "Why did we get called?");

  bool isSafeToFlush = IsSafeToFlush();

  // If layout could possibly trigger scripts, then it's only safe to flush if
  // it's safe to run script.
  bool hasHadScriptObject;
  if (mDocument->GetScriptHandlingObject(hasHadScriptObject) ||
      hasHadScriptObject) {
    isSafeToFlush = isSafeToFlush && nsContentUtils::IsSafeToRunScript();
  }

  NS_ASSERTION(!isSafeToFlush || mViewManager, "Must have view manager");
  // Make sure the view manager stays alive.
  nsRefPtr<nsViewManager> viewManagerDeathGrip = mViewManager;
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

    // We need to make sure external resource documents are flushed too (for
    // example, svg filters that reference a filter in an external document
    // need the frames in the external document to be constructed for the
    // filter to work). We only need external resources to be flushed when the
    // main document is flushing >= Flush_Frames, so we flush external
    // resources here instead of nsDocument::FlushPendingNotifications.
    mDocument->FlushExternalResources(flushType);

    // Force flushing of any pending content notifications that might have
    // queued up while our event was pending.  That will ensure that we don't
    // construct frames for content right now that's still waiting to be
    // notified on,
    mDocument->FlushPendingNotifications(Flush_ContentAndNotify);

    // Process pending restyles, since any flush of the presshell wants
    // up-to-date style data.
    if (!mIsDestroying) {
      mViewManager->FlushDelayedResize(false);
      mPresContext->FlushPendingMediaFeatureValuesChanged();

      // Flush any pending update of the user font set, since that could
      // cause style changes (for updating ex/ch units, and to cause a
      // reflow).
      mPresContext->FlushUserFontSet();

      mPresContext->FlushCounterStyles();

      // Flush any requested SMIL samples.
      if (mDocument->HasAnimationController()) {
        mDocument->GetAnimationController()->FlushResampleRequests();
      }

      if (aFlush.mFlushAnimations &&
          !mPresContext->StyleUpdateForAllAnimationsIsUpToDate()) {
        if (mPresContext->AnimationManager()) {
          mPresContext->AnimationManager()->
            FlushAnimations(CommonAnimationManager::Cannot_Throttle);
        }
        if (mPresContext->TransitionManager()) {
          mPresContext->TransitionManager()->
            FlushTransitions(CommonAnimationManager::Cannot_Throttle);
        }
        mPresContext->TickLastStyleUpdateForAllAnimations();
      }

      // The FlushResampleRequests() above flushed style changes.
      if (!mIsDestroying) {
        nsAutoScriptBlocker scriptBlocker;
        mPresContext->RestyleManager()->ProcessPendingRestyles();
      }
    }

    // Dispatch any 'animationstart' events those (or earlier) restyles
    // queued up.
    if (!mIsDestroying) {
      mPresContext->AnimationManager()->DispatchEvents();
    }

    // Process whatever XBL constructors those restyles queued up.  This
    // ensures that onload doesn't fire too early and that we won't do extra
    // reflows after those constructors run.
    if (!mIsDestroying) {
      mDocument->BindingManager()->ProcessAttachedQueue();
    }

    // Now those constructors or events might have posted restyle
    // events.  At the same time, we still need up-to-date style data.
    // In particular, reflow depends on style being completely up to
    // date.  If it's not, then style context reparenting, which can
    // happen during reflow, might suddenly pick up the new rules and
    // we'll end up with frames whose style doesn't match the frame
    // type.
    if (!mIsDestroying) {
      nsAutoScriptBlocker scriptBlocker;
      mPresContext->RestyleManager()->ProcessPendingRestyles();
    }


    // There might be more pending constructors now, but we're not going to
    // worry about them.  They can't be triggered during reflow, so we should
    // be good.

    if (flushType >= (mSuppressInterruptibleReflows ? Flush_Layout : Flush_InterruptibleLayout) &&
        !mIsDestroying) {
      mFrameConstructor->RecalcQuotesAndCounters();
      mViewManager->FlushDelayedResize(true);
      if (ProcessReflowCommands(flushType < Flush_Layout) && mContentToScrollTo) {
        // We didn't get interrupted.  Go ahead and scroll to our content
        DoScrollContentIntoView();
        if (mContentToScrollTo) {
          mContentToScrollTo->DeleteProperty(nsGkAtoms::scrolling);
          mContentToScrollTo = nullptr;
        }
      }
    } else if (!mIsDestroying && mSuppressInterruptibleReflows &&
               flushType == Flush_InterruptibleLayout) {
      // We suppressed this flush, but the document thinks it doesn't
      // need to flush anymore.  Let it know what's really going on.
      mDocument->SetNeedLayoutFlush();
    }

    if (flushType >= Flush_Layout) {
      if (!mIsDestroying) {
        mViewManager->UpdateWidgetGeometry();
      }
    }
  }
}

void
PresShell::CharacterDataWillChange(nsIDocument *aDocument,
                                   nsIContent*  aContent,
                                   CharacterDataChangeInfo* aInfo)
{
  NS_PRECONDITION(!mIsDocumentGone, "Unexpected CharacterDataChanged");
  NS_PRECONDITION(aDocument == mDocument, "Unexpected aDocument");

  if (mCaret) {
    // Invalidate the caret's current location before we call into the frame
    // constructor. It is important to do this now, and not wait until the
    // resulting reflow, because this call causes continuation frames of the
    // text frame the caret is in to forget what part of the content they
    // refer to, making it hard for them to return the correct continuation
    // frame to the caret.
    //
    // It's also important to do this before the content actually changes, since
    // in bidi text the caret needs to look at the content to determine its
    // position and shape.
    mCaret->InvalidateOutsideCaret();
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

  // Call this here so it only happens for real content mutations and
  // not cases when the frame constructor calls its own methods to force
  // frame reconstruction.
  nsIContent *container = aContent->GetParent();
  uint32_t selectorFlags =
    container ? (container->GetFlags() & NODE_ALL_SELECTOR_FLAGS) : 0;
  if (selectorFlags != 0 && !aContent->IsRootOfAnonymousSubtree()) {
    Element* element = container->AsElement();
    if (aInfo->mAppend && !aContent->GetNextSibling())
      mPresContext->RestyleManager()->RestyleForAppend(element, aContent);
    else
      mPresContext->RestyleManager()->RestyleForInsertOrChange(element, aContent);
  }

  mFrameConstructor->CharacterDataChanged(aContent, aInfo);
  VERIFY_STYLE_TREE;
}

void
PresShell::ContentStateChanged(nsIDocument* aDocument,
                               nsIContent* aContent,
                               EventStates aStateMask)
{
  NS_PRECONDITION(!mIsDocumentGone, "Unexpected ContentStateChanged");
  NS_PRECONDITION(aDocument == mDocument, "Unexpected aDocument");

  if (mDidInitialize) {
    nsAutoCauseReflowNotifier crNotifier(this);
    mPresContext->RestyleManager()->ContentStateChanged(aContent, aStateMask);
    VERIFY_STYLE_TREE;
  }
}

void
PresShell::DocumentStatesChanged(nsIDocument* aDocument,
                                 EventStates aStateMask)
{
  NS_PRECONDITION(!mIsDocumentGone, "Unexpected DocumentStatesChanged");
  NS_PRECONDITION(aDocument == mDocument, "Unexpected aDocument");

  if (mDidInitialize &&
      mStyleSet->HasDocumentStateDependentStyle(mPresContext,
                                                mDocument->GetRootElement(),
                                                aStateMask)) {
    mPresContext->RestyleManager()->PostRestyleEvent(mDocument->GetRootElement(),
                                                     eRestyle_Subtree,
                                                     NS_STYLE_HINT_NONE);
    VERIFY_STYLE_TREE;
  }

  if (aStateMask.HasState(NS_DOCUMENT_STATE_WINDOW_INACTIVE)) {
    nsIFrame* root = mFrameConstructor->GetRootFrame();
    if (root) {
      root->SchedulePaint();
    }
  }
}

void
PresShell::AttributeWillChange(nsIDocument* aDocument,
                               Element*     aElement,
                               int32_t      aNameSpaceID,
                               nsIAtom*     aAttribute,
                               int32_t      aModType)
{
  NS_PRECONDITION(!mIsDocumentGone, "Unexpected AttributeWillChange");
  NS_PRECONDITION(aDocument == mDocument, "Unexpected aDocument");

  // XXXwaterson it might be more elegant to wait until after the
  // initial reflow to begin observing the document. That would
  // squelch any other inappropriate notifications as well.
  if (mDidInitialize) {
    nsAutoCauseReflowNotifier crNotifier(this);
    mPresContext->RestyleManager()->AttributeWillChange(aElement, aNameSpaceID,
                                                        aAttribute, aModType);
    VERIFY_STYLE_TREE;
  }
}

void
PresShell::AttributeChanged(nsIDocument* aDocument,
                            Element*     aElement,
                            int32_t      aNameSpaceID,
                            nsIAtom*     aAttribute,
                            int32_t      aModType)
{
  NS_PRECONDITION(!mIsDocumentGone, "Unexpected AttributeChanged");
  NS_PRECONDITION(aDocument == mDocument, "Unexpected aDocument");

  // XXXwaterson it might be more elegant to wait until after the
  // initial reflow to begin observing the document. That would
  // squelch any other inappropriate notifications as well.
  if (mDidInitialize) {
    nsAutoCauseReflowNotifier crNotifier(this);
    mPresContext->RestyleManager()->AttributeChanged(aElement, aNameSpaceID,
                                                     aAttribute, aModType);
    VERIFY_STYLE_TREE;
  }
}

void
PresShell::ContentAppended(nsIDocument *aDocument,
                           nsIContent* aContainer,
                           nsIContent* aFirstNewContent,
                           int32_t     aNewIndexInContainer)
{
  NS_PRECONDITION(!mIsDocumentGone, "Unexpected ContentAppended");
  NS_PRECONDITION(aDocument == mDocument, "Unexpected aDocument");
  NS_PRECONDITION(aContainer, "must have container");

  if (!mDidInitialize) {
    return;
  }

  nsAutoCauseReflowNotifier crNotifier(this);

  // Call this here so it only happens for real content mutations and
  // not cases when the frame constructor calls its own methods to force
  // frame reconstruction.
  if (aContainer->IsElement()) {
    // Ensure the container is an element before trying to restyle
    // because it can be the case that the container is a ShadowRoot
    // which is a document fragment.
    mPresContext->RestyleManager()->
      RestyleForAppend(aContainer->AsElement(), aFirstNewContent);
  }

  mFrameConstructor->ContentAppended(aContainer, aFirstNewContent, true);

  if (static_cast<nsINode*>(aContainer) == static_cast<nsINode*>(aDocument) &&
      aFirstNewContent->NodeType() == nsIDOMNode::DOCUMENT_TYPE_NODE) {
    NotifyFontSizeInflationEnabledIsDirty();
  }

  VERIFY_STYLE_TREE;
}

void
PresShell::ContentInserted(nsIDocument* aDocument,
                           nsIContent*  aContainer,
                           nsIContent*  aChild,
                           int32_t      aIndexInContainer)
{
  NS_PRECONDITION(!mIsDocumentGone, "Unexpected ContentInserted");
  NS_PRECONDITION(aDocument == mDocument, "Unexpected aDocument");

  if (!mDidInitialize) {
    return;
  }

  nsAutoCauseReflowNotifier crNotifier(this);

  // Call this here so it only happens for real content mutations and
  // not cases when the frame constructor calls its own methods to force
  // frame reconstruction.
  if (aContainer && aContainer->IsElement()) {
    // Ensure the container is an element before trying to restyle
    // because it can be the case that the container is a ShadowRoot
    // which is a document fragment.
    mPresContext->RestyleManager()->
      RestyleForInsertOrChange(aContainer->AsElement(), aChild);
  }

  mFrameConstructor->ContentInserted(aContainer, aChild, nullptr, true);

  if (((!aContainer && aDocument) ||
      (static_cast<nsINode*>(aContainer) == static_cast<nsINode*>(aDocument))) &&
      aChild->NodeType() == nsIDOMNode::DOCUMENT_TYPE_NODE) {
    NotifyFontSizeInflationEnabledIsDirty();
  }

  VERIFY_STYLE_TREE;
}

void
PresShell::ContentRemoved(nsIDocument *aDocument,
                          nsIContent* aContainer,
                          nsIContent* aChild,
                          int32_t     aIndexInContainer,
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

  // XXX_jwir3: There is no null check for aDocument necessary, since, even
  //            though by nsIMutationObserver, aDocument could be null, the
  //            precondition check that mDocument == aDocument ensures that
  //            aDocument will not be null (since mDocument can't be null unless
  //            we're still intializing).
  mPresContext->EventStateManager()->ContentRemoved(aDocument, aChild);

  nsAutoCauseReflowNotifier crNotifier(this);

  // Call this here so it only happens for real content mutations and
  // not cases when the frame constructor calls its own methods to force
  // frame reconstruction.
  nsIContent* oldNextSibling;
  if (aContainer) {
    oldNextSibling = aContainer->GetChildAt(aIndexInContainer);
  } else {
    oldNextSibling = nullptr;
  }

  if (aContainer && aContainer->IsElement()) {
    mPresContext->RestyleManager()->
      RestyleForRemove(aContainer->AsElement(), aChild, oldNextSibling);
  }

  bool didReconstruct;
  mFrameConstructor->ContentRemoved(aContainer, aChild, oldNextSibling,
                                    nsCSSFrameConstructor::REMOVE_CONTENT,
                                    &didReconstruct);


  if (((aContainer &&
      static_cast<nsINode*>(aContainer) == static_cast<nsINode*>(aDocument)) ||
      aDocument) && aChild->NodeType() == nsIDOMNode::DOCUMENT_TYPE_NODE) {
    NotifyFontSizeInflationEnabledIsDirty();
  }

  VERIFY_STYLE_TREE;
}

void
PresShell::NotifyCounterStylesAreDirty()
{
  nsAutoCauseReflowNotifier reflowNotifier(this);
  mFrameConstructor->BeginUpdate();
  mFrameConstructor->NotifyCounterStylesAreDirty();
  mFrameConstructor->EndUpdate();
}

nsresult
PresShell::ReconstructFrames(void)
{
  NS_PRECONDITION(!mFrameConstructor->GetRootFrame() || mDidInitialize,
                  "Must not have root frame before initial reflow");
  if (!mDidInitialize) {
    // Nothing to do here
    return NS_OK;
  }

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
  nsAutoTArray<nsRefPtr<mozilla::dom::Element>,1> scopeRoots;
  mChangedScopeStyleRoots.SwapElements(scopeRoots);

  if (mStylesHaveChanged) {
    // If we need to restyle everything, no need to restyle individual
    // scoped style roots.
    scopeRoots.Clear();
  }

  mStylesHaveChanged = false;

  if (mIsDestroying) {
    // We don't want to mess with restyles at this point
    return;
  }

  if (mPresContext) {
    mPresContext->RebuildUserFontSet();
    mPresContext->RebuildCounterStyles();
  }

  Element* root = mDocument->GetRootElement();
  if (!mDidInitialize) {
    // Nothing to do here, since we have no frames yet
    return;
  }

  if (!root) {
    // No content to restyle
    return;
  }

  RestyleManager* restyleManager = mPresContext->RestyleManager();
  if (scopeRoots.IsEmpty()) {
    // If scopeRoots is empty, we know that mStylesHaveChanged was true at
    // the beginning of this function, and that we need to restyle the whole
    // document.
    restyleManager->PostRestyleEvent(root, eRestyle_Subtree,
                                     NS_STYLE_HINT_NONE);
  } else {
    for (uint32_t i = 0; i < scopeRoots.Length(); i++) {
      Element* scopeRoot = scopeRoots[i];
      restyleManager->PostRestyleEvent(scopeRoot, eRestyle_Subtree,
                                       NS_STYLE_HINT_NONE);
    }
  }
}

void
nsIPresShell::ReconstructStyleDataExternal()
{
  ReconstructStyleDataInternal();
}

void
PresShell::RecordStyleSheetChange(nsIStyleSheet* aStyleSheet)
{
  if (mStylesHaveChanged)
    return;

  nsRefPtr<CSSStyleSheet> cssStyleSheet = do_QueryObject(aStyleSheet);
  if (cssStyleSheet) {
    Element* scopeElement = cssStyleSheet->GetScopeElement();
    if (scopeElement) {
      mChangedScopeStyleRoots.AppendElement(scopeElement);
      return;
    }
  }

  mStylesHaveChanged = true;
}

void
PresShell::StyleSheetAdded(nsIDocument *aDocument,
                           nsIStyleSheet* aStyleSheet,
                           bool aDocumentSheet)
{
  // We only care when enabled sheets are added
  NS_PRECONDITION(aStyleSheet, "Must have a style sheet!");

  if (aStyleSheet->IsApplicable() && aStyleSheet->HasRules()) {
    RecordStyleSheetChange(aStyleSheet);
  }
}

void
PresShell::StyleSheetRemoved(nsIDocument *aDocument,
                             nsIStyleSheet* aStyleSheet,
                             bool aDocumentSheet)
{
  // We only care when enabled sheets are removed
  NS_PRECONDITION(aStyleSheet, "Must have a style sheet!");

  if (aStyleSheet->IsApplicable() && aStyleSheet->HasRules()) {
    RecordStyleSheetChange(aStyleSheet);
  }
}

void
PresShell::StyleSheetApplicableStateChanged(nsIDocument *aDocument,
                                            nsIStyleSheet* aStyleSheet,
                                            bool aApplicable)
{
  if (aStyleSheet->HasRules()) {
    RecordStyleSheetChange(aStyleSheet);
  }
}

void
PresShell::StyleRuleChanged(nsIDocument *aDocument,
                            nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aOldStyleRule,
                            nsIStyleRule* aNewStyleRule)
{
  RecordStyleSheetChange(aStyleSheet);
}

void
PresShell::StyleRuleAdded(nsIDocument *aDocument,
                          nsIStyleSheet* aStyleSheet,
                          nsIStyleRule* aStyleRule)
{
  RecordStyleSheetChange(aStyleSheet);
}

void
PresShell::StyleRuleRemoved(nsIDocument *aDocument,
                            nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule)
{
  RecordStyleSheetChange(aStyleSheet);
}

nsIFrame*
PresShell::GetRealPrimaryFrameFor(nsIContent* aContent) const
{
  if (aContent->GetDocument() != GetDocument()) {
    return nullptr;
  }
  nsIFrame *primaryFrame = aContent->GetPrimaryFrame();
  if (!primaryFrame)
    return nullptr;
  return nsPlaceholderFrame::GetRealFrameFor(primaryFrame);
}

nsIFrame*
PresShell::GetPlaceholderFrameFor(nsIFrame* aFrame) const
{
  return mFrameConstructor->GetPlaceholderFrameFor(aFrame);
}

nsresult
PresShell::RenderDocument(const nsRect& aRect, uint32_t aFlags,
                          nscolor aBackgroundColor,
                          gfxContext* aThebesContext)
{
  NS_ENSURE_TRUE(!(aFlags & RENDER_IS_UNTRUSTED), NS_ERROR_NOT_IMPLEMENTED);

  nsRootPresContext* rootPresContext = mPresContext->GetRootPresContext();
  if (rootPresContext) {
    rootPresContext->FlushWillPaintObservers();
    if (mIsDestroying)
      return NS_OK;
  }

  nsAutoScriptBlocker blockScripts;

  // Set up the rectangle as the path in aThebesContext
  gfxRect r(0, 0,
            nsPresContext::AppUnitsToFloatCSSPixels(aRect.width),
            nsPresContext::AppUnitsToFloatCSSPixels(aRect.height));
  aThebesContext->NewPath();
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
  aThebesContext->Rectangle(r, true);
#else
  aThebesContext->Rectangle(r);
#endif

  nsIFrame* rootFrame = mFrameConstructor->GetRootFrame();
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
  bool needsGroup = oldOperator != gfxContext::OPERATOR_OVER;

  if (needsGroup) {
    aThebesContext->PushGroup(NS_GET_A(aBackgroundColor) == 0xff ?
                              gfxContentType::COLOR :
                              gfxContentType::COLOR_ALPHA);
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

  nsDeviceContext* devCtx = mPresContext->DeviceContext();
  gfxFloat scale = gfxFloat(devCtx->AppUnitsPerDevPixel())/nsPresContext::AppUnitsPerCSSPixel();
  aThebesContext->Scale(scale, scale);

  // Since canvas APIs use floats to set up their matrices, we may have
  // some slight inaccuracy here. Adjust matrix components that are
  // integers up to the accuracy of floats to be those integers.
  aThebesContext->NudgeCurrentMatrixToIntegers();

  AutoSaveRestoreRenderingState _(this);

  nsRefPtr<nsRenderingContext> rc = new nsRenderingContext();
  rc->Init(devCtx, aThebesContext);

  bool wouldFlushRetainedLayers = false;
  uint32_t flags = nsLayoutUtils::PAINT_IGNORE_SUPPRESSION;
  if (aThebesContext->CurrentMatrix().HasNonIntegerTranslation()) {
    flags |= nsLayoutUtils::PAINT_IN_TRANSFORM;
  }
  if (!(aFlags & RENDER_ASYNC_DECODE_IMAGES)) {
    flags |= nsLayoutUtils::PAINT_SYNC_DECODE_IMAGES;
  }
  if (aFlags & RENDER_USE_WIDGET_LAYERS) {
    // We only support using widget layers on display root's with widgets.
    nsView* view = rootFrame->GetView();
    if (view && view->GetWidget() &&
        nsLayoutUtils::GetDisplayRootFrame(rootFrame) == rootFrame) {
      flags |= nsLayoutUtils::PAINT_WIDGET_LAYERS;
    }
  }
  if (!(aFlags & RENDER_CARET)) {
    wouldFlushRetainedLayers = true;
    flags |= nsLayoutUtils::PAINT_HIDE_CARET;
  }
  if (aFlags & RENDER_IGNORE_VIEWPORT_SCROLLING) {
    wouldFlushRetainedLayers = !IgnoringViewportScrolling();
    mRenderFlags = ChangeFlag(mRenderFlags, true, STATE_IGNORING_VIEWPORT_SCROLLING);
  }
  if (aFlags & RENDER_DRAWWINDOW_NOT_FLUSHING) {
    mRenderFlags = ChangeFlag(mRenderFlags, true, STATE_DRAWWINDOW_NOT_FLUSHING);
  }
  if (aFlags & RENDER_DOCUMENT_RELATIVE) {
    // XXX be smarter about this ... drawWindow might want a rect
    // that's "pretty close" to what our retained layer tree covers.
    // In that case, it wouldn't disturb normal rendering too much,
    // and we should allow it.
    wouldFlushRetainedLayers = true;
    flags |= nsLayoutUtils::PAINT_DOCUMENT_RELATIVE;
  }

  // Don't let drawWindow blow away our retained layer tree
  if ((flags & nsLayoutUtils::PAINT_WIDGET_LAYERS) && wouldFlushRetainedLayers) {
    flags &= ~nsLayoutUtils::PAINT_WIDGET_LAYERS;
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
                           nsRange* aRange)
{
  // iterate though the display items and add up the bounding boxes of each.
  // This will allow the total area of the frames within the range to be
  // determined. To do this, remove an item from the bottom of the list, check
  // whether it should be part of the range, and if so, append it to the top
  // of the temporary list tmpList. If the item is a text frame at the end of
  // the selection range, clip it to the portion of the text frame that is
  // part of the selection. Then, append the wrapper to the top of the list.
  // Otherwise, just delete the item and don't append it.
  nsRect surfaceRect;
  nsDisplayList tmpList;

  nsDisplayItem* i;
  while ((i = aList->RemoveBottom())) {
    // itemToInsert indiciates the item that should be inserted into the
    // temporary list. If null, no item should be inserted.
    nsDisplayItem* itemToInsert = nullptr;
    nsIFrame* frame = i->Frame();
    nsIContent* content = frame->GetContent();
    if (content) {
      bool atStart = (content == aRange->GetStartParent());
      bool atEnd = (content == aRange->GetEndParent());
      if ((atStart || atEnd) && frame->GetType() == nsGkAtoms::textFrame) {
        int32_t frameStartOffset, frameEndOffset;
        frame->GetOffsets(frameStartOffset, frameEndOffset);

        int32_t hilightStart =
          atStart ? std::max(aRange->StartOffset(), frameStartOffset) : frameStartOffset;
        int32_t hilightEnd =
          atEnd ? std::min(aRange->EndOffset(), frameEndOffset) : frameEndOffset;
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
          nscoord x = std::min(startPoint.x, endPoint.x);
          textRect.x += x;
          textRect.width = std::max(startPoint.x, endPoint.x) - x;
          surfaceRect.UnionRect(surfaceRect, textRect);

          DisplayItemClip newClip;
          newClip.SetTo(textRect);
          newClip.IntersectWith(i->GetClip());
          i->SetClip(aBuilder, newClip);
          itemToInsert = i;
        }
      }
      // Don't try to descend into subdocuments.
      // If this ever changes we'd need to add handling for subdocuments with
      // different zoom levels.
      else if (content->GetCurrentDoc() ==
                 aRange->GetStartParent()->GetCurrentDoc()) {
        // if the node is within the range, append it to the temporary list
        bool before, after;
        nsresult rv =
          nsRange::CompareNodeToRange(content, aRange, &before, &after);
        if (NS_SUCCEEDED(rv) && !before && !after) {
          itemToInsert = i;
          bool snap;
          surfaceRect.UnionRect(surfaceRect, i->GetBounds(aBuilder, &snap));
        }
      }
    }

    // insert the item into the list if necessary. If the item has a child
    // list, insert that as well
    nsDisplayList* sublist = i->GetSameCoordinateSystemChildren();
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

static bool gDumpRangePaintList = false;
#endif

RangePaintInfo*
PresShell::CreateRangePaintInfo(nsIDOMRange* aRange,
                                nsRect& aSurfaceRect,
                                bool aForPrimarySelection)
{
  RangePaintInfo* info = nullptr;

  nsRange* range = static_cast<nsRange*>(aRange);

  nsIFrame* ancestorFrame;
  nsIFrame* rootFrame = GetRootFrame();

  // If the start or end of the range is the document, just use the root
  // frame, otherwise get the common ancestor of the two endpoints of the
  // range.
  nsINode* startParent = range->GetStartParent();
  nsINode* endParent = range->GetEndParent();
  nsIDocument* doc = startParent->GetCrossShadowCurrentDoc();
  if (startParent == doc || endParent == doc) {
    ancestorFrame = rootFrame;
  }
  else {
    nsINode* ancestor = nsContentUtils::GetCommonAncestor(startParent, endParent);
    NS_ASSERTION(!ancestor || ancestor->IsNodeOfType(nsINode::eCONTENT),
                 "common ancestor is not content");
    if (!ancestor || !ancestor->IsNodeOfType(nsINode::eCONTENT))
      return nullptr;

    nsIContent* ancestorContent = static_cast<nsIContent*>(ancestor);
    ancestorFrame = ancestorContent->GetPrimaryFrame();

    // use the nearest ancestor frame that includes all continuations as the
    // root for building the display list
    while (ancestorFrame &&
           nsLayoutUtils::GetNextContinuationOrIBSplitSibling(ancestorFrame))
      ancestorFrame = ancestorFrame->GetParent();
  }

  if (!ancestorFrame)
    return nullptr;

  info = new RangePaintInfo(range, ancestorFrame);

  nsRect ancestorRect = ancestorFrame->GetVisualOverflowRect();

  // get a display list containing the range
  info->mBuilder.SetIncludeAllOutOfFlows();
  if (aForPrimarySelection) {
    info->mBuilder.SetSelectedFramesOnly();
  }
  info->mBuilder.EnterPresShell(ancestorFrame, ancestorRect);
  ancestorFrame->BuildDisplayListForStackingContext(&info->mBuilder,
                                                    ancestorRect, &info->mList);

#ifdef DEBUG
  if (gDumpRangePaintList) {
    fprintf(stderr, "CreateRangePaintInfo --- before ClipListToRange:\n");
    nsFrame::PrintDisplayList(&(info->mBuilder), info->mList);
  }
#endif

  nsRect rangeRect = ClipListToRange(&info->mBuilder, &info->mList, range);

  info->mBuilder.LeavePresShell(ancestorFrame, ancestorRect);

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

TemporaryRef<SourceSurface>
PresShell::PaintRangePaintInfo(nsTArray<nsAutoPtr<RangePaintInfo> >* aItems,
                               nsISelection* aSelection,
                               nsIntRegion* aRegion,
                               nsRect aArea,
                               nsIntPoint& aPoint,
                               nsIntRect* aScreenRect)
{
  nsPresContext* pc = GetPresContext();
  if (!pc || aArea.width == 0 || aArea.height == 0)
    return nullptr;

  nsDeviceContext* deviceContext = pc->DeviceContext();

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
  bool resize = (pixelArea.width > maxWidth || pixelArea.height > maxHeight);
  if (resize) {
    scale = 1.0;
    // divide the maximum size by the image size in both directions. Whichever
    // direction produces the smallest result determines how much should be
    // scaled.
    if (pixelArea.width > maxWidth)
      scale = std::min(scale, float(maxWidth) / pixelArea.width);
    if (pixelArea.height > maxHeight)
      scale = std::min(scale, float(maxHeight) / pixelArea.height);

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

  RefPtr<DrawTarget> dt =
   gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
                                 IntSize(pixelArea.width, pixelArea.height),
                                 SurfaceFormat::B8G8R8A8);
  if (!dt) {
    return nullptr;
  }

  nsRefPtr<gfxContext> ctx = new gfxContext(dt);
  nsRefPtr<nsRenderingContext> rc = new nsRenderingContext();
  rc->Init(deviceContext, ctx);

  if (aRegion) {
    // Convert aRegion from CSS pixels to dev pixels
    nsIntRegion region =
      aRegion->ToAppUnits(nsPresContext::AppUnitsPerCSSPixel())
        .ToOutsidePixels(pc->AppUnitsPerDevPixel());
    rc->SetClip(region);
  }

  if (resize)
    rc->Scale(scale, scale);

  // translate so that points are relative to the surface area
  rc->Translate(-aArea.TopLeft());

  // temporarily hide the selection so that text is drawn normally. If a
  // selection is being rendered, use that, otherwise use the presshell's
  // selection.
  nsRefPtr<nsFrameSelection> frameSelection;
  if (aSelection) {
    frameSelection = static_cast<Selection*>(aSelection)->GetFrameSelection();
  }
  else {
    frameSelection = FrameSelection();
  }
  int16_t oldDisplaySelection = frameSelection->GetDisplaySelection();
  frameSelection->SetDisplaySelection(nsISelectionController::SELECTION_HIDDEN);

  // next, paint each range in the selection
  int32_t count = aItems->Length();
  for (int32_t i = 0; i < count; i++) {
    RangePaintInfo* rangeInfo = (*aItems)[i];
    // the display lists paint relative to the offset from the reference
    // frame, so translate the rendering context
    nsRenderingContext::AutoPushTranslation
      translate(rc, rangeInfo->mRootOffset);

    aArea.MoveBy(-rangeInfo->mRootOffset.x, -rangeInfo->mRootOffset.y);
    nsRegion visible(aArea);
    rangeInfo->mList.ComputeVisibilityForRoot(&rangeInfo->mBuilder, &visible);
    rangeInfo->mList.PaintRoot(&rangeInfo->mBuilder, rc, nsDisplayList::PAINT_DEFAULT);
    aArea.MoveBy(rangeInfo->mRootOffset.x, rangeInfo->mRootOffset.y);
  }

  // restore the old selection display state
  frameSelection->SetDisplaySelection(oldDisplaySelection);

  return dt->Snapshot();
}

TemporaryRef<SourceSurface>
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
    return nullptr;

  nsRefPtr<nsRange> range = new nsRange(node);
  if (NS_FAILED(range->SelectNode(aNode)))
    return nullptr;

  RangePaintInfo* info = CreateRangePaintInfo(range, area, false);
  if (info && !rangeItems.AppendElement(info)) {
    delete info;
    return nullptr;
  }

  if (aRegion) {
    // combine the area with the supplied region
    nsIntRect rrectPixels = aRegion->GetBounds();

    nsRect rrect = rrectPixels.ToAppUnits(nsPresContext::AppUnitsPerCSSPixel());
    area.IntersectRect(area, rrect);

    nsPresContext* pc = GetPresContext();
    if (!pc)
      return nullptr;

    // move the region so that it is offset from the topleft corner of the surface
    aRegion->MoveBy(-pc->AppUnitsToDevPixels(area.x),
                    -pc->AppUnitsToDevPixels(area.y));
  }

  return PaintRangePaintInfo(&rangeItems, nullptr, aRegion, area, aPoint,
                             aScreenRect);
}

TemporaryRef<SourceSurface>
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
  int32_t numRanges;
  aSelection->GetRangeCount(&numRanges);
  NS_ASSERTION(numRanges > 0, "RenderSelection called with no selection");

  for (int32_t r = 0; r < numRanges; r++)
  {
    nsCOMPtr<nsIDOMRange> range;
    aSelection->GetRangeAt(r, getter_AddRefs(range));

    RangePaintInfo* info = CreateRangePaintInfo(range, area, true);
    if (info && !rangeItems.AppendElement(info)) {
      delete info;
      return nullptr;
    }
  }

  return PaintRangePaintInfo(&rangeItems, aSelection, nullptr, area, aPoint,
                             aScreenRect);
}

void
PresShell::AddPrintPreviewBackgroundItem(nsDisplayListBuilder& aBuilder,
                                         nsDisplayList&        aList,
                                         nsIFrame*             aFrame,
                                         const nsRect&         aBounds)
{
  aList.AppendNewToBottom(new (&aBuilder)
    nsDisplaySolidColor(&aBuilder, aFrame, aBounds, NS_RGB(115, 115, 115)));
}

static bool
AddCanvasBackgroundColor(const nsDisplayList& aList, nsIFrame* aCanvasFrame,
                         nscolor aColor)
{
  for (nsDisplayItem* i = aList.GetBottom(); i; i = i->GetAbove()) {
    if (i->Frame() == aCanvasFrame &&
        i->GetType() == nsDisplayItem::TYPE_CANVAS_BACKGROUND_COLOR) {
      nsDisplayCanvasBackgroundColor* bg = static_cast<nsDisplayCanvasBackgroundColor*>(i);
      bg->SetExtraBackgroundColor(aColor);
      return true;
    }
    nsDisplayList* sublist = i->GetSameCoordinateSystemChildren();
    if (sublist && AddCanvasBackgroundColor(*sublist, aCanvasFrame, aColor))
      return true;
  }
  return false;
}

void
PresShell::AddCanvasBackgroundColorItem(nsDisplayListBuilder& aBuilder,
                                        nsDisplayList&        aList,
                                        nsIFrame*             aFrame,
                                        const nsRect&         aBounds,
                                        nscolor               aBackstopColor,
                                        uint32_t              aFlags)
{
  if (aBounds.IsEmpty()) {
    return;
  }
  // We don't want to add an item for the canvas background color if the frame
  // (sub)tree we are painting doesn't include any canvas frames. There isn't
  // an easy way to check this directly, but if we check if the root of the
  // (sub)tree we are painting is a canvas frame that should cover us in all
  // cases (it will usually be a viewport frame when we have a canvas frame in
  // the (sub)tree).
  if (!(aFlags & nsIPresShell::FORCE_DRAW) &&
      !nsCSSRendering::IsCanvasFrame(aFrame)) {
    return;
  }

  nscolor bgcolor = NS_ComposeColors(aBackstopColor, mCanvasBackgroundColor);
  if (NS_GET_A(bgcolor) == 0)
    return;

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
          return;
      }
    }
  }

  aList.AppendNewToBottom(
    new (&aBuilder) nsDisplaySolidColor(&aBuilder, aFrame, aBounds, bgcolor));
}

static bool IsTransparentContainerElement(nsPresContext* aPresContext)
{
  nsCOMPtr<nsIDocShell> docShell = aPresContext->GetDocShell();
  if (!docShell) {
    return false;
  }

  nsCOMPtr<nsPIDOMWindow> pwin = docShell->GetWindow();
  if (!pwin)
    return false;
  nsCOMPtr<Element> containerElement = pwin->GetFrameElementInternal();
  return containerElement &&
         containerElement->HasAttr(kNameSpaceID_None, nsGkAtoms::transparent);
}

nscolor PresShell::GetDefaultBackgroundColorToDraw()
{
  if (!mPresContext || !mPresContext->GetBackgroundColorDraw()) {
    return NS_RGB(255,255,255);
  }
  return mPresContext->DefaultBackgroundColor();
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
    bool drawBackgroundImage;
    bool drawBackgroundColor;

    mCanvasBackgroundColor =
      nsCSSRendering::DetermineBackgroundColor(mPresContext, bgStyle,
                                               rootStyleFrame,
                                               drawBackgroundImage,
                                               drawBackgroundColor);
    if (GetPresContext()->IsCrossProcessRootContentDocument() &&
        !IsTransparentContainerElement(mPresContext)) {
      mCanvasBackgroundColor =
        NS_ComposeColors(GetDefaultBackgroundColorToDraw(), mCanvasBackgroundColor);
    }
  }

  // If the root element of the document (ie html) has style 'display: none'
  // then the document's background color does not get drawn; cache the
  // color we actually draw.
  if (!FrameConstructor()->GetRootElementFrame()) {
    mCanvasBackgroundColor = GetDefaultBackgroundColorToDraw();
  }
  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    if (TabChild* tabChild = TabChild::GetFrom(this)) {
      tabChild->SetBackgroundColor(mCanvasBackgroundColor);
    }
  }
}

nscolor PresShell::ComputeBackstopColor(nsView* aDisplayRoot)
{
  nsIWidget* widget = aDisplayRoot->GetWidget();
  if (widget && (widget->GetTransparencyMode() != eTransparencyOpaque ||
                 widget->WidgetPaintsBackground())) {
    // Within a transparent widget, so the backstop color must be
    // totally transparent.
    return NS_RGBA(0,0,0,0);
  }
  // Within an opaque widget (or no widget at all), so the backstop
  // color must be totally opaque. The user's default background
  // as reported by the prescontext is guaranteed to be opaque.
  return GetDefaultBackgroundColorToDraw();
}

struct PaintParams {
  nscolor mBackgroundColor;
};

LayerManager* PresShell::GetLayerManager()
{
  NS_ASSERTION(mViewManager, "Should have view manager");

  nsView* rootView = mViewManager->GetRootView();
  if (rootView) {
    if (nsIWidget* widget = rootView->GetWidget()) {
      return widget->GetLayerManager();
    }
  }
  return nullptr;
}

void PresShell::SetIgnoreViewportScrolling(bool aIgnore)
{
  if (IgnoringViewportScrolling() == aIgnore) {
    return;
  }
  RenderingState state(this);
  state.mRenderFlags = ChangeFlag(state.mRenderFlags, aIgnore,
                                  STATE_IGNORING_VIEWPORT_SCROLLING);
  SetRenderingState(state);
}

nsresult PresShell::SetResolution(float aXResolution, float aYResolution)
{
  if (!(aXResolution > 0.0 && aYResolution > 0.0)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  if (aXResolution == mXResolution && aYResolution == mYResolution) {
    return NS_OK;
  }
  RenderingState state(this);
  state.mXResolution = aXResolution;
  state.mYResolution = aYResolution;
  SetRenderingState(state);
  return NS_OK;
}

gfxSize PresShell::GetCumulativeResolution()
{
  gfxSize resolution = GetResolution();
  nsPresContext* parentCtx = GetPresContext()->GetParentPresContext();
  if (parentCtx) {
    resolution = resolution * parentCtx->PresShell()->GetCumulativeResolution();
  }
  return resolution;
}

void PresShell::SetRenderingState(const RenderingState& aState)
{
  if (mRenderFlags != aState.mRenderFlags) {
    // Rendering state changed in a way that forces us to flush any
    // retained layers we might already have.
    LayerManager* manager = GetLayerManager();
    if (manager) {
      FrameLayerBuilder::InvalidateAllLayers(manager);
    }
  }

  mRenderFlags = aState.mRenderFlags;
  mXResolution = aState.mXResolution;
  mYResolution = aState.mYResolution;
}

void PresShell::SynthesizeMouseMove(bool aFromScroll)
{
  if (!sSynthMouseMove)
    return;

  if (mPaintingSuppressed || !mIsActive || !mPresContext) {
    return;
  }

  if (!mPresContext->IsRoot()) {
    nsIPresShell* rootPresShell = GetRootPresShell();
    if (rootPresShell) {
      rootPresShell->SynthesizeMouseMove(aFromScroll);
    }
    return;
  }

  if (mMouseLocation == nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE))
    return;

  if (!mSynthMouseMoveEvent.IsPending()) {
    nsRefPtr<nsSynthMouseMoveEvent> ev =
        new nsSynthMouseMoveEvent(this, aFromScroll);

    if (!GetPresContext()->RefreshDriver()->AddRefreshObserver(ev,
                                                               Flush_Display)) {
      NS_WARNING("failed to dispatch nsSynthMouseMoveEvent");
      return;
    }

    mSynthMouseMoveEvent = ev;
  }
}

/**
 * Find the first floating view with a widget in a postorder traversal of the
 * view tree that contains the point. Thus more deeply nested floating views
 * are preferred over their ancestors, and floating views earlier in the
 * view hierarchy (i.e., added later) are preferred over their siblings.
 * This is adequate for finding the "topmost" floating view under a point,
 * given that floating views don't supporting having a specific z-index.
 *
 * We cannot exit early when aPt is outside the view bounds, because floating
 * views aren't necessarily included in their parent's bounds, so this could
 * traverse the entire view hierarchy --- use carefully.
 */
static nsView* FindFloatingViewContaining(nsView* aView, nsPoint aPt)
{
  if (aView->GetVisibility() == nsViewVisibility_kHide)
    // No need to look into descendants.
    return nullptr;

  nsIFrame* frame = aView->GetFrame();
  if (frame) {
    if (!frame->IsVisibleConsideringAncestors(nsIFrame::VISIBILITY_CROSS_CHROME_CONTENT_BOUNDARY) ||
        !frame->PresContext()->PresShell()->IsActive()) {
      return nullptr;
    }
  }

  for (nsView* v = aView->GetFirstChild(); v; v = v->GetNextSibling()) {
    nsView* r = FindFloatingViewContaining(v, v->ConvertFromParentCoords(aPt));
    if (r)
      return r;
  }

  if (aView->GetFloating() && aView->HasWidget() &&
      aView->GetDimensions().Contains(aPt))
    return aView;

  return nullptr;
}

/*
 * This finds the first view containing the given point in a postorder
 * traversal of the view tree that contains the point, assuming that the
 * point is not in a floating view.  It assumes that only floating views
 * extend outside the bounds of their parents.
 *
 * This methods should only be called if FindFloatingViewContaining
 * returns null.
 */
static nsView* FindViewContaining(nsView* aView, nsPoint aPt)
{
  if (!aView->GetDimensions().Contains(aPt) ||
      aView->GetVisibility() == nsViewVisibility_kHide) {
    return nullptr;
  }

  nsIFrame* frame = aView->GetFrame();
  if (frame) {
    if (!frame->IsVisibleConsideringAncestors(nsIFrame::VISIBILITY_CROSS_CHROME_CONTENT_BOUNDARY) ||
        !frame->PresContext()->PresShell()->IsActive()) {
      return nullptr;
    }
  }

  for (nsView* v = aView->GetFirstChild(); v; v = v->GetNextSibling()) {
    nsView* r = FindViewContaining(v, v->ConvertFromParentCoords(aPt));
    if (r)
      return r;
  }

  return aView;
}

void
PresShell::ProcessSynthMouseMoveEvent(bool aFromScroll)
{
  // If drag session has started, we shouldn't synthesize mousemove event.
  nsCOMPtr<nsIDragSession> dragSession = nsContentUtils::GetDragSession();
  if (dragSession) {
    mSynthMouseMoveEvent.Forget();
    return;
  }

  // allow new event to be posted while handling this one only if the
  // source of the event is a scroll (to prevent infinite reflow loops)
  if (aFromScroll) {
    mSynthMouseMoveEvent.Forget();
  }

  nsView* rootView = mViewManager ? mViewManager->GetRootView() : nullptr;
  if (mMouseLocation == nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE) ||
      !rootView || !rootView->HasWidget() || !mPresContext) {
    mSynthMouseMoveEvent.Forget();
    return;
  }

  NS_ASSERTION(mPresContext->IsRoot(), "Only a root pres shell should be here");

  // Hold a ref to ourselves so DispatchEvent won't destroy us (since
  // we need to access members after we call DispatchEvent).
  nsCOMPtr<nsIPresShell> kungFuDeathGrip(this);

#ifdef DEBUG_MOUSE_LOCATION
  printf("[ps=%p]synthesizing mouse move to (%d,%d)\n",
         this, mMouseLocation.x, mMouseLocation.y);
#endif

  int32_t APD = mPresContext->AppUnitsPerDevPixel();

  // We need a widget to put in the event we are going to dispatch so we look
  // for a view that has a widget and the mouse location is over. We first look
  // for floating views, if there isn't one we use the root view. |view| holds
  // that view.
  nsView* view = nullptr;

  // The appunits per devpixel ratio of |view|.
  int32_t viewAPD;

  // refPoint will be mMouseLocation relative to the widget of |view|, the
  // widget we will put in the event we dispatch, in viewAPD appunits
  nsPoint refpoint(0, 0);

  // We always dispatch the event to the pres shell that contains the view that
  // the mouse is over. pointVM is the VM of that pres shell.
  nsViewManager *pointVM = nullptr;

  // This could be a bit slow (traverses entire view hierarchy)
  // but it's OK to do it once per synthetic mouse event
  view = FindFloatingViewContaining(rootView, mMouseLocation);
  if (!view) {
    view = rootView;
    nsView *pointView = FindViewContaining(rootView, mMouseLocation);
    // pointView can be null in situations related to mouse capture
    pointVM = (pointView ? pointView : view)->GetViewManager();
    refpoint = mMouseLocation + rootView->ViewToWidgetOffset();
    viewAPD = APD;
  } else {
    pointVM = view->GetViewManager();
    nsIFrame* frame = view->GetFrame();
    NS_ASSERTION(frame, "floating views can't be anonymous");
    viewAPD = frame->PresContext()->AppUnitsPerDevPixel();
    refpoint = mMouseLocation.ConvertAppUnits(APD, viewAPD);
    refpoint -= view->GetOffsetTo(rootView);
    refpoint += view->ViewToWidgetOffset();
  }
  NS_ASSERTION(view->GetWidget(), "view should have a widget here");
  WidgetMouseEvent event(true, NS_MOUSE_MOVE, view->GetWidget(),
                         WidgetMouseEvent::eSynthesized);
  event.refPoint = LayoutDeviceIntPoint::FromAppUnitsToNearest(refpoint, viewAPD);
  event.time = PR_IntervalNow();
  // XXX set event.modifiers ?
  // XXX mnakano I think that we should get the latest information from widget.

  nsCOMPtr<nsIPresShell> shell = pointVM->GetPresShell();
  if (shell) {
    shell->DispatchSynthMouseMove(&event, !aFromScroll);
  }

  if (!aFromScroll) {
    mSynthMouseMoveEvent.Forget();
  }
}

/* static */ void
PresShell::MarkImagesInListVisible(const nsDisplayList& aList)
{
  for (nsDisplayItem* item = aList.GetBottom(); item; item = item->GetAbove()) {
    nsDisplayList* sublist = item->GetChildren();
    if (sublist) {
      MarkImagesInListVisible(*sublist);
      continue;
    }
    nsIFrame* f = item->Frame();
    // We could check the type of the display item, only a handful can hold an
    // image loading content.
    // dont bother nscomptr here, it is wasteful
    nsCOMPtr<nsIImageLoadingContent> content(do_QueryInterface(f->GetContent()));
    if (content) {
      // use the presshell containing the image
      PresShell* presShell = static_cast<PresShell*>(f->PresContext()->PresShell());
      uint32_t count = presShell->mVisibleImages.Count();
      presShell->mVisibleImages.PutEntry(content);
      if (presShell->mVisibleImages.Count() > count) {
        // content was added to mVisibleImages, so we need to increment its visible count
        content->IncrementVisibleCount();
      }
    }
  }
}

static PLDHashOperator
RemoveAndStore(nsRefPtrHashKey<nsIImageLoadingContent>* aEntry, void* userArg)
{
  nsTArray< nsRefPtr<nsIImageLoadingContent> >* array =
    static_cast< nsTArray< nsRefPtr<nsIImageLoadingContent> >* >(userArg);
  array->AppendElement(aEntry->GetKey());
  return PL_DHASH_REMOVE;
}

void
PresShell::RebuildImageVisibilityDisplayList(const nsDisplayList& aList)
{
  MOZ_ASSERT(!mImageVisibilityVisited, "already visited?");
  mImageVisibilityVisited = true;
  // Remove the entries of the mVisibleImages hashtable and put them in the
  // beforeImageList array.
  nsTArray< nsRefPtr<nsIImageLoadingContent> > beforeImageList;
  beforeImageList.SetCapacity(mVisibleImages.Count());
  mVisibleImages.EnumerateEntries(RemoveAndStore, &beforeImageList);
  MarkImagesInListVisible(aList);
  for (size_t i = 0; i < beforeImageList.Length(); ++i) {
    beforeImageList[i]->DecrementVisibleCount();
  }
}

/* static */ void
PresShell::ClearImageVisibilityVisited(nsView* aView, bool aClear)
{
  nsViewManager* vm = aView->GetViewManager();
  if (aClear) {
    PresShell* presShell = static_cast<PresShell*>(vm->GetPresShell());
    if (!presShell->mImageVisibilityVisited) {
      presShell->ClearVisibleImagesList();
    }
    presShell->mImageVisibilityVisited = false;
  }
  for (nsView* v = aView->GetFirstChild(); v; v = v->GetNextSibling()) {
    ClearImageVisibilityVisited(v, v->GetViewManager() != vm);
  }
}

static PLDHashOperator
DecrementVisibleCount(nsRefPtrHashKey<nsIImageLoadingContent>* aEntry, void* userArg)
{
  aEntry->GetKey()->DecrementVisibleCount();
  return PL_DHASH_NEXT;
}

void
PresShell::ClearVisibleImagesList()
{
  mVisibleImages.EnumerateEntries(DecrementVisibleCount, nullptr);
  mVisibleImages.Clear();
}

void
PresShell::MarkImagesInSubtreeVisible(nsIFrame* aFrame, const nsRect& aRect)
{
  MOZ_ASSERT(aFrame->PresContext()->PresShell() == this, "wrong presshell");

  nsCOMPtr<nsIImageLoadingContent> content(do_QueryInterface(aFrame->GetContent()));
  if (content && aFrame->StyleVisibility()->IsVisible()) {
    uint32_t count = mVisibleImages.Count();
    mVisibleImages.PutEntry(content);
    if (mVisibleImages.Count() > count) {
      // content was added to mVisibleImages, so we need to increment its visible count
      content->IncrementVisibleCount();
    }
  }

  nsSubDocumentFrame* subdocFrame = do_QueryFrame(aFrame);
  if (subdocFrame) {
    nsIPresShell* presShell = subdocFrame->GetSubdocumentPresShellForPainting(
      nsSubDocumentFrame::IGNORE_PAINT_SUPPRESSION);
    if (presShell) {
      nsRect rect = aRect;
      nsIFrame* root = presShell->GetRootFrame();
      if (root) {
        rect.MoveBy(aFrame->GetOffsetToCrossDoc(root));
      } else {
        rect.MoveBy(-aFrame->GetContentRectRelativeToSelf().TopLeft());
      }
      rect = rect.ConvertAppUnitsRoundOut(
        aFrame->PresContext()->AppUnitsPerDevPixel(),
        presShell->GetPresContext()->AppUnitsPerDevPixel());

      presShell->RebuildImageVisibility(&rect);
    }
    return;
  }

  nsRect rect = aRect;

  nsIScrollableFrame* scrollFrame = do_QueryFrame(aFrame);
  if (scrollFrame) {
    nsRect displayPort;
    bool usingDisplayport = nsLayoutUtils::GetDisplayPort(aFrame->GetContent(), &displayPort);
    if (usingDisplayport) {
      rect = displayPort;
    } else {
      rect = rect.Intersect(scrollFrame->GetScrollPortRect());      
    }
    rect = scrollFrame->ExpandRectToNearlyVisible(rect);
  }

  bool preserves3DChildren = aFrame->Preserves3DChildren();

  // we assume all images in popups are visible elsewhere, so we skip them here
  const nsIFrame::ChildListIDs skip(nsIFrame::kPopupList |
                                    nsIFrame::kSelectPopupList);
  for (nsIFrame::ChildListIterator childLists(aFrame);
       !childLists.IsDone(); childLists.Next()) {
    if (skip.Contains(childLists.CurrentID())) {
      continue;
    }

    nsFrameList children = childLists.CurrentList();
    for (nsFrameList::Enumerator e(children); !e.AtEnd(); e.Next()) {
      nsIFrame* child = e.get();

      nsRect r = rect - child->GetPosition();
      if (!r.IntersectRect(r, child->GetVisualOverflowRect())) {
        continue;
      }
      if (child->IsTransformed()) {
        // for children of a preserve3d element we just pass down the same dirty rect
        if (!preserves3DChildren || !child->Preserves3D()) {
          const nsRect overflow = child->GetVisualOverflowRectRelativeToSelf();
          nsRect out;
          if (nsDisplayTransform::UntransformRect(r, overflow, child, nsPoint(0,0), &out)) {
            r = out;
          } else {
            r.SetEmpty();
          }
        }
      }
      MarkImagesInSubtreeVisible(child, r);
    }
  }
}

void
PresShell::RebuildImageVisibility(nsRect* aRect)
{
  MOZ_ASSERT(!mImageVisibilityVisited, "already visited?");
  mImageVisibilityVisited = true;

  nsIFrame* rootFrame = GetRootFrame();
  if (!rootFrame) {
    return;
  }

  // Remove the entries of the mVisibleImages hashtable and put them in the
  // beforeImageList array.
  nsTArray< nsRefPtr<nsIImageLoadingContent> > beforeImageList;
  beforeImageList.SetCapacity(mVisibleImages.Count());
  mVisibleImages.EnumerateEntries(RemoveAndStore, &beforeImageList);

  nsRect vis(nsPoint(0, 0), rootFrame->GetSize());
  if (aRect) {
    vis = *aRect;
  }
  MarkImagesInSubtreeVisible(rootFrame, vis);

  for (size_t i = 0; i < beforeImageList.Length(); ++i) {
    beforeImageList[i]->DecrementVisibleCount();
  }
}

void
PresShell::UpdateImageVisibility()
{
  MOZ_ASSERT(!mPresContext || mPresContext->IsRootContentDocument(),
    "updating image visibility on a non-root content document?");

  mUpdateImageVisibilityEvent.Revoke();

  if (mHaveShutDown || mIsDestroying) {
    return;
  }

  // call update on that frame
  nsIFrame* rootFrame = GetRootFrame();
  if (!rootFrame) {
    ClearVisibleImagesList();
    return;
  }

  RebuildImageVisibility();
  ClearImageVisibilityVisited(rootFrame->GetView(), true);

#ifdef DEBUG_IMAGE_VISIBILITY_DISPLAY_LIST
  // This can be used to debug the frame walker by comparing beforeImageList and
  // mVisibleImages in RebuildImageVisibilityDisplayList to see if they produce
  // the same results (mVisibleImages holds the images the display list thinks
  // are visible, beforeImageList holds the images the frame walker thinks are
  // visible).
  nsDisplayListBuilder builder(rootFrame, nsDisplayListBuilder::IMAGE_VISIBILITY, false);
  nsRect updateRect(nsPoint(0, 0), rootFrame->GetSize());
  nsIFrame* rootScroll = GetRootScrollFrame();
  if (rootScroll) {
    nsIContent* content = rootScroll->GetContent();
    if (content) {
      nsLayoutUtils::GetDisplayPort(content, &updateRect);
    }

    if (IgnoringViewportScrolling()) {
      builder.SetIgnoreScrollFrame(rootScroll);
      // The ExpandRectToNearlyVisible that the root scroll frame would do gets short
      // circuited due to us ignoring the root scroll frame, so we do it here.
      nsIScrollableFrame* rootScrollable = do_QueryFrame(rootScroll);
      updateRect = rootScrollable->ExpandRectToNearlyVisible(updateRect);
    }
  }
  builder.IgnorePaintSuppression();
  builder.EnterPresShell(rootFrame, updateRect);
  nsDisplayList list;
  rootFrame->BuildDisplayListForStackingContext(&builder, updateRect, &list);
  builder.LeavePresShell(rootFrame, updateRect);

  RebuildImageVisibilityDisplayList(list);

  ClearImageVisibilityVisited(rootFrame->GetView(), true);

  list.DeleteAll();
#endif
}

bool
PresShell::AssumeAllImagesVisible()
{
  static bool sImageVisibilityEnabled = true;
  static bool sImageVisibilityEnabledForBrowserElementsOnly = false;
  static bool sImageVisibilityPrefCached = false;

  if (!sImageVisibilityPrefCached) {
    Preferences::AddBoolVarCache(&sImageVisibilityEnabled,
      "layout.imagevisibility.enabled", true);
    Preferences::AddBoolVarCache(&sImageVisibilityEnabledForBrowserElementsOnly,
      "layout.imagevisibility.enabled_for_browser_elements_only", false);
    sImageVisibilityPrefCached = true;
  }

  if ((!sImageVisibilityEnabled &&
       !sImageVisibilityEnabledForBrowserElementsOnly) ||
      !mPresContext || !mDocument) {
    return true;
  }

  // We assume all images are visible in print, print preview, chrome, xul, and
  // resource docs and don't keep track of them.
  if (mPresContext->Type() == nsPresContext::eContext_PrintPreview ||
      mPresContext->Type() == nsPresContext::eContext_Print ||
      mPresContext->IsChrome() ||
      mDocument->IsResourceDoc() ||
      mDocument->IsXUL()) {
    return true;
  }

  if (!sImageVisibilityEnabled &&
      sImageVisibilityEnabledForBrowserElementsOnly) {
    nsCOMPtr<nsIDocShell> docshell(mPresContext->GetDocShell());
    if (!docshell || !docshell->GetIsInBrowserElement()) {
      return true;
    }
  }

  return false;
}

void
PresShell::ScheduleImageVisibilityUpdate()
{
  if (AssumeAllImagesVisible())
    return;

  if (!mPresContext->IsRootContentDocument()) {
    nsPresContext* presContext = mPresContext->GetToplevelContentDocumentPresContext();
    if (!presContext)
      return;
    MOZ_ASSERT(presContext->IsRootContentDocument(),
      "Didn't get a root prescontext from GetToplevelContentDocumentPresContext?");
    presContext->PresShell()->ScheduleImageVisibilityUpdate();
    return;
  }

  if (mHaveShutDown || mIsDestroying)
    return;

  if (mUpdateImageVisibilityEvent.IsPending())
    return;

  nsRefPtr<nsRunnableMethod<PresShell> > ev =
    NS_NewRunnableMethod(this, &PresShell::UpdateImageVisibility);
  if (NS_SUCCEEDED(NS_DispatchToCurrentThread(ev))) {
    mUpdateImageVisibilityEvent = ev;
  }
}

void
PresShell::EnsureImageInVisibleList(nsIImageLoadingContent* aImage)
{
  if (AssumeAllImagesVisible()) {
    aImage->IncrementVisibleCount();
    return;
  }

#ifdef DEBUG
  // if it has a frame make sure its in this presshell
  nsCOMPtr<nsIContent> content = do_QueryInterface(aImage);
  if (content) {
    PresShell* shell = static_cast<PresShell*>(content->OwnerDoc()->GetShell());
    MOZ_ASSERT(!shell || shell == this, "wrong shell");
  }
#endif

  if (!mVisibleImages.Contains(aImage)) {
    mVisibleImages.PutEntry(aImage);
    aImage->IncrementVisibleCount();
  }
}

void
PresShell::RemoveImageFromVisibleList(nsIImageLoadingContent* aImage)
{
#ifdef DEBUG
  // if it has a frame make sure its in this presshell
  nsCOMPtr<nsIContent> content = do_QueryInterface(aImage);
  if (content) {
    PresShell* shell = static_cast<PresShell*>(content->OwnerDoc()->GetShell());
    MOZ_ASSERT(!shell || shell == this, "wrong shell");
  }
#endif

  if (AssumeAllImagesVisible()) {
    MOZ_ASSERT(mVisibleImages.Count() == 0, "shouldn't have any images in the table");
    return;
  }

  uint32_t count = mVisibleImages.Count();
  mVisibleImages.RemoveEntry(aImage);
  if (mVisibleImages.Count() < count) {
    // aImage was in the hashtable, so we need to decrement its visible count
    aImage->DecrementVisibleCount();
  }
}

class nsAutoNotifyDidPaint
{
public:
  nsAutoNotifyDidPaint(PresShell* aShell, uint32_t aFlags)
    : mShell(aShell), mFlags(aFlags)
  {
  }
  ~nsAutoNotifyDidPaint()
  {
    mShell->GetPresContext()->NotifyDidPaintForSubtree(mFlags);
  }

private:
  PresShell* mShell;
  uint32_t mFlags;
};

class AutoUpdateHitRegion
{
public:
  AutoUpdateHitRegion(PresShell* aShell, nsIFrame* aFrame)
    : mShell(aShell), mFrame(aFrame)
  {
  }
  ~AutoUpdateHitRegion()
  {
    if (XRE_GetProcessType() != GeckoProcessType_Content ||
        !mFrame || !mShell) {
      return;
    }
    TabChild* tabChild = TabChild::GetFrom(mShell);
    if (!tabChild || !tabChild->GetUpdateHitRegion()) {
      return;
    }
    nsRegion region;
    nsDisplayListBuilder builder(mFrame,
                                 nsDisplayListBuilder::EVENT_DELIVERY,
                                 /* aBuildCert= */ false);
    nsDisplayList list;
    nsAutoTArray<nsIFrame*, 100> outFrames;
    nsDisplayItem::HitTestState hitTestState;
    nsRect bounds = mShell->GetPresContext()->GetVisibleArea();
    builder.EnterPresShell(mFrame, bounds);
    mFrame->BuildDisplayListForStackingContext(&builder, bounds, &list);
    builder.LeavePresShell(mFrame, bounds);
    list.HitTest(&builder, bounds, &hitTestState, &outFrames);
    list.DeleteAll();
    for (int32_t i = outFrames.Length() - 1; i >= 0; --i) {
      region.Or(region, nsLayoutUtils::TransformFrameRectToAncestor(
                  outFrames[i], nsRect(nsPoint(0, 0), outFrames[i]->GetSize()), mFrame));
    }
    tabChild->UpdateHitRegion(region);
  }
private:
  PresShell* mShell;
  nsIFrame* mFrame;
};

void
PresShell::RecordShadowStyleChange(ShadowRoot* aShadowRoot)
{
  mChangedScopeStyleRoots.AppendElement(aShadowRoot->GetHost()->AsElement());
}

void
PresShell::Paint(nsView*        aViewToPaint,
                 const nsRegion& aDirtyRegion,
                 uint32_t        aFlags)
{
  PROFILER_LABEL("PresShell", "Paint",
    js::ProfileEntry::Category::GRAPHICS);

  NS_ASSERTION(!mIsDestroying, "painting a destroyed PresShell");
  NS_ASSERTION(aViewToPaint, "null view");

  MOZ_ASSERT(!mImageVisibilityVisited, "should have been cleared");

  if (!mIsActive || mIsZombie) {
    return;
  }

  nsPresContext* presContext = GetPresContext();
  AUTO_LAYOUT_PHASE_ENTRY_POINT(presContext, Paint);

  nsIFrame* frame = aViewToPaint->GetFrame();

  bool isRetainingManager;
  LayerManager* layerManager =
    aViewToPaint->GetWidget()->GetLayerManager(&isRetainingManager);
  NS_ASSERTION(layerManager, "Must be in paint event");
  bool shouldInvalidate = layerManager->NeedsWidgetInvalidation();

  nsAutoNotifyDidPaint notifyDidPaint(this, aFlags);
  AutoUpdateHitRegion updateHitRegion(this, frame);

  // Whether or not we should set first paint when painting is
  // suppressed is debatable. For now we'll do it because
  // B2G relies on first paint to configure the viewport and
  // we only want to do that when we have real content to paint.
  // See Bug 798245
  if (mIsFirstPaint && !mPaintingSuppressed) {
    layerManager->SetIsFirstPaint();
    mIsFirstPaint = false;
  }

  layerManager->BeginTransaction();

  if (frame && isRetainingManager) {
    // Try to do an empty transaction, if the frame tree does not
    // need to be updated. Do not try to do an empty transaction on
    // a non-retained layer manager (like the BasicLayerManager that
    // draws the window title bar on Mac), because a) it won't work
    // and b) below we don't want to clear NS_FRAME_UPDATE_LAYER_TREE,
    // that will cause us to forget to update the real layer manager!

    if (!(aFlags & PAINT_LAYERS)) {
      if (layerManager->EndEmptyTransaction()) {
        return;
      }
      NS_WARNING("Must complete empty transaction when compositing!");
    }

    if (!(frame->GetStateBits() & NS_FRAME_UPDATE_LAYER_TREE) &&
        !mNextPaintCompressed) {
      NotifySubDocInvalidationFunc computeInvalidFunc =
        presContext->MayHavePaintEventListenerInSubDocument() ? nsPresContext::NotifySubDocInvalidation : 0;
      bool computeInvalidRect = computeInvalidFunc ||
                                (layerManager->GetBackendType() == LayersBackend::LAYERS_BASIC);

      nsAutoPtr<LayerProperties> props(computeInvalidRect ?
                                         LayerProperties::CloneFrom(layerManager->GetRoot()) :
                                         nullptr);

      MaybeSetupTransactionIdAllocator(layerManager, aViewToPaint);

      if (layerManager->EndEmptyTransaction((aFlags & PAINT_COMPOSITE) ?
            LayerManager::END_DEFAULT : LayerManager::END_NO_COMPOSITE)) {
        nsIntRegion invalid;
        if (props) {
          invalid = props->ComputeDifferences(layerManager->GetRoot(), computeInvalidFunc);
        } else {
          LayerProperties::ClearInvalidations(layerManager->GetRoot());
        }
        if (props) {
          if (!invalid.IsEmpty()) {
            nsIntRect bounds = invalid.GetBounds();
            nsRect rect(presContext->DevPixelsToAppUnits(bounds.x),
                        presContext->DevPixelsToAppUnits(bounds.y),
                        presContext->DevPixelsToAppUnits(bounds.width),
                        presContext->DevPixelsToAppUnits(bounds.height));
            if (shouldInvalidate) {
              aViewToPaint->GetViewManager()->InvalidateViewNoSuppression(aViewToPaint, rect);
            }
            presContext->NotifyInvalidation(bounds, 0);
          }
        } else if (shouldInvalidate) {
          aViewToPaint->GetViewManager()->InvalidateView(aViewToPaint);
        }

        frame->UpdatePaintCountForPaintedPresShells();
        return;
      }
    }
    frame->RemoveStateBits(NS_FRAME_UPDATE_LAYER_TREE);
  }
  if (frame) {
    frame->ClearPresShellsFromLastPaint();
  }

  nscolor bgcolor = ComputeBackstopColor(aViewToPaint);
  uint32_t flags = nsLayoutUtils::PAINT_WIDGET_LAYERS | nsLayoutUtils::PAINT_EXISTING_TRANSACTION;
  if (!(aFlags & PAINT_COMPOSITE)) {
    flags |= nsLayoutUtils::PAINT_NO_COMPOSITE;
  }
  if (mNextPaintCompressed) {
    flags |= nsLayoutUtils::PAINT_COMPRESSED;
    mNextPaintCompressed = false;
  }

  if (frame) {
    // We can paint directly into the widget using its layer manager.
    nsLayoutUtils::PaintFrame(nullptr, frame, aDirtyRegion, bgcolor, flags);
    return;
  }

  nsRefPtr<ColorLayer> root = layerManager->CreateColorLayer();
  if (root) {
    nsPresContext* pc = GetPresContext();
    nsIntRect bounds =
      pc->GetVisibleArea().ToOutsidePixels(pc->AppUnitsPerDevPixel());
    bgcolor = NS_ComposeColors(bgcolor, mCanvasBackgroundColor);
    root->SetColor(bgcolor);
    root->SetVisibleRegion(bounds);
    layerManager->SetRoot(root);
  }
  MaybeSetupTransactionIdAllocator(layerManager, aViewToPaint);
  layerManager->EndTransaction(nullptr, nullptr, (aFlags & PAINT_COMPOSITE) ?
    LayerManager::END_DEFAULT : LayerManager::END_NO_COMPOSITE);
}

// static
void
nsIPresShell::SetCapturingContent(nsIContent* aContent, uint8_t aFlags)
{
  // If capture was set for pointer lock, don't unlock unless we are coming
  // out of pointer lock explicitly.
  if (!aContent && gCaptureInfo.mPointerLock &&
      !(aFlags & CAPTURE_POINTERLOCK)) {
    return;
  }

  NS_IF_RELEASE(gCaptureInfo.mContent);

  // only set capturing content if allowed or the CAPTURE_IGNOREALLOWED or
  // CAPTURE_POINTERLOCK flags are used.
  if ((aFlags & CAPTURE_IGNOREALLOWED) || gCaptureInfo.mAllowed ||
      (aFlags & CAPTURE_POINTERLOCK)) {
    if (aContent) {
      NS_ADDREF(gCaptureInfo.mContent = aContent);
    }
    // CAPTURE_POINTERLOCK is the same as CAPTURE_RETARGETTOELEMENT & CAPTURE_IGNOREALLOWED
    gCaptureInfo.mRetargetToElement = ((aFlags & CAPTURE_RETARGETTOELEMENT) != 0) ||
                                      ((aFlags & CAPTURE_POINTERLOCK) != 0);
    gCaptureInfo.mPreventDrag = (aFlags & CAPTURE_PREVENTDRAG) != 0;
    gCaptureInfo.mPointerLock = (aFlags & CAPTURE_POINTERLOCK) != 0;
  }
}

/* static */ void
nsIPresShell::SetPointerCapturingContent(uint32_t aPointerId, nsIContent* aContent)
{
  nsIContent* content = GetPointerCapturingContent(aPointerId);

  PointerInfo* pointerInfo = nullptr;
  if (!content && gActivePointersIds->Get(aPointerId, &pointerInfo) &&
      pointerInfo &&
      nsIDOMMouseEvent::MOZ_SOURCE_MOUSE == pointerInfo->mPointerType) {
    SetCapturingContent(aContent, CAPTURE_PREVENTDRAG);
  }

  if (content) {
    // Releasing capture for given pointer.
    gPointerCaptureList->Remove(aPointerId);
    DispatchGotOrLostPointerCaptureEvent(false, aPointerId, content);
    // Need to check the state because a lostpointercapture listener
    // may have called SetPointerCapture
    if (GetPointerCapturingContent(aPointerId)) {
      return;
    }
  }

  gPointerCaptureList->Put(aPointerId, aContent);
  DispatchGotOrLostPointerCaptureEvent(true, aPointerId, aContent);
}

/* static */ void
nsIPresShell::ReleasePointerCapturingContent(uint32_t aPointerId, nsIContent* aContent)
{
  if (gActivePointersIds->Get(aPointerId)) {
    SetCapturingContent(nullptr, CAPTURE_PREVENTDRAG);
  }

  // Releasing capture for given pointer.
  gPointerCaptureList->Remove(aPointerId);

  DispatchGotOrLostPointerCaptureEvent(false, aPointerId, aContent);
}

/* static */ nsIContent*
nsIPresShell::GetPointerCapturingContent(uint32_t aPointerId)
{
  return gPointerCaptureList->GetWeak(aPointerId);
}

/* static */ bool
nsIPresShell::GetPointerInfo(uint32_t aPointerId, bool& aActiveState)
{
  PointerInfo* pointerInfo = nullptr;
  if (gActivePointersIds->Get(aPointerId, &pointerInfo) && pointerInfo) {
    aActiveState = pointerInfo->mActiveState;
    return true;
  }
  return false;
}

void
PresShell::UpdateActivePointerState(WidgetGUIEvent* aEvent)
{
  switch (aEvent->message) {
  case NS_MOUSE_ENTER:
    // In this case we have to know information about available mouse pointers
    if (WidgetMouseEvent* mouseEvent = aEvent->AsMouseEvent()) {
      gActivePointersIds->Put(mouseEvent->pointerId, new PointerInfo(false, mouseEvent->inputSource));
    }
    break;
  case NS_POINTER_DOWN:
    // In this case we switch pointer to active state
    if (WidgetPointerEvent* pointerEvent = aEvent->AsPointerEvent()) {
      gActivePointersIds->Put(pointerEvent->pointerId, new PointerInfo(true, pointerEvent->inputSource));
    }
    break;
  case NS_POINTER_UP:
    // In this case we remove information about pointer or turn off active state
    if (WidgetPointerEvent* pointerEvent = aEvent->AsPointerEvent()) {
      if(pointerEvent->inputSource != nsIDOMMouseEvent::MOZ_SOURCE_TOUCH) {
        gActivePointersIds->Put(pointerEvent->pointerId, new PointerInfo(false, pointerEvent->inputSource));
      } else {
        gActivePointersIds->Remove(pointerEvent->pointerId);
      }
    }
    break;
  case NS_MOUSE_EXIT:
    // In this case we have to remove information about disappeared mouse pointers
    if (WidgetMouseEvent* mouseEvent = aEvent->AsMouseEvent()) {
      gActivePointersIds->Remove(mouseEvent->pointerId);
    }
    break;
  }
}

nsIContent*
PresShell::GetCurrentEventContent()
{
  if (mCurrentEventContent &&
      mCurrentEventContent->GetCrossShadowCurrentDoc() != mDocument) {
    mCurrentEventContent = nullptr;
    mCurrentEventFrame = nullptr;
  }
  return mCurrentEventContent;
}

nsIFrame*
PresShell::GetCurrentEventFrame()
{
  if (MOZ_UNLIKELY(mIsDestroying)) {
    return nullptr;
  }

  // GetCurrentEventContent() makes sure the content is still in the
  // same document that this pres shell belongs to. If not, then the
  // frame shouldn't get an event, nor should we even assume its safe
  // to try and find the frame.
  nsIContent* content = GetCurrentEventContent();
  if (!mCurrentEventFrame && content) {
    mCurrentEventFrame = content->GetPrimaryFrame();
    MOZ_ASSERT(!mCurrentEventFrame ||
               mCurrentEventFrame->PresContext()->GetPresShell() == this);
  }
  return mCurrentEventFrame;
}

nsIFrame*
PresShell::GetEventTargetFrame()
{
  return GetCurrentEventFrame();
}

already_AddRefed<nsIContent>
PresShell::GetEventTargetContent(WidgetEvent* aEvent)
{
  nsCOMPtr<nsIContent> content = GetCurrentEventContent();
  if (!content) {
    nsIFrame* currentEventFrame = GetCurrentEventFrame();
    if (currentEventFrame) {
      currentEventFrame->GetContentForEvent(aEvent, getter_AddRefs(content));
      NS_ASSERTION(!content || content->GetCrossShadowCurrentDoc() == mDocument,
                   "handing out content from a different doc");
    }
  }
  return content.forget();
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
  mCurrentEventFrame = nullptr;
  mCurrentEventContent = nullptr;

  if (0 != mCurrentEventFrameStack.Length()) {
    mCurrentEventFrame = mCurrentEventFrameStack.ElementAt(0);
    mCurrentEventFrameStack.RemoveElementAt(0);
    mCurrentEventContent = mCurrentEventContentStack.ObjectAt(0);
    mCurrentEventContentStack.RemoveObjectAt(0);

    // Don't use it if it has moved to a different document.
    if (mCurrentEventContent &&
        mCurrentEventContent->GetCrossShadowCurrentDoc() != mDocument) {
      mCurrentEventContent = nullptr;
      mCurrentEventFrame = nullptr;
    }
  }
}

bool PresShell::InZombieDocument(nsIContent *aContent)
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
  nsCOMPtr<nsIPresShell> parent = GetParentPresShellForEventHandling();
  NS_ENSURE_TRUE(parent, nullptr);
  return parent->GetRootWindow();
}

already_AddRefed<nsIPresShell>
PresShell::GetParentPresShellForEventHandling()
{
  NS_ENSURE_TRUE(mPresContext, nullptr);

  // Now, find the parent pres shell and send the event there
  nsCOMPtr<nsIDocShellTreeItem> treeItem = mPresContext->GetDocShell();
  if (!treeItem) {
    treeItem = mForwardingContainer.get();
  }

  // Might have gone away, or never been around to start with
  NS_ENSURE_TRUE(treeItem, nullptr);

  nsCOMPtr<nsIDocShellTreeItem> parentTreeItem;
  treeItem->GetParent(getter_AddRefs(parentTreeItem));
  nsCOMPtr<nsIDocShell> parentDocShell = do_QueryInterface(parentTreeItem);
  NS_ENSURE_TRUE(parentDocShell && treeItem != parentTreeItem, nullptr);

  nsCOMPtr<nsIPresShell> parentPresShell = parentDocShell->GetPresShell();
  return parentPresShell.forget();
}

nsresult
PresShell::RetargetEventToParent(WidgetGUIEvent* aEvent,
                                 nsEventStatus* aEventStatus)
{
  // Send this events straight up to the parent pres shell.
  // We do this for keystroke events in zombie documents or if either a frame
  // or a root content is not present.
  // That way at least the UI key bindings can work.

  nsCOMPtr<nsIPresShell> kungFuDeathGrip(this);
  nsCOMPtr<nsIPresShell> parentPresShell = GetParentPresShellForEventHandling();
  NS_ENSURE_TRUE(parentPresShell, NS_ERROR_FAILURE);

  // Fake the event as though it's from the parent pres shell's root frame.
  return parentPresShell->HandleEvent(parentPresShell->GetRootFrame(), aEvent, true, aEventStatus);
}

void
PresShell::DisableNonTestMouseEvents(bool aDisable)
{
  sDisableNonTestMouseEvents = aDisable;
}

already_AddRefed<nsPIDOMWindow>
PresShell::GetFocusedDOMWindowInOurWindow()
{
  nsCOMPtr<nsPIDOMWindow> rootWindow = GetRootWindow();
  NS_ENSURE_TRUE(rootWindow, nullptr);
  nsCOMPtr<nsPIDOMWindow> focusedWindow;
  nsFocusManager::GetFocusedDescendant(rootWindow, true,
                                       getter_AddRefs(focusedWindow));
  return focusedWindow.forget();
}

void
PresShell::RecordMouseLocation(WidgetGUIEvent* aEvent)
{
  if (!mPresContext)
    return;

  if (!mPresContext->IsRoot()) {
    PresShell* rootPresShell = GetRootPresShell();
    if (rootPresShell) {
      rootPresShell->RecordMouseLocation(aEvent);
    }
    return;
  }

  if ((aEvent->message == NS_MOUSE_MOVE &&
       aEvent->AsMouseEvent()->reason == WidgetMouseEvent::eReal) ||
      aEvent->message == NS_MOUSE_ENTER ||
      aEvent->message == NS_MOUSE_BUTTON_DOWN ||
      aEvent->message == NS_MOUSE_BUTTON_UP) {
    nsIFrame* rootFrame = GetRootFrame();
    if (!rootFrame) {
      nsView* rootView = mViewManager->GetRootView();
      mMouseLocation = nsLayoutUtils::TranslateWidgetToView(mPresContext,
        aEvent->widget, LayoutDeviceIntPoint::ToUntyped(aEvent->refPoint),
        rootView);
    } else {
      mMouseLocation =
        nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, rootFrame);
    }
#ifdef DEBUG_MOUSE_LOCATION
    if (aEvent->message == NS_MOUSE_ENTER)
      printf("[ps=%p]got mouse enter for %p\n",
             this, aEvent->widget);
    printf("[ps=%p]setting mouse location to (%d,%d)\n",
           this, mMouseLocation.x, mMouseLocation.y);
#endif
    if (aEvent->message == NS_MOUSE_ENTER)
      SynthesizeMouseMove(false);
  } else if (aEvent->message == NS_MOUSE_EXIT) {
    // Although we only care about the mouse moving into an area for which this
    // pres shell doesn't receive mouse move events, we don't check which widget
    // the mouse exit was for since this seems to vary by platform.  Hopefully
    // this won't matter at all since we'll get the mouse move or enter after
    // the mouse exit when the mouse moves from one of our widgets into another.
    mMouseLocation = nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
#ifdef DEBUG_MOUSE_LOCATION
    printf("[ps=%p]got mouse exit for %p\n",
           this, aEvent->widget);
    printf("[ps=%p]clearing mouse location\n",
           this);
#endif
  }
}

static void
EvictTouchPoint(nsRefPtr<dom::Touch>& aTouch,
                nsIDocument* aLimitToDocument = nullptr)
{
  nsCOMPtr<nsINode> node(do_QueryInterface(aTouch->mTarget));
  if (node) {
    nsIDocument* doc = node->GetCurrentDoc();
    if (doc && (!aLimitToDocument || aLimitToDocument == doc)) {
      nsIPresShell* presShell = doc->GetShell();
      if (presShell) {
        nsIFrame* frame = presShell->GetRootFrame();
        if (frame) {
          nsPoint pt(aTouch->mRefPoint.x, aTouch->mRefPoint.y);
          nsCOMPtr<nsIWidget> widget = frame->GetView()->GetNearestWidget(&pt);
          if (widget) {
            WidgetTouchEvent event(true, NS_TOUCH_END, widget);
            event.widget = widget;
            event.time = PR_IntervalNow();
            event.touches.AppendElement(aTouch);
            nsEventStatus status;
            widget->DispatchEvent(&event, status);
            return;
          }
        }
      }
    }
  }
  if (!node || !aLimitToDocument || node->OwnerDoc() == aLimitToDocument) {
    // We couldn't dispatch touchend. Remove the touch from gCaptureTouchList
    // explicitly.
    nsIPresShell::gCaptureTouchList->Remove(aTouch->Identifier());
  }
}

static PLDHashOperator
AppendToTouchList(const uint32_t& aKey, nsRefPtr<dom::Touch>& aData, void *aTouchList)
{
  nsTArray< nsRefPtr<dom::Touch> >* touches =
    static_cast<nsTArray< nsRefPtr<dom::Touch> >*>(aTouchList);
  aData->mChanged = false;
  touches->AppendElement(aData);
  return PL_DHASH_NEXT;
}

void
PresShell::EvictTouches()
{
  nsTArray< nsRefPtr<dom::Touch> > touches;
  gCaptureTouchList->Enumerate(&AppendToTouchList, &touches);
  for (uint32_t i = 0; i < touches.Length(); ++i) {
    EvictTouchPoint(touches[i], mDocument);
  }
}

static PLDHashOperator
FindAnyTarget(const uint32_t& aKey, nsRefPtr<dom::Touch>& aData,
              void* aAnyTarget)
{
  if (aData) {
    dom::EventTarget* target = aData->GetTarget();
    if (target) {
      nsCOMPtr<nsIContent>* content =
        static_cast<nsCOMPtr<nsIContent>*>(aAnyTarget);
      *content = do_QueryInterface(target);
      return PL_DHASH_STOP;
    }
  }
  return PL_DHASH_NEXT;
}

nsIFrame* GetNearestFrameContainingPresShell(nsIPresShell* aPresShell)
{
  nsView* view = aPresShell->GetViewManager()->GetRootView();
  while (view && !view->GetFrame()) {
    view = view->GetParent();
  }

  nsIFrame* frame = nullptr;
  if (view) {
    frame = view->GetFrame();
  }

  return frame;
}

static bool
FlushThrottledStyles(nsIDocument *aDocument, void *aData)
{
  nsIPresShell* shell = aDocument->GetShell();
  if (shell && shell->IsVisible()) {
    nsPresContext* presContext = shell->GetPresContext();
    if (presContext) {
      presContext->TransitionManager()->UpdateAllThrottledStyles();
      presContext->AnimationManager()->UpdateAllThrottledStyles();
    }
  }

  return true;
}

static nsresult
DispatchPointerFromMouseOrTouch(PresShell* aShell,
                                nsIFrame* aFrame,
                                WidgetGUIEvent* aEvent,
                                bool aDontRetargetEvents,
                                nsEventStatus* aStatus)
{
  uint32_t pointerMessage = 0;
  if (aEvent->eventStructType == NS_MOUSE_EVENT) {
    WidgetMouseEvent* mouseEvent = aEvent->AsMouseEvent();
    // if it is not mouse then it is likely will come as touch event
    if (!mouseEvent->convertToPointer) {
      return NS_OK;
    }
    int16_t button = mouseEvent->button;
    switch (mouseEvent->message) {
    case NS_MOUSE_MOVE:
      if (mouseEvent->buttons == 0) {
        button = -1;
      }
      pointerMessage = NS_POINTER_MOVE;
      break;
    case NS_MOUSE_BUTTON_UP:
      pointerMessage = NS_POINTER_UP;
      break;
    case NS_MOUSE_BUTTON_DOWN:
      pointerMessage = NS_POINTER_DOWN;
      break;
    default:
      return NS_OK;
    }

    WidgetPointerEvent event(*mouseEvent);
    event.message = pointerMessage;
    event.button = button;
    event.pressure = event.buttons ?
                     mouseEvent->pressure ? mouseEvent->pressure : 0.5f :
                     0.0f;
    event.convertToPointer = mouseEvent->convertToPointer = false;
    aShell->HandleEvent(aFrame, &event, aDontRetargetEvents, aStatus);
  } else if (aEvent->eventStructType == NS_TOUCH_EVENT) {
    WidgetTouchEvent* touchEvent = aEvent->AsTouchEvent();
    // loop over all touches and dispatch pointer events on each touch
    // copy the event
    switch (touchEvent->message) {
    case NS_TOUCH_MOVE:
      pointerMessage = NS_POINTER_MOVE;
      break;
    case NS_TOUCH_END:
      pointerMessage = NS_POINTER_UP;
      break;
    case NS_TOUCH_START:
      pointerMessage = NS_POINTER_DOWN;
      break;
    case NS_TOUCH_CANCEL:
      pointerMessage = NS_POINTER_CANCEL;
      break;
    default:
      return NS_OK;
    }

    for (uint32_t i = 0; i < touchEvent->touches.Length(); ++i) {
      mozilla::dom::Touch* touch = touchEvent->touches[i];
      if (!touch || !touch->convertToPointer) {
        continue;
      }

      WidgetPointerEvent event(touchEvent->mFlags.mIsTrusted, pointerMessage, touchEvent->widget);
      event.isPrimary = i == 0;
      event.pointerId = touch->Identifier();
      event.refPoint.x = touch->mRefPoint.x;
      event.refPoint.y = touch->mRefPoint.y;
      event.modifiers = touchEvent->modifiers;
      event.width = touch->RadiusX();
      event.height = touch->RadiusY();
      event.tiltX = touch->tiltX;
      event.tiltY = touch->tiltY;
      event.time = touchEvent->time;
      event.timeStamp = touchEvent->timeStamp;
      event.mFlags = touchEvent->mFlags;
      event.button = WidgetMouseEvent::eLeftButton;
      event.buttons = WidgetMouseEvent::eLeftButtonFlag;
      event.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
      event.convertToPointer = touch->convertToPointer = false;
      aShell->HandleEvent(aFrame, &event, aDontRetargetEvents, aStatus);
    }
  }
  return NS_OK;
}

class ReleasePointerCaptureCaller
{
public:
  ReleasePointerCaptureCaller() :
    mPointerId(0),
    mContent(nullptr)
  {
  }
  ~ReleasePointerCaptureCaller()
  {
    if (mContent) {
      nsIPresShell::ReleasePointerCapturingContent(mPointerId, mContent);
    }
  }
  void SetTarget(uint32_t aPointerId, nsIContent* aContent)
  {
    mPointerId = aPointerId;
    mContent = aContent;
  }

private:
  int32_t mPointerId;
  nsCOMPtr<nsIContent> mContent;
};

nsresult
PresShell::HandleEvent(nsIFrame* aFrame,
                       WidgetGUIEvent* aEvent,
                       bool aDontRetargetEvents,
                       nsEventStatus* aEventStatus)
{
#ifdef MOZ_TASK_TRACER
  // Make touch events, mouse events and hardware key events to be the source
  // events of TaskTracer, and originate the rest correlation tasks from here.
  SourceEventType type = SourceEventType::UNKNOWN;
  if (WidgetTouchEvent* inputEvent = aEvent->AsTouchEvent()) {
    type = SourceEventType::TOUCH;
  } else if (WidgetMouseEvent* inputEvent = aEvent->AsMouseEvent()) {
    type = SourceEventType::MOUSE;
  } else if (WidgetKeyboardEvent* inputEvent = aEvent->AsKeyboardEvent()) {
    type = SourceEventType::KEY;
  }
  AutoSourceEvent taskTracerEvent(type);
#endif

  if (sPointerEventEnabled) {
    DispatchPointerFromMouseOrTouch(this, aFrame, aEvent, aDontRetargetEvents, aEventStatus);
  }

  NS_ASSERTION(aFrame, "null frame");

  if (mIsDestroying ||
      (sDisableNonTestMouseEvents && !aEvent->mFlags.mIsSynthesizedForTests &&
       aEvent->HasMouseEventMessage())) {
    return NS_OK;
  }

  RecordMouseLocation(aEvent);

  // Determine whether event need to be consumed by touch caret or not.
  if (TouchCaretPrefEnabled() || SelectionCaretPrefEnabled()) {
    // We have to target the focus window because regardless of where the
    // touch goes, we want to access the touch caret when user is typing on an
    // editable element.
    nsCOMPtr<nsPIDOMWindow> window = GetFocusedDOMWindowInOurWindow();
    nsCOMPtr<nsIDocument> retargetEventDoc = window ? window->GetExtantDoc() : nullptr;
    nsCOMPtr<nsIPresShell> presShell = retargetEventDoc ?
                                       retargetEventDoc->GetShell() :
                                       nullptr;

    nsRefPtr<SelectionCarets> selectionCaret = presShell ?
                                               presShell->GetSelectionCarets() :
                                               nullptr;
    if (selectionCaret) {
      *aEventStatus = selectionCaret->HandleEvent(aEvent);
      if (*aEventStatus == nsEventStatus_eConsumeNoDefault) {
        // If the event is consumed by the touch caret, Cancel APZC panning by
        // setting mMultipleActionsPrevented.
        aEvent->mFlags.mMultipleActionsPrevented = true;
        return NS_OK;
      }
    }

    nsRefPtr<TouchCaret> touchCaret = presShell ? presShell->GetTouchCaret() : nullptr;
    if (touchCaret) {
      *aEventStatus = touchCaret->HandleEvent(aEvent);
      if (*aEventStatus == nsEventStatus_eConsumeNoDefault) {
        // If the event is consumed by the touch caret, cancel APZC panning by
        // setting mMultipleActionsPrevented.
        aEvent->mFlags.mMultipleActionsPrevented = true;
        return NS_OK;
      }
    }
  }

  if (sPointerEventEnabled) {
    UpdateActivePointerState(aEvent);
  }

  if (!nsContentUtils::IsSafeToRunScript())
    return NS_OK;

  nsIContent* capturingContent =
    (aEvent->HasMouseEventMessage() ||
     aEvent->eventStructType == NS_WHEEL_EVENT ? GetCapturingContent() :
                                                 nullptr);

  nsCOMPtr<nsIDocument> retargetEventDoc;
  if (!aDontRetargetEvents) {
    // key and IME related events should not cross top level window boundary.
    // Basically, such input events should be fired only on focused widget.
    // However, some IMEs might need to clean up composition after focused
    // window is deactivated.  And also some tests on MozMill want to test key
    // handling on deactivated window because MozMill window can be activated
    // during tests.  So, there is no merit the events should be redirected to
    // active window.  So, the events should be handled on the last focused
    // content in the last focused DOM window in same top level window.
    // Note, if no DOM window has been focused yet, we can discard the events.
    if (aEvent->IsTargetedAtFocusedWindow()) {
      nsCOMPtr<nsPIDOMWindow> window = GetFocusedDOMWindowInOurWindow();
      // No DOM window in same top level window has not been focused yet,
      // discard the events.
      if (!window) {
        return NS_OK;
      }

      retargetEventDoc = window->GetExtantDoc();
      if (!retargetEventDoc)
        return NS_OK;
    } else if (capturingContent) {
      // if the mouse is being captured then retarget the mouse event at the
      // document that is being captured.
      retargetEventDoc = capturingContent->GetCrossShadowCurrentDoc();
#ifdef ANDROID
    } else if (aEvent->eventStructType == NS_TOUCH_EVENT) {
      retargetEventDoc = GetTouchEventTargetDocument();
#endif
    }

    if (retargetEventDoc) {
      nsCOMPtr<nsIPresShell> presShell = retargetEventDoc->GetShell();
      if (!presShell)
        return NS_OK;

      if (presShell != this) {
        nsIFrame* frame = presShell->GetRootFrame();
        if (!frame) {
          if (aEvent->message == NS_QUERY_TEXT_CONTENT ||
              aEvent->IsContentCommandEvent()) {
            return NS_OK;
          }

          frame = GetNearestFrameContainingPresShell(presShell);
        }

        if (!frame)
          return NS_OK;

        nsCOMPtr<nsIPresShell> shell = frame->PresContext()->GetPresShell();
        return shell->HandleEvent(frame, aEvent, true, aEventStatus);
      }
    }
  }

  if (aEvent->eventStructType == NS_KEY_EVENT &&
      mDocument && mDocument->EventHandlingSuppressed()) {
    if (aEvent->message == NS_KEY_DOWN) {
      mNoDelayedKeyEvents = true;
    } else if (!mNoDelayedKeyEvents) {
      DelayedEvent* event = new DelayedKeyEvent(aEvent->AsKeyboardEvent());
      if (!mDelayedEvents.AppendElement(event)) {
        delete event;
      }
    }
    return NS_OK;
  }

  nsIFrame* frame = aFrame;

  if (aEvent->IsUsingCoordinates()) {
    ReleasePointerCaptureCaller releasePointerCaptureCaller;
    if (nsLayoutUtils::AreAsyncAnimationsEnabled() && mDocument) {
      if (aEvent->eventStructType == NS_TOUCH_EVENT) {
        nsIDocument::UnlockPointer();
      }

      {  // scope for scriptBlocker.
        nsAutoScriptBlocker scriptBlocker;
        GetRootPresShell()->GetDocument()->
          EnumerateSubDocuments(FlushThrottledStyles, nullptr);
      }
      frame = GetNearestFrameContainingPresShell(this);
    }

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
        frame == mFrameConstructor->GetRootFrame()) {
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

    bool captureRetarget = false;
    if (capturingContent) {
      // If a capture is active, determine if the docshell is visible. If not,
      // clear the capture and target the mouse event normally instead. This
      // would occur if the mouse button is held down while a tab change occurs.
      // If the docshell is visible, look for a scrolling container.
      bool vis;
      nsCOMPtr<nsIBaseWindow> baseWin =
        do_QueryInterface(mPresContext->GetContainerWeak());
      if (baseWin && NS_SUCCEEDED(baseWin->GetVisibility(&vis)) && vis) {
        captureRetarget = gCaptureInfo.mRetargetToElement;
        if (!captureRetarget) {
          // A check was already done above to ensure that capturingContent is
          // in this presshell.
          NS_ASSERTION(capturingContent->GetCrossShadowCurrentDoc() == GetDocument(),
                       "Unexpected document");
          nsIFrame* captureFrame = capturingContent->GetPrimaryFrame();
          if (captureFrame) {
            if (capturingContent->Tag() == nsGkAtoms::select &&
                capturingContent->IsHTML()) {
              // a dropdown <select> has a child in its selectPopupList and we should
              // capture on that instead.
              nsIFrame* childFrame = captureFrame->GetChildList(nsIFrame::kSelectPopupList).FirstChild();
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
        ClearMouseCapture(nullptr);
        capturingContent = nullptr;
      }
    }

    // all touch events except for touchstart use a captured target
    if (aEvent->eventStructType == NS_TOUCH_EVENT &&
        aEvent->message != NS_TOUCH_START) {
      captureRetarget = true;
    }

    WidgetMouseEvent* mouseEvent = aEvent->AsMouseEvent();
    bool isWindowLevelMouseExit = (aEvent->message == NS_MOUSE_EXIT) &&
      (mouseEvent && mouseEvent->exit == WidgetMouseEvent::eTopLevel);

    // Get the frame at the event point. However, don't do this if we're
    // capturing and retargeting the event because the captured frame will
    // be used instead below. Also keep using the root frame if we're dealing
    // with a window-level mouse exit event since we want to start sending
    // mouse out events at the root EventStateManager.
    if (!captureRetarget && !isWindowLevelMouseExit) {
      nsPoint eventPoint;
      uint32_t flags = 0;
      if (aEvent->message == NS_TOUCH_START) {
        flags |= INPUT_IGNORE_ROOT_SCROLL_FRAME;
        WidgetTouchEvent* touchEvent = aEvent->AsTouchEvent();
        // if this is a continuing session, ensure that all these events are
        // in the same document by taking the target of the events already in
        // the capture list
        nsCOMPtr<nsIContent> anyTarget;
        if (gCaptureTouchList->Count() > 0 && touchEvent->touches.Length() > 1) {
          gCaptureTouchList->Enumerate(&FindAnyTarget, &anyTarget);
        } else {
          gPreventMouseEvents = false;
        }

        for (int32_t i = touchEvent->touches.Length(); i; ) {
          --i;
          dom::Touch* touch = touchEvent->touches[i];

          int32_t id = touch->Identifier();
          if (!gCaptureTouchList->Get(id, nullptr)) {
            // find the target for this touch
            eventPoint = nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent,
                                                              touch->mRefPoint,
                                                              frame);
            nsIFrame* target = FindFrameTargetedByInputEvent(aEvent,
                                                             frame,
                                                             eventPoint,
                                                             flags);
            if (target && !anyTarget) {
              target->GetContentForEvent(aEvent, getter_AddRefs(anyTarget));
              while (anyTarget && !anyTarget->IsElement()) {
                anyTarget = anyTarget->GetParent();
              }
              touch->SetTarget(anyTarget);
            } else {
              nsIFrame* newTargetFrame = nullptr;
              for (nsIFrame* f = target; f;
                   f = nsLayoutUtils::GetParentOrPlaceholderForCrossDoc(f)) {
                if (f->PresContext()->Document() == anyTarget->OwnerDoc()) {
                  newTargetFrame = f;
                  break;
                }
                // We must be in a subdocument so jump directly to the root frame.
                // GetParentOrPlaceholderForCrossDoc gets called immediately to
                // jump up to the containing document.
                f = f->PresContext()->GetPresShell()->GetRootFrame();
              }

              // if we couldn't find a target frame in the same document as
              // anyTarget, remove the touch from the capture touch list, as
              // well as the event->touches array. touchmove events that aren't
              // in the captured touch list will be discarded
              if (!newTargetFrame) {
                touchEvent->touches.RemoveElementAt(i);
              } else {
                target = newTargetFrame;
                nsCOMPtr<nsIContent> targetContent;
                target->GetContentForEvent(aEvent, getter_AddRefs(targetContent));
                while (targetContent && !targetContent->IsElement()) {
                  targetContent = targetContent->GetParent();
                }
                touch->SetTarget(targetContent);
              }
            }
            if (target) {
              frame = target;
            }
          } else {
            // This touch is an old touch, we need to ensure that is not
            // marked as changed and set its target correctly
            touch->mChanged = false;
            int32_t id = touch->Identifier();

            nsRefPtr<dom::Touch> oldTouch = gCaptureTouchList->GetWeak(id);
            if (oldTouch) {
              touch->SetTarget(oldTouch->mTarget);
            }
          }
        }
      } else {
        eventPoint = nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, frame);
      }
      if (mouseEvent && mouseEvent->eventStructType == NS_MOUSE_EVENT &&
          mouseEvent->ignoreRootScrollFrame) {
        flags |= INPUT_IGNORE_ROOT_SCROLL_FRAME;
      }
      nsIFrame* target =
        FindFrameTargetedByInputEvent(aEvent, frame, eventPoint, flags);
      if (target) {
        frame = target;
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
      NS_ASSERTION(capturingContent->GetCrossShadowCurrentDoc() == GetDocument(),
                   "Unexpected document");
      nsIFrame* capturingFrame = capturingContent->GetPrimaryFrame();
      if (capturingFrame) {
        frame = capturingFrame;
      }
    }

    if (aEvent->eventStructType == NS_POINTER_EVENT &&
        aEvent->message != NS_POINTER_DOWN) {
      if (WidgetPointerEvent* pointerEvent = aEvent->AsPointerEvent()) {
        uint32_t pointerId = pointerEvent->pointerId;
        nsIContent* pointerCapturingContent = GetPointerCapturingContent(pointerId);

        if (pointerCapturingContent) {
          if (nsIFrame* capturingFrame = pointerCapturingContent->GetPrimaryFrame()) {
            frame = capturingFrame;
          }

          if (pointerEvent->message == NS_POINTER_UP ||
              pointerEvent->message == NS_POINTER_CANCEL) {
            // Implicitly releasing capture for given pointer.
            // LOST_POINTER_CAPTURE should be send after NS_POINTER_UP or NS_POINTER_CANCEL.
            releasePointerCaptureCaller.SetTarget(pointerId, pointerCapturingContent);
          }
        }
      }
    }

    // Suppress mouse event if it's being targeted at an element inside
    // a document which needs events suppressed
    if (aEvent->eventStructType == NS_MOUSE_EVENT &&
        frame->PresContext()->Document()->EventHandlingSuppressed()) {
      if (aEvent->message == NS_MOUSE_BUTTON_DOWN) {
        mNoDelayedMouseEvents = true;
      } else if (!mNoDelayedMouseEvents && aEvent->message == NS_MOUSE_BUTTON_UP) {
        DelayedEvent* event = new DelayedMouseEvent(aEvent->AsMouseEvent());
        if (!mDelayedEvents.AppendElement(event)) {
          delete event;
        }
      }

      return NS_OK;
    }

    PresShell* shell =
        static_cast<PresShell*>(frame->PresContext()->PresShell());
    switch (aEvent->message) {
      case NS_TOUCH_MOVE:
      case NS_TOUCH_CANCEL:
      case NS_TOUCH_END: {
        // get the correct shell to dispatch to
        WidgetTouchEvent* touchEvent = aEvent->AsTouchEvent();
        nsTArray< nsRefPtr<dom::Touch> >& touches = touchEvent->touches;
        for (uint32_t i = 0; i < touches.Length(); ++i) {
          dom::Touch* touch = touches[i];
          if (!touch) {
            break;
          }

          nsRefPtr<dom::Touch> oldTouch =
            gCaptureTouchList->GetWeak(touch->Identifier());
          if (!oldTouch) {
            break;
          }

          nsCOMPtr<nsIContent> content =
            do_QueryInterface(oldTouch->GetTarget());
          if (!content) {
            break;
          }

          nsIFrame* contentFrame = content->GetPrimaryFrame();
          if (!contentFrame) {
            break;
          }

          shell = static_cast<PresShell*>(
                      contentFrame->PresContext()->PresShell());
          if (shell) {
            break;
          }
        }
        break;
      }
    }

    // Check if we have an active EventStateManager which isn't the
    // EventStateManager of the current PresContext.
    // If that is the case, and mouse is over some ancestor document,
    // forward event handling to the active document.
    // This way content can get mouse events even when
    // mouse is over the chrome or outside the window.
    //
    // Note, currently for backwards compatibility we don't forward mouse events
    // to the active document when mouse is over some subdocument.
    EventStateManager* activeESM =
      EventStateManager::GetActiveEventStateManager();
    if (activeESM && aEvent->HasMouseEventMessage() &&
        activeESM != shell->GetPresContext()->EventStateManager() &&
        static_cast<EventStateManager*>(activeESM)->GetPresContext()) {
      nsIPresShell* activeShell =
        static_cast<EventStateManager*>(activeESM)->GetPresContext()->
          GetPresShell();
      if (activeShell &&
          nsContentUtils::ContentIsCrossDocDescendantOf(activeShell->GetDocument(),
                                                        shell->GetDocument())) {
        shell = static_cast<PresShell*>(activeShell);
        frame = shell->GetRootFrame();
      }
    }

    if (shell != this) {
      // Handle the event in the correct shell.
      // Prevent deletion until we're done with event handling (bug 336582).
      nsCOMPtr<nsIPresShell> kungFuDeathGrip(shell);
      // We pass the subshell's root frame as the frame to start from. This is
      // the only correct alternative; if the event was captured then it
      // must have been captured by us or some ancestor shell and we
      // now ask the subshell to dispatch it normally.
      return shell->HandlePositionedEvent(frame, aEvent, aEventStatus);
    }

    return HandlePositionedEvent(frame, aEvent, aEventStatus);
  }

  nsresult rv = NS_OK;

  if (frame) {
    PushCurrentEventInfo(nullptr, nullptr);

    // key and IME related events go to the focused frame in this DOM window.
    if (aEvent->IsTargetedAtFocusedContent()) {
      mCurrentEventContent = nullptr;

      nsCOMPtr<nsPIDOMWindow> window =
        do_QueryInterface(mDocument->GetWindow());
      nsCOMPtr<nsPIDOMWindow> focusedWindow;
      nsCOMPtr<nsIContent> eventTarget =
        nsFocusManager::GetFocusedDescendant(window, false,
                                             getter_AddRefs(focusedWindow));

      // otherwise, if there is no focused content or the focused content has
      // no frame, just use the root content. This ensures that key events
      // still get sent to the window properly if nothing is focused or if a
      // frame goes away while it is focused.
      if (!eventTarget || !eventTarget->GetPrimaryFrame()) {
        nsCOMPtr<nsIDOMHTMLDocument> htmlDoc = do_QueryInterface(mDocument);
        if (htmlDoc) {
          nsCOMPtr<nsIDOMHTMLElement> body;
          htmlDoc->GetBody(getter_AddRefs(body));
          eventTarget = do_QueryInterface(body);
          if (!eventTarget) {
            eventTarget = mDocument->GetRootElement();
          }
        } else {
          eventTarget = mDocument->GetRootElement();
        }
      }

      if (aEvent->message == NS_KEY_DOWN) {
        NS_IF_RELEASE(gKeyDownTarget);
        NS_IF_ADDREF(gKeyDownTarget = eventTarget);
      }
      else if ((aEvent->message == NS_KEY_PRESS || aEvent->message == NS_KEY_UP) &&
               gKeyDownTarget) {
        // If a different element is now focused for the keypress/keyup event
        // than what was focused during the keydown event, check if the new
        // focused element is not in a chrome document any more, and if so,
        // retarget the event back at the keydown target. This prevents a
        // content area from grabbing the focus from chrome in-between key
        // events.
        if (eventTarget &&
            nsContentUtils::IsChromeDoc(gKeyDownTarget->GetCurrentDoc()) !=
            nsContentUtils::IsChromeDoc(eventTarget->GetCurrentDoc())) {
          eventTarget = gKeyDownTarget;
        }

        if (aEvent->message == NS_KEY_UP) {
          NS_RELEASE(gKeyDownTarget);
        }
      }

      mCurrentEventFrame = nullptr;
      nsIDocument* targetDoc = eventTarget ? eventTarget->OwnerDoc() : nullptr;
      if (targetDoc && targetDoc != mDocument) {
        PopCurrentEventInfo();
        nsCOMPtr<nsIPresShell> shell = targetDoc->GetShell();
        if (shell) {
          rv = static_cast<PresShell*>(shell.get())->
            HandleRetargetedEvent(aEvent, aEventStatus, eventTarget);
        }
        return rv;
      } else {
        mCurrentEventContent = eventTarget;
      }

      if (!GetCurrentEventContent() || !GetCurrentEventFrame() ||
          InZombieDocument(mCurrentEventContent)) {
        rv = RetargetEventToParent(aEvent, aEventStatus);
        PopCurrentEventInfo();
        return rv;
      }
    } else {
      mCurrentEventFrame = frame;
    }
    if (GetCurrentEventFrame()) {
      rv = HandleEventInternal(aEvent, aEventStatus);
    }

#ifdef DEBUG
    ShowEventTargetDebug();
#endif
    PopCurrentEventInfo();
  } else {
    // Activation events need to be dispatched even if no frame was found, since
    // we don't want the focus to be out of sync.

    if (!NS_EVENT_NEEDS_FRAME(aEvent)) {
      mCurrentEventFrame = nullptr;
      return HandleEventInternal(aEvent, aEventStatus);
    }
    else if (aEvent->HasKeyEventMessage()) {
      // Keypress events in new blank tabs should not be completely thrown away.
      // Retarget them -- the parent chrome shell might make use of them.
      return RetargetEventToParent(aEvent, aEventStatus);
    }
  }

  return rv;
}

#ifdef ANDROID
nsIDocument*
PresShell::GetTouchEventTargetDocument()
{
  nsPresContext* context = GetPresContext();
  if (!context || !context->IsRoot()) {
    return nullptr;
  }

  nsCOMPtr<nsIDocShellTreeItem> shellAsTreeItem = context->GetDocShell();
  if (!shellAsTreeItem) {
    return nullptr;
  }

  nsCOMPtr<nsIDocShellTreeOwner> owner;
  shellAsTreeItem->GetTreeOwner(getter_AddRefs(owner));
  if (!owner) {
    return nullptr;
  }

  // now get the primary content shell (active tab)
  nsCOMPtr<nsIDocShellTreeItem> item;
  owner->GetPrimaryContentShell(getter_AddRefs(item));
  nsCOMPtr<nsIDocShell> childDocShell = do_QueryInterface(item);
  if (!childDocShell) {
    return nullptr;
  }

  return childDocShell->GetDocument();
}
#endif

#ifdef DEBUG
void
PresShell::ShowEventTargetDebug()
{
  if (nsFrame::GetShowEventTargetFrameBorder() &&
      GetCurrentEventFrame()) {
    if (mDrawEventTargetFrame) {
      mDrawEventTargetFrame->InvalidateFrame();
    }

    mDrawEventTargetFrame = mCurrentEventFrame;
    mDrawEventTargetFrame->InvalidateFrame();
  }
}
#endif

nsresult
PresShell::HandlePositionedEvent(nsIFrame* aTargetFrame,
                                 WidgetGUIEvent* aEvent,
                                 nsEventStatus* aEventStatus)
{
  nsresult rv = NS_OK;

  PushCurrentEventInfo(nullptr, nullptr);

  mCurrentEventFrame = aTargetFrame;

  if (mCurrentEventFrame) {
    nsCOMPtr<nsIContent> targetElement;
    mCurrentEventFrame->GetContentForEvent(aEvent,
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
        mCurrentEventContent = nullptr;
        mCurrentEventFrame = nullptr;
      } else if (targetElement != mCurrentEventContent) {
        mCurrentEventContent = targetElement;
      }
    }
  }

  if (GetCurrentEventFrame()) {
    rv = HandleEventInternal(aEvent, aEventStatus);
  }

#ifdef DEBUG
  ShowEventTargetDebug();
#endif
  PopCurrentEventInfo();
  return rv;
}

nsresult
PresShell::HandleEventWithTarget(WidgetEvent* aEvent, nsIFrame* aFrame,
                                 nsIContent* aContent, nsEventStatus* aStatus)
{
#if DEBUG
  MOZ_ASSERT(!aFrame || aFrame->PresContext()->GetPresShell() == this,
             "wrong shell");
  if (aContent) {
    nsIDocument* doc = aContent->GetCrossShadowCurrentDoc();
    NS_ASSERTION(doc, "event for content that isn't in a document");
    NS_ASSERTION(!doc || doc->GetShell() == this, "wrong shell");
  }
#endif
  NS_ENSURE_STATE(!aContent || aContent->GetCrossShadowCurrentDoc() == mDocument);

  PushCurrentEventInfo(aFrame, aContent);
  nsresult rv = HandleEventInternal(aEvent, aStatus);
  PopCurrentEventInfo();
  return rv;
}

nsresult
PresShell::HandleEventInternal(WidgetEvent* aEvent, nsEventStatus* aStatus)
{
  nsRefPtr<EventStateManager> manager = mPresContext->EventStateManager();
  nsresult rv = NS_OK;

  if (!NS_EVENT_NEEDS_FRAME(aEvent) || GetCurrentEventFrame()) {
    bool touchIsNew = false;
    bool isHandlingUserInput = false;

    // XXX How about IME events and input events for plugins?
    if (aEvent->mFlags.mIsTrusted) {
      switch (aEvent->message) {
      case NS_KEY_PRESS:
      case NS_KEY_DOWN:
      case NS_KEY_UP: {
        nsIDocument* doc = GetCurrentEventContent() ?
                           mCurrentEventContent->OwnerDoc() : nullptr;
        nsIDocument* fullscreenAncestor = nullptr;
        if (aEvent->AsKeyboardEvent()->keyCode == NS_VK_ESCAPE) {
          if ((fullscreenAncestor = nsContentUtils::GetFullscreenAncestor(doc))) {
            // Prevent default action on ESC key press when exiting
            // DOM fullscreen mode. This prevents the browser ESC key
            // handler from stopping all loads in the document, which
            // would cause <video> loads to stop.
            aEvent->mFlags.mDefaultPrevented = true;
            aEvent->mFlags.mOnlyChromeDispatch = true;

            if (aEvent->message == NS_KEY_UP) {
              // ESC key released while in DOM fullscreen mode.
              // If fullscreen is running in content-only mode, exit the target
              // doctree branch from fullscreen, otherwise fully exit all
              // browser windows and documents from fullscreen mode.
              // Note: in the content-only fullscreen case, we pass the
              // fullscreenAncestor since |doc| may not actually be fullscreen
              // here, and ExitFullscreen() has no affect when passed a
              // non-fullscreen document.
              nsIDocument::ExitFullscreen(
                nsContentUtils::IsFullscreenApiContentOnly() ? fullscreenAncestor : nullptr,
                /* async */ true);
            }
          }
          nsCOMPtr<nsIDocument> pointerLockedDoc =
            do_QueryReferent(EventStateManager::sPointerLockedDoc);
          if (pointerLockedDoc) {
            aEvent->mFlags.mDefaultPrevented = true;
            aEvent->mFlags.mOnlyChromeDispatch = true;
            if (aEvent->message == NS_KEY_UP) {
              nsIDocument::UnlockPointer();
            }
          }
        }
        // Else not full-screen mode or key code is unrestricted, fall
        // through to normal handling.
      }
      case NS_MOUSE_BUTTON_DOWN:
      case NS_MOUSE_BUTTON_UP:
        isHandlingUserInput = true;
        break;
      case NS_TOUCH_START: {
        WidgetTouchEvent* touchEvent = aEvent->AsTouchEvent();
        // if there is only one touch in this touchstart event, assume that it is
        // the start of a new touch session and evict any old touches in the
        // queue
        if (touchEvent->touches.Length() == 1) {
          nsTArray< nsRefPtr<dom::Touch> > touches;
          gCaptureTouchList->Enumerate(&AppendToTouchList, (void *)&touches);
          for (uint32_t i = 0; i < touches.Length(); ++i) {
            EvictTouchPoint(touches[i]);
          }
        }
        // Add any new touches to the queue
        for (uint32_t i = 0; i < touchEvent->touches.Length(); ++i) {
          dom::Touch* touch = touchEvent->touches[i];
          int32_t id = touch->Identifier();
          if (!gCaptureTouchList->Get(id, nullptr)) {
            // If it is not already in the queue, it is a new touch
            touch->mChanged = true;
          }
          touch->mMessage = aEvent->message;
          gCaptureTouchList->Put(id, touch);
        }
        break;
      }
      case NS_TOUCH_CANCEL:
      case NS_TOUCH_END: {
        // Remove the changed touches
        // need to make sure we only remove touches that are ending here
        WidgetTouchEvent* touchEvent = aEvent->AsTouchEvent();
        nsTArray< nsRefPtr<dom::Touch> >& touches = touchEvent->touches;
        for (uint32_t i = 0; i < touches.Length(); ++i) {
          dom::Touch* touch = touches[i];
          if (!touch) {
            continue;
          }
          touch->mMessage = aEvent->message;
          touch->mChanged = true;

          int32_t id = touch->Identifier();
          nsRefPtr<dom::Touch> oldTouch = gCaptureTouchList->GetWeak(id);
          if (!oldTouch) {
            continue;
          }
          nsCOMPtr<EventTarget> targetPtr = oldTouch->mTarget;

          mCurrentEventContent = do_QueryInterface(targetPtr);
          touch->SetTarget(targetPtr);
          gCaptureTouchList->Remove(id);
        }
        // add any touches left in the touch list, but ensure changed=false
        gCaptureTouchList->Enumerate(&AppendToTouchList, (void *)&touches);
        break;
      }
      case NS_TOUCH_MOVE: {
        // Check for touches that changed. Mark them add to queue
        WidgetTouchEvent* touchEvent = aEvent->AsTouchEvent();
        nsTArray< nsRefPtr<dom::Touch> >& touches = touchEvent->touches;
        bool haveChanged = false;
        for (int32_t i = touches.Length(); i; ) {
          --i;
          dom::Touch* touch = touches[i];
          if (!touch) {
            continue;
          }
          int32_t id = touch->Identifier();
          touch->mMessage = aEvent->message;

          nsRefPtr<dom::Touch> oldTouch = gCaptureTouchList->GetWeak(id);
          if (!oldTouch) {
            touches.RemoveElementAt(i);
            continue;
          }
          if (!touch->Equals(oldTouch)) {
            touch->mChanged = true;
            haveChanged = true;
          }

          nsCOMPtr<dom::EventTarget> targetPtr = oldTouch->mTarget;
          if (!targetPtr) {
            touches.RemoveElementAt(i);
            continue;
          }
          touch->SetTarget(targetPtr);

          gCaptureTouchList->Put(id, touch);
          // if we're moving from touchstart to touchmove for this touch
          // we allow preventDefault to prevent mouse events
          if (oldTouch->mMessage != touch->mMessage) {
            touchIsNew = true;
          }
        }
        // is nothing has changed, we should just return
        if (!haveChanged) {
          if (gPreventMouseEvents) {
              *aStatus = nsEventStatus_eConsumeNoDefault;
          }
          return NS_OK;
        }
        break;
      }
      case NS_DRAGDROP_DROP:
        nsCOMPtr<nsIDragSession> session = nsContentUtils::GetDragSession();
        if (session) {
          bool onlyChromeDrop = false;
          session->GetOnlyChromeDrop(&onlyChromeDrop);
          if (onlyChromeDrop) {
            aEvent->mFlags.mOnlyChromeDispatch = true;
          }
        }
        break;
      }
    }

    if (aEvent->message == NS_CONTEXTMENU) {
      WidgetMouseEvent* mouseEvent = aEvent->AsMouseEvent();
      if (mouseEvent->context == WidgetMouseEvent::eContextMenuKey &&
          !AdjustContextMenuKeyEvent(mouseEvent)) {
        return NS_OK;
      }
      if (mouseEvent->IsShift()) {
        aEvent->mFlags.mOnlyChromeDispatch = true;
        aEvent->mFlags.mRetargetToNonNativeAnonymous = true;
      }
    }

    AutoHandlingUserInputStatePusher userInpStatePusher(isHandlingUserInput,
                                                        aEvent, mDocument);

    if (aEvent->mFlags.mIsTrusted && aEvent->message == NS_MOUSE_MOVE) {
      nsIPresShell::AllowMouseCapture(
        EventStateManager::GetActiveEventStateManager() == manager);
    }

    nsAutoPopupStatePusher popupStatePusher(
                             Event::GetEventPopupControlState(aEvent));

    // FIXME. If the event was reused, we need to clear the old target,
    // bug 329430
    aEvent->target = nullptr;

    // 1. Give event to event manager for pre event state changes and
    //    generation of synthetic events.
    rv = manager->PreHandleEvent(mPresContext, aEvent, mCurrentEventFrame, aStatus);

    // 2. Give event to the DOM for third party and JS use.
    if (NS_SUCCEEDED(rv)) {
      bool wasHandlingKeyBoardEvent =
        nsContentUtils::IsHandlingKeyBoardEvent();
      if (aEvent->eventStructType == NS_KEY_EVENT) {
        nsContentUtils::SetIsHandlingKeyBoardEvent(true);
      }
      if (aEvent->IsAllowedToDispatchDOMEvent()) {
        nsPresShellEventCB eventCB(this);
        if (aEvent->eventStructType == NS_TOUCH_EVENT) {
          DispatchTouchEvent(aEvent, aStatus, &eventCB, touchIsNew);
        } else {
          nsCOMPtr<nsINode> eventTarget = mCurrentEventContent.get();
          nsPresShellEventCB* eventCBPtr = &eventCB;
          if (!eventTarget) {
            nsCOMPtr<nsIContent> targetContent;
            if (mCurrentEventFrame) {
              rv = mCurrentEventFrame->
                     GetContentForEvent(aEvent, getter_AddRefs(targetContent));
            }
            if (NS_SUCCEEDED(rv) && targetContent) {
              eventTarget = do_QueryInterface(targetContent);
            } else if (mDocument) {
              eventTarget = do_QueryInterface(mDocument);
              // If we don't have any content, the callback wouldn't probably
              // do nothing.
              eventCBPtr = nullptr;
            }
          }
          if (eventTarget) {
            if (aEvent->eventStructType == NS_COMPOSITION_EVENT ||
                aEvent->eventStructType == NS_TEXT_EVENT) {
              IMEStateManager::DispatchCompositionEvent(eventTarget,
                mPresContext, aEvent, aStatus, eventCBPtr);
            } else {
              EventDispatcher::Dispatch(eventTarget, mPresContext,
                                        aEvent, nullptr, aStatus, eventCBPtr);
            }
          }
        }
      }

      nsContentUtils::SetIsHandlingKeyBoardEvent(wasHandlingKeyBoardEvent);

      // 3. Give event to event manager for post event state changes and
      //    generation of synthetic events.
      if (!mIsDestroying && NS_SUCCEEDED(rv)) {
        rv = manager->PostHandleEvent(mPresContext, aEvent,
                                      GetCurrentEventFrame(), aStatus);
      }
    }

    if (aEvent->message == NS_MOUSE_BUTTON_UP) {
      // reset the capturing content now that the mouse button is up
      SetCapturingContent(nullptr, 0);
    } else if (aEvent->message == NS_MOUSE_MOVE) {
      nsIPresShell::AllowMouseCapture(false);
    }
  }
  return rv;
}

void
nsIPresShell::DispatchGotOrLostPointerCaptureEvent(bool aIsGotCapture,
                                                   uint32_t aPointerId,
                                                   nsIContent* aCaptureTarget)
{
  PointerEventInit init;
  init.mPointerId = aPointerId;
  init.mBubbles = true;
  nsRefPtr<mozilla::dom::PointerEvent> event;
  event = PointerEvent::Constructor(aCaptureTarget,
                                    aIsGotCapture
                                      ? NS_LITERAL_STRING("gotpointercapture")
                                      : NS_LITERAL_STRING("lostpointercapture"),
                                    init);
  if (event) {
    bool dummy;
    aCaptureTarget->DispatchEvent(event->InternalDOMEvent(), &dummy);
  }
}

void
PresShell::DispatchTouchEvent(WidgetEvent* aEvent,
                              nsEventStatus* aStatus,
                              nsPresShellEventCB* aEventCB,
                              bool aTouchIsNew)
{
  // calling preventDefault on touchstart or the first touchmove for a
  // point prevents mouse events
  bool canPrevent = aEvent->message == NS_TOUCH_START ||
              (aEvent->message == NS_TOUCH_MOVE && aTouchIsNew);
  bool preventDefault = false;
  nsEventStatus tmpStatus = nsEventStatus_eIgnore;
  WidgetTouchEvent* touchEvent = aEvent->AsTouchEvent();

  // loop over all touches and dispatch events on any that have changed
  for (uint32_t i = 0; i < touchEvent->touches.Length(); ++i) {
    dom::Touch* touch = touchEvent->touches[i];
    if (!touch || !touch->mChanged) {
      continue;
    }

    nsCOMPtr<EventTarget> targetPtr = touch->mTarget;
    nsCOMPtr<nsIContent> content = do_QueryInterface(targetPtr);
    if (!content) {
      continue;
    }

    nsIDocument* doc = content->OwnerDoc();
    nsIContent* capturingContent = GetCapturingContent();
    if (capturingContent) {
      if (capturingContent->OwnerDoc() != doc) {
        // Wrong document, don't dispatch anything.
        continue;
      }
      content = capturingContent;
    }
    // copy the event
    WidgetTouchEvent newEvent(touchEvent->mFlags.mIsTrusted,
                              touchEvent->message, touchEvent->widget);
    newEvent.AssignTouchEventData(*touchEvent, false);
    newEvent.target = targetPtr;

    nsRefPtr<PresShell> contentPresShell;
    if (doc == mDocument) {
      contentPresShell = static_cast<PresShell*>(doc->GetShell());
      if (contentPresShell) {
        //XXXsmaug huge hack. Pushing possibly capturing content,
        //         even though event target is something else.
        contentPresShell->PushCurrentEventInfo(
            content->GetPrimaryFrame(), content);
      }
    }

    nsIPresShell *presShell = doc->GetShell();
    if (!presShell) {
      continue;
    }

    nsPresContext *context = presShell->GetPresContext();

    tmpStatus = nsEventStatus_eIgnore;
    EventDispatcher::Dispatch(targetPtr, context,
                              &newEvent, nullptr, &tmpStatus, aEventCB);
    if (nsEventStatus_eConsumeNoDefault == tmpStatus ||
        newEvent.mFlags.mMultipleActionsPrevented) {
      preventDefault = true;
    }

    if (newEvent.mFlags.mMultipleActionsPrevented) {
      touchEvent->mFlags.mMultipleActionsPrevented = true;
    }

    if (contentPresShell) {
      contentPresShell->PopCurrentEventInfo();
    }
  }

  // if preventDefault was called on any of the events dispatched
  // and this is touchstart, or the first touchmove, widget should consume
  // other events that would be associated with this touch session
  if (preventDefault && canPrevent) {
    gPreventMouseEvents = true;
  }

  if (gPreventMouseEvents) {
    *aStatus = nsEventStatus_eConsumeNoDefault;
  } else {
    *aStatus = nsEventStatus_eIgnore;
  }
}

// Dispatch event to content only (NOT full processing)
// See also HandleEventWithTarget which does full event processing.
nsresult
PresShell::HandleDOMEventWithTarget(nsIContent* aTargetContent,
                                    WidgetEvent* aEvent,
                                    nsEventStatus* aStatus)
{
  nsresult rv = NS_OK;

  PushCurrentEventInfo(nullptr, aTargetContent);

  // Bug 41013: Check if the event should be dispatched to content.
  // It's possible that we are in the middle of destroying the window
  // and the js context is out of date. This check detects the case
  // that caused a crash in bug 41013, but there may be a better way
  // to handle this situation!
  nsCOMPtr<nsISupports> container = mPresContext->GetContainerWeak();
  if (container) {

    // Dispatch event to content
    rv = EventDispatcher::Dispatch(aTargetContent, mPresContext, aEvent,
                                   nullptr, aStatus);
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

  PushCurrentEventInfo(nullptr, aTargetContent);
  nsCOMPtr<nsISupports> container = mPresContext->GetContainerWeak();
  if (container) {
    rv = EventDispatcher::DispatchDOMEvent(aTargetContent, nullptr, aEvent,
                                           mPresContext, aStatus);
  }

  PopCurrentEventInfo();
  return rv;
}

bool
PresShell::AdjustContextMenuKeyEvent(WidgetMouseEvent* aEvent)
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
      aEvent->refPoint = LayoutDeviceIntPoint::FromUntyped(
        itemFrame->GetScreenRect().BottomLeft() - widgetPoint);

      mCurrentEventContent = itemFrame->GetContent();
      mCurrentEventFrame = itemFrame;

      return true;
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
      nsIFrame* rootFrame = mFrameConstructor->GetRootFrame();
      if (rootFrame) {
        nsView* view = rootFrame->GetClosestView(&offset);
        offset += view->GetOffsetToWidget(aEvent->widget);
        aEvent->refPoint =
          LayoutDeviceIntPoint::FromAppUnitsToNearest(offset, mPresContext->AppUnitsPerDevPixel());
      }
    }
  } else {
    aEvent->widget = nullptr;
  }

  // see if we should use the caret position for the popup
  nsIntPoint caretPoint;
  // Beware! This may flush notifications via synchronous
  // ScrollSelectionIntoView.
  if (PrepareToUseCaretPosition(aEvent->widget, caretPoint)) {
    // caret position is good
    aEvent->refPoint = LayoutDeviceIntPoint::FromUntyped(caretPoint);
    return true;
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
      mCurrentEventFrame = nullptr;
      GetCurrentEventFrame();
    }
  }

  return true;
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
bool
PresShell::PrepareToUseCaretPosition(nsIWidget* aEventWidget, nsIntPoint& aTargetPt)
{
  nsresult rv;

  // check caret visibility
  nsRefPtr<nsCaret> caret = GetCaret();
  NS_ENSURE_TRUE(caret, false);

  bool caretVisible = false;
  rv = caret->GetCaretVisible(&caretVisible);
  if (NS_FAILED(rv) || ! caretVisible)
    return false;

  // caret selection, this is a temporary weak reference, so no refcounting is
  // needed
  nsISelection* domSelection = caret->GetCaretDOMSelection();
  NS_ENSURE_TRUE(domSelection, false);

  // since the match could be an anonymous textnode inside a
  // <textarea> or text <input>, we need to get the outer frame
  // note: frames are not refcounted
  nsIFrame* frame = nullptr; // may be nullptr
  nsCOMPtr<nsIDOMNode> node;
  rv = domSelection->GetFocusNode(getter_AddRefs(node));
  NS_ENSURE_SUCCESS(rv, false);
  NS_ENSURE_TRUE(node, false);
  nsCOMPtr<nsIContent> content(do_QueryInterface(node));
  if (content) {
    nsIContent* nonNative = content->FindFirstNonChromeOnlyAccessContent();
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
    rv = ScrollContentIntoView(content,
                               nsIPresShell::ScrollAxis(
                                 nsIPresShell::SCROLL_MINIMUM,
                                 nsIPresShell::SCROLL_IF_NOT_VISIBLE),
                               nsIPresShell::ScrollAxis(
                                 nsIPresShell::SCROLL_MINIMUM,
                                 nsIPresShell::SCROLL_IF_NOT_VISIBLE),
                               nsIPresShell::SCROLL_OVERFLOW_HIDDEN);
    NS_ENSURE_SUCCESS(rv, false);
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
                                         nsISelectionController::SELECTION_FOCUS_REGION,
                                         nsISelectionController::SCROLL_SYNCHRONOUS);
    NS_ENSURE_SUCCESS(rv, false);
  }

  nsPresContext* presContext = GetPresContext();

  // get caret position relative to the closest view
  nsRect caretCoords;
  nsIFrame* caretFrame = caret->GetGeometry(domSelection, &caretCoords);
  if (!caretFrame)
    return false;
  nsPoint viewOffset;
  nsView* view = caretFrame->GetClosestView(&viewOffset);
  if (!view)
    return false;
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

  return true;
}

void
PresShell::GetCurrentItemAndPositionForElement(nsIDOMElement *aCurrentEl,
                                               nsIContent** aTargetToUse,
                                               LayoutDeviceIntPoint& aTargetPt,
                                               nsIWidget *aRootWidget)
{
  nsCOMPtr<nsIContent> focusedContent(do_QueryInterface(aCurrentEl));
  ScrollContentIntoView(focusedContent,
                        ScrollAxis(),
                        ScrollAxis(),
                        nsIPresShell::SCROLL_OVERFLOW_HIDDEN);

  nsPresContext* presContext = GetPresContext();

  bool istree = false, checkLineHeight = true;
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
    checkLineHeight = false;

    int32_t currentIndex;
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
          int32_t firstVisibleRow, rowHeight;
          treeBox->GetFirstVisibleRow(&firstVisibleRow);
          treeBox->GetRowHeight(&rowHeight);

          extraTreeY += presContext->CSSPixelsToAppUnits(
                          (currentIndex - firstVisibleRow + 1) * rowHeight);
          istree = true;

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
        checkLineHeight = false;
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
    nsView *view = frame->GetClosestView(&frameOrigin);
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
          int32_t APD = presContext->AppUnitsPerDevPixel();
          int32_t scrollAPD = f->PresContext()->AppUnitsPerDevPixel();
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

bool
PresShell::ShouldIgnoreInvalidation()
{
  return mPaintingSuppressed || !mIsActive || mIsNeverPainting;
}

void
PresShell::WillPaint()
{
  nsRootPresContext* rootPresContext = mPresContext->GetRootPresContext();
  if (!rootPresContext) {
    // In some edge cases, such as when we don't have a root frame yet,
    // we can't find the root prescontext. There's nothing to do in that
    // case.
    return;
  }

  // Don't bother doing anything if some viewmanager in our tree is painting
  // while we still have painting suppressed or we are not active.
  if (mPaintingSuppressed || !mIsActive || !IsVisible()) {
    return;
  }

  rootPresContext->FlushWillPaintObservers();
  if (mIsDestroying)
    return;

  // Process reflows, if we have them, to reduce flicker due to invalidates and
  // reflow being interspersed.  Note that we _do_ allow this to be
  // interruptible; if we can't do all the reflows it's better to flicker a bit
  // than to freeze up.
  FlushPendingNotifications(ChangesToFlush(Flush_InterruptibleLayout, false));
}

void
PresShell::WillPaintWindow()
{
  nsRootPresContext* rootPresContext = mPresContext->GetRootPresContext();
  if (rootPresContext != mPresContext) {
    // This could be a popup's presshell. We don't allow plugins in popups
    // so there's nothing to do here.
    return;
  }

#ifndef XP_MACOSX
  rootPresContext->ApplyPluginGeometryUpdates();
#endif
}

void
PresShell::DidPaintWindow()
{
  nsRootPresContext* rootPresContext = mPresContext->GetRootPresContext();
  if (rootPresContext != mPresContext) {
    // This could be a popup's presshell. No point in notifying XPConnect
    // about compositing of popups.
    return;
  }

  if (nsContentUtils::XPConnect()) {
    nsContentUtils::XPConnect()->NotifyDidPaint();
  }
}

bool
PresShell::IsVisible()
{
  if (!mViewManager)
    return false;

  nsView* view = mViewManager->GetRootView();
  if (!view)
    return true;

  // inner view of subdoc frame
  view = view->GetParent();
  if (!view)
    return true;

  // subdoc view
  view = view->GetParent();
  if (!view)
    return true;

  nsIFrame* frame = view->GetFrame();
  if (!frame)
    return true;

  return frame->IsVisibleConsideringAncestors(nsIFrame::VISIBILITY_CROSS_CHROME_CONTENT_BOUNDARY);
}

nsresult
PresShell::GetAgentStyleSheets(nsCOMArray<nsIStyleSheet>& aSheets)
{
  aSheets.Clear();
  int32_t sheetCount = mStyleSet->SheetCount(nsStyleSet::eAgentSheet);

  for (int32_t i = 0; i < sheetCount; ++i) {
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
FreezeElement(nsISupports *aSupports, void * /* unused */)
{
  nsCOMPtr<nsIObjectLoadingContent> olc(do_QueryInterface(aSupports));
  if (olc) {
    olc->StopPluginInstance();
  }
}

static bool
FreezeSubDocument(nsIDocument *aDocument, void *aData)
{
  nsIPresShell *shell = aDocument->GetShell();
  if (shell)
    shell->Freeze();

  return true;
}

void
PresShell::Freeze()
{
  mUpdateImageVisibilityEvent.Revoke();

  MaybeReleaseCapturingContent();

  mDocument->EnumerateActivityObservers(FreezeElement, nullptr);

  if (mCaret) {
    SetCaretEnabled(false);
  }

  mPaintingSuppressed = true;

  if (mDocument) {
    mDocument->EnumerateSubDocuments(FreezeSubDocument, nullptr);
  }

  nsPresContext* presContext = GetPresContext();
  if (presContext &&
      presContext->RefreshDriver()->PresContext() == presContext) {
    presContext->RefreshDriver()->Freeze();
  }

  mFrozen = true;
  if (mDocument) {
    UpdateImageLockingState();
  }
}

void
PresShell::FireOrClearDelayedEvents(bool aFireEvents)
{
  mNoDelayedMouseEvents = false;
  mNoDelayedKeyEvents = false;
  if (!aFireEvents) {
    mDelayedEvents.Clear();
    return;
  }

  if (mDocument) {
    nsCOMPtr<nsIDocument> doc = mDocument;
    while (!mIsDestroying && mDelayedEvents.Length() &&
           !doc->EventHandlingSuppressed()) {
      nsAutoPtr<DelayedEvent> ev(mDelayedEvents[0].forget());
      mDelayedEvents.RemoveElementAt(0);
      ev->Dispatch();
    }
    if (!doc->EventHandlingSuppressed()) {
      mDelayedEvents.Clear();
    }
  }
}

static void
ThawElement(nsISupports *aSupports, void *aShell)
{
  nsCOMPtr<nsIObjectLoadingContent> olc(do_QueryInterface(aSupports));
  if (olc) {
    olc->AsyncStartPluginInstance();
  }
}

static bool
ThawSubDocument(nsIDocument *aDocument, void *aData)
{
  nsIPresShell *shell = aDocument->GetShell();
  if (shell)
    shell->Thaw();

  return true;
}

void
PresShell::Thaw()
{
  nsPresContext* presContext = GetPresContext();
  if (presContext &&
      presContext->RefreshDriver()->PresContext() == presContext) {
    presContext->RefreshDriver()->Thaw();
  }

  mDocument->EnumerateActivityObservers(ThawElement, this);

  if (mDocument)
    mDocument->EnumerateSubDocuments(ThawSubDocument, nullptr);

  // Get the activeness of our presshell, as this might have changed
  // while we were in the bfcache
  QueryIsActive();

  // We're now unfrozen
  mFrozen = false;
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
    mReflowScheduled = true;
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
  }

  mPresContext->FlushUserFontSet();

  mPresContext->FlushCounterStyles();

  mFrameConstructor->BeginUpdate();

  mLastReflowStart = GetPerformanceNow();
}

void
PresShell::DidDoReflow(bool aInterruptible, bool aWasInterrupted)
{
  mFrameConstructor->EndUpdate();

  HandlePostedReflowCallbacks(aInterruptible);

  nsCOMPtr<nsIDocShell> docShell = mPresContext->GetDocShell();
  if (docShell) {
    DOMHighResTimeStamp now = GetPerformanceNow();
    docShell->NotifyReflowObservers(aInterruptible, mLastReflowStart, now);
  }

  if (sSynthMouseMove) {
    SynthesizeMouseMove(false);
  }
  if (mCaret) {
    // Update the caret's position now to account for any changes created by
    // the reflow.
    mCaret->InvalidateOutsideCaret();
    mCaret->UpdateCaretPosition();
  }

  if (!aWasInterrupted) {
    ClearReflowOnZoomPending();
  }
}

DOMHighResTimeStamp
PresShell::GetPerformanceNow()
{
  DOMHighResTimeStamp now = 0;
  nsPIDOMWindow* window = mDocument->GetInnerWindow();

  if (window) {
    nsPerformance* perf = window->GetPerformance();

    if (perf) {
      now = perf->Now();
    }
  }

  return now;
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
  self->mReflowContinueTimer = nullptr;
  self->ScheduleReflow();
}

bool
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
      return false;
    }
  }
  return true;
}

bool
PresShell::DoReflow(nsIFrame* target, bool aInterruptible)
{
  if (mIsZombie) {
    return true;
  }

  gfxTextPerfMetrics* tp = mPresContext->GetTextPerfMetrics();
  TimeStamp timeStart;
  if (tp) {
    tp->Accumulate();
    tp->reflowCount++;
    timeStart = TimeStamp::Now();
  }

  target->SchedulePaint();
  nsIFrame *parent = nsLayoutUtils::GetCrossDocParentFrame(target);
  while (parent) {
    nsSVGEffects::InvalidateDirectRenderingObservers(parent);
    parent = nsLayoutUtils::GetCrossDocParentFrame(parent);
  }

  nsAutoCString docURL("N/A");
  nsIURI *uri = mDocument->GetDocumentURI();
  if (uri)
    uri->GetSpec(docURL);

  PROFILER_LABEL_PRINTF("PresShell", "DoReflow",
    js::ProfileEntry::Category::GRAPHICS, "(%s)", docURL.get());

  if (mReflowContinueTimer) {
    mReflowContinueTimer->Cancel();
    mReflowContinueTimer = nullptr;
  }

  nsIFrame* rootFrame = mFrameConstructor->GetRootFrame();

  nsRefPtr<nsRenderingContext> rcx = CreateReferenceRenderingContext();

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
  } else {
     size = target->GetSize();
  }

  NS_ASSERTION(!target->GetNextInFlow() && !target->GetPrevInFlow(),
               "reflow roots should never split");

  // Don't pass size directly to the reflow state, since a
  // constrained height implies page/column breaking.
  nsSize reflowSize(size.width, NS_UNCONSTRAINEDSIZE);
  nsHTMLReflowState reflowState(mPresContext, target, rcx, reflowSize,
                                nsHTMLReflowState::CALLER_WILL_INIT);

  if (rootFrame == target) {
    reflowState.Init(mPresContext);

    // When the root frame is being reflowed with unconstrained height
    // (which happens when we're called from
    // nsDocumentViewer::SizeToContent), we're effectively doing a
    // vertical resize, since it changes the meaning of percentage
    // heights even if no heights actually changed.  The same applies
    // when we reflow again after that computation.  This is an unusual
    // case, and isn't caught by nsHTMLReflowState::InitResizeFlags.
    bool hasUnconstrainedHeight = size.height == NS_UNCONSTRAINEDSIZE;

    if (hasUnconstrainedHeight || mLastRootReflowHadUnconstrainedHeight) {
      reflowState.mFlags.mVResize = true;
    }

    mLastRootReflowHadUnconstrainedHeight = hasUnconstrainedHeight;
  } else {
    // Initialize reflow state with current used border and padding,
    // in case this was set specially by the parent frame when the reflow root
    // was reflowed by its parent.
    nsMargin currentBorder = target->GetUsedBorder();
    nsMargin currentPadding = target->GetUsedPadding();
    reflowState.Init(mPresContext, -1, -1, &currentBorder, &currentPadding);
  }

  // fix the computed height
  NS_ASSERTION(reflowState.ComputedPhysicalMargin() == nsMargin(0, 0, 0, 0),
               "reflow state should not set margin for reflow roots");
  if (size.height != NS_UNCONSTRAINEDSIZE) {
    nscoord computedHeight =
      size.height - reflowState.ComputedPhysicalBorderPadding().TopBottom();
    computedHeight = std::max(computedHeight, 0);
    reflowState.SetComputedHeight(computedHeight);
  }
  NS_ASSERTION(reflowState.ComputedWidth() ==
                 size.width -
                   reflowState.ComputedPhysicalBorderPadding().LeftRight(),
               "reflow state computed incorrect width");

  mPresContext->ReflowStarted(aInterruptible);
  mIsReflowing = true;

  nsReflowStatus status;
  nsHTMLReflowMetrics desiredSize(reflowState);
  target->Reflow(mPresContext, desiredSize, reflowState, status);

  // If an incremental reflow is initiated at a frame other than the
  // root frame, then its desired size had better not change!  If it's
  // initiated at the root, then the size better not change unless its
  // height was unconstrained to start with.
  nsRect boundsRelativeToTarget = nsRect(0, 0, desiredSize.Width(), desiredSize.Height());
  NS_ASSERTION((target == rootFrame && size.height == NS_UNCONSTRAINEDSIZE) ||
               (desiredSize.Width() == size.width &&
                desiredSize.Height() == size.height),
               "non-root frame's desired size changed during an "
               "incremental reflow");
  NS_ASSERTION(target == rootFrame ||
               desiredSize.VisualOverflow().IsEqualInterior(boundsRelativeToTarget),
               "non-root reflow roots must not have visible overflow");
  NS_ASSERTION(target == rootFrame ||
               desiredSize.ScrollableOverflow().IsEqualEdges(boundsRelativeToTarget),
               "non-root reflow roots must not have scrollable overflow");
  NS_ASSERTION(status == NS_FRAME_COMPLETE,
               "reflow roots should never split");

  target->SetSize(boundsRelativeToTarget.Size());

  // Always use boundsRelativeToTarget here, not desiredSize.GetVisualOverflowArea(),
  // because for root frames (where they could be different, since root frames
  // are allowed to have overflow) the root view bounds need to match the
  // viewport bounds; the view manager "window dimensions" code depends on it.
  nsContainerFrame::SyncFrameViewAfterReflow(mPresContext, target,
                                             target->GetView(),
                                             boundsRelativeToTarget);
  nsContainerFrame::SyncWindowProperties(mPresContext, target,
                                         target->GetView(), rcx);

  target->DidReflow(mPresContext, nullptr, nsDidReflowStatus::FINISHED);
  if (target == rootFrame && size.height == NS_UNCONSTRAINEDSIZE) {
    mPresContext->SetVisibleArea(boundsRelativeToTarget);
  }

#ifdef DEBUG
  mCurrentReflowRoot = nullptr;
#endif

  NS_ASSERTION(mPresContext->HasPendingInterrupt() ||
               mFramesToDirty.Count() == 0,
               "Why do we need to dirty anything if not interrupted?");

  mIsReflowing = false;
  bool interrupted = mPresContext->HasPendingInterrupt();
  if (interrupted) {
    // Make sure target gets reflowed again.
    mFramesToDirty.EnumerateEntries(&MarkFramesDirtyToRoot, target);
    NS_ASSERTION(NS_SUBTREE_DIRTY(target), "Why is the target not dirty?");
    mDirtyRoots.AppendElement(target);
    mDocument->SetNeedLayoutFlush();

    // Clear mFramesToDirty after we've done the NS_SUBTREE_DIRTY(target)
    // assertion so that if it fails it's easier to see what's going on.
#ifdef NOISY_INTERRUPTIBLE_REFLOW
    printf("mFramesToDirty.Count() == %u\n", mFramesToDirty.Count());
#endif /* NOISY_INTERRUPTIBLE_REFLOW */
    mFramesToDirty.Clear();

    // Any FlushPendingNotifications with interruptible reflows
    // should be suppressed now. We don't want to do extra reflow work
    // before our reflow event happens.
    mSuppressInterruptibleReflows = true;
    MaybeScheduleReflow();
  }

#ifdef PR_LOGGING
  // dump text perf metrics for reflows with significant text processing
  if (tp) {
    if (tp->current.numChars > 100) {
      TimeDuration reflowTime = TimeStamp::Now() - timeStart;
      LogTextPerfStats(tp, this, tp->current,
                       reflowTime.ToMilliseconds(), eLog_reflow, nullptr);
    }
    tp->Accumulate();
  }
#endif

  return !interrupted;
}

#ifdef DEBUG
void
PresShell::DoVerifyReflow()
{
  if (GetVerifyReflowEnable()) {
    // First synchronously render what we have so far so that we can
    // see it.
    nsView* rootView = mViewManager->GetRootView();
    mViewManager->InvalidateView(rootView);

    FlushPendingNotifications(Flush_Layout);
    mInVerifyReflow = true;
    bool ok = VerifyIncrementalReflow();
    mInVerifyReflow = false;
    if (VERIFY_REFLOW_ALL & gVerifyReflowFlags) {
      printf("ProcessReflowCommands: finished (%s)\n",
             ok ? "ok" : "failed");
    }

    if (!mDirtyRoots.IsEmpty()) {
      printf("XXX yikes! reflow commands queued during verify-reflow\n");
    }
  }
}
#endif

// used with Telemetry metrics
#define NS_LONG_REFLOW_TIME_MS    5000

bool
PresShell::ProcessReflowCommands(bool aInterruptible)
{
  if (mDirtyRoots.IsEmpty() && !mShouldUnsuppressPainting) {
    // Nothing to do; bail out
    return true;
  }

  mozilla::TimeStamp timerStart = mozilla::TimeStamp::Now();
  bool interrupted = false;
  if (!mDirtyRoots.IsEmpty()) {

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
      nsViewManager::AutoDisableRefresh refreshBlocker(mViewManager);

      do {
        // Send an incremental reflow notification to the target frame.
        int32_t idx = mDirtyRoots.Length() - 1;
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
      } while (!interrupted && !mDirtyRoots.IsEmpty() &&
               (!aInterruptible || PR_IntervalNow() < deadline));

      interrupted = !mDirtyRoots.IsEmpty();
    }

    // Exiting the scriptblocker might have killed us
    if (!mIsDestroying) {
      DidDoReflow(aInterruptible, interrupted);
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
      if (!mDirtyRoots.IsEmpty()) {
        MaybeScheduleReflow();
        // And tell our document that we might need flushing
        mDocument->SetNeedLayoutFlush();
      }
    }
  }

  if (!mIsDestroying && mShouldUnsuppressPainting &&
      mDirtyRoots.IsEmpty()) {
    // We only unlock if we're out of reflows.  It's pointless
    // to unlock if reflows are still pending, since reflows
    // are just going to thrash the frames around some more.  By
    // waiting we avoid an overeager "jitter" effect.
    mShouldUnsuppressPainting = false;
    UnsuppressAndInvalidate();
  }

  if (mDocument->GetRootElement()) {
    TimeDuration elapsed = TimeStamp::Now() - timerStart;
    int32_t intElapsed = int32_t(elapsed.ToMilliseconds());

    Telemetry::ID id;
    if (mDocument->GetRootElement()->IsXUL()) {
      id = mIsActive
        ? Telemetry::XUL_FOREGROUND_REFLOW_MS
        : Telemetry::XUL_BACKGROUND_REFLOW_MS;
    } else {
      id = mIsActive
        ? Telemetry::HTML_FOREGROUND_REFLOW_MS_2
        : Telemetry::HTML_BACKGROUND_REFLOW_MS_2;
    }
    Telemetry::Accumulate(id, intElapsed);
    if (intElapsed > NS_LONG_REFLOW_TIME_MS) {
      Telemetry::Accumulate(Telemetry::LONG_REFLOW_INTERRUPTIBLE,
                            aInterruptible ? 1 : 0);
    }
  }

  return !interrupted;
}

void
PresShell::WindowSizeMoveDone()
{
  if (mPresContext) {
    EventStateManager::ClearGlobalActiveContent(nullptr);
    ClearMouseCapture(nullptr);
  }
}

#ifdef MOZ_XUL
/*
 * It's better to add stuff to the |DidSetStyleContext| method of the
 * relevant frames than adding it here.  These methods should (ideally,
 * anyway) go away.
 */

// Return value says whether to walk children.
typedef bool (* frameWalkerFn)(nsIFrame *aFrame, void *aClosure);

static bool
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
  nsMenuFrame* menu = do_QueryFrame(aFrame);
  if (menu)
    menu->CloseMenu(true);
  return true;
}

static bool
ReframeImageBoxes(nsIFrame *aFrame, void *aClosure)
{
  nsStyleChangeList *list = static_cast<nsStyleChangeList*>(aClosure);
  if (aFrame->GetType() == nsGkAtoms::imageBoxFrame) {
    list->AppendChange(aFrame, aFrame->GetContent(),
                       NS_STYLE_HINT_FRAMECHANGE);
    return false; // don't walk descendants
  }
  return true; // walk descendants
}

static void
WalkFramesThroughPlaceholders(nsPresContext *aPresContext, nsIFrame *aFrame,
                              frameWalkerFn aFunc, void *aClosure)
{
  bool walkChildren = (*aFunc)(aFrame, aClosure);
  if (!walkChildren)
    return;

  nsIFrame::ChildListIterator lists(aFrame);
  for (; !lists.IsDone(); lists.Next()) {
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      nsIFrame* child = childFrames.get();
      if (!(child->GetStateBits() & NS_FRAME_OUT_OF_FLOW)) {
        // only do frames that are in flow, and recur through the
        // out-of-flows of placeholders.
        WalkFramesThroughPlaceholders(aPresContext,
                                      nsPlaceholderFrame::GetRealFrameFor(child),
                                      aFunc, aClosure);
      }
    }
  }
}
#endif

NS_IMETHODIMP
PresShell::Observe(nsISupports* aSubject,
                   const char* aTopic,
                   const char16_t* aData)
{
#ifdef MOZ_XUL
  if (!nsCRT::strcmp(aTopic, "chrome-flush-skin-caches")) {
    nsIFrame *rootFrame = mFrameConstructor->GetRootFrame();
    // Need to null-check because "chrome-flush-skin-caches" can happen
    // at interesting times during startup.
    if (rootFrame) {
      NS_ASSERTION(mViewManager, "View manager must exist");

      nsWeakFrame weakRoot(rootFrame);
      // Have to make sure that the content notifications are flushed before we
      // start messing with the frame model; otherwise we can get content doubling.
      mDocument->FlushPendingNotifications(Flush_ContentAndNotify);

      if (weakRoot.IsAlive()) {
        WalkFramesThroughPlaceholders(mPresContext, rootFrame,
                                      &ReResolveMenusAndTrees, nullptr);

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
          RestyleManager* restyleManager = mPresContext->RestyleManager();
          restyleManager->ProcessRestyledFrames(changeList);
          restyleManager->FlushOverflowChangedTracker();
          --mChangeNestCount;
        }
      }
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

  if (!nsCRT::strcmp(aTopic, "author-sheet-added") && mStyleSet) {
    AddAuthorSheet(aSubject);
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

  if (!nsCRT::strcmp(aTopic, "author-sheet-removed") && mStyleSet) {
    RemoveSheet(nsStyleSet::eDocSheet, aSubject);
    return NS_OK;
  }

  NS_WARNING("unrecognized topic in PresShell::Observe");
  return NS_ERROR_FAILURE;
}

bool
nsIPresShell::AddRefreshObserverInternal(nsARefreshObserver* aObserver,
                                         mozFlushType aFlushType)
{
  nsPresContext* presContext = GetPresContext();
  return presContext &&
      presContext->RefreshDriver()->AddRefreshObserver(aObserver, aFlushType);
}

/* virtual */ bool
nsIPresShell::AddRefreshObserverExternal(nsARefreshObserver* aObserver,
                                         mozFlushType aFlushType)
{
  return AddRefreshObserverInternal(aObserver, aFlushType);
}

bool
nsIPresShell::RemoveRefreshObserverInternal(nsARefreshObserver* aObserver,
                                            mozFlushType aFlushType)
{
  nsPresContext* presContext = GetPresContext();
  return presContext &&
      presContext->RefreshDriver()->RemoveRefreshObserver(aObserver, aFlushType);
}

/* virtual */ bool
nsIPresShell::RemoveRefreshObserverExternal(nsARefreshObserver* aObserver,
                                            mozFlushType aFlushType)
{
  return RemoveRefreshObserverInternal(aObserver, aFlushType);
}

/* virtual */ bool
nsIPresShell::AddPostRefreshObserver(nsAPostRefreshObserver* aObserver)
{
  nsPresContext* presContext = GetPresContext();
  if (!presContext) {
    return false;
  }
  presContext->RefreshDriver()->AddPostRefreshObserver(aObserver);
  return true;
}

/* virtual */ bool
nsIPresShell::RemovePostRefreshObserver(nsAPostRefreshObserver* aObserver)
{
  nsPresContext* presContext = GetPresContext();
  if (!presContext) {
    return false;
  }
  presContext->RefreshDriver()->RemovePostRefreshObserver(aObserver);
  return true;
}

//------------------------------------------------------
// End of protected and private methods on the PresShell
//------------------------------------------------------

//------------------------------------------------------------------
//-- Delayed event Classes Impls
//------------------------------------------------------------------

PresShell::DelayedInputEvent::DelayedInputEvent() :
  DelayedEvent(),
  mEvent(nullptr)
{
}

PresShell::DelayedInputEvent::~DelayedInputEvent()
{
  delete mEvent;
}

void
PresShell::DelayedInputEvent::Dispatch()
{
  if (!mEvent || !mEvent->widget) {
    return;
  }
  nsCOMPtr<nsIWidget> widget = mEvent->widget;
  nsEventStatus status;
  widget->DispatchEvent(mEvent, status);
}

PresShell::DelayedMouseEvent::DelayedMouseEvent(WidgetMouseEvent* aEvent) :
  DelayedInputEvent()
{
  WidgetMouseEvent* mouseEvent =
    new WidgetMouseEvent(aEvent->mFlags.mIsTrusted,
                         aEvent->message,
                         aEvent->widget,
                         aEvent->reason,
                         aEvent->context);
  mouseEvent->AssignMouseEventData(*aEvent, false);
  mEvent = mouseEvent;
}

PresShell::DelayedKeyEvent::DelayedKeyEvent(WidgetKeyboardEvent* aEvent) :
  DelayedInputEvent()
{
  WidgetKeyboardEvent* keyEvent =
    new WidgetKeyboardEvent(aEvent->mFlags.mIsTrusted,
                            aEvent->message,
                            aEvent->widget);
  keyEvent->AssignKeyEventData(*aEvent, false);
  keyEvent->mFlags.mIsSynthesizedForTests = aEvent->mFlags.mIsSynthesizedForTests;
  mEvent = keyEvent;
}

// Start of DEBUG only code

#ifdef DEBUG

static void
LogVerifyMessage(nsIFrame* k1, nsIFrame* k2, const char* aMsg)
{
  nsAutoString n1, n2;
  if (k1) {
    k1->GetFrameName(n1);
  } else {
    n1.AssignLiteral(MOZ_UTF16("(null)"));
  }

  if (k2) {
    k2->GetFrameName(n2);
  } else {
    n2.AssignLiteral(MOZ_UTF16("(null)"));
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

static bool
CompareTrees(nsPresContext* aFirstPresContext, nsIFrame* aFirstFrame,
             nsPresContext* aSecondPresContext, nsIFrame* aSecondFrame)
{
  if (!aFirstPresContext || !aFirstFrame || !aSecondPresContext || !aSecondFrame)
    return true;
  // XXX Evil hack to reduce false positives; I can't seem to figure
  // out how to flush scrollbar changes correctly
  //if (aFirstFrame->GetType() == nsGkAtoms::scrollbarFrame)
  //  return true;
  bool ok = true;
  nsIFrame::ChildListIterator lists1(aFirstFrame);
  nsIFrame::ChildListIterator lists2(aSecondFrame);
  do {
    const nsFrameList& kids1 = !lists1.IsDone() ? lists1.CurrentList() : nsFrameList();
    const nsFrameList& kids2 = !lists2.IsDone() ? lists2.CurrentList() : nsFrameList();
    int32_t l1 = kids1.GetLength();
    int32_t l2 = kids2.GetLength();;
    if (l1 != l2) {
      ok = false;
      LogVerifyMessage(kids1.FirstChild(), kids2.FirstChild(),
                       "child counts don't match: ");
      printf("%d != %d\n", l1, l2);
      if (0 == (VERIFY_REFLOW_ALL & gVerifyReflowFlags)) {
        break;
      }
    }

    nsIntRect r1, r2;
    nsView* v1, *v2;
    for (nsFrameList::Enumerator e1(kids1), e2(kids2);
         ;
         e1.Next(), e2.Next()) {
      nsIFrame* k1 = e1.get();
      nsIFrame* k2 = e2.get();
      if (((nullptr == k1) && (nullptr != k2)) ||
          ((nullptr != k1) && (nullptr == k2))) {
        ok = false;
        LogVerifyMessage(k1, k2, "child lists are different\n");
        break;
      }
      else if (nullptr != k1) {
        // Verify that the frames are the same size
        if (!k1->GetRect().IsEqualInterior(k2->GetRect())) {
          ok = false;
          LogVerifyMessage(k1, k2, "(frame rects)", k1->GetRect(), k2->GetRect());
        }

        // Make sure either both have views or neither have views; if they
        // do have views, make sure the views are the same size. If the
        // views have widgets, make sure they both do or neither does. If
        // they do, make sure the widgets are the same size.
        v1 = k1->GetView();
        v2 = k2->GetView();
        if (((nullptr == v1) && (nullptr != v2)) ||
            ((nullptr != v1) && (nullptr == v2))) {
          ok = false;
          LogVerifyMessage(k1, k2, "child views are not matched\n");
        }
        else if (nullptr != v1) {
          if (!v1->GetBounds().IsEqualInterior(v2->GetBounds())) {
            LogVerifyMessage(k1, k2, "(view rects)", v1->GetBounds(), v2->GetBounds());
          }

          nsIWidget* w1 = v1->GetWidget();
          nsIWidget* w2 = v2->GetWidget();
          if (((nullptr == w1) && (nullptr != w2)) ||
              ((nullptr != w1) && (nullptr == w2))) {
            ok = false;
            LogVerifyMessage(k1, k2, "child widgets are not matched\n");
          }
          else if (nullptr != w1) {
            w1->GetBounds(r1);
            w2->GetBounds(r2);
            if (!r1.IsEqualEdges(r2)) {
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
          ok = false;
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

    lists1.Next();
    lists2.Next();
    if (lists1.IsDone() != lists2.IsDone() ||
        (!lists1.IsDone() && lists1.CurrentID() != lists2.CurrentID())) {
      if (0 == (VERIFY_REFLOW_ALL & gVerifyReflowFlags)) {
        ok = false;
      }
      LogVerifyMessage(kids1.FirstChild(), kids2.FirstChild(),
                       "child list names are not matched: ");
      fprintf(stdout, "%s != %s\n",
              !lists1.IsDone() ? mozilla::layout::ChildListName(lists1.CurrentID()) : "(null)",
              !lists2.IsDone() ? mozilla::layout::ChildListName(lists2.CurrentID()) : "(null)");
      break;
    }
  } while (ok && !lists1.IsDone());

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
      if (nullptr != tag) {
        NS_RELEASE(tag);
        return aRoot;
      }
    }

    // Try one of the children
    nsIFrame* kid = aRoot->GetFirstPrincipalChild();
    while (nullptr != kid) {
      nsIFrame* result = FindTopFrame(kid);
      if (nullptr != result) {
        return result;
      }
      kid = kid->GetNextSibling();
    }
  }
  return nullptr;
}
#endif


#ifdef DEBUG

nsStyleSet*
PresShell::CloneStyleSet(nsStyleSet* aSet)
{
  nsStyleSet *clone = new nsStyleSet();

  int32_t i, n = aSet->SheetCount(nsStyleSet::eOverrideSheet);
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
  return clone;
}

// After an incremental reflow, we verify the correctness by doing a
// full reflow into a fresh frame tree.
bool
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
  NS_ENSURE_TRUE(cx, false);

  nsDeviceContext *dc = mPresContext->DeviceContext();
  nsresult rv = cx->Init(dc);
  NS_ENSURE_SUCCESS(rv, false);

  // Get our scrolling preference
  nsView* rootView = mViewManager->GetRootView();
  NS_ENSURE_TRUE(rootView->HasWidget(), false);
  nsIWidget* parentWidget = rootView->GetWidget();

  // Create a new view manager.
  nsRefPtr<nsViewManager> vm = new nsViewManager();
  NS_ENSURE_TRUE(vm, false);
  rv = vm->Init(dc);
  NS_ENSURE_SUCCESS(rv, false);

  // Create a child window of the parent that is our "root view/window"
  // Create a view
  nsRect tbounds = mPresContext->GetVisibleArea();
  nsView* view = vm->CreateView(tbounds, nullptr);
  NS_ENSURE_TRUE(view, false);

  //now create the widget for the view
  rv = view->CreateWidgetForParent(parentWidget, nullptr, true);
  NS_ENSURE_SUCCESS(rv, false);

  // Setup hierarchical relationship in view manager
  vm->SetRootView(view);

  // Make the new presentation context the same size as our
  // presentation context.
  nsRect r = mPresContext->GetVisibleArea();
  cx->SetVisibleArea(r);

  // Create a new presentation shell to view the document. Use the
  // exact same style information that this document has.
  nsAutoPtr<nsStyleSet> newSet(CloneStyleSet(mStyleSet));
  nsCOMPtr<nsIPresShell> sh = mDocument->CreateShell(cx, vm, newSet);
  NS_ENSURE_TRUE(sh, false);
  newSet.forget();
  // Note that after we create the shell, we must make sure to destroy it
  sh->SetVerifyReflowEnable(false); // turn off verify reflow while we're reflowing the test frame tree
  vm->SetPresShell(sh);
  {
    nsAutoCauseReflowNotifier crNotifier(this);
    sh->Initialize(r.width, r.height);
  }
  mDocument->BindingManager()->ProcessAttachedQueue();
  sh->FlushPendingNotifications(Flush_Layout);
  sh->SetVerifyReflowEnable(true);  // turn on verify reflow again now that we're done reflowing the test frame tree
  // Force the non-primary presshell to unsuppress; it doesn't want to normally
  // because it thinks it's hidden
  ((PresShell*)sh.get())->mPaintingSuppressed = false;
  if (VERIFY_REFLOW_NOISY & gVerifyReflowFlags) {
     printf("Verification Tree built, comparing...\n");
  }

  // Now that the document has been reflowed, use its frame tree to
  // compare against our frame tree.
  nsIFrame* root1 = mFrameConstructor->GetRootFrame();
  nsIFrame* root2 = sh->GetRootFrame();
  bool ok = CompareTrees(mPresContext, root1, cx, root2);
  if (!ok && (VERIFY_REFLOW_NOISY & gVerifyReflowFlags)) {
    printf("Verify reflow failed, primary tree:\n");
    root1->List(stdout, 0);
    printf("Verification tree:\n");
    root2->List(stdout, 0);
  }

#if 0
  // Sample code for dumping page to png
  // XXX Needs to be made more flexible
  if (!ok) {
    nsString stra;
    static int num = 0;
    stra.AppendLiteral("C:\\mozilla\\mozilla\\debug\\filea");
    stra.AppendInt(num);
    stra.AppendLiteral(".png");
    gfxUtils::WriteAsPNG(sh, stra);
    nsString strb;
    strb.AppendLiteral("C:\\mozilla\\mozilla\\debug\\fileb");
    strb.AppendInt(num);
    strb.AppendLiteral(".png");
    gfxUtils::WriteAsPNG(sh, strb);
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
PresShell::ListStyleContexts(nsIFrame *aRootFrame, FILE *out, int32_t aIndent)
{
  nsStyleContext *sc = aRootFrame->StyleContext();
  if (sc)
    sc->List(out, aIndent);
}

void
PresShell::ListStyleSheets(FILE *out, int32_t aIndent)
{
  int32_t sheetCount = mStyleSet->SheetCount(nsStyleSet::eDocSheet);
  for (int32_t i = 0; i < sheetCount; ++i) {
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
    nsAutoCString uriStr;
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
                      nsRenderingContext* aRenderingContext,
                      nsPresContext* aPresContext,
                      nsIFrame * aFrame,
                      const nsPoint& aOffset,
                      uint32_t aColor)
{
  if (mReflowCountMgr) {
    mReflowCountMgr->PaintCount(aName, aRenderingContext, aPresContext,
                                aFrame, aOffset, aColor);
  }
}

//-------------------------------------------------------------
void
PresShell::SetPaintFrameCount(bool aPaintFrameCounts)
{
  if (mReflowCountMgr) {
    mReflowCountMgr->SetPaintFrameCounts(aPaintFrameCounts);
  }
}

bool
PresShell::IsPaintingFrameCounts()
{
  if (mReflowCountMgr)
    return mReflowCountMgr->IsPaintingFrameCounts();
  return false;
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
void ReflowCounter::DisplayTotals(uint32_t aTotal, const char * aTitle)
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
void ReflowCounter::DisplayHTMLTotals(uint32_t aTotal, const char * aTitle)
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

#define KEY_BUF_SIZE_FOR_PTR  24 // adequate char[] buffer to sprintf a pointer

ReflowCountMgr::ReflowCountMgr()
{
  mCounts = PL_NewHashTable(10, PL_HashString, PL_CompareStrings,
                                PL_CompareValues, nullptr, nullptr);
  mIndiFrameCounts = PL_NewHashTable(10, PL_HashString, PL_CompareStrings,
                                     PL_CompareValues, nullptr, nullptr);
  mCycledOnce              = false;
  mDumpFrameCounts         = false;
  mDumpFrameByFrameCounts  = false;
  mPaintFrameByFrameCounts = false;
}

//------------------------------------------------------------------
ReflowCountMgr::~ReflowCountMgr()
{
  CleanUp();
}

//------------------------------------------------------------------
ReflowCounter * ReflowCountMgr::LookUp(const char * aName)
{
  if (nullptr != mCounts) {
    ReflowCounter * counter = (ReflowCounter *)PL_HashTableLookup(mCounts, aName);
    return counter;
  }
  return nullptr;

}

//------------------------------------------------------------------
void ReflowCountMgr::Add(const char * aName, nsIFrame * aFrame)
{
  NS_ASSERTION(aName != nullptr, "Name shouldn't be null!");

  if (mDumpFrameCounts && nullptr != mCounts) {
    ReflowCounter * counter = (ReflowCounter *)PL_HashTableLookup(mCounts, aName);
    if (counter == nullptr) {
      counter = new ReflowCounter(this);
      char * name = NS_strdup(aName);
      NS_ASSERTION(name != nullptr, "null ptr");
      PL_HashTableAdd(mCounts, name, counter);
    }
    counter->Add();
  }

  if ((mDumpFrameByFrameCounts || mPaintFrameByFrameCounts) &&
      nullptr != mIndiFrameCounts &&
      aFrame != nullptr) {
    char key[KEY_BUF_SIZE_FOR_PTR];
    sprintf(key, "%p", (void*)aFrame);
    IndiReflowCounter * counter = (IndiReflowCounter *)PL_HashTableLookup(mIndiFrameCounts, key);
    if (counter == nullptr) {
      counter = new IndiReflowCounter(this);
      counter->mFrame = aFrame;
      counter->mName.AssignASCII(aName);
      PL_HashTableAdd(mIndiFrameCounts, NS_strdup(key), counter);
    }
    // this eliminates extra counts from super classes
    if (counter != nullptr && counter->mName.EqualsASCII(aName)) {
      counter->mCount++;
      counter->mCounter.Add(1);
    }
  }
}

//------------------------------------------------------------------
void ReflowCountMgr::PaintCount(const char*     aName,
                                nsRenderingContext* aRenderingContext,
                                nsPresContext*  aPresContext,
                                nsIFrame*       aFrame,
                                const nsPoint&  aOffset,
                                uint32_t        aColor)
{
  if (mPaintFrameByFrameCounts &&
      nullptr != mIndiFrameCounts &&
      aFrame != nullptr) {
    char key[KEY_BUF_SIZE_FOR_PTR];
    sprintf(key, "%p", (void*)aFrame);
    IndiReflowCounter * counter =
      (IndiReflowCounter *)PL_HashTableLookup(mIndiFrameCounts, key);
    if (counter != nullptr && counter->mName.EqualsASCII(aName)) {
      aRenderingContext->PushState();
      aRenderingContext->Translate(aOffset);
      nsFont font(eFamily_serif, NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
                  NS_FONT_WEIGHT_NORMAL, NS_FONT_STRETCH_NORMAL, 0,
                  nsPresContext::CSSPixelsToAppUnits(11));

      nsRefPtr<nsFontMetrics> fm;
      aPresContext->DeviceContext()->GetMetricsFor(font,
        // We have one frame, therefore we must have a root...
        aPresContext->GetPresShell()->GetRootFrame()->
          StyleFont()->mLanguage,
        aPresContext->GetUserFontSet(),
        aPresContext->GetTextPerfMetrics(),
        *getter_AddRefs(fm));

      aRenderingContext->SetFont(fm);
      char buf[16];
      sprintf(buf, "%d", counter->mCount);
      nscoord x = 0, y = fm->MaxAscent();
      nscoord width, height = fm->MaxHeight();
      aRenderingContext->SetTextRunRTL(false);
      width = aRenderingContext->GetWidth(buf);

      uint32_t color;
      uint32_t color2;
      if (aColor != 0) {
        color  = aColor;
        color2 = NS_RGB(0,0,0);
      } else {
        uint8_t rc = 0, gc = 0, bc = 0;
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
int ReflowCountMgr::RemoveItems(PLHashEntry *he, int i, void *arg)
{
  char *str = (char *)he->key;
  ReflowCounter * counter = (ReflowCounter *)he->value;
  delete counter;
  NS_Free(str);

  return HT_ENUMERATE_REMOVE;
}

//------------------------------------------------------------------
int ReflowCountMgr::RemoveIndiItems(PLHashEntry *he, int i, void *arg)
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
  if (nullptr != mCounts) {
    PL_HashTableEnumerateEntries(mCounts, RemoveItems, nullptr);
    PL_HashTableDestroy(mCounts);
    mCounts = nullptr;
  }

  if (nullptr != mIndiFrameCounts) {
    PL_HashTableEnumerateEntries(mIndiFrameCounts, RemoveIndiItems, nullptr);
    PL_HashTableDestroy(mIndiFrameCounts);
    mIndiFrameCounts = nullptr;
  }
}

//------------------------------------------------------------------
int ReflowCountMgr::DoSingleTotal(PLHashEntry *he, int i, void *arg)
{
  char *str = (char *)he->key;
  ReflowCounter * counter = (ReflowCounter *)he->value;

  counter->DisplayTotals(str);

  return HT_ENUMERATE_NEXT;
}

//------------------------------------------------------------------
void ReflowCountMgr::DoGrandTotals()
{
  if (nullptr != mCounts) {
    ReflowCounter * gTots = (ReflowCounter *)PL_HashTableLookup(mCounts, kGrandTotalsStr);
    if (gTots == nullptr) {
      gTots = new ReflowCounter(this);
      PL_HashTableAdd(mCounts, NS_strdup(kGrandTotalsStr), gTots);
    } else {
      gTots->ClearTotals();
    }

    printf("\t\t\t\tTotal\n");
    for (uint32_t i=0;i<78;i++) {
      printf("-");
    }
    printf("\n");
    PL_HashTableEnumerateEntries(mCounts, DoSingleTotal, this);
  }
}

static void RecurseIndiTotals(nsPresContext* aPresContext,
                              PLHashTable *   aHT,
                              nsIFrame *      aParentFrame,
                              int32_t         aLevel)
{
  if (aParentFrame == nullptr) {
    return;
  }

  char key[KEY_BUF_SIZE_FOR_PTR];
  sprintf(key, "%p", (void*)aParentFrame);
  IndiReflowCounter * counter = (IndiReflowCounter *)PL_HashTableLookup(aHT, key);
  if (counter) {
    counter->mHasBeenOutput = true;
    char * name = ToNewCString(counter->mName);
    for (int32_t i=0;i<aLevel;i++) printf(" ");
    printf("%s - %p   [%d][", name, (void*)aParentFrame, counter->mCount);
    printf("%d", counter->mCounter.GetTotal());
    printf("]\n");
    nsMemory::Free(name);
  }

  nsIFrame* child = aParentFrame->GetFirstPrincipalChild();
  while (child) {
    RecurseIndiTotals(aPresContext, aHT, child, aLevel+1);
    child = child->GetNextSibling();
  }

}

//------------------------------------------------------------------
int ReflowCountMgr::DoSingleIndi(PLHashEntry *he, int i, void *arg)
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
  if (nullptr != mCounts) {
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
int ReflowCountMgr::DoSingleHTMLTotal(PLHashEntry *he, int i, void *arg)
{
  char *str = (char *)he->key;
  ReflowCounter * counter = (ReflowCounter *)he->value;

  counter->DisplayHTMLTotals(str);

  return HT_ENUMERATE_NEXT;
}

//------------------------------------------------------------------
void ReflowCountMgr::DoGrandHTMLTotals()
{
  if (nullptr != mCounts) {
    ReflowCounter * gTots = (ReflowCounter *)PL_HashTableLookup(mCounts, kGrandTotalsStr);
    if (gTots == nullptr) {
      gTots = new ReflowCounter(this);
      PL_HashTableAdd(mCounts, NS_strdup(kGrandTotalsStr), gTots);
    } else {
      gTots->ClearTotals();
    }

    static const char * title[] = {"Class", "Reflows"};
    fprintf(mFD, "<tr>");
    for (uint32_t i=0; i < ArrayLength(title); i++) {
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
    mFD = nullptr;
  }
#endif // not XP!
}

//------------------------------------------------------------------
int ReflowCountMgr::DoClearTotals(PLHashEntry *he, int i, void *arg)
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
  if (nullptr != mCounts) {
    ReflowCounter * gTots = (ReflowCounter *)PL_HashTableLookup(mCounts, kGrandTotalsStr);
    if (gTots == nullptr) {
      gTots = new ReflowCounter(this);
      PL_HashTableAdd(mCounts, NS_strdup(kGrandTotalsStr), gTots);
    } else {
      gTots->ClearTotals();
      gTots->SetTotalsCache();
    }
  }
}

//------------------------------------------------------------------
int ReflowCountMgr::DoDisplayDiffTotals(PLHashEntry *he, int i, void *arg)
{
  bool cycledOnce = (arg != 0);

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
    for (int32_t i=0;i<78;i++) {
      printf("-");
    }
    printf("\n");
    ClearGrandTotals();
  }
  PL_HashTableEnumerateEntries(mCounts, DoDisplayDiffTotals, (void *)mCycledOnce);

  mCycledOnce = true;
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
  return FrameConstructor()->GetAbsoluteContainingBlock(aFrame,
      nsCSSFrameConstructor::ABS_POS);
}

#ifdef ACCESSIBILITY
bool
nsIPresShell::IsAccessibilityActive()
{
  return GetAccService() != nullptr;
}

nsAccessibilityService*
nsIPresShell::AccService()
{
  return GetAccService();
}
#endif

void nsIPresShell::InitializeStatics()
{
  NS_ASSERTION(!gCaptureTouchList, "InitializeStatics called multiple times!");
  gCaptureTouchList = new nsRefPtrHashtable<nsUint32HashKey, dom::Touch>;
  gPointerCaptureList = new nsRefPtrHashtable<nsUint32HashKey, nsIContent>;
  gActivePointersIds = new nsClassHashtable<nsUint32HashKey, PointerInfo>;
}

void nsIPresShell::ReleaseStatics()
{
  NS_ASSERTION(gCaptureTouchList, "ReleaseStatics called without Initialize!");
  delete gCaptureTouchList;
  gCaptureTouchList = nullptr;
  delete gPointerCaptureList;
  gPointerCaptureList = nullptr;
  delete gActivePointersIds;
  gActivePointersIds = nullptr;
}

// Asks our docshell whether we're active.
void PresShell::QueryIsActive()
{
  nsCOMPtr<nsISupports> container = mPresContext->GetContainerWeak();
  if (mDocument) {
    nsIDocument* displayDoc = mDocument->GetDisplayDocument();
    if (displayDoc) {
      // Ok, we're an external resource document -- we need to use our display
      // document's docshell to determine "IsActive" status, since we lack
      // a container.
      NS_ABORT_IF_FALSE(!container,
                        "external resource doc shouldn't have "
                        "its own container");

      nsIPresShell* displayPresShell = displayDoc->GetShell();
      if (displayPresShell) {
        container = displayPresShell->GetPresContext()->GetContainerWeak();
      }
    }
  }

  nsCOMPtr<nsIDocShell> docshell(do_QueryInterface(container));
  if (docshell) {
    bool isActive;
    nsresult rv = docshell->GetIsActive(&isActive);
    if (NS_SUCCEEDED(rv))
      SetIsActive(isActive);
  }
}

// Helper for propagating mIsActive changes to external resources
static bool
SetExternalResourceIsActive(nsIDocument* aDocument, void* aClosure)
{
  nsIPresShell* shell = aDocument->GetShell();
  if (shell) {
    shell->SetIsActive(*static_cast<bool*>(aClosure));
  }
  return true;
}

static void
SetPluginIsActive(nsISupports* aSupports, void* aClosure)
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(aSupports));
  if (!content) {
    return;
  }

  nsIFrame *frame = content->GetPrimaryFrame();
  nsIObjectFrame *objectFrame = do_QueryFrame(frame);
  if (objectFrame) {
    objectFrame->SetIsDocumentActive(*static_cast<bool*>(aClosure));
  }
}

nsresult
PresShell::SetIsActive(bool aIsActive)
{
  NS_PRECONDITION(mDocument, "should only be called with a document");

  mIsActive = aIsActive;
  nsPresContext* presContext = GetPresContext();
  if (presContext &&
      presContext->RefreshDriver()->PresContext() == presContext) {
    presContext->RefreshDriver()->SetThrottled(!mIsActive);
  }

  // Propagate state-change to my resource documents' PresShells
  mDocument->EnumerateExternalResources(SetExternalResourceIsActive,
                                        &aIsActive);
  mDocument->EnumerateActivityObservers(SetPluginIsActive,
                                        &aIsActive);
  nsresult rv = UpdateImageLockingState();
#ifdef ACCESSIBILITY
  if (aIsActive) {
    nsAccessibilityService* accService = AccService();
    if (accService) {
      accService->PresShellActivated(this);
    }
  }
#endif

  // We have this odd special case here because remote content behaves
  // differently from same-process content when "hidden".  In
  // desktop-type "browser UIs", hidden "tabs" have documents that are
  // part of the chrome tree.  When the tabs are hidden, their content
  // is no longer part of the visible document tree, and the layers
  // for the content are naturally released.
  //
  // Remote content is its own top-level tree in its subprocess.  When
  // it's "hidden", there's no transaction in which the document
  // thinks it's not visible, so layers can be retained forever.  This
  // is problematic when those layers uselessly hold on to precious
  // resources like directly texturable memory.
  //
  // PresShell::SetIsActive() is the first C++ entry point at which we
  // (i) know that our parent process wants our content to be hidden;
  // and (ii) has easy access to the TabChild.  So we use this
  // notification to signal the TabChild to drop its layer tree and
  // stop trying to repaint.
  if (TabChild* tab = TabChild::GetFrom(this)) {
    if (aIsActive) {
      tab->MakeVisible();
      if (!mIsZombie) {
        if (nsIFrame* root = mFrameConstructor->GetRootFrame()) {
          FrameLayerBuilder::InvalidateAllLayersForFrame(
            nsLayoutUtils::GetDisplayRootFrame(root));
          root->SchedulePaint();
        }
      }
    } else {
      tab->MakeHidden();
    }
  }

  return rv;
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

PresShell*
PresShell::GetRootPresShell()
{
  if (mPresContext) {
    nsPresContext* rootPresContext = mPresContext->GetRootPresContext();
    if (rootPresContext) {
      return static_cast<PresShell*>(rootPresContext->PresShell());
    }
  }
  return nullptr;
}

void
PresShell::AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf,
                                  nsArenaMemoryStats *aArenaObjectsSize,
                                  size_t *aPresShellSize,
                                  size_t *aStyleSetsSize,
                                  size_t *aTextRunsSize,
                                  size_t *aPresContextSize)
{
  mFrameArena.AddSizeOfExcludingThis(aMallocSizeOf, aArenaObjectsSize);
  *aPresShellSize += aMallocSizeOf(this);
  if (mCaret) {
    *aPresShellSize += mCaret->SizeOfIncludingThis(aMallocSizeOf);
  }
  *aPresShellSize += mVisibleImages.SizeOfExcludingThis(nullptr,
                                                        aMallocSizeOf,
                                                        nullptr);
  *aPresShellSize += mFramesToDirty.SizeOfExcludingThis(nullptr,
                                                        aMallocSizeOf,
                                                        nullptr);
  *aPresShellSize += aArenaObjectsSize->mOther;

  *aStyleSetsSize += StyleSet()->SizeOfIncludingThis(aMallocSizeOf);

  *aTextRunsSize += SizeOfTextRuns(aMallocSizeOf);

  *aPresContextSize += mPresContext->SizeOfIncludingThis(aMallocSizeOf);
}

size_t
PresShell::SizeOfTextRuns(MallocSizeOf aMallocSizeOf) const
{
  nsIFrame* rootFrame = mFrameConstructor->GetRootFrame();
  if (!rootFrame) {
    return 0;
  }

  // clear the TEXT_RUN_MEMORY_ACCOUNTED flags
  nsLayoutUtils::SizeOfTextRunsForFrames(rootFrame, nullptr,
                                         /* clear = */true);

  // collect the total memory in use for textruns
  return nsLayoutUtils::SizeOfTextRunsForFrames(rootFrame, aMallocSizeOf,
                                                /* clear = */false);
}

void
nsIPresShell::MarkFixedFramesForReflow(IntrinsicDirty aIntrinsicDirty)
{
  nsIFrame* rootFrame = mFrameConstructor->GetRootFrame();
  if (rootFrame) {
    const nsFrameList& childList = rootFrame->GetChildList(nsIFrame::kFixedList);
    for (nsFrameList::Enumerator e(childList); !e.AtEnd(); e.Next()) {
      FrameNeedsReflow(e.get(), aIntrinsicDirty, NS_FRAME_IS_DIRTY);
    }
  }
}

void
nsIPresShell::SetScrollPositionClampingScrollPortSize(nscoord aWidth, nscoord aHeight)
{
  if (!mScrollPositionClampingScrollPortSizeSet ||
      mScrollPositionClampingScrollPortSize.width != aWidth ||
      mScrollPositionClampingScrollPortSize.height != aHeight) {
    mScrollPositionClampingScrollPortSizeSet = true;
    mScrollPositionClampingScrollPortSize.width = aWidth;
    mScrollPositionClampingScrollPortSize.height = aHeight;

    MarkFixedFramesForReflow(eResize);
  }
}

void
nsIPresShell::SetContentDocumentFixedPositionMargins(const nsMargin& aMargins)
{
  if (mContentDocumentFixedPositionMargins == aMargins) {
    return;
  }

  mContentDocumentFixedPositionMargins = aMargins;

  MarkFixedFramesForReflow(eResize);
}

void
PresShell::SetupFontInflation()
{
  mFontSizeInflationEmPerLine = nsLayoutUtils::FontSizeInflationEmPerLine();
  mFontSizeInflationMinTwips = nsLayoutUtils::FontSizeInflationMinTwips();
  mFontSizeInflationLineThreshold = nsLayoutUtils::FontSizeInflationLineThreshold();
  mFontSizeInflationForceEnabled = nsLayoutUtils::FontSizeInflationForceEnabled();
  mFontSizeInflationDisabledInMasterProcess = nsLayoutUtils::FontSizeInflationDisabledInMasterProcess();

  NotifyFontSizeInflationEnabledIsDirty();
}

void
nsIPresShell::RecomputeFontSizeInflationEnabled()
{
  mFontSizeInflationEnabledIsDirty = false;

  MOZ_ASSERT(mPresContext, "our pres context should not be null");
  if ((FontSizeInflationEmPerLine() == 0 &&
      FontSizeInflationMinTwips() == 0) || mPresContext->IsChrome()) {
    mFontSizeInflationEnabled = false;
    return;
  }

  // Force-enabling font inflation always trumps the heuristics here.
  if (!FontSizeInflationForceEnabled()) {
    if (TabChild* tab = TabChild::GetFrom(this)) {
      // We're in a child process.  Cancel inflation if we're not
      // async-pan zoomed.
      if (!tab->IsAsyncPanZoomEnabled()) {
        mFontSizeInflationEnabled = false;
        return;
      }
    } else if (XRE_GetProcessType() == GeckoProcessType_Default) {
      // We're in the master process.  Cancel inflation if it's been
      // explicitly disabled.
      if (FontSizeInflationDisabledInMasterProcess()) {
        mFontSizeInflationEnabled = false;
        return;
      }
    }
  }

  // XXXjwir3:
  // See bug 706918, comment 23 for more information on this particular section
  // of the code. We're using "screen size" in place of the size of the content
  // area, because on mobile, these are close or equal. This will work for our
  // purposes (bug 706198), but it will need to be changed in the future to be
  // more correct when we bring the rest of the viewport code into platform.
  // We actually want the size of the content area, in the event that we don't
  // have any metadata about the width and/or height. On mobile, the screen size
  // and the size of the content area are very close, or the same value.
  // In XUL fennec, the content area is the size of the <browser> widget, but
  // in native fennec, the content area is the size of the Gecko LayerView
  // object.

  // TODO:
  // Once bug 716575 has been resolved, this code should be changed so that it
  // does the right thing on all platforms.
  nsresult rv;
  nsCOMPtr<nsIScreenManager> screenMgr =
    do_GetService("@mozilla.org/gfx/screenmanager;1", &rv);
  if (!NS_SUCCEEDED(rv)) {
    mFontSizeInflationEnabled = false;
    return;
  }

  nsCOMPtr<nsIScreen> screen;
  screenMgr->GetPrimaryScreen(getter_AddRefs(screen));
  if (screen) {
    int32_t screenLeft, screenTop, screenWidth, screenHeight;
    screen->GetRect(&screenLeft, &screenTop, &screenWidth, &screenHeight);

    nsViewportInfo vInf =
      nsContentUtils::GetViewportInfo(GetDocument(), ScreenIntSize(screenWidth, screenHeight));

    if (vInf.GetDefaultZoom() >= CSSToScreenScale(1.0f) || vInf.IsAutoSizeEnabled()) {
      mFontSizeInflationEnabled = false;
      return;
    }
  }

  mFontSizeInflationEnabled = true;
}

bool
nsIPresShell::FontSizeInflationEnabled()
{
  if (mFontSizeInflationEnabledIsDirty) {
    RecomputeFontSizeInflationEnabled();
  }

  return mFontSizeInflationEnabled;
}

void
nsIPresShell::SetMaxLineBoxWidth(nscoord aMaxLineBoxWidth)
{
  NS_ASSERTION(aMaxLineBoxWidth >= 0, "attempting to set max line box width to a negative value");

  if (mMaxLineBoxWidth != aMaxLineBoxWidth) {
    mMaxLineBoxWidth = aMaxLineBoxWidth;
    mReflowOnZoomPending = true;
    FrameNeedsReflow(GetRootFrame(), eResize, NS_FRAME_HAS_DIRTY_CHILDREN);
  }
}

void
PresShell::PausePainting()
{
  if (GetPresContext()->RefreshDriver()->PresContext() != GetPresContext())
    return;

  mPaintingIsFrozen = true;
  GetPresContext()->RefreshDriver()->Freeze();
}

void
PresShell::ResumePainting()
{
  if (GetPresContext()->RefreshDriver()->PresContext() != GetPresContext())
    return;

  mPaintingIsFrozen = false;
  GetPresContext()->RefreshDriver()->Thaw();
}

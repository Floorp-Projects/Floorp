/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* a presentation of a document, part 2 */

#include "mozilla/PresShell.h"

#include "Units.h"
#include "mozilla/dom/FontFaceSet.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/ContentIterator.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/EventStates.h"
#include "mozilla/GeckoMVMContext.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/Likely.h"
#include "mozilla/Logging.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/PerfStats.h"
#include "mozilla/PresShellInlines.h"
#include "mozilla/RangeUtils.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StaticPrefs_apz.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPrefs_font.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/ViewportUtils.h"
#include <algorithm>

#ifdef XP_WIN
#  include "winuser.h"
#endif

#include "gfxContext.h"
#include "gfxUserFontSet.h"
#include "nsContentList.h"
#include "nsPresContext.h"
#include "nsIContent.h"
#include "mozilla/dom/BrowserBridgeChild.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/PointerEventHandler.h"
#include "mozilla/dom/PopupBlocker.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/UserActivation.h"
#include "nsAnimationManager.h"
#include "nsNameSpaceManager.h"  // for Pref-related rule management (bugs 22963,20760,31816)
#include "nsFlexContainerFrame.h"
#include "nsFrame.h"
#include "FrameLayerBuilder.h"
#include "nsViewManager.h"
#include "nsView.h"
#include "nsCRTGlue.h"
#include "prinrval.h"
#include "nsTArray.h"
#include "nsCOMArray.h"
#include "nsContainerFrame.h"
#include "mozilla/dom/Selection.h"
#include "nsGkAtoms.h"
#include "nsRange.h"
#include "nsWindowSizes.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsPageSequenceFrame.h"
#include "nsCaret.h"
#include "mozilla/AccessibleCaretEventHub.h"
#include "nsFrameManager.h"
#include "nsXPCOM.h"
#include "nsILayoutHistoryState.h"
#include "nsILineIterator.h"  // for ScrollContentIntoView
#include "PLDHashTable.h"
#include "mozilla/dom/Touch.h"
#include "mozilla/dom/TouchEvent.h"
#include "mozilla/dom/PointerEventBinding.h"
#include "mozilla/dom/ShadowIncludingTreeIterator.h"
#include "nsIObserverService.h"
#include "nsDocShell.h"  // for reflow observation
#include "nsIBaseWindow.h"
#include "nsError.h"
#include "nsLayoutUtils.h"
#include "nsViewportInfo.h"
#include "nsCSSRendering.h"
// for |#ifdef DEBUG| code
#include "prenv.h"
#include "nsDisplayList.h"
#include "nsRegion.h"
#include "nsAutoLayoutPhase.h"
#ifdef MOZ_GECKO_PROFILER
#  include "AutoProfilerStyleMarker.h"
#endif
#ifdef MOZ_REFLOW_PERF
#  include "nsFontMetrics.h"
#endif
#include "MobileViewportManager.h"
#include "OverflowChangedTracker.h"
#include "PositionedEventTargeting.h"

#include "nsIReflowCallback.h"

#include "nsPIDOMWindow.h"
#include "nsFocusManager.h"
#include "nsIObjectFrame.h"
#include "nsIObjectLoadingContent.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "nsStyleSheetService.h"
#include "gfxUtils.h"
#include "mozilla/SMILAnimationController.h"
#include "SVGContentUtils.h"
#include "SVGObserverUtils.h"
#include "SVGFragmentIdentifier.h"
#include "nsFrameSelection.h"

#include "mozilla/dom/Performance.h"
#include "nsRefreshDriver.h"
#include "nsDOMNavigationTiming.h"

// Drag & Drop, Clipboard
#include "nsIDocShellTreeItem.h"
#include "nsIURI.h"
#include "nsIScrollableFrame.h"
#include "nsITimer.h"
#ifdef ACCESSIBILITY
#  include "nsAccessibilityService.h"
#  include "mozilla/a11y/DocAccessible.h"
#  ifdef DEBUG
#    include "mozilla/a11y/Logging.h"
#  endif
#endif

// For style data reconstruction
#include "nsStyleChangeList.h"
#include "nsCSSFrameConstructor.h"
#ifdef MOZ_XUL
#  include "nsMenuFrame.h"
#  include "nsTreeBodyFrame.h"
#  include "XULTreeElement.h"
#  include "nsMenuPopupFrame.h"
#  include "nsTreeColumns.h"
#  include "nsIDOMXULMultSelectCntrlEl.h"
#  include "nsIDOMXULSelectCntrlItemEl.h"
#  include "nsIDOMXULMenuListElement.h"
#  include "nsXULElement.h"
#endif  // MOZ_XUL

#include "mozilla/layers/CompositorBridgeChild.h"
#include "ClientLayerManager.h"
#include "GeckoProfiler.h"
#include "gfxPlatform.h"
#include "Layers.h"
#include "LayerTreeInvalidation.h"
#include "mozilla/css/ImageLoader.h"
#include "mozilla/dom/DocumentTimeline.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"
#include "nsCanvasFrame.h"
#include "nsImageFrame.h"
#include "nsIScreen.h"
#include "nsIScreenManager.h"
#include "nsPlaceholderFrame.h"
#include "nsTransitionManager.h"
#include "ChildIterator.h"
#include "mozilla/RestyleManager.h"
#include "nsIDragSession.h"
#include "nsIFrameInlines.h"
#include "mozilla/gfx/2D.h"
#include "nsNetUtil.h"
#include "nsSubDocumentFrame.h"
#include "nsQueryObject.h"
#include "mozilla/GlobalStyleSheetCache.h"
#include "mozilla/layers/InputAPZContext.h"
#include "mozilla/layers/FocusTarget.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/layers/WebRenderUserData.h"
#include "mozilla/layout/ScrollAnchorContainer.h"
#include "mozilla/ScrollTypes.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/dom/ImageTracker.h"
#include "nsIDocShellTreeOwner.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "VisualViewport.h"
#include "ZoomConstraintsClient.h"

#ifdef MOZ_TASK_TRACER
#  include "GeckoTaskTracer.h"
using namespace mozilla::tasktracer;
#endif

// define the scalfactor of drag and drop images
// relative to the max screen height/width
#define RELATIVE_SCALEFACTOR 0.0925f

using namespace mozilla;
using namespace mozilla::css;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::layers;
using namespace mozilla::gfx;
using namespace mozilla::layout;
using PaintFrameFlags = nsLayoutUtils::PaintFrameFlags;
typedef ScrollableLayerGuid::ViewID ViewID;

PresShell::CapturingContentInfo PresShell::sCapturingContentInfo;

// RangePaintInfo is used to paint ranges to offscreen buffers
struct RangePaintInfo {
  RefPtr<nsRange> mRange;
  nsDisplayListBuilder mBuilder;
  nsDisplayList mList;

  // offset of builder's reference frame to the root frame
  nsPoint mRootOffset;

  RangePaintInfo(nsRange* aRange, nsIFrame* aFrame)
      : mRange(aRange),
        mBuilder(aFrame, nsDisplayListBuilderMode::Painting, false) {
    MOZ_COUNT_CTOR(RangePaintInfo);
    mBuilder.BeginFrame();
  }

  ~RangePaintInfo() {
    mList.DeleteAll(&mBuilder);
    mBuilder.EndFrame();
    MOZ_COUNT_DTOR(RangePaintInfo);
  }
};

#undef NOISY

// ----------------------------------------------------------------------

#ifdef DEBUG
// Set the environment variable GECKO_VERIFY_REFLOW_FLAGS to one or
// more of the following flags (comma separated) for handy debug
// output.
static VerifyReflowFlags gVerifyReflowFlags;

struct VerifyReflowFlagData {
  const char* name;
  VerifyReflowFlags bit;
};

static const VerifyReflowFlagData gFlags[] = {
    // clang-format off
  { "verify",                VerifyReflowFlags::On },
  { "reflow",                VerifyReflowFlags::Noisy },
  { "all",                   VerifyReflowFlags::All },
  { "list-commands",         VerifyReflowFlags::DumpCommands },
  { "noisy-commands",        VerifyReflowFlags::NoisyCommands },
  { "really-noisy-commands", VerifyReflowFlags::ReallyNoisyCommands },
  { "resize",                VerifyReflowFlags::DuringResizeReflow },
    // clang-format on
};

#  define NUM_VERIFY_REFLOW_FLAGS (sizeof(gFlags) / sizeof(gFlags[0]))

static void ShowVerifyReflowFlags() {
  printf("Here are the available GECKO_VERIFY_REFLOW_FLAGS:\n");
  const VerifyReflowFlagData* flag = gFlags;
  const VerifyReflowFlagData* limit = gFlags + NUM_VERIFY_REFLOW_FLAGS;
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
  explicit ReflowCounter(ReflowCountMgr* aMgr = nullptr);
  ~ReflowCounter();

  void ClearTotals();
  void DisplayTotals(const char* aStr);
  void DisplayDiffTotals(const char* aStr);
  void DisplayHTMLTotals(const char* aStr);

  void Add() { mTotal++; }
  void Add(uint32_t aTotal) { mTotal += aTotal; }

  void CalcDiffInTotals();
  void SetTotalsCache();

  void SetMgr(ReflowCountMgr* aMgr) { mMgr = aMgr; }

  uint32_t GetTotal() { return mTotal; }

 protected:
  void DisplayTotals(uint32_t aTotal, const char* aTitle);
  void DisplayHTMLTotals(uint32_t aTotal, const char* aTitle);

  uint32_t mTotal;
  uint32_t mCacheTotal;

  ReflowCountMgr* mMgr;  // weak reference (don't delete)
};

// Counting Class
class IndiReflowCounter {
 public:
  explicit IndiReflowCounter(ReflowCountMgr* aMgr = nullptr)
      : mFrame(nullptr),
        mCount(0),
        mMgr(aMgr),
        mCounter(aMgr),
        mHasBeenOutput(false) {}
  virtual ~IndiReflowCounter() = default;

  nsAutoString mName;
  nsIFrame* mFrame;  // weak reference (don't delete)
  int32_t mCount;

  ReflowCountMgr* mMgr;  // weak reference (don't delete)

  ReflowCounter mCounter;
  bool mHasBeenOutput;
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
  void DisplayTotals(const char* aStr);
  void DisplayHTMLTotals(const char* aStr);
  void DisplayDiffsInTotals();

  void Add(const char* aName, nsIFrame* aFrame);
  ReflowCounter* LookUp(const char* aName);

  void PaintCount(const char* aName, gfxContext* aRenderingContext,
                  nsPresContext* aPresContext, nsIFrame* aFrame,
                  const nsPoint& aOffset, uint32_t aColor);

  FILE* GetOutFile() { return mFD; }

  void SetPresContext(nsPresContext* aPresContext) {
    mPresContext = aPresContext;  // weak reference
  }
  void SetPresShell(PresShell* aPresShell) {
    mPresShell = aPresShell;  // weak reference
  }

  void SetDumpFrameCounts(bool aVal) { mDumpFrameCounts = aVal; }
  void SetDumpFrameByFrameCounts(bool aVal) { mDumpFrameByFrameCounts = aVal; }
  void SetPaintFrameCounts(bool aVal) { mPaintFrameByFrameCounts = aVal; }

  bool IsPaintingFrameCounts() { return mPaintFrameByFrameCounts; }

 protected:
  void DisplayTotals(uint32_t aTotal, uint32_t* aDupArray, char* aTitle);
  void DisplayHTMLTotals(uint32_t aTotal, uint32_t* aDupArray, char* aTitle);

  void DoGrandTotals();
  void DoIndiTotalsTree();

  // HTML Output Methods
  void DoGrandHTMLTotals();

  nsClassHashtable<nsCharPtrHashKey, ReflowCounter> mCounts;
  nsClassHashtable<nsCharPtrHashKey, IndiReflowCounter> mIndiFrameCounts;
  FILE* mFD;

  bool mDumpFrameCounts;
  bool mDumpFrameByFrameCounts;
  bool mPaintFrameByFrameCounts;

  bool mCycledOnce;

  // Root Frame for Individual Tracking
  nsPresContext* mPresContext;
  PresShell* mPresShell;

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
#define NS_MAX_REFLOW_TIME 1000000
static int32_t gMaxRCProcessingTime = -1;

struct nsCallbackEventRequest {
  nsIReflowCallback* callback;
  nsCallbackEventRequest* next;
};

// ----------------------------------------------------------------------------
//
// NOTE(emilio): It'd be nice for this to assert that our document isn't in the
// bfcache, but font pref changes don't care about that, and maybe / probably
// shouldn't.
#ifdef DEBUG
#  define ASSERT_REFLOW_SCHEDULED_STATE()                                   \
    {                                                                       \
      if (ObservingLayoutFlushes()) {                                       \
        MOZ_ASSERT(                                                         \
            mDocument->GetBFCacheEntry() ||                                 \
                mPresContext->RefreshDriver()->IsLayoutFlushObserver(this), \
            "Unexpected state");                                            \
      } else {                                                              \
        MOZ_ASSERT(                                                         \
            !mPresContext->RefreshDriver()->IsLayoutFlushObserver(this),    \
            "Unexpected state");                                            \
      }                                                                     \
    }
#else
#  define ASSERT_REFLOW_SCHEDULED_STATE() /* nothing */
#endif

class nsAutoCauseReflowNotifier {
 public:
  MOZ_CAN_RUN_SCRIPT explicit nsAutoCauseReflowNotifier(PresShell* aPresShell)
      : mPresShell(aPresShell) {
    mPresShell->WillCauseReflow();
  }
  MOZ_CAN_RUN_SCRIPT ~nsAutoCauseReflowNotifier() {
    // This check should not be needed. Currently the only place that seem
    // to need it is the code that deals with bug 337586.
    if (!mPresShell->mHaveShutDown) {
      RefPtr<PresShell> presShell(mPresShell);
      presShell->DidCauseReflow();
    } else {
      nsContentUtils::RemoveScriptBlocker();
    }
  }

  PresShell* mPresShell;
};

class MOZ_STACK_CLASS nsPresShellEventCB : public EventDispatchingCallback {
 public:
  explicit nsPresShellEventCB(PresShell* aPresShell) : mPresShell(aPresShell) {}

  MOZ_CAN_RUN_SCRIPT
  virtual void HandleEvent(EventChainPostVisitor& aVisitor) override {
    if (aVisitor.mPresContext && aVisitor.mEvent->mClass != eBasicEventClass) {
      if (aVisitor.mEvent->mMessage == eMouseDown ||
          aVisitor.mEvent->mMessage == eMouseUp) {
        // Mouse-up and mouse-down events call nsFrame::HandlePress/Release
        // which call GetContentOffsetsFromPoint which requires up-to-date
        // layout. Bring layout up-to-date now so that GetCurrentEventFrame()
        // below will return a real frame and we don't have to worry about
        // destroying it by flushing later.
        MOZ_KnownLive(mPresShell)->FlushPendingNotifications(FlushType::Layout);
      } else if (aVisitor.mEvent->mMessage == eWheel &&
                 aVisitor.mEventStatus != nsEventStatus_eConsumeNoDefault) {
        nsIFrame* frame = mPresShell->GetCurrentEventFrame();
        if (frame) {
          // chrome (including addons) should be able to know if content
          // handles both D3E "wheel" event and legacy mouse scroll events.
          // We should dispatch legacy mouse events before dispatching the
          // "wheel" event into system group.
          RefPtr<EventStateManager> esm =
              aVisitor.mPresContext->EventStateManager();
          esm->DispatchLegacyMouseScrollEvents(
              frame, aVisitor.mEvent->AsWheelEvent(), &aVisitor.mEventStatus);
        }
      }
      nsIFrame* frame = mPresShell->GetCurrentEventFrame();
      if (!frame && (aVisitor.mEvent->mMessage == eMouseUp ||
                     aVisitor.mEvent->mMessage == eTouchEnd)) {
        // Redirect BUTTON_UP and TOUCH_END events to the root frame to ensure
        // that capturing is released.
        frame = mPresShell->GetRootFrame();
      }
      if (frame) {
        frame->HandleEvent(MOZ_KnownLive(aVisitor.mPresContext),
                           aVisitor.mEvent->AsGUIEvent(),
                           &aVisitor.mEventStatus);
      }
    }
  }

  RefPtr<PresShell> mPresShell;
};

class nsBeforeFirstPaintDispatcher : public Runnable {
 public:
  explicit nsBeforeFirstPaintDispatcher(Document* aDocument)
      : mozilla::Runnable("nsBeforeFirstPaintDispatcher"),
        mDocument(aDocument) {}

  // Fires the "before-first-paint" event so that interested parties (right now,
  // the mobile browser) are aware of it.
  NS_IMETHOD Run() override {
    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (observerService) {
      observerService->NotifyObservers(ToSupports(mDocument),
                                       "before-first-paint", nullptr);
    }
    return NS_OK;
  }

 private:
  RefPtr<Document> mDocument;
};

// This is a helper class to track whether the targeted frame is destroyed after
// dispatching pointer events. In that case, we need the original targeted
// content so that we can dispatch the mouse events to it.
class MOZ_STACK_CLASS AutoPointerEventTargetUpdater final {
 public:
  AutoPointerEventTargetUpdater(PresShell* aShell, WidgetEvent* aEvent,
                                nsIFrame* aFrame, nsIContent** aTargetContent) {
    MOZ_ASSERT(aEvent);
    if (!aTargetContent || aEvent->mClass != ePointerEventClass) {
      // Make the destructor happy.
      mTargetContent = nullptr;
      return;
    }
    MOZ_ASSERT(aShell);
    MOZ_ASSERT(aFrame);
    MOZ_ASSERT(!aFrame->GetContent() ||
               aShell->GetDocument() == aFrame->GetContent()->OwnerDoc());

    MOZ_ASSERT(StaticPrefs::dom_w3c_pointer_events_enabled());
    mShell = aShell;
    mWeakFrame = aFrame;
    mTargetContent = aTargetContent;
    aShell->mPointerEventTarget = aFrame->GetContent();
  }

  ~AutoPointerEventTargetUpdater() {
    if (!mTargetContent || !mShell || mWeakFrame.IsAlive()) {
      return;
    }
    mShell->mPointerEventTarget.swap(*mTargetContent);
  }

 private:
  RefPtr<PresShell> mShell;
  AutoWeakFrame mWeakFrame;
  nsIContent** mTargetContent;
};

void PresShell::DirtyRootsList::Add(nsIFrame* aFrame) {
  // Is this root already scheduled for reflow?
  // FIXME: This could possibly be changed to a uniqueness assertion, with some
  // work in ResizeReflowIgnoreOverride (and maybe others?)
  if (mList.Contains(aFrame)) {
    // We don't expect frame to change depths.
    MOZ_ASSERT(aFrame->GetDepthInFrameTree() ==
               mList[mList.IndexOf(aFrame)].mDepth);
    return;
  }

  mList.InsertElementSorted(
      FrameAndDepth{aFrame, aFrame->GetDepthInFrameTree()},
      FrameAndDepth::CompareByReverseDepth{});
}

void PresShell::DirtyRootsList::Remove(nsIFrame* aFrame) {
  mList.RemoveElement(aFrame);
}

nsIFrame* PresShell::DirtyRootsList::PopShallowestRoot() {
  // List is sorted in order of decreasing depth, so there are no deeper
  // frames than the last one.
  const FrameAndDepth& lastFAD = mList.LastElement();
  nsIFrame* frame = lastFAD.mFrame;
  // We don't expect frame to change depths.
  MOZ_ASSERT(frame->GetDepthInFrameTree() == lastFAD.mDepth);
  mList.RemoveLastElement();
  return frame;
}

void PresShell::DirtyRootsList::Clear() { mList.Clear(); }

bool PresShell::DirtyRootsList::Contains(nsIFrame* aFrame) const {
  return mList.Contains(aFrame);
}

bool PresShell::DirtyRootsList::IsEmpty() const { return mList.IsEmpty(); }

bool PresShell::DirtyRootsList::FrameIsAncestorOfDirtyRoot(
    nsIFrame* aFrame) const {
  MOZ_ASSERT(aFrame);

  // Look for a path from any dirty roots to aFrame, following GetParent().
  // This check mirrors what FrameNeedsReflow() would have done if the reflow
  // root didn't get in the way.
  for (nsIFrame* dirtyFrame : mList) {
    do {
      if (dirtyFrame == aFrame) {
        return true;
      }
      dirtyFrame = dirtyFrame->GetParent();
    } while (dirtyFrame);
  }

  return false;
}

bool PresShell::sDisableNonTestMouseEvents = false;

LazyLogModule PresShell::gLog("PresShell");

TimeStamp PresShell::EventHandler::sLastInputCreated;
TimeStamp PresShell::EventHandler::sLastInputProcessed;
StaticRefPtr<Element> PresShell::EventHandler::sLastKeyDownEventTargetElement;

bool PresShell::sProcessInteractable = false;

static bool gVerifyReflowEnabled;

bool PresShell::GetVerifyReflowEnable() {
#ifdef DEBUG
  static bool firstTime = true;
  if (firstTime) {
    firstTime = false;
    char* flags = PR_GetEnv("GECKO_VERIFY_REFLOW_FLAGS");
    if (flags) {
      bool error = false;

      for (;;) {
        char* comma = PL_strchr(flags, ',');
        if (comma) *comma = '\0';

        bool found = false;
        const VerifyReflowFlagData* flag = gFlags;
        const VerifyReflowFlagData* limit = gFlags + NUM_VERIFY_REFLOW_FLAGS;
        while (flag < limit) {
          if (PL_strcasecmp(flag->name, flags) == 0) {
            gVerifyReflowFlags |= flag->bit;
            found = true;
            break;
          }
          ++flag;
        }

        if (!found) error = true;

        if (!comma) break;

        *comma = ',';
        flags = comma + 1;
      }

      if (error) ShowVerifyReflowFlags();
    }

    if (VerifyReflowFlags::On & gVerifyReflowFlags) {
      gVerifyReflowEnabled = true;

      printf("Note: verifyreflow is enabled");
      if (VerifyReflowFlags::Noisy & gVerifyReflowFlags) {
        printf(" (noisy)");
      }
      if (VerifyReflowFlags::All & gVerifyReflowFlags) {
        printf(" (all)");
      }
      if (VerifyReflowFlags::DumpCommands & gVerifyReflowFlags) {
        printf(" (show reflow commands)");
      }
      if (VerifyReflowFlags::NoisyCommands & gVerifyReflowFlags) {
        printf(" (noisy reflow commands)");
        if (VerifyReflowFlags::ReallyNoisyCommands & gVerifyReflowFlags) {
          printf(" (REALLY noisy reflow commands)");
        }
      }
      printf("\n");
    }
  }
#endif
  return gVerifyReflowEnabled;
}

void PresShell::SetVerifyReflowEnable(bool aEnabled) {
  gVerifyReflowEnabled = aEnabled;
}

void PresShell::AddAutoWeakFrame(AutoWeakFrame* aWeakFrame) {
  if (aWeakFrame->GetFrame()) {
    aWeakFrame->GetFrame()->AddStateBits(NS_FRAME_EXTERNAL_REFERENCE);
  }
  aWeakFrame->SetPreviousWeakFrame(mAutoWeakFrames);
  mAutoWeakFrames = aWeakFrame;
}

void PresShell::AddWeakFrame(WeakFrame* aWeakFrame) {
  if (aWeakFrame->GetFrame()) {
    aWeakFrame->GetFrame()->AddStateBits(NS_FRAME_EXTERNAL_REFERENCE);
  }
  MOZ_ASSERT(!mWeakFrames.GetEntry(aWeakFrame));
  mWeakFrames.PutEntry(aWeakFrame);
}

void PresShell::RemoveAutoWeakFrame(AutoWeakFrame* aWeakFrame) {
  if (mAutoWeakFrames == aWeakFrame) {
    mAutoWeakFrames = aWeakFrame->GetPreviousWeakFrame();
    return;
  }
  AutoWeakFrame* nextWeak = mAutoWeakFrames;
  while (nextWeak && nextWeak->GetPreviousWeakFrame() != aWeakFrame) {
    nextWeak = nextWeak->GetPreviousWeakFrame();
  }
  if (nextWeak) {
    nextWeak->SetPreviousWeakFrame(aWeakFrame->GetPreviousWeakFrame());
  }
}

void PresShell::RemoveWeakFrame(WeakFrame* aWeakFrame) {
  MOZ_ASSERT(mWeakFrames.GetEntry(aWeakFrame));
  mWeakFrames.RemoveEntry(aWeakFrame);
}

already_AddRefed<nsFrameSelection> PresShell::FrameSelection() {
  RefPtr<nsFrameSelection> ret = mSelection;
  return ret.forget();
}

//----------------------------------------------------------------------

static uint32_t sNextPresShellId;

/* static */
bool PresShell::AccessibleCaretEnabled(nsIDocShell* aDocShell) {
  // If the pref forces it on, then enable it.
  if (StaticPrefs::layout_accessiblecaret_enabled()) {
    return true;
  }
  // If the touch pref is on, and touch events are enabled (this depends
  // on the specific device running), then enable it.
  if (StaticPrefs::layout_accessiblecaret_enabled_on_touch() &&
      dom::TouchEvent::PrefEnabled(aDocShell)) {
    return true;
  }
  // Otherwise, disabled.
  return false;
}

PresShell::PresShell(Document* aDocument)
    : mDocument(aDocument),
      mViewManager(nullptr),
      mFrameManager(nullptr),
      mAutoWeakFrames(nullptr),
#ifdef ACCESSIBILITY
      mDocAccessible(nullptr),
#endif  // #ifdef ACCESSIBILITY
      mCurrentEventFrame(nullptr),
      mMouseLocation(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE),
      mPaintCount(0),
      mAPZFocusSequenceNumber(0),
      mCanvasBackgroundColor(NS_RGBA(0, 0, 0, 0)),
      mActiveSuppressDisplayport(0),
      mPresShellId(sNextPresShellId++),
      mFontSizeInflationEmPerLine(0),
      mFontSizeInflationMinTwips(0),
      mFontSizeInflationLineThreshold(0),
      mSelectionFlags(nsISelectionDisplay::DISPLAY_TEXT |
                      nsISelectionDisplay::DISPLAY_IMAGES),
      mChangeNestCount(0),
      mRenderingStateFlags(RenderingStateFlags::None),
      mInFlush(false),
      mCaretEnabled(false),
      mNeedLayoutFlush(true),
      mNeedStyleFlush(true),
      mNeedThrottledAnimationFlush(true),
      mVisualViewportSizeSet(false),
      mDidInitialize(false),
      mIsDestroying(false),
      mIsReflowing(false),
      mIsObservingDocument(false),
      mForbiddenToFlush(false),
      mIsDocumentGone(false),
      mHaveShutDown(false),
      mPaintingSuppressed(false),
      mLastRootReflowHadUnconstrainedBSize(false),
      mShouldUnsuppressPainting(false),
      mIgnoreFrameDestruction(false),
      mIsActive(true),
      mFrozen(false),
      mIsFirstPaint(true),  // FIXME/bug 735029: find a better solution
      mObservesMutationsForPrint(false),
      mWasLastReflowInterrupted(false),
      mObservingStyleFlushes(false),
      mObservingLayoutFlushes(false),
      mResizeEventPending(false),
      mFontSizeInflationForceEnabled(false),
      mFontSizeInflationDisabledInMasterProcess(false),
      mFontSizeInflationEnabled(false),
      mPaintingIsFrozen(false),
      mIsNeverPainting(false),
      mResolutionUpdated(false),
      mResolutionUpdatedByApz(false),
      mUnderHiddenEmbedderElement(false),
      mDocumentLoading(false),
      mNoDelayedMouseEvents(false),
      mNoDelayedKeyEvents(false),
      mApproximateFrameVisibilityVisited(false),
      mNextPaintCompressed(false),
      mHasCSSBackgroundColor(true),
      mIsLastChromeOnlyEscapeKeyConsumed(false),
      mHasReceivedPaintMessage(false),
      mIsLastKeyDownCanceled(false),
      mHasHandledUserInput(false),
      mForceDispatchKeyPressEventsForNonPrintableKeys(false),
      mForceUseLegacyKeyCodeAndCharCodeValues(false),
      mInitializedWithKeyPressEventDispatchingBlacklist(false),
      mForceUseLegacyNonPrimaryDispatch(false),
      mInitializedWithClickEventDispatchingBlacklist(false),
      mMouseLocationWasSetBySynthesizedMouseEventForTests(false) {
  MOZ_LOG(gLog, LogLevel::Debug, ("PresShell::PresShell this=%p", this));
  MOZ_ASSERT(aDocument);

#ifdef MOZ_REFLOW_PERF
  mReflowCountMgr = MakeUnique<ReflowCountMgr>();
  mReflowCountMgr->SetPresContext(mPresContext);
  mReflowCountMgr->SetPresShell(this);
#endif
  mLastOSWake = mLoadBegin = TimeStamp::Now();
}

NS_INTERFACE_TABLE_HEAD(PresShell)
  NS_INTERFACE_TABLE_BEGIN
    // In most cases, PresShell should be treated as concrete class, but need to
    // QI for weak reference.  Therefore, the case needed by do_QueryReferent()
    // should be tested first.
    NS_INTERFACE_TABLE_ENTRY(PresShell, PresShell)
    NS_INTERFACE_TABLE_ENTRY(PresShell, nsIDocumentObserver)
    NS_INTERFACE_TABLE_ENTRY(PresShell, nsISelectionController)
    NS_INTERFACE_TABLE_ENTRY(PresShell, nsISelectionDisplay)
    NS_INTERFACE_TABLE_ENTRY(PresShell, nsIObserver)
    NS_INTERFACE_TABLE_ENTRY(PresShell, nsISupportsWeakReference)
    NS_INTERFACE_TABLE_ENTRY(PresShell, nsIMutationObserver)
    NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(PresShell, nsISupports, nsIObserver)
  NS_INTERFACE_TABLE_END
  NS_INTERFACE_TABLE_TO_MAP_SEGUE
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(PresShell)
NS_IMPL_RELEASE(PresShell)

PresShell::~PresShell() {
  MOZ_RELEASE_ASSERT(!mForbiddenToFlush,
                     "Flag should only be set temporarily, while doing things "
                     "that shouldn't cause destruction");
  MOZ_LOG(gLog, LogLevel::Debug, ("PresShell::~PresShell this=%p", this));

  if (!mHaveShutDown) {
    MOZ_ASSERT_UNREACHABLE("Someone did not call PresShell::Destroy()");
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

  MOZ_ASSERT(mAllocatedPointers.IsEmpty(),
             "Some pres arena objects were not freed");

  mFrameManager = nullptr;
  mFrameConstructor = nullptr;

  mCurrentEventContent = nullptr;
}

/**
 * Initialize the presentation shell. Create view manager and style
 * manager.
 * Note this can't be merged into our constructor because caret initialization
 * calls AddRef() on us.
 */
void PresShell::Init(nsPresContext* aPresContext, nsViewManager* aViewManager) {
  MOZ_ASSERT(mDocument);
  MOZ_ASSERT(aPresContext);
  MOZ_ASSERT(aViewManager);
  MOZ_ASSERT(!mViewManager, "already initialized");

  mViewManager = aViewManager;

  // mDocument is now set.  It might have a display document whose "need layout/
  // style" flush flags are not set, but ours will be set.  To keep these
  // consistent, call the flag setting functions to propagate those flags up
  // to the display document.
  SetNeedLayoutFlush();
  SetNeedStyleFlush();

  // Create our frame constructor.
  mFrameConstructor = MakeUnique<nsCSSFrameConstructor>(mDocument, this);

  mFrameManager = mFrameConstructor.get();

  // The document viewer owns both view manager and pres shell.
  mViewManager->SetPresShell(this);

  // Bind the context to the presentation shell.
  // FYI: We cannot initialize mPresContext in the constructor because we
  //      cannot call AttachPresShell() in it and once we initialize
  //      mPresContext, other objects may refer refresh driver or restyle
  //      manager via mPresContext and that causes hitting MOZ_ASSERT in some
  //      places.  Therefore, we should initialize mPresContext here with
  //      const_cast hack since we want to guarantee that mPresContext lives
  //      as long as the PresShell.
  const_cast<RefPtr<nsPresContext>&>(mPresContext) = aPresContext;
  mPresContext->AttachPresShell(this);

  mPresContext->DeviceContext()->InitFontCache();

  // FIXME(emilio, bug 1544185): Some Android code somehow depends on the shell
  // being eagerly registered as a style flush observer. This shouldn't be
  // needed otherwise.
  EnsureStyleFlush();

  // Add the preference style sheet.
  UpdatePreferenceStyles();

  bool accessibleCaretEnabled =
      AccessibleCaretEnabled(mDocument->GetDocShell());
  if (accessibleCaretEnabled) {
    // Need to happen before nsFrameSelection has been set up.
    mAccessibleCaretEventHub = new AccessibleCaretEventHub(this);
  }

  mSelection = new nsFrameSelection(this, nullptr, accessibleCaretEnabled);

  // Important: this has to happen after the selection has been set up
#ifdef SHOW_CARET
  // make the caret
  mCaret = new nsCaret();
  mCaret->Init(this);
  mOriginalCaret = mCaret;

  // SetCaretEnabled(true);       // make it show in browser windows
#endif
  // set up selection to be displayed in document
  // Don't enable selection for print media
  nsPresContext::nsPresContextType type = mPresContext->Type();
  if (type != nsPresContext::eContext_PrintPreview &&
      type != nsPresContext::eContext_Print) {
    SetDisplaySelection(nsISelectionController::SELECTION_DISABLED);
  }

  if (gMaxRCProcessingTime == -1) {
    gMaxRCProcessingTime =
        Preferences::GetInt("layout.reflow.timeslice", NS_MAX_REFLOW_TIME);
  }

  if (nsStyleSheetService* ss = nsStyleSheetService::GetInstance()) {
    ss->RegisterPresShell(this);
  }

  {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->AddObserver(this, "memory-pressure", false);
      os->AddObserver(this, NS_WIDGET_WAKE_OBSERVER_TOPIC, false);
      if (XRE_IsParentProcess() && !sProcessInteractable) {
        os->AddObserver(this, "sessionstore-one-or-no-tab-restored", false);
      }
      os->AddObserver(this, "font-info-updated", false);
      os->AddObserver(this, "look-and-feel-pref-changed", false);
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
    SMILAnimationController* animCtrl = mDocument->GetAnimationController();
    animCtrl->NotifyRefreshDriverCreated(GetPresContext()->RefreshDriver());
  }

  for (DocumentTimeline* timeline : mDocument->Timelines()) {
    timeline->NotifyRefreshDriverCreated(GetPresContext()->RefreshDriver());
  }

  // Get our activeness from the docShell.
  QueryIsActive();

  // Setup our font inflation preferences.
  mFontSizeInflationEmPerLine = StaticPrefs::font_size_inflation_emPerLine();
  mFontSizeInflationMinTwips = StaticPrefs::font_size_inflation_minTwips();
  mFontSizeInflationLineThreshold =
      StaticPrefs::font_size_inflation_lineThreshold();
  mFontSizeInflationForceEnabled =
      StaticPrefs::font_size_inflation_forceEnabled();
  mFontSizeInflationDisabledInMasterProcess =
      StaticPrefs::font_size_inflation_disabledInMasterProcess();
  // We'll compute the font size inflation state in Initialize(), when we know
  // the document type.

  mTouchManager.Init(this, mDocument);

  if (mPresContext->IsRootContentDocumentCrossProcess()) {
    mZoomConstraintsClient = new ZoomConstraintsClient();
    mZoomConstraintsClient->Init(this, mDocument);

    // We call this to create mMobileViewportManager, if it is needed.
    UpdateViewportOverridden(false);
  }

  if (nsCOMPtr<nsIDocShell> docShell = mPresContext->GetDocShell()) {
    BrowsingContext* bc = docShell->GetBrowsingContext();
    bool embedderFrameIsHidden = true;
    if (Element* embedderElement = bc->GetEmbedderElement()) {
      if (auto embedderFrame = embedderElement->GetPrimaryFrame()) {
        embedderFrameIsHidden = !embedderFrame->StyleVisibility()->IsVisible();
      }
    }

    if (BrowsingContext* parent = bc->GetParent()) {
      if (nsCOMPtr<nsIDocShell> parentDocShell = parent->GetDocShell()) {
        if (PresShell* parentPresShell = parentDocShell->GetPresShell()) {
          mUnderHiddenEmbedderElement =
              parentPresShell->IsUnderHiddenEmbedderElement() ||
              embedderFrameIsHidden;
        }
      }
    }
  }
}

enum TextPerfLogType { eLog_reflow, eLog_loaddone, eLog_totals };

static void LogTextPerfStats(gfxTextPerfMetrics* aTextPerf,
                             PresShell* aPresShell,
                             const gfxTextPerfMetrics::TextCounts& aCounts,
                             float aTime, TextPerfLogType aLogType,
                             const char* aURL) {
  LogModule* tpLog = gfxPlatform::GetLog(eGfxLog_textperf);

  // ignore XUL contexts unless at debug level
  mozilla::LogLevel logLevel = LogLevel::Warning;
  if (aCounts.numContentTextRuns == 0) {
    logLevel = LogLevel::Debug;
  }

  if (!MOZ_LOG_TEST(tpLog, logLevel)) {
    return;
  }

  char prefix[256];

  switch (aLogType) {
    case eLog_reflow:
      SprintfLiteral(prefix, "(textperf-reflow) %p time-ms: %7.0f", aPresShell,
                     aTime);
      break;
    case eLog_loaddone:
      SprintfLiteral(prefix, "(textperf-loaddone) %p time-ms: %7.0f",
                     aPresShell, aTime);
      break;
    default:
      MOZ_ASSERT(aLogType == eLog_totals, "unknown textperf log type");
      SprintfLiteral(prefix, "(textperf-totals) %p", aPresShell);
  }

  double hitRatio = 0.0;
  uint32_t lookups = aCounts.wordCacheHit + aCounts.wordCacheMiss;
  if (lookups) {
    hitRatio = double(aCounts.wordCacheHit) / double(lookups);
  }

  if (aLogType == eLog_loaddone) {
    MOZ_LOG(
        tpLog, logLevel,
        ("%s reflow: %d chars: %d "
         "[%s] "
         "content-textruns: %d chrome-textruns: %d "
         "max-textrun-len: %d "
         "word-cache-lookups: %d word-cache-hit-ratio: %4.3f "
         "word-cache-space: %d word-cache-long: %d "
         "pref-fallbacks: %d system-fallbacks: %d "
         "textruns-const: %d textruns-destr: %d "
         "generic-lookups: %d "
         "cumulative-textruns-destr: %d\n",
         prefix, aTextPerf->reflowCount, aCounts.numChars, (aURL ? aURL : ""),
         aCounts.numContentTextRuns, aCounts.numChromeTextRuns,
         aCounts.maxTextRunLen, lookups, hitRatio, aCounts.wordCacheSpaceRules,
         aCounts.wordCacheLong, aCounts.fallbackPrefs, aCounts.fallbackSystem,
         aCounts.textrunConst, aCounts.textrunDestr, aCounts.genericLookups,
         aTextPerf->cumulative.textrunDestr));
  } else {
    MOZ_LOG(
        tpLog, logLevel,
        ("%s reflow: %d chars: %d "
         "content-textruns: %d chrome-textruns: %d "
         "max-textrun-len: %d "
         "word-cache-lookups: %d word-cache-hit-ratio: %4.3f "
         "word-cache-space: %d word-cache-long: %d "
         "pref-fallbacks: %d system-fallbacks: %d "
         "textruns-const: %d textruns-destr: %d "
         "generic-lookups: %d "
         "cumulative-textruns-destr: %d\n",
         prefix, aTextPerf->reflowCount, aCounts.numChars,
         aCounts.numContentTextRuns, aCounts.numChromeTextRuns,
         aCounts.maxTextRunLen, lookups, hitRatio, aCounts.wordCacheSpaceRules,
         aCounts.wordCacheLong, aCounts.fallbackPrefs, aCounts.fallbackSystem,
         aCounts.textrunConst, aCounts.textrunDestr, aCounts.genericLookups,
         aTextPerf->cumulative.textrunDestr));
  }
}

void PresShell::Destroy() {
  // Do not add code before this line please!
  if (mHaveShutDown) {
    return;
  }

  NS_ASSERTION(!nsContentUtils::IsSafeToRunScript(),
               "destroy called on presshell while scripts not blocked");

  AUTO_PROFILER_LABEL("PresShell::Destroy", LAYOUT);

  // dump out cumulative text perf metrics
  gfxTextPerfMetrics* tp;
  if (mPresContext && (tp = mPresContext->GetTextPerfMetrics())) {
    tp->Accumulate();
    if (tp->cumulative.numChars > 0) {
      LogTextPerfStats(tp, this, tp->cumulative, 0.0, eLog_totals, nullptr);
    }
  }
  if (mPresContext) {
    if (gfxUserFontSet* fs = mPresContext->GetUserFontSet()) {
      uint32_t fontCount;
      uint64_t fontSize;
      fs->GetLoadStatistics(fontCount, fontSize);
      Telemetry::Accumulate(Telemetry::WEBFONT_PER_PAGE, fontCount);
      Telemetry::Accumulate(Telemetry::WEBFONT_SIZE_PER_PAGE,
                            uint32_t(fontSize / 1024));
    } else {
      Telemetry::Accumulate(Telemetry::WEBFONT_PER_PAGE, 0);
      Telemetry::Accumulate(Telemetry::WEBFONT_SIZE_PER_PAGE, 0);
    }
    const auto* stats = mPresContext->GetFontMatchingStats();
    if (stats) {
      Document* doc = GetDocument();
      if (doc && doc->IsContentDocument()) {
        nsIURI* uri = doc->GetDocumentURI();
        nsAutoCString path;
        if (uri && !uri->SchemeIs("about") && !uri->SchemeIs("chrome") &&
            !uri->SchemeIs("resource") &&
            !(uri->SchemeIs("moz-extension") &&
              (NS_SUCCEEDED(uri->GetFilePath(path)) &&
               StringEndsWith(
                   path,
                   NS_LITERAL_CSTRING("/_generated_background_page.html"))))) {
          Telemetry::Accumulate(Telemetry::BASE_FONT_FAMILIES_PER_PAGE,
                                stats->mBaseFonts);
          Telemetry::Accumulate(Telemetry::LANGPACK_FONT_FAMILIES_PER_PAGE,
                                stats->mLangPackFonts);
          Telemetry::Accumulate(Telemetry::USER_FONT_FAMILIES_PER_PAGE,
                                stats->mUserFonts);
          Telemetry::Accumulate(Telemetry::WEB_FONT_FAMILIES_PER_PAGE,
                                stats->mWebFonts);
          Telemetry::Accumulate(
              Telemetry::FALLBACK_TO_PREFS_FONT,
              bool(stats->mFallbacks & FallbackTypes::FallbackToPrefsFont));
          Telemetry::Accumulate(
              Telemetry::FALLBACK_TO_BASE_FONT,
              bool(stats->mFallbacks & FallbackTypes::FallbackToBaseFont));
          Telemetry::Accumulate(
              Telemetry::FALLBACK_TO_LANGPACK_FONT,
              bool(stats->mFallbacks & FallbackTypes::FallbackToLangPackFont));
          Telemetry::Accumulate(
              Telemetry::FALLBACK_TO_USER_FONT,
              bool(stats->mFallbacks & FallbackTypes::FallbackToUserFont));
          Telemetry::Accumulate(
              Telemetry::MISSING_FONT,
              bool(stats->mFallbacks & FallbackTypes::MissingFont));
          Telemetry::Accumulate(
              Telemetry::MISSING_FONT_LANGPACK,
              bool(stats->mFallbacks & FallbackTypes::MissingFontLangPack));
          Telemetry::Accumulate(
              Telemetry::MISSING_FONT_USER,
              bool(stats->mFallbacks & FallbackTypes::MissingFontUser));
        }
      }
    }
  }

#ifdef MOZ_REFLOW_PERF
  DumpReflows();
  mReflowCountMgr = nullptr;
#endif

  if (mZoomConstraintsClient) {
    mZoomConstraintsClient->Destroy();
    mZoomConstraintsClient = nullptr;
  }
  if (mMobileViewportManager) {
    mMobileViewportManager->Destroy();
    mMobileViewportManager = nullptr;
    mMVMContext = nullptr;
  }

#ifdef ACCESSIBILITY
  if (mDocAccessible) {
#  ifdef DEBUG
    if (a11y::logging::IsEnabled(a11y::logging::eDocDestroy))
      a11y::logging::DocDestroy("presshell destroyed", mDocument);
#  endif

    mDocAccessible->Shutdown();
    mDocAccessible = nullptr;
  }
#endif  // ACCESSIBILITY

  MaybeReleaseCapturingContent();

  EventHandler::OnPresShellDestroy(mDocument);

  if (mContentToScrollTo) {
    mContentToScrollTo->RemoveProperty(nsGkAtoms::scrolling);
    mContentToScrollTo = nullptr;
  }

  if (mPresContext) {
    // We need to notify the destroying the nsPresContext to ESM for
    // suppressing to use from ESM.
    mPresContext->EventStateManager()->NotifyDestroyPresContext(mPresContext);
  }

  if (nsStyleSheetService* ss = nsStyleSheetService::GetInstance()) {
    ss->UnregisterPresShell(this);
  }

  {
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->RemoveObserver(this, "memory-pressure");
      os->RemoveObserver(this, NS_WIDGET_WAKE_OBSERVER_TOPIC);
      if (XRE_IsParentProcess()) {
        os->RemoveObserver(this, "sessionstore-one-or-no-tab-restored");
      }
      os->RemoveObserver(this, "font-info-updated");
      os->RemoveObserver(this, "look-and-feel-pref-changed");
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

  mUpdateApproximateFrameVisibilityEvent.Revoke();

  ClearApproximatelyVisibleFramesList(Some(OnNonvisible::DiscardImages));

  if (mCaret) {
    mCaret->Terminate();
    mCaret = nullptr;
  }

  mFocusedFrameSelection = nullptr;

  if (mSelection) {
    RefPtr<nsFrameSelection> frameSelection = mSelection;
    frameSelection->DisconnectFromPresShell();
  }

  // release our pref style sheet, if we have one still
  //
  // TODO(emilio): Should we move the preference sheet tracking to the Document?
  RemovePreferenceStyles();

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
  mPendingScrollAnchorSelection.Clear();
  mPendingScrollAnchorAdjustment.Clear();

  if (mViewManager) {
    // Clear the view manager's weak pointer back to |this| in case it
    // was leaked.
    mViewManager->SetPresShell(nullptr);
    mViewManager = nullptr;
  }

  nsRefreshDriver* rd = GetPresContext()->RefreshDriver();

  // This shell must be removed from the document before the frame
  // hierarchy is torn down to avoid finding deleted frames through
  // this presshell while the frames are being torn down
  if (mDocument) {
    NS_ASSERTION(mDocument->GetPresShell() == this, "Wrong shell?");
    mDocument->ClearServoRestyleRoot();
    mDocument->DeletePresShell();

    if (mDocument->HasAnimationController()) {
      mDocument->GetAnimationController()->NotifyRefreshDriverDestroying(rd);
    }
    for (DocumentTimeline* timeline : mDocument->Timelines()) {
      timeline->NotifyRefreshDriverDestroying(rd);
    }
  }

  if (mPresContext) {
    rd->CancelPendingAnimationEvents(mPresContext->AnimationEventDispatcher());
  }

  // Revoke any pending events.  We need to do this and cancel pending reflows
  // before we destroy the frame manager, since apparently frame destruction
  // sometimes spins the event queue when plug-ins are involved(!).
  StopObservingRefreshDriver();

  if (rd->GetPresContext() == GetPresContext()) {
    rd->RevokeViewManagerFlush();
    rd->ClearHasScheduleFlush();
  }

  CancelAllPendingReflows();
  CancelPostedReflowCallbacks();

  // Destroy the frame manager. This will destroy the frame hierarchy
  mFrameConstructor->WillDestroyFrameTree();

  NS_WARNING_ASSERTION(!mAutoWeakFrames && mWeakFrames.IsEmpty(),
                       "Weak frames alive after destroying FrameManager");
  while (mAutoWeakFrames) {
    mAutoWeakFrames->Clear(this);
  }
  nsTArray<WeakFrame*> toRemove(mWeakFrames.Count());
  for (auto iter = mWeakFrames.Iter(); !iter.Done(); iter.Next()) {
    toRemove.AppendElement(iter.Get()->GetKey());
  }
  for (WeakFrame* weakFrame : toRemove) {
    weakFrame->Clear(this);
  }

  // Terminate AccessibleCaretEventHub after tearing down the frame tree so that
  // we don't need to remove caret element's frame in
  // AccessibleCaret::RemoveCaretElement().
  if (mAccessibleCaretEventHub) {
    mAccessibleCaretEventHub->Terminate();
    mAccessibleCaretEventHub = nullptr;
  }

  if (mPresContext) {
    // We hold a reference to the pres context, and it holds a weak link back
    // to us. To avoid the pres context having a dangling reference, set its
    // pres shell to nullptr
    mPresContext->DetachPresShell();
  }

  mHaveShutDown = true;

  mTouchManager.Destroy();
}

void PresShell::StopObservingRefreshDriver() {
  nsRefreshDriver* rd = mPresContext->RefreshDriver();
  if (mResizeEventPending) {
    rd->RemoveResizeEventFlushObserver(this);
  }
  if (mObservingLayoutFlushes) {
    rd->RemoveLayoutFlushObserver(this);
  }
  if (mObservingStyleFlushes) {
    rd->RemoveStyleFlushObserver(this);
  }
}

void PresShell::StartObservingRefreshDriver() {
  nsRefreshDriver* rd = mPresContext->RefreshDriver();
  if (mResizeEventPending) {
    rd->AddResizeEventFlushObserver(this);
  }
  if (mObservingLayoutFlushes) {
    rd->AddLayoutFlushObserver(this);
  }
  if (mObservingStyleFlushes) {
    rd->AddStyleFlushObserver(this);
  }
}

nsRefreshDriver* PresShell::GetRefreshDriver() const {
  return mPresContext ? mPresContext->RefreshDriver() : nullptr;
}

void PresShell::SetAuthorStyleDisabled(bool aStyleDisabled) {
  if (aStyleDisabled != StyleSet()->GetAuthorStyleDisabled()) {
    StyleSet()->SetAuthorStyleDisabled(aStyleDisabled);
    mDocument->ApplicableStylesChanged();

    nsCOMPtr<nsIObserverService> observerService =
        mozilla::services::GetObserverService();
    if (observerService) {
      observerService->NotifyObservers(
          ToSupports(mDocument), "author-style-disabled-changed", nullptr);
    }
  }
}

bool PresShell::GetAuthorStyleDisabled() const {
  return StyleSet()->GetAuthorStyleDisabled();
}

void PresShell::UpdatePreferenceStyles() {
  if (!mDocument) {
    return;
  }

  // If the document doesn't have a window there's no need to notify
  // its presshell about changes to preferences since the document is
  // in a state where it doesn't matter any more (see
  // nsDocumentViewer::Close()).
  if (!mDocument->GetWindow()) {
    return;
  }

  // Documents in chrome shells do not have any preference style rules applied.
  if (nsContentUtils::IsInChromeDocshell(mDocument)) {
    return;
  }

  PreferenceSheet::EnsureInitialized();
  auto* cache = GlobalStyleSheetCache::Singleton();

  RefPtr<StyleSheet> newPrefSheet =
      PreferenceSheet::ShouldUseChromePrefs(*mDocument)
          ? cache->ChromePreferenceSheet()
          : cache->ContentPreferenceSheet();

  if (mPrefStyleSheet == newPrefSheet) {
    return;
  }

  RemovePreferenceStyles();

  // NOTE(emilio): This sheet is added as an agent sheet, because we don't want
  // it to be modifiable from devtools and similar, see bugs 1239336 and
  // 1436782. I think it conceptually should be a user sheet, and could be
  // without too much trouble I'd think.
  StyleSet()->AppendStyleSheet(*newPrefSheet);
  mPrefStyleSheet = newPrefSheet;
}

void PresShell::RemovePreferenceStyles() {
  if (mPrefStyleSheet) {
    StyleSet()->RemoveStyleSheet(*mPrefStyleSheet);
    mPrefStyleSheet = nullptr;
  }
}

void PresShell::AddUserSheet(StyleSheet* aSheet) {
  // Make sure this does what nsDocumentViewer::CreateStyleSet does wrt
  // ordering. We want this new sheet to come after all the existing stylesheet
  // service sheets (which are at the start), but before other user sheets; see
  // nsIStyleSheetService.idl for the ordering.

  nsStyleSheetService* sheetService = nsStyleSheetService::GetInstance();
  nsTArray<RefPtr<StyleSheet>>& userSheets = *sheetService->UserStyleSheets();

  // Search for the place to insert the new user sheet. Since all of the
  // stylesheet service provided user sheets should be at the start of the style
  // set's list, and aSheet should be at the end of userSheets. Given that, we
  // can find the right place to insert the new sheet based on the length of
  // userSheets.
  MOZ_ASSERT(aSheet);
  MOZ_ASSERT(userSheets.LastElement() == aSheet);

  size_t index = userSheets.Length() - 1;

  // Assert that all of userSheets (except for the last, new element) matches up
  // with what's in the style set.
  for (size_t i = 0; i < index; ++i) {
    MOZ_ASSERT(StyleSet()->SheetAt(StyleOrigin::User, i) == userSheets[i]);
  }

  if (index == static_cast<size_t>(StyleSet()->SheetCount(StyleOrigin::User))) {
    StyleSet()->AppendStyleSheet(*aSheet);
  } else {
    StyleSheet* ref = StyleSet()->SheetAt(StyleOrigin::User, index);
    StyleSet()->InsertStyleSheetBefore(*aSheet, *ref);
  }

  mDocument->ApplicableStylesChanged();
}

void PresShell::AddAgentSheet(StyleSheet* aSheet) {
  // Make sure this does what nsDocumentViewer::CreateStyleSet does
  // wrt ordering.
  StyleSet()->AppendStyleSheet(*aSheet);
  mDocument->ApplicableStylesChanged();
}

void PresShell::AddAuthorSheet(StyleSheet* aSheet) {
  // Document specific "additional" Author sheets should be stronger than the
  // ones added with the StyleSheetService.
  StyleSheet* firstAuthorSheet = mDocument->GetFirstAdditionalAuthorSheet();
  if (firstAuthorSheet) {
    StyleSet()->InsertStyleSheetBefore(*aSheet, *firstAuthorSheet);
  } else {
    StyleSet()->AppendStyleSheet(*aSheet);
  }

  mDocument->ApplicableStylesChanged();
}

void PresShell::SelectionWillTakeFocus() {
  if (mSelection) {
    FrameSelectionWillTakeFocus(*mSelection);
  }
}

void PresShell::SelectionWillLoseFocus() {
  // Do nothing, the main selection is the default focused selection.
}

// Selection repainting code relies on selection offsets being properly
// adjusted (see bug 1626291), so we need to wait until the DOM is finished
// notifying.
static void RepaintNormalSelectionWhenSafe(nsFrameSelection& aFrameSelection) {
  if (nsContentUtils::IsSafeToRunScript()) {
    aFrameSelection.RepaintSelection(SelectionType::eNormal);
    return;
  }

  // Note that importantly we don't defer changing the DisplaySelection. That'd
  // be potentially racy with other code that may change it.
  nsContentUtils::AddScriptRunner(NS_NewRunnableFunction(
      "RepaintNormalSelectionWhenSafe",
      [sel = RefPtr<nsFrameSelection>(&aFrameSelection)] {
        sel->RepaintSelection(SelectionType::eNormal);
      }));
}

void PresShell::FrameSelectionWillLoseFocus(nsFrameSelection& aFrameSelection) {
  if (mFocusedFrameSelection != &aFrameSelection) {
    return;
  }

  // Do nothing, the main selection is the default focused selection.
  if (&aFrameSelection == mSelection) {
    return;
  }

  RefPtr<nsFrameSelection> old = std::move(mFocusedFrameSelection);
  MOZ_ASSERT(!mFocusedFrameSelection);

  if (old->GetDisplaySelection() != nsISelectionController::SELECTION_HIDDEN) {
    old->SetDisplaySelection(nsISelectionController::SELECTION_HIDDEN);
    RepaintNormalSelectionWhenSafe(*old);
  }

  if (mSelection) {
    FrameSelectionWillTakeFocus(*mSelection);
  }
}

void PresShell::FrameSelectionWillTakeFocus(nsFrameSelection& aFrameSelection) {
  if (mFocusedFrameSelection == &aFrameSelection) {
#ifdef XP_MACOSX
    // FIXME: Mac needs to update the global selection cache, even if the
    // document's focused selection doesn't change, and this is currently done
    // from RepaintSelection. Maybe we should move part of the global selection
    // handling here, or something of that sort, unclear.
    RepaintNormalSelectionWhenSafe(aFrameSelection);
#endif
    return;
  }

  RefPtr<nsFrameSelection> old = std::move(mFocusedFrameSelection);
  mFocusedFrameSelection = &aFrameSelection;

  if (old &&
      old->GetDisplaySelection() != nsISelectionController::SELECTION_HIDDEN) {
    old->SetDisplaySelection(nsISelectionController::SELECTION_HIDDEN);
    RepaintNormalSelectionWhenSafe(*old);
  }

  if (aFrameSelection.GetDisplaySelection() !=
      nsISelectionController::SELECTION_ON) {
    aFrameSelection.SetDisplaySelection(nsISelectionController::SELECTION_ON);
    RepaintNormalSelectionWhenSafe(aFrameSelection);
  }
}

NS_IMETHODIMP
PresShell::SetDisplaySelection(int16_t aToggle) {
  RefPtr<nsFrameSelection> frameSelection = mSelection;
  frameSelection->SetDisplaySelection(aToggle);
  return NS_OK;
}

NS_IMETHODIMP
PresShell::GetDisplaySelection(int16_t* aToggle) {
  RefPtr<nsFrameSelection> frameSelection = mSelection;
  *aToggle = frameSelection->GetDisplaySelection();
  return NS_OK;
}

NS_IMETHODIMP
PresShell::GetSelectionFromScript(RawSelectionType aRawSelectionType,
                                  Selection** aSelection) {
  if (!aSelection || !mSelection) return NS_ERROR_NULL_POINTER;

  RefPtr<nsFrameSelection> frameSelection = mSelection;
  RefPtr<Selection> selection =
      frameSelection->GetSelection(ToSelectionType(aRawSelectionType));

  if (!selection) {
    return NS_ERROR_INVALID_ARG;
  }

  selection.forget(aSelection);
  return NS_OK;
}

Selection* PresShell::GetSelection(RawSelectionType aRawSelectionType) {
  if (!mSelection) {
    return nullptr;
  }

  RefPtr<nsFrameSelection> frameSelection = mSelection;
  return frameSelection->GetSelection(ToSelectionType(aRawSelectionType));
}

Selection* PresShell::GetCurrentSelection(SelectionType aSelectionType) {
  if (!mSelection) {
    return nullptr;
  }

  RefPtr<nsFrameSelection> frameSelection = mSelection;
  return frameSelection->GetSelection(aSelectionType);
}

nsFrameSelection* PresShell::GetLastFocusedFrameSelection() {
  return mFocusedFrameSelection ? mFocusedFrameSelection : mSelection;
}

NS_IMETHODIMP
PresShell::ScrollSelectionIntoView(RawSelectionType aRawSelectionType,
                                   SelectionRegion aRegion, int16_t aFlags) {
  if (!mSelection) return NS_ERROR_NULL_POINTER;

  RefPtr<nsFrameSelection> frameSelection = mSelection;
  return frameSelection->ScrollSelectionIntoView(
      ToSelectionType(aRawSelectionType), aRegion, aFlags);
}

NS_IMETHODIMP
PresShell::RepaintSelection(RawSelectionType aRawSelectionType) {
  if (!mSelection) {
    return NS_ERROR_NULL_POINTER;
  }

  if (MOZ_UNLIKELY(mIsDestroying)) {
    return NS_OK;
  }

  RefPtr<nsFrameSelection> frameSelection = mSelection;
  return frameSelection->RepaintSelection(ToSelectionType(aRawSelectionType));
}

// Make shell be a document observer
void PresShell::BeginObservingDocument() {
  if (mDocument && !mIsDestroying) {
    mIsObservingDocument = true;
    if (mIsDocumentGone) {
      NS_WARNING(
          "Adding a presshell that was disconnected from the document "
          "as a document observer?  Sounds wrong...");
      mIsDocumentGone = false;
    }
  }
}

// Make shell stop being a document observer
void PresShell::EndObservingDocument() {
  // XXXbz do we need to tell the frame constructor that the document
  // is gone, perhaps?  Except for printing it's NOT gone, sometimes.
  mIsDocumentGone = true;
  mIsObservingDocument = false;
}

#ifdef DEBUG_kipp
char* nsPresShell_ReflowStackPointerTop;
#endif

nsresult PresShell::Initialize() {
  if (mIsDestroying) {
    return NS_OK;
  }

  if (!mDocument) {
    // Nothing to do
    return NS_OK;
  }

  MOZ_LOG(gLog, LogLevel::Debug, ("PresShell::Initialize this=%p", this));

  NS_ASSERTION(!mDidInitialize, "Why are we being called?");

  RefPtr<PresShell> kungFuDeathGrip(this);

  RecomputeFontSizeInflationEnabled();
  MOZ_DIAGNOSTIC_ASSERT(!mIsDestroying);

  // Ensure the pres context doesn't think it has changed, since we haven't even
  // started layout. This avoids spurious restyles / reflows afterwards.
  //
  // Note that this is very intentionally before setting mDidInitialize so it
  // doesn't notify the document, or run media query change events.
  mPresContext->FlushPendingMediaFeatureValuesChanged();
  MOZ_DIAGNOSTIC_ASSERT(!mIsDestroying);

  mDidInitialize = true;

#ifdef DEBUG
  if (VerifyReflowFlags::NoisyCommands & gVerifyReflowFlags) {
    if (mDocument) {
      nsIURI* uri = mDocument->GetDocumentURI();
      if (uri) {
        printf("*** PresShell::Initialize (this=%p, url='%s')\n", (void*)this,
               uri->GetSpecOrDefault().get());
      }
    }
  }
#endif

  // Get the root frame from the frame manager
  // XXXbz it would be nice to move this somewhere else... like frame manager
  // Init(), say.  But we need to make sure our views are all set up by the
  // time we do this!
  nsIFrame* rootFrame = mFrameConstructor->GetRootFrame();
  NS_ASSERTION(!rootFrame, "How did that happen, exactly?");

  if (!rootFrame) {
    nsAutoScriptBlocker scriptBlocker;
    rootFrame = mFrameConstructor->ConstructRootFrame();
    mFrameConstructor->SetRootFrame(rootFrame);
  }

  NS_ENSURE_STATE(!mHaveShutDown);

  if (!rootFrame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (Element* root = mDocument->GetRootElement()) {
    {
      nsAutoCauseReflowNotifier reflowNotifier(this);
      // Have the style sheet processor construct frame for the root
      // content object down
      mFrameConstructor->ContentInserted(
          root, nsCSSFrameConstructor::InsertionKind::Sync);
    }
    // Something in mFrameConstructor->ContentInserted may have caused
    // Destroy() to get called, bug 337586.  Or, nsAutoCauseReflowNotifier
    // (which sets up a script blocker) going out of scope may have killed us
    // too
    NS_ENSURE_STATE(!mHaveShutDown);
  }

  mDocument->TriggerAutoFocus();

  NS_ASSERTION(rootFrame, "How did that happen?");

  // Note: when the frame was created above it had the NS_FRAME_IS_DIRTY bit
  // set, but XBL processing could have caused a reflow which clears it.
  if (MOZ_LIKELY(rootFrame->GetStateBits() & NS_FRAME_IS_DIRTY)) {
    // Unset the DIRTY bits so that FrameNeedsReflow() will work right.
    rootFrame->RemoveStateBits(NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN);
    NS_ASSERTION(!mDirtyRoots.Contains(rootFrame),
                 "Why is the root in mDirtyRoots already?");
    FrameNeedsReflow(rootFrame, IntrinsicDirty::Resize, NS_FRAME_IS_DIRTY);
    NS_ASSERTION(mDirtyRoots.Contains(rootFrame),
                 "Should be in mDirtyRoots now");
    NS_ASSERTION(mObservingLayoutFlushes, "Why no reflow scheduled?");
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
    Document::ReadyState readyState = mDocument->GetReadyStateEnum();
    if (readyState != Document::READYSTATE_COMPLETE) {
      mPaintSuppressionTimer = NS_NewTimer();
    }
    if (!mPaintSuppressionTimer) {
      mPaintingSuppressed = false;
    } else {
      // Initialize the timer.

      // Default to PAINTLOCK_EVENT_DELAY if we can't get the pref value.
      int32_t delay = Preferences::GetInt("nglayout.initialpaint.delay",
                                          PAINTLOCK_EVENT_DELAY);

      mPaintSuppressionTimer->SetTarget(
          mDocument->EventTargetFor(TaskCategory::Other));
      mPaintSuppressionTimer->InitWithNamedFuncCallback(
          sPaintSuppressionCallback, this, delay, nsITimer::TYPE_ONE_SHOT,
          "PresShell::sPaintSuppressionCallback");
    }
  }

  // If we get here and painting is not suppressed, we still want to run the
  // unsuppression logic, so set mShouldUnsuppressPainting to true.
  if (!mPaintingSuppressed) {
    mShouldUnsuppressPainting = true;
  }

  return NS_OK;  // XXX this needs to be real. MMP
}

void PresShell::sPaintSuppressionCallback(nsITimer* aTimer, void* aPresShell) {
  RefPtr<PresShell> self = static_cast<PresShell*>(aPresShell);
  if (self) self->UnsuppressPainting();
}

nsresult PresShell::ResizeReflow(nscoord aWidth, nscoord aHeight,
                                 ResizeReflowOptions aOptions) {
  if (mZoomConstraintsClient) {
    // If we have a ZoomConstraintsClient and the available screen area
    // changed, then we might need to disable double-tap-to-zoom, so notify
    // the ZCC to update itself.
    mZoomConstraintsClient->ScreenSizeChanged();
  }
  if (mMobileViewportManager) {
    // If we have a mobile viewport manager, request a reflow from it. It can
    // recompute the final CSS viewport and trigger a call to
    // ResizeReflowIgnoreOverride if it changed. We don't force adjusting
    // of resolution, because that is only necessary when we are destroying
    // the MVM.
    mMobileViewportManager->RequestReflow(false);
    return NS_OK;
  }

  return ResizeReflowIgnoreOverride(aWidth, aHeight, aOptions);
}

void PresShell::SimpleResizeReflow(nscoord aWidth, nscoord aHeight,
                                   ResizeReflowOptions aOptions) {
  MOZ_ASSERT(aWidth != NS_UNCONSTRAINEDSIZE);
  MOZ_ASSERT(aHeight != NS_UNCONSTRAINEDSIZE);
  nsSize oldSize = mPresContext->GetVisibleArea().Size();
  mPresContext->SetVisibleArea(nsRect(0, 0, aWidth, aHeight));
  nsIFrame* rootFrame = GetRootFrame();
  if (!rootFrame) {
    return;
  }
  WritingMode wm = rootFrame->GetWritingMode();
  bool isBSizeChanging =
      wm.IsVertical() ? oldSize.width != aWidth : oldSize.height != aHeight;
  if (isBSizeChanging) {
    nsLayoutUtils::MarkIntrinsicISizesDirtyIfDependentOnBSize(rootFrame);
  }
  FrameNeedsReflow(rootFrame, IntrinsicDirty::Resize,
                   NS_FRAME_HAS_DIRTY_CHILDREN);

  // For compat with the old code path which always reflowed as long as there
  // was a root frame.
  bool suppressReflow = (aOptions & ResizeReflowOptions::SuppressReflow) ||
                        mPresContext->SuppressingResizeReflow();
  if (!suppressReflow) {
    mDocument->FlushPendingNotifications(FlushType::InterruptibleLayout);
  }
}

void PresShell::AddResizeEventFlushObserverIfNeeded() {
  if (!mIsDestroying && !mResizeEventPending &&
      MOZ_LIKELY(!mDocument->GetBFCacheEntry())) {
    mResizeEventPending = true;
    mPresContext->RefreshDriver()->AddResizeEventFlushObserver(this);
  }
}

nsresult PresShell::ResizeReflowIgnoreOverride(nscoord aWidth, nscoord aHeight,
                                               ResizeReflowOptions aOptions) {
  MOZ_ASSERT(!mIsReflowing, "Shouldn't be in reflow here!");

  // Historically we never fired resize events if there was no root frame by the
  // time this function got called.
  const bool initialized = mDidInitialize;
  RefPtr<PresShell> kungFuDeathGrip(this);

  auto postResizeEventIfNeeded = [this, initialized]() {
    if (initialized) {
      AddResizeEventFlushObserverIfNeeded();
    }
  };

  if (!(aOptions & ResizeReflowOptions::BSizeLimit)) {
    nsSize oldSize = mPresContext->GetVisibleArea().Size();
    if (oldSize == nsSize(aWidth, aHeight)) {
      return NS_OK;
    }

    SimpleResizeReflow(aWidth, aHeight, aOptions);
    postResizeEventIfNeeded();
    return NS_OK;
  }

  MOZ_ASSERT(!mPresContext->SuppressingResizeReflow() &&
                 !(aOptions & ResizeReflowOptions::SuppressReflow),
             "Can't suppress resize reflow and shrink-wrap at the same time");

  // Make sure that style is flushed before setting the pres context
  // VisibleArea.
  //
  // Otherwise we may end up with bogus viewport units resolved against the
  // unconstrained bsize, or restyling the whole document resolving viewport
  // units against targetWidth, which may end up doing wasteful work.
  mDocument->FlushPendingNotifications(FlushType::Frames);

  nsIFrame* rootFrame = GetRootFrame();
  if (mIsDestroying || !rootFrame) {
    // If we don't have a root frame yet, that means we haven't had our initial
    // reflow... If that's the case, and aWidth or aHeight is unconstrained,
    // ignore them altogether.
    if (aHeight == NS_UNCONSTRAINEDSIZE || aWidth == NS_UNCONSTRAINEDSIZE) {
      // We can't do the work needed for SizeToContent without a root
      // frame, and we want to return before setting the visible area.
      return NS_ERROR_NOT_AVAILABLE;
    }

    mPresContext->SetVisibleArea(nsRect(0, 0, aWidth, aHeight));
    // There isn't anything useful we can do if the initial reflow hasn't
    // happened.
    return NS_OK;
  }

  WritingMode wm = rootFrame->GetWritingMode();
  MOZ_ASSERT((wm.IsVertical() ? aHeight : aWidth) != NS_UNCONSTRAINEDSIZE,
             "unconstrained isize not allowed");

  nscoord targetWidth = aWidth;
  nscoord targetHeight = aHeight;
  if (wm.IsVertical()) {
    targetWidth = NS_UNCONSTRAINEDSIZE;
  } else {
    targetHeight = NS_UNCONSTRAINEDSIZE;
  }

  mPresContext->SetVisibleArea(nsRect(0, 0, targetWidth, targetHeight));
  // XXX Do a full invalidate at the beginning so that invalidates along
  // the way don't have region accumulation issues?

  // For height:auto BSizes (i.e. layout-controlled), descendant
  // intrinsic sizes can't depend on them. So the only other case is
  // viewport-controlled BSizes which we handle here.
  nsLayoutUtils::MarkIntrinsicISizesDirtyIfDependentOnBSize(rootFrame);

  {
    nsAutoCauseReflowNotifier crNotifier(this);
    WillDoReflow();

    // Kick off a top-down reflow
    AUTO_LAYOUT_PHASE_ENTRY_POINT(GetPresContext(), Reflow);
    nsViewManager::AutoDisableRefresh refreshBlocker(mViewManager);

    mDirtyRoots.Remove(rootFrame);
    DoReflow(rootFrame, true, nullptr);

    const bool reflowAgain =
        wm.IsVertical() ? mPresContext->GetVisibleArea().width > aWidth
                        : mPresContext->GetVisibleArea().height > aHeight;

    if (reflowAgain) {
      mPresContext->SetVisibleArea(nsRect(0, 0, aWidth, aHeight));
      DoReflow(rootFrame, true, nullptr);
    }
  }

  // Now, we may have been destroyed by the destructor of
  // `nsAutoCauseReflowNotifier`.

  DidDoReflow(true);

  // the reflow above should've set our bsize if it was NS_UNCONSTRAINEDSIZE,
  // and the isize shouldn't be NS_UNCONSTRAINEDSIZE anyway.
  MOZ_DIAGNOSTIC_ASSERT(
      mPresContext->GetVisibleArea().width != NS_UNCONSTRAINEDSIZE,
      "width should not be NS_UNCONSTRAINEDSIZE after reflow");
  MOZ_DIAGNOSTIC_ASSERT(
      mPresContext->GetVisibleArea().height != NS_UNCONSTRAINEDSIZE,
      "height should not be NS_UNCONSTRAINEDSIZE after reflow");

  postResizeEventIfNeeded();
  return NS_OK;  // XXX this needs to be real. MMP
}

void PresShell::FireResizeEvent() {
  if (mIsDocumentGone) {
    return;
  }

  // If event handling is suppressed, repost the resize event to the refresh
  // driver. The event is marked as delayed so that the refresh driver does not
  // continue ticking.
  if (mDocument->EventHandlingSuppressed()) {
    if (MOZ_LIKELY(!mDocument->GetBFCacheEntry())) {
      mDocument->SetHasDelayedRefreshEvent();
      mPresContext->RefreshDriver()->AddResizeEventFlushObserver(
          this, /* aDelayed = */ true);
    }
    return;
  }

  mResizeEventPending = false;

  // Send resize event from here.
  WidgetEvent event(true, mozilla::eResize);
  nsEventStatus status = nsEventStatus_eIgnore;

  if (nsPIDOMWindowOuter* window = mDocument->GetWindow()) {
    EventDispatcher::Dispatch(window, mPresContext, &event, nullptr, &status);
  }
}

static nsIContent* GetNativeAnonymousSubtreeRoot(nsIContent* aContent) {
  if (!aContent) {
    return nullptr;
  }
  return aContent->GetClosestNativeAnonymousSubtreeRoot();
}

void PresShell::NativeAnonymousContentRemoved(nsIContent* aAnonContent) {
  MOZ_ASSERT(aAnonContent->IsRootOfNativeAnonymousSubtree());
  if (nsIContent* root = GetNativeAnonymousSubtreeRoot(mCurrentEventContent)) {
    if (aAnonContent == root) {
      mCurrentEventContent = aAnonContent->GetFlattenedTreeParent();
      mCurrentEventFrame = nullptr;
    }
  }

  for (unsigned int i = 0; i < mCurrentEventContentStack.Length(); i++) {
    nsIContent* anon =
        GetNativeAnonymousSubtreeRoot(mCurrentEventContentStack.ElementAt(i));
    if (aAnonContent == anon) {
      mCurrentEventContentStack.ReplaceObjectAt(
          aAnonContent->GetFlattenedTreeParent(), i);
      mCurrentEventFrameStack[i] = nullptr;
    }
  }
}

void PresShell::SetIgnoreFrameDestruction(bool aIgnore) {
  if (mDocument) {
    // We need to tell the ImageLoader to drop all its references to frames
    // because they're about to go away and it won't get notifications of that.
    mDocument->StyleImageLoader()->ClearFrames(mPresContext);
  }
  mIgnoreFrameDestruction = aIgnore;
}

void PresShell::NotifyDestroyingFrame(nsIFrame* aFrame) {
  // We must remove these from FrameLayerBuilder::DisplayItemData::mFrameList
  // here, otherwise the DisplayItemData destructor will use the destroyed frame
  // when it tries to remove it from the (array) value of this property.
  aFrame->RemoveDisplayItemDataForDeletion();

  if (!mIgnoreFrameDestruction) {
    if (aFrame->HasImageRequest()) {
      mDocument->StyleImageLoader()->DropRequestsForFrame(aFrame);
    }

    mFrameConstructor->NotifyDestroyingFrame(aFrame);

    mDirtyRoots.Remove(aFrame);

    // Remove frame properties
    aFrame->RemoveAllProperties();

    if (aFrame == mCurrentEventFrame) {
      mCurrentEventContent = aFrame->GetContent();
      mCurrentEventFrame = nullptr;
    }

#ifdef DEBUG
    if (aFrame == mDrawEventTargetFrame) {
      mDrawEventTargetFrame = nullptr;
    }
#endif

    for (unsigned int i = 0; i < mCurrentEventFrameStack.Length(); i++) {
      if (aFrame == mCurrentEventFrameStack.ElementAt(i)) {
        // One of our stack frames was deleted.  Get its content so that when we
        // pop it we can still get its new frame from its content
        nsIContent* currentEventContent = aFrame->GetContent();
        mCurrentEventContentStack.ReplaceObjectAt(currentEventContent, i);
        mCurrentEventFrameStack[i] = nullptr;
      }
    }

    mFramesToDirty.RemoveEntry(aFrame);

    nsIScrollableFrame* scrollableFrame = do_QueryFrame(aFrame);
    if (scrollableFrame) {
      mPendingScrollAnchorSelection.RemoveEntry(scrollableFrame);
      mPendingScrollAnchorAdjustment.RemoveEntry(scrollableFrame);
    }
  }
}

already_AddRefed<nsCaret> PresShell::GetCaret() const {
  RefPtr<nsCaret> caret = mCaret;
  return caret.forget();
}

already_AddRefed<AccessibleCaretEventHub>
PresShell::GetAccessibleCaretEventHub() const {
  RefPtr<AccessibleCaretEventHub> eventHub = mAccessibleCaretEventHub;
  return eventHub.forget();
}

void PresShell::SetCaret(nsCaret* aNewCaret) { mCaret = aNewCaret; }

void PresShell::RestoreCaret() { mCaret = mOriginalCaret; }

NS_IMETHODIMP PresShell::SetCaretEnabled(bool aInEnable) {
  bool oldEnabled = mCaretEnabled;

  mCaretEnabled = aInEnable;

  if (mCaretEnabled != oldEnabled) {
    MOZ_ASSERT(mCaret);
    if (mCaret) {
      mCaret->SetVisible(mCaretEnabled);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP PresShell::SetCaretReadOnly(bool aReadOnly) {
  if (mCaret) mCaret->SetCaretReadOnly(aReadOnly);
  return NS_OK;
}

NS_IMETHODIMP PresShell::GetCaretEnabled(bool* aOutEnabled) {
  NS_ENSURE_ARG_POINTER(aOutEnabled);
  *aOutEnabled = mCaretEnabled;
  return NS_OK;
}

NS_IMETHODIMP PresShell::SetCaretVisibilityDuringSelection(bool aVisibility) {
  if (mCaret) mCaret->SetVisibilityDuringSelection(aVisibility);
  return NS_OK;
}

NS_IMETHODIMP PresShell::GetCaretVisible(bool* aOutIsVisible) {
  *aOutIsVisible = false;
  if (mCaret) {
    *aOutIsVisible = mCaret->IsVisible();
  }
  return NS_OK;
}

NS_IMETHODIMP PresShell::SetSelectionFlags(int16_t aFlags) {
  mSelectionFlags = aFlags;
  return NS_OK;
}

NS_IMETHODIMP PresShell::GetSelectionFlags(int16_t* aFlags) {
  if (!aFlags) {
    return NS_ERROR_INVALID_ARG;
  }

  *aFlags = mSelectionFlags;
  return NS_OK;
}

// implementation of nsISelectionController

NS_IMETHODIMP
PresShell::PhysicalMove(int16_t aDirection, int16_t aAmount, bool aExtend) {
  RefPtr<nsFrameSelection> frameSelection = mSelection;
  return frameSelection->PhysicalMove(aDirection, aAmount, aExtend);
}

NS_IMETHODIMP
PresShell::CharacterMove(bool aForward, bool aExtend) {
  RefPtr<nsFrameSelection> frameSelection = mSelection;
  return frameSelection->CharacterMove(aForward, aExtend);
}

NS_IMETHODIMP
PresShell::WordMove(bool aForward, bool aExtend) {
  RefPtr<nsFrameSelection> frameSelection = mSelection;
  nsresult result = frameSelection->WordMove(aForward, aExtend);
  // if we can't go down/up any more we must then move caret completely to
  // end/beginning respectively.
  if (NS_FAILED(result)) result = CompleteMove(aForward, aExtend);
  return result;
}

NS_IMETHODIMP
PresShell::LineMove(bool aForward, bool aExtend) {
  RefPtr<nsFrameSelection> frameSelection = mSelection;
  nsresult result = frameSelection->LineMove(aForward, aExtend);
  // if we can't go down/up any more we must then move caret completely to
  // end/beginning respectively.
  if (NS_FAILED(result)) result = CompleteMove(aForward, aExtend);
  return result;
}

NS_IMETHODIMP
PresShell::IntraLineMove(bool aForward, bool aExtend) {
  RefPtr<nsFrameSelection> frameSelection = mSelection;
  return frameSelection->IntraLineMove(aForward, aExtend);
}

NS_IMETHODIMP
PresShell::PageMove(bool aForward, bool aExtend) {
  nsIFrame* frame = nullptr;
  if (!aExtend) {
    frame = do_QueryFrame(
        GetScrollableFrameToScroll(ScrollableDirection::Vertical));
    // If there is no scrollable frame, get the frame to move caret instead.
  }
  if (!frame) {
    frame = mSelection->GetFrameToPageSelect();
    if (!frame) {
      return NS_OK;
    }
  }
  // We may scroll parent scrollable element of current selection limiter.
  // In such case, we don't want to scroll selection into view unless
  // selection is changed.
  RefPtr<nsFrameSelection> frameSelection = mSelection;
  return frameSelection->PageMove(
      aForward, aExtend, frame, nsFrameSelection::SelectionIntoView::IfChanged);
}

NS_IMETHODIMP
PresShell::ScrollPage(bool aForward) {
  nsIScrollableFrame* scrollFrame =
      GetScrollableFrameToScroll(ScrollableDirection::Vertical);
  if (scrollFrame) {
    scrollFrame->ScrollBy(nsIntPoint(0, aForward ? 1 : -1), ScrollUnit::PAGES,
                          ScrollMode::Smooth, nullptr, nullptr,
                          nsIScrollableFrame::NOT_MOMENTUM,
                          nsIScrollableFrame::ENABLE_SNAP);
  }
  return NS_OK;
}

NS_IMETHODIMP
PresShell::ScrollLine(bool aForward) {
  nsIScrollableFrame* scrollFrame =
      GetScrollableFrameToScroll(ScrollableDirection::Vertical);
  if (scrollFrame) {
    int32_t lineCount =
        Preferences::GetInt("toolkit.scrollbox.verticalScrollDistance",
                            NS_DEFAULT_VERTICAL_SCROLL_DISTANCE);
    scrollFrame->ScrollBy(nsIntPoint(0, aForward ? lineCount : -lineCount),
                          ScrollUnit::LINES, ScrollMode::Smooth, nullptr,
                          nullptr, nsIScrollableFrame::NOT_MOMENTUM,
                          nsIScrollableFrame::ENABLE_SNAP);
  }
  return NS_OK;
}

NS_IMETHODIMP
PresShell::ScrollCharacter(bool aRight) {
  nsIScrollableFrame* scrollFrame =
      GetScrollableFrameToScroll(ScrollableDirection::Horizontal);
  if (scrollFrame) {
    int32_t h =
        Preferences::GetInt("toolkit.scrollbox.horizontalScrollDistance",
                            NS_DEFAULT_HORIZONTAL_SCROLL_DISTANCE);
    scrollFrame->ScrollBy(nsIntPoint(aRight ? h : -h, 0), ScrollUnit::LINES,
                          ScrollMode::Smooth, nullptr, nullptr,
                          nsIScrollableFrame::NOT_MOMENTUM,
                          nsIScrollableFrame::ENABLE_SNAP);
  }
  return NS_OK;
}

NS_IMETHODIMP
PresShell::CompleteScroll(bool aForward) {
  nsIScrollableFrame* scrollFrame =
      GetScrollableFrameToScroll(ScrollableDirection::Vertical);
  if (scrollFrame) {
    scrollFrame->ScrollBy(nsIntPoint(0, aForward ? 1 : -1), ScrollUnit::WHOLE,
                          ScrollMode::Smooth, nullptr, nullptr,
                          nsIScrollableFrame::NOT_MOMENTUM,
                          nsIScrollableFrame::ENABLE_SNAP);
  }
  return NS_OK;
}

NS_IMETHODIMP
PresShell::CompleteMove(bool aForward, bool aExtend) {
  // Beware! This may flush notifications via synchronous
  // ScrollSelectionIntoView.
  RefPtr<nsFrameSelection> frameSelection = mSelection;
  nsIContent* limiter = frameSelection->GetAncestorLimiter();
  nsIFrame* frame = limiter ? limiter->GetPrimaryFrame()
                            : FrameConstructor()->GetRootElementFrame();
  if (!frame) return NS_ERROR_FAILURE;
  nsIFrame::CaretPosition pos = frame->GetExtremeCaretPosition(!aForward);

  const nsFrameSelection::FocusMode focusMode =
      aExtend ? nsFrameSelection::FocusMode::kExtendSelection
              : nsFrameSelection::FocusMode::kCollapseToNewPoint;
  frameSelection->HandleClick(
      pos.mResultContent, pos.mContentOffset, pos.mContentOffset, focusMode,
      aForward ? CARET_ASSOCIATE_AFTER : CARET_ASSOCIATE_BEFORE);
  if (limiter) {
    // HandleClick resets ancestorLimiter, so set it again.
    frameSelection->SetAncestorLimiter(limiter);
  }

  // After ScrollSelectionIntoView(), the pending notifications might be
  // flushed and PresShell/PresContext/Frames may be dead. See bug 418470.
  return ScrollSelectionIntoView(
      nsISelectionController::SELECTION_NORMAL,
      nsISelectionController::SELECTION_FOCUS_REGION,
      nsISelectionController::SCROLL_SYNCHRONOUS |
          nsISelectionController::SCROLL_FOR_CARET_MOVE);
}

NS_IMETHODIMP
PresShell::SelectAll() {
  RefPtr<nsFrameSelection> frameSelection = mSelection;
  return frameSelection->SelectAll();
}

static void DoCheckVisibility(nsPresContext* aPresContext, nsIContent* aNode,
                              int16_t aStartOffset, int16_t aEndOffset,
                              bool* aRetval) {
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
PresShell::CheckVisibility(nsINode* node, int16_t startOffset,
                           int16_t EndOffset, bool* _retval) {
  if (!node || startOffset > EndOffset || !_retval || startOffset < 0 ||
      EndOffset < 0)
    return NS_ERROR_INVALID_ARG;
  *_retval = false;  // initialize return parameter
  nsCOMPtr<nsIContent> content(do_QueryInterface(node));
  if (!content) return NS_ERROR_FAILURE;

  DoCheckVisibility(mPresContext, content, startOffset, EndOffset, _retval);
  return NS_OK;
}

nsresult PresShell::CheckVisibilityContent(nsIContent* aNode,
                                           int16_t aStartOffset,
                                           int16_t aEndOffset, bool* aRetval) {
  if (!aNode || aStartOffset > aEndOffset || !aRetval || aStartOffset < 0 ||
      aEndOffset < 0) {
    return NS_ERROR_INVALID_ARG;
  }

  *aRetval = false;
  DoCheckVisibility(mPresContext, aNode, aStartOffset, aEndOffset, aRetval);
  return NS_OK;
}

// end implementations nsISelectionController

nsIFrame* PresShell::GetRootScrollFrame() const {
  nsIFrame* rootFrame = mFrameConstructor->GetRootFrame();
  // Ensure root frame is a viewport frame
  if (!rootFrame || !rootFrame->IsViewportFrame()) return nullptr;
  nsIFrame* theFrame = rootFrame->PrincipalChildList().FirstChild();
  if (!theFrame || !theFrame->IsScrollFrame()) return nullptr;
  return theFrame;
}

nsIScrollableFrame* PresShell::GetRootScrollFrameAsScrollable() const {
  nsIFrame* frame = GetRootScrollFrame();
  if (!frame) return nullptr;
  nsIScrollableFrame* scrollableFrame = do_QueryFrame(frame);
  NS_ASSERTION(scrollableFrame,
               "All scroll frames must implement nsIScrollableFrame");
  return scrollableFrame;
}

nsPageSequenceFrame* PresShell::GetPageSequenceFrame() const {
  return mFrameConstructor->GetPageSequenceFrame();
}

nsCanvasFrame* PresShell::GetCanvasFrame() const {
  nsIFrame* frame = mFrameConstructor->GetDocElementContainingBlock();
  return do_QueryFrame(frame);
}

void PresShell::RestoreRootScrollPosition() {
  nsIScrollableFrame* scrollableFrame = GetRootScrollFrameAsScrollable();
  if (scrollableFrame) {
    scrollableFrame->ScrollToRestoredPosition();
  }
}

void PresShell::MaybeReleaseCapturingContent() {
  RefPtr<nsFrameSelection> frameSelection = FrameSelection();
  if (frameSelection) {
    frameSelection->SetDragState(false);
  }
  if (sCapturingContentInfo.mContent &&
      sCapturingContentInfo.mContent->OwnerDoc() == mDocument) {
    PresShell::ReleaseCapturingContent();
  }
}

void PresShell::BeginLoad(Document* aDocument) {
  mDocumentLoading = true;

  gfxTextPerfMetrics* tp = nullptr;
  if (mPresContext) {
    tp = mPresContext->GetTextPerfMetrics();
  }

  bool shouldLog = MOZ_LOG_TEST(gLog, LogLevel::Debug);
  if (shouldLog || tp) {
    mLoadBegin = TimeStamp::Now();
  }

  if (shouldLog) {
    nsIURI* uri = mDocument->GetDocumentURI();
    MOZ_LOG(gLog, LogLevel::Debug,
            ("(presshell) %p load begin [%s]\n", this,
             uri ? uri->GetSpecOrDefault().get() : ""));
  }
}

void PresShell::EndLoad(Document* aDocument) {
  MOZ_ASSERT(aDocument == mDocument, "Wrong document");

  RestoreRootScrollPosition();

  mDocumentLoading = false;
}

void PresShell::LoadComplete() {
  gfxTextPerfMetrics* tp = nullptr;
  if (mPresContext) {
    tp = mPresContext->GetTextPerfMetrics();
  }

  // log load
  bool shouldLog = MOZ_LOG_TEST(gLog, LogLevel::Debug);
  if (shouldLog || tp) {
    TimeDuration loadTime = TimeStamp::Now() - mLoadBegin;
    nsIURI* uri = mDocument->GetDocumentURI();
    nsAutoCString spec;
    if (uri) {
      spec = uri->GetSpecOrDefault();
    }
    if (shouldLog) {
      MOZ_LOG(gLog, LogLevel::Debug,
              ("(presshell) %p load done time-ms: %9.2f [%s]\n", this,
               loadTime.ToMilliseconds(), spec.get()));
    }
    if (tp) {
      tp->Accumulate();
      if (tp->cumulative.numChars > 0) {
        LogTextPerfStats(tp, this, tp->cumulative, loadTime.ToMilliseconds(),
                         eLog_loaddone, spec.get());
      }
    }
  }
}

#ifdef DEBUG
void PresShell::VerifyHasDirtyRootAncestor(nsIFrame* aFrame) {
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
    if ((aFrame->HasAnyStateBits(NS_FRAME_REFLOW_ROOT |
                                 NS_FRAME_DYNAMIC_REFLOW_ROOT) ||
         !aFrame->GetParent()) &&
        mDirtyRoots.Contains(aFrame)) {
      return;
    }

    aFrame = aFrame->GetParent();
  }

  MOZ_ASSERT_UNREACHABLE(
      "Frame has dirty bits set but isn't scheduled to be "
      "reflowed?");
}
#endif

void PresShell::PostPendingScrollAnchorSelection(
    mozilla::layout::ScrollAnchorContainer* aContainer) {
  mPendingScrollAnchorSelection.PutEntry(aContainer->ScrollableFrame());
}

void PresShell::FlushPendingScrollAnchorSelections() {
  for (auto iter = mPendingScrollAnchorSelection.Iter(); !iter.Done();
       iter.Next()) {
    nsIScrollableFrame* scroll = iter.Get()->GetKey();
    scroll->Anchor()->SelectAnchor();
  }
  mPendingScrollAnchorSelection.Clear();
}

void PresShell::PostPendingScrollAnchorAdjustment(
    ScrollAnchorContainer* aContainer) {
  mPendingScrollAnchorAdjustment.PutEntry(aContainer->ScrollableFrame());
}

void PresShell::FlushPendingScrollAnchorAdjustments() {
  for (auto iter = mPendingScrollAnchorAdjustment.Iter(); !iter.Done();
       iter.Next()) {
    nsIScrollableFrame* scroll = iter.Get()->GetKey();
    scroll->Anchor()->ApplyAdjustments();
  }
  mPendingScrollAnchorAdjustment.Clear();
}

void PresShell::FrameNeedsReflow(nsIFrame* aFrame,
                                 IntrinsicDirty aIntrinsicDirty,
                                 nsFrameState aBitToAdd,
                                 ReflowRootHandling aRootHandling) {
  MOZ_ASSERT(aBitToAdd == NS_FRAME_IS_DIRTY ||
                 aBitToAdd == NS_FRAME_HAS_DIRTY_CHILDREN || !aBitToAdd,
             "Unexpected bits being added");

  // FIXME bug 478135
  NS_ASSERTION(!(aIntrinsicDirty == IntrinsicDirty::StyleChange &&
                 aBitToAdd == NS_FRAME_HAS_DIRTY_CHILDREN),
               "bits don't correspond to style change reason");

  // FIXME bug 457400
  NS_ASSERTION(!mIsReflowing, "can't mark frame dirty during reflow");

  // If we've not yet done the initial reflow, then don't bother
  // enqueuing a reflow command yet.
  if (!mDidInitialize) return;

  // If we're already destroying, don't bother with this either.
  if (mIsDestroying) return;

#ifdef DEBUG
  // printf("gShellCounter: %d\n", gShellCounter++);
  if (mInVerifyReflow) return;

  if (VerifyReflowFlags::NoisyCommands & gVerifyReflowFlags) {
    printf("\nPresShell@%p: frame %p needs reflow\n", (void*)this,
           (void*)aFrame);
    if (VerifyReflowFlags::ReallyNoisyCommands & gVerifyReflowFlags) {
      printf("Current content model:\n");
      Element* rootElement = mDocument->GetRootElement();
      if (rootElement) {
        rootElement->List(stdout, 0);
      }
    }
  }
#endif

  AutoTArray<nsIFrame*, 4> subtrees;
  subtrees.AppendElement(aFrame);

  do {
    nsIFrame* subtreeRoot = subtrees.PopLastElement();

    // Grab |wasDirty| now so we can go ahead and update the bits on
    // subtreeRoot.
    bool wasDirty = NS_SUBTREE_DIRTY(subtreeRoot);
    subtreeRoot->AddStateBits(aBitToAdd);

    // Determine whether we need to keep looking for the next ancestor
    // reflow root if subtreeRoot itself is a reflow root.
    bool targetNeedsReflowFromParent;
    switch (aRootHandling) {
      case ReflowRootHandling::PositionOrSizeChange:
        targetNeedsReflowFromParent = true;
        break;
      case ReflowRootHandling::NoPositionOrSizeChange:
        targetNeedsReflowFromParent = false;
        break;
      case ReflowRootHandling::InferFromBitToAdd:
        targetNeedsReflowFromParent = (aBitToAdd == NS_FRAME_IS_DIRTY);
        break;
    }

#define FRAME_IS_REFLOW_ROOT(_f)                          \
  ((_f)->HasAnyStateBits(NS_FRAME_REFLOW_ROOT |           \
                         NS_FRAME_DYNAMIC_REFLOW_ROOT) && \
   ((_f) != subtreeRoot || !targetNeedsReflowFromParent))

    // Mark the intrinsic widths as dirty on the frame, all of its ancestors,
    // and all of its descendants, if needed:

    if (aIntrinsicDirty != IntrinsicDirty::Resize) {
      // Mark argument and all ancestors dirty. (Unless we hit a reflow
      // root that should contain the reflow.  That root could be
      // subtreeRoot itself if it's not dirty, or it could be some
      // ancestor of subtreeRoot.)
      for (nsIFrame* a = subtreeRoot; a && !FRAME_IS_REFLOW_ROOT(a);
           a = a->GetParent()) {
        a->MarkIntrinsicISizesDirty();
        if (a->HasAnyStateBits(NS_FRAME_OUT_OF_FLOW) &&
            a->IsAbsolutelyPositioned()) {
          // If we get here, 'a' is abspos, so its subtree's intrinsic sizing
          // has no effect on its ancestors' intrinsic sizing. So, don't loop
          // upwards any further.
          break;
        }
      }
    }

    const bool styleChange = (aIntrinsicDirty == IntrinsicDirty::StyleChange);
    const bool dirty = (aBitToAdd == NS_FRAME_IS_DIRTY);
    if (styleChange || dirty) {
      // Mark all descendants dirty (using an nsTArray stack rather than
      // recursion).
      // Note that ReflowInput::InitResizeFlags has some similar
      // code; see comments there for how and why it differs.
      AutoTArray<nsIFrame*, 32> stack;
      stack.AppendElement(subtreeRoot);

      do {
        nsIFrame* f = stack.PopLastElement();

        if (styleChange) {
          if (f->IsPlaceholderFrame()) {
            nsIFrame* oof = nsPlaceholderFrame::GetRealFrameForPlaceholder(f);
            if (!nsLayoutUtils::IsProperAncestorFrame(subtreeRoot, oof)) {
              // We have another distinct subtree we need to mark.
              subtrees.AppendElement(oof);
            }
          }
        }

        nsIFrame::ChildListIterator lists(f);
        for (; !lists.IsDone(); lists.Next()) {
          for (nsIFrame* kid : lists.CurrentList()) {
            if (styleChange) {
              kid->MarkIntrinsicISizesDirty();
            }
            if (dirty) {
              kid->AddStateBits(NS_FRAME_IS_DIRTY);
            }
            stack.AppendElement(kid);
          }
        }
      } while (stack.Length() != 0);
    }

    // Skip setting dirty bits up the tree if we weren't given a bit to add.
    if (!aBitToAdd) {
      continue;
    }

    // Set NS_FRAME_HAS_DIRTY_CHILDREN bits (via nsIFrame::ChildIsDirty)
    // up the tree until we reach either a frame that's already dirty or
    // a reflow root.
    nsIFrame* f = subtreeRoot;
    for (;;) {
      if (FRAME_IS_REFLOW_ROOT(f) || !f->GetParent()) {
        // we've hit a reflow root or the root frame
        if (!wasDirty) {
          mDirtyRoots.Add(f);
          SetNeedLayoutFlush();
        }
#ifdef DEBUG
        else {
          VerifyHasDirtyRootAncestor(f);
        }
#endif

        break;
      }

      nsIFrame* child = f;
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

void PresShell::FrameNeedsToContinueReflow(nsIFrame* aFrame) {
  NS_ASSERTION(mIsReflowing, "Must be in reflow when marking path dirty.");
  MOZ_ASSERT(mCurrentReflowRoot, "Must have a current reflow root here");
  NS_ASSERTION(
      aFrame == mCurrentReflowRoot ||
          nsLayoutUtils::IsProperAncestorFrame(mCurrentReflowRoot, aFrame),
      "Frame passed in is not the descendant of mCurrentReflowRoot");
  NS_ASSERTION(aFrame->GetStateBits() & NS_FRAME_IN_REFLOW,
               "Frame passed in not in reflow?");

  mFramesToDirty.PutEntry(aFrame);
}

already_AddRefed<nsIContent> PresShell::GetContentForScrolling() const {
  if (nsCOMPtr<nsIContent> focused = GetFocusedContentInOurWindow()) {
    return focused.forget();
  }
  return GetSelectedContentForScrolling();
}

already_AddRefed<nsIContent> PresShell::GetSelectedContentForScrolling() const {
  nsCOMPtr<nsIContent> selectedContent;
  if (mSelection) {
    Selection* domSelection = mSelection->GetSelection(SelectionType::eNormal);
    if (domSelection) {
      selectedContent =
          nsIContent::FromNodeOrNull(domSelection->GetFocusNode());
    }
  }
  return selectedContent.forget();
}

nsIScrollableFrame* PresShell::GetNearestScrollableFrame(
    nsIFrame* aFrame, ScrollableDirection aDirection) {
  if (aDirection == ScrollableDirection::Either) {
    return nsLayoutUtils::GetNearestScrollableFrame(aFrame);
  }

  return nsLayoutUtils::GetNearestScrollableFrameForDirection(
      aFrame, aDirection == ScrollableDirection::Vertical
                  ? nsLayoutUtils::eVertical
                  : nsLayoutUtils::eHorizontal);
}

nsIScrollableFrame* PresShell::GetScrollableFrameToScrollForContent(
    nsIContent* aContent, ScrollableDirection aDirection) {
  nsIScrollableFrame* scrollFrame = nullptr;
  if (aContent) {
    nsIFrame* startFrame = aContent->GetPrimaryFrame();
    if (startFrame) {
      scrollFrame = startFrame->GetScrollTargetFrame();
      if (scrollFrame) {
        startFrame = scrollFrame->GetScrolledFrame();
      }
      scrollFrame = GetNearestScrollableFrame(startFrame, aDirection);
    }
  }
  if (!scrollFrame) {
    scrollFrame = GetRootScrollFrameAsScrollable();
    if (!scrollFrame || !scrollFrame->GetScrolledFrame()) {
      return nullptr;
    }
    scrollFrame =
        GetNearestScrollableFrame(scrollFrame->GetScrolledFrame(), aDirection);
  }
  return scrollFrame;
}

nsIScrollableFrame* PresShell::GetScrollableFrameToScroll(
    ScrollableDirection aDirection) {
  nsCOMPtr<nsIContent> content = GetContentForScrolling();
  return GetScrollableFrameToScrollForContent(content.get(), aDirection);
}

void PresShell::CancelAllPendingReflows() {
  mDirtyRoots.Clear();

  if (mObservingLayoutFlushes) {
    GetPresContext()->RefreshDriver()->RemoveLayoutFlushObserver(this);
    mObservingLayoutFlushes = false;
  }

  ASSERT_REFLOW_SCHEDULED_STATE();
}

static bool DestroyFramesAndStyleDataFor(
    Element* aElement, nsPresContext& aPresContext,
    RestyleManager::IncludeRoot aIncludeRoot) {
  bool didReconstruct =
      aPresContext.FrameConstructor()->DestroyFramesFor(aElement);
  RestyleManager::ClearServoDataFromSubtree(aElement, aIncludeRoot);
  return didReconstruct;
}

void PresShell::SlotAssignmentWillChange(Element& aElement,
                                         HTMLSlotElement* aOldSlot,
                                         HTMLSlotElement* aNewSlot) {
  MOZ_ASSERT(aOldSlot != aNewSlot);

  if (MOZ_UNLIKELY(!mDidInitialize)) {
    return;
  }

  // If the old slot is about to become empty and show fallback, let layout know
  // that it needs to do work.
  if (aOldSlot && aOldSlot->AssignedNodes().Length() == 1 &&
      aOldSlot->HasChildren()) {
    DestroyFramesForAndRestyle(aOldSlot);
  }

  // Ensure the new element starts off clean.
  DestroyFramesAndStyleDataFor(&aElement, *mPresContext,
                               RestyleManager::IncludeRoot::Yes);

  if (aNewSlot) {
    // If the new slot will stop showing fallback content, we need to reframe it
    // altogether.
    if (aNewSlot->AssignedNodes().IsEmpty() && aNewSlot->HasChildren()) {
      DestroyFramesForAndRestyle(aNewSlot);
      // Otherwise we just care about the element, but we need to ensure that
      // something takes care of traversing to the relevant slot, if needed.
    } else if (aNewSlot->HasServoData() &&
               !Servo_Element_IsDisplayNone(aNewSlot)) {
      // Set the reframe bits...
      aNewSlot->NoteDescendantsNeedFramesForServo();
      aElement.SetFlags(NODE_NEEDS_FRAME);
      // Now the style dirty bits. Note that we can't just do
      // aElement.NoteDirtyForServo(), because the new slot is not setup yet.
      aNewSlot->SetHasDirtyDescendantsForServo();
      aNewSlot->NoteDirtySubtreeForServo();
    }
  }
}

#ifdef DEBUG
static void AssertNoFramesInSubtree(nsIContent* aContent) {
  for (nsINode* node : ShadowIncludingTreeIterator(*aContent)) {
    nsIContent* c = nsIContent::FromNode(node);
    MOZ_ASSERT(!c->GetPrimaryFrame());
  }
}
#endif

void PresShell::DestroyFramesForAndRestyle(Element* aElement) {
#ifdef DEBUG
  auto postCondition =
      mozilla::MakeScopeExit([&]() { AssertNoFramesInSubtree(aElement); });
#endif

  MOZ_ASSERT(aElement);
  if (MOZ_UNLIKELY(!mDidInitialize)) {
    return;
  }

  if (!aElement->GetFlattenedTreeParentNode()) {
    // Nothing to do here, the element already is out of the frame tree.
    return;
  }

  nsAutoScriptBlocker scriptBlocker;

  // Mark ourselves as not safe to flush while we're doing frame destruction.
  ++mChangeNestCount;

  const bool didReconstruct = FrameConstructor()->DestroyFramesFor(aElement);

  // Clear the style data from all the flattened tree descendants, but _not_
  // from us, since otherwise we wouldn't see the reframe.
  RestyleManager::ClearServoDataFromSubtree(aElement,
                                            RestyleManager::IncludeRoot::No);

  auto changeHint =
      didReconstruct ? nsChangeHint(0) : nsChangeHint_ReconstructFrame;

  mPresContext->RestyleManager()->PostRestyleEvent(
      aElement, RestyleHint::RestyleSubtree(), changeHint);

  --mChangeNestCount;
}

void PresShell::PostRecreateFramesFor(Element* aElement) {
  if (MOZ_UNLIKELY(!mDidInitialize)) {
    // Nothing to do here. In fact, if we proceed and aElement is the root, we
    // will crash.
    return;
  }

  mPresContext->RestyleManager()->PostRestyleEvent(
      aElement, RestyleHint{0}, nsChangeHint_ReconstructFrame);
}

void PresShell::RestyleForAnimation(Element* aElement, RestyleHint aHint) {
  // Now that we no longer have separate non-animation and animation
  // restyles, this method having a distinct identity is less important,
  // but it still seems useful to offer as a "more public" API and as a
  // chokepoint for these restyles to go through.
  mPresContext->RestyleManager()->PostRestyleEvent(aElement, aHint,
                                                   nsChangeHint(0));
}

void PresShell::SetForwardingContainer(const WeakPtr<nsDocShell>& aContainer) {
  mForwardingContainer = aContainer;
}

void PresShell::ClearFrameRefs(nsIFrame* aFrame) {
  mPresContext->EventStateManager()->ClearFrameRefs(aFrame);

  AutoWeakFrame* weakFrame = mAutoWeakFrames;
  while (weakFrame) {
    AutoWeakFrame* prev = weakFrame->GetPreviousWeakFrame();
    if (weakFrame->GetFrame() == aFrame) {
      // This removes weakFrame from mAutoWeakFrames.
      weakFrame->Clear(this);
    }
    weakFrame = prev;
  }

  AutoTArray<WeakFrame*, 4> toRemove;
  for (auto iter = mWeakFrames.Iter(); !iter.Done(); iter.Next()) {
    WeakFrame* weakFrame = iter.Get()->GetKey();
    if (weakFrame->GetFrame() == aFrame) {
      toRemove.AppendElement(weakFrame);
    }
  }
  for (WeakFrame* weakFrame : toRemove) {
    weakFrame->Clear(this);
  }
}

already_AddRefed<gfxContext> PresShell::CreateReferenceRenderingContext() {
  nsDeviceContext* devCtx = mPresContext->DeviceContext();
  RefPtr<gfxContext> rc;
  if (mPresContext->IsScreen()) {
    rc = gfxContext::CreateOrNull(
        gfxPlatform::GetPlatform()->ScreenReferenceDrawTarget().get());
  } else {
    // We assume the devCtx has positive width and height for this call.
    // However, width and height, may be outside of the reasonable range
    // so rc may still be null.
    rc = devCtx->CreateReferenceRenderingContext();
  }

  return rc ? rc.forget() : nullptr;
}

nsresult PresShell::GoToAnchor(const nsAString& aAnchorName, bool aScroll,
                               ScrollFlags aAdditionalScrollFlags) {
  if (!mDocument) {
    return NS_ERROR_FAILURE;
  }

  const Element* root = mDocument->GetRootElement();
  if (root && root->IsSVGElement(nsGkAtoms::svg)) {
    // We need to execute this even if there is an empty anchor name
    // so that any existing SVG fragment identifier effect is removed
    if (SVGFragmentIdentifier::ProcessFragmentIdentifier(mDocument,
                                                         aAnchorName)) {
      return NS_OK;
    }
  }

  // Hold a reference to the ESM in case event dispatch tears us down.
  RefPtr<EventStateManager> esm = mPresContext->EventStateManager();

  if (aAnchorName.IsEmpty()) {
    NS_ASSERTION(!aScroll, "can't scroll to empty anchor name");
    esm->SetContentState(nullptr, NS_EVENT_STATE_URLTARGET);
    return NS_OK;
  }

  nsresult rv = NS_OK;
  nsCOMPtr<nsIContent> content;

  // Search for an element with a matching "id" attribute
  if (mDocument) {
    content = mDocument->GetElementById(aAnchorName);
  }

  // Search for an anchor element with a matching "name" attribute
  if (!content && mDocument->IsHTMLDocument()) {
    // Find a matching list of named nodes
    nsCOMPtr<nsINodeList> list = mDocument->GetElementsByName(aAnchorName);
    if (list) {
      // Loop through the named nodes looking for the first anchor
      uint32_t length = list->Length();
      for (uint32_t i = 0; i < length; i++) {
        nsIContent* node = list->Item(i);
        if (node->IsHTMLElement(nsGkAtoms::a)) {
          content = node;
          break;
        }
      }
    }
  }

  // Search for anchor in the HTML namespace with a matching name
  if (!content && !mDocument->IsHTMLDocument()) {
    NS_NAMED_LITERAL_STRING(nameSpace, "http://www.w3.org/1999/xhtml");
    // Get the list of anchor elements
    nsCOMPtr<nsINodeList> list =
        mDocument->GetElementsByTagNameNS(nameSpace, NS_LITERAL_STRING("a"));
    // Loop through the anchors looking for the first one with the given name.
    for (uint32_t i = 0; true; i++) {
      nsIContent* node = list->Item(i);
      if (!node) {  // End of list
        break;
      }

      // Compare the name attribute
      if (node->IsElement() &&
          node->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::name,
                                         aAnchorName, eCaseMatters)) {
        content = node;
        break;
      }
    }
  }

  esm->SetContentState(content, NS_EVENT_STATE_URLTARGET);

#ifdef ACCESSIBILITY
  nsIContent* anchorTarget = content;
#endif

  nsIScrollableFrame* rootScroll = GetRootScrollFrameAsScrollable();
  if (rootScroll && rootScroll->DidHistoryRestore()) {
    // Scroll position restored from history trumps scrolling to anchor.
    aScroll = false;
    rootScroll->ClearDidHistoryRestore();
  }

  if (content) {
    if (aScroll) {
      rv = ScrollContentIntoView(
          content, ScrollAxis(kScrollToTop, WhenToScroll::Always), ScrollAxis(),
          ScrollFlags::AnchorScrollFlags | aAdditionalScrollFlags);
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
    RefPtr<nsRange> jumpToRange = nsRange::Create(mDocument);
    while (content && content->GetFirstChild()) {
      content = content->GetFirstChild();
    }
    jumpToRange->SelectNodeContents(*content, IgnoreErrors());
    // Select the anchor
    RefPtr<Selection> sel = mSelection->GetSelection(SelectionType::eNormal);
    if (sel) {
      sel->RemoveAllRanges(IgnoreErrors());
      sel->AddRangeAndSelectFramesAndNotifyListeners(*jumpToRange,
                                                     IgnoreErrors());
      if (!selectAnchor) {
        // Use a caret (collapsed selection) at the start of the anchor
        sel->CollapseToStart(IgnoreErrors());
      }
    }
    // Selection is at anchor.
    // Now focus the document itself if focus is on an element within it.
    nsPIDOMWindowOuter* win = mDocument->GetWindow();

    nsIFocusManager* fm = nsFocusManager::GetFocusManager();
    if (fm && win) {
      nsCOMPtr<mozIDOMWindowProxy> focusedWindow;
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
        sf->ScrollTo(nsPoint(0, 0), ScrollMode::Instant);
      }
    }
  }

#ifdef ACCESSIBILITY
  if (anchorTarget) {
    if (nsAccessibilityService* accService = GetAccessibilityService()) {
      accService->NotifyOfAnchorJumpTo(anchorTarget);
    }
  }
#endif  // #ifdef ACCESSIBILITY

  return rv;
}

nsresult PresShell::ScrollToAnchor() {
  nsCOMPtr<nsIContent> lastAnchor = std::move(mLastAnchorScrolledTo);
  if (!lastAnchor) {
    return NS_OK;
  }

  NS_ASSERTION(mDidInitialize, "should have done initial reflow by now");
  nsIScrollableFrame* rootScroll = GetRootScrollFrameAsScrollable();
  if (!rootScroll ||
      mLastAnchorScrollPositionY != rootScroll->GetScrollPosition().y) {
    return NS_OK;
  }
  return ScrollContentIntoView(lastAnchor,
                               ScrollAxis(kScrollToTop, WhenToScroll::Always),
                               ScrollAxis(), ScrollFlags::AnchorScrollFlags);
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
static void AccumulateFrameBounds(nsIFrame* aContainerFrame, nsIFrame* aFrame,
                                  bool aUseWholeLineHeightForInlines,
                                  nsRect& aRect, bool& aHaveRect,
                                  nsIFrame*& aPrevBlock,
                                  nsAutoLineIterator& aLines,
                                  int32_t& aCurLine) {
  nsIFrame* frame = aFrame;
  nsRect frameBounds = nsRect(nsPoint(0, 0), aFrame->GetSize());

  // If this is an inline frame and either the bounds height is 0 (quirks
  // layout model) or aUseWholeLineHeightForInlines is set, we need to
  // change the top of the bounds to include the whole line.
  if (frameBounds.height == 0 || aUseWholeLineHeightForInlines) {
    nsIFrame* prevFrame = aFrame;
    nsIFrame* f = aFrame;

    while (f && f->IsFrameOfType(nsIFrame::eLineParticipant) &&
           !f->IsTransformed() && !f->IsAbsPosContainingBlock()) {
      prevFrame = f;
      f = prevFrame->GetParent();
    }

    if (f != aFrame && f && f->IsBlockFrame()) {
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
          nsIFrame* trash1;
          int32_t trash2;
          nsRect lineBounds;

          if (NS_SUCCEEDED(
                  aLines->GetLine(index, &trash1, &trash2, lineBounds))) {
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

  nsRect transformedBounds = nsLayoutUtils::TransformFrameRectToAncestor(
      frame, frameBounds, aContainerFrame);

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

static bool ComputeNeedToScroll(WhenToScroll aWhenToScroll, nscoord aLineSize,
                                nscoord aRectMin, nscoord aRectMax,
                                nscoord aViewMin, nscoord aViewMax) {
  // See how the rect should be positioned vertically
  if (WhenToScroll::Always == aWhenToScroll) {
    // The caller wants the frame as visible as possible
    return true;
  } else if (WhenToScroll::IfNotVisible == aWhenToScroll) {
    // Scroll only if no part of the frame is visible in this view
    return aRectMax - aLineSize <= aViewMin || aRectMin + aLineSize >= aViewMax;
  } else if (WhenToScroll::IfNotFullyVisible == aWhenToScroll) {
    // Scroll only if part of the frame is hidden and more can fit in view
    return !(aRectMin >= aViewMin && aRectMax <= aViewMax) &&
           std::min(aViewMax, aRectMax) - std::max(aRectMin, aViewMin) <
               aViewMax - aViewMin;
  }
  return false;
}

static nscoord ComputeWhereToScroll(WhereToScroll aWhereToScroll,
                                    nscoord aOriginalCoord, nscoord aRectMin,
                                    nscoord aRectMax, nscoord aViewMin,
                                    nscoord aViewMax, nscoord* aRangeMin,
                                    nscoord* aRangeMax) {
  nscoord resultCoord = aOriginalCoord;
  nscoord scrollPortLength = aViewMax - aViewMin;
  if (kScrollMinimum == aWhereToScroll) {
    // Scroll the minimum amount necessary to show as much as possible of the
    // frame. If the frame is too large, don't hide any initially visible part
    // of it.
    nscoord min = std::min(aRectMin, aRectMax - scrollPortLength);
    nscoord max = std::max(aRectMin, aRectMax - scrollPortLength);
    resultCoord = std::min(std::max(aOriginalCoord, min), max);
  } else {
    nscoord frameAlignCoord = NSToCoordRound(
        aRectMin + (aRectMax - aRectMin) * (aWhereToScroll / 100.0f));
    resultCoord = NSToCoordRound(frameAlignCoord -
                                 scrollPortLength * (aWhereToScroll / 100.0f));
  }
  // Force the scroll range to extend to include resultCoord.
  *aRangeMin = std::min(resultCoord, aRectMax - scrollPortLength);
  *aRangeMax = std::max(resultCoord, aRectMin);
  return resultCoord;
}

/**
 * This function takes a scrollable frame, a rect in the coordinate system
 * of the scrolled frame, and a desired percentage-based scroll
 * position and attempts to scroll the rect to that position in the
 * visual viewport.
 *
 * This needs to work even if aRect has a width or height of zero.
 */
static void ScrollToShowRect(nsIScrollableFrame* aFrameAsScrollable,
                             const nsRect& aRect, ScrollAxis aVertical,
                             ScrollAxis aHorizontal, ScrollFlags aScrollFlags) {
  nsPoint scrollPt = aFrameAsScrollable->GetVisualViewportOffset();
  nsRect visibleRect(scrollPt, aFrameAsScrollable->GetVisualViewportSize());

  nsSize lineSize;
  // Don't call GetLineScrollAmount unless we actually need it. Not only
  // does this save time, but it's not safe to call GetLineScrollAmount
  // during reflow (because it depends on font size inflation and doesn't
  // use the in-reflow-safe font-size inflation path). If we did call it,
  // it would assert and possible give the wrong result.
  if (aVertical.mWhenToScroll == WhenToScroll::IfNotVisible ||
      aHorizontal.mWhenToScroll == WhenToScroll::IfNotVisible) {
    lineSize = aFrameAsScrollable->GetLineScrollAmount();
  }
  ScrollStyles ss = aFrameAsScrollable->GetScrollStyles();
  nsRect allowedRange(scrollPt, nsSize(0, 0));
  bool needToScroll = false;
  uint32_t directions = aFrameAsScrollable->GetAvailableScrollingDirections();

  if (((aScrollFlags & ScrollFlags::ScrollOverflowHidden) ||
       ss.mVertical != StyleOverflow::Hidden) &&
      (!aVertical.mOnlyIfPerceivedScrollableDirection ||
       (directions & nsIScrollableFrame::VERTICAL))) {
    if (ComputeNeedToScroll(aVertical.mWhenToScroll, lineSize.height, aRect.y,
                            aRect.YMost(), visibleRect.y,
                            visibleRect.YMost())) {
      nscoord maxHeight;
      scrollPt.y = ComputeWhereToScroll(
          aVertical.mWhereToScroll, scrollPt.y, aRect.y, aRect.YMost(),
          visibleRect.y, visibleRect.YMost(), &allowedRange.y, &maxHeight);
      allowedRange.height = maxHeight - allowedRange.y;
      needToScroll = true;
    }
  }

  if (((aScrollFlags & ScrollFlags::ScrollOverflowHidden) ||
       ss.mHorizontal != StyleOverflow::Hidden) &&
      (!aHorizontal.mOnlyIfPerceivedScrollableDirection ||
       (directions & nsIScrollableFrame::HORIZONTAL))) {
    if (ComputeNeedToScroll(aHorizontal.mWhenToScroll, lineSize.width, aRect.x,
                            aRect.XMost(), visibleRect.x,
                            visibleRect.XMost())) {
      nscoord maxWidth;
      scrollPt.x = ComputeWhereToScroll(
          aHorizontal.mWhereToScroll, scrollPt.x, aRect.x, aRect.XMost(),
          visibleRect.x, visibleRect.XMost(), &allowedRange.x, &maxWidth);
      allowedRange.width = maxWidth - allowedRange.x;
      needToScroll = true;
    }
  }

  // If we don't need to scroll, then don't try since it might cancel
  // a current smooth scroll operation.
  if (needToScroll) {
    ScrollMode scrollMode = ScrollMode::Instant;
    bool autoBehaviorIsSmooth = aFrameAsScrollable->IsSmoothScroll();
    bool smoothScroll = (aScrollFlags & ScrollFlags::ScrollSmooth) ||
                        ((aScrollFlags & ScrollFlags::ScrollSmoothAuto) &&
                         autoBehaviorIsSmooth);
    if (StaticPrefs::layout_css_scroll_behavior_enabled() && smoothScroll) {
      scrollMode = ScrollMode::SmoothMsd;
    }
    nsIFrame* frame = do_QueryFrame(aFrameAsScrollable);
    AutoWeakFrame weakFrame(frame);
    aFrameAsScrollable->ScrollTo(scrollPt, scrollMode, &allowedRange,
                                 aScrollFlags & ScrollFlags::ScrollSnap
                                     ? nsIScrollbarMediator::ENABLE_SNAP
                                     : nsIScrollbarMediator::DISABLE_SNAP);
    if (!weakFrame.IsAlive()) {
      return;
    }

    // If this is the RCD-RSF, also call ScrollToVisual() since we want to
    // scroll the rect into view visually, and that may require scrolling
    // the visual viewport in scenarios where there is not enough layout
    // scroll range.
    if (aFrameAsScrollable->IsRootScrollFrameOfDocument() &&
        frame->PresShell()->GetPresContext()->IsRootContentDocument()) {
      frame->PresShell()->ScrollToVisual(scrollPt, FrameMetrics::eMainThread,
                                         scrollMode);
    }
  }
}

nsresult PresShell::ScrollContentIntoView(nsIContent* aContent,
                                          ScrollAxis aVertical,
                                          ScrollAxis aHorizontal,
                                          ScrollFlags aScrollFlags) {
  NS_ENSURE_TRUE(aContent, NS_ERROR_NULL_POINTER);
  RefPtr<Document> composedDoc = aContent->GetComposedDoc();
  NS_ENSURE_STATE(composedDoc);

  NS_ASSERTION(mDidInitialize, "should have done initial reflow by now");

  if (mContentToScrollTo) {
    mContentToScrollTo->RemoveProperty(nsGkAtoms::scrolling);
  }
  mContentToScrollTo = aContent;
  ScrollIntoViewData* data = new ScrollIntoViewData();
  data->mContentScrollVAxis = aVertical;
  data->mContentScrollHAxis = aHorizontal;
  data->mContentToScrollToFlags = aScrollFlags;
  if (NS_FAILED(mContentToScrollTo->SetProperty(
          nsGkAtoms::scrolling, data,
          nsINode::DeleteProperty<PresShell::ScrollIntoViewData>))) {
    mContentToScrollTo = nullptr;
  }

  // Flush layout and attempt to scroll in the process.
  if (PresShell* presShell = composedDoc->GetPresShell()) {
    presShell->SetNeedLayoutFlush();
  }
  composedDoc->FlushPendingNotifications(FlushType::InterruptibleLayout);

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

void PresShell::DoScrollContentIntoView() {
  NS_ASSERTION(mDidInitialize, "should have done initial reflow by now");

  nsIFrame* frame = mContentToScrollTo->GetPrimaryFrame();
  if (!frame) {
    mContentToScrollTo->RemoveProperty(nsGkAtoms::scrolling);
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
  nsIFrame* container = nsLayoutUtils::GetClosestFrameOfType(
      frame->GetParent(), LayoutFrameType::Scroll);
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

  // Get the scroll-margin here since |frame| is going to be changed to iterate
  // over all continuation frames below.
  nsMargin scrollMargin;
  if (!(data->mContentToScrollToFlags & ScrollFlags::IgnoreMarginAndPadding)) {
    scrollMargin = frame->StyleMargin()->GetScrollMargin();
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
  bool useWholeLineHeightForInlines = data->mContentScrollVAxis.mWhenToScroll !=
                                      WhenToScroll::IfNotFullyVisible;
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

  frameBounds.Inflate(scrollMargin);

  ScrollFrameRectIntoView(container, frameBounds, data->mContentScrollVAxis,
                          data->mContentScrollHAxis,
                          data->mContentToScrollToFlags);
}

bool PresShell::ScrollFrameRectIntoView(nsIFrame* aFrame, const nsRect& aRect,
                                        ScrollAxis aVertical,
                                        ScrollAxis aHorizontal,
                                        ScrollFlags aScrollFlags) {
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
      // Inflate the scrolled rect by the container's padding in each dimension,
      // unless we have 'overflow-clip-box-*: content-box' in that dimension.
      auto* disp = container->StyleDisplay();
      if (disp->mOverflowClipBoxBlock == StyleOverflowClipBox::ContentBox ||
          disp->mOverflowClipBoxInline == StyleOverflowClipBox::ContentBox) {
        WritingMode wm = container->GetWritingMode();
        bool cbH = (wm.IsVertical() ? disp->mOverflowClipBoxBlock
                                    : disp->mOverflowClipBoxInline) ==
                   StyleOverflowClipBox::ContentBox;
        bool cbV = (wm.IsVertical() ? disp->mOverflowClipBoxInline
                                    : disp->mOverflowClipBoxBlock) ==
                   StyleOverflowClipBox::ContentBox;
        nsMargin padding = container->GetUsedPadding();
        if (!cbH) {
          padding.left = padding.right = nscoord(0);
        }
        if (!cbV) {
          padding.top = padding.bottom = nscoord(0);
        }
        targetRect.Inflate(padding);
      }

      targetRect -= sf->GetScrolledFrame()->GetPosition();
      if (!(aScrollFlags & ScrollFlags::IgnoreMarginAndPadding)) {
        nsMargin scrollPadding = sf->GetScrollPadding();
        targetRect.Inflate(scrollPadding);
        targetRect = targetRect.Intersect(sf->GetScrolledRect());
      }

      {
        AutoWeakFrame wf(container);
        ScrollToShowRect(sf, targetRect, aVertical, aHorizontal, aScrollFlags);
        if (!wf.IsAlive()) {
          return didScroll;
        }
      }

      nsPoint newPosition = sf->LastScrollDestination();
      // If the scroll position increased, that means our content moved up,
      // so our rect's offset should decrease
      rect += oldPosition - newPosition;

      if (oldPosition != newPosition) {
        didScroll = true;
      }

      // only scroll one container when this flag is set
      if (aScrollFlags & ScrollFlags::ScrollFirstAncestorOnly) {
        break;
      }
    }
    nsIFrame* parent;
    if (container->IsTransformed()) {
      container->GetTransformMatrix(ViewportType::Layout, RelativeTo{nullptr},
                                    &parent);
      rect =
          nsLayoutUtils::TransformFrameRectToAncestor(container, rect, parent);
    } else {
      rect += container->GetPosition();
      parent = container->GetParent();
    }
    if (!parent && !(aScrollFlags & ScrollFlags::ScrollNoParentFrames)) {
      nsPoint extraOffset(0, 0);
      int32_t APD = container->PresContext()->AppUnitsPerDevPixel();
      parent = nsLayoutUtils::GetCrossDocParentFrame(container, &extraOffset);
      if (parent) {
        int32_t parentAPD = parent->PresContext()->AppUnitsPerDevPixel();
        rect = rect.ScaleToOtherAppUnitsRoundOut(APD, parentAPD);
        rect += extraOffset;
      } else {
        nsCOMPtr<nsIDocShell> docShell =
            container->PresContext()->GetDocShell();
        if (BrowserChild* browserChild = BrowserChild::GetFrom(docShell)) {
          // Defer to the parent document if this is an out-of-process iframe.
          Unused << browserChild->SendScrollRectIntoView(
              rect, aVertical, aHorizontal, aScrollFlags, APD);
        }
      }
    }
    container = parent;
  } while (container);

  return didScroll;
}

void PresShell::ScheduleViewManagerFlush(PaintType aType) {
  if (MOZ_UNLIKELY(mIsDestroying)) {
    return;
  }

  if (aType == PaintType::DelayedCompress) {
    // Delay paint for 1 second.
    static const uint32_t kPaintDelayPeriod = 1000;
    if (!mDelayedPaintTimer) {
      nsTimerCallbackFunc PaintTimerCallBack = [](nsITimer* aTimer,
                                                  void* aClosure) {
        // The passed-in PresShell is always alive here. Because if PresShell
        // died, mDelayedPaintTimer->Cancel() would be called during the
        // destruction and this callback would never be invoked.
        auto self = static_cast<PresShell*>(aClosure);
        self->SetNextPaintCompressed();
        self->ScheduleViewManagerFlush();
      };

      NS_NewTimerWithFuncCallback(
          getter_AddRefs(mDelayedPaintTimer), PaintTimerCallBack, this,
          kPaintDelayPeriod, nsITimer::TYPE_ONE_SHOT, "PaintTimerCallBack",
          mDocument->EventTargetFor(TaskCategory::Other));
    }
    return;
  }

  nsPresContext* presContext = GetPresContext();
  if (presContext) {
    presContext->RefreshDriver()->ScheduleViewManagerFlush();
  }
  SetNeedLayoutFlush();
}

void PresShell::DispatchSynthMouseMove(WidgetGUIEvent* aEvent) {
  AUTO_PROFILER_TRACING_MARKER_DOCSHELL("Paint", "DispatchSynthMouseMove",
                                        GRAPHICS, mPresContext->GetDocShell());
  nsEventStatus status = nsEventStatus_eIgnore;
  nsView* targetView = nsView::GetViewFor(aEvent->mWidget);
  if (!targetView) return;
  RefPtr<nsViewManager> viewManager = targetView->GetViewManager();
  viewManager->DispatchEvent(aEvent, targetView, &status);
}

void PresShell::ClearMouseCaptureOnView(nsView* aView) {
  if (sCapturingContentInfo.mContent) {
    if (aView) {
      // if a view was specified, ensure that the captured content is within
      // this view.
      nsIFrame* frame = sCapturingContentInfo.mContent->GetPrimaryFrame();
      if (frame) {
        nsView* view = frame->GetClosestView();
        // if there is no view, capturing won't be handled any more, so
        // just release the capture.
        if (view) {
          do {
            if (view == aView) {
              sCapturingContentInfo.mContent = nullptr;
              // the view containing the captured content likely disappeared so
              // disable capture for now.
              sCapturingContentInfo.mAllowed = false;
              break;
            }

            view = view->GetParent();
          } while (view);
          // return if the view wasn't found
          return;
        }
      }
    }

    sCapturingContentInfo.mContent = nullptr;
  }

  // disable mouse capture until the next mousedown as a dialog has opened
  // or a drag has started. Otherwise, someone could start capture during
  // the modal dialog or drag.
  sCapturingContentInfo.mAllowed = false;
}

void PresShell::ClearMouseCapture(nsIFrame* aFrame) {
  if (!sCapturingContentInfo.mContent) {
    sCapturingContentInfo.mAllowed = false;
    return;
  }

  // null frame argument means clear the capture
  if (!aFrame) {
    sCapturingContentInfo.mContent = nullptr;
    sCapturingContentInfo.mAllowed = false;
    return;
  }

  nsIFrame* capturingFrame = sCapturingContentInfo.mContent->GetPrimaryFrame();
  if (!capturingFrame) {
    sCapturingContentInfo.mContent = nullptr;
    sCapturingContentInfo.mAllowed = false;
    return;
  }

  if (nsLayoutUtils::IsAncestorFrameCrossDoc(aFrame, capturingFrame)) {
    sCapturingContentInfo.mContent = nullptr;
    sCapturingContentInfo.mAllowed = false;
  }
}

nsresult PresShell::CaptureHistoryState(nsILayoutHistoryState** aState) {
  MOZ_ASSERT(nullptr != aState, "null state pointer");

  // We actually have to mess with the docshell here, since we want to
  // store the state back in it.
  // XXXbz this isn't really right, since this is being called in the
  // content viewer's Hide() method...  by that point the docshell's
  // state could be wrong.  We should sort out a better ownership
  // model for the layout history state.
  nsCOMPtr<nsIDocShell> docShell(mPresContext->GetDocShell());
  if (!docShell) return NS_ERROR_FAILURE;

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

void PresShell::ScheduleBeforeFirstPaint() {
  if (!mDocument->IsResourceDoc()) {
    // Notify observers that a new page is about to be drawn. Execute this
    // as soon as it is safe to run JS, which is guaranteed to be before we
    // go back to the event loop and actually draw the page.
    MOZ_LOG(gLog, LogLevel::Debug,
            ("PresShell::ScheduleBeforeFirstPaint this=%p", this));

    nsContentUtils::AddScriptRunner(
        new nsBeforeFirstPaintDispatcher(mDocument));
  }
}

void PresShell::UnsuppressAndInvalidate() {
  // Note: We ignore the EnsureVisible check for resource documents, because
  // they won't have a docshell, so they'll always fail EnsureVisible.
  if ((!mDocument->IsResourceDoc() && !mPresContext->EnsureVisible()) ||
      mHaveShutDown) {
    // No point; we're about to be torn down anyway.
    return;
  }

  ScheduleBeforeFirstPaint();

  mPaintingSuppressed = false;
  nsIFrame* rootFrame = mFrameConstructor->GetRootFrame();
  if (rootFrame) {
    // let's assume that outline on a root frame is not supported
    rootFrame->InvalidateFrame();
  }

  // now that painting is unsuppressed, focus may be set on the document
  if (nsPIDOMWindowOuter* win = mDocument->GetWindow()) win->SetReadyForFocus();

  if (!mHaveShutDown) {
    SynthesizeMouseMove(false);
    ScheduleApproximateFrameVisibilityUpdateNow();
  }
}

void PresShell::UnsuppressPainting() {
  if (mPaintSuppressionTimer) {
    mPaintSuppressionTimer->Cancel();
    mPaintSuppressionTimer = nullptr;
  }

  if (mIsDocumentGone || !mPaintingSuppressed) return;

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
nsresult PresShell::PostReflowCallback(nsIReflowCallback* aCallback) {
  void* result = AllocateByObjectID(eArenaObjectID_nsCallbackEventRequest,
                                    sizeof(nsCallbackEventRequest));
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

void PresShell::CancelReflowCallback(nsIReflowCallback* aCallback) {
  nsCallbackEventRequest* before = nullptr;
  nsCallbackEventRequest* node = mFirstCallbackEventRequest;
  while (node) {
    nsIReflowCallback* callback = node->callback;

    if (callback == aCallback) {
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

      FreeByObjectID(eArenaObjectID_nsCallbackEventRequest, toFree);
    } else {
      before = node;
      node = node->next;
    }
  }
}

void PresShell::CancelPostedReflowCallbacks() {
  while (mFirstCallbackEventRequest) {
    nsCallbackEventRequest* node = mFirstCallbackEventRequest;
    mFirstCallbackEventRequest = node->next;
    if (!mFirstCallbackEventRequest) {
      mLastCallbackEventRequest = nullptr;
    }
    nsIReflowCallback* callback = node->callback;
    FreeByObjectID(eArenaObjectID_nsCallbackEventRequest, node);
    if (callback) {
      callback->ReflowCallbackCanceled();
    }
  }
}

void PresShell::HandlePostedReflowCallbacks(bool aInterruptible) {
  bool shouldFlush = false;

  while (mFirstCallbackEventRequest) {
    nsCallbackEventRequest* node = mFirstCallbackEventRequest;
    mFirstCallbackEventRequest = node->next;
    if (!mFirstCallbackEventRequest) {
      mLastCallbackEventRequest = nullptr;
    }
    nsIReflowCallback* callback = node->callback;
    FreeByObjectID(eArenaObjectID_nsCallbackEventRequest, node);
    if (callback) {
      if (callback->ReflowFinished()) {
        shouldFlush = true;
      }
    }
  }

  FlushType flushType =
      aInterruptible ? FlushType::InterruptibleLayout : FlushType::Layout;
  if (shouldFlush && !mIsDestroying) {
    FlushPendingNotifications(flushType);
  }
}

bool PresShell::IsSafeToFlush() const {
  // Not safe if we are getting torn down, reflowing, or in the middle of frame
  // construction.
  if (mIsReflowing || mChangeNestCount || mIsDestroying) {
    return false;
  }

  // Not safe if we are painting
  if (nsViewManager* viewManager = GetViewManager()) {
    bool isPainting = false;
    viewManager->IsPainting(isPainting);
    if (isPainting) {
      return false;
    }
  }

  return true;
}

void PresShell::NotifyFontFaceSetOnRefresh() {
  if (FontFaceSet* set = mDocument->GetFonts()) {
    set->DidRefresh();
  }
}

void PresShell::DoFlushPendingNotifications(FlushType aType) {
  // by default, flush animations if aType >= FlushType::Style
  mozilla::ChangesToFlush flush(aType, aType >= FlushType::Style);
  FlushPendingNotifications(flush);
}

#ifdef DEBUG
static void AssertFrameSubtreeIsSane(const nsIFrame& aRoot) {
  if (const nsIContent* content = aRoot.GetContent()) {
    MOZ_ASSERT(content->GetFlattenedTreeParentNodeForStyle(),
               "Node not in the flattened tree still has a frame?");
  }

  nsIFrame::ChildListIterator childLists(&aRoot);
  for (; !childLists.IsDone(); childLists.Next()) {
    for (const nsIFrame* child : childLists.CurrentList()) {
      AssertFrameSubtreeIsSane(*child);
    }
  }
}
#endif

static inline void AssertFrameTreeIsSane(const PresShell& aPresShell) {
#ifdef DEBUG
  if (const nsIFrame* root = aPresShell.GetRootFrame()) {
    AssertFrameSubtreeIsSane(*root);
  }
#endif
}

void PresShell::DoFlushPendingNotifications(mozilla::ChangesToFlush aFlush) {
  // FIXME(emilio, bug 1530177): Turn into a release assert when bug 1530188 and
  // bug 1530190 are fixed.
  MOZ_DIAGNOSTIC_ASSERT(!mForbiddenToFlush, "This is bad!");

  // Per our API contract, hold a strong ref to ourselves until we return.
  RefPtr<PresShell> kungFuDeathGrip = this;

  /**
   * VERY IMPORTANT: If you add some sort of new flushing to this
   * method, make sure to add the relevant SetNeedLayoutFlush or
   * SetNeedStyleFlush calls on the shell.
   */
  FlushType flushType = aFlush.mFlushType;

  MOZ_ASSERT(NeedFlush(flushType), "Why did we get called?");

#ifdef MOZ_GECKO_PROFILER
  // clang-format off
  static const EnumeratedArray<FlushType, FlushType::Count, const char*>
      flushTypeNames = {
    "",
    "Event",
    "Content",
    "ContentAndNotify",
    "Style",
    // As far as the profiler is concerned, EnsurePresShellInitAndFrames and
    // Frames are the same
    "Style",
    "Style",
    "InterruptibleLayout",
    "Layout",
    "Display"
  };
  // clang-format on
  AUTO_PROFILER_LABEL_DYNAMIC_CSTR_NONSENSITIVE(
      "PresShell::DoFlushPendingNotifications", LAYOUT,
      flushTypeNames[flushType]);
#endif

#ifdef ACCESSIBILITY
#  ifdef DEBUG
  if (nsAccessibilityService* accService = GetAccService()) {
    NS_ASSERTION(!accService->IsProcessingRefreshDriverNotification(),
                 "Flush during accessible tree update!");
  }
#  endif
#endif

  NS_ASSERTION(flushType >= FlushType::Style, "Why did we get called?");

  mNeedStyleFlush = false;
  mNeedThrottledAnimationFlush =
      mNeedThrottledAnimationFlush && !aFlush.mFlushAnimations;
  mNeedLayoutFlush =
      mNeedLayoutFlush && (flushType < FlushType::InterruptibleLayout);

  bool isSafeToFlush = IsSafeToFlush();

  // If layout could possibly trigger scripts, then it's only safe to flush if
  // it's safe to run script.
  bool hasHadScriptObject;
  if (mDocument->GetScriptHandlingObject(hasHadScriptObject) ||
      hasHadScriptObject) {
    isSafeToFlush = isSafeToFlush && nsContentUtils::IsSafeToRunScript();
  }

  // Don't flush if the doc is already in the bfcache.
  if (MOZ_UNLIKELY(mDocument->GetPresShell() != this)) {
    MOZ_DIAGNOSTIC_ASSERT(!mDocument->GetPresShell(),
                          "Where did this shell come from?");
    isSafeToFlush = false;
  }

  MOZ_DIAGNOSTIC_ASSERT(!mIsDestroying || !isSafeToFlush);
  MOZ_DIAGNOSTIC_ASSERT(mIsDestroying || mViewManager);
  MOZ_DIAGNOSTIC_ASSERT(mIsDestroying || mDocument->HasShellOrBFCacheEntry());

  // Make sure the view manager stays alive.
  RefPtr<nsViewManager> viewManager = mViewManager;
  bool didStyleFlush = false;
  bool didLayoutFlush = false;
  if (isSafeToFlush) {
    // Record that we are in a flush, so that our optimization in
    // Document::FlushPendingNotifications doesn't skip any re-entrant
    // calls to us.  Otherwise, we might miss some needed flushes, since
    // we clear mNeedStyleFlush / mNeedLayoutFlush here at the top of
    // the function but we might not have done the work yet.
    AutoRestore<bool> guard(mInFlush);
    mInFlush = true;

    // We need to make sure external resource documents are flushed too (for
    // example, svg filters that reference a filter in an external document
    // need the frames in the external document to be constructed for the
    // filter to work). We only need external resources to be flushed when the
    // main document is flushing >= FlushType::Frames, so we flush external
    // resources here instead of Document::FlushPendingNotifications.
    mDocument->FlushExternalResources(flushType);

    // Force flushing of any pending content notifications that might have
    // queued up while our event was pending.  That will ensure that we don't
    // construct frames for content right now that's still waiting to be
    // notified on,
    mDocument->FlushPendingNotifications(FlushType::ContentAndNotify);

    mDocument->UpdateSVGUseElementShadowTrees();

    // Process pending restyles, since any flush of the presshell wants
    // up-to-date style data.
    if (MOZ_LIKELY(!mIsDestroying)) {
      viewManager->FlushDelayedResize(false);
      mPresContext->FlushPendingMediaFeatureValuesChanged();
    }

    if (MOZ_LIKELY(!mIsDestroying)) {
      // Now that we have flushed media queries, update the rules before looking
      // up @font-face / @counter-style / @font-feature-values rules.
      StyleSet()->UpdateStylistIfNeeded();

      // Flush any pending update of the user font set, since that could
      // cause style changes (for updating ex/ch units, and to cause a
      // reflow).
      mDocument->FlushUserFontSet();

      mPresContext->FlushCounterStyles();

      mPresContext->FlushFontFeatureValues();

      // Flush any requested SMIL samples.
      if (mDocument->HasAnimationController()) {
        mDocument->GetAnimationController()->FlushResampleRequests();
      }

      if (aFlush.mFlushAnimations && mPresContext->EffectCompositor()) {
        mPresContext->EffectCompositor()->PostRestyleForThrottledAnimations();
      }
    }

    // The FlushResampleRequests() above flushed style changes.
    if (MOZ_LIKELY(!mIsDestroying)) {
      nsAutoScriptBlocker scriptBlocker;
#ifdef MOZ_GECKO_PROFILER
      Maybe<uint64_t> innerWindowID;
      if (auto* window = mDocument->GetInnerWindow()) {
        innerWindowID = Some(window->WindowID());
      }
      AutoProfilerStyleMarker tracingStyleFlush(std::move(mStyleCause),
                                                innerWindowID);
#endif
      PerfStats::AutoMetricRecording<PerfStats::Metric::Styling> autoRecording;
      LAYOUT_TELEMETRY_RECORD_BASE(Restyle);

      mPresContext->RestyleManager()->ProcessPendingRestyles();
    }

    // Now those constructors or events might have posted restyle
    // events.  At the same time, we still need up-to-date style data.
    // In particular, reflow depends on style being completely up to
    // date.  If it's not, then style reparenting, which can
    // happen during reflow, might suddenly pick up the new rules and
    // we'll end up with frames whose style doesn't match the frame
    // type.
    if (MOZ_LIKELY(!mIsDestroying)) {
      nsAutoScriptBlocker scriptBlocker;
#ifdef MOZ_GECKO_PROFILER
      Maybe<uint64_t> innerWindowID;
      if (auto* window = mDocument->GetInnerWindow()) {
        innerWindowID = Some(window->WindowID());
      }
      AutoProfilerStyleMarker tracingStyleFlush(std::move(mStyleCause),
                                                innerWindowID);
#endif
      PerfStats::AutoMetricRecording<PerfStats::Metric::Styling> autoRecording;
      LAYOUT_TELEMETRY_RECORD_BASE(Restyle);

      mPresContext->RestyleManager()->ProcessPendingRestyles();
      // Clear mNeedStyleFlush here agagin to make this flag work properly for
      // optimization since the flag might have set in ProcessPendingRestyles().
      mNeedStyleFlush = false;
    }

    AssertFrameTreeIsSane(*this);

    didStyleFlush = true;

    // There might be more pending constructors now, but we're not going to
    // worry about them.  They can't be triggered during reflow, so we should
    // be good.

    if (flushType >= (SuppressInterruptibleReflows()
                          ? FlushType::Layout
                          : FlushType::InterruptibleLayout) &&
        !mIsDestroying) {
      didLayoutFlush = true;
      mFrameConstructor->RecalcQuotesAndCounters();
      if (ProcessReflowCommands(flushType < FlushType::Layout)) {
        if (mContentToScrollTo) {
          DoScrollContentIntoView();
          if (mContentToScrollTo) {
            mContentToScrollTo->RemoveProperty(nsGkAtoms::scrolling);
            mContentToScrollTo = nullptr;
          }
        }
      }
    }

    if (flushType >= FlushType::Layout) {
      if (!mIsDestroying) {
        viewManager->UpdateWidgetGeometry();
      }
    }
  }

  if (!didStyleFlush && flushType >= FlushType::Style && !mIsDestroying) {
    SetNeedStyleFlush();
    if (aFlush.mFlushAnimations) {
      SetNeedThrottledAnimationFlush();
    }
  }

  if (!didLayoutFlush && flushType >= FlushType::InterruptibleLayout &&
      !mIsDestroying) {
    // We suppressed this flush either due to it not being safe to flush,
    // or due to SuppressInterruptibleReflows().  Either way, the
    // mNeedLayoutFlush flag needs to be re-set.
    SetNeedLayoutFlush();
  }

  // Update flush counters
  if (didStyleFlush) {
    mLayoutTelemetry.IncReqsPerFlush(FlushType::Style);
  }

  if (didLayoutFlush) {
    mLayoutTelemetry.IncReqsPerFlush(FlushType::Layout);
  }

  // Record telemetry for the number of requests per each flush type.
  //
  // Flushes happen as style or style+layout. This depends upon the `flushType`
  // where flushType >= InterruptibleLayout means flush layout and flushType >=
  // Style means flush style. We only report if didLayoutFlush or didStyleFlush
  // is true because we care if a flush really did take place. (Flush is guarded
  // by `isSafeToFlush == true`.)
  if (flushType >= FlushType::InterruptibleLayout && didLayoutFlush) {
    MOZ_ASSERT(didLayoutFlush == didStyleFlush);
    mLayoutTelemetry.PingReqsPerFlushTelemetry(FlushType::Layout);
  } else if (flushType >= FlushType::Style && didStyleFlush) {
    MOZ_ASSERT(!didLayoutFlush);
    mLayoutTelemetry.PingReqsPerFlushTelemetry(FlushType::Style);
  }
}

MOZ_CAN_RUN_SCRIPT_BOUNDARY void PresShell::CharacterDataChanged(
    nsIContent* aContent, const CharacterDataChangeInfo& aInfo) {
  MOZ_ASSERT(!nsContentUtils::IsSafeToRunScript());
  MOZ_ASSERT(!mIsDocumentGone, "Unexpected CharacterDataChanged");
  MOZ_ASSERT(aContent->OwnerDoc() == mDocument, "Unexpected document");

  nsAutoCauseReflowNotifier crNotifier(this);

  mPresContext->RestyleManager()->CharacterDataChanged(aContent, aInfo);
  mFrameConstructor->CharacterDataChanged(aContent, aInfo);
}

MOZ_CAN_RUN_SCRIPT_BOUNDARY void PresShell::ContentStateChanged(
    Document* aDocument, nsIContent* aContent, EventStates aStateMask) {
  MOZ_ASSERT(!nsContentUtils::IsSafeToRunScript());
  MOZ_ASSERT(!mIsDocumentGone, "Unexpected ContentStateChanged");
  MOZ_ASSERT(aDocument == mDocument, "Unexpected aDocument");

  if (mDidInitialize) {
    nsAutoCauseReflowNotifier crNotifier(this);
    mPresContext->RestyleManager()->ContentStateChanged(aContent, aStateMask);
  }
}

void PresShell::DocumentStatesChanged(EventStates aStateMask) {
  MOZ_ASSERT(!mIsDocumentGone, "Unexpected DocumentStatesChanged");
  MOZ_ASSERT(mDocument);
  MOZ_ASSERT(!aStateMask.IsEmpty());

  if (mDidInitialize) {
    StyleSet()->InvalidateStyleForDocumentStateChanges(aStateMask);
  }

  if (aStateMask.HasState(NS_DOCUMENT_STATE_WINDOW_INACTIVE)) {
    if (nsIFrame* root = mFrameConstructor->GetRootFrame()) {
      root->SchedulePaint();
    }
  }
}

MOZ_CAN_RUN_SCRIPT_BOUNDARY void PresShell::AttributeWillChange(
    Element* aElement, int32_t aNameSpaceID, nsAtom* aAttribute,
    int32_t aModType) {
  MOZ_ASSERT(!nsContentUtils::IsSafeToRunScript());
  MOZ_ASSERT(!mIsDocumentGone, "Unexpected AttributeWillChange");
  MOZ_ASSERT(aElement->OwnerDoc() == mDocument, "Unexpected document");

  // XXXwaterson it might be more elegant to wait until after the
  // initial reflow to begin observing the document. That would
  // squelch any other inappropriate notifications as well.
  if (mDidInitialize) {
    nsAutoCauseReflowNotifier crNotifier(this);
    mPresContext->RestyleManager()->AttributeWillChange(aElement, aNameSpaceID,
                                                        aAttribute, aModType);
  }
}

MOZ_CAN_RUN_SCRIPT_BOUNDARY void PresShell::AttributeChanged(
    Element* aElement, int32_t aNameSpaceID, nsAtom* aAttribute,
    int32_t aModType, const nsAttrValue* aOldValue) {
  MOZ_ASSERT(!nsContentUtils::IsSafeToRunScript());
  MOZ_ASSERT(!mIsDocumentGone, "Unexpected AttributeChanged");
  MOZ_ASSERT(aElement->OwnerDoc() == mDocument, "Unexpected document");

  // XXXwaterson it might be more elegant to wait until after the
  // initial reflow to begin observing the document. That would
  // squelch any other inappropriate notifications as well.
  if (mDidInitialize) {
    nsAutoCauseReflowNotifier crNotifier(this);
    mPresContext->RestyleManager()->AttributeChanged(
        aElement, aNameSpaceID, aAttribute, aModType, aOldValue);
  }
}

MOZ_CAN_RUN_SCRIPT_BOUNDARY void PresShell::ContentAppended(
    nsIContent* aFirstNewContent) {
  MOZ_ASSERT(!nsContentUtils::IsSafeToRunScript());
  MOZ_ASSERT(!mIsDocumentGone, "Unexpected ContentAppended");
  MOZ_ASSERT(aFirstNewContent->OwnerDoc() == mDocument, "Unexpected document");

  // We never call ContentAppended with a document as the container, so we can
  // assert that we have an nsIContent parent.
  MOZ_ASSERT(aFirstNewContent->GetParent());
  MOZ_ASSERT(aFirstNewContent->GetParent()->IsElement() ||
             aFirstNewContent->GetParent()->IsShadowRoot());

  if (!mDidInitialize) {
    return;
  }

  nsAutoCauseReflowNotifier crNotifier(this);

  // Call this here so it only happens for real content mutations and
  // not cases when the frame constructor calls its own methods to force
  // frame reconstruction.
  mPresContext->RestyleManager()->ContentAppended(aFirstNewContent);

  mFrameConstructor->ContentAppended(
      aFirstNewContent, nsCSSFrameConstructor::InsertionKind::Async);
}

MOZ_CAN_RUN_SCRIPT_BOUNDARY void PresShell::ContentInserted(
    nsIContent* aChild) {
  MOZ_ASSERT(!nsContentUtils::IsSafeToRunScript());
  MOZ_ASSERT(!mIsDocumentGone, "Unexpected ContentInserted");
  MOZ_ASSERT(aChild->OwnerDoc() == mDocument, "Unexpected document");

  if (!mDidInitialize) {
    return;
  }

  nsAutoCauseReflowNotifier crNotifier(this);

  // Call this here so it only happens for real content mutations and
  // not cases when the frame constructor calls its own methods to force
  // frame reconstruction.
  mPresContext->RestyleManager()->ContentInserted(aChild);

  mFrameConstructor->ContentInserted(
      aChild, nsCSSFrameConstructor::InsertionKind::Async);
}

MOZ_CAN_RUN_SCRIPT_BOUNDARY void PresShell::ContentRemoved(
    nsIContent* aChild, nsIContent* aPreviousSibling) {
  MOZ_ASSERT(!nsContentUtils::IsSafeToRunScript());
  MOZ_ASSERT(!mIsDocumentGone, "Unexpected ContentRemoved");
  MOZ_ASSERT(aChild->OwnerDoc() == mDocument, "Unexpected document");
  nsINode* container = aChild->GetParentNode();

  // Notify the ESM that the content has been removed, so that
  // it can clean up any state related to the content.

  mPresContext->EventStateManager()->ContentRemoved(mDocument, aChild);

  nsAutoCauseReflowNotifier crNotifier(this);

  // Call this here so it only happens for real content mutations and
  // not cases when the frame constructor calls its own methods to force
  // frame reconstruction.
  nsIContent* oldNextSibling = nullptr;

  // Editor calls into here with NAC via HTMLEditor::DeleteRefToAnonymousNode.
  // This could be asserted if that caller is fixed.
  if (MOZ_LIKELY(!aChild->IsRootOfAnonymousSubtree())) {
    oldNextSibling = aPreviousSibling ? aPreviousSibling->GetNextSibling()
                                      : container->GetFirstChild();
  }

  // After removing aChild from tree we should save information about live
  // ancestor
  if (mPointerEventTarget &&
      mPointerEventTarget->IsInclusiveDescendantOf(aChild)) {
    mPointerEventTarget = aChild->GetParent();
  }

  mFrameConstructor->ContentRemoved(aChild, oldNextSibling,
                                    nsCSSFrameConstructor::REMOVE_CONTENT);

  // NOTE(emilio): It's important that this goes after the frame constructor
  // stuff, otherwise the frame constructor can't see elements which are
  // display: contents / display: none, because we'd have cleared all the style
  // data from there.
  mPresContext->RestyleManager()->ContentRemoved(aChild, oldNextSibling);
}

void PresShell::NotifyCounterStylesAreDirty() {
  // TODO: Looks like that nsFrameConstructor::NotifyCounterStylesAreDirty()
  //       does not run script.  If so, we don't need to block script with
  //       nsAutoCauseReflowNotifier here.  Instead, there should be methods
  //       and stack only class which manages only mChangeNestCount for
  //       avoiding unnecessary `MOZ_CAN_RUN_SCRIPT` marking.
  nsAutoCauseReflowNotifier reflowNotifier(this);
  mFrameConstructor->NotifyCounterStylesAreDirty();
}

bool PresShell::FrameIsAncestorOfDirtyRoot(nsIFrame* aFrame) const {
  return mDirtyRoots.FrameIsAncestorOfDirtyRoot(aFrame);
}

void PresShell::ReconstructFrames() {
  MOZ_ASSERT(!mFrameConstructor->GetRootFrame() || mDidInitialize,
             "Must not have root frame before initial reflow");
  if (!mDidInitialize || mIsDestroying) {
    // Nothing to do here
    return;
  }

  // Have to make sure that the content notifications are flushed before we
  // start messing with the frame model; otherwise we can get content doubling.
  //
  // Also make sure that styles are flushed before calling into the frame
  // constructor, since that's what it expects.
  mDocument->FlushPendingNotifications(FlushType::Style);

  if (mIsDestroying) {
    return;
  }

  nsAutoCauseReflowNotifier crNotifier(this);
  mFrameConstructor->ReconstructDocElementHierarchy(
      nsCSSFrameConstructor::InsertionKind::Sync);
}

nsresult PresShell::RenderDocument(const nsRect& aRect,
                                   RenderDocumentFlags aFlags,
                                   nscolor aBackgroundColor,
                                   gfxContext* aThebesContext) {
  NS_ENSURE_TRUE(!(aFlags & RenderDocumentFlags::IsUntrusted),
                 NS_ERROR_NOT_IMPLEMENTED);

  nsRootPresContext* rootPresContext = mPresContext->GetRootPresContext();
  if (rootPresContext) {
    rootPresContext->FlushWillPaintObservers();
    if (mIsDestroying) return NS_OK;
  }

  nsAutoScriptBlocker blockScripts;

  // Set up the rectangle as the path in aThebesContext
  gfxRect r(0, 0, nsPresContext::AppUnitsToFloatCSSPixels(aRect.width),
            nsPresContext::AppUnitsToFloatCSSPixels(aRect.height));
  aThebesContext->NewPath();
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
  aThebesContext->SnappedRectangle(r);
#else
  aThebesContext->Rectangle(r);
#endif

  nsIFrame* rootFrame = mFrameConstructor->GetRootFrame();
  if (!rootFrame) {
    // Nothing to paint, just fill the rect
    aThebesContext->SetColor(sRGBColor::FromABGR(aBackgroundColor));
    aThebesContext->Fill();
    return NS_OK;
  }

  gfxContextAutoSaveRestore save(aThebesContext);

  MOZ_ASSERT(aThebesContext->CurrentOp() == CompositionOp::OP_OVER);

  aThebesContext->Clip();

  nsDeviceContext* devCtx = mPresContext->DeviceContext();

  gfxPoint offset(-nsPresContext::AppUnitsToFloatCSSPixels(aRect.x),
                  -nsPresContext::AppUnitsToFloatCSSPixels(aRect.y));
  gfxFloat scale =
      gfxFloat(devCtx->AppUnitsPerDevPixel()) / AppUnitsPerCSSPixel();

  // Since canvas APIs use floats to set up their matrices, we may have some
  // slight rounding errors here.  We use NudgeToIntegers() here to adjust
  // matrix components that are integers up to the accuracy of floats to be
  // those integers.
  gfxMatrix newTM = aThebesContext->CurrentMatrixDouble()
                        .PreTranslate(offset)
                        .PreScale(scale, scale)
                        .NudgeToIntegers();
  aThebesContext->SetMatrixDouble(newTM);

  AutoSaveRestoreRenderingState _(this);

  bool wouldFlushRetainedLayers = false;
  PaintFrameFlags flags = PaintFrameFlags::IgnoreSuppression;
  if (aThebesContext->CurrentMatrix().HasNonIntegerTranslation()) {
    flags |= PaintFrameFlags::InTransform;
  }
  if (!(aFlags & RenderDocumentFlags::AsyncDecodeImages)) {
    flags |= PaintFrameFlags::SyncDecodeImages;
  }
  if (aFlags & RenderDocumentFlags::UseWidgetLayers) {
    // We only support using widget layers on display root's with widgets.
    nsView* view = rootFrame->GetView();
    if (view && view->GetWidget() &&
        nsLayoutUtils::GetDisplayRootFrame(rootFrame) == rootFrame) {
      LayerManager* layerManager = view->GetWidget()->GetLayerManager();
      // ClientLayerManagers or WebRenderLayerManagers in content processes
      // don't support taking snapshots.
      if (layerManager &&
          (!layerManager->AsKnowsCompositor() || XRE_IsParentProcess())) {
        flags |= PaintFrameFlags::WidgetLayers;
      }
    }
  }
  if (!(aFlags & RenderDocumentFlags::DrawCaret)) {
    wouldFlushRetainedLayers = true;
    flags |= PaintFrameFlags::HideCaret;
  }
  if (aFlags & RenderDocumentFlags::IgnoreViewportScrolling) {
    wouldFlushRetainedLayers = !IgnoringViewportScrolling();
    mRenderingStateFlags |= RenderingStateFlags::IgnoringViewportScrolling;
  }
  if (aFlags & RenderDocumentFlags::DrawWindowNotFlushing) {
    mRenderingStateFlags |= RenderingStateFlags::DrawWindowNotFlushing;
  }
  if (aFlags & RenderDocumentFlags::DocumentRelative) {
    // XXX be smarter about this ... drawWindow might want a rect
    // that's "pretty close" to what our retained layer tree covers.
    // In that case, it wouldn't disturb normal rendering too much,
    // and we should allow it.
    wouldFlushRetainedLayers = true;
    flags |= PaintFrameFlags::DocumentRelative;
  }

  // Don't let drawWindow blow away our retained layer tree
  if ((flags & PaintFrameFlags::WidgetLayers) && wouldFlushRetainedLayers) {
    flags &= ~PaintFrameFlags::WidgetLayers;
  }

  nsLayoutUtils::PaintFrame(aThebesContext, rootFrame, nsRegion(aRect),
                            aBackgroundColor,
                            nsDisplayListBuilderMode::Painting, flags);

  return NS_OK;
}

/*
 * Clip the display list aList to a range. Returns the clipped
 * rectangle surrounding the range.
 */
nsRect PresShell::ClipListToRange(nsDisplayListBuilder* aBuilder,
                                  nsDisplayList* aList, nsRange* aRange) {
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
    if (i->GetType() == DisplayItemType::TYPE_CONTAINER) {
      tmpList.AppendToTop(i);
      surfaceRect.UnionRect(
          surfaceRect, ClipListToRange(aBuilder, i->GetChildren(), aRange));
      continue;
    }

    // itemToInsert indiciates the item that should be inserted into the
    // temporary list. If null, no item should be inserted.
    nsDisplayItem* itemToInsert = nullptr;
    nsIFrame* frame = i->Frame();
    nsIContent* content = frame->GetContent();
    if (content) {
      bool atStart = (content == aRange->GetStartContainer());
      bool atEnd = (content == aRange->GetEndContainer());
      if ((atStart || atEnd) && frame->IsTextFrame()) {
        int32_t frameStartOffset, frameEndOffset;
        frame->GetOffsets(frameStartOffset, frameEndOffset);

        int32_t hilightStart =
            atStart ? std::max(static_cast<int32_t>(aRange->StartOffset()),
                               frameStartOffset)
                    : frameStartOffset;
        int32_t hilightEnd =
            atEnd ? std::min(static_cast<int32_t>(aRange->EndOffset()),
                             frameEndOffset)
                  : frameEndOffset;
        if (hilightStart < hilightEnd) {
          // determine the location of the start and end edges of the range.
          nsPoint startPoint, endPoint;
          frame->GetPointFromOffset(hilightStart, &startPoint);
          frame->GetPointFromOffset(hilightEnd, &endPoint);

          // The clip rectangle is determined by taking the the start and
          // end points of the range, offset from the reference frame.
          // Because of rtl, the end point may be to the left of (or above,
          // in vertical mode) the start point, so x (or y) is set to the
          // lower of the values.
          nsRect textRect(aBuilder->ToReferenceFrame(frame), frame->GetSize());
          if (frame->GetWritingMode().IsVertical()) {
            nscoord y = std::min(startPoint.y, endPoint.y);
            textRect.y += y;
            textRect.height = std::max(startPoint.y, endPoint.y) - y;
          } else {
            nscoord x = std::min(startPoint.x, endPoint.x);
            textRect.x += x;
            textRect.width = std::max(startPoint.x, endPoint.x) - x;
          }
          surfaceRect.UnionRect(surfaceRect, textRect);

          const ActiveScrolledRoot* asr = i->GetActiveScrolledRoot();

          DisplayItemClip newClip;
          newClip.SetTo(textRect);

          const DisplayItemClipChain* newClipChain =
              aBuilder->AllocateDisplayItemClipChain(newClip, asr, nullptr);

          i->IntersectClip(aBuilder, newClipChain, true);
          itemToInsert = i;
        }
      }
      // Don't try to descend into subdocuments.
      // If this ever changes we'd need to add handling for subdocuments with
      // different zoom levels.
      else if (content->GetUncomposedDoc() ==
               aRange->GetStartContainer()->GetUncomposedDoc()) {
        // if the node is within the range, append it to the temporary list
        bool before, after;
        nsresult rv =
            RangeUtils::CompareNodeToRange(content, aRange, &before, &after);
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
    } else {
      // otherwise, just delete the item and don't readd it to the list
      i->Destroy(aBuilder);
    }
  }

  // now add all the items back onto the original list again
  aList->AppendToTop(&tmpList);

  return surfaceRect;
}

#ifdef DEBUG
#  include <stdio.h>

static bool gDumpRangePaintList = false;
#endif

UniquePtr<RangePaintInfo> PresShell::CreateRangePaintInfo(
    nsRange* aRange, nsRect& aSurfaceRect, bool aForPrimarySelection) {
  nsIFrame* ancestorFrame;
  nsIFrame* rootFrame = GetRootFrame();

  // If the start or end of the range is the document, just use the root
  // frame, otherwise get the common ancestor of the two endpoints of the
  // range.
  nsINode* startContainer = aRange->GetStartContainer();
  nsINode* endContainer = aRange->GetEndContainer();
  Document* doc = startContainer->GetComposedDoc();
  if (startContainer == doc || endContainer == doc) {
    ancestorFrame = rootFrame;
  } else {
    nsINode* ancestor = nsContentUtils::GetClosestCommonInclusiveAncestor(
        startContainer, endContainer);
    NS_ASSERTION(!ancestor || ancestor->IsContent(),
                 "common ancestor is not content");

    while (ancestor && ancestor->IsContent()) {
      ancestorFrame = ancestor->AsContent()->GetPrimaryFrame();
      if (ancestorFrame) {
        break;
      }

      ancestor = ancestor->GetParentOrShadowHostNode();
    }

    // use the nearest ancestor frame that includes all continuations as the
    // root for building the display list
    while (ancestorFrame &&
           nsLayoutUtils::GetNextContinuationOrIBSplitSibling(ancestorFrame))
      ancestorFrame = ancestorFrame->GetParent();
  }

  if (!ancestorFrame) {
    return nullptr;
  }

  // get a display list containing the range
  auto info = MakeUnique<RangePaintInfo>(aRange, ancestorFrame);
  info->mBuilder.SetIncludeAllOutOfFlows();
  if (aForPrimarySelection) {
    info->mBuilder.SetSelectedFramesOnly();
  }
  info->mBuilder.EnterPresShell(ancestorFrame);

  ContentSubtreeIterator subtreeIter;
  nsresult rv = subtreeIter.Init(aRange);
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  auto BuildDisplayListForNode = [&](nsINode* aNode) {
    if (MOZ_UNLIKELY(!aNode->IsContent())) {
      return;
    }
    nsIFrame* frame = aNode->AsContent()->GetPrimaryFrame();
    // XXX deal with frame being null due to display:contents
    for (; frame;
         frame = nsLayoutUtils::GetNextContinuationOrIBSplitSibling(frame)) {
      info->mBuilder.SetVisibleRect(frame->GetVisualOverflowRect());
      info->mBuilder.SetDirtyRect(frame->GetVisualOverflowRect());
      frame->BuildDisplayListForStackingContext(&info->mBuilder, &info->mList);
    }
  };
  if (startContainer->NodeType() == nsINode::TEXT_NODE) {
    BuildDisplayListForNode(startContainer);
  }
  for (; !subtreeIter.IsDone(); subtreeIter.Next()) {
    nsCOMPtr<nsINode> node = subtreeIter.GetCurrentNode();
    BuildDisplayListForNode(node);
  }
  if (endContainer != startContainer &&
      endContainer->NodeType() == nsINode::TEXT_NODE) {
    BuildDisplayListForNode(endContainer);
  }

#ifdef DEBUG
  if (gDumpRangePaintList) {
    fprintf(stderr, "CreateRangePaintInfo --- before ClipListToRange:\n");
    nsFrame::PrintDisplayList(&(info->mBuilder), info->mList);
  }
#endif

  nsRect rangeRect = ClipListToRange(&info->mBuilder, &info->mList, aRange);

  info->mBuilder.LeavePresShell(ancestorFrame, &info->mList);

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

already_AddRefed<SourceSurface> PresShell::PaintRangePaintInfo(
    const nsTArray<UniquePtr<RangePaintInfo>>& aItems, Selection* aSelection,
    const Maybe<CSSIntRegion>& aRegion, nsRect aArea,
    const LayoutDeviceIntPoint aPoint, LayoutDeviceIntRect* aScreenRect,
    RenderImageFlags aFlags) {
  nsPresContext* pc = GetPresContext();
  if (!pc || aArea.width == 0 || aArea.height == 0) return nullptr;

  // use the rectangle to create the surface
  nsIntRect pixelArea = aArea.ToOutsidePixels(pc->AppUnitsPerDevPixel());

  // if the image should not be resized, scale must be 1
  float scale = 1.0;
  nsIntRect rootScreenRect =
      GetRootFrame()->GetScreenRectInAppUnits().ToNearestPixels(
          pc->AppUnitsPerDevPixel());

  nsRect maxSize;
  pc->DeviceContext()->GetClientRect(maxSize);

  // check if the image should be resized
  bool resize = !!(aFlags & RenderImageFlags::AutoScale);

  if (resize) {
    // check if image-resizing-algorithm should be used
    if (aFlags & RenderImageFlags::IsImage) {
      // get max screensize
      nscoord maxWidth = pc->AppUnitsToDevPixels(maxSize.width);
      nscoord maxHeight = pc->AppUnitsToDevPixels(maxSize.height);
      // resize image relative to the screensize
      // get best height/width relative to screensize
      float bestHeight = float(maxHeight) * RELATIVE_SCALEFACTOR;
      float bestWidth = float(maxWidth) * RELATIVE_SCALEFACTOR;
      // calculate scale for bestWidth
      float adjustedScale = bestWidth / float(pixelArea.width);
      // get the worst height (height when width is perfect)
      float worstHeight = float(pixelArea.height) * adjustedScale;
      // get the difference of best and worst height
      float difference = bestHeight - worstHeight;
      // halve the difference and add it to worstHeight to get
      // the best compromise between bestHeight and bestWidth,
      // then calculate the corresponding scale factor
      adjustedScale = (worstHeight + difference / 2) / float(pixelArea.height);
      // prevent upscaling
      scale = std::min(scale, adjustedScale);
    } else {
      // get half of max screensize
      nscoord maxWidth = pc->AppUnitsToDevPixels(maxSize.width >> 1);
      nscoord maxHeight = pc->AppUnitsToDevPixels(maxSize.height >> 1);
      if (pixelArea.width > maxWidth || pixelArea.height > maxHeight) {
        // divide the maximum size by the image size in both directions.
        // Whichever direction produces the smallest result determines how much
        // should be scaled.
        if (pixelArea.width > maxWidth)
          scale = std::min(scale, float(maxWidth) / pixelArea.width);
        if (pixelArea.height > maxHeight)
          scale = std::min(scale, float(maxHeight) / pixelArea.height);
      }
    }

    pixelArea.width = NSToIntFloor(float(pixelArea.width) * scale);
    pixelArea.height = NSToIntFloor(float(pixelArea.height) * scale);
    if (!pixelArea.width || !pixelArea.height) return nullptr;

    // adjust the screen position based on the rescaled size
    nscoord left = rootScreenRect.x + pixelArea.x;
    nscoord top = rootScreenRect.y + pixelArea.y;
    aScreenRect->x = NSToIntFloor(aPoint.x - float(aPoint.x - left) * scale);
    aScreenRect->y = NSToIntFloor(aPoint.y - float(aPoint.y - top) * scale);
  } else {
    // move aScreenRect to the position of the surface in screen coordinates
    aScreenRect->MoveTo(rootScreenRect.x + pixelArea.x,
                        rootScreenRect.y + pixelArea.y);
  }
  aScreenRect->width = pixelArea.width;
  aScreenRect->height = pixelArea.height;

  RefPtr<DrawTarget> dt =
      gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(
          IntSize(pixelArea.width, pixelArea.height), SurfaceFormat::B8G8R8A8);
  if (!dt || !dt->IsValid()) {
    return nullptr;
  }

  RefPtr<gfxContext> ctx = gfxContext::CreateOrNull(dt);
  MOZ_ASSERT(ctx);  // already checked the draw target above

  if (aRegion) {
    RefPtr<PathBuilder> builder = dt->CreatePathBuilder(FillRule::FILL_WINDING);

    // Convert aRegion from CSS pixels to dev pixels
    nsIntRegion region = aRegion->ToAppUnits(AppUnitsPerCSSPixel())
                             .ToOutsidePixels(pc->AppUnitsPerDevPixel());
    for (auto iter = region.RectIter(); !iter.Done(); iter.Next()) {
      const IntRect& rect = iter.Get();

      builder->MoveTo(rect.TopLeft());
      builder->LineTo(rect.TopRight());
      builder->LineTo(rect.BottomRight());
      builder->LineTo(rect.BottomLeft());
      builder->LineTo(rect.TopLeft());
    }

    RefPtr<Path> path = builder->Finish();
    ctx->Clip(path);
  }

  gfxMatrix initialTM = ctx->CurrentMatrixDouble();

  if (resize) initialTM.PreScale(scale, scale);

  // translate so that points are relative to the surface area
  gfxPoint surfaceOffset = nsLayoutUtils::PointToGfxPoint(
      -aArea.TopLeft(), pc->AppUnitsPerDevPixel());
  initialTM.PreTranslate(surfaceOffset);

  // temporarily hide the selection so that text is drawn normally. If a
  // selection is being rendered, use that, otherwise use the presshell's
  // selection.
  RefPtr<nsFrameSelection> frameSelection;
  if (aSelection) {
    frameSelection = aSelection->GetFrameSelection();
  } else {
    frameSelection = FrameSelection();
  }
  int16_t oldDisplaySelection = frameSelection->GetDisplaySelection();
  frameSelection->SetDisplaySelection(nsISelectionController::SELECTION_HIDDEN);

  // next, paint each range in the selection
  for (const UniquePtr<RangePaintInfo>& rangeInfo : aItems) {
    // the display lists paint relative to the offset from the reference
    // frame, so account for that translation too:
    gfxPoint rootOffset = nsLayoutUtils::PointToGfxPoint(
        rangeInfo->mRootOffset, pc->AppUnitsPerDevPixel());
    ctx->SetMatrixDouble(initialTM.PreTranslate(rootOffset));
    aArea.MoveBy(-rangeInfo->mRootOffset.x, -rangeInfo->mRootOffset.y);
    nsRegion visible(aArea);
    RefPtr<LayerManager> layerManager = rangeInfo->mList.PaintRoot(
        &rangeInfo->mBuilder, ctx, nsDisplayList::PAINT_DEFAULT);
    aArea.MoveBy(rangeInfo->mRootOffset.x, rangeInfo->mRootOffset.y);
  }

  // restore the old selection display state
  frameSelection->SetDisplaySelection(oldDisplaySelection);

  return dt->Snapshot();
}

already_AddRefed<SourceSurface> PresShell::RenderNode(
    nsINode* aNode, const Maybe<CSSIntRegion>& aRegion,
    const LayoutDeviceIntPoint aPoint, LayoutDeviceIntRect* aScreenRect,
    RenderImageFlags aFlags) {
  // area will hold the size of the surface needed to draw the node, measured
  // from the root frame.
  nsRect area;
  nsTArray<UniquePtr<RangePaintInfo>> rangeItems;

  // nothing to draw if the node isn't in a document
  if (!aNode->IsInComposedDoc()) {
    return nullptr;
  }

  RefPtr<nsRange> range = nsRange::Create(aNode);
  IgnoredErrorResult rv;
  range->SelectNode(*aNode, rv);
  if (rv.Failed()) {
    return nullptr;
  }

  UniquePtr<RangePaintInfo> info = CreateRangePaintInfo(range, area, false);
  if (info) {
    // XXX(Bug 1631371) Check if this should use a fallible operation as it
    // pretended earlier, or change the return type to void.
    rangeItems.AppendElement(std::move(info));
  }

  Maybe<CSSIntRegion> region = aRegion;
  if (region) {
    // combine the area with the supplied region
    CSSIntRect rrectPixels = region->GetBounds();

    nsRect rrect = ToAppUnits(rrectPixels, AppUnitsPerCSSPixel());
    area.IntersectRect(area, rrect);

    nsPresContext* pc = GetPresContext();
    if (!pc) return nullptr;

    // move the region so that it is offset from the topleft corner of the
    // surface
    region->MoveBy(-nsPresContext::AppUnitsToIntCSSPixels(area.x),
                   -nsPresContext::AppUnitsToIntCSSPixels(area.y));
  }

  return PaintRangePaintInfo(rangeItems, nullptr, region, area, aPoint,
                             aScreenRect, aFlags);
}

already_AddRefed<SourceSurface> PresShell::RenderSelection(
    Selection* aSelection, const LayoutDeviceIntPoint aPoint,
    LayoutDeviceIntRect* aScreenRect, RenderImageFlags aFlags) {
  // area will hold the size of the surface needed to draw the selection,
  // measured from the root frame.
  nsRect area;
  nsTArray<UniquePtr<RangePaintInfo>> rangeItems;

  // iterate over each range and collect them into the rangeItems array.
  // This is done so that the size of selection can be determined so as
  // to allocate a surface area
  uint32_t numRanges = aSelection->RangeCount();
  NS_ASSERTION(numRanges > 0, "RenderSelection called with no selection");

  for (uint32_t r = 0; r < numRanges; r++) {
    RefPtr<nsRange> range = aSelection->GetRangeAt(r);

    UniquePtr<RangePaintInfo> info = CreateRangePaintInfo(range, area, true);
    if (info) {
      // XXX(Bug 1631371) Check if this should use a fallible operation as it
      // pretended earlier.
      rangeItems.AppendElement(std::move(info));
    }
  }

  return PaintRangePaintInfo(rangeItems, aSelection, Nothing(), area, aPoint,
                             aScreenRect, aFlags);
}

void PresShell::AddPrintPreviewBackgroundItem(nsDisplayListBuilder* aBuilder,
                                              nsDisplayList* aList,
                                              nsIFrame* aFrame,
                                              const nsRect& aBounds) {
  aList->AppendNewToBottom<nsDisplaySolidColor>(aBuilder, aFrame, aBounds,
                                                NS_RGB(115, 115, 115));
}

static bool AddCanvasBackgroundColor(const nsDisplayList* aList,
                                     nsIFrame* aCanvasFrame, nscolor aColor,
                                     bool aCSSBackgroundColor) {
  for (nsDisplayItem* i = aList->GetBottom(); i; i = i->GetAbove()) {
    const DisplayItemType type = i->GetType();

    if (i->Frame() == aCanvasFrame &&
        type == DisplayItemType::TYPE_CANVAS_BACKGROUND_COLOR) {
      auto* bg = static_cast<nsDisplayCanvasBackgroundColor*>(i);
      bg->SetExtraBackgroundColor(aColor);
      return true;
    }

    const bool isBlendContainer =
        type == DisplayItemType::TYPE_BLEND_CONTAINER ||
        type == DisplayItemType::TYPE_TABLE_BLEND_CONTAINER;

    nsDisplayList* sublist = i->GetSameCoordinateSystemChildren();
    if (sublist && !(isBlendContainer && !aCSSBackgroundColor) &&
        AddCanvasBackgroundColor(sublist, aCanvasFrame, aColor,
                                 aCSSBackgroundColor))
      return true;
  }
  return false;
}

void PresShell::AddCanvasBackgroundColorItem(
    nsDisplayListBuilder* aBuilder, nsDisplayList* aList, nsIFrame* aFrame,
    const nsRect& aBounds, nscolor aBackstopColor,
    AddCanvasBackgroundColorFlags aFlags) {
  if (aBounds.IsEmpty()) {
    return;
  }
  // We don't want to add an item for the canvas background color if the frame
  // (sub)tree we are painting doesn't include any canvas frames. There isn't
  // an easy way to check this directly, but if we check if the root of the
  // (sub)tree we are painting is a canvas frame that should cover us in all
  // cases (it will usually be a viewport frame when we have a canvas frame in
  // the (sub)tree).
  if (!(aFlags & AddCanvasBackgroundColorFlags::ForceDraw) &&
      !nsCSSRendering::IsCanvasFrame(aFrame)) {
    return;
  }

  nscolor bgcolor = NS_ComposeColors(aBackstopColor, mCanvasBackgroundColor);
  if (NS_GET_A(bgcolor) == 0) return;

  // To make layers work better, we want to avoid having a big non-scrolled
  // color background behind a scrolled transparent background. Instead,
  // we'll try to move the color background into the scrolled content
  // by making nsDisplayCanvasBackground paint it.
  // If we're only adding an unscrolled item, then pretend that we've
  // already done it.
  bool addedScrollingBackgroundColor =
      !!(aFlags & AddCanvasBackgroundColorFlags::AppendUnscrolledOnly);
  if (!aFrame->GetParent() && !addedScrollingBackgroundColor) {
    nsIScrollableFrame* sf =
        aFrame->PresShell()->GetRootScrollFrameAsScrollable();
    if (sf) {
      nsCanvasFrame* canvasFrame = do_QueryFrame(sf->GetScrolledFrame());
      if (canvasFrame && canvasFrame->IsVisibleForPainting()) {
        addedScrollingBackgroundColor = AddCanvasBackgroundColor(
            aList, canvasFrame, bgcolor, mHasCSSBackgroundColor);
      }
    }
  }

  // With async scrolling, we'd like to have two instances of the background
  // color: one that scrolls with the content (for the reasons stated above),
  // and one underneath which does not scroll with the content, but which can
  // be shown during checkerboarding and overscroll.
  // We can only do that if the color is opaque.
  bool forceUnscrolledItem =
      nsLayoutUtils::UsesAsyncScrolling(aFrame) && NS_GET_A(bgcolor) == 255;

  if (!addedScrollingBackgroundColor || forceUnscrolledItem) {
    aList->AppendNewToBottom<nsDisplaySolidColor>(aBuilder, aFrame, aBounds,
                                                  bgcolor);
  }
}

static bool IsTransparentContainerElement(nsPresContext* aPresContext) {
  nsCOMPtr<nsIDocShell> docShell = aPresContext->GetDocShell();
  if (!docShell) {
    return false;
  }

  nsCOMPtr<nsPIDOMWindowOuter> pwin = docShell->GetWindow();
  if (!pwin) return false;
  nsCOMPtr<Element> containerElement = pwin->GetFrameElementInternal();

  BrowserChild* tab = BrowserChild::GetFrom(docShell);
  if (tab) {
    // Check if presShell is the top PresShell. Only the top can
    // influence the canvas background color.
    if (aPresContext->GetPresShell() != tab->GetTopLevelPresShell()) {
      tab = nullptr;
    }
  }

  return (containerElement && containerElement->HasAttr(
                                  kNameSpaceID_None, nsGkAtoms::transparent)) ||
         (tab && tab->IsTransparent());
}

nscolor PresShell::GetDefaultBackgroundColorToDraw() {
  if (!mPresContext || !mPresContext->GetBackgroundColorDraw()) {
    return NS_RGB(255, 255, 255);
  }

  nscolor backgroundColor = mPresContext->DefaultBackgroundColor();
  if (backgroundColor != NS_RGB(255, 255, 255)) {
    // Return non-default color.
    return backgroundColor;
  }

  // Use a dark background for top-level about:blank that is inaccessible to
  // content JS.
  Document* doc = GetDocument();
  BrowsingContext* bc = doc->GetBrowsingContext();
  if (bc && bc->IsTop() && !bc->HasOpener() && doc->GetDocumentURI() &&
      NS_IsAboutBlank(doc->GetDocumentURI()) &&
      doc->PrefersColorScheme() == StylePrefersColorScheme::Dark) {
    // Use --in-content-page-background for prefers-color-scheme: dark.
    return NS_RGB(0x2A, 0x2A, 0x2E);
  }

  return backgroundColor;
}

void PresShell::UpdateCanvasBackground() {
  // If we have a frame tree and it has style information that
  // specifies the background color of the canvas, update our local
  // cache of that color.
  nsIFrame* rootStyleFrame = FrameConstructor()->GetRootElementStyleFrame();
  if (rootStyleFrame) {
    ComputedStyle* bgStyle =
        nsCSSRendering::FindRootFrameBackground(rootStyleFrame);
    // XXX We should really be passing the canvasframe, not the root element
    // style frame but we don't have access to the canvasframe here. It isn't
    // a problem because only a few frames can return something other than true
    // and none of them would be a canvas frame or root element style frame.
    bool drawBackgroundImage;
    bool drawBackgroundColor;
    mCanvasBackgroundColor = nsCSSRendering::DetermineBackgroundColor(
        mPresContext, bgStyle, rootStyleFrame, drawBackgroundImage,
        drawBackgroundColor);
    mHasCSSBackgroundColor = drawBackgroundColor;
    if (mPresContext->IsRootContentDocumentCrossProcess() &&
        !IsTransparentContainerElement(mPresContext)) {
      mCanvasBackgroundColor = NS_ComposeColors(
          GetDefaultBackgroundColorToDraw(), mCanvasBackgroundColor);
    }
  }

  // If the root element of the document (ie html) has style 'display: none'
  // then the document's background color does not get drawn; cache the
  // color we actually draw.
  if (!FrameConstructor()->GetRootElementFrame()) {
    mCanvasBackgroundColor = GetDefaultBackgroundColorToDraw();
  }
}

nscolor PresShell::ComputeBackstopColor(nsView* aDisplayRoot) {
  nsIWidget* widget = aDisplayRoot->GetWidget();
  if (widget && (widget->GetTransparencyMode() != eTransparencyOpaque ||
                 widget->WidgetPaintsBackground())) {
    // Within a transparent widget, so the backstop color must be
    // totally transparent.
    return NS_RGBA(0, 0, 0, 0);
  }
  // Within an opaque widget (or no widget at all), so the backstop
  // color must be totally opaque. The user's default background
  // as reported by the prescontext is guaranteed to be opaque.
  return GetDefaultBackgroundColorToDraw();
}

struct PaintParams {
  nscolor mBackgroundColor;
};

LayerManager* PresShell::GetLayerManager() {
  NS_ASSERTION(mViewManager, "Should have view manager");

  nsView* rootView = mViewManager->GetRootView();
  if (rootView) {
    if (nsIWidget* widget = rootView->GetWidget()) {
      return widget->GetLayerManager();
    }
  }
  return nullptr;
}

bool PresShell::AsyncPanZoomEnabled() {
  NS_ASSERTION(mViewManager, "Should have view manager");
  nsView* rootView = mViewManager->GetRootView();
  if (rootView) {
    if (nsIWidget* widget = rootView->GetWidget()) {
      return widget->AsyncPanZoomEnabled();
    }
  }
  return gfxPlatform::AsyncPanZoomEnabled();
}

nsresult PresShell::SetResolutionAndScaleTo(float aResolution,
                                            ResolutionChangeOrigin aOrigin) {
  if (!(aResolution > 0.0)) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  if (aResolution == mResolution.valueOr(0.0)) {
    MOZ_ASSERT(mResolution.isSome());
    return NS_OK;
  }
  RenderingState state(this);
  state.mResolution = Some(aResolution);
  SetRenderingState(state);
  if (mMobileViewportManager) {
    mMobileViewportManager->ResolutionUpdated(aOrigin);
  }
  if (aOrigin == ResolutionChangeOrigin::Apz) {
    mResolutionUpdatedByApz = true;
  } else {
    mResolutionUpdated = true;
  }

  if (auto* window = nsGlobalWindowInner::Cast(mDocument->GetInnerWindow())) {
    window->VisualViewport()->PostResizeEvent();
  }

  return NS_OK;
}

float PresShell::GetCumulativeResolution() {
  float resolution = GetResolution();
  nsPresContext* parentCtx = GetPresContext()->GetParentPresContext();
  if (parentCtx) {
    resolution *= parentCtx->PresShell()->GetCumulativeResolution();
  }
  return resolution;
}

void PresShell::SetRestoreResolution(float aResolution,
                                     LayoutDeviceIntSize aDisplaySize) {
  if (mMobileViewportManager) {
    mMobileViewportManager->SetRestoreResolution(aResolution, aDisplaySize);
  }
}

void PresShell::SetRenderingState(const RenderingState& aState) {
  if (mRenderingStateFlags != aState.mRenderingStateFlags) {
    // Rendering state changed in a way that forces us to flush any
    // retained layers we might already have.
    LayerManager* manager = GetLayerManager();
    if (manager) {
      FrameLayerBuilder::InvalidateAllLayers(manager);
    }
  }

  // nsSubDocumentFrame uses a resolution different from 1.0 to determine if it
  // needs to build a nsDisplayResolution item. So if we are going from or
  // to 1.0 then we need to invalidate the subdoc frame so that item gets
  // created/removed.
  if (mResolution.valueOr(1.0) != aState.mResolution.valueOr(1.0) &&
      (mResolution.valueOr(1.0) == 1.0 ||
       aState.mResolution.valueOr(1.0) == 1.0)) {
    if (nsIFrame* frame = GetRootFrame()) {
      frame = nsLayoutUtils::GetCrossDocParentFrame(frame);
      if (frame) {
        frame->InvalidateFrame();
      }
    }
  }

  mRenderingStateFlags = aState.mRenderingStateFlags;
  mResolution = aState.mResolution;
}

void PresShell::SynthesizeMouseMove(bool aFromScroll) {
  if (!StaticPrefs::layout_reflow_synthMouseMove()) return;

  if (mPaintingSuppressed || !mIsActive || !mPresContext) {
    return;
  }

  if (!mPresContext->IsRoot()) {
    if (PresShell* rootPresShell = GetRootPresShell()) {
      rootPresShell->SynthesizeMouseMove(aFromScroll);
    }
    return;
  }

  if (mMouseLocation == nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE))
    return;

  if (!mSynthMouseMoveEvent.IsPending()) {
    RefPtr<nsSynthMouseMoveEvent> ev =
        new nsSynthMouseMoveEvent(this, aFromScroll);

    GetPresContext()->RefreshDriver()->AddRefreshObserver(ev,
                                                          FlushType::Display);
    mSynthMouseMoveEvent = std::move(ev);
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
static nsView* FindFloatingViewContaining(nsView* aView, nsPoint aPt) {
  if (aView->GetVisibility() == nsViewVisibility_kHide)
    // No need to look into descendants.
    return nullptr;

  nsIFrame* frame = aView->GetFrame();
  if (frame) {
    if (!frame->IsVisibleConsideringAncestors(
            nsIFrame::VISIBILITY_CROSS_CHROME_CONTENT_BOUNDARY) ||
        !frame->PresShell()->IsActive()) {
      return nullptr;
    }
  }

  for (nsView* v = aView->GetFirstChild(); v; v = v->GetNextSibling()) {
    nsView* r = FindFloatingViewContaining(v, v->ConvertFromParentCoords(aPt));
    if (r) return r;
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
static nsView* FindViewContaining(nsView* aView, nsPoint aPt) {
  if (!aView->GetDimensions().Contains(aPt) ||
      aView->GetVisibility() == nsViewVisibility_kHide) {
    return nullptr;
  }

  nsIFrame* frame = aView->GetFrame();
  if (frame) {
    if (!frame->IsVisibleConsideringAncestors(
            nsIFrame::VISIBILITY_CROSS_CHROME_CONTENT_BOUNDARY) ||
        !frame->PresShell()->IsActive()) {
      return nullptr;
    }
  }

  for (nsView* v = aView->GetFirstChild(); v; v = v->GetNextSibling()) {
    nsView* r = FindViewContaining(v, v->ConvertFromParentCoords(aPt));
    if (r) return r;
  }

  return aView;
}

static BrowserBridgeChild* GetChildBrowser(nsView* aView) {
  if (!aView) {
    return nullptr;
  }
  nsIFrame* frame = aView->GetFrame();
  if (!frame && aView->GetParent()) {
    // If frame is null then view is an anonymous inner view, and we want
    // the frame from the corresponding outer view.
    frame = aView->GetParent()->GetFrame();
  }
  if (!frame || !frame->GetContent()) {
    return nullptr;
  }
  return BrowserBridgeChild::GetFrom(frame->GetContent());
}

void PresShell::ProcessSynthMouseMoveEvent(bool aFromScroll) {
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
  RefPtr<PresShell> kungFuDeathGrip(this);

#ifdef DEBUG_MOUSE_LOCATION
  printf("[ps=%p]synthesizing mouse move to (%d,%d)\n", this, mMouseLocation.x,
         mMouseLocation.y);
#endif

  int32_t APD = mPresContext->AppUnitsPerDevPixel();

  // We need a widget to put in the event we are going to dispatch so we look
  // for a view that has a widget and the mouse location is over. We first look
  // for floating views, if there isn't one we use the root view. |view| holds
  // that view.
  nsView* view = nullptr;

  // The appunits per devpixel ratio of |view|.
  int32_t viewAPD;

  // mRefPoint will be mMouseLocation relative to the widget of |view|, the
  // widget we will put in the event we dispatch, in viewAPD appunits
  nsPoint refpoint(0, 0);

  // We always dispatch the event to the pres shell that contains the view that
  // the mouse is over. pointVM is the VM of that pres shell.
  nsViewManager* pointVM = nullptr;

  // This could be a bit slow (traverses entire view hierarchy)
  // but it's OK to do it once per synthetic mouse event
  view = FindFloatingViewContaining(rootView, mMouseLocation);
  nsView* pointView = view;
  if (!view) {
    view = rootView;
    pointView = FindViewContaining(rootView, mMouseLocation);
    // pointView can be null in situations related to mouse capture
    pointVM = (pointView ? pointView : view)->GetViewManager();
    refpoint = mMouseLocation + rootView->ViewToWidgetOffset();
    viewAPD = APD;
  } else {
    pointVM = view->GetViewManager();
    nsIFrame* frame = view->GetFrame();
    NS_ASSERTION(frame, "floating views can't be anonymous");
    viewAPD = frame->PresContext()->AppUnitsPerDevPixel();
    refpoint = mMouseLocation.ScaleToOtherAppUnits(APD, viewAPD);
    refpoint -= view->GetOffsetTo(rootView);
    refpoint += view->ViewToWidgetOffset();
  }
  NS_ASSERTION(view->GetWidget(), "view should have a widget here");
  WidgetMouseEvent event(true, eMouseMove, view->GetWidget(),
                         WidgetMouseEvent::eSynthesized);
  event.mRefPoint =
      LayoutDeviceIntPoint::FromAppUnitsToNearest(refpoint, viewAPD);
  event.mTime = PR_IntervalNow();
  // XXX set event.mModifiers ?
  // XXX mnakano I think that we should get the latest information from widget.

  if (BrowserBridgeChild* bbc = GetChildBrowser(pointView)) {
    // If we have a BrowserBridgeChild, we're going to be dispatching this
    // mouse event into an OOP iframe of the current document.
    event.mLayersId = bbc->GetLayersId();
    bbc->SendDispatchSynthesizedMouseEvent(event);
  } else if (RefPtr<PresShell> presShell = pointVM->GetPresShell()) {
    // Since this gets run in a refresh tick there isn't an InputAPZContext on
    // the stack from the nsBaseWidget. We need to simulate one with at least
    // the correct target guid, so that the correct callback transform gets
    // applied if this event goes to a child process. The input block id is set
    // to 0 because this is a synthetic event which doesn't really belong to any
    // input block. Same for the APZ response field.
    InputAPZContext apzContext(mMouseEventTargetGuid, 0, nsEventStatus_eIgnore);
    presShell->DispatchSynthMouseMove(&event);
  }

  if (!aFromScroll) {
    mSynthMouseMoveEvent.Forget();
  }
}

/* static */
void PresShell::MarkFramesInListApproximatelyVisible(
    const nsDisplayList& aList) {
  for (nsDisplayItem* item = aList.GetBottom(); item; item = item->GetAbove()) {
    nsDisplayList* sublist = item->GetChildren();
    if (sublist) {
      MarkFramesInListApproximatelyVisible(*sublist);
      continue;
    }

    nsIFrame* frame = item->Frame();
    MOZ_ASSERT(frame);

    if (!frame->TrackingVisibility()) {
      continue;
    }

    // Use the presshell containing the frame.
    PresShell* presShell = frame->PresShell();
    MOZ_ASSERT(!presShell->AssumeAllFramesVisible());
    if (presShell->mApproximatelyVisibleFrames.EnsureInserted(frame)) {
      // The frame was added to mApproximatelyVisibleFrames, so increment its
      // visible count.
      frame->IncApproximateVisibleCount();
    }
  }
}

/* static */
void PresShell::DecApproximateVisibleCount(
    VisibleFrames& aFrames, const Maybe<OnNonvisible>& aNonvisibleAction
    /* = Nothing() */) {
  for (auto iter = aFrames.Iter(); !iter.Done(); iter.Next()) {
    nsIFrame* frame = iter.Get()->GetKey();
    // Decrement the frame's visible count if we're still tracking its
    // visibility. (We may not be, if the frame disabled visibility tracking
    // after we added it to the visible frames list.)
    if (frame->TrackingVisibility()) {
      frame->DecApproximateVisibleCount(aNonvisibleAction);
    }
  }
}

void PresShell::RebuildApproximateFrameVisibilityDisplayList(
    const nsDisplayList& aList) {
  MOZ_ASSERT(!mApproximateFrameVisibilityVisited, "already visited?");
  mApproximateFrameVisibilityVisited = true;

  // Remove the entries of the mApproximatelyVisibleFrames hashtable and put
  // them in oldApproxVisibleFrames.
  VisibleFrames oldApproximatelyVisibleFrames;
  mApproximatelyVisibleFrames.SwapElements(oldApproximatelyVisibleFrames);

  MarkFramesInListApproximatelyVisible(aList);

  DecApproximateVisibleCount(oldApproximatelyVisibleFrames);
}

/* static */
void PresShell::ClearApproximateFrameVisibilityVisited(nsView* aView,
                                                       bool aClear) {
  nsViewManager* vm = aView->GetViewManager();
  if (aClear) {
    PresShell* presShell = vm->GetPresShell();
    if (!presShell->mApproximateFrameVisibilityVisited) {
      presShell->ClearApproximatelyVisibleFramesList();
    }
    presShell->mApproximateFrameVisibilityVisited = false;
  }
  for (nsView* v = aView->GetFirstChild(); v; v = v->GetNextSibling()) {
    ClearApproximateFrameVisibilityVisited(v, v->GetViewManager() != vm);
  }
}

void PresShell::ClearApproximatelyVisibleFramesList(
    const Maybe<OnNonvisible>& aNonvisibleAction
    /* = Nothing() */) {
  DecApproximateVisibleCount(mApproximatelyVisibleFrames, aNonvisibleAction);
  mApproximatelyVisibleFrames.Clear();
}

void PresShell::MarkFramesInSubtreeApproximatelyVisible(
    nsIFrame* aFrame, const nsRect& aRect, bool aRemoveOnly /* = false */) {
  MOZ_ASSERT(aFrame->PresShell() == this, "wrong presshell");

  if (aFrame->TrackingVisibility() && aFrame->StyleVisibility()->IsVisible() &&
      (!aRemoveOnly ||
       aFrame->GetVisibility() == Visibility::ApproximatelyVisible)) {
    MOZ_ASSERT(!AssumeAllFramesVisible());
    if (mApproximatelyVisibleFrames.EnsureInserted(aFrame)) {
      // The frame was added to mApproximatelyVisibleFrames, so increment its
      // visible count.
      aFrame->IncApproximateVisibleCount();
    }
  }

  nsSubDocumentFrame* subdocFrame = do_QueryFrame(aFrame);
  if (subdocFrame) {
    PresShell* presShell = subdocFrame->GetSubdocumentPresShellForPainting(
        nsSubDocumentFrame::IGNORE_PAINT_SUPPRESSION);
    if (presShell && !presShell->AssumeAllFramesVisible()) {
      nsRect rect = aRect;
      nsIFrame* root = presShell->GetRootFrame();
      if (root) {
        rect.MoveBy(aFrame->GetOffsetToCrossDoc(root));
      } else {
        rect.MoveBy(-aFrame->GetContentRectRelativeToSelf().TopLeft());
      }
      rect = rect.ScaleToOtherAppUnitsRoundOut(
          aFrame->PresContext()->AppUnitsPerDevPixel(),
          presShell->GetPresContext()->AppUnitsPerDevPixel());

      presShell->RebuildApproximateFrameVisibility(&rect);
    }
    return;
  }

  nsRect rect = aRect;

  nsIScrollableFrame* scrollFrame = do_QueryFrame(aFrame);
  if (scrollFrame) {
    bool ignoreDisplayPort = false;
    if (nsLayoutUtils::IsMissingDisplayPortBaseRect(aFrame->GetContent())) {
      // We can properly set the base rect for root scroll frames on top level
      // and root content documents. Otherwise the base rect we compute might
      // be way too big without the limiting that
      // ScrollFrameHelper::DecideScrollableLayer does, so we just ignore the
      // displayport in that case.
      nsPresContext* pc = aFrame->PresContext();
      if (scrollFrame->IsRootScrollFrameOfDocument() &&
          (pc->IsRootContentDocument() || !pc->GetParentPresContext())) {
        nsRect baseRect =
            nsRect(nsPoint(0, 0),
                   nsLayoutUtils::CalculateCompositionSizeForFrame(aFrame));
        nsLayoutUtils::SetDisplayPortBase(aFrame->GetContent(), baseRect);
      } else {
        ignoreDisplayPort = true;
      }
    }

    nsRect displayPort;
    bool usingDisplayport =
        !ignoreDisplayPort && nsLayoutUtils::GetDisplayPortForVisibilityTesting(
                                  aFrame->GetContent(), &displayPort,
                                  DisplayportRelativeTo::ScrollFrame);

    scrollFrame->NotifyApproximateFrameVisibilityUpdate(!usingDisplayport);

    if (usingDisplayport) {
      rect = displayPort;
    } else {
      rect = rect.Intersect(scrollFrame->GetScrollPortRect());
    }
    rect = scrollFrame->ExpandRectToNearlyVisible(rect);
  }

  bool preserves3DChildren = aFrame->Extend3DContext();

  // We assume all frames in popups are visible, so we skip them here.
  const nsIFrame::ChildListIDs skip = {nsIFrame::kPopupList,
                                       nsIFrame::kSelectPopupList};
  for (nsIFrame::ChildListIterator childLists(aFrame); !childLists.IsDone();
       childLists.Next()) {
    if (skip.contains(childLists.CurrentID())) {
      continue;
    }

    for (nsIFrame* child : childLists.CurrentList()) {
      nsRect r = rect - child->GetPosition();
      if (!r.IntersectRect(r, child->GetVisualOverflowRect())) {
        continue;
      }
      if (child->IsTransformed()) {
        // for children of a preserve3d element we just pass down the same dirty
        // rect
        if (!preserves3DChildren ||
            !child->Combines3DTransformWithAncestors()) {
          const nsRect overflow = child->GetVisualOverflowRectRelativeToSelf();
          nsRect out;
          if (nsDisplayTransform::UntransformRect(r, overflow, child, &out)) {
            r = out;
          } else {
            r.SetEmpty();
          }
        }
      }
      MarkFramesInSubtreeApproximatelyVisible(child, r);
    }
  }
}

void PresShell::RebuildApproximateFrameVisibility(
    nsRect* aRect, bool aRemoveOnly /* = false */) {
  MOZ_ASSERT(!mApproximateFrameVisibilityVisited, "already visited?");
  mApproximateFrameVisibilityVisited = true;

  nsIFrame* rootFrame = GetRootFrame();
  if (!rootFrame) {
    return;
  }

  // Remove the entries of the mApproximatelyVisibleFrames hashtable and put
  // them in oldApproximatelyVisibleFrames.
  VisibleFrames oldApproximatelyVisibleFrames;
  mApproximatelyVisibleFrames.SwapElements(oldApproximatelyVisibleFrames);

  nsRect vis(nsPoint(0, 0), rootFrame->GetSize());
  if (aRect) {
    vis = *aRect;
  }

  MarkFramesInSubtreeApproximatelyVisible(rootFrame, vis, aRemoveOnly);

  DecApproximateVisibleCount(oldApproximatelyVisibleFrames);
}

void PresShell::UpdateApproximateFrameVisibility() {
  DoUpdateApproximateFrameVisibility(/* aRemoveOnly = */ false);
}

void PresShell::DoUpdateApproximateFrameVisibility(bool aRemoveOnly) {
  MOZ_ASSERT(
      !mPresContext || mPresContext->IsRootContentDocument(),
      "Updating approximate frame visibility on a non-root content document?");

  mUpdateApproximateFrameVisibilityEvent.Revoke();

  if (mHaveShutDown || mIsDestroying) {
    return;
  }

  // call update on that frame
  nsIFrame* rootFrame = GetRootFrame();
  if (!rootFrame) {
    ClearApproximatelyVisibleFramesList(Some(OnNonvisible::DiscardImages));
    return;
  }

  RebuildApproximateFrameVisibility(/* aRect = */ nullptr, aRemoveOnly);
  ClearApproximateFrameVisibilityVisited(rootFrame->GetView(), true);

#ifdef DEBUG_FRAME_VISIBILITY_DISPLAY_LIST
  // This can be used to debug the frame walker by comparing beforeFrameList
  // and mApproximatelyVisibleFrames in RebuildFrameVisibilityDisplayList to see
  // if they produce the same results (mApproximatelyVisibleFrames holds the
  // frames the display list thinks are visible, beforeFrameList holds the
  // frames the frame walker thinks are visible).
  nsDisplayListBuilder builder(
      rootFrame, nsDisplayListBuilderMode::FRAME_VISIBILITY, false);
  nsRect updateRect(nsPoint(0, 0), rootFrame->GetSize());
  nsIFrame* rootScroll = GetRootScrollFrame();
  if (rootScroll) {
    nsIContent* content = rootScroll->GetContent();
    if (content) {
      Unused << nsLayoutUtils::GetDisplayPortForVisibilityTesting(
          content, &updateRect, RelativeTo::ScrollFrame);
    }

    if (IgnoringViewportScrolling()) {
      builder.SetIgnoreScrollFrame(rootScroll);
    }
  }
  builder.IgnorePaintSuppression();
  builder.EnterPresShell(rootFrame);
  nsDisplayList list;
  rootFrame->BuildDisplayListForStackingContext(&builder, updateRect, &list);
  builder.LeavePresShell(rootFrame, &list);

  RebuildApproximateFrameVisibilityDisplayList(list);

  ClearApproximateFrameVisibilityVisited(rootFrame->GetView(), true);

  list.DeleteAll(&builder);
#endif
}

bool PresShell::AssumeAllFramesVisible() {
  if (!StaticPrefs::layout_framevisibility_enabled() || !mPresContext ||
      !mDocument) {
    return true;
  }

  // We assume all frames are visible in print, print preview, chrome, and
  // resource docs and don't keep track of them.
  if (mPresContext->Type() == nsPresContext::eContext_PrintPreview ||
      mPresContext->Type() == nsPresContext::eContext_Print ||
      mPresContext->IsChrome() || mDocument->IsResourceDoc()) {
    return true;
  }

  // If we're assuming all frames are visible in the top level content
  // document, we need to in subdocuments as well. Otherwise we can get in a
  // situation where things like animations won't work in subdocuments because
  // their frames appear not to be visible, since we won't schedule an image
  // visibility update if the top level content document is assuming all
  // frames are visible.
  //
  // Note that it's not safe to call IsRootContentDocument() if we're
  // currently being destroyed, so we have to check that first.
  if (!mHaveShutDown && !mIsDestroying &&
      !mPresContext->IsRootContentDocumentInProcess()) {
    nsPresContext* presContext =
        mPresContext->GetToplevelContentDocumentPresContext();
    if (presContext && presContext->PresShell()->AssumeAllFramesVisible()) {
      return true;
    }
  }

  return false;
}

void PresShell::ScheduleApproximateFrameVisibilityUpdateSoon() {
  if (AssumeAllFramesVisible()) {
    return;
  }

  if (!mPresContext) {
    return;
  }

  nsRefreshDriver* refreshDriver = mPresContext->RefreshDriver();
  if (!refreshDriver) {
    return;
  }

  // Ask the refresh driver to update frame visibility soon.
  refreshDriver->ScheduleFrameVisibilityUpdate();
}

void PresShell::ScheduleApproximateFrameVisibilityUpdateNow() {
  if (AssumeAllFramesVisible()) {
    return;
  }

  if (!mPresContext->IsRootContentDocument()) {
    nsPresContext* presContext =
        mPresContext->GetToplevelContentDocumentPresContext();
    if (!presContext) return;
    MOZ_ASSERT(presContext->IsRootContentDocument(),
               "Didn't get a root prescontext from "
               "GetToplevelContentDocumentPresContext?");
    presContext->PresShell()->ScheduleApproximateFrameVisibilityUpdateNow();
    return;
  }

  if (mHaveShutDown || mIsDestroying) {
    return;
  }

  if (mUpdateApproximateFrameVisibilityEvent.IsPending()) {
    return;
  }

  RefPtr<nsRunnableMethod<PresShell>> event =
      NewRunnableMethod("PresShell::UpdateApproximateFrameVisibility", this,
                        &PresShell::UpdateApproximateFrameVisibility);
  nsresult rv = mDocument->Dispatch(TaskCategory::Other, do_AddRef(event));

  if (NS_SUCCEEDED(rv)) {
    mUpdateApproximateFrameVisibilityEvent = std::move(event);
  }
}

void PresShell::EnsureFrameInApproximatelyVisibleList(nsIFrame* aFrame) {
  if (!aFrame->TrackingVisibility()) {
    return;
  }

  if (AssumeAllFramesVisible()) {
    aFrame->IncApproximateVisibleCount();
    return;
  }

#ifdef DEBUG
  // Make sure it's in this pres shell.
  nsCOMPtr<nsIContent> content = aFrame->GetContent();
  if (content) {
    PresShell* presShell = content->OwnerDoc()->GetPresShell();
    MOZ_ASSERT(!presShell || presShell == this, "wrong shell");
  }
#endif

  if (mApproximatelyVisibleFrames.EnsureInserted(aFrame)) {
    // We inserted a new entry.
    aFrame->IncApproximateVisibleCount();
  }
}

void PresShell::RemoveFrameFromApproximatelyVisibleList(nsIFrame* aFrame) {
#ifdef DEBUG
  // Make sure it's in this pres shell.
  nsCOMPtr<nsIContent> content = aFrame->GetContent();
  if (content) {
    PresShell* presShell = content->OwnerDoc()->GetPresShell();
    MOZ_ASSERT(!presShell || presShell == this, "wrong shell");
  }
#endif

  if (AssumeAllFramesVisible()) {
    MOZ_ASSERT(mApproximatelyVisibleFrames.Count() == 0,
               "Shouldn't have any frames in the table");
    return;
  }

  if (mApproximatelyVisibleFrames.EnsureRemoved(aFrame) &&
      aFrame->TrackingVisibility()) {
    // aFrame was in the hashtable, and we're still tracking its visibility,
    // so we need to decrement its visible count.
    aFrame->DecApproximateVisibleCount();
  }
}

class nsAutoNotifyDidPaint {
 public:
  nsAutoNotifyDidPaint(PresShell* aShell, PaintFlags aFlags)
      : mShell(aShell), mFlags(aFlags) {}
  ~nsAutoNotifyDidPaint() {
    if (!!(mFlags & PaintFlags::PaintComposite)) {
      mShell->GetPresContext()->NotifyDidPaintForSubtree();
    }
  }

 private:
  PresShell* mShell;
  PaintFlags mFlags;
};

void PresShell::Paint(nsView* aViewToPaint, const nsRegion& aDirtyRegion,
                      PaintFlags aFlags) {
  nsCString url;
  nsIURI* uri = mDocument->GetDocumentURI();
  Document* contentRoot = GetPrimaryContentDocument();
  if (contentRoot) {
    uri = contentRoot->GetDocumentURI();
  }
  url = uri ? uri->GetSpecOrDefault() : NS_LITERAL_CSTRING("N/A");
#ifdef MOZ_GECKO_PROFILER
  AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING("PresShell::Paint", GRAPHICS, url);
#endif

  Maybe<js::AutoAssertNoContentJS> nojs;

  // On Android, Flash can call into content JS during painting, so we can't
  // assert there. However, we don't rely on this assertion on Android because
  // we don't paint while JS is running.
#if !defined(MOZ_WIDGET_ANDROID)
  if (!(aFlags & PaintFlags::PaintComposite)) {
    // We need to allow content JS when the flag is set since we may trigger
    // MozAfterPaint events in content in those cases.
    nojs.emplace(dom::danger::GetJSContext());
  }
#endif

  NS_ASSERTION(!mIsDestroying, "painting a destroyed PresShell");
  NS_ASSERTION(aViewToPaint, "null view");

  MOZ_ASSERT(!mApproximateFrameVisibilityVisited, "Should have been cleared");

  if (!mIsActive) {
    return;
  }

  if (StaticPrefs::apz_keyboard_enabled_AtStartup()) {
    // Update the focus target for async keyboard scrolling. This will be
    // forwarded to APZ by nsDisplayList::PaintRoot. We need to to do this
    // before we enter the paint phase because dispatching eVoid events can
    // cause layout to happen.
    mAPZFocusTarget = FocusTarget(this, mAPZFocusSequenceNumber);
  }

  nsPresContext* presContext = GetPresContext();
  AUTO_LAYOUT_PHASE_ENTRY_POINT(presContext, Paint);

  nsIFrame* frame = aViewToPaint->GetFrame();

  LayerManager* layerManager = aViewToPaint->GetWidget()->GetLayerManager();
  NS_ASSERTION(layerManager, "Must be in paint event");
  bool shouldInvalidate = layerManager->NeedsWidgetInvalidation();

  nsAutoNotifyDidPaint notifyDidPaint(this, aFlags);

  // Whether or not we should set first paint when painting is suppressed
  // is debatable. For now we'll do it because B2G relied on first paint
  // to configure the viewport and we only want to do that when we have
  // real content to paint. See Bug 798245
  if (mIsFirstPaint && !mPaintingSuppressed) {
    layerManager->SetIsFirstPaint();
    mIsFirstPaint = false;
  }

  if (!layerManager->BeginTransaction(url)) {
    return;
  }

  // Send an updated focus target with this transaction. Be sure to do this
  // before we paint in the case this is an empty transaction.
  layerManager->SetFocusTarget(mAPZFocusTarget);

  if (frame) {
    // Try to do an empty transaction, if the frame tree does not
    // need to be updated. Do not try to do an empty transaction on
    // a non-retained layer manager (like the BasicLayerManager that
    // draws the window title bar on Mac), because a) it won't work
    // and b) below we don't want to clear NS_FRAME_UPDATE_LAYER_TREE,
    // that will cause us to forget to update the real layer manager!

    if (!(aFlags & PaintFlags::PaintLayers)) {
      if (layerManager->EndEmptyTransaction()) {
        return;
      }
      NS_WARNING("Must complete empty transaction when compositing!");
    }

    if (!(aFlags & PaintFlags::PaintSyncDecodeImages) &&
        !(frame->GetStateBits() & NS_FRAME_UPDATE_LAYER_TREE) &&
        !mNextPaintCompressed) {
      NotifySubDocInvalidationFunc computeInvalidFunc =
          presContext->MayHavePaintEventListenerInSubDocument()
              ? nsPresContext::NotifySubDocInvalidation
              : 0;
      bool computeInvalidRect =
          computeInvalidFunc ||
          (layerManager->GetBackendType() == LayersBackend::LAYERS_BASIC);

      UniquePtr<LayerProperties> props;
      // For WR, the layermanager has no root layer. We want to avoid
      // calling ComputeDifferences in that case because it assumes non-null
      // and crashes.
      if (computeInvalidRect && layerManager->GetRoot()) {
        props = LayerProperties::CloneFrom(layerManager->GetRoot());
      }

      MaybeSetupTransactionIdAllocator(layerManager, presContext);

      if (layerManager->EndEmptyTransaction(
              (aFlags & PaintFlags::PaintComposite)
                  ? LayerManager::END_DEFAULT
                  : LayerManager::END_NO_COMPOSITE)) {
        nsIntRegion invalid;
        bool areaOverflowed = false;
        if (props) {
          if (!props->ComputeDifferences(layerManager->GetRoot(), invalid,
                                         computeInvalidFunc)) {
            areaOverflowed = true;
          }
        } else {
          LayerProperties::ClearInvalidations(layerManager->GetRoot());
        }
        if (props && !areaOverflowed) {
          if (!invalid.IsEmpty()) {
            nsIntRect bounds = invalid.GetBounds();
            nsRect rect(presContext->DevPixelsToAppUnits(bounds.x),
                        presContext->DevPixelsToAppUnits(bounds.y),
                        presContext->DevPixelsToAppUnits(bounds.width),
                        presContext->DevPixelsToAppUnits(bounds.height));
            if (shouldInvalidate) {
              aViewToPaint->GetViewManager()->InvalidateViewNoSuppression(
                  aViewToPaint, rect);
            }
            presContext->NotifyInvalidation(
                layerManager->GetLastTransactionId(), bounds);
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
  PaintFrameFlags flags =
      PaintFrameFlags::WidgetLayers | PaintFrameFlags::ExistingTransaction;
  if (!(aFlags & PaintFlags::PaintComposite)) {
    flags |= PaintFrameFlags::NoComposite;
  }
  if (aFlags & PaintFlags::PaintSyncDecodeImages) {
    flags |= PaintFrameFlags::SyncDecodeImages;
  }
  if (mNextPaintCompressed) {
    flags |= PaintFrameFlags::Compressed;
    mNextPaintCompressed = false;
  }
  if (layerManager->GetBackendType() == layers::LayersBackend::LAYERS_WR) {
    flags |= PaintFrameFlags::ForWebRender;
  }

  if (frame) {
    // We can paint directly into the widget using its layer manager.
    nsLayoutUtils::PaintFrame(nullptr, frame, aDirtyRegion, bgcolor,
                              nsDisplayListBuilderMode::Painting, flags);
    return;
  }

  if (layerManager->GetBackendType() == layers::LayersBackend::LAYERS_WR) {
    nsPresContext* pc = GetPresContext();
    LayoutDeviceRect bounds = LayoutDeviceRect::FromAppUnits(
        pc->GetVisibleArea(), pc->AppUnitsPerDevPixel());
    bgcolor = NS_ComposeColors(bgcolor, mCanvasBackgroundColor);
    WebRenderBackgroundData data(wr::ToLayoutRect(bounds),
                                 wr::ToColorF(ToDeviceColor(bgcolor)));
    WrFiltersHolder wrFilters;

    MaybeSetupTransactionIdAllocator(layerManager, presContext);
    layerManager->AsWebRenderLayerManager()->EndTransactionWithoutLayer(
        nullptr, nullptr, std::move(wrFilters), &data);
    return;
  }

  RefPtr<ColorLayer> root = layerManager->CreateColorLayer();
  if (root) {
    nsPresContext* pc = GetPresContext();
    nsIntRect bounds =
        pc->GetVisibleArea().ToOutsidePixels(pc->AppUnitsPerDevPixel());
    bgcolor = NS_ComposeColors(bgcolor, mCanvasBackgroundColor);
    root->SetColor(ToDeviceColor(bgcolor));
    root->SetVisibleRegion(LayerIntRegion::FromUnknownRegion(bounds));
    layerManager->SetRoot(root);
  }
  MaybeSetupTransactionIdAllocator(layerManager, presContext);
  layerManager->EndTransaction(nullptr, nullptr,
                               (aFlags & PaintFlags::PaintComposite)
                                   ? LayerManager::END_DEFAULT
                                   : LayerManager::END_NO_COMPOSITE);
}

// static
void PresShell::SetCapturingContent(nsIContent* aContent, CaptureFlags aFlags) {
  // If capture was set for pointer lock, don't unlock unless we are coming
  // out of pointer lock explicitly.
  if (!aContent && sCapturingContentInfo.mPointerLock &&
      !(aFlags & CaptureFlags::PointerLock)) {
    return;
  }

  sCapturingContentInfo.mContent = nullptr;

  // only set capturing content if allowed or the
  // CaptureFlags::IgnoreAllowedState or CaptureFlags::PointerLock are used.
  if ((aFlags & CaptureFlags::IgnoreAllowedState) ||
      sCapturingContentInfo.mAllowed || (aFlags & CaptureFlags::PointerLock)) {
    if (aContent) {
      sCapturingContentInfo.mContent = aContent;
    }
    // CaptureFlags::PointerLock is the same as
    // CaptureFlags::RetargetToElement & CaptureFlags::IgnoreAllowedState.
    sCapturingContentInfo.mRetargetToElement =
        !!(aFlags & CaptureFlags::RetargetToElement) ||
        !!(aFlags & CaptureFlags::PointerLock);
    sCapturingContentInfo.mPreventDrag =
        !!(aFlags & CaptureFlags::PreventDragStart);
    sCapturingContentInfo.mPointerLock = !!(aFlags & CaptureFlags::PointerLock);
  }
}

nsIContent* PresShell::GetCurrentEventContent() {
  if (mCurrentEventContent &&
      mCurrentEventContent->GetComposedDoc() != mDocument) {
    mCurrentEventContent = nullptr;
    mCurrentEventFrame = nullptr;
  }
  return mCurrentEventContent;
}

nsIFrame* PresShell::GetCurrentEventFrame() {
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

already_AddRefed<nsIContent> PresShell::GetEventTargetContent(
    WidgetEvent* aEvent) {
  nsCOMPtr<nsIContent> content = GetCurrentEventContent();
  if (!content) {
    nsIFrame* currentEventFrame = GetCurrentEventFrame();
    if (currentEventFrame) {
      currentEventFrame->GetContentForEvent(aEvent, getter_AddRefs(content));
      NS_ASSERTION(!content || content->GetComposedDoc() == mDocument,
                   "handing out content from a different doc");
    }
  }
  return content.forget();
}

void PresShell::PushCurrentEventInfo(nsIFrame* aFrame, nsIContent* aContent) {
  if (mCurrentEventFrame || mCurrentEventContent) {
    mCurrentEventFrameStack.InsertElementAt(0, mCurrentEventFrame);
    mCurrentEventContentStack.InsertObjectAt(mCurrentEventContent, 0);
  }
  mCurrentEventFrame = aFrame;
  mCurrentEventContent = aContent;
}

void PresShell::PopCurrentEventInfo() {
  mCurrentEventFrame = nullptr;
  mCurrentEventContent = nullptr;

  if (0 != mCurrentEventFrameStack.Length()) {
    mCurrentEventFrame = mCurrentEventFrameStack.ElementAt(0);
    mCurrentEventFrameStack.RemoveElementAt(0);
    mCurrentEventContent = mCurrentEventContentStack.ObjectAt(0);
    mCurrentEventContentStack.RemoveObjectAt(0);

    // Don't use it if it has moved to a different document.
    if (mCurrentEventContent &&
        mCurrentEventContent->GetComposedDoc() != mDocument) {
      mCurrentEventContent = nullptr;
      mCurrentEventFrame = nullptr;
    }
  }
}

// static
bool PresShell::EventHandler::InZombieDocument(nsIContent* aContent) {
  // If a content node points to a null document, or the document is not
  // attached to a window, then it is possibly in a zombie document,
  // about to be replaced by a newly loading document.
  // Such documents cannot handle DOM events.
  // It might actually be in a node not attached to any document,
  // in which case there is not parent presshell to retarget it to.
  Document* doc = aContent->GetComposedDoc();
  return !doc || !doc->GetWindow();
}

already_AddRefed<nsPIDOMWindowOuter> PresShell::GetRootWindow() {
  nsCOMPtr<nsPIDOMWindowOuter> window = mDocument->GetWindow();
  if (window) {
    nsCOMPtr<nsPIDOMWindowOuter> rootWindow = window->GetPrivateRoot();
    NS_ASSERTION(rootWindow, "nsPIDOMWindow::GetPrivateRoot() returns NULL");
    return rootWindow.forget();
  }

  // If we don't have DOM window, we're zombie, we should find the root window
  // with our parent shell.
  RefPtr<PresShell> parentPresShell = GetParentPresShellForEventHandling();
  NS_ENSURE_TRUE(parentPresShell, nullptr);
  return parentPresShell->GetRootWindow();
}

already_AddRefed<nsPIDOMWindowOuter>
PresShell::GetFocusedDOMWindowInOurWindow() {
  nsCOMPtr<nsPIDOMWindowOuter> rootWindow = GetRootWindow();
  NS_ENSURE_TRUE(rootWindow, nullptr);
  nsCOMPtr<nsPIDOMWindowOuter> focusedWindow;
  nsFocusManager::GetFocusedDescendant(rootWindow,
                                       nsFocusManager::eIncludeAllDescendants,
                                       getter_AddRefs(focusedWindow));
  return focusedWindow.forget();
}

already_AddRefed<nsIContent> PresShell::GetFocusedContentInOurWindow() const {
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm && mDocument) {
    RefPtr<Element> focusedElement;
    fm->GetFocusedElementForWindow(mDocument->GetWindow(), false, nullptr,
                                   getter_AddRefs(focusedElement));
    return focusedElement.forget();
  }
  return nullptr;
}

already_AddRefed<PresShell> PresShell::GetParentPresShellForEventHandling() {
  NS_ENSURE_TRUE(mPresContext, nullptr);

  // Now, find the parent pres shell and send the event there
  nsCOMPtr<nsIDocShellTreeItem> treeItem = mPresContext->GetDocShell();
  if (!treeItem) {
    treeItem = mForwardingContainer.get();
  }

  // Might have gone away, or never been around to start with
  NS_ENSURE_TRUE(treeItem, nullptr);

  nsCOMPtr<nsIDocShellTreeItem> parentTreeItem;
  treeItem->GetInProcessParent(getter_AddRefs(parentTreeItem));
  nsCOMPtr<nsIDocShell> parentDocShell = do_QueryInterface(parentTreeItem);
  NS_ENSURE_TRUE(parentDocShell && treeItem != parentTreeItem, nullptr);

  RefPtr<PresShell> parentPresShell = parentDocShell->GetPresShell();
  return parentPresShell.forget();
}

nsresult PresShell::EventHandler::RetargetEventToParent(
    WidgetGUIEvent* aGUIEvent, nsEventStatus* aEventStatus) {
  // Send this events straight up to the parent pres shell.
  // We do this for keystroke events in zombie documents or if either a frame
  // or a root content is not present.
  // That way at least the UI key bindings can work.

  RefPtr<PresShell> parentPresShell = GetParentPresShellForEventHandling();
  NS_ENSURE_TRUE(parentPresShell, NS_ERROR_FAILURE);

  // Fake the event as though it's from the parent pres shell's root frame.
  return parentPresShell->HandleEvent(parentPresShell->GetRootFrame(),
                                      aGUIEvent, true, aEventStatus);
}

void PresShell::DisableNonTestMouseEvents(bool aDisable) {
  sDisableNonTestMouseEvents = aDisable;
}

bool PresShell::MouseLocationWasSetBySynthesizedMouseEventForTests() const {
  if (!mPresContext) {
    return false;
  }
  if (mPresContext->IsRoot()) {
    return mMouseLocationWasSetBySynthesizedMouseEventForTests;
  }
  PresShell* rootPresShell = GetRootPresShell();
  return rootPresShell &&
         rootPresShell->mMouseLocationWasSetBySynthesizedMouseEventForTests;
}

void PresShell::RecordMouseLocation(WidgetGUIEvent* aEvent) {
  if (!mPresContext) return;

  if (!mPresContext->IsRoot()) {
    PresShell* rootPresShell = GetRootPresShell();
    if (rootPresShell) {
      rootPresShell->RecordMouseLocation(aEvent);
    }
    return;
  }

  if ((aEvent->mMessage == eMouseMove &&
       aEvent->AsMouseEvent()->mReason == WidgetMouseEvent::eReal) ||
      aEvent->mMessage == eMouseEnterIntoWidget ||
      aEvent->mMessage == eMouseDown || aEvent->mMessage == eMouseUp) {
    nsIFrame* rootFrame = GetRootFrame();
    if (!rootFrame) {
      nsView* rootView = mViewManager->GetRootView();
      mMouseLocation = nsLayoutUtils::TranslateWidgetToView(
          mPresContext, aEvent->mWidget, aEvent->mRefPoint, rootView);
      mMouseEventTargetGuid = InputAPZContext::GetTargetLayerGuid();
    } else {
      mMouseLocation = nsLayoutUtils::GetEventCoordinatesRelativeTo(
          aEvent, RelativeTo{rootFrame, ViewportType::Visual});
      mMouseEventTargetGuid = InputAPZContext::GetTargetLayerGuid();
    }
    mMouseLocationWasSetBySynthesizedMouseEventForTests =
        aEvent->mFlags.mIsSynthesizedForTests;
#ifdef DEBUG_MOUSE_LOCATION
    if (aEvent->mMessage == eMouseEnterIntoWidget) {
      printf("[ps=%p]got mouse enter for %p\n", this, aEvent->mWidget);
    }
    printf("[ps=%p]setting mouse location to (%d,%d)\n", this, mMouseLocation.x,
           mMouseLocation.y);
#endif
    if (aEvent->mMessage == eMouseEnterIntoWidget) {
      SynthesizeMouseMove(false);
    }
  } else if (aEvent->mMessage == eMouseExitFromWidget) {
    // Although we only care about the mouse moving into an area for which this
    // pres shell doesn't receive mouse move events, we don't check which widget
    // the mouse exit was for since this seems to vary by platform.  Hopefully
    // this won't matter at all since we'll get the mouse move or enter after
    // the mouse exit when the mouse moves from one of our widgets into another.
    mMouseLocation = nsPoint(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
    mMouseEventTargetGuid = InputAPZContext::GetTargetLayerGuid();
    mMouseLocationWasSetBySynthesizedMouseEventForTests =
        aEvent->mFlags.mIsSynthesizedForTests;
#ifdef DEBUG_MOUSE_LOCATION
    printf("[ps=%p]got mouse exit for %p\n", this, aEvent->mWidget);
    printf("[ps=%p]clearing mouse location\n", this);
#endif
  }
}

// static
nsIFrame* PresShell::EventHandler::GetNearestFrameContainingPresShell(
    PresShell* aPresShell) {
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

static CallState FlushThrottledStyles(Document& aDocument) {
  PresShell* presShell = aDocument.GetPresShell();
  if (presShell && presShell->IsVisible()) {
    if (nsPresContext* presContext = presShell->GetPresContext()) {
      presContext->RestyleManager()->UpdateOnlyAnimationStyles();
    }
  }

  aDocument.EnumerateSubDocuments(FlushThrottledStyles);
  return CallState::Continue;
}

bool PresShell::CanDispatchEvent(const WidgetGUIEvent* aEvent) const {
  bool rv =
      mPresContext && !mHaveShutDown && nsContentUtils::IsSafeToRunScript();
  if (aEvent) {
    rv &= (aEvent && aEvent->mWidget && !aEvent->mWidget->Destroyed());
  }
  return rv;
}

/* static */
PresShell* PresShell::GetShellForEventTarget(nsIFrame* aFrame,
                                             nsIContent* aContent) {
  if (aFrame) {
    return aFrame->PresShell();
  }
  if (aContent) {
    Document* doc = aContent->GetComposedDoc();
    if (!doc) {
      return nullptr;
    }
    return doc->GetPresShell();
  }
  return nullptr;
}

/* static */
PresShell* PresShell::GetShellForTouchEvent(WidgetGUIEvent* aEvent) {
  switch (aEvent->mMessage) {
    case eTouchMove:
    case eTouchCancel:
    case eTouchEnd: {
      // get the correct shell to dispatch to
      WidgetTouchEvent* touchEvent = aEvent->AsTouchEvent();
      for (dom::Touch* touch : touchEvent->mTouches) {
        if (!touch) {
          return nullptr;
        }

        RefPtr<dom::Touch> oldTouch =
            TouchManager::GetCapturedTouch(touch->Identifier());
        if (!oldTouch) {
          return nullptr;
        }

        nsCOMPtr<nsIContent> content = do_QueryInterface(oldTouch->GetTarget());
        if (!content) {
          return nullptr;
        }

        nsIFrame* contentFrame = content->GetPrimaryFrame();
        if (!contentFrame) {
          return nullptr;
        }

        PresShell* presShell = contentFrame->PresContext()->GetPresShell();
        if (presShell) {
          return presShell;
        }
      }
      return nullptr;
    }
    default:
      return nullptr;
  }
}

nsresult PresShell::HandleEvent(nsIFrame* aFrameForPresShell,
                                WidgetGUIEvent* aGUIEvent,
                                bool aDontRetargetEvents,
                                nsEventStatus* aEventStatus) {
  MOZ_ASSERT(aGUIEvent);
  // If it's synthesized in the parent process and our mouse location was set
  // by a mouse event which was synthesized for tests because the test does not
  // want to change `:hover` state with the synthesized mouse event for native
  // mouse cursor position.
  if (aGUIEvent->mMessage == eMouseMove &&
      aGUIEvent->CameFromAnotherProcess() && XRE_IsContentProcess() &&
      !aGUIEvent->mFlags.mIsSynthesizedForTests &&
      MouseLocationWasSetBySynthesizedMouseEventForTests() &&
      aGUIEvent->AsMouseEvent()->mReason == WidgetMouseEvent::eSynthesized) {
    return NS_OK;
  }
  EventHandler eventHandler(*this);
  return eventHandler.HandleEvent(aFrameForPresShell, aGUIEvent,
                                  aDontRetargetEvents, aEventStatus);
}

nsresult PresShell::EventHandler::HandleEvent(nsIFrame* aFrameForPresShell,
                                              WidgetGUIEvent* aGUIEvent,
                                              bool aDontRetargetEvents,
                                              nsEventStatus* aEventStatus) {
  MOZ_ASSERT(aGUIEvent);
  MOZ_DIAGNOSTIC_ASSERT(aGUIEvent->IsTrusted());
  MOZ_ASSERT(aEventStatus);

#ifdef MOZ_TASK_TRACER
  Maybe<AutoSourceEvent> taskTracerEvent;
  if (MOZ_UNLIKELY(IsStartLogging())) {
    // Make touch events, mouse events and hardware key events to be
    // the source events of TaskTracer, and originate the rest
    // correlation tasks from here.
    SourceEventType type = SourceEventType::Unknown;
    if (aGUIEvent->AsTouchEvent()) {
      type = SourceEventType::Touch;
    } else if (aGUIEvent->AsMouseEvent()) {
      type = SourceEventType::Mouse;
    } else if (aGUIEvent->AsKeyboardEvent()) {
      type = SourceEventType::Key;
    }
    taskTracerEvent.emplace(type);
  }
#endif

  NS_ASSERTION(aFrameForPresShell, "aFrameForPresShell should be not null");

  // Update the latest focus sequence number with this new sequence number;
  // the next transasction that gets sent to the compositor will carry this over
  if (mPresShell->mAPZFocusSequenceNumber < aGUIEvent->mFocusSequenceNumber) {
    mPresShell->mAPZFocusSequenceNumber = aGUIEvent->mFocusSequenceNumber;
  }

  if (mPresShell->IsDestroying() ||
      (PresShell::sDisableNonTestMouseEvents &&
       !aGUIEvent->mFlags.mIsSynthesizedForTests &&
       aGUIEvent->HasMouseEventMessage())) {
    return NS_OK;
  }

  mPresShell->RecordMouseLocation(aGUIEvent);

  if (MaybeHandleEventWithAccessibleCaret(aFrameForPresShell, aGUIEvent,
                                          aEventStatus)) {
    // Probably handled by AccessibleCaretEventHub.
    return NS_OK;
  }

  if (MaybeDiscardEvent(aGUIEvent)) {
    // Nobody cannot handle the event for now.
    return NS_OK;
  }

  if (!aDontRetargetEvents) {
    // If aGUIEvent should be handled in another PresShell, we should call its
    // HandleEvent() and do nothing here.
    nsresult rv = NS_OK;
    if (MaybeHandleEventWithAnotherPresShell(aFrameForPresShell, aGUIEvent,
                                             aEventStatus, &rv)) {
      // Handled by another PresShell or nobody can handle the event.
      return rv;
    }
  }

  if (MaybeDiscardOrDelayKeyboardEvent(aGUIEvent)) {
    // The event is discarded or put into the delayed event queue.
    return NS_OK;
  }

  if (aGUIEvent->IsUsingCoordinates()) {
    return HandleEventUsingCoordinates(aFrameForPresShell, aGUIEvent,
                                       aEventStatus, aDontRetargetEvents);
  }

  // Activation events need to be dispatched even if no frame was found, since
  // we don't want the focus to be out of sync.
  if (!aFrameForPresShell) {
    if (!NS_EVENT_NEEDS_FRAME(aGUIEvent)) {
      // Push nullptr for both current event target content and frame since
      // there is no frame but the event does not require a frame.
      AutoCurrentEventInfoSetter eventInfoSetter(*this);
      return HandleEventWithCurrentEventInfo(aGUIEvent, aEventStatus, true,
                                             nullptr);
    }

    if (aGUIEvent->HasKeyEventMessage()) {
      // Keypress events in new blank tabs should not be completely thrown away.
      // Retarget them -- the parent chrome shell might make use of them.
      return RetargetEventToParent(aGUIEvent, aEventStatus);
    }

    return NS_OK;
  }

  if (aGUIEvent->IsTargetedAtFocusedContent()) {
    return HandleEventAtFocusedContent(aGUIEvent, aEventStatus);
  }

  return HandleEventWithFrameForPresShell(aFrameForPresShell, aGUIEvent,
                                          aEventStatus);
}

nsresult PresShell::EventHandler::HandleEventUsingCoordinates(
    nsIFrame* aFrameForPresShell, WidgetGUIEvent* aGUIEvent,
    nsEventStatus* aEventStatus, bool aDontRetargetEvents) {
  MOZ_ASSERT(aGUIEvent);
  MOZ_ASSERT(aGUIEvent->IsUsingCoordinates());
  MOZ_ASSERT(aEventStatus);

  // Flush pending notifications to handle the event with the latest layout.
  // But if it causes destroying the frame for mPresShell, stop handling the
  // event. (why?)
  AutoWeakFrame weakFrame(aFrameForPresShell);
  MaybeFlushPendingNotifications(aGUIEvent);
  if (!weakFrame.IsAlive()) {
    *aEventStatus = nsEventStatus_eIgnore;
    return NS_OK;
  }

  // XXX Retrieving capturing content here.  However, some of the following
  //     methods allow to run script.  So, isn't it possible the capturing
  //     content outdated?
  nsCOMPtr<nsIContent> capturingContent =
      EventHandler::GetCapturingContentFor(aGUIEvent);

  if (GetDocument() && aGUIEvent->mClass == eTouchEventClass) {
    Document::UnlockPointer();
  }

  nsIFrame* frameForPresShell = MaybeFlushThrottledStyles(aFrameForPresShell);
  if (NS_WARN_IF(!frameForPresShell)) {
    return NS_OK;
  }

  bool isCapturingContentIgnored = false;
  bool isCaptureRetargeted = false;
  nsIFrame* rootFrameToHandleEvent = ComputeRootFrameToHandleEvent(
      frameForPresShell, aGUIEvent, capturingContent,
      &isCapturingContentIgnored, &isCaptureRetargeted);
  if (isCapturingContentIgnored) {
    capturingContent = nullptr;
  }

  // The order to generate pointer event is
  // 1. check pending pointer capture.
  // 2. check if there is a capturing content.
  // 3. hit test
  // 4. dispatch pointer events
  // 5. check whether the targets of all Touch instances are in the same
  //    document and suppress invalid instances.
  // 6. dispatch mouse or touch events.

  // Try to keep frame for following check, because frame can be damaged
  // during MaybeProcessPointerCapture.
  {
    AutoWeakFrame frameKeeper(rootFrameToHandleEvent);
    PointerEventHandler::MaybeProcessPointerCapture(aGUIEvent);
    // Prevent application crashes, in case damaged frame.
    if (!frameKeeper.IsAlive()) {
      NS_WARNING("Nothing to handle this event!");
      return NS_OK;
    }
  }

  // Only capture mouse events and pointer events.
  nsCOMPtr<nsIContent> pointerCapturingContent =
      PointerEventHandler::GetPointerCapturingContent(aGUIEvent);

  if (pointerCapturingContent) {
    rootFrameToHandleEvent = pointerCapturingContent->GetPrimaryFrame();
    if (!rootFrameToHandleEvent) {
      return HandleEventWithPointerCapturingContentWithoutItsFrame(
          aFrameForPresShell, aGUIEvent, pointerCapturingContent, aEventStatus);
    }
  }

  WidgetMouseEvent* mouseEvent = aGUIEvent->AsMouseEvent();
  bool isWindowLevelMouseExit =
      (aGUIEvent->mMessage == eMouseExitFromWidget) &&
      (mouseEvent && mouseEvent->mExitFrom == WidgetMouseEvent::eTopLevel);

  // Get the frame at the event point. However, don't do this if we're
  // capturing and retargeting the event because the captured frame will
  // be used instead below. Also keep using the root frame if we're dealing
  // with a window-level mouse exit event since we want to start sending
  // mouse out events at the root EventStateManager.
  EventTargetData eventTargetData(rootFrameToHandleEvent);
  if (!isCaptureRetargeted && !isWindowLevelMouseExit &&
      !pointerCapturingContent) {
    if (!ComputeEventTargetFrameAndPresShellAtEventPoint(
            rootFrameToHandleEvent, aGUIEvent, &eventTargetData)) {
      *aEventStatus = nsEventStatus_eIgnore;
      return NS_OK;
    }
  }

  // if a node is capturing the mouse, check if the event needs to be
  // retargeted at the capturing content instead. This will be the case when
  // capture retargeting is being used, no frame was found or the frame's
  // content is not a descendant of the capturing content.
  if (capturingContent && !pointerCapturingContent &&
      (PresShell::sCapturingContentInfo.mRetargetToElement ||
       !eventTargetData.mFrame->GetContent() ||
       !nsContentUtils::ContentIsCrossDocDescendantOf(
           eventTargetData.mFrame->GetContent(), capturingContent))) {
    // A check was already done above to ensure that capturingContent is
    // in this presshell.
    NS_ASSERTION(capturingContent->OwnerDoc() == GetDocument(),
                 "Unexpected document");
    nsIFrame* capturingFrame = capturingContent->GetPrimaryFrame();
    if (capturingFrame) {
      eventTargetData.SetFrameAndComputePresShell(capturingFrame);
    }
  }

  if (NS_WARN_IF(!eventTargetData.mFrame)) {
    return NS_OK;
  }

  // Suppress mouse event if it's being targeted at an element inside
  // a document which needs events suppressed
  if (MaybeDiscardOrDelayMouseEvent(eventTargetData.mFrame, aGUIEvent)) {
    return NS_OK;
  }

  // Check if we have an active EventStateManager which isn't the
  // EventStateManager of the current PresContext.  If that is the case, and
  // mouse is over some ancestor document, forward event handling to the
  // active document.  This way content can get mouse events even when mouse
  // is over the chrome or outside the window.
  if (eventTargetData.MaybeRetargetToActiveDocument(aGUIEvent) &&
      NS_WARN_IF(!eventTargetData.mFrame)) {
    return NS_OK;
  }

  if (!eventTargetData.ComputeElementFromFrame(aGUIEvent)) {
    return NS_OK;
  }
  // Note that even if ComputeElementFromFrame() returns true,
  // eventTargetData.mContent can be nullptr here.

  // Dispatch a pointer event if Pointer Events is enabled.  Note that if
  // pointer event listeners change the layout, eventTargetData is
  // automatically updated.
  if (!DispatchPrecedingPointerEvent(
          aFrameForPresShell, aGUIEvent, pointerCapturingContent,
          aDontRetargetEvents, &eventTargetData, aEventStatus)) {
    return NS_OK;
  }

  // frame could be null after dispatching pointer events.
  // XXX Despite of this comment, we update the event target data outside
  //     DispatchPrecedingPointerEvent().  Can we make it call
  //     UpdateTouchEventTarget()?
  eventTargetData.UpdateTouchEventTarget(aGUIEvent);

  // Handle the event in the correct shell.
  // We pass the subshell's root frame as the frame to start from. This is
  // the only correct alternative; if the event was captured then it
  // must have been captured by us or some ancestor shell and we
  // now ask the subshell to dispatch it normally.
  EventHandler eventHandler(*eventTargetData.mPresShell);
  AutoCurrentEventInfoSetter eventInfoSetter(eventHandler, eventTargetData);
  // eventTargetData is on the stack and is guaranteed to keep its
  // mOverrideClickTarget alive, so we can just use MOZ_KnownLive here.
  nsresult rv = eventHandler.HandleEventWithCurrentEventInfo(
      aGUIEvent, aEventStatus, true,
      MOZ_KnownLive(eventTargetData.mOverrideClickTarget));
#ifdef DEBUG
  eventTargetData.mPresShell->ShowEventTargetDebug();
#endif
  return rv;
}

bool PresShell::EventHandler::MaybeFlushPendingNotifications(
    WidgetGUIEvent* aGUIEvent) {
  MOZ_ASSERT(aGUIEvent);

  switch (aGUIEvent->mMessage) {
    case eMouseDown:
    case eMouseUp: {
      RefPtr<nsPresContext> presContext = mPresShell->GetPresContext();
      if (NS_WARN_IF(!presContext)) {
        return false;
      }
      uint64_t framesConstructedCount = presContext->FramesConstructedCount();
      uint64_t framesReflowedCount = presContext->FramesReflowedCount();

      MOZ_KnownLive(mPresShell)->FlushPendingNotifications(FlushType::Layout);
      return framesConstructedCount != presContext->FramesConstructedCount() ||
             framesReflowedCount != presContext->FramesReflowedCount();
    }
    default:
      return false;
  }
}

nsIFrame* PresShell::EventHandler::GetFrameToHandleNonTouchEvent(
    nsIFrame* aRootFrameToHandleEvent, WidgetGUIEvent* aGUIEvent) {
  MOZ_ASSERT(aGUIEvent);
  MOZ_ASSERT(aGUIEvent->mClass != eTouchEventClass);

  ViewportType viewportType = ViewportType::Layout;
  if (aRootFrameToHandleEvent->Type() == LayoutFrameType::Viewport &&
      aRootFrameToHandleEvent->PresContext()->IsRootContentDocument()) {
    viewportType = ViewportType::Visual;
  }
  RelativeTo relativeTo{aRootFrameToHandleEvent, viewportType};
  nsPoint eventPoint =
      nsLayoutUtils::GetEventCoordinatesRelativeTo(aGUIEvent, relativeTo);

  uint32_t flags = 0;
  if (aGUIEvent->mClass == eMouseEventClass) {
    WidgetMouseEvent* mouseEvent = aGUIEvent->AsMouseEvent();
    if (mouseEvent && mouseEvent->mIgnoreRootScrollFrame) {
      flags |= INPUT_IGNORE_ROOT_SCROLL_FRAME;
    }
  }

  nsIFrame* targetFrame =
      FindFrameTargetedByInputEvent(aGUIEvent, relativeTo, eventPoint, flags);
  if (!targetFrame) {
    return aRootFrameToHandleEvent;
  }

  if (targetFrame->PresShell() == mPresShell) {
    // If found target is in mPresShell, we've already found it in the latest
    // layout so that we can use it.
    return targetFrame;
  }

  // If target is in a child document, we've not flushed its layout yet.
  PresShell* childPresShell = targetFrame->PresShell();
  EventHandler childEventHandler(*childPresShell);
  AutoWeakFrame weakFrame(aRootFrameToHandleEvent);
  bool layoutChanged =
      childEventHandler.MaybeFlushPendingNotifications(aGUIEvent);
  if (!weakFrame.IsAlive()) {
    // Stop handling the event if the root frame to handle event is destroyed
    // by the reflow. (but why?)
    return nullptr;
  }
  if (!layoutChanged) {
    // If the layout in the child PresShell hasn't been changed, we don't
    // need to recompute the target.
    return targetFrame;
  }

  // Finally, we need to recompute the target with the latest layout.
  targetFrame =
      FindFrameTargetedByInputEvent(aGUIEvent, relativeTo, eventPoint, flags);

  return targetFrame ? targetFrame : aRootFrameToHandleEvent;
}

bool PresShell::EventHandler::ComputeEventTargetFrameAndPresShellAtEventPoint(
    nsIFrame* aRootFrameToHandleEvent, WidgetGUIEvent* aGUIEvent,
    EventTargetData* aEventTargetData) {
  MOZ_ASSERT(aRootFrameToHandleEvent);
  MOZ_ASSERT(aGUIEvent);
  MOZ_ASSERT(aEventTargetData);

  if (aGUIEvent->mClass == eTouchEventClass) {
    nsIFrame* targetFrameAtTouchEvent = TouchManager::SetupTarget(
        aGUIEvent->AsTouchEvent(), aRootFrameToHandleEvent);
    aEventTargetData->SetFrameAndComputePresShell(targetFrameAtTouchEvent);
    return true;
  }

  nsIFrame* targetFrame =
      GetFrameToHandleNonTouchEvent(aRootFrameToHandleEvent, aGUIEvent);
  aEventTargetData->SetFrameAndComputePresShell(targetFrame);
  return !!aEventTargetData->mFrame;
}

bool PresShell::EventHandler::DispatchPrecedingPointerEvent(
    nsIFrame* aFrameForPresShell, WidgetGUIEvent* aGUIEvent,
    nsIContent* aPointerCapturingContent, bool aDontRetargetEvents,
    EventTargetData* aEventTargetData, nsEventStatus* aEventStatus) {
  MOZ_ASSERT(aFrameForPresShell);
  MOZ_ASSERT(aGUIEvent);
  MOZ_ASSERT(aEventTargetData);
  MOZ_ASSERT(aEventStatus);

  if (!StaticPrefs::dom_w3c_pointer_events_enabled()) {
    return true;
  }

  // Dispatch pointer events from the mouse or touch events. Regarding
  // pointer events from mouse, we should dispatch those pointer events to
  // the same target as the source mouse events. We pass the frame found
  // in hit test to PointerEventHandler and dispatch pointer events to it.
  //
  // Regarding pointer events from touch, the behavior is different. Touch
  // events are dispatched to the same target as the target of touchstart.
  // Multiple touch points must be dispatched to the same document. Pointer
  // events from touch can be dispatched to different documents. We Pass the
  // original frame to PointerEventHandler, reentry PresShell::HandleEvent,
  // and do hit test for each point.
  nsIFrame* targetFrame = aGUIEvent->mClass == eTouchEventClass
                              ? aFrameForPresShell
                              : aEventTargetData->mFrame;

  if (aPointerCapturingContent) {
    aEventTargetData->mOverrideClickTarget =
        GetOverrideClickTarget(aGUIEvent, aFrameForPresShell);
    aEventTargetData->mPresShell =
        PresShell::GetShellForEventTarget(nullptr, aPointerCapturingContent);
    if (!aEventTargetData->mPresShell) {
      // If we can't process event for the capturing content, release
      // the capture.
      PointerEventHandler::ReleaseIfCaptureByDescendant(
          aPointerCapturingContent);
      return false;
    }

    targetFrame = aPointerCapturingContent->GetPrimaryFrame();
    aEventTargetData->mFrame = targetFrame;
  }

  AutoWeakFrame weakTargetFrame(targetFrame);
  AutoWeakFrame weakFrame(aEventTargetData->mFrame);
  nsCOMPtr<nsIContent> content(aEventTargetData->mContent);
  RefPtr<PresShell> presShell(aEventTargetData->mPresShell);
  nsCOMPtr<nsIContent> targetContent;
  PointerEventHandler::DispatchPointerFromMouseOrTouch(
      presShell, aEventTargetData->mFrame, content, aGUIEvent,
      aDontRetargetEvents, aEventStatus, getter_AddRefs(targetContent));

  // If the target frame is alive, the caller should keep handling the event
  // unless event target frame is destroyed.
  if (weakTargetFrame.IsAlive()) {
    return weakFrame.IsAlive();
  }

  // If the event is not a mouse event, the caller should keep handling the
  // event unless event target frame is destroyed.  Note that this case is
  // not defined by the spec.
  if (aGUIEvent->mClass != eMouseEventClass) {
    return weakFrame.IsAlive();
  }

  // Spec defines that mouse events must be dispatched to the same target as
  // the pointer event. If the target is no longer participating in its
  // ownerDocument's tree, fire the event at the original target's nearest
  // ancestor node
  if (!targetContent) {
    return false;
  }

  // XXX Why don't we reset aEventTargetData->mContent here?
  aEventTargetData->mFrame = targetContent->GetPrimaryFrame();
  aEventTargetData->mPresShell = PresShell::GetShellForEventTarget(
      aEventTargetData->mFrame, targetContent);

  // If new target PresShel is not found, we cannot keep handling the event.
  return !!aEventTargetData->mPresShell;
}

bool PresShell::EventHandler::MaybeHandleEventWithAccessibleCaret(
    nsIFrame* aFrameForPresShell, WidgetGUIEvent* aGUIEvent,
    nsEventStatus* aEventStatus) {
  MOZ_ASSERT(aGUIEvent);
  MOZ_ASSERT(aEventStatus);

  // Don't dispatch event to AccessibleCaretEventHub when the event status
  // is nsEventStatus_eConsumeNoDefault. This might be happened when content
  // preventDefault on the pointer events. In such case, we also call
  // preventDefault on mouse events to stop default behaviors.
  if (*aEventStatus == nsEventStatus_eConsumeNoDefault) {
    return false;
  }

  if (!AccessibleCaretEnabled(GetDocument()->GetDocShell())) {
    return false;
  }

  // AccessibleCaretEventHub handles only mouse, touch, and keyboard events.
  if (aGUIEvent->mClass != eMouseEventClass &&
      aGUIEvent->mClass != eTouchEventClass &&
      aGUIEvent->mClass != eKeyboardEventClass) {
    return false;
  }

  // First, try the event hub at the event point to handle a long press to
  // select a word in an unfocused window.
  do {
    EventTargetData eventTargetData(nullptr);
    if (!ComputeEventTargetFrameAndPresShellAtEventPoint(
            aFrameForPresShell, aGUIEvent, &eventTargetData)) {
      break;
    }

    if (!eventTargetData.mPresShell) {
      break;
    }

    RefPtr<AccessibleCaretEventHub> eventHub =
        eventTargetData.mPresShell->GetAccessibleCaretEventHub();
    if (!eventHub) {
      break;
    }

    *aEventStatus = eventHub->HandleEvent(aGUIEvent);
    if (*aEventStatus != nsEventStatus_eConsumeNoDefault) {
      break;
    }

    // If the event is consumed, cancel APZC panning by setting
    // mMultipleActionsPrevented.
    aGUIEvent->mFlags.mMultipleActionsPrevented = true;
    return true;
  } while (false);

  // Then, we target the event to the event hub at the focused window.
  nsCOMPtr<nsPIDOMWindowOuter> window = GetFocusedDOMWindowInOurWindow();
  if (!window) {
    return false;
  }
  RefPtr<Document> retargetEventDoc = window->GetExtantDoc();
  if (!retargetEventDoc) {
    return false;
  }
  RefPtr<PresShell> presShell = retargetEventDoc->GetPresShell();
  if (!presShell) {
    return false;
  }

  RefPtr<AccessibleCaretEventHub> eventHub =
      presShell->GetAccessibleCaretEventHub();
  if (!eventHub) {
    return false;
  }
  *aEventStatus = eventHub->HandleEvent(aGUIEvent);
  if (*aEventStatus != nsEventStatus_eConsumeNoDefault) {
    return false;
  }
  // If the event is consumed, cancel APZC panning by setting
  // mMultipleActionsPrevented.
  aGUIEvent->mFlags.mMultipleActionsPrevented = true;
  return true;
}

bool PresShell::EventHandler::MaybeDiscardEvent(WidgetGUIEvent* aGUIEvent) {
  MOZ_ASSERT(aGUIEvent);

  // If it is safe to dispatch events now, don't discard the event.
  if (nsContentUtils::IsSafeToRunScript()) {
    return false;
  }

  // If the event does not cause dispatching DOM event (i.e., internal event),
  // we can keep handling it even when it's not safe to run script.
  if (!aGUIEvent->IsAllowedToDispatchDOMEvent()) {
    return false;
  }

  // If the event is a composition event, we need to let IMEStateManager know
  // it's discarded because it needs to listen all composition events to manage
  // TextComposition instance.
  if (aGUIEvent->mClass == eCompositionEventClass) {
    IMEStateManager::OnCompositionEventDiscarded(
        aGUIEvent->AsCompositionEvent());
  }

#ifdef DEBUG
  if (aGUIEvent->IsIMERelatedEvent()) {
    nsPrintfCString warning("%s event is discarded",
                            ToChar(aGUIEvent->mMessage));
    NS_WARNING(warning.get());
  }
#endif  // #ifdef DEBUG

  nsContentUtils::WarnScriptWasIgnored(GetDocument());
  return true;
}

// static
nsIContent* PresShell::EventHandler::GetCapturingContentFor(
    WidgetGUIEvent* aGUIEvent) {
  return (aGUIEvent->mClass == ePointerEventClass ||
          aGUIEvent->mClass == eWheelEventClass ||
          aGUIEvent->HasMouseEventMessage())
             ? PresShell::GetCapturingContent()
             : nullptr;
}

bool PresShell::EventHandler::GetRetargetEventDocument(
    WidgetGUIEvent* aGUIEvent, Document** aRetargetEventDocument) {
  MOZ_ASSERT(aGUIEvent);
  MOZ_ASSERT(aRetargetEventDocument);

  *aRetargetEventDocument = nullptr;

  // key and IME related events should not cross top level window boundary.
  // Basically, such input events should be fired only on focused widget.
  // However, some IMEs might need to clean up composition after focused
  // window is deactivated.  And also some tests on MozMill want to test key
  // handling on deactivated window because MozMill window can be activated
  // during tests.  So, there is no merit the events should be redirected to
  // active window.  So, the events should be handled on the last focused
  // content in the last focused DOM window in same top level window.
  // Note, if no DOM window has been focused yet, we can discard the events.
  if (aGUIEvent->IsTargetedAtFocusedWindow()) {
    nsCOMPtr<nsPIDOMWindowOuter> window = GetFocusedDOMWindowInOurWindow();
    // No DOM window in same top level window has not been focused yet,
    // discard the events.
    if (!window) {
      return false;
    }

    RefPtr<Document> retargetEventDoc = window->GetExtantDoc();
    if (!retargetEventDoc) {
      return false;
    }
    retargetEventDoc.forget(aRetargetEventDocument);
    return true;
  }

  nsIContent* capturingContent =
      EventHandler::GetCapturingContentFor(aGUIEvent);
  if (capturingContent) {
    // if the mouse is being captured then retarget the mouse event at the
    // document that is being captured.
    RefPtr<Document> retargetEventDoc = capturingContent->GetComposedDoc();
    retargetEventDoc.forget(aRetargetEventDocument);
    return true;
  }

#ifdef ANDROID
  if (aGUIEvent->mClass == eTouchEventClass ||
      aGUIEvent->mClass == eMouseEventClass ||
      aGUIEvent->mClass == eWheelEventClass) {
    RefPtr<Document> retargetEventDoc = mPresShell->GetPrimaryContentDocument();
    retargetEventDoc.forget(aRetargetEventDocument);
    return true;
  }
#endif  // #ifdef ANDROID

  // When we don't find another document to handle the event, we need to keep
  // handling the event by ourselves.
  return true;
}

nsIFrame* PresShell::EventHandler::GetFrameForHandlingEventWith(
    WidgetGUIEvent* aGUIEvent, Document* aRetargetDocument,
    nsIFrame* aFrameForPresShell) {
  MOZ_ASSERT(aGUIEvent);
  MOZ_ASSERT(aRetargetDocument);

  RefPtr<PresShell> retargetPresShell = aRetargetDocument->GetPresShell();
  // Even if the document doesn't have PresShell, i.e., it's invisible, we
  // need to dispatch only KeyboardEvent in its nearest visible document
  // because key focus shouldn't be caught by invisible document.
  if (!retargetPresShell) {
    if (!aGUIEvent->HasKeyEventMessage()) {
      return nullptr;
    }
    Document* retargetEventDoc = aRetargetDocument;
    while (!retargetPresShell) {
      retargetEventDoc = retargetEventDoc->GetInProcessParentDocument();
      if (!retargetEventDoc) {
        return nullptr;
      }
      retargetPresShell = retargetEventDoc->GetPresShell();
    }
  }

  // If the found PresShell is this instance, caller needs to keep handling
  // aGUIEvent by itself.  Therefore, return the given frame which was set
  // to aFrame of HandleEvent().
  if (retargetPresShell == mPresShell) {
    return aFrameForPresShell;
  }

  // Use root frame of the new PresShell if there is.
  nsIFrame* rootFrame = retargetPresShell->GetRootFrame();
  if (rootFrame) {
    return rootFrame;
  }

  // Otherwise, and if aGUIEvent requires content of PresShell, caller should
  // stop handling the event.
  if (aGUIEvent->mMessage == eQueryTextContent ||
      aGUIEvent->IsContentCommandEvent()) {
    return nullptr;
  }

  // Otherwise, use nearest ancestor frame which includes the PresShell.
  return GetNearestFrameContainingPresShell(retargetPresShell);
}

bool PresShell::EventHandler::MaybeHandleEventWithAnotherPresShell(
    nsIFrame* aFrameForPresShell, WidgetGUIEvent* aGUIEvent,
    nsEventStatus* aEventStatus, nsresult* aRv) {
  MOZ_ASSERT(aGUIEvent);
  MOZ_ASSERT(aEventStatus);
  MOZ_ASSERT(aRv);

  *aRv = NS_OK;

  RefPtr<Document> retargetEventDoc;
  if (!GetRetargetEventDocument(aGUIEvent, getter_AddRefs(retargetEventDoc))) {
    // Nobody can handle this event.  So, treat as handled by somebody to make
    // caller do nothing anymore.
    return true;
  }

  // If there is no proper retarget document, the caller should handle the
  // event by itself.
  if (!retargetEventDoc) {
    return false;
  }

  nsIFrame* frame = GetFrameForHandlingEventWith(aGUIEvent, retargetEventDoc,
                                                 aFrameForPresShell);
  if (!frame) {
    // Nobody can handle this event.  So, treat as handled by somebody to make
    // caller do nothing anymore.
    return true;
  }

  // If we reached same frame as set to HandleEvent(), the caller should handle
  // the event by itself.
  if (frame == aFrameForPresShell) {
    return false;
  }

  // We need to handle aGUIEvent with another PresShell.
  RefPtr<PresShell> presShell = frame->PresContext()->PresShell();
  *aRv = presShell->HandleEvent(frame, aGUIEvent, true, aEventStatus);
  return true;
}

bool PresShell::EventHandler::MaybeDiscardOrDelayKeyboardEvent(
    WidgetGUIEvent* aGUIEvent) {
  MOZ_ASSERT(aGUIEvent);

  if (aGUIEvent->mClass != eKeyboardEventClass) {
    return false;
  }

  Document* document = GetDocument();
  if (!document || !document->EventHandlingSuppressed()) {
    return false;
  }

  if (aGUIEvent->mMessage == eKeyDown) {
    mPresShell->mNoDelayedKeyEvents = true;
  } else if (!mPresShell->mNoDelayedKeyEvents) {
    UniquePtr<DelayedKeyEvent> delayedKeyEvent =
        MakeUnique<DelayedKeyEvent>(aGUIEvent->AsKeyboardEvent());
    PushDelayedEventIntoQueue(std::move(delayedKeyEvent));
  }
  aGUIEvent->mFlags.mIsSuppressedOrDelayed = true;
  return true;
}

bool PresShell::EventHandler::MaybeDiscardOrDelayMouseEvent(
    nsIFrame* aFrameToHandleEvent, WidgetGUIEvent* aGUIEvent) {
  MOZ_ASSERT(aFrameToHandleEvent);
  MOZ_ASSERT(aGUIEvent);

  if (aGUIEvent->mClass != eMouseEventClass) {
    return false;
  }

  if (!aFrameToHandleEvent->PresContext()
           ->Document()
           ->EventHandlingSuppressed()) {
    return false;
  }

  if (aGUIEvent->mMessage == eMouseDown) {
    mPresShell->mNoDelayedMouseEvents = true;
  } else if (!mPresShell->mNoDelayedMouseEvents &&
             (aGUIEvent->mMessage == eMouseUp ||
              // contextmenu is triggered after right mouseup on Windows and
              // right mousedown on other platforms.
              aGUIEvent->mMessage == eContextMenu)) {
    UniquePtr<DelayedMouseEvent> delayedMouseEvent =
        MakeUnique<DelayedMouseEvent>(aGUIEvent->AsMouseEvent());
    PushDelayedEventIntoQueue(std::move(delayedMouseEvent));
  }

  // If there is a suppressed event listener associated with the document,
  // notify it about the suppressed mouse event. This allows devtools
  // features to continue receiving mouse events even when the devtools
  // debugger has paused execution in a page.
  RefPtr<EventListener> suppressedListener = aFrameToHandleEvent->PresContext()
                                                 ->Document()
                                                 ->GetSuppressedEventListener();
  if (!suppressedListener ||
      aGUIEvent->AsMouseEvent()->mReason == WidgetMouseEvent::eSynthesized) {
    return true;
  }

  nsCOMPtr<nsIContent> targetContent;
  aFrameToHandleEvent->GetContentForEvent(aGUIEvent,
                                          getter_AddRefs(targetContent));
  if (targetContent) {
    aGUIEvent->mTarget = targetContent;
  }

  nsCOMPtr<EventTarget> eventTarget = aGUIEvent->mTarget;
  RefPtr<Event> event = EventDispatcher::CreateEvent(
      eventTarget, aFrameToHandleEvent->PresContext(), aGUIEvent,
      EmptyString());

  suppressedListener->HandleEvent(*event);
  return true;
}

nsIFrame* PresShell::EventHandler::MaybeFlushThrottledStyles(
    nsIFrame* aFrameForPresShell) {
  if (!GetDocument()) {
    // XXX Only when mPresShell has document, we'll try to look for a frame
    //     containing mPresShell even if given frame is nullptr.  Does this
    //     make sense?
    return aFrameForPresShell;
  }

  PresShell* rootPresShell = mPresShell->GetRootPresShell();
  if (NS_WARN_IF(!rootPresShell)) {
    return nullptr;
  }
  Document* rootDocument = rootPresShell->GetDocument();
  if (NS_WARN_IF(!rootDocument)) {
    return nullptr;
  }

  AutoWeakFrame weakFrameForPresShell(aFrameForPresShell);
  {  // scope for scriptBlocker.
    nsAutoScriptBlocker scriptBlocker;
    FlushThrottledStyles(*rootDocument);
  }

  if (weakFrameForPresShell.IsAlive()) {
    return aFrameForPresShell;
  }

  return GetNearestFrameContainingPresShell(mPresShell);
}

nsIFrame* PresShell::EventHandler::ComputeRootFrameToHandleEvent(
    nsIFrame* aFrameForPresShell, WidgetGUIEvent* aGUIEvent,
    nsIContent* aCapturingContent, bool* aIsCapturingContentIgnored,
    bool* aIsCaptureRetargeted) {
  MOZ_ASSERT(aFrameForPresShell);
  MOZ_ASSERT(aGUIEvent);
  MOZ_ASSERT(aIsCapturingContentIgnored);
  MOZ_ASSERT(aIsCaptureRetargeted);

  nsIFrame* rootFrameToHandleEvent = ComputeRootFrameToHandleEventWithPopup(
      aFrameForPresShell, aGUIEvent, aCapturingContent,
      aIsCapturingContentIgnored);
  if (*aIsCapturingContentIgnored) {
    // If the capturing content is ignored, we don't need to respect it.
    return rootFrameToHandleEvent;
  }

  if (!aCapturingContent) {
    return rootFrameToHandleEvent;
  }

  // If we have capturing content, let's compute root frame with it again.
  return ComputeRootFrameToHandleEventWithCapturingContent(
      rootFrameToHandleEvent, aCapturingContent, aIsCapturingContentIgnored,
      aIsCaptureRetargeted);
}

nsIFrame* PresShell::EventHandler::ComputeRootFrameToHandleEventWithPopup(
    nsIFrame* aRootFrameToHandleEvent, WidgetGUIEvent* aGUIEvent,
    nsIContent* aCapturingContent, bool* aIsCapturingContentIgnored) {
  MOZ_ASSERT(aRootFrameToHandleEvent);
  MOZ_ASSERT(aGUIEvent);
  MOZ_ASSERT(aIsCapturingContentIgnored);

  *aIsCapturingContentIgnored = false;

  nsPresContext* framePresContext = aRootFrameToHandleEvent->PresContext();
  nsPresContext* rootPresContext = framePresContext->GetRootPresContext();
  NS_ASSERTION(rootPresContext == GetPresContext()->GetRootPresContext(),
               "How did we end up outside the connected "
               "prescontext/viewmanager hierarchy?");
  nsIFrame* popupFrame = nsLayoutUtils::GetPopupFrameForEventCoordinates(
      rootPresContext, aGUIEvent);
  if (!popupFrame) {
    return aRootFrameToHandleEvent;
  }

  // If a remote browser is currently capturing input break out if we
  // detect a chrome generated popup.
  if (aCapturingContent &&
      EventStateManager::IsRemoteTarget(aCapturingContent)) {
    *aIsCapturingContentIgnored = true;
  }

  // If the popupFrame is an ancestor of the 'frame', the frame should
  // handle the event, otherwise, the popup should handle it.
  if (nsContentUtils::ContentIsCrossDocDescendantOf(
          framePresContext->GetPresShell()->GetDocument(),
          popupFrame->GetContent())) {
    return aRootFrameToHandleEvent;
  }

  // If we aren't starting our event dispatch from the root frame of the
  // root prescontext, then someone must be capturing the mouse. In that
  // case we only want to use the popup list if the capture is
  // inside the popup.
  if (framePresContext == rootPresContext &&
      aRootFrameToHandleEvent == FrameConstructor()->GetRootFrame()) {
    return popupFrame;
  }

  if (aCapturingContent && !*aIsCapturingContentIgnored &&
      aCapturingContent->IsInclusiveDescendantOf(popupFrame->GetContent())) {
    return popupFrame;
  }

  return aRootFrameToHandleEvent;
}

nsIFrame*
PresShell::EventHandler::ComputeRootFrameToHandleEventWithCapturingContent(
    nsIFrame* aRootFrameToHandleEvent, nsIContent* aCapturingContent,
    bool* aIsCapturingContentIgnored, bool* aIsCaptureRetargeted) {
  MOZ_ASSERT(aRootFrameToHandleEvent);
  MOZ_ASSERT(aCapturingContent);
  MOZ_ASSERT(aIsCapturingContentIgnored);
  MOZ_ASSERT(aIsCaptureRetargeted);

  *aIsCapturingContentIgnored = false;
  *aIsCaptureRetargeted = false;

  // If a capture is active, determine if the docshell is visible. If not,
  // clear the capture and target the mouse event normally instead. This
  // would occur if the mouse button is held down while a tab change occurs.
  // If the docshell is visible, look for a scrolling container.
  nsCOMPtr<nsIBaseWindow> baseWindow =
      do_QueryInterface(GetPresContext()->GetContainerWeak());
  if (!baseWindow) {
    ClearMouseCapture(nullptr);
    *aIsCapturingContentIgnored = true;
    return aRootFrameToHandleEvent;
  }

  bool isBaseWindowVisible = false;
  nsresult rv = baseWindow->GetVisibility(&isBaseWindowVisible);
  if (NS_FAILED(rv) || !isBaseWindowVisible) {
    ClearMouseCapture(nullptr);
    *aIsCapturingContentIgnored = true;
    return aRootFrameToHandleEvent;
  }

  if (PresShell::sCapturingContentInfo.mRetargetToElement) {
    *aIsCaptureRetargeted = true;
    return aRootFrameToHandleEvent;
  }

  // A check was already done above to ensure that aCapturingContent is
  // in this presshell.
  NS_ASSERTION(aCapturingContent->OwnerDoc() == GetDocument(),
               "Unexpected document");
  nsIFrame* captureFrame = aCapturingContent->GetPrimaryFrame();
  if (!captureFrame) {
    return aRootFrameToHandleEvent;
  }

  if (aCapturingContent->IsHTMLElement(nsGkAtoms::select)) {
    // a dropdown <select> has a child in its selectPopupList and we should
    // capture on that instead.
    nsIFrame* childFrame =
        captureFrame->GetChildList(nsIFrame::kSelectPopupList).FirstChild();
    if (childFrame) {
      captureFrame = childFrame;
    }
  }

  // scrollable frames should use the scrolling container as the root instead
  // of the document
  nsIScrollableFrame* scrollFrame = do_QueryFrame(captureFrame);
  return scrollFrame ? scrollFrame->GetScrolledFrame()
                     : aRootFrameToHandleEvent;
}

nsresult
PresShell::EventHandler::HandleEventWithPointerCapturingContentWithoutItsFrame(
    nsIFrame* aFrameForPresShell, WidgetGUIEvent* aGUIEvent,
    nsIContent* aPointerCapturingContent, nsEventStatus* aEventStatus) {
  MOZ_ASSERT(aGUIEvent);
  MOZ_ASSERT(aPointerCapturingContent);
  MOZ_ASSERT(!aPointerCapturingContent->GetPrimaryFrame(),
             "Handle the event with frame rather than only with the content");
  MOZ_ASSERT(aEventStatus);

  RefPtr<PresShell> presShellForCapturingContent =
      PresShell::GetShellForEventTarget(nullptr, aPointerCapturingContent);
  if (!presShellForCapturingContent) {
    // If we can't process event for the capturing content, release
    // the capture.
    PointerEventHandler::ReleaseIfCaptureByDescendant(aPointerCapturingContent);
    return NS_OK;
  }

  nsCOMPtr<nsIContent> overrideClickTarget =
      GetOverrideClickTarget(aGUIEvent, aFrameForPresShell);

  // Dispatch events to the capturing content even it's frame is
  // destroyed.
  PointerEventHandler::DispatchPointerFromMouseOrTouch(
      presShellForCapturingContent, nullptr, aPointerCapturingContent,
      aGUIEvent, false, aEventStatus, nullptr);

  if (presShellForCapturingContent == mPresShell) {
    return HandleEventWithTarget(aGUIEvent, nullptr, aPointerCapturingContent,
                                 aEventStatus, true, nullptr,
                                 overrideClickTarget);
  }

  EventHandler eventHandlerForCapturingContent(
      std::move(presShellForCapturingContent));
  return eventHandlerForCapturingContent.HandleEventWithTarget(
      aGUIEvent, nullptr, aPointerCapturingContent, aEventStatus, true, nullptr,
      overrideClickTarget);
}

nsresult PresShell::EventHandler::HandleEventAtFocusedContent(
    WidgetGUIEvent* aGUIEvent, nsEventStatus* aEventStatus) {
  MOZ_ASSERT(aGUIEvent);
  MOZ_ASSERT(aGUIEvent->IsTargetedAtFocusedContent());
  MOZ_ASSERT(aEventStatus);

  AutoCurrentEventInfoSetter eventInfoSetter(*this);

  RefPtr<Element> eventTargetElement =
      ComputeFocusedEventTargetElement(aGUIEvent);

  mPresShell->mCurrentEventFrame = nullptr;
  if (eventTargetElement) {
    nsresult rv = NS_OK;
    if (MaybeHandleEventWithAnotherPresShell(eventTargetElement, aGUIEvent,
                                             aEventStatus, &rv)) {
      return rv;
    }
  }

  // If we cannot handle the event with mPresShell, let's try to handle it
  // with parent PresShell.
  mPresShell->mCurrentEventContent = eventTargetElement;
  if (!mPresShell->GetCurrentEventContent() ||
      !mPresShell->GetCurrentEventFrame() ||
      InZombieDocument(mPresShell->mCurrentEventContent)) {
    return RetargetEventToParent(aGUIEvent, aEventStatus);
  }

  nsresult rv =
      HandleEventWithCurrentEventInfo(aGUIEvent, aEventStatus, true, nullptr);

#ifdef DEBUG
  mPresShell->ShowEventTargetDebug();
#endif

  return rv;
}

Element* PresShell::EventHandler::ComputeFocusedEventTargetElement(
    WidgetGUIEvent* aGUIEvent) {
  MOZ_ASSERT(aGUIEvent);
  MOZ_ASSERT(aGUIEvent->IsTargetedAtFocusedContent());

  // key and IME related events go to the focused frame in this DOM window.
  nsPIDOMWindowOuter* window = GetDocument()->GetWindow();
  nsCOMPtr<nsPIDOMWindowOuter> focusedWindow;
  Element* eventTargetElement = nsFocusManager::GetFocusedDescendant(
      window, nsFocusManager::eOnlyCurrentWindow,
      getter_AddRefs(focusedWindow));

  // otherwise, if there is no focused content or the focused content has
  // no frame, just use the root content. This ensures that key events
  // still get sent to the window properly if nothing is focused or if a
  // frame goes away while it is focused.
  if (!eventTargetElement || !eventTargetElement->GetPrimaryFrame()) {
    eventTargetElement = GetDocument()->GetUnfocusedKeyEventTarget();
  }

  switch (aGUIEvent->mMessage) {
    case eKeyDown:
      sLastKeyDownEventTargetElement = eventTargetElement;
      return eventTargetElement;
    case eKeyPress:
    case eKeyUp:
      if (!sLastKeyDownEventTargetElement) {
        return eventTargetElement;
      }
      // If a different element is now focused for the keypress/keyup event
      // than what was focused during the keydown event, check if the new
      // focused element is not in a chrome document any more, and if so,
      // retarget the event back at the keydown target. This prevents a
      // content area from grabbing the focus from chrome in-between key
      // events.
      if (eventTargetElement) {
        bool keyDownIsChrome = nsContentUtils::IsChromeDoc(
            sLastKeyDownEventTargetElement->GetComposedDoc());
        if (keyDownIsChrome != nsContentUtils::IsChromeDoc(
                                   eventTargetElement->GetComposedDoc()) ||
            (keyDownIsChrome && BrowserParent::GetFrom(eventTargetElement))) {
          eventTargetElement = sLastKeyDownEventTargetElement;
        }
      }

      if (aGUIEvent->mMessage == eKeyUp) {
        sLastKeyDownEventTargetElement = nullptr;
      }
      [[fallthrough]];
    default:
      return eventTargetElement;
  }
}

bool PresShell::EventHandler::MaybeHandleEventWithAnotherPresShell(
    Element* aEventTargetElement, WidgetGUIEvent* aGUIEvent,
    nsEventStatus* aEventStatus, nsresult* aRv) {
  MOZ_ASSERT(aEventTargetElement);
  MOZ_ASSERT(aGUIEvent);
  MOZ_ASSERT(!aGUIEvent->IsUsingCoordinates());
  MOZ_ASSERT(aEventStatus);
  MOZ_ASSERT(aRv);

  Document* eventTargetDocument = aEventTargetElement->OwnerDoc();
  if (!eventTargetDocument || eventTargetDocument == GetDocument()) {
    *aRv = NS_OK;
    return false;
  }

  RefPtr<PresShell> eventTargetPresShell = eventTargetDocument->GetPresShell();
  if (!eventTargetPresShell) {
    *aRv = NS_OK;
    return true;  // No PresShell can handle the event.
  }

  EventHandler eventHandler(std::move(eventTargetPresShell));
  *aRv = eventHandler.HandleRetargetedEvent(aGUIEvent, aEventStatus,
                                            aEventTargetElement);
  return true;
}

nsresult PresShell::EventHandler::HandleEventWithFrameForPresShell(
    nsIFrame* aFrameForPresShell, WidgetGUIEvent* aGUIEvent,
    nsEventStatus* aEventStatus) {
  MOZ_ASSERT(aGUIEvent);
  MOZ_ASSERT(!aGUIEvent->IsUsingCoordinates());
  MOZ_ASSERT(!aGUIEvent->IsTargetedAtFocusedContent());
  MOZ_ASSERT(aEventStatus);

  AutoCurrentEventInfoSetter eventInfoSetter(*this, aFrameForPresShell,
                                             nullptr);

  nsresult rv = NS_OK;
  if (mPresShell->GetCurrentEventFrame()) {
    rv =
        HandleEventWithCurrentEventInfo(aGUIEvent, aEventStatus, true, nullptr);
  }

#ifdef DEBUG
  mPresShell->ShowEventTargetDebug();
#endif

  return rv;
}

Document* PresShell::GetPrimaryContentDocument() {
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

#ifdef DEBUG
void PresShell::ShowEventTargetDebug() {
  if (nsFrame::GetShowEventTargetFrameBorder() && GetCurrentEventFrame()) {
    if (mDrawEventTargetFrame) {
      mDrawEventTargetFrame->InvalidateFrame();
    }

    mDrawEventTargetFrame = mCurrentEventFrame;
    mDrawEventTargetFrame->InvalidateFrame();
  }
}
#endif

nsresult PresShell::EventHandler::HandleEventWithTarget(
    WidgetEvent* aEvent, nsIFrame* aNewEventFrame, nsIContent* aNewEventContent,
    nsEventStatus* aEventStatus, bool aIsHandlingNativeEvent,
    nsIContent** aTargetContent, nsIContent* aOverrideClickTarget) {
  MOZ_ASSERT(aEvent);
  MOZ_DIAGNOSTIC_ASSERT(aEvent->IsTrusted());

#if DEBUG
  MOZ_ASSERT(!aNewEventFrame ||
                 aNewEventFrame->PresContext()->GetPresShell() == mPresShell,
             "wrong shell");
  if (aNewEventContent) {
    Document* doc = aNewEventContent->GetComposedDoc();
    NS_ASSERTION(doc, "event for content that isn't in a document");
    // NOTE: We don't require that the document still have a PresShell.
    // See bug 1375940.
  }
#endif
  NS_ENSURE_STATE(!aNewEventContent ||
                  aNewEventContent->GetComposedDoc() == GetDocument());
  AutoPointerEventTargetUpdater updater(mPresShell, aEvent, aNewEventFrame,
                                        aTargetContent);
  AutoCurrentEventInfoSetter eventInfoSetter(*this, aNewEventFrame,
                                             aNewEventContent);
  nsresult rv = HandleEventWithCurrentEventInfo(aEvent, aEventStatus, false,
                                                aOverrideClickTarget);
  return rv;
}

namespace {

class MOZ_RAII AutoEventHandler final {
 public:
  AutoEventHandler(WidgetEvent* aEvent, Document* aDocument) : mEvent(aEvent) {
    MOZ_ASSERT(mEvent);
    MOZ_ASSERT(mEvent->IsTrusted());

    if (mEvent->mMessage == eMouseDown) {
      PresShell::ReleaseCapturingContent();
      PresShell::AllowMouseCapture(true);
    }
    if (NeedsToUpdateCurrentMouseBtnState()) {
      WidgetMouseEvent* mouseEvent = mEvent->AsMouseEvent();
      if (mouseEvent) {
        EventStateManager::sCurrentMouseBtn = mouseEvent->mButton;
      }
    }
  }

  ~AutoEventHandler() {
    if (mEvent->mMessage == eMouseDown) {
      PresShell::AllowMouseCapture(false);
    }
    if (NeedsToUpdateCurrentMouseBtnState()) {
      EventStateManager::sCurrentMouseBtn = MouseButton::eNotPressed;
    }
  }

 protected:
  bool NeedsToUpdateCurrentMouseBtnState() const {
    return mEvent->mMessage == eMouseDown || mEvent->mMessage == eMouseUp ||
           mEvent->mMessage == ePointerDown || mEvent->mMessage == ePointerUp;
  }

  WidgetEvent* mEvent;
};

}  // anonymous namespace

nsresult PresShell::EventHandler::HandleEventWithCurrentEventInfo(
    WidgetEvent* aEvent, nsEventStatus* aEventStatus,
    bool aIsHandlingNativeEvent, nsIContent* aOverrideClickTarget) {
  MOZ_ASSERT(aEvent);
  MOZ_ASSERT(aEventStatus);

  RefPtr<EventStateManager> manager = GetPresContext()->EventStateManager();

  // If we cannot handle the event with mPresShell because of no target,
  // just record the response time.
  // XXX Is this intentional?  In such case, the score is really good because
  //     of nothing to do.  So, it may make average and median better.
  if (NS_EVENT_NEEDS_FRAME(aEvent) && !mPresShell->GetCurrentEventFrame() &&
      !mPresShell->GetCurrentEventContent()) {
    RecordEventHandlingResponsePerformance(aEvent);
    return NS_OK;
  }

  if (mPresShell->mCurrentEventContent && aEvent->IsTargetedAtFocusedWindow()) {
    nsFocusManager* fm = nsFocusManager::GetFocusManager();
    if (fm) {
      // This may run script now.  So, mPresShell might be destroyed after here.
      fm->FlushBeforeEventHandlingIfNeeded(mPresShell->mCurrentEventContent);
    }
  }

  bool touchIsNew = false;
  if (!PrepareToDispatchEvent(aEvent, aEventStatus, &touchIsNew)) {
    return NS_OK;
  }

  // We finished preparing to dispatch the event.  So, let's record the
  // performance.
  RecordEventPreparationPerformance(aEvent);

  AutoHandlingUserInputStatePusher userInpStatePusher(
      UserActivation::IsUserInteractionEvent(aEvent), aEvent);
  AutoEventHandler eventHandler(aEvent, GetDocument());
  AutoPopupStatePusher popupStatePusher(
      PopupBlocker::GetEventPopupControlState(aEvent));

  // FIXME. If the event was reused, we need to clear the old target,
  // bug 329430
  aEvent->mTarget = nullptr;

  HandlingTimeAccumulator handlingTimeAccumulator(*this, aEvent);

  nsresult rv = DispatchEvent(manager, aEvent, touchIsNew, aEventStatus,
                              aOverrideClickTarget);

  if (!mPresShell->IsDestroying() && aIsHandlingNativeEvent) {
    // Ensure that notifications to IME should be sent before getting next
    // native event from the event queue.
    // XXX Should we check the event message or event class instead of
    //     using aIsHandlingNativeEvent?
    manager->TryToFlushPendingNotificationsToIME();
  }

  FinalizeHandlingEvent(aEvent);

  RecordEventHandlingResponsePerformance(aEvent);

  return rv;  // Result of DispatchEvent()
}

nsresult PresShell::EventHandler::DispatchEvent(
    EventStateManager* aEventStateManager, WidgetEvent* aEvent,
    bool aTouchIsNew, nsEventStatus* aEventStatus,
    nsIContent* aOverrideClickTarget) {
  MOZ_ASSERT(aEventStateManager);
  MOZ_ASSERT(aEvent);
  MOZ_ASSERT(aEventStatus);

  // 1. Give event to event manager for pre event state changes and
  //    generation of synthetic events.
  {  // Scope for presContext
    RefPtr<nsPresContext> presContext = GetPresContext();
    nsCOMPtr<nsIContent> eventContent = mPresShell->mCurrentEventContent;
    nsresult rv = aEventStateManager->PreHandleEvent(
        presContext, aEvent, mPresShell->mCurrentEventFrame, eventContent,
        aEventStatus, aOverrideClickTarget);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // 2. Give event to the DOM for third party and JS use.
  bool wasHandlingKeyBoardEvent = nsContentUtils::IsHandlingKeyBoardEvent();
  if (aEvent->mClass == eKeyboardEventClass) {
    nsContentUtils::SetIsHandlingKeyBoardEvent(true);
  }
  // If EventStateManager or something wants reply from remote process and
  // needs to win any other event listeners in chrome, the event is both
  // stopped its propagation and marked as "waiting reply from remote
  // process".  In this case, PresShell shouldn't dispatch the event into
  // the DOM tree because they don't have a chance to stop propagation in
  // the system event group.  On the other hand, if its propagation is not
  // stopped, that means that the event may be reserved by chrome.  If it's
  // reserved by chrome, the event shouldn't be sent to any remote
  // processes.  In this case, PresShell needs to dispatch the event to
  // the DOM tree for checking if it's reserved.
  if (aEvent->IsAllowedToDispatchDOMEvent() &&
      !(aEvent->PropagationStopped() &&
        aEvent->IsWaitingReplyFromRemoteProcess())) {
    MOZ_ASSERT(nsContentUtils::IsSafeToRunScript(),
               "Somebody changed aEvent to cause a DOM event!");
    nsPresShellEventCB eventCB(mPresShell);
    if (nsIFrame* target = mPresShell->GetCurrentEventFrame()) {
      if (target->OnlySystemGroupDispatch(aEvent->mMessage)) {
        aEvent->StopPropagation();
      }
    }
    if (aEvent->mClass == eTouchEventClass) {
      DispatchTouchEventToDOM(aEvent, aEventStatus, &eventCB, aTouchIsNew);
    } else {
      DispatchEventToDOM(aEvent, aEventStatus, &eventCB);
    }
  }

  nsContentUtils::SetIsHandlingKeyBoardEvent(wasHandlingKeyBoardEvent);

  if (aEvent->mMessage == ePointerUp || aEvent->mMessage == ePointerCancel) {
    // Implicitly releasing capture for given pointer.
    // ePointerLostCapture should be send after ePointerUp or
    // ePointerCancel.
    WidgetPointerEvent* pointerEvent = aEvent->AsPointerEvent();
    MOZ_ASSERT(pointerEvent);
    PointerEventHandler::ReleasePointerCaptureById(pointerEvent->pointerId);
    PointerEventHandler::CheckPointerCaptureState(pointerEvent);
  }

  if (mPresShell->IsDestroying()) {
    return NS_OK;
  }

  // 3. Give event to event manager for post event state changes and
  //    generation of synthetic events.
  // Refetch the prescontext, in case it changed.
  RefPtr<nsPresContext> presContext = GetPresContext();
  return aEventStateManager->PostHandleEvent(
      presContext, aEvent, mPresShell->GetCurrentEventFrame(), aEventStatus,
      aOverrideClickTarget);
}

bool PresShell::EventHandler::PrepareToDispatchEvent(
    WidgetEvent* aEvent, nsEventStatus* aEventStatus, bool* aTouchIsNew) {
  MOZ_ASSERT(aEvent->IsTrusted());
  MOZ_ASSERT(aEventStatus);
  MOZ_ASSERT(aTouchIsNew);

  *aTouchIsNew = false;
  if (aEvent->IsUserAction()) {
    mPresShell->mHasHandledUserInput = true;
  }

  switch (aEvent->mMessage) {
    case eKeyPress:
    case eKeyDown:
    case eKeyUp: {
      WidgetKeyboardEvent* keyboardEvent = aEvent->AsKeyboardEvent();
      MaybeHandleKeyboardEventBeforeDispatch(keyboardEvent);
      return true;
    }
    case eMouseMove: {
      bool allowCapture = EventStateManager::GetActiveEventStateManager() &&
                          GetPresContext() &&
                          GetPresContext()->EventStateManager() ==
                              EventStateManager::GetActiveEventStateManager();
      PresShell::AllowMouseCapture(allowCapture);
      return true;
    }
    case eDrop: {
      nsCOMPtr<nsIDragSession> session = nsContentUtils::GetDragSession();
      if (session) {
        bool onlyChromeDrop = false;
        session->GetOnlyChromeDrop(&onlyChromeDrop);
        if (onlyChromeDrop) {
          aEvent->mFlags.mOnlyChromeDispatch = true;
        }
      }
      return true;
    }
    case eContextMenu: {
      // If we cannot open context menu even though eContextMenu is fired, we
      // should stop dispatching it into the DOM.
      WidgetMouseEvent* mouseEvent = aEvent->AsMouseEvent();
      if (mouseEvent->IsContextMenuKeyEvent() &&
          !AdjustContextMenuKeyEvent(mouseEvent)) {
        return false;
      }

      // If "Shift" state is active, context menu should be forcibly opened even
      // if web apps want to prevent it since we respect our users' intention.
      // In this case, we don't fire "contextmenu" event on web content because
      // of not cancelable.
      if (mouseEvent->IsShift()) {
        aEvent->mFlags.mOnlyChromeDispatch = true;
        aEvent->mFlags.mRetargetToNonNativeAnonymous = true;
      }
      return true;
    }
    case eTouchStart:
    case eTouchMove:
    case eTouchEnd:
    case eTouchCancel:
    case eTouchPointerCancel:
      return mPresShell->mTouchManager.PreHandleEvent(
          aEvent, aEventStatus, *aTouchIsNew, mPresShell->mCurrentEventContent);
    default:
      return true;
  }
}

void PresShell::EventHandler::FinalizeHandlingEvent(WidgetEvent* aEvent) {
  switch (aEvent->mMessage) {
    case eKeyPress:
    case eKeyDown:
    case eKeyUp: {
      if (aEvent->AsKeyboardEvent()->mKeyCode == NS_VK_ESCAPE) {
        if (aEvent->mMessage == eKeyUp) {
          // Reset this flag after key up is handled.
          mPresShell->mIsLastChromeOnlyEscapeKeyConsumed = false;
        } else {
          if (aEvent->mFlags.mOnlyChromeDispatch &&
              aEvent->mFlags.mDefaultPreventedByChrome) {
            mPresShell->mIsLastChromeOnlyEscapeKeyConsumed = true;
          }
        }
      }
      if (aEvent->mMessage == eKeyDown) {
        mPresShell->mIsLastKeyDownCanceled = aEvent->mFlags.mDefaultPrevented;
      }
      return;
    }
    case eMouseUp:
      // reset the capturing content now that the mouse button is up
      PresShell::ReleaseCapturingContent();
      return;
    case eMouseMove:
      PresShell::AllowMouseCapture(false);
      return;
    case eDrag:
    case eDragEnd:
    case eDragEnter:
    case eDragExit:
    case eDragLeave:
    case eDragOver:
    case eDrop: {
      // After any drag event other than dragstart (which is handled
      // separately, as we need to collect the data first), the DataTransfer
      // needs to be made protected, and then disconnected.
      DataTransfer* dataTransfer = aEvent->AsDragEvent()->mDataTransfer;
      if (dataTransfer) {
        dataTransfer->Disconnect();
      }
      return;
    }
    default:
      return;
  }
}

void PresShell::EventHandler::MaybeHandleKeyboardEventBeforeDispatch(
    WidgetKeyboardEvent* aKeyboardEvent) {
  MOZ_ASSERT(aKeyboardEvent);

  if (aKeyboardEvent->mKeyCode != NS_VK_ESCAPE) {
    return;
  }

  // If we're in fullscreen mode, exit from it forcibly when Escape key is
  // pressed.
  Document* doc = mPresShell->GetCurrentEventContent()
                      ? mPresShell->mCurrentEventContent->OwnerDoc()
                      : nullptr;
  Document* root = nsContentUtils::GetRootDocument(doc);
  if (root && root->GetFullscreenElement()) {
    // Prevent default action on ESC key press when exiting
    // DOM fullscreen mode. This prevents the browser ESC key
    // handler from stopping all loads in the document, which
    // would cause <video> loads to stop.
    // XXX We need to claim the Escape key event which will be
    //     dispatched only into chrome is already consumed by
    //     content because we need to prevent its default here
    //     for some reasons (not sure) but we need to detect
    //     if a chrome event handler will call PreventDefault()
    //     again and check it later.
    aKeyboardEvent->PreventDefaultBeforeDispatch(CrossProcessForwarding::eStop);
    aKeyboardEvent->mFlags.mOnlyChromeDispatch = true;

    // The event listeners in chrome can prevent this ESC behavior by
    // calling prevent default on the preceding keydown/press events.
    if (!mPresShell->mIsLastChromeOnlyEscapeKeyConsumed &&
        aKeyboardEvent->mMessage == eKeyUp) {
      // ESC key released while in DOM fullscreen mode.
      // Fully exit all browser windows and documents from
      // fullscreen mode.
      Document::AsyncExitFullscreen(nullptr);
    }
  }

  nsCOMPtr<Document> pointerLockedDoc =
      do_QueryReferent(EventStateManager::sPointerLockedDoc);
  if (!mPresShell->mIsLastChromeOnlyEscapeKeyConsumed && pointerLockedDoc) {
    // XXX See above comment to understand the reason why this needs
    //     to claim that the Escape key event is consumed by content
    //     even though it will be dispatched only into chrome.
    aKeyboardEvent->PreventDefaultBeforeDispatch(CrossProcessForwarding::eStop);
    aKeyboardEvent->mFlags.mOnlyChromeDispatch = true;
    if (aKeyboardEvent->mMessage == eKeyUp) {
      Document::UnlockPointer();
    }
  }
}

void PresShell::EventHandler::RecordEventPreparationPerformance(
    const WidgetEvent* aEvent) {
  MOZ_ASSERT(aEvent);

  switch (aEvent->mMessage) {
    case eKeyPress:
    case eKeyDown:
    case eKeyUp:
      if (aEvent->AsKeyboardEvent()->ShouldInteractionTimeRecorded()) {
        GetPresContext()->RecordInteractionTime(
            nsPresContext::InteractionType::KeyInteraction, aEvent->mTimeStamp);
      }
      Telemetry::AccumulateTimeDelta(Telemetry::INPUT_EVENT_QUEUED_KEYBOARD_MS,
                                     aEvent->mTimeStamp);
      return;

    case eMouseDown:
    case eMouseUp:
      Telemetry::AccumulateTimeDelta(Telemetry::INPUT_EVENT_QUEUED_CLICK_MS,
                                     aEvent->mTimeStamp);
      [[fallthrough]];
    case ePointerDown:
    case ePointerUp:
      GetPresContext()->RecordInteractionTime(
          nsPresContext::InteractionType::ClickInteraction, aEvent->mTimeStamp);
      return;

    case eMouseMove:
      if (aEvent->mFlags.mHandledByAPZ) {
        Telemetry::AccumulateTimeDelta(
            Telemetry::INPUT_EVENT_QUEUED_APZ_MOUSE_MOVE_MS,
            aEvent->mTimeStamp);
      }
      GetPresContext()->RecordInteractionTime(
          nsPresContext::InteractionType::MouseMoveInteraction,
          aEvent->mTimeStamp);
      return;

    case eWheel:
      if (aEvent->mFlags.mHandledByAPZ) {
        Telemetry::AccumulateTimeDelta(
            Telemetry::INPUT_EVENT_QUEUED_APZ_WHEEL_MS, aEvent->mTimeStamp);
      }
      return;

    case eTouchMove:
      if (aEvent->mFlags.mHandledByAPZ) {
        Telemetry::AccumulateTimeDelta(
            Telemetry::INPUT_EVENT_QUEUED_APZ_TOUCH_MOVE_MS,
            aEvent->mTimeStamp);
      }
      return;

    default:
      return;
  }
}

void PresShell::EventHandler::RecordEventHandlingResponsePerformance(
    const WidgetEvent* aEvent) {
  if (!Telemetry::CanRecordBase() || aEvent->mTimeStamp.IsNull() ||
      aEvent->mTimeStamp <= mPresShell->mLastOSWake ||
      !aEvent->AsInputEvent()) {
    return;
  }

  TimeStamp now = TimeStamp::Now();
  double millis = (now - aEvent->mTimeStamp).ToMilliseconds();
  Telemetry::Accumulate(Telemetry::INPUT_EVENT_RESPONSE_MS, millis);
  if (GetDocument() &&
      GetDocument()->GetReadyStateEnum() != Document::READYSTATE_COMPLETE) {
    Telemetry::Accumulate(Telemetry::LOAD_INPUT_EVENT_RESPONSE_MS, millis);
  }

  if (!sLastInputProcessed || sLastInputProcessed < aEvent->mTimeStamp) {
    if (sLastInputProcessed) {
      // This input event was created after we handled the last one.
      // Accumulate the previous events' coalesced duration.
      double lastMillis =
          (sLastInputProcessed - sLastInputCreated).ToMilliseconds();
      Telemetry::Accumulate(Telemetry::INPUT_EVENT_RESPONSE_COALESCED_MS,
                            lastMillis);

      if (MOZ_UNLIKELY(!PresShell::sProcessInteractable)) {
        // For content process, we use the ready state of
        // top-level-content-document to know if the process has finished the
        // start-up.
        // For parent process, see the topic
        // 'sessionstore-one-or-no-tab-restored' in PresShell::Observe.
        if (XRE_IsContentProcess() && GetDocument() &&
            GetDocument()->IsTopLevelContentDocument()) {
          switch (GetDocument()->GetReadyStateEnum()) {
            case Document::READYSTATE_INTERACTIVE:
            case Document::READYSTATE_COMPLETE:
              PresShell::sProcessInteractable = true;
              break;
            default:
              break;
          }
        }
      }
      if (MOZ_LIKELY(PresShell::sProcessInteractable)) {
        Telemetry::Accumulate(Telemetry::INPUT_EVENT_RESPONSE_POST_STARTUP_MS,
                              lastMillis);
      } else {
        Telemetry::Accumulate(Telemetry::INPUT_EVENT_RESPONSE_STARTUP_MS,
                              lastMillis);
      }
    }
    sLastInputCreated = aEvent->mTimeStamp;
  } else if (aEvent->mTimeStamp < sLastInputCreated) {
    // This event was created before the last input. May be processing out
    // of order, so coalesce backwards, too.
    sLastInputCreated = aEvent->mTimeStamp;
  }
  sLastInputProcessed = now;
}

// static
nsIPrincipal*
PresShell::EventHandler::GetDocumentPrincipalToCompareWithBlacklist(
    PresShell& aPresShell) {
  nsPresContext* presContext = aPresShell.GetPresContext();
  if (NS_WARN_IF(!presContext)) {
    return nullptr;
  }
  // If the document is sandboxed document or data: document, we should
  // get URI of the parent document.
  for (Document* document = presContext->Document();
       document && document->IsContentDocument();
       document = document->GetInProcessParentDocument()) {
    // The document URI may be about:blank even if it comes from actual web
    // site.  Therefore, we need to check the URI of its principal.
    nsIPrincipal* principal = document->NodePrincipal();
    if (principal->GetIsNullPrincipal()) {
      continue;
    }
    return principal;
  }
  return nullptr;
}

nsresult PresShell::EventHandler::DispatchEventToDOM(
    WidgetEvent* aEvent, nsEventStatus* aEventStatus,
    nsPresShellEventCB* aEventCB) {
  nsresult rv = NS_OK;
  nsCOMPtr<nsINode> eventTarget = mPresShell->mCurrentEventContent;
  nsPresShellEventCB* eventCBPtr = aEventCB;
  if (!eventTarget) {
    nsCOMPtr<nsIContent> targetContent;
    if (mPresShell->mCurrentEventFrame) {
      rv = mPresShell->mCurrentEventFrame->GetContentForEvent(
          aEvent, getter_AddRefs(targetContent));
    }
    if (NS_SUCCEEDED(rv) && targetContent) {
      eventTarget = targetContent;
    } else if (GetDocument()) {
      eventTarget = GetDocument();
      // If we don't have any content, the callback wouldn't probably
      // do nothing.
      eventCBPtr = nullptr;
    }
  }
  if (eventTarget) {
    if (aEvent->IsBlockedForFingerprintingResistance()) {
      aEvent->mFlags.mOnlySystemGroupDispatchInContent = true;
    } else if (aEvent->mMessage == eKeyPress) {
      // If eKeyPress event is marked as not dispatched in the default event
      // group in web content, it's caused by non-printable key or key
      // combination.  In this case, UI Events declares that browsers
      // shouldn't dispatch keypress event.  However, some web apps may be
      // broken with this strict behavior due to historical issue.
      // Therefore, we need to keep dispatching keypress event for such keys
      // even with breaking the standard.
      // Similarly, the other browsers sets non-zero value of keyCode or
      // charCode of keypress event to the other.  Therefore, we should
      // behave so, however, some web apps may be broken.  On such web apps,
      // we should keep using legacy our behavior.
      if (!mPresShell->mInitializedWithKeyPressEventDispatchingBlacklist) {
        bool isInPrefList = false;
        mPresShell->mInitializedWithKeyPressEventDispatchingBlacklist = true;
        nsCOMPtr<nsIPrincipal> principal =
            GetDocumentPrincipalToCompareWithBlacklist(*mPresShell);
        if (principal) {
          principal->IsURIInPrefList(
              "dom.keyboardevent.keypress.hack.dispatch_non_printable_keys",
              &isInPrefList);
          mPresShell->mForceDispatchKeyPressEventsForNonPrintableKeys =
              isInPrefList;

          principal->IsURIInPrefList(
              "dom.keyboardevent.keypress.hack."
              "dispatch_non_printable_keys.addl",
              &isInPrefList);
          mPresShell->mForceDispatchKeyPressEventsForNonPrintableKeys |=
              isInPrefList;

          principal->IsURIInPrefList(
              "dom.keyboardevent.keypress.hack."
              "use_legacy_keycode_and_charcode",
              &isInPrefList);
          mPresShell->mForceUseLegacyKeyCodeAndCharCodeValues |= isInPrefList;

          principal->IsURIInPrefList(
              "dom.keyboardevent.keypress.hack."
              "use_legacy_keycode_and_charcode",
              &isInPrefList);
          mPresShell->mForceUseLegacyKeyCodeAndCharCodeValues |= isInPrefList;

          principal->IsURIInPrefList(
              "dom.keyboardevent.keypress.hack."
              "use_legacy_keycode_and_charcode.addl",
              &isInPrefList);
          mPresShell->mForceUseLegacyKeyCodeAndCharCodeValues |= isInPrefList;
        }
      }
      if (mPresShell->mForceDispatchKeyPressEventsForNonPrintableKeys) {
        aEvent->mFlags.mOnlySystemGroupDispatchInContent = false;
      }
      if (mPresShell->mForceUseLegacyKeyCodeAndCharCodeValues) {
        aEvent->AsKeyboardEvent()->mUseLegacyKeyCodeAndCharCodeValues = true;
      }
    } else if (aEvent->mMessage == eMouseUp) {
      // Historically Firefox has dispatched click events for non-primary
      // buttons, but only on window and document (and inside input/textarea),
      // not on elements in general. The UI events spec forbids click (and
      // dblclick) for non-primary mouse buttons, and specifies auxclick
      // instead. In case of some websites that rely on non-primary click to
      // prevent new tab etc. and don't have auxclick code to do the same, we
      // need to revert to the historial non-standard behaviour
      if (!mPresShell->mInitializedWithClickEventDispatchingBlacklist) {
        mPresShell->mInitializedWithClickEventDispatchingBlacklist = true;

        nsCOMPtr<nsIPrincipal> principal =
            GetDocumentPrincipalToCompareWithBlacklist(*mPresShell);

        if (principal) {
          bool isInPrefList = false;
          principal->IsURIInPrefList(
              "dom.mouseevent.click.hack.use_legacy_non-primary_dispatch",
              &isInPrefList);
          mPresShell->mForceUseLegacyNonPrimaryDispatch = isInPrefList;
        }
      }
      if (mPresShell->mForceUseLegacyNonPrimaryDispatch) {
        aEvent->AsMouseEvent()->mUseLegacyNonPrimaryDispatch = true;
      }
    }

    if (aEvent->mClass == eCompositionEventClass) {
      IMEStateManager::DispatchCompositionEvent(
          eventTarget, GetPresContext(), BrowserParent::GetFocused(),
          aEvent->AsCompositionEvent(), aEventStatus, eventCBPtr);
    } else {
      EventDispatcher::Dispatch(eventTarget, GetPresContext(), aEvent, nullptr,
                                aEventStatus, eventCBPtr);
    }
  }
  return rv;
}

void PresShell::EventHandler::DispatchTouchEventToDOM(
    WidgetEvent* aEvent, nsEventStatus* aEventStatus,
    nsPresShellEventCB* aEventCB, bool aTouchIsNew) {
  // calling preventDefault on touchstart or the first touchmove for a
  // point prevents mouse events. calling it on the touchend should
  // prevent click dispatching.
  bool canPrevent = (aEvent->mMessage == eTouchStart) ||
                    (aEvent->mMessage == eTouchMove && aTouchIsNew) ||
                    (aEvent->mMessage == eTouchEnd);
  bool preventDefault = false;
  nsEventStatus tmpStatus = nsEventStatus_eIgnore;
  WidgetTouchEvent* touchEvent = aEvent->AsTouchEvent();

  // loop over all touches and dispatch events on any that have changed
  for (dom::Touch* touch : touchEvent->mTouches) {
    // We should remove all suppressed touch instances in
    // TouchManager::PreHandleEvent.
    MOZ_ASSERT(!touch->mIsTouchEventSuppressed);

    if (!touch || !touch->mChanged) {
      continue;
    }

    nsCOMPtr<EventTarget> targetPtr = touch->mTarget;
    nsCOMPtr<nsIContent> content = do_QueryInterface(targetPtr);
    if (!content) {
      continue;
    }

    Document* doc = content->OwnerDoc();
    nsIContent* capturingContent = PresShell::GetCapturingContent();
    if (capturingContent) {
      if (capturingContent->OwnerDoc() != doc) {
        // Wrong document, don't dispatch anything.
        continue;
      }
      content = capturingContent;
    }
    // copy the event
    MOZ_ASSERT(touchEvent->IsTrusted());
    WidgetTouchEvent newEvent(true, touchEvent->mMessage, touchEvent->mWidget);
    newEvent.AssignTouchEventData(*touchEvent, false);
    newEvent.mTarget = targetPtr;
    newEvent.mFlags.mHandledByAPZ = touchEvent->mFlags.mHandledByAPZ;

    RefPtr<PresShell> contentPresShell;
    if (doc == GetDocument()) {
      contentPresShell = doc->GetPresShell();
      if (contentPresShell) {
        // XXXsmaug huge hack. Pushing possibly capturing content,
        //         even though event target is something else.
        contentPresShell->PushCurrentEventInfo(content->GetPrimaryFrame(),
                                               content);
      }
    }

    nsPresContext* context = doc->GetPresContext();
    if (!context) {
      if (contentPresShell) {
        contentPresShell->PopCurrentEventInfo();
      }
      continue;
    }

    tmpStatus = nsEventStatus_eIgnore;
    EventDispatcher::Dispatch(targetPtr, context, &newEvent, nullptr,
                              &tmpStatus, aEventCB);
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

  if (preventDefault && canPrevent) {
    *aEventStatus = nsEventStatus_eConsumeNoDefault;
  } else {
    *aEventStatus = nsEventStatus_eIgnore;
  }
}

// Dispatch event to content only (NOT full processing)
// See also HandleEventWithTarget which does full event processing.
nsresult PresShell::HandleDOMEventWithTarget(nsIContent* aTargetContent,
                                             WidgetEvent* aEvent,
                                             nsEventStatus* aStatus) {
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
nsresult PresShell::HandleDOMEventWithTarget(nsIContent* aTargetContent,
                                             Event* aEvent,
                                             nsEventStatus* aStatus) {
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

bool PresShell::EventHandler::AdjustContextMenuKeyEvent(
    WidgetMouseEvent* aMouseEvent) {
#ifdef MOZ_XUL
  // if a menu is open, open the context menu relative to the active item on the
  // menu.
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm) {
    nsIFrame* popupFrame = pm->GetTopPopup(ePopupTypeMenu);
    if (popupFrame) {
      nsIFrame* itemFrame =
          (static_cast<nsMenuPopupFrame*>(popupFrame))->GetCurrentMenuItem();
      if (!itemFrame) itemFrame = popupFrame;

      nsCOMPtr<nsIWidget> widget = popupFrame->GetNearestWidget();
      aMouseEvent->mWidget = widget;
      LayoutDeviceIntPoint widgetPoint = widget->WidgetToScreenOffset();
      aMouseEvent->mRefPoint =
          LayoutDeviceIntPoint::FromAppUnitsToNearest(
              itemFrame->GetScreenRectInAppUnits().BottomLeft(),
              itemFrame->PresContext()->AppUnitsPerDevPixel()) -
          widgetPoint;

      mPresShell->mCurrentEventContent = itemFrame->GetContent();
      mPresShell->mCurrentEventFrame = itemFrame;

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
  nsRootPresContext* rootPC = GetPresContext()->GetRootPresContext();
  aMouseEvent->mRefPoint = LayoutDeviceIntPoint(0, 0);
  if (rootPC) {
    rootPC->PresShell()->GetViewManager()->GetRootWidget(
        getter_AddRefs(aMouseEvent->mWidget));

    if (aMouseEvent->mWidget) {
      // default the refpoint to the topleft of our document
      nsPoint offset(0, 0);
      nsIFrame* rootFrame = FrameConstructor()->GetRootFrame();
      if (rootFrame) {
        nsView* view = rootFrame->GetClosestView(&offset);
        offset += view->GetOffsetToWidget(aMouseEvent->mWidget);
        aMouseEvent->mRefPoint = LayoutDeviceIntPoint::FromAppUnitsToNearest(
            offset, GetPresContext()->AppUnitsPerDevPixel());
      }
    }
  } else {
    aMouseEvent->mWidget = nullptr;
  }

  // see if we should use the caret position for the popup
  LayoutDeviceIntPoint caretPoint;
  // Beware! This may flush notifications via synchronous
  // ScrollSelectionIntoView.
  if (PrepareToUseCaretPosition(MOZ_KnownLive(aMouseEvent->mWidget),
                                caretPoint)) {
    // caret position is good
    int32_t devPixelRatio = GetPresContext()->AppUnitsPerDevPixel();
    caretPoint = LayoutDeviceIntPoint::FromAppUnitsToNearest(
        ViewportUtils::LayoutToVisual(
            LayoutDeviceIntPoint::ToAppUnits(caretPoint, devPixelRatio),
            GetPresContext()->PresShell()),
        devPixelRatio);
    aMouseEvent->mRefPoint = caretPoint;
    return true;
  }

  // If we're here because of the key-equiv for showing context menus, we
  // have to reset the event target to the currently focused element. Get it
  // from the focus controller.
  RefPtr<Element> currentFocus;
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm) {
    currentFocus = fm->GetFocusedElement();
  }

  // Reset event coordinates relative to focused frame in view
  if (currentFocus) {
    nsCOMPtr<nsIContent> currentPointElement;
    GetCurrentItemAndPositionForElement(
        currentFocus, getter_AddRefs(currentPointElement),
        aMouseEvent->mRefPoint, MOZ_KnownLive(aMouseEvent->mWidget));
    if (currentPointElement) {
      mPresShell->mCurrentEventContent = currentPointElement;
      mPresShell->mCurrentEventFrame = nullptr;
      mPresShell->GetCurrentEventFrame();
    }
  }

  return true;
}

// PresShell::EventHandler::PrepareToUseCaretPosition
//
//    This checks to see if we should use the caret position for popup context
//    menus. Returns true if the caret position should be used, and the
//    coordinates of that position is returned in aTargetPt. This function
//    will also scroll the window as needed to make the caret visible.
//
//    The event widget should be the widget that generated the event, and
//    whose coordinate system the resulting event's mRefPoint should be
//    relative to.  The returned point is in device pixels realtive to the
//    widget passed in.
bool PresShell::EventHandler::PrepareToUseCaretPosition(
    nsIWidget* aEventWidget, LayoutDeviceIntPoint& aTargetPt) {
  nsresult rv;

  // check caret visibility
  RefPtr<nsCaret> caret = mPresShell->GetCaret();
  NS_ENSURE_TRUE(caret, false);

  bool caretVisible = caret->IsVisible();
  if (!caretVisible) return false;

  // caret selection, this is a temporary weak reference, so no refcounting is
  // needed
  Selection* domSelection = caret->GetSelection();
  NS_ENSURE_TRUE(domSelection, false);

  // since the match could be an anonymous textnode inside a
  // <textarea> or text <input>, we need to get the outer frame
  // note: frames are not refcounted
  nsIFrame* frame = nullptr;  // may be nullptr
  nsINode* node = domSelection->GetFocusNode();
  NS_ENSURE_TRUE(node, false);
  nsCOMPtr<nsIContent> content = nsIContent::FromNode(node);
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
    rv =
        MOZ_KnownLive(mPresShell)
            ->ScrollContentIntoView(
                content, ScrollAxis(kScrollMinimum, WhenToScroll::IfNotVisible),
                ScrollAxis(kScrollMinimum, WhenToScroll::IfNotVisible),
                ScrollFlags::ScrollOverflowHidden |
                    ScrollFlags::IgnoreMarginAndPadding);
    NS_ENSURE_SUCCESS(rv, false);
    frame = content->GetPrimaryFrame();
    NS_WARNING_ASSERTION(frame, "No frame for focused content?");
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
    selCon = static_cast<nsISelectionController*>(mPresShell);
  if (selCon) {
    rv = selCon->ScrollSelectionIntoView(
        nsISelectionController::SELECTION_NORMAL,
        nsISelectionController::SELECTION_FOCUS_REGION,
        nsISelectionController::SCROLL_SYNCHRONOUS);
    NS_ENSURE_SUCCESS(rv, false);
  }

  nsPresContext* presContext = GetPresContext();

  // get caret position relative to the closest view
  nsRect caretCoords;
  nsIFrame* caretFrame = caret->GetGeometry(&caretCoords);
  if (!caretFrame) return false;
  nsPoint viewOffset;
  nsView* view = caretFrame->GetClosestView(&viewOffset);
  if (!view) return false;
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

void PresShell::EventHandler::GetCurrentItemAndPositionForElement(
    Element* aFocusedElement, nsIContent** aTargetToUse,
    LayoutDeviceIntPoint& aTargetPt, nsIWidget* aRootWidget) {
  nsCOMPtr<nsIContent> focusedContent = aFocusedElement;
  MOZ_KnownLive(mPresShell)
      ->ScrollContentIntoView(focusedContent, ScrollAxis(), ScrollAxis(),
                              ScrollFlags::ScrollOverflowHidden);

  nsPresContext* presContext = GetPresContext();

  bool istree = false, checkLineHeight = true;
  nscoord extraTreeY = 0;

#ifdef MOZ_XUL
  // Set the position to just underneath the current item for multi-select
  // lists or just underneath the selected item for single-select lists. If
  // the element is not a list, or there is no selection, leave the position
  // as is.
  nsCOMPtr<Element> item;
  nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelect =
      aFocusedElement->AsXULMultiSelectControl();
  if (multiSelect) {
    checkLineHeight = false;

    int32_t currentIndex;
    multiSelect->GetCurrentIndex(&currentIndex);
    if (currentIndex >= 0) {
      RefPtr<XULTreeElement> tree = XULTreeElement::FromNode(focusedContent);
      // Tree view special case (tree items have no frames)
      // Get the focused row and add its coordinates, which are already in
      // pixels
      // XXX Boris, should we create a new interface so that this doesn't
      // need to know about trees? Something like nsINodelessChildCreator
      // which could provide the current focus coordinates?
      if (tree) {
        tree->EnsureRowIsVisible(currentIndex);
        int32_t firstVisibleRow = tree->GetFirstVisibleRow();
        int32_t rowHeight = tree->RowHeight();

        extraTreeY += nsPresContext::CSSPixelsToAppUnits(
            (currentIndex - firstVisibleRow + 1) * rowHeight);
        istree = true;

        RefPtr<nsTreeColumns> cols = tree->GetColumns();
        if (cols) {
          nsTreeColumn* col = cols->GetFirstColumn();
          if (col) {
            RefPtr<Element> colElement = col->Element();
            nsIFrame* frame = colElement->GetPrimaryFrame();
            if (frame) {
              extraTreeY += frame->GetSize().height;
            }
          }
        }
      } else {
        multiSelect->GetCurrentItem(getter_AddRefs(item));
      }
    }
  } else {
    // don't check menulists as the selected item will be inside a popup.
    nsCOMPtr<nsIDOMXULMenuListElement> menulist =
        aFocusedElement->AsXULMenuList();
    if (!menulist) {
      nsCOMPtr<nsIDOMXULSelectControlElement> select =
          aFocusedElement->AsXULSelectControl();
      if (select) {
        checkLineHeight = false;
        select->GetSelectedItem(getter_AddRefs(item));
      }
    }
  }

  if (item) {
    focusedContent = item;
  }
#endif

  nsIFrame* frame = focusedContent->GetPrimaryFrame();
  if (frame) {
    NS_ASSERTION(
        frame->PresContext() == GetPresContext(),
        "handling event for focused content that is not in our document?");

    nsPoint frameOrigin(0, 0);

    // Get the frame's origin within its view
    nsView* view = frame->GetClosestView(&frameOrigin);
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
        nsIScrollableFrame* scrollFrame =
            nsLayoutUtils::GetNearestScrollableFrame(frame);
        if (scrollFrame) {
          nsSize scrollAmount = scrollFrame->GetLineScrollAmount();
          nsIFrame* f = do_QueryFrame(scrollFrame);
          int32_t APD = presContext->AppUnitsPerDevPixel();
          int32_t scrollAPD = f->PresContext()->AppUnitsPerDevPixel();
          scrollAmount = scrollAmount.ScaleToOtherAppUnits(scrollAPD, APD);
          if (extra > scrollAmount.height) {
            extra = scrollAmount.height;
          }
        }
      }
    }

    aTargetPt.x = presContext->AppUnitsToDevPixels(frameOrigin.x);
    aTargetPt.y =
        presContext->AppUnitsToDevPixels(frameOrigin.y + extra + extraTreeY);
  }

  NS_IF_ADDREF(*aTargetToUse = focusedContent);
}

bool PresShell::ShouldIgnoreInvalidation() {
  return mPaintingSuppressed || !mIsActive || mIsNeverPainting;
}

void PresShell::WillPaint() {
  // Check the simplest things first.  In particular, it's important to
  // check mIsActive before making any of the more expensive calls such
  // as GetRootPresContext, for the case of a browser with a large
  // number of tabs.
  // Don't bother doing anything if some viewmanager in our tree is painting
  // while we still have painting suppressed or we are not active.
  if (!mIsActive || mPaintingSuppressed || !IsVisible()) {
    return;
  }

  nsRootPresContext* rootPresContext = mPresContext->GetRootPresContext();
  if (!rootPresContext) {
    // In some edge cases, such as when we don't have a root frame yet,
    // we can't find the root prescontext. There's nothing to do in that
    // case.
    return;
  }

  rootPresContext->FlushWillPaintObservers();
  if (mIsDestroying) return;

  // Process reflows, if we have them, to reduce flicker due to invalidates and
  // reflow being interspersed.  Note that we _do_ allow this to be
  // interruptible; if we can't do all the reflows it's better to flicker a bit
  // than to freeze up.
  FlushPendingNotifications(
      ChangesToFlush(FlushType::InterruptibleLayout, false));
}

void PresShell::WillPaintWindow() {
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

void PresShell::DidPaintWindow() {
  nsRootPresContext* rootPresContext = mPresContext->GetRootPresContext();
  if (rootPresContext != mPresContext) {
    // This could be a popup's presshell. No point in notifying XPConnect
    // about compositing of popups.
    return;
  }

  if (!mHasReceivedPaintMessage) {
    mHasReceivedPaintMessage = true;

    nsCOMPtr<nsIObserverService> obsvc = services::GetObserverService();
    if (obsvc && mDocument) {
      nsPIDOMWindowOuter* window = mDocument->GetWindow();
      nsCOMPtr<nsIDOMChromeWindow> chromeWin(do_QueryInterface(window));
      if (chromeWin) {
        obsvc->NotifyObservers(chromeWin, "widget-first-paint", nullptr);
      }
    }
  }
}

bool PresShell::IsVisible() const {
  if (!mIsActive || !mViewManager) return false;

  nsView* view = mViewManager->GetRootView();
  if (!view) return true;

  // inner view of subdoc frame
  view = view->GetParent();
  if (!view) return true;

  // subdoc view
  view = view->GetParent();
  if (!view) return true;

  nsIFrame* frame = view->GetFrame();
  if (!frame) return true;

  return frame->IsVisibleConsideringAncestors(
      nsIFrame::VISIBILITY_CROSS_CHROME_CONTENT_BOUNDARY);
}

void PresShell::SuppressDisplayport(bool aEnabled) {
  if (aEnabled) {
    mActiveSuppressDisplayport++;
  } else if (mActiveSuppressDisplayport > 0) {
    bool isSuppressed = IsDisplayportSuppressed();
    mActiveSuppressDisplayport--;
    if (isSuppressed && !IsDisplayportSuppressed()) {
      // We unsuppressed the displayport, trigger a paint
      if (nsIFrame* rootFrame = mFrameConstructor->GetRootFrame()) {
        rootFrame->SchedulePaint();
      }
    }
  }
}

static bool sDisplayPortSuppressionRespected = true;

void PresShell::RespectDisplayportSuppression(bool aEnabled) {
  bool isSuppressed = IsDisplayportSuppressed();
  sDisplayPortSuppressionRespected = aEnabled;
  if (isSuppressed && !IsDisplayportSuppressed()) {
    // We unsuppressed the displayport, trigger a paint
    if (nsIFrame* rootFrame = mFrameConstructor->GetRootFrame()) {
      rootFrame->SchedulePaint();
    }
  }
}

bool PresShell::IsDisplayportSuppressed() {
  return sDisplayPortSuppressionRespected && mActiveSuppressDisplayport > 0;
}

static void FreezeElement(nsISupports* aSupports) {
  if (nsCOMPtr<nsIObjectLoadingContent> olc = do_QueryInterface(aSupports)) {
    olc->StopPluginInstance();
  }
}

static CallState FreezeSubDocument(Document& aDocument) {
  if (PresShell* presShell = aDocument.GetPresShell()) {
    presShell->Freeze();
  }
  return CallState::Continue;
}

void PresShell::Freeze() {
  mUpdateApproximateFrameVisibilityEvent.Revoke();

  MaybeReleaseCapturingContent();

  mDocument->EnumerateActivityObservers(FreezeElement);

  if (mCaret) {
    SetCaretEnabled(false);
  }

  mPaintingSuppressed = true;

  if (mDocument) {
    mDocument->EnumerateSubDocuments(FreezeSubDocument);
  }

  nsPresContext* presContext = GetPresContext();
  if (presContext) {
    presContext->DisableInteractionTimeRecording();
    if (presContext->RefreshDriver()->GetPresContext() == presContext) {
      presContext->RefreshDriver()->Freeze();
    }
  }

  mFrozen = true;
  if (mDocument) {
    UpdateImageLockingState();
  }
}

void PresShell::FireOrClearDelayedEvents(bool aFireEvents) {
  mNoDelayedMouseEvents = false;
  mNoDelayedKeyEvents = false;
  if (!aFireEvents) {
    mDelayedEvents.Clear();
    return;
  }

  if (mDocument) {
    RefPtr<Document> doc = mDocument;
    while (!mIsDestroying && mDelayedEvents.Length() &&
           !doc->EventHandlingSuppressed()) {
      UniquePtr<DelayedEvent> ev = std::move(mDelayedEvents[0]);
      mDelayedEvents.RemoveElementAt(0);
      if (ev->IsKeyPressEvent() && mIsLastKeyDownCanceled) {
        continue;
      }
      ev->Dispatch();
    }
    if (!doc->EventHandlingSuppressed()) {
      mDelayedEvents.Clear();
    }
  }
}

void PresShell::Thaw() {
  nsPresContext* presContext = GetPresContext();
  if (presContext &&
      presContext->RefreshDriver()->GetPresContext() == presContext) {
    presContext->RefreshDriver()->Thaw();
  }

  mDocument->EnumerateActivityObservers([](nsISupports* aSupports) {
    if (nsCOMPtr<nsIObjectLoadingContent> olc = do_QueryInterface(aSupports)) {
      olc->AsyncStartPluginInstance();
    }
  });

  if (mDocument) {
    mDocument->EnumerateSubDocuments([](Document& aSubDoc) {
      if (PresShell* presShell = aSubDoc.GetPresShell()) {
        presShell->Thaw();
      }
      return CallState::Continue;
    });
  }

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

void PresShell::MaybeScheduleReflow() {
  ASSERT_REFLOW_SCHEDULED_STATE();
  if (mObservingLayoutFlushes || mIsDestroying || mIsReflowing ||
      mDirtyRoots.IsEmpty())
    return;

  if (!mPresContext->HasPendingInterrupt() || !ScheduleReflowOffTimer()) {
    ScheduleReflow();
  }

  ASSERT_REFLOW_SCHEDULED_STATE();
}

void PresShell::ScheduleReflow() {
  ASSERT_REFLOW_SCHEDULED_STATE();
  DoObserveLayoutFlushes();
  ASSERT_REFLOW_SCHEDULED_STATE();
}

void PresShell::WillCauseReflow() {
  nsContentUtils::AddScriptBlocker();
  ++mChangeNestCount;
}

void PresShell::DidCauseReflow() {
  NS_ASSERTION(mChangeNestCount != 0, "Unexpected call to DidCauseReflow()");
  --mChangeNestCount;
  nsContentUtils::RemoveScriptBlocker();
}

void PresShell::WillDoReflow() {
  mDocument->FlushUserFontSet();

  mPresContext->FlushCounterStyles();

  mPresContext->FlushFontFeatureValues();

  mLastReflowStart = GetPerformanceNowUnclamped();
}

void PresShell::DidDoReflow(bool aInterruptible) {
  HandlePostedReflowCallbacks(aInterruptible);
  if (mIsDestroying) {
    return;
  }

  nsAutoScriptBlocker scriptBlocker;
  AutoAssertNoFlush noReentrantFlush(*this);
  if (nsCOMPtr<nsIDocShell> docShell = mPresContext->GetDocShell()) {
    DOMHighResTimeStamp now = GetPerformanceNowUnclamped();
    docShell->NotifyReflowObservers(aInterruptible, mLastReflowStart, now);
  }

  if (!mPresContext->HasPendingInterrupt()) {
    mDocument->ScheduleResizeObserversNotification();
  }

  if (StaticPrefs::layout_reflow_synthMouseMove()) {
    SynthesizeMouseMove(false);
  }

  mPresContext->NotifyMissingFonts();
}

DOMHighResTimeStamp PresShell::GetPerformanceNowUnclamped() {
  DOMHighResTimeStamp now = 0;

  if (nsPIDOMWindowInner* window = mDocument->GetInnerWindow()) {
    Performance* perf = window->GetPerformance();

    if (perf) {
      now = perf->NowUnclamped();
    }
  }

  return now;
}

void PresShell::sReflowContinueCallback(nsITimer* aTimer, void* aPresShell) {
  RefPtr<PresShell> self = static_cast<PresShell*>(aPresShell);

  MOZ_ASSERT(aTimer == self->mReflowContinueTimer, "Unexpected timer");
  self->mReflowContinueTimer = nullptr;
  self->ScheduleReflow();
}

bool PresShell::ScheduleReflowOffTimer() {
  MOZ_ASSERT(!mObservingLayoutFlushes, "Shouldn't get here");
  ASSERT_REFLOW_SCHEDULED_STATE();

  if (!mReflowContinueTimer) {
    nsresult rv = NS_NewTimerWithFuncCallback(
        getter_AddRefs(mReflowContinueTimer), sReflowContinueCallback, this, 30,
        nsITimer::TYPE_ONE_SHOT, "sReflowContinueCallback",
        mDocument->EventTargetFor(TaskCategory::Other));
    return NS_SUCCEEDED(rv);
  }
  return true;
}

bool PresShell::DoReflow(nsIFrame* target, bool aInterruptible,
                         OverflowChangedTracker* aOverflowTracker) {
#ifdef MOZ_GECKO_PROFILER
  nsIURI* uri = mDocument->GetDocumentURI();
  AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING(
      "Reflow", LAYOUT_Reflow,
      uri ? uri->GetSpecOrDefault() : NS_LITERAL_CSTRING("N/A"));
#endif

  LAYOUT_TELEMETRY_RECORD_BASE(Reflow);

  PerfStats::AutoMetricRecording<PerfStats::Metric::Reflowing> autoRecording;

  gfxTextPerfMetrics* tp = mPresContext->GetTextPerfMetrics();
  TimeStamp timeStart;
  if (tp) {
    tp->Accumulate();
    tp->reflowCount++;
    timeStart = TimeStamp::Now();
  }

  // Schedule a paint, but don't actually mark this frame as changed for
  // retained DL building purposes. If any child frames get moved, then
  // they will schedule paint again. We could probaby skip this, and just
  // schedule a similar paint when a frame is deleted.
  target->SchedulePaint(nsIFrame::PAINT_DEFAULT, false);

  nsDocShell* docShell =
      static_cast<nsDocShell*>(GetPresContext()->GetDocShell());
  RefPtr<TimelineConsumers> timelines = TimelineConsumers::Get();
  bool isTimelineRecording = timelines && timelines->HasConsumer(docShell);

  if (isTimelineRecording) {
    timelines->AddMarkerForDocShell(docShell, "Reflow",
                                    MarkerTracingType::START);
  }

#ifdef MOZ_GECKO_PROFILER
  Maybe<uint64_t> innerWindowID;
  if (auto* window = mDocument->GetInnerWindow()) {
    innerWindowID = Some(window->WindowID());
  }
  AutoProfilerTracing tracingLayoutFlush(
      "Paint", "Reflow", JS::ProfilingCategoryPair::LAYOUT,
      std::move(mReflowCause), innerWindowID);
  mReflowCause = nullptr;
#endif

  FlushPendingScrollAnchorSelections();

  if (mReflowContinueTimer) {
    mReflowContinueTimer->Cancel();
    mReflowContinueTimer = nullptr;
  }

  const bool isRoot = target == mFrameConstructor->GetRootFrame();

  MOZ_ASSERT(isRoot || aOverflowTracker,
             "caller must provide overflow tracker when reflowing "
             "non-root frames");

  // CreateReferenceRenderingContext can return nullptr
  RefPtr<gfxContext> rcx(CreateReferenceRenderingContext());

#ifdef DEBUG
  mCurrentReflowRoot = target;
#endif

  // If the target frame is the root of the frame hierarchy, then
  // use all the available space. If it's simply a `reflow root',
  // then use the target frame's size as the available space.
  WritingMode wm = target->GetWritingMode();
  LogicalSize size(wm);
  if (isRoot) {
    size = LogicalSize(wm, mPresContext->GetVisibleArea().Size());
  } else {
    size = target->GetLogicalSize();
  }

  nsOverflowAreas oldOverflow;  // initialized and used only when !isRoot
  if (!isRoot) {
    oldOverflow = target->GetOverflowAreas();
  }

  NS_ASSERTION(!target->GetNextInFlow() && !target->GetPrevInFlow(),
               "reflow roots should never split");

  // Don't pass size directly to the reflow input, since a
  // constrained height implies page/column breaking.
  LogicalSize reflowSize(wm, size.ISize(wm), NS_UNCONSTRAINEDSIZE);
  ReflowInput reflowInput(mPresContext, target, rcx, reflowSize,
                          ReflowInput::CALLER_WILL_INIT);
  reflowInput.mOrthogonalLimit = size.BSize(wm);

  if (isRoot) {
    reflowInput.Init(mPresContext);

    // When the root frame is being reflowed with unconstrained block-size
    // (which happens when we're called from
    // nsDocumentViewer::SizeToContent), we're effectively doing a
    // resize in the block direction, since it changes the meaning of
    // percentage block-sizes even if no block-sizes actually changed.
    // The same applies when we reflow again after that computation. This is
    // an unusual case, and isn't caught by ReflowInput::InitResizeFlags.
    bool hasUnconstrainedBSize = size.BSize(wm) == NS_UNCONSTRAINEDSIZE;

    if (hasUnconstrainedBSize || mLastRootReflowHadUnconstrainedBSize) {
      reflowInput.SetBResize(true);
    }

    mLastRootReflowHadUnconstrainedBSize = hasUnconstrainedBSize;
  } else {
    // Initialize reflow input with current used border and padding,
    // in case this was set specially by the parent frame when the reflow root
    // was reflowed by its parent.
    nsMargin currentBorder = target->GetUsedBorder();
    nsMargin currentPadding = target->GetUsedPadding();
    reflowInput.Init(mPresContext, Nothing(), &currentBorder, &currentPadding);
  }

  // fix the computed height
  NS_ASSERTION(reflowInput.ComputedPhysicalMargin() == nsMargin(0, 0, 0, 0),
               "reflow input should not set margin for reflow roots");
  if (size.BSize(wm) != NS_UNCONSTRAINEDSIZE) {
    nscoord computedBSize =
        size.BSize(wm) -
        reflowInput.ComputedLogicalBorderPadding().BStartEnd(wm);
    computedBSize = std::max(computedBSize, 0);
    reflowInput.SetComputedBSize(computedBSize);
  }
  NS_ASSERTION(reflowInput.ComputedISize() ==
                   size.ISize(wm) -
                       reflowInput.ComputedLogicalBorderPadding().IStartEnd(wm),
               "reflow input computed incorrect inline size");

  mPresContext->ReflowStarted(aInterruptible);
  mIsReflowing = true;

  nsReflowStatus status;
  ReflowOutput desiredSize(reflowInput);
  target->Reflow(mPresContext, desiredSize, reflowInput, status);

  // If an incremental reflow is initiated at a frame other than the
  // root frame, then its desired size had better not change!  If it's
  // initiated at the root, then the size better not change unless its
  // height was unconstrained to start with.
  nsRect boundsRelativeToTarget =
      nsRect(0, 0, desiredSize.Width(), desiredSize.Height());
  NS_ASSERTION((isRoot && size.BSize(wm) == NS_UNCONSTRAINEDSIZE) ||
                   (desiredSize.ISize(wm) == size.ISize(wm) &&
                    desiredSize.BSize(wm) == size.BSize(wm)),
               "non-root frame's desired size changed during an "
               "incremental reflow");
  NS_ASSERTION(status.IsEmpty(), "reflow roots should never split");

  target->SetSize(boundsRelativeToTarget.Size());

  // Always use boundsRelativeToTarget here, not
  // desiredSize.GetVisualOverflowArea(), because for root frames (where they
  // could be different, since root frames are allowed to have overflow) the
  // root view bounds need to match the viewport bounds; the view manager
  // "window dimensions" code depends on it.
  nsContainerFrame::SyncFrameViewAfterReflow(
      mPresContext, target, target->GetView(), boundsRelativeToTarget);
  nsContainerFrame::SyncWindowProperties(mPresContext, target,
                                         target->GetView(), rcx,
                                         nsContainerFrame::SET_ASYNC);

  target->DidReflow(mPresContext, nullptr);
  if (target->IsInScrollAnchorChain()) {
    ScrollAnchorContainer* container = ScrollAnchorContainer::FindFor(target);
    PostPendingScrollAnchorAdjustment(container);
  }
  if (isRoot && size.BSize(wm) == NS_UNCONSTRAINEDSIZE) {
    mPresContext->SetVisibleArea(boundsRelativeToTarget);
  }

#ifdef DEBUG
  mCurrentReflowRoot = nullptr;
#endif

  if (!isRoot && oldOverflow != target->GetOverflowAreas()) {
    // The overflow area changed.  Propagate this change to ancestors.
    aOverflowTracker->AddFrame(target->GetParent(),
                               OverflowChangedTracker::CHILDREN_CHANGED);
  }

  NS_ASSERTION(
      mPresContext->HasPendingInterrupt() || mFramesToDirty.Count() == 0,
      "Why do we need to dirty anything if not interrupted?");

  mIsReflowing = false;
  bool interrupted = mPresContext->HasPendingInterrupt();
  if (interrupted) {
    // Make sure target gets reflowed again.
    for (auto iter = mFramesToDirty.Iter(); !iter.Done(); iter.Next()) {
      // Mark frames dirty until target frame.
      nsPtrHashKey<nsIFrame>* p = iter.Get();
      for (nsIFrame* f = p->GetKey(); f && !NS_SUBTREE_DIRTY(f);
           f = f->GetParent()) {
        f->AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
        if (f->IsFlexItem()) {
          nsFlexContainerFrame::MarkCachedFlexMeasurementsDirty(f);
        }

        if (f == target) {
          break;
        }
      }
    }

    NS_ASSERTION(NS_SUBTREE_DIRTY(target), "Why is the target not dirty?");
    mDirtyRoots.Add(target);
    SetNeedLayoutFlush();

    // Clear mFramesToDirty after we've done the NS_SUBTREE_DIRTY(target)
    // assertion so that if it fails it's easier to see what's going on.
#ifdef NOISY_INTERRUPTIBLE_REFLOW
    printf("mFramesToDirty.Count() == %u\n", mFramesToDirty.Count());
#endif /* NOISY_INTERRUPTIBLE_REFLOW */
    mFramesToDirty.Clear();

    // Any FlushPendingNotifications with interruptible reflows
    // should be suppressed now. We don't want to do extra reflow work
    // before our reflow event happens.
    mWasLastReflowInterrupted = true;
    MaybeScheduleReflow();
  }

  // dump text perf metrics for reflows with significant text processing
  if (tp) {
    if (tp->current.numChars > 100) {
      TimeDuration reflowTime = TimeStamp::Now() - timeStart;
      LogTextPerfStats(tp, this, tp->current, reflowTime.ToMilliseconds(),
                       eLog_reflow, nullptr);
    }
    tp->Accumulate();
  }

  if (isTimelineRecording) {
    timelines->AddMarkerForDocShell(docShell, "Reflow", MarkerTracingType::END);
  }

  return !interrupted;
}

#ifdef DEBUG
void PresShell::DoVerifyReflow() {
  if (GetVerifyReflowEnable()) {
    // First synchronously render what we have so far so that we can
    // see it.
    nsView* rootView = mViewManager->GetRootView();
    mViewManager->InvalidateView(rootView);

    FlushPendingNotifications(FlushType::Layout);
    mInVerifyReflow = true;
    bool ok = VerifyIncrementalReflow();
    mInVerifyReflow = false;
    if (VerifyReflowFlags::All & gVerifyReflowFlags) {
      printf("ProcessReflowCommands: finished (%s)\n", ok ? "ok" : "failed");
    }

    if (!mDirtyRoots.IsEmpty()) {
      printf("XXX yikes! reflow commands queued during verify-reflow\n");
    }
  }
}
#endif

// used with Telemetry metrics
#define NS_LONG_REFLOW_TIME_MS 5000

bool PresShell::ProcessReflowCommands(bool aInterruptible) {
  if (mDirtyRoots.IsEmpty() && !mShouldUnsuppressPainting) {
    // Nothing to do; bail out
    return true;
  }

  mozilla::TimeStamp timerStart = mozilla::TimeStamp::Now();
  bool interrupted = false;
  if (!mDirtyRoots.IsEmpty()) {
#ifdef DEBUG
    if (VerifyReflowFlags::DumpCommands & gVerifyReflowFlags) {
      printf("ProcessReflowCommands: begin incremental reflow\n");
    }
#endif

    // If reflow is interruptible, then make a note of our deadline.
    const PRIntervalTime deadline =
        aInterruptible
            ? PR_IntervalNow() + PR_MicrosecondsToInterval(gMaxRCProcessingTime)
            : (PRIntervalTime)0;

    // Scope for the reflow entry point
    {
      nsAutoScriptBlocker scriptBlocker;
      WillDoReflow();
      AUTO_LAYOUT_PHASE_ENTRY_POINT(GetPresContext(), Reflow);
      nsViewManager::AutoDisableRefresh refreshBlocker(mViewManager);

      OverflowChangedTracker overflowTracker;

      do {
        // Send an incremental reflow notification to the target frame.
        nsIFrame* target = mDirtyRoots.PopShallowestRoot();

        if (!NS_SUBTREE_DIRTY(target)) {
          // It's not dirty anymore, which probably means the notification
          // was posted in the middle of a reflow (perhaps with a reflow
          // root in the middle).  Don't do anything.
          continue;
        }

        interrupted = !DoReflow(target, aInterruptible, &overflowTracker);

        // Keep going until we're out of reflow commands, or we've run
        // past our deadline, or we're interrupted.
      } while (!interrupted && !mDirtyRoots.IsEmpty() &&
               (!aInterruptible || PR_IntervalNow() < deadline));

      interrupted = !mDirtyRoots.IsEmpty();

      overflowTracker.Flush();

      if (!interrupted) {
        // We didn't get interrupted. Go ahead and perform scroll anchor
        // adjustments.
        FlushPendingScrollAnchorAdjustments();
      }
    }

    // Exiting the scriptblocker might have killed us
    if (!mIsDestroying) {
      DidDoReflow(aInterruptible);
    }

    // DidDoReflow might have killed us
    if (!mIsDestroying) {
#ifdef DEBUG
      if (VerifyReflowFlags::DumpCommands & gVerifyReflowFlags) {
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
        // And record that we might need flushing
        SetNeedLayoutFlush();
      }
    }
  }

  if (!mIsDestroying && mShouldUnsuppressPainting && mDirtyRoots.IsEmpty()) {
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

    if (intElapsed > NS_LONG_REFLOW_TIME_MS) {
      Telemetry::Accumulate(Telemetry::LONG_REFLOW_INTERRUPTIBLE,
                            aInterruptible ? 1 : 0);
    }
  }

  return !interrupted;
}

void PresShell::WindowSizeMoveDone() {
  if (mPresContext) {
    EventStateManager::ClearGlobalActiveContent(nullptr);
    ClearMouseCapture(nullptr);
  }
}

NS_IMETHODIMP
PresShell::Observe(nsISupports* aSubject, const char* aTopic,
                   const char16_t* aData) {
  if (mIsDestroying) {
    NS_WARNING("our observers should have been unregistered by now");
    return NS_OK;
  }

  if (!nsCRT::strcmp(aTopic, "memory-pressure")) {
    if (!AssumeAllFramesVisible() && mPresContext->IsRootContentDocument()) {
      DoUpdateApproximateFrameVisibility(/* aRemoveOnly = */ true);
    }
    return NS_OK;
  }

  if (!nsCRT::strcmp(aTopic, NS_WIDGET_WAKE_OBSERVER_TOPIC)) {
    mLastOSWake = TimeStamp::Now();
    return NS_OK;
  }

  // For parent process, user may expect the UI is interactable after a
  // tab (previously opened page or home page) has restored.
  if (!nsCRT::strcmp(aTopic, "sessionstore-one-or-no-tab-restored")) {
    MOZ_ASSERT(XRE_IsParentProcess());
    sProcessInteractable = true;

    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (os) {
      os->RemoveObserver(this, "sessionstore-one-or-no-tab-restored");
    }
    return NS_OK;
  }

  if (!nsCRT::strcmp(aTopic, "font-info-updated")) {
    mPresContext->ForceReflowForFontInfoUpdate();
    return NS_OK;
  }

  if (!nsCRT::strcmp(aTopic, "look-and-feel-pref-changed")) {
    ThemeChanged();
    return NS_OK;
  }

  NS_WARNING("unrecognized topic in PresShell::Observe");
  return NS_ERROR_FAILURE;
}

bool PresShell::AddRefreshObserver(nsARefreshObserver* aObserver,
                                   FlushType aFlushType) {
  nsPresContext* presContext = GetPresContext();
  if (MOZ_UNLIKELY(!presContext)) {
    return false;
  }
  presContext->RefreshDriver()->AddRefreshObserver(aObserver, aFlushType);
  return true;
}

bool PresShell::RemoveRefreshObserver(nsARefreshObserver* aObserver,
                                      FlushType aFlushType) {
  nsPresContext* presContext = GetPresContext();
  return presContext && presContext->RefreshDriver()->RemoveRefreshObserver(
                            aObserver, aFlushType);
}

bool PresShell::AddPostRefreshObserver(nsAPostRefreshObserver* aObserver) {
  nsPresContext* presContext = GetPresContext();
  if (!presContext) {
    return false;
  }
  presContext->RefreshDriver()->AddPostRefreshObserver(aObserver);
  return true;
}

bool PresShell::RemovePostRefreshObserver(nsAPostRefreshObserver* aObserver) {
  nsPresContext* presContext = GetPresContext();
  if (!presContext) {
    return false;
  }
  presContext->RefreshDriver()->RemovePostRefreshObserver(aObserver);
  return true;
}

void PresShell::DoObserveStyleFlushes() {
  MOZ_ASSERT(!ObservingStyleFlushes());
  mObservingStyleFlushes = true;

  if (MOZ_LIKELY(!mDocument->GetBFCacheEntry())) {
    mPresContext->RefreshDriver()->AddStyleFlushObserver(this);
  }
}

void PresShell::DoObserveLayoutFlushes() {
  MOZ_ASSERT(!ObservingLayoutFlushes());
  mObservingLayoutFlushes = true;

  if (MOZ_LIKELY(!mDocument->GetBFCacheEntry())) {
    mPresContext->RefreshDriver()->AddLayoutFlushObserver(this);
  }
}

//------------------------------------------------------
// End of protected and private methods on the PresShell
//------------------------------------------------------

//------------------------------------------------------------------
//-- Delayed event Classes Impls
//------------------------------------------------------------------

PresShell::DelayedInputEvent::DelayedInputEvent()
    : DelayedEvent(), mEvent(nullptr) {}

PresShell::DelayedInputEvent::~DelayedInputEvent() { delete mEvent; }

void PresShell::DelayedInputEvent::Dispatch() {
  if (!mEvent || !mEvent->mWidget) {
    return;
  }
  nsCOMPtr<nsIWidget> widget = mEvent->mWidget;
  nsEventStatus status;
  widget->DispatchEvent(mEvent, status);
}

PresShell::DelayedMouseEvent::DelayedMouseEvent(WidgetMouseEvent* aEvent)
    : DelayedInputEvent() {
  MOZ_DIAGNOSTIC_ASSERT(aEvent->IsTrusted());
  WidgetMouseEvent* mouseEvent =
      new WidgetMouseEvent(true, aEvent->mMessage, aEvent->mWidget,
                           aEvent->mReason, aEvent->mContextMenuTrigger);
  mouseEvent->AssignMouseEventData(*aEvent, false);
  mEvent = mouseEvent;
}

PresShell::DelayedKeyEvent::DelayedKeyEvent(WidgetKeyboardEvent* aEvent)
    : DelayedInputEvent() {
  MOZ_DIAGNOSTIC_ASSERT(aEvent->IsTrusted());
  WidgetKeyboardEvent* keyEvent =
      new WidgetKeyboardEvent(true, aEvent->mMessage, aEvent->mWidget);
  keyEvent->AssignKeyEventData(*aEvent, false);
  keyEvent->mFlags.mIsSynthesizedForTests =
      aEvent->mFlags.mIsSynthesizedForTests;
  keyEvent->mFlags.mIsSuppressedOrDelayed = true;
  mEvent = keyEvent;
}

bool PresShell::DelayedKeyEvent::IsKeyPressEvent() {
  return mEvent->mMessage == eKeyPress;
}

// Start of DEBUG only code

#ifdef DEBUG

static void LogVerifyMessage(nsIFrame* k1, nsIFrame* k2, const char* aMsg) {
  nsAutoString n1, n2;
  if (k1) {
    k1->GetFrameName(n1);
  } else {
    n1.AssignLiteral(u"(null)");
  }

  if (k2) {
    k2->GetFrameName(n2);
  } else {
    n2.AssignLiteral(u"(null)");
  }

  printf("verifyreflow: %s %p != %s %p  %s\n",
         NS_LossyConvertUTF16toASCII(n1).get(), (void*)k1,
         NS_LossyConvertUTF16toASCII(n2).get(), (void*)k2, aMsg);
}

static void LogVerifyMessage(nsIFrame* k1, nsIFrame* k2, const char* aMsg,
                             const nsRect& r1, const nsRect& r2) {
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
  printf("{%d, %d, %d, %d}\n  %s\n", r2.x, r2.y, r2.width, r2.height, aMsg);
}

static void LogVerifyMessage(nsIFrame* k1, nsIFrame* k2, const char* aMsg,
                             const nsIntRect& r1, const nsIntRect& r2) {
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
  printf("{%d, %d, %d, %d}\n  %s\n", r2.x, r2.y, r2.width, r2.height, aMsg);
}

static bool CompareTrees(nsPresContext* aFirstPresContext,
                         nsIFrame* aFirstFrame,
                         nsPresContext* aSecondPresContext,
                         nsIFrame* aSecondFrame) {
  if (!aFirstPresContext || !aFirstFrame || !aSecondPresContext ||
      !aSecondFrame)
    return true;
  // XXX Evil hack to reduce false positives; I can't seem to figure
  // out how to flush scrollbar changes correctly
  // if (aFirstFrame->IsScrollbarFrame())
  //  return true;
  bool ok = true;
  nsIFrame::ChildListIterator lists1(aFirstFrame);
  nsIFrame::ChildListIterator lists2(aSecondFrame);
  do {
    const nsFrameList& kids1 =
        !lists1.IsDone() ? lists1.CurrentList() : nsFrameList();
    const nsFrameList& kids2 =
        !lists2.IsDone() ? lists2.CurrentList() : nsFrameList();
    int32_t l1 = kids1.GetLength();
    int32_t l2 = kids2.GetLength();
    if (l1 != l2) {
      ok = false;
      LogVerifyMessage(kids1.FirstChild(), kids2.FirstChild(),
                       "child counts don't match: ");
      printf("%d != %d\n", l1, l2);
      if (!(VerifyReflowFlags::All & gVerifyReflowFlags)) {
        break;
      }
    }

    LayoutDeviceIntRect r1, r2;
    nsView* v1;
    nsView* v2;
    for (nsFrameList::Enumerator e1(kids1), e2(kids2);; e1.Next(), e2.Next()) {
      nsIFrame* k1 = e1.get();
      nsIFrame* k2 = e2.get();
      if (((nullptr == k1) && (nullptr != k2)) ||
          ((nullptr != k1) && (nullptr == k2))) {
        ok = false;
        LogVerifyMessage(k1, k2, "child lists are different\n");
        break;
      } else if (nullptr != k1) {
        // Verify that the frames are the same size
        if (!k1->GetRect().IsEqualInterior(k2->GetRect())) {
          ok = false;
          LogVerifyMessage(k1, k2, "(frame rects)", k1->GetRect(),
                           k2->GetRect());
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
        } else if (nullptr != v1) {
          if (!v1->GetBounds().IsEqualInterior(v2->GetBounds())) {
            LogVerifyMessage(k1, k2, "(view rects)", v1->GetBounds(),
                             v2->GetBounds());
          }

          nsIWidget* w1 = v1->GetWidget();
          nsIWidget* w2 = v2->GetWidget();
          if (((nullptr == w1) && (nullptr != w2)) ||
              ((nullptr != w1) && (nullptr == w2))) {
            ok = false;
            LogVerifyMessage(k1, k2, "child widgets are not matched\n");
          } else if (nullptr != w1) {
            r1 = w1->GetBounds();
            r2 = w2->GetBounds();
            if (!r1.IsEqualEdges(r2)) {
              LogVerifyMessage(k1, k2, "(widget rects)", r1.ToUnknownRect(),
                               r2.ToUnknownRect());
            }
          }
        }
        if (!ok && !(VerifyReflowFlags::All & gVerifyReflowFlags)) {
          break;
        }

        // XXX Should perhaps compare their float managers.

        // Compare the sub-trees too
        if (!CompareTrees(aFirstPresContext, k1, aSecondPresContext, k2)) {
          ok = false;
          if (!(VerifyReflowFlags::All & gVerifyReflowFlags)) {
            break;
          }
        }
      } else {
        break;
      }
    }
    if (!ok && (!(VerifyReflowFlags::All & gVerifyReflowFlags))) {
      break;
    }

    lists1.Next();
    lists2.Next();
    if (lists1.IsDone() != lists2.IsDone() ||
        (!lists1.IsDone() && lists1.CurrentID() != lists2.CurrentID())) {
      if (!(VerifyReflowFlags::All & gVerifyReflowFlags)) {
        ok = false;
      }
      LogVerifyMessage(kids1.FirstChild(), kids2.FirstChild(),
                       "child list names are not matched: ");
      fprintf(
          stdout, "%s != %s\n",
          !lists1.IsDone() ? mozilla::layout::ChildListName(lists1.CurrentID())
                           : "(null)",
          !lists2.IsDone() ? mozilla::layout::ChildListName(lists2.CurrentID())
                           : "(null)");
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
      nsAtom* tag;
      content->GetTag(tag);
      if (nullptr != tag) {
        NS_RELEASE(tag);
        return aRoot;
      }
    }

    // Try one of the children
    for (nsIFrame* kid : aRoot->PrincipalChildList()) {
      nsIFrame* result = FindTopFrame(kid);
      if (nullptr != result) {
        return result;
      }
    }
  }
  return nullptr;
}
#endif

#ifdef DEBUG

// After an incremental reflow, we verify the correctness by doing a
// full reflow into a fresh frame tree.
bool PresShell::VerifyIncrementalReflow() {
  if (VerifyReflowFlags::Noisy & gVerifyReflowFlags) {
    printf("Building Verification Tree...\n");
  }

  // Create a presentation context to view the new frame tree
  RefPtr<nsPresContext> cx = new nsRootPresContext(
      mDocument, mPresContext->IsPaginated()
                     ? nsPresContext::eContext_PrintPreview
                     : nsPresContext::eContext_Galley);
  NS_ENSURE_TRUE(cx, false);

  nsDeviceContext* dc = mPresContext->DeviceContext();
  nsresult rv = cx->Init(dc);
  NS_ENSURE_SUCCESS(rv, false);

  // Get our scrolling preference
  nsView* rootView = mViewManager->GetRootView();
  NS_ENSURE_TRUE(rootView->HasWidget(), false);
  nsIWidget* parentWidget = rootView->GetWidget();

  // Create a new view manager.
  RefPtr<nsViewManager> vm = new nsViewManager();
  NS_ENSURE_TRUE(vm, false);
  rv = vm->Init(dc);
  NS_ENSURE_SUCCESS(rv, false);

  // Create a child window of the parent that is our "root view/window"
  // Create a view
  nsRect tbounds = mPresContext->GetVisibleArea();
  nsView* view = vm->CreateView(tbounds, nullptr);
  NS_ENSURE_TRUE(view, false);

  // now create the widget for the view
  rv = view->CreateWidgetForParent(parentWidget, nullptr, true);
  NS_ENSURE_SUCCESS(rv, false);

  // Setup hierarchical relationship in view manager
  vm->SetRootView(view);

  // Make the new presentation context the same size as our
  // presentation context.
  cx->SetVisibleArea(mPresContext->GetVisibleArea());

  RefPtr<PresShell> presShell = mDocument->CreatePresShell(cx, vm);
  NS_ENSURE_TRUE(presShell, false);

  // Note that after we create the shell, we must make sure to destroy it
  presShell->SetVerifyReflowEnable(
      false);  // turn off verify reflow while we're
               // reflowing the test frame tree
  vm->SetPresShell(presShell);
  {
    nsAutoCauseReflowNotifier crNotifier(this);
    presShell->Initialize();
  }
  presShell->FlushPendingNotifications(FlushType::Layout);
  presShell->SetVerifyReflowEnable(
      true);  // turn on verify reflow again now that
              // we're done reflowing the test frame tree
  // Force the non-primary presshell to unsuppress; it doesn't want to normally
  // because it thinks it's hidden
  presShell->mPaintingSuppressed = false;
  if (VerifyReflowFlags::Noisy & gVerifyReflowFlags) {
    printf("Verification Tree built, comparing...\n");
  }

  // Now that the document has been reflowed, use its frame tree to
  // compare against our frame tree.
  nsIFrame* root1 = mFrameConstructor->GetRootFrame();
  nsIFrame* root2 = presShell->GetRootFrame();
  bool ok = CompareTrees(mPresContext, root1, cx, root2);
  if (!ok && (VerifyReflowFlags::Noisy & gVerifyReflowFlags)) {
    printf("Verify reflow failed, primary tree:\n");
    root1->List(stdout);
    printf("Verification tree:\n");
    root2->List(stdout);
  }

#  if 0
  // Sample code for dumping page to png
  // XXX Needs to be made more flexible
  if (!ok) {
    nsString stra;
    static int num = 0;
    stra.AppendLiteral("C:\\mozilla\\mozilla\\debug\\filea");
    stra.AppendInt(num);
    stra.AppendLiteral(".png");
    gfxUtils::WriteAsPNG(presShell, stra);
    nsString strb;
    strb.AppendLiteral("C:\\mozilla\\mozilla\\debug\\fileb");
    strb.AppendInt(num);
    strb.AppendLiteral(".png");
    gfxUtils::WriteAsPNG(presShell, strb);
    ++num;
  }
#  endif

  presShell->EndObservingDocument();
  presShell->Destroy();
  if (VerifyReflowFlags::Noisy & gVerifyReflowFlags) {
    printf("Finished Verifying Reflow...\n");
  }

  return ok;
}

// Layout debugging hooks
void PresShell::ListComputedStyles(FILE* out, int32_t aIndent) {
  nsIFrame* rootFrame = GetRootFrame();
  if (rootFrame) {
    rootFrame->Style()->List(out, aIndent);
  }

  // The root element's frame's ComputedStyle is the root of a separate tree.
  Element* rootElement = mDocument->GetRootElement();
  if (rootElement) {
    nsIFrame* rootElementFrame = rootElement->GetPrimaryFrame();
    if (rootElementFrame) {
      rootElementFrame->Style()->List(out, aIndent);
    }
  }
}

void PresShell::ListStyleSheets(FILE* out, int32_t aIndent) {
  int32_t sheetCount = StyleSet()->SheetCount(StyleOrigin::Author);
  for (int32_t i = 0; i < sheetCount; ++i) {
    StyleSet()->SheetAt(StyleOrigin::Author, i)->List(out, aIndent);
    fputs("\n", out);
  }
}
#endif

//=============================================================
//=============================================================
//-- Debug Reflow Counts
//=============================================================
//=============================================================
#ifdef MOZ_REFLOW_PERF
//-------------------------------------------------------------
void PresShell::DumpReflows() {
  if (mReflowCountMgr) {
    nsAutoCString uriStr;
    if (mDocument) {
      nsIURI* uri = mDocument->GetDocumentURI();
      if (uri) {
        uri->GetPathQueryRef(uriStr);
      }
    }
    mReflowCountMgr->DisplayTotals(uriStr.get());
    mReflowCountMgr->DisplayHTMLTotals(uriStr.get());
    mReflowCountMgr->DisplayDiffsInTotals();
  }
}

//-------------------------------------------------------------
void PresShell::CountReflows(const char* aName, nsIFrame* aFrame) {
  if (mReflowCountMgr) {
    mReflowCountMgr->Add(aName, aFrame);
  }
}

//-------------------------------------------------------------
void PresShell::PaintCount(const char* aName, gfxContext* aRenderingContext,
                           nsPresContext* aPresContext, nsIFrame* aFrame,
                           const nsPoint& aOffset, uint32_t aColor) {
  if (mReflowCountMgr) {
    mReflowCountMgr->PaintCount(aName, aRenderingContext, aPresContext, aFrame,
                                aOffset, aColor);
  }
}

//-------------------------------------------------------------
void PresShell::SetPaintFrameCount(bool aPaintFrameCounts) {
  if (mReflowCountMgr) {
    mReflowCountMgr->SetPaintFrameCounts(aPaintFrameCounts);
  }
}

bool PresShell::IsPaintingFrameCounts() {
  if (mReflowCountMgr) return mReflowCountMgr->IsPaintingFrameCounts();
  return false;
}

//------------------------------------------------------------------
//-- Reflow Counter Classes Impls
//------------------------------------------------------------------

//------------------------------------------------------------------
ReflowCounter::ReflowCounter(ReflowCountMgr* aMgr) : mMgr(aMgr) {
  ClearTotals();
  SetTotalsCache();
}

//------------------------------------------------------------------
ReflowCounter::~ReflowCounter() = default;

//------------------------------------------------------------------
void ReflowCounter::ClearTotals() { mTotal = 0; }

//------------------------------------------------------------------
void ReflowCounter::SetTotalsCache() { mCacheTotal = mTotal; }

//------------------------------------------------------------------
void ReflowCounter::CalcDiffInTotals() { mCacheTotal = mTotal - mCacheTotal; }

//------------------------------------------------------------------
void ReflowCounter::DisplayTotals(const char* aStr) {
  DisplayTotals(mTotal, aStr ? aStr : "Totals");
}

//------------------------------------------------------------------
void ReflowCounter::DisplayDiffTotals(const char* aStr) {
  DisplayTotals(mCacheTotal, aStr ? aStr : "Diff Totals");
}

//------------------------------------------------------------------
void ReflowCounter::DisplayHTMLTotals(const char* aStr) {
  DisplayHTMLTotals(mTotal, aStr ? aStr : "Totals");
}

//------------------------------------------------------------------
void ReflowCounter::DisplayTotals(uint32_t aTotal, const char* aTitle) {
  // figure total
  if (aTotal == 0) {
    return;
  }
  ReflowCounter* gTots = (ReflowCounter*)mMgr->LookUp(kGrandTotalsStr);

  printf("%25s\t", aTitle);
  printf("%d\t", aTotal);
  if (gTots != this && aTotal > 0) {
    gTots->Add(aTotal);
  }
}

//------------------------------------------------------------------
void ReflowCounter::DisplayHTMLTotals(uint32_t aTotal, const char* aTitle) {
  if (aTotal == 0) {
    return;
  }

  ReflowCounter* gTots = (ReflowCounter*)mMgr->LookUp(kGrandTotalsStr);
  FILE* fd = mMgr->GetOutFile();
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

#  define KEY_BUF_SIZE_FOR_PTR \
    24  // adequate char[] buffer to sprintf a pointer

ReflowCountMgr::ReflowCountMgr() : mCounts(10), mIndiFrameCounts(10) {
  mCycledOnce = false;
  mDumpFrameCounts = false;
  mDumpFrameByFrameCounts = false;
  mPaintFrameByFrameCounts = false;
}

//------------------------------------------------------------------
ReflowCountMgr::~ReflowCountMgr() = default;

//------------------------------------------------------------------
ReflowCounter* ReflowCountMgr::LookUp(const char* aName) {
  return mCounts.Get(aName);
}

//------------------------------------------------------------------
void ReflowCountMgr::Add(const char* aName, nsIFrame* aFrame) {
  NS_ASSERTION(aName != nullptr, "Name shouldn't be null!");

  if (mDumpFrameCounts) {
    const auto& counter = mCounts.LookupForAdd(aName).OrInsert(
        [this]() { return new ReflowCounter(this); });
    counter->Add();
  }

  if ((mDumpFrameByFrameCounts || mPaintFrameByFrameCounts) &&
      aFrame != nullptr) {
    char key[KEY_BUF_SIZE_FOR_PTR];
    SprintfLiteral(key, "%p", (void*)aFrame);
    const auto& counter =
        mIndiFrameCounts.LookupForAdd(key).OrInsert([&aName, &aFrame, this]() {
          auto counter = new IndiReflowCounter(this);
          counter->mFrame = aFrame;
          counter->mName.AssignASCII(aName);
          return counter;
        });
    // this eliminates extra counts from super classes
    if (counter && counter->mName.EqualsASCII(aName)) {
      counter->mCount++;
      counter->mCounter.Add(1);
    }
  }
}

//------------------------------------------------------------------
void ReflowCountMgr::PaintCount(const char* aName,
                                gfxContext* aRenderingContext,
                                nsPresContext* aPresContext, nsIFrame* aFrame,
                                const nsPoint& aOffset, uint32_t aColor) {
  if (mPaintFrameByFrameCounts && aFrame != nullptr) {
    char key[KEY_BUF_SIZE_FOR_PTR];
    SprintfLiteral(key, "%p", (void*)aFrame);
    IndiReflowCounter* counter = mIndiFrameCounts.Get(key);
    if (counter != nullptr && counter->mName.EqualsASCII(aName)) {
      DrawTarget* drawTarget = aRenderingContext->GetDrawTarget();
      int32_t appUnitsPerDevPixel = aPresContext->AppUnitsPerDevPixel();

      aRenderingContext->Save();
      gfxPoint devPixelOffset =
          nsLayoutUtils::PointToGfxPoint(aOffset, appUnitsPerDevPixel);
      aRenderingContext->SetMatrixDouble(
          aRenderingContext->CurrentMatrixDouble().PreTranslate(
              devPixelOffset));

      // We don't care about the document language or user fonts here;
      // just get a default Latin font.
      nsFont font(StyleGenericFontFamily::Serif,
                  nsPresContext::CSSPixelsToAppUnits(11));
      nsFontMetrics::Params params;
      params.language = nsGkAtoms::x_western;
      params.textPerf = aPresContext->GetTextPerfMetrics();
      params.fontStats = nullptr;  // not interested here
      params.featureValueLookup = aPresContext->GetFontFeatureValuesLookup();
      RefPtr<nsFontMetrics> fm =
          aPresContext->DeviceContext()->GetMetricsFor(font, params);

      char buf[16];
      int len = SprintfLiteral(buf, "%d", counter->mCount);
      nscoord x = 0, y = fm->MaxAscent();
      nscoord width, height = fm->MaxHeight();
      fm->SetTextRunRTL(false);
      width = fm->GetWidth(buf, len, drawTarget);

      sRGBColor color;
      sRGBColor color2;
      if (aColor != 0) {
        color = sRGBColor::FromABGR(aColor);
        color2 = sRGBColor(0.f, 0.f, 0.f);
      } else {
        gfx::Float rc = 0.f, gc = 0.f, bc = 0.f;
        if (counter->mCount < 5) {
          rc = 1.f;
          gc = 1.f;
        } else if (counter->mCount < 11) {
          gc = 1.f;
        } else {
          rc = 1.f;
        }
        color = sRGBColor(rc, gc, bc);
        color2 = sRGBColor(rc / 2, gc / 2, bc / 2);
      }

      nsRect rect(0, 0, width + 15, height + 15);
      Rect devPxRect =
          NSRectToSnappedRect(rect, appUnitsPerDevPixel, *drawTarget);
      ColorPattern black(ToDeviceColor(sRGBColor::OpaqueBlack()));
      drawTarget->FillRect(devPxRect, black);

      aRenderingContext->SetColor(color2);
      fm->DrawString(buf, len, x + 15, y + 15, aRenderingContext);
      aRenderingContext->SetColor(color);
      fm->DrawString(buf, len, x, y, aRenderingContext);

      aRenderingContext->Restore();
    }
  }
}

//------------------------------------------------------------------
void ReflowCountMgr::DoGrandTotals() {
  auto entry = mCounts.LookupForAdd(kGrandTotalsStr);
  if (!entry) {
    entry.OrInsert([this]() { return new ReflowCounter(this); });
  } else {
    entry.Data()->ClearTotals();
  }

  printf("\t\t\t\tTotal\n");
  for (uint32_t i = 0; i < 78; i++) {
    printf("-");
  }
  printf("\n");
  for (auto iter = mCounts.Iter(); !iter.Done(); iter.Next()) {
    iter.Data()->DisplayTotals(iter.Key());
  }
}

static void RecurseIndiTotals(
    nsPresContext* aPresContext,
    nsClassHashtable<nsCharPtrHashKey, IndiReflowCounter>& aHT,
    nsIFrame* aParentFrame, int32_t aLevel) {
  if (aParentFrame == nullptr) {
    return;
  }

  char key[KEY_BUF_SIZE_FOR_PTR];
  SprintfLiteral(key, "%p", (void*)aParentFrame);
  IndiReflowCounter* counter = aHT.Get(key);
  if (counter) {
    counter->mHasBeenOutput = true;
    char* name = ToNewCString(counter->mName);
    for (int32_t i = 0; i < aLevel; i++) printf(" ");
    printf("%s - %p   [%d][", name, (void*)aParentFrame, counter->mCount);
    printf("%d", counter->mCounter.GetTotal());
    printf("]\n");
    free(name);
  }

  for (nsIFrame* child : aParentFrame->PrincipalChildList()) {
    RecurseIndiTotals(aPresContext, aHT, child, aLevel + 1);
  }
}

//------------------------------------------------------------------
void ReflowCountMgr::DoIndiTotalsTree() {
  printf("\n------------------------------------------------\n");
  printf("-- Individual Frame Counts\n");
  printf("------------------------------------------------\n");

  if (mPresShell) {
    nsIFrame* rootFrame = mPresShell->GetRootFrame();
    RecurseIndiTotals(mPresContext, mIndiFrameCounts, rootFrame, 0);
    printf("------------------------------------------------\n");
    printf("-- Individual Counts of Frames not in Root Tree\n");
    printf("------------------------------------------------\n");
    for (auto iter = mIndiFrameCounts.Iter(); !iter.Done(); iter.Next()) {
      IndiReflowCounter* counter = iter.UserData();
      if (!counter->mHasBeenOutput) {
        char* name = ToNewCString(counter->mName);
        printf("%s - %p   [%d][", name, (void*)counter->mFrame,
               counter->mCount);
        printf("%d", counter->mCounter.GetTotal());
        printf("]\n");
        free(name);
      }
    }
  }
}

//------------------------------------------------------------------
void ReflowCountMgr::DoGrandHTMLTotals() {
  auto entry = mCounts.LookupForAdd(kGrandTotalsStr);
  if (!entry) {
    entry.OrInsert([this]() { return new ReflowCounter(this); });
  } else {
    entry.Data()->ClearTotals();
  }

  static const char* title[] = {"Class", "Reflows"};
  fprintf(mFD, "<tr>");
  for (uint32_t i = 0; i < ArrayLength(title); i++) {
    fprintf(mFD, "<td><center><b>%s<b></center></td>", title[i]);
  }
  fprintf(mFD, "</tr>\n");

  for (auto iter = mCounts.Iter(); !iter.Done(); iter.Next()) {
    iter.Data()->DisplayHTMLTotals(iter.Key());
  }
}

//------------------------------------
void ReflowCountMgr::DisplayTotals(const char* aStr) {
#  ifdef DEBUG_rods
  printf("%s\n", aStr ? aStr : "No name");
#  endif
  if (mDumpFrameCounts) {
    DoGrandTotals();
  }
  if (mDumpFrameByFrameCounts) {
    DoIndiTotalsTree();
  }
}
//------------------------------------
void ReflowCountMgr::DisplayHTMLTotals(const char* aStr) {
#  ifdef WIN32x  // XXX NOT XP!
  char name[1024];

  char* sptr = strrchr(aStr, '/');
  if (sptr) {
    sptr++;
    strcpy(name, sptr);
    char* eptr = strrchr(name, '.');
    if (eptr) {
      *eptr = 0;
    }
    strcat(name, "_stats.html");
  }
  mFD = fopen(name, "w");
  if (mFD) {
    fprintf(mFD, "<html><head><title>Reflow Stats</title></head><body>\n");
    const char* title = aStr ? aStr : "No name";
    fprintf(mFD,
            "<center><b>%s</b><br><table border=1 "
            "style=\"background-color:#e0e0e0\">",
            title);
    DoGrandHTMLTotals();
    fprintf(mFD, "</center></table>\n");
    fprintf(mFD, "</body></html>\n");
    fclose(mFD);
    mFD = nullptr;
  }
#  endif  // not XP!
}

//------------------------------------------------------------------
void ReflowCountMgr::ClearTotals() {
  for (auto iter = mCounts.Iter(); !iter.Done(); iter.Next()) {
    iter.Data()->ClearTotals();
  }
}

//------------------------------------------------------------------
void ReflowCountMgr::ClearGrandTotals() {
  auto entry = mCounts.LookupForAdd(kGrandTotalsStr);
  if (!entry) {
    entry.OrInsert([this]() { return new ReflowCounter(this); });
  } else {
    entry.Data()->ClearTotals();
    entry.Data()->SetTotalsCache();
  }
}

//------------------------------------------------------------------
void ReflowCountMgr::DisplayDiffsInTotals() {
  if (mCycledOnce) {
    printf("Differences\n");
    for (int32_t i = 0; i < 78; i++) {
      printf("-");
    }
    printf("\n");
    ClearGrandTotals();
  }

  for (auto iter = mCounts.Iter(); !iter.Done(); iter.Next()) {
    if (mCycledOnce) {
      iter.Data()->CalcDiffInTotals();
      iter.Data()->DisplayDiffTotals(iter.Key());
    }
    iter.Data()->SetTotalsCache();
  }

  mCycledOnce = true;
}

#endif  // MOZ_REFLOW_PERF

nsIFrame* PresShell::GetAbsoluteContainingBlock(nsIFrame* aFrame) {
  return FrameConstructor()->GetAbsoluteContainingBlock(
      aFrame, nsCSSFrameConstructor::ABS_POS);
}

#ifdef ACCESSIBILITY

// static
bool PresShell::IsAccessibilityActive() { return GetAccService() != nullptr; }

// static
nsAccessibilityService* PresShell::GetAccessibilityService() {
  return GetAccService();
}

#endif  // #ifdef ACCESSIBILITY

// Asks our docshell whether we're active.
void PresShell::QueryIsActive() {
  nsCOMPtr<nsISupports> container = mPresContext->GetContainerWeak();
  if (mDocument) {
    Document* displayDoc = mDocument->GetDisplayDocument();
    if (displayDoc) {
      // Ok, we're an external resource document -- we need to use our display
      // document's docshell to determine "IsActive" status, since we lack
      // a container.
      MOZ_ASSERT(!container,
                 "external resource doc shouldn't have its own container");

      nsPresContext* displayPresContext = displayDoc->GetPresContext();
      if (displayPresContext) {
        container = displayPresContext->GetContainerWeak();
      }
    }
  }

  nsCOMPtr<nsIDocShell> docshell(do_QueryInterface(container));
  if (docshell) {
    bool isActive;
    nsresult rv = docshell->GetIsActive(&isActive);
    // Even though in theory the docshell here could be "Inactive and
    // Foreground", thus implying aIsHidden=false for SetIsActive(),
    // this is a newly created PresShell so we'd like to invalidate anyway
    // upon being made active to ensure that the contents get painted.
    if (NS_SUCCEEDED(rv)) SetIsActive(isActive);
  }
}

static void SetPluginIsActive(nsISupports* aSupports, bool aIsActive) {
  nsCOMPtr<nsIContent> content(do_QueryInterface(aSupports));
  if (!content) {
    return;
  }

  if (nsIObjectFrame* objectFrame = do_QueryFrame(content->GetPrimaryFrame())) {
    objectFrame->SetIsDocumentActive(aIsActive);
  }
}

nsresult PresShell::SetIsActive(bool aIsActive) {
  MOZ_ASSERT(mDocument, "should only be called with a document");

#if defined(MOZ_WIDGET_ANDROID)
  const bool changed = mIsActive != aIsActive;
#endif

  mIsActive = aIsActive;

  nsPresContext* presContext = GetPresContext();
  if (presContext &&
      presContext->RefreshDriver()->GetPresContext() == presContext) {
    presContext->RefreshDriver()->SetThrottled(!mIsActive);
  }

  {
    // Propagate state-change to my resource documents' PresShells
    auto recurse = [aIsActive](Document& aResourceDoc) {
      if (PresShell* presShell = aResourceDoc.GetPresShell()) {
        presShell->SetIsActive(aIsActive);
      }
      return CallState::Continue;
    };
    mDocument->EnumerateExternalResources(recurse);
  }
  {
    auto visitPlugin = [aIsActive](nsISupports* aSupports) {
      SetPluginIsActive(aSupports, aIsActive);
    };
    mDocument->EnumerateActivityObservers(visitPlugin);
  }
  nsresult rv = UpdateImageLockingState();
#ifdef ACCESSIBILITY
  if (aIsActive) {
    if (nsAccessibilityService* accService =
            PresShell::GetAccessibilityService()) {
      accService->PresShellActivated(this);
    }
  }
#endif  // #ifdef ACCESSIBILITY

#if defined(MOZ_WIDGET_ANDROID)
  if (changed && !aIsActive && presContext &&
      presContext->IsRootContentDocumentCrossProcess()) {
    if (BrowserChild* browserChild = BrowserChild::GetFrom(this)) {
      // Reset the dynamic toolbar offset state.
      presContext->UpdateDynamicToolbarOffset(0);
    }
  }
#endif

  return rv;
}

RefPtr<MobileViewportManager> PresShell::GetMobileViewportManager() const {
  return mMobileViewportManager;
}

bool UseMobileViewportManager(PresShell* aPresShell, Document* aDocument) {
  // If we're not using APZ, we won't be able to zoom, so there is no
  // point in having an MVM.
  if (nsPresContext* presContext = aPresShell->GetPresContext()) {
    if (nsIWidget* widget = presContext->GetNearestWidget()) {
      if (!widget->AsyncPanZoomEnabled()) {
        return false;
      }
    }
  }
  return nsLayoutUtils::ShouldHandleMetaViewport(aDocument) ||
         nsLayoutUtils::AllowZoomingForDocument(aDocument);
}

void PresShell::UpdateViewportOverridden(bool aAfterInitialization) {
  // Determine if we require a MobileViewportManager. We need one any
  // time we allow resolution zooming for a document, and any time we
  // want to obey <meta name="viewport"> tags for it.
  bool needMVM = UseMobileViewportManager(this, mDocument);

  if (needMVM == !!mMobileViewportManager) {
    // Either we've need one and we've already got it, or we don't need one
    // and don't have it. Either way, we're done.
    return;
  }

  if (needMVM) {
    if (mPresContext->IsRootContentDocumentCrossProcess()) {
      // Store the resolution so we can restore to this resolution when
      // the MVM is destroyed.
      mDocument->SetSavedResolutionBeforeMVM(mResolution.valueOr(1.0f));

      mMVMContext = new GeckoMVMContext(mDocument, this);
      mMobileViewportManager = new MobileViewportManager(mMVMContext);

      if (aAfterInitialization) {
        // Setting the initial viewport will trigger a reflow.
        mMobileViewportManager->SetInitialViewport();
      }
    }
    return;
  }

  MOZ_ASSERT(mMobileViewportManager,
             "Shouldn't reach this without a MobileViewportManager.");

  mMobileViewportManager->Destroy();
  mMobileViewportManager = nullptr;
  mMVMContext = nullptr;

  ResetVisualViewportSize();

  // After we clear out the MVM and the MVMContext, also reset the
  // resolution to its pre-MVM value.
  SetResolutionAndScaleTo(mDocument->GetSavedResolutionBeforeMVM(),
                          ResolutionChangeOrigin::MainThreadRestore);

  if (aAfterInitialization) {
    // Force a reflow to our correct size by going back to the docShell
    // and asking it to reassert its size. This is necessary because
    // everything underneath the docShell, like the ViewManager, has been
    // altered by the MobileViewportManager in an irreversible way.
    nsDocShell* docShell =
        static_cast<nsDocShell*>(GetPresContext()->GetDocShell());
    int32_t width, height;
    docShell->GetSize(&width, &height);
    docShell->SetSize(width, height, false);
  }
}

bool PresShell::UsesMobileViewportSizing() const {
  return GetIsViewportOverridden() &&
         nsLayoutUtils::ShouldHandleMetaViewport(mDocument);
}

/*
 * Determines the current image locking state. Called when one of the
 * dependent factors changes.
 */
nsresult PresShell::UpdateImageLockingState() {
  // We're locked if we're both thawed and active.
  bool locked = !mFrozen && mIsActive;

  nsresult rv = mDocument->ImageTracker()->SetLockingState(locked);

  if (locked) {
    // Request decodes for visible image frames; we want to start decoding as
    // quickly as possible when we get foregrounded to minimize flashing.
    for (auto iter = mApproximatelyVisibleFrames.Iter(); !iter.Done();
         iter.Next()) {
      nsImageFrame* imageFrame = do_QueryFrame(iter.Get()->GetKey());
      if (imageFrame) {
        imageFrame->MaybeDecodeForPredictedSize();
      }
    }
  }

  return rv;
}

PresShell* PresShell::GetRootPresShell() const {
  if (mPresContext) {
    nsPresContext* rootPresContext = mPresContext->GetRootPresContext();
    if (rootPresContext) {
      return rootPresContext->PresShell();
    }
  }
  return nullptr;
}

void PresShell::AddSizeOfIncludingThis(nsWindowSizes& aSizes) const {
  MallocSizeOf mallocSizeOf = aSizes.mState.mMallocSizeOf;
  mFrameArena.AddSizeOfExcludingThis(aSizes, Arena::ArenaKind::PresShell);
  aSizes.mLayoutPresShellSize += mallocSizeOf(this);
  if (mCaret) {
    aSizes.mLayoutPresShellSize += mCaret->SizeOfIncludingThis(mallocSizeOf);
  }
  aSizes.mLayoutPresShellSize +=
      mApproximatelyVisibleFrames.ShallowSizeOfExcludingThis(mallocSizeOf) +
      mFramesToDirty.ShallowSizeOfExcludingThis(mallocSizeOf) +
      mPendingScrollAnchorSelection.ShallowSizeOfExcludingThis(mallocSizeOf) +
      mPendingScrollAnchorAdjustment.ShallowSizeOfExcludingThis(mallocSizeOf);

  aSizes.mLayoutTextRunsSize += SizeOfTextRuns(mallocSizeOf);

  aSizes.mLayoutPresContextSize +=
      mPresContext->SizeOfIncludingThis(mallocSizeOf);

  mFrameConstructor->AddSizeOfIncludingThis(aSizes);
}

size_t PresShell::SizeOfTextRuns(MallocSizeOf aMallocSizeOf) const {
  nsIFrame* rootFrame = mFrameConstructor->GetRootFrame();
  if (!rootFrame) {
    return 0;
  }

  // clear the TEXT_RUN_MEMORY_ACCOUNTED flags
  nsLayoutUtils::SizeOfTextRunsForFrames(rootFrame, nullptr,
                                         /* clear = */ true);

  // collect the total memory in use for textruns
  return nsLayoutUtils::SizeOfTextRunsForFrames(rootFrame, aMallocSizeOf,
                                                /* clear = */ false);
}

void PresShell::MarkFixedFramesForReflow(IntrinsicDirty aIntrinsicDirty) {
  nsIFrame* rootFrame = mFrameConstructor->GetRootFrame();
  if (rootFrame) {
    const nsFrameList& childList =
        rootFrame->GetChildList(nsIFrame::kFixedList);
    for (nsIFrame* childFrame : childList) {
      FrameNeedsReflow(childFrame, aIntrinsicDirty, NS_FRAME_IS_DIRTY);
    }
  }
}

void PresShell::CompleteChangeToVisualViewportSize() {
  if (nsIScrollableFrame* rootScrollFrame = GetRootScrollFrameAsScrollable()) {
    rootScrollFrame->MarkScrollbarsDirtyForReflow();
  }
  MarkFixedFramesForReflow(IntrinsicDirty::Resize);

  if (auto* window = nsGlobalWindowInner::Cast(mDocument->GetInnerWindow())) {
    window->VisualViewport()->PostResizeEvent();
  }

  if (nsIScrollableFrame* rootScrollFrame = GetRootScrollFrameAsScrollable()) {
    ScrollAnchorContainer* container = rootScrollFrame->Anchor();
    container->UserScrolled();
  }
}

void PresShell::SetVisualViewportSize(nscoord aWidth, nscoord aHeight) {
  if (!mVisualViewportSizeSet || mVisualViewportSize.width != aWidth ||
      mVisualViewportSize.height != aHeight) {
    mVisualViewportSizeSet = true;
    mVisualViewportSize.width = aWidth;
    mVisualViewportSize.height = aHeight;

    CompleteChangeToVisualViewportSize();
  }
}

void PresShell::ResetVisualViewportSize() {
  if (mVisualViewportSizeSet) {
    mVisualViewportSizeSet = false;
    mVisualViewportSize.width = 0;
    mVisualViewportSize.height = 0;

    CompleteChangeToVisualViewportSize();
  }
}

bool PresShell::SetVisualViewportOffset(const nsPoint& aScrollOffset,
                                        const nsPoint& aPrevLayoutScrollPos) {
  nsPoint prevOffset = GetVisualViewportOffset();
  if (prevOffset == aScrollOffset) {
    return false;
  }

  mVisualViewportOffset = Some(aScrollOffset);

  if (auto* window = nsGlobalWindowInner::Cast(mDocument->GetInnerWindow())) {
    window->VisualViewport()->PostScrollEvent(prevOffset, aPrevLayoutScrollPos);
  }

  if (IsVisualViewportSizeSet()) {
    if (nsIScrollableFrame* rootScrollFrame =
            GetRootScrollFrameAsScrollable()) {
      rootScrollFrame->Anchor()->UserScrolled();
    }
  }

  return true;
}

void PresShell::ScrollToVisual(const nsPoint& aVisualViewportOffset,
                               FrameMetrics::ScrollOffsetUpdateType aUpdateType,
                               ScrollMode aMode) {
  MOZ_ASSERT(aMode == ScrollMode::Instant || aMode == ScrollMode::SmoothMsd);

  if (aMode == ScrollMode::SmoothMsd) {
    if (nsIScrollableFrame* sf = GetRootScrollFrameAsScrollable()) {
      if (sf->SmoothScrollVisual(aVisualViewportOffset, aUpdateType)) {
        return;
      }
    }
  }

  // If the caller asked for instant scroll, or if we failed
  // to do a smooth scroll, do an instant scroll.
  SetPendingVisualScrollUpdate(aVisualViewportOffset, aUpdateType);
}

void PresShell::SetPendingVisualScrollUpdate(
    const nsPoint& aVisualViewportOffset,
    FrameMetrics::ScrollOffsetUpdateType aUpdateType) {
  mPendingVisualScrollUpdate =
      Some(VisualScrollUpdate{aVisualViewportOffset, aUpdateType});

  // The pending update is picked up during the next paint.
  // Schedule a paint to make sure one will happen.
  if (nsIFrame* rootFrame = GetRootFrame()) {
    rootFrame->SchedulePaint();
  }
}

void PresShell::ClearPendingVisualScrollUpdate() {
  if (mPendingVisualScrollUpdate && mPendingVisualScrollUpdate->mAcknowledged) {
    mPendingVisualScrollUpdate = mozilla::Nothing();
  }
}

void PresShell::AcknowledgePendingVisualScrollUpdate() {
  MOZ_ASSERT(mPendingVisualScrollUpdate);
  mPendingVisualScrollUpdate->mAcknowledged = true;
}

nsPoint PresShell::GetVisualViewportOffsetRelativeToLayoutViewport() const {
  return GetVisualViewportOffset() - GetLayoutViewportOffset();
}

nsPoint PresShell::GetLayoutViewportOffset() const {
  nsPoint result;
  if (nsIScrollableFrame* sf = GetRootScrollFrameAsScrollable()) {
    result = sf->GetScrollPosition();
  }
  return result;
}

nsSize PresShell::GetLayoutViewportSize() const {
  nsSize result;
  if (nsIScrollableFrame* sf = GetRootScrollFrameAsScrollable()) {
    result = sf->GetScrollPortRect().Size();
  }
  return result;
}

nsSize PresShell::GetVisualViewportSizeUpdatedByDynamicToolbar() const {
  NS_ASSERTION(mVisualViewportSizeSet,
               "asking for visual viewport size when its not set?");
  if (!mMobileViewportManager) {
    return mVisualViewportSize;
  }

  MOZ_ASSERT(GetDynamicToolbarState() == DynamicToolbarState::InTransition ||
             GetDynamicToolbarState() == DynamicToolbarState::Collapsed);

  return mMobileViewportManager->GetVisualViewportSizeUpdatedByDynamicToolbar();
}

void PresShell::RecomputeFontSizeInflationEnabled() {
  mFontSizeInflationEnabled = DetermineFontSizeInflationState();

  // Divide by 100 to convert the pref from a percentage to a fraction.
  float fontScale = StaticPrefs::font_size_systemFontScale() / 100.0f;
  if (fontScale == 0.0f) {
    return;
  }

  MOZ_ASSERT(mDocument);
  MOZ_ASSERT(mPresContext);
  if (mFontSizeInflationEnabled || mDocument->IsSyntheticDocument()) {
    mPresContext->SetSystemFontScale(1.0f);
  } else {
    mPresContext->SetSystemFontScale(fontScale);
  }
}

bool PresShell::DetermineFontSizeInflationState() {
  MOZ_ASSERT(mPresContext, "our pres context should not be null");
  if (mPresContext->IsChrome()) {
    return false;
  }

  if (FontSizeInflationEmPerLine() == 0 && FontSizeInflationMinTwips() == 0) {
    return false;
  }

  // Force-enabling font inflation always trumps the heuristics here.
  if (!FontSizeInflationForceEnabled()) {
    if (BrowserChild* tab = BrowserChild::GetFrom(this)) {
      // We're in a child process.  Cancel inflation if we're not
      // async-pan zoomed.
      if (!tab->AsyncPanZoomEnabled()) {
        return false;
      }
    } else if (XRE_IsParentProcess()) {
      // We're in the master process.  Cancel inflation if it's been
      // explicitly disabled.
      if (FontSizeInflationDisabledInMasterProcess()) {
        return false;
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
    return false;
  }

  nsCOMPtr<nsIScreen> screen;
  screenMgr->GetPrimaryScreen(getter_AddRefs(screen));
  if (screen) {
    int32_t screenLeft, screenTop, screenWidth, screenHeight;
    screen->GetRect(&screenLeft, &screenTop, &screenWidth, &screenHeight);

    nsViewportInfo vInf = GetDocument()->GetViewportInfo(
        ScreenIntSize(screenWidth, screenHeight));

    if (vInf.GetDefaultZoom() >= CSSToScreenScale(1.0f) ||
        vInf.IsAutoSizeEnabled()) {
      return false;
    }
  }

  return true;
}

void PresShell::PausePainting() {
  if (GetPresContext()->RefreshDriver()->GetPresContext() != GetPresContext())
    return;

  mPaintingIsFrozen = true;
  GetPresContext()->RefreshDriver()->Freeze();
}

void PresShell::ResumePainting() {
  if (GetPresContext()->RefreshDriver()->GetPresContext() != GetPresContext())
    return;

  mPaintingIsFrozen = false;
  GetPresContext()->RefreshDriver()->Thaw();
}

void PresShell::SyncWindowProperties(nsView* aView) {
  nsIFrame* frame = aView->GetFrame();
  if (frame && mPresContext) {
    // CreateReferenceRenderingContext can return nullptr
    RefPtr<gfxContext> rcx(CreateReferenceRenderingContext());
    nsContainerFrame::SyncWindowProperties(mPresContext, frame, aView, rcx, 0);
  }
}

nsresult PresShell::HasRuleProcessorUsedByMultipleStyleSets(uint32_t aSheetType,
                                                            bool* aRetVal) {
  *aRetVal = false;
  return NS_OK;
}

void PresShell::NotifyStyleSheetServiceSheetAdded(StyleSheet* aSheet,
                                                  uint32_t aSheetType) {
  switch (aSheetType) {
    case nsIStyleSheetService::AGENT_SHEET:
      AddAgentSheet(aSheet);
      break;
    case nsIStyleSheetService::USER_SHEET:
      AddUserSheet(aSheet);
      break;
    case nsIStyleSheetService::AUTHOR_SHEET:
      AddAuthorSheet(aSheet);
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("unexpected aSheetType value");
      break;
  }
}

void PresShell::NotifyStyleSheetServiceSheetRemoved(StyleSheet* aSheet,
                                                    uint32_t aSheetType) {
  StyleSet()->RemoveStyleSheet(*aSheet);
  mDocument->ApplicableStylesChanged();
}

void PresShell::SetIsUnderHiddenEmbedderElement(
    bool aUnderHiddenEmbedderElement) {
  if (mUnderHiddenEmbedderElement == aUnderHiddenEmbedderElement) {
    return;
  }

  mUnderHiddenEmbedderElement = aUnderHiddenEmbedderElement;

  if (nsCOMPtr<nsIDocShell> docShell = mPresContext->GetDocShell()) {
    BrowsingContext* bc = docShell->GetBrowsingContext();

    // Propagate to children.
    for (BrowsingContext* child : bc->Children()) {
      Element* embedderElement = child->GetEmbedderElement();
      if (!embedderElement) {
        // TODO: We shouldn't need to null check here since `child` and the
        // element returned by `child->GetEmbedderElement()` are in our
        // process (the actual browsing context represented by `child` may not
        // be, but that doesn't matter).  However, there are currently a very
        // small number of crashes due to `embedderElement` being null, somehow
        // - see bug 1551241.  For now we wallpaper the crash.
        continue;
      }

      bool embedderFrameIsHidden = true;
      if (auto embedderFrame = embedderElement->GetPrimaryFrame()) {
        embedderFrameIsHidden = !embedderFrame->StyleVisibility()->IsVisible();
      }

      if (nsIDocShell* childDocShell = child->GetDocShell()) {
        PresShell* presShell = childDocShell->GetPresShell();
        if (!presShell) {
          continue;
        }
        presShell->SetIsUnderHiddenEmbedderElement(
            aUnderHiddenEmbedderElement || embedderFrameIsHidden);
      } else {
        BrowserBridgeChild* bridgeChild =
            BrowserBridgeChild::GetFrom(embedderElement);
        bridgeChild->SetIsUnderHiddenEmbedderElement(
            aUnderHiddenEmbedderElement || embedderFrameIsHidden);
      }
    }
  }
}

nsIContent* PresShell::EventHandler::GetOverrideClickTarget(
    WidgetGUIEvent* aGUIEvent, nsIFrame* aFrame) {
  if (aGUIEvent->mMessage != eMouseUp) {
    return nullptr;
  }

  MOZ_ASSERT(aGUIEvent->mClass == eMouseEventClass);
  WidgetMouseEvent* mouseEvent = aGUIEvent->AsMouseEvent();

  uint32_t flags = 0;
  RelativeTo relativeTo{aFrame};
  nsPoint eventPoint =
      nsLayoutUtils::GetEventCoordinatesRelativeTo(aGUIEvent, relativeTo);
  if (mouseEvent->mIgnoreRootScrollFrame) {
    flags |= INPUT_IGNORE_ROOT_SCROLL_FRAME;
  }

  nsIFrame* target =
      FindFrameTargetedByInputEvent(aGUIEvent, relativeTo, eventPoint, flags);
  if (!target) {
    return nullptr;
  }

  nsIContent* overrideClickTarget = target->GetContent();
  while (overrideClickTarget && !overrideClickTarget->IsElement()) {
    overrideClickTarget = overrideClickTarget->GetFlattenedTreeParent();
  }
  return overrideClickTarget;
}

/******************************************************************************
 * PresShell::EventHandler::EventTargetData
 ******************************************************************************/

void PresShell::EventHandler::EventTargetData::SetFrameAndComputePresShell(
    nsIFrame* aFrameToHandleEvent) {
  if (aFrameToHandleEvent) {
    mFrame = aFrameToHandleEvent;
    mPresShell = aFrameToHandleEvent->PresShell();
  } else {
    mFrame = nullptr;
    mPresShell = nullptr;
  }
}

void PresShell::EventHandler::EventTargetData::
    SetFrameAndComputePresShellAndContent(nsIFrame* aFrameToHandleEvent,
                                          WidgetGUIEvent* aGUIEvent) {
  MOZ_ASSERT(aFrameToHandleEvent);
  MOZ_ASSERT(aGUIEvent);

  SetFrameAndComputePresShell(aFrameToHandleEvent);
  SetContentForEventFromFrame(aGUIEvent);
}

void PresShell::EventHandler::EventTargetData::SetContentForEventFromFrame(
    WidgetGUIEvent* aGUIEvent) {
  MOZ_ASSERT(mFrame);
  mContent = nullptr;
  mFrame->GetContentForEvent(aGUIEvent, getter_AddRefs(mContent));
}

nsIContent* PresShell::EventHandler::EventTargetData::GetFrameContent() const {
  return mFrame ? mFrame->GetContent() : nullptr;
}

bool PresShell::EventHandler::EventTargetData::MaybeRetargetToActiveDocument(
    WidgetGUIEvent* aGUIEvent) {
  MOZ_ASSERT(aGUIEvent);
  MOZ_ASSERT(mFrame);
  MOZ_ASSERT(mPresShell);
  MOZ_ASSERT(!mContent, "Doesn't support to retarget the content");

  EventStateManager* activeESM =
      EventStateManager::GetActiveEventStateManager();
  if (!activeESM) {
    return false;
  }

  if (aGUIEvent->mClass != ePointerEventClass &&
      !aGUIEvent->HasMouseEventMessage()) {
    return false;
  }

  if (activeESM == GetEventStateManager()) {
    return false;
  }

  nsPresContext* activePresContext = activeESM->GetPresContext();
  if (!activePresContext) {
    return false;
  }

  PresShell* activePresShell = activePresContext->GetPresShell();
  if (!activePresShell) {
    return false;
  }

  // Note, currently for backwards compatibility we don't forward mouse events
  // to the active document when mouse is over some subdocument.
  if (!nsContentUtils::ContentIsCrossDocDescendantOf(
          activePresShell->GetDocument(), GetDocument())) {
    return false;
  }

  SetFrameAndComputePresShell(activePresShell->GetRootFrame());
  return true;
}

bool PresShell::EventHandler::EventTargetData::ComputeElementFromFrame(
    WidgetGUIEvent* aGUIEvent) {
  MOZ_ASSERT(aGUIEvent);
  MOZ_ASSERT(aGUIEvent->IsUsingCoordinates());
  MOZ_ASSERT(mPresShell);
  MOZ_ASSERT(mFrame);

  SetContentForEventFromFrame(aGUIEvent);

  // If there is no content for this frame, target it anyway.  Some frames can
  // be targeted but do not have content, particularly windows with scrolling
  // off.
  if (!mContent) {
    return true;
  }

  // Bug 103055, bug 185889: mouse events apply to *elements*, not all nodes.
  // Thus we get the nearest element parent here.
  // XXX we leave the frame the same even if we find an element parent, so that
  // the text frame will receive the event (selection and friends are the ones
  // who care about that anyway)
  //
  // We use weak pointers because during this tight loop, the node
  // will *not* go away.  And this happens on every mousemove.
  nsIContent* content = mContent;
  while (content && !content->IsElement()) {
    content = content->GetFlattenedTreeParent();
  }
  mContent = content;

  // If we found an element, target it.  Otherwise, target *nothing*.
  return !!mContent;
}

void PresShell::EventHandler::EventTargetData::UpdateTouchEventTarget(
    WidgetGUIEvent* aGUIEvent) {
  MOZ_ASSERT(aGUIEvent);

  if (aGUIEvent->mClass != eTouchEventClass) {
    return;
  }

  if (aGUIEvent->mMessage == eTouchStart) {
    WidgetTouchEvent* touchEvent = aGUIEvent->AsTouchEvent();
    nsIFrame* newFrame =
        TouchManager::SuppressInvalidPointsAndGetTargetedFrame(touchEvent);
    if (!newFrame) {
      return;  // XXX Why don't we stop handling the event in this case?
    }
    SetFrameAndComputePresShellAndContent(newFrame, aGUIEvent);
    return;
  }

  PresShell* newPresShell = PresShell::GetShellForTouchEvent(aGUIEvent);
  if (!newPresShell) {
    return;  // XXX Why don't we stop handling the event in this case?
  }

  // Touch events (except touchstart) are dispatching to the captured
  // element. Get correct shell from it.
  mPresShell = newPresShell;
}

/******************************************************************************
 * PresShell::EventHandler::HandlingTimeAccumulator
 ******************************************************************************/

PresShell::EventHandler::HandlingTimeAccumulator::HandlingTimeAccumulator(
    const PresShell::EventHandler& aEventHandler, const WidgetEvent* aEvent)
    : mEventHandler(aEventHandler),
      mEvent(aEvent),
      mHandlingStartTime(TimeStamp::Now()) {
  MOZ_ASSERT(mEvent);
  MOZ_ASSERT(mEvent->IsTrusted());
}

PresShell::EventHandler::HandlingTimeAccumulator::~HandlingTimeAccumulator() {
  if (mEvent->mTimeStamp <= mEventHandler.mPresShell->mLastOSWake) {
    return;
  }

  switch (mEvent->mMessage) {
    case eKeyPress:
    case eKeyDown:
    case eKeyUp:
      Telemetry::AccumulateTimeDelta(Telemetry::INPUT_EVENT_HANDLED_KEYBOARD_MS,
                                     mHandlingStartTime);
      return;
    case eMouseDown:
      Telemetry::AccumulateTimeDelta(
          Telemetry::INPUT_EVENT_HANDLED_MOUSE_DOWN_MS, mHandlingStartTime);
      return;
    case eMouseUp:
      Telemetry::AccumulateTimeDelta(Telemetry::INPUT_EVENT_HANDLED_MOUSE_UP_MS,
                                     mHandlingStartTime);
      return;
    case eMouseMove:
      if (mEvent->mFlags.mHandledByAPZ) {
        Telemetry::AccumulateTimeDelta(
            Telemetry::INPUT_EVENT_HANDLED_APZ_MOUSE_MOVE_MS,
            mHandlingStartTime);
      }
      return;
    case eWheel:
      if (mEvent->mFlags.mHandledByAPZ) {
        Telemetry::AccumulateTimeDelta(
            Telemetry::INPUT_EVENT_HANDLED_APZ_WHEEL_MS, mHandlingStartTime);
      }
      return;
    case eTouchMove:
      if (mEvent->mFlags.mHandledByAPZ) {
        Telemetry::AccumulateTimeDelta(
            Telemetry::INPUT_EVENT_HANDLED_APZ_TOUCH_MOVE_MS,
            mHandlingStartTime);
      }
      return;
    default:
      return;
  }
}

void PresShell::EndPaint() {
  ClearPendingVisualScrollUpdate();

  if (mDocument) {
    mDocument->EnumerateSubDocuments([](Document& aSubDoc) {
      if (PresShell* presShell = aSubDoc.GetPresShell()) {
        presShell->EndPaint();
      }
      return CallState::Continue;
    });
  }
}

void PresShell::PingPerTickTelemetry(FlushType aFlushType) {
  mLayoutTelemetry.PingPerTickTelemetry(aFlushType);
}

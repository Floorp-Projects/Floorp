/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* a presentation of a document, part 2 */

#ifndef nsIPresShell_h___
#define nsIPresShell_h___

#include "mozilla/PresShellForwards.h"

#include "mozilla/ArenaObjectID.h"
#include "mozilla/EventForwards.h"
#include "mozilla/FlushType.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ScrollTypes.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/ServoStyleConsts.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WeakPtr.h"
#include "FrameMetrics.h"
#include "GeckoProfiler.h"
#include "gfxPoint.h"
#include "nsDOMNavigationTiming.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "nsISupports.h"
#include "nsIContent.h"
#include "nsISelectionController.h"
#include "nsQueryFrame.h"
#include "nsStringFwd.h"
#include "nsCoord.h"
#include "nsColor.h"
#include "nsFrameManager.h"
#include "nsRect.h"
#include "nsRegionFwd.h"
#include <stdio.h>  // for FILE definition
#include "nsChangeHint.h"
#include "nsRefPtrHashtable.h"
#include "nsClassHashtable.h"
#include "nsPresArena.h"
#include "nsIImageLoadingContent.h"
#include "nsMargin.h"
#include "nsFrameState.h"
#include "nsStubDocumentObserver.h"
#include "nsCOMArray.h"
#include "Units.h"

#ifdef MOZ_REFLOW_PERF
class ReflowCountMgr;
#endif

class gfxContext;
struct nsCallbackEventRequest;
class nsDocShell;
class nsIFrame;
class nsPresContext;
class nsWindowSizes;
class nsViewManager;
class nsView;
class nsIPageSequenceFrame;
class nsCanvasFrame;
class nsCaret;
namespace mozilla {
class AccessibleCaretEventHub;
class OverflowChangedTracker;
class StyleSheet;
}  // namespace mozilla
class nsFrameSelection;
class nsFrameManager;
class nsILayoutHistoryState;
class nsIReflowCallback;
class nsCSSFrameConstructor;
template <class E>
class nsCOMArray;
class AutoWeakFrame;
class MobileViewportManager;
class WeakFrame;
class nsIScrollableFrame;
class nsDisplayList;
class nsDisplayListBuilder;
class nsPIDOMWindowOuter;
struct nsPoint;
class nsINode;
struct nsRect;
class nsRegion;
class nsRefreshDriver;
class nsAutoCauseReflowNotifier;
class nsARefreshObserver;
class nsAPostRefreshObserver;
#ifdef ACCESSIBILITY
class nsAccessibilityService;
namespace mozilla {
namespace a11y {
class DocAccessible;
}  // namespace a11y
}  // namespace mozilla
#endif
class nsITimer;

namespace mozilla {
class EventStates;

namespace dom {
class Element;
class Event;
class Document;
class HTMLSlotElement;
class Touch;
class Selection;
class ShadowRoot;
}  // namespace dom

namespace layout {
class ScrollAnchorContainer;
}  // namespace layout

namespace layers {
class LayerManager;
}  // namespace layers

namespace gfx {
class SourceSurface;
}  // namespace gfx
}  // namespace mozilla

// b7b89561-4f03-44b3-9afa-b47e7f313ffb
#define NS_IPRESSHELL_IID                            \
  {                                                  \
    0xb7b89561, 0x4f03, 0x44b3, {                    \
      0x9a, 0xfa, 0xb4, 0x7e, 0x7f, 0x31, 0x3f, 0xfb \
    }                                                \
  }

#undef NOISY_INTERRUPTIBLE_REFLOW

/**
 * Presentation shell interface. Presentation shells are the
 * controlling point for managing the presentation of a document. The
 * presentation shell holds a live reference to the document, the
 * presentation context, the style manager, the style set and the root
 * frame. <p>
 *
 * When this object is Release'd, it will release the document, the
 * presentation context, the style manager, the style set and the root
 * frame.
 */

class nsIPresShell : public nsStubDocumentObserver {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IPRESSHELL_IID)

 protected:
  typedef mozilla::dom::Document Document;
  typedef mozilla::layers::FrameMetrics FrameMetrics;
  typedef mozilla::layers::LayerManager LayerManager;
  typedef mozilla::gfx::SourceSurface SourceSurface;

  enum eRenderFlag {
    STATE_IGNORING_VIEWPORT_SCROLLING = 0x1,
    STATE_DRAWWINDOW_NOT_FLUSHING = 0x2
  };
  typedef uint8_t RenderFlags;  // for storing the above flags

 public:
  nsIPresShell();

 protected:
  // IMPORTANT: The ownership implicit in the following member variables
  // has been explicitly checked.  If you add any members to this class,
  // please make the ownership explicit (pinkerton, scc).

  // These are the same Document and PresContext owned by the DocViewer.
  // we must share ownership.
  RefPtr<Document> mDocument;
  RefPtr<nsPresContext> mPresContext;
  // The document's style set owns it but we maintain a ref, may be null.
  RefPtr<mozilla::StyleSheet> mPrefStyleSheet;
  mozilla::UniquePtr<nsCSSFrameConstructor> mFrameConstructor;
  nsViewManager* mViewManager;  // [WEAK] docViewer owns it so I don't have to
  nsPresArena<8192> mFrameArena;
  RefPtr<nsFrameSelection> mSelection;
  RefPtr<nsCaret> mCaret;
  RefPtr<nsCaret> mOriginalCaret;
  RefPtr<mozilla::AccessibleCaretEventHub> mAccessibleCaretEventHub;
  // Pointer into mFrameConstructor - this is purely so that GetRootFrame() can
  // be inlined:
  nsFrameManager* mFrameManager;
  mozilla::WeakPtr<nsDocShell> mForwardingContainer;

  // The `performance.now()` value when we last started to process reflows.
  DOMHighResTimeStamp mLastReflowStart{0.0};

  // At least on Win32 and Mac after interupting a reflow we need to post
  // the resume reflow event off a timer to avoid event starvation because
  // posted messages are processed before other messages when the modal
  // moving/sizing loop is running, see bug 491700 for details.
  nsCOMPtr<nsITimer> mReflowContinueTimer;

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  // We track allocated pointers in a debug-only hashtable to assert against
  // missing/double frees.
  nsTHashtable<nsPtrHashKey<void>> mAllocatedPointers;
#endif

  // Count of the number of times this presshell has been painted to a window.
  uint64_t mPaintCount;

  // A list of stack weak frames. This is a pointer to the last item in the
  // list.
  AutoWeakFrame* mAutoWeakFrames;

  // A hash table of heap allocated weak frames.
  nsTHashtable<nsPtrHashKey<WeakFrame>> mWeakFrames;

  class DirtyRootsList {
   public:
    // Add a dirty root.
    void Add(nsIFrame* aFrame);
    // Remove this frame if present.
    void Remove(nsIFrame* aFrame);
    // Remove and return one of the shallowest dirty roots from the list.
    // (If two roots are at the same depth, order is indeterminate.)
    nsIFrame* PopShallowestRoot();
    // Remove all dirty roots.
    void Clear();
    // Is this frame one of the dirty roots?
    bool Contains(nsIFrame* aFrame) const;
    // Are there no dirty roots?
    bool IsEmpty() const;
    // Is the given frame an ancestor of any dirty root?
    bool FrameIsAncestorOfDirtyRoot(nsIFrame* aFrame) const;

   private:
    struct FrameAndDepth {
      nsIFrame* mFrame;
      const uint32_t mDepth;

      // Easy conversion to nsIFrame*, as it's the most likely need.
      operator nsIFrame*() const { return mFrame; }

      // Used to sort by reverse depths, i.e., deeper < shallower.
      class CompareByReverseDepth {
       public:
        bool Equals(const FrameAndDepth& aA, const FrameAndDepth& aB) const {
          return aA.mDepth == aB.mDepth;
        }
        bool LessThan(const FrameAndDepth& aA, const FrameAndDepth& aB) const {
          // Reverse depth! So '>' instead of '<'.
          return aA.mDepth > aB.mDepth;
        }
      };
    };
    // List of all known dirty roots, sorted by decreasing depths.
    nsTArray<FrameAndDepth> mList;
  };

  // Reflow roots that need to be reflowed.
  DirtyRootsList mDirtyRoots;

#ifdef MOZ_GECKO_PROFILER
  // These two fields capture call stacks of any changes that require a restyle
  // or a reflow. Only the first change per restyle / reflow is recorded (the
  // one that caused a call to SetNeedStyleFlush() / SetNeedLayoutFlush()).
  UniqueProfilerBacktrace mStyleCause;
  UniqueProfilerBacktrace mReflowCause;
#endif

  // Most recent canvas background color.
  nscolor mCanvasBackgroundColor;

  // Used to force allocation and rendering of proportionally more or
  // less pixels in both dimensions.
  mozilla::Maybe<float> mResolution;

  int16_t mSelectionFlags;

  // This is used to protect ourselves from triggering reflow while in the
  // middle of frame construction and the like... it really shouldn't be
  // needed, one hopes, but it is for now.
  uint16_t mChangeNestCount;

  // Flags controlling how our document is rendered.  These persist
  // between paints and so are tied with retained layer pixels.
  // PresShell flushes retained layers when the rendering state
  // changes in a way that prevents us from being able to (usefully)
  // re-use old pixels.
  RenderFlags mRenderFlags;
  bool mDidInitialize : 1;
  bool mIsDestroying : 1;
  bool mIsReflowing : 1;
  bool mIsObservingDocument : 1;
  // Whether we shouldn't ever get to FlushPendingNotifications. This flag is
  // meant only to sanity-check / assert that FlushPendingNotifications doesn't
  // happen during certain periods of time. It shouldn't be made public nor used
  // for other purposes.
  bool mForbiddenToFlush : 1;

  // We've been disconnected from the document.  We will refuse to paint the
  // document until either our timer fires or all frames are constructed.
  bool mIsDocumentGone : 1;
  bool mHaveShutDown : 1;

  // For all documents we initially lock down painting.
  bool mPaintingSuppressed : 1;

  bool mLastRootReflowHadUnconstrainedBSize : 1;

  // Indicates that it is safe to unlock painting once all pending reflows
  // have been processed.
  bool mShouldUnsuppressPainting : 1;

  bool mIgnoreFrameDestruction : 1;

  bool mIsActive : 1;
  bool mFrozen : 1;
  bool mIsFirstPaint : 1;
  bool mObservesMutationsForPrint : 1;

  // Whether the most recent interruptible reflow was actually interrupted:
  bool mWasLastReflowInterrupted : 1;

  // True if we're observing the refresh driver for style flushes.
  bool mObservingStyleFlushes : 1;

  // True if we're observing the refresh driver for layout flushes, that is, if
  // we have a reflow scheduled.
  //
  // Guaranteed to be false if mReflowContinueTimer is non-null.
  bool mObservingLayoutFlushes : 1;

  bool mResizeEventPending : 1;

  bool mFontSizeInflationForceEnabled : 1;
  bool mFontSizeInflationDisabledInMasterProcess : 1;
  bool mFontSizeInflationEnabled : 1;

  bool mPaintingIsFrozen : 1;

  // If a document belongs to an invisible DocShell, this flag must be set
  // to true, so we can avoid any paint calls for widget related to this
  // presshell.
  bool mIsNeverPainting : 1;

  // Whether the most recent change to the pres shell resolution was
  // originated by the main thread.
  bool mResolutionUpdated : 1;

  // True if the resolution has been ever changed by APZ.
  bool mResolutionUpdatedByApz : 1;

  uint32_t mPresShellId;

  // Cached font inflation values. This is done to prevent changing of font
  // inflation until a page is reloaded.
  uint32_t mFontSizeInflationEmPerLine;
  uint32_t mFontSizeInflationMinTwips;
  uint32_t mFontSizeInflationLineThreshold;

  // Whether we're currently under a FlushPendingNotifications.
  // This is used to handle flush reentry correctly.
  // NOTE: This can't be a bitfield since AutoRestore has a reference to this
  // variable.
  bool mInFlush;

  nsIFrame* mCurrentEventFrame;
  nsCOMPtr<nsIContent> mCurrentEventContent;
  nsTArray<nsIFrame*> mCurrentEventFrameStack;
  nsCOMArray<nsIContent> mCurrentEventContentStack;
  // Set of frames that we should mark with NS_FRAME_HAS_DIRTY_CHILDREN after
  // we finish reflowing mCurrentReflowRoot.
  nsTHashtable<nsPtrHashKey<nsIFrame>> mFramesToDirty;
  nsTHashtable<nsPtrHashKey<nsIScrollableFrame>> mPendingScrollAnchorSelection;
  nsTHashtable<nsPtrHashKey<nsIScrollableFrame>> mPendingScrollAnchorAdjustment;

  nsCallbackEventRequest* mFirstCallbackEventRequest = nullptr;
  nsCallbackEventRequest* mLastCallbackEventRequest = nullptr;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIPresShell, NS_IPRESSHELL_IID)

#endif /* nsIPresShell_h___ */

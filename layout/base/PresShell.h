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

#ifndef mozilla_PresShell_h
#define mozilla_PresShell_h

#include "MobileViewportManager.h"
#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/StyleSetHandle.h"
#include "mozilla/UniquePtr.h"
#include "nsAutoPtr.h"
#include "nsContentUtils.h" // For AddScriptBlocker().
#include "nsCRT.h"
#include "nsIObserver.h"
#include "nsIPresShell.h"
#include "nsISelectionController.h"
#include "nsIWidget.h"
#include "nsPresContext.h"
#include "nsRefreshDriver.h"
#include "nsStubDocumentObserver.h"
#include "nsThreadUtils.h"
#include "nsWeakReference.h"
#include "TouchManager.h"
#include "ZoomConstraintsClient.h"

class nsIDocShell;
class nsRange;

struct RangePaintInfo;
struct nsCallbackEventRequest;
#ifdef MOZ_REFLOW_PERF
class ReflowCountMgr;
#endif

class nsPresShellEventCB;
class nsAutoCauseReflowNotifier;

namespace mozilla {

class EventDispatchingCallback;

// A set type for tracking visible frames, for use by the visibility code in
// PresShell. The set contains nsIFrame* pointers.
typedef nsTHashtable<nsPtrHashKey<nsIFrame>> VisibleFrames;

// A hash table type for tracking visible regions, for use by the visibility
// code in PresShell. The mapping is from view IDs to regions in the
// coordinate system of that view's scrolled frame.
typedef nsClassHashtable<nsUint64HashKey, mozilla::CSSIntRegion> VisibleRegions;

// This is actually pref-controlled, but we use this value if we fail
// to get the pref for any reason.
#ifdef MOZ_WIDGET_ANDROID
#define PAINTLOCK_EVENT_DELAY 250
#else
#define PAINTLOCK_EVENT_DELAY 5
#endif

class PresShell final : public nsIPresShell,
                        public nsStubDocumentObserver,
                        public nsISelectionController,
                        public nsIObserver,
                        public nsSupportsWeakReference
{
public:
  PresShell();

  // nsISupports
  NS_DECL_ISUPPORTS

  static bool AccessibleCaretEnabled(nsIDocShell* aDocShell);

  void Init(nsIDocument* aDocument, nsPresContext* aPresContext,
            nsViewManager* aViewManager, mozilla::StyleSetHandle aStyleSet);
  virtual void Destroy() override;

  virtual void UpdatePreferenceStyles() override;

  NS_IMETHOD GetSelection(RawSelectionType aRawSelectionType,
                          nsISelection** aSelection) override;
  virtual mozilla::dom::Selection*
    GetCurrentSelection(SelectionType aSelectionType) override;
  virtual already_AddRefed<nsISelectionController>
            GetSelectionControllerForFocusedContent(
              nsIContent** aFocusedContent = nullptr) override;

  NS_IMETHOD SetDisplaySelection(int16_t aToggle) override;
  NS_IMETHOD GetDisplaySelection(int16_t *aToggle) override;
  NS_IMETHOD ScrollSelectionIntoView(RawSelectionType aRawSelectionType,
                                     SelectionRegion aRegion,
                                     int16_t aFlags) override;
  NS_IMETHOD RepaintSelection(RawSelectionType aRawSelectionType) override;

  virtual void BeginObservingDocument() override;
  virtual void EndObservingDocument() override;
  virtual nsresult Initialize(nscoord aWidth, nscoord aHeight) override;
  virtual nsresult ResizeReflow(nscoord aWidth, nscoord aHeight, nscoord aOldWidth = 0, nscoord aOldHeight = 0) override;
  virtual nsresult ResizeReflowIgnoreOverride(nscoord aWidth, nscoord aHeight, nscoord aOldWidth, nscoord aOldHeight) override;
  virtual nsIPageSequenceFrame* GetPageSequenceFrame() const override;
  virtual nsCanvasFrame* GetCanvasFrame() const override;

  virtual nsIFrame* GetPlaceholderFrameFor(nsIFrame* aFrame) const override;
  virtual void FrameNeedsReflow(nsIFrame *aFrame, IntrinsicDirty aIntrinsicDirty,
                                nsFrameState aBitToAdd,
                                ReflowRootHandling aRootHandling =
                                  eInferFromBitToAdd) override;
  virtual void FrameNeedsToContinueReflow(nsIFrame *aFrame) override;
  virtual void CancelAllPendingReflows() override;
  virtual bool IsSafeToFlush() const override;
  virtual void FlushPendingNotifications(mozilla::FlushType aType) override;
  virtual void FlushPendingNotifications(mozilla::ChangesToFlush aType) override;
  virtual void DestroyFramesFor(nsIContent*  aContent,
                                nsIContent** aDestroyedFramesFor) override;
  virtual void CreateFramesFor(nsIContent* aContent) override;

  /**
   * Recreates the frames for a node
   */
  virtual nsresult RecreateFramesFor(nsIContent* aContent) override;

  /**
   * Post a callback that should be handled after reflow has finished.
   */
  virtual nsresult PostReflowCallback(nsIReflowCallback* aCallback) override;
  virtual void CancelReflowCallback(nsIReflowCallback* aCallback) override;

  virtual void ClearFrameRefs(nsIFrame* aFrame) override;
  virtual already_AddRefed<gfxContext> CreateReferenceRenderingContext() override;
  virtual nsresult GoToAnchor(const nsAString& aAnchorName, bool aScroll,
                              uint32_t aAdditionalScrollFlags = 0) override;
  virtual nsresult ScrollToAnchor() override;

  virtual nsresult ScrollContentIntoView(nsIContent* aContent,
                                                     ScrollAxis  aVertical,
                                                     ScrollAxis  aHorizontal,
                                                     uint32_t    aFlags) override;
  virtual bool ScrollFrameRectIntoView(nsIFrame*     aFrame,
                                       const nsRect& aRect,
                                       ScrollAxis    aVertical,
                                       ScrollAxis    aHorizontal,
                                       uint32_t      aFlags) override;
  virtual nsRectVisibility GetRectVisibility(nsIFrame *aFrame,
                                             const nsRect &aRect,
                                             nscoord aMinTwips) const override;

  virtual void SetIgnoreFrameDestruction(bool aIgnore) override;
  virtual void NotifyDestroyingFrame(nsIFrame* aFrame) override;

  virtual nsresult CaptureHistoryState(nsILayoutHistoryState** aLayoutHistoryState) override;

  virtual void UnsuppressPainting() override;

  virtual nsresult GetAgentStyleSheets(
      nsTArray<RefPtr<mozilla::StyleSheet>>& aSheets) override;
  virtual nsresult SetAgentStyleSheets(
      const nsTArray<RefPtr<mozilla::StyleSheet>>& aSheets) override;

  virtual nsresult AddOverrideStyleSheet(mozilla::StyleSheet* aSheet) override;
  virtual nsresult RemoveOverrideStyleSheet(mozilla::StyleSheet* aSheet) override;

  virtual nsresult HandleEventWithTarget(
                                 mozilla::WidgetEvent* aEvent,
                                 nsIFrame* aFrame,
                                 nsIContent* aContent,
                                 nsEventStatus* aStatus) override;
  virtual nsIFrame* GetEventTargetFrame() override;
  virtual already_AddRefed<nsIContent> GetEventTargetContent(
                                                     mozilla::WidgetEvent* aEvent) override;

  virtual void NotifyCounterStylesAreDirty() override;

  virtual nsresult ReconstructFrames(void) override;
  virtual void Freeze() override;
  virtual void Thaw() override;
  virtual void FireOrClearDelayedEvents(bool aFireEvents) override;

  virtual nsresult RenderDocument(const nsRect& aRect, uint32_t aFlags,
                                              nscolor aBackgroundColor,
                                              gfxContext* aThebesContext) override;

  virtual already_AddRefed<SourceSurface>
  RenderNode(nsIDOMNode* aNode,
             nsIntRegion* aRegion,
             const mozilla::LayoutDeviceIntPoint aPoint,
             mozilla::LayoutDeviceIntRect* aScreenRect,
             uint32_t aFlags) override;

  virtual already_AddRefed<SourceSurface>
  RenderSelection(nsISelection* aSelection,
                  const mozilla::LayoutDeviceIntPoint aPoint,
                  mozilla::LayoutDeviceIntRect* aScreenRect,
                  uint32_t aFlags) override;

  virtual already_AddRefed<nsPIDOMWindowOuter> GetRootWindow() override;

  virtual LayerManager* GetLayerManager() override;

  virtual bool AsyncPanZoomEnabled() override;

  virtual void SetIgnoreViewportScrolling(bool aIgnore) override;

  virtual nsresult SetResolution(float aResolution) override {
    return SetResolutionImpl(aResolution, /* aScaleToResolution = */ false);
  }
  virtual nsresult SetResolutionAndScaleTo(float aResolution) override {
    return SetResolutionImpl(aResolution, /* aScaleToResolution = */ true);
  }
  virtual bool ScaleToResolution() const override;
  virtual float GetCumulativeResolution() override;
  virtual float GetCumulativeNonRootScaleResolution() override;
  virtual void SetRestoreResolution(float aResolution,
                                    mozilla::LayoutDeviceIntSize aDisplaySize) override;

  //nsIViewObserver interface

  virtual void Paint(nsView* aViewToPaint, const nsRegion& aDirtyRegion,
                     uint32_t aFlags) override;
  virtual nsresult HandleEvent(nsIFrame* aFrame,
                               mozilla::WidgetGUIEvent* aEvent,
                               bool aDontRetargetEvents,
                               nsEventStatus* aEventStatus,
                               nsIContent** aTargetContent) override;
  virtual nsresult HandleDOMEventWithTarget(
                                 nsIContent* aTargetContent,
                                 mozilla::WidgetEvent* aEvent,
                                 nsEventStatus* aStatus) override;
  virtual nsresult HandleDOMEventWithTarget(nsIContent* aTargetContent,
                                                        nsIDOMEvent* aEvent,
                                                        nsEventStatus* aStatus) override;
  virtual bool ShouldIgnoreInvalidation() override;
  virtual void WillPaint() override;
  virtual void WillPaintWindow() override;
  virtual void DidPaintWindow() override;
  virtual void ScheduleViewManagerFlush(PaintType aType = PAINT_DEFAULT) override;
  virtual void DispatchSynthMouseMove(mozilla::WidgetGUIEvent* aEvent,
                                      bool aFlushOnHoverChange) override;
  virtual void ClearMouseCaptureOnView(nsView* aView) override;
  virtual bool IsVisible() override;

  virtual already_AddRefed<mozilla::AccessibleCaretEventHub> GetAccessibleCaretEventHub() const override;

  // caret handling
  virtual already_AddRefed<nsCaret> GetCaret() const override;
  NS_IMETHOD SetCaretEnabled(bool aInEnable) override;
  NS_IMETHOD SetCaretReadOnly(bool aReadOnly) override;
  NS_IMETHOD GetCaretEnabled(bool *aOutEnabled) override;
  NS_IMETHOD SetCaretVisibilityDuringSelection(bool aVisibility) override;
  NS_IMETHOD GetCaretVisible(bool *_retval) override;
  virtual void SetCaret(nsCaret *aNewCaret) override;
  virtual void RestoreCaret() override;

  NS_IMETHOD SetSelectionFlags(int16_t aInEnable) override;
  NS_IMETHOD GetSelectionFlags(int16_t *aOutEnable) override;

  // nsISelectionController

  NS_IMETHOD PhysicalMove(int16_t aDirection, int16_t aAmount, bool aExtend) override;
  NS_IMETHOD CharacterMove(bool aForward, bool aExtend) override;
  NS_IMETHOD CharacterExtendForDelete() override;
  NS_IMETHOD CharacterExtendForBackspace() override;
  NS_IMETHOD WordMove(bool aForward, bool aExtend) override;
  NS_IMETHOD WordExtendForDelete(bool aForward) override;
  NS_IMETHOD LineMove(bool aForward, bool aExtend) override;
  NS_IMETHOD IntraLineMove(bool aForward, bool aExtend) override;
  NS_IMETHOD PageMove(bool aForward, bool aExtend) override;
  NS_IMETHOD ScrollPage(bool aForward) override;
  NS_IMETHOD ScrollLine(bool aForward) override;
  NS_IMETHOD ScrollCharacter(bool aRight) override;
  NS_IMETHOD CompleteScroll(bool aForward) override;
  NS_IMETHOD CompleteMove(bool aForward, bool aExtend) override;
  NS_IMETHOD SelectAll() override;
  NS_IMETHOD CheckVisibility(nsIDOMNode *node, int16_t startOffset, int16_t EndOffset, bool *_retval) override;
  virtual nsresult CheckVisibilityContent(nsIContent* aNode, int16_t aStartOffset,
                                          int16_t aEndOffset, bool* aRetval) override;

  // nsIDocumentObserver
  NS_DECL_NSIDOCUMENTOBSERVER_BEGINUPDATE
  NS_DECL_NSIDOCUMENTOBSERVER_ENDUPDATE
  NS_DECL_NSIDOCUMENTOBSERVER_BEGINLOAD
  NS_DECL_NSIDOCUMENTOBSERVER_ENDLOAD
  NS_DECL_NSIDOCUMENTOBSERVER_CONTENTSTATECHANGED
  NS_DECL_NSIDOCUMENTOBSERVER_DOCUMENTSTATESCHANGED
  NS_DECL_NSIDOCUMENTOBSERVER_STYLESHEETADDED
  NS_DECL_NSIDOCUMENTOBSERVER_STYLESHEETREMOVED
  NS_DECL_NSIDOCUMENTOBSERVER_STYLESHEETAPPLICABLESTATECHANGED
  NS_DECL_NSIDOCUMENTOBSERVER_STYLERULECHANGED
  NS_DECL_NSIDOCUMENTOBSERVER_STYLERULEADDED
  NS_DECL_NSIDOCUMENTOBSERVER_STYLERULEREMOVED

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTEWILLCHANGE
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  NS_DECL_NSIOBSERVER

#ifdef MOZ_REFLOW_PERF
  virtual void DumpReflows() override;
  virtual void CountReflows(const char * aName, nsIFrame * aFrame) override;
  virtual void PaintCount(const char * aName,
                                      nsRenderingContext* aRenderingContext,
                                      nsPresContext* aPresContext,
                                      nsIFrame * aFrame,
                                      const nsPoint& aOffset,
                                      uint32_t aColor) override;
  virtual void SetPaintFrameCount(bool aOn) override;
  virtual bool IsPaintingFrameCounts() override;
#endif

#ifdef DEBUG
  virtual void ListStyleContexts(FILE *out, int32_t aIndent = 0) override;

  virtual void ListStyleSheets(FILE *out, int32_t aIndent = 0) override;
  virtual void VerifyStyleTree() override;
#endif

  static mozilla::LazyLogModule gLog;

  virtual void DisableNonTestMouseEvents(bool aDisable) override;

  virtual void UpdateCanvasBackground() override;

  virtual void AddCanvasBackgroundColorItem(nsDisplayListBuilder& aBuilder,
                                            nsDisplayList& aList,
                                            nsIFrame* aFrame,
                                            const nsRect& aBounds,
                                            nscolor aBackstopColor,
                                            uint32_t aFlags) override;

  virtual void AddPrintPreviewBackgroundItem(nsDisplayListBuilder& aBuilder,
                                             nsDisplayList& aList,
                                             nsIFrame* aFrame,
                                             const nsRect& aBounds) override;

  virtual nscolor ComputeBackstopColor(nsView* aDisplayRoot) override;

  virtual nsresult SetIsActive(bool aIsActive) override;

  virtual bool GetIsViewportOverridden() override {
    return (mMobileViewportManager != nullptr);
  }

  virtual bool IsLayoutFlushObserver() override
  {
    return GetPresContext()->RefreshDriver()->
      IsLayoutFlushObserver(this);
  }

  virtual void LoadComplete() override;

  void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                              nsArenaMemoryStats *aArenaObjectsSize,
                              size_t *aPresShellSize,
                              size_t *aStyleSetsSize,
                              size_t *aTextRunsSize,
                              size_t *aPresContextSize) override;
  size_t SizeOfTextRuns(mozilla::MallocSizeOf aMallocSizeOf) const;

  virtual void AddInvalidateHiddenPresShellObserver(nsRefreshDriver *aDriver) override;

  // This data is stored as a content property (nsGkAtoms::scrolling) on
  // mContentToScrollTo when we have a pending ScrollIntoView.
  struct ScrollIntoViewData {
    ScrollAxis mContentScrollVAxis;
    ScrollAxis mContentScrollHAxis;
    uint32_t   mContentToScrollToFlags;
  };


  //////////////////////////////////////////////////////////////////////////////
  // Approximate frame visibility tracking public API.
  //////////////////////////////////////////////////////////////////////////////

  void ScheduleApproximateFrameVisibilityUpdateSoon() override;
  void ScheduleApproximateFrameVisibilityUpdateNow() override;

  void RebuildApproximateFrameVisibilityDisplayList(const nsDisplayList& aList) override;
  void RebuildApproximateFrameVisibility(nsRect* aRect = nullptr,
                                         bool aRemoveOnly = false) override;

  void EnsureFrameInApproximatelyVisibleList(nsIFrame* aFrame) override;
  void RemoveFrameFromApproximatelyVisibleList(nsIFrame* aFrame) override;

  bool AssumeAllFramesVisible() override;


  virtual void RecordShadowStyleChange(mozilla::dom::ShadowRoot* aShadowRoot) override;

  virtual bool CanDispatchEvent(
      const mozilla::WidgetGUIEvent* aEvent = nullptr) const override;

  void SetNextPaintCompressed() { mNextPaintCompressed = true; }

protected:
  virtual ~PresShell();

  void HandlePostedReflowCallbacks(bool aInterruptible);
  void CancelPostedReflowCallbacks();

  void ScheduleBeforeFirstPaint();
  void UnsuppressAndInvalidate();

  void WillCauseReflow() {
    nsContentUtils::AddScriptBlocker();
    ++mChangeNestCount;
  }
  nsresult DidCauseReflow();
  friend class ::nsAutoCauseReflowNotifier;

  nsresult DispatchEventToDOM(mozilla::WidgetEvent* aEvent,
                              nsEventStatus* aStatus,
                              nsPresShellEventCB* aEventCB);
  void DispatchTouchEventToDOM(mozilla::WidgetEvent* aEvent,
                               nsEventStatus* aStatus,
                               nsPresShellEventCB* aEventCB,
                               bool aTouchIsNew);

  void     WillDoReflow();

  /**
   * Callback handler for whether reflow happened.
   *
   * @param aInterruptible Whether or not reflow interruption is allowed.
   */
  void     DidDoReflow(bool aInterruptible);
  // ProcessReflowCommands returns whether we processed all our dirty roots
  // without interruptions.
  bool     ProcessReflowCommands(bool aInterruptible);
  // MaybeScheduleReflow checks if posting a reflow is needed, then checks if
  // the last reflow was interrupted. In the interrupted case ScheduleReflow is
  // called off a timer, otherwise it is called directly.
  void     MaybeScheduleReflow();
  // Actually schedules a reflow.  This should only be called by
  // MaybeScheduleReflow and the reflow timer ScheduleReflowOffTimer
  // sets up.
  void     ScheduleReflow();

  // DoReflow returns whether the reflow finished without interruption
  bool DoReflow(nsIFrame* aFrame, bool aInterruptible);
#ifdef DEBUG
  void DoVerifyReflow();
  void VerifyHasDirtyRootAncestor(nsIFrame* aFrame);
#endif

  // Helper for ScrollContentIntoView
  void DoScrollContentIntoView();

  /**
   * Initialize cached font inflation preference values and do an initial
   * computation to determine if font inflation is enabled.
   *
   * @see nsLayoutUtils::sFontSizeInflationEmPerLine
   * @see nsLayoutUtils::sFontSizeInflationMinTwips
   * @see nsLayoutUtils::sFontSizeInflationLineThreshold
   */
  void SetupFontInflation();

  struct RenderingState {
    explicit RenderingState(PresShell* aPresShell)
      : mResolution(aPresShell->mResolution)
      , mRenderFlags(aPresShell->mRenderFlags)
    { }
    Maybe<float> mResolution;
    RenderFlags mRenderFlags;
  };

  struct AutoSaveRestoreRenderingState {
    explicit AutoSaveRestoreRenderingState(PresShell* aPresShell)
      : mPresShell(aPresShell)
      , mOldState(aPresShell)
    {}

    ~AutoSaveRestoreRenderingState()
    {
      mPresShell->mRenderFlags = mOldState.mRenderFlags;
      mPresShell->mResolution = mOldState.mResolution;
    }

    PresShell* mPresShell;
    RenderingState mOldState;
  };
  static RenderFlags ChangeFlag(RenderFlags aFlags, bool aOnOff,
                                eRenderFlag aFlag)
  {
    return aOnOff ? (aFlags | aFlag) : (aFlag & ~aFlag);
  }


  void SetRenderingState(const RenderingState& aState);

  friend class ::nsPresShellEventCB;

  bool mCaretEnabled;

#ifdef DEBUG
  nsStyleSet* CloneStyleSet(nsStyleSet* aSet);
  bool VerifyIncrementalReflow();
  bool mInVerifyReflow;
  void ShowEventTargetDebug();
#endif

  void RecordStyleSheetChange(mozilla::StyleSheet* aStyleSheet);

  void RemovePreferenceStyles();

  // methods for painting a range to an offscreen buffer

  // given a display list, clip the items within the list to
  // the range
  nsRect ClipListToRange(nsDisplayListBuilder *aBuilder,
                         nsDisplayList* aList,
                         nsRange* aRange);

  // create a RangePaintInfo for the range aRange containing the
  // display list needed to paint the range to a surface
  mozilla::UniquePtr<RangePaintInfo>
  CreateRangePaintInfo(nsIDOMRange* aRange,
                       nsRect& aSurfaceRect,
                       bool aForPrimarySelection);

  /*
   * Paint the items to a new surface and return it.
   *
   * aSelection - selection being painted, if any
   * aRegion - clip region, if any
   * aArea - area that the surface occupies, relative to the root frame
   * aPoint - reference point, typically the mouse position
   * aScreenRect - [out] set to the area of the screen the painted area should
   *               be displayed at
   * aFlags - set RENDER_AUTO_SCALE to scale down large images, but it must not
   *          be set if a custom image was specified
   */
  already_AddRefed<SourceSurface>
  PaintRangePaintInfo(const nsTArray<mozilla::UniquePtr<RangePaintInfo>>& aItems,
                      nsISelection* aSelection,
                      nsIntRegion* aRegion,
                      nsRect aArea,
                      const mozilla::LayoutDeviceIntPoint aPoint,
                      mozilla::LayoutDeviceIntRect* aScreenRect,
                      uint32_t aFlags);

  /**
   * Methods to handle changes to user and UA sheet lists that we get
   * notified about.
   */
  void AddUserSheet(nsISupports* aSheet);
  void AddAgentSheet(nsISupports* aSheet);
  void AddAuthorSheet(nsISupports* aSheet);
  void RemoveSheet(mozilla::SheetType aType, nsISupports* aSheet);

  // Hide a view if it is a popup
  void HideViewIfPopup(nsView* aView);

  // Utility method to restore the root scrollframe state
  void RestoreRootScrollPosition();

  void MaybeReleaseCapturingContent();

  nsresult HandleRetargetedEvent(mozilla::WidgetEvent* aEvent,
                                 nsEventStatus* aStatus,
                                 nsIContent* aTarget)
  {
    PushCurrentEventInfo(nullptr, nullptr);
    mCurrentEventContent = aTarget;
    nsresult rv = NS_OK;
    if (GetCurrentEventFrame()) {
      rv = HandleEventInternal(aEvent, aStatus, true);
    }
    PopCurrentEventInfo();
    return rv;
  }

  class DelayedEvent
  {
  public:
    virtual ~DelayedEvent() { }
    virtual void Dispatch() { }
    virtual bool IsKeyPressEvent() { return false; }
  };

  class DelayedInputEvent : public DelayedEvent
  {
  public:
    virtual void Dispatch() override;

  protected:
    DelayedInputEvent();
    virtual ~DelayedInputEvent();

    mozilla::WidgetInputEvent* mEvent;
  };

  class DelayedMouseEvent : public DelayedInputEvent
  {
  public:
    explicit DelayedMouseEvent(mozilla::WidgetMouseEvent* aEvent);
  };

  class DelayedKeyEvent : public DelayedInputEvent
  {
  public:
    explicit DelayedKeyEvent(mozilla::WidgetKeyboardEvent* aEvent);
    virtual bool IsKeyPressEvent() override;
  };

  // Check if aEvent is a mouse event and record the mouse location for later
  // synth mouse moves.
  void RecordMouseLocation(mozilla::WidgetGUIEvent* aEvent);
  class nsSynthMouseMoveEvent final : public nsARefreshObserver {
  public:
    nsSynthMouseMoveEvent(PresShell* aPresShell, bool aFromScroll)
      : mPresShell(aPresShell), mFromScroll(aFromScroll) {
      NS_ASSERTION(mPresShell, "null parameter");
    }

  private:
  // Private destructor, to discourage deletion outside of Release():
    ~nsSynthMouseMoveEvent() {
      Revoke();
    }

  public:
    NS_INLINE_DECL_REFCOUNTING(nsSynthMouseMoveEvent, override)

    void Revoke() {
      if (mPresShell) {
        mPresShell->GetPresContext()->RefreshDriver()->
          RemoveRefreshObserver(this, FlushType::Display);
        mPresShell = nullptr;
      }
    }
    virtual void WillRefresh(mozilla::TimeStamp aTime) override {
      if (mPresShell) {
        RefPtr<PresShell> shell = mPresShell;
        shell->ProcessSynthMouseMoveEvent(mFromScroll);
      }
    }
  private:
    PresShell* mPresShell;
    bool mFromScroll;
  };
  void ProcessSynthMouseMoveEvent(bool aFromScroll);

  void QueryIsActive();
  nsresult UpdateImageLockingState();

  bool InZombieDocument(nsIContent *aContent);
  already_AddRefed<nsIPresShell> GetParentPresShellForEventHandling();
  nsIContent* GetCurrentEventContent();
  nsIFrame* GetCurrentEventFrame();
  nsresult RetargetEventToParent(mozilla::WidgetGUIEvent* aEvent,
                                 nsEventStatus* aEventStatus);
  void PushCurrentEventInfo(nsIFrame* aFrame, nsIContent* aContent);
  void PopCurrentEventInfo();
  /**
   * @param aIsHandlingNativeEvent      true when the caller (perhaps) handles
   *                                    an event which is caused by native
   *                                    event.  Otherwise, false.
   */
  nsresult HandleEventInternal(mozilla::WidgetEvent* aEvent,
                               nsEventStatus* aStatus,
                               bool aIsHandlingNativeEvent);
  nsresult HandlePositionedEvent(nsIFrame* aTargetFrame,
                                 mozilla::WidgetGUIEvent* aEvent,
                                 nsEventStatus* aEventStatus);
  // This returns the focused DOM window under our top level window.
  //  I.e., when we are deactive, this returns the *last* focused DOM window.
  already_AddRefed<nsPIDOMWindowOuter> GetFocusedDOMWindowInOurWindow();

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
  bool AdjustContextMenuKeyEvent(mozilla::WidgetMouseEvent* aEvent);

  //
  bool PrepareToUseCaretPosition(nsIWidget* aEventWidget,
                                 mozilla::LayoutDeviceIntPoint& aTargetPt);

  // Get the selected item and coordinates in device pixels relative to root
  // document's root view for element, first ensuring the element is onscreen
  void GetCurrentItemAndPositionForElement(nsIDOMElement *aCurrentEl,
                                           nsIContent **aTargetToUse,
                                           mozilla::LayoutDeviceIntPoint& aTargetPt,
                                           nsIWidget *aRootWidget);

  void FireResizeEvent();
  static void AsyncResizeEventCallback(nsITimer* aTimer, void* aPresShell);

  virtual void SynthesizeMouseMove(bool aFromScroll) override;

  PresShell* GetRootPresShell();

  nscolor GetDefaultBackgroundColorToDraw();

  DOMHighResTimeStamp GetPerformanceNow();

  // The callback for the mPaintSuppressionTimer timer.
  static void sPaintSuppressionCallback(nsITimer* aTimer, void* aPresShell);

  // The callback for the mReflowContinueTimer timer.
  static void sReflowContinueCallback(nsITimer* aTimer, void* aPresShell);
  bool ScheduleReflowOffTimer();

  // Widget notificiations
  virtual void WindowSizeMoveDone() override;
  virtual void SysColorChanged() override { mPresContext->SysColorChanged(); }
  virtual void ThemeChanged() override { mPresContext->ThemeChanged(); }
  virtual void BackingScaleFactorChanged() override { mPresContext->UIResolutionChanged(); }
  virtual nsIDocument* GetPrimaryContentDocument() override;

  virtual void PausePainting() override;
  virtual void ResumePainting() override;

  void UpdateActivePointerState(mozilla::WidgetGUIEvent* aEvent);


  //////////////////////////////////////////////////////////////////////////////
  // Approximate frame visibility tracking implementation.
  //////////////////////////////////////////////////////////////////////////////

  void UpdateApproximateFrameVisibility();
  void DoUpdateApproximateFrameVisibility(bool aRemoveOnly);

  void ClearApproximatelyVisibleFramesList(Maybe<mozilla::OnNonvisible> aNonvisibleAction
                                             = Nothing());
  static void ClearApproximateFrameVisibilityVisited(nsView* aView, bool aClear);
  static void MarkFramesInListApproximatelyVisible(const nsDisplayList& aList,
                                                   Maybe<VisibleRegions>& aVisibleRegions);
  void MarkFramesInSubtreeApproximatelyVisible(nsIFrame* aFrame,
                                               const nsRect& aRect,
                                               Maybe<VisibleRegions>& aVisibleRegions,
                                               bool aRemoveOnly = false);

  void DecApproximateVisibleCount(VisibleFrames& aFrames,
                                  Maybe<OnNonvisible> aNonvisibleAction = Nothing());

  nsRevocableEventPtr<nsRunnableMethod<PresShell>> mUpdateApproximateFrameVisibilityEvent;

  // A set of frames that were visible or could be visible soon at the time
  // that we last did an approximate frame visibility update.
  VisibleFrames mApproximatelyVisibleFrames;

  nsresult SetResolutionImpl(float aResolution, bool aScaleToResolution);

#ifdef DEBUG
  // The reflow root under which we're currently reflowing.  Null when
  // not in reflow.
  nsIFrame*                 mCurrentReflowRoot;
  uint32_t                  mUpdateCount;
#endif

#ifdef MOZ_REFLOW_PERF
  ReflowCountMgr*           mReflowCountMgr;
#endif

  // This is used for synthetic mouse events that are sent when what is under
  // the mouse pointer may have changed without the mouse moving (eg scrolling,
  // change to the document contents).
  // It is set only on a presshell for a root document, this value represents
  // the last observed location of the mouse relative to that root document. It
  // is set to (NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE) if the mouse isn't
  // over our window or there is no last observed mouse location for some
  // reason.
  nsPoint                   mMouseLocation;
  // This is an APZ state variable that tracks the target guid for the last
  // mouse event that was processed (corresponding to mMouseLocation). This is
  // needed for the synthetic mouse events.
  mozilla::layers::ScrollableLayerGuid mMouseEventTargetGuid;

  // mStyleSet owns it but we maintain a ref, may be null
  RefPtr<mozilla::StyleSheet> mPrefStyleSheet;

  // Set of frames that we should mark with NS_FRAME_HAS_DIRTY_CHILDREN after
  // we finish reflowing mCurrentReflowRoot.
  nsTHashtable<nsPtrHashKey<nsIFrame> > mFramesToDirty;

  // Reflow roots that need to be reflowed.
  nsTArray<nsIFrame*>       mDirtyRoots;

  nsTArray<nsAutoPtr<DelayedEvent> > mDelayedEvents;
  nsRevocableEventPtr<nsRunnableMethod<PresShell> > mResizeEvent;
  nsCOMPtr<nsITimer>        mAsyncResizeEventTimer;
private:
  nsIFrame*                 mCurrentEventFrame;
  nsCOMPtr<nsIContent>      mCurrentEventContent;
  nsTArray<nsIFrame*>       mCurrentEventFrameStack;
  nsCOMArray<nsIContent>    mCurrentEventContentStack;
protected:
  nsRevocableEventPtr<nsSynthMouseMoveEvent> mSynthMouseMoveEvent;
  nsCOMPtr<nsIContent>      mLastAnchorScrolledTo;
  RefPtr<nsCaret>         mCaret;
  RefPtr<nsCaret>         mOriginalCaret;
  nsCallbackEventRequest*   mFirstCallbackEventRequest;
  nsCallbackEventRequest*   mLastCallbackEventRequest;

  mozilla::TouchManager     mTouchManager;

  RefPtr<ZoomConstraintsClient> mZoomConstraintsClient;
  RefPtr<MobileViewportManager> mMobileViewportManager;

  RefPtr<mozilla::AccessibleCaretEventHub> mAccessibleCaretEventHub;

  // This timer controls painting suppression.  Until it fires
  // or all frames are constructed, we won't paint anything but
  // our <body> background and scrollbars.
  nsCOMPtr<nsITimer>        mPaintSuppressionTimer;

  nsCOMPtr<nsITimer>        mDelayedPaintTimer;

  // The `performance.now()` value when we last started to process reflows.
  DOMHighResTimeStamp       mLastReflowStart;

  mozilla::TimeStamp        mLoadBegin;  // used to time loads

  // Information needed to properly handle scrolling content into view if the
  // pre-scroll reflow flush can be interrupted.  mContentToScrollTo is
  // non-null between the initial scroll attempt and the first time we finish
  // processing all our dirty roots.  mContentToScrollTo has a content property
  // storing the details for the scroll operation, see ScrollIntoViewData above.
  nsCOMPtr<nsIContent>      mContentToScrollTo;

  nscoord                   mLastAnchorScrollPositionY;

  // Information about live content (which still stay in DOM tree).
  // Used in case we need re-dispatch event after sending pointer event,
  // when target of pointer event was deleted during executing user handlers.
  nsCOMPtr<nsIContent>      mPointerEventTarget;

  // This is used to protect ourselves from triggering reflow while in the
  // middle of frame construction and the like... it really shouldn't be
  // needed, one hopes, but it is for now.
  uint16_t                  mChangeNestCount;

  bool                      mDocumentLoading : 1;
  bool                      mIgnoreFrameDestruction : 1;
  bool                      mHaveShutDown : 1;
  bool                      mLastRootReflowHadUnconstrainedBSize : 1;
  bool                      mNoDelayedMouseEvents : 1;
  bool                      mNoDelayedKeyEvents : 1;

  // We've been disconnected from the document.  We will refuse to paint the
  // document until either our timer fires or all frames are constructed.
  bool                      mIsDocumentGone : 1;

  // Indicates that it is safe to unlock painting once all pending reflows
  // have been processed.
  bool                      mShouldUnsuppressPainting : 1;

  bool                      mAsyncResizeTimerIsActive : 1;
  bool                      mInResize : 1;

  bool                      mApproximateFrameVisibilityVisited : 1;

  bool                      mNextPaintCompressed : 1;

  bool                      mHasCSSBackgroundColor : 1;

  // Whether content should be scaled by the resolution amount. If this is
  // not set, a transform that scales by the inverse of the resolution is
  // applied to rendered layers.
  bool                      mScaleToResolution : 1;

  // Whether the last chrome-only escape key event is consumed.
  bool                      mIsLastChromeOnlyEscapeKeyConsumed : 1;

  // Whether the widget has received a paint message yet.
  bool                      mHasReceivedPaintMessage : 1;

  bool                      mIsLastKeyDownCanceled : 1;

  static bool               sDisableNonTestMouseEvents;
};

} // namespace mozilla

#endif // mozilla_PresShell_h

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* a presentation of a document, part 2 */

#ifndef mozilla_PresShell_h
#define mozilla_PresShell_h

#include "MobileViewportManager.h"
#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "mozilla/layers/FocusTarget.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ServoStyleSet.h"
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
class AutoPointerEventTargetUpdater;

namespace mozilla {

namespace dom {
class Element;
class Selection;
}  // namespace dom

class EventDispatchingCallback;

// A set type for tracking visible frames, for use by the visibility code in
// PresShell. The set contains nsIFrame* pointers.
typedef nsTHashtable<nsPtrHashKey<nsIFrame>> VisibleFrames;

// This is actually pref-controlled, but we use this value if we fail
// to get the pref for any reason.
#ifdef MOZ_WIDGET_ANDROID
#define PAINTLOCK_EVENT_DELAY 250
#else
#define PAINTLOCK_EVENT_DELAY 5
#endif

class PresShell final : public nsIPresShell,
                        public nsISelectionController,
                        public nsIObserver,
                        public nsSupportsWeakReference
{
  typedef layers::FocusTarget FocusTarget;

public:
  PresShell();

  // nsISupports
  NS_DECL_ISUPPORTS

  static bool AccessibleCaretEnabled(nsIDocShell* aDocShell);

  void Init(nsIDocument* aDocument, nsPresContext* aPresContext,
            nsViewManager* aViewManager,
            UniquePtr<ServoStyleSet> aStyleSet);
  void Destroy() override;

  void UpdatePreferenceStyles() override;

  NS_IMETHOD GetSelectionFromScript(RawSelectionType aRawSelectionType,
                                    dom::Selection** aSelection) override;
  dom::Selection* GetSelection(RawSelectionType aRawSelectionType) override;

  dom::Selection* GetCurrentSelection(SelectionType aSelectionType) override;

  already_AddRefed<nsISelectionController>
    GetSelectionControllerForFocusedContent(
      nsIContent** aFocusedContent = nullptr) override;

  NS_IMETHOD SetDisplaySelection(int16_t aToggle) override;
  NS_IMETHOD GetDisplaySelection(int16_t *aToggle) override;
  NS_IMETHOD ScrollSelectionIntoView(RawSelectionType aRawSelectionType,
                                     SelectionRegion aRegion,
                                     int16_t aFlags) override;
  NS_IMETHOD RepaintSelection(RawSelectionType aRawSelectionType) override;

  nsresult Initialize() override;
  nsresult ResizeReflow(nscoord aWidth, nscoord aHeight,
                        nscoord aOldWidth = 0, nscoord aOldHeight = 0,
                        ResizeReflowOptions aOptions =
                        ResizeReflowOptions::eBSizeExact) override;
  nsresult ResizeReflowIgnoreOverride(nscoord aWidth, nscoord aHeight,
                                      nscoord aOldWidth, nscoord aOldHeight,
                                      ResizeReflowOptions aOptions =
                                      ResizeReflowOptions::eBSizeExact) override;
  nsIPageSequenceFrame* GetPageSequenceFrame() const override;
  nsCanvasFrame* GetCanvasFrame() const override;

  void FrameNeedsReflow(nsIFrame *aFrame, IntrinsicDirty aIntrinsicDirty,
                                nsFrameState aBitToAdd,
                                ReflowRootHandling aRootHandling =
                                  eInferFromBitToAdd) override;
  void FrameNeedsToContinueReflow(nsIFrame *aFrame) override;
  void CancelAllPendingReflows() override;
  void DoFlushPendingNotifications(FlushType aType) override;
  void DoFlushPendingNotifications(ChangesToFlush aType) override;

  /**
   * Post a callback that should be handled after reflow has finished.
   */
  nsresult PostReflowCallback(nsIReflowCallback* aCallback) override;
  void CancelReflowCallback(nsIReflowCallback* aCallback) override;

  void ClearFrameRefs(nsIFrame* aFrame) override;
  already_AddRefed<gfxContext> CreateReferenceRenderingContext() override;
  nsresult GoToAnchor(const nsAString& aAnchorName, bool aScroll,
                              uint32_t aAdditionalScrollFlags = 0) override;
  nsresult ScrollToAnchor() override;

  nsresult ScrollContentIntoView(nsIContent* aContent,
                                                     ScrollAxis  aVertical,
                                                     ScrollAxis  aHorizontal,
                                                     uint32_t    aFlags) override;
  bool ScrollFrameRectIntoView(nsIFrame*     aFrame,
                                       const nsRect& aRect,
                                       ScrollAxis    aVertical,
                                       ScrollAxis    aHorizontal,
                                       uint32_t      aFlags) override;
  nsRectVisibility GetRectVisibility(nsIFrame *aFrame,
                                             const nsRect &aRect,
                                             nscoord aMinTwips) const override;

  void SetIgnoreFrameDestruction(bool aIgnore) override;
  void NotifyDestroyingFrame(nsIFrame* aFrame) override;

  nsresult CaptureHistoryState(nsILayoutHistoryState** aLayoutHistoryState) override;

  void UnsuppressPainting() override;

  nsresult GetAgentStyleSheets(
      nsTArray<RefPtr<StyleSheet>>& aSheets) override;
  nsresult SetAgentStyleSheets(
      const nsTArray<RefPtr<StyleSheet>>& aSheets) override;

  nsresult AddOverrideStyleSheet(StyleSheet* aSheet) override;
  nsresult RemoveOverrideStyleSheet(StyleSheet* aSheet) override;

  nsresult HandleEventWithTarget(WidgetEvent* aEvent,
                                 nsIFrame* aFrame,
                                 nsIContent* aContent,
                                 nsEventStatus* aStatus,
                                 bool aIsHandlingNativeEvent = false,
                                 nsIContent** aTargetContent = nullptr,
                                 nsIContent* aOverrideClickTarget = nullptr)
                                 override;
  nsIFrame* GetEventTargetFrame() override;
  already_AddRefed<nsIContent>
    GetEventTargetContent(WidgetEvent* aEvent) override;

  void NotifyCounterStylesAreDirty() override;

  void ReconstructFrames(void) override;
  void Freeze() override;
  void Thaw() override;
  void FireOrClearDelayedEvents(bool aFireEvents) override;

  nsresult RenderDocument(const nsRect& aRect,
                          uint32_t aFlags,
                          nscolor aBackgroundColor,
                          gfxContext* aThebesContext) override;

  already_AddRefed<SourceSurface>
  RenderNode(nsINode* aNode,
             nsIntRegion* aRegion,
             const LayoutDeviceIntPoint aPoint,
             LayoutDeviceIntRect* aScreenRect,
             uint32_t aFlags) override;

  already_AddRefed<SourceSurface>
  RenderSelection(dom::Selection* aSelection,
                  const LayoutDeviceIntPoint aPoint,
                  LayoutDeviceIntRect* aScreenRect,
                  uint32_t aFlags) override;

  already_AddRefed<nsPIDOMWindowOuter> GetRootWindow() override;

  already_AddRefed<nsPIDOMWindowOuter> GetFocusedDOMWindowInOurWindow() override;

  LayerManager* GetLayerManager() override;

  bool AsyncPanZoomEnabled() override;

  void SetIgnoreViewportScrolling(bool aIgnore) override;

  nsresult SetResolution(float aResolution) override {
    return SetResolutionImpl(aResolution, /* aScaleToResolution = */ false);
  }
  nsresult SetResolutionAndScaleTo(float aResolution) override {
    return SetResolutionImpl(aResolution, /* aScaleToResolution = */ true);
  }
  bool ScaleToResolution() const override;
  float GetCumulativeResolution() override;
  float GetCumulativeNonRootScaleResolution() override;
  void SetRestoreResolution(float aResolution,
                            LayoutDeviceIntSize aDisplaySize) override;

  //nsIViewObserver interface

  void Paint(nsView* aViewToPaint, const nsRegion& aDirtyRegion,
             uint32_t aFlags) override;
  MOZ_CAN_RUN_SCRIPT nsresult HandleEvent(nsIFrame* aFrame,
                                          WidgetGUIEvent* aEvent,
                                          bool aDontRetargetEvents,
                                          nsEventStatus* aEventStatus) override;
  nsresult HandleDOMEventWithTarget(nsIContent* aTargetContent,
                                    WidgetEvent* aEvent,
                                    nsEventStatus* aStatus) override;
  nsresult HandleDOMEventWithTarget(nsIContent* aTargetContent,
                                    dom::Event* aEvent,
                                    nsEventStatus* aStatus) override;
  bool ShouldIgnoreInvalidation() override;
  void WillPaint() override;
  void WillPaintWindow() override;
  void DidPaintWindow() override;
  void ScheduleViewManagerFlush(PaintType aType = PAINT_DEFAULT) override;
  void ClearMouseCaptureOnView(nsView* aView) override;
  bool IsVisible() override;
  void SuppressDisplayport(bool aEnabled) override;
  void RespectDisplayportSuppression(bool aEnabled) override;
  bool IsDisplayportSuppressed() override;

  already_AddRefed<AccessibleCaretEventHub> GetAccessibleCaretEventHub() const override;

  // caret handling
  already_AddRefed<nsCaret> GetCaret() const override;
  NS_IMETHOD SetCaretEnabled(bool aInEnable) override;
  NS_IMETHOD SetCaretReadOnly(bool aReadOnly) override;
  NS_IMETHOD GetCaretEnabled(bool *aOutEnabled) override;
  NS_IMETHOD SetCaretVisibilityDuringSelection(bool aVisibility) override;
  NS_IMETHOD GetCaretVisible(bool *_retval) override;
  void SetCaret(nsCaret *aNewCaret) override;
  void RestoreCaret() override;

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
  NS_IMETHOD CheckVisibility(nsINode *node, int16_t startOffset, int16_t EndOffset, bool *_retval) override;
  nsresult CheckVisibilityContent(nsIContent* aNode, int16_t aStartOffset,
                                  int16_t aEndOffset, bool* aRetval) override;

  // nsIDocumentObserver
  NS_DECL_NSIDOCUMENTOBSERVER_BEGINLOAD
  NS_DECL_NSIDOCUMENTOBSERVER_ENDLOAD
  NS_DECL_NSIDOCUMENTOBSERVER_CONTENTSTATECHANGED
  NS_DECL_NSIDOCUMENTOBSERVER_DOCUMENTSTATESCHANGED

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTEWILLCHANGE
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  NS_DECL_NSIOBSERVER

#ifdef MOZ_REFLOW_PERF
  void DumpReflows() override;
  void CountReflows(const char * aName, nsIFrame * aFrame) override;
  void PaintCount(const char * aName,
                  gfxContext* aRenderingContext,
                  nsPresContext* aPresContext,
                  nsIFrame * aFrame,
                  const nsPoint& aOffset,
                  uint32_t aColor) override;
  void SetPaintFrameCount(bool aOn) override;
  bool IsPaintingFrameCounts() override;
#endif

#ifdef DEBUG
  void ListComputedStyles(FILE *out, int32_t aIndent = 0) override;

  void ListStyleSheets(FILE *out, int32_t aIndent = 0) override;
#endif

  static LazyLogModule gLog;

  void DisableNonTestMouseEvents(bool aDisable) override;

  void UpdateCanvasBackground() override;

  void AddCanvasBackgroundColorItem(nsDisplayListBuilder& aBuilder,
                                    nsDisplayList& aList,
                                    nsIFrame* aFrame,
                                    const nsRect& aBounds,
                                    nscolor aBackstopColor,
                                    uint32_t aFlags) override;

  void AddPrintPreviewBackgroundItem(nsDisplayListBuilder& aBuilder,
                                     nsDisplayList& aList,
                                     nsIFrame* aFrame,
                                     const nsRect& aBounds) override;

  nscolor ComputeBackstopColor(nsView* aDisplayRoot) override;

  nsresult SetIsActive(bool aIsActive) override;

  bool GetIsViewportOverridden() override {
    return (mMobileViewportManager != nullptr);
  }

  bool IsLayoutFlushObserver() override
  {
    return GetPresContext()->RefreshDriver()->
      IsLayoutFlushObserver(this);
  }

  void LoadComplete() override;

  void AddSizeOfIncludingThis(nsWindowSizes& aWindowSizes)
    const override;
  size_t SizeOfTextRuns(MallocSizeOf aMallocSizeOf) const;

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

  bool CanDispatchEvent(const WidgetGUIEvent* aEvent = nullptr) const override;

  void SetNextPaintCompressed() { mNextPaintCompressed = true; }

  void NotifyStyleSheetServiceSheetAdded(StyleSheet* aSheet,
                                         uint32_t aSheetType) override;
  void NotifyStyleSheetServiceSheetRemoved(StyleSheet* aSheet,
                                           uint32_t aSheetType) override;

  bool HasHandledUserInput() const override {
    return mHasHandledUserInput;
  }

  void FireResizeEvent() override;

  static PresShell* GetShellForEventTarget(nsIFrame* aFrame,
                                           nsIContent* aContent);
  static PresShell* GetShellForTouchEvent(WidgetGUIEvent* aEvent);

private:
  ~PresShell();

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
  friend class ::AutoPointerEventTargetUpdater;

  nsresult DispatchEventToDOM(WidgetEvent* aEvent,
                              nsEventStatus* aStatus,
                              nsPresShellEventCB* aEventCB);
  void DispatchTouchEventToDOM(WidgetEvent* aEvent,
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
  UniquePtr<ServoStyleSet> CloneStyleSet(ServoStyleSet* aSet);
  bool VerifyIncrementalReflow();
  bool mInVerifyReflow;
  void ShowEventTargetDebug();
#endif

  void RemovePreferenceStyles();

  // methods for painting a range to an offscreen buffer

  // given a display list, clip the items within the list to
  // the range
  nsRect ClipListToRange(nsDisplayListBuilder *aBuilder,
                         nsDisplayList* aList,
                         nsRange* aRange);

  // create a RangePaintInfo for the range aRange containing the
  // display list needed to paint the range to a surface
  UniquePtr<RangePaintInfo>
  CreateRangePaintInfo(nsRange* aRange,
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
  PaintRangePaintInfo(const nsTArray<UniquePtr<RangePaintInfo>>& aItems,
                      dom::Selection* aSelection,
                      nsIntRegion* aRegion,
                      nsRect aArea,
                      const LayoutDeviceIntPoint aPoint,
                      LayoutDeviceIntRect* aScreenRect,
                      uint32_t aFlags);

  /**
   * Methods to handle changes to user and UA sheet lists that we get
   * notified about.
   */
  void AddUserSheet(StyleSheet* aSheet);
  void AddAgentSheet(StyleSheet* aSheet);
  void AddAuthorSheet(StyleSheet* aSheet);
  void RemoveSheet(SheetType aType, StyleSheet* aSheet);

  // Hide a view if it is a popup
  void HideViewIfPopup(nsView* aView);

  // Utility method to restore the root scrollframe state
  void RestoreRootScrollPosition();

  void MaybeReleaseCapturingContent();

  nsresult HandleRetargetedEvent(WidgetEvent* aEvent,
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
    void Dispatch() override;

  protected:
    DelayedInputEvent();
    ~DelayedInputEvent() override;

    WidgetInputEvent* mEvent;
  };

  class DelayedMouseEvent : public DelayedInputEvent
  {
  public:
    explicit DelayedMouseEvent(WidgetMouseEvent* aEvent);
  };

  class DelayedKeyEvent : public DelayedInputEvent
  {
  public:
    explicit DelayedKeyEvent(WidgetKeyboardEvent* aEvent);
    bool IsKeyPressEvent() override;
  };

  // Check if aEvent is a mouse event and record the mouse location for later
  // synth mouse moves.
  void RecordMouseLocation(WidgetGUIEvent* aEvent);
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
    void WillRefresh(TimeStamp aTime) override {
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
  MOZ_CAN_RUN_SCRIPT nsresult
  RetargetEventToParent(WidgetGUIEvent* aEvent, nsEventStatus* aEventStatus);
  void PushCurrentEventInfo(nsIFrame* aFrame, nsIContent* aContent);
  void PopCurrentEventInfo();
  /**
   * @param aIsHandlingNativeEvent      true when the caller (perhaps) handles
   *                                    an event which is caused by native
   *                                    event.  Otherwise, false.
   */
  nsresult HandleEventInternal(WidgetEvent* aEvent,
                               nsEventStatus* aStatus,
                               bool aIsHandlingNativeEvent,
                               nsIContent* aOverrideClickTarget = nullptr);

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
  bool AdjustContextMenuKeyEvent(WidgetMouseEvent* aEvent);

  //
  bool PrepareToUseCaretPosition(nsIWidget* aEventWidget,
                                 LayoutDeviceIntPoint& aTargetPt);

  // Get the selected item and coordinates in device pixels relative to root
  // document's root view for element, first ensuring the element is onscreen
  void GetCurrentItemAndPositionForElement(dom::Element* aFocusedElement,
                                           nsIContent **aTargetToUse,
                                           LayoutDeviceIntPoint& aTargetPt,
                                           nsIWidget *aRootWidget);

  void SynthesizeMouseMove(bool aFromScroll) override;

  PresShell* GetRootPresShell();

  nscolor GetDefaultBackgroundColorToDraw();

  DOMHighResTimeStamp GetPerformanceNowUnclamped();

  // The callback for the mPaintSuppressionTimer timer.
  static void sPaintSuppressionCallback(nsITimer* aTimer, void* aPresShell);

  // The callback for the mReflowContinueTimer timer.
  static void sReflowContinueCallback(nsITimer* aTimer, void* aPresShell);
  bool ScheduleReflowOffTimer();

  // Widget notificiations
  void WindowSizeMoveDone() override;
  void SysColorChanged() override { mPresContext->SysColorChanged(); }
  void ThemeChanged() override { mPresContext->ThemeChanged(); }
  void BackingScaleFactorChanged() override { mPresContext->UIResolutionChanged(); }
  nsIDocument* GetPrimaryContentDocument() override;

  void PausePainting() override;
  void ResumePainting() override;

  //////////////////////////////////////////////////////////////////////////////
  // Approximate frame visibility tracking implementation.
  //////////////////////////////////////////////////////////////////////////////

  void UpdateApproximateFrameVisibility();
  void DoUpdateApproximateFrameVisibility(bool aRemoveOnly);

  void ClearApproximatelyVisibleFramesList(const Maybe<OnNonvisible>& aNonvisibleAction
                                             = Nothing());
  static void ClearApproximateFrameVisibilityVisited(nsView* aView, bool aClear);
  static void MarkFramesInListApproximatelyVisible(const nsDisplayList& aList);
  void MarkFramesInSubtreeApproximatelyVisible(nsIFrame* aFrame,
                                               const nsRect& aRect,
                                               bool aRemoveOnly = false);

  void DecApproximateVisibleCount(VisibleFrames& aFrames,
                                  const Maybe<OnNonvisible>& aNonvisibleAction = Nothing());

  nsRevocableEventPtr<nsRunnableMethod<PresShell>> mUpdateApproximateFrameVisibilityEvent;

  // A set of frames that were visible or could be visible soon at the time
  // that we last did an approximate frame visibility update.
  VisibleFrames mApproximatelyVisibleFrames;

  nsresult SetResolutionImpl(float aResolution, bool aScaleToResolution);

  nsIContent* GetOverrideClickTarget(WidgetGUIEvent* aEvent,
                                     nsIFrame* aFrame);
#ifdef DEBUG
  // The reflow root under which we're currently reflowing.  Null when
  // not in reflow.
  nsIFrame* mCurrentReflowRoot;
#endif

#ifdef MOZ_REFLOW_PERF
  ReflowCountMgr* mReflowCountMgr;
#endif

  // This is used for synthetic mouse events that are sent when what is under
  // the mouse pointer may have changed without the mouse moving (eg scrolling,
  // change to the document contents).
  // It is set only on a presshell for a root document, this value represents
  // the last observed location of the mouse relative to that root document. It
  // is set to (NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE) if the mouse isn't
  // over our window or there is no last observed mouse location for some
  // reason.
  nsPoint mMouseLocation;
  // This is an APZ state variable that tracks the target guid for the last
  // mouse event that was processed (corresponding to mMouseLocation). This is
  // needed for the synthetic mouse events.
  layers::ScrollableLayerGuid mMouseEventTargetGuid;

  // mStyleSet owns it but we maintain a ref, may be null
  RefPtr<StyleSheet> mPrefStyleSheet;

  // Set of frames that we should mark with NS_FRAME_HAS_DIRTY_CHILDREN after
  // we finish reflowing mCurrentReflowRoot.
  nsTHashtable<nsPtrHashKey<nsIFrame> > mFramesToDirty;

  // Reflow roots that need to be reflowed.
  nsTArray<nsIFrame*> mDirtyRoots;

  nsTArray<nsAutoPtr<DelayedEvent> > mDelayedEvents;
private:
  nsIFrame* mCurrentEventFrame;
  nsCOMPtr<nsIContent> mCurrentEventContent;
  nsTArray<nsIFrame*> mCurrentEventFrameStack;
  nsCOMArray<nsIContent> mCurrentEventContentStack;
  nsRevocableEventPtr<nsSynthMouseMoveEvent> mSynthMouseMoveEvent;
  nsCOMPtr<nsIContent> mLastAnchorScrolledTo;
  RefPtr<nsCaret> mCaret;
  RefPtr<nsCaret> mOriginalCaret;
  nsCallbackEventRequest* mFirstCallbackEventRequest;
  nsCallbackEventRequest* mLastCallbackEventRequest;

  TouchManager mTouchManager;

  RefPtr<ZoomConstraintsClient> mZoomConstraintsClient;
  RefPtr<MobileViewportManager> mMobileViewportManager;

  RefPtr<AccessibleCaretEventHub> mAccessibleCaretEventHub;

  // This timer controls painting suppression.  Until it fires
  // or all frames are constructed, we won't paint anything but
  // our <body> background and scrollbars.
  nsCOMPtr<nsITimer> mPaintSuppressionTimer;

  nsCOMPtr<nsITimer> mDelayedPaintTimer;

  // The `performance.now()` value when we last started to process reflows.
  DOMHighResTimeStamp mLastReflowStart;

  TimeStamp mLoadBegin;  // used to time loads

  // Information needed to properly handle scrolling content into view if the
  // pre-scroll reflow flush can be interrupted.  mContentToScrollTo is
  // non-null between the initial scroll attempt and the first time we finish
  // processing all our dirty roots.  mContentToScrollTo has a content property
  // storing the details for the scroll operation, see ScrollIntoViewData above.
  nsCOMPtr<nsIContent> mContentToScrollTo;

  nscoord mLastAnchorScrollPositionY;

  // Information about live content (which still stay in DOM tree).
  // Used in case we need re-dispatch event after sending pointer event,
  // when target of pointer event was deleted during executing user handlers.
  nsCOMPtr<nsIContent> mPointerEventTarget;

  int32_t mActiveSuppressDisplayport;

  // The focus sequence number of the last processed input event
  uint64_t mAPZFocusSequenceNumber;
  // The focus information needed for async keyboard scrolling
  FocusTarget mAPZFocusTarget;

  bool mDocumentLoading : 1;
  bool mIgnoreFrameDestruction : 1;
  bool mHaveShutDown : 1;
  bool mLastRootReflowHadUnconstrainedBSize : 1;
  bool mNoDelayedMouseEvents : 1;
  bool mNoDelayedKeyEvents : 1;

  // Indicates that it is safe to unlock painting once all pending reflows
  // have been processed.
  bool mShouldUnsuppressPainting : 1;

  bool mApproximateFrameVisibilityVisited : 1;

  bool mNextPaintCompressed : 1;

  bool mHasCSSBackgroundColor : 1;

  // Whether content should be scaled by the resolution amount. If this is
  // not set, a transform that scales by the inverse of the resolution is
  // applied to rendered layers.
  bool mScaleToResolution : 1;

  // Whether the last chrome-only escape key event is consumed.
  bool mIsLastChromeOnlyEscapeKeyConsumed : 1;

  // Whether the widget has received a paint message yet.
  bool mHasReceivedPaintMessage : 1;

  bool mIsLastKeyDownCanceled : 1;

  // Whether we have ever handled a user input event
  bool mHasHandledUserInput : 1;

#ifdef NIGHTLY_BUILD
  // Whether we should dispatch keypress events even for non-printable keys
  // for keeping backward compatibility.
  bool mForceDispatchKeyPressEventsForNonPrintableKeys : 1;
  // Whether mForceDispatchKeyPressEventsForNonPrintableKeys is initialized.
  bool mInitializedForceDispatchKeyPressEventsForNonPrintableKeys : 1;
#endif // #ifdef NIGHTLY_BUILD

  static bool sDisableNonTestMouseEvents;

  TimeStamp mLastOSWake;

  static TimeStamp sLastInputCreated;
  static TimeStamp sLastInputProcessed;

  static bool sProcessInteractable;
};

} // namespace mozilla

#endif // mozilla_PresShell_h

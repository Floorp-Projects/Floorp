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

#ifndef nsPresShell_h_
#define nsPresShell_h_

#include "nsIPresShell.h"
#include "nsStubDocumentObserver.h"
#include "nsISelectionController.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsCRT.h"
#include "nsAutoPtr.h"
#include "nsIWidget.h"
#include "nsStyleSet.h"
#include "nsContentUtils.h" // For AddScriptBlocker().
#include "nsRefreshDriver.h"
#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "mozilla/MemoryReporting.h"

class nsRange;
class nsIDragService;

struct RangePaintInfo;
struct nsCallbackEventRequest;
#ifdef MOZ_REFLOW_PERF
class ReflowCountMgr;
#endif

class nsPresShellEventCB;
class nsAutoCauseReflowNotifier;

namespace mozilla {
class CSSStyleSheet;
class EventDispatchingCallback;
} // namespace mozilla

// 250ms.  This is actually pref-controlled, but we use this value if we fail
// to get the pref for any reason.
#define PAINTLOCK_EVENT_DELAY 250

class PresShell MOZ_FINAL : public nsIPresShell,
                            public nsStubDocumentObserver,
                            public nsISelectionController, public nsIObserver,
                            public nsSupportsWeakReference
{
public:
  PresShell();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  // nsISupports
  NS_DECL_ISUPPORTS

  // Touch caret preference
  static bool TouchCaretPrefEnabled();

  // Selection caret preference
  static bool SelectionCaretPrefEnabled();

  // BeforeAfterKeyboardEvent preference
  static bool BeforeAfterKeyboardEventEnabled();

  void Init(nsIDocument* aDocument, nsPresContext* aPresContext,
            nsViewManager* aViewManager, nsStyleSet* aStyleSet,
            nsCompatibility aCompatMode);
  virtual void Destroy() MOZ_OVERRIDE;
  virtual void MakeZombie() MOZ_OVERRIDE;

  virtual nsresult SetPreferenceStyleRules(bool aForceReflow) MOZ_OVERRIDE;

  NS_IMETHOD GetSelection(SelectionType aType, nsISelection** aSelection) MOZ_OVERRIDE;
  virtual mozilla::dom::Selection* GetCurrentSelection(SelectionType aType) MOZ_OVERRIDE;

  NS_IMETHOD SetDisplaySelection(int16_t aToggle) MOZ_OVERRIDE;
  NS_IMETHOD GetDisplaySelection(int16_t *aToggle) MOZ_OVERRIDE;
  NS_IMETHOD ScrollSelectionIntoView(SelectionType aType, SelectionRegion aRegion,
                                     int16_t aFlags) MOZ_OVERRIDE;
  NS_IMETHOD RepaintSelection(SelectionType aType) MOZ_OVERRIDE;

  virtual void BeginObservingDocument() MOZ_OVERRIDE;
  virtual void EndObservingDocument() MOZ_OVERRIDE;
  virtual nsresult Initialize(nscoord aWidth, nscoord aHeight) MOZ_OVERRIDE;
  virtual nsresult ResizeReflow(nscoord aWidth, nscoord aHeight) MOZ_OVERRIDE;
  virtual nsresult ResizeReflowOverride(nscoord aWidth, nscoord aHeight) MOZ_OVERRIDE;
  virtual nsIPageSequenceFrame* GetPageSequenceFrame() const MOZ_OVERRIDE;
  virtual nsCanvasFrame* GetCanvasFrame() const MOZ_OVERRIDE;
  virtual nsIFrame* GetRealPrimaryFrameFor(nsIContent* aContent) const MOZ_OVERRIDE;

  virtual nsIFrame* GetPlaceholderFrameFor(nsIFrame* aFrame) const MOZ_OVERRIDE;
  virtual void FrameNeedsReflow(nsIFrame *aFrame, IntrinsicDirty aIntrinsicDirty,
                                            nsFrameState aBitToAdd) MOZ_OVERRIDE;
  virtual void FrameNeedsToContinueReflow(nsIFrame *aFrame) MOZ_OVERRIDE;
  virtual void CancelAllPendingReflows() MOZ_OVERRIDE;
  virtual bool IsSafeToFlush() const MOZ_OVERRIDE;
  virtual void FlushPendingNotifications(mozFlushType aType) MOZ_OVERRIDE;
  virtual void FlushPendingNotifications(mozilla::ChangesToFlush aType) MOZ_OVERRIDE;
  virtual void DestroyFramesFor(nsIContent*  aContent,
                                nsIContent** aDestroyedFramesFor) MOZ_OVERRIDE;
  virtual void CreateFramesFor(nsIContent* aContent) MOZ_OVERRIDE;

  /**
   * Recreates the frames for a node
   */
  virtual nsresult RecreateFramesFor(nsIContent* aContent) MOZ_OVERRIDE;

  /**
   * Post a callback that should be handled after reflow has finished.
   */
  virtual nsresult PostReflowCallback(nsIReflowCallback* aCallback) MOZ_OVERRIDE;
  virtual void CancelReflowCallback(nsIReflowCallback* aCallback) MOZ_OVERRIDE;

  virtual void ClearFrameRefs(nsIFrame* aFrame) MOZ_OVERRIDE;
  virtual already_AddRefed<gfxContext> CreateReferenceRenderingContext() MOZ_OVERRIDE;
  virtual nsresult GoToAnchor(const nsAString& aAnchorName, bool aScroll,
                              uint32_t aAdditionalScrollFlags = 0) MOZ_OVERRIDE;
  virtual nsresult ScrollToAnchor() MOZ_OVERRIDE;

  virtual nsresult ScrollContentIntoView(nsIContent* aContent,
                                                     ScrollAxis  aVertical,
                                                     ScrollAxis  aHorizontal,
                                                     uint32_t    aFlags) MOZ_OVERRIDE;
  virtual bool ScrollFrameRectIntoView(nsIFrame*     aFrame,
                                       const nsRect& aRect,
                                       ScrollAxis    aVertical,
                                       ScrollAxis    aHorizontal,
                                       uint32_t      aFlags) MOZ_OVERRIDE;
  virtual nsRectVisibility GetRectVisibility(nsIFrame *aFrame,
                                             const nsRect &aRect,
                                             nscoord aMinTwips) const MOZ_OVERRIDE;

  virtual void SetIgnoreFrameDestruction(bool aIgnore) MOZ_OVERRIDE;
  virtual void NotifyDestroyingFrame(nsIFrame* aFrame) MOZ_OVERRIDE;

  virtual nsresult CaptureHistoryState(nsILayoutHistoryState** aLayoutHistoryState) MOZ_OVERRIDE;

  virtual void UnsuppressPainting() MOZ_OVERRIDE;

  virtual nsresult GetAgentStyleSheets(nsCOMArray<nsIStyleSheet>& aSheets) MOZ_OVERRIDE;
  virtual nsresult SetAgentStyleSheets(const nsCOMArray<nsIStyleSheet>& aSheets) MOZ_OVERRIDE;

  virtual nsresult AddOverrideStyleSheet(nsIStyleSheet *aSheet) MOZ_OVERRIDE;
  virtual nsresult RemoveOverrideStyleSheet(nsIStyleSheet *aSheet) MOZ_OVERRIDE;

  virtual nsresult HandleEventWithTarget(
                                 mozilla::WidgetEvent* aEvent,
                                 nsIFrame* aFrame,
                                 nsIContent* aContent,
                                 nsEventStatus* aStatus) MOZ_OVERRIDE;
  virtual nsIFrame* GetEventTargetFrame() MOZ_OVERRIDE;
  virtual already_AddRefed<nsIContent> GetEventTargetContent(
                                                     mozilla::WidgetEvent* aEvent) MOZ_OVERRIDE;

  virtual void NotifyCounterStylesAreDirty() MOZ_OVERRIDE;

  virtual nsresult ReconstructFrames(void) MOZ_OVERRIDE;
  virtual void Freeze() MOZ_OVERRIDE;
  virtual void Thaw() MOZ_OVERRIDE;
  virtual void FireOrClearDelayedEvents(bool aFireEvents) MOZ_OVERRIDE;

  virtual nsresult RenderDocument(const nsRect& aRect, uint32_t aFlags,
                                              nscolor aBackgroundColor,
                                              gfxContext* aThebesContext) MOZ_OVERRIDE;

  virtual mozilla::TemporaryRef<SourceSurface>
  RenderNode(nsIDOMNode* aNode,
             nsIntRegion* aRegion,
             nsIntPoint& aPoint,
             nsIntRect* aScreenRect) MOZ_OVERRIDE;

  virtual mozilla::TemporaryRef<SourceSurface>
  RenderSelection(nsISelection* aSelection,
                  nsIntPoint& aPoint,
                  nsIntRect* aScreenRect) MOZ_OVERRIDE;

  virtual already_AddRefed<nsPIDOMWindow> GetRootWindow() MOZ_OVERRIDE;

  virtual LayerManager* GetLayerManager() MOZ_OVERRIDE;

  virtual void SetIgnoreViewportScrolling(bool aIgnore) MOZ_OVERRIDE;

  virtual nsresult SetResolution(float aXResolution, float aYResolution) MOZ_OVERRIDE {
    return SetResolutionImpl(aXResolution, aYResolution, /* aScaleToResolution = */ false);
  }
  virtual nsresult SetResolutionAndScaleTo(float aXResolution, float aYResolution) MOZ_OVERRIDE {
    return SetResolutionImpl(aXResolution, aYResolution, /* aScaleToResolution = */ true);
  }
  virtual bool ScaleToResolution() const MOZ_OVERRIDE;
  virtual gfxSize GetCumulativeResolution() MOZ_OVERRIDE;

  //nsIViewObserver interface

  virtual void Paint(nsView* aViewToPaint, const nsRegion& aDirtyRegion,
                     uint32_t aFlags) MOZ_OVERRIDE;
  virtual nsresult HandleEvent(nsIFrame* aFrame,
                               mozilla::WidgetGUIEvent* aEvent,
                               bool aDontRetargetEvents,
                               nsEventStatus* aEventStatus) MOZ_OVERRIDE;
  virtual nsresult HandleDOMEventWithTarget(
                                 nsIContent* aTargetContent,
                                 mozilla::WidgetEvent* aEvent,
                                 nsEventStatus* aStatus) MOZ_OVERRIDE;
  virtual nsresult HandleDOMEventWithTarget(nsIContent* aTargetContent,
                                                        nsIDOMEvent* aEvent,
                                                        nsEventStatus* aStatus) MOZ_OVERRIDE;
  virtual bool ShouldIgnoreInvalidation() MOZ_OVERRIDE;
  virtual void WillPaint() MOZ_OVERRIDE;
  virtual void WillPaintWindow() MOZ_OVERRIDE;
  virtual void DidPaintWindow() MOZ_OVERRIDE;
  virtual void ScheduleViewManagerFlush(PaintType aType = PAINT_DEFAULT) MOZ_OVERRIDE;
  virtual void DispatchSynthMouseMove(mozilla::WidgetGUIEvent* aEvent,
                                      bool aFlushOnHoverChange) MOZ_OVERRIDE;
  virtual void ClearMouseCaptureOnView(nsView* aView) MOZ_OVERRIDE;
  virtual bool IsVisible() MOZ_OVERRIDE;

  // touch caret
  virtual already_AddRefed<mozilla::TouchCaret> GetTouchCaret() const MOZ_OVERRIDE;
  virtual mozilla::dom::Element* GetTouchCaretElement() const MOZ_OVERRIDE;
  virtual void SetMayHaveTouchCaret(bool aSet) MOZ_OVERRIDE;
  virtual bool MayHaveTouchCaret() MOZ_OVERRIDE;
  // selection caret
  virtual already_AddRefed<mozilla::SelectionCarets> GetSelectionCarets() const MOZ_OVERRIDE;
  virtual mozilla::dom::Element* GetSelectionCaretsStartElement() const MOZ_OVERRIDE;
  virtual mozilla::dom::Element* GetSelectionCaretsEndElement() const MOZ_OVERRIDE;
  // caret handling
  virtual already_AddRefed<nsCaret> GetCaret() const MOZ_OVERRIDE;
  NS_IMETHOD SetCaretEnabled(bool aInEnable) MOZ_OVERRIDE;
  NS_IMETHOD SetCaretReadOnly(bool aReadOnly) MOZ_OVERRIDE;
  NS_IMETHOD GetCaretEnabled(bool *aOutEnabled) MOZ_OVERRIDE;
  NS_IMETHOD SetCaretVisibilityDuringSelection(bool aVisibility) MOZ_OVERRIDE;
  NS_IMETHOD GetCaretVisible(bool *_retval) MOZ_OVERRIDE;
  virtual void SetCaret(nsCaret *aNewCaret) MOZ_OVERRIDE;
  virtual void RestoreCaret() MOZ_OVERRIDE;

  NS_IMETHOD SetSelectionFlags(int16_t aInEnable) MOZ_OVERRIDE;
  NS_IMETHOD GetSelectionFlags(int16_t *aOutEnable) MOZ_OVERRIDE;

  // nsISelectionController

  NS_IMETHOD PhysicalMove(int16_t aDirection, int16_t aAmount, bool aExtend) MOZ_OVERRIDE;
  NS_IMETHOD CharacterMove(bool aForward, bool aExtend) MOZ_OVERRIDE;
  NS_IMETHOD CharacterExtendForDelete() MOZ_OVERRIDE;
  NS_IMETHOD CharacterExtendForBackspace() MOZ_OVERRIDE;
  NS_IMETHOD WordMove(bool aForward, bool aExtend) MOZ_OVERRIDE;
  NS_IMETHOD WordExtendForDelete(bool aForward) MOZ_OVERRIDE;
  NS_IMETHOD LineMove(bool aForward, bool aExtend) MOZ_OVERRIDE;
  NS_IMETHOD IntraLineMove(bool aForward, bool aExtend) MOZ_OVERRIDE;
  NS_IMETHOD PageMove(bool aForward, bool aExtend) MOZ_OVERRIDE;
  NS_IMETHOD ScrollPage(bool aForward) MOZ_OVERRIDE;
  NS_IMETHOD ScrollLine(bool aForward) MOZ_OVERRIDE;
  NS_IMETHOD ScrollCharacter(bool aRight) MOZ_OVERRIDE;
  NS_IMETHOD CompleteScroll(bool aForward) MOZ_OVERRIDE;
  NS_IMETHOD CompleteMove(bool aForward, bool aExtend) MOZ_OVERRIDE;
  NS_IMETHOD SelectAll() MOZ_OVERRIDE;
  NS_IMETHOD CheckVisibility(nsIDOMNode *node, int16_t startOffset, int16_t EndOffset, bool *_retval) MOZ_OVERRIDE;
  virtual nsresult CheckVisibilityContent(nsIContent* aNode, int16_t aStartOffset,
                                          int16_t aEndOffset, bool* aRetval) MOZ_OVERRIDE;

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
  virtual void DumpReflows() MOZ_OVERRIDE;
  virtual void CountReflows(const char * aName, nsIFrame * aFrame) MOZ_OVERRIDE;
  virtual void PaintCount(const char * aName,
                                      nsRenderingContext* aRenderingContext,
                                      nsPresContext* aPresContext,
                                      nsIFrame * aFrame,
                                      const nsPoint& aOffset,
                                      uint32_t aColor) MOZ_OVERRIDE;
  virtual void SetPaintFrameCount(bool aOn) MOZ_OVERRIDE;
  virtual bool IsPaintingFrameCounts() MOZ_OVERRIDE;
#endif

#ifdef DEBUG
  virtual void ListStyleContexts(nsIFrame *aRootFrame, FILE *out,
                                 int32_t aIndent = 0) MOZ_OVERRIDE;

  virtual void ListStyleSheets(FILE *out, int32_t aIndent = 0) MOZ_OVERRIDE;
  virtual void VerifyStyleTree() MOZ_OVERRIDE;
#endif

#ifdef PR_LOGGING
  static PRLogModuleInfo* gLog;
#endif

  virtual void DisableNonTestMouseEvents(bool aDisable) MOZ_OVERRIDE;

  virtual void UpdateCanvasBackground() MOZ_OVERRIDE;

  virtual void AddCanvasBackgroundColorItem(nsDisplayListBuilder& aBuilder,
                                            nsDisplayList& aList,
                                            nsIFrame* aFrame,
                                            const nsRect& aBounds,
                                            nscolor aBackstopColor,
                                            uint32_t aFlags) MOZ_OVERRIDE;

  virtual void AddPrintPreviewBackgroundItem(nsDisplayListBuilder& aBuilder,
                                             nsDisplayList& aList,
                                             nsIFrame* aFrame,
                                             const nsRect& aBounds) MOZ_OVERRIDE;

  virtual nscolor ComputeBackstopColor(nsView* aDisplayRoot) MOZ_OVERRIDE;

  virtual nsresult SetIsActive(bool aIsActive) MOZ_OVERRIDE;

  virtual bool GetIsViewportOverridden() MOZ_OVERRIDE { return mViewportOverridden; }

  virtual bool IsLayoutFlushObserver() MOZ_OVERRIDE
  {
    return GetPresContext()->RefreshDriver()->
      IsLayoutFlushObserver(this);
  }

  virtual void LoadComplete() MOZ_OVERRIDE;

  void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                              nsArenaMemoryStats *aArenaObjectsSize,
                              size_t *aPresShellSize,
                              size_t *aStyleSetsSize,
                              size_t *aTextRunsSize,
                              size_t *aPresContextSize) MOZ_OVERRIDE;
  size_t SizeOfTextRuns(mozilla::MallocSizeOf aMallocSizeOf) const;

  virtual void AddInvalidateHiddenPresShellObserver(nsRefreshDriver *aDriver) MOZ_OVERRIDE;

  // This data is stored as a content property (nsGkAtoms::scrolling) on
  // mContentToScrollTo when we have a pending ScrollIntoView.
  struct ScrollIntoViewData {
    ScrollAxis mContentScrollVAxis;
    ScrollAxis mContentScrollHAxis;
    uint32_t   mContentToScrollToFlags;
  };

  virtual void ScheduleImageVisibilityUpdate() MOZ_OVERRIDE;

  virtual void RebuildImageVisibilityDisplayList(const nsDisplayList& aList) MOZ_OVERRIDE;
  virtual void RebuildImageVisibility(nsRect* aRect = nullptr) MOZ_OVERRIDE;

  virtual void EnsureImageInVisibleList(nsIImageLoadingContent* aImage) MOZ_OVERRIDE;

  virtual void RemoveImageFromVisibleList(nsIImageLoadingContent* aImage) MOZ_OVERRIDE;

  virtual bool AssumeAllImagesVisible() MOZ_OVERRIDE;

  virtual void RecordShadowStyleChange(mozilla::dom::ShadowRoot* aShadowRoot) MOZ_OVERRIDE;

  virtual void DispatchAfterKeyboardEvent(nsINode* aTarget,
                                          const mozilla::WidgetKeyboardEvent& aEvent,
                                          bool aEmbeddedCancelled) MOZ_OVERRIDE;

  void SetNextPaintCompressed() { mNextPaintCompressed = true; }

protected:
  virtual ~PresShell();

  void HandlePostedReflowCallbacks(bool aInterruptible);
  void CancelPostedReflowCallbacks();

  void UnsuppressAndInvalidate();

  void WillCauseReflow() {
    nsContentUtils::AddScriptBlocker();
    ++mChangeNestCount;
  }
  nsresult DidCauseReflow();
  friend class nsAutoCauseReflowNotifier;

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
   * @param aWasInterrupted Whether or not the reflow was interrupted earlier.
   *
   */
  void     DidDoReflow(bool aInterruptible, bool aWasInterrupted);
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

  // Reflow regardless of whether the override bit has been set.
  nsresult ResizeReflowIgnoreOverride(nscoord aWidth, nscoord aHeight);

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

  friend struct AutoRenderingStateSaveRestore;
  friend struct RenderingState;

  struct RenderingState {
    explicit RenderingState(PresShell* aPresShell)
      : mXResolution(aPresShell->mXResolution)
      , mYResolution(aPresShell->mYResolution)
      , mRenderFlags(aPresShell->mRenderFlags)
    { }
    float mXResolution;
    float mYResolution;
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
      mPresShell->mXResolution = mOldState.mXResolution;
      mPresShell->mYResolution = mOldState.mYResolution;
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

  friend class nsPresShellEventCB;

  bool mCaretEnabled;
#ifdef DEBUG
  nsStyleSet* CloneStyleSet(nsStyleSet* aSet);
  bool VerifyIncrementalReflow();
  bool mInVerifyReflow;
  void ShowEventTargetDebug();
#endif

  void RecordStyleSheetChange(nsIStyleSheet* aStyleSheet);

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
                         nsRange* aRange);

  // create a RangePaintInfo for the range aRange containing the
  // display list needed to paint the range to a surface
  RangePaintInfo* CreateRangePaintInfo(nsIDOMRange* aRange,
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
   */
  mozilla::TemporaryRef<SourceSurface>
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
  void AddAuthorSheet(nsISupports* aSheet);
  void RemoveSheet(nsStyleSet::sheetType aType, nsISupports* aSheet);

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
      rv = HandleEventInternal(aEvent, aStatus);
    }
    PopCurrentEventInfo();
    return rv;
  }

  class DelayedEvent
  {
  public:
    virtual ~DelayedEvent() { }
    virtual void Dispatch() { }
  };

  class DelayedInputEvent : public DelayedEvent
  {
  public:
    virtual void Dispatch() MOZ_OVERRIDE;

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
  };

  // Check if aEvent is a mouse event and record the mouse location for later
  // synth mouse moves.
  void RecordMouseLocation(mozilla::WidgetGUIEvent* aEvent);
  class nsSynthMouseMoveEvent MOZ_FINAL : public nsARefreshObserver {
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
    NS_INLINE_DECL_REFCOUNTING(nsSynthMouseMoveEvent, MOZ_OVERRIDE)

    void Revoke() {
      if (mPresShell) {
        mPresShell->GetPresContext()->RefreshDriver()->
          RemoveRefreshObserver(this, Flush_Display);
        mPresShell = nullptr;
      }
    }
    virtual void WillRefresh(mozilla::TimeStamp aTime) MOZ_OVERRIDE {
      if (mPresShell) {
        nsRefPtr<PresShell> shell = mPresShell;
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

#ifdef ANDROID
  nsIDocument* GetTouchEventTargetDocument();
#endif
  bool InZombieDocument(nsIContent *aContent);
  already_AddRefed<nsIPresShell> GetParentPresShellForEventHandling();
  nsIContent* GetCurrentEventContent();
  nsIFrame* GetCurrentEventFrame();
  nsresult RetargetEventToParent(mozilla::WidgetGUIEvent* aEvent,
                                 nsEventStatus* aEventStatus);
  void PushCurrentEventInfo(nsIFrame* aFrame, nsIContent* aContent);
  void PopCurrentEventInfo();
  nsresult HandleEventInternal(mozilla::WidgetEvent* aEvent,
                               nsEventStatus* aStatus);
  nsresult HandlePositionedEvent(nsIFrame* aTargetFrame,
                                 mozilla::WidgetGUIEvent* aEvent,
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

  virtual void SynthesizeMouseMove(bool aFromScroll) MOZ_OVERRIDE;

  PresShell* GetRootPresShell();

  nscolor GetDefaultBackgroundColorToDraw();

  DOMHighResTimeStamp GetPerformanceNow();

  // The callback for the mPaintSuppressionTimer timer.
  static void sPaintSuppressionCallback(nsITimer* aTimer, void* aPresShell);

  // The callback for the mReflowContinueTimer timer.
  static void sReflowContinueCallback(nsITimer* aTimer, void* aPresShell);
  bool ScheduleReflowOffTimer();

  // Widget notificiations
  virtual void WindowSizeMoveDone() MOZ_OVERRIDE;
  virtual void SysColorChanged() MOZ_OVERRIDE { mPresContext->SysColorChanged(); }
  virtual void ThemeChanged() MOZ_OVERRIDE { mPresContext->ThemeChanged(); }
  virtual void BackingScaleFactorChanged() MOZ_OVERRIDE { mPresContext->UIResolutionChanged(); }

  virtual void PausePainting() MOZ_OVERRIDE;
  virtual void ResumePainting() MOZ_OVERRIDE;

  void UpdateImageVisibility();
  void UpdateActivePointerState(mozilla::WidgetGUIEvent* aEvent);

  nsRevocableEventPtr<nsRunnableMethod<PresShell> > mUpdateImageVisibilityEvent;

  void ClearVisibleImagesList(uint32_t aNonvisibleAction);
  static void ClearImageVisibilityVisited(nsView* aView, bool aClear);
  static void MarkImagesInListVisible(const nsDisplayList& aList);
  void MarkImagesInSubtreeVisible(nsIFrame* aFrame, const nsRect& aRect);

  void EvictTouches();

  // Methods for dispatching KeyboardEvent and BeforeAfterKeyboardEvent.
  void HandleKeyboardEvent(nsINode* aTarget,
                           mozilla::WidgetKeyboardEvent& aEvent,
                           bool aEmbeddedCancelled,
                           nsEventStatus* aStatus,
                           mozilla::EventDispatchingCallback* aEventCB);
  void DispatchBeforeKeyboardEventInternal(
         const nsTArray<nsCOMPtr<mozilla::dom::Element> >& aChain,
         const mozilla::WidgetKeyboardEvent& aEvent,
         size_t& aChainIndex,
         bool& aDefaultPrevented);
  void DispatchAfterKeyboardEventInternal(
         const nsTArray<nsCOMPtr<mozilla::dom::Element> >& aChain,
         const mozilla::WidgetKeyboardEvent& aEvent,
         bool aEmbeddedCancelled,
         size_t aChainIndex = 0);
  bool CanDispatchEvent(const mozilla::WidgetGUIEvent* aEvent = nullptr) const;

  // A list of images that are visible or almost visible.
  nsTHashtable< nsRefPtrHashKey<nsIImageLoadingContent> > mVisibleImages;

  nsresult SetResolutionImpl(float aXResolution, float aYResolution, bool aScaleToResolution);

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

  // mStyleSet owns it but we maintain a ref, may be null
  nsRefPtr<mozilla::CSSStyleSheet> mPrefStyleSheet;

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
  nsRefPtr<nsCaret>         mCaret;
  nsRefPtr<nsCaret>         mOriginalCaret;
  nsCallbackEventRequest*   mFirstCallbackEventRequest;
  nsCallbackEventRequest*   mLastCallbackEventRequest;

  // TouchCaret
  nsRefPtr<mozilla::TouchCaret> mTouchCaret;
  nsRefPtr<mozilla::SelectionCarets> mSelectionCarets;

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

  // This is used to protect ourselves from triggering reflow while in the
  // middle of frame construction and the like... it really shouldn't be
  // needed, one hopes, but it is for now.
  uint16_t                  mChangeNestCount;

  bool                      mDocumentLoading : 1;
  bool                      mIgnoreFrameDestruction : 1;
  bool                      mHaveShutDown : 1;
  bool                      mViewportOverridden : 1;
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

  bool                      mImageVisibilityVisited : 1;

  bool                      mNextPaintCompressed : 1;

  bool                      mHasCSSBackgroundColor : 1;

  // Whether content should be scaled by the resolution amount. If this is
  // not set, a transform that scales by the inverse of the resolution is
  // applied to rendered layers.
  bool                      mScaleToResolution : 1;

  // Whether the last chrome-only escape key event is consumed.
  bool                      mIsLastChromeOnlyEscapeKeyConsumed : 1;

  static bool               sDisableNonTestMouseEvents;
};

#endif /* !defined(nsPresShell_h_) */

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
#include "nsPresArena.h"
#include "nsFrameSelection.h"
#include "nsGUIEvent.h"
#include "nsContentUtils.h" // For AddScriptBlocker().
#include "nsRefreshDriver.h"
#include "mozilla/Attributes.h"

class nsRange;
class nsIDragService;
class nsCSSStyleSheet;

struct RangePaintInfo;
struct nsCallbackEventRequest;
#ifdef MOZ_REFLOW_PERF
class ReflowCountMgr;
#endif

class nsPresShellEventCB;
class nsAutoCauseReflowNotifier;

// 250ms.  This is actually pref-controlled, but we use this value if we fail
// to get the pref for any reason.
#define PAINTLOCK_EVENT_DELAY 250

class PresShell : public nsIPresShell,
                  public nsStubDocumentObserver,
                  public nsISelectionController, public nsIObserver,
                  public nsSupportsWeakReference
{
public:
  PresShell();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  // nsISupports
  NS_DECL_ISUPPORTS

  void Init(nsIDocument* aDocument, nsPresContext* aPresContext,
            nsViewManager* aViewManager, nsStyleSet* aStyleSet,
            nsCompatibility aCompatMode);
  virtual NS_HIDDEN_(void) Destroy() MOZ_OVERRIDE;
  virtual NS_HIDDEN_(void) MakeZombie() MOZ_OVERRIDE;

  virtual NS_HIDDEN_(nsresult) SetPreferenceStyleRules(bool aForceReflow) MOZ_OVERRIDE;

  NS_IMETHOD GetSelection(SelectionType aType, nsISelection** aSelection);
  virtual mozilla::Selection* GetCurrentSelection(SelectionType aType) MOZ_OVERRIDE;

  NS_IMETHOD SetDisplaySelection(int16_t aToggle) MOZ_OVERRIDE;
  NS_IMETHOD GetDisplaySelection(int16_t *aToggle) MOZ_OVERRIDE;
  NS_IMETHOD ScrollSelectionIntoView(SelectionType aType, SelectionRegion aRegion,
                                     int16_t aFlags) MOZ_OVERRIDE;
  NS_IMETHOD RepaintSelection(SelectionType aType) MOZ_OVERRIDE;

  virtual NS_HIDDEN_(void) BeginObservingDocument() MOZ_OVERRIDE;
  virtual NS_HIDDEN_(void) EndObservingDocument() MOZ_OVERRIDE;
  virtual NS_HIDDEN_(nsresult) Initialize(nscoord aWidth, nscoord aHeight) MOZ_OVERRIDE;
  virtual NS_HIDDEN_(nsresult) ResizeReflow(nscoord aWidth, nscoord aHeight) MOZ_OVERRIDE;
  virtual NS_HIDDEN_(nsresult) ResizeReflowOverride(nscoord aWidth, nscoord aHeight) MOZ_OVERRIDE;
  virtual NS_HIDDEN_(nsIPageSequenceFrame*) GetPageSequenceFrame() const MOZ_OVERRIDE;
  virtual NS_HIDDEN_(nsIFrame*) GetRealPrimaryFrameFor(nsIContent* aContent) const MOZ_OVERRIDE;

  virtual NS_HIDDEN_(nsIFrame*) GetPlaceholderFrameFor(nsIFrame* aFrame) const MOZ_OVERRIDE;
  virtual NS_HIDDEN_(void) FrameNeedsReflow(nsIFrame *aFrame, IntrinsicDirty aIntrinsicDirty,
                                            nsFrameState aBitToAdd) MOZ_OVERRIDE;
  virtual NS_HIDDEN_(void) FrameNeedsToContinueReflow(nsIFrame *aFrame) MOZ_OVERRIDE;
  virtual NS_HIDDEN_(void) CancelAllPendingReflows() MOZ_OVERRIDE;
  virtual NS_HIDDEN_(bool) IsSafeToFlush() const MOZ_OVERRIDE;
  virtual NS_HIDDEN_(void) FlushPendingNotifications(mozFlushType aType) MOZ_OVERRIDE;
  virtual NS_HIDDEN_(void) FlushPendingNotifications(mozilla::ChangesToFlush aType) MOZ_OVERRIDE;

  /**
   * Recreates the frames for a node
   */
  virtual NS_HIDDEN_(nsresult) RecreateFramesFor(nsIContent* aContent) MOZ_OVERRIDE;

  /**
   * Post a callback that should be handled after reflow has finished.
   */
  virtual NS_HIDDEN_(nsresult) PostReflowCallback(nsIReflowCallback* aCallback) MOZ_OVERRIDE;
  virtual NS_HIDDEN_(void) CancelReflowCallback(nsIReflowCallback* aCallback) MOZ_OVERRIDE;

  virtual NS_HIDDEN_(void) ClearFrameRefs(nsIFrame* aFrame) MOZ_OVERRIDE;
  virtual NS_HIDDEN_(already_AddRefed<nsRenderingContext>) GetReferenceRenderingContext();
  virtual NS_HIDDEN_(nsresult) GoToAnchor(const nsAString& aAnchorName, bool aScroll) MOZ_OVERRIDE;
  virtual NS_HIDDEN_(nsresult) ScrollToAnchor() MOZ_OVERRIDE;

  virtual NS_HIDDEN_(nsresult) ScrollContentIntoView(nsIContent* aContent,
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

  virtual NS_HIDDEN_(void) SetIgnoreFrameDestruction(bool aIgnore) MOZ_OVERRIDE;
  virtual NS_HIDDEN_(void) NotifyDestroyingFrame(nsIFrame* aFrame) MOZ_OVERRIDE;

  virtual NS_HIDDEN_(nsresult) CaptureHistoryState(nsILayoutHistoryState** aLayoutHistoryState) MOZ_OVERRIDE;

  virtual NS_HIDDEN_(void) UnsuppressPainting() MOZ_OVERRIDE;

  virtual nsresult GetAgentStyleSheets(nsCOMArray<nsIStyleSheet>& aSheets) MOZ_OVERRIDE;
  virtual nsresult SetAgentStyleSheets(const nsCOMArray<nsIStyleSheet>& aSheets) MOZ_OVERRIDE;

  virtual nsresult AddOverrideStyleSheet(nsIStyleSheet *aSheet) MOZ_OVERRIDE;
  virtual nsresult RemoveOverrideStyleSheet(nsIStyleSheet *aSheet) MOZ_OVERRIDE;

  virtual NS_HIDDEN_(nsresult) HandleEventWithTarget(nsEvent* aEvent, nsIFrame* aFrame,
                                                     nsIContent* aContent,
                                                     nsEventStatus* aStatus) MOZ_OVERRIDE;
  virtual NS_HIDDEN_(nsIFrame*) GetEventTargetFrame() MOZ_OVERRIDE;
  virtual NS_HIDDEN_(already_AddRefed<nsIContent>) GetEventTargetContent(nsEvent* aEvent) MOZ_OVERRIDE;


  virtual nsresult ReconstructFrames(void) MOZ_OVERRIDE;
  virtual void Freeze() MOZ_OVERRIDE;
  virtual void Thaw() MOZ_OVERRIDE;
  virtual void FireOrClearDelayedEvents(bool aFireEvents) MOZ_OVERRIDE;

  virtual NS_HIDDEN_(nsresult) RenderDocument(const nsRect& aRect, uint32_t aFlags,
                                              nscolor aBackgroundColor,
                                              gfxContext* aThebesContext) MOZ_OVERRIDE;

  virtual already_AddRefed<gfxASurface> RenderNode(nsIDOMNode* aNode,
                                                   nsIntRegion* aRegion,
                                                   nsIntPoint& aPoint,
                                                   nsIntRect* aScreenRect) MOZ_OVERRIDE;

  virtual already_AddRefed<gfxASurface> RenderSelection(nsISelection* aSelection,
                                                        nsIntPoint& aPoint,
                                                        nsIntRect* aScreenRect) MOZ_OVERRIDE;

  virtual already_AddRefed<nsPIDOMWindow> GetRootWindow() MOZ_OVERRIDE;

  virtual LayerManager* GetLayerManager() MOZ_OVERRIDE;

  virtual void SetIgnoreViewportScrolling(bool aIgnore) MOZ_OVERRIDE;

  virtual void SetDisplayPort(const nsRect& aDisplayPort);

  virtual nsresult SetResolution(float aXResolution, float aYResolution) MOZ_OVERRIDE;

  //nsIViewObserver interface

  virtual void Paint(nsView* aViewToPaint, const nsRegion& aDirtyRegion,
                     uint32_t aFlags) MOZ_OVERRIDE;
  virtual nsresult HandleEvent(nsIFrame*       aFrame,
                               nsGUIEvent*     aEvent,
                               bool            aDontRetargetEvents,
                               nsEventStatus*  aEventStatus) MOZ_OVERRIDE;
  virtual NS_HIDDEN_(nsresult) HandleDOMEventWithTarget(nsIContent* aTargetContent,
                                                        nsEvent* aEvent,
                                                        nsEventStatus* aStatus) MOZ_OVERRIDE;
  virtual NS_HIDDEN_(nsresult) HandleDOMEventWithTarget(nsIContent* aTargetContent,
                                                        nsIDOMEvent* aEvent,
                                                        nsEventStatus* aStatus) MOZ_OVERRIDE;
  virtual bool ShouldIgnoreInvalidation() MOZ_OVERRIDE;
  virtual void WillPaint() MOZ_OVERRIDE;
  virtual void WillPaintWindow() MOZ_OVERRIDE;
  virtual void DidPaintWindow() MOZ_OVERRIDE;
  virtual void ScheduleViewManagerFlush() MOZ_OVERRIDE;
  virtual void DispatchSynthMouseMove(nsGUIEvent *aEvent, bool aFlushOnHoverChange) MOZ_OVERRIDE;
  virtual void ClearMouseCaptureOnView(nsView* aView) MOZ_OVERRIDE;
  virtual bool IsVisible() MOZ_OVERRIDE;

  // caret handling
  virtual NS_HIDDEN_(already_AddRefed<nsCaret>) GetCaret() const MOZ_OVERRIDE;
  virtual NS_HIDDEN_(void) MaybeInvalidateCaretPosition() MOZ_OVERRIDE;
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
  virtual NS_HIDDEN_(void) DumpReflows() MOZ_OVERRIDE;
  virtual NS_HIDDEN_(void) CountReflows(const char * aName, nsIFrame * aFrame) MOZ_OVERRIDE;
  virtual NS_HIDDEN_(void) PaintCount(const char * aName,
                                      nsRenderingContext* aRenderingContext,
                                      nsPresContext* aPresContext,
                                      nsIFrame * aFrame,
                                      const nsPoint& aOffset,
                                      uint32_t aColor) MOZ_OVERRIDE;
  virtual NS_HIDDEN_(void) SetPaintFrameCount(bool aOn) MOZ_OVERRIDE;
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

  virtual NS_HIDDEN_(void) DisableNonTestMouseEvents(bool aDisable) MOZ_OVERRIDE;

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

  virtual NS_HIDDEN_(nsresult) SetIsActive(bool aIsActive) MOZ_OVERRIDE;

  virtual bool GetIsViewportOverridden() MOZ_OVERRIDE { return mViewportOverridden; }

  virtual bool IsLayoutFlushObserver() MOZ_OVERRIDE
  {
    return GetPresContext()->RefreshDriver()->
      IsLayoutFlushObserver(this);
  }

  void SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf,
                           nsArenaMemoryStats *aArenaObjectsSize,
                           size_t *aPresShellSize,
                           size_t *aStyleSetsSize,
                           size_t *aTextRunsSize,
                           size_t *aPresContextSize) MOZ_OVERRIDE;
  size_t SizeOfTextRuns(nsMallocSizeOfFun aMallocSizeOf) const;

  virtual void AddInvalidateHiddenPresShellObserver(nsRefreshDriver *aDriver) MOZ_OVERRIDE;


  // This data is stored as a content property (nsGkAtoms::scrolling) on
  // mContentToScrollTo when we have a pending ScrollIntoView.
  struct ScrollIntoViewData {
    ScrollAxis mContentScrollVAxis;
    ScrollAxis mContentScrollHAxis;
    uint32_t   mContentToScrollToFlags;
  };

  virtual void ScheduleImageVisibilityUpdate() MOZ_OVERRIDE;

  virtual void RebuildImageVisibility(const nsDisplayList& aList) MOZ_OVERRIDE;

  virtual void EnsureImageInVisibleList(nsIImageLoadingContent* aImage) MOZ_OVERRIDE;

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

  void DispatchTouchEvent(nsEvent *aEvent,
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
    RenderingState(PresShell* aPresShell) 
      : mXResolution(aPresShell->mXResolution)
      , mYResolution(aPresShell->mYResolution)
      , mRenderFlags(aPresShell->mRenderFlags)
    { }
    float mXResolution;
    float mYResolution;
    RenderFlags mRenderFlags;
  };

  struct AutoSaveRestoreRenderingState {
    AutoSaveRestoreRenderingState(PresShell* aPresShell)
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
  void AddAuthorSheet(nsISupports* aSheet);
  void RemoveSheet(nsStyleSet::sheetType aType, nsISupports* aSheet);

  // Hide a view if it is a popup
  void HideViewIfPopup(nsView* aView);

  // Utility method to restore the root scrollframe state
  void RestoreRootScrollPosition();

  void MaybeReleaseCapturingContent()
  {
    nsRefPtr<nsFrameSelection> frameSelection = FrameSelection();
    if (frameSelection) {
      frameSelection->SetMouseDownState(false);
    }
    if (gCaptureInfo.mContent &&
        gCaptureInfo.mContent->OwnerDoc() == mDocument) {
      SetCapturingContent(nullptr, 0);
    }
  }

  nsresult HandleRetargetedEvent(nsEvent* aEvent, nsEventStatus* aStatus, nsIContent* aTarget)
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
      mEvent->modifiers = aEvent->modifiers;
    }

    nsDelayedInputEvent()
    : nsDelayedEvent(), mEvent(nullptr) {}

    nsInputEvent* mEvent;
  };

  class nsDelayedMouseEvent : public nsDelayedInputEvent
  {
  public:
    nsDelayedMouseEvent(nsMouseEvent* aEvent) : nsDelayedInputEvent()
    {
      mEvent = new nsMouseEvent(aEvent->mFlags.mIsTrusted,
                                aEvent->message,
                                aEvent->widget,
                                aEvent->reason,
                                aEvent->context);
      Init(aEvent);
      static_cast<nsMouseEvent*>(mEvent)->clickCount = aEvent->clickCount;
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
      mEvent = new nsKeyEvent(aEvent->mFlags.mIsTrusted,
                              aEvent->message,
                              aEvent->widget);
      Init(aEvent);
      static_cast<nsKeyEvent*>(mEvent)->keyCode = aEvent->keyCode;
      static_cast<nsKeyEvent*>(mEvent)->charCode = aEvent->charCode;
      static_cast<nsKeyEvent*>(mEvent)->alternativeCharCodes =
        aEvent->alternativeCharCodes;
      static_cast<nsKeyEvent*>(mEvent)->isChar = aEvent->isChar;
    }

    virtual ~nsDelayedKeyEvent()
    {
      delete static_cast<nsKeyEvent*>(mEvent);
    }
  };

  // Check if aEvent is a mouse event and record the mouse location for later
  // synth mouse moves.
  void RecordMouseLocation(nsGUIEvent* aEvent);
  class nsSynthMouseMoveEvent MOZ_FINAL : public nsARefreshObserver {
  public:
    nsSynthMouseMoveEvent(PresShell* aPresShell, bool aFromScroll)
      : mPresShell(aPresShell), mFromScroll(aFromScroll) {
      NS_ASSERTION(mPresShell, "null parameter");
    }
    ~nsSynthMouseMoveEvent() {
      Revoke();
    }

    NS_INLINE_DECL_REFCOUNTING(nsSynthMouseMoveEvent)
    
    void Revoke() {
      if (mPresShell) {
        mPresShell->GetPresContext()->RefreshDriver()->
          RemoveRefreshObserver(this, Flush_Display);
        mPresShell = nullptr;
      }
    }
    virtual void WillRefresh(mozilla::TimeStamp aTime) MOZ_OVERRIDE {
      if (mPresShell)
        mPresShell->ProcessSynthMouseMoveEvent(mFromScroll);
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
  already_AddRefed<nsIPresShell> GetParentPresShell();
  nsIContent* GetCurrentEventContent();
  nsIFrame* GetCurrentEventFrame();
  nsresult RetargetEventToParent(nsGUIEvent* aEvent,
                                 nsEventStatus*  aEventStatus);
  void PushCurrentEventInfo(nsIFrame* aFrame, nsIContent* aContent);
  void PopCurrentEventInfo();
  nsresult HandleEventInternal(nsEvent* aEvent, nsEventStatus *aStatus);
  nsresult HandlePositionedEvent(nsIFrame*      aTargetFrame,
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
  bool AdjustContextMenuKeyEvent(nsMouseEvent* aEvent);

  // 
  bool PrepareToUseCaretPosition(nsIWidget* aEventWidget, nsIntPoint& aTargetPt);

  // Get the selected item and coordinates in device pixels relative to root
  // document's root view for element, first ensuring the element is onscreen
  void GetCurrentItemAndPositionForElement(nsIDOMElement *aCurrentEl,
                                           nsIContent **aTargetToUse,
                                           nsIntPoint& aTargetPt,
                                           nsIWidget *aRootWidget);

  void FireResizeEvent();
  void FireBeforeResizeEvent();
  static void AsyncResizeEventCallback(nsITimer* aTimer, void* aPresShell);

  virtual void SynthesizeMouseMove(bool aFromScroll) MOZ_OVERRIDE;

  PresShell* GetRootPresShell();

  nscolor GetDefaultBackgroundColorToDraw();

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

  void UpdateImageVisibility();

  nsRevocableEventPtr<nsRunnableMethod<PresShell> > mUpdateImageVisibilityEvent;

  void ClearVisibleImagesList();
  static void ClearImageVisibilityVisited(nsView* aView, bool aClear);
  static void MarkImagesInListVisible(const nsDisplayList& aList);

  // A list of images that are visible or almost visible.
  nsTArray< nsCOMPtr<nsIImageLoadingContent > > mVisibleImages;

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
  nsRefPtr<nsCSSStyleSheet> mPrefStyleSheet; 

  // Set of frames that we should mark with NS_FRAME_HAS_DIRTY_CHILDREN after
  // we finish reflowing mCurrentReflowRoot.
  nsTHashtable<nsPtrHashKey<nsIFrame> > mFramesToDirty;

  // Reflow roots that need to be reflowed.
  nsTArray<nsIFrame*>       mDirtyRoots;

  nsTArray<nsAutoPtr<nsDelayedEvent> > mDelayedEvents;
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

  // This timer controls painting suppression.  Until it fires
  // or all frames are constructed, we won't paint anything but
  // our <body> background and scrollbars.
  nsCOMPtr<nsITimer>        mPaintSuppressionTimer;

  // At least on Win32 and Mac after interupting a reflow we need to post
  // the resume reflow event off a timer to avoid event starvation because
  // posted messages are processed before other messages when the modal
  // moving/sizing loop is running, see bug 491700 for details.
  nsCOMPtr<nsITimer>        mReflowContinueTimer;

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
  bool                      mLastRootReflowHadUnconstrainedHeight : 1;
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

  static bool               sDisableNonTestMouseEvents;
};

#endif /* !defined(nsPresShell_h_) */

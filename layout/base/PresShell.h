/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* a presentation of a document, part 2 */

#ifndef mozilla_PresShell_h
#define mozilla_PresShell_h

#include "mozilla/PresShellForwards.h"
#include "nsIPresShell.h"

#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "mozilla/dom/HTMLDocumentBinding.h"
#include "mozilla/layers/FocusTarget.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ScrollTypes.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsContentUtils.h"  // For AddScriptBlocker().
#include "nsCRT.h"
#include "nsIObserver.h"
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

class nsPresShellEventCB;
class AutoPointerEventTargetUpdater;
class MobileViewportManager;

namespace mozilla {

namespace dom {
class Element;
class Selection;
}  // namespace dom

class EventDispatchingCallback;
class GeckoMVMContext;
class OverflowChangedTracker;

// 039d8ffc-fa55-42d7-a53a-388cb129b052
#define NS_PRESSHELL_IID                             \
  {                                                  \
    0x039d8ffc, 0xfa55, 0x42d7, {                    \
      0xa5, 0x3a, 0x38, 0x8c, 0xb1, 0x29, 0xb0, 0x52 \
    }                                                \
  }

class PresShell final : public nsIPresShell,
                        public nsISelectionController,
                        public nsIObserver,
                        public nsSupportsWeakReference {
  typedef layers::FocusTarget FocusTarget;
  typedef dom::Element Element;

  // A set type for tracking visible frames, for use by the visibility code in
  // PresShell. The set contains nsIFrame* pointers.
  typedef nsTHashtable<nsPtrHashKey<nsIFrame>> VisibleFrames;

 public:
  PresShell();

  // nsISupports
  NS_DECL_ISUPPORTS

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_PRESSHELL_IID)

  static bool AccessibleCaretEnabled(nsIDocShell* aDocShell);

  /**
   * Return the active content currently capturing the mouse if any.
   */
  static nsIContent* GetCapturingContent() {
    return sCapturingContentInfo.mContent;
  }

  /**
   * Allow or disallow mouse capturing.
   */
  static void AllowMouseCapture(bool aAllowed) {
    sCapturingContentInfo.mAllowed = aAllowed;
  }

  /**
   * Returns true if there is an active mouse capture that wants to prevent
   * drags.
   */
  static bool IsMouseCapturePreventingDrag() {
    return sCapturingContentInfo.mPreventDrag && sCapturingContentInfo.mContent;
  }

  static void ClearMouseCaptureOnView(nsView* aView);

  // If a frame in the subtree rooted at aFrame is capturing the mouse then
  // clears that capture.
  static void ClearMouseCapture(nsIFrame* aFrame);

#ifdef ACCESSIBILITY
  /**
   * Return the document accessible for this PresShell if there is one.
   */
  a11y::DocAccessible* GetDocAccessible() const { return mDocAccessible; }

  /**
   * Set the document accessible for this PresShell.
   */
  void SetDocAccessible(a11y::DocAccessible* aDocAccessible) {
    mDocAccessible = aDocAccessible;
  }

  /**
   * Return true if accessibility is active.
   */
  static bool IsAccessibilityActive();

  /**
   * Return accessibility service if accessibility is active.
   */
  static nsAccessibilityService* GetAccessibilityService();
#endif  // #ifdef ACCESSIBILITY

  void Init(Document*, nsPresContext*, nsViewManager*);
  void Destroy() override;

  NS_IMETHOD GetSelectionFromScript(RawSelectionType aRawSelectionType,
                                    dom::Selection** aSelection) override;
  dom::Selection* GetSelection(RawSelectionType aRawSelectionType) override;

  dom::Selection* GetCurrentSelection(SelectionType aSelectionType) override;

  already_AddRefed<nsISelectionController>
  GetSelectionControllerForFocusedContent(
      nsIContent** aFocusedContent = nullptr) override;

  NS_IMETHOD SetDisplaySelection(int16_t aToggle) override;
  NS_IMETHOD GetDisplaySelection(int16_t* aToggle) override;
  NS_IMETHOD ScrollSelectionIntoView(RawSelectionType aRawSelectionType,
                                     SelectionRegion aRegion,
                                     int16_t aFlags) override;
  NS_IMETHOD RepaintSelection(RawSelectionType aRawSelectionType) override;

  nsresult Initialize() override;
  MOZ_CAN_RUN_SCRIPT nsresult ResizeReflow(
      nscoord aWidth, nscoord aHeight, nscoord aOldWidth = 0,
      nscoord aOldHeight = 0,
      ResizeReflowOptions aOptions = ResizeReflowOptions::NoOption) override;
  MOZ_CAN_RUN_SCRIPT nsresult ResizeReflowIgnoreOverride(
      nscoord aWidth, nscoord aHeight, nscoord aOldWidth, nscoord aOldHeight,
      ResizeReflowOptions aOptions = ResizeReflowOptions::NoOption) override;

  RectVisibility GetRectVisibility(nsIFrame* aFrame, const nsRect& aRect,
                                   nscoord aMinTwips) const override;

  nsresult CaptureHistoryState(
      nsILayoutHistoryState** aLayoutHistoryState) override;

  void UnsuppressPainting() override;

  nsresult AddOverrideStyleSheet(StyleSheet*) override;
  nsresult RemoveOverrideStyleSheet(StyleSheet*) override;

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  nsresult HandleEventWithTarget(
      WidgetEvent* aEvent, nsIFrame* aFrame, nsIContent* aContent,
      nsEventStatus* aEventStatus, bool aIsHandlingNativeEvent = false,
      nsIContent** aTargetContent = nullptr,
      nsIContent* aOverrideClickTarget = nullptr) override {
    MOZ_ASSERT(aEvent);
    EventHandler eventHandler(*this);
    return eventHandler.HandleEventWithTarget(
        aEvent, aFrame, aContent, aEventStatus, aIsHandlingNativeEvent,
        aTargetContent, aOverrideClickTarget);
  }

  void ReconstructFrames(void) override;
  void Freeze() override;
  void Thaw() override;
  void FireOrClearDelayedEvents(bool aFireEvents) override;

  nsresult RenderDocument(const nsRect& aRect, RenderDocumentFlags aFlags,
                          nscolor aBackgroundColor,
                          gfxContext* aThebesContext) override;

  already_AddRefed<SourceSurface> RenderNode(nsINode* aNode,
                                             const Maybe<CSSIntRegion>& aRegion,
                                             const LayoutDeviceIntPoint aPoint,
                                             LayoutDeviceIntRect* aScreenRect,
                                             RenderImageFlags aFlags) override;

  already_AddRefed<SourceSurface> RenderSelection(
      dom::Selection* aSelection, const LayoutDeviceIntPoint aPoint,
      LayoutDeviceIntRect* aScreenRect, RenderImageFlags aFlags) override;

  already_AddRefed<nsPIDOMWindowOuter> GetRootWindow() override;

  already_AddRefed<nsPIDOMWindowOuter> GetFocusedDOMWindowInOurWindow()
      override;

  LayerManager* GetLayerManager() override;

  bool AsyncPanZoomEnabled() override;

  void SetIgnoreViewportScrolling(bool aIgnore) override;

  /**
   * Set a "resolution" for the document, which if not 1.0 will
   * allocate more or fewer pixels for rescalable content by a factor
   * of |resolution| in both dimensions.  Return NS_OK iff the
   * resolution bounds are sane, and the resolution of this was
   * actually updated.
   *
   * Also increase the scale of the content by the same amount
   * (that's the "AndScaleTo" part).
   *
   * The resolution defaults to 1.0.
   *
   * |aOrigin| specifies who originated the resolution change. For changes
   * sent by APZ, pass ResolutionChangeOrigin::Apz. For changes sent by
   * the main thread, use pass ResolutionChangeOrigin::MainThread (similar
   * to the |aOrigin| parameter of nsIScrollableFrame::ScrollToCSSPixels()).
   */
  nsresult SetResolutionAndScaleTo(float aResolution,
                                   ResolutionChangeOrigin aOrigin);

  float GetCumulativeResolution() override;
  float GetCumulativeNonRootScaleResolution() override;
  void SetRestoreResolution(float aResolution,
                            LayoutDeviceIntSize aDisplaySize) override;

  // Widget notificiations
  void WindowSizeMoveDone();
  void SysColorChanged() { mPresContext->SysColorChanged(); }
  void ThemeChanged() { mPresContext->ThemeChanged(); }
  void BackingScaleFactorChanged() { mPresContext->UIResolutionChangedSync(); }

  void SynthesizeMouseMove(bool aFromScroll) override;

  Document* GetPrimaryContentDocument() override;

  void PausePainting() override;
  void ResumePainting() override;

  // nsIViewObserver interface

  void Paint(nsView* aViewToPaint, const nsRegion& aDirtyRegion,
             PaintFlags aFlags);

  MOZ_CAN_RUN_SCRIPT nsresult HandleEvent(nsIFrame* aFrameForPresShell,
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
  /**
   * Notify that we're going to call Paint with PaintFlags::PaintLayers
   * on the pres shell for a widget (which might not be this one, since
   * WillPaint is called on all presshells in the same toplevel window as the
   * painted widget). This is issued at a time when it's safe to modify
   * widget geometry.
   */
  MOZ_CAN_RUN_SCRIPT void WillPaint();
  void WillPaintWindow() override;
  void DidPaintWindow() override;

  /**
   * Ensures that the refresh driver is running, and schedules a view
   * manager flush on the next tick.
   *
   * @param aType PaintType::DelayedCompress : Schedule a paint to be executed
   * after a delay, and put FrameLayerBuilder in 'compressed' mode that avoids
   * short cut optimizations.
   */
  void ScheduleViewManagerFlush(PaintType aType = PaintType::Default);

  bool IsVisible() override;
  void SuppressDisplayport(bool aEnabled) override;
  void RespectDisplayportSuppression(bool aEnabled) override;
  bool IsDisplayportSuppressed() override;

  // caret handling
  NS_IMETHOD SetCaretEnabled(bool aInEnable) override;
  NS_IMETHOD SetCaretReadOnly(bool aReadOnly) override;
  NS_IMETHOD GetCaretEnabled(bool* aOutEnabled) override;
  NS_IMETHOD SetCaretVisibilityDuringSelection(bool aVisibility) override;
  NS_IMETHOD GetCaretVisible(bool* _retval) override;

  NS_IMETHOD SetSelectionFlags(int16_t aInEnable) override;
  NS_IMETHOD GetSelectionFlags(int16_t* aOutEnable) override;

  using nsIPresShell::GetSelectionFlags;

  // nsISelectionController

  NS_IMETHOD PhysicalMove(int16_t aDirection, int16_t aAmount,
                          bool aExtend) override;
  NS_IMETHOD CharacterMove(bool aForward, bool aExtend) override;
  NS_IMETHOD CharacterExtendForDelete() override;
  NS_IMETHOD CharacterExtendForBackspace() override;
  NS_IMETHOD WordMove(bool aForward, bool aExtend) override;
  NS_IMETHOD WordExtendForDelete(bool aForward) override;
  NS_IMETHOD LineMove(bool aForward, bool aExtend) override;
  NS_IMETHOD IntraLineMove(bool aForward, bool aExtend) override;
  MOZ_CAN_RUN_SCRIPT
  NS_IMETHOD PageMove(bool aForward, bool aExtend) override;
  NS_IMETHOD ScrollPage(bool aForward) override;
  NS_IMETHOD ScrollLine(bool aForward) override;
  NS_IMETHOD ScrollCharacter(bool aRight) override;
  NS_IMETHOD CompleteScroll(bool aForward) override;
  NS_IMETHOD CompleteMove(bool aForward, bool aExtend) override;
  NS_IMETHOD SelectAll() override;
  NS_IMETHOD CheckVisibility(nsINode* node, int16_t startOffset,
                             int16_t EndOffset, bool* _retval) override;
  nsresult CheckVisibilityContent(nsIContent* aNode, int16_t aStartOffset,
                                  int16_t aEndOffset, bool* aRetval) override;

  // Notifies that the state of the document has changed.
  void DocumentStatesChanged(EventStates);

  // nsIDocumentObserver
  NS_DECL_NSIDOCUMENTOBSERVER_BEGINLOAD
  NS_DECL_NSIDOCUMENTOBSERVER_ENDLOAD
  NS_DECL_NSIDOCUMENTOBSERVER_CONTENTSTATECHANGED

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTEWILLCHANGE
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  NS_DECL_NSIOBSERVER

  // Inline methods defined in PresShellInlines.h
  inline void EnsureStyleFlush();
  inline void SetNeedStyleFlush();
  inline void SetNeedLayoutFlush();
  inline void SetNeedThrottledAnimationFlush();
  inline ServoStyleSet* StyleSet() const;

  /**
   * Whether we might need a flush for the given flush type.  If this
   * function returns false, we definitely don't need to flush.
   *
   * @param aFlushType The flush type to check.  This must be
   *   >= FlushType::Style.  This also returns true if a throttled
   *   animation flush is required.
   */
  bool NeedFlush(FlushType aType) const {
    // We check mInFlush to handle re-entrant calls to FlushPendingNotifications
    // by reporting that we always need a flush in that case.  Otherwise,
    // we could end up missing needed flushes, since we clear the mNeedXXXFlush
    // flags at the top of FlushPendingNotifications.
    MOZ_ASSERT(aType >= FlushType::Style);
    return mNeedStyleFlush ||
           (mNeedLayoutFlush && aType >= FlushType::InterruptibleLayout) ||
           aType >= FlushType::Display || mNeedThrottledAnimationFlush ||
           mInFlush;
  }

  /**
   * Returns true if we might need to flush layout, even if we haven't scheduled
   * one yet (as opposed to HasPendingReflow, which returns true if a flush is
   * scheduled or will soon be scheduled).
   */
  bool NeedLayoutFlush() const { return mNeedLayoutFlush; }

  bool NeedStyleFlush() const { return mNeedStyleFlush; }

  /**
   * Flush pending notifications of the type specified.  This method
   * will not affect the content model; it'll just affect style and
   * frames. Callers that actually want up-to-date presentation (other
   * than the document itself) should probably be calling
   * Document::FlushPendingNotifications.
   *
   * This method can execute script, which can destroy this presshell object
   * unless someone is holding a reference to it on the stack.  The presshell
   * itself will ensure it lives up until the method returns, but callers who
   * plan to use the presshell after this call should hold a strong ref
   * themselves!
   *
   * @param aType the type of notifications to flush
   */
  MOZ_CAN_RUN_SCRIPT
  void FlushPendingNotifications(FlushType aType) {
    if (!NeedFlush(aType)) {
      return;
    }

    DoFlushPendingNotifications(aType);
  }

  MOZ_CAN_RUN_SCRIPT
  void FlushPendingNotifications(ChangesToFlush aType) {
    if (!NeedFlush(aType.mFlushType)) {
      return;
    }

    DoFlushPendingNotifications(aType);
  }

  /**
   * Tell the pres shell that a frame needs to be marked dirty and needs
   * Reflow.  It's OK if this is an ancestor of the frame needing reflow as
   * long as the ancestor chain between them doesn't cross a reflow root.
   *
   * The bit to add should be NS_FRAME_IS_DIRTY, NS_FRAME_HAS_DIRTY_CHILDREN
   * or nsFrameState(0); passing 0 means that dirty bits won't be set on the
   * frame or its ancestors/descendants, but that intrinsic widths will still
   * be marked dirty.  Passing aIntrinsicDirty = eResize and aBitToAdd = 0
   * would result in no work being done, so don't do that.
   */
  void FrameNeedsReflow(
      nsIFrame* aFrame, IntrinsicDirty aIntrinsicDirty, nsFrameState aBitToAdd,
      ReflowRootHandling aRootHandling = ReflowRootHandling::InferFromBitToAdd);

  /**
   * Calls FrameNeedsReflow on all fixed position children of the root frame.
   */
  void MarkFixedFramesForReflow(IntrinsicDirty aIntrinsicDirty);

  // This function handles all the work after VisualViewportSize is set
  // or reset.
  void CompleteChangeToVisualViewportSize();

  /**
   * The return value indicates whether the offset actually changed.
   */
  bool SetVisualViewportOffset(const nsPoint& aScrollOffset,
                               const nsPoint& aPrevLayoutScrollPos);

  nsPoint GetVisualViewportOffset() const {
    return mVisualViewportOffset.valueOr(nsPoint());
  }
  bool IsVisualViewportOffsetSet() const {
    return mVisualViewportOffset.isSome();
  }

  void SetVisualViewportSize(nscoord aWidth, nscoord aHeight);
  void ResetVisualViewportSize();
  bool IsVisualViewportSizeSet() { return mVisualViewportSizeSet; }
  nsSize GetVisualViewportSize() {
    NS_ASSERTION(mVisualViewportSizeSet,
                 "asking for visual viewport size when its not set?");
    return mVisualViewportSize;
  }

  nsPoint GetVisualViewportOffsetRelativeToLayoutViewport() const;

  /* Enable/disable author style level. Disabling author style disables the
   * entire author level of the cascade, including the HTML preshint level.
   */
  // XXX these could easily be inlined, but there is a circular #include
  // problem with nsStyleSet.
  void SetAuthorStyleDisabled(bool aDisabled);
  bool GetAuthorStyleDisabled() const;

  /**
   * Update the style set somehow to take into account changed prefs which
   * affect document styling.
   */
  void UpdatePreferenceStyles();

  // aSheetType is one of the nsIStyleSheetService *_SHEET constants.
  void NotifyStyleSheetServiceSheetAdded(StyleSheet* aSheet,
                                         uint32_t aSheetType);
  void NotifyStyleSheetServiceSheetRemoved(StyleSheet* aSheet,
                                           uint32_t aSheetType);

  // DoReflow returns whether the reflow finished without interruption
  // If aFrame is not the root frame, the caller must pass a non-null
  // aOverflowTracker.
  bool DoReflow(nsIFrame* aFrame, bool aInterruptible,
                OverflowChangedTracker* aOverflowTracker);

#ifdef MOZ_REFLOW_PERF
  void DumpReflows() override;
  void CountReflows(const char* aName, nsIFrame* aFrame) override;
  void PaintCount(const char* aName, gfxContext* aRenderingContext,
                  nsPresContext* aPresContext, nsIFrame* aFrame,
                  const nsPoint& aOffset, uint32_t aColor) override;
  void SetPaintFrameCount(bool aOn) override;
  bool IsPaintingFrameCounts() override;
#endif

#ifdef DEBUG
  void ListComputedStyles(FILE* out, int32_t aIndent = 0) override;

  void ListStyleSheets(FILE* out, int32_t aIndent = 0) override;
#endif

  void DisableNonTestMouseEvents(bool aDisable) override;

  void UpdateCanvasBackground() override;

  /**
   * Add a solid color item to the bottom of aList with frame aFrame and bounds
   * aBounds. Checks first if this needs to be done by checking if aFrame is a
   * canvas frame (if the AddCanvasBackgroundColorFlags::ForceDraw is passed
   * then this check is skipped). aBackstopColor is composed behind the
   * background color of the canvas, it is transparent by default.
   *
   * We attempt to make the background color part of the scrolled canvas (to
   * reduce transparent layers), and if async scrolling is enabled (and the
   * background is opaque) then we add a second, unscrolled item to handle the
   * checkerboarding case.
   *
   * AddCanvasBackgroundColorFlags::AddSubDocument should be specified when
   * calling this for a subdocument, and LayoutUseContainersForRootFrame might
   * cause the whole list to be scrolled. In that case the second unscrolled
   * item will be elided.
   *
   * AddCanvasBackgroundColorFlags::AppendUnscrolledOnly only attempts to add
   * the unscrolled item, so that we can add it manually after
   * LayoutUseContainersForRootFrame has built the scrolling ContainerLayer.
   */
  void AddCanvasBackgroundColorItem(
      nsDisplayListBuilder& aBuilder, nsDisplayList& aList, nsIFrame* aFrame,
      const nsRect& aBounds, nscolor aBackstopColor = NS_RGBA(0, 0, 0, 0),
      AddCanvasBackgroundColorFlags aFlags =
          AddCanvasBackgroundColorFlags::None);

  void AddPrintPreviewBackgroundItem(nsDisplayListBuilder& aBuilder,
                                     nsDisplayList& aList, nsIFrame* aFrame,
                                     const nsRect& aBounds) override;

  nscolor ComputeBackstopColor(nsView* aDisplayRoot) override;

  nsresult SetIsActive(bool aIsActive) override;

  bool GetIsViewportOverridden() override {
    return (mMobileViewportManager != nullptr);
  }

  RefPtr<MobileViewportManager> GetMobileViewportManager() const override;

  void UpdateViewportOverridden(bool aAfterInitialization) override;

  bool IsLayoutFlushObserver() override {
    return GetPresContext()->RefreshDriver()->IsLayoutFlushObserver(this);
  }

  void LoadComplete() override;

  void AddSizeOfIncludingThis(nsWindowSizes& aWindowSizes) const override;
  size_t SizeOfTextRuns(MallocSizeOf aMallocSizeOf) const;

  //////////////////////////////////////////////////////////////////////////////
  // Approximate frame visibility tracking public API.
  //////////////////////////////////////////////////////////////////////////////

  void ScheduleApproximateFrameVisibilityUpdateSoon() override;
  void ScheduleApproximateFrameVisibilityUpdateNow() override;

  void RebuildApproximateFrameVisibilityDisplayList(
      const nsDisplayList& aList) override;
  void RebuildApproximateFrameVisibility(nsRect* aRect = nullptr,
                                         bool aRemoveOnly = false) override;

  void EnsureFrameInApproximatelyVisibleList(nsIFrame* aFrame) override;
  void RemoveFrameFromApproximatelyVisibleList(nsIFrame* aFrame) override;

  bool AssumeAllFramesVisible() override;

  bool CanDispatchEvent(const WidgetGUIEvent* aEvent = nullptr) const override;

  void SetNextPaintCompressed() { mNextPaintCompressed = true; }

  bool HasHandledUserInput() const override { return mHasHandledUserInput; }

  void FireResizeEvent() override;

  void SetKeyPressEventModel(uint16_t aKeyPressEventModel) override {
    mForceUseLegacyKeyCodeAndCharCodeValues |=
        aKeyPressEventModel ==
        dom::HTMLDocument_Binding::KEYPRESS_EVENT_MODEL_SPLIT;
  }

  static PresShell* GetShellForEventTarget(nsIFrame* aFrame,
                                           nsIContent* aContent);
  static PresShell* GetShellForTouchEvent(WidgetGUIEvent* aEvent);

  /**
   * Informs the pres shell that the document is now at the anchor with
   * the given name.  If |aScroll| is true, scrolls the view of the
   * document so that the anchor with the specified name is displayed at
   * the top of the window.  If |aAnchorName| is empty, then this informs
   * the pres shell that there is no current target, and |aScroll| must
   * be false.  If |aAdditionalScrollFlags| is ScrollFlags::ScrollSmoothAuto
   * and |aScroll| is true, the scrolling may be performed with an animation.
   */
  MOZ_CAN_RUN_SCRIPT
  nsresult GoToAnchor(const nsAString& aAnchorName, bool aScroll,
                      ScrollFlags aAdditionalScrollFlags = ScrollFlags::None);

  /**
   * Tells the presshell to scroll again to the last anchor scrolled to by
   * GoToAnchor, if any. This scroll only happens if the scroll
   * position has not changed since the last GoToAnchor. This is called
   * by nsDocumentViewer::LoadComplete. This clears the last anchor
   * scrolled to by GoToAnchor (we don't want to keep it alive if it's
   * removed from the DOM), so don't call this more than once.
   */
  MOZ_CAN_RUN_SCRIPT nsresult ScrollToAnchor();

  /**
   * Scrolls the view of the document so that the primary frame of the content
   * is displayed in the window. Layout is flushed before scrolling.
   *
   * @param aContent  The content object of which primary frame should be
   *                  scrolled into view.
   * @param aVertical How to align the frame vertically and when to do so.
   *                  This is a ScrollAxis of Where and When.
   * @param aHorizontal How to align the frame horizontally and when to do so.
   *                  This is a ScrollAxis of Where and When.
   * @param aScrollFlags  If ScrollFlags::ScrollFirstAncestorOnly is set,
   *                      only the nearest scrollable ancestor is scrolled,
   *                      otherwise all scrollable ancestors may be scrolled
   *                      if necessary.  If ScrollFlags::ScrollOverflowHidden
   *                      is set then we may scroll in a direction even if
   *                      overflow:hidden is specified in that direction;
   *                      otherwise we will not scroll in that direction when
   *                      overflow:hidden is set for that direction.  If
   *                      ScrollFlags::ScrollNoParentFrames is set then we
   *                      only scroll nodes in this document, not in any
   *                      parent documents which contain this document in a
   *                      iframe or the like.  If ScrollFlags::ScrollSmooth
   *                      is set and CSSOM-VIEW scroll-behavior is enabled,
   *                      we will scroll smoothly using
   *                      nsIScrollableFrame::ScrollMode::SMOOTH_MSD;
   *                      otherwise, nsIScrollableFrame::ScrollMode::INSTANT
   *                      will be used.  If ScrollFlags::ScrollSmoothAuto is
   *                      set, the CSSOM-View scroll-behavior attribute is
   *                      set to 'smooth' on the scroll frame, and CSSOM-VIEW
   *                      scroll-behavior is enabled, we will scroll smoothly
   *                      using nsIScrollableFrame::ScrollMode::SMOOTH_MSD;
   *                      otherwise, nsIScrollableFrame::ScrollMode::INSTANT
   *                      will be used.
   */
  MOZ_CAN_RUN_SCRIPT
  nsresult ScrollContentIntoView(nsIContent* aContent, ScrollAxis aVertical,
                                 ScrollAxis aHorizontal,
                                 ScrollFlags aScrollFlags);

  /**
   * When capturing content is set, it traps all mouse events and retargets
   * them at this content node. If capturing is not allowed
   * (gCaptureInfo.mAllowed is false), then capturing is not set. However, if
   * the CaptureFlags::IgnoreAllowedState is set, the allowed state is ignored
   * and capturing is set regardless. To disable capture, pass null for the
   * value of aContent.
   *
   * If CaptureFlags::RetargetedToElement is set, all mouse events are
   * targeted at aContent only. Otherwise, mouse events are targeted at
   * aContent or its descendants. That is, descendants of aContent receive
   * mouse events as they normally would, but mouse events outside of aContent
   * are retargeted to aContent.
   *
   * If CaptureFlags::PreventDragStart is set then drags are prevented from
   * starting while this capture is active.
   *
   * If CaptureFlags::PointerLock is set, similar to
   * CaptureFlags::RetargetToElement, then events are targeted at aContent,
   * but capturing is held more strongly (i.e., calls to SetCapturingContent()
   * won't unlock unless CaptureFlags::PointerLock is set again).
   */
  static void SetCapturingContent(nsIContent* aContent, CaptureFlags aFlags);

  /**
   * Alias for SetCapturingContent(nullptr, CaptureFlags::None) for making
   * callers what they do clearer.
   */
  static void ReleaseCapturingContent() {
    PresShell::SetCapturingContent(nullptr, CaptureFlags::None);
  }

  // Called at the end of nsLayoutUtils::PaintFrame().
  // This is used to clear any pending visual scroll updates that have been
  // acknowledged, to make sure they don't stick around for the next paint.
  void EndPaint();

  /**
   * Tell the presshell that the given frame's reflow was interrupted.  This
   * will mark as having dirty children a path from the given frame (inclusive)
   * to the nearest ancestor with a dirty subtree, or to the reflow root
   * currently being reflowed if no such ancestor exists (inclusive).  This is
   * to be done immediately after reflow of the current reflow root completes.
   * This method must only be called during reflow, and the frame it's being
   * called on must be in the process of being reflowed when it's called.  This
   * method doesn't mark any intrinsic widths dirty and doesn't add any bits
   * other than NS_FRAME_HAS_DIRTY_CHILDREN.
   */
  void FrameNeedsToContinueReflow(nsIFrame* aFrame);

  /**
   * Notification sent by a frame informing the pres shell that it is about to
   * be destroyed.
   * This allows any outstanding references to the frame to be cleaned up
   */
  void NotifyDestroyingFrame(nsIFrame* aFrame);

#ifdef DEBUG
  nsIFrame* GetDrawEventTargetFrame() { return mDrawEventTargetFrame; }
#endif

 private:
  ~PresShell();

  friend class ::AutoPointerEventTargetUpdater;

  // ProcessReflowCommands returns whether we processed all our dirty roots
  // without interruptions.
  MOZ_CAN_RUN_SCRIPT bool ProcessReflowCommands(bool aInterruptible);

  /**
   * Callback handler for whether reflow happened.
   *
   * @param aInterruptible Whether or not reflow interruption is allowed.
   */
  MOZ_CAN_RUN_SCRIPT void DidDoReflow(bool aInterruptible);

  MOZ_CAN_RUN_SCRIPT void HandlePostedReflowCallbacks(bool aInterruptible);

  /**
   * Helper for ScrollContentIntoView()
   */
  MOZ_CAN_RUN_SCRIPT void DoScrollContentIntoView();

  /**
   * Methods to handle changes to user and UA sheet lists that we get
   * notified about.
   */
  void AddUserSheet(StyleSheet*);
  void AddAgentSheet(StyleSheet*);
  void AddAuthorSheet(StyleSheet*);
  void RemoveSheet(StyleOrigin, StyleSheet*);
  void RemovePreferenceStyles();

  /**
   * Initialize cached font inflation preference values and do an initial
   * computation to determine if font inflation is enabled.
   *
   * @see nsLayoutUtils::sFontSizeInflationEmPerLine
   * @see nsLayoutUtils::sFontSizeInflationMinTwips
   * @see nsLayoutUtils::sFontSizeInflationLineThreshold
   */
  void SetupFontInflation();

  /**
   * Implementation methods for FlushPendingNotifications.
   */
  MOZ_CAN_RUN_SCRIPT void DoFlushPendingNotifications(FlushType aType);
  MOZ_CAN_RUN_SCRIPT void DoFlushPendingNotifications(ChangesToFlush aType);

  struct RenderingState {
    explicit RenderingState(PresShell* aPresShell)
        : mResolution(aPresShell->mResolution),
          mRenderFlags(aPresShell->mRenderFlags) {}
    Maybe<float> mResolution;
    RenderFlags mRenderFlags;
  };

  struct AutoSaveRestoreRenderingState {
    explicit AutoSaveRestoreRenderingState(PresShell* aPresShell)
        : mPresShell(aPresShell), mOldState(aPresShell) {}

    ~AutoSaveRestoreRenderingState() {
      mPresShell->mRenderFlags = mOldState.mRenderFlags;
      mPresShell->mResolution = mOldState.mResolution;
    }

    PresShell* mPresShell;
    RenderingState mOldState;
  };
  static RenderFlags ChangeFlag(RenderFlags aFlags, bool aOnOff,
                                eRenderFlag aFlag) {
    return aOnOff ? (aFlags | aFlag) : (aFlag & ~aFlag);
  }

  void SetRenderingState(const RenderingState& aState);

  friend class ::nsPresShellEventCB;

  bool mCaretEnabled;

  // methods for painting a range to an offscreen buffer

  // given a display list, clip the items within the list to
  // the range
  nsRect ClipListToRange(nsDisplayListBuilder* aBuilder, nsDisplayList* aList,
                         nsRange* aRange);

  // create a RangePaintInfo for the range aRange containing the
  // display list needed to paint the range to a surface
  UniquePtr<RangePaintInfo> CreateRangePaintInfo(nsRange* aRange,
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
   * aFlags - set RenderImageFlags::AutoScale to scale down large images, but
   * it must not be set if a custom image was specified
   */
  already_AddRefed<SourceSurface> PaintRangePaintInfo(
      const nsTArray<UniquePtr<RangePaintInfo>>& aItems,
      dom::Selection* aSelection, const Maybe<CSSIntRegion>& aRegion,
      nsRect aArea, const LayoutDeviceIntPoint aPoint,
      LayoutDeviceIntRect* aScreenRect, RenderImageFlags aFlags);

  // Hide a view if it is a popup
  void HideViewIfPopup(nsView* aView);

  // Utility method to restore the root scrollframe state
  void RestoreRootScrollPosition();

  void MaybeReleaseCapturingContent();

  class DelayedEvent {
   public:
    virtual ~DelayedEvent() {}
    virtual void Dispatch() {}
    virtual bool IsKeyPressEvent() { return false; }
  };

  class DelayedInputEvent : public DelayedEvent {
   public:
    void Dispatch() override;

   protected:
    DelayedInputEvent();
    ~DelayedInputEvent() override;

    WidgetInputEvent* mEvent;
  };

  class DelayedMouseEvent : public DelayedInputEvent {
   public:
    explicit DelayedMouseEvent(WidgetMouseEvent* aEvent);
  };

  class DelayedKeyEvent : public DelayedInputEvent {
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
    ~nsSynthMouseMoveEvent() { Revoke(); }

   public:
    NS_INLINE_DECL_REFCOUNTING(nsSynthMouseMoveEvent, override)

    void Revoke() {
      if (mPresShell) {
        mPresShell->GetPresContext()->RefreshDriver()->RemoveRefreshObserver(
            this, FlushType::Display);
        mPresShell = nullptr;
      }
    }
    MOZ_CAN_RUN_SCRIPT
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
  MOZ_CAN_RUN_SCRIPT void ProcessSynthMouseMoveEvent(bool aFromScroll);

  void QueryIsActive();
  nsresult UpdateImageLockingState();

  already_AddRefed<PresShell> GetParentPresShellForEventHandling();

  /**
   * EventHandler is implementation of nsIPresShell::HandleEvent().
   */
  class MOZ_STACK_CLASS EventHandler final {
   public:
    EventHandler() = delete;
    EventHandler(const EventHandler& aOther) = delete;
    explicit EventHandler(PresShell& aPresShell)
        : mPresShell(aPresShell), mCurrentEventInfoSetter(nullptr) {}
    explicit EventHandler(RefPtr<PresShell>&& aPresShell)
        : mPresShell(aPresShell.forget()), mCurrentEventInfoSetter(nullptr) {}

    /**
     * HandleEvent() may dispatch aGUIEvent.  This may redirect the event to
     * another PresShell, or the event may be handled by other classes like
     * AccessibleCaretEventHub, or discarded.  Otherwise, this sets current
     * event info of mPresShell and calls HandleEventWithCurrentEventInfo()
     * to dispatch the event into the DOM tree.
     *
     * @param aFrameForPresShell        The frame for PresShell.  If PresShell
     *                                  has root frame, it should be set.
     *                                  Otherwise, a frame which contains the
     *                                  PresShell should be set instead.  I.e.,
     *                                  in the latter case, the frame is in
     *                                  a parent document.
     * @param aGUIEvent                 Event to be handled.  Must be a trusted
     *                                  event.
     * @param aDontRetargetEvents       true if this shouldn't redirect the
     *                                  event to different PresShell.
     *                                  false if this can redirect the event to
     *                                  different PresShell.
     * @param aEventStatus              [in/out] EventStatus of aGUIEvent.
     */
    MOZ_CAN_RUN_SCRIPT
    nsresult HandleEvent(nsIFrame* aFrameForPresShell,
                         WidgetGUIEvent* aGUIEvent, bool aDontRetargetEvents,
                         nsEventStatus* aEventStatus);

    /**
     * HandleEventWithTarget() tries to dispatch aEvent on aContent after
     * setting current event target content to aNewEventContent and current
     * event frame to aNewEventFrame temporarily.  Note that this supports
     * WidgetEvent, not WidgetGUIEvent.  So, you can dispatch a simple event
     * with this.
     *
     * @param aEvent                    Event to be dispatched.  Must be a
     *                                  trusted event.
     * @param aNewEventFrame            Temporal new event frame.
     * @param aNewEventContent          Temporal new event content.
     * @param aEventStatus              [in/out] EventStuatus of aEvent.
     * @param aIsHandlingNativeEvent    true if aEvent represents a native
     *                                  event.
     * @param aTargetContent            This is used only when aEvent is a
     *                                  pointer event.  If
     *                                  PresShell::mPointerEventTarget is
     *                                  changed during dispatching aEvent,
     *                                  this is set to the new target.
     * @param aOverrideClickTarget      Override click event target.
     */
    MOZ_CAN_RUN_SCRIPT
    nsresult HandleEventWithTarget(WidgetEvent* aEvent,
                                   nsIFrame* aNewEventFrame,
                                   nsIContent* aNewEventContent,
                                   nsEventStatus* aEventStatus,
                                   bool aIsHandlingNativeEvent,
                                   nsIContent** aTargetContent,
                                   nsIContent* aOverrideClickTarget);

    /**
     * OnPresShellDestroy() is called when every PresShell instance is being
     * destroyed.
     */
    static inline void OnPresShellDestroy(Document* aDocument);

   private:
    static bool InZombieDocument(nsIContent* aContent);
    static nsIFrame* GetNearestFrameContainingPresShell(PresShell* aPresShell);
    static already_AddRefed<nsIURI> GetDocumentURIToCompareWithBlacklist(
        PresShell& aPresShell);

    /**
     * HandleEventUsingCoordinates() handles aGUIEvent whose
     * IsUsingCoordinates() returns true with the following helper methods.
     *
     * @param aFrameForPresShell        The frame for PresShell.  See
     *                                  explanation of HandleEvent() for the
     *                                  details.
     * @param aGUIEvent                 The handling event.  Make sure that
     *                                  its IsUsingCoordinates() returns true.
     * @param aEventStatus              The status of aGUIEvent.
     * @param aDontRetargetEvents       true if we've already retarget document.
     *                                  Otherwise, false.
     */
    MOZ_CAN_RUN_SCRIPT
    nsresult HandleEventUsingCoordinates(nsIFrame* aFrameForPresShell,
                                         WidgetGUIEvent* aGUIEvent,
                                         nsEventStatus* aEventStatus,
                                         bool aDontRetargetEvents);

    /**
     * EventTargetData struct stores a set of a PresShell (event handler),
     * a frame (to handle the event) and a content (event target for the frame).
     */
    struct MOZ_STACK_CLASS EventTargetData final {
      EventTargetData() = delete;
      EventTargetData(const EventTargetData& aOther) = delete;
      EventTargetData(PresShell* aPresShell, nsIFrame* aFrameToHandleEvent)
          : mPresShell(aPresShell), mFrame(aFrameToHandleEvent) {}

      void SetPresShellAndFrame(PresShell* aPresShell,
                                nsIFrame* aFrameToHandleEvent) {
        mPresShell = aPresShell;
        mFrame = aFrameToHandleEvent;
        mContent = nullptr;
      }
      void SetFrameAndComputePresShell(nsIFrame* aFrameToHandleEvent);
      void SetFrameAndComputePresShellAndContent(nsIFrame* aFrameToHandleEvent,
                                                 WidgetGUIEvent* aGUIEvent);
      void SetContentForEventFromFrame(WidgetGUIEvent* aGUIEvent);

      nsPresContext* GetPresContext() const {
        return mPresShell ? mPresShell->GetPresContext() : nullptr;
      };
      EventStateManager* GetEventStateManager() const {
        nsPresContext* presContext = GetPresContext();
        return presContext ? presContext->EventStateManager() : nullptr;
      }
      Document* GetDocument() const {
        return mPresShell ? mPresShell->GetDocument() : nullptr;
      }
      nsIContent* GetFrameContent() const;

      /**
       * MaybeRetargetToActiveDocument() tries retarget aGUIEvent into
       * active document if there is.  Note that this does not support to
       * retarget mContent.  Make sure it is nullptr before calling this.
       *
       * @param aGUIEvent       The handling event.
       * @return                true if retargetted.
       */
      bool MaybeRetargetToActiveDocument(WidgetGUIEvent* aGUIEvent);

      /**
       * ComputeElementFromFrame() computes mContent for aGUIEvent.  If
       * mContent is set by this method, mContent is always nullptr or an
       * Element.
       *
       * @param aGUIEvent       The handling event.
       * @return                true if caller can keep handling the event.
       *                        Otherwise, false.
       *                        Note that even if this returns true, mContent
       *                        may be nullptr.
       */
      bool ComputeElementFromFrame(WidgetGUIEvent* aGUIEvent);

      /**
       * UpdateTouchEventTarget() updates mFrame, mPresShell and mContent if
       * aGUIEvent is a touch event and there is new proper target.
       *
       * @param aGUIEvent       The handled event.  If it's not a touch event,
       *                        this method does nothing.
       */
      void UpdateTouchEventTarget(WidgetGUIEvent* aGUIEvent);

      RefPtr<PresShell> mPresShell;
      nsIFrame* mFrame;
      nsCOMPtr<nsIContent> mContent;
      nsCOMPtr<nsIContent> mOverrideClickTarget;
    };

    /**
     * MaybeFlushPendingNotifications() maybe flush pending notifications if
     * aGUIEvent should be handled with the latest layout.
     *
     * @param aGUIEvent                 The handling event.
     * @return                          true if this actually flushes pending
     *                                  layout and that has caused changing the
     *                                  layout.
     */
    MOZ_CAN_RUN_SCRIPT
    bool MaybeFlushPendingNotifications(WidgetGUIEvent* aGUIEvent);

    /**
     * GetFrameToHandleNonTouchEvent() returns a frame to handle the event.
     * This may flush pending layout if the target is in child PresShell.
     *
     * @param aRootFrameToHandleEvent   The root frame to handle the event.
     * @param aGUIEvent                 The handling event.
     * @return                          The frame which should handle the
     *                                  event.  nullptr if the caller should
     *                                  stop handling the event.
     */
    MOZ_CAN_RUN_SCRIPT
    nsIFrame* GetFrameToHandleNonTouchEvent(nsIFrame* aRootFrameToHandleEvent,
                                            WidgetGUIEvent* aGUIEvent);

    /**
     * ComputeEventTargetFrameAndPresShellAtEventPoint() computes event
     * target frame at the event point of aGUIEvent and set it to
     * aEventTargetData.
     *
     * @param aRootFrameToHandleEvent   The root frame to handle aGUIEvent.
     * @param aGUIEvent                 The handling event.
     * @param aEventTargetData          [out] Its frame and PresShell will
     *                                  be set.
     * @return                          true if the caller can handle the
     *                                  event.  Otherwise, false.
     */
    MOZ_CAN_RUN_SCRIPT
    bool ComputeEventTargetFrameAndPresShellAtEventPoint(
        nsIFrame* aRootFrameToHandleEvent, WidgetGUIEvent* aGUIEvent,
        EventTargetData* aEventTargetData);

    /**
     * DispatchPrecedingPointerEvent() dispatches preceding pointer event for
     * aGUIEvent if Pointer Events is enabled.
     *
     * @param aFrameForPresShell        The frame for PresShell.  See
     *                                  explanation of HandleEvent() for the
     *                                  details.
     * @param aGUIEvent                 The handled event.
     * @param aPointerCapturingContent  The content which is capturing pointer
     *                                  events if there is.  Otherwise, nullptr.
     * @param aDontRetargetEvents       Set aDontRetargetEvents of
     *                                  HandleEvent() which called this method.
     * @param aEventTargetData          [in/out] Event target data of
     *                                  aGUIEvent.  If pointer event listeners
     *                                  change the DOM tree or reframe the
     *                                  target, updated by this method.
     * @param aEventStatus              [in/out] The event status of aGUIEvent.
     * @return                          true if the caller can handle the
     *                                  event.  Otherwise, false.
     */
    MOZ_CAN_RUN_SCRIPT
    bool DispatchPrecedingPointerEvent(nsIFrame* aFrameForPresShell,
                                       WidgetGUIEvent* aGUIEvent,
                                       nsIContent* aPointerCapturingContent,
                                       bool aDontRetargetEvents,
                                       EventTargetData* aEventTargetData,
                                       nsEventStatus* aEventStatus);

    /**
     * MaybeDiscardEvent() checks whether it's safe to handle aGUIEvent right
     * now.  If it's not safe, this may notify somebody of discarding event if
     * necessary.
     *
     * @param aGUIEvent   Handling event.
     * @return            true if it's not safe to handle the event.
     */
    bool MaybeDiscardEvent(WidgetGUIEvent* aGUIEvent);

    /**
     * GetCapturingContentFor() returns capturing content for aGUIEvent.
     * If aGUIEvent is not related to capturing, this returns nullptr.
     */
    static nsIContent* GetCapturingContentFor(WidgetGUIEvent* aGUIEvent);

    /**
     * GetRetargetEventDocument() returns a document if aGUIEvent should be
     * handled in another document.
     *
     * @param aGUIEvent                 Handling event.
     * @param aRetargetEventDocument    Document which should handle aGUIEvent.
     * @return                          true if caller can keep handling
     *                                  aGUIEvent.
     */
    bool GetRetargetEventDocument(WidgetGUIEvent* aGUIEvent,
                                  Document** aRetargetEventDocument);

    /**
     * GetFrameForHandlingEventWith() returns a frame which should be used as
     * aFrameForPresShell of HandleEvent().  See @return for the details.
     *
     * @param aGUIEvent                 Handling event.
     * @param aRetargetDocument         Document which aGUIEvent should be
     *                                  fired on.  Typically, should be result
     *                                  of GetRetargetEventDocument().
     * @param aFrameForPresShell        The frame for PresShell.  See
     *                                  explanation of HandleEvent() for the
     *                                  details.
     * @return                          nullptr if caller should stop handling
     *                                  the event.
     *                                  aFrameForPresShell if caller should
     *                                  keep handling the event by itself.
     *                                  Otherwise, caller should handle it with
     *                                  another PresShell which is result of
     *                                  nsIFrame::PresContext()->GetPresShell().
     */
    nsIFrame* GetFrameForHandlingEventWith(WidgetGUIEvent* aGUIEvent,
                                           Document* aRetargetDocument,
                                           nsIFrame* aFrameForPresShell);

    /**
     * MaybeHandleEventWithAnotherPresShell() may handle aGUIEvent with another
     * PresShell.
     *
     * @param aFrameForPresShell        The frame for PresShell.  See
     *                                  explanation of HandleEvent() for the
     *                                  details.
     * @param aGUIEvent                 Handling event.
     * @param aEventStatus              [in/out] EventStatus of aGUIEvent.
     * @param aRv                       [out] Returns error if this gets an
     *                                  error handling the event.
     * @return                          false if caller needs to keep handling
     *                                  the event by itself.
     *                                  true if caller shouldn't keep handling
     *                                  the event.  Note that when no PresShell
     *                                  can handle the event, this returns true.
     */
    MOZ_CAN_RUN_SCRIPT
    bool MaybeHandleEventWithAnotherPresShell(nsIFrame* aFrameForPresShell,
                                              WidgetGUIEvent* aGUIEvent,
                                              nsEventStatus* aEventStatus,
                                              nsresult* aRv);

    MOZ_CAN_RUN_SCRIPT
    nsresult RetargetEventToParent(WidgetGUIEvent* aGUIEvent,
                                   nsEventStatus* aEventStatus);

    /**
     * MaybeHandleEventWithAccessibleCaret() may handle aGUIEvent with
     * AccessibleCaretEventHub if it's necessary.
     *
     * @param aGUIEvent         Event may be handled by AccessibleCaretEventHub.
     * @param aEventStatus      [in/out] EventStatus of aGUIEvent.
     * @return                  true if AccessibleCaretEventHub handled the
     *                          event and caller shouldn't keep handling it.
     */
    MOZ_CAN_RUN_SCRIPT
    bool MaybeHandleEventWithAccessibleCaret(WidgetGUIEvent* aGUIEvent,
                                             nsEventStatus* aEventStatus);

    /**
     * MaybeDiscardOrDelayKeyboardEvent() may discared or put aGUIEvent into
     * the delayed event queue if it's a keyboard event and if we should do so.
     * If aGUIEvent is not a keyboard event, this does nothing.
     *
     * @param aGUIEvent         The handling event.
     * @return                  true if this method discard the event or
     *                          put it into the delayed event queue.
     */
    bool MaybeDiscardOrDelayKeyboardEvent(WidgetGUIEvent* aGUIEvent);

    /**
     * MaybeDiscardOrDelayMouseEvent() may discard or put aGUIEvent into the
     * delayed event queue if it's a mouse event and if we should do so.
     * If aGUIEvent is not a mouse event, this does nothing.
     * If there is suppressed event listener like debugger of devtools, this
     * notifies it of the event after discard or put it into the delayed
     * event queue.
     *
     * @param aFrameToHandleEvent       The frame to handle aGUIEvent.
     * @param aGUIEvent                 The handling event.
     * @return                          true if this method discard the event
     *                                  or put it into the delayed event queue.
     */
    bool MaybeDiscardOrDelayMouseEvent(nsIFrame* aFrameToHandleEvent,
                                       WidgetGUIEvent* aGUIEvent);

    /**
     * MaybeFlushThrottledStyles() tries to flush pending animation.  If it's
     * flushed and then aFrameForPresShell is destroyed, returns new frame
     * which contains mPresShell.
     *
     * @param aFrameForPresShell        The frame for PresShell.  See
     *                                  explanation of HandleEvent() for the
     *                                  details.  This can be nullptr.
     * @return                          Maybe new frame for mPresShell.
     *                                  If aFrameForPresShell is not nullptr
     *                                  and hasn't been destroyed, returns
     *                                  aFrameForPresShell as-is.
     */
    MOZ_CAN_RUN_SCRIPT
    nsIFrame* MaybeFlushThrottledStyles(nsIFrame* aFrameForPresShell);

    /**
     * ComputeRootFrameToHandleEvent() returns root frame to handle the event.
     * For example, if there is a popup, this returns the popup frame.
     * If there is capturing content and it's in a scrolled frame, returns
     * the scrolled frame.
     *
     * @param aFrameForPresShell                The frame for PresShell.  See
     *                                          explanation of HandleEvent() for
     *                                          the details.
     * @param aGUIEvent                         The handling event.
     * @param aCapturingContent                 Capturing content if there is.
     *                                          nullptr, otherwise.
     * @param aIsCapturingContentIgnored        [out] true if aCapturingContent
     *                                          is not nullptr but it should be
     *                                          ignored to handle the event.
     * @param aIsCaptureRetargeted              [out] true if aCapturingContent
     *                                          is not nullptr but it's
     *                                          retargeted.
     * @return                                  Root frame to handle the event.
     */
    nsIFrame* ComputeRootFrameToHandleEvent(nsIFrame* aFrameForPresShell,
                                            WidgetGUIEvent* aGUIEvent,
                                            nsIContent* aCapturingContent,
                                            bool* aIsCapturingContentIgnored,
                                            bool* aIsCaptureRetargeted);

    /**
     * ComputeRootFrameToHandleEventWithPopup() returns popup frame if there
     * is a popup and we should handle the event in it.  Otherwise, returns
     * aRootFrameToHandleEvent.
     *
     * @param aRootFrameToHandleEvent           Candidate root frame to handle
     *                                          the event.
     * @param aGUIEvent                         The handling event.
     * @param aCapturingContent                 Capturing content if there is.
     *                                          nullptr, otherwise.
     * @param aIsCapturingContentIgnored        [out] true if aCapturingContent
     *                                          is not nullptr but it should be
     *                                          ignored to handle the event.
     * @return                                  A popup frame if there is a
     *                                          popup and we should handle the
     *                                          event in it.  Otherwise,
     *                                          aRootFrameToHandleEvent.
     *                                          I.e., never returns nullptr.
     */
    nsIFrame* ComputeRootFrameToHandleEventWithPopup(
        nsIFrame* aRootFrameToHandleEvent, WidgetGUIEvent* aGUIEvent,
        nsIContent* aCapturingContent, bool* aIsCapturingContentIgnored);

    /**
     * ComputeRootFrameToHandleEventWithCapturingContent() returns root frame
     * to handle event for the capturing content, or aRootFrameToHandleEvent
     * if it should be ignored.
     *
     * @param aRootFrameToHandleEvent           Candidate root frame to handle
     *                                          the event.
     * @param aCapturingContent                 Capturing content.  nullptr is
     *                                          not allowed.
     * @param aIsCapturingContentIgnored        [out] true if aCapturingContent
     *                                          is not nullptr but it should be
     *                                          ignored to handle the event.
     * @param aIsCaptureRetargeted              [out] true if aCapturingContent
     *                                          is not nullptr but it's
     *                                          retargeted.
     * @return                                  A popup frame if there is a
     *                                          popup and we should handle the
     *                                          event in it.  Otherwise,
     *                                          aRootFrameToHandleEvent.
     *                                          I.e., never returns nullptr.
     */
    nsIFrame* ComputeRootFrameToHandleEventWithCapturingContent(
        nsIFrame* aRootFrameToHandleEvent, nsIContent* aCapturingContent,
        bool* aIsCapturingContentIgnored, bool* aIsCaptureRetargeted);

    /**
     * HandleEventWithPointerCapturingContentWithoutItsFrame() handles
     * aGUIEvent with aPointerCapturingContent when it does not have primary
     * frame.
     *
     * @param aFrameForPresShell        The frame for PresShell.  See
     *                                  explanation of HandleEvent() for the
     *                                  details.
     * @param aGUIEvent                 The handling event.
     * @param aPointerCapturingContent  Current pointer capturing content.
     *                                  Must not be nullptr.
     * @param aEventStatus              [in/out] The event status of aGUIEvent.
     * @return                          Basically, result of
     *                                  HandeEventWithTraget().
     */
    MOZ_CAN_RUN_SCRIPT
    nsresult HandleEventWithPointerCapturingContentWithoutItsFrame(
        nsIFrame* aFrameForPresShell, WidgetGUIEvent* aGUIEvent,
        nsIContent* aPointerCapturingContent, nsEventStatus* aEventStatus);

    /**
     * HandleEventAtFocusedContent() handles aGUIEvent at focused content.
     *
     * @param aGUIEvent         The handling event which should be handled at
     *                          focused content.
     * @param aEventStatus      [in/out] The event status of aGUIEvent.
     */
    MOZ_CAN_RUN_SCRIPT
    nsresult HandleEventAtFocusedContent(WidgetGUIEvent* aGUIEvent,
                                         nsEventStatus* aEventStatus);

    /**
     * ComputeFocusedEventTargetElement() returns event target element for
     * aGUIEvent which should be handled with focused content.
     * This may set/unset sLastKeyDownEventTarget if necessary.
     *
     * @param aGUIEvent                 The handling event.
     * @return                          The element which should be the event
     *                                  target of aGUIEvent.
     */
    dom::Element* ComputeFocusedEventTargetElement(WidgetGUIEvent* aGUIEvent);

    /**
     * MaybeHandleEventWithAnotherPresShell() may handle aGUIEvent with another
     * PresShell.
     *
     * @param aEventTargetElement       The event target element of aGUIEvent.
     * @param aGUIEvent                 Handling event.
     * @param aEventStatus              [in/out] EventStatus of aGUIEvent.
     * @param aRv                       [out] Returns error if this gets an
     *                                  error handling the event.
     * @return                          false if caller needs to keep handling
     *                                  the event by itself.
     *                                  true if caller shouldn't keep handling
     *                                  the event.  Note that when no PresShell
     *                                  can handle the event, this returns true.
     */
    MOZ_CAN_RUN_SCRIPT
    bool MaybeHandleEventWithAnotherPresShell(dom::Element* aEventTargetElement,
                                              WidgetGUIEvent* aGUIEvent,
                                              nsEventStatus* aEventStatus,
                                              nsresult* aRv);

    /**
     * HandleRetargetedEvent() dispatches aGUIEvent on the PresShell without
     * retargetting.  This should be used only when caller computes final
     * target of aGUIEvent.
     *
     * @param aGUIEvent         Event to be dispatched.
     * @param aEventStatus      [in/out] EventStatus of aGUIEvent.
     * @param aTarget           The final target of aGUIEvent.
     */
    MOZ_CAN_RUN_SCRIPT
    nsresult HandleRetargetedEvent(WidgetGUIEvent* aGUIEvent,
                                   nsEventStatus* aEventStatus,
                                   nsIContent* aTarget) {
      AutoCurrentEventInfoSetter eventInfoSetter(*this, nullptr, aTarget);
      if (!mPresShell->GetCurrentEventFrame()) {
        return NS_OK;
      }
      nsCOMPtr<nsIContent> overrideClickTarget;
      return HandleEventWithCurrentEventInfo(aGUIEvent, aEventStatus, true,
                                             overrideClickTarget);
    }

    /**
     * HandleEventWithFrameForPresShell() handles aGUIEvent with the frame
     * for mPresShell.
     *
     * @param aFrameForPresShell        The frame for mPresShell.
     * @param aGUIEvent                 The handling event.  It shouldn't be
     *                                  handled with using coordinates nor
     *                                  handled at focused content.
     * @param aEventStatus              [in/out] The status of aGUIEvent.
     */
    MOZ_CAN_RUN_SCRIPT
    nsresult HandleEventWithFrameForPresShell(nsIFrame* aFrameForPresShell,
                                              WidgetGUIEvent* aGUIEvent,
                                              nsEventStatus* aEventStatus);

    /**
     * HandleEventWithCurrentEventInfo() prepares to dispatch aEvent into the
     * DOM, dispatches aEvent into the DOM with using current event info of
     * mPresShell and notifies EventStateManager of that.
     *
     * @param aEvent                    Event to be dispatched.
     * @param aEventStatus              [in/out] EventStatus of aEvent.
     * @param aIsHandlingNativeEvent    true if aGUIEvent represents a native
     *                                  event.
     * @param aOverrideClickTarget      Override click event target.
     */
    MOZ_CAN_RUN_SCRIPT
    nsresult HandleEventWithCurrentEventInfo(WidgetEvent* aEvent,
                                             nsEventStatus* aEventStatus,
                                             bool aIsHandlingNativeEvent,
                                             nsIContent* aOverrideClickTarget);

    /**
     * HandlingTimeAccumulator() may accumulate handling time of telemetry
     * for each type of events.
     */
    class MOZ_STACK_CLASS HandlingTimeAccumulator final {
     public:
      HandlingTimeAccumulator() = delete;
      HandlingTimeAccumulator(const HandlingTimeAccumulator& aOther) = delete;
      HandlingTimeAccumulator(const EventHandler& aEventHandler,
                              const WidgetEvent* aEvent);
      ~HandlingTimeAccumulator();

     private:
      const EventHandler& mEventHandler;
      const WidgetEvent* mEvent;
      TimeStamp mHandlingStartTime;
    };

    /**
     * RecordEventPreparationPerformance() records event preparation performance
     * with telemetry only when aEvent is a trusted event.
     *
     * @param aEvent            The handling event which we've finished
     *                          preparing something to dispatch.
     */
    void RecordEventPreparationPerformance(const WidgetEvent* aEvent);

    /**
     * RecordEventHandlingResponsePerformance() records event handling response
     * performance with telemetry.
     *
     * @param aEvent            The handled event.
     */
    void RecordEventHandlingResponsePerformance(const WidgetEvent* aEvent);

    /**
     * PrepareToDispatchEvent() prepares to dispatch aEvent.
     *
     * @param aEvent                    The handling event.
     * @param aEventStatus              [in/out] The status of aEvent.
     * @param aIsUserInteraction        [out] Set to true if the event is user
     *                                  interaction.  I.e., enough obvious input
     *                                  to allow to open popup, etc.  Otherwise,
     *                                  set to false.
     * @param aTouchIsNew               [out] Set to true if the event is an
     *                                  eTouchMove event and it represents new
     *                                  touch.  Otherwise, set to false.
     * @return                          true if the caller can dispatch the
     *                                  event into the DOM.
     */
    MOZ_CAN_RUN_SCRIPT
    bool PrepareToDispatchEvent(WidgetEvent* aEvent,
                                nsEventStatus* aEventStatus,
                                bool* aIsUserInteraction, bool* aTouchIsNew);

    /**
     * MaybeHandleKeyboardEventBeforeDispatch() may handle aKeyboardEvent
     * if it should do something before dispatched into the DOM.
     *
     * @param aKeyboardEvent    The handling keyboard event.
     */
    MOZ_CAN_RUN_SCRIPT
    void MaybeHandleKeyboardEventBeforeDispatch(
        WidgetKeyboardEvent* aKeyboardEvent);

    /**
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
    MOZ_CAN_RUN_SCRIPT
    bool AdjustContextMenuKeyEvent(WidgetMouseEvent* aMouseEvent);

    MOZ_CAN_RUN_SCRIPT
    bool PrepareToUseCaretPosition(nsIWidget* aEventWidget,
                                   LayoutDeviceIntPoint& aTargetPt);

    /**
     * Get the selected item and coordinates in device pixels relative to root
     * document's root view for element, first ensuring the element is onscreen.
     */
    MOZ_CAN_RUN_SCRIPT
    void GetCurrentItemAndPositionForElement(dom::Element* aFocusedElement,
                                             nsIContent** aTargetToUse,
                                             LayoutDeviceIntPoint& aTargetPt,
                                             nsIWidget* aRootWidget);

    nsIContent* GetOverrideClickTarget(WidgetGUIEvent* aGUIEvent,
                                       nsIFrame* aFrame);

    /**
     * DispatchEvent() tries to dispatch aEvent and notifies aEventStateManager
     * of doing it.
     *
     * @param aEventStateManager        EventStateManager which should handle
     *                                  the event before/after dispatching
     *                                  aEvent into the DOM.
     * @param aEvent                    The handling event.
     * @param aTouchIsNew               Set this to true when the message is
     *                                  eTouchMove and it's newly touched.
     *                                  Then, the "touchmove" event becomes
     *                                  cancelable.
     * @param aEventStatus              [in/out] The status of aEvent.
     * @param aOverrideClickTarget      Override click event target.
     */
    MOZ_CAN_RUN_SCRIPT
    nsresult DispatchEvent(EventStateManager* aEventStateManager,
                           WidgetEvent* aEvent, bool aTouchIsNew,
                           nsEventStatus* aEventStatus,
                           nsIContent* aOverrideClickTarget);

    /**
     * DispatchEventToDOM() actually dispatches aEvent into the DOM tree.
     *
     * @param aEvent            Event to be dispatched into the DOM tree.
     * @param aEventStatus      [in/out] EventStatus of aEvent.
     * @param aEventCB          The callback kicked when the event moves
     *                          from the default group to the system group.
     */
    nsresult DispatchEventToDOM(WidgetEvent* aEvent,
                                nsEventStatus* aEventStatus,
                                nsPresShellEventCB* aEventCB);

    /**
     * DispatchTouchEventToDOM() dispatches touch events into the DOM tree.
     *
     * @param aEvent            The source of events to be dispatched into the
     *                          DOM tree.
     * @param aEventStatus      [in/out] EventStatus of aEvent.
     * @param aEventCB          The callback kicked when the events move
     *                          from the default group to the system group.
     * @param aTouchIsNew       Set this to true when the message is eTouchMove
     *                          and it's newly touched.  Then, the "touchmove"
     *                          event becomes cancelable.
     */
    void DispatchTouchEventToDOM(WidgetEvent* aEvent,
                                 nsEventStatus* aEventStatus,
                                 nsPresShellEventCB* aEventCB,
                                 bool aTouchIsNew);

    /**
     * FinalizeHandlingEvent() should be called after calling DispatchEvent()
     * and then, this cleans up the state of mPresShell and aEvent.
     *
     * @param aEvent            The handled event.
     */
    void FinalizeHandlingEvent(WidgetEvent* aEvent);

    /**
     * AutoCurrentEventInfoSetter() pushes and pops current event info of
     * aEventHandler.mPresShell.
     */
    struct MOZ_STACK_CLASS AutoCurrentEventInfoSetter final {
      explicit AutoCurrentEventInfoSetter(EventHandler& aEventHandler)
          : mEventHandler(aEventHandler) {
        MOZ_DIAGNOSTIC_ASSERT(!mEventHandler.mCurrentEventInfoSetter);
        mEventHandler.mCurrentEventInfoSetter = this;
        mEventHandler.mPresShell->PushCurrentEventInfo(nullptr, nullptr);
      }
      AutoCurrentEventInfoSetter(EventHandler& aEventHandler, nsIFrame* aFrame,
                                 nsIContent* aContent)
          : mEventHandler(aEventHandler) {
        MOZ_DIAGNOSTIC_ASSERT(!mEventHandler.mCurrentEventInfoSetter);
        mEventHandler.mCurrentEventInfoSetter = this;
        mEventHandler.mPresShell->PushCurrentEventInfo(aFrame, aContent);
      }
      AutoCurrentEventInfoSetter(EventHandler& aEventHandler,
                                 EventTargetData& aEventTargetData)
          : mEventHandler(aEventHandler) {
        MOZ_DIAGNOSTIC_ASSERT(!mEventHandler.mCurrentEventInfoSetter);
        mEventHandler.mCurrentEventInfoSetter = this;
        mEventHandler.mPresShell->PushCurrentEventInfo(
            aEventTargetData.mFrame, aEventTargetData.mContent);
      }
      ~AutoCurrentEventInfoSetter() {
        mEventHandler.mPresShell->PopCurrentEventInfo();
        mEventHandler.mCurrentEventInfoSetter = nullptr;
      }

     private:
      EventHandler& mEventHandler;
    };

    /**
     * Wrapper methods to access methods of mPresShell.
     */
    nsPresContext* GetPresContext() const {
      return mPresShell->GetPresContext();
    }
    Document* GetDocument() const { return mPresShell->GetDocument(); }
    nsCSSFrameConstructor* FrameConstructor() const {
      return mPresShell->FrameConstructor();
    }
    already_AddRefed<nsPIDOMWindowOuter> GetFocusedDOMWindowInOurWindow() {
      return mPresShell->GetFocusedDOMWindowInOurWindow();
    }
    already_AddRefed<PresShell> GetParentPresShellForEventHandling() {
      return mPresShell->GetParentPresShellForEventHandling();
    }
    void PushDelayedEventIntoQueue(UniquePtr<DelayedEvent>&& aDelayedEvent) {
      mPresShell->mDelayedEvents.AppendElement(std::move(aDelayedEvent));
    }

    OwningNonNull<PresShell> mPresShell;
    AutoCurrentEventInfoSetter* mCurrentEventInfoSetter;
    static TimeStamp sLastInputCreated;
    static TimeStamp sLastInputProcessed;
    static StaticRefPtr<dom::Element> sLastKeyDownEventTargetElement;
  };

  PresShell* GetRootPresShell();

  nscolor GetDefaultBackgroundColorToDraw();

  // The callback for the mPaintSuppressionTimer timer.
  static void sPaintSuppressionCallback(nsITimer* aTimer, void* aPresShell);

  //////////////////////////////////////////////////////////////////////////////
  // Approximate frame visibility tracking implementation.
  //////////////////////////////////////////////////////////////////////////////

  void UpdateApproximateFrameVisibility();
  void DoUpdateApproximateFrameVisibility(bool aRemoveOnly);

  void ClearApproximatelyVisibleFramesList(
      const Maybe<OnNonvisible>& aNonvisibleAction = Nothing());
  static void ClearApproximateFrameVisibilityVisited(nsView* aView,
                                                     bool aClear);
  static void MarkFramesInListApproximatelyVisible(const nsDisplayList& aList);
  void MarkFramesInSubtreeApproximatelyVisible(nsIFrame* aFrame,
                                               const nsRect& aRect,
                                               bool aRemoveOnly = false);

  void DecApproximateVisibleCount(
      VisibleFrames& aFrames,
      const Maybe<OnNonvisible>& aNonvisibleAction = Nothing());

  nsRevocableEventPtr<nsRunnableMethod<PresShell>>
      mUpdateApproximateFrameVisibilityEvent;

  // A set of frames that were visible or could be visible soon at the time
  // that we last did an approximate frame visibility update.
  VisibleFrames mApproximatelyVisibleFrames;

  nsresult SetResolutionImpl(float aResolution, bool aScaleToResolution,
                             nsAtom* aOrigin);

#ifdef DEBUG
  MOZ_CAN_RUN_SCRIPT_BOUNDARY bool VerifyIncrementalReflow();
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void DoVerifyReflow();
  void VerifyHasDirtyRootAncestor(nsIFrame* aFrame);
  void ShowEventTargetDebug();

  bool mInVerifyReflow = false;
  // The reflow root under which we're currently reflowing.  Null when
  // not in reflow.
  nsIFrame* mCurrentReflowRoot = nullptr;

  nsIFrame* mDrawEventTargetFrame = nullptr;
#endif  // #ifdef DEBUG

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

  nsTArray<UniquePtr<DelayedEvent>> mDelayedEvents;

 private:
  nsRevocableEventPtr<nsSynthMouseMoveEvent> mSynthMouseMoveEvent;

  TouchManager mTouchManager;

  RefPtr<ZoomConstraintsClient> mZoomConstraintsClient;
  RefPtr<GeckoMVMContext> mMVMContext;
  RefPtr<MobileViewportManager> mMobileViewportManager;

  // This timer controls painting suppression.  Until it fires
  // or all frames are constructed, we won't paint anything but
  // our <body> background and scrollbars.
  nsCOMPtr<nsITimer> mPaintSuppressionTimer;

  nsCOMPtr<nsITimer> mDelayedPaintTimer;

  TimeStamp mLoadBegin;  // used to time loads

  // Information about live content (which still stay in DOM tree).
  // Used in case we need re-dispatch event after sending pointer event,
  // when target of pointer event was deleted during executing user handlers.
  nsCOMPtr<nsIContent> mPointerEventTarget;

  nsCOMPtr<nsIContent> mLastAnchorScrolledTo;

  // Information needed to properly handle scrolling content into view if the
  // pre-scroll reflow flush can be interrupted.  mContentToScrollTo is non-null
  // between the initial scroll attempt and the first time we finish processing
  // all our dirty roots.  mContentToScrollTo has a content property storing the
  // details for the scroll operation, see ScrollIntoViewData above.
  nsCOMPtr<nsIContent> mContentToScrollTo;

#ifdef ACCESSIBILITY
  a11y::DocAccessible* mDocAccessible;
#endif  // #ifdef ACCESSIBILITY

  nsSize mVisualViewportSize;

  mozilla::Maybe<nsPoint> mVisualViewportOffset;

  TimeStamp mLastOSWake;

  // The focus sequence number of the last processed input event
  uint64_t mAPZFocusSequenceNumber;
  // The focus information needed for async keyboard scrolling
  FocusTarget mAPZFocusTarget;

  nscoord mLastAnchorScrollPositionY = 0;

  int32_t mActiveSuppressDisplayport;

  // True if a layout flush might not be a no-op
  bool mNeedLayoutFlush : 1;

  // True if a style flush might not be a no-op
  bool mNeedStyleFlush : 1;

  // True if there are throttled animations that would be processed when
  // performing a flush with mFlushAnimations == true.
  bool mNeedThrottledAnimationFlush : 1;

  bool mVisualViewportSizeSet : 1;

  bool mDocumentLoading : 1;
  bool mNoDelayedMouseEvents : 1;
  bool mNoDelayedKeyEvents : 1;

  bool mApproximateFrameVisibilityVisited : 1;

  bool mNextPaintCompressed : 1;

  bool mHasCSSBackgroundColor : 1;

  // Whether the last chrome-only escape key event is consumed.
  bool mIsLastChromeOnlyEscapeKeyConsumed : 1;

  // Whether the widget has received a paint message yet.
  bool mHasReceivedPaintMessage : 1;

  bool mIsLastKeyDownCanceled : 1;

  // Whether we have ever handled a user input event
  bool mHasHandledUserInput : 1;

  // Whether we should dispatch keypress events even for non-printable keys
  // for keeping backward compatibility.
  bool mForceDispatchKeyPressEventsForNonPrintableKeys : 1;
  // Whether we should set keyCode or charCode value of keypress events whose
  // value is zero to the other value or not.  When this is set to true, we
  // should keep using legacy keyCode and charCode values (i.e., one of them
  // is always 0).
  bool mForceUseLegacyKeyCodeAndCharCodeValues : 1;
  // Whether mForceDispatchKeyPressEventsForNonPrintableKeys and
  // mForceUseLegacyKeyCodeAndCharCodeValues are initialized.
  bool mInitializedWithKeyPressEventDispatchingBlacklist : 1;

  // Whether we should dispatch click events for non-primary mouse buttons.
  bool mForceUseLegacyNonPrimaryDispatch : 1;
  // Whether mForceUseLegacyNonPrimaryDispatch is initialised.
  bool mInitializedWithClickEventDispatchingBlacklist : 1;

  struct CapturingContentInfo final {
    CapturingContentInfo()
        : mAllowed(false),
          mPointerLock(false),
          mRetargetToElement(false),
          mPreventDrag(false) {}

    // capture should only be allowed during a mousedown event
    StaticRefPtr<nsIContent> mContent;
    bool mAllowed;
    bool mPointerLock;
    bool mRetargetToElement;
    bool mPreventDrag;
  };
  static CapturingContentInfo sCapturingContentInfo;

  static bool sDisableNonTestMouseEvents;

  static bool sProcessInteractable;
};

NS_DEFINE_STATIC_IID_ACCESSOR(PresShell, NS_PRESSHELL_IID)

}  // namespace mozilla

#endif  // mozilla_PresShell_h

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class of all rendering objects */

#ifndef nsFrame_h___
#define nsFrame_h___

#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "mozilla/Likely.h"
#include "nsBox.h"
#include "mozilla/Logging.h"

#include "nsIPresShell.h"
#include "mozilla/ReflowInput.h"
#include "nsHTMLParts.h"
#include "nsISelectionDisplay.h"

namespace mozilla {
enum class TableSelection : uint32_t;
} // namespace mozilla

/**
 * nsFrame logging constants. We redefine the nspr
 * PRLogModuleInfo.level field to be a bitfield.  Each bit controls a
 * specific type of logging. Each logging operation has associated
 * inline methods defined below.
 *
 * Due to the redefinition of the level field we cannot use MOZ_LOG directly
 * as that will cause assertions due to invalid log levels.
 */
#define NS_FRAME_TRACE_CALLS        0x1
#define NS_FRAME_TRACE_PUSH_PULL    0x2
#define NS_FRAME_TRACE_CHILD_REFLOW 0x4
#define NS_FRAME_TRACE_NEW_FRAMES   0x8

#define NS_FRAME_LOG_TEST(_lm,_bit) (int(((mozilla::LogModule*)_lm)->Level()) & (_bit))

#ifdef DEBUG
#define NS_FRAME_LOG(_bit,_args)                                \
  PR_BEGIN_MACRO                                                \
    if (NS_FRAME_LOG_TEST(nsFrame::sFrameLogModule,_bit)) {  \
      printf_stderr _args; \
    }                                                           \
  PR_END_MACRO
#else
#define NS_FRAME_LOG(_bit,_args)
#endif

// XXX Need to rework this so that logging is free when it's off
#ifdef DEBUG
#define NS_FRAME_TRACE_IN(_method) Trace(_method, true)

#define NS_FRAME_TRACE_OUT(_method) Trace(_method, false)

// XXX remove me
#define NS_FRAME_TRACE_MSG(_bit,_args)                          \
  PR_BEGIN_MACRO                                                \
    if (NS_FRAME_LOG_TEST(nsFrame::sFrameLogModule,_bit)) {  \
      TraceMsg _args;                                           \
    }                                                           \
  PR_END_MACRO

#define NS_FRAME_TRACE(_bit,_args)                              \
  PR_BEGIN_MACRO                                                \
    if (NS_FRAME_LOG_TEST(nsFrame::sFrameLogModule,_bit)) {  \
      TraceMsg _args;                                           \
    }                                                           \
  PR_END_MACRO

#define NS_FRAME_TRACE_REFLOW_IN(_method) Trace(_method, true)

#define NS_FRAME_TRACE_REFLOW_OUT(_method, _status) \
  Trace(_method, false, _status)

#else
#define NS_FRAME_TRACE(_bits,_args)
#define NS_FRAME_TRACE_IN(_method)
#define NS_FRAME_TRACE_OUT(_method)
#define NS_FRAME_TRACE_MSG(_bits,_args)
#define NS_FRAME_TRACE_REFLOW_IN(_method)
#define NS_FRAME_TRACE_REFLOW_OUT(_method, _status)
#endif

// Frame allocation boilerplate macros. Every subclass of nsFrame must
// either use NS_{DECL,IMPL}_FRAMEARENA_HELPERS pair for allocating
// memory correctly, or use NS_DECL_ABSTRACT_FRAME to declare a frame
// class abstract and stop it from being instantiated. If a frame class
// without its own operator new and GetFrameId gets instantiated, the
// per-frame recycler lists in nsPresArena will not work correctly,
// with potentially catastrophic consequences (not enough memory is
// allocated for a frame object).

#define NS_DECL_FRAMEARENA_HELPERS(class)                           \
  NS_DECL_QUERYFRAME_TARGET(class)                                  \
  static constexpr nsIFrame::ClassID kClassID = nsIFrame::ClassID::class##_id;  \
  void* operator new(size_t, nsIPresShell*) MOZ_MUST_OVERRIDE;      \
  nsQueryFrame::FrameIID GetFrameId() override MOZ_MUST_OVERRIDE {  \
    return nsQueryFrame::class##_id;                                \
  }

#define NS_IMPL_FRAMEARENA_HELPERS(class)                         \
  void* class::operator new(size_t sz, nsIPresShell* aShell)      \
  { return aShell->AllocateFrame(nsQueryFrame::class##_id, sz); } \

#define NS_DECL_ABSTRACT_FRAME(class)                                   \
  void* operator new(size_t, nsIPresShell*) MOZ_MUST_OVERRIDE = delete; \
  virtual nsQueryFrame::FrameIID GetFrameId() override MOZ_MUST_OVERRIDE = 0;

//----------------------------------------------------------------------

struct nsBoxLayoutMetrics;
struct nsRect;

/**
 * Implementation of a simple frame that's not splittable and has no
 * child frames.
 *
 * Sets the NS_FRAME_SYNCHRONIZE_FRAME_AND_VIEW bit, so the default
 * behavior is to keep the frame and view position and size in sync.
 */
class nsFrame : public nsBox
{
public:
  /**
   * Create a new "empty" frame that maps a given piece of content into a
   * 0,0 area.
   */
  friend nsIFrame* NS_NewEmptyFrame(nsIPresShell* aShell,
                                    ComputedStyle* aStyle);

private:
  // Left undefined; nsFrame objects are never allocated from the heap.
  void* operator new(size_t sz) CPP_THROW_NEW;

protected:
  // Overridden to prevent the global delete from being called, since
  // the memory came out of an arena instead of the heap.
  //
  // Ideally this would be private and undefined, like the normal
  // operator new.  Unfortunately, the C++ standard requires an
  // overridden operator delete to be accessible to any subclass that
  // defines a virtual destructor, so we can only make it protected;
  // worse, some C++ compilers will synthesize calls to this function
  // from the "deleting destructors" that they emit in case of
  // delete-expressions, so it can't even be undefined.
  void operator delete(void* aPtr, size_t sz);

public:

  // nsQueryFrame
  NS_DECL_QUERYFRAME
  NS_DECL_QUERYFRAME_TARGET(nsFrame)
  virtual nsQueryFrame::FrameIID GetFrameId() MOZ_MUST_OVERRIDE {
    return kFrameIID;
  }
  void* operator new(size_t, nsIPresShell*) MOZ_MUST_OVERRIDE;

  // nsIFrame
  void Init(nsIContent*       aContent,
            nsContainerFrame* aParent,
            nsIFrame*         aPrevInFlow) override;
  void DestroyFrom(nsIFrame* aDestructRoot, PostDestroyData& aPostDestroyData) override;
  ComputedStyle* GetAdditionalComputedStyle(int32_t aIndex) const override;
  void SetAdditionalComputedStyle(int32_t aIndex,
                                  ComputedStyle* aComputedStyle) override;
  nscoord GetLogicalBaseline(mozilla::WritingMode aWritingMode) const override;
  const nsFrameList& GetChildList(ChildListID aListID) const override;
  void GetChildLists(nsTArray<ChildList>* aLists) const override;

  nsresult HandleEvent(nsPresContext* aPresContext,
                       mozilla::WidgetGUIEvent* aEvent,
                       nsEventStatus* aEventStatus) override;
  nsresult GetContentForEvent(mozilla::WidgetEvent* aEvent,
                              nsIContent** aContent) override;
  nsresult GetCursor(const nsPoint&    aPoint,
                     nsIFrame::Cursor& aCursor) override;

  nsresult GetPointFromOffset(int32_t  inOffset,
                              nsPoint* outPoint) override;
  nsresult GetCharacterRectsInRange(int32_t aInOffset,
                                    int32_t aLength,
                                    nsTArray<nsRect>& aOutRect) override;

  nsresult GetChildFrameContainingOffset(int32_t    inContentOffset,
                                         bool       inHint,
                                         int32_t*   outFrameContentOffset,
                                         nsIFrame** outChildFrame) override;

  static nsresult GetNextPrevLineFromeBlockFrame(nsPresContext* aPresContext,
                                                 nsPeekOffsetStruct *aPos,
                                                 nsIFrame *aBlockFrame,
                                                 int32_t aLineStart,
                                                 int8_t aOutSideLimit);

  nsresult CharacterDataChanged(const CharacterDataChangeInfo& aInfo) override;
  nsresult AttributeChanged(int32_t  aNameSpaceID,
                            nsAtom* aAttribute,
                            int32_t aModType) override;
  nsSplittableType GetSplittableType() const override;
  nsIFrame* GetPrevContinuation() const override;
  void SetPrevContinuation(nsIFrame*) override;
  nsIFrame* GetNextContinuation() const override;
  void SetNextContinuation(nsIFrame*) override;
  nsIFrame* GetPrevInFlowVirtual() const override;
  void SetPrevInFlow(nsIFrame*) override;
  nsIFrame* GetNextInFlowVirtual() const override;
  void SetNextInFlow(nsIFrame*) override;

  nsresult GetSelectionController(nsPresContext *aPresContext,
                                  nsISelectionController **aSelCon) override;

  FrameSearchResult PeekOffsetNoAmount(bool aForward,
                                       int32_t* aOffset) override;
  FrameSearchResult
  PeekOffsetCharacter(bool aForward, int32_t* aOffset,
                      PeekOffsetCharacterOptions aOptions =
                                PeekOffsetCharacterOptions()) override;
  FrameSearchResult PeekOffsetWord(bool aForward, bool aWordSelectEatSpace,
                                   bool aIsKeyboardSelect,
                                   int32_t* aOffset,
                                   PeekWordState *aState) override;
  /**
   * Check whether we should break at a boundary between punctuation and
   * non-punctuation. Only call it at a punctuation boundary
   * (i.e. exactly one of the previous and next characters are punctuation).
   * @param aForward true if we're moving forward in content order
   * @param aPunctAfter true if the next character is punctuation
   * @param aWhitespaceAfter true if the next character is whitespace
   */
  bool BreakWordBetweenPunctuation(const PeekWordState* aState,
                                   bool aForward,
                                   bool aPunctAfter, bool aWhitespaceAfter,
                                   bool aIsKeyboardSelect);

  nsresult CheckVisibility(nsPresContext* aContext,
                           int32_t aStartIndex, int32_t aEndIndex,
                           bool aRecurse, bool *aFinished,
                           bool *_retval) override;

  nsresult GetOffsets(int32_t &aStart, int32_t &aEnd) const override;
  void ChildIsDirty(nsIFrame* aChild) override;

#ifdef ACCESSIBILITY
  mozilla::a11y::AccType AccessibleType() override;
#endif

  ComputedStyle* GetParentComputedStyle(nsIFrame** aProviderFrame) const override {
    return DoGetParentComputedStyle(aProviderFrame);
  }

  /**
   * Do the work for getting the parent ComputedStyle frame so that
   * other frame's |GetParentComputedStyle| methods can call this
   * method on *another* frame.  (This function handles out-of-flow
   * frames by using the frame manager's placeholder map and it also
   * handles block-within-inline and generated content wrappers.)
   *
   * @param aProviderFrame (out) the frame associated with the returned value
   *   or null if the ComputedStyle is for display:contents content.
   * @return The ComputedStyle that should be the parent of this frame's
   *   ComputedStyle.  Null is permitted, and means that this frame's
   *   ComputedStyle should be the root of the ComputedStyle tree.
   */
  ComputedStyle* DoGetParentComputedStyle(nsIFrame** aProviderFrame) const;

  bool IsEmpty() override;
  bool IsSelfEmpty() override;

  void MarkIntrinsicISizesDirty() override;
  nscoord GetMinISize(gfxContext *aRenderingContext) override;
  nscoord GetPrefISize(gfxContext *aRenderingContext) override;
  void AddInlineMinISize(gfxContext *aRenderingContext,
                         InlineMinISizeData *aData) override;
  void AddInlinePrefISize(gfxContext *aRenderingContext,
                          InlinePrefISizeData *aData) override;
  IntrinsicISizeOffsetData
  IntrinsicISizeOffsets(nscoord aPercentageBasis = NS_UNCONSTRAINEDSIZE) override;
  mozilla::IntrinsicSize GetIntrinsicSize() override;
  nsSize GetIntrinsicRatio() override;

  mozilla::LogicalSize
  ComputeSize(gfxContext*                 aRenderingContext,
              mozilla::WritingMode        aWM,
              const mozilla::LogicalSize& aCBSize,
              nscoord                     aAvailableISize,
              const mozilla::LogicalSize& aMargin,
              const mozilla::LogicalSize& aBorder,
              const mozilla::LogicalSize& aPadding,
              ComputeSizeFlags            aFlags) override;

  /**
   * Calculate the used values for 'width' and 'height' for a replaced element.
   *   http://www.w3.org/TR/CSS21/visudet.html#min-max-widths
   */
  mozilla::LogicalSize
  ComputeSizeWithIntrinsicDimensions(
              gfxContext*                   aRenderingContext,
              mozilla::WritingMode          aWM,
              const mozilla::IntrinsicSize& aIntrinsicSize,
              nsSize                        aIntrinsicRatio,
              const mozilla::LogicalSize&   aCBSize,
              const mozilla::LogicalSize&   aMargin,
              const mozilla::LogicalSize&   aBorder,
              const mozilla::LogicalSize&   aPadding,
              ComputeSizeFlags              aFlags);

  // Compute tight bounds assuming this frame honours its border, background
  // and outline, its children's tight bounds, and nothing else.
  nsRect ComputeSimpleTightBounds(mozilla::gfx::DrawTarget* aDrawTarget) const;

  /**
   * A helper, used by |nsFrame::ComputeSize| (for frames that need to
   * override only this part of ComputeSize), that computes the size
   * that should be returned when 'width', 'height', and
   * min/max-width/height are all 'auto' or equivalent.
   *
   * In general, frames that can accept any computed width/height should
   * override only ComputeAutoSize, and frames that cannot do so need to
   * override ComputeSize to enforce their width/height invariants.
   *
   * Implementations may optimize by returning a garbage width if
   * StylePosition()->mWidth.GetUnit() != eStyleUnit_Auto, and
   * likewise for height, since in such cases the result is guaranteed
   * to be unused.
   */
  virtual mozilla::LogicalSize
  ComputeAutoSize(gfxContext*                 aRenderingContext,
                  mozilla::WritingMode        aWM,
                  const mozilla::LogicalSize& aCBSize,
                  nscoord                     aAvailableISize,
                  const mozilla::LogicalSize& aMargin,
                  const mozilla::LogicalSize& aBorder,
                  const mozilla::LogicalSize& aPadding,
                  ComputeSizeFlags            aFlags);

  /**
   * Utility function for ComputeAutoSize implementations.  Return
   * max(GetMinISize(), min(aISizeInCB, GetPrefISize()))
   */
  nscoord ShrinkWidthToFit(gfxContext*         aRenderingContext,
                           nscoord             aISizeInCB,
                           ComputeSizeFlags    aFlags);

  /**
   * Calculates the size of this frame after reflowing (calling Reflow on, and
   * updating the size and position of) its children, as necessary.  The
   * calculated size is returned to the caller via the ReflowOutput
   * outparam.  (The caller is responsible for setting the actual size and
   * position of this frame.)
   *
   * A frame's children must _all_ be reflowed if the frame is dirty (the
   * NS_FRAME_IS_DIRTY bit is set on it).  Otherwise, individual children
   * must be reflowed if they are dirty or have the NS_FRAME_HAS_DIRTY_CHILDREN
   * bit set on them.  Otherwise, whether children need to be reflowed depends
   * on the frame's type (it's up to individual Reflow methods), and on what
   * has changed.  For example, a change in the width of the frame may require
   * all of its children to be reflowed (even those without dirty bits set on
   * them), whereas a change in its height might not.
   * (ReflowInput::ShouldReflowAllKids may be helpful in deciding whether
   * to reflow all the children, but for some frame types it might result in
   * over-reflow.)
   *
   * Note: if it's only the overflow rect(s) of a frame that need to be
   * updated, then UpdateOverflow should be called instead of Reflow.
   */
  void Reflow(nsPresContext*     aPresContext,
              ReflowOutput&      aDesiredSize,
              const ReflowInput& aReflowInput,
              nsReflowStatus&    aStatus) override;
  void DidReflow(nsPresContext*     aPresContext,
                 const ReflowInput* aReflowInput) override;

  /**
   * NOTE: aStatus is assumed to be already-initialized. The reflow statuses of
   * any reflowed absolute children will be merged into aStatus; aside from
   * that, this method won't modify aStatus.
   */
  void ReflowAbsoluteFrames(nsPresContext*     aPresContext,
                            ReflowOutput&      aDesiredSize,
                            const ReflowInput& aReflowInput,
                            nsReflowStatus&    aStatus,
                            bool               aConstrainBSize = true);
  void FinishReflowWithAbsoluteFrames(nsPresContext*     aPresContext,
                                      ReflowOutput&      aDesiredSize,
                                      const ReflowInput& aReflowInput,
                                      nsReflowStatus&    aStatus,
                                      bool aConstrainBSize = true);

  /*
   * If this frame is dirty, marks all absolutely-positioned children of this
   * frame dirty. If this frame isn't dirty, or if there are no
   * absolutely-positioned children, does nothing.
   *
   * It's necessary to use PushDirtyBitToAbsoluteFrames() when you plan to
   * reflow this frame's absolutely-positioned children after the dirty bit on
   * this frame has already been cleared, which prevents ReflowInput from
   * propagating the dirty bit normally. This situation generally only arises
   * when a multipass layout algorithm is used.
   */
  void PushDirtyBitToAbsoluteFrames();

  bool CanContinueTextRun() const override;

  bool ComputeCustomOverflow(nsOverflowAreas& aOverflowAreas) override;

  void UnionChildOverflow(nsOverflowAreas& aOverflowAreas) override;

  // Selection Methods

  NS_IMETHOD HandlePress(nsPresContext* aPresContext,
                         mozilla::WidgetGUIEvent* aEvent,
                         nsEventStatus* aEventStatus);

  NS_IMETHOD HandleMultiplePress(nsPresContext* aPresContext,
                                 mozilla::WidgetGUIEvent* aEvent,
                                 nsEventStatus* aEventStatus,
                                 bool aControlHeld);

  NS_IMETHOD HandleDrag(nsPresContext* aPresContext,
                        mozilla::WidgetGUIEvent* aEvent,
                        nsEventStatus* aEventStatus);

  NS_IMETHOD HandleRelease(nsPresContext* aPresContext,
                           mozilla::WidgetGUIEvent* aEvent,
                           nsEventStatus* aEventStatus);

  enum { SELECT_ACCUMULATE = 0x01 };

  nsresult PeekBackwardAndForward(nsSelectionAmount aAmountBack,
                                  nsSelectionAmount aAmountForward,
                                  int32_t aStartPos,
                                  bool aJumpLines,
                                  uint32_t aSelectFlags);

  nsresult SelectByTypeAtPoint(nsPresContext* aPresContext,
                               const nsPoint& aPoint,
                               nsSelectionAmount aBeginAmountType,
                               nsSelectionAmount aEndAmountType,
                               uint32_t aSelectFlags);

  // Helper for GetContentAndOffsetsFromPoint; calculation of content offsets
  // in this function assumes there is no child frame that can be targeted.
  virtual ContentOffsets CalcContentOffsetsFromFramePoint(const nsPoint& aPoint);

  // Box layout methods
  nsSize GetXULPrefSize(nsBoxLayoutState& aBoxLayoutState) override;
  nsSize GetXULMinSize(nsBoxLayoutState& aBoxLayoutState) override;
  nsSize GetXULMaxSize(nsBoxLayoutState& aBoxLayoutState) override;
  nscoord GetXULFlex() override;
  nscoord GetXULBoxAscent(nsBoxLayoutState& aBoxLayoutState) override;

  // We compute and store the HTML content's overflow area. So don't
  // try to compute it in the box code.
  bool ComputesOwnOverflowArea() override { return true; }

  //--------------------------------------------------
  // Additional methods

  // Helper function that tests if the frame tree is too deep; if it is
  // it marks the frame as "unflowable", zeroes out the metrics, sets
  // the reflow status, and returns true. Otherwise, the frame is
  // unmarked "unflowable" and the metrics and reflow status are not
  // touched and false is returned.
  bool IsFrameTreeTooDeep(const ReflowInput& aReflowInput,
                            ReflowOutput& aMetrics,
                            nsReflowStatus& aStatus);

  // Incorporate the child overflow areas into aOverflowAreas.
  // If the child does not have a overflow, use the child area.
  void ConsiderChildOverflow(nsOverflowAreas& aOverflowAreas,
                             nsIFrame* aChildFrame);

  /**
   * @return true if we should avoid a page/column break in this frame.
   */
  bool ShouldAvoidBreakInside(const ReflowInput& aReflowInput) const;

#ifdef DEBUG
  /**
   * Tracing method that writes a method enter/exit routine to the
   * nspr log using the nsIFrame log module. The tracing is only
   * done when the NS_FRAME_TRACE_CALLS bit is set in the log module's
   * level field.
   */
  void Trace(const char* aMethod, bool aEnter);
  void Trace(const char* aMethod, bool aEnter, const nsReflowStatus& aStatus);
  void TraceMsg(const char* fmt, ...) MOZ_FORMAT_PRINTF(2, 3);

  // Helper function that verifies that each frame in the list has the
  // NS_FRAME_IS_DIRTY bit set
  static void VerifyDirtyBitSet(const nsFrameList& aFrameList);

  // Display Reflow Debugging
  static void* DisplayReflowEnter(nsPresContext*          aPresContext,
                                  nsIFrame*                aFrame,
                                  const ReflowInput& aReflowInput);
  static void* DisplayLayoutEnter(nsIFrame* aFrame);
  static void* DisplayIntrinsicISizeEnter(nsIFrame* aFrame,
                                          const char* aType);
  static void* DisplayIntrinsicSizeEnter(nsIFrame* aFrame,
                                         const char* aType);
  static void DisplayReflowExit(nsPresContext* aPresContext,
                                nsIFrame* aFrame,
                                ReflowOutput& aMetrics,
                                const nsReflowStatus& aStatus,
                                void* aFrameTreeNode);
  static void DisplayLayoutExit(nsIFrame* aFrame,
                                 void* aFrameTreeNode);
  static void DisplayIntrinsicISizeExit(nsIFrame* aFrame,
                                         const char* aType,
                                         nscoord aResult,
                                         void* aFrameTreeNode);
  static void DisplayIntrinsicSizeExit(nsIFrame* aFrame,
                                        const char* aType,
                                        nsSize aResult,
                                        void* aFrameTreeNode);

  static void DisplayReflowStartup();
  static void DisplayReflowShutdown();
#endif

  /**
   * Adds display items for standard CSS background if necessary.
   * Does not check IsVisibleForPainting.
   * @param aForceBackground draw the background even if the frame
   * background style appears to have no background --- this is useful
   * for frames that might receive a propagated background via
   * nsCSSRendering::FindBackground
   * @return whether a themed background item was created.
   */
  bool DisplayBackgroundUnconditional(nsDisplayListBuilder* aBuilder,
                                      const nsDisplayListSet& aLists,
                                      bool aForceBackground);
  /**
   * Adds display items for standard CSS borders, background and outline for
   * for this frame, as necessary. Checks IsVisibleForPainting and won't
   * display anything if the frame is not visible.
   * @param aForceBackground draw the background even if the frame
   * background style appears to have no background --- this is useful
   * for frames that might receive a propagated background via
   * nsCSSRendering::FindBackground
   */
  void DisplayBorderBackgroundOutline(nsDisplayListBuilder*   aBuilder,
                                      const nsDisplayListSet& aLists,
                                      bool aForceBackground = false);
  /**
   * Add a display item for the CSS outline. Does not check visibility.
   */
  void DisplayOutlineUnconditional(nsDisplayListBuilder*   aBuilder,
                                   const nsDisplayListSet& aLists);
  /**
   * Add a display item for the CSS outline, after calling
   * IsVisibleForPainting to confirm we are visible.
   */
  void DisplayOutline(nsDisplayListBuilder*   aBuilder,
                      const nsDisplayListSet& aLists);

  /**
   * Adjust the given parent frame to the right ComputedStyle parent frame for
   * the child, given the pseudo-type of the prospective child.  This handles
   * things like walking out of table pseudos and so forth.
   *
   * @param aProspectiveParent what GetParent() on the child returns.
   *                           Must not be null.
   * @param aChildPseudo the child's pseudo type, if any.
   */
  static nsIFrame*
  CorrectStyleParentFrame(nsIFrame* aProspectiveParent, nsAtom* aChildPseudo);

protected:
  // Protected constructor and destructor
  nsFrame(ComputedStyle* aStyle, ClassID aID);
  explicit nsFrame(ComputedStyle* aStyle)
    : nsFrame(aStyle, ClassID::nsFrame_id) {}
  virtual ~nsFrame();

  /**
   * To be called by |BuildDisplayLists| of this class or derived classes to add
   * a translucent overlay if this frame's content is selected.
   * @param aContentType an nsISelectionDisplay DISPLAY_ constant identifying
   * which kind of content this is for
   */
  void DisplaySelectionOverlay(nsDisplayListBuilder* aBuilder,
      nsDisplayList* aList, uint16_t aContentType = nsISelectionDisplay::DISPLAY_FRAMES);

  int16_t DisplaySelection(nsPresContext* aPresContext, bool isOkToTurnOn = false);

  // Style post processing hook
  void DidSetComputedStyle(ComputedStyle* aOldComputedStyle) override;

public:
  /**
   * Helper method to create a view for a frame.  Only used by a few sub-classes
   * that need a view.
   */
  void CreateView();

  //given a frame five me the first/last leaf available
  //XXX Robert O'Callahan wants to move these elsewhere
  static void GetLastLeaf(nsPresContext* aPresContext, nsIFrame **aFrame);
  static void GetFirstLeaf(nsPresContext* aPresContext, nsIFrame **aFrame);

  // Return the line number of the aFrame, and (optionally) the containing block
  // frame.
  // If aScrollLock is true, don't break outside scrollframes when looking for a
  // containing block frame.
  static int32_t GetLineNumber(nsIFrame *aFrame,
                               bool aLockScroll,
                               nsIFrame** aContainingBlock = nullptr);

  /**
   * Returns true if aFrame should apply overflow clipping.
   */
  static bool ShouldApplyOverflowClipping(const nsIFrame* aFrame,
                                          const nsStyleDisplay* aDisp)
  {
    // clip overflow:-moz-hidden-unscrollable, except for nsListControlFrame,
    // which is an nsHTMLScrollFrame.
    if (MOZ_UNLIKELY(aDisp->mOverflowX == NS_STYLE_OVERFLOW_CLIP &&
                     !aFrame->IsListControlFrame())) {
      return true;
    }

    // contain: paint, which we should interpret as -moz-hidden-unscrollable
    // by default.
    if (aDisp->IsContainPaint()) {
      return true;
    }

    // and overflow:hidden that we should interpret as -moz-hidden-unscrollable
    if (aDisp->mOverflowX == NS_STYLE_OVERFLOW_HIDDEN &&
        aDisp->mOverflowY == NS_STYLE_OVERFLOW_HIDDEN) {
      // REVIEW: these are the frame types that set up clipping.
      mozilla::LayoutFrameType type = aFrame->Type();
      if (type == mozilla::LayoutFrameType::Table ||
          type == mozilla::LayoutFrameType::TableCell ||
          type == mozilla::LayoutFrameType::BCTableCell ||
          type == mozilla::LayoutFrameType::SVGOuterSVG ||
          type == mozilla::LayoutFrameType::SVGInnerSVG ||
          type == mozilla::LayoutFrameType::SVGSymbol ||
          type == mozilla::LayoutFrameType::SVGForeignObject) {
        return true;
      }
      if (aFrame->IsFrameOfType(nsIFrame::eReplacedContainsBlock)) {
        if (type == mozilla::LayoutFrameType::TextInput) {
          // It always has an anonymous scroll frame that handles any overflow.
          return false;
        }
        return true;
      }
    }

    if ((aFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT)) {
      return false;
    }

    // If we're paginated and a block, and have NS_BLOCK_CLIP_PAGINATED_OVERFLOW
    // set, then we want to clip our overflow.
    return (aFrame->GetStateBits() & NS_BLOCK_CLIP_PAGINATED_OVERFLOW) != 0 &&
           aFrame->PresContext()->IsPaginated() && aFrame->IsBlockFrame();
  }

  nsILineIterator* GetLineIterator() override;

protected:

  // Test if we are selecting a table object:
  //  Most table/cell selection requires that Ctrl (Cmd on Mac) key is down
  //   during a mouse click or drag. Exception is using Shift+click when
  //   already in "table/cell selection mode" to extend a block selection
  //  Get the parent content node and offset of the frame
  //   of the enclosing cell or table (if not inside a cell)
  //  aTarget tells us what table element to select (currently only cell and table supported)
  //  (enums for this are defined in nsIFrame.h)
  nsresult GetDataForTableSelection(const nsFrameSelection* aFrameSelection,
                                    nsIPresShell* aPresShell,
                                    mozilla::WidgetMouseEvent* aMouseEvent,
                                    nsIContent** aParentContent,
                                    int32_t* aContentOffset,
                                    mozilla::TableSelection* aTarget);

  // Fills aCursor with the appropriate information from ui
  static void FillCursorInformationFromStyle(const nsStyleUserInterface* ui,
                                             nsIFrame::Cursor& aCursor);
  NS_IMETHOD DoXULLayout(nsBoxLayoutState& aBoxLayoutState) override;

  nsBoxLayoutMetrics* BoxMetrics() const;

  // Fire DOM event. If no aContent argument use frame's mContent.
  void FireDOMEvent(const nsAString& aDOMEventName, nsIContent *aContent = nullptr);

private:
  void BoxReflow(nsBoxLayoutState& aState,
                 nsPresContext*    aPresContext,
                 ReflowOutput&     aDesiredSize,
                 gfxContext*       aRenderingContext,
                 nscoord aX,
                 nscoord aY,
                 nscoord aWidth,
                 nscoord aHeight,
                 bool aMoveFrame = true);

  NS_IMETHODIMP RefreshSizeCache(nsBoxLayoutState& aState);

  // Returns true if this frame has any kind of CSS animations.
  bool HasCSSAnimations();

  // Returns true if this frame has any kind of CSS transitions.
  bool HasCSSTransitions();

public:

#ifdef DEBUG_FRAME_DUMP
  /**
   * Get a printable from of the name of the frame type.
   * XXX This should be eliminated and we use GetType() instead...
   */
  nsresult GetFrameName(nsAString& aResult) const override;
  nsresult MakeFrameName(const nsAString& aKind, nsAString& aResult) const;
  // Helper function to return the index in parent of the frame's content
  // object. Returns -1 on error or if the frame doesn't have a content object
  static int32_t ContentIndexInContainer(const nsIFrame* aFrame);
#endif

#ifdef DEBUG
  static mozilla::LazyLogModule sFrameLogModule;

  // Show frame borders when rendering
  static void ShowFrameBorders(bool aEnable);
  static bool GetShowFrameBorders();

  // Show frame border of event target
  static void ShowEventTargetFrameBorder(bool aEnable);
  static bool GetShowEventTargetFrameBorder();
#endif

  static void PrintDisplayList(nsDisplayListBuilder* aBuilder,
                               const nsDisplayList& aList,
                               bool aDumpHtml = false)
  {
    std::stringstream ss;
    PrintDisplayList(aBuilder, aList, ss, aDumpHtml);
    fprintf_stderr(stderr, "%s", ss.str().c_str());
  }
  static void PrintDisplayList(nsDisplayListBuilder* aBuilder,
                               const nsDisplayList& aList,
                               std::stringstream& aStream,
                               bool aDumpHtml = false);
  static void PrintDisplayListSet(nsDisplayListBuilder* aBuilder,
                                  const nsDisplayListSet& aList,
                                  std::stringstream& aStream,
                                  bool aDumpHtml = false);

};

// Start Display Reflow Debugging
#ifdef DEBUG

  struct DR_cookie {
    DR_cookie(nsPresContext*          aPresContext,
              nsIFrame*                aFrame,
              const mozilla::ReflowInput& aReflowInput,
              mozilla::ReflowOutput&     aMetrics,
              nsReflowStatus&          aStatus);
    ~DR_cookie();
    void Change() const;

    nsPresContext*          mPresContext;
    nsIFrame*                mFrame;
    const mozilla::ReflowInput& mReflowInput;
    mozilla::ReflowOutput&     mMetrics;
    nsReflowStatus&          mStatus;
    void*                    mValue;
  };

  struct DR_layout_cookie {
    explicit DR_layout_cookie(nsIFrame* aFrame);
    ~DR_layout_cookie();

    nsIFrame* mFrame;
    void* mValue;
  };

  struct DR_intrinsic_width_cookie {
    DR_intrinsic_width_cookie(nsIFrame* aFrame, const char* aType,
                              nscoord& aResult);
    ~DR_intrinsic_width_cookie();

    nsIFrame* mFrame;
    const char* mType;
    nscoord& mResult;
    void* mValue;
  };

  struct DR_intrinsic_size_cookie {
    DR_intrinsic_size_cookie(nsIFrame* aFrame, const char* aType,
                             nsSize& aResult);
    ~DR_intrinsic_size_cookie();

    nsIFrame* mFrame;
    const char* mType;
    nsSize& mResult;
    void* mValue;
  };

  struct DR_init_constraints_cookie {
    DR_init_constraints_cookie(nsIFrame* aFrame, mozilla::ReflowInput* aState,
                               nscoord aCBWidth, nscoord aCBHeight,
                               const nsMargin* aBorder,
                               const nsMargin* aPadding);
    ~DR_init_constraints_cookie();

    nsIFrame* mFrame;
    mozilla::ReflowInput* mState;
    void* mValue;
  };

  struct DR_init_offsets_cookie {
    DR_init_offsets_cookie(nsIFrame* aFrame, mozilla::SizeComputationInput* aState,
                           nscoord aPercentBasis,
                           mozilla::WritingMode aCBWritingMode,
                           const nsMargin* aBorder,
                           const nsMargin* aPadding);
    ~DR_init_offsets_cookie();

    nsIFrame* mFrame;
    mozilla::SizeComputationInput* mState;
    void* mValue;
  };

  struct DR_init_type_cookie {
    DR_init_type_cookie(nsIFrame* aFrame, mozilla::ReflowInput* aState);
    ~DR_init_type_cookie();

    nsIFrame* mFrame;
    mozilla::ReflowInput* mState;
    void* mValue;
  };

#define DISPLAY_REFLOW(dr_pres_context, dr_frame, dr_rf_state, dr_rf_metrics, dr_rf_status) \
  DR_cookie dr_cookie(dr_pres_context, dr_frame, dr_rf_state, dr_rf_metrics, dr_rf_status);
#define DISPLAY_REFLOW_CHANGE() \
  dr_cookie.Change();
#define DISPLAY_LAYOUT(dr_frame) \
  DR_layout_cookie dr_cookie(dr_frame);
#define DISPLAY_MIN_WIDTH(dr_frame, dr_result) \
  DR_intrinsic_width_cookie dr_cookie(dr_frame, "Min", dr_result)
#define DISPLAY_PREF_WIDTH(dr_frame, dr_result) \
  DR_intrinsic_width_cookie dr_cookie(dr_frame, "Pref", dr_result)
#define DISPLAY_PREF_SIZE(dr_frame, dr_result) \
  DR_intrinsic_size_cookie dr_cookie(dr_frame, "Pref", dr_result)
#define DISPLAY_MIN_SIZE(dr_frame, dr_result) \
  DR_intrinsic_size_cookie dr_cookie(dr_frame, "Min", dr_result)
#define DISPLAY_MAX_SIZE(dr_frame, dr_result) \
  DR_intrinsic_size_cookie dr_cookie(dr_frame, "Max", dr_result)
#define DISPLAY_INIT_CONSTRAINTS(dr_frame, dr_state, dr_cbw, dr_cbh,       \
                                 dr_bdr, dr_pad)                           \
  DR_init_constraints_cookie dr_cookie(dr_frame, dr_state, dr_cbw, dr_cbh, \
                                       dr_bdr, dr_pad)
#define DISPLAY_INIT_OFFSETS(dr_frame, dr_state, dr_pb, dr_cbwm, dr_bdr,      \
                             dr_pad)                                          \
  DR_init_offsets_cookie dr_cookie(dr_frame, dr_state, dr_pb, dr_cbwm,        \
                             dr_bdr, dr_pad)
#define DISPLAY_INIT_TYPE(dr_frame, dr_result) \
  DR_init_type_cookie dr_cookie(dr_frame, dr_result)

#else

#define DISPLAY_REFLOW(dr_pres_context, dr_frame, dr_rf_state, dr_rf_metrics, dr_rf_status)
#define DISPLAY_REFLOW_CHANGE()
#define DISPLAY_LAYOUT(dr_frame) PR_BEGIN_MACRO PR_END_MACRO
#define DISPLAY_MIN_WIDTH(dr_frame, dr_result) PR_BEGIN_MACRO PR_END_MACRO
#define DISPLAY_PREF_WIDTH(dr_frame, dr_result) PR_BEGIN_MACRO PR_END_MACRO
#define DISPLAY_PREF_SIZE(dr_frame, dr_result) PR_BEGIN_MACRO PR_END_MACRO
#define DISPLAY_MIN_SIZE(dr_frame, dr_result) PR_BEGIN_MACRO PR_END_MACRO
#define DISPLAY_MAX_SIZE(dr_frame, dr_result) PR_BEGIN_MACRO PR_END_MACRO
#define DISPLAY_INIT_CONSTRAINTS(dr_frame, dr_state, dr_cbw, dr_cbh,       \
                                 dr_bdr, dr_pad)                           \
  PR_BEGIN_MACRO PR_END_MACRO
#define DISPLAY_INIT_OFFSETS(dr_frame, dr_state, dr_pb, dr_cbwm, dr_bdr,      \
                             dr_pad)                                          \
  PR_BEGIN_MACRO PR_END_MACRO
#define DISPLAY_INIT_TYPE(dr_frame, dr_result) PR_BEGIN_MACRO PR_END_MACRO

#endif
// End Display Reflow Debugging

// similar to NS_ENSURE_TRUE but with no return value
#define ENSURE_TRUE(x)                                        \
  PR_BEGIN_MACRO                                              \
    if (!(x)) {                                               \
       NS_WARNING("ENSURE_TRUE(" #x ") failed");              \
       return;                                                \
    }                                                         \
  PR_END_MACRO
#endif /* nsFrame_h___ */

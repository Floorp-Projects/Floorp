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
#include "mozilla/Logging.h"

#include "mozilla/ReflowInput.h"
#include "nsHTMLParts.h"
#include "nsISelectionDisplay.h"

namespace mozilla {
enum class TableSelectionMode : uint32_t;
class PresShell;
}  // namespace mozilla

/**
 * nsFrame logging constants. We redefine the nspr
 * PRLogModuleInfo.level field to be a bitfield.  Each bit controls a
 * specific type of logging. Each logging operation has associated
 * inline methods defined below.
 *
 * Due to the redefinition of the level field we cannot use MOZ_LOG directly
 * as that will cause assertions due to invalid log levels.
 */
#define NS_FRAME_TRACE_CALLS 0x1
#define NS_FRAME_TRACE_PUSH_PULL 0x2
#define NS_FRAME_TRACE_CHILD_REFLOW 0x4
#define NS_FRAME_TRACE_NEW_FRAMES 0x8

#define NS_FRAME_LOG_TEST(_lm, _bit) \
  (int(((mozilla::LogModule*)_lm)->Level()) & (_bit))

#ifdef DEBUG
#  define NS_FRAME_LOG(_bit, _args)                          \
    PR_BEGIN_MACRO                                           \
    if (NS_FRAME_LOG_TEST(nsFrame::sFrameLogModule, _bit)) { \
      printf_stderr _args;                                   \
    }                                                        \
    PR_END_MACRO
#else
#  define NS_FRAME_LOG(_bit, _args)
#endif

// XXX Need to rework this so that logging is free when it's off
#ifdef DEBUG
#  define NS_FRAME_TRACE_IN(_method) Trace(_method, true)

#  define NS_FRAME_TRACE_OUT(_method) Trace(_method, false)

#  define NS_FRAME_TRACE(_bit, _args)                        \
    PR_BEGIN_MACRO                                           \
    if (NS_FRAME_LOG_TEST(nsFrame::sFrameLogModule, _bit)) { \
      TraceMsg _args;                                        \
    }                                                        \
    PR_END_MACRO

#  define NS_FRAME_TRACE_REFLOW_IN(_method) Trace(_method, true)

#  define NS_FRAME_TRACE_REFLOW_OUT(_method, _status) \
    Trace(_method, false, _status)

#else
#  define NS_FRAME_TRACE(_bits, _args)
#  define NS_FRAME_TRACE_IN(_method)
#  define NS_FRAME_TRACE_OUT(_method)
#  define NS_FRAME_TRACE_REFLOW_IN(_method)
#  define NS_FRAME_TRACE_REFLOW_OUT(_method, _status)
#endif

// Frame allocation boilerplate macros. Every subclass of nsFrame must
// either use NS_{DECL,IMPL}_FRAMEARENA_HELPERS pair for allocating
// memory correctly, or use NS_DECL_ABSTRACT_FRAME to declare a frame
// class abstract and stop it from being instantiated. If a frame class
// without its own operator new and GetFrameId gets instantiated, the
// per-frame recycler lists in nsPresArena will not work correctly,
// with potentially catastrophic consequences (not enough memory is
// allocated for a frame object).

#define NS_DECL_FRAMEARENA_HELPERS(class)                                      \
  NS_DECL_QUERYFRAME_TARGET(class)                                             \
  static constexpr nsIFrame::ClassID kClassID = nsIFrame::ClassID::class##_id; \
  void* operator new(size_t, mozilla::PresShell*) MOZ_MUST_OVERRIDE;           \
  nsQueryFrame::FrameIID GetFrameId() const override MOZ_MUST_OVERRIDE {       \
    return nsQueryFrame::class##_id;                                           \
  }

#define NS_IMPL_FRAMEARENA_HELPERS(class)                             \
  void* class ::operator new(size_t sz, mozilla::PresShell* aShell) { \
    return aShell->AllocateFrame(nsQueryFrame::class##_id, sz);       \
  }

#define NS_DECL_ABSTRACT_FRAME(class)                                         \
  void* operator new(size_t, mozilla::PresShell*) MOZ_MUST_OVERRIDE = delete; \
  nsQueryFrame::FrameIID GetFrameId() const override MOZ_MUST_OVERRIDE = 0;

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
class nsFrame : public nsIFrame {
 public:
  /**
   * Create a new "empty" frame that maps a given piece of content into a
   * 0,0 area.
   */
  friend nsIFrame* NS_NewEmptyFrame(mozilla::PresShell* aShell,
                                    ComputedStyle* aStyle);

 private:
  // Left undefined; nsFrame objects are never allocated from the heap.
  void* operator new(size_t sz) noexcept(true);

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
  virtual nsQueryFrame::FrameIID GetFrameId() const MOZ_MUST_OVERRIDE {
    return kFrameIID;
  }
  void* operator new(size_t, mozilla::PresShell*) MOZ_MUST_OVERRIDE;

  // nsIFrame
  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;
  void DestroyFrom(nsIFrame* aDestructRoot,
                   PostDestroyData& aPostDestroyData) override;

  static nsresult GetNextPrevLineFromeBlockFrame(nsPresContext* aPresContext,
                                                 nsPeekOffsetStruct* aPos,
                                                 nsIFrame* aBlockFrame,
                                                 int32_t aLineStart,
                                                 int8_t aOutSideLimit);

  /**
   * Calculate the used values for 'width' and 'height' for a replaced element.
   *   http://www.w3.org/TR/CSS21/visudet.html#min-max-widths
   */
  mozilla::LogicalSize ComputeSizeWithIntrinsicDimensions(
      gfxContext* aRenderingContext, mozilla::WritingMode aWM,
      const mozilla::IntrinsicSize& aIntrinsicSize,
      const mozilla::AspectRatio& aIntrinsicRatio,
      const mozilla::LogicalSize& aCBSize, const mozilla::LogicalSize& aMargin,
      const mozilla::LogicalSize& aBorder, const mozilla::LogicalSize& aPadding,
      ComputeSizeFlags aFlags);

  // Compute tight bounds assuming this frame honours its border, background
  // and outline, its children's tight bounds, and nothing else.
  nsRect ComputeSimpleTightBounds(mozilla::gfx::DrawTarget* aDrawTarget) const;

  void DidReflow(nsPresContext* aPresContext,
                 const ReflowInput* aReflowInput) override;

  void FinishReflowWithAbsoluteFrames(nsPresContext* aPresContext,
                                      ReflowOutput& aDesiredSize,
                                      const ReflowInput& aReflowInput,
                                      nsReflowStatus& aStatus,
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

  nsresult PeekBackwardAndForward(nsSelectionAmount aAmountBack,
                                  nsSelectionAmount aAmountForward,
                                  int32_t aStartPos, bool aJumpLines,
                                  uint32_t aSelectFlags);

  // Helper for GetContentAndOffsetsFromPoint; calculation of content offsets
  // in this function assumes there is no child frame that can be targeted.
  virtual ContentOffsets CalcContentOffsetsFromFramePoint(
      const nsPoint& aPoint);

  //--------------------------------------------------
  // Additional methods

  // Helper function that tests if the frame tree is too deep; if it is
  // it marks the frame as "unflowable", zeroes out the metrics, sets
  // the reflow status, and returns true. Otherwise, the frame is
  // unmarked "unflowable" and the metrics and reflow status are not
  // touched and false is returned.
  bool IsFrameTreeTooDeep(const ReflowInput& aReflowInput,
                          ReflowOutput& aMetrics, nsReflowStatus& aStatus);

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
  static void* DisplayReflowEnter(nsPresContext* aPresContext, nsIFrame* aFrame,
                                  const ReflowInput& aReflowInput);
  static void* DisplayLayoutEnter(nsIFrame* aFrame);
  static void* DisplayIntrinsicISizeEnter(nsIFrame* aFrame, const char* aType);
  static void* DisplayIntrinsicSizeEnter(nsIFrame* aFrame, const char* aType);
  static void DisplayReflowExit(nsPresContext* aPresContext, nsIFrame* aFrame,
                                ReflowOutput& aMetrics,
                                const nsReflowStatus& aStatus,
                                void* aFrameTreeNode);
  static void DisplayLayoutExit(nsIFrame* aFrame, void* aFrameTreeNode);
  static void DisplayIntrinsicISizeExit(nsIFrame* aFrame, const char* aType,
                                        nscoord aResult, void* aFrameTreeNode);
  static void DisplayIntrinsicSizeExit(nsIFrame* aFrame, const char* aType,
                                       nsSize aResult, void* aFrameTreeNode);

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
  void DisplayBorderBackgroundOutline(nsDisplayListBuilder* aBuilder,
                                      const nsDisplayListSet& aLists,
                                      bool aForceBackground = false);
  /**
   * Add a display item for the CSS outline. Does not check visibility.
   */
  void DisplayOutlineUnconditional(nsDisplayListBuilder* aBuilder,
                                   const nsDisplayListSet& aLists);
  /**
   * Add a display item for the CSS outline, after calling
   * IsVisibleForPainting to confirm we are visible.
   */
  void DisplayOutline(nsDisplayListBuilder* aBuilder,
                      const nsDisplayListSet& aLists);

  /**
   * Add a display item for CSS inset box shadows. Does not check visibility.
   */
  void DisplayInsetBoxShadowUnconditional(nsDisplayListBuilder* aBuilder,
                                          nsDisplayList* aList);

  /**
   * Add a display item for CSS inset box shadow, after calling
   * IsVisibleForPainting to confirm we are visible.
   */
  void DisplayInsetBoxShadow(nsDisplayListBuilder* aBuilder,
                             nsDisplayList* aList);

  /**
   * Add a display item for CSS outset box shadows. Does not check visibility.
   */
  void DisplayOutsetBoxShadowUnconditional(nsDisplayListBuilder* aBuilder,
                                           nsDisplayList* aList);

  /**
   * Add a display item for CSS outset box shadow, after calling
   * IsVisibleForPainting to confirm we are visible.
   */
  void DisplayOutsetBoxShadow(nsDisplayListBuilder* aBuilder,
                              nsDisplayList* aList);

  /**
   * Adjust the given parent frame to the right ComputedStyle parent frame for
   * the child, given the pseudo-type of the prospective child.  This handles
   * things like walking out of table pseudos and so forth.
   *
   * @param aProspectiveParent what GetParent() on the child returns.
   *                           Must not be null.
   * @param aChildPseudo the child's pseudo type, if any.
   */
  static nsIFrame* CorrectStyleParentFrame(
      nsIFrame* aProspectiveParent, mozilla::PseudoStyleType aChildPseudo);

 protected:
  // Protected constructor and destructor
  nsFrame(ComputedStyle* aStyle, nsPresContext* aPresContext, ClassID aID);
  explicit nsFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : nsFrame(aStyle, aPresContext, ClassID::nsFrame_id) {}
  virtual ~nsFrame();

  /**
   * To be called by |BuildDisplayLists| of this class or derived classes to add
   * a translucent overlay if this frame's content is selected.
   * @param aContentType an nsISelectionDisplay DISPLAY_ constant identifying
   * which kind of content this is for
   */
  void DisplaySelectionOverlay(
      nsDisplayListBuilder* aBuilder, nsDisplayList* aList,
      uint16_t aContentType = nsISelectionDisplay::DISPLAY_FRAMES);

 public:
  /**
   * Helper method to create a view for a frame.  Only used by a few sub-classes
   * that need a view.
   */
  void CreateView();

  // given a frame five me the first/last leaf available
  // XXX Robert O'Callahan wants to move these elsewhere
  static void GetLastLeaf(nsPresContext* aPresContext, nsIFrame** aFrame);
  static void GetFirstLeaf(nsPresContext* aPresContext, nsIFrame** aFrame);

  // Return the line number of the aFrame, and (optionally) the containing block
  // frame.
  // If aScrollLock is true, don't break outside scrollframes when looking for a
  // containing block frame.
  static int32_t GetLineNumber(nsIFrame* aFrame, bool aLockScroll,
                               nsIFrame** aContainingBlock = nullptr);

  /**
   * Returns true if aFrame should apply overflow clipping.
   */
  static bool ShouldApplyOverflowClipping(const nsIFrame* aFrame,
                                          const nsStyleDisplay* aDisp) {
    MOZ_ASSERT(aDisp == aFrame->StyleDisplay(), "Wong display struct");
    // clip overflow:-moz-hidden-unscrollable, except for nsListControlFrame,
    // which is an nsHTMLScrollFrame.
    if (MOZ_UNLIKELY(aDisp->mOverflowX ==
                         mozilla::StyleOverflow::MozHiddenUnscrollable &&
                     !aFrame->IsListControlFrame())) {
      return true;
    }

    // contain: paint, which we interpret as -moz-hidden-unscrollable
    // Exception: for scrollframes, we don't need contain:paint to add any
    // clipping, because the scrollable frame will already clip overflowing
    // content, and because contain:paint should prevent all means of escaping
    // that clipping (e.g. because it forms a fixed-pos containing block).
    if (aDisp->IsContainPaint() && !aFrame->IsScrollFrame() &&
        aFrame->IsFrameOfType(eSupportsContainLayoutAndPaint)) {
      return true;
    }

    // and overflow:hidden that we should interpret as -moz-hidden-unscrollable
    if (aDisp->mOverflowX == mozilla::StyleOverflow::Hidden &&
        aDisp->mOverflowY == mozilla::StyleOverflow::Hidden) {
      // REVIEW: these are the frame types that set up clipping.
      mozilla::LayoutFrameType type = aFrame->Type();
      if (type == mozilla::LayoutFrameType::Table ||
          type == mozilla::LayoutFrameType::TableCell ||
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

 protected:
  // Fire DOM event. If no aContent argument use frame's mContent.
  void FireDOMEvent(const nsAString& aDOMEventName,
                    nsIContent* aContent = nullptr);

 private:
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
                               bool aDumpHtml = false) {
    std::stringstream ss;
    PrintDisplayList(aBuilder, aList, ss, aDumpHtml);
    fprintf_stderr(stderr, "%s", ss.str().c_str());
  }
  static void PrintDisplayList(nsDisplayListBuilder* aBuilder,
                               const nsDisplayList& aList,
                               std::stringstream& aStream,
                               bool aDumpHtml = false);
  static void PrintDisplayItem(nsDisplayListBuilder* aBuilder,
                               nsDisplayItem* aItem, std::stringstream& aStream,
                               uint32_t aIndent = 0, bool aDumpSublist = false,
                               bool aDumpHtml = false);
  static void PrintDisplayListSet(nsDisplayListBuilder* aBuilder,
                                  const nsDisplayListSet& aList,
                                  std::stringstream& aStream,
                                  bool aDumpHtml = false);
};

// Start Display Reflow Debugging
#ifdef DEBUG

struct DR_cookie {
  DR_cookie(nsPresContext* aPresContext, nsIFrame* aFrame,
            const mozilla::ReflowInput& aReflowInput,
            mozilla::ReflowOutput& aMetrics, nsReflowStatus& aStatus);
  ~DR_cookie();
  void Change() const;

  nsPresContext* mPresContext;
  nsIFrame* mFrame;
  const mozilla::ReflowInput& mReflowInput;
  mozilla::ReflowOutput& mMetrics;
  nsReflowStatus& mStatus;
  void* mValue;
};

struct DR_layout_cookie {
  explicit DR_layout_cookie(nsIFrame* aFrame);
  ~DR_layout_cookie();

  nsIFrame* mFrame;
  void* mValue;
};

struct DR_intrinsic_inline_size_cookie {
  DR_intrinsic_inline_size_cookie(nsIFrame* aFrame, const char* aType,
                                  nscoord& aResult);
  ~DR_intrinsic_inline_size_cookie();

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
                             const nsMargin* aBorder, const nsMargin* aPadding);
  ~DR_init_constraints_cookie();

  nsIFrame* mFrame;
  mozilla::ReflowInput* mState;
  void* mValue;
};

struct DR_init_offsets_cookie {
  DR_init_offsets_cookie(nsIFrame* aFrame,
                         mozilla::SizeComputationInput* aState,
                         nscoord aPercentBasis,
                         mozilla::WritingMode aCBWritingMode,
                         const nsMargin* aBorder, const nsMargin* aPadding);
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

#  define DISPLAY_REFLOW(dr_pres_context, dr_frame, dr_rf_state,               \
                         dr_rf_metrics, dr_rf_status)                          \
    DR_cookie dr_cookie(dr_pres_context, dr_frame, dr_rf_state, dr_rf_metrics, \
                        dr_rf_status);
#  define DISPLAY_REFLOW_CHANGE() dr_cookie.Change();
#  define DISPLAY_LAYOUT(dr_frame) DR_layout_cookie dr_cookie(dr_frame);
#  define DISPLAY_MIN_INLINE_SIZE(dr_frame, dr_result) \
    DR_intrinsic_inline_size_cookie dr_cookie(dr_frame, "Min", dr_result)
#  define DISPLAY_PREF_INLINE_SIZE(dr_frame, dr_result) \
    DR_intrinsic_inline_size_cookie dr_cookie(dr_frame, "Pref", dr_result)
#  define DISPLAY_PREF_SIZE(dr_frame, dr_result) \
    DR_intrinsic_size_cookie dr_cookie(dr_frame, "Pref", dr_result)
#  define DISPLAY_MIN_SIZE(dr_frame, dr_result) \
    DR_intrinsic_size_cookie dr_cookie(dr_frame, "Min", dr_result)
#  define DISPLAY_MAX_SIZE(dr_frame, dr_result) \
    DR_intrinsic_size_cookie dr_cookie(dr_frame, "Max", dr_result)
#  define DISPLAY_INIT_CONSTRAINTS(dr_frame, dr_state, dr_cbw, dr_cbh, dr_bdr, \
                                   dr_pad)                                     \
    DR_init_constraints_cookie dr_cookie(dr_frame, dr_state, dr_cbw, dr_cbh,   \
                                         dr_bdr, dr_pad)
#  define DISPLAY_INIT_OFFSETS(dr_frame, dr_state, dr_pb, dr_cbwm, dr_bdr, \
                               dr_pad)                                     \
    DR_init_offsets_cookie dr_cookie(dr_frame, dr_state, dr_pb, dr_cbwm,   \
                                     dr_bdr, dr_pad)
#  define DISPLAY_INIT_TYPE(dr_frame, dr_result) \
    DR_init_type_cookie dr_cookie(dr_frame, dr_result)

#else

#  define DISPLAY_REFLOW(dr_pres_context, dr_frame, dr_rf_state, \
                         dr_rf_metrics, dr_rf_status)
#  define DISPLAY_REFLOW_CHANGE()
#  define DISPLAY_LAYOUT(dr_frame) PR_BEGIN_MACRO PR_END_MACRO
#  define DISPLAY_MIN_INLINE_SIZE(dr_frame, dr_result) \
    PR_BEGIN_MACRO PR_END_MACRO
#  define DISPLAY_PREF_INLINE_SIZE(dr_frame, dr_result) \
    PR_BEGIN_MACRO PR_END_MACRO
#  define DISPLAY_PREF_SIZE(dr_frame, dr_result) PR_BEGIN_MACRO PR_END_MACRO
#  define DISPLAY_MIN_SIZE(dr_frame, dr_result) PR_BEGIN_MACRO PR_END_MACRO
#  define DISPLAY_MAX_SIZE(dr_frame, dr_result) PR_BEGIN_MACRO PR_END_MACRO
#  define DISPLAY_INIT_CONSTRAINTS(dr_frame, dr_state, dr_cbw, dr_cbh, dr_bdr, \
                                   dr_pad)                                     \
    PR_BEGIN_MACRO PR_END_MACRO
#  define DISPLAY_INIT_OFFSETS(dr_frame, dr_state, dr_pb, dr_cbwm, dr_bdr, \
                               dr_pad)                                     \
    PR_BEGIN_MACRO PR_END_MACRO
#  define DISPLAY_INIT_TYPE(dr_frame, dr_result) PR_BEGIN_MACRO PR_END_MACRO

#endif
// End Display Reflow Debugging

#endif /* nsFrame_h___ */

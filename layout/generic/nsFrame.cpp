/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class of all rendering objects */

#include "nsFrame.h"

#include <stdarg.h>
#include <algorithm>

#include "gfx2DGlue.h"
#include "gfxUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/ComputedStyle.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/dom/ElementInlines.h"
#include "mozilla/dom/ImageTracker.h"
#include "mozilla/dom/Selection.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/PathHelpers.h"
#include "mozilla/PresShell.h"
#include "mozilla/PresShellInlines.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/ToString.h"

#include "nsCOMPtr.h"
#include "nsFlexContainerFrame.h"
#include "nsFrameList.h"
#include "nsPlaceholderFrame.h"
#include "nsPluginFrame.h"
#include "nsIBaseWindow.h"
#include "nsIContent.h"
#include "nsIContentInlines.h"
#include "nsContentUtils.h"
#include "nsCSSFrameConstructor.h"
#include "nsCSSProps.h"
#include "nsCSSPseudoElements.h"
#include "nsCSSRendering.h"
#include "nsAtom.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsTableWrapperFrame.h"
#include "nsView.h"
#include "nsViewManager.h"
#include "nsIScrollableFrame.h"
#include "nsPresContext.h"
#include "nsPresContextInlines.h"
#include "nsStyleConsts.h"
#include "mozilla/Logging.h"
#include "nsLayoutUtils.h"
#include "LayoutLogging.h"
#include "mozilla/RestyleManager.h"
#include "nsImageFrame.h"
#include "nsInlineFrame.h"
#include "nsFrameSelection.h"
#include "nsGkAtoms.h"
#include "nsCSSAnonBoxes.h"
#include "nsCSSClipPathInstance.h"
#include "nsCanvasFrame.h"

#include "nsFrameTraversal.h"
#include "nsRange.h"
#include "nsITextControlFrame.h"
#include "nsNameSpaceManager.h"
#include "nsIPercentBSizeObserver.h"
#include "nsStyleStructInlines.h"
#include "FrameLayerBuilder.h"
#include "ImageLayers.h"

#include "nsBidiPresUtils.h"
#include "RubyUtils.h"
#include "TextOverflow.h"
#include "nsAnimationManager.h"

// For triple-click pref
#include "imgIRequest.h"
#include "nsError.h"
#include "nsContainerFrame.h"
#include "nsBoxLayoutState.h"
#include "nsBlockFrame.h"
#include "nsDisplayList.h"
#include "nsSVGIntegrationUtils.h"
#include "SVGObserverUtils.h"
#include "nsSVGMaskFrame.h"
#include "nsChangeHint.h"
#include "nsDeckFrame.h"
#include "nsSubDocumentFrame.h"
#include "SVGTextFrame.h"
#include "RetainedDisplayListBuilder.h"

#include "gfxContext.h"
#include "nsAbsoluteContainingBlock.h"
#include "StickyScrollContainer.h"
#include "nsFontInflationData.h"
#include "nsRegion.h"
#include "nsIFrameInlines.h"
#include "nsStyleChangeList.h"
#include "nsWindowSizes.h"

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/EffectCompositor.h"
#include "mozilla/EffectSet.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/EventStates.h"
#include "mozilla/Preferences.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/ServoStyleSetInlines.h"
#include "mozilla/css/ImageLoader.h"
#include "mozilla/dom/HTMLBodyElement.h"
#include "mozilla/dom/SVGPathData.h"
#include "mozilla/dom/TouchEvent.h"
#include "mozilla/gfx/Tools.h"
#include "mozilla/layers/WebRenderUserData.h"
#include "mozilla/layout/ScrollAnchorContainer.h"
#include "nsPrintfCString.h"
#include "ActiveLayerTracker.h"

#include "nsITheme.h"

using namespace mozilla;
using namespace mozilla::css;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::layers;
using namespace mozilla::layout;
typedef nsAbsoluteContainingBlock::AbsPosReflowFlags AbsPosReflowFlags;
using nsStyleTransformMatrix::TransformReferenceBox;

const mozilla::LayoutFrameType nsIFrame::sLayoutFrameTypes[
#define FRAME_ID(...) 1 +
#define ABSTRACT_FRAME_ID(...)
#include "mozilla/FrameIdList.h"
#undef FRAME_ID
#undef ABSTRACT_FRAME_ID
    0] = {
#define FRAME_ID(class_, type_, ...) mozilla::LayoutFrameType::type_,
#define ABSTRACT_FRAME_ID(...)
#include "mozilla/FrameIdList.h"
#undef FRAME_ID
#undef ABSTRACT_FRAME_ID
};

const nsIFrame::FrameClassBits nsIFrame::sFrameClassBits[
#define FRAME_ID(...) 1 +
#define ABSTRACT_FRAME_ID(...)
#include "mozilla/FrameIdList.h"
#undef FRAME_ID
#undef ABSTRACT_FRAME_ID
    0] = {
#define Leaf eFrameClassBitsLeaf
#define NotLeaf eFrameClassBitsNone
#define DynamicLeaf eFrameClassBitsDynamicLeaf
#define FRAME_ID(class_, type_, leaf_, ...) leaf_,
#define ABSTRACT_FRAME_ID(...)
#include "mozilla/FrameIdList.h"
#undef Leaf
#undef NotLeaf
#undef DynamicLeaf
#undef FRAME_ID
#undef ABSTRACT_FRAME_ID
};

// Struct containing cached metrics for box-wrapped frames.
struct nsBoxLayoutMetrics {
  nsSize mPrefSize;
  nsSize mMinSize;
  nsSize mMaxSize;

  nsSize mBlockMinSize;
  nsSize mBlockPrefSize;
  nscoord mBlockAscent;

  nscoord mFlex;
  nscoord mAscent;

  nsSize mLastSize;
};

struct nsContentAndOffset {
  nsIContent* mContent = nullptr;
  int32_t mOffset = 0;
};

// Some Misc #defines
#define SELECTION_DEBUG 0
#define FORCE_SELECTION_UPDATE 1
#define CALC_DEBUG 0

// This is faster than nsBidiPresUtils::IsFrameInParagraphDirection,
// because it uses the frame pointer passed in without drilling down to
// the leaf frame.
static bool IsReversedDirectionFrame(nsIFrame* aFrame) {
  FrameBidiData bidiData = aFrame->GetBidiData();
  return !IS_SAME_DIRECTION(bidiData.embeddingLevel, bidiData.baseLevel);
}

#include "nsILineIterator.h"
#include "prenv.h"

NS_DECLARE_FRAME_PROPERTY_DELETABLE(BoxMetricsProperty, nsBoxLayoutMetrics)

static void InitBoxMetrics(nsIFrame* aFrame, bool aClear) {
  if (aClear) {
    aFrame->RemoveProperty(BoxMetricsProperty());
  }

  nsBoxLayoutMetrics* metrics = new nsBoxLayoutMetrics();
  aFrame->SetProperty(BoxMetricsProperty(), metrics);

  static_cast<nsFrame*>(aFrame)->nsFrame::MarkIntrinsicISizesDirty();
  metrics->mBlockAscent = 0;
  metrics->mLastSize.SizeTo(0, 0);
}

// Utility function to set a nsRect-valued property table entry on aFrame,
// reusing the existing storage if the property happens to be already set.
template <typename T>
static void SetOrUpdateRectValuedProperty(
    nsIFrame* aFrame, FrameProperties::Descriptor<T> aProperty,
    const nsRect& aNewValue) {
  bool found;
  nsRect* rectStorage = aFrame->GetProperty(aProperty, &found);
  if (!found) {
    rectStorage = new nsRect(aNewValue);
    aFrame->AddProperty(aProperty, rectStorage);
  } else {
    *rectStorage = aNewValue;
  }
}

static bool IsXULBoxWrapped(const nsIFrame* aFrame) {
  return aFrame->GetParent() && aFrame->GetParent()->IsXULBoxFrame() &&
         !aFrame->IsXULBoxFrame();
}

void nsReflowStatus::UpdateTruncated(const ReflowInput& aReflowInput,
                                     const ReflowOutput& aMetrics) {
  const WritingMode containerWM = aMetrics.GetWritingMode();
  if (aReflowInput.GetWritingMode().IsOrthogonalTo(containerWM)) {
    // Orthogonal flows are always reflowed with an unconstrained dimension,
    // so should never end up truncated (see ReflowInput::Init()).
    mTruncated = false;
  } else if (aReflowInput.AvailableBSize() != NS_UNCONSTRAINEDSIZE &&
             aReflowInput.AvailableBSize() < aMetrics.BSize(containerWM) &&
             !aReflowInput.mFlags.mIsTopOfPage) {
    mTruncated = true;
  } else {
    mTruncated = false;
  }
}

/* static */
void nsIFrame::DestroyAnonymousContent(
    nsPresContext* aPresContext, already_AddRefed<nsIContent>&& aContent) {
  if (nsCOMPtr<nsIContent> content = aContent) {
    aPresContext->EventStateManager()->NativeAnonymousContentRemoved(content);
    aPresContext->PresShell()->NativeAnonymousContentRemoved(content);
    content->UnbindFromTree();
  }
}

// Formerly the nsIFrameDebug interface

std::ostream& operator<<(std::ostream& aStream, const nsReflowStatus& aStatus) {
  char complete = 'Y';
  if (aStatus.IsIncomplete()) {
    complete = 'N';
  } else if (aStatus.IsOverflowIncomplete()) {
    complete = 'O';
  }

  char brk = 'N';
  if (aStatus.IsInlineBreakBefore()) {
    brk = 'B';
  } else if (aStatus.IsInlineBreakAfter()) {
    brk = 'A';
  }

  aStream << "["
          << "Complete=" << complete << ","
          << "NIF=" << (aStatus.NextInFlowNeedsReflow() ? 'Y' : 'N') << ","
          << "Truncated=" << (aStatus.IsTruncated() ? 'Y' : 'N') << ","
          << "Break=" << brk << ","
          << "FirstLetter=" << (aStatus.FirstLetterComplete() ? 'Y' : 'N')
          << "]";
  return aStream;
}

#ifdef DEBUG
static bool gShowFrameBorders = false;

void nsFrame::ShowFrameBorders(bool aEnable) { gShowFrameBorders = aEnable; }

bool nsFrame::GetShowFrameBorders() { return gShowFrameBorders; }

static bool gShowEventTargetFrameBorder = false;

void nsFrame::ShowEventTargetFrameBorder(bool aEnable) {
  gShowEventTargetFrameBorder = aEnable;
}

bool nsFrame::GetShowEventTargetFrameBorder() {
  return gShowEventTargetFrameBorder;
}

/**
 * Note: the log module is created during library initialization which
 * means that you cannot perform logging before then.
 */
mozilla::LazyLogModule nsFrame::sFrameLogModule("frame");

#endif

NS_DECLARE_FRAME_PROPERTY_DELETABLE(AbsoluteContainingBlockProperty,
                                    nsAbsoluteContainingBlock)

bool nsIFrame::HasAbsolutelyPositionedChildren() const {
  return IsAbsoluteContainer() &&
         GetAbsoluteContainingBlock()->HasAbsoluteFrames();
}

nsAbsoluteContainingBlock* nsIFrame::GetAbsoluteContainingBlock() const {
  NS_ASSERTION(IsAbsoluteContainer(),
               "The frame is not marked as an abspos container correctly");
  nsAbsoluteContainingBlock* absCB =
      GetProperty(AbsoluteContainingBlockProperty());
  NS_ASSERTION(absCB,
               "The frame is marked as an abspos container but doesn't have "
               "the property");
  return absCB;
}

void nsIFrame::MarkAsAbsoluteContainingBlock() {
  MOZ_ASSERT(GetStateBits() & NS_FRAME_CAN_HAVE_ABSPOS_CHILDREN);
  NS_ASSERTION(!GetProperty(AbsoluteContainingBlockProperty()),
               "Already has an abs-pos containing block property?");
  NS_ASSERTION(!HasAnyStateBits(NS_FRAME_HAS_ABSPOS_CHILDREN),
               "Already has NS_FRAME_HAS_ABSPOS_CHILDREN state bit?");
  AddStateBits(NS_FRAME_HAS_ABSPOS_CHILDREN);
  SetProperty(AbsoluteContainingBlockProperty(),
              new nsAbsoluteContainingBlock(GetAbsoluteListID()));
}

void nsIFrame::MarkAsNotAbsoluteContainingBlock() {
  NS_ASSERTION(!HasAbsolutelyPositionedChildren(), "Think of the children!");
  NS_ASSERTION(GetProperty(AbsoluteContainingBlockProperty()),
               "Should have an abs-pos containing block property");
  NS_ASSERTION(HasAnyStateBits(NS_FRAME_HAS_ABSPOS_CHILDREN),
               "Should have NS_FRAME_HAS_ABSPOS_CHILDREN state bit");
  MOZ_ASSERT(HasAnyStateBits(NS_FRAME_CAN_HAVE_ABSPOS_CHILDREN));
  RemoveStateBits(NS_FRAME_HAS_ABSPOS_CHILDREN);
  RemoveProperty(AbsoluteContainingBlockProperty());
}

bool nsIFrame::CheckAndClearPaintedState() {
  bool result = (GetStateBits() & NS_FRAME_PAINTED_THEBES);
  RemoveStateBits(NS_FRAME_PAINTED_THEBES);

  nsIFrame::ChildListIterator lists(this);
  for (; !lists.IsDone(); lists.Next()) {
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      nsIFrame* child = childFrames.get();
      if (child->CheckAndClearPaintedState()) {
        result = true;
      }
    }
  }
  return result;
}

bool nsIFrame::CheckAndClearDisplayListState() {
  bool result = BuiltDisplayList();
  SetBuiltDisplayList(false);

  nsIFrame::ChildListIterator lists(this);
  for (; !lists.IsDone(); lists.Next()) {
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      nsIFrame* child = childFrames.get();
      if (child->CheckAndClearDisplayListState()) {
        result = true;
      }
    }
  }
  return result;
}

bool nsIFrame::IsVisibleConsideringAncestors(uint32_t aFlags) const {
  if (!StyleVisibility()->IsVisible()) {
    return false;
  }

  if (PresShell()->IsUnderHiddenEmbedderElement()) {
    return false;
  }

  const nsIFrame* frame = this;
  while (frame) {
    nsView* view = frame->GetView();
    if (view && view->GetVisibility() == nsViewVisibility_kHide) return false;

    nsIFrame* parent = frame->GetParent();
    nsDeckFrame* deck = do_QueryFrame(parent);
    if (deck) {
      if (deck->GetSelectedBox() != frame) return false;
    }

    if (parent) {
      frame = parent;
    } else {
      parent = nsLayoutUtils::GetCrossDocParentFrame(frame);
      if (!parent) break;

      if ((aFlags & nsIFrame::VISIBILITY_CROSS_CHROME_CONTENT_BOUNDARY) == 0 &&
          parent->PresContext()->IsChrome() &&
          !frame->PresContext()->IsChrome()) {
        break;
      }

      frame = parent;
    }
  }

  return true;
}

void nsIFrame::FindCloserFrameForSelection(
    const nsPoint& aPoint, FrameWithDistance* aCurrentBestFrame) {
  if (nsLayoutUtils::PointIsCloserToRect(aPoint, mRect,
                                         aCurrentBestFrame->mXDistance,
                                         aCurrentBestFrame->mYDistance)) {
    aCurrentBestFrame->mFrame = this;
  }
}

void nsIFrame::ContentStatesChanged(mozilla::EventStates aStates) {}

AutoWeakFrame::AutoWeakFrame(const WeakFrame& aOther)
    : mPrev(nullptr), mFrame(nullptr) {
  Init(aOther.GetFrame());
}

void AutoWeakFrame::Init(nsIFrame* aFrame) {
  Clear(mFrame ? mFrame->PresContext()->GetPresShell() : nullptr);
  mFrame = aFrame;
  if (mFrame) {
    mozilla::PresShell* presShell = mFrame->PresContext()->GetPresShell();
    NS_WARNING_ASSERTION(presShell, "Null PresShell in AutoWeakFrame!");
    if (presShell) {
      presShell->AddAutoWeakFrame(this);
    } else {
      mFrame = nullptr;
    }
  }
}

void WeakFrame::Init(nsIFrame* aFrame) {
  Clear(mFrame ? mFrame->PresContext()->GetPresShell() : nullptr);
  mFrame = aFrame;
  if (mFrame) {
    mozilla::PresShell* presShell = mFrame->PresContext()->GetPresShell();
    MOZ_ASSERT(presShell, "Null PresShell in WeakFrame!");
    if (presShell) {
      presShell->AddWeakFrame(this);
    } else {
      mFrame = nullptr;
    }
  }
}

nsIFrame* NS_NewEmptyFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell) nsFrame(aStyle, aPresShell->GetPresContext());
}

nsFrame::nsFrame(ComputedStyle* aStyle, nsPresContext* aPresContext,
                 ClassID aID)
    : nsBox(aStyle, aPresContext, aID) {
  MOZ_COUNT_CTOR(nsFrame);
}

nsFrame::~nsFrame() {
  MOZ_COUNT_DTOR(nsFrame);

  MOZ_ASSERT(GetVisibility() != Visibility::ApproximatelyVisible,
             "Visible nsFrame is being destroyed");
}

NS_IMPL_FRAMEARENA_HELPERS(nsFrame)

// Dummy operator delete.  Will never be called, but must be defined
// to satisfy some C++ ABIs.
void nsFrame::operator delete(void*, size_t) {
  MOZ_CRASH("nsFrame::operator delete should never be called");
}

NS_QUERYFRAME_HEAD(nsFrame)
  NS_QUERYFRAME_ENTRY(nsIFrame)
NS_QUERYFRAME_TAIL_INHERITANCE_ROOT

/////////////////////////////////////////////////////////////////////////////
// nsIFrame

static bool IsFontSizeInflationContainer(nsIFrame* aFrame,
                                         const nsStyleDisplay* aStyleDisplay) {
  /*
   * Font size inflation is built around the idea that we're inflating
   * the fonts for a pan-and-zoom UI so that when the user scales up a
   * block or other container to fill the width of the device, the fonts
   * will be readable.  To do this, we need to pick what counts as a
   * container.
   *
   * From a code perspective, the only hard requirement is that frames
   * that are line participants
   * (nsIFrame::IsFrameOfType(nsIFrame::eLineParticipant)) are never
   * containers, since line layout assumes that the inflation is
   * consistent within a line.
   *
   * This is not an imposition, since we obviously want a bunch of text
   * (possibly with inline elements) flowing within a block to count the
   * block (or higher) as its container.
   *
   * We also want form controls, including the text in the anonymous
   * content inside of them, to match each other and the text next to
   * them, so they and their anonymous content should also not be a
   * container.
   *
   * However, because we can't reliably compute sizes across XUL during
   * reflow, any XUL frame with a XUL parent is always a container.
   *
   * There are contexts where it would be nice if some blocks didn't
   * count as a container, so that, for example, an indented quotation
   * didn't end up with a smaller font size.  However, it's hard to
   * distinguish these situations where we really do want the indented
   * thing to count as a container, so we don't try, and blocks are
   * always containers.
   */

  // The root frame should always be an inflation container.
  if (!aFrame->GetParent()) {
    return true;
  }

  nsIContent* content = aFrame->GetContent();
  if (content && content->IsInNativeAnonymousSubtree()) {
    // Native anonymous content shouldn't be a font inflation root,
    // except for the canvas custom content container.
    nsCanvasFrame* canvas = aFrame->PresShell()->GetCanvasFrame();
    return canvas && canvas->GetCustomContentContainer() == content;
  }

  LayoutFrameType frameType = aFrame->Type();
  bool isInline =
      (nsStyleDisplay::IsInlineFlow(aFrame->GetDisplay()) ||
       RubyUtils::IsRubyBox(frameType) ||
       (aFrame->IsFloating() && frameType == LayoutFrameType::Letter) ||
       // Given multiple frames for the same node, only the
       // outer one should be considered a container.
       // (Important, e.g., for nsSelectsAreaFrame.)
       (aFrame->GetParent()->GetContent() == content) ||
       (content &&
        // Form controls shouldn't become inflation containers.
        (content->IsAnyOfHTMLElements(nsGkAtoms::option, nsGkAtoms::optgroup,
                                      nsGkAtoms::select, nsGkAtoms::input,
                                      nsGkAtoms::button)))) &&
      !(aFrame->IsXULBoxFrame() && aFrame->GetParent()->IsXULBoxFrame());
  NS_ASSERTION(!aFrame->IsFrameOfType(nsIFrame::eLineParticipant) || isInline ||
                   // br frames and mathml frames report being line
                   // participants even when their position or display is
                   // set
                   aFrame->IsBrFrame() ||
                   aFrame->IsFrameOfType(nsIFrame::eMathML),
               "line participants must not be containers");
  NS_ASSERTION(!aFrame->IsBulletFrame() || isInline,
               "bullets should not be containers");
  return !isInline;
}

static void MaybeScheduleReflowSVGNonDisplayText(nsFrame* aFrame) {
  if (!nsSVGUtils::IsInSVGTextSubtree(aFrame)) {
    return;
  }

  // We need to ensure that any non-display SVGTextFrames get reflowed when a
  // child text frame gets new style. Thus we need to schedule a reflow in
  // |DidSetComputedStyle|. We also need to call it from |DestroyFrom|,
  // because otherwise we won't get notified when style changes to
  // "display:none".
  SVGTextFrame* svgTextFrame = static_cast<SVGTextFrame*>(
      nsLayoutUtils::GetClosestFrameOfType(aFrame, LayoutFrameType::SVGText));
  nsIFrame* anonBlock = svgTextFrame->PrincipalChildList().FirstChild();

  // Note that we must check NS_FRAME_FIRST_REFLOW on our SVGTextFrame's
  // anonymous block frame rather than our aFrame, since NS_FRAME_FIRST_REFLOW
  // may be set on us if we're a new frame that has been inserted after the
  // document's first reflow. (In which case this DidSetComputedStyle call may
  // be happening under frame construction under a Reflow() call.)
  if (!anonBlock || anonBlock->HasAnyStateBits(NS_FRAME_FIRST_REFLOW)) {
    return;
  }

  if (!svgTextFrame->HasAnyStateBits(NS_FRAME_IS_NONDISPLAY) ||
      svgTextFrame->HasAnyStateBits(NS_STATE_SVG_TEXT_IN_REFLOW)) {
    return;
  }

  svgTextFrame->ScheduleReflowSVGNonDisplayText(IntrinsicDirty::StyleChange);
}

bool nsIFrame::IsPrimaryFrameOfRootOrBodyElement() const {
  if (!IsPrimaryFrame()) {
    return false;
  }
  nsIContent* content = GetContent();
  Document* document = content->OwnerDoc();
  return content == document->GetRootElement() ||
         content == document->GetBodyElement();
}

void nsFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                   nsIFrame* aPrevInFlow) {
  MOZ_ASSERT(nsQueryFrame::FrameIID(mClass) == GetFrameId());
  MOZ_ASSERT(!mContent, "Double-initing a frame?");
  NS_ASSERTION(IsFrameOfType(eDEBUGAllFrames) && !IsFrameOfType(eDEBUGNoFrames),
               "IsFrameOfType implementation that doesn't call base class");

  mContent = aContent;
  mParent = aParent;
  MOZ_DIAGNOSTIC_ASSERT(!mParent || PresShell() == mParent->PresShell());

  if (aPrevInFlow) {
    mWritingMode = aPrevInFlow->GetWritingMode();

    // Copy some state bits from prev-in-flow (the bits that should apply
    // throughout a continuation chain). The bits are sorted according to their
    // order in nsFrameStateBits.h.

    // clang-format off
    AddStateBits(aPrevInFlow->GetStateBits() &
                 (NS_FRAME_ANONYMOUSCONTENTCREATOR_CONTENT |
                  NS_FRAME_GENERATED_CONTENT |
                  NS_FRAME_OUT_OF_FLOW |
                  NS_FRAME_CAN_HAVE_ABSPOS_CHILDREN |
                  NS_FRAME_INDEPENDENT_SELECTION |
                  NS_FRAME_PART_OF_IBSPLIT |
                  NS_FRAME_MAY_BE_TRANSFORMED |
                  NS_FRAME_HAS_MULTI_COLUMN_ANCESTOR));
    // clang-format on

    // Copy other bits in nsIFrame from prev-in-flow.
    mHasColumnSpanSiblings = aPrevInFlow->HasColumnSpanSiblings();
  } else {
    PresContext()->ConstructedFrame();
  }

  if (GetParent()) {
    if (MOZ_UNLIKELY(mContent == PresContext()->Document()->GetRootElement() &&
                     mContent == GetParent()->GetContent())) {
      // Our content is the root element and we have the same content as our
      // parent. That is, we are the internal anonymous frame of the root
      // element. Copy the used mWritingMode from our parent because
      // mDocElementContainingBlock gets its mWritingMode from <body>.
      mWritingMode = GetParent()->GetWritingMode();
    }

    // Copy some state bits from our parent (the bits that should apply
    // recursively throughout a subtree). The bits are sorted according to their
    // order in nsFrameStateBits.h.

    // clang-format off
    AddStateBits(GetParent()->GetStateBits() &
                 (NS_FRAME_GENERATED_CONTENT |
                  NS_FRAME_INDEPENDENT_SELECTION |
                  NS_FRAME_IS_SVG_TEXT |
                  NS_FRAME_IN_POPUP |
                  NS_FRAME_IS_NONDISPLAY));
    // clang-format on

    if (HasAnyStateBits(NS_FRAME_IN_POPUP) && TrackingVisibility()) {
      // Assume all frames in popups are visible.
      IncApproximateVisibleCount();
    }
  }
  if (aPrevInFlow) {
    mMayHaveOpacityAnimation = aPrevInFlow->MayHaveOpacityAnimation();
    mMayHaveTransformAnimation = aPrevInFlow->MayHaveTransformAnimation();
  } else if (mContent) {
    // It's fine to fetch the EffectSet for the style frame here because in the
    // following code we take care of the case where animations may target
    // a different frame.
    EffectSet* effectSet = EffectSet::GetEffectSetForStyleFrame(this);
    if (effectSet) {
      mMayHaveOpacityAnimation = effectSet->MayHaveOpacityAnimation();

      if (effectSet->MayHaveTransformAnimation()) {
        // If we are the inner table frame for display:table content, then
        // transform animations should go on our parent frame (the table wrapper
        // frame).
        //
        // We do this when initializing the child frame (table inner frame),
        // because when initializng the table wrapper frame, we don't yet have
        // access to its children so we can't tell if we have transform
        // animations or not.
        if (IsFrameOfType(eSupportsCSSTransforms)) {
          mMayHaveTransformAnimation = true;
          AddStateBits(NS_FRAME_MAY_BE_TRANSFORMED);
        } else if (aParent && nsLayoutUtils::GetStyleFrame(aParent) == this) {
          MOZ_ASSERT(
              aParent->IsFrameOfType(eSupportsCSSTransforms),
              "Style frames that don't support transforms should have parents"
              " that do");
          aParent->mMayHaveTransformAnimation = true;
          aParent->AddStateBits(NS_FRAME_MAY_BE_TRANSFORMED);
        }
      }
    }
  }

  const nsStyleDisplay* disp = StyleDisplay();
  if (disp->HasTransform(this)) {
    // If 'transform' dynamically changes, RestyleManager takes care of
    // updating this bit.
    AddStateBits(NS_FRAME_MAY_BE_TRANSFORMED);
  }

  if (disp->IsContainLayout() && disp->IsContainSize() &&
      // All frames that support contain:layout also support contain:size.
      IsFrameOfType(eSupportsContainLayoutAndPaint) && !IsTableWrapperFrame()) {
    // In general, frames that have contain:layout+size can be reflow roots.
    // (One exception: table-wrapper frames don't work well as reflow roots,
    // because their inner-table ReflowInput init path tries to reuse & deref
    // the wrapper's containing block reflow input, which may be null if we
    // initiate reflow from the table-wrapper itself.)
    //
    // Changes to `contain` force frame reconstructions, so this bit can be set
    // for the whole lifetime of this frame.
    AddStateBits(NS_FRAME_REFLOW_ROOT);
  }

  if (nsLayoutUtils::FontSizeInflationEnabled(PresContext()) ||
      !GetParent()
#ifdef DEBUG
      // We have assertions that check inflation invariants even when
      // font size inflation is not enabled.
      || true
#endif
  ) {
    if (IsFontSizeInflationContainer(this, disp)) {
      AddStateBits(NS_FRAME_FONT_INFLATION_CONTAINER);
      if (!GetParent() ||
          // I'd use NS_FRAME_OUT_OF_FLOW, but it's not set yet.
          disp->IsFloating(this) || disp->IsAbsolutelyPositioned(this) ||
          GetParent()->IsFlexContainerFrame() ||
          GetParent()->IsGridContainerFrame()) {
        AddStateBits(NS_FRAME_FONT_INFLATION_FLOW_ROOT);
      }
    }
    NS_ASSERTION(
        GetParent() || (GetStateBits() & NS_FRAME_FONT_INFLATION_CONTAINER),
        "root frame should always be a container");
  }

  if (PresShell()->AssumeAllFramesVisible() && TrackingVisibility()) {
    IncApproximateVisibleCount();
  }

  DidSetComputedStyle(nullptr);

  if (::IsXULBoxWrapped(this)) ::InitBoxMetrics(this, false);

  // For a newly created frame, we need to update this frame's visibility state.
  // Usually we update the state when the frame is restyled and has a
  // VisibilityChange change hint but we don't generate any change hints for
  // newly created frames.
  // Note: We don't need to do this for placeholders since placeholders have
  // different styles so that the styles don't have visibility:hidden even if
  // the parent has visibility:hidden style. We also don't need to update the
  // state when creating continuations because its visibility is the same as its
  // prev-in-flow, and the animation code cares only primary frames.
  if (!IsPlaceholderFrame() && !aPrevInFlow) {
    UpdateVisibleDescendantsState();
  }
}

void nsFrame::DestroyFrom(nsIFrame* aDestructRoot,
                          PostDestroyData& aPostDestroyData) {
  NS_ASSERTION(!nsContentUtils::IsSafeToRunScript(),
               "destroy called on frame while scripts not blocked");
  NS_ASSERTION(!GetNextSibling() && !GetPrevSibling(),
               "Frames should be removed before destruction.");
  NS_ASSERTION(aDestructRoot, "Must specify destruct root");
  MOZ_ASSERT(!HasAbsolutelyPositionedChildren());
  MOZ_ASSERT(!HasAnyStateBits(NS_FRAME_PART_OF_IBSPLIT),
             "NS_FRAME_PART_OF_IBSPLIT set on non-nsContainerFrame?");

  MaybeScheduleReflowSVGNonDisplayText(this);

  SVGObserverUtils::InvalidateDirectRenderingObservers(this);

  if (StyleDisplay()->mPosition == StylePositionProperty::Sticky) {
    StickyScrollContainer* ssc =
        StickyScrollContainer::GetStickyScrollContainerForFrame(this);
    if (ssc) {
      ssc->RemoveFrame(this);
    }
  }

  nsPresContext* presContext = PresContext();
  mozilla::PresShell* presShell = presContext->GetPresShell();
  if (mState & NS_FRAME_OUT_OF_FLOW) {
    nsPlaceholderFrame* placeholder = GetPlaceholderFrame();
    NS_ASSERTION(
        !placeholder || (aDestructRoot != this),
        "Don't call Destroy() on OOFs, call Destroy() on the placeholder.");
    NS_ASSERTION(!placeholder || nsLayoutUtils::IsProperAncestorFrame(
                                     aDestructRoot, placeholder),
                 "Placeholder relationship should have been torn down already; "
                 "this might mean we have a stray placeholder in the tree.");
    if (placeholder) {
      placeholder->SetOutOfFlowFrame(nullptr);
    }
  }

  if (IsPrimaryFrame()) {
    // This needs to happen before we clear our Properties() table.
    ActiveLayerTracker::TransferActivityToContent(this, mContent);
  }

  ScrollAnchorContainer* anchor = nullptr;
  if (IsScrollAnchor(&anchor)) {
    anchor->InvalidateAnchor();
  }

  if (HasCSSAnimations() || HasCSSTransitions() ||
      // It's fine to look up the style frame here since if we're destroying the
      // frames for display:table content we should be destroying both wrapper
      // and inner frame.
      EffectSet::GetEffectSetForStyleFrame(this)) {
    // If no new frame for this element is created by the end of the
    // restyling process, stop animations and transitions for this frame
    RestyleManager::AnimationsWithDestroyedFrame* adf =
        presContext->RestyleManager()->GetAnimationsWithDestroyedFrame();
    // AnimationsWithDestroyedFrame only lives during the restyling process.
    if (adf) {
      adf->Put(mContent, mComputedStyle);
    }
  }

  // Disable visibility tracking. Note that we have to do this before we clear
  // frame properties and lose track of whether we were previously visible.
  // XXX(seth): It'd be ideal to assert that we're already marked nonvisible
  // here, but it's unfortunately tricky to guarantee in the face of things like
  // frame reconstruction induced by style changes.
  DisableVisibilityTracking();

  // Ensure that we're not in the approximately visible list anymore.
  PresContext()->GetPresShell()->RemoveFrameFromApproximatelyVisibleList(this);

  presShell->NotifyDestroyingFrame(this);

  if (mState & NS_FRAME_EXTERNAL_REFERENCE) {
    presShell->ClearFrameRefs(this);
  }

  nsView* view = GetView();
  if (view) {
    view->SetFrame(nullptr);
    view->Destroy();
  }

  // Make sure that our deleted frame can't be returned from GetPrimaryFrame()
  if (IsPrimaryFrame()) {
    mContent->SetPrimaryFrame(nullptr);

    // Pass the root of a generated content subtree (e.g. ::after/::before) to
    // aPostDestroyData to unbind it after frame destruction is done.
    if (HasAnyStateBits(NS_FRAME_GENERATED_CONTENT) &&
        mContent->IsRootOfNativeAnonymousSubtree()) {
      aPostDestroyData.AddAnonymousContent(mContent.forget());
    }
  }

  // Remove all properties attached to the frame, to ensure any property
  // destructors that need the frame pointer are handled properly.
  RemoveAllProperties();

  // Must retrieve the object ID before calling destructors, so the
  // vtable is still valid.
  //
  // Note to future tweakers: having the method that returns the
  // object size call the destructor will not avoid an indirect call;
  // the compiler cannot devirtualize the call to the destructor even
  // if it's from a method defined in the same class.

  nsQueryFrame::FrameIID id = GetFrameId();
  this->~nsFrame();

#ifdef DEBUG
  {
    nsIFrame* rootFrame = presShell->GetRootFrame();
    MOZ_ASSERT(rootFrame);
    if (this != rootFrame) {
      const RetainedDisplayListData* data =
          GetRetainedDisplayListData(rootFrame);

      const bool inModifiedList =
          data && (data->GetFlags(this) &
                   RetainedDisplayListData::FrameFlags::Modified);

      MOZ_ASSERT(!inModifiedList,
                 "A dtor added this frame to modified frames list!");
    }
  }
#endif

  // Now that we're totally cleaned out, we need to add ourselves to
  // the presshell's recycler.
  presShell->FreeFrame(id, this);
}

nsresult nsFrame::GetOffsets(int32_t& aStart, int32_t& aEnd) const {
  aStart = 0;
  aEnd = 0;
  return NS_OK;
}

static void CompareLayers(
    const nsStyleImageLayers* aFirstLayers,
    const nsStyleImageLayers* aSecondLayers,
    const std::function<void(imgRequestProxy* aReq)>& aCallback) {
  NS_FOR_VISIBLE_IMAGE_LAYERS_BACK_TO_FRONT(i, (*aFirstLayers)) {
    const auto& image = aFirstLayers->mLayers[i].mImage;
    if (!image.IsImageRequestType() || !image.IsResolved()) {
      continue;
    }

    // aCallback is called when the style image in aFirstLayers is thought to
    // be different with the corresponded one in aSecondLayers
    if (!aSecondLayers || i >= aSecondLayers->mImageCount ||
        (!aSecondLayers->mLayers[i].mImage.IsResolved() ||
         image.GetImageRequest() !=
             aSecondLayers->mLayers[i].mImage.GetImageRequest())) {
      if (imgRequestProxy* req = image.GetImageRequest()) {
        aCallback(req);
      }
    }
  }
}

static void AddAndRemoveImageAssociations(
    ImageLoader& aImageLoader, nsFrame* aFrame,
    const nsStyleImageLayers* aOldLayers,
    const nsStyleImageLayers* aNewLayers) {
  // If the old context had a background-image image, or mask-image image,
  // and new context does not have the same image, clear the image load
  // notifier (which keeps the image loading, if it still is) for the frame.
  // We want to do this conservatively because some frames paint their
  // backgrounds from some other frame's style data, and we don't want
  // to clear those notifiers unless we have to.  (They'll be reset
  // when we paint, although we could miss a notification in that
  // interval.)
  if (aOldLayers && aFrame->HasImageRequest()) {
    CompareLayers(aOldLayers, aNewLayers, [&](imgRequestProxy* aReq) {
      aImageLoader.DisassociateRequestFromFrame(aReq, aFrame);
    });
  }

  CompareLayers(aNewLayers, aOldLayers, [&](imgRequestProxy* aReq) {
    aImageLoader.AssociateRequestToFrame(aReq, aFrame, 0);
  });
}

void nsIFrame::AddDisplayItem(nsDisplayItemBase* aItem) {
  DisplayItemArray* items = GetProperty(DisplayItems());
  if (!items) {
    items = new DisplayItemArray();
    AddProperty(DisplayItems(), items);
  }
  MOZ_DIAGNOSTIC_ASSERT(!items->Contains(aItem));
  items->AppendElement(aItem);
}

bool nsIFrame::RemoveDisplayItem(nsDisplayItemBase* aItem) {
  DisplayItemArray* items = GetProperty(DisplayItems());
  if (!items) {
    return false;
  }
  bool result = items->RemoveElement(aItem);
  if (items->IsEmpty()) {
    RemoveProperty(DisplayItems());
  }
  return result;
}

bool nsIFrame::HasDisplayItems() {
  DisplayItemArray* items = GetProperty(DisplayItems());
  return items != nullptr;
}

bool nsIFrame::HasDisplayItem(nsDisplayItemBase* aItem) {
  DisplayItemArray* items = GetProperty(DisplayItems());
  if (!items) {
    return false;
  }
  return items->Contains(aItem);
}

bool nsIFrame::HasDisplayItem(uint32_t aKey) {
  DisplayItemArray* items = GetProperty(DisplayItems());
  if (!items) {
    return false;
  }

  for (nsDisplayItemBase* i : *items) {
    if (i->GetPerFrameKey() == aKey) {
      return true;
    }
  }
  return false;
}

template <typename Condition>
static void DiscardDisplayItems(nsIFrame* aFrame, Condition aCondition) {
  auto* items = aFrame->GetProperty(nsIFrame::DisplayItems());
  if (!items) {
    return;
  }

  for (nsDisplayItemBase* i : *items) {
    // Only discard items that are invalidated by this frame, as we're only
    // guaranteed to rebuild those items. Table background items are created by
    // the relevant table part, but have the cell frame as the primary frame,
    // and we don't want to remove them if this is the cell.
    if (aCondition(i) && i->FrameForInvalidation() == aFrame) {
      i->SetCantBeReused();
    }
  }
}

static void DiscardOldItems(nsIFrame* aFrame) {
  DiscardDisplayItems(
      aFrame, [](nsDisplayItemBase* aItem) { return aItem->IsOldItem(); });
}

void nsIFrame::RemoveDisplayItemDataForDeletion() {
  // Destroying a WebRenderUserDataTable can cause destruction of other objects
  // which can remove frame properties in their destructor. If we delete a frame
  // property it runs the destructor of the stored object in the middle of
  // updating the frame property table, so if the destruction of that object
  // causes another update to the frame property table it would leave the frame
  // property table in an inconsistent state. So we remove it from the table and
  // then destroy it. (bug 1530657)
  WebRenderUserDataTable* userDataTable =
      TakeProperty(WebRenderUserDataProperty::Key());
  if (userDataTable) {
    for (auto iter = userDataTable->Iter(); !iter.Done(); iter.Next()) {
      iter.UserData()->RemoveFromTable();
    }
    delete userDataTable;
  }

  FrameLayerBuilder::RemoveFrameFromLayerManager(this, DisplayItemData());
  DisplayItemData().Clear();

  DisplayItemArray* items = TakeProperty(DisplayItems());
  if (items) {
    for (nsDisplayItemBase* i : *items) {
      if (i->GetDependentFrame() == this && !i->HasDeletedFrame()) {
        i->Frame()->MarkNeedsDisplayItemRebuild();
      }
      i->RemoveFrame(this);
    }
    delete items;
  }

  if (!nsLayoutUtils::AreRetainedDisplayListsEnabled()) {
    // Retained display lists are disabled, no need to update
    // RetainedDisplayListData.
    return;
  }

  const bool updateData = IsFrameModified() || HasOverrideDirtyRegion() ||
                          MayHaveWillChangeBudget();

  if (!updateData) {
    // No RetainedDisplayListData to update.
    return;
  }

  nsIFrame* rootFrame = PresShell()->GetRootFrame();
  MOZ_ASSERT(rootFrame);

  RetainedDisplayListData* data = GetOrSetRetainedDisplayListData(rootFrame);

  if (MayHaveWillChangeBudget()) {
    // Keep the frame in list, so it can be removed from the will-change budget.
    data->Flags(this) = RetainedDisplayListData::FrameFlags::HadWillChange;
    return;
  }

  if (IsFrameModified() || HasOverrideDirtyRegion()) {
    // Remove deleted frames from RetainedDisplayListData.
    DebugOnly<bool> removed = data->Remove(this);
    MOZ_ASSERT(removed,
               "Frame had flags set, but it was not found in DisplayListData!");
  }
}

void nsIFrame::MarkNeedsDisplayItemRebuild() {
  if (!nsLayoutUtils::AreRetainedDisplayListsEnabled() || IsFrameModified() ||
      HasAnyStateBits(NS_FRAME_IN_POPUP)) {
    // Skip frames that are already marked modified.
    return;
  }

  if (Type() == LayoutFrameType::Placeholder) {
    nsIFrame* oof = static_cast<nsPlaceholderFrame*>(this)->GetOutOfFlowFrame();
    if (oof) {
      oof->MarkNeedsDisplayItemRebuild();
    }
    // Do not mark placeholder frames modified.
    return;
  }

  if (!nsLayoutUtils::DisplayRootHasRetainedDisplayListBuilder(this)) {
    return;
  }

  nsIFrame* rootFrame = PresShell()->GetRootFrame();
  MOZ_ASSERT(rootFrame);

  if (rootFrame->IsFrameModified()) {
    return;
  }

  RetainedDisplayListData* data = GetOrSetRetainedDisplayListData(rootFrame);

  if (data->ModifiedFramesCount() >
      StaticPrefs::layout_display_list_rebuild_frame_limit()) {
    // If the modified frames count is above the rebuild limit, mark the root
    // frame modified, and stop marking additional frames modified.
    data->AddModifiedFrame(rootFrame);
    rootFrame->SetFrameIsModified(true);
    return;
  }

  data->AddModifiedFrame(this);
  SetFrameIsModified(true);

  MOZ_ASSERT(
      PresContext()->LayoutPhaseCount(nsLayoutPhase::DisplayListBuilding) == 0);

  // Hopefully this is cheap, but we could use a frame state bit to note
  // the presence of dependencies to speed it up.
  DisplayItemArray* items = GetProperty(DisplayItems());
  if (items) {
    for (nsDisplayItemBase* i : *items) {
      if (i->HasDeletedFrame() || i->Frame() == this) {
        // Ignore the items with deleted frames, and the items with |this| as
        // the primary frame.
        continue;
      }

      if (i->GetDependentFrame() == this) {
        // For items with |this| as a dependent frame, mark the primary frame
        // for rebuild.
        i->Frame()->MarkNeedsDisplayItemRebuild();
      }
    }
  }
}

// Subclass hook for style post processing
/* virtual */
void nsFrame::DidSetComputedStyle(ComputedStyle* aOldComputedStyle) {
  MaybeScheduleReflowSVGNonDisplayText(this);

  Document* doc = PresContext()->Document();
  ImageLoader* loader = doc->StyleImageLoader();
  // Continuing text frame doesn't initialize its continuation pointer before
  // reaching here for the first time, so we have to exclude text frames. This
  // doesn't affect correctness because text can't match selectors.
  //
  // FIXME(emilio): We should consider fixing that.
  //
  // TODO(emilio): Can we avoid doing some / all of the image stuff when
  // isNonTextFirstContinuation is false? We should consider doing this just for
  // primary frames and pseudos, but the first-line reparenting code makes it
  // all bad, should get around to bug 1465474 eventually :(
  const bool isNonText = !IsTextFrame();
  if (isNonText) {
    mComputedStyle->StartImageLoads(*doc, aOldComputedStyle);
  }

  const nsStyleImageLayers* oldLayers =
      aOldComputedStyle ? &aOldComputedStyle->StyleBackground()->mImage
                        : nullptr;
  const nsStyleImageLayers* newLayers = &StyleBackground()->mImage;
  AddAndRemoveImageAssociations(*loader, this, oldLayers, newLayers);

  oldLayers =
      aOldComputedStyle ? &aOldComputedStyle->StyleSVGReset()->mMask : nullptr;
  newLayers = &StyleSVGReset()->mMask;
  AddAndRemoveImageAssociations(*loader, this, oldLayers, newLayers);

  const nsStyleDisplay* disp = StyleDisplay();
  bool handleStickyChange = false;
  if (aOldComputedStyle) {
    // Detect style changes that should trigger a scroll anchor adjustment
    // suppression.
    // https://drafts.csswg.org/css-scroll-anchoring/#suppression-triggers
    bool needAnchorSuppression = false;

    // If we detect a change on margin, padding or border, we store the old
    // values on the frame itself between now and reflow, so if someone
    // calls GetUsed(Margin|Border|Padding)() before the next reflow, we
    // can give an accurate answer.
    // We don't want to set the property if one already exists.
    nsMargin oldValue(0, 0, 0, 0);
    nsMargin newValue(0, 0, 0, 0);
    const nsStyleMargin* oldMargin = aOldComputedStyle->StyleMargin();
    if (oldMargin->GetMargin(oldValue)) {
      if (!StyleMargin()->GetMargin(newValue) || oldValue != newValue) {
        if (!HasProperty(UsedMarginProperty())) {
          AddProperty(UsedMarginProperty(), new nsMargin(oldValue));
        }
        needAnchorSuppression = true;
      }
    }

    const nsStylePadding* oldPadding = aOldComputedStyle->StylePadding();
    if (oldPadding->GetPadding(oldValue)) {
      if (!StylePadding()->GetPadding(newValue) || oldValue != newValue) {
        if (!HasProperty(UsedPaddingProperty())) {
          AddProperty(UsedPaddingProperty(), new nsMargin(oldValue));
        }
        needAnchorSuppression = true;
      }
    }

    const nsStyleBorder* oldBorder = aOldComputedStyle->StyleBorder();
    oldValue = oldBorder->GetComputedBorder();
    newValue = StyleBorder()->GetComputedBorder();
    if (oldValue != newValue && !HasProperty(UsedBorderProperty())) {
      AddProperty(UsedBorderProperty(), new nsMargin(oldValue));
    }

    const nsStyleDisplay* oldDisp = aOldComputedStyle->StyleDisplay();
    if (oldDisp->mOverflowAnchor != disp->mOverflowAnchor) {
      if (auto* container = ScrollAnchorContainer::FindFor(this)) {
        container->InvalidateAnchor();
      }
      if (nsIScrollableFrame* scrollableFrame = do_QueryFrame(this)) {
        scrollableFrame->Anchor()->InvalidateAnchor();
      }
    }

    if (mInScrollAnchorChain) {
      const nsStylePosition* pos = StylePosition();
      const nsStylePosition* oldPos = aOldComputedStyle->StylePosition();
      if (!needAnchorSuppression &&
          (oldPos->mOffset != pos->mOffset || oldPos->mWidth != pos->mWidth ||
           oldPos->mMinWidth != pos->mMinWidth ||
           oldPos->mMaxWidth != pos->mMaxWidth ||
           oldPos->mHeight != pos->mHeight ||
           oldPos->mMinHeight != pos->mMinHeight ||
           oldPos->mMaxHeight != pos->mMaxHeight ||
           oldDisp->mPosition != disp->mPosition ||
           oldDisp->mTransform != disp->mTransform)) {
        needAnchorSuppression = true;
      }

      if (needAnchorSuppression &&
          StaticPrefs::layout_css_scroll_anchoring_suppressions_enabled()) {
        ScrollAnchorContainer::FindFor(this)->SuppressAdjustments();
      }
    }

    if (disp->mPosition != oldDisp->mPosition) {
      if (!disp->IsRelativelyPositionedStyle() &&
          oldDisp->IsRelativelyPositionedStyle()) {
        RemoveProperty(NormalPositionProperty());
      }

      handleStickyChange = disp->mPosition == StylePositionProperty::Sticky ||
                           oldDisp->mPosition == StylePositionProperty::Sticky;
    }
  } else {  // !aOldComputedStyle
    handleStickyChange = disp->mPosition == StylePositionProperty::Sticky;
  }

  if (handleStickyChange && !HasAnyStateBits(NS_FRAME_IS_NONDISPLAY) &&
      !GetPrevInFlow()) {
    // Note that we only add first continuations, but we really only
    // want to add first continuation-or-ib-split-siblings. But since we don't
    // yet know if we're a later part of a block-in-inline split, we'll just
    // add later members of a block-in-inline split here, and then
    // StickyScrollContainer will remove them later.
    if (auto* ssc =
            StickyScrollContainer::GetStickyScrollContainerForFrame(this)) {
      if (disp->mPosition == StylePositionProperty::Sticky) {
        ssc->AddFrame(this);
      } else {
        ssc->RemoveFrame(this);
      }
    }
  }

  imgIRequest* oldBorderImage =
      aOldComputedStyle
          ? aOldComputedStyle->StyleBorder()->GetBorderImageRequest()
          : nullptr;
  imgIRequest* newBorderImage = StyleBorder()->GetBorderImageRequest();
  // FIXME (Bug 759996): The following is no longer true.
  // For border-images, we can't be as conservative (we need to set the
  // new loaders if there has been any change) since the CalcDifference
  // call depended on the result of GetComputedBorder() and that result
  // depends on whether the image has loaded, start the image load now
  // so that we'll get notified when it completes loading and can do a
  // restyle.  Otherwise, the image might finish loading from the
  // network before we start listening to its notifications, and then
  // we'll never know that it's finished loading.  Likewise, we want to
  // do this for freshly-created frames to prevent a similar race if the
  // image loads between reflow (which can depend on whether the image
  // is loaded) and paint.  We also don't really care about any callers who try
  // to paint borders with a different style, because they won't have the
  // correct size for the border either.
  if (oldBorderImage != newBorderImage) {
    // stop and restart the image loading/notification
    if (oldBorderImage && HasImageRequest()) {
      RemoveProperty(CachedBorderImageDataProperty());
      loader->DisassociateRequestFromFrame(oldBorderImage, this);
    }
    if (newBorderImage) {
      loader->AssociateRequestToFrame(newBorderImage, this, 0);
    }
  }

  auto GetShapeImageRequest = [](const ComputedStyle* aStyle) -> imgIRequest* {
    if (!aStyle) {
      return nullptr;
    }
    auto& shape = aStyle->StyleDisplay()->mShapeOutside;
    if (!shape.IsImage()) {
      return nullptr;
    }
    return shape.AsImage().GetImageRequest();
  };

  imgIRequest* oldShapeImage = GetShapeImageRequest(aOldComputedStyle);
  imgIRequest* newShapeImage = GetShapeImageRequest(Style());
  if (oldShapeImage != newShapeImage) {
    if (oldShapeImage && HasImageRequest()) {
      loader->DisassociateRequestFromFrame(oldShapeImage, this);
    }
    if (newShapeImage) {
      loader->AssociateRequestToFrame(newShapeImage, this,
                                      ImageLoader::REQUEST_REQUIRES_REFLOW);
    }
  }

  // SVGObserverUtils::GetEffectProperties() asserts that we only invoke it with
  // the first continuation so we need to check that in advance.
  const bool isNonTextFirstContinuation = isNonText && !GetPrevContinuation();
  if (isNonTextFirstContinuation) {
    // Kick off loading of external SVG resources referenced from properties if
    // any. This currently includes filter, clip-path, and mask.
    SVGObserverUtils::InitiateResourceDocLoads(this);
  }

  // If the page contains markup that overrides text direction, and
  // does not contain any characters that would activate the Unicode
  // bidi algorithm, we need to call |SetBidiEnabled| on the pres
  // context before reflow starts.  See bug 115921.
  if (StyleVisibility()->mDirection == StyleDirection::Rtl) {
    PresContext()->SetBidiEnabled();
  }

  // The following part is for caching offset-path:path(). We cache the
  // flatten gfx path, so we don't have to rebuild and re-flattern it at
  // each cycle if we have animations on offset-* with a fixed offset-path.
  const StyleOffsetPath* oldPath =
      aOldComputedStyle ? &aOldComputedStyle->StyleDisplay()->mOffsetPath
                        : nullptr;
  const StyleOffsetPath& newPath = StyleDisplay()->mOffsetPath;
  if (!oldPath || *oldPath != newPath) {
    if (newPath.IsPath()) {
      // Here we only need to build a valid path for motion path, so
      // using the default values of stroke-width, stoke-linecap, and fill-rule
      // is fine for now because what we want is to get the point and its normal
      // vector along the path, instead of rendering it.
      RefPtr<gfx::PathBuilder> builder =
          gfxPlatform::GetPlatform()
              ->ScreenReferenceDrawTarget()
              ->CreatePathBuilder(gfx::FillRule::FILL_WINDING);
      RefPtr<gfx::Path> path =
          MotionPathUtils::BuildPath(newPath.AsPath(), builder);
      if (path) {
        // The newPath could be path('') (i.e. empty path), so its gfx path
        // could be nullptr, and so we only set property for a non-empty path.
        SetProperty(nsIFrame::OffsetPathCache(), path.forget().take());
      } else {
        // May have an old cached path, so we have to delete it.
        RemoveProperty(nsIFrame::OffsetPathCache());
      }
    } else if (oldPath) {
      RemoveProperty(nsIFrame::OffsetPathCache());
    }
  }

  RemoveStateBits(NS_FRAME_SIMPLE_EVENT_REGIONS | NS_FRAME_SIMPLE_DISPLAYLIST);

  mMayHaveRoundedCorners = true;
}

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
void nsIFrame::AssertNewStyleIsSane(ComputedStyle& aNewStyle) {
  MOZ_DIAGNOSTIC_ASSERT(
      aNewStyle.GetPseudoType() == mComputedStyle->GetPseudoType() ||
      // ::first-line continuations are weird, this should probably be fixed via
      // bug 1465474.
      (mComputedStyle->GetPseudoType() == PseudoStyleType::firstLine &&
       aNewStyle.GetPseudoType() == PseudoStyleType::mozLineFrame) ||
      // ::first-letter continuations are broken, in particular floating ones,
      // see bug 1490281. The construction code tries to fix this up after the
      // fact, then restyling undoes it...
      (mComputedStyle->GetPseudoType() == PseudoStyleType::mozText &&
       aNewStyle.GetPseudoType() == PseudoStyleType::firstLetterContinuation) ||
      (mComputedStyle->GetPseudoType() ==
           PseudoStyleType::firstLetterContinuation &&
       aNewStyle.GetPseudoType() == PseudoStyleType::mozText));
}
#endif

void nsIFrame::ReparentFrameViewTo(nsViewManager* aViewManager,
                                   nsView* aNewParentView,
                                   nsView* aOldParentView) {
  if (HasView()) {
#ifdef MOZ_XUL
    if (IsMenuPopupFrame()) {
      // This view must be parented by the root view, don't reparent it.
      return;
    }
#endif
    nsView* view = GetView();
    // Verify that the current parent view is what we think it is
    // nsView*  parentView;
    // NS_ASSERTION(parentView == aOldParentView, "unexpected parent view");

    aViewManager->RemoveChild(view);

    // The view will remember the Z-order and other attributes that have been
    // set on it.
    nsView* insertBefore =
        nsLayoutUtils::FindSiblingViewFor(aNewParentView, this);
    aViewManager->InsertChild(aNewParentView, view, insertBefore,
                              insertBefore != nullptr);
  } else if (GetStateBits() & NS_FRAME_HAS_CHILD_WITH_VIEW) {
    nsIFrame::ChildListIterator lists(this);
    for (; !lists.IsDone(); lists.Next()) {
      // Iterate the child frames, and check each child frame to see if it has
      // a view
      nsFrameList::Enumerator childFrames(lists.CurrentList());
      for (; !childFrames.AtEnd(); childFrames.Next()) {
        childFrames.get()->ReparentFrameViewTo(aViewManager, aNewParentView,
                                               aOldParentView);
      }
    }
  }
}

void nsIFrame::SyncFrameViewProperties(nsView* aView) {
  if (!aView) {
    aView = GetView();
    if (!aView) {
      return;
    }
  }

  nsViewManager* vm = aView->GetViewManager();

  // Make sure visibility is correct. This only affects nsSubDocumentFrame.
  if (!SupportsVisibilityHidden()) {
    // See if the view should be hidden or visible
    ComputedStyle* sc = Style();
    vm->SetViewVisibility(aView, sc->StyleVisibility()->IsVisible()
                                     ? nsViewVisibility_kShow
                                     : nsViewVisibility_kHide);
  }

  int32_t zIndex = 0;
  bool autoZIndex = false;

  if (IsAbsPosContainingBlock()) {
    // Make sure z-index is correct
    ComputedStyle* sc = Style();
    const nsStylePosition* position = sc->StylePosition();
    if (position->mZIndex.IsInteger()) {
      zIndex = position->mZIndex.AsInteger();
    } else {
      MOZ_ASSERT(position->mZIndex.IsAuto());
      autoZIndex = true;
    }
  } else {
    autoZIndex = true;
  }

  vm->SetViewZIndex(aView, autoZIndex, zIndex);
}

void nsFrame::CreateView() {
  MOZ_ASSERT(!HasView());

  nsView* parentView = GetParent()->GetClosestView();
  MOZ_ASSERT(parentView, "no parent with view");

  nsViewManager* viewManager = parentView->GetViewManager();
  MOZ_ASSERT(viewManager, "null view manager");

  nsView* view = viewManager->CreateView(GetRect(), parentView);
  SyncFrameViewProperties(view);

  nsView* insertBefore = nsLayoutUtils::FindSiblingViewFor(parentView, this);
  // we insert this view 'above' the insertBefore view, unless insertBefore is
  // null, in which case we want to call with aAbove == false to insert at the
  // beginning in document order
  viewManager->InsertChild(parentView, view, insertBefore,
                           insertBefore != nullptr);

  // REVIEW: Don't create a widget for fixed-pos elements anymore.
  // ComputeRepaintRegionForCopy will calculate the right area to repaint
  // when we scroll.
  // Reparent views on any child frames (or their descendants) to this
  // view. We can just call ReparentFrameViewTo on this frame because
  // we know this frame has no view, so it will crawl the children. Also,
  // we know that any descendants with views must have 'parentView' as their
  // parent view.
  ReparentFrameViewTo(viewManager, view, parentView);

  // Remember our view
  SetView(view);

  NS_FRAME_LOG(NS_FRAME_TRACE_CALLS,
               ("nsFrame::CreateView: frame=%p view=%p", this, view));
}

// MSVC fails with link error "one or more multiply defined symbols found",
// gcc fails with "hidden symbol `nsIFrame::kPrincipalList' isn't defined"
// etc if they are not defined.
#ifndef _MSC_VER
// static nsIFrame constants; initialized in the header file.
const nsIFrame::ChildListID nsIFrame::kPrincipalList;
const nsIFrame::ChildListID nsIFrame::kAbsoluteList;
const nsIFrame::ChildListID nsIFrame::kBulletList;
const nsIFrame::ChildListID nsIFrame::kCaptionList;
const nsIFrame::ChildListID nsIFrame::kColGroupList;
const nsIFrame::ChildListID nsIFrame::kExcessOverflowContainersList;
const nsIFrame::ChildListID nsIFrame::kFixedList;
const nsIFrame::ChildListID nsIFrame::kFloatList;
const nsIFrame::ChildListID nsIFrame::kOverflowContainersList;
const nsIFrame::ChildListID nsIFrame::kOverflowList;
const nsIFrame::ChildListID nsIFrame::kOverflowOutOfFlowList;
const nsIFrame::ChildListID nsIFrame::kPopupList;
const nsIFrame::ChildListID nsIFrame::kPushedFloatsList;
const nsIFrame::ChildListID nsIFrame::kSelectPopupList;
const nsIFrame::ChildListID nsIFrame::kNoReflowPrincipalList;
#endif

/* virtual */
nsMargin nsIFrame::GetUsedMargin() const {
  nsMargin margin(0, 0, 0, 0);
  if (((mState & NS_FRAME_FIRST_REFLOW) && !(mState & NS_FRAME_IN_REFLOW)) ||
      nsSVGUtils::IsInSVGTextSubtree(this))
    return margin;

  nsMargin* m = GetProperty(UsedMarginProperty());
  if (m) {
    margin = *m;
  } else {
    if (!StyleMargin()->GetMargin(margin)) {
      // If we get here, our caller probably shouldn't be calling us...
      NS_ERROR(
          "Returning bogus 0-sized margin, because this margin "
          "depends on layout & isn't cached!");
    }
  }
  return margin;
}

/* virtual */
nsMargin nsIFrame::GetUsedBorder() const {
  nsMargin border(0, 0, 0, 0);
  if (((mState & NS_FRAME_FIRST_REFLOW) && !(mState & NS_FRAME_IN_REFLOW)) ||
      nsSVGUtils::IsInSVGTextSubtree(this))
    return border;

  // Theme methods don't use const-ness.
  nsIFrame* mutable_this = const_cast<nsIFrame*>(this);

  const nsStyleDisplay* disp = StyleDisplay();
  if (mutable_this->IsThemed(disp)) {
    nsPresContext* pc = PresContext();
    LayoutDeviceIntMargin widgetBorder = pc->Theme()->GetWidgetBorder(
        pc->DeviceContext(), mutable_this, disp->mAppearance);
    border =
        LayoutDevicePixel::ToAppUnits(widgetBorder, pc->AppUnitsPerDevPixel());
    return border;
  }

  nsMargin* b = GetProperty(UsedBorderProperty());
  if (b) {
    border = *b;
  } else {
    border = StyleBorder()->GetComputedBorder();
  }
  return border;
}

/* virtual */
nsMargin nsIFrame::GetUsedPadding() const {
  nsMargin padding(0, 0, 0, 0);
  if (((mState & NS_FRAME_FIRST_REFLOW) && !(mState & NS_FRAME_IN_REFLOW)) ||
      nsSVGUtils::IsInSVGTextSubtree(this))
    return padding;

  // Theme methods don't use const-ness.
  nsIFrame* mutable_this = const_cast<nsIFrame*>(this);

  const nsStyleDisplay* disp = StyleDisplay();
  if (mutable_this->IsThemed(disp)) {
    nsPresContext* pc = PresContext();
    LayoutDeviceIntMargin widgetPadding;
    if (pc->Theme()->GetWidgetPadding(pc->DeviceContext(), mutable_this,
                                      disp->mAppearance, &widgetPadding)) {
      return LayoutDevicePixel::ToAppUnits(widgetPadding,
                                           pc->AppUnitsPerDevPixel());
    }
  }

  nsMargin* p = GetProperty(UsedPaddingProperty());
  if (p) {
    padding = *p;
  } else {
    if (!StylePadding()->GetPadding(padding)) {
      // If we get here, our caller probably shouldn't be calling us...
      NS_ERROR(
          "Returning bogus 0-sized padding, because this padding "
          "depends on layout & isn't cached!");
    }
  }
  return padding;
}

nsIFrame::Sides nsIFrame::GetSkipSides(const ReflowInput* aReflowInput) const {
  if (MOZ_UNLIKELY(StyleBorder()->mBoxDecorationBreak ==
                   StyleBoxDecorationBreak::Clone) &&
      !(GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER)) {
    return Sides();
  }

  // Convert the logical skip sides to physical sides using the frame's
  // writing mode
  WritingMode writingMode = GetWritingMode();
  LogicalSides logicalSkip = GetLogicalSkipSides(aReflowInput);
  Sides skip;

  if (logicalSkip.BStart()) {
    if (writingMode.IsVertical()) {
      skip |= writingMode.IsVerticalLR() ? SideBits::eLeft : SideBits::eRight;
    } else {
      skip |= SideBits::eTop;
    }
  }

  if (logicalSkip.BEnd()) {
    if (writingMode.IsVertical()) {
      skip |= writingMode.IsVerticalLR() ? SideBits::eRight : SideBits::eLeft;
    } else {
      skip |= SideBits::eBottom;
    }
  }

  if (logicalSkip.IStart()) {
    if (writingMode.IsVertical()) {
      skip |= SideBits::eTop;
    } else {
      skip |= writingMode.IsBidiLTR() ? SideBits::eLeft : SideBits::eRight;
    }
  }

  if (logicalSkip.IEnd()) {
    if (writingMode.IsVertical()) {
      skip |= SideBits::eBottom;
    } else {
      skip |= writingMode.IsBidiLTR() ? SideBits::eRight : SideBits::eLeft;
    }
  }
  return skip;
}

nsRect nsIFrame::GetPaddingRectRelativeToSelf() const {
  nsMargin border = GetUsedBorder().ApplySkipSides(GetSkipSides());
  nsRect r(0, 0, mRect.width, mRect.height);
  r.Deflate(border);
  return r;
}

nsRect nsIFrame::GetPaddingRect() const {
  return GetPaddingRectRelativeToSelf() + GetPosition();
}

WritingMode nsIFrame::WritingModeForLine(WritingMode aSelfWM,
                                         nsIFrame* aSubFrame) const {
  MOZ_ASSERT(aSelfWM == GetWritingMode());
  WritingMode writingMode = aSelfWM;

  if (StyleTextReset()->mUnicodeBidi & NS_STYLE_UNICODE_BIDI_PLAINTEXT) {
    nsBidiLevel frameLevel = nsBidiPresUtils::GetFrameBaseLevel(aSubFrame);
    writingMode.SetDirectionFromBidiLevel(frameLevel);
  }

  return writingMode;
}

nsRect nsIFrame::GetMarginRectRelativeToSelf() const {
  nsMargin m = GetUsedMargin().ApplySkipSides(GetSkipSides());
  nsRect r(0, 0, mRect.width, mRect.height);
  r.Inflate(m);
  return r;
}

bool nsIFrame::IsTransformed(const nsStyleDisplay* aStyleDisplay) const {
  return IsCSSTransformed(aStyleDisplay) || IsSVGTransformed();
}

bool nsIFrame::IsCSSTransformed(const nsStyleDisplay* aStyleDisplay) const {
  MOZ_ASSERT(aStyleDisplay == StyleDisplay());
  return ((mState & NS_FRAME_MAY_BE_TRANSFORMED) &&
          (aStyleDisplay->HasTransform(this) || HasAnimationOfTransform()));
}

bool nsIFrame::HasAnimationOfTransform() const {
  return IsPrimaryFrame() &&
         nsLayoutUtils::HasAnimationOfTransformAndMotionPath(this) &&
         IsFrameOfType(eSupportsCSSTransforms);
}

bool nsIFrame::ChildrenHavePerspective(
    const nsStyleDisplay* aStyleDisplay) const {
  MOZ_ASSERT(aStyleDisplay == StyleDisplay());
  return aStyleDisplay->HasPerspective(this);
}

bool nsIFrame::HasAnimationOfOpacity(EffectSet* aEffectSet) const {
  return ((nsLayoutUtils::IsPrimaryStyleFrame(this) ||
           nsLayoutUtils::FirstContinuationOrIBSplitSibling(this)
               ->IsPrimaryFrame()) &&
          nsLayoutUtils::HasAnimationOfPropertySet(
              this, nsCSSPropertyIDSet::OpacityProperties(), aEffectSet));
}

bool nsIFrame::HasOpacityInternal(float aThreshold,
                                  const nsStyleDisplay* aStyleDisplay,
                                  const nsStyleEffects* aStyleEffects,
                                  EffectSet* aEffectSet) const {
  MOZ_ASSERT(0.0 <= aThreshold && aThreshold <= 1.0, "Invalid argument");
  if (aStyleEffects->mOpacity < aThreshold ||
      (aStyleDisplay->mWillChange.bits & StyleWillChangeBits::OPACITY)) {
    return true;
  }

  if (!mMayHaveOpacityAnimation) {
    return false;
  }

  return HasAnimationOfOpacity(aEffectSet);
}

bool nsIFrame::IsSVGTransformed(gfx::Matrix* aOwnTransforms,
                                gfx::Matrix* aFromParentTransforms) const {
  return false;
}

bool nsIFrame::Extend3DContext(const nsStyleDisplay* aStyleDisplay,
                               const nsStyleEffects* aStyleEffects,
                               mozilla::EffectSet* aEffectSetForOpacity) const {
  if (!(mState & NS_FRAME_MAY_BE_TRANSFORMED)) {
    return false;
  }
  const nsStyleDisplay* disp = StyleDisplayWithOptionalParam(aStyleDisplay);
  if (disp->mTransformStyle != StyleTransformStyle::Preserve3d ||
      !IsFrameOfType(nsIFrame::eSupportsCSSTransforms)) {
    return false;
  }

  // If we're all scroll frame, then all descendants will be clipped, so we
  // can't preserve 3d.
  if (IsScrollFrame()) {
    return false;
  }

  const nsStyleEffects* effects = StyleEffectsWithOptionalParam(aStyleEffects);
  if (HasOpacity(disp, effects, aEffectSetForOpacity)) {
    return false;
  }

  return !nsFrame::ShouldApplyOverflowClipping(this, disp) &&
         !GetClipPropClipRect(disp, effects, GetSize()) &&
         !nsSVGIntegrationUtils::UsingEffectsForFrame(this);
}

bool nsIFrame::Combines3DTransformWithAncestors(
    const nsStyleDisplay* aStyleDisplay) const {
  MOZ_ASSERT(aStyleDisplay == StyleDisplay());
  nsIFrame* parent = GetClosestFlattenedTreeAncestorPrimaryFrame();
  if (!parent || !parent->Extend3DContext()) {
    return false;
  }
  return IsCSSTransformed(aStyleDisplay) || BackfaceIsHidden(aStyleDisplay);
}

bool nsIFrame::In3DContextAndBackfaceIsHidden() const {
  // While both tests fail most of the time, test BackfaceIsHidden()
  // first since it's likely to fail faster.
  const nsStyleDisplay* disp = StyleDisplay();
  return BackfaceIsHidden(disp) && Combines3DTransformWithAncestors(disp);
}

bool nsIFrame::HasPerspective(const nsStyleDisplay* aStyleDisplay) const {
  MOZ_ASSERT(aStyleDisplay == StyleDisplay());
  if (!IsTransformed(aStyleDisplay)) {
    return false;
  }
  nsIFrame* containingBlock =
      GetContainingBlock(SKIP_SCROLLED_FRAME, aStyleDisplay);
  if (!containingBlock) {
    return false;
  }
  return containingBlock->ChildrenHavePerspective();
}

nsRect nsIFrame::GetContentRectRelativeToSelf() const {
  nsMargin bp = GetUsedBorderAndPadding().ApplySkipSides(GetSkipSides());
  nsRect r(0, 0, mRect.width, mRect.height);
  r.Deflate(bp);
  return r;
}

nsRect nsIFrame::GetContentRect() const {
  return GetContentRectRelativeToSelf() + GetPosition();
}

bool nsIFrame::ComputeBorderRadii(const BorderRadius& aBorderRadius,
                                  const nsSize& aFrameSize,
                                  const nsSize& aBorderArea, Sides aSkipSides,
                                  nscoord aRadii[8]) {
  // Percentages are relative to whichever side they're on.
  for (const auto i : mozilla::AllPhysicalHalfCorners()) {
    const LengthPercentage& c = aBorderRadius.Get(i);
    nscoord axis = HalfCornerIsX(i) ? aFrameSize.width : aFrameSize.height;
    aRadii[i] = std::max(0, c.Resolve(axis));
  }

  if (aSkipSides.Top()) {
    aRadii[eCornerTopLeftX] = 0;
    aRadii[eCornerTopLeftY] = 0;
    aRadii[eCornerTopRightX] = 0;
    aRadii[eCornerTopRightY] = 0;
  }

  if (aSkipSides.Right()) {
    aRadii[eCornerTopRightX] = 0;
    aRadii[eCornerTopRightY] = 0;
    aRadii[eCornerBottomRightX] = 0;
    aRadii[eCornerBottomRightY] = 0;
  }

  if (aSkipSides.Bottom()) {
    aRadii[eCornerBottomRightX] = 0;
    aRadii[eCornerBottomRightY] = 0;
    aRadii[eCornerBottomLeftX] = 0;
    aRadii[eCornerBottomLeftY] = 0;
  }

  if (aSkipSides.Left()) {
    aRadii[eCornerBottomLeftX] = 0;
    aRadii[eCornerBottomLeftY] = 0;
    aRadii[eCornerTopLeftX] = 0;
    aRadii[eCornerTopLeftY] = 0;
  }

  // css3-background specifies this algorithm for reducing
  // corner radii when they are too big.
  bool haveRadius = false;
  double ratio = 1.0f;
  for (const auto side : mozilla::AllPhysicalSides()) {
    uint32_t hc1 = SideToHalfCorner(side, false, true);
    uint32_t hc2 = SideToHalfCorner(side, true, true);
    nscoord length =
        SideIsVertical(side) ? aBorderArea.height : aBorderArea.width;
    nscoord sum = aRadii[hc1] + aRadii[hc2];
    if (sum) {
      haveRadius = true;
      // avoid floating point division in the normal case
      if (length < sum) {
        ratio = std::min(ratio, double(length) / sum);
      }
    }
  }
  if (ratio < 1.0) {
    for (const auto corner : mozilla::AllPhysicalHalfCorners()) {
      aRadii[corner] *= ratio;
    }
  }

  return haveRadius;
}

/* static */
void nsIFrame::InsetBorderRadii(nscoord aRadii[8], const nsMargin& aOffsets) {
  for (const auto side : mozilla::AllPhysicalSides()) {
    nscoord offset = aOffsets.Side(side);
    uint32_t hc1 = SideToHalfCorner(side, false, false);
    uint32_t hc2 = SideToHalfCorner(side, true, false);
    aRadii[hc1] = std::max(0, aRadii[hc1] - offset);
    aRadii[hc2] = std::max(0, aRadii[hc2] - offset);
  }
}

/* static */
void nsIFrame::OutsetBorderRadii(nscoord aRadii[8], const nsMargin& aOffsets) {
  auto AdjustOffset = [](const uint32_t aRadius, const nscoord aOffset) {
    // Implement the cubic formula to adjust offset when aOffset > 0 and
    // aRadius / aOffset < 1.
    // https://drafts.csswg.org/css-shapes/#valdef-shape-box-margin-box
    if (aOffset > 0) {
      const double ratio = aRadius / double(aOffset);
      if (ratio < 1.0) {
        return nscoord(aOffset * (1.0 + std::pow(ratio - 1, 3)));
      }
    }
    return aOffset;
  };

  for (const auto side : mozilla::AllPhysicalSides()) {
    const nscoord offset = aOffsets.Side(side);
    const uint32_t hc1 = SideToHalfCorner(side, false, false);
    const uint32_t hc2 = SideToHalfCorner(side, true, false);
    if (aRadii[hc1] > 0) {
      const nscoord offset1 = AdjustOffset(aRadii[hc1], offset);
      aRadii[hc1] = std::max(0, aRadii[hc1] + offset1);
    }
    if (aRadii[hc2] > 0) {
      const nscoord offset2 = AdjustOffset(aRadii[hc2], offset);
      aRadii[hc2] = std::max(0, aRadii[hc2] + offset2);
    }
  }
}

static inline bool RadiiAreDefinitelyZero(const BorderRadius& aBorderRadius) {
  for (const auto corner : mozilla::AllPhysicalHalfCorners()) {
    if (!aBorderRadius.Get(corner).IsDefinitelyZero()) {
      return false;
    }
  }
  return true;
}

/* virtual */
bool nsIFrame::GetBorderRadii(const nsSize& aFrameSize,
                              const nsSize& aBorderArea, Sides aSkipSides,
                              nscoord aRadii[8]) const {
  if (!mMayHaveRoundedCorners) {
    memset(aRadii, 0, sizeof(nscoord) * 8);
    return false;
  }

  if (IsThemed()) {
    // When we're themed, the native theme code draws the border and
    // background, and therefore it doesn't make sense to tell other
    // code that's interested in border-radius that we have any radii.
    //
    // In an ideal world, we might have a way for the them to tell us an
    // border radius, but since we don't, we're better off assuming
    // zero.
    for (const auto corner : mozilla::AllPhysicalHalfCorners()) {
      aRadii[corner] = 0;
    }
    return false;
  }

  const auto& radii = StyleBorder()->mBorderRadius;
  const bool hasRadii =
      ComputeBorderRadii(radii, aFrameSize, aBorderArea, aSkipSides, aRadii);
  if (!hasRadii) {
    // TODO(emilio): Maybe we can just remove this bit and do the
    // IsDefinitelyZero check unconditionally. That should still avoid most of
    // the work, though maybe not the cache miss of going through the style and
    // the border struct.
    const_cast<nsIFrame*>(this)->mMayHaveRoundedCorners =
        !RadiiAreDefinitelyZero(radii);
  }
  return hasRadii;
}

bool nsIFrame::GetBorderRadii(nscoord aRadii[8]) const {
  nsSize sz = GetSize();
  return GetBorderRadii(sz, sz, GetSkipSides(), aRadii);
}

bool nsIFrame::GetMarginBoxBorderRadii(nscoord aRadii[8]) const {
  return GetBoxBorderRadii(aRadii, GetUsedMargin(), true);
}

bool nsIFrame::GetPaddingBoxBorderRadii(nscoord aRadii[8]) const {
  return GetBoxBorderRadii(aRadii, GetUsedBorder(), false);
}

bool nsIFrame::GetContentBoxBorderRadii(nscoord aRadii[8]) const {
  return GetBoxBorderRadii(aRadii, GetUsedBorderAndPadding(), false);
}

bool nsIFrame::GetBoxBorderRadii(nscoord aRadii[8], nsMargin aOffset,
                                 bool aIsOutset) const {
  if (!GetBorderRadii(aRadii)) return false;
  if (aIsOutset) {
    OutsetBorderRadii(aRadii, aOffset);
  } else {
    InsetBorderRadii(aRadii, aOffset);
  }
  for (const auto corner : mozilla::AllPhysicalHalfCorners()) {
    if (aRadii[corner]) return true;
  }
  return false;
}

bool nsIFrame::GetShapeBoxBorderRadii(nscoord aRadii[8]) const {
  using Tag = StyleShapeOutside::Tag;
  auto& shapeOutside = StyleDisplay()->mShapeOutside;
  auto box = StyleShapeBox::MarginBox;
  switch (shapeOutside.tag) {
    case Tag::Image:
    case Tag::None:
      return false;
    case Tag::Box:
      box = shapeOutside.AsBox();
      break;
    case Tag::Shape:
      box = shapeOutside.AsShape()._1;
      break;
  }

  switch (box) {
    case StyleShapeBox::ContentBox:
      return GetContentBoxBorderRadii(aRadii);
    case StyleShapeBox::PaddingBox:
      return GetPaddingBoxBorderRadii(aRadii);
    case StyleShapeBox::BorderBox:
      return GetBorderRadii(aRadii);
    case StyleShapeBox::MarginBox:
      return GetMarginBoxBorderRadii(aRadii);
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected box value");
      return false;
  }
}

ComputedStyle* nsFrame::GetAdditionalComputedStyle(int32_t aIndex) const {
  MOZ_ASSERT(aIndex >= 0, "invalid index number");
  return nullptr;
}

void nsFrame::SetAdditionalComputedStyle(int32_t aIndex,
                                         ComputedStyle* aComputedStyle) {
  MOZ_ASSERT(aIndex >= 0, "invalid index number");
}

nscoord nsFrame::GetLogicalBaseline(WritingMode aWritingMode) const {
  NS_ASSERTION(!NS_SUBTREE_DIRTY(this), "frame must not be dirty");
  // Baseline for inverted line content is the top (block-start) margin edge,
  // as the frame is in effect "flipped" for alignment purposes.
  if (aWritingMode.IsLineInverted()) {
    return -GetLogicalUsedMargin(aWritingMode).BStart(aWritingMode);
  }
  // Otherwise, the bottom margin edge, per CSS2.1's definition of the
  // 'baseline' value of 'vertical-align'.
  return BSize(aWritingMode) +
         GetLogicalUsedMargin(aWritingMode).BEnd(aWritingMode);
}

const nsFrameList& nsFrame::GetChildList(ChildListID aListID) const {
  if (IsAbsoluteContainer() && aListID == GetAbsoluteListID()) {
    return GetAbsoluteContainingBlock()->GetChildList();
  } else {
    return nsFrameList::EmptyList();
  }
}

void nsFrame::GetChildLists(nsTArray<ChildList>* aLists) const {
  if (IsAbsoluteContainer()) {
    nsFrameList absoluteList = GetAbsoluteContainingBlock()->GetChildList();
    absoluteList.AppendIfNonempty(aLists, GetAbsoluteListID());
  }
}

void nsIFrame::GetCrossDocChildLists(nsTArray<ChildList>* aLists) {
  nsSubDocumentFrame* subdocumentFrame = do_QueryFrame(this);
  if (subdocumentFrame) {
    // Descend into the subdocument
    nsIFrame* root = subdocumentFrame->GetSubdocumentRootFrame();
    if (root) {
      aLists->AppendElement(nsIFrame::ChildList(
          nsFrameList(root, nsLayoutUtils::GetLastSibling(root)),
          nsIFrame::kPrincipalList));
    }
  }

  GetChildLists(aLists);
}

Visibility nsIFrame::GetVisibility() const {
  if (!(GetStateBits() & NS_FRAME_VISIBILITY_IS_TRACKED)) {
    return Visibility::Untracked;
  }

  bool isSet = false;
  uint32_t visibleCount = GetProperty(VisibilityStateProperty(), &isSet);

  MOZ_ASSERT(isSet,
             "Should have a VisibilityStateProperty value "
             "if NS_FRAME_VISIBILITY_IS_TRACKED is set");

  return visibleCount > 0 ? Visibility::ApproximatelyVisible
                          : Visibility::ApproximatelyNonVisible;
}

void nsIFrame::UpdateVisibilitySynchronously() {
  mozilla::PresShell* presShell = PresShell();
  if (!presShell) {
    return;
  }

  if (presShell->AssumeAllFramesVisible()) {
    presShell->EnsureFrameInApproximatelyVisibleList(this);
    return;
  }

  bool visible = StyleVisibility()->IsVisible();
  nsIFrame* f = GetParent();
  nsRect rect = GetRectRelativeToSelf();
  nsIFrame* rectFrame = this;
  while (f && visible) {
    nsIScrollableFrame* sf = do_QueryFrame(f);
    if (sf) {
      nsRect transformedRect =
          nsLayoutUtils::TransformFrameRectToAncestor(rectFrame, rect, f);
      if (!sf->IsRectNearlyVisible(transformedRect)) {
        visible = false;
        break;
      }

      // In this code we're trying to synchronously update *approximate*
      // visibility. (In the future we may update precise visibility here as
      // well, which is why the method name does not contain 'approximate'.) The
      // IsRectNearlyVisible() check above tells us that the rect we're checking
      // is approximately visible within the scrollframe, but we still need to
      // ensure that, even if it was scrolled into view, it'd be visible when we
      // consider the rest of the document. To do that, we move transformedRect
      // to be contained in the scrollport as best we can (it might not fit) to
      // pretend that it was scrolled into view.
      rect = transformedRect.MoveInsideAndClamp(sf->GetScrollPortRect());
      rectFrame = f;
    }
    nsIFrame* parent = f->GetParent();
    if (!parent) {
      parent = nsLayoutUtils::GetCrossDocParentFrame(f);
      if (parent && parent->PresContext()->IsChrome()) {
        break;
      }
    }
    f = parent;
  }

  if (visible) {
    presShell->EnsureFrameInApproximatelyVisibleList(this);
  } else {
    presShell->RemoveFrameFromApproximatelyVisibleList(this);
  }
}

void nsIFrame::EnableVisibilityTracking() {
  if (GetStateBits() & NS_FRAME_VISIBILITY_IS_TRACKED) {
    return;  // Nothing to do.
  }

  MOZ_ASSERT(!HasProperty(VisibilityStateProperty()),
             "Shouldn't have a VisibilityStateProperty value "
             "if NS_FRAME_VISIBILITY_IS_TRACKED is not set");

  // Add the state bit so we know to track visibility for this frame, and
  // initialize the frame property.
  AddStateBits(NS_FRAME_VISIBILITY_IS_TRACKED);
  SetProperty(VisibilityStateProperty(), 0);

  mozilla::PresShell* presShell = PresShell();
  if (!presShell) {
    return;
  }

  // Schedule a visibility update. This method will virtually always be called
  // when layout has changed anyway, so it's very unlikely that any additional
  // visibility updates will be triggered by this, but this way we guarantee
  // that if this frame is currently visible we'll eventually find out.
  presShell->ScheduleApproximateFrameVisibilityUpdateSoon();
}

void nsIFrame::DisableVisibilityTracking() {
  if (!(GetStateBits() & NS_FRAME_VISIBILITY_IS_TRACKED)) {
    return;  // Nothing to do.
  }

  bool isSet = false;
  uint32_t visibleCount = TakeProperty(VisibilityStateProperty(), &isSet);

  MOZ_ASSERT(isSet,
             "Should have a VisibilityStateProperty value "
             "if NS_FRAME_VISIBILITY_IS_TRACKED is set");

  RemoveStateBits(NS_FRAME_VISIBILITY_IS_TRACKED);

  if (visibleCount == 0) {
    return;  // We were nonvisible.
  }

  // We were visible, so send an OnVisibilityChange() notification.
  OnVisibilityChange(Visibility::ApproximatelyNonVisible);
}

void nsIFrame::DecApproximateVisibleCount(
    const Maybe<OnNonvisible>& aNonvisibleAction
    /* = Nothing() */) {
  MOZ_ASSERT(GetStateBits() & NS_FRAME_VISIBILITY_IS_TRACKED);

  bool isSet = false;
  uint32_t visibleCount = GetProperty(VisibilityStateProperty(), &isSet);

  MOZ_ASSERT(isSet,
             "Should have a VisibilityStateProperty value "
             "if NS_FRAME_VISIBILITY_IS_TRACKED is set");
  MOZ_ASSERT(visibleCount > 0,
             "Frame is already nonvisible and we're "
             "decrementing its visible count?");

  visibleCount--;
  SetProperty(VisibilityStateProperty(), visibleCount);
  if (visibleCount > 0) {
    return;
  }

  // We just became nonvisible, so send an OnVisibilityChange() notification.
  OnVisibilityChange(Visibility::ApproximatelyNonVisible, aNonvisibleAction);
}

void nsIFrame::IncApproximateVisibleCount() {
  MOZ_ASSERT(GetStateBits() & NS_FRAME_VISIBILITY_IS_TRACKED);

  bool isSet = false;
  uint32_t visibleCount = GetProperty(VisibilityStateProperty(), &isSet);

  MOZ_ASSERT(isSet,
             "Should have a VisibilityStateProperty value "
             "if NS_FRAME_VISIBILITY_IS_TRACKED is set");

  visibleCount++;
  SetProperty(VisibilityStateProperty(), visibleCount);
  if (visibleCount > 1) {
    return;
  }

  // We just became visible, so send an OnVisibilityChange() notification.
  OnVisibilityChange(Visibility::ApproximatelyVisible);
}

void nsIFrame::OnVisibilityChange(Visibility aNewVisibility,
                                  const Maybe<OnNonvisible>& aNonvisibleAction
                                  /* = Nothing() */) {
  // XXX(seth): In bug 1218990 we'll implement visibility tracking for CSS
  // images here.
}

static nsIFrame* GetActiveSelectionFrame(nsPresContext* aPresContext,
                                         nsIFrame* aFrame) {
  nsIContent* capturingContent = PresShell::GetCapturingContent();
  if (capturingContent) {
    nsIFrame* activeFrame = aPresContext->GetPrimaryFrameFor(capturingContent);
    return activeFrame ? activeFrame : aFrame;
  }

  return aFrame;
}

int16_t nsFrame::DetermineDisplaySelection() {
  int16_t selType = nsISelectionController::SELECTION_OFF;

  nsCOMPtr<nsISelectionController> selCon;
  nsresult result =
      GetSelectionController(PresContext(), getter_AddRefs(selCon));
  if (NS_SUCCEEDED(result) && selCon) {
    result = selCon->GetDisplaySelection(&selType);
    if (NS_SUCCEEDED(result) &&
        (selType != nsISelectionController::SELECTION_OFF)) {
      // Check whether style allows selection.
      if (!IsSelectable(nullptr)) {
        selType = nsISelectionController::SELECTION_OFF;
      }
    }
  }
  return selType;
}

class nsDisplaySelectionOverlay : public nsPaintedDisplayItem {
 public:
  /**
   * @param aSelectionValue nsISelectionController::getDisplaySelection.
   */
  nsDisplaySelectionOverlay(nsDisplayListBuilder* aBuilder, nsFrame* aFrame,
                            int16_t aSelectionValue)
      : nsPaintedDisplayItem(aBuilder, aFrame),
        mSelectionValue(aSelectionValue) {
    MOZ_COUNT_CTOR(nsDisplaySelectionOverlay);
  }
  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplaySelectionOverlay)

  virtual void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;
  bool CreateWebRenderCommands(
      mozilla::wr::DisplayListBuilder& aBuilder,
      mozilla::wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      mozilla::layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;
  NS_DISPLAY_DECL_NAME("SelectionOverlay", TYPE_SELECTION_OVERLAY)
 private:
  DeviceColor ComputeColor() const;

  static DeviceColor ComputeColorFromSelectionStyle(ComputedStyle&);
  static DeviceColor ApplyTransparencyIfNecessary(nscolor);

  // nsISelectionController::getDisplaySelection.
  int16_t mSelectionValue;
};

DeviceColor nsDisplaySelectionOverlay::ApplyTransparencyIfNecessary(
    nscolor aColor) {
  // If it has already alpha, leave it like that.
  if (NS_GET_A(aColor) != 255) {
    return ToDeviceColor(aColor);
  }

  // NOTE(emilio): Blink and WebKit do something slightly different here, and
  // blend the color with white instead, both for overlays and text backgrounds.
  auto color = sRGBColor::FromABGR(aColor);
  color.a = 0.5;
  return ToDeviceColor(color);
}

DeviceColor nsDisplaySelectionOverlay::ComputeColorFromSelectionStyle(
    ComputedStyle& aStyle) {
  return ApplyTransparencyIfNecessary(
      aStyle.GetVisitedDependentColor(&nsStyleBackground::mBackgroundColor));
}

DeviceColor nsDisplaySelectionOverlay::ComputeColor() const {
  LookAndFeel::ColorID colorID;
  if (RefPtr<ComputedStyle> style =
          mFrame->ComputeSelectionStyle(mSelectionValue)) {
    return ComputeColorFromSelectionStyle(*style);
  }
  if (mSelectionValue == nsISelectionController::SELECTION_ON) {
    colorID = LookAndFeel::ColorID::TextSelectBackground;
  } else if (mSelectionValue == nsISelectionController::SELECTION_ATTENTION) {
    colorID = LookAndFeel::ColorID::TextSelectBackgroundAttention;
  } else {
    colorID = LookAndFeel::ColorID::TextSelectBackgroundDisabled;
  }

  return ApplyTransparencyIfNecessary(
      LookAndFeel::GetColor(colorID, NS_RGB(255, 255, 255)));
}

void nsDisplaySelectionOverlay::Paint(nsDisplayListBuilder* aBuilder,
                                      gfxContext* aCtx) {
  DrawTarget& aDrawTarget = *aCtx->GetDrawTarget();
  ColorPattern color(ComputeColor());

  nsIntRect pxRect = GetPaintRect().ToOutsidePixels(
      mFrame->PresContext()->AppUnitsPerDevPixel());
  Rect rect(pxRect.x, pxRect.y, pxRect.width, pxRect.height);
  MaybeSnapToDevicePixels(rect, aDrawTarget, true);

  aDrawTarget.FillRect(rect, color);
}

bool nsDisplaySelectionOverlay::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  wr::LayoutRect bounds = wr::ToLayoutRect(LayoutDeviceRect::FromAppUnits(
      nsRect(ToReferenceFrame(), Frame()->GetSize()),
      mFrame->PresContext()->AppUnitsPerDevPixel()));
  aBuilder.PushRect(bounds, bounds, !BackfaceIsHidden(),
                    wr::ToColorF(ComputeColor()));
  return true;
}

static Element* FindElementAncestorForMozSelection(nsIContent* aContent) {
  NS_ENSURE_TRUE(aContent, nullptr);
  while (aContent && aContent->IsInNativeAnonymousSubtree()) {
    aContent = aContent->GetClosestNativeAnonymousSubtreeRootParent();
  }
  NS_ASSERTION(aContent, "aContent isn't in non-anonymous tree?");
  return aContent ? aContent->GetAsElementOrParentElement() : nullptr;
}

already_AddRefed<ComputedStyle> nsIFrame::ComputeSelectionStyle(
    int16_t aSelectionStatus) const {
  // Just bail out if not a selection-status that ::selection applies to.
  if (aSelectionStatus != nsISelectionController::SELECTION_ON &&
      aSelectionStatus != nsISelectionController::SELECTION_DISABLED) {
    return nullptr;
  }
  // When in high-contrast mode, the style system ends up ignoring the color
  // declarations, which means that the ::selection style becomes the inherited
  // color, and default background. That's no good.
  if (!PresContext()->PrefSheetPrefs().mUseDocumentColors) {
    return nullptr;
  }
  Element* element = FindElementAncestorForMozSelection(GetContent());
  if (!element) {
    return nullptr;
  }
  return PresContext()->StyleSet()->ProbePseudoElementStyle(
      *element, PseudoStyleType::selection, Style());
}

template <typename SizeOrMaxSize>
static inline bool IsIntrinsicKeyword(const SizeOrMaxSize& aSize) {
  if (!aSize.IsExtremumLength()) {
    return false;
  }

  // All of the keywords except for '-moz-available' depend on intrinsic sizes.
  return aSize.AsExtremumLength() != StyleExtremumLength::MozAvailable;
}

bool nsIFrame::CanBeDynamicReflowRoot() const {
  if (!StaticPrefs::layout_dynamic_reflow_roots_enabled()) {
    return false;
  }

  auto& display = *StyleDisplay();
  if (IsFrameOfType(nsIFrame::eLineParticipant) ||
      nsStyleDisplay::IsRubyDisplayType(display.mDisplay) ||
      display.DisplayOutside() == StyleDisplayOutside::InternalTable ||
      display.DisplayInside() == StyleDisplayInside::Table ||
      (GetParent() && GetParent()->IsXULBoxFrame())) {
    // We have a display type where 'width' and 'height' don't actually set the
    // width or height (i.e., the size depends on content).
    MOZ_ASSERT(!HasAnyStateBits(NS_FRAME_DYNAMIC_REFLOW_ROOT),
               "should not have dynamic reflow root bit");
    return false;
  }

  // We can't serve as a dynamic reflow root if our used 'width' and 'height'
  // might be influenced by content.
  //
  // FIXME: For display:block, we should probably optimize inline-size: auto.
  // FIXME: Other flex and grid cases?
  auto& pos = *StylePosition();
  const auto& width = pos.mWidth;
  const auto& height = pos.mHeight;
  if (!width.IsLengthPercentage() || width.HasPercent() ||
      !height.IsLengthPercentage() || height.HasPercent() ||
      IsIntrinsicKeyword(pos.mMinWidth) || IsIntrinsicKeyword(pos.mMaxWidth) ||
      IsIntrinsicKeyword(pos.mMinHeight) ||
      IsIntrinsicKeyword(pos.mMaxHeight) ||
      ((pos.mMinWidth.IsAuto() || pos.mMinHeight.IsAuto()) &&
       IsFlexOrGridItem())) {
    return false;
  }

  // If our flex-basis is 'auto', it'll defer to 'width' (or 'height') which
  // we've already checked. Otherwise, it preempts them, so we need to
  // perform the same "could-this-value-be-influenced-by-content" checks that
  // we performed for 'width' and 'height' above.
  if (IsFlexItem()) {
    const auto& flexBasis = pos.mFlexBasis;
    if (!flexBasis.IsAuto()) {
      if (!flexBasis.IsSize() || !flexBasis.AsSize().IsLengthPercentage() ||
          flexBasis.AsSize().HasPercent()) {
        return false;
      }
    }
  }

  if (!IsFixedPosContainingBlock()) {
    // We can't treat this frame as a reflow root, since dynamic changes
    // to absolutely-positioned frames inside of it require that we
    // reflow the placeholder before we reflow the absolutely positioned
    // frame.
    // FIXME:  Alternatively, we could sort the reflow roots in
    // PresShell::ProcessReflowCommands by depth in the tree, from
    // deepest to least deep.  However, for performance (FIXME) we
    // should really be sorting them in the opposite order!
    return false;
  }

  // If we participate in a container's block reflow context, or margins
  // can collapse through us, we can't be a dynamic reflow root.
  if (IsBlockFrameOrSubclass() &&
      !HasAllStateBits(NS_BLOCK_FLOAT_MGR | NS_BLOCK_MARGIN_ROOT)) {
    return false;
  }

  // Subgrids are never reflow roots, but 'contain:layout/paint' prevents
  // creating a subgrid in the first place.
  if (pos.mGridTemplateColumns.IsSubgrid() ||
      pos.mGridTemplateRows.IsSubgrid()) {
    // NOTE: we could check that 'display' of our parent's primary frame is
    // '[inline-]grid' here but that's probably not worth it in practice.
    if (!display.IsContainLayout() && !display.IsContainPaint()) {
      return false;
    }
  }

  return true;
}

/********************************************************
 * Refreshes each content's frame
 *********************************************************/

void nsFrame::DisplaySelectionOverlay(nsDisplayListBuilder* aBuilder,
                                      nsDisplayList* aList,
                                      uint16_t aContentType) {
  if (!IsSelected() || !IsVisibleForPainting()) {
    return;
  }

  int16_t displaySelection = PresShell()->GetSelectionFlags();
  if (!(displaySelection & aContentType)) {
    return;
  }

  const nsFrameSelection* frameSelection = GetConstFrameSelection();
  int16_t selectionValue = frameSelection->GetDisplaySelection();

  if (selectionValue <= nsISelectionController::SELECTION_HIDDEN) {
    return;  // selection is hidden or off
  }

  nsIContent* newContent = mContent->GetParent();

  // check to see if we are anonymous content
  int32_t offset = 0;
  if (newContent) {
    // XXXbz there has GOT to be a better way of determining this!
    offset = newContent->ComputeIndexOf(mContent);
  }

  // look up to see what selection(s) are on this frame
  UniquePtr<SelectionDetails> details =
      frameSelection->LookUpSelection(newContent, offset, 1, false);
  if (!details) return;

  bool normal = false;
  for (SelectionDetails* sd = details.get(); sd; sd = sd->mNext.get()) {
    if (sd->mSelectionType == SelectionType::eNormal) {
      normal = true;
    }
  }

  if (!normal && aContentType == nsISelectionDisplay::DISPLAY_IMAGES) {
    // Don't overlay an image if it's not in the primary selection.
    return;
  }

  aList->AppendNewToTop<nsDisplaySelectionOverlay>(aBuilder, this,
                                                   selectionValue);
}

void nsFrame::DisplayOutlineUnconditional(nsDisplayListBuilder* aBuilder,
                                          const nsDisplayListSet& aLists) {
  // Per https://drafts.csswg.org/css-tables-3/#global-style-overrides:
  // "All css properties of table-column and table-column-group boxes are
  // ignored, except when explicitly specified by this specification."
  // CSS outlines fall into this category, so we skip them on these boxes.
  MOZ_ASSERT(!IsTableColGroupFrame() && !IsTableColFrame());

  if (!StyleOutline()->ShouldPaintOutline()) {
    return;
  }

  // Outlines are painted by the table wrapper frame.
  if (IsTableFrame()) {
    return;
  }

  if (HasAnyStateBits(NS_FRAME_PART_OF_IBSPLIT) &&
      GetScrollableOverflowRect().IsEmpty()) {
    // Skip parts of IB-splits with an empty overflow rect, see bug 434301.
    // We may still want to fix some of the overflow area calculations over in
    // that bug.
    return;
  }

  aLists.Outlines()->AppendNewToTop<nsDisplayOutline>(aBuilder, this);
}

void nsFrame::DisplayOutline(nsDisplayListBuilder* aBuilder,
                             const nsDisplayListSet& aLists) {
  if (!IsVisibleForPainting()) return;

  DisplayOutlineUnconditional(aBuilder, aLists);
}

void nsFrame::DisplayInsetBoxShadowUnconditional(nsDisplayListBuilder* aBuilder,
                                                 nsDisplayList* aList) {
  // XXXbz should box-shadow for rows/rowgroups/columns/colgroups get painted
  // just because we're visible?  Or should it depend on the cell visibility
  // when we're not the whole table?
  const auto* effects = StyleEffects();
  if (effects->HasBoxShadowWithInset(true)) {
    aList->AppendNewToTop<nsDisplayBoxShadowInner>(aBuilder, this);
  }
}

void nsFrame::DisplayInsetBoxShadow(nsDisplayListBuilder* aBuilder,
                                    nsDisplayList* aList) {
  if (!IsVisibleForPainting()) return;

  DisplayInsetBoxShadowUnconditional(aBuilder, aList);
}

void nsFrame::DisplayOutsetBoxShadowUnconditional(
    nsDisplayListBuilder* aBuilder, nsDisplayList* aList) {
  // XXXbz should box-shadow for rows/rowgroups/columns/colgroups get painted
  // just because we're visible?  Or should it depend on the cell visibility
  // when we're not the whole table?
  const auto* effects = StyleEffects();
  if (effects->HasBoxShadowWithInset(false)) {
    aList->AppendNewToTop<nsDisplayBoxShadowOuter>(aBuilder, this);
  }
}

void nsFrame::DisplayOutsetBoxShadow(nsDisplayListBuilder* aBuilder,
                                     nsDisplayList* aList) {
  if (!IsVisibleForPainting()) return;

  DisplayOutsetBoxShadowUnconditional(aBuilder, aList);
}

void nsIFrame::DisplayCaret(nsDisplayListBuilder* aBuilder,
                            nsDisplayList* aList) {
  if (!IsVisibleForPainting()) return;

  aList->AppendNewToTop<nsDisplayCaret>(aBuilder, this);
}

nscolor nsIFrame::GetCaretColorAt(int32_t aOffset) {
  return nsLayoutUtils::GetColor(this, &nsStyleUI::mCaretColor);
}

bool nsFrame::DisplayBackgroundUnconditional(nsDisplayListBuilder* aBuilder,
                                             const nsDisplayListSet& aLists,
                                             bool aForceBackground) {
  const bool hitTesting = aBuilder->IsForEventDelivery();
  if (hitTesting && !aBuilder->HitTestIsForVisibility()) {
    // For hit-testing, we generally just need a light-weight data structure
    // like nsDisplayEventReceiver. But if the hit-testing is for visibility,
    // then we need to know the opaque region in order to determine whether to
    // stop or not.
    aLists.BorderBackground()->AppendNewToTop<nsDisplayEventReceiver>(aBuilder,
                                                                      this);
    return false;
  }

  // Here we don't try to detect background propagation. Frames that might
  // receive a propagated background should just set aForceBackground to
  // true.
  if (hitTesting || aForceBackground ||
      !StyleBackground()->IsTransparent(this) ||
      StyleDisplay()->HasAppearance()) {
    return nsDisplayBackgroundImage::AppendBackgroundItemsToTop(
        aBuilder, this,
        GetRectRelativeToSelf() + aBuilder->ToReferenceFrame(this),
        aLists.BorderBackground());
  }

  return false;
}

void nsFrame::DisplayBorderBackgroundOutline(nsDisplayListBuilder* aBuilder,
                                             const nsDisplayListSet& aLists,
                                             bool aForceBackground) {
  // The visibility check belongs here since child elements have the
  // opportunity to override the visibility property and display even if
  // their parent is hidden.
  if (!IsVisibleForPainting()) {
    return;
  }

  DisplayOutsetBoxShadowUnconditional(aBuilder, aLists.BorderBackground());

  bool bgIsThemed =
      DisplayBackgroundUnconditional(aBuilder, aLists, aForceBackground);

  DisplayInsetBoxShadowUnconditional(aBuilder, aLists.BorderBackground());

  // If there's a themed background, we should not create a border item.
  // It won't be rendered.
  // Don't paint borders for tables here, since they paint them in a different
  // order.
  if (!bgIsThemed && StyleBorder()->HasBorder() && !IsTableFrame()) {
    aLists.BorderBackground()->AppendNewToTop<nsDisplayBorder>(aBuilder, this);
  }

  DisplayOutlineUnconditional(aBuilder, aLists);
}

inline static bool IsSVGContentWithCSSClip(const nsIFrame* aFrame) {
  // The CSS spec says that the 'clip' property only applies to absolutely
  // positioned elements, whereas the SVG spec says that it applies to SVG
  // elements regardless of the value of the 'position' property. Here we obey
  // the CSS spec for outer-<svg> (since that's what we generally do), but
  // obey the SVG spec for other SVG elements to which 'clip' applies.
  return (aFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT) &&
         aFrame->GetContent()->IsAnyOfSVGElements(nsGkAtoms::svg,
                                                  nsGkAtoms::foreignObject);
}

bool nsIFrame::FormsBackdropRoot(const nsStyleDisplay* aStyleDisplay,
                                 const nsStyleEffects* aStyleEffects,
                                 const nsStyleSVGReset* aStyleSVGReset) {
  // Check if this is a root frame.
  if (!GetParent()) {
    return true;
  }

  // Check for filter effects.
  if (aStyleEffects->HasFilters() || aStyleEffects->HasBackdropFilters() ||
      aStyleEffects->HasMixBlendMode()) {
    return true;
  }

  // Check for opacity.
  if (HasOpacity(aStyleDisplay, aStyleEffects)) {
    return true;
  }

  // Check for mask or clip path.
  if (aStyleSVGReset->HasMask() || aStyleSVGReset->HasClipPath()) {
    return true;
  }

  // TODO(cbrewster): Check will-change attributes

  return false;
}

Maybe<nsRect> nsIFrame::GetClipPropClipRect(const nsStyleDisplay* aDisp,
                                            const nsStyleEffects* aEffects,
                                            const nsSize& aSize) const {
  if (aEffects->mClip.IsAuto() ||
      !(aDisp->IsAbsolutelyPositioned(this) || IsSVGContentWithCSSClip(this))) {
    return Nothing();
  }

  auto& clipRect = aEffects->mClip.AsRect();
  nsRect rect = clipRect.ToLayoutRect();
  if (MOZ_LIKELY(StyleBorder()->mBoxDecorationBreak ==
                 StyleBoxDecorationBreak::Slice)) {
    // The clip applies to the joined boxes so it's relative the first
    // continuation.
    nscoord y = 0;
    for (nsIFrame* f = GetPrevContinuation(); f; f = f->GetPrevContinuation()) {
      y += f->GetRect().height;
    }
    rect.MoveBy(nsPoint(0, -y));
  }

  if (clipRect.right.IsAuto()) {
    rect.width = aSize.width - rect.x;
  }
  if (clipRect.bottom.IsAuto()) {
    rect.height = aSize.height - rect.y;
  }
  return Some(rect);
}

/**
 * If the CSS 'overflow' property applies to this frame, and is not
 * handled by constructing a dedicated nsHTML/XULScrollFrame, set up clipping
 * for that overflow in aBuilder->ClipState() to clip all containing-block
 * descendants.
 *
 * Return true if clipping was applied.
 */
static void ApplyOverflowClipping(
    nsDisplayListBuilder* aBuilder, const nsIFrame* aFrame,
    DisplayListClipState::AutoClipMultiple& aClipState) {
  // Only -moz-hidden-unscrollable is handled here (and 'hidden' for table
  // frames, and any non-visible value for blocks in a paginated context).
  // We allow -moz-hidden-unscrollable to apply to any kind of frame. This
  // is required by comboboxes which make their display text (an inline frame)
  // have clipping.
  MOZ_ASSERT(
      nsFrame::ShouldApplyOverflowClipping(aFrame, aFrame->StyleDisplay()));

  nsRect clipRect;
  bool haveRadii = false;
  nscoord radii[8];
  auto* disp = aFrame->StyleDisplay();
  // Only deflate the padding if we clip to the content-box in that axis.
  auto wm = aFrame->GetWritingMode();
  bool cbH = (wm.IsVertical() ? disp->mOverflowClipBoxBlock
                              : disp->mOverflowClipBoxInline) ==
             StyleOverflowClipBox::ContentBox;
  bool cbV = (wm.IsVertical() ? disp->mOverflowClipBoxInline
                              : disp->mOverflowClipBoxBlock) ==
             StyleOverflowClipBox::ContentBox;
  nsMargin bp = aFrame->GetUsedPadding();
  if (!cbH) {
    bp.left = bp.right = nscoord(0);
  }
  if (!cbV) {
    bp.top = bp.bottom = nscoord(0);
  }

  bp += aFrame->GetUsedBorder();
  bp.ApplySkipSides(aFrame->GetSkipSides());
  nsRect rect(nsPoint(0, 0), aFrame->GetSize());
  rect.Deflate(bp);
  clipRect = rect + aBuilder->ToReferenceFrame(aFrame);
  haveRadii = aFrame->GetBoxBorderRadii(radii, bp, false);
  aClipState.ClipContainingBlockDescendantsExtra(clipRect,
                                                 haveRadii ? radii : nullptr);
}

#ifdef DEBUG
static void PaintDebugBorder(nsIFrame* aFrame, DrawTarget* aDrawTarget,
                             const nsRect& aDirtyRect, nsPoint aPt) {
  nsRect r(aPt, aFrame->GetSize());
  int32_t appUnitsPerDevPixel = aFrame->PresContext()->AppUnitsPerDevPixel();
  sRGBColor blueOrRed(aFrame->HasView() ? sRGBColor(0.f, 0.f, 1.f, 1.f)
                                        : sRGBColor(1.f, 0.f, 0.f, 1.f));
  aDrawTarget->StrokeRect(NSRectToRect(r, appUnitsPerDevPixel),
                          ColorPattern(ToDeviceColor(blueOrRed)));
}

static void PaintEventTargetBorder(nsIFrame* aFrame, DrawTarget* aDrawTarget,
                                   const nsRect& aDirtyRect, nsPoint aPt) {
  nsRect r(aPt, aFrame->GetSize());
  int32_t appUnitsPerDevPixel = aFrame->PresContext()->AppUnitsPerDevPixel();
  ColorPattern purple(ToDeviceColor(sRGBColor(.5f, 0.f, .5f, 1.f)));
  aDrawTarget->StrokeRect(NSRectToRect(r, appUnitsPerDevPixel), purple);
}

static void DisplayDebugBorders(nsDisplayListBuilder* aBuilder,
                                nsIFrame* aFrame,
                                const nsDisplayListSet& aLists) {
  // Draw a border around the child
  // REVIEW: From nsContainerFrame::PaintChild
  if (nsFrame::GetShowFrameBorders() && !aFrame->GetRect().IsEmpty()) {
    aLists.Outlines()->AppendNewToTop<nsDisplayGeneric>(
        aBuilder, aFrame, PaintDebugBorder, "DebugBorder",
        DisplayItemType::TYPE_DEBUG_BORDER);
  }
  // Draw a border around the current event target
  if (nsFrame::GetShowEventTargetFrameBorder() &&
      aFrame->PresShell()->GetDrawEventTargetFrame() == aFrame) {
    aLists.Outlines()->AppendNewToTop<nsDisplayGeneric>(
        aBuilder, aFrame, PaintEventTargetBorder, "EventTargetBorder",
        DisplayItemType::TYPE_EVENT_TARGET_BORDER);
  }
}
#endif

static bool IsScrollFrameActive(nsDisplayListBuilder* aBuilder,
                                nsIScrollableFrame* aScrollableFrame) {
  return aScrollableFrame && aScrollableFrame->IsScrollingActive(aBuilder);
}

/**
 * Returns whether a display item that gets created with the builder's current
 * state will have a scrolled clip, i.e. a clip that is scrolled by a scroll
 * frame which does not move the item itself.
 */
static bool BuilderHasScrolledClip(nsDisplayListBuilder* aBuilder) {
  const DisplayItemClipChain* currentClip =
      aBuilder->ClipState().GetCurrentCombinedClipChain(aBuilder);
  if (!currentClip) {
    return false;
  }

  const ActiveScrolledRoot* currentClipASR = currentClip->mASR;
  const ActiveScrolledRoot* currentASR = aBuilder->CurrentActiveScrolledRoot();
  return ActiveScrolledRoot::PickDescendant(currentClipASR, currentASR) !=
         currentASR;
}

class AutoSaveRestoreContainsBlendMode {
  nsDisplayListBuilder& mBuilder;
  bool mSavedContainsBlendMode;

 public:
  explicit AutoSaveRestoreContainsBlendMode(nsDisplayListBuilder& aBuilder)
      : mBuilder(aBuilder),
        mSavedContainsBlendMode(aBuilder.ContainsBlendMode()) {}

  ~AutoSaveRestoreContainsBlendMode() {
    mBuilder.SetContainsBlendMode(mSavedContainsBlendMode);
  }
};

class AutoSaveRestoreContainsBackdropFilter {
  nsDisplayListBuilder& mBuilder;
  bool mSavedContainsBackdropFilter;

 public:
  explicit AutoSaveRestoreContainsBackdropFilter(nsDisplayListBuilder& aBuilder)
      : mBuilder(aBuilder),
        mSavedContainsBackdropFilter(aBuilder.ContainsBackdropFilter()) {}

  /**
   * This is called if a stacking context which does not form a backdrop root
   * contains a descendent with a backdrop filter. In this case we need to
   * delegate backdrop root creation to the next parent in the tree until we hit
   * the nearest backdrop root ancestor.
   */
  void DelegateUp(bool aContainsBackdropFilter) {
    mSavedContainsBackdropFilter = aContainsBackdropFilter;
  }

  ~AutoSaveRestoreContainsBackdropFilter() {
    mBuilder.SetContainsBackdropFilter(mSavedContainsBackdropFilter);
  }
};

static void CheckForApzAwareEventHandlers(nsDisplayListBuilder* aBuilder,
                                          nsIFrame* aFrame) {
  if (aBuilder->GetAncestorHasApzAwareEventHandler()) {
    return;
  }

  nsIContent* content = aFrame->GetContent();
  if (!content) {
    return;
  }

  if (content->IsNodeApzAware()) {
    aBuilder->SetAncestorHasApzAwareEventHandler(true);
  }
}

/**
 * True if aDescendant participates the context aAncestor participating.
 */
static bool FrameParticipatesIn3DContext(nsIFrame* aAncestor,
                                         nsIFrame* aDescendant) {
  MOZ_ASSERT(aAncestor != aDescendant);
  MOZ_ASSERT(aAncestor->GetContent() != aDescendant->GetContent());
  MOZ_ASSERT(aAncestor->Extend3DContext());

  nsIFrame* ancestor = aAncestor->FirstContinuation();
  MOZ_ASSERT(ancestor->IsPrimaryFrame());

  nsIFrame* frame;
  for (frame = aDescendant->GetClosestFlattenedTreeAncestorPrimaryFrame();
       frame && ancestor != frame;
       frame = frame->GetClosestFlattenedTreeAncestorPrimaryFrame()) {
    if (!frame->Extend3DContext()) {
      return false;
    }
  }

  MOZ_ASSERT(frame == ancestor);
  return true;
}

static bool ItemParticipatesIn3DContext(nsIFrame* aAncestor,
                                        nsDisplayItem* aItem) {
  auto type = aItem->GetType();
  const bool isContainer = type == DisplayItemType::TYPE_WRAP_LIST ||
                           type == DisplayItemType::TYPE_CONTAINER;

  if (isContainer && aItem->GetChildren()->Count() == 1) {
    // If the wraplist has only one child item, use the type of that item.
    type = aItem->GetChildren()->GetBottom()->GetType();
  }

  if (type != DisplayItemType::TYPE_TRANSFORM &&
      type != DisplayItemType::TYPE_PERSPECTIVE) {
    return false;
  }
  nsIFrame* transformFrame = aItem->Frame();
  if (aAncestor->GetContent() == transformFrame->GetContent()) {
    return true;
  }
  return FrameParticipatesIn3DContext(aAncestor, transformFrame);
}

static void WrapSeparatorTransform(nsDisplayListBuilder* aBuilder,
                                   nsIFrame* aFrame,
                                   nsDisplayList* aNonParticipants,
                                   nsDisplayList* aParticipants, int aIndex,
                                   nsDisplayItem** aSeparator) {
  if (aNonParticipants->IsEmpty()) {
    return;
  }

  nsDisplayTransform* item = MakeDisplayItem<nsDisplayTransform>(
      aBuilder, aFrame, aNonParticipants, aBuilder->GetVisibleRect(), aIndex);

  if (*aSeparator == nullptr && item) {
    *aSeparator = item;
  }

  aParticipants->AppendToTop(item);
}

// Try to compute a clip rect to bound the contents of the mask item
// that will be built for |aMaskedFrame|. If we're not able to compute
// one, return an empty Maybe.
// The returned clip rect, if there is one, is relative to |aMaskedFrame|.
static Maybe<nsRect> ComputeClipForMaskItem(nsDisplayListBuilder* aBuilder,
                                            nsIFrame* aMaskedFrame) {
  const nsStyleSVGReset* svgReset = aMaskedFrame->StyleSVGReset();

  nsSVGUtils::MaskUsage maskUsage;
  nsSVGUtils::DetermineMaskUsage(aMaskedFrame, false, maskUsage);

  nsPoint offsetToUserSpace =
      nsLayoutUtils::ComputeOffsetToUserSpace(aBuilder, aMaskedFrame);
  int32_t devPixelRatio = aMaskedFrame->PresContext()->AppUnitsPerDevPixel();
  gfxPoint devPixelOffsetToUserSpace =
      nsLayoutUtils::PointToGfxPoint(offsetToUserSpace, devPixelRatio);
  gfxMatrix cssToDevMatrix = nsSVGUtils::GetCSSPxToDevPxMatrix(aMaskedFrame);

  nsPoint toReferenceFrame;
  aBuilder->FindReferenceFrameFor(aMaskedFrame, &toReferenceFrame);

  Maybe<gfxRect> combinedClip;
  if (maskUsage.shouldApplyBasicShapeOrPath) {
    Maybe<Rect> result =
        nsCSSClipPathInstance::GetBoundingRectForBasicShapeOrPathClip(
            aMaskedFrame, svgReset->mClipPath);
    if (result) {
      combinedClip = Some(ThebesRect(*result));
    }
  } else if (maskUsage.shouldApplyClipPath) {
    gfxRect result = nsSVGUtils::GetBBox(
        aMaskedFrame,
        nsSVGUtils::eBBoxIncludeClipped | nsSVGUtils::eBBoxIncludeFill |
            nsSVGUtils::eBBoxIncludeMarkers | nsSVGUtils::eBBoxIncludeStroke |
            nsSVGUtils::eDoNotClipToBBoxOfContentInsideClipPath);
    combinedClip = Some(cssToDevMatrix.TransformBounds(result));
  } else {
    // The code for this case is adapted from ComputeMaskGeometry().

    nsRect borderArea(toReferenceFrame, aMaskedFrame->GetSize());
    borderArea -= offsetToUserSpace;

    // Use an infinite dirty rect to pass into nsCSSRendering::
    // GetImageLayerClip() because we don't have an actual dirty rect to
    // pass in. This is fine because the only time GetImageLayerClip() will
    // not intersect the incoming dirty rect with something is in the "NoClip"
    // case, and we handle that specially.
    nsRect dirtyRect(nscoord_MIN / 2, nscoord_MIN / 2, nscoord_MAX,
                     nscoord_MAX);

    nsIFrame* firstFrame =
        nsLayoutUtils::FirstContinuationOrIBSplitSibling(aMaskedFrame);
    nsTArray<nsSVGMaskFrame*> maskFrames;
    // XXX check return value?
    SVGObserverUtils::GetAndObserveMasks(firstFrame, &maskFrames);

    for (uint32_t i = 0; i < maskFrames.Length(); ++i) {
      gfxRect clipArea;
      if (maskFrames[i]) {
        clipArea = maskFrames[i]->GetMaskArea(aMaskedFrame);
        clipArea = cssToDevMatrix.TransformBounds(clipArea);
      } else {
        const auto& layer = svgReset->mMask.mLayers[i];
        if (layer.mClip == StyleGeometryBox::NoClip) {
          return Nothing();
        }

        nsCSSRendering::ImageLayerClipState clipState;
        nsCSSRendering::GetImageLayerClip(
            layer, aMaskedFrame, *aMaskedFrame->StyleBorder(), borderArea,
            dirtyRect, false /* aWillPaintBorder */, devPixelRatio, &clipState);
        clipArea = clipState.mDirtyRectInDevPx;
      }
      combinedClip = UnionMaybeRects(combinedClip, Some(clipArea));
    }
  }
  if (combinedClip) {
    if (combinedClip->IsEmpty()) {
      // *clipForMask might be empty if all mask references are not resolvable
      // or the size of them are empty. We still need to create a transparent
      // mask before bug 1276834 fixed, so don't clip ctx by an empty rectangle
      // for for now.
      return Nothing();
    }

    // Convert to user space.
    *combinedClip += devPixelOffsetToUserSpace;

    // Round the clip out. In FrameLayerBuilder we round clips to nearest
    // pixels, and if we have a really thin clip here, that can cause the
    // clip to become empty if we didn't round out here.
    // The rounding happens in coordinates that are relative to the reference
    // frame, which matches what FrameLayerBuilder does.
    combinedClip->RoundOut();

    // Convert to app units.
    nsRect result =
        nsLayoutUtils::RoundGfxRectToAppRect(*combinedClip, devPixelRatio);

    // The resulting clip is relative to the reference frame, but the caller
    // expects it to be relative to the masked frame, so adjust it.
    result -= toReferenceFrame;
    return Some(result);
  }
  return Nothing();
}

struct AutoCheckBuilder {
  explicit AutoCheckBuilder(nsDisplayListBuilder* aBuilder)
      : mBuilder(aBuilder) {
    aBuilder->Check();
  }

  ~AutoCheckBuilder() { mBuilder->Check(); }

  nsDisplayListBuilder* mBuilder;
};

/**
 * Helper class to track container creation. Stores the first tracked container.
 * Used to find the innermost container for hit test information, and to notify
 * callers whether a container item was created or not.
 */
struct ContainerTracker {
  void TrackContainer(nsDisplayItem* aContainer) {
    if (!aContainer) {
      return;
    }

    if (!mContainer) {
      mContainer = aContainer;
    }

    mCreatedContainer = true;
  }

  void ResetCreatedContainer() { mCreatedContainer = false; }

  nsDisplayItem* mContainer = nullptr;
  bool mCreatedContainer = false;
};

/**
 * Adds hit test information |aHitTestInfo| on the container item |aContainer|,
 * or if the container item is null, creates a separate hit test item that is
 * added to the bottom of the display list |aList|.
 */
static void AddHitTestInfo(nsDisplayListBuilder* aBuilder, nsDisplayList* aList,
                           nsDisplayItem* aContainer, nsIFrame* aFrame,
                           mozilla::UniquePtr<HitTestInfo>&& aHitTestInfo) {
  nsDisplayHitTestInfoBase* hitTestItem;

  if (aContainer) {
    MOZ_ASSERT(aContainer->IsHitTestItem());
    hitTestItem = static_cast<nsDisplayHitTestInfoBase*>(aContainer);
    hitTestItem->SetHitTestInfo(std::move(aHitTestInfo));
  } else {
    // No container item was created for this frame. Create a separate
    // nsDisplayCompositorHitTestInfo item instead.
    aList->AppendNewToBottom<nsDisplayCompositorHitTestInfo>(
        aBuilder, aFrame, std::move(aHitTestInfo));
  }
}

void nsIFrame::BuildDisplayListForStackingContext(
    nsDisplayListBuilder* aBuilder, nsDisplayList* aList,
    bool* aCreatedContainerItem) {
  AutoCheckBuilder check(aBuilder);
  if (GetStateBits() & NS_FRAME_TOO_DEEP_IN_FRAME_TREE) return;

  // Replaced elements have their visibility handled here, because
  // they're visually atomic
  if (IsFrameOfType(eReplaced) && !IsVisibleForPainting()) return;

  const nsStyleDisplay* disp = StyleDisplay();
  const nsStyleEffects* effects = StyleEffects();
  EffectSet* effectSetForOpacity = EffectSet::GetEffectSetForFrame(
      this, nsCSSPropertyIDSet::OpacityProperties());
  // We can stop right away if this is a zero-opacity stacking context and
  // we're painting, and we're not animating opacity. Don't do this
  // if we're going to compute plugin geometry, since opacity-0 plugins
  // need to have display items built for them.
  bool needHitTestInfo =
      aBuilder->BuildCompositorHitTestInfo() &&
      StyleUI()->GetEffectivePointerEvents(this) != StylePointerEvents::None;
  bool opacityItemForEventsAndPluginsOnly = false;
  if (effects->mOpacity == 0.0 && aBuilder->IsForPainting() &&
      !(disp->mWillChange.bits & StyleWillChangeBits::OPACITY) &&
      !nsLayoutUtils::HasAnimationOfPropertySet(
          this, nsCSSPropertyIDSet::OpacityProperties(), effectSetForOpacity)) {
    if (needHitTestInfo || aBuilder->WillComputePluginGeometry()) {
      opacityItemForEventsAndPluginsOnly = true;
    } else {
      return;
    }
  }

  if (aBuilder->IsForPainting() && disp->mWillChange.bits) {
    aBuilder->AddToWillChangeBudget(this, GetSize());
  }

  // For preserves3d, use the dirty rect already installed on the
  // builder, since aDirtyRect maybe distorted for transforms along
  // the chain.
  nsRect visibleRect = aBuilder->GetVisibleRect();
  nsRect dirtyRect = aBuilder->GetDirtyRect();

  // We build an opacity item if it's not going to be drawn by SVG content.
  // We could in principle skip creating an nsDisplayOpacity item if
  // nsDisplayOpacity::NeedsActiveLayer returns false and usingSVGEffects is
  // true (the nsDisplayFilter/nsDisplayMasksAndClipPaths could handle the
  // opacity). Since SVG has perf issues where we sometimes spend a lot of
  // time creating display list items that might be helpful.  We'd need to
  // restore our mechanism to do that (changed in bug 1482403), and we'd
  // need to invalidate the frame if the value that would be return from
  // NeedsActiveLayer was to change, which we don't currently do.
  const bool useOpacity =
      HasVisualOpacity(disp, effects, effectSetForOpacity) &&
      !nsSVGUtils::CanOptimizeOpacity(this);

  const bool isTransformed = IsTransformed(disp);
  const bool hasPerspective = isTransformed && HasPerspective(disp);
  const bool extend3DContext =
      Extend3DContext(disp, effects, effectSetForOpacity);
  const bool combines3DTransformWithAncestors =
      (extend3DContext || isTransformed) &&
      Combines3DTransformWithAncestors(disp);

  Maybe<nsDisplayListBuilder::AutoPreserves3DContext> autoPreserves3DContext;
  if (extend3DContext && !combines3DTransformWithAncestors) {
    // Start a new preserves3d context to keep informations on
    // nsDisplayListBuilder.
    autoPreserves3DContext.emplace(aBuilder);
    // Save dirty rect on the builder to avoid being distorted for
    // multiple transforms along the chain.
    aBuilder->SavePreserves3DRect();

    // We rebuild everything within preserve-3d and don't try
    // to retain, so override the dirty rect now.
    if (aBuilder->IsRetainingDisplayList()) {
      dirtyRect = visibleRect;
      aBuilder->SetDisablePartialUpdates(true);
    }
  }

  const bool useBlendMode = effects->mMixBlendMode != StyleBlend::Normal;
  if (useBlendMode) {
    aBuilder->SetContainsBlendMode(true);
  }

  // reset blend mode so we can keep track if this stacking context needs have
  // a nsDisplayBlendContainer. Set the blend mode back when the routine exits
  // so we keep track if the parent stacking context needs a container too.
  AutoSaveRestoreContainsBlendMode autoRestoreBlendMode(*aBuilder);
  aBuilder->SetContainsBlendMode(false);

  bool backdropFilterEnabled =
      StaticPrefs::layout_css_backdrop_filter_enabled();
  bool usingBackdropFilter =
      backdropFilterEnabled && effects->HasBackdropFilters() &&
      nsDisplayBackdropFilters::CanCreateWebRenderCommands(aBuilder, this);

  if (usingBackdropFilter) {
    aBuilder->SetContainsBackdropFilter(true);
  }

  AutoSaveRestoreContainsBackdropFilter autoRestoreBackdropFilter(*aBuilder);
  aBuilder->SetContainsBackdropFilter(false);

  nsRect visibleRectOutsideTransform = visibleRect;
  bool allowAsyncAnimation = false;
  bool inTransform = aBuilder->IsInTransform();
  if (isTransformed) {
    nsDisplayTransform::PrerenderInfo decision =
        nsDisplayTransform::ShouldPrerenderTransformedContent(aBuilder, this,
                                                              &dirtyRect);

    switch (decision.mDecision) {
      case nsDisplayTransform::PrerenderDecision::Full:
        allowAsyncAnimation = true;
        visibleRect = dirtyRect;
        break;
      case nsDisplayTransform::PrerenderDecision::Partial:
        allowAsyncAnimation = true;
        visibleRect = dirtyRect;
        [[fallthrough]];
        // fall through to the PrerenderDecision::No case
      case nsDisplayTransform::PrerenderDecision::No: {
        // If we didn't prerender an animated frame in a preserve-3d context,
        // then we want disable async animations for the rest of the preserve-3d
        // (especially ancestors).
        if ((extend3DContext || combines3DTransformWithAncestors) &&
            decision.mDecision == nsDisplayTransform::PrerenderDecision::No &&
            decision.mHasAnimations) {
          aBuilder->SavePreserves3DAllowAsyncAnimation(false);
        }

        const nsRect overflow = GetVisualOverflowRectRelativeToSelf();
        if (overflow.IsEmpty() && !extend3DContext) {
          return;
        }

        // If we're in preserve-3d then grab the dirty rect that was given to
        // the root and transform using the combined transform.
        if (combines3DTransformWithAncestors) {
          visibleRect = dirtyRect = aBuilder->GetPreserves3DRect();
        }

        nsRect untransformedDirtyRect;
        if (nsDisplayTransform::UntransformRect(dirtyRect, overflow, this,
                                                &untransformedDirtyRect)) {
          dirtyRect = untransformedDirtyRect;
          nsDisplayTransform::UntransformRect(visibleRect, overflow, this,
                                              &visibleRect);
        } else {
          // This should only happen if the transform is singular, in which case
          // nothing is visible anyway
          dirtyRect.SetEmpty();
          visibleRect.SetEmpty();
        }
      }
    }
    inTransform = true;
  } else if (IsFixedPosContainingBlock()) {
    // Restict the building area to the overflow rect for these frames, since
    // RetainedDisplayListBuilder uses it to know if the size of the stacking
    // context changed.
    visibleRect.IntersectRect(visibleRect, GetVisualOverflowRect());
    dirtyRect.IntersectRect(dirtyRect, GetVisualOverflowRect());
  }

  bool hasOverrideDirtyRect = false;
  // If we're doing a partial build, we're not invalid and we're capable
  // of having an override building rect (stacking context and fixed pos
  // containing block), then we should assume we have one.
  // Either we have an explicit one, or nothing in our subtree changed and
  // we have an implicit empty rect.
  if (aBuilder->IsPartialUpdate() && !aBuilder->InInvalidSubtree() &&
      !IsFrameModified() && IsFixedPosContainingBlock()) {
    dirtyRect = nsRect();
    if (HasOverrideDirtyRegion()) {
      nsDisplayListBuilder::DisplayListBuildingData* data =
          GetProperty(nsDisplayListBuilder::DisplayListBuildingRect());
      if (data) {
        dirtyRect = data->mDirtyRect.Intersect(visibleRect);
        hasOverrideDirtyRect = true;
      }
    }
  }

  bool usingFilter = effects->HasFilters();
  bool usingMask = nsSVGIntegrationUtils::UsingMaskOrClipPathForFrame(this);
  bool usingSVGEffects = usingFilter || usingMask;

  nsRect visibleRectOutsideSVGEffects = visibleRect;
  nsDisplayList hoistedScrollInfoItemsStorage;
  if (usingSVGEffects) {
    dirtyRect =
        nsSVGIntegrationUtils::GetRequiredSourceForInvalidArea(this, dirtyRect);
    visibleRect = nsSVGIntegrationUtils::GetRequiredSourceForInvalidArea(
        this, visibleRect);
    aBuilder->EnterSVGEffectsContents(this, &hoistedScrollInfoItemsStorage);
  }

  bool useStickyPosition =
      disp->mPosition == StylePositionProperty::Sticky &&
      IsScrollFrameActive(
          aBuilder,
          nsLayoutUtils::GetNearestScrollableFrame(
              GetParent(), nsLayoutUtils::SCROLLABLE_SAME_DOC |
                               nsLayoutUtils::SCROLLABLE_INCLUDE_HIDDEN));
  bool useFixedPosition = disp->mPosition == StylePositionProperty::Fixed &&
                          (nsLayoutUtils::IsFixedPosFrameInDisplayPort(this) ||
                           BuilderHasScrolledClip(aBuilder));

  nsDisplayListBuilder::AutoBuildingDisplayList buildingDisplayList(
      aBuilder, this, visibleRect, dirtyRect, isTransformed);

  // Depending on the effects that are applied to this frame, we can create
  // multiple container display items and wrap them around our contents.
  // This enum lists all the potential container display items, in the order
  // outside to inside.
  enum class ContainerItemType : uint8_t {
    None = 0,
    OwnLayerIfNeeded,
    BlendMode,
    FixedPosition,
    OwnLayerForTransformWithRoundedClip,
    Perspective,
    Transform,
    SeparatorTransforms,
    Opacity,
    Filter,
    BlendContainer
  };

  nsDisplayListBuilder::AutoContainerASRTracker contASRTracker(aBuilder);

  auto cssClip = GetClipPropClipRect(disp, effects, GetSize());
  auto ApplyClipProp = [&](DisplayListClipState::AutoSaveRestore& aClipState) {
    if (!cssClip) {
      return;
    }
    nsPoint offset = aBuilder->GetCurrentFrameOffsetToReferenceFrame();
    aBuilder->IntersectDirtyRect(*cssClip);
    aBuilder->IntersectVisibleRect(*cssClip);
    aClipState.ClipContentDescendants(*cssClip + offset);
  };

  // The CSS clip property is effectively inside the transform, but outside the
  // filters. So if we're not transformed we can apply it just here for
  // simplicity, instead of on each of the places that handle clipCapturedBy.
  DisplayListClipState::AutoSaveRestore untransformedCssClip(aBuilder);
  if (!isTransformed) {
    ApplyClipProp(untransformedCssClip);
  }

  // If there is a current clip, then depending on the container items we
  // create, different things can happen to it. Some container items simply
  // propagate the clip to their children and aren't clipped themselves.
  // But other container items, especially those that establish a different
  // geometry for their contents (e.g. transforms), capture the clip on
  // themselves and unset the clip for their contents. If we create more than
  // one of those container items, the clip will be captured on the outermost
  // one and the inner container items will be unclipped.
  ContainerItemType clipCapturedBy = ContainerItemType::None;
  if (useFixedPosition) {
    clipCapturedBy = ContainerItemType::FixedPosition;
  } else if (isTransformed) {
    const DisplayItemClipChain* currentClip =
        aBuilder->ClipState().GetCurrentCombinedClipChain(aBuilder);
    if ((hasPerspective || extend3DContext) &&
        (currentClip && currentClip->HasRoundedCorners())) {
      // If we're creating an nsDisplayTransform item that is going to combine
      // its transform with its children (preserve-3d or perspective), then we
      // can't have an intermediate surface. Mask layers force an intermediate
      // surface, so if we're going to need both then create a separate
      // wrapping layer for the mask.
      clipCapturedBy = ContainerItemType::OwnLayerForTransformWithRoundedClip;
    } else if (hasPerspective) {
      clipCapturedBy = ContainerItemType::Perspective;
    } else {
      clipCapturedBy = ContainerItemType::Transform;
    }
  } else if (usingFilter) {
    clipCapturedBy = ContainerItemType::Filter;
  }

  DisplayListClipState::AutoSaveRestore clipState(aBuilder);
  if (clipCapturedBy != ContainerItemType::None) {
    clipState.Clear();
  }

  DisplayListClipState::AutoSaveRestore transformedCssClip(aBuilder);
  if (isTransformed) {
    // FIXME(emilio, bug 1525159): In the case we have a both a transform _and_
    // filters, this clips the input to the filters as well, which is not
    // correct (clipping by the `clip` property is supposed to happen after
    // applying the filter effects, per [1].
    //
    // This is not a regression though, since we used to do that anyway before
    // bug 1514384, and even without the transform we get it wrong.
    //
    // [1]: https://drafts.fxtf.org/css-masking/#placement
    ApplyClipProp(transformedCssClip);
  }

  mozilla::UniquePtr<HitTestInfo> hitTestInfo;

  nsDisplayListCollection set(aBuilder);
  Maybe<nsRect> clipForMask;
  bool insertBackdropRoot;
  {
    DisplayListClipState::AutoSaveRestore nestedClipState(aBuilder);
    nsDisplayListBuilder::AutoInTransformSetter inTransformSetter(aBuilder,
                                                                  inTransform);
    nsDisplayListBuilder::AutoEnterFilter filterASRSetter(aBuilder,
                                                          usingFilter);
    nsDisplayListBuilder::AutoInEventsAndPluginsOnly inEventsAndPluginsSetter(
        aBuilder, opacityItemForEventsAndPluginsOnly);

    CheckForApzAwareEventHandlers(aBuilder, this);

    if (usingMask) {
      clipForMask = ComputeClipForMaskItem(aBuilder, this);
      if (clipForMask) {
        aBuilder->IntersectDirtyRect(*clipForMask);
        aBuilder->IntersectVisibleRect(*clipForMask);
        nestedClipState.ClipContentDescendants(
            *clipForMask + aBuilder->GetCurrentFrameOffsetToReferenceFrame());
      }
    }

    // extend3DContext also guarantees that applyAbsPosClipping and
    // usingSVGEffects are false We only modify the preserve-3d rect if we are
    // the top of a preserve-3d heirarchy
    if (extend3DContext) {
      // Mark these first so MarkAbsoluteFramesForDisplayList knows if we are
      // going to be forced to descend into frames.
      aBuilder->MarkPreserve3DFramesForDisplayList(this);
    }

    aBuilder->AdjustWindowDraggingRegion(this);

    if (gfxVars::UseWebRender()) {
      aBuilder->BuildCompositorHitTestInfoIfNeeded(this, set.BorderBackground(),
                                                   true);
    } else {
      CompositorHitTestInfo info = aBuilder->BuildCompositorHitTestInfo()
                                       ? GetCompositorHitTestInfo(aBuilder)
                                       : CompositorHitTestInvisibleToHit;

      if (info != CompositorHitTestInvisibleToHit) {
        // Frame has hit test flags set, initialize the hit test info structure.
        hitTestInfo = mozilla::MakeUnique<HitTestInfo>(aBuilder, this, info);

        // Let child frames know the current hit test area and hit test flags.
        aBuilder->SetCompositorHitTestInfo(hitTestInfo->mArea,
                                           hitTestInfo->mFlags);
      }
    }

    MarkAbsoluteFramesForDisplayList(aBuilder);
    aBuilder->Check();
    BuildDisplayList(aBuilder, set);
    aBuilder->Check();
    aBuilder->DisplayCaret(this, set.Outlines());

    insertBackdropRoot = backdropFilterEnabled &&
                         aBuilder->ContainsBackdropFilter() &&
                         FormsBackdropRoot(disp, effects, StyleSVGReset());

    // Blend modes are a real pain for retained display lists. We build a blend
    // container item if the built list contains any blend mode items within
    // the current stacking context. This can change without an invalidation
    // to the stacking context frame, or the blend mode frame (e.g. by moving
    // an intermediate frame).
    // When we gain/remove a blend container item, we need to mark this frame
    // as invalid and have the full display list for merging to track
    // the change correctly.
    // It seems really hard to track this in advance, as the bookkeeping
    // required to note which stacking contexts have blend descendants
    // is complex and likely to be buggy.
    // Instead we're doing the sad thing, detecting it afterwards, and just
    // repeating display list building if it changed.
    // We have to repeat building for the entire display list (or at least
    // the outer stacking context), since we need to mark this frame as invalid
    // to remove any existing content that isn't wrapped in the blend container,
    // and then we need to build content infront/behind the blend container
    // to get correct positioning during merging.
    if ((insertBackdropRoot || aBuilder->ContainsBlendMode()) &&
        aBuilder->IsRetainingDisplayList()) {
      if (aBuilder->IsPartialUpdate()) {
        aBuilder->SetPartialBuildFailed(true);
      } else {
        aBuilder->SetDisablePartialUpdates(true);
      }
    }
  }

  // If a child contains a backdrop filter, but this stacking context does not
  // form a backdrop root, we need to propogate up the tree until we find an
  // ancestor that does form a backdrop root.
  if (!insertBackdropRoot && aBuilder->ContainsBackdropFilter()) {
    autoRestoreBackdropFilter.DelegateUp(true);
  }

  if (aBuilder->IsBackgroundOnly()) {
    set.BlockBorderBackgrounds()->DeleteAll(aBuilder);
    set.Floats()->DeleteAll(aBuilder);
    set.Content()->DeleteAll(aBuilder);
    set.PositionedDescendants()->DeleteAll(aBuilder);
    set.Outlines()->DeleteAll(aBuilder);
  }

  if (hasOverrideDirtyRect &&
      StaticPrefs::layout_display_list_show_rebuild_area()) {
    nsDisplaySolidColor* color = MakeDisplayItem<nsDisplaySolidColor>(
        aBuilder, this,
        dirtyRect + aBuilder->GetCurrentFrameOffsetToReferenceFrame(),
        NS_RGBA(255, 0, 0, 64), false);
    if (color) {
      color->SetOverrideZIndex(INT32_MAX);
      set.PositionedDescendants()->AppendToTop(color);
    }
  }

  nsIContent* content = GetContent();
  if (!content) {
    content = PresContext()->Document()->GetRootElement();
  }

  nsDisplayList resultList;
  set.SerializeWithCorrectZOrder(&resultList, content);

#ifdef DEBUG
  DisplayDebugBorders(aBuilder, this, set);
#endif

  // Get the ASR to use for the container items that we create here.
  const ActiveScrolledRoot* containerItemASR = contASRTracker.GetContainerASR();

  ContainerTracker ct;

  /* If adding both a nsDisplayBlendContainer and a nsDisplayBlendMode to the
   * same list, the nsDisplayBlendContainer should be added first. This only
   * happens when the element creating this stacking context has mix-blend-mode
   * and also contains a child which has mix-blend-mode.
   * The nsDisplayBlendContainer must be added to the list first, so it does not
   * isolate the containing element blending as well.
   */
  if (aBuilder->ContainsBlendMode()) {
    DisplayListClipState::AutoSaveRestore blendContainerClipState(aBuilder);
    resultList.AppendToTop(nsDisplayBlendContainer::CreateForMixBlendMode(
        aBuilder, this, &resultList, containerItemASR));
    ct.TrackContainer(resultList.GetTop());
  }

  if (insertBackdropRoot) {
    DisplayListClipState::AutoSaveRestore backdropRootContainerClipState(
        aBuilder);
    resultList.AppendNewToTop<nsDisplayBackdropRootContainer>(
        aBuilder, this, &resultList, containerItemASR);
    ct.TrackContainer(resultList.GetTop());
  }

  if (usingBackdropFilter) {
    DisplayListClipState::AutoSaveRestore clipState(aBuilder);
    nsRect backdropRect =
        GetRectRelativeToSelf() + aBuilder->ToReferenceFrame(this);
    resultList.AppendNewToTop<nsDisplayBackdropFilters>(
        aBuilder, this, &resultList, backdropRect);
    ct.TrackContainer(resultList.GetTop());
  }

  /* If there are any SVG effects, wrap the list up in an SVG effects item
   * (which also handles CSS group opacity). Note that we create an SVG effects
   * item even if resultList is empty, since a filter can produce graphical
   * output even if the element being filtered wouldn't otherwise do so.
   */
  if (usingSVGEffects) {
    MOZ_ASSERT(usingFilter || usingMask,
               "Beside filter & mask/clip-path, what else effect do we have?");

    if (clipCapturedBy == ContainerItemType::Filter) {
      clipState.Restore();
    }
    // Revert to the post-filter dirty rect.
    aBuilder->SetVisibleRect(visibleRectOutsideSVGEffects);

    // Skip all filter effects while generating glyph mask.
    if (usingFilter && !aBuilder->IsForGenerateGlyphMask()) {
      /* List now emptied, so add the new list to the top. */
      resultList.AppendNewToTop<nsDisplayFilters>(aBuilder, this, &resultList);
      ct.TrackContainer(resultList.GetTop());
    }

    if (usingMask) {
      DisplayListClipState::AutoSaveRestore maskClipState(aBuilder);
      // The mask should move with aBuilder->CurrentActiveScrolledRoot(), so
      // that's the ASR we prefer to use for the mask item. However, we can
      // only do this if the mask if clipped with respect to that ASR, because
      // an item always needs to have finite bounds with respect to its ASR.
      // If we weren't able to compute a clip for the mask, we fall back to
      // using containerItemASR, which is the lowest common ancestor clip of
      // the mask's contents. That's not entirely correct, but it satisfies
      // the base requirement of the ASR system (that items have finite bounds
      // wrt. their ASR).
      const ActiveScrolledRoot* maskASR =
          clipForMask.isSome() ? aBuilder->CurrentActiveScrolledRoot()
                               : containerItemASR;
      /* List now emptied, so add the new list to the top. */
      resultList.AppendNewToTop<nsDisplayMasksAndClipPaths>(
          aBuilder, this, &resultList, maskASR);
      ct.TrackContainer(resultList.GetTop());
    }

    // TODO(miko): We could probably create a wraplist here and avoid creating
    // it later in |BuildDisplayListForChild()|.
    ct.ResetCreatedContainer();

    // Also add the hoisted scroll info items. We need those for APZ scrolling
    // because nsDisplayMasksAndClipPaths items can't build active layers.
    aBuilder->ExitSVGEffectsContents();
    resultList.AppendToTop(&hoistedScrollInfoItemsStorage);
  }

  /* If the list is non-empty and there is CSS group opacity without SVG
   * effects, wrap it up in an opacity item.
   */
  if (useOpacity) {
    // Don't clip nsDisplayOpacity items. We clip their descendants instead.
    // The clip we would set on an element with opacity would clip
    // all descendant content, but some should not be clipped.
    DisplayListClipState::AutoSaveRestore opacityClipState(aBuilder);
    const bool needsActiveOpacityLayer =
        nsDisplayOpacity::NeedsActiveLayer(aBuilder, this);

    resultList.AppendNewToTop<nsDisplayOpacity>(
        aBuilder, this, &resultList, containerItemASR,
        opacityItemForEventsAndPluginsOnly, needsActiveOpacityLayer);
    ct.TrackContainer(resultList.GetTop());
  }

  /* If we're going to apply a transformation and don't have preserve-3d set,
   * wrap everything in an nsDisplayTransform. If there's nothing in the list,
   * don't add anything.
   *
   * For the preserve-3d case we want to individually wrap every child in the
   * list with a separate nsDisplayTransform instead. When the child is already
   * an nsDisplayTransform, we can skip this step, as the computed transform
   * will already include our own.
   *
   * We also traverse into sublists created by nsDisplayWrapList, so that we
   * find all the correct children.
   */
  if (isTransformed && extend3DContext) {
    // Install dummy nsDisplayTransform as a leaf containing
    // descendants not participating this 3D rendering context.
    nsDisplayList nonparticipants;
    nsDisplayList participants;
    int index = 1;

    nsDisplayItem* separator = nullptr;

    while (nsDisplayItem* item = resultList.RemoveBottom()) {
      if (ItemParticipatesIn3DContext(this, item) &&
          !item->GetClip().HasClip()) {
        // The frame of this item participates the same 3D context.
        WrapSeparatorTransform(aBuilder, this, &nonparticipants, &participants,
                               index++, &separator);

        participants.AppendToTop(item);
      } else {
        // The frame of the item doesn't participate the current
        // context, or has no transform.
        //
        // For items participating but not transformed, they are add
        // to nonparticipants to get a separator layer for handling
        // clips, if there is, on an intermediate surface.
        // \see ContainerLayer::DefaultComputeEffectiveTransforms().
        nonparticipants.AppendToTop(item);
      }
    }
    WrapSeparatorTransform(aBuilder, this, &nonparticipants, &participants,
                           index++, &separator);

    if (separator) {
      ct.TrackContainer(separator);
    }

    resultList.AppendToTop(&participants);
  }

  if (isTransformed) {
    transformedCssClip.Restore();
    if (clipCapturedBy == ContainerItemType::Transform) {
      // Restore clip state now so nsDisplayTransform is clipped properly.
      clipState.Restore();
    }
    // Revert to the dirtyrect coming in from the parent, without our transform
    // taken into account.
    aBuilder->SetVisibleRect(visibleRectOutsideTransform);
    // Revert to the outer reference frame and offset because all display
    // items we create from now on are outside the transform.
    nsPoint toOuterReferenceFrame;
    const nsIFrame* outerReferenceFrame = this;
    if (this != aBuilder->RootReferenceFrame()) {
      outerReferenceFrame =
          aBuilder->FindReferenceFrameFor(GetParent(), &toOuterReferenceFrame);
    }
    buildingDisplayList.SetReferenceFrameAndCurrentOffset(
        outerReferenceFrame, GetOffsetToCrossDoc(outerReferenceFrame));

    // We would like to block async animations for ancestors of ones not
    // prerendered in the preserve-3d tree. Now that we've finished processing
    // all descendants, update allowAsyncAnimation to take their prerender
    // state into account
    // FIXME: We don't block async animations for previous siblings because
    // their prerender decisions have been made. We may have to figure out a
    // better way to rollback their prerender decisions.
    // Alternatively we could not block animations for later siblings, and only
    // block them for ancestors of a blocked one.
    if ((extend3DContext || combines3DTransformWithAncestors) &&
        allowAsyncAnimation) {
      // aBuilder->GetPreserves3DAllowAsyncAnimation() means the inner or
      // previous silbing frames are allowed/disallowed for async animations.
      allowAsyncAnimation = aBuilder->GetPreserves3DAllowAsyncAnimation();
    }

    nsDisplayTransform* transformItem = MakeDisplayItem<nsDisplayTransform>(
        aBuilder, this, &resultList, visibleRect, 0, allowAsyncAnimation);
    if (transformItem) {
      resultList.AppendToTop(transformItem);
      ct.TrackContainer(transformItem);
    }

    if (hasPerspective) {
      if (clipCapturedBy == ContainerItemType::Perspective) {
        clipState.Restore();
      }
      resultList.AppendNewToTop<nsDisplayPerspective>(aBuilder, this,
                                                      &resultList);
      ct.TrackContainer(resultList.GetTop());
    }
  }

  if (clipCapturedBy ==
      ContainerItemType::OwnLayerForTransformWithRoundedClip) {
    clipState.Restore();
    resultList.AppendNewToTop<nsDisplayOwnLayer>(
        aBuilder, this, &resultList, aBuilder->CurrentActiveScrolledRoot(),
        nsDisplayOwnLayerFlags::None, ScrollbarData{},
        /* aForceActive = */ false, false,
        nsDisplayOwnLayer::OwnLayerForTransformWithRoundedClip);
    ct.TrackContainer(resultList.GetTop());
  }

  /* If we have sticky positioning, wrap it in a sticky position item.
   */
  if (useFixedPosition) {
    if (clipCapturedBy == ContainerItemType::FixedPosition) {
      clipState.Restore();
    }
    // The ASR for the fixed item should be the ASR of our containing block,
    // which has been set as the builder's current ASR, unless this frame is
    // invisible and we hadn't saved display item data for it. In that case,
    // we need to take the containerItemASR since we might have fixed children.
    // For WebRender, we want to the know what |containerItemASR| is for the
    // case where the fixed-pos item is not a "real" fixed-pos item (e.g. it's
    // nested inside a scrolling transform), so we stash that on the display
    // item as well.
    const ActiveScrolledRoot* fixedASR = ActiveScrolledRoot::PickAncestor(
        containerItemASR, aBuilder->CurrentActiveScrolledRoot());
    resultList.AppendNewToTop<nsDisplayFixedPosition>(
        aBuilder, this, &resultList, fixedASR, containerItemASR);
    ct.TrackContainer(resultList.GetTop());
  } else if (useStickyPosition) {
    // For position:sticky, the clip needs to be applied both to the sticky
    // container item and to the contents. The container item needs the clip
    // because a scrolled clip needs to move independently from the sticky
    // contents, and the contents need the clip so that they have finite
    // clipped bounds with respect to the container item's ASR. The latter is
    // a little tricky in the case where the sticky item has both fixed and
    // non-fixed descendants, because that means that the sticky container
    // item's ASR is the ASR of the fixed descendant.
    // For WebRender display list building, though, we still want to know the
    // the ASR that the sticky container item would normally have, so we stash
    // that on the display item as the "container ASR" (i.e. the normal ASR of
    // the container item, excluding the special behaviour induced by fixed
    // descendants).
    const ActiveScrolledRoot* stickyASR = ActiveScrolledRoot::PickAncestor(
        containerItemASR, aBuilder->CurrentActiveScrolledRoot());
    resultList.AppendNewToTop<nsDisplayStickyPosition>(
        aBuilder, this, &resultList, stickyASR,
        aBuilder->CurrentActiveScrolledRoot());
    ct.TrackContainer(resultList.GetTop());

    // If the sticky element is inside a filter, annotate the scroll frame that
    // scrolls the filter as having out-of-flow content inside a filter (this
    // inhibits paint skipping).
    if (aBuilder->GetFilterASR() && aBuilder->GetFilterASR() == stickyASR) {
      aBuilder->GetFilterASR()
          ->mScrollableFrame->SetHasOutOfFlowContentInsideFilter();
    }
  }

  /* If there's blending, wrap up the list in a blend-mode item. Note
   * that opacity can be applied before blending as the blend color is
   * not affected by foreground opacity (only background alpha).
   */

  if (useBlendMode) {
    DisplayListClipState::AutoSaveRestore blendModeClipState(aBuilder);
    resultList.AppendNewToTop<nsDisplayBlendMode>(
        aBuilder, this, &resultList, effects->mMixBlendMode, containerItemASR);
    ct.TrackContainer(resultList.GetTop());
  }

  bool createdOwnLayer = false;
  CreateOwnLayerIfNeeded(aBuilder, &resultList,
                         nsDisplayOwnLayer::OwnLayerForStackingContext,
                         &createdOwnLayer);
  if (createdOwnLayer) {
    ct.TrackContainer(resultList.GetTop());
  }

  if (aCreatedContainerItem) {
    *aCreatedContainerItem = ct.mCreatedContainer;
  }

  if (hitTestInfo) {
    // WebRender support is not yet implemented.
    MOZ_ASSERT(!gfxVars::UseWebRender());
    AddHitTestInfo(aBuilder, &resultList, ct.mContainer, this,
                   std::move(hitTestInfo));
  }

  aList->AppendToTop(&resultList);
}

static nsDisplayItem* WrapInWrapList(nsDisplayListBuilder* aBuilder,
                                     nsIFrame* aFrame, nsDisplayList* aList,
                                     const ActiveScrolledRoot* aContainerASR,
                                     bool aBuiltContainerItem = false) {
  nsDisplayItem* item = aList->GetBottom();
  if (!item) {
    return nullptr;
  }

  // We need a wrap list if there are multiple items, or if the single
  // item has a different frame. This can change in a partial build depending
  // on which items we build, so we need to ensure that we don't transition
  // to/from a wrap list without invalidating correctly.
  bool needsWrapList =
      item->GetAbove() || item->Frame() != aFrame || item->GetChildren();

  // If we have an explicit container item (that can't change without an
  // invalidation) or we're doing a full build and don't need a wrap list, then
  // we can skip adding one.
  if (aBuiltContainerItem || (!aBuilder->IsPartialUpdate() && !needsWrapList)) {
    aList->RemoveBottom();
    return item;
  }

  // If we're doing a partial build and we didn't need a wrap list
  // previously then we can try to work from there.
  if (aBuilder->IsPartialUpdate() &&
      !aFrame->HasDisplayItem(uint32_t(DisplayItemType::TYPE_CONTAINER))) {
    // If we now need a wrap list, we must previously have had no display items
    // or a single one belonging to this frame. Mark the item itself as
    // discarded so that RetainedDisplayListBuilder uses the ones we just built.
    // We don't want to mark the frame as modified as that would invalidate
    // positioned descendants that might be outside of this list, and might not
    // have been rebuilt this time.
    if (needsWrapList) {
      DiscardOldItems(aFrame);
    } else {
      aList->RemoveBottom();
      return item;
    }
  }

  // The last case we could try to handle is when we previously had a wrap list,
  // but no longer need it. Unfortunately we can't differentiate this case from
  // a partial build where other children exist but we just didn't build them
  // this time.
  // TODO:RetainedDisplayListBuilder's merge phase has the full list and
  // could strip them out.

  return MakeDisplayItem<nsDisplayContainer>(aBuilder, aFrame, aContainerASR,
                                             aList);
}

/**
 * Check if a frame should be visited for building display list.
 */
static bool DescendIntoChild(nsDisplayListBuilder* aBuilder,
                             const nsIFrame* aChild, const nsRect& aVisible,
                             const nsRect& aDirty) {
  if (aChild->GetStateBits() & NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO) {
    return true;
  }

  // If the child is a scrollframe that we want to ignore, then we need
  // to descend into it because its scrolled child may intersect the dirty
  // area even if the scrollframe itself doesn't.
  if (aChild == aBuilder->GetIgnoreScrollFrame()) {
    return true;
  }

  // There are cases where the "ignore scroll frame" on the builder is not set
  // correctly, and so we additionally want to catch cases where the child is
  // a root scrollframe and we are ignoring scrolling on the viewport.
  if (aChild == aBuilder->GetPresShellIgnoreScrollFrame()) {
    return true;
  }

  const nsRect overflow = aChild->GetVisualOverflowRect();

  if (aDirty.Intersects(overflow)) {
    return true;
  }

  if (aChild->ForceDescendIntoIfVisible() && aVisible.Intersects(overflow)) {
    return true;
  }

  if (aChild->IsFrameOfType(nsIFrame::eTablePart)) {
    // Relative positioning and transforms can cause table parts to move, but we
    // will still paint the backgrounds for their ancestor parts under them at
    // their 'normal' position. That means that we must consider the overflow
    // rects at both positions.

    // We convert the overflow rect into the nsTableFrame's coordinate
    // space, applying the normal position offset at each step. Then we
    // compare that against the builder's cached dirty rect in table
    // coordinate space.
    const nsIFrame* f = aChild;
    nsRect normalPositionOverflowRelativeToTable = overflow;

    while (f->IsFrameOfType(nsIFrame::eTablePart)) {
      normalPositionOverflowRelativeToTable += f->GetNormalPosition();
      f = f->GetParent();
    }

    nsDisplayTableBackgroundSet* tableBGs = aBuilder->GetTableBackgroundSet();
    if (tableBGs && tableBGs->GetDirtyRect().Intersects(
                        normalPositionOverflowRelativeToTable)) {
      return true;
    }
  }

  return false;
}

void nsIFrame::BuildDisplayListForSimpleChild(nsDisplayListBuilder* aBuilder,
                                              nsIFrame* aChild,
                                              const nsDisplayListSet& aLists) {
  // This is the shortcut for frames been handled along the common
  // path, the most common one of THE COMMON CASE mentioned later.
  MOZ_ASSERT(aChild->Type() != LayoutFrameType::Placeholder);
  MOZ_ASSERT(!aBuilder->GetSelectedFramesOnly() &&
                 !aBuilder->GetIncludeAllOutOfFlows(),
             "It should be held for painting to window");
  MOZ_ASSERT(aChild->GetStateBits() & NS_FRAME_SIMPLE_DISPLAYLIST);

  const nsPoint offset = aChild->GetOffsetTo(this);
  const nsRect visible = aBuilder->GetVisibleRect() - offset;
  const nsRect dirty = aBuilder->GetDirtyRect() - offset;

  if (!DescendIntoChild(aBuilder, aChild, visible, dirty)) {
    return;
  }

  // Child cannot be transformed since it is not a stacking context.
  nsDisplayListBuilder::AutoBuildingDisplayList buildingForChild(
      aBuilder, aChild, visible, dirty, false);

  CheckForApzAwareEventHandlers(aBuilder, aChild);

  aBuilder->BuildCompositorHitTestInfoIfNeeded(
      aChild, aLists.BorderBackground(),
      buildingForChild.IsAnimatedGeometryRoot());

  aChild->MarkAbsoluteFramesForDisplayList(aBuilder);
  aBuilder->AdjustWindowDraggingRegion(aChild);
  aBuilder->Check();
  aChild->BuildDisplayList(aBuilder, aLists);
  aBuilder->Check();
  aBuilder->DisplayCaret(aChild, aLists.Outlines());
#ifdef DEBUG
  DisplayDebugBorders(aBuilder, aChild, aLists);
#endif
}

static bool ShouldSkipFrame(nsDisplayListBuilder* aBuilder,
                            const nsIFrame* aFrame) {
  // If painting is restricted to just the background of the top level frame,
  // then we have nothing to do here.
  if (aBuilder->IsBackgroundOnly()) {
    return true;
  }

  if (aBuilder->IsForGenerateGlyphMask() &&
      (!aFrame->IsTextFrame() && aFrame->IsLeaf())) {
    return true;
  }

  // The placeholder frame should have the same content as the OOF frame.
  if (aBuilder->GetSelectedFramesOnly() &&
      (aFrame->IsLeaf() && !aFrame->IsSelected())) {
    return true;
  }

  static const nsFrameState skipFlags =
      (NS_FRAME_TOO_DEEP_IN_FRAME_TREE | NS_FRAME_IS_NONDISPLAY);

  return (aFrame->GetStateBits() & skipFlags);
}

void nsIFrame::BuildDisplayListForChild(nsDisplayListBuilder* aBuilder,
                                        nsIFrame* aChild,
                                        const nsDisplayListSet& aLists,
                                        uint32_t aFlags) {
  AutoCheckBuilder check(aBuilder);

  if (ShouldSkipFrame(aBuilder, aChild)) {
    return;
  }

  nsIFrame* child = aChild;
  auto* placeholder = child->IsPlaceholderFrame()
                          ? static_cast<nsPlaceholderFrame*>(child)
                          : nullptr;
  nsIFrame* childOrOutOfFlow =
      placeholder ? placeholder->GetOutOfFlowFrame() : child;

  nsIFrame* parent = childOrOutOfFlow->GetParent();
  const bool shouldApplyOverflowClip =
      nsFrame::ShouldApplyOverflowClipping(parent, parent->StyleDisplay());

  const bool isPaintingToWindow = aBuilder->IsPaintingToWindow();
  const bool doingShortcut =
      isPaintingToWindow &&
      (child->GetStateBits() & NS_FRAME_SIMPLE_DISPLAYLIST) &&
      // Animations may change the stacking context state.
      // ShouldApplyOverflowClipping is affected by the parent style, which does
      // not invalidate the NS_FRAME_SIMPLE_DISPLAYLIST bit.
      !(shouldApplyOverflowClip || child->MayHaveTransformAnimation() ||
        child->MayHaveOpacityAnimation());

  if (aBuilder->IsForPainting()) {
    aBuilder->ClearWillChangeBudgetStatus(child);
  }

  if (StaticPrefs::layout_css_scroll_anchoring_highlight()) {
    if (child->FirstContinuation()->IsScrollAnchor()) {
      nsRect bounds = child->GetContentRectRelativeToSelf() +
                      aBuilder->ToReferenceFrame(child);
      nsDisplaySolidColor* color = MakeDisplayItem<nsDisplaySolidColor>(
          aBuilder, child, bounds, NS_RGBA(255, 0, 255, 64));
      if (color) {
        color->SetOverrideZIndex(INT32_MAX);
        aLists.PositionedDescendants()->AppendToTop(color);
      }
    }
  }

  if (doingShortcut) {
    BuildDisplayListForSimpleChild(aBuilder, child, aLists);
    return;
  }

  // dirty rect in child-relative coordinates
  NS_ASSERTION(aBuilder->GetCurrentFrame() == this, "Wrong coord space!");
  const nsPoint offset = child->GetOffsetTo(this);
  nsRect visible = aBuilder->GetVisibleRect() - offset;
  nsRect dirty = aBuilder->GetDirtyRect() - offset;

  nsDisplayListBuilder::OutOfFlowDisplayData* savedOutOfFlowData = nullptr;
  if (placeholder) {
    if (placeholder->GetStateBits() & PLACEHOLDER_FOR_TOPLAYER) {
      // If the out-of-flow frame is in the top layer, the viewport frame
      // will paint it. Skip it here. Note that, only out-of-flow frames
      // with this property should be skipped, because non-HTML elements
      // may stop their children from being out-of-flow. Those frames
      // should still be handled in the normal in-flow path.
      return;
    }

    child = childOrOutOfFlow;
    if (aBuilder->IsForPainting()) {
      aBuilder->ClearWillChangeBudgetStatus(child);
    }

    // If 'child' is a pushed float then it's owned by a block that's not an
    // ancestor of the placeholder, and it will be painted by that block and
    // should not be painted through the placeholder. Also recheck
    // NS_FRAME_TOO_DEEP_IN_FRAME_TREE and NS_FRAME_IS_NONDISPLAY.
    static const nsFrameState skipFlags =
        (NS_FRAME_IS_PUSHED_FLOAT | NS_FRAME_TOO_DEEP_IN_FRAME_TREE |
         NS_FRAME_IS_NONDISPLAY);
    if (child->HasAnyStateBits(skipFlags) || nsLayoutUtils::IsPopup(child)) {
      return;
    }

    MOZ_ASSERT(child->HasAnyStateBits(NS_FRAME_OUT_OF_FLOW));
    savedOutOfFlowData = nsDisplayListBuilder::GetOutOfFlowData(child);

    if (aBuilder->GetIncludeAllOutOfFlows()) {
      visible = child->GetVisualOverflowRect();
      dirty = child->GetVisualOverflowRect();
    } else if (savedOutOfFlowData) {
      visible =
          savedOutOfFlowData->GetVisibleRectForFrame(aBuilder, child, &dirty);
    } else {
      // The out-of-flow frame did not intersect the dirty area. We may still
      // need to traverse into it, since it may contain placeholders we need
      // to enter to reach other out-of-flow frames that are visible.
      visible.SetEmpty();
      dirty.SetEmpty();
    }
  }

  NS_ASSERTION(!child->IsPlaceholderFrame(),
               "Should have dealt with placeholders already");

  if (!DescendIntoChild(aBuilder, child, visible, dirty)) {
    return;
  }

  const bool isSVG = child->GetStateBits() & NS_FRAME_SVG_LAYOUT;

  // This flag is raised if the control flow strays off the common path.
  // The common path is the most common one of THE COMMON CASE mentioned later.
  bool awayFromCommonPath = !isPaintingToWindow;

  // true if this is a real or pseudo stacking context
  bool pseudoStackingContext =
      (aFlags & DISPLAY_CHILD_FORCE_PSEUDO_STACKING_CONTEXT) != 0;

  if (!pseudoStackingContext && !isSVG && (aFlags & DISPLAY_CHILD_INLINE) &&
      !child->IsFrameOfType(eLineParticipant)) {
    // child is a non-inline frame in an inline context, i.e.,
    // it acts like inline-block or inline-table. Therefore it is a
    // pseudo-stacking-context.
    pseudoStackingContext = true;
  }

  const nsStyleDisplay* ourDisp = StyleDisplay();
  // REVIEW: Taken from nsBoxFrame::Paint
  // Don't paint our children if the theme object is a leaf.
  if (IsThemed(ourDisp) &&
      !PresContext()->Theme()->WidgetIsContainer(ourDisp->mAppearance))
    return;

  // Since we're now sure that we're adding this frame to the display list
  // (which means we're painting it, modulo occlusion), mark it as visible
  // within the displayport.
  if (isPaintingToWindow && child->TrackingVisibility()) {
    child->PresShell()->EnsureFrameInApproximatelyVisibleList(child);
    awayFromCommonPath = true;
  }

  child->SetBuiltDisplayList(true);

  // Child is composited if it's transformed, partially transparent, or has
  // SVG effects or a blend mode..
  const nsStyleDisplay* disp = child->StyleDisplay();
  const nsStyleEffects* effects = child->StyleEffects();
  const nsStylePosition* pos = child->StylePosition();

  const bool isPositioned = disp->IsAbsPosContainingBlock(child);

  const bool isStackingContext =
      (aFlags & DISPLAY_CHILD_FORCE_STACKING_CONTEXT) ||
      child->IsStackingContext(disp, pos, effects, isPositioned);

  if (pseudoStackingContext || isStackingContext || isPositioned ||
      placeholder || (!isSVG && disp->IsFloating(child)) ||
      (isSVG && effects->mClip.IsRect() && IsSVGContentWithCSSClip(child))) {
    pseudoStackingContext = true;
    awayFromCommonPath = true;
  }

  NS_ASSERTION(!isStackingContext || pseudoStackingContext,
               "Stacking contexts must also be pseudo-stacking-contexts");

  nsDisplayListBuilder::AutoBuildingDisplayList buildingForChild(
      aBuilder, child, visible, dirty);
  DisplayListClipState::AutoClipMultiple clipState(aBuilder);
  nsDisplayListBuilder::AutoCurrentActiveScrolledRootSetter asrSetter(aBuilder);
  CheckForApzAwareEventHandlers(aBuilder, child);

  if (savedOutOfFlowData) {
    aBuilder->SetBuildingInvisibleItems(false);

    clipState.SetClipChainForContainingBlockDescendants(
        savedOutOfFlowData->mContainingBlockClipChain);
    asrSetter.SetCurrentActiveScrolledRoot(
        savedOutOfFlowData->mContainingBlockActiveScrolledRoot);
    MOZ_ASSERT(awayFromCommonPath,
               "It is impossible when savedOutOfFlowData is true");
  } else if (HasAnyStateBits(NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO) &&
             placeholder) {
    NS_ASSERTION(visible.IsEmpty(), "should have empty visible rect");
    // Every item we build from now until we descent into an out of flow that
    // does have saved out of flow data should be invisible. This state gets
    // restored when AutoBuildingDisplayList gets out of scope.
    aBuilder->SetBuildingInvisibleItems(true);

    // If we have nested out-of-flow frames and the outer one isn't visible
    // then we won't have stored clip data for it. We can just clear the clip
    // instead since we know we won't render anything, and the inner out-of-flow
    // frame will setup the correct clip for itself.
    clipState.SetClipChainForContainingBlockDescendants(nullptr);
  }

  // Setup clipping for the parent's overflow:-moz-hidden-unscrollable,
  // or overflow:hidden on elements that don't support scrolling (and therefore
  // don't create nsHTML/XULScrollFrame). This clipping needs to not clip
  // anything directly rendered by the parent, only the rendering of its
  // children.
  // Don't use overflowClip to restrict the dirty rect, since some of the
  // descendants may not be clipped by it. Even if we end up with unnecessary
  // display items, they'll be pruned during ComputeVisibility.
  //
  // FIXME(emilio): Why can't we handle this more similarly to `clip` (on the
  // parent, rather than on the children)? Would ClipContentDescendants do what
  // we want?
  if (shouldApplyOverflowClip) {
    ApplyOverflowClipping(aBuilder, parent, clipState);
    awayFromCommonPath = true;
  }

  nsDisplayList list;
  nsDisplayList extraPositionedDescendants;
  const ActiveScrolledRoot* wrapListASR;
  bool builtContainerItem = false;
  if (isStackingContext) {
    // True stacking context.
    // For stacking contexts, BuildDisplayListForStackingContext handles
    // clipping and MarkAbsoluteFramesForDisplayList.
    nsDisplayListBuilder::AutoContainerASRTracker contASRTracker(aBuilder);
    child->BuildDisplayListForStackingContext(aBuilder, &list,
                                              &builtContainerItem);
    wrapListASR = contASRTracker.GetContainerASR();
    if (aBuilder->GetCaretFrame() == child) {
      builtContainerItem = false;
    }
  } else {
    Maybe<nsRect> clipPropClip =
        child->GetClipPropClipRect(disp, effects, child->GetSize());
    if (clipPropClip) {
      aBuilder->IntersectVisibleRect(*clipPropClip);
      aBuilder->IntersectDirtyRect(*clipPropClip);
      clipState.ClipContentDescendants(*clipPropClip +
                                       aBuilder->ToReferenceFrame(child));
      awayFromCommonPath = true;
    }

    child->MarkAbsoluteFramesForDisplayList(aBuilder);

    const bool differentAGR = buildingForChild.IsAnimatedGeometryRoot();

    if (!awayFromCommonPath &&
        // Some SVG frames might change opacity without invalidating the frame,
        // so exclude them from the fast-path.
        !child->IsFrameOfType(nsIFrame::eSVG)) {
      // The shortcut is available for the child for next time.
      child->AddStateBits(NS_FRAME_SIMPLE_DISPLAYLIST);
    }

    if (!pseudoStackingContext) {
      // THIS IS THE COMMON CASE.
      // Not a pseudo or real stacking context. Do the simple thing and
      // return early.

      aBuilder->BuildCompositorHitTestInfoIfNeeded(
          child, aLists.BorderBackground(), differentAGR);

      aBuilder->AdjustWindowDraggingRegion(child);
      aBuilder->Check();
      child->BuildDisplayList(aBuilder, aLists);
      aBuilder->Check();
      aBuilder->DisplayCaret(child, aLists.Outlines());
#ifdef DEBUG
      DisplayDebugBorders(aBuilder, child, aLists);
#endif
      return;
    }

    // A pseudo-stacking context (e.g., a positioned element with z-index auto).
    // We allow positioned descendants of the child to escape to our parent
    // stacking context's positioned descendant list, because they might be
    // z-index:non-auto
    nsDisplayListCollection pseudoStack(aBuilder);

    // If this frame has z-index != 0, then the display item might get sorted
    // into a different place in the list, and we can't rely on the previous
    // hit test info to still be behind us. Force a new hit test info for this
    // item, and for the item after it, so that we always have the right hit
    // test info.
    bool mayBeSorted = isPositioned && (ZIndex() != 0);

    aBuilder->BuildCompositorHitTestInfoIfNeeded(
        child, pseudoStack.BorderBackground(), differentAGR || mayBeSorted);

    aBuilder->AdjustWindowDraggingRegion(child);
    nsDisplayListBuilder::AutoContainerASRTracker contASRTracker(aBuilder);
    aBuilder->Check();
    child->BuildDisplayList(aBuilder, pseudoStack);
    aBuilder->Check();
    if (aBuilder->DisplayCaret(child, pseudoStack.Outlines())) {
      builtContainerItem = false;
    }

    // If we forced a new hit-test info because this frame is going to be
    // sorted, then clear the 'previous' data on the builder so that the next
    // item also gets a new hit test info. That way we're guaranteeing hit-test
    // info before and after each item that might get moved to a different spot.
    if (mayBeSorted) {
      aBuilder->SetCompositorHitTestInfo(
          nsRect(), CompositorHitTestFlags::eVisibleToHitTest);
    }

    wrapListASR = contASRTracker.GetContainerASR();

    list.AppendToTop(pseudoStack.BorderBackground());
    list.AppendToTop(pseudoStack.BlockBorderBackgrounds());
    list.AppendToTop(pseudoStack.Floats());
    list.AppendToTop(pseudoStack.Content());
    list.AppendToTop(pseudoStack.Outlines());
    extraPositionedDescendants.AppendToTop(pseudoStack.PositionedDescendants());
#ifdef DEBUG
    DisplayDebugBorders(aBuilder, child, aLists);
#endif
  }

  buildingForChild.RestoreBuildingInvisibleItemsValue();

  if (isPositioned || isStackingContext ||
      (aFlags & DISPLAY_CHILD_FORCE_STACKING_CONTEXT)) {
    // Genuine stacking contexts, and positioned pseudo-stacking-contexts,
    // go in this level.
    if (!list.IsEmpty()) {
      nsDisplayItem* item = WrapInWrapList(aBuilder, child, &list, wrapListASR,
                                           builtContainerItem);
      if (isSVG) {
        aLists.Content()->AppendToTop(item);
      } else {
        aLists.PositionedDescendants()->AppendToTop(item);
      }
    }
  } else if (!isSVG && disp->IsFloating(child)) {
    if (!list.IsEmpty()) {
      aLists.Floats()->AppendToTop(
          WrapInWrapList(aBuilder, child, &list, wrapListASR));
    }
  } else {
    aLists.Content()->AppendToTop(&list);
  }
  // We delay placing the positioned descendants of positioned frames to here,
  // because in the absence of z-index this is the correct order for them.
  // This doesn't affect correctness because the positioned descendants list
  // is sorted by z-order and content in BuildDisplayListForStackingContext,
  // but it means that sort routine needs to do less work.
  aLists.PositionedDescendants()->AppendToTop(&extraPositionedDescendants);
}

void nsIFrame::MarkAbsoluteFramesForDisplayList(
    nsDisplayListBuilder* aBuilder) {
  if (IsAbsoluteContainer()) {
    aBuilder->MarkFramesForDisplayList(
        this, GetAbsoluteContainingBlock()->GetChildList());
  }
}

nsresult nsFrame::GetContentForEvent(WidgetEvent* aEvent,
                                     nsIContent** aContent) {
  nsIFrame* f = nsLayoutUtils::GetNonGeneratedAncestor(this);
  *aContent = f->GetContent();
  NS_IF_ADDREF(*aContent);
  return NS_OK;
}

void nsFrame::FireDOMEvent(const nsAString& aDOMEventName,
                           nsIContent* aContent) {
  nsIContent* target = aContent ? aContent : GetContent();

  if (target) {
    RefPtr<AsyncEventDispatcher> asyncDispatcher = new AsyncEventDispatcher(
        target, aDOMEventName, CanBubble::eYes, ChromeOnlyDispatch::eNo);
    DebugOnly<nsresult> rv = asyncDispatcher->PostDOMEvent();
    NS_ASSERTION(NS_SUCCEEDED(rv), "AsyncEventDispatcher failed to dispatch");
  }
}

nsresult nsFrame::HandleEvent(nsPresContext* aPresContext,
                              WidgetGUIEvent* aEvent,
                              nsEventStatus* aEventStatus) {
  if (aEvent->mMessage == eMouseMove) {
    // XXX If the second argument of HandleDrag() is WidgetMouseEvent,
    //     the implementation becomes simpler.
    return HandleDrag(aPresContext, aEvent, aEventStatus);
  }

  if ((aEvent->mClass == eMouseEventClass &&
       aEvent->AsMouseEvent()->mButton == MouseButton::eLeft) ||
      aEvent->mClass == eTouchEventClass) {
    if (aEvent->mMessage == eMouseDown || aEvent->mMessage == eTouchStart) {
      HandlePress(aPresContext, aEvent, aEventStatus);
    } else if (aEvent->mMessage == eMouseUp || aEvent->mMessage == eTouchEnd) {
      HandleRelease(aPresContext, aEvent, aEventStatus);
    }
  }
  return NS_OK;
}

nsresult nsFrame::GetDataForTableSelection(
    const nsFrameSelection* aFrameSelection, mozilla::PresShell* aPresShell,
    WidgetMouseEvent* aMouseEvent, nsIContent** aParentContent,
    int32_t* aContentOffset, TableSelectionMode* aTarget) {
  if (!aFrameSelection || !aPresShell || !aMouseEvent || !aParentContent ||
      !aContentOffset || !aTarget)
    return NS_ERROR_NULL_POINTER;

  *aParentContent = nullptr;
  *aContentOffset = 0;
  *aTarget = TableSelectionMode::None;

  int16_t displaySelection = aPresShell->GetSelectionFlags();

  bool selectingTableCells = aFrameSelection->GetTableCellSelection();

  // DISPLAY_ALL means we're in an editor.
  // If already in cell selection mode,
  //  continue selecting with mouse drag or end on mouse up,
  //  or when using shift key to extend block of cells
  //  (Mouse down does normal selection unless Ctrl/Cmd is pressed)
  bool doTableSelection =
      displaySelection == nsISelectionDisplay::DISPLAY_ALL &&
      selectingTableCells &&
      (aMouseEvent->mMessage == eMouseMove ||
       (aMouseEvent->mMessage == eMouseUp &&
        aMouseEvent->mButton == MouseButton::eLeft) ||
       aMouseEvent->IsShift());

  if (!doTableSelection) {
    // In Browser, special 'table selection' key must be pressed for table
    // selection or when just Shift is pressed and we're already in table/cell
    // selection mode
#ifdef XP_MACOSX
    doTableSelection = aMouseEvent->IsMeta() ||
                       (aMouseEvent->IsShift() && selectingTableCells);
#else
    doTableSelection = aMouseEvent->IsControl() ||
                       (aMouseEvent->IsShift() && selectingTableCells);
#endif
  }
  if (!doTableSelection) return NS_OK;

  // Get the cell frame or table frame (or parent) of the current content node
  nsIFrame* frame = this;
  bool foundCell = false;
  bool foundTable = false;

  // Get the limiting node to stop parent frame search
  nsIContent* limiter = aFrameSelection->GetLimiter();

  // If our content node is an ancestor of the limiting node,
  // we should stop the search right now.
  if (limiter && limiter->IsInclusiveDescendantOf(GetContent())) return NS_OK;

  // We don't initiate row/col selection from here now,
  //  but we may in future
  // bool selectColumn = false;
  // bool selectRow = false;

  while (frame) {
    // Check for a table cell by querying to a known CellFrame interface
    nsITableCellLayout* cellElement = do_QueryFrame(frame);
    if (cellElement) {
      foundCell = true;
      // TODO: If we want to use proximity to top or left border
      //      for row and column selection, this is the place to do it
      break;
    } else {
      // If not a cell, check for table
      // This will happen when starting frame is the table or child of a table,
      //  such as a row (we were inbetween cells or in table border)
      nsTableWrapperFrame* tableFrame = do_QueryFrame(frame);
      if (tableFrame) {
        foundTable = true;
        // TODO: How can we select row when along left table edge
        //  or select column when along top edge?
        break;
      } else {
        frame = frame->GetParent();
        // Stop if we have hit the selection's limiting content node
        if (frame && frame->GetContent() == limiter) break;
      }
    }
  }
  // We aren't in a cell or table
  if (!foundCell && !foundTable) return NS_OK;

  nsIContent* tableOrCellContent = frame->GetContent();
  if (!tableOrCellContent) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> parentContent = tableOrCellContent->GetParent();
  if (!parentContent) return NS_ERROR_FAILURE;

  int32_t offset = parentContent->ComputeIndexOf(tableOrCellContent);
  // Not likely?
  if (offset < 0) return NS_ERROR_FAILURE;

  // Everything is OK -- set the return values
  parentContent.forget(aParentContent);

  *aContentOffset = offset;

#if 0
  if (selectRow)
    *aTarget = TableSelectionMode::Row;
  else if (selectColumn)
    *aTarget = TableSelectionMode::Column;
  else
#endif
  if (foundCell) {
    *aTarget = TableSelectionMode::Cell;
  } else if (foundTable) {
    *aTarget = TableSelectionMode::Table;
  }

  return NS_OK;
}

static bool IsEditingHost(const nsIFrame* aFrame) {
  auto* element = nsGenericHTMLElement::FromNodeOrNull(aFrame->GetContent());
  return element && element->IsEditableRoot();
}

static StyleUserSelect UsedUserSelect(const nsIFrame* aFrame) {
  if (aFrame->HasAnyStateBits(NS_FRAME_GENERATED_CONTENT)) {
    return StyleUserSelect::None;
  }

  // Per https://drafts.csswg.org/css-ui-4/#content-selection:
  //
  // The computed value is the specified value, except:
  //
  //   1 - on editable elements where the computed value is always 'contain'
  //       regardless of the specified value.
  //   2 - when the specified value is auto, which computes to one of the other
  //       values [...]
  //
  // See https://github.com/w3c/csswg-drafts/issues/3344 to see why we do this
  // at used-value time instead of at computed-value time.
  //
  // Also, we check for auto first to allow explicitly overriding the value for
  // the editing host.
  auto style = aFrame->StyleUIReset()->mUserSelect;
  if (style != StyleUserSelect::Auto) {
    return style;
  }

  if (aFrame->IsTextInputFrame() || IsEditingHost(aFrame)) {
    // We don't implement 'contain' itself, but we make 'text' behave as
    // 'contain' for contenteditable and <input> / <textarea> elements anyway so
    // this is ok.
    return StyleUserSelect::Text;
  }

  auto* parent = nsLayoutUtils::GetParentOrPlaceholderFor(aFrame);
  return parent ? UsedUserSelect(parent) : StyleUserSelect::Text;
}

bool nsIFrame::IsSelectable(StyleUserSelect* aSelectStyle) const {
  auto style = UsedUserSelect(this);
  if (aSelectStyle) {
    *aSelectStyle = style;
  }
  return style != StyleUserSelect::None;
}

/**
 * Handles the Mouse Press Event for the frame
 */
NS_IMETHODIMP
nsFrame::HandlePress(nsPresContext* aPresContext, WidgetGUIEvent* aEvent,
                     nsEventStatus* aEventStatus) {
  NS_ENSURE_ARG_POINTER(aEventStatus);
  if (nsEventStatus_eConsumeNoDefault == *aEventStatus) {
    return NS_OK;
  }

  NS_ENSURE_ARG_POINTER(aEvent);
  if (aEvent->mClass == eTouchEventClass) {
    return NS_OK;
  }

  // We often get out of sync state issues with mousedown events that
  // get interrupted by alerts/dialogs.
  // Check with the ESM to see if we should process this one
  if (!aPresContext->EventStateManager()->EventStatusOK(aEvent)) return NS_OK;

  mozilla::PresShell* presShell = aPresContext->GetPresShell();
  if (!presShell) {
    return NS_ERROR_FAILURE;
  }

  // if we are in Navigator and the click is in a draggable node, we don't want
  // to start selection because we don't want to interfere with a potential
  // drag of said node and steal all its glory.
  int16_t isEditor = presShell->GetSelectionFlags();
  // weaaak. only the editor can display frame selection not just text and
  // images
  isEditor = isEditor == nsISelectionDisplay::DISPLAY_ALL;

  WidgetMouseEvent* mouseEvent = aEvent->AsMouseEvent();

  if (!mouseEvent->IsAlt()) {
    for (nsIContent* content = mContent; content;
         content = content->GetFlattenedTreeParent()) {
      if (nsContentUtils::ContentIsDraggable(content) &&
          !content->IsEditable()) {
        // coordinate stuff is the fix for bug #55921
        if ((mRect - GetPosition())
                .Contains(nsLayoutUtils::GetEventCoordinatesRelativeTo(
                    mouseEvent, this))) {
          return NS_OK;
        }
      }
    }
  }

  // check whether style allows selection
  // if not, don't tell selection the mouse event even occurred.
  StyleUserSelect selectStyle;
  // check for select: none
  if (!IsSelectable(&selectStyle)) {
    return NS_OK;
  }

  bool useFrameSelection = (selectStyle == StyleUserSelect::Text);

  // If the mouse is dragged outside the nearest enclosing scrollable area
  // while making a selection, the area will be scrolled. To do this, capture
  // the mouse on the nearest scrollable frame. If there isn't a scrollable
  // frame, or something else is already capturing the mouse, there's no
  // reason to capture.
  if (!PresShell::GetCapturingContent()) {
    nsIScrollableFrame* scrollFrame = nsLayoutUtils::GetNearestScrollableFrame(
        this, nsLayoutUtils::SCROLLABLE_SAME_DOC |
                  nsLayoutUtils::SCROLLABLE_INCLUDE_HIDDEN);
    if (scrollFrame) {
      nsIFrame* capturingFrame = do_QueryFrame(scrollFrame);
      PresShell::SetCapturingContent(capturingFrame->GetContent(),
                                     CaptureFlags::IgnoreAllowedState);
    }
  }

  // XXX This is screwy; it really should use the selection frame, not the
  // event frame
  const nsFrameSelection* frameselection = nullptr;
  if (useFrameSelection)
    frameselection = GetConstFrameSelection();
  else
    frameselection = presShell->ConstFrameSelection();

  if (!frameselection || frameselection->GetDisplaySelection() ==
                             nsISelectionController::SELECTION_OFF)
    return NS_OK;  // nothing to do we cannot affect selection from here

#ifdef XP_MACOSX
  if (mouseEvent->IsControl())
    return NS_OK;  // short circuit. hard coded for mac due to time restraints.
  bool control = mouseEvent->IsMeta();
#else
  bool control = mouseEvent->IsControl();
#endif

  RefPtr<nsFrameSelection> fc = const_cast<nsFrameSelection*>(frameselection);
  if (mouseEvent->mClickCount > 1) {
    // These methods aren't const but can't actually delete anything,
    // so no need for AutoWeakFrame.
    fc->SetDragState(true);
    return HandleMultiplePress(aPresContext, mouseEvent, aEventStatus, control);
  }

  nsPoint pt = nsLayoutUtils::GetEventCoordinatesRelativeTo(mouseEvent, this);
  ContentOffsets offsets = GetContentOffsetsFromPoint(pt, SKIP_HIDDEN);

  if (!offsets.content) return NS_ERROR_FAILURE;

  // Let Ctrl/Cmd+mouse down do table selection instead of drag initiation
  nsCOMPtr<nsIContent> parentContent;
  int32_t contentOffset;
  TableSelectionMode target;
  nsresult rv;
  rv = GetDataForTableSelection(frameselection, presShell, mouseEvent,
                                getter_AddRefs(parentContent), &contentOffset,
                                &target);
  if (NS_SUCCEEDED(rv) && parentContent) {
    fc->SetDragState(true);
    return fc->HandleTableSelection(parentContent, contentOffset, target,
                                    mouseEvent);
  }

  fc->SetDelayedCaretData(0);

  // Check if any part of this frame is selected, and if the
  // user clicked inside the selected region. If so, we delay
  // starting a new selection since the user may be trying to
  // drag the selected region to some other app.

  if (GetContent() && GetContent()->IsMaybeSelected()) {
    bool inSelection = false;
    UniquePtr<SelectionDetails> details = frameselection->LookUpSelection(
        offsets.content, 0, offsets.EndOffset(), false);

    //
    // If there are any details, check to see if the user clicked
    // within any selected region of the frame.
    //

    for (SelectionDetails* curDetail = details.get(); curDetail;
         curDetail = curDetail->mNext.get()) {
      //
      // If the user clicked inside a selection, then just
      // return without doing anything. We will handle placing
      // the caret later on when the mouse is released. We ignore
      // the spellcheck, find and url formatting selections.
      //
      if (curDetail->mSelectionType != SelectionType::eSpellCheck &&
          curDetail->mSelectionType != SelectionType::eFind &&
          curDetail->mSelectionType != SelectionType::eURLSecondary &&
          curDetail->mSelectionType != SelectionType::eURLStrikeout &&
          curDetail->mStart <= offsets.StartOffset() &&
          offsets.EndOffset() <= curDetail->mEnd) {
        inSelection = true;
      }
    }

    if (inSelection) {
      fc->SetDragState(false);
      fc->SetDelayedCaretData(mouseEvent);
      return NS_OK;
    }
  }

  fc->SetDragState(true);

  // Do not touch any nsFrame members after this point without adding
  // weakFrame checks.
  const nsFrameSelection::FocusMode focusMode = [&]() {
    // If "Shift" and "Ctrl" are both pressed, "Shift" is given precedence. This
    // mimics the old behaviour.
    if (mouseEvent->IsShift()) {
      return nsFrameSelection::FocusMode::kExtendSelection;
    }

    if (control) {
      return nsFrameSelection::FocusMode::kMultiRangeSelection;
    }

    return nsFrameSelection::FocusMode::kCollapseToNewPoint;
  }();

  rv = fc->HandleClick(offsets.content, offsets.StartOffset(),
                       offsets.EndOffset(), focusMode, offsets.associate);

  if (NS_FAILED(rv)) return rv;

  if (offsets.offset != offsets.secondaryOffset) fc->MaintainSelection();

  if (isEditor && !mouseEvent->IsShift() &&
      (offsets.EndOffset() - offsets.StartOffset()) == 1) {
    // A single node is selected and we aren't extending an existing
    // selection, which means the user clicked directly on an object (either
    // -moz-user-select: all or a non-text node without children).
    // Therefore, disable selection extension during mouse moves.
    // XXX This is a bit hacky; shouldn't editor be able to deal with this?
    fc->SetDragState(false);
  }

  return rv;
}

/*
 * SelectByTypeAtPoint
 *
 * Search for selectable content at point and attempt to select
 * based on the start and end selection behaviours.
 *
 * @param aPresContext Presentation context
 * @param aPoint Point at which selection will occur. Coordinates
 * should be relaitve to this frame.
 * @param aBeginAmountType, aEndAmountType Selection behavior, see
 * nsIFrame for definitions.
 * @param aSelectFlags Selection flags defined in nsFame.h.
 * @return success or failure at finding suitable content to select.
 */
nsresult nsFrame::SelectByTypeAtPoint(nsPresContext* aPresContext,
                                      const nsPoint& aPoint,
                                      nsSelectionAmount aBeginAmountType,
                                      nsSelectionAmount aEndAmountType,
                                      uint32_t aSelectFlags) {
  NS_ENSURE_ARG_POINTER(aPresContext);

  // No point in selecting if selection is turned off
  if (DetermineDisplaySelection() == nsISelectionController::SELECTION_OFF) {
    return NS_OK;
  }

  ContentOffsets offsets = GetContentOffsetsFromPoint(aPoint, SKIP_HIDDEN);
  if (!offsets.content) return NS_ERROR_FAILURE;

  int32_t offset;
  nsIFrame* theFrame = nsFrameSelection::GetFrameForNodeOffset(
      offsets.content, offsets.offset, offsets.associate, &offset);
  if (!theFrame) return NS_ERROR_FAILURE;

  nsFrame* frame = static_cast<nsFrame*>(theFrame);
  return frame->PeekBackwardAndForward(aBeginAmountType, aEndAmountType, offset,
                                       aBeginAmountType != eSelectWord,
                                       aSelectFlags);
}

/**
 * Multiple Mouse Press -- line or paragraph selection -- for the frame.
 * Wouldn't it be nice if this didn't have to be hardwired into Frame code?
 */
NS_IMETHODIMP
nsFrame::HandleMultiplePress(nsPresContext* aPresContext,
                             WidgetGUIEvent* aEvent,
                             nsEventStatus* aEventStatus, bool aControlHeld) {
  NS_ENSURE_ARG_POINTER(aEvent);
  NS_ENSURE_ARG_POINTER(aEventStatus);

  if (nsEventStatus_eConsumeNoDefault == *aEventStatus ||
      DetermineDisplaySelection() == nsISelectionController::SELECTION_OFF) {
    return NS_OK;
  }

  // Find out whether we're doing line or paragraph selection.
  // If browser.triple_click_selects_paragraph is true, triple-click selects
  // paragraph. Otherwise, triple-click selects line, and quadruple-click
  // selects paragraph (on platforms that support quadruple-click).
  nsSelectionAmount beginAmount, endAmount;
  WidgetMouseEvent* mouseEvent = aEvent->AsMouseEvent();
  if (!mouseEvent) {
    return NS_OK;
  }

  if (mouseEvent->mClickCount == 4) {
    beginAmount = endAmount = eSelectParagraph;
  } else if (mouseEvent->mClickCount == 3) {
    if (Preferences::GetBool("browser.triple_click_selects_paragraph")) {
      beginAmount = endAmount = eSelectParagraph;
    } else {
      beginAmount = eSelectBeginLine;
      endAmount = eSelectEndLine;
    }
  } else if (mouseEvent->mClickCount == 2) {
    // We only want inline frames; PeekBackwardAndForward dislikes blocks
    beginAmount = endAmount = eSelectWord;
  } else {
    return NS_OK;
  }

  nsPoint relPoint =
      nsLayoutUtils::GetEventCoordinatesRelativeTo(mouseEvent, this);
  return SelectByTypeAtPoint(aPresContext, relPoint, beginAmount, endAmount,
                             (aControlHeld ? SELECT_ACCUMULATE : 0));
}

nsresult nsFrame::PeekBackwardAndForward(nsSelectionAmount aAmountBack,
                                         nsSelectionAmount aAmountForward,
                                         int32_t aStartPos, bool aJumpLines,
                                         uint32_t aSelectFlags) {
  nsIFrame* baseFrame = this;
  int32_t baseOffset = aStartPos;
  nsresult rv;

  if (aAmountBack == eSelectWord) {
    // To avoid selecting the previous word when at start of word,
    // first move one character forward.
    nsPeekOffsetStruct pos(eSelectCharacter, eDirNext, aStartPos, nsPoint(0, 0),
                           aJumpLines,
                           true,  // limit on scrolled views
                           false, false, false);
    rv = PeekOffset(&pos);
    if (NS_SUCCEEDED(rv)) {
      baseFrame = pos.mResultFrame;
      baseOffset = pos.mContentOffset;
    }
  }

  // Use peek offset one way then the other:
  nsPeekOffsetStruct startpos(aAmountBack, eDirPrevious, baseOffset,
                              nsPoint(0, 0), aJumpLines,
                              true,  // limit on scrolled views
                              false, false, false);
  rv = baseFrame->PeekOffset(&startpos);
  if (NS_FAILED(rv)) return rv;

  nsPeekOffsetStruct endpos(aAmountForward, eDirNext, aStartPos, nsPoint(0, 0),
                            aJumpLines,
                            true,  // limit on scrolled views
                            false, false, false);
  rv = PeekOffset(&endpos);
  if (NS_FAILED(rv)) return rv;

  // Keep frameSelection alive.
  RefPtr<nsFrameSelection> frameSelection = GetFrameSelection();

  const nsFrameSelection::FocusMode focusMode =
      (aSelectFlags & SELECT_ACCUMULATE)
          ? nsFrameSelection::FocusMode::kMultiRangeSelection
          : nsFrameSelection::FocusMode::kCollapseToNewPoint;
  rv = frameSelection->HandleClick(
      startpos.mResultContent, startpos.mContentOffset, startpos.mContentOffset,
      focusMode, CARET_ASSOCIATE_AFTER);
  if (NS_FAILED(rv)) return rv;

  rv = frameSelection->HandleClick(
      endpos.mResultContent, endpos.mContentOffset, endpos.mContentOffset,
      nsFrameSelection::FocusMode::kExtendSelection, CARET_ASSOCIATE_BEFORE);
  if (NS_FAILED(rv)) return rv;

  // maintain selection
  return frameSelection->MaintainSelection(aAmountBack);
}

NS_IMETHODIMP nsFrame::HandleDrag(nsPresContext* aPresContext,
                                  WidgetGUIEvent* aEvent,
                                  nsEventStatus* aEventStatus) {
  MOZ_ASSERT(aEvent->mClass == eMouseEventClass,
             "HandleDrag can only handle mouse event");

  RefPtr<nsFrameSelection> frameselection = GetFrameSelection();
  if (!frameselection) {
    return NS_OK;
  }

  bool mouseDown = frameselection->GetDragState();
  if (!mouseDown) {
    return NS_OK;
  }

  nsIFrame* scrollbar =
      nsLayoutUtils::GetClosestFrameOfType(this, LayoutFrameType::Scrollbar);
  if (!scrollbar) {
    // XXX Do we really need to exclude non-selectable content here?
    // GetContentOffsetsFromPoint can handle it just fine, although some
    // other stuff might not like it.
    // NOTE: DetermineDisplaySelection() returns SELECTION_OFF for
    // non-selectable frames.
    if (DetermineDisplaySelection() == nsISelectionController::SELECTION_OFF) {
      return NS_OK;
    }
  }

  frameselection->StopAutoScrollTimer();

  // Check if we are dragging in a table cell
  nsCOMPtr<nsIContent> parentContent;
  int32_t contentOffset;
  TableSelectionMode target;
  WidgetMouseEvent* mouseEvent = aEvent->AsMouseEvent();
  mozilla::PresShell* presShell = aPresContext->PresShell();
  nsresult result;
  result = GetDataForTableSelection(frameselection, presShell, mouseEvent,
                                    getter_AddRefs(parentContent),
                                    &contentOffset, &target);

  AutoWeakFrame weakThis = this;
  if (NS_SUCCEEDED(result) && parentContent) {
    result = frameselection->HandleTableSelection(parentContent, contentOffset,
                                                  target, mouseEvent);
    if (NS_WARN_IF(NS_FAILED(result))) {
      return result;
    }
  } else {
    nsPoint pt = nsLayoutUtils::GetEventCoordinatesRelativeTo(mouseEvent, this);
    frameselection->HandleDrag(this, pt);
  }

  // The frameselection object notifies selection listeners synchronously above
  // which might have killed us.
  if (!weakThis.IsAlive()) {
    return NS_OK;
  }

  // get the nearest scrollframe
  nsIScrollableFrame* scrollFrame = nsLayoutUtils::GetNearestScrollableFrame(
      this, nsLayoutUtils::SCROLLABLE_SAME_DOC |
                nsLayoutUtils::SCROLLABLE_INCLUDE_HIDDEN);

  if (scrollFrame) {
    nsIFrame* capturingFrame = scrollFrame->GetScrolledFrame();
    if (capturingFrame) {
      nsPoint pt = nsLayoutUtils::GetEventCoordinatesRelativeTo(mouseEvent,
                                                                capturingFrame);
      frameselection->StartAutoScrollTimer(capturingFrame, pt, 30);
    }
  }

  return NS_OK;
}

/**
 * This static method handles part of the nsFrame::HandleRelease in a way
 * which doesn't rely on the nsFrame object to stay alive.
 */
static nsresult HandleFrameSelection(nsFrameSelection* aFrameSelection,
                                     nsIFrame::ContentOffsets& aOffsets,
                                     bool aHandleTableSel,
                                     int32_t aContentOffsetForTableSel,
                                     TableSelectionMode aTargetForTableSel,
                                     nsIContent* aParentContentForTableSel,
                                     WidgetGUIEvent* aEvent,
                                     const nsEventStatus* aEventStatus) {
  if (!aFrameSelection) {
    return NS_OK;
  }

  nsresult rv = NS_OK;

  if (nsEventStatus_eConsumeNoDefault != *aEventStatus) {
    if (!aHandleTableSel) {
      if (!aOffsets.content || !aFrameSelection->HasDelayedCaretData()) {
        return NS_ERROR_FAILURE;
      }

      // We are doing this to simulate what we would have done on HandlePress.
      // We didn't do it there to give the user an opportunity to drag
      // the text, but since they didn't drag, we want to place the
      // caret.
      // However, we'll use the mouse position from the release, since:
      //  * it's easier
      //  * that's the normal click position to use (although really, in
      //    the normal case, small movements that don't count as a drag
      //    can do selection)
      aFrameSelection->SetDragState(true);

      const nsFrameSelection::FocusMode focusMode =
          aFrameSelection->IsShiftDownInDelayedCaretData()
              ? nsFrameSelection::FocusMode::kExtendSelection
              : nsFrameSelection::FocusMode::kCollapseToNewPoint;
      rv = aFrameSelection->HandleClick(
          aOffsets.content, aOffsets.StartOffset(), aOffsets.EndOffset(),
          focusMode, aOffsets.associate);
      if (NS_FAILED(rv)) {
        return rv;
      }
    } else if (aParentContentForTableSel) {
      aFrameSelection->SetDragState(false);
      rv = aFrameSelection->HandleTableSelection(
          aParentContentForTableSel, aContentOffsetForTableSel,
          aTargetForTableSel, aEvent->AsMouseEvent());
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
    aFrameSelection->SetDelayedCaretData(0);
  }

  aFrameSelection->SetDragState(false);
  aFrameSelection->StopAutoScrollTimer();

  return NS_OK;
}

NS_IMETHODIMP nsFrame::HandleRelease(nsPresContext* aPresContext,
                                     WidgetGUIEvent* aEvent,
                                     nsEventStatus* aEventStatus) {
  if (aEvent->mClass != eMouseEventClass) {
    return NS_OK;
  }

  nsIFrame* activeFrame = GetActiveSelectionFrame(aPresContext, this);

  nsCOMPtr<nsIContent> captureContent = PresShell::GetCapturingContent();

  // We can unconditionally stop capturing because
  // we should never be capturing when the mouse button is up
  PresShell::ReleaseCapturingContent();

  bool selectionOff =
      (DetermineDisplaySelection() == nsISelectionController::SELECTION_OFF);

  RefPtr<nsFrameSelection> frameselection;
  ContentOffsets offsets;
  nsCOMPtr<nsIContent> parentContent;
  int32_t contentOffsetForTableSel = 0;
  TableSelectionMode targetForTableSel = TableSelectionMode::None;
  bool handleTableSelection = true;

  if (!selectionOff) {
    frameselection = GetFrameSelection();
    if (nsEventStatus_eConsumeNoDefault != *aEventStatus && frameselection) {
      // Check if the frameselection recorded the mouse going down.
      // If not, the user must have clicked in a part of the selection.
      // Place the caret before continuing!

      if (frameselection->MouseDownRecorded()) {
        nsPoint pt = nsLayoutUtils::GetEventCoordinatesRelativeTo(aEvent, this);
        offsets = GetContentOffsetsFromPoint(pt, SKIP_HIDDEN);
        handleTableSelection = false;
      } else {
        GetDataForTableSelection(frameselection, PresShell(),
                                 aEvent->AsMouseEvent(),
                                 getter_AddRefs(parentContent),
                                 &contentOffsetForTableSel, &targetForTableSel);
      }
    }
  }

  // We might be capturing in some other document and the event just happened to
  // trickle down here. Make sure that document's frame selection is notified.
  // Note, this may cause the current nsFrame object to be deleted, bug 336592.
  RefPtr<nsFrameSelection> frameSelection;
  if (activeFrame != this &&
      static_cast<nsFrame*>(activeFrame)->DetermineDisplaySelection() !=
          nsISelectionController::SELECTION_OFF) {
    frameSelection = activeFrame->GetFrameSelection();
  }

  // Also check the selection of the capturing content which might be in a
  // different document.
  if (!frameSelection && captureContent) {
    Document* doc = captureContent->GetUncomposedDoc();
    if (doc) {
      mozilla::PresShell* capturingPresShell = doc->GetPresShell();
      if (capturingPresShell &&
          capturingPresShell != PresContext()->GetPresShell()) {
        frameSelection = capturingPresShell->FrameSelection();
      }
    }
  }

  if (frameSelection) {
    frameSelection->SetDragState(false);
    frameSelection->StopAutoScrollTimer();
    nsIScrollableFrame* scrollFrame = nsLayoutUtils::GetNearestScrollableFrame(
        this, nsLayoutUtils::SCROLLABLE_SAME_DOC |
                  nsLayoutUtils::SCROLLABLE_INCLUDE_HIDDEN);
    if (scrollFrame) {
      // Perform any additional scrolling needed to maintain CSS snap point
      // requirements when autoscrolling is over.
      scrollFrame->ScrollSnap();
    }
  }

  // Do not call any methods of the current object after this point!!!
  // The object is perhaps dead!

  return selectionOff ? NS_OK
                      : HandleFrameSelection(
                            frameselection, offsets, handleTableSelection,
                            contentOffsetForTableSel, targetForTableSel,
                            parentContent, aEvent, aEventStatus);
}

struct MOZ_STACK_CLASS FrameContentRange {
  FrameContentRange(nsIContent* aContent, int32_t aStart, int32_t aEnd)
      : content(aContent), start(aStart), end(aEnd) {}
  nsCOMPtr<nsIContent> content;
  int32_t start;
  int32_t end;
};

// Retrieve the content offsets of a frame
static FrameContentRange GetRangeForFrame(nsIFrame* aFrame) {
  nsIContent* content = aFrame->GetContent();
  if (!content) {
    NS_WARNING("Frame has no content");
    return FrameContentRange(nullptr, -1, -1);
  }

  LayoutFrameType type = aFrame->Type();
  if (type == LayoutFrameType::Text) {
    int32_t offset, offsetEnd;
    aFrame->GetOffsets(offset, offsetEnd);
    return FrameContentRange(content, offset, offsetEnd);
  }

  if (type == LayoutFrameType::Br) {
    nsIContent* parent = content->GetParent();
    int32_t beginOffset = parent->ComputeIndexOf(content);
    return FrameContentRange(parent, beginOffset, beginOffset);
  }

  while (content->IsRootOfAnonymousSubtree()) {
    content = content->GetParent();
  }

  nsIContent* parent = content->GetParent();
  if (aFrame->IsBlockFrameOrSubclass() || !parent) {
    return FrameContentRange(content, 0, content->GetChildCount());
  }

  // TODO(emilio): Revise this in presence of Shadow DOM / display: contents,
  // it's likely that we don't want to just walk the light tree, and we need to
  // change the representation of FrameContentRange.
  int32_t index = parent->ComputeIndexOf(content);
  MOZ_ASSERT(index >= 0);
  return FrameContentRange(parent, index, index + 1);
}

// The FrameTarget represents the closest frame to a point that can be selected
// The frame is the frame represented, frameEdge says whether one end of the
// frame is the result (in which case different handling is needed), and
// afterFrame says which end is repersented if frameEdge is true
struct FrameTarget {
  FrameTarget(nsIFrame* aFrame, bool aFrameEdge, bool aAfterFrame)
      : frame(aFrame), frameEdge(aFrameEdge), afterFrame(aAfterFrame) {}

  static FrameTarget Null() { return FrameTarget(nullptr, false, false); }

  bool IsNull() { return !frame; }
  nsIFrame* frame;
  bool frameEdge;
  bool afterFrame;
};

// See function implementation for information
static FrameTarget GetSelectionClosestFrame(nsIFrame* aFrame,
                                            const nsPoint& aPoint,
                                            uint32_t aFlags);

static bool SelfIsSelectable(nsIFrame* aFrame, uint32_t aFlags) {
  if ((aFlags & nsIFrame::SKIP_HIDDEN) &&
      !aFrame->StyleVisibility()->IsVisible()) {
    return false;
  }
  return !aFrame->IsGeneratedContentFrame() &&
         aFrame->StyleUIReset()->mUserSelect != StyleUserSelect::None;
}

static bool SelectionDescendToKids(nsIFrame* aFrame) {
  StyleUserSelect style = aFrame->StyleUIReset()->mUserSelect;
  nsIFrame* parent = aFrame->GetParent();
  // If we are only near (not directly over) then don't traverse
  // frames with independent selection (e.g. text and list controls)
  // unless we're already inside such a frame (see bug 268497).  Note that this
  // prevents any of the users of this method from entering form controls.
  // XXX We might want some way to allow using the up-arrow to go into a form
  // control, but the focus didn't work right anyway; it'd probably be enough
  // if the left and right arrows could enter textboxes (which I don't believe
  // they can at the moment)
  return !aFrame->IsGeneratedContentFrame() && style != StyleUserSelect::All &&
         style != StyleUserSelect::None &&
         ((parent->GetStateBits() & NS_FRAME_INDEPENDENT_SELECTION) ||
          !(aFrame->GetStateBits() & NS_FRAME_INDEPENDENT_SELECTION));
}

static FrameTarget GetSelectionClosestFrameForChild(nsIFrame* aChild,
                                                    const nsPoint& aPoint,
                                                    uint32_t aFlags) {
  nsIFrame* parent = aChild->GetParent();
  if (SelectionDescendToKids(aChild)) {
    nsPoint pt = aPoint - aChild->GetOffsetTo(parent);
    return GetSelectionClosestFrame(aChild, pt, aFlags);
  }
  return FrameTarget(aChild, false, false);
}

// When the cursor needs to be at the beginning of a block, it shouldn't be
// before the first child.  A click on a block whose first child is a block
// should put the cursor in the child.  The cursor shouldn't be between the
// blocks, because that's not where it's expected.
// Note that this method is guaranteed to succeed.
static FrameTarget DrillDownToSelectionFrame(nsIFrame* aFrame, bool aEndFrame,
                                             uint32_t aFlags) {
  if (SelectionDescendToKids(aFrame)) {
    nsIFrame* result = nullptr;
    nsIFrame* frame = aFrame->PrincipalChildList().FirstChild();
    if (!aEndFrame) {
      while (frame && (!SelfIsSelectable(frame, aFlags) || frame->IsEmpty()))
        frame = frame->GetNextSibling();
      if (frame) result = frame;
    } else {
      // Because the frame tree is singly linked, to find the last frame,
      // we have to iterate through all the frames
      // XXX I have a feeling this could be slow for long blocks, although
      //     I can't find any slowdowns
      while (frame) {
        if (!frame->IsEmpty() && SelfIsSelectable(frame, aFlags))
          result = frame;
        frame = frame->GetNextSibling();
      }
    }
    if (result) return DrillDownToSelectionFrame(result, aEndFrame, aFlags);
  }
  // If the current frame has no targetable children, target the current frame
  return FrameTarget(aFrame, true, aEndFrame);
}

// This method finds the closest valid FrameTarget on a given line; if there is
// no valid FrameTarget on the line, it returns a null FrameTarget
static FrameTarget GetSelectionClosestFrameForLine(
    nsBlockFrame* aParent, nsBlockFrame::LineIterator aLine,
    const nsPoint& aPoint, uint32_t aFlags) {
  // Account for end of lines (any iterator from the block is valid)
  if (aLine == aParent->LinesEnd())
    return DrillDownToSelectionFrame(aParent, true, aFlags);
  nsIFrame* frame = aLine->mFirstChild;
  nsIFrame* closestFromIStart = nullptr;
  nsIFrame* closestFromIEnd = nullptr;
  nscoord closestIStart = aLine->IStart(), closestIEnd = aLine->IEnd();
  WritingMode wm = aLine->mWritingMode;
  LogicalPoint pt(wm, aPoint, aLine->mContainerSize);
  bool canSkipBr = false;
  bool lastFrameWasEditable = false;
  for (int32_t n = aLine->GetChildCount(); n;
       --n, frame = frame->GetNextSibling()) {
    // Skip brFrames. Can only skip if the line contains at least
    // one selectable and non-empty frame before. Also, avoid skipping brs if
    // the previous thing had a different editableness than us, since then we
    // may end up not being able to select after it if the br is the last thing
    // on the line.
    if (!SelfIsSelectable(frame, aFlags) || frame->IsEmpty() ||
        (canSkipBr && frame->IsBrFrame() &&
         lastFrameWasEditable == frame->GetContent()->IsEditable())) {
      continue;
    }
    canSkipBr = true;
    lastFrameWasEditable =
        frame->GetContent() && frame->GetContent()->IsEditable();
    LogicalRect frameRect =
        LogicalRect(wm, frame->GetRect(), aLine->mContainerSize);
    if (pt.I(wm) >= frameRect.IStart(wm)) {
      if (pt.I(wm) < frameRect.IEnd(wm)) {
        return GetSelectionClosestFrameForChild(frame, aPoint, aFlags);
      }
      if (frameRect.IEnd(wm) >= closestIStart) {
        closestFromIStart = frame;
        closestIStart = frameRect.IEnd(wm);
      }
    } else {
      if (frameRect.IStart(wm) <= closestIEnd) {
        closestFromIEnd = frame;
        closestIEnd = frameRect.IStart(wm);
      }
    }
  }
  if (!closestFromIStart && !closestFromIEnd) {
    // We should only get here if there are no selectable frames on a line
    // XXX Do we need more elaborate handling here?
    return FrameTarget::Null();
  }
  if (closestFromIStart &&
      (!closestFromIEnd ||
       (abs(pt.I(wm) - closestIStart) <= abs(pt.I(wm) - closestIEnd)))) {
    return GetSelectionClosestFrameForChild(closestFromIStart, aPoint, aFlags);
  }
  return GetSelectionClosestFrameForChild(closestFromIEnd, aPoint, aFlags);
}

// This method is for the special handling we do for block frames; they're
// special because they represent paragraphs and because they are organized
// into lines, which have bounds that are not stored elsewhere in the
// frame tree.  Returns a null FrameTarget for frames which are not
// blocks or blocks with no lines except editable one.
static FrameTarget GetSelectionClosestFrameForBlock(nsIFrame* aFrame,
                                                    const nsPoint& aPoint,
                                                    uint32_t aFlags) {
  nsBlockFrame* bf = do_QueryFrame(aFrame);
  if (!bf) return FrameTarget::Null();

  // This code searches for the correct line
  nsBlockFrame::LineIterator end = bf->LinesEnd();
  nsBlockFrame::LineIterator curLine = bf->LinesBegin();
  nsBlockFrame::LineIterator closestLine = end;

  if (curLine != end) {
    // Convert aPoint into a LogicalPoint in the writing-mode of this block
    WritingMode wm = curLine->mWritingMode;
    LogicalPoint pt(wm, aPoint, curLine->mContainerSize);
    do {
      // Check to see if our point lies within the line's block-direction bounds
      nscoord BCoord = pt.B(wm) - curLine->BStart();
      nscoord BSize = curLine->BSize();
      if (BCoord >= 0 && BCoord < BSize) {
        closestLine = curLine;
        break;  // We found the line; stop looking
      }
      if (BCoord < 0) break;
      ++curLine;
    } while (curLine != end);

    if (closestLine == end) {
      nsBlockFrame::LineIterator prevLine = curLine.prev();
      nsBlockFrame::LineIterator nextLine = curLine;
      // Avoid empty lines
      while (nextLine != end && nextLine->IsEmpty()) ++nextLine;
      while (prevLine != end && prevLine->IsEmpty()) --prevLine;

      // This hidden pref dictates whether a point above or below all lines
      // comes up with a line or the beginning or end of the frame; 0 on
      // Windows, 1 on other platforms by default at the writing of this code
      int32_t dragOutOfFrame =
          Preferences::GetInt("browser.drag_out_of_frame_style");

      if (prevLine == end) {
        if (dragOutOfFrame == 1 || nextLine == end)
          return DrillDownToSelectionFrame(aFrame, false, aFlags);
        closestLine = nextLine;
      } else if (nextLine == end) {
        if (dragOutOfFrame == 1)
          return DrillDownToSelectionFrame(aFrame, true, aFlags);
        closestLine = prevLine;
      } else {  // Figure out which line is closer
        if (pt.B(wm) - prevLine->BEnd() < nextLine->BStart() - pt.B(wm))
          closestLine = prevLine;
        else
          closestLine = nextLine;
      }
    }
  }

  do {
    FrameTarget target =
        GetSelectionClosestFrameForLine(bf, closestLine, aPoint, aFlags);
    if (!target.IsNull()) return target;
    ++closestLine;
  } while (closestLine != end);

  // Fall back to just targeting the last targetable place
  return DrillDownToSelectionFrame(aFrame, true, aFlags);
}

// GetSelectionClosestFrame is the helper function that calculates the closest
// frame to the given point.
// It doesn't completely account for offset styles, so needs to be used in
// restricted environments.
// Cannot handle overlapping frames correctly, so it should receive the output
// of GetFrameForPoint
// Guaranteed to return a valid FrameTarget
static FrameTarget GetSelectionClosestFrame(nsIFrame* aFrame,
                                            const nsPoint& aPoint,
                                            uint32_t aFlags) {
  {
    // Handle blocks; if the frame isn't a block, the method fails
    FrameTarget target =
        GetSelectionClosestFrameForBlock(aFrame, aPoint, aFlags);
    if (!target.IsNull()) return target;
  }

  if (nsIFrame* kid = aFrame->PrincipalChildList().FirstChild()) {
    // Go through all the child frames to find the closest one
    nsIFrame::FrameWithDistance closest = {nullptr, nscoord_MAX, nscoord_MAX};
    for (; kid; kid = kid->GetNextSibling()) {
      if (!SelfIsSelectable(kid, aFlags) || kid->IsEmpty()) continue;

      kid->FindCloserFrameForSelection(aPoint, &closest);
    }
    if (closest.mFrame) {
      if (nsSVGUtils::IsInSVGTextSubtree(closest.mFrame))
        return FrameTarget(closest.mFrame, false, false);
      return GetSelectionClosestFrameForChild(closest.mFrame, aPoint, aFlags);
    }
  }

  // Use frame edge for grid, flex, table, and non-editable image frames.
  const bool useFrameEdge =
      aFrame->IsFlexOrGridContainer() || aFrame->IsTableFrame() ||
      (static_cast<nsImageFrame*>(do_QueryFrame(aFrame)) &&
       !aFrame->GetContent()->IsEditable());
  return FrameTarget(aFrame, useFrameEdge, false);
}

static nsIFrame::ContentOffsets OffsetsForSingleFrame(nsIFrame* aFrame,
                                                      const nsPoint& aPoint) {
  nsIFrame::ContentOffsets offsets;
  FrameContentRange range = GetRangeForFrame(aFrame);
  offsets.content = range.content;
  // If there are continuations (meaning it's not one rectangle), this is the
  // best this function can do
  if (aFrame->GetNextContinuation() || aFrame->GetPrevContinuation()) {
    offsets.offset = range.start;
    offsets.secondaryOffset = range.end;
    offsets.associate = CARET_ASSOCIATE_AFTER;
    return offsets;
  }

  // Figure out whether the offsets should be over, after, or before the frame
  nsRect rect(nsPoint(0, 0), aFrame->GetSize());

  bool isBlock = !aFrame->StyleDisplay()->IsInlineFlow();
  bool isRtl = (aFrame->StyleVisibility()->mDirection == StyleDirection::Rtl);
  if ((isBlock && rect.y < aPoint.y) ||
      (!isBlock && ((isRtl && rect.x + rect.width / 2 > aPoint.x) ||
                    (!isRtl && rect.x + rect.width / 2 < aPoint.x)))) {
    offsets.offset = range.end;
    if (rect.Contains(aPoint))
      offsets.secondaryOffset = range.start;
    else
      offsets.secondaryOffset = range.end;
  } else {
    offsets.offset = range.start;
    if (rect.Contains(aPoint))
      offsets.secondaryOffset = range.end;
    else
      offsets.secondaryOffset = range.start;
  }
  offsets.associate = offsets.offset == range.start ? CARET_ASSOCIATE_AFTER
                                                    : CARET_ASSOCIATE_BEFORE;
  return offsets;
}

static nsIFrame* AdjustFrameForSelectionStyles(nsIFrame* aFrame) {
  nsIFrame* adjustedFrame = aFrame;
  for (nsIFrame* frame = aFrame; frame; frame = frame->GetParent()) {
    // These are the conditions that make all children not able to handle
    // a cursor.
    StyleUserSelect userSelect = frame->StyleUIReset()->mUserSelect;
    if (userSelect != StyleUserSelect::Auto &&
        userSelect != StyleUserSelect::All) {
      break;
    }
    if (userSelect == StyleUserSelect::All ||
        frame->IsGeneratedContentFrame()) {
      adjustedFrame = frame;
    }
  }
  return adjustedFrame;
}

nsIFrame::ContentOffsets nsIFrame::GetContentOffsetsFromPoint(
    const nsPoint& aPoint, uint32_t aFlags) {
  nsIFrame* adjustedFrame;
  if (aFlags & IGNORE_SELECTION_STYLE) {
    adjustedFrame = this;
  } else {
    // This section of code deals with special selection styles.  Note that
    // -moz-all exists, even though it doesn't need to be explicitly handled.
    //
    // The offset is forced not to end up in generated content; content offsets
    // cannot represent content outside of the document's content tree.

    adjustedFrame = AdjustFrameForSelectionStyles(this);

    // -moz-user-select: all needs special handling, because clicking on it
    // should lead to the whole frame being selected
    if (adjustedFrame->StyleUIReset()->mUserSelect == StyleUserSelect::All) {
      nsPoint adjustedPoint = aPoint + this->GetOffsetTo(adjustedFrame);
      return OffsetsForSingleFrame(adjustedFrame, adjustedPoint);
    }

    // For other cases, try to find a closest frame starting from the parent of
    // the unselectable frame
    if (adjustedFrame != this) adjustedFrame = adjustedFrame->GetParent();
  }

  nsPoint adjustedPoint = aPoint + this->GetOffsetTo(adjustedFrame);

  FrameTarget closest =
      GetSelectionClosestFrame(adjustedFrame, adjustedPoint, aFlags);

  // If the correct offset is at one end of a frame, use offset-based
  // calculation method
  if (closest.frameEdge) {
    ContentOffsets offsets;
    FrameContentRange range = GetRangeForFrame(closest.frame);
    offsets.content = range.content;
    if (closest.afterFrame)
      offsets.offset = range.end;
    else
      offsets.offset = range.start;
    offsets.secondaryOffset = offsets.offset;
    offsets.associate = offsets.offset == range.start ? CARET_ASSOCIATE_AFTER
                                                      : CARET_ASSOCIATE_BEFORE;
    return offsets;
  }

  nsPoint pt;
  if (closest.frame != this) {
    if (nsSVGUtils::IsInSVGTextSubtree(closest.frame)) {
      pt = nsLayoutUtils::TransformAncestorPointToFrame(closest.frame, aPoint,
                                                        this);
    } else {
      pt = aPoint - closest.frame->GetOffsetTo(this);
    }
  } else {
    pt = aPoint;
  }
  return static_cast<nsFrame*>(closest.frame)
      ->CalcContentOffsetsFromFramePoint(pt);

  // XXX should I add some kind of offset standardization?
  // consider <b>xxxxx</b><i>zzzzz</i>; should any click between the last
  // x and first z put the cursor in the same logical position in addition
  // to the same visual position?
}

nsIFrame::ContentOffsets nsFrame::CalcContentOffsetsFromFramePoint(
    const nsPoint& aPoint) {
  return OffsetsForSingleFrame(this, aPoint);
}

bool nsIFrame::AssociateImage(const StyleImage& aImage) {
  imgRequestProxy* req = aImage.GetImageRequest();
  if (!req) {
    return false;
  }

  mozilla::css::ImageLoader* loader =
      PresContext()->Document()->StyleImageLoader();

  loader->AssociateRequestToFrame(req, this, 0);
  return true;
}

void nsIFrame::DisassociateImage(const StyleImage& aImage) {
  imgRequestProxy* req = aImage.GetImageRequest();
  if (!req) {
    return;
  }

  mozilla::css::ImageLoader* loader =
      PresContext()->Document()->StyleImageLoader();

  loader->DisassociateRequestFromFrame(req, this);
}

Maybe<nsIFrame::Cursor> nsIFrame::GetCursor(const nsPoint&) {
  StyleCursorKind kind = StyleUI()->mCursor.keyword;
  if (kind == StyleCursorKind::Auto) {
    // If this is editable, I-beam cursor is better for most elements.
    kind = (mContent && mContent->IsEditable()) ? StyleCursorKind::Text
                                                : StyleCursorKind::Default;
  }
  if (kind == StyleCursorKind::Text && GetWritingMode().IsVertical()) {
    // Per CSS UI spec, UA may treat value 'text' as
    // 'vertical-text' for vertical text.
    kind = StyleCursorKind::VerticalText;
  }

  return Some(Cursor{kind, AllowCustomCursorImage::Yes});
}

// Resize and incremental reflow

/* virtual */
void nsFrame::MarkIntrinsicISizesDirty() {
  // This version is meant only for what used to be box-to-block adaptors.
  // It should not be called by other derived classes.
  if (::IsXULBoxWrapped(this)) {
    nsBoxLayoutMetrics* metrics = BoxMetrics();

    SizeNeedsRecalc(metrics->mPrefSize);
    SizeNeedsRecalc(metrics->mMinSize);
    SizeNeedsRecalc(metrics->mMaxSize);
    SizeNeedsRecalc(metrics->mBlockPrefSize);
    SizeNeedsRecalc(metrics->mBlockMinSize);
    CoordNeedsRecalc(metrics->mFlex);
    CoordNeedsRecalc(metrics->mAscent);
  }

  // If we're a flex item, clear our flex-item-specific cached measurements
  // (which likely depended on our now-stale intrinsic isize).
  auto* parentFrame = GetParent();
  if (parentFrame && parentFrame->IsFlexContainerFrame()) {
    nsFlexContainerFrame::MarkCachedFlexMeasurementsDirty(this);
  }

  if (GetStateBits() & NS_FRAME_FONT_INFLATION_FLOW_ROOT) {
    nsFontInflationData::MarkFontInflationDataTextDirty(this);
  }
}

void nsIFrame::MarkSubtreeDirty() {
  if (HasAnyStateBits(NS_FRAME_IS_DIRTY)) {
    return;
  }
  // Unconditionally mark given frame dirty.
  AddStateBits(NS_FRAME_IS_DIRTY);

  // Mark all descendants dirty, unless:
  // - Already dirty.
  // - TableColGroup
  // - XULBox
  AutoTArray<nsIFrame*, 32> stack;
  for (nsIFrame::ChildListIterator lists(this); !lists.IsDone(); lists.Next()) {
    for (nsIFrame* kid : lists.CurrentList()) {
      stack.AppendElement(kid);
    }
  }
  while (!stack.IsEmpty()) {
    nsIFrame* f = stack.PopLastElement();
    if (f->HasAnyStateBits(NS_FRAME_IS_DIRTY) || f->IsTableColGroupFrame() ||
        f->IsXULBoxFrame()) {
      continue;
    }

    f->AddStateBits(NS_FRAME_IS_DIRTY);

    for (nsIFrame::ChildListIterator lists(f); !lists.IsDone(); lists.Next()) {
      for (nsIFrame* kid : lists.CurrentList()) {
        stack.AppendElement(kid);
      }
    }
  }
}

/* virtual */
nscoord nsFrame::GetMinISize(gfxContext* aRenderingContext) {
  nscoord result = 0;
  DISPLAY_MIN_INLINE_SIZE(this, result);
  return result;
}

/* virtual */
nscoord nsFrame::GetPrefISize(gfxContext* aRenderingContext) {
  nscoord result = 0;
  DISPLAY_PREF_INLINE_SIZE(this, result);
  return result;
}

/* virtual */
void nsFrame::AddInlineMinISize(gfxContext* aRenderingContext,
                                nsIFrame::InlineMinISizeData* aData) {
  nscoord isize = nsLayoutUtils::IntrinsicForContainer(
      aRenderingContext, this, nsLayoutUtils::MIN_ISIZE);
  aData->DefaultAddInlineMinISize(this, isize);
}

/* virtual */
void nsFrame::AddInlinePrefISize(gfxContext* aRenderingContext,
                                 nsIFrame::InlinePrefISizeData* aData) {
  nscoord isize = nsLayoutUtils::IntrinsicForContainer(
      aRenderingContext, this, nsLayoutUtils::PREF_ISIZE);
  aData->DefaultAddInlinePrefISize(isize);
}

void nsIFrame::InlineMinISizeData::DefaultAddInlineMinISize(nsIFrame* aFrame,
                                                            nscoord aISize,
                                                            bool aAllowBreak) {
  auto parent = aFrame->GetParent();
  MOZ_ASSERT(parent, "Must have a parent if we get here!");
  const bool mayBreak = aAllowBreak && !aFrame->CanContinueTextRun() &&
                        !parent->Style()->ShouldSuppressLineBreak() &&
                        parent->StyleText()->WhiteSpaceCanWrap(parent);
  if (mayBreak) {
    OptionallyBreak();
  }
  mTrailingWhitespace = 0;
  mSkipWhitespace = false;
  mCurrentLine += aISize;
  mAtStartOfLine = false;
  if (mayBreak) {
    OptionallyBreak();
  }
}

void nsIFrame::InlinePrefISizeData::DefaultAddInlinePrefISize(nscoord aISize) {
  mCurrentLine = NSCoordSaturatingAdd(mCurrentLine, aISize);
  mTrailingWhitespace = 0;
  mSkipWhitespace = false;
  mLineIsEmpty = false;
}

void nsIFrame::InlineMinISizeData::ForceBreak() {
  mCurrentLine -= mTrailingWhitespace;
  mPrevLines = std::max(mPrevLines, mCurrentLine);
  mCurrentLine = mTrailingWhitespace = 0;

  for (uint32_t i = 0, i_end = mFloats.Length(); i != i_end; ++i) {
    nscoord float_min = mFloats[i].Width();
    if (float_min > mPrevLines) mPrevLines = float_min;
  }
  mFloats.Clear();
  mSkipWhitespace = true;
}

void nsIFrame::InlineMinISizeData::OptionallyBreak(nscoord aHyphenWidth) {
  // If we can fit more content into a smaller width by staying on this
  // line (because we're still at a negative offset due to negative
  // text-indent or negative margin), don't break.  Otherwise, do the
  // same as ForceBreak.  it doesn't really matter when we accumulate
  // floats.
  if (mCurrentLine + aHyphenWidth < 0 || mAtStartOfLine) return;
  mCurrentLine += aHyphenWidth;
  ForceBreak();
}

void nsIFrame::InlinePrefISizeData::ForceBreak(StyleClear aBreakType) {
  MOZ_ASSERT(aBreakType == StyleClear::None || aBreakType == StyleClear::Both ||
                 aBreakType == StyleClear::Left ||
                 aBreakType == StyleClear::Right,
             "Must be a physical break type");

  // If this force break is not clearing any float, we can leave all the
  // floats to the next force break.
  if (mFloats.Length() != 0 && aBreakType != StyleClear::None) {
    // preferred widths accumulated for floats that have already
    // been cleared past
    nscoord floats_done = 0,
            // preferred widths accumulated for floats that have not yet
            // been cleared past
        floats_cur_left = 0, floats_cur_right = 0;

    for (uint32_t i = 0, i_end = mFloats.Length(); i != i_end; ++i) {
      const FloatInfo& floatInfo = mFloats[i];
      const nsStyleDisplay* floatDisp = floatInfo.Frame()->StyleDisplay();
      StyleClear breakType = floatDisp->mBreakType;
      if (breakType == StyleClear::Left || breakType == StyleClear::Right ||
          breakType == StyleClear::Both) {
        nscoord floats_cur =
            NSCoordSaturatingAdd(floats_cur_left, floats_cur_right);
        if (floats_cur > floats_done) {
          floats_done = floats_cur;
        }
        if (breakType != StyleClear::Right) {
          floats_cur_left = 0;
        }
        if (breakType != StyleClear::Left) {
          floats_cur_right = 0;
        }
      }

      StyleFloat floatStyle = floatDisp->mFloat;
      nscoord& floats_cur =
          floatStyle == StyleFloat::Left ? floats_cur_left : floats_cur_right;
      nscoord floatWidth = floatInfo.Width();
      // Negative-width floats don't change the available space so they
      // shouldn't change our intrinsic line width either.
      floats_cur = NSCoordSaturatingAdd(floats_cur, std::max(0, floatWidth));
    }

    nscoord floats_cur =
        NSCoordSaturatingAdd(floats_cur_left, floats_cur_right);
    if (floats_cur > floats_done) floats_done = floats_cur;

    mCurrentLine = NSCoordSaturatingAdd(mCurrentLine, floats_done);

    if (aBreakType == StyleClear::Both) {
      mFloats.Clear();
    } else {
      // If the break type does not clear all floats, it means there may
      // be some floats whose isize should contribute to the intrinsic
      // isize of the next line. The code here scans the current mFloats
      // and keeps floats which are not cleared by this break. Note that
      // floats may be cleared directly or indirectly. See below.
      nsTArray<FloatInfo> newFloats;
      MOZ_ASSERT(
          aBreakType == StyleClear::Left || aBreakType == StyleClear::Right,
          "Other values should have been handled in other branches");
      StyleFloat clearFloatType =
          aBreakType == StyleClear::Left ? StyleFloat::Left : StyleFloat::Right;
      // Iterate the array in reverse so that we can stop when there are
      // no longer any floats we need to keep. See below.
      for (FloatInfo& floatInfo : Reversed(mFloats)) {
        const nsStyleDisplay* floatDisp = floatInfo.Frame()->StyleDisplay();
        if (floatDisp->mFloat != clearFloatType) {
          newFloats.AppendElement(floatInfo);
        } else {
          // This is a float on the side that this break directly clears
          // which means we're not keeping it in mFloats. However, if
          // this float clears floats on the opposite side (via a value
          // of either 'both' or one of 'left'/'right'), any remaining
          // (earlier) floats on that side would be indirectly cleared
          // as well. Thus, we should break out of this loop and stop
          // considering earlier floats to be kept in mFloats.
          StyleClear floatBreakType = floatDisp->mBreakType;
          if (floatBreakType != aBreakType &&
              floatBreakType != StyleClear::None) {
            break;
          }
        }
      }
      newFloats.Reverse();
      mFloats = std::move(newFloats);
    }
  }

  mCurrentLine =
      NSCoordSaturatingSubtract(mCurrentLine, mTrailingWhitespace, nscoord_MAX);
  mPrevLines = std::max(mPrevLines, mCurrentLine);
  mCurrentLine = mTrailingWhitespace = 0;
  mSkipWhitespace = true;
  mLineIsEmpty = true;
}

static nscoord ResolveMargin(const LengthPercentageOrAuto& aStyle,
                             nscoord aPercentageBasis) {
  if (aStyle.IsAuto()) {
    return nscoord(0);
  }
  return nsLayoutUtils::ResolveToLength<false>(aStyle.AsLengthPercentage(),
                                               aPercentageBasis);
}

static nscoord ResolvePadding(const LengthPercentage& aStyle,
                              nscoord aPercentageBasis) {
  return nsLayoutUtils::ResolveToLength<true>(aStyle, aPercentageBasis);
}

static nsIFrame::IntrinsicSizeOffsetData IntrinsicSizeOffsets(
    nsIFrame* aFrame, nscoord aPercentageBasis, bool aForISize) {
  nsIFrame::IntrinsicSizeOffsetData result;
  WritingMode wm = aFrame->GetWritingMode();
  const auto& margin = aFrame->StyleMargin()->mMargin;
  bool verticalAxis = aForISize == wm.IsVertical();
  if (verticalAxis) {
    result.margin += ResolveMargin(margin.Get(eSideTop), aPercentageBasis);
    result.margin += ResolveMargin(margin.Get(eSideBottom), aPercentageBasis);
  } else {
    result.margin += ResolveMargin(margin.Get(eSideLeft), aPercentageBasis);
    result.margin += ResolveMargin(margin.Get(eSideRight), aPercentageBasis);
  }

  const auto& padding = aFrame->StylePadding()->mPadding;
  if (verticalAxis) {
    result.padding += ResolvePadding(padding.Get(eSideTop), aPercentageBasis);
    result.padding +=
        ResolvePadding(padding.Get(eSideBottom), aPercentageBasis);
  } else {
    result.padding += ResolvePadding(padding.Get(eSideLeft), aPercentageBasis);
    result.padding += ResolvePadding(padding.Get(eSideRight), aPercentageBasis);
  }

  const nsStyleBorder* styleBorder = aFrame->StyleBorder();
  if (verticalAxis) {
    result.border += styleBorder->GetComputedBorderWidth(eSideTop);
    result.border += styleBorder->GetComputedBorderWidth(eSideBottom);
  } else {
    result.border += styleBorder->GetComputedBorderWidth(eSideLeft);
    result.border += styleBorder->GetComputedBorderWidth(eSideRight);
  }

  const nsStyleDisplay* disp = aFrame->StyleDisplay();
  if (aFrame->IsThemed(disp)) {
    nsPresContext* presContext = aFrame->PresContext();

    LayoutDeviceIntMargin border = presContext->Theme()->GetWidgetBorder(
        presContext->DeviceContext(), aFrame, disp->mAppearance);
    result.border = presContext->DevPixelsToAppUnits(
        verticalAxis ? border.TopBottom() : border.LeftRight());

    LayoutDeviceIntMargin padding;
    if (presContext->Theme()->GetWidgetPadding(presContext->DeviceContext(),
                                               aFrame, disp->mAppearance,
                                               &padding)) {
      result.padding = presContext->DevPixelsToAppUnits(
          verticalAxis ? padding.TopBottom() : padding.LeftRight());
    }
  }
  return result;
}

/* virtual */ nsIFrame::IntrinsicSizeOffsetData nsFrame::IntrinsicISizeOffsets(
    nscoord aPercentageBasis) {
  return IntrinsicSizeOffsets(this, aPercentageBasis, true);
}

nsIFrame::IntrinsicSizeOffsetData nsIFrame::IntrinsicBSizeOffsets(
    nscoord aPercentageBasis) {
  return IntrinsicSizeOffsets(this, aPercentageBasis, false);
}

/* virtual */
IntrinsicSize nsFrame::GetIntrinsicSize() {
  return IntrinsicSize();  // default is width/height set to eStyleUnit_None
}

/* virtual */
AspectRatio nsFrame::GetIntrinsicRatio() { return AspectRatio(); }

/* virtual */
LogicalSize nsFrame::ComputeSize(gfxContext* aRenderingContext, WritingMode aWM,
                                 const LogicalSize& aCBSize,
                                 nscoord aAvailableISize,
                                 const LogicalSize& aMargin,
                                 const LogicalSize& aBorder,
                                 const LogicalSize& aPadding,
                                 ComputeSizeFlags aFlags) {
  MOZ_ASSERT(!GetIntrinsicRatio(),
             "Please override this method and call "
             "nsFrame::ComputeSizeWithIntrinsicDimensions instead.");
  LogicalSize result =
      ComputeAutoSize(aRenderingContext, aWM, aCBSize, aAvailableISize, aMargin,
                      aBorder, aPadding, aFlags);
  const nsStylePosition* stylePos = StylePosition();

  LogicalSize boxSizingAdjust(aWM);
  if (stylePos->mBoxSizing == StyleBoxSizing::Border) {
    boxSizingAdjust = aBorder + aPadding;
  }
  nscoord boxSizingToMarginEdgeISize = aMargin.ISize(aWM) + aBorder.ISize(aWM) +
                                       aPadding.ISize(aWM) -
                                       boxSizingAdjust.ISize(aWM);

  const auto* inlineStyleCoord = &stylePos->ISize(aWM);
  const auto* blockStyleCoord = &stylePos->BSize(aWM);

  auto parentFrame = GetParent();
  auto alignCB = parentFrame;
  bool isGridItem = parentFrame && parentFrame->IsGridContainerFrame() &&
                    !HasAnyStateBits(NS_FRAME_OUT_OF_FLOW);
  if (parentFrame && parentFrame->IsTableWrapperFrame() && IsTableFrame()) {
    // An inner table frame is sized as a grid item if its table wrapper is,
    // because they actually have the same CB (the wrapper's CB).
    // @see ReflowInput::InitCBReflowInput
    auto tableWrapper = GetParent();
    auto grandParent = tableWrapper->GetParent();
    isGridItem = (grandParent->IsGridContainerFrame() &&
                  !(tableWrapper->GetStateBits() & NS_FRAME_OUT_OF_FLOW));
    if (isGridItem) {
      // When resolving justify/align-self below, we want to use the grid
      // container's justify/align-items value and WritingMode.
      alignCB = grandParent;
    }
  }
  bool isFlexItem =
      parentFrame && parentFrame->IsFlexContainerFrame() &&
      !parentFrame->HasAnyStateBits(NS_STATE_FLEX_IS_EMULATING_LEGACY_BOX) &&
      !HasAnyStateBits(NS_FRAME_OUT_OF_FLOW);
  // This variable only gets set (and used) if isFlexItem is true.  It
  // indicates which axis (in this frame's own WM) corresponds to its
  // flex container's main axis.
  LogicalAxis flexMainAxis =
      eLogicalAxisInline;  // (init to make valgrind happy)
  if (isFlexItem) {
    // Flex items use their "flex-basis" property in place of their main-size
    // property for sizing purposes, *unless* they have "flex-basis:auto", in
    // which case they use their main-size property after all.
    flexMainAxis = nsFlexContainerFrame::IsItemInlineAxisMainAxis(this)
                       ? eLogicalAxisInline
                       : eLogicalAxisBlock;

    // NOTE: The logic here should match the similar chunk for updating
    // mainAxisCoord in nsFrame::ComputeSizeWithIntrinsicDimensions() (aside
    // from using a different dummy value in the IsUsedFlexBasisContent() case).
    const auto* flexBasis = &stylePos->mFlexBasis;
    auto& mainAxisCoord =
        (flexMainAxis == eLogicalAxisInline ? inlineStyleCoord
                                            : blockStyleCoord);

    // NOTE: If we're a table-wrapper frame, we skip this clause and just stick
    // with 'main-size:auto' behavior (which -- unlike 'content'
    // i.e. 'max-content' -- will give us the ability to honor percent sizes on
    // our table-box child when resolving the flex base size). The flexbox spec
    // doesn't call for this special case, but webcompat & regression-avoidance
    // seems to require it, for the time being... Tables sure are special.
    if (nsFlexContainerFrame::IsUsedFlexBasisContent(*flexBasis,
                                                     *mainAxisCoord) &&
        MOZ_LIKELY(!IsTableWrapperFrame())) {
      static const StyleSize maxContStyleCoord(
          StyleSize::ExtremumLength(StyleExtremumLength::MaxContent));
      mainAxisCoord = &maxContStyleCoord;
      // (Note: if our main axis is the block axis, then this 'max-content'
      // value will be treated like 'auto', via the IsAutoBSize() call below.)
    } else if (!flexBasis->IsAuto()) {
      // For all other non-'auto' flex-basis values, we just swap in the
      // flex-basis itself for the main-size property.
      mainAxisCoord = &flexBasis->AsSize();
    }  // else: flex-basis is 'auto', which is deferring to some explicit value
       // in mainAxisCoord. So we proceed w/o touching mainAxisCoord.
  }

  // Compute inline-axis size
  if (!inlineStyleCoord->IsAuto()) {
    result.ISize(aWM) = ComputeISizeValue(
        aRenderingContext, aCBSize.ISize(aWM), boxSizingAdjust.ISize(aWM),
        boxSizingToMarginEdgeISize, *inlineStyleCoord, aFlags);
  } else if (MOZ_UNLIKELY(isGridItem) && !IS_TRUE_OVERFLOW_CONTAINER(this)) {
    // 'auto' inline-size for grid-level box - fill the CB for 'stretch' /
    // 'normal' and clamp it to the CB if requested:
    bool stretch = false;
    if (!(aFlags & nsIFrame::eShrinkWrap) &&
        !StyleMargin()->HasInlineAxisAuto(aWM)) {
      auto inlineAxisAlignment =
          aWM.IsOrthogonalTo(alignCB->GetWritingMode())
              ? StylePosition()->UsedAlignSelf(alignCB->Style())._0
              : StylePosition()->UsedJustifySelf(alignCB->Style())._0;
      stretch = inlineAxisAlignment == StyleAlignFlags::NORMAL ||
                inlineAxisAlignment == StyleAlignFlags::STRETCH;
    }
    if (stretch || (aFlags & ComputeSizeFlags::eIClampMarginBoxMinSize)) {
      auto iSizeToFillCB =
          std::max(nscoord(0), aCBSize.ISize(aWM) - aPadding.ISize(aWM) -
                                   aBorder.ISize(aWM) - aMargin.ISize(aWM));
      if (stretch || result.ISize(aWM) > iSizeToFillCB) {
        result.ISize(aWM) = iSizeToFillCB;
      }
    }
  }

  // Flex items ignore their min & max sizing properties in their
  // flex container's main-axis.  (Those properties get applied later in
  // the flexbox algorithm.)
  const auto& maxISizeCoord = stylePos->MaxISize(aWM);
  nscoord maxISize = NS_UNCONSTRAINEDSIZE;
  if (!maxISizeCoord.IsNone() &&
      !(isFlexItem && flexMainAxis == eLogicalAxisInline)) {
    maxISize = ComputeISizeValue(
        aRenderingContext, aCBSize.ISize(aWM), boxSizingAdjust.ISize(aWM),
        boxSizingToMarginEdgeISize, maxISizeCoord, aFlags);
    result.ISize(aWM) = std::min(maxISize, result.ISize(aWM));
  }

  const auto& minISizeCoord = stylePos->MinISize(aWM);
  nscoord minISize;
  if (!minISizeCoord.IsAuto() &&
      !(isFlexItem && flexMainAxis == eLogicalAxisInline)) {
    minISize = ComputeISizeValue(
        aRenderingContext, aCBSize.ISize(aWM), boxSizingAdjust.ISize(aWM),
        boxSizingToMarginEdgeISize, minISizeCoord, aFlags);
  } else if (MOZ_UNLIKELY(aFlags & eIApplyAutoMinSize)) {
    // This implements "Implied Minimum Size of Grid Items".
    // https://drafts.csswg.org/css-grid/#min-size-auto
    minISize = std::min(maxISize, GetMinISize(aRenderingContext));
    if (inlineStyleCoord->IsLengthPercentage()) {
      minISize = std::min(minISize, result.ISize(aWM));
    } else if (aFlags & eIClampMarginBoxMinSize) {
      // "if the grid item spans only grid tracks that have a fixed max track
      // sizing function, its automatic minimum size in that dimension is
      // further clamped to less than or equal to the size necessary to fit
      // its margin box within the resulting grid area (flooring at zero)"
      // https://drafts.csswg.org/css-grid/#min-size-auto
      auto maxMinISize =
          std::max(nscoord(0), aCBSize.ISize(aWM) - aPadding.ISize(aWM) -
                                   aBorder.ISize(aWM) - aMargin.ISize(aWM));
      minISize = std::min(minISize, maxMinISize);
    }
  } else {
    // Treat "min-width: auto" as 0.
    // NOTE: Technically, "auto" is supposed to behave like "min-content" on
    // flex items. However, we don't need to worry about that here, because
    // flex items' min-sizes are intentionally ignored until the flex
    // container explicitly considers them during space distribution.
    minISize = 0;
  }
  result.ISize(aWM) = std::max(minISize, result.ISize(aWM));

  // Compute block-axis size
  // (but not if we have auto bsize or if we received the "eUseAutoBSize"
  // flag -- then, we'll just stick with the bsize that we already calculated
  // in the initial ComputeAutoSize() call.)
  if (!(aFlags & nsIFrame::eUseAutoBSize)) {
    if (!nsLayoutUtils::IsAutoBSize(*blockStyleCoord, aCBSize.BSize(aWM))) {
      result.BSize(aWM) = nsLayoutUtils::ComputeBSizeValue(
          aCBSize.BSize(aWM), boxSizingAdjust.BSize(aWM),
          blockStyleCoord->AsLengthPercentage());
    } else if (MOZ_UNLIKELY(isGridItem) && blockStyleCoord->IsAuto() &&
               !IS_TRUE_OVERFLOW_CONTAINER(this)) {
      auto cbSize = aCBSize.BSize(aWM);
      if (cbSize != NS_UNCONSTRAINEDSIZE) {
        // 'auto' block-size for grid-level box - fill the CB for 'stretch' /
        // 'normal' and clamp it to the CB if requested:
        bool stretch = false;
        if (!StyleMargin()->HasBlockAxisAuto(aWM)) {
          auto blockAxisAlignment =
              !aWM.IsOrthogonalTo(alignCB->GetWritingMode())
                  ? StylePosition()->UsedAlignSelf(alignCB->Style())._0
                  : StylePosition()->UsedJustifySelf(alignCB->Style())._0;
          stretch = blockAxisAlignment == StyleAlignFlags::NORMAL ||
                    blockAxisAlignment == StyleAlignFlags::STRETCH;
        }
        if (stretch || (aFlags & ComputeSizeFlags::eBClampMarginBoxMinSize)) {
          auto bSizeToFillCB =
              std::max(nscoord(0), cbSize - aPadding.BSize(aWM) -
                                       aBorder.BSize(aWM) - aMargin.BSize(aWM));
          if (stretch || (result.BSize(aWM) != NS_UNCONSTRAINEDSIZE &&
                          result.BSize(aWM) > bSizeToFillCB)) {
            result.BSize(aWM) = bSizeToFillCB;
          }
        }
      }
    }
  }

  const auto& maxBSizeCoord = stylePos->MaxBSize(aWM);

  if (result.BSize(aWM) != NS_UNCONSTRAINEDSIZE) {
    if (!nsLayoutUtils::IsAutoBSize(maxBSizeCoord, aCBSize.BSize(aWM)) &&
        !(isFlexItem && flexMainAxis == eLogicalAxisBlock)) {
      nscoord maxBSize = nsLayoutUtils::ComputeBSizeValue(
          aCBSize.BSize(aWM), boxSizingAdjust.BSize(aWM),
          maxBSizeCoord.AsLengthPercentage());
      result.BSize(aWM) = std::min(maxBSize, result.BSize(aWM));
    }

    const auto& minBSizeCoord = stylePos->MinBSize(aWM);

    if (!nsLayoutUtils::IsAutoBSize(minBSizeCoord, aCBSize.BSize(aWM)) &&
        !(isFlexItem && flexMainAxis == eLogicalAxisBlock)) {
      nscoord minBSize = nsLayoutUtils::ComputeBSizeValue(
          aCBSize.BSize(aWM), boxSizingAdjust.BSize(aWM),
          minBSizeCoord.AsLengthPercentage());
      result.BSize(aWM) = std::max(minBSize, result.BSize(aWM));
    }
  }

  const nsStyleDisplay* disp = StyleDisplay();
  if (IsThemed(disp)) {
    LayoutDeviceIntSize widget;
    bool canOverride = true;
    nsPresContext* presContext = PresContext();
    presContext->Theme()->GetMinimumWidgetSize(
        presContext, this, disp->mAppearance, &widget, &canOverride);

    // Convert themed widget's physical dimensions to logical coords
    LogicalSize size(aWM,
                     nsSize(presContext->DevPixelsToAppUnits(widget.width),
                            presContext->DevPixelsToAppUnits(widget.height)));

    // GMWS() returns border-box; we need content-box
    size.ISize(aWM) -= aBorder.ISize(aWM) + aPadding.ISize(aWM);
    size.BSize(aWM) -= aBorder.BSize(aWM) + aPadding.BSize(aWM);

    if (size.BSize(aWM) > result.BSize(aWM) || !canOverride) {
      result.BSize(aWM) = size.BSize(aWM);
    }
    if (size.ISize(aWM) > result.ISize(aWM) || !canOverride) {
      result.ISize(aWM) = size.ISize(aWM);
    }
  }

  result.ISize(aWM) = std::max(0, result.ISize(aWM));
  result.BSize(aWM) = std::max(0, result.BSize(aWM));

  return result;
}

LogicalSize nsFrame::ComputeSizeWithIntrinsicDimensions(
    gfxContext* aRenderingContext, WritingMode aWM,
    const IntrinsicSize& aIntrinsicSize, const AspectRatio& aIntrinsicRatio,
    const LogicalSize& aCBSize, const LogicalSize& aMargin,
    const LogicalSize& aBorder, const LogicalSize& aPadding,
    ComputeSizeFlags aFlags) {
  auto logicalRatio =
      aWM.IsVertical() ? aIntrinsicRatio.Inverted() : aIntrinsicRatio;
  const nsStylePosition* stylePos = StylePosition();
  const auto* inlineStyleCoord = &stylePos->ISize(aWM);
  const auto* blockStyleCoord = &stylePos->BSize(aWM);
  auto* parentFrame = GetParent();
  const bool isGridItem = parentFrame && parentFrame->IsGridContainerFrame() &&
                          !HasAnyStateBits(NS_FRAME_OUT_OF_FLOW);
  const bool isFlexItem =
      parentFrame && parentFrame->IsFlexContainerFrame() &&
      !parentFrame->HasAnyStateBits(NS_STATE_FLEX_IS_EMULATING_LEGACY_BOX) &&
      !HasAnyStateBits(NS_FRAME_OUT_OF_FLOW);
  // This variable only gets set (and used) if isFlexItem is true.  It
  // indicates which axis (in this frame's own WM) corresponds to its
  // flex container's main axis.
  LogicalAxis flexMainAxis =
      eLogicalAxisInline;  // (init to make valgrind happy)
  Maybe<StyleSize> imposedMainSizeStyleCoord;

  // If this is a flex item, and we're measuring its cross size after flexing
  // to resolve its main size, then we need to use the resolved main size
  // that the container provides to us *instead of* the main-size coordinate
  // from our style struct. (Otherwise, we'll be using an irrelevant value in
  // the aspect-ratio calculations below.)
  if (isFlexItem) {
    flexMainAxis = nsFlexContainerFrame::IsItemInlineAxisMainAxis(this)
                       ? eLogicalAxisInline
                       : eLogicalAxisBlock;

    // If FlexItemMainSizeOverride frame-property is set, then that means the
    // flex container is imposing a main-size on this flex item for it to use
    // as its size in the container's main axis.
    bool didImposeMainSize;
    nscoord imposedMainSize =
        GetProperty(nsIFrame::FlexItemMainSizeOverride(), &didImposeMainSize);
    if (didImposeMainSize) {
      imposedMainSizeStyleCoord = Some(StyleSize::LengthPercentage(
          LengthPercentage::FromAppUnits(imposedMainSize)));
      if (flexMainAxis == eLogicalAxisInline) {
        inlineStyleCoord = imposedMainSizeStyleCoord.ptr();
      } else {
        blockStyleCoord = imposedMainSizeStyleCoord.ptr();
      }

    } else {
      // Flex items use their "flex-basis" property in place of their main-size
      // property (e.g. "width") for sizing purposes, *unless* they have
      // "flex-basis:auto", in which case they use their main-size property
      // after all.
      // NOTE: The logic here should match the similar chunk for updating
      // mainAxisCoord in nsFrame::ComputeSize() (aside from using a different
      // dummy value in the IsUsedFlexBasisContent() case).
      const auto* flexBasis = &stylePos->mFlexBasis;
      auto& mainAxisCoord =
          (flexMainAxis == eLogicalAxisInline ? inlineStyleCoord
                                              : blockStyleCoord);

      if (nsFlexContainerFrame::IsUsedFlexBasisContent(*flexBasis,
                                                       *mainAxisCoord)) {
        // If we get here, we're resolving the flex base size for a flex item,
        // and we fall into the flexbox spec section 9.2 step 3, substep C (if
        // we have a definite cross size) or E (if not). And specifically:
        //
        // * If we have a definite cross size, we're supposed to resolve our
        //   main-size based on that and our intrinsic ratio.
        // * Otherwise, we're supposed to produce our max-content size.
        //
        // Conveniently, we can handle both of those scenarios (regardless of
        // which substep we fall into) by using the 'auto' keyword for our
        // main-axis coordinate here. (This makes sense, because the spec is
        // effectively trying to produce the 'auto' sizing behavior).
        static const StyleSize autoSize(StyleSize::Auto());
        mainAxisCoord = &autoSize;
      } else if (!flexBasis->IsAuto()) {
        // For all other non-'auto' flex-basis values, we just swap in the
        // flex-basis itself for the main-size property.
        mainAxisCoord = &flexBasis->AsSize();
      }  // else: flex-basis is 'auto', which is deferring to some explicit
         // value in mainAxisCoord. So we proceed w/o touching mainAxisCoord.
    }
  }

  // Handle intrinsic sizes and their interaction with
  // {min-,max-,}{width,height} according to the rules in
  // http://www.w3.org/TR/CSS21/visudet.html#min-max-widths

  // Note: throughout the following section of the function, I avoid
  // a * (b / c) because of its reduced accuracy relative to a * b / c
  // or (a * b) / c (which are equivalent).

  const bool isAutoISize = inlineStyleCoord->IsAuto();
  const bool isAutoBSize =
      nsLayoutUtils::IsAutoBSize(*blockStyleCoord, aCBSize.BSize(aWM));

  LogicalSize boxSizingAdjust(aWM);
  if (stylePos->mBoxSizing == StyleBoxSizing::Border) {
    boxSizingAdjust = aBorder + aPadding;
  }
  nscoord boxSizingToMarginEdgeISize = aMargin.ISize(aWM) + aBorder.ISize(aWM) +
                                       aPadding.ISize(aWM) -
                                       boxSizingAdjust.ISize(aWM);

  nscoord iSize, minISize, maxISize, bSize, minBSize, maxBSize;
  enum class Stretch {
    // stretch to fill the CB (preserving intrinsic ratio) in the relevant axis
    StretchPreservingRatio,  // XXX not used yet
    // stretch to fill the CB in the relevant axis
    Stretch,
    // no stretching in the relevant axis
    NoStretch,
  };
  // just to avoid having to type these out everywhere:
  const auto eStretchPreservingRatio = Stretch::StretchPreservingRatio;
  const auto eStretch = Stretch::Stretch;
  const auto eNoStretch = Stretch::NoStretch;

  Stretch stretchI = eNoStretch;  // stretch behavior in the inline axis
  Stretch stretchB = eNoStretch;  // stretch behavior in the block axis

  const bool isVertical = aWM.IsVertical();
  const auto& isizeCoord =
      isVertical ? aIntrinsicSize.height : aIntrinsicSize.width;
  const bool hasIntrinsicISize = isizeCoord.isSome();
  nscoord intrinsicISize = std::max(0, isizeCoord.valueOr(0));

  const auto& bsizeCoord =
      isVertical ? aIntrinsicSize.width : aIntrinsicSize.height;
  const bool hasIntrinsicBSize = bsizeCoord.isSome();
  nscoord intrinsicBSize = std::max(0, bsizeCoord.valueOr(0));

  if (!isAutoISize) {
    iSize = ComputeISizeValue(
        aRenderingContext, aCBSize.ISize(aWM), boxSizingAdjust.ISize(aWM),
        boxSizingToMarginEdgeISize, *inlineStyleCoord, aFlags);
  } else if (MOZ_UNLIKELY(isGridItem)) {
    MOZ_ASSERT(!IS_TRUE_OVERFLOW_CONTAINER(this));
    // 'auto' inline-size for grid-level box - apply 'stretch' as needed:
    auto cbSize = aCBSize.ISize(aWM);
    if (cbSize != NS_UNCONSTRAINEDSIZE) {
      if (!StyleMargin()->HasInlineAxisAuto(aWM)) {
        auto inlineAxisAlignment =
            aWM.IsOrthogonalTo(GetParent()->GetWritingMode())
                ? stylePos->UsedAlignSelf(GetParent()->Style())._0
                : stylePos->UsedJustifySelf(GetParent()->Style())._0;
        // Note: 'normal' means 'start' for elements with an intrinsic size
        // or ratio in the relevant dimension, otherwise 'stretch'.
        // https://drafts.csswg.org/css-grid/#grid-item-sizing
        if ((inlineAxisAlignment == StyleAlignFlags::NORMAL &&
             !hasIntrinsicISize && !logicalRatio) ||
            inlineAxisAlignment == StyleAlignFlags::STRETCH) {
          stretchI = eStretch;
        }
      }
      if (stretchI != eNoStretch ||
          (aFlags & ComputeSizeFlags::eIClampMarginBoxMinSize)) {
        iSize =
            std::max(nscoord(0), cbSize - aPadding.ISize(aWM) -
                                     aBorder.ISize(aWM) - aMargin.ISize(aWM));
      }
    } else {
      // Reset this flag to avoid applying the clamping below.
      aFlags =
          ComputeSizeFlags(aFlags & ~ComputeSizeFlags::eIClampMarginBoxMinSize);
    }
  }

  const auto& maxISizeCoord = stylePos->MaxISize(aWM);

  if (!maxISizeCoord.IsNone() &&
      !(isFlexItem && flexMainAxis == eLogicalAxisInline)) {
    maxISize = ComputeISizeValue(
        aRenderingContext, aCBSize.ISize(aWM), boxSizingAdjust.ISize(aWM),
        boxSizingToMarginEdgeISize, maxISizeCoord, aFlags);
  } else {
    maxISize = nscoord_MAX;
  }

  // NOTE: Flex items ignore their min & max sizing properties in their
  // flex container's main-axis.  (Those properties get applied later in
  // the flexbox algorithm.)

  const auto& minISizeCoord = stylePos->MinISize(aWM);

  if (!minISizeCoord.IsAuto() &&
      !(isFlexItem && flexMainAxis == eLogicalAxisInline)) {
    minISize = ComputeISizeValue(
        aRenderingContext, aCBSize.ISize(aWM), boxSizingAdjust.ISize(aWM),
        boxSizingToMarginEdgeISize, minISizeCoord, aFlags);
  } else {
    // Treat "min-width: auto" as 0.
    // NOTE: Technically, "auto" is supposed to behave like "min-content" on
    // flex items. However, we don't need to worry about that here, because
    // flex items' min-sizes are intentionally ignored until the flex
    // container explicitly considers them during space distribution.
    minISize = 0;
  }

  if (!isAutoBSize) {
    bSize = nsLayoutUtils::ComputeBSizeValue(
        aCBSize.BSize(aWM), boxSizingAdjust.BSize(aWM),
        blockStyleCoord->AsLengthPercentage());
  } else if (MOZ_UNLIKELY(isGridItem)) {
    MOZ_ASSERT(!IS_TRUE_OVERFLOW_CONTAINER(this));
    // 'auto' block-size for grid-level box - apply 'stretch' as needed:
    auto cbSize = aCBSize.BSize(aWM);
    if (cbSize != NS_UNCONSTRAINEDSIZE) {
      if (!StyleMargin()->HasBlockAxisAuto(aWM)) {
        auto blockAxisAlignment =
            !aWM.IsOrthogonalTo(GetParent()->GetWritingMode())
                ? stylePos->UsedAlignSelf(GetParent()->Style())._0
                : stylePos->UsedJustifySelf(GetParent()->Style())._0;
        // Note: 'normal' means 'start' for elements with an intrinsic size
        // or ratio in the relevant dimension, otherwise 'stretch'.
        // https://drafts.csswg.org/css-grid/#grid-item-sizing
        if ((blockAxisAlignment == StyleAlignFlags::NORMAL &&
             !hasIntrinsicBSize && !logicalRatio) ||
            blockAxisAlignment == StyleAlignFlags::STRETCH) {
          stretchB = eStretch;
        }
      }
      if (stretchB != eNoStretch ||
          (aFlags & ComputeSizeFlags::eBClampMarginBoxMinSize)) {
        bSize =
            std::max(nscoord(0), cbSize - aPadding.BSize(aWM) -
                                     aBorder.BSize(aWM) - aMargin.BSize(aWM));
      }
    } else {
      // Reset this flag to avoid applying the clamping below.
      aFlags =
          ComputeSizeFlags(aFlags & ~ComputeSizeFlags::eBClampMarginBoxMinSize);
    }
  }

  const auto& maxBSizeCoord = stylePos->MaxBSize(aWM);

  if (!nsLayoutUtils::IsAutoBSize(maxBSizeCoord, aCBSize.BSize(aWM)) &&
      !(isFlexItem && flexMainAxis == eLogicalAxisBlock)) {
    maxBSize = nsLayoutUtils::ComputeBSizeValue(
        aCBSize.BSize(aWM), boxSizingAdjust.BSize(aWM),
        maxBSizeCoord.AsLengthPercentage());
  } else {
    maxBSize = nscoord_MAX;
  }

  const auto& minBSizeCoord = stylePos->MinBSize(aWM);

  if (!nsLayoutUtils::IsAutoBSize(minBSizeCoord, aCBSize.BSize(aWM)) &&
      !(isFlexItem && flexMainAxis == eLogicalAxisBlock)) {
    minBSize = nsLayoutUtils::ComputeBSizeValue(
        aCBSize.BSize(aWM), boxSizingAdjust.BSize(aWM),
        minBSizeCoord.AsLengthPercentage());
  } else {
    minBSize = 0;
  }

  NS_ASSERTION(aCBSize.ISize(aWM) != NS_UNCONSTRAINEDSIZE,
               "Our containing block must not have unconstrained inline-size!");

  // Now calculate the used values for iSize and bSize:

  if (isAutoISize) {
    if (isAutoBSize) {
      // 'auto' iSize, 'auto' bSize

      // Get tentative values - CSS 2.1 sections 10.3.2 and 10.6.2:

      nscoord tentISize, tentBSize;

      if (hasIntrinsicISize) {
        tentISize = intrinsicISize;
      } else if (hasIntrinsicBSize && logicalRatio) {
        tentISize = logicalRatio.ApplyTo(intrinsicBSize);
      } else if (logicalRatio) {
        tentISize =
            aCBSize.ISize(aWM) - boxSizingToMarginEdgeISize;  // XXX scrollbar?
        if (tentISize < 0) tentISize = 0;
      } else {
        tentISize = nsPresContext::CSSPixelsToAppUnits(300);
      }

      // If we need to clamp the inline size to fit the CB, we use the 'stretch'
      // or 'normal' codepath.  We use the ratio-preserving 'normal' codepath
      // unless we have 'stretch' in the other axis.
      if ((aFlags & ComputeSizeFlags::eIClampMarginBoxMinSize) &&
          stretchI != eStretch && tentISize > iSize) {
        stretchI = (stretchB == eStretch ? eStretch : eStretchPreservingRatio);
      }

      if (hasIntrinsicBSize) {
        tentBSize = intrinsicBSize;
      } else if (logicalRatio) {
        tentBSize = logicalRatio.Inverted().ApplyTo(tentISize);
      } else {
        tentBSize = nsPresContext::CSSPixelsToAppUnits(150);
      }

      // (ditto the comment about clamping the inline size above)
      if ((aFlags & ComputeSizeFlags::eBClampMarginBoxMinSize) &&
          stretchB != eStretch && tentBSize > bSize) {
        stretchB = (stretchI == eStretch ? eStretch : eStretchPreservingRatio);
      }

      if (logicalRatio) {
        if (stretchI == eStretch) {
          tentISize = iSize;  // * / 'stretch'
          if (stretchB == eStretch) {
            tentBSize = bSize;  // 'stretch' / 'stretch'
          } else if (stretchB == eStretchPreservingRatio) {
            // 'normal' / 'stretch'
            tentBSize = logicalRatio.Inverted().ApplyTo(iSize);
          }
        } else if (stretchB == eStretch) {
          tentBSize = bSize;  // 'stretch' / * (except 'stretch')
          if (stretchI == eStretchPreservingRatio) {
            // 'stretch' / 'normal'
            tentISize = logicalRatio.ApplyTo(bSize);
          }
        } else if (stretchI == eStretchPreservingRatio) {
          tentISize = iSize;  // * (except 'stretch') / 'normal'
          tentBSize = logicalRatio.Inverted().ApplyTo(iSize);
          if (stretchB == eStretchPreservingRatio && tentBSize > bSize) {
            // Stretch within the CB size with preserved intrinsic ratio.
            tentBSize = bSize;  // 'normal' / 'normal'
            tentISize = logicalRatio.ApplyTo(bSize);
          }
        } else if (stretchB == eStretchPreservingRatio) {
          tentBSize = bSize;  // 'normal' / * (except 'normal' and 'stretch')
          tentISize = logicalRatio.ApplyTo(bSize);
        }
      }

      // ComputeAutoSizeWithIntrinsicDimensions preserves the ratio when
      // applying the min/max-size.  We don't want that when we have 'stretch'
      // in either axis because tentISize/tentBSize is likely not according to
      // ratio now.
      if (logicalRatio && stretchI != eStretch && stretchB != eStretch) {
        nsSize autoSize = nsLayoutUtils::ComputeAutoSizeWithIntrinsicDimensions(
            minISize, minBSize, maxISize, maxBSize, tentISize, tentBSize);
        // The nsSize that ComputeAutoSizeWithIntrinsicDimensions returns will
        // actually contain logical values if the parameters passed to it were
        // logical coordinates, so we do NOT perform a physical-to-logical
        // conversion here, but just assign the fields directly to our result.
        iSize = autoSize.width;
        bSize = autoSize.height;
      } else {
        // Not honoring an intrinsic ratio: clamp the dimensions independently.
        iSize = NS_CSS_MINMAX(tentISize, minISize, maxISize);
        bSize = NS_CSS_MINMAX(tentBSize, minBSize, maxBSize);
      }
    } else {
      // 'auto' iSize, non-'auto' bSize
      bSize = NS_CSS_MINMAX(bSize, minBSize, maxBSize);
      if (stretchI != eStretch) {
        if (logicalRatio) {
          iSize = logicalRatio.ApplyTo(bSize);
        } else if (hasIntrinsicISize) {
          if (!((aFlags & ComputeSizeFlags::eIClampMarginBoxMinSize) &&
                intrinsicISize > iSize)) {
            iSize = intrinsicISize;
          }  // else - leave iSize as is to fill the CB
        } else {
          iSize = nsPresContext::CSSPixelsToAppUnits(300);
        }
      }  // else - leave iSize as is to fill the CB
      iSize = NS_CSS_MINMAX(iSize, minISize, maxISize);
    }
  } else {
    if (isAutoBSize) {
      // non-'auto' iSize, 'auto' bSize
      iSize = NS_CSS_MINMAX(iSize, minISize, maxISize);
      if (stretchB != eStretch) {
        if (logicalRatio) {
          bSize = logicalRatio.Inverted().ApplyTo(iSize);
        } else if (hasIntrinsicBSize) {
          if (!((aFlags & ComputeSizeFlags::eBClampMarginBoxMinSize) &&
                intrinsicBSize > bSize)) {
            bSize = intrinsicBSize;
          }  // else - leave bSize as is to fill the CB
        } else {
          bSize = nsPresContext::CSSPixelsToAppUnits(150);
        }
      }  // else - leave bSize as is to fill the CB
      bSize = NS_CSS_MINMAX(bSize, minBSize, maxBSize);

    } else {
      // non-'auto' iSize, non-'auto' bSize
      iSize = NS_CSS_MINMAX(iSize, minISize, maxISize);
      bSize = NS_CSS_MINMAX(bSize, minBSize, maxBSize);
    }
  }

  return LogicalSize(aWM, iSize, bSize);
}

nsRect nsIFrame::ComputeTightBounds(DrawTarget* aDrawTarget) const {
  return GetVisualOverflowRect();
}

nsRect nsFrame::ComputeSimpleTightBounds(DrawTarget* aDrawTarget) const {
  if (StyleOutline()->ShouldPaintOutline() || StyleBorder()->HasBorder() ||
      !StyleBackground()->IsTransparent(this) ||
      StyleDisplay()->HasAppearance()) {
    // Not necessarily tight, due to clipping, negative
    // outline-offset, and lots of other issues, but that's OK
    return GetVisualOverflowRect();
  }

  nsRect r(0, 0, 0, 0);
  ChildListIterator lists(this);
  for (; !lists.IsDone(); lists.Next()) {
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      nsIFrame* child = childFrames.get();
      r.UnionRect(
          r, child->ComputeTightBounds(aDrawTarget) + child->GetPosition());
    }
  }
  return r;
}

/* virtual */
nsresult nsIFrame::GetPrefWidthTightBounds(gfxContext* aContext, nscoord* aX,
                                           nscoord* aXMost) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* virtual */
LogicalSize nsFrame::ComputeAutoSize(
    gfxContext* aRenderingContext, WritingMode aWM,
    const mozilla::LogicalSize& aCBSize, nscoord aAvailableISize,
    const mozilla::LogicalSize& aMargin, const mozilla::LogicalSize& aBorder,
    const mozilla::LogicalSize& aPadding, ComputeSizeFlags aFlags) {
  // Use basic shrink-wrapping as a default implementation.
  LogicalSize result(aWM, 0xdeadbeef, NS_UNCONSTRAINEDSIZE);

  // don't bother setting it if the result won't be used
  if (StylePosition()->ISize(aWM).IsAuto()) {
    nscoord availBased = aAvailableISize - aMargin.ISize(aWM) -
                         aBorder.ISize(aWM) - aPadding.ISize(aWM);
    result.ISize(aWM) = ShrinkWidthToFit(aRenderingContext, availBased, aFlags);
  }
  return result;
}

nscoord nsFrame::ShrinkWidthToFit(gfxContext* aRenderingContext,
                                  nscoord aISizeInCB, ComputeSizeFlags aFlags) {
  // If we're a container for font size inflation, then shrink
  // wrapping inside of us should not apply font size inflation.
  AutoMaybeDisableFontInflation an(this);

  nscoord result;
  nscoord minISize = GetMinISize(aRenderingContext);
  if (minISize > aISizeInCB) {
    const bool clamp = aFlags & ComputeSizeFlags::eIClampMarginBoxMinSize;
    result = MOZ_UNLIKELY(clamp) ? aISizeInCB : minISize;
  } else {
    nscoord prefISize = GetPrefISize(aRenderingContext);
    if (prefISize > aISizeInCB) {
      result = aISizeInCB;
    } else {
      result = prefISize;
    }
  }
  return result;
}

nscoord nsIFrame::ComputeISizeValue(gfxContext* aRenderingContext,
                                    nscoord aContainingBlockISize,
                                    nscoord aContentEdgeToBoxSizing,
                                    nscoord aBoxSizingToMarginEdge,
                                    StyleExtremumLength aSize,
                                    ComputeSizeFlags aFlags) {
  // If 'this' is a container for font size inflation, then shrink
  // wrapping inside of it should not apply font size inflation.
  AutoMaybeDisableFontInflation an(this);
  nscoord result;
  switch (aSize) {
    case StyleExtremumLength::MaxContent:
      result = GetPrefISize(aRenderingContext);
      NS_ASSERTION(result >= 0, "inline-size less than zero");
      return result;
    case StyleExtremumLength::MinContent:
      result = GetMinISize(aRenderingContext);
      NS_ASSERTION(result >= 0, "inline-size less than zero");
      if (MOZ_UNLIKELY(aFlags & ComputeSizeFlags::eIClampMarginBoxMinSize)) {
        auto available = aContainingBlockISize -
                         (aBoxSizingToMarginEdge + aContentEdgeToBoxSizing);
        result = std::min(available, result);
      }
      return result;
    case StyleExtremumLength::MozFitContent: {
      nscoord pref = GetPrefISize(aRenderingContext),
              min = GetMinISize(aRenderingContext),
              fill = aContainingBlockISize -
                     (aBoxSizingToMarginEdge + aContentEdgeToBoxSizing);
      if (MOZ_UNLIKELY(aFlags & ComputeSizeFlags::eIClampMarginBoxMinSize)) {
        min = std::min(min, fill);
      }
      result = std::max(min, std::min(pref, fill));
      NS_ASSERTION(result >= 0, "inline-size less than zero");
      return result;
    }
    case StyleExtremumLength::MozAvailable:
      return aContainingBlockISize -
             (aBoxSizingToMarginEdge + aContentEdgeToBoxSizing);
  }
  MOZ_ASSERT_UNREACHABLE("Unknown extremum length?");
  return 0;
}

nscoord nsIFrame::ComputeISizeValue(gfxContext* aRenderingContext,
                                    nscoord aContainingBlockISize,
                                    nscoord aContentEdgeToBoxSizing,
                                    nscoord aBoxSizingToMarginEdge,
                                    const LengthPercentage& aCoord,
                                    ComputeSizeFlags aFlags) {
  MOZ_ASSERT(aRenderingContext, "non-null rendering context expected");
  LAYOUT_WARN_IF_FALSE(
      aContainingBlockISize != NS_UNCONSTRAINEDSIZE,
      "have unconstrained inline-size; this should only result from "
      "very large sizes, not attempts at intrinsic inline-size "
      "calculation");
  MOZ_ASSERT(aContainingBlockISize >= 0, "inline-size less than zero");

  nscoord result = aCoord.Resolve(aContainingBlockISize);
  // The result of a calc() expression might be less than 0; we
  // should clamp at runtime (below).  (Percentages and coords that
  // are less than 0 have already been dropped by the parser.)
  result -= aContentEdgeToBoxSizing;
  return std::max(0, result);
}

void nsFrame::DidReflow(nsPresContext* aPresContext,
                        const ReflowInput* aReflowInput) {
  NS_FRAME_TRACE_MSG(NS_FRAME_TRACE_CALLS, ("nsFrame::DidReflow"));

  SVGObserverUtils::InvalidateDirectRenderingObservers(
      this, SVGObserverUtils::INVALIDATE_REFLOW);

  RemoveStateBits(NS_FRAME_IN_REFLOW | NS_FRAME_FIRST_REFLOW |
                  NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN);

  // Clear state that was used in ReflowInput::InitResizeFlags (see
  // comment there for why we can't clear it there).
  SetHasBSizeChange(false);

  // Notify the percent bsize observer if there is a percent bsize.
  // The observer may be able to initiate another reflow with a computed
  // bsize. This happens in the case where a table cell has no computed
  // bsize but can fabricate one when the cell bsize is known.
  if (aReflowInput && aReflowInput->mPercentBSizeObserver && !GetPrevInFlow()) {
    const auto& bsize =
        aReflowInput->mStylePosition->BSize(aReflowInput->GetWritingMode());
    if (bsize.HasPercent()) {
      aReflowInput->mPercentBSizeObserver->NotifyPercentBSize(*aReflowInput);
    }
  }

  aPresContext->ReflowedFrame();
}

void nsFrame::FinishReflowWithAbsoluteFrames(nsPresContext* aPresContext,
                                             ReflowOutput& aDesiredSize,
                                             const ReflowInput& aReflowInput,
                                             nsReflowStatus& aStatus,
                                             bool aConstrainBSize) {
  ReflowAbsoluteFrames(aPresContext, aDesiredSize, aReflowInput, aStatus,
                       aConstrainBSize);

  FinishAndStoreOverflow(&aDesiredSize, aReflowInput.mStyleDisplay);
}

void nsFrame::ReflowAbsoluteFrames(nsPresContext* aPresContext,
                                   ReflowOutput& aDesiredSize,
                                   const ReflowInput& aReflowInput,
                                   nsReflowStatus& aStatus,
                                   bool aConstrainBSize) {
  if (HasAbsolutelyPositionedChildren()) {
    nsAbsoluteContainingBlock* absoluteContainer = GetAbsoluteContainingBlock();

    // Let the absolutely positioned container reflow any absolutely positioned
    // child frames that need to be reflowed

    // The containing block for the abs pos kids is formed by our padding edge.
    nsMargin usedBorder = GetUsedBorder();
    nscoord containingBlockWidth =
        std::max(0, aDesiredSize.Width() - usedBorder.LeftRight());
    nscoord containingBlockHeight =
        std::max(0, aDesiredSize.Height() - usedBorder.TopBottom());
    nsContainerFrame* container = do_QueryFrame(this);
    NS_ASSERTION(container,
                 "Abs-pos children only supported on container frames for now");

    nsRect containingBlock(0, 0, containingBlockWidth, containingBlockHeight);
    AbsPosReflowFlags flags =
        AbsPosReflowFlags::CBWidthAndHeightChanged;  // XXX could be optimized
    if (aConstrainBSize) {
      flags |= AbsPosReflowFlags::ConstrainHeight;
    }
    absoluteContainer->Reflow(container, aPresContext, aReflowInput, aStatus,
                              containingBlock, flags,
                              &aDesiredSize.mOverflowAreas);
  }
}

void nsFrame::PushDirtyBitToAbsoluteFrames() {
  if (!(GetStateBits() & NS_FRAME_IS_DIRTY)) {
    return;  // No dirty bit to push.
  }
  if (!HasAbsolutelyPositionedChildren()) {
    return;  // No absolute children to push to.
  }
  GetAbsoluteContainingBlock()->MarkAllFramesDirty();
}

/* virtual */
bool nsFrame::CanContinueTextRun() const {
  // By default, a frame will *not* allow a text run to be continued
  // through it.
  return false;
}

void nsFrame::Reflow(nsPresContext* aPresContext, ReflowOutput& aDesiredSize,
                     const ReflowInput& aReflowInput, nsReflowStatus& aStatus) {
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsFrame");
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");
  aDesiredSize.ClearSize();
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aDesiredSize);
}

bool nsIFrame::IsContentDisabled() const {
  // FIXME(emilio): Doing this via CSS means callers must ensure the style is up
  // to date, and they don't!
  if (StyleUI()->mUserInput == StyleUserInput::None) {
    return true;
  }

  auto* element = nsGenericHTMLElement::FromNodeOrNull(GetContent());
  return element && element->IsDisabled();
}

nsresult nsFrame::CharacterDataChanged(const CharacterDataChangeInfo&) {
  MOZ_ASSERT_UNREACHABLE("should only be called for text frames");
  return NS_OK;
}

nsresult nsFrame::AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                   int32_t aModType) {
  return NS_OK;
}

// Flow member functions

nsIFrame* nsFrame::GetPrevContinuation() const { return nullptr; }

void nsFrame::SetPrevContinuation(nsIFrame* aPrevContinuation) {
  MOZ_ASSERT(false, "not splittable");
}

nsIFrame* nsFrame::GetNextContinuation() const { return nullptr; }

void nsFrame::SetNextContinuation(nsIFrame*) {
  MOZ_ASSERT(false, "not splittable");
}

nsIFrame* nsFrame::GetPrevInFlow() const { return nullptr; }

void nsFrame::SetPrevInFlow(nsIFrame* aPrevInFlow) {
  MOZ_ASSERT(false, "not splittable");
}

nsIFrame* nsFrame::GetNextInFlow() const { return nullptr; }

void nsFrame::SetNextInFlow(nsIFrame*) { MOZ_ASSERT(false, "not splittable"); }

nsIFrame* nsIFrame::GetTailContinuation() {
  nsIFrame* frame = this;
  while (frame->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER) {
    frame = frame->GetPrevContinuation();
    NS_ASSERTION(frame, "first continuation can't be overflow container");
  }
  for (nsIFrame* next = frame->GetNextContinuation();
       next && !(next->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER);
       next = frame->GetNextContinuation()) {
    frame = next;
  }

  MOZ_ASSERT(frame, "illegal state in continuation chain.");
  return frame;
}

// Associated view object
void nsIFrame::SetView(nsView* aView) {
  if (aView) {
    aView->SetFrame(this);

#ifdef DEBUG
    LayoutFrameType frameType = Type();
    NS_ASSERTION(frameType == LayoutFrameType::SubDocument ||
                     frameType == LayoutFrameType::ListControl ||
                     frameType == LayoutFrameType::Object ||
                     frameType == LayoutFrameType::Viewport ||
                     frameType == LayoutFrameType::MenuPopup,
                 "Only specific frame types can have an nsView");
#endif

    // Store the view on the frame.
    SetViewInternal(aView);

    // Set the frame state bit that says the frame has a view
    AddStateBits(NS_FRAME_HAS_VIEW);

    // Let all of the ancestors know they have a descendant with a view.
    for (nsIFrame* f = GetParent();
         f && !(f->GetStateBits() & NS_FRAME_HAS_CHILD_WITH_VIEW);
         f = f->GetParent())
      f->AddStateBits(NS_FRAME_HAS_CHILD_WITH_VIEW);
  } else {
    MOZ_ASSERT_UNREACHABLE("Destroying a view while the frame is alive?");
    RemoveStateBits(NS_FRAME_HAS_VIEW);
    SetViewInternal(nullptr);
  }
}

// Find the first geometric parent that has a view
nsIFrame* nsIFrame::GetAncestorWithView() const {
  for (nsIFrame* f = GetParent(); nullptr != f; f = f->GetParent()) {
    if (f->HasView()) {
      return f;
    }
  }
  return nullptr;
}

template <nsPoint (nsIFrame::*PositionGetter)() const>
static nsPoint OffsetCalculator(const nsIFrame* aThis, const nsIFrame* aOther) {
  MOZ_ASSERT(aOther, "Must have frame for destination coordinate system!");

  NS_ASSERTION(aThis->PresContext() == aOther->PresContext(),
               "GetOffsetTo called on frames in different documents");

  nsPoint offset(0, 0);
  const nsIFrame* f;
  for (f = aThis; f != aOther && f; f = f->GetParent()) {
    offset += (f->*PositionGetter)();
  }

  if (f != aOther) {
    // Looks like aOther wasn't an ancestor of |this|.  So now we have
    // the root-frame-relative position of |this| in |offset|.  Convert back
    // to the coordinates of aOther
    while (aOther) {
      offset -= (aOther->*PositionGetter)();
      aOther = aOther->GetParent();
    }
  }

  return offset;
}

nsPoint nsIFrame::GetOffsetTo(const nsIFrame* aOther) const {
  return OffsetCalculator<&nsIFrame::GetPosition>(this, aOther);
}

nsPoint nsIFrame::GetOffsetToIgnoringScrolling(const nsIFrame* aOther) const {
  return OffsetCalculator<&nsIFrame::GetPositionIgnoringScrolling>(this,
                                                                   aOther);
}

nsPoint nsIFrame::GetOffsetToCrossDoc(const nsIFrame* aOther) const {
  return GetOffsetToCrossDoc(aOther, PresContext()->AppUnitsPerDevPixel());
}

nsPoint nsIFrame::GetOffsetToCrossDoc(const nsIFrame* aOther,
                                      const int32_t aAPD) const {
  MOZ_ASSERT(aOther, "Must have frame for destination coordinate system!");
  NS_ASSERTION(PresContext()->GetRootPresContext() ==
                   aOther->PresContext()->GetRootPresContext(),
               "trying to get the offset between frames in different document "
               "hierarchies?");
  if (PresContext()->GetRootPresContext() !=
      aOther->PresContext()->GetRootPresContext()) {
    // crash right away, we are almost certainly going to crash anyway.
    MOZ_CRASH(
        "trying to get the offset between frames in different "
        "document hierarchies?");
  }

  const nsIFrame* root = nullptr;
  // offset will hold the final offset
  // docOffset holds the currently accumulated offset at the current APD, it
  // will be converted and added to offset when the current APD changes.
  nsPoint offset(0, 0), docOffset(0, 0);
  const nsIFrame* f = this;
  int32_t currAPD = PresContext()->AppUnitsPerDevPixel();
  while (f && f != aOther) {
    docOffset += f->GetPosition();
    nsIFrame* parent = f->GetParent();
    if (parent) {
      f = parent;
    } else {
      nsPoint newOffset(0, 0);
      root = f;
      f = nsLayoutUtils::GetCrossDocParentFrame(f, &newOffset);
      int32_t newAPD = f ? f->PresContext()->AppUnitsPerDevPixel() : 0;
      if (!f || newAPD != currAPD) {
        // Convert docOffset to the right APD and add it to offset.
        offset += docOffset.ScaleToOtherAppUnits(currAPD, aAPD);
        docOffset.x = docOffset.y = 0;
      }
      currAPD = newAPD;
      docOffset += newOffset;
    }
  }
  if (f == aOther) {
    offset += docOffset.ScaleToOtherAppUnits(currAPD, aAPD);
  } else {
    // Looks like aOther wasn't an ancestor of |this|.  So now we have
    // the root-document-relative position of |this| in |offset|. Subtract the
    // root-document-relative position of |aOther| from |offset|.
    // This call won't try to recurse again because root is an ancestor of
    // aOther.
    nsPoint negOffset = aOther->GetOffsetToCrossDoc(root, aAPD);
    offset -= negOffset;
  }

  return offset;
}

CSSIntRect nsIFrame::GetScreenRect() const {
  return CSSIntRect::FromAppUnitsToNearest(GetScreenRectInAppUnits());
}

nsRect nsIFrame::GetScreenRectInAppUnits() const {
  nsPresContext* presContext = PresContext();
  nsIFrame* rootFrame = presContext->PresShell()->GetRootFrame();
  nsPoint rootScreenPos(0, 0);
  nsPoint rootFrameOffsetInParent(0, 0);
  nsIFrame* rootFrameParent = nsLayoutUtils::GetCrossDocParentFrame(
      rootFrame, &rootFrameOffsetInParent);
  if (rootFrameParent) {
    nsRect parentScreenRectAppUnits =
        rootFrameParent->GetScreenRectInAppUnits();
    nsPresContext* parentPresContext = rootFrameParent->PresContext();
    double parentScale = double(presContext->AppUnitsPerDevPixel()) /
                         parentPresContext->AppUnitsPerDevPixel();
    nsPoint rootPt =
        parentScreenRectAppUnits.TopLeft() + rootFrameOffsetInParent;
    rootScreenPos.x = NS_round(parentScale * rootPt.x);
    rootScreenPos.y = NS_round(parentScale * rootPt.y);
  } else {
    nsCOMPtr<nsIWidget> rootWidget;
    presContext->PresShell()->GetViewManager()->GetRootWidget(
        getter_AddRefs(rootWidget));
    if (rootWidget) {
      LayoutDeviceIntPoint rootDevPx = rootWidget->WidgetToScreenOffset();
      rootScreenPos.x = presContext->DevPixelsToAppUnits(rootDevPx.x);
      rootScreenPos.y = presContext->DevPixelsToAppUnits(rootDevPx.y);
    }
  }

  return nsRect(rootScreenPos + GetOffsetTo(rootFrame), GetSize());
}

// Returns the offset from this frame to the closest geometric parent that
// has a view. Also returns the containing view or null in case of error
void nsIFrame::GetOffsetFromView(nsPoint& aOffset, nsView** aView) const {
  MOZ_ASSERT(nullptr != aView, "null OUT parameter pointer");
  nsIFrame* frame = const_cast<nsIFrame*>(this);

  *aView = nullptr;
  aOffset.MoveTo(0, 0);
  do {
    aOffset += frame->GetPosition();
    frame = frame->GetParent();
  } while (frame && !frame->HasView());

  if (frame) {
    *aView = frame->GetView();
  }
}

nsIWidget* nsIFrame::GetNearestWidget() const {
  return GetClosestView()->GetNearestWidget(nullptr);
}

nsIWidget* nsIFrame::GetNearestWidget(nsPoint& aOffset) const {
  nsPoint offsetToView;
  nsPoint offsetToWidget;
  nsIWidget* widget =
      GetClosestView(&offsetToView)->GetNearestWidget(&offsetToWidget);
  aOffset = offsetToView + offsetToWidget;
  return widget;
}

Matrix4x4Flagged nsIFrame::GetTransformMatrix(const nsIFrame* aStopAtAncestor,
                                              nsIFrame** aOutAncestor,
                                              uint32_t aFlags) const {
  MOZ_ASSERT(aOutAncestor, "Need a place to put the ancestor!");

  /* If we're transformed, we want to hand back the combination
   * transform/translate matrix that will apply our current transform, then
   * shift us to our parent.
   */
  if (IsTransformed()) {
    /* Compute the delta to the parent, which we need because we are converting
     * coordinates to our parent.
     */
    NS_ASSERTION(nsLayoutUtils::GetCrossDocParentFrame(this),
                 "Cannot transform the viewport frame!");
    int32_t scaleFactor =
        ((aFlags & IN_CSS_UNITS) ? AppUnitsPerCSSPixel()
                                 : PresContext()->AppUnitsPerDevPixel());

    Matrix4x4 result = nsDisplayTransform::GetResultingTransformMatrix(
        this, nsPoint(0, 0), scaleFactor,
        nsDisplayTransform::INCLUDE_PERSPECTIVE |
            nsDisplayTransform::OFFSET_BY_ORIGIN);
    *aOutAncestor = nsLayoutUtils::GetCrossDocParentFrame(this);
    nsPoint delta = GetOffsetToCrossDoc(*aOutAncestor);
    /* Combine the raw transform with a translation to our parent. */
    result.PostTranslate(NSAppUnitsToFloatPixels(delta.x, scaleFactor),
                         NSAppUnitsToFloatPixels(delta.y, scaleFactor), 0.0f);

    return result;
  }

  if (nsLayoutUtils::IsPopup(this) && IsListControlFrame()) {
    nsPresContext* presContext = PresContext();
    nsIFrame* docRootFrame = presContext->PresShell()->GetRootFrame();

    // Compute a matrix that transforms from the popup widget to the toplevel
    // widget. We use the widgets because they're the simplest and most
    // accurate approach --- this should work no matter how the widget position
    // was chosen.
    nsIWidget* widget = GetView()->GetWidget();
    nsPresContext* rootPresContext = PresContext()->GetRootPresContext();
    // Maybe the widget hasn't been created yet? Popups without widgets are
    // treated as regular frames. That should work since they'll be rendered
    // as part of the page if they're rendered at all.
    if (widget && rootPresContext) {
      nsIWidget* toplevel = rootPresContext->GetNearestWidget();
      if (toplevel) {
        LayoutDeviceIntRect screenBounds = widget->GetClientBounds();
        LayoutDeviceIntRect toplevelScreenBounds = toplevel->GetClientBounds();
        LayoutDeviceIntPoint translation =
            screenBounds.TopLeft() - toplevelScreenBounds.TopLeft();

        Matrix4x4 transformToTop;
        transformToTop._41 = translation.x;
        transformToTop._42 = translation.y;

        *aOutAncestor = docRootFrame;
        Matrix4x4 docRootTransformToTop =
            nsLayoutUtils::GetTransformToAncestor(docRootFrame, nullptr)
                .GetMatrix();
        if (docRootTransformToTop.IsSingular()) {
          NS_WARNING(
              "Containing document is invisible, we can't compute a valid "
              "transform");
        } else {
          docRootTransformToTop.Invert();
          return transformToTop * docRootTransformToTop;
        }
      }
    }
  }

  *aOutAncestor = nsLayoutUtils::GetCrossDocParentFrame(this);

  /* Otherwise, we're not transformed.  In that case, we'll walk up the frame
   * tree until we either hit the root frame or something that may be
   * transformed.  We'll then change coordinates into that frame, since we're
   * guaranteed that nothing in-between can be transformed.  First, however,
   * we have to check to see if we have a parent.  If not, we'll set the
   * outparam to null (indicating that there's nothing left) and will hand back
   * the identity matrix.
   */
  if (!*aOutAncestor) return Matrix4x4();

  /* Keep iterating while the frame can't possibly be transformed. */
  const nsIFrame* current = this;
  while (!(*aOutAncestor)->IsTransformed() &&
         !nsLayoutUtils::IsPopup(*aOutAncestor) &&
         *aOutAncestor != aStopAtAncestor &&
         (!(aFlags & STOP_AT_STACKING_CONTEXT_AND_DISPLAY_PORT) ||
          (!(*aOutAncestor)->IsStackingContext() &&
           !nsLayoutUtils::FrameHasDisplayPort(*aOutAncestor, current)))) {
    /* If no parent, stop iterating.  Otherwise, update the ancestor. */
    nsIFrame* parent = nsLayoutUtils::GetCrossDocParentFrame(*aOutAncestor);
    if (!parent) break;

    current = *aOutAncestor;
    *aOutAncestor = parent;
  }

  NS_ASSERTION(*aOutAncestor, "Somehow ended up with a null ancestor...?");

  /* Translate from this frame to our ancestor, if it exists.  That's the
   * entire transform, so we're done.
   */
  nsPoint delta = GetOffsetToCrossDoc(*aOutAncestor);
  int32_t scaleFactor =
      ((aFlags & IN_CSS_UNITS) ? AppUnitsPerCSSPixel()
                               : PresContext()->AppUnitsPerDevPixel());
  return Matrix4x4::Translation(NSAppUnitsToFloatPixels(delta.x, scaleFactor),
                                NSAppUnitsToFloatPixels(delta.y, scaleFactor),
                                0.0f);
}

static void InvalidateRenderingObservers(nsIFrame* aDisplayRoot,
                                         nsIFrame* aFrame,
                                         bool aFrameChanged = true) {
  MOZ_ASSERT(aDisplayRoot == nsLayoutUtils::GetDisplayRootFrame(aFrame));
  SVGObserverUtils::InvalidateDirectRenderingObservers(aFrame);
  nsIFrame* parent = aFrame;
  while (parent != aDisplayRoot &&
         (parent = nsLayoutUtils::GetCrossDocParentFrame(parent)) &&
         !parent->HasAnyStateBits(NS_FRAME_DESCENDANT_NEEDS_PAINT)) {
    SVGObserverUtils::InvalidateDirectRenderingObservers(parent);
  }

  if (!aFrameChanged) {
    return;
  }

  aFrame->MarkNeedsDisplayItemRebuild();
}

static void SchedulePaintInternal(
    nsIFrame* aDisplayRoot, nsIFrame* aFrame,
    nsIFrame::PaintType aType = nsIFrame::PAINT_DEFAULT) {
  MOZ_ASSERT(aDisplayRoot == nsLayoutUtils::GetDisplayRootFrame(aFrame));
  nsPresContext* pres = aDisplayRoot->PresContext()->GetRootPresContext();

  // No need to schedule a paint for an external document since they aren't
  // painted directly.
  if (!pres || (pres->Document() && pres->Document()->IsResourceDoc())) {
    return;
  }
  if (!pres->GetContainerWeak()) {
    NS_WARNING("Shouldn't call SchedulePaint in a detached pres context");
    return;
  }

  pres->PresShell()->ScheduleViewManagerFlush(
      aType == nsIFrame::PAINT_DELAYED_COMPRESS ? PaintType::DelayedCompress
                                                : PaintType::Default);

  if (aType == nsIFrame::PAINT_DELAYED_COMPRESS) {
    return;
  }

  if (aType == nsIFrame::PAINT_DEFAULT) {
    aDisplayRoot->AddStateBits(NS_FRAME_UPDATE_LAYER_TREE);
  }
}

static void InvalidateFrameInternal(nsIFrame* aFrame, bool aHasDisplayItem,
                                    bool aRebuildDisplayItems) {
  if (aHasDisplayItem) {
    aFrame->AddStateBits(NS_FRAME_NEEDS_PAINT);
  }

  if (aRebuildDisplayItems) {
    aFrame->MarkNeedsDisplayItemRebuild();
  }
  SVGObserverUtils::InvalidateDirectRenderingObservers(aFrame);
  bool needsSchedulePaint = false;
  if (nsLayoutUtils::IsPopup(aFrame)) {
    needsSchedulePaint = true;
  } else {
    nsIFrame* parent = nsLayoutUtils::GetCrossDocParentFrame(aFrame);
    while (parent &&
           !parent->HasAnyStateBits(NS_FRAME_DESCENDANT_NEEDS_PAINT)) {
      if (aHasDisplayItem && !parent->HasAnyStateBits(NS_FRAME_IS_NONDISPLAY)) {
        parent->AddStateBits(NS_FRAME_DESCENDANT_NEEDS_PAINT);
      }
      SVGObserverUtils::InvalidateDirectRenderingObservers(parent);

      // If we're inside a popup, then we need to make sure that we
      // call schedule paint so that the NS_FRAME_UPDATE_LAYER_TREE
      // flag gets added to the popup display root frame.
      if (nsLayoutUtils::IsPopup(parent)) {
        needsSchedulePaint = true;
        break;
      }
      parent = nsLayoutUtils::GetCrossDocParentFrame(parent);
    }
    if (!parent) {
      needsSchedulePaint = true;
    }
  }
  if (!aHasDisplayItem) {
    return;
  }
  if (needsSchedulePaint) {
    nsIFrame* displayRoot = nsLayoutUtils::GetDisplayRootFrame(aFrame);
    SchedulePaintInternal(displayRoot, aFrame);
  }
  if (aFrame->HasAnyStateBits(NS_FRAME_HAS_INVALID_RECT)) {
    aFrame->RemoveProperty(nsIFrame::InvalidationRect());
    aFrame->RemoveStateBits(NS_FRAME_HAS_INVALID_RECT);
  }
}

void nsIFrame::InvalidateFrameSubtree(bool aRebuildDisplayItems /* = true */) {
  InvalidateFrame(0, aRebuildDisplayItems);

  if (HasAnyStateBits(NS_FRAME_ALL_DESCENDANTS_NEED_PAINT)) {
    return;
  }

  AddStateBits(NS_FRAME_ALL_DESCENDANTS_NEED_PAINT);

  AutoTArray<nsIFrame::ChildList, 4> childListArray;
  GetCrossDocChildLists(&childListArray);

  nsIFrame::ChildListArrayIterator lists(childListArray);
  for (; !lists.IsDone(); lists.Next()) {
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      // Don't explicitly rebuild display items for our descendants,
      // since we should be marked and it implicitly includes all
      // descendants.
      childFrames.get()->InvalidateFrameSubtree(false);
    }
  }
}

void nsIFrame::ClearInvalidationStateBits() {
  if (HasAnyStateBits(NS_FRAME_DESCENDANT_NEEDS_PAINT)) {
    AutoTArray<nsIFrame::ChildList, 4> childListArray;
    GetCrossDocChildLists(&childListArray);

    nsIFrame::ChildListArrayIterator lists(childListArray);
    for (; !lists.IsDone(); lists.Next()) {
      nsFrameList::Enumerator childFrames(lists.CurrentList());
      for (; !childFrames.AtEnd(); childFrames.Next()) {
        childFrames.get()->ClearInvalidationStateBits();
      }
    }
  }

  RemoveStateBits(NS_FRAME_NEEDS_PAINT | NS_FRAME_DESCENDANT_NEEDS_PAINT |
                  NS_FRAME_ALL_DESCENDANTS_NEED_PAINT);
}

void nsIFrame::InvalidateFrame(uint32_t aDisplayItemKey,
                               bool aRebuildDisplayItems /* = true */) {
  bool hasDisplayItem =
      !aDisplayItemKey ||
      FrameLayerBuilder::HasRetainedDataFor(this, aDisplayItemKey);
  InvalidateFrameInternal(this, hasDisplayItem, aRebuildDisplayItems);
}

void nsIFrame::InvalidateFrameWithRect(const nsRect& aRect,
                                       uint32_t aDisplayItemKey,
                                       bool aRebuildDisplayItems /* = true */) {
  if (aRect.IsEmpty()) {
    return;
  }
  bool hasDisplayItem =
      !aDisplayItemKey ||
      FrameLayerBuilder::HasRetainedDataFor(this, aDisplayItemKey);
  bool alreadyInvalid = false;
  if (!HasAnyStateBits(NS_FRAME_NEEDS_PAINT)) {
    InvalidateFrameInternal(this, hasDisplayItem, aRebuildDisplayItems);
  } else {
    alreadyInvalid = true;
  }

  if (!hasDisplayItem) {
    return;
  }

  nsRect* rect;
  if (HasAnyStateBits(NS_FRAME_HAS_INVALID_RECT)) {
    rect = GetProperty(InvalidationRect());
    MOZ_ASSERT(rect);
  } else {
    if (alreadyInvalid) {
      return;
    }
    rect = new nsRect();
    AddProperty(InvalidationRect(), rect);
    AddStateBits(NS_FRAME_HAS_INVALID_RECT);
  }

  *rect = rect->Union(aRect);
}

/*static*/
uint8_t nsIFrame::sLayerIsPrerenderedDataKey;

static bool DoesLayerHaveOutOfDateFrameMetrics(Layer* aLayer) {
  for (uint32_t i = 0; i < aLayer->GetScrollMetadataCount(); i++) {
    const FrameMetrics& metrics = aLayer->GetFrameMetrics(i);
    if (!metrics.IsScrollable()) {
      continue;
    }
    nsIScrollableFrame* scrollableFrame =
        nsLayoutUtils::FindScrollableFrameFor(metrics.GetScrollId());
    if (!scrollableFrame) {
      // This shouldn't happen, so let's do the safe thing and trigger a full
      // paint if it does.
      return true;
    }
    nsPoint scrollPosition = scrollableFrame->GetScrollPosition();
    if (metrics.GetScrollOffset() != CSSPoint::FromAppUnits(scrollPosition)) {
      return true;
    }
  }
  return false;
}

static bool DoesLayerOrAncestorsHaveOutOfDateFrameMetrics(Layer* aLayer) {
  for (Layer* layer = aLayer; layer; layer = layer->GetParent()) {
    if (DoesLayerHaveOutOfDateFrameMetrics(layer)) {
      return true;
    }
  }
  return false;
}

bool nsIFrame::TryUpdateTransformOnly(Layer** aLayerResult) {
  // If we move a transformed layer when we have a merged display
  // list, then it can end up intersecting other items for which
  // we don't have a defined ordering.
  // We could allow this if the display list is in the canonical
  // ordering (correctly sorted for all intersections), but we
  // don't have a way to check that yet.
  if (nsLayoutUtils::AreRetainedDisplayListsEnabled()) {
    return false;
  }

  Layer* layer = FrameLayerBuilder::GetDedicatedLayer(
      this, DisplayItemType::TYPE_TRANSFORM);
  if (!layer || !layer->HasUserData(LayerIsPrerenderedDataKey())) {
    // If this layer isn't prerendered or we clip composites to our OS
    // window, then we can't correctly optimize to an empty
    // transaction in general.
    return false;
  }

  if (DoesLayerOrAncestorsHaveOutOfDateFrameMetrics(layer)) {
    // At least one scroll frame that can affect the position of this layer
    // has changed its scroll offset since the last paint. Schedule a full
    // paint to make sure that this layer's transform and all the frame
    // metrics that affect it are in sync.
    return false;
  }

  gfx::Matrix4x4Flagged transform3d;
  if (!nsLayoutUtils::GetLayerTransformForFrame(this, &transform3d)) {
    // We're not able to compute a layer transform that we know would
    // be used at the next layers transaction, so we can't only update
    // the transform and will need to schedule an invalidating paint.
    return false;
  }
  gfx::Matrix transform;
  gfx::Matrix previousTransform;
  // FIXME/bug 796690 and 796705: in general, changes to 3D
  // transforms, or transform changes to properties other than
  // translation, may lead us to choose a different rendering
  // resolution for our layer.  So if the transform is 3D or has a
  // non-translation change, bail and schedule an invalidating paint.
  // (We can often do better than this, for example for scale-down
  // changes.)
  static const gfx::Float kError = 0.0001f;
  if (!transform3d.Is2D(&transform) ||
      !layer->GetBaseTransform().Is2D(&previousTransform) ||
      !gfx::FuzzyEqual(transform._11, previousTransform._11, kError) ||
      !gfx::FuzzyEqual(transform._22, previousTransform._22, kError) ||
      !gfx::FuzzyEqual(transform._21, previousTransform._21, kError) ||
      !gfx::FuzzyEqual(transform._12, previousTransform._12, kError)) {
    return false;
  }
  layer->SetBaseTransformForNextTransaction(transform3d.GetMatrix());
  *aLayerResult = layer;
  return true;
}

bool nsIFrame::IsInvalid(nsRect& aRect) {
  if (!HasAnyStateBits(NS_FRAME_NEEDS_PAINT)) {
    return false;
  }

  if (HasAnyStateBits(NS_FRAME_HAS_INVALID_RECT)) {
    nsRect* rect = GetProperty(InvalidationRect());
    NS_ASSERTION(
        rect, "Must have an invalid rect if NS_FRAME_HAS_INVALID_RECT is set!");
    aRect = *rect;
  } else {
    aRect.SetEmpty();
  }
  return true;
}

void nsIFrame::SchedulePaint(PaintType aType, bool aFrameChanged) {
  nsIFrame* displayRoot = nsLayoutUtils::GetDisplayRootFrame(this);
  InvalidateRenderingObservers(displayRoot, this, aFrameChanged);
  SchedulePaintInternal(displayRoot, this, aType);
}

void nsIFrame::SchedulePaintWithoutInvalidatingObservers(PaintType aType) {
  nsIFrame* displayRoot = nsLayoutUtils::GetDisplayRootFrame(this);
  SchedulePaintInternal(displayRoot, this, aType);
}

Layer* nsIFrame::InvalidateLayer(DisplayItemType aDisplayItemKey,
                                 const nsIntRect* aDamageRect,
                                 const nsRect* aFrameDamageRect,
                                 uint32_t aFlags /* = 0 */) {
  NS_ASSERTION(aDisplayItemKey > DisplayItemType::TYPE_ZERO, "Need a key");

  Layer* layer = FrameLayerBuilder::GetDedicatedLayer(this, aDisplayItemKey);

  nsIFrame* displayRoot = nsLayoutUtils::GetDisplayRootFrame(this);
  InvalidateRenderingObservers(displayRoot, this, false);

  // Check if frame supports WebRender's async update
  if ((aFlags & UPDATE_IS_ASYNC) &&
      WebRenderUserData::SupportsAsyncUpdate(this)) {
    // WebRender does not use layer, then return nullptr.
    return nullptr;
  }

  // If the layer is being updated asynchronously, and it's being forwarded
  // to a compositor, then we don't need to invalidate.
  if ((aFlags & UPDATE_IS_ASYNC) && layer && layer->SupportsAsyncUpdate()) {
    return layer;
  }

  if (!layer) {
    if (aFrameDamageRect && aFrameDamageRect->IsEmpty()) {
      return nullptr;
    }

    // Plugins can transition from not rendering anything to rendering,
    // and still only call this. So always invalidate, with specifying
    // the display item type just in case.
    //
    // In the bug 930056, dialer app startup but not shown on the
    // screen because sometimes we don't have any retainned data
    // for remote type displayitem and thus Repaint event is not
    // triggered. So, always invalidate here as well.
    DisplayItemType displayItemKey = aDisplayItemKey;
    if (aDisplayItemKey == DisplayItemType::TYPE_PLUGIN ||
        aDisplayItemKey == DisplayItemType::TYPE_REMOTE) {
      displayItemKey = DisplayItemType::TYPE_ZERO;
    }

    if (aFrameDamageRect) {
      InvalidateFrameWithRect(*aFrameDamageRect,
                              static_cast<uint32_t>(displayItemKey));
    } else {
      InvalidateFrame(static_cast<uint32_t>(displayItemKey));
    }

    return nullptr;
  }

  if (aDamageRect && aDamageRect->IsEmpty()) {
    return layer;
  }

  if (aDamageRect) {
    layer->AddInvalidRect(*aDamageRect);
  } else {
    layer->SetInvalidRectToVisibleRegion();
  }

  SchedulePaintInternal(displayRoot, this, PAINT_COMPOSITE_ONLY);
  return layer;
}

static nsRect ComputeEffectsRect(nsIFrame* aFrame, const nsRect& aOverflowRect,
                                 const nsSize& aNewSize) {
  nsRect r = aOverflowRect;

  if (aFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT) {
    // For SVG frames, we only need to account for filters.
    // TODO: We could also take account of clipPath and mask to reduce the
    // visual overflow, but that's not essential.
    if (aFrame->StyleEffects()->HasFilters()) {
      SetOrUpdateRectValuedProperty(aFrame, nsIFrame::PreEffectsBBoxProperty(),
                                    r);
      r = nsSVGUtils::GetPostFilterVisualOverflowRect(aFrame, aOverflowRect);
    }
    return r;
  }

  // box-shadow
  r.UnionRect(r, nsLayoutUtils::GetBoxShadowRectForFrame(aFrame, aNewSize));

  // border-image-outset.
  // We need to include border-image-outset because it can cause the
  // border image to be drawn beyond the border box.

  // (1) It's important we not check whether there's a border-image
  //     since the style hint for a change in border image doesn't cause
  //     reflow, and that's probably more important than optimizing the
  //     overflow areas for the silly case of border-image-outset without
  //     border-image
  // (2) It's important that we not check whether the border-image
  //     is actually loaded, since that would require us to reflow when
  //     the image loads.
  const nsStyleBorder* styleBorder = aFrame->StyleBorder();
  nsMargin outsetMargin = styleBorder->GetImageOutset();

  if (outsetMargin != nsMargin(0, 0, 0, 0)) {
    nsRect outsetRect(nsPoint(0, 0), aNewSize);
    outsetRect.Inflate(outsetMargin);
    r.UnionRect(r, outsetRect);
  }

  // Note that we don't remove the outlineInnerRect if a frame loses outline
  // style. That would require an extra property lookup for every frame,
  // or a new frame state bit to track whether a property had been stored,
  // or something like that. It's not worth doing that here. At most it's
  // only one heap-allocated rect per frame and it will be cleaned up when
  // the frame dies.

  if (nsSVGIntegrationUtils::UsingOverflowAffectingEffects(aFrame)) {
    SetOrUpdateRectValuedProperty(aFrame, nsIFrame::PreEffectsBBoxProperty(),
                                  r);
    r = nsSVGIntegrationUtils::ComputePostEffectsVisualOverflowRect(aFrame, r);
  }

  return r;
}

void nsIFrame::MovePositionBy(const nsPoint& aTranslation) {
  nsPoint position = GetNormalPosition() + aTranslation;

  const nsMargin* computedOffsets = nullptr;
  if (IsRelativelyPositioned()) {
    computedOffsets = GetProperty(nsIFrame::ComputedOffsetProperty());
  }
  ReflowInput::ApplyRelativePositioning(
      this, computedOffsets ? *computedOffsets : nsMargin(), &position);
  SetPosition(position);
}

nsRect nsIFrame::GetNormalRect() const {
  // It might be faster to first check
  // StyleDisplay()->IsRelativelyPositionedStyle().
  nsPoint* normalPosition = GetProperty(NormalPositionProperty());
  if (normalPosition) {
    return nsRect(*normalPosition, GetSize());
  }
  return GetRect();
}

nsPoint nsIFrame::GetPositionIgnoringScrolling() const {
  return GetParent() ? GetParent()->GetPositionOfChildIgnoringScrolling(this)
                     : GetPosition();
}

nsRect nsIFrame::GetOverflowRect(nsOverflowType aType) const {
  MOZ_ASSERT(aType == eVisualOverflow || aType == eScrollableOverflow,
             "unexpected type");

  // Note that in some cases the overflow area might not have been
  // updated (yet) to reflect any outline set on the frame or the area
  // of child frames. That's OK because any reflow that updates these
  // areas will invalidate the appropriate area, so any (mis)uses of
  // this method will be fixed up.

  if (mOverflow.mType == NS_FRAME_OVERFLOW_LARGE) {
    // there is an overflow rect, and it's not stored as deltas but as
    // a separately-allocated rect
    return GetOverflowAreasProperty()->Overflow(aType);
  }

  if (aType == eVisualOverflow && mOverflow.mType != NS_FRAME_OVERFLOW_NONE) {
    return GetVisualOverflowFromDeltas();
  }

  return nsRect(nsPoint(0, 0), GetSize());
}

nsOverflowAreas nsIFrame::GetOverflowAreas() const {
  if (mOverflow.mType == NS_FRAME_OVERFLOW_LARGE) {
    // there is an overflow rect, and it's not stored as deltas but as
    // a separately-allocated rect
    return *GetOverflowAreasProperty();
  }

  return nsOverflowAreas(GetVisualOverflowFromDeltas(),
                         nsRect(nsPoint(0, 0), GetSize()));
}

nsOverflowAreas nsIFrame::GetOverflowAreasRelativeToSelf() const {
  if (IsTransformed()) {
    nsOverflowAreas* preTransformOverflows =
        GetProperty(PreTransformOverflowAreasProperty());
    if (preTransformOverflows) {
      return nsOverflowAreas(preTransformOverflows->VisualOverflow(),
                             preTransformOverflows->ScrollableOverflow());
    }
  }
  return nsOverflowAreas(GetVisualOverflowRect(), GetScrollableOverflowRect());
}

nsRect nsIFrame::GetScrollableOverflowRectRelativeToParent() const {
  return GetScrollableOverflowRect() + mRect.TopLeft();
}

nsRect nsIFrame::GetVisualOverflowRectRelativeToParent() const {
  return GetVisualOverflowRect() + mRect.TopLeft();
}

nsRect nsIFrame::GetScrollableOverflowRectRelativeToSelf() const {
  if (IsTransformed()) {
    nsOverflowAreas* preTransformOverflows =
        GetProperty(PreTransformOverflowAreasProperty());
    if (preTransformOverflows)
      return preTransformOverflows->ScrollableOverflow();
  }
  return GetScrollableOverflowRect();
}

nsRect nsIFrame::GetVisualOverflowRectRelativeToSelf() const {
  if (IsTransformed()) {
    nsOverflowAreas* preTransformOverflows =
        GetProperty(PreTransformOverflowAreasProperty());
    if (preTransformOverflows) return preTransformOverflows->VisualOverflow();
  }
  return GetVisualOverflowRect();
}

nsRect nsIFrame::GetPreEffectsVisualOverflowRect() const {
  nsRect* r = GetProperty(nsIFrame::PreEffectsBBoxProperty());
  return r ? *r : GetVisualOverflowRectRelativeToSelf();
}

bool nsIFrame::UpdateOverflow() {
  MOZ_ASSERT(FrameMaintainsOverflow(),
             "Non-display SVG do not maintain visual overflow rects");

  nsRect rect(nsPoint(0, 0), GetSize());
  nsOverflowAreas overflowAreas(rect, rect);

  if (!ComputeCustomOverflow(overflowAreas)) {
    // If updating overflow wasn't supported by this frame, then it should
    // have scheduled any necessary reflows. We can return false to say nothing
    // changed, and wait for reflow to correct it.
    return false;
  }

  UnionChildOverflow(overflowAreas);

  if (FinishAndStoreOverflow(overflowAreas, GetSize())) {
    nsView* view = GetView();
    if (view) {
      ReflowChildFlags flags = GetXULLayoutFlags();
      if (!(flags & ReflowChildFlags::NoSizeView)) {
        // Make sure the frame's view is properly sized.
        nsViewManager* vm = view->GetViewManager();
        vm->ResizeView(view, overflowAreas.VisualOverflow(), true);
      }
    }

    return true;
  }

  // Frames that combine their 3d transform with their ancestors
  // only compute a pre-transform overflow rect, and then contribute
  // to the normal overflow rect of the preserve-3d root. Always return
  // true here so that we propagate changes up to the root for final
  // calculation.
  return Combines3DTransformWithAncestors();
}

/* virtual */
bool nsFrame::ComputeCustomOverflow(nsOverflowAreas& aOverflowAreas) {
  return true;
}

/* virtual */
void nsFrame::UnionChildOverflow(nsOverflowAreas& aOverflowAreas) {
  if (!DoesClipChildren() &&
      !(IsXULCollapsed() && (IsXULBoxFrame() || ::IsXULBoxWrapped(this)))) {
    nsLayoutUtils::UnionChildOverflow(this, aOverflowAreas);
  }
}

// Define the MAX_FRAME_DEPTH to be the ContentSink's MAX_REFLOW_DEPTH plus
// 4 for the frames above the document's frames:
//  the Viewport, GFXScroll, ScrollPort, and Canvas
#define MAX_FRAME_DEPTH (MAX_REFLOW_DEPTH + 4)

bool nsFrame::IsFrameTreeTooDeep(const ReflowInput& aReflowInput,
                                 ReflowOutput& aMetrics,
                                 nsReflowStatus& aStatus) {
  if (aReflowInput.mReflowDepth > MAX_FRAME_DEPTH) {
    NS_WARNING("frame tree too deep; setting zero size and returning");
    AddStateBits(NS_FRAME_TOO_DEEP_IN_FRAME_TREE);
    ClearOverflowRects();
    aMetrics.ClearSize();
    aMetrics.SetBlockStartAscent(0);
    aMetrics.mCarriedOutBEndMargin.Zero();
    aMetrics.mOverflowAreas.Clear();

    aStatus.Reset();
    if (GetNextInFlow()) {
      // Reflow depth might vary between reflows, so we might have
      // successfully reflowed and split this frame before.  If so, we
      // shouldn't delete its continuations.
      aStatus.SetIncomplete();
    }

    return true;
  }
  RemoveStateBits(NS_FRAME_TOO_DEEP_IN_FRAME_TREE);
  return false;
}

bool nsIFrame::IsBlockWrapper() const {
  auto pseudoType = Style()->GetPseudoType();
  return pseudoType == PseudoStyleType::mozBlockInsideInlineWrapper ||
         pseudoType == PseudoStyleType::buttonContent ||
         pseudoType == PseudoStyleType::cellContent ||
         pseudoType == PseudoStyleType::columnSpanWrapper;
}

bool nsIFrame::IsBlockFrameOrSubclass() const {
  const nsBlockFrame* thisAsBlock = do_QueryFrame(this);
  return !!thisAsBlock;
}

static nsIFrame* GetNearestBlockContainer(nsIFrame* frame) {
  // The block wrappers we use to wrap blocks inside inlines aren't
  // described in the CSS spec.  We need to make them not be containing
  // blocks.
  // Since the parent of such a block is either a normal block or
  // another such pseudo, this shouldn't cause anything bad to happen.
  // Also the anonymous blocks inside table cells are not containing blocks.
  //
  // If we ever start skipping table row groups from being containing blocks,
  // you need to remove the StickyScrollContainer hack referencing bug 1421660.
  while (frame->IsFrameOfType(nsIFrame::eLineParticipant) ||
         frame->IsBlockWrapper() ||
         // Table rows are not containing blocks either
         frame->IsTableRowFrame()) {
    frame = frame->GetParent();
    NS_ASSERTION(
        frame,
        "How come we got to the root frame without seeing a containing block?");
  }
  return frame;
}

nsIFrame* nsIFrame::GetContainingBlock(
    uint32_t aFlags, const nsStyleDisplay* aStyleDisplay) const {
  MOZ_ASSERT(aStyleDisplay == StyleDisplay());
  if (!GetParent()) {
    return nullptr;
  }
  // MathML frames might have absolute positioning style, but they would
  // still be in-flow.  So we have to check to make sure that the frame
  // is really out-of-flow too.
  nsIFrame* f;
  if (IsAbsolutelyPositioned(aStyleDisplay) &&
      (GetStateBits() & NS_FRAME_OUT_OF_FLOW)) {
    f = GetParent();  // the parent is always the containing block
  } else {
    f = GetNearestBlockContainer(GetParent());
  }

  if (aFlags & SKIP_SCROLLED_FRAME && f &&
      f->Style()->GetPseudoType() == PseudoStyleType::scrolledContent) {
    f = f->GetParent();
  }
  return f;
}

#ifdef DEBUG_FRAME_DUMP

int32_t nsFrame::ContentIndexInContainer(const nsIFrame* aFrame) {
  int32_t result = -1;

  nsIContent* content = aFrame->GetContent();
  if (content) {
    nsIContent* parentContent = content->GetParent();
    if (parentContent) {
      result = parentContent->ComputeIndexOf(content);
    }
  }

  return result;
}

/**
 * List a frame tree to stderr. Meant to be called from gdb.
 */
void DebugListFrameTree(nsIFrame* aFrame) { ((nsFrame*)aFrame)->List(stderr); }

nsAutoCString nsIFrame::ListTag() const {
  nsAutoString tmp;
  GetFrameName(tmp);

  nsAutoCString tag;
  tag += NS_ConvertUTF16toUTF8(tmp);
  tag += nsPrintfCString("@%p", static_cast<const void*>(this));
  return tag;
}

std::string nsIFrame::ConvertToString(const LogicalRect& aRect,
                                      const WritingMode aWM, ListFlags aFlags) {
  if (aFlags.contains(ListFlag::DisplayInCSSPixels)) {
    // Abuse CSSRect to store all LogicalRect's dimensions in CSS pixels.
    return ToString(mozilla::CSSRect(CSSPixel::FromAppUnits(aRect.IStart(aWM)),
                                     CSSPixel::FromAppUnits(aRect.BStart(aWM)),
                                     CSSPixel::FromAppUnits(aRect.ISize(aWM)),
                                     CSSPixel::FromAppUnits(aRect.BSize(aWM))));
  }
  return ToString(aRect);
}

std::string nsIFrame::ConvertToString(const LogicalSize& aSize,
                                      const WritingMode aWM, ListFlags aFlags) {
  if (aFlags.contains(ListFlag::DisplayInCSSPixels)) {
    // Abuse CSSSize to store all LogicalSize's dimensions in CSS pixels.
    return ToString(CSSSize(CSSPixel::FromAppUnits(aSize.ISize(aWM)),
                            CSSPixel::FromAppUnits(aSize.BSize(aWM))));
  }
  return ToString(aSize);
}

// Debugging
void nsIFrame::ListGeneric(nsACString& aTo, const char* aPrefix,
                           ListFlags aFlags) const {
  aTo += aPrefix;
  aTo += ListTag();
  if (HasView()) {
    aTo += nsPrintfCString(" [view=%p]", static_cast<void*>(GetView()));
  }
  if (GetParent()) {
    aTo += nsPrintfCString(" parent=%p", static_cast<void*>(GetParent()));
  }
  if (GetNextSibling()) {
    aTo += nsPrintfCString(" next=%p", static_cast<void*>(GetNextSibling()));
  }
  if (GetPrevContinuation()) {
    bool fluid = GetPrevInFlow() == GetPrevContinuation();
    aTo += nsPrintfCString(" prev-%s=%p", fluid ? "in-flow" : "continuation",
                           static_cast<void*>(GetPrevContinuation()));
  }
  if (GetNextContinuation()) {
    bool fluid = GetNextInFlow() == GetNextContinuation();
    aTo += nsPrintfCString(" next-%s=%p", fluid ? "in-flow" : "continuation",
                           static_cast<void*>(GetNextContinuation()));
  }
  void* IBsibling = GetProperty(IBSplitSibling());
  if (IBsibling) {
    aTo += nsPrintfCString(" IBSplitSibling=%p", IBsibling);
  }
  void* IBprevsibling = GetProperty(IBSplitPrevSibling());
  if (IBprevsibling) {
    aTo += nsPrintfCString(" IBSplitPrevSibling=%p", IBprevsibling);
  }
  if (nsLayoutUtils::FontSizeInflationEnabled(PresContext())) {
    if (HasAnyStateBits(NS_FRAME_FONT_INFLATION_FLOW_ROOT)) {
      aTo += nsPrintfCString(" FFR");
      if (nsFontInflationData* data =
              nsFontInflationData::FindFontInflationDataFor(this)) {
        aTo += nsPrintfCString(
            ",enabled=%s,UIS=%s", data->InflationEnabled() ? "yes" : "no",
            ConvertToString(data->UsableISize(), aFlags).c_str());
      }
    }
    if (HasAnyStateBits(NS_FRAME_FONT_INFLATION_CONTAINER)) {
      aTo += nsPrintfCString(" FIC");
    }
    aTo += nsPrintfCString(" FI=%f", nsLayoutUtils::FontSizeInflationFor(this));
  }
  aTo += nsPrintfCString(" %s", ConvertToString(mRect, aFlags).c_str());

  mozilla::WritingMode wm = GetWritingMode();
  if (wm.IsVertical() || wm.IsBidiRTL()) {
    aTo +=
        nsPrintfCString(" wm=%s logical-size=(%s)", ToString(wm).c_str(),
                        ConvertToString(GetLogicalSize(), wm, aFlags).c_str());
  }

  nsIFrame* parent = GetParent();
  if (parent) {
    WritingMode pWM = parent->GetWritingMode();
    if (pWM.IsVertical() || pWM.IsBidiRTL()) {
      nsSize containerSize = parent->mRect.Size();
      LogicalRect lr(pWM, mRect, containerSize);
      aTo += nsPrintfCString(" parent-wm=%s cs=(%s) logical-rect=%s",
                             ToString(pWM).c_str(),
                             ConvertToString(containerSize, aFlags).c_str(),
                             ConvertToString(lr, pWM, aFlags).c_str());
    }
  }
  nsIFrame* f = const_cast<nsIFrame*>(this);
  if (f->HasOverflowAreas()) {
    nsRect vo = f->GetVisualOverflowRect();
    if (!vo.IsEqualEdges(mRect)) {
      aTo += nsPrintfCString(" vis-overflow=%s",
                             ConvertToString(vo, aFlags).c_str());
    }
    nsRect so = f->GetScrollableOverflowRect();
    if (!so.IsEqualEdges(mRect)) {
      aTo += nsPrintfCString(" scr-overflow=%s",
                             ConvertToString(so, aFlags).c_str());
    }
  }
  bool hasNormalPosition;
  nsPoint normalPosition = GetNormalPosition(&hasNormalPosition);
  if (hasNormalPosition) {
    aTo += nsPrintfCString(" normal-position=%s",
                           ConvertToString(normalPosition, aFlags).c_str());
  }
  if (0 != mState) {
    aTo += nsPrintfCString(" [state=%016llx]", (unsigned long long)mState);
  }
  if (HasProperty(BidiDataProperty())) {
    FrameBidiData bidi = GetBidiData();
    aTo += nsPrintfCString(" bidi(%d,%d,%d)", bidi.baseLevel,
                           bidi.embeddingLevel, bidi.precedingControl);
  }
  if (IsTransformed()) {
    aTo += nsPrintfCString(" transformed");
  }
  if (ChildrenHavePerspective()) {
    aTo += nsPrintfCString(" perspective");
  }
  if (Extend3DContext()) {
    aTo += nsPrintfCString(" extend-3d");
  }
  if (Combines3DTransformWithAncestors()) {
    aTo += nsPrintfCString(" combines-3d-transform-with-ancestors");
  }
  if (mContent) {
    aTo += nsPrintfCString(" [content=%p]", static_cast<void*>(mContent));
  }
  aTo += nsPrintfCString(" [cs=%p", static_cast<void*>(mComputedStyle));
  if (mComputedStyle) {
    auto pseudoType = mComputedStyle->GetPseudoType();
    aTo += ToString(pseudoType).c_str();
  }
  aTo += "]";
}

void nsIFrame::List(FILE* out, const char* aPrefix, ListFlags aFlags) const {
  nsCString str;
  ListGeneric(str, aPrefix, aFlags);
  fprintf_stderr(out, "%s\n", str.get());
}

void nsIFrame::ListMatchedRules(FILE* out, const char* aPrefix) const {
  nsTArray<const RawServoStyleRule*> rawRuleList;
  Servo_ComputedValues_GetStyleRuleList(mComputedStyle, &rawRuleList);
  for (const RawServoStyleRule* rawRule : rawRuleList) {
    nsString ruleText;
    Servo_StyleRule_GetCssText(rawRule, &ruleText);
    fprintf_stderr(out, "%s%s\n", aPrefix,
                   NS_ConvertUTF16toUTF8(ruleText).get());
  }
}

void nsIFrame::ListWithMatchedRules(FILE* out, const char* aPrefix) const {
  fprintf_stderr(out, "%s%s\n", aPrefix, ListTag().get());

  nsCString rulePrefix;
  rulePrefix += aPrefix;
  rulePrefix += "    ";
  ListMatchedRules(out, rulePrefix.get());
}

nsresult nsFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(NS_LITERAL_STRING("Frame"), aResult);
}

nsresult nsFrame::MakeFrameName(const nsAString& aType,
                                nsAString& aResult) const {
  aResult = aType;
  if (mContent && !mContent->IsText()) {
    nsAutoString buf;
    mContent->NodeInfo()->NameAtom()->ToString(buf);
    if (IsSubDocumentFrame()) {
      nsAutoString src;
      mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::src, src);
      buf.AppendLiteral(" src=");
      buf.Append(src);
    }
    aResult.Append('(');
    aResult.Append(buf);
    aResult.Append(')');
  }
  aResult.Append('(');
  aResult.AppendInt(ContentIndexInContainer(this));
  aResult.Append(')');
  return NS_OK;
}

void nsIFrame::DumpFrameTree() const { RootFrameList(PresContext(), stderr); }

void nsIFrame::DumpFrameTreeLimited() const { List(stderr); }

void nsIFrame::RootFrameList(nsPresContext* aPresContext, FILE* out,
                             const char* aPrefix) {
  if (!aPresContext || !out) return;

  if (mozilla::PresShell* presShell = aPresContext->GetPresShell()) {
    nsIFrame* frame = presShell->GetRootFrame();
    if (frame) {
      frame->List(out, aPrefix);
    }
  }
}
#endif

bool nsIFrame::IsVisibleForPainting() { return StyleVisibility()->IsVisible(); }

bool nsIFrame::IsVisibleOrCollapsedForPainting() {
  return StyleVisibility()->IsVisibleOrCollapsed();
}

/* virtual */
bool nsFrame::IsEmpty() { return false; }

bool nsIFrame::CachedIsEmpty() {
  MOZ_ASSERT(!(GetStateBits() & NS_FRAME_IS_DIRTY),
             "Must only be called on reflowed lines");
  return IsEmpty();
}

/* virtual */
bool nsFrame::IsSelfEmpty() { return false; }

nsresult nsFrame::GetSelectionController(nsPresContext* aPresContext,
                                         nsISelectionController** aSelCon) {
  if (!aPresContext || !aSelCon) return NS_ERROR_INVALID_ARG;

  nsIFrame* frame = this;
  while (frame && (frame->GetStateBits() & NS_FRAME_INDEPENDENT_SELECTION)) {
    nsITextControlFrame* tcf = do_QueryFrame(frame);
    if (tcf) {
      return tcf->GetOwnedSelectionController(aSelCon);
    }
    frame = frame->GetParent();
  }

  *aSelCon = do_AddRef(aPresContext->PresShell()).take();
  return NS_OK;
}

already_AddRefed<nsFrameSelection> nsIFrame::GetFrameSelection() {
  RefPtr<nsFrameSelection> fs =
      const_cast<nsFrameSelection*>(GetConstFrameSelection());
  return fs.forget();
}

const nsFrameSelection* nsIFrame::GetConstFrameSelection() const {
  nsIFrame* frame = const_cast<nsIFrame*>(this);
  while (frame && (frame->GetStateBits() & NS_FRAME_INDEPENDENT_SELECTION)) {
    nsITextControlFrame* tcf = do_QueryFrame(frame);
    if (tcf) {
      return tcf->GetOwnedFrameSelection();
    }
    frame = frame->GetParent();
  }

  return PresShell()->ConstFrameSelection();
}

bool nsIFrame::IsFrameSelected() const {
  NS_ASSERTION(!GetContent() || GetContent()->IsMaybeSelected(),
               "use the public IsSelected() instead");
  return GetContent()->IsSelected(0, GetContent()->GetChildCount());
}

nsresult nsFrame::GetPointFromOffset(int32_t inOffset, nsPoint* outPoint) {
  MOZ_ASSERT(outPoint != nullptr, "Null parameter");
  nsRect contentRect = GetContentRectRelativeToSelf();
  nsPoint pt = contentRect.TopLeft();
  if (mContent) {
    nsIContent* newContent = mContent->GetParent();
    if (newContent) {
      int32_t newOffset = newContent->ComputeIndexOf(mContent);

      // Find the direction of the frame from the EmbeddingLevelProperty,
      // which is the resolved bidi level set in
      // nsBidiPresUtils::ResolveParagraph (odd levels = right-to-left).
      // If the embedding level isn't set, just use the CSS direction
      // property.
      bool hasBidiData;
      FrameBidiData bidiData = GetProperty(BidiDataProperty(), &hasBidiData);
      bool isRTL = hasBidiData
                       ? IS_LEVEL_RTL(bidiData.embeddingLevel)
                       : StyleVisibility()->mDirection == StyleDirection::Rtl;
      if ((!isRTL && inOffset > newOffset) ||
          (isRTL && inOffset <= newOffset)) {
        pt = contentRect.TopRight();
      }
    }
  }
  *outPoint = pt;
  return NS_OK;
}

nsresult nsFrame::GetCharacterRectsInRange(int32_t aInOffset, int32_t aLength,
                                           nsTArray<nsRect>& aOutRect) {
  /* no text */
  return NS_ERROR_FAILURE;
}

nsresult nsFrame::GetChildFrameContainingOffset(int32_t inContentOffset,
                                                bool inHint,
                                                int32_t* outFrameContentOffset,
                                                nsIFrame** outChildFrame) {
  MOZ_ASSERT(outChildFrame && outFrameContentOffset, "Null parameter");
  *outFrameContentOffset = (int32_t)inHint;
  // the best frame to reflect any given offset would be a visible frame if
  // possible i.e. we are looking for a valid frame to place the blinking caret
  nsRect rect = GetRect();
  if (!rect.width || !rect.height) {
    // if we have a 0 width or height then lets look for another frame that
    // possibly has the same content.  If we have no frames in flow then just
    // let us return 'this' frame
    nsIFrame* nextFlow = GetNextInFlow();
    if (nextFlow)
      return nextFlow->GetChildFrameContainingOffset(
          inContentOffset, inHint, outFrameContentOffset, outChildFrame);
  }
  *outChildFrame = this;
  return NS_OK;
}

//
// What I've pieced together about this routine:
// Starting with a block frame (from which a line frame can be gotten)
// and a line number, drill down and get the first/last selectable
// frame on that line, depending on aPos->mDirection.
// aOutSideLimit != 0 means ignore aLineStart, instead work from
// the end (if > 0) or beginning (if < 0).
//
nsresult nsFrame::GetNextPrevLineFromeBlockFrame(nsPresContext* aPresContext,
                                                 nsPeekOffsetStruct* aPos,
                                                 nsIFrame* aBlockFrame,
                                                 int32_t aLineStart,
                                                 int8_t aOutSideLimit) {
  // magic numbers aLineStart will be -1 for end of block 0 will be start of
  // block
  if (!aBlockFrame || !aPos) return NS_ERROR_NULL_POINTER;

  aPos->mResultFrame = nullptr;
  aPos->mResultContent = nullptr;
  aPos->mAttach = aPos->mDirection == eDirNext ? CARET_ASSOCIATE_AFTER
                                               : CARET_ASSOCIATE_BEFORE;

  nsAutoLineIterator it = aBlockFrame->GetLineIterator();
  if (!it) return NS_ERROR_FAILURE;
  int32_t searchingLine = aLineStart;
  int32_t countLines = it->GetNumLines();
  if (aOutSideLimit > 0)  // start at end
    searchingLine = countLines;
  else if (aOutSideLimit < 0)  // start at beginning
    searchingLine = -1;        //"next" will be 0
  else if ((aPos->mDirection == eDirPrevious && searchingLine == 0) ||
           (aPos->mDirection == eDirNext &&
            searchingLine >= (countLines - 1))) {
    // we need to jump to new block frame.
    return NS_ERROR_FAILURE;
  }
  int32_t lineFrameCount;
  nsIFrame* resultFrame = nullptr;
  nsIFrame* farStoppingFrame = nullptr;  // we keep searching until we find a
                                         // "this" frame then we go to next line
  nsIFrame* nearStoppingFrame = nullptr;  // if we are backing up from edge,
                                          // stop here
  nsIFrame* firstFrame;
  nsIFrame* lastFrame;
  nsRect rect;
  bool isBeforeFirstFrame, isAfterLastFrame;
  bool found = false;

  nsresult result = NS_OK;
  while (!found) {
    if (aPos->mDirection == eDirPrevious)
      searchingLine--;
    else
      searchingLine++;
    if ((aPos->mDirection == eDirPrevious && searchingLine < 0) ||
        (aPos->mDirection == eDirNext && searchingLine >= countLines)) {
      // we need to jump to new block frame.
      return NS_ERROR_FAILURE;
    }
    result = it->GetLine(searchingLine, &firstFrame, &lineFrameCount, rect);
    if (!lineFrameCount) continue;
    if (NS_SUCCEEDED(result)) {
      lastFrame = firstFrame;
      for (; lineFrameCount > 1; lineFrameCount--) {
        // result = lastFrame->GetNextSibling(&lastFrame, searchingLine);
        result = it->GetNextSiblingOnLine(lastFrame, searchingLine);
        if (NS_FAILED(result) || !lastFrame) {
          NS_ERROR("GetLine promised more frames than could be found");
          return NS_ERROR_FAILURE;
        }
      }
      GetLastLeaf(aPresContext, &lastFrame);

      if (aPos->mDirection == eDirNext) {
        nearStoppingFrame = firstFrame;
        farStoppingFrame = lastFrame;
      } else {
        nearStoppingFrame = lastFrame;
        farStoppingFrame = firstFrame;
      }
      nsPoint offset;
      nsView* view;  // used for call of get offset from view
      aBlockFrame->GetOffsetFromView(offset, &view);
      nsPoint newDesiredPos =
          aPos->mDesiredPos -
          offset;  // get desired position into blockframe coords
      result = it->FindFrameAt(searchingLine, newDesiredPos, &resultFrame,
                               &isBeforeFirstFrame, &isAfterLastFrame);
      if (NS_FAILED(result)) continue;
    }

    if (NS_SUCCEEDED(result) && resultFrame) {
      // check to see if this is ANOTHER blockframe inside the other one if so
      // then call into its lines
      nsAutoLineIterator newIt = resultFrame->GetLineIterator();
      if (newIt) {
        aPos->mResultFrame = resultFrame;
        return NS_OK;
      }
      // resultFrame is not a block frame
      result = NS_ERROR_FAILURE;

      nsCOMPtr<nsIFrameEnumerator> frameTraversal;
      result = NS_NewFrameTraversal(getter_AddRefs(frameTraversal),
                                    aPresContext, resultFrame, ePostOrder,
                                    false,  // aVisual
                                    aPos->mScrollViewStop,
                                    false,  // aFollowOOFs
                                    false   // aSkipPopupChecks
      );
      if (NS_FAILED(result)) return result;

      auto FoundValidFrame = [aPos](const ContentOffsets& aOffsets,
                                    const nsIFrame* aFrame) {
        if (!aOffsets.content) {
          return false;
        }
        if (!aFrame->IsSelectable(nullptr)) {
          return false;
        }
        if (aPos->mForceEditableRegion && !aOffsets.content->IsEditable()) {
          return false;
        }
        return true;
      };

      nsIFrame* storeOldResultFrame = resultFrame;
      while (!found) {
        nsPoint point;
        nsRect tempRect = resultFrame->GetRect();
        nsPoint offset;
        nsView* view;  // used for call of get offset from view
        resultFrame->GetOffsetFromView(offset, &view);
        if (!view) {
          return NS_ERROR_FAILURE;
        }
        if (resultFrame->GetWritingMode().IsVertical()) {
          point.y = aPos->mDesiredPos.y;
          point.x = tempRect.width + offset.x;
        } else {
          point.y = tempRect.height + offset.y;
          point.x = aPos->mDesiredPos.x;
        }

        // special check. if we allow non-text selection then we can allow a hit
        // location to fall before a table. otherwise there is no way to get and
        // click signal to fall before a table (it being a line iterator itself)
        mozilla::PresShell* presShell = aPresContext->GetPresShell();
        if (!presShell) {
          return NS_ERROR_FAILURE;
        }
        int16_t isEditor = presShell->GetSelectionFlags();
        isEditor = isEditor == nsISelectionDisplay::DISPLAY_ALL;
        if (isEditor) {
          if (resultFrame->IsTableWrapperFrame()) {
            if (((point.x - offset.x + tempRect.x) < 0) ||
                ((point.x - offset.x + tempRect.x) >
                 tempRect.width))  // off left/right side
            {
              nsIContent* content = resultFrame->GetContent();
              if (content) {
                nsIContent* parent = content->GetParent();
                if (parent) {
                  aPos->mResultContent = parent;
                  aPos->mContentOffset = parent->ComputeIndexOf(content);
                  aPos->mAttach = CARET_ASSOCIATE_BEFORE;
                  if ((point.x - offset.x + tempRect.x) > tempRect.width) {
                    aPos->mContentOffset++;  // go to end of this frame
                    aPos->mAttach = CARET_ASSOCIATE_AFTER;
                  }
                  // result frame is the result frames parent.
                  aPos->mResultFrame = resultFrame->GetParent();
                  return NS_POSITION_BEFORE_TABLE;
                }
              }
            }
          }
        }

        if (!resultFrame->HasView()) {
          nsView* view;
          nsPoint offset;
          resultFrame->GetOffsetFromView(offset, &view);
          ContentOffsets offsets =
              resultFrame->GetContentOffsetsFromPoint(point - offset);
          aPos->mResultContent = offsets.content;
          aPos->mContentOffset = offsets.offset;
          aPos->mAttach = offsets.associate;
          if (FoundValidFrame(offsets, resultFrame)) {
            found = true;
            break;
          }
        }

        if (aPos->mDirection == eDirPrevious &&
            (resultFrame == farStoppingFrame))
          break;
        if (aPos->mDirection == eDirNext && (resultFrame == nearStoppingFrame))
          break;
        // always try previous on THAT line if that fails go the other way
        frameTraversal->Prev();
        resultFrame = frameTraversal->CurrentItem();
        if (!resultFrame) return NS_ERROR_FAILURE;
      }

      if (!found) {
        resultFrame = storeOldResultFrame;

        result = NS_NewFrameTraversal(getter_AddRefs(frameTraversal),
                                      aPresContext, resultFrame, eLeaf,
                                      false,  // aVisual
                                      aPos->mScrollViewStop,
                                      false,  // aFollowOOFs
                                      false   // aSkipPopupChecks
        );
      }
      while (!found) {
        nsPoint point = aPos->mDesiredPos;
        nsView* view;
        nsPoint offset;
        resultFrame->GetOffsetFromView(offset, &view);
        ContentOffsets offsets =
            resultFrame->GetContentOffsetsFromPoint(point - offset);
        aPos->mResultContent = offsets.content;
        aPos->mContentOffset = offsets.offset;
        aPos->mAttach = offsets.associate;
        if (FoundValidFrame(offsets, resultFrame)) {
          found = true;
          if (resultFrame == farStoppingFrame)
            aPos->mAttach = CARET_ASSOCIATE_BEFORE;
          else
            aPos->mAttach = CARET_ASSOCIATE_AFTER;
          break;
        }
        if (aPos->mDirection == eDirPrevious &&
            (resultFrame == nearStoppingFrame))
          break;
        if (aPos->mDirection == eDirNext && (resultFrame == farStoppingFrame))
          break;
        // previous didnt work now we try "next"
        frameTraversal->Next();
        nsIFrame* tempFrame = frameTraversal->CurrentItem();
        if (!tempFrame) break;
        resultFrame = tempFrame;
      }
      aPos->mResultFrame = resultFrame;
    } else {
      // we need to jump to new block frame.
      aPos->mAmount = eSelectLine;
      aPos->mStartOffset = 0;
      aPos->mAttach = aPos->mDirection == eDirNext ? CARET_ASSOCIATE_BEFORE
                                                   : CARET_ASSOCIATE_AFTER;
      if (aPos->mDirection == eDirPrevious)
        aPos->mStartOffset = -1;  // start from end
      return aBlockFrame->PeekOffset(aPos);
    }
  }
  return NS_OK;
}

nsIFrame::CaretPosition nsIFrame::GetExtremeCaretPosition(bool aStart) {
  CaretPosition result;

  FrameTarget targetFrame = DrillDownToSelectionFrame(this, !aStart, 0);
  FrameContentRange range = GetRangeForFrame(targetFrame.frame);
  result.mResultContent = range.content;
  result.mContentOffset = aStart ? range.start : range.end;
  return result;
}

// If this is a preformatted text frame, see if it ends with a newline
static nsContentAndOffset FindLineBreakInText(nsIFrame* aFrame,
                                              nsDirection aDirection) {
  nsContentAndOffset result;

  if (aFrame->IsGeneratedContentFrame() ||
      !aFrame->HasSignificantTerminalNewline()) {
    return result;
  }

  int32_t startOffset, endOffset;
  aFrame->GetOffsets(startOffset, endOffset);
  result.mContent = aFrame->GetContent();
  result.mOffset = endOffset - (aDirection == eDirPrevious ? 0 : 1);
  return result;
}

// Find the first (or last) descendant of the given frame
// which is either a block-level frame or a BRFrame, or some other kind of break
// which stops the line.
static nsContentAndOffset FindLineBreakingFrame(nsIFrame* aFrame,
                                                nsDirection aDirection) {
  nsContentAndOffset result;

  if (aFrame->IsGeneratedContentFrame()) {
    return result;
  }

  // Treat form controls as inline leaves
  // XXX we really need a way to determine whether a frame is inline-level
  if (static_cast<nsIFormControlFrame*>(do_QueryFrame(aFrame))) {
    return result;
  }

  // Check the frame itself
  // Fall through block-in-inline split frames because their mContent is
  // the content of the inline frames they were created from. The
  // first/last child of such frames is the real block frame we're
  // looking for.
  if ((aFrame->IsBlockOutside() &&
       !(aFrame->GetStateBits() & NS_FRAME_PART_OF_IBSPLIT)) ||
      aFrame->IsBrFrame()) {
    nsIContent* content = aFrame->GetContent();
    result.mContent = content->GetParent();
    // In some cases (bug 310589, bug 370174) we end up here with a null
    // content. This probably shouldn't ever happen, but since it sometimes
    // does, we want to avoid crashing here.
    NS_ASSERTION(result.mContent, "Unexpected orphan content");
    if (result.mContent)
      result.mOffset = result.mContent->ComputeIndexOf(content) +
                       (aDirection == eDirPrevious ? 1 : 0);
    return result;
  }

  result = FindLineBreakInText(aFrame, aDirection);
  if (result.mContent) {
    return result;
  }

  // Iterate over children and call ourselves recursively
  if (aDirection == eDirPrevious) {
    nsIFrame* child =
        aFrame->GetChildList(nsIFrame::kPrincipalList).LastChild();
    while (child && !result.mContent) {
      result = FindLineBreakingFrame(child, aDirection);
      child = child->GetPrevSibling();
    }
  } else {  // eDirNext
    nsIFrame* child = aFrame->PrincipalChildList().FirstChild();
    while (child && !result.mContent) {
      result = FindLineBreakingFrame(child, aDirection);
      child = child->GetNextSibling();
    }
  }
  return result;
}

nsresult nsIFrame::PeekOffsetParagraph(nsPeekOffsetStruct* aPos) {
  nsIFrame* frame = this;
  nsContentAndOffset blockFrameOrBR;
  blockFrameOrBR.mContent = nullptr;
  bool reachedBlockAncestor = frame->IsBlockOutside();

  // Go through containing frames until reaching a block frame.
  // In each step, search the previous (or next) siblings for the closest
  // "stop frame" (a block frame or a BRFrame).
  // If found, set it to be the selection boundary and abort.

  if (aPos->mDirection == eDirPrevious) {
    while (!reachedBlockAncestor) {
      nsIFrame* parent = frame->GetParent();
      // Treat a frame associated with the root content as if it were a block
      // frame.
      if (!frame->mContent || !frame->mContent->GetParent()) {
        reachedBlockAncestor = true;
        break;
      }
      nsIFrame* sibling = frame->GetPrevSibling();
      while (sibling && !blockFrameOrBR.mContent) {
        blockFrameOrBR = FindLineBreakingFrame(sibling, eDirPrevious);
        sibling = sibling->GetPrevSibling();
      }
      if (blockFrameOrBR.mContent) {
        aPos->mResultContent = blockFrameOrBR.mContent;
        aPos->mContentOffset = blockFrameOrBR.mOffset;
        break;
      }
      frame = parent;
      reachedBlockAncestor = frame && frame->IsBlockOutside();
    }
    if (reachedBlockAncestor) {  // no "stop frame" found
      aPos->mResultContent = frame->GetContent();
      aPos->mContentOffset = 0;
    }
  } else {  // eDirNext
    while (!reachedBlockAncestor) {
      nsIFrame* parent = frame->GetParent();
      // Treat a frame associated with the root content as if it were a block
      // frame.
      if (!frame->mContent || !frame->mContent->GetParent()) {
        reachedBlockAncestor = true;
        break;
      }

      // Try to find our own line-break before looking at our siblings.
      blockFrameOrBR = FindLineBreakInText(frame, eDirNext);

      nsIFrame* sibling = frame->GetNextSibling();
      while (sibling && !blockFrameOrBR.mContent) {
        blockFrameOrBR = FindLineBreakingFrame(sibling, eDirNext);
        sibling = sibling->GetNextSibling();
      }
      if (blockFrameOrBR.mContent) {
        aPos->mResultContent = blockFrameOrBR.mContent;
        aPos->mContentOffset = blockFrameOrBR.mOffset;
        break;
      }
      frame = parent;
      reachedBlockAncestor = frame && frame->IsBlockOutside();
    }
    if (reachedBlockAncestor) {  // no "stop frame" found
      aPos->mResultContent = frame->GetContent();
      if (aPos->mResultContent)
        aPos->mContentOffset = aPos->mResultContent->GetChildCount();
    }
  }
  return NS_OK;
}

// Determine movement direction relative to frame
static bool IsMovingInFrameDirection(nsIFrame* frame, nsDirection aDirection,
                                     bool aVisual) {
  bool isReverseDirection = aVisual && IsReversedDirectionFrame(frame);
  return aDirection == (isReverseDirection ? eDirPrevious : eDirNext);
}

nsresult nsIFrame::PeekOffset(nsPeekOffsetStruct* aPos) {
  if (!aPos) return NS_ERROR_NULL_POINTER;
  nsresult result = NS_ERROR_FAILURE;

  if (mState & NS_FRAME_IS_DIRTY) return NS_ERROR_UNEXPECTED;

  // Translate content offset to be relative to frame
  FrameContentRange range = GetRangeForFrame(this);
  int32_t offset = aPos->mStartOffset - range.start;
  nsIFrame* current = this;

  switch (aPos->mAmount) {
    case eSelectCharacter:
    case eSelectCluster: {
      bool eatingNonRenderableWS = false;
      nsIFrame::FrameSearchResult peekSearchState = CONTINUE;
      bool jumpedLine = false;
      bool movedOverNonSelectableText = false;

      while (peekSearchState != FOUND) {
        bool movingInFrameDirection =
            IsMovingInFrameDirection(current, aPos->mDirection, aPos->mVisual);

        if (eatingNonRenderableWS) {
          peekSearchState =
              current->PeekOffsetNoAmount(movingInFrameDirection, &offset);
        } else {
          PeekOffsetCharacterOptions options;
          options.mRespectClusters = aPos->mAmount == eSelectCluster;
          peekSearchState = current->PeekOffsetCharacter(movingInFrameDirection,
                                                         &offset, options);
        }

        movedOverNonSelectableText |=
            (peekSearchState == CONTINUE_UNSELECTABLE);

        if (peekSearchState != FOUND) {
          bool movedOverNonSelectable = false;
          result = current->GetFrameFromDirection(
              aPos->mDirection, aPos->mVisual, aPos->mJumpLines,
              aPos->mScrollViewStop, aPos->mForceEditableRegion, &current,
              &offset, &jumpedLine, &movedOverNonSelectable);
          if (NS_FAILED(result)) return result;

          // If we jumped lines, it's as if we found a character, but we still
          // need to eat non-renderable content on the new line.
          if (jumpedLine) eatingNonRenderableWS = true;

          // Remember if we moved over non-selectable text when finding another
          // frame.
          movedOverNonSelectableText |= movedOverNonSelectable;
        }

        // Found frame, but because we moved over non selectable text we want
        // the offset to be at the frame edge. Note that if we are extending the
        // selection, this doesn't matter.
        if (peekSearchState == FOUND && movedOverNonSelectableText &&
            !aPos->mExtend) {
          int32_t start, end;
          current->GetOffsets(start, end);
          offset = aPos->mDirection == eDirNext ? 0 : end - start;
        }
      }

      // Set outputs
      range = GetRangeForFrame(current);
      aPos->mResultFrame = current;
      aPos->mResultContent = range.content;
      // Output offset is relative to content, not frame
      aPos->mContentOffset = offset < 0 ? range.end : range.start + offset;
      // If we're dealing with a text frame and moving backward positions us at
      // the end of that line, decrease the offset by one to make sure that
      // we're placed before the linefeed character on the previous line.
      if (offset < 0 && jumpedLine && aPos->mDirection == eDirPrevious &&
          current->HasSignificantTerminalNewline()) {
        --aPos->mContentOffset;
      }

      break;
    }
    case eSelectWordNoSpace:
      // eSelectWordNoSpace means that we should not be eating any whitespace
      // when moving to the adjacent word.  This means that we should set aPos->
      // mWordMovementType to eEndWord if we're moving forwards, and to
      // eStartWord if we're moving backwards.
      if (aPos->mDirection == eDirPrevious) {
        aPos->mWordMovementType = eStartWord;
      } else {
        aPos->mWordMovementType = eEndWord;
      }
      // Intentionally fall through the eSelectWord case.
      [[fallthrough]];
    case eSelectWord: {
      // wordSelectEatSpace means "are we looking for a boundary between
      // whitespace and non-whitespace (in the direction we're moving in)". It
      // is true when moving forward and looking for a beginning of a word, or
      // when moving backwards and looking for an end of a word.
      bool wordSelectEatSpace;
      if (aPos->mWordMovementType != eDefaultBehavior) {
        // aPos->mWordMovementType possible values:
        //       eEndWord: eat the space if we're moving backwards
        //       eStartWord: eat the space if we're moving forwards
        wordSelectEatSpace = ((aPos->mWordMovementType == eEndWord) ==
                              (aPos->mDirection == eDirPrevious));
      } else {
        // Use the hidden preference which is based on operating system
        // behavior. This pref only affects whether moving forward by word
        // should go to the end of this word or start of the next word. When
        // going backwards, the start of the word is always used, on every
        // operating system.
        wordSelectEatSpace =
            aPos->mDirection == eDirNext &&
            Preferences::GetBool("layout.word_select.eat_space_to_next_word");
      }

      // mSawBeforeType means "we already saw characters of the type
      // before the boundary we're looking for". Examples:
      // 1. If we're moving forward, looking for a word beginning (i.e. a
      //    boundary between whitespace and non-whitespace), then
      //    eatingWS==true means "we already saw some whitespace".
      // 2. If we're moving backward, looking for a word beginning (i.e. a
      //    boundary between non-whitespace and whitespace), then
      //     eatingWS==true means "we already saw some non-whitespace".
      PeekWordState state;
      int32_t offsetAdjustment = 0;
      bool done = false;
      while (!done) {
        bool movingInFrameDirection =
            IsMovingInFrameDirection(current, aPos->mDirection, aPos->mVisual);

        done =
            current->PeekOffsetWord(movingInFrameDirection, wordSelectEatSpace,
                                    aPos->mIsKeyboardSelect, &offset, &state,
                                    aPos->mTrimSpaces) == FOUND;

        if (!done) {
          nsIFrame* nextFrame;
          int32_t nextFrameOffset;
          bool jumpedLine, movedOverNonSelectableText;
          result = current->GetFrameFromDirection(
              aPos->mDirection, aPos->mVisual, aPos->mJumpLines,
              aPos->mScrollViewStop, aPos->mForceEditableRegion, &nextFrame,
              &nextFrameOffset, &jumpedLine, &movedOverNonSelectableText);
          // We can't jump lines if we're looking for whitespace following
          // non-whitespace, and we already encountered non-whitespace.
          if (NS_FAILED(result) ||
              (jumpedLine && !wordSelectEatSpace && state.mSawBeforeType)) {
            done = true;
            // If we've crossed the line boundary, check to make sure that we
            // have not consumed a trailing newline as whitesapce if it's
            // significant.
            if (jumpedLine && wordSelectEatSpace &&
                current->HasSignificantTerminalNewline()) {
              offsetAdjustment = -1;
            }
          } else {
            if (jumpedLine) {
              state.mContext.Truncate();
            }
            current = nextFrame;
            offset = nextFrameOffset;
            // Jumping a line is equivalent to encountering whitespace
            if (wordSelectEatSpace && jumpedLine) state.SetSawBeforeType();
          }
        }
      }

      // Set outputs
      range = GetRangeForFrame(current);
      aPos->mResultFrame = current;
      aPos->mResultContent = range.content;
      // Output offset is relative to content, not frame
      aPos->mContentOffset =
          (offset < 0 ? range.end : range.start + offset) + offsetAdjustment;
      break;
    }
    case eSelectLine: {
      nsAutoLineIterator iter;
      nsIFrame* blockFrame = this;

      while (NS_FAILED(result)) {
        int32_t thisLine = nsFrame::GetLineNumber(
            blockFrame, aPos->mScrollViewStop, &blockFrame);
        if (thisLine < 0) return NS_ERROR_FAILURE;
        iter = blockFrame->GetLineIterator();
        NS_ASSERTION(iter, "GetLineNumber() succeeded but no block frame?");

        int edgeCase = 0;  // no edge case. this should look at thisLine

        bool doneLooping = false;  // tells us when no more block frames hit.
        // this part will find a frame or a block frame. if it's a block frame
        // it will "drill down" to find a viable frame or it will return an
        // error.
        nsIFrame* lastFrame = this;
        do {
          result = nsFrame::GetNextPrevLineFromeBlockFrame(
              PresContext(), aPos, blockFrame, thisLine,
              edgeCase);  // start from thisLine

          // we came back to same spot! keep going
          if (NS_SUCCEEDED(result) &&
              (!aPos->mResultFrame || aPos->mResultFrame == lastFrame)) {
            aPos->mResultFrame = nullptr;
            if (aPos->mDirection == eDirPrevious)
              thisLine--;
            else
              thisLine++;
          } else                 // if failure or success with different frame.
            doneLooping = true;  // do not continue with while loop

          lastFrame = aPos->mResultFrame;  // set last frame

          // make sure block element is not the same as the one we had before
          if (NS_SUCCEEDED(result) && aPos->mResultFrame &&
              blockFrame != aPos->mResultFrame) {
            /* SPECIAL CHECK FOR TABLE NAVIGATION
               tables need to navigate also and the frame that supports it is
               nsTableRowGroupFrame which is INSIDE nsTableWrapperFrame.
               If we have stumbled onto an nsTableWrapperFrame we need to drill
               into nsTableRowGroup if we hit a header or footer that's ok just
               go into them.
             */
            bool searchTableBool = false;
            if (aPos->mResultFrame->IsTableWrapperFrame() ||
                aPos->mResultFrame->IsTableCellFrame()) {
              nsIFrame* frame =
                  aPos->mResultFrame->PrincipalChildList().FirstChild();
              // got the table frame now
              // ok time to drill down to find iterator
              while (frame) {
                iter = frame->GetLineIterator();
                if (iter) {
                  aPos->mResultFrame = frame;
                  searchTableBool = true;
                  result = NS_OK;
                  break;  // while(frame)
                }
                result = NS_ERROR_FAILURE;
                frame = frame->PrincipalChildList().FirstChild();
              }
            }

            if (!searchTableBool) {
              iter = aPos->mResultFrame->GetLineIterator();
              result = iter ? NS_OK : NS_ERROR_FAILURE;
            }

            // we've struck another block element!
            if (NS_SUCCEEDED(result) && iter) {
              doneLooping = false;
              if (aPos->mDirection == eDirPrevious)
                edgeCase = 1;  // far edge, search from end backwards
              else
                edgeCase = -1;  // near edge search from beginning onwards
              thisLine = 0;     // this line means nothing now.
              // everything else means something so keep looking "inside" the
              // block
              blockFrame = aPos->mResultFrame;
            } else {
              // THIS is to mean that everything is ok to the containing while
              // loop
              result = NS_OK;
              break;
            }
          }
        } while (!doneLooping);
      }
      return result;
    }

    case eSelectParagraph:
      return PeekOffsetParagraph(aPos);

    case eSelectBeginLine:
    case eSelectEndLine: {
      // Adjusted so that the caret can't get confused when content changes
      nsIFrame* blockFrame = AdjustFrameForSelectionStyles(this);
      int32_t thisLine = nsFrame::GetLineNumber(
          blockFrame, aPos->mScrollViewStop, &blockFrame);
      if (thisLine < 0) return NS_ERROR_FAILURE;
      nsAutoLineIterator it = blockFrame->GetLineIterator();
      NS_ASSERTION(it, "GetLineNumber() succeeded but no block frame?");

      int32_t lineFrameCount;
      nsIFrame* firstFrame;
      nsRect usedRect;
      nsIFrame* baseFrame = nullptr;
      bool endOfLine = (eSelectEndLine == aPos->mAmount);

      if (aPos->mVisual && PresContext()->BidiEnabled()) {
        bool lineIsRTL = it->GetDirection();
        bool isReordered;
        nsIFrame* lastFrame;
        result =
            it->CheckLineOrder(thisLine, &isReordered, &firstFrame, &lastFrame);
        baseFrame = endOfLine ? lastFrame : firstFrame;
        if (baseFrame) {
          bool frameIsRTL =
              (nsBidiPresUtils::FrameDirection(baseFrame) == NSBIDI_RTL);
          // If the direction of the frame on the edge is opposite to
          // that of the line, we'll need to drill down to its opposite
          // end, so reverse endOfLine.
          if (frameIsRTL != lineIsRTL) {
            endOfLine = !endOfLine;
          }
        }
      } else {
        it->GetLine(thisLine, &firstFrame, &lineFrameCount, usedRect);

        nsIFrame* frame = firstFrame;
        bool lastFrameWasEditable = false;
        for (int32_t count = lineFrameCount; count;
             --count, frame = frame->GetNextSibling()) {
          if (frame->IsGeneratedContentFrame()) {
            continue;
          }
          // When jumping to the end of the line with the "end" key,
          // try to skip over brFrames
          if (endOfLine && lineFrameCount > 1 && frame->IsBrFrame() &&
              lastFrameWasEditable == frame->GetContent()->IsEditable()) {
            continue;
          }
          lastFrameWasEditable =
              frame->GetContent() && frame->GetContent()->IsEditable();
          baseFrame = frame;
          if (!endOfLine) {
            break;
          }
        }
      }
      if (!baseFrame) return NS_ERROR_FAILURE;
      FrameTarget targetFrame =
          DrillDownToSelectionFrame(baseFrame, endOfLine, 0);
      FrameContentRange range = GetRangeForFrame(targetFrame.frame);
      aPos->mResultContent = range.content;
      aPos->mContentOffset = endOfLine ? range.end : range.start;
      if (endOfLine && targetFrame.frame->HasSignificantTerminalNewline()) {
        // Do not position the caret after the terminating newline if we're
        // trying to move to the end of line (see bug 596506)
        --aPos->mContentOffset;
      }
      aPos->mResultFrame = targetFrame.frame;
      aPos->mAttach = aPos->mContentOffset == range.start
                          ? CARET_ASSOCIATE_AFTER
                          : CARET_ASSOCIATE_BEFORE;
      if (!range.content) return NS_ERROR_FAILURE;
      return NS_OK;
    }

    default: {
      NS_ASSERTION(false, "Invalid amount");
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

nsIFrame::FrameSearchResult nsFrame::PeekOffsetNoAmount(bool aForward,
                                                        int32_t* aOffset) {
  NS_ASSERTION(aOffset && *aOffset <= 1, "aOffset out of range");
  // Sure, we can stop right here.
  return FOUND;
}

nsIFrame::FrameSearchResult nsFrame::PeekOffsetCharacter(
    bool aForward, int32_t* aOffset, PeekOffsetCharacterOptions aOptions) {
  NS_ASSERTION(aOffset && *aOffset <= 1, "aOffset out of range");
  int32_t startOffset = *aOffset;
  // A negative offset means "end of frame", which in our case means offset 1.
  if (startOffset < 0) startOffset = 1;
  if (aForward == (startOffset == 0)) {
    // We're before the frame and moving forward, or after it and moving
    // backwards: skip to the other side and we're done.
    *aOffset = 1 - startOffset;
    return FOUND;
  }
  return CONTINUE;
}

nsIFrame::FrameSearchResult nsFrame::PeekOffsetWord(
    bool aForward, bool aWordSelectEatSpace, bool aIsKeyboardSelect,
    int32_t* aOffset, PeekWordState* aState, bool /*aTrimSpaces*/) {
  NS_ASSERTION(aOffset && *aOffset <= 1, "aOffset out of range");
  int32_t startOffset = *aOffset;
  // This isn't text, so truncate the context
  aState->mContext.Truncate();
  if (startOffset < 0) startOffset = 1;
  if (aForward == (startOffset == 0)) {
    // We're before the frame and moving forward, or after it and moving
    // backwards. If we're looking for non-whitespace, we found it (without
    // skipping this frame).
    if (!aState->mAtStart) {
      if (aState->mLastCharWasPunctuation) {
        // We're not punctuation, so this is a punctuation boundary.
        if (BreakWordBetweenPunctuation(aState, aForward, false, false,
                                        aIsKeyboardSelect))
          return FOUND;
      } else {
        // This is not a punctuation boundary.
        if (aWordSelectEatSpace && aState->mSawBeforeType) return FOUND;
      }
    }
    // Otherwise skip to the other side and note that we encountered
    // non-whitespace.
    *aOffset = 1 - startOffset;
    aState->Update(false,  // not punctuation
                   false   // not whitespace
    );
    if (!aWordSelectEatSpace) aState->SetSawBeforeType();
  }
  return CONTINUE;
}

// static
bool nsFrame::BreakWordBetweenPunctuation(const PeekWordState* aState,
                                          bool aForward, bool aPunctAfter,
                                          bool aWhitespaceAfter,
                                          bool aIsKeyboardSelect) {
  NS_ASSERTION(aPunctAfter != aState->mLastCharWasPunctuation,
               "Call this only at punctuation boundaries");
  if (aState->mLastCharWasWhitespace) {
    // We always stop between whitespace and punctuation
    return true;
  }
  if (!Preferences::GetBool("layout.word_select.stop_at_punctuation")) {
    // When this pref is false, we never stop at a punctuation boundary unless
    // it's followed by whitespace (in the relevant direction).
    return aWhitespaceAfter;
  }
  if (!aIsKeyboardSelect) {
    // mouse caret movement (e.g. word selection) always stops at every
    // punctuation boundary
    return true;
  }
  bool afterPunct = aForward ? aState->mLastCharWasPunctuation : aPunctAfter;
  if (!afterPunct) {
    // keyboard caret movement only stops after punctuation (in content order)
    return false;
  }
  // Stop only if we've seen some non-punctuation since the last whitespace;
  // don't stop after punctuation that follows whitespace.
  return aState->mSeenNonPunctuationSinceWhitespace;
}

nsresult nsFrame::CheckVisibility(nsPresContext*, int32_t, int32_t, bool, bool*,
                                  bool*) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

int32_t nsFrame::GetLineNumber(nsIFrame* aFrame, bool aLockScroll,
                               nsIFrame** aContainingBlock) {
  NS_ASSERTION(aFrame, "null aFrame");
  nsIFrame* blockFrame = aFrame;
  nsIFrame* thisBlock;
  nsAutoLineIterator it;
  nsresult result = NS_ERROR_FAILURE;
  while (NS_FAILED(result) && blockFrame) {
    thisBlock = blockFrame;
    if (thisBlock->GetStateBits() & NS_FRAME_OUT_OF_FLOW) {
      // if we are searching for a frame that is not in flow we will not find
      // it. we must instead look for its placeholder
      if (thisBlock->GetStateBits() & NS_FRAME_IS_OVERFLOW_CONTAINER) {
        // abspos continuations don't have placeholders, get the fif
        thisBlock = thisBlock->FirstInFlow();
      }
      thisBlock = thisBlock->GetPlaceholderFrame();
      if (!thisBlock) return -1;
    }
    blockFrame = thisBlock->GetParent();
    result = NS_OK;
    if (blockFrame) {
      if (aLockScroll && blockFrame->IsScrollFrame()) return -1;
      it = blockFrame->GetLineIterator();
      if (!it) result = NS_ERROR_FAILURE;
    }
  }
  if (!blockFrame || !it) return -1;

  if (aContainingBlock) *aContainingBlock = blockFrame;
  return it->FindLineContaining(thisBlock);
}

nsresult nsIFrame::GetFrameFromDirection(
    nsDirection aDirection, bool aVisual, bool aJumpLines, bool aScrollViewStop,
    bool aForceEditableRegion, nsIFrame** aOutFrame, int32_t* aOutOffset,
    bool* aOutJumpedLine, bool* aOutMovedOverNonSelectableText) {
  nsresult result;

  if (!aOutFrame || !aOutOffset || !aOutJumpedLine)
    return NS_ERROR_NULL_POINTER;

  nsPresContext* presContext = PresContext();
  *aOutFrame = nullptr;
  *aOutOffset = 0;
  *aOutJumpedLine = false;
  *aOutMovedOverNonSelectableText = false;

  // Find the prev/next selectable frame
  bool selectable = false;
  nsIFrame* traversedFrame = this;
  while (!selectable) {
    nsIFrame* blockFrame;

    int32_t thisLine =
        nsFrame::GetLineNumber(traversedFrame, aScrollViewStop, &blockFrame);
    if (thisLine < 0) return NS_ERROR_FAILURE;

    nsAutoLineIterator it = blockFrame->GetLineIterator();
    NS_ASSERTION(it, "GetLineNumber() succeeded but no block frame?");

    bool atLineEdge;
    nsIFrame* firstFrame;
    nsIFrame* lastFrame;
    if (aVisual && presContext->BidiEnabled()) {
      bool lineIsRTL = it->GetDirection();
      bool isReordered;
      result =
          it->CheckLineOrder(thisLine, &isReordered, &firstFrame, &lastFrame);
      nsIFrame** framePtr =
          aDirection == eDirPrevious ? &firstFrame : &lastFrame;
      if (*framePtr) {
        bool frameIsRTL =
            (nsBidiPresUtils::FrameDirection(*framePtr) == NSBIDI_RTL);
        if ((frameIsRTL == lineIsRTL) == (aDirection == eDirPrevious)) {
          nsFrame::GetFirstLeaf(presContext, framePtr);
        } else {
          nsFrame::GetLastLeaf(presContext, framePtr);
        }
        atLineEdge = *framePtr == traversedFrame;
      } else {
        atLineEdge = true;
      }
    } else {
      nsRect nonUsedRect;
      int32_t lineFrameCount;
      result = it->GetLine(thisLine, &firstFrame, &lineFrameCount, nonUsedRect);
      if (NS_FAILED(result)) return result;

      if (aDirection == eDirPrevious) {
        nsFrame::GetFirstLeaf(presContext, &firstFrame);
        atLineEdge = firstFrame == traversedFrame;
      } else {  // eDirNext
        lastFrame = firstFrame;
        for (; lineFrameCount > 1; lineFrameCount--) {
          result = it->GetNextSiblingOnLine(lastFrame, thisLine);
          if (NS_FAILED(result) || !lastFrame) {
            NS_ERROR("should not be reached nsFrame");
            return NS_ERROR_FAILURE;
          }
        }
        nsFrame::GetLastLeaf(presContext, &lastFrame);
        atLineEdge = lastFrame == traversedFrame;
      }
    }

    if (atLineEdge) {
      *aOutJumpedLine = true;
      if (!aJumpLines)
        return NS_ERROR_FAILURE;  // we are done. cannot jump lines
    }

    nsCOMPtr<nsIFrameEnumerator> frameTraversal;
    result = NS_NewFrameTraversal(
        getter_AddRefs(frameTraversal), presContext, traversedFrame, eLeaf,
        aVisual && presContext->BidiEnabled(), aScrollViewStop,
        true,  // aFollowOOFs
        false  // aSkipPopupChecks
    );
    if (NS_FAILED(result)) return result;

    if (aDirection == eDirNext)
      frameTraversal->Next();
    else
      frameTraversal->Prev();

    traversedFrame = frameTraversal->CurrentItem();
    if (!traversedFrame) {
      return NS_ERROR_FAILURE;
    }

    auto IsSelectable = [aForceEditableRegion](const nsIFrame* aFrame) {
      if (!aFrame->IsSelectable(nullptr)) {
        return false;
      }
      return !aForceEditableRegion || aFrame->GetContent()->IsEditable();
    };

    // Skip brFrames, but only we can select something before hitting the end of
    // the line or a non-selectable region.
    if (atLineEdge && aDirection == eDirPrevious &&
        traversedFrame->IsBrFrame()) {
      bool canSkipBr = false;
      for (nsIFrame* current = traversedFrame->GetPrevSibling(); current;
           current = current->GetPrevSibling()) {
        if (!current->IsBlockOutside() && IsSelectable(current)) {
          canSkipBr = true;
          break;
        }
      }
      if (canSkipBr) {
        continue;
      }
    }

    selectable = IsSelectable(traversedFrame);
    if (!selectable) {
      *aOutMovedOverNonSelectableText = true;
    }
  }  // while (!selectable)

  *aOutOffset = (aDirection == eDirNext) ? 0 : -1;

  if (aVisual && IsReversedDirectionFrame(traversedFrame)) {
    // The new frame is reverse-direction, go to the other end
    *aOutOffset = -1 - *aOutOffset;
  }
  *aOutFrame = traversedFrame;
  return NS_OK;
}

nsView* nsIFrame::GetClosestView(nsPoint* aOffset) const {
  nsPoint offset(0, 0);
  for (const nsIFrame* f = this; f; f = f->GetParent()) {
    if (f->HasView()) {
      if (aOffset) *aOffset = offset;
      return f->GetView();
    }
    offset += f->GetPosition();
  }

  MOZ_ASSERT_UNREACHABLE("No view on any parent?  How did that happen?");
  return nullptr;
}

/* virtual */
void nsFrame::ChildIsDirty(nsIFrame* aChild) {
  MOZ_ASSERT_UNREACHABLE(
      "should never be called on a frame that doesn't "
      "inherit from nsContainerFrame");
}

#ifdef ACCESSIBILITY
a11y::AccType nsFrame::AccessibleType() {
  if (IsTableCaption() && !GetRect().IsEmpty()) {
    return a11y::eHTMLCaptionType;
  }
  return a11y::eNoType;
}
#endif

bool nsIFrame::ClearOverflowRects() {
  if (mOverflow.mType == NS_FRAME_OVERFLOW_NONE) {
    return false;
  }
  if (mOverflow.mType == NS_FRAME_OVERFLOW_LARGE) {
    RemoveProperty(OverflowAreasProperty());
  }
  mOverflow.mType = NS_FRAME_OVERFLOW_NONE;
  return true;
}

/** Set the overflowArea rect, storing it as deltas or a separate rect
 * depending on its size in relation to the primary frame rect.
 */
bool nsIFrame::SetOverflowAreas(const nsOverflowAreas& aOverflowAreas) {
  if (mOverflow.mType == NS_FRAME_OVERFLOW_LARGE) {
    nsOverflowAreas* overflow = GetOverflowAreasProperty();
    bool changed = *overflow != aOverflowAreas;
    *overflow = aOverflowAreas;

    // Don't bother with converting to the deltas form if we already
    // have a property.
    return changed;
  }

  const nsRect& vis = aOverflowAreas.VisualOverflow();
  uint32_t l = -vis.x,                 // left edge: positive delta is leftwards
      t = -vis.y,                      // top: positive is upwards
      r = vis.XMost() - mRect.width,   // right: positive is rightwards
      b = vis.YMost() - mRect.height;  // bottom: positive is downwards
  if (aOverflowAreas.ScrollableOverflow().IsEqualEdges(
          nsRect(nsPoint(0, 0), GetSize())) &&
      l <= NS_FRAME_OVERFLOW_DELTA_MAX && t <= NS_FRAME_OVERFLOW_DELTA_MAX &&
      r <= NS_FRAME_OVERFLOW_DELTA_MAX && b <= NS_FRAME_OVERFLOW_DELTA_MAX &&
      // we have to check these against zero because we *never* want to
      // set a frame as having no overflow in this function.  This is
      // because FinishAndStoreOverflow calls this function prior to
      // SetRect based on whether the overflow areas match aNewSize.
      // In the case where the overflow areas exactly match mRect but
      // do not match aNewSize, we need to store overflow in a property
      // so that our eventual SetRect/SetSize will know that it has to
      // reset our overflow areas.
      (l | t | r | b) != 0) {
    VisualDeltas oldDeltas = mOverflow.mVisualDeltas;
    // It's a "small" overflow area so we store the deltas for each edge
    // directly in the frame, rather than allocating a separate rect.
    // If they're all zero, that's fine; we're setting things to
    // no-overflow.
    mOverflow.mVisualDeltas.mLeft = l;
    mOverflow.mVisualDeltas.mTop = t;
    mOverflow.mVisualDeltas.mRight = r;
    mOverflow.mVisualDeltas.mBottom = b;
    // There was no scrollable overflow before, and there isn't now.
    return oldDeltas != mOverflow.mVisualDeltas;
  } else {
    bool changed = !aOverflowAreas.ScrollableOverflow().IsEqualEdges(
                       nsRect(nsPoint(0, 0), GetSize())) ||
                   !aOverflowAreas.VisualOverflow().IsEqualEdges(
                       GetVisualOverflowFromDeltas());

    // it's a large overflow area that we need to store as a property
    mOverflow.mType = NS_FRAME_OVERFLOW_LARGE;
    AddProperty(OverflowAreasProperty(), new nsOverflowAreas(aOverflowAreas));
    return changed;
  }
}

/**
 * Compute the union of the border boxes of aFrame and its descendants,
 * in aFrame's coordinate space (if aApplyTransform is false) or its
 * post-transform coordinate space (if aApplyTransform is true).
 */
static nsRect UnionBorderBoxes(
    nsIFrame* aFrame, bool aApplyTransform, bool& aOutValid,
    const nsSize* aSizeOverride = nullptr,
    const nsOverflowAreas* aOverflowOverride = nullptr) {
  const nsRect bounds(nsPoint(0, 0),
                      aSizeOverride ? *aSizeOverride : aFrame->GetSize());

  // The SVG container frames besides SVGTextFrame do not maintain
  // an accurate mRect. It will make the outline be larger than
  // we expect, we need to make them narrow to their children's outline.
  // aOutValid is set to false if the returned nsRect is not valid
  // and should not be included in the outline rectangle.
  aOutValid = !(aFrame->GetStateBits() & NS_FRAME_SVG_LAYOUT) ||
              !aFrame->IsFrameOfType(nsIFrame::eSVGContainer) ||
              aFrame->IsSVGTextFrame();

  nsRect u;

  if (!aFrame->FrameMaintainsOverflow()) {
    return u;
  }

  // Start from our border-box, transformed.  See comment below about
  // transform of children.
  bool doTransform = aApplyTransform && aFrame->IsTransformed();
  TransformReferenceBox boundsRefBox(nullptr, bounds);
  if (doTransform) {
    u = nsDisplayTransform::TransformRect(bounds, aFrame, boundsRefBox);
  } else {
    u = bounds;
  }

  // Only iterate through the children if the overflow areas suggest
  // that we might need to, and if the frame doesn't clip its overflow
  // anyway.
  if (aOverflowOverride) {
    if (!doTransform &&
        bounds.IsEqualEdges(aOverflowOverride->VisualOverflow()) &&
        bounds.IsEqualEdges(aOverflowOverride->ScrollableOverflow())) {
      return u;
    }
  } else {
    if (!doTransform && bounds.IsEqualEdges(aFrame->GetVisualOverflowRect()) &&
        bounds.IsEqualEdges(aFrame->GetScrollableOverflowRect())) {
      return u;
    }
  }
  const nsStyleDisplay* disp = aFrame->StyleDisplay();
  LayoutFrameType fType = aFrame->Type();
  if (nsFrame::ShouldApplyOverflowClipping(aFrame, disp) ||
      fType == LayoutFrameType::Scroll ||
      fType == LayoutFrameType::ListControl ||
      fType == LayoutFrameType::SVGOuterSVG) {
    return u;
  }

  const nsStyleEffects* effects = aFrame->StyleEffects();
  Maybe<nsRect> clipPropClipRect =
      aFrame->GetClipPropClipRect(disp, effects, bounds.Size());

  // Iterate over all children except pop-up, absolutely-positioned,
  // float, and overflow ones.
  const nsIFrame::ChildListIDs skip = {
      nsIFrame::kPopupList,    nsIFrame::kSelectPopupList,
      nsIFrame::kAbsoluteList, nsIFrame::kFixedList,
      nsIFrame::kFloatList,    nsIFrame::kOverflowList};
  for (nsIFrame::ChildListIterator childLists(aFrame); !childLists.IsDone();
       childLists.Next()) {
    if (skip.contains(childLists.CurrentID())) {
      continue;
    }

    nsFrameList children = childLists.CurrentList();
    for (nsFrameList::Enumerator e(children); !e.AtEnd(); e.Next()) {
      nsIFrame* child = e.get();
      if (child->IsPlaceholderFrame()) {
        continue;
      }

      // Note that passing |true| for aApplyTransform when
      // child->Combines3DTransformWithAncestors() is incorrect if our
      // aApplyTransform is false... but the opposite would be as
      // well.  This is because elements within a preserve-3d scene
      // are always transformed up to the top of the scene.  This
      // means we don't have a mechanism for getting a transform up to
      // an intermediate point within the scene.  We choose to
      // over-transform rather than under-transform because this is
      // consistent with other overflow areas.
      bool validRect = true;
      nsRect childRect =
          UnionBorderBoxes(child, true, validRect) + child->GetPosition();

      if (!validRect) {
        continue;
      }

      if (clipPropClipRect) {
        // Intersect with the clip before transforming.
        childRect.IntersectRect(childRect, *clipPropClipRect);
      }

      // Note that we transform each child separately according to
      // aFrame's transform, and then union, which gives a different
      // (smaller) result from unioning and then transforming the
      // union.  This doesn't match the way we handle overflow areas
      // with 2-D transforms, though it does match the way we handle
      // overflow areas in preserve-3d 3-D scenes.
      if (doTransform && !child->Combines3DTransformWithAncestors()) {
        childRect =
            nsDisplayTransform::TransformRect(childRect, aFrame, boundsRefBox);
      }

      // If a SVGContainer has a non-SVGContainer child, we assign
      // its child's outline to this SVGContainer directly.
      if (!aOutValid && validRect) {
        u = childRect;
        aOutValid = true;
      } else {
        u.UnionRectEdges(u, childRect);
      }
    }
  }

  return u;
}

static void ComputeAndIncludeOutlineArea(nsIFrame* aFrame,
                                         nsOverflowAreas& aOverflowAreas,
                                         const nsSize& aNewSize) {
  const nsStyleOutline* outline = aFrame->StyleOutline();
  if (!outline->ShouldPaintOutline()) {
    return;
  }

  // When the outline property is set on a :-moz-block-inside-inline-wrapper
  // pseudo-element, it inherited that outline from the inline that was broken
  // because it contained a block.  In that case, we don't want a really wide
  // outline if the block inside the inline is narrow, so union the actual
  // contents of the anonymous blocks.
  nsIFrame* frameForArea = aFrame;
  do {
    PseudoStyleType pseudoType = frameForArea->Style()->GetPseudoType();
    if (pseudoType != PseudoStyleType::mozBlockInsideInlineWrapper) break;
    // If we're done, we really want it and all its later siblings.
    frameForArea = frameForArea->PrincipalChildList().FirstChild();
    NS_ASSERTION(frameForArea, "anonymous block with no children?");
  } while (frameForArea);

  // Find the union of the border boxes of all descendants, or in
  // the block-in-inline case, all descendants we care about.
  //
  // Note that the interesting perspective-related cases are taken
  // care of by the code that handles those issues for overflow
  // calling FinishAndStoreOverflow again, which in turn calls this
  // function again.  We still need to deal with preserve-3d a bit.
  nsRect innerRect;
  bool validRect;
  if (frameForArea == aFrame) {
    innerRect =
        UnionBorderBoxes(aFrame, false, validRect, &aNewSize, &aOverflowAreas);
  } else {
    for (; frameForArea; frameForArea = frameForArea->GetNextSibling()) {
      nsRect r(UnionBorderBoxes(frameForArea, true, validRect));

      // Adjust for offsets transforms up to aFrame's pre-transform
      // (i.e., normal) coordinate space; see comments in
      // UnionBorderBoxes for some of the subtlety here.
      for (nsIFrame *f = frameForArea, *parent = f->GetParent();
           /* see middle of loop */; f = parent, parent = f->GetParent()) {
        r += f->GetPosition();
        if (parent == aFrame) {
          break;
        }
        if (parent->IsTransformed() && !f->Combines3DTransformWithAncestors()) {
          TransformReferenceBox refBox(parent);
          r = nsDisplayTransform::TransformRect(r, parent, refBox);
        }
      }

      innerRect.UnionRect(innerRect, r);
    }
  }

  // Keep this code in sync with GetOutlineInnerRect in nsCSSRendering.cpp.
  SetOrUpdateRectValuedProperty(aFrame, nsIFrame::OutlineInnerRectProperty(),
                                innerRect);
  const nscoord offset = outline->mOutlineOffset.ToAppUnits();
  nsRect outerRect(innerRect);
  bool useOutlineAuto = false;
  if (StaticPrefs::layout_css_outline_style_auto_enabled()) {
    useOutlineAuto = outline->mOutlineStyle.IsAuto();
    if (MOZ_UNLIKELY(useOutlineAuto)) {
      nsPresContext* presContext = aFrame->PresContext();
      nsITheme* theme = presContext->Theme();
      if (theme->ThemeSupportsWidget(presContext, aFrame,
                                     StyleAppearance::FocusOutline)) {
        outerRect.Inflate(offset);
        theme->GetWidgetOverflow(presContext->DeviceContext(), aFrame,
                                 StyleAppearance::FocusOutline, &outerRect);
      } else {
        useOutlineAuto = false;
      }
    }
  }
  if (MOZ_LIKELY(!useOutlineAuto)) {
    nscoord width = outline->GetOutlineWidth();
    outerRect.Inflate(width + offset);
  }

  nsRect& vo = aOverflowAreas.VisualOverflow();
  vo.UnionRectEdges(vo, innerRect.Union(outerRect));
}

bool nsIFrame::FinishAndStoreOverflow(nsOverflowAreas& aOverflowAreas,
                                      nsSize aNewSize, nsSize* aOldSize,
                                      const nsStyleDisplay* aStyleDisplay) {
  MOZ_ASSERT(FrameMaintainsOverflow(),
             "Don't call - overflow rects not maintained on these SVG frames");

  const nsStyleDisplay* disp = StyleDisplayWithOptionalParam(aStyleDisplay);
  bool hasTransform = IsTransformed(disp);

  nsRect bounds(nsPoint(0, 0), aNewSize);
  // Store the passed in overflow area if we are a preserve-3d frame or we have
  // a transform, and it's not just the frame bounds.
  if (hasTransform || Combines3DTransformWithAncestors(disp)) {
    if (!aOverflowAreas.VisualOverflow().IsEqualEdges(bounds) ||
        !aOverflowAreas.ScrollableOverflow().IsEqualEdges(bounds)) {
      nsOverflowAreas* initial =
          GetProperty(nsIFrame::InitialOverflowProperty());
      if (!initial) {
        AddProperty(nsIFrame::InitialOverflowProperty(),
                    new nsOverflowAreas(aOverflowAreas));
      } else if (initial != &aOverflowAreas) {
        *initial = aOverflowAreas;
      }
    } else {
      RemoveProperty(nsIFrame::InitialOverflowProperty());
    }
#ifdef DEBUG
    SetProperty(nsIFrame::DebugInitialOverflowPropertyApplied(), true);
#endif
  } else {
#ifdef DEBUG
    RemoveProperty(nsIFrame::DebugInitialOverflowPropertyApplied());
#endif
  }

  nsSize oldSize = mRect.Size();
  bool sizeChanged = ((aOldSize ? *aOldSize : oldSize) != aNewSize);

  // Our frame size may not have been computed and set yet, but code under
  // functions such as ComputeEffectsRect (which we're about to call) use the
  // values that are stored in our frame rect to compute their results.  We
  // need the results from those functions to be based on the frame size that
  // we *will* have, so we temporarily set our frame size here before calling
  // those functions.
  //
  // XXX Someone should document here why we revert the frame size before we
  // return rather than just leaving it set.
  //
  // We pass false here to avoid invalidating display items for this temporary
  // change. We sometimes reflow frames multiple times, with the final size
  // being the same as the initial. The single call to SetSize after reflow is
  // done will take care of invalidating display items if the size has actually
  // changed.
  SetSize(aNewSize, false);

  const bool applyOverflowClipping =
      nsFrame::ShouldApplyOverflowClipping(this, disp);

  if (ChildrenHavePerspective(disp) && sizeChanged) {
    RecomputePerspectiveChildrenOverflow(this);

    if (!applyOverflowClipping) {
      aOverflowAreas.SetAllTo(bounds);
      DebugOnly<bool> ok = ComputeCustomOverflow(aOverflowAreas);

      // ComputeCustomOverflow() should not return false, when
      // FrameMaintainsOverflow() returns true.
      MOZ_ASSERT(ok, "FrameMaintainsOverflow() != ComputeCustomOverflow()");

      UnionChildOverflow(aOverflowAreas);
    }
  }

  // This is now called FinishAndStoreOverflow() instead of
  // StoreOverflow() because frame-generic ways of adding overflow
  // can happen here, e.g. CSS2 outline and native theme.
  // If the overflow area width or height is nscoord_MAX, then a
  // saturating union may have encounted an overflow, so the overflow may not
  // contain the frame border-box. Don't warn in that case.
  // Don't warn for SVG either, since SVG doesn't need the overflow area
  // to contain the frame bounds.
  NS_FOR_FRAME_OVERFLOW_TYPES(otype) {
    DebugOnly<nsRect*> r = &aOverflowAreas.Overflow(otype);
    NS_ASSERTION(aNewSize.width == 0 || aNewSize.height == 0 ||
                     r->width == nscoord_MAX || r->height == nscoord_MAX ||
                     (mState & NS_FRAME_SVG_LAYOUT) ||
                     r->Contains(nsRect(nsPoint(0, 0), aNewSize)),
                 "Computed overflow area must contain frame bounds");
  }

  // If we clip our children, clear accumulated overflow area. The
  // children are actually clipped to the padding-box, but since the
  // overflow area should include the entire border-box, just set it to
  // the border-box here.
  NS_ASSERTION((disp->mOverflowY == StyleOverflow::MozHiddenUnscrollable) ==
                   (disp->mOverflowX == StyleOverflow::MozHiddenUnscrollable),
               "If one overflow is clip, the other should be too");
  if (applyOverflowClipping) {
    // The contents are actually clipped to the padding area
    aOverflowAreas.SetAllTo(bounds);
  }

  // Overflow area must always include the frame's top-left and bottom-right,
  // even if the frame rect is empty (so we can scroll to those positions).
  // Pending a real fix for bug 426879, don't do this for inline frames
  // with zero width.
  // Do not do this for SVG either, since it will usually massively increase
  // the area unnecessarily.
  if ((aNewSize.width != 0 || !IsInlineFrame()) &&
      !(GetStateBits() & NS_FRAME_SVG_LAYOUT)) {
    NS_FOR_FRAME_OVERFLOW_TYPES(otype) {
      nsRect& o = aOverflowAreas.Overflow(otype);
      o.UnionRectEdges(o, bounds);
    }
  }

  // Note that StyleOverflow::MozHiddenUnscrollable doesn't clip the frame
  // background, so we add theme background overflow here so it's not clipped.
  if (!::IsXULBoxWrapped(this) && IsThemed(disp)) {
    nsRect r(bounds);
    nsPresContext* presContext = PresContext();
    if (presContext->Theme()->GetWidgetOverflow(presContext->DeviceContext(),
                                                this, disp->mAppearance, &r)) {
      nsRect& vo = aOverflowAreas.VisualOverflow();
      vo.UnionRectEdges(vo, r);
    }
  }

  ComputeAndIncludeOutlineArea(this, aOverflowAreas, aNewSize);

  // Nothing in here should affect scrollable overflow.
  aOverflowAreas.VisualOverflow() =
      ComputeEffectsRect(this, aOverflowAreas.VisualOverflow(), aNewSize);

  // Absolute position clipping
  const nsStyleEffects* effects = StyleEffects();
  Maybe<nsRect> clipPropClipRect = GetClipPropClipRect(disp, effects, aNewSize);
  if (clipPropClipRect) {
    NS_FOR_FRAME_OVERFLOW_TYPES(otype) {
      nsRect& o = aOverflowAreas.Overflow(otype);
      o.IntersectRect(o, *clipPropClipRect);
    }
  }

  /* If we're transformed, transform the overflow rect by the current
   * transformation. */
  if (hasTransform) {
    SetProperty(nsIFrame::PreTransformOverflowAreasProperty(),
                new nsOverflowAreas(aOverflowAreas));

    if (Combines3DTransformWithAncestors(disp)) {
      /* If we're a preserve-3d leaf frame, then our pre-transform overflow
       * should be correct. Our post-transform overflow is empty though, because
       * we only contribute to the overflow area of the preserve-3d root frame.
       * If we're an intermediate frame then the pre-transform overflow should
       * contain all our non-preserve-3d children, which is what we want. Again
       * we have no post-transform overflow.
       */
      aOverflowAreas.SetAllTo(nsRect());
    } else {
      TransformReferenceBox refBox(this);
      NS_FOR_FRAME_OVERFLOW_TYPES(otype) {
        nsRect& o = aOverflowAreas.Overflow(otype);
        o = nsDisplayTransform::TransformRect(o, this, refBox);
      }

      /* If we're the root of the 3d context, then we want to include the
       * overflow areas of all the participants. This won't have happened yet as
       * the code above set their overflow area to empty. Manually collect these
       * overflow areas now.
       */
      if (Extend3DContext(disp, effects)) {
        ComputePreserve3DChildrenOverflow(aOverflowAreas);
      }
    }
  } else {
    RemoveProperty(nsIFrame::PreTransformOverflowAreasProperty());
  }

  /* Revert the size change in case some caller is depending on this. */
  SetSize(oldSize, false);

  bool anyOverflowChanged;
  if (aOverflowAreas != nsOverflowAreas(bounds, bounds)) {
    anyOverflowChanged = SetOverflowAreas(aOverflowAreas);
  } else {
    anyOverflowChanged = ClearOverflowRects();
  }

  if (anyOverflowChanged) {
    SVGObserverUtils::InvalidateDirectRenderingObservers(this);
    if (IsBlockFrameOrSubclass() &&
        TextOverflow::CanHaveOverflowMarkers(this)) {
      DiscardDisplayItems(this, [](nsDisplayItemBase* aItem) {
        return aItem->GetType() == DisplayItemType::TYPE_TEXT_OVERFLOW;
      });
      SchedulePaint(PAINT_DEFAULT, false);
    }
  }
  return anyOverflowChanged;
}

void nsIFrame::RecomputePerspectiveChildrenOverflow(
    const nsIFrame* aStartFrame) {
  nsIFrame::ChildListIterator lists(this);
  for (; !lists.IsDone(); lists.Next()) {
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      nsIFrame* child = childFrames.get();
      if (!child->FrameMaintainsOverflow()) {
        continue;  // frame does not maintain overflow rects
      }
      if (child->HasPerspective()) {
        nsOverflowAreas* overflow =
            child->GetProperty(nsIFrame::InitialOverflowProperty());
        nsRect bounds(nsPoint(0, 0), child->GetSize());
        if (overflow) {
          nsOverflowAreas overflowCopy = *overflow;
          child->FinishAndStoreOverflow(overflowCopy, bounds.Size());
        } else {
          nsOverflowAreas boundsOverflow;
          boundsOverflow.SetAllTo(bounds);
          child->FinishAndStoreOverflow(boundsOverflow, bounds.Size());
        }
      } else if (child->GetContainingBlock(SKIP_SCROLLED_FRAME) ==
                 aStartFrame) {
        // If a frame is using perspective, then the size used to compute
        // perspective-origin is the size of the frame belonging to its parent
        // style. We must find any descendant frames using our size
        // (by recursing into frames that have the same containing block)
        // to update their overflow rects too.
        child->RecomputePerspectiveChildrenOverflow(aStartFrame);
      }
    }
  }
}

void nsIFrame::ComputePreserve3DChildrenOverflow(
    nsOverflowAreas& aOverflowAreas) {
  // Find all descendants that participate in the 3d context, and include their
  // overflow. These descendants have an empty overflow, so won't have been
  // included in the normal overflow calculation. Any children that don't
  // participate have normal overflow, so will have been included already.

  nsRect childVisual;
  nsRect childScrollable;
  nsIFrame::ChildListIterator lists(this);
  for (; !lists.IsDone(); lists.Next()) {
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      nsIFrame* child = childFrames.get();

      // If this child participates in the 3d context, then take the
      // pre-transform region (which contains all descendants that aren't
      // participating in the 3d context) and transform it into the 3d context
      // root coordinate space.
      const nsStyleDisplay* childDisp = child->StyleDisplay();
      if (child->Combines3DTransformWithAncestors(childDisp)) {
        nsOverflowAreas childOverflow = child->GetOverflowAreasRelativeToSelf();
        TransformReferenceBox refBox(child);
        NS_FOR_FRAME_OVERFLOW_TYPES(otype) {
          nsRect& o = childOverflow.Overflow(otype);
          o = nsDisplayTransform::TransformRect(o, child, refBox);
        }

        aOverflowAreas.UnionWith(childOverflow);

        // If this child also extends the 3d context, then recurse into it
        // looking for more participants.
        if (child->Extend3DContext(childDisp, child->StyleEffects())) {
          child->ComputePreserve3DChildrenOverflow(aOverflowAreas);
        }
      }
    }
  }
}

int32_t nsIFrame::ZIndex() const {
  if (!IsAbsPosContainingBlock() && !IsFlexOrGridItem()) {
    return 0;
  }

  const nsStylePosition* position = StylePosition();
  if (position->mZIndex.IsInteger()) {
    return position->mZIndex.AsInteger();
  }
  MOZ_ASSERT(position->mZIndex.IsAuto());
  // sort the auto and 0 elements together
  return 0;
}

bool nsIFrame::IsScrollAnchor(ScrollAnchorContainer** aOutContainer) {
  if (!mInScrollAnchorChain) {
    return false;
  }

  ScrollAnchorContainer* container = ScrollAnchorContainer::FindFor(this);
  if (container->AnchorNode() != this) {
    return false;
  }

  if (aOutContainer) {
    *aOutContainer = container;
  }
  return true;
}

bool nsIFrame::IsInScrollAnchorChain() const { return mInScrollAnchorChain; }

void nsIFrame::SetInScrollAnchorChain(bool aInChain) {
  mInScrollAnchorChain = aInChain;
}

uint32_t nsIFrame::GetDepthInFrameTree() const {
  uint32_t result = 0;
  for (nsContainerFrame* ancestor = GetParent(); ancestor;
       ancestor = ancestor->GetParent()) {
    result++;
  }
  return result;
}

void nsFrame::ConsiderChildOverflow(nsOverflowAreas& aOverflowAreas,
                                    nsIFrame* aChildFrame) {
  if (StyleDisplay()->IsContainLayout() &&
      IsFrameOfType(eSupportsContainLayoutAndPaint)) {
    // If we have layout containment and are not a non-atomic, inline-level
    // principal box, we should only consider our child's visual (ink) overflow,
    // leaving the scrollable regions of the parent unaffected.
    // Note: scrollable overflow is a subset of visual overflow,
    // so this has the same affect as unioning the child's visual and
    // scrollable overflow with the parent's visual overflow.
    nsRect childVisual = aChildFrame->GetVisualOverflowRect();
    nsOverflowAreas combined = nsOverflowAreas(childVisual, nsRect());
    aOverflowAreas.UnionWith(combined + aChildFrame->GetPosition());
  } else {
    aOverflowAreas.UnionWith(aChildFrame->GetOverflowAreas() +
                             aChildFrame->GetPosition());
  }
}

bool nsFrame::ShouldAvoidBreakInside(const ReflowInput& aReflowInput) const {
  const auto* disp = StyleDisplay();
  return !aReflowInput.mFlags.mIsTopOfPage &&
         StyleBreakWithin::Avoid == disp->mBreakInside &&
         !(HasAnyStateBits(NS_FRAME_OUT_OF_FLOW) &&
           IsAbsolutelyPositioned(disp)) &&
         !GetPrevInFlow();
}

/**
 * This function takes a frame that is part of a block-in-inline split,
 * and _if_ that frame is an anonymous block created by an ib split it
 * returns the block's preceding inline.  This is needed because the
 * split inline's style is the parent of the anonymous block's style.
 *
 * If aFrame is not an anonymous block, null is returned.
 */
static nsIFrame* GetIBSplitSiblingForAnonymousBlock(const nsIFrame* aFrame) {
  MOZ_ASSERT(aFrame, "Must have a non-null frame!");
  NS_ASSERTION(aFrame->GetStateBits() & NS_FRAME_PART_OF_IBSPLIT,
               "GetIBSplitSibling should only be called on ib-split frames");

  if (aFrame->Style()->GetPseudoType() !=
      PseudoStyleType::mozBlockInsideInlineWrapper) {
    // it's not an anonymous block
    return nullptr;
  }

  // Find the first continuation of the frame.  (Ugh.  This ends up
  // being O(N^2) when it is called O(N) times.)
  aFrame = aFrame->FirstContinuation();

  /*
   * Now look up the nsGkAtoms::IBSplitPrevSibling
   * property.
   */
  nsIFrame* ibSplitSibling =
      aFrame->GetProperty(nsIFrame::IBSplitPrevSibling());
  NS_ASSERTION(ibSplitSibling, "Broken frame tree?");
  return ibSplitSibling;
}

/**
 * Get the parent, corrected for the mangled frame tree resulting from
 * having a block within an inline.  The result only differs from the
 * result of |GetParent| when |GetParent| returns an anonymous block
 * that was created for an element that was 'display: inline' because
 * that element contained a block.
 *
 * Also skip anonymous scrolled-content parents; inherit directly from the
 * outer scroll frame.
 *
 * Also skip NAC parents if the child frame is NAC.
 */
static nsIFrame* GetCorrectedParent(const nsIFrame* aFrame) {
  nsIFrame* parent = aFrame->GetParent();
  if (!parent) {
    return nullptr;
  }

  // For a table caption we want the _inner_ table frame (unless it's anonymous)
  // as the style parent.
  if (aFrame->IsTableCaption()) {
    nsIFrame* innerTable = parent->PrincipalChildList().FirstChild();
    if (!innerTable->Style()->IsAnonBox()) {
      return innerTable;
    }
  }

  // Table wrappers are always anon boxes; if we're in here for an outer
  // table, that actually means its the _inner_ table that wants to
  // know its parent. So get the pseudo of the inner in that case.
  auto pseudo = aFrame->Style()->GetPseudoType();
  if (pseudo == PseudoStyleType::tableWrapper) {
    pseudo =
        aFrame->PrincipalChildList().FirstChild()->Style()->GetPseudoType();
  }

  // Prevent a NAC pseudo-element from inheriting from its NAC parent, and
  // inherit from the NAC generator element instead.
  if (pseudo != PseudoStyleType::NotPseudo) {
    MOZ_ASSERT(aFrame->GetContent());
    Element* element = Element::FromNode(aFrame->GetContent());
    // Make sure to avoid doing the fixup for non-element-backed pseudos like
    // ::first-line and such.
    if (element && !element->IsRootOfNativeAnonymousSubtree() &&
        element->GetPseudoElementType() == aFrame->Style()->GetPseudoType()) {
      while (parent->GetContent() &&
             !parent->GetContent()->IsRootOfAnonymousSubtree()) {
        parent = parent->GetInFlowParent();
      }
      parent = parent->GetInFlowParent();
    }
  }

  return nsFrame::CorrectStyleParentFrame(parent, pseudo);
}

/* static */
nsIFrame* nsFrame::CorrectStyleParentFrame(nsIFrame* aProspectiveParent,
                                           PseudoStyleType aChildPseudo) {
  MOZ_ASSERT(aProspectiveParent, "Must have a prospective parent");

  if (aChildPseudo != PseudoStyleType::NotPseudo) {
    // Non-inheriting anon boxes have no style parent frame at all.
    if (PseudoStyle::IsNonInheritingAnonBox(aChildPseudo)) {
      return nullptr;
    }

    // Other anon boxes are parented to their actual parent already, except
    // for non-elements.  Those should not be treated as an anon box.
    if (PseudoStyle::IsAnonBox(aChildPseudo) &&
        !nsCSSAnonBoxes::IsNonElement(aChildPseudo)) {
      NS_ASSERTION(aChildPseudo != PseudoStyleType::mozBlockInsideInlineWrapper,
                   "Should have dealt with kids that have "
                   "NS_FRAME_PART_OF_IBSPLIT elsewhere");
      return aProspectiveParent;
    }
  }

  // Otherwise, walk up out of all anon boxes.  For placeholder frames, walk out
  // of all pseudo-elements as well.  Otherwise ReparentComputedStyle could
  // cause style data to be out of sync with the frame tree.
  nsIFrame* parent = aProspectiveParent;
  do {
    if (parent->GetStateBits() & NS_FRAME_PART_OF_IBSPLIT) {
      nsIFrame* sibling = GetIBSplitSiblingForAnonymousBlock(parent);

      if (sibling) {
        // |parent| was a block in an {ib} split; use the inline as
        // |the style parent.
        parent = sibling;
      }
    }

    if (!parent->Style()->IsPseudoOrAnonBox()) {
      return parent;
    }

    if (!parent->Style()->IsAnonBox() && aChildPseudo != PseudoStyleType::MAX) {
      // nsPlaceholderFrame passes in PseudoStyleType::MAX for
      // aChildPseudo (even though that's not a valid pseudo-type) just to
      // trigger this behavior of walking up to the nearest non-pseudo
      // ancestor.
      return parent;
    }

    parent = parent->GetInFlowParent();
  } while (parent);

  if (aProspectiveParent->Style()->GetPseudoType() ==
      PseudoStyleType::viewportScroll) {
    // aProspectiveParent is the scrollframe for a viewport
    // and the kids are the anonymous scrollbars
    return aProspectiveParent;
  }

  // We can get here if the root element is absolutely positioned.
  // We can't test for this very accurately, but it can only happen
  // when the prospective parent is a canvas frame.
  NS_ASSERTION(aProspectiveParent->IsCanvasFrame(),
               "Should have found a parent before this");
  return nullptr;
}

ComputedStyle* nsFrame::DoGetParentComputedStyle(
    nsIFrame** aProviderFrame) const {
  *aProviderFrame = nullptr;

  // Handle display:contents and the root frame, when there's no parent frame
  // to inherit from.
  if (MOZ_LIKELY(mContent)) {
    Element* parentElement = mContent->GetFlattenedTreeParentElement();
    if (MOZ_LIKELY(parentElement)) {
      auto pseudo = Style()->GetPseudoType();
      if (pseudo == PseudoStyleType::NotPseudo || !mContent->IsElement() ||
          (!PseudoStyle::IsAnonBox(pseudo) &&
           // Ensure that we don't return the display:contents style
           // of the parent content for pseudos that have the same content
           // as their primary frame (like -moz-list-bullets do):
           IsPrimaryFrame()) ||
          /* if next is true then it's really a request for the table frame's
             parent context, see nsTable[Outer]Frame::GetParentComputedStyle. */
          pseudo == PseudoStyleType::tableWrapper) {
        // In some edge cases involving display: contents, we may end up here
        // for something that's pending to be reframed. In this case we return
        // the wrong style from here (because we've already lost track of it!),
        // but it's not a big deal as we're going to be reframed anyway.
        if (MOZ_LIKELY(parentElement->HasServoData()) &&
            Servo_Element_IsDisplayContents(parentElement)) {
          RefPtr<ComputedStyle> style =
              ServoStyleSet::ResolveServoStyle(*parentElement);
          // NOTE(emilio): we return a weak reference because the element also
          // holds the style context alive. This is a bit silly (we could've
          // returned a weak ref directly), but it's probably not worth
          // optimizing, given this function has just one caller which is rare,
          // and this path is rare itself.
          return style;
        }
      }
    } else {
      if (Style()->GetPseudoType() == PseudoStyleType::NotPseudo) {
        // We're a frame for the root.  We have no style parent.
        return nullptr;
      }
    }
  }

  if (!(mState & NS_FRAME_OUT_OF_FLOW)) {
    /*
     * If this frame is an anonymous block created when an inline with a block
     * inside it got split, then the parent style is on its preceding inline. We
     * can get to it using GetIBSplitSiblingForAnonymousBlock.
     */
    if (mState & NS_FRAME_PART_OF_IBSPLIT) {
      nsIFrame* ibSplitSibling = GetIBSplitSiblingForAnonymousBlock(this);
      if (ibSplitSibling) {
        return (*aProviderFrame = ibSplitSibling)->Style();
      }
    }

    // If this frame is one of the blocks that split an inline, we must
    // return the "special" inline parent, i.e., the parent that this
    // frame would have if we didn't mangle the frame structure.
    *aProviderFrame = GetCorrectedParent(this);
    return *aProviderFrame ? (*aProviderFrame)->Style() : nullptr;
  }

  // We're an out-of-flow frame.  For out-of-flow frames, we must
  // resolve underneath the placeholder's parent.  The placeholder is
  // reached from the first-in-flow.
  nsPlaceholderFrame* placeholder = FirstInFlow()->GetPlaceholderFrame();
  if (!placeholder) {
    MOZ_ASSERT_UNREACHABLE("no placeholder frame for out-of-flow frame");
    *aProviderFrame = GetCorrectedParent(this);
    return *aProviderFrame ? (*aProviderFrame)->Style() : nullptr;
  }
  return placeholder->GetParentComputedStyleForOutOfFlow(aProviderFrame);
}

void nsFrame::GetLastLeaf(nsPresContext* aPresContext, nsIFrame** aFrame) {
  if (!aFrame || !*aFrame) return;
  nsIFrame* child = *aFrame;
  // if we are a block frame then go for the last line of 'this'
  while (1) {
    child = child->PrincipalChildList().FirstChild();
    if (!child) return;  // nothing to do
    nsIFrame* siblingFrame;
    nsIContent* content;
    // ignore anonymous elements, e.g. mozTableAdd* mozTableRemove*
    // see bug 278197 comment #12 #13 for details
    while ((siblingFrame = child->GetNextSibling()) &&
           (content = siblingFrame->GetContent()) &&
           !content->IsRootOfNativeAnonymousSubtree())
      child = siblingFrame;
    *aFrame = child;
  }
}

void nsFrame::GetFirstLeaf(nsPresContext* aPresContext, nsIFrame** aFrame) {
  if (!aFrame || !*aFrame) return;
  nsIFrame* child = *aFrame;
  while (1) {
    child = child->PrincipalChildList().FirstChild();
    if (!child) return;  // nothing to do
    *aFrame = child;
  }
}

/* virtual */
bool nsIFrame::IsFocusable(int32_t* aTabIndex, bool aWithMouse) {
  int32_t tabIndex = -1;
  if (aTabIndex) {
    *aTabIndex = -1;  // Default for early return is not focusable
  }
  bool isFocusable = false;

  if (mContent && mContent->IsElement() && IsVisibleConsideringAncestors() &&
      Style()->GetPseudoType() != PseudoStyleType::anonymousFlexItem &&
      Style()->GetPseudoType() != PseudoStyleType::anonymousGridItem) {
    const nsStyleUI* ui = StyleUI();
    if (ui->mUserFocus != StyleUserFocus::Ignore &&
        ui->mUserFocus != StyleUserFocus::None) {
      // Pass in default tabindex of -1 for nonfocusable and 0 for focusable
      tabIndex = 0;
    }
    isFocusable = mContent->IsFocusable(&tabIndex, aWithMouse);
    if (!isFocusable && !aWithMouse && IsScrollFrame() &&
        mContent->IsHTMLElement() &&
        !mContent->IsRootOfNativeAnonymousSubtree() && mContent->GetParent() &&
        !mContent->AsElement()->HasAttr(kNameSpaceID_None,
                                        nsGkAtoms::tabindex)) {
      // Elements with scrollable view are focusable with script & tabbable
      // Otherwise you couldn't scroll them with keyboard, which is
      // an accessibility issue (e.g. Section 508 rules)
      // However, we don't make them to be focusable with the mouse,
      // because the extra focus outlines are considered unnecessarily ugly.
      // When clicked on, the selection position within the element
      // will be enough to make them keyboard scrollable.
      nsIScrollableFrame* scrollFrame = do_QueryFrame(this);
      if (scrollFrame && !scrollFrame->IsForTextControlWithNoScrollbars() &&
          !scrollFrame->GetScrollStyles().IsHiddenInBothDirections() &&
          !scrollFrame->GetScrollRange().IsEqualEdges(nsRect(0, 0, 0, 0))) {
        // Scroll bars will be used for overflow
        isFocusable = true;
        tabIndex = 0;
      }
    }
  }

  if (aTabIndex) {
    *aTabIndex = tabIndex;
  }
  return isFocusable;
}

/**
 * @return true if this text frame ends with a newline character which is
 * treated as preformatted. It should return false if this is not a text frame.
 */
bool nsIFrame::HasSignificantTerminalNewline() const { return false; }

static StyleVerticalAlignKeyword ConvertSVGDominantBaselineToVerticalAlign(
    StyleDominantBaseline aDominantBaseline) {
  // Most of these are approximate mappings.
  switch (aDominantBaseline) {
    case StyleDominantBaseline::Hanging:
    case StyleDominantBaseline::TextBeforeEdge:
      return StyleVerticalAlignKeyword::TextTop;
    case StyleDominantBaseline::TextAfterEdge:
    case StyleDominantBaseline::Ideographic:
      return StyleVerticalAlignKeyword::TextBottom;
    case StyleDominantBaseline::Central:
    case StyleDominantBaseline::Middle:
    case StyleDominantBaseline::Mathematical:
      return StyleVerticalAlignKeyword::Middle;
    case StyleDominantBaseline::Auto:
    case StyleDominantBaseline::Alphabetic:
      return StyleVerticalAlignKeyword::Baseline;
    default:
      MOZ_ASSERT_UNREACHABLE("unexpected aDominantBaseline value");
      return StyleVerticalAlignKeyword::Baseline;
  }
}

Maybe<StyleVerticalAlignKeyword> nsIFrame::VerticalAlignEnum() const {
  if (nsSVGUtils::IsInSVGTextSubtree(this)) {
    StyleDominantBaseline dominantBaseline = StyleSVG()->mDominantBaseline;
    return Some(ConvertSVGDominantBaselineToVerticalAlign(dominantBaseline));
  }

  const auto& verticalAlign = StyleDisplay()->mVerticalAlign;
  if (verticalAlign.IsKeyword()) {
    return Some(verticalAlign.AsKeyword());
  }

  return Nothing();
}

NS_IMETHODIMP
nsFrame::RefreshSizeCache(nsBoxLayoutState& aState) {
  // XXXbz this comment needs some rewriting to make sense in the
  // post-reflow-branch world.

  // Ok we need to compute our minimum, preferred, and maximum sizes.
  // 1) Maximum size. This is easy. Its infinite unless it is overloaded by CSS.
  // 2) Preferred size. This is a little harder. This is the size the
  //    block would be if it were laid out on an infinite canvas. So we can
  //    get this by reflowing the block with and INTRINSIC width and height. We
  //    can also do a nice optimization for incremental reflow. If the reflow is
  //    incremental then we can pass a flag to have the block compute the
  //    preferred width for us! Preferred height can just be the minimum height;
  // 3) Minimum size. This is a toughy. We can pass the block a flag asking for
  //    the max element size. That would give us the width. Unfortunately you
  //    can only ask for a maxElementSize during an incremental reflow. So on
  //    other reflows we will just have to use 0. The min height on the other
  //    hand is fairly easy we need to get the largest line height. This can be
  //    done with the line iterator.

  // if we do have a rendering context
  gfxContext* rendContext = aState.GetRenderingContext();
  if (rendContext) {
    nsPresContext* presContext = aState.PresContext();

    // If we don't have any HTML constraints and it's a resize, then nothing in
    // the block could have changed, so no refresh is necessary.
    nsBoxLayoutMetrics* metrics = BoxMetrics();
    if (!DoesNeedRecalc(metrics->mBlockPrefSize)) return NS_OK;

    // the rect we plan to size to.
    nsRect rect = GetRect();

    nsMargin bp(0, 0, 0, 0);
    GetXULBorderAndPadding(bp);

    {
      // If we're a container for font size inflation, then shrink
      // wrapping inside of us should not apply font size inflation.
      AutoMaybeDisableFontInflation an(this);

      metrics->mBlockPrefSize.width =
          GetPrefISize(rendContext) + bp.LeftRight();
      metrics->mBlockMinSize.width = GetMinISize(rendContext) + bp.LeftRight();
    }

    // do the nasty.
    const WritingMode wm = aState.OuterReflowInput()
                               ? aState.OuterReflowInput()->GetWritingMode()
                               : GetWritingMode();
    ReflowOutput desiredSize(wm);
    BoxReflow(aState, presContext, desiredSize, rendContext, rect.x, rect.y,
              metrics->mBlockPrefSize.width, NS_UNCONSTRAINEDSIZE);

    metrics->mBlockMinSize.height = 0;
    // ok we need the max ascent of the items on the line. So to do this
    // ask the block for its line iterator. Get the max ascent.
    nsAutoLineIterator lines = GetLineIterator();
    if (lines) {
      metrics->mBlockMinSize.height = 0;
      int count = 0;
      nsIFrame* firstFrame = nullptr;
      int32_t framesOnLine;
      nsRect lineBounds;

      do {
        lines->GetLine(count, &firstFrame, &framesOnLine, lineBounds);

        if (lineBounds.height > metrics->mBlockMinSize.height)
          metrics->mBlockMinSize.height = lineBounds.height;

        count++;
      } while (firstFrame);
    } else {
      metrics->mBlockMinSize.height = desiredSize.Height();
    }

    metrics->mBlockPrefSize.height = metrics->mBlockMinSize.height;

    if (desiredSize.BlockStartAscent() == ReflowOutput::ASK_FOR_BASELINE) {
      if (!nsLayoutUtils::GetFirstLineBaseline(wm, this,
                                               &metrics->mBlockAscent))
        metrics->mBlockAscent = GetLogicalBaseline(wm);
    } else {
      metrics->mBlockAscent = desiredSize.BlockStartAscent();
    }

#ifdef DEBUG_adaptor
    printf("min=(%d,%d), pref=(%d,%d), ascent=%d\n",
           metrics->mBlockMinSize.width, metrics->mBlockMinSize.height,
           metrics->mBlockPrefSize.width, metrics->mBlockPrefSize.height,
           metrics->mBlockAscent);
#endif
  }

  return NS_OK;
}

/* virtual */
nsILineIterator* nsFrame::GetLineIterator() { return nullptr; }

nsSize nsFrame::GetXULPrefSize(nsBoxLayoutState& aState) {
  nsSize size(0, 0);
  DISPLAY_PREF_SIZE(this, size);
  // If the size is cached, and there are no HTML constraints that we might
  // be depending on, then we just return the cached size.
  nsBoxLayoutMetrics* metrics = BoxMetrics();
  if (!DoesNeedRecalc(metrics->mPrefSize)) {
    size = metrics->mPrefSize;
    return size;
  }

  if (IsXULCollapsed()) return size;

  // get our size in CSS.
  bool widthSet, heightSet;
  bool completelyRedefined =
      nsIFrame::AddXULPrefSize(this, size, widthSet, heightSet);

  // Refresh our caches with new sizes.
  if (!completelyRedefined) {
    RefreshSizeCache(aState);
    nsSize blockSize = metrics->mBlockPrefSize;

    // notice we don't need to add our borders or padding
    // in. That's because the block did it for us.
    if (!widthSet) size.width = blockSize.width;
    if (!heightSet) size.height = blockSize.height;
  }

  metrics->mPrefSize = size;
  return size;
}

nsSize nsFrame::GetXULMinSize(nsBoxLayoutState& aState) {
  nsSize size(0, 0);
  DISPLAY_MIN_SIZE(this, size);
  // Don't use the cache if we have HTMLReflowInput constraints --- they might
  // have changed
  nsBoxLayoutMetrics* metrics = BoxMetrics();
  if (!DoesNeedRecalc(metrics->mMinSize)) {
    size = metrics->mMinSize;
    return size;
  }

  if (IsXULCollapsed()) return size;

  // get our size in CSS.
  bool widthSet, heightSet;
  bool completelyRedefined =
      nsIFrame::AddXULMinSize(this, size, widthSet, heightSet);

  // Refresh our caches with new sizes.
  if (!completelyRedefined) {
    RefreshSizeCache(aState);
    nsSize blockSize = metrics->mBlockMinSize;

    if (!widthSet) size.width = blockSize.width;
    if (!heightSet) size.height = blockSize.height;
  }

  metrics->mMinSize = size;
  return size;
}

nsSize nsFrame::GetXULMaxSize(nsBoxLayoutState& aState) {
  nsSize size(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  DISPLAY_MAX_SIZE(this, size);
  // Don't use the cache if we have HTMLReflowInput constraints --- they might
  // have changed
  nsBoxLayoutMetrics* metrics = BoxMetrics();
  if (!DoesNeedRecalc(metrics->mMaxSize)) {
    size = metrics->mMaxSize;
    return size;
  }

  if (IsXULCollapsed()) return size;

  size = nsBox::GetXULMaxSize(aState);
  metrics->mMaxSize = size;

  return size;
}

nscoord nsFrame::GetXULFlex() {
  nsBoxLayoutMetrics* metrics = BoxMetrics();
  if (!DoesNeedRecalc(metrics->mFlex)) return metrics->mFlex;

  metrics->mFlex = nsBox::GetXULFlex();

  return metrics->mFlex;
}

nscoord nsFrame::GetXULBoxAscent(nsBoxLayoutState& aState) {
  nsBoxLayoutMetrics* metrics = BoxMetrics();
  if (!DoesNeedRecalc(metrics->mAscent)) return metrics->mAscent;

  if (IsXULCollapsed()) {
    metrics->mAscent = 0;
  } else {
    // Refresh our caches with new sizes.
    RefreshSizeCache(aState);
    metrics->mAscent = metrics->mBlockAscent;
  }

  return metrics->mAscent;
}

nsresult nsFrame::DoXULLayout(nsBoxLayoutState& aState) {
  nsRect ourRect(mRect);

  gfxContext* rendContext = aState.GetRenderingContext();
  nsPresContext* presContext = aState.PresContext();
  WritingMode ourWM = GetWritingMode();
  const WritingMode outerWM = aState.OuterReflowInput()
                                  ? aState.OuterReflowInput()->GetWritingMode()
                                  : ourWM;
  ReflowOutput desiredSize(outerWM);
  LogicalSize ourSize = GetLogicalSize(outerWM);

  if (rendContext) {
    BoxReflow(aState, presContext, desiredSize, rendContext, ourRect.x,
              ourRect.y, ourRect.width, ourRect.height);

    if (IsXULCollapsed()) {
      SetSize(nsSize(0, 0));
    } else {
      // if our child needs to be bigger. This might happend with
      // wrapping text. There is no way to predict its height until we
      // reflow it. Now that we know the height reshuffle upward.
      if (desiredSize.ISize(outerWM) > ourSize.ISize(outerWM) ||
          desiredSize.BSize(outerWM) > ourSize.BSize(outerWM)) {
#ifdef DEBUG_GROW
        XULDumpBox(stdout);
        printf(" GREW from (%d,%d) -> (%d,%d)\n", ourSize.ISize(outerWM),
               ourSize.BSize(outerWM), desiredSize.ISize(outerWM),
               desiredSize.BSize(outerWM));
#endif

        if (desiredSize.ISize(outerWM) > ourSize.ISize(outerWM)) {
          ourSize.ISize(outerWM) = desiredSize.ISize(outerWM);
        }

        if (desiredSize.BSize(outerWM) > ourSize.BSize(outerWM)) {
          ourSize.BSize(outerWM) = desiredSize.BSize(outerWM);
        }
      }

      // ensure our size is what we think is should be. Someone could have
      // reset the frame to be smaller or something dumb like that.
      SetSize(ourSize.ConvertTo(ourWM, outerWM));
    }
  }

  // Should we do this if IsXULCollapsed() is true?
  LogicalSize size(GetLogicalSize(outerWM));
  desiredSize.ISize(outerWM) = size.ISize(outerWM);
  desiredSize.BSize(outerWM) = size.BSize(outerWM);
  desiredSize.UnionOverflowAreasWithDesiredBounds();

  if (HasAbsolutelyPositionedChildren()) {
    // Set up a |reflowInput| to pass into ReflowAbsoluteFrames
    ReflowInput reflowInput(aState.PresContext(), this,
                            aState.GetRenderingContext(),
                            LogicalSize(ourWM, ISize(), NS_UNCONSTRAINEDSIZE),
                            ReflowInput::DUMMY_PARENT_REFLOW_INPUT);

    AddStateBits(NS_FRAME_IN_REFLOW);
    // Set up a |reflowStatus| to pass into ReflowAbsoluteFrames
    // (just a dummy value; hopefully that's OK)
    nsReflowStatus reflowStatus;
    ReflowAbsoluteFrames(aState.PresContext(), desiredSize, reflowInput,
                         reflowStatus);
    RemoveStateBits(NS_FRAME_IN_REFLOW);
  }

  nsSize oldSize(ourRect.Size());
  FinishAndStoreOverflow(desiredSize.mOverflowAreas,
                         size.GetPhysicalSize(outerWM), &oldSize);

  SyncLayout(aState);

  return NS_OK;
}

void nsFrame::BoxReflow(nsBoxLayoutState& aState, nsPresContext* aPresContext,
                        ReflowOutput& aDesiredSize,
                        gfxContext* aRenderingContext, nscoord aX, nscoord aY,
                        nscoord aWidth, nscoord aHeight, bool aMoveFrame) {
  DO_GLOBAL_REFLOW_COUNT("nsBoxToBlockAdaptor");

#ifdef DEBUG_REFLOW
  nsAdaptorAddIndents();
  printf("Reflowing: ");
  mFrame->ListTag(stdout);
  printf("\n");
  gIndent2++;
#endif

  nsBoxLayoutMetrics* metrics = BoxMetrics();
  if (MOZ_UNLIKELY(!metrics)) {
    // Can't proceed without BoxMetrics. This should only happen if something
    // is seriously broken, e.g. if we try to do XUL layout on a non-XUL frame.
    // (If this is a content process, we'll abort even in release builds,
    // because XUL layout mixup is extra surprising in content, and aborts are
    // less catastrophic in content vs. in chrome.)
    MOZ_RELEASE_ASSERT(!XRE_IsContentProcess(),
                       "Starting XUL BoxReflow w/o BoxMetrics (in content)?");
    MOZ_ASSERT_UNREACHABLE("Starting XUL BoxReflow w/o BoxMetrics?");
    return;
  }

  nsReflowStatus status;
  WritingMode wm = aDesiredSize.GetWritingMode();

  bool needsReflow = NS_SUBTREE_DIRTY(this);

  // if we don't need a reflow then
  // lets see if we are already that size. Yes? then don't even reflow. We are
  // done.
  if (!needsReflow) {
    if (aWidth != NS_UNCONSTRAINEDSIZE && aHeight != NS_UNCONSTRAINEDSIZE) {
      // if the new calculated size has a 0 width or a 0 height
      if ((metrics->mLastSize.width == 0 || metrics->mLastSize.height == 0) &&
          (aWidth == 0 || aHeight == 0)) {
        needsReflow = false;
        aDesiredSize.Width() = aWidth;
        aDesiredSize.Height() = aHeight;
        SetSize(aDesiredSize.Size(wm).ConvertTo(GetWritingMode(), wm));
      } else {
        aDesiredSize.Width() = metrics->mLastSize.width;
        aDesiredSize.Height() = metrics->mLastSize.height;

        // remove the margin. The rect of our child does not include it but our
        // calculated size does. don't reflow if we are already the right size
        if (metrics->mLastSize.width == aWidth &&
            metrics->mLastSize.height == aHeight)
          needsReflow = false;
        else
          needsReflow = true;
      }
    } else {
      // if the width or height are intrinsic alway reflow because
      // we don't know what it should be.
      needsReflow = true;
    }
  }

  // ok now reflow the child into the spacers calculated space
  if (needsReflow) {
    aDesiredSize.ClearSize();

    // create a reflow input to tell our child to flow at the given size.

    // Construct a bogus parent reflow input so that there's a usable
    // containing block reflow input.
    nsMargin margin(0, 0, 0, 0);
    GetXULMargin(margin);

    nsSize parentSize(aWidth, aHeight);
    if (parentSize.height != NS_UNCONSTRAINEDSIZE)
      parentSize.height += margin.TopBottom();
    if (parentSize.width != NS_UNCONSTRAINEDSIZE)
      parentSize.width += margin.LeftRight();

    nsIFrame* parentFrame = GetParent();
    WritingMode parentWM = parentFrame->GetWritingMode();
    ReflowInput parentReflowInput(aPresContext, parentFrame, aRenderingContext,
                                  LogicalSize(parentWM, parentSize),
                                  ReflowInput::DUMMY_PARENT_REFLOW_INPUT);

    // This may not do very much useful, but it's probably worth trying.
    if (parentSize.width != NS_UNCONSTRAINEDSIZE)
      parentReflowInput.SetComputedWidth(std::max(parentSize.width, 0));
    if (parentSize.height != NS_UNCONSTRAINEDSIZE)
      parentReflowInput.SetComputedHeight(std::max(parentSize.height, 0));
    parentReflowInput.ComputedPhysicalMargin().SizeTo(0, 0, 0, 0);
    // XXX use box methods
    parentFrame->GetXULPadding(parentReflowInput.ComputedPhysicalPadding());
    parentFrame->GetXULBorder(
        parentReflowInput.ComputedPhysicalBorderPadding());
    parentReflowInput.ComputedPhysicalBorderPadding() +=
        parentReflowInput.ComputedPhysicalPadding();

    // Construct the parent chain manually since constructing it normally
    // messes up dimensions.
    const ReflowInput* outerReflowInput = aState.OuterReflowInput();
    NS_ASSERTION(!outerReflowInput || outerReflowInput->mFrame != this,
                 "in and out of XUL on a single frame?");
    const ReflowInput* parentRI;
    if (outerReflowInput && outerReflowInput->mFrame == parentFrame) {
      // We're a frame (such as a text control frame) that jumps into
      // box reflow and then straight out of it on the child frame.
      // This means we actually have a real parent reflow input.
      // nsLayoutUtils::InflationMinFontSizeFor used to need this to be
      // linked up correctly for text control frames, so do so here).
      parentRI = outerReflowInput;
    } else {
      parentRI = &parentReflowInput;
    }

    // XXX Is it OK that this reflow input has only one ancestor?
    // (It used to have a bogus parent, skipping all the boxes).
    WritingMode wm = GetWritingMode();
    LogicalSize logicalSize(wm, nsSize(aWidth, aHeight));
    logicalSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;
    ReflowInput reflowInput(aPresContext, *parentRI, this, logicalSize,
                            Nothing(), ReflowInput::DUMMY_PARENT_REFLOW_INPUT);

    // XXX_jwir3: This is somewhat fishy. If this is actually changing the value
    //            here (which it might be), then we should make sure that it's
    //            correct the first time around, rather than changing it later.
    reflowInput.mCBReflowInput = parentRI;

    reflowInput.mReflowDepth = aState.GetReflowDepth();

    // mComputedWidth and mComputedHeight are content-box, not
    // border-box
    if (aWidth != NS_UNCONSTRAINEDSIZE) {
      nscoord computedWidth =
          aWidth - reflowInput.ComputedPhysicalBorderPadding().LeftRight();
      computedWidth = std::max(computedWidth, 0);
      reflowInput.SetComputedWidth(computedWidth);
    }

    // Most child frames of box frames (e.g. subdocument or scroll frames)
    // need to be constrained to the provided size and overflow as necessary.
    // The one exception are block frames, because we need to know their
    // natural height excluding any overflow area which may be caused by
    // various CSS effects such as shadow or outline.
    if (!IsBlockFrameOrSubclass()) {
      if (aHeight != NS_UNCONSTRAINEDSIZE) {
        nscoord computedHeight =
            aHeight - reflowInput.ComputedPhysicalBorderPadding().TopBottom();
        computedHeight = std::max(computedHeight, 0);
        reflowInput.SetComputedHeight(computedHeight);
      } else {
        reflowInput.SetComputedHeight(
            ComputeSize(aRenderingContext, wm, logicalSize,
                        logicalSize.ISize(wm),
                        reflowInput.ComputedLogicalMargin().Size(wm),
                        reflowInput.ComputedLogicalBorderPadding().Size(wm) -
                            reflowInput.ComputedLogicalPadding().Size(wm),
                        reflowInput.ComputedLogicalPadding().Size(wm),
                        ComputeSizeFlags::eDefault)
                .Height(wm));
      }
    }

    // Box layout calls SetRect before XULLayout, whereas non-box layout
    // calls SetRect after Reflow.
    // XXX Perhaps we should be doing this by twiddling the rect back to
    // mLastSize before calling Reflow and then switching it back, but
    // However, mLastSize can also be the size passed to BoxReflow by
    // RefreshSizeCache, so that doesn't really make sense.
    if (metrics->mLastSize.width != aWidth) {
      reflowInput.SetHResize(true);

      // When font size inflation is enabled, a horizontal resize
      // requires a full reflow.  See ReflowInput::InitResizeFlags
      // for more details.
      if (nsLayoutUtils::FontSizeInflationEnabled(aPresContext)) {
        this->MarkSubtreeDirty();
      }
    }
    if (metrics->mLastSize.height != aHeight) {
      reflowInput.SetVResize(true);
    }

#ifdef DEBUG_REFLOW
    nsAdaptorAddIndents();
    printf("Size=(%d,%d)\n", reflowInput.ComputedWidth(),
           reflowInput.ComputedHeight());
    nsAdaptorAddIndents();
    nsAdaptorPrintReason(reflowInput);
    printf("\n");
#endif

    // place the child and reflow

    Reflow(aPresContext, aDesiredSize, reflowInput, status);

    NS_ASSERTION(status.IsComplete(), "bad status");

    ReflowChildFlags layoutFlags = aState.LayoutFlags();
    nsContainerFrame::FinishReflowChild(
        this, aPresContext, aDesiredSize, &reflowInput, aX, aY,
        layoutFlags | ReflowChildFlags::NoMoveFrame);

    // Save the ascent.  (bug 103925)
    if (IsXULCollapsed()) {
      metrics->mAscent = 0;
    } else {
      if (aDesiredSize.BlockStartAscent() == ReflowOutput::ASK_FOR_BASELINE) {
        if (!nsLayoutUtils::GetFirstLineBaseline(wm, this, &metrics->mAscent))
          metrics->mAscent = GetLogicalBaseline(wm);
      } else
        metrics->mAscent = aDesiredSize.BlockStartAscent();
    }

  } else {
    aDesiredSize.SetBlockStartAscent(metrics->mBlockAscent);
  }

#ifdef DEBUG_REFLOW
  if (aHeight != NS_UNCONSTRAINEDSIZE && aDesiredSize.Height() != aHeight) {
    nsAdaptorAddIndents();
    printf("*****got taller!*****\n");
  }
  if (aWidth != NS_UNCONSTRAINEDSIZE && aDesiredSize.Width() != aWidth) {
    nsAdaptorAddIndents();
    printf("*****got wider!******\n");
  }
#endif

  metrics->mLastSize.width = aDesiredSize.Width();
  metrics->mLastSize.height = aDesiredSize.Height();

#ifdef DEBUG_REFLOW
  gIndent2--;
#endif
}

nsBoxLayoutMetrics* nsFrame::BoxMetrics() const {
  nsBoxLayoutMetrics* metrics = GetProperty(BoxMetricsProperty());
  NS_ASSERTION(
      metrics,
      "A box layout method was called but InitBoxMetrics was never called");
  return metrics;
}

void nsIFrame::UpdateStyleOfChildAnonBox(nsIFrame* aChildFrame,
                                         ServoRestyleState& aRestyleState) {
#ifdef DEBUG
  nsIFrame* parent = aChildFrame->GetInFlowParent();
  if (aChildFrame->IsTableFrame()) {
    parent = parent->GetParent();
  }
  if (parent->IsLineFrame()) {
    parent = parent->GetParent();
  }
  MOZ_ASSERT(nsLayoutUtils::FirstContinuationOrIBSplitSibling(parent) == this,
             "This should only be used for children!");
#endif  // DEBUG
  MOZ_ASSERT(!GetContent() || !aChildFrame->GetContent() ||
                 aChildFrame->GetContent() == GetContent(),
             "What content node is it a frame for?");
  MOZ_ASSERT(!aChildFrame->GetPrevContinuation(),
             "Only first continuations should end up here");

  // We could force the caller to pass in the pseudo, since some callers know it
  // statically...  But this API is a bit nicer.
  auto pseudo = aChildFrame->Style()->GetPseudoType();
  MOZ_ASSERT(PseudoStyle::IsAnonBox(pseudo), "Child is not an anon box?");
  MOZ_ASSERT(!PseudoStyle::IsNonInheritingAnonBox(pseudo),
             "Why did the caller bother calling us?");

  // Anon boxes inherit from their parent; that's us.
  RefPtr<ComputedStyle> newContext =
      aRestyleState.StyleSet().ResolveInheritingAnonymousBoxStyle(pseudo,
                                                                  Style());

  nsChangeHint childHint =
      UpdateStyleOfOwnedChildFrame(aChildFrame, newContext, aRestyleState);

  // Now that we've updated the style on aChildFrame, check whether it itself
  // has anon boxes to deal with.
  ServoRestyleState childrenState(*aChildFrame, aRestyleState, childHint,
                                  ServoRestyleState::Type::InFlow);
  aChildFrame->UpdateStyleOfOwnedAnonBoxes(childrenState);

  // Assuming anon boxes don't have ::backdrop associated with them... if that
  // ever changes, we'd need to handle that here, like we do in
  // RestyleManager::ProcessPostTraversal

  // We do need to handle block pseudo-elements here, though.  Especially list
  // bullets.
  if (nsBlockFrame* block = do_QueryFrame(aChildFrame)) {
    block->UpdatePseudoElementStyles(childrenState);
  }
}

/* static */
nsChangeHint nsIFrame::UpdateStyleOfOwnedChildFrame(
    nsIFrame* aChildFrame, ComputedStyle* aNewComputedStyle,
    ServoRestyleState& aRestyleState,
    const Maybe<ComputedStyle*>& aContinuationComputedStyle) {
  MOZ_ASSERT(!aChildFrame->GetAdditionalComputedStyle(0),
             "We don't handle additional styles here");

  // Figure out whether we have an actual change.  It's important that we do
  // this, for several reasons:
  //
  // 1) Even if all the child's changes are due to properties it inherits from
  //    us, it's possible that no one ever asked us for those style structs and
  //    hence changes to them aren't reflected in the changes handled at all.
  //
  // 2) Content can change stylesheets that change the styles of pseudos, and
  //    extensions can add/remove stylesheets that change the styles of
  //    anonymous boxes directly.
  uint32_t equalStructs;  // Not used, actually.
  nsChangeHint childHint = aChildFrame->Style()->CalcStyleDifference(
      *aNewComputedStyle, &equalStructs);

  // If aChildFrame is out of flow, then aRestyleState's "changes handled by the
  // parent" doesn't apply to it, because it may have some other parent in the
  // frame tree.
  if (!aChildFrame->HasAnyStateBits(NS_FRAME_OUT_OF_FLOW)) {
    childHint = NS_RemoveSubsumedHints(
        childHint, aRestyleState.ChangesHandledFor(aChildFrame));
  }
  if (childHint) {
    if (childHint & nsChangeHint_ReconstructFrame) {
      // If we generate a reconstruct here, remove any non-reconstruct hints we
      // may have already generated for this content.
      aRestyleState.ChangeList().PopChangesForContent(
          aChildFrame->GetContent());
    }
    aRestyleState.ChangeList().AppendChange(
        aChildFrame, aChildFrame->GetContent(), childHint);
  }

  aChildFrame->SetComputedStyle(aNewComputedStyle);
  ComputedStyle* continuationStyle = aContinuationComputedStyle
                                         ? *aContinuationComputedStyle
                                         : aNewComputedStyle;
  for (nsIFrame* kid = aChildFrame->GetNextContinuation(); kid;
       kid = kid->GetNextContinuation()) {
    MOZ_ASSERT(!kid->GetAdditionalComputedStyle(0));
    kid->SetComputedStyle(continuationStyle);
  }

  return childHint;
}

/* static */
void nsIFrame::AddInPopupStateBitToDescendants(nsIFrame* aFrame) {
  if (!aFrame->HasAnyStateBits(NS_FRAME_IN_POPUP) &&
      aFrame->TrackingVisibility()) {
    // Assume all frames in popups are visible.
    aFrame->IncApproximateVisibleCount();
  }

  aFrame->AddStateBits(NS_FRAME_IN_POPUP);

  AutoTArray<nsIFrame::ChildList, 4> childListArray;
  aFrame->GetCrossDocChildLists(&childListArray);

  nsIFrame::ChildListArrayIterator lists(childListArray);
  for (; !lists.IsDone(); lists.Next()) {
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      AddInPopupStateBitToDescendants(childFrames.get());
    }
  }
}

/* static */
void nsIFrame::RemoveInPopupStateBitFromDescendants(nsIFrame* aFrame) {
  if (!aFrame->HasAnyStateBits(NS_FRAME_IN_POPUP) ||
      nsLayoutUtils::IsPopup(aFrame)) {
    return;
  }

  aFrame->RemoveStateBits(NS_FRAME_IN_POPUP);

  if (aFrame->TrackingVisibility()) {
    // We assume all frames in popups are visible, so this decrement balances
    // out the increment in AddInPopupStateBitToDescendants above.
    aFrame->DecApproximateVisibleCount();
  }

  AutoTArray<nsIFrame::ChildList, 4> childListArray;
  aFrame->GetCrossDocChildLists(&childListArray);

  nsIFrame::ChildListArrayIterator lists(childListArray);
  for (; !lists.IsDone(); lists.Next()) {
    nsFrameList::Enumerator childFrames(lists.CurrentList());
    for (; !childFrames.AtEnd(); childFrames.Next()) {
      RemoveInPopupStateBitFromDescendants(childFrames.get());
    }
  }
}

void nsIFrame::SetParent(nsContainerFrame* aParent) {
  // If our parent is a wrapper anon box, our new parent should be too.  We
  // _can_ change parent if our parent is a wrapper anon box, because some
  // wrapper anon boxes can have continuations.
  MOZ_ASSERT_IF(ParentIsWrapperAnonBox(),
                aParent->Style()->IsInheritingAnonBox());

  // Note that the current mParent may already be destroyed at this point.
  mParent = aParent;
  MOZ_DIAGNOSTIC_ASSERT(!mParent || PresShell() == mParent->PresShell());
  if (::IsXULBoxWrapped(this)) {
    ::InitBoxMetrics(this, true);
  } else {
    // We could call Properties().Delete(BoxMetricsProperty()); here but
    // that's kind of slow and re-parenting in such a way that we were
    // IsXULBoxWrapped() before but not now should be very rare, so we'll just
    // keep this unused frame property until this frame dies instead.
  }

  if (GetStateBits() & (NS_FRAME_HAS_VIEW | NS_FRAME_HAS_CHILD_WITH_VIEW)) {
    for (nsIFrame* f = aParent;
         f && !(f->GetStateBits() & NS_FRAME_HAS_CHILD_WITH_VIEW);
         f = f->GetParent()) {
      f->AddStateBits(NS_FRAME_HAS_CHILD_WITH_VIEW);
    }
  }

  if (HasAnyStateBits(NS_FRAME_CONTAINS_RELATIVE_BSIZE)) {
    for (nsIFrame* f = aParent; f; f = f->GetParent()) {
      if (f->HasAnyStateBits(NS_FRAME_CONTAINS_RELATIVE_BSIZE)) {
        break;
      }
      f->AddStateBits(NS_FRAME_CONTAINS_RELATIVE_BSIZE);
    }
  }

  if (HasAnyStateBits(NS_FRAME_DESCENDANT_INTRINSIC_ISIZE_DEPENDS_ON_BSIZE)) {
    for (nsIFrame* f = aParent; f; f = f->GetParent()) {
      if (f->HasAnyStateBits(
              NS_FRAME_DESCENDANT_INTRINSIC_ISIZE_DEPENDS_ON_BSIZE)) {
        break;
      }
      f->AddStateBits(NS_FRAME_DESCENDANT_INTRINSIC_ISIZE_DEPENDS_ON_BSIZE);
    }
  }

  if (HasInvalidFrameInSubtree()) {
    for (nsIFrame* f = aParent;
         f && !f->HasAnyStateBits(NS_FRAME_DESCENDANT_NEEDS_PAINT |
                                  NS_FRAME_IS_NONDISPLAY);
         f = nsLayoutUtils::GetCrossDocParentFrame(f)) {
      f->AddStateBits(NS_FRAME_DESCENDANT_NEEDS_PAINT);
    }
  }

  if (aParent->HasAnyStateBits(NS_FRAME_IN_POPUP)) {
    AddInPopupStateBitToDescendants(this);
  } else {
    RemoveInPopupStateBitFromDescendants(this);
  }

  // If our new parent only has invalid children, then we just invalidate
  // ourselves too. This is probably faster than clearing the flag all
  // the way up the frame tree.
  if (aParent->HasAnyStateBits(NS_FRAME_ALL_DESCENDANTS_NEED_PAINT)) {
    InvalidateFrame();
  } else {
    SchedulePaint();
  }
}

void nsIFrame::CreateOwnLayerIfNeeded(nsDisplayListBuilder* aBuilder,
                                      nsDisplayList* aList, uint16_t aType,
                                      bool* aCreatedContainerItem) {
  if (GetContent() && GetContent()->IsXULElement() &&
      GetContent()->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::layer)) {
    aList->AppendNewToTop<nsDisplayOwnLayer>(
        aBuilder, this, aList, aBuilder->CurrentActiveScrolledRoot(),
        nsDisplayOwnLayerFlags::None, ScrollbarData{}, true, false, aType);
    if (aCreatedContainerItem) {
      *aCreatedContainerItem = true;
    }
  }
}

bool nsIFrame::IsStackingContext(const nsStyleDisplay* aStyleDisplay,
                                 const nsStylePosition* aStylePosition,
                                 const nsStyleEffects* aStyleEffects,
                                 bool aIsPositioned) {
  return HasOpacity(aStyleDisplay, aStyleEffects, nullptr) ||
         IsTransformed(aStyleDisplay) ||
         ((aStyleDisplay->IsContainPaint() ||
           aStyleDisplay->IsContainLayout()) &&
          IsFrameOfType(eSupportsContainLayoutAndPaint)) ||
         // strictly speaking, 'perspective' doesn't require visual atomicity,
         // but the spec says it acts like the rest of these
         ChildrenHavePerspective(aStyleDisplay) ||
         aStyleEffects->mMixBlendMode != StyleBlend::Normal ||
         nsSVGIntegrationUtils::UsingEffectsForFrame(this) ||
         (aIsPositioned && (aStyleDisplay->IsPositionForcingStackingContext() ||
                            aStylePosition->mZIndex.IsInteger())) ||
         (aStyleDisplay->mWillChange.bits &
          StyleWillChangeBits::STACKING_CONTEXT) ||
         aStyleDisplay->mIsolation != StyleIsolation::Auto ||
         aStyleEffects->HasBackdropFilters();
}

bool nsIFrame::IsStackingContext() {
  const nsStyleDisplay* disp = StyleDisplay();
  const bool isPositioned = disp->IsAbsPosContainingBlock(this);
  return IsStackingContext(disp, StylePosition(), StyleEffects(), isPositioned);
}

static bool IsFrameScrolledOutOfView(const nsIFrame* aTarget,
                                     const nsRect& aTargetRect,
                                     const nsIFrame* aParent) {
  nsIScrollableFrame* scrollableFrame =
      nsLayoutUtils::GetNearestScrollableFrame(
          const_cast<nsIFrame*>(aParent),
          nsLayoutUtils::SCROLLABLE_FIXEDPOS_FINDS_ROOT |
              nsLayoutUtils::SCROLLABLE_INCLUDE_HIDDEN);
  if (!scrollableFrame) {
    // Even if we couldn't find the nearest scrollable frame, it might mean we
    // are in an out-of-process iframe, try to see if |aTarget| frame is
    // scrolled out of view in an scrollable frame in a cross-process ancestor
    // document.
    return nsLayoutUtils::FrameIsScrolledOutOfViewInCrossProcess(aTarget);
  }

  nsIFrame* scrollableParent = do_QueryFrame(scrollableFrame);
  nsRect scrollableRect =
      scrollableParent->GetVisualOverflowRectRelativeToSelf();
  // We consider that the target is scrolled out if the scrollable frame is
  // empty.
  if (scrollableRect.IsEmpty()) {
    return true;
  }

  nsRect transformedRect = nsLayoutUtils::TransformFrameRectToAncestor(
      aTarget, aTargetRect, scrollableParent);

  if (transformedRect.IsEmpty()) {
    // If the transformed rect is empty it represents a line or a point that we
    // should check is outside the the scrollable rect.
    if (transformedRect.x > scrollableRect.XMost() ||
        transformedRect.y > scrollableRect.YMost() ||
        scrollableRect.x > transformedRect.XMost() ||
        scrollableRect.y > transformedRect.YMost()) {
      return true;
    }
  } else if (!transformedRect.Intersects(scrollableRect)) {
    return true;
  }

  nsIFrame* parent = scrollableParent->GetParent();
  if (!parent) {
    return false;
  }

  return IsFrameScrolledOutOfView(aTarget, aTargetRect, parent);
}

bool nsIFrame::IsScrolledOutOfView() const {
  nsRect rect = GetVisualOverflowRectRelativeToSelf();
  return IsFrameScrolledOutOfView(this, rect, this);
}

gfx::Matrix nsIFrame::ComputeWidgetTransform() {
  const nsStyleUIReset* uiReset = StyleUIReset();
  if (uiReset->mMozWindowTransform.IsNone()) {
    return gfx::Matrix();
  }

  TransformReferenceBox refBox(nullptr, nsRect(nsPoint(), GetSize()));

  nsPresContext* presContext = PresContext();
  int32_t appUnitsPerDevPixel = presContext->AppUnitsPerDevPixel();
  gfx::Matrix4x4 matrix = nsStyleTransformMatrix::ReadTransforms(
      uiReset->mMozWindowTransform, refBox, float(appUnitsPerDevPixel));

  // Apply the -moz-window-transform-origin translation to the matrix.
  const StyleTransformOrigin& origin = uiReset->mWindowTransformOrigin;
  Point transformOrigin = nsStyleTransformMatrix::Convert2DPosition(
      origin.horizontal, origin.vertical, refBox, appUnitsPerDevPixel);
  matrix.ChangeBasis(Point3D(transformOrigin.x, transformOrigin.y, 0));

  gfx::Matrix result2d;
  if (!matrix.CanDraw2D(&result2d)) {
    // FIXME: It would be preferable to reject non-2D transforms at parse time.
    NS_WARNING(
        "-moz-window-transform does not describe a 2D transform, "
        "but only 2d transforms are supported");
    return gfx::Matrix();
  }

  return result2d;
}

void nsIFrame::DoUpdateStyleOfOwnedAnonBoxes(ServoRestyleState& aRestyleState) {
  // As a special case, we check for {ib}-split block frames here, rather
  // than have an nsInlineFrame::AppendDirectlyOwnedAnonBoxes implementation
  // that returns them.
  //
  // (If we did handle them in AppendDirectlyOwnedAnonBoxes, we would have to
  // return *all* of the in-flow {ib}-split block frames, not just the first
  // one.  For restyling, we really just need the first in flow, and the other
  // user of the AppendOwnedAnonBoxes API, AllChildIterator, doesn't need to
  // know about them at all, since these block frames never create NAC.  So we
  // avoid any unncessary hashtable lookups for the {ib}-split frames by calling
  // UpdateStyleOfOwnedAnonBoxesForIBSplit directly here.)
  if (IsInlineFrame()) {
    if ((GetStateBits() & NS_FRAME_PART_OF_IBSPLIT)) {
      static_cast<nsInlineFrame*>(this)->UpdateStyleOfOwnedAnonBoxesForIBSplit(
          aRestyleState);
    }
    return;
  }

  AutoTArray<OwnedAnonBox, 4> frames;
  AppendDirectlyOwnedAnonBoxes(frames);
  for (OwnedAnonBox& box : frames) {
    if (box.mUpdateStyleFn) {
      box.mUpdateStyleFn(this, box.mAnonBoxFrame, aRestyleState);
    } else {
      UpdateStyleOfChildAnonBox(box.mAnonBoxFrame, aRestyleState);
    }
  }
}

/* virtual */
void nsIFrame::AppendDirectlyOwnedAnonBoxes(nsTArray<OwnedAnonBox>& aResult) {
  MOZ_ASSERT(!(GetStateBits() & NS_FRAME_OWNS_ANON_BOXES));
  MOZ_ASSERT(false, "Why did this get called?");
}

void nsIFrame::DoAppendOwnedAnonBoxes(nsTArray<OwnedAnonBox>& aResult) {
  size_t i = aResult.Length();
  AppendDirectlyOwnedAnonBoxes(aResult);

  // After appending the directly owned anonymous boxes of this frame to
  // aResult above, we need to check each of them to see if they own
  // any anonymous boxes themselves.  Note that we keep progressing
  // through aResult, looking for additional entries in aResult from these
  // subsequent AppendDirectlyOwnedAnonBoxes calls.  (Thus we can't
  // use a ranged for loop here.)

  while (i < aResult.Length()) {
    nsIFrame* f = aResult[i].mAnonBoxFrame;
    if (f->GetStateBits() & NS_FRAME_OWNS_ANON_BOXES) {
      f->AppendDirectlyOwnedAnonBoxes(aResult);
    }
    ++i;
  }
}

nsIFrame::CaretPosition::CaretPosition() : mContentOffset(0) {}

nsIFrame::CaretPosition::~CaretPosition() = default;

bool nsFrame::HasCSSAnimations() {
  auto collection =
      AnimationCollection<CSSAnimation>::GetAnimationCollection(this);
  return collection && collection->mAnimations.Length() > 0;
}

bool nsFrame::HasCSSTransitions() {
  auto collection =
      AnimationCollection<CSSTransition>::GetAnimationCollection(this);
  return collection && collection->mAnimations.Length() > 0;
}

void nsIFrame::AddSizeOfExcludingThisForTree(nsWindowSizes& aSizes) const {
  aSizes.mLayoutFramePropertiesSize +=
      mProperties.SizeOfExcludingThis(aSizes.mState.mMallocSizeOf);

  // We don't do this for Gecko because this stuff is stored in the nsPresArena
  // and so measured elsewhere.
  if (!aSizes.mState.HaveSeenPtr(mComputedStyle)) {
    mComputedStyle->AddSizeOfIncludingThis(aSizes,
                                           &aSizes.mLayoutComputedValuesNonDom);
  }

  // And our additional styles.
  int32_t index = 0;
  while (auto* extra = GetAdditionalComputedStyle(index++)) {
    if (!aSizes.mState.HaveSeenPtr(extra)) {
      extra->AddSizeOfIncludingThis(aSizes,
                                    &aSizes.mLayoutComputedValuesNonDom);
    }
  }

  FrameChildListIterator iter(this);
  while (!iter.IsDone()) {
    for (const nsIFrame* f : iter.CurrentList()) {
      f->AddSizeOfExcludingThisForTree(aSizes);
    }
    iter.Next();
  }
}

nsRect nsIFrame::GetCompositorHitTestArea(nsDisplayListBuilder* aBuilder) {
  nsRect area;

  nsIScrollableFrame* scrollFrame = nsLayoutUtils::GetScrollableFrameFor(this);
  if (scrollFrame) {
    // If the frame is content of a scrollframe, then we need to pick up the
    // area corresponding to the overflow rect as well. Otherwise the parts of
    // the overflow that are not occupied by descendants get skipped and the
    // APZ code sends touch events to the content underneath instead.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=1127773#c15.
    area = GetScrollableOverflowRect();
  } else {
    area = nsRect(nsPoint(0, 0), GetSize());
  }

  if (!area.IsEmpty()) {
    return area + aBuilder->ToReferenceFrame(this);
  }

  return area;
}

CompositorHitTestInfo nsIFrame::GetCompositorHitTestInfo(
    nsDisplayListBuilder* aBuilder) {
  CompositorHitTestInfo result = CompositorHitTestInvisibleToHit;

  if (aBuilder->IsInsidePointerEventsNoneDoc()) {
    // Somewhere up the parent document chain is a subdocument with pointer-
    // events:none set on it.
    return result;
  }
  if (!GetParent()) {
    MOZ_ASSERT(IsViewportFrame());
    // Viewport frames are never event targets, other frames, like canvas
    // frames, are the event targets for any regions viewport frames may cover.
    return result;
  }
  const StylePointerEvents pointerEvents =
      StyleUI()->GetEffectivePointerEvents(this);
  if (pointerEvents == StylePointerEvents::None) {
    return result;
  }
  if (!StyleVisibility()->IsVisible()) {
    return result;
  }

  // Anything that didn't match the above conditions is visible to hit-testing.
  result = CompositorHitTestFlags::eVisibleToHitTest;
  if (nsSVGIntegrationUtils::UsingMaskOrClipPathForFrame(this)) {
    // If WebRender is enabled, simple clip-paths can be converted into WR
    // clips that WR knows how to hit-test against, so we don't need to mark
    // it as an irregular area.
    if (!gfxVars::UseWebRender() ||
        !nsSVGIntegrationUtils::UsingSimpleClipPathForFrame(this)) {
      result += CompositorHitTestFlags::eIrregularArea;
    }
  }

  if (aBuilder->IsBuildingNonLayerizedScrollbar()) {
    // Scrollbars may be painted into a layer below the actual layer they will
    // scroll, and therefore wheel events may be dispatched to the outer frame
    // instead of the intended scrollframe. To address this, we force a d-t-c
    // region on scrollbar frames that won't be placed in their own layer. See
    // bug 1213324 for details.
    result += CompositorHitTestFlags::eInactiveScrollframe;
  } else if (aBuilder->GetAncestorHasApzAwareEventHandler()) {
    result += CompositorHitTestFlags::eApzAwareListeners;
  } else if (IsObjectFrame()) {
    // If the frame is a plugin frame and wants to handle wheel events as
    // default action, we should add the frame to dispatch-to-content region.
    nsPluginFrame* pluginFrame = do_QueryFrame(this);
    if (pluginFrame && pluginFrame->WantsToHandleWheelEventAsDefaultAction()) {
      result += CompositorHitTestFlags::eApzAwareListeners;
    }
  }

  if (aBuilder->IsTouchEventPrefEnabledDoc()) {
    // Inherit the touch-action flags from the parent, if there is one. We do
    // this because of how the touch-action on a frame combines the touch-action
    // from ancestor DOM elements. Refer to the documentation in
    // TouchActionHelper.cpp for details; this code is meant to be equivalent to
    // that code, but woven into the top-down recursive display list building
    // process.
    CompositorHitTestInfo inheritedTouchAction =
        aBuilder->GetHitTestInfo() & CompositorHitTestTouchActionMask;

    nsIFrame* touchActionFrame = this;
    if (nsIScrollableFrame* scrollFrame =
            nsLayoutUtils::GetScrollableFrameFor(this)) {
      touchActionFrame = do_QueryFrame(scrollFrame);
      // On scrollframes, stop inheriting the pan-x and pan-y flags; instead,
      // reset them back to zero to allow panning on the scrollframe unless we
      // encounter an element that disables it that's inside the scrollframe.
      // This is equivalent to the |considerPanning| variable in
      // TouchActionHelper.cpp, but for a top-down traversal.
      CompositorHitTestInfo panMask(
          CompositorHitTestFlags::eTouchActionPanXDisabled,
          CompositorHitTestFlags::eTouchActionPanYDisabled);
      inheritedTouchAction -= panMask;
    }

    result += inheritedTouchAction;

    const StyleTouchAction touchAction =
        nsLayoutUtils::GetTouchActionFromFrame(touchActionFrame);
    // The CSS allows the syntax auto | none | [pan-x || pan-y] | manipulation
    // so we can eliminate some combinations of things.
    if (touchAction == StyleTouchAction::AUTO) {
      // nothing to do
    } else if (touchAction & StyleTouchAction::MANIPULATION) {
      result += CompositorHitTestFlags::eTouchActionDoubleTapZoomDisabled;
    } else {
      // This path handles the cases none | [pan-x || pan-y] and so both
      // double-tap and pinch zoom are disabled in here.
      result += CompositorHitTestFlags::eTouchActionPinchZoomDisabled;
      result += CompositorHitTestFlags::eTouchActionDoubleTapZoomDisabled;

      if (!(touchAction & StyleTouchAction::PAN_X)) {
        result += CompositorHitTestFlags::eTouchActionPanXDisabled;
      }
      if (!(touchAction & StyleTouchAction::PAN_Y)) {
        result += CompositorHitTestFlags::eTouchActionPanYDisabled;
      }
      if (touchAction & StyleTouchAction::NONE) {
        // all the touch-action disabling flags will already have been set above
        MOZ_ASSERT(result.contains(CompositorHitTestTouchActionMask));
      }
    }
  }

  const Maybe<ScrollDirection> scrollDirection =
      aBuilder->GetCurrentScrollbarDirection();
  if (scrollDirection.isSome()) {
    if (GetContent()->IsXULElement(nsGkAtoms::thumb)) {
      const bool thumbGetsLayer = aBuilder->GetCurrentScrollbarTarget() !=
                                  layers::ScrollableLayerGuid::NULL_SCROLL_ID;
      if (thumbGetsLayer) {
        result += CompositorHitTestFlags::eScrollbarThumb;
      } else {
        result += CompositorHitTestFlags::eInactiveScrollframe;
      }
    }

    if (*scrollDirection == ScrollDirection::eVertical) {
      result += CompositorHitTestFlags::eScrollbarVertical;
    }

    // includes the ScrollbarFrame, SliderFrame, anything else that
    // might be inside the xul:scrollbar
    result += CompositorHitTestFlags::eScrollbar;
  }

  return result;
}

// Returns true if we can guarantee there is no visible descendants.
static bool HasNoVisibleDescendants(const nsIFrame* aFrame) {
  for (nsIFrame::ChildListIterator lists(aFrame); !lists.IsDone();
       lists.Next()) {
    for (nsIFrame* f : lists.CurrentList()) {
      if (nsPlaceholderFrame::GetRealFrameFor(f)
              ->IsVisibleOrMayHaveVisibleDescendants()) {
        return false;
      }
    }
  }
  return true;
}

void nsIFrame::UpdateVisibleDescendantsState() {
  if (StyleVisibility()->IsVisible()) {
    // Notify invisible ancestors that a visible descendant exists now.
    nsIFrame* ancestor;
    for (ancestor = GetInFlowParent();
         ancestor && !ancestor->StyleVisibility()->IsVisible();
         ancestor = ancestor->GetInFlowParent()) {
      ancestor->mAllDescendantsAreInvisible = false;
    }
  } else {
    mAllDescendantsAreInvisible = HasNoVisibleDescendants(this);
  }
}

// Box layout debugging
#ifdef DEBUG_REFLOW
int32_t gIndent2 = 0;

void nsAdaptorAddIndents() {
  for (int32_t i = 0; i < gIndent2; i++) {
    printf(" ");
  }
}

void nsAdaptorPrintReason(ReflowInput& aReflowInput) {
  char* reflowReasonString;

  switch (aReflowInput.reason) {
    case eReflowReason_Initial:
      reflowReasonString = "initial";
      break;

    case eReflowReason_Resize:
      reflowReasonString = "resize";
      break;
    case eReflowReason_Dirty:
      reflowReasonString = "dirty";
      break;
    case eReflowReason_StyleChange:
      reflowReasonString = "stylechange";
      break;
    case eReflowReason_Incremental: {
      switch (aReflowInput.reflowCommand->Type()) {
        case eReflowType_StyleChanged:
          reflowReasonString = "incremental (StyleChanged)";
          break;
        case eReflowType_ReflowDirty:
          reflowReasonString = "incremental (ReflowDirty)";
          break;
        default:
          reflowReasonString = "incremental (Unknown)";
      }
    } break;
    default:
      reflowReasonString = "unknown";
      break;
  }

  printf("%s", reflowReasonString);
}

#endif

#ifdef DEBUG
static void GetTagName(nsFrame* aFrame, nsIContent* aContent, int aResultSize,
                       char* aResult) {
  if (aContent) {
    snprintf(aResult, aResultSize, "%s@%p",
             nsAtomCString(aContent->NodeInfo()->NameAtom()).get(), aFrame);
  } else {
    snprintf(aResult, aResultSize, "@%p", aFrame);
  }
}

void nsFrame::Trace(const char* aMethod, bool aEnter) {
  if (NS_FRAME_LOG_TEST(sFrameLogModule, NS_FRAME_TRACE_CALLS)) {
    char tagbuf[40];
    GetTagName(this, mContent, sizeof(tagbuf), tagbuf);
    printf_stderr("%s: %s %s", tagbuf, aEnter ? "enter" : "exit", aMethod);
  }
}

void nsFrame::Trace(const char* aMethod, bool aEnter,
                    const nsReflowStatus& aStatus) {
  if (NS_FRAME_LOG_TEST(sFrameLogModule, NS_FRAME_TRACE_CALLS)) {
    char tagbuf[40];
    GetTagName(this, mContent, sizeof(tagbuf), tagbuf);
    printf_stderr("%s: %s %s, status=%scomplete%s", tagbuf,
                  aEnter ? "enter" : "exit", aMethod,
                  aStatus.IsIncomplete() ? "not" : "",
                  (aStatus.NextInFlowNeedsReflow()) ? "+reflow" : "");
  }
}

void nsFrame::TraceMsg(const char* aFormatString, ...) {
  if (NS_FRAME_LOG_TEST(sFrameLogModule, NS_FRAME_TRACE_CALLS)) {
    // Format arguments into a buffer
    char argbuf[200];
    va_list ap;
    va_start(ap, aFormatString);
    VsprintfLiteral(argbuf, aFormatString, ap);
    va_end(ap);

    char tagbuf[40];
    GetTagName(this, mContent, sizeof(tagbuf), tagbuf);
    printf_stderr("%s: %s", tagbuf, argbuf);
  }
}

void nsFrame::VerifyDirtyBitSet(const nsFrameList& aFrameList) {
  for (nsFrameList::Enumerator e(aFrameList); !e.AtEnd(); e.Next()) {
    NS_ASSERTION(e.get()->GetStateBits() & NS_FRAME_IS_DIRTY,
                 "dirty bit not set");
  }
}

// Start Display Reflow
DR_cookie::DR_cookie(nsPresContext* aPresContext, nsIFrame* aFrame,
                     const ReflowInput& aReflowInput, ReflowOutput& aMetrics,
                     nsReflowStatus& aStatus)
    : mPresContext(aPresContext),
      mFrame(aFrame),
      mReflowInput(aReflowInput),
      mMetrics(aMetrics),
      mStatus(aStatus) {
  MOZ_COUNT_CTOR(DR_cookie);
  mValue = nsFrame::DisplayReflowEnter(aPresContext, mFrame, mReflowInput);
}

DR_cookie::~DR_cookie() {
  MOZ_COUNT_DTOR(DR_cookie);
  nsFrame::DisplayReflowExit(mPresContext, mFrame, mMetrics, mStatus, mValue);
}

DR_layout_cookie::DR_layout_cookie(nsIFrame* aFrame) : mFrame(aFrame) {
  MOZ_COUNT_CTOR(DR_layout_cookie);
  mValue = nsFrame::DisplayLayoutEnter(mFrame);
}

DR_layout_cookie::~DR_layout_cookie() {
  MOZ_COUNT_DTOR(DR_layout_cookie);
  nsFrame::DisplayLayoutExit(mFrame, mValue);
}

DR_intrinsic_inline_size_cookie::DR_intrinsic_inline_size_cookie(
    nsIFrame* aFrame, const char* aType, nscoord& aResult)
    : mFrame(aFrame), mType(aType), mResult(aResult) {
  MOZ_COUNT_CTOR(DR_intrinsic_inline_size_cookie);
  mValue = nsFrame::DisplayIntrinsicISizeEnter(mFrame, mType);
}

DR_intrinsic_inline_size_cookie::~DR_intrinsic_inline_size_cookie() {
  MOZ_COUNT_DTOR(DR_intrinsic_inline_size_cookie);
  nsFrame::DisplayIntrinsicISizeExit(mFrame, mType, mResult, mValue);
}

DR_intrinsic_size_cookie::DR_intrinsic_size_cookie(nsIFrame* aFrame,
                                                   const char* aType,
                                                   nsSize& aResult)
    : mFrame(aFrame), mType(aType), mResult(aResult) {
  MOZ_COUNT_CTOR(DR_intrinsic_size_cookie);
  mValue = nsFrame::DisplayIntrinsicSizeEnter(mFrame, mType);
}

DR_intrinsic_size_cookie::~DR_intrinsic_size_cookie() {
  MOZ_COUNT_DTOR(DR_intrinsic_size_cookie);
  nsFrame::DisplayIntrinsicSizeExit(mFrame, mType, mResult, mValue);
}

DR_init_constraints_cookie::DR_init_constraints_cookie(
    nsIFrame* aFrame, ReflowInput* aState, nscoord aCBWidth, nscoord aCBHeight,
    const nsMargin* aMargin, const nsMargin* aPadding)
    : mFrame(aFrame), mState(aState) {
  MOZ_COUNT_CTOR(DR_init_constraints_cookie);
  mValue = ReflowInput::DisplayInitConstraintsEnter(
      mFrame, mState, aCBWidth, aCBHeight, aMargin, aPadding);
}

DR_init_constraints_cookie::~DR_init_constraints_cookie() {
  MOZ_COUNT_DTOR(DR_init_constraints_cookie);
  ReflowInput::DisplayInitConstraintsExit(mFrame, mState, mValue);
}

DR_init_offsets_cookie::DR_init_offsets_cookie(nsIFrame* aFrame,
                                               SizeComputationInput* aState,
                                               nscoord aPercentBasis,
                                               WritingMode aCBWritingMode,
                                               const nsMargin* aMargin,
                                               const nsMargin* aPadding)
    : mFrame(aFrame), mState(aState) {
  MOZ_COUNT_CTOR(DR_init_offsets_cookie);
  mValue = SizeComputationInput::DisplayInitOffsetsEnter(
      mFrame, mState, aPercentBasis, aCBWritingMode, aMargin, aPadding);
}

DR_init_offsets_cookie::~DR_init_offsets_cookie() {
  MOZ_COUNT_DTOR(DR_init_offsets_cookie);
  SizeComputationInput::DisplayInitOffsetsExit(mFrame, mState, mValue);
}

DR_init_type_cookie::DR_init_type_cookie(nsIFrame* aFrame, ReflowInput* aState)
    : mFrame(aFrame), mState(aState) {
  MOZ_COUNT_CTOR(DR_init_type_cookie);
  mValue = ReflowInput::DisplayInitFrameTypeEnter(mFrame, mState);
}

DR_init_type_cookie::~DR_init_type_cookie() {
  MOZ_COUNT_DTOR(DR_init_type_cookie);
  ReflowInput::DisplayInitFrameTypeExit(mFrame, mState, mValue);
}

struct DR_Rule;

struct DR_FrameTypeInfo {
  DR_FrameTypeInfo(LayoutFrameType aFrameType, const char* aFrameNameAbbrev,
                   const char* aFrameName);
  ~DR_FrameTypeInfo();

  LayoutFrameType mType;
  char mNameAbbrev[16];
  char mName[32];
  nsTArray<DR_Rule*> mRules;

 private:
  DR_FrameTypeInfo& operator=(const DR_FrameTypeInfo&) = delete;
};

struct DR_FrameTreeNode;
struct DR_Rule;

struct DR_State {
  DR_State();
  ~DR_State();
  void Init();
  void AddFrameTypeInfo(LayoutFrameType aFrameType,
                        const char* aFrameNameAbbrev, const char* aFrameName);
  DR_FrameTypeInfo* GetFrameTypeInfo(LayoutFrameType aFrameType);
  DR_FrameTypeInfo* GetFrameTypeInfo(char* aFrameName);
  void InitFrameTypeTable();
  DR_FrameTreeNode* CreateTreeNode(nsIFrame* aFrame,
                                   const ReflowInput* aReflowInput);
  void FindMatchingRule(DR_FrameTreeNode& aNode);
  bool RuleMatches(DR_Rule& aRule, DR_FrameTreeNode& aNode);
  bool GetToken(FILE* aFile, char* aBuf, size_t aBufSize);
  DR_Rule* ParseRule(FILE* aFile);
  void ParseRulesFile();
  void AddRule(nsTArray<DR_Rule*>& aRules, DR_Rule& aRule);
  bool IsWhiteSpace(int c);
  bool GetNumber(char* aBuf, int32_t& aNumber);
  void PrettyUC(nscoord aSize, char* aBuf, int aBufSize);
  void PrintMargin(const char* tag, const nsMargin* aMargin);
  void DisplayFrameTypeInfo(nsIFrame* aFrame, int32_t aIndent);
  void DeleteTreeNode(DR_FrameTreeNode& aNode);

  bool mInited;
  bool mActive;
  int32_t mCount;
  int32_t mAssert;
  int32_t mIndent;
  bool mIndentUndisplayedFrames;
  bool mDisplayPixelErrors;
  nsTArray<DR_Rule*> mWildRules;
  nsTArray<DR_FrameTypeInfo> mFrameTypeTable;
  // reflow specific state
  nsTArray<DR_FrameTreeNode*> mFrameTreeLeaves;
};

static DR_State* DR_state;  // the one and only DR_State

struct DR_RulePart {
  explicit DR_RulePart(LayoutFrameType aFrameType)
      : mFrameType(aFrameType), mNext(0) {}

  void Destroy();

  LayoutFrameType mFrameType;
  DR_RulePart* mNext;
};

void DR_RulePart::Destroy() {
  if (mNext) {
    mNext->Destroy();
  }
  delete this;
}

struct DR_Rule {
  DR_Rule() : mLength(0), mTarget(nullptr), mDisplay(false) {
    MOZ_COUNT_CTOR(DR_Rule);
  }
  ~DR_Rule() {
    if (mTarget) mTarget->Destroy();
    MOZ_COUNT_DTOR(DR_Rule);
  }
  void AddPart(LayoutFrameType aFrameType);

  uint32_t mLength;
  DR_RulePart* mTarget;
  bool mDisplay;
};

void DR_Rule::AddPart(LayoutFrameType aFrameType) {
  DR_RulePart* newPart = new DR_RulePart(aFrameType);
  newPart->mNext = mTarget;
  mTarget = newPart;
  mLength++;
}

DR_FrameTypeInfo::~DR_FrameTypeInfo() {
  int32_t numElements;
  numElements = mRules.Length();
  for (int32_t i = numElements - 1; i >= 0; i--) {
    delete mRules.ElementAt(i);
  }
}

DR_FrameTypeInfo::DR_FrameTypeInfo(LayoutFrameType aFrameType,
                                   const char* aFrameNameAbbrev,
                                   const char* aFrameName) {
  mType = aFrameType;
  PL_strncpyz(mNameAbbrev, aFrameNameAbbrev, sizeof(mNameAbbrev));
  PL_strncpyz(mName, aFrameName, sizeof(mName));
}

struct DR_FrameTreeNode {
  DR_FrameTreeNode(nsIFrame* aFrame, DR_FrameTreeNode* aParent)
      : mFrame(aFrame), mParent(aParent), mDisplay(0), mIndent(0) {
    MOZ_COUNT_CTOR(DR_FrameTreeNode);
  }

  MOZ_COUNTED_DTOR(DR_FrameTreeNode)

  nsIFrame* mFrame;
  DR_FrameTreeNode* mParent;
  bool mDisplay;
  uint32_t mIndent;
};

// DR_State implementation

DR_State::DR_State()
    : mInited(false),
      mActive(false),
      mCount(0),
      mAssert(-1),
      mIndent(0),
      mIndentUndisplayedFrames(false),
      mDisplayPixelErrors(false) {
  MOZ_COUNT_CTOR(DR_State);
}

void DR_State::Init() {
  char* env = PR_GetEnv("GECKO_DISPLAY_REFLOW_ASSERT");
  int32_t num;
  if (env) {
    if (GetNumber(env, num))
      mAssert = num;
    else
      printf("GECKO_DISPLAY_REFLOW_ASSERT - invalid value = %s", env);
  }

  env = PR_GetEnv("GECKO_DISPLAY_REFLOW_INDENT_START");
  if (env) {
    if (GetNumber(env, num))
      mIndent = num;
    else
      printf("GECKO_DISPLAY_REFLOW_INDENT_START - invalid value = %s", env);
  }

  env = PR_GetEnv("GECKO_DISPLAY_REFLOW_INDENT_UNDISPLAYED_FRAMES");
  if (env) {
    if (GetNumber(env, num))
      mIndentUndisplayedFrames = num;
    else
      printf(
          "GECKO_DISPLAY_REFLOW_INDENT_UNDISPLAYED_FRAMES - invalid value = %s",
          env);
  }

  env = PR_GetEnv("GECKO_DISPLAY_REFLOW_FLAG_PIXEL_ERRORS");
  if (env) {
    if (GetNumber(env, num))
      mDisplayPixelErrors = num;
    else
      printf("GECKO_DISPLAY_REFLOW_FLAG_PIXEL_ERRORS - invalid value = %s",
             env);
  }

  InitFrameTypeTable();
  ParseRulesFile();
  mInited = true;
}

DR_State::~DR_State() {
  MOZ_COUNT_DTOR(DR_State);
  int32_t numElements, i;
  numElements = mWildRules.Length();
  for (i = numElements - 1; i >= 0; i--) {
    delete mWildRules.ElementAt(i);
  }
  numElements = mFrameTreeLeaves.Length();
  for (i = numElements - 1; i >= 0; i--) {
    delete mFrameTreeLeaves.ElementAt(i);
  }
}

bool DR_State::GetNumber(char* aBuf, int32_t& aNumber) {
  if (sscanf(aBuf, "%d", &aNumber) > 0)
    return true;
  else
    return false;
}

bool DR_State::IsWhiteSpace(int c) {
  return (c == ' ') || (c == '\t') || (c == '\n') || (c == '\r');
}

bool DR_State::GetToken(FILE* aFile, char* aBuf, size_t aBufSize) {
  bool haveToken = false;
  aBuf[0] = 0;
  // get the 1st non whitespace char
  int c = -1;
  for (c = getc(aFile); (c > 0) && IsWhiteSpace(c); c = getc(aFile)) {
  }

  if (c > 0) {
    haveToken = true;
    aBuf[0] = c;
    // get everything up to the next whitespace char
    size_t cX;
    for (cX = 1; cX + 1 < aBufSize; cX++) {
      c = getc(aFile);
      if (c < 0) {  // EOF
        ungetc(' ', aFile);
        break;
      } else {
        if (IsWhiteSpace(c)) {
          break;
        } else {
          aBuf[cX] = c;
        }
      }
    }
    aBuf[cX] = 0;
  }
  return haveToken;
}

DR_Rule* DR_State::ParseRule(FILE* aFile) {
  char buf[128];
  int32_t doDisplay;
  DR_Rule* rule = nullptr;
  while (GetToken(aFile, buf, sizeof(buf))) {
    if (GetNumber(buf, doDisplay)) {
      if (rule) {
        rule->mDisplay = !!doDisplay;
        break;
      } else {
        printf("unexpected token - %s \n", buf);
      }
    } else {
      if (!rule) {
        rule = new DR_Rule;
      }
      if (strcmp(buf, "*") == 0) {
        rule->AddPart(LayoutFrameType::None);
      } else {
        DR_FrameTypeInfo* info = GetFrameTypeInfo(buf);
        if (info) {
          rule->AddPart(info->mType);
        } else {
          printf("invalid frame type - %s \n", buf);
        }
      }
    }
  }
  return rule;
}

void DR_State::AddRule(nsTArray<DR_Rule*>& aRules, DR_Rule& aRule) {
  int32_t numRules = aRules.Length();
  for (int32_t ruleX = 0; ruleX < numRules; ruleX++) {
    DR_Rule* rule = aRules.ElementAt(ruleX);
    NS_ASSERTION(rule, "program error");
    if (aRule.mLength > rule->mLength) {
      aRules.InsertElementAt(ruleX, &aRule);
      return;
    }
  }
  aRules.AppendElement(&aRule);
}

static Maybe<bool> ShouldLogReflow(const char* processes) {
  switch (processes[0]) {
    case 'A':
    case 'a':
      return Some(true);
    case 'P':
    case 'p':
      return Some(XRE_IsParentProcess());
    case 'C':
    case 'c':
      return Some(XRE_IsContentProcess());
    default:
      return Nothing{};
  }
}

void DR_State::ParseRulesFile() {
  char* processes = PR_GetEnv("GECKO_DISPLAY_REFLOW_PROCESSES");
  if (processes) {
    Maybe<bool> enableLog = ShouldLogReflow(processes);
    if (enableLog.isNothing()) {
      MOZ_CRASH("GECKO_DISPLAY_REFLOW_PROCESSES: [a]ll [p]arent [c]ontent");
    } else if (enableLog.value()) {
      DR_Rule* rule = new DR_Rule;
      rule->AddPart(LayoutFrameType::None);
      rule->mDisplay = true;
      AddRule(mWildRules, *rule);
      mActive = true;
    }
    return;
  }

  char* path = PR_GetEnv("GECKO_DISPLAY_REFLOW_RULES_FILE");
  if (path) {
    FILE* inFile = fopen(path, "r");
    if (!inFile) {
      MOZ_CRASH(
          "Failed to open the specified rules file; Try `--setpref "
          "security.sandbox.content.level=2` if the sandbox is at cause");
    }
    for (DR_Rule* rule = ParseRule(inFile); rule; rule = ParseRule(inFile)) {
      if (rule->mTarget) {
        LayoutFrameType fType = rule->mTarget->mFrameType;
        if (fType != LayoutFrameType::None) {
          DR_FrameTypeInfo* info = GetFrameTypeInfo(fType);
          AddRule(info->mRules, *rule);
        } else {
          AddRule(mWildRules, *rule);
        }
        mActive = true;
      }
    }

    fclose(inFile);
  }
}

void DR_State::AddFrameTypeInfo(LayoutFrameType aFrameType,
                                const char* aFrameNameAbbrev,
                                const char* aFrameName) {
  mFrameTypeTable.AppendElement(
      DR_FrameTypeInfo(aFrameType, aFrameNameAbbrev, aFrameName));
}

DR_FrameTypeInfo* DR_State::GetFrameTypeInfo(LayoutFrameType aFrameType) {
  int32_t numEntries = mFrameTypeTable.Length();
  NS_ASSERTION(numEntries != 0, "empty FrameTypeTable");
  for (int32_t i = 0; i < numEntries; i++) {
    DR_FrameTypeInfo& info = mFrameTypeTable.ElementAt(i);
    if (info.mType == aFrameType) {
      return &info;
    }
  }
  return &mFrameTypeTable.ElementAt(numEntries -
                                    1);  // return unknown frame type
}

DR_FrameTypeInfo* DR_State::GetFrameTypeInfo(char* aFrameName) {
  int32_t numEntries = mFrameTypeTable.Length();
  NS_ASSERTION(numEntries != 0, "empty FrameTypeTable");
  for (int32_t i = 0; i < numEntries; i++) {
    DR_FrameTypeInfo& info = mFrameTypeTable.ElementAt(i);
    if ((strcmp(aFrameName, info.mName) == 0) ||
        (strcmp(aFrameName, info.mNameAbbrev) == 0)) {
      return &info;
    }
  }
  return &mFrameTypeTable.ElementAt(numEntries -
                                    1);  // return unknown frame type
}

void DR_State::InitFrameTypeTable() {
  AddFrameTypeInfo(LayoutFrameType::Block, "block", "block");
  AddFrameTypeInfo(LayoutFrameType::Br, "br", "br");
  AddFrameTypeInfo(LayoutFrameType::Bullet, "bullet", "bullet");
  AddFrameTypeInfo(LayoutFrameType::ColorControl, "color", "colorControl");
  AddFrameTypeInfo(LayoutFrameType::GfxButtonControl, "button",
                   "gfxButtonControl");
  AddFrameTypeInfo(LayoutFrameType::HTMLButtonControl, "HTMLbutton",
                   "HTMLButtonControl");
  AddFrameTypeInfo(LayoutFrameType::HTMLCanvas, "HTMLCanvas", "HTMLCanvas");
  AddFrameTypeInfo(LayoutFrameType::SubDocument, "subdoc", "subDocument");
  AddFrameTypeInfo(LayoutFrameType::Image, "img", "image");
  AddFrameTypeInfo(LayoutFrameType::Inline, "inline", "inline");
  AddFrameTypeInfo(LayoutFrameType::Letter, "letter", "letter");
  AddFrameTypeInfo(LayoutFrameType::Line, "line", "line");
  AddFrameTypeInfo(LayoutFrameType::ListControl, "select", "select");
  AddFrameTypeInfo(LayoutFrameType::Object, "obj", "object");
  AddFrameTypeInfo(LayoutFrameType::Page, "page", "page");
  AddFrameTypeInfo(LayoutFrameType::Placeholder, "place", "placeholder");
  AddFrameTypeInfo(LayoutFrameType::Canvas, "canvas", "canvas");
  AddFrameTypeInfo(LayoutFrameType::Root, "root", "root");
  AddFrameTypeInfo(LayoutFrameType::Scroll, "scroll", "scroll");
  AddFrameTypeInfo(LayoutFrameType::TableCell, "cell", "tableCell");
  AddFrameTypeInfo(LayoutFrameType::TableCol, "col", "tableCol");
  AddFrameTypeInfo(LayoutFrameType::TableColGroup, "colG", "tableColGroup");
  AddFrameTypeInfo(LayoutFrameType::Table, "tbl", "table");
  AddFrameTypeInfo(LayoutFrameType::TableWrapper, "tblW", "tableWrapper");
  AddFrameTypeInfo(LayoutFrameType::TableRowGroup, "rowG", "tableRowGroup");
  AddFrameTypeInfo(LayoutFrameType::TableRow, "row", "tableRow");
  AddFrameTypeInfo(LayoutFrameType::TextInput, "textCtl", "textInput");
  AddFrameTypeInfo(LayoutFrameType::Text, "text", "text");
  AddFrameTypeInfo(LayoutFrameType::Viewport, "VP", "viewport");
#  ifdef MOZ_XUL
  AddFrameTypeInfo(LayoutFrameType::XULLabel, "XULLabel", "XULLabel");
  AddFrameTypeInfo(LayoutFrameType::Box, "Box", "Box");
  AddFrameTypeInfo(LayoutFrameType::Slider, "Slider", "Slider");
  AddFrameTypeInfo(LayoutFrameType::PopupSet, "PopupSet", "PopupSet");
#  endif
  AddFrameTypeInfo(LayoutFrameType::None, "unknown", "unknown");
}

void DR_State::DisplayFrameTypeInfo(nsIFrame* aFrame, int32_t aIndent) {
  DR_FrameTypeInfo* frameTypeInfo = GetFrameTypeInfo(aFrame->Type());
  if (frameTypeInfo) {
    for (int32_t i = 0; i < aIndent; i++) {
      printf(" ");
    }
    if (!strcmp(frameTypeInfo->mNameAbbrev, "unknown")) {
      if (aFrame) {
        nsAutoString name;
        aFrame->GetFrameName(name);
        printf("%s %p ", NS_LossyConvertUTF16toASCII(name).get(),
               (void*)aFrame);
      } else {
        printf("%s %p ", frameTypeInfo->mNameAbbrev, (void*)aFrame);
      }
    } else {
      printf("%s %p ", frameTypeInfo->mNameAbbrev, (void*)aFrame);
    }
  }
}

bool DR_State::RuleMatches(DR_Rule& aRule, DR_FrameTreeNode& aNode) {
  NS_ASSERTION(aRule.mTarget, "program error");

  DR_RulePart* rulePart;
  DR_FrameTreeNode* parentNode;
  for (rulePart = aRule.mTarget->mNext, parentNode = aNode.mParent;
       rulePart && parentNode;
       rulePart = rulePart->mNext, parentNode = parentNode->mParent) {
    if (rulePart->mFrameType != LayoutFrameType::None) {
      if (parentNode->mFrame) {
        if (rulePart->mFrameType != parentNode->mFrame->Type()) {
          return false;
        }
      } else
        NS_ASSERTION(false, "program error");
    }
    // else wild card match
  }
  return true;
}

void DR_State::FindMatchingRule(DR_FrameTreeNode& aNode) {
  if (!aNode.mFrame) {
    NS_ASSERTION(false, "invalid DR_FrameTreeNode \n");
    return;
  }

  bool matchingRule = false;

  DR_FrameTypeInfo* info = GetFrameTypeInfo(aNode.mFrame->Type());
  NS_ASSERTION(info, "program error");
  int32_t numRules = info->mRules.Length();
  for (int32_t ruleX = 0; ruleX < numRules; ruleX++) {
    DR_Rule* rule = info->mRules.ElementAt(ruleX);
    if (rule && RuleMatches(*rule, aNode)) {
      aNode.mDisplay = rule->mDisplay;
      matchingRule = true;
      break;
    }
  }
  if (!matchingRule) {
    int32_t numWildRules = mWildRules.Length();
    for (int32_t ruleX = 0; ruleX < numWildRules; ruleX++) {
      DR_Rule* rule = mWildRules.ElementAt(ruleX);
      if (rule && RuleMatches(*rule, aNode)) {
        aNode.mDisplay = rule->mDisplay;
        break;
      }
    }
  }
}

DR_FrameTreeNode* DR_State::CreateTreeNode(nsIFrame* aFrame,
                                           const ReflowInput* aReflowInput) {
  // find the frame of the parent reflow input (usually just the parent of
  // aFrame)
  nsIFrame* parentFrame;
  if (aReflowInput) {
    const ReflowInput* parentRI = aReflowInput->mParentReflowInput;
    parentFrame = (parentRI) ? parentRI->mFrame : nullptr;
  } else {
    parentFrame = aFrame->GetParent();
  }

  // find the parent tree node leaf
  DR_FrameTreeNode* parentNode = nullptr;

  DR_FrameTreeNode* lastLeaf = nullptr;
  if (mFrameTreeLeaves.Length())
    lastLeaf = mFrameTreeLeaves.ElementAt(mFrameTreeLeaves.Length() - 1);
  if (lastLeaf) {
    for (parentNode = lastLeaf;
         parentNode && (parentNode->mFrame != parentFrame);
         parentNode = parentNode->mParent) {
    }
  }
  DR_FrameTreeNode* newNode = new DR_FrameTreeNode(aFrame, parentNode);
  FindMatchingRule(*newNode);

  newNode->mIndent = mIndent;
  if (newNode->mDisplay || mIndentUndisplayedFrames) {
    ++mIndent;
  }

  if (lastLeaf && (lastLeaf == parentNode)) {
    mFrameTreeLeaves.RemoveLastElement();
  }
  mFrameTreeLeaves.AppendElement(newNode);
  mCount++;

  return newNode;
}

void DR_State::PrettyUC(nscoord aSize, char* aBuf, int aBufSize) {
  if (NS_UNCONSTRAINEDSIZE == aSize) {
    strcpy(aBuf, "UC");
  } else {
    if ((nscoord)0xdeadbeefU == aSize) {
      strcpy(aBuf, "deadbeef");
    } else {
      snprintf(aBuf, aBufSize, "%d", aSize);
    }
  }
}

void DR_State::PrintMargin(const char* tag, const nsMargin* aMargin) {
  if (aMargin) {
    char t[16], r[16], b[16], l[16];
    PrettyUC(aMargin->top, t, 16);
    PrettyUC(aMargin->right, r, 16);
    PrettyUC(aMargin->bottom, b, 16);
    PrettyUC(aMargin->left, l, 16);
    printf(" %s=%s,%s,%s,%s", tag, t, r, b, l);
  } else {
    // use %p here for consistency with other null-pointer printouts
    printf(" %s=%p", tag, (void*)aMargin);
  }
}

void DR_State::DeleteTreeNode(DR_FrameTreeNode& aNode) {
  mFrameTreeLeaves.RemoveElement(&aNode);
  int32_t numLeaves = mFrameTreeLeaves.Length();
  if ((0 == numLeaves) ||
      (aNode.mParent != mFrameTreeLeaves.ElementAt(numLeaves - 1))) {
    mFrameTreeLeaves.AppendElement(aNode.mParent);
  }

  if (aNode.mDisplay || mIndentUndisplayedFrames) {
    --mIndent;
  }
  // delete the tree node
  delete &aNode;
}

static void CheckPixelError(nscoord aSize, int32_t aPixelToTwips) {
  if (NS_UNCONSTRAINEDSIZE != aSize) {
    if ((aSize % aPixelToTwips) > 0) {
      printf("VALUE %d is not a whole pixel \n", aSize);
    }
  }
}

static void DisplayReflowEnterPrint(nsPresContext* aPresContext,
                                    nsIFrame* aFrame,
                                    const ReflowInput& aReflowInput,
                                    DR_FrameTreeNode& aTreeNode,
                                    bool aChanged) {
  if (aTreeNode.mDisplay) {
    DR_state->DisplayFrameTypeInfo(aFrame, aTreeNode.mIndent);

    char width[16];
    char height[16];

    DR_state->PrettyUC(aReflowInput.AvailableWidth(), width, 16);
    DR_state->PrettyUC(aReflowInput.AvailableHeight(), height, 16);
    printf("Reflow a=%s,%s ", width, height);

    DR_state->PrettyUC(aReflowInput.ComputedWidth(), width, 16);
    DR_state->PrettyUC(aReflowInput.ComputedHeight(), height, 16);
    printf("c=%s,%s ", width, height);

    if (aFrame->GetStateBits() & NS_FRAME_IS_DIRTY) printf("dirty ");

    if (aFrame->GetStateBits() & NS_FRAME_HAS_DIRTY_CHILDREN)
      printf("dirty-children ");

    if (aReflowInput.mFlags.mSpecialBSizeReflow) printf("special-bsize ");

    if (aReflowInput.IsHResize()) printf("h-resize ");

    if (aReflowInput.IsVResize()) printf("v-resize ");

    nsIFrame* inFlow = aFrame->GetPrevInFlow();
    if (inFlow) {
      printf("pif=%p ", (void*)inFlow);
    }
    inFlow = aFrame->GetNextInFlow();
    if (inFlow) {
      printf("nif=%p ", (void*)inFlow);
    }
    if (aChanged)
      printf("CHANGED \n");
    else
      printf("cnt=%d \n", DR_state->mCount);
    if (DR_state->mDisplayPixelErrors) {
      int32_t d2a = aPresContext->AppUnitsPerDevPixel();
      CheckPixelError(aReflowInput.AvailableWidth(), d2a);
      CheckPixelError(aReflowInput.AvailableHeight(), d2a);
      CheckPixelError(aReflowInput.ComputedWidth(), d2a);
      CheckPixelError(aReflowInput.ComputedHeight(), d2a);
    }
  }
}

void* nsFrame::DisplayReflowEnter(nsPresContext* aPresContext, nsIFrame* aFrame,
                                  const ReflowInput& aReflowInput) {
  if (!DR_state->mInited) DR_state->Init();
  if (!DR_state->mActive) return nullptr;

  NS_ASSERTION(aFrame, "invalid call");

  DR_FrameTreeNode* treeNode = DR_state->CreateTreeNode(aFrame, &aReflowInput);
  if (treeNode) {
    DisplayReflowEnterPrint(aPresContext, aFrame, aReflowInput, *treeNode,
                            false);
  }
  return treeNode;
}

void* nsFrame::DisplayLayoutEnter(nsIFrame* aFrame) {
  if (!DR_state->mInited) DR_state->Init();
  if (!DR_state->mActive) return nullptr;

  NS_ASSERTION(aFrame, "invalid call");

  DR_FrameTreeNode* treeNode = DR_state->CreateTreeNode(aFrame, nullptr);
  if (treeNode && treeNode->mDisplay) {
    DR_state->DisplayFrameTypeInfo(aFrame, treeNode->mIndent);
    printf("XULLayout\n");
  }
  return treeNode;
}

void* nsFrame::DisplayIntrinsicISizeEnter(nsIFrame* aFrame, const char* aType) {
  if (!DR_state->mInited) DR_state->Init();
  if (!DR_state->mActive) return nullptr;

  NS_ASSERTION(aFrame, "invalid call");

  DR_FrameTreeNode* treeNode = DR_state->CreateTreeNode(aFrame, nullptr);
  if (treeNode && treeNode->mDisplay) {
    DR_state->DisplayFrameTypeInfo(aFrame, treeNode->mIndent);
    printf("Get%sISize\n", aType);
  }
  return treeNode;
}

void* nsFrame::DisplayIntrinsicSizeEnter(nsIFrame* aFrame, const char* aType) {
  if (!DR_state->mInited) DR_state->Init();
  if (!DR_state->mActive) return nullptr;

  NS_ASSERTION(aFrame, "invalid call");

  DR_FrameTreeNode* treeNode = DR_state->CreateTreeNode(aFrame, nullptr);
  if (treeNode && treeNode->mDisplay) {
    DR_state->DisplayFrameTypeInfo(aFrame, treeNode->mIndent);
    printf("Get%sSize\n", aType);
  }
  return treeNode;
}

void nsFrame::DisplayReflowExit(nsPresContext* aPresContext, nsIFrame* aFrame,
                                ReflowOutput& aMetrics,
                                const nsReflowStatus& aStatus,
                                void* aFrameTreeNode) {
  if (!DR_state->mActive) return;

  NS_ASSERTION(aFrame, "DisplayReflowExit - invalid call");
  if (!aFrameTreeNode) return;

  DR_FrameTreeNode* treeNode = (DR_FrameTreeNode*)aFrameTreeNode;
  if (treeNode->mDisplay) {
    DR_state->DisplayFrameTypeInfo(aFrame, treeNode->mIndent);

    char width[16];
    char height[16];
    char x[16];
    char y[16];
    DR_state->PrettyUC(aMetrics.Width(), width, 16);
    DR_state->PrettyUC(aMetrics.Height(), height, 16);
    printf("Reflow d=%s,%s", width, height);

    if (!aStatus.IsEmpty()) {
      printf(" status=%s", ToString(aStatus).c_str());
    }
    if (aFrame->HasOverflowAreas()) {
      DR_state->PrettyUC(aMetrics.VisualOverflow().x, x, 16);
      DR_state->PrettyUC(aMetrics.VisualOverflow().y, y, 16);
      DR_state->PrettyUC(aMetrics.VisualOverflow().width, width, 16);
      DR_state->PrettyUC(aMetrics.VisualOverflow().height, height, 16);
      printf(" vis-o=(%s,%s) %s x %s", x, y, width, height);

      nsRect storedOverflow = aFrame->GetVisualOverflowRect();
      DR_state->PrettyUC(storedOverflow.x, x, 16);
      DR_state->PrettyUC(storedOverflow.y, y, 16);
      DR_state->PrettyUC(storedOverflow.width, width, 16);
      DR_state->PrettyUC(storedOverflow.height, height, 16);
      printf(" vis-sto=(%s,%s) %s x %s", x, y, width, height);

      DR_state->PrettyUC(aMetrics.ScrollableOverflow().x, x, 16);
      DR_state->PrettyUC(aMetrics.ScrollableOverflow().y, y, 16);
      DR_state->PrettyUC(aMetrics.ScrollableOverflow().width, width, 16);
      DR_state->PrettyUC(aMetrics.ScrollableOverflow().height, height, 16);
      printf(" scr-o=(%s,%s) %s x %s", x, y, width, height);

      storedOverflow = aFrame->GetScrollableOverflowRect();
      DR_state->PrettyUC(storedOverflow.x, x, 16);
      DR_state->PrettyUC(storedOverflow.y, y, 16);
      DR_state->PrettyUC(storedOverflow.width, width, 16);
      DR_state->PrettyUC(storedOverflow.height, height, 16);
      printf(" scr-sto=(%s,%s) %s x %s", x, y, width, height);
    }
    printf("\n");
    if (DR_state->mDisplayPixelErrors) {
      int32_t d2a = aPresContext->AppUnitsPerDevPixel();
      CheckPixelError(aMetrics.Width(), d2a);
      CheckPixelError(aMetrics.Height(), d2a);
    }
  }
  DR_state->DeleteTreeNode(*treeNode);
}

void nsFrame::DisplayLayoutExit(nsIFrame* aFrame, void* aFrameTreeNode) {
  if (!DR_state->mActive) return;

  NS_ASSERTION(aFrame, "non-null frame required");
  if (!aFrameTreeNode) return;

  DR_FrameTreeNode* treeNode = (DR_FrameTreeNode*)aFrameTreeNode;
  if (treeNode->mDisplay) {
    DR_state->DisplayFrameTypeInfo(aFrame, treeNode->mIndent);
    nsRect rect = aFrame->GetRect();
    printf("XULLayout=%d,%d,%d,%d\n", rect.x, rect.y, rect.width, rect.height);
  }
  DR_state->DeleteTreeNode(*treeNode);
}

void nsFrame::DisplayIntrinsicISizeExit(nsIFrame* aFrame, const char* aType,
                                        nscoord aResult, void* aFrameTreeNode) {
  if (!DR_state->mActive) return;

  NS_ASSERTION(aFrame, "non-null frame required");
  if (!aFrameTreeNode) return;

  DR_FrameTreeNode* treeNode = (DR_FrameTreeNode*)aFrameTreeNode;
  if (treeNode->mDisplay) {
    DR_state->DisplayFrameTypeInfo(aFrame, treeNode->mIndent);
    char iSize[16];
    DR_state->PrettyUC(aResult, iSize, 16);
    printf("Get%sISize=%s\n", aType, iSize);
  }
  DR_state->DeleteTreeNode(*treeNode);
}

void nsFrame::DisplayIntrinsicSizeExit(nsIFrame* aFrame, const char* aType,
                                       nsSize aResult, void* aFrameTreeNode) {
  if (!DR_state->mActive) return;

  NS_ASSERTION(aFrame, "non-null frame required");
  if (!aFrameTreeNode) return;

  DR_FrameTreeNode* treeNode = (DR_FrameTreeNode*)aFrameTreeNode;
  if (treeNode->mDisplay) {
    DR_state->DisplayFrameTypeInfo(aFrame, treeNode->mIndent);

    char width[16];
    char height[16];
    DR_state->PrettyUC(aResult.width, width, 16);
    DR_state->PrettyUC(aResult.height, height, 16);
    printf("Get%sSize=%s,%s\n", aType, width, height);
  }
  DR_state->DeleteTreeNode(*treeNode);
}

/* static */
void nsFrame::DisplayReflowStartup() { DR_state = new DR_State(); }

/* static */
void nsFrame::DisplayReflowShutdown() {
  delete DR_state;
  DR_state = nullptr;
}

void DR_cookie::Change() const {
  DR_FrameTreeNode* treeNode = (DR_FrameTreeNode*)mValue;
  if (treeNode && treeNode->mDisplay) {
    DisplayReflowEnterPrint(mPresContext, mFrame, mReflowInput, *treeNode,
                            true);
  }
}

/* static */
void* ReflowInput::DisplayInitConstraintsEnter(nsIFrame* aFrame,
                                               ReflowInput* aState,
                                               nscoord aContainingBlockWidth,
                                               nscoord aContainingBlockHeight,
                                               const nsMargin* aBorder,
                                               const nsMargin* aPadding) {
  MOZ_ASSERT(aFrame, "non-null frame required");
  MOZ_ASSERT(aState, "non-null state required");

  if (!DR_state->mInited) DR_state->Init();
  if (!DR_state->mActive) return nullptr;

  DR_FrameTreeNode* treeNode = DR_state->CreateTreeNode(aFrame, aState);
  if (treeNode && treeNode->mDisplay) {
    DR_state->DisplayFrameTypeInfo(aFrame, treeNode->mIndent);

    printf("InitConstraints parent=%p", (void*)aState->mParentReflowInput);

    char width[16];
    char height[16];

    DR_state->PrettyUC(aContainingBlockWidth, width, 16);
    DR_state->PrettyUC(aContainingBlockHeight, height, 16);
    printf(" cb=%s,%s", width, height);

    DR_state->PrettyUC(aState->AvailableWidth(), width, 16);
    DR_state->PrettyUC(aState->AvailableHeight(), height, 16);
    printf(" as=%s,%s", width, height);

    DR_state->PrintMargin("b", aBorder);
    DR_state->PrintMargin("p", aPadding);
    putchar('\n');
  }
  return treeNode;
}

/* static */
void ReflowInput::DisplayInitConstraintsExit(nsIFrame* aFrame,
                                             ReflowInput* aState,
                                             void* aValue) {
  MOZ_ASSERT(aFrame, "non-null frame required");
  MOZ_ASSERT(aState, "non-null state required");

  if (!DR_state->mActive) return;
  if (!aValue) return;

  DR_FrameTreeNode* treeNode = (DR_FrameTreeNode*)aValue;
  if (treeNode->mDisplay) {
    DR_state->DisplayFrameTypeInfo(aFrame, treeNode->mIndent);
    char cmiw[16], cw[16], cmxw[16], cmih[16], ch[16], cmxh[16];
    DR_state->PrettyUC(aState->ComputedMinWidth(), cmiw, 16);
    DR_state->PrettyUC(aState->ComputedWidth(), cw, 16);
    DR_state->PrettyUC(aState->ComputedMaxWidth(), cmxw, 16);
    DR_state->PrettyUC(aState->ComputedMinHeight(), cmih, 16);
    DR_state->PrettyUC(aState->ComputedHeight(), ch, 16);
    DR_state->PrettyUC(aState->ComputedMaxHeight(), cmxh, 16);
    printf("InitConstraints= cw=(%s <= %s <= %s) ch=(%s <= %s <= %s)", cmiw, cw,
           cmxw, cmih, ch, cmxh);
    DR_state->PrintMargin("co", &aState->ComputedPhysicalOffsets());
    putchar('\n');
  }
  DR_state->DeleteTreeNode(*treeNode);
}

/* static */
void* SizeComputationInput::DisplayInitOffsetsEnter(
    nsIFrame* aFrame, SizeComputationInput* aState, nscoord aPercentBasis,
    WritingMode aCBWritingMode, const nsMargin* aBorder,
    const nsMargin* aPadding) {
  MOZ_ASSERT(aFrame, "non-null frame required");
  MOZ_ASSERT(aState, "non-null state required");

  if (!DR_state->mInited) DR_state->Init();
  if (!DR_state->mActive) return nullptr;

  // aState is not necessarily a ReflowInput
  DR_FrameTreeNode* treeNode = DR_state->CreateTreeNode(aFrame, nullptr);
  if (treeNode && treeNode->mDisplay) {
    DR_state->DisplayFrameTypeInfo(aFrame, treeNode->mIndent);

    char pctBasisStr[16];
    DR_state->PrettyUC(aPercentBasis, pctBasisStr, 16);
    printf("InitOffsets pct_basis=%s", pctBasisStr);

    DR_state->PrintMargin("b", aBorder);
    DR_state->PrintMargin("p", aPadding);
    putchar('\n');
  }
  return treeNode;
}

/* static */
void SizeComputationInput::DisplayInitOffsetsExit(nsIFrame* aFrame,
                                                  SizeComputationInput* aState,
                                                  void* aValue) {
  MOZ_ASSERT(aFrame, "non-null frame required");
  MOZ_ASSERT(aState, "non-null state required");

  if (!DR_state->mActive) return;
  if (!aValue) return;

  DR_FrameTreeNode* treeNode = (DR_FrameTreeNode*)aValue;
  if (treeNode->mDisplay) {
    DR_state->DisplayFrameTypeInfo(aFrame, treeNode->mIndent);
    printf("InitOffsets=");
    DR_state->PrintMargin("m", &aState->ComputedPhysicalMargin());
    DR_state->PrintMargin("p", &aState->ComputedPhysicalPadding());
    DR_state->PrintMargin("p+b", &aState->ComputedPhysicalBorderPadding());
    putchar('\n');
  }
  DR_state->DeleteTreeNode(*treeNode);
}

/* static */
void* ReflowInput::DisplayInitFrameTypeEnter(nsIFrame* aFrame,
                                             ReflowInput* aState) {
  MOZ_ASSERT(aFrame, "non-null frame required");
  MOZ_ASSERT(aState, "non-null state required");

  if (!DR_state->mInited) DR_state->Init();
  if (!DR_state->mActive) return nullptr;

  // we don't print anything here
  return DR_state->CreateTreeNode(aFrame, aState);
}

/* static */
void ReflowInput::DisplayInitFrameTypeExit(nsIFrame* aFrame,
                                           ReflowInput* aState, void* aValue) {
  MOZ_ASSERT(aFrame, "non-null frame required");
  MOZ_ASSERT(aState, "non-null state required");

  if (!DR_state->mActive) return;
  if (!aValue) return;

  DR_FrameTreeNode* treeNode = (DR_FrameTreeNode*)aValue;
  if (treeNode->mDisplay) {
    DR_state->DisplayFrameTypeInfo(aFrame, treeNode->mIndent);
    printf("InitFrameType");

    if (aFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW) printf(" out-of-flow");
    if (aFrame->GetPrevInFlow()) printf(" prev-in-flow");
    if (aFrame->IsAbsolutelyPositioned()) printf(" abspos");
    if (aFrame->IsFloating()) printf(" float");

    {
      nsAutoString result;
      aFrame->Style()->GetComputedPropertyValue(eCSSProperty_display, result);
      printf(" display=%s", NS_ConvertUTF16toUTF8(result).get());
    }

    // This array must exactly match the NS_CSS_FRAME_TYPE constants.
    const char* const cssFrameTypes[] = {
        "unknown", "inline", "block", "floating", "absolute", "internal-table"};
    nsCSSFrameType bareType = NS_FRAME_GET_TYPE(aState->mFrameType);
    bool repNoBlock = NS_FRAME_IS_REPLACED_NOBLOCK(aState->mFrameType);
    bool repBlock = NS_FRAME_IS_REPLACED_CONTAINS_BLOCK(aState->mFrameType);

    if (bareType >= ArrayLength(cssFrameTypes)) {
      printf(" result=type %u", bareType);
    } else {
      printf(" result=%s", cssFrameTypes[bareType]);
    }
    printf("%s%s\n", repNoBlock ? " +rep" : "", repBlock ? " +repBlk" : "");
  }
  DR_state->DeleteTreeNode(*treeNode);
}

// End Display Reflow

// Validation of SideIsVertical.
#  define CASE(side, result) \
    static_assert(SideIsVertical(side) == result, "SideIsVertical is wrong")
CASE(eSideTop, false);
CASE(eSideRight, true);
CASE(eSideBottom, false);
CASE(eSideLeft, true);
#  undef CASE

// Validation of HalfCornerIsX.
#  define CASE(corner, result) \
    static_assert(HalfCornerIsX(corner) == result, "HalfCornerIsX is wrong")
CASE(eCornerTopLeftX, true);
CASE(eCornerTopLeftY, false);
CASE(eCornerTopRightX, true);
CASE(eCornerTopRightY, false);
CASE(eCornerBottomRightX, true);
CASE(eCornerBottomRightY, false);
CASE(eCornerBottomLeftX, true);
CASE(eCornerBottomLeftY, false);
#  undef CASE

// Validation of HalfToFullCorner.
#  define CASE(corner, result)                        \
    static_assert(HalfToFullCorner(corner) == result, \
                  "HalfToFullCorner is "              \
                  "wrong")
CASE(eCornerTopLeftX, eCornerTopLeft);
CASE(eCornerTopLeftY, eCornerTopLeft);
CASE(eCornerTopRightX, eCornerTopRight);
CASE(eCornerTopRightY, eCornerTopRight);
CASE(eCornerBottomRightX, eCornerBottomRight);
CASE(eCornerBottomRightY, eCornerBottomRight);
CASE(eCornerBottomLeftX, eCornerBottomLeft);
CASE(eCornerBottomLeftY, eCornerBottomLeft);
#  undef CASE

// Validation of FullToHalfCorner.
#  define CASE(corner, vert, result)                        \
    static_assert(FullToHalfCorner(corner, vert) == result, \
                  "FullToHalfCorner is wrong")
CASE(eCornerTopLeft, false, eCornerTopLeftX);
CASE(eCornerTopLeft, true, eCornerTopLeftY);
CASE(eCornerTopRight, false, eCornerTopRightX);
CASE(eCornerTopRight, true, eCornerTopRightY);
CASE(eCornerBottomRight, false, eCornerBottomRightX);
CASE(eCornerBottomRight, true, eCornerBottomRightY);
CASE(eCornerBottomLeft, false, eCornerBottomLeftX);
CASE(eCornerBottomLeft, true, eCornerBottomLeftY);
#  undef CASE

// Validation of SideToFullCorner.
#  define CASE(side, second, result)                        \
    static_assert(SideToFullCorner(side, second) == result, \
                  "SideToFullCorner is wrong")
CASE(eSideTop, false, eCornerTopLeft);
CASE(eSideTop, true, eCornerTopRight);

CASE(eSideRight, false, eCornerTopRight);
CASE(eSideRight, true, eCornerBottomRight);

CASE(eSideBottom, false, eCornerBottomRight);
CASE(eSideBottom, true, eCornerBottomLeft);

CASE(eSideLeft, false, eCornerBottomLeft);
CASE(eSideLeft, true, eCornerTopLeft);
#  undef CASE

// Validation of SideToHalfCorner.
#  define CASE(side, second, parallel, result)                        \
    static_assert(SideToHalfCorner(side, second, parallel) == result, \
                  "SideToHalfCorner is wrong")
CASE(eSideTop, false, true, eCornerTopLeftX);
CASE(eSideTop, false, false, eCornerTopLeftY);
CASE(eSideTop, true, true, eCornerTopRightX);
CASE(eSideTop, true, false, eCornerTopRightY);

CASE(eSideRight, false, false, eCornerTopRightX);
CASE(eSideRight, false, true, eCornerTopRightY);
CASE(eSideRight, true, false, eCornerBottomRightX);
CASE(eSideRight, true, true, eCornerBottomRightY);

CASE(eSideBottom, false, true, eCornerBottomRightX);
CASE(eSideBottom, false, false, eCornerBottomRightY);
CASE(eSideBottom, true, true, eCornerBottomLeftX);
CASE(eSideBottom, true, false, eCornerBottomLeftY);

CASE(eSideLeft, false, false, eCornerBottomLeftX);
CASE(eSideLeft, false, true, eCornerBottomLeftY);
CASE(eSideLeft, true, false, eCornerTopLeftX);
CASE(eSideLeft, true, true, eCornerTopLeftY);
#  undef CASE

#endif

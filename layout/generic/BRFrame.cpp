/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for HTML <br> elements */

#include "mozilla/CaretAssociationHint.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/HTMLBRElement.h"
#include "gfxContext.h"
#include "nsCOMPtr.h"
#include "nsContainerFrame.h"
#include "nsFontMetrics.h"
#include "nsHTMLParts.h"
#include "nsIFrame.h"
#include "nsPresContext.h"
#include "nsLineLayout.h"
#include "nsStyleConsts.h"
#include "nsGkAtoms.h"
#include "nsLayoutUtils.h"

// FOR SELECTION
#include "nsIContent.h"
// END INCLUDES FOR SELECTION

using namespace mozilla;

namespace mozilla {

class BRFrame final : public nsIFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(BRFrame)

  friend nsIFrame* ::NS_NewBRFrame(mozilla::PresShell* aPresShell,
                                   ComputedStyle* aStyle);

  ContentOffsets CalcContentOffsetsFromFramePoint(
      const nsPoint& aPoint) override;

  FrameSearchResult PeekOffsetNoAmount(bool aForward,
                                       int32_t* aOffset) override;
  FrameSearchResult PeekOffsetCharacter(
      bool aForward, int32_t* aOffset,
      PeekOffsetCharacterOptions aOptions =
          PeekOffsetCharacterOptions()) override;
  FrameSearchResult PeekOffsetWord(bool aForward, bool aWordSelectEatSpace,
                                   bool aIsKeyboardSelect, int32_t* aOffset,
                                   PeekWordState* aState,
                                   bool aTrimSpaces) override;

  void Reflow(nsPresContext* aPresContext, ReflowOutput& aMetrics,
              const ReflowInput& aReflowInput,
              nsReflowStatus& aStatus) override;
  void AddInlineMinISize(gfxContext* aRenderingContext,
                         InlineMinISizeData* aData) override;
  void AddInlinePrefISize(gfxContext* aRenderingContext,
                          InlinePrefISizeData* aData) override;
  nscoord GetMinISize(gfxContext* aRenderingContext) override;
  nscoord GetPrefISize(gfxContext* aRenderingContext) override;

  Maybe<nscoord> GetNaturalBaselineBOffset(
      WritingMode aWM, BaselineSharingGroup aBaselineGroup,
      BaselineExportContext) const override;

#ifdef ACCESSIBILITY
  mozilla::a11y::AccType AccessibleType() override;
#endif

#ifdef DEBUG_FRAME_DUMP
  nsresult GetFrameName(nsAString& aResult) const override {
    return MakeFrameName(u"BR"_ns, aResult);
  }
#endif

 protected:
  BRFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
      : nsIFrame(aStyle, aPresContext, kClassID),
        mAscent(NS_INTRINSIC_ISIZE_UNKNOWN) {}

  virtual ~BRFrame();

  nscoord mAscent;
};

}  // namespace mozilla

nsIFrame* NS_NewBRFrame(mozilla::PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell) BRFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(BRFrame)

BRFrame::~BRFrame() = default;

void BRFrame::Reflow(nsPresContext* aPresContext, ReflowOutput& aMetrics,
                     const ReflowInput& aReflowInput, nsReflowStatus& aStatus) {
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("BRFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aMetrics, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

  WritingMode wm = aReflowInput.GetWritingMode();
  LogicalSize finalSize(wm);
  finalSize.BSize(wm) = 0;  // BR frames with block size 0 are ignored in quirks
                            // mode by nsLineLayout::VerticalAlignFrames .
                            // However, it's not always 0.  See below.
  finalSize.ISize(wm) = 0;
  aMetrics.SetBlockStartAscent(0);

  // Only when the BR is operating in a line-layout situation will it
  // behave like a BR. Additionally, we suppress breaks from BR inside
  // of ruby frames. To determine if we're inside ruby, we have to rely
  // on the *parent's* ShouldSuppressLineBreak() method, instead of our
  // own, because we may have custom "display" value that makes our
  // ShouldSuppressLineBreak() return false.
  nsLineLayout* ll = aReflowInput.mLineLayout;
  if (ll && !GetParent()->Style()->ShouldSuppressLineBreak()) {
    // Note that the compatibility mode check excludes AlmostStandards
    // mode, since this is the inline box model.  See bug 161691.
    if (ll->LineIsEmpty() ||
        aPresContext->CompatibilityMode() == eCompatibility_FullStandards) {
      // The line is logically empty; any whitespace is trimmed away.
      //
      // If this frame is going to terminate the line we know
      // that nothing else will go on the line. Therefore, in this
      // case, we provide some height for the BR frame so that it
      // creates some vertical whitespace.  It's necessary to use the
      // line-height rather than the font size because the
      // quirks-mode fix that doesn't apply the block's min
      // line-height makes this necessary to make BR cause a line
      // of the full line-height

      // We also do this in strict mode because BR should act like a
      // normal inline frame.  That line-height is used is important
      // here for cases where the line-height is less than 1.
      RefPtr<nsFontMetrics> fm =
          nsLayoutUtils::GetInflatedFontMetricsForFrame(this);
      if (fm) {
        nscoord logicalHeight = aReflowInput.GetLineHeight();
        finalSize.BSize(wm) = logicalHeight;
        aMetrics.SetBlockStartAscent(nsLayoutUtils::GetCenteredFontBaseline(
            fm, logicalHeight, wm.IsLineInverted()));
      } else {
        aMetrics.SetBlockStartAscent(aMetrics.BSize(wm) = 0);
      }

      // XXX temporary until I figure out a better solution; see the
      // code in nsLineLayout::VerticalAlignFrames that zaps minY/maxY
      // if the width is zero.
      // XXX This also fixes bug 10036!
      // Warning: nsTextControlFrame::CalculateSizeStandard depends on
      // the following line, see bug 228752.
      // The code below in AddInlinePrefISize also adds 1 appunit to width
      finalSize.ISize(wm) = 1;
    }

    // Return our reflow status
    aStatus.SetInlineLineBreakAfter(aReflowInput.mStyleDisplay->mClear);
    ll->SetLineEndsInBR(true);
  }

  aMetrics.SetSize(wm, finalSize);
  aMetrics.SetOverflowAreasToDesiredBounds();

  mAscent = aMetrics.BlockStartAscent();
}

/* virtual */
void BRFrame::AddInlineMinISize(gfxContext* aRenderingContext,
                                nsIFrame::InlineMinISizeData* aData) {
  if (!GetParent()->Style()->ShouldSuppressLineBreak()) {
    aData->ForceBreak();
  }
}

/* virtual */
void BRFrame::AddInlinePrefISize(gfxContext* aRenderingContext,
                                 nsIFrame::InlinePrefISizeData* aData) {
  if (!GetParent()->Style()->ShouldSuppressLineBreak()) {
    // Match the 1 appunit width assigned in the Reflow method above
    aData->mCurrentLine += 1;
    aData->ForceBreak();
  }
}

/* virtual */
nscoord BRFrame::GetMinISize(gfxContext* aRenderingContext) {
  nscoord result = 0;
  DISPLAY_MIN_INLINE_SIZE(this, result);
  return result;
}

/* virtual */
nscoord BRFrame::GetPrefISize(gfxContext* aRenderingContext) {
  nscoord result = 0;
  DISPLAY_PREF_INLINE_SIZE(this, result);
  return result;
}

Maybe<nscoord> BRFrame::GetNaturalBaselineBOffset(
    WritingMode aWM, BaselineSharingGroup aBaselineGroup,
    BaselineExportContext) const {
  if (aBaselineGroup == BaselineSharingGroup::Last) {
    return Nothing{};
  }
  return Some(mAscent);
}

nsIFrame::ContentOffsets BRFrame::CalcContentOffsetsFromFramePoint(
    const nsPoint& aPoint) {
  ContentOffsets offsets;
  offsets.content = mContent->GetParent();
  if (offsets.content) {
    offsets.offset = offsets.content->ComputeIndexOf_Deprecated(mContent);
    offsets.secondaryOffset = offsets.offset;
    offsets.associate = CaretAssociationHint::After;
  }
  return offsets;
}

nsIFrame::FrameSearchResult BRFrame::PeekOffsetNoAmount(bool aForward,
                                                        int32_t* aOffset) {
  NS_ASSERTION(aOffset && *aOffset <= 1, "aOffset out of range");
  int32_t startOffset = *aOffset;
  // If we hit the end of a BR going backwards, go to its beginning and stay
  // there.
  if (!aForward && startOffset != 0) {
    *aOffset = 0;
    return FOUND;
  }
  // Otherwise, stop if we hit the beginning, continue (forward) if we hit the
  // end.
  return (startOffset == 0) ? FOUND : CONTINUE;
}

nsIFrame::FrameSearchResult BRFrame::PeekOffsetCharacter(
    bool aForward, int32_t* aOffset, PeekOffsetCharacterOptions aOptions) {
  NS_ASSERTION(aOffset && *aOffset <= 1, "aOffset out of range");
  // Keep going. The actual line jumping will stop us.
  return CONTINUE;
}

nsIFrame::FrameSearchResult BRFrame::PeekOffsetWord(
    bool aForward, bool aWordSelectEatSpace, bool aIsKeyboardSelect,
    int32_t* aOffset, PeekWordState* aState, bool aTrimSpaces) {
  NS_ASSERTION(aOffset && *aOffset <= 1, "aOffset out of range");
  // Keep going. The actual line jumping will stop us.
  return CONTINUE;
}

#ifdef ACCESSIBILITY
a11y::AccType BRFrame::AccessibleType() {
  dom::HTMLBRElement* brElement = dom::HTMLBRElement::FromNode(mContent);
  if (brElement->IsPaddingForEmptyEditor() ||
      brElement->IsPaddingForEmptyLastLine()) {
    // This <br> is a "padding <br> element" used when there is no text or an
    // empty last line in an editor.
    return a11y::eNoType;
  }

  return a11y::eHTMLBRType;
}
#endif

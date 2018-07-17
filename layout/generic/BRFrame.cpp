/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for HTML <br> elements */

#include "gfxContext.h"
#include "nsCOMPtr.h"
#include "nsContainerFrame.h"
#include "nsFontMetrics.h"
#include "nsFrame.h"
#include "nsPresContext.h"
#include "nsLineLayout.h"
#include "nsStyleConsts.h"
#include "nsGkAtoms.h"
#include "nsLayoutUtils.h"

//FOR SELECTION
#include "nsIContent.h"
//END INCLUDES FOR SELECTION

using namespace mozilla;

namespace mozilla {

class BRFrame final : public nsFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS(BRFrame)

  friend nsIFrame* ::NS_NewBRFrame(nsIPresShell* aPresShell, ComputedStyle* aStyle);

  ContentOffsets CalcContentOffsetsFromFramePoint(const nsPoint& aPoint) override;

  virtual FrameSearchResult PeekOffsetNoAmount(bool aForward, int32_t* aOffset) override;
  virtual FrameSearchResult
  PeekOffsetCharacter(bool aForward, int32_t* aOffset,
                      PeekOffsetCharacterOptions aOptions =
                        PeekOffsetCharacterOptions()) override;
  virtual FrameSearchResult PeekOffsetWord(bool aForward, bool aWordSelectEatSpace,
                              bool aIsKeyboardSelect, int32_t* aOffset,
                              PeekWordState* aState) override;

  virtual void Reflow(nsPresContext* aPresContext,
                          ReflowOutput& aDesiredSize,
                          const ReflowInput& aReflowInput,
                          nsReflowStatus& aStatus) override;
  virtual void AddInlineMinISize(gfxContext *aRenderingContext,
                                 InlineMinISizeData *aData) override;
  virtual void AddInlinePrefISize(gfxContext *aRenderingContext,
                                  InlinePrefISizeData *aData) override;
  virtual nscoord GetMinISize(gfxContext *aRenderingContext) override;
  virtual nscoord GetPrefISize(gfxContext *aRenderingContext) override;
  virtual nscoord GetLogicalBaseline(mozilla::WritingMode aWritingMode) const override;

  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    return nsFrame::IsFrameOfType(aFlags & ~(nsIFrame::eReplaced |
                                             nsIFrame::eLineParticipant));
  }

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() override;
#endif

protected:
  explicit BRFrame(ComputedStyle* aStyle)
    : nsFrame(aStyle, kClassID)
    , mAscent(NS_INTRINSIC_WIDTH_UNKNOWN)
  {}

  virtual ~BRFrame();

  nscoord mAscent;
};

} // namespace mozilla

nsIFrame*
NS_NewBRFrame(nsIPresShell* aPresShell, ComputedStyle* aStyle)
{
  return new (aPresShell) BRFrame(aStyle);
}

NS_IMPL_FRAMEARENA_HELPERS(BRFrame)

BRFrame::~BRFrame()
{
}

void
BRFrame::Reflow(nsPresContext* aPresContext,
                ReflowOutput& aMetrics,
                const ReflowInput& aReflowInput,
                nsReflowStatus& aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("BRFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aMetrics, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

  WritingMode wm = aReflowInput.GetWritingMode();
  LogicalSize finalSize(wm);
  finalSize.BSize(wm) = 0; // BR frames with block size 0 are ignored in quirks
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
    if ( ll->LineIsEmpty() ||
         aPresContext->CompatibilityMode() == eCompatibility_FullStandards ) {
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
        nscoord logicalHeight = aReflowInput.CalcLineHeight();
        finalSize.BSize(wm) = logicalHeight;
        aMetrics.SetBlockStartAscent(nsLayoutUtils::GetCenteredFontBaseline(
                                       fm, logicalHeight, wm.IsLineInverted()));
      }
      else {
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
    StyleClear breakType = aReflowInput.mStyleDisplay->mBreakType;
    if (StyleClear::None == breakType) {
      breakType = StyleClear::Line;
    }

    aStatus.SetInlineLineBreakAfter(breakType);
    ll->SetLineEndsInBR(true);
  }

  aMetrics.SetSize(wm, finalSize);
  aMetrics.SetOverflowAreasToDesiredBounds();

  mAscent = aMetrics.BlockStartAscent();

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aMetrics);
}

/* virtual */ void
BRFrame::AddInlineMinISize(gfxContext *aRenderingContext,
                           nsIFrame::InlineMinISizeData *aData)
{
  if (!GetParent()->Style()->ShouldSuppressLineBreak()) {
    aData->ForceBreak();
  }
}

/* virtual */ void
BRFrame::AddInlinePrefISize(gfxContext *aRenderingContext,
                            nsIFrame::InlinePrefISizeData *aData)
{
  if (!GetParent()->Style()->ShouldSuppressLineBreak()) {
    // Match the 1 appunit width assigned in the Reflow method above
    aData->mCurrentLine += 1;
    aData->ForceBreak();
  }
}

/* virtual */ nscoord
BRFrame::GetMinISize(gfxContext *aRenderingContext)
{
  nscoord result = 0;
  DISPLAY_MIN_WIDTH(this, result);
  return result;
}

/* virtual */ nscoord
BRFrame::GetPrefISize(gfxContext *aRenderingContext)
{
  nscoord result = 0;
  DISPLAY_PREF_WIDTH(this, result);
  return result;
}

nscoord
BRFrame::GetLogicalBaseline(mozilla::WritingMode aWritingMode) const
{
  return mAscent;
}

nsIFrame::ContentOffsets BRFrame::CalcContentOffsetsFromFramePoint(const nsPoint& aPoint)
{
  ContentOffsets offsets;
  offsets.content = mContent->GetParent();
  if (offsets.content) {
    offsets.offset = offsets.content->ComputeIndexOf(mContent);
    offsets.secondaryOffset = offsets.offset;
    offsets.associate = CARET_ASSOCIATE_AFTER;
  }
  return offsets;
}

nsIFrame::FrameSearchResult
BRFrame::PeekOffsetNoAmount(bool aForward, int32_t* aOffset)
{
  NS_ASSERTION (aOffset && *aOffset <= 1, "aOffset out of range");
  int32_t startOffset = *aOffset;
  // If we hit the end of a BR going backwards, go to its beginning and stay there.
  if (!aForward && startOffset != 0) {
    *aOffset = 0;
    return FOUND;
  }
  // Otherwise, stop if we hit the beginning, continue (forward) if we hit the end.
  return (startOffset == 0) ? FOUND : CONTINUE;
}

nsIFrame::FrameSearchResult
BRFrame::PeekOffsetCharacter(bool aForward, int32_t* aOffset,
                             PeekOffsetCharacterOptions aOptions)
{
  NS_ASSERTION (aOffset && *aOffset <= 1, "aOffset out of range");
  // Keep going. The actual line jumping will stop us.
  return CONTINUE;
}

nsIFrame::FrameSearchResult
BRFrame::PeekOffsetWord(bool aForward, bool aWordSelectEatSpace, bool aIsKeyboardSelect,
                        int32_t* aOffset, PeekWordState* aState)
{
  NS_ASSERTION (aOffset && *aOffset <= 1, "aOffset out of range");
  // Keep going. The actual line jumping will stop us.
  return CONTINUE;
}

#ifdef ACCESSIBILITY
a11y::AccType
BRFrame::AccessibleType()
{
  nsIContent *parent = mContent->GetParent();
  if (parent && parent->IsRootOfNativeAnonymousSubtree() &&
      parent->GetChildCount() == 1) {
    // This <br> is the only node in a text control, therefore it is the hacky
    // "bogus node" used when there is no text in the control
    return a11y::eNoType;
  }

  // Trailing HTML br element don't play any difference. We don't need to expose
  // it to AT (see bug https://bugzilla.mozilla.org/show_bug.cgi?id=899433#c16
  // for details).
  if (!mContent->GetNextSibling() && !GetNextSibling()) {
    return a11y::eNoType;
  }

  return a11y::eHTMLBRType;
}
#endif


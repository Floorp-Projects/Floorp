/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for HTML <br> elements */

#include "nsCOMPtr.h"
#include "nsFontMetrics.h"
#include "nsFrame.h"
#include "nsPresContext.h"
#include "nsLineLayout.h"
#include "nsStyleConsts.h"
#include "nsGkAtoms.h"
#include "nsRenderingContext.h"
#include "nsLayoutUtils.h"

//FOR SELECTION
#include "nsIContent.h"
//END INCLUDES FOR SELECTION

using namespace mozilla;

class BRFrame : public nsFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewBRFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  virtual ContentOffsets CalcContentOffsetsFromFramePoint(nsPoint aPoint) MOZ_OVERRIDE;

  virtual FrameSearchResult PeekOffsetNoAmount(bool aForward, int32_t* aOffset) MOZ_OVERRIDE;
  virtual FrameSearchResult PeekOffsetCharacter(bool aForward, int32_t* aOffset,
                                     bool aRespectClusters = true) MOZ_OVERRIDE;
  virtual FrameSearchResult PeekOffsetWord(bool aForward, bool aWordSelectEatSpace,
                              bool aIsKeyboardSelect, int32_t* aOffset,
                              PeekWordState* aState) MOZ_OVERRIDE;

  virtual void Reflow(nsPresContext* aPresContext,
                          nsHTMLReflowMetrics& aDesiredSize,
                          const nsHTMLReflowState& aReflowState,
                          nsReflowStatus& aStatus) MOZ_OVERRIDE;
  virtual void AddInlineMinISize(nsRenderingContext *aRenderingContext,
                                 InlineMinISizeData *aData) MOZ_OVERRIDE;
  virtual void AddInlinePrefISize(nsRenderingContext *aRenderingContext,
                                  InlinePrefISizeData *aData) MOZ_OVERRIDE;
  virtual nscoord GetMinISize(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;
  virtual nscoord GetPrefISize(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;
  virtual nscoord GetLogicalBaseline(mozilla::WritingMode aWritingMode) const MOZ_OVERRIDE;

  virtual bool IsFrameOfType(uint32_t aFlags) const MOZ_OVERRIDE
  {
    return nsFrame::IsFrameOfType(aFlags & ~(nsIFrame::eReplaced |
                                             nsIFrame::eLineParticipant));
  }

#ifdef ACCESSIBILITY
  virtual mozilla::a11y::AccType AccessibleType() MOZ_OVERRIDE;
#endif

protected:
  explicit BRFrame(nsStyleContext* aContext) : nsFrame(aContext) {}
  virtual ~BRFrame();

  nscoord mAscent;
};

nsIFrame*
NS_NewBRFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) BRFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(BRFrame)

BRFrame::~BRFrame()
{
}

void
BRFrame::Reflow(nsPresContext* aPresContext,
                nsHTMLReflowMetrics& aMetrics,
                const nsHTMLReflowState& aReflowState,
                nsReflowStatus& aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("BRFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aMetrics, aStatus);
  WritingMode wm = aReflowState.GetWritingMode();
  LogicalSize finalSize(wm);
  finalSize.BSize(wm) = 0; // BR frames with block size 0 are ignored in quirks
                           // mode by nsLineLayout::VerticalAlignFrames .
                           // However, it's not always 0.  See below.
  finalSize.ISize(wm) = 0;
  aMetrics.SetBlockStartAscent(0);

  // Only when the BR is operating in a line-layout situation will it
  // behave like a BR.
  nsLineLayout* ll = aReflowState.mLineLayout;
  if (ll) {
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
      nsRefPtr<nsFontMetrics> fm;
      nsLayoutUtils::GetFontMetricsForFrame(this, getter_AddRefs(fm),
        nsLayoutUtils::FontSizeInflationFor(this));
      aReflowState.rendContext->SetFont(fm); // FIXME: maybe not needed?
      if (fm) {
        nscoord logicalHeight = aReflowState.CalcLineHeight();
        finalSize.BSize(wm) = logicalHeight;
        aMetrics.SetBlockStartAscent(nsLayoutUtils::GetCenteredFontBaseline(fm, logicalHeight));
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
      finalSize.ISize(wm) = 1;
    }

    // Return our reflow status
    uint32_t breakType = aReflowState.mStyleDisplay->mBreakType;
    if (NS_STYLE_CLEAR_NONE == breakType) {
      breakType = NS_STYLE_CLEAR_LINE;
    }

    aStatus = NS_INLINE_BREAK | NS_INLINE_BREAK_AFTER |
      NS_INLINE_MAKE_BREAK_TYPE(breakType);
    ll->SetLineEndsInBR(true);
  }
  else {
    aStatus = NS_FRAME_COMPLETE;
  }

  aMetrics.SetSize(wm, finalSize);
  aMetrics.SetOverflowAreasToDesiredBounds();

  mAscent = aMetrics.BlockStartAscent();

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aMetrics);
}

/* virtual */ void
BRFrame::AddInlineMinISize(nsRenderingContext *aRenderingContext,
                           nsIFrame::InlineMinISizeData *aData)
{
  aData->ForceBreak(aRenderingContext);
}

/* virtual */ void
BRFrame::AddInlinePrefISize(nsRenderingContext *aRenderingContext,
                            nsIFrame::InlinePrefISizeData *aData)
{
  aData->ForceBreak(aRenderingContext);
}

/* virtual */ nscoord
BRFrame::GetMinISize(nsRenderingContext *aRenderingContext)
{
  nscoord result = 0;
  DISPLAY_MIN_WIDTH(this, result);
  return result;
}

/* virtual */ nscoord
BRFrame::GetPrefISize(nsRenderingContext *aRenderingContext)
{
  nscoord result = 0;
  DISPLAY_PREF_WIDTH(this, result);
  return result;
}

nsIAtom*
BRFrame::GetType() const
{
  return nsGkAtoms::brFrame;
}

nscoord
BRFrame::GetLogicalBaseline(mozilla::WritingMode aWritingMode) const
{
  return mAscent;
}

nsIFrame::ContentOffsets BRFrame::CalcContentOffsetsFromFramePoint(nsPoint aPoint)
{
  ContentOffsets offsets;
  offsets.content = mContent->GetParent();
  if (offsets.content) {
    offsets.offset = offsets.content->IndexOf(mContent);
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
                             bool aRespectClusters)
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

  return a11y::eHTMLBRType;
}
#endif


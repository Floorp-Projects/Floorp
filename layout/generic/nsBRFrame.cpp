/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* rendering object for HTML <br> elements */

#include "nsCOMPtr.h"
#include "nsFrame.h"
#include "nsHTMLParts.h"
#include "nsPresContext.h"
#include "nsLineLayout.h"
#include "nsStyleConsts.h"
#include "nsGkAtoms.h"
#include "nsIFontMetrics.h"
#include "nsIRenderingContext.h"
#include "nsLayoutUtils.h"

#ifdef ACCESSIBILITY
#include "nsIServiceManager.h"
#include "nsIAccessibilityService.h"
#endif

//FOR SELECTION
#include "nsIContent.h"
#include "nsFrameSelection.h"
//END INCLUDES FOR SELECTION

#define BR_USING_CENTERED_FONT_BASELINE NS_FRAME_STATE_BIT(63)

class BRFrame : public nsFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewBRFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  virtual ContentOffsets CalcContentOffsetsFromFramePoint(nsPoint aPoint);

  virtual PRBool PeekOffsetNoAmount(PRBool aForward, PRInt32* aOffset);
  virtual PRBool PeekOffsetCharacter(PRBool aForward, PRInt32* aOffset);
  virtual PRBool PeekOffsetWord(PRBool aForward, PRBool aWordSelectEatSpace, PRBool aIsKeyboardSelect,
                                PRInt32* aOffset, PeekWordState* aState);

  NS_IMETHOD Reflow(nsPresContext* aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);
  virtual void AddInlineMinWidth(nsIRenderingContext *aRenderingContext,
                                 InlineMinWidthData *aData);
  virtual void AddInlinePrefWidth(nsIRenderingContext *aRenderingContext,
                                  InlinePrefWidthData *aData);
  virtual nscoord GetMinWidth(nsIRenderingContext *aRenderingContext);
  virtual nscoord GetPrefWidth(nsIRenderingContext *aRenderingContext);
  virtual nsIAtom* GetType() const;
  virtual nscoord GetBaseline() const;

  virtual PRBool IsFrameOfType(PRUint32 aFlags) const
  {
    return nsFrame::IsFrameOfType(aFlags & ~(nsIFrame::eReplaced |
                                             nsIFrame::eLineParticipant));
  }

#ifdef ACCESSIBILITY
  virtual already_AddRefed<nsAccessible> CreateAccessible();
#endif

protected:
  BRFrame(nsStyleContext* aContext) : nsFrame(aContext) {}
  virtual ~BRFrame();
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

NS_IMETHODIMP
BRFrame::Reflow(nsPresContext* aPresContext,
                nsHTMLReflowMetrics& aMetrics,
                const nsHTMLReflowState& aReflowState,
                nsReflowStatus& aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("BRFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aMetrics, aStatus);
  aMetrics.height = 0; // BR frames with height 0 are ignored in quirks
                       // mode by nsLineLayout::VerticalAlignFrames .
                       // However, it's not always 0.  See below.
  aMetrics.width = 0;
  aMetrics.ascent = 0;
  RemoveStateBits(BR_USING_CENTERED_FONT_BASELINE);

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
      nsLayoutUtils::SetFontFromStyle(aReflowState.rendContext, mStyleContext);
      nsCOMPtr<nsIFontMetrics> fm;
      aReflowState.rendContext->GetFontMetrics(*getter_AddRefs(fm));
      if (fm) {
        nscoord logicalHeight = aReflowState.CalcLineHeight();
        aMetrics.height = logicalHeight;
        aMetrics.ascent =
          nsLayoutUtils::GetCenteredFontBaseline(fm, logicalHeight);
        AddStateBits(BR_USING_CENTERED_FONT_BASELINE);
      }
      else {
        aMetrics.ascent = aMetrics.height = 0;
      }

      // XXX temporary until I figure out a better solution; see the
      // code in nsLineLayout::VerticalAlignFrames that zaps minY/maxY
      // if the width is zero.
      // XXX This also fixes bug 10036!
      // Warning: nsTextControlFrame::CalculateSizeStandard depends on
      // the following line, see bug 228752.
      aMetrics.width = 1;
    }

    // Return our reflow status
    PRUint32 breakType = aReflowState.mStyleDisplay->mBreakType;
    if (NS_STYLE_CLEAR_NONE == breakType) {
      breakType = NS_STYLE_CLEAR_LINE;
    }

    aStatus = NS_INLINE_BREAK | NS_INLINE_BREAK_AFTER |
      NS_INLINE_MAKE_BREAK_TYPE(breakType);
    ll->SetLineEndsInBR(PR_TRUE);
  }
  else {
    aStatus = NS_FRAME_COMPLETE;
  }

  aMetrics.SetOverflowAreasToDesiredBounds();

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aMetrics);
  return NS_OK;
}

/* virtual */ void
BRFrame::AddInlineMinWidth(nsIRenderingContext *aRenderingContext,
                           nsIFrame::InlineMinWidthData *aData)
{
  aData->ForceBreak(aRenderingContext);
}

/* virtual */ void
BRFrame::AddInlinePrefWidth(nsIRenderingContext *aRenderingContext,
                            nsIFrame::InlinePrefWidthData *aData)
{
  aData->ForceBreak(aRenderingContext);
}

/* virtual */ nscoord
BRFrame::GetMinWidth(nsIRenderingContext *aRenderingContext)
{
  nscoord result = 0;
  DISPLAY_MIN_WIDTH(this, result);
  return result;
}

/* virtual */ nscoord
BRFrame::GetPrefWidth(nsIRenderingContext *aRenderingContext)
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
BRFrame::GetBaseline() const
{
  nscoord ascent = 0;
  nsCOMPtr<nsIFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(this, getter_AddRefs(fm));
  if (fm) {
    nscoord logicalHeight = GetRect().height;
    if (GetStateBits() & BR_USING_CENTERED_FONT_BASELINE) {
      ascent = nsLayoutUtils::GetCenteredFontBaseline(fm, logicalHeight);
    } else {
      fm->GetMaxAscent(ascent);
      ascent += GetUsedBorderAndPadding().top;
    }
  }
  return ascent;
}

nsIFrame::ContentOffsets BRFrame::CalcContentOffsetsFromFramePoint(nsPoint aPoint)
{
  ContentOffsets offsets;
  offsets.content = mContent->GetParent();
  if (offsets.content) {
    offsets.offset = offsets.content->IndexOf(mContent);
    offsets.secondaryOffset = offsets.offset;
    offsets.associateWithNext = PR_TRUE;
  }
  return offsets;
}

PRBool
BRFrame::PeekOffsetNoAmount(PRBool aForward, PRInt32* aOffset)
{
  NS_ASSERTION (aOffset && *aOffset <= 1, "aOffset out of range");
  PRInt32 startOffset = *aOffset;
  // If we hit the end of a BR going backwards, go to its beginning and stay there.
  if (!aForward && startOffset != 0) {
    *aOffset = 0;
    return PR_TRUE;
  }
  // Otherwise, stop if we hit the beginning, continue (forward) if we hit the end.
  return (startOffset == 0);
}

PRBool
BRFrame::PeekOffsetCharacter(PRBool aForward, PRInt32* aOffset)
{
  NS_ASSERTION (aOffset && *aOffset <= 1, "aOffset out of range");
  // Keep going. The actual line jumping will stop us.
  return PR_FALSE;
}

PRBool
BRFrame::PeekOffsetWord(PRBool aForward, PRBool aWordSelectEatSpace, PRBool aIsKeyboardSelect,
                        PRInt32* aOffset, PeekWordState* aState)
{
  NS_ASSERTION (aOffset && *aOffset <= 1, "aOffset out of range");
  // Keep going. The actual line jumping will stop us.
  return PR_FALSE;
}

#ifdef ACCESSIBILITY
already_AddRefed<nsAccessible>
BRFrame::CreateAccessible()
{
  nsCOMPtr<nsIAccessibilityService> accService = do_GetService("@mozilla.org/accessibilityService;1");
  if (!accService) {
    return nsnull;
  }
  nsIContent *parent = mContent->GetParent();
  if (parent &&
      parent->IsRootOfNativeAnonymousSubtree() &&
      parent->GetChildCount() == 1) {
    // This <br> is the only node in a text control, therefore it is the hacky
    // "bogus node" used when there is no text in the control
    return nsnull;
  }
  return accService->CreateHTMLBRAccessible(mContent,
                                            PresContext()->PresShell());
}
#endif


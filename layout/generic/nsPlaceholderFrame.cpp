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
 *   Mats Palmgren <mats.palmgren@bredband.net>
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

/*
 * rendering object for the point that anchors out-of-flow rendering
 * objects such as floats and absolutely positioned elements
 */

#include "nsPlaceholderFrame.h"
#include "nsLineLayout.h"
#include "nsIContent.h"
#include "nsPresContext.h"
#include "nsIRenderingContext.h"
#include "nsGkAtoms.h"
#include "nsFrameManager.h"
#include "nsDisplayList.h"

nsIFrame*
NS_NewPlaceholderFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsPlaceholderFrame(aContext);
}

nsPlaceholderFrame::~nsPlaceholderFrame()
{
}

/* virtual */ nscoord
nsPlaceholderFrame::GetMinWidth(nsIRenderingContext *aRenderingContext)
{
  nscoord result = 0;
  DISPLAY_MIN_WIDTH(this, result);
  return result;
}

/* virtual */ nscoord
nsPlaceholderFrame::GetPrefWidth(nsIRenderingContext *aRenderingContext)
{
  nscoord result = 0;
  DISPLAY_PREF_WIDTH(this, result);
  return result;
}

/* virtual */ void
nsPlaceholderFrame::AddInlineMinWidth(nsIRenderingContext *aRenderingContext,
                                      nsIFrame::InlineMinWidthData *aData)
{
  // Override AddInlineMinWith so that *nothing* happens.  In
  // particular, we don't want to zero out |aData->trailingWhitespace|,
  // since nsLineLayout skips placeholders when trimming trailing
  // whitespace, and we don't want to set aData->skipWhitespace to
  // false.

  // ...but push floats onto the list
  if (mOutOfFlowFrame->GetStyleDisplay()->mFloats != NS_STYLE_FLOAT_NONE)
    aData->floats.AppendElement(mOutOfFlowFrame);
}

/* virtual */ void
nsPlaceholderFrame::AddInlinePrefWidth(nsIRenderingContext *aRenderingContext,
                                       nsIFrame::InlinePrefWidthData *aData)
{
  // Override AddInlinePrefWith so that *nothing* happens.  In
  // particular, we don't want to zero out |aData->trailingWhitespace|,
  // since nsLineLayout skips placeholders when trimming trailing
  // whitespace, and we don't want to set aData->skipWhitespace to
  // false.

  // ...but push floats onto the list
  if (mOutOfFlowFrame->GetStyleDisplay()->mFloats != NS_STYLE_FLOAT_NONE)
    aData->floats.AppendElement(mOutOfFlowFrame);
}

NS_IMETHODIMP
nsPlaceholderFrame::Reflow(nsPresContext*          aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsPlaceholderFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  aDesiredSize.width = 0;
  aDesiredSize.height = 0;

  aStatus = NS_FRAME_COMPLETE;
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

void
nsPlaceholderFrame::Destroy()
{
  nsIPresShell* shell = PresContext()->GetPresShell();
  if (shell && mOutOfFlowFrame) {
    NS_ASSERTION(!shell->FrameManager()->GetPlaceholderFrameFor(mOutOfFlowFrame),
                 "Placeholder relationship should have been torn down; see "
                 "comments in nsPlaceholderFrame.h");
  }

  nsSplittableFrame::Destroy();
}

nsSplittableType
nsPlaceholderFrame::GetSplittableType() const
{
  NS_ASSERTION(mOutOfFlowFrame, "GetSplittableType called at the wrong time");
  return mOutOfFlowFrame->GetSplittableType();
}

nsIAtom*
nsPlaceholderFrame::GetType() const
{
  return nsGkAtoms::placeholderFrame; 
}

/* virtual */ PRBool
nsPlaceholderFrame::CanContinueTextRun() const
{
  if (!mOutOfFlowFrame) {
    return PR_FALSE;
  }
  // first-letter frames can continue text runs, and placeholders for floated
  // first-letter frames can too
  return mOutOfFlowFrame->CanContinueTextRun();
}

#ifdef DEBUG
static void
PaintDebugPlaceholder(nsIFrame* aFrame, nsIRenderingContext* aCtx,
                      const nsRect& aDirtyRect, nsPoint aPt)
{
  aCtx->SetColor(NS_RGB(0, 255, 255));
  nscoord x = nsPresContext::CSSPixelsToAppUnits(-5);
  aCtx->FillRect(aPt.x + x, aPt.y,
                 nsPresContext::CSSPixelsToAppUnits(13), nsPresContext::CSSPixelsToAppUnits(3));
  nscoord y = nsPresContext::CSSPixelsToAppUnits(-10);
  aCtx->FillRect(aPt.x, aPt.y + y,
                 nsPresContext::CSSPixelsToAppUnits(3), nsPresContext::CSSPixelsToAppUnits(10));
}
#endif // DEBUG

#if defined(DEBUG) || (defined(MOZ_REFLOW_PERF_DSP) && defined(MOZ_REFLOW_PERF))

NS_IMETHODIMP
nsPlaceholderFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                     const nsRect&           aDirtyRect,
                                     const nsDisplayListSet& aLists)
{
  DO_GLOBAL_REFLOW_COUNT_DSP("nsPlaceholderFrame");
  
#ifdef DEBUG
  if (!GetShowFrameBorders())
    return NS_OK;
  
  return aLists.Outlines()->AppendNewToTop(new (aBuilder)
      nsDisplayGeneric(this, PaintDebugPlaceholder, "DebugPlaceholder"));
#else // DEBUG
  return NS_OK;
#endif // DEBUG
}
#endif // DEBUG || (MOZ_REFLOW_PERF_DSP && MOZ_REFLOW_PERF)

#ifdef DEBUG
NS_IMETHODIMP
nsPlaceholderFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Placeholder"), aResult);
}

NS_IMETHODIMP
nsPlaceholderFrame::List(FILE* out, PRInt32 aIndent) const
{
  IndentBy(out, aIndent);
  ListTag(out);
#ifdef DEBUG_waterson
  fprintf(out, " [parent=%p]", NS_STATIC_CAST(void*, mParent));
#endif
  if (HasView()) {
    fprintf(out, " [view=%p]", (void*)GetView());
  }
  fprintf(out, " {%d,%d,%d,%d}", mRect.x, mRect.y, mRect.width, mRect.height);
  if (0 != mState) {
    fprintf(out, " [state=%08x]", mState);
  }
  nsIFrame* prevInFlow = GetPrevInFlow();
  nsIFrame* nextInFlow = GetNextInFlow();
  if (nsnull != prevInFlow) {
    fprintf(out, " prev-in-flow=%p", NS_STATIC_CAST(void*, prevInFlow));
  }
  if (nsnull != nextInFlow) {
    fprintf(out, " next-in-flow=%p", NS_STATIC_CAST(void*, nextInFlow));
  }
  if (mOutOfFlowFrame) {
    fprintf(out, " outOfFlowFrame=");
    nsFrame::ListTag(out, mOutOfFlowFrame);
  }
  fputs("\n", out);
  return NS_OK;
}
#endif

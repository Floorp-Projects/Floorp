/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Steve Clark <buster@netscape.com>
 *   Robert O'Callahan <roc+moz@cs.cmu.edu>
 *   L. David Baron <dbaron@dbaron.org>
 *   IBM Corporation
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
 * rendering object for CSS display:block and display:list-item objects,
 * also used inside table cells
 */

#include "nsCOMPtr.h"
#include "nsBlockFrame.h"
#include "nsBlockReflowContext.h"
#include "nsBlockReflowState.h"
#include "nsBlockBandData.h"
#include "nsBulletFrame.h"
#include "nsLineBox.h"
#include "nsInlineFrame.h"
#include "nsLineLayout.h"
#include "nsPlaceholderFrame.h"
#include "nsStyleConsts.h"
#include "nsFrameManager.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsStyleContext.h"
#include "nsIView.h"
#include "nsIFontMetrics.h"
#include "nsHTMLParts.h"
#include "nsGkAtoms.h"
#include "nsIDOMEvent.h"
#include "nsGenericHTMLElement.h"
#include "prprf.h"
#include "nsStyleChangeList.h"
#include "nsFrameSelection.h"
#include "nsSpaceManager.h"
#include "nsIntervalSet.h"
#include "prenv.h"
#include "plstr.h"
#include "nsGUIEvent.h"
#include "nsLayoutErrors.h"
#include "nsAutoPtr.h"
#include "nsIServiceManager.h"
#include "nsIScrollableFrame.h"
#ifdef ACCESSIBILITY
#include "nsIDOMHTMLDocument.h"
#include "nsIAccessibilityService.h"
#endif
#include "nsLayoutUtils.h"
#include "nsBoxLayoutState.h"
#include "nsDisplayList.h"
#include "nsContentErrors.h"

#ifdef IBMBIDI
#include "nsBidiPresUtils.h"
#endif // IBMBIDI

#include "nsIDOMHTMLBodyElement.h"
#include "nsIDOMHTMLHtmlElement.h"

static const int MIN_LINES_NEEDING_CURSOR = 20;

#ifdef DEBUG
#include "nsPrintfCString.h"
#include "nsBlockDebugFlags.h"

PRBool nsBlockFrame::gLamePaintMetrics;
PRBool nsBlockFrame::gLameReflowMetrics;
PRBool nsBlockFrame::gNoisy;
PRBool nsBlockFrame::gNoisyDamageRepair;
PRBool nsBlockFrame::gNoisyIntrinsic;
PRBool nsBlockFrame::gNoisyReflow;
PRBool nsBlockFrame::gReallyNoisyReflow;
PRBool nsBlockFrame::gNoisySpaceManager;
PRBool nsBlockFrame::gVerifyLines;
PRBool nsBlockFrame::gDisableResizeOpt;

PRInt32 nsBlockFrame::gNoiseIndent;

struct BlockDebugFlags {
  const char* name;
  PRBool* on;
};

static const BlockDebugFlags gFlags[] = {
  { "reflow", &nsBlockFrame::gNoisyReflow },
  { "really-noisy-reflow", &nsBlockFrame::gReallyNoisyReflow },
  { "intrinsic", &nsBlockFrame::gNoisyIntrinsic },
  { "space-manager", &nsBlockFrame::gNoisySpaceManager },
  { "verify-lines", &nsBlockFrame::gVerifyLines },
  { "damage-repair", &nsBlockFrame::gNoisyDamageRepair },
  { "lame-paint-metrics", &nsBlockFrame::gLamePaintMetrics },
  { "lame-reflow-metrics", &nsBlockFrame::gLameReflowMetrics },
  { "disable-resize-opt", &nsBlockFrame::gDisableResizeOpt },
};
#define NUM_DEBUG_FLAGS (sizeof(gFlags) / sizeof(gFlags[0]))

static void
ShowDebugFlags()
{
  printf("Here are the available GECKO_BLOCK_DEBUG_FLAGS:\n");
  const BlockDebugFlags* bdf = gFlags;
  const BlockDebugFlags* end = gFlags + NUM_DEBUG_FLAGS;
  for (; bdf < end; bdf++) {
    printf("  %s\n", bdf->name);
  }
  printf("Note: GECKO_BLOCK_DEBUG_FLAGS is a comma separated list of flag\n");
  printf("names (no whitespace)\n");
}

void
nsBlockFrame::InitDebugFlags()
{
  static PRBool firstTime = PR_TRUE;
  if (firstTime) {
    firstTime = PR_FALSE;
    char* flags = PR_GetEnv("GECKO_BLOCK_DEBUG_FLAGS");
    if (flags) {
      PRBool error = PR_FALSE;
      for (;;) {
        char* cm = PL_strchr(flags, ',');
        if (cm) *cm = '\0';

        PRBool found = PR_FALSE;
        const BlockDebugFlags* bdf = gFlags;
        const BlockDebugFlags* end = gFlags + NUM_DEBUG_FLAGS;
        for (; bdf < end; bdf++) {
          if (PL_strcasecmp(bdf->name, flags) == 0) {
            *(bdf->on) = PR_TRUE;
            printf("nsBlockFrame: setting %s debug flag on\n", bdf->name);
            gNoisy = PR_TRUE;
            found = PR_TRUE;
            break;
          }
        }
        if (!found) {
          error = PR_TRUE;
        }

        if (!cm) break;
        *cm = ',';
        flags = cm + 1;
      }
      if (error) {
        ShowDebugFlags();
      }
    }
  }
}

#endif

// add in a sanity check for absurdly deep frame trees.  See bug 42138
// can't just use IsFrameTreeTooDeep() because that method has side effects we don't want
#define MAX_DEPTH_FOR_LIST_RENUMBERING 200  // 200 open displayable tags is pretty unrealistic

//----------------------------------------------------------------------

// Debugging support code

#ifdef DEBUG
const char* nsBlockFrame::kReflowCommandType[] = {
  "ContentChanged",
  "StyleChanged",
  "ReflowDirty",
  "Timeout",
  "UserDefined",
};
#endif

#ifdef REALLY_NOISY_FIRST_LINE
static void
DumpStyleGeneaology(nsIFrame* aFrame, const char* gap)
{
  fputs(gap, stdout);
  nsFrame::ListTag(stdout, aFrame);
  printf(": ");
  nsStyleContext* sc = aFrame->GetStyleContext();
  while (nsnull != sc) {
    nsStyleContext* psc;
    printf("%p ", sc);
    psc = sc->GetParent();
    sc = psc;
  }
  printf("\n");
}
#endif

#ifdef REFLOW_STATUS_COVERAGE
static void
RecordReflowStatus(PRBool aChildIsBlock, nsReflowStatus aFrameReflowStatus)
{
  static PRUint32 record[2];

  // 0: child-is-block
  // 1: child-is-inline
  PRIntn index = 0;
  if (!aChildIsBlock) index |= 1;

  // Compute new status
  PRUint32 newS = record[index];
  if (NS_INLINE_IS_BREAK(aFrameReflowStatus)) {
    if (NS_INLINE_IS_BREAK_BEFORE(aFrameReflowStatus)) {
      newS |= 1;
    }
    else if (NS_FRAME_IS_NOT_COMPLETE(aFrameReflowStatus)) {
      newS |= 2;
    }
    else {
      newS |= 4;
    }
  }
  else if (NS_FRAME_IS_NOT_COMPLETE(aFrameReflowStatus)) {
    newS |= 8;
  }
  else {
    newS |= 16;
  }

  // Log updates to the status that yield different values
  if (record[index] != newS) {
    record[index] = newS;
    printf("record(%d): %02x %02x\n", index, record[0], record[1]);
  }
}
#endif

//----------------------------------------------------------------------

const nsIID kBlockFrameCID = NS_BLOCK_FRAME_CID;

nsIFrame*
NS_NewBlockFrame(nsIPresShell* aPresShell, nsStyleContext* aContext, PRUint32 aFlags)
{
  nsBlockFrame* it = new (aPresShell) nsBlockFrame(aContext);
  if (it) {
    it->SetFlags(aFlags);
  }
  return it;
}

nsBlockFrame::~nsBlockFrame()
{
}

void
nsBlockFrame::Destroy()
{
  mAbsoluteContainer.DestroyFrames(this);
  // Outside bullets are not in our child-list so check for them here
  // and delete them when present.
  if (mBullet && HaveOutsideBullet()) {
    mBullet->Destroy();
    mBullet = nsnull;
  }

  mFloats.DestroyFrames();
  
  nsPresContext* presContext = GetPresContext();

  nsLineBox::DeleteLineList(presContext, mLines);

  // destroy overflow lines now
  nsLineList* overflowLines = RemoveOverflowLines();
  if (overflowLines) {
    nsLineBox::DeleteLineList(presContext, *overflowLines);
  }

  {
    nsAutoOOFFrameList oofs(this);
    oofs.mList.DestroyFrames();
    // oofs is now empty and will remove the frame list property
  }

  nsBlockFrameSuper::Destroy();
}

NS_IMETHODIMP
nsBlockFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(aInstancePtr, "null out param");
  if (aIID.Equals(kBlockFrameCID)) {
    *aInstancePtr = NS_STATIC_CAST(void*, NS_STATIC_CAST(nsBlockFrame*, this));
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsILineIterator)) ||
      aIID.Equals(NS_GET_IID(nsILineIteratorNavigator)))
  {
    nsLineIterator* it = new nsLineIterator;
    if (!it) {
      *aInstancePtr = nsnull;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(it); // reference passed to caller
    const nsStyleVisibility* visibility = GetStyleVisibility();
    nsresult rv = it->Init(mLines,
                           visibility->mDirection == NS_STYLE_DIRECTION_RTL);
    if (NS_FAILED(rv)) {
      NS_RELEASE(it);
      return rv;
    }
    *aInstancePtr = NS_STATIC_CAST(void*,
            NS_STATIC_CAST(nsILineIteratorNavigator*, it));
    return NS_OK;
  }
  return nsBlockFrameSuper::QueryInterface(aIID, aInstancePtr);
}

nsSplittableType
nsBlockFrame::GetSplittableType() const
{
  return NS_FRAME_SPLITTABLE_NON_RECTANGULAR;
}

#ifdef DEBUG
NS_METHOD
nsBlockFrame::List(FILE* out, PRInt32 aIndent) const
{
  IndentBy(out, aIndent);
  ListTag(out);
#ifdef DEBUG_waterson
  fprintf(out, " [parent=%p]", mParent);
#endif
  if (HasView()) {
    fprintf(out, " [view=%p]", NS_STATIC_CAST(void*, GetView()));
  }
  if (nsnull != mNextSibling) {
    fprintf(out, " next=%p", NS_STATIC_CAST(void*, mNextSibling));
  }

  // Output the flow linkage
  if (nsnull != GetPrevInFlow()) {
    fprintf(out, " prev-in-flow=%p", NS_STATIC_CAST(void*, GetPrevInFlow()));
  }
  if (nsnull != GetNextInFlow()) {
    fprintf(out, " next-in-flow=%p", NS_STATIC_CAST(void*, GetNextInFlow()));
  }

  // Output the rect and state
  fprintf(out, " {%d,%d,%d,%d}", mRect.x, mRect.y, mRect.width, mRect.height);
  if (0 != mState) {
    fprintf(out, " [state=%08x]", mState);
  }
  nsBlockFrame* f = NS_CONST_CAST(nsBlockFrame*, this);
  nsRect* overflowArea = f->GetOverflowAreaProperty(PR_FALSE);
  if (overflowArea) {
    fprintf(out, " [overflow=%d,%d,%d,%d]", overflowArea->x, overflowArea->y,
            overflowArea->width, overflowArea->height);
  }
  PRInt32 numInlineLines = 0;
  PRInt32 numBlockLines = 0;
  if (! mLines.empty()) {
    for (const_line_iterator line = begin_lines(), line_end = end_lines();
         line != line_end;
         ++line)
    {
      if (line->IsBlock())
        numBlockLines++;
      else
        numInlineLines++;
    }
  }
  fprintf(out, " sc=%p(i=%d,b=%d)",
          NS_STATIC_CAST(void*, mStyleContext), numInlineLines, numBlockLines);
  nsIAtom* pseudoTag = mStyleContext->GetPseudoType();
  if (pseudoTag) {
    nsAutoString atomString;
    pseudoTag->ToString(atomString);
    fprintf(out, " pst=%s",
            NS_LossyConvertUTF16toASCII(atomString).get());
  }
  fputs("<\n", out);

  aIndent++;

  // Output the lines
  if (! mLines.empty()) {
    for (const_line_iterator line = begin_lines(), line_end = end_lines();
         line != line_end;
         ++line)
    {
      line->List(out, aIndent);
    }
  }

  nsIAtom* listName = nsnull;
  PRInt32 listIndex = 0;
  for (;;) {
    listName = GetAdditionalChildListName(listIndex++);
    if (nsnull == listName) {
      break;
    }
    nsIFrame* kid = GetFirstChild(listName);
    if (kid) {
      IndentBy(out, aIndent);
      nsAutoString tmp;
      if (nsnull != listName) {
        listName->ToString(tmp);
        fputs(NS_LossyConvertUTF16toASCII(tmp).get(), out);
      }
      fputs("<\n", out);
      while (kid) {
        nsIFrameDebug*  frameDebug;

        if (NS_SUCCEEDED(CallQueryInterface(kid, &frameDebug))) {
          frameDebug->List(out, aIndent + 1);
        }
        kid = kid->GetNextSibling();
      }
      IndentBy(out, aIndent);
      fputs(">\n", out);
    }
  }

  aIndent--;
  IndentBy(out, aIndent);
  fputs(">\n", out);

  return NS_OK;
}

NS_IMETHODIMP_(nsFrameState)
nsBlockFrame::GetDebugStateBits() const
{
  // We don't want to include our cursor flag in the bits the
  // regression tester looks at
  return nsBlockFrameSuper::GetDebugStateBits() & ~NS_BLOCK_HAS_LINE_CURSOR;
}

NS_IMETHODIMP
nsBlockFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Block"), aResult);
}
#endif

nsIAtom*
nsBlockFrame::GetType() const
{
  return nsGkAtoms::blockFrame;
}

void
nsBlockFrame::InvalidateInternal(const nsRect& aDamageRect,
                                 nscoord aX, nscoord aY, nsIFrame* aForChild,
                                 PRBool aImmediate)
{
  // Optimize by suppressing invalidation of areas that are clipped out
  // with CSS 'clip'.
  const nsStyleDisplay* disp = GetStyleDisplay();
  nsRect absPosClipRect;
  if (GetAbsPosClipRect(disp, &absPosClipRect)) {
    // Restrict the invalidated area to abs-pos clip rect
    // abs-pos clipping clips everything in the frame
    nsRect r;
    if (r.IntersectRect(aDamageRect, absPosClipRect - nsPoint(aX, aY))) {
      nsBlockFrameSuper::InvalidateInternal(r, aX, aY, aForChild, aImmediate);
    }
    return;
  }

  nsBlockFrameSuper::InvalidateInternal(aDamageRect, aX, aY, aForChild, aImmediate);
}

nscoord
nsBlockFrame::GetBaseline() const
{
  NS_ASSERTION(!(GetStateBits() & (NS_FRAME_IS_DIRTY |
                                   NS_FRAME_HAS_DIRTY_CHILDREN)),
               "frame must not be dirty");
  nscoord result;
  if (nsLayoutUtils::GetLastLineBaseline(this, &result))
    return result;
  return nsFrame::GetBaseline();
}

/////////////////////////////////////////////////////////////////////////////
// Child frame enumeration

nsIFrame*
nsBlockFrame::GetFirstChild(nsIAtom* aListName) const
{
  if (mAbsoluteContainer.GetChildListName() == aListName) {
    nsIFrame* result = nsnull;
    mAbsoluteContainer.FirstChild(this, aListName, &result);
    return result;
  }
  else if (nsnull == aListName) {
    return (mLines.empty()) ? nsnull : mLines.front()->mFirstChild;
  }
  else if (aListName == nsGkAtoms::overflowList) {
    nsLineList* overflowLines = GetOverflowLines();
    return overflowLines ? overflowLines->front()->mFirstChild : nsnull;
  }
  else if (aListName == nsGkAtoms::overflowOutOfFlowList) {
    return GetOverflowOutOfFlows().FirstChild();
  }
  else if (aListName == nsGkAtoms::floatList) {
    return mFloats.FirstChild();
  }
  else if (aListName == nsGkAtoms::bulletList) {
    if (HaveOutsideBullet()) {
      return mBullet;
    }
  }
  return nsnull;
}

nsIAtom*
nsBlockFrame::GetAdditionalChildListName(PRInt32 aIndex) const
{
  switch (aIndex) {
  case NS_BLOCK_FRAME_FLOAT_LIST_INDEX:
    return nsGkAtoms::floatList;
  case NS_BLOCK_FRAME_BULLET_LIST_INDEX:
    return nsGkAtoms::bulletList;
  case NS_BLOCK_FRAME_OVERFLOW_LIST_INDEX:
    return nsGkAtoms::overflowList;
  case NS_BLOCK_FRAME_OVERFLOW_OOF_LIST_INDEX:
    return nsGkAtoms::overflowOutOfFlowList;
  case NS_BLOCK_FRAME_ABSOLUTE_LIST_INDEX:
    return mAbsoluteContainer.GetChildListName();
  default:
    return nsnull;
  }
}

/* virtual */ PRBool
nsBlockFrame::IsContainingBlock() const
{
  return PR_TRUE;
}

/* virtual */ PRBool
nsBlockFrame::IsFloatContainingBlock() const
{
  return PR_TRUE;
}

static PRBool IsContinuationPlaceholder(nsIFrame* aFrame)
{
  return aFrame->GetPrevInFlow() &&
    nsGkAtoms::placeholderFrame == aFrame->GetType();
}

static void ReparentFrame(nsIFrame* aFrame, nsIFrame* aOldParent,
                          nsIFrame* aNewParent) {
  NS_ASSERTION(aOldParent == aFrame->GetParent(),
               "Parent not consistent with exepectations");

  aFrame->SetParent(aNewParent);

  // When pushing and pulling frames we need to check for whether any
  // views need to be reparented
  nsHTMLContainerFrame::ReparentFrameView(aFrame->GetPresContext(), aFrame,
                                          aOldParent, aNewParent);
}
 
//////////////////////////////////////////////////////////////////////
// Frame structure methods

//////////////////////////////////////////////////////////////////////
// Reflow methods

/* virtual */ void
nsBlockFrame::MarkIntrinsicWidthsDirty()
{
  mMinWidth = NS_INTRINSIC_WIDTH_UNKNOWN;
  mPrefWidth = NS_INTRINSIC_WIDTH_UNKNOWN;

  nsBlockFrameSuper::MarkIntrinsicWidthsDirty();
}

/* virtual */ nscoord
nsBlockFrame::GetMinWidth(nsIRenderingContext *aRenderingContext)
{
  DISPLAY_MIN_WIDTH(this, mMinWidth);
  if (mMinWidth != NS_INTRINSIC_WIDTH_UNKNOWN)
    return mMinWidth;

#ifdef DEBUG
  if (gNoisyIntrinsic) {
    IndentBy(stdout, gNoiseIndent);
    ListTag(stdout);
    printf(": GetMinWidth\n");
  }
  AutoNoisyIndenter indent(gNoisyIntrinsic);
#endif

  PRInt32 lineNumber = 0;
  InlineMinWidthData data;
  for (line_iterator line = begin_lines(), line_end = end_lines();
       line != line_end; ++line, ++lineNumber)
  {
#ifdef DEBUG
    if (gNoisyIntrinsic) {
      IndentBy(stdout, gNoiseIndent);
      printf("line %d (%s%s)\n", lineNumber,
             line->IsBlock() ? "block" : "inline",
             line->IsEmpty() ? ",empty" : "");
    }
    AutoNoisyIndenter lineindent(gNoisyIntrinsic);
#endif
    if (line->IsBlock()) {
      data.Break(aRenderingContext);
      data.currentLine = nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
          line->mFirstChild, nsLayoutUtils::MIN_WIDTH);
      data.Break(aRenderingContext);
    } else {

      nsIFrame *kid = line->mFirstChild;
      for (PRInt32 i = 0, i_end = line->GetChildCount(); i != i_end;
           ++i, kid = kid->GetNextSibling()) {
        kid->AddInlineMinWidth(aRenderingContext, &data);
      }
    }
#ifdef DEBUG
    if (gNoisyIntrinsic) {
      IndentBy(stdout, gNoiseIndent);
      printf("min: [prevLines=%d currentLine=%d]\n",
             data.prevLines, data.currentLine);
    }
#endif
  }
  data.Break(aRenderingContext);

  mMinWidth = data.prevLines;
  return mMinWidth;
}

/* virtual */ nscoord
nsBlockFrame::GetPrefWidth(nsIRenderingContext *aRenderingContext)
{
  DISPLAY_PREF_WIDTH(this, mPrefWidth);
  if (mPrefWidth != NS_INTRINSIC_WIDTH_UNKNOWN)
    return mPrefWidth;

#ifdef DEBUG
  if (gNoisyIntrinsic) {
    IndentBy(stdout, gNoiseIndent);
    ListTag(stdout);
    printf(": GetPrefWidth\n");
  }
  AutoNoisyIndenter indent(gNoisyIntrinsic);
#endif

  PRInt32 lineNumber = 0;
  InlinePrefWidthData data;
  for (line_iterator line = begin_lines(), line_end = end_lines();
       line != line_end; ++line, ++lineNumber)
  {
#ifdef DEBUG
    if (gNoisyIntrinsic) {
      IndentBy(stdout, gNoiseIndent);
      printf("line %d (%s%s)\n", lineNumber,
             line->IsBlock() ? "block" : "inline",
             line->IsEmpty() ? ",empty" : "");
    }
    AutoNoisyIndenter lineindent(gNoisyIntrinsic);
#endif
    if (line->IsBlock()) {
      data.Break(aRenderingContext);
      data.currentLine = nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                      line->mFirstChild, nsLayoutUtils::PREF_WIDTH);
      data.Break(aRenderingContext);
    } else {

      nsIFrame *kid = line->mFirstChild;
      for (PRInt32 i = 0, i_end = line->GetChildCount(); i != i_end;
           ++i, kid = kid->GetNextSibling()) {
        kid->AddInlinePrefWidth(aRenderingContext, &data);
      }
    }
#ifdef DEBUG
    if (gNoisyIntrinsic) {
      IndentBy(stdout, gNoiseIndent);
      printf("pref: [prevLines=%d currentLine=%d]\n",
             data.prevLines, data.currentLine);
    }
#endif
  }
  data.Break(aRenderingContext);

  mPrefWidth = data.prevLines;
  return mPrefWidth;
}

static nsSize
CalculateContainingBlockSizeForAbsolutes(const nsHTMLReflowState& aReflowState,
                                         nsSize aFrameSize)
{
  // The issue here is that for a 'height' of 'auto' the reflow state
  // code won't know how to calculate the containing block height
  // because it's calculated bottom up. So we use our own computed
  // size as the dimensions. We don't really want to do this for the
  // initial containing block
  nsIFrame* frame = aReflowState.frame;
  if (nsLayoutUtils::IsInitialContainingBlock(frame)) {
    return nsSize(-1, -1);
  }

  nsSize cbSize(aFrameSize);
    // Containing block is relative to the padding edge
  const nsMargin& border = aReflowState.mStyleBorder->GetBorder();
  cbSize.width -= border.left + border.right;
  cbSize.height -= border.top + border.bottom;

  if (frame->GetParent()->GetContent() == frame->GetContent()) {
    // We are a wrapped frame for the content. Use the container's
    // dimensions, if they have been precomputed.
    // XXX This is a hack! We really should be waiting until the outermost
    // frame is fully reflowed and using the resulting dimensions, even
    // if they're intrinsic.
    // In fact we should be attaching absolute children to the outermost
    // frame and not always sticking them in block frames.

    // First, find the reflow state for the outermost frame for this
    // content.
    const nsHTMLReflowState* aLastRS = &aReflowState;
    const nsHTMLReflowState* lastButOneRS = &aReflowState;
    while (aLastRS->parentReflowState &&
           aLastRS->parentReflowState->frame->GetContent() == frame->GetContent()) {
      lastButOneRS = aLastRS;
      aLastRS = aLastRS->parentReflowState;
    }
    if (aLastRS != &aReflowState) {
      // The wrapper frame should be block-level. If it isn't, how the
      // heck did it end up wrapping this block frame?
      NS_ASSERTION(aLastRS->frame->GetStyleDisplay()->IsBlockLevel(),
                   "Wrapping frame should be block-level");
      // Scrollbars need to be specifically excluded, if present, because they are outside the
      // padding-edge. We need better APIs for getting the various boxes from a frame.
      nsIScrollableFrame* scrollFrame;
      CallQueryInterface(aLastRS->frame, &scrollFrame);
      nsMargin scrollbars(0,0,0,0);
      if (scrollFrame) {
        nsBoxLayoutState dummyState(aLastRS->frame->GetPresContext(),
                                    aLastRS->rendContext);
        scrollbars = scrollFrame->GetDesiredScrollbarSizes(&dummyState);
        // XXX We should account for the horizontal scrollbar too --- but currently
        // nsGfxScrollFrame assumes nothing depends on the presence (or absence) of
        // a horizontal scrollbar, so accounting for it would create incremental
        // reflow bugs.
        //if (!lastButOneRS->mFlags.mAssumingHScrollbar) {
          scrollbars.top = scrollbars.bottom = 0;
        //}
        if (!lastButOneRS->mFlags.mAssumingVScrollbar) {
          scrollbars.left = scrollbars.right = 0;
        }
      }
      // We found a reflow state for the outermost wrapping frame, so use
      // its computed metrics if available
      if (aLastRS->ComputedWidth() != NS_UNCONSTRAINEDSIZE) {
        cbSize.width = PR_MAX(0,
          aLastRS->ComputedWidth() + aLastRS->mComputedPadding.LeftRight() - scrollbars.LeftRight());
      }
      if (aLastRS->mComputedHeight != NS_UNCONSTRAINEDSIZE) {
        cbSize.height = PR_MAX(0,
          aLastRS->mComputedHeight + aLastRS->mComputedPadding.TopBottom() - scrollbars.TopBottom());
      }
    }
  }

  return cbSize;
}

NS_IMETHODIMP
nsBlockFrame::Reflow(nsPresContext*          aPresContext,
                     nsHTMLReflowMetrics&     aMetrics,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsBlockFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aMetrics, aStatus);
#ifdef DEBUG
  if (gNoisyReflow) {
    IndentBy(stdout, gNoiseIndent);
    ListTag(stdout);
    printf(": begin reflow availSize=%d,%d computedSize=%d,%d\n",
           aReflowState.availableWidth, aReflowState.availableHeight,
           aReflowState.ComputedWidth(), aReflowState.mComputedHeight);
  }
  AutoNoisyIndenter indent(gNoisy);
  PRTime start = LL_ZERO; // Initialize these variablies to silence the compiler.
  PRInt32 ctc = 0;        // We only use these if they are set (gLameReflowMetrics).
  if (gLameReflowMetrics) {
    start = PR_Now();
    ctc = nsLineBox::GetCtorCount();
  }
#endif

  nsSize oldSize = GetSize();

  // Should we create a space manager?
  nsAutoSpaceManager autoSpaceManager(NS_CONST_CAST(nsHTMLReflowState &, aReflowState));

  // XXXldb If we start storing the space manager in the frame rather
  // than keeping it around only during reflow then we should create it
  // only when there are actually floats to manage.  Otherwise things
  // like tables will gain significant bloat.
  if (NS_BLOCK_SPACE_MGR & mState)
    autoSpaceManager.CreateSpaceManagerFor(aPresContext, this);

  // OK, some lines may be reflowed. Blow away any saved line cursor because
  // we may invalidate the nondecreasing combinedArea.y/yMost invariant,
  // and we may even delete the line with the line cursor.
  ClearLineCursor();

  if (IsFrameTreeTooDeep(aReflowState, aMetrics)) {
#ifdef DEBUG_kipp
    {
      extern char* nsPresShell_ReflowStackPointerTop;
      char marker;
      char* newsp = (char*) &marker;
      printf("XXX: frame tree is too deep; approx stack size = %d\n",
             nsPresShell_ReflowStackPointerTop - newsp);
    }
#endif
    aStatus = NS_FRAME_COMPLETE;
    return NS_OK;
  }

  nsBlockReflowState state(aReflowState, aPresContext, this, aMetrics,
                           (NS_BLOCK_MARGIN_ROOT & mState),
                           (NS_BLOCK_MARGIN_ROOT & mState));

#ifdef IBMBIDI
  if (! mLines.empty()) {
    if (aPresContext->BidiEnabled()) {
      nsBidiPresUtils* bidiUtils = aPresContext->GetBidiUtils();
      if (bidiUtils) {
        bidiUtils->Resolve(aPresContext, this,
                           mLines.front()->mFirstChild,
                           aReflowState.mFlags.mVisualBidiFormControl);
      }
    }
  }
#endif // IBMBIDI

  RenumberLists(aPresContext);

  nsresult rv = NS_OK;

  // ALWAYS drain overflow. We never want to leave the previnflow's
  // overflow lines hanging around; block reflow depends on the
  // overflow line lists being cleared out between reflow passes.
  DrainOverflowLines(state);
  state.SetupOverflowPlaceholdersProperty();
 
  // If we're not dirty (which means we'll mark everything dirty later)
  // and our width has changed, mark the lines dirty that we need to
  // mark dirty for a resize reflow.
  if (aReflowState.mFlags.mHResize)
    PrepareResizeReflow(state);

  mState &= ~NS_FRAME_FIRST_REFLOW;

  // Now reflow...
  rv = ReflowDirtyLines(state);
  NS_ASSERTION(NS_SUCCEEDED(rv), "reflow dirty lines failed");
  if (NS_FAILED(rv)) return rv;

  // If the block is complete, put continuted floats in the closest ancestor 
  // block that uses the same space manager and leave the block complete; this 
  // allows subsequent lines on the page to be impacted by floats. If the 
  // block is incomplete or there is no ancestor using the same space manager, 
  // put continued floats at the beginning of the first overflow line.
  if (state.mOverflowPlaceholders.NotEmpty()) {
    NS_ASSERTION(aReflowState.availableHeight != NS_UNCONSTRAINEDSIZE,
                 "Somehow we failed to fit all content, even though we have unlimited space!");
    if (NS_FRAME_IS_COMPLETE(state.mReflowStatus)) {
      // find the nearest block ancestor that uses the same space manager
      for (const nsHTMLReflowState* ancestorRS = aReflowState.parentReflowState; 
           ancestorRS; 
           ancestorRS = ancestorRS->parentReflowState) {
        nsIFrame* ancestor = ancestorRS->frame;
        nsIAtom* fType = ancestor->GetType();
        if ((nsGkAtoms::blockFrame == fType || nsGkAtoms::areaFrame == fType) &&
            aReflowState.mSpaceManager == ancestorRS->mSpaceManager) {
          // Put the continued floats in ancestor since it uses the same space manager
          nsFrameList* ancestorPlace =
            ((nsBlockFrame*)ancestor)->GetOverflowPlaceholders();
          // The ancestor should have this list, since it's being reflowed. But maybe
          // it isn't because of reflow roots or something.
          if (ancestorPlace) {
            for (nsIFrame* f = state.mOverflowPlaceholders.FirstChild();
                 f; f = f->GetNextSibling()) {
              NS_ASSERTION(IsContinuationPlaceholder(f),
                           "Overflow placeholders must be continuation placeholders");
              ReparentFrame(f, this, ancestorRS->frame);
              nsIFrame* oof = nsPlaceholderFrame::GetRealFrameForPlaceholder(f);
              mFloats.RemoveFrame(oof);
              ReparentFrame(oof, this, ancestorRS->frame);
              // Clear the next-sibling in case the frame wasn't in mFloats
              oof->SetNextSibling(nsnull);
              // Do not put the float into any child frame list, because
              // placeholders in the overflow-placeholder block-state list
              // don't keep their out of flows in a child frame list.
            }
            ancestorPlace->AppendFrames(nsnull, state.mOverflowPlaceholders.FirstChild());
            state.mOverflowPlaceholders.SetFrames(nsnull);
            break;
          }
        }
      }
    }
    if (!state.mOverflowPlaceholders.IsEmpty()) {
      state.mOverflowPlaceholders.SortByContentOrder();
      PRInt32 numOverflowPlace = state.mOverflowPlaceholders.GetLength();
      nsLineBox* newLine =
        state.NewLineBox(state.mOverflowPlaceholders.FirstChild(),
                         numOverflowPlace, PR_FALSE);
      if (newLine) {
        nsLineList* overflowLines = GetOverflowLines();
        if (overflowLines) {
          // Need to put the overflow placeholders' floats into our
          // overflow-out-of-flows list, since the overflow placeholders are
          // going onto our overflow line list. Put them last, because that's
          // where the placeholders are going.
          nsFrameList floats;
          nsIFrame* lastFloat = nsnull;
          for (nsIFrame* f = state.mOverflowPlaceholders.FirstChild();
               f; f = f->GetNextSibling()) {
            NS_ASSERTION(IsContinuationPlaceholder(f),
                         "Overflow placeholders must be continuation placeholders");
            nsIFrame* oof = nsPlaceholderFrame::GetRealFrameForPlaceholder(f);
            // oof is not currently in any child list
            floats.InsertFrames(nsnull, lastFloat, oof);
            lastFloat = oof;
          }

          // Put the new placeholders *last* in the overflow lines
          // because they might have previnflows in the overflow lines.
          nsIFrame* lastChild = overflowLines->back()->LastChild();
          lastChild->SetNextSibling(state.mOverflowPlaceholders.FirstChild());
          // Create a new line as the last line and put the
          // placeholders there
          overflowLines->push_back(newLine);

          nsAutoOOFFrameList oofs(this);
          oofs.mList.AppendFrames(nsnull, floats.FirstChild());
        }
        else {
          mLines.push_back(newLine);
          nsLineList::iterator nextToLastLine = ----end_lines();
          PushLines(state, nextToLastLine);
        }
        state.mOverflowPlaceholders.SetFrames(nsnull);
      }
      state.mReflowStatus |= NS_FRAME_NOT_COMPLETE | NS_FRAME_REFLOW_NEXTINFLOW;
    }
  }

  if (NS_FRAME_IS_NOT_COMPLETE(state.mReflowStatus)) {
    if (GetOverflowLines()) {
      state.mReflowStatus |= NS_FRAME_REFLOW_NEXTINFLOW;
    }

    if (NS_STYLE_OVERFLOW_CLIP == aReflowState.mStyleDisplay->mOverflowX) {
      state.mReflowStatus = NS_FRAME_COMPLETE;
    }
    else {
#ifdef DEBUG_kipp
      ListTag(stdout); printf(": block is not complete\n");
#endif
    }
  }

  CheckFloats(state);

  // Place the "marker" (bullet) frame if it is placed next to a block
  // child.
  //
  // According to the CSS2 spec, section 12.6.1, the "marker" box
  // participates in the height calculation of the list-item box's
  // first line box.
  //
  // There are exactly two places a bullet can be placed: near the
  // first or second line. It's only placed on the second line in a
  // rare case: an empty first line followed by a second line that
  // contains a block (example: <LI>\n<P>... ). This is where
  // the second case can happen.
  if (mBullet && HaveOutsideBullet() &&
      (mLines.empty() ||
       mLines.front()->IsBlock() ||
       0 == mLines.front()->mBounds.height)) {
    // Reflow the bullet
    nsHTMLReflowMetrics metrics;
    ReflowBullet(state, metrics);

    nscoord baseline;
    if (!nsLayoutUtils::GetFirstLineBaseline(this, &baseline)) {
      baseline = 0;
    }
    
    // Doing the alignment using the baseline will also cater for
    // bullets that are placed next to a child block (bug 92896)
    
    // Tall bullets won't look particularly nice here...
    nsRect bbox = mBullet->GetRect();
    bbox.y = baseline - metrics.ascent;
    mBullet->SetRect(bbox);
  }

  // Compute our final size
  ComputeFinalSize(aReflowState, state, aMetrics);

  ComputeCombinedArea(aReflowState, aMetrics);

  // see if verifyReflow is enabled, and if so store off the space manager pointer
#ifdef DEBUG
  PRInt32 verifyReflowFlags = nsIPresShell::GetVerifyReflowFlags();
  if (VERIFY_REFLOW_INCLUDE_SPACE_MANAGER & verifyReflowFlags)
  {
    // this is a leak of the space manager, but it's only in debug if verify reflow is enabled, so not a big deal
    nsIPresShell *shell = aPresContext->GetPresShell();
    if (shell) {
      nsHTMLReflowState&  reflowState = (nsHTMLReflowState&)aReflowState;
      rv = SetProperty(nsGkAtoms::spaceManagerProperty,
                       reflowState.mSpaceManager,
                       nsnull /* should be nsSpaceManagerDestroyer*/);

      autoSpaceManager.DebugOrphanSpaceManager();
    }
  }
#endif

  // Let the absolutely positioned container reflow any absolutely positioned
  // child frames that need to be reflowed, e.g., elements with a percentage
  // based width/height
  // We want to do this under either of two conditions:
  //  1. If we didn't do the incremental reflow above.
  //  2. If our size changed.
  // Even though it's the padding edge that's the containing block, we
  // can use our rect (the border edge) since if the border style
  // changed, the reflow would have been targeted at us so we'd satisfy
  // condition 1.
  if (mAbsoluteContainer.HasAbsoluteFrames()) {
    nsRect childBounds;
    nsSize containingBlockSize
      = CalculateContainingBlockSizeForAbsolutes(aReflowState,
                                                 nsSize(aMetrics.width, aMetrics.height));

    // Mark frames that depend on changes we just made to this frame as dirty:
    // Now we can assume that the padding edge hasn't moved.
    // We need to reflow the absolutes if one of them depends on
    // its placeholder position, or the containing block size in a
    // direction in which the containing block size might have
    // changed.
    PRBool cbWidthChanged = aMetrics.width != oldSize.width;
    PRBool isRoot = !GetContent()->GetParent();
    // If isRoot and we have auto height, then we are the initial
    // containing block and the containing block height is the
    // viewport height, which can't change during incremental
    // reflow.
    PRBool cbHeightChanged =
      !(isRoot && NS_UNCONSTRAINEDSIZE == aReflowState.mComputedHeight) &&
      aMetrics.height != oldSize.height;

    rv = mAbsoluteContainer.Reflow(this, aPresContext, aReflowState,
                                   containingBlockSize.width,
                                   containingBlockSize.height,
                                   cbWidthChanged, cbHeightChanged,
                                   &childBounds);

    // Factor the absolutely positioned child bounds into the overflow area
    aMetrics.mOverflowArea.UnionRect(aMetrics.mOverflowArea, childBounds);
  }

  FinishAndStoreOverflow(&aMetrics);

  // Determine if we need to repaint our border, background or outline
  CheckInvalidateSizeChange(aPresContext, aMetrics, aReflowState);

  // Clear the space manager pointer in the block reflow state so we
  // don't waste time translating the coordinate system back on a dead
  // space manager.
  if (NS_BLOCK_SPACE_MGR & mState)
    state.mSpaceManager = nsnull;

  aStatus = state.mReflowStatus;

#ifdef DEBUG
  if (gNoisyReflow) {
    IndentBy(stdout, gNoiseIndent);
    ListTag(stdout);
    printf(": status=%x (%scomplete) metrics=%d,%d carriedMargin=%d",
           aStatus, NS_FRAME_IS_COMPLETE(aStatus) ? "" : "not ",
           aMetrics.width, aMetrics.height,
           aMetrics.mCarriedOutBottomMargin.get());
    if (mState & NS_FRAME_OUTSIDE_CHILDREN) {
      printf(" combinedArea={%d,%d,%d,%d}",
             aMetrics.mOverflowArea.x,
             aMetrics.mOverflowArea.y,
             aMetrics.mOverflowArea.width,
             aMetrics.mOverflowArea.height);
    }
    printf("\n");
  }

  if (gLameReflowMetrics) {
    PRTime end = PR_Now();

    PRInt32 ectc = nsLineBox::GetCtorCount();
    PRInt32 numLines = mLines.size();
    if (!numLines) numLines = 1;
    PRTime delta, perLineDelta, lines;
    LL_I2L(lines, numLines);
    LL_SUB(delta, end, start);
    LL_DIV(perLineDelta, delta, lines);

    ListTag(stdout);
    char buf[400];
    PR_snprintf(buf, sizeof(buf),
                ": %lld elapsed (%lld per line) (%d lines; %d new lines)",
                delta, perLineDelta, numLines, ectc - ctc);
    printf("%s\n", buf);
  }
#endif

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aMetrics);
  return rv;
}

PRBool
nsBlockFrame::CheckForCollapsedBottomMarginFromClearanceLine()
{
  line_iterator begin = begin_lines();
  line_iterator line = end_lines();

  while (PR_TRUE) {
    if (begin == line) {
      return PR_FALSE;
    }
    --line;
    if (line->mBounds.height != 0 || !line->CachedIsEmpty()) {
      return PR_FALSE;
    }
    if (line->HasClearance()) {
      return PR_TRUE;
    }
  }
  // not reached
}

void
nsBlockFrame::ComputeFinalSize(const nsHTMLReflowState& aReflowState,
                               nsBlockReflowState&      aState,
                               nsHTMLReflowMetrics&     aMetrics)
{
  const nsMargin& borderPadding = aState.BorderPadding();
#ifdef NOISY_FINAL_SIZE
  ListTag(stdout);
  printf(": mY=%d mIsBottomMarginRoot=%s mPrevBottomMargin=%d bp=%d,%d\n",
         aState.mY, aState.GetFlag(BRS_ISBOTTOMMARGINROOT) ? "yes" : "no",
         aState.mPrevBottomMargin,
         borderPadding.top, borderPadding.bottom);
#endif

  // Compute final width
  aMetrics.width = borderPadding.left + aReflowState.ComputedWidth() +
    borderPadding.right;

  // Return bottom margin information
  // rbs says he hit this assertion occasionally (see bug 86947), so
  // just set the margin to zero and we'll figure out why later
  //NS_ASSERTION(aMetrics.mCarriedOutBottomMargin.IsZero(),
  //             "someone else set the margin");
  nscoord nonCarriedOutVerticalMargin = 0;
  if (!aState.GetFlag(BRS_ISBOTTOMMARGINROOT)) {
    // Apply rule from CSS 2.1 section 8.3.1. If we have some empty
    // line with clearance and a non-zero top margin and all
    // subsequent lines are empty, then we do not allow our children's
    // carried out bottom margin to be carried out of us and collapse
    // with our own bottom margin.
    if (CheckForCollapsedBottomMarginFromClearanceLine()) {
      // Convert the children's carried out margin to something that
      // we will include in our height
      nonCarriedOutVerticalMargin = aState.mPrevBottomMargin.get();
      aState.mPrevBottomMargin.Zero();
    }
    aMetrics.mCarriedOutBottomMargin = aState.mPrevBottomMargin;
  } else {
    aMetrics.mCarriedOutBottomMargin.Zero();
  }

  // Compute final height
  if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedHeight) {
    // Figure out how much of the computed height should be
    // applied to this frame.
    nscoord computedHeightLeftOver = aReflowState.mComputedHeight;
    if (GetPrevInFlow()) {
      // Reduce the height by the computed height of prev-in-flows.
      for (nsIFrame* prev = GetPrevInFlow(); prev; prev = prev->GetPrevInFlow()) {
        computedHeightLeftOver -= prev->GetRect().height;
      }
      // We just subtracted our top-border padding, since it was included in the
      // first frame's height. Add it back to get the content height.
      computedHeightLeftOver += aReflowState.mComputedBorderPadding.top;
      // We may have stretched the frame beyond its computed height. Oh well.
      computedHeightLeftOver = PR_MAX(0, computedHeightLeftOver);
    }

    if (NS_FRAME_IS_COMPLETE(aState.mReflowStatus)) {
      aMetrics.height = borderPadding.top + computedHeightLeftOver + borderPadding.bottom;
      if (computedHeightLeftOver > 0 &&
          aMetrics.height > aReflowState.availableHeight) {
        // We don't fit and we consumed some of the computed height,
        // so we should consume all the available height and then
        // break.  If our bottom border/padding straddles the break
        // point, then this will increase our height and push the
        // border/padding to the next page/column.
        aMetrics.height = aReflowState.availableHeight;
        aState.mReflowStatus |= NS_FRAME_NOT_COMPLETE;
      }
    }
    else {
      // Use the current height; continuations will take up the rest.
      // Do extend the height to at least consume the available
      // height, otherwise our left/right borders (for example) won't
      // extend all the way to the break.
      aMetrics.height = PR_MAX(aReflowState.availableHeight,
                               aState.mY + nonCarriedOutVerticalMargin);
      // ... but don't take up more height than is available
      aMetrics.height = PR_MIN(aMetrics.height,
                               borderPadding.top + computedHeightLeftOver);
      // XXX It's pretty wrong that our bottom border still gets drawn on
      // on its own on the last-in-flow, even if we ran out of height
      // here. We need GetSkipSides to check whether we ran out of content
      // height in the current frame, not whether it's last-in-flow.
    }

    // Don't carry out a bottom margin when our height is fixed.
    aMetrics.mCarriedOutBottomMargin.Zero();
  }
  else {
    nscoord autoHeight = aState.mY + nonCarriedOutVerticalMargin;

    // Shrink wrap our height around our contents.
    if (aState.GetFlag(BRS_ISBOTTOMMARGINROOT)) {
      // When we are a bottom-margin root make sure that our last
      // childs bottom margin is fully applied.
      // Apply the margin only if there's space for it.
      if (autoHeight < aState.mReflowState.availableHeight)
      {
        // Truncate bottom margin if it doesn't fit to our available height.
        autoHeight = PR_MIN(autoHeight + aState.mPrevBottomMargin.get(), aState.mReflowState.availableHeight);
      }
    }

    if (NS_BLOCK_SPACE_MGR & mState) {
      // Include the space manager's state to properly account for the
      // bottom margin of any floated elements; e.g., inside a table cell.
      nscoord ymost;
      if (aReflowState.mSpaceManager->YMost(ymost) &&
          autoHeight < ymost)
        autoHeight = ymost;
    }

    // Apply min/max values
    autoHeight -= borderPadding.top;
    nscoord oldAutoHeight = autoHeight;
    aReflowState.ApplyMinMaxConstraints(nsnull, &autoHeight);
    if (autoHeight != oldAutoHeight) {
      // Our min-height or max-height made our height change.  Don't carry out
      // our kids' bottom margins.
      aMetrics.mCarriedOutBottomMargin.Zero();
    }
    autoHeight += borderPadding.top + borderPadding.bottom;
    aMetrics.height = autoHeight;
  }

#ifdef DEBUG_blocks
  if (CRAZY_WIDTH(aMetrics.width) || CRAZY_HEIGHT(aMetrics.height)) {
    ListTag(stdout);
    printf(": WARNING: desired:%d,%d\n", aMetrics.width, aMetrics.height);
  }
#endif
}

void
nsBlockFrame::ComputeCombinedArea(const nsHTMLReflowState& aReflowState,
                                  nsHTMLReflowMetrics& aMetrics)
{
  // Compute the combined area of our children
  // XXX_perf: This can be done incrementally.  It is currently one of
  // the things that makes incremental reflow O(N^2).
  nsRect area(0, 0, aMetrics.width, aMetrics.height);
  if (NS_STYLE_OVERFLOW_CLIP != aReflowState.mStyleDisplay->mOverflowX) {
    for (line_iterator line = begin_lines(), line_end = end_lines();
         line != line_end;
         ++line) {
      area.UnionRect(area, line->GetCombinedArea());
    }

    // Factor the bullet in; normally the bullet will be factored into
    // the line-box's combined area. However, if the line is a block
    // line then it won't; if there are no lines, it won't. So just
    // factor it in anyway (it can't hurt if it was already done).
    // XXXldb Can we just fix GetCombinedArea instead?
    if (mBullet) {
      area.UnionRect(area, mBullet->GetRect());
    }
  }
#ifdef NOISY_COMBINED_AREA
  ListTag(stdout);
  printf(": ca=%d,%d,%d,%d\n", area.x, area.y, area.width, area.height);
#endif

  aMetrics.mOverflowArea = area;
}

nsresult
nsBlockFrame::MarkLineDirty(line_iterator aLine)
{
  // Mark aLine dirty
  aLine->MarkDirty();
#ifdef DEBUG
  if (gNoisyReflow) {
    IndentBy(stdout, gNoiseIndent);
    ListTag(stdout);
    printf(": mark line %p dirty\n", NS_STATIC_CAST(void*, aLine.get()));
  }
#endif

  // Mark previous line dirty if it's an inline line so that it can
  // maybe pullup something from the line just affected.
  // XXX We don't need to do this if aPrevLine ends in a break-after...
  if (aLine != mLines.front() &&
      aLine->IsInline() &&
      aLine.prev()->IsInline()) {
    aLine.prev()->MarkDirty();
#ifdef DEBUG
    if (gNoisyReflow) {
      IndentBy(stdout, gNoiseIndent);
      ListTag(stdout);
      printf(": mark prev-line %p dirty\n",
             NS_STATIC_CAST(void*, aLine.prev().get()));
    }
#endif
  }

  return NS_OK;
}

nsresult
nsBlockFrame::PrepareResizeReflow(nsBlockReflowState& aState)
{
  // we need to calculate if any part of then block itself 
  // is impacted by a float (bug 19579)
  aState.GetAvailableSpace();

  const nsStyleText* styleText = GetStyleText();
  // See if we can try and avoid marking all the lines as dirty
  PRBool tryAndSkipLines =
      // There must be no floats.
      !aState.IsImpactedByFloat() &&
      // The text must be left-aligned.
      (NS_STYLE_TEXT_ALIGN_LEFT == styleText->mTextAlign ||
       (NS_STYLE_TEXT_ALIGN_DEFAULT == styleText->mTextAlign &&
        NS_STYLE_DIRECTION_LTR ==
          aState.mReflowState.mStyleVisibility->mDirection)) &&
      // The left content-edge must be a constant distance from the left
      // border-edge.
      GetStylePadding()->mPadding.GetLeftUnit() != eStyleUnit_Percent;

#ifdef DEBUG
  if (gDisableResizeOpt) {
    tryAndSkipLines = PR_FALSE;
  }
  if (gNoisyReflow) {
    if (!tryAndSkipLines) {
      IndentBy(stdout, gNoiseIndent);
      ListTag(stdout);
      printf(": marking all lines dirty: availWidth=%d textAlign=%d\n",
             aState.mReflowState.availableWidth,
             styleText->mTextAlign);
    }
  }
#endif

  if (tryAndSkipLines) {
    nscoord newAvailWidth = aState.mReflowState.mComputedBorderPadding.left +
                            aState.mReflowState.ComputedWidth();
    NS_ASSERTION(NS_UNCONSTRAINEDSIZE != aState.mReflowState.mComputedBorderPadding.left &&
                 NS_UNCONSTRAINEDSIZE != aState.mReflowState.ComputedWidth(),
                 "math on NS_UNCONSTRAINEDSIZE");

#ifdef DEBUG
    if (gNoisyReflow) {
      IndentBy(stdout, gNoiseIndent);
      ListTag(stdout);
      printf(": trying to avoid marking all lines dirty\n");
    }
#endif

    for (line_iterator line = begin_lines(), line_end = end_lines();
         line != line_end;
         ++line)
    {
      // We let child blocks make their own decisions the same
      // way we are here.
      if (line->IsBlock() ||
          line->HasFloats() ||
          (line != mLines.back() && !line->HasBreakAfter()) ||
          line->ResizeReflowOptimizationDisabled() ||
          line->IsImpactedByFloat() ||
          (line->mBounds.XMost() > newAvailWidth)) {
        line->MarkDirty();
      }

#ifdef REALLY_NOISY_REFLOW
      if (!line->IsBlock()) {
        printf("PrepareResizeReflow thinks line %p is %simpacted by floats\n", 
               line.get(), line->IsImpactedByFloat() ? "" : "not ");
      }
#endif
#ifdef DEBUG
      if (gNoisyReflow && !line->IsDirty()) {
        IndentBy(stdout, gNoiseIndent + 1);
        printf("skipped: line=%p next=%p %s %s%s%s breakTypeBefore/After=%d/%d xmost=%d\n",
           NS_STATIC_CAST(void*, line.get()),
           NS_STATIC_CAST(void*, (line.next() != end_lines() ? line.next().get() : nsnull)),
           line->IsBlock() ? "block" : "inline",
           line->HasBreakAfter() ? "has-break-after " : "",
           line->HasFloats() ? "has-floats " : "",
           line->IsImpactedByFloat() ? "impacted " : "",
           line->GetBreakTypeBefore(), line->GetBreakTypeAfter(),
           line->mBounds.XMost());
      }
#endif
    }
  }
  else {
    // Mark everything dirty
    for (line_iterator line = begin_lines(), line_end = end_lines();
         line != line_end;
         ++line)
    {
      line->MarkDirty();
    }
  }
  return NS_OK;
}

//----------------------------------------

nsBlockFrame::line_iterator
nsBlockFrame::FindLineFor(nsIFrame* aFrame)
{
  NS_PRECONDITION(aFrame, "why pass a null frame?");
  line_iterator line = begin_lines(),
                line_end = end_lines();
  for ( ; line != line_end; ++line) {
    // If the target frame is in-flow, and this line contains the it,
    // then we've found our line.
    if (line->Contains(aFrame))
      return line;

    // If the target frame is floated, and this line contains the
    // float's placeholder, then we've found our line.
    if (line->HasFloats()) {
      for (nsFloatCache *fc = line->GetFirstFloat();
           fc != nsnull;
           fc = fc->Next()) {
        if (aFrame == fc->mPlaceholder->GetOutOfFlowFrame())
          return line;
      }
    }
  }

  return line_end;
}

/**
 * Propagate reflow "damage" from from earlier lines to the current
 * line.  The reflow damage comes from the following sources:
 *  1. The regions of float damage remembered during reflow.
 *  2. The combination of nonzero |aDeltaY| and any impact by a float,
 *     either the previous reflow or now.
 *
 * When entering this function, |aLine| is still at its old position and
 * |aDeltaY| indicates how much it will later be slid (assuming it
 * doesn't get marked dirty and reflowed entirely).
 */
void
nsBlockFrame::PropagateFloatDamage(nsBlockReflowState& aState,
                                   nsLineBox* aLine,
                                   nscoord aDeltaY)
{
  NS_PRECONDITION(!aLine->IsDirty(), "should never be called on dirty lines");

  // Check the damage region recorded in the float damage.
  nsSpaceManager *spaceManager = aState.mReflowState.mSpaceManager;
  if (spaceManager->HasFloatDamage()) {
    nscoord lineYA = aLine->mBounds.y + aDeltaY;
    nscoord lineYB = lineYA + aLine->mBounds.height;
    if (spaceManager->IntersectsDamage(lineYA, lineYB)) {
      aLine->MarkDirty();
      return;
    }
  }

  if (aDeltaY) {
    // Cases we need to find:
    //
    // 1. the line was impacted by a float and now isn't
    // 2. the line wasn't impacted by a float and now is
    // 3. the line is impacted by a float both before and after and 
    //    the float has changed position relative to the line (or it's
    //    a different float).  (XXXPerf we don't currently
    //    check whether the float changed size.  We currently just
    //    mark blocks dirty and ignore any possibility of damage to
    //    inlines by it being a different float with a different
    //    size.)
    //
    //    XXXPerf: An optimization: if the line was and is completely
    //    impacted by a float and the float hasn't changed size,
    //    then we don't need to mark the line dirty.
    aState.GetAvailableSpace(aLine->mBounds.y + aDeltaY, PR_FALSE);
    PRBool wasImpactedByFloat = aLine->IsImpactedByFloat();
    PRBool isImpactedByFloat = aState.IsImpactedByFloat();
#ifdef REALLY_NOISY_REFLOW
    printf("nsBlockFrame::PropagateFloatDamage %p was = %d, is=%d\n", 
       this, wasImpactedByFloat, isImpactedByFloat);
#endif
    // Mark the line dirty if:
    //  1. It used to be impacted by a float and now isn't, or vice
    //     versa.
    //  2. It is impacted by a float and it is a block, which means
    //     that more or less of the line could be impacted than was in
    //     the past.  (XXXPerf This could be optimized further, since
    //     we're marking the whole line dirty.)
    if ((wasImpactedByFloat != isImpactedByFloat) ||
        (isImpactedByFloat && aLine->IsBlock())) {
      aLine->MarkDirty();
    }
  }
}

static void PlaceFrameView(nsIFrame* aFrame);

static PRBool LineHasClear(nsLineBox* aLine) {
  return aLine->GetBreakTypeBefore() || aLine->HasFloatBreakAfter()
    || (aLine->IsBlock() && (aLine->mFirstChild->GetStateBits() & NS_BLOCK_HAS_CLEAR_CHILDREN));
}


/**
 * Reparent a whole list of floats from aOldParent to this block.  The
 * floats might be taken from aOldParent's overflow list. They will be
 * removed from the list. They end up appended to our mFloats list.
 */
void
nsBlockFrame::ReparentFloats(nsIFrame* aFirstFrame,
                             nsBlockFrame* aOldParent, PRBool aFromOverflow) {
  nsFrameList list;
  nsIFrame* tail = nsnull;
  aOldParent->CollectFloats(aFirstFrame, list, &tail, aFromOverflow);
  if (list.NotEmpty()) {
    for (nsIFrame* f = list.FirstChild(); f; f = f->GetNextSibling()) {
      ReparentFrame(f, aOldParent, this);
    }
    mFloats.AppendFrames(nsnull, list.FirstChild());
  }
}

static void DumpLine(const nsBlockReflowState& aState, nsLineBox* aLine,
                     nscoord aDeltaY, PRInt32 aDeltaIndent) {
#ifdef DEBUG
  if (nsBlockFrame::gNoisyReflow) {
    nsRect lca(aLine->GetCombinedArea());
    nsBlockFrame::IndentBy(stdout, nsBlockFrame::gNoiseIndent + aDeltaIndent);
    printf("line=%p mY=%d dirty=%s oldBounds={%d,%d,%d,%d} oldCombinedArea={%d,%d,%d,%d} deltaY=%d mPrevBottomMargin=%d childCount=%d\n",
           NS_STATIC_CAST(void*, aLine), aState.mY,
           aLine->IsDirty() ? "yes" : "no",
           aLine->mBounds.x, aLine->mBounds.y,
           aLine->mBounds.width, aLine->mBounds.height,
           lca.x, lca.y, lca.width, lca.height,
           aDeltaY, aState.mPrevBottomMargin.get(), aLine->GetChildCount());
  }
#endif
}

/**
 * Reflow the dirty lines
 */
nsresult
nsBlockFrame::ReflowDirtyLines(nsBlockReflowState& aState)
{
  nsresult rv = NS_OK;
  PRBool keepGoing = PR_TRUE;
  PRBool repositionViews = PR_FALSE; // should we really need this?
  PRBool foundAnyClears = PR_FALSE;

#ifdef DEBUG
  if (gNoisyReflow) {
    IndentBy(stdout, gNoiseIndent);
    ListTag(stdout);
    printf(": reflowing dirty lines");
    printf(" computedWidth=%d\n", aState.mReflowState.ComputedWidth());
  }
  AutoNoisyIndenter indent(gNoisyReflow);
#endif

  PRBool selfDirty = (GetStateBits() & NS_FRAME_IS_DIRTY) ||
                     (aState.mReflowState.mFlags.mVResize &&
                      (GetStateBits() & NS_FRAME_CONTAINS_RELATIVE_HEIGHT));
  
    // the amount by which we will slide the current line if it is not
    // dirty
  nscoord deltaY = 0;

    // whether we did NOT reflow the previous line and thus we need to
    // recompute the carried out margin before the line if we want to
    // reflow it or if its previous margin is dirty
  PRBool needToRecoverState = PR_FALSE;
  PRBool reflowedFloat = PR_FALSE;
  PRBool lastLineMovedUp = PR_FALSE;
  // We save up information about BR-clearance here
  PRUint8 inlineFloatBreakType = NS_STYLE_CLEAR_NONE;

  line_iterator line = begin_lines(), line_end = end_lines();

  // Reflow the lines that are already ours
  for ( ; line != line_end; ++line, aState.AdvanceToNextLine()) {
    DumpLine(aState, line, deltaY, 0);
#ifdef DEBUG
    AutoNoisyIndenter indent2(gNoisyReflow);
#endif

    if (selfDirty)
      line->MarkDirty();

    // This really sucks, but we have to look inside any blocks that have clear
    // elements inside them.
    // XXX what can we do smarter here?
    if (!line->IsDirty() && line->IsBlock() &&
        (line->mFirstChild->GetStateBits() & NS_BLOCK_HAS_CLEAR_CHILDREN)) {
      line->MarkDirty();
    }

    // We have to reflow the line if it's a block whose clearance
    // might have changed, so detect that.
    if (!line->IsDirty() && line->GetBreakTypeBefore() != NS_STYLE_CLEAR_NONE) {
      nscoord curY = aState.mY;
      // See where we would be after applying any clearance due to
      // BRs.
      if (inlineFloatBreakType != NS_STYLE_CLEAR_NONE) {
        curY = aState.ClearFloats(curY, inlineFloatBreakType);
      }

      nscoord newY = aState.ClearFloats(curY, line->GetBreakTypeBefore());
      
      if (line->HasClearance()) {
        // Reflow the line if it might not have clearance anymore.
        if (newY == curY
            // aState.mY is the clearance point which should be the
            // top border-edge of the block frame. If sliding the
            // block by deltaY isn't going to put it in the predicted
            // position, then we'd better reflow the line.
            || newY != line->mBounds.y + deltaY) {
          line->MarkDirty();
        }
      } else {
        // Reflow the line if the line might have clearance now.
        if (curY != newY) {
          line->MarkDirty();
        }
      }
    }

    // We might have to reflow a line that is after a clearing BR.
    if (inlineFloatBreakType != NS_STYLE_CLEAR_NONE) {
      aState.mY = aState.ClearFloats(aState.mY, inlineFloatBreakType);
      if (aState.mY != line->mBounds.y + deltaY) {
        // SlideLine is not going to put the line where the clearance
        // put it. Reflow the line to be sure.
        line->MarkDirty();
      }
      inlineFloatBreakType = NS_STYLE_CLEAR_NONE;
    }

    PRBool previousMarginWasDirty = line->IsPreviousMarginDirty();
    if (previousMarginWasDirty) {
      // If the previous margin is dirty, reflow the current line
      line->MarkDirty();
      line->ClearPreviousMarginDirty();
    } else if (line->mBounds.YMost() + deltaY > aState.mBottomEdge) {
      // Lines that aren't dirty but get slid past our height constraint must
      // be reflowed.
      line->MarkDirty();
    }

    if (!line->IsDirty()) {
      // See if there's any reflow damage that requires that we mark the
      // line dirty.
      PropagateFloatDamage(aState, line, deltaY);
    }

    if (needToRecoverState && line->IsDirty()) {
      // We need to reconstruct the bottom margin only if we didn't
      // reflow the previous line and we do need to reflow (or repair
      // the top position of) the next line.
      aState.ReconstructMarginAbove(line);
    }

    if (needToRecoverState) {
      needToRecoverState = PR_FALSE;

      // Update aState.mPrevChild as if we had reflowed all of the frames in
      // this line.  This is expensive in some cases, since it requires
      // walking |GetNextSibling|.
      if (line->IsDirty())
        aState.mPrevChild = line.prev()->LastChild();
    }

    // Now repair the line and update |aState.mY| by calling
    // |ReflowLine| or |SlideLine|.
    if (line->IsDirty()) {
      lastLineMovedUp = PR_TRUE;

      PRBool maybeReflowingForFirstTime =
        line->mBounds.x == 0 && line->mBounds.y == 0 &&
        line->mBounds.width == 0 && line->mBounds.height == 0;

      // Compute the dirty lines "before" YMost, after factoring in
      // the running deltaY value - the running value is implicit in
      // aState.mY.
      nscoord oldY = line->mBounds.y;
      nscoord oldYMost = line->mBounds.YMost();

      // Reflow the dirty line. If it's an incremental reflow, then force
      // it to invalidate the dirty area if necessary
      rv = ReflowLine(aState, line, &keepGoing);
      NS_ENSURE_SUCCESS(rv, rv);
      
      if (line->HasFloats()) {
        reflowedFloat = PR_TRUE;
      }

      if (!keepGoing) {
        DumpLine(aState, line, deltaY, -1);
        if (0 == line->GetChildCount()) {
          DeleteLine(aState, line, line_end);
        }
        break;
      }

      // Test to see whether the margin that should be carried out
      // to the next line (NL) might have changed. In ReflowBlockFrame
      // we call nextLine->MarkPreviousMarginDirty if the block's
      // actual carried-out bottom margin changed. So here we only
      // need to worry about the following effects:
      // 1) the line was just created, and it might now be blocking
      // a carried-out bottom margin from previous lines that
      // used to reach NL from reaching NL
      // 2) the line used to be empty, and is now not empty,
      // thus blocking a carried-out bottom margin from previous lines
      // that used to reach NL from reaching NL
      // 3) the line wasn't empty, but now is, so a carried-out
      // bottom margin from previous lines that didn't used to reach NL
      // now does
      // 4) the line might have changed in a way that affects NL's
      // ShouldApplyTopMargin decision. The three things that matter
      // are the line's emptiness, its adjacency to the top of the block,
      // and whether it has clearance (the latter only matters if the block
      // was and is adjacent to the top and empty).
      //
      // If the line is empty now, we can't reliably tell if the line was empty
      // before, so we just assume it was and do nextLine->MarkPreviousMarginDirty.
      // This means the checks in 4) are redundant; if the line is empty now
      // we don't need to check 4), but if the line is not empty now and we're sure
      // it wasn't empty before, any adjacency and clearance changes are irrelevant
      // to the result of nextLine->ShouldApplyTopMargin.
      if (line.next() != end_lines()) {
        PRBool maybeWasEmpty = oldY == oldYMost;
        PRBool isEmpty = line->mBounds.height == 0 && line->CachedIsEmpty();
        if (maybeReflowingForFirstTime /*1*/ ||
            (isEmpty || maybeWasEmpty) /*2/3/4*/) {
          line.next()->MarkPreviousMarginDirty();
          // since it's marked dirty, nobody will care about |deltaY|
        }
      }

      // If the line was just reflowed for the first time, then its
      // old mBounds cannot be trusted so this deltaY computation is
      // bogus. But that's OK because we just did
      // MarkPreviousMarginDirty on the next line which will force it
      // to be reflowed, so this computation of deltaY will not be
      // used.
      deltaY = line->mBounds.YMost() - oldYMost;
    } else {
      lastLineMovedUp = deltaY < 0;

      if (deltaY != 0)
        SlideLine(aState, line, deltaY);
      else
        repositionViews = PR_TRUE;

      // XXX EVIL O(N^2) EVIL
      aState.RecoverStateFrom(line, deltaY);

      // Keep mY up to date in case we're propagating reflow damage
      // and also because our final height may depend on it. If the
      // line is inlines, then only update mY if the line is not
      // empty, because that's what PlaceLine does. (Empty blocks may
      // want to update mY, e.g. if they have clearance.)
      if (line->IsBlock() || !line->CachedIsEmpty()) {
        aState.mY = line->mBounds.YMost();
      }

      needToRecoverState = PR_TRUE;
    }

    // Record if we need to clear floats before reflowing the next
    // line. Note that inlineFloatBreakType will be handled and
    // cleared before the next line is processed, so there is no
    // need to combine break types here.
    if (line->HasFloatBreakAfter()) {
      inlineFloatBreakType = line->GetBreakTypeAfter();
    }

    if (LineHasClear(line.get())) {
      foundAnyClears = PR_TRUE;
    }

    DumpLine(aState, line, deltaY, -1);
  }

  // Handle BR-clearance from the last line of the block
  if (inlineFloatBreakType != NS_STYLE_CLEAR_NONE) {
    aState.mY = aState.ClearFloats(aState.mY, inlineFloatBreakType);
  }

  if (needToRecoverState) {
    // Is this expensive?
    aState.ReconstructMarginAbove(line);

    // Update aState.mPrevChild as if we had reflowed all of the frames in
    // the last line.  This is expensive in some cases, since it requires
    // walking |GetNextSibling|.
    aState.mPrevChild = line.prev()->LastChild();
  }

  // Should we really have to do this?
  if (repositionViews)
    ::PlaceFrameView(this);

  // We can skip trying to pull up the next line if there is no next
  // in flow or we were told not to or we know it will be futile, i.e.,
  // -- the next in flow is not changing
  // -- and we cannot have added more space for its first line to be
  // pulled up into,
  // -- it's an incremental reflow of a descendant
  // -- and we didn't reflow any floats (so the available space
  // didn't change)
  // XXXldb We should also check that the first line of the next-in-flow
  // isn't dirty.
  if (!aState.mNextInFlow ||
      (aState.mReflowState.mFlags.mNextInFlowUntouched &&
       !lastLineMovedUp && 
       !(GetStateBits() & NS_FRAME_IS_DIRTY) &&
       !reflowedFloat)) {
    if (aState.mNextInFlow) {
      aState.mReflowStatus |= NS_FRAME_NOT_COMPLETE;
    }
  } else {
    // Pull data from a next-in-flow if there's still room for more
    // content here.
    while (keepGoing && (nsnull != aState.mNextInFlow)) {
      // Grab first line from our next-in-flow
      nsBlockFrame* nextInFlow = aState.mNextInFlow;
      line_iterator nifLine = nextInFlow->begin_lines();
      nsLineBox *toMove;
      PRBool collectOverflowFloats;
      if (nifLine != nextInFlow->end_lines()) {
        if (HandleOverflowPlaceholdersOnPulledLine(aState, nifLine)) {
          // go around again in case the line was deleted
          continue;
        }
        toMove = nifLine;
        nextInFlow->mLines.erase(nifLine);
        collectOverflowFloats = PR_FALSE;
      } else {
        // Grab an overflow line if there are any
        nsLineList* overflowLines = nextInFlow->GetOverflowLines();
        if (overflowLines &&
            HandleOverflowPlaceholdersOnPulledLine(aState, overflowLines->front())) {
          // go around again in case the line was deleted
          continue;
        }
        if (!overflowLines) {
          aState.mNextInFlow =
            NS_STATIC_CAST(nsBlockFrame*, nextInFlow->GetNextInFlow());
          continue;
        }
        nifLine = overflowLines->begin();
        NS_ASSERTION(nifLine != overflowLines->end(),
                     "Stored overflow line list should not be empty");
        toMove = nifLine;
        nextInFlow->RemoveOverflowLines();
        nifLine = overflowLines->erase(nifLine);
        if (nifLine != overflowLines->end()) {
          // We need to this remove-and-put-back dance because we want
          // to avoid making the overflow line list empty while it's
          // stored in the property (because the property has the
          // invariant that the list is never empty).
          nextInFlow->SetOverflowLines(overflowLines);
        }
        collectOverflowFloats = PR_TRUE;
      }

      if (0 == toMove->GetChildCount()) {
        // The line is empty. Try the next one.
        NS_ASSERTION(nsnull == toMove->mFirstChild, "bad empty line");
        aState.FreeLineBox(toMove);
        continue;
      }

      // XXX move to a subroutine: run-in, overflow, pullframe and this do this
      // Make the children in the line ours.
      nsIFrame* frame = toMove->mFirstChild;
      nsIFrame* lastFrame = nsnull;
      PRInt32 n = toMove->GetChildCount();
      while (--n >= 0) {
        ReparentFrame(frame, nextInFlow, this);
        lastFrame = frame;
        frame = frame->GetNextSibling();
      }
      lastFrame->SetNextSibling(nsnull);

      // Reparent floats whose placeholders are in the line.
      ReparentFloats(toMove->mFirstChild, nextInFlow, collectOverflowFloats);

      // Add line to our line list
      if (aState.mPrevChild) {
        aState.mPrevChild->SetNextSibling(toMove->mFirstChild);
      }

      line = mLines.before_insert(end_lines(), toMove);

      DumpLine(aState, toMove, deltaY, 0);
#ifdef DEBUG
      AutoNoisyIndenter indent2(gNoisyReflow);
#endif

      // Now reflow it and any lines that it makes during it's reflow
      // (we have to loop here because reflowing the line may case a new
      // line to be created; see SplitLine's callers for examples of
      // when this happens).
      while (line != end_lines()) {
        rv = ReflowLine(aState, line, &keepGoing);
        NS_ENSURE_SUCCESS(rv, rv);
        DumpLine(aState, line, deltaY, -1);
        if (!keepGoing) {
          if (0 == line->GetChildCount()) {
            DeleteLine(aState, line, line_end);
          }
          break;
        }

        if (LineHasClear(line.get())) {
          foundAnyClears = PR_TRUE;
        }

        // If this is an inline frame then its time to stop
        ++line;
        aState.AdvanceToNextLine();
      }
    }

    if (NS_FRAME_IS_NOT_COMPLETE(aState.mReflowStatus)) {
      aState.mReflowStatus |= NS_FRAME_REFLOW_NEXTINFLOW;
    }
  }

  // Handle an odd-ball case: a list-item with no lines
  if (mBullet && HaveOutsideBullet() && mLines.empty()) {
    nsHTMLReflowMetrics metrics;
    ReflowBullet(aState, metrics);

    // There are no lines so we have to fake up some y motion so that
    // we end up with *some* height.
    aState.mY += metrics.height;
  }

  if (foundAnyClears) {
    AddStateBits(NS_BLOCK_HAS_CLEAR_CHILDREN);
  } else {
    RemoveStateBits(NS_BLOCK_HAS_CLEAR_CHILDREN);
  }

#ifdef DEBUG
  if (gNoisyReflow) {
    IndentBy(stdout, gNoiseIndent - 1);
    ListTag(stdout);
    printf(": done reflowing dirty lines (status=%x)\n",
           aState.mReflowStatus);
  }
#endif

  return rv;
}

void
nsBlockFrame::DeleteLine(nsBlockReflowState& aState,
                         nsLineList::iterator aLine,
                         nsLineList::iterator aLineEnd)
{
  NS_PRECONDITION(0 == aLine->GetChildCount(), "can't delete !empty line");
  if (0 == aLine->GetChildCount()) {
    NS_ASSERTION(aState.mCurrentLine == aLine,
                 "using function more generally than designed, "
                 "but perhaps OK now");
    nsLineBox *line = aLine;
    aLine = mLines.erase(aLine);
    aState.FreeLineBox(line);
    // Mark the previous margin of the next line dirty since we need to
    // recompute its top position.
    if (aLine != aLineEnd)
      aLine->MarkPreviousMarginDirty();
  }
}

/**
 * Takes two rectangles whose origins must be the same, and computes
 * the difference between their union and their intersection as two
 * rectangles. (This difference is a superset of the difference
 * between the two rectangles.)
 */
static void GetRectDifferenceStrips(const nsRect& aR1, const nsRect& aR2,
                                    nsRect* aHStrip, nsRect* aVStrip) {
  NS_ASSERTION(aR1.TopLeft() == aR2.TopLeft(),
               "expected rects at the same position");
  nsRect unionRect(aR1.x, aR1.y, PR_MAX(aR1.width, aR2.width),
                   PR_MAX(aR1.height, aR2.height));
  nscoord VStripStart = PR_MIN(aR1.width, aR2.width);
  nscoord HStripStart = PR_MIN(aR1.height, aR2.height);
  *aVStrip = unionRect;
  aVStrip->x += VStripStart;
  aVStrip->width -= VStripStart;
  *aHStrip = unionRect;
  aHStrip->y += HStripStart;
  aHStrip->height -= HStripStart;
}

/**
 * Reflow a line. The line will either contain a single block frame
 * or contain 1 or more inline frames. aKeepReflowGoing indicates
 * whether or not the caller should continue to reflow more lines.
 */
nsresult
nsBlockFrame::ReflowLine(nsBlockReflowState& aState,
                         line_iterator aLine,
                         PRBool* aKeepReflowGoing)
{
  nsresult rv = NS_OK;

  NS_ABORT_IF_FALSE(aLine->GetChildCount(), "reflowing empty line");

  // Setup the line-layout for the new line
  aState.mCurrentLine = aLine;
  aLine->ClearDirty();
  aLine->InvalidateCachedIsEmpty();
  
  // Now that we know what kind of line we have, reflow it
  if (aLine->IsBlock()) {
    nsRect oldBounds = aLine->mFirstChild->GetRect();
    nsRect oldCombinedArea(aLine->GetCombinedArea());
    rv = ReflowBlockFrame(aState, aLine, aKeepReflowGoing);
    nsRect newBounds = aLine->mFirstChild->GetRect();

    // We expect blocks to damage any area inside their bounds that is
    // dirty; however, if the frame changes size or position then we
    // need to do some repainting.
    // XXX roc --- the above statement is ambiguous about whether 'bounds'
    // means the frame's bounds or overflowArea, and in fact this is a source
    // of much confusion and bugs. Thus the following hack considers *both*
    // overflowArea and bounds. This should be considered a temporary hack
    // until we decide how it's really supposed to work.
    nsRect lineCombinedArea(aLine->GetCombinedArea());
    if (oldCombinedArea.TopLeft() != lineCombinedArea.TopLeft() ||
        oldBounds.TopLeft() != newBounds.TopLeft()) {
      // The block has moved, and so to be safe we need to repaint
      // XXX We need to improve on this...
      nsRect  dirtyRect;
      dirtyRect.UnionRect(oldCombinedArea, lineCombinedArea);
#ifdef NOISY_BLOCK_INVALIDATE
      printf("%p invalidate 6 (%d, %d, %d, %d)\n",
             this, dirtyRect.x, dirtyRect.y, dirtyRect.width, dirtyRect.height);
#endif
      Invalidate(dirtyRect);
    } else {
      nsRect combinedAreaHStrip, combinedAreaVStrip;
      nsRect boundsHStrip, boundsVStrip;
      GetRectDifferenceStrips(oldBounds, newBounds,
                              &boundsHStrip, &boundsVStrip);
      GetRectDifferenceStrips(oldCombinedArea, lineCombinedArea,
                              &combinedAreaHStrip, &combinedAreaVStrip);

#ifdef NOISY_BLOCK_INVALIDATE
      printf("%p invalidate boundsVStrip (%d, %d, %d, %d)\n",
             this, boundsVStrip.x, boundsVStrip.y, boundsVStrip.width, boundsVStrip.height);
      printf("%p invalidate boundsHStrip (%d, %d, %d, %d)\n",
             this, boundsHStrip.x, boundsHStrip.y, boundsHStrip.width, boundsHStrip.height);
      printf("%p invalidate combinedAreaVStrip (%d, %d, %d, %d)\n",
             this, combinedAreaVStrip.x, combinedAreaVStrip.y, combinedAreaVStrip.width, combinedAreaVStrip.height);
      printf("%p invalidate combinedAreaHStrip (%d, %d, %d, %d)\n",
             this, combinedAreaHStrip.x, combinedAreaHStrip.y, combinedAreaHStrip.width, combinedAreaHStrip.height);
#endif
      // The first thing Invalidate does is check if the rect is empty, so
      // don't bother doing that here.
      Invalidate(boundsVStrip);
      Invalidate(boundsHStrip);
      Invalidate(combinedAreaVStrip);
      Invalidate(combinedAreaHStrip);
    }
  }
  else {
    nsRect oldCombinedArea(aLine->GetCombinedArea());
    aLine->SetLineWrapped(PR_FALSE);

    rv = ReflowInlineFrames(aState, aLine, aKeepReflowGoing);

    // We don't really know what changed in the line, so use the union
    // of the old and new combined areas
    nsRect dirtyRect;
    dirtyRect.UnionRect(oldCombinedArea, aLine->GetCombinedArea());
#ifdef NOISY_BLOCK_INVALIDATE
    printf("%p invalidate (%d, %d, %d, %d)\n",
           this, dirtyRect.x, dirtyRect.y, dirtyRect.width, dirtyRect.height);
    if (aLine->IsForceInvalidate())
      printf("  dirty line is %p\n", NS_STATIC_CAST(void*, aLine.get());
#endif
    Invalidate(dirtyRect);
  }

  return rv;
}

/**
 * Pull frame from the next available location (one of our lines or
 * one of our next-in-flows lines).
 */
nsresult
nsBlockFrame::PullFrame(nsBlockReflowState& aState,
                        line_iterator aLine,
                        nsIFrame*& aFrameResult)
{
  aFrameResult = nsnull;

  // First check our remaining lines
  if (end_lines() != aLine.next()) {
#ifdef DEBUG
    PRBool retry =
#endif
      PullFrameFrom(aState, aLine, this, PR_FALSE, aLine.next(), aFrameResult);
    NS_ASSERTION(!retry, "Shouldn't have to retry in the current block");
    return NS_OK;
  }

  NS_ASSERTION(!GetOverflowLines(),
    "Our overflow lines should have been removed at the start of reflow");

  // Try each next in flows
  nsBlockFrame* nextInFlow = aState.mNextInFlow;
  while (nextInFlow) {
    // first normal lines, then overflow lines
    if (!nextInFlow->mLines.empty()) {
      if (PullFrameFrom(aState, aLine, nextInFlow, PR_FALSE,
                        nextInFlow->mLines.begin(), aFrameResult)) {
        // try again with the same value of nextInFlow
        continue;
      }
      break;
    }

    nsLineList* overflowLines = nextInFlow->GetOverflowLines();
    if (overflowLines) {
      if (PullFrameFrom(aState, aLine, nextInFlow, PR_TRUE,
                        overflowLines->begin(), aFrameResult)) {
        // try again with the same value of nextInFlow
        continue;
      }
      break;
    }

    nextInFlow = (nsBlockFrame*) nextInFlow->GetNextInFlow();
    aState.mNextInFlow = nextInFlow;
  }

  return NS_OK;
}

/**
 * Try to pull a frame out of a line pointed at by aFromLine. If
 * aUpdateGeometricParent is set then the pulled frames geometric parent
 * will be updated (e.g. when pulling from a next-in-flows line list).
 *
 * Note: pulling a frame from a line that is a place-holder frame
 * doesn't automatically remove the corresponding float from the
 * line's float array. This happens indirectly: either the line gets
 * emptied (and destroyed) or the line gets reflowed (because we mark
 * it dirty) and the code at the top of ReflowLine empties the
 * array. So eventually, it will be removed, just not right away.
 *
 * @return PR_TRUE to force retrying of the pull.
 */
PRBool
nsBlockFrame::PullFrameFrom(nsBlockReflowState& aState,
                            nsLineBox* aLine,
                            nsBlockFrame* aFromContainer,
                            PRBool aFromOverflowLine,
                            nsLineList::iterator aFromLine,
                            nsIFrame*& aFrameResult)
{
  nsLineBox* fromLine = aFromLine;
  NS_ABORT_IF_FALSE(fromLine, "bad line to pull from");
  NS_ABORT_IF_FALSE(fromLine->GetChildCount(), "empty line");
  NS_ABORT_IF_FALSE(aLine->GetChildCount(), "empty line");

  NS_ASSERTION(fromLine->IsBlock() == fromLine->mFirstChild->GetStyleDisplay()->IsBlockLevel(),
               "Disagreement about whether it's a block or not");

  if (fromLine->IsBlock()) {
    // If our line is not empty and the child in aFromLine is a block
    // then we cannot pull up the frame into this line. In this case
    // we stop pulling.
    aFrameResult = nsnull;
  }
  else {
    // Take frame from fromLine
    nsIFrame* frame = fromLine->mFirstChild;

    if (aFromContainer != this) {
      if (HandleOverflowPlaceholdersForPulledFrame(aState, frame)) {
        // we lost this one, retry
        return PR_TRUE;
      }

      aLine->LastChild()->SetNextSibling(frame);
    }
    // when aFromContainer is 'this', then aLine->LastChild()'s next sibling
    // is already set correctly.
    aLine->SetChildCount(aLine->GetChildCount() + 1);
      
    PRInt32 fromLineChildCount = fromLine->GetChildCount();
    if (0 != --fromLineChildCount) {
      // Mark line dirty now that we pulled a child
      fromLine->SetChildCount(fromLineChildCount);
      fromLine->MarkDirty();
      fromLine->mFirstChild = frame->GetNextSibling();
    }
    else {
      // Free up the fromLine now that it's empty
      // Its bounds might need to be redrawn, though.
      // XXX WHY do we invalidate the bounds AND the combined area? doesn't
      // the combined area always enclose the bounds?
      Invalidate(fromLine->mBounds);
      nsLineList* fromLineList = aFromOverflowLine
        ? aFromContainer->RemoveOverflowLines()
        : &aFromContainer->mLines;
      if (aFromLine.next() != fromLineList->end())
        aFromLine.next()->MarkPreviousMarginDirty();

      Invalidate(fromLine->GetCombinedArea());
      fromLineList->erase(aFromLine);
      // Note that aFromLine just got incremented, so don't use it again here!
      aState.FreeLineBox(fromLine);

      // Put any remaining overflow lines back.
      if (aFromOverflowLine && !fromLineList->empty()) {
        aFromContainer->SetOverflowLines(fromLineList);
      }
    }

    // Change geometric parents
    if (aFromContainer != this) {
      // When pushing and pulling frames we need to check for whether any
      // views need to be reparented
      NS_ASSERTION(frame->GetParent() == aFromContainer, "unexpected parent frame");

      ReparentFrame(frame, aFromContainer, this);

      // The frame is being pulled from a next-in-flow; therefore we
      // need to add it to our sibling list.
      frame->SetNextSibling(nsnull);
      if (nsnull != aState.mPrevChild) {
        aState.mPrevChild->SetNextSibling(frame);
      }

      // The frame might have (or contain) floats that need to be
      // brought over too.
      ReparentFloats(frame, aFromContainer, aFromOverflowLine);
    }

    // Stop pulling because we found a frame to pull
    aFrameResult = frame;
#ifdef DEBUG
    VerifyLines(PR_TRUE);
#endif
  }
  return PR_FALSE;
}

static void
PlaceFrameView(nsIFrame* aFrame)
{
  if (aFrame->HasView())
    nsContainerFrame::PositionFrameView(aFrame);
  else
    nsContainerFrame::PositionChildViews(aFrame);
}

void
nsBlockFrame::SlideLine(nsBlockReflowState& aState,
                        nsLineBox* aLine, nscoord aDY)
{
  NS_PRECONDITION(aDY != 0, "why slide a line nowhere?");

  Invalidate(aLine->GetCombinedArea());
  // Adjust line state
  aLine->SlideBy(aDY);
  Invalidate(aLine->GetCombinedArea());

  // Adjust the frames in the line
  nsIFrame* kid = aLine->mFirstChild;
  if (!kid) {
    return;
  }

  if (aLine->IsBlock()) {
    if (aDY) {
      nsPoint p = kid->GetPosition();
      p.y += aDY;
      kid->SetPosition(p);
    }

    // Make sure the frame's view and any child views are updated
    ::PlaceFrameView(kid);
  }
  else {
    // Adjust the Y coordinate of the frames in the line.
    // Note: we need to re-position views even if aDY is 0, because
    // one of our parent frames may have moved and so the view's position
    // relative to its parent may have changed
    PRInt32 n = aLine->GetChildCount();
    while (--n >= 0) {
      if (aDY) {
        nsPoint p = kid->GetPosition();
        p.y += aDY;
        kid->SetPosition(p);
      }
      // Make sure the frame's view and any child views are updated
      ::PlaceFrameView(kid);
      kid = kid->GetNextSibling();
    }
  }
}

NS_IMETHODIMP 
nsBlockFrame::AttributeChanged(PRInt32         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               PRInt32         aModType)
{
  nsresult rv = nsBlockFrameSuper::AttributeChanged(aNameSpaceID,
                                                    aAttribute, aModType);

  if (NS_FAILED(rv)) {
    return rv;
  }
  if (nsGkAtoms::start == aAttribute) {
    nsPresContext* presContext = GetPresContext();

    // XXX Not sure if this is necessary anymore
    RenumberLists(presContext);

    presContext->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eStyleChange);
  }
  else if (nsGkAtoms::value == aAttribute) {
    const nsStyleDisplay* styleDisplay = GetStyleDisplay();
    if (NS_STYLE_DISPLAY_LIST_ITEM == styleDisplay->mDisplay) {
      // Search for the closest ancestor that's a block frame. We
      // make the assumption that all related list items share a
      // common block parent.
      // XXXldb I think that's a bad assumption.
      nsBlockFrame* blockParent = nsLayoutUtils::FindNearestBlockAncestor(this);

      // Tell the enclosing block frame to renumber list items within
      // itself
      if (nsnull != blockParent) {
        nsPresContext* presContext = GetPresContext();
        // XXX Not sure if this is necessary anymore
        blockParent->RenumberLists(presContext);

        presContext->PresShell()->
          FrameNeedsReflow(blockParent, nsIPresShell::eStyleChange);
      }
    }
  }

  return rv;
}

inline PRBool
IsPaddingZero(nsStyleUnit aUnit, nsStyleCoord &aCoord)
{
    return (aUnit == eStyleUnit_Null ||
            (aUnit == eStyleUnit_Coord && aCoord.GetCoordValue() == 0) ||
            (aUnit == eStyleUnit_Percent && aCoord.GetPercentValue() == 0.0));
}

inline PRBool
IsMarginZero(nsStyleUnit aUnit, nsStyleCoord &aCoord)
{
    return (aUnit == eStyleUnit_Null ||
            aUnit == eStyleUnit_Auto ||
            (aUnit == eStyleUnit_Coord && aCoord.GetCoordValue() == 0) ||
            (aUnit == eStyleUnit_Percent && aCoord.GetPercentValue() == 0.0));
}

/* virtual */ PRBool
nsBlockFrame::IsSelfEmpty()
{
  const nsStylePosition* position = GetStylePosition();

  switch (position->mMinHeight.GetUnit()) {
    case eStyleUnit_Coord:
      if (position->mMinHeight.GetCoordValue() != 0)
        return PR_FALSE;
      break;
    case eStyleUnit_Percent:
      if (position->mMinHeight.GetPercentValue() != 0.0f)
        return PR_FALSE;
      break;
    default:
      return PR_FALSE;
  }

  switch (position->mHeight.GetUnit()) {
    case eStyleUnit_Auto:
      break;
    case eStyleUnit_Coord:
      if (position->mHeight.GetCoordValue() != 0)
        return PR_FALSE;
      break;
    case eStyleUnit_Percent:
      if (position->mHeight.GetPercentValue() != 0.0f)
        return PR_FALSE;
      break;
    default:
      return PR_FALSE;
  }

  const nsStyleBorder* border = GetStyleBorder();
  const nsStylePadding* padding = GetStylePadding();
  nsStyleCoord coord;
  if (border->GetBorderWidth(NS_SIDE_TOP) != 0 ||
      border->GetBorderWidth(NS_SIDE_BOTTOM) != 0 ||
      !IsPaddingZero(padding->mPadding.GetTopUnit(),
                    padding->mPadding.GetTop(coord)) ||
      !IsPaddingZero(padding->mPadding.GetBottomUnit(),
                    padding->mPadding.GetBottom(coord))) {
    return PR_FALSE;
  }

  return PR_TRUE;
}

PRBool
nsBlockFrame::CachedIsEmpty()
{
  if (!IsSelfEmpty()) {
    return PR_FALSE;
  }

  for (line_iterator line = begin_lines(), line_end = end_lines();
       line != line_end;
       ++line)
  {
    if (!line->CachedIsEmpty())
      return PR_FALSE;
  }

  return PR_TRUE;
}

PRBool
nsBlockFrame::IsEmpty()
{
  if (!IsSelfEmpty()) {
    return PR_FALSE;
  }

  for (line_iterator line = begin_lines(), line_end = end_lines();
       line != line_end;
       ++line)
  {
    if (!line->IsEmpty())
      return PR_FALSE;
  }

  return PR_TRUE;
}

PRBool
nsBlockFrame::ShouldApplyTopMargin(nsBlockReflowState& aState,
                                   nsLineBox* aLine)
{
  if (aState.GetFlag(BRS_APPLYTOPMARGIN)) {
    // Apply short-circuit check to avoid searching the line list
    return PR_TRUE;
  }

  if (!aState.IsAdjacentWithTop()) {
    // If we aren't at the top Y coordinate then something of non-zero
    // height must have been placed. Therefore the childs top-margin
    // applies.
    aState.SetFlag(BRS_APPLYTOPMARGIN, PR_TRUE);
    return PR_TRUE;
  }

  // Determine if this line is "essentially" the first line
  line_iterator line = begin_lines();
  if (aState.GetFlag(BRS_HAVELINEADJACENTTOTOP)) {
    line = aState.mLineAdjacentToTop;
  }
  while (line != aLine) {
    if (!line->CachedIsEmpty() || line->HasClearance()) {
      // A line which precedes aLine is non-empty, or has clearance,
      // so therefore the top margin applies.
      aState.SetFlag(BRS_APPLYTOPMARGIN, PR_TRUE);
      return PR_TRUE;
    }
    // No need to apply the top margin if the line has floats.  We
    // should collapse anyway (bug 44419)
    ++line;
    aState.SetFlag(BRS_HAVELINEADJACENTTOTOP, PR_TRUE);
    aState.mLineAdjacentToTop = line;
  }

  // The line being reflowed is "essentially" the first line in the
  // block. Therefore its top-margin will be collapsed by the
  // generational collapsing logic with its parent (us).
  return PR_FALSE;
}

nsIFrame*
nsBlockFrame::GetTopBlockChild(nsPresContext* aPresContext)
{
  if (mLines.empty())
    return nsnull;

  nsLineBox *firstLine = mLines.front();
  if (firstLine->IsBlock())
    return firstLine->mFirstChild;

  if (!firstLine->CachedIsEmpty())
    return nsnull;

  line_iterator secondLine = begin_lines();
  ++secondLine;
  if (secondLine == end_lines() || !secondLine->IsBlock())
    return nsnull;

  return secondLine->mFirstChild;
}

// If placeholders/floats split during reflowing a line, but that line will 
// be put on the next page, then put the placeholders/floats back the way
// they were before the line was reflowed. 
void
nsBlockFrame::UndoSplitPlaceholders(nsBlockReflowState& aState,
                                    nsIFrame*           aLastPlaceholder)
{
  nsIFrame* undoPlaceholder;
  if (aLastPlaceholder) {
    undoPlaceholder = aLastPlaceholder->GetNextSibling();
    aLastPlaceholder->SetNextSibling(nsnull);
  }
  else {
    undoPlaceholder = aState.mOverflowPlaceholders.FirstChild();
    aState.mOverflowPlaceholders.SetFrames(nsnull);
  }
  // remove the next in flows of the placeholders that need to be removed
  for (nsPlaceholderFrame* placeholder = NS_STATIC_CAST(nsPlaceholderFrame*, undoPlaceholder);
       placeholder; ) {
    NS_ASSERTION(!placeholder->GetNextInFlow(), "Must be the last placeholder");

    nsFrameManager* fm = aState.mPresContext->GetPresShell()->FrameManager();
    fm->UnregisterPlaceholderFrame(placeholder);
    placeholder->SetOutOfFlowFrame(nsnull);

    // XXX we probably should be doing something with oof here. But what?

    nsSplittableFrame::RemoveFromFlow(placeholder);
    nsIFrame* savePlaceholder = placeholder; 
    placeholder = NS_STATIC_CAST(nsPlaceholderFrame*, placeholder->GetNextSibling());
    savePlaceholder->Destroy();
  }
}

nsresult
nsBlockFrame::ReflowBlockFrame(nsBlockReflowState& aState,
                               line_iterator aLine,
                               PRBool* aKeepReflowGoing)
{
  NS_PRECONDITION(*aKeepReflowGoing, "bad caller");

  nsresult rv = NS_OK;

  nsIFrame* frame = aLine->mFirstChild;
  if (!frame) {
    NS_ASSERTION(PR_FALSE, "program error - unexpected empty line"); 
    return NS_ERROR_NULL_POINTER; 
  }

  // Prepare the block reflow engine
  const nsStyleDisplay* display = frame->GetStyleDisplay();
  nsBlockReflowContext brc(aState.mPresContext, aState.mReflowState);

  PRUint8 breakType = display->mBreakType;
  // If a float split and its prev-in-flow was followed by a <BR>, then combine 
  // the <BR>'s break type with the block's break type (the block will be the very 
  // next frame after the split float).
  if (NS_STYLE_CLEAR_NONE != aState.mFloatBreakType) {
    breakType = nsLayoutUtils::CombineBreakType(breakType,
                                                aState.mFloatBreakType);
    aState.mFloatBreakType = NS_STYLE_CLEAR_NONE;
  }

  // Clear past floats before the block if the clear style is not none
  aLine->SetBreakTypeBefore(breakType);

  // See if we should apply the top margin. If the block frame being
  // reflowed is a continuation (non-null prev-in-flow) then we don't
  // apply its top margin because it's not significant. Otherwise, dig
  // deeper.
  PRBool applyTopMargin =
    !frame->GetPrevInFlow() && ShouldApplyTopMargin(aState, aLine);

  if (applyTopMargin) {
    // The HasClearance setting is only valid if ShouldApplyTopMargin
    // returned PR_FALSE (in which case the top-margin-root set our
    // clearance flag). Otherwise clear it now. We'll set it later on
    // ourselves if necessary.
    aLine->ClearHasClearance();
  }
  PRBool treatWithClearance = aLine->HasClearance();
  // If our top margin was counted as part of some parents top-margin
  // collapse and we are being speculatively reflowed assuming this
  // frame DID NOT need clearance, then we need to check that
  // assumption.
  if (!treatWithClearance && !applyTopMargin && breakType != NS_STYLE_CLEAR_NONE &&
      aState.mReflowState.mDiscoveredClearance) {
    nscoord curY = aState.mY + aState.mPrevBottomMargin.get();
    nscoord clearY = aState.ClearFloats(curY, breakType);
    if (clearY != curY) {
      // Looks like that assumption was invalid, we do need
      // clearance. Tell our ancestor so it can reflow again. It is
      // responsible for actually setting our clearance flag before
      // the next reflow.
      treatWithClearance = PR_TRUE;
      // Only record the first frame that requires clearance
      if (!*aState.mReflowState.mDiscoveredClearance) {
        *aState.mReflowState.mDiscoveredClearance = frame;
      }
      // Exactly what we do now is flexible since we'll definitely be
      // reflowed.
    }
  }
  if (treatWithClearance) {
    applyTopMargin = PR_TRUE;
  }

  nsIFrame* clearanceFrame = nsnull;
  nscoord startingY = aState.mY;
  nsCollapsingMargin incomingMargin = aState.mPrevBottomMargin;
  nscoord clearance;
  while (PR_TRUE) {
    // Save the frame's current position. We might need it later.
    nscoord originalY = frame->GetRect().y;
    
    clearance = 0;
    nscoord topMargin = 0;
    PRBool mayNeedRetry = PR_FALSE;
    if (applyTopMargin) {
      // Precompute the blocks top margin value so that we can get the
      // correct available space (there might be a float that's
      // already been placed below the aState.mPrevBottomMargin

      // Setup a reflowState to get the style computed margin-top
      // value. We'll use a reason of `resize' so that we don't fudge
      // any incremental reflow state.
      
      // The availSpace here is irrelevant to our needs - all we want
      // out if this setup is the margin-top value which doesn't depend
      // on the childs available space.
      // XXX building a complete nsHTMLReflowState just to get the margin-top
      // seems like a waste. And we do this for almost every block!
      nsSize availSpace(aState.mContentArea.width, NS_UNCONSTRAINEDSIZE);
      nsHTMLReflowState reflowState(aState.mPresContext, aState.mReflowState,
                                    frame, availSpace);
      
      if (treatWithClearance) {
        aState.mY += aState.mPrevBottomMargin.get();
        aState.mPrevBottomMargin.Zero();
      }
      
      // Now compute the collapsed margin-top value into aState.mPrevBottomMargin, assuming
      // that all child margins collapse down to clearanceFrame.
      nsBlockReflowContext::ComputeCollapsedTopMargin(reflowState,
                                                      &aState.mPrevBottomMargin, clearanceFrame, &mayNeedRetry);
      
      // XXX optimization; we could check the collapsing children to see if they are sure
      // to require clearance, and so avoid retrying them
      
      if (clearanceFrame) {
        // Don't allow retries on the second pass. The clearance decisions for the
        // blocks whose top-margins collapse with ours are now fixed.
        mayNeedRetry = PR_FALSE;
      }
      
      if (!treatWithClearance && !clearanceFrame && breakType != NS_STYLE_CLEAR_NONE) {
        // We don't know if we need clearance and this is the first,
        // optimistic pass.  So determine whether *this block* needs
        // clearance. Note that we do not allow the decision for whether
        // this block has clearance to change on the second pass; that
        // decision is only allowed to be made under the optimistic
        // first pass.
        nscoord curY = aState.mY + aState.mPrevBottomMargin.get();
        nscoord clearY = aState.ClearFloats(curY, breakType);
        if (clearY != curY) {
          // Looks like we need clearance and we didn't know about it already. So
          // recompute collapsed margin
          treatWithClearance = PR_TRUE;
          // Remember this decision, needed for incremental reflow
          aLine->SetHasClearance();
          
          // Apply incoming margins
          aState.mY += aState.mPrevBottomMargin.get();
          aState.mPrevBottomMargin.Zero();
          
          // Compute the collapsed margin again, ignoring the incoming margin this time
          mayNeedRetry = PR_FALSE;
          nsBlockReflowContext::ComputeCollapsedTopMargin(reflowState,
                                                          &aState.mPrevBottomMargin, clearanceFrame, &mayNeedRetry);
        }
      }
      
      // Temporarily advance the running Y value so that the
      // GetAvailableSpace method will return the right available
      // space. This undone as soon as the horizontal margins are
      // computed.
      topMargin = aState.mPrevBottomMargin.get();
      
      if (treatWithClearance) {
        nscoord currentY = aState.mY;
        // advance mY to the clear position.
        aState.mY = aState.ClearFloats(aState.mY, breakType);
        
        // Compute clearance. It's the amount we need to add to the top
        // border-edge of the frame, after applying collapsed margins
        // from the frame and its children, to get it to line up with
        // the bottom of the floats. The former is currentY + topMargin,
        // the latter is the current aState.mY.
        // Note that negative clearance is possible
        clearance = aState.mY - (currentY + topMargin);
        
        // Add clearance to our top margin while we compute available
        // space for the frame
        topMargin += clearance;
        
        // Note that aState.mY should stay where it is: at the top
        // border-edge of the frame
      } else {
        // Advance aState.mY to the top border-edge of the frame.
        aState.mY += topMargin;
      }
    }
    
    // Here aState.mY is the top border-edge of the block.
    // Compute the available space for the block
    aState.GetAvailableSpace();
#ifdef REALLY_NOISY_REFLOW
    printf("setting line %p isImpacted to %s\n", aLine.get(), aState.IsImpactedByFloat()?"true":"false");
#endif
    PRBool isImpacted = aState.IsImpactedByFloat() ? PR_TRUE : PR_FALSE;
    aLine->SetLineIsImpactedByFloat(isImpacted);
    nsRect availSpace;
    aState.ComputeBlockAvailSpace(frame, display, availSpace);
    
    // Now put the Y coordinate back to the top of the top-margin +
    // clearance, and flow the block.
    aState.mY -= topMargin;
    availSpace.y -= topMargin;
    if (NS_UNCONSTRAINEDSIZE != availSpace.height) {
      availSpace.height += topMargin;
    }
    
    // keep track of the last overflow float in case we need to undo any new additions
    nsIFrame* lastPlaceholder = aState.mOverflowPlaceholders.LastChild();
    
    // Reflow the block into the available space
    nsMargin computedOffsets;
    // construct the html reflow state for the block. ReflowBlock 
    // will initialize it
    nsHTMLReflowState blockHtmlRS(aState.mPresContext, aState.mReflowState, frame, 
                                  nsSize(availSpace.width, availSpace.height));
    blockHtmlRS.mFlags.mHasClearance = aLine->HasClearance();
    
    nsSpaceManager::SavedState spaceManagerState;
    if (mayNeedRetry) {
      blockHtmlRS.mDiscoveredClearance = &clearanceFrame;
      aState.mSpaceManager->PushState(&spaceManagerState);
    } else if (!applyTopMargin) {
      blockHtmlRS.mDiscoveredClearance = aState.mReflowState.mDiscoveredClearance;
    }
    
    nsReflowStatus frameReflowStatus = NS_FRAME_COMPLETE;
    rv = brc.ReflowBlock(availSpace, applyTopMargin, aState.mPrevBottomMargin,
                         clearance, aState.IsAdjacentWithTop(), computedOffsets,
                         blockHtmlRS, frameReflowStatus);

    // If this was a second-pass reflow and the block's vertical position
    // changed, invalidates from the first pass might have happened in the
    // wrong places.  Invalidate the entire overflow rect at the new position.
    if (!mayNeedRetry && clearanceFrame && frame->GetRect().y != originalY) {
      Invalidate(frame->GetOverflowRect() + frame->GetPosition());
    }
    
    NS_ENSURE_SUCCESS(rv, rv);
    
    if (mayNeedRetry && clearanceFrame) {
      UndoSplitPlaceholders(aState, lastPlaceholder);
      aState.mSpaceManager->PopState(&spaceManagerState);
      aState.mY = startingY;
      aState.mPrevBottomMargin = incomingMargin;
      continue;
    }
    
    aState.mPrevChild = frame;
    
#if defined(REFLOW_STATUS_COVERAGE)
    RecordReflowStatus(PR_TRUE, frameReflowStatus);
#endif
    
    if (NS_INLINE_IS_BREAK_BEFORE(frameReflowStatus)) {
      // None of the child block fits.
      UndoSplitPlaceholders(aState, lastPlaceholder);
      PushLines(aState, aLine.prev());
      *aKeepReflowGoing = PR_FALSE;
      aState.mReflowStatus = NS_FRAME_NOT_COMPLETE;
    }
    else {
      // Note: line-break-after a block is a nop
      
      // Try to place the child block.
      // Don't force the block to fit if we have positive clearance, because
      // pushing it to the next page would give it more room.
      // Don't force the block to fit if it's impacted by a float. If it is,
      // then pushing it to the next page would give it more room. Note that
      // isImpacted doesn't include impact from the block's own floats.
      PRBool forceFit = aState.IsAdjacentWithTop() && clearance <= 0 &&
        !isImpacted;
      nsCollapsingMargin collapsedBottomMargin;
      nsRect combinedArea(0,0,0,0);
      *aKeepReflowGoing = brc.PlaceBlock(blockHtmlRS, forceFit, aLine.get(),
                                         computedOffsets, collapsedBottomMargin,
                                         aLine->mBounds, combinedArea, frameReflowStatus);
      if (aLine->SetCarriedOutBottomMargin(collapsedBottomMargin)) {
        line_iterator nextLine = aLine;
        ++nextLine;
        if (nextLine != end_lines()) {
          nextLine->MarkPreviousMarginDirty();
        }
      }
      
      aLine->SetCombinedArea(combinedArea);
      if (*aKeepReflowGoing) {
        // Some of the child block fit
        
        // Advance to new Y position
        nscoord newY = aLine->mBounds.YMost();
        aState.mY = newY;
        
        // Continue the block frame now if it didn't completely fit in
        // the available space.
        if (NS_FRAME_IS_NOT_COMPLETE(frameReflowStatus)) {
          PRBool madeContinuation;
          rv = CreateContinuationFor(aState, nsnull, frame, madeContinuation);
          NS_ENSURE_SUCCESS(rv, rv);
          
          nsIFrame* nextFrame = frame->GetNextInFlow();
          
          // Push continuation to a new line, but only if we actually made one.
          if (madeContinuation) {
            nsLineBox* line = aState.NewLineBox(nextFrame, 1, PR_TRUE);
            NS_ENSURE_TRUE(line, NS_ERROR_OUT_OF_MEMORY);
            mLines.after_insert(aLine, line);
          }
          
          // Advance to next line since some of the block fit. That way
          // only the following lines will be pushed.
          PushLines(aState, aLine);
          aState.mReflowStatus = NS_FRAME_NOT_COMPLETE;
          // If we need to reflow the continuation of the block child,
          // then we'd better reflow our continuation
          if (frameReflowStatus & NS_FRAME_REFLOW_NEXTINFLOW) {
            aState.mReflowStatus |= NS_FRAME_REFLOW_NEXTINFLOW;
            // We also need to make that continuation's line dirty so
            // it gets reflowed when we reflow our next in flow. The
            // nif's line must always be either a line of the nif's
            // parent block (only if we didn't make a continuation) or
            // else one of our own overflow lines. In the latter case
            // the line is already marked dirty, so just handle the
            // first case.
            if (!madeContinuation) {
              nsBlockFrame* nifBlock = NS_STATIC_CAST(nsBlockFrame*, nextFrame->GetParent());
              NS_ASSERTION(nifBlock->GetType() == nsGkAtoms::blockFrame
                           || nifBlock->GetType() == nsGkAtoms::areaFrame,
                           "A block's child's next in flow's parent must be a block!");
              for (line_iterator line = nifBlock->begin_lines(),
                     line_end = nifBlock->end_lines(); line != line_end; ++line) {
                if (line->Contains(nextFrame)) {
                  line->MarkDirty();
                  break;
                }
              }
            }
          }
          *aKeepReflowGoing = PR_FALSE;
          
          // The bottom margin for a block is only applied on the last
          // flow block. Since we just continued the child block frame,
          // we know that line->mFirstChild is not the last flow block
          // therefore zero out the running margin value.
#ifdef NOISY_VERTICAL_MARGINS
          ListTag(stdout);
          printf(": reflow incomplete, frame=");
          nsFrame::ListTag(stdout, frame);
          printf(" prevBottomMargin=%d, setting to zero\n",
                 aState.mPrevBottomMargin);
#endif
          aState.mPrevBottomMargin.Zero();
        }
        else {
#ifdef NOISY_VERTICAL_MARGINS
          ListTag(stdout);
          printf(": reflow complete for ");
          nsFrame::ListTag(stdout, frame);
          printf(" prevBottomMargin=%d collapsedBottomMargin=%d\n",
                 aState.mPrevBottomMargin, collapsedBottomMargin.get());
#endif
          aState.mPrevBottomMargin = collapsedBottomMargin;
        }
#ifdef NOISY_VERTICAL_MARGINS
        ListTag(stdout);
        printf(": frame=");
        nsFrame::ListTag(stdout, frame);
        printf(" carriedOutBottomMargin=%d collapsedBottomMargin=%d => %d\n",
               brc.GetCarriedOutBottomMargin(), collapsedBottomMargin.get(),
               aState.mPrevBottomMargin);
#endif
      }
      else {
        // None of the block fits. Determine the correct reflow status.
        if (aLine == mLines.front()) {
          // If it's our very first line then we need to be pushed to
          // our parents next-in-flow. Therefore, return break-before
          // status for our reflow status.
          aState.mReflowStatus = NS_INLINE_LINE_BREAK_BEFORE();
        }
        else {
          // Push the line that didn't fit and any lines that follow it
          // to our next-in-flow.
          UndoSplitPlaceholders(aState, lastPlaceholder);
          PushLines(aState, aLine.prev());
          aState.mReflowStatus = NS_FRAME_NOT_COMPLETE;
        }
      }
    }
    break; // out of the reflow retry loop
  }
  
#ifdef DEBUG
  VerifyLines(PR_TRUE);
#endif
  return rv;
}

nsresult
nsBlockFrame::ReflowInlineFrames(nsBlockReflowState& aState,
                                 line_iterator aLine,
                                 PRBool* aKeepReflowGoing)
{
  nsresult rv = NS_OK;
  *aKeepReflowGoing = PR_TRUE;

#ifdef DEBUG
  PRInt32 spins = 0;
#endif
  LineReflowStatus lineReflowStatus = LINE_REFLOW_REDO_NEXT_BAND;
  PRBool movedPastFloat = PR_FALSE;
  do {
    PRBool allowPullUp = PR_TRUE;
    nsIContent* forceBreakInContent = nsnull;
    PRInt32 forceBreakOffset = -1;
    do {
      nsSpaceManager::SavedState spaceManagerState;
      aState.mReflowState.mSpaceManager->PushState(&spaceManagerState);

      // Once upon a time we allocated the first 30 nsLineLayout objects
      // on the stack, and then we switched to the heap.  At that time
      // these objects were large (1100 bytes on a 32 bit system).
      // Then the nsLineLayout object was shrunk to 156 bytes by
      // removing some internal buffers.  Given that it is so much
      // smaller, the complexity of 2 different ways of allocating
      // no longer makes sense.  Now we always allocate on the stack
      nsLineLayout lineLayout(aState.mPresContext,
                              aState.mReflowState.mSpaceManager,
                              &aState.mReflowState, &aLine);
      lineLayout.Init(&aState, aState.mMinLineHeight, aState.mLineNumber);
      if (forceBreakInContent) {
        lineLayout.ForceBreakAtPosition(forceBreakInContent, forceBreakOffset);
      }
      rv = DoReflowInlineFrames(aState, lineLayout, aLine,
                                aKeepReflowGoing, &lineReflowStatus,
                                allowPullUp);
      lineLayout.EndLineReflow();

      if (LINE_REFLOW_REDO_NO_PULL == lineReflowStatus ||
          LINE_REFLOW_REDO_NEXT_BAND == lineReflowStatus) {
        if (lineLayout.NeedsBackup()) {
          NS_ASSERTION(!forceBreakInContent, "Backuping up twice; this should never be necessary");
          // If there is no saved break position, then this will set
          // set forceBreakInContent to null and we won't back up, which is
          // correct.
          forceBreakInContent = lineLayout.GetLastOptionalBreakPosition(&forceBreakOffset);
        } else {
          forceBreakInContent = nsnull;
        }
        // restore the space manager state
        aState.mReflowState.mSpaceManager->PopState(&spaceManagerState);
        // Clear out float lists
        aState.mCurrentLineFloats.DeleteAll();
        aState.mBelowCurrentLineFloats.DeleteAll();
      }
      
#ifdef DEBUG
      spins++;
      if (1000 == spins) {
        ListTag(stdout);
        printf(": yikes! spinning on a line over 1000 times!\n");
        NS_ABORT();
      }
#endif

      // Don't allow pullup on a subsequent LINE_REFLOW_REDO_NO_PULL pass
      allowPullUp = PR_FALSE;
    } while (NS_SUCCEEDED(rv) && LINE_REFLOW_REDO_NO_PULL == lineReflowStatus);

    if (LINE_REFLOW_REDO_NEXT_BAND == lineReflowStatus) {
      movedPastFloat = PR_TRUE;
    }
  } while (NS_SUCCEEDED(rv) && LINE_REFLOW_REDO_NEXT_BAND == lineReflowStatus);

  // If we did at least one REDO_FOR_FLOAT, then the line did not fit next to some float.
  // Mark it as impacted by a float, even if it no longer is next to a float.
  if (movedPastFloat) {
    aLine->SetLineIsImpactedByFloat(PR_TRUE);
  }

  return rv;
}

// If at least one float on the line was complete, not at the top of
// page, but was truncated, then restore the overflow floats to what
// they were before and push the line.  The floats that will be removed
// from the list aren't yet known by the block's next in flow.  
void
nsBlockFrame::PushTruncatedPlaceholderLine(nsBlockReflowState& aState,
                                           line_iterator       aLine,
                                           nsIFrame*           aLastPlaceholder,
                                           PRBool&             aKeepReflowGoing)
{
  UndoSplitPlaceholders(aState, aLastPlaceholder);

  line_iterator prevLine = aLine;
  --prevLine;
  PushLines(aState, prevLine);
  aKeepReflowGoing = PR_FALSE;
  aState.mReflowStatus = NS_FRAME_NOT_COMPLETE;
}

#ifdef DEBUG
static const char* LineReflowStatusNames[] = {
  "LINE_REFLOW_OK", "LINE_REFLOW_STOP", "LINE_REFLOW_REDO_NO_PULL",
  "LINE_REFLOW_REDO_NEXT_BAND", "LINE_REFLOW_TRUNCATED"
};
#endif

nsresult
nsBlockFrame::DoReflowInlineFrames(nsBlockReflowState& aState,
                                   nsLineLayout& aLineLayout,
                                   line_iterator aLine,
                                   PRBool* aKeepReflowGoing,
                                   LineReflowStatus* aLineReflowStatus,
                                   PRBool aAllowPullUp)
{
  // Forget all of the floats on the line
  aLine->FreeFloats(aState.mFloatCacheFreeList);
  aState.mFloatCombinedArea.SetRect(0, 0, 0, 0);

  // Setup initial coordinate system for reflowing the inline frames
  // into. Apply a previous block frame's bottom margin first.
  if (ShouldApplyTopMargin(aState, aLine)) {
    aState.mY += aState.mPrevBottomMargin.get();
  }
  aState.GetAvailableSpace();
  PRBool impactedByFloats = aState.IsImpactedByFloat() ? PR_TRUE : PR_FALSE;
  aLine->SetLineIsImpactedByFloat(impactedByFloats);
#ifdef REALLY_NOISY_REFLOW
  printf("nsBlockFrame::DoReflowInlineFrames %p impacted = %d\n",
         this, impactedByFloats);
#endif

  const nsMargin& borderPadding = aState.BorderPadding();
  nscoord x = aState.mAvailSpaceRect.x + borderPadding.left;
  nscoord availWidth = aState.mAvailSpaceRect.width;
  nscoord availHeight;
  if (aState.GetFlag(BRS_UNCONSTRAINEDHEIGHT)) {
    availHeight = NS_UNCONSTRAINEDSIZE;
  }
  else {
    /* XXX get the height right! */
    availHeight = aState.mAvailSpaceRect.height;
  }
  aLineLayout.BeginLineReflow(x, aState.mY,
                              availWidth, availHeight,
                              impactedByFloats,
                              PR_FALSE /*XXX isTopOfPage*/);

  // XXX Unfortunately we need to know this before reflowing the first
  // inline frame in the line. FIX ME.
  if ((0 == aLineLayout.GetLineNumber()) &&
      (NS_BLOCK_HAS_FIRST_LETTER_STYLE & mState)) {
    aLineLayout.SetFirstLetterStyleOK(PR_TRUE);
  }

  // keep track of the last overflow float in case we need to undo any new additions
  nsIFrame* lastPlaceholder = aState.mOverflowPlaceholders.LastChild();

  // Reflow the frames that are already on the line first
  nsresult rv = NS_OK;
  LineReflowStatus lineReflowStatus = LINE_REFLOW_OK;
  PRInt32 i;
  nsIFrame* frame = aLine->mFirstChild;
  aLine->EnableResizeReflowOptimization();

  // Determine whether this is a line of placeholders for out-of-flow
  // continuations
  PRBool isContinuingPlaceholders = PR_FALSE;

  if (impactedByFloats) {
    // There is a soft break opportunity at the start of the line, because
    // we can always move this line down below float(s).
    if (aLineLayout.NotifyOptionalBreakPosition(frame->GetContent(), 0)) {
      lineReflowStatus = LINE_REFLOW_REDO_NEXT_BAND;
    }
  }

  // need to repeatedly call GetChildCount here, because the child
  // count can change during the loop!
  for (i = 0; LINE_REFLOW_OK == lineReflowStatus && i < aLine->GetChildCount();
       i++, frame = frame->GetNextSibling()) {
    if (IsContinuationPlaceholder(frame)) {
      isContinuingPlaceholders = PR_TRUE;
    }
    rv = ReflowInlineFrame(aState, aLineLayout, aLine, frame,
                           &lineReflowStatus);
    NS_ENSURE_SUCCESS(rv, rv);
    if (LINE_REFLOW_OK != lineReflowStatus) {
      // It is possible that one or more of next lines are empty
      // (because of DeleteNextInFlowChild). If so, delete them now
      // in case we are finished.
      ++aLine;
      while ((aLine != end_lines()) && (0 == aLine->GetChildCount())) {
        // XXX Is this still necessary now that DeleteNextInFlowChild
        // uses DoRemoveFrame?
        nsLineBox *toremove = aLine;
        aLine = mLines.erase(aLine);
        NS_ASSERTION(nsnull == toremove->mFirstChild, "bad empty line");
        aState.FreeLineBox(toremove);
      }
      --aLine;

      if (LINE_REFLOW_TRUNCATED == lineReflowStatus) {
        // Push the line with the truncated float 
        PushTruncatedPlaceholderLine(aState, aLine, lastPlaceholder, *aKeepReflowGoing);
      }
    }
  }

  // Don't pull up new frames into lines with continuation placeholders
  if (!isContinuingPlaceholders && aAllowPullUp) {
    // Pull frames and reflow them until we can't
    while (LINE_REFLOW_OK == lineReflowStatus) {
      rv = PullFrame(aState, aLine, frame);
      NS_ENSURE_SUCCESS(rv, rv);
      if (nsnull == frame) {
        break;
      }

      while (LINE_REFLOW_OK == lineReflowStatus) {
        PRInt32 oldCount = aLine->GetChildCount();
        rv = ReflowInlineFrame(aState, aLineLayout, aLine, frame,
                               &lineReflowStatus);
        NS_ENSURE_SUCCESS(rv, rv);
        if (aLine->GetChildCount() != oldCount) {
          // We just created a continuation for aFrame AND its going
          // to end up on this line (e.g. :first-letter
          // situation). Therefore we have to loop here before trying
          // to pull another frame.
          frame = frame->GetNextSibling();
        }
        else {
          break;
        }
      }
    }
  }

  // We only need to backup if the line isn't going to be reflowed again anyway
  PRBool needsBackup = aLineLayout.NeedsBackup() &&
    (lineReflowStatus == LINE_REFLOW_STOP || lineReflowStatus == LINE_REFLOW_OK);
  if (needsBackup && aLineLayout.HaveForcedBreakPosition()) {
  	NS_WARNING("We shouldn't be backing up more than once! "
               "Someone must have set a break opportunity beyond the available width");
    needsBackup = PR_FALSE;
  }
  if (needsBackup) {
    // We need to try backing up to before a text run
    PRInt32 offset;
    nsIContent* breakContent = aLineLayout.GetLastOptionalBreakPosition(&offset);
    if (breakContent) {
      // We can back up!
      lineReflowStatus = LINE_REFLOW_REDO_NO_PULL;
    }
  } else {
    // In case we reflow this line again, remember that we don't
    // need to force any breaking
    aLineLayout.ClearOptionalBreakPosition();
  }

  if (LINE_REFLOW_REDO_NEXT_BAND == lineReflowStatus) {
    // This happens only when we have a line that is impacted by
    // floats and the first element in the line doesn't fit with
    // the floats.
    //
    // What we do is to advance past the first float we find and
    // then reflow the line all over again.
    NS_ASSERTION(aState.IsImpactedByFloat(),
                 "redo line on totally empty line");
    NS_ASSERTION(NS_UNCONSTRAINEDSIZE != aState.mAvailSpaceRect.height,
                 "unconstrained height on totally empty line");

    if (aState.mAvailSpaceRect.height > 0) {
      aState.mY += aState.mAvailSpaceRect.height;
    } else {
      NS_ASSERTION(NS_UNCONSTRAINEDSIZE != aState.mReflowState.availableHeight,
                   "We shouldn't be running out of height here");
      if (NS_UNCONSTRAINEDSIZE == aState.mReflowState.availableHeight) {
        // just move it down a bit to try to get out of this mess
        aState.mY += 1;
      } else {
        // There's nowhere to retry placing the line. Just treat it as if
        // we placed the float but it was truncated so we need this line
        // to go to the next page/column.
        lineReflowStatus = LINE_REFLOW_TRUNCATED;
        // Push the line that didn't fit
        PushTruncatedPlaceholderLine(aState, aLine, lastPlaceholder,
                                     *aKeepReflowGoing);
      }
    }
      
    // We don't want to advance by the bottom margin anymore (we did it
    // once at the beginning of this function, which will just be called
    // again), and we certainly don't want to go back if it's negative
    // (infinite loop, bug 153429).
    aState.mPrevBottomMargin.Zero();

    // XXX: a small optimization can be done here when paginating:
    // if the new Y coordinate is past the end of the block then
    // push the line and return now instead of later on after we are
    // past the float.
  }
  else if (LINE_REFLOW_REDO_NO_PULL == lineReflowStatus) {
    // We don't want to advance by the bottom margin anymore (we did it
    // once at the beginning of this function, which will just be called
    // again), and we certainly don't want to go back if it's negative
    // (infinite loop, bug 153429).
    aState.mPrevBottomMargin.Zero();
  }
  else if (LINE_REFLOW_TRUNCATED != lineReflowStatus) {
    // If we are propagating out a break-before status then there is
    // no point in placing the line.
    if (!NS_INLINE_IS_BREAK_BEFORE(aState.mReflowStatus)) {
      if (PlaceLine(aState, aLineLayout, aLine, aKeepReflowGoing)) {
        UndoSplitPlaceholders(aState, lastPlaceholder); // undo since we pushed the current line
      }
    }
  }
#ifdef DEBUG
  if (gNoisyReflow) {
    printf("Line reflow status = %s\n", LineReflowStatusNames[lineReflowStatus]);
  }
#endif
  *aLineReflowStatus = lineReflowStatus;

  return rv;
}

/**
 * Reflow an inline frame. The reflow status is mapped from the frames
 * reflow status to the lines reflow status (not to our reflow status).
 * The line reflow status is simple: PR_TRUE means keep placing frames
 * on the line; PR_FALSE means don't (the line is done). If the line
 * has some sort of breaking affect then aLine's break-type will be set
 * to something other than NS_STYLE_CLEAR_NONE.
 */
nsresult
nsBlockFrame::ReflowInlineFrame(nsBlockReflowState& aState,
                                nsLineLayout& aLineLayout,
                                line_iterator aLine,
                                nsIFrame* aFrame,
                                LineReflowStatus* aLineReflowStatus)
{
  NS_ENSURE_ARG_POINTER(aFrame);
  
  *aLineReflowStatus = LINE_REFLOW_OK;

  // If it's currently ok to be reflowing in first-letter style then
  // we must be about to reflow a frame that has first-letter style.
  PRBool reflowingFirstLetter = aLineLayout.GetFirstLetterStyleOK();
#ifdef NOISY_FIRST_LETTER
  ListTag(stdout);
  printf(": reflowing ");
  nsFrame::ListTag(stdout, aFrame);
  printf(" reflowingFirstLetter=%s\n", reflowingFirstLetter ? "on" : "off");
#endif

  // Reflow the inline frame
  nsReflowStatus frameReflowStatus;
  PRBool         pushedFrame;
  nsresult rv = aLineLayout.ReflowFrame(aFrame, frameReflowStatus,
                                        nsnull, pushedFrame);

  if (frameReflowStatus & NS_FRAME_REFLOW_NEXTINFLOW) {
    // we need to ensure that the frame's nextinflow gets reflowed.
    aState.mReflowStatus |= NS_FRAME_REFLOW_NEXTINFLOW;
    nsBlockFrame* ourNext = NS_STATIC_CAST(nsBlockFrame*, GetNextInFlow());
    if (ourNext && aFrame->GetNextInFlow()) {
      line_iterator f = ourNext->FindLineFor(aFrame->GetNextInFlow());
      if (f != ourNext->end_lines()) {
        f->MarkDirty();
      }
    }
  }

  NS_ENSURE_SUCCESS(rv, rv);
#ifdef REALLY_NOISY_REFLOW_CHILD
  nsFrame::ListTag(stdout, aFrame);
  printf(": status=%x\n", frameReflowStatus);
#endif

#if defined(REFLOW_STATUS_COVERAGE)
  RecordReflowStatus(PR_FALSE, frameReflowStatus);
#endif

  // Send post-reflow notification
  aState.mPrevChild = aFrame;

   /* XXX
      This is where we need to add logic to handle some odd behavior.
      For one thing, we should usually place at least one thing next
      to a left float, even when that float takes up all the width on a line.
      see bug 22496
   */

  // Process the child frames reflow status. There are 5 cases:
  // complete, not-complete, break-before, break-after-complete,
  // break-after-not-complete. There are two situations: we are a
  // block or we are an inline. This makes a total of 10 cases
  // (fortunately, there is some overlap).
  aLine->SetBreakTypeAfter(NS_STYLE_CLEAR_NONE);
  if (NS_INLINE_IS_BREAK(frameReflowStatus) || 
      (NS_STYLE_CLEAR_NONE != aState.mFloatBreakType)) {
    // Always abort the line reflow (because a line break is the
    // minimal amount of break we do).
    *aLineReflowStatus = LINE_REFLOW_STOP;

    // XXX what should aLine's break-type be set to in all these cases?
    PRUint8 breakType = NS_INLINE_GET_BREAK_TYPE(frameReflowStatus);
    NS_ASSERTION((NS_STYLE_CLEAR_NONE != breakType) || 
                 (NS_STYLE_CLEAR_NONE != aState.mFloatBreakType), "bad break type");
    NS_ASSERTION(NS_STYLE_CLEAR_PAGE != breakType, "no page breaks yet");

    if (NS_INLINE_IS_BREAK_BEFORE(frameReflowStatus)) {
      // Break-before cases.
      if (aFrame == aLine->mFirstChild) {
        // If we break before the first frame on the line then we must
        // be trying to place content where there's no room (e.g. on a
        // line with wide floats). Inform the caller to reflow the
        // line after skipping past a float.
        *aLineReflowStatus = LINE_REFLOW_REDO_NEXT_BAND;
      }
      else {
        // It's not the first child on this line so go ahead and split
        // the line. We will see the frame again on the next-line.
        rv = SplitLine(aState, aLineLayout, aLine, aFrame, aLineReflowStatus);
        NS_ENSURE_SUCCESS(rv, rv);

        // If we're splitting the line because the frame didn't fit and it
        // was pushed, then mark the line as having word wrapped. We need to
        // know that if we're shrink wrapping our width
        if (pushedFrame) {
          aLine->SetLineWrapped(PR_TRUE);
        }
      }
    }
    else {
      // If a float split and its prev-in-flow was followed by a <BR>, then combine 
      // the <BR>'s break type with the inline's break type (the inline will be the very 
      // next frame after the split float).
      if (NS_STYLE_CLEAR_NONE != aState.mFloatBreakType) {
        breakType = nsLayoutUtils::CombineBreakType(breakType,
                                                    aState.mFloatBreakType);
        aState.mFloatBreakType = NS_STYLE_CLEAR_NONE;
      }
      // Break-after cases
      if (breakType == NS_STYLE_CLEAR_LINE) {
        if (!aLineLayout.GetLineEndsInBR()) {
          breakType = NS_STYLE_CLEAR_NONE;
        }
      }
      aLine->SetBreakTypeAfter(breakType);
      if (NS_FRAME_IS_NOT_COMPLETE(frameReflowStatus)) {
        // Create a continuation for the incomplete frame. Note that the
        // frame may already have a continuation.
        PRBool madeContinuation;
        rv = CreateContinuationFor(aState, aLine, aFrame, madeContinuation);
        NS_ENSURE_SUCCESS(rv, rv);
        if (!aLineLayout.GetLineEndsInBR()) {
          // Remember that the line has wrapped
          aLine->SetLineWrapped(PR_TRUE);
        }
      }

      // Split line, but after the frame just reflowed
      rv = SplitLine(aState, aLineLayout, aLine, aFrame->GetNextSibling(), aLineReflowStatus);
      NS_ENSURE_SUCCESS(rv, rv);

      if (NS_FRAME_IS_NOT_COMPLETE(frameReflowStatus) ||
          (NS_INLINE_IS_BREAK_AFTER(frameReflowStatus) &&
           !aLineLayout.GetLineEndsInBR())) {
        // Mark next line dirty in case SplitLine didn't end up
        // pushing any frames.
        nsLineList_iterator next = aLine.next();
        if (next != end_lines() && !next->IsBlock()) {
          next->MarkDirty();
        }
      }
    }
  }
  else if (NS_FRAME_IS_NOT_COMPLETE(frameReflowStatus)) {
    // Frame is not-complete, no special breaking status

    nsIAtom* frameType = aFrame->GetType();

    // Create a continuation for the incomplete frame. Note that the
    // frame may already have a continuation.
    PRBool madeContinuation;
    rv = (nsGkAtoms::placeholderFrame == frameType)
         ? SplitPlaceholder(aState, aFrame)
         : CreateContinuationFor(aState, aLine, aFrame, madeContinuation);
    NS_ENSURE_SUCCESS(rv, rv);

    // Remember that the line has wrapped
    if (!aLineLayout.GetLineEndsInBR()) {
      aLine->SetLineWrapped(PR_TRUE);
    }
    
    // If we are reflowing the first letter frame or a placeholder then 
    // don't split the line and don't stop the line reflow...
    PRBool splitLine = !reflowingFirstLetter && 
      nsGkAtoms::placeholderFrame != frameType;
    if (reflowingFirstLetter) {
      if ((nsGkAtoms::inlineFrame == frameType) ||
          (nsGkAtoms::lineFrame == frameType)) {
        splitLine = PR_TRUE;
      }
    }

    if (splitLine) {
      // Split line after the current frame
      *aLineReflowStatus = LINE_REFLOW_STOP;
      rv = SplitLine(aState, aLineLayout, aLine, aFrame->GetNextSibling(), aLineReflowStatus);
      NS_ENSURE_SUCCESS(rv, rv);

      // Mark next line dirty in case SplitLine didn't end up
      // pushing any frames.
      nsLineList_iterator next = aLine.next();
      if (next != end_lines() && !next->IsBlock()) {
        next->MarkDirty();
      }
    }
  }
  else if (NS_FRAME_IS_TRUNCATED(frameReflowStatus)) {
    // if the frame is a placeholder and was complete but truncated (and not at the top
    // of page), the entire line will be pushed to give it another chance to not truncate.
    if (nsGkAtoms::placeholderFrame == aFrame->GetType()) {
      *aLineReflowStatus = LINE_REFLOW_TRUNCATED;
    }
  }  

  return NS_OK;
}

/**
 * Create a continuation, if necessary, for aFrame. Place it in aLine
 * if aLine is not null. Set aMadeNewFrame to PR_TRUE if a new frame is created.
 */
nsresult
nsBlockFrame::CreateContinuationFor(nsBlockReflowState& aState,
                                    nsLineBox*          aLine,
                                    nsIFrame*           aFrame,
                                    PRBool&             aMadeNewFrame)
{
  aMadeNewFrame = PR_FALSE;
  nsresult rv;
  nsIFrame* nextInFlow;
  rv = CreateNextInFlow(aState.mPresContext, this, aFrame, nextInFlow);
  NS_ENSURE_SUCCESS(rv, rv);
  if (nsnull != nextInFlow) {
    aMadeNewFrame = PR_TRUE;
    if (aLine) { 
      aLine->SetChildCount(aLine->GetChildCount() + 1);
    }
  }
#ifdef DEBUG
  VerifyLines(PR_FALSE);
#endif
  return rv;
}

nsresult
nsBlockFrame::SplitPlaceholder(nsBlockReflowState& aState,
                               nsIFrame*           aPlaceholder)
{
  nsIFrame* nextInFlow;
  nsresult rv = CreateNextInFlow(aState.mPresContext, this, aPlaceholder, nextInFlow);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!nextInFlow) {
    // Next in flow was not created because it already exists.
    return NS_OK;
  }

  // put the sibling list back to what it was before the continuation was created
  nsIFrame *contFrame = aPlaceholder->GetNextSibling();
  nsIFrame *next = contFrame->GetNextSibling();
  aPlaceholder->SetNextSibling(next);
  contFrame->SetNextSibling(nsnull);
  
  NS_ASSERTION(IsContinuationPlaceholder(contFrame),
               "Didn't create the right kind of frame");

  // The new out of flow frame does not get put anywhere; the out-of-flows
  // for placeholders in mOverflowPlaceholders are not kept in any child list
  aState.mOverflowPlaceholders.AppendFrame(this, contFrame);
  return NS_OK;
}

static nsFloatCache*
GetLastFloat(nsLineBox* aLine)
{
  nsFloatCache* fc = aLine->GetFirstFloat();
  while (fc && fc->Next()) {
    fc = fc->Next();
  }
  return fc;
}

static PRBool
CheckPlaceholderInLine(nsIFrame* aBlock, nsLineBox* aLine, nsFloatCache* aFC)
{
  if (!aFC)
    return PR_TRUE;
  for (nsIFrame* f = aFC->mPlaceholder; f; f = f->GetParent()) {
    if (f->GetParent() == aBlock)
      return aLine->Contains(f);
  }
  NS_ASSERTION(PR_FALSE, "aBlock is not an ancestor of aFrame!");
  return PR_TRUE;
}

nsresult
nsBlockFrame::SplitLine(nsBlockReflowState& aState,
                        nsLineLayout& aLineLayout,
                        line_iterator aLine,
                        nsIFrame* aFrame,
                        LineReflowStatus* aLineReflowStatus)
{
  NS_ABORT_IF_FALSE(aLine->IsInline(), "illegal SplitLine on block line");

  PRInt32 pushCount = aLine->GetChildCount() - aLineLayout.GetCurrentSpanCount();
  NS_ABORT_IF_FALSE(pushCount >= 0, "bad push count"); 

#ifdef DEBUG
  if (gNoisyReflow) {
    nsFrame::IndentBy(stdout, gNoiseIndent);
    printf("split line: from line=%p pushCount=%d aFrame=",
           NS_STATIC_CAST(void*, aLine.get()), pushCount);
    if (aFrame) {
      nsFrame::ListTag(stdout, aFrame);
    }
    else {
      printf("(null)");
    }
    printf("\n");
    if (gReallyNoisyReflow) {
      aLine->List(stdout, gNoiseIndent+1);
    }
  }
#endif

  if (0 != pushCount) {
    NS_ABORT_IF_FALSE(aLine->GetChildCount() > pushCount, "bad push");
    NS_ABORT_IF_FALSE(nsnull != aFrame, "whoops");
    NS_ASSERTION(nsFrameList(aFrame).GetLength() >= pushCount,
                 "Not enough frames to push");

    // Put frames being split out into their own line
    nsLineBox* newLine = aState.NewLineBox(aFrame, pushCount, PR_FALSE);
    if (!newLine) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    mLines.after_insert(aLine, newLine);
    aLine->SetChildCount(aLine->GetChildCount() - pushCount);
#ifdef DEBUG
    if (gReallyNoisyReflow) {
      newLine->List(stdout, gNoiseIndent+1);
    }
#endif

    // Let line layout know that some frames are no longer part of its
    // state.
    aLineLayout.SplitLineTo(aLine->GetChildCount());

    // If floats have been placed whose placeholders have been pushed to the new
    // line, we need to reflow the old line again. We don't want to look at the
    // frames in the new line, because as a large paragraph is laid out the 
    // we'd get O(N^2) performance. So instead we just check that the last
    // float and the last below-current-line float are still in aLine.
    if (!CheckPlaceholderInLine(this, aLine, GetLastFloat(aLine)) ||
        !CheckPlaceholderInLine(this, aLine, aState.mBelowCurrentLineFloats.Tail())) {
      *aLineReflowStatus = LINE_REFLOW_REDO_NO_PULL;
    }

#ifdef DEBUG
    VerifyLines(PR_TRUE);
#endif
  }
  return NS_OK;
}

PRBool
nsBlockFrame::ShouldJustifyLine(nsBlockReflowState& aState,
                                line_iterator aLine)
{
  while (++aLine != end_lines()) {
    // There is another line
    if (0 != aLine->GetChildCount()) {
      // If the next line is a block line then we must not justify
      // this line because it means that this line is the last in a
      // group of inline lines.
      return !aLine->IsBlock();
    }
    // The next line is empty, try the next one
  }

  // XXX Not sure about this part
  // Try our next-in-flows lines to answer the question
  nsBlockFrame* nextInFlow = (nsBlockFrame*) GetNextInFlow();
  while (nsnull != nextInFlow) {
    for (line_iterator line = nextInFlow->begin_lines(),
                   line_end = nextInFlow->end_lines();
         line != line_end;
         ++line)
    {
      if (0 != line->GetChildCount())
        return !line->IsBlock();
    }
    nextInFlow = (nsBlockFrame*) nextInFlow->GetNextInFlow();
  }

  // This is the last line - so don't allow justification
  return PR_FALSE;
}

PRBool
nsBlockFrame::PlaceLine(nsBlockReflowState& aState,
                        nsLineLayout&       aLineLayout,
                        line_iterator       aLine,
                        PRBool*             aKeepReflowGoing)
{
  // Trim extra white-space from the line before placing the frames
  aLineLayout.TrimTrailingWhiteSpace();

  // Vertically align the frames on this line.
  //
  // According to the CSS2 spec, section 12.6.1, the "marker" box
  // participates in the height calculation of the list-item box's
  // first line box.
  //
  // There are exactly two places a bullet can be placed: near the
  // first or second line. It's only placed on the second line in a
  // rare case: an empty first line followed by a second line that
  // contains a block (example: <LI>\n<P>... ).
  //
  // For this code, only the first case is possible because this
  // method is used for placing a line of inline frames. If the rare
  // case is happening then the worst that will happen is that the
  // bullet frame will be reflowed twice.
  PRBool addedBullet = PR_FALSE;
  if (mBullet && HaveOutsideBullet() && (aLine == mLines.front()) &&
      (!aLineLayout.IsZeroHeight() || (aLine == mLines.back()))) {
    nsHTMLReflowMetrics metrics;
    ReflowBullet(aState, metrics);
    aLineLayout.AddBulletFrame(mBullet, metrics);
    addedBullet = PR_TRUE;
  }
  aLineLayout.VerticalAlignLine();

#ifdef DEBUG
  {
    static nscoord lastHeight = 0;
    if (CRAZY_HEIGHT(aLine->mBounds.y)) {
      lastHeight = aLine->mBounds.y;
      if (abs(aLine->mBounds.y - lastHeight) > CRAZY_H/10) {
        nsFrame::ListTag(stdout);
        printf(": line=%p y=%d line.bounds.height=%d\n",
               NS_STATIC_CAST(void*, aLine.get()),
               aLine->mBounds.y, aLine->mBounds.height);
      }
    }
    else {
      lastHeight = 0;
    }
  }
#endif

  // Only block frames horizontally align their children because
  // inline frames "shrink-wrap" around their children (therefore
  // there is no extra horizontal space).
  const nsStyleText* styleText = GetStyleText();
  PRBool allowJustify = NS_STYLE_TEXT_ALIGN_JUSTIFY == styleText->mTextAlign &&
                        !aLineLayout.GetLineEndsInBR() &&
                        ShouldJustifyLine(aState, aLine);
  aLineLayout.HorizontalAlignFrames(aLine->mBounds, allowJustify);
  // XXX: not only bidi: right alignment can be broken after
  // RelativePositionFrames!!!
  // XXXldb Is something here considering relatively positioned frames at
  // other than their original positions?
#ifdef IBMBIDI
  // XXXldb Why don't we do this earlier?
  if (aState.mPresContext->BidiEnabled()) {
    if (!aState.mPresContext->IsVisualMode()) {
      nsBidiPresUtils* bidiUtils = aState.mPresContext->GetBidiUtils();

      if (bidiUtils && bidiUtils->IsSuccessful() ) {
        bidiUtils->ReorderFrames(aState.mPresContext,
                                 aState.mReflowState.rendContext,
                                 aLine->mFirstChild, aLine->GetChildCount());
      } // bidiUtils
    } // not visual mode
  } // bidi enabled
#endif // IBMBIDI

  nsRect combinedArea;
  aLineLayout.RelativePositionFrames(combinedArea);  // XXXldb This returned width as -15, 2001-06-12, Bugzilla
  aLine->SetCombinedArea(combinedArea);
  if (addedBullet) {
    aLineLayout.RemoveBulletFrame(mBullet);
  }

  // Inline lines do not have margins themselves; however they are
  // impacted by prior block margins. If this line ends up having some
  // height then we zero out the previous bottom margin value that was
  // already applied to the line's starting Y coordinate. Otherwise we
  // leave it be so that the previous blocks bottom margin can be
  // collapsed with a block that follows.
  nscoord newY;

  if (!aLine->CachedIsEmpty()) {
    // This line has some height. Therefore the application of the
    // previous-bottom-margin should stick.
    aState.mPrevBottomMargin.Zero();
    newY = aLine->mBounds.YMost();
  }
  else {
    // Don't let the previous-bottom-margin value affect the newY
    // coordinate (it was applied in ReflowInlineFrames speculatively)
    // since the line is empty.
    // We already called |ShouldApplyTopMargin|, and if we applied it
    // then BRS_APPLYTOPMARGIN is set.
    nscoord dy = aState.GetFlag(BRS_APPLYTOPMARGIN)
                   ? -aState.mPrevBottomMargin.get() : 0;
    newY = aState.mY + dy;
    aLine->SlideBy(dy); // XXXldb Do we really want to do this?
  }

  // See if the line fit. If it doesn't we need to push it. Our first
  // line will always fit.
  if (mLines.front() != aLine &&
      newY > aState.mBottomEdge &&
      aState.mBottomEdge != NS_UNCONSTRAINEDSIZE) {
    // Push this line and all of its children and anything else that
    // follows to our next-in-flow
    NS_ASSERTION((aState.mCurrentLine == aLine), "oops");
    PushLines(aState, aLine.prev());

    // Stop reflow and whack the reflow status if reflow hasn't
    // already been stopped.
    if (*aKeepReflowGoing) {
      aState.mReflowStatus |= NS_FRAME_NOT_COMPLETE;
      *aKeepReflowGoing = PR_FALSE;
    }
    return PR_TRUE;
  }

  // May be needed below
  PRBool wasAdjacentWIthTop = aState.IsAdjacentWithTop();

  aState.mY = newY;
  
  // Add the already placed current-line floats to the line
  aLine->AppendFloats(aState.mCurrentLineFloats);

  // Any below current line floats to place?
  if (aState.mBelowCurrentLineFloats.NotEmpty()) {
    // Reflow the below-current-line floats, then add them to the
    // lines float list if there aren't any truncated floats.
    if (aState.PlaceBelowCurrentLineFloats(aState.mBelowCurrentLineFloats,
                                           wasAdjacentWIthTop)) {
      aLine->AppendFloats(aState.mBelowCurrentLineFloats);
    }
    else { 
      // At least one float is truncated, so fix up any placeholders that got split and 
      // push the line. XXX It may be better to put the float on the next line, but this 
      // is not common enough to justify the complexity. Or maybe it is now...

      nsIFrame* lastPlaceholder = aState.mOverflowPlaceholders.LastChild();
      PushTruncatedPlaceholderLine(aState, aLine, lastPlaceholder, *aKeepReflowGoing);
    }
  }

  // When a line has floats, factor them into the combined-area
  // computations.
  if (aLine->HasFloats()) {
    // Combine the float combined area (stored in aState) and the
    // value computed by the line layout code.
    nsRect lineCombinedArea(aLine->GetCombinedArea());
#ifdef NOISY_COMBINED_AREA
    ListTag(stdout);
    printf(": lineCA=%d,%d,%d,%d floatCA=%d,%d,%d,%d\n",
           lineCombinedArea.x, lineCombinedArea.y,
           lineCombinedArea.width, lineCombinedArea.height,
           aState.mFloatCombinedArea.x, aState.mFloatCombinedArea.y,
           aState.mFloatCombinedArea.width,
           aState.mFloatCombinedArea.height);
#endif
    lineCombinedArea.UnionRect(aState.mFloatCombinedArea, lineCombinedArea);

    aLine->SetCombinedArea(lineCombinedArea);
#ifdef NOISY_COMBINED_AREA
    printf("  ==> final lineCA=%d,%d,%d,%d\n",
           lineCombinedArea.x, lineCombinedArea.y,
           lineCombinedArea.width, lineCombinedArea.height);
#endif
  }

  // Apply break-after clearing if necessary
  // This must stay in sync with |ReflowDirtyLines|.
  if (aLine->HasFloatBreakAfter()) {
    aState.mY = aState.ClearFloats(aState.mY, aLine->GetBreakTypeAfter());
  }

  return PR_FALSE;
}

void
nsBlockFrame::PushLines(nsBlockReflowState&  aState,
                        nsLineList::iterator aLineBefore)
{
  nsLineList::iterator overBegin(aLineBefore.next());

  // PushTruncatedPlaceholderLine sometimes pushes the first line.  Ugh.
  PRBool firstLine = overBegin == begin_lines();

  if (overBegin != end_lines()) {
    // Remove floats in the lines from mFloats
    nsFrameList floats;
    nsIFrame* tail = nsnull;
    CollectFloats(overBegin->mFirstChild, floats, &tail, PR_FALSE);

    if (floats.NotEmpty()) {
      // Push the floats onto the front of the overflow out-of-flows list
      nsFrameList oofs = GetOverflowOutOfFlows();
      if (oofs.NotEmpty()) {
        floats.InsertFrames(nsnull, tail, oofs.FirstChild());
      }
      SetOverflowOutOfFlows(floats);
    }

    // overflow lines can already exist in some cases, in particular,
    // when shrinkwrapping and we discover that the shrinkwap causes
    // the height of some child block to grow which creates additional
    // overflowing content. In such cases we must prepend the new
    // overflow to the existing overflow.
    nsLineList* overflowLines = RemoveOverflowLines();
    if (!overflowLines) {
      // XXXldb use presshell arena!
      overflowLines = new nsLineList();
    }
    if (overflowLines) {
      if (!overflowLines->empty()) {
        mLines.back()->LastChild()->SetNextSibling(overflowLines->front()->mFirstChild);
      }
      overflowLines->splice(overflowLines->begin(), mLines, overBegin,
                            end_lines());
      NS_ASSERTION(!overflowLines->empty(), "should not be empty");
      // this takes ownership but it won't delete it immediately so we
      // can keep using it.
      SetOverflowLines(overflowLines);
  
      // Mark all the overflow lines dirty so that they get reflowed when
      // they are pulled up by our next-in-flow.

      // XXXldb Can this get called O(N) times making the whole thing O(N^2)?
      for (line_iterator line = overflowLines->begin(),
             line_end = overflowLines->end();
           line != line_end;
           ++line)
        {
          line->MarkDirty();
          line->MarkPreviousMarginDirty();
          line->mBounds.SetRect(0, 0, 0, 0);
          if (line->HasFloats()) {
            line->FreeFloats(aState.mFloatCacheFreeList);
          }
        }
    }
  }

  // Break frame sibling list
  if (!firstLine)
    aLineBefore->LastChild()->SetNextSibling(nsnull);

#ifdef DEBUG
  VerifyOverflowSituation();
#endif
}

/**
 * Call this when a frame will be pulled from the block's
 * next-in-flow into this frame. If it's a continuation placeholder,
 * it should not be here so we push it into our overflow placeholders
 * list. To avoid creating holes (e.g., the following block doesn't
 * have a placeholder but the block after it does) we also need to
 * pull all the following placeholders and put them in our overflow
 * placeholders list too.
 *
 * If it's a first-in-flow placeholder, or it contains one, then we
 * need to do this to the continuation placeholders.
 *
 * We return PR_TRUE if we removed the frame and it cannot be used.
 * If we return PR_FALSE then the frame *must* be pulled immediately.
 */
PRBool
nsBlockFrame::HandleOverflowPlaceholdersForPulledFrame(
  nsBlockReflowState& aState, nsIFrame* aFrame)
{
  if (nsGkAtoms::placeholderFrame != aFrame->GetType()) {
    // Descend into children that are not float containing blocks.
    // We should encounter only first-in-flow placeholders, so the
    // frame subtree rooted at aFrame should not change.
    if (!aFrame->IsFloatContainingBlock()) {
      for (nsIFrame* f = aFrame->GetFirstChild(nsnull); f; f = f->GetNextSibling()) {
#ifdef DEBUG
        PRBool changed =
#endif
          HandleOverflowPlaceholdersForPulledFrame(aState, f);
        NS_ASSERTION(!changed, "Shouldn't find any continuation placeholders inside inlines");
      }
    }
    return PR_FALSE;
  }

  PRBool taken = PR_TRUE;
  nsIFrame* frame = aFrame;
  if (!aFrame->GetPrevInFlow()) {
    // First in flow frame. We only want to deal with its
    // next in flows, if there are any.
    taken = PR_FALSE;
    frame = frame->GetNextInFlow();
    if (!frame)
      return PR_FALSE;
  }

  nsBlockFrame* parent = NS_STATIC_CAST(nsBlockFrame*, frame->GetParent());
  // Remove aFrame and all its next in flows from their parents, but
  // don't destroy the frames.
#ifdef DEBUG
  nsresult rv =
#endif
    parent->DoRemoveFrame(frame, PR_FALSE);
  NS_ASSERTION(NS_SUCCEEDED(rv), "frame should be in parent's lists");
  
  nsIFrame* lastOverflowPlace = aState.mOverflowPlaceholders.LastChild();
  while (frame) {
    NS_ASSERTION(IsContinuationPlaceholder(frame),
                 "Should only be dealing with continuation placeholders here");

    parent = NS_STATIC_CAST(nsBlockFrame*, frame->GetParent());
    ReparentFrame(frame, parent, this);

    // continuation placeholders are always direct children of a block
    nsIFrame* outOfFlow = nsPlaceholderFrame::GetRealFrameForPlaceholder(frame);

    if (!parent->mFloats.RemoveFrame(outOfFlow)) {
      nsAutoOOFFrameList oofs(parent);
#ifdef DEBUG
      PRBool found =
#endif
        oofs.mList.RemoveFrame(outOfFlow);
      NS_ASSERTION(found, "Must have the out of flow in some child list");
    }
    ReparentFrame(outOfFlow, parent, this);

    aState.mOverflowPlaceholders.InsertFrames(nsnull, lastOverflowPlace, frame);
    // outOfFlow isn't inserted anywhere yet. Eventually the overflow
    // placeholders get put into the overflow lines, and at the same time we
    // insert the placeholders' out of flows into the overflow out-of-flows
    // list.
    lastOverflowPlace = frame;

    frame = frame->GetNextInFlow();
  }

  return taken;
}

/**
 * Call this when a line will be pulled from the block's
 * next-in-flow's line.
 *
 * @return PR_TRUE we consumed the entire line, delete it and try again
 */
PRBool
nsBlockFrame::HandleOverflowPlaceholdersOnPulledLine(
  nsBlockReflowState& aState, nsLineBox* aLine)
{
  // First, see if it's a line of continuation placeholders. If it
  // is, remove one and retry.
  if (aLine->mFirstChild && IsContinuationPlaceholder(aLine->mFirstChild)) {
    PRBool taken =
      HandleOverflowPlaceholdersForPulledFrame(aState, aLine->mFirstChild);
    NS_ASSERTION(taken, "We must have removed that frame");
    return PR_TRUE;
  }
 
  // OK, it's a normal line. Scan it for floats with continuations that
  // need to be taken care of. We won't need to change the line.
  PRInt32 n = aLine->GetChildCount();
  for (nsIFrame* f = aLine->mFirstChild; n > 0; f = f->GetNextSibling(), --n) {
#ifdef DEBUG
    PRBool taken =
#endif
      HandleOverflowPlaceholdersForPulledFrame(aState, f);
    NS_ASSERTION(!taken, "Shouldn't be any continuation placeholders on this line");
  }

  return PR_FALSE;
}

// The overflowLines property is stored as a pointer to a line list,
// which must be deleted.  However, the following functions all maintain
// the invariant that the property is never set if the list is empty.

PRBool
nsBlockFrame::DrainOverflowLines(nsBlockReflowState& aState)
{
#ifdef DEBUG
  VerifyOverflowSituation();
#endif
  nsLineList* overflowLines = nsnull;
  nsLineList* ourOverflowLines = nsnull;

  // First grab the prev-in-flows overflow lines
  nsBlockFrame* prevBlock = (nsBlockFrame*) GetPrevInFlow();
  if (prevBlock) {
    overflowLines = prevBlock->RemoveOverflowLines();
    if (overflowLines) {
      NS_ASSERTION(! overflowLines->empty(),
                   "overflow lines should never be set and empty");
      // Make all the frames on the overflow line list mine
      nsIFrame* frame = overflowLines->front()->mFirstChild;
      while (nsnull != frame) {
        ReparentFrame(frame, prevBlock, this);

        // Get the next frame
        frame = frame->GetNextSibling();
      }

      // make the overflow out-of-flow frames mine too
      nsAutoOOFFrameList oofs(prevBlock);
      if (oofs.mList.NotEmpty()) {
        for (nsIFrame* f = oofs.mList.FirstChild(); f; f = f->GetNextSibling()) {
          ReparentFrame(f, prevBlock, this);
        }
        mFloats.InsertFrames(nsnull, nsnull, oofs.mList.FirstChild());
        oofs.mList.SetFrames(nsnull);
      }
    }
    
    // The lines on the overflow list have already been marked dirty and their
    // previous margins marked dirty also.
  }

  // Don't need to reparent frames in our own overflow lines/oofs, because they're
  // already ours. But we should put overflow floats back in mFloats.
  ourOverflowLines = RemoveOverflowLines();
  if (ourOverflowLines) {
    nsAutoOOFFrameList oofs(this);
    if (oofs.mList.NotEmpty()) {
      // The overflow floats go after our regular floats
      mFloats.AppendFrames(nsnull, oofs.mList.FirstChild());
      oofs.mList.SetFrames(nsnull);
    }
  }

  if (!overflowLines && !ourOverflowLines) {
    // nothing to do; always the case for non-constrained-height reflows
    return PR_FALSE;
  }

  NS_ASSERTION(aState.mOverflowPlaceholders.IsEmpty(),
               "Should have no overflow placeholders yet");

  // HANDLING CONTINUATION PLACEHOLDERS (floats only at the moment, because
  // abs-pos frames don't have continuations)
  //
  // All continuation placeholders need to be moved to the front of
  // our line list. We also need to maintain the invariant that at
  // most one frame for a given piece of content is in our normal
  // child list, by pushing all but the first placeholder to our
  // overflow placeholders list.
  // 
  // One problem we have to deal with is that some of these
  // continuation placeholders may have been donated to us by a
  // descendant block that was complete. We need to push them down to
  // a lower block if possible.
  //
  // We keep the lists ordered so that prev in flows come before their
  // next in flows. We do not worry about properly ordering the
  // placeholders for different content relative to each other until
  // the end. Then we sort them.
  //
  // When we're shuffling placeholders we also need to shuffle their out of
  // flows to match. As we put placeholders into keepPlaceholders, we unhook
  // their floats from mFloats. Later we put the floats back based on the
  // order of the placeholders.
  nsIFrame* lastOP = nsnull;
  nsFrameList keepPlaceholders;
  nsFrameList keepOutOfFlows;
  nsIFrame* lastKP = nsnull;
  nsIFrame* lastKOOF = nsnull;
  nsLineList* lineLists[3] = { overflowLines, &mLines, ourOverflowLines };
  static const PRPackedBool searchFirstLinesOnly[3] = { PR_FALSE, PR_TRUE, PR_FALSE };
  for (PRInt32 i = 0; i < 3; ++i) {
    nsLineList* ll = lineLists[i];
    if (ll && !ll->empty()) {
      line_iterator iter = ll->begin();
      line_iterator iter_end = ll->end();
      nsIFrame* lastFrame = nsnull;
      while (iter != iter_end) {
        PRUint32 n = iter->GetChildCount();
        if (n == 0 || !IsContinuationPlaceholder(iter->mFirstChild)) {
          if (lastFrame) {
            lastFrame->SetNextSibling(iter->mFirstChild);
          }
          if (searchFirstLinesOnly[i]) {
            break;
          }
          lastFrame = iter->LastChild();
          ++iter;
        } else {
          nsLineBox* line = iter;
          iter = ll->erase(iter);
          nsIFrame* next;
          for (nsPlaceholderFrame* f = NS_STATIC_CAST(nsPlaceholderFrame*, line->mFirstChild);
               n > 0; --n, f = NS_STATIC_CAST(nsPlaceholderFrame*, next)) {
            if (!IsContinuationPlaceholder(f)) {
              NS_ASSERTION(IsContinuationPlaceholder(f),
                           "Line frames should all be continuation placeholders");
            }
            next = f->GetNextSibling();
            nsIFrame* fpif = f->GetPrevInFlow();
            nsIFrame* oof = f->GetOutOfFlowFrame();
            
            // Take this out of mFloats for now. We may put it back later in
            // this function
#ifdef DEBUG
            PRBool found =
#endif
              mFloats.RemoveFrame(oof);
            NS_ASSERTION(found, "Float should have been put in our mFloats list");

            PRBool isAncestor = nsLayoutUtils::IsProperAncestorFrame(this, fpif);
            if (isAncestor) {
              // oops. we already have a prev-in-flow for this
              // placeholder. We have to move this frame out of here. We
              // can put it in our overflow placeholders.
              aState.mOverflowPlaceholders.InsertFrame(nsnull, lastOP, f);
              // Let oof dangle for now, because placeholders in
              // mOverflowPlaceholders do not keep their floats in any child list
              lastOP = f;
            } else {
              if (fpif->GetParent() == prevBlock) {
                keepPlaceholders.InsertFrame(nsnull, lastKP, f);
                keepOutOfFlows.InsertFrame(nsnull, lastKOOF, oof);
                lastKP = f;
                lastKOOF = oof;
              } else {
                // Ok, now we're in the tough situation where some child
                // of prevBlock was complete and pushed its overflow
                // placeholders up to prevBlock's overflow. We might be
                // able to find a more appropriate parent for f somewhere
                // down in our descendants.
                NS_ASSERTION(nsLayoutUtils::IsProperAncestorFrame(prevBlock, fpif),
                             "bad prev-in-flow ancestor chain");
                // Find the first ancestor of f's prev-in-flow that has a next in flow
                // that can contain the float.
                // That next in flow should become f's parent.
                nsIFrame* fpAncestor;
                for (fpAncestor = fpif->GetParent();
                     !fpAncestor->GetNextInFlow() || !fpAncestor->IsFloatContainingBlock();
                     fpAncestor = fpAncestor->GetParent())
                  ;
                if (fpAncestor == prevBlock) {
                  // We're still the best parent.
                  keepPlaceholders.InsertFrame(nsnull, lastKP, f);
                  keepOutOfFlows.InsertFrame(nsnull, lastKOOF, oof);
                  lastKP = f;
                  lastKOOF = oof;
                } else {
                  // Just put it at the front of
                  // fpAncestor->GetNextInFlow()'s lines.
                  nsLineBox* newLine = aState.NewLineBox(f, 1, PR_FALSE);
                  if (newLine) {
                    nsBlockFrame* target =
                      NS_STATIC_CAST(nsBlockFrame*, fpAncestor->GetNextInFlow());
                    if (!target->mLines.empty()) {
                      f->SetNextSibling(target->mLines.front()->mFirstChild);
                    } else {
                      f->SetNextSibling(nsnull);
                    }
                    target->mLines.push_front(newLine);
                    ReparentFrame(f, this, target);

                    target->mFloats.InsertFrame(nsnull, nsnull, oof);
                    ReparentFrame(oof, this, target);
                  }
                }
              }
            }
          }
          aState.FreeLineBox(line);
        }
      }
      if (lastFrame) {
        lastFrame->SetNextSibling(nsnull);
      }
    }
  }

  // Now join the line lists into mLines
  if (overflowLines) {
    if (!overflowLines->empty()) {
      // Join the line lists
      if (! mLines.empty()) 
        {
          // Remember to recompute the margins on the first line. This will
          // also recompute the correct deltaY if necessary.
          mLines.front()->MarkPreviousMarginDirty();
          // Join the sibling lists together
          nsIFrame* lastFrame = overflowLines->back()->LastChild();
          lastFrame->SetNextSibling(mLines.front()->mFirstChild);
        }
      // Place overflow lines at the front of our line list
      mLines.splice(mLines.begin(), *overflowLines);
      NS_ASSERTION(overflowLines->empty(), "splice should empty list");
    }
    delete overflowLines;
  }
  if (ourOverflowLines) {
    if (!ourOverflowLines->empty()) {
      if (!mLines.empty()) {
        mLines.back()->LastChild()->
          SetNextSibling(ourOverflowLines->front()->mFirstChild);
      }
      // append the overflow to mLines
      mLines.splice(mLines.end(), *ourOverflowLines);
    }
    delete ourOverflowLines;
  }

  // store the placeholders that we're keeping in our frame list
  if (keepPlaceholders.NotEmpty()) {
    keepPlaceholders.SortByContentOrder();
    nsLineBox* newLine = aState.NewLineBox(keepPlaceholders.FirstChild(),
                                           keepPlaceholders.GetLength(), PR_FALSE);
    if (newLine) {
      if (!mLines.empty()) {
        keepPlaceholders.LastChild()->SetNextSibling(mLines.front()->mFirstChild);
      }
      mLines.push_front(newLine);
    }

    // Put the placeholders' out of flows into the float list
    keepOutOfFlows.SortByContentOrder();
    mFloats.InsertFrames(nsnull, nsnull, keepOutOfFlows.FirstChild());
  }

  return PR_TRUE;
}

nsLineList*
nsBlockFrame::GetOverflowLines() const
{
  if (!(GetStateBits() & NS_BLOCK_HAS_OVERFLOW_LINES)) {
    return nsnull;
  }
  nsLineList* lines = NS_STATIC_CAST(nsLineList*,
    GetProperty(nsGkAtoms::overflowLinesProperty));
  NS_ASSERTION(lines && !lines->empty(),
               "value should always be stored and non-empty when state set");
  return lines;
}

nsLineList*
nsBlockFrame::RemoveOverflowLines()
{
  if (!(GetStateBits() & NS_BLOCK_HAS_OVERFLOW_LINES)) {
    return nsnull;
  }
  nsLineList* lines = NS_STATIC_CAST(nsLineList*,
    UnsetProperty(nsGkAtoms::overflowLinesProperty));
  NS_ASSERTION(lines && !lines->empty(),
               "value should always be stored and non-empty when state set");
  RemoveStateBits(NS_BLOCK_HAS_OVERFLOW_LINES);
  return lines;
}

// Destructor function for the overflowLines frame property
static void
DestroyOverflowLines(void*           aFrame,
                     nsIAtom*        aPropertyName,
                     void*           aPropertyValue,
                     void*           aDtorData)
{
  if (aPropertyValue) {
    nsLineList* lines = NS_STATIC_CAST(nsLineList*, aPropertyValue);
    nsPresContext *context = NS_STATIC_CAST(nsPresContext*, aDtorData);
    nsLineBox::DeleteLineList(context, *lines);
    delete lines;
  }
}

// This takes ownership of aOverflowLines.
// XXX We should allocate overflowLines from presShell arena!
nsresult
nsBlockFrame::SetOverflowLines(nsLineList* aOverflowLines)
{
  NS_ASSERTION(aOverflowLines, "null lines");
  NS_ASSERTION(!aOverflowLines->empty(), "empty lines");
  NS_ASSERTION(!(GetStateBits() & NS_BLOCK_HAS_OVERFLOW_LINES),
               "Overwriting existing overflow lines");

  nsPresContext *presContext = GetPresContext();
  nsresult rv = presContext->PropertyTable()->
    SetProperty(this, nsGkAtoms::overflowLinesProperty, aOverflowLines,
                DestroyOverflowLines, presContext);
  // Verify that we didn't overwrite an existing overflow list
  NS_ASSERTION(rv != NS_PROPTABLE_PROP_OVERWRITTEN, "existing overflow list");
  AddStateBits(NS_BLOCK_HAS_OVERFLOW_LINES);
  return rv;
}

nsFrameList
nsBlockFrame::GetOverflowOutOfFlows() const
{
  if (!(GetStateBits() & NS_BLOCK_HAS_OVERFLOW_OUT_OF_FLOWS)) {
    return nsFrameList();
  }
  nsIFrame* result = NS_STATIC_CAST(nsIFrame*,
    GetProperty(nsGkAtoms::overflowOutOfFlowsProperty));
  NS_ASSERTION(result, "value should always be non-empty when state set");
  return nsFrameList(result);
}

// This takes ownership of the frames
void
nsBlockFrame::SetOverflowOutOfFlows(const nsFrameList& aList)
{
  if (aList.IsEmpty()) {
    if (!(GetStateBits() & NS_BLOCK_HAS_OVERFLOW_OUT_OF_FLOWS)) {
      return;
    }
    nsIFrame* result = NS_STATIC_CAST(nsIFrame*,
                                      UnsetProperty(nsGkAtoms::overflowOutOfFlowsProperty));
    NS_ASSERTION(result, "value should always be non-empty when state set");
    RemoveStateBits(NS_BLOCK_HAS_OVERFLOW_OUT_OF_FLOWS);
  } else {
    SetProperty(nsGkAtoms::overflowOutOfFlowsProperty,
                aList.FirstChild(), nsnull);
    AddStateBits(NS_BLOCK_HAS_OVERFLOW_OUT_OF_FLOWS);
  }
}

nsFrameList*
nsBlockFrame::GetOverflowPlaceholders() const
{
  if (!(GetStateBits() & NS_BLOCK_HAS_OVERFLOW_PLACEHOLDERS)) {
    return nsnull;
  }
  nsFrameList* result = NS_STATIC_CAST(nsFrameList*,
    GetProperty(nsGkAtoms::overflowPlaceholdersProperty));
  NS_ASSERTION(result, "value should always be non-empty when state set");
  return result;
}

//////////////////////////////////////////////////////////////////////
// Frame list manipulation routines

nsIFrame*
nsBlockFrame::LastChild()
{
  if (! mLines.empty()) {
    return mLines.back()->LastChild();
  }
  return nsnull;
}

NS_IMETHODIMP
nsBlockFrame::AppendFrames(nsIAtom*  aListName,
                           nsIFrame* aFrameList)
{
  if (nsnull == aFrameList) {
    return NS_OK;
  }
  if (aListName) {
    if (mAbsoluteContainer.GetChildListName() == aListName) {
      return mAbsoluteContainer.AppendFrames(this, aListName, aFrameList);
    }
    else if (nsGkAtoms::floatList == aListName) {
      mFloats.AppendFrames(nsnull, aFrameList);
      return NS_OK;
    }
    else {
      NS_ERROR("unexpected child list");
      return NS_ERROR_INVALID_ARG;
    }
  }

  // Find the proper last-child for where the append should go
  nsIFrame* lastKid = nsnull;
  nsLineBox* lastLine = mLines.empty() ? nsnull : mLines.back();
  if (lastLine) {
    lastKid = lastLine->LastChild();
  }

  // Add frames after the last child
#ifdef NOISY_REFLOW_REASON
  ListTag(stdout);
  printf(": append ");
  nsFrame::ListTag(stdout, aFrameList);
  if (lastKid) {
    printf(" after ");
    nsFrame::ListTag(stdout, lastKid);
  }
  printf("\n");
#endif
  nsresult rv = AddFrames(aFrameList, lastKid);
  if (NS_SUCCEEDED(rv)) {
    AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN); // XXX sufficient?
    GetPresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eTreeChange);
  }
  return rv;
}

NS_IMETHODIMP
nsBlockFrame::InsertFrames(nsIAtom*  aListName,
                           nsIFrame* aPrevFrame,
                           nsIFrame* aFrameList)
{
  NS_ASSERTION(!aPrevFrame || aPrevFrame->GetParent() == this,
               "inserting after sibling frame with different parent");

  if (aListName) {
    if (mAbsoluteContainer.GetChildListName() == aListName) {
      return mAbsoluteContainer.InsertFrames(this, aListName, aPrevFrame,
                                             aFrameList);
    }
    else if (nsGkAtoms::floatList == aListName) {
      mFloats.InsertFrames(this, aPrevFrame, aFrameList);
      return NS_OK;
    }
#ifdef IBMBIDI
    else if (nsGkAtoms::nextBidi == aListName) {}
#endif // IBMBIDI
    else {
      NS_ERROR("unexpected child list");
      return NS_ERROR_INVALID_ARG;
    }
  }

#ifdef NOISY_REFLOW_REASON
  ListTag(stdout);
  printf(": insert ");
  nsFrame::ListTag(stdout, aFrameList);
  if (aPrevFrame) {
    printf(" after ");
    nsFrame::ListTag(stdout, aPrevFrame);
  }
  printf("\n");
#endif
  nsresult rv = AddFrames(aFrameList, aPrevFrame);
#ifdef IBMBIDI
  if (aListName != nsGkAtoms::nextBidi)
#endif // IBMBIDI
  if (NS_SUCCEEDED(rv)) {
    AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN); // XXX sufficient?
    GetPresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eTreeChange);
  }
  return rv;
}

static PRBool
ShouldPutNextSiblingOnNewLine(nsIFrame* aLastFrame)
{
  nsIAtom* type = aLastFrame->GetType();
  if (type == nsGkAtoms::brFrame)
    return PR_TRUE;
  if (type == nsGkAtoms::textFrame)
    return aLastFrame->HasTerminalNewline() &&
           aLastFrame->GetStyleText()->WhiteSpaceIsSignificant();
  if (type == nsGkAtoms::placeholderFrame)
    return IsContinuationPlaceholder(aLastFrame);
  return PR_FALSE;
}

nsresult
nsBlockFrame::AddFrames(nsIFrame* aFrameList,
                        nsIFrame* aPrevSibling)
{
  // Clear our line cursor, since our lines may change.
  ClearLineCursor();

  if (nsnull == aFrameList) {
    return NS_OK;
  }

  // If we're inserting at the beginning of our list and we have an
  // inside bullet, insert after that bullet.
  if (!aPrevSibling && mBullet && !HaveOutsideBullet()) {
    NS_ASSERTION(!nsFrameList(aFrameList).ContainsFrame(mBullet),
                 "Trying to make mBullet prev sibling to itself");
    aPrevSibling = mBullet;
  }
  
  nsIPresShell *presShell = GetPresContext()->PresShell();

  // Attempt to find the line that contains the previous sibling
  nsLineList::iterator prevSibLine = end_lines();
  PRInt32 prevSiblingIndex = -1;
  if (aPrevSibling) {
    // XXX_perf This is technically O(N^2) in some cases, but by using
    // RFind instead of Find, we make it O(N) in the most common case,
    // which is appending content.

    // Find the line that contains the previous sibling
    if (! nsLineBox::RFindLineContaining(aPrevSibling,
                                         begin_lines(), prevSibLine,
                                         &prevSiblingIndex)) {
      // Note: defensive code! RFindLineContaining must not return
      // false in this case, so if it does...
      NS_NOTREACHED("prev sibling not in line list");
      aPrevSibling = nsnull;
      prevSibLine = end_lines();
    }
  }

  // Find the frame following aPrevSibling so that we can join up the
  // two lists of frames.
  nsIFrame* prevSiblingNextFrame = nsnull;
  if (aPrevSibling) {
    prevSiblingNextFrame = aPrevSibling->GetNextSibling();

    // Split line containing aPrevSibling in two if the insertion
    // point is somewhere in the middle of the line.
    PRInt32 rem = prevSibLine->GetChildCount() - prevSiblingIndex - 1;
    if (rem) {
      // Split the line in two where the frame(s) are being inserted.
      nsLineBox* line = NS_NewLineBox(presShell, prevSiblingNextFrame, rem, PR_FALSE);
      if (!line) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      mLines.after_insert(prevSibLine, line);
      prevSibLine->SetChildCount(prevSibLine->GetChildCount() - rem);
      prevSibLine->MarkDirty();
    }

    // Now (partially) join the sibling lists together
    aPrevSibling->SetNextSibling(aFrameList);
  }
  else if (! mLines.empty()) {
    prevSiblingNextFrame = mLines.front()->mFirstChild;
  }

  // Walk through the new frames being added and update the line data
  // structures to fit.
  nsIFrame* newFrame = aFrameList;
  while (newFrame) {
    PRBool isBlock = nsLineLayout::TreatFrameAsBlock(newFrame);

    // If the frame is a block frame, or if there is no previous line or if the
    // previous line is a block line we need to make a new line.  We also make
    // a new line, as an optimization, in the three cases we know we'll need it:
    // if the previous line ended with a <br>, if it has significant whitespace and
    // ended in a newline, or if it contains continuation placeholders.
    if (isBlock || prevSibLine == end_lines() || prevSibLine->IsBlock() ||
        (aPrevSibling && ShouldPutNextSiblingOnNewLine(aPrevSibling))) {
      // Create a new line for the frame and add its line to the line
      // list.
      nsLineBox* line = NS_NewLineBox(presShell, newFrame, 1, isBlock);
      if (!line) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      if (prevSibLine != end_lines()) {
        // Append new line after prevSibLine
        mLines.after_insert(prevSibLine, line);
        ++prevSibLine;
      }
      else {
        // New line is going before the other lines
        mLines.push_front(line);
        prevSibLine = begin_lines();
      }
    }
    else {
      prevSibLine->SetChildCount(prevSibLine->GetChildCount() + 1);
      prevSibLine->MarkDirty();
    }

    aPrevSibling = newFrame;
    newFrame = newFrame->GetNextSibling();
  }
  if (prevSiblingNextFrame) {
    // Connect the last new frame to the remainder of the sibling list
    aPrevSibling->SetNextSibling(prevSiblingNextFrame);
  }

#ifdef DEBUG
  VerifyLines(PR_TRUE);
#endif
  return NS_OK;
}

nsBlockFrame::line_iterator
nsBlockFrame::RemoveFloat(nsIFrame* aFloat) {
  // Find which line contains the float, so we can update
  // the float cache.
  line_iterator line = begin_lines(), line_end = end_lines();
  for ( ; line != line_end; ++line) {
    if (line->IsInline() && line->RemoveFloat(aFloat)) {
      break;
    }
  }

  // Unlink the placeholder *after* we searched the lines, because
  // the line search uses the placeholder relationship.
  nsFrameManager* fm = GetPresContext()->GetPresShell()->FrameManager();
  nsPlaceholderFrame* placeholder = fm->GetPlaceholderFrameFor(aFloat);
  if (placeholder) {
    fm->UnregisterPlaceholderFrame(placeholder);
    placeholder->SetOutOfFlowFrame(nsnull);
  }

  // Try to destroy if it's in mFloats.
  if (mFloats.DestroyFrame(aFloat)) {
    return line;
  }

  // Try our overflow list
  {
    nsAutoOOFFrameList oofs(this);
    if (oofs.mList.DestroyFrame(aFloat)) {
      return line_end;
    }
  }
  
  // If this is during reflow, it could be the out-of-flow frame for a
  // placeholder in our block reflow state's mOverflowPlaceholders. But that's
  // OK; it's not part of any child list, so we can just go ahead and delete it.
  aFloat->Destroy();
  return line_end;
}

static void MarkAllDescendantLinesDirty(nsBlockFrame* aBlock)
{
  nsLineList::iterator line = aBlock->begin_lines();
  nsLineList::iterator endLine = aBlock->end_lines();
  while (line != endLine) {
    if (line->IsBlock()) {
      nsIFrame* f = line->mFirstChild;
      void* bf;
      if (NS_SUCCEEDED(f->QueryInterface(kBlockFrameCID, &bf))) {
        MarkAllDescendantLinesDirty(NS_STATIC_CAST(nsBlockFrame*, f));
      }
    }
    line->MarkDirty();
    ++line;
  }
}

static void MarkSameSpaceManagerLinesDirty(nsBlockFrame* aBlock)
{
  nsBlockFrame* blockWithSpaceMgr = aBlock;
  while (!(blockWithSpaceMgr->GetStateBits() & NS_BLOCK_SPACE_MGR)) {
    void* bf;
    if (NS_FAILED(blockWithSpaceMgr->GetParent()->
                  QueryInterface(kBlockFrameCID, &bf))) {
      break;
    }
    blockWithSpaceMgr = NS_STATIC_CAST(nsBlockFrame*, blockWithSpaceMgr->GetParent());
  }
    
  // Mark every line at and below the line where the float was
  // dirty, and mark their lines dirty too. We could probably do
  // something more efficient --- e.g., just dirty the lines that intersect
  // the float vertically.
  MarkAllDescendantLinesDirty(blockWithSpaceMgr);
}

/**
 * Returns PR_TRUE if aFrame is a block that has one or more float children.
 */
static PRBool BlockHasAnyFloats(nsIFrame* aFrame)
{
  void* bf;
  if (NS_FAILED(aFrame->QueryInterface(kBlockFrameCID, &bf)))
    return PR_FALSE;
  nsBlockFrame* block = NS_STATIC_CAST(nsBlockFrame*, aFrame);
  if (block->GetFirstChild(nsGkAtoms::floatList))
    return PR_TRUE;
    
  nsLineList::iterator line = block->begin_lines();
  nsLineList::iterator endLine = block->end_lines();
  while (line != endLine) {
    if (line->IsBlock() && BlockHasAnyFloats(line->mFirstChild))
      return PR_TRUE;
    ++line;
  }
  return PR_FALSE;
}

NS_IMETHODIMP
nsBlockFrame::RemoveFrame(nsIAtom*  aListName,
                          nsIFrame* aOldFrame)
{
  nsresult rv = NS_OK;

#ifdef NOISY_REFLOW_REASON
  ListTag(stdout);
  printf(": remove ");
  nsFrame::ListTag(stdout, aOldFrame);
  printf("\n");
#endif

  if (nsnull == aListName) {
    PRBool hasFloats = BlockHasAnyFloats(aOldFrame);
    rv = DoRemoveFrame(aOldFrame, PR_TRUE, PR_FALSE);
    if (hasFloats) {
      MarkSameSpaceManagerLinesDirty(this);
    }
  }
  else if (mAbsoluteContainer.GetChildListName() == aListName) {
    return mAbsoluteContainer.RemoveFrame(this, aListName, aOldFrame);
  }
  else if (nsGkAtoms::floatList == aListName) {
    RemoveFloat(aOldFrame);
    MarkSameSpaceManagerLinesDirty(this);
  }
#ifdef IBMBIDI
  else if (nsGkAtoms::nextBidi == aListName) {
    // Skip the call to |FrameNeedsReflow| below by returning now.
    return DoRemoveFrame(aOldFrame, PR_TRUE, PR_FALSE);
  }
#endif // IBMBIDI
  else {
    NS_ERROR("unexpected child list");
    rv = NS_ERROR_INVALID_ARG;
  }

  if (NS_SUCCEEDED(rv)) {
    AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN); // XXX sufficient?
    GetPresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eTreeChange);
  }
  return rv;
}

void
nsBlockFrame::DoRemoveOutOfFlowFrame(nsIFrame* aFrame)
{
  // First remove aFrame's next in flow
  nsIFrame* nextInFlow = aFrame->GetNextInFlow();
  if (nextInFlow) {
    nsBlockFrame::DoRemoveOutOfFlowFrame(nextInFlow);
  }
  // Now remove aFrame
  const nsStyleDisplay* display = aFrame->GetStyleDisplay();

  // The containing block is always the parent of aFrame.
  nsBlockFrame* block = (nsBlockFrame*)aFrame->GetParent();
  // Remove aFrame from the appropriate list. 
  if (display->IsAbsolutelyPositioned()) {
    block->mAbsoluteContainer.RemoveFrame(block,
                                          block->mAbsoluteContainer.GetChildListName(),
                                          aFrame);
    aFrame->Destroy();
  }
  else {
    // This also destroys the frame.
    block->RemoveFloat(aFrame);
  }
}

/**
 * This helps us iterate over the list of all normal + overflow lines
 */
void
nsBlockFrame::TryAllLines(nsLineList::iterator* aIterator,
                          nsLineList::iterator* aEndIterator,
                          PRBool* aInOverflowLines) {
  if (*aIterator == *aEndIterator) {
    if (!*aInOverflowLines) {
      *aInOverflowLines = PR_TRUE;
      // Try the overflow lines
      nsLineList* overflowLines = GetOverflowLines();
      if (overflowLines) {
        *aIterator = overflowLines->begin();
        *aEndIterator = overflowLines->end();
      }
    }
  }
}

static nsresult RemoveBlockChild(nsIFrame* aFrame, PRBool aDestroyFrames)
{
  if (!aFrame)
    return NS_OK;

  nsBlockFrame* nextBlock = NS_STATIC_CAST(nsBlockFrame*, aFrame->GetParent());
  NS_ASSERTION(nextBlock->GetType() == nsGkAtoms::blockFrame ||
               nextBlock->GetType() == nsGkAtoms::areaFrame,
               "Our child's continuation's parent is not a block?");
  return nextBlock->DoRemoveFrame(aFrame, aDestroyFrames);
}

// This function removes aDeletedFrame and all its continuations.  It
// is optimized for deleting a whole series of frames. The easy
// implementation would invoke itself recursively on
// aDeletedFrame->GetNextContinuation, then locate the line containing
// aDeletedFrame and remove aDeletedFrame from that line. But here we
// start by locating aDeletedFrame and then scanning from that point
// on looking for continuations.
nsresult
nsBlockFrame::DoRemoveFrame(nsIFrame* aDeletedFrame, PRBool aDestroyFrames,
                            PRBool aRemoveOnlyFluidContinuations)
{
  // Clear our line cursor, since our lines may change.
  ClearLineCursor();
        
  if (aDeletedFrame->GetStateBits() & NS_FRAME_OUT_OF_FLOW) {
    NS_ASSERTION(aDestroyFrames, "We can't not destroy out of flows");
    DoRemoveOutOfFlowFrame(aDeletedFrame);
    return NS_OK;
  }
  
  nsPresContext* presContext = GetPresContext();
  nsIPresShell* presShell = presContext->PresShell();

  PRBool isPlaceholder = nsGkAtoms::placeholderFrame == aDeletedFrame->GetType();
  if (isPlaceholder) {
    nsFrameList* overflowPlaceholders = GetOverflowPlaceholders();
    if (overflowPlaceholders && overflowPlaceholders->RemoveFrame(aDeletedFrame)) {
      nsIFrame* nif = aDeletedFrame->GetNextInFlow();
      if (aDestroyFrames) {
        aDeletedFrame->Destroy();
      } else {
        aDeletedFrame->SetNextSibling(nsnull);
      }
      return RemoveBlockChild(nif, aDestroyFrames);
    }
  }
  
  // Find the line and the previous sibling that contains
  // deletedFrame; we also find the pointer to the line.
  nsLineList::iterator line = mLines.begin(),
                       line_end = mLines.end();
  PRBool searchingOverflowList = PR_FALSE;
  nsIFrame* prevSibling = nsnull;
  // Make sure we look in the overflow lines even if the normal line
  // list is empty
  TryAllLines(&line, &line_end, &searchingOverflowList);
  while (line != line_end) {
    nsIFrame* frame = line->mFirstChild;
    PRInt32 n = line->GetChildCount();
    while (--n >= 0) {
      if (frame == aDeletedFrame) {
        goto found_frame;
      }
      prevSibling = frame;
      frame = frame->GetNextSibling();
    }
    ++line;
    TryAllLines(&line, &line_end, &searchingOverflowList);
  }
found_frame:;
  if (line == line_end) {
    NS_ERROR("can't find deleted frame in lines");
    return NS_ERROR_FAILURE;
  }

  if (prevSibling && !prevSibling->GetNextSibling()) {
    // We must have found the first frame in the overflow line list. So
    // there is no prevSibling
    prevSibling = nsnull;
  }
  NS_ASSERTION(!prevSibling || prevSibling->GetNextSibling() == aDeletedFrame, "bad prevSibling");

  while ((line != line_end) && (nsnull != aDeletedFrame)) {
    NS_ASSERTION(this == aDeletedFrame->GetParent(), "messed up delete code");
    NS_ASSERTION(line->Contains(aDeletedFrame), "frame not in line");

    // If the frame being deleted is the last one on the line then
    // optimize away the line->Contains(next-in-flow) call below.
    PRBool isLastFrameOnLine = (1 == line->GetChildCount() ||
                                line->LastChild() == aDeletedFrame);

    // Remove aDeletedFrame from the line
    nsIFrame* nextFrame = aDeletedFrame->GetNextSibling();
    if (line->mFirstChild == aDeletedFrame) {
      // We should be setting this to null if aDeletedFrame
      // is the only frame on the line. HOWEVER in that case
      // we will be removing the line anyway, see below.
      line->mFirstChild = nextFrame;
    }

    // Hmm, this won't do anything if we're removing a frame in the first
    // overflow line... Hopefully doesn't matter
    --line;
    if (line != line_end && !line->IsBlock()) {
      // Since we just removed a frame that follows some inline
      // frames, we need to reflow the previous line.
      line->MarkDirty();
    }
    ++line;

    // Take aDeletedFrame out of the sibling list. Note that
    // prevSibling will only be nsnull when we are deleting the very
    // first frame in the main or overflow list.
    if (prevSibling) {
      prevSibling->SetNextSibling(nextFrame);
    }

    // Update the child count of the line to be accurate
    PRInt32 lineChildCount = line->GetChildCount();
    lineChildCount--;
    line->SetChildCount(lineChildCount);

    // Destroy frame; capture its next continuation first in case we need
    // to destroy that too.
    nsIFrame* deletedNextContinuation = aRemoveOnlyFluidContinuations ?
      aDeletedFrame->GetNextInFlow() : aDeletedFrame->GetNextContinuation();
#ifdef NOISY_REMOVE_FRAME
    printf("DoRemoveFrame: %s line=%p frame=",
           searchingOverflowList?"overflow":"normal", line.get());
    nsFrame::ListTag(stdout, aDeletedFrame);
    printf(" prevSibling=%p deletedNextContinuation=%p\n", prevSibling, deletedNextContinuation);
#endif

    if (aDestroyFrames) {
      aDeletedFrame->Destroy();
    } else {
      aDeletedFrame->SetNextSibling(nsnull);
    }
    aDeletedFrame = deletedNextContinuation;

    PRBool haveAdvancedToNextLine = PR_FALSE;
    // If line is empty, remove it now.
    if (0 == lineChildCount) {
#ifdef NOISY_REMOVE_FRAME
        printf("DoRemoveFrame: %s line=%p became empty so it will be removed\n",
               searchingOverflowList?"overflow":"normal", line.get());
#endif
      nsLineBox *cur = line;
      if (!searchingOverflowList) {
        line = mLines.erase(line);
        // Invalidate the space taken up by the line.
        // XXX We need to do this if we're removing a frame as a result of
        // a call to RemoveFrame(), but we may not need to do this in all
        // cases...
        nsRect lineCombinedArea(cur->GetCombinedArea());
#ifdef NOISY_BLOCK_INVALIDATE
        printf("%p invalidate 10 (%d, %d, %d, %d)\n",
               this, lineCombinedArea.x, lineCombinedArea.y,
               lineCombinedArea.width, lineCombinedArea.height);
#endif
        Invalidate(lineCombinedArea);
      } else {
        nsLineList* lineList = RemoveOverflowLines();
        line = lineList->erase(line);
        if (!lineList->empty()) {
          SetOverflowLines(lineList);
        }
      }
      cur->Destroy(presShell);

      // If we're removing a line, ReflowDirtyLines isn't going to
      // know that it needs to slide lines unless something is marked
      // dirty.  So mark the previous margin of the next line dirty if
      // there is one.
      if (line != line_end) {
        line->MarkPreviousMarginDirty();
      }
      haveAdvancedToNextLine = PR_TRUE;
    } else {
      // Make the line that just lost a frame dirty, and advance to
      // the next line.
      if (!deletedNextContinuation || isLastFrameOnLine ||
          !line->Contains(deletedNextContinuation)) {
        line->MarkDirty();
        ++line;
        haveAdvancedToNextLine = PR_TRUE;
      }
    }

    if (deletedNextContinuation) {
      // Continuations for placeholder frames don't always appear in
      // consecutive lines. So for placeholders, just continue the slow easy way.
      if (isPlaceholder) {
        return RemoveBlockChild(deletedNextContinuation, aDestroyFrames);
      }

      // See if we should keep looking in the current flow's line list.
      if (deletedNextContinuation->GetParent() != this) {
        // The deceased frames continuation is not a child of the
        // current block. So break out of the loop so that we advance
        // to the next parent.
        break;
      }

      // If we advanced to the next line then check if we should switch to the
      // overflow line list.
      if (haveAdvancedToNextLine) {
        if (line != line_end && !searchingOverflowList &&
            !line->Contains(deletedNextContinuation)) {
          // We have advanced to the next *normal* line but the next-in-flow
          // is not there - force a switch to the overflow line list.
          line = line_end;
        }

        PRBool wasSearchingOverflowList = searchingOverflowList;
        TryAllLines(&line, &line_end, &searchingOverflowList);
        if (NS_UNLIKELY(searchingOverflowList && !wasSearchingOverflowList &&
                        prevSibling)) {
          // We switched to the overflow line list and we have a prev sibling
          // (in the main list), in this case we don't want to pick up any
          // sibling list from the deceased frames (bug 344557).
          prevSibling->SetNextSibling(nsnull);
          prevSibling = nsnull;
        }
#ifdef NOISY_REMOVE_FRAME
        printf("DoRemoveFrame: now on %s line=%p prevSibling=%p\n",
               searchingOverflowList?"overflow":"normal", line.get(),
               prevSibling);
#endif
      }
    }
  }

#ifdef DEBUG
  VerifyLines(PR_TRUE);
#endif

  // Advance to next flow block if the frame has more continuations
  return RemoveBlockChild(aDeletedFrame, aDestroyFrames);
}

void
nsBlockFrame::DeleteNextInFlowChild(nsPresContext* aPresContext,
                                    nsIFrame*       aNextInFlow)
{
  nsIFrame* prevInFlow = aNextInFlow->GetPrevInFlow();
  NS_PRECONDITION(prevInFlow, "bad next-in-flow");
  NS_PRECONDITION(IsChild(aNextInFlow), "bad geometric parent");

  DoRemoveFrame(aNextInFlow);
}

////////////////////////////////////////////////////////////////////////
// Float support

nsresult
nsBlockFrame::ReflowFloat(nsBlockReflowState& aState,
                          nsPlaceholderFrame* aPlaceholder,
                          nsFloatCache*       aFloatCache,
                          nsReflowStatus&     aReflowStatus)
{
  // Reflow the float.
  nsIFrame* floatFrame = aPlaceholder->GetOutOfFlowFrame();
  aReflowStatus = NS_FRAME_COMPLETE;

#ifdef NOISY_FLOAT
  printf("Reflow Float %p in parent %p, availSpace(%d,%d,%d,%d)\n",
          aPlaceholder->GetOutOfFlowFrame(), this, 
          aState.mAvailSpaceRect.x, aState.mAvailSpaceRect.y, 
          aState.mAvailSpaceRect.width, aState.mAvailSpaceRect.height
  );
#endif

  // Compute the available width. By default, assume the width of the
  // containing block.
  nscoord availWidth;
  const nsStyleDisplay* floatDisplay = floatFrame->GetStyleDisplay();

  if (NS_STYLE_DISPLAY_TABLE != floatDisplay->mDisplay ||
      eCompatibility_NavQuirks != aState.mPresContext->CompatibilityMode() ) {
    availWidth = aState.mContentArea.width;
  }
  else {
    // This quirk matches the one in nsBlockReflowState::FlowAndPlaceFloat
    // give tables only the available space
    // if they can shrink we may not be constrained to place
    // them in the next line
    availWidth = aState.mAvailSpaceRect.width;
    // round down to twips per pixel so that we fit
    // needed when prev. float has procentage width
    // (maybe is a table flaw that makes table chose to round up
    // but I don't want to change that, too risky)
    nscoord twp = nsPresContext::CSSPixelsToAppUnits(1);
    availWidth -=  availWidth % twp;
  }

  // aState.mY is relative to the border-top, make it relative to the content-top
  nscoord contentYOffset = aState.mY - aState.BorderPadding().top;
  nscoord availHeight = NS_UNCONSTRAINEDSIZE == aState.mContentArea.height
                        ? NS_UNCONSTRAINEDSIZE 
                        : PR_MAX(0, aState.mContentArea.height - contentYOffset);

  nsRect availSpace(aState.BorderPadding().left,
                    aState.BorderPadding().top,
                    availWidth, availHeight);

  // construct the html reflow state for the float. ReflowBlock will 
  // initialize it.
  nsHTMLReflowState floatRS(aState.mPresContext, aState.mReflowState,
                            floatFrame, 
                            nsSize(availSpace.width, availSpace.height));

  // Setup a block reflow state to reflow the float.
  nsBlockReflowContext brc(aState.mPresContext, aState.mReflowState);

  // Reflow the float
  PRBool isAdjacentWithTop = aState.IsAdjacentWithTop();

  nsIFrame* clearanceFrame = nsnull;
  nsresult rv;
  do {
    nsCollapsingMargin margin;
    PRBool mayNeedRetry = PR_FALSE;
    floatRS.mDiscoveredClearance = nsnull;
    // Only first in flow gets a top margin.
    if (!floatFrame->GetPrevInFlow()) {
      nsBlockReflowContext::ComputeCollapsedTopMargin(floatRS, &margin,
                                                      clearanceFrame, &mayNeedRetry);

      if (mayNeedRetry && !clearanceFrame) {
        floatRS.mDiscoveredClearance = &clearanceFrame;
        // We don't need to push the space manager state because the the block has its own
        // space manager that will be destroyed and recreated
      }
    }

    rv = brc.ReflowBlock(availSpace, PR_TRUE, margin,
                         0, isAdjacentWithTop,
                         aFloatCache->mOffsets, floatRS,
                         aReflowStatus);
  } while (NS_SUCCEEDED(rv) && clearanceFrame);

  // An incomplete reflow status means we should split the float 
  // if the height is constrained (bug 145305). 
  if (NS_FRAME_IS_NOT_COMPLETE(aReflowStatus) && (NS_UNCONSTRAINEDSIZE == availHeight))
    aReflowStatus = NS_FRAME_COMPLETE;
  
  if (NS_FRAME_IS_COMPLETE(aReflowStatus)) {
    // Float is now complete, so delete the placeholder's next in
    // flows, if any; their floats (which are this float's continuations)
    // have already been deleted.
    // XXX won't this be done later in nsLineLayout::ReflowFrame anyway??
    nsIFrame* nextInFlow = aPlaceholder->GetNextInFlow();
    if (nextInFlow) {
      NS_STATIC_CAST(nsHTMLContainerFrame*, nextInFlow->GetParent())
        ->DeleteNextInFlowChild(aState.mPresContext, nextInFlow);
      // that takes care of all subsequent nextinflows too
    }
  }
  if (aReflowStatus & NS_FRAME_REFLOW_NEXTINFLOW) {
    aState.mReflowStatus |= NS_FRAME_REFLOW_NEXTINFLOW;
  }

  if (floatFrame->GetType() == nsGkAtoms::letterFrame) {
    // We never split floating first letters; an incomplete state for
    // such frames simply means that there is more content to be
    // reflowed on the line.
    if (NS_FRAME_IS_NOT_COMPLETE(aReflowStatus)) 
      aReflowStatus = NS_FRAME_COMPLETE;
  }

  if (NS_FAILED(rv)) {
    return rv;
  }

  // Capture the margin information for the caller
  const nsMargin& m = brc.GetMargin();
  aFloatCache->mMargins.top = brc.GetTopMargin();
  aFloatCache->mMargins.right = m.right;
  // Only last in flows get a bottom margin
  if (NS_FRAME_IS_COMPLETE(aReflowStatus)) {
    brc.GetCarriedOutBottomMargin().Include(m.bottom);
  }
  aFloatCache->mMargins.bottom = brc.GetCarriedOutBottomMargin().get();
  aFloatCache->mMargins.left = m.left;

  const nsHTMLReflowMetrics& metrics = brc.GetMetrics();
  aFloatCache->mCombinedArea = metrics.mOverflowArea;

  // Set the rect, make sure the view is properly sized and positioned,
  // and tell the frame we're done reflowing it
  // XXXldb This seems like the wrong place to be doing this -- shouldn't
  // we be doing this in nsBlockReflowState::FlowAndPlaceFloat after
  // we've positioned the float, and shouldn't we be doing the equivalent
  // of |::PlaceFrameView| here?
  floatFrame->SetSize(nsSize(metrics.width, metrics.height));
  if (floatFrame->HasView()) {
    nsContainerFrame::SyncFrameViewAfterReflow(aState.mPresContext, floatFrame,
                                               floatFrame->GetView(),
                                               &metrics.mOverflowArea,
                                               NS_FRAME_NO_MOVE_VIEW);
  }
  // Pass floatRS so the frame hierarchy can be used (redoFloatRS has the same hierarchy)  
  floatFrame->DidReflow(aState.mPresContext, &floatRS,
                        NS_FRAME_REFLOW_FINISHED);

#ifdef NOISY_FLOAT
  printf("end ReflowFloat %p, sized to %d,%d\n",
         floatFrame, metrics.width, metrics.height);
#endif

  // If the placeholder was continued and its first-in-flow was followed by a 
  // <BR>, then cache the <BR>'s break type in aState.mFloatBreakType so that
  // the next frame after the placeholder can combine that break type with its own
  nsIFrame* prevPlaceholder = aPlaceholder->GetPrevInFlow();
  if (prevPlaceholder) {
    // the break occurs only after the last continued placeholder
    PRBool lastPlaceholder = PR_TRUE;
    nsIFrame* next = aPlaceholder->GetNextSibling();
    if (next) {
      if (nsGkAtoms::placeholderFrame == next->GetType()) {
        lastPlaceholder = PR_FALSE;
      }
    }
    if (lastPlaceholder) {
      // get the containing block of prevPlaceholder which is our prev-in-flow
      if (GetPrevInFlow()) {
        // get the break type of the last line in mPrevInFlow
        line_iterator endLine = --((nsBlockFrame*)GetPrevInFlow())->end_lines();
        if (endLine->HasFloatBreakAfter()) {
          aState.mFloatBreakType = endLine->GetBreakTypeAfter();
        }
      }
      else NS_ASSERTION(PR_FALSE, "no prev in flow");
    }
  }
  return NS_OK;
}

//////////////////////////////////////////////////////////////////////
// Painting, event handling

PRIntn
nsBlockFrame::GetSkipSides() const
{
  PRIntn skip = 0;
  if (nsnull != GetPrevInFlow()) {
    skip |= 1 << NS_SIDE_TOP;
  }
  if (nsnull != GetNextInFlow()) {
    skip |= 1 << NS_SIDE_BOTTOM;
  }
  return skip;
}

#ifdef DEBUG
static void ComputeCombinedArea(nsLineList& aLines,
                                nscoord aWidth, nscoord aHeight,
                                nsRect& aResult)
{
  nscoord xa = 0, ya = 0, xb = aWidth, yb = aHeight;
  for (nsLineList::iterator line = aLines.begin(), line_end = aLines.end();
       line != line_end;
       ++line) {
    // Compute min and max x/y values for the reflowed frame's
    // combined areas
    nsRect lineCombinedArea(line->GetCombinedArea());
    nscoord x = lineCombinedArea.x;
    nscoord y = lineCombinedArea.y;
    nscoord xmost = x + lineCombinedArea.width;
    nscoord ymost = y + lineCombinedArea.height;
    if (x < xa) {
      xa = x;
    }
    if (xmost > xb) {
      xb = xmost;
    }
    if (y < ya) {
      ya = y;
    }
    if (ymost > yb) {
      yb = ymost;
    }
  }

  aResult.x = xa;
  aResult.y = ya;
  aResult.width = xb - xa;
  aResult.height = yb - ya;
}
#endif

PRBool
nsBlockFrame::IsVisibleInSelection(nsISelection* aSelection)
{
  nsCOMPtr<nsIDOMHTMLHtmlElement> html(do_QueryInterface(mContent));
  nsCOMPtr<nsIDOMHTMLBodyElement> body(do_QueryInterface(mContent));
  if (html || body)
    return PR_TRUE;

  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(mContent));
  PRBool visible;
  nsresult rv = aSelection->ContainsNode(node, PR_TRUE, &visible);
  return NS_SUCCEEDED(rv) && visible;
}

/* virtual */ void
nsBlockFrame::PaintTextDecorationLine(nsIRenderingContext& aRenderingContext, 
                                      nsPoint aPt,
                                      nsLineBox* aLine,
                                      nscolor aColor, 
                                      nscoord aOffset, 
                                      nscoord aAscent, 
                                      nscoord aSize) 
{
  aRenderingContext.SetColor(aColor);
  NS_ASSERTION(!aLine->IsBlock(), "Why did we ask for decorations on a block?");

  nscoord start = aLine->mBounds.x;
  nscoord width = aLine->mBounds.width;

  if (aLine == begin_lines().get()) {
    // Adjust for the text-indent.  See similar code in
    // nsLineLayout::BeginLineReflow.
    nscoord indent = 0;
    const nsStyleText* styleText = GetStyleText();
    nsStyleUnit unit = styleText->mTextIndent.GetUnit();
    if (eStyleUnit_Coord == unit) {
      indent = styleText->mTextIndent.GetCoordValue();
    } else if (eStyleUnit_Percent == unit) {
      // It's a percentage of the containing block width.
      nsIFrame* containingBlock =
        nsHTMLReflowState::GetContainingBlockFor(this);
      NS_ASSERTION(containingBlock, "Must have containing block!");
      indent = nscoord(styleText->mTextIndent.GetPercentValue() *
                       containingBlock->GetContentRect().width);
    }

    // Adjust the start position and the width of the decoration by the
    // value of the indent.  Note that indent can be negative; that's OK.
    // It'll just increase the width (which can also happen to be
    // negative!).
    start += indent;
    width -= indent;
  }
      
  // Only paint if we have a positive width
  if (width > 0) {
    aRenderingContext.FillRect(start + aPt.x,
                               aLine->mBounds.y + aLine->GetAscent() - aOffset + aPt.y, 
                               width, aSize);
  }
}

#ifdef DEBUG
static void DebugOutputDrawLine(PRInt32 aDepth, nsLineBox* aLine, PRBool aDrawn) {
  if (nsBlockFrame::gNoisyDamageRepair) {
    nsFrame::IndentBy(stdout, aDepth+1);
    nsRect lineArea = aLine->GetCombinedArea();
    printf("%s line=%p bounds=%d,%d,%d,%d ca=%d,%d,%d,%d\n",
           aDrawn ? "draw" : "skip",
           NS_STATIC_CAST(void*, aLine),
           aLine->mBounds.x, aLine->mBounds.y,
           aLine->mBounds.width, aLine->mBounds.height,
           lineArea.x, lineArea.y,
           lineArea.width, lineArea.height);
  }
}
#endif

static nsresult
DisplayLine(nsDisplayListBuilder* aBuilder, const nsRect& aLineArea,
            const nsRect& aDirtyRect, nsBlockFrame::line_iterator& aLine,
            PRInt32 aDepth, PRInt32& aDrawnLines, const nsDisplayListSet& aLists,
            nsBlockFrame* aFrame) {
  // If the line's combined area (which includes child frames that
  // stick outside of the line's bounding box or our bounding box)
  // intersects the dirty rect then paint the line.
  PRBool intersect = aLineArea.Intersects(aDirtyRect);
#ifdef DEBUG
  if (nsBlockFrame::gLamePaintMetrics) {
    aDrawnLines++;
  }
  DebugOutputDrawLine(aDepth, aLine.get(), intersect);
#endif
  // The line might contain a placeholder for a visible out-of-flow, in which
  // case we need to descend into it. If there is such a placeholder, we will
  // have NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO set.
  if (!intersect &&
      !(aFrame->GetStateBits() & NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO))
    return NS_OK;

  nsresult rv;
  nsDisplayList aboveTextDecorations;
  PRBool lineInline = aLine->IsInline();
  if (lineInline) {
    // Display the text-decoration for the hypothetical anonymous inline box
    // that wraps these inlines
    rv = aFrame->DisplayTextDecorations(aBuilder, aLists.Content(),
                                        &aboveTextDecorations, aLine);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Block-level child backgrounds go on the blockBorderBackgrounds list ...
  // Inline-level child backgrounds go on the regular child content list.
  nsDisplayListSet childLists(aLists,
      lineInline ? aLists.Content() : aLists.BlockBorderBackgrounds());
  nsIFrame* kid = aLine->mFirstChild;
  PRInt32 n = aLine->GetChildCount();
  while (--n >= 0) {
    rv = aFrame->BuildDisplayListForChild(aBuilder, kid, aDirtyRect, childLists,
                                          lineInline ? nsIFrame::DISPLAY_CHILD_INLINE : 0);
    NS_ENSURE_SUCCESS(rv, rv);
    kid = kid->GetNextSibling();
  }
  
  aLists.Content()->AppendToTop(&aboveTextDecorations);
  return NS_OK;
}

NS_IMETHODIMP
nsBlockFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                               const nsRect&           aDirtyRect,
                               const nsDisplayListSet& aLists)
{
  PRInt32 drawnLines; // Will only be used if set (gLamePaintMetrics).
  PRInt32 depth = 0;
#ifdef DEBUG
  if (gNoisyDamageRepair) {
      depth = GetDepth();
      nsRect ca;
      ::ComputeCombinedArea(mLines, mRect.width, mRect.height, ca);
      nsFrame::IndentBy(stdout, depth);
      ListTag(stdout);
      printf(": bounds=%d,%d,%d,%d dirty(absolute)=%d,%d,%d,%d ca=%d,%d,%d,%d\n",
             mRect.x, mRect.y, mRect.width, mRect.height,
             aDirtyRect.x, aDirtyRect.y, aDirtyRect.width, aDirtyRect.height,
             ca.x, ca.y, ca.width, ca.height);
  }
  PRTime start = LL_ZERO; // Initialize these variables to silence the compiler.
  if (gLamePaintMetrics) {
    start = PR_Now();
    drawnLines = 0;
  }
#endif

  DisplayBorderBackgroundOutline(aBuilder, aLists);
  
  aBuilder->MarkFramesForDisplayList(this, mFloats.FirstChild(), aDirtyRect);
  aBuilder->MarkFramesForDisplayList(this, mAbsoluteContainer.GetFirstChild(), aDirtyRect);

  // Don't use the line cursor if we might have a descendant placeholder ...
  // it might skip lines that contain placeholders but don't themselves
  // intersect with the dirty area.
  nsLineBox* cursor = GetStateBits() & NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO
    ? nsnull : GetFirstLineContaining(aDirtyRect.y);
  line_iterator line_end = end_lines();
  nsresult rv = NS_OK;
  
  if (cursor) {
    for (line_iterator line = mLines.begin(cursor);
         line != line_end;
         ++line) {
      nsRect lineArea = line->GetCombinedArea();
      if (!lineArea.IsEmpty()) {
        // Because we have a cursor, the combinedArea.ys are non-decreasing.
        // Once we've passed aDirtyRect.YMost(), we can never see it again.
        if (lineArea.y >= aDirtyRect.YMost()) {
          break;
        }
        rv = DisplayLine(aBuilder, lineArea, aDirtyRect, line, depth, drawnLines,
                         aLists, this);
        if (NS_FAILED(rv))
          break;
      }
    }
  } else {
    PRBool nonDecreasingYs = PR_TRUE;
    PRInt32 lineCount = 0;
    nscoord lastY = PR_INT32_MIN;
    nscoord lastYMost = PR_INT32_MIN;
    for (line_iterator line = begin_lines();
         line != line_end;
         ++line) {
      nsRect lineArea = line->GetCombinedArea();
      rv = DisplayLine(aBuilder, lineArea, aDirtyRect, line, depth, drawnLines,
                       aLists, this);
      if (NS_FAILED(rv))
        break;
      if (!lineArea.IsEmpty()) {
        if (lineArea.y < lastY
            || lineArea.YMost() < lastYMost) {
          nonDecreasingYs = PR_FALSE;
        }
        lastY = lineArea.y;
        lastYMost = lineArea.YMost();
      }
      lineCount++;
    }

    if (NS_SUCCEEDED(rv) && nonDecreasingYs && lineCount >= MIN_LINES_NEEDING_CURSOR) {
      SetupLineCursor();
    }
  }

  if (NS_SUCCEEDED(rv) && (nsnull != mBullet) && HaveOutsideBullet()) {
    // Display outside bullets manually
    rv = BuildDisplayListForChild(aBuilder, mBullet, aDirtyRect, aLists);
  }

#ifdef DEBUG
  if (gLamePaintMetrics) {
    PRTime end = PR_Now();

    PRInt32 numLines = mLines.size();
    if (!numLines) numLines = 1;
    PRTime lines, deltaPerLine, delta;
    LL_I2L(lines, numLines);
    LL_SUB(delta, end, start);
    LL_DIV(deltaPerLine, delta, lines);

    ListTag(stdout);
    char buf[400];
    PR_snprintf(buf, sizeof(buf),
                ": %lld elapsed (%lld per line) lines=%d drawn=%d skip=%d",
                delta, deltaPerLine,
                numLines, drawnLines, numLines - drawnLines);
    printf("%s\n", buf);
  }
#endif

  return rv;
}

#ifdef ACCESSIBILITY
NS_IMETHODIMP nsBlockFrame::GetAccessible(nsIAccessible** aAccessible)
{
  *aAccessible = nsnull;
  nsCOMPtr<nsIAccessibilityService> accService = 
    do_GetService("@mozilla.org/accessibilityService;1");
  NS_ENSURE_TRUE(accService, NS_ERROR_FAILURE);

  // block frame may be for <hr>
  if (mContent->Tag() == nsGkAtoms::hr) {
    return accService->CreateHTMLHRAccessible(NS_STATIC_CAST(nsIFrame*, this), aAccessible);
  }

  nsPresContext *aPresContext = GetPresContext();
  if (!mBullet || !aPresContext) {
    if (!mContent || !mContent->GetParent()) {
      // Don't create accessible objects for the root content node, they are redundant with
      // the nsDocAccessible object created with the document node
      return NS_ERROR_FAILURE;
    }
    
    nsCOMPtr<nsIDOMHTMLDocument> htmlDoc =
      do_QueryInterface(mContent->GetDocument());
    if (htmlDoc) {
      nsCOMPtr<nsIDOMHTMLElement> body;
      htmlDoc->GetBody(getter_AddRefs(body));
      if (SameCOMIdentity(body, mContent)) {
        // Don't create accessible objects for the body, they are redundant with
        // the nsDocAccessible object created with the document node
        return NS_ERROR_FAILURE;
      }
    }

    // Not a bullet, treat as normal HTML container
    return accService->CreateHyperTextAccessible(NS_STATIC_CAST(nsIFrame*, this), aAccessible);
  }

  // Create special list bullet accessible
  const nsStyleList* myList = GetStyleList();
  nsAutoString bulletText;
  if (myList->mListStyleImage || myList->mListStyleType == NS_STYLE_LIST_STYLE_DISC ||
      myList->mListStyleType == NS_STYLE_LIST_STYLE_CIRCLE ||
      myList->mListStyleType == NS_STYLE_LIST_STYLE_SQUARE) {
    bulletText.Assign(PRUnichar(0x2022));; // Unicode bullet character
  }
  else if (myList->mListStyleType != NS_STYLE_LIST_STYLE_NONE) {
    mBullet->GetListItemText(*myList, bulletText);
  }

  return accService->CreateHTMLLIAccessible(NS_STATIC_CAST(nsIFrame*, this), 
                                            NS_STATIC_CAST(nsIFrame*, mBullet), 
                                            bulletText,
                                            aAccessible);
}
#endif

void nsBlockFrame::ClearLineCursor() {
  if (!(GetStateBits() & NS_BLOCK_HAS_LINE_CURSOR)) {
    return;
  }

  UnsetProperty(nsGkAtoms::lineCursorProperty);
  RemoveStateBits(NS_BLOCK_HAS_LINE_CURSOR);
}

void nsBlockFrame::SetupLineCursor() {
  if (GetStateBits() & NS_BLOCK_HAS_LINE_CURSOR
      || mLines.empty()) {
    return;
  }
   
  SetProperty(nsGkAtoms::lineCursorProperty,
              mLines.front(), nsnull);
  AddStateBits(NS_BLOCK_HAS_LINE_CURSOR);
}

nsLineBox* nsBlockFrame::GetFirstLineContaining(nscoord y) {
  if (!(GetStateBits() & NS_BLOCK_HAS_LINE_CURSOR)) {
    return nsnull;
  }

  nsLineBox* property = NS_STATIC_CAST(nsLineBox*,
    GetProperty(nsGkAtoms::lineCursorProperty));
  line_iterator cursor = mLines.begin(property);
  nsRect cursorArea = cursor->GetCombinedArea();

  while ((cursorArea.IsEmpty() || cursorArea.YMost() > y)
         && cursor != mLines.front()) {
    cursor = cursor.prev();
    cursorArea = cursor->GetCombinedArea();
  }
  while ((cursorArea.IsEmpty() || cursorArea.YMost() <= y)
         && cursor != mLines.back()) {
    cursor = cursor.next();
    cursorArea = cursor->GetCombinedArea();
  }

  if (cursor.get() != property) {
    SetProperty(nsGkAtoms::lineCursorProperty,
                cursor.get(), nsnull);
  }

  return cursor.get();
}

/* virtual */ void
nsBlockFrame::ChildIsDirty(nsIFrame* aChild)
{
  // See if the child is absolutely positioned
  if (aChild->GetStateBits() & NS_FRAME_OUT_OF_FLOW &&
      aChild->GetStyleDisplay()->IsAbsolutelyPositioned()) {
    // do nothing
  } else if (aChild == mBullet && HaveOutsideBullet()) {
    // The bullet lives in the first line, unless the first line has
    // height 0 and there is a second line, in which case it lives
    // in the second line.
    line_iterator bulletLine = begin_lines();
    if (bulletLine != end_lines() && bulletLine->mBounds.height == 0 &&
        bulletLine != mLines.back()) {
      bulletLine = bulletLine.next();
    }
    
    if (bulletLine != end_lines()) {
      MarkLineDirty(bulletLine);
    }
    // otherwise we have an empty line list, and ReflowDirtyLines
    // will handle reflowing the bullet.
  } else {
    // Mark the line containing the child frame dirty.
    line_iterator fline = FindLineFor(aChild);
    if (fline != end_lines())
      MarkLineDirty(fline);
  }

  nsBlockFrameSuper::ChildIsDirty(aChild);
}

//////////////////////////////////////////////////////////////////////
// Start Debugging

#ifdef NS_DEBUG
static PRBool
InLineList(nsLineList& aLines, nsIFrame* aFrame)
{
  for (nsLineList::iterator line = aLines.begin(), line_end = aLines.end();
       line != line_end;
       ++line) {
    nsIFrame* frame = line->mFirstChild;
    PRInt32 n = line->GetChildCount();
    while (--n >= 0) {
      if (frame == aFrame) {
        return PR_TRUE;
      }
      frame = frame->GetNextSibling();
    }
  }
  return PR_FALSE;
}

static PRBool
InSiblingList(nsLineList& aLines, nsIFrame* aFrame)
{
  if (! aLines.empty()) {
    nsIFrame* frame = aLines.front()->mFirstChild;
    while (frame) {
      if (frame == aFrame) {
        return PR_TRUE;
      }
      frame = frame->GetNextSibling();
    }
  }
  return PR_FALSE;
}

PRBool
nsBlockFrame::IsChild(nsIFrame* aFrame)
{
  // Continued out-of-flows don't satisfy InLineList(), continued out-of-flows
  // and placeholders don't satisfy InSiblingList().
  PRBool skipLineList    = PR_FALSE;
  PRBool skipSiblingList = PR_FALSE;
  nsIFrame* prevInFlow = aFrame->GetPrevInFlow();
  PRBool isPlaceholder = nsGkAtoms::placeholderFrame == aFrame->GetType();
  if (prevInFlow) {
    nsFrameState state = aFrame->GetStateBits();
    skipLineList    = (state & NS_FRAME_OUT_OF_FLOW); 
    skipSiblingList = isPlaceholder || (state & NS_FRAME_OUT_OF_FLOW);
  }

  if (isPlaceholder) {
    nsFrameList* overflowPlaceholders = GetOverflowPlaceholders();
    if (overflowPlaceholders && overflowPlaceholders->ContainsFrame(aFrame)) {
      return PR_TRUE;
    }
  }

  if (aFrame->GetParent() != (nsIFrame*)this) {
    return PR_FALSE;
  }
  if ((skipLineList || InLineList(mLines, aFrame)) && 
      (skipSiblingList || InSiblingList(mLines, aFrame))) {
    return PR_TRUE;
  }
  nsLineList* overflowLines = GetOverflowLines();
  if (overflowLines && (skipLineList || InLineList(*overflowLines, aFrame)) && 
      (skipSiblingList || InSiblingList(*overflowLines, aFrame))) {
    return PR_TRUE;
  }
  return PR_FALSE;
}

NS_IMETHODIMP
nsBlockFrame::VerifyTree() const
{
  // XXX rewrite this
  return NS_OK;
}
#endif

// End Debugging
//////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsBlockFrame::Init(nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIFrame*        aPrevInFlow)
{
  if (aPrevInFlow) {
    // Copy over the block/area frame type flags
    nsBlockFrame*  blockFrame = (nsBlockFrame*)aPrevInFlow;

    SetFlags(blockFrame->mState &
             (NS_BLOCK_FLAGS_MASK & ~NS_BLOCK_FRAME_HAS_OUTSIDE_BULLET));
  }

  nsresult rv = nsBlockFrameSuper::Init(aContent, aParent, aPrevInFlow);

  if (IsBoxWrapped())
    mState |= NS_BLOCK_SPACE_MGR;

  return rv;
}

NS_IMETHODIMP
nsBlockFrame::SetInitialChildList(nsIAtom*        aListName,
                                  nsIFrame*       aChildList)
{
  nsresult rv = NS_OK;

  if (mAbsoluteContainer.GetChildListName() == aListName) {
    mAbsoluteContainer.SetInitialChildList(this, aListName, aChildList);
  }
  else if (nsGkAtoms::floatList == aListName) {
    mFloats.SetFrames(aChildList);
  }
  else {
    nsPresContext* presContext = GetPresContext();

    // Lookup up the two pseudo style contexts
    if (nsnull == GetPrevInFlow()) {
      nsRefPtr<nsStyleContext> firstLetterStyle = GetFirstLetterStyle(presContext);
      if (nsnull != firstLetterStyle) {
        mState |= NS_BLOCK_HAS_FIRST_LETTER_STYLE;
#ifdef NOISY_FIRST_LETTER
        ListTag(stdout);
        printf(": first-letter style found\n");
#endif
      }
    }

    rv = AddFrames(aChildList, nsnull);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Create list bullet if this is a list-item. Note that this is done
    // here so that RenumberLists will work (it needs the bullets to
    // store the bullet numbers).
    const nsStyleDisplay* styleDisplay = GetStyleDisplay();
    if ((nsnull == GetPrevInFlow()) &&
        (NS_STYLE_DISPLAY_LIST_ITEM == styleDisplay->mDisplay) &&
        (nsnull == mBullet)) {
      // Resolve style for the bullet frame
      const nsStyleList* styleList = GetStyleList();
      nsIAtom *pseudoElement;
      switch (styleList->mListStyleType) {
        case NS_STYLE_LIST_STYLE_DISC:
        case NS_STYLE_LIST_STYLE_CIRCLE:
        case NS_STYLE_LIST_STYLE_SQUARE:
          pseudoElement = nsCSSPseudoElements::mozListBullet;
          break;
        default:
          pseudoElement = nsCSSPseudoElements::mozListNumber;
          break;
      }

      nsIPresShell *shell = presContext->PresShell();

      nsRefPtr<nsStyleContext> kidSC = shell->StyleSet()->
        ResolvePseudoStyleFor(mContent, pseudoElement, mStyleContext);

      // Create bullet frame
      nsBulletFrame* bullet = new (shell) nsBulletFrame(kidSC);
      if (nsnull == bullet) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      bullet->Init(mContent, this, nsnull);

      // If the list bullet frame should be positioned inside then add
      // it to the flow now.
      if (NS_STYLE_LIST_STYLE_POSITION_INSIDE ==
          styleList->mListStylePosition) {
        AddFrames(bullet, nsnull);
        mState &= ~NS_BLOCK_FRAME_HAS_OUTSIDE_BULLET;
      }
      else {
        mState |= NS_BLOCK_FRAME_HAS_OUTSIDE_BULLET;
      }

      mBullet = bullet;
    }
  }

  return NS_OK;
}

// static
PRBool
nsBlockFrame::FrameStartsCounterScope(nsIFrame* aFrame)
{
  nsIContent* content = aFrame->GetContent();
  if (!content || !content->IsNodeOfType(nsINode::eHTML))
    return PR_FALSE;

  nsIAtom *localName = content->NodeInfo()->NameAtom();
  return localName == nsGkAtoms::ol ||
         localName == nsGkAtoms::ul ||
         localName == nsGkAtoms::dir ||
         localName == nsGkAtoms::menu;
}

void
nsBlockFrame::RenumberLists(nsPresContext* aPresContext)
{
  if (!FrameStartsCounterScope(this)) {
    // If this frame doesn't start a counter scope then we don't need
    // to renumber child list items.
    return;
  }

  // Setup initial list ordinal value
  // XXX Map html's start property to counter-reset style
  PRInt32 ordinal = 1;

  nsGenericHTMLElement *hc = nsGenericHTMLElement::FromContent(mContent);

  if (hc) {
    const nsAttrValue* attr = hc->GetParsedAttr(nsGkAtoms::start);
    if (attr && attr->Type() == nsAttrValue::eInteger) {
      ordinal = attr->GetIntegerValue();
    }
  }

  // Get to first-in-flow
  nsBlockFrame* block = (nsBlockFrame*) GetFirstInFlow();
  if (RenumberListsInBlock(aPresContext, block, &ordinal, 0))
    AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
}

PRBool
nsBlockFrame::RenumberListsInBlock(nsPresContext* aPresContext,
                                   nsBlockFrame* aBlockFrame,
                                   PRInt32* aOrdinal,
                                   PRInt32 aDepth)
{
  PRBool renumberedABullet = PR_FALSE;

  while (nsnull != aBlockFrame) {
    // Examine each line in the block
    for (line_iterator line = aBlockFrame->begin_lines(),
                       line_end = aBlockFrame->end_lines();
         line != line_end;
         ++line) {
      nsIFrame* kid = line->mFirstChild;
      PRInt32 n = line->GetChildCount();
      while (--n >= 0) {
        PRBool kidRenumberedABullet = RenumberListsFor(aPresContext, kid, aOrdinal, aDepth);
        if (kidRenumberedABullet) {
          line->MarkDirty();
          renumberedABullet = PR_TRUE;
        }
        kid = kid->GetNextSibling();
      }
    }

    // Advance to the next continuation
    aBlockFrame = NS_STATIC_CAST(nsBlockFrame*, aBlockFrame->GetNextInFlow());
  }

  return renumberedABullet;
}

PRBool
nsBlockFrame::RenumberListsFor(nsPresContext* aPresContext,
                               nsIFrame* aKid,
                               PRInt32* aOrdinal,
                               PRInt32 aDepth)
{
  NS_PRECONDITION(aPresContext && aKid && aOrdinal, "null params are immoral!");

  // add in a sanity check for absurdly deep frame trees.  See bug 42138
  if (MAX_DEPTH_FOR_LIST_RENUMBERING < aDepth)
    return PR_FALSE;

  PRBool kidRenumberedABullet = PR_FALSE;

  // if the frame is a placeholder, then get the out of flow frame
  nsIFrame* kid = nsPlaceholderFrame::GetRealFrameFor(aKid);

  // drill down through any wrappers to the real frame
  kid = kid->GetContentInsertionFrame();

  // If the frame is a list-item and the frame implements our
  // block frame API then get its bullet and set the list item
  // ordinal.
  const nsStyleDisplay* display = kid->GetStyleDisplay();
  if (NS_STYLE_DISPLAY_LIST_ITEM == display->mDisplay) {
    // Make certain that the frame is a block frame in case
    // something foreign has crept in.
    nsBlockFrame* listItem;
    nsresult rv = kid->QueryInterface(kBlockFrameCID, (void**)&listItem);
    if (NS_SUCCEEDED(rv)) {
      if (nsnull != listItem->mBullet) {
        PRBool changed;
        *aOrdinal = listItem->mBullet->SetListItemOrdinal(*aOrdinal,
                                                          &changed);
        if (changed) {
          kidRenumberedABullet = PR_TRUE;

          // Invalidate the bullet content area since it may look different now
          nsRect damageRect(nsPoint(0, 0), listItem->mBullet->GetSize());
          listItem->mBullet->Invalidate(damageRect);
        }
      }

      // XXX temporary? if the list-item has child list-items they
      // should be numbered too; especially since the list-item is
      // itself (ASSUMED!) not to be a counter-resetter.
      PRBool meToo = RenumberListsInBlock(aPresContext, listItem, aOrdinal, aDepth + 1);
      if (meToo) {
        kidRenumberedABullet = PR_TRUE;
      }
    }
  }
  else if (NS_STYLE_DISPLAY_BLOCK == display->mDisplay) {
    if (FrameStartsCounterScope(kid)) {
      // Don't bother recursing into a block frame that is a new
      // counter scope. Any list-items in there will be handled by
      // it.
    }
    else {
      // If the display=block element is a block frame then go ahead
      // and recurse into it, as it might have child list-items.
      nsBlockFrame* kidBlock;
      nsresult rv = kid->QueryInterface(kBlockFrameCID, (void**) &kidBlock);
      if (NS_SUCCEEDED(rv)) {
        kidRenumberedABullet = RenumberListsInBlock(aPresContext, kidBlock, aOrdinal, aDepth + 1);
      }
    }
  }
  return kidRenumberedABullet;
}

void
nsBlockFrame::ReflowBullet(nsBlockReflowState& aState,
                           nsHTMLReflowMetrics& aMetrics)
{
  const nsHTMLReflowState &rs = aState.mReflowState;

  // Reflow the bullet now
  nsSize availSize;
  // Make up a width since it doesn't really matter (XXX).
  availSize.width = rs.ComputedWidth();
  availSize.height = NS_UNCONSTRAINEDSIZE;

  // Get the reason right.
  // XXXwaterson Should this look just like the logic in
  // nsBlockReflowContext::ReflowBlock and nsLineLayout::ReflowFrame?
  nsHTMLReflowState reflowState(aState.mPresContext, rs,
                                mBullet, availSize);
  nsReflowStatus  status;
  mBullet->WillReflow(aState.mPresContext);
  mBullet->Reflow(aState.mPresContext, aMetrics, reflowState, status);

  // Place the bullet now; use its right margin to distance it
  // from the rest of the frames in the line
  nscoord x = 
#ifdef IBMBIDI
           (NS_STYLE_DIRECTION_RTL == GetStyleVisibility()->mDirection)
             // According to the CSS2 spec, section 12.6.1, outside marker box
             // is distanced from the associated principal box's border edge.
             // |rs.availableWidth| reflects exactly a border edge: it includes
             // border, padding, and content area, without margins.
             ? rs.availableWidth + reflowState.mComputedMargin.left :
#endif
             - reflowState.mComputedMargin.right - aMetrics.width;

  // Approximate the bullets position; vertical alignment will provide
  // the final vertical location.
  const nsMargin& bp = aState.BorderPadding();
  nscoord y = bp.top;
  mBullet->SetRect(nsRect(x, y, aMetrics.width, aMetrics.height));
  mBullet->DidReflow(aState.mPresContext, &aState.mReflowState, NS_FRAME_REFLOW_FINISHED);
}

// This is used to scan frames for any float placeholders, add their
// floats to the list represented by aHead and aTail, and remove the
// floats from whatever list they might be in. We don't search descendants
// that are float containing blocks. The floats must be children of 'this'.
void nsBlockFrame::CollectFloats(nsIFrame* aFrame, nsFrameList& aList, nsIFrame** aTail,
                                 PRBool aFromOverflow) {
  while (aFrame) {
    // Don't descend into float containing blocks.
    if (!aFrame->IsFloatContainingBlock()) {
      nsIFrame *outOfFlowFrame = nsLayoutUtils::GetFloatFromPlaceholder(aFrame);
      if (outOfFlowFrame) {
        // Make sure that its parent is us. Otherwise we don't want
        // to mess around with it because it belongs to someone
        // else. I think this could happen if the overflow lines
        // contain a block descendant which owns its own floats.
        NS_ASSERTION(outOfFlowFrame->GetParent() == this,
                     "Out of flow frame doesn't have the expected parent");
        if (aFromOverflow) {
          nsAutoOOFFrameList oofs(this);
          oofs.mList.RemoveFrame(outOfFlowFrame);
        } else {
          mFloats.RemoveFrame(outOfFlowFrame);
        }
        aList.InsertFrame(nsnull, *aTail, outOfFlowFrame);
        *aTail = outOfFlowFrame;
      }

      CollectFloats(aFrame->GetFirstChild(nsnull), aList, aTail, aFromOverflow);
    }
    
    aFrame = aFrame->GetNextSibling();
  }
}

void
nsBlockFrame::CheckFloats(nsBlockReflowState& aState)
{
#ifdef DEBUG
  // Check that the float list is what we would have built
  nsAutoVoidArray lineFloats;
  for (line_iterator line = begin_lines(), line_end = end_lines();
       line != line_end; ++line) {
    if (line->HasFloats()) {
      nsFloatCache* fc = line->GetFirstFloat();
      while (fc) {
        nsIFrame* floatFrame = fc->mPlaceholder->GetOutOfFlowFrame();
        lineFloats.AppendElement(floatFrame);
        fc = fc->Next();
      }
    }
  }
  
  nsAutoVoidArray storedFloats;
  PRBool equal = PR_TRUE;
  PRInt32 i = 0;
  for (nsIFrame* f = mFloats.FirstChild(); f; f = f->GetNextSibling()) {
    storedFloats.AppendElement(f);
    if (i < lineFloats.Count() && lineFloats.ElementAt(i) != f) {
      equal = PR_FALSE;
    }
    ++i;
  }

  if (!equal || lineFloats.Count() != storedFloats.Count()) {
    NS_WARNING("nsBlockFrame::CheckFloats: Explicit float list is out of sync with float cache");
#if defined(DEBUG_roc)
    nsIFrameDebug::RootFrameList(GetPresContext(), stdout, 0);
    for (i = 0; i < lineFloats.Count(); ++i) {
      printf("Line float: %p\n", lineFloats.ElementAt(i));
    }
    for (i = 0; i < storedFloats.Count(); ++i) {
      printf("Stored float: %p\n", storedFloats.ElementAt(i));
    }
#endif
  }
#endif

  nsFrameList oofs = GetOverflowOutOfFlows();
  if (oofs.NotEmpty()) {
    // Floats that were pushed should be removed from our space
    // manager.  Otherwise the space manager's YMost or XMost might
    // be larger than necessary, causing this block to get an
    // incorrect desired height (or width).  Some of these floats
    // may not actually have been added to the space manager because
    // they weren't reflowed before being pushed; that's OK,
    // RemoveRegions will ignore them. It is safe to do this here
    // because we know from here on the space manager will only be
    // used for its XMost and YMost, not to place new floats and
    // lines.
    aState.mSpaceManager->RemoveTrailingRegions(oofs.FirstChild());
  }
}

NS_IMETHODIMP
nsBlockFrame::SetParent(const nsIFrame* aParent)
{
  nsresult rv = nsBlockFrameSuper::SetParent(aParent);
  if (IsBoxWrapped())
    mState |= NS_BLOCK_SPACE_MGR;

  // XXX should we clear NS_BLOCK_SPACE_MGR if we were the child of a box
  // but no longer are?

  return rv;
}

// XXX keep the text-run data in the first-in-flow of the block

#ifdef DEBUG
void
nsBlockFrame::VerifyLines(PRBool aFinalCheckOK)
{
  if (!gVerifyLines) {
    return;
  }
  if (mLines.empty()) {
    return;
  }

  // Add up the counts on each line. Also validate that IsFirstLine is
  // set properly.
  PRInt32 count = 0;
  PRBool seenBlock = PR_FALSE;
  line_iterator line, line_end;
  for (line = begin_lines(), line_end = end_lines();
       line != line_end;
       ++line) {
    if (aFinalCheckOK) {
      NS_ABORT_IF_FALSE(line->GetChildCount(), "empty line");
      if (line->IsBlock()) {
        seenBlock = PR_TRUE;
      }
      if (line->IsBlock()) {
        NS_ASSERTION(1 == line->GetChildCount(), "bad first line");
      }
    }
    count += line->GetChildCount();
  }

  // Then count the frames
  PRInt32 frameCount = 0;
  nsIFrame* frame = mLines.front()->mFirstChild;
  while (frame) {
    frameCount++;
    frame = frame->GetNextSibling();
  }
  NS_ASSERTION(count == frameCount, "bad line list");

  // Next: test that each line has right number of frames on it
  for (line = begin_lines(), line_end = end_lines();
       line != line_end;
        ) {
    count = line->GetChildCount();
    frame = line->mFirstChild;
    while (--count >= 0) {
      frame = frame->GetNextSibling();
    }
    ++line;
    if ((line != line_end) && (0 != line->GetChildCount())) {
      NS_ASSERTION(frame == line->mFirstChild, "bad line list");
    }
  }
}

// Its possible that a frame can have some frames on an overflow
// list. But its never possible for multiple frames to have overflow
// lists. Check that this fact is actually true.
void
nsBlockFrame::VerifyOverflowSituation()
{
  nsBlockFrame* flow = (nsBlockFrame*) GetFirstInFlow();
  while (nsnull != flow) {
    nsLineList* overflowLines = GetOverflowLines();
    if (nsnull != overflowLines) {
      NS_ASSERTION(! overflowLines->empty(), "should not be empty if present");
      NS_ASSERTION(overflowLines->front()->mFirstChild, "bad overflow list");
    }
    flow = (nsBlockFrame*) flow->GetNextInFlow();
  }
}

PRInt32
nsBlockFrame::GetDepth() const
{
  PRInt32 depth = 0;
  nsIFrame* parent = mParent;
  while (parent) {
    parent = parent->GetParent();
    depth++;
  }
  return depth;
}
#endif

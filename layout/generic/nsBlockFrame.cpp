/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *   L. David Baron <dbaron@fas.harvard.edu>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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
#include "nsHTMLIIDs.h"
#include "nsCSSRendering.h"
#include "nsIFrameManager.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIReflowCommand.h"
#include "nsISpaceManager.h"
#include "nsIStyleContext.h"
#include "nsIView.h"
#include "nsIFontMetrics.h"
#include "nsHTMLParts.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLValue.h"
#include "nsIDOMEvent.h"
#include "nsIHTMLContent.h"
#include "prprf.h"
#include "nsLayoutAtoms.h"
#include "nsITextContent.h"
#include "nsStyleChangeList.h"
#include "nsISizeOfHandler.h"
#include "nsIFocusTracker.h"
#include "nsIFrameSelection.h"
#include "nsSpaceManager.h"
#include "nsIntervalSet.h"
#include "prenv.h"
#include "plstr.h"
#include "nsGUIEvent.h"

#ifdef IBMBIDI
#include "nsBidiPresUtils.h"
#endif // IBMBIDI

#include "nsIDOMHTMLBodyElement.h"
#include "nsIDOMHTMLHtmlElement.h"

#ifdef DEBUG
#include "nsBlockDebugFlags.h"


PRBool nsBlockFrame::gLamePaintMetrics;
PRBool nsBlockFrame::gLameReflowMetrics;
PRBool nsBlockFrame::gNoisy;
PRBool nsBlockFrame::gNoisyDamageRepair;
PRBool nsBlockFrame::gNoisyMaxElementSize;
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

static BlockDebugFlags gFlags[] = {
  { "reflow", &nsBlockFrame::gNoisyReflow },
  { "really-noisy-reflow", &nsBlockFrame::gReallyNoisyReflow },
  { "max-element-size", &nsBlockFrame::gNoisyMaxElementSize },
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
  BlockDebugFlags* bdf = gFlags;
  BlockDebugFlags* end = gFlags + NUM_DEBUG_FLAGS;
  for (; bdf < end; bdf++) {
    printf("  %s\n", bdf->name);
  }
  printf("Note: GECKO_BLOCK_DEBUG_FLAGS is a comma seperated list of flag\n");
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
        BlockDebugFlags* bdf = gFlags;
        BlockDebugFlags* end = gFlags + NUM_DEBUG_FLAGS;
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
static const char* kReflowCommandType[] = {
  "ContentChanged",
  "StyleChanged",
  "ReflowDirty",
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
  nsIStyleContext* sc;
  aFrame->GetStyleContext(&sc);
  while (nsnull != sc) {
    nsIStyleContext* psc;
    printf("%p ", sc);
    psc = sc->GetParent();
    NS_RELEASE(sc);
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

void
nsBlockFrame::CombineRects(const nsRect& r1, nsRect& r2)
{
  nscoord xa = r2.x;
  nscoord ya = r2.y;
  nscoord xb = xa + r2.width;
  nscoord yb = ya + r2.height;
  nscoord x = r1.x;
  nscoord y = r1.y;
  nscoord xmost = x + r1.width;
  nscoord ymost = y + r1.height;
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
  r2.x = xa;
  r2.y = ya;
  r2.width = xb - xa;
  r2.height = yb - ya;
}

//----------------------------------------------------------------------

const nsIID kBlockFrameCID = NS_BLOCK_FRAME_CID;

nsresult
NS_NewBlockFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRUint32 aFlags)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsBlockFrame* it = new (aPresShell) nsBlockFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  it->SetFlags(aFlags);
  *aNewFrame = it;
  return NS_OK;
}

nsBlockFrame::nsBlockFrame()
{
#ifdef DEBUG
  InitDebugFlags();
#endif
}

nsBlockFrame::~nsBlockFrame()
{
}

NS_IMETHODIMP
nsBlockFrame::Destroy(nsIPresContext* aPresContext)
{
  mAbsoluteContainer.DestroyFrames(this, aPresContext);
  // Outside bullets are not in our child-list so check for them here
  // and delete them when present.
  if (mBullet && HaveOutsideBullet()) {
    mBullet->Destroy(aPresContext);
    mBullet = nsnull;
  }

  mFloaters.DestroyFrames(aPresContext);

  nsLineBox::DeleteLineList(aPresContext, mLines);

  return nsBlockFrameSuper::Destroy(aPresContext);
}

NS_IMETHODIMP
nsBlockFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kBlockFrameCID)) {
    nsBlockFrame* tmp = this;
    *aInstancePtr = (void*) tmp;
    return NS_OK;
  }
  if ( aIID.Equals(NS_GET_IID(nsILineIterator)) ||
       aIID.Equals(NS_GET_IID(nsILineIteratorNavigator)) )
  {
    nsLineIterator* it = new nsLineIterator;
    if (!it) {
      *aInstancePtr = nsnull;
      return NS_ERROR_OUT_OF_MEMORY;
    }
    const nsStyleVisibility* visibility;
    GetStyleData(eStyleStruct_Visibility, (const nsStyleStruct*&) visibility);
    nsresult rv = it->Init(mLines,
                           visibility->mDirection == NS_STYLE_DIRECTION_RTL);
    if (NS_FAILED(rv)) {
      delete it;
      return rv;
    }
    NS_ADDREF((nsILineIterator *) (*aInstancePtr = (void *) it));
    return NS_OK;
  }
  return nsBlockFrameSuper::QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP
nsBlockFrame::IsSplittable(nsSplittableType& aIsSplittable) const
{
  aIsSplittable = NS_FRAME_SPLITTABLE_NON_RECTANGULAR;
  return NS_OK;
}

#ifdef DEBUG
NS_METHOD
nsBlockFrame::List(nsIPresContext* aPresContext, FILE* out, PRInt32 aIndent) const
{
  IndentBy(out, aIndent);
  ListTag(out);
#ifdef DEBUG_waterson
  fprintf(out, " [parent=%p]", mParent);
#endif
  nsIView* view;
  GetView(aPresContext, &view);
  if (nsnull != view) {
    fprintf(out, " [view=%p]", NS_STATIC_CAST(void*, view));
  }
  if (nsnull != mNextSibling) {
    fprintf(out, " next=%p", NS_STATIC_CAST(void*, mNextSibling));
  }

  // Output the flow linkage
  if (nsnull != mPrevInFlow) {
    fprintf(out, " prev-in-flow=%p", NS_STATIC_CAST(void*, mPrevInFlow));
  }
  if (nsnull != mNextInFlow) {
    fprintf(out, " next-in-flow=%p", NS_STATIC_CAST(void*, mNextInFlow));
  }

  // Output the rect and state
  fprintf(out, " {%d,%d,%d,%d}", mRect.x, mRect.y, mRect.width, mRect.height);
  if (0 != mState) {
    fprintf(out, " [state=%08x]", mState);
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
  fprintf(out, " sc=%p(i=%d,b=%d)<\n",
          NS_STATIC_CAST(void*, mStyleContext), numInlineLines, numBlockLines);
  aIndent++;

  // Output the lines
  if (! mLines.empty()) {
    for (const_line_iterator line = begin_lines(), line_end = end_lines();
         line != line_end;
         ++line)
    {
      line->List(aPresContext, out, aIndent);
    }
  }

  nsIAtom* listName = nsnull;
  PRInt32 listIndex = 0;
  for (;;) {
    nsIFrame* kid;
    GetAdditionalChildListName(listIndex++, &listName);
    if (nsnull == listName) {
      break;
    }
    FirstChild(aPresContext, listName, &kid);
    if (nsnull != kid) {
      IndentBy(out, aIndent);
      nsAutoString tmp;
      if (nsnull != listName) {
        listName->ToString(tmp);
        fputs(NS_LossyConvertUCS2toASCII(tmp).get(), out);
      }
      fputs("<\n", out);
      while (nsnull != kid) {
        nsIFrameDebug*  frameDebug;

        if (NS_SUCCEEDED(CallQueryInterface(kid, &frameDebug))) {
          frameDebug->List(aPresContext, out, aIndent + 1);
        }
        kid->GetNextSibling(&kid);
      }
      IndentBy(out, aIndent);
      fputs(">\n", out);
    }
    NS_IF_RELEASE(listName);
  }

  aIndent--;
  IndentBy(out, aIndent);
  fputs(">\n", out);

  return NS_OK;
}

NS_IMETHODIMP
nsBlockFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("Block", aResult);
}
#endif

NS_IMETHODIMP
nsBlockFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::blockFrame; 
  NS_ADDREF(*aType);
  return NS_OK;
}

/////////////////////////////////////////////////////////////////////////////
// Child frame enumeration

NS_IMETHODIMP
nsBlockFrame::FirstChild(nsIPresContext* aPresContext,
                         nsIAtom*        aListName,
                         nsIFrame**      aFirstChild) const
{
  NS_PRECONDITION(nsnull != aFirstChild, "null OUT parameter pointer");
  if (aListName == nsLayoutAtoms::absoluteList) {
    return mAbsoluteContainer.FirstChild(this, aListName, aFirstChild);
  }
  else if (nsnull == aListName) {
    *aFirstChild = (mLines.empty()) ? nsnull : mLines.front()->mFirstChild;
    return NS_OK;
  }
  else if (aListName == nsLayoutAtoms::overflowList) {
    nsLineList* overflowLines = GetOverflowLines(aPresContext, PR_FALSE);
    *aFirstChild = overflowLines
                       ? overflowLines->front()->mFirstChild
                       : nsnull;
    return NS_OK;
  }
  else if (aListName == nsLayoutAtoms::floaterList) {
    *aFirstChild = mFloaters.FirstChild();
    return NS_OK;
  }
  else if (aListName == nsLayoutAtoms::bulletList) {
    if (HaveOutsideBullet()) {
      *aFirstChild = mBullet;
    }
    else {
      *aFirstChild = nsnull;
    }
    return NS_OK;
  }
  *aFirstChild = nsnull;
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
nsBlockFrame::GetAdditionalChildListName(PRInt32   aIndex,
                                         nsIAtom** aListName) const
{
  NS_PRECONDITION(nsnull != aListName, "null OUT parameter pointer");
  if (aIndex < 0) {
    return NS_ERROR_INVALID_ARG;
  }
  *aListName = nsnull;
  switch (aIndex) {
  case NS_BLOCK_FRAME_FLOATER_LIST_INDEX:
    *aListName = nsLayoutAtoms::floaterList;
    NS_ADDREF(*aListName);
    break;
  case NS_BLOCK_FRAME_BULLET_LIST_INDEX:
    *aListName = nsLayoutAtoms::bulletList;
    NS_ADDREF(*aListName);
    break;
  case NS_BLOCK_FRAME_ABSOLUTE_LIST_INDEX:
    *aListName = nsLayoutAtoms::absoluteList;
    NS_ADDREF(*aListName);
    break;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsBlockFrame::IsPercentageBase(PRBool& aBase) const
{
  aBase = PR_TRUE;
  return NS_OK;
}

//////////////////////////////////////////////////////////////////////
// Frame structure methods

//////////////////////////////////////////////////////////////////////
// Reflow methods

static void
CalculateContainingBlock(const nsHTMLReflowState& aReflowState,
                         nscoord                  aFrameWidth,
                         nscoord                  aFrameHeight,
                         nscoord&                 aContainingBlockWidth,
                         nscoord&                 aContainingBlockHeight)
{
  aContainingBlockWidth = -1;  // have reflow state calculate
  aContainingBlockHeight = -1; // have reflow state calculate

  // The issue there is that for a 'height' of 'auto' the reflow state code
  // won't know how to calculate the containing block height because it's
  // calculated bottom up. We don't really want to do this for the initial
  // containing block so that's why we have the check for if the element
  // is absolutely or relatively positioned
  if (aReflowState.mStyleDisplay->IsAbsolutelyPositioned() ||
      (NS_STYLE_POSITION_RELATIVE == aReflowState.mStyleDisplay->mPosition)) {
    aContainingBlockWidth = aFrameWidth;
    aContainingBlockHeight = aFrameHeight;

    // Containing block is relative to the padding edge
    nsMargin  border;
    if (!aReflowState.mStyleBorder->GetBorder(border)) {
      NS_NOTYETIMPLEMENTED("percentage border");
    }
    aContainingBlockWidth -= border.left + border.right;
    aContainingBlockHeight -= border.top + border.bottom;
  }
}


NS_IMETHODIMP
nsBlockFrame::Reflow(nsIPresContext*          aPresContext,
                     nsHTMLReflowMetrics&     aMetrics,
                     const nsHTMLReflowState& aReflowState,
                     nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsBlockFrame", aReflowState.reason);

#ifdef DEBUG
  if (gNoisyReflow) {
    IndentBy(stdout, gNoiseIndent);
    ListTag(stdout);
    printf(": begin %s reflow availSize=%d,%d computedSize=%d,%d\n",
           nsHTMLReflowState::ReasonToString(aReflowState.reason),
           aReflowState.availableWidth, aReflowState.availableHeight,
           aReflowState.mComputedWidth, aReflowState.mComputedHeight);
  }
  if (gNoisy) {
    gNoiseIndent++;
  }
  PRTime start = LL_ZERO; // Initialize these variablies to silence the compiler.
  PRInt32 ctc = 0;        // We only use these if they are set (gLameReflowMetrics).
  if (gLameReflowMetrics) {
    start = PR_Now();
    ctc = nsLineBox::GetCtorCount();
  }
#endif

  // See if it's an incremental reflow command
  if (eReflowReason_Incremental == aReflowState.reason) {
    // Give the absolute positioning code a chance to handle it
    nscoord containingBlockWidth;
    nscoord containingBlockHeight;
    PRBool  handled;
    nsRect  childBounds;

    CalculateContainingBlock(aReflowState, mRect.width, mRect.height,
                             containingBlockWidth, containingBlockHeight);
    
    mAbsoluteContainer.IncrementalReflow(this, aPresContext, aReflowState,
                                         containingBlockWidth, containingBlockHeight,
                                         handled, childBounds);

    // If the incremental reflow command was handled by the absolute positioning
    // code, then we're all done
    if (handled) {
      // Just return our current size as our desired size.
      // XXX We need to know the overflow area for the flowed content, and
      // we don't have a way to get that currently so for the time being pretend
      // a resize reflow occured
#if 0
      aMetrics.width = mRect.width;
      aMetrics.height = mRect.height;
      aMetrics.ascent = mRect.height;
      aMetrics.descent = 0;
  
      // Whether or not we're complete hasn't changed
      aStatus = (nsnull != mNextInFlow) ? NS_FRAME_NOT_COMPLETE : NS_FRAME_COMPLETE;
#else
      nsHTMLReflowState reflowState(aReflowState);
      reflowState.reason = eReflowReason_Resize;
      reflowState.reflowCommand = nsnull;
      nsBlockFrame::Reflow(aPresContext, aMetrics, reflowState, aStatus);
#endif
      
      // Factor the absolutely positioned child bounds into the overflow area
      aMetrics.mOverflowArea.UnionRect(aMetrics.mOverflowArea, childBounds);

      // Make sure the NS_FRAME_OUTSIDE_CHILDREN flag is set correctly
      if ((aMetrics.mOverflowArea.x < 0) ||
          (aMetrics.mOverflowArea.y < 0) ||
          (aMetrics.mOverflowArea.XMost() > aMetrics.width) ||
          (aMetrics.mOverflowArea.YMost() > aMetrics.height)) {
        mState |= NS_FRAME_OUTSIDE_CHILDREN;
      } else {
        mState &= ~NS_FRAME_OUTSIDE_CHILDREN;
      }
      return NS_OK;
    }
  }

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

  // Should we create a space manager?
  nsCOMPtr<nsISpaceManager> spaceManager;
  nsISpaceManager*          oldSpaceManager = aReflowState.mSpaceManager;
  // XXXldb If we start storing the space manager in the frame rather
  // than keeping it around only during reflow then we should create it
  // only when there are actually floats to manage.  Otherwise things
  // like tables will gain significant bloat.
  if (NS_BLOCK_SPACE_MGR & mState) {
    nsCOMPtr<nsIPresShell> shell;
    aPresContext->GetShell(getter_AddRefs(shell));
    nsSpaceManager* rawPtr = nsSpaceManager::Create(shell, this);
    if (!rawPtr) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    spaceManager = do_QueryInterface(rawPtr);

    // Set the space manager in the existing reflow state
    nsHTMLReflowState& reflowState =
      NS_CONST_CAST(nsHTMLReflowState&, aReflowState);
    reflowState.mSpaceManager = spaceManager.get();
#ifdef NOISY_SPACEMANAGER
    printf("constructed new space manager %p (replacing %p)\n", reflowState.mSpaceManager, oldSpaceManager);
#endif
  }

  nsBlockReflowState state(aReflowState, aPresContext, this, aMetrics,
                           NS_BLOCK_MARGIN_ROOT & mState);

  if (eReflowReason_Resize != aReflowState.reason) {
#ifdef IBMBIDI
    if (! mLines.empty()) {
      PRBool bidiEnabled;
      aPresContext->GetBidiEnabled(&bidiEnabled);
      if (bidiEnabled) {
        nsBidiPresUtils* bidiUtils;
        aPresContext->GetBidiUtils(&bidiUtils);
        if (bidiUtils) {
          PRBool forceReflow;
          nsresult rc = bidiUtils->Resolve(aPresContext, this,
                                           mLines.front()->mFirstChild,
                                           forceReflow);
          if (NS_SUCCEEDED(rc) && forceReflow) {
            // Mark everything dirty
            // XXXldb This should be done right.
            for (line_iterator line = begin_lines(), line_end = end_lines();
                 line != line_end;
                 ++line)
            {
              line->MarkDirty();
            }
          }
        }
      }
    }
#endif // IBMBIDI
    RenumberLists(aPresContext);
  }

  nsresult rv = NS_OK;
  PRBool isStyleChange = PR_FALSE;

  nsIFrame* target;
  switch (aReflowState.reason) {
  case eReflowReason_Initial:
#ifdef NOISY_REFLOW_REASON
    ListTag(stdout);
    printf(": reflow=initial\n");
#endif
    DrainOverflowLines(aPresContext);
    rv = PrepareInitialReflow(state);
    mState &= ~NS_FRAME_FIRST_REFLOW;
    break;  

  case eReflowReason_Dirty:    
    break;

  case eReflowReason_Incremental:  // should call GetNext() ?
    aReflowState.reflowCommand->GetTarget(target);
    if (this == target) {
      nsIReflowCommand::ReflowType type;
      aReflowState.reflowCommand->GetType(type);
#ifdef NOISY_REFLOW_REASON
      ListTag(stdout);
      printf(": reflow=incremental type=%d\n", type);
#endif
      switch (type) {
      case nsIReflowCommand::StyleChanged:
        rv = PrepareStyleChangedReflow(state);
        isStyleChange = PR_TRUE;
        break;
      case nsIReflowCommand::ReflowDirty:
        break;
      default:
        // Map any other incremental operations into full reflows
        rv = PrepareResizeReflow(state);
        break;
      }
    }
    else {
      // Get next frame in reflow command chain
      aReflowState.reflowCommand->GetNext(state.mNextRCFrame);
#ifdef NOISY_REFLOW_REASON
      ListTag(stdout);
      printf(": reflow=incremental");
      if (state.mNextRCFrame) {
        printf(" next=");
        nsFrame::ListTag(stdout, state.mNextRCFrame);
      }
      printf("\n");
#endif

      rv = PrepareChildIncrementalReflow(state);
    }
    break;

  case eReflowReason_StyleChange:
    DrainOverflowLines(aPresContext);
    rv = PrepareStyleChangedReflow(state);
    break;

  case eReflowReason_Resize:
  default:
#ifdef NOISY_REFLOW_REASON
    ListTag(stdout);
    printf(": reflow=resize (%d)\n", aReflowState.reason);
#endif
    DrainOverflowLines(aPresContext);
    rv = PrepareResizeReflow(state);
    break;
  }

  NS_ASSERTION(NS_SUCCEEDED(rv), "setting up reflow failed");
  if (NS_FAILED(rv)) return rv;

  // Now reflow...
  rv = ReflowDirtyLines(state);
  NS_ASSERTION(NS_SUCCEEDED(rv), "reflow dirty lines failed");
  if (NS_FAILED(rv)) return rv;

  if (!state.GetFlag(BRS_ISINLINEINCRREFLOW)) {
    // XXXwaterson are we sure we don't need to do this work if BRS_ISINLINEINCRREFLOW?
    aStatus = state.mReflowStatus;
    if (NS_FRAME_IS_NOT_COMPLETE(aStatus)) {
      if (NS_STYLE_OVERFLOW_HIDDEN == aReflowState.mStyleDisplay->mOverflow) {
        aStatus = NS_FRAME_COMPLETE;
      }
      else {
#ifdef DEBUG_kipp
        ListTag(stdout); printf(": block is not complete\n");
#endif
      }
    }

    // XXX_perf get rid of this!
    BuildFloaterList();
  }
  
  // Compute our final size
  ComputeFinalSize(aReflowState, state, aMetrics);

  // see if verifyReflow is enabled, and if so store off the space manager pointer
#ifdef DEBUG
  PRInt32 verifyReflowFlags = nsIPresShell::GetVerifyReflowFlags();
  if (VERIFY_REFLOW_INCLUDE_SPACE_MANAGER & verifyReflowFlags)
  {
    // this is a leak of the space manager, but it's only in debug if verify reflow is enabled, so not a big deal
    nsCOMPtr<nsIPresShell> shell;
    aPresContext->GetShell(getter_AddRefs(shell));
    if (shell) {
      nsCOMPtr<nsIFrameManager>  frameManager;
      shell->GetFrameManager(getter_AddRefs(frameManager));  
      if (frameManager) {
        nsHTMLReflowState&  reflowState = (nsHTMLReflowState&)aReflowState;
        NS_ADDREF(reflowState.mSpaceManager);
        rv = frameManager->SetFrameProperty(this, nsLayoutAtoms::spaceManagerProperty,
                                            reflowState.mSpaceManager, nsnull /* should be nsSpaceManagerDestroyer*/);
      }
    }
  }
#endif

  // If we set the space manager, then restore the old space manager now that we're
  // going out of scope
  if (NS_BLOCK_SPACE_MGR & mState) {
    nsHTMLReflowState&  reflowState = NS_CONST_CAST(nsHTMLReflowState&, aReflowState);
#ifdef NOISY_SPACEMANAGER
    printf("restoring old space manager %p\n", oldSpaceManager);
#endif
    reflowState.mSpaceManager = oldSpaceManager;
  }

  NS_ASSERTION(aReflowState.mSpaceManager == oldSpaceManager, "lost a space manager");

#ifdef NOISY_SPACEMANAGER
  nsHTMLReflowState&  reflowState = NS_CONST_CAST(nsHTMLReflowState&, aReflowState);
  if (reflowState.mSpaceManager) {
    ListTag(stdout);
    printf(": space-manager %p after reflow\n", reflowState.mSpaceManager);
    reflowState.mSpaceManager->List(stdout);
  }
#endif
  
  // If this is an incremental reflow and we changed size, then make sure our
  // border is repainted if necessary
  //
  // XXXwaterson are we sure we can skip this if BRS_ISINLINEINCRREFLOW?
  if (!state.GetFlag(BRS_ISINLINEINCRREFLOW) &&
      (eReflowReason_Incremental == aReflowState.reason ||
       eReflowReason_Dirty == aReflowState.reason)) {
    if (isStyleChange) {
      // This is only true if it's a style change reflow targeted at this
      // frame (rather than an ancestor) (I think).  That seems to be
      // what's wanted here.
      // Lots of things could have changed so damage our entire
      // bounds
#ifdef NOISY_BLOCK_INVALIDATE
      printf("%p invalidate 1 (%d, %d, %d, %d)\n",
             this, 0, 0, mRect.width, mRect.height);
#endif

      Invalidate(aPresContext, nsRect(0, 0, mRect.width, mRect.height));
    } else {
      nsMargin  border = aReflowState.mComputedBorderPadding -
                         aReflowState.mComputedPadding;
  
      // See if our width changed
      if ((aMetrics.width != mRect.width) && (border.right > 0)) {
        nsRect  damageRect;
  
        if (aMetrics.width < mRect.width) {
          // Our new width is smaller, so we need to make sure that
          // we paint our border in its new position
          damageRect.x = aMetrics.width - border.right;
          damageRect.width = border.right;
          damageRect.y = 0;
          damageRect.height = aMetrics.height;
  
        } else {
          // Our new width is larger, so we need to erase our border in its
          // old position
          damageRect.x = mRect.width - border.right;
          damageRect.width = border.right;
          damageRect.y = 0;
          damageRect.height = mRect.height;
        }
#ifdef NOISY_BLOCK_INVALIDATE
        printf("%p invalidate 2 (%d, %d, %d, %d)\n",
               this, damageRect.x, damageRect.y, damageRect.width, damageRect.height);
#endif
        Invalidate(aPresContext, damageRect);
      }
  
      // See if our height changed
      if ((aMetrics.height != mRect.height) && (border.bottom > 0)) {
        nsRect  damageRect;
        
        if (aMetrics.height < mRect.height) {
          // Our new height is smaller, so we need to make sure that
          // we paint our border in its new position
          damageRect.x = 0;
          damageRect.width = aMetrics.width;
          damageRect.y = aMetrics.height - border.bottom;
          damageRect.height = border.bottom;
  
        } else {
          // Our new height is larger, so we need to erase our border in its
          // old position
          damageRect.x = 0;
          damageRect.width = mRect.width;
          damageRect.y = mRect.height - border.bottom;
          damageRect.height = border.bottom;
        }
#ifdef NOISY_BLOCK_INVALIDATE
        printf("%p invalidate 3 (%d, %d, %d, %d)\n",
               this, damageRect.x, damageRect.y, damageRect.width, damageRect.height);
#endif
        Invalidate(aPresContext, damageRect);
      }
    }
  }

  // Let the absolutely positioned container reflow any absolutely positioned
  // child frames that need to be reflowed, e.g., elements with a percentage
  // based width/height
  //
  // XXXwaterson are we sure we can skip this if BRS_ISINLINEINCRREFLOW?
  if (NS_SUCCEEDED(rv) && mAbsoluteContainer.HasAbsoluteFrames()
      && !state.GetFlag(BRS_ISINLINEINCRREFLOW)) {
    nscoord containingBlockWidth;
    nscoord containingBlockHeight;
    nsRect  childBounds;

    CalculateContainingBlock(aReflowState, aMetrics.width, aMetrics.height,
                             containingBlockWidth, containingBlockHeight);

    rv = mAbsoluteContainer.Reflow(this, aPresContext, aReflowState,
                                   containingBlockWidth, containingBlockHeight,
                                   childBounds);

    // Factor the absolutely positioned child bounds into the overflow area
    aMetrics.mOverflowArea.UnionRect(aMetrics.mOverflowArea, childBounds);

    // Make sure the NS_FRAME_OUTSIDE_CHILDREN flag is set correctly
    if ((aMetrics.mOverflowArea.x < 0) ||
        (aMetrics.mOverflowArea.y < 0) ||
        (aMetrics.mOverflowArea.XMost() > aMetrics.width) ||
        (aMetrics.mOverflowArea.YMost() > aMetrics.height)) {
      mState |= NS_FRAME_OUTSIDE_CHILDREN;
    } else {
      mState &= ~NS_FRAME_OUTSIDE_CHILDREN;
    }
  }

#ifdef DEBUG
  if (gNoisy) {
    gNoiseIndent--;
  }
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
    if (aMetrics.maxElementSize) {
      printf(" maxElementSize=%d,%d",
             aMetrics.maxElementSize->width,
             aMetrics.maxElementSize->height);
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
#ifdef NOISY_MAX_ELEMENT_SIZE
  if (aMetrics.maxElementSize) {
    printf("block %p returning with maxElementSize=%d,%d\n", this,
           aMetrics.maxElementSize->width,
           aMetrics.maxElementSize->height);
  }
#endif

  return rv;
}

static PRBool
HaveAutoWidth(const nsHTMLReflowState& aReflowState)
{
  const nsHTMLReflowState* rs = &aReflowState;
  if (NS_UNCONSTRAINEDSIZE == rs->mComputedWidth) {
    return PR_TRUE;
  }
  const nsStylePosition* pos = rs->mStylePosition;

  for (;;) {
    if (!pos) {
      return PR_TRUE;
    }
    nsStyleUnit widthUnit = pos->mWidth.GetUnit();
    if (eStyleUnit_Auto == widthUnit) {
      return PR_TRUE;
    }
    if (eStyleUnit_Inherit != widthUnit) {
      break;
    }
    const nsHTMLReflowState* prs = rs->parentReflowState;
    if (!prs) {
      return PR_TRUE;
    }
    rs = prs;
    pos = prs->mStylePosition;
  }

  return PR_FALSE;
}


// XXXldb why do we check vertical and horizontal at the same time?  Don't
// we usually care about one or the other?
static PRBool
IsPercentageAwareChild(const nsIFrame* aFrame)
{
  NS_ASSERTION(aFrame, "null frame is not allowed");
  nsresult rv;

  const nsStyleMargin* margin;
  rv = aFrame->GetStyleData(eStyleStruct_Margin,(const nsStyleStruct*&) margin);
  if (NS_FAILED(rv)) {
    return PR_TRUE; // just to be on the safe side
  }
  if (nsLineLayout::IsPercentageUnitSides(&margin->mMargin)) {
    return PR_TRUE;
  }

  const nsStylePadding* padding;
  rv = aFrame->GetStyleData(eStyleStruct_Padding,(const nsStyleStruct*&) padding);
  if (NS_FAILED(rv)) {
    return PR_TRUE; // just to be on the safe side
  }
  if (nsLineLayout::IsPercentageUnitSides(&padding->mPadding)) {
    return PR_TRUE;
  }

  const nsStyleBorder* border;
  rv = aFrame->GetStyleData(eStyleStruct_Border,(const nsStyleStruct*&) border);
  if (NS_FAILED(rv)) {
    return PR_TRUE; // just to be on the safe side
  }
  if (nsLineLayout::IsPercentageUnitSides(&border->mBorder)) {
    return PR_TRUE;
  }

  const nsStylePosition* pos;
  rv = aFrame->GetStyleData(eStyleStruct_Position,(const nsStyleStruct*&) pos);
  if (NS_FAILED(rv)) {
    return PR_TRUE; // just to be on the safe side
  }

  if (eStyleUnit_Percent == pos->mWidth.GetUnit()
    || eStyleUnit_Percent == pos->mMaxWidth.GetUnit()
    || eStyleUnit_Percent == pos->mMinWidth.GetUnit()
    || eStyleUnit_Percent == pos->mHeight.GetUnit()
    || eStyleUnit_Percent == pos->mMinHeight.GetUnit()
    || eStyleUnit_Percent == pos->mMaxHeight.GetUnit()
    || nsLineLayout::IsPercentageUnitSides(&pos->mOffset)) { // XXX need more here!!!
    return PR_TRUE;
  }

  return PR_FALSE;
}

void
nsBlockFrame::ComputeFinalSize(const nsHTMLReflowState& aReflowState,
                               nsBlockReflowState& aState,
                               nsHTMLReflowMetrics& aMetrics)
{
  const nsMargin& borderPadding = aState.BorderPadding();
#ifdef NOISY_FINAL_SIZE
  ListTag(stdout);
  printf(": mY=%d mIsBottomMarginRoot=%s mPrevBottomMargin=%d bp=%d,%d\n",
         aState.mY, aState.GetFlag(BRS_ISBOTTOMMARGINROOT) ? "yes" : "no",
         aState.mPrevBottomMargin,
         borderPadding.top, borderPadding.bottom);
#endif

  // XXXldb Handling min-width/max-width stuff after reflowing children
  // seems wrong.  But IIRC this function only does more than a little
  // bit in rare cases (or something like that, I'm not really sure).
  // What are those cases, and do we get the wrong behavior?

  // Compute final width
  nscoord maxWidth = 0, maxHeight = 0;
#ifdef NOISY_KIDXMOST
  printf("%p aState.mKidXMost=%d\n", this, aState.mKidXMost); 
#endif
  nscoord minWidth = aState.mKidXMost + borderPadding.right;
  if (!HaveAutoWidth(aReflowState)) {
    // Use style defined width
    aMetrics.width = borderPadding.left + aReflowState.mComputedWidth +
      borderPadding.right;
    // XXX quote css1 section here
    if ((0 == aReflowState.mComputedWidth) && (aMetrics.width < minWidth)) {
      aMetrics.width = minWidth;
    }

    // When style defines the width use it for the max-element-size
    // because we can't shrink any smaller.
    maxWidth = aMetrics.width;
  }
  else {
    nscoord computedWidth = minWidth;
    PRBool compact = PR_FALSE;
#if 0
    if (NS_STYLE_DISPLAY_COMPACT == aReflowState.mStyleDisplay->mDisplay) {
      // If we are display: compact AND we have no lines or we have
      // exactly one line and that line is not a block line AND that
      // line doesn't end in a BR of any sort THEN we remain a compact
      // frame.
      if ((mLines.empty()) ||
          ((mLines.front() == mLines.back()) && !mLines.front()->IsBlock() &&
           (NS_STYLE_CLEAR_NONE == mLines.front()->GetBreakType())
           /*XXX && (computedWidth <= aState.mCompactMarginWidth) */
            )) {
        compact = PR_TRUE;
      }
    }
#endif

    // There are two options here. We either shrink wrap around our
    // contents or we fluff out to the maximum block width. Note:
    // We always shrink wrap when given an unconstrained width.
    if ((0 == (NS_BLOCK_SHRINK_WRAP & mState)) &&
        !aState.GetFlag(BRS_UNCONSTRAINEDWIDTH) &&
        !aState.GetFlag(BRS_SHRINKWRAPWIDTH) &&
        !compact) {
      // Set our width to the max width if we aren't already that
      // wide. Note that the max-width has nothing to do with our
      // contents (CSS2 section XXX)
      computedWidth = borderPadding.left + aState.mContentArea.width +
        borderPadding.right;
    }

    // See if we should compute our max element size
    if (aState.GetFlag(BRS_COMPUTEMAXELEMENTSIZE)) {
      // Add in border and padding dimensions to already computed
      // max-element-size values.
      maxWidth = aState.mMaxElementSize.width +
        borderPadding.left + borderPadding.right;
      if (computedWidth < maxWidth) {
        computedWidth = maxWidth;
      }
    }

    // Apply min/max values
    if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedMaxWidth) {
      nscoord computedMaxWidth = aReflowState.mComputedMaxWidth +
        borderPadding.left + borderPadding.right;
      if (computedWidth > computedMaxWidth) {
        computedWidth = computedMaxWidth;
      }
    }
    if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedMinWidth) {
      nscoord computedMinWidth = aReflowState.mComputedMinWidth +
        borderPadding.left + borderPadding.right;
      if (computedWidth < computedMinWidth) {
        computedWidth = computedMinWidth;
      }
    }
    aMetrics.width = computedWidth;

    // If we're shrink wrapping, then now that we know our final width we
    // need to do horizontal alignment of the inline lines and make sure
    // blocks are correctly sized and positioned. Any lines that need
    // final adjustment will have been marked as dirty
    if (aState.GetFlag(BRS_SHRINKWRAPWIDTH) && aState.GetFlag(BRS_NEEDRESIZEREFLOW)) {
      // If the parent reflow state is also shrink wrap width, then
      // we don't need to do this, because it will reflow us after it
      // calculates the final width
      PRBool  parentIsShrinkWrapWidth = PR_FALSE;
      if (aReflowState.parentReflowState) {
        if (NS_SHRINKWRAPWIDTH == aReflowState.parentReflowState->mComputedWidth) {
          parentIsShrinkWrapWidth = PR_TRUE;
        }
      }

      if (!parentIsShrinkWrapWidth) {
        nsHTMLReflowState reflowState(aReflowState);

        reflowState.mComputedWidth = aMetrics.width - borderPadding.left -
                                     borderPadding.right;
        reflowState.reason = eReflowReason_Resize;
        reflowState.mSpaceManager->ClearRegions();

#ifdef DEBUG
        nscoord oldDesiredWidth = aMetrics.width;
#endif
        nsBlockReflowState state(reflowState, aState.mPresContext, this, aMetrics,
                                 NS_BLOCK_MARGIN_ROOT & mState);
        ReflowDirtyLines(state);
        aState.mY = state.mY;
        NS_ASSERTION(oldDesiredWidth == aMetrics.width, "bad desired width");
      }
    }
  }

  if (aState.GetFlag(BRS_SHRINKWRAPWIDTH)) {
    PRBool  parentIsShrinkWrapWidth = PR_FALSE;
    if (aReflowState.parentReflowState) {
      if (NS_SHRINKWRAPWIDTH == aReflowState.parentReflowState->mComputedWidth) {
        parentIsShrinkWrapWidth = PR_TRUE;
      }
    }
  }

  // Compute final height
  if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedHeight) {
    // Use style defined height
    aMetrics.height = borderPadding.top + aReflowState.mComputedHeight +
      borderPadding.bottom;

    // When style defines the height use it for the max-element-size
    // because we can't shrink any smaller.
    maxHeight = aMetrics.height;

    // Don't carry out a bottom margin when our height is fixed
    // unless the bottom of the last line adjoins the bottom of our
    // content area.
    if (!aState.GetFlag(BRS_ISBOTTOMMARGINROOT)) {
      if (aState.mY + aState.mPrevBottomMargin.get() != aMetrics.height) {
        aState.mPrevBottomMargin.Zero();
      }
    }
  }
  else {
    nscoord autoHeight = aState.mY;

    // Shrink wrap our height around our contents.
    if (aState.GetFlag(BRS_ISBOTTOMMARGINROOT)) {
      // When we are a bottom-margin root make sure that our last
      // childs bottom margin is fully applied.
      // XXX check for a fit
      autoHeight += aState.mPrevBottomMargin.get();
    }
    autoHeight += borderPadding.bottom;

    // Apply min/max values
    if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedMaxHeight) {
      nscoord computedMaxHeight = aReflowState.mComputedMaxHeight +
        borderPadding.top + borderPadding.bottom;
      if (autoHeight > computedMaxHeight) {
        autoHeight = computedMaxHeight;
      }
    }
    if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedMinHeight) {
      nscoord computedMinHeight = aReflowState.mComputedMinHeight +
        borderPadding.top + borderPadding.bottom;
      if (autoHeight < computedMinHeight) {
        autoHeight = computedMinHeight;
      }
    }
    aMetrics.height = autoHeight;

    if (aState.GetFlag(BRS_COMPUTEMAXELEMENTSIZE)) {
      maxHeight = aState.mMaxElementSize.height +
        borderPadding.top + borderPadding.bottom;
    }
  }

  aMetrics.ascent = mAscent;
  aMetrics.descent = aMetrics.height - aMetrics.ascent;

  if (aState.GetFlag(BRS_COMPUTEMAXELEMENTSIZE)) {
    // Store away the final value
    aMetrics.maxElementSize->width = maxWidth;
    aMetrics.maxElementSize->height = maxHeight;
#ifdef NOISY_MAX_ELEMENT_SIZE
    printf ("nsBlockFrame::CFS: %p returning MES %d\n", 
             this, aMetrics.maxElementSize->width);
#endif
  }

  // Return bottom margin information
  // rbs says he hit this assertion occasionally (see bug 86947), so
  // just set the margin to zero and we'll figure out why later
  //NS_ASSERTION(aMetrics.mCarriedOutBottomMargin.IsZero(),
  //             "someone else set the margin");
  if (!aState.GetFlag(BRS_ISBOTTOMMARGINROOT))
    aMetrics.mCarriedOutBottomMargin = aState.mPrevBottomMargin;
  else
    aMetrics.mCarriedOutBottomMargin.Zero();

#ifdef DEBUG_blocks
  if (CRAZY_WIDTH(aMetrics.width) || CRAZY_HEIGHT(aMetrics.height)) {
    ListTag(stdout);
    printf(": WARNING: desired:%d,%d\n", aMetrics.width, aMetrics.height);
  }
  if (aState.GetFlag(BRS_COMPUTEMAXELEMENTSIZE) &&
      ((maxWidth > aMetrics.width) || (maxHeight > aMetrics.height))) {
    ListTag(stdout);
    printf(": WARNING: max-element-size:%d,%d desired:%d,%d maxSize:%d,%d\n",
           maxWidth, maxHeight, aMetrics.width, aMetrics.height,
           aState.mReflowState.availableWidth,
           aState.mReflowState.availableHeight);
  }
#endif
#ifdef NOISY_MAX_ELEMENT_SIZE
  if (aState.GetFlag(BRS_COMPUTEMAXELEMENTSIZE)) {
    IndentBy(stdout, GetDepth());
    if (NS_UNCONSTRAINEDSIZE == aState.mReflowState.availableWidth) {
      printf("PASS1 ");
    }
    ListTag(stdout);
    printf(": max-element-size:%d,%d desired:%d,%d maxSize:%d,%d\n",
           maxWidth, maxHeight, aMetrics.width, aMetrics.height,
           aState.mReflowState.availableWidth,
           aState.mReflowState.availableHeight);
  }
#endif

  // If we're requested to update our maximum width, then compute it
  if (aState.GetFlag(BRS_COMPUTEMAXWIDTH)) {
    // We need to add in for the right border/padding
    aMetrics.mMaximumWidth = aState.mMaximumWidth + borderPadding.right;
#ifdef NOISY_MAXIMUM_WIDTH
    printf("nsBlockFrame::ComputeFinalSize block %p setting aMetrics.mMaximumWidth to %d\n", this, aMetrics.mMaximumWidth);
#endif
  }

  // Compute the combined area of our children
// XXX_perf: This can be done incrementally
  nscoord xa = 0, ya = 0, xb = aMetrics.width, yb = aMetrics.height;
  if (NS_STYLE_OVERFLOW_HIDDEN != aReflowState.mStyleDisplay->mOverflow) {
    for (line_iterator line = begin_lines(), line_end = end_lines();
         line != line_end;
         ++line)
    {
      // Compute min and max x/y values for the reflowed frame's
      // combined areas
      nsRect lineCombinedArea;
      line->GetCombinedArea(&lineCombinedArea);
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

    // Factor the bullet in; normally the bullet will be factored into
    // the line-box's combined area. However, if the line is a block
    // line then it won't; if there are no lines, it won't. So just
    // factor it in anyway (it can't hurt if it was already done).
    // XXXldb Can we just fix GetCombinedArea instead?
    if (mBullet) {
      nsRect r;
      mBullet->GetRect(r);
      if (r.x < xa) xa = r.x;
      if (r.y < ya) ya = r.y;
      nscoord xmost = r.XMost();
      if (xmost > xb) xb = xmost;
      nscoord ymost = r.YMost();
      if (ymost > yb) yb = ymost;
    }
  }
#ifdef NOISY_COMBINED_AREA
  ListTag(stdout);
  printf(": ca=%d,%d,%d,%d\n", xa, ya, xb-xa, yb-ya);
#endif

  // If the combined area of our children exceeds our bounding box
  // then set the NS_FRAME_OUTSIDE_CHILDREN flag, otherwise clear it.
  aMetrics.mOverflowArea.x = xa;
  aMetrics.mOverflowArea.y = ya;
  aMetrics.mOverflowArea.width = xb - xa;
  aMetrics.mOverflowArea.height = yb - ya;
  if ((aMetrics.mOverflowArea.x < 0) ||
      (aMetrics.mOverflowArea.y < 0) ||
      (aMetrics.mOverflowArea.XMost() > aMetrics.width) ||
      (aMetrics.mOverflowArea.YMost() > aMetrics.height)) {
    mState |= NS_FRAME_OUTSIDE_CHILDREN;
  }
  else {
    mState &= ~NS_FRAME_OUTSIDE_CHILDREN;
  }

  if (NS_BLOCK_WRAP_SIZE & mState) {
    // When the area frame is supposed to wrap around all in-flow
    // children, make sure it is big enough to include those that stick
    // outside the box.
    if (NS_FRAME_OUTSIDE_CHILDREN & mState) {
      nscoord xMost = aMetrics.mOverflowArea.XMost();
      if (xMost > aMetrics.width) {
#ifdef NOISY_FINAL_SIZE
        ListTag(stdout);
        printf(": changing desired width from %d to %d\n", aMetrics.width, xMost);
#endif
        aMetrics.width = xMost;
      }
      nscoord yMost = aMetrics.mOverflowArea.YMost();
      if (yMost > aMetrics.height) {
#ifdef NOISY_FINAL_SIZE
        ListTag(stdout);
        printf(": changing desired height from %d to %d\n", aMetrics.height, yMost);
#endif
        aMetrics.height = yMost;
        // adjust descent to absorb any excess difference
        aMetrics.descent = aMetrics.height - aMetrics.ascent;
      }
    }
  }
}

nsresult
nsBlockFrame::PrepareInitialReflow(nsBlockReflowState& aState)
{
  PrepareResizeReflow(aState);
  return NS_OK;
}

nsresult
nsBlockFrame::PrepareChildIncrementalReflow(nsBlockReflowState& aState)
{
  // Determine the line being impacted
  PRBool isFloater;
  line_iterator line;
  FindLineFor(aState.mNextRCFrame, &isFloater, &line);
  if (line == end_lines()) {
    // This assertion actually fires on lots of pages
    // (e.g., bugzilla, bugzilla query page), so limit it
    // to a few people until we fix the problem causing it.
    //
    // I think waterson explained once why it was happening -- I think
    // it has something to do with the interaction of the unconstrained
    // reflow in multi-pass reflow with the reflow command's chain, but
    // I don't remember the details.
#if defined(DEBUG_dbaron) || defined(DEBUG_waterson)
    NS_NOTREACHED("We don't have a line for the target of the reflow.  "
                  "Being inefficient");
#endif
    // This can't happen, but just in case it does...
    return PrepareResizeReflow(aState);
  }

  // XXX: temporary: If the child frame is a floater then punt
  // XXX_perf XXXldb How big a perf problem is this?
  if (isFloater) {
    return PrepareResizeReflow(aState);
  }

  if (line->IsInline()) {
    aState.SetFlag(BRS_ISINLINEINCRREFLOW, PR_TRUE);
  }

  // Just mark this line dirty.  We never need to mark the
  // previous line dirty since either:
  //  * the line is a block, and there would never be a chance to pull
  //    something up
  //  * It's an incremental reflow to something within an inline, which
  //    we know must be very limited.
  line->MarkDirty();

  return NS_OK;
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

  // Mark previous line dirty if its an inline line so that it can
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
nsBlockFrame::UpdateBulletPosition(nsBlockReflowState& aState)
{
  if (nsnull == mBullet) {
    // Don't bother if there is no bullet
    return NS_OK;
  }
  const nsStyleList* styleList;
  GetStyleData(eStyleStruct_List, (const nsStyleStruct*&) styleList);
  if (NS_STYLE_LIST_STYLE_POSITION_INSIDE == styleList->mListStylePosition) {
    if (mBullet && HaveOutsideBullet()) {
      // We now have an inside bullet, but used to have an outside
      // bullet.  Adjust the frame line list
      if (! mLines.empty()) {
        // if we have a line already, then move the bullet to the front of the
        // first line
        nsIFrame* child = nsnull;
        nsLineBox* firstLine = mLines.front();
        
#ifdef DEBUG
        // bullet should not have any siblings if it was an outside bullet
        nsIFrame* next = nsnull;
        mBullet->GetNextSibling(&next);
        NS_ASSERTION(!next, "outside bullet should not have siblings");
#endif
        // move bullet to front and chain the previous frames, and update the line count
        child = firstLine->mFirstChild;
        firstLine->mFirstChild = mBullet;
        mBullet->SetNextSibling(child);
        PRInt32 count = firstLine->GetChildCount();
        firstLine->SetChildCount(count+1);
        // dirty it here in case the caller does not
        firstLine->MarkDirty();
      } else {
        // no prior lines, just create a new line for the bullet
        nsLineBox* line = aState.NewLineBox(mBullet, 1, PR_FALSE);
        if (!line) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
        mLines.push_back(line);
      }
    }
    mState &= ~NS_BLOCK_FRAME_HAS_OUTSIDE_BULLET;
  }
  else {
    if (!HaveOutsideBullet()) {
      // We now have an outside bullet, but used to have an inside
      // bullet. Take the bullet frame out of the first lines frame
      // list.
      if ((! mLines.empty()) && (mBullet == mLines.front()->mFirstChild)) {
        nsIFrame* next;
        mBullet->GetNextSibling(&next);
        mBullet->SetNextSibling(nsnull);
        PRInt32 count = mLines.front()->GetChildCount() - 1;
        NS_ASSERTION(count >= 0, "empty line w/o bullet");
        mLines.front()->SetChildCount(count);
        if (0 == count) {
          nsLineBox* oldFront = mLines.front();
          mLines.pop_front();
          aState.FreeLineBox(oldFront);
          if (! mLines.empty()) {
            mLines.front()->MarkDirty();
          }
        }
        else {
          mLines.front()->mFirstChild = next;
          mLines.front()->MarkDirty();
        }
      }
    }
    mState |= NS_BLOCK_FRAME_HAS_OUTSIDE_BULLET;
  }
#ifdef DEBUG
  VerifyLines(PR_TRUE);
#endif
  return NS_OK;
}

nsresult
nsBlockFrame::PrepareStyleChangedReflow(nsBlockReflowState& aState)
{
  nsresult rv = UpdateBulletPosition(aState);

  // Mark everything dirty
  for (line_iterator line = begin_lines(), line_end = end_lines();
       line != line_end;
       ++line)
  {
    line->MarkDirty();
  }
  return rv;
}

nsresult
nsBlockFrame::PrepareResizeReflow(nsBlockReflowState& aState)
{
  // See if we can try and avoid marking all the lines as dirty
  PRBool  tryAndSkipLines = PR_FALSE;

  // we need to calculate if any part of then block itself 
  // is impacted by a floater (bug 19579)
  aState.GetAvailableSpace();

  // See if this is this a constrained resize reflow that is not impacted by floaters
  if ((! aState.IsImpactedByFloater()) &&
      (aState.mReflowState.reason == eReflowReason_Resize) &&
      (NS_UNCONSTRAINEDSIZE != aState.mReflowState.availableWidth)) {

    // If the text is left-aligned, then we try and avoid reflowing the lines
    const nsStyleText* styleText = (const nsStyleText*)
      mStyleContext->GetStyleData(eStyleStruct_Text);

    if ((NS_STYLE_TEXT_ALIGN_LEFT == styleText->mTextAlign) ||
        ((NS_STYLE_TEXT_ALIGN_DEFAULT == styleText->mTextAlign) &&
         (NS_STYLE_DIRECTION_LTR == aState.mReflowState.mStyleVisibility->mDirection))) {
      tryAndSkipLines = PR_TRUE;
    }
  }

#ifdef DEBUG
  if (gDisableResizeOpt) {
    tryAndSkipLines = PR_FALSE;
  }
  if (gNoisyReflow) {
    if (!tryAndSkipLines) {
      const nsStyleText* mStyleText = (const nsStyleText*)
        mStyleContext->GetStyleData(eStyleStruct_Text);
      IndentBy(stdout, gNoiseIndent);
      ListTag(stdout);
      printf(": marking all lines dirty: reason=%d availWidth=%d textAlign=%d\n",
             aState.mReflowState.reason,
             aState.mReflowState.availableWidth,
             mStyleText->mTextAlign);
    }
  }
#endif

  if (tryAndSkipLines) {
    nscoord newAvailWidth = aState.mReflowState.mComputedBorderPadding.left;
     
    if (NS_SHRINKWRAPWIDTH == aState.mReflowState.mComputedWidth) {
      if (NS_UNCONSTRAINEDSIZE != aState.mReflowState.mComputedMaxWidth) {
        newAvailWidth += aState.mReflowState.mComputedMaxWidth;
      }
      else {
        newAvailWidth += aState.mReflowState.availableWidth;
      }
    } else {
      if (NS_UNCONSTRAINEDSIZE != aState.mReflowState.mComputedWidth) {
        newAvailWidth += aState.mReflowState.mComputedWidth;
      }
      else {
        newAvailWidth += aState.mReflowState.availableWidth;
      }
    }
    NS_ASSERTION(NS_UNCONSTRAINEDSIZE != newAvailWidth, "bad math, newAvailWidth is infinite");

#ifdef DEBUG
    if (gNoisyReflow) {
      IndentBy(stdout, gNoiseIndent);
      ListTag(stdout);
      printf(": trying to avoid marking all lines dirty\n");
    }
#endif

    PRBool wrapping = !aState.GetFlag(BRS_NOWRAP);
    for (line_iterator line = begin_lines(), line_end = end_lines();
         line != line_end;
         ++line)
    {
      // We let child blocks make their own decisions the same
      // way we are here.
      //
      // For inline lines with no-wrap, the only way things
      // could change is if there is a percentage-sized child.
      if (line->IsBlock() ||
          line->HasPercentageChild() || 
          (wrapping &&
           ((line != mLines.back() && !line->HasBreak()) ||
           line->ResizeReflowOptimizationDisabled() ||
           line->HasFloaters() ||
           line->IsImpactedByFloater() ||
           (line->mBounds.XMost() > newAvailWidth)))) {
        line->MarkDirty();
      }

#ifdef REALLY_NOISY_REFLOW
      if (!line->IsBlock()) {
        printf("PrepareResizeReflow thinks line %p is %simpacted by floaters\n", 
        line, line->IsImpactedByFloater() ? "" : "not ");
      }
#endif
#ifdef DEBUG
      if (gNoisyReflow && !line->IsDirty() && wrapping) {
        IndentBy(stdout, gNoiseIndent + 1);
        printf("skipped: line=%p next=%p %s %s %s%s%s breakType=%d xmost=%d\n",
           NS_STATIC_CAST(void*, line.get()),
           NS_STATIC_CAST(void*, line.next().get()),
           line->IsBlock() ? "block" : "inline",
           "wrapping",
           line->HasBreak() ? "has-break " : "",
           line->HasFloaters() ? "has-floaters " : "",
           line->IsImpactedByFloater() ? "impacted " : "",
           line->GetBreakType(),
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

PRBool
nsBlockFrame::FindLineFor(nsIFrame* aFrame,
                          PRBool* aIsFloaterResult,
                          line_iterator* aFoundLine)
{
  // This assertion actually fires on lots of pages (e.g., bugzilla,
  // bugzilla query page), so limit it to a few people until we fix the
  // problem causing it.  It's related to the similarly |#ifdef|ed
  // assertion in |PrepareChildIncrementalReflow|.
#if defined(DEBUG_dbaron) || defined(DEBUG_waterson)
  NS_PRECONDITION(aFrame, "why pass a null frame?");
#endif

  PRBool isFloater = PR_FALSE;
  line_iterator line = begin_lines(),
                line_end = end_lines();
  for ( ; line != line_end; ++line) {
    if (line->Contains(aFrame)) {
      break;
    }
    // XXXldb what does this do?
    if (line->HasFloaters()) {
      nsFloaterCache* fc = line->GetFirstFloater();
      while (fc) {
        if (aFrame == fc->mPlaceholder->GetOutOfFlowFrame()) {
          isFloater = PR_TRUE;
          goto done;
        }
        fc = fc->Next();
      }
    }
  }

 done:
  *aIsFloaterResult = isFloater;
  *aFoundLine = line;
  return (line != line_end);
}

// SEC: added GetCurrentLine() for bug 45152
// we need a way for line layout to know what line is being reflowed,
// but we don't want to expose the innards of nsBlockReflowState.
nsresult 
nsBlockFrame::GetCurrentLine(nsBlockReflowState *aState, nsLineBox ** aOutCurrentLine)
{
  if (!aState || !aOutCurrentLine) return NS_ERROR_FAILURE;
  *aOutCurrentLine = aState->mCurrentLine;
  return NS_OK;  
}

/**
 * Remember regions of reflow damage from floaters that changed size (so
 * far, only vertically, which is a bug) in the space manager so that we
 * can mark any impacted lines dirty in |PropagateFloaterDamage|.
 */
void
nsBlockFrame::RememberFloaterDamage(nsBlockReflowState& aState,
                                    nsLineBox* aLine,
                                    const nsRect& aOldCombinedArea)
{
  nsRect lineCombinedArea;
  aLine->GetCombinedArea(&lineCombinedArea);
  if (lineCombinedArea != aLine->mBounds &&
      lineCombinedArea != aOldCombinedArea) {
    // The line's combined-area changed. Therefore we need to damage
    // the lines below that were previously (or are now) impacted by
    // the change. It's possible that a floater shrunk or grew so
    // use the larger of the impacted area.

    // XXXldb If just the width of the floater changed, then this code
    // won't be triggered and the code below (in |PropagateFloaterDamage|)
    // won't kick in for "non-Block lines".  See
    // "XXX: Maybe the floater itself changed size?" below.
    //
    // This is a major flaw in this code.
    //
    nscoord newYMost = lineCombinedArea.YMost();
    nscoord oldYMost = aOldCombinedArea.YMost();
    nscoord impactYB = newYMost < oldYMost ? oldYMost : newYMost;
    nscoord impactYA = lineCombinedArea.y;
    nsISpaceManager *spaceManager = aState.mReflowState.mSpaceManager;
    spaceManager->IncludeInDamage(impactYA, impactYB);
  }
}

/**
 * Propagate reflow "damage" from from earlier lines to the current
 * line.  The reflow damage comes from the following sources:
 *  1. The regions of floater damage remembered in
 *     |RememberFloaterDamage|.
 *  2. The combination of nonzero |aDeltaY| and any impact by a floater,
 *     either the previous reflow or now.
 *
 * When entering this function, |aLine| is still at its old position and
 * |aDeltaY| indicates how much it will later be slid (assuming it
 * doesn't get marked dirty and reflowed entirely).
 */
void
nsBlockFrame::PropagateFloaterDamage(nsBlockReflowState& aState,
                                     nsLineBox* aLine,
                                     nscoord aDeltaY)
{
  NS_PRECONDITION(!aLine->IsDirty(), "should never be called on dirty lines");

  // Check the damage region recorded in the float damage.
  nsISpaceManager *spaceManager = aState.mReflowState.mSpaceManager;
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
    // 1. the line was impacted by a floater and now isn't
    // 2. the line wasn't impacted by a floater and now is
    // 3. the line is impacted by a floater both before and after and 
    //    the floater has changed position relative to the line (or it's
    //    a different floater).  (XXXPerf we don't currently
    //    check whether the floater changed size.  We currently just
    //    mark blocks dirty and ignore any possibility of damage to
    //    inlines by it being a different floater with a different
    //    size.)
    //
    //    XXXPerf: An optimization: if the line was and is completely
    //    impacted by a floater and the floater hasn't changed size,
    //    then we don't need to mark the line dirty.
    aState.GetAvailableSpace(aLine->mBounds.y + aDeltaY);
    PRBool wasImpactedByFloater = aLine->IsImpactedByFloater();
    PRBool isImpactedByFloater = aState.IsImpactedByFloater();
#ifdef REALLY_NOISY_REFLOW
    printf("nsBlockFrame::PropagateFloaterDamage %p was = %d, is=%d\n", 
       this, wasImpactedByFloater, isImpactedByFloater);
#endif
    // Mark the line dirty if:
    //  1. It used to be impacted by a floater and now isn't, or vice
    //     versa.
    //  2. It is impacted by a floater and it is a block, which means
    //     that more or less of the line could be impacted than was in
    //     the past.  (XXXPerf This could be optimized further, since
    //     we're marking the whole line dirty.)
    if ((wasImpactedByFloater != isImpactedByFloater) ||
        (isImpactedByFloater && aLine->IsBlock())) {
      aLine->MarkDirty();
    }
  }
}

// NOTE:  The first parameter *must* be passed by value.
static PRBool
WrappedLinesAreDirty(nsLineList::iterator aLine,
                     const nsLineList::iterator aLineEnd)
{
  if (aLine->IsInline()) {
    while (aLine->IsLineWrapped()) {
      ++aLine;
      if (aLine == aLineEnd) {
        break;
      }

      NS_ASSERTION(!aLine->IsBlock(), "didn't expect a block line");
      if (aLine->IsDirty()) {
        // we found a continuing line that is dirty
        return PR_TRUE;
      }
    }
  }

  return PR_FALSE;
}

PRBool nsBlockFrame::IsIncrementalDamageConstrained(const nsBlockReflowState& aState) const
{
  // see if the reflow will go through a text control.  if so, we can optimize 
  // because we know the text control won't change size.
  if (aState.mReflowState.reflowCommand)
  {
    nsIFrame *target;
    aState.mReflowState.reflowCommand->GetTarget(target);
    while (target)
    { // starting with the target's parent, scan for a text control
      nsIFrame *parent;
      target->GetParent(&parent);
      if ((nsIFrame*)this==parent || !parent)  // the null check is paranoia, it should never happen
        break;  // we found ourself, so we know there's no text control between us and target
      nsCOMPtr<nsIAtom> frameType;
      parent->GetFrameType(getter_AddRefs(frameType));
      if (frameType)
      {
        if (nsLayoutAtoms::textInputFrame == frameType.get())
          return PR_TRUE; // damage is constrained to the text control innards
      }
      target = parent;  // advance the loop up the frame tree
    }
  }
  return PR_FALSE;  // default case, damage is not constrained (or unknown)
}

static void PlaceFrameView(nsIPresContext* aPresContext, nsIFrame* aFrame);

/**
 * Reflow the dirty lines
 */
nsresult
nsBlockFrame::ReflowDirtyLines(nsBlockReflowState& aState)
{
  nsresult rv = NS_OK;
  PRBool keepGoing = PR_TRUE;
  PRBool repositionViews = PR_FALSE; // should we really need this?

#ifdef DEBUG
  if (gNoisyReflow) {
    if (aState.mReflowState.reason == eReflowReason_Incremental) {
      nsIReflowCommand::ReflowType type;
      aState.mReflowState.reflowCommand->GetType(type);
      IndentBy(stdout, gNoiseIndent);
      ListTag(stdout);
      printf(": incrementally reflowing dirty lines: type=%s(%d) isInline=%s",
             kReflowCommandType[type], type,
             aState.GetFlag(BRS_ISINLINEINCRREFLOW) ? "true" : "false");
    }
    else {
      IndentBy(stdout, gNoiseIndent);
      ListTag(stdout);
      printf(": reflowing dirty lines");
    }
    printf(" computedWidth=%d\n", aState.mReflowState.mComputedWidth);
    gNoiseIndent++;
  }
#endif

  // Check whether this is an incremental reflow
  PRBool  incrementalReflow = aState.mReflowState.reason ==
                              eReflowReason_Incremental ||
                              aState.mReflowState.reason ==
                              eReflowReason_Dirty;
  
    // the amount by which we will slide the current line if it is not
    // dirty
  nscoord deltaY = 0;

    // whether we did NOT reflow the previous line and thus we need to
    // recompute the carried out margin before the line if we want to
    // reflow it or if its previous margin is dirty
  PRBool needToRecoverMargin = PR_FALSE;
  
  // Reflow the lines that are already ours
  line_iterator line = begin_lines(), line_end = end_lines();
  for ( ; line != line_end; ++line, aState.AdvanceToNextLine()) {
#ifdef DEBUG
    if (gNoisyReflow) {
      nsRect lca;
      line->GetCombinedArea(&lca);
      IndentBy(stdout, gNoiseIndent);
      printf("line=%p mY=%d dirty=%s oldBounds={%d,%d,%d,%d} oldCombinedArea={%d,%d,%d,%d} deltaY=%d mPrevBottomMargin=%d\n",
             NS_STATIC_CAST(void*, line.get()), aState.mY,
             line->IsDirty() ? "yes" : "no",
             line->mBounds.x, line->mBounds.y,
             line->mBounds.width, line->mBounds.height,
             lca.x, lca.y, lca.width, lca.height,
             deltaY, aState.mPrevBottomMargin.get());
      gNoiseIndent++;
    }
#endif

    // If we're supposed to update our maximum width, then we'll also need to
    // reflow this line if it's line wrapped and any of the continuing lines
    // are dirty
    if (!line->IsDirty() &&
        (aState.GetFlag(BRS_COMPUTEMAXWIDTH) &&
         ::WrappedLinesAreDirty(line, line_end))) {
      line->MarkDirty();
    }

    // Make sure |aState.mPrevBottomMargin| is at the correct position
    // before calling PropagateFloaterDamage.
    if (needToRecoverMargin) {
      needToRecoverMargin = PR_FALSE;
      // We need to reconstruct the bottom margin only if we didn't
      // reflow the previous line and we do need to reflow (or repair
      // the top position of) the next line.
      if (line->IsDirty() || line->IsPreviousMarginDirty())
        aState.ReconstructMarginAbove(line);
    }

    if (line->IsPreviousMarginDirty() && !line->IsDirty()) {
      // If the previous margin is dirty and we're not going to reflow
      // the line we need to pull out the correct top margin and set
      // |deltaY| correctly.
      // If there's float damage we might end up doing this work twice,
      // but whatever...
      if (line->IsBlock()) {
        // XXXPerf We could actually make this faster by stealing code
        // from the top of nsBlockFrame::ReflowBlockFrame, but it's an
        // edge case that will generally happen at most once in a given
        // reflow (insertion of a new line before a block)
        line->MarkDirty();
      } else {
        deltaY = aState.mY + aState.mPrevBottomMargin.get() - line->mBounds.y;
      }
    }
    line->ClearPreviousMarginDirty();

    // See if there's any reflow damage that requires that we mark the
    // line dirty.
    if (!line->IsDirty()) {
      PropagateFloaterDamage(aState, line, deltaY);
    }

    // Now repair the line and update |aState.mY| by calling
    // |ReflowLine| or |SlideLine|.
    if (line->IsDirty()) {
      // Compute the dirty lines "before" YMost, after factoring in
      // the running deltaY value - the running value is implicit in
      // aState.mY.
      nscoord oldY = line->mBounds.y;
      nscoord oldYMost = line->mBounds.YMost();
      nsRect oldCombinedArea;
      line->GetCombinedArea(&oldCombinedArea);

      // Reflow the dirty line. If it's an incremental reflow, then force
      // it to invalidate the dirty area if necessary
      PRBool forceInvalidate = PR_FALSE;
      if (incrementalReflow) {
        forceInvalidate = !IsIncrementalDamageConstrained(aState);
      }
      rv = ReflowLine(aState, line, &keepGoing, forceInvalidate);
      if (NS_FAILED(rv)) {
        return rv;
      }
      if (!keepGoing) {
        if (0 == line->GetChildCount()) {
          DeleteLine(aState, line, line_end);
        }
        break;
      }
      if (oldY == 0 && deltaY != line->mBounds.y) {
        // This means the current line was just reflowed for the first
        // time.  Thus we must mark the the previous margin of the next
        // line dirty.
        // XXXldb Move this into where we insert the line!  (or will
        // that mess up deltaY manipulation?)
        if (line.next() != end_lines()) {
          line.next()->MarkPreviousMarginDirty();
          // since it's marked dirty, nobody will care about |deltaY|
        }
      } else {
        deltaY = line->mBounds.YMost() - oldYMost;
      }

      // Remember any things that could potentially be changes in the
      // positions of floaters in this line so that we later damage the
      // lines adjacent to those changes.
      RememberFloaterDamage(aState, line, oldCombinedArea);
    } else {
      if (deltaY != 0)
        SlideLine(aState, line, deltaY);
      else
        repositionViews = PR_TRUE;

      // XXX EVIL O(N^2) EVIL
      aState.RecoverStateFrom(line, deltaY);

      // Keep mY up to date in case we're propagating reflow damage.
      aState.mY = line->mBounds.YMost();
      needToRecoverMargin = PR_TRUE;
    }

#ifdef DEBUG
    if (gNoisyReflow) {
      gNoiseIndent--;
      nsRect lca;
      line->GetCombinedArea(&lca);
      IndentBy(stdout, gNoiseIndent);
      printf("line=%p mY=%d newBounds={%d,%d,%d,%d} newCombinedArea={%d,%d,%d,%d} deltaY=%d mPrevBottomMargin=%d\n",
             NS_STATIC_CAST(void*, line.get()), aState.mY,
             line->mBounds.x, line->mBounds.y,
             line->mBounds.width, line->mBounds.height,
             lca.x, lca.y, lca.width, lca.height,
             deltaY, aState.mPrevBottomMargin.get());
    }
#endif
  }

  if (needToRecoverMargin) {
    // Is this expensive?
    aState.ReconstructMarginAbove(line);
  }

  // Should we really have to do this?
  if (repositionViews)
    ::PlaceFrameView(aState.mPresContext, this);

  // Pull data from a next-in-flow if there's still room for more
  // content here.
  while (keepGoing && (nsnull != aState.mNextInFlow)) {
    // Grab first line from our next-in-flow
    nsBlockFrame* nextInFlow = aState.mNextInFlow;
    line_iterator nifLine = nextInFlow->begin_lines();
    if (nifLine == nextInFlow->end_lines()) {
      aState.mNextInFlow = (nsBlockFrame*) aState.mNextInFlow->mNextInFlow;
      continue;
    }
    // XXX See if the line is not dirty; if it's not maybe we can
    // avoid the pullup if it can't fit?
    nsLineBox *toMove = nifLine;
    nextInFlow->mLines.erase(nifLine);
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
      frame->SetParent(this);
      // When pushing and pulling frames we need to check for whether any
      // views need to be reparented
      nsHTMLContainerFrame::ReparentFrameView(aState.mPresContext, frame, mNextInFlow, this);
      lastFrame = frame;
      frame->GetNextSibling(&frame);
    }
    lastFrame->SetNextSibling(nsnull);

    // Add line to our line list
    if (aState.mPrevChild)
      aState.mPrevChild->SetNextSibling(toMove->mFirstChild);
    line = mLines.before_insert(end_lines(), toMove);

    // If line contains floaters, remove them from aState.mNextInFlow's
    // floater list. They will be pushed onto this blockframe's floater
    // list, via BuildFloaterList(), when we are done reflowing dirty lines.
    //
    // XXX: If the call to BuildFloaterList() is removed from
    //      nsBlockFrame::Reflow(), we'll probably need to manually
    //      append the floaters to |this|'s floater list.

    if (line->HasFloaters()) {
      nsFloaterCache* fc = line->GetFirstFloater();
      while (fc) {
        if (fc->mPlaceholder) {
          nsIFrame* floater = fc->mPlaceholder->GetOutOfFlowFrame();
          if (floater)
            aState.mNextInFlow->mFloaters.RemoveFrame(floater);
        }
        fc = fc->Next();
      }
    }

    // Now reflow it and any lines that it makes during it's reflow
    // (we have to loop here because reflowing the line may case a new
    // line to be created; see SplitLine's callers for examples of
    // when this happens).
    while (line != end_lines()) {
      rv = ReflowLine(aState, line, &keepGoing,
                      incrementalReflow /* force invalidate */);
      if (NS_FAILED(rv)) {
        return rv;
      }
      if (!keepGoing) {
        if (0 == line->GetChildCount()) {
          DeleteLine(aState, line, line_end);
        }
        break;
      }

      // If this is an inline frame then its time to stop
      ++line;
      aState.AdvanceToNextLine();
    }
  }

  // Handle an odd-ball case: a list-item with no lines
  if (mBullet && HaveOutsideBullet() && mLines.empty()) {
    nsHTMLReflowMetrics metrics(nsnull);
    ReflowBullet(aState, metrics);

    // There are no lines so we have to fake up some y motion so that
    // we end up with *some* height.
    aState.mY += metrics.height;
  }

#ifdef DEBUG
  if (gNoisyReflow) {
    gNoiseIndent--;
    IndentBy(stdout, gNoiseIndent);
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
 * Reflow a line. The line will either contain a single block frame
 * or contain 1 or more inline frames. aKeepReflowGoing indicates
 * whether or not the caller should continue to reflow more lines.
 */
nsresult
nsBlockFrame::ReflowLine(nsBlockReflowState& aState,
                         line_iterator aLine,
                         PRBool* aKeepReflowGoing,
                         PRBool aDamageDirtyArea)
{
  nsresult rv = NS_OK;

  NS_ABORT_IF_FALSE(aLine->GetChildCount(), "reflowing empty line");

  // Setup the line-layout for the new line
  aState.mCurrentLine = aLine;
  aLine->ClearDirty();

  // Now that we know what kind of line we have, reflow it
  nsRect oldCombinedArea;
  aLine->GetCombinedArea(&oldCombinedArea);

  if (aLine->IsBlock()) {
    rv = ReflowBlockFrame(aState, aLine, aKeepReflowGoing);
    
    // We expect blocks to damage any area inside their bounds that is
    // dirty; however, if the frame changes size or position then we
    // need to do some repainting
    if (aDamageDirtyArea) {
      nsRect lineCombinedArea;
      aLine->GetCombinedArea(&lineCombinedArea);
      if ((oldCombinedArea.x != lineCombinedArea.x) ||
          (oldCombinedArea.y != lineCombinedArea.y)) {
        // The block has moved, and so to be safe we need to repaint
        // XXX We need to improve on this...
        nsRect  dirtyRect;
        dirtyRect.UnionRect(oldCombinedArea, lineCombinedArea);
#ifdef NOISY_BLOCK_INVALIDATE
        printf("%p invalidate 6 (%d, %d, %d, %d)\n",
               this, dirtyRect.x, dirtyRect.y, dirtyRect.width, dirtyRect.height);
#endif
        Invalidate(aState.mPresContext, dirtyRect);

      } else {
        if (oldCombinedArea.width != lineCombinedArea.width) {
          nsRect  dirtyRect;

          // Just damage the vertical strip that was either added or went
          // away
          dirtyRect.x = PR_MIN(oldCombinedArea.XMost(),
                               lineCombinedArea.XMost());
          dirtyRect.y = lineCombinedArea.y;
          dirtyRect.width = PR_MAX(oldCombinedArea.XMost(),
                                   lineCombinedArea.XMost()) -
                            dirtyRect.x;
          dirtyRect.height = PR_MAX(oldCombinedArea.height,
                                    lineCombinedArea.height);
#ifdef NOISY_BLOCK_INVALIDATE
          printf("%p invalidate 7 (%d, %d, %d, %d)\n",
                 this, dirtyRect.x, dirtyRect.y, dirtyRect.width, dirtyRect.height);
#endif
          Invalidate(aState.mPresContext, dirtyRect);
        }
        if (oldCombinedArea.height != lineCombinedArea.height) {
          nsRect  dirtyRect;
          
          // Just damage the horizontal strip that was either added or went
          // away
          dirtyRect.x = lineCombinedArea.x;
          dirtyRect.y = PR_MIN(oldCombinedArea.YMost(),
                               lineCombinedArea.YMost());
          dirtyRect.width = PR_MAX(oldCombinedArea.width,
                                   lineCombinedArea.width);
          dirtyRect.height = PR_MAX(oldCombinedArea.YMost(),
                                    lineCombinedArea.YMost()) -
                             dirtyRect.y;
#ifdef NOISY_BLOCK_INVALIDATE
          printf("%p invalidate 8 (%d, %d, %d, %d)\n",
                 this, dirtyRect.x, dirtyRect.y, dirtyRect.width, dirtyRect.height);
#endif
          Invalidate(aState.mPresContext, dirtyRect);
        }
      }
    }
  }
  else {
    aLine->SetLineWrapped(PR_FALSE);

    // If we're supposed to update the maximum width, then we'll need to reflow
    // the line with an unconstrained width (which will give us the new maximum
    // width), then we'll reflow it again with the constrained width.
    // We only do this if this is a beginning line, i.e., don't do this for
    // lines associated with content that line wrapped (see ReflowDirtyLines()
    // for details).
    // XXX This approach doesn't work when floaters are involved in which case
    // we'll either need to recover the floater state that applies to the
    // unconstrained reflow or keep it around in a separate space manager...
    PRBool isBeginningLine = aState.mCurrentLine == begin_lines() ||
                             !aState.mCurrentLine.prev()->IsLineWrapped();
    if (aState.GetFlag(BRS_COMPUTEMAXWIDTH) && isBeginningLine) {
      nscoord oldY = aState.mY;
      nsCollapsingMargin oldPrevBottomMargin(aState.mPrevBottomMargin);
      PRBool  oldUnconstrainedWidth = aState.GetFlag(BRS_UNCONSTRAINEDWIDTH);

      // First reflow the line with an unconstrained width. When doing this
      // we need to set the block reflow state's "mUnconstrainedWidth" variable
      // to PR_TRUE so if we encounter a placeholder and then reflow its
      // associated floater we don't end up resetting the line's right edge and
      // have it think the width is unconstrained...
      aState.SetFlag(BRS_UNCONSTRAINEDWIDTH, PR_TRUE);
      ReflowInlineFrames(aState, aLine, aKeepReflowGoing, aDamageDirtyArea, PR_TRUE);
      aState.mY = oldY;
      aState.mPrevBottomMargin = oldPrevBottomMargin;
      aState.SetFlag(BRS_UNCONSTRAINEDWIDTH, oldUnconstrainedWidth);

      // Update the line's maximum width
      aLine->mMaximumWidth = aLine->mBounds.XMost();
#ifdef NOISY_MAXIMUM_WIDTH
      printf("nsBlockFrame::ReflowLine block %p line %p setting aLine.mMaximumWidth to %d\n", 
             this, aLine, aLine->mMaximumWidth);
#endif
      aState.UpdateMaximumWidth(aLine->mMaximumWidth);

      // Remove any floaters associated with the line from the space
      // manager
      aLine->RemoveFloatersFromSpaceManager(aState.mSpaceManager);

      // Now reflow the line again this time without having it compute
      // the maximum width or max-element-size.
      // Note: we need to reset both member variables, because the inline
      // code examines mComputeMaxElementSize and if there is a placeholder
      // on this line the code to reflow the floater looks at both...
      nscoord oldComputeMaxElementSize = aState.GetFlag(BRS_COMPUTEMAXELEMENTSIZE);
      nscoord oldComputeMaximumWidth = aState.GetFlag(BRS_COMPUTEMAXWIDTH);

      aState.SetFlag(BRS_COMPUTEMAXELEMENTSIZE, PR_FALSE);
      aState.SetFlag(BRS_COMPUTEMAXWIDTH, PR_FALSE);
      rv = ReflowInlineFrames(aState, aLine, aKeepReflowGoing, aDamageDirtyArea);
      aState.SetFlag(BRS_COMPUTEMAXELEMENTSIZE, oldComputeMaxElementSize);
      aState.SetFlag(BRS_COMPUTEMAXWIDTH, oldComputeMaximumWidth);

    } else {
      rv = ReflowInlineFrames(aState, aLine, aKeepReflowGoing, aDamageDirtyArea);
      if (NS_SUCCEEDED(rv))
      {
        if (aState.GetFlag(BRS_COMPUTEMAXWIDTH))
        {
#ifdef NOISY_MAXIMUM_WIDTH
          printf("nsBlockFrame::ReflowLine block %p line %p setting aLine.mMaximumWidth to %d\n", 
                 this, aLine, aLine->mMaximumWidth);
#endif
          aState.UpdateMaximumWidth(aLine->mMaximumWidth);
        }
        if (aState.GetFlag(BRS_COMPUTEMAXELEMENTSIZE))
        {
#ifdef NOISY_MAX_ELEMENT_SIZE
          printf("nsBlockFrame::ReflowLine block %p line %p setting aLine.mMaxElementWidth to %d\n", 
                 this, aLine, aLine->mMaxElementWidth);
#endif
          aState.UpdateMaxElementSize(nsSize(aLine->mMaxElementWidth, aLine->mBounds.height));
        }
      }
    }

    // We don't really know what changed in the line, so use the union
    // of the old and new combined areas
    // SEC: added "aLine->IsForceInvalidate()" for bug 45152
    if (aDamageDirtyArea || aLine->IsForceInvalidate()) {
      aLine->SetForceInvalidate(PR_FALSE);  // doing the invalidate now, force flag to off
      nsRect combinedArea;
      aLine->GetCombinedArea(&combinedArea);

      nsRect dirtyRect;
      dirtyRect.UnionRect(oldCombinedArea, combinedArea);
#ifdef NOISY_BLOCK_INVALIDATE
      printf("%p invalidate because %s is true (%d, %d, %d, %d)\n",
             this, aDamageDirtyArea ? "aDamageDirtyArea" : "aLine->IsForceInvalidate",
             dirtyRect.x, dirtyRect.y, dirtyRect.width, dirtyRect.height);
      if (aLine->IsForceInvalidate())
        printf("  dirty line is %p\n");
#endif
      Invalidate(aState.mPresContext, dirtyRect);
    }
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
                        PRBool aDamageDeletedLines,
                        nsIFrame*& aFrameResult)
{
  nsresult rv = NS_OK;
  PRBool stopPulling;
  aFrameResult = nsnull;

  // First check our remaining lines
  while (end_lines() != aLine.next()) {
    rv = PullFrameFrom(aState, aLine, mLines, aLine.next(), PR_FALSE,
                       aDamageDeletedLines, aFrameResult, stopPulling);
    if (NS_FAILED(rv) || stopPulling) {
      return rv;
    }
  }

  // Pull frames from the next-in-flow(s) until we can't
  nsBlockFrame* nextInFlow = aState.mNextInFlow;
  while (nsnull != nextInFlow) {
    if (nextInFlow->mLines.empty()) {
      nextInFlow = (nsBlockFrame*) nextInFlow->mNextInFlow;
      aState.mNextInFlow = nextInFlow;
      continue;
    }
    rv = PullFrameFrom(aState, aLine, nextInFlow->mLines,
                       nextInFlow->mLines.begin(), PR_TRUE,
                       aDamageDeletedLines, aFrameResult, stopPulling);
    if (NS_FAILED(rv) || stopPulling) {
      return rv;
    }
  }
  return rv;
}

/**
 * Try to pull a frame out of a line pointed at by aFromLine. If a frame
 * is pulled then aPulled will be set to PR_TRUE.  In addition, if
 * aUpdateGeometricParent is set then the pulled frames geometric parent
 * will be updated (e.g. when pulling from a next-in-flows line list).
 *
 * Note: pulling a frame from a line that is a place-holder frame
 * doesn't automatically remove the corresponding floater from the
 * line's floater array. This happens indirectly: either the line gets
 * emptied (and destroyed) or the line gets reflowed (because we mark
 * it dirty) and the code at the top of ReflowLine empties the
 * array. So eventually, it will be removed, just not right away.
 */
nsresult
nsBlockFrame::PullFrameFrom(nsBlockReflowState& aState,
                            nsLineBox* aLine,
                            nsLineList& aFromContainer,
                            nsLineList::iterator aFromLine,
                            PRBool aUpdateGeometricParent,
                            PRBool aDamageDeletedLines,
                            nsIFrame*& aFrameResult,
                            PRBool& aStopPulling)
{
  nsLineBox* fromLine = aFromLine;
  NS_ABORT_IF_FALSE(fromLine, "bad line to pull from");
  NS_ABORT_IF_FALSE(fromLine->GetChildCount(), "empty line");
  NS_ABORT_IF_FALSE(aLine->GetChildCount(), "empty line");

  if (fromLine->IsBlock()) {
    // If our line is not empty and the child in aFromLine is a block
    // then we cannot pull up the frame into this line. In this case
    // we stop pulling.
    aStopPulling = PR_TRUE;
    aFrameResult = nsnull;
  }
  else {
    // Take frame from fromLine
    nsIFrame* frame = fromLine->mFirstChild;
    aLine->SetChildCount(aLine->GetChildCount() + 1);

    PRInt32 fromLineChildCount = fromLine->GetChildCount();
    if (0 != --fromLineChildCount) {
      // Mark line dirty now that we pulled a child
      fromLine->SetChildCount(fromLineChildCount);
      fromLine->MarkDirty();
      frame->GetNextSibling(&fromLine->mFirstChild);
    }
    else {
      // Free up the fromLine now that it's empty
      // Its bounds might need to be redrawn, though.
      if (aDamageDeletedLines) {
        Invalidate(aState.mPresContext, fromLine->mBounds);
      }
      if (aFromLine.next() != end_lines())
        aFromLine.next()->MarkPreviousMarginDirty();
      aFromContainer.erase(aFromLine);
      aState.FreeLineBox(fromLine);
    }

    // Change geometric parents
    if (aUpdateGeometricParent) {
      // Before we set the new parent frame get the current parent
      nsIFrame* oldParentFrame;
      frame->GetParent(&oldParentFrame);
      frame->SetParent(this);

      // When pushing and pulling frames we need to check for whether any
      // views need to be reparented
      NS_ASSERTION(oldParentFrame != this, "unexpected parent frame");
      nsHTMLContainerFrame::ReparentFrameView(aState.mPresContext, frame, oldParentFrame, this);
      
      // The frame is being pulled from a next-in-flow; therefore we
      // need to add it to our sibling list.
      if (nsnull != aState.mPrevChild) {
        aState.mPrevChild->SetNextSibling(frame);
      }
      frame->SetNextSibling(nsnull);
    }

    // Stop pulling because we found a frame to pull
    aStopPulling = PR_TRUE;
    aFrameResult = frame;
#ifdef DEBUG
    VerifyLines(PR_TRUE);
#endif
  }
  return NS_OK;
}

static void
PlaceFrameView(nsIPresContext* aPresContext,
               nsIFrame*       aFrame)
{
  nsIView*  view;
  aFrame->GetView(aPresContext, &view);
  if (view)
    nsContainerFrame::SyncFrameViewAfterReflow(aPresContext, aFrame, view, nsnull);

  nsContainerFrame::PositionChildViews(aPresContext, aFrame);
}

void
nsBlockFrame::SlideLine(nsBlockReflowState& aState,
                        nsLineBox* aLine, nscoord aDY)
{
  NS_PRECONDITION(aDY != 0, "why slide a line nowhere?");

  PRBool doInvalidate = !aLine->mBounds.IsEmpty();
  if (doInvalidate)
    Invalidate(aState.mPresContext, aLine->mBounds);
  // Adjust line state
  aLine->SlideBy(aDY);
  if (doInvalidate)
    Invalidate(aState.mPresContext, aLine->mBounds);

  // Adjust the frames in the line
  nsIFrame* kid = aLine->mFirstChild;
  if (!kid) {
    return;
  }

  if (aLine->IsBlock()) {
    nsRect r;
    kid->GetRect(r);
    if (aDY) {
      r.y += aDY;
      kid->SetRect(aState.mPresContext, r);
    }

    // Make sure the frame's view and any child views are updated
    ::PlaceFrameView(aState.mPresContext, kid);

    // If the child has any floaters that impact the space-manager,
    // place them now so that they are present in the space-manager
    // again (they were removed by the space-manager's frame when
    // the reflow began).
    nsBlockFrame* bf;
    nsresult rv = kid->QueryInterface(kBlockFrameCID, (void**) &bf);
    if (NS_SUCCEEDED(rv)) {
      // Translate spacemanager to the child blocks upper-left corner
      // so that when it places its floaters (which are relative to
      // it) the right coordinates are used. Note that we have already
      // been translated by our border+padding so factor that in to
      // get the right translation.
      const nsMargin& bp = aState.BorderPadding();
      nscoord dx = r.x - bp.left;
      nscoord dy = r.y - bp.top;
      aState.mSpaceManager->Translate(dx, dy);
      bf->UpdateSpaceManager(aState.mPresContext, aState.mSpaceManager);
      aState.mSpaceManager->Translate(-dx, -dy);
    }
  }
  else {
    // Adjust the Y coordinate of the frames in the line.
    // Note: we need to re-position views even if aDY is 0, because
    // one of our parent frames may have moved and so the view's position
    // relative to its parent may have changed
    nsRect r;
    PRInt32 n = aLine->GetChildCount();
    while (--n >= 0) {
      if (aDY) {
        kid->GetRect(r);
        r.y += aDY;
        kid->SetRect(aState.mPresContext, r);
      }
      // Make sure the frame's view and any child views are updated
      ::PlaceFrameView(aState.mPresContext, kid);
      kid->GetNextSibling(&kid);
    }
  }
}

nsresult
nsBlockFrame::UpdateSpaceManager(nsIPresContext* aPresContext,
                                 nsISpaceManager* aSpaceManager)
{
  for (line_iterator line = begin_lines(), line_end = end_lines();
       line != line_end;
       ++line) {
    // Place the floaters in the spacemanager
    if (line->HasFloaters()) {
      nsFloaterCache* fc = line->GetFirstFloater();
      while (fc) {
        nsIFrame* floater = fc->mPlaceholder->GetOutOfFlowFrame();
        aSpaceManager->AddRectRegion(floater, fc->mRegion);
#ifdef NOISY_SPACEMANAGER
        nscoord tx, ty;
        aSpaceManager->GetTranslation(tx, ty);
        nsFrame::ListTag(stdout, this);
        printf(": UpdateSpaceManager: AddRectRegion: txy=%d,%d {%d,%d,%d,%d}\n",
               tx, ty,
               fc->mRegion.x, fc->mRegion.y,
               fc->mRegion.width, fc->mRegion.height);
#endif
        fc = fc->Next();
      }
    }

    // Tell kids about the move too
    if (line->mFirstChild && line->IsBlock()) {
      // If the child has any floaters that impact the space-manager,
      // place them now so that they are present in the space-manager
      // again (they were removed by the space-manager's frame when
      // the reflow began).
      nsBlockFrame* bf;
      nsresult rv = line->mFirstChild->QueryInterface(kBlockFrameCID,
                                            NS_REINTERPRET_CAST(void**, &bf));
      if (NS_SUCCEEDED(rv)) {
        nsPoint origin;
        bf->GetOrigin(origin);

        // Translate spacemanager to the child blocks upper-left
        // corner so that when it places its floaters (which are
        // relative to it) the right coordinates are used.
        aSpaceManager->Translate(origin.x, origin.y);
        bf->UpdateSpaceManager(aPresContext, aSpaceManager);
        aSpaceManager->Translate(-origin.x, -origin.y);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsBlockFrame::AttributeChanged(nsIPresContext* aPresContext,
                               nsIContent*     aChild,
                               PRInt32         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               PRInt32         aModType, 
                               PRInt32         aHint)
{
  nsresult rv = nsBlockFrameSuper::AttributeChanged(aPresContext, aChild,
                                                    aNameSpaceID, aAttribute, aModType, aHint);

  if (NS_FAILED(rv)) {
    return rv;
  }
  if (nsHTMLAtoms::start == aAttribute) {
    // XXX Not sure if this is necessary anymore
    RenumberLists(aPresContext);

    nsCOMPtr<nsIPresShell> shell;
    aPresContext->GetShell(getter_AddRefs(shell));
    
    nsIReflowCommand* reflowCmd;
    rv = NS_NewHTMLReflowCommand(&reflowCmd, this,
                                 nsIReflowCommand::ContentChanged,
                                 nsnull,
                                 aAttribute);
    if (NS_SUCCEEDED(rv)) {
      shell->AppendReflowCommand(reflowCmd);
      NS_RELEASE(reflowCmd);
    }
  }
  else if (nsHTMLAtoms::value == aAttribute) {
    const nsStyleDisplay* styleDisplay;
    GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&) styleDisplay);
    if (NS_STYLE_DISPLAY_LIST_ITEM == styleDisplay->mDisplay) {
      nsIFrame* nextAncestor = mParent;
      nsBlockFrame* blockParent = nsnull;
      
      // Search for the closest ancestor that's a block frame. We
      // make the assumption that all related list items share a
      // common block parent.
      // XXXldb I think that's a bad assumption.
      while (nextAncestor) {
        if (NS_OK == nextAncestor->QueryInterface(kBlockFrameCID, 
                                                  (void**)&blockParent)) {
          break;
        }
        nextAncestor->GetParent(&nextAncestor);
      }

      // Tell the enclosing block frame to renumber list items within
      // itself
      if (nsnull != blockParent) {
        // XXX Not sure if this is necessary anymore
        blockParent->RenumberLists(aPresContext);

        nsCOMPtr<nsIPresShell> shell;
        aPresContext->GetShell(getter_AddRefs(shell));
        
        nsIReflowCommand* reflowCmd;
        rv = NS_NewHTMLReflowCommand(&reflowCmd, blockParent,
                                     nsIReflowCommand::ContentChanged,
                                     nsnull,
                                     aAttribute);
        if (NS_SUCCEEDED(rv)) {
          shell->AppendReflowCommand(reflowCmd);
          NS_RELEASE(reflowCmd);
        }
      }
    }
  }

  return rv;
}

inline PRBool
IsBorderZero(nsStyleUnit aUnit, nsStyleCoord &aCoord)
{
    return ((aUnit == eStyleUnit_Coord && aCoord.GetCoordValue() == 0));
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

NS_IMETHODIMP
nsBlockFrame::IsEmpty(PRBool aIsQuirkMode, PRBool aIsPre, PRBool *aResult)
{
  // XXXldb In hindsight, I'm not sure why I made this check the margin,
  // but it seems to work right and I'm a little hesitant to change it.
  const nsStyleMargin* margin = NS_STATIC_CAST(const nsStyleMargin*,
                             mStyleContext->GetStyleData(eStyleStruct_Margin));
  const nsStyleBorder* border = NS_STATIC_CAST(const nsStyleBorder*,
                             mStyleContext->GetStyleData(eStyleStruct_Border));
  const nsStylePadding* padding = NS_STATIC_CAST(const nsStylePadding*,
                            mStyleContext->GetStyleData(eStyleStruct_Padding));
  nsStyleCoord coord;
  if ((border->IsBorderSideVisible(NS_SIDE_TOP) &&
       !IsBorderZero(border->mBorder.GetTopUnit(),
                     border->mBorder.GetTop(coord))) ||
      (border->IsBorderSideVisible(NS_SIDE_BOTTOM) &&
       !IsBorderZero(border->mBorder.GetTopUnit(),
                     border->mBorder.GetTop(coord))) ||
      !IsPaddingZero(padding->mPadding.GetTopUnit(),
                    padding->mPadding.GetTop(coord)) ||
      !IsPaddingZero(padding->mPadding.GetBottomUnit(),
                    padding->mPadding.GetBottom(coord)) ||
      !IsMarginZero(margin->mMargin.GetTopUnit(),
                    margin->mMargin.GetTop(coord)) ||
      !IsMarginZero(margin->mMargin.GetBottomUnit(),
                    margin->mMargin.GetBottom(coord))) {
    *aResult = PR_FALSE;
    return NS_OK;
  }


  const nsStyleText* styleText = NS_STATIC_CAST(const nsStyleText*,
      mStyleContext->GetStyleData(eStyleStruct_Text));
  PRBool isPre =
      ((NS_STYLE_WHITESPACE_PRE == styleText->mWhiteSpace) ||
       (NS_STYLE_WHITESPACE_MOZ_PRE_WRAP == styleText->mWhiteSpace));
  *aResult = PR_TRUE;
  for (line_iterator line = begin_lines(), line_end = end_lines();
       line != line_end;
       ++line)
  {
    line->IsEmpty(aIsQuirkMode, isPre, aResult);
    if (! *aResult)
      break;
  }
  return NS_OK;
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
  for (line_iterator line = begin_lines();
       line != aLine;
       ++line) {
    if (line->IsBlock()) {
      // A line which preceeds aLine contains a block; therefore the
      // top margin applies.
      // XXXldb this is wrong if that block is empty and the margin
      // is collapsed outside the parent
      aState.SetFlag(BRS_APPLYTOPMARGIN, PR_TRUE);
      return PR_TRUE;
    }
    // No need to apply the top margin if the line has floaters.  We
    // should collapse anyway (bug 44419)
  }

  // The line being reflowed is "essentially" the first line in the
  // block. Therefore its top-margin will be collapsed by the
  // generational collapsing logic with its parent (us).
  return PR_FALSE;
}

nsIFrame*
nsBlockFrame::GetTopBlockChild()
{
  nsIFrame* firstChild = mLines.empty() ? nsnull : mLines.front()->mFirstChild;
  if (firstChild) {
    if (mLines.front()->IsBlock()) {
      // Winner
      return firstChild;
    }

    // If the first line is not a block line then the second line must
    // be a block line otherwise the top child can't be a block.
    line_iterator next = begin_lines();
    ++next;
    if ((next == end_lines()) || !next->IsBlock()) {
      // There is no line after the first line or its not a block so
      // don't bother trying to skip over the first line.
      return nsnull;
    }

    // The only time we can skip over the first line and pretend its
    // not there is if the line contains only compressed
    // whitespace. If white-space is significant to this frame then we
    // can't skip over the line.
    const nsStyleText* styleText;
    GetStyleData(eStyleStruct_Text, (const nsStyleStruct*&) styleText);
    if ((NS_STYLE_WHITESPACE_PRE == styleText->mWhiteSpace) ||
        (NS_STYLE_WHITESPACE_MOZ_PRE_WRAP == styleText->mWhiteSpace)) {
      // Whitespace is significant
      return nsnull;
    }

    // See if each frame is a text frame that contains nothing but
    // whitespace.
    PRInt32 n = mLines.front()->GetChildCount();
    while (--n >= 0) {
      nsIContent* content;
      nsresult rv = firstChild->GetContent(&content);
      if (NS_FAILED(rv) || !content) {
        return nsnull;
      }
      if (!content->IsContentOfType(nsIContent::eTEXT)) {
        NS_RELEASE(content);
        return nsnull;
      }
      nsITextContent* tc;
      rv = content->QueryInterface(kITextContentIID, (void**) &tc);
      NS_RELEASE(content);
      if (NS_FAILED(rv) || (nsnull == tc)) {
        return nsnull;
      }
      PRBool isws = PR_FALSE;
      tc->IsOnlyWhitespace(&isws);
      NS_RELEASE(tc);
      if (!isws) {
        return nsnull;
      }
      firstChild->GetNextSibling(&firstChild);
    }

    // If we make it to this point then every frame on the first line
    // was compressible white-space. Since we already know that the
    // second line contains a block, that block is the
    // top-block-child.
    return next->mFirstChild;
  }

  return nsnull;
}

nsresult
nsBlockFrame::ReflowBlockFrame(nsBlockReflowState& aState,
                               line_iterator aLine,
                               PRBool* aKeepReflowGoing)
{
  NS_PRECONDITION(*aKeepReflowGoing, "bad caller");

  nsresult rv = NS_OK;

  nsIFrame* frame = aLine->mFirstChild;

  // Prepare the block reflow engine
  const nsStyleDisplay* display;
  frame->GetStyleData(eStyleStruct_Display,
                      (const nsStyleStruct*&) display);
  nsBlockReflowContext brc(aState.mPresContext, aState.mReflowState,
                           aState.GetFlag(BRS_COMPUTEMAXELEMENTSIZE),
                           aState.GetFlag(BRS_COMPUTEMAXWIDTH));
  brc.SetNextRCFrame(aState.mNextRCFrame);

  // See if we should apply the top margin. If the block frame being
  // reflowed is a continuation (non-null prev-in-flow) then we don't
  // apply its top margin because its not significant. Otherwise, dig
  // deeper.
  PRBool applyTopMargin = PR_FALSE;
  nsIFrame* framePrevInFlow;
  frame->GetPrevInFlow(&framePrevInFlow);
  if (nsnull == framePrevInFlow) {
    applyTopMargin = ShouldApplyTopMargin(aState, aLine);
  }

  // Clear past floaters before the block if the clear style is not none
  PRUint8 breakType = display->mBreakType;
  aLine->SetBreakType(breakType);
  if (NS_STYLE_CLEAR_NONE != breakType) {
    PRBool alsoApplyTopMargin = aState.ClearPastFloaters(breakType);
    if (alsoApplyTopMargin) {
      applyTopMargin = PR_TRUE;
    }
#ifdef NOISY_VERTICAL_MARGINS
    ListTag(stdout);
    printf(": y=%d child ", aState.mY);
    ListTag(stdout, frame);
    printf(" has clear of %d => %s, mPrevBottomMargin=%d\n",
           breakType,
           applyTopMargin ? "applyTopMargin" : "nope",
           aState.mPrevBottomMargin);
#endif
  }

  nscoord topMargin = 0;
  if (applyTopMargin) {
    // Precompute the blocks top margin value so that we can get the
    // correct available space (there might be a floater that's
    // already been placed below the aState.mPrevBottomMargin

    // Setup a reflowState to get the style computed margin-top value

    // The availSpace here is irrelevant to our needs - all we want
    // out if this setup is the margin-top value which doesn't depend
    // on the childs available space.
    nsSize availSpace(aState.mContentArea.width, NS_UNCONSTRAINEDSIZE);
    nsHTMLReflowState reflowState(aState.mPresContext, aState.mReflowState,
                                  frame, availSpace);

    // Now compute the collapsed margin-top value into aState.mPrevBottomMargin
    nsCollapsingMargin oldPrevBottomMargin = aState.mPrevBottomMargin;
    nsBlockReflowContext::ComputeCollapsedTopMargin(aState.mPresContext,
                                                    reflowState,
                                                    aState.mPrevBottomMargin);
    topMargin = aState.mPrevBottomMargin.get();
    aState.mPrevBottomMargin = oldPrevBottomMargin; // perhaps not needed

    // Temporarily advance the running Y value so that the
    // GetAvailableSpace method will return the right available
    // space. This undone as soon as the margin is computed.
    aState.mY += topMargin;
  }

  // Compute the available space for the block
  aState.GetAvailableSpace();
#ifdef REALLY_NOISY_REFLOW
  printf("setting line %p isImpacted to %s\n", aLine, aState.IsImpactedByFloater()?"true":"false");
#endif
  PRBool isImpacted = aState.IsImpactedByFloater() ? PR_TRUE : PR_FALSE;
  aLine->SetLineIsImpactedByFloater(isImpacted);
  nsSplittableType splitType = NS_FRAME_NOT_SPLITTABLE;
  frame->IsSplittable(splitType);
  nsRect availSpace;
  aState.ComputeBlockAvailSpace(frame, splitType, display, availSpace);

  // Now put the Y coordinate back and flow the block letting the
  // block reflow context compute the same top margin value we just
  // computed (sigh).
  if (topMargin) {
    aState.mY -= topMargin;
    availSpace.y -= topMargin;
    if (NS_UNCONSTRAINEDSIZE != availSpace.height) {
      availSpace.height += topMargin;
    }
  }

  // Reflow the block into the available space
  nsReflowStatus frameReflowStatus=NS_FRAME_COMPLETE;
  nsMargin computedOffsets;
  rv = brc.ReflowBlock(frame, availSpace,
                       applyTopMargin, aState.mPrevBottomMargin,
                       aState.IsAdjacentWithTop(),
                       computedOffsets, frameReflowStatus);
  if (brc.BlockShouldInvalidateItself()) {
    Invalidate(aState.mPresContext, mRect);
  }

  if (frame == aState.mNextRCFrame) {
    // NULL out mNextRCFrame so if we reflow it again we don't think it's still
    // an incremental reflow
    aState.mNextRCFrame = nsnull;
  }
  if (NS_FAILED(rv)) {
    return rv;
  }
  aState.mPrevChild = frame;

#if defined(REFLOW_STATUS_COVERAGE)
  RecordReflowStatus(PR_TRUE, frameReflowStatus);
#endif

  if (NS_INLINE_IS_BREAK_BEFORE(frameReflowStatus)) {
    // None of the child block fits.
    PushLines(aState, aLine.prev());
    *aKeepReflowGoing = PR_FALSE;
    aState.mReflowStatus = NS_FRAME_NOT_COMPLETE;
  }
  else {
    // Note: line-break-after a block is a nop

    // Try to place the child block
    PRBool isAdjacentWithTop = aState.IsAdjacentWithTop();
    nsCollapsingMargin collapsedBottomMargin;
    nsRect combinedArea(0,0,0,0);
    *aKeepReflowGoing = brc.PlaceBlock(isAdjacentWithTop, computedOffsets,
                                       collapsedBottomMargin,
                                       aLine->mBounds, combinedArea);
    aLine->SetCarriedOutBottomMargin(collapsedBottomMargin);

    if (aState.GetFlag(BRS_SHRINKWRAPWIDTH)) {
      // Mark the line as dirty so once we known the final shrink wrap width
      // we can reflow the block to the correct size
      // XXX We don't always need to do this...
      aLine->MarkDirty();
      aState.SetFlag(BRS_NEEDRESIZEREFLOW, PR_TRUE);
    }
    if (aState.GetFlag(BRS_UNCONSTRAINEDWIDTH) || aState.GetFlag(BRS_SHRINKWRAPWIDTH)) {
      // Add the right margin to the line's bounds.  That way it will be
      // taken into account when we compute our shrink wrap size.
      nscoord marginRight = brc.GetMargin().right;
      if (marginRight != NS_UNCONSTRAINEDSIZE) {
        aLine->mBounds.width += marginRight;
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
        rv = CreateContinuationFor(aState, aLine, frame, madeContinuation);
        if (NS_FAILED(rv)) {
          return rv;
        }

        // Push continuation to a new line, but only if we actually
        // made one.
        if (madeContinuation) {
          frame->GetNextSibling(&frame);
          nsLineBox* line = aState.NewLineBox(frame, 1, PR_TRUE);
          if (nsnull == line) {
            return NS_ERROR_OUT_OF_MEMORY;
          }
          mLines.after_insert(aLine, line);

          // Do not count the continuation child on the line it used
          // to be on
          aLine->SetChildCount(aLine->GetChildCount() - 1);
       }

        // Advance to next line since some of the block fit. That way
        // only the following lines will be pushed.
        PushLines(aState, aLine);
        aState.mReflowStatus = NS_FRAME_NOT_COMPLETE;
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

      // Post-process the "line"
      nsSize maxElementSize(0, 0);
      if (aState.GetFlag(BRS_COMPUTEMAXELEMENTSIZE)) {
        maxElementSize = brc.GetMaxElementSize();
        if (aState.IsImpactedByFloater() &&
            (NS_FRAME_SPLITTABLE_NON_RECTANGULAR != splitType)) {
          // Add in floater impacts to the lines max-element-size, but
          // only if the block element isn't one of us (otherwise the
          // floater impacts will be counted twice).
          ComputeLineMaxElementSize(aState, aLine, &maxElementSize);
        }
      }
      // If we asked the block to update its maximum width, then record the
      // updated value in the line, and update the current maximum width
      if (aState.GetFlag(BRS_COMPUTEMAXWIDTH)) {
        aLine->mMaximumWidth = brc.GetMaximumWidth();
        // need to add in margin on block's reported max width (see bug 35964)
        const nsMargin& margin = brc.GetMargin();
        aLine->mMaximumWidth += margin.left + margin.right;
#ifdef NOISY_MAXIMUM_WIDTH
        printf("nsBlockFrame::ReflowBlockFrame parent block %p line %p aLine->mMaximumWidth set to brc.GetMaximumWidth %d, updating aState.mMaximumWidth\n", 
             this, aLine, aLine->mMaximumWidth);
#endif
        aState.UpdateMaximumWidth(aLine->mMaximumWidth);

      }
      PostPlaceLine(aState, aLine, maxElementSize);

      // If the block frame that we just reflowed happens to be our
      // first block, then its computed ascent is ours
      if (frame == GetTopBlockChild()) {
        const nsHTMLReflowMetrics& metrics = brc.GetMetrics();
        mAscent = metrics.ascent;
      }

      // Place the "marker" (bullet) frame.
      //
      // According to the CSS2 spec, section 12.6.1, the "marker" box
      // participates in the height calculation of the list-item box's
      // first line box.
      //
      // There are exactly two places a bullet can be placed: near the
      // first or second line. Its only placed on the second line in a
      // rare case: an empty first line followed by a second line that
      // contains a block (example: <LI>\n<P>... ). This is where
      // the second case can happen.
      if (mBullet && HaveOutsideBullet() &&
          ((aLine == mLines.front()) ||
           ((0 == mLines.front()->mBounds.height) &&
            (aLine == begin_lines().next())))) {
        // Reflow the bullet
        nsHTMLReflowMetrics metrics(nsnull);
        ReflowBullet(aState, metrics);

        // For bullets that are placed next to a child block, there will
        // be no correct ascent value. Therefore, make one up...
        // XXXldb We should be able to get the correct ascent value now.
        nscoord ascent = 0;
        const nsStyleFont* font;
        frame->GetStyleData(eStyleStruct_Font,
                            (const nsStyleStruct*&) font);
        nsIRenderingContext& rc = *aState.mReflowState.rendContext;
        rc.SetFont(font->mFont);
        nsIFontMetrics* fm;
        rv = rc.GetFontMetrics(fm);
        if (NS_SUCCEEDED(rv) && (nsnull != fm)) {
          fm->GetMaxAscent(ascent);
          NS_RELEASE(fm);
        }
        rv = NS_OK;

        // Tall bullets won't look particularly nice here...
        nsRect bbox;
        mBullet->GetRect(bbox);
        nscoord bulletTopMargin = applyTopMargin
                                    ? collapsedBottomMargin.get()
                                    : 0;
        bbox.y = aState.BorderPadding().top + ascent -
          metrics.ascent + bulletTopMargin;
        mBullet->SetRect(aState.mPresContext, bbox);
      }
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
        PushLines(aState, aLine.prev());
        aState.mReflowStatus = NS_FRAME_NOT_COMPLETE;
      }
    }
  }
#ifdef DEBUG
  VerifyLines(PR_TRUE);
#endif
  return rv;
}

#define LINE_REFLOW_OK   0
#define LINE_REFLOW_STOP 1
#define LINE_REFLOW_REDO 2

nsresult
nsBlockFrame::ReflowInlineFrames(nsBlockReflowState& aState,
                                 line_iterator aLine,
                                 PRBool* aKeepReflowGoing,
                                 PRBool aDamageDirtyArea,
                                 PRBool aUpdateMaximumWidth)
{
  nsresult rv = NS_OK;
  *aKeepReflowGoing = PR_TRUE;

#ifdef DEBUG
  PRInt32 spins = 0;
#endif
  PRUint8 lineReflowStatus = LINE_REFLOW_REDO;
  while (LINE_REFLOW_REDO == lineReflowStatus) {
    // Prevent overflowing limited thread stacks by creating
    // nsLineLayout from the heap when the frame tree depth gets
    // large.
    if (aState.mReflowState.mReflowDepth > 30) {//XXX layout-tune.h?
      rv = DoReflowInlineFramesMalloc(aState, aLine, aKeepReflowGoing,
                                      &lineReflowStatus,
                                      aUpdateMaximumWidth, aDamageDirtyArea);
    }
    else {
      rv = DoReflowInlineFramesAuto(aState, aLine, aKeepReflowGoing,
                                    &lineReflowStatus,
                                    aUpdateMaximumWidth, aDamageDirtyArea);
    }
    if (NS_FAILED(rv)) {
      break;
    }
#ifdef DEBUG
    spins++;
    if (1000 == spins) {
      ListTag(stdout);
      printf(": yikes! spinning on a line over 1000 times!\n");
      NS_ABORT();
    }
#endif
  }
  return rv;
}

nsresult
nsBlockFrame::DoReflowInlineFramesMalloc(nsBlockReflowState& aState,
                                         line_iterator aLine,
                                         PRBool* aKeepReflowGoing,
                                         PRUint8* aLineReflowStatus,
                                         PRBool aUpdateMaximumWidth,
                                         PRBool aDamageDirtyArea)
{
  // XXXldb Using the PresShell arena here would be nice.
  nsLineLayout* ll = new nsLineLayout(aState.mPresContext,
                                      aState.mReflowState.mSpaceManager,
                                      &aState.mReflowState,
                                      aState.GetFlag(BRS_COMPUTEMAXELEMENTSIZE));
  if (!ll) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  ll->Init(&aState, aState.mMinLineHeight, aState.mLineNumber);
  nsresult rv = DoReflowInlineFrames(aState, *ll, aLine, aKeepReflowGoing,
                                     aLineReflowStatus, aUpdateMaximumWidth, aDamageDirtyArea);
  ll->EndLineReflow();
  delete ll;
  return rv;
}

nsresult
nsBlockFrame::DoReflowInlineFramesAuto(nsBlockReflowState& aState,
                                       line_iterator aLine,
                                       PRBool* aKeepReflowGoing,
                                       PRUint8* aLineReflowStatus,
                                       PRBool aUpdateMaximumWidth,
                                       PRBool aDamageDirtyArea)
{
  nsLineLayout lineLayout(aState.mPresContext,
                          aState.mReflowState.mSpaceManager,
                          &aState.mReflowState,
                          aState.GetFlag(BRS_COMPUTEMAXELEMENTSIZE));
  lineLayout.Init(&aState, aState.mMinLineHeight, aState.mLineNumber);
  nsresult rv = DoReflowInlineFrames(aState, lineLayout, aLine,
                                     aKeepReflowGoing, aLineReflowStatus,
                                     aUpdateMaximumWidth, aDamageDirtyArea);
  lineLayout.EndLineReflow();
  return rv;
}

nsresult
nsBlockFrame::DoReflowInlineFrames(nsBlockReflowState& aState,
                                   nsLineLayout& aLineLayout,
                                   line_iterator aLine,
                                   PRBool* aKeepReflowGoing,
                                   PRUint8* aLineReflowStatus,
                                   PRBool aUpdateMaximumWidth,
                                   PRBool aDamageDirtyArea)
{
  // Forget all of the floaters on the line
  aLine->FreeFloaters(aState.mFloaterCacheFreeList);
  aState.mFloaterCombinedArea.SetRect(0, 0, 0, 0);
  aState.mRightFloaterCombinedArea.SetRect(0, 0, 0, 0);

  // Setup initial coordinate system for reflowing the inline frames
  // into. Apply a previous block frame's bottom margin first.
  aState.mY += aState.mPrevBottomMargin.get();
  aState.GetAvailableSpace();
  PRBool impactedByFloaters = aState.IsImpactedByFloater() ? PR_TRUE : PR_FALSE;
  aLine->SetLineIsImpactedByFloater(impactedByFloaters);
#ifdef REALLY_NOISY_REFLOW
  printf("nsBlockFrame::DoReflowInlineFrames %p impacted = %d\n",
         this, impactedByFloaters);
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
  if (aUpdateMaximumWidth) {
    availWidth = NS_UNCONSTRAINEDSIZE;
  }
#ifdef IBMBIDI
  else {
    nscoord rightEdge = aState.mReflowState.mRightEdge;
    if ( (rightEdge != NS_UNCONSTRAINEDSIZE)
         && (availWidth < rightEdge) ) {
      availWidth = rightEdge;
    }
  }
#endif // IBMBIDI
  aLineLayout.BeginLineReflow(x, aState.mY,
                              availWidth, availHeight,
                              impactedByFloaters,
                              PR_FALSE /*XXX isTopOfPage*/);

  // XXX Unfortunately we need to know this before reflowing the first
  // inline frame in the line. FIX ME.
  if ((0 == aLineLayout.GetLineNumber()) &&
      (NS_BLOCK_HAS_FIRST_LETTER_STYLE & mState)) {
    aLineLayout.SetFirstLetterStyleOK(PR_TRUE);
  }

  // Reflow the frames that are already on the line first
  nsresult rv = NS_OK;
  PRUint8 lineReflowStatus = LINE_REFLOW_OK;
  PRInt32 i;
  nsIFrame* frame = aLine->mFirstChild;
  aLine->SetHasPercentageChild(PR_FALSE); // To be set by ReflowInlineFrame below
  // need to repeatedly call GetChildCount here, because the child
  // count can change during the loop!
  for (i = 0; i < aLine->GetChildCount(); i++) { 
    rv = ReflowInlineFrame(aState, aLineLayout, aLine, frame,
                           &lineReflowStatus);
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (LINE_REFLOW_OK != lineReflowStatus) {
      // It is possible that one or more of next lines are empty
      // (because of DeleteChildsNextInFlow). If so, delete them now
      // in case we are finished.
      ++aLine;
      while ((aLine != end_lines()) && (0 == aLine->GetChildCount())) {
        // XXX Is this still necessary now that DeleteChildsNextInFlow
        // uses DoRemoveFrame?
        nsLineBox *toremove = aLine;
        aLine = mLines.erase(aLine);
        NS_ASSERTION(nsnull == toremove->mFirstChild, "bad empty line");
        aState.FreeLineBox(toremove);
      }
      --aLine;
      break;
    }
    frame->GetNextSibling(&frame);
  }

  // Pull frames and reflow them until we can't
  while (LINE_REFLOW_OK == lineReflowStatus) {
    rv = PullFrame(aState, aLine, aDamageDirtyArea, frame);
    if (NS_FAILED(rv)) {
      return rv;
    }
    if (nsnull == frame) {
      break;
    }
    while (LINE_REFLOW_OK == lineReflowStatus) {
      PRInt32 oldCount = aLine->GetChildCount();
      rv = ReflowInlineFrame(aState, aLineLayout, aLine, frame,
                             &lineReflowStatus);
      if (NS_FAILED(rv)) {
        return rv;
      }
      if (aLine->GetChildCount() != oldCount) {
        // We just created a continuation for aFrame AND its going
        // to end up on this line (e.g. :first-letter
        // situation). Therefore we have to loop here before trying
        // to pull another frame.
        frame->GetNextSibling(&frame);
      }
      else {
        break;
      }
    }
  }
  if (LINE_REFLOW_REDO == lineReflowStatus) {
    // This happens only when we have a line that is impacted by
    // floaters and the first element in the line doesn't fit with
    // the floaters.
    //
    // What we do is to advance past the first floater we find and
    // then reflow the line all over again.
    NS_ASSERTION(aState.IsImpactedByFloater(),
                 "redo line on totally empty line");
    NS_ASSERTION(NS_UNCONSTRAINEDSIZE != aState.mAvailSpaceRect.height,
                 "unconstrained height on totally empty line");

    aState.mY += aState.mAvailSpaceRect.height;
    // XXX: a small optimization can be done here when paginating:
    // if the new Y coordinate is past the end of the block then
    // push the line and return now instead of later on after we are
    // past the floater.
  }
  else {
    // If we are propogating out a break-before status then there is
    // no point in placing the line.
    if (!NS_INLINE_IS_BREAK_BEFORE(aState.mReflowStatus)) {
      rv = PlaceLine(aState, aLineLayout, aLine, aKeepReflowGoing, aUpdateMaximumWidth);
    }
  }
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
                                PRUint8* aLineReflowStatus)
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

  // Remember if we have a percentage aware child on this line
  if (IsPercentageAwareChild(aFrame)) {
    aLine->SetHasPercentageChild(PR_TRUE);
  }

  // Reflow the inline frame
  nsReflowStatus frameReflowStatus;
  PRBool         pushedFrame;
  nsresult rv = aLineLayout.ReflowFrame(aFrame, &aState.mNextRCFrame,
                                        frameReflowStatus, nsnull, pushedFrame);
  if (aFrame == aState.mNextRCFrame) {
    // NULL out mNextRCFrame so if we reflow it again we don't think it's still
    // an incremental reflow
    aState.mNextRCFrame = nsnull;
  }
  if (NS_FAILED(rv)) {
    return rv;
  }
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
      to a left floater, even when that floater takes up all the width on a line.
      see bug 22496
   */

  // Process the child frames reflow status. There are 5 cases:
  // complete, not-complete, break-before, break-after-complete,
  // break-after-not-complete. There are two situations: we are a
  // block or we are an inline. This makes a total of 10 cases
  // (fortunately, there is some overlap).
  aLine->SetBreakType(NS_STYLE_CLEAR_NONE);
  if (NS_INLINE_IS_BREAK(frameReflowStatus)) {
    // Always abort the line reflow (because a line break is the
    // minimal amount of break we do).
    *aLineReflowStatus = LINE_REFLOW_STOP;

    // XXX what should aLine's break-type be set to in all these cases?
    PRUint8 breakType = NS_INLINE_GET_BREAK_TYPE(frameReflowStatus);
    NS_ASSERTION(breakType != NS_STYLE_CLEAR_NONE, "bad break type");
    NS_ASSERTION(NS_STYLE_CLEAR_PAGE != breakType, "no page breaks yet");

    if (NS_INLINE_IS_BREAK_BEFORE(frameReflowStatus)) {
      // Break-before cases.
      if (aFrame == aLine->mFirstChild) {
        // If we break before the first frame on the line then we must
        // be trying to place content where theres no room (e.g. on a
        // line with wide floaters). Inform the caller to reflow the
        // line after skipping past a floater.
        *aLineReflowStatus = LINE_REFLOW_REDO;
      }
      else {
        // It's not the first child on this line so go ahead and split
        // the line. We will see the frame again on the next-line.
        rv = SplitLine(aState, aLineLayout, aLine, aFrame);
        if (NS_FAILED(rv)) {
          return rv;
        }

        // If we're splitting the line because the frame didn't fit and it
        // was pushed, then mark the line as having word wrapped. We need to
        // know that if we're shrink wrapping our width
        if (pushedFrame) {
          aLine->SetLineWrapped(PR_TRUE);
        }
      }
    }
    else {
      // Break-after cases
      if (breakType == NS_STYLE_CLEAR_LINE) {
        if (!aLineLayout.GetLineEndsInBR()) {
          breakType = NS_STYLE_CLEAR_NONE;
        }
      }
      aLine->SetBreakType(breakType);
      if (NS_FRAME_IS_NOT_COMPLETE(frameReflowStatus)) {
        // Create a continuation for the incomplete frame. Note that the
        // frame may already have a continuation.
        PRBool madeContinuation;
        rv = CreateContinuationFor(aState, aLine, aFrame, madeContinuation);
        if (NS_FAILED(rv)) {
          return rv;
        }
        // Remember that the line has wrapped
        aLine->SetLineWrapped(PR_TRUE);
      }

      // Split line, but after the frame just reflowed
      nsIFrame* nextFrame;
      aFrame->GetNextSibling(&nextFrame);
      rv = SplitLine(aState, aLineLayout, aLine, nextFrame);
      if (NS_FAILED(rv)) {
        return rv;
      }

      if (NS_FRAME_IS_NOT_COMPLETE(frameReflowStatus)) {
        // Mark next line dirty in case SplitLine didn't end up
        // pushing any frames.
        nsLineBox* next = aLine.next();
        if ((nsnull != next) && !next->IsBlock()) {
          next->MarkDirty();
        }
      }
    }
  }
  else if (NS_FRAME_IS_NOT_COMPLETE(frameReflowStatus)) {
    // Frame is not-complete, no special breaking status

    // Create a continuation for the incomplete frame. Note that the
    // frame may already have a continuation.
    PRBool madeContinuation;
    rv = CreateContinuationFor(aState, aLine, aFrame, madeContinuation);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Remember that the line has wrapped
    aLine->SetLineWrapped(PR_TRUE);
    
    // If we are reflowing the first letter frame then don't split the
    // line and don't stop the line reflow...
    PRBool splitLine = !reflowingFirstLetter;
    if (reflowingFirstLetter) {
      nsCOMPtr<nsIAtom> frameType;
      aFrame->GetFrameType(getter_AddRefs(frameType));
      if ((nsLayoutAtoms::inlineFrame == frameType.get()) ||
          (nsLayoutAtoms::lineFrame == frameType.get())) {
        splitLine = PR_TRUE;
      }
    }

    if (splitLine) {
      // Split line after the current frame
      *aLineReflowStatus = LINE_REFLOW_STOP;
      aFrame->GetNextSibling(&aFrame);
      rv = SplitLine(aState, aLineLayout, aLine, aFrame);
      if (NS_FAILED(rv)) {
        return rv;
      }

      // Mark next line dirty in case SplitLine didn't end up
      // pushing any frames.
      nsLineBox* next = aLine.next();
      if ((nsnull != next) && !next->IsBlock()) {
        next->MarkDirty();
      }
    }
  }

  return NS_OK;
}

/**
 * Create a continuation, if necessary, for aFrame. Place it on the
 * same line that aFrame is on. Set aMadeNewFrame to PR_TRUE if a
 * new frame is created.
 */
nsresult
nsBlockFrame::CreateContinuationFor(nsBlockReflowState& aState,
                                    nsLineBox* aLine,
                                    nsIFrame* aFrame,
                                    PRBool& aMadeNewFrame)
{
  aMadeNewFrame = PR_FALSE;
  nsresult rv;
  nsIFrame* nextInFlow;
  rv = CreateNextInFlow(aState.mPresContext, this, aFrame, nextInFlow);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (nsnull != nextInFlow) {
    aMadeNewFrame = PR_TRUE;
    aLine->SetChildCount(aLine->GetChildCount() + 1);
  }
#ifdef DEBUG
  VerifyLines(PR_FALSE);
#endif
  return rv;
}

nsresult
nsBlockFrame::SplitLine(nsBlockReflowState& aState,
                        nsLineLayout& aLineLayout,
                        line_iterator aLine,
                        nsIFrame* aFrame)
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
      aLine->List(aState.mPresContext, stdout, gNoiseIndent+1);
    }
  }
#endif

  if (0 != pushCount) {
    NS_ABORT_IF_FALSE(aLine->GetChildCount() > pushCount, "bad push");
    NS_ABORT_IF_FALSE(nsnull != aFrame, "whoops");

    // Put frames being split out into their own line
    nsLineBox* newLine = aState.NewLineBox(aFrame, pushCount, PR_FALSE);
    if (!newLine) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    mLines.after_insert(aLine, newLine);
    aLine->SetChildCount(aLine->GetChildCount() - pushCount);
#ifdef DEBUG
    if (gReallyNoisyReflow) {
      newLine->List(aState.mPresContext, stdout, gNoiseIndent+1);
    }
#endif

    // Let line layout know that some frames are no longer part of its
    // state.
    aLineLayout.SplitLineTo(aLine->GetChildCount());
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
  nsBlockFrame* nextInFlow = (nsBlockFrame*) mNextInFlow;
  while (nsnull != nextInFlow) {
    for (line_iterator line = nextInFlow->begin_lines(),
                   line_end = nextInFlow->end_lines();
         line != line_end;
         ++line)
    {
      if (0 != line->GetChildCount())
        return !line->IsBlock();
    }
    nextInFlow = (nsBlockFrame*) nextInFlow->mNextInFlow;
  }

  // This is the last line - so don't allow justification
  return PR_FALSE;
}

nsresult
nsBlockFrame::PlaceLine(nsBlockReflowState& aState,
                        nsLineLayout& aLineLayout,
                        line_iterator aLine,
                        PRBool* aKeepReflowGoing,
                        PRBool aUpdateMaximumWidth)
{
  nsresult rv = NS_OK;

  // Trim extra white-space from the line before placing the frames
  aLineLayout.TrimTrailingWhiteSpace();

  // Vertically align the frames on this line.
  //
  // According to the CSS2 spec, section 12.6.1, the "marker" box
  // participates in the height calculation of the list-item box's
  // first line box.
  //
  // There are exactly two places a bullet can be placed: near the
  // first or second line. Its only placed on the second line in a
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
    nsHTMLReflowMetrics metrics(nsnull);
    ReflowBullet(aState, metrics);
    aLineLayout.AddBulletFrame(mBullet, metrics);
    addedBullet = PR_TRUE;
  }
  nsSize maxElementSize;
  nscoord lineAscent;
  aLineLayout.VerticalAlignLine(aLine, maxElementSize, lineAscent);
  // Our ascent is the ascent of our first line
  if (aLine == mLines.front()) {
    mAscent = lineAscent;
  }

  // See if we're shrink wrapping the width
  if (aState.GetFlag(BRS_SHRINKWRAPWIDTH)) {
    // When determining the line's width we also need to include any
    // right floaters that impact us. This represents the shrink wrap
    // width of the line
    if (aState.IsImpactedByFloater() && !aLine->IsLineWrapped()) {
      NS_ASSERTION(aState.mContentArea.width >= aState.mAvailSpaceRect.XMost(), "bad state");
      aLine->mBounds.width += aState.mContentArea.width - aState.mAvailSpaceRect.XMost();
    }
  }
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
  const nsStyleText* styleText = (const nsStyleText*)
    mStyleContext->GetStyleData(eStyleStruct_Text);
  PRBool allowJustify = NS_STYLE_TEXT_ALIGN_JUSTIFY == styleText->mTextAlign
    && !aLineLayout.GetLineEndsInBR() && ShouldJustifyLine(aState, aLine);
#ifdef IBMBIDI
  nsRect bounds(aLine->mBounds);

  if (mRect.x) {
    const nsStyleVisibility* vis;
    GetStyleData(eStyleStruct_Visibility, (const nsStyleStruct*&)vis);

    if (NS_STYLE_DIRECTION_RTL == vis->mDirection) {
      bounds.x += mRect.x;
    }
  }
  PRBool successful = aLineLayout.HorizontalAlignFrames(bounds, allowJustify,
    aState.GetFlag(BRS_SHRINKWRAPWIDTH));
  // XXX: not only bidi: right alignment can be broken after RelativePositionFrames!!!
  // XXXldb Is something here considering relatively positioned frames at
  // other than their original positions?
#else
  PRBool successful = aLineLayout.HorizontalAlignFrames(aLine->mBounds, allowJustify,
                                                 aState.GetFlag(BRS_SHRINKWRAPWIDTH));
#endif // IBMBIDI
  if (!successful) {
    // Mark the line dirty and then later once we've determined the width
    // we can do the horizontal alignment
    aLine->MarkDirty();
    aState.SetFlag(BRS_NEEDRESIZEREFLOW, PR_TRUE);
  }
#ifdef IBMBIDI
  // XXXldb Why don't we do this earlier?
  else {
    PRBool bidiEnabled;
    aState.mPresContext->GetBidiEnabled(&bidiEnabled);

    if (bidiEnabled) {
      PRBool isVisual;
      aState.mPresContext->IsVisualMode(isVisual);
      if (!isVisual) {
        nsBidiPresUtils* bidiUtils;
        aState.mPresContext->GetBidiUtils(&bidiUtils);

        if (bidiUtils && bidiUtils->IsSuccessful() ) {
          nsIFrame* nextInFlow = (aLine.next() != end_lines())
                                 ? aLine.next()->mFirstChild : nsnull;

          bidiUtils->ReorderFrames(aState.mPresContext,
                                   aState.mReflowState.rendContext,
                                   aLine->mFirstChild, nextInFlow,
                                   aLine->GetChildCount() );
        } // bidiUtils
      } // not visual mode
    } // bidi enabled
  } // successful
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
  if (aLine->mBounds.height > 0) {
    // This line has some height. Therefore the application of the
    // previous-bottom-margin should stick.
    aState.mPrevBottomMargin.Zero();
    newY = aLine->mBounds.YMost();
  }
  else {
    // Don't let the previous-bottom-margin value affect the newY
    // coordinate (it was applied in ReflowInlineFrames speculatively)
    // since the line is empty.
    nscoord dy = -aState.mPrevBottomMargin.get();
    newY = aState.mY + dy;
    aLine->SlideBy(dy);
    // keep our ascent in sync
    if (mLines.front() == aLine) {
      mAscent += dy;
    }
  }

  // See if the line fit. If it doesn't we need to push it. Our first
  // line will always fit.
  if ((mLines.front() != aLine) && (newY > aState.mBottomEdge)) {
    // Push this line and all of it's children and anything else that
    // follows to our next-in-flow
    NS_ASSERTION((aState.mCurrentLine == aLine), "oops");
    PushLines(aState, aLine.prev());

    // Stop reflow and whack the reflow status if reflow hasn't
    // already been stopped.
    if (*aKeepReflowGoing) {
      NS_ASSERTION(NS_FRAME_COMPLETE == aState.mReflowStatus, 
                   "lost reflow status");
      aState.mReflowStatus = NS_FRAME_NOT_COMPLETE;
      *aKeepReflowGoing = PR_FALSE;
    }
    return rv;
  }

  aState.mY = newY;
  if (aState.GetFlag(BRS_COMPUTEMAXELEMENTSIZE)) {
#ifdef NOISY_MAX_ELEMENT_SIZE
    IndentBy(stdout, GetDepth());
    if (NS_UNCONSTRAINEDSIZE == aState.mReflowState.availableWidth) {
      printf("PASS1 ");
    }
    ListTag(stdout);
    printf(": line.floaters=%s band.floaterCount=%d\n",
           //aLine->mFloaters.NotEmpty() ? "yes" : "no",
           aState.mHaveRightFloaters ? "(have right floaters)" : "",
           aState.mBand.GetFloaterCount());
#endif
    if (0 != aState.mBand.GetFloaterCount()) {
      // Add in floater impacts to the lines max-element-size
      ComputeLineMaxElementSize(aState, aLine, &maxElementSize);
    }
  }
  
  // If we're reflowing the line just to incrementally update the
  // maximum width, then don't post-place the line. It's doing work we
  // don't need, and it will update things like aState.mKidXMost that
  // we don't want updated...
  if (aUpdateMaximumWidth) {
    // However, we do need to update the max-element-size if requested
    if (aState.GetFlag(BRS_COMPUTEMAXELEMENTSIZE)) {
      aState.UpdateMaxElementSize(maxElementSize);
      // We also cache the max element width in the line. This is needed for
      // incremental reflow
      aLine->mMaxElementWidth = maxElementSize.width;
#ifdef NOISY_MAX_ELEMENT_SIZE
      printf ("nsBlockFrame::PlaceLine: %p setting MES for line %p to %d\n", 
               this, aLine, maxElementSize.width);
#endif
    }

  } else {
    PostPlaceLine(aState, aLine, maxElementSize);
  }

  // Add the already placed current-line floaters to the line
  aLine->AppendFloaters(aState.mCurrentLineFloaters);

  // Any below current line floaters to place?
  if (aState.mBelowCurrentLineFloaters.NotEmpty()) {
    // Reflow the below-current-line floaters, then add them to the
    // lines floater list.
    aState.PlaceBelowCurrentLineFloaters(aState.mBelowCurrentLineFloaters);
    aLine->AppendFloaters(aState.mBelowCurrentLineFloaters);
  }

  // When a line has floaters, factor them into the combined-area
  // computations.
  if (aLine->HasFloaters()) {
    // Combine the floater combined area (stored in aState) and the
    // value computed by the line layout code.
    nsRect lineCombinedArea;
    aLine->GetCombinedArea(&lineCombinedArea);
#ifdef NOISY_COMBINED_AREA
    ListTag(stdout);
    printf(": lineCA=%d,%d,%d,%d floaterCA=%d,%d,%d,%d\n",
           lineCombinedArea.x, lineCombinedArea.y,
           lineCombinedArea.width, lineCombinedArea.height,
           aState.mFloaterCombinedArea.x, aState.mFloaterCombinedArea.y,
           aState.mFloaterCombinedArea.width,
           aState.mFloaterCombinedArea.height);
#endif
    CombineRects(aState.mFloaterCombinedArea, lineCombinedArea);

    if (aState.mHaveRightFloaters &&
        (aState.GetFlag(BRS_UNCONSTRAINEDWIDTH) || aState.GetFlag(BRS_SHRINKWRAPWIDTH))) {
      // We are reflowing in an unconstrained situation or shrink wrapping and
      // have some right floaters. They were placed at the infinite right edge
      // which will cause the combined area to be unusable.
      //
      // To solve this issue, we pretend that the right floaters ended up just
      // past the end of the line. Note that the right floater combined area
      // we computed as we were going will have as its X coordinate the left
      // most edge of all the right floaters. Therefore, to accomplish our goal
      // all we do is set that X value to the lines XMost value.
#ifdef NOISY_COMBINED_AREA
      printf("  ==> rightFloaterCA=%d,%d,%d,%d lineXMost=%d\n",
             aState.mRightFloaterCombinedArea.x,
             aState.mRightFloaterCombinedArea.y,
             aState.mRightFloaterCombinedArea.width,
             aState.mRightFloaterCombinedArea.height,
             aLine->mBounds.XMost());
#endif
      aState.mRightFloaterCombinedArea.x = aLine->mBounds.XMost();
      CombineRects(aState.mRightFloaterCombinedArea, lineCombinedArea);

      if (aState.GetFlag(BRS_SHRINKWRAPWIDTH)) {
        // Mark the line dirty so we come back and re-place the floater once
        // the shrink wrap width is determined
        aLine->MarkDirty();
        aState.SetFlag(BRS_NEEDRESIZEREFLOW, PR_TRUE);
      }
    }
    aLine->SetCombinedArea(lineCombinedArea);
#ifdef NOISY_COMBINED_AREA
    printf("  ==> final lineCA=%d,%d,%d,%d\n",
           lineCombinedArea.x, lineCombinedArea.y,
           lineCombinedArea.width, lineCombinedArea.height);
#endif
    aState.mHaveRightFloaters = PR_FALSE;
  }

  // Apply break-after clearing if necessary
  PRUint8 breakType = aLine->GetBreakType();
  switch (breakType) {
  case NS_STYLE_CLEAR_LEFT:
  case NS_STYLE_CLEAR_RIGHT:
  case NS_STYLE_CLEAR_LEFT_AND_RIGHT:
    aState.ClearFloaters(aState.mY, breakType);
    break;
  }

  return rv;
}

// Compute the line's max-element-size by adding into the raw value
// computed by reflowing the contents of the line (aMaxElementSize)
// the impact of floaters on this line or the preceeding lines.
void
nsBlockFrame::ComputeLineMaxElementSize(nsBlockReflowState& aState,
                                        nsLineBox* aLine,
                                        nsSize* aMaxElementSize)
{
  nscoord maxWidth, maxHeight;
  aState.mBand.GetMaxElementSize(aState.mPresContext, &maxWidth, &maxHeight);
#ifdef NOISY_MAX_ELEMENT_SIZE
  IndentBy(stdout, GetDepth());
  if (NS_UNCONSTRAINEDSIZE == aState.mReflowState.availableWidth) {
    printf("PASS1 ");
  }
  ListTag(stdout);
  printf(": maxFloaterSize=%d,%d\n", maxWidth, maxHeight);
#endif

  // To ensure that we always place some content next to a floater,
  // _add_ the max floater width to our line's max element size.
  aMaxElementSize->width += maxWidth;

  // Only update the max-element-size's height value if the floater is
  // part of the current line.
  if (aLine->HasFloaters()) {
    // If the maximum-height of the tallest floater is larger than the
    // maximum-height of the content then update the max-element-size
    // height
    if (maxHeight > aMaxElementSize->height) {
      aMaxElementSize->height = maxHeight;
    }
  }
#ifdef NOISY_MAX_ELEMENT_SIZE
  printf ("nsBlockFrame::ComputeLineMaxElementSize: %p returning MES %d\n", 
           this, aMaxElementSize->width);
#endif
}

void
nsBlockFrame::PostPlaceLine(nsBlockReflowState& aState,
                            nsLineBox* aLine,
                            const nsSize& aMaxElementSize)
{
  // If it's inline elements, then make sure the views are correctly
  // positioned and sized
  if (aLine->IsInline()) {
    nsIFrame* frame = aLine->mFirstChild;
    for (PRInt32 i = 0; i < aLine->GetChildCount(); i++) {
      ::PlaceFrameView(aState.mPresContext, frame);
      frame->GetNextSibling(&frame);
    }
  }

  // Update max-element-size
  if (aState.GetFlag(BRS_COMPUTEMAXELEMENTSIZE)) {
    aState.UpdateMaxElementSize(aMaxElementSize);
    // We also cache the max element width in the line. This is needed for
    // incremental reflow
    aLine->mMaxElementWidth = aMaxElementSize.width;
#ifdef NOISY_MAX_ELEMENT_SIZE
    printf ("nsBlockFrame::PostPlaceLine: %p setting line %p MES %d\n", 
               this, aLine, aMaxElementSize.width);
#endif
  }

  // If this is an unconstrained reflow, then cache the line width in the
  // line. We'll need this during incremental reflow if we're asked to
  // calculate the maximum width
  if (aState.GetFlag(BRS_UNCONSTRAINEDWIDTH)) {
#ifdef NOISY_MAXIMUM_WIDTH
    printf("nsBlockFrame::PostPlaceLine during UC Reflow of block %p line %p caching max width %d\n", 
           this, aLine, aLine->mBounds.XMost());
#endif
    aLine->mMaximumWidth = aLine->mBounds.XMost();
  }

  // Update xmost
  nscoord xmost = aLine->mBounds.XMost();

#ifdef DEBUG
  if (CRAZY_WIDTH(xmost)) {
    ListTag(stdout);
    printf(": line=%p xmost=%d\n", NS_STATIC_CAST(void*, aLine), xmost);
  }
#endif
  if (xmost > aState.mKidXMost) {
    aState.mKidXMost = xmost;
#ifdef NOISY_KIDXMOST
    printf("%p PostPlaceLine aState.mKidXMost=%d\n", this, aState.mKidXMost); 
#endif
  }
}

void
nsBlockFrame::PushLines(nsBlockReflowState& aState,
                        nsLineList::iterator aLineBefore)
{
  nsLineList::iterator overBegin(aLineBefore.next());
  NS_ASSERTION(overBegin != begin_lines(), "bad push");

  if (overBegin != end_lines()) {
    // XXXldb use presshell arena!
    nsLineList* overflowLines = new nsLineList();
    overflowLines->splice(overflowLines->end(), mLines, overBegin,
                          end_lines());
    NS_ASSERTION(!overflowLines->empty(), "should not be empty");
      // this takes ownership but it won't delete it immediately so we
      // can keep using it.
    SetOverflowLines(aState.mPresContext, overflowLines);
  
    // Mark all the overflow lines dirty so that they get reflowed when
    // they are pulled up by our next-in-flow.

    // XXXldb Can this get called O(N) times making the whole thing O(N^2)?
    for (line_iterator line = overflowLines->begin(),
                   line_end = overflowLines->end();
         line != line_end;
         ++line)
    {
      line->MarkDirty();
    }
  }

  // Break frame sibling list
  aLineBefore->LastChild()->SetNextSibling(nsnull);

#ifdef DEBUG
  VerifyOverflowSituation(aState.mPresContext);
#endif
}


// The overflowLines property is stored as a pointer to a line list,
// which must be deleted.  However, the following functions all maintain
// the invariant that the property is never set if the list is empty.

PRBool
nsBlockFrame::DrainOverflowLines(nsIPresContext* aPresContext)
{
#ifdef DEBUG
  VerifyOverflowSituation(aPresContext);
#endif
  PRBool drained = PR_FALSE;
  nsLineList* overflowLines;

  // First grab the prev-in-flows overflow lines
  nsBlockFrame* prevBlock = (nsBlockFrame*) mPrevInFlow;
  if (nsnull != prevBlock) {
    overflowLines = prevBlock->GetOverflowLines(aPresContext, PR_TRUE);
    if (nsnull != overflowLines) {
      NS_ASSERTION(! overflowLines->empty(),
                   "overflow lines should never be set and empty");
      drained = PR_TRUE;

      // Make all the frames on the overflow line list mine
      nsIFrame* lastFrame = nsnull;
      nsIFrame* frame = overflowLines->front()->mFirstChild;
      while (nsnull != frame) {
        frame->SetParent(this);

        // When pushing and pulling frames we need to check for whether any
        // views need to be reparented
        nsHTMLContainerFrame::ReparentFrameView(aPresContext, frame, prevBlock, this);

        // If the frame we are looking at is a placeholder for a floater, we
        // need to reparent both it's out-of-flow frame and any views it has.
        //
        // Note: A floating table (example: style="position: relative; float: right")
        //       is an example of an out-of-flow frame with a view

        nsCOMPtr<nsIAtom> frameType;
        frame->GetFrameType(getter_AddRefs(frameType));

        if (nsLayoutAtoms::placeholderFrame == frameType.get()) {
          nsIFrame *outOfFlowFrame = NS_STATIC_CAST(nsPlaceholderFrame*, frame)->GetOutOfFlowFrame();
          if (outOfFlowFrame) {
            const nsStyleDisplay*  display = nsnull;
            outOfFlowFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display);
            if (display && !display->IsAbsolutelyPositioned()) {
              // It's not an absolute or fixed positioned frame, so it
              // must be a floater!
              outOfFlowFrame->SetParent(this);
              nsHTMLContainerFrame::ReparentFrameView(aPresContext, outOfFlowFrame, prevBlock, this);
            }
          }
        }

        // Get the next frame
        lastFrame = frame;
        frame->GetNextSibling(&frame);
      }

      // Join the line lists
      NS_ASSERTION(lastFrame, "overflow list was created with no frames");
      if (! mLines.empty()) {
        // Join the sibling lists together
        lastFrame->SetNextSibling(mLines.front()->mFirstChild);
      }
      // Place overflow lines at the front of our line list
      mLines.splice(mLines.begin(), *overflowLines);
      NS_ASSERTION(overflowLines->empty(), "splice should empty list");
      delete overflowLines;
    }
  }

  // Now grab our own overflow lines
  overflowLines = GetOverflowLines(aPresContext, PR_TRUE);
  if (overflowLines) {
    NS_ASSERTION(! overflowLines->empty(),
                 "overflow lines should never be set and empty");
    // This can happen when we reflow and not everything fits and then
    // we are told to reflow again before a next-in-flow is created
    // and reflows.

    if (! mLines.empty()) {
      mLines.back()->LastChild()->SetNextSibling(
          overflowLines->front()->mFirstChild );
    }
    // append the overflow to mLines
    mLines.splice(mLines.end(), *overflowLines);
    drained = PR_TRUE;
    delete overflowLines;
  }
  return drained;
}

nsLineList*
nsBlockFrame::GetOverflowLines(nsIPresContext* aPresContext,
                               PRBool          aRemoveProperty) const
{
  nsCOMPtr<nsIPresShell>     presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));

  if (presShell) {
    nsCOMPtr<nsIFrameManager>  frameManager;
    presShell->GetFrameManager(getter_AddRefs(frameManager));
  
    if (frameManager) {
      PRUint32  options = 0;
      nsLineList* value;
  
      if (aRemoveProperty) {
        options |= NS_IFRAME_MGR_REMOVE_PROP;
      }
      frameManager->GetFrameProperty(NS_CONST_CAST(nsBlockFrame*, this),
                                     nsLayoutAtoms::overflowLinesProperty,
                                     options,
                                     NS_REINTERPRET_CAST(void**, &value));
      NS_ASSERTION(!value || !value->empty(),
                   "value should never be stored as empty");
      return value;
    }
  }

  return nsnull;
}

// Destructor function for the overflowLines frame property
static void
DestroyOverflowLines(nsIPresContext* aPresContext,
                     nsIFrame*       aFrame,
                     nsIAtom*        aPropertyName,
                     void*           aPropertyValue)
{
  if (aPropertyValue) {
    nsLineList* lines = NS_STATIC_CAST(nsLineList*, aPropertyValue);
    nsLineBox::DeleteLineList(aPresContext, *lines);
    delete lines;
  }
}

// This takes ownership of aOverflowLines.
// XXX We should allocate overflowLines from presShell arena!
nsresult
nsBlockFrame::SetOverflowLines(nsIPresContext* aPresContext,
                               nsLineList*     aOverflowLines)
{
  nsCOMPtr<nsIPresShell>     presShell;
  nsresult                   rv = NS_ERROR_FAILURE;

  NS_ASSERTION(aOverflowLines, "null lines");
  NS_ASSERTION(! aOverflowLines->empty(), "empty lines");

  aPresContext->GetShell(getter_AddRefs(presShell));
  if (presShell) {
    nsCOMPtr<nsIFrameManager>  frameManager;
    presShell->GetFrameManager(getter_AddRefs(frameManager));
  
    if (frameManager) {
      rv = frameManager->SetFrameProperty(this,
                                          nsLayoutAtoms::overflowLinesProperty,
                                          aOverflowLines,
                                          DestroyOverflowLines);

      // Verify that we didn't overwrite an existing overflow list
      NS_ASSERTION(rv != NS_IFRAME_MGR_PROP_OVERWRITTEN, "existing overflow list");
    }
  }
  return rv;
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
nsBlockFrame::AppendFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList)
{
  if (nsnull == aFrameList) {
    return NS_OK;
  }
  if (nsLayoutAtoms::absoluteList == aListName) {
    return mAbsoluteContainer.AppendFrames(this, aPresContext, aPresShell, aListName,
                                           aFrameList);
  }
  else if (nsLayoutAtoms::floaterList == aListName) {
    // XXX we don't *really* care about this right now because we are
    // BuildFloaterList ing still
    mFloaters.AppendFrames(nsnull, aFrameList);
    return NS_OK;
  }
  else if (nsnull != aListName) {
    return NS_ERROR_INVALID_ARG;
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
  nsresult rv = AddFrames(aPresContext, aFrameList, lastKid);
  if (NS_SUCCEEDED(rv)) {
    // Ask the parent frame to reflow me.
    ReflowDirtyChild(&aPresShell, nsnull);
  }
  return rv;
}

NS_IMETHODIMP
nsBlockFrame::InsertFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aPrevFrame,
                           nsIFrame*       aFrameList)
{
  if (nsLayoutAtoms::absoluteList == aListName) {
    return mAbsoluteContainer.InsertFrames(this, aPresContext, aPresShell, aListName,
                                           aPrevFrame, aFrameList);
  }
  else if (nsLayoutAtoms::floaterList == aListName) {
    // XXX we don't *really* care about this right now because we are
    // BuildFloaterList'ing still
    mFloaters.AppendFrames(nsnull, aFrameList);
    return NS_OK;
  }
#ifdef IBMBIDI
  else if (nsLayoutAtoms::nextBidi == aListName) {}
#endif // IBMBIDI
  else if (nsnull != aListName) {
    return NS_ERROR_INVALID_ARG;
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
  nsresult rv = AddFrames(aPresContext, aFrameList, aPrevFrame);
#ifdef IBMBIDI
  if (aListName != nsLayoutAtoms::nextBidi)
#endif // IBMBIDI
  if (NS_SUCCEEDED(rv)) {
    // Ask the parent frame to reflow me.
    ReflowDirtyChild(&aPresShell, nsnull);
  }
  return rv;
}

nsresult
nsBlockFrame::AddFrames(nsIPresContext* aPresContext,
                        nsIFrame* aFrameList,
                        nsIFrame* aPrevSibling)
{
  if (nsnull == aFrameList) {
    return NS_OK;
  }

  nsCOMPtr<nsIPresShell> presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));

  // Attempt to find the line that contains the previous sibling
  nsLineList::iterator prevSibLine = end_lines();
  PRInt32 prevSiblingIndex = -1;
  if (aPrevSibling) {
    // XXX_perf This is technically O(N^2) in some cases, but by using
    // RFind instead of Find, we make it O(N) in the most common case,
    // which is appending cotent.

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
    aPrevSibling->GetNextSibling(&prevSiblingNextFrame);

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

    // If the frame is a block frame, or if there is no previous line
    // or if the previous line is a block line then make a new line.
    if (isBlock || prevSibLine == end_lines() || prevSibLine->IsBlock()) {
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
    newFrame->GetNextSibling(&newFrame);
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


void
nsBlockFrame::FixParentAndView(nsIPresContext* aPresContext, nsIFrame* aFrame)
{
  while (aFrame) {
    nsIFrame* oldParent;
    aFrame->GetParent(&oldParent);
    aFrame->SetParent(this);
    if (this != oldParent) {
      nsHTMLContainerFrame::ReparentFrameView(aPresContext, aFrame, oldParent, this);
      aPresContext->ReParentStyleContext(aFrame, mStyleContext);
    }
    aFrame->GetNextSibling(&aFrame);
  }
}

NS_IMETHODIMP
nsBlockFrame::RemoveFrame(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aOldFrame)
{
  nsresult rv = NS_OK;

#ifdef NOISY_REFLOW_REASON
    ListTag(stdout);
    printf(": remove ");
    nsFrame::ListTag(stdout, aOldFrame);
    printf("\n");
#endif

  if (nsLayoutAtoms::absoluteList == aListName) {
      return mAbsoluteContainer.RemoveFrame(this, aPresContext, aPresShell, aListName, aOldFrame);
  }
  else if (nsLayoutAtoms::floaterList == aListName) {
    // Remove floater from the floater list first
    mFloaters.RemoveFrame(aOldFrame);

    // Find which line contains the floater
    line_iterator line = begin_lines(), line_end = end_lines();
    for ( ; line != line_end; ++line) {
      if (line->IsInline() && line->RemoveFloater(aOldFrame)) {
        aOldFrame->Destroy(aPresContext);
        break;
      }
    }

    // Mark every line at and below the line where the floater was dirty
    // XXXldb This could be done more efficiently.
    for ( ; line != line_end; ++line) {
      line->MarkDirty();
    }
  }
#ifdef IBMBIDI
  else if (nsLayoutAtoms::nextBidi == aListName) {
    rv = DoRemoveFrame(aPresContext, aOldFrame);
  }
#endif // IBMBIDI
  else if (nsnull != aListName) {
    rv = NS_ERROR_INVALID_ARG;
  }
  else {
    rv = DoRemoveFrame(aPresContext, aOldFrame);
  }

#ifdef IBMBIDI
  if (nsLayoutAtoms::nextBidi != aListName)
#endif // IBMBIDI
  if (NS_SUCCEEDED(rv)) {
    // Ask the parent frame to reflow me.
    ReflowDirtyChild(&aPresShell, nsnull);  
  }
  return rv;
}

nsresult
nsBlockFrame::DoRemoveFrame(nsIPresContext* aPresContext,
                            nsIFrame* aDeletedFrame)
{
  nsCOMPtr<nsIPresShell> presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));
  
  // Find the line and the previous sibling that contains
  // deletedFrame; we also find the pointer to the line.
  nsBlockFrame* flow = this;
  nsLineList& lines = flow->mLines;
  nsLineList::iterator line = lines.begin(),
                       line_end = lines.end();
  nsIFrame* prevSibling = nsnull;
  for ( ; line != line_end; ++line) {
    nsIFrame* frame = line->mFirstChild;
    PRInt32 n = line->GetChildCount();
    while (--n >= 0) {
      if (frame == aDeletedFrame) {
        goto found_frame;
      }
      prevSibling = frame;
      frame->GetNextSibling(&frame);
    }
  }
 found_frame:;
#ifdef NS_DEBUG
  NS_ASSERTION(line != line_end, "can't find deleted frame in lines");
  if (nsnull != prevSibling) {
    nsIFrame* tmp;
    prevSibling->GetNextSibling(&tmp);
    NS_ASSERTION(tmp == aDeletedFrame, "bad prevSibling");
  }
#endif

  // Remove frame and all of its continuations
  while (nsnull != aDeletedFrame) {
    while ((line != line_end) && (nsnull != aDeletedFrame)) {
#ifdef NS_DEBUG
      nsIFrame* parent;
      aDeletedFrame->GetParent(&parent);
      NS_ASSERTION(flow == parent, "messed up delete code");
      NS_ASSERTION(line->Contains(aDeletedFrame), "frame not in line");
#endif

      // See if the frame being deleted is the last one on the line
      PRBool isLastFrameOnLine = PR_FALSE;
      if (1 == line->GetChildCount()) {
        isLastFrameOnLine = PR_TRUE;
      }
      else if (line->LastChild() == aDeletedFrame) {
        isLastFrameOnLine = PR_TRUE;
      }

      // Remove aDeletedFrame from the line
      nsIFrame* nextFrame;
      aDeletedFrame->GetNextSibling(&nextFrame);
      if (line->mFirstChild == aDeletedFrame) {
        line->mFirstChild = nextFrame;
      }

      --line;
      if (line != line_end && !line->IsBlock()) {
        // Since we just removed a frame that follows some inline
        // frames, we need to reflow the previous line.
        line->MarkDirty();
      }
      ++line;

      // Take aDeletedFrame out of the sibling list. Note that
      // prevSibling will only be nsnull when we are deleting the very
      // first frame.
      if (prevSibling) {
        prevSibling->SetNextSibling(nextFrame);
      }

      // Destroy frame; capture its next-in-flow first in case we need
      // to destroy that too.
      nsIFrame* nextInFlow;
      aDeletedFrame->GetNextInFlow(&nextInFlow);
      nsSplittableType st;
      aDeletedFrame->IsSplittable(st);
      if (NS_FRAME_NOT_SPLITTABLE != st) {
        nsSplittableFrame::RemoveFromFlow(aDeletedFrame);
      }
#ifdef NOISY_REMOVE_FRAME
      printf("DoRemoveFrame: line=%p frame=", line);
      nsFrame::ListTag(stdout, aDeletedFrame);
      printf(" prevSibling=%p nextInFlow=%p\n", prevSibling, nextInFlow);
#endif
      aDeletedFrame->Destroy(aPresContext);
      aDeletedFrame = nextInFlow;

      // If line is empty, remove it now
      PRInt32 lineChildCount = line->GetChildCount();
      if (0 == --lineChildCount) {
        nsLineBox *cur = line;
        line = lines.erase(line);
        // Invalidate the space taken up by the line.
        // XXX We need to do this if we're removing a frame as a result of
        // a call to RemoveFrame(), but we may not need to do this in all
        // cases...
        nsRect lineCombinedArea;
        cur->GetCombinedArea(&lineCombinedArea);
#ifdef NOISY_BLOCK_INVALIDATE
        printf("%p invalidate 10 (%d, %d, %d, %d)\n",
               this, lineCombinedArea.x, lineCombinedArea.y, lineCombinedArea.width, lineCombinedArea.height);
#endif
        Invalidate(aPresContext, lineCombinedArea);
        cur->Destroy(presShell);

        // If we're removing a line, ReflowDirtyLines isn't going to
        // know that it needs to slide lines unless something is marked
        // dirty.  So mark the previous margin of the next line dirty if
        // there is one.
        if (line != line_end)
          line->MarkPreviousMarginDirty();
      }
      else {
        // Make the line that just lost a frame dirty
        line->SetChildCount(lineChildCount);
        line->MarkDirty();

        // If we just removed the last frame on the line then we need
        // to advance to the next line.
        if (isLastFrameOnLine) {
          ++line;
        }
      }

      // See if we should keep looking in the current flow's line list.
      if (nsnull != aDeletedFrame) {
        if (aDeletedFrame != nextFrame) {
          // The deceased frames continuation is not the next frame in
          // the current flow's frame list. Therefore we know that the
          // continuation is in a different parent. So break out of
          // the loop so that we advance to the next parent.
#ifdef NS_DEBUG
          nsIFrame* checkParent;
          aDeletedFrame->GetParent(&checkParent);
          NS_ASSERTION(checkParent != flow, "strange continuation");
#endif
          break;
        }
      }
    }

    // Advance to next flow block if the frame has more continuations
    if (flow && aDeletedFrame) {
      flow = (nsBlockFrame*) flow->mNextInFlow;
      NS_ASSERTION(nsnull != flow, "whoops, continuation without a parent");
      // add defensive pointer check for bug 56894
      if(flow) {
        lines = flow->mLines;
        line = lines.begin();
        line_end = lines.end();
        prevSibling = nsnull;
      }
    }
  }

#ifdef DEBUG
  VerifyLines(PR_TRUE);
#endif
  return NS_OK;
}

void
nsBlockFrame::DeleteChildsNextInFlow(nsIPresContext* aPresContext,
                                     nsIFrame* aChild)
{
  NS_PRECONDITION(IsChild(aPresContext, aChild), "bad geometric parent");
  nsIFrame* nextInFlow;
  aChild->GetNextInFlow(&nextInFlow);
  NS_PRECONDITION(nsnull != nextInFlow, "null next-in-flow");
#ifdef IBMBIDI
  nsIFrame* nextBidi;
  aChild->GetBidiProperty(aPresContext, nsLayoutAtoms::nextBidi,
                          (void**) &nextBidi,sizeof(nextBidi));
  if (nextBidi != nextInFlow) {
#endif // IBMBIDI
  nsBlockFrame* parent;
  nextInFlow->GetParent((nsIFrame**)&parent);
  NS_PRECONDITION(nsnull != parent, "next-in-flow with no parent");
  NS_PRECONDITION(! parent->mLines.empty(), "next-in-flow with weird parent");
//  NS_PRECONDITION(nsnull == parent->mOverflowLines, "parent with overflow");
  parent->DoRemoveFrame(aPresContext, nextInFlow);
#ifdef IBMBIDI
  }
#endif // IBMBIDI
}

////////////////////////////////////////////////////////////////////////
// Floater support

nsresult
nsBlockFrame::ReflowFloater(nsBlockReflowState& aState,
                            nsPlaceholderFrame* aPlaceholder,
                            nsRect& aCombinedRectResult,
                            nsMargin& aMarginResult,
                            nsMargin& aComputedOffsetsResult)
{
  // Reflow the floater.
  nsIFrame* floater = aPlaceholder->GetOutOfFlowFrame();

#ifdef NOISY_FLOATER
  printf("Reflow Floater %p in parent %p, availSpace(%d,%d,%d,%d)\n",
          aPlaceholder->GetOutOfFlowFrame(), this, 
          aState.mAvailSpaceRect.x, aState.mAvailSpaceRect.y, 
          aState.mAvailSpaceRect.width, aState.mAvailSpaceRect.height
  );
#endif

  // Compute the available width. By default, assume the width of the
  // containing block.
  nscoord availWidth = aState.GetFlag(BRS_UNCONSTRAINEDWIDTH)
                        ? NS_UNCONSTRAINEDSIZE
                        : aState.mContentArea.width;

  // If the floater's width is automatic, we can't let the floater's
  // width shrink below its maxElementSize.
  const nsStylePosition* position;
  floater->GetStyleData(eStyleStruct_Position,
                        NS_REINTERPRET_CAST(const nsStyleStruct*&, position));
  PRBool isAutoWidth = (eStyleUnit_Auto == position->mWidth.GetUnit());

  // We'll need to compute the max element size if either 1) we're
  // auto-width or 2) the state wanted us to compute it anyway.
  PRBool computeMaxElementSize =
    isAutoWidth || aState.GetFlag(BRS_COMPUTEMAXELEMENTSIZE);

  nsRect availSpace(aState.BorderPadding().left,
                    aState.BorderPadding().top,
                    availWidth, NS_UNCONSTRAINEDSIZE);

  // Setup a block reflow state to reflow the floater.
  nsBlockReflowContext brc(aState.mPresContext, aState.mReflowState,
                           computeMaxElementSize,
                           aState.GetFlag(BRS_COMPUTEMAXWIDTH));

  brc.SetNextRCFrame(aState.mNextRCFrame);

  // Reflow the floater
  PRBool isAdjacentWithTop = aState.IsAdjacentWithTop();

  nsReflowStatus frameReflowStatus;
  nsCollapsingMargin margin;
  nsresult rv = brc.ReflowBlock(floater, availSpace, PR_TRUE, margin,
                                isAdjacentWithTop,
                                aComputedOffsetsResult, frameReflowStatus);
  if (NS_SUCCEEDED(rv) && isAutoWidth) {
    nscoord maxElementWidth = brc.GetMaxElementSize().width;
    if (maxElementWidth > availSpace.width) {
      // The floater's maxElementSize is larger than the available
      // width. Reflow it again, this time pinning the width to the
      // maxElementSize.
      availSpace.width = maxElementWidth;
      nsCollapsingMargin marginMES;
      rv = brc.ReflowBlock(floater, availSpace, PR_TRUE, marginMES,
                           isAdjacentWithTop,
                           aComputedOffsetsResult, frameReflowStatus);
    }
  }

  if (brc.BlockShouldInvalidateItself()) {
    Invalidate(aState.mPresContext, mRect);
  }

  if (floater == aState.mNextRCFrame) {
    // Null out mNextRCFrame so if we reflow it again, we don't think
    // it's still an incremental reflow
    aState.mNextRCFrame = nsnull;
  }

  if (NS_FAILED(rv)) {
    return rv;
  }

  // Capture the margin information for the caller
  const nsMargin& m = brc.GetMargin();
  aMarginResult.top = brc.GetTopMargin();
  aMarginResult.right = m.right;
  brc.GetCarriedOutBottomMargin().Include(m.bottom);
  aMarginResult.bottom = brc.GetCarriedOutBottomMargin().get();
  aMarginResult.left = m.left;

  const nsHTMLReflowMetrics& metrics = brc.GetMetrics();
  aCombinedRectResult = metrics.mOverflowArea;

  // Set the rect, make sure the view is properly sized and positioned,
  // and tell the frame we're done reflowing it
  // XXXldb This seems like the wrong place to be doing this -- shouldn't
  // we be doing this in nsBlockReflowState::FlowAndPlaceFloater after
  // we've positioned the floater, and shouldn't we be doing the equivalent
  // of |::PlaceFrameView| here?
  floater->SizeTo(aState.mPresContext, metrics.width, metrics.height);
  nsIView*  view;
  floater->GetView(aState.mPresContext, &view);
  if (view) {
    nsContainerFrame::SyncFrameViewAfterReflow(aState.mPresContext, floater, view,
                                               &metrics.mOverflowArea,
                                               NS_FRAME_NO_MOVE_VIEW);
  }
  floater->DidReflow(aState.mPresContext, NS_FRAME_REFLOW_FINISHED);

  // If we computed it, then stash away the max-element-size for later
  if (computeMaxElementSize) {
    nsSize mes = brc.GetMaxElementSize();
    mes.SizeBy(aMarginResult.left + aMarginResult.right, 
               aMarginResult.top  + aMarginResult.bottom);
    aState.StoreMaxElementSize(floater, mes);
    aState.UpdateMaxElementSize(mes); // fix for bug 13553
  }
#ifdef NOISY_FLOATER
  printf("end ReflowFloater %p, sized to %d,%d\n", floater, metrics.width, metrics.height);
#endif
  return NS_OK;
}

//////////////////////////////////////////////////////////////////////
// Painting, event handling

PRIntn
nsBlockFrame::GetSkipSides() const
{
  PRIntn skip = 0;
  if (nsnull != mPrevInFlow) {
    skip |= 1 << NS_SIDE_TOP;
  }
  if (nsnull != mNextInFlow) {
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
    nsRect lineCombinedArea;
    line->GetCombinedArea(&lineCombinedArea);
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

NS_IMETHODIMP
nsBlockFrame::IsVisibleForPainting(nsIPresContext *     aPresContext, 
                                   nsIRenderingContext& aRenderingContext,
                                   PRBool               aCheckVis,
                                   PRBool*              aIsVisible)
{
  // first check to see if we are visible
  if (aCheckVis) {
    const nsStyleVisibility* vis = 
      (const nsStyleVisibility*)((nsIStyleContext*)mStyleContext)->GetStyleData(eStyleStruct_Visibility);
    if (!vis->IsVisible()) {
      *aIsVisible = PR_FALSE;
      return NS_OK;
    }
  }

  // Start by assuming we are visible and need to be painted
  *aIsVisible = PR_TRUE;

  // NOTE: GetSelectionforVisCheck checks the pagination to make sure we are printing
  // In otherwords, the selection will ALWAYS be null if we are not printing, meaning
  // the visibility will be TRUE in that case
  nsCOMPtr<nsISelection> selection;
  nsresult rv = GetSelectionForVisCheck(aPresContext, getter_AddRefs(selection));
  if (NS_SUCCEEDED(rv) && selection) {
    nsCOMPtr<nsIDOMNode> node(do_QueryInterface(mContent));

    nsCOMPtr<nsIDOMHTMLHtmlElement> html(do_QueryInterface(mContent));
    nsCOMPtr<nsIDOMHTMLBodyElement> body(do_QueryInterface(mContent));

    if (!html && !body) {
      rv = selection->ContainsNode(node, PR_TRUE, aIsVisible);
    }
  }

  return rv;
}

NS_IMETHODIMP
nsBlockFrame::Paint(nsIPresContext*      aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect&        aDirtyRect,
                    nsFramePaintLayer    aWhichLayer,
                    PRUint32             aFlags)
{
  if (NS_FRAME_IS_UNFLOWABLE & mState) {
    return NS_OK;
  }

#ifdef DEBUG
  if (gNoisyDamageRepair) {
    if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
      PRInt32 depth = GetDepth();
      nsRect ca;
      ComputeCombinedArea(mLines, mRect.width, mRect.height, ca);
      nsFrame::IndentBy(stdout, depth);
      ListTag(stdout);
      printf(": bounds=%d,%d,%d,%d dirty=%d,%d,%d,%d ca=%d,%d,%d,%d\n",
             mRect.x, mRect.y, mRect.width, mRect.height,
             aDirtyRect.x, aDirtyRect.y, aDirtyRect.width, aDirtyRect.height,
             ca.x, ca.y, ca.width, ca.height);
    }
  }
#endif  

  PRBool isVisible;
  if (NS_FAILED(IsVisibleForPainting(aPresContext, aRenderingContext, PR_TRUE, &isVisible))) {
    return NS_ERROR_FAILURE;
  }

  // Only paint the border and background if we're visible
  if (isVisible && (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) &&
      (0 != mRect.width) && (0 != mRect.height)) {
    PRIntn skipSides = GetSkipSides();
    const nsStyleBackground* color = (const nsStyleBackground*)
      mStyleContext->GetStyleData(eStyleStruct_Background);
    const nsStyleBorder* border = (const nsStyleBorder*)
      mStyleContext->GetStyleData(eStyleStruct_Border);
    const nsStyleOutline* outline = (const nsStyleOutline*)
      mStyleContext->GetStyleData(eStyleStruct_Outline);

    // Paint background, border and outline
    nsRect rect(0, 0, mRect.width, mRect.height);
    nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                    aDirtyRect, rect, *color, *border, 0, 0);
    nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                aDirtyRect, rect, *border, mStyleContext,
                                skipSides);
    nsCSSRendering::PaintOutline(aPresContext, aRenderingContext, this,
                                 aDirtyRect, rect, *border, *outline, mStyleContext, 0);
  }

  PRBool paintingSuppressed = PR_FALSE;  
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  shell->IsPaintingSuppressed(&paintingSuppressed);
  if (paintingSuppressed)
    return NS_OK;

  const nsStyleDisplay* disp = (const nsStyleDisplay*)
    mStyleContext->GetStyleData(eStyleStruct_Display);

  // If overflow is hidden then set the clip rect so that children don't
  // leak out of us. Note that because overflow'-clip' only applies to
  // the content area we do this after painting the border and background
  if (NS_STYLE_OVERFLOW_HIDDEN == disp->mOverflow) {
    aRenderingContext.PushState();
    SetOverflowClipRect(aRenderingContext);
  }

  // Child elements have the opportunity to override the visibility
  // property and display even if the parent is hidden
  if (NS_FRAME_PAINT_LAYER_FLOATERS == aWhichLayer) {
    PaintFloaters(aPresContext, aRenderingContext, aDirtyRect);
  }
  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);

  if (NS_STYLE_OVERFLOW_HIDDEN == disp->mOverflow) {
    PRBool clipState;
    aRenderingContext.PopState(clipState);
  }

#if 0
  if ((NS_FRAME_PAINT_LAYER_DEBUG == aWhichLayer) && GetShowFrameBorders()) {
    // Render the bands in the spacemanager
    nsISpaceManager* sm = mSpaceManager;

    if (nsnull != sm) {
      nsBlockBandData band;
      band.Init(sm, nsSize(mRect.width, mRect.height));
      nscoord y = 0;
      while (y < mRect.height) {
        nsRect availArea;
        band.GetAvailableSpace(y, availArea);
  
        // Render a box and a diagonal line through the band
        aRenderingContext.SetColor(NS_RGB(0,255,0));
        aRenderingContext.DrawRect(0, availArea.y,
                                   mRect.width, availArea.height);
        aRenderingContext.DrawLine(0, availArea.y,
                                   mRect.width, availArea.YMost());
  
        // Render boxes and opposite diagonal lines around the
        // unavailable parts of the band.
        PRInt32 i;
        for (i = 0; i < band.GetTrapezoidCount(); i++) {
          const nsBandTrapezoid* trapezoid = band.GetTrapezoid(i);
          if (nsBandTrapezoid::Available != trapezoid->mState) {
            nsRect r;
            trapezoid->GetRect(r);
            if (nsBandTrapezoid::OccupiedMultiple == trapezoid->mState) {
              aRenderingContext.SetColor(NS_RGB(0,255,128));
            }
            else {
              aRenderingContext.SetColor(NS_RGB(128,255,0));
            }
            aRenderingContext.DrawRect(r);
            aRenderingContext.DrawLine(r.x, r.YMost(), r.XMost(), r.y);
          }
        }
        y = availArea.YMost();
      }
    }
  }
#endif

  return NS_OK;
}

void
nsBlockFrame::PaintFloaters(nsIPresContext* aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect)
{
  for (line_iterator line = begin_lines(), line_end = end_lines();
       line != line_end;
       ++line) {
    if (!line->HasFloaters()) {
      continue;
    }
    nsFloaterCache* fc = line->GetFirstFloater();
    while (fc) {
      nsIFrame* floater = fc->mPlaceholder->GetOutOfFlowFrame();
      PaintChild(aPresContext, aRenderingContext, aDirtyRect,
                 floater, NS_FRAME_PAINT_LAYER_BACKGROUND);
      PaintChild(aPresContext, aRenderingContext, aDirtyRect,
                 floater, NS_FRAME_PAINT_LAYER_FLOATERS);
      PaintChild(aPresContext, aRenderingContext, aDirtyRect,
                 floater, NS_FRAME_PAINT_LAYER_FOREGROUND);
      fc = fc->Next();
    }
  }
}

void
nsBlockFrame::PaintChildren(nsIPresContext*      aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect&        aDirtyRect,
                            nsFramePaintLayer    aWhichLayer,
                            PRUint32             aFlags)
{
#ifdef DEBUG
  PRInt32 depth = 0;
  if (gNoisyDamageRepair) {
    if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
      depth = GetDepth();
    }
  }
  PRTime start = LL_ZERO; // Initialize these variables to silence the compiler.
  PRInt32 drawnLines = 0; // They will only be used if set (gLamePaintMetrics).
  if (gLamePaintMetrics) {
    start = PR_Now();
    drawnLines = 0;
  }
#endif

  for (line_iterator line = begin_lines(), line_end = end_lines();
       line != line_end;
       ++line) {
    // If the line's combined area (which includes child frames that
    // stick outside of the line's bounding box or our bounding box)
    // intersects the dirty rect then paint the line.
    if (line->CombinedAreaIntersects(aDirtyRect)) {
#ifdef DEBUG
      if (gNoisyDamageRepair &&
          (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer)) {
        nsRect lineCombinedArea;
        line->GetCombinedArea(&lineCombinedArea);
        nsFrame::IndentBy(stdout, depth+1);
        printf("draw line=%p bounds=%d,%d,%d,%d ca=%d,%d,%d,%d\n",
               NS_STATIC_CAST(void*, line.get()),
               line->mBounds.x, line->mBounds.y,
               line->mBounds.width, line->mBounds.height,
               lineCombinedArea.x, lineCombinedArea.y,
               lineCombinedArea.width, lineCombinedArea.height);
      }
      if (gLamePaintMetrics) {
        drawnLines++;
      }
#endif
      nsIFrame* kid = line->mFirstChild;
      PRInt32 n = line->GetChildCount();
      while (--n >= 0) {
        PaintChild(aPresContext, aRenderingContext, aDirtyRect, kid,
                   aWhichLayer);
        kid->GetNextSibling(&kid);
      }
    }
#ifdef DEBUG
    else {
      if (gNoisyDamageRepair &&
          (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer)) {
        nsRect lineCombinedArea;
        line->GetCombinedArea(&lineCombinedArea);
        nsFrame::IndentBy(stdout, depth+1);
        printf("skip line=%p bounds=%d,%d,%d,%d ca=%d,%d,%d,%d\n",
               NS_STATIC_CAST(void*, line.get()),
               line->mBounds.x, line->mBounds.y,
               line->mBounds.width, line->mBounds.height,
               lineCombinedArea.x, lineCombinedArea.y,
               lineCombinedArea.width, lineCombinedArea.height);
      }
    }
#endif  
  }

  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
    if ((nsnull != mBullet) && HaveOutsideBullet()) {
      // Paint outside bullets manually
      PaintChild(aPresContext, aRenderingContext, aDirtyRect, mBullet,
                 aWhichLayer);
    }
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
}

// XXXldb Does this handle all overlap cases correctly?  (How?)
nsresult
nsBlockFrame::GetClosestLine(nsILineIterator *aLI, 
                             const nsPoint &aOrigin, 
                             const nsPoint &aPoint, 
                             PRInt32 &aClosestLine)
{
  if (!aLI)
    return NS_ERROR_NULL_POINTER;

  nsRect rect;
  PRInt32 numLines;
  PRInt32 lineFrameCount;
  nsIFrame *firstFrame;
  PRUint32 flags;

  nsresult result = aLI->GetNumLines(&numLines);

  if (NS_FAILED(result) || numLines < 0)
    return NS_OK;//do not handle

  PRInt32 shifted = numLines;
  PRInt32 start = 0, midpoint = 0;
  PRInt32 y = 0;

  while(shifted > 0)
  {
    // Cut the number of lines to look at in half and
    // calculate the midpoint of the region we are looking at.

    shifted >>= 1; //divide by 2
    midpoint  = start + shifted;

    // Get the dimensions of the line that is at the half
    // point of the region we are looking at.

    result = aLI->GetLine(midpoint, &firstFrame, &lineFrameCount,rect,&flags);
    if (NS_FAILED(result))
      break;//do not handle

    // Check to see if our point lies with the line's Y bounds.

    rect+=aOrigin; //offset origin to get comparative coordinates

    y = aPoint.y - rect.y;
    if (y >=0 && (aPoint.y < (rect.y+rect.height)))
    {
      aClosestLine = midpoint; //spot on!
      return NS_OK;
    }

    if (y > 0)
    {
      // If we get here, no match was found above, so aPoint.y must
      // be greater than the Y bounds of the current line rect. Move
      // our starting point just beyond the midpoint of the current region.

      start = midpoint;

      if (numLines > 1 && start < (numLines - 1))
        ++start;
      else
        shifted = 0;
    }
  }

  // Make sure we don't go off the edge in either direction!

  NS_ASSERTION(start >=0 && start <= numLines, "Invalid start calculated.");

  if (start < 0)
    start = 0;
  else if (start >= numLines)
    start = numLines - 1; 

  aClosestLine = start; //close as we could come

  return NS_OK;
}

NS_IMETHODIMP
nsBlockFrame::HandleEvent(nsIPresContext* aPresContext, 
                          nsGUIEvent*     aEvent,
                          nsEventStatus*  aEventStatus)
{

  nsresult result;
  nsCOMPtr<nsIPresShell> shell;
  if (aEvent->message == NS_MOUSE_MOVE) {
    aPresContext->GetShell(getter_AddRefs(shell));
    if (!shell)
      return NS_OK;
    nsCOMPtr<nsIFrameSelection> frameSelection;
    PRBool mouseDown = PR_FALSE;
//check to see if we need to ask the selection controller..
    if (mState & NS_FRAME_INDEPENDENT_SELECTION)
    {
      nsCOMPtr<nsISelectionController> selCon;
      result = GetSelectionController(aPresContext, getter_AddRefs(selCon));
      if (NS_FAILED(result) || !selCon)
        return result?result:NS_ERROR_FAILURE;
      frameSelection = do_QueryInterface(selCon);
    }
    else
      shell->GetFrameSelection(getter_AddRefs(frameSelection));
    if (!frameSelection || NS_FAILED(frameSelection->GetMouseDownState(&mouseDown)) || !mouseDown) 
      return NS_OK;//do not handle
  }

  if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN || aEvent->message == NS_MOUSE_MOVE ||
    aEvent->message == NS_MOUSE_LEFT_DOUBLECLICK ) {

    nsMouseEvent *me = (nsMouseEvent *)aEvent;

    nsIFrame *resultFrame = nsnull;//this will be passed the handle event when we 
                                   //can tell who to pass it to
    nsIFrame *mainframe = this;
    aPresContext->GetShell(getter_AddRefs(shell));
    if (!shell)
      return NS_OK;
    nsCOMPtr<nsIFocusTracker> tracker( do_QueryInterface(shell, &result) );

    nsCOMPtr<nsILineIterator> it( do_QueryInterface(mainframe, &result) );
    nsIView* parentWithView;
    nsPoint origin;
    nsPeekOffsetStruct pos;

    while(NS_OK == result)
    { //we are starting aloop to allow us to "drill down to the one we want" 
      mainframe->GetOffsetFromView(aPresContext, origin, &parentWithView);

      if (NS_FAILED(result))
        return NS_OK;//do not handle
      PRInt32 closestLine;

      if (NS_FAILED(result = GetClosestLine(it,origin,aEvent->point,closestLine)))
        return result;
      
      
      //we will now ask where to go. if we cant find what we want"aka another block frame" 
      //we drill down again
      pos.mTracker = tracker;
      pos.mDirection = eDirNext;
      pos.mDesiredX = aEvent->point.x;
      
      result = nsFrame::GetNextPrevLineFromeBlockFrame(aPresContext,
                                          &pos,
                                          mainframe, 
                                          closestLine-1, 
                                          0
                                          );
      
      if (NS_SUCCEEDED(result) && pos.mResultFrame){
        if (result == NS_OK)
          it = do_QueryInterface(pos.mResultFrame, &result);//if this fails thats ok
        resultFrame = pos.mResultFrame;
        mainframe = resultFrame;
      }
      else
        break;//time to go nothing was found
    }
    //end while loop. if nssucceeded resutl then keep going that means
    //we have successfully hit another block frame and we should keep going.


    if (resultFrame)
    {
      if (NS_COMFALSE == result)
      {
        nsCOMPtr<nsISelectionController> selCon;
        result = GetSelectionController(aPresContext, getter_AddRefs(selCon));
        //get the selection controller
        if (NS_SUCCEEDED(result) && selCon) 
        {
          PRInt16 displayresult;
          selCon->GetDisplaySelection(&displayresult);
          if (displayresult == nsISelectionController::SELECTION_OFF)
            return NS_OK;//nothing to do we cannot affect selection from here
        }
        nsCOMPtr<nsIFrameSelection> frameselection;
        shell->GetFrameSelection(getter_AddRefs(frameselection));
        PRBool mouseDown = aEvent->message == NS_MOUSE_MOVE;
        if (frameselection)
        {
          result = frameselection->HandleClick(pos.mResultContent, pos.mContentOffset, 
                                               pos.mContentOffsetEnd, mouseDown || me->isShift, PR_FALSE, pos.mPreferLeft);
        }
      }
      else
        result = resultFrame->HandleEvent(aPresContext, aEvent, aEventStatus);//else let the frame/container do what it needs
      if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN && !IsMouseCaptured(aPresContext))
          CaptureMouse(aPresContext, PR_TRUE);
      return result;
    }
    else
    {
      /*we have to add this because any frame that overrides nsFrame::HandleEvent for mouse down MUST capture the mouse events!!
      if (aEvent->message == NS_MOUSE_LEFT_BUTTON_DOWN && !IsMouseCaptured(aPresContext))
          CaptureMouse(aPresContext, PR_TRUE);*/
      return NS_OK; //just stop it
    }
  }
  return nsFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}


NS_IMETHODIMP
nsBlockFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                               const nsPoint& aPoint,
                               nsFramePaintLayer aWhichLayer,
                               nsIFrame** aFrame)
{
  nsresult rv;

  switch (aWhichLayer) {
    case NS_FRAME_PAINT_LAYER_FOREGROUND:
      rv = GetFrameForPointUsing(aPresContext, aPoint, nsnull, NS_FRAME_PAINT_LAYER_FOREGROUND, PR_FALSE, aFrame);
      if (NS_OK == rv) {
        return NS_OK;
      }
      if (nsnull != mBullet) {
        rv = GetFrameForPointUsing(aPresContext, aPoint, nsLayoutAtoms::bulletList, NS_FRAME_PAINT_LAYER_FOREGROUND, PR_FALSE, aFrame);
      }
      return rv;
      break;

    case NS_FRAME_PAINT_LAYER_FLOATERS:
      // we painted our floaters before our children, and thus
      // we should check floaters within children first
      rv = GetFrameForPointUsing(aPresContext, aPoint, nsnull, NS_FRAME_PAINT_LAYER_FLOATERS, PR_FALSE, aFrame);
      if (NS_OK == rv) {
        return NS_OK;
      }
      if (mFloaters.NotEmpty()) {

        rv = GetFrameForPointUsing(aPresContext, aPoint, nsLayoutAtoms::floaterList, NS_FRAME_PAINT_LAYER_FOREGROUND, PR_FALSE, aFrame);
        if (NS_OK == rv) {
          return NS_OK;
        }

        rv = GetFrameForPointUsing(aPresContext, aPoint, nsLayoutAtoms::floaterList, NS_FRAME_PAINT_LAYER_FLOATERS, PR_FALSE, aFrame);
        if (NS_OK == rv) {
          return NS_OK;
        }

        return GetFrameForPointUsing(aPresContext, aPoint, nsLayoutAtoms::floaterList, NS_FRAME_PAINT_LAYER_BACKGROUND, PR_FALSE, aFrame);

      } else {
        return NS_ERROR_FAILURE;
      }
      break;

    case NS_FRAME_PAINT_LAYER_BACKGROUND:
      // we're a block, so PR_TRUE for consider self
      return GetFrameForPointUsing(aPresContext, aPoint, nsnull, NS_FRAME_PAINT_LAYER_BACKGROUND, PR_TRUE, aFrame);
      break;
  }
  // we shouldn't get here
  NS_ASSERTION(PR_FALSE, "aWhichLayer was not understood");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsBlockFrame::ReflowDirtyChild(nsIPresShell* aPresShell, nsIFrame* aChild)
{
#ifdef DEBUG
  if (gNoisyReflow) {
    IndentBy(stdout, gNoiseIndent);
    ListTag(stdout);
    printf(": ReflowDirtyChild (");
    if (aChild)
      nsFrame::ListTag(stdout, aChild);
    else
      printf("null");
    printf(")\n");

    gNoiseIndent++;
  }
#endif

  if (aChild) {
    // See if the child is absolutely positioned
    nsFrameState  childState;
    aChild->GetFrameState(&childState);
    if (childState & NS_FRAME_OUT_OF_FLOW) {
      const nsStyleDisplay*  disp;
      aChild->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)disp);

      if (disp->IsAbsolutelyPositioned()) {
        // Generate a reflow command to reflow our dirty absolutely
        // positioned child frames.
        // XXX Note that we don't currently try and coalesce the reflow commands,
        // although we should. We can't use the NS_FRAME_HAS_DIRTY_CHILDREN
        // flag, because that's used to indicate whether in-flow children are
        // dirty...
        nsIReflowCommand* reflowCmd;
        nsresult          rv = NS_NewHTMLReflowCommand(&reflowCmd, this,
                                                       nsIReflowCommand::ReflowDirty);
        if (NS_SUCCEEDED(rv)) {
          reflowCmd->SetChildListName(nsLayoutAtoms::absoluteList);
          aPresShell->AppendReflowCommand(reflowCmd);
          NS_RELEASE(reflowCmd);
        }

#ifdef DEBUG
        if (gNoisyReflow) {
          IndentBy(stdout, gNoiseIndent);
          printf("scheduled reflow command for absolutely positioned frame\n");
          --gNoiseIndent;
        }
#endif

        return rv;
      }
    }

    // Mark the line containing the child frame dirty.
    PRBool isFloater;
    line_iterator fline;
    FindLineFor(aChild, &isFloater, &fline);
    
    if (!isFloater) {
      if (fline != end_lines())
        MarkLineDirty(fline);
    }
    else {
      // XXXldb Could we do this more efficiently?
      for (line_iterator line = begin_lines(), line_end = end_lines();
           line != line_end;
           ++line) {
        line->MarkDirty();
      }
    }
  }

  // Either generate a reflow command to reflow the dirty child or 
  // coalesce this reflow request with an existing reflow command    
  if (!(mState & NS_FRAME_HAS_DIRTY_CHILDREN)) {
    // If this is the first dirty child, 
    // post a dirty children reflow command targeted at yourself
    mState |= NS_FRAME_HAS_DIRTY_CHILDREN;

    nsFrame::CreateAndPostReflowCommand(aPresShell, this, 
      nsIReflowCommand::ReflowDirty, nsnull, nsnull, nsnull);

#ifdef DEBUG
    if (gNoisyReflow) {
      IndentBy(stdout, gNoiseIndent);
      printf("scheduled reflow command targeted at self\n");
    }
#endif
  }
  else {
    if (!(mState & NS_FRAME_IS_DIRTY)) {      
      // Mark yourself as dirty
      mState |= NS_FRAME_IS_DIRTY;

      // Cancel the dirty children reflow command you posted earlier
      nsIReflowCommand::ReflowType type = nsIReflowCommand::ReflowDirty;
      aPresShell->CancelReflowCommand(this, &type);

#ifdef DEBUG
      if (gNoisyReflow) {
        IndentBy(stdout, gNoiseIndent);
        printf("cancelled reflow targeted at self\n");
      }
#endif

      // Pass up the reflow request to the parent frame.
      mParent->ReflowDirtyChild(aPresShell, this);
    }
  }

#ifdef DEBUG
  if (gNoisyReflow) {
    --gNoiseIndent;
  }
#endif
  
  return NS_OK;
}

//////////////////////////////////////////////////////////////////////
// Debugging

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
      frame->GetNextSibling(&frame);
    }
  }
  return PR_FALSE;
}

static PRBool
InSiblingList(nsLineList& aLines, nsIFrame* aFrame)
{
  if (! aLines.empty()) {
    nsIFrame* frame = aLines.front()->mFirstChild;
    while (nsnull != frame) {
      if (frame == aFrame) {
        return PR_TRUE;
      }
      frame->GetNextSibling(&frame);
    }
  }
  return PR_FALSE;
}

PRBool
nsBlockFrame::IsChild(nsIPresContext* aPresContext, nsIFrame* aFrame)
{
  nsIFrame* parent;
  aFrame->GetParent(&parent);
  if (parent != (nsIFrame*)this) {
    return PR_FALSE;
  }
  if (InLineList(mLines, aFrame) && InSiblingList(mLines, aFrame)) {
    return PR_TRUE;
  }
  nsLineList* overflowLines = GetOverflowLines(aPresContext, PR_FALSE);
  if (InLineList(*overflowLines, aFrame) && InSiblingList(*overflowLines, aFrame)) {
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

NS_IMETHODIMP
nsBlockFrame::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  if (!aHandler || !aResult) {
    return NS_ERROR_NULL_POINTER;
  }

  PRUint32 sum = sizeof(*this);

  // Add in size of each line object
  for (const_line_iterator line = begin_lines(), line_end = end_lines();
       line != line_end;
       ++line) {
    PRUint32  lineBoxSize;
    nsIAtom* atom = line->SizeOf(aHandler, &lineBoxSize);
    aHandler->AddSize(atom, lineBoxSize);
  }

  *aResult = sum;
  return NS_OK;
}
#endif

//----------------------------------------------------------------------

NS_IMETHODIMP
nsBlockFrame::Init(nsIPresContext*  aPresContext,
                   nsIContent*      aContent,
                   nsIFrame*        aParent,
                   nsIStyleContext* aContext,
                   nsIFrame*        aPrevInFlow)
{
  if (aPrevInFlow) {
    // Copy over the block/area frame type flags
    nsBlockFrame*  blockFrame = (nsBlockFrame*)aPrevInFlow;

    SetFlags(blockFrame->mState & NS_BLOCK_FLAGS_MASK);
  }

  nsresult rv = nsBlockFrameSuper::Init(aPresContext, aContent, aParent,
                                        aContext, aPrevInFlow);
  return rv;
}

nsIStyleContext*
nsBlockFrame::GetFirstLetterStyle(nsIPresContext* aPresContext)
{
  nsIStyleContext* fls;
  aPresContext->ProbePseudoStyleContextFor(mContent,
                                           nsHTMLAtoms::firstLetterPseudo,
                                           mStyleContext, PR_FALSE, &fls);
  return fls;
}

NS_IMETHODIMP
nsBlockFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                  nsIAtom*        aListName,
                                  nsIFrame*       aChildList)
{
  nsresult rv = NS_OK;

  if (nsLayoutAtoms::absoluteList == aListName) {
    mAbsoluteContainer.SetInitialChildList(this, aPresContext, aListName, aChildList);
  }
  else if (nsLayoutAtoms::floaterList == aListName) {
    mFloaters.SetFrames(aChildList);
  }
  else {

    // Lookup up the two pseudo style contexts
    if (nsnull == mPrevInFlow) {
      nsIStyleContext* firstLetterStyle = GetFirstLetterStyle(aPresContext);
      if (nsnull != firstLetterStyle) {
        mState |= NS_BLOCK_HAS_FIRST_LETTER_STYLE;
#ifdef NOISY_FIRST_LETTER
        ListTag(stdout);
        printf(": first-letter style found\n");
#endif
        NS_RELEASE(firstLetterStyle);
      }
    }

    rv = AddFrames(aPresContext, aChildList, nsnull);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Create list bullet if this is a list-item. Note that this is done
    // here so that RenumberLists will work (it needs the bullets to
    // store the bullet numbers).
    const nsStyleDisplay* styleDisplay;
    GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&) styleDisplay);
    if ((nsnull == mPrevInFlow) &&
        (NS_STYLE_DISPLAY_LIST_ITEM == styleDisplay->mDisplay) &&
        (nsnull == mBullet)) {
      // Resolve style for the bullet frame
      nsIStyleContext* kidSC;
      aPresContext->ResolvePseudoStyleContextFor(mContent, 
                                             nsHTMLAtoms::mozListBulletPseudo,
                                             mStyleContext, PR_FALSE, &kidSC);

      // Create bullet frame
      nsCOMPtr<nsIPresShell> shell;
      aPresContext->GetShell(getter_AddRefs(shell));
      mBullet = new (shell.get()) nsBulletFrame;

      if (nsnull == mBullet) {
        NS_RELEASE(kidSC);
        return NS_ERROR_OUT_OF_MEMORY;
      }
      mBullet->Init(aPresContext, mContent, this, kidSC, nsnull);
      NS_RELEASE(kidSC);

      // If the list bullet frame should be positioned inside then add
      // it to the flow now.
      const nsStyleList* styleList;
      GetStyleData(eStyleStruct_List, (const nsStyleStruct*&) styleList);
      if (NS_STYLE_LIST_STYLE_POSITION_INSIDE ==
          styleList->mListStylePosition) {
        AddFrames(aPresContext, mBullet, nsnull);
        mState &= ~NS_BLOCK_FRAME_HAS_OUTSIDE_BULLET;
      }
      else {
        mState |= NS_BLOCK_FRAME_HAS_OUTSIDE_BULLET;
      }
    }
  }

  return NS_OK;
}

PRBool
nsBlockFrame::FrameStartsCounterScope(nsIFrame* aFrame)
{
  const nsStyleContent* styleContent;
  aFrame->GetStyleData(eStyleStruct_Content,
                       (const nsStyleStruct*&) styleContent);
  if (0 != styleContent->CounterResetCount()) {
    // Winner
    return PR_TRUE;
  }
  return PR_FALSE;
}

void
nsBlockFrame::RenumberLists(nsIPresContext* aPresContext)
{
  if (!FrameStartsCounterScope(this)) {
    // If this frame doesn't start a counter scope then we don't need
    // to renumber child list items.
    return;
  }

  // Setup initial list ordinal value
  // XXX Map html's start property to counter-reset style
  PRInt32 ordinal = 1;
  nsIHTMLContent* hc;
  if (mContent && (NS_OK == mContent->QueryInterface(kIHTMLContentIID, (void**) &hc))) {
    nsHTMLValue value;
    if (NS_CONTENT_ATTR_HAS_VALUE ==
        hc->GetHTMLAttribute(nsHTMLAtoms::start, value)) {
      if (eHTMLUnit_Integer == value.GetUnit()) {
        ordinal = value.GetIntValue();
        if (ordinal <= 0) {
          ordinal = 1;
        }
      }
    }
    NS_RELEASE(hc);
  }

  // Get to first-in-flow
  nsBlockFrame* block = (nsBlockFrame*) GetFirstInFlow();
  RenumberListsInBlock(aPresContext, block, &ordinal, 0);
}

PRBool
nsBlockFrame::RenumberListsInBlock(nsIPresContext* aPresContext,
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
        kid->GetNextSibling(&kid);
      }
    }

    // Advance to the next continuation
    aBlockFrame->GetNextInFlow((nsIFrame**) &aBlockFrame);
  }

  return renumberedABullet;
}

PRBool
nsBlockFrame::RenumberListsFor(nsIPresContext* aPresContext,
                               nsIFrame* aKid,
                               PRInt32* aOrdinal,
                               PRInt32 aDepth)
{
  NS_PRECONDITION(aPresContext && aKid && aOrdinal, "null params are immoral!");

  // add in a sanity check for absurdly deep frame trees.  See bug 42138
  if (MAX_DEPTH_FOR_LIST_RENUMBERING < aDepth)
    return PR_FALSE;

  PRBool kidRenumberedABullet = PR_FALSE;
  nsIFrame* kid = aKid;

  // if the frame is a placeholder, then get the out of flow frame
  nsCOMPtr<nsIAtom> frameType;
  aKid->GetFrameType(getter_AddRefs(frameType));
  if (nsLayoutAtoms::placeholderFrame == frameType.get()) {
    kid = NS_STATIC_CAST(nsPlaceholderFrame*, aKid)->GetOutOfFlowFrame();
    NS_ASSERTION(kid, "no out-of-flow frame");
  }

  // If the frame is a list-item and the frame implements our
  // block frame API then get its bullet and set the list item
  // ordinal.
  const nsStyleDisplay* display;
  kid->GetStyleData(eStyleStruct_Display,
                    (const nsStyleStruct*&) display);
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
  // Reflow the bullet now
  nsSize availSize;
  availSize.width = NS_UNCONSTRAINEDSIZE;
  availSize.height = NS_UNCONSTRAINEDSIZE;
  nsHTMLReflowState reflowState(aState.mPresContext, aState.mReflowState,
                                mBullet, availSize);
  nsReflowStatus  status;
  mBullet->WillReflow(aState.mPresContext);
  mBullet->Reflow(aState.mPresContext, aMetrics, reflowState, status);

#ifdef IBMBIDI
  const nsStyleVisibility* vis;
  GetStyleData(eStyleStruct_Visibility, (const nsStyleStruct*&)vis);
#endif // IBMBIDI
  // Place the bullet now; use its right margin to distance it
  // from the rest of the frames in the line
  nscoord x = 
#ifdef IBMBIDI
    // For direction RTL: set x to the right margin for now.
    // This value will be used to indent the bullet from the right most
    // egde of the previous frame in nsLineLayout::HorizontalAlignFrames.
             (NS_STYLE_DIRECTION_RTL == vis->mDirection)
             ? reflowState.mComputedMargin.right :
#endif // IBMBIDI
             - reflowState.mComputedMargin.right - aMetrics.width;

  // Approximate the bullets position; vertical alignment will provide
  // the final vertical location.
  const nsMargin& bp = aState.BorderPadding();
  nscoord y = bp.top;
  mBullet->SetRect(aState.mPresContext, nsRect(x, y, aMetrics.width, aMetrics.height));
  mBullet->DidReflow(aState.mPresContext, NS_FRAME_REFLOW_FINISHED);
}

//XXX get rid of this -- its slow
void
nsBlockFrame::BuildFloaterList()
{
  nsIFrame* head = nsnull;
  nsIFrame* current = nsnull;
  for (line_iterator line = begin_lines(), line_end = end_lines();
       line != line_end;
       ++line) {
    if (line->HasFloaters()) {
      nsFloaterCache* fc = line->GetFirstFloater();
      while (fc) {
        nsIFrame* floater = fc->mPlaceholder->GetOutOfFlowFrame();
        if (nsnull == head) {
          current = head = floater;
        }
        else {
          current->SetNextSibling(floater);
          current = floater;
        }
        fc = fc->Next();
      }
    }
  }

  // Terminate end of floater list just in case a floater was removed
  if (nsnull != current) {
    current->SetNextSibling(nsnull);
  }
  mFloaters.SetFrames(head);
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
  while (nsnull != frame) {
    frameCount++;
    frame->GetNextSibling(&frame);
  }
  NS_ASSERTION(count == frameCount, "bad line list");

  // Next: test that each line has right number of frames on it
  for (line = begin_lines(), line_end = end_lines();
       line != line_end;
        ) {
    count = line->GetChildCount();
    frame = line->mFirstChild;
    while (--count >= 0) {
      frame->GetNextSibling(&frame);
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
nsBlockFrame::VerifyOverflowSituation(nsIPresContext* aPresContext)
{
  nsBlockFrame* flow = (nsBlockFrame*) GetFirstInFlow();
  while (nsnull != flow) {
    nsLineList* overflowLines = GetOverflowLines(aPresContext, PR_FALSE);
    if (nsnull != overflowLines) {
      NS_ASSERTION(! overflowLines->empty(), "should not be empty if present");
      NS_ASSERTION(overflowLines->front()->mFirstChild, "bad overflow list");
    }
    flow = (nsBlockFrame*) flow->mNextInFlow;
  }
}

PRInt32
nsBlockFrame::GetDepth() const
{
  PRInt32 depth = 0;
  nsIFrame* parent = mParent;
  while (nsnull != parent) {
    parent->GetParent(&parent);
    depth++;
  }
  return depth;
}
#endif

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
#include "nsCSSRendering.h"
#include "nsIFrameManager.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsReflowPath.h"
#include "nsIStyleContext.h"
#include "nsIView.h"
#include "nsIFontMetrics.h"
#include "nsHTMLParts.h"
#include "nsHTMLAtoms.h"
#include "nsCSSPseudoElements.h"
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
#include "nsLayoutErrors.h"

#ifdef IBMBIDI
#include "nsBidiPresUtils.h"
#endif // IBMBIDI

#include "nsIDOMHTMLBodyElement.h"
#include "nsIDOMHTMLHtmlElement.h"

#ifdef DEBUG
#include "nsPrintfCString.h"
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

/**
 * A helper class to manage maintenance of the space manager during
 * nsBlockFrame::Reflow. It automatically restores the old space
 * manager in the reflow state when the object goes out of scope.
 */
class nsAutoSpaceManager {
public:
  nsAutoSpaceManager(nsHTMLReflowState& aReflowState)
    : mReflowState(aReflowState),
#ifdef DEBUG
      mOwns(PR_TRUE),
#endif
      mNew(nsnull),
      mOld(nsnull) {}

  ~nsAutoSpaceManager();

  /**
   * Create a new space manager for the specified frame. This will
   * `remember' the old space manager, and install the new space
   * manager in the reflow state.
   */
  nsresult
  CreateSpaceManagerFor(nsIPresContext *aPresContext,
                        nsIFrame *aFrame);

#ifdef DEBUG
  /**
   * `Orphan' any space manager that the nsAutoSpaceManager created;
   * i.e., make it so that we don't destroy the space manager when we
   * go out of scope.
   */
  void DebugOrphanSpaceManager() { mOwns = PR_FALSE; }
#endif

protected:
  nsHTMLReflowState &mReflowState;
#ifdef DEBUG
  PRBool mOwns;
#endif
  nsSpaceManager *mNew;
  nsSpaceManager *mOld;
};

nsAutoSpaceManager::~nsAutoSpaceManager()
{
  // Restore the old space manager in the reflow state if necessary.
  if (mNew) {
#ifdef NOISY_SPACEMANAGER
    printf("restoring old space manager %p\n", mOld);
#endif

    mReflowState.mSpaceManager = mOld;

#ifdef NOISY_SPACEMANAGER
    if (mOld) {
      NS_STATIC_CAST(nsFrame *, mReflowState.frame)->ListTag(stdout);
      printf(": space-manager %p after reflow\n", mOld);
      mOld->List(stdout);
    }
#endif

#ifdef DEBUG
    if (mOwns)
#endif
      delete mNew;
  }
}

nsresult
nsAutoSpaceManager::CreateSpaceManagerFor(nsIPresContext *aPresContext, nsIFrame *aFrame)
{
  // Create a new space manager and install it in the reflow
  // state. `Remember' the old space manager so we can restore it
  // later.
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
  mNew = new nsSpaceManager(shell, aFrame);
  if (! mNew)
    return NS_ERROR_OUT_OF_MEMORY;

#ifdef NOISY_SPACEMANAGER
  printf("constructed new space manager %p (replacing %p)\n",
         mNew, mReflowState.mSpaceManager);
#endif

  // Set the space manager in the existing reflow state
  mOld = mReflowState.mSpaceManager;
  mReflowState.mSpaceManager = mNew;
  return NS_OK;
}

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
    const nsStyleVisibility* visibility;
    GetStyleData(eStyleStruct_Visibility, (const nsStyleStruct*&) visibility);
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
nsBlockFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Block"), aResult);
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
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aMetrics, aStatus);
#ifdef DEBUG
  if (gNoisyReflow) {
    nsCAutoString reflow;
    reflow.Append(nsHTMLReflowState::ReasonToString(aReflowState.reason));

    if (aReflowState.reason == eReflowReason_Incremental) {
      nsHTMLReflowCommand *command = aReflowState.path->mReflowCommand;

      if (command) {
        // We're the target.
        reflow += " (";

        nsReflowType type;
        command->GetType(type);
        reflow += kReflowCommandType[type];

        reflow += ")";
      }
    }

    IndentBy(stdout, gNoiseIndent);
    ListTag(stdout);
    printf(": begin %s reflow availSize=%d,%d computedSize=%d,%d\n",
           reflow.get(),
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

  nsRect oldRect(mRect);

  // Should we create a space manager?
  nsAutoSpaceManager autoSpaceManager(NS_CONST_CAST(nsHTMLReflowState &, aReflowState));

  // XXXldb If we start storing the space manager in the frame rather
  // than keeping it around only during reflow then we should create it
  // only when there are actually floats to manage.  Otherwise things
  // like tables will gain significant bloat.
  if (NS_BLOCK_SPACE_MGR & mState)
    autoSpaceManager.CreateSpaceManagerFor(aPresContext, this);

  // See if it's an incremental reflow command
  if (mAbsoluteContainer.HasAbsoluteFrames() &&
      eReflowReason_Incremental == aReflowState.reason) {
    // Give the absolute positioning code a chance to handle it
    nscoord containingBlockWidth;
    nscoord containingBlockHeight;
    PRBool  handled;

    CalculateContainingBlock(aReflowState, mRect.width, mRect.height,
                             containingBlockWidth, containingBlockHeight);
    
    mAbsoluteContainer.IncrementalReflow(this, aPresContext, aReflowState,
                                         containingBlockWidth,
                                         containingBlockHeight,
                                         handled);

    // If the incremental reflow command was handled by the absolute
    // positioning code, then we're all done.
    if (handled) {
      // Just return our current size as our desired size.
      aMetrics.width = mRect.width;
      aMetrics.height = mRect.height;
      aMetrics.ascent = mAscent;
      aMetrics.descent = aMetrics.height - aMetrics.ascent;

      // Whether or not we're complete hasn't changed
      aStatus = (nsnull != mNextInFlow) ? NS_FRAME_NOT_COMPLETE : NS_FRAME_COMPLETE;
      
      // Factor the absolutely positioned child bounds into the overflow area
      ComputeCombinedArea(aReflowState, aMetrics);
      nsRect childBounds;
      mAbsoluteContainer.CalculateChildBounds(aPresContext, childBounds);
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

  nsBlockReflowState state(aReflowState, aPresContext, this, aMetrics,
                           NS_BLOCK_MARGIN_ROOT & mState);

  // The condition for doing Bidi resolutions includes a test for the
  // dirtiness flags, because blocks sometimes send a resize reflow
  // even though they have dirty children, An example where this can
  // occur is when adding lines to a text control (bugs 95228 and 95400
  // were caused by not doing Bidi resolution in these cases)
  if (eReflowReason_Resize != aReflowState.reason ||
      mState & NS_FRAME_IS_DIRTY || mState & NS_FRAME_HAS_DIRTY_CHILDREN) {
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
                                           forceReflow,
                                           aReflowState.mFlags.mVisualBidiFormControl);
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
    // Do nothing; the dirty lines will already have been marked.
    break;

  case eReflowReason_Incremental: {
#ifdef NOISY_REFLOW_REASON
    ListTag(stdout);
    printf(": reflow=incremental ");
#endif
    nsReflowPath *path = aReflowState.path;
    nsHTMLReflowCommand *command = path->mReflowCommand;
    if (command) {
      nsReflowType type;
      command->GetType(type);
#ifdef NOISY_REFLOW_REASON
      printf("type=%s ", kReflowCommandType[type]);
#endif
      switch (type) {
      case eReflowType_StyleChanged:
        rv = PrepareStyleChangedReflow(state);
        break;
      case eReflowType_ReflowDirty:
        // Do nothing; the dirty lines will already have been marked.
        break;
      case eReflowType_ContentChanged:
        // Perform a full reflow.
        rv = PrepareResizeReflow(state);
        break;
      default:
        // We shouldn't get here. But, if we do, perform a full reflow.
        NS_ERROR("unexpected reflow type");
        rv = PrepareResizeReflow(state);
        break;
      }
    }

    if (path->FirstChild() != path->EndChildren()) {
      // We're along the reflow path, but not necessarily the target
      // of the reflow.
#ifdef NOISY_REFLOW_REASON
      ListTag(stdout);
      printf(" next={ ");

      for (nsReflowPath::iterator iter = path->FirstChild();
           iter != path->EndChildren();
           ++iter) {
        nsFrame::ListTag(stdout, *iter);
        printf(" ");
      }

      printf("}");
#endif

      rv = PrepareChildIncrementalReflow(state);
    }

#ifdef NOISY_REFLOW_REASON
    printf("\n");
#endif

    break;
  }

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

  // If the block is complete, put continuted floaters in the closest ancestor 
  // block that uses the same space manager and leave the block complete; this 
  // allows subsequent lines on the page to be impacted by floaters. If the 
  // block is incomplete or there is no ancestor using the same space manager, 
  // put continued floaters at the beginning of the first overflow line.
  nsFrameList* overflowPlace = nsnull;
  if ((NS_UNCONSTRAINEDSIZE != aReflowState.availableHeight) && 
      (overflowPlace = GetOverflowPlaceholders(aPresContext, PR_TRUE))) {
    PRBool gaveToAncestor = PR_FALSE;
    if (NS_FRAME_IS_COMPLETE(state.mReflowStatus)) {
      // find the nearest block ancestor that uses the same space manager
      for (const nsHTMLReflowState* ancestorRS = aReflowState.parentReflowState; 
           ancestorRS; 
           ancestorRS = ancestorRS->parentReflowState) {
        nsIFrame* ancestor = ancestorRS->frame;
        nsCOMPtr<nsIAtom> fType;
        ancestor->GetFrameType(getter_AddRefs(fType));
        if ((nsLayoutAtoms::blockFrame == fType) || (nsLayoutAtoms::areaFrame == fType)) {
          if (aReflowState.mSpaceManager == ancestorRS->mSpaceManager) {
            // Put the continued floaters in ancestor since it uses the same space manager
            nsFrameList* ancestorPlace =
              ((nsBlockFrame*)ancestor)->GetOverflowPlaceholders(aPresContext, PR_FALSE);
            if (ancestorPlace) {
              ancestorPlace->AppendFrames(ancestor, overflowPlace->FirstChild());
            }
            else {
              ancestorPlace = new nsFrameList(overflowPlace->FirstChild());
              if (ancestorPlace) {
                ((nsBlockFrame*)ancestor)->SetOverflowPlaceholders(aPresContext, ancestorPlace);
              }
              else 
                return NS_ERROR_OUT_OF_MEMORY;
            }
            gaveToAncestor = PR_TRUE;
            break;
          }
        }
      }
    }
    if (!gaveToAncestor) {
      PRInt32 numOverflowPlace = overflowPlace->GetLength();
      nsLineList* overflowLines = GetOverflowLines(aPresContext, PR_FALSE);
      if (overflowLines) {
        line_iterator firstLine = overflowLines->begin(); 
        if (firstLine->IsBlock()) { 
          // Create a new line as the first line and put the floaters there;
          nsLineBox* newLine = state.NewLineBox(overflowPlace->FirstChild(), numOverflowPlace, PR_FALSE);
          firstLine = mLines.before_insert(firstLine, newLine);
        }
        else { // floaters go on 1st overflow line
          nsIFrame* firstFrame = firstLine->mFirstChild;
          firstLine->mFirstChild = overflowPlace->FirstChild();
          // hook up the last placeholder with the original frames
          nsPlaceholderFrame* lastPlaceholder = 
            (nsPlaceholderFrame*)overflowPlace->LastChild();
          lastPlaceholder->SetNextSibling(firstFrame);
          NS_ASSERTION(firstFrame != lastPlaceholder, "trying to set next sibling to self");
          firstLine->SetChildCount(firstLine->GetChildCount() + numOverflowPlace);
        }
      }
      else {
        // Create a line, put the floaters in it, and then push.
        nsLineBox* newLine = state.NewLineBox(overflowPlace->FirstChild(), numOverflowPlace, PR_FALSE);
        if (!newLine) 
          return NS_ERROR_OUT_OF_MEMORY;
        mLines.push_back(newLine);
        nsLineList::iterator nextToLastLine = ----end_lines();
        PushLines(state, nextToLastLine);
      }
      state.mReflowStatus = NS_FRAME_NOT_COMPLETE;
    }
    delete overflowPlace;
  }

  if (NS_FRAME_IS_NOT_COMPLETE(state.mReflowStatus)) {
    if (NS_STYLE_OVERFLOW_HIDDEN == aReflowState.mStyleDisplay->mOverflow) {
      state.mReflowStatus = NS_FRAME_COMPLETE;
    }
    else {
#ifdef DEBUG_kipp
      ListTag(stdout); printf(": block is not complete\n");
#endif
    }
  }

  // XXX_perf get rid of this!  This is one of the things that makes
  // incremental reflow O(N^2).
  BuildFloaterList();
  
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
        rv = frameManager->SetFrameProperty(
                             this, nsLayoutAtoms::spaceManagerProperty,
                             reflowState.mSpaceManager,
                             nsnull /* should be nsSpaceManagerDestroyer*/);

        autoSpaceManager.DebugOrphanSpaceManager();
      }
    }
  }
#endif

  // Determine if we need to repaint our border
  CheckInvalidateBorder(aPresContext, aMetrics, aReflowState);

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
  if (NS_SUCCEEDED(rv) && mAbsoluteContainer.HasAbsoluteFrames()) {
    nsRect childBounds;
    if (eReflowReason_Incremental != aReflowState.reason ||
        aReflowState.path->mReflowCommand ||
        mRect != oldRect) {
      nscoord containingBlockWidth;
      nscoord containingBlockHeight;

      CalculateContainingBlock(aReflowState, aMetrics.width, aMetrics.height,
                               containingBlockWidth, containingBlockHeight);

      rv = mAbsoluteContainer.Reflow(this, aPresContext, aReflowState,
                                     containingBlockWidth,
                                     containingBlockHeight,
                                     childBounds);
    } else {
      mAbsoluteContainer.CalculateChildBounds(aPresContext, childBounds);
    }

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

  // Clear the space manager pointer in the block reflow state so we
  // don't waste time translating the coordinate system back on a dead
  // space manager.
  if (NS_BLOCK_SPACE_MGR & mState)
    state.mSpaceManager = nsnull;

  aStatus = state.mReflowStatus;

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
  if (gNoisyMaxElementSize) {
    if (aMetrics.maxElementSize) {
      IndentBy(stdout, gNoiseIndent);
      printf("block %p returning with maxElementSize=%d,%d\n", this,
             aMetrics.maxElementSize->width,
             aMetrics.maxElementSize->height);
    }
  }
#endif

#ifdef DEBUG_roc
      printf("*** Metrics width/height on the way out=%d,%d\n", aMetrics.width, aMetrics.height);
#endif

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aMetrics);
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

  // XXXldb Handling min-width/max-width stuff after reflowing children
  // seems wrong.  But IIRC this function only does more than a little
  // bit in rare cases (or something like that, I'm not really sure).
  // What are those cases, and do we get the wrong behavior?

  // Compute final width
  nscoord maxWidth = 0, maxHeight = 0;
#ifdef NOISY_KIDXMOST
  printf("%p aState.mKidXMost=%d\n", this, aState.mKidXMost); 
#endif
  if (!HaveAutoWidth(aReflowState)) {
    // Use style defined width
    aMetrics.width = borderPadding.left + aReflowState.mComputedWidth +
      borderPadding.right;

    // When style defines the width use it for the max-element-size
    // because we can't shrink any smaller.
    maxWidth = aMetrics.width;
  }
  else {
    nscoord minWidth = aState.mKidXMost + borderPadding.right;
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
    if (NS_FRAME_IS_COMPLETE(aState.mReflowStatus)) {
      // Calculate the total unconstrained height including borders and padding. A continuation 
      // will have the same value as the first-in-flow, since the reflow state logic is based on
      // style and doesn't alter mComputedHeight based on prev-in-flows.
      aMetrics.height = borderPadding.top + aReflowState.mComputedHeight + borderPadding.bottom;
      if (mPrevInFlow) {
        // Reduce the height by the height of prev-in-flows. The last-in-flow will automatically
        // pick up the bottom border/padding, since it was part of the original aMetrics.height.
        for (nsIFrame* prev = mPrevInFlow; prev; prev->GetPrevInFlow(&prev)) {
          nsRect rect;
          prev->GetRect(rect);
          aMetrics.height -= rect.height;
          // XXX: All block level next-in-flows have borderPadding.top applied to them (bug 174688). 
          // The following should be removed when this gets fixed. bug 174688 prevents us from honoring 
          // a style height (exactly) and this hack at least compensates by increasing the height by the
          // excessive borderPadding.top.
          aMetrics.height += borderPadding.top;
        }
        aMetrics.height = PR_MAX(0, aMetrics.height);
      }
      if (aMetrics.height > aReflowState.availableHeight) {
        // Take up the available height; continuations will take up the rest.
        aMetrics.height = aReflowState.availableHeight;
        aState.mReflowStatus = NS_FRAME_NOT_COMPLETE;
      }
    }
    else {
      // Use the current height; continuations will take up the rest.
      aMetrics.height = aState.mY;
    }

    // When style defines the height use it for the max-element-size
    // because we can't shrink any smaller.
    maxHeight = aMetrics.height;

    // Don't carry out a bottom margin when our height is fixed.
    aState.mPrevBottomMargin.Zero();
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

    if (NS_BLOCK_SPACE_MGR & mState) {
      // Include the space manager's state to properly account for the
      // bottom margin of any floated elements; e.g., inside a table cell.
      nscoord ymost;
      aReflowState.mSpaceManager->YMost(ymost);
      if (ymost > autoHeight)
        autoHeight = ymost;
    }

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
#ifdef DEBUG
    if (gNoisyMaxElementSize) {
      IndentBy(stdout, gNoiseIndent);
      printf ("nsBlockFrame::CFS: %p returning MES %d\n", 
              this, aMetrics.maxElementSize->width);
    }
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
#ifdef DEBUG
  if (gNoisyMaxElementSize) {
    if (aState.GetFlag(BRS_COMPUTEMAXELEMENTSIZE)) {
      IndentBy(stdout, gNoiseIndent);
      if (NS_UNCONSTRAINEDSIZE == aState.mReflowState.availableWidth) {
        printf("PASS1 ");
      }
      ListTag(stdout);
      printf(": max-element-size:%d,%d desired:%d,%d maxSize:%d,%d\n",
             maxWidth, maxHeight, aMetrics.width, aMetrics.height,
             aState.mReflowState.availableWidth,
             aState.mReflowState.availableHeight);
    }
  }
#endif

  // If we're requested to update our maximum width, then compute it
  if (aState.GetFlag(BRS_COMPUTEMAXWIDTH)) {
    // We need to add in for the right border/padding
    // XXXldb Why right and not left?
    aMetrics.mMaximumWidth = aState.mMaximumWidth + borderPadding.right;
#ifdef NOISY_MAXIMUM_WIDTH
    printf("nsBlockFrame::ComputeFinalSize block %p setting aMetrics.mMaximumWidth to %d\n", this, aMetrics.mMaximumWidth);
#endif
  }

  ComputeCombinedArea(aReflowState, aMetrics);

  // If the combined area of our children exceeds our bounding box
  // then set the NS_FRAME_OUTSIDE_CHILDREN flag, otherwise clear it.
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

void
nsBlockFrame::ComputeCombinedArea(const nsHTMLReflowState& aReflowState,
                                  nsHTMLReflowMetrics& aMetrics)
{
  // Compute the combined area of our children
  // XXX_perf: This can be done incrementally.  It is currently one of
  // the things that makes incremental reflow O(N^2).
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

  aMetrics.mOverflowArea.x = xa;
  aMetrics.mOverflowArea.y = ya;
  aMetrics.mOverflowArea.width = xb - xa;
  aMetrics.mOverflowArea.height = yb - ya;

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
  // XXXwaterson this is non-optimal. We'd rather do this in
  // ReflowDirtyLines; however, I'm not quite ready to figure out how
  // to deal with reflow path retargeting yet.
  nsReflowPath *path = aState.mReflowState.path;

  nsReflowPath::iterator iter = path->FirstChild();
  nsReflowPath::iterator end = path->EndChildren();

  for ( ; iter != end; ++iter) {
    // Determine the line being impacted
    line_iterator line = FindLineFor(*iter);
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
      PrepareResizeReflow(aState);
      continue;
    }

    if (line->IsInline()) {
      if (aState.GetFlag(BRS_COMPUTEMAXWIDTH)) {
        // We've been asked to compute the maximum width of the block
        // frame, which ReflowLine() will handle by performing an
        // unconstrained reflow on the line. If this incremental
        // reflow is targeted at a continuing frame, we may have to
        // retarget it, as the unconstrained reflow can destroy some
        // of the continuations. This will allow the incremental
        // reflow to arrive at the target frame during the first-pass
        // unconstrained reflow.
        nsIFrame *prevInFlow;
        (*iter)->GetPrevInFlow(&prevInFlow);
        if (prevInFlow)
          RetargetInlineIncrementalReflow(iter, line, prevInFlow);
      }
    }

    // Just mark this line dirty.  We never need to mark the
    // previous line dirty since either:
    //  * the line is a block, and there would never be a chance to pull
    //    something up
    //  * It's an incremental reflow to something within an inline, which
    //    we know must be very limited.
    line->MarkDirty();
  }
  return NS_OK;
}

void
nsBlockFrame::RetargetInlineIncrementalReflow(nsReflowPath::iterator &aTarget,
                                              line_iterator &aLine,
                                              nsIFrame *aPrevInFlow)
{
  // To retarget the reflow, we'll walk back through the continuations
  // until we reach the primary frame, or we reach a continuation that
  // is preceded by a ``hard'' line break.
  NS_ASSERTION(aLine->Contains(*aTarget),
               "line doesn't contain the target of the incremental reflow");

  // Now fix the iterator, keeping track of how many lines we walk
  // back through.
  PRInt32 lineCount = 0;
  do {
    // XXX this might happen if the block is split; e.g.,
    // printing or print preview. For now, panic.
    NS_ASSERTION(aLine != begin_lines(),
                 "ran out of lines before we ran out of prev-in-flows");

    // Is the previous line a ``hard'' break? If so, stop: these
    // continuations will be preserved during an unconstrained reflow.
    // XXXwaterson should this be `!= NS_STYLE_CLEAR_NONE'?
    --aLine;
    if (aLine->GetBreakType() == NS_STYLE_CLEAR_LINE)
      break;

    *aTarget = aPrevInFlow;
    aPrevInFlow->GetPrevInFlow(&aPrevInFlow);

#ifdef DEBUG
    // Paranoia. Ensure that the prev-in-flow is really in the
    // previous line.
    line_iterator check = FindLineFor(*aTarget);
    NS_ASSERTION(check == aLine, "prev-in-flow not in previous linebox");
#endif

    ++lineCount;
  } while (aPrevInFlow);

  if (lineCount > 0) {
    // Fix any frames deeper in the reflow path.
#if 0
    // XXXwaterson fix me! we've got to recurse through the iterator's
    // kids.  This really shouldn't matter unless we want to implement
    // `display: inline-block' or do XBL form controls. Why, you ask?
    // Because what will happen is that inline frames will get flowed
    // with a resize reflow, which will be sufficient to mask any
    // glitches that would otherwise occur. However, as soon as boxes
    // or blocks end up in the flow here (and aren't explicit reflow
    // roots), they may optimize away the resize reflow.

    // Get the reflow path, which is stored as a stack (i.e., the next
    // frame in the reflow is at the _end_ of the array).
    nsVoidArray *path = aState.mReflowState.reflowCommand->GetPath();

    for (PRInt32 i = path->Count() - 1; i >= 0; --i) {
      nsIFrame *frame = NS_STATIC_CAST(nsIFrame *, path->ElementAt(i));

      // Stop if we encounter a non-inline frame in the reflow path.
      const nsStyleDisplay *display;
      ::GetStyleData(frame, &display);

      if (NS_STYLE_DISPLAY_INLINE != display->mDisplay)
        break;

      // Walk back to the primary frame.
      PRInt32 count = lineCount;
      nsIFrame *prevInFlow;
      do {
        frame->GetPrevInFlow(&prevInFlow);
      } while (--count >= 0 && prevInFlow && (frame = prevInFlow));

      path->ReplaceElementAt(frame, i);
    }
#else
    NS_WARNING("blowing an incremental reflow targeted at a nested inline");
#endif
  }
}

PRBool
nsBlockFrame::IsLineEmpty(nsIPresContext* aPresContext,
                          const nsLineBox* aLine) const
{
  const nsStyleText* styleText;
  ::GetStyleData(mStyleContext, &styleText);
  PRBool isPre = NS_STYLE_WHITESPACE_PRE == styleText->mWhiteSpace ||
                 NS_STYLE_WHITESPACE_MOZ_PRE_WRAP == styleText->mWhiteSpace;

  nsCompatibility compat;
  aPresContext->GetCompatibilityMode(&compat);

  PRBool empty = PR_FALSE;
  aLine->IsEmpty(compat, isPre, &empty);
  return empty;
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
      // However, right floaters within such lines might need to be
      // repositioned, so we check for floaters as well.
      if (line->IsBlock() ||
          line->HasPercentageChild() || 
          line->HasFloaters() ||
          (wrapping &&
           ((line != mLines.back() && !line->HasBreak()) ||
           line->ResizeReflowOptimizationDisabled() ||
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
           NS_STATIC_CAST(void*, (line.next() != end_lines() ? line.next().get() : nsnull)),
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

nsBlockFrame::line_iterator
nsBlockFrame::FindLineFor(nsIFrame* aFrame)
{
  // This assertion actually fires on lots of pages (e.g., bugzilla,
  // bugzilla query page), so limit it to a few people until we fix the
  // problem causing it.  It's related to the similarly |#ifdef|ed
  // assertion in |PrepareChildIncrementalReflow|.
#if defined(DEBUG_dbaron) || defined(DEBUG_waterson)
  NS_PRECONDITION(aFrame, "why pass a null frame?");
#endif

  line_iterator line = begin_lines(),
                line_end = end_lines();
  for ( ; line != line_end; ++line) {
    // If the target frame is in-flow, and this line contains the it,
    // then we've found our line.
    if (line->Contains(aFrame))
      return line;

    // If the target frame is floated, and this line contains the
    // floater's placeholder, then we've found our line.
    if (line->HasFloaters()) {
      for (nsFloaterCache *fc = line->GetFirstFloater();
           fc != nsnull;
           fc = fc->Next()) {
        if (aFrame == fc->mPlaceholder->GetOutOfFlowFrame())
          return line;
      }
    }      
  }

  return line_end;
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
 * Propagate reflow "damage" from from earlier lines to the current
 * line.  The reflow damage comes from the following sources:
 *  1. The regions of floater damage remembered during reflow.
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
      IndentBy(stdout, gNoiseIndent);
      ListTag(stdout);
      printf(": incrementally reflowing dirty lines");

      nsHTMLReflowCommand *command = aState.mReflowState.path->mReflowCommand;
      if (command) {
        nsReflowType type;
        command->GetType(type);
        printf(": type=%s(%d)", kReflowCommandType[type], type);
      }
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
  PRBool needToRecoverState = PR_FALSE;
  
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
    // are dirty.  If we are printing (constrained height), always reflow
    // the line.
    if ((NS_UNCONSTRAINEDSIZE != aState.mReflowState.availableHeight) ||
        (!line->IsDirty() &&
         aState.GetFlag(BRS_COMPUTEMAXWIDTH) &&
         ::WrappedLinesAreDirty(line, line_end))) {
      line->MarkDirty();
    }

    // Make sure |aState.mPrevBottomMargin| is at the correct position
    // before calling PropagateFloaterDamage.
    if (needToRecoverState &&
        (line->IsDirty() || line->IsPreviousMarginDirty())) {
      // We need to reconstruct the bottom margin only if we didn't
      // reflow the previous line and we do need to reflow (or repair
      // the top position of) the next line.
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
      // Compute the dirty lines "before" YMost, after factoring in
      // the running deltaY value - the running value is implicit in
      // aState.mY.
      nscoord oldY = line->mBounds.y;
      nscoord oldYMost = line->mBounds.YMost();
      nsRect oldCombinedArea;
      line->GetCombinedArea(&oldCombinedArea);

      // Reflow the dirty line. If it's an incremental reflow, then force
      // it to invalidate the dirty area if necessary
      PRBool forceInvalidate = incrementalReflow && !aState.GetFlag(BRS_DAMAGECONSTRAINED);
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
    } else {
      if (deltaY != 0)
        SlideLine(aState, line, deltaY);
      else
        repositionViews = PR_TRUE;

      // XXX EVIL O(N^2) EVIL
      aState.RecoverStateFrom(line, deltaY);

      // Keep mY up to date in case we're propagating reflow damage.
      aState.mY = line->mBounds.YMost();
      needToRecoverState = PR_TRUE;
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

  if (needToRecoverState) {
    // Is this expensive?
    aState.ReconstructMarginAbove(line);

    // Update aState.mPrevChild as if we had reflowed all of the frames in
    // this line.  This is expensive in some cases, since it requires
    // walking |GetNextSibling|.
    aState.mPrevChild = line.prev()->LastChild();
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
        if (!dirtyRect.IsEmpty()) {
          Invalidate(aState.mPresContext, dirtyRect);
        }

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
          if (!dirtyRect.IsEmpty()) {
            Invalidate(aState.mPresContext, dirtyRect);
          }
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
          if (!dirtyRect.IsEmpty()) {
            Invalidate(aState.mPresContext, dirtyRect);
          }
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
      // First reflow the line with an unconstrained width. 
      nscoord oldY = aState.mY;
      nsCollapsingMargin oldPrevBottomMargin(aState.mPrevBottomMargin);
      PRBool  oldUnconstrainedWidth = aState.GetFlag(BRS_UNCONSTRAINEDWIDTH);

#if defined(DEBUG_waterson) || defined(DEBUG_dbaron)
      if (oldUnconstrainedWidth)
        printf("*** oldUnconstrainedWidth was already set.\n"
               "*** This code (%s:%d) could be optimized a lot!\n",
               __FILE__, __LINE__);
#endif

      // When doing this we need to set the block reflow state's
      // "mUnconstrainedWidth" variable to PR_TRUE so if we encounter
      // a placeholder and then reflow its associated floater we don't
      // end up resetting the line's right edge and have it think the
      // width is unconstrained...
      aState.mSpaceManager->PushState();
      aState.SetFlag(BRS_UNCONSTRAINEDWIDTH, PR_TRUE);
      ReflowInlineFrames(aState, aLine, aKeepReflowGoing, aDamageDirtyArea, PR_TRUE);
      aState.mY = oldY;
      aState.mPrevBottomMargin = oldPrevBottomMargin;
      aState.SetFlag(BRS_UNCONSTRAINEDWIDTH, oldUnconstrainedWidth);
      aState.mSpaceManager->PopState();

#ifdef DEBUG_waterson
      // XXXwaterson if oldUnconstrainedWidth was set, why do we need
      // to do the second reflow, below?
      if (oldUnconstrainedWidth)
        printf("+++ possibly doing an unnecessary second-pass unconstrained reflow\n");
#endif

      // Update the line's maximum width
      aLine->mMaximumWidth = aLine->mBounds.XMost();
#ifdef NOISY_MAXIMUM_WIDTH
      printf("nsBlockFrame::ReflowLine block %p line %p setting aLine.mMaximumWidth to %d\n", 
             this, NS_STATIC_CAST(void*, aLine.get()), aLine->mMaximumWidth);
#endif
      aState.UpdateMaximumWidth(aLine->mMaximumWidth);

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
                 this, NS_STATIC_CAST(void*, aLine.get()), aLine->mMaximumWidth);
#endif
          aState.UpdateMaximumWidth(aLine->mMaximumWidth);
        }
        if (aState.GetFlag(BRS_COMPUTEMAXELEMENTSIZE))
        {
#ifdef DEBUG
          if (gNoisyMaxElementSize) {
            IndentBy(stdout, gNoiseIndent);
            printf("nsBlockFrame::ReflowLine block %p line %p setting aLine.mMaxElementWidth to %d\n", 
                   this, NS_STATIC_CAST(void*, aLine.get()), aLine->mMaxElementWidth);
          }
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
        printf("  dirty line is %p\n", NS_STATIC_CAST(void*, aLine.get());
#endif
      if (!dirtyRect.IsEmpty()) {
        Invalidate(aState.mPresContext, dirtyRect);
      }
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
  aFrameResult = nsnull;

  // First check our remaining lines
  if (end_lines() != aLine.next()) {
    return PullFrameFrom(aState, aLine, mLines, aLine.next(), PR_FALSE,
                         aDamageDeletedLines, aFrameResult);
  }

  // Pull frames from the next-in-flow(s) until we can't
  nsBlockFrame* nextInFlow = aState.mNextInFlow;
  while (nsnull != nextInFlow) {
    if (! nextInFlow->mLines.empty()) {
      return PullFrameFrom(aState, aLine, nextInFlow->mLines,
                           nextInFlow->mLines.begin(), PR_TRUE,
                           aDamageDeletedLines, aFrameResult);
    }

    nextInFlow = (nsBlockFrame*) nextInFlow->mNextInFlow;
    aState.mNextInFlow = nextInFlow;
  }

  return NS_OK;
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
                            nsIFrame*& aFrameResult)
{
  nsLineBox* fromLine = aFromLine;
  NS_ABORT_IF_FALSE(fromLine, "bad line to pull from");
  NS_ABORT_IF_FALSE(fromLine->GetChildCount(), "empty line");
  NS_ABORT_IF_FALSE(aLine->GetChildCount(), "empty line");

  if (fromLine->IsBlock()) {
    // If our line is not empty and the child in aFromLine is a block
    // then we cannot pull up the frame into this line. In this case
    // we stop pulling.
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
      nsRect combinedArea;
      fromLine->GetCombinedArea(&combinedArea);

      // Free up the fromLine now that it's empty
      // Its bounds might need to be redrawn, though.
      if (aDamageDeletedLines && !fromLine->mBounds.IsEmpty()) {
        Invalidate(aState.mPresContext, fromLine->mBounds);
      }
      if (aFromLine.next() != end_lines())
        aFromLine.next()->MarkPreviousMarginDirty();
      Invalidate(aState.mPresContext, combinedArea);
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

  nsRect lineCombinedArea;
  aLine->GetCombinedArea(&lineCombinedArea);

  PRBool doInvalidate = !lineCombinedArea.IsEmpty();
  if (doInvalidate)
    Invalidate(aState.mPresContext, lineCombinedArea);
  // Adjust line state
  aLine->SlideBy(aDY);
  if (doInvalidate) {
    aLine->GetCombinedArea(&lineCombinedArea);
    Invalidate(aState.mPresContext, lineCombinedArea);
  }

  // Adjust the frames in the line
  nsIFrame* kid = aLine->mFirstChild;
  if (!kid) {
    return;
  }

  if (aLine->IsBlock()) {
    if (aDY) {
      nsPoint p;
      kid->GetOrigin(p);
      p.y += aDY;
      kid->MoveTo(aState.mPresContext, p.x, p.y);
    }

    // Make sure the frame's view and any child views are updated
    ::PlaceFrameView(aState.mPresContext, kid);
  }
  else {
    // Adjust the Y coordinate of the frames in the line.
    // Note: we need to re-position views even if aDY is 0, because
    // one of our parent frames may have moved and so the view's position
    // relative to its parent may have changed
    PRInt32 n = aLine->GetChildCount();
    while (--n >= 0) {
      if (aDY) {
        nsPoint p;
        kid->GetOrigin(p);
        p.y += aDY;
        kid->MoveTo(aState.mPresContext, p.x, p.y);
      }
      // Make sure the frame's view and any child views are updated
      ::PlaceFrameView(aState.mPresContext, kid);
      kid->GetNextSibling(&kid);
    }
  }
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
    
    nsHTMLReflowCommand* reflowCmd;
    rv = NS_NewHTMLReflowCommand(&reflowCmd, this,
                                 eReflowType_ContentChanged,
                                 nsnull,
                                 aAttribute);
    if (NS_SUCCEEDED(rv))
      shell->AppendReflowCommand(reflowCmd);
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
        
        nsHTMLReflowCommand* reflowCmd;
        rv = NS_NewHTMLReflowCommand(&reflowCmd, blockParent,
                                     eReflowType_ContentChanged,
                                     nsnull,
                                     aAttribute);
        if (NS_SUCCEEDED(rv))
          shell->AppendReflowCommand(reflowCmd);
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
nsBlockFrame::IsEmpty(nsCompatibility aCompatMode, PRBool aIsPre,
                      PRBool *aResult)
{
  // Start with a bunch of early returns for things that mean we're not
  // empty.
  *aResult = PR_FALSE;

  const nsStylePosition* position;
  ::GetStyleData(this, &position);

  switch (position->mMinHeight.GetUnit()) {
    case eStyleUnit_Coord:
      if (position->mMinHeight.GetCoordValue() != 0)
        return NS_OK;
      break;
    case eStyleUnit_Percent:
      if (position->mMinHeight.GetPercentValue() != 0.0f)
        return NS_OK;
      break;
    default:
      return NS_OK;
  }

  switch (position->mHeight.GetUnit()) {
    case eStyleUnit_Auto:
      break;
    case eStyleUnit_Coord:
      if (position->mHeight.GetCoordValue() != 0)
        return NS_OK;
      break;
    case eStyleUnit_Percent:
      if (position->mHeight.GetPercentValue() != 0.0f)
        return NS_OK;
      break;
    default:
      return NS_OK;
  }

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
                    padding->mPadding.GetBottom(coord))) {
    return NS_OK;
  }

  const nsStyleText* styleText;
  ::GetStyleData(this, &styleText);
  PRBool isPre = NS_STYLE_WHITESPACE_PRE == styleText->mWhiteSpace ||
                 NS_STYLE_WHITESPACE_MOZ_PRE_WRAP == styleText->mWhiteSpace;
  // Now the only thing that could make us non-empty is one of the lines
  // being non-empty.  So now assume that we are empty until told
  // otherwise.
  *aResult = PR_TRUE;
  for (line_iterator line = begin_lines(), line_end = end_lines();
       line != line_end;
       ++line)
  {
    line->IsEmpty(aCompatMode, isPre, aResult);
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
  //
  const nsStyleText* styleText;
  ::GetStyleData(this, &styleText);
  PRBool isPre = NS_STYLE_WHITESPACE_PRE == styleText->mWhiteSpace ||
                 NS_STYLE_WHITESPACE_MOZ_PRE_WRAP == styleText->mWhiteSpace;

  nsCompatibility compat;
  aState.mPresContext->GetCompatibilityMode(&compat);

  for (line_iterator line = begin_lines(); line != aLine; ++line) {
    PRBool empty;
    line->IsEmpty(compat, isPre, &empty);
    if (!empty) {
      // A line which preceeds aLine is non-empty, so therefore the
      // top margin applies.
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
nsBlockFrame::GetTopBlockChild(nsIPresContext* aPresContext)
{
  if (mLines.empty())
    return nsnull;

  nsLineBox *firstLine = mLines.front();
  if (firstLine->IsBlock())
    return firstLine->mFirstChild;

  if (!IsLineEmpty(aPresContext, firstLine))
    return nsnull;

  line_iterator secondLine = begin_lines();
  ++secondLine;
  if (secondLine == end_lines() || !secondLine->IsBlock())
    return nsnull;

  return secondLine->mFirstChild;
}

// If placeholders/floaters split during reflowing a line, but that line will 
// be put on the next page, then put the placeholders/floaters back the way
// they were before the line was reflowed. 
void
nsBlockFrame::UndoSplitPlaceholders(nsBlockReflowState& aState,
                                    nsIFrame*           aLastPlaceholder)
{
  nsIFrame* undoPlaceholder = nsnull;
  if (aLastPlaceholder) {
    aLastPlaceholder->GetNextSibling(&undoPlaceholder);
    aLastPlaceholder->SetNextSibling(nsnull);
  }
  else {
    // just remove the property
    nsFrameList* overflowPlace = GetOverflowPlaceholders(aState.mPresContext, PR_TRUE);
    delete overflowPlace;
  }
  // remove the next in flows of the placeholders that need to be removed
  for (nsIFrame* placeholder = undoPlaceholder; placeholder; ) {
    nsSplittableFrame::RemoveFromFlow(placeholder);
    nsIFrame* savePlaceholder = placeholder; 
    placeholder->GetNextSibling(&placeholder);
    savePlaceholder->Destroy(aState.mPresContext);
  }
}

// Combine aNewBreakType with aOrigBreakType, but limit the break types
// to NS_STYLE_CLEAR_LEFT, RIGHT, LEFT_AND_RIGHT. When there is a <BR> right 
// after a floater and the floater splits, then the <BR>'s break type is combined 
// with the break type of the frame right after the floaters next-in-flow.
static PRUint8
CombineBreakType(PRUint8 aOrigBreakType, 
                 PRUint8 aNewBreakType)
{
  PRUint8 breakType = aOrigBreakType;
  switch(breakType) {
  case NS_STYLE_CLEAR_LEFT:
    if ((NS_STYLE_CLEAR_RIGHT          == aNewBreakType) || 
        (NS_STYLE_CLEAR_LEFT_AND_RIGHT == aNewBreakType)) {
      breakType = NS_STYLE_CLEAR_LEFT_AND_RIGHT;
    }
    break;
  case NS_STYLE_CLEAR_RIGHT:
    if ((NS_STYLE_CLEAR_LEFT           == aNewBreakType) || 
        (NS_STYLE_CLEAR_LEFT_AND_RIGHT == aNewBreakType)) {
      breakType = NS_STYLE_CLEAR_LEFT_AND_RIGHT;
    }
    break;
  case NS_STYLE_CLEAR_NONE:
    if ((NS_STYLE_CLEAR_LEFT           == aNewBreakType) ||
        (NS_STYLE_CLEAR_RIGHT          == aNewBreakType) ||    
        (NS_STYLE_CLEAR_LEFT_AND_RIGHT == aNewBreakType)) {
      breakType = NS_STYLE_CLEAR_LEFT_AND_RIGHT;
    }
  }
  return breakType;
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
  const nsStyleDisplay* display;
  frame->GetStyleData(eStyleStruct_Display,
                      (const nsStyleStruct*&) display);
  nsBlockReflowContext brc(aState.mPresContext, aState.mReflowState,
                           aState.GetFlag(BRS_COMPUTEMAXELEMENTSIZE),
                           aState.GetFlag(BRS_COMPUTEMAXWIDTH));

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

  PRUint8 breakType = display->mBreakType;
  // If a floater split and its prev-in-flow was followed by a <BR>, then combine 
  // the <BR>'s break type with the block's break type (the block will be the very 
  // next frame after the split floater).
  if (NS_STYLE_CLEAR_NONE != aState.mFloaterBreakType) {
    breakType = ::CombineBreakType(breakType, aState.mFloaterBreakType);
    aState.mFloaterBreakType = NS_STYLE_CLEAR_NONE;
  }
  // Clear past floaters before the block if the clear style is not none
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

    // Setup a reflowState to get the style computed margin-top
    // value. We'll use a reason of `resize' so that we don't fudge
    // any incremental reflow state.

    // The availSpace here is irrelevant to our needs - all we want
    // out if this setup is the margin-top value which doesn't depend
    // on the childs available space.
    nsSize availSpace(aState.mContentArea.width, NS_UNCONSTRAINEDSIZE);
    nsHTMLReflowState reflowState(aState.mPresContext, aState.mReflowState,
                                  frame, availSpace, eReflowReason_Resize);

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

  // keep track of the last overflow floater in case we need to undo any new additions
  nsFrameList* overflowPlace = GetOverflowPlaceholders(aState.mPresContext, PR_FALSE);
  nsIFrame* lastPlaceholder = (overflowPlace) ? overflowPlace->LastChild() : nsnull;

  // Reflow the block into the available space
  nsReflowStatus frameReflowStatus=NS_FRAME_COMPLETE;
  nsMargin computedOffsets;
  // construct the html reflow state for the block. ReflowBlock 
  // will initialize it and set its reason.
  nsHTMLReflowState blockHtmlRS(aState.mPresContext, aState.mReflowState, frame, 
                                nsSize(availSpace.width, availSpace.height), 
                                aState.mReflowState.reason, PR_FALSE);
  rv = brc.ReflowBlock(availSpace, applyTopMargin, aState.mPrevBottomMargin,
                       aState.IsAdjacentWithTop(), computedOffsets, 
                       blockHtmlRS, frameReflowStatus);

  if (brc.BlockShouldInvalidateItself() && !mRect.IsEmpty()) {
    Invalidate(aState.mPresContext, mRect);
  }

  // Remove the frame from the reflow tree.
  if (aState.mReflowState.path)
    aState.mReflowState.path->RemoveChild(frame);

  if (NS_FAILED(rv)) {
    return rv;
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

    // Try to place the child block
    PRBool isAdjacentWithTop = aState.IsAdjacentWithTop();
    nsCollapsingMargin collapsedBottomMargin;
    nsRect combinedArea(0,0,0,0);
    *aKeepReflowGoing = brc.PlaceBlock(blockHtmlRS, isAdjacentWithTop,
                                       computedOffsets, collapsedBottomMargin,
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
        rv = CreateContinuationFor(aState, nsnull, frame, madeContinuation);
        if (NS_FAILED(rv)) 
          return rv;

        // Push continuation to a new line, but only if we actually made one.
        if (madeContinuation) {
          frame->GetNextSibling(&frame);
          nsLineBox* line = aState.NewLineBox(frame, 1, PR_TRUE);
          if (nsnull == line) {
            return NS_ERROR_OUT_OF_MEMORY;
          }
          mLines.after_insert(aLine, line);
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
      if (frame == GetTopBlockChild(aState.mPresContext)) {
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

        // Doing the alignment using |mAscent| will also cater for bullets
        // that are placed next to a child block (bug 92896)
        // (Note that mAscent should be set by now, otherwise why would
        // we be placing the bullet yet?)

        // Tall bullets won't look particularly nice here...
        nsRect bbox;
        mBullet->GetRect(bbox);
        nscoord bulletTopMargin = applyTopMargin
                                    ? collapsedBottomMargin.get()
                                    : 0;
        bbox.y = aState.BorderPadding().top + mAscent -
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
        UndoSplitPlaceholders(aState, lastPlaceholder);
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

#define LINE_REFLOW_OK        0
#define LINE_REFLOW_STOP      1
#define LINE_REFLOW_REDO      2
// a frame was complete, but truncated and not at the top of a page
#define LINE_REFLOW_TRUNCATED 3

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

// If at least one floater on the line was complete, not at the top of page, but was 
// truncated, then restore the overflow floaters to what they were before and push the line.
// The floaters that will be removed from the list aren't yet known by the block's next in flow.  
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
  if (ShouldApplyTopMargin(aState, aLine)) {
    aState.mY += aState.mPrevBottomMargin.get();
  }
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

  // keep track of the last overflow floater in case we need to undo any new additions
  nsFrameList* overflowPlace = GetOverflowPlaceholders(aState.mPresContext, PR_FALSE);
  nsIFrame* lastPlaceholder = (overflowPlace) ? overflowPlace->LastChild() : nsnull;

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
        // Push the line with the truncated floater 
        PushTruncatedPlaceholderLine(aState, aLine, lastPlaceholder, *aKeepReflowGoing);
      }
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

    // We don't want to advance by the bottom margin anymore (we did it
    // once at the beginning of this function, which will just be called
    // again), and we certainly don't want to go back if it's negative
    // (infinite loop, bug 153429).
    aState.mPrevBottomMargin.Zero();

    // XXX: a small optimization can be done here when paginating:
    // if the new Y coordinate is past the end of the block then
    // push the line and return now instead of later on after we are
    // past the floater.
  }
  else if (LINE_REFLOW_TRUNCATED != lineReflowStatus) {
    // If we are propagating out a break-before status then there is
    // no point in placing the line.
    if (!NS_INLINE_IS_BREAK_BEFORE(aState.mReflowStatus)) {
      if (PlaceLine(aState, aLineLayout, aLine, aKeepReflowGoing, aUpdateMaximumWidth)) {
        UndoSplitPlaceholders(aState, lastPlaceholder); // undo since we pushed the current line
      }
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
  nsresult rv = aLineLayout.ReflowFrame(aFrame, frameReflowStatus,
                                        nsnull, pushedFrame);

  // If this is an incremental reflow, prune the child from the path
  // so we don't incrementally reflow it again.
  // XXX note that we don't currently have any incremental reflows
  // that trace a path through an inline frame (thanks to reflow roots
  // dealing with text inputs), but we may need to deal with this
  // again, someday.
  if (aState.mReflowState.path)
    aState.mReflowState.path->RemoveChild(aFrame);

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
  if (NS_INLINE_IS_BREAK(frameReflowStatus) || 
      (NS_STYLE_CLEAR_NONE != aState.mFloaterBreakType)) {
    // Always abort the line reflow (because a line break is the
    // minimal amount of break we do).
    *aLineReflowStatus = LINE_REFLOW_STOP;

    // XXX what should aLine's break-type be set to in all these cases?
    PRUint8 breakType = NS_INLINE_GET_BREAK_TYPE(frameReflowStatus);
    NS_ASSERTION((NS_STYLE_CLEAR_NONE != breakType) || 
                 (NS_STYLE_CLEAR_NONE != aState.mFloaterBreakType), "bad break type");
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
      // If a floater split and its prev-in-flow was followed by a <BR>, then combine 
      // the <BR>'s break type with the inline's break type (the inline will be the very 
      // next frame after the split floater).
      if (NS_STYLE_CLEAR_NONE != aState.mFloaterBreakType) {
        breakType = ::CombineBreakType(breakType, aState.mFloaterBreakType);
        aState.mFloaterBreakType = NS_STYLE_CLEAR_NONE;
      }
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
        if (NS_FAILED(rv)) 
          return rv;
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

    nsCOMPtr<nsIAtom> frameType;
    aFrame->GetFrameType(getter_AddRefs(frameType));

    // Create a continuation for the incomplete frame. Note that the
    // frame may already have a continuation.
    PRBool madeContinuation;
    rv = (nsLayoutAtoms::placeholderFrame == frameType)
         ? SplitPlaceholder(*aState.mPresContext, *aFrame)
         : CreateContinuationFor(aState, aLine, aFrame, madeContinuation);
    if (NS_FAILED(rv)) 
      return rv;

    // Remember that the line has wrapped
    aLine->SetLineWrapped(PR_TRUE);
    
    // If we are reflowing the first letter frame or a placeholder then 
    // don't split the line and don't stop the line reflow...
    PRBool splitLine = !reflowingFirstLetter && 
                       (nsLayoutAtoms::placeholderFrame != frameType);
    if (reflowingFirstLetter) {
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
  else if (NS_FRAME_IS_TRUNCATED(frameReflowStatus)) {
    // if the frame is a placeholder and was complete but truncated (and not at the top
    // of page), the entire line will be pushed to give it another chance to not truncate.
    nsCOMPtr<nsIAtom> frameType;
    aFrame->GetFrameType(getter_AddRefs(frameType));
    if (nsLayoutAtoms::placeholderFrame == frameType) {
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
  if (NS_FAILED(rv)) {
    return rv;
  }
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
nsBlockFrame::SplitPlaceholder(nsIPresContext& aPresContext,
                               nsIFrame&       aPlaceholder)
{
  nsIFrame* nextInFlow;
  nsresult rv = CreateNextInFlow(&aPresContext, this, &aPlaceholder, nextInFlow);
  if (NS_FAILED(rv)) 
    return rv;
  // put the sibling list back to what it was before the continuation was created
  nsIFrame *contFrame, *next; 
  aPlaceholder.GetNextSibling(&contFrame);
  contFrame->GetNextSibling(&next);
  aPlaceholder.SetNextSibling(next);
  contFrame->SetNextSibling(nsnull);
  // add the placehoder to the overflow floaters
  nsFrameList* overflowPlace = GetOverflowPlaceholders(&aPresContext, PR_FALSE);
  if (overflowPlace) {
    overflowPlace->AppendFrames(this, contFrame);
  }
  else {
    overflowPlace = new nsFrameList(contFrame);
    if (overflowPlace) {
      SetOverflowPlaceholders(&aPresContext, overflowPlace);
    }
    else return NS_ERROR_NULL_POINTER;
  }
  return NS_OK;
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

PRBool
nsBlockFrame::PlaceLine(nsBlockReflowState& aState,
                        nsLineLayout&       aLineLayout,
                        line_iterator       aLine,
                        PRBool*             aKeepReflowGoing,
                        PRBool              aUpdateMaximumWidth)
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
  PRBool allowJustify = NS_STYLE_TEXT_ALIGN_JUSTIFY == styleText->mTextAlign &&
                        !aLineLayout.GetLineEndsInBR() &&
                        ShouldJustifyLine(aState, aLine);
  PRBool successful = aLineLayout.HorizontalAlignFrames(aLine->mBounds,
                            allowJustify, aState.GetFlag(BRS_SHRINKWRAPWIDTH));
  // XXX: not only bidi: right alignment can be broken after
  // RelativePositionFrames!!!
  // XXXldb Is something here considering relatively positioned frames at
  // other than their original positions?
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

  if (!IsLineEmpty(aState.mPresContext, aLine)) {
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
    // keep our ascent in sync
    // XXXldb If it's empty, shouldn't the next line control the ascent?
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
    return PR_TRUE;
  }

  aState.mY = newY;
  if (aState.GetFlag(BRS_COMPUTEMAXELEMENTSIZE)) {
#ifdef DEBUG
    if (gNoisyMaxElementSize) {
      IndentBy(stdout, gNoiseIndent);
      if (NS_UNCONSTRAINEDSIZE == aState.mReflowState.availableWidth) {
        printf("PASS1 ");
      }
      ListTag(stdout);
      printf(": line.floaters=%s band.floaterCount=%d\n",
             //aLine->mFloaters.NotEmpty() ? "yes" : "no",
             aState.mHaveRightFloaters ? "(have right floaters)" : "",
             aState.mBand.GetFloaterCount());
    }
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
#ifdef DEBUG
      if (gNoisyMaxElementSize) {
        IndentBy(stdout, gNoiseIndent);
        printf ("nsBlockFrame::PlaceLine: %p setting MES for line %p to %d\n", 
                this, NS_STATIC_CAST(void*, aLine.get()), maxElementSize.width);
      }
#endif
    }

  } else {
    PostPlaceLine(aState, aLine, maxElementSize);
  }

  // Add the already placed current-line floaters to the line
  aLine->AppendFloaters(aState.mCurrentLineFloaters);

  // Any below current line floaters to place?
  if (aState.mBelowCurrentLineFloaters.NotEmpty()) {
    // keep track of the last overflow floater in case we need to undo and push the line
    nsFrameList* overflowPlace = GetOverflowPlaceholders(aState.mPresContext, PR_FALSE);
    nsIFrame* lastPlaceholder = (overflowPlace) ? overflowPlace->LastChild() : nsnull;
    // Reflow the below-current-line floaters, then add them to the
    // lines floater list if there aren't any truncated floaters.
    if (aState.PlaceBelowCurrentLineFloaters(aState.mBelowCurrentLineFloaters)) {
      aLine->AppendFloaters(aState.mBelowCurrentLineFloaters);
    }
    else { 
      // At least one floater is truncated, so fix up any placeholders that got split and 
      // push the line. XXX It may be better to put the floater on the next line, but this 
      // is not common enough to justify the complexity.
      PushTruncatedPlaceholderLine(aState, aLine, lastPlaceholder, *aKeepReflowGoing);
    }
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

  return PR_FALSE;
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
#ifdef DEBUG
  if (gNoisyMaxElementSize) {
    IndentBy(stdout, gNoiseIndent);
    if (NS_UNCONSTRAINEDSIZE == aState.mReflowState.availableWidth) {
      printf("PASS1 ");
    }
    ListTag(stdout);
    printf(": maxFloaterSize=%d,%d\n", maxWidth, maxHeight);
  }
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
#ifdef DEBUG
  if (gNoisyMaxElementSize) {
    IndentBy(stdout, gNoiseIndent);
    printf ("nsBlockFrame::ComputeLineMaxElementSize: %p returning MES %d\n", 
            this, aMaxElementSize->width);
  }
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
    for (PRInt32 i = 0; (i < aLine->GetChildCount() && frame); i++) {
       ::PlaceFrameView(aState.mPresContext, frame);
       frame->GetNextSibling(&frame);
      NS_ASSERTION(frame || (i+1) == aLine->GetChildCount(),
        "Child count bigger than the number of linked frames!!!");
    }
  }

  // Update max-element-size
  if (aState.GetFlag(BRS_COMPUTEMAXELEMENTSIZE)) {
    aState.UpdateMaxElementSize(aMaxElementSize);
    // We also cache the max element width in the line. This is needed for
    // incremental reflow
    aLine->mMaxElementWidth = aMaxElementSize.width;
#ifdef DEBUG
    if (gNoisyMaxElementSize) {
      IndentBy(stdout, gNoiseIndent);
      printf ("nsBlockFrame::PostPlaceLine: %p setting line %p MES %d\n", 
              this, aLine, aMaxElementSize.width);
    }
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
nsBlockFrame::PushLines(nsBlockReflowState&  aState,
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
  nsLineList* lines = 
    NS_STATIC_CAST(nsLineList*, GetProperty(aPresContext, 
                                            nsLayoutAtoms::overflowLinesProperty, 
                                            aRemoveProperty));
  NS_ASSERTION(!lines || !lines->empty(), "value should never be stored as empty");
  return lines;
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
  NS_ASSERTION(aOverflowLines, "null lines");
  NS_ASSERTION(!aOverflowLines->empty(), "empty lines");

  nsresult rv = SetProperty(aPresContext, nsLayoutAtoms::overflowLinesProperty,
                            aOverflowLines, DestroyOverflowLines);
  // Verify that we didn't overwrite an existing overflow list
  NS_ASSERTION(rv != NS_IFRAME_MGR_PROP_OVERWRITTEN, "existing overflow list");
  return rv;
}

nsFrameList*
nsBlockFrame::GetOverflowPlaceholders(nsIPresContext* aPresContext,
                                      PRBool          aRemoveProperty) const
{
  nsFrameList* placeholders = 
    NS_STATIC_CAST(nsFrameList*, 
                   GetProperty(aPresContext, nsLayoutAtoms::overflowPlaceholdersProperty, 
                               aRemoveProperty));
  return placeholders;
}

// Destructor function for the overflowPlaceholders frame property
static void
DestroyOverflowPlaceholders(nsIPresContext* aPresContext,
                            nsIFrame*       aFrame,
                            nsIAtom*        aPropertyName,
                            void*           aPropertyValue)
{
  nsFrameList* overflowPlace = NS_STATIC_CAST(nsFrameList*, aPropertyValue);
  delete overflowPlace;
}

// This takes ownership of aOverflowLines.
// XXX We should allocate overflowLines from presShell arena!
nsresult
nsBlockFrame::SetOverflowPlaceholders(nsIPresContext* aPresContext,
                                      nsFrameList*    aOverflowPlaceholders)
{
  nsresult rv = SetProperty(aPresContext, nsLayoutAtoms::overflowPlaceholdersProperty,
                            aOverflowPlaceholders, DestroyOverflowPlaceholders);
  // Verify that we didn't overwrite an existing overflow list
  NS_ASSERTION(rv != NS_IFRAME_MGR_PROP_OVERWRITTEN, "existing overflow placeholder list");
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

  if (nsnull == aListName) {
    rv = DoRemoveFrame(aPresContext, aOldFrame);
  }
  else if (nsLayoutAtoms::absoluteList == aListName) {
    return mAbsoluteContainer.RemoveFrame(this, aPresContext, aPresShell,
                                          aListName, aOldFrame);
  }
  else if (nsLayoutAtoms::floaterList == aListName) {
    // Find which line contains the floater
    line_iterator line = begin_lines(), line_end = end_lines();
    for ( ; line != line_end; ++line) {
      if (line->IsInline() && line->RemoveFloater(aOldFrame)) {
        break;
      }
    }

    mFloaters.DestroyFrame(aPresContext, aOldFrame);

    // Mark every line at and below the line where the floater was dirty
    // XXXldb This could be done more efficiently.
    for ( ; line != line_end; ++line) {
      line->MarkDirty();
    }
  }
#ifdef IBMBIDI
  else if (nsLayoutAtoms::nextBidi == aListName) {
    // Skip the call to |ReflowDirtyChild| below by returning now.
    return DoRemoveFrame(aPresContext, aOldFrame);
  }
#endif // IBMBIDI
  else {
    rv = NS_ERROR_INVALID_ARG;
  }

  if (NS_SUCCEEDED(rv)) {
    // Ask the parent frame to reflow me.
    ReflowDirtyChild(&aPresShell, nsnull);  
  }
  return rv;
}

void
nsBlockFrame::DoRemoveOutOfFlowFrame(nsIPresContext* aPresContext,
                                     nsIFrame*       aFrame)
{
  // First remove aFrame's next in flow
  nsIFrame* nextInFlow;
  aFrame->GetNextInFlow(&nextInFlow);
  if (nextInFlow) {
    nsBlockFrame::DoRemoveOutOfFlowFrame(aPresContext, nextInFlow);
  }
  // Now remove aFrame
  const nsStyleDisplay* display = nsnull;
  aFrame->GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&)display);
  NS_ASSERTION(display, "program error");
  // find the containing block, this is either the parent or the grandparent
  // if the parent is an inline frame
  nsIFrame* parent;
  aFrame->GetParent(&parent); 
  nsCOMPtr<nsIAtom> parentType;
  parent->GetFrameType(getter_AddRefs(parentType));
  while (parent && (nsLayoutAtoms::blockFrame != parentType) &&
                   (nsLayoutAtoms::areaFrame  != parentType)) {
    parent->GetParent(&parent);
    parent->GetFrameType(getter_AddRefs(parentType));
  }
  if (!parent) {
    NS_ASSERTION(PR_FALSE, "null parent");
    return;
  }
  
  nsBlockFrame* block = (nsBlockFrame*)parent;
  // Remove aFrame from the appropriate list. 
  if (display->IsAbsolutelyPositioned()) {
    nsCOMPtr<nsIPresShell> presShell;
    aPresContext->GetShell(getter_AddRefs(presShell));
    block->mAbsoluteContainer.RemoveFrame(block, aPresContext, *presShell,
                                          nsLayoutAtoms::absoluteList, aFrame);
  }
  else {
    block->mFloaters.RemoveFrame(aFrame);
  }
  // Destroy aFrame
  aFrame->Destroy(aPresContext);
}

nsresult
nsBlockFrame::DoRemoveFrame(nsIPresContext* aPresContext,
                            nsIFrame* aDeletedFrame)
{
  nsFrameState state;
  aDeletedFrame->GetFrameState(&state);
  if (state & NS_FRAME_OUT_OF_FLOW) {
    DoRemoveOutOfFlowFrame(aPresContext, aDeletedFrame);
    return NS_OK;
  }

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
  if (line == line_end) 
    return NS_ERROR_FAILURE;

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

      // Update the child count of the line to be accurate
      PRInt32 lineChildCount = line->GetChildCount();
      lineChildCount--;
      line->SetChildCount(lineChildCount);

      // Destroy frame; capture its next-in-flow first in case we need
      // to destroy that too.
      nsIFrame* nextInFlow;
      aDeletedFrame->GetNextInFlow(&nextInFlow);
#ifdef NOISY_REMOVE_FRAME
      printf("DoRemoveFrame: line=%p frame=", line);
      nsFrame::ListTag(stdout, aDeletedFrame);
      printf(" prevSibling=%p nextInFlow=%p\n", prevSibling, nextInFlow);
#endif
      aDeletedFrame->Destroy(aPresContext);
      aDeletedFrame = nextInFlow;

      // If line is empty, remove it now
      if (0 == lineChildCount) {
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
        if (!lineCombinedArea.IsEmpty()) {
          Invalidate(aPresContext, lineCombinedArea);
        }
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
nsBlockFrame::DeleteNextInFlowChild(nsIPresContext* aPresContext,
                                    nsIFrame*       aNextInFlow)
{
  nsIFrame* prevInFlow;
  aNextInFlow->GetPrevInFlow(&prevInFlow);
  NS_PRECONDITION(prevInFlow, "bad next-in-flow");
  NS_PRECONDITION(IsChild(aPresContext, aNextInFlow), "bad geometric parent");

#ifdef IBMBIDI
  nsIFrame* nextBidi;
  prevInFlow->GetBidiProperty(aPresContext, nsLayoutAtoms::nextBidi,
                              (void**) &nextBidi,sizeof(nextBidi));
  if (nextBidi != aNextInFlow) {
#endif // IBMBIDI
  DoRemoveFrame(aPresContext, aNextInFlow);
#ifdef IBMBIDI
  }
#endif // IBMBIDI
}

////////////////////////////////////////////////////////////////////////
// Floater support

nsresult
nsBlockFrame::ReflowFloater(nsBlockReflowState& aState,
                            nsPlaceholderFrame* aPlaceholder,
                            nsRect&             aCombinedRectResult,
                            nsMargin&           aMarginResult,
                            nsMargin&           aComputedOffsetsResult,
                            nsReflowStatus&     aReflowStatus)
{
  // Delete the placeholder's next in flows, if any
  nsIFrame* nextInFlow;
  aPlaceholder->GetNextInFlow(&nextInFlow);
  if (nextInFlow) {
    // If aPlaceholder's parent is an inline, nextInFlow's will be a block.
    nsHTMLContainerFrame* parent;
    nextInFlow->GetParent((nsIFrame**)&parent);
    parent->DeleteNextInFlowChild(aState.mPresContext, nextInFlow);
  }
  // Reflow the floater.
  nsIFrame* floater = aPlaceholder->GetOutOfFlowFrame();
  aReflowStatus = NS_FRAME_COMPLETE;

#ifdef NOISY_FLOATER
  printf("Reflow Floater %p in parent %p, availSpace(%d,%d,%d,%d)\n",
          aPlaceholder->GetOutOfFlowFrame(), this, 
          aState.mAvailSpaceRect.x, aState.mAvailSpaceRect.y, 
          aState.mAvailSpaceRect.width, aState.mAvailSpaceRect.height
  );
#endif

  // Compute the available width. By default, assume the width of the
  // containing block.
  nscoord availWidth;
  if (aState.GetFlag(BRS_UNCONSTRAINEDWIDTH)) {
    availWidth = NS_UNCONSTRAINEDSIZE;
  }
  else {
    const nsStyleDisplay* floaterDisplay;
    floater->GetStyleData(eStyleStruct_Display,
                          (const nsStyleStruct*&)floaterDisplay);
    nsCompatibility mode;
    aState.mPresContext->GetCompatibilityMode(&mode);

    nsIFrame* prevInFlow;
    floater->GetPrevInFlow(&prevInFlow);
    // if the floater is continued, constrain its width to the prev-in-flow's width
    if (prevInFlow) {
      nsRect rect;
      prevInFlow->GetRect(rect);
      availWidth = rect.width;
    }
    else if (NS_STYLE_DISPLAY_TABLE != floaterDisplay->mDisplay ||
             eCompatibility_NavQuirks != mode ) {
      availWidth = aState.mContentArea.width;
    }
    else {
      // give tables only the available space
      // if they can shrink we may not be constrained to place
      // them in the next line
      availWidth = aState.mAvailSpaceRect.width;
      // round down to twips per pixel so that we fit
      // needed when prev. floater has procentage width
      // (maybe is a table flaw that makes table chose to round up
      // but i don't want to change that, too risky)
      nscoord twp;
      float p2t;
      aState.mPresContext->GetScaledPixelsToTwips(&p2t);
      twp = NSIntPixelsToTwips(1,p2t);
      availWidth -=  availWidth % twp;
    }
  }
  nscoord availHeight = ((NS_UNCONSTRAINEDSIZE == aState.mAvailSpaceRect.height) ||
                         (NS_UNCONSTRAINEDSIZE == aState.mContentArea.height))
                        ? NS_UNCONSTRAINEDSIZE 
                        : PR_MAX(0, aState.mContentArea.height - aState.mY);

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
                    availWidth, availHeight);

  // construct the html reflow state for the floater. ReflowBlock will 
  // initialize it and set its reason.
  nsHTMLReflowState floaterRS(aState.mPresContext, aState.mReflowState, floater, 
                              nsSize(availSpace.width, availSpace.height), 
                              aState.mReflowState.reason, PR_FALSE);
  // Setup a block reflow state to reflow the floater.
  nsBlockReflowContext brc(aState.mPresContext, aState.mReflowState,
                           computeMaxElementSize,
                           aState.GetFlag(BRS_COMPUTEMAXWIDTH));

  // Reflow the floater
  PRBool isAdjacentWithTop = aState.IsAdjacentWithTop();

  nsCollapsingMargin margin;
  nsresult rv = brc.ReflowBlock(availSpace, PR_TRUE, margin, isAdjacentWithTop, 
                                aComputedOffsetsResult, floaterRS, aReflowStatus);
  // An incomplete reflow status means we should split the floater 
  // if the height is constrained (bug 145305). 
  if (NS_FRAME_IS_NOT_COMPLETE(aReflowStatus) && (NS_UNCONSTRAINEDSIZE == availHeight)) 
    aReflowStatus = NS_FRAME_COMPLETE;

  if (NS_SUCCEEDED(rv) && isAutoWidth) {
    nscoord maxElementWidth = brc.GetMaxElementSize().width;
    if (maxElementWidth > availSpace.width) {
      // The floater's maxElementSize is larger than the available
      // width. Reflow it again, this time pinning the width to the
      // maxElementSize.
      availSpace.width = maxElementWidth;
      nsCollapsingMargin marginMES;
      // construct the html reflow state for the floater. 
      // ReflowBlock will initialize it and set its reason.
      nsHTMLReflowState redoFloaterRS(aState.mPresContext, aState.mReflowState, floater, 
                                      nsSize(availSpace.width, availSpace.height), 
                                      aState.mReflowState.reason, PR_FALSE);
      rv = brc.ReflowBlock(availSpace, PR_TRUE, marginMES, isAdjacentWithTop, 
                           aComputedOffsetsResult, redoFloaterRS, aReflowStatus);
    }
  }

  if (brc.BlockShouldInvalidateItself() && !mRect.IsEmpty()) {
    Invalidate(aState.mPresContext, mRect);
  }

  // Remove the floater from the reflow tree.
  if (aState.mReflowState.path)
    aState.mReflowState.path->RemoveChild(floater);

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
  // Pass floaterRS so the frame hierarchy can be used (redoFloaterRS has the same hierarchy)  
  floater->DidReflow(aState.mPresContext, &floaterRS, NS_FRAME_REFLOW_FINISHED);

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

  // If the placeholder was continued and its first-in-flow was followed by a 
  // <BR>, then cache the <BR>'s break type in aState.mFloaterBreakType so that
  // the next frame after the placeholder can combine that break type with its own
  nsIFrame* prevPlaceholder = nsnull;
  aPlaceholder->GetPrevInFlow(&prevPlaceholder);
  if (prevPlaceholder) {
    // the break occurs only after the last continued placeholder
    PRBool lastPlaceholder = PR_TRUE;
    nsIFrame* next;
    aPlaceholder->GetNextSibling(&next);
    if (next) {
      nsCOMPtr<nsIAtom> nextType;
      next->GetFrameType(getter_AddRefs(nextType));
      if (nsLayoutAtoms::placeholderFrame == nextType) {
        lastPlaceholder = PR_FALSE;
      }
    }
    if (lastPlaceholder) {
      // get the containing block of prevPlaceholder which is our prev-in-flow
      if (mPrevInFlow) {
        // get the break type of the last line in mPrevInFlow
        line_iterator endLine = --((nsBlockFrame*)mPrevInFlow)->end_lines();
        PRUint8 breakType = endLine->GetBreakType();
        if ((NS_STYLE_CLEAR_LEFT           == breakType) ||
            (NS_STYLE_CLEAR_RIGHT          == breakType) ||
            (NS_STYLE_CLEAR_LEFT_AND_RIGHT == breakType)) {
          aState.mFloaterBreakType = breakType;
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
      ::ComputeCombinedArea(mLines, mRect.width, mRect.height, ca);
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
    const nsStyleBorder* border = (const nsStyleBorder*)
      mStyleContext->GetStyleData(eStyleStruct_Border);
    const nsStylePadding* padding = (const nsStylePadding*)
      mStyleContext->GetStyleData(eStyleStruct_Padding);
    const nsStyleOutline* outline = (const nsStyleOutline*)
      mStyleContext->GetStyleData(eStyleStruct_Outline);

    // Paint background, border and outline
    nsRect rect(0, 0, mRect.width, mRect.height);
    nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                    aDirtyRect, rect, *border, *padding,
                                    0, 0);
    nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                aDirtyRect, rect, *border, mStyleContext,
                                skipSides);
    nsCSSRendering::PaintOutline(aPresContext, aRenderingContext, this,
                                 aDirtyRect, rect, *border, *outline,
                                 mStyleContext, 0);
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
    nsSpaceManager* sm = mSpaceManager;

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
      pos.mScrollViewStop = PR_FALSE;
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
      if (NS_POSITION_BEFORE_TABLE == result)
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

        rv = GetFrameForPointUsing(aPresContext, aPoint,
                                   nsLayoutAtoms::floaterList,
                                   NS_FRAME_PAINT_LAYER_FOREGROUND,
                                   PR_FALSE, aFrame);
        if (NS_OK == rv) {
          return NS_OK;
        }

        rv = GetFrameForPointUsing(aPresContext, aPoint,
                                   nsLayoutAtoms::floaterList,
                                   NS_FRAME_PAINT_LAYER_FLOATERS,
                                   PR_FALSE, aFrame);
        if (NS_OK == rv) {
          return NS_OK;
        }

        return GetFrameForPointUsing(aPresContext, aPoint,
                                     nsLayoutAtoms::floaterList,
                                     NS_FRAME_PAINT_LAYER_BACKGROUND,
                                     PR_FALSE, aFrame);

      } else {
        return NS_ERROR_FAILURE;
      }
      break;

    case NS_FRAME_PAINT_LAYER_BACKGROUND:
      // we're a block, so PR_TRUE for consider self
      return GetFrameForPointUsing(aPresContext, aPoint, nsnull,
                                   NS_FRAME_PAINT_LAYER_BACKGROUND,
                                   PR_TRUE, aFrame);
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
        nsHTMLReflowCommand* reflowCmd;
        nsresult          rv = NS_NewHTMLReflowCommand(&reflowCmd, this,
                                                       eReflowType_ReflowDirty);
        if (NS_SUCCEEDED(rv)) {
          reflowCmd->SetChildListName(nsLayoutAtoms::absoluteList);
          aPresShell->AppendReflowCommand(reflowCmd);
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
    line_iterator fline = FindLineFor(aChild);
    if (fline != end_lines())
      MarkLineDirty(fline);
  }

  // Either generate a reflow command to reflow the dirty child or 
  // coalesce this reflow request with an existing reflow command    
  if (!(mState & NS_FRAME_HAS_DIRTY_CHILDREN)) {
    // If this is the first dirty child, 
    // post a dirty children reflow command targeted at yourself
    mState |= NS_FRAME_HAS_DIRTY_CHILDREN;

    nsFrame::CreateAndPostReflowCommand(aPresShell, this, 
      eReflowType_ReflowDirty, nsnull, nsnull, nsnull);

#ifdef DEBUG
    if (gNoisyReflow) {
      IndentBy(stdout, gNoiseIndent);
      printf("scheduled reflow command targeted at self\n");
    }
#endif
  }

#ifdef DEBUG
  if (gNoisyReflow) {
    --gNoiseIndent;
  }
#endif
  
  return NS_OK;
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
  // Continued out-of-flows don't satisfy InLineList(), continued out-of-flows
  // and placeholders don't satisfy InSiblingList().  
  PRBool skipLineList    = PR_FALSE;
  PRBool skipSiblingList = PR_FALSE;
  nsIFrame* prevInFlow;
  aFrame->GetPrevInFlow(&prevInFlow);
  if (prevInFlow) {
    nsFrameState state;
    aFrame->GetFrameState(&state);
    nsCOMPtr<nsIAtom> frameType;
    aFrame->GetFrameType(getter_AddRefs(frameType));
    skipLineList    = (state & NS_FRAME_OUT_OF_FLOW); 
    skipSiblingList = (nsLayoutAtoms::placeholderFrame == frameType) ||
                      (state & NS_FRAME_OUT_OF_FLOW);
  }

  nsIFrame* parent;
  aFrame->GetParent(&parent);
  if (parent != (nsIFrame*)this) {
    return PR_FALSE;
  }
  if ((skipLineList || InLineList(mLines, aFrame)) && 
      (skipSiblingList || InSiblingList(mLines, aFrame))) {
    return PR_TRUE;
  }
  nsLineList* overflowLines = GetOverflowLines(aPresContext, PR_FALSE);
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

// End Debugging
//////////////////////////////////////////////////////////////////////

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
  // This check is here because nsComboboxControlFrame creates
  // nsBlockFrame objects that have an |mContent| pointing to a text
  // node.  This check ensures we don't try to do selector matching on
  // that text node.
  //
  // XXX This check should go away once we fix nsComboboxControlFrame.
  //
  if (!mContent->IsContentOfType(nsIContent::eELEMENT))
    return nsnull;

  nsIStyleContext* fls;
  aPresContext->ProbePseudoStyleContextFor(mContent,
                                           nsCSSPseudoElements::firstLetter,
                                           mStyleContext, &fls);
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
      const nsStyleList* styleList;
      GetStyleData(eStyleStruct_List, (const nsStyleStruct*&) styleList);
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
      nsIStyleContext* kidSC;
      aPresContext->ResolvePseudoStyleContextFor(mContent, pseudoElement,
                                                 mStyleContext, &kidSC);

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

  nsCOMPtr<nsIHTMLContent> hc(do_QueryInterface(mContent));

  if (hc) {
    nsHTMLValue value;
    if (NS_CONTENT_ATTR_HAS_VALUE ==
        hc->GetHTMLAttribute(nsHTMLAtoms::start, value)) {
      if (eHTMLUnit_Integer == value.GetUnit()) {
        ordinal = value.GetIntValue();
      }
    }
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

          nsRect damageRect;
          listItem->mBullet->GetRect(damageRect);
          damageRect.x = damageRect.y = 0;
          if (damageRect.width > 0 || damageRect.height > 0)
            listItem->mBullet->Invalidate(aPresContext, damageRect);
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

  // Get the reason right.
  // XXXwaterson Should this look just like the logic in
  // nsBlockReflowContext::ReflowBlock and nsLineLayout::ReflowFrame?
  const nsHTMLReflowState &rs = aState.mReflowState;
  nsReflowReason reason = rs.reason;
  if (reason == eReflowReason_Incremental) {
    if (! rs.path->HasChild(mBullet)) {
      // An incremental reflow not explicitly destined to (or through)
      // the child should be treated as a resize...
      reason = eReflowReason_Resize;
    }

    // ...unless its an incremental `style changed' reflow targeted at
    // the block, in which case, we propagate that to its children.
    nsHTMLReflowCommand *command = rs.path->mReflowCommand;
    if (command) {
      nsReflowType type;
      command->GetType(type);
      if (type == eReflowType_StyleChanged)
        reason = eReflowReason_StyleChange;
    }
  }

  nsHTMLReflowState reflowState(aState.mPresContext, rs,
                                mBullet, availSize, reason);
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
  mBullet->DidReflow(aState.mPresContext, &aState.mReflowState, NS_FRAME_REFLOW_FINISHED);
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

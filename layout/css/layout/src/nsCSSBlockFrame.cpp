/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include "nsCSSBlockFrame.h"
#include "nsCSSLineLayout.h"
#include "nsCSSInlineLayout.h"
#include "nsCSSLayout.h"
#include "nsPlaceholderFrame.h"
#include "nsStyleConsts.h"
#include "nsHTMLIIDs.h"

#include "nsIAnchoredItems.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIReflowCommand.h"
#include "nsIStyleContext.h"
#include "nsIView.h"

#include "nsHTMLBase.h"// XXX rename to nsCSSBase?
#include "nsHTMLParts.h"// XXX html reflow command???
#include "nsHTMLAtoms.h"// XXX list ordinal hack
#include "nsHTMLValue.h"// XXX list ordinal hack
#include "nsIHTMLContent.h"// XXX list ordinal hack

// XXX mLastContentOffset, mFirstContentOffset, mLastContentIsComplete
// XXX IsFirstChild
// XXX max-element-size
// XXX no-wrap
// XXX margin collapsing and empty InlineFrameData's
// XXX eliminate PutCachedData usage
// XXX floaters in inlines
// XXX Check handling of mUnconstrainedWidth
// XXX pagination
// XXX prev-in-flow continuations

// XXX MULTICOL support

// XXX better lists (bullet numbering)

//----------------------------------------------------------------------
// XXX It's really important that blocks strip out extra whitespace;
// otherwise we will see ALOT of this, which will waste memory big time:
//
//   <fd(inline - empty height because of compressed \n)>
//   <fd(block)>
//   <fd(inline - empty height because of compressed \n)>
//   <fd(block)>
//   ...
//----------------------------------------------------------------------

void nsBlockBandData::ComputeAvailSpaceRect()
{
  nsBandTrapezoid*  trapezoid = data;

  if (count > 1) {
    // If there's more than one trapezoid that means there are floaters
    PRInt32 i;

    // Stop when we get to space occupied by a right floater, or when we've
    // looked at every trapezoid and none are right floaters
    for (i = 0; i < count; i++) {
      nsBandTrapezoid*  trapezoid = &data[i];
      if (trapezoid->state != nsBandTrapezoid::Available) {
        const nsStyleDisplay* display;
        if (nsBandTrapezoid::OccupiedMultiple == trapezoid->state) {
          PRInt32 j, numFrames = trapezoid->frames->Count();
          NS_ASSERTION(numFrames > 0, "bad trapezoid frame list");
          for (j = 0; j < numFrames; j++) {
            nsIFrame* f = (nsIFrame*)trapezoid->frames->ElementAt(j);
            f->GetStyleData(eStyleStruct_Display,
                            (const nsStyleStruct*&)display);
            if (NS_STYLE_FLOAT_RIGHT == display->mFloats) {
              goto foundRightFloater;
            }
          }
        } else {
          trapezoid->frame->GetStyleData(eStyleStruct_Display,
                                         (const nsStyleStruct*&)display);
          if (NS_STYLE_FLOAT_RIGHT == display->mFloats) {
            break;
          }
        }
      }
    }
  foundRightFloater:

    if (i > 0) {
      trapezoid = &data[i - 1];
    }
  }

  if (nsBandTrapezoid::Available == trapezoid->state) {
    // The trapezoid is available
    trapezoid->GetRect(availSpace);
  } else {
    const nsStyleDisplay* display;

    // The trapezoid is occupied. That means there's no available space
    trapezoid->GetRect(availSpace);

    // XXX Handle the case of multiple frames
    trapezoid->frame->GetStyleData(eStyleStruct_Display,
                                   (const nsStyleStruct*&)display);
    if (NS_STYLE_FLOAT_LEFT == display->mFloats) {
      availSpace.x = availSpace.XMost();
    }
    availSpace.width = 0;
  }
}

//----------------------------------------------------------------------

nsCSSBlockReflowState::nsCSSBlockReflowState(nsIPresContext*      aPresContext,
                                             nsISpaceManager*     aSpaceManager,
                                             nsCSSContainerFrame* aBlock,
                                             const nsReflowState& aReflowState,
                                             nsSize*              aMaxElementSize)
  : nsReflowState(aBlock, *aReflowState.parentReflowState,
                  aReflowState.maxSize)
{
  mPresContext = aPresContext;
  mSpaceManager = aSpaceManager;
  mBlock = aBlock;
  mBlockIsPseudo = aBlock->IsPseudoFrame();
  aBlock->GetStyleContext(aPresContext, mBlockSC);
  mPrevPosBottomMargin = 0;
  mPrevNegBottomMargin = 0;
  mKidXMost = 0;

  mX = 0;
  mY = 0;
  mAvailSize = aReflowState.maxSize;
  mUnconstrainedWidth = PRBool(mAvailSize.width == NS_UNCONSTRAINEDSIZE);
  mUnconstrainedHeight = PRBool(mAvailSize.height == NS_UNCONSTRAINEDSIZE);
  mMaxElementSize = aMaxElementSize;
  if (nsnull != aMaxElementSize) {
    // XXX CAN'T pass our's to an inline child frame because it will
    // do the same thing...
    aMaxElementSize->width = 0;
    aMaxElementSize->height = 0;
  }

  // Set mNoWrap flag
  const nsStyleText* blockText = (const nsStyleText*)
    mBlockSC->GetStyleData(eStyleStruct_Text);
  switch (blockText->mWhiteSpace) {
  case NS_STYLE_WHITESPACE_PRE:
  case NS_STYLE_WHITESPACE_NOWRAP:
    mNoWrap = PR_TRUE;
    break;
  default:
    mNoWrap = PR_FALSE;
    break;
  }

  // Apply border and padding adjustments for regular frames only
  nsRect blockRect;
  mBlock->GetRect(blockRect);
  if (!mBlockIsPseudo) {
    const nsStyleSpacing* blockSpacing = (const nsStyleSpacing*)
      mBlockSC->GetStyleData(eStyleStruct_Spacing);

    blockSpacing->CalcBorderPaddingFor(mBlock, mBorderPadding);
    mY = mBorderPadding.top;
    mX = mBorderPadding.left;

    if (mUnconstrainedWidth) {
      // If our width is unconstrained don't bother futzing with the
      // available width/height because they don't matter - we are
      // going to get reflowed again.
      mDeltaWidth = NS_UNCONSTRAINEDSIZE;
    }
    else {
      // When we are constrained we need to apply the width/height
      // style properties. When we have a width/height it applies to
      // the content width/height of our box. The content width/height
      // doesn't include the border+padding so we have to add that in
      // instead of subtracting it out of our maxsize.
      nscoord lr = mBorderPadding.left + mBorderPadding.right;

      // Get and apply the stylistic size. Note: do not limit the
      // height until we are done reflowing.
      PRIntn ss = nsCSSLayout::GetStyleSize(aPresContext, aReflowState,
                                            mStyleSize);
      mStyleSizeFlags = ss;
      if (0 != (ss & NS_SIZE_HAS_WIDTH)) {
        mAvailSize.width = mStyleSize.width + lr;
      }
      else {
        mAvailSize.width -= lr;
      }
      mDeltaWidth = mAvailSize.width - blockRect.width;
    }
  }
  else {
    mBorderPadding.SizeTo(0, 0, 0, 0);
    mDeltaWidth = mAvailSize.width - blockRect.width;
  }

  mCurrentLine = nsnull;
  mChildPrevInFlow = nsnull;

  // Setup initial list ordinal value

  // XXX translate the starting value to a css style type and stop
  // doing this!
  mNextListOrdinal = -1;
  nsIContent* blockContent;
  mBlock->GetContent(blockContent);
  nsIAtom* tag = blockContent->GetTag();
  if ((tag == nsHTMLAtoms::ul) || (tag == nsHTMLAtoms::ol) ||
      (tag == nsHTMLAtoms::menu) || (tag == nsHTMLAtoms::dir)) {
    nsHTMLValue value;
    if (eContentAttr_HasValue ==
        ((nsIHTMLContent*)blockContent)->GetAttribute(nsHTMLAtoms::start,
                                                      value)) {
      if (eHTMLUnit_Integer == value.GetUnit()) {
        mNextListOrdinal = value.GetIntValue();
      }
    }
  }
  NS_RELEASE(blockContent);
}

nsCSSBlockReflowState::~nsCSSBlockReflowState()
{
  NS_RELEASE(mBlockSC);
}

// aY has borderpadding.top already factored in
void
nsCSSBlockReflowState::GetAvailableSpace()
{
  nsresult rv = NS_OK;

  nsISpaceManager* sm = mSpaceManager;

  // Fill in band data for the specific Y coordinate
  sm->Translate(mBorderPadding.left, mY);
  sm->GetBandData(0, mAvailSize, mCurrentBand);
  sm->Translate(-mBorderPadding.left, -mY);

  // Compute the bounding rect of the available space, i.e. space
  // between any left and right floaters
  mCurrentBand.ComputeAvailSpaceRect();
  mCurrentBand.availSpace.MoveBy(mBorderPadding.left, mY);

  NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
               ("nsCSSBlockReflowState::GetAvailableSpace: band={%d,%d,%d,%d}",
                mCurrentBand.availSpace.x,
                mCurrentBand.availSpace.y,
                mCurrentBand.availSpace.width,
                mCurrentBand.availSpace.height));
}

// Used when placing run-in floaters (floaters displayed at the top of
// the block as supposed to below the current line)
void
nsCSSBlockReflowState::PlaceCurrentLineFloater(nsIFrame* aFloater)
{
  nsISpaceManager* sm = mSpaceManager;

  // Get the type of floater
  const nsStyleDisplay* floaterDisplay;
  aFloater->GetStyleData(eStyleStruct_Display,
                         (const nsStyleStruct*&)floaterDisplay);
  const nsStyleSpacing* floaterSpacing;
  aFloater->GetStyleData(eStyleStruct_Spacing,
                         (const nsStyleStruct*&)floaterSpacing);

  // Commit some space in the space manager, and adjust our current
  // band of available space.
  nsRect region;
  aFloater->GetRect(region);
  region.y = mY;
  switch (floaterDisplay->mFloats) {
  default:
    NS_NOTYETIMPLEMENTED("Unsupported floater type");
    // FALL THROUGH

  case NS_STYLE_FLOAT_LEFT:
    region.x = mX;
    break;

  case NS_STYLE_FLOAT_RIGHT:
    region.x = mCurrentBand.availSpace.XMost() - region.width;
    if (region.x < 0) {
      region.x = 0;
    }
  }

  // Factor in the floaters margins
  nsMargin floaterMargin;
  floaterSpacing->CalcMarginFor(aFloater, floaterMargin);
  region.width += floaterMargin.left + floaterMargin.right;
  region.height += floaterMargin.top + floaterMargin.bottom;
  sm->AddRectRegion(aFloater, region);

  // Set the origin of the floater in world coordinates
  nscoord worldX, worldY;
  sm->GetTranslation(worldX, worldY);
  aFloater->MoveTo(region.x + worldX + floaterMargin.left,
                   region.y + worldY + floaterMargin.top);

  // Update the band of available space to reflect space taken up by
  // the floater
  GetAvailableSpace();

  // XXX if the floater is a child of an inline frame then this won't
  // work because we won't update the correct nsCSSInlineLayout.
  nsCSSInlineLayout* inlineLayout = mCurrentLine;
  if (nsnull != inlineLayout) {
    inlineLayout->SetReflowSpace(mCurrentBand.availSpace.x,
                                 mY,
                                 mCurrentBand.availSpace.width,
                                 mCurrentBand.availSpace.height);
  }
}

void
nsCSSBlockReflowState::PlaceBelowCurrentLineFloaters(nsVoidArray* aFloaterList)
{
  NS_PRECONDITION(aFloaterList->Count() > 0, "no floaters");

  nsISpaceManager* sm = mSpaceManager;
  nsBlockBandData* bd = &mCurrentBand;

  // XXX Factor this code with PlaceFloater()...
  nsRect region;
  PRInt32 numFloaters = aFloaterList->Count();
  for (PRInt32 i = 0; i < numFloaters; i++) {
    nsPlaceholderFrame* placeholderFrame = (nsPlaceholderFrame*)
      aFloaterList->ElementAt(i);
    nsIFrame* floater = placeholderFrame->GetAnchoredItem();

    // Get the band of available space
    // XXX This is inefficient to do this inside the loop...
    GetAvailableSpace();

    // Get the type of floater
    const nsStyleDisplay* floaterDisplay;
    const nsStyleSpacing* floaterSpacing;
    floater->GetStyleData(eStyleStruct_Display,
                          (const nsStyleStruct*&)floaterDisplay);
    floater->GetStyleData(eStyleStruct_Spacing,
                          (const nsStyleStruct*&)floaterSpacing);

    floater->GetRect(region);
    region.y = bd->availSpace.y;
    switch (floaterDisplay->mFloats) {
    default:
      NS_NOTYETIMPLEMENTED("Unsupported floater type");
      // FALL THROUGH

    case NS_STYLE_FLOAT_LEFT:
      region.x = bd->availSpace.x;
      break;

    case NS_STYLE_FLOAT_RIGHT:
      region.x = bd->availSpace.XMost() - region.width;
      if (region.x < 0) {
        region.x = 0;
      }
    }

    // XXX Temporary incremental hack (kipp asks: why is this
    // temporary or an incremental hack?)

    // Factor in the floaters margins
    nsMargin floaterMargin;
    floaterSpacing->CalcMarginFor(floater, floaterMargin);
    region.width += floaterMargin.left + floaterMargin.right;
    region.height += floaterMargin.top + floaterMargin.bottom;
    sm->RemoveRegion(floater);/* XXX temporary code */
    sm->AddRectRegion(floater, region);

    // Set the origin of the floater in world coordinates
    nscoord worldX, worldY;
    sm->GetTranslation(worldX, worldY);
    floater->MoveTo(region.x + worldX + floaterMargin.left,
                    region.y + worldY + floaterMargin.top);
  }

  // Pass on updated available space to the current line
  GetAvailableSpace();
  if (nsnull != mCurrentLine) {
    mCurrentLine->SetReflowSpace(bd->availSpace.x, mY,
                                 bd->availSpace.width,
                                 bd->availSpace.height);
  }
}

void
nsCSSBlockReflowState::ClearFloaters(PRUint8 aBreakType)
{
  for (;;) {
    if (mCurrentBand.count <= 1) {
      // No floaters in this band therefore nothing to clear
      break;
    }

    // Find the Y coordinate to clear to
    nscoord clearYMost = mY;
    nsRect tmp;
    PRInt32 i;
    for (i = 0; i < mCurrentBand.count; i++) {
      const nsStyleDisplay* display;
      nsBandTrapezoid* trapezoid = &mCurrentBand.data[i];
      if (trapezoid->state != nsBandTrapezoid::Available) {
        if (nsBandTrapezoid::OccupiedMultiple == trapezoid->state) {
          PRInt32 fn, numFrames = trapezoid->frames->Count();
          NS_ASSERTION(numFrames > 0, "bad trapezoid frame list");
          for (fn = 0; fn < numFrames; fn++) {
            nsIFrame* f = (nsIFrame*) trapezoid->frames->ElementAt(fn);
            f->GetStyleData(eStyleStruct_Display,
                            (const nsStyleStruct*&)display);
            switch (display->mFloats) {
            case NS_STYLE_FLOAT_LEFT:
              if ((NS_STYLE_CLEAR_LEFT == aBreakType) ||
                  (NS_STYLE_CLEAR_LEFT_AND_RIGHT == aBreakType)) {
                trapezoid->GetRect(tmp);
                nscoord ym = tmp.YMost();
                if (ym > clearYMost) {
                  clearYMost = ym;
                }
              }
              break;
            case NS_STYLE_FLOAT_RIGHT:
              if ((NS_STYLE_CLEAR_RIGHT == aBreakType) ||
                  (NS_STYLE_CLEAR_LEFT_AND_RIGHT == aBreakType)) {
                trapezoid->GetRect(tmp);
                nscoord ym = tmp.YMost();
                if (ym > clearYMost) {
                  clearYMost = ym;
                }
              }
              break;
            }
          }
        }
        else {
          trapezoid->frame->GetStyleData(eStyleStruct_Display,
                                         (const nsStyleStruct*&)display);
          switch (display->mFloats) {
          case NS_STYLE_FLOAT_LEFT:
            if ((NS_STYLE_CLEAR_LEFT == aBreakType) ||
                (NS_STYLE_CLEAR_LEFT_AND_RIGHT == aBreakType)) {
              trapezoid->GetRect(tmp);
              nscoord ym = tmp.YMost();
              if (ym > clearYMost) {
                clearYMost = ym;
              }
            }
            break;
          case NS_STYLE_FLOAT_RIGHT:
            if ((NS_STYLE_CLEAR_RIGHT == aBreakType) ||
                (NS_STYLE_CLEAR_LEFT_AND_RIGHT == aBreakType)) {
              trapezoid->GetRect(tmp);
              nscoord ym = tmp.YMost();
              if (ym > clearYMost) {
                clearYMost = ym;
              }
            }
            break;
          }
        }
      }
    }

    if (clearYMost == mY) {
      // Nothing to clear
      break;
    }

    NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
                 ("nsCSSBlockReflowState::ClearFloaters: mY=%d clearYMost=%d",
                  mY, clearYMost));

    mY = clearYMost + 1;

    // Get a new band
    GetAvailableSpace();
  }
}

//----------------------------------------------------------------------

// XXX Keep a floater impacted list that binds a floater to a given
// collection of FrameData's that are affected by the floater.

// FrameData represents a run of child frames that are either all
// inline frames or all block frames. Block frames are reflowed simply
// by vertically stacking them after sizing them. Inline frames use
// the nsCSSLineLayout engine for their more complex placement.
struct FrameData {
  FrameData() {
    mNext = nsnull;
  }

  virtual ~FrameData();

  virtual PRBool IsBlock() const = 0;

  virtual PRBool Contains(nsIFrame* aChild) const = 0;

  virtual nsresult ResizeReflow(nsCSSBlockReflowState& aState) = 0;

  virtual nscoord GetXMost() = 0;

  virtual nscoord GetYMost() = 0;

  virtual void MoveChildren(nscoord dx, nscoord dy) = 0;

  virtual void List(FILE* out, PRInt32 aIndent) = 0;

  virtual void DeleteNextInFlowFrame(nsIFrame* aDeadFrame) = 0;

  static FrameData* FindFrameDataFor(FrameData* fp, nsIFrame* aChild);

  FrameData* mNext;
};

FrameData::~FrameData()
{
}

FrameData*
FrameData::FindFrameDataFor(FrameData* fp, nsIFrame* aChild)
{
  while (nsnull != fp) {
    if (fp->Contains(aChild)) {
      return fp;
    }
    fp = fp->mNext;
  }
  return nsnull;
}

//----------------------------------------------------------------------

// dirty bit for BlockFrameData, InlineFrameData
#define FRAME_DATA_DIRTY 0x1

struct BlockFrameData : public FrameData {
  BlockFrameData(nsIFrame* aChild) {
    mChild = aChild;
  }

  virtual ~BlockFrameData();

  virtual PRBool IsBlock() const {
    return PR_TRUE;
  }

  virtual PRBool Contains(nsIFrame* aChild) const {
    return PRBool(aChild == mChild);
  }

  virtual nsresult ResizeReflow(nsCSSBlockReflowState& aState) {
    return Reflow(aState, eReflowReason_Resize);
  }

  virtual nscoord GetXMost() {
    return mBounds.XMost();
  }

  virtual nscoord GetYMost() {
    return mBounds.YMost();
  }

  virtual void MoveChildren(nscoord dx, nscoord dy);

  virtual void List(FILE* out, PRInt32 aIndent);

  virtual void DeleteNextInFlowFrame(nsIFrame* aDeadFrame) {
    // Block frame's that are continued will have a different
    // parent??? What about MULTICOL!!!
  }

  void MarkDirty() {
    mFlags |= FRAME_DATA_DIRTY;
  }

  nsresult Reflow(nsCSSBlockReflowState& aState, nsReflowReason aReason);

  nsIFrame* mChild;
  //XXX yes? nscoord mY, mWidth, mHeight;
  nsRect mBounds;
  PRUint16 mFlags;
};

BlockFrameData::~BlockFrameData()
{
}

nsresult
BlockFrameData::Reflow(nsCSSBlockReflowState& aState, nsReflowReason aReason)
{
  // Get the block's margins
  nsMargin blockMargin;
  const nsStyleSpacing* blockSpacing;
  mChild->GetStyleData(eStyleStruct_Spacing,
                       (const nsStyleStruct*&)blockSpacing);
  blockSpacing->CalcMarginFor(mChild, blockMargin);

  // XXX ebina margins...
  nscoord posTopMargin, negTopMargin;
  if (blockMargin.top < 0) {
    negTopMargin = -blockMargin.top;
    posTopMargin = 0;
  }
  else {
    negTopMargin = 0;
    posTopMargin = blockMargin.top;
  }
  nscoord prevPos = aState.mPrevPosBottomMargin;
  nscoord prevNeg = aState.mPrevNegBottomMargin;
  nscoord maxPos = PR_MAX(prevPos, posTopMargin);
  nscoord maxNeg = PR_MAX(prevNeg, negTopMargin);
  nscoord topMargin = maxPos - maxNeg;

  // Remember bottom margin for next time
  if (blockMargin.bottom < 0) {
    aState.mPrevNegBottomMargin = -blockMargin.bottom;
    aState.mPrevPosBottomMargin = 0;
  }
  else {
    aState.mPrevNegBottomMargin = 0;
    aState.mPrevPosBottomMargin = blockMargin.bottom;
  }

  nscoord x = aState.mX + blockMargin.left;
  nscoord y = aState.mY + topMargin;
  mChild->WillReflow(*aState.mPresContext);
  mChild->MoveTo(x, y);

  // Reflow the block frame. Use the run-around API if possible;
  // otherwise treat it as a rectangular lump and place it.
  nsresult rv;
  nsReflowMetrics metrics(aState.mMaxElementSize);/* XXX mMaxElementSize*/
  nsReflowStatus reflowStatus;
  nsIRunaround* runAround;
  if ((nsnull != aState.mSpaceManager) &&
      (NS_OK == mChild->QueryInterface(kIRunaroundIID, (void**)&runAround))) {
    // Compute the available space that the child block can reflow into
    nsSize availSize;
    if (aState.mUnconstrainedWidth) {
      // Never give a block an unconstrained width
      availSize.width = aState.maxSize.width;
    }
    else {
      availSize.width = aState.mAvailSize.width -
        (blockMargin.left + blockMargin.right);
    }
    availSize.height = aState.mAvailSize.height;

    // Reflow the block
    nsReflowState reflowState(mChild, aState, availSize);
    reflowState.reason = aReason;
    nsRect r;
    rv = runAround->Reflow(aState.mPresContext, aState.mSpaceManager,
                           metrics, reflowState, r, reflowStatus);
    metrics.width = r.width;
    metrics.height = r.height;
    metrics.ascent = r.height;
    metrics.descent = 0;
  }
  else {
    // Compute the available space that the child block can reflow into
    nsSize availSize;
    if (aState.mUnconstrainedWidth) {
      // Never give a block an unconstrained width
      availSize.width = aState.maxSize.width;
    }
    else {
      availSize.width = aState.mAvailSize.width -
        (blockMargin.left + blockMargin.right);
    }
    availSize.height = aState.mAvailSize.height;

    nsReflowState reflowState(mChild, aState, availSize);
    reflowState.reason = aReason;
    rv = mChild->Reflow(aState.mPresContext, metrics, reflowState,
                        reflowStatus);
  }
  if (IS_REFLOW_ERROR(rv)) {
    return rv;
  }

  if (NS_FRAME_IS_COMPLETE(reflowStatus)) {
    nsIFrame* kidNextInFlow;
    mChild->GetNextInFlow(kidNextInFlow);
    if (nsnull != kidNextInFlow) {
      // Remove all of the childs next-in-flows. Make sure that we ask
      // the right parent to do the removal (it's possible that the
      // parent is not this because we are executing pullup code)
      nsCSSContainerFrame* parent;
      mChild->GetGeometricParent((nsIFrame*&)parent);
      parent->DeleteChildsNextInFlow(mChild);
    }
  }

  // XXX See if it fit when we are doing pagination

  // Save away bounds before other adjustments
  mBounds.x = x;
  mBounds.y = y;
  mBounds.width = metrics.width;
  mBounds.height = metrics.height;

  mChild->SetRect(mBounds);
  aState.mY = y + metrics.height;

  // Align the block within ourselves
  if (!aState.mUnconstrainedWidth) {
#if XXX_fix_me
    nsCSSLayout::HorizontallyPlaceChildren(aState.mPresContext,
                                           mBlock,
                                           mBlockText->mTextAlign,
                                           mDirection,
                                           mChild, 1,
                                           mX - mLeftEdge,
                                           mAvailWidth);
#endif
  }

  // Get location of block before relative positioning
  nsCSSLayout::RelativePositionChildren(aState.mPresContext,
                                        aState.mBlock,
                                        mChild, 1);

  nscoord xmost = mBounds.XMost();
  if (xmost > aState.mKidXMost) {
    aState.mKidXMost = xmost;
  }

  return NS_FRAME_COMPLETE;
}

void
BlockFrameData::MoveChildren(nscoord dx, nscoord dy)
{
  nsPoint oldOrigin;
  mChild->GetOrigin(oldOrigin);
  mChild->MoveTo(oldOrigin.x + dx, oldOrigin.y + dy);
}

void
BlockFrameData::List(FILE* out, PRInt32 aIndent)
{
  PRInt32 i;
  for (i = aIndent; --i >= 0; ) fputs("  ", out);

  fprintf(out, "block flags=%x ", mFlags);
  out << mBounds;
  fputs("<\n", out);

  mChild->List(out, aIndent + 1);

  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  fputs(">\n", out);
}

//----------------------------------------------------------------------

struct InlineLineData {
  InlineLineData(nsIFrame* aChild, PRInt32 aCount) {
    mFirstChild = aChild;
    mChildCount = aCount;
    mFlags = FRAME_DATA_DIRTY;
    mBounds.SetRect(0, 0, 0, 0);
    mFloaters = nsnull;
    mNext = nsnull;
  }

  nsIFrame* mFirstChild;
  PRUint16 mFlags;
  PRUint16 mChildCount;
  nsRect mBounds;
  nsVoidArray* mFloaters;
  InlineLineData* mNext;
};

struct InlineFrameData : public FrameData {
  InlineFrameData(nsIFrame* aFirstChild)
    : mLines(aFirstChild, 1)
  {
    mTextRuns = nsnull;
  }

  virtual ~InlineFrameData();

  virtual PRBool IsBlock() const {
    return PR_FALSE;
  }

  // XXX this loses because ild is lost
  virtual PRBool Contains(nsIFrame* aChild) const {
    const InlineLineData* ild = &mLines;
    while (nsnull != ild) {
      nsIFrame* child = ild->mFirstChild;
      PRInt32 n = ild->mChildCount;
      while (--n >= 0) {
        if (child == aChild) {
          return PR_TRUE;
        }
        child->GetNextSibling(child);
      }
      ild = ild->mNext;
    }
    return PR_FALSE;
  }

  virtual nsresult ResizeReflow(nsCSSBlockReflowState& aState);

  virtual nscoord GetXMost();

  virtual nscoord GetYMost();

  virtual void MoveChildren(nscoord dx, nscoord dy);

  virtual void List(FILE* out, PRInt32 aIndent);

  virtual void DeleteNextInFlowFrame(nsIFrame* aDeadFrame);

  InlineLineData* LastInlineLineData() const;

  void MarkDirty() {
    mLines.mFlags |= FRAME_DATA_DIRTY;
  }

  nsresult FrameAppendedReflow(nsCSSBlockReflowState& aState,
                               InlineLineData* ild);

  PRInt32 TotalChildCount() const;

  nsresult FindTextRuns(nsCSSLineLayout& lineLayout);

  nsresult PlaceLine(nsCSSBlockReflowState&    aState,
                     nsCSSInlineLayout&   aInlineLayout,
                     InlineLineData*      ild,
                     nsInlineReflowStatus aReflowStatus);

  nsresult ReflowLine(nsCSSBlockReflowState& aState,
                      nsCSSLineLayout&  aLineLayout,
                      InlineLineData*   ild);

  nsresult MaybeCreateNextInFlow(nsCSSBlockReflowState&  aState,
                                 nsCSSInlineLayout& aInlineLayout,
                                 InlineLineData*    ild,
                                 nsIFrame*          aFrame);

  nsresult SplitLine(nsCSSBlockReflowState&  aState,
                     nsCSSInlineLayout& aInlineLayout,
                     InlineLineData*    ild,
                     nsIFrame*          aFrame);
#ifdef NS_DEBUG
  PRInt32 GetLineNumber(InlineLineData* aLine) {
    PRInt32 n = 0;
    InlineLineData* ild = &mLines;
    while (ild != aLine) {
      ild = ild->mNext;
      n++;
    }
    return n;
  }
#endif

  // Note: first InlineLineData is embedded in this structure
  InlineLineData mLines;

  nsCSSTextRun* mTextRuns;
};

InlineFrameData::~InlineFrameData()
{
  // XXX write me
}

InlineLineData*
InlineFrameData::LastInlineLineData() const
{
  InlineLineData* ild = (InlineLineData*) &mLines;
  while (nsnull != ild->mNext) {
    ild = ild->mNext;
  }
  return ild;
}

PRInt32
InlineFrameData::TotalChildCount() const
{
  PRInt32 sum = 0;
  const InlineLineData* ild = &mLines;
  while (nsnull != ild) {
    sum += ild->mChildCount;
    ild = ild->mNext;
  }
  return sum;
}

nsresult
InlineFrameData::FindTextRuns(nsCSSLineLayout& lineLayout)
{
  if (nsnull != mTextRuns) {
    delete mTextRuns;
    mTextRuns = nsnull;
  }

  // Find all the text runs in all of our children, recursively
  // through containers that implement nsIInlineReflow.
  lineLayout.EndTextRun();
  nsresult rv = NS_OK;
  InlineLineData* ild = &mLines;
  while (nsnull != ild) {
    nsIFrame* child = ild->mFirstChild;
    PRInt32 i, n = ild->mChildCount;
    for (i = 0; i < n; i++) {
      nsIInlineReflow* inlineReflow;
      if (NS_OK == child->QueryInterface(kIInlineReflowIID,
                                         (void**)&inlineReflow)) {
        rv = inlineReflow->FindTextRuns(lineLayout);
        if (NS_OK != rv) {
          return rv;
        }
      }
      else {
        lineLayout.EndTextRun();
      }
      child->GetNextSibling(child);
    }
    ild = ild->mNext;
  }

  mTextRuns = lineLayout.TakeTextRuns();
  return rv;
}

nscoord
InlineFrameData::GetXMost()
{
  nscoord xmost = 0;
  InlineLineData* ild = &mLines;
  do {
    nscoord xm = ild->mBounds.XMost();
    if (xm > xmost) {
      xmost = xm;
    }
    ild = ild->mNext;
  } while (nsnull != ild);
  return xmost;
}

nscoord
InlineFrameData::GetYMost()
{
  InlineLineData* ild = LastInlineLineData();
  return ild->mBounds.YMost();
}

void
InlineFrameData::MoveChildren(nscoord dx, nscoord dy)
{
  nsPoint oldOrigin;
  InlineLineData* ild = &mLines;
  do {
    nsIFrame* frame = ild->mFirstChild;
    PRInt32 n = ild->mChildCount;
    while (--n >= 0) {
      frame->GetOrigin(oldOrigin);
      frame->MoveTo(oldOrigin.x + dx, oldOrigin.y + dy);
      frame->GetNextSibling(frame);
    }
    ild = ild->mNext;
  } while (nsnull != ild);
}

void
InlineFrameData::List(FILE* out, PRInt32 aIndent)
{
  PRInt32 i;
  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  fputs("inline lines [\n", out);

  aIndent++;
  InlineLineData* ild = &mLines;
  PRInt32 lineNumber = 0;
  do {
    for (i = aIndent; --i >= 0; ) fputs("  ", out);
    fprintf(out, "line %d flags=%x count=%d ", lineNumber, ild->mFlags,
            ild->mChildCount);
    out << ild->mBounds;
    fputs("<\n", out);
    nsIFrame* child = ild->mFirstChild;
    PRInt32 n = ild->mChildCount;
    while (--n >= 0) {
      child->List(out, aIndent + 1);
      child->GetNextSibling(child);
    }
    for (i = aIndent; --i >= 0; ) fputs("  ", out);
    fputs(">\n", out);
    nsVoidArray* fa = ild->mFloaters;
    if (nsnull != fa) {
      for (i = aIndent; --i >= 0; ) fputs("  ", out);
      n = fa->Count();
      fprintf(out, "line %d floaters=%d <\n", lineNumber, n);
      for (i = 0; i < n; i++) {
        nsIFrame* floater = (nsIFrame*) fa->ElementAt(i);
        floater->List(out, aIndent + 1);
      }
      for (i = aIndent; --i >= 0; ) fputs("  ", out);
      fputs(">\n", out);
    }
    lineNumber++;
    ild = ild->mNext;
  } while (nsnull != ild);
  aIndent--;
  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  fputs("]\n", out);

  if (nsnull != mTextRuns) {
    for (i = aIndent; --i >= 0; ) fputs("  ", out);
    fputs("inline text-runs [\n", out);
    nsCSSTextRun* run = mTextRuns;
    while (nsnull != run) {
      run->List(out, aIndent + 1);
      run = run->mNext;
    }
    for (i = aIndent; --i >= 0; ) fputs("  ", out);
    fputs("]\n", out);
  }
}

void
InlineFrameData::DeleteNextInFlowFrame(nsIFrame* aDeadFrame)
{
  InlineLineData* ild = &mLines;
  do {
    if (ild->mFirstChild == aDeadFrame) {
      if (0 == --ild->mChildCount) {
        ild->mFirstChild = nsnull;
      }
      else {
        aDeadFrame->GetNextSibling(ild->mFirstChild);
      }
      break;
    }
    ild = ild->mNext;
  } while (nsnull != ild);
}

nsresult
InlineFrameData::ResizeReflow(nsCSSBlockReflowState& aState)
{
  nsresult rv = NS_FRAME_COMPLETE;

  // TextRun data was already computed last time; therefore just use
  // it recompute the frames on each InlineLineData.
  nsCSSLineLayout lineLayout(aState.mPresContext, aState.mSpaceManager);

  // Reflow the physical lines
  InlineLineData* ild = &mLines;
  while (nsnull != ild) {
    // Reflow the line
    rv = ReflowLine(aState, lineLayout, ild);
    if (IS_REFLOW_ERROR(rv)) {
      return rv;
    }
    // XXX check for out of vertical space for pagination

    ild = ild->mNext;
  }

  return rv;
}

nsresult
InlineFrameData::FrameAppendedReflow(nsCSSBlockReflowState& aState,
                                     InlineLineData*   ild)
{
  nsCSSLineLayout lineLayout(aState.mPresContext, aState.mSpaceManager);

  nsresult rv = FindTextRuns(lineLayout);
  if (NS_OK != rv) {
    return rv;
  }

  // XXX use ild to pick up, when possible, where we left off; this is
  // non-trivial considering that an appended piece of content may end
  // up being joined to a preceeding piece of content because of
  // line-breaking behavior.

  // Now reflow the world
  rv = ResizeReflow(aState);

  return rv;
}

//----------------------------------------

//XXX this gets passed an nsCSSLineLayout object because I'm assuming
//(For now) that there will be carry-forward state in it.

nsresult
InlineFrameData::ReflowLine(nsCSSBlockReflowState& aState,
                            nsCSSLineLayout& aLineLayout,
                            InlineLineData* ild)
{
  // If a previous block frame left behind a lingering bottom margin,
  // apply it now. We may undo this if the line ends up being zero
  // sized.

  // XXX two problems here: incremental reflow won't do it right and
  // we need to undo getting the band on failure...
  nscoord bottomMargin = aState.mPrevPosBottomMargin -
    aState.mPrevNegBottomMargin;
  if (ild == &mLines) {
    // Before we place the first inline child on this line apply
    // the previous block's bottom margin.
    aState.mY += bottomMargin;
    if (aState.mY >= aState.mCurrentBand.availSpace.YMost()) {
      aState.GetAvailableSpace();
    }
  }

  // Prepare for reflowing this line
  aLineLayout.Prepare(aState.mX);
  nsCSSInlineLayout inlineLayout(aLineLayout, aState.mBlock, aState.mBlockSC);
  inlineLayout.Init(&aState);
  inlineLayout.Prepare(aState.mUnconstrainedWidth, aState.mNoWrap,
                       aState.mMaxElementSize);
  inlineLayout.SetReflowSpace(aState.mCurrentBand.availSpace.x,
                              aState.mY,
                              aState.mCurrentBand.availSpace.width,
                              aState.mCurrentBand.availSpace.height);

  // XXX yuck; and there are lots of returns below that don't undo this!
  aState.mCurrentLine = &inlineLayout;

  // Reflow each child on this line
  nsInlineReflowStatus rs = NS_FRAME_COMPLETE;
  nsresult rv;
  nsIFrame* prevChild = nsnull;
  nsIFrame* child = ild->mFirstChild;
  PRInt32 n = ild->mChildCount;
  while (--n >= 0) {
    rs = inlineLayout.ReflowAndPlaceFrame(child);
    if (IS_REFLOW_ERROR(rs)) {
      return nsresult(rs);
    }
    switch (rs & NS_INLINE_REFLOW_REFLOW_MASK) {
    case NS_INLINE_REFLOW_COMPLETE:
      prevChild = child;
      child->GetNextSibling(child);
      aState.mChildPrevInFlow = nsnull;
      break;

    case NS_INLINE_REFLOW_NOT_COMPLETE:
      // Create continuation frame (if necessary); add it to the end
      // of this InlineLineData
      MaybeCreateNextInFlow(aState, inlineLayout, ild, child);
      // FALL THROUGH

    case NS_INLINE_REFLOW_BREAK_AFTER:
      aState.mChildPrevInFlow = nsnull;
      prevChild = child;
      child->GetNextSibling(child);
      // FALL THROUGH

    case NS_INLINE_REFLOW_BREAK_BEFORE:
      aState.mChildPrevInFlow = nsnull;
      rv = SplitLine(aState, inlineLayout, ild, child);
      if (IS_REFLOW_ERROR(rv)) {
        return rv;
      }
      goto done;
    }
  }

  // Pullup children from next line(s) to this line
  // XXX factor this better while still keeping it fast
  while (nsnull != ild->mNext) {
    InlineLineData* nextLine = ild->mNext;
    PRInt32 i, n = nextLine->mChildCount;
    for (i = 0; i < n; i++) {
      nsIFrame* nextKid;
      prevChild->GetNextSibling(nextKid);
      NS_ASSERTION(nextKid == nextLine->mFirstChild, "bad lines");

      // Get first child from the next line and try reflowing and placing it
      child = nextLine->mFirstChild;
      rs = inlineLayout.ReflowAndPlaceFrame(child);
      if (IS_REFLOW_ERROR(rs)) {
        return nsresult(rs);
      }
      switch (rs & NS_INLINE_REFLOW_REFLOW_MASK) {
      case NS_INLINE_REFLOW_COMPLETE:
        // Pullup succeeded
        if (0 == ild->mChildCount) {
          ild->mFirstChild = child;
        }
        ild->mChildCount++;
        if (--nextLine->mChildCount == 0) {
          ild->mNext = nextLine->mNext;
          delete nextLine;
          nextLine = nsnull;
        }
        else {
          child->GetNextSibling(nextLine->mFirstChild);
        }
        prevChild = child;
        aState.mChildPrevInFlow = nsnull;
        break;

      case NS_INLINE_REFLOW_BREAK_AFTER:
        // Pullup succeeded and we are done
        if (0 == ild->mChildCount) {
          ild->mFirstChild = child;
        }
        ild->mChildCount++;
        if (--nextLine->mChildCount == 0) {
          ild->mNext = nextLine->mNext;
          delete nextLine;
          nextLine = nsnull;
        }
        else {
          child->GetNextSibling(nextLine->mFirstChild);
        }
        aState.mChildPrevInFlow = nsnull;
        goto done;

      case NS_INLINE_REFLOW_BREAK_BEFORE:
        // The child can't fit on this line so never mind
        aState.mChildPrevInFlow = nsnull;
        goto done;

      case NS_INLINE_REFLOW_NOT_COMPLETE:
        // Some of the child fit on the line.
        if (0 == ild->mChildCount) {
          ild->mFirstChild = child;
        }
        ild->mChildCount++;
        if (--nextLine->mChildCount == 0) {
          ild->mNext = nextLine->mNext;
          delete nextLine;
          nextLine = nsnull;
        }
        else {
          child->GetNextSibling(nextLine->mFirstChild);
        }

        // XXX as this section is coded, the above will delete an empty
        // nextLine and then the MaybeCreateNextInFlow code will turn
        // around and recreate it

        // Create continuation frame (if necessary); add it to the end
        // of this InlineLineData
        MaybeCreateNextInFlow(aState, inlineLayout, ild, child);
        child->GetNextSibling(child);

        // Put continuation frame onto the next InlineLineData
        rv = SplitLine(aState, inlineLayout, ild, child);
        if (IS_REFLOW_ERROR(rv)) {
          return rv;
        }
        goto done;
      }
    }

    // Remove empty lines from the list
    ild->mNext = nextLine->mNext;
    delete nextLine;/* XXX freeList in nsCSSBlockReflowState? */
  }

  // Pullup children from the blocks next-in-flow
  // XXX write me when supporting pagination

 done:;

  // See if speculative application of the margin should stick
  if (ild == &mLines) {
    if (0 == inlineLayout.mMaxAscent + inlineLayout.mMaxDescent) {
      // No, undo margin application when we get a zero height child.
      aState.mY -= bottomMargin;
      if (aState.mY + bottomMargin >= aState.mCurrentBand.availSpace.YMost()) {
        // Restore band to previous setting
        aState.GetAvailableSpace();
      }
    }
    else {
      // Yes, keep the margin application.
      aState.mPrevPosBottomMargin = 0;
      aState.mPrevNegBottomMargin = 0;
      // XXX remember bottomMargin ended up being so that it can undo
      // it if it ends up pushing the line.
    }
  }

  return PlaceLine(aState, inlineLayout, ild, rs);
}

nsresult
InlineFrameData::MaybeCreateNextInFlow(nsCSSBlockReflowState&  aState,
                                       nsCSSInlineLayout& aInlineLayout,
                                       InlineLineData*    ild,
                                       nsIFrame*          aFrame)
{
  // Remember prev-in-flow for the next line
  aState.mChildPrevInFlow = aFrame;

  // Maybe create the next-in-flow
  nsIFrame* nextInFlow;
  nsresult rv = aInlineLayout.MaybeCreateNextInFlow(aFrame, nextInFlow);
  if (NS_OK != rv) {
    return rv;
  }
  if (nsnull != nextInFlow) {
    // Let container know it has a new child
    aState.mBlock->UpdateChildCount(1);

    // Add new child to the line
    ild->mChildCount++;
  }
  return NS_OK;
}

nsresult
InlineFrameData::SplitLine(nsCSSBlockReflowState&  aState,
                           nsCSSInlineLayout& aInlineLayout,
                           InlineLineData*    ild,
                           nsIFrame*          aFrame)
{
  PRInt32 pushCount = ild->mChildCount - aInlineLayout.mFrameNum;
  NS_FRAME_LOG(NS_FRAME_TRACE_CHILD_REFLOW,
               ("InlineFrameData::SplitLine: pushing %d frames",
                pushCount));

  if (0 != pushCount) {
    NS_ASSERTION(nsnull != aFrame, "whoops");
    InlineLineData* to = ild->mNext;
    if (nsnull != to) {
      // Only push into the next line if it's empty; otherwise we can
      // end up pushing a frame which is continued into the same frame
      // as it's continuation. This causes all sorts of side effects
      // so we don't allow it.
      if (to->mChildCount != 0) {
        InlineLineData* insertedLine = new InlineLineData(aFrame, pushCount);
        ild->mNext = insertedLine;
        insertedLine->mNext = to;
        to = insertedLine;
//XXX        to->mLastContentOffset = from->mLastContentOffset;
//XXX        to->mLastContentIsComplete = from->mLastContentIsComplete;
      }
      else {
        to->mFirstChild = aFrame;
        to->mChildCount += pushCount;
      }
    } else {
      to = new InlineLineData(aFrame, pushCount);
      ild->mNext = to;
    }
    if (nsnull == to) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

//XXX    PRInt32 kidIndexInParent;
//XXX    mKidFrame->GetContentIndex(kidIndexInParent);
//XXX    to->mFirstContentOffset = kidIndexInParent;

    // The to-line is going to be reflowed therefore it's last content
    // offset and completion status don't matter. In fact, it's expensive
    // to compute them so don't bother.
#ifdef NS_DEBUG
//XXX    to->mLastContentOffset = -1;
//XXX    to->mLastContentIsComplete = PRPackedBool(0x255);
#endif

    ild->mChildCount -= pushCount;
    NS_ASSERTION(0 != ild->mChildCount, "bad push");
  }

  return NS_OK;
}

nsresult
InlineFrameData::PlaceLine(nsCSSBlockReflowState&    aState,
                           nsCSSInlineLayout&        aInlineLayout,
                           InlineLineData*           ild,
                           nsInlineReflowStatus      aReflowStatus)
{
  // Align the children. This also determines the actual height and
  // width of the line.
  aInlineLayout.AlignFrames(ild->mFirstChild, ild->mChildCount,
                            ild->mBounds);
  aState.mY += ild->mBounds.height;

  // XXX will the line fit??? check...if the line is pushed, so must
  // it's below current line floaters too.

  nscoord xmost = ild->mBounds.XMost();
  if (xmost > aState.mKidXMost) {
    aState.mKidXMost = xmost;
  }

  // XXX add in pagination support; update band data, etc. place
  // below-line floaters, etc.

  PRBool updateReflowSpace = PR_FALSE;
  if (NS_INLINE_REFLOW_BREAK_AFTER ==
      (aReflowStatus & NS_INLINE_REFLOW_REFLOW_MASK)) {
    // Apply break to the line
    PRUint8 breakType = NS_INLINE_REFLOW_GET_BREAK_TYPE(aReflowStatus);
    switch (breakType) {
    default:
      break;
    case NS_STYLE_CLEAR_LEFT:
    case NS_STYLE_CLEAR_RIGHT:
    case NS_STYLE_CLEAR_LEFT_AND_RIGHT:
      aState.ClearFloaters(breakType);
      updateReflowSpace = PR_TRUE;
      break;
    }
    // XXX page breaks, etc, need to be passed upwards too!
  }

  // Any below current line floaters to place?
  // XXX We really want to know whether this is the initial reflow (reflow
  // unmapped) or a subsequent reflow in which case we only need to offset
  // the existing floaters...
  if (aState.mPendingFloaters.Count() > 0) {
    if (nsnull == ild->mFloaters) {
      ild->mFloaters = new nsVoidArray;
    }
    ild->mFloaters->operator=(aState.mPendingFloaters);
    aState.mPendingFloaters.Clear();
  }

  if (nsnull != ild->mFloaters) {
    aState.PlaceBelowCurrentLineFloaters(ild->mFloaters);
    // XXX Factor in the height of the floaters as well when considering
    // whether the line fits.
    // The default policy is that if there isn't room for the floaters then
    // both the line and the floaters are pushed to the next-in-flow...
  }

  if (aState.mY >= aState.mCurrentBand.availSpace.YMost()) {
    // The current y coordinate is now past our available space
    // rectangle. Get a new band of space.
    aState.GetAvailableSpace();
    updateReflowSpace = PR_TRUE;
  }

  if (updateReflowSpace) {
    aInlineLayout.SetReflowSpace(aState.mCurrentBand.availSpace.x,
                                 aState.mY,
                                 aState.mCurrentBand.availSpace.width,
                                 aState.mCurrentBand.availSpace.height);
  }

  return aReflowStatus;
}

//----------------------------------------------------------------------

// This code concerns itself with:

// floater placement
// spacemanager usage
// incremental construction of the FrameData
// coarse grain incremental reflow (block children)

nsresult
NS_NewCSSBlockFrame(nsIFrame**  aInstancePtrResult,
                    nsIContent* aContent,
                    nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsCSSBlockFrame(aContent, aParent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}

nsCSSBlockFrame::nsCSSBlockFrame(nsIContent* aContent, nsIFrame* aParent)
  : nsCSSContainerFrame(aContent, aParent)
{
}

nsCSSBlockFrame::~nsCSSBlockFrame()
{
  FrameData* fd = mFrames;
  while (nsnull != fd) {
    FrameData* next = fd->mNext;
    delete fd;
    fd = next;
  }
}

NS_IMETHODIMP
nsCSSBlockFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
#if 0
  if (aIID.Equals(kBlockFrameCID)) {
    *aInstancePtr = (void*) (this);
    return NS_OK;
  }
#endif
  if (aIID.Equals(kIRunaroundIID)) {
    *aInstancePtr = (void*) ((nsIRunaround*) this);
    return NS_OK;
  }
  if (aIID.Equals(kIFloaterContainerIID)) {
    *aInstancePtr = (void*) ((nsIFloaterContainer*) this);
    return NS_OK;
  }
  return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP
nsCSSBlockFrame::IsSplittable(nsSplittableType& aIsSplittable) const
{
  aIsSplittable = NS_FRAME_SPLITTABLE_NON_RECTANGULAR;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSBlockFrame::CreateContinuingFrame(nsIPresContext*  aCX,
                                       nsIFrame*        aParent,
                                       nsIStyleContext* aStyleContext,
                                       nsIFrame*&       aContinuingFrame)
{
  nsCSSBlockFrame* cf = new nsCSSBlockFrame(mContent, aParent);
  if (nsnull == cf) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  PrepareContinuingFrame(aCX, aParent, aStyleContext, cf);
  aContinuingFrame = cf;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSBlockFrame::ListTag(FILE* out) const
{
  if ((nsnull != mGeometricParent) && IsPseudoFrame()) {
    fprintf(out, "*block<");
    nsIAtom* atom = mContent->GetTag();
    if (nsnull != atom) {
      nsAutoString tmp;
      atom->ToString(tmp);
      fputs(tmp, out);
    }
    PRInt32 contentIndex;
    GetContentIndex(contentIndex);
    fprintf(out, ">(%d)@%p", contentIndex, this);
  } else {
    nsHTMLContainerFrame::ListTag(out);
  }
  return NS_OK;
}

NS_METHOD
nsCSSBlockFrame::List(FILE* out, PRInt32 aIndent) const
{
  PRInt32 i;

  // Indent
  for (i = aIndent; --i >= 0; ) fputs("  ", out);

  // Output the tag
  ListTag(out);
  nsIView* view;
  GetView(view);
  if (nsnull != view) {
    fprintf(out, " [view=%p]", view);
    NS_RELEASE(view);
  }

  // Output the first/last content offset
  fprintf(out, "[%d,%d,%c] ", mFirstContentOffset, mLastContentOffset,
          (mLastContentIsComplete ? 'T' : 'F'));
  if (nsnull != mPrevInFlow) {
    fprintf(out, "prev-in-flow=%p ", mPrevInFlow);
  }
  if (nsnull != mNextInFlow) {
    fprintf(out, "next-in-flow=%p ", mNextInFlow);
  }

  // Output the rect
  out << mRect;

  // Output the children, one line at a time
  if (nsnull != mFrames) {
    if (0 != mState) {
      fprintf(out, " [state=%08x]", mState);
    }
    fputs("<\n", out);

    aIndent++;
    FrameData* fd = mFrames;
    while (nsnull != fd) {
      fd->List(out, aIndent);
      fd = fd->mNext;
    }
    aIndent--;

    for (i = aIndent; --i >= 0; ) fputs("  ", out);
    fputs(">\n", out);
  } else {
    if (0 != mState) {
      fprintf(out, " [state=%08x]", mState);
    }
    fputs("<>\n", out);
  }

  return NS_OK;
}

PRIntn
nsCSSBlockFrame::GetSkipSides() const
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

NS_IMETHODIMP
nsCSSBlockFrame::Reflow(nsIPresContext*      aPresContext,
                        nsISpaceManager*     aSpaceManager,
                        nsReflowMetrics&     aMetrics,
                        const nsReflowState& aReflowState,
                        nsRect&              aDesiredRect,
                        nsReflowStatus&      aStatus)
{
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("enter nsCSSBlockFrame::Reflow: maxSize=%d,%d reason=%d [%d,%d,%c]",
      aReflowState.maxSize.width,
      aReflowState.maxSize.height,
      aReflowState.reason,
      mFirstContentOffset, mLastContentOffset,
      mLastContentIsComplete?'T':'F'));

  // If this is the initial reflow, generate any synthetic content
  // that needs generating.
  if (eReflowReason_Initial == aReflowState.reason) {
    NS_ASSERTION(0 != (NS_FRAME_FIRST_REFLOW & mState), "bad mState");
    nsresult rv = ProcessInitialReflow(aPresContext);
    if (NS_OK != rv) {
      return rv;
    }
  }
  else {
    NS_ASSERTION(0 == (NS_FRAME_FIRST_REFLOW & mState), "bad mState");
  }

  // Replace parent provided reflow state with our own significantly
  // more extensive version.
  nsCSSBlockReflowState state(aPresContext, aSpaceManager, this, aReflowState,
                              aMetrics.maxElementSize);

  nsresult rv = NS_OK;
  if (eReflowReason_Initial == state.reason) {
    state.GetAvailableSpace();
    rv = FrameAppendedReflow(state);
  }
  else if (eReflowReason_Incremental == state.reason) {
    nsIFrame* target;
    state.reflowCommand->GetTarget(target);
    if (this == target) {
      nsIReflowCommand::ReflowType type;
      state.reflowCommand->GetType(type);
      switch (type) {
      case nsIReflowCommand::FrameAppended:
        state.GetAvailableSpace();
        //XXX RecoverState(state);
        rv = FrameAppendedReflow(state);
        break;

      default:
        NS_NOTYETIMPLEMENTED("XXX");
      }
    }
    else {
      rv = ChildIncrementalReflow(state);
    }
  }
  else if (eReflowReason_Resize == state.reason) {
    state.GetAvailableSpace();
    rv = ResizeReflow(state);
  }
  ComputeFinalSize(state, aMetrics, aDesiredRect);

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("exit nsCSSBlockFrame::Reflow: size=%d,%d rv=%x [%d,%d,%c]",
      aMetrics.width, aMetrics.height, rv,
      mFirstContentOffset, mLastContentOffset,
      mLastContentIsComplete?'T':'F'));
  return rv;
}

void
nsCSSBlockFrame::ComputeFinalSize(nsCSSBlockReflowState& aState,
                                  nsReflowMetrics&  aMetrics,
                                  nsRect&           aDesiredRect)
{
  aDesiredRect.x = 0;
  aDesiredRect.y = 0;

  // Special check for zero sized content: If our content is zero
  // sized then we collapse into nothingness.
  if ((0 == aState.mKidXMost - aState.mBorderPadding.left) &&
      (0 == aState.mY - aState.mBorderPadding.top)) {
    aDesiredRect.width = 0;
    aDesiredRect.height = 0;
  }
  else {
    aDesiredRect.width = aState.mKidXMost + aState.mBorderPadding.right;
    if (!aState.mUnconstrainedWidth) {
      // Make sure we're at least as wide as the max size we were given
      nscoord maxWidth = aState.mAvailSize.width + aState.mBorderPadding.left +
        aState.mBorderPadding.right;
      if (aDesiredRect.width < maxWidth) {
        aDesiredRect.width = maxWidth;
      }
    }
    aState.mY += aState.mBorderPadding.bottom;
    nscoord lastBottomMargin = aState.mPrevPosBottomMargin -
      aState.mPrevNegBottomMargin;
    if (!aState.mUnconstrainedHeight && (lastBottomMargin > 0)) {
      // It's possible that we don't have room for the last bottom
      // margin (the last bottom margin is the margin following a block
      // element that we contain; it isn't applied immediately because
      // of the margin collapsing logic). This can happen when we are
      // reflowed in a limited amount of space because we don't know in
      // advance what the last bottom margin will be.
      nscoord maxY = aState.maxSize.height;
      if (aState.mY + lastBottomMargin > maxY) {
        lastBottomMargin = maxY - aState.mY;
        if (lastBottomMargin < 0) {
          lastBottomMargin = 0;
        }
      }
    }
    aState.mY += lastBottomMargin;
    aDesiredRect.height = aState.mY;

    if (!aState.mBlockIsPseudo) {
      // Clamp the desired rect height when style height applies
      PRIntn ss = aState.mStyleSizeFlags;
      if (0 != (ss & NS_SIZE_HAS_HEIGHT)) {
        aDesiredRect.height = aState.mBorderPadding.top +
          aState.mStyleSize.height + aState.mBorderPadding.bottom;
      }
    }
  }

  aMetrics.width = aDesiredRect.width;
  aMetrics.height = aDesiredRect.height;
  aMetrics.ascent = aMetrics.height;
  aMetrics.descent = 0;
}

// XXX move this somewhere else!!!
static PRBool
IsBlock(PRUint8 aDisplay)
{
  switch (aDisplay) {
  case NS_STYLE_DISPLAY_BLOCK:
  case NS_STYLE_DISPLAY_LIST_ITEM:
  case NS_STYLE_DISPLAY_TABLE:
    return PR_TRUE;
  }
  return PR_FALSE;
}

/**
 * This method is used to reflow content that has been appended to
 * the content container that we layout. Initial reflow and subsequent
 * incremental append operations use this method.
 */
nsresult
nsCSSBlockFrame::FrameAppendedReflow(nsCSSBlockReflowState& aState)
{
  // Get to the last FrameData (that is where appends occur)
  FrameData** fdp = &mFrames;
  FrameData* fd = mFrames;
  if (nsnull != fd) {
    while (nsnull != fd->mNext) {
      fd = fd->mNext;
    }
    fdp = &fd->mNext;
  }

  // Get InlineLineData (if appropriate) where append is occuring
  nsIFrame* lastKid;
  LastChild(lastKid);
  InlineLineData* ild = nsnull;
  if ((nsnull != fd) && !fd->IsBlock()) {
    ild = ((InlineFrameData*)fd)->LastInlineLineData();
    ((InlineFrameData*)fd)->MarkDirty();
  }

  PRInt32 kidContentIndex = NextChildOffset();

  // Get the childPrevInFlow for our eventual first child if we are a
  // continuation and we have no children and the last child in our
  // prev-in-flow is incomplete.
  nsIFrame* childPrevInFlow = nsnull;
  if ((nsnull == mFirstChild) && (nsnull != mPrevInFlow)) {
    nsCSSBlockFrame* prev = (nsCSSBlockFrame*)mPrevInFlow;
    NS_ASSERTION(prev->mLastContentOffset >= prev->mFirstContentOffset,
                 "bad prevInFlow");

    kidContentIndex = prev->NextChildOffset();
    if (!prev->mLastContentIsComplete) {
      // Our prev-in-flow's last child is not complete
      prev->LastChild(childPrevInFlow);
    }
  }

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
     ("enter nsCSSBlockFrame::FrameAppendedReflow: kci=%d fd=%p childPrevInFlow=%p",
      kidContentIndex, fd, childPrevInFlow));

  // XXX pagination: set our last-content-is-complete correctly

  // Create frames for each new piece of content
  nsresult rv = NS_FRAME_COMPLETE;
  PRInt32 lastContentIndex;
  lastContentIndex = mContent->ChildCount();
  for (; kidContentIndex < lastContentIndex; kidContentIndex++) {
    nsIContent* kid;
    kid = mContent->ChildAt(kidContentIndex);
    if (nsnull == kid) {
      break;
    }

    // Create frame for our new child and add it to our child list
    nsIFrame* kidFrame;
    rv = nsHTMLBase::CreateFrame(aState.mPresContext, this, kid,
                                 childPrevInFlow, kidFrame);
    NS_RELEASE(kid);
    if (NS_OK != rv) {
      return rv;
    }
    if (nsnull != lastKid) {
      lastKid->SetNextSibling(kidFrame);
    }
    else {
      mFirstChild = kidFrame;
      mFirstContentOffset = kidContentIndex;
    }
    mChildCount++;
    lastKid = kidFrame;
    childPrevInFlow = nsnull;

    // Get display style for the new child frame
    const nsStyleDisplay* kidDisplay;
    rv = kidFrame->GetStyleData(eStyleStruct_Display,
                                (const nsStyleStruct*&) kidDisplay);
    if (NS_OK != rv) {
      return rv;
    }
    PRBool isBlock = IsBlock(kidDisplay->mDisplay);
    if (isBlock) {
      BlockFrameData* newfd = new BlockFrameData(kidFrame);
      if (nsnull == newfd) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      *fdp = newfd;
      fdp = &newfd->mNext;

      // If previous FrameData was inline, reflow it now we know it's complete
      if (nsnull != ild) {
        rv = ((InlineFrameData*)fd)->FrameAppendedReflow(aState, ild);
        if (IS_REFLOW_ERROR(rv)) {
          return rv;
        }
        if (NS_FRAME_IS_NOT_COMPLETE(rv)) {
          // We ran out of room
          newfd->MarkDirty();
          fd = newfd;
          break;
        }
        ild = nsnull;
      }

      // Reflow block frames now (that way we can do a better job of
      // incremental painting in some cases)
      fd = newfd;
      rv = newfd->Reflow(aState, eReflowReason_Initial);
      if (IS_REFLOW_ERROR(rv)) {
        return rv;
      }
      if (NS_FRAME_IS_NOT_COMPLETE(rv)) {
        // We ran out of room
        break;
      }
    }
    else {
      if (nsnull == ild) {
        InlineFrameData* newfd = new InlineFrameData(kidFrame);
        if (nsnull == newfd) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
        *fdp = newfd;
        fdp = &newfd->mNext;
        ild = &newfd->mLines;
        fd = newfd;
      }
      else {
        ild->mChildCount++;
      }
    }
  }

  mLastContentIsComplete = PR_TRUE;
  if (lastContentIndex == 0) {
    mLastContentOffset = 0;
  } else {
    mLastContentOffset = lastContentIndex - 1;
  }

  // Reflow last InlineFrameData now that we are done
  if (nsnull != ild) {
    NS_ASSERTION((nsnull != fd) && !fd->IsBlock(), "bug");
    rv = ((InlineFrameData*)fd)->FrameAppendedReflow(aState, ild);
  }

  return rv;
}

  // If the child we just reflowed is a floater, then we have to
  // reflow all of the impacted children FrameData's; in addition, if
  // the floater's size change impacts other frames (not our chilren
  // or ourselves) we should generate reflow commands

  // Floater Shortcut: Reflow the floater and get it's new size; reset
  // space manager; call ResizeReflow

  // recover state, including Y coordinate up to point of reflow and
  // previous top, bottom margins

  // after reflow of affected child, update Y coordinates of following
  // children and recompute collapsed bottom+top margin when
  // applicable.

  // if child appended/inserted/deleted/replaced is an inline child
  // then we will need to incementally update the pass1 inline reflow
  // information. That's probably too much work so we should just toss
  // it and then reflow the InlineFrameData (marking all of it's
  // InlineLineData's dirty?)

nsresult
nsCSSBlockFrame::ChildIncrementalReflow(nsCSSBlockReflowState& aState)
{
  nsIFrame* nextFrame;
  aState.reflowCommand->GetNext(nextFrame);

  // First recover our state
  nscoord xmost = 0;
  FrameData* prevFD = nsnull;
  FrameData* fd = mFrames;
  while (nsnull != fd) {
    if (fd->Contains(nextFrame)) {
      break;
    }
    nscoord xm = fd->GetXMost();
    if (xm > xmost) {
      xmost = xm;
    }
    prevFD = fd;
    fd = fd->mNext;
  }
  if (nsnull != prevFD) {
    aState.mY = prevFD->GetYMost();
  }

  // Get band at the starting Y location
  aState.GetAvailableSpace();

  // Now reflow the affected FrameData
  nscoord oldYMost = fd->GetYMost();
  nsresult rv = fd->ResizeReflow(aState);
  if (IS_REFLOW_ERROR(rv)) {
    return rv;
  }
  nscoord xm = fd->GetXMost();
  if (xm > xmost) {
    xmost = xm;
  }

  // XXX margin updates

  // XXX Push other FrameData's to next-in-flow when we get a
  // not-complete status back; update our mLastContentIsComplete and
  // mLastContentOffset properly

  // XXX reflow any FrameData's impacted by floaters that moved/changed-size

  // Slide down remaining FrameData's and finish computing final state
  nscoord dy = fd->GetYMost() - oldYMost;
  FrameData* lastFD = fd;
  fd = fd->mNext;
  while (nsnull != fd) {
    if (0 != dy) {
      fd->MoveChildren(0, dy);
    }
    nscoord xm = fd->GetXMost();
    if (xm > xmost) {
      xmost = xm;
    }
    lastFD = fd;
    fd = fd->mNext;
  }

  // Set final Y coordinate into aState
  aState.mY = lastFD->GetYMost();

  return NS_OK;
}

/**
 * This is called when the block frame is resized. Every FrameData
 * needs reflowing in this case. However, the FrameData doesn't need
 * recomputing (except maybe the InlineLineData).
 */
nsresult
nsCSSBlockFrame::ResizeReflow(nsCSSBlockReflowState& aState)
{
  nsresult rv = NS_FRAME_COMPLETE;

  if (nsnull != mRunInFloaters) {
    aState.PlaceBelowCurrentLineFloaters(mRunInFloaters);
  }

  FrameData* fd = mFrames;
  while (nsnull != fd) {
    rv = fd->ResizeReflow(aState);
    if (IS_REFLOW_ERROR(rv)) {
      return rv;
    }
    if (NS_FRAME_IS_NOT_COMPLETE(rv)) {
      break;
    }
    fd = fd->mNext;
  }

  return rv;
}

void nsCSSBlockFrame::ReflowFloater(nsIPresContext*   aPresContext,
                                    nsCSSBlockReflowState& aState,
                                    nsIFrame*         aFloaterFrame)
{
//XXX fix_me: use the values already stored in the nsCSSBlockReflowState
  // Compute the available space for the floater. Use the default
  // 'auto' width and height values
  nsSize kidAvailSize(0, NS_UNCONSTRAINEDSIZE);
  nsSize styleSize;
  PRIntn styleSizeFlags =
    nsCSSLayout::GetStyleSize(aPresContext, aState, styleSize);

  // XXX The width and height are for the content area only. Add in space for
  // border and padding
  if (styleSizeFlags & NS_SIZE_HAS_WIDTH) {
    kidAvailSize.width = styleSize.width;
  }
  if (styleSizeFlags & NS_SIZE_HAS_HEIGHT) {
    kidAvailSize.height = styleSize.height;
  }

  // Resize reflow the anchored item into the available space
  // XXX Check for complete?
  nsReflowMetrics desiredSize(nsnull);
  nsReflowState   reflowState(aFloaterFrame, aState, kidAvailSize,
                              eReflowReason_Initial);
  nsReflowStatus  status;

  aFloaterFrame->WillReflow(*aPresContext);
  aFloaterFrame->Reflow(aPresContext, desiredSize, reflowState, status);
  aFloaterFrame->SizeTo(desiredSize.width, desiredSize.height);

  aFloaterFrame->DidReflow(*aPresContext, NS_FRAME_REFLOW_FINISHED);
}

PRBool
nsCSSBlockFrame::AddFloater(nsIPresContext*      aPresContext,
                            const nsReflowState& aReflowState,
                            nsIFrame*            aFloater,
                            nsPlaceholderFrame*  aPlaceholder)
{
  // Walk up reflow state chain, looking for ourself
  const nsReflowState* rs = &aReflowState;
  while (nsnull != rs) {
    if (rs->frame == this) {
      break;
    }
    rs = rs->parentReflowState;
  }
  if (nsnull == rs) {
    // Never mind
    return PR_FALSE;
  }
  nsCSSBlockReflowState* state = (nsCSSBlockReflowState*) rs;

  // Get the frame associated with the space manager, and get its
  // nsIAnchoredItems interface
  nsIFrame* frame = state->mSpaceManager->GetFrame();
  nsIAnchoredItems* anchoredItems = nsnull;

  frame->QueryInterface(kIAnchoredItemsIID, (void**)&anchoredItems);
  NS_ASSERTION(nsnull != anchoredItems, "no anchored items interface");
  if (nsnull != anchoredItems) {
    anchoredItems->AddAnchoredItem(aFloater,
                                   nsIAnchoredItems::anHTMLFloater,
                                   this);

    // Reflow the floater
    ReflowFloater(aPresContext, *state, aFloater);

    // Determine whether we place it at the top or we place it below the
    // current line
    if (IsLeftMostChild(aPlaceholder)) {
      if (nsnull == mRunInFloaters) {
        mRunInFloaters = new nsVoidArray;
      }
      mRunInFloaters->AppendElement(aPlaceholder);
      state->PlaceCurrentLineFloater(aFloater);
    } else {
      // Add the placeholder to our to-do list
      state->mPendingFloaters.AppendElement(aPlaceholder);
    }
    return PR_TRUE;
  }

  return PR_FALSE;
}

PRBool
nsCSSBlockFrame::IsLeftMostChild(nsIFrame* aFrame)
{
  do {
    nsIFrame* parent;
    aFrame->GetGeometricParent(parent);
  
    // See if there are any non-zero sized child frames that precede
    // aFrame in the child list
    nsIFrame* child;
    parent->FirstChild(child);
    while ((nsnull != child) && (aFrame != child)) {
      nsSize  size;

      // Is the child zero-sized?
      child->GetSize(size);
      if ((size.width > 0) || (size.height > 0)) {
        // We found a non-zero sized child frame that precedes aFrame
        return PR_FALSE;
      }
      child->GetNextSibling(child);
    }
  
    // aFrame is the left-most non-zero sized frame in its geometric parent.
    // Walk up one level and check that its parent is left-most as well
    aFrame = parent;
  } while (aFrame != this);
  return PR_TRUE;
}

void
nsCSSBlockFrame::WillDeleteNextInFlowFrame(nsIFrame* aDeadFrame)
{
  FrameData* fd = mFrames;
  while (nsnull != fd) {
    fd->DeleteNextInFlowFrame(aDeadFrame);
    fd = fd->mNext;
  }
}

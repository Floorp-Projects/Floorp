/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#ifndef nsBlockReflowState_h__
#define nsBlockReflowState_h__

#include "nsBlockBandData.h"
#include "nsLineBox.h"

class nsBlockFrame;

class nsBlockReflowState {
public:
  nsBlockReflowState(const nsHTMLReflowState& aReflowState,
                     nsIPresContext* aPresContext,
                     nsBlockFrame* aFrame,
                     const nsHTMLReflowMetrics& aMetrics,
                     PRBool aBlockMarginRoot);

  ~nsBlockReflowState();

  /**
   * Update our state when aLine is skipped over during incremental
   * reflow.
   */
  void RecoverStateFrom(nsLineBox* aLine, PRBool aPrevLineWasClean);

  /**
   * Get the available reflow space for the current y coordinate. The
   * available space is relative to our coordinate system (0,0) is our
   * upper left corner.
   */
  void GetAvailableSpace() {
    GetAvailableSpace(mY);
  }

  void GetAvailableSpace(nscoord aY);

  void InitFloater(nsLineLayout& aLineLayout,
                   nsPlaceholderFrame* aPlaceholderFrame);

  void AddFloater(nsLineLayout& aLineLayout,
                  nsPlaceholderFrame* aPlaceholderFrame,
                  PRBool aInitialReflow);

  PRBool CanPlaceFloater(const nsRect& aFloaterRect, PRUint8 aFloats);

  void FlowAndPlaceFloater(nsFloaterCache* aFloaterCache,
                           PRBool* aIsLeftFloater);

  void PlaceBelowCurrentLineFloaters(nsFloaterCacheList& aFloaters);

  void ClearFloaters(nscoord aY, PRUint8 aBreakType);

  PRBool ClearPastFloaters(PRUint8 aBreakType);

  PRBool IsAdjacentWithTop() const {
    return mY == mReflowState.mComputedBorderPadding.top;
  }

  const nsMargin& BorderPadding() const {
    return mReflowState.mComputedBorderPadding;
  }

  const nsMargin& Margin() const {
    return mReflowState.mComputedMargin;
  }

  void UpdateMaxElementSize(const nsSize& aMaxElementSize);

  void UpdateMaximumWidth(nscoord aMaximumWidth);

  void RecoverVerticalMargins(nsLineBox* aLine,
                              PRBool aApplyTopMargin,
                              nscoord* aTopMarginResult,
                              nscoord* aBottomMarginResult);

  void ComputeBlockAvailSpace(nsIFrame* aFrame,
                              nsSplittableType aSplitType,
                              const nsStyleDisplay* aDisplay,
                              nsRect& aResult);

  void RecoverStateFrom(nsLineBox* aLine,
                        PRBool aApplyTopMargin,
                        nsRect* aDamageRect);

  void AdvanceToNextLine() {
    mLineNumber++;
  }

  PRBool IsImpactedByFloater() const;

  nsLineBox* NewLineBox(nsIFrame* aFrame, PRInt32 aCount, PRBool aIsBlock);

  void FreeLineBox(nsLineBox* aLine);

  void StoreMaxElementSize(nsIFrame* aFloater, const nsSize& aSize) {
    mBand.StoreMaxElementSize(mPresContext, aFloater, aSize);
  }

  //----------------------------------------

  // This state is the "global" state computed once for the reflow of
  // the block.

  // The block frame that is using this object
  nsBlockFrame* mBlock;

  nsIPresContext* mPresContext;

  const nsHTMLReflowState& mReflowState;

  nsISpaceManager* mSpaceManager;

  // The coordinates within the spacemanager where the block is being
  // placed <b>after</b> taking into account the blocks border and
  // padding. This, therefore, represents the inner "content area" (in
  // spacemanager coordinates) where child frames will be placed,
  // including child blocks and floaters.
  nscoord mSpaceManagerX, mSpaceManagerY;

  // XXX get rid of this
  nsReflowStatus mReflowStatus;

  nscoord mBottomEdge;

  // The content area to reflow child frames within. The x/y
  // coordinates are known to be mBorderPadding.left and
  // mBorderPadding.top. The width/height may be NS_UNCONSTRAINEDSIZE
  // if the container reflowing this frame has given the frame an
  // unconstrained area.
  nsSize mContentArea;

  //----------------------------------------

  // This state is "running" state updated by the reflow of each line
  // in the block. This same state is "recovered" when a line is not
  // dirty and is passed over during incremental reflow.

  // The current line being reflowed
  nsLineBox* mCurrentLine;

  // The previous line just reflowed
  nsLineBox* mPrevLine;

  // The current Y coordinate in the block
  nscoord mY;

  // The available space within the current band.
  nsRect mAvailSpaceRect;

  // The maximum x-most of each line
  nscoord mKidXMost;

  // The combined area of all floaters placed so far
  nsRect mFloaterCombinedArea;

  // The y-coordinate of the last floater placed. We keep this around
  // to enforce 9.5.1 rule [2]
  nscoord mLastFloaterY;

  // For unconstained-width reflow, we keep the right floaters
  // combined area stored seperately.
  PRBool mHaveRightFloaters;
  nsRect mRightFloaterCombinedArea;

  nsFloaterCacheFreeList mFloaterCacheFreeList;

  // Previous child. This is used when pulling up a frame to update
  // the sibling list.
  nsIFrame* mPrevChild;

  // The next immediate child frame that is the target of an
  // incremental reflow command. Once that child has been reflowed we
  // null this slot out.
  nsIFrame* mNextRCFrame;

  // The previous child frames collapsed bottom margin value.
  nscoord mPrevBottomMargin;

  // The current next-in-flow for the block. When lines are pulled
  // from a next-in-flow, this is used to know which next-in-flow to
  // pull from. When a next-in-flow is emptied of lines, we advance
  // this to the next next-in-flow.
  nsBlockFrame* mNextInFlow;

  // The current band data for the current Y coordinate
  nsBlockBandData mBand;

  //----------------------------------------

  // Temporary line-reflow state. This state is used during the reflow
  // of a given line, but doesn't have meaning before or after.

  // The list of floaters that are "current-line" floaters. These are
  // added to the line after the line has been reflowed, to keep the
  // list fiddling from being N^2.
  nsFloaterCacheFreeList mCurrentLineFloaters;

  // The list of floaters which are "below current-line"
  // floaters. These are reflowed/placed after the line is reflowed
  // and placed. Again, this is done to keep the list fiddling from
  // being N^2.
  nsFloaterCacheFreeList mBelowCurrentLineFloaters;

  nsSize mMaxElementSize;
  nscoord mMaximumWidth;

  nscoord mMinLineHeight;

  PRInt32 mLineNumber;

  // block reflow state flags
#define BRS_UNCONSTRAINEDWIDTH    0x00000001
#define BRS_UNCONSTRAINEDHEIGHT   0x00000002
#define BRS_SHRINKWRAPWIDTH       0x00000004
#define BRS_NEEDRESIZEREFLOW      0x00000008
#define BRS_ISINLINEINCRREFLOW    0x00000010
#define BRS_NOWRAP                0x00000020
#define BRS_ISTOPMARGINROOT       0x00000040  // Is this frame a root for top/bottom margin collapsing?
#define BRS_ISBOTTOMMARGINROOT    0x00000080
#define BRS_APPLYTOPMARGIN        0x00000100  // See ShouldApplyTopMargin
#define BRS_COMPUTEMAXELEMENTSIZE 0x00000200
#define BRS_COMPUTEMAXWIDTH       0x00000400
#define BRS_LASTFLAG              BRS_COMPUTEMAXWIDTH

  PRInt16 mFlags;

  void SetFlag(PRUint32 aFlag, PRBool aValue)
  {
    NS_ASSERTION(aFlag<=BRS_LASTFLAG, "bad flag");
    NS_ASSERTION(aValue==PR_FALSE || aValue==PR_TRUE, "bad value");
    if (aValue) { // set flag
      mFlags |= aFlag;
    }
    else {        // unset flag
      mFlags &= ~aFlag;
    }
  }

  PRBool GetFlag(PRUint32 aFlag) const
  {
    NS_ASSERTION(aFlag<=BRS_LASTFLAG, "bad flag");
    PRBool result = (mFlags & aFlag);
    if (result) return PR_TRUE;
    return PR_FALSE;
  }
};

#endif // nsBlockReflowState_h__

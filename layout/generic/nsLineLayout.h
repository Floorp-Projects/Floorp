/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsLineLayout_h___
#define nsLineLayout_h___

#include "nsIFrame.h"
#include "nsCoord.h"
#include "nsSize.h"
#include "nsVoidArray.h"

class nsIPresContext;
class nsISpaceManager;
class nsBlockFrame;
struct nsBlockReflowState;

/**
 * Persistent per-line data
 */
struct nsLineData {
  nsLineData();
  ~nsLineData();

  nsIFrame* GetLastChild();
  nsresult Verify(PRBool aFinalCheck = PR_TRUE) const;
  void List(FILE* out = stdout, PRInt32 aIndent = 0) const;
  void UnlinkLine();
  PRIntn GetLineNumber() const;

  nsLineData* mNextLine;
  nsLineData* mPrevLine;
  nsIFrame* mFirstChild;
  PRInt32 mChildCount;
  PRInt32 mFirstContentOffset;
  PRInt32 mLastContentOffset;
  nsRect mBounds;
  PRPackedBool mLastContentIsComplete;
  PRPackedBool mIsBlock;
  nsVoidArray* mFloaters;  // placeholder frames for below current line floaters
};

//----------------------------------------------------------------------

/**
 * Transient state used during line reflow.
 */

// State used during line layout that may need to be rewound because
// of an inconvenient word break.
struct nsLineLayoutState {
  // The next x coordinate to place the child frame
  nscoord mX;

  // This is true if the next frame to reflow should skip over leading
  // whitespace.
  PRBool mSkipLeadingWhiteSpace;

  // The column number when pre-formatting (for tab spacing)
  PRInt32 mColumn;

  // The child frame being considered for reflow
  nsIFrame* mKidFrame;

  // The previous child frame just reflowed
  nsIFrame* mPrevKidFrame;

  // The child frame's content index-in-parent
  PRInt32 mKidIndex;

  // The child frame number within this line
  PRInt32 mKidFrameNum;

  // Current line values values
  nsSize mMaxElementSize;
  nscoord mMaxAscent;
  nscoord mMaxDescent;
};

struct nsLineLayout {
  nsLineLayout(nsBlockReflowState& aState);
  ~nsLineLayout();

  nsresult Initialize(nsBlockReflowState& aState, nsLineData* aLine);

  void SetReflowSpace(nsRect& aAvailableSpaceRect);

  nsresult ReflowLine();

  nsresult IncrementalReflowFromChild(nsReflowCommand* aReflowCommand,
                                      nsIFrame*        aChildFrame);
                                      
  void AtSpace();

  void AtWordStart(nsIFrame* aFrame, nscoord aX);

  PRBool SkipLeadingWhiteSpace() {
    return mState.mSkipLeadingWhiteSpace;
  }

  void SetSkipLeadingWhiteSpace(PRBool aSkip) {
    mState.mSkipLeadingWhiteSpace = aSkip;
  }

  PRIntn GetColumn() {
    return mState.mColumn;
  }

  void SetColumn(PRIntn aNewColumn) {
    mState.mColumn = aNewColumn;
  }

  // A value set by line-layout cognizant frames. Frames that are not
  // aware of line layout leave the value unchanged and can thus be
  // detected by the line-layout algorithm.
  PRInt32 mReflowResult;

  // The presentation context
  nsIPresContext* mPresContext;

  // The block behind the line
  nsBlockFrame* mBlock;
  nsBlockReflowState& mBlockReflowState;
  nsISpaceManager* mSpaceManager;
  nsIContent* mBlockContent;

  // The line we are reflowing
  nsLineData* mLine;
  nsIFrame* mKidPrevInFlow;

  // Whenever the line layout creates a frame this slot is incremented
  PRIntn mNewFrames;
  PRIntn mFramesReflowed;
  PRIntn mOldChildCount;

  nsLineLayoutState mState;
  nsLineLayoutState mSavedState;

  // The current breakable point in the line. If a line-break is
  // requested then this is where the line will break at.
  nsIFrame* mBreakFrame;
  nscoord mBreakX;
  PRUint8 mPendingBreak;

  // XXX ick
  // This is set by the block code when it updates the available
  // reflow area when a floater is placed.
  PRBool mReflowDataChanged;
  PRPackedBool mMustReflowMappedChildren;

  PRPackedBool mUnconstrainedWidth;
  PRPackedBool mUnconstrainedHeight;
  PRPackedBool mMarginApplied;

  nscoord mY;
  nscoord mLineHeight;
  nscoord mMaxWidth;
  nscoord mMaxHeight;
  nsSize* mMaxElementSizePointer;
  nscoord mLeftEdge;
  nscoord mRightEdge;

  nscoord* mAscents;
  nscoord mAscentBuf[20];
  nscoord mMaxAscents;

protected:
  nsresult SetAscent(nscoord aAscent);

  nsresult ReflowMappedChild();

  nsresult ReflowChild(nsReflowCommand* aReflowCommand, PRBool aNewChild);

  nsresult PlaceChild(nsRect& kidRect,
                      const nsReflowMetrics& kidMetrics,
                      const nsSize* kidMaxElementSize,
                      const nsMargin& kidMargin,
                      nsReflowStatus kidReflowStatus);

  nsresult ReflowMapped();

  nsresult SplitLine(PRInt32 aChildReflowStatus);

  nsresult PushChildren();

  nsresult CreateNextInFlow(nsIFrame* aFrame);

  nsresult PullUpChildren();

  nsresult CreateFrameFor(nsIContent* aKid);

  nsresult ReflowUnmapped();

  void AlignChildren();

  PRBool CanBreak();
};

// Value's for nsLineLayout.mReflowResult
#define NS_LINE_LAYOUT_REFLOW_RESULT_NOT_AWARE   0
#define NS_LINE_LAYOUT_REFLOW_RESULT_AWARE       1
#define NS_LINE_LAYOUT_REFLOW_RESULT_BREAK_AFTER 2

// Return value's from nsLineLayout reflow methods
#define NS_LINE_LAYOUT_COMPLETE                  0
#define NS_LINE_LAYOUT_NOT_COMPLETE              1
#define NS_LINE_LAYOUT_BREAK_BEFORE              2
#define NS_LINE_LAYOUT_BREAK_AFTER               3

#endif /* nsLineLayout_h___ */

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
  PRPackedBool mHasBullet;
  PRPackedBool mIsBlock;
};

//----------------------------------------------------------------------

/**
 * Transient state used during line reflow.
 */

struct nsLineLayoutReflowData {
  nscoord mX;
  nscoord mAvailWidth;
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

  nsresult IncrementalReflowFromChild(nsReflowCommand& aReflowCommand,
                                      nsIFrame*        aChildFrame);
                                      
  // The presentation context
  nsIPresContext* mPresContext;

  // The block behind the line
  nsBlockFrame* mBlock;
  nsBlockReflowState& mBlockReflowState;
  nsISpaceManager* mSpaceManager;
  nsIContent* mBlockContent;
  PRInt32 mKidIndex;

  // The line we are reflowing
  nsLineData* mLine;
  nsIFrame* mKidPrevInFlow;

  // Whenever the line layout creates a frame this slot is incremented
  PRInt32 mNewFrames;
  PRIntn mFramesReflowed;

  // Current reflow data indicating where we are in the line, how much
  // width remains and the maximum element size so far.
  nsLineLayoutReflowData mReflowData;

  // This is set by the block code when it updates the available
  // reflow area when a floater is placed.
  PRBool mReflowDataChanged;

  PRPackedBool mUnconstrainedWidth;
  PRPackedBool mUnconstrainedHeight;
  PRPackedBool mMarginApplied;
  nscoord mY;
  nscoord mLineHeight;
  nscoord mMaxWidth;
  nscoord mMaxHeight;
  nsSize* mMaxElementSizePointer;
  nscoord mX0;
  nscoord mNewRightEdge;

  nscoord* mAscents;
  nscoord mAscentBuf[20];
  nscoord mMaxAscents;
  nscoord mAscentNum;

  // The current type of reflow in progress. Normally, this value is
  // set to NS_LINE_LAYOUT_NORMAL_REFLOW. However, in the special case
  // of needing to chop off some frames because of a soft-line-break
  // that was triggered by a word-wrap, the value will be set to
  // NS_LINE_LAYOUT_WORD_WRAP_REFLOW. nsInlineFrame's and text know
  // about this flag and respond to it appropriately.
  PRInt32 mReflowType;

  // A value set by line-layout cognizant frames. Frames that are not
  // aware of line layout leave the value unchanged and can thus be
  // detected by the line-layout algorithm.
  PRInt32 mReflowResult;

  // The current child frame being reflowed
  nsIFrame* mKidFrame;

  // The previous child frame that was just reflowed
  nsIFrame* mPrevKidFrame;

  // When a span of text ends in non-whitespace then we need to track
  // the start of the word in case we need to word break at that word.
  // Note that mWordStart may not be a direct child; instead it may be
  // several levels down in inline frames. It may or may not be the
  // first child of a contained inline.
  nsIFrame* mWordStart;
  nsIFrame* mWordStartParent;
  PRInt32 mWordStartOffset;
  nsLineLayoutReflowData mWordStartReflowData;

  PRBool mSkipLeadingWhiteSpace;
  PRInt32 mColumn;

protected:

  nsresult AddAscent(nscoord aAscent);

  nsresult WordBreakReflow();

  nsresult ReflowMappedChild(nsReflowCommand* aReflowCommand);

  nsresult ReflowChild(nsReflowCommand* aReflowCommand);

  nsresult PlaceChild(const nsRect& kidRect,
                      const nsReflowMetrics& kidMetrics,
                      const nsSize* kidMaxElementSize,
                      const nsMargin& kidMargin,
                      nsReflowStatus kidReflowStatus);

  nsresult ReflowMapped();

  nsresult SplitLine(PRInt32 aChildReflowStatus, PRInt32 aRemainingKids);

  nsresult PushChildren();

  nsresult CreateNextInFlow(nsIFrame* aFrame);

  nsresult PullUpChildren();

  nsresult CreateFrameFor(nsIContent* aKid);

  nsresult ReflowUnmapped();

  nsresult AlignChildren();

  nsIFrame* GetWordStartParent();
};

// Value's for nsLineLayout.mReflowType
#define NS_LINE_LAYOUT_REFLOW_TYPE_NORMAL    0
#define NS_LINE_LAYOUT_REFLOW_TYPE_WORD_WRAP 1

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

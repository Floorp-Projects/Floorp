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
#ifndef nsCSSLineLayout_h___
#define nsCSSLineLayout_h___

#include "nsCSSBlockFrame.h"
#include "nsVoidArray.h"
#include "nsStyleConsts.h"

class nsCSSLineLayout;
struct nsStyleDisplay;
struct nsStyleFont;
struct nsStyleText;

/* d76e29b0-ff56-11d1-89e7-006008911b81 */
#define NS_IINLINE_REFLOW_IID \
{0xd76e29b0, 0xff56, 0x11d1, {0x89, 0xe7, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81}}

class nsIInlineReflow {
public:
  /**
   * Recursively find all of the text runs contained in an outer
   * block container. Inline frames implement this by recursing over
   * their children; note that inlines frames may need to create
   * missing child frames before proceeding (e.g. when a tree
   * containing inlines is appended/inserted into a block container
   */
  NS_IMETHOD FindTextRuns(nsCSSLineLayout& aLineLayout) = 0;

  /**
   * InlineReflow method. See below for how to interpret the return value.
   */
  NS_IMETHOD InlineReflow(nsCSSLineLayout&     aLineLayout,
                          nsReflowMetrics&     aDesiredSize,
                          const nsReflowState& aReflowState) = 0;
};

/**
 * For InlineReflow the return value (an nsresult) indicates the
 * status of the reflow operation. If the return value is negative
 * then some sort of catastrophic error has occured (e.g. out of memory).
 * If the return value is non-negative then the macros below can be
 * used to interpret it.
 */
typedef nsresult nsInlineReflowStatus;

// The low 3 bits of the nsInlineReflowStatus indicate what happened
// to the child during it's reflow.
#define NS_INLINE_REFLOW_COMPLETE     0             // note: not a bit!
#define NS_INLINE_REFLOW_NOT_COMPLETE 1         
#define NS_INLINE_REFLOW_BREAK_BEFORE 2
#define NS_INLINE_REFLOW_BREAK_AFTER  3
#define NS_INLINE_REFLOW_REFLOW_MASK  0x3

// The inline reflow status may need to indicate that the next-in-flow
// must be reflowed. This bit is or'd in in that case.
#define NS_INLINE_REFLOW_NEXT_IN_FLOW 0x4

// When a break is indicated (break-before/break-after) the type of
// break requested is indicate in bits 4-7.
#define NS_INLINE_REFLOW_BREAK_MASK   0xF0
#define NS_INLINE_REFLOW_GET_BREAK_TYPE(_status) (((_status) >> 4) & 0xF)
#define NS_INLINE_REFLOW_MAKE_BREAK_TYPE(_type)  ((_type) << 4)

// This macro maps an nsIFrame nsReflowStatus value into an
// nsInlineReflowStatus value.
#define NS_FRAME_REFLOW_STATUS_2_INLINE_REFLOW_STATUS(_status) \
  (((_status) & 0x1) | (((_status) & NS_FRAME_REFLOW_NEXTINFLOW) << 2))

// Convenience macro's
#define NS_INLINE_REFLOW_LINE_BREAK_BEFORE      \
  (NS_INLINE_REFLOW_BREAK_BEFORE |              \
   NS_INLINE_REFLOW_MAKE_BREAK_TYPE(NS_STYLE_CLEAR_LINE))

#define NS_INLINE_REFLOW_LINE_BREAK_AFTER       \
  (NS_INLINE_REFLOW_BREAK_AFTER |               \
   NS_INLINE_REFLOW_MAKE_BREAK_TYPE(NS_STYLE_CLEAR_LINE))

// This macro tests to see if an nsInlineReflowStatus is an error value
// or just a regular return value
#define NS_INLINE_REFLOW_ERROR(_status) (PRInt32(_status) < 0)

//----------------------------------------------------------------------

// This structure represents a run of text. In mText are the
// nsIFrame's that are considered text frames.
struct nsCSSTextRun {
  nsCSSTextRun() {
    mNext = nsnull;
  }

  ~nsCSSTextRun() {
    nsCSSTextRun* run = mNext;
    while (nsnull != run) {
      nsCSSTextRun* next = run->mNext;
      delete run;
      run = next;
    }
  }

  void List(FILE* out, PRInt32 aIndent);

  nsVoidArray mArray;
  nsCSSTextRun* mNext;
};

//----------------------------------------------------------------------

/**
 * This structure contains the horizontal layout state for line
 * layout frame placement. This is factored out of nsCSSLineLayout so
 * that the block layout code and the inline layout code can use
 * nsCSSLineLayout to reflow and place frames.
 */
struct nsCSSInlineLayout {
  nsCSSInlineLayout(nsCSSLineLayout&     aLineLayout,
                    nsIFrame*            aContainerFrame,
                    nsIStyleContext*     aContainerStyle,
                    const nsReflowState& aContainerReflowState);
  ~nsCSSInlineLayout();

  void Prepare(PRBool aUnconstrainedWidth,
               PRBool aNoWrap,
               nsSize* aMaxElementSize);

  void SetReflowSpace(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight);

  nsInlineReflowStatus ReflowAndPlaceFrame(nsIFrame* aFrame);

  nscoord AlignFrames(nsIFrame* aFrame, PRInt32 aFrameCount,
                      nsRect& aBounds);

  PRInt32 GetFrameNum() { return mFrameNum; }

  /**
   * Return an approximation of the line height for the line as
   * reflowed so far. Note that this value doesn't take into account
   * any vertical alignment properties on the frames in the line so
   * it may be smaller than it should be.
   */
  nscoord GetLineHeight() { return mMaxAscent + mMaxDescent; }

protected:
  PRBool IsFirstChild();

  nsresult SetAscent(nscoord aAscent);

  PRBool ComputeMaxSize(nsIFrame* aFrame,
                        nsMargin& aKidMargin,
                        nsSize&   aResult);

  nsInlineReflowStatus ReflowFrame(nsIFrame*            aFrame,
                                   nsReflowMetrics&     aDesiredSize,
                                   const nsReflowState& aReflowState,
                                   PRBool&              aInlineAware);

  nsInlineReflowStatus PlaceFrame(nsIFrame* aFrame,
                                  nsRect& kidRect,
                                  const nsReflowMetrics& kidMetrics,
                                  const nsMargin& kidMargin,
                                  nsInlineReflowStatus kidReflowStatus);

  nsCSSLineLayout& mLineLayout;
  nsIFrame* mContainerFrame;
  const nsStyleFont* mContainerFont;
  const nsStyleText* mContainerText;
  const nsStyleDisplay* mContainerDisplay;
  const nsReflowState& mContainerReflowState;
  PRUint8 mDirection;

  PRPackedBool mUnconstrainedWidth;
  PRPackedBool mNoWrap;
  nscoord mAvailWidth;
  nscoord mAvailHeight;
  nscoord mX, mY;
  nscoord mLeftEdge, mRightEdge;

  PRInt32 mFrameNum;
  nscoord* mAscents;
  nscoord mAscentBuf[20];
  nscoord mMaxAscents;

  nscoord mMaxAscent;
  nscoord mMaxDescent;
  nsSize* mMaxElementSize;
};

//----------------------------------------------------------------------

// 1) collecting frames for text-runs
// 2) horizontal layout of frames for block
// 3) horizontal layout of frames for inline
// 4) state storage for text-reflow

class nsCSSLineLayout {
public:
  nsCSSLineLayout(nsIPresContext* aPresContext,
                  nsISpaceManager* aSpaceManager);
  ~nsCSSLineLayout();

  void EndTextRun();

  // Note: continuation frames must NOT add themselves; just the
  // first-in-flow
  nsresult AddText(nsIFrame* aTextFrame);

  nsCSSTextRun* TakeTextRuns() {
    nsCSSTextRun* result = mTextRuns;
    mTextRuns = nsnull;
    return result;
  }

  void Prepare(nscoord aLeftEdge) {
    mLeftEdge = aLeftEdge;
    mColumn = 0;
  }

  PRInt32 GetColumn() {
    return mColumn;
  }

  void SetColumn(PRInt32 aNewColumn) {
    mColumn = aNewColumn;
  }

  nsIPresContext* mPresContext;
  nsISpaceManager* mSpaceManager;

  PRBool mListPositionOutside;
  PRInt32 mLineNumber;
  nscoord mLeftEdge;
  PRInt32 mColumn;

  nsCSSTextRun* mTextRuns;
  nsCSSTextRun** mTextRunP;
  nsCSSTextRun* mCurrentTextRun;
};

#endif /* nsCSSLineLayout_h___ */

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
#ifndef nsLineLayout_h___
#define nsLineLayout_h___

#include "nsFrame.h"
#include "nsVoidArray.h"
#include "nsTextReflow.h"

class nsISpaceManager;
class nsBlockReflowState;
class nsPlaceholderFrame;
struct nsStyleText;

// If your machine has a limited size stack, you'll want to adjust
// these numbers. Note that it will force the line layout code to use
// the heap more so layout will be slower!
#if defined(XP_MAC)
#define NS_LINELAYOUT_NUM_FRAMES        15
#define NS_LINELAYOUT_NUM_SPANS         5
#else
#define NS_LINELAYOUT_NUM_FRAMES        50
#define NS_LINELAYOUT_NUM_SPANS         20
#endif

class nsLineLayout {
public:
  nsLineLayout(nsIPresContext& aPresContext,
               nsISpaceManager* aSpaceManager,
               const nsHTMLReflowState* aOuterReflowState,
               PRBool aComputeMaxElementSize);
  nsLineLayout(nsIPresContext& aPresContext);
  ~nsLineLayout();

  void Init(nsBlockReflowState* aState) {
    mBlockRS = aState;
  }

  PRInt32 GetColumn() {
    return mColumn;
  }

  void SetColumn(PRInt32 aNewColumn) {
    mColumn = aNewColumn;
  }

  void AdvanceToNextLine() {
    mLineNumber++;
  }
  
  PRInt32 GetLineNumber() const {
    return mLineNumber;
  }

  void BeginLineReflow(nscoord aX, nscoord aY,
                       nscoord aWidth, nscoord aHeight,
                       PRBool aImpactedByFloaters,
                       PRBool aIsTopOfPage);

  void EndLineReflow();

  void UpdateBand(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                  PRBool aPlacedLeftFloater);

  nsresult BeginSpan(nsIFrame* aFrame,
                     const nsHTMLReflowState* aSpanReflowState,
                     nscoord aLeftEdge,
                     nscoord aRightEdge);

  void EndSpan(nsIFrame* aFrame, nsSize& aSizeResult,
               nsSize* aMaxElementSize);

  PRInt32 GetCurrentSpanCount() const;

  void SplitLineTo(PRInt32 aNewCount);

  PRBool IsZeroHeight();

  nsresult ReflowFrame(nsIFrame* aFrame,
                       nsIFrame** aNextRCFrame,
                       nsReflowStatus& aReflowStatus);

  nscoord GetCarriedOutBottomMargin() const {
    return mCarriedOutBottomMargin;
  }

  nsresult AddBulletFrame(nsIFrame* aFrame,
                          const nsHTMLReflowMetrics& aMetrics);

  void RemoveBulletFrame(nsIFrame* aFrame) {
    PushFrame(aFrame);
  }

  void VerticalAlignFrames(nsRect& aLineBoxResult,
                           nsSize& aMaxElementSizeResult);

  void TrimTrailingWhiteSpace(nsRect& aLineBounds);

  void HorizontalAlignFrames(nsRect& aLineBounds, PRBool aAllowJustify);

  void RelativePositionFrames(nsRect& aCombinedArea);

  //----------------------------------------

  // Support methods for white-space compression and word-wrapping
  // during line reflow

  void SetEndsInWhiteSpace(PRBool aState) {
    mEndsInWhiteSpace = aState;
  }

  PRBool GetEndsInWhiteSpace() const {
    return mEndsInWhiteSpace;
  }

  void SetUnderstandsWhiteSpace(PRBool aSetting) {
    mUnderstandsWhiteSpace = aSetting;
  }

  void RecordWordFrame(nsIFrame* aWordFrame) {
    mWordFrames.AppendElement(aWordFrame);
  }

  PRBool InWord() const {
    return 0 != mWordFrames.Count();
  }

  void ForgetWordFrame(nsIFrame* aFrame);

  void ForgetWordFrames() {
    mWordFrames.Clear();
  }

  nsIFrame* FindNextText(nsIFrame* aFrame);

  PRBool CanPlaceFloaterNow() const;

  PRBool LineIsEmpty() const;

  PRBool LineIsBreakable() const;

  //----------------------------------------

  // Inform the line-layout engine about the presence of a BR frame
  // XXX get rid of this: use get-frame-type?
  void SetBRFrame(nsIFrame* aFrame) {
    mBRFrame = aFrame;
  }

  // Return the line's BR frame if any
  nsIFrame* GetBRFrame() const {
    return mBRFrame;
  }

  //----------------------------------------
  // Inform the line-layout about the presence of a floating frame
  // XXX get rid of this: use get-frame-type?
  void InitFloater(nsPlaceholderFrame* aFrame);
  void AddFloater(nsPlaceholderFrame* aFrame);

  //----------------------------------------

  PRBool GetFirstLetterStyleOK() const {
    return mFirstLetterStyleOK;
  }

  void SetFirstLetterStyleOK(PRBool aSetting) {
    mFirstLetterStyleOK = aSetting;
  }

  void SetFirstLetterFrame(nsIFrame* aFrame) {
    mFirstLetterFrame = aFrame;
  }

  //----------------------------------------
  // Text run usage methods. These methods are using during reflow to
  // track the current text run and to advance through text runs.

  void SetReflowTextRuns(nsTextRun* aTextRuns) {
    mReflowTextRuns = aTextRuns;
  }

  //----------------------------------------

  static PRBool TreatFrameAsBlock(nsIFrame* aFrame);

  //----------------------------------------

  // XXX Move this out of line-layout; make some little interface to
  // deal with it...

  // Add another piece of text to a text-run during FindTextRuns.
  // Note: continuation frames must NOT add themselves; just the
  // first-in-flow
  nsresult AddText(nsIFrame* aTextFrame);

  // Close out a text-run during FindTextRuns.
  void EndTextRun();

  // This returns the first nsTextRun found during a FindTextRuns. The
  // internal text-run state is reset.
  nsTextRun* TakeTextRuns();

  nsIPresContext& mPresContext;

protected:
  // This state is constant for a given block frame doing line layout
  nsISpaceManager* mSpaceManager;
  const nsStyleText* mStyleText;
  const nsHTMLReflowState* mBlockReflowState;
  nsBlockReflowState* mBlockRS;/* XXX hack! */
  nscoord mMinLineHeight;
  PRBool mComputeMaxElementSize;
  PRUint8 mTextAlign;

  // This state varies during the reflow of a line but is line
  // "global" state not span "local" state.
  nsIFrame* mBRFrame;
  nsIFrame* mFirstLetterFrame;
  PRInt32 mLineNumber;
  PRInt32 mColumn;
  PRBool mEndsInWhiteSpace;
  PRBool mUnderstandsWhiteSpace;
  PRBool mFirstLetterStyleOK;
  PRBool mIsTopOfPage;
  PRBool mUpdatedBand;
  PRBool mImpactedByFloaters;
  PRBool mCanPlaceFloater;
  PRUint8 mPlacedFloaters;
  PRInt32 mTotalPlacedFrames;
  nsVoidArray mWordFrames;

  nscoord mTopEdge;
  nscoord mBottomEdge;
  nscoord mMaxTopBoxHeight;
  nscoord mMaxBottomBoxHeight;
  nscoord mCarriedOutBottomMargin;

  nsTextRun* mReflowTextRuns;
  nsTextRun* mTextRun;

  // Per-frame data recorded by the line-layout reflow logic. This
  // state is the state needed to post-process the line after reflow
  // has completed (vertical alignment, horizontal alignment,
  // justification and relative positioning).

#ifdef AIX
public:
#endif /* AIX */
  struct PerSpanData;
  struct PerFrameData {
    // link to next/prev frame in same span
    PerFrameData* mNext;
    PerFrameData* mPrev;

    // pointer to child span data if this is an inline container frame
    PerSpanData* mSpan;

    // The frame and its type
    nsIFrame* mFrame;
    nsCSSFrameType mFrameType;

    // From metrics
    nscoord mAscent, mDescent;
    nsRect mBounds;
    nsSize mMaxElementSize;
    nsRect mCombinedArea;

    // From reflow-state
    nsMargin mMargin;
    nsMargin mBorderPadding;
    nsMargin mOffsets;
    PRBool mRelativePos;

    // Other state we use
    PRUint8 mVerticalAlign;
  };
#ifdef AIX
protected:
#endif /* AIX */
  PerFrameData mFrameDataBuf[NS_LINELAYOUT_NUM_FRAMES];
  PerFrameData* mFrameFreeList;
  PRInt32 mInitialFramesFreed;

#ifdef AIX
public:
#endif /* AIX */
  struct PerSpanData {
    union {
      PerSpanData* mParent;
      PerSpanData* mNextFreeSpan;
    };
    PerFrameData* mFrame;
    PerFrameData* mFirstFrame;
    PerFrameData* mLastFrame;

    const nsHTMLReflowState* mReflowState;
    PRBool mNoWrap;
    PRUint8 mDirection;

    nscoord mLeftEdge;
    nscoord mX;
    nscoord mRightEdge;

    nscoord mTopLeading, mBottomLeading;
    nscoord mLogicalHeight;
    nscoord mMinY, mMaxY;

    void AppendFrame(PerFrameData* pfd) {
      if (nsnull == mLastFrame) {
        mFirstFrame = pfd;
      }
      else {
        mLastFrame->mNext = pfd;
        pfd->mPrev = mLastFrame;
      }
      mLastFrame = pfd;
    }
  };
#ifdef AIX
protected:
#endif /* AIX */
  PerSpanData mSpanDataBuf[NS_LINELAYOUT_NUM_SPANS];
  PerSpanData* mSpanFreeList;
  PRInt32 mInitialSpansFreed;
  PerSpanData* mRootSpan;
  PerSpanData* mCurrentSpan;
  PRInt32 mSpanDepth;
#ifdef DEBUG
  PRInt32 mSpansAllocated, mSpansFreed;
  PRInt32 mFramesAllocated, mFramesFreed;
#endif

  // XXX These slots are used ONLY during FindTextRuns
  nsTextRun* mTextRuns;
  nsTextRun** mTextRunP;
  nsTextRun* mNewTextRun;

  nsresult NewPerFrameData(PerFrameData** aResult);

  nsresult NewPerSpanData(PerSpanData** aResult);

  void FreeSpan(PerSpanData* psd);

  PRBool InBlockContext() const {
    return mSpanDepth == 0;
  }

  void PushFrame(nsIFrame* aFrame);

  void ApplyLeftMargin(PerFrameData* pfd,
                       nsHTMLReflowState& aReflowState);

  PRBool CanPlaceFrame(PerFrameData* pfd,
                       const nsHTMLReflowState& aReflowState,
                       PRBool aNotSafeToBreak,
                       nsHTMLReflowMetrics& aMetrics,
                       nsReflowStatus& aStatus);

  void PlaceFrame(PerFrameData* pfd,
                  nsHTMLReflowMetrics& aMetrics);

  void UpdateFrames();

  void VerticalAlignFrames(PerSpanData* psd);

  void PlaceTopBottomFrames(PerSpanData* psd,
                            nscoord aDistanceFromTop,
                            nscoord aLineHeight);

  void RelativePositionFrames(PerSpanData* psd, nsRect& aCombinedArea);

#ifdef DEBUG
  void DumpPerSpanData(PerSpanData* psd, PRInt32 aIndent);
#endif
};

#endif /* nsLineLayout_h___ */

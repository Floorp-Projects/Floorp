/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:cindent:ts=2:et:sw=2:
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * ***** END LICENSE BLOCK *****
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 VisualAge build.
 */
#ifndef nsLineLayout_h___
#define nsLineLayout_h___

#include "nsFrame.h"
#include "nsDeque.h"
#include "nsLineBox.h"
#include "nsBlockReflowState.h"
#include "plarena.h"

class nsSpaceManager;
class nsPlaceholderFrame;
struct nsStyleText;

class nsLineLayout {
public:
  nsLineLayout(nsPresContext* aPresContext,
               nsSpaceManager* aSpaceManager,
               const nsHTMLReflowState* aOuterReflowState,
               PRBool aComputeMaxElementWidth);
  ~nsLineLayout();

  class ArenaDeque : public nsDeque
  {
  public:
    ArenaDeque() : nsDeque(nsnull) {}
    void *operator new(size_t, PLArenaPool &pool);
    void  operator delete(void *) {} // Dont do anything.  Its Arena memory
  };

  void Init(nsBlockReflowState* aState, nscoord aMinLineHeight,
            PRInt32 aLineNumber) {
    mBlockRS = aState;
    mMinLineHeight = aMinLineHeight;
    mLineNumber = aLineNumber;
  }

  PRInt32 GetColumn() {
    return mColumn;
  }

  void SetColumn(PRInt32 aNewColumn) {
    mColumn = aNewColumn;
  }
  
  PRInt32 GetLineNumber() const {
    return mLineNumber;
  }

  void BeginLineReflow(nscoord aX, nscoord aY,
                       nscoord aWidth, nscoord aHeight,
                       PRBool aImpactedByFloats,
                       PRBool aIsTopOfPage);

  void EndLineReflow();

  void UpdateBand(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                  PRBool aPlacedLeftFloat,
                  nsIFrame* aFloatFrame);

  nsresult BeginSpan(nsIFrame* aFrame,
                     const nsHTMLReflowState* aSpanReflowState,
                     nscoord aLeftEdge,
                     nscoord aRightEdge);

  void EndSpan(nsIFrame* aFrame, nsSize& aSizeResult,
               nscoord* aMaxElementWidth);

  PRInt32 GetCurrentSpanCount() const;

  void SplitLineTo(PRInt32 aNewCount);

  PRBool IsZeroHeight();

  // Reflows the frame and returns the reflow status. aPushedFrame is PR_TRUE
  // if the frame is pushed to the next line because it doesn't fit
  nsresult ReflowFrame(nsIFrame* aFrame,
                       nsReflowStatus& aReflowStatus,
                       nsHTMLReflowMetrics* aMetrics,
                       PRBool& aPushedFrame);

  nsresult AddBulletFrame(nsIFrame* aFrame,
                          const nsHTMLReflowMetrics& aMetrics);

  void RemoveBulletFrame(nsIFrame* aFrame) {
    PushFrame(aFrame);
  }

  void VerticalAlignLine(nsLineBox* aLineBox,
                         nscoord* aMaxElementWidthResult);

  PRBool TrimTrailingWhiteSpace();

  PRBool HorizontalAlignFrames(nsRect& aLineBounds,
                               PRBool aAllowJustify,
                               PRBool aShrinkWrapWidth);

  /**
   * Handle all the relative positioning in the line, compute the
   * combined area (== overflow area) for the line, and handle view
   * sizing/positioning and the setting of NS_FRAME_OUTSIDE_CHILDREN.
   */
  void RelativePositionFrames(nsRect& aCombinedArea);

  //----------------------------------------

  // Supporting methods and data for flags
protected:
#define LL_ENDSINWHITESPACE            0x00000001
#define LL_UNDERSTANDSNWHITESPACE      0x00000002
#define LL_FIRSTLETTERSTYLEOK          0x00000008
#define LL_ISTOPOFPAGE                 0x00000010
#define LL_UPDATEDBAND                 0x00000020
#define LL_IMPACTEDBYFLOATS            0x00000040
#define LL_LASTFLOATWASLETTERFRAME     0x00000080
#define LL_CANPLACEFLOAT               0x00000100
#define LL_LINEENDSINBR                0x00000200
#define LL_LASTFLAG                    LL_LINEENDSINBR

  PRUint16 mFlags;

  void SetFlag(PRUint32 aFlag, PRBool aValue)
  {
    NS_ASSERTION(aFlag<=LL_LASTFLAG, "bad flag");
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
    NS_ASSERTION(aFlag<=LL_LASTFLAG, "bad flag");
    PRBool result = (mFlags & aFlag);
    if (result) return PR_TRUE;
    return PR_FALSE;
  }

public:

  // Support methods for white-space compression and word-wrapping
  // during line reflow

  void SetEndsInWhiteSpace(PRBool aState) {
    SetFlag(LL_ENDSINWHITESPACE, aState);
  }

  PRBool GetEndsInWhiteSpace() const {
    return GetFlag(LL_ENDSINWHITESPACE);
  }

  void SetUnderstandsWhiteSpace(PRBool aSetting) {
    SetFlag(LL_UNDERSTANDSNWHITESPACE, aSetting);
  }

  void SetTextJustificationWeights(PRInt32 aNumSpaces, PRInt32 aNumLetters) {
    mTextJustificationNumSpaces = aNumSpaces;
    mTextJustificationNumLetters = aNumLetters;
  }

  void RecordWordFrame(nsIFrame* aWordFrame) {
    if(mWordFrames || AllocateDeque())
      mWordFrames->Push(aWordFrame);
  }

  PRBool InWord() const {
    return mWordFrames && 0 != mWordFrames->GetSize();
  }

  void ForgetWordFrame(nsIFrame* aFrame);

  void ForgetWordFrames() {
    if(mWordFrames) {
      mWordFrames->Empty();
    }
  }

  nsIFrame* FindNextText(nsPresContext* aPresContext, nsIFrame* aFrame);

  PRBool CanPlaceFloatNow() const;

  PRBool LineIsBreakable() const;

  PRBool GetLineEndsInBR() const 
  { 
    return GetFlag(LL_LINEENDSINBR); 
  }

  void SetLineEndsInBR(PRBool aOn) 
  { 
    SetFlag(LL_LINEENDSINBR, aOn); 
  }

  PRBool InStrictMode() const
  {
    return mCompatMode != eCompatibility_NavQuirks;
  }

  nsCompatibility GetCompatMode() const
  {
    return mCompatMode;
  }

  //----------------------------------------
  // Inform the line-layout about the presence of a floating frame
  // XXX get rid of this: use get-frame-type?
  void InitFloat(nsPlaceholderFrame* aFrame, nsReflowStatus& aReflowStatus) {
    mBlockRS->InitFloat(*this, aFrame, aReflowStatus);
  }

  void AddFloat(nsPlaceholderFrame* aFrame, nsReflowStatus& aReflowStatus) {
    mBlockRS->AddFloat(*this, aFrame, PR_FALSE, aReflowStatus);
  }

  //----------------------------------------

  PRBool GetFirstLetterStyleOK() const {
    return GetFlag(LL_FIRSTLETTERSTYLEOK);
  }

  void SetFirstLetterStyleOK(PRBool aSetting) {
    SetFlag(LL_FIRSTLETTERSTYLEOK, aSetting);
  }

  void SetFirstLetterFrame(nsIFrame* aFrame) {
    mFirstLetterFrame = aFrame;
  }

  //----------------------------------------

  static PRBool TreatFrameAsBlock(nsIFrame* aFrame);

  static PRBool IsPercentageUnitSides(const nsStyleSides* aSides);

  static PRBool IsPercentageAwareReplacedElement(nsPresContext *aPresContext, 
                                                 nsIFrame       *aFrame);


  //----------------------------------------

  nsPresContext* mPresContext;

protected:
  // This state is constant for a given block frame doing line layout
  nsSpaceManager* mSpaceManager;
  const nsStyleText* mStyleText; // for the block
  const nsHTMLReflowState* mBlockReflowState;

  // XXX remove this when landing bug 154892 (splitting absolute positioned frames)
  friend class nsInlineFrame;

  nsBlockReflowState* mBlockRS;/* XXX hack! */
  nsCompatibility mCompatMode;
  nscoord mMinLineHeight;
  PRPackedBool mComputeMaxElementWidth;
  PRUint8 mTextAlign;

  PRUint8 mPlacedFloats;
  
  // The amount of text indent that we applied to this line, needed for
  // max-element-size calculation.
  nscoord mTextIndent;

  // This state varies during the reflow of a line but is line
  // "global" state not span "local" state.
  nsIFrame* mFirstLetterFrame;
  PRInt32 mLineNumber;
  PRInt32 mColumn;
  PRInt32 mTextJustificationNumSpaces;
  PRInt32 mTextJustificationNumLetters;

  nsLineBox* mLineBox;

  PRInt32 mTotalPlacedFrames;
  ArenaDeque *mWordFrames;
  PRBool  AllocateDeque();

  nscoord mTopEdge;
  nscoord mBottomEdge;
  nscoord mMaxTopBoxHeight;
  nscoord mMaxBottomBoxHeight;

  // Final computed line-height value after VerticalAlignFrames for
  // the block has been called.
  nscoord mFinalLineHeight;

  // Per-frame data recorded by the line-layout reflow logic. This
  // state is the state needed to post-process the line after reflow
  // has completed (vertical alignment, horizontal alignment,
  // justification and relative positioning).

  struct PerSpanData;
  struct PerFrameData;
  friend struct PerSpanData;
  friend struct PerFrameData;
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
    nscoord mMaxElementWidth;
    nsRect mCombinedArea;

    // From reflow-state
    nsMargin mMargin;
    nsMargin mBorderPadding;
    nsMargin mOffsets;

    // state for text justification
    PRInt32 mJustificationNumSpaces;
    PRInt32 mJustificationNumLetters;
    
    // Other state we use
    PRUint8 mVerticalAlign;

// PerFrameData flags
#define PFD_RELATIVEPOS                 0x00000001
#define PFD_ISTEXTFRAME                 0x00000002
#define PFD_ISNONEMPTYTEXTFRAME         0x00000004
#define PFD_ISNONWHITESPACETEXTFRAME    0x00000008
#define PFD_ISLETTERFRAME               0x00000010
#define PFD_ISSTICKY                    0x00000020
#define PFD_ISBULLET                    0x00000040
#define PFD_ISPLACEHOLDERFRAME          0x00000080
#define PFD_LASTFLAG                    PFD_ISPLACEHOLDERFRAME

    PRPackedBool mFlags;

    void SetFlag(PRUint32 aFlag, PRBool aValue)
    {
      NS_ASSERTION(aFlag<=PFD_LASTFLAG, "bad flag");
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
      NS_ASSERTION(aFlag<=PFD_LASTFLAG, "bad flag");
      PRBool result = (mFlags & aFlag);
      if (result) return PR_TRUE;
      return PR_FALSE;
    }


    PerFrameData* Last() {
      PerFrameData* pfd = this;
      while (pfd->mNext) {
        pfd = pfd->mNext;
      }
      return pfd;
    }
  };
  PerFrameData* mFrameFreeList;

  struct PerSpanData {
    union {
      PerSpanData* mParent;
      PerSpanData* mNextFreeSpan;
    };
    PerFrameData* mFrame;
    PerFrameData* mFirstFrame;
    PerFrameData* mLastFrame;

    const nsHTMLReflowState* mReflowState;
    PRPackedBool mNoWrap;
    PRUint8 mDirection;
    PRPackedBool mChangedFrameDirection;
    PRPackedBool mZeroEffectiveSpanBox;
    PRPackedBool mContainsFloat;

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
  PerSpanData* mSpanFreeList;
  PerSpanData* mRootSpan;
  PerSpanData* mCurrentSpan;
  PRInt32 mSpanDepth;
#ifdef DEBUG
  PRInt32 mSpansAllocated, mSpansFreed;
  PRInt32 mFramesAllocated, mFramesFreed;
#endif
  PLArenaPool mArena; // Per span and per frame data

  nsresult NewPerFrameData(PerFrameData** aResult);

  nsresult NewPerSpanData(PerSpanData** aResult);

  void FreeSpan(PerSpanData* psd);

  PRBool InBlockContext() const {
    return mSpanDepth == 0;
  }

  void PushFrame(nsIFrame* aFrame);

  void ApplyStartMargin(PerFrameData* pfd,
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

  PRBool TrimTrailingWhiteSpaceIn(PerSpanData* psd, nscoord* aDeltaWidth);


  void ComputeJustificationWeights(PerSpanData* psd, PRInt32* numSpaces, PRInt32* numLetters);

  struct FrameJustificationState {
    PRInt32 mTotalNumSpaces;
    PRInt32 mTotalNumLetters;
    nscoord mTotalWidthForSpaces;
    nscoord mTotalWidthForLetters;
    PRInt32 mNumSpacesProcessed;
    PRInt32 mNumLettersProcessed;
    nscoord mWidthForSpacesProcessed;
    nscoord mWidthForLettersProcessed;
  };
  nscoord ApplyFrameJustification(PerSpanData* aPSD, FrameJustificationState* aState);


#ifdef DEBUG
  void DumpPerSpanData(PerSpanData* psd, PRInt32 aIndent);
#endif
};

#endif /* nsLineLayout_h___ */

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

/* state and methods used while laying out a single line of a block frame */

#ifndef nsLineLayout_h___
#define nsLineLayout_h___

#include "nsFrame.h"
#include "nsDeque.h"
#include "nsLineBox.h"
#include "nsBlockReflowState.h"
#include "plarena.h"
#include "gfxTypes.h"

class nsBlockFrame;

class nsFloatManager;
class nsPlaceholderFrame;
struct nsStyleText;

class nsLineLayout {
public:
  nsLineLayout(nsPresContext* aPresContext,
               nsFloatManager* aFloatManager,
               const nsHTMLReflowState* aOuterReflowState,
               const nsLineList::iterator* aLine);
  ~nsLineLayout();

  void Init(nsBlockReflowState* aState, nscoord aMinLineHeight,
            PRInt32 aLineNumber) {
    mBlockRS = aState;
    mMinLineHeight = aMinLineHeight;
    mLineNumber = aLineNumber;
  }

  PRInt32 GetLineNumber() const {
    return mLineNumber;
  }

  void BeginLineReflow(nscoord aX, nscoord aY,
                       nscoord aWidth, nscoord aHeight,
                       bool aImpactedByFloats,
                       bool aIsTopOfPage);

  void EndLineReflow();

  /**
   * Called when a float has been placed. This method updates the
   * inline frame and span data to account for any change in positions
   * due to available space for the line boxes changing.
   * @param aX/aY/aWidth/aHeight are the new available
   * space rectangle, relative to the containing block.
   * @param aFloatFrame the float frame that was placed.
   */
  void UpdateBand(const nsRect& aNewAvailableSpace,
                  nsIFrame* aFloatFrame);

  nsresult BeginSpan(nsIFrame* aFrame,
                     const nsHTMLReflowState* aSpanReflowState,
                     nscoord aLeftEdge,
                     nscoord aRightEdge,
                     nscoord* aBaseline);

  // Returns the width of the span
  nscoord EndSpan(nsIFrame* aFrame);

  PRInt32 GetCurrentSpanCount() const;

  void SplitLineTo(PRInt32 aNewCount);

  bool IsZeroHeight();

  // Reflows the frame and returns the reflow status. aPushedFrame is PR_TRUE
  // if the frame is pushed to the next line because it doesn't fit
  nsresult ReflowFrame(nsIFrame* aFrame,
                       nsReflowStatus& aReflowStatus,
                       nsHTMLReflowMetrics* aMetrics,
                       bool& aPushedFrame);

  nsresult AddBulletFrame(nsIFrame* aFrame,
                          const nsHTMLReflowMetrics& aMetrics);

  void RemoveBulletFrame(nsIFrame* aFrame) {
    PushFrame(aFrame);
  }

  void VerticalAlignLine();

  bool TrimTrailingWhiteSpace();

  void HorizontalAlignFrames(nsRect& aLineBounds, bool aAllowJustify);

  /**
   * Handle all the relative positioning in the line, compute the
   * combined area (== overflow area) for the line, and handle view
   * sizing/positioning and the setting of the overflow rect.
   */
  void RelativePositionFrames(nsOverflowAreas& aOverflowAreas);

  //----------------------------------------

  // Supporting methods and data for flags
protected:
#define LL_FIRSTLETTERSTYLEOK          0x00000008
#define LL_ISTOPOFPAGE                 0x00000010
#define LL_IMPACTEDBYFLOATS            0x00000040
#define LL_LASTFLOATWASLETTERFRAME     0x00000080
#define LL_LINEISEMPTY                 0x00000100
#define LL_LINEENDSINBR                0x00000200
#define LL_NEEDBACKUP                  0x00000400
#define LL_INFIRSTLINE                 0x00000800
#define LL_GOTLINEBOX                  0x00001000
#define LL_INFIRSTLETTER               0x00002000
#define LL_HASBULLET                   0x00004000
#define LL_DIRTYNEXTLINE               0x00008000
#define LL_LINEATSTART                 0x00010000
#define LL_LASTFLAG                    LL_LINEATSTART

  void SetFlag(PRUint32 aFlag, bool aValue)
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

  bool GetFlag(PRUint32 aFlag) const
  {
    NS_ASSERTION(aFlag<=LL_LASTFLAG, "bad flag");
    return !!(mFlags & aFlag);
  }

public:

  // Support methods for word-wrapping during line reflow

  void SetTextJustificationWeights(PRInt32 aNumSpaces, PRInt32 aNumLetters) {
    mTextJustificationNumSpaces = aNumSpaces;
    mTextJustificationNumLetters = aNumLetters;
  }

  /**
   * @return true if so far during reflow no non-empty content has been
   * placed in the line (according to nsIFrame::IsEmpty())
   */
  bool LineIsEmpty() const
  {
    return GetFlag(LL_LINEISEMPTY);
  }

  /**
   * @return true if so far during reflow no non-empty leaf content
   * (non-collapsed whitespace, replaced element, inline-block, etc) has been
   * placed in the line
   */
  bool LineAtStart() const
  {
    return GetFlag(LL_LINEATSTART);
  }

  bool LineIsBreakable() const;

  bool GetLineEndsInBR() const 
  { 
    return GetFlag(LL_LINEENDSINBR); 
  }

  void SetLineEndsInBR(bool aOn) 
  { 
    SetFlag(LL_LINEENDSINBR, aOn); 
  }

  //----------------------------------------
  // Inform the line-layout about the presence of a floating frame
  // XXX get rid of this: use get-frame-type?
  bool AddFloat(nsIFrame* aFloat, nscoord aAvailableWidth)
  {
    return mBlockRS->AddFloat(this, aFloat, aAvailableWidth);
  }

  void SetTrimmableWidth(nscoord aTrimmableWidth) {
    mTrimmableWidth = aTrimmableWidth;
  }

  //----------------------------------------

  bool GetFirstLetterStyleOK() const {
    return GetFlag(LL_FIRSTLETTERSTYLEOK);
  }

  void SetFirstLetterStyleOK(bool aSetting) {
    SetFlag(LL_FIRSTLETTERSTYLEOK, aSetting);
  }

  bool GetInFirstLetter() const {
    return GetFlag(LL_INFIRSTLETTER);
  }

  void SetInFirstLetter(bool aSetting) {
    SetFlag(LL_INFIRSTLETTER, aSetting);
  }

  bool GetInFirstLine() const {
    return GetFlag(LL_INFIRSTLINE);
  }

  void SetInFirstLine(bool aSetting) {
    SetFlag(LL_INFIRSTLINE, aSetting);
  }

  // Calling this during block reflow ensures that the next line of inlines
  // will be marked dirty, if there is one.
  void SetDirtyNextLine() {
    SetFlag(LL_DIRTYNEXTLINE, PR_TRUE);
  }
  bool GetDirtyNextLine() {
    return GetFlag(LL_DIRTYNEXTLINE);
  }

  //----------------------------------------

  nsPresContext* mPresContext;

  /**
   * Record where an optional break could have been placed. During line reflow,
   * frames containing optional break points (e.g., whitespace in text frames)
   * can call SetLastOptionalBreakPosition to record where a break could
   * have been made, but wasn't because we decided to place more content on
   * the line. For non-text frames, offset 0 means
   * before the content, offset PR_INT32_MAX means after the content.
   * 
   * Currently this is used to handle cases where a single word comprises
   * multiple frames, and the first frame fits on the line but the whole word
   * doesn't. We look back to the last optional break position and
   * reflow the whole line again, forcing a break at that position. The last
   * optional break position could be in a text frame or else after a frame
   * that cannot be part of a text run, so those are the positions we record.
   * 
   * @param aFits set to true if the break position is within the available width.
   * 
   * @param aPriority the priority of the break opportunity. If we are
   * prioritizing break opportunities, we will not set a break if we have
   * already set a break with a higher priority. @see gfxBreakPriority.
   *
   * @return PR_TRUE if we are actually reflowing with forced break position and we
   * should break here
   */
  bool NotifyOptionalBreakPosition(nsIContent* aContent, PRInt32 aOffset,
                                     bool aFits, gfxBreakPriority aPriority) {
    NS_ASSERTION(!aFits || !GetFlag(LL_NEEDBACKUP),
                  "Shouldn't be updating the break position with a break that fits after we've already flagged an overrun");
    // Remember the last break position that fits; if there was no break that fit,
    // just remember the first break
    if ((aFits && aPriority >= mLastOptionalBreakPriority) ||
        !mLastOptionalBreakContent) {
      mLastOptionalBreakContent = aContent;
      mLastOptionalBreakContentOffset = aOffset;
      mLastOptionalBreakPriority = aPriority;
    }
    return aContent && mForceBreakContent == aContent &&
      mForceBreakContentOffset == aOffset;
  }
  /**
   * Like NotifyOptionalBreakPosition, but here it's OK for LL_NEEDBACKUP
   * to be set, because the caller is merely pruning some saved break position(s)
   * that are actually not feasible.
   */
  void RestoreSavedBreakPosition(nsIContent* aContent, PRInt32 aOffset,
                                 gfxBreakPriority aPriority) {
    mLastOptionalBreakContent = aContent;
    mLastOptionalBreakContentOffset = aOffset;
    mLastOptionalBreakPriority = aPriority;
  }
  /**
   * Signal that no backing up will be required after all.
   */
  void ClearOptionalBreakPosition() {
    SetFlag(LL_NEEDBACKUP, PR_FALSE);
    mLastOptionalBreakContent = nsnull;
    mLastOptionalBreakContentOffset = -1;
    mLastOptionalBreakPriority = eNoBreak;
  }
  // Retrieve last set optional break position. When this returns null, no
  // optional break has been recorded (which means that the line can't break yet).
  nsIContent* GetLastOptionalBreakPosition(PRInt32* aOffset,
                                           gfxBreakPriority* aPriority) {
    *aOffset = mLastOptionalBreakContentOffset;
    *aPriority = mLastOptionalBreakPriority;
    return mLastOptionalBreakContent;
  }
  
  /**
   * Check whether frames overflowed the available width and CanPlaceFrame
   * requested backing up to a saved break position.
   */  
  bool NeedsBackup() { return GetFlag(LL_NEEDBACKUP); }
  
  // Line layout may place too much content on a line, overflowing its available
  // width. When that happens, if SetLastOptionalBreakPosition has been
  // used to record an optional break that wasn't taken, we can reflow the line
  // again and force the break to happen at that point (i.e., backtracking
  // to the last choice point).

  // Record that we want to break at the given content+offset (which
  // should have been previously returned by GetLastOptionalBreakPosition
  // from another nsLineLayout).
  void ForceBreakAtPosition(nsIContent* aContent, PRInt32 aOffset) {
    mForceBreakContent = aContent;
    mForceBreakContentOffset = aOffset;
  }
  bool HaveForcedBreakPosition() { return mForceBreakContent != nsnull; }
  PRInt32 GetForcedBreakPosition(nsIContent* aContent) {
    return mForceBreakContent == aContent ? mForceBreakContentOffset : -1;
  }

  /**
   * This can't be null. It usually returns a block frame but may return
   * some other kind of frame when inline frames are reflowed in a non-block
   * context (e.g. MathML or floating first-letter).
   */
  nsIFrame* GetLineContainerFrame() const { return mBlockReflowState->frame; }
  const nsLineList::iterator* GetLine() const {
    return GetFlag(LL_GOTLINEBOX) ? &mLineBox : nsnull;
  }
  nsLineList::iterator* GetLine() {
    return GetFlag(LL_GOTLINEBOX) ? &mLineBox : nsnull;
  }
  
  /**
   * Returns the accumulated advance width of frames before the current frame
   * on the line, plus the line container's left border+padding.
   * This is always positive, the advance width is measured from
   * the right edge for RTL blocks and from the left edge for LTR blocks.
   * In other words, the current frame's distance from the line container's
   * start content edge is:
   * <code>GetCurrentFrameXDistanceFromBlock() - lineContainer->GetUsedBorderAndPadding().left</code>
   * Note the use of <code>.left</code> for both LTR and RTL line containers.
   */
  nscoord GetCurrentFrameXDistanceFromBlock();

protected:
  // This state is constant for a given block frame doing line layout
  nsFloatManager* mFloatManager;
  const nsStyleText* mStyleText; // for the block
  const nsHTMLReflowState* mBlockReflowState;

  nsIContent* mLastOptionalBreakContent;
  nsIContent* mForceBreakContent;
  
  // XXX remove this when landing bug 154892 (splitting absolute positioned frames)
  friend class nsInlineFrame;

  nsBlockReflowState* mBlockRS;/* XXX hack! */

  nsLineList::iterator mLineBox;

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

    // The frame
    nsIFrame* mFrame;

    // From metrics
    nscoord mAscent;
    nsRect mBounds;
    nsOverflowAreas mOverflowAreas;

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
#define PFD_RECOMPUTEOVERFLOW           0x00000020
#define PFD_ISBULLET                    0x00000040
#define PFD_SKIPWHENTRIMMINGWHITESPACE  0x00000080
#define PFD_LASTFLAG                    PFD_SKIPWHENTRIMMINGWHITESPACE

    PRUint8 mFlags;

    void SetFlag(PRUint32 aFlag, bool aValue)
    {
      NS_ASSERTION(aFlag<=PFD_LASTFLAG, "bad flag");
      NS_ASSERTION(aFlag<=PR_UINT8_MAX, "bad flag");
      NS_ASSERTION(aValue==PR_FALSE || aValue==PR_TRUE, "bad value");
      if (aValue) { // set flag
        mFlags |= aFlag;
      }
      else {        // unset flag
        mFlags &= ~aFlag;
      }
    }

    bool GetFlag(PRUint32 aFlag) const
    {
      NS_ASSERTION(aFlag<=PFD_LASTFLAG, "bad flag");
      return !!(mFlags & aFlag);
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
    bool mNoWrap;
    PRUint8 mDirection;
    bool mChangedFrameDirection;
    bool mZeroEffectiveSpanBox;
    bool mContainsFloat;
    bool mHasNonemptyContent;

    nscoord mLeftEdge;
    nscoord mX;
    nscoord mRightEdge;

    nscoord mTopLeading, mBottomLeading;
    nscoord mLogicalHeight;
    nscoord mMinY, mMaxY;
    nscoord* mBaseline;

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

  gfxBreakPriority mLastOptionalBreakPriority;
  PRInt32     mLastOptionalBreakContentOffset;
  PRInt32     mForceBreakContentOffset;

  nscoord mMinLineHeight;
  
  // The amount of text indent that we applied to this line, needed for
  // max-element-size calculation.
  nscoord mTextIndent;

  // This state varies during the reflow of a line but is line
  // "global" state not span "local" state.
  PRInt32 mLineNumber;
  PRInt32 mTextJustificationNumSpaces;
  PRInt32 mTextJustificationNumLetters;

  PRInt32 mTotalPlacedFrames;

  nscoord mTopEdge;
  nscoord mMaxTopBoxHeight;
  nscoord mMaxBottomBoxHeight;

  // Final computed line-height value after VerticalAlignFrames for
  // the block has been called.
  nscoord mFinalLineHeight;
  
  // Amount of trimmable whitespace width for the trailing text frame, if any
  nscoord mTrimmableWidth;

  PRInt32 mSpanDepth;
#ifdef DEBUG
  PRInt32 mSpansAllocated, mSpansFreed;
  PRInt32 mFramesAllocated, mFramesFreed;
#endif
  PLArenaPool mArena; // Per span and per frame data, 4 byte aligned

  PRUint32 mFlags;

  PRUint8 mTextAlign;

  nsresult NewPerFrameData(PerFrameData** aResult);

  nsresult NewPerSpanData(PerSpanData** aResult);

  void FreeSpan(PerSpanData* psd);

  bool InBlockContext() const {
    return mSpanDepth == 0;
  }

  void PushFrame(nsIFrame* aFrame);

  void ApplyStartMargin(PerFrameData* pfd,
                        nsHTMLReflowState& aReflowState);

  bool CanPlaceFrame(PerFrameData* pfd,
                       PRUint8 aFrameDirection,
                       bool aNotSafeToBreak,
                       bool aFrameCanContinueTextRun,
                       bool aCanRollBackBeforeFrame,
                       nsHTMLReflowMetrics& aMetrics,
                       nsReflowStatus& aStatus,
                       bool* aOptionalBreakAfterFits);

  void PlaceFrame(PerFrameData* pfd,
                  nsHTMLReflowMetrics& aMetrics);

  void VerticalAlignFrames(PerSpanData* psd);

  void PlaceTopBottomFrames(PerSpanData* psd,
                            nscoord aDistanceFromTop,
                            nscoord aLineHeight);

  void RelativePositionFrames(PerSpanData* psd, nsOverflowAreas& aOverflowAreas);

  bool TrimTrailingWhiteSpaceIn(PerSpanData* psd, nscoord* aDeltaWidth);

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

  // Apply justification.  The return value is the amount by which the width of
  // the span corresponding to aPSD got increased due to justification.
  nscoord ApplyFrameJustification(PerSpanData* aPSD,
                                  FrameJustificationState* aState);


#ifdef DEBUG
  void DumpPerSpanData(PerSpanData* psd, PRInt32 aIndent);
#endif
};

#endif /* nsLineLayout_h___ */

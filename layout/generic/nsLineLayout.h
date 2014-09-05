/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:cindent:ts=2:et:sw=2:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
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

#include "nsLineBox.h"
#include "nsBlockReflowState.h"
#include "plarena.h"
#include "gfxTypes.h"
#include "WritingModes.h"

class nsFloatManager;
struct nsStyleText;

class nsLineLayout {
public:
  nsLineLayout(nsPresContext* aPresContext,
               nsFloatManager* aFloatManager,
               const nsHTMLReflowState* aOuterReflowState,
               const nsLineList::iterator* aLine);
  ~nsLineLayout();

  void Init(nsBlockReflowState* aState, nscoord aMinLineBSize,
            int32_t aLineNumber) {
    mBlockRS = aState;
    mMinLineBSize = aMinLineBSize;
    mLineNumber = aLineNumber;
  }

  int32_t GetLineNumber() const {
    return mLineNumber;
  }

  void BeginLineReflow(nscoord aICoord, nscoord aBCoord,
                       nscoord aISize, nscoord aBSize,
                       bool aImpactedByFloats,
                       bool aIsTopOfPage,
                       mozilla::WritingMode aWritingMode,
                       nscoord aContainerWidth);

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

  void BeginSpan(nsIFrame* aFrame, const nsHTMLReflowState* aSpanReflowState,
                 nscoord aLeftEdge, nscoord aRightEdge, nscoord* aBaseline);

  // Returns the width of the span
  nscoord EndSpan(nsIFrame* aFrame);

  int32_t GetCurrentSpanCount() const;

  void SplitLineTo(int32_t aNewCount);

  bool IsZeroBSize();

  // Reflows the frame and returns the reflow status. aPushedFrame is true
  // if the frame is pushed to the next line because it doesn't fit.
  void ReflowFrame(nsIFrame* aFrame,
                   nsReflowStatus& aReflowStatus,
                   nsHTMLReflowMetrics* aMetrics,
                   bool& aPushedFrame);

  void AddBulletFrame(nsIFrame* aFrame, const nsHTMLReflowMetrics& aMetrics);

  void RemoveBulletFrame(nsIFrame* aFrame) {
    PushFrame(aFrame);
  }

  /**
   * Place frames in the block direction (CSS property vertical-align)
   */
  void VerticalAlignLine();

  bool TrimTrailingWhiteSpace();

  /**
   * Place frames in the inline direction (CSS property text-align).
   */
  void TextAlignLine(nsLineBox* aLine, bool aIsLastLine);

  /**
   * Handle all the relative positioning in the line, compute the
   * combined area (== overflow area) for the line, and handle view
   * sizing/positioning and the setting of the overflow rect.
   */
  void RelativePositionFrames(nsOverflowAreas& aOverflowAreas);

  // Support methods for word-wrapping during line reflow

  void SetTextJustificationWeights(int32_t aNumSpaces, int32_t aNumLetters) {
    mTextJustificationNumSpaces = aNumSpaces;
    mTextJustificationNumLetters = aNumLetters;
  }

  /**
   * @return true if so far during reflow no non-empty content has been
   * placed in the line (according to nsIFrame::IsEmpty())
   */
  bool LineIsEmpty() const
  {
    return mLineIsEmpty;
  }

  /**
   * @return true if so far during reflow no non-empty leaf content
   * (non-collapsed whitespace, replaced element, inline-block, etc) has been
   * placed in the line
   */
  bool LineAtStart() const
  {
    return mLineAtStart;
  }

  bool LineIsBreakable() const;

  bool GetLineEndsInBR() const 
  { 
    return mLineEndsInBR;
  }

  void SetLineEndsInBR(bool aOn) 
  { 
    mLineEndsInBR = aOn;
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
    return mFirstLetterStyleOK;
  }

  void SetFirstLetterStyleOK(bool aSetting) {
    mFirstLetterStyleOK = aSetting;
  }

  bool GetInFirstLetter() const {
    return mInFirstLetter;
  }

  void SetInFirstLetter(bool aSetting) {
    mInFirstLetter = aSetting;
  }

  bool GetInFirstLine() const {
    return mInFirstLine;
  }

  void SetInFirstLine(bool aSetting) {
    mInFirstLine = aSetting;
  }

  // Calling this during block reflow ensures that the next line of inlines
  // will be marked dirty, if there is one.
  void SetDirtyNextLine() {
    mDirtyNextLine = true;
  }
  bool GetDirtyNextLine() {
    return mDirtyNextLine;
  }

  //----------------------------------------

  nsPresContext* mPresContext;

  /**
   * Record where an optional break could have been placed. During line reflow,
   * frames containing optional break points (e.g., whitespace in text frames)
   * can call SetLastOptionalBreakPosition to record where a break could
   * have been made, but wasn't because we decided to place more content on
   * the line. For non-text frames, offset 0 means
   * before the content, offset INT32_MAX means after the content.
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
   * @return true if we are actually reflowing with forced break position and we
   * should break here
   */
  bool NotifyOptionalBreakPosition(nsIContent* aContent, int32_t aOffset,
                                     bool aFits, gfxBreakPriority aPriority) {
    NS_ASSERTION(!aFits || !mNeedBackup,
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
   * Like NotifyOptionalBreakPosition, but here it's OK for mNeedBackup
   * to be set, because the caller is merely pruning some saved break position(s)
   * that are actually not feasible.
   */
  void RestoreSavedBreakPosition(nsIContent* aContent, int32_t aOffset,
                                 gfxBreakPriority aPriority) {
    mLastOptionalBreakContent = aContent;
    mLastOptionalBreakContentOffset = aOffset;
    mLastOptionalBreakPriority = aPriority;
  }
  /**
   * Signal that no backing up will be required after all.
   */
  void ClearOptionalBreakPosition() {
    mNeedBackup = false;
    mLastOptionalBreakContent = nullptr;
    mLastOptionalBreakContentOffset = -1;
    mLastOptionalBreakPriority = gfxBreakPriority::eNoBreak;
  }
  // Retrieve last set optional break position. When this returns null, no
  // optional break has been recorded (which means that the line can't break yet).
  nsIContent* GetLastOptionalBreakPosition(int32_t* aOffset,
                                           gfxBreakPriority* aPriority) {
    *aOffset = mLastOptionalBreakContentOffset;
    *aPriority = mLastOptionalBreakPriority;
    return mLastOptionalBreakContent;
  }
  
  /**
   * Check whether frames overflowed the available width and CanPlaceFrame
   * requested backing up to a saved break position.
   */  
  bool NeedsBackup() { return mNeedBackup; }
  
  // Line layout may place too much content on a line, overflowing its available
  // width. When that happens, if SetLastOptionalBreakPosition has been
  // used to record an optional break that wasn't taken, we can reflow the line
  // again and force the break to happen at that point (i.e., backtracking
  // to the last choice point).

  // Record that we want to break at the given content+offset (which
  // should have been previously returned by GetLastOptionalBreakPosition
  // from another nsLineLayout).
  void ForceBreakAtPosition(nsIContent* aContent, int32_t aOffset) {
    mForceBreakContent = aContent;
    mForceBreakContentOffset = aOffset;
  }
  bool HaveForcedBreakPosition() { return mForceBreakContent != nullptr; }
  int32_t GetForcedBreakPosition(nsIContent* aContent) {
    return mForceBreakContent == aContent ? mForceBreakContentOffset : -1;
  }

  /**
   * This can't be null. It usually returns a block frame but may return
   * some other kind of frame when inline frames are reflowed in a non-block
   * context (e.g. MathML or floating first-letter).
   */
  nsIFrame* LineContainerFrame() const { return mBlockReflowState->frame; }
  const nsHTMLReflowState* LineContainerRS() const { return mBlockReflowState; }
  const nsLineList::iterator* GetLine() const {
    return mGotLineBox ? &mLineBox : nullptr;
  }
  nsLineList::iterator* GetLine() {
    return mGotLineBox ? &mLineBox : nullptr;
  }
  
  /**
   * Returns the accumulated advance width of frames before the current frame
   * on the line, plus the line container's left border+padding.
   * This is always positive, the advance width is measured from
   * the right edge for RTL blocks and from the left edge for LTR blocks.
   * In other words, the current frame's distance from the line container's
   * start content edge is:
   * <code>GetCurrentFrameInlineDistanceFromBlock() - lineContainer->GetUsedBorderAndPadding().left</code>
   * Note the use of <code>.left</code> for both LTR and RTL line containers.
   */
  nscoord GetCurrentFrameInlineDistanceFromBlock();

  /**
   * Move the inline position where the next frame will be reflowed forward by
   * aAmount.
   */
  void AdvanceICoord(nscoord aAmount);
  /**
   * Returns the writing mode for the root span.
   */
  mozilla::WritingMode GetWritingMode();
  /**
   * Returns the inline position where the next frame will be reflowed.
   */
  nscoord GetCurrentICoord();

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
  // has completed (block-direction alignment, inline-direction alignment,
  // justification and relative positioning).

  struct PerSpanData;
  struct PerFrameData;
  friend struct PerSpanData;
  friend struct PerFrameData;
  struct PerFrameData
  {
    explicit PerFrameData(mozilla::WritingMode aWritingMode)
      : mBounds(aWritingMode)
      , mMargin(aWritingMode)
      , mBorderPadding(aWritingMode)
      , mOffsets(aWritingMode)
    {}

    // link to next/prev frame in same span
    PerFrameData* mNext;
    PerFrameData* mPrev;

    // pointer to child span data if this is an inline container frame
    PerSpanData* mSpan;

    // The frame
    nsIFrame* mFrame;

    // From metrics
    nscoord mAscent;
    // note that mBounds is a logical rect in the *line*'s writing mode.
    // When setting frame coordinates, we have to convert to the frame's
    //  writing mode
    mozilla::LogicalRect mBounds;
    nsOverflowAreas mOverflowAreas;

    // From reflow-state
    mozilla::LogicalMargin mMargin;
    mozilla::LogicalMargin mBorderPadding;
    mozilla::LogicalMargin mOffsets;

    // state for text justification
    int32_t mJustificationNumSpaces;
    int32_t mJustificationNumLetters;
    
    // Other state we use
    uint8_t mBlockDirAlign;

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

    uint8_t mFlags;

    void SetFlag(uint32_t aFlag, bool aValue)
    {
      NS_ASSERTION(aFlag<=PFD_LASTFLAG, "bad flag");
      NS_ASSERTION(aFlag<=UINT8_MAX, "bad flag");
      if (aValue) { // set flag
        mFlags |= aFlag;
      }
      else {        // unset flag
        mFlags &= ~aFlag;
      }
    }

    bool GetFlag(uint32_t aFlag) const
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
    mozilla::WritingMode mWritingMode;
    bool mZeroEffectiveSpanBox;
    bool mContainsFloat;
    bool mHasNonemptyContent;

    nscoord mIStart;
    nscoord mICoord;
    nscoord mIEnd;

    nscoord mBStartLeading, mBEndLeading;
    nscoord mLogicalBSize;
    nscoord mMinBCoord, mMaxBCoord;
    nscoord* mBaseline;

    void AppendFrame(PerFrameData* pfd) {
      if (nullptr == mLastFrame) {
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
  int32_t     mLastOptionalBreakContentOffset;
  int32_t     mForceBreakContentOffset;

  nscoord mMinLineBSize;
  
  // The amount of text indent that we applied to this line, needed for
  // max-element-size calculation.
  nscoord mTextIndent;

  // This state varies during the reflow of a line but is line
  // "global" state not span "local" state.
  int32_t mLineNumber;
  int32_t mTextJustificationNumSpaces;
  int32_t mTextJustificationNumLetters;

  int32_t mTotalPlacedFrames;

  nscoord mBStartEdge;
  nscoord mMaxStartBoxBSize;
  nscoord mMaxEndBoxBSize;

  nscoord mInflationMinFontSize;

  // Final computed line-bSize value after VerticalAlignFrames for
  // the block has been called.
  nscoord mFinalLineBSize;
  
  // Amount of trimmable whitespace width for the trailing text frame, if any
  nscoord mTrimmableWidth;

  nscoord mContainerWidth;

  bool mFirstLetterStyleOK      : 1;
  bool mIsTopOfPage             : 1;
  bool mImpactedByFloats        : 1;
  bool mLastFloatWasLetterFrame : 1;
  bool mLineIsEmpty             : 1;
  bool mLineEndsInBR            : 1;
  bool mNeedBackup              : 1;
  bool mInFirstLine             : 1;
  bool mGotLineBox              : 1;
  bool mInFirstLetter           : 1;
  bool mHasBullet               : 1;
  bool mDirtyNextLine           : 1;
  bool mLineAtStart             : 1;

  int32_t mSpanDepth;
#ifdef DEBUG
  int32_t mSpansAllocated, mSpansFreed;
  int32_t mFramesAllocated, mFramesFreed;
#endif
  PLArenaPool mArena; // Per span and per frame data, 4 byte aligned

  /**
   * Allocate a PerFrameData from the mArena pool. The allocation is infallible.
   */
  PerFrameData* NewPerFrameData(nsIFrame* aFrame);

  /**
   * Allocate a PerSpanData from the mArena pool. The allocation is infallible.
   */
  PerSpanData* NewPerSpanData();

  void FreeSpan(PerSpanData* psd);

  bool InBlockContext() const {
    return mSpanDepth == 0;
  }

  void PushFrame(nsIFrame* aFrame);

  void AllowForStartMargin(PerFrameData* pfd,
                           nsHTMLReflowState& aReflowState);

  bool CanPlaceFrame(PerFrameData* pfd,
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
                            nscoord aDistanceFromStart,
                            nscoord aLineBSize);

  void RelativePositionFrames(PerSpanData* psd, nsOverflowAreas& aOverflowAreas);

  bool TrimTrailingWhiteSpaceIn(PerSpanData* psd, nscoord* aDeltaISize);

  void ComputeJustificationWeights(PerSpanData* psd, int32_t* numSpaces, int32_t* numLetters);

  struct FrameJustificationState {
    int32_t mTotalNumSpaces;
    int32_t mTotalNumLetters;
    nscoord mTotalWidthForSpaces;
    nscoord mTotalWidthForLetters;
    int32_t mNumSpacesProcessed;
    int32_t mNumLettersProcessed;
    nscoord mWidthForSpacesProcessed;
    nscoord mWidthForLettersProcessed;
  };

  // Apply justification.  The return value is the amount by which the width of
  // the span corresponding to aPSD got increased due to justification.
  nscoord ApplyFrameJustification(PerSpanData* aPSD,
                                  FrameJustificationState* aState);


#ifdef DEBUG
  void DumpPerSpanData(PerSpanData* psd, int32_t aIndent);
#endif
};

#endif /* nsLineLayout_h___ */

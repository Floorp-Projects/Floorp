/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert O'Callahan <robert@ocallahan.org>
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Prabhat Hegde <prabhat.hegde@sun.com>
 *   Tomi Leppikangas <tomi.leppikangas@oulu.fi>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Daniel Glazman <glazman@netscape.com>
 *   Neil Deakin <neil@mozdevgroup.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *   Uri Bernstein <uriber@gmail.com>
 *   Stephen Blackheath <entangled.mooched.stephen@blacksapphire.com>
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
 * ***** END LICENSE BLOCK ***** */

/* rendering object for textual content of elements */

#include "nsCOMPtr.h"
#include "nsHTMLParts.h"
#include "nsCRT.h"
#include "nsSplittableFrame.h"
#include "nsLineLayout.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsPresContext.h"
#include "nsIContent.h"
#include "nsStyleConsts.h"
#include "nsStyleContext.h"
#include "nsCoord.h"
#include "nsIFontMetrics.h"
#include "nsIRenderingContext.h"
#include "nsIPresShell.h"
#include "nsITimer.h"
#include "prtime.h"
#include "nsVoidArray.h"
#include "prprf.h"
#include "nsIDOMText.h"
#include "nsIDocument.h"
#include "nsIDeviceContext.h"
#include "nsICaret.h"
#include "nsCSSPseudoElements.h"
#include "nsCompatibility.h"
#include "nsCSSColorUtils.h"
#include "nsLayoutUtils.h"
#include "nsDisplayList.h"
#include "nsFrame.h"
#include "nsTextTransformer.h"
#include "nsTextFrameUtils.h"
#include "nsTextRunTransformations.h"
#include "nsFrameManager.h"

#include "nsTextFragment.h"
#include "nsGkAtoms.h"
#include "nsFrameSelection.h"
#include "nsISelection.h"
#include "nsIDOMRange.h"
#include "nsILookAndFeel.h"
#include "nsCSSRendering.h"
#include "nsContentUtils.h"
#include "nsLineBreaker.h"

#include "nsILineIterator.h"

#include "nsIServiceManager.h"
#ifdef ACCESSIBILITY
#include "nsIAccessible.h"
#include "nsIAccessibilityService.h"
#endif
#include "nsAutoPtr.h"
#include "nsStyleSet.h"

#include "nsBidiFrames.h"
#include "nsBidiPresUtils.h"
#include "nsBidiUtils.h"

#include "nsIThebesFontMetrics.h"
#include "gfxFont.h"
#include "gfxContext.h"

#ifdef NS_DEBUG
#undef NOISY_BLINK
#undef NOISY_REFLOW
#undef NOISY_TRIM
#else
#undef NOISY_BLINK
#undef NOISY_REFLOW
#undef NOISY_TRIM
#endif

// The following flags are set during reflow

// This bit is set on the first frame in a continuation indicating
// that it was chopped short because of :first-letter style.
#define TEXT_FIRST_LETTER    0x00100000
// This bit is set on frames that are logically adjacent to the start of the
// line (i.e. no prior frame on line with actual displayed in-flow content).
#define TEXT_START_OF_LINE   0x00200000
// This bit is set on frames that are logically adjacent to the end of the
// line (i.e. no following on line with actual displayed in-flow content).
#define TEXT_END_OF_LINE     0x00400000
// This bit is set on frames that end with a hyphenated break.
#define TEXT_HYPHEN_BREAK    0x00800000
// This bit is set on frames that trimmed trailing whitespace characters when
// calculating their width during reflow.
#define TEXT_TRIMMED_TRAILING_WHITESPACE 0x01000000

#define TEXT_REFLOW_FLAGS    \
  (TEXT_FIRST_LETTER|TEXT_START_OF_LINE|TEXT_END_OF_LINE|TEXT_HYPHEN_BREAK| \
   TEXT_TRIMMED_TRAILING_WHITESPACE)

// Cache bits for IsEmpty().
// Set this bit if the textframe is known to be only collapsible whitespace.
#define TEXT_IS_ONLY_WHITESPACE    0x08000000
// Set this bit if the textframe is known to be not only collapsible whitespace.
#define TEXT_ISNOT_ONLY_WHITESPACE 0x10000000

#define TEXT_WHITESPACE_FLAGS      0x18000000

// This bit is set if this frame is the owner of the textrun (i.e., occurs
// as the mStartFrame of some flow associated with the textrun)
#define TEXT_IS_RUN_OWNER          0x20000000

// This bit is set while the frame is registered as a blinking frame.
#define TEXT_BLINK_ON              0x80000000

/*
 * Some general notes
 * 
 * Text frames delegate work to gfxTextRun objects. The gfxTextRun object
 * transforms text to positioned glyphs. It can report the geometry of the
 * glyphs and paint them. Text frames configure gfxTextRuns by providing text,
 * spacing, language, and other information.
 * 
 * A gfxTextRun can cover more than one DOM text node. This is necessary to
 * get kerning, ligatures and shaping for text that spans multiple text nodes
 * but is all the same font. The userdata for a gfxTextRun object is a
 * TextRunUserData* or an nsIFrame*.
 * 
 * We go to considerable effort to make sure things work even if in-flow
 * siblings have different style contexts (i.e., first-letter and first-line).
 * 
 * Tabs in preformatted text act as if they expand to 1-8 spaces, so that the
 * number of clusters on the line up to the end of the tab is a multiple of 8.
 * This is implemented by transforming tabs to spaces before handing off text
 * to the text run, and then configuring the spacing after the tab to be the
 * width of 0-7 spaces.
 * 
 * Our convention is that unsigned integer character offsets are offsets into
 * the transformed string. Signed integer character offsets are offsets into
 * the DOM string.
 * 
 * XXX currently we don't handle hyphenated breaks between text frames where the
 * hyphen occurs at the end of the first text frame, e.g.
 *   <b>Kit&shy;</b>ty
 */

class nsTextFrame;
class PropertyProvider;

/**
 * We use an array of these objects to record which text frames
 * are associated with the textrun. mStartFrame is the start of a list of
 * text frames. Some sequence of its continuations are covered by the textrun.
 * A content textnode can have at most one TextRunMappedFlow associated with it
 * for a given textrun.
 * 
 * mDOMOffsetToBeforeTransformOffset is added to DOM offsets for those frames to obtain
 * the offset into the before-transformation text of the textrun. It can be
 * positive (when a text node starts in the middle of a text run) or
 * negative (when a text run starts in the middle of a text node).
 * 
 * mStartFrame has TEXT_IS_RUN_OWNER set.
 */
struct TextRunMappedFlow {
  nsTextFrame* mStartFrame;
  PRInt32      mDOMOffsetToBeforeTransformOffset;
  // The text mapped starts at mStartFrame->GetContentOffset() and is this long
  PRUint32     mContentLength;
};

/**
 * This is our user data for the textrun. When there is no user data, the
 * textrun maps exactly the in-flow relatives of the current frame and the
 * first-in-flow's offset into the text run is 0.
 */
struct TextRunUserData {
  TextRunMappedFlow* mMappedFlows;
  PRInt32            mMappedFlowCount;

  PRUint32           mLastFlowIndex;
};

/**
 * This helper object computes colors used for painting, and also IME
 * underline information. The data is computed lazily and cached as necessary.
 * These live for just the duration of one paint operation.
 */
class nsTextPaintStyle {
public:
  nsTextPaintStyle(nsTextFrame* aFrame);

  nscolor GetTextColor();
  /**
   * Compute the colors for normally-selected text. Returns false if
   * the normal selection is not being displayed.
   */
  PRBool GetSelectionColors(nscolor* aForeColor,
                            nscolor* aBackColor);
  void GetIMESelectionColors(PRInt32  aIndex,
                             nscolor* aForeColor,
                             nscolor* aBackColor);
  // if this returns PR_FALSE, we don't need to draw underline.
  PRBool GetIMEUnderline(PRInt32  aIndex,
                         nscolor* aLineColor,
                         float*   aRelativeSize);

  nsPresContext* GetPresContext() { return mPresContext; }

  enum {
    eIndexRawInput = 0,
    eIndexSelRawText,
    eIndexConvText,
    eIndexSelConvText
  };

protected:
  nsTextFrame*   mFrame;
  nsPresContext* mPresContext;
  PRPackedBool   mInitCommonColors;
  PRPackedBool   mInitSelectionColors;

  // Selection data

  PRInt16      mSelectionStatus; // see nsIDocument.h SetDisplaySelection()
  nscolor      mSelectionTextColor;
  nscolor      mSelectionBGColor;

  // Common data

  PRInt32 mSufficientContrast;
  nscolor mFrameBackgroundColor;

  // IME selection colors and underline info
  struct nsIMEColor {
    PRBool mInit;
    nscolor mTextColor;
    nscolor mBGColor;
    nscolor mUnderlineColor;
  };
  nsIMEColor mIMEColor[4];
  // indices
  float mIMEUnderlineRelativeSize;

  // Color initializations
  void InitCommonColors();
  PRBool InitSelectionColors();

  nsIMEColor* GetIMEColor(PRInt32 aIndex);
  void InitIMEColor(PRInt32 aIndex);

  PRBool EnsureSufficientContrast(nscolor *aForeColor, nscolor *aBackColor);

  nscolor GetResolvedForeColor(nscolor aColor, nscolor aDefaultForeColor,
                               nscolor aBackColor);
};

class nsTextFrame : public nsFrame {
public:
  nsTextFrame(nsStyleContext* aContext) : nsFrame(aContext)
  {
    NS_ASSERTION(mContentOffset == 0, "Bogus content offset");
    NS_ASSERTION(mContentLength == 0, "Bogus content length");
  }
  
  // nsIFrame
  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);

  virtual void Destroy();
  
  NS_IMETHOD GetCursor(const nsPoint& aPoint,
                       nsIFrame::Cursor& aCursor);
  
  NS_IMETHOD CharacterDataChanged(nsPresContext* aPresContext,
                                  nsIContent*     aChild,
                                  PRBool          aAppend);
                                  
  NS_IMETHOD DidSetStyleContext();
  
  virtual nsIFrame* GetNextContinuation() const {
    return mNextContinuation;
  }
  NS_IMETHOD SetNextContinuation(nsIFrame* aNextContinuation) {
    NS_ASSERTION (!aNextContinuation || GetType() == aNextContinuation->GetType(),
                  "setting a next continuation with incorrect type!");
    NS_ASSERTION (!nsSplittableFrame::IsInNextContinuationChain(aNextContinuation, this),
                  "creating a loop in continuation chain!");
    mNextContinuation = aNextContinuation;
    if (aNextContinuation)
      aNextContinuation->RemoveStateBits(NS_FRAME_IS_FLUID_CONTINUATION);
    return NS_OK;
  }
  virtual nsIFrame* GetNextInFlowVirtual() const { return GetNextInFlow(); }
  nsIFrame* GetNextInFlow() const {
    return mNextContinuation && (mNextContinuation->GetStateBits() & NS_FRAME_IS_FLUID_CONTINUATION) ? 
      mNextContinuation : nsnull;
  }
  NS_IMETHOD SetNextInFlow(nsIFrame* aNextInFlow) {
    NS_ASSERTION (!aNextInFlow || GetType() == aNextInFlow->GetType(),
                  "setting a next in flow with incorrect type!");
    NS_ASSERTION (!nsSplittableFrame::IsInNextContinuationChain(aNextInFlow, this),
                  "creating a loop in continuation chain!");
    mNextContinuation = aNextInFlow;
    if (aNextInFlow)
      aNextInFlow->AddStateBits(NS_FRAME_IS_FLUID_CONTINUATION);
    return NS_OK;
  }
  virtual nsIFrame* GetLastInFlow() const;
  virtual nsIFrame* GetLastContinuation() const;
  
  virtual nsSplittableType GetSplittableType() const {
    return NS_FRAME_SPLITTABLE;
  }
  
  /**
    * Get the "type" of the frame
   *
   * @see nsGkAtoms::textFrame
   */
  virtual nsIAtom* GetType() const;
  
  virtual PRBool IsFrameOfType(PRUint32 aFlags) const
  {
    // Set the frame state bit for text frames to mark them as replaced.
    // XXX kipp: temporary
    return nsFrame::IsFrameOfType(aFlags & ~(nsIFrame::eReplaced));
  }

#ifdef DEBUG
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
  NS_IMETHOD_(nsFrameState) GetDebugStateBits() const ;
#endif
  
  virtual ContentOffsets CalcContentOffsetsFromFramePoint(nsPoint aPoint);
   
  NS_IMETHOD SetSelected(nsPresContext* aPresContext,
                         nsIDOMRange *aRange,
                         PRBool aSelected,
                         nsSpread aSpread);
  
  virtual PRBool PeekOffsetNoAmount(PRBool aForward, PRInt32* aOffset);
  virtual PRBool PeekOffsetCharacter(PRBool aForward, PRInt32* aOffset);
  virtual PRBool PeekOffsetWord(PRBool aForward, PRBool aWordSelectEatSpace, PRBool aIsKeyboardSelect,
                                PRInt32* aOffset, PRBool* aSawBeforeType);

  NS_IMETHOD CheckVisibility(nsPresContext* aContext, PRInt32 aStartIndex, PRInt32 aEndIndex, PRBool aRecurse, PRBool *aFinished, PRBool *_retval);
  
  NS_IMETHOD GetOffsets(PRInt32 &start, PRInt32 &end)const;
  
  virtual void AdjustOffsetsForBidi(PRInt32 start, PRInt32 end);
  
  NS_IMETHOD GetPointFromOffset(nsPresContext*         inPresContext,
                                nsIRenderingContext*    inRendContext,
                                PRInt32                 inOffset,
                                nsPoint*                outPoint);
  
  NS_IMETHOD  GetChildFrameContainingOffset(PRInt32     inContentOffset,
                                            PRBool                  inHint,
                                            PRInt32*                outFrameContentOffset,
                                            nsIFrame*               *outChildFrame);
  
  virtual PRBool IsVisibleInSelection(nsISelection* aSelection);
  
  virtual PRBool IsEmpty();
  virtual PRBool IsSelfEmpty() { return IsEmpty(); }
  
  /**
   * @return PR_TRUE if this text frame ends with a newline character.  It
   * should return PR_FALSE if this is not a text frame.
   */
  virtual PRBool HasTerminalNewline() const;
  
#ifdef ACCESSIBILITY
  NS_IMETHOD GetAccessible(nsIAccessible** aAccessible);
#endif
  
  virtual nscoord GetMinWidth(nsIRenderingContext *aRenderingContext);
  virtual nscoord GetPrefWidth(nsIRenderingContext *aRenderingContext);
  virtual void AddInlineMinWidth(nsIRenderingContext *aRenderingContext,
                                 InlineMinWidthData *aData);
  virtual void AddInlinePrefWidth(nsIRenderingContext *aRenderingContext,
                                  InlinePrefWidthData *aData);
  virtual nsSize ComputeSize(nsIRenderingContext *aRenderingContext,
                             nsSize aCBSize, nscoord aAvailableWidth,
                             nsSize aMargin, nsSize aBorder, nsSize aPadding,
                             PRBool aShrinkWrap);
  NS_IMETHOD Reflow(nsPresContext* aPresContext,
                    nsHTMLReflowMetrics& aMetrics,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);
  virtual PRBool CanContinueTextRun() const;
  NS_IMETHOD TrimTrailingWhiteSpace(nsPresContext* aPresContext,
                                    nsIRenderingContext& aRC,
                                    nscoord& aDeltaWidth,
                                    PRBool& aLastCharIsJustifiable);

  void AddInlineMinWidthForFlow(nsIRenderingContext *aRenderingContext,
                                nsIFrame::InlineMinWidthData *aData);
  void AddInlinePrefWidthForFlow(nsIRenderingContext *aRenderingContext,
                                 InlinePrefWidthData *aData);

  // primary frame paint method called from nsDisplayText
  void PaintText(nsIRenderingContext* aRenderingContext, nsPoint aPt,
                 const nsRect& aDirtyRect);
  // helper: paint quirks-mode CSS text decorations
  void PaintTextDecorations(gfxContext* aCtx, const gfxRect& aDirtyRect,
                            const gfxPoint& aFramePt, nsTextPaintStyle& aTextStyle,
                            PropertyProvider& aProvider);
  // helper: paint text frame when we're impacted by at least one selection.
  // Return PR_FALSE if the text was not painted and we should continue with
  // the fast path.
  PRBool PaintTextWithSelection(gfxContext* aCtx,
                                const gfxPoint& aFramePt,
                                const gfxPoint& aTextBaselinePt,
                                const gfxRect& aDirtyRect,
                                PropertyProvider& aProvider,
                                nsTextPaintStyle& aTextPaintStyle);
  // helper: paint text with foreground and background colors determined
  // by selection(s). Also computes a mask of all selection types applying to
  // our text, returned in aAllTypes.
  void PaintTextWithSelectionColors(gfxContext* aCtx,
                                    const gfxPoint& aFramePt,
                                    const gfxPoint& aTextBaselinePt,
                                    const gfxRect& aDirtyRect,
                                    PropertyProvider& aProvider,
                                    nsTextPaintStyle& aTextPaintStyle,
                                    SelectionDetails* aDetails,
                                    SelectionType* aAllTypes);
  // helper: paint text decorations for text selected by aSelectionType
  void PaintTextSelectionDecorations(gfxContext* aCtx,
                                     const gfxPoint& aFramePt,
                                     const gfxPoint& aTextBaselinePt,
                                     const gfxRect& aDirtyRect,
                                     PropertyProvider& aProvider,
                                     nsTextPaintStyle& aTextPaintStyle,
                                     SelectionDetails* aDetails,
                                     SelectionType aSelectionType);

  PRInt16 GetSelectionStatus(PRInt16* aSelectionFlags);

#ifdef DEBUG
  void ToCString(nsString& aBuf, PRInt32* aTotalContentLength) const;
#endif

  PRInt32 GetContentOffset() { return mContentOffset; }
  PRInt32 GetContentLength() { return mContentLength; }

  // Compute the length of the content mapped by this frame
  // and all its in-flow siblings. Basically this means starting at mContentOffset
  // and going to the end of the text node or the next bidi continuation
  // boundary.
  PRInt32 GetInFlowContentLength();

  // Clears out mTextRun from this frame and all other frames that hold a reference
  // to it, then deletes the textrun.
  void ClearTextRun();
  /**
   * Acquires the text run for this content, if necessary.
   * @param aRC the rendering context to use as a reference for creating
   * the textrun, if available (if not, we'll create one which will just be slower)
   * @param aBlock the block ancestor for this frame, or nsnull if unknown
   * @param aLine the line that this frame is on, if any, or nsnull if unknown
   * @return a gfxSkipCharsIterator set up to map DOM offsets for this frame
   * to offsets into the textrun; its initial offset is set to this frame's
   * content offset
   */
  gfxSkipCharsIterator EnsureTextRun(nsIRenderingContext* aRC = nsnull,
                                     nsBlockFrame* aBlock = nsnull,
                                     const nsLineList::iterator* aLine = nsnull,
                                     PRUint32* aFlowEndInTextRun = nsnull);

  gfxTextRun* GetTextRun() { return mTextRun; }
  void SetTextRun(gfxTextRun* aTextRun) { mTextRun = aTextRun; }
  
  PRInt32 GetColumn() { return mColumn; }

  // Get the DOM content range mapped by this frame after excluding
  // whitespace subject to start-of-line and end-of-line trimming.
  // The textrun must have been created before calling this.
  struct TrimmedOffsets {
    PRInt32 mStart;
    PRInt32 mLength;
  };
  TrimmedOffsets GetTrimmedOffsets(const nsTextFragment* aFrag,
                                   PRBool aTrimAfter);

protected:
  virtual ~nsTextFrame();
  
  nsIFrame*   mNextContinuation;
  PRInt32     mContentOffset;
  PRInt32     mContentLength;
  PRInt32     mColumn;
  nscoord     mAscent;
  gfxTextRun* mTextRun;

  SelectionDetails* GetSelectionDetails();
  
  void AdjustSelectionPointsForBidi(SelectionDetails *sdptr,
                                    PRInt32 textLength,
                                    PRBool isRTLChars,
                                    PRBool isOddLevel,
                                    PRBool isBidiSystem);
  
  void SetOffsets(PRInt32 start, PRInt32 end);
};

PRInt32 nsTextFrame::GetInFlowContentLength() {
#ifdef IBMBIDI
  nsTextFrame* nextBidi = nsnull;
  PRInt32      start = -1, end;

  if (mState & NS_FRAME_IS_BIDI) {
    nextBidi = NS_STATIC_CAST(nsTextFrame*, GetLastInFlow()->GetNextContinuation());
    if (nextBidi) {
      nextBidi->GetOffsets(start, end);
      return start - mContentOffset;
    }
  }
#endif //IBMBIDI
  return mContent->TextLength() - mContentOffset;
}

// Smarter versions of XP_IS_SPACE.
// Unicode is really annoying; sometimes a space character isn't whitespace ---
// when it combines with another character
// So we have several versions of IsSpace for use in different contexts.

static PRBool IsCSSWordSpacingSpace(const nsTextFragment* aFrag, PRUint32 aPos)
{
  NS_ASSERTION(aPos < aFrag->GetLength(), "No text for IsSpace!");
  PRUnichar ch = aFrag->CharAt(aPos);
  if (ch == ' ' || ch == 0x3000) { // IDEOGRAPHIC SPACE
    if (!aFrag->Is2b())
      return PR_TRUE;
    return !nsTextFrameUtils::IsSpaceCombiningSequenceTail(
        aFrag->Get2b() + aPos + 1, aFrag->GetLength() - (aPos + 1));
  } else {
    return ch == '\t' || ch == '\n' || ch == '\f';
  }
}

static PRBool IsSpace(const PRUnichar* aChars, PRUint32 aLength)
{
  NS_ASSERTION(aLength > 0, "No text for IsSpace!");
  PRUnichar ch = *aChars;
  if (ch == ' ') {
    return !nsTextFrameUtils::IsSpaceCombiningSequenceTail(aChars + 1, aLength - 1);
  } else {
    return ch == '\t' || ch == '\n' || ch == '\f';
  }
}

static PRBool IsSpace(char aCh)
{
  return aCh == ' ' || aCh == '\t' || aCh == '\n' || aCh == '\f';
}

static PRBool IsSpace(const nsTextFragment* aFrag, PRUint32 aPos)
{
  NS_ASSERTION(aPos < aFrag->GetLength(), "No text for IsSpace!");
  PRUnichar ch = aFrag->CharAt(aPos);
  if (ch == ' ') {
    if (!aFrag->Is2b())
      return PR_TRUE;
    return !nsTextFrameUtils::IsSpaceCombiningSequenceTail(
        aFrag->Get2b() + aPos + 1, aFrag->GetLength() - (aPos + 1));
  } else {
    return ch == '\t' || ch == '\n' || ch == '\f';
  }
}

static PRUint32 GetWhitespaceCount(const nsTextFragment* frag, PRInt32 aStartOffset,
                                   PRInt32 aLength, PRInt32 aDirection)
{
  PRInt32 count = 0;
  if (frag->Is2b()) {
    const PRUnichar* str = frag->Get2b() + aStartOffset;
    PRInt32 fragLen = frag->GetLength() - aStartOffset;
    for (; count < aLength; ++count) {
      if (!IsSpace(str, fragLen))
        break;
      str += aDirection;
      fragLen -= aDirection;
    }
  } else {
    const char* str = frag->Get1b() + aStartOffset;
    for (; count < aLength; ++count) {
      if (!IsSpace(*str))
        break;
      str += aDirection;
    }
  }
  return count;
}

/**
 * This class accumulates state as we scan a paragraph of text. It detects
 * textrun boundaries (changes from text to non-text, hard
 * line breaks, and font changes) and builds a gfxTextRun at each boundary.
 * It also detects linebreaker run boundaries (changes from text to non-text,
 * and hard line breaks) and at each boundary runs the linebreaker to compute
 * potential line breaks. It also records actual line breaks to store them in
 * the textruns.
 */
class BuildTextRunsScanner {
public:
  BuildTextRunsScanner(nsPresContext* aPresContext, nsBlockFrame* aBlockFrame,
                       gfxContext* aContext) :
    mCurrentFramesAllSameTextRun(nsnull), mBlockFrame(aBlockFrame),
    mContext(aContext), mBidiEnabled(aPresContext->BidiEnabled()),
    mTrimNextRunLeadingWhitespace(PR_FALSE) {
    ResetRunInfo();
  }

  void SetAtStartOfLine() {
    mStartOfLine = PR_TRUE;
  }
  void SetCommonAncestorWithLastFrame(nsIFrame* aFrame) {
    mCommonAncestorWithLastFrame = aFrame;
  }
  nsIFrame* GetCommonAncestorWithLastFrame() {
    return mCommonAncestorWithLastFrame;
  }
  void LiftCommonAncestorWithLastFrameToParent(nsIFrame* aFrame) {
    if (mCommonAncestorWithLastFrame &&
        mCommonAncestorWithLastFrame->GetParent() == aFrame) {
      mCommonAncestorWithLastFrame = aFrame;
    }
  }
  void ScanFrame(nsIFrame* aFrame);
  void FlushFrames(PRBool aFlushLineBreaks);
  void ResetRunInfo() {
    mLastFrame = nsnull;
    mMappedFlows.Clear();
    mLineBreakBeforeFrames.Clear();
    mMaxTextLength = 0;
    mDoubleByteText = PR_FALSE;
  }
  void AccumulateRunInfo(nsTextFrame* aFrame);
  void BuildTextRunForFrames(void* aTextBuffer);
  void AssignTextRun(gfxTextRun* aTextRun);
  nsTextFrame* GetNextBreakBeforeFrame(PRUint32* aIndex);
  void SetupBreakSinksForTextRun(gfxTextRun* aTextRun, const void* aText, PRUint32 aLength,
                                 PRBool aIs2b, PRBool aIsExistingTextRun);

  PRBool StylesMatchForTextRun(nsIFrame* aFrame1, nsIFrame* aFrame2);

  // Like TextRunMappedFlow but with some differences. mStartFrame to mEndFrame
  // are a sequence of in-flow frames. There can be multiple MappedFlows per
  // content element; the frames in each MappedFlow all have the same style
  // context.
  struct MappedFlow {
    nsTextFrame* mStartFrame;
    nsTextFrame* mEndFrame;
    // When we break between non-whitespace characters, the nearest common
    // ancestor of the frames containing the characters is the one whose
    // CSS 'white-space' property governs. So this records the nearest common
    // ancestor of mStartFrame and the previous text frame, or null if there
    // was no previous text frame on this line.
    nsIFrame*    mAncestorControllingInitialBreak;
    PRInt32      mContentOffset;
    PRInt32      mContentEndOffset;
    PRUint32     mTransformedTextOffset; // Only used inside BuildTextRunForFrames
  };

  class BreakSink : public nsILineBreakSink {
  public:
    BreakSink(gfxTextRun* aTextRun, PRUint32 aOffsetIntoTextRun,
              PRBool aExistingTextRun) :
                  mTextRun(aTextRun), mOffsetIntoTextRun(aOffsetIntoTextRun),
                  mChangedBreaks(PR_FALSE), mExistingTextRun(aExistingTextRun) {}

    virtual void SetBreaks(PRUint32 aOffset, PRUint32 aLength,
                           PRPackedBool* aBreakBefore) {
      if (mTextRun->SetPotentialLineBreaks(aOffset + mOffsetIntoTextRun, aLength,
                                           aBreakBefore)) {
        mChangedBreaks = PR_TRUE;
      }
    }

    gfxTextRun*  mTextRun;
    PRUint32     mOffsetIntoTextRun;
    PRPackedBool mChangedBreaks;
    PRPackedBool mExistingTextRun;
  };

private:
  nsAutoTArray<MappedFlow,10>   mMappedFlows;
  nsAutoTArray<nsTextFrame*,50> mLineBreakBeforeFrames;
  nsAutoTArray<nsAutoPtr<BreakSink>,10> mBreakSinks;
  nsLineBreaker                 mLineBreaker;
  gfxTextRun*                   mCurrentFramesAllSameTextRun;
  nsBlockFrame*                 mBlockFrame;
  gfxContext*                   mContext;
  nsTextFrame*                  mLastFrame;
  // The common ancestor of the current frame and the previous text frame
  // on the line, if there's no non-text frame boundaries in between. Otherwise
  // null.
  nsIFrame*                     mCommonAncestorWithLastFrame;
  // mMaxTextLength is an upper bound on the size of the text in all mapped frames
  PRUint32                      mMaxTextLength;
  PRPackedBool                  mDoubleByteText;
  PRPackedBool                  mBidiEnabled;
  PRPackedBool                  mStartOfLine;
  PRPackedBool                  mTrimNextRunLeadingWhitespace;
  PRPackedBool                  mCurrentRunTrimLeadingWhitespace;
};

/**
 * General routine for building text runs. This is hairy because of the need
 * to build text runs that span content nodes. Right now our strategy is fairly
 * simple. We find the lines containing aForFrame bounded by hard line breaks
 * (blocks or <br>s), and build textruns for all the frames in those lines.
 * Some might already have textruns in which case we leave those alone
 * if they're still valid. We could make this a lot more complicated if we
 * need to squeeze out performance.
 * 
 * @param aForFrameLine the line containing aForFrame; if null, we'll figure
 * out the line (slowly)
 * @param aBlockFrame the block containing aForFrame; if null, we'll figure
 * out the block (slowly)
 */
static void
BuildTextRuns(nsIRenderingContext* aRC, nsTextFrame* aForFrame,
              nsBlockFrame* aBlockFrame, const nsLineList::iterator* aForFrameLine)
{
  if (!aBlockFrame) {
    aBlockFrame = nsLayoutUtils::FindNearestBlockAncestor(aForFrame);
  }
  // XXX Need to do something here to detect situations where we're not in a block
  // context (e.g., MathML). Need to detect the true container and scan it
  // as if it was one line.

  nsPresContext* presContext = aForFrame->GetPresContext();

  // Find line where we can start building text runs. It's the first line at
  // or before aForFrameLine that's after a hard break.
  nsBlockFrame::line_iterator line;
  if (aForFrameLine) {
    line = *aForFrameLine;
  } else {
    nsIFrame* immediateChild =
      nsLayoutUtils::FindChildContainingDescendant(aBlockFrame, aForFrame);
    // This may be a float e.g. for a floated first-letter
    if (immediateChild->GetStateBits() & NS_FRAME_OUT_OF_FLOW) {
      immediateChild =
        nsLayoutUtils::FindChildContainingDescendant(aBlockFrame,
          presContext->FrameManager()->GetPlaceholderFrameFor(immediateChild));
    }
    line = aBlockFrame->FindLineFor(immediateChild);
    NS_ASSERTION(line != aBlockFrame->end_lines(),
                 "Frame is not in the block!!!");
  }
  nsBlockFrame::line_iterator firstLine = aBlockFrame->begin_lines();
  while (line != firstLine) {
    --line;
    if (line->IsBlock()) {
      ++line;
      break;
    }
  }

  // Now iterate over all text frames starting from the current line. First-in-flow
  // text frames will be accumulated into textRunFrames as we go. When a
  // text run boundary is required we flush textRunFrames ((re)building their
  // gfxTextRuns as necessary).
  nsBlockFrame::line_iterator endLines = aBlockFrame->end_lines();
  gfxContext* ctx = NS_STATIC_CAST(gfxContext*,
    aRC->GetNativeGraphicData(nsIRenderingContext::NATIVE_THEBES_CONTEXT));
  BuildTextRunsScanner scanner(presContext, aBlockFrame, ctx);
  NS_ASSERTION(line != endLines && !line->IsBlock(), "Where is this frame anyway??");
  nsIFrame* child = line->mFirstChild;
  do {
    scanner.SetAtStartOfLine();
    scanner.SetCommonAncestorWithLastFrame(nsnull);
    PRInt32 i;
    for (i = line->GetChildCount() - 1; i >= 0; --i) {
      scanner.ScanFrame(child);
      scanner.LiftCommonAncestorWithLastFrameToParent(aBlockFrame);
      child = child->GetNextSibling();
    }
    ++line;
  } while (line != endLines && !line->IsBlock());

  // Set mStartOfLine so FlushFrames knows its textrun ends a line
  scanner.SetAtStartOfLine();
  scanner.FlushFrames(PR_TRUE);
}

static PRUnichar*
ExpandBuffer(PRUnichar* aDest, PRUint8* aSrc, PRUint32 aCount)
{
  while (aCount) {
    *aDest = *aSrc;
    ++aDest;
    ++aSrc;
    --aCount;
  }
  return aDest;
}

static void*
TransformTextToBuffer(nsTextFrame* aFrame, PRInt32 aContentLength,
                      void* aBuffer, PRInt32 aCharSize, gfxSkipCharsBuilder* aBuilder,
                      PRPackedBool* aIncomingWhitespace)
{
  const nsTextFragment* frag = aFrame->GetContent()->GetText();
  PRInt32 contentStart = aFrame->GetContentOffset();
  PRBool compressWhitespace = !aFrame->GetStyleText()->WhiteSpaceIsSignificant();
  PRUint32 analysisFlags;

  if (frag->Is2b()) {
    NS_ASSERTION(aCharSize == 2, "Wrong size buffer!");
    return nsTextFrameUtils::TransformText(
        frag->Get2b() + contentStart, aContentLength, NS_STATIC_CAST(PRUnichar*, aBuffer),
        compressWhitespace, aIncomingWhitespace, aBuilder, &analysisFlags);
  } else {
    if (aCharSize == 2) {
      // Need to expand the text. First transform it into a temporary buffer,
      // then expand.
      nsAutoTArray<PRUint8,BIG_TEXT_NODE_SIZE> tempBuf;
      if (!tempBuf.AppendElements(aContentLength))
        return nsnull;
      PRUint8* end = nsTextFrameUtils::TransformText(
          NS_REINTERPRET_CAST(const PRUint8*, frag->Get1b()) + contentStart, aContentLength,
          tempBuf.Elements(), compressWhitespace, aIncomingWhitespace, aBuilder, &analysisFlags);
      return ExpandBuffer(NS_STATIC_CAST(PRUnichar*, aBuffer),
                          tempBuf.Elements(), end - tempBuf.Elements());
    } else {
      return nsTextFrameUtils::TransformText(
          NS_REINTERPRET_CAST(const PRUint8*, frag->Get1b()) + contentStart, aContentLength,
          NS_STATIC_CAST(PRUint8*, aBuffer),
          compressWhitespace, aIncomingWhitespace, aBuilder, &analysisFlags);
    }
  }
}

static void
ReconstructTextForRun(gfxTextRun* aTextRun, PRBool aRememberText,
                      BuildTextRunsScanner* aSetupBreaks,
                      PRPackedBool* aIncomingWhitespace)
{
  gfxSkipCharsBuilder builder;
  nsAutoTArray<PRUint8,BIG_TEXT_NODE_SIZE> buffer;
  PRUint32 charSize = (aTextRun->GetFlags() & gfxTextRunFactory::TEXT_IS_8BIT) ? 1 : 2;
  PRInt32 length;
  void* bufEnd;
  nsTextFrame* f;

  if (aTextRun->GetFlags() & nsTextFrameUtils::TEXT_IS_SIMPLE_FLOW) {
    f = NS_STATIC_CAST(nsTextFrame*, aTextRun->GetUserData());
    const nsTextFragment* frag = f->GetContent()->GetText();
    length = frag->GetLength() - f->GetContentOffset();
    if (!buffer.AppendElements(length*charSize))
      return;
    bufEnd = TransformTextToBuffer(f, length, buffer.Elements(), charSize, &builder,
                                   aIncomingWhitespace);
    if (!bufEnd)
      return;
  } else {
    TextRunUserData* userData = NS_STATIC_CAST(TextRunUserData*, aTextRun->GetUserData());
    length = 0;
    PRInt32 i;
    for (i = 0; i < userData->mMappedFlowCount; ++i) {
      TextRunMappedFlow* flow = &userData->mMappedFlows[i];
      length += flow->mContentLength;
    }
    if (!buffer.AppendElements(length*charSize))
      return;

    bufEnd = buffer.Elements();
    for (i = 0; i < userData->mMappedFlowCount; ++i) {
      TextRunMappedFlow* flow = &userData->mMappedFlows[i];
      bufEnd = TransformTextToBuffer(flow->mStartFrame, flow->mContentLength, bufEnd,
                                     charSize, &builder, aIncomingWhitespace);
      if (!bufEnd)
        return;
    }
    f = userData->mMappedFlows[0].mStartFrame;
  }
  PRUint32 transformedLength = NS_STATIC_CAST(PRUint8*, bufEnd) - buffer.Elements();
  if (charSize == 2) {
    transformedLength >>= 1;
  }

  if (aRememberText) {
    if (charSize == 2) {
      aTextRun->RememberText(NS_REINTERPRET_CAST(PRUnichar*, buffer.Elements()), transformedLength);
    } else {
      aTextRun->RememberText(buffer.Elements(), transformedLength);
    }
  }

  if (aSetupBreaks) {
    aSetupBreaks->SetupBreakSinksForTextRun(aTextRun, buffer.Elements(), transformedLength,
                                            charSize == 2, PR_TRUE);
  }
}

/**
 * This gets called when we need to make a text run for the current list of
 * frames.
 */
void BuildTextRunsScanner::FlushFrames(PRBool aFlushLineBreaks)
{
  if (mMappedFlows.Length() == 0)
    return;

  if (mCurrentFramesAllSameTextRun &&
      ((mCurrentFramesAllSameTextRun->GetFlags() & nsTextFrameUtils::TEXT_INCOMING_WHITESPACE) != 0) ==
      mCurrentRunTrimLeadingWhitespace) {
    // We do not need to (re)build the textrun.
    // Note that if the textrun included all these frames and more, and something
    // changed so that it can only cover these frames, then one of the frames
    // at the boundary would have detected the change and nuked the textrun.

    // Feed this run's text into the linebreaker to provide context. This also
    // updates mTrimNextRunLeadingWhitespace appropriately.
    ReconstructTextForRun(mCurrentFramesAllSameTextRun, PR_FALSE, this,
                          &mTrimNextRunLeadingWhitespace);
  } else {
    nsAutoTArray<PRUint8,BIG_TEXT_NODE_SIZE> buffer;
    if (!buffer.AppendElements(mMaxTextLength*(mDoubleByteText ? 2 : 1)))
      return;
    BuildTextRunForFrames(buffer.Elements());
  }

  if (aFlushLineBreaks) {
    mLineBreaker.Reset();
    PRUint32 i;
    for (i = 0; i < mBreakSinks.Length(); ++i) {
      if (!mBreakSinks[i]->mExistingTextRun || mBreakSinks[i]->mChangedBreaks) {
        // TODO cause frames associated with the textrun to be reflowed, if they
        // aren't being reflowed already!
      }
    }
    mBreakSinks.Clear();
  }

  ResetRunInfo();
}

void BuildTextRunsScanner::AccumulateRunInfo(nsTextFrame* aFrame)
{
  mMaxTextLength += aFrame->GetContentLength();
  mDoubleByteText |= aFrame->GetContent()->GetText()->Is2b();
  mLastFrame = aFrame;
  mCommonAncestorWithLastFrame = aFrame;

  if (mStartOfLine) {
    mLineBreakBeforeFrames.AppendElement(aFrame);
    mStartOfLine = PR_FALSE;
  }
}

PRBool
BuildTextRunsScanner::StylesMatchForTextRun(nsIFrame* aFrame1, nsIFrame* aFrame2)
{
  if (mBidiEnabled &&
      NS_GET_EMBEDDING_LEVEL(aFrame1) != NS_GET_EMBEDDING_LEVEL(aFrame2))
    return PR_FALSE;

  nsStyleContext* sc1 = aFrame1->GetStyleContext();
  nsStyleContext* sc2 = aFrame2->GetStyleContext();

  if (sc1 == sc2)
    return PR_TRUE;
  return sc1->GetStyleFont()->mFont.BaseEquals(sc2->GetStyleFont()->mFont) &&
    sc1->GetStyleVisibility()->mLangGroup == sc2->GetStyleVisibility()->mLangGroup;
}

void BuildTextRunsScanner::ScanFrame(nsIFrame* aFrame)
{
  // First check if we can extend the current mapped frame block. This is common.
  if (mMappedFlows.Length() > 0) {
    MappedFlow* mappedFlow = &mMappedFlows[mMappedFlows.Length() - 1];
    if (mappedFlow->mEndFrame == aFrame) {
      NS_ASSERTION(aFrame->GetType() == nsGkAtoms::textFrame,
                   "Flow-sibling of a text frame is not a text frame?");

      // this is almost always true
      if (mLastFrame->GetStyleContext() == aFrame->GetStyleContext()) {
        nsTextFrame* frame = NS_STATIC_CAST(nsTextFrame*, aFrame);
        mappedFlow->mEndFrame = NS_STATIC_CAST(nsTextFrame*, frame->GetNextInFlow());
        // Frames in the same flow can overlap at least temporarily
        // (e.g. when first-line builds its textrun, we need to have it suck
        // up all the in-flow content because we don't know how long the line
        // is going to be).
        mappedFlow->mContentEndOffset =
          PR_MAX(mappedFlow->mContentEndOffset,
                 frame->GetContentOffset() + frame->GetContentLength());
        AccumulateRunInfo(frame);
        return;
      }
    }
  }

  // Now see if we can add a new set of frames to the current textrun
  if (aFrame->GetType() == nsGkAtoms::textFrame) {
    nsTextFrame* frame = NS_STATIC_CAST(nsTextFrame*, aFrame);

    if (mLastFrame && !StylesMatchForTextRun(mLastFrame, aFrame)) {
      FlushFrames(PR_FALSE);
    }

    MappedFlow* mappedFlow = mMappedFlows.AppendElement();
    if (!mappedFlow)
      return;

    mappedFlow->mStartFrame = frame;
    mappedFlow->mEndFrame = NS_STATIC_CAST(nsTextFrame*, frame->GetNextInFlow());
    mappedFlow->mAncestorControllingInitialBreak = mCommonAncestorWithLastFrame;
    mappedFlow->mContentOffset = frame->GetContentOffset();
    mappedFlow->mContentEndOffset =
      frame->GetContentOffset() + frame->GetContentLength();
    mappedFlow->mTransformedTextOffset = 0;
    mLastFrame = frame;

    AccumulateRunInfo(frame);
    if (mMappedFlows.Length() == 1) {
      mCurrentFramesAllSameTextRun = frame->GetTextRun();
      mCurrentRunTrimLeadingWhitespace = mTrimNextRunLeadingWhitespace;
    } else {
      if (mCurrentFramesAllSameTextRun != frame->GetTextRun()) {
        mCurrentFramesAllSameTextRun = nsnull;
      }
    }
    return;
  }

  PRBool continueTextRun = aFrame->CanContinueTextRun();
  PRBool descendInto = PR_TRUE;
  if (!continueTextRun) {
    FlushFrames(PR_TRUE);
    mCommonAncestorWithLastFrame = nsnull;
    // XXX do we need this? are there frames we need to descend into that aren't
    // float-containing-blocks?
    descendInto = !aFrame->IsFloatContainingBlock();
    mStartOfLine = PR_FALSE;
    mTrimNextRunLeadingWhitespace = PR_FALSE;
  }

  if (descendInto) {
    nsIFrame* f;
    for (f = aFrame->GetFirstChild(nsnull); f; f = f->GetNextSibling()) {
      ScanFrame(f);
      LiftCommonAncestorWithLastFrameToParent(aFrame);
    }
  }

  if (!continueTextRun) {
    FlushFrames(PR_TRUE);
    mCommonAncestorWithLastFrame = nsnull;
    mTrimNextRunLeadingWhitespace = PR_FALSE;
  }
}

static nscoord StyleToCoord(const nsStyleCoord& aCoord)
{
  if (eStyleUnit_Coord == aCoord.GetUnit()) {
    return aCoord.GetCoordValue();
  } else {
    return 0;
  }
}

static void
DestroyUserData(void* aUserData)
{
  TextRunUserData* userData = NS_STATIC_CAST(TextRunUserData*, aUserData);
  if (userData) {
    nsMemory::Free(userData);
  }
}
 
nsTextFrame*
BuildTextRunsScanner::GetNextBreakBeforeFrame(PRUint32* aIndex)
{
  PRUint32 index = *aIndex;
  if (index >= mLineBreakBeforeFrames.Length())
    return nsnull;
  *aIndex = index + 1;
  return NS_STATIC_CAST(nsTextFrame*, mLineBreakBeforeFrames.ElementAt(index));
}

static PRUint32
GetSpacingFlags(const nsStyleCoord& aStyleCoord)
{
  nscoord spacing = StyleToCoord(aStyleCoord);
  if (!spacing)
    return 0;
  if (spacing > 0)
    return gfxTextRunFactory::TEXT_ENABLE_SPACING;
  return gfxTextRunFactory::TEXT_ENABLE_SPACING |
         gfxTextRunFactory::TEXT_ENABLE_NEGATIVE_SPACING;
}

static gfxFontGroup*
GetFontGroupForFrame(nsIFrame* aFrame)
{
  nsIDeviceContext* devContext = aFrame->GetPresContext()->DeviceContext();
  const nsStyleFont* fontStyle = aFrame->GetStyleFont();
  const nsStyleVisibility* visibilityStyle = aFrame->GetStyleVisibility();
  nsCOMPtr<nsIFontMetrics> metrics;
  devContext->GetMetricsFor(fontStyle->mFont, visibilityStyle->mLangGroup,
                            *getter_AddRefs(metrics));
  if (!metrics) 
    return nsnull;

  nsIFontMetrics* metricsRaw = metrics;
  nsIThebesFontMetrics* fm = NS_STATIC_CAST(nsIThebesFontMetrics*, metricsRaw);
  return fm->GetThebesFontGroup();
}

static gfxTextRun*
GetSpecialString(gfxFontGroup* aFontGroup, gfxFontGroup::SpecialString aSpecial,
                 gfxTextRun* aTextRun)
{
  if (!aFontGroup)
    return nsnull;
  return aFontGroup->GetSpecialStringTextRun(aSpecial, aTextRun);
}

static gfxFont::Metrics
GetFontMetrics(gfxFontGroup* aFontGroup)
{
  if (!aFontGroup)
    return gfxFont::Metrics();
  gfxFont* font = aFontGroup->GetFontAt(0);
  if (!font)
    return gfxFont::Metrics();
  return font->GetMetrics();
}

void
BuildTextRunsScanner::BuildTextRunForFrames(void* aTextBuffer)
{
  gfxSkipCharsBuilder builder;

  const void* textPtr = aTextBuffer;
  PRBool anySmallcapsStyle = PR_FALSE;
  PRBool anyTextTransformStyle = PR_FALSE;
  nsIContent* lastContent = nsnull;
  PRInt32 endOfLastContent = 0;
  PRBool anyMixedStyleFlows = PR_FALSE;
  PRUint32 textFlags = gfxTextRunFactory::TEXT_NEED_BOUNDING_BOX;

  if (mCurrentRunTrimLeadingWhitespace) {
    textFlags |= nsTextFrameUtils::TEXT_INCOMING_WHITESPACE;
  }

  nsAutoTArray<PRUint32,50> textBreakPoints;
  // We might have a final break offset for the end of the textrun
  if (!textBreakPoints.AppendElements(mLineBreakBeforeFrames.Length() + 1))
    return;

  TextRunUserData dummyData;
  TextRunMappedFlow dummyMappedFlow;

  TextRunUserData* userData;
  // If the situation is particularly simple (and common) we don't need to
  // allocate userData.
  if (mMappedFlows.Length() == 1 && !mMappedFlows[0].mEndFrame &&
      !mMappedFlows[0].mContentOffset) {
    userData = &dummyData;
    dummyData.mMappedFlows = &dummyMappedFlow;
  } else {
    userData = NS_STATIC_CAST(TextRunUserData*,
      nsMemory::Alloc(sizeof(TextRunUserData) + mMappedFlows.Length()*sizeof(TextRunMappedFlow)));
    userData->mMappedFlows = NS_REINTERPRET_CAST(TextRunMappedFlow*, userData + 1);
  }
  userData->mLastFlowIndex = 0;

  PRUint32 finalMappedFlowCount = 0;
  PRUint32 currentTransformedTextOffset = 0;

  PRUint32 nextBreakIndex = 0;
  nsTextFrame* nextBreakBeforeFrame = GetNextBreakBeforeFrame(&nextBreakIndex);

  PRUint32 i;
  for (i = 0; i < mMappedFlows.Length(); ++i) {
    MappedFlow* mappedFlow = &mMappedFlows[i];
    nsTextFrame* f = mappedFlow->mStartFrame;

    mappedFlow->mTransformedTextOffset = currentTransformedTextOffset;

    // Detect use of text-transform or font-variant anywhere in the run
    const nsStyleText* textStyle = f->GetStyleText();
    if (NS_STYLE_TEXT_TRANSFORM_NONE != textStyle->mTextTransform) {
      anyTextTransformStyle = PR_TRUE;
    }
    textFlags |= GetSpacingFlags(textStyle->mLetterSpacing);
    textFlags |= GetSpacingFlags(textStyle->mWordSpacing);
    PRBool compressWhitespace = !textStyle->WhiteSpaceIsSignificant();
    if (NS_STYLE_TEXT_ALIGN_JUSTIFY == textStyle->mTextAlign && compressWhitespace) {
      textFlags |= gfxTextRunFactory::TEXT_ENABLE_SPACING;
    }
    const nsStyleFont* fontStyle = f->GetStyleFont();
    if (NS_STYLE_FONT_VARIANT_SMALL_CAPS == fontStyle->mFont.variant) {
      anySmallcapsStyle = PR_TRUE;
    }

    // Figure out what content is included in this flow.
    nsIContent* content = f->GetContent();
    const nsTextFragment* frag = content->GetText();
    PRInt32 contentStart = mappedFlow->mContentOffset;
    PRInt32 contentEnd = mappedFlow->mContentEndOffset;
    PRInt32 contentLength = contentEnd - contentStart;

    if (content == lastContent) {
      NS_ASSERTION(endOfLastContent >= contentStart,
                   "Gap in textframes mapping content?!"); 
      // Text frames can overlap (see comment in ScanFrame below)
      contentStart = PR_MAX(contentStart, endOfLastContent);
      if (contentStart >= contentEnd)
        continue;
      anyMixedStyleFlows = PR_TRUE;
      userData->mMappedFlows[finalMappedFlowCount - 1].mContentLength += contentLength;
    } else {
      TextRunMappedFlow* newFlow = &userData->mMappedFlows[finalMappedFlowCount];

      newFlow->mStartFrame = mappedFlow->mStartFrame;
      newFlow->mDOMOffsetToBeforeTransformOffset = builder.GetCharCount() - mappedFlow->mContentOffset;
      newFlow->mContentLength = contentLength;
      ++finalMappedFlowCount;

      while (nextBreakBeforeFrame && nextBreakBeforeFrame->GetContent() == content) {
        textBreakPoints[nextBreakIndex - 1] =
          nextBreakBeforeFrame->GetContentOffset() + newFlow->mDOMOffsetToBeforeTransformOffset;
        nextBreakBeforeFrame = GetNextBreakBeforeFrame(&nextBreakIndex);
      }
    }

    PRUint32 analysisFlags;
    if (frag->Is2b()) {
      NS_ASSERTION(mDoubleByteText, "Wrong buffer char size!");
      PRUnichar* bufStart = NS_STATIC_CAST(PRUnichar*, aTextBuffer);
      PRUnichar* bufEnd = nsTextFrameUtils::TransformText(
          frag->Get2b() + contentStart, contentLength, bufStart,
          compressWhitespace, &mTrimNextRunLeadingWhitespace, &builder, &analysisFlags);
      aTextBuffer = bufEnd;
    } else {
      if (mDoubleByteText) {
        // Need to expand the text. First transform it into a temporary buffer,
        // then expand.
        nsAutoTArray<PRUint8,BIG_TEXT_NODE_SIZE> tempBuf;
        if (!tempBuf.AppendElements(contentLength)) {
          DestroyUserData(userData);
          return;
        }
        PRUint8* bufStart = tempBuf.Elements();
        PRUint8* end = nsTextFrameUtils::TransformText(
            NS_REINTERPRET_CAST(const PRUint8*, frag->Get1b()) + contentStart, contentLength,
            bufStart, compressWhitespace, &mTrimNextRunLeadingWhitespace,
            &builder, &analysisFlags);
        aTextBuffer = ExpandBuffer(NS_STATIC_CAST(PRUnichar*, aTextBuffer),
                                   tempBuf.Elements(), end - tempBuf.Elements());
      } else {
        PRUint8* bufStart = NS_STATIC_CAST(PRUint8*, aTextBuffer);
        PRUint8* end = nsTextFrameUtils::TransformText(
            NS_REINTERPRET_CAST(const PRUint8*, frag->Get1b()) + contentStart, contentLength,
            bufStart,
            compressWhitespace, &mTrimNextRunLeadingWhitespace, &builder, &analysisFlags);
        aTextBuffer = end;
      }
    }
    textFlags |= analysisFlags;

    currentTransformedTextOffset =
      (NS_STATIC_CAST(const PRUint8*, aTextBuffer) - NS_STATIC_CAST(const PRUint8*, textPtr)) >> mDoubleByteText;

    lastContent = content;
    endOfLastContent = contentEnd;
  }

  // Check for out-of-memory in gfxSkipCharsBuilder
  if (!builder.IsOK()) {
    DestroyUserData(userData);
    return;
  }

  void* finalUserData;
  if (userData == &dummyData) {
    textFlags |= nsTextFrameUtils::TEXT_IS_SIMPLE_FLOW;
    userData = nsnull;
    finalUserData = mMappedFlows[0].mStartFrame;
  } else {
    userData = NS_STATIC_CAST(TextRunUserData*,
      nsMemory::Realloc(userData, sizeof(TextRunUserData) + finalMappedFlowCount*sizeof(TextRunMappedFlow)));
    if (!userData)
      return;
    userData->mMappedFlows = NS_REINTERPRET_CAST(TextRunMappedFlow*, userData + 1);
    userData->mMappedFlowCount = finalMappedFlowCount;
    finalUserData = userData;
  }

  PRUint32 transformedLength = currentTransformedTextOffset;

  if (!(textFlags & nsTextFrameUtils::TEXT_WAS_TRANSFORMED) &&
      mMappedFlows.Length() == 1) {
    // The textrun maps one continuous, unmodified run of DOM text. It can
    // point to the DOM text directly.
    const nsTextFragment* frag = lastContent->GetText();
    if (frag->Is2b()) {
      textPtr = frag->Get2b() + mMappedFlows[0].mContentOffset;
    } else {
      textPtr = frag->Get1b() + mMappedFlows[0].mContentOffset;
    }
    textFlags |= gfxTextRunFactory::TEXT_IS_PERSISTENT;
  }

  // Now build the textrun
  nsTextFrame* firstFrame = mMappedFlows[0].mStartFrame;
  gfxFontGroup* fontGroup = GetFontGroupForFrame(firstFrame);
  if (!fontGroup) {
    DestroyUserData(userData);
    return;
  }

  // Setup factory chain
  nsAutoPtr<nsTransformingTextRunFactory> transformingFactory;
  if (anySmallcapsStyle) {
    transformingFactory = new nsFontVariantTextRunFactory(fontGroup);
  }
  if (anyTextTransformStyle) {
    transformingFactory =
      new nsCaseTransformTextRunFactory(fontGroup, transformingFactory.forget());
  }
  nsTArray<nsStyleContext*> styles;
  if (transformingFactory) {
    for (i = 0; i < mMappedFlows.Length(); ++i) {
      MappedFlow* mappedFlow = &mMappedFlows[i];
      PRUint32 end = i == mMappedFlows.Length() - 1 ? transformedLength :
          mMappedFlows[i + 1].mTransformedTextOffset;
      nsStyleContext* sc = mappedFlow->mStartFrame->GetStyleContext();
      PRUint32 j;
      for (j = mappedFlow->mTransformedTextOffset; j < end; ++j) {
        styles.AppendElement(sc);
      }
    }
  }

  if (textFlags & nsTextFrameUtils::TEXT_HAS_TAB) {
    textFlags |= gfxTextRunFactory::TEXT_ENABLE_SPACING;
  }
  if (textFlags & nsTextFrameUtils::TEXT_HAS_SHY) {
    textFlags |= gfxTextRunFactory::TEXT_ENABLE_HYPHEN_BREAKS;
  }
  if (!(textFlags & nsTextFrameUtils::TEXT_HAS_NON_ASCII)) {
    textFlags |= gfxTextRunFactory::TEXT_IS_ASCII;
  }
  if (mBidiEnabled && (NS_GET_EMBEDDING_LEVEL(firstFrame) & 1)) {
    textFlags |= gfxTextRunFactory::TEXT_IS_RTL;
  }

  gfxSkipChars skipChars;
  skipChars.TakeFrom(&builder);
  // Convert linebreak coordinates to transformed string offsets
  NS_ASSERTION(nextBreakIndex == mLineBreakBeforeFrames.Length(),
               "Didn't find all the frames to break-before...");
  gfxSkipCharsIterator iter(skipChars);
  for (i = 0; i < nextBreakIndex; ++i) {
    PRUint32* breakPoint = &textBreakPoints[i];
    *breakPoint = iter.ConvertOriginalToSkipped(*breakPoint);
  }
  if (mStartOfLine) {
    textBreakPoints[nextBreakIndex] = transformedLength;
    ++nextBreakIndex;
  }

  gfxTextRunFactory::Parameters params =
      { mContext, finalUserData, firstFrame->GetStyleVisibility()->mLangGroup, &skipChars,
        textBreakPoints.Elements(), nextBreakIndex,
        firstFrame->GetPresContext()->AppUnitsPerDevPixel(), textFlags };

  gfxTextRun* textRun;
  if (mDoubleByteText) {
    if (textFlags & gfxTextRunFactory::TEXT_IS_ASCII) {
      NS_WARNING("Hmm ... why are we taking the Unicode path when the text is all ASCII?");
    }
    const PRUnichar* text = NS_STATIC_CAST(const PRUnichar*, textPtr);
    if (transformingFactory) {
      textRun = transformingFactory->MakeTextRun(text, transformedLength, &params,
                                                 styles.Elements());
      if (textRun) {
        transformingFactory.forget();
      }
    } else {
      textRun = fontGroup->MakeTextRun(text, transformedLength, &params);
    }
    if (!textRun) {
      DestroyUserData(userData);
      return;
    }
    if (anyMixedStyleFlows) {
      // Make sure we're not asked to recover the text, it's too hard to do later.
      textRun->RememberText(text, transformedLength);
    }
  } else {
    const PRUint8* text = NS_STATIC_CAST(const PRUint8*, textPtr);
    if (transformingFactory) {
      textRun = transformingFactory->MakeTextRun(text, transformedLength, &params,
                                                 styles.Elements());
      if (textRun) {
        transformingFactory.forget();
      }
    } else {
      textRun = fontGroup->MakeTextRun(text, transformedLength, &params);
    }
    if (!textRun) {
      DestroyUserData(userData);
      return;
    }
    if (anyMixedStyleFlows) {
      // Make sure we're not asked to recover the text, it's too hard to do later.
      textRun->RememberText(text, transformedLength);
    }
  }
  // We have to set these up after we've created the textrun, because
  // the breaks may be stored in the textrun during this very call.
  // This is a bit annoying because it requires another loop over the frames
  // making up the textrun, but I don't see a way to avoid this.
  SetupBreakSinksForTextRun(textRun, textPtr, transformedLength, mDoubleByteText,
                            PR_FALSE);

  // Actually wipe out the textruns associated with the mapped frames and associate
  // those frames with this text run.
  AssignTextRun(textRun);
}

void
BuildTextRunsScanner::SetupBreakSinksForTextRun(gfxTextRun* aTextRun, const void* aText, PRUint32 aLength,
                                                PRBool aIs2b, PRBool aIsExistingTextRun)
{
  // textruns have uniform language
  nsIAtom* lang = mMappedFlows[0].mStartFrame->GetStyleVisibility()->mLangGroup;
  PRUint32 i;
  for (i = 0; i < mMappedFlows.Length(); ++i) {
    MappedFlow* mappedFlow = &mMappedFlows[i];
    nsAutoPtr<BreakSink>* breakSink = mBreakSinks.AppendElement(
      new BreakSink(aTextRun, mappedFlow->mTransformedTextOffset, aIsExistingTextRun));
    if (!breakSink || !*breakSink)
      return;
    PRUint32 offset = mappedFlow->mTransformedTextOffset;

    PRUint32 length =
      (i == mMappedFlows.Length() - 1 ? aLength : mMappedFlows[i + 1].mTransformedTextOffset)
      - offset;

    PRUint32 flags = 0;
    if (!mappedFlow->mAncestorControllingInitialBreak ||
        mappedFlow->mAncestorControllingInitialBreak->GetStyleText()->WhiteSpaceCanWrap()) {
      flags |= nsLineBreaker::BREAK_NONWHITESPACE_BEFORE;
    }
    if (mappedFlow->mStartFrame->GetStyleText()->WhiteSpaceCanWrap()) {
      flags |= nsLineBreaker::BREAK_WHITESPACE | nsLineBreaker::BREAK_NONWHITESPACE_INSIDE;
    }
    // If length is zero and BREAK_WHITESPACE is active, this will notify
    // the linebreaker to insert a break opportunity before the next character.
    // Thus runs of entirely-skipped whitespace can still induce breaks.
    if (aIs2b) {
      mLineBreaker.AppendText(lang, NS_STATIC_CAST(const PRUnichar*, aText) + offset,
                              length, flags, *breakSink);
    } else {
      mLineBreaker.AppendText(lang, NS_STATIC_CAST(const PRUint8*, aText) + offset,
                              length, flags, *breakSink);
    }
  }
}

void
BuildTextRunsScanner::AssignTextRun(gfxTextRun* aTextRun)
{
  nsIContent* lastContent = nsnull;
  PRUint32 i;
  for (i = 0; i < mMappedFlows.Length(); ++i) {
    MappedFlow* mappedFlow = &mMappedFlows[i];
    nsTextFrame* startFrame = mappedFlow->mStartFrame;
    nsTextFrame* endFrame = mappedFlow->mEndFrame;
    nsTextFrame* f;
    for (f = startFrame; f != endFrame;
         f = NS_STATIC_CAST(nsTextFrame*, f->GetNextContinuation())) {
#ifdef DEBUG
      if (f->GetTextRun()) {
        gfxTextRun* textRun = f->GetTextRun();
        if (textRun->GetFlags() & nsTextFrameUtils::TEXT_IS_SIMPLE_FLOW) {
          if (mMappedFlows[0].mStartFrame != NS_STATIC_CAST(nsTextFrame*, textRun->GetUserData())) {
            NS_WARNING("REASSIGNING SIMPLE FLOW TEXT RUN!");
          }
        } else {
          TextRunUserData* userData =
            NS_STATIC_CAST(TextRunUserData*, textRun->GetUserData());
         
          if (PRUint32(userData->mMappedFlowCount) >= mMappedFlows.Length() ||
              userData->mMappedFlows[userData->mMappedFlowCount - 1].mStartFrame !=
              mMappedFlows[userData->mMappedFlowCount - 1].mStartFrame) {
            NS_WARNING("REASSIGNING MULTIFLOW TEXT RUN (not append)!");
          }
        }
      }
#endif
      f->ClearTextRun();
      f->SetTextRun(aTextRun);
    }
    nsIContent* content = startFrame->GetContent();
    if (content != lastContent) {
      startFrame->AddStateBits(TEXT_IS_RUN_OWNER);
      lastContent = content;
    }    
  }
}

gfxSkipCharsIterator
nsTextFrame::EnsureTextRun(nsIRenderingContext* aRC, nsBlockFrame* aBlock,
                           const nsLineList::iterator* aLine,
                           PRUint32* aFlowEndInTextRun)
{
  if (!mTextRun) {
    if (!aRC) {
      nsCOMPtr<nsIRenderingContext> rendContext;      
      nsresult rv = GetPresContext()->PresShell()->
        CreateRenderingContext(this, getter_AddRefs(rendContext));
      if (NS_SUCCEEDED(rv)) {
        BuildTextRuns(rendContext, this, aBlock, aLine);
      }
    } else {
      BuildTextRuns(aRC, this, aBlock, aLine);
    }      
    if (!mTextRun) {
      // A text run was not constructed for this frame. This is bad. The caller
      // will check mTextRun.
      static const gfxSkipChars emptySkipChars;
      return gfxSkipCharsIterator(emptySkipChars, 0);
    }
  }

  if (mTextRun->GetFlags() & nsTextFrameUtils::TEXT_IS_SIMPLE_FLOW) {
    if (aFlowEndInTextRun) {
      *aFlowEndInTextRun = mTextRun->GetLength();
    }
    return gfxSkipCharsIterator(mTextRun->GetSkipChars(), 0, mContentOffset);
  }

  TextRunUserData* userData = NS_STATIC_CAST(TextRunUserData*, mTextRun->GetUserData());
  // Find the flow that contains us
  PRInt32 direction;
  PRInt32 startAt = userData->mLastFlowIndex;
  // Search first forward and then backward from the current position
  for (direction = 1; direction >= -1; direction -= 2) {
    PRInt32 i;
    for (i = startAt; 0 <= i && i < userData->mMappedFlowCount; i += direction) {
      TextRunMappedFlow* flow = &userData->mMappedFlows[i];
      if (flow->mStartFrame->GetContent() == mContent) {
        // This may not actually be the flow that we're in. But BuildTextRuns
        // promises that this will work ... flows for the same content in the same
        // textrun have to be consecutive, they can't skip characters in the middle.
        // See assertion "Gap in textframes mapping content?!" above.
        userData->mLastFlowIndex = i;
        gfxSkipCharsIterator iter(mTextRun->GetSkipChars(),
                                  flow->mDOMOffsetToBeforeTransformOffset, mContentOffset);
        if (aFlowEndInTextRun) {
          if (i + 1 < userData->mMappedFlowCount) {
            gfxSkipCharsIterator end(mTextRun->GetSkipChars());
            *aFlowEndInTextRun = end.ConvertOriginalToSkipped(
                flow[1].mStartFrame->GetContentOffset() + flow[1].mDOMOffsetToBeforeTransformOffset);
          } else {
            *aFlowEndInTextRun = mTextRun->GetLength();
          }
        }
        return iter;
      }
      ++flow;
    }
    startAt = userData->mLastFlowIndex - 1;
  }
  NS_ERROR("Can't find flow containing this frame???");
  static const gfxSkipChars emptySkipChars;
  return gfxSkipCharsIterator(emptySkipChars, 0);
}

static PRUint32
GetLengthOfTrimmedText(const nsTextFragment* aFrag,
                       PRUint32 aStart, PRUint32 aEnd,
                       gfxSkipCharsIterator* aIterator)
{
  aIterator->SetSkippedOffset(aEnd);
  while (aIterator->GetSkippedOffset() > aStart) {
    aIterator->AdvanceSkipped(-1);
    if (!IsSpace(aFrag, aIterator->GetOriginalOffset()))
      return aIterator->GetSkippedOffset() + 1 - aStart;
  }
  return 0;
}

nsTextFrame::TrimmedOffsets
nsTextFrame::GetTrimmedOffsets(const nsTextFragment* aFrag,
                               PRBool aTrimAfter)
{
  NS_ASSERTION(mTextRun, "Need textrun here");

  TrimmedOffsets offsets = { mContentOffset, mContentLength };
  if (GetStyleText()->WhiteSpaceIsSignificant())
    return offsets;

  if (GetStateBits() & TEXT_START_OF_LINE) {
    PRInt32 whitespaceCount =
      GetWhitespaceCount(aFrag, offsets.mStart, offsets.mLength, 1);
    offsets.mStart += whitespaceCount;
    offsets.mLength -= whitespaceCount;
  }

  if (aTrimAfter && (GetStateBits() & TEXT_END_OF_LINE)) {
    PRInt32 whitespaceCount =
      GetWhitespaceCount(aFrag, offsets.mStart + offsets.mLength - 1, offsets.mLength, -1);
    offsets.mLength -= whitespaceCount;
  }
  return offsets;
}

/*
 * Currently only Unicode characters below 0x10000 have their spacing modified
 * by justification. If characters above 0x10000 turn out to need
 * justification spacing, that will require extra work. Currently,
 * this function must not include 0xd800 to 0xdbff because these characters
 * are surrogates.
 */
static PRBool IsJustifiableCharacter(const nsTextFragment* aFrag, PRInt32 aPos,
                                     PRBool aLangIsCJ)
{
  PRUnichar ch = aFrag->CharAt(aPos);
  if (0x20u == ch || 0xa0u == ch) {
    // Don't justify spaces that are combined with diacriticals
    if (!aFrag->Is2b())
      return PR_TRUE;
    return !nsTextFrameUtils::IsSpaceCombiningSequenceTail(
        aFrag->Get2b() + aPos + 1, aFrag->GetLength() - (aPos + 1));
  }
  if (ch < 0x2150u)
    return PR_FALSE;
  if (aLangIsCJ && (
       (0x2150u <= ch && ch <= 0x22ffu) || // Number Forms, Arrows, Mathematical Operators
       (0x2460u <= ch && ch <= 0x24ffu) || // Enclosed Alphanumerics
       (0x2580u <= ch && ch <= 0x27bfu) || // Block Elements, Geometric Shapes, Miscellaneous Symbols, Dingbats
       (0x27f0u <= ch && ch <= 0x2bffu) || // Supplemental Arrows-A, Braille Patterns, Supplemental Arrows-B,
                                           // Miscellaneous Mathematical Symbols-B, Supplemental Mathematical Operators,
                                           // Miscellaneous Symbols and Arrows
       (0x2e80u <= ch && ch <= 0x312fu) || // CJK Radicals Supplement, CJK Radicals Supplement,
                                           // Ideographic Description Characters, CJK Symbols and Punctuation,
                                           // Hiragana, Katakana, Bopomofo
       (0x3190u <= ch && ch <= 0xabffu) || // Kanbun, Bopomofo Extended, Katakana Phonetic Extensions,
                                           // Enclosed CJK Letters and Months, CJK Compatibility,
                                           // CJK Unified Ideographs Extension A, Yijing Hexagram Symbols,
                                           // CJK Unified Ideographs, Yi Syllables, Yi Radicals
       (0xf900u <= ch && ch <= 0xfaffu) || // CJK Compatibility Ideographs
       (0xff5eu <= ch && ch <= 0xff9fu)    // Halfwidth and Fullwidth Forms(a part)
     ))
    return PR_TRUE;
  return PR_FALSE;
}

static void ClearMetrics(nsHTMLReflowMetrics& aMetrics)
{
  aMetrics.width = 0;
  aMetrics.height = 0;
  aMetrics.ascent = 0;
#ifdef MOZ_MATHML
  aMetrics.mBoundingMetrics.Clear();
#endif
}

static PRInt32 FindChar(const nsTextFragment* frag,
                        PRInt32 aOffset, PRInt32 aLength, PRUnichar ch)
{
  PRInt32 i = 0;
  if (frag->Is2b()) {
    const PRUnichar* str = frag->Get2b() + aOffset;
    for (; i < aLength; ++i) {
      if (*str == ch)
        return i + aOffset;
      ++str;
    }
  } else {
    if (PRUint16(ch) <= 0xFF) {
      const char* str = frag->Get1b() + aOffset;
      void* p = memchr(str, ch, aLength);
      if (p)
        return (NS_STATIC_CAST(char*, p) - str) + aOffset;
    }
  }
  return -1;
}

static PRBool IsChineseJapaneseLangGroup(nsIFrame* aFrame)
{
  nsIAtom* langGroup = aFrame->GetStyleVisibility()->mLangGroup;
  return langGroup == nsGkAtoms::Japanese
      || langGroup == nsGkAtoms::Chinese
      || langGroup == nsGkAtoms::Taiwanese
      || langGroup == nsGkAtoms::HongKongChinese;
}

#ifdef DEBUG
static PRBool IsInBounds(const gfxSkipCharsIterator& aStart, PRInt32 aContentLength,
                         PRUint32 aOffset, PRUint32 aLength) {
  if (aStart.GetSkippedOffset() > aOffset)
    return PR_FALSE;
  gfxSkipCharsIterator iter(aStart);
  iter.AdvanceSkipped(aLength);
  return iter.GetOriginalOffset() <= aStart.GetOriginalOffset() + aContentLength;
}
#endif

class PropertyProvider : public gfxTextRun::PropertyProvider {
public:
  /**
   * Use this constructor for reflow, when we don't know what text is
   * really mapped by the frame and we have a lot of other data around.
   */
  PropertyProvider(gfxTextRun* aTextRun, const nsStyleText* aTextStyle,
                   const nsTextFragment* aFrag, nsTextFrame* aFrame,
                   const gfxSkipCharsIterator& aStart, PRInt32 aLength)
    : mTextRun(aTextRun), mFontGroup(nsnull), mTextStyle(aTextStyle), mFrag(aFrag),
      mFrame(aFrame), mStart(aStart), mLength(aLength),
      mWordSpacing(StyleToCoord(mTextStyle->mWordSpacing)),
      mLetterSpacing(StyleToCoord(mTextStyle->mLetterSpacing)),
      mJustificationSpacing(0),
      mHyphenWidth(-1)
  {
    NS_ASSERTION(mStart.IsInitialized(), "Start not initialized?");
  }

  /**
   * Use this constructor after the frame has been reflowed and we don't
   * have other data around. Gets everything from the frame. EnsureTextRun
   * *must* be called before this!!!
   */
  PropertyProvider(nsTextFrame* aFrame, const gfxSkipCharsIterator& aStart)
    : mTextRun(aFrame->GetTextRun()), mFontGroup(nsnull), mTextStyle(aFrame->GetStyleText()),
      mFrag(aFrame->GetContent()->GetText()),
      mFrame(aFrame), mStart(aStart), mLength(aFrame->GetContentLength()),
      mWordSpacing(StyleToCoord(mTextStyle->mWordSpacing)),
      mLetterSpacing(StyleToCoord(mTextStyle->mLetterSpacing)),
      mJustificationSpacing(0),
      mHyphenWidth(-1)
  {
    NS_ASSERTION(mTextRun, "Textrun not initialized!");
  }

  // Call this after construction if you're not going to reflow the text
  void InitializeForDisplay(PRBool aTrimAfter);

  virtual void ForceRememberText() {
    PRPackedBool incomingWhitespace =
      (mTextRun->GetFlags() & nsTextFrameUtils::TEXT_INCOMING_WHITESPACE) != 0;
    ReconstructTextForRun(mTextRun, PR_TRUE, nsnull, &incomingWhitespace);
  }

  virtual void GetSpacing(PRUint32 aStart, PRUint32 aLength, Spacing* aSpacing);
  virtual gfxFloat GetHyphenWidth();
  virtual void GetHyphenationBreaks(PRUint32 aStart, PRUint32 aLength,
                                    PRPackedBool* aBreakBefore);

  /**
   * Count the number of justifiable characters in the given DOM range
   */
  PRUint32 ComputeJustifiableCharacters(PRInt32 aOffset, PRInt32 aLength);
  void FindEndOfJustificationRange(gfxSkipCharsIterator* aIter);

  /**
   * Count the number of spaces inserted for tabs in the given transformed substring
   */
  PRUint32 GetTabExpansionCount(PRUint32 aOffset, PRUint32 aLength);

  const nsStyleText* GetStyleText() { return mTextStyle; }
  nsTextFrame* GetFrame() { return mFrame; }
  // This may not be equal to the frame offset/length in because we may have
  // adjusted for whitespace trimming according to the state bits set in the frame
  // (for the static provider)
  const gfxSkipCharsIterator& GetStart() { return mStart; }
  PRUint32 GetOriginalLength() { return mLength; }
  const nsTextFragment* GetFragment() { return mFrag; }

  gfxFontGroup* GetFontGroup() {
    if (!mFontGroup) {
      mFontGroup = GetFontGroupForFrame(mFrame);
    }
    return mFontGroup;
  }

protected:
  void SetupJustificationSpacing();

  // Offsets in transformed string coordinates
  PRUint8* ComputeTabSpaceCount(PRUint32 aOffset, PRUint32 aLength);

  gfxTextRun*           mTextRun;
  gfxFontGroup*         mFontGroup;
  const nsStyleText*    mTextStyle;
  const nsTextFragment* mFrag;
  nsTextFrame*          mFrame;
  gfxSkipCharsIterator  mStart;  // Offset in original and transformed string
  nsTArray<PRUint8>     mTabSpaceCounts;  // counts for transformed string characters
  PRUint32              mCurrentColumn;
  PRInt32               mLength; // DOM string length
  gfxFloat              mWordSpacing;     // space for each whitespace char
  gfxFloat              mLetterSpacing;   // space for each letter
  gfxFloat              mJustificationSpacing;
  gfxFloat              mHyphenWidth;
};

PRUint32
PropertyProvider::ComputeJustifiableCharacters(PRInt32 aOffset, PRInt32 aLength)
{
  // Scan non-skipped characters and count justifiable chars.
  nsSkipCharsRunIterator
    run(mStart, nsSkipCharsRunIterator::LENGTH_INCLUDES_SKIPPED, aLength);
  run.SetOriginalOffset(aOffset);
  PRUint32 justifiableChars = 0;
  PRBool isCJK = IsChineseJapaneseLangGroup(mFrame);
  while (run.NextRun()) {
    PRInt32 i;
    for (i = 0; i < run.GetRunLength(); ++i) {
      justifiableChars +=
        IsJustifiableCharacter(mFrag, run.GetOriginalOffset() + i, isCJK);
    }
  }
  return justifiableChars;
}

PRUint8*
PropertyProvider::ComputeTabSpaceCount(PRUint32 aOffset, PRUint32 aLength)
{
  PRUint32 tabsEnd = mStart.GetSkippedOffset() + mTabSpaceCounts.Length();
  // We incrementally compute the tab space counts because it could be
  // inefficient to run ahead and compute space counts for tabs we don't need.
  // mTabSpaceCounts is an array of space counts whose first element is
  // the space count for the character at startOffset
  if (aOffset + aLength > tabsEnd) {
    PRUint32 column = mTabSpaceCounts.Length() ? mCurrentColumn : mFrame->GetColumn();
    PRInt32 count = aOffset + aLength - tabsEnd;
    nsSkipCharsRunIterator
      run(mStart, nsSkipCharsRunIterator::LENGTH_UNSKIPPED_ONLY, count);
    run.SetSkippedOffset(tabsEnd);
    while (run.NextRun()) {
      PRInt32 i;
      for (i = 0; i < run.GetRunLength(); ++i) {
        if (mFrag->CharAt(i + run.GetOriginalOffset()) == '\t') {
          PRInt32 spaces = 8 - column%8;
          column += spaces;
          // The tab itself counts as a space
          mTabSpaceCounts.AppendElement(spaces - 1);
        } else {
          if (mTextRun->IsClusterStart(i + run.GetSkippedOffset())) {
            ++column;
          }
          mTabSpaceCounts.AppendElement(0);
        }
      }
    }
    mCurrentColumn = column;
  }

  return mTabSpaceCounts.Elements() + aOffset - mStart.GetSkippedOffset();
}

/**
 * Finds the offset of the first character of the cluster containing aPos
 */
static void FindClusterStart(gfxTextRun* aTextRun,
                             gfxSkipCharsIterator* aPos)
{
  while (aPos->GetOriginalOffset() > 0) {
    if (aPos->IsOriginalCharSkipped() ||
        aTextRun->IsClusterStart(aPos->GetSkippedOffset())) {
      break;
    }
    aPos->AdvanceOriginal(-1);
  }
}

/**
 * Finds the offset of the last character of the cluster containing aPos
 */
static void FindClusterEnd(gfxTextRun* aTextRun, PRInt32 aOriginalEnd,
                           gfxSkipCharsIterator* aPos)
{
  NS_PRECONDITION(aPos->GetOriginalOffset() < aOriginalEnd,
                  "character outside string");
  aPos->AdvanceOriginal(1);
  while (aPos->GetOriginalOffset() < aOriginalEnd) {
    if (aPos->IsOriginalCharSkipped() ||
        aTextRun->IsClusterStart(aPos->GetSkippedOffset())) {
      break;
    }
    aPos->AdvanceOriginal(1);
  }
  aPos->AdvanceOriginal(-1);
}

// aStart, aLength in transformed string offsets
void
PropertyProvider::GetSpacing(PRUint32 aStart, PRUint32 aLength,
                             Spacing* aSpacing)
{
  NS_PRECONDITION(IsInBounds(mStart, mLength, aStart, aLength), "Range out of bounds");

  PRUint32 index;
  for (index = 0; index < aLength; ++index) {
    aSpacing[index].mBefore = 0.0;
    aSpacing[index].mAfter = 0.0;
  }

  // Find our offset into the original+transformed string
  gfxSkipCharsIterator start(mStart);
  start.SetSkippedOffset(aStart);

  // First, compute the word and letter spacing
  if (mWordSpacing || mLetterSpacing) {
    // Iterate over non-skipped characters
    nsSkipCharsRunIterator
      run(start, nsSkipCharsRunIterator::LENGTH_UNSKIPPED_ONLY, aLength);
    while (run.NextRun()) {
      PRUint32 runOffsetInSubstring = run.GetSkippedOffset() - aStart;
      PRInt32 i;
      gfxSkipCharsIterator iter = run.GetPos();
      for (i = 0; i < run.GetRunLength(); ++i) {
        if (i + 1 >= run.GetRunLength() ||
            mTextRun->IsClusterStart(i + 1 + run.GetSkippedOffset())) {
          // End of a cluster, put letter-spacing after it
          aSpacing[runOffsetInSubstring + i].mAfter += mLetterSpacing;
        }
        if (IsCSSWordSpacingSpace(mFrag, i + run.GetOriginalOffset())) {
          // It kinda sucks, but space characters can be part of clusters,
          // and even still be whitespace (I think!)
          iter.SetSkippedOffset(run.GetSkippedOffset() + i);
          FindClusterEnd(mTextRun, run.GetOriginalOffset() + run.GetRunLength(),
                         &iter);
          aSpacing[iter.GetSkippedOffset() - aStart].mAfter += mWordSpacing;
        }
      }
    }
  }

  // Now add tab spacing, if there is any
  if (mTextRun->GetFlags() & nsTextFrameUtils::TEXT_HAS_TAB) {
    // ComputeTabSpaceCount() will tell us where spaces need to be inserted
    // for tabs, and how many.
    // ComputeTabSpaceCount takes transformed string offsets.
    PRUint8* tabSpaceList = ComputeTabSpaceCount(aStart, aLength);
    gfxTextRun* spaceTextRun =
      GetSpecialString(GetFontGroup(), gfxFontGroup::STRING_SPACE, mTextRun);
    gfxFloat spaceWidth = mLetterSpacing + mWordSpacing;
    if (spaceTextRun) {
      spaceWidth += spaceTextRun->GetAdvanceWidth(0,  spaceTextRun->GetLength(), nsnull);
    }
    for (index = 0; index < aLength; ++index) {
      PRInt32 tabSpaces = tabSpaceList[index];
      aSpacing[index].mAfter += spaceWidth*tabSpaces;
    }
  }

  // Now add in justification spacing
  if (mJustificationSpacing) {
    gfxFloat halfJustificationSpace = mJustificationSpacing/2;
    // Scan non-skipped characters and adjust justifiable chars, adding
    // justification space on either side of the cluster
    PRBool isCJK = IsChineseJapaneseLangGroup(mFrame);
    gfxSkipCharsIterator justificationEnd(mStart);
    FindEndOfJustificationRange(&justificationEnd);

    nsSkipCharsRunIterator
      run(start, nsSkipCharsRunIterator::LENGTH_UNSKIPPED_ONLY, aLength);
    while (run.NextRun()) {
      PRInt32 i;
      gfxSkipCharsIterator iter = run.GetPos();
      for (i = 0; i < run.GetRunLength(); ++i) {
        PRInt32 originalOffset = run.GetOriginalOffset() + i;
        if (IsJustifiableCharacter(mFrag, originalOffset, isCJK)) {
          iter.SetOriginalOffset(originalOffset);
          FindClusterStart(mTextRun, &iter);
          PRUint32 clusterFirstChar = iter.GetSkippedOffset();
          FindClusterEnd(mTextRun, run.GetOriginalOffset() + run.GetRunLength(), &iter);
          PRUint32 clusterLastChar = iter.GetSkippedOffset();
          // Only apply justification to characters before justificationEnd
          if (clusterLastChar < justificationEnd.GetSkippedOffset()) {
            aSpacing[clusterFirstChar - aStart].mBefore += halfJustificationSpace;
            aSpacing[clusterLastChar - aStart].mAfter += halfJustificationSpace;
          }
        }
      }
    }
  }
}

PRUint32
PropertyProvider::GetTabExpansionCount(PRUint32 aStart, PRUint32 aLength)
{
  if (!(mTextRun->GetFlags() & nsTextFrameUtils::TEXT_HAS_TAB))
    return 0;

  PRUint8* spaces = ComputeTabSpaceCount(aStart, aLength);
  PRUint32 i;
  PRUint32 sum = 0;
  for (i = 0; i < aLength; ++i) {
    sum += spaces[i];
  }
  return sum;
}

gfxFloat
PropertyProvider::GetHyphenWidth()
{
  if (mHyphenWidth < 0) {
    gfxTextRun* hyphenTextRun =
      GetSpecialString(GetFontGroup(), gfxFontGroup::STRING_HYPHEN, mTextRun);
    mHyphenWidth = mLetterSpacing;
    if (hyphenTextRun) {
      mHyphenWidth += hyphenTextRun->GetAdvanceWidth(0, hyphenTextRun->GetLength(), nsnull);
    }
  }
  return mHyphenWidth;
}

void
PropertyProvider::GetHyphenationBreaks(PRUint32 aStart, PRUint32 aLength,
                                       PRPackedBool* aBreakBefore)
{
  NS_PRECONDITION(IsInBounds(mStart, mLength, aStart, aLength), "Range out of bounds");

  if (!mTextStyle->WhiteSpaceCanWrap()) {
    memset(aBreakBefore, PR_FALSE, aLength);
    return;
  }

  // Iterate through the original-string character runs
  nsSkipCharsRunIterator
    run(mStart, nsSkipCharsRunIterator::LENGTH_INCLUDES_SKIPPED, aLength);
  run.SetSkippedOffset(aStart);
  // We need to visit skipped characters so that we can detect SHY
  run.SetVisitSkipped();

  PRBool allowHyphenBreakBeforeNextChar =
    run.GetPos().GetOriginalOffset() > mStart.GetOriginalOffset() &&
    mFrag->CharAt(run.GetPos().GetOriginalOffset() - 1) == CH_SHY;

  while (run.NextRun()) {
    NS_ASSERTION(run.GetRunLength() > 0, "Shouldn't return zero-length runs");
    if (run.IsSkipped()) {
      // Check if there's a soft hyphen which would let us hyphenate before
      // the next non-skipped character. Don't look at soft hyphens followed
      // by other skipped characters, we won't use them.
      allowHyphenBreakBeforeNextChar =
        mFrag->CharAt(run.GetOriginalOffset() + run.GetRunLength() - 1) == CH_SHY;
    } else {
      PRInt32 runOffsetInSubstring = run.GetSkippedOffset() - aStart;
      memset(aBreakBefore + runOffsetInSubstring, 0, run.GetRunLength());
      aBreakBefore[runOffsetInSubstring] = allowHyphenBreakBeforeNextChar;
      allowHyphenBreakBeforeNextChar = PR_FALSE;
    }
  }
}

void
PropertyProvider::InitializeForDisplay(PRBool aTrimAfter)
{
  nsTextFrame::TrimmedOffsets trimmed =
    mFrame->GetTrimmedOffsets(mFrag, aTrimAfter);
  mStart.SetOriginalOffset(trimmed.mStart);
  mLength = trimmed.mLength;
  SetupJustificationSpacing();
}

static PRUint32 GetSkippedDistance(const gfxSkipCharsIterator& aStart,
                                   const gfxSkipCharsIterator& aEnd)
{
  return aEnd.GetSkippedOffset() - aStart.GetSkippedOffset();
}

void
PropertyProvider::FindEndOfJustificationRange(gfxSkipCharsIterator* aIter)
{
  if (!(mFrame->GetStateBits() & TEXT_END_OF_LINE))
    return;

  // Ignore trailing cluster at end of line for justification purposes
  aIter->SetOriginalOffset(mStart.GetOriginalOffset() + mLength);
  while (aIter->GetOriginalOffset() > mStart.GetOriginalOffset()) {
    aIter->AdvanceOriginal(-1);
    if (!aIter->IsOriginalCharSkipped() &&
        mTextRun->IsClusterStart(aIter->GetSkippedOffset()))
      break;
  }
}

void
PropertyProvider::SetupJustificationSpacing()
{
  if (NS_STYLE_TEXT_ALIGN_JUSTIFY != mTextStyle->mTextAlign ||
      mTextStyle->WhiteSpaceIsSignificant())
    return;

  gfxSkipCharsIterator end(mStart);
  end.AdvanceOriginal(mLength);
  gfxSkipCharsIterator realEnd(end);
  FindEndOfJustificationRange(&end);

  PRInt32 justifiableCharacters =
    ComputeJustifiableCharacters(mStart.GetOriginalOffset(),
                                 end.GetOriginalOffset() - mStart.GetOriginalOffset());
  if (justifiableCharacters == 0) {
    // Nothing to do, nothing is justifiable and we shouldn't have any
    // justification space assigned
    return;
  }

  gfxFloat naturalWidth =
    mTextRun->GetAdvanceWidth(mStart.GetSkippedOffset(),
                              GetSkippedDistance(mStart, realEnd), this);
  gfxFloat totalJustificationSpace = mFrame->GetSize().width - naturalWidth;
  if (totalJustificationSpace <= 0) {
    // No space available
    return;
  }
  
  mJustificationSpacing = totalJustificationSpace/justifiableCharacters;
}

//----------------------------------------------------------------------

// Helper class for managing blinking text

class nsBlinkTimer : public nsITimerCallback
{
public:
  nsBlinkTimer();
  virtual ~nsBlinkTimer();

  NS_DECL_ISUPPORTS

  void AddFrame(nsPresContext* aPresContext, nsIFrame* aFrame);

  PRBool RemoveFrame(nsIFrame* aFrame);

  PRInt32 FrameCount();

  void Start();

  void Stop();

  NS_DECL_NSITIMERCALLBACK

  static nsresult AddBlinkFrame(nsPresContext* aPresContext, nsIFrame* aFrame);
  static nsresult RemoveBlinkFrame(nsIFrame* aFrame);
  
  static PRBool   GetBlinkIsOff() { return sState == 3; }
  
protected:

  struct FrameData {
    nsPresContext* mPresContext;  // pres context associated with the frame
    nsIFrame*       mFrame;


    FrameData(nsPresContext* aPresContext,
              nsIFrame*       aFrame)
      : mPresContext(aPresContext), mFrame(aFrame) {}
  };

  nsCOMPtr<nsITimer> mTimer;
  nsVoidArray     mFrames;
  nsPresContext* mPresContext;

protected:

  static nsBlinkTimer* sTextBlinker;
  static PRUint32      sState; // 0-2 == on; 3 == off
  
};

nsBlinkTimer* nsBlinkTimer::sTextBlinker = nsnull;
PRUint32      nsBlinkTimer::sState = 0;

#ifdef NOISY_BLINK
static PRTime gLastTick;
#endif

nsBlinkTimer::nsBlinkTimer()
{
}

nsBlinkTimer::~nsBlinkTimer()
{
  Stop();
  sTextBlinker = nsnull;
}

void nsBlinkTimer::Start()
{
  nsresult rv;
  mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
  if (NS_OK == rv) {
    mTimer->InitWithCallback(this, 250, nsITimer::TYPE_REPEATING_PRECISE);
  }
}

void nsBlinkTimer::Stop()
{
  if (nsnull != mTimer) {
    mTimer->Cancel();
  }
}

NS_IMPL_ISUPPORTS1(nsBlinkTimer, nsITimerCallback)

void nsBlinkTimer::AddFrame(nsPresContext* aPresContext, nsIFrame* aFrame) {
  FrameData* frameData = new FrameData(aPresContext, aFrame);
  mFrames.AppendElement(frameData);
  if (1 == mFrames.Count()) {
    Start();
  }
}

PRBool nsBlinkTimer::RemoveFrame(nsIFrame* aFrame) {
  PRInt32 i, n = mFrames.Count();
  PRBool rv = PR_FALSE;
  for (i = 0; i < n; i++) {
    FrameData* frameData = (FrameData*) mFrames.ElementAt(i);

    if (frameData->mFrame == aFrame) {
      rv = mFrames.RemoveElementAt(i);
      delete frameData;
      break;
    }
  }
  
  if (0 == mFrames.Count()) {
    Stop();
  }
  return rv;
}

PRInt32 nsBlinkTimer::FrameCount() {
  return mFrames.Count();
}

NS_IMETHODIMP nsBlinkTimer::Notify(nsITimer *timer)
{
  // Toggle blink state bit so that text code knows whether or not to
  // render. All text code shares the same flag so that they all blink
  // in unison.
  sState = (sState + 1) % 4;
  if (sState == 1 || sState == 2)
    // States 0, 1, and 2 are all the same.
    return NS_OK;

#ifdef NOISY_BLINK
  PRTime now = PR_Now();
  char buf[50];
  PRTime delta;
  LL_SUB(delta, now, gLastTick);
  gLastTick = now;
  PR_snprintf(buf, sizeof(buf), "%lldusec", delta);
  printf("%s\n", buf);
#endif

  PRInt32 i, n = mFrames.Count();
  for (i = 0; i < n; i++) {
    FrameData* frameData = (FrameData*) mFrames.ElementAt(i);

    // Determine damaged area and tell view manager to redraw it
    // blink doesn't blink outline ... I hope
    nsRect bounds(nsPoint(0, 0), frameData->mFrame->GetSize());
    frameData->mFrame->Invalidate(bounds, PR_FALSE);
  }
  return NS_OK;
}


// static
nsresult nsBlinkTimer::AddBlinkFrame(nsPresContext* aPresContext, nsIFrame* aFrame)
{
  if (!sTextBlinker)
  {
    sTextBlinker = new nsBlinkTimer;
    if (!sTextBlinker) return NS_ERROR_OUT_OF_MEMORY;
  }
  
  NS_ADDREF(sTextBlinker);

  sTextBlinker->AddFrame(aPresContext, aFrame);
  return NS_OK;
}


// static
nsresult nsBlinkTimer::RemoveBlinkFrame(nsIFrame* aFrame)
{
  NS_ASSERTION(sTextBlinker, "Should have blink timer here");
  
  nsBlinkTimer* blinkTimer = sTextBlinker;    // copy so we can call NS_RELEASE on it
  if (!blinkTimer) return NS_OK;
  
  blinkTimer->RemoveFrame(aFrame);  
  NS_RELEASE(blinkTimer);
  
  return NS_OK;
}

//----------------------------------------------------------------------

static nscolor
EnsureDifferentColors(nscolor colorA, nscolor colorB)
{
  if (colorA == colorB) {
    nscolor res;
    res = NS_RGB(NS_GET_R(colorA) ^ 0xff,
                 NS_GET_G(colorA) ^ 0xff,
                 NS_GET_B(colorA) ^ 0xff);
    return res;
  }
  return colorA;
}

//-----------------------------------------------------------------------------

// TODO delete nsCSSRendering::TransformColor because we're moving it here
static nscolor
DarkenColor(nscolor aColor)
{
  PRUint16  hue,sat,value;

  // convert the RBG to HSV so we can get the lightness (which is the v)
  NS_RGB2HSV(aColor,hue,sat,value);

  // The goal here is to send white to black while letting colored
  // stuff stay colored... So we adopt the following approach.
  // Something with sat = 0 should end up with value = 0.  Something
  // with a high sat can end up with a high value and it's ok.... At
  // the same time, we don't want to make things lighter.  Do
  // something simple, since it seems to work.
  if (value > sat) {
    value = sat;
    // convert this color back into the RGB color space.
    NS_HSV2RGB(aColor,hue,sat,value);
  }
  return aColor;
}

// Check whether we should darken text colors. We need to do this if
// background images and colors are being suppressed, because that means
// light text will not be visible against the (presumed light-colored) background.
static PRBool
ShouldDarkenColors(nsPresContext* aPresContext)
{
  return !aPresContext->GetBackgroundColorDraw() &&
    !aPresContext->GetBackgroundImageDraw();
}

nsTextPaintStyle::nsTextPaintStyle(nsTextFrame* aFrame)
  : mFrame(aFrame),
    mPresContext(aFrame->GetPresContext()),
    mInitCommonColors(PR_FALSE),
    mInitSelectionColors(PR_FALSE)
{
  for (int i = 0; i < 4; i++)
    mIMEColor[i].mInit = PR_FALSE;
  mIMEUnderlineRelativeSize = -1.0f;
}

PRBool
nsTextPaintStyle::EnsureSufficientContrast(nscolor *aForeColor, nscolor *aBackColor)
{
  InitCommonColors();

  // If the combination of selection background color and frame background color
  // is sufficient contrast, don't exchange the selection colors.
  PRInt32 backLuminosityDifference =
            NS_LUMINOSITY_DIFFERENCE(*aBackColor, mFrameBackgroundColor);
  if (backLuminosityDifference >= mSufficientContrast)
    return PR_FALSE;

  // Otherwise, we should use the higher-contrast color for the selection
  // background color.
  PRInt32 foreLuminosityDifference =
            NS_LUMINOSITY_DIFFERENCE(*aForeColor, mFrameBackgroundColor);
  if (backLuminosityDifference < foreLuminosityDifference) {
    nscolor tmpColor = *aForeColor;
    *aForeColor = *aBackColor;
    *aBackColor = tmpColor;
    return PR_TRUE;
  }
  return PR_FALSE;
}

nscolor
nsTextPaintStyle::GetTextColor()
{
  nscolor color = mFrame->GetStyleColor()->mColor;
  if (ShouldDarkenColors(mPresContext)) {
    color = DarkenColor(color);
  }
  return color;
}

PRBool
nsTextPaintStyle::GetSelectionColors(nscolor* aForeColor,
                                     nscolor* aBackColor)
{
  NS_ASSERTION(aForeColor, "aForeColor is null");
  NS_ASSERTION(aBackColor, "aBackColor is null");

  if (!InitSelectionColors())
    return PR_FALSE;

  *aForeColor = mSelectionTextColor;
  *aBackColor = mSelectionBGColor;
  return PR_TRUE;
}

void
nsTextPaintStyle::GetIMESelectionColors(PRInt32  aIndex,
                                        nscolor* aForeColor,
                                        nscolor* aBackColor)
{
  NS_ASSERTION(aForeColor, "aForeColor is null");
  NS_ASSERTION(aBackColor, "aBackColor is null");
  NS_ASSERTION(aIndex >= 0 && aIndex < 4, "Index out of range");

  nsIMEColor* IMEColor = GetIMEColor(aIndex);
  *aForeColor = IMEColor->mTextColor;
  *aBackColor = IMEColor->mBGColor;
}

PRBool
nsTextPaintStyle::GetIMEUnderline(PRInt32  aIndex,
                                  nscolor* aLineColor,
                                  float*   aRelativeSize)
{
  NS_ASSERTION(aLineColor, "aLineColor is null");
  NS_ASSERTION(aRelativeSize, "aRelativeSize is null");
  NS_ASSERTION(aIndex >= 0 && aIndex < 4, "Index out of range");

  nsIMEColor* IMEColor = GetIMEColor(aIndex);
  if (IMEColor->mUnderlineColor == NS_TRANSPARENT ||
      mIMEUnderlineRelativeSize <= 0.0f)
    return PR_FALSE;

  *aLineColor = IMEColor->mUnderlineColor;
  *aRelativeSize = mIMEUnderlineRelativeSize;
  return PR_TRUE;
}

void
nsTextPaintStyle::InitCommonColors()
{
  if (mInitCommonColors)
    return;

  nsStyleContext* sc = mFrame->GetStyleContext();

  const nsStyleBackground* bg =
    nsCSSRendering::FindNonTransparentBackground(sc);
  NS_ASSERTION(bg, "Cannot find NonTransparentBackground.");
  mFrameBackgroundColor = bg->mBackgroundColor;

  nsILookAndFeel* look = mPresContext->LookAndFeel();
  nscolor defaultWindowBackgroundColor, selectionTextColor, selectionBGColor;
  look->GetColor(nsILookAndFeel::eColor_TextSelectBackground,
                 selectionBGColor);
  look->GetColor(nsILookAndFeel::eColor_TextSelectForeground,
                 selectionTextColor);
  look->GetColor(nsILookAndFeel::eColor_WindowBackground,
                 defaultWindowBackgroundColor);

  mSufficientContrast =
    PR_MIN(PR_MIN(NS_SUFFICIENT_LUMINOSITY_DIFFERENCE,
                  NS_LUMINOSITY_DIFFERENCE(selectionTextColor,
                                           selectionBGColor)),
                  NS_LUMINOSITY_DIFFERENCE(defaultWindowBackgroundColor,
                                           selectionBGColor));

  mInitCommonColors = PR_TRUE;
}

static nsIFrame* GetNonGeneratedAncestor(nsIFrame* f) {
  while (f->GetStateBits() & NS_FRAME_GENERATED_CONTENT) {
    f = f->GetParent();
  }
  return f;
}

static nsIContent*
FindElementAncestor(nsINode* aNode)
{
  while (aNode && !aNode->IsNodeOfType(nsINode::eELEMENT)) {
    aNode = aNode->GetParent();
  }
  return NS_STATIC_CAST(nsIContent*, aNode);
}

PRBool
nsTextPaintStyle::InitSelectionColors()
{
  if (mInitSelectionColors)
    return PR_TRUE;

  PRInt16 selectionFlags;
  PRInt16 selectionStatus = mFrame->GetSelectionStatus(&selectionFlags);
  if (!(selectionFlags & nsISelectionDisplay::DISPLAY_TEXT) ||
      selectionStatus < nsISelectionController::SELECTION_ON) {
    // Not displaying the normal selection.
    // We're not caching this fact, so every call to GetSelectionColors
    // will come through here. We could avoid this, but it's not really worth it.
    return PR_FALSE;
  }

  mInitSelectionColors = PR_TRUE;

  nsIFrame* nonGeneratedAncestor = GetNonGeneratedAncestor(mFrame);
  nsIContent* selectionContent = FindElementAncestor(nonGeneratedAncestor->GetContent());

  if (selectionContent &&
      selectionStatus == nsISelectionController::SELECTION_ON) {
    nsRefPtr<nsStyleContext> sc = nsnull;
    sc = mPresContext->StyleSet()->
      ProbePseudoStyleFor(selectionContent, nsCSSPseudoElements::mozSelection,
                          mFrame->GetStyleContext());
    // Use -moz-selection pseudo class.
    if (sc) {
      const nsStyleBackground* bg = sc->GetStyleBackground();
      mSelectionBGColor = bg->mBackgroundColor;
      if (bg->mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT) {
        mSelectionBGColor = NS_RGBA(0,0,0,0);
      }
      mSelectionTextColor = sc->GetStyleColor()->mColor;
      return PR_TRUE;
    }
  }

  nsILookAndFeel* look = mPresContext->LookAndFeel();

  nscolor selectionBGColor;
  look->GetColor(nsILookAndFeel::eColor_TextSelectBackground,
                 selectionBGColor);

  if (selectionStatus == nsISelectionController::SELECTION_ATTENTION) {
    look->GetColor(nsILookAndFeel::eColor_TextSelectBackgroundAttention,
                   mSelectionBGColor);
    mSelectionBGColor  = EnsureDifferentColors(mSelectionBGColor,
                                               selectionBGColor);
  } else if (selectionStatus != nsISelectionController::SELECTION_ON) {
    look->GetColor(nsILookAndFeel::eColor_TextSelectBackgroundDisabled,
                   mSelectionBGColor);
    mSelectionBGColor  = EnsureDifferentColors(mSelectionBGColor,
                                               selectionBGColor);
  } else {
    mSelectionBGColor = selectionBGColor;
  }

  look->GetColor(nsILookAndFeel::eColor_TextSelectForeground,
                 mSelectionTextColor);

  // On MacOS X, we don't exchange text color and BG color.
  if (mSelectionTextColor == NS_DONT_CHANGE_COLOR) {
    mSelectionTextColor = EnsureDifferentColors(mFrame->GetStyleColor()->mColor,
                                                mSelectionBGColor);
  } else {
    EnsureSufficientContrast(&mSelectionTextColor, &mSelectionBGColor);
  }
  return PR_TRUE;
}

nsTextPaintStyle::nsIMEColor*
nsTextPaintStyle::GetIMEColor(PRInt32 aIndex)
{
  InitIMEColor(aIndex);
  return &mIMEColor[aIndex];
}

struct ColorIDTriple {
  nsILookAndFeel::nsColorID mForeground, mBackground, mLine;
};
static ColorIDTriple IMEColorIDs[] = {
  { nsILookAndFeel::eColor_IMERawInputForeground,
    nsILookAndFeel::eColor_IMERawInputBackground,
    nsILookAndFeel::eColor_IMERawInputUnderline },
  { nsILookAndFeel::eColor_IMESelectedRawTextForeground,
    nsILookAndFeel::eColor_IMESelectedRawTextBackground,
    nsILookAndFeel::eColor_IMESelectedRawTextUnderline },
  { nsILookAndFeel::eColor_IMEConvertedTextForeground,
    nsILookAndFeel::eColor_IMEConvertedTextBackground,
    nsILookAndFeel::eColor_IMEConvertedTextUnderline },
  { nsILookAndFeel::eColor_IMESelectedConvertedTextForeground,
    nsILookAndFeel::eColor_IMESelectedConvertedTextBackground,
    nsILookAndFeel::eColor_IMESelectedConvertedTextUnderline }
};

void
nsTextPaintStyle::InitIMEColor(PRInt32 aIndex)
{
  nsIMEColor* IMEColor = &mIMEColor[aIndex];
  if (IMEColor->mInit)
    return;

  ColorIDTriple* colorIDs = &IMEColorIDs[aIndex];

  nsILookAndFeel* look = mPresContext->LookAndFeel();
  nscolor foreColor, backColor, lineColor;
  look->GetColor(colorIDs->mForeground, foreColor);
  look->GetColor(colorIDs->mBackground, backColor);
  look->GetColor(colorIDs->mLine, lineColor);

  // Convert special color to actual color
  NS_ASSERTION(foreColor != NS_TRANSPARENT,
               "foreColor cannot be NS_TRANSPARENT");
  NS_ASSERTION(backColor != NS_SAME_AS_FOREGROUND_COLOR,
               "backColor cannot be NS_SAME_AS_FOREGROUND_COLOR");
  NS_ASSERTION(backColor != NS_40PERCENT_FOREGROUND_COLOR,
               "backColor cannot be NS_40PERCENT_FOREGROUND_COLOR");

  foreColor = GetResolvedForeColor(foreColor, GetTextColor(), backColor);

  if (NS_GET_A(backColor) > 0)
    EnsureSufficientContrast(&foreColor, &backColor);

  lineColor = GetResolvedForeColor(lineColor, foreColor, backColor);

  IMEColor->mTextColor       = foreColor;
  IMEColor->mBGColor         = backColor;
  IMEColor->mUnderlineColor  = lineColor;
  IMEColor->mInit            = PR_TRUE;

  if (mIMEUnderlineRelativeSize == -1.0f) {
    look->GetMetric(nsILookAndFeel::eMetricFloat_IMEUnderlineRelativeSize,
                    mIMEUnderlineRelativeSize);
    NS_ASSERTION(mIMEUnderlineRelativeSize >= 0.0f,
                 "underline size must be larger than 0");
  }
}

inline nscolor Get40PercentColor(nscolor aForeColor, nscolor aBackColor)
{
  nscolor foreColor = NS_RGBA(NS_GET_R(aForeColor),
                              NS_GET_G(aForeColor),
                              NS_GET_B(aForeColor),
                              (PRUint8)(255 * 0.4f));
  return NS_ComposeColors(aBackColor, foreColor);
}

nscolor
nsTextPaintStyle::GetResolvedForeColor(nscolor aColor,
                                       nscolor aDefaultForeColor,
                                       nscolor aBackColor)
{
  if (aColor == NS_SAME_AS_FOREGROUND_COLOR)
    return aDefaultForeColor;

  if (aColor != NS_40PERCENT_FOREGROUND_COLOR)
    return aColor;

  // Get actual background color
  nscolor actualBGColor = aBackColor;
  if (actualBGColor == NS_TRANSPARENT) {
    InitCommonColors();
    actualBGColor = mFrameBackgroundColor;
  }
  return Get40PercentColor(aDefaultForeColor, actualBGColor);
}

//-----------------------------------------------------------------------------

#ifdef ACCESSIBILITY
NS_IMETHODIMP nsTextFrame::GetAccessible(nsIAccessible** aAccessible)
{
  if (!IsEmpty() || GetNextInFlow()) {

    nsCOMPtr<nsIAccessibilityService> accService = do_GetService("@mozilla.org/accessibilityService;1");

    if (accService) {
      return accService->CreateHTMLTextAccessible(NS_STATIC_CAST(nsIFrame*, this), aAccessible);
    }
  }
  return NS_ERROR_FAILURE;
}
#endif


//-----------------------------------------------------------------------------
NS_IMETHODIMP
nsTextFrame::Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow)
{
  NS_ASSERTION(!aPrevInFlow, "Can't be a continuation!");
  NS_PRECONDITION(aContent->IsNodeOfType(nsINode::eTEXT),
                  "Bogus content!");
  nsresult rv = nsFrame::Init(aContent, aParent, aPrevInFlow);
  // Note that if we're created due to bidi splitting the bidi code
  // will override what we compute here, so it's ok.
  // We're not a continuing frame.
  // mContentOffset = 0; not necessary since we get zeroed out at init
  mContentLength = GetInFlowContentLength();
  return rv;
}

void
nsTextFrame::Destroy()
{
  if (mNextContinuation) {
    mNextContinuation->SetPrevInFlow(nsnull);
  }
  ClearTextRun();
  // Let the base class destroy the frame
  nsFrame::Destroy();
}

class nsContinuingTextFrame : public nsTextFrame {
public:
  friend nsIFrame* NS_NewContinuingTextFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);

  virtual void Destroy();

  virtual nsIFrame* GetPrevContinuation() const {
    return mPrevContinuation;
  }
  NS_IMETHOD SetPrevContinuation(nsIFrame* aPrevContinuation) {
    NS_ASSERTION (!aPrevContinuation || GetType() == aPrevContinuation->GetType(),
                  "setting a prev continuation with incorrect type!");
    NS_ASSERTION (!nsSplittableFrame::IsInPrevContinuationChain(aPrevContinuation, this),
                  "creating a loop in continuation chain!");
    mPrevContinuation = aPrevContinuation;
    RemoveStateBits(NS_FRAME_IS_FLUID_CONTINUATION);
    return NS_OK;
  }
  virtual nsIFrame* GetPrevInFlowVirtual() const { return GetPrevInFlow(); }
  nsIFrame* GetPrevInFlow() const {
    return (GetStateBits() & NS_FRAME_IS_FLUID_CONTINUATION) ? mPrevContinuation : nsnull;
  }
  NS_IMETHOD SetPrevInFlow(nsIFrame* aPrevInFlow) {
    NS_ASSERTION (!aPrevInFlow || GetType() == aPrevInFlow->GetType(),
                  "setting a prev in flow with incorrect type!");
    NS_ASSERTION (!nsSplittableFrame::IsInPrevContinuationChain(aPrevInFlow, this),
                  "creating a loop in continuation chain!");
    mPrevContinuation = aPrevInFlow;
    AddStateBits(NS_FRAME_IS_FLUID_CONTINUATION);
    return NS_OK;
  }
  virtual nsIFrame* GetFirstInFlow() const;
  virtual nsIFrame* GetFirstContinuation() const;

  virtual void AddInlineMinWidth(nsIRenderingContext *aRenderingContext,
                                 InlineMinWidthData *aData);
  virtual void AddInlinePrefWidth(nsIRenderingContext *aRenderingContext,
                                  InlinePrefWidthData *aData);
  
protected:
  nsContinuingTextFrame(nsStyleContext* aContext) : nsTextFrame(aContext) {}
  nsIFrame* mPrevContinuation;
};

NS_IMETHODIMP
nsContinuingTextFrame::Init(nsIContent* aContent,
                            nsIFrame*   aParent,
                            nsIFrame*   aPrevInFlow)
{
  NS_ASSERTION(aPrevInFlow, "Must be a continuation!");
  // NOTE: bypassing nsTextFrame::Init!!!
  nsresult rv = nsFrame::Init(aContent, aParent, aPrevInFlow);

  nsIFrame* nextContinuation = aPrevInFlow->GetNextContinuation();
  // Hook the frame into the flow
  SetPrevInFlow(aPrevInFlow);
  aPrevInFlow->SetNextInFlow(this);
  nsTextFrame* prev = NS_STATIC_CAST(nsTextFrame*, aPrevInFlow);
  mTextRun = prev->GetTextRun();
  mContentOffset = prev->GetContentOffset() + prev->GetContentLength();
#ifdef IBMBIDI
  if (aPrevInFlow->GetStateBits() & NS_FRAME_IS_BIDI) {
    PRInt32 start, end;
    aPrevInFlow->GetOffsets(start, mContentOffset);

    nsPropertyTable *propTable = GetPresContext()->PropertyTable();
    propTable->SetProperty(this, nsGkAtoms::embeddingLevel,
          propTable->GetProperty(aPrevInFlow, nsGkAtoms::embeddingLevel),
                           nsnull, nsnull);
    propTable->SetProperty(this, nsGkAtoms::baseLevel,
              propTable->GetProperty(aPrevInFlow, nsGkAtoms::baseLevel),
                           nsnull, nsnull);
    propTable->SetProperty(this, nsGkAtoms::charType,
               propTable->GetProperty(aPrevInFlow, nsGkAtoms::charType),
                           nsnull, nsnull);
    if (nextContinuation) {
      SetNextContinuation(nextContinuation);
      nextContinuation->SetPrevContinuation(this);
      nextContinuation->GetOffsets(start, end);
      mContentLength = PR_MAX(1, start - mContentOffset);
    }
    mState |= NS_FRAME_IS_BIDI;
  } // prev frame is bidi
#endif // IBMBIDI

  return rv;
}

void
nsContinuingTextFrame::Destroy()
{
  if (mPrevContinuation || mNextContinuation) {
    nsSplittableFrame::RemoveFromFlow(this);
  }
  // Let the base class destroy the frame
  nsFrame::Destroy();
}

nsIFrame*
nsContinuingTextFrame::GetFirstInFlow() const
{
  // Can't cast to |nsContinuingTextFrame*| because the first one isn't.
  nsIFrame *firstInFlow,
           *previous = NS_CONST_CAST(nsIFrame*,
                                     NS_STATIC_CAST(const nsIFrame*, this));
  do {
    firstInFlow = previous;
    previous = firstInFlow->GetPrevInFlow();
  } while (previous);
  return firstInFlow;
}

nsIFrame*
nsContinuingTextFrame::GetFirstContinuation() const
{
  // Can't cast to |nsContinuingTextFrame*| because the first one isn't.
  nsIFrame *firstContinuation,
  *previous = NS_CONST_CAST(nsIFrame*,
                            NS_STATIC_CAST(const nsIFrame*, mPrevContinuation));
  do {
    firstContinuation = previous;
    previous = firstContinuation->GetPrevContinuation();
  } while (previous);
  return firstContinuation;
}

// XXX Do we want to do all the work for the first-in-flow or do the
// work for each part?  (Be careful of first-letter / first-line, though,
// especially first-line!)  Doing all the work on the first-in-flow has
// the advantage of avoiding the potential for incremental reflow bugs,
// but depends on our maintining the frame tree in reasonable ways even
// for edge cases (block-within-inline splits, nextBidi, etc.)

// XXX We really need to make :first-letter happen during frame
// construction.

// Needed for text frames in XUL.
/* virtual */ nscoord
nsTextFrame::GetMinWidth(nsIRenderingContext *aRenderingContext)
{
  return nsLayoutUtils::MinWidthFromInline(this, aRenderingContext);
}

// Needed for text frames in XUL.
/* virtual */ nscoord
nsTextFrame::GetPrefWidth(nsIRenderingContext *aRenderingContext)
{
  return nsLayoutUtils::PrefWidthFromInline(this, aRenderingContext);
}

/* virtual */ void
nsContinuingTextFrame::AddInlineMinWidth(nsIRenderingContext *aRenderingContext,
                                         InlineMinWidthData *aData)
{
  // Do nothing, since the first-in-flow accounts for everything.
  return;
}

/* virtual */ void
nsContinuingTextFrame::AddInlinePrefWidth(nsIRenderingContext *aRenderingContext,
                                          InlinePrefWidthData *aData)
{
  // Do nothing, since the first-in-flow accounts for everything.
  return;
}

static void 
DestroySelectionDetails(SelectionDetails* aDetails)
{
  while (aDetails) {
    SelectionDetails* next = aDetails->mNext;
    delete aDetails;
    aDetails = next;
  }
}

//----------------------------------------------------------------------

#if defined(DEBUG_rbs) || defined(DEBUG_bzbarsky)
static void
VerifyNotDirty(nsFrameState state)
{
  PRBool isZero = state & NS_FRAME_FIRST_REFLOW;
  PRBool isDirty = state & NS_FRAME_IS_DIRTY;
  if (!isZero && isDirty)
    NS_WARNING("internal offsets may be out-of-sync");
}
#define DEBUG_VERIFY_NOT_DIRTY(state) \
VerifyNotDirty(state)
#else
#define DEBUG_VERIFY_NOT_DIRTY(state)
#endif

nsIFrame*
NS_NewTextFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsTextFrame(aContext);
}

nsIFrame*
NS_NewContinuingTextFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsContinuingTextFrame(aContext);
}

nsTextFrame::~nsTextFrame()
{
  if (0 != (mState & TEXT_BLINK_ON))
  {
    nsBlinkTimer::RemoveBlinkFrame(this);
  }
}

NS_IMETHODIMP
nsTextFrame::GetCursor(const nsPoint& aPoint,
                       nsIFrame::Cursor& aCursor)
{
  FillCursorInformationFromStyle(GetStyleUserInterface(), aCursor);  
  if (NS_STYLE_CURSOR_AUTO == aCursor.mCursor) {
    aCursor.mCursor = NS_STYLE_CURSOR_TEXT;

    // If tabindex >= 0, use default cursor to indicate it's not selectable
    nsIFrame *ancestorFrame = this;
    while ((ancestorFrame = ancestorFrame->GetParent()) != nsnull) {
      nsIContent *ancestorContent = ancestorFrame->GetContent();
      if (ancestorContent && ancestorContent->HasAttr(kNameSpaceID_None, nsGkAtoms::tabindex)) {
        nsAutoString tabIndexStr;
        ancestorContent->GetAttr(kNameSpaceID_None, nsGkAtoms::tabindex, tabIndexStr);
        if (!tabIndexStr.IsEmpty()) {
          PRInt32 rv, tabIndexVal = tabIndexStr.ToInteger(&rv);
          if (NS_SUCCEEDED(rv) && tabIndexVal >= 0) {
            aCursor.mCursor = NS_STYLE_CURSOR_DEFAULT;
            break;
          }
        }
      }
    }
  }

  return NS_OK;
}

nsIFrame*
nsTextFrame::GetLastInFlow() const
{
  nsTextFrame* lastInFlow = NS_CONST_CAST(nsTextFrame*, this);
  while (lastInFlow->GetNextInFlow())  {
    lastInFlow = NS_STATIC_CAST(nsTextFrame*, lastInFlow->GetNextInFlow());
  }
  NS_POSTCONDITION(lastInFlow, "illegal state in flow chain.");
  return lastInFlow;
}
nsIFrame*
nsTextFrame::GetLastContinuation() const
{
  nsTextFrame* lastInFlow = NS_CONST_CAST(nsTextFrame*, this);
  while (lastInFlow->mNextContinuation)  {
    lastInFlow = NS_STATIC_CAST(nsTextFrame*, lastInFlow->mNextContinuation);
  }
  NS_POSTCONDITION(lastInFlow, "illegal state in continuation chain.");
  return lastInFlow;
}

static void
ClearAllTextRunReferences(nsTextFrame* aFrame, gfxTextRun* aTextRun)
{
  aFrame->RemoveStateBits(TEXT_IS_RUN_OWNER);
  while (aFrame) {
    if (aFrame->GetTextRun() != aTextRun)
      break;
    aFrame->SetTextRun(nsnull);
    aFrame = NS_STATIC_CAST(nsTextFrame*, aFrame->GetNextContinuation());
  }
}

void
nsTextFrame::ClearTextRun()
{
  // save textrun because ClearAllTextRunReferences will clear ours
  gfxTextRun* textRun = mTextRun;
  
  if (!textRun || !(GetStateBits() & TEXT_IS_RUN_OWNER))
    return;

  // Kill all references to the textrun. It could be referenced by any of its
  // owners, and all their in-flows.
  if (textRun->GetFlags() & nsTextFrameUtils::TEXT_IS_SIMPLE_FLOW) {
    nsIFrame* firstInFlow = NS_STATIC_CAST(nsIFrame*, textRun->GetUserData());
    ClearAllTextRunReferences(NS_STATIC_CAST(nsTextFrame*, firstInFlow), textRun);
  } else {
    TextRunUserData* userData =
      NS_STATIC_CAST(TextRunUserData*, textRun->GetUserData());
    PRInt32 i;
    for (i = 0; i < userData->mMappedFlowCount; ++i) {
      ClearAllTextRunReferences(userData->mMappedFlows[i].mStartFrame, textRun);
    }
    DestroyUserData(userData);
  }
  delete textRun;
}

NS_IMETHODIMP
nsTextFrame::CharacterDataChanged(nsPresContext* aPresContext,
                                  nsIContent*     aChild,
                                  PRBool          aAppend)
{
  ClearTextRun();

  nsTextFrame* targetTextFrame;
  nsTextFrame* lastTextFrame;
  PRInt32 nodeLength = mContent->GetText()->GetLength();

  if (aAppend) {
    lastTextFrame = NS_STATIC_CAST(nsTextFrame*, GetLastContinuation());
    lastTextFrame->mState &= ~TEXT_WHITESPACE_FLAGS;
    lastTextFrame->mState |= NS_FRAME_IS_DIRTY;
    targetTextFrame = lastTextFrame;
  } else {
    // Mark this frame and all the continuation frames as dirty, and fix up
    // mContentLengths to be valid
    nsTextFrame* textFrame = this;
    PRInt32 newLength = nodeLength;
    do {
      textFrame->mState &= ~TEXT_WHITESPACE_FLAGS;
      textFrame->mState |= NS_FRAME_IS_DIRTY;
      // If the text node has shrunk, clip the frame contentlength as necessary
      textFrame->mContentLength = PR_MIN(mContentLength, newLength);
      newLength -= textFrame->mContentLength;
      lastTextFrame = textFrame;
      textFrame = NS_STATIC_CAST(nsTextFrame*, textFrame->GetNextContinuation());
    } while (textFrame);
    targetTextFrame = this;
  }
  // Set the length of the last text frame in the chain (necessary if the node grew)
  lastTextFrame->mContentLength = PR_MAX(0, nodeLength - lastTextFrame->mContentOffset);

  // Ask the parent frame to reflow me.
  aPresContext->GetPresShell()->FrameNeedsReflow(targetTextFrame,
                                                 nsIPresShell::eStyleChange);

  return NS_OK;
}

NS_IMETHODIMP
nsTextFrame::DidSetStyleContext()
{
  ClearTextRun();
  return NS_OK;
} 

class nsDisplayText : public nsDisplayItem {
public:
  nsDisplayText(nsTextFrame* aFrame) : nsDisplayItem(aFrame) {
    MOZ_COUNT_CTOR(nsDisplayText);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayText() {
    MOZ_COUNT_DTOR(nsDisplayText);
  }
#endif

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder) {
    return mFrame->GetOverflowRect() + aBuilder->ToReferenceFrame(mFrame);
  }
  virtual nsIFrame* HitTest(nsDisplayListBuilder* aBuilder, nsPoint aPt) { return mFrame; }
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx,
     const nsRect& aDirtyRect);
  NS_DISPLAY_DECL_NAME("Text")
};

void
nsDisplayText::Paint(nsDisplayListBuilder* aBuilder,
     nsIRenderingContext* aCtx, const nsRect& aDirtyRect) {
  NS_STATIC_CAST(nsTextFrame*, mFrame)->
    PaintText(aCtx, aBuilder->ToReferenceFrame(mFrame), aDirtyRect);
}

NS_IMETHODIMP
nsTextFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists)
{
  if (!IsVisibleForPainting(aBuilder))
    return NS_OK;
  
  DO_GLOBAL_REFLOW_COUNT_DSP("nsTextFrame");

  if ((0 != (mState & TEXT_BLINK_ON)) && nsBlinkTimer::GetBlinkIsOff())
    return NS_OK;
    
  return aLists.Content()->AppendNewToTop(new (aBuilder) nsDisplayText(this));
}

static nsIFrame*
GetGeneratedContentOwner(nsIFrame* aFrame, PRBool* aIsBefore)
{
  *aIsBefore = PR_FALSE;
  while (aFrame && (aFrame->GetStateBits() & NS_FRAME_GENERATED_CONTENT)) {
    if (aFrame->GetStyleContext()->GetPseudoType() == nsCSSPseudoElements::before) {
      *aIsBefore = PR_TRUE;
    }
    aFrame = aFrame->GetParent();
  }
  return aFrame;
}

SelectionDetails*
nsTextFrame::GetSelectionDetails()
{
  if (!(GetStateBits() & NS_FRAME_GENERATED_CONTENT)) {
    SelectionDetails* details =
      GetFrameSelection()->LookUpSelection(mContent, mContentOffset, 
                                           mContentLength, PR_FALSE);
    SelectionDetails* sd;
    for (sd = details; sd; sd = sd->mNext) {
      sd->mStart += mContentOffset;
      sd->mEnd += mContentOffset;
    }
    return details;
  }

  // Check if the beginning or end of the element is selected, depending on
  // whether we're :before content or :after content.
  PRBool isBefore;
  nsIFrame* owner = GetGeneratedContentOwner(this, &isBefore);
  if (!owner || !owner->GetContent())
    return nsnull;

  SelectionDetails* details =
    GetFrameSelection()->LookUpSelection(owner->GetContent(),
        isBefore ? 0 : owner->GetContent()->GetChildCount(), 0, PR_FALSE);
  SelectionDetails* sd;
  for (sd = details; sd; sd = sd->mNext) {
    // The entire text is selected!
    sd->mStart = mContentOffset;
    sd->mEnd = mContentOffset + mContentLength;
  }
  return details;
}

static void
FillClippedRect(gfxContext* aCtx, nsPresContext* aPresContext,
                nscolor aColor, const gfxRect& aDirtyRect, const gfxRect& aRect)
{
  gfxRect r = aRect.Intersect(aDirtyRect);
  // For now, we need to put this in pixel coordinates
  float t2p = 1.0/aPresContext->AppUnitsPerDevPixel();
  aCtx->NewPath();
  // pixel-snap
  aCtx->Rectangle(gfxRect(r.X()*t2p, r.Y()*t2p, r.Width()*t2p, r.Height()*t2p), PR_TRUE);
  aCtx->SetColor(gfxRGBA(aColor));
  aCtx->Fill();
}

void 
nsTextFrame::PaintTextDecorations(gfxContext* aCtx, const gfxRect& aDirtyRect,
                                  const gfxPoint& aFramePt,
                                  nsTextPaintStyle& aTextPaintStyle,
                                  PropertyProvider& aProvider)
{
  // Quirks mode text decoration are rendered by children; see bug 1777
  // In non-quirks mode, nsHTMLContainer::Paint and nsBlockFrame::Paint
  // does the painting of text decorations.
  if (eCompatibility_NavQuirks != aTextPaintStyle.GetPresContext()->CompatibilityMode())
    return;

  PRBool useOverride = PR_FALSE;
  nscolor overrideColor;

  PRUint8 decorations = NS_STYLE_TEXT_DECORATION_NONE;
  // A mask of all possible decorations.
  PRUint8 decorMask = NS_STYLE_TEXT_DECORATION_UNDERLINE | 
                      NS_STYLE_TEXT_DECORATION_OVERLINE |
                      NS_STYLE_TEXT_DECORATION_LINE_THROUGH;    
  nscolor overColor, underColor, strikeColor;
  nsStyleContext* context = GetStyleContext();
  PRBool hasDecorations = context->HasTextDecorations();

  while (hasDecorations) {
    const nsStyleTextReset* styleText = context->GetStyleTextReset();
    if (!useOverride && 
        (NS_STYLE_TEXT_DECORATION_OVERRIDE_ALL & styleText->mTextDecoration)) {
      // This handles the <a href="blah.html"><font color="green">La 
      // la la</font></a> case. The link underline should be green.
      useOverride = PR_TRUE;
      overrideColor = context->GetStyleColor()->mColor;          
    }

    PRUint8 useDecorations = decorMask & styleText->mTextDecoration;
    if (useDecorations) {// a decoration defined here
      nscolor color = context->GetStyleColor()->mColor;
  
      if (NS_STYLE_TEXT_DECORATION_UNDERLINE & useDecorations) {
        underColor = useOverride ? overrideColor : color;
        decorMask &= ~NS_STYLE_TEXT_DECORATION_UNDERLINE;
        decorations |= NS_STYLE_TEXT_DECORATION_UNDERLINE;
      }
      if (NS_STYLE_TEXT_DECORATION_OVERLINE & useDecorations) {
        overColor = useOverride ? overrideColor : color;
        decorMask &= ~NS_STYLE_TEXT_DECORATION_OVERLINE;
        decorations |= NS_STYLE_TEXT_DECORATION_OVERLINE;
      }
      if (NS_STYLE_TEXT_DECORATION_LINE_THROUGH & useDecorations) {
        strikeColor = useOverride ? overrideColor : color;
        decorMask &= ~NS_STYLE_TEXT_DECORATION_LINE_THROUGH;
        decorations |= NS_STYLE_TEXT_DECORATION_LINE_THROUGH;
      }
    }
    if (0 == decorMask)
      break;
    context = context->GetParent();
    if (!context)
      break;
    hasDecorations = context->HasTextDecorations();
  }

  if (!decorations)
    return;

  gfxFont::Metrics fontMetrics = GetFontMetrics(aProvider.GetFontGroup());
  gfxFloat pix2app = mTextRun->GetAppUnitsPerDevUnit();

  if (decorations & NS_FONT_DECORATION_OVERLINE) {
    FillClippedRect(aCtx, aTextPaintStyle.GetPresContext(), overColor, aDirtyRect,
                    gfxRect(aFramePt.x, aFramePt.y,
                            GetRect().width, fontMetrics.underlineSize*pix2app));
  }
  if (decorations & NS_FONT_DECORATION_UNDERLINE) {
    FillClippedRect(aCtx, aTextPaintStyle.GetPresContext(), underColor, aDirtyRect,
                    gfxRect(aFramePt.x,
                            aFramePt.y + mAscent - fontMetrics.underlineOffset,
                            GetRect().width, fontMetrics.underlineSize*pix2app));
  }
  if (decorations & NS_FONT_DECORATION_LINE_THROUGH) {
    FillClippedRect(aCtx, aTextPaintStyle.GetPresContext(), strikeColor, aDirtyRect,
                    gfxRect(aFramePt.x,
                            aFramePt.y + mAscent - fontMetrics.strikeoutOffset,
                            GetRect().width, fontMetrics.strikeoutSize*pix2app));
  }
}

// Make sure this stays in sync with DrawSelectionDecorations below
static const SelectionType SelectionTypesWithDecorations =
  nsISelectionController::SELECTION_SPELLCHECK |
  nsISelectionController::SELECTION_IME_RAWINPUT |
  nsISelectionController::SELECTION_IME_SELECTEDRAWTEXT |
  nsISelectionController::SELECTION_IME_CONVERTEDTEXT |
  nsISelectionController::SELECTION_IME_SELECTEDCONVERTEDTEXT;

static void DrawIMEUnderline(gfxContext* aContext, PRInt32 aIndex,
    nsTextPaintStyle& aTextPaintStyle, const gfxPoint& aBaselinePt, gfxFloat aWidth,
    const gfxRect& aDirtyRect, const gfxFont::Metrics& aFontMetrics)
{
  float p2t = aTextPaintStyle.GetPresContext()->AppUnitsPerDevPixel();
  nscolor color;
  float relativeSize;
  if (!aTextPaintStyle.GetIMEUnderline(aIndex, &color, &relativeSize))
    return;

  gfxFloat y = aBaselinePt.y - aFontMetrics.underlineOffset*p2t;
  gfxFloat size = aFontMetrics.underlineSize*p2t;
  FillClippedRect(aContext, aTextPaintStyle.GetPresContext(),
                  color, aDirtyRect,
                  gfxRect(aBaselinePt.x + size, y,
                          PR_MAX(0, aWidth - 2*size), relativeSize*size));
}

/**
 * This, plus SelectionTypesWithDecorations, encapsulates all knowledge about
 * drawing text decoration for selections.
 */
static void DrawSelectionDecorations(gfxContext* aContext, SelectionType aType,
    nsTextPaintStyle& aTextPaintStyle, const gfxPoint& aBaselinePt, gfxFloat aWidth,
    const gfxRect& aDirtyRect, const gfxFont::Metrics& aFontMetrics)
{
  float p2t = aTextPaintStyle.GetPresContext()->AppUnitsPerDevPixel();
  float t2p = 1/p2t;

  switch (aType) {
    case nsISelectionController::SELECTION_SPELLCHECK: {
      gfxFloat y = aBaselinePt.y*t2p - aFontMetrics.underlineOffset;
      aContext->SetDash(gfxContext::gfxLineDotted);
      aContext->SetColor(gfxRGBA(1.0, 0.0, 0.0));
      aContext->SetLineWidth(1.0);
      aContext->NewPath();
      aContext->Line(gfxPoint(aBaselinePt.x*t2p, y),
                     gfxPoint((aBaselinePt.x + aWidth)*t2p, y));
      aContext->Stroke();
      break;
    }

    case nsISelectionController::SELECTION_IME_RAWINPUT:
      DrawIMEUnderline(aContext, nsTextPaintStyle::eIndexRawInput, aTextPaintStyle,
                       aBaselinePt, aWidth, aDirtyRect, aFontMetrics);
      break;
    case nsISelectionController::SELECTION_IME_SELECTEDRAWTEXT:
      DrawIMEUnderline(aContext, nsTextPaintStyle::eIndexSelRawText, aTextPaintStyle,
                       aBaselinePt, aWidth, aDirtyRect, aFontMetrics);
      break;
    case nsISelectionController::SELECTION_IME_CONVERTEDTEXT:
      DrawIMEUnderline(aContext, nsTextPaintStyle::eIndexConvText, aTextPaintStyle,
                       aBaselinePt, aWidth, aDirtyRect, aFontMetrics);
      break;
    case nsISelectionController::SELECTION_IME_SELECTEDCONVERTEDTEXT:
      DrawIMEUnderline(aContext, nsTextPaintStyle::eIndexSelConvText, aTextPaintStyle,
                       aBaselinePt, aWidth, aDirtyRect, aFontMetrics);
      break;

    default:
      NS_WARNING("Requested selection decorations when there aren't any");
      break;
  }
}

/**
 * This function encapsulates all knowledge of how selections affect foreground
 * and background colors.
 * @return true if the selection affects colors, false otherwise
 * @param aForeground the foreground color to use
 * @param aBackground the background color to use, or RGBA(0,0,0,0) if no
 * background should be painted
 */
static PRBool GetSelectionTextColors(SelectionType aType, nsTextPaintStyle& aTextPaintStyle,
                                     nscolor* aForeground, nscolor* aBackground)
{
  switch (aType) {
    case nsISelectionController::SELECTION_NORMAL:
      return aTextPaintStyle.GetSelectionColors(aForeground, aBackground);

    case nsISelectionController::SELECTION_IME_RAWINPUT:
      aTextPaintStyle.GetIMESelectionColors(nsTextPaintStyle::eIndexRawInput,
                                            aForeground, aBackground);
      return PR_TRUE;
    case nsISelectionController::SELECTION_IME_SELECTEDRAWTEXT:
      aTextPaintStyle.GetIMESelectionColors(nsTextPaintStyle::eIndexSelRawText,
                                            aForeground, aBackground);
      return PR_TRUE;
    case nsISelectionController::SELECTION_IME_CONVERTEDTEXT:
      aTextPaintStyle.GetIMESelectionColors(nsTextPaintStyle::eIndexConvText,
                                            aForeground, aBackground);
      return PR_TRUE;
    case nsISelectionController::SELECTION_IME_SELECTEDCONVERTEDTEXT:
      aTextPaintStyle.GetIMESelectionColors(nsTextPaintStyle::eIndexSelConvText,
                                            aForeground, aBackground);
      return PR_TRUE;
      
    default:
      *aForeground = aTextPaintStyle.GetTextColor();
      *aBackground = NS_RGBA(0,0,0,0);
      return PR_FALSE;
  }
}

/**
 * This class lets us iterate over chunks of text in a uniform selection state,
 * observing cluster boundaries, in content order, maintaining the current
 * x-offset as we go, and telling whether the text chunk has a hyphen after
 * it or not. The caller is responsible for actually computing the advance
 * width of each chunk.
 */
class SelectionIterator {
public:
  /**
   * aStart and aLength are in the original string. aSelectionBuffer is
   * according to the original string.
   */
  SelectionIterator(SelectionType* aSelectionBuffer, PRInt32 aStart,
                    PRInt32 aLength, PropertyProvider& aProvider,
                    gfxTextRun* aTextRun);
  
  /**
   * Returns the next segment of uniformly selected (or not) text.
   * @param aXOffset the offset from the origin of the frame to the start
   * of the text (the left baseline origin for LTR, the right baseline origin
   * for RTL)
   * @param aOffset the transformed string offset of the text for this segment
   * @param aLength the transformed string length of the text for this segment
   * @param aHyphenWidth if a hyphen is to be rendered after the text, the
   * width of the hyphen, otherwise zero
   * @param aType the selection type for this segment
   * @return false if there are no more segments
   */
  PRBool GetNextSegment(gfxFloat* aXOffset, PRUint32* aOffset, PRUint32* aLength,
                        gfxFloat* aHyphenWidth, SelectionType* aType);
  void UpdateWithAdvance(gfxFloat aAdvance) {
    mXOffset += aAdvance*mTextRun->GetDirection();
  }

private:
  SelectionType*          mSelectionBuffer;
  PropertyProvider&       mProvider;
  gfxTextRun*             mTextRun;
  gfxSkipCharsIterator    mIterator;
  PRInt32                 mOriginalStart;
  PRInt32                 mOriginalEnd;
  gfxFloat                mXOffset;
};

SelectionIterator::SelectionIterator(SelectionType* aSelectionBuffer,
    PRInt32 aStart, PRInt32 aLength, PropertyProvider& aProvider,
    gfxTextRun* aTextRun)
  : mSelectionBuffer(aSelectionBuffer), mProvider(aProvider),
    mTextRun(aTextRun), mIterator(aProvider.GetStart()),
    mOriginalStart(aStart), mOriginalEnd(aStart + aLength),
    mXOffset(mTextRun->IsRightToLeft() ? aProvider.GetFrame()->GetSize().width : 0)
{
  mIterator.SetOriginalOffset(aStart);
}

PRBool SelectionIterator::GetNextSegment(gfxFloat* aXOffset,
    PRUint32* aOffset, PRUint32* aLength, gfxFloat* aHyphenWidth, SelectionType* aType)
{
  for (;;) {
    if (mIterator.GetOriginalOffset() >= mOriginalEnd)
      return PR_FALSE;
  
    // save offset into transformed string now
    PRUint32 runOffset = mIterator.GetSkippedOffset();
  
    PRInt32 index = mIterator.GetOriginalOffset() - mOriginalStart;
    SelectionType type = mSelectionBuffer[index];
    do {
      ++index;
      if (mSelectionBuffer[index] != type)
        break;
    } while (mOriginalStart + index < mOriginalEnd);
    mIterator.SetOriginalOffset(index + mOriginalStart);
  
    // Advance to the next cluster boundary
    while (mIterator.GetOriginalOffset() < mOriginalEnd &&
           !mIterator.IsOriginalCharSkipped() &&
           !mTextRun->IsClusterStart(mIterator.GetSkippedOffset())) {
      mIterator.AdvanceOriginal(1);
    }
  
    // Check whether we actually got some non-skipped text in this segment
    if (runOffset < mIterator.GetSkippedOffset()) {    
      *aOffset = runOffset;
      *aLength = mIterator.GetSkippedOffset() - runOffset;
      *aXOffset = mXOffset;
      *aHyphenWidth = 0;
      if (mIterator.GetOriginalOffset() == mOriginalEnd &&
          (mProvider.GetFrame()->GetStateBits() & TEXT_HYPHEN_BREAK)) {
        *aHyphenWidth = mProvider.GetHyphenWidth();
      }
      *aType = type;
      return PR_TRUE;
    }
  }
}

// Paints selection backgrounds and text in the correct colors. Also computes
// aAllTypes, the union of all selection types that are applying to this text.
void
nsTextFrame::PaintTextWithSelectionColors(gfxContext* aCtx,
    const gfxPoint& aFramePt,
    const gfxPoint& aTextBaselinePt, const gfxRect& aDirtyRect,
    PropertyProvider& aProvider, nsTextPaintStyle& aTextPaintStyle,
    SelectionDetails* aDetails, SelectionType* aAllTypes)
{
  PRInt32 contentOffset = aProvider.GetStart().GetOriginalOffset();
  PRInt32 contentLength = aProvider.GetOriginalLength();

  // Figure out which selections control the colors to use for each character.
  nsAutoTArray<SelectionType,BIG_TEXT_NODE_SIZE> prevailingSelectionsBuffer;
  if (!prevailingSelectionsBuffer.AppendElements(contentLength))
    return;
  SelectionType* prevailingSelections = prevailingSelectionsBuffer.Elements();
  PRInt32 i;
  SelectionType allTypes = 0;
  for (i = 0; i < contentLength; ++i) {
    prevailingSelections[i] = nsISelectionController::SELECTION_NONE;
  }

  SelectionDetails *sdptr = aDetails;
  PRBool anyBackgrounds = PR_FALSE;
  while (sdptr) {
    PRInt32 start = PR_MAX(0, sdptr->mStart - contentOffset);
    PRInt32 end = PR_MIN(contentLength, sdptr->mEnd - contentOffset);
    SelectionType type = sdptr->mType;
    if (start < end) {
      allTypes |= type;
      // Ignore selections that don't set colors
      nscolor foreground, background;
      if (GetSelectionTextColors(type, aTextPaintStyle, &foreground, &background)) {
        if (NS_GET_A(background) > 0) {
          anyBackgrounds = PR_TRUE;
        }
        for (i = start; i < end; ++i) {
          PRInt16 currentPrevailingSelection = prevailingSelections[i];
          // Favour normal selection over IME selections
          if (currentPrevailingSelection == nsISelectionController::SELECTION_NONE ||
              type < currentPrevailingSelection) {
            prevailingSelections[i] = type;
          }
        }
      }
    }
    sdptr = sdptr->mNext;
  }
  *aAllTypes = allTypes;

  gfxFloat xOffset, hyphenWidth;
  PRUint32 offset, length; // in transformed string
  SelectionType type;
  // Draw background colors
  if (anyBackgrounds) {
    SelectionIterator iterator(prevailingSelections, contentOffset, contentLength,
                               aProvider, mTextRun);
    while (iterator.GetNextSegment(&xOffset, &offset, &length, &hyphenWidth, &type)) {
      nscolor foreground, background;
      GetSelectionTextColors(type, aTextPaintStyle, &foreground, &background);
      // Draw background color
      gfxFloat advance = hyphenWidth +
        mTextRun->GetAdvanceWidth(offset, length, &aProvider);
      if (NS_GET_A(background) > 0) {
        gfxFloat x = xOffset - (mTextRun->IsRightToLeft() ? advance : 0);
        FillClippedRect(aCtx, aTextPaintStyle.GetPresContext(),
                        background, aDirtyRect,
                        gfxRect(aFramePt.x + x, aFramePt.y, advance, GetSize().height));
      }
      iterator.UpdateWithAdvance(advance);
    }
  }
  
  // Draw text
  SelectionIterator iterator(prevailingSelections, contentOffset, contentLength,
                             aProvider, mTextRun);
  while (iterator.GetNextSegment(&xOffset, &offset, &length, &hyphenWidth, &type)) {
    nscolor foreground, background;
    GetSelectionTextColors(type, aTextPaintStyle, &foreground, &background);
    // Draw text segment
    aCtx->SetColor(gfxRGBA(foreground));
    gfxFloat advance;
    mTextRun->Draw(aCtx, gfxPoint(aFramePt.x + xOffset, aTextBaselinePt.y), offset, length,
                   &aDirtyRect, &aProvider, &advance);
    if (hyphenWidth) {
      // Draw the hyphen
      gfxFloat hyphenBaselineX = aFramePt.x + xOffset + mTextRun->GetDirection()*advance;
      gfxTextRun* hyphenTextRun =
        GetSpecialString(aProvider.GetFontGroup(), gfxFontGroup::STRING_HYPHEN, mTextRun);
      if (hyphenTextRun) {
        hyphenTextRun->Draw(aCtx, gfxPoint(hyphenBaselineX, aTextBaselinePt.y),
                            0, hyphenTextRun->GetLength(), &aDirtyRect, nsnull, nsnull);
      }
      advance += hyphenWidth;
    }
    iterator.UpdateWithAdvance(advance);
  }
}

void
nsTextFrame::PaintTextSelectionDecorations(gfxContext* aCtx,
    const gfxPoint& aFramePt,
    const gfxPoint& aTextBaselinePt, const gfxRect& aDirtyRect,
    PropertyProvider& aProvider, nsTextPaintStyle& aTextPaintStyle,
    SelectionDetails* aDetails, SelectionType aSelectionType)
{
  PRInt32 contentOffset = aProvider.GetStart().GetOriginalOffset();
  PRInt32 contentLength = aProvider.GetOriginalLength();

  // Figure out which characters will be decorated for this selection. Here
  // we just fill the buffer with either SELECTION_NONE or aSelectionType.
  nsAutoTArray<SelectionType,BIG_TEXT_NODE_SIZE> selectedCharsBuffer;
  if (!selectedCharsBuffer.AppendElements(contentLength))
    return;
  SelectionType* selectedChars = selectedCharsBuffer.Elements();
  PRInt32 i;
  for (i = 0; i < contentLength; ++i) {
    selectedChars[i] = nsISelectionController::SELECTION_NONE;
  }

  SelectionDetails *sdptr = aDetails;
  while (sdptr) {
    if (sdptr->mType == aSelectionType) {
      PRInt32 start = PR_MAX(0, sdptr->mStart - contentOffset);
      PRInt32 end = PR_MIN(contentLength, sdptr->mEnd - contentOffset);
      for (i = start; i < end; ++i) {
        selectedChars[i] = aSelectionType;
      }
    }
    sdptr = sdptr->mNext;
  }

  gfxFont::Metrics decorationMetrics = GetFontMetrics(aProvider.GetFontGroup());

  SelectionIterator iterator(selectedChars, contentOffset, contentLength,
                             aProvider, mTextRun);
  gfxFloat xOffset, hyphenWidth;
  PRUint32 offset, length;
  SelectionType type;
  while (iterator.GetNextSegment(&xOffset, &offset, &length, &hyphenWidth, &type)) {
    gfxFloat advance = hyphenWidth +
      mTextRun->GetAdvanceWidth(offset, length, &aProvider);
    if (type == aSelectionType) {
      gfxFloat x = xOffset - (mTextRun->IsRightToLeft() ? advance : 0);
      DrawSelectionDecorations(aCtx, aSelectionType, aTextPaintStyle,
                               gfxPoint(x, aTextBaselinePt.y), advance,
                               aDirtyRect, decorationMetrics);
    }
    iterator.UpdateWithAdvance(advance);
  }
}

PRBool
nsTextFrame::PaintTextWithSelection(gfxContext* aCtx,
    const gfxPoint& aFramePt,
    const gfxPoint& aTextBaselinePt, const gfxRect& aDirtyRect,
    PropertyProvider& aProvider, nsTextPaintStyle& aTextPaintStyle)
{
  SelectionDetails* details = GetSelectionDetails();
  if (!details)
    return PR_FALSE;

  SelectionType allTypes;
  PaintTextWithSelectionColors(aCtx, aFramePt, aTextBaselinePt, aDirtyRect,
                               aProvider, aTextPaintStyle, details, &allTypes);
  PaintTextDecorations(aCtx, aDirtyRect, aFramePt, aTextPaintStyle, aProvider);
  PRInt32 i;
  // Iterate through just the selection types that paint decorations and
  // paint decorations for any that actually occur in this frame. Paint
  // higher-numbered selection types below lower-numered ones on the
  // general principal that lower-numbered selections are higher priority.
  allTypes &= SelectionTypesWithDecorations;
  for (i = nsISelectionController::NUM_SELECTIONTYPES - 1; i >= 1; --i) {
    SelectionType type = 1 << (i - 1);
    if (allTypes & type) {
      // There is some selection of this type. Try to paint its decorations
      // (there might not be any for this type but that's OK,
      // PaintTextSelectionDecorations will exit early).
      PaintTextSelectionDecorations(aCtx, aFramePt, aTextBaselinePt, aDirtyRect,
                                    aProvider, aTextPaintStyle, details, type);
    }
  }

  DestroySelectionDetails(details);
  return PR_TRUE;
}

static PRUint32
ComputeTransformedLength(PropertyProvider& aProvider)
{
  gfxSkipCharsIterator iter(aProvider.GetStart());
  PRUint32 start = iter.GetSkippedOffset();
  iter.AdvanceOriginal(aProvider.GetOriginalLength());
  return iter.GetSkippedOffset() - start;
}

void
nsTextFrame::PaintText(nsIRenderingContext* aRenderingContext, nsPoint aPt,
                       const nsRect& aDirtyRect)
{
  // XXX get the block and line passed to us somehow! This is slow!
  gfxSkipCharsIterator iter = EnsureTextRun(aRenderingContext);
  if (!mTextRun)
    return;

  nsTextPaintStyle textPaintStyle(this);
  PropertyProvider provider(this, iter);
  // Trim trailing whitespace
  provider.InitializeForDisplay(PR_TRUE);

  gfxContext* ctx = NS_STATIC_CAST(gfxContext*,
      aRenderingContext->GetNativeGraphicData(nsIRenderingContext::NATIVE_THEBES_CONTEXT));

  gfxPoint framePt(aPt.x, aPt.y);
  gfxPoint textBaselinePt(
      mTextRun->IsRightToLeft() ? gfxFloat(aPt.x + GetSize().width) : framePt.x,
      aPt.y + mAscent);

  gfxRect dirtyRect(aDirtyRect.x, aDirtyRect.y,
                    aDirtyRect.width, aDirtyRect.height);

  // Fork off to the (slower) paint-with-selection path if necessary.
  if (GetNonGeneratedAncestor(this)->GetStateBits() & NS_FRAME_SELECTED_CONTENT) {
    if (PaintTextWithSelection(ctx, framePt, textBaselinePt,
                               dirtyRect, provider, textPaintStyle))
      return;
  }

  gfxFloat advanceWidth;
  gfxFloat* needAdvanceWidth =
    (GetStateBits() & TEXT_HYPHEN_BREAK) ? &advanceWidth : nsnull;
  ctx->SetColor(gfxRGBA(textPaintStyle.GetTextColor()));
  
  mTextRun->Draw(ctx, textBaselinePt,
                 provider.GetStart().GetSkippedOffset(),
                 ComputeTransformedLength(provider),
                 &dirtyRect, &provider, needAdvanceWidth);
  if (GetStateBits() & TEXT_HYPHEN_BREAK) {
    gfxFloat hyphenBaselineX = textBaselinePt.x + mTextRun->GetDirection()*advanceWidth;
    gfxTextRun* hyphenTextRun =
      GetSpecialString(provider.GetFontGroup(), gfxFontGroup::STRING_HYPHEN, mTextRun);
    if (hyphenTextRun) {
      hyphenTextRun->Draw(ctx, gfxPoint(hyphenBaselineX, textBaselinePt.y),
                          0, hyphenTextRun->GetLength(), &dirtyRect, nsnull, nsnull);
    }
  }
  PaintTextDecorations(ctx, dirtyRect, framePt, textPaintStyle, provider);
}

PRInt16
nsTextFrame::GetSelectionStatus(PRInt16* aSelectionFlags)
{
  // get the selection controller
  nsCOMPtr<nsISelectionController> selectionController;
  nsresult rv = GetSelectionController(GetPresContext(),
                                       getter_AddRefs(selectionController));
  if (NS_FAILED(rv) || !selectionController)
    return nsISelectionController::SELECTION_OFF;

  selectionController->GetSelectionFlags(aSelectionFlags);

  PRInt16 selectionValue;
  selectionController->GetDisplaySelection(&selectionValue);

  return selectionValue;
}

PRBool
nsTextFrame::IsVisibleInSelection(nsISelection* aSelection)
{
  // Check the quick way first
  PRBool isSelected = (mState & NS_FRAME_SELECTED_CONTENT) == NS_FRAME_SELECTED_CONTENT;
  if (!isSelected)
    return PR_FALSE;
    
  SelectionDetails* details = GetSelectionDetails();
  PRBool found = PR_FALSE;
    
  // where are the selection points "really"
  SelectionDetails *sdptr = details;
  while (sdptr) {
    if (sdptr->mEnd > mContentOffset &&
        sdptr->mStart < mContentOffset + mContentLength &&
        sdptr->mType == nsISelectionController::SELECTION_NORMAL) {
      found = PR_TRUE;
      break;
    }
    sdptr = sdptr->mNext;
  }
  DestroySelectionDetails(details);

  return found;
}

static PRUint32
CountCharsFit(gfxTextRun* aTextRun, PRUint32 aStart, PRUint32 aLength,
              gfxFloat aWidth, PropertyProvider* aProvider,
              gfxFloat* aFitWidth)
{
  PRUint32 last = 0;
  gfxFloat totalWidth = 0;
  PRUint32 i;
  for (i = 1; i <= aLength; ++i) {
    if (i == aLength || aTextRun->IsClusterStart(aStart + i)) {
      gfxFloat lastWidth = totalWidth;
      totalWidth += aTextRun->GetAdvanceWidth(aStart + last, i - last, aProvider);
      if (totalWidth > aWidth) {
        *aFitWidth = lastWidth;
        return last;
      }
      last = i;
    }
  }
  *aFitWidth = 0;
  return 0;
}

nsIFrame::ContentOffsets
nsTextFrame::CalcContentOffsetsFromFramePoint(nsPoint aPoint) {
  ContentOffsets offsets;
  
  gfxSkipCharsIterator iter = EnsureTextRun();
  if (!mTextRun)
    return offsets;
  
  PropertyProvider provider(this, iter);
  // Trim leading but not trailing whitespace if possible
  provider.InitializeForDisplay(PR_FALSE);
  gfxFloat width = mTextRun->IsRightToLeft() ? mRect.width - aPoint.x : aPoint.x;
  gfxFloat fitWidth;
  PRUint32 skippedLength = ComputeTransformedLength(provider);

  PRUint32 charsFit = CountCharsFit(mTextRun,
      provider.GetStart().GetSkippedOffset(), skippedLength, width, &provider, &fitWidth);

  PRInt32 selectedOffset;
  if (charsFit < skippedLength) {
    // charsFit characters fitted, but no more could fit. See if we're
    // more than halfway through the cluster.. If we are, choose the next
    // cluster.
    gfxSkipCharsIterator extraCluster(provider.GetStart());
    extraCluster.AdvanceSkipped(charsFit);
    gfxSkipCharsIterator extraClusterLastChar(extraCluster);
    FindClusterEnd(mTextRun,
                   provider.GetStart().GetOriginalOffset() + provider.GetOriginalLength(),
                   &extraClusterLastChar);
    gfxFloat charWidth =
        mTextRun->GetAdvanceWidth(extraCluster.GetSkippedOffset(),
                                  GetSkippedDistance(extraCluster, extraClusterLastChar) + 1,
                                  &provider);
    selectedOffset = width <= fitWidth + charWidth/2
        ? extraCluster.GetOriginalOffset()
        : extraClusterLastChar.GetOriginalOffset() + 1;
  } else {
    // All characters fitted, we're at (or beyond) the end of the text.
    // XXX This could be some pathological situation where negative spacing
    // caused characters to move backwards. We can't really handle that
    // in the current frame system because frames can't have negative
    // intrinsic widths.
    selectedOffset =
        provider.GetStart().GetOriginalOffset() + provider.GetOriginalLength();
  }

  offsets.content = GetContent();
  offsets.offset = offsets.secondaryOffset = selectedOffset;
  offsets.associateWithNext = mContentOffset == offsets.offset;
  return offsets;
}

//null range means the whole thing
NS_IMETHODIMP
nsTextFrame::SetSelected(nsPresContext* aPresContext,
                         nsIDOMRange *aRange,
                         PRBool aSelected,
                         nsSpread aSpread)
{
  DEBUG_VERIFY_NOT_DIRTY(mState);
#if 0 //XXXrbs disable due to bug 310318
  if (mState & NS_FRAME_IS_DIRTY)
    return NS_ERROR_UNEXPECTED;
#endif

  if (aSelected && ParentDisablesSelection())
    return NS_OK;

  // check whether style allows selection
  PRBool selectable;
  IsSelectable(&selectable, nsnull);
  if (!selectable)
    return NS_OK;//do not continue no selection for this frame.

  PRBool found = PR_FALSE;
  if (aRange) {
    //lets see if the range contains us, if so we must redraw!
    nsCOMPtr<nsIDOMNode> endNode;
    PRInt32 endOffset;
    nsCOMPtr<nsIDOMNode> startNode;
    PRInt32 startOffset;
    aRange->GetEndContainer(getter_AddRefs(endNode));
    aRange->GetEndOffset(&endOffset);
    aRange->GetStartContainer(getter_AddRefs(startNode));
    aRange->GetStartOffset(&startOffset);
    nsCOMPtr<nsIDOMNode> thisNode = do_QueryInterface(GetContent());

    if (thisNode == startNode)
    {
      if ((mContentOffset + mContentLength) >= startOffset)
      {
        found = PR_TRUE;
        if (thisNode == endNode)
        { //special case
          if (endOffset == startOffset) //no need to redraw since drawing takes place with cursor
            found = PR_FALSE;

          if (mContentOffset > endOffset)
            found = PR_FALSE;
        }
      }
    }
    else if (thisNode == endNode)
    {
      if (mContentOffset < endOffset)
        found = PR_TRUE;
      else
      {
        found = PR_FALSE;
      }
    }
    else
    {
      found = PR_TRUE;
    }
  }
  else {
    // null range means the whole thing
    found = PR_TRUE;
  }

  if ( aSelected )
    AddStateBits(NS_FRAME_SELECTED_CONTENT);
  else
  { //we need to see if any other selection is available.
    SelectionDetails *details = GetSelectionDetails();
    if (!details) {
      RemoveStateBits(NS_FRAME_SELECTED_CONTENT);
    } else {
      DestroySelectionDetails(details);
    }
  }
  if (found) {
    // Selection might change anything. Invalidate the overflow area.
    Invalidate(GetOverflowRect(), PR_FALSE);
  }
  if (aSpread == eSpreadDown)
  {
    nsIFrame* frame = GetPrevContinuation();
    while(frame){
      frame->SetSelected(aPresContext, aRange,aSelected,eSpreadNone);
      frame = frame->GetPrevContinuation();
    }
    frame = GetNextContinuation();
    while (frame){
      frame->SetSelected(aPresContext, aRange,aSelected,eSpreadNone);
      frame = frame->GetNextContinuation();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsTextFrame::GetPointFromOffset(nsPresContext* aPresContext,
                                nsIRenderingContext* inRendContext,
                                PRInt32 inOffset,
                                nsPoint* outPoint)
{
  if (!aPresContext || !inRendContext || !outPoint)
    return NS_ERROR_NULL_POINTER;

  outPoint->x = 0;
  outPoint->y = 0;

  DEBUG_VERIFY_NOT_DIRTY(mState);
  if (mState & NS_FRAME_IS_DIRTY)
    return NS_ERROR_UNEXPECTED;

  if (mContentLength <= 0) {
    return NS_OK;
  }

  gfxSkipCharsIterator iter = EnsureTextRun();
  if (!mTextRun)
    return NS_ERROR_FAILURE;

  PropertyProvider properties(this, iter);
  // Don't trim trailing whitespace, we want the caret to appear in the right
  // place if it's positioned there
  properties.InitializeForDisplay(PR_FALSE);  

  if (inOffset < mContentOffset){
    NS_WARNING("offset before this frame's content");
    inOffset = mContentOffset;
  } else if (inOffset > mContentOffset + mContentLength) {
    NS_WARNING("offset after this frame's content");
    inOffset = mContentOffset + mContentLength;
  }
  PRInt32 trimmedOffset = properties.GetStart().GetOriginalOffset();
  PRInt32 trimmedEnd = trimmedOffset + properties.GetOriginalLength();
  inOffset = PR_MAX(inOffset, trimmedOffset);
  inOffset = PR_MIN(inOffset, trimmedEnd);

  iter.SetOriginalOffset(inOffset);

  if (inOffset < trimmedEnd &&
      !iter.IsOriginalCharSkipped() &&
      !mTextRun->IsClusterStart(iter.GetSkippedOffset())) {
    NS_WARNING("GetPointFromOffset called for non-cluster boundary");
    FindClusterStart(mTextRun, &iter);
  }

  gfxFloat advanceWidth =
    mTextRun->GetAdvanceWidth(properties.GetStart().GetSkippedOffset(),
                              GetSkippedDistance(properties.GetStart(), iter),
                              &properties);
  nscoord width = NSToCoordCeil(advanceWidth);

  if (mTextRun->IsRightToLeft()) {
    outPoint->x = mRect.width - width;
  } else {
    outPoint->x = width;
  }
  outPoint->y = 0;

  return NS_OK;
}

NS_IMETHODIMP
nsTextFrame::GetChildFrameContainingOffset(PRInt32 inContentOffset,
                                           PRBool  inHint,
                                           PRInt32* outFrameContentOffset,
                                           nsIFrame **outChildFrame)
{
  DEBUG_VERIFY_NOT_DIRTY(mState);
#if 0 //XXXrbs disable due to bug 310227
  if (mState & NS_FRAME_IS_DIRTY)
    return NS_ERROR_UNEXPECTED;
#endif

  if (nsnull == outChildFrame)
    return NS_ERROR_NULL_POINTER;
  PRInt32 contentOffset = inContentOffset;
  
  if (contentOffset != -1) //-1 signified the end of the current content
    contentOffset = inContentOffset - mContentOffset;

  if ((contentOffset > mContentLength) || ((contentOffset == mContentLength) && inHint) )
  {
    //this is not the frame we are looking for.
    nsIFrame* nextContinuation = GetNextContinuation();
    if (nextContinuation)
    {
      return nextContinuation->GetChildFrameContainingOffset(inContentOffset, inHint, outFrameContentOffset, outChildFrame);
    }
    else {
      if (contentOffset != mContentLength) //that condition was only for when there is a choice
        return NS_ERROR_FAILURE;
    }
  }

  if (inContentOffset < mContentOffset) //could happen with floats!
  {
    *outChildFrame = GetPrevInFlow();
    if (*outChildFrame)
      return (*outChildFrame)->GetChildFrameContainingOffset(inContentOffset, inHint,
        outFrameContentOffset,outChildFrame);
    else
      return NS_OK; //this can't be the right thing to do?
  }
  
  *outFrameContentOffset = contentOffset;
  *outChildFrame = this;
  return NS_OK;
}

PRBool
nsTextFrame::PeekOffsetNoAmount(PRBool aForward, PRInt32* aOffset)
{
  NS_ASSERTION(aOffset && *aOffset <= mContentLength, "aOffset out of range");

  gfxSkipCharsIterator iter = EnsureTextRun();
  if (!mTextRun)
    return PR_FALSE;

  TrimmedOffsets trimmed = GetTrimmedOffsets(mContent->GetText(), PR_TRUE);
  // Check whether there are nonskipped characters in the trimmmed range
  return iter.ConvertOriginalToSkipped(trimmed.mStart + trimmed.mLength) >
         iter.ConvertOriginalToSkipped(trimmed.mStart);
}

PRBool
nsTextFrame::PeekOffsetCharacter(PRBool aForward, PRInt32* aOffset)
{
  NS_ASSERTION(aOffset && *aOffset <= mContentLength, "aOffset out of range");

  PRBool selectable;
  PRUint8 selectStyle;  
  IsSelectable(&selectable, &selectStyle);
  if (selectStyle == NS_STYLE_USER_SELECT_ALL)
    return PR_FALSE;

  gfxSkipCharsIterator iter = EnsureTextRun();
  if (!mTextRun)
    return PR_FALSE;

  TrimmedOffsets trimmed = GetTrimmedOffsets(mContent->GetText(), PR_TRUE);

  // A negative offset means "end of frame".
  PRInt32 startOffset = mContentOffset + (*aOffset < 0 ? mContentLength : *aOffset);

  if (!aForward) {
    *aOffset = 0;
    PRInt32 i;
    for (i = PR_MIN(trimmed.mStart + trimmed.mLength, startOffset) - 1;
         i >= trimmed.mStart; --i) {
      iter.SetOriginalOffset(i);
      if (!iter.IsOriginalCharSkipped() &&
          mTextRun->IsClusterStart(iter.GetSkippedOffset())) {
        *aOffset = i - mContentOffset;
        return PR_TRUE;
      }
    }
  } else {
    *aOffset = mContentLength;
    PRInt32 i;
    // XXX there's something weird here about end-of-line. Need to test
    // caret movement through line endings. The old code could return mContentLength,
    // but we can't anymore...
    for (i = startOffset; i < trimmed.mStart + trimmed.mLength; ++i) {
      iter.SetOriginalOffset(i);
      if (!iter.IsOriginalCharSkipped() &&
          mTextRun->IsClusterStart(iter.GetSkippedOffset())) {
        *aOffset = i - mContentOffset;
        return PR_TRUE;
      }
    }
  }
  
  return PR_FALSE;
}

PRBool
nsTextFrame::PeekOffsetWord(PRBool aForward, PRBool aWordSelectEatSpace, PRBool aIsKeyboardSelect,
                            PRInt32* aOffset, PRBool* aSawBeforeType)
{
  NS_ASSERTION (aOffset && *aOffset <= mContentLength, "aOffset out of range");

  PRBool selectable;
  PRUint8 selectStyle;
  IsSelectable(&selectable, &selectStyle);
  if (selectStyle == NS_STYLE_USER_SELECT_ALL)
    return PR_FALSE;

  const nsTextFragment* frag = mContent->GetText();
  gfxSkipCharsIterator iter = EnsureTextRun();
  if (!mTextRun)
    return PR_FALSE;

  TrimmedOffsets trimmed = GetTrimmedOffsets(frag, PR_TRUE);

  // A negative offset means "end of frame".
  PRInt32 startOffset = mContentOffset + (*aOffset < 0 ? mContentLength : *aOffset);
  startOffset = PR_MIN(startOffset, trimmed.mStart + trimmed.mLength);

  // Find DOM offset of first nonskipped character
  PRInt32 offset = trimmed.mStart;
  PRInt32 length = trimmed.mLength;
  PRInt32 runLength;
  if (iter.IsOriginalCharSkipped(&runLength)) {
    offset += runLength;
    length -= runLength;
  }
  startOffset = PR_MAX(startOffset, offset);

  PRBool isWhitespace;
  PRBool stopAfterPunctuation = nsTextTransformer::GetWordSelectStopAtPunctuation();
  PRBool stopBeforePunctuation = stopAfterPunctuation && aIsKeyboardSelect;
  PRInt32 direction = aForward ? 1 : -1;
  *aOffset = aForward ? mContentLength : 0;
  if (startOffset + direction < offset ||
      startOffset + direction >= offset + length)
    return PR_FALSE;

  PRInt32 wordLen = nsTextFrameUtils::FindWordBoundary(frag,
      mTextRun, &iter, offset, length, startOffset + direction,
      direction, stopBeforePunctuation, stopAfterPunctuation, &isWhitespace);
  if (wordLen < 0)
    return PR_FALSE;

  if (aWordSelectEatSpace == isWhitespace || !*aSawBeforeType) {
    PRInt32 nextWordFirstChar = startOffset + direction*wordLen;
    *aOffset = (aForward ? nextWordFirstChar : nextWordFirstChar + 1) - mContentOffset;
    if (aWordSelectEatSpace == isWhitespace) {
      *aSawBeforeType = PR_TRUE;
    }

    for (;;) {
      if (nextWordFirstChar < offset || nextWordFirstChar >= offset + length)
        return PR_FALSE;
      wordLen = nsTextFrameUtils::FindWordBoundary(frag,
        mTextRun, &iter, offset, length, nextWordFirstChar,
        direction, stopBeforePunctuation, stopAfterPunctuation, &isWhitespace);
      if (wordLen < 0 ||
          (aWordSelectEatSpace ? !isWhitespace : *aSawBeforeType))
        break;
      nextWordFirstChar = nextWordFirstChar + direction*wordLen;
      *aOffset = (aForward ? nextWordFirstChar : nextWordFirstChar + 1) - mContentOffset;
      if (aWordSelectEatSpace == isWhitespace) {
        *aSawBeforeType = PR_TRUE;
      }
    }
  } else {
    *aOffset = aForward ? 0 : mContentLength;
  }
  return PR_TRUE;
}

 // TODO this needs to be deCOMtaminated with the interface fixed in
// nsIFrame.h, but we won't do that until the old textframe is gone.
NS_IMETHODIMP
nsTextFrame::CheckVisibility(nsPresContext* aContext, PRInt32 aStartIndex,
    PRInt32 aEndIndex, PRBool aRecurse, PRBool *aFinished, PRBool *aRetval)
{
  if (!aRetval)
    return NS_ERROR_NULL_POINTER;

  // Text in the range is visible if there is at least one character in the range
  // that is not skipped and is mapped by this frame (which is the primary frame)
  // or one of its continuations.
  for (nsTextFrame* f = this; f;
       f = NS_STATIC_CAST(nsTextFrame*, GetNextContinuation())) {
    if (f->PeekOffsetNoAmount(PR_TRUE, nsnull)) {
      *aRetval = PR_TRUE;
      return NS_OK;
    }
  }

  *aRetval = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsTextFrame::GetOffsets(PRInt32 &start, PRInt32 &end) const
{
  start = mContentOffset;
  end = mContentOffset+mContentLength;
  return NS_OK;
}

// returns PR_TRUE if this text frame completes the first-letter, PR_FALSE
// if it does not contain a true "letter"
// if returns PR_TRUE, then it also updates aLength to cover just the first-letter
// text
// XXX :first-letter should be handled during frame construction
// (and it has a good bit in common with nextBidi)
static PRBool
FindFirstLetterRange(const nsTextFragment* aFrag,
                     gfxTextRun* aTextRun,
                     PRInt32 aOffset, PRInt32* aLength)
{
  // Find first non-whitespace, non-punctuation cluster, and stop after it
  PRInt32 i;
  PRInt32 length = *aLength;
  for (i = 0; i < length; ++i) {
    if (!IsSpace(aFrag, aOffset + i) &&
        !nsTextFrameUtils::IsPunctuationMark(aFrag->CharAt(aOffset + i)))
      break;
  }

  if (i == length)
    return PR_FALSE;

  // Advance to the end of the cluster (when i+1 starts a new cluster)
  while (i + 1 < length) {
    if (aTextRun->IsClusterStart(aOffset + i + 1))
      break;
  }
  *aLength = i + 1;
  return PR_TRUE;
}

static void
AddCharToMetrics(gfxFloat aWidth, PropertyProvider* aProvider,
                 gfxFontGroup::SpecialString aSpecial, gfxTextRun* aTextRun,
                 gfxTextRun::Metrics* aMetrics, PRBool aTightBoundingBox)
{
  gfxRect charRect;
  if (aTightBoundingBox) {
    gfxTextRun* specialTextRun =
      GetSpecialString(aProvider->GetFontGroup(), aSpecial, aTextRun);
    gfxTextRun::Metrics charMetrics;
    if (specialTextRun) {
      charMetrics =
        specialTextRun->MeasureText(0, specialTextRun->GetLength(), PR_TRUE, nsnull);
    }
    charRect = charMetrics.mBoundingBox;
  } else {
    // assume char does not overflow font metrics!!!
    charRect = gfxRect(0, -aMetrics->mAscent, aWidth,
                       aMetrics->mAscent + aMetrics->mDescent);
  }
  if (aTextRun->IsRightToLeft()) {
    // Char comes before text, so the bounding box is moved to the
    // right by aWidth
    aMetrics->mBoundingBox.MoveBy(gfxPoint(aWidth, 0));
  } else {
    // char is moved to the right by mAdvanceWidth
    charRect.MoveBy(gfxPoint(aMetrics->mAdvanceWidth, 0));
  }
  aMetrics->mBoundingBox = aMetrics->mBoundingBox.Union(charRect);

  aMetrics->mAdvanceWidth += aWidth;
}

static nsRect ConvertGfxRectOutward(const gfxRect& aRect)
{
  nsRect r;
  r.x = NSToCoordFloor(aRect.X());
  r.y = NSToCoordFloor(aRect.Y());
  r.width = NSToCoordCeil(aRect.XMost()) - r.x;
  r.height = NSToCoordCeil(aRect.YMost()) - r.y;
  return r;
}

static PRUint32
FindStartAfterSkippingWhitespace(PropertyProvider* aProvider,
                                 nsIFrame::InlineIntrinsicWidthData* aData,
                                 PRBool aCollapseWhitespace,
                                 gfxSkipCharsIterator* aIterator,
                                 PRUint32 aFlowEndInTextRun)
{
  if (aData->skipWhitespace && aCollapseWhitespace) {
    while (aIterator->GetSkippedOffset() < aFlowEndInTextRun &&
           IsSpace(aProvider->GetFragment(), aIterator->GetOriginalOffset())) {
      aIterator->AdvanceSkipped(1);
    }
  }
  return aIterator->GetSkippedOffset();  
}

// XXX this doesn't handle characters shaped by line endings. We need to
// temporarily override the "current line ending" settings.
void
nsTextFrame::AddInlineMinWidthForFlow(nsIRenderingContext *aRenderingContext,
                                      nsIFrame::InlineMinWidthData *aData)
{
  PRUint32 flowEndInTextRun;
  gfxSkipCharsIterator iter =
    EnsureTextRun(aRenderingContext, nsnull, nsnull, &flowEndInTextRun);
  if (!mTextRun)
    return;

  PropertyProvider provider(mTextRun, GetStyleText(), mContent->GetText(), this,
                            iter, GetInFlowContentLength());

  PRBool collapseWhitespace = !provider.GetStyleText()->WhiteSpaceIsSignificant();
  PRUint32 start =
    FindStartAfterSkippingWhitespace(&provider, aData, collapseWhitespace,
                                     &iter, flowEndInTextRun);
  if (start >= flowEndInTextRun)
    return;

  if (mTextRun->CanBreakLineBefore(start)) {
    aData->Break(aRenderingContext);
  }

  PRUint32 i;
  PRUint32 wordStart = start;
  // XXX Should we consider hyphenation here?
  for (i = start + 1; i <= flowEndInTextRun; ++i) {
    if (i < flowEndInTextRun && !mTextRun->CanBreakLineBefore(i))
      continue;

    nscoord width =
      NSToCoordCeil(mTextRun->GetAdvanceWidth(wordStart, i - wordStart, &provider));
    aData->currentLine += width;

    if (collapseWhitespace) {
      nscoord trailingWhitespaceWidth;
      PRUint32 lengthAfterTrim =
        GetLengthOfTrimmedText(provider.GetFragment(), wordStart, i, &iter);
      if (lengthAfterTrim == 0) {
        trailingWhitespaceWidth = width;
      } else {
        PRUint32 trimStart = wordStart + lengthAfterTrim;
        trailingWhitespaceWidth =
          NSToCoordCeil(mTextRun->GetAdvanceWidth(trimStart, i - trimStart, &provider));
      }
      aData->trailingWhitespace += trailingWhitespaceWidth;
    } else {
      aData->trailingWhitespace = 0;
    }
    if (i < flowEndInTextRun) {
      aData->Break(aRenderingContext);
      wordStart = i;
    }
  }

  // Check if we have whitespace at the end
  aData->skipWhitespace =
    IsSpace(provider.GetFragment(),
            iter.ConvertSkippedToOriginal(flowEndInTextRun - 1));
}

// XXX Need to do something here to avoid incremental reflow bugs due to
// first-line and first-letter changing min-width
/* virtual */ void
nsTextFrame::AddInlineMinWidth(nsIRenderingContext *aRenderingContext,
                               nsIFrame::InlineMinWidthData *aData)
{
  AddInlineMinWidthForFlow(aRenderingContext, aData);
  if (mTextRun && !(mTextRun->GetFlags() & nsTextFrameUtils::TEXT_IS_SIMPLE_FLOW)) {
    // mTextRun did not cover all continuations of this frame.
    // Measure all additional flows associated with this frame's content.
    nsTextFrame* f = this;
    for (;;) {
      f = NS_STATIC_CAST(nsTextFrame*, f->GetNextContinuation());
      if (!f)
        break;
      if (f->GetStateBits() & TEXT_IS_RUN_OWNER) {
        f->AddInlineMinWidthForFlow(aRenderingContext, aData);
      }
    }
  }
}

// XXX this doesn't handle characters shaped by line endings. We need to
// temporarily override the "current line ending" settings.
void
nsTextFrame::AddInlinePrefWidthForFlow(nsIRenderingContext *aRenderingContext,
                                       nsIFrame::InlinePrefWidthData *aData)
{
  PRUint32 flowEndInTextRun;
  gfxSkipCharsIterator iter =
    EnsureTextRun(aRenderingContext, nsnull, nsnull, &flowEndInTextRun);
  if (!mTextRun)
    return;

  PropertyProvider provider(mTextRun, GetStyleText(), mContent->GetText(), this,
                            iter, GetInFlowContentLength());

  PRBool collapseWhitespace = !provider.GetStyleText()->WhiteSpaceIsSignificant();
  PRUint32 start =
    FindStartAfterSkippingWhitespace(&provider, aData, collapseWhitespace,
                                     &iter, flowEndInTextRun);
  if (start >= flowEndInTextRun)
    return;

  if (collapseWhitespace) {
    // \n line breaks are not honoured, so everything would like to go
    // onto one line, so just measure it
    PRUint32 lengthAfterTrim =
      GetLengthOfTrimmedText(provider.GetFragment(), start, flowEndInTextRun, &iter);
    aData->currentLine +=
      NSToCoordCeil(mTextRun->GetAdvanceWidth(start, flowEndInTextRun - start, &provider));

    PRUint32 trimStart = start + lengthAfterTrim;
    nscoord trimWidth =
      NSToCoordCeil(mTextRun->GetAdvanceWidth(trimStart, flowEndInTextRun - trimStart, &provider));
    if (lengthAfterTrim == 0) {
      // This is *all* trimmable whitespace, so whatever trailingWhitespace
      // we saw previously is still trailing...
      aData->trailingWhitespace += trimWidth;
    } else {
      // Some non-whitespace so the old trailingWhitespace is no longer trailing
      aData->trailingWhitespace = trimWidth;
    }
  } else {
    // We respect line breaks, so measure off each line (or part of line).
    PRInt32 end = mContentOffset + GetInFlowContentLength();
    PRUint32 startRun = start;
    aData->trailingWhitespace = 0;
    while (iter.GetOriginalOffset() < end) {
      if (provider.GetFragment()->CharAt(iter.GetOriginalOffset()) == '\n') {
        PRUint32 endRun = iter.GetSkippedOffset();
        aData->currentLine +=
          NSToCoordCeil(mTextRun->GetAdvanceWidth(startRun, endRun - startRun, &provider));
        aData->Break(aRenderingContext);
        startRun = endRun;
      }
      iter.AdvanceOriginal(1);
    }
    aData->currentLine +=
      NSToCoordCeil(mTextRun->GetAdvanceWidth(startRun, iter.GetSkippedOffset() - startRun, &provider));
  }

  // Check if we have whitespace at the end
  aData->skipWhitespace =
    IsSpace(provider.GetFragment(),
            iter.ConvertSkippedToOriginal(flowEndInTextRun - 1));
}

// XXX Need to do something here to avoid incremental reflow bugs due to
// first-line and first-letter changing pref-width
/* virtual */ void
nsTextFrame::AddInlinePrefWidth(nsIRenderingContext *aRenderingContext,
                                nsIFrame::InlinePrefWidthData *aData)
{
  AddInlinePrefWidthForFlow(aRenderingContext, aData);
  if (mTextRun && !(mTextRun->GetFlags() & nsTextFrameUtils::TEXT_IS_SIMPLE_FLOW)) {
    // mTextRun did not cover all continuations of this frame.
    // Measure all additional flows associated with this frame's content.
    nsTextFrame* f = this;
    for (;;) {
      f = NS_STATIC_CAST(nsTextFrame*, f->GetNextContinuation());
      if (!f)
        break;
      if (f->GetStateBits() & TEXT_IS_RUN_OWNER) {
        f->AddInlinePrefWidthForFlow(aRenderingContext, aData);
      }
    }
  }
}

/* virtual */ nsSize
nsTextFrame::ComputeSize(nsIRenderingContext *aRenderingContext,
                         nsSize aCBSize, nscoord aAvailableWidth,
                         nsSize aMargin, nsSize aBorder, nsSize aPadding,
                         PRBool aShrinkWrap)
{
  // Inlines and text don't compute size before reflow.
  return nsSize(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
}

NS_IMETHODIMP
nsTextFrame::Reflow(nsPresContext*           aPresContext,
                    nsHTMLReflowMetrics&     aMetrics,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsTextFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aMetrics, aStatus);
#ifdef NOISY_REFLOW
  ListTag(stdout);
  printf(": BeginReflow: availableSize=%d,%d\n",
         aReflowState.availableWidth, aReflowState.availableHeight);
#endif

  /////////////////////////////////////////////////////////////////////
  // Set up flags and clear out state
  /////////////////////////////////////////////////////////////////////

  // Clear out the reflow state flags in mState (without destroying
  // the TEXT_BLINK_ON bit). We also clear the whitespace flags because this
  // can change whether the frame maps whitespace-only text or not.
  RemoveStateBits(TEXT_REFLOW_FLAGS | TEXT_WHITESPACE_FLAGS);

  nsTextFrame* prevInFlow = NS_STATIC_CAST(nsTextFrame*, GetPrevInFlow());
  if (prevInFlow) {
    // Our mContentOffset may be out of date due to our prev-in-flow growing or
    // shrinking. Update it.
    mContentOffset = prevInFlow->GetContentOffset() + prevInFlow->GetContentLength();
  }

  // Temporarily map all possible content while we construct our new textrun.
  // so that when doing reflow our styles prevail over any part of the
  // textrun we look at. Note that next-in-flows may be mapping the same
  // content; gfxTextRun construction logic will ensure that we take priority.
  PRInt32 maxContentLength = GetInFlowContentLength();
  mContentLength = maxContentLength;

  // XXX If there's no line layout, we shouldn't even have created this
  // frame. This may happen if, for example, this is text inside a table
  // but not inside a cell. For now, just don't reflow. We also don't need to
  // reflow if there is no content.
  if (!aReflowState.mLineLayout || !mContentLength) {
    ClearMetrics(aMetrics);
    aStatus = NS_FRAME_COMPLETE;
    return NS_OK;
  }

  nsLineLayout& lineLayout = *aReflowState.mLineLayout;

  if (aPresContext->BidiEnabled()) {
    // SetIsBidiSystem should go away at some point since we're going to require
    // it to be effectively always true
    aPresContext->SetIsBidiSystem(PR_TRUE);
  }

  if (aReflowState.mFlags.mBlinks) {
    if (0 == (mState & TEXT_BLINK_ON)) {
      mState |= TEXT_BLINK_ON;
      nsBlinkTimer::AddBlinkFrame(aPresContext, this);
    }
  }
  else {
    if (0 != (mState & TEXT_BLINK_ON)) {
      mState &= ~TEXT_BLINK_ON;
      nsBlinkTimer::RemoveBlinkFrame(this);
    }
  }

  const nsStyleText* textStyle = GetStyleText();

  PRBool atStartOfLine = lineLayout.CanPlaceFloatNow();
  if (atStartOfLine) {
    AddStateBits(TEXT_START_OF_LINE);
  }

  PRInt32 column = lineLayout.GetColumn();
  mColumn = column;

  // Layout dependent styles are a problem because we need to reconstruct
  // the gfxTextRun based on our layout.
  PRBool layoutDependentTextRun =
    lineLayout.GetFirstLetterStyleOK() || lineLayout.GetInFirstLine();
  if (layoutDependentTextRun) {
    // Nuke any text run since it may not be valid for our reflow
    ClearTextRun();
  }

  PRUint32 flowEndInTextRun;
  nsIFrame* lineContainer = lineLayout.GetLineContainerFrame();
  // Sometimes the line layout's container isn't really a block (hello
  // floating first-letter!!)
  nsBlockFrame* blockContainer = nsnull;
  lineContainer->QueryInterface(kBlockFrameCID, (void**)&blockContainer);
  gfxSkipCharsIterator iter =
    EnsureTextRun(aReflowState.rendContext, blockContainer,
                  lineLayout.GetLine(), &flowEndInTextRun);

  if (!mTextRun) {
    ClearMetrics(aMetrics);
    aStatus = NS_FRAME_COMPLETE;
    return NS_OK;
  }

  const nsTextFragment* frag = mContent->GetText();
  // DOM offsets of the text range we need to measure, after trimming
  // whitespace, restricting to first-letter, and restricting preformatted text
  // to nearest newline
  PRInt32 length = mContentLength;
  PRInt32 offset = mContentOffset;

  // Restrict preformatted text to the nearest newline
  PRInt32 newLineOffset = -1;
  if (textStyle->WhiteSpaceIsSignificant()) {
    newLineOffset = FindChar(frag, offset, length, '\n');
    if (newLineOffset >= 0) {
      length = newLineOffset + 1 - offset;
    }
  } else {
    if (atStartOfLine) {
      // Skip leading whitespace
      PRInt32 whitespaceCount = GetWhitespaceCount(frag, offset, length, 1);
      offset += whitespaceCount;
      length -= whitespaceCount;
    }
  }

  // Restrict to just the first-letter if necessary
  PRBool completedFirstLetter = PR_FALSE;
  if (lineLayout.GetFirstLetterStyleOK()) {
    AddStateBits(TEXT_FIRST_LETTER);
    completedFirstLetter = FindFirstLetterRange(frag, mTextRun, offset, &length);
  }

  /////////////////////////////////////////////////////////////////////
  // See how much text should belong to this text frame, and measure it
  /////////////////////////////////////////////////////////////////////
  
  iter.SetOriginalOffset(offset);
  PropertyProvider provider(mTextRun, textStyle, frag, this, iter, length);

  PRUint32 transformedOffset = provider.GetStart().GetSkippedOffset();

  // The metrics for the text go in here
  gfxTextRun::Metrics textMetrics;
  PRBool needTightBoundingBox = (GetStateBits() & TEXT_FIRST_LETTER) != 0;
#ifdef MOZ_MATHML
  if (NS_REFLOW_CALC_BOUNDING_METRICS & aMetrics.mFlags) {
    needTightBoundingBox = PR_TRUE;
  }
#endif
  // The "end" iterator points to the first character after the string mapped
  // by this frame. Basically, it's original-string offset is offset+charsFit
  // after we've computed charsFit.
  gfxSkipCharsIterator end(provider.GetStart());

  PRBool suppressInitialBreak = PR_FALSE;
  if (!lineLayout.LineIsBreakable()) {
    suppressInitialBreak = PR_TRUE;
  } else {
    PRBool trailingTextFrameCanWrap;
    nsIFrame* lastTextFrame = lineLayout.GetTrailingTextFrame(&trailingTextFrameCanWrap);
    if (!lastTextFrame) {
      suppressInitialBreak = PR_TRUE;
    }
  }

  PRInt32 limitLength = length;
  PRInt32 forceBreak = lineLayout.GetForcedBreakPosition(mContent);
  if (forceBreak >= 0) {
    limitLength = forceBreak - offset;
    NS_ASSERTION(limitLength >= 0, "Weird break found!");
  }
  // This is the heart of text reflow right here! We don't know where
  // to break, so we need to see how much text fits in the available width.
  PRUint32 transformedLength;
  if (offset + limitLength >= PRInt32(frag->GetLength())) {
    NS_ASSERTION(offset + limitLength == PRInt32(frag->GetLength()),
                 "Content offset/length out of bounds");
    NS_ASSERTION(flowEndInTextRun >= transformedOffset,
                 "Negative flow length?");
    transformedLength = flowEndInTextRun - transformedOffset;
  } else {
    // we're not looking at all the content, so we need to compute the
    // length of the transformed substring we're looking at
    gfxSkipCharsIterator iter(provider.GetStart());
    iter.SetOriginalOffset(offset + limitLength);
    transformedLength = iter.GetSkippedOffset() - transformedOffset;
  }
  PRUint32 transformedLastBreak = 0;
  PRBool usedHyphenation;
  PRUint32 transformedCharsFit =
    mTextRun->BreakAndMeasureText(transformedOffset, transformedLength,
                                  (GetStateBits() & TEXT_START_OF_LINE) != 0,
                                  aReflowState.availableWidth,
                                  &provider, suppressInitialBreak,
                                  &textMetrics, needTightBoundingBox,
                                  &usedHyphenation, &transformedLastBreak);
  end.SetSkippedOffset(transformedOffset + transformedCharsFit);
  PRInt32 charsFit = end.GetOriginalOffset() - offset;
  // That might have taken us beyond our assigned content range, so get back
  // in.
  PRInt32 lastBreak = -1;
  if (charsFit >= limitLength) {
    charsFit = limitLength;
    if (transformedLastBreak != PR_UINT32_MAX) {
      // lastBreak is needed. Use the "end" iterator for this because
      // it's likely to be close to the desired point
      end.SetSkippedOffset(transformedOffset + transformedLastBreak);
      // This may set lastBreak greater than 'length', but that's OK
      lastBreak = end.GetOriginalOffset();
      // Reset 'end' to the correct offset.
      end.SetOriginalOffset(offset + charsFit);
    }
  }
  if (usedHyphenation) {
    // Fix up metrics to include hyphen
    AddCharToMetrics(provider.GetHyphenWidth(), &provider, gfxFontGroup::STRING_HYPHEN,
                     mTextRun, &textMetrics, needTightBoundingBox);
    AddStateBits(TEXT_HYPHEN_BREAK);
  }

  // If it's only whitespace that didn't fit, then we will include
  // the whitespace in this frame, but we will eagerly trim all trailing
  // whitespace from our width calculations. Basically we do
  // TrimTrailingWhitespace early so that line layout sees that we fit on
  // the line.
  if (charsFit < length) {
    PRInt32 whitespaceCount =
      GetWhitespaceCount(frag, offset + charsFit, length - charsFit, 1);
    if (whitespaceCount > 0) {
      charsFit += whitespaceCount;

      // Now trim all trailing whitespace from our width, including possibly
      // even whitespace that fitted (which could only happen with
      // white-space:pre-wrap, because when whitespace collapsing is in effect
      // there can only be one whitespace character rendered at the end of
      // the frame)
      AddStateBits(TEXT_TRIMMED_TRAILING_WHITESPACE);
      PRUint32 currentEnd = end.GetSkippedOffset();
      PRUint32 trimmedEnd = transformedOffset +
        GetLengthOfTrimmedText(frag, transformedOffset, currentEnd, &end);
      textMetrics.mAdvanceWidth -=
        mTextRun->GetAdvanceWidth(trimmedEnd,
                                  currentEnd - trimmedEnd, &provider);
      // XXX We don't adjust the bounding box in textMetrics. But we should! Do
      // we need to remeasure the text? Maybe the metrics returned by the textrun
      // should have a way of saying "no glyphs outside their font-boxes" so
      // we know we don't need to adjust the bounding box here and elsewhere...
      end.SetOriginalOffset(offset + charsFit);
    }
  } else {
    // All text fit. Record the last potential break, if there is one.
    if (lastBreak >= 0) {
      lineLayout.NotifyOptionalBreakPosition(mContent, lastBreak,
          textMetrics.mAdvanceWidth < aReflowState.availableWidth);
    }
  }
  mContentLength = offset + charsFit - mContentOffset;

  /////////////////////////////////////////////////////////////////////
  // Compute output metrics
  /////////////////////////////////////////////////////////////////////

  // first-letter frames should use the tight bounding box metrics for ascent/descent
  // for good drop-cap effects
  if (GetStateBits() & TEXT_FIRST_LETTER) {
    textMetrics.mAscent = PR_MAX(0, -textMetrics.mBoundingBox.Y());
    textMetrics.mDescent = PR_MAX(0, textMetrics.mBoundingBox.YMost());
    textMetrics.mAdvanceWidth = textMetrics.mBoundingBox.XMost();
  }
  
  // Setup metrics for caller
  // Disallow negative widths
  aMetrics.width = NSToCoordCeil(PR_MAX(0, textMetrics.mAdvanceWidth));
  aMetrics.ascent = NSToCoordCeil(textMetrics.mAscent);
  aMetrics.height = aMetrics.ascent + NSToCoordCeil(textMetrics.mDescent);
  NS_ASSERTION(aMetrics.ascent >= 0, "Negative ascent???");
  NS_ASSERTION(aMetrics.height - aMetrics.ascent >= 0, "Negative descent???");

  mAscent = aMetrics.ascent;

  // Handle text that runs outside its normal bounds.
  nsRect boundingBox =
    ConvertGfxRectOutward(textMetrics.mBoundingBox + gfxPoint(0, textMetrics.mAscent));
  aMetrics.mOverflowArea.UnionRect(boundingBox,
                                   nsRect(0, 0, aMetrics.width, aMetrics.height));

#ifdef MOZ_MATHML
  // Store MathML bounding metrics. We've already calculated them above.
  if (needTightBoundingBox) {
    aMetrics.mBoundingMetrics.ascent =
      NSToCoordCeil(PR_MAX(0, -textMetrics.mBoundingBox.Y()));
    aMetrics.mBoundingMetrics.descent =
      NSToCoordCeil(PR_MAX(0, textMetrics.mBoundingBox.YMost()));
    aMetrics.mBoundingMetrics.leftBearing =
      NSToCoordFloor(textMetrics.mBoundingBox.X());
    aMetrics.mBoundingMetrics.rightBearing =
      NSToCoordCeil(textMetrics.mBoundingBox.XMost());
    aMetrics.mBoundingMetrics.width = aMetrics.width;
  }
#endif

  /////////////////////////////////////////////////////////////////////
  // Clean up, update state
  /////////////////////////////////////////////////////////////////////

  // Update lineLayout state  
  column += textMetrics.mClusterCount +
      provider.GetTabExpansionCount(provider.GetStart().GetSkippedOffset(),
                                    GetSkippedDistance(provider.GetStart(), end));
  lineLayout.SetColumn(column);
  lineLayout.SetUnderstandsWhiteSpace(PR_TRUE);
  PRBool endsInWhitespace = PR_FALSE;
  if (charsFit > 0) {
    endsInWhitespace = IsSpace(frag, offset + charsFit - 1);
    lineLayout.SetInWord(!endsInWhitespace);
    lineLayout.SetEndsInWhiteSpace(endsInWhitespace);
    PRBool wrapping = textStyle->WhiteSpaceCanWrap();
    lineLayout.SetTrailingTextFrame(this, wrapping);
    if (charsFit == length && endsInWhitespace && wrapping) {
      // Record a potential break after final breakable whitespace
      lineLayout.NotifyOptionalBreakPosition(mContent, offset + length, PR_TRUE);
    }
  } else {
    // Don't allow subsequent text frame to break-before. All our text is       
    // being skipped (usually whitespace, could be discarded Unicode control    
    // characters).
    lineLayout.SetEndsInWhiteSpace(PR_FALSE);
    lineLayout.SetTrailingTextFrame(nsnull, PR_FALSE);
  }
  if (completedFirstLetter) {
    lineLayout.SetFirstLetterStyleOK(PR_FALSE);
  }

  // Compute reflow status
  aStatus = mContentLength == maxContentLength
    ? NS_FRAME_COMPLETE : NS_FRAME_NOT_COMPLETE;

  if (charsFit == 0 && length > 0) {
    // Couldn't place any text
    aStatus = NS_INLINE_LINE_BREAK_BEFORE();
  } else if (mContentLength > 0 && mContentLength - 1 == newLineOffset) {
    // Ends in \n
    aStatus = NS_INLINE_LINE_BREAK_AFTER(aStatus);
    lineLayout.SetLineEndsInBR(PR_TRUE);
  }

  // Compute space and letter counts for justification, if required
  if (NS_STYLE_TEXT_ALIGN_JUSTIFY == textStyle->mTextAlign &&
      !textStyle->WhiteSpaceIsSignificant()) {
    // This will include a space for trailing whitespace, if any is present.
    // This is corrected for in nsLineLayout::TrimWhiteSpaceIn.
    PRInt32 numJustifiableCharacters =
      provider.ComputeJustifiableCharacters(offset, charsFit);
    NS_ASSERTION(numJustifiableCharacters <= textMetrics.mClusterCount,
                 "Justifiable characters combined???");
    lineLayout.SetTextJustificationWeights(numJustifiableCharacters,
        textMetrics.mClusterCount - numJustifiableCharacters);
  }

  if (layoutDependentTextRun) {
    // Nuke any text run since it may not be valid now that we have reflowed
    ClearTextRun();
  }

  Invalidate(nsRect(nsPoint(0, 0), GetSize()));

#ifdef NOISY_REFLOW
  ListTag(stdout);
  printf(": desiredSize=%d,%d(b=%d) status=%x\n",
         aMetrics.width, aMetrics.height, aMetrics.ascent,
         aStatus);
#endif
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aMetrics);
  return NS_OK;
}

/* virtual */ PRBool
nsTextFrame::CanContinueTextRun() const
{
  // We can continue a text run through a text frame
  return PR_TRUE;
}

NS_IMETHODIMP
nsTextFrame::TrimTrailingWhiteSpace(nsPresContext* aPresContext,
                                    nsIRenderingContext& aRC,
                                    nscoord& aDeltaWidth,
                                    PRBool& aLastCharIsJustifiable)
{
  aLastCharIsJustifiable = PR_FALSE;
  aDeltaWidth = 0;

  AddStateBits(TEXT_END_OF_LINE);

  if (!mContentLength)
    return NS_OK;

  gfxSkipCharsIterator iter = EnsureTextRun(&aRC);
  if (!mTextRun)
    return NS_ERROR_FAILURE;
  PRUint32 trimmedStart = iter.GetSkippedOffset();

  const nsTextFragment* frag = mContent->GetText();
  TrimmedOffsets trimmed = GetTrimmedOffsets(frag, PR_TRUE);
  PRUint32 trimmedEnd = iter.ConvertOriginalToSkipped(trimmed.mStart + trimmed.mLength);
  const nsStyleText* textStyle = GetStyleText();
  gfxFloat delta = 0;
  PropertyProvider provider(mTextRun, textStyle, frag, this, iter, mContentLength);

  if (GetStateBits() & TEXT_TRIMMED_TRAILING_WHITESPACE) {
    aLastCharIsJustifiable = PR_TRUE;
  } else if (trimmed.mStart + trimmed.mLength < mContentOffset + mContentLength) {
    gfxSkipCharsIterator end = iter;
    PRUint32 endOffset = end.ConvertOriginalToSkipped(mContentOffset + mContentLength);
    if (trimmedEnd < endOffset) {
      delta = mTextRun->GetAdvanceWidth(trimmedEnd, endOffset - trimmedEnd, &provider);
      // non-compressed whitespace being skipped at end of line -> justifiable
      // XXX should we actually *count* justifiable characters that should be
      // removed from the overall count? I think so...
      aLastCharIsJustifiable = PR_TRUE;
    }
  }

  if (!aLastCharIsJustifiable &&
      NS_STYLE_TEXT_ALIGN_JUSTIFY == textStyle->mTextAlign) {
    // Check if any character in the last cluster is justifiable
    PRBool isCJK = IsChineseJapaneseLangGroup(this);
    gfxSkipCharsIterator justificationEnd(iter);
    provider.FindEndOfJustificationRange(&justificationEnd);

    PRInt32 i;
    for (i = justificationEnd.GetOriginalOffset(); i < trimmed.mStart + trimmed.mLength; ++i) {
      if (IsJustifiableCharacter(frag, i, isCJK)) {
        aLastCharIsJustifiable = PR_TRUE;
      }
    }
  }

  gfxFloat advanceDelta;
  mTextRun->SetLineBreaks(trimmedStart, trimmedEnd - trimmedStart,
                          (GetStateBits() & TEXT_START_OF_LINE) != 0, PR_TRUE,
                          &provider, &advanceDelta);

  // aDeltaWidth is *subtracted* from our width.
  // If advanceDelta is positive then setting the line break made us longer,
  // so aDeltaWidth could go negative.
  aDeltaWidth = NSToCoordFloor(delta - advanceDelta);
  // XXX if aDeltaWidth goes negative, that means this frame might not actually fit
  // anymore!!! We need higher level line layout to recover somehow. This can
  // really only happen when we have glyphs with special shapes at the end of
  // lines, I think. Breaking inside a kerning pair won't do it because that
  // would mean we broke inside this textrun, and BreakAndMeasureText should
  // make sure the resulting shaped substring fits. Maybe if we passed a
  // maxTextLength? But that only happens at direction changes (so we
  // we wouldn't kern across the boundary) or for first-letter (which always fits
  // because it starts the line!).
  if (aDeltaWidth < 0) {
    NS_WARNING("Negative deltawidth, something odd is happening");
  }

  // XXX what about adjusting bounding metrics?

#ifdef NOISY_TRIM
  ListTag(stdout);
  printf(": trim => %d\n", aDeltaWidth);
#endif
  return NS_OK;
}

#ifdef DEBUG
// Translate the mapped content into a string that's printable
void
nsTextFrame::ToCString(nsString& aBuf, PRInt32* aTotalContentLength) const
{
  // Get the frames text content
  const nsTextFragment* frag = mContent->GetText();
  if (!frag) {
    return;
  }

  // Compute the total length of the text content.
  *aTotalContentLength = frag->GetLength();

  // Set current fragment and current fragment offset
  if (0 == mContentLength) {
    return;
  }
  PRInt32 fragOffset = mContentOffset;
  PRInt32 n = fragOffset + mContentLength;
  while (fragOffset < n) {
    PRUnichar ch = frag->CharAt(fragOffset++);
    if (ch == '\r') {
      aBuf.AppendLiteral("\\r");
    } else if (ch == '\n') {
      aBuf.AppendLiteral("\\n");
    } else if (ch == '\t') {
      aBuf.AppendLiteral("\\t");
    } else if ((ch < ' ') || (ch >= 127)) {
      aBuf.AppendLiteral("\\0");
      aBuf.AppendInt((PRInt32)ch, 8);
    } else {
      aBuf.Append(ch);
    }
  }
}
#endif

nsIAtom*
nsTextFrame::GetType() const
{
  return nsGkAtoms::textFrame;
} 

/* virtual */ PRBool
nsTextFrame::IsEmpty()
{
  NS_ASSERTION(!(mState & TEXT_IS_ONLY_WHITESPACE) ||
               !(mState & TEXT_ISNOT_ONLY_WHITESPACE),
               "Invalid state");
  
  // XXXldb Should this check compatibility mode as well???
  if (GetStyleText()->WhiteSpaceIsSignificant()) {
    return PR_FALSE;
  }

  if (mState & TEXT_ISNOT_ONLY_WHITESPACE) {
    return PR_FALSE;
  }

  if (mState & TEXT_IS_ONLY_WHITESPACE) {
    return PR_TRUE;
  }
  
  PRBool isEmpty = mContent->TextIsOnlyWhitespace();
  mState |= (isEmpty ? TEXT_IS_ONLY_WHITESPACE : TEXT_ISNOT_ONLY_WHITESPACE);
  return isEmpty;
}

#ifdef DEBUG
NS_IMETHODIMP
nsTextFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Text"), aResult);
}

NS_IMETHODIMP_(nsFrameState)
nsTextFrame::GetDebugStateBits() const
{
  // mask out our emptystate flags; those are just caches
  return nsFrame::GetDebugStateBits() &
    ~(TEXT_WHITESPACE_FLAGS | TEXT_REFLOW_FLAGS);
}

NS_IMETHODIMP
nsTextFrame::List(FILE* out, PRInt32 aIndent) const
{
  // Output the tag
  IndentBy(out, aIndent);
  ListTag(out);
#ifdef DEBUG_waterson
  fprintf(out, " [parent=%p]", mParent);
#endif
  if (HasView()) {
    fprintf(out, " [view=%p]", NS_STATIC_CAST(void*, GetView()));
  }

  PRInt32 totalContentLength;
  nsAutoString tmp;
  ToCString(tmp, &totalContentLength);

  // Output the first/last content offset and prev/next in flow info
  PRBool isComplete = (mContentOffset + mContentLength) == totalContentLength;
  fprintf(out, "[%d,%d,%c] ", 
          mContentOffset, mContentLength,
          isComplete ? 'T':'F');
  
  if (nsnull != mNextSibling) {
    fprintf(out, " next=%p", NS_STATIC_CAST(void*, mNextSibling));
  }
  nsIFrame* prevContinuation = GetPrevContinuation();
  if (nsnull != prevContinuation) {
    fprintf(out, " prev-continuation=%p", NS_STATIC_CAST(void*, prevContinuation));
  }
  if (nsnull != mNextContinuation) {
    fprintf(out, " next-continuation=%p", NS_STATIC_CAST(void*, mNextContinuation));
  }

  // Output the rect and state
  fprintf(out, " {%d,%d,%d,%d}", mRect.x, mRect.y, mRect.width, mRect.height);
  if (0 != mState) {
    if (mState & NS_FRAME_SELECTED_CONTENT) {
      fprintf(out, " [state=%08x] SELECTED", mState);
    } else {
      fprintf(out, " [state=%08x]", mState);
    }
  }
  fprintf(out, " [content=%p]", NS_STATIC_CAST(void*, mContent));
  fprintf(out, " sc=%p", NS_STATIC_CAST(void*, mStyleContext));
  nsIAtom* pseudoTag = mStyleContext->GetPseudoType();
  if (pseudoTag) {
    nsAutoString atomString;
    pseudoTag->ToString(atomString);
    fprintf(out, " pst=%s",
            NS_LossyConvertUTF16toASCII(atomString).get());
  }
  fputs("<\n", out);

  // Output the text
  aIndent++;

  IndentBy(out, aIndent);
  fputs("\"", out);
  fputs(NS_LossyConvertUTF16toASCII(tmp).get(), out);
  fputs("\"\n", out);

  aIndent--;
  IndentBy(out, aIndent);
  fputs(">\n", out);

  return NS_OK;
}
#endif

void nsTextFrame::AdjustSelectionPointsForBidi(SelectionDetails *sdptr,
                                               PRInt32 textLength,
                                               PRBool isRTLChars,
                                               PRBool isOddLevel,
                                               PRBool isBidiSystem)
{
  /* This adjustment is required whenever the text has been reversed by
   * Mozilla before rendering.
   *
   * In theory this means any text whose Bidi embedding level has been
   * set by the Unicode Bidi algorithm to an odd value, but this is
   * only true in practice on a non-Bidi platform.
   * 
   * On a Bidi platform the situation is more complicated because the
   * platform will automatically reverse right-to-left characters; so
   * Mozilla reverses text whose natural directionality is the opposite
   * of its embedding level: right-to-left characters whose Bidi
   * embedding level is even (e.g. Visual Hebrew) or left-to-right and
   * neutral characters whose Bidi embedding level is odd (e.g. English
   * text with <bdo dir="rtl">).
   *
   * The following condition is accordingly an optimization of
   *  if ( (!isBidiSystem && isOddLevel) ||
   *       (isBidiSystem &&
   *        ((isRTLChars && !isOddLevel) ||
   *         (!isRTLChars && isOddLevel))))
   */
  if (isOddLevel ^ (isRTLChars && isBidiSystem)) {

    PRInt32 swap  = sdptr->mStart;
    sdptr->mStart = textLength - sdptr->mEnd;
    sdptr->mEnd   = textLength - swap;

    // temp fix for 75026 crasher until we fix the bidi code
    // the above bidi code cause mStart < 0 in some case
    // the problem is we have whitespace compression code in 
    // nsTextTransformer which cause mEnd > textLength
    NS_ASSERTION((sdptr->mStart >= 0) , "mStart >= 0");
    if(sdptr->mStart < 0 )
      sdptr->mStart = 0;

    NS_ASSERTION((sdptr->mEnd >= 0) , "mEnd >= 0");
    if(sdptr->mEnd < 0 )
      sdptr->mEnd = 0;

    NS_ASSERTION((sdptr->mStart <= sdptr->mEnd), "mStart <= mEnd");
    if(sdptr->mStart > sdptr->mEnd)
      sdptr->mEnd = sdptr->mStart;
  }
  
  return;
}

void
nsTextFrame::AdjustOffsetsForBidi(PRInt32 aStart, PRInt32 aEnd)
{
  AddStateBits(NS_FRAME_IS_BIDI);
  SetOffsets(aStart, aEnd);
}

void
nsTextFrame::SetOffsets(PRInt32 aStart, PRInt32 aEnd)
{
  mContentOffset = aStart;
  mContentLength = aEnd - aStart;
}

/**
 * @return PR_TRUE if this text frame ends with a newline character.  It should return
 * PR_FALSE if it is not a text frame.
 */
PRBool
nsTextFrame::HasTerminalNewline() const
{
  const nsTextFragment* frag = mContent->GetText();
  if (frag && mContentLength > 0) {
    PRUnichar ch = frag->CharAt(mContentOffset + mContentLength - 1);
    if (ch == '\n')
      return PR_TRUE;
  }
  return PR_FALSE;
}

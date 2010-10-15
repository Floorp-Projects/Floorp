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
 *   Mats Palmgren <matspal@gmail.com>
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

#ifndef nsTextFrame_h__
#define nsTextFrame_h__

#include "nsFrame.h"
#include "nsSplittableFrame.h"
#include "nsLineBox.h"
#include "gfxFont.h"
#include "gfxSkipChars.h"
#include "gfxContext.h"

class nsTextPaintStyle;
class PropertyProvider;

// This state bit is set on frames that have some non-collapsed characters after
// reflow
#define TEXT_HAS_NONCOLLAPSED_CHARACTERS NS_FRAME_STATE_BIT(31)

class nsTextFrame : public nsFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend class nsContinuingTextFrame;

  nsTextFrame(nsStyleContext* aContext) : nsFrame(aContext)
  {
    NS_ASSERTION(mContentOffset == 0, "Bogus content offset");
  }
  
  // nsIFrame
  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists);

  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);

  virtual void DestroyFrom(nsIFrame* aDestructRoot);
  
  NS_IMETHOD GetCursor(const nsPoint& aPoint,
                       nsIFrame::Cursor& aCursor);
  
  NS_IMETHOD CharacterDataChanged(CharacterDataChangeInfo* aInfo);
                                  
  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext);
  
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
    return nsFrame::IsFrameOfType(aFlags & ~(nsIFrame::eReplaced |
                                             nsIFrame::eLineParticipant));
  }

#ifdef DEBUG
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
  NS_IMETHOD_(nsFrameState) GetDebugStateBits() const ;
#endif
  
  virtual ContentOffsets CalcContentOffsetsFromFramePoint(nsPoint aPoint);
  ContentOffsets GetCharacterOffsetAtFramePoint(const nsPoint &aPoint);

  /**
   * This is called only on the primary text frame. It indicates that
   * the selection state of the given character range has changed.
   * Text in the range is unconditionally invalidated
   * (nsTypedSelection::Repaint depends on this).
   * @param aSelected true if the selection has been added to the range,
   * false otherwise
   * @param aType the type of selection added or removed
   */
  virtual void SetSelected(PRBool        aSelected,
                           SelectionType aType);
  void SetSelectedRange(PRUint32 aStart,
                        PRUint32 aEnd,
                        PRBool aSelected,
                        SelectionType aType);

  virtual PRBool PeekOffsetNoAmount(PRBool aForward, PRInt32* aOffset);
  virtual PRBool PeekOffsetCharacter(PRBool aForward, PRInt32* aOffset);
  virtual PRBool PeekOffsetWord(PRBool aForward, PRBool aWordSelectEatSpace, PRBool aIsKeyboardSelect,
                                PRInt32* aOffset, PeekWordState* aState);

  NS_IMETHOD CheckVisibility(nsPresContext* aContext, PRInt32 aStartIndex, PRInt32 aEndIndex, PRBool aRecurse, PRBool *aFinished, PRBool *_retval);
  
  // Update offsets to account for new length. This may clear mTextRun.
  void SetLength(PRInt32 aLength, nsLineLayout* aLineLayout);
  
  NS_IMETHOD GetOffsets(PRInt32 &start, PRInt32 &end)const;
  
  virtual void AdjustOffsetsForBidi(PRInt32 start, PRInt32 end);
  
  NS_IMETHOD GetPointFromOffset(PRInt32                 inOffset,
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

  /**
   * Returns true if this text frame is logically adjacent to the end of the
   * line.
   */
  PRBool IsAtEndOfLine() const;
  
  /**
   * Call this only after reflow the frame. Returns true if non-collapsed
   * characters are present.
   */
  PRBool HasNoncollapsedCharacters() const {
    return (GetStateBits() & TEXT_HAS_NONCOLLAPSED_CHARACTERS) != 0;
  }
  
#ifdef ACCESSIBILITY
  virtual already_AddRefed<nsAccessible> CreateAccessible();
#endif
  
  virtual void MarkIntrinsicWidthsDirty();
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
  virtual nsRect ComputeTightBounds(gfxContext* aContext) const;
  NS_IMETHOD Reflow(nsPresContext* aPresContext,
                    nsHTMLReflowMetrics& aMetrics,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);
  virtual PRBool CanContinueTextRun() const;
  // Method that is called for a text frame that is logically
  // adjacent to the end of the line (i.e. followed only by empty text frames,
  // placeholders or inlines containing such).
  struct TrimOutput {
    // true if we trimmed some space or changed metrics in some other way.
    // In this case, we should call RecomputeOverflow on this frame.
    PRPackedBool mChanged;
    // true if the last character is not justifiable so should be subtracted
    // from the count of justifiable characters in the frame, since the last
    // character in a line is not justifiable.
    PRPackedBool mLastCharIsJustifiable;
    // an amount to *subtract* from the frame's width (zero if !mChanged)
    nscoord      mDeltaWidth;
  };
  TrimOutput TrimTrailingWhiteSpace(nsIRenderingContext* aRC);
  virtual nsresult GetRenderedText(nsAString* aString = nsnull,
                                   gfxSkipChars* aSkipChars = nsnull,
                                   gfxSkipCharsIterator* aSkipIter = nsnull,
                                   PRUint32 aSkippedStartOffset = 0,
                                   PRUint32 aSkippedMaxLength = PR_UINT32_MAX);

  nsOverflowAreas RecomputeOverflow();

  void AddInlineMinWidthForFlow(nsIRenderingContext *aRenderingContext,
                                nsIFrame::InlineMinWidthData *aData);
  void AddInlinePrefWidthForFlow(nsIRenderingContext *aRenderingContext,
                                 InlinePrefWidthData *aData);

  gfxFloat GetSnappedBaselineY(gfxContext* aContext, gfxFloat aY);

  // primary frame paint method called from nsDisplayText
  // The private DrawText() is what applies the text to a graphics context
  void PaintText(nsIRenderingContext* aRenderingContext, nsPoint aPt,
                 const nsRect& aDirtyRect);
  // helper: paint quirks-mode CSS text decorations
  void PaintTextDecorations(gfxContext* aCtx, const gfxRect& aDirtyRect,
                            const gfxPoint& aFramePt,
                            const gfxPoint& aTextBaselinePt,
                            nsTextPaintStyle& aTextStyle,
                            PropertyProvider& aProvider,
                            const nscolor* aOverrideColor = nsnull);
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

  virtual nscolor GetCaretColorAt(PRInt32 aOffset);

  PRInt16 GetSelectionStatus(PRInt16* aSelectionFlags);

#ifdef DEBUG
  void ToCString(nsCString& aBuf, PRInt32* aTotalContentLength) const;
#endif

  PRInt32 GetContentOffset() const { return mContentOffset; }
  PRInt32 GetContentLength() const
  {
    NS_ASSERTION(GetContentEnd() - mContentOffset >= 0, "negative length");
    return GetContentEnd() - mContentOffset;
  }
  PRInt32 GetContentEnd() const;
  // This returns the length the frame thinks it *should* have after it was
  // last reflowed (0 if it hasn't been reflowed yet). This should be used only
  // when setting up the text offsets for a new continuation frame.
  PRInt32 GetContentLengthHint() const { return mContentLengthHint; }

  // Compute the length of the content mapped by this frame
  // and all its in-flow siblings. Basically this means starting at mContentOffset
  // and going to the end of the text node or the next bidi continuation
  // boundary.
  PRInt32 GetInFlowContentLength();

  /**
   * Acquires the text run for this content, if necessary.
   * @param aRC the rendering context to use as a reference for creating
   * the textrun, if available (if not, we'll create one which will just be slower)
   * @param aBlock the block ancestor for this frame, or nsnull if unknown
   * @param aLine the line that this frame is on, if any, or nsnull if unknown
   * @param aFlowEndInTextRun if non-null, this returns the textrun offset of
   * end of the text associated with this frame and its in-flow siblings
   * @return a gfxSkipCharsIterator set up to map DOM offsets for this frame
   * to offsets into the textrun; its initial offset is set to this frame's
   * content offset
   */
  gfxSkipCharsIterator EnsureTextRun(gfxContext* aReferenceContext = nsnull,
                                     nsIFrame* aLineContainer = nsnull,
                                     const nsLineList::iterator* aLine = nsnull,
                                     PRUint32* aFlowEndInTextRun = nsnull);

  gfxTextRun* GetTextRun() { return mTextRun; }
  void SetTextRun(gfxTextRun* aTextRun) { mTextRun = aTextRun; }
  /**
   * Clears out |mTextRun| from all frames that hold a reference to it,
   * starting at |aStartContinuation|, or if it's nsnull, starting at |this|.
   * Deletes |mTextRun| if all references were cleared and it's not cached.
   */
  void ClearTextRun(nsTextFrame* aStartContinuation);

  // Get the DOM content range mapped by this frame after excluding
  // whitespace subject to start-of-line and end-of-line trimming.
  // The textrun must have been created before calling this.
  struct TrimmedOffsets {
    PRInt32 mStart;
    PRInt32 mLength;
    PRInt32 GetEnd() { return mStart + mLength; }
  };
  TrimmedOffsets GetTrimmedOffsets(const nsTextFragment* aFrag,
                                   PRBool aTrimAfter);

  // Similar to Reflow(), but for use from nsLineLayout
  void ReflowText(nsLineLayout& aLineLayout, nscoord aAvailableWidth,
                  nsIRenderingContext* aRenderingContext, PRBool aShouldBlink,
                  nsHTMLReflowMetrics& aMetrics, nsReflowStatus& aStatus);

protected:
  virtual ~nsTextFrame();

  nsIFrame*   mNextContinuation;
  // The key invariant here is that mContentOffset never decreases along
  // a next-continuation chain. And of course mContentOffset is always <= the
  // the text node's content length, and the mContentOffset for the first frame
  // is always 0. Furthermore the text mapped by a frame is determined by
  // GetContentOffset() and GetContentLength()/GetContentEnd(), which get
  // the length from the difference between this frame's offset and the next
  // frame's offset, or the text length if there is no next frame. This means
  // the frames always map the text node without overlapping or leaving any gaps.
  PRInt32     mContentOffset;
  // This does *not* indicate the length of text currently mapped by the frame;
  // instead it's a hint saying that this frame *wants* to map this much text
  // so if we create a new continuation, this is where that continuation should
  // start.
  PRInt32     mContentLengthHint;
  nscoord     mAscent;
  gfxTextRun* mTextRun;

  // The caller of this method must call DestroySelectionDetails() on the
  // return value, if that return value is not null.  Calling
  // DestroySelectionDetails() on a null value is still OK, just not necessary.
  SelectionDetails* GetSelectionDetails();
  
  void UnionTextDecorationOverflow(nsPresContext* aPresContext,
                                   PropertyProvider& aProvider,
                                   nsRect* aVisualOverflowRect);

  void DrawText(gfxContext* aCtx,
                const gfxPoint& aTextBaselinePt,
                PRUint32 aOffset,
                PRUint32 aLength,
                const gfxRect* aDirtyRect,
                PropertyProvider* aProvider,
                gfxFloat& aAdvanceWidth,
                PRBool aDrawSoftHyphen);

  void PaintOneShadow(PRUint32 aOffset,
                      PRUint32 aLength,
                      nsCSSShadowItem* aShadowDetails,
                      PropertyProvider* aProvider,
                      const nsRect& aDirtyRect,
                      const gfxPoint& aFramePt,
                      const gfxPoint& aTextBaselinePt,
                      gfxContext* aCtx,
                      const nscolor& aForegroundColor);

  struct TextDecorations {
    PRUint8 mDecorations;
    nscolor mOverColor;
    nscolor mUnderColor;
    nscolor mStrikeColor;

    TextDecorations() :
      mDecorations(0), mOverColor(NS_RGB(0, 0, 0)),
      mUnderColor(NS_RGB(0, 0, 0)), mStrikeColor(NS_RGB(0, 0, 0))
    { }

    PRBool HasDecorationlines() {
      return !!(mDecorations & (NS_STYLE_TEXT_DECORATION_UNDERLINE |
                                NS_STYLE_TEXT_DECORATION_OVERLINE |
                                NS_STYLE_TEXT_DECORATION_LINE_THROUGH));
    }
    PRBool HasUnderline() {
      return !!(mDecorations & NS_STYLE_TEXT_DECORATION_UNDERLINE);
    }
    PRBool HasOverline() {
      return !!(mDecorations & NS_STYLE_TEXT_DECORATION_OVERLINE);
    }
    PRBool HasStrikeout() {
      return !!(mDecorations & NS_STYLE_TEXT_DECORATION_LINE_THROUGH);
    }
  };
  TextDecorations GetTextDecorations(nsPresContext* aPresContext);

  // Set non empty rect to aRect, it should be overflow rect or frame rect.
  // If the result rect is larger than the given rect, this returns PR_TRUE.
  PRBool CombineSelectionUnderlineRect(nsPresContext* aPresContext,
                                       nsRect& aRect);

  PRBool IsFloatingFirstLetterChild();

  ContentOffsets GetCharacterOffsetAtFramePointInternal(const nsPoint &aPoint,
                   PRBool aForInsertionPoint);

  void ClearFrameOffsetCache();

  virtual PRBool HasAnyNoncollapsedCharacters();
};

#endif

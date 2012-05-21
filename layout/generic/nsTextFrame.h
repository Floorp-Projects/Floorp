/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTextFrame_h__
#define nsTextFrame_h__

#include "nsFrame.h"
#include "nsSplittableFrame.h"
#include "nsLineBox.h"
#include "gfxFont.h"
#include "gfxSkipChars.h"
#include "gfxContext.h"
#include "nsDisplayList.h"

class nsTextPaintStyle;
class PropertyProvider;

// This state bit is set on frames that have some non-collapsed characters after
// reflow
#define TEXT_HAS_NONCOLLAPSED_CHARACTERS NS_FRAME_STATE_BIT(31)

#define TEXT_HAS_FONT_INFLATION          NS_FRAME_STATE_BIT(61)

class nsTextFrame : public nsFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend class nsContinuingTextFrame;

  nsTextFrame(nsStyleContext* aContext)
    : nsFrame(aContext)
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
  
  virtual bool IsFrameOfType(PRUint32 aFlags) const
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
  void SetSelectedRange(PRUint32 aStart, PRUint32 aEnd, bool aSelected,
                        SelectionType aType);

  virtual bool PeekOffsetNoAmount(bool aForward, PRInt32* aOffset);
  virtual bool PeekOffsetCharacter(bool aForward, PRInt32* aOffset,
                                     bool aRespectClusters = true);
  virtual bool PeekOffsetWord(bool aForward, bool aWordSelectEatSpace, bool aIsKeyboardSelect,
                                PRInt32* aOffset, PeekWordState* aState);

  NS_IMETHOD CheckVisibility(nsPresContext* aContext, PRInt32 aStartIndex, PRInt32 aEndIndex, bool aRecurse, bool *aFinished, bool *_retval);
  
  // Flags for aSetLengthFlags
  enum { ALLOW_FRAME_CREATION_AND_DESTRUCTION = 0x01 };

  // Update offsets to account for new length. This may clear mTextRun.
  void SetLength(PRInt32 aLength, nsLineLayout* aLineLayout,
                 PRUint32 aSetLengthFlags = 0);
  
  NS_IMETHOD GetOffsets(PRInt32 &start, PRInt32 &end)const;
  
  virtual void AdjustOffsetsForBidi(PRInt32 start, PRInt32 end);
  
  NS_IMETHOD GetPointFromOffset(PRInt32                 inOffset,
                                nsPoint*                outPoint);
  
  NS_IMETHOD  GetChildFrameContainingOffset(PRInt32     inContentOffset,
                                            bool                    inHint,
                                            PRInt32*                outFrameContentOffset,
                                            nsIFrame*               *outChildFrame);
  
  virtual bool IsVisibleInSelection(nsISelection* aSelection);
  
  virtual bool IsEmpty();
  virtual bool IsSelfEmpty() { return IsEmpty(); }
  virtual nscoord GetBaseline() const;
  
  /**
   * @return true if this text frame ends with a newline character.  It
   * should return false if this is not a text frame.
   */
  virtual bool HasTerminalNewline() const;

  /**
   * Returns true if this text frame is logically adjacent to the end of the
   * line.
   */
  bool IsAtEndOfLine() const;
  
  /**
   * Call this only after reflow the frame. Returns true if non-collapsed
   * characters are present.
   */
  bool HasNoncollapsedCharacters() const {
    return (GetStateBits() & TEXT_HAS_NONCOLLAPSED_CHARACTERS) != 0;
  }
  
#ifdef ACCESSIBILITY
  virtual already_AddRefed<nsAccessible> CreateAccessible();
#endif

  float GetFontSizeInflation() const;
  bool HasFontSizeInflation() const {
    return (GetStateBits() & TEXT_HAS_FONT_INFLATION) != 0;
  }
  void SetFontSizeInflation(float aInflation);

  virtual void MarkIntrinsicWidthsDirty();
  virtual nscoord GetMinWidth(nsRenderingContext *aRenderingContext);
  virtual nscoord GetPrefWidth(nsRenderingContext *aRenderingContext);
  virtual void AddInlineMinWidth(nsRenderingContext *aRenderingContext,
                                 InlineMinWidthData *aData);
  virtual void AddInlinePrefWidth(nsRenderingContext *aRenderingContext,
                                  InlinePrefWidthData *aData);
  virtual nsSize ComputeSize(nsRenderingContext *aRenderingContext,
                             nsSize aCBSize, nscoord aAvailableWidth,
                             nsSize aMargin, nsSize aBorder, nsSize aPadding,
                             PRUint32 aFlags) MOZ_OVERRIDE;
  virtual nsRect ComputeTightBounds(gfxContext* aContext) const;
  NS_IMETHOD Reflow(nsPresContext* aPresContext,
                    nsHTMLReflowMetrics& aMetrics,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);
  virtual bool CanContinueTextRun() const;
  // Method that is called for a text frame that is logically
  // adjacent to the end of the line (i.e. followed only by empty text frames,
  // placeholders or inlines containing such).
  struct TrimOutput {
    // true if we trimmed some space or changed metrics in some other way.
    // In this case, we should call RecomputeOverflow on this frame.
    bool mChanged;
    // true if the last character is not justifiable so should be subtracted
    // from the count of justifiable characters in the frame, since the last
    // character in a line is not justifiable.
    bool mLastCharIsJustifiable;
    // an amount to *subtract* from the frame's width (zero if !mChanged)
    nscoord      mDeltaWidth;
  };
  TrimOutput TrimTrailingWhiteSpace(nsRenderingContext* aRC);
  virtual nsresult GetRenderedText(nsAString* aString = nsnull,
                                   gfxSkipChars* aSkipChars = nsnull,
                                   gfxSkipCharsIterator* aSkipIter = nsnull,
                                   PRUint32 aSkippedStartOffset = 0,
                                   PRUint32 aSkippedMaxLength = PR_UINT32_MAX);

  nsOverflowAreas
    RecomputeOverflow(const nsHTMLReflowState& aBlockReflowState);

  enum TextRunType {
    // Anything in reflow (but not intrinsic width calculation) or
    // painting should use the inflated text run (i.e., with font size
    // inflation applied).
    eInflated,
    // Intrinsic width calculation should use the non-inflated text run.
    // When there is font size inflation, it will be different.
    eNotInflated
  };

  void AddInlineMinWidthForFlow(nsRenderingContext *aRenderingContext,
                                nsIFrame::InlineMinWidthData *aData,
                                TextRunType aTextRunType);
  void AddInlinePrefWidthForFlow(nsRenderingContext *aRenderingContext,
                                 InlinePrefWidthData *aData,
                                 TextRunType aTextRunType);

  /**
   * Calculate the horizontal bounds of the grapheme clusters that fit entirely
   * inside the given left/right edges (which are positive lengths from the
   * respective frame edge).  If an input value is zero it is ignored and the
   * result for that edge is zero.  All out parameter values are undefined when
   * the method returns false.
   * @return true if at least one whole grapheme cluster fit between the edges
   */
  bool MeasureCharClippedText(nscoord aLeftEdge, nscoord aRightEdge,
                              nscoord* aSnappedLeftEdge,
                              nscoord* aSnappedRightEdge);
  /**
   * Same as above; this method also the returns the corresponding text run
   * offset and number of characters that fit.  All out parameter values are
   * undefined when the method returns false.
   * @return true if at least one whole grapheme cluster fit between the edges
   */
  bool MeasureCharClippedText(PropertyProvider& aProvider,
                              nscoord aLeftEdge, nscoord aRightEdge,
                              PRUint32* aStartOffset, PRUint32* aMaxLength,
                              nscoord* aSnappedLeftEdge,
                              nscoord* aSnappedRightEdge);
  // primary frame paint method called from nsDisplayText
  // The private DrawText() is what applies the text to a graphics context
  void PaintText(nsRenderingContext* aRenderingContext, nsPoint aPt,
                 const nsRect& aDirtyRect, const nsCharClipDisplayItem& aItem);
  // helper: paint text frame when we're impacted by at least one selection.
  // Return false if the text was not painted and we should continue with
  // the fast path.
  bool PaintTextWithSelection(gfxContext* aCtx,
                              const gfxPoint& aFramePt,
                              const gfxPoint& aTextBaselinePt,
                              const gfxRect& aDirtyRect,
                              PropertyProvider& aProvider,
                              PRUint32 aContentOffset,
                              PRUint32 aContentLength,
                              nsTextPaintStyle& aTextPaintStyle,
                              const nsCharClipDisplayItem::ClipEdges& aClipEdges);
  // helper: paint text with foreground and background colors determined
  // by selection(s). Also computes a mask of all selection types applying to
  // our text, returned in aAllTypes.
  // Return false if the text was not painted and we should continue with
  // the fast path.
  bool PaintTextWithSelectionColors(gfxContext* aCtx,
                                    const gfxPoint& aFramePt,
                                    const gfxPoint& aTextBaselinePt,
                                    const gfxRect& aDirtyRect,
                                    PropertyProvider& aProvider,
                                    PRUint32 aContentOffset,
                                    PRUint32 aContentLength,
                                    nsTextPaintStyle& aTextPaintStyle,
                                    SelectionDetails* aDetails,
                                    SelectionType* aAllTypes,
                            const nsCharClipDisplayItem::ClipEdges& aClipEdges);
  // helper: paint text decorations for text selected by aSelectionType
  void PaintTextSelectionDecorations(gfxContext* aCtx,
                                     const gfxPoint& aFramePt,
                                     const gfxPoint& aTextBaselinePt,
                                     const gfxRect& aDirtyRect,
                                     PropertyProvider& aProvider,
                                     PRUint32 aContentOffset,
                                     PRUint32 aContentLength,
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
   * @param aWhichTextRun indicates whether to get an inflated or non-inflated
   * text run
   * @param aReferenceContext the rendering context to use as a reference for
   * creating the textrun, if available (if not, we'll create one which will
   * just be slower)
   * @param aLineContainer the block ancestor for this frame, or nsnull if
   * unknown
   * @param aFlowEndInTextRun if non-null, this returns the textrun offset of
   * end of the text associated with this frame and its in-flow siblings
   * @return a gfxSkipCharsIterator set up to map DOM offsets for this frame
   * to offsets into the textrun; its initial offset is set to this frame's
   * content offset
   */
  gfxSkipCharsIterator EnsureTextRun(TextRunType aWhichTextRun,
                                     gfxContext* aReferenceContext = nsnull,
                                     nsIFrame* aLineContainer = nsnull,
                                     const nsLineList::iterator* aLine = nsnull,
                                     PRUint32* aFlowEndInTextRun = nsnull);

  gfxTextRun* GetTextRun(TextRunType aWhichTextRun) {
    if (aWhichTextRun == eInflated || !HasFontSizeInflation())
      return mTextRun;
    return GetUninflatedTextRun();
  }
  gfxTextRun* GetUninflatedTextRun();
  void SetTextRun(gfxTextRun* aTextRun, TextRunType aWhichTextRun,
                  float aInflation);
  /**
   * Notify the frame that it should drop its pointer to a text run.
   * Returns whether the text run was removed (i.e., whether it was
   * associated with this frame, either as its inflated or non-inflated
   * text run.
   */
  bool RemoveTextRun(gfxTextRun* aTextRun);
  /**
   * Clears out |mTextRun| (or the uninflated text run, when aInflated
   * is nsTextFrame::eNotInflated and there is inflation) from all frames that hold a
   * reference to it, starting at |aStartContinuation|, or if it's
   * nsnull, starting at |this|.  Deletes the text run if all references
   * were cleared and it's not cached.
   */
  void ClearTextRun(nsTextFrame* aStartContinuation,
                    TextRunType aWhichTextRun);

  void ClearTextRuns() {
    ClearTextRun(nsnull, nsTextFrame::eInflated);
    ClearTextRun(nsnull, nsTextFrame::eNotInflated);
  }

  // Get the DOM content range mapped by this frame after excluding
  // whitespace subject to start-of-line and end-of-line trimming.
  // The textrun must have been created before calling this.
  struct TrimmedOffsets {
    PRInt32 mStart;
    PRInt32 mLength;
    PRInt32 GetEnd() { return mStart + mLength; }
  };
  TrimmedOffsets GetTrimmedOffsets(const nsTextFragment* aFrag,
                                   bool aTrimAfter);

  // Similar to Reflow(), but for use from nsLineLayout
  void ReflowText(nsLineLayout& aLineLayout, nscoord aAvailableWidth,
                  nsRenderingContext* aRenderingContext, bool aShouldBlink,
                  nsHTMLReflowMetrics& aMetrics, nsReflowStatus& aStatus);

  bool IsFloatingFirstLetterChild() const;

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

  /**
   * Return true if the frame is part of a Selection.
   * Helper method to implement the public IsSelected() API.
   */
  virtual bool IsFrameSelected() const;

  // The caller of this method must call DestroySelectionDetails() on the
  // return value, if that return value is not null.  Calling
  // DestroySelectionDetails() on a null value is still OK, just not necessary.
  SelectionDetails* GetSelectionDetails();

  void UnionAdditionalOverflow(nsPresContext* aPresContext,
                               const nsHTMLReflowState& aBlockReflowState,
                               PropertyProvider& aProvider,
                               nsRect* aVisualOverflowRect,
                               bool aIncludeTextDecorations);

  void PaintOneShadow(PRUint32 aOffset,
                      PRUint32 aLength,
                      nsCSSShadowItem* aShadowDetails,
                      PropertyProvider* aProvider,
                      const nsRect& aDirtyRect,
                      const gfxPoint& aFramePt,
                      const gfxPoint& aTextBaselinePt,
                      gfxContext* aCtx,
                      const nscolor& aForegroundColor,
                      const nsCharClipDisplayItem::ClipEdges& aClipEdges,
                      nscoord aLeftSideOffset,
                      gfxRect& aBoundingBox);

  struct LineDecoration {
    nsIFrame* mFrame;

    // This is represents the offset from our baseline to mFrame's baseline;
    // positive offsets are *above* the baseline and negative offsets below
    nscoord mBaselineOffset;

    nscolor mColor;
    PRUint8 mStyle;

    LineDecoration(nsIFrame *const aFrame,
                   const nscoord aOff,
                   const nscolor aColor,
                   const PRUint8 aStyle)
      : mFrame(aFrame),
        mBaselineOffset(aOff),
        mColor(aColor),
        mStyle(aStyle)
    {}

    LineDecoration(const LineDecoration& aOther)
      : mFrame(aOther.mFrame),
        mBaselineOffset(aOther.mBaselineOffset),
        mColor(aOther.mColor),
        mStyle(aOther.mStyle)
    {}

    bool operator==(const LineDecoration& aOther) const {
      return mFrame == aOther.mFrame &&
             mStyle == aOther.mStyle &&
             mColor == aOther.mColor &&
             mBaselineOffset == aOther.mBaselineOffset;
    }
  };
  struct TextDecorations {
    nsAutoTArray<LineDecoration, 1> mOverlines, mUnderlines, mStrikes;

    TextDecorations() { }

    bool HasDecorationLines() const {
      return HasUnderline() || HasOverline() || HasStrikeout();
    }
    bool HasUnderline() const {
      return !mUnderlines.IsEmpty();
    }
    bool HasOverline() const {
      return !mOverlines.IsEmpty();
    }
    bool HasStrikeout() const {
      return !mStrikes.IsEmpty();
    }
  };
  void GetTextDecorations(nsPresContext* aPresContext,
                          TextDecorations& aDecorations);

  void DrawTextRun(gfxContext* const aCtx,
                   const gfxPoint& aTextBaselinePt,
                   PRUint32 aOffset,
                   PRUint32 aLength,
                   PropertyProvider& aProvider,
                   gfxFloat& aAdvanceWidth,
                   bool aDrawSoftHyphen);

  void DrawTextRunAndDecorations(gfxContext* const aCtx,
                                 const gfxRect& aDirtyRect,
                                 const gfxPoint& aFramePt,
                                 const gfxPoint& aTextBaselinePt,
                                 PRUint32 aOffset,
                                 PRUint32 aLength,
                                 PropertyProvider& aProvider,
                                 const nsTextPaintStyle& aTextStyle,
                             const nsCharClipDisplayItem::ClipEdges& aClipEdges,
                                 gfxFloat& aAdvanceWidth,
                                 bool aDrawSoftHyphen,
                                 const TextDecorations& aDecorations,
                                 const nscolor* const aDecorationOverrideColor);

  void DrawText(gfxContext* const aCtx,
                const gfxRect& aDirtyRect,
                const gfxPoint& aFramePt,
                const gfxPoint& aTextBaselinePt,
                PRUint32 aOffset,
                PRUint32 aLength,
                PropertyProvider& aProvider,
                const nsTextPaintStyle& aTextStyle,
                const nsCharClipDisplayItem::ClipEdges& aClipEdges,
                gfxFloat& aAdvanceWidth,
                bool aDrawSoftHyphen,
                const nscolor* const aDecorationOverrideColor = nsnull);

  // Set non empty rect to aRect, it should be overflow rect or frame rect.
  // If the result rect is larger than the given rect, this returns true.
  bool CombineSelectionUnderlineRect(nsPresContext* aPresContext,
                                       nsRect& aRect);

  ContentOffsets GetCharacterOffsetAtFramePointInternal(const nsPoint &aPoint,
                   bool aForInsertionPoint);

  void ClearFrameOffsetCache();

  virtual bool HasAnyNoncollapsedCharacters();

  void ClearMetrics(nsHTMLReflowMetrics& aMetrics);
};

#endif

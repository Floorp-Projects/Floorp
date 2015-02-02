/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTextFrame_h__
#define nsTextFrame_h__

#include "mozilla/Attributes.h"
#include "mozilla/gfx/2D.h"
#include "nsFrame.h"
#include "nsSplittableFrame.h"
#include "nsLineBox.h"
#include "gfxSkipChars.h"
#include "gfxTextRun.h"
#include "nsDisplayList.h"
#include "JustificationUtils.h"

// Undo the windows.h damage
#if defined(XP_WIN) && defined(DrawText)
#undef DrawText
#endif

class nsTextPaintStyle;
class PropertyProvider;
struct SelectionDetails;
class nsTextFragment;

typedef nsFrame nsTextFrameBase;

class nsDisplayTextGeometry;
class nsDisplayText;

class nsTextFrameTextRunCache {
public:
  static void Init();
  static void Shutdown();
};

class nsTextFrame : public nsTextFrameBase {
  typedef mozilla::gfx::DrawTarget DrawTarget;
  typedef mozilla::gfx::Rect Rect;

public:
  NS_DECL_QUERYFRAME_TARGET(nsTextFrame)
  NS_DECL_FRAMEARENA_HELPERS

  friend class nsContinuingTextFrame;
  friend class nsDisplayTextGeometry;
  friend class nsDisplayText;

  explicit nsTextFrame(nsStyleContext* aContext)
    : nsTextFrameBase(aContext)
  {
    NS_ASSERTION(mContentOffset == 0, "Bogus content offset");
  }
  
  // nsQueryFrame
  NS_DECL_QUERYFRAME

  // nsIFrame
  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) MOZ_OVERRIDE;

  virtual void DestroyFrom(nsIFrame* aDestructRoot) MOZ_OVERRIDE;
  
  virtual nsresult GetCursor(const nsPoint& aPoint,
                             nsIFrame::Cursor& aCursor) MOZ_OVERRIDE;
  
  virtual nsresult CharacterDataChanged(CharacterDataChangeInfo* aInfo) MOZ_OVERRIDE;
                                  
  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext) MOZ_OVERRIDE;
  
  virtual nsIFrame* GetNextContinuation() const MOZ_OVERRIDE {
    return mNextContinuation;
  }
  virtual void SetNextContinuation(nsIFrame* aNextContinuation) MOZ_OVERRIDE {
    NS_ASSERTION (!aNextContinuation || GetType() == aNextContinuation->GetType(),
                  "setting a next continuation with incorrect type!");
    NS_ASSERTION (!nsSplittableFrame::IsInNextContinuationChain(aNextContinuation, this),
                  "creating a loop in continuation chain!");
    mNextContinuation = aNextContinuation;
    if (aNextContinuation)
      aNextContinuation->RemoveStateBits(NS_FRAME_IS_FLUID_CONTINUATION);
  }
  virtual nsIFrame* GetNextInFlowVirtual() const MOZ_OVERRIDE { return GetNextInFlow(); }
  nsIFrame* GetNextInFlow() const {
    return mNextContinuation && (mNextContinuation->GetStateBits() & NS_FRAME_IS_FLUID_CONTINUATION) ? 
      mNextContinuation : nullptr;
  }
  virtual void SetNextInFlow(nsIFrame* aNextInFlow) MOZ_OVERRIDE {
    NS_ASSERTION (!aNextInFlow || GetType() == aNextInFlow->GetType(),
                  "setting a next in flow with incorrect type!");
    NS_ASSERTION (!nsSplittableFrame::IsInNextContinuationChain(aNextInFlow, this),
                  "creating a loop in continuation chain!");
    mNextContinuation = aNextInFlow;
    if (aNextInFlow)
      aNextInFlow->AddStateBits(NS_FRAME_IS_FLUID_CONTINUATION);
  }
  virtual nsIFrame* LastInFlow() const MOZ_OVERRIDE;
  virtual nsIFrame* LastContinuation() const MOZ_OVERRIDE;
  
  virtual nsSplittableType GetSplittableType() const MOZ_OVERRIDE {
    return NS_FRAME_SPLITTABLE;
  }
  
  /**
    * Get the "type" of the frame
   *
   * @see nsGkAtoms::textFrame
   */
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;
  
  virtual bool IsFrameOfType(uint32_t aFlags) const MOZ_OVERRIDE
  {
    // Set the frame state bit for text frames to mark them as replaced.
    // XXX kipp: temporary
    return nsFrame::IsFrameOfType(aFlags & ~(nsIFrame::eReplaced |
                                             nsIFrame::eLineParticipant));
  }

  virtual void InvalidateFrame(uint32_t aDisplayItemKey = 0) MOZ_OVERRIDE;
  virtual void InvalidateFrameWithRect(const nsRect& aRect, uint32_t aDisplayItemKey = 0) MOZ_OVERRIDE;

#ifdef DEBUG_FRAME_DUMP
  void List(FILE* out = stderr, const char* aPrefix = "", uint32_t aFlags = 0) const MOZ_OVERRIDE;
  virtual nsresult GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
  void ToCString(nsCString& aBuf, int32_t* aTotalContentLength) const;
#endif

#ifdef DEBUG
  virtual nsFrameState GetDebugStateBits() const MOZ_OVERRIDE;
#endif
  
  virtual ContentOffsets CalcContentOffsetsFromFramePoint(nsPoint aPoint) MOZ_OVERRIDE;
  ContentOffsets GetCharacterOffsetAtFramePoint(const nsPoint &aPoint);

  /**
   * This is called only on the primary text frame. It indicates that
   * the selection state of the given character range has changed.
   * Text in the range is unconditionally invalidated
   * (Selection::Repaint depends on this).
   * @param aSelected true if the selection has been added to the range,
   * false otherwise
   * @param aType the type of selection added or removed
   */
  void SetSelectedRange(uint32_t aStart, uint32_t aEnd, bool aSelected,
                        SelectionType aType);

  virtual FrameSearchResult PeekOffsetNoAmount(bool aForward, int32_t* aOffset) MOZ_OVERRIDE;
  virtual FrameSearchResult PeekOffsetCharacter(bool aForward, int32_t* aOffset,
                                     bool aRespectClusters = true) MOZ_OVERRIDE;
  virtual FrameSearchResult PeekOffsetWord(bool aForward, bool aWordSelectEatSpace, bool aIsKeyboardSelect,
                                int32_t* aOffset, PeekWordState* aState) MOZ_OVERRIDE;

  virtual nsresult CheckVisibility(nsPresContext* aContext, int32_t aStartIndex, int32_t aEndIndex, bool aRecurse, bool *aFinished, bool *_retval) MOZ_OVERRIDE;
  
  // Flags for aSetLengthFlags
  enum { ALLOW_FRAME_CREATION_AND_DESTRUCTION = 0x01 };

  // Update offsets to account for new length. This may clear mTextRun.
  void SetLength(int32_t aLength, nsLineLayout* aLineLayout,
                 uint32_t aSetLengthFlags = 0);
  
  virtual nsresult GetOffsets(int32_t &start, int32_t &end)const MOZ_OVERRIDE;
  
  virtual void AdjustOffsetsForBidi(int32_t start, int32_t end) MOZ_OVERRIDE;
  
  virtual nsresult GetPointFromOffset(int32_t  inOffset,
                                      nsPoint* outPoint) MOZ_OVERRIDE;
  
  virtual nsresult GetChildFrameContainingOffset(int32_t inContentOffset,
                                                 bool    inHint,
                                                 int32_t* outFrameContentOffset,
                                                 nsIFrame** outChildFrame) MOZ_OVERRIDE;
  
  virtual bool IsVisibleInSelection(nsISelection* aSelection) MOZ_OVERRIDE;
  
  virtual bool IsEmpty() MOZ_OVERRIDE;
  virtual bool IsSelfEmpty() MOZ_OVERRIDE { return IsEmpty(); }
  virtual nscoord GetLogicalBaseline(mozilla::WritingMode aWritingMode) const MOZ_OVERRIDE;
  
  virtual bool HasSignificantTerminalNewline() const MOZ_OVERRIDE;

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
  virtual mozilla::a11y::AccType AccessibleType() MOZ_OVERRIDE;
#endif

  float GetFontSizeInflation() const;
  bool IsCurrentFontInflation(float aInflation) const;
  bool HasFontSizeInflation() const {
    return (GetStateBits() & TEXT_HAS_FONT_INFLATION) != 0;
  }
  void SetFontSizeInflation(float aInflation);

  virtual void MarkIntrinsicISizesDirty() MOZ_OVERRIDE;
  virtual nscoord GetMinISize(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;
  virtual nscoord GetPrefISize(nsRenderingContext *aRenderingContext) MOZ_OVERRIDE;
  virtual void AddInlineMinISize(nsRenderingContext *aRenderingContext,
                                 InlineMinISizeData *aData) MOZ_OVERRIDE;
  virtual void AddInlinePrefISize(nsRenderingContext *aRenderingContext,
                                  InlinePrefISizeData *aData) MOZ_OVERRIDE;
  virtual mozilla::LogicalSize
  ComputeSize(nsRenderingContext *aRenderingContext,
              mozilla::WritingMode aWritingMode,
              const mozilla::LogicalSize& aCBSize,
              nscoord aAvailableISize,
              const mozilla::LogicalSize& aMargin,
              const mozilla::LogicalSize& aBorder,
              const mozilla::LogicalSize& aPadding,
              ComputeSizeFlags aFlags) MOZ_OVERRIDE;
  virtual nsRect ComputeTightBounds(gfxContext* aContext) const MOZ_OVERRIDE;
  virtual nsresult GetPrefWidthTightBounds(nsRenderingContext* aContext,
                                           nscoord* aX,
                                           nscoord* aXMost) MOZ_OVERRIDE;
  virtual void Reflow(nsPresContext* aPresContext,
                      nsHTMLReflowMetrics& aMetrics,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus& aStatus) MOZ_OVERRIDE;
  virtual bool CanContinueTextRun() const MOZ_OVERRIDE;
  // Method that is called for a text frame that is logically
  // adjacent to the end of the line (i.e. followed only by empty text frames,
  // placeholders or inlines containing such).
  struct TrimOutput {
    // true if we trimmed some space or changed metrics in some other way.
    // In this case, we should call RecomputeOverflow on this frame.
    bool mChanged;
    // an amount to *subtract* from the frame's width (zero if !mChanged)
    nscoord      mDeltaWidth;
  };
  TrimOutput TrimTrailingWhiteSpace(nsRenderingContext* aRC);
  virtual nsresult GetRenderedText(nsAString* aString = nullptr,
                                   gfxSkipChars* aSkipChars = nullptr,
                                   gfxSkipCharsIterator* aSkipIter = nullptr,
                                   uint32_t aSkippedStartOffset = 0,
                                   uint32_t aSkippedMaxLength = UINT32_MAX) MOZ_OVERRIDE;

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

  void AddInlineMinISizeForFlow(nsRenderingContext *aRenderingContext,
                                nsIFrame::InlineMinISizeData *aData,
                                TextRunType aTextRunType);
  void AddInlinePrefISizeForFlow(nsRenderingContext *aRenderingContext,
                                 InlinePrefISizeData *aData,
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
                              uint32_t* aStartOffset, uint32_t* aMaxLength,
                              nscoord* aSnappedLeftEdge,
                              nscoord* aSnappedRightEdge);

  /**
   * Object with various callbacks for PaintText() to invoke for different parts
   * of the frame's text rendering, when we're generating paths rather than
   * painting.
   *
   * Callbacks are invoked in the following order:
   *
   *   (NotifySelectionBackgroundNeedsFill)?
   *   (NotifyBeforeDecorationLine NotifyDecorationLinePathEmitted)*
   *   NotifyBeforeText
   *   (NotifyGlyphPathEmitted |
   *    (NotifyBeforeSVGGlyphPainted NotifyAfterSVGGlyphPainted))*
   *   NotifyAfterText
   *   (NotifyBeforeDecorationLine NotifyDecorationLinePathEmitted)*
   *   (NotifyBeforeSelectionDecorationLine NotifySelectionDecorationLinePathEmitted)*
   *
   * The color of each part of the frame's text rendering is passed as an argument
   * to the NotifyBefore* callback for that part.  The nscolor can take on one of
   * the three selection special colors defined in LookAndFeel.h --
   * NS_TRANSPARENT, NS_SAME_AS_FOREGROUND_COLOR and
   * NS_40PERCENT_FOREGROUND_COLOR.
   */
  struct DrawPathCallbacks : gfxTextRunDrawCallbacks
  {
    /**
     * @param aShouldPaintSVGGlyphs Whether SVG glyphs should be painted.
     */
    explicit DrawPathCallbacks(bool aShouldPaintSVGGlyphs = false)
      : gfxTextRunDrawCallbacks(aShouldPaintSVGGlyphs)
    {
    }

    /**
     * Called to have the selection highlight drawn before the text is drawn
     * over the top.
     */
    virtual void NotifySelectionBackgroundNeedsFill(const Rect& aBackgroundRect,
                                                    nscolor aColor,
                                                    DrawTarget& aDrawTarget) { }

    /**
     * Called just before any paths have been emitted to the gfxContext
     * for the glyphs of the frame's text.
     */
    virtual void NotifyBeforeText(nscolor aColor) { }

    /**
     * Called just after all the paths have been emitted to the gfxContext
     * for the glyphs of the frame's text.
     */
    virtual void NotifyAfterText() { }

    /**
     * Called just before a path corresponding to a text decoration line
     * has been emitted to the gfxContext.
     */
    virtual void NotifyBeforeDecorationLine(nscolor aColor) { }

    /**
     * Called just after a path corresponding to a text decoration line
     * has been emitted to the gfxContext.
     */
    virtual void NotifyDecorationLinePathEmitted() { }

    /**
     * Called just before a path corresponding to a selection decoration line
     * has been emitted to the gfxContext.
     */
    virtual void NotifyBeforeSelectionDecorationLine(nscolor aColor) { }

    /**
     * Called just after a path corresponding to a selection decoration line
     * has been emitted to the gfxContext.
     */
    virtual void NotifySelectionDecorationLinePathEmitted() { }
  };

  // Primary frame paint method called from nsDisplayText.  Can also be used
  // to generate paths rather than paint the frame's text by passing a callback
  // object.  The private DrawText() is what applies the text to a graphics
  // context.
  void PaintText(nsRenderingContext* aRenderingContext, nsPoint aPt,
                 const nsRect& aDirtyRect, const nsCharClipDisplayItem& aItem,
                 gfxTextContextPaint* aContextPaint = nullptr,
                 DrawPathCallbacks* aCallbacks = nullptr);
  // helper: paint text frame when we're impacted by at least one selection.
  // Return false if the text was not painted and we should continue with
  // the fast path.
  bool PaintTextWithSelection(gfxContext* aCtx,
                              const gfxPoint& aFramePt,
                              const gfxPoint& aTextBaselinePt,
                              const gfxRect& aDirtyRect,
                              PropertyProvider& aProvider,
                              uint32_t aContentOffset,
                              uint32_t aContentLength,
                              nsTextPaintStyle& aTextPaintStyle,
                              const nsCharClipDisplayItem::ClipEdges& aClipEdges,
                              gfxTextContextPaint* aContextPaint,
                              DrawPathCallbacks* aCallbacks);
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
                                    uint32_t aContentOffset,
                                    uint32_t aContentLength,
                                    nsTextPaintStyle& aTextPaintStyle,
                                    SelectionDetails* aDetails,
                                    SelectionType* aAllTypes,
                             const nsCharClipDisplayItem::ClipEdges& aClipEdges,
                                    DrawPathCallbacks* aCallbacks);
  // helper: paint text decorations for text selected by aSelectionType
  void PaintTextSelectionDecorations(gfxContext* aCtx,
                                     const gfxPoint& aFramePt,
                                     const gfxPoint& aTextBaselinePt,
                                     const gfxRect& aDirtyRect,
                                     PropertyProvider& aProvider,
                                     uint32_t aContentOffset,
                                     uint32_t aContentLength,
                                     nsTextPaintStyle& aTextPaintStyle,
                                     SelectionDetails* aDetails,
                                     SelectionType aSelectionType,
                                     DrawPathCallbacks* aCallbacks);

  virtual nscolor GetCaretColorAt(int32_t aOffset) MOZ_OVERRIDE;

  int16_t GetSelectionStatus(int16_t* aSelectionFlags);

  int32_t GetContentOffset() const { return mContentOffset; }
  int32_t GetContentLength() const
  {
    NS_ASSERTION(GetContentEnd() - mContentOffset >= 0, "negative length");
    return GetContentEnd() - mContentOffset;
  }
  int32_t GetContentEnd() const;
  // This returns the length the frame thinks it *should* have after it was
  // last reflowed (0 if it hasn't been reflowed yet). This should be used only
  // when setting up the text offsets for a new continuation frame.
  int32_t GetContentLengthHint() const { return mContentLengthHint; }

  // Compute the length of the content mapped by this frame
  // and all its in-flow siblings. Basically this means starting at mContentOffset
  // and going to the end of the text node or the next bidi continuation
  // boundary.
  int32_t GetInFlowContentLength();

  /**
   * Acquires the text run for this content, if necessary.
   * @param aWhichTextRun indicates whether to get an inflated or non-inflated
   * text run
   * @param aReferenceContext the rendering context to use as a reference for
   * creating the textrun, if available (if not, we'll create one which will
   * just be slower)
   * @param aLineContainer the block ancestor for this frame, or nullptr if
   * unknown
   * @param aFlowEndInTextRun if non-null, this returns the textrun offset of
   * end of the text associated with this frame and its in-flow siblings
   * @return a gfxSkipCharsIterator set up to map DOM offsets for this frame
   * to offsets into the textrun; its initial offset is set to this frame's
   * content offset
   */
  gfxSkipCharsIterator EnsureTextRun(TextRunType aWhichTextRun,
                                     gfxContext* aReferenceContext = nullptr,
                                     nsIFrame* aLineContainer = nullptr,
                                     const nsLineList::iterator* aLine = nullptr,
                                     uint32_t* aFlowEndInTextRun = nullptr);

  gfxTextRun* GetTextRun(TextRunType aWhichTextRun) {
    if (aWhichTextRun == eInflated || !HasFontSizeInflation())
      return mTextRun;
    return GetUninflatedTextRun();
  }
  gfxTextRun* GetUninflatedTextRun();
  void SetTextRun(gfxTextRun* aTextRun, TextRunType aWhichTextRun,
                  float aInflation);
  bool IsInTextRunUserData() const {
    return GetStateBits() &
      (TEXT_IN_TEXTRUN_USER_DATA | TEXT_IN_UNINFLATED_TEXTRUN_USER_DATA);
  }
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
   * nullptr, starting at |this|.  Deletes the text run if all references
   * were cleared and it's not cached.
   */
  void ClearTextRun(nsTextFrame* aStartContinuation,
                    TextRunType aWhichTextRun);

  void ClearTextRuns() {
    ClearTextRun(nullptr, nsTextFrame::eInflated);
    if (HasFontSizeInflation()) {
      ClearTextRun(nullptr, nsTextFrame::eNotInflated);
    }
  }

  /**
   * Wipe out references to textrun(s) without deleting the textruns.
   */
  void DisconnectTextRuns();

  // Get the DOM content range mapped by this frame after excluding
  // whitespace subject to start-of-line and end-of-line trimming.
  // The textrun must have been created before calling this.
  struct TrimmedOffsets {
    int32_t mStart;
    int32_t mLength;
    int32_t GetEnd() const { return mStart + mLength; }
  };
  TrimmedOffsets GetTrimmedOffsets(const nsTextFragment* aFrag,
                                   bool aTrimAfter, bool aPostReflow = true);

  // Similar to Reflow(), but for use from nsLineLayout
  void ReflowText(nsLineLayout& aLineLayout, nscoord aAvailableWidth,
                  nsRenderingContext* aRenderingContext,
                  nsHTMLReflowMetrics& aMetrics, nsReflowStatus& aStatus);

  bool IsFloatingFirstLetterChild() const;

  virtual bool UpdateOverflow() MOZ_OVERRIDE;

  void AssignJustificationGaps(const mozilla::JustificationAssignment& aAssign);
  mozilla::JustificationAssignment GetJustificationAssignment() const;

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
  int32_t     mContentOffset;
  // This does *not* indicate the length of text currently mapped by the frame;
  // instead it's a hint saying that this frame *wants* to map this much text
  // so if we create a new continuation, this is where that continuation should
  // start.
  int32_t     mContentLengthHint;
  nscoord     mAscent;
  gfxTextRun* mTextRun;

  /**
   * Return true if the frame is part of a Selection.
   * Helper method to implement the public IsSelected() API.
   */
  virtual bool IsFrameSelected() const MOZ_OVERRIDE;

  // The caller of this method must call DestroySelectionDetails() on the
  // return value, if that return value is not null.  Calling
  // DestroySelectionDetails() on a null value is still OK, just not necessary.
  SelectionDetails* GetSelectionDetails();

  void UnionAdditionalOverflow(nsPresContext* aPresContext,
                               nsIFrame* aBlock,
                               PropertyProvider& aProvider,
                               nsRect* aVisualOverflowRect,
                               bool aIncludeTextDecorations);

  void PaintOneShadow(uint32_t aOffset,
                      uint32_t aLength,
                      nsCSSShadowItem* aShadowDetails,
                      PropertyProvider* aProvider,
                      const nsRect& aDirtyRect,
                      const gfxPoint& aFramePt,
                      const gfxPoint& aTextBaselinePt,
                      gfxContext* aCtx,
                      const nscolor& aForegroundColor,
                      const nsCharClipDisplayItem::ClipEdges& aClipEdges,
                      nscoord aLeftSideOffset,
                      gfxRect& aBoundingBox,
                      uint32_t aBlurFlags);

  void PaintShadows(nsCSSShadowArray* aShadow,
                    uint32_t aOffset, uint32_t aLength,
                    const nsRect& aDirtyRect,
                    const gfxPoint& aFramePt,
                    const gfxPoint& aTextBaselinePt,
                    nscoord aLeftEdgeOffset,
                    PropertyProvider& aProvider,
                    nscolor aForegroundColor,
                    const nsCharClipDisplayItem::ClipEdges& aClipEdges,
                    gfxContext* aCtx);

  struct LineDecoration {
    nsIFrame* mFrame;

    // This is represents the offset from our baseline to mFrame's baseline;
    // positive offsets are *above* the baseline and negative offsets below
    nscoord mBaselineOffset;

    nscolor mColor;
    uint8_t mStyle;

    LineDecoration(nsIFrame *const aFrame,
                   const nscoord aOff,
                   const nscolor aColor,
                   const uint8_t aStyle)
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

    bool operator!=(const LineDecoration& aOther) const {
      return !(*this == aOther);
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
    bool operator==(const TextDecorations& aOther) const {
      return mOverlines == aOther.mOverlines &&
             mUnderlines == aOther.mUnderlines &&
             mStrikes == aOther.mStrikes;
    }
    
    bool operator!=(const TextDecorations& aOther) const {
      return !(*this == aOther);
    }

  };
  enum TextDecorationColorResolution {
    eResolvedColors,
    eUnresolvedColors
  };
  void GetTextDecorations(nsPresContext* aPresContext,
                          TextDecorationColorResolution aColorResolution,
                          TextDecorations& aDecorations);

  void DrawTextRun(gfxContext* const aCtx,
                   const gfxPoint& aTextBaselinePt,
                   uint32_t aOffset,
                   uint32_t aLength,
                   PropertyProvider& aProvider,
                   nscolor aTextColor,
                   gfxFloat& aAdvanceWidth,
                   bool aDrawSoftHyphen,
                   gfxTextContextPaint* aContextPaint,
                   DrawPathCallbacks* aCallbacks);

  void DrawTextRunAndDecorations(gfxContext* const aCtx,
                                 const gfxRect& aDirtyRect,
                                 const gfxPoint& aFramePt,
                                 const gfxPoint& aTextBaselinePt,
                                 uint32_t aOffset,
                                 uint32_t aLength,
                                 PropertyProvider& aProvider,
                                 const nsTextPaintStyle& aTextStyle,
                                 nscolor aTextColor,
                             const nsCharClipDisplayItem::ClipEdges& aClipEdges,
                                 gfxFloat& aAdvanceWidth,
                                 bool aDrawSoftHyphen,
                                 const TextDecorations& aDecorations,
                                 const nscolor* const aDecorationOverrideColor,
                                 gfxTextContextPaint* aContextPaint,
                                 DrawPathCallbacks* aCallbacks);

  void DrawText(gfxContext* const aCtx,
                const gfxRect& aDirtyRect,
                const gfxPoint& aFramePt,
                const gfxPoint& aTextBaselinePt,
                uint32_t aOffset,
                uint32_t aLength,
                PropertyProvider& aProvider,
                const nsTextPaintStyle& aTextStyle,
                nscolor aTextColor,
                const nsCharClipDisplayItem::ClipEdges& aClipEdges,
                gfxFloat& aAdvanceWidth,
                bool aDrawSoftHyphen,
                const nscolor* const aDecorationOverrideColor = nullptr,
                gfxTextContextPaint* aContextPaint = nullptr,
                DrawPathCallbacks* aCallbacks = nullptr);

  // Set non empty rect to aRect, it should be overflow rect or frame rect.
  // If the result rect is larger than the given rect, this returns true.
  bool CombineSelectionUnderlineRect(nsPresContext* aPresContext,
                                       nsRect& aRect);

  ContentOffsets GetCharacterOffsetAtFramePointInternal(nsPoint aPoint,
                   bool aForInsertionPoint);

  void ClearFrameOffsetCache();

  virtual bool HasAnyNoncollapsedCharacters() MOZ_OVERRIDE;

  void ClearMetrics(nsHTMLReflowMetrics& aMetrics);

  NS_DECLARE_FRAME_PROPERTY(JustificationAssignment, nullptr)
};

#endif

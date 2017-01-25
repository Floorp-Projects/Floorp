/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTextFrame_h__
#define nsTextFrame_h__

#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/UniquePtr.h"
#include "nsFrame.h"
#include "nsSplittableFrame.h"
#include "nsLineBox.h"
#include "gfxSkipChars.h"
#include "gfxTextRun.h"
#include "nsDisplayList.h"
#include "JustificationUtils.h"
#include "RubyUtils.h"

// Undo the windows.h damage
#if defined(XP_WIN) && defined(DrawText)
#undef DrawText
#endif

class nsTextPaintStyle;
class PropertyProvider;
struct SelectionDetails;
class nsTextFragment;

class nsDisplayTextGeometry;
class nsDisplayText;

namespace mozilla {
class SVGContextPaint;
};

class nsTextFrame : public nsFrame {
  typedef mozilla::LayoutDeviceRect LayoutDeviceRect;
  typedef mozilla::RawSelectionType RawSelectionType;
  typedef mozilla::SelectionType SelectionType;
  typedef mozilla::TextRangeStyle TextRangeStyle;
  typedef mozilla::gfx::DrawTarget DrawTarget;
  typedef mozilla::gfx::Point Point;
  typedef mozilla::gfx::Rect Rect;
  typedef mozilla::gfx::Size Size;
  typedef gfxTextRun::Range Range;

public:
  NS_DECL_QUERYFRAME_TARGET(nsTextFrame)
  NS_DECL_FRAMEARENA_HELPERS

  friend class nsContinuingTextFrame;
  friend class nsDisplayTextGeometry;
  friend class nsDisplayText;

  explicit nsTextFrame(nsStyleContext* aContext)
    : nsFrame(aContext)
    , mNextContinuation(nullptr)
    , mContentOffset(0)
    , mContentLengthHint(0)
    , mAscent(0)
  {
    NS_ASSERTION(mContentOffset == 0, "Bogus content offset");
  }
  
  // nsQueryFrame
  NS_DECL_QUERYFRAME

  // nsIFrame
  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  virtual void Init(nsIContent*       aContent,
                    nsContainerFrame* aParent,
                    nsIFrame*         aPrevInFlow) override;

  virtual void DestroyFrom(nsIFrame* aDestructRoot) override;
  
  virtual nsresult GetCursor(const nsPoint& aPoint,
                             nsIFrame::Cursor& aCursor) override;
  
  virtual nsresult CharacterDataChanged(CharacterDataChangeInfo* aInfo) override;
                                  
  virtual nsIFrame* GetNextContinuation() const override {
    return mNextContinuation;
  }
  virtual void SetNextContinuation(nsIFrame* aNextContinuation) override {
    NS_ASSERTION (!aNextContinuation || GetType() == aNextContinuation->GetType(),
                  "setting a next continuation with incorrect type!");
    NS_ASSERTION (!nsSplittableFrame::IsInNextContinuationChain(aNextContinuation, this),
                  "creating a loop in continuation chain!");
    mNextContinuation = aNextContinuation;
    if (aNextContinuation)
      aNextContinuation->RemoveStateBits(NS_FRAME_IS_FLUID_CONTINUATION);
  }
  virtual nsIFrame* GetNextInFlowVirtual() const override { return GetNextInFlow(); }
  nsIFrame* GetNextInFlow() const {
    return mNextContinuation && (mNextContinuation->GetStateBits() & NS_FRAME_IS_FLUID_CONTINUATION) ? 
      mNextContinuation : nullptr;
  }
  virtual void SetNextInFlow(nsIFrame* aNextInFlow) override {
    NS_ASSERTION (!aNextInFlow || GetType() == aNextInFlow->GetType(),
                  "setting a next in flow with incorrect type!");
    NS_ASSERTION (!nsSplittableFrame::IsInNextContinuationChain(aNextInFlow, this),
                  "creating a loop in continuation chain!");
    mNextContinuation = aNextInFlow;
    if (aNextInFlow)
      aNextInFlow->AddStateBits(NS_FRAME_IS_FLUID_CONTINUATION);
  }
  virtual nsIFrame* LastInFlow() const override;
  virtual nsIFrame* LastContinuation() const override;
  
  virtual nsSplittableType GetSplittableType() const override {
    return NS_FRAME_SPLITTABLE;
  }
  
  /**
    * Get the "type" of the frame
   *
   * @see nsGkAtoms::textFrame
   */
  virtual nsIAtom* GetType() const override;
  
  virtual bool IsFrameOfType(uint32_t aFlags) const override
  {
    // Set the frame state bit for text frames to mark them as replaced.
    // XXX kipp: temporary
    return nsFrame::IsFrameOfType(aFlags & ~(nsIFrame::eReplaced |
                                             nsIFrame::eLineParticipant));
  }

  bool ShouldSuppressLineBreak() const
  {
    // If the parent frame of the text frame is ruby content box, it must
    // suppress line break inside. This check is necessary, because when
    // a whitespace is only contained by pseudo ruby frames, its style
    // context won't have SuppressLineBreak bit set.
    if (mozilla::RubyUtils::IsRubyContentBox(GetParent()->GetType())) {
      return true;
    }
    return StyleContext()->ShouldSuppressLineBreak();
  }

  virtual void InvalidateFrame(uint32_t aDisplayItemKey = 0) override;
  virtual void InvalidateFrameWithRect(const nsRect& aRect, uint32_t aDisplayItemKey = 0) override;

#ifdef DEBUG_FRAME_DUMP
  void List(FILE* out = stderr, const char* aPrefix = "", uint32_t aFlags = 0) const override;
  virtual nsresult GetFrameName(nsAString& aResult) const override;
  void ToCString(nsCString& aBuf, int32_t* aTotalContentLength) const;
#endif

#ifdef DEBUG
  virtual nsFrameState GetDebugStateBits() const override;
#endif
  
  virtual ContentOffsets CalcContentOffsetsFromFramePoint(nsPoint aPoint) override;
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
                        SelectionType aSelectionType);

  virtual FrameSearchResult PeekOffsetNoAmount(bool aForward, int32_t* aOffset) override;
  virtual FrameSearchResult PeekOffsetCharacter(bool aForward, int32_t* aOffset,
                                     bool aRespectClusters = true) override;
  virtual FrameSearchResult PeekOffsetWord(bool aForward, bool aWordSelectEatSpace, bool aIsKeyboardSelect,
                                int32_t* aOffset, PeekWordState* aState) override;

  virtual nsresult CheckVisibility(nsPresContext* aContext, int32_t aStartIndex, int32_t aEndIndex, bool aRecurse, bool *aFinished, bool *_retval) override;
  
  // Flags for aSetLengthFlags
  enum { ALLOW_FRAME_CREATION_AND_DESTRUCTION = 0x01 };

  // Update offsets to account for new length. This may clear mTextRun.
  void SetLength(int32_t aLength, nsLineLayout* aLineLayout,
                 uint32_t aSetLengthFlags = 0);
  
  virtual nsresult GetOffsets(int32_t &start, int32_t &end)const override;
  
  virtual void AdjustOffsetsForBidi(int32_t start, int32_t end) override;
  
  virtual nsresult GetPointFromOffset(int32_t  inOffset,
                                      nsPoint* outPoint) override;
  virtual nsresult GetCharacterRectsInRange(int32_t  aInOffset,
                                            int32_t  aLength,
                                            nsTArray<nsRect>& aRects) override;
  
  virtual nsresult GetChildFrameContainingOffset(int32_t inContentOffset,
                                                 bool    inHint,
                                                 int32_t* outFrameContentOffset,
                                                 nsIFrame** outChildFrame) override;
  
  virtual bool IsVisibleInSelection(nsISelection* aSelection) override;
  
  virtual bool IsEmpty() override;
  virtual bool IsSelfEmpty() override { return IsEmpty(); }
  nscoord GetLogicalBaseline(mozilla::WritingMode aWritingMode) const final;
  
  virtual bool HasSignificantTerminalNewline() const override;

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
  virtual mozilla::a11y::AccType AccessibleType() override;
#endif

  float GetFontSizeInflation() const;
  bool IsCurrentFontInflation(float aInflation) const;
  bool HasFontSizeInflation() const {
    return (GetStateBits() & TEXT_HAS_FONT_INFLATION) != 0;
  }
  void SetFontSizeInflation(float aInflation);

  virtual void MarkIntrinsicISizesDirty() override;
  virtual nscoord GetMinISize(nsRenderingContext *aRenderingContext) override;
  virtual nscoord GetPrefISize(nsRenderingContext *aRenderingContext) override;
  virtual void AddInlineMinISize(nsRenderingContext *aRenderingContext,
                                 InlineMinISizeData *aData) override;
  virtual void AddInlinePrefISize(nsRenderingContext *aRenderingContext,
                                  InlinePrefISizeData *aData) override;
  virtual mozilla::LogicalSize
  ComputeSize(nsRenderingContext *aRenderingContext,
              mozilla::WritingMode aWritingMode,
              const mozilla::LogicalSize& aCBSize,
              nscoord aAvailableISize,
              const mozilla::LogicalSize& aMargin,
              const mozilla::LogicalSize& aBorder,
              const mozilla::LogicalSize& aPadding,
              ComputeSizeFlags aFlags) override;
  virtual nsRect ComputeTightBounds(DrawTarget* aDrawTarget) const override;
  virtual nsresult GetPrefWidthTightBounds(nsRenderingContext* aContext,
                                           nscoord* aX,
                                           nscoord* aXMost) override;
  virtual void Reflow(nsPresContext* aPresContext,
                      ReflowOutput& aMetrics,
                      const ReflowInput& aReflowInput,
                      nsReflowStatus& aStatus) override;
  virtual bool CanContinueTextRun() const override;
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
  TrimOutput TrimTrailingWhiteSpace(DrawTarget* aDrawTarget);
  virtual RenderedText GetRenderedText(uint32_t aStartOffset = 0,
                                       uint32_t aEndOffset = UINT32_MAX,
                                       TextOffsetType aOffsetType =
                                           TextOffsetType::OFFSETS_IN_CONTENT_TEXT,
                                       TrailingWhitespace aTrimTrailingWhitespace =
                                           TrailingWhitespace::TRIM_TRAILING_WHITESPACE) override;

  nsOverflowAreas RecomputeOverflow(nsIFrame* aBlockFrame);

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
   * inside the given left[top]/right[bottom] edges (which are positive lengths
   * from the respective frame edge).  If an input value is zero it is ignored
   * and the result for that edge is zero.  All out parameter values are
   * undefined when the method returns false.
   * @return true if at least one whole grapheme cluster fit between the edges
   */
  bool MeasureCharClippedText(nscoord aVisIStartEdge, nscoord aVisIEndEdge,
                              nscoord* aSnappedStartEdge,
                              nscoord* aSnappedEndEdge);
  /**
   * Same as above; this method also the returns the corresponding text run
   * offset and number of characters that fit.  All out parameter values are
   * undefined when the method returns false.
   * @return true if at least one whole grapheme cluster fit between the edges
   */
  bool MeasureCharClippedText(PropertyProvider& aProvider,
                              nscoord aVisIStartEdge, nscoord aVisIEndEdge,
                              uint32_t* aStartOffset, uint32_t* aMaxLength,
                              nscoord* aSnappedStartEdge,
                              nscoord* aSnappedEndEdge);

  /**
   * Object with various callbacks for PaintText() to invoke for different parts
   * of the frame's text rendering, when we're generating paths rather than
   * painting.
   *
   * Callbacks are invoked in the following order:
   *
   *   NotifySelectionBackgroundNeedsFill?
   *   PaintDecorationLine*
   *   NotifyBeforeText
   *   NotifyGlyphPathEmitted*
   *   NotifyAfterText
   *   PaintDecorationLine*
   *   PaintSelectionDecorationLine*
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
     * Called before (for under/over-line) or after (for line-through) the text
     * is drawn to have a text decoration line drawn.
     */
    virtual void PaintDecorationLine(Rect aPath, nscolor aColor) { }

    /**
     * Called after selected text is drawn to have a decoration line drawn over
     * the text. (All types of text decoration are drawn after the text when
     * text is selected.)
     */
    virtual void PaintSelectionDecorationLine(Rect aPath, nscolor aColor) { }

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

  struct PaintTextParams
  {
    gfxContext* context;
    gfxPoint framePt;
    LayoutDeviceRect dirtyRect;
    mozilla::SVGContextPaint* contextPaint = nullptr;
    DrawPathCallbacks* callbacks = nullptr;
    enum {
      PaintText,           // Normal text painting.
      PaintTextBGColor,    // Only paint background color of the selected text
                           // range in this state.
      GenerateTextMask     // To generate a mask from a text frame. Should
                           // only paint text itself with opaque color.
                           // Text shadow, text selection color and text
                           // decoration are all discarded in this state.
    };
    uint8_t state = PaintText;
    explicit PaintTextParams(gfxContext* aContext) : context(aContext) {}

    bool IsPaintText() const { return state == PaintText; }
    bool IsGenerateTextMask() const { return state == GenerateTextMask; }
    bool IsPaintBGColor() const { return state == PaintTextBGColor; }
  };

  struct PaintTextSelectionParams : PaintTextParams
  {
    gfxPoint textBaselinePt;
    PropertyProvider* provider = nullptr;
    Range contentRange;
    nsTextPaintStyle* textPaintStyle = nullptr;
    explicit PaintTextSelectionParams(const PaintTextParams& aParams)
      : PaintTextParams(aParams) {}
  };

  struct DrawTextRunParams
  {
    gfxContext* context;
    PropertyProvider* provider = nullptr;
    gfxFloat* advanceWidth = nullptr;
    mozilla::SVGContextPaint* contextPaint = nullptr;
    DrawPathCallbacks* callbacks = nullptr;
    nscolor textColor = NS_RGBA(0, 0, 0, 0);
    nscolor textStrokeColor = NS_RGBA(0, 0, 0, 0);
    float textStrokeWidth = 0.0f;
    bool drawSoftHyphen = false;
    explicit DrawTextRunParams(gfxContext* aContext)
      : context(aContext) {}
  };

  struct DrawTextParams : DrawTextRunParams
  {
    gfxPoint framePt;
    LayoutDeviceRect dirtyRect;
    const nsTextPaintStyle* textStyle = nullptr;
    const nsCharClipDisplayItem::ClipEdges* clipEdges = nullptr;
    const nscolor* decorationOverrideColor = nullptr;
    explicit DrawTextParams(gfxContext* aContext)
      : DrawTextRunParams(aContext) {}
  };

  // Primary frame paint method called from nsDisplayText.  Can also be used
  // to generate paths rather than paint the frame's text by passing a callback
  // object.  The private DrawText() is what applies the text to a graphics
  // context.
  void PaintText(const PaintTextParams& aParams,
                 const nsCharClipDisplayItem& aItem,
                 float aOpacity = 1.0f);
  // helper: paint text frame when we're impacted by at least one selection.
  // Return false if the text was not painted and we should continue with
  // the fast path.
  bool PaintTextWithSelection(const PaintTextSelectionParams& aParams,
                              const nsCharClipDisplayItem::ClipEdges& aClipEdges);
  // helper: paint text with foreground and background colors determined
  // by selection(s). Also computes a mask of all selection types applying to
  // our text, returned in aAllTypes.
  // Return false if the text was not painted and we should continue with
  // the fast path.
  bool PaintTextWithSelectionColors(
         const PaintTextSelectionParams& aParams,
         const mozilla::UniquePtr<SelectionDetails>& aDetails,
         RawSelectionType* aAllRawSelectionTypes,
         const nsCharClipDisplayItem::ClipEdges& aClipEdges);
  // helper: paint text decorations for text selected by aSelectionType
  void PaintTextSelectionDecorations(const PaintTextSelectionParams& aParams,
                                     const mozilla::UniquePtr<SelectionDetails>& aDetails,
                                     SelectionType aSelectionType);

  void DrawEmphasisMarks(gfxContext* aContext,
                         mozilla::WritingMode aWM,
                         const gfxPoint& aTextBaselinePt,
                         const gfxPoint& aFramePt, Range aRange,
                         const nscolor* aDecorationOverrideColor,
                         PropertyProvider* aProvider);

  virtual nscolor GetCaretColorAt(int32_t aOffset) override;

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
   * @param aRefDrawTarget the DrawTarget to use as a reference for creating the
   * textrun, if available (if not, we'll create one which will just be slower)
   * @param aLineContainer the block ancestor for this frame, or nullptr if
   * unknown
   * @param aFlowEndInTextRun if non-null, this returns the textrun offset of
   * end of the text associated with this frame and its in-flow siblings
   * @return a gfxSkipCharsIterator set up to map DOM offsets for this frame
   * to offsets into the textrun; its initial offset is set to this frame's
   * content offset
   */
  gfxSkipCharsIterator EnsureTextRun(TextRunType aWhichTextRun,
                                     DrawTarget* aRefDrawTarget = nullptr,
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
                  DrawTarget* aDrawTarget,
                  ReflowOutput& aMetrics, nsReflowStatus& aStatus);

  bool IsFloatingFirstLetterChild() const;

  bool IsInitialLetterChild() const;

  virtual bool ComputeCustomOverflow(nsOverflowAreas& aOverflowAreas) override;

  void AssignJustificationGaps(const mozilla::JustificationAssignment& aAssign);
  mozilla::JustificationAssignment GetJustificationAssignment() const;

  uint32_t CountGraphemeClusters() const;

protected:
  virtual ~nsTextFrame();

  RefPtr<gfxTextRun> mTextRun;
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

  /**
   * Return true if the frame is part of a Selection.
   * Helper method to implement the public IsSelected() API.
   */
  virtual bool IsFrameSelected() const override;

  mozilla::UniquePtr<SelectionDetails> GetSelectionDetails();

  void UnionAdditionalOverflow(nsPresContext* aPresContext,
                               nsIFrame* aBlock,
                               PropertyProvider& aProvider,
                               nsRect* aVisualOverflowRect,
                               bool aIncludeTextDecorations);

  // Update information of emphasis marks, and return the visial
  // overflow rect of the emphasis marks.
  nsRect UpdateTextEmphasis(mozilla::WritingMode aWM,
                            PropertyProvider& aProvider);

  struct PaintShadowParams
  {
    gfxTextRun::Range range;
    LayoutDeviceRect dirtyRect;
    gfxPoint framePt;
    gfxPoint textBaselinePt;
    gfxContext* context;
    nscolor foregroundColor = NS_RGBA(0, 0, 0, 0);
    const nsCharClipDisplayItem::ClipEdges* clipEdges = nullptr;
    PropertyProvider* provider = nullptr;
    nscoord leftSideOffset = 0;
    explicit PaintShadowParams(const PaintTextParams& aParams)
      : dirtyRect(aParams.dirtyRect)
      , framePt(aParams.framePt)
      , context(aParams.context) {}
  };

  void PaintOneShadow(const PaintShadowParams& aParams,
                      nsCSSShadowItem* aShadowDetails,
                      gfxRect& aBoundingBox,
                      uint32_t aBlurFlags);

  void PaintShadows(nsCSSShadowArray* aShadow,
                    const PaintShadowParams& aParams);

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
    AutoTArray<LineDecoration, 1> mOverlines, mUnderlines, mStrikes;

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

  void DrawTextRun(Range aRange, const gfxPoint& aTextBaselinePt,
                   const DrawTextRunParams& aParams);

  void DrawTextRunAndDecorations(Range aRange, const gfxPoint& aTextBaselinePt,
                                 const DrawTextParams& aParams,
                                 const TextDecorations& aDecorations);

  void DrawText(Range aRange, const gfxPoint& aTextBaselinePt,
                const DrawTextParams& aParams);

  // Set non empty rect to aRect, it should be overflow rect or frame rect.
  // If the result rect is larger than the given rect, this returns true.
  bool CombineSelectionUnderlineRect(nsPresContext* aPresContext,
                                       nsRect& aRect);

  /**
   * Utility methods to paint selection.
   */
  void DrawSelectionDecorations(gfxContext* aContext,
                                const LayoutDeviceRect& aDirtyRect,
                                mozilla::SelectionType aSelectionType,
                                nsTextPaintStyle& aTextPaintStyle,
                                const TextRangeStyle &aRangeStyle,
                                const Point& aPt,
                                gfxFloat aICoordInFrame,
                                gfxFloat aWidth,
                                gfxFloat aAscent,
                                const gfxFont::Metrics& aFontMetrics,
                                DrawPathCallbacks* aCallbacks,
                                bool aVertical,
                                gfxFloat aDecorationOffsetDir,
                                uint8_t aDecoration);

  struct PaintDecorationLineParams;
  void PaintDecorationLine(const PaintDecorationLineParams& aParams);
  /**
   * ComputeDescentLimitForSelectionUnderline() computes the most far position
   * where we can put selection underline.
   *
   * @return The maximum underline offset from the baseline (positive value
   *         means that the underline can put below the baseline).
   */
  gfxFloat ComputeDescentLimitForSelectionUnderline(
             nsPresContext* aPresContext,
             const gfxFont::Metrics& aFontMetrics);
  /**
   * This function encapsulates all knowledge of how selections affect
   * foreground and background colors.
   * @param aForeground the foreground color to use
   * @param aBackground the background color to use, or RGBA(0,0,0,0) if no
   *                    background should be painted
   * @return            true if the selection affects colors, false otherwise
   */
  static bool GetSelectionTextColors(SelectionType aSelectionType,
                                     nsTextPaintStyle& aTextPaintStyle,
                                     const TextRangeStyle &aRangeStyle,
                                     nscolor* aForeground,
                                     nscolor* aBackground);
  /**
   * ComputeSelectionUnderlineHeight() computes selection underline height of
   * the specified selection type from the font metrics.
   */
  static gfxFloat ComputeSelectionUnderlineHeight(
                    nsPresContext* aPresContext,
                    const gfxFont::Metrics& aFontMetrics,
                    SelectionType aSelectionType);

  ContentOffsets GetCharacterOffsetAtFramePointInternal(nsPoint aPoint,
                   bool aForInsertionPoint);

  void ClearFrameOffsetCache();

  virtual bool HasAnyNoncollapsedCharacters() override;

  void ClearMetrics(ReflowOutput& aMetrics);

  /**
   * UpdateIteratorFromOffset() updates the iterator from a given offset.
   * Also, aInOffset may be updated to cluster start if aInOffset isn't
   * the offset of cluster start.
   */
  void UpdateIteratorFromOffset(const PropertyProvider& aProperties,
                                int32_t& aInOffset,
                                gfxSkipCharsIterator& aIter);

  nsPoint GetPointFromIterator(const gfxSkipCharsIterator& aIter,
                               PropertyProvider& aProperties);
};

#endif

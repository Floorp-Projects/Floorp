/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTextFrame_h__
#define nsTextFrame_h__

#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/Text.h"
#include "nsDisplayList.h"
#include "nsIFrame.h"
#include "nsFrameSelection.h"
#include "nsSplittableFrame.h"
#include "nsLineBox.h"
#include "gfxSkipChars.h"
#include "gfxTextRun.h"
#include "nsDisplayList.h"
#include "nsFontMetrics.h"
#include "JustificationUtils.h"
#include "RubyUtils.h"

// Undo the windows.h damage
#if defined(XP_WIN) && defined(DrawText)
#  undef DrawText
#endif

class nsTextPaintStyle;
struct SelectionDetails;
class nsTextFragment;

namespace mozilla {
class SVGContextPaint;
class SVGTextFrame;
}  // namespace mozilla

class nsTextFrame : public nsIFrame {
  using nsDisplayText = mozilla::nsDisplayText;
  typedef mozilla::LayoutDeviceRect LayoutDeviceRect;
  typedef mozilla::SelectionTypeMask SelectionTypeMask;
  typedef mozilla::SelectionType SelectionType;
  typedef mozilla::TextRangeStyle TextRangeStyle;
  typedef mozilla::gfx::DrawTarget DrawTarget;
  typedef mozilla::gfx::Point Point;
  typedef mozilla::gfx::Rect Rect;
  typedef mozilla::gfx::Size Size;
  typedef gfxTextRun::Range Range;

 public:
  enum TextRunType : uint8_t;
  struct TabWidthStore;

  /**
   * An implementation of gfxTextRun::PropertyProvider that computes spacing and
   * hyphenation based on CSS properties for a text frame.
   */
  class MOZ_STACK_CLASS PropertyProvider final
      : public gfxTextRun::PropertyProvider {
    typedef gfxTextRun::Range Range;
    typedef gfxTextRun::HyphenType HyphenType;
    typedef mozilla::gfx::DrawTarget DrawTarget;

   public:
    /**
     * Use this constructor for reflow, when we don't know what text is
     * really mapped by the frame and we have a lot of other data around.
     *
     * @param aLength can be INT32_MAX to indicate we cover all the text
     * associated with aFrame up to where its flow chain ends in the given
     * textrun. If INT32_MAX is passed, justification and hyphen-related methods
     * cannot be called, nor can GetOriginalLength().
     */
    PropertyProvider(gfxTextRun* aTextRun, const nsStyleText* aTextStyle,
                     const nsTextFragment* aFrag, nsTextFrame* aFrame,
                     const gfxSkipCharsIterator& aStart, int32_t aLength,
                     nsIFrame* aLineContainer,
                     nscoord aOffsetFromBlockOriginForTabs,
                     nsTextFrame::TextRunType aWhichTextRun);

    /**
     * Use this constructor after the frame has been reflowed and we don't
     * have other data around. Gets everything from the frame. EnsureTextRun
     * *must* be called before this!!!
     */
    PropertyProvider(nsTextFrame* aFrame, const gfxSkipCharsIterator& aStart,
                     nsTextFrame::TextRunType aWhichTextRun,
                     nsFontMetrics* aFontMetrics);

    /**
     * As above, but assuming we want the inflated text run and associated
     * metrics.
     */
    PropertyProvider(nsTextFrame* aFrame, const gfxSkipCharsIterator& aStart)
        : PropertyProvider(aFrame, aStart, nsTextFrame::eInflated,
                           aFrame->InflatedFontMetrics()) {}

    // Call this after construction if you're not going to reflow the text
    void InitializeForDisplay(bool aTrimAfter);

    void InitializeForMeasure();

    void GetSpacing(Range aRange, Spacing* aSpacing) const final;
    gfxFloat GetHyphenWidth() const final;
    void GetHyphenationBreaks(Range aRange,
                              HyphenType* aBreakBefore) const final;
    mozilla::StyleHyphens GetHyphensOption() const final {
      return mTextStyle->mHyphens;
    }

    already_AddRefed<DrawTarget> GetDrawTarget() const final;

    uint32_t GetAppUnitsPerDevUnit() const final {
      return mTextRun->GetAppUnitsPerDevUnit();
    }

    void GetSpacingInternal(Range aRange, Spacing* aSpacing,
                            bool aIgnoreTabs) const;

    /**
     * Compute the justification information in given DOM range, return
     * justification info and assignments if requested.
     */
    mozilla::JustificationInfo ComputeJustification(
        Range aRange,
        nsTArray<mozilla::JustificationAssignment>* aAssignments = nullptr);

    const nsTextFrame* GetFrame() const { return mFrame; }
    // This may not be equal to the frame offset/length in because we may have
    // adjusted for whitespace trimming according to the state bits set in the
    // frame (for the static provider)
    const gfxSkipCharsIterator& GetStart() const { return mStart; }
    // May return INT32_MAX if that was given to the constructor
    uint32_t GetOriginalLength() const {
      NS_ASSERTION(mLength != INT32_MAX, "Length not known");
      return mLength;
    }
    const nsTextFragment* GetFragment() const { return mFrag; }

    gfxFontGroup* GetFontGroup() const {
      if (!mFontGroup) {
        mFontGroup = GetFontMetrics()->GetThebesFontGroup();
      }
      return mFontGroup;
    }

    nsFontMetrics* GetFontMetrics() const {
      if (!mFontMetrics) {
        InitFontGroupAndFontMetrics();
      }
      return mFontMetrics;
    }

    void CalcTabWidths(Range aTransformedRange, gfxFloat aTabWidth) const;

    gfxFloat MinTabAdvance() const;

    const gfxSkipCharsIterator& GetEndHint() const { return mTempIterator; }

   protected:
    void SetupJustificationSpacing(bool aPostReflow);

    void InitFontGroupAndFontMetrics() const;

    const RefPtr<gfxTextRun> mTextRun;
    mutable gfxFontGroup* mFontGroup;
    mutable RefPtr<nsFontMetrics> mFontMetrics;
    const nsStyleText* mTextStyle;
    const nsTextFragment* mFrag;
    const nsIFrame* mLineContainer;
    nsTextFrame* mFrame;
    gfxSkipCharsIterator mStart;  // Offset in original and transformed string
    const gfxSkipCharsIterator mTempIterator;

    // Either null, or pointing to the frame's TabWidthProperty.
    mutable nsTextFrame::TabWidthStore* mTabWidths;
    // How far we've done tab-width calculation; this is ONLY valid when
    // mTabWidths is nullptr (otherwise rely on mTabWidths->mLimit instead).
    // It's a DOM offset relative to the current frame's offset.
    mutable uint32_t mTabWidthsAnalyzedLimit;

    int32_t mLength;                  // DOM string length, may be INT32_MAX
    const gfxFloat mWordSpacing;      // space for each whitespace char
    const gfxFloat mLetterSpacing;    // space for each letter
    mutable gfxFloat mMinTabAdvance;  // min advance for <tab> char
    mutable gfxFloat mHyphenWidth;
    mutable gfxFloat mOffsetFromBlockOriginForTabs;

    // The values in mJustificationSpacings corresponds to unskipped
    // characters start from mJustificationArrayStart.
    uint32_t mJustificationArrayStart;
    nsTArray<Spacing> mJustificationSpacings;

    const bool mReflowing;
    const nsTextFrame::TextRunType mWhichTextRun;
  };

  explicit nsTextFrame(ComputedStyle* aStyle, nsPresContext* aPresContext,
                       ClassID aID = kClassID)
      : nsIFrame(aStyle, aPresContext, aID),
        mNextContinuation(nullptr),
        mContentOffset(0),
        mContentLengthHint(0),
        mAscent(0),
        mIsSelected(SelectionState::Unknown) {}

  NS_DECL_FRAMEARENA_HELPERS(nsTextFrame)

  friend class nsContinuingTextFrame;

  // nsQueryFrame
  NS_DECL_QUERYFRAME

  NS_DECLARE_FRAME_PROPERTY_DELETABLE(ContinuationsProperty,
                                      nsTArray<nsTextFrame*>)

  // nsIFrame
  void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                        const nsDisplayListSet& aLists) final;

  void Init(nsIContent* aContent, nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;

  void DestroyFrom(nsIFrame* aDestructRoot,
                   PostDestroyData& aPostDestroyData) override;

  mozilla::Maybe<Cursor> GetCursor(const nsPoint&) final;

  nsresult CharacterDataChanged(const CharacterDataChangeInfo&) final;

  nsTextFrame* FirstContinuation() const override {
    return const_cast<nsTextFrame*>(this);
  }
  nsTextFrame* GetPrevContinuation() const override { return nullptr; }
  nsTextFrame* GetNextContinuation() const final { return mNextContinuation; }
  void SetNextContinuation(nsIFrame* aNextContinuation) final {
    NS_ASSERTION(!aNextContinuation || Type() == aNextContinuation->Type(),
                 "setting a next continuation with incorrect type!");
    NS_ASSERTION(
        !nsSplittableFrame::IsInNextContinuationChain(aNextContinuation, this),
        "creating a loop in continuation chain!");
    mNextContinuation = static_cast<nsTextFrame*>(aNextContinuation);
    if (aNextContinuation)
      aNextContinuation->RemoveStateBits(NS_FRAME_IS_FLUID_CONTINUATION);
    // Setting a non-fluid continuation might affect our flow length (they're
    // quite rare so we assume it always does) so we delete our cached value:
    if (GetContent()->HasFlag(NS_HAS_FLOWLENGTH_PROPERTY)) {
      GetContent()->RemoveProperty(nsGkAtoms::flowlength);
      GetContent()->UnsetFlags(NS_HAS_FLOWLENGTH_PROPERTY);
    }
  }
  nsTextFrame* GetNextInFlow() const final {
    return mNextContinuation && mNextContinuation->HasAnyStateBits(
                                    NS_FRAME_IS_FLUID_CONTINUATION)
               ? mNextContinuation
               : nullptr;
  }
  void SetNextInFlow(nsIFrame* aNextInFlow) final {
    NS_ASSERTION(!aNextInFlow || Type() == aNextInFlow->Type(),
                 "setting a next in flow with incorrect type!");
    NS_ASSERTION(
        !nsSplittableFrame::IsInNextContinuationChain(aNextInFlow, this),
        "creating a loop in continuation chain!");
    mNextContinuation = static_cast<nsTextFrame*>(aNextInFlow);
    if (mNextContinuation &&
        !mNextContinuation->HasAnyStateBits(NS_FRAME_IS_FLUID_CONTINUATION)) {
      // Changing from non-fluid to fluid continuation might affect our flow
      // length, so we delete our cached value:
      if (GetContent()->HasFlag(NS_HAS_FLOWLENGTH_PROPERTY)) {
        GetContent()->RemoveProperty(nsGkAtoms::flowlength);
        GetContent()->UnsetFlags(NS_HAS_FLOWLENGTH_PROPERTY);
      }
    }
    if (aNextInFlow) {
      aNextInFlow->AddStateBits(NS_FRAME_IS_FLUID_CONTINUATION);
    }
  }
  nsTextFrame* LastInFlow() const final;
  nsTextFrame* LastContinuation() const final;

  bool IsFrameOfType(uint32_t aFlags) const final {
    // Set the frame state bit for text frames to mark them as replaced.
    // XXX kipp: temporary
    return nsIFrame::IsFrameOfType(
        aFlags & ~(nsIFrame::eReplaced | nsIFrame::eLineParticipant));
  }

  bool ShouldSuppressLineBreak() const {
    // If the parent frame of the text frame is ruby content box, it must
    // suppress line break inside. This check is necessary, because when
    // a whitespace is only contained by pseudo ruby frames, its style
    // context won't have SuppressLineBreak bit set.
    if (mozilla::RubyUtils::IsRubyContentBox(GetParent()->Type())) {
      return true;
    }
    return Style()->ShouldSuppressLineBreak();
  }

  void InvalidateFrame(uint32_t aDisplayItemKey = 0,
                       bool aRebuildDisplayItems = true) final;
  void InvalidateFrameWithRect(const nsRect& aRect,
                               uint32_t aDisplayItemKey = 0,
                               bool aRebuildDisplayItems = true) final;

#ifdef DEBUG_FRAME_DUMP
  void List(FILE* out = stderr, const char* aPrefix = "",
            ListFlags aFlags = ListFlags()) const final;
  nsresult GetFrameName(nsAString& aResult) const final;
  void ToCString(nsCString& aBuf, int32_t* aTotalContentLength) const;
  void ListTextRuns(FILE* out, nsTHashSet<const void*>& aSeen) const final;
#endif

  // Returns this text frame's content's text fragment.
  //
  // Assertions in Init() ensure we only ever get a Text node as content.
  const nsTextFragment* TextFragment() const {
    return &mContent->AsText()->TextFragment();
  }

  ContentOffsets CalcContentOffsetsFromFramePoint(const nsPoint& aPoint) final;
  ContentOffsets GetCharacterOffsetAtFramePoint(const nsPoint& aPoint);

  /**
   * This is called only on the primary text frame. It indicates that
   * the selection state of the given character range has changed.
   * Frames corresponding to the character range are unconditionally invalidated
   * (Selection::Repaint depends on this).
   * @param aStart start of character range.
   * @param aEnd end (exclusive) of character range.
   * @param aSelected true iff the character range is now selected.
   * @param aType the type of the changed selection.
   */
  void SelectionStateChanged(uint32_t aStart, uint32_t aEnd, bool aSelected,
                             SelectionType aSelectionType);

  FrameSearchResult PeekOffsetNoAmount(bool aForward, int32_t* aOffset) final;
  FrameSearchResult PeekOffsetCharacter(
      bool aForward, int32_t* aOffset,
      PeekOffsetCharacterOptions aOptions = PeekOffsetCharacterOptions()) final;
  FrameSearchResult PeekOffsetWord(bool aForward, bool aWordSelectEatSpace,
                                   bool aIsKeyboardSelect, int32_t* aOffset,
                                   PeekWordState* aState,
                                   bool aTrimSpaces) final;

  nsresult CheckVisibility(nsPresContext* aContext, int32_t aStartIndex,
                           int32_t aEndIndex, bool aRecurse, bool* aFinished,
                           bool* _retval) final;

  // Flags for aSetLengthFlags
  enum { ALLOW_FRAME_CREATION_AND_DESTRUCTION = 0x01 };

  // Update offsets to account for new length. This may clear mTextRun.
  void SetLength(int32_t aLength, nsLineLayout* aLineLayout,
                 uint32_t aSetLengthFlags = 0);

  std::pair<int32_t, int32_t> GetOffsets() const final;

  void AdjustOffsetsForBidi(int32_t start, int32_t end) final;

  nsresult GetPointFromOffset(int32_t inOffset, nsPoint* outPoint) final;
  nsresult GetCharacterRectsInRange(int32_t aInOffset, int32_t aLength,
                                    nsTArray<nsRect>& aRects) final;

  nsresult GetChildFrameContainingOffset(int32_t inContentOffset, bool inHint,
                                         int32_t* outFrameContentOffset,
                                         nsIFrame** outChildFrame) final;

  bool IsEmpty() final;
  bool IsSelfEmpty() final { return IsEmpty(); }
  nscoord GetLogicalBaseline(mozilla::WritingMode aWritingMode) const final;

  bool HasSignificantTerminalNewline() const final;

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
    return HasAnyStateBits(TEXT_HAS_NONCOLLAPSED_CHARACTERS);
  }

#ifdef ACCESSIBILITY
  mozilla::a11y::AccType AccessibleType() final;
#endif

  float GetFontSizeInflation() const;
  bool IsCurrentFontInflation(float aInflation) const;
  bool HasFontSizeInflation() const {
    return HasAnyStateBits(TEXT_HAS_FONT_INFLATION);
  }
  void SetFontSizeInflation(float aInflation);

  void MarkIntrinsicISizesDirty() final;
  nscoord GetMinISize(gfxContext* aRenderingContext) final;
  nscoord GetPrefISize(gfxContext* aRenderingContext) final;
  void AddInlineMinISize(gfxContext* aRenderingContext,
                         InlineMinISizeData* aData) override;
  void AddInlinePrefISize(gfxContext* aRenderingContext,
                          InlinePrefISizeData* aData) override;
  SizeComputationResult ComputeSize(
      gfxContext* aRenderingContext, mozilla::WritingMode aWM,
      const mozilla::LogicalSize& aCBSize, nscoord aAvailableISize,
      const mozilla::LogicalSize& aMargin,
      const mozilla::LogicalSize& aBorderPadding,
      const mozilla::StyleSizeOverrides& aSizeOverrides,
      mozilla::ComputeSizeFlags aFlags) final;
  nsRect ComputeTightBounds(DrawTarget* aDrawTarget) const final;
  nsresult GetPrefWidthTightBounds(gfxContext* aContext, nscoord* aX,
                                   nscoord* aXMost) final;
  void Reflow(nsPresContext* aPresContext, ReflowOutput& aMetrics,
              const ReflowInput& aReflowInput, nsReflowStatus& aStatus) final;
  bool CanContinueTextRun() const final;
  // Method that is called for a text frame that is logically
  // adjacent to the end of the line (i.e. followed only by empty text frames,
  // placeholders or inlines containing such).
  struct TrimOutput {
    // true if we trimmed some space or changed metrics in some other way.
    // In this case, we should call RecomputeOverflow on this frame.
    bool mChanged;
    // an amount to *subtract* from the frame's width (zero if !mChanged)
    nscoord mDeltaWidth;
  };
  TrimOutput TrimTrailingWhiteSpace(DrawTarget* aDrawTarget);
  RenderedText GetRenderedText(
      uint32_t aStartOffset = 0, uint32_t aEndOffset = UINT32_MAX,
      TextOffsetType aOffsetType = TextOffsetType::OffsetsInContentText,
      TrailingWhitespace aTrimTrailingWhitespace =
          TrailingWhitespace::Trim) final;

  mozilla::OverflowAreas RecomputeOverflow(nsIFrame* aBlockFrame,
                                           bool aIncludeShadows = true);

  enum TextRunType : uint8_t {
    // Anything in reflow (but not intrinsic width calculation) or
    // painting should use the inflated text run (i.e., with font size
    // inflation applied).
    eInflated,
    // Intrinsic width calculation should use the non-inflated text run.
    // When there is font size inflation, it will be different.
    eNotInflated
  };

  void AddInlineMinISizeForFlow(gfxContext* aRenderingContext,
                                nsIFrame::InlineMinISizeData* aData,
                                TextRunType aTextRunType);
  void AddInlinePrefISizeForFlow(gfxContext* aRenderingContext,
                                 InlinePrefISizeData* aData,
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
   * Return true if this box has some text to display.
   * It returns false if at least one of these conditions are met:
   * a. the frame hasn't been reflowed yet
   * b. GetContentLength() == 0
   * c. it contains only non-significant white-space
   */
  bool HasNonSuppressedText() const;

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
   * The color of each part of the frame's text rendering is passed as an
   * argument to the NotifyBefore* callback for that part.  The nscolor can take
   * on one of the three selection special colors defined in LookAndFeel.h --
   * NS_TRANSPARENT, NS_SAME_AS_FOREGROUND_COLOR and
   * NS_40PERCENT_FOREGROUND_COLOR.
   */
  struct DrawPathCallbacks : gfxTextRunDrawCallbacks {
    /**
     * @param aShouldPaintSVGGlyphs Whether SVG glyphs should be painted.
     */
    explicit DrawPathCallbacks(bool aShouldPaintSVGGlyphs = false)
        : gfxTextRunDrawCallbacks(aShouldPaintSVGGlyphs) {}

    /**
     * Called to have the selection highlight drawn before the text is drawn
     * over the top.
     */
    virtual void NotifySelectionBackgroundNeedsFill(const Rect& aBackgroundRect,
                                                    nscolor aColor,
                                                    DrawTarget& aDrawTarget) {}

    /**
     * Called before (for under/over-line) or after (for line-through) the text
     * is drawn to have a text decoration line drawn.
     */
    virtual void PaintDecorationLine(Rect aPath, nscolor aColor) {}

    /**
     * Called after selected text is drawn to have a decoration line drawn over
     * the text. (All types of text decoration are drawn after the text when
     * text is selected.)
     */
    virtual void PaintSelectionDecorationLine(Rect aPath, nscolor aColor) {}

    /**
     * Called just before any paths have been emitted to the gfxContext
     * for the glyphs of the frame's text.
     */
    virtual void NotifyBeforeText(nscolor aColor) {}

    /**
     * Called just after all the paths have been emitted to the gfxContext
     * for the glyphs of the frame's text.
     */
    virtual void NotifyAfterText() {}

    /**
     * Called just before a path corresponding to a selection decoration line
     * has been emitted to the gfxContext.
     */
    virtual void NotifyBeforeSelectionDecorationLine(nscolor aColor) {}

    /**
     * Called just after a path corresponding to a selection decoration line
     * has been emitted to the gfxContext.
     */
    virtual void NotifySelectionDecorationLinePathEmitted() {}
  };

  struct PaintTextParams {
    gfxContext* context;
    mozilla::gfx::Point framePt;
    LayoutDeviceRect dirtyRect;
    mozilla::SVGContextPaint* contextPaint = nullptr;
    DrawPathCallbacks* callbacks = nullptr;
    enum {
      PaintText,        // Normal text painting.
      GenerateTextMask  // To generate a mask from a text frame. Should
                        // only paint text itself with opaque color.
                        // Text shadow, text selection color and text
                        // decoration are all discarded in this state.
    };
    uint8_t state = PaintText;
    explicit PaintTextParams(gfxContext* aContext) : context(aContext) {}

    bool IsPaintText() const { return state == PaintText; }
    bool IsGenerateTextMask() const { return state == GenerateTextMask; }
  };

  struct PaintTextSelectionParams : PaintTextParams {
    mozilla::gfx::Point textBaselinePt;
    PropertyProvider* provider = nullptr;
    Range contentRange;
    nsTextPaintStyle* textPaintStyle = nullptr;
    Range glyphRange;
    explicit PaintTextSelectionParams(const PaintTextParams& aParams)
        : PaintTextParams(aParams) {}
  };

  struct DrawTextRunParams {
    gfxContext* context;
    PropertyProvider* provider = nullptr;
    gfxFloat* advanceWidth = nullptr;
    mozilla::SVGContextPaint* contextPaint = nullptr;
    DrawPathCallbacks* callbacks = nullptr;
    nscolor textColor = NS_RGBA(0, 0, 0, 0);
    nscolor textStrokeColor = NS_RGBA(0, 0, 0, 0);
    float textStrokeWidth = 0.0f;
    bool drawSoftHyphen = false;
    explicit DrawTextRunParams(gfxContext* aContext) : context(aContext) {}
  };

  struct DrawTextParams : DrawTextRunParams {
    mozilla::gfx::Point framePt;
    LayoutDeviceRect dirtyRect;
    const nsTextPaintStyle* textStyle = nullptr;
    const nsDisplayText::ClipEdges* clipEdges = nullptr;
    const nscolor* decorationOverrideColor = nullptr;
    Range glyphRange;
    explicit DrawTextParams(gfxContext* aContext)
        : DrawTextRunParams(aContext) {}
  };

  // Primary frame paint method called from nsDisplayText.  Can also be used
  // to generate paths rather than paint the frame's text by passing a callback
  // object.  The private DrawText() is what applies the text to a graphics
  // context.
  void PaintText(const PaintTextParams& aParams, const nscoord aVisIStartEdge,
                 const nscoord aVisIEndEdge, const nsPoint& aToReferenceFrame,
                 const bool aIsSelected, float aOpacity = 1.0f);
  // helper: paint text frame when we're impacted by at least one selection.
  // Return false if the text was not painted and we should continue with
  // the fast path.
  bool PaintTextWithSelection(const PaintTextSelectionParams& aParams,
                              const nsDisplayText::ClipEdges& aClipEdges);
  // helper: paint text with foreground and background colors determined
  // by selection(s). Also computes a mask of all selection types applying to
  // our text, returned in aAllSelectionTypeMask.
  // Return false if the text was not painted and we should continue with
  // the fast path.
  bool PaintTextWithSelectionColors(
      const PaintTextSelectionParams& aParams,
      const mozilla::UniquePtr<SelectionDetails>& aDetails,
      SelectionTypeMask* aAllSelectionTypeMask,
      const nsDisplayText::ClipEdges& aClipEdges);
  // helper: paint text decorations for text selected by aSelectionType
  void PaintTextSelectionDecorations(
      const PaintTextSelectionParams& aParams,
      const mozilla::UniquePtr<SelectionDetails>& aDetails,
      SelectionType aSelectionType);

  void DrawEmphasisMarks(gfxContext* aContext, mozilla::WritingMode aWM,
                         const mozilla::gfx::Point& aTextBaselinePt,
                         const mozilla::gfx::Point& aFramePt, Range aRange,
                         const nscolor* aDecorationOverrideColor,
                         PropertyProvider* aProvider);

  nscolor GetCaretColorAt(int32_t aOffset) final;

  // @param aSelectionFlags may be multiple of nsISelectionDisplay::DISPLAY_*.
  // @return nsISelectionController.idl's `getDisplaySelection`.
  int16_t GetSelectionStatus(int16_t* aSelectionFlags);

  int32_t GetContentOffset() const { return mContentOffset; }
  int32_t GetContentLength() const {
    NS_ASSERTION(GetContentEnd() - mContentOffset >= 0, "negative length");
    return GetContentEnd() - mContentOffset;
  }
  int32_t GetContentEnd() const;
  // This returns the length the frame thinks it *should* have after it was
  // last reflowed (0 if it hasn't been reflowed yet). This should be used only
  // when setting up the text offsets for a new continuation frame.
  int32_t GetContentLengthHint() const { return mContentLengthHint; }

  // Compute the length of the content mapped by this frame
  // and all its in-flow siblings. Basically this means starting at
  // mContentOffset and going to the end of the text node or the next bidi
  // continuation boundary.
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
  gfxSkipCharsIterator EnsureTextRun(
      TextRunType aWhichTextRun, DrawTarget* aRefDrawTarget = nullptr,
      nsIFrame* aLineContainer = nullptr,
      const nsLineList::iterator* aLine = nullptr,
      uint32_t* aFlowEndInTextRun = nullptr);

  gfxTextRun* GetTextRun(TextRunType aWhichTextRun) const {
    if (aWhichTextRun == eInflated || !HasFontSizeInflation()) return mTextRun;
    return GetUninflatedTextRun();
  }
  gfxTextRun* GetUninflatedTextRun() const;
  void SetTextRun(gfxTextRun* aTextRun, TextRunType aWhichTextRun,
                  float aInflation);
  bool IsInTextRunUserData() const {
    return HasAnyStateBits(TEXT_IN_TEXTRUN_USER_DATA |
                           TEXT_IN_UNINFLATED_TEXTRUN_USER_DATA);
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
   * is nsTextFrame::eNotInflated and there is inflation) from all frames that
   * hold a reference to it, starting at |aStartContinuation|, or if it's
   * nullptr, starting at |this|.  Deletes the text run if all references
   * were cleared and it's not cached.
   */
  void ClearTextRun(nsTextFrame* aStartContinuation, TextRunType aWhichTextRun);

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
  enum class TrimmedOffsetFlags : uint8_t {
    Default = 0,
    NotPostReflow = 1 << 0,
    NoTrimAfter = 1 << 1,
    NoTrimBefore = 1 << 2
  };
  TrimmedOffsets GetTrimmedOffsets(
      const nsTextFragment* aFrag,
      TrimmedOffsetFlags aFlags = TrimmedOffsetFlags::Default) const;

  // Similar to Reflow(), but for use from nsLineLayout
  void ReflowText(nsLineLayout& aLineLayout, nscoord aAvailableWidth,
                  DrawTarget* aDrawTarget, ReflowOutput& aMetrics,
                  nsReflowStatus& aStatus);

  bool IsFloatingFirstLetterChild() const;

  bool IsInitialLetterChild() const;

  bool ComputeCustomOverflow(mozilla::OverflowAreas& aOverflowAreas) final;
  bool ComputeCustomOverflowInternal(mozilla::OverflowAreas& aOverflowAreas,
                                     bool aIncludeShadows);

  void AssignJustificationGaps(const mozilla::JustificationAssignment& aAssign);
  mozilla::JustificationAssignment GetJustificationAssignment() const;

  uint32_t CountGraphemeClusters() const;

  bool HasAnyNoncollapsedCharacters() final;

  /**
   * Call this after you have manually changed the text node contents without
   * notifying that change.  This behaves as if all the text contents changed.
   * (You should only use this for native anonymous content.)
   */
  void NotifyNativeAnonymousTextnodeChange(uint32_t aOldLength);

  void SetInflatedFontMetrics(nsFontMetrics* aMetrics) {
    mFontMetrics = aMetrics;
  }
  nsFontMetrics* InflatedFontMetrics() const { return mFontMetrics; }

  nsRect WebRenderBounds();

  // Find the continuation (which may be this frame itself) containing the
  // given offset. Note that this may return null, if the offset is beyond the
  // text covered by the continuation chain.
  // (To be used only on the first textframe in the chain.)
  nsTextFrame* FindContinuationForOffset(int32_t aOffset);

 protected:
  virtual ~nsTextFrame();

  friend class mozilla::nsDisplayTextGeometry;
  friend class mozilla::nsDisplayText;

  RefPtr<nsFontMetrics> mFontMetrics;
  RefPtr<gfxTextRun> mTextRun;
  nsTextFrame* mNextContinuation;
  // The key invariant here is that mContentOffset never decreases along
  // a next-continuation chain. And of course mContentOffset is always <= the
  // the text node's content length, and the mContentOffset for the first frame
  // is always 0. Furthermore the text mapped by a frame is determined by
  // GetContentOffset() and GetContentLength()/GetContentEnd(), which get
  // the length from the difference between this frame's offset and the next
  // frame's offset, or the text length if there is no next frame. This means
  // the frames always map the text node without overlapping or leaving any
  // gaps.
  int32_t mContentOffset;
  // This does *not* indicate the length of text currently mapped by the frame;
  // instead it's a hint saying that this frame *wants* to map this much text
  // so if we create a new continuation, this is where that continuation should
  // start.
  int32_t mContentLengthHint;
  nscoord mAscent;

  // Cached selection state.
  enum class SelectionState : uint8_t {
    Unknown,
    Selected,
    NotSelected,
  };
  mutable SelectionState mIsSelected;

  // Whether a cached continuations array is present.
  bool mHasContinuationsProperty = false;

  /**
   * Return true if the frame is part of a Selection.
   * Helper method to implement the public IsSelected() API.
   */
  bool IsFrameSelected() const final;

  void InvalidateSelectionState() { mIsSelected = SelectionState::Unknown; }

  mozilla::UniquePtr<SelectionDetails> GetSelectionDetails();

  void UnionAdditionalOverflow(nsPresContext* aPresContext, nsIFrame* aBlock,
                               PropertyProvider& aProvider,
                               nsRect* aInkOverflowRect,
                               bool aIncludeTextDecorations,
                               bool aIncludeShadows);

  // Update information of emphasis marks, and return the visial
  // overflow rect of the emphasis marks.
  nsRect UpdateTextEmphasis(mozilla::WritingMode aWM,
                            PropertyProvider& aProvider);

  struct PaintShadowParams {
    gfxTextRun::Range range;
    LayoutDeviceRect dirtyRect;
    mozilla::gfx::Point framePt;
    mozilla::gfx::Point textBaselinePt;
    gfxContext* context;
    nscolor foregroundColor = NS_RGBA(0, 0, 0, 0);
    const nsDisplayText::ClipEdges* clipEdges = nullptr;
    PropertyProvider* provider = nullptr;
    nscoord leftSideOffset = 0;
    explicit PaintShadowParams(const PaintTextParams& aParams)
        : dirtyRect(aParams.dirtyRect),
          framePt(aParams.framePt),
          context(aParams.context) {}
  };

  void PaintOneShadow(const PaintShadowParams& aParams,
                      const mozilla::StyleSimpleShadow& aShadowDetails,
                      gfxRect& aBoundingBox, uint32_t aBlurFlags);

  void PaintShadows(mozilla::Span<const mozilla::StyleSimpleShadow>,
                    const PaintShadowParams& aParams);

  struct LineDecoration {
    nsIFrame* mFrame;

    // This is represents the offset from our baseline to mFrame's baseline;
    // positive offsets are *above* the baseline and negative offsets below
    nscoord mBaselineOffset;

    // This represents the offset from the initial position of the underline
    const mozilla::LengthPercentageOrAuto mTextUnderlineOffset;

    // for CSS property text-decoration-thickness, the width refers to the
    // thickness of the decoration line
    const mozilla::StyleTextDecorationLength mTextDecorationThickness;
    nscolor mColor;
    uint8_t mStyle;

    // The text-underline-position property; affects the underline offset only
    // if mTextUnderlineOffset is auto.
    const mozilla::StyleTextUnderlinePosition mTextUnderlinePosition;

    LineDecoration(nsIFrame* const aFrame, const nscoord aOff,
                   mozilla::StyleTextUnderlinePosition aUnderlinePosition,
                   const mozilla::LengthPercentageOrAuto& aUnderlineOffset,
                   const mozilla::StyleTextDecorationLength& aDecThickness,
                   const nscolor aColor, const uint8_t aStyle)
        : mFrame(aFrame),
          mBaselineOffset(aOff),
          mTextUnderlineOffset(aUnderlineOffset),
          mTextDecorationThickness(aDecThickness),
          mColor(aColor),
          mStyle(aStyle),
          mTextUnderlinePosition(aUnderlinePosition) {}

    LineDecoration(const LineDecoration& aOther) = default;

    bool operator==(const LineDecoration& aOther) const {
      return mFrame == aOther.mFrame && mStyle == aOther.mStyle &&
             mColor == aOther.mColor &&
             mBaselineOffset == aOther.mBaselineOffset &&
             mTextUnderlinePosition == aOther.mTextUnderlinePosition &&
             mTextUnderlineOffset == aOther.mTextUnderlineOffset &&
             mTextDecorationThickness == aOther.mTextDecorationThickness;
    }

    bool operator!=(const LineDecoration& aOther) const {
      return !(*this == aOther);
    }
  };
  struct TextDecorations {
    AutoTArray<LineDecoration, 1> mOverlines, mUnderlines, mStrikes;

    TextDecorations() = default;

    bool HasDecorationLines() const {
      return HasUnderline() || HasOverline() || HasStrikeout();
    }
    bool HasUnderline() const { return !mUnderlines.IsEmpty(); }
    bool HasOverline() const { return !mOverlines.IsEmpty(); }
    bool HasStrikeout() const { return !mStrikes.IsEmpty(); }
    bool operator==(const TextDecorations& aOther) const {
      return mOverlines == aOther.mOverlines &&
             mUnderlines == aOther.mUnderlines && mStrikes == aOther.mStrikes;
    }
    bool operator!=(const TextDecorations& aOther) const {
      return !(*this == aOther);
    }
  };
  enum TextDecorationColorResolution { eResolvedColors, eUnresolvedColors };
  void GetTextDecorations(nsPresContext* aPresContext,
                          TextDecorationColorResolution aColorResolution,
                          TextDecorations& aDecorations);

  void DrawTextRun(Range aRange, const mozilla::gfx::Point& aTextBaselinePt,
                   const DrawTextRunParams& aParams);

  void DrawTextRunAndDecorations(Range aRange,
                                 const mozilla::gfx::Point& aTextBaselinePt,
                                 const DrawTextParams& aParams,
                                 const TextDecorations& aDecorations);

  void DrawText(Range aRange, const mozilla::gfx::Point& aTextBaselinePt,
                const DrawTextParams& aParams);

  // Set non empty rect to aRect, it should be overflow rect or frame rect.
  // If the result rect is larger than the given rect, this returns true.
  bool CombineSelectionUnderlineRect(nsPresContext* aPresContext,
                                     nsRect& aRect);

  /**
   * Utility methods to paint selection.
   */
  void DrawSelectionDecorations(
      gfxContext* aContext, const LayoutDeviceRect& aDirtyRect,
      mozilla::SelectionType aSelectionType, nsTextPaintStyle& aTextPaintStyle,
      const TextRangeStyle& aRangeStyle, const Point& aPt,
      gfxFloat aICoordInFrame, gfxFloat aWidth, gfxFloat aAscent,
      const gfxFont::Metrics& aFontMetrics, DrawPathCallbacks* aCallbacks,
      bool aVertical, mozilla::StyleTextDecorationLine aDecoration);

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
      nsPresContext* aPresContext, const gfxFont::Metrics& aFontMetrics);
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
                                     const TextRangeStyle& aRangeStyle,
                                     nscolor* aForeground,
                                     nscolor* aBackground);
  /**
   * ComputeSelectionUnderlineHeight() computes selection underline height of
   * the specified selection type from the font metrics.
   */
  static gfxFloat ComputeSelectionUnderlineHeight(
      nsPresContext* aPresContext, const gfxFont::Metrics& aFontMetrics,
      SelectionType aSelectionType);

  ContentOffsets GetCharacterOffsetAtFramePointInternal(
      const nsPoint& aPoint, bool aForInsertionPoint);

  static float GetTextCombineScaleFactor(nsTextFrame* aFrame);

  void ClearFrameOffsetCache();

  void ClearMetrics(ReflowOutput& aMetrics);

  // Return pointer to an array of all frames in the continuation chain, or
  // null if we're too short of memory.
  nsTArray<nsTextFrame*>* GetContinuations();

  // Clear any cached continuations array; this should be called whenever the
  // chain is modified.
  void ClearCachedContinuations() {
    MOZ_ASSERT(NS_IsMainThread());
    if (mHasContinuationsProperty) {
      RemoveProperty(ContinuationsProperty());
      mHasContinuationsProperty = false;
    }
  }

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

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(nsTextFrame::TrimmedOffsetFlags)

#endif

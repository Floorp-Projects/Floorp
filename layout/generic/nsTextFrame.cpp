/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for textual content of elements */

#include "nsTextFrame.h"

#include "gfx2DGlue.h"
#include "gfxPrefs.h"
#include "gfxUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/Likely.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/TextEvents.h"
#include "mozilla/BinarySearch.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/Unused.h"
#include "mozilla/PodOperations.h"

#include "nsCOMPtr.h"
#include "nsBlockFrame.h"
#include "nsFontMetrics.h"
#include "nsSplittableFrame.h"
#include "nsLineLayout.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsPresContext.h"
#include "nsIContent.h"
#include "nsStyleConsts.h"
#include "nsStyleContext.h"
#include "nsStyleStruct.h"
#include "nsStyleStructInlines.h"
#include "SVGTextFrame.h"
#include "nsCoord.h"
#include "gfxContext.h"
#include "nsIPresShell.h"
#include "nsTArray.h"
#include "nsCSSPseudoElements.h"
#include "nsCSSFrameConstructor.h"
#include "nsCompatibility.h"
#include "nsCSSColorUtils.h"
#include "nsLayoutUtils.h"
#include "nsDisplayList.h"
#include "nsFrame.h"
#include "nsIMathMLFrame.h"
#include "nsPlaceholderFrame.h"
#include "nsTextFrameUtils.h"
#include "nsTextRunTransformations.h"
#include "MathMLTextRunFactory.h"
#include "nsUnicodeProperties.h"
#include "nsStyleUtil.h"
#include "nsRubyFrame.h"
#include "TextDrawTarget.h"

#include "nsTextFragment.h"
#include "nsGkAtoms.h"
#include "nsFrameSelection.h"
#include "nsRange.h"
#include "nsCSSRendering.h"
#include "nsContentUtils.h"
#include "nsLineBreaker.h"
#include "nsIFrameInlines.h"
#include "mozilla/intl/WordBreaker.h"
#include "mozilla/StyleSetHandle.h"
#include "mozilla/StyleSetHandleInlines.h"
#include "mozilla/layers/LayersMessages.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "mozilla/layers/StackingContextHelper.h"

#include <algorithm>
#include <limits>
#ifdef ACCESSIBILITY
#include "nsAccessibilityService.h"
#endif

#include "nsPrintfCString.h"

#include "gfxContext.h"
#include "mozilla/gfx/DrawTargetRecording.h"

#include "mozilla/UniquePtr.h"
#include "mozilla/dom/Element.h"
#include "mozilla/LookAndFeel.h"

#include "GeckoProfiler.h"

#ifdef DEBUG
#undef NOISY_REFLOW
#undef NOISY_TRIM
#else
#undef NOISY_REFLOW
#undef NOISY_TRIM
#endif

#ifdef DrawText
#undef DrawText
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::layers;

typedef mozilla::layout::TextDrawTarget TextDrawTarget;

struct TabWidth {
  TabWidth(uint32_t aOffset, uint32_t aWidth)
    : mOffset(aOffset), mWidth(float(aWidth))
  { }

  uint32_t mOffset; // DOM offset relative to the current frame's offset.
  float    mWidth;  // extra space to be added at this position (in app units)
};

struct TabWidthStore {
  explicit TabWidthStore(int32_t aValidForContentOffset)
    : mLimit(0)
    , mValidForContentOffset(aValidForContentOffset)
  { }

  // Apply tab widths to the aSpacing array, which corresponds to characters
  // beginning at aOffset and has length aLength. (Width records outside this
  // range will be ignored.)
  void ApplySpacing(gfxTextRun::PropertyProvider::Spacing *aSpacing,
                    uint32_t aOffset, uint32_t aLength);

  // Offset up to which tabs have been measured; positions beyond this have not
  // been calculated yet but may be appended if needed later.  It's a DOM
  // offset relative to the current frame's offset.
  uint32_t mLimit;

  // Need to recalc tab offsets if frame content offset differs from this.
  int32_t mValidForContentOffset;

  // A TabWidth record for each tab character measured so far.
  nsTArray<TabWidth> mWidths;
};

namespace {

struct TabwidthAdaptor
{
  const nsTArray<TabWidth>& mWidths;
  explicit TabwidthAdaptor(const nsTArray<TabWidth>& aWidths)
    : mWidths(aWidths) {}
  uint32_t operator[](size_t aIdx) const {
    return mWidths[aIdx].mOffset;
  }
};

} // namespace

void
TabWidthStore::ApplySpacing(gfxTextRun::PropertyProvider::Spacing *aSpacing,
                            uint32_t aOffset, uint32_t aLength)
{
  size_t i = 0;
  const size_t len = mWidths.Length();

  // If aOffset is non-zero, do a binary search to find where to start
  // processing the tab widths, in case the list is really long. (See bug
  // 953247.)
  // We need to start from the first entry where mOffset >= aOffset.
  if (aOffset > 0) {
    mozilla::BinarySearch(TabwidthAdaptor(mWidths), 0, len, aOffset, &i);
  }

  uint32_t limit = aOffset + aLength;
  while (i < len) {
    const TabWidth& tw = mWidths[i];
    if (tw.mOffset >= limit) {
      break;
    }
    aSpacing[tw.mOffset - aOffset].mAfter += tw.mWidth;
    i++;
  }
}

NS_DECLARE_FRAME_PROPERTY_DELETABLE(TabWidthProperty, TabWidthStore)

NS_DECLARE_FRAME_PROPERTY_WITHOUT_DTOR(OffsetToFrameProperty, nsTextFrame)

NS_DECLARE_FRAME_PROPERTY_RELEASABLE(UninflatedTextRunProperty, gfxTextRun)

NS_DECLARE_FRAME_PROPERTY_SMALL_VALUE(FontSizeInflationProperty, float)

/**
 * A glyph observer for the change of a font glyph in a text run.
 *
 * This is stored in {Simple, Complex}TextRunUserData.
 */
class GlyphObserver : public gfxFont::GlyphChangeObserver {
public:
  GlyphObserver(gfxFont* aFont, gfxTextRun* aTextRun)
    : gfxFont::GlyphChangeObserver(aFont), mTextRun(aTextRun) {
    MOZ_ASSERT(aTextRun->GetUserData());
  }
  virtual void NotifyGlyphsChanged() override;
private:
  gfxTextRun* mTextRun;
};

static const nsFrameState TEXT_REFLOW_FLAGS =
   TEXT_FIRST_LETTER |
   TEXT_START_OF_LINE |
   TEXT_END_OF_LINE |
   TEXT_HYPHEN_BREAK |
   TEXT_TRIMMED_TRAILING_WHITESPACE |
   TEXT_JUSTIFICATION_ENABLED |
   TEXT_HAS_NONCOLLAPSED_CHARACTERS |
   TEXT_SELECTION_UNDERLINE_OVERFLOWED |
   TEXT_NO_RENDERED_GLYPHS;

static const nsFrameState TEXT_WHITESPACE_FLAGS =
    TEXT_IS_ONLY_WHITESPACE |
    TEXT_ISNOT_ONLY_WHITESPACE;

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
 * but is all the same font.
 *
 * The userdata for a gfxTextRun object can be:
 *
 *   - A nsTextFrame* in the case a text run maps to only one flow. In this
 *   case, the textrun's user data pointer is a pointer to mStartFrame for that
 *   flow, mDOMOffsetToBeforeTransformOffset is zero, and mContentLength is the
 *   length of the text node.
 *
 *   - A SimpleTextRunUserData in the case a text run maps to one flow, but we
 *   still have to keep a list of glyph observers.
 *
 *   - A ComplexTextRunUserData in the case a text run maps to multiple flows,
 *   but we need to keep a list of glyph observers.
 *
 *   - A TextRunUserData in the case a text run maps multiple flows, but it
 *   doesn't have any glyph observer for changes in SVG fonts.
 *
 * You can differentiate between the four different cases with the
 * TEXT_IS_SIMPLE_FLOW and TEXT_MIGHT_HAVE_GLYPH_CHANGES flags.
 *
 * We go to considerable effort to make sure things work even if in-flow
 * siblings have different style contexts (i.e., first-letter and first-line).
 *
 * Our convention is that unsigned integer character offsets are offsets into
 * the transformed string. Signed integer character offsets are offsets into
 * the DOM string.
 *
 * XXX currently we don't handle hyphenated breaks between text frames where the
 * hyphen occurs at the end of the first text frame, e.g.
 *   <b>Kit&shy;</b>ty
 */

/**
 * This is our user data for the textrun, when textRun->GetFlags2() has
 * TEXT_IS_SIMPLE_FLOW set, and also TEXT_MIGHT_HAVE_GLYPH_CHANGES.
 *
 * This allows having an array of observers if there are fonts whose glyphs
 * might change, but also avoid allocation in the simple case that there aren't.
 */
struct SimpleTextRunUserData {
  nsTArray<UniquePtr<GlyphObserver>> mGlyphObservers;
  nsTextFrame* mFrame;
  explicit SimpleTextRunUserData(nsTextFrame* aFrame)
    : mFrame(aFrame)
  {
  }
};

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
 * negative (when a text run starts in the middle of a text node). Of course
 * it can also be zero.
 */
struct TextRunMappedFlow {
  nsTextFrame* mStartFrame;
  int32_t      mDOMOffsetToBeforeTransformOffset;
  // The text mapped starts at mStartFrame->GetContentOffset() and is this long
  uint32_t     mContentLength;
};

/**
 * This is the type in the gfxTextRun's userdata field in the common case that
 * the text run maps to multiple flows, but no fonts have been found with
 * animatable glyphs.
 *
 * This way, we avoid allocating and constructing the extra nsTArray.
 */
struct TextRunUserData {
#ifdef DEBUG
  TextRunMappedFlow* mMappedFlows;
#endif
  uint32_t           mMappedFlowCount;
  uint32_t           mLastFlowIndex;
};

/**
 * This is our user data for the textrun, when textRun->GetFlags2() does not
 * have TEXT_IS_SIMPLE_FLOW set and has the TEXT_MIGHT HAVE_GLYPH_CHANGES flag.
 */
struct ComplexTextRunUserData : public TextRunUserData {
  nsTArray<UniquePtr<GlyphObserver>> mGlyphObservers;
};

/**
 * This helper object computes colors used for painting, and also IME
 * underline information. The data is computed lazily and cached as necessary.
 * These live for just the duration of one paint operation.
 */
class nsTextPaintStyle {
public:
  explicit nsTextPaintStyle(nsTextFrame* aFrame);

  void SetResolveColors(bool aResolveColors) {
    mResolveColors = aResolveColors;
  }

  nscolor GetTextColor();

  // SVG text has its own painting process, so we should never get its stroke
  // property from here.
  nscolor GetWebkitTextStrokeColor() {
    if (nsSVGUtils::IsInSVGTextSubtree(mFrame)) {
      return 0;
    }
    return mFrame->StyleColor()->
      CalcComplexColor(mFrame->StyleText()->mWebkitTextStrokeColor);
  }
  float GetWebkitTextStrokeWidth() {
    if (nsSVGUtils::IsInSVGTextSubtree(mFrame)) {
      return 0.0f;
    }
    nscoord coord = mFrame->StyleText()->mWebkitTextStrokeWidth;
    return mFrame->PresContext()->AppUnitsToFloatDevPixels(coord);
  }

  /**
   * Compute the colors for normally-selected text. Returns false if
   * the normal selection is not being displayed.
   */
  bool GetSelectionColors(nscolor* aForeColor,
                            nscolor* aBackColor);
  void GetHighlightColors(nscolor* aForeColor,
                          nscolor* aBackColor);
  void GetURLSecondaryColor(nscolor* aForeColor);
  void GetIMESelectionColors(int32_t  aIndex,
                             nscolor* aForeColor,
                             nscolor* aBackColor);
  // if this returns false, we don't need to draw underline.
  bool GetSelectionUnderlineForPaint(int32_t  aIndex,
                                       nscolor* aLineColor,
                                       float*   aRelativeSize,
                                       uint8_t* aStyle);

  // if this returns false, we don't need to draw underline.
  static bool GetSelectionUnderline(nsPresContext* aPresContext,
                                      int32_t aIndex,
                                      nscolor* aLineColor,
                                      float* aRelativeSize,
                                      uint8_t* aStyle);

  // if this returns false, no text-shadow was specified for the selection
  // and the *aShadow parameter was not modified.
  bool GetSelectionShadow(nsCSSShadowArray** aShadow);

  nsPresContext* PresContext() const { return mPresContext; }

  enum {
    eIndexRawInput = 0,
    eIndexSelRawText,
    eIndexConvText,
    eIndexSelConvText,
    eIndexSpellChecker
  };

  static int32_t GetUnderlineStyleIndexForSelectionType(
                   SelectionType aSelectionType)
  {
    switch (aSelectionType) {
      case SelectionType::eIMERawClause:
        return eIndexRawInput;
      case SelectionType::eIMESelectedRawClause:
        return eIndexSelRawText;
      case SelectionType::eIMEConvertedClause:
        return eIndexConvText;
      case SelectionType::eIMESelectedClause:
        return eIndexSelConvText;
      case SelectionType::eSpellCheck:
        return eIndexSpellChecker;
      default:
        NS_WARNING("non-IME selection type");
        return eIndexRawInput;
    }
  }

  nscolor GetSystemFieldForegroundColor();
  nscolor GetSystemFieldBackgroundColor();

protected:
  nsTextFrame*   mFrame;
  nsPresContext* mPresContext;
  bool           mInitCommonColors;
  bool           mInitSelectionColorsAndShadow;
  bool           mResolveColors;

  // Selection data

  int16_t      mSelectionStatus; // see nsIDocument.h SetDisplaySelection()
  nscolor      mSelectionTextColor;
  nscolor      mSelectionBGColor;
  RefPtr<nsCSSShadowArray> mSelectionShadow;
  bool                       mHasSelectionShadow;

  // Common data

  int32_t mSufficientContrast;
  nscolor mFrameBackgroundColor;
  nscolor mSystemFieldForegroundColor;
  nscolor mSystemFieldBackgroundColor;

  // selection colors and underline info, the colors are resolved colors if
  // mResolveColors is true (which is the default), i.e., the foreground color
  // and background color are swapped if it's needed. And also line color will
  // be resolved from them.
  struct nsSelectionStyle {
    bool    mInit;
    nscolor mTextColor;
    nscolor mBGColor;
    nscolor mUnderlineColor;
    uint8_t mUnderlineStyle;
    float   mUnderlineRelativeSize;
  };
  nsSelectionStyle mSelectionStyle[5];

  // Color initializations
  void InitCommonColors();
  bool InitSelectionColorsAndShadow();

  nsSelectionStyle* GetSelectionStyle(int32_t aIndex);
  void InitSelectionStyle(int32_t aIndex);

  bool EnsureSufficientContrast(nscolor *aForeColor, nscolor *aBackColor);

  nscolor GetResolvedForeColor(nscolor aColor, nscolor aDefaultForeColor,
                               nscolor aBackColor);
};

static TextRunUserData*
CreateUserData(uint32_t aMappedFlowCount)
{
  TextRunUserData* data = static_cast<TextRunUserData*>
      (moz_xmalloc(sizeof(TextRunUserData) +
       aMappedFlowCount * sizeof(TextRunMappedFlow)));
#ifdef DEBUG
  data->mMappedFlows = reinterpret_cast<TextRunMappedFlow*>(data + 1);
#endif
  data->mMappedFlowCount = aMappedFlowCount;
  data->mLastFlowIndex = 0;
  return data;
}

static void
DestroyUserData(TextRunUserData* aUserData)
{
  if (aUserData) {
    free(aUserData);
  }
}

static ComplexTextRunUserData*
CreateComplexUserData(uint32_t aMappedFlowCount)
{
  ComplexTextRunUserData* data = static_cast<ComplexTextRunUserData*>
      (moz_xmalloc(sizeof(ComplexTextRunUserData) +
       aMappedFlowCount * sizeof(TextRunMappedFlow)));
  new (data) ComplexTextRunUserData();
#ifdef DEBUG
  data->mMappedFlows = reinterpret_cast<TextRunMappedFlow*>(data + 1);
#endif
  data->mMappedFlowCount = aMappedFlowCount;
  data->mLastFlowIndex = 0;
  return data;
}

static void
DestroyComplexUserData(ComplexTextRunUserData* aUserData)
{
  if (aUserData) {
    aUserData->~ComplexTextRunUserData();
    free(aUserData);
  }
}

static void
DestroyTextRunUserData(gfxTextRun* aTextRun)
{
  MOZ_ASSERT(aTextRun->GetUserData());
  if (aTextRun->GetFlags2() & nsTextFrameUtils::Flags::TEXT_IS_SIMPLE_FLOW) {
    if (aTextRun->GetFlags2() & nsTextFrameUtils::Flags::TEXT_MIGHT_HAVE_GLYPH_CHANGES) {
      delete static_cast<SimpleTextRunUserData*>(aTextRun->GetUserData());
    }
  } else {
    if (aTextRun->GetFlags2() & nsTextFrameUtils::Flags::TEXT_MIGHT_HAVE_GLYPH_CHANGES) {
      DestroyComplexUserData(
        static_cast<ComplexTextRunUserData*>(aTextRun->GetUserData()));
    } else {
      DestroyUserData(
        static_cast<TextRunUserData*>(aTextRun->GetUserData()));
    }
  }
  aTextRun->ClearFlagBits(nsTextFrameUtils::Flags::TEXT_MIGHT_HAVE_GLYPH_CHANGES);
  aTextRun->SetUserData(nullptr);
}

static TextRunMappedFlow*
GetMappedFlows(const gfxTextRun* aTextRun)
{
  MOZ_ASSERT(aTextRun->GetUserData(), "UserData must exist.");
  MOZ_ASSERT(!(aTextRun->GetFlags2() & nsTextFrameUtils::Flags::TEXT_IS_SIMPLE_FLOW),
            "The method should not be called for simple flows.");
  TextRunMappedFlow* flows;
  if (aTextRun->GetFlags2() & nsTextFrameUtils::Flags::TEXT_MIGHT_HAVE_GLYPH_CHANGES) {
    flows = reinterpret_cast<TextRunMappedFlow*>(
      static_cast<ComplexTextRunUserData*>(aTextRun->GetUserData()) + 1);
  } else {
    flows = reinterpret_cast<TextRunMappedFlow*>(
      static_cast<TextRunUserData*>(aTextRun->GetUserData()) + 1);
  }
  MOZ_ASSERT(static_cast<TextRunUserData*>(aTextRun->GetUserData())->
             mMappedFlows == flows,
             "GetMappedFlows should return the same pointer as mMappedFlows.");
  return flows;
}

/**
 * These are utility functions just for helping with the complexity related with
 * the text runs user data.
 */
static nsTextFrame*
GetFrameForSimpleFlow(const gfxTextRun* aTextRun)
{
  MOZ_ASSERT(aTextRun->GetFlags2() & nsTextFrameUtils::Flags::TEXT_IS_SIMPLE_FLOW,
             "Not so simple flow?");
  if (aTextRun->GetFlags2() & nsTextFrameUtils::Flags::TEXT_MIGHT_HAVE_GLYPH_CHANGES) {
    return static_cast<SimpleTextRunUserData*>(aTextRun->GetUserData())->mFrame;
  }

  return static_cast<nsTextFrame*>(aTextRun->GetUserData());
}

/**
 * Remove |aTextRun| from the frame continuation chain starting at
 * |aStartContinuation| if non-null, otherwise starting at |aFrame|.
 * Unmark |aFrame| as a text run owner if it's the frame we start at.
 * Return true if |aStartContinuation| is non-null and was found
 * in the next-continuation chain of |aFrame|.
 */
static bool
ClearAllTextRunReferences(nsTextFrame* aFrame, gfxTextRun* aTextRun,
                          nsTextFrame* aStartContinuation,
                          nsFrameState aWhichTextRunState)
{
  NS_PRECONDITION(aFrame, "");
  NS_PRECONDITION(!aStartContinuation ||
                  (!aStartContinuation->GetTextRun(nsTextFrame::eInflated) ||
                   aStartContinuation->GetTextRun(nsTextFrame::eInflated) == aTextRun) ||
                  (!aStartContinuation->GetTextRun(nsTextFrame::eNotInflated) ||
                   aStartContinuation->GetTextRun(nsTextFrame::eNotInflated) == aTextRun),
                  "wrong aStartContinuation for this text run");

  if (!aStartContinuation || aStartContinuation == aFrame) {
    aFrame->RemoveStateBits(aWhichTextRunState);
  } else {
    do {
      NS_ASSERTION(aFrame->IsTextFrame(), "Bad frame");
      aFrame = aFrame->GetNextContinuation();
    } while (aFrame && aFrame != aStartContinuation);
  }
  bool found = aStartContinuation == aFrame;
  while (aFrame) {
    NS_ASSERTION(aFrame->IsTextFrame(), "Bad frame");
    if (!aFrame->RemoveTextRun(aTextRun)) {
      break;
    }
    aFrame = aFrame->GetNextContinuation();
  }

  MOZ_ASSERT(!found || aStartContinuation, "how did we find null?");
  return found;
}

/**
 * Kill all references to |aTextRun| starting at |aStartContinuation|.
 * It could be referenced by any of its owners, and all their in-flows.
 * If |aStartContinuation| is null then process all userdata frames
 * and their continuations.
 * @note the caller is expected to take care of possibly destroying the
 * text run if all userdata frames were reset (userdata is deallocated
 * by this function though). The caller can detect this has occured by
 * checking |aTextRun->GetUserData() == nullptr|.
 */
static void
UnhookTextRunFromFrames(gfxTextRun* aTextRun, nsTextFrame* aStartContinuation)
{
  if (!aTextRun->GetUserData()) {
    return;
  }

  if (aTextRun->GetFlags2() & nsTextFrameUtils::Flags::TEXT_IS_SIMPLE_FLOW) {
    nsTextFrame* userDataFrame = GetFrameForSimpleFlow(aTextRun);
    nsFrameState whichTextRunState =
      userDataFrame->GetTextRun(nsTextFrame::eInflated) == aTextRun
        ? TEXT_IN_TEXTRUN_USER_DATA
        : TEXT_IN_UNINFLATED_TEXTRUN_USER_DATA;
    DebugOnly<bool> found =
      ClearAllTextRunReferences(userDataFrame, aTextRun,
                                aStartContinuation, whichTextRunState);
    NS_ASSERTION(!aStartContinuation || found,
                 "aStartContinuation wasn't found in simple flow text run");
    if (!(userDataFrame->GetStateBits() & whichTextRunState)) {
      DestroyTextRunUserData(aTextRun);
    }
  } else {
    auto userData = static_cast<TextRunUserData*>(aTextRun->GetUserData());
    TextRunMappedFlow* userMappedFlows = GetMappedFlows(aTextRun);
    int32_t destroyFromIndex = aStartContinuation ? -1 : 0;
    for (uint32_t i = 0; i < userData->mMappedFlowCount; ++i) {
      nsTextFrame* userDataFrame = userMappedFlows[i].mStartFrame;
      nsFrameState whichTextRunState =
        userDataFrame->GetTextRun(nsTextFrame::eInflated) == aTextRun
          ? TEXT_IN_TEXTRUN_USER_DATA
          : TEXT_IN_UNINFLATED_TEXTRUN_USER_DATA;
      bool found =
        ClearAllTextRunReferences(userDataFrame, aTextRun,
                                  aStartContinuation, whichTextRunState);
      if (found) {
        if (userDataFrame->GetStateBits() & whichTextRunState) {
          destroyFromIndex = i + 1;
        } else {
          destroyFromIndex = i;
        }
        aStartContinuation = nullptr;
      }
    }
    NS_ASSERTION(destroyFromIndex >= 0,
                 "aStartContinuation wasn't found in multi flow text run");
    if (destroyFromIndex == 0) {
      DestroyTextRunUserData(aTextRun);
    } else {
      userData->mMappedFlowCount = uint32_t(destroyFromIndex);
      if (userData->mLastFlowIndex >= uint32_t(destroyFromIndex)) {
        userData->mLastFlowIndex = uint32_t(destroyFromIndex) - 1;
      }
    }
  }
}

static void
InvalidateFrameDueToGlyphsChanged(nsIFrame* aFrame)
{
  MOZ_ASSERT(aFrame);

  nsIPresShell* shell = aFrame->PresShell();
  for (nsIFrame* f = aFrame; f;
       f = nsLayoutUtils::GetNextContinuationOrIBSplitSibling(f)) {
    f->InvalidateFrame();

    // If this is a non-display text frame within SVG <text>, we need
    // to reflow the SVGTextFrame. (This is similar to reflowing the
    // SVGTextFrame in response to style changes, in
    // SVGTextFrame::DidSetStyleContext.)
    if (nsSVGUtils::IsInSVGTextSubtree(f) &&
        f->GetStateBits() & NS_FRAME_IS_NONDISPLAY) {
      auto svgTextFrame = static_cast<SVGTextFrame*>(
        nsLayoutUtils::GetClosestFrameOfType(f, LayoutFrameType::SVGText));
      svgTextFrame->ScheduleReflowSVGNonDisplayText(nsIPresShell::eResize);
    } else {
      // Theoretically we could just update overflow areas, perhaps using
      // OverflowChangedTracker, but that would do a bunch of work eagerly that
      // we should probably do lazily here since there could be a lot
      // of text frames affected and we'd like to coalesce the work. So that's
      // not easy to do well.
      shell->FrameNeedsReflow(f, nsIPresShell::eResize, NS_FRAME_IS_DIRTY);
    }
  }
}

void
GlyphObserver::NotifyGlyphsChanged()
{
  if (mTextRun->GetFlags2() & nsTextFrameUtils::Flags::TEXT_IS_SIMPLE_FLOW) {
    InvalidateFrameDueToGlyphsChanged(GetFrameForSimpleFlow(mTextRun));
    return;
  }

  auto data = static_cast<TextRunUserData*>(mTextRun->GetUserData());
  TextRunMappedFlow* userMappedFlows = GetMappedFlows(mTextRun);
  for (uint32_t i = 0; i < data->mMappedFlowCount; ++i) {
    InvalidateFrameDueToGlyphsChanged(userMappedFlows[i].mStartFrame);
  }
}

int32_t nsTextFrame::GetContentEnd() const {
  nsTextFrame* next = GetNextContinuation();
  return next ? next->GetContentOffset() : mContent->GetText()->GetLength();
}

struct FlowLengthProperty {
  int32_t mStartOffset;
  // The offset of the next fixed continuation after mStartOffset, or
  // of the end of the text if there is none
  int32_t mEndFlowOffset;
};

int32_t nsTextFrame::GetInFlowContentLength() {
  if (!(mState & NS_FRAME_IS_BIDI)) {
    return mContent->TextLength() - mContentOffset;
  }

  FlowLengthProperty* flowLength =
    mContent->HasFlag(NS_HAS_FLOWLENGTH_PROPERTY)
    ? static_cast<FlowLengthProperty*>(mContent->GetProperty(nsGkAtoms::flowlength))
    : nullptr;

  /**
   * This frame must start inside the cached flow. If the flow starts at
   * mContentOffset but this frame is empty, logically it might be before the
   * start of the cached flow.
   */
  if (flowLength &&
      (flowLength->mStartOffset < mContentOffset ||
       (flowLength->mStartOffset == mContentOffset && GetContentEnd() > mContentOffset)) &&
      flowLength->mEndFlowOffset > mContentOffset) {
#ifdef DEBUG
    NS_ASSERTION(flowLength->mEndFlowOffset >= GetContentEnd(),
		 "frame crosses fixed continuation boundary");
#endif
    return flowLength->mEndFlowOffset - mContentOffset;
  }

  nsTextFrame* nextBidi = LastInFlow()->GetNextContinuation();
  int32_t endFlow = nextBidi ? nextBidi->GetContentOffset() : mContent->TextLength();

  if (!flowLength) {
    flowLength = new FlowLengthProperty;
    if (NS_FAILED(mContent->SetProperty(nsGkAtoms::flowlength, flowLength,
                                        nsINode::DeleteProperty<FlowLengthProperty>))) {
      delete flowLength;
      flowLength = nullptr;
    }
    mContent->SetFlags(NS_HAS_FLOWLENGTH_PROPERTY);
  }
  if (flowLength) {
    flowLength->mStartOffset = mContentOffset;
    flowLength->mEndFlowOffset = endFlow;
  }

  return endFlow - mContentOffset;
}

// Smarter versions of dom::IsSpaceCharacter.
// Unicode is really annoying; sometimes a space character isn't whitespace ---
// when it combines with another character
// So we have several versions of IsSpace for use in different contexts.

static bool IsSpaceCombiningSequenceTail(const nsTextFragment* aFrag, uint32_t aPos)
{
  NS_ASSERTION(aPos <= aFrag->GetLength(), "Bad offset");
  if (!aFrag->Is2b())
    return false;
  return nsTextFrameUtils::IsSpaceCombiningSequenceTail(
    aFrag->Get2b() + aPos, aFrag->GetLength() - aPos);
}

// Check whether aPos is a space for CSS 'word-spacing' purposes
static bool
IsCSSWordSpacingSpace(const nsTextFragment* aFrag, uint32_t aPos,
                      const nsTextFrame* aFrame, const nsStyleText* aStyleText)
{
  NS_ASSERTION(aPos < aFrag->GetLength(), "No text for IsSpace!");

  char16_t ch = aFrag->CharAt(aPos);
  switch (ch) {
  case ' ':
  case CH_NBSP:
    return !IsSpaceCombiningSequenceTail(aFrag, aPos + 1);
  case '\r':
  case '\t': return !aStyleText->WhiteSpaceIsSignificant();
  case '\n': return !aStyleText->NewlineIsSignificant(aFrame);
  default: return false;
  }
}

// Check whether the string aChars/aLength starts with space that's
// trimmable according to CSS 'white-space:normal/nowrap'.
static bool IsTrimmableSpace(const char16_t* aChars, uint32_t aLength)
{
  NS_ASSERTION(aLength > 0, "No text for IsSpace!");

  char16_t ch = *aChars;
  if (ch == ' ')
    return !nsTextFrameUtils::IsSpaceCombiningSequenceTail(aChars + 1, aLength - 1);
  return ch == '\t' || ch == '\f' || ch == '\n' || ch == '\r';
}

// Check whether the character aCh is trimmable according to CSS
// 'white-space:normal/nowrap'
static bool IsTrimmableSpace(char aCh)
{
  return aCh == ' ' || aCh == '\t' || aCh == '\f' || aCh == '\n' || aCh == '\r';
}

static bool IsTrimmableSpace(const nsTextFragment* aFrag, uint32_t aPos,
                               const nsStyleText* aStyleText)
{
  NS_ASSERTION(aPos < aFrag->GetLength(), "No text for IsSpace!");

  switch (aFrag->CharAt(aPos)) {
  case ' ': return !aStyleText->WhiteSpaceIsSignificant() &&
                   !IsSpaceCombiningSequenceTail(aFrag, aPos + 1);
  case '\n': return !aStyleText->NewlineIsSignificantStyle() &&
                    aStyleText->mWhiteSpace != mozilla::StyleWhiteSpace::PreSpace;
  case '\t':
  case '\r':
  case '\f': return !aStyleText->WhiteSpaceIsSignificant();
  default: return false;
  }
}

static bool IsSelectionSpace(const nsTextFragment* aFrag, uint32_t aPos)
{
  NS_ASSERTION(aPos < aFrag->GetLength(), "No text for IsSpace!");
  char16_t ch = aFrag->CharAt(aPos);
  if (ch == ' ' || ch == CH_NBSP)
    return !IsSpaceCombiningSequenceTail(aFrag, aPos + 1);
  return ch == '\t' || ch == '\n' || ch == '\f' || ch == '\r';
}

// Count the amount of trimmable whitespace (as per CSS
// 'white-space:normal/nowrap') in a text fragment. The first
// character is at offset aStartOffset; the maximum number of characters
// to check is aLength. aDirection is -1 or 1 depending on whether we should
// progress backwards or forwards.
static uint32_t
GetTrimmableWhitespaceCount(const nsTextFragment* aFrag,
                            int32_t aStartOffset, int32_t aLength,
                            int32_t aDirection)
{
  int32_t count = 0;
  if (aFrag->Is2b()) {
    const char16_t* str = aFrag->Get2b() + aStartOffset;
    int32_t fragLen = aFrag->GetLength() - aStartOffset;
    for (; count < aLength; ++count) {
      if (!IsTrimmableSpace(str, fragLen))
        break;
      str += aDirection;
      fragLen -= aDirection;
    }
  } else {
    const char* str = aFrag->Get1b() + aStartOffset;
    for (; count < aLength; ++count) {
      if (!IsTrimmableSpace(*str))
        break;
      str += aDirection;
    }
  }
  return count;
}

static bool
IsAllWhitespace(const nsTextFragment* aFrag, bool aAllowNewline)
{
  if (aFrag->Is2b())
    return false;
  int32_t len = aFrag->GetLength();
  const char* str = aFrag->Get1b();
  for (int32_t i = 0; i < len; ++i) {
    char ch = str[i];
    if (ch == ' ' || ch == '\t' || ch == '\r' || (ch == '\n' && aAllowNewline))
      continue;
    return false;
  }
  return true;
}

static void
ClearObserversFromTextRun(gfxTextRun* aTextRun)
{
  if (!(aTextRun->GetFlags2() & nsTextFrameUtils::Flags::TEXT_MIGHT_HAVE_GLYPH_CHANGES)) {
    return;
  }

  if (aTextRun->GetFlags2() & nsTextFrameUtils::Flags::TEXT_IS_SIMPLE_FLOW) {
    static_cast<SimpleTextRunUserData*>(aTextRun->GetUserData())
      ->mGlyphObservers.Clear();
  } else {
    static_cast<ComplexTextRunUserData*>(aTextRun->GetUserData())
      ->mGlyphObservers.Clear();
  }
}

static void
CreateObserversForAnimatedGlyphs(gfxTextRun* aTextRun)
{
  if (!aTextRun->GetUserData()) {
    return;
  }

  ClearObserversFromTextRun(aTextRun);

  nsTArray<gfxFont*> fontsWithAnimatedGlyphs;
  uint32_t numGlyphRuns;
  const gfxTextRun::GlyphRun* glyphRuns =
    aTextRun->GetGlyphRuns(&numGlyphRuns);
  for (uint32_t i = 0; i < numGlyphRuns; ++i) {
    gfxFont* font = glyphRuns[i].mFont;
    if (font->GlyphsMayChange() && !fontsWithAnimatedGlyphs.Contains(font)) {
      fontsWithAnimatedGlyphs.AppendElement(font);
    }
  }
  if (fontsWithAnimatedGlyphs.IsEmpty()) {
    // NB: Theoretically, we should clear the TEXT_MIGHT_HAVE_GLYPH_CHANGES
    // here. That would involve de-allocating the simple user data struct if
    // present too, and resetting the pointer to the frame. In practice, I
    // don't think worth doing that work here, given the flag's only purpose is
    // to distinguish what kind of user data is there.
    return;
  }

  nsTArray<UniquePtr<GlyphObserver>>* observers;

  if (aTextRun->GetFlags2() & nsTextFrameUtils::Flags::TEXT_IS_SIMPLE_FLOW) {
    // Swap the frame pointer for a just-allocated SimpleTextRunUserData if
    // appropriate.
    if (!(aTextRun->GetFlags2() & nsTextFrameUtils::Flags::TEXT_MIGHT_HAVE_GLYPH_CHANGES)) {
      auto frame = static_cast<nsTextFrame*>(aTextRun->GetUserData());
      aTextRun->SetUserData(new SimpleTextRunUserData(frame));
    }

    auto data =
      static_cast<SimpleTextRunUserData*>(aTextRun->GetUserData());
    observers = &data->mGlyphObservers;
  } else {
    if (!(aTextRun->GetFlags2() & nsTextFrameUtils::Flags::TEXT_MIGHT_HAVE_GLYPH_CHANGES)) {
      auto oldData = static_cast<TextRunUserData*>(aTextRun->GetUserData());
      TextRunMappedFlow* oldMappedFlows = GetMappedFlows(aTextRun);
      ComplexTextRunUserData* data =
        CreateComplexUserData(oldData->mMappedFlowCount);
      TextRunMappedFlow* dataMappedFlows =
        reinterpret_cast<TextRunMappedFlow*>(data + 1);
      data->mLastFlowIndex = oldData->mLastFlowIndex;
      for (uint32_t i = 0; i < oldData->mMappedFlowCount; ++i) {
        dataMappedFlows[i] = oldMappedFlows[i];
      }
      DestroyUserData(oldData);
      aTextRun->SetUserData(data);
    }
    auto data = static_cast<ComplexTextRunUserData*>(aTextRun->GetUserData());
    observers = &data->mGlyphObservers;
  }

  aTextRun->SetFlagBits(nsTextFrameUtils::Flags::TEXT_MIGHT_HAVE_GLYPH_CHANGES);

  for (auto font : fontsWithAnimatedGlyphs) {
    observers->AppendElement(new GlyphObserver(font, aTextRun));
  }
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
  BuildTextRunsScanner(nsPresContext* aPresContext, DrawTarget* aDrawTarget,
      nsIFrame* aLineContainer, nsTextFrame::TextRunType aWhichTextRun) :
    mDrawTarget(aDrawTarget),
    mLineContainer(aLineContainer),
    mCommonAncestorWithLastFrame(nullptr),
    mMissingFonts(aPresContext->MissingFontRecorder()),
    mBidiEnabled(aPresContext->BidiEnabled()),
    mSkipIncompleteTextRuns(false),
    mWhichTextRun(aWhichTextRun),
    mNextRunContextInfo(nsTextFrameUtils::INCOMING_NONE),
    mCurrentRunContextInfo(nsTextFrameUtils::INCOMING_NONE) {
    ResetRunInfo();
  }
  ~BuildTextRunsScanner() {
    NS_ASSERTION(mBreakSinks.IsEmpty(), "Should have been cleared");
    NS_ASSERTION(mLineBreakBeforeFrames.IsEmpty(), "Should have been cleared");
    NS_ASSERTION(mMappedFlows.IsEmpty(), "Should have been cleared");
  }

  void SetAtStartOfLine() {
    mStartOfLine = true;
    mCanStopOnThisLine = false;
  }
  void SetSkipIncompleteTextRuns(bool aSkip) {
    mSkipIncompleteTextRuns = aSkip;
  }
  void SetCommonAncestorWithLastFrame(nsIFrame* aFrame) {
    mCommonAncestorWithLastFrame = aFrame;
  }
  bool CanStopOnThisLine() {
    return mCanStopOnThisLine;
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
  bool IsTextRunValidForMappedFlows(const gfxTextRun* aTextRun);
  void FlushFrames(bool aFlushLineBreaks, bool aSuppressTrailingBreak);
  void FlushLineBreaks(gfxTextRun* aTrailingTextRun);
  void ResetRunInfo() {
    mLastFrame = nullptr;
    mMappedFlows.Clear();
    mLineBreakBeforeFrames.Clear();
    mMaxTextLength = 0;
    mDoubleByteText = false;
  }
  void AccumulateRunInfo(nsTextFrame* aFrame);
  /**
   * @return null to indicate either textrun construction failed or
   * we constructed just a partial textrun to set up linebreaker and other
   * state for following textruns.
   */
  already_AddRefed<gfxTextRun> BuildTextRunForFrames(void* aTextBuffer);
  bool SetupLineBreakerContext(gfxTextRun *aTextRun);
  void AssignTextRun(gfxTextRun* aTextRun, float aInflation);
  nsTextFrame* GetNextBreakBeforeFrame(uint32_t* aIndex);
  void SetupBreakSinksForTextRun(gfxTextRun* aTextRun, const void* aTextPtr);
  void SetupTextEmphasisForTextRun(gfxTextRun* aTextRun, const void* aTextPtr);
  struct FindBoundaryState {
    nsIFrame*    mStopAtFrame;
    nsTextFrame* mFirstTextFrame;
    nsTextFrame* mLastTextFrame;
    bool mSeenTextRunBoundaryOnLaterLine;
    bool mSeenTextRunBoundaryOnThisLine;
    bool mSeenSpaceForLineBreakingOnThisLine;
    nsTArray<char16_t>& mBuffer;
  };
  enum FindBoundaryResult {
    FB_CONTINUE,
    FB_STOPPED_AT_STOP_FRAME,
    FB_FOUND_VALID_TEXTRUN_BOUNDARY
  };
  FindBoundaryResult FindBoundaries(nsIFrame* aFrame, FindBoundaryState* aState);

  bool ContinueTextRunAcrossFrames(nsTextFrame* aFrame1, nsTextFrame* aFrame2);

  // Like TextRunMappedFlow but with some differences. mStartFrame to mEndFrame
  // (exclusive) are a sequence of in-flow frames (if mEndFrame is null, then
  // continuations starting from mStartFrame are a sequence of in-flow frames).
  struct MappedFlow {
    nsTextFrame* mStartFrame;
    nsTextFrame* mEndFrame;
    // When we consider breaking between elements, the nearest common
    // ancestor of the elements containing the characters is the one whose
    // CSS 'white-space' property governs. So this records the nearest common
    // ancestor of mStartFrame and the previous text frame, or null if there
    // was no previous text frame on this line.
    nsIFrame*    mAncestorControllingInitialBreak;

    int32_t GetContentEnd() {
      return mEndFrame ? mEndFrame->GetContentOffset()
          : mStartFrame->GetContent()->GetText()->GetLength();
    }
  };

  class BreakSink final : public nsILineBreakSink {
  public:
    BreakSink(gfxTextRun* aTextRun, DrawTarget* aDrawTarget,
              uint32_t aOffsetIntoTextRun)
      : mTextRun(aTextRun)
      , mDrawTarget(aDrawTarget)
      , mOffsetIntoTextRun(aOffsetIntoTextRun)
    {}

    virtual void SetBreaks(uint32_t aOffset, uint32_t aLength,
                           uint8_t* aBreakBefore) override {
      gfxTextRun::Range range(aOffset + mOffsetIntoTextRun,
                              aOffset + mOffsetIntoTextRun + aLength);
      if (mTextRun->SetPotentialLineBreaks(range, aBreakBefore)) {
        // Be conservative and assume that some breaks have been set
        mTextRun->ClearFlagBits(nsTextFrameUtils::Flags::TEXT_NO_BREAKS);
      }
    }

    virtual void SetCapitalization(uint32_t aOffset, uint32_t aLength,
                                   bool* aCapitalize) override {
      MOZ_ASSERT(mTextRun->GetFlags2() & nsTextFrameUtils::Flags::TEXT_IS_TRANSFORMED,
                 "Text run should be transformed!");
      if (mTextRun->GetFlags2() & nsTextFrameUtils::Flags::TEXT_IS_TRANSFORMED) {
        nsTransformedTextRun* transformedTextRun =
          static_cast<nsTransformedTextRun*>(mTextRun.get());
        transformedTextRun->SetCapitalization(aOffset + mOffsetIntoTextRun, aLength,
                                              aCapitalize);
      }
    }

    void Finish(gfxMissingFontRecorder* aMFR) {
      MOZ_ASSERT(!(mTextRun->GetFlags2() & nsTextFrameUtils::Flags::TEXT_UNUSED_FLAGS),
                   "Flag set that should never be set! (memory safety error?)");
      if (mTextRun->GetFlags2() & nsTextFrameUtils::Flags::TEXT_IS_TRANSFORMED) {
        nsTransformedTextRun* transformedTextRun =
          static_cast<nsTransformedTextRun*>(mTextRun.get());
        transformedTextRun->FinishSettingProperties(mDrawTarget, aMFR);
      }
      // The way nsTransformedTextRun is implemented, its glyph runs aren't
      // available until after nsTransformedTextRun::FinishSettingProperties()
      // is called. So that's why we defer checking for animated glyphs to here.
      CreateObserversForAnimatedGlyphs(mTextRun);
    }

    RefPtr<gfxTextRun> mTextRun;
    DrawTarget*  mDrawTarget;
    uint32_t     mOffsetIntoTextRun;
  };

private:
  AutoTArray<MappedFlow,10>   mMappedFlows;
  AutoTArray<nsTextFrame*,50> mLineBreakBeforeFrames;
  AutoTArray<UniquePtr<BreakSink>,10> mBreakSinks;
  nsLineBreaker                 mLineBreaker;
  RefPtr<gfxTextRun>            mCurrentFramesAllSameTextRun;
  DrawTarget*                   mDrawTarget;
  nsIFrame*                     mLineContainer;
  nsTextFrame*                  mLastFrame;
  // The common ancestor of the current frame and the previous leaf frame
  // on the line, or null if there was no previous leaf frame.
  nsIFrame*                     mCommonAncestorWithLastFrame;
  gfxMissingFontRecorder*       mMissingFonts;
  // mMaxTextLength is an upper bound on the size of the text in all mapped frames
  // The value UINT32_MAX represents overflow; text will be discarded
  uint32_t                      mMaxTextLength;
  bool                          mDoubleByteText;
  bool                          mBidiEnabled;
  bool                          mStartOfLine;
  bool                          mSkipIncompleteTextRuns;
  bool                          mCanStopOnThisLine;
  nsTextFrame::TextRunType      mWhichTextRun;
  uint8_t                       mNextRunContextInfo;
  uint8_t                       mCurrentRunContextInfo;
};

static nsIFrame*
FindLineContainer(nsIFrame* aFrame)
{
  while (aFrame && (aFrame->IsFrameOfType(nsIFrame::eLineParticipant) ||
                    aFrame->CanContinueTextRun())) {
    aFrame = aFrame->GetParent();
  }
  return aFrame;
}

static bool
IsLineBreakingWhiteSpace(char16_t aChar)
{
  // 0x0A (\n) is not handled as white-space by the line breaker, since
  // we break before it, if it isn't transformed to a normal space.
  // (If we treat it as normal white-space then we'd only break after it.)
  // However, it does induce a line break or is converted to a regular
  // space, and either way it can be used to bound the region of text
  // that needs to be analyzed for line breaking.
  return nsLineBreaker::IsSpace(aChar) || aChar == 0x0A;
}

static bool
TextContainsLineBreakerWhiteSpace(const void* aText, uint32_t aLength,
                                  bool aIsDoubleByte)
{
  if (aIsDoubleByte) {
    const char16_t* chars = static_cast<const char16_t*>(aText);
    for (uint32_t i = 0; i < aLength; ++i) {
      if (IsLineBreakingWhiteSpace(chars[i]))
        return true;
    }
    return false;
  } else {
    const uint8_t* chars = static_cast<const uint8_t*>(aText);
    for (uint32_t i = 0; i < aLength; ++i) {
      if (IsLineBreakingWhiteSpace(chars[i]))
        return true;
    }
    return false;
  }
}

static_assert(uint8_t(mozilla::StyleWhiteSpace::Normal) == 0, "Convention: StyleWhiteSpace::Normal should be 0");
static_assert(uint8_t(mozilla::StyleWhiteSpace::Pre) == 1, "Convention: StyleWhiteSpace::Pre should be 1");
static_assert(uint8_t(mozilla::StyleWhiteSpace::Nowrap) == 2, "Convention: StyleWhiteSpace::NoWrap should be 2");
static_assert(uint8_t(mozilla::StyleWhiteSpace::PreWrap) == 3, "Convention: StyleWhiteSpace::PreWrap should be 3");
static_assert(uint8_t(mozilla::StyleWhiteSpace::PreLine) == 4, "Convention: StyleWhiteSpace::PreLine should be 4");
static_assert(uint8_t(mozilla::StyleWhiteSpace::PreSpace) == 5, "Convention: StyleWhiteSpace::PreSpace should be 5");

static nsTextFrameUtils::CompressionMode
GetCSSWhitespaceToCompressionMode(nsTextFrame* aFrame,
                                  const nsStyleText* aStyleText)
{
  static const nsTextFrameUtils::CompressionMode sModes[] =
  {
    nsTextFrameUtils::COMPRESS_WHITESPACE_NEWLINE,     // normal
    nsTextFrameUtils::COMPRESS_NONE,                   // pre
    nsTextFrameUtils::COMPRESS_WHITESPACE_NEWLINE,     // nowrap
    nsTextFrameUtils::COMPRESS_NONE,                   // pre-wrap
    nsTextFrameUtils::COMPRESS_WHITESPACE,             // pre-line
    nsTextFrameUtils::COMPRESS_NONE_TRANSFORM_TO_SPACE // -moz-pre-space
  };

  auto compression = sModes[uint8_t(aStyleText->mWhiteSpace)];
  if (compression == nsTextFrameUtils::COMPRESS_NONE &&
      !aStyleText->NewlineIsSignificant(aFrame)) {
    // If newline is set to be preserved, but then suppressed,
    // transform newline to space.
    compression = nsTextFrameUtils::COMPRESS_NONE_TRANSFORM_TO_SPACE;
  }
  return compression;
}

struct FrameTextTraversal
{
  FrameTextTraversal()
    : mFrameToScan(nullptr)
    , mOverflowFrameToScan(nullptr)
    , mScanSiblings(false)
    , mLineBreakerCanCrossFrameBoundary(false)
    , mTextRunCanCrossFrameBoundary(false)
  {}

  // These fields identify which frames should be recursively scanned
  // The first normal frame to scan (or null, if no such frame should be scanned)
  nsIFrame*    mFrameToScan;
  // The first overflow frame to scan (or null, if no such frame should be scanned)
  nsIFrame*    mOverflowFrameToScan;
  // Whether to scan the siblings of mFrameToDescendInto/mOverflowFrameToDescendInto
  bool mScanSiblings;

  // These identify the boundaries of the context required for
  // line breaking or textrun construction
  bool mLineBreakerCanCrossFrameBoundary;
  bool mTextRunCanCrossFrameBoundary;

  nsIFrame* NextFrameToScan() {
    nsIFrame* f;
    if (mFrameToScan) {
      f = mFrameToScan;
      mFrameToScan = mScanSiblings ? f->GetNextSibling() : nullptr;
    } else if (mOverflowFrameToScan) {
      f = mOverflowFrameToScan;
      mOverflowFrameToScan = mScanSiblings ? f->GetNextSibling() : nullptr;
    } else {
      f = nullptr;
    }
    return f;
  }
};

static FrameTextTraversal
CanTextCrossFrameBoundary(nsIFrame* aFrame)
{
  FrameTextTraversal result;

  bool continuesTextRun = aFrame->CanContinueTextRun();
  if (aFrame->IsPlaceholderFrame()) {
    // placeholders are "invisible", so a text run should be able to span
    // across one. But don't descend into the out-of-flow.
    result.mLineBreakerCanCrossFrameBoundary = true;
    if (continuesTextRun) {
      // ... Except for first-letter floats, which are really in-flow
      // from the point of view of capitalization etc, so we'd better
      // descend into them. But we actually need to break the textrun for
      // first-letter floats since things look bad if, say, we try to make a
      // ligature across the float boundary.
      result.mFrameToScan =
        (static_cast<nsPlaceholderFrame*>(aFrame))->GetOutOfFlowFrame();
    } else {
      result.mTextRunCanCrossFrameBoundary = true;
    }
  } else {
    if (continuesTextRun) {
      result.mFrameToScan = aFrame->PrincipalChildList().FirstChild();
      result.mOverflowFrameToScan =
        aFrame->GetChildList(nsIFrame::kOverflowList).FirstChild();
      NS_WARNING_ASSERTION(
        !result.mOverflowFrameToScan,
        "Scanning overflow inline frames is something we should avoid");
      result.mScanSiblings = true;
      result.mTextRunCanCrossFrameBoundary = true;
      result.mLineBreakerCanCrossFrameBoundary = true;
    } else {
      MOZ_ASSERT(!aFrame->IsRubyTextContainerFrame(),
                 "Shouldn't call this method for ruby text container");
    }
  }
  return result;
}

BuildTextRunsScanner::FindBoundaryResult
BuildTextRunsScanner::FindBoundaries(nsIFrame* aFrame, FindBoundaryState* aState)
{
  LayoutFrameType frameType = aFrame->Type();
  if (frameType == LayoutFrameType::RubyTextContainer) {
    // Don't stop a text run for ruby text container. We want ruby text
    // containers to be skipped, but continue the text run across them.
    return FB_CONTINUE;
  }

  nsTextFrame* textFrame = frameType == LayoutFrameType::Text
                             ? static_cast<nsTextFrame*>(aFrame)
                             : nullptr;
  if (textFrame) {
    if (aState->mLastTextFrame &&
        textFrame != aState->mLastTextFrame->GetNextInFlow() &&
        !ContinueTextRunAcrossFrames(aState->mLastTextFrame, textFrame)) {
      aState->mSeenTextRunBoundaryOnThisLine = true;
      if (aState->mSeenSpaceForLineBreakingOnThisLine)
        return FB_FOUND_VALID_TEXTRUN_BOUNDARY;
    }
    if (!aState->mFirstTextFrame) {
      aState->mFirstTextFrame = textFrame;
    }
    aState->mLastTextFrame = textFrame;
  }

  if (aFrame == aState->mStopAtFrame)
    return FB_STOPPED_AT_STOP_FRAME;

  if (textFrame) {
    if (aState->mSeenSpaceForLineBreakingOnThisLine) {
      return FB_CONTINUE;
    }
    const nsTextFragment* frag = textFrame->GetContent()->GetText();
    uint32_t start = textFrame->GetContentOffset();
    uint32_t length = textFrame->GetContentLength();
    const void* text;
    if (frag->Is2b()) {
      // It is possible that we may end up removing all whitespace in
      // a piece of text because of The White Space Processing Rules,
      // so we need to transform it before we can check existence of
      // such whitespaces.
      aState->mBuffer.EnsureLengthAtLeast(length);
      nsTextFrameUtils::CompressionMode compression =
        GetCSSWhitespaceToCompressionMode(textFrame, textFrame->StyleText());
      uint8_t incomingFlags = 0;
      gfxSkipChars skipChars;
      nsTextFrameUtils::Flags analysisFlags;
      char16_t* bufStart = aState->mBuffer.Elements();
      char16_t* bufEnd = nsTextFrameUtils::TransformText(
        frag->Get2b() + start, length, bufStart, compression,
        &incomingFlags, &skipChars, &analysisFlags);
      text = bufStart;
      length = bufEnd - bufStart;
    } else {
      // If the text only contains ASCII characters, it is currently
      // impossible that TransformText would remove all whitespaces,
      // and thus the check below should return the same result for
      // transformed text and original text. So we don't need to try
      // transforming it here.
      text = static_cast<const void*>(frag->Get1b() + start);
    }
    if (TextContainsLineBreakerWhiteSpace(text, length, frag->Is2b())) {
      aState->mSeenSpaceForLineBreakingOnThisLine = true;
      if (aState->mSeenTextRunBoundaryOnLaterLine) {
        return FB_FOUND_VALID_TEXTRUN_BOUNDARY;
      }
    }
    return FB_CONTINUE;
  }

  FrameTextTraversal traversal = CanTextCrossFrameBoundary(aFrame);
  if (!traversal.mTextRunCanCrossFrameBoundary) {
    aState->mSeenTextRunBoundaryOnThisLine = true;
    if (aState->mSeenSpaceForLineBreakingOnThisLine)
      return FB_FOUND_VALID_TEXTRUN_BOUNDARY;
  }

  for (nsIFrame* f = traversal.NextFrameToScan(); f;
       f = traversal.NextFrameToScan()) {
    FindBoundaryResult result = FindBoundaries(f, aState);
    if (result != FB_CONTINUE)
      return result;
  }

  if (!traversal.mTextRunCanCrossFrameBoundary) {
    aState->mSeenTextRunBoundaryOnThisLine = true;
    if (aState->mSeenSpaceForLineBreakingOnThisLine)
      return FB_FOUND_VALID_TEXTRUN_BOUNDARY;
  }

  return FB_CONTINUE;
}

// build text runs for the 200 lines following aForFrame, and stop after that
// when we get a chance.
#define NUM_LINES_TO_BUILD_TEXT_RUNS 200

/**
 * General routine for building text runs. This is hairy because of the need
 * to build text runs that span content nodes.
 *
 * @param aContext The gfxContext we're using to construct this text run.
 * @param aForFrame The nsTextFrame for which we're building this text run.
 * @param aLineContainer the line container containing aForFrame; if null,
 *        we'll walk the ancestors to find it.  It's required to be non-null
 *        when aForFrameLine is non-null.
 * @param aForFrameLine the line containing aForFrame; if null, we'll figure
 *        out the line (slowly)
 * @param aWhichTextRun The type of text run we want to build. If font inflation
 *        is enabled, this will be eInflated, otherwise it's eNotInflated.
 */
static void
BuildTextRuns(DrawTarget* aDrawTarget, nsTextFrame* aForFrame,
              nsIFrame* aLineContainer,
              const nsLineList::iterator* aForFrameLine,
              nsTextFrame::TextRunType aWhichTextRun)
{
  MOZ_ASSERT(aForFrame, "for no frame?");
  NS_ASSERTION(!aForFrameLine || aLineContainer,
               "line but no line container");

  nsIFrame* lineContainerChild = aForFrame;
  if (!aLineContainer) {
    if (aForFrame->IsFloatingFirstLetterChild()) {
      lineContainerChild = aForFrame->GetParent()->GetPlaceholderFrame();
    }
    aLineContainer = FindLineContainer(lineContainerChild);
  } else {
    NS_ASSERTION((aLineContainer == FindLineContainer(aForFrame) ||
                  (aLineContainer->IsLetterFrame() &&
                   aLineContainer->IsFloating())),
                 "Wrong line container hint");
  }

  if (aForFrame->HasAnyStateBits(TEXT_IS_IN_TOKEN_MATHML)) {
    aLineContainer->AddStateBits(TEXT_IS_IN_TOKEN_MATHML);
    if (aForFrame->HasAnyStateBits(NS_FRAME_IS_IN_SINGLE_CHAR_MI)) {
      aLineContainer->AddStateBits(NS_FRAME_IS_IN_SINGLE_CHAR_MI);
    }
  }
  if (aForFrame->HasAnyStateBits(NS_FRAME_MATHML_SCRIPT_DESCENDANT)) {
    aLineContainer->AddStateBits(NS_FRAME_MATHML_SCRIPT_DESCENDANT);
  }

  nsPresContext* presContext = aLineContainer->PresContext();
  BuildTextRunsScanner scanner(presContext, aDrawTarget,
                               aLineContainer, aWhichTextRun);

  nsBlockFrame* block = nsLayoutUtils::GetAsBlock(aLineContainer);

  if (!block) {
    nsIFrame* textRunContainer = aLineContainer;
    if (aLineContainer->IsRubyTextContainerFrame()) {
      textRunContainer = aForFrame;
      while (textRunContainer && !textRunContainer->IsRubyTextFrame()) {
        textRunContainer = textRunContainer->GetParent();
      }
      MOZ_ASSERT(textRunContainer &&
                 textRunContainer->GetParent() == aLineContainer);
    } else {
      NS_ASSERTION(
        !aLineContainer->GetPrevInFlow() && !aLineContainer->GetNextInFlow(),
        "Breakable non-block line containers other than "
        "ruby text container is not supported");
    }
    // Just loop through all the children of the linecontainer ... it's really
    // just one line
    scanner.SetAtStartOfLine();
    scanner.SetCommonAncestorWithLastFrame(nullptr);
    for (nsIFrame* child : textRunContainer->PrincipalChildList()) {
      scanner.ScanFrame(child);
    }
    // Set mStartOfLine so FlushFrames knows its textrun ends a line
    scanner.SetAtStartOfLine();
    scanner.FlushFrames(true, false);
    return;
  }

  // Find the line containing 'lineContainerChild'.

  bool isValid = true;
  nsBlockInFlowLineIterator backIterator(block, &isValid);
  if (aForFrameLine) {
    backIterator = nsBlockInFlowLineIterator(block, *aForFrameLine);
  } else {
    backIterator = nsBlockInFlowLineIterator(block, lineContainerChild, &isValid);
    NS_ASSERTION(isValid, "aForFrame not found in block, someone lied to us");
    NS_ASSERTION(backIterator.GetContainer() == block,
                 "Someone lied to us about the block");
  }
  nsBlockFrame::LineIterator startLine = backIterator.GetLine();

  // Find a line where we can start building text runs. We choose the last line
  // where:
  // -- there is a textrun boundary between the start of the line and the
  // start of aForFrame
  // -- there is a space between the start of the line and the textrun boundary
  // (this is so we can be sure the line breaks will be set properly
  // on the textruns we construct).
  // The possibly-partial text runs up to and including the first space
  // are not reconstructed. We construct partial text runs for that text ---
  // for the sake of simplifying the code and feeding the linebreaker ---
  // but we discard them instead of assigning them to frames.
  // This is a little awkward because we traverse lines in the reverse direction
  // but we traverse the frames in each line in the forward direction.
  nsBlockInFlowLineIterator forwardIterator = backIterator;
  nsIFrame* stopAtFrame = lineContainerChild;
  nsTextFrame* nextLineFirstTextFrame = nullptr;
  AutoTArray<char16_t, BIG_TEXT_NODE_SIZE> buffer;
  bool seenTextRunBoundaryOnLaterLine = false;
  bool mayBeginInTextRun = true;
  while (true) {
    forwardIterator = backIterator;
    nsBlockFrame::LineIterator line = backIterator.GetLine();
    if (!backIterator.Prev() || backIterator.GetLine()->IsBlock()) {
      mayBeginInTextRun = false;
      break;
    }

    BuildTextRunsScanner::FindBoundaryState state = { stopAtFrame, nullptr, nullptr,
      bool(seenTextRunBoundaryOnLaterLine), false, false, buffer };
    nsIFrame* child = line->mFirstChild;
    bool foundBoundary = false;
    for (int32_t i = line->GetChildCount() - 1; i >= 0; --i) {
      BuildTextRunsScanner::FindBoundaryResult result =
          scanner.FindBoundaries(child, &state);
      if (result == BuildTextRunsScanner::FB_FOUND_VALID_TEXTRUN_BOUNDARY) {
        foundBoundary = true;
        break;
      } else if (result == BuildTextRunsScanner::FB_STOPPED_AT_STOP_FRAME) {
        break;
      }
      child = child->GetNextSibling();
    }
    if (foundBoundary)
      break;
    if (!stopAtFrame && state.mLastTextFrame && nextLineFirstTextFrame &&
        !scanner.ContinueTextRunAcrossFrames(state.mLastTextFrame, nextLineFirstTextFrame)) {
      // Found a usable textrun boundary at the end of the line
      if (state.mSeenSpaceForLineBreakingOnThisLine)
        break;
      seenTextRunBoundaryOnLaterLine = true;
    } else if (state.mSeenTextRunBoundaryOnThisLine) {
      seenTextRunBoundaryOnLaterLine = true;
    }
    stopAtFrame = nullptr;
    if (state.mFirstTextFrame) {
      nextLineFirstTextFrame = state.mFirstTextFrame;
    }
  }
  scanner.SetSkipIncompleteTextRuns(mayBeginInTextRun);

  // Now iterate over all text frames starting from the current line. First-in-flow
  // text frames will be accumulated into textRunFrames as we go. When a
  // text run boundary is required we flush textRunFrames ((re)building their
  // gfxTextRuns as necessary).
  bool seenStartLine = false;
  uint32_t linesAfterStartLine = 0;
  do {
    nsBlockFrame::LineIterator line = forwardIterator.GetLine();
    if (line->IsBlock())
      break;
    line->SetInvalidateTextRuns(false);
    scanner.SetAtStartOfLine();
    scanner.SetCommonAncestorWithLastFrame(nullptr);
    nsIFrame* child = line->mFirstChild;
    for (int32_t i = line->GetChildCount() - 1; i >= 0; --i) {
      scanner.ScanFrame(child);
      child = child->GetNextSibling();
    }
    if (line.get() == startLine.get()) {
      seenStartLine = true;
    }
    if (seenStartLine) {
      ++linesAfterStartLine;
      if (linesAfterStartLine >= NUM_LINES_TO_BUILD_TEXT_RUNS && scanner.CanStopOnThisLine()) {
        // Don't flush frames; we may be in the middle of a textrun
        // that we can't end here. That's OK, we just won't build it.
        // Note that we must already have finished the textrun for aForFrame,
        // because we've seen the end of a textrun in a line after the line
        // containing aForFrame.
        scanner.FlushLineBreaks(nullptr);
        // This flushes out mMappedFlows and mLineBreakBeforeFrames, which
        // silences assertions in the scanner destructor.
        scanner.ResetRunInfo();
        return;
      }
    }
  } while (forwardIterator.Next());

  // Set mStartOfLine so FlushFrames knows its textrun ends a line
  scanner.SetAtStartOfLine();
  scanner.FlushFrames(true, false);
}

static char16_t*
ExpandBuffer(char16_t* aDest, uint8_t* aSrc, uint32_t aCount)
{
  while (aCount) {
    *aDest = *aSrc;
    ++aDest;
    ++aSrc;
    --aCount;
  }
  return aDest;
}

bool
BuildTextRunsScanner::IsTextRunValidForMappedFlows(const gfxTextRun* aTextRun)
{
  if (aTextRun->GetFlags2() & nsTextFrameUtils::Flags::TEXT_IS_SIMPLE_FLOW) {
    return mMappedFlows.Length() == 1 &&
      mMappedFlows[0].mStartFrame == GetFrameForSimpleFlow(aTextRun) &&
      mMappedFlows[0].mEndFrame == nullptr;
  }

  auto userData = static_cast<TextRunUserData*>(aTextRun->GetUserData());
  TextRunMappedFlow* userMappedFlows = GetMappedFlows(aTextRun);
  if (userData->mMappedFlowCount != mMappedFlows.Length())
    return false;
  for (uint32_t i = 0; i < mMappedFlows.Length(); ++i) {
    if (userMappedFlows[i].mStartFrame != mMappedFlows[i].mStartFrame ||
        int32_t(userMappedFlows[i].mContentLength) !=
            mMappedFlows[i].GetContentEnd() - mMappedFlows[i].mStartFrame->GetContentOffset())
      return false;
  }
  return true;
}

/**
 * This gets called when we need to make a text run for the current list of
 * frames.
 */
void BuildTextRunsScanner::FlushFrames(bool aFlushLineBreaks, bool aSuppressTrailingBreak)
{
  RefPtr<gfxTextRun> textRun;
  if (!mMappedFlows.IsEmpty()) {
    if (!mSkipIncompleteTextRuns && mCurrentFramesAllSameTextRun &&
        !!(mCurrentFramesAllSameTextRun->GetFlags2() & nsTextFrameUtils::Flags::TEXT_INCOMING_WHITESPACE) ==
        !!(mCurrentRunContextInfo & nsTextFrameUtils::INCOMING_WHITESPACE) &&
        !!(mCurrentFramesAllSameTextRun->GetFlags() & gfx::ShapedTextFlags::TEXT_INCOMING_ARABICCHAR) ==
        !!(mCurrentRunContextInfo & nsTextFrameUtils::INCOMING_ARABICCHAR) &&
        IsTextRunValidForMappedFlows(mCurrentFramesAllSameTextRun)) {
      // Optimization: We do not need to (re)build the textrun.
      textRun = mCurrentFramesAllSameTextRun;

      // Feed this run's text into the linebreaker to provide context.
      if (!SetupLineBreakerContext(textRun)) {
        return;
      }

      // Update mNextRunContextInfo appropriately
      mNextRunContextInfo = nsTextFrameUtils::INCOMING_NONE;
      if (textRun->GetFlags2() & nsTextFrameUtils::Flags::TEXT_TRAILING_WHITESPACE) {
        mNextRunContextInfo |= nsTextFrameUtils::INCOMING_WHITESPACE;
      }
      if (textRun->GetFlags() & gfx::ShapedTextFlags::TEXT_TRAILING_ARABICCHAR) {
        mNextRunContextInfo |= nsTextFrameUtils::INCOMING_ARABICCHAR;
      }
    } else {
      AutoTArray<uint8_t,BIG_TEXT_NODE_SIZE> buffer;
      uint32_t bufferSize = mMaxTextLength*(mDoubleByteText ? 2 : 1);
      if (bufferSize < mMaxTextLength || bufferSize == UINT32_MAX ||
          !buffer.AppendElements(bufferSize, fallible)) {
        return;
      }
      textRun = BuildTextRunForFrames(buffer.Elements());
    }
  }

  if (aFlushLineBreaks) {
    FlushLineBreaks(aSuppressTrailingBreak ? nullptr : textRun.get());
  }

  mCanStopOnThisLine = true;
  ResetRunInfo();
}

void BuildTextRunsScanner::FlushLineBreaks(gfxTextRun* aTrailingTextRun)
{
  bool trailingLineBreak;
  nsresult rv = mLineBreaker.Reset(&trailingLineBreak);
  // textRun may be null for various reasons, including because we constructed
  // a partial textrun just to get the linebreaker and other state set up
  // to build the next textrun.
  if (NS_SUCCEEDED(rv) && trailingLineBreak && aTrailingTextRun) {
    aTrailingTextRun->SetFlagBits(nsTextFrameUtils::Flags::TEXT_HAS_TRAILING_BREAK);
  }

  for (uint32_t i = 0; i < mBreakSinks.Length(); ++i) {
    // TODO cause frames associated with the textrun to be reflowed, if they
    // aren't being reflowed already!
    mBreakSinks[i]->Finish(mMissingFonts);
  }
  mBreakSinks.Clear();
}

void BuildTextRunsScanner::AccumulateRunInfo(nsTextFrame* aFrame)
{
  if (mMaxTextLength != UINT32_MAX) {
    NS_ASSERTION(mMaxTextLength < UINT32_MAX - aFrame->GetContentLength(), "integer overflow");
    if (mMaxTextLength >= UINT32_MAX - aFrame->GetContentLength()) {
      mMaxTextLength = UINT32_MAX;
    } else {
      mMaxTextLength += aFrame->GetContentLength();
    }
  }
  mDoubleByteText |= aFrame->GetContent()->GetText()->Is2b();
  mLastFrame = aFrame;
  mCommonAncestorWithLastFrame = aFrame->GetParent();

  MappedFlow* mappedFlow = &mMappedFlows[mMappedFlows.Length() - 1];
  NS_ASSERTION(mappedFlow->mStartFrame == aFrame ||
               mappedFlow->GetContentEnd() == aFrame->GetContentOffset(),
               "Overlapping or discontiguous frames => BAD");
  mappedFlow->mEndFrame = aFrame->GetNextContinuation();
  if (mCurrentFramesAllSameTextRun != aFrame->GetTextRun(mWhichTextRun)) {
    mCurrentFramesAllSameTextRun = nullptr;
  }

  if (mStartOfLine) {
    mLineBreakBeforeFrames.AppendElement(aFrame);
    mStartOfLine = false;
  }
}

static bool
HasTerminalNewline(const nsTextFrame* aFrame)
{
  if (aFrame->GetContentLength() == 0)
    return false;
  const nsTextFragment* frag = aFrame->GetContent()->GetText();
  return frag->CharAt(aFrame->GetContentEnd() - 1) == '\n';
}

static gfxFont::Metrics
GetFirstFontMetrics(gfxFontGroup* aFontGroup, bool aVerticalMetrics)
{
  if (!aFontGroup)
    return gfxFont::Metrics();
  gfxFont* font = aFontGroup->GetFirstValidFont();
  return font->GetMetrics(aVerticalMetrics ? gfxFont::eVertical
                                           : gfxFont::eHorizontal);
}

static gfxFloat
GetSpaceWidthAppUnits(const gfxTextRun* aTextRun)
{
  // Round the space width when converting to appunits the same way textruns
  // do.
  gfxFloat spaceWidthAppUnits =
    NS_round(GetFirstFontMetrics(aTextRun->GetFontGroup(),
                                 aTextRun->UseCenterBaseline()).spaceWidth *
             aTextRun->GetAppUnitsPerDevUnit());

  return spaceWidthAppUnits;
}

static nscoord
LetterSpacing(nsIFrame* aFrame, const nsStyleText* aStyleText = nullptr)
{
  if (nsSVGUtils::IsInSVGTextSubtree(aFrame)) {
    return 0;
  }
  if (!aStyleText) {
    aStyleText = aFrame->StyleText();
  }

  const nsStyleCoord& coord = aStyleText->mLetterSpacing;
  if (eStyleUnit_Coord == coord.GetUnit()) {
    return coord.GetCoordValue();
  }
  return 0;
}

// This function converts non-coord values (e.g. percentages) to nscoord.
static nscoord
WordSpacing(nsIFrame* aFrame, const gfxTextRun* aTextRun,
            const nsStyleText* aStyleText = nullptr)
{
  if (nsSVGUtils::IsInSVGTextSubtree(aFrame)) {
    return 0;
  }
  if (!aStyleText) {
    aStyleText = aFrame->StyleText();
  }

  const nsStyleCoord& coord = aStyleText->mWordSpacing;
  if (coord.IsCoordPercentCalcUnit()) {
    nscoord pctBasis = coord.HasPercent() ? GetSpaceWidthAppUnits(aTextRun) : 0;
    return coord.ComputeCoordPercentCalc(pctBasis);
  }
  return 0;
}

// Returns gfxTextRunFactory::TEXT_ENABLE_SPACING if non-standard
// letter-spacing or word-spacing is present.
static gfx::ShapedTextFlags
GetSpacingFlags(nsIFrame* aFrame, const nsStyleText* aStyleText = nullptr)
{
  if (nsSVGUtils::IsInSVGTextSubtree(aFrame)) {
    return gfx::ShapedTextFlags();
  }

  const nsStyleText* styleText = aFrame->StyleText();
  const nsStyleCoord& ls = styleText->mLetterSpacing;
  const nsStyleCoord& ws = styleText->mWordSpacing;

  // It's possible to have a calc() value that computes to zero but for which
  // IsDefinitelyZero() is false, in which case we'll return
  // TEXT_ENABLE_SPACING unnecessarily. That's ok because such cases are likely
  // to be rare, and avoiding TEXT_ENABLE_SPACING is just an optimization.
  bool nonStandardSpacing =
    (eStyleUnit_Coord == ls.GetUnit() && ls.GetCoordValue() != 0) ||
    (eStyleUnit_Coord == ws.GetUnit() && ws.GetCoordValue() != 0) ||
    (eStyleUnit_Percent == ws.GetUnit() && ws.GetPercentValue() != 0) ||
    (eStyleUnit_Calc == ws.GetUnit() && !ws.GetCalcValue()->IsDefinitelyZero());

  return nonStandardSpacing
    ? gfx::ShapedTextFlags::TEXT_ENABLE_SPACING
    : gfx::ShapedTextFlags();
}

bool
BuildTextRunsScanner::ContinueTextRunAcrossFrames(nsTextFrame* aFrame1, nsTextFrame* aFrame2)
{
  // We don't need to check font size inflation, since
  // |FindLineContainer| above (via |nsIFrame::CanContinueTextRun|)
  // ensures that text runs never cross block boundaries.  This means
  // that the font size inflation on all text frames in the text run is
  // already guaranteed to be the same as each other (and for the line
  // container).
  if (mBidiEnabled) {
    FrameBidiData data1 = aFrame1->GetBidiData();
    FrameBidiData data2 = aFrame2->GetBidiData();
    if (data1.embeddingLevel != data2.embeddingLevel ||
        data2.precedingControl != kBidiLevelNone) {
      return false;
    }
  }

  nsStyleContext* sc1 = aFrame1->StyleContext();
  const nsStyleText* textStyle1 = sc1->StyleText();
  // If the first frame ends in a preformatted newline, then we end the textrun
  // here. This avoids creating giant textruns for an entire plain text file.
  // Note that we create a single text frame for a preformatted text node,
  // even if it has newlines in it, so typically we won't see trailing newlines
  // until after reflow has broken up the frame into one (or more) frames per
  // line. That's OK though.
  if (textStyle1->NewlineIsSignificant(aFrame1) && HasTerminalNewline(aFrame1))
    return false;

  if (aFrame1->GetContent() == aFrame2->GetContent() &&
      aFrame1->GetNextInFlow() != aFrame2) {
    // aFrame2 must be a non-fluid continuation of aFrame1. This can happen
    // sometimes when the unicode-bidi property is used; the bidi resolver
    // breaks text into different frames even though the text has the same
    // direction. We can't allow these two frames to share the same textrun
    // because that would violate our invariant that two flows in the same
    // textrun have different content elements.
    return false;
  }

  nsStyleContext* sc2 = aFrame2->StyleContext();
  const nsStyleText* textStyle2 = sc2->StyleText();
  if (sc1 == sc2)
    return true;

  const nsStyleFont* fontStyle1 = sc1->StyleFont();
  const nsStyleFont* fontStyle2 = sc2->StyleFont();
  nscoord letterSpacing1 = LetterSpacing(aFrame1);
  nscoord letterSpacing2 = LetterSpacing(aFrame2);
  return fontStyle1->mFont == fontStyle2->mFont &&
    fontStyle1->mLanguage == fontStyle2->mLanguage &&
    textStyle1->mTextTransform == textStyle2->mTextTransform &&
    nsLayoutUtils::GetTextRunFlagsForStyle(sc1, fontStyle1, textStyle1, letterSpacing1) ==
      nsLayoutUtils::GetTextRunFlagsForStyle(sc2, fontStyle2, textStyle2, letterSpacing2);
}

void BuildTextRunsScanner::ScanFrame(nsIFrame* aFrame)
{
  LayoutFrameType frameType = aFrame->Type();
  if (frameType == LayoutFrameType::RubyTextContainer) {
    // Don't include any ruby text container into the text run.
    return;
  }

  // First check if we can extend the current mapped frame block. This is common.
  if (mMappedFlows.Length() > 0) {
    MappedFlow* mappedFlow = &mMappedFlows[mMappedFlows.Length() - 1];
    if (mappedFlow->mEndFrame == aFrame &&
        (aFrame->GetStateBits() & NS_FRAME_IS_FLUID_CONTINUATION)) {
      NS_ASSERTION(frameType == LayoutFrameType::Text,
                   "Flow-sibling of a text frame is not a text frame?");

      // Don't do this optimization if mLastFrame has a terminal newline...
      // it's quite likely preformatted and we might want to end the textrun here.
      // This is almost always true:
      if (mLastFrame->StyleContext() == aFrame->StyleContext() &&
          !HasTerminalNewline(mLastFrame)) {
        AccumulateRunInfo(static_cast<nsTextFrame*>(aFrame));
        return;
      }
    }
  }

  // Now see if we can add a new set of frames to the current textrun
  if (frameType == LayoutFrameType::Text) {
    nsTextFrame* frame = static_cast<nsTextFrame*>(aFrame);

    if (mLastFrame) {
      if (!ContinueTextRunAcrossFrames(mLastFrame, frame)) {
        FlushFrames(false, false);
      } else {
        if (mLastFrame->GetContent() == frame->GetContent()) {
          AccumulateRunInfo(frame);
          return;
        }
      }
    }

    MappedFlow* mappedFlow = mMappedFlows.AppendElement();
    if (!mappedFlow)
      return;

    mappedFlow->mStartFrame = frame;
    mappedFlow->mAncestorControllingInitialBreak = mCommonAncestorWithLastFrame;

    AccumulateRunInfo(frame);
    if (mMappedFlows.Length() == 1) {
      mCurrentFramesAllSameTextRun = frame->GetTextRun(mWhichTextRun);
      mCurrentRunContextInfo = mNextRunContextInfo;
    }
    return;
  }

  FrameTextTraversal traversal = CanTextCrossFrameBoundary(aFrame);
  bool isBR = frameType == LayoutFrameType::Br;
  if (!traversal.mLineBreakerCanCrossFrameBoundary) {
    // BR frames are special. We do not need or want to record a break opportunity
    // before a BR frame.
    FlushFrames(true, isBR);
    mCommonAncestorWithLastFrame = aFrame;
    mNextRunContextInfo &= ~nsTextFrameUtils::INCOMING_WHITESPACE;
    mStartOfLine = false;
  } else if (!traversal.mTextRunCanCrossFrameBoundary) {
    FlushFrames(false, false);
  }

  for (nsIFrame* f = traversal.NextFrameToScan(); f;
       f = traversal.NextFrameToScan()) {
    ScanFrame(f);
  }

  if (!traversal.mLineBreakerCanCrossFrameBoundary) {
    // Really if we're a BR frame this is unnecessary since descendInto will be
    // false. In fact this whole "if" statement should move into the descendInto.
    FlushFrames(true, isBR);
    mCommonAncestorWithLastFrame = aFrame;
    mNextRunContextInfo &= ~nsTextFrameUtils::INCOMING_WHITESPACE;
  } else if (!traversal.mTextRunCanCrossFrameBoundary) {
    FlushFrames(false, false);
  }

  LiftCommonAncestorWithLastFrameToParent(aFrame->GetParent());
}

nsTextFrame*
BuildTextRunsScanner::GetNextBreakBeforeFrame(uint32_t* aIndex)
{
  uint32_t index = *aIndex;
  if (index >= mLineBreakBeforeFrames.Length())
    return nullptr;
  *aIndex = index + 1;
  return static_cast<nsTextFrame*>(mLineBreakBeforeFrames.ElementAt(index));
}

// Bug 1403220: Suspected MSVC PGO miscompilation
#if defined(_MSC_VER) && defined(_M_IX86)
#pragma optimize("", off)
#endif
static gfxFontGroup*
GetFontGroupForFrame(const nsIFrame* aFrame, float aFontSizeInflation,
                     nsFontMetrics** aOutFontMetrics = nullptr)
{
  RefPtr<nsFontMetrics> metrics =
    nsLayoutUtils::GetFontMetricsForFrame(aFrame, aFontSizeInflation);
  gfxFontGroup* fontGroup = metrics->GetThebesFontGroup();

  // Populate outparam before we return:
  if (aOutFontMetrics) {
    metrics.forget(aOutFontMetrics);
  }
  // XXX this is a bit bogus, we're releasing 'metrics' so the
  // returned font-group might actually be torn down, although because
  // of the way the device context caches font metrics, this seems to
  // not actually happen. But we should fix this.
  return fontGroup;
}
#if defined(_MSC_VER) && defined(_M_IX86)
#pragma optimize("", on)
#endif

static already_AddRefed<DrawTarget>
CreateReferenceDrawTarget(const nsTextFrame* aTextFrame)
{
  RefPtr<gfxContext> ctx =
    aTextFrame->PresShell()->CreateReferenceRenderingContext();
  RefPtr<DrawTarget> dt = ctx->GetDrawTarget();
  return dt.forget();
}

static already_AddRefed<gfxTextRun>
GetHyphenTextRun(const gfxTextRun* aTextRun, DrawTarget* aDrawTarget,
                 nsTextFrame* aTextFrame)
{
  RefPtr<DrawTarget> dt = aDrawTarget;
  if (!dt) {
    dt = CreateReferenceDrawTarget(aTextFrame);
    if (!dt) {
      return nullptr;
    }
  }

  return aTextRun->GetFontGroup()->
    MakeHyphenTextRun(dt, aTextRun->GetAppUnitsPerDevUnit());
}

already_AddRefed<gfxTextRun>
BuildTextRunsScanner::BuildTextRunForFrames(void* aTextBuffer)
{
  gfxSkipChars skipChars;

  const void* textPtr = aTextBuffer;
  bool anyTextTransformStyle = false;
  bool anyMathMLStyling = false;
  bool anyTextEmphasis = false;
  uint8_t sstyScriptLevel = 0;
  uint32_t mathFlags = 0;
  gfx::ShapedTextFlags flags = gfx::ShapedTextFlags();
  nsTextFrameUtils::Flags flags2 = nsTextFrameUtils::Flags::TEXT_NO_BREAKS;

  if (mCurrentRunContextInfo & nsTextFrameUtils::INCOMING_WHITESPACE) {
    flags2 |= nsTextFrameUtils::Flags::TEXT_INCOMING_WHITESPACE;
  }
  if (mCurrentRunContextInfo & nsTextFrameUtils::INCOMING_ARABICCHAR) {
    flags |= gfx::ShapedTextFlags::TEXT_INCOMING_ARABICCHAR;
  }

  AutoTArray<int32_t,50> textBreakPoints;
  TextRunUserData dummyData;
  TextRunMappedFlow dummyMappedFlow;
  TextRunMappedFlow* userMappedFlows;
  TextRunUserData* userData;
  TextRunUserData* userDataToDestroy;
  // If the situation is particularly simple (and common) we don't need to
  // allocate userData.
  if (mMappedFlows.Length() == 1 && !mMappedFlows[0].mEndFrame &&
      mMappedFlows[0].mStartFrame->GetContentOffset() == 0) {
    userData = &dummyData;
    userMappedFlows = &dummyMappedFlow;
    userDataToDestroy = nullptr;
    dummyData.mMappedFlowCount = mMappedFlows.Length();
    dummyData.mLastFlowIndex = 0;
  } else {
    userData = CreateUserData(mMappedFlows.Length());
    userMappedFlows = reinterpret_cast<TextRunMappedFlow*>(userData + 1);
    userDataToDestroy = userData;
  }

  uint32_t currentTransformedTextOffset = 0;

  uint32_t nextBreakIndex = 0;
  nsTextFrame* nextBreakBeforeFrame = GetNextBreakBeforeFrame(&nextBreakIndex);
  bool isSVG = nsSVGUtils::IsInSVGTextSubtree(mLineContainer);
  bool enabledJustification =
    (mLineContainer->StyleText()->mTextAlign == NS_STYLE_TEXT_ALIGN_JUSTIFY ||
     mLineContainer->StyleText()->mTextAlignLast == NS_STYLE_TEXT_ALIGN_JUSTIFY);

  const nsStyleText* textStyle = nullptr;
  const nsStyleFont* fontStyle = nullptr;
  nsStyleContext* lastStyleContext = nullptr;
  for (uint32_t i = 0; i < mMappedFlows.Length(); ++i) {
    MappedFlow* mappedFlow = &mMappedFlows[i];
    nsTextFrame* f = mappedFlow->mStartFrame;

    lastStyleContext = f->StyleContext();
    // Detect use of text-transform or font-variant anywhere in the run
    textStyle = f->StyleText();
    if (NS_STYLE_TEXT_TRANSFORM_NONE != textStyle->mTextTransform ||
        // text-combine-upright requires converting from full-width
        // characters to non-full-width correspendent in some cases.
        lastStyleContext->IsTextCombined()) {
      anyTextTransformStyle = true;
    }
    if (textStyle->HasTextEmphasis()) {
      anyTextEmphasis = true;
    }
    flags |= GetSpacingFlags(f);
    nsTextFrameUtils::CompressionMode compression =
      GetCSSWhitespaceToCompressionMode(f, textStyle);
    if ((enabledJustification || f->ShouldSuppressLineBreak()) &&
        !textStyle->WhiteSpaceIsSignificant() && !isSVG) {
      flags |= gfx::ShapedTextFlags::TEXT_ENABLE_SPACING;
    }
    fontStyle = f->StyleFont();
    nsIFrame* parent = mLineContainer->GetParent();
    if (NS_MATHML_MATHVARIANT_NONE != fontStyle->mMathVariant) {
      if (NS_MATHML_MATHVARIANT_NORMAL != fontStyle->mMathVariant) {
        anyMathMLStyling = true;
      }
    } else if (mLineContainer->GetStateBits() & NS_FRAME_IS_IN_SINGLE_CHAR_MI) {
      flags2 |= nsTextFrameUtils::Flags::TEXT_IS_SINGLE_CHAR_MI;
      anyMathMLStyling = true;
      // Test for fontstyle attribute as StyleFont() may not be accurate
      // To be consistent in terms of ignoring CSS style changes, fontweight
      // gets checked too.
      if (parent) {
        nsIContent* content = parent->GetContent();
        if (content) {
          if (content->AttrValueIs(kNameSpaceID_None,
                                  nsGkAtoms::fontstyle_,
                                  NS_LITERAL_STRING("normal"),
                                  eCaseMatters)) {
            mathFlags |= MathMLTextRunFactory::MATH_FONT_STYLING_NORMAL;
          }
          if (content->AttrValueIs(kNameSpaceID_None,
                                   nsGkAtoms::fontweight_,
                                   NS_LITERAL_STRING("bold"),
                                   eCaseMatters)) {
            mathFlags |= MathMLTextRunFactory::MATH_FONT_WEIGHT_BOLD;
          }
        }
      }
    }
    if (mLineContainer->HasAnyStateBits(TEXT_IS_IN_TOKEN_MATHML)) {
      // All MathML tokens except <mtext> use 'math' script.
      if (!(parent && parent->GetContent() &&
          parent->GetContent()->IsMathMLElement(nsGkAtoms::mtext_))) {
        flags |= gfx::ShapedTextFlags::TEXT_USE_MATH_SCRIPT;
      }
      nsIMathMLFrame* mathFrame = do_QueryFrame(parent);
      if (mathFrame) {
        nsPresentationData presData;
        mathFrame->GetPresentationData(presData);
        if (NS_MATHML_IS_DTLS_SET(presData.flags)) {
          mathFlags |= MathMLTextRunFactory::MATH_FONT_FEATURE_DTLS;
          anyMathMLStyling = true;
        }
      }
    }
    nsIFrame* child = mLineContainer;
    uint8_t oldScriptLevel = 0;
    while (parent &&
           child->HasAnyStateBits(NS_FRAME_MATHML_SCRIPT_DESCENDANT)) {
      // Reconstruct the script level ignoring any user overrides. It is
      // calculated this way instead of using scriptlevel to ensure the
      // correct ssty font feature setting is used even if the user sets a
      // different (especially negative) scriptlevel.
      nsIMathMLFrame* mathFrame= do_QueryFrame(parent);
      if (mathFrame) {
        sstyScriptLevel += mathFrame->ScriptIncrement(child);
      }
      if (sstyScriptLevel < oldScriptLevel) {
        // overflow
        sstyScriptLevel = UINT8_MAX;
        break;
      }
      child = parent;
      parent = parent->GetParent();
      oldScriptLevel = sstyScriptLevel;
    }
    if (sstyScriptLevel) {
      anyMathMLStyling = true;
    }

    // Figure out what content is included in this flow.
    nsIContent* content = f->GetContent();
    const nsTextFragment* frag = content->GetText();
    int32_t contentStart = mappedFlow->mStartFrame->GetContentOffset();
    int32_t contentEnd = mappedFlow->GetContentEnd();
    int32_t contentLength = contentEnd - contentStart;

    TextRunMappedFlow* newFlow = &userMappedFlows[i];
    newFlow->mStartFrame = mappedFlow->mStartFrame;
    newFlow->mDOMOffsetToBeforeTransformOffset = skipChars.GetOriginalCharCount() -
      mappedFlow->mStartFrame->GetContentOffset();
    newFlow->mContentLength = contentLength;

    while (nextBreakBeforeFrame && nextBreakBeforeFrame->GetContent() == content) {
      textBreakPoints.AppendElement(
          nextBreakBeforeFrame->GetContentOffset() + newFlow->mDOMOffsetToBeforeTransformOffset);
      nextBreakBeforeFrame = GetNextBreakBeforeFrame(&nextBreakIndex);
    }

    nsTextFrameUtils::Flags analysisFlags;
    if (frag->Is2b()) {
      NS_ASSERTION(mDoubleByteText, "Wrong buffer char size!");
      char16_t* bufStart = static_cast<char16_t*>(aTextBuffer);
      char16_t* bufEnd = nsTextFrameUtils::TransformText(
          frag->Get2b() + contentStart, contentLength, bufStart,
          compression, &mNextRunContextInfo, &skipChars, &analysisFlags);
      aTextBuffer = bufEnd;
      currentTransformedTextOffset = bufEnd - static_cast<const char16_t*>(textPtr);
    } else {
      if (mDoubleByteText) {
        // Need to expand the text. First transform it into a temporary buffer,
        // then expand.
        AutoTArray<uint8_t,BIG_TEXT_NODE_SIZE> tempBuf;
        uint8_t* bufStart = tempBuf.AppendElements(contentLength, fallible);
        if (!bufStart) {
          DestroyUserData(userDataToDestroy);
          return nullptr;
        }
        uint8_t* end = nsTextFrameUtils::TransformText(
            reinterpret_cast<const uint8_t*>(frag->Get1b()) + contentStart, contentLength,
            bufStart, compression, &mNextRunContextInfo, &skipChars, &analysisFlags);
        aTextBuffer = ExpandBuffer(static_cast<char16_t*>(aTextBuffer),
                                   tempBuf.Elements(), end - tempBuf.Elements());
        currentTransformedTextOffset =
          static_cast<char16_t*>(aTextBuffer) - static_cast<const char16_t*>(textPtr);
      } else {
        uint8_t* bufStart = static_cast<uint8_t*>(aTextBuffer);
        uint8_t* end = nsTextFrameUtils::TransformText(
            reinterpret_cast<const uint8_t*>(frag->Get1b()) + contentStart, contentLength,
            bufStart, compression, &mNextRunContextInfo, &skipChars, &analysisFlags);
        aTextBuffer = end;
        currentTransformedTextOffset = end - static_cast<const uint8_t*>(textPtr);
      }
    }
    flags2 |= analysisFlags;
  }

  void* finalUserData;
  if (userData == &dummyData) {
    flags2 |= nsTextFrameUtils::Flags::TEXT_IS_SIMPLE_FLOW;
    userData = nullptr;
    finalUserData = mMappedFlows[0].mStartFrame;
  } else {
    finalUserData = userData;
  }

  uint32_t transformedLength = currentTransformedTextOffset;

  // Now build the textrun
  nsTextFrame* firstFrame = mMappedFlows[0].mStartFrame;
  float fontInflation;
  if (mWhichTextRun == nsTextFrame::eNotInflated) {
    fontInflation = 1.0f;
  } else {
    fontInflation = nsLayoutUtils::FontSizeInflationFor(firstFrame);
  }

  gfxFontGroup* fontGroup = GetFontGroupForFrame(firstFrame, fontInflation);
  if (!fontGroup) {
    DestroyUserData(userDataToDestroy);
    return nullptr;
  }

  if (flags2 & nsTextFrameUtils::Flags::TEXT_HAS_TAB) {
    flags |= gfx::ShapedTextFlags::TEXT_ENABLE_SPACING;
  }
  if (flags2 & nsTextFrameUtils::Flags::TEXT_HAS_SHY) {
    flags |= gfx::ShapedTextFlags::TEXT_ENABLE_HYPHEN_BREAKS;
  }
  if (mBidiEnabled && (IS_LEVEL_RTL(firstFrame->GetEmbeddingLevel()))) {
    flags |= gfx::ShapedTextFlags::TEXT_IS_RTL;
  }
  if (mNextRunContextInfo & nsTextFrameUtils::INCOMING_WHITESPACE) {
    flags2 |= nsTextFrameUtils::Flags::TEXT_TRAILING_WHITESPACE;
  }
  if (mNextRunContextInfo & nsTextFrameUtils::INCOMING_ARABICCHAR) {
    flags |= gfx::ShapedTextFlags::TEXT_TRAILING_ARABICCHAR;
  }
  // ContinueTextRunAcrossFrames guarantees that it doesn't matter which
  // frame's style is used, so we use a mixture of the first frame and
  // last frame's style
  flags |= nsLayoutUtils::GetTextRunFlagsForStyle(lastStyleContext,
      fontStyle, textStyle, LetterSpacing(firstFrame, textStyle));
  // XXX this is a bit of a hack. For performance reasons, if we're favouring
  // performance over quality, don't try to get accurate glyph extents.
  if (!(flags & gfx::ShapedTextFlags::TEXT_OPTIMIZE_SPEED)) {
    flags |= gfx::ShapedTextFlags::TEXT_NEED_BOUNDING_BOX;
  }

  // Convert linebreak coordinates to transformed string offsets
  NS_ASSERTION(nextBreakIndex == mLineBreakBeforeFrames.Length(),
               "Didn't find all the frames to break-before...");
  gfxSkipCharsIterator iter(skipChars);
  AutoTArray<uint32_t,50> textBreakPointsAfterTransform;
  for (uint32_t i = 0; i < textBreakPoints.Length(); ++i) {
    nsTextFrameUtils::AppendLineBreakOffset(&textBreakPointsAfterTransform,
            iter.ConvertOriginalToSkipped(textBreakPoints[i]));
  }
  if (mStartOfLine) {
    nsTextFrameUtils::AppendLineBreakOffset(&textBreakPointsAfterTransform,
                                            transformedLength);
  }

  // Setup factory chain
  UniquePtr<nsTransformingTextRunFactory> transformingFactory;
  if (anyTextTransformStyle) {
    transformingFactory =
      MakeUnique<nsCaseTransformTextRunFactory>(Move(transformingFactory));
  }
  if (anyMathMLStyling) {
    transformingFactory =
      MakeUnique<MathMLTextRunFactory>(Move(transformingFactory), mathFlags,
                                       sstyScriptLevel, fontInflation);
  }
  nsTArray<RefPtr<nsTransformedCharStyle>> styles;
  if (transformingFactory) {
    iter.SetOriginalOffset(0);
    for (uint32_t i = 0; i < mMappedFlows.Length(); ++i) {
      MappedFlow* mappedFlow = &mMappedFlows[i];
      nsTextFrame* f;
      nsStyleContext* sc = nullptr;
      RefPtr<nsTransformedCharStyle> charStyle;
      for (f = mappedFlow->mStartFrame; f != mappedFlow->mEndFrame;
           f = f->GetNextContinuation()) {
        uint32_t offset = iter.GetSkippedOffset();
        iter.AdvanceOriginal(f->GetContentLength());
        uint32_t end = iter.GetSkippedOffset();
        // Text-combined frames have content-dependent transform, so we
        // want to create new nsTransformedCharStyle for them anyway.
        if (sc != f->StyleContext() || sc->IsTextCombined()) {
          sc = f->StyleContext();
          charStyle = new nsTransformedCharStyle(sc);
          if (sc->IsTextCombined() && f->CountGraphemeClusters() > 1) {
            charStyle->mForceNonFullWidth = true;
          }
        }
        uint32_t j;
        for (j = offset; j < end; ++j) {
          styles.AppendElement(charStyle);
        }
      }
    }
    flags2 |= nsTextFrameUtils::Flags::TEXT_IS_TRANSFORMED;
    NS_ASSERTION(iter.GetSkippedOffset() == transformedLength,
                 "We didn't cover all the characters in the text run!");
  }

  RefPtr<gfxTextRun> textRun;
  gfxTextRunFactory::Parameters params =
      { mDrawTarget, finalUserData, &skipChars,
        textBreakPointsAfterTransform.Elements(),
        uint32_t(textBreakPointsAfterTransform.Length()),
        int32_t(firstFrame->PresContext()->AppUnitsPerDevPixel())};

  if (mDoubleByteText) {
    const char16_t* text = static_cast<const char16_t*>(textPtr);
    if (transformingFactory) {
      textRun = transformingFactory->MakeTextRun(text, transformedLength,
                                                 &params, fontGroup, flags, flags2,
                                                 Move(styles), true);
      if (textRun) {
        // ownership of the factory has passed to the textrun
        // TODO: bug 1285316: clean up ownership transfer from the factory to
        // the textrun
        Unused << transformingFactory.release();
      }
    } else {
      textRun = fontGroup->MakeTextRun(text, transformedLength, &params,
                                       flags, flags2, mMissingFonts);
    }
  } else {
    const uint8_t* text = static_cast<const uint8_t*>(textPtr);
    flags |= gfx::ShapedTextFlags::TEXT_IS_8BIT;
    if (transformingFactory) {
      textRun = transformingFactory->MakeTextRun(text, transformedLength,
                                                 &params, fontGroup, flags, flags2,
                                                 Move(styles), true);
      if (textRun) {
        // ownership of the factory has passed to the textrun
        // TODO: bug 1285316: clean up ownership transfer from the factory to
        // the textrun
        Unused << transformingFactory.release();
      }
    } else {
      textRun = fontGroup->MakeTextRun(text, transformedLength, &params,
                                       flags, flags2, mMissingFonts);
    }
  }
  if (!textRun) {
    DestroyUserData(userDataToDestroy);
    return nullptr;
  }

  // We have to set these up after we've created the textrun, because
  // the breaks may be stored in the textrun during this very call.
  // This is a bit annoying because it requires another loop over the frames
  // making up the textrun, but I don't see a way to avoid this.
  SetupBreakSinksForTextRun(textRun.get(), textPtr);

  if (anyTextEmphasis) {
    SetupTextEmphasisForTextRun(textRun.get(), textPtr);
  }

  if (mSkipIncompleteTextRuns) {
    mSkipIncompleteTextRuns = !TextContainsLineBreakerWhiteSpace(textPtr,
        transformedLength, mDoubleByteText);
    // Since we're doing to destroy the user data now, avoid a dangling
    // pointer. Strictly speaking we don't need to do this since it should
    // not be used (since this textrun will not be used and will be
    // itself deleted soon), but it's always better to not have dangling
    // pointers around.
    textRun->SetUserData(nullptr);
    DestroyUserData(userDataToDestroy);
    return nullptr;
  }

  // Actually wipe out the textruns associated with the mapped frames and associate
  // those frames with this text run.
  AssignTextRun(textRun.get(), fontInflation);
  return textRun.forget();
}

// This is a cut-down version of BuildTextRunForFrames used to set up
// context for the line-breaker, when the textrun has already been created.
// So it does the same walk over the mMappedFlows, but doesn't actually
// build a new textrun.
bool
BuildTextRunsScanner::SetupLineBreakerContext(gfxTextRun *aTextRun)
{
  AutoTArray<uint8_t,BIG_TEXT_NODE_SIZE> buffer;
  uint32_t bufferSize = mMaxTextLength*(mDoubleByteText ? 2 : 1);
  if (bufferSize < mMaxTextLength || bufferSize == UINT32_MAX) {
    return false;
  }
  void *textPtr = buffer.AppendElements(bufferSize, fallible);
  if (!textPtr) {
    return false;
  }

  gfxSkipChars skipChars;

  TextRunUserData dummyData;
  TextRunMappedFlow dummyMappedFlow;
  TextRunMappedFlow* userMappedFlows;
  TextRunUserData* userData;
  TextRunUserData* userDataToDestroy;
  // If the situation is particularly simple (and common) we don't need to
  // allocate userData.
  if (mMappedFlows.Length() == 1 && !mMappedFlows[0].mEndFrame &&
      mMappedFlows[0].mStartFrame->GetContentOffset() == 0) {
    userData = &dummyData;
    userMappedFlows = &dummyMappedFlow;
    userDataToDestroy = nullptr;
    dummyData.mMappedFlowCount = mMappedFlows.Length();
    dummyData.mLastFlowIndex = 0;
  } else {
    userData = CreateUserData(mMappedFlows.Length());
    userMappedFlows = reinterpret_cast<TextRunMappedFlow*>(userData + 1);
    userDataToDestroy = userData;
  }

  const nsStyleText* textStyle = nullptr;
  for (uint32_t i = 0; i < mMappedFlows.Length(); ++i) {
    MappedFlow* mappedFlow = &mMappedFlows[i];
    nsTextFrame* f = mappedFlow->mStartFrame;

    textStyle = f->StyleText();
    nsTextFrameUtils::CompressionMode compression =
      GetCSSWhitespaceToCompressionMode(f, textStyle);

    // Figure out what content is included in this flow.
    nsIContent* content = f->GetContent();
    const nsTextFragment* frag = content->GetText();
    int32_t contentStart = mappedFlow->mStartFrame->GetContentOffset();
    int32_t contentEnd = mappedFlow->GetContentEnd();
    int32_t contentLength = contentEnd - contentStart;

    TextRunMappedFlow* newFlow = &userMappedFlows[i];
    newFlow->mStartFrame = mappedFlow->mStartFrame;
    newFlow->mDOMOffsetToBeforeTransformOffset = skipChars.GetOriginalCharCount() -
      mappedFlow->mStartFrame->GetContentOffset();
    newFlow->mContentLength = contentLength;

    nsTextFrameUtils::Flags analysisFlags;
    if (frag->Is2b()) {
      NS_ASSERTION(mDoubleByteText, "Wrong buffer char size!");
      char16_t* bufStart = static_cast<char16_t*>(textPtr);
      char16_t* bufEnd = nsTextFrameUtils::TransformText(
          frag->Get2b() + contentStart, contentLength, bufStart,
          compression, &mNextRunContextInfo, &skipChars, &analysisFlags);
      textPtr = bufEnd;
    } else {
      if (mDoubleByteText) {
        // Need to expand the text. First transform it into a temporary buffer,
        // then expand.
        AutoTArray<uint8_t,BIG_TEXT_NODE_SIZE> tempBuf;
        uint8_t* bufStart = tempBuf.AppendElements(contentLength, fallible);
        if (!bufStart) {
          DestroyUserData(userDataToDestroy);
          return false;
        }
        uint8_t* end = nsTextFrameUtils::TransformText(
            reinterpret_cast<const uint8_t*>(frag->Get1b()) + contentStart, contentLength,
            bufStart, compression, &mNextRunContextInfo, &skipChars, &analysisFlags);
        textPtr = ExpandBuffer(static_cast<char16_t*>(textPtr),
                               tempBuf.Elements(), end - tempBuf.Elements());
      } else {
        uint8_t* bufStart = static_cast<uint8_t*>(textPtr);
        uint8_t* end = nsTextFrameUtils::TransformText(
            reinterpret_cast<const uint8_t*>(frag->Get1b()) + contentStart, contentLength,
            bufStart, compression, &mNextRunContextInfo, &skipChars, &analysisFlags);
        textPtr = end;
      }
    }
  }

  // We have to set these up after we've created the textrun, because
  // the breaks may be stored in the textrun during this very call.
  // This is a bit annoying because it requires another loop over the frames
  // making up the textrun, but I don't see a way to avoid this.
  SetupBreakSinksForTextRun(aTextRun, buffer.Elements());

  DestroyUserData(userDataToDestroy);

  return true;
}

static bool
HasCompressedLeadingWhitespace(nsTextFrame* aFrame, const nsStyleText* aStyleText,
                               int32_t aContentEndOffset,
                               const gfxSkipCharsIterator& aIterator)
{
  if (!aIterator.IsOriginalCharSkipped())
    return false;

  gfxSkipCharsIterator iter = aIterator;
  int32_t frameContentOffset = aFrame->GetContentOffset();
  const nsTextFragment* frag = aFrame->GetContent()->GetText();
  while (frameContentOffset < aContentEndOffset && iter.IsOriginalCharSkipped()) {
    if (IsTrimmableSpace(frag, frameContentOffset, aStyleText))
      return true;
    ++frameContentOffset;
    iter.AdvanceOriginal(1);
  }
  return false;
}

void
BuildTextRunsScanner::SetupBreakSinksForTextRun(gfxTextRun* aTextRun,
                                                const void* aTextPtr)
{
  using mozilla::intl::LineBreaker;

  // for word-break style
  switch (mLineContainer->StyleText()->mWordBreak) {
    case NS_STYLE_WORDBREAK_BREAK_ALL:
      mLineBreaker.SetWordBreak(LineBreaker::kWordBreak_BreakAll);
      break;
    case NS_STYLE_WORDBREAK_KEEP_ALL:
      mLineBreaker.SetWordBreak(LineBreaker::kWordBreak_KeepAll);
      break;
    default:
      mLineBreaker.SetWordBreak(LineBreaker::kWordBreak_Normal);
      break;
  }

  // textruns have uniform language
  const nsStyleFont *styleFont = mMappedFlows[0].mStartFrame->StyleFont();
  // We should only use a language for hyphenation if it was specified
  // explicitly.
  nsAtom* hyphenationLanguage =
    styleFont->mExplicitLanguage ? styleFont->mLanguage.get() : nullptr;
  // We keep this pointed at the skip-chars data for the current mappedFlow.
  // This lets us cheaply check whether the flow has compressed initial
  // whitespace...
  gfxSkipCharsIterator iter(aTextRun->GetSkipChars());

  for (uint32_t i = 0; i < mMappedFlows.Length(); ++i) {
    MappedFlow* mappedFlow = &mMappedFlows[i];
    uint32_t offset = iter.GetSkippedOffset();
    gfxSkipCharsIterator iterNext = iter;
    iterNext.AdvanceOriginal(mappedFlow->GetContentEnd() -
            mappedFlow->mStartFrame->GetContentOffset());

    UniquePtr<BreakSink>* breakSink =
      mBreakSinks.AppendElement(MakeUnique<BreakSink>(aTextRun, mDrawTarget, offset));
    if (!breakSink || !*breakSink)
      return;

    uint32_t length = iterNext.GetSkippedOffset() - offset;
    uint32_t flags = 0;
    nsIFrame* initialBreakController = mappedFlow->mAncestorControllingInitialBreak;
    if (!initialBreakController) {
      initialBreakController = mLineContainer;
    }
    if (!initialBreakController->StyleText()->
                                 WhiteSpaceCanWrap(initialBreakController)) {
      flags |= nsLineBreaker::BREAK_SUPPRESS_INITIAL;
    }
    nsTextFrame* startFrame = mappedFlow->mStartFrame;
    const nsStyleText* textStyle = startFrame->StyleText();
    if (!textStyle->WhiteSpaceCanWrap(startFrame)) {
      flags |= nsLineBreaker::BREAK_SUPPRESS_INSIDE;
    }
    if (aTextRun->GetFlags2() & nsTextFrameUtils::Flags::TEXT_NO_BREAKS) {
      flags |= nsLineBreaker::BREAK_SKIP_SETTING_NO_BREAKS;
    }
    if (textStyle->mTextTransform == NS_STYLE_TEXT_TRANSFORM_CAPITALIZE) {
      flags |= nsLineBreaker::BREAK_NEED_CAPITALIZATION;
    }
    if (textStyle->mHyphens == StyleHyphens::Auto) {
      flags |= nsLineBreaker::BREAK_USE_AUTO_HYPHENATION;
    }

    if (HasCompressedLeadingWhitespace(startFrame, textStyle,
                                       mappedFlow->GetContentEnd(), iter)) {
      mLineBreaker.AppendInvisibleWhitespace(flags);
    }

    if (length > 0) {
      BreakSink* sink =
        mSkipIncompleteTextRuns ? nullptr : (*breakSink).get();
      if (mDoubleByteText) {
        const char16_t* text = reinterpret_cast<const char16_t*>(aTextPtr);
        mLineBreaker.AppendText(hyphenationLanguage, text + offset,
                                length, flags, sink);
      } else {
        const uint8_t* text = reinterpret_cast<const uint8_t*>(aTextPtr);
        mLineBreaker.AppendText(hyphenationLanguage, text + offset,
                                length, flags, sink);
      }
    }

    iter = iterNext;
  }
}

static bool
MayCharacterHaveEmphasisMark(uint32_t aCh)
{
  auto category = unicode::GetGeneralCategory(aCh);
  // Comparing an unsigned variable against zero is a compile error,
  // so we use static assert here to ensure we really don't need to
  // compare it with the given constant.
  static_assert(IsUnsigned<decltype(category)>::value &&
                HB_UNICODE_GENERAL_CATEGORY_CONTROL == 0,
                "if this constant is not zero, or category is signed, "
                "we need to explicitly do the comparison below");
  return !(category <= HB_UNICODE_GENERAL_CATEGORY_UNASSIGNED ||
           (category >= HB_UNICODE_GENERAL_CATEGORY_LINE_SEPARATOR &&
            category <= HB_UNICODE_GENERAL_CATEGORY_SPACE_SEPARATOR));
}

static bool
MayCharacterHaveEmphasisMark(uint8_t aCh)
{
  // 0x00~0x1f and 0x7f~0x9f are in category Cc
  // 0x20 and 0xa0 are in category Zs
  bool result = !(aCh <= 0x20 || (aCh >= 0x7f && aCh <= 0xa0));
  MOZ_ASSERT(result == MayCharacterHaveEmphasisMark(uint32_t(aCh)),
             "result for uint8_t should match result for uint32_t");
  return result;
}

void
BuildTextRunsScanner::SetupTextEmphasisForTextRun(gfxTextRun* aTextRun,
                                                  const void* aTextPtr)
{
  if (!mDoubleByteText) {
    auto text = reinterpret_cast<const uint8_t*>(aTextPtr);
    for (auto i : IntegerRange(aTextRun->GetLength())) {
      if (!MayCharacterHaveEmphasisMark(text[i])) {
        aTextRun->SetNoEmphasisMark(i);
      }
    }
  } else {
    auto text = reinterpret_cast<const char16_t*>(aTextPtr);
    auto length = aTextRun->GetLength();
    for (size_t i = 0; i < length; ++i) {
      if (NS_IS_HIGH_SURROGATE(text[i]) && i + 1 < length &&
          NS_IS_LOW_SURROGATE(text[i + 1])) {
        uint32_t ch = SURROGATE_TO_UCS4(text[i], text[i + 1]);
        if (!MayCharacterHaveEmphasisMark(ch)) {
          aTextRun->SetNoEmphasisMark(i);
          aTextRun->SetNoEmphasisMark(i + 1);
        }
        ++i;
      } else {
        if (!MayCharacterHaveEmphasisMark(uint32_t(text[i]))) {
          aTextRun->SetNoEmphasisMark(i);
        }
      }
    }
  }
}

// Find the flow corresponding to aContent in aUserData
static inline TextRunMappedFlow*
FindFlowForContent(TextRunUserData* aUserData, nsIContent* aContent,
                   TextRunMappedFlow* userMappedFlows)
{
  // Find the flow that contains us
  int32_t i = aUserData->mLastFlowIndex;
  int32_t delta = 1;
  int32_t sign = 1;
  // Search starting at the current position and examine close-by
  // positions first, moving further and further away as we go.
  while (i >= 0 && uint32_t(i) < aUserData->mMappedFlowCount) {
    TextRunMappedFlow* flow = &userMappedFlows[i];
    if (flow->mStartFrame->GetContent() == aContent) {
      return flow;
    }

    i += delta;
    sign = -sign;
    delta = -delta + sign;
  }

  // We ran into an array edge.  Add |delta| to |i| once more to get
  // back to the side where we still need to search, then step in
  // the |sign| direction.
  i += delta;
  if (sign > 0) {
    for (; i < int32_t(aUserData->mMappedFlowCount); ++i) {
      TextRunMappedFlow* flow = &userMappedFlows[i];
      if (flow->mStartFrame->GetContent() == aContent) {
        return flow;
      }
    }
  } else {
    for (; i >= 0; --i) {
      TextRunMappedFlow* flow = &userMappedFlows[i];
      if (flow->mStartFrame->GetContent() == aContent) {
        return flow;
      }
    }
  }

  return nullptr;
}

void
BuildTextRunsScanner::AssignTextRun(gfxTextRun* aTextRun, float aInflation)
{
  for (uint32_t i = 0; i < mMappedFlows.Length(); ++i) {
    MappedFlow* mappedFlow = &mMappedFlows[i];
    nsTextFrame* startFrame = mappedFlow->mStartFrame;
    nsTextFrame* endFrame = mappedFlow->mEndFrame;
    nsTextFrame* f;
    for (f = startFrame; f != endFrame; f = f->GetNextContinuation()) {
#ifdef DEBUG_roc
      if (f->GetTextRun(mWhichTextRun)) {
        gfxTextRun* textRun = f->GetTextRun(mWhichTextRun);
        if (textRun->GetFlags2() & nsTextFrameUtils::Flags::TEXT_IS_SIMPLE_FLOW) {
          if (mMappedFlows[0].mStartFrame != GetFrameForSimpleFlow(textRun)) {
            NS_WARNING("REASSIGNING SIMPLE FLOW TEXT RUN!");
          }
        } else {
          auto userData = static_cast<TextRunUserData*>(aTextRun->GetUserData());
          TextRunMappedFlow* userMappedFlows = GetMappedFlows(aTextRun);
          if (userData->mMappedFlowCount >= mMappedFlows.Length() ||
              userMappedFlows[userData->mMappedFlowCount - 1].mStartFrame !=
              mMappedFlows[userdata->mMappedFlowCount - 1].mStartFrame) {
            NS_WARNING("REASSIGNING MULTIFLOW TEXT RUN (not append)!");
          }
        }
      }
#endif

      gfxTextRun* oldTextRun = f->GetTextRun(mWhichTextRun);
      if (oldTextRun) {
        nsTextFrame* firstFrame = nullptr;
        uint32_t startOffset = 0;
        if (oldTextRun->GetFlags2() & nsTextFrameUtils::Flags::TEXT_IS_SIMPLE_FLOW) {
          firstFrame = GetFrameForSimpleFlow(oldTextRun);
        } else {
          auto userData = static_cast<TextRunUserData*>(oldTextRun->GetUserData());
          TextRunMappedFlow* userMappedFlows = GetMappedFlows(oldTextRun);
          firstFrame = userMappedFlows[0].mStartFrame;
          if (MOZ_UNLIKELY(f != firstFrame)) {
            TextRunMappedFlow* flow =
              FindFlowForContent(userData, f->GetContent(), userMappedFlows);
            if (flow) {
              startOffset = flow->mDOMOffsetToBeforeTransformOffset;
            } else {
              NS_ERROR("Can't find flow containing frame 'f'");
            }
          }
        }

        // Optimization: if |f| is the first frame in the flow then there are no
        // prev-continuations that use |oldTextRun|.
        nsTextFrame* clearFrom = nullptr;
        if (MOZ_UNLIKELY(f != firstFrame)) {
          // If all the frames in the mapped flow starting at |f| (inclusive)
          // are empty then we let the prev-continuations keep the old text run.
          gfxSkipCharsIterator iter(oldTextRun->GetSkipChars(), startOffset, f->GetContentOffset());
          uint32_t textRunOffset = iter.ConvertOriginalToSkipped(f->GetContentOffset());
          clearFrom = textRunOffset == oldTextRun->GetLength() ? f : nullptr;
        }
        f->ClearTextRun(clearFrom, mWhichTextRun);

#ifdef DEBUG
        if (firstFrame && !firstFrame->GetTextRun(mWhichTextRun)) {
          // oldTextRun was destroyed - assert that we don't reference it.
          for (uint32_t j = 0; j < mBreakSinks.Length(); ++j) {
            NS_ASSERTION(oldTextRun != mBreakSinks[j]->mTextRun,
                         "destroyed text run is still in use");
          }
        }
#endif
      }
      f->SetTextRun(aTextRun, mWhichTextRun, aInflation);
    }
    // Set this bit now; we can't set it any earlier because
    // f->ClearTextRun() might clear it out.
    nsFrameState whichTextRunState =
      startFrame->GetTextRun(nsTextFrame::eInflated) == aTextRun
        ? TEXT_IN_TEXTRUN_USER_DATA
        : TEXT_IN_UNINFLATED_TEXTRUN_USER_DATA;
    startFrame->AddStateBits(whichTextRunState);
  }
}

NS_QUERYFRAME_HEAD(nsTextFrame)
  NS_QUERYFRAME_ENTRY(nsTextFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsFrame)

gfxSkipCharsIterator
nsTextFrame::EnsureTextRun(TextRunType aWhichTextRun,
                           DrawTarget* aRefDrawTarget,
                           nsIFrame* aLineContainer,
                           const nsLineList::iterator* aLine,
                           uint32_t* aFlowEndInTextRun)
{
  gfxTextRun *textRun = GetTextRun(aWhichTextRun);
  if (!textRun || (aLine && (*aLine)->GetInvalidateTextRuns())) {
    RefPtr<DrawTarget> refDT = aRefDrawTarget;
    if (!refDT) {
      refDT = CreateReferenceDrawTarget(this);
    }
    if (refDT) {
      BuildTextRuns(refDT, this, aLineContainer, aLine, aWhichTextRun);
    }
    textRun = GetTextRun(aWhichTextRun);
    if (!textRun) {
      // A text run was not constructed for this frame. This is bad. The caller
      // will check mTextRun.
      return gfxSkipCharsIterator(gfxPlatform::
                                  GetPlatform()->EmptySkipChars(), 0);
    }
    TabWidthStore* tabWidths = GetProperty(TabWidthProperty());
    if (tabWidths && tabWidths->mValidForContentOffset != GetContentOffset()) {
      DeleteProperty(TabWidthProperty());
    }
  }

  if (textRun->GetFlags2() & nsTextFrameUtils::Flags::TEXT_IS_SIMPLE_FLOW) {
    if (aFlowEndInTextRun) {
      *aFlowEndInTextRun = textRun->GetLength();
    }
    return gfxSkipCharsIterator(textRun->GetSkipChars(), 0, mContentOffset);
  }

  auto userData = static_cast<TextRunUserData*>(textRun->GetUserData());
  TextRunMappedFlow* userMappedFlows = GetMappedFlows(textRun);
  TextRunMappedFlow* flow =
    FindFlowForContent(userData, mContent, userMappedFlows);
  if (flow) {
    // Since textruns can only contain one flow for a given content element,
    // this must be our flow.
    uint32_t flowIndex = flow - userMappedFlows;
    userData->mLastFlowIndex = flowIndex;
    gfxSkipCharsIterator iter(textRun->GetSkipChars(),
                              flow->mDOMOffsetToBeforeTransformOffset, mContentOffset);
    if (aFlowEndInTextRun) {
      if (flowIndex + 1 < userData->mMappedFlowCount) {
        gfxSkipCharsIterator end(textRun->GetSkipChars());
        *aFlowEndInTextRun = end.ConvertOriginalToSkipped(
              flow[1].mStartFrame->GetContentOffset() + flow[1].mDOMOffsetToBeforeTransformOffset);
      } else {
        *aFlowEndInTextRun = textRun->GetLength();
      }
    }
    return iter;
  }

  NS_ERROR("Can't find flow containing this frame???");
  return gfxSkipCharsIterator(gfxPlatform::GetPlatform()->EmptySkipChars(), 0);
}

static uint32_t
GetEndOfTrimmedText(const nsTextFragment* aFrag, const nsStyleText* aStyleText,
                    uint32_t aStart, uint32_t aEnd,
                    gfxSkipCharsIterator* aIterator)
{
  aIterator->SetSkippedOffset(aEnd);
  while (aIterator->GetSkippedOffset() > aStart) {
    aIterator->AdvanceSkipped(-1);
    if (!IsTrimmableSpace(aFrag, aIterator->GetOriginalOffset(), aStyleText))
      return aIterator->GetSkippedOffset() + 1;
  }
  return aStart;
}

nsTextFrame::TrimmedOffsets
nsTextFrame::GetTrimmedOffsets(const nsTextFragment* aFrag,
                               bool aTrimAfter, bool aPostReflow) const
{
  NS_ASSERTION(mTextRun, "Need textrun here");
  if (aPostReflow) {
    // This should not be used during reflow. We need our TEXT_REFLOW_FLAGS
    // to be set correctly.  If our parent wasn't reflowed due to the frame
    // tree being too deep then the return value doesn't matter.
    NS_ASSERTION(!(GetStateBits() & NS_FRAME_FIRST_REFLOW) ||
                 (GetParent()->GetStateBits() &
                  NS_FRAME_TOO_DEEP_IN_FRAME_TREE),
                 "Can only call this on frames that have been reflowed");
    NS_ASSERTION(!(GetStateBits() & NS_FRAME_IN_REFLOW),
                 "Can only call this on frames that are not being reflowed");
  }

  TrimmedOffsets offsets = { GetContentOffset(), GetContentLength() };
  const nsStyleText* textStyle = StyleText();
  // Note that pre-line newlines should still allow us to trim spaces
  // for display
  if (textStyle->WhiteSpaceIsSignificant())
    return offsets;

  if (!aPostReflow || (GetStateBits() & TEXT_START_OF_LINE)) {
    int32_t whitespaceCount =
      GetTrimmableWhitespaceCount(aFrag,
                                  offsets.mStart, offsets.mLength, 1);
    offsets.mStart += whitespaceCount;
    offsets.mLength -= whitespaceCount;
  }

  if (aTrimAfter && (!aPostReflow || (GetStateBits() & TEXT_END_OF_LINE))) {
    // This treats a trailing 'pre-line' newline as trimmable. That's fine,
    // it's actually what we want since we want whitespace before it to
    // be trimmed.
    int32_t whitespaceCount =
      GetTrimmableWhitespaceCount(aFrag,
                                  offsets.GetEnd() - 1, offsets.mLength, -1);
    offsets.mLength -= whitespaceCount;
  }
  return offsets;
}

static bool IsJustifiableCharacter(const nsStyleText* aTextStyle,
                                   const nsTextFragment* aFrag, int32_t aPos,
                                   bool aLangIsCJ)
{
  NS_ASSERTION(aPos >= 0, "negative position?!");

  StyleTextJustify justifyStyle = aTextStyle->mTextJustify;
  if (justifyStyle == StyleTextJustify::None) {
    return false;
  }

  char16_t ch = aFrag->CharAt(aPos);
  if (ch == '\n' || ch == '\t' || ch == '\r') {
    return true;
  }
  if (ch == ' ' || ch == CH_NBSP) {
    // Don't justify spaces that are combined with diacriticals
    if (!aFrag->Is2b()) {
      return true;
    }
    return !nsTextFrameUtils::IsSpaceCombiningSequenceTail(
      aFrag->Get2b() + aPos + 1, aFrag->GetLength() - (aPos + 1));
  }

  if (justifyStyle == StyleTextJustify::InterCharacter) {
    return true;
  } else if (justifyStyle == StyleTextJustify::InterWord) {
    return false;
  }

  // text-justify: auto
  if (ch < 0x2150u) {
    return false;
  }
  if (aLangIsCJ) {
    if ((0x2150u <= ch && ch <= 0x22ffu) || // Number Forms, Arrows, Mathematical Operators
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
       ) {
      return true;
    }
    char16_t ch2;
    if (NS_IS_HIGH_SURROGATE(ch) && aFrag->GetLength() > uint32_t(aPos) + 1 &&
        NS_IS_LOW_SURROGATE(ch2 = aFrag->CharAt(aPos + 1))) {
      uint32_t u = SURROGATE_TO_UCS4(ch, ch2);
      if (0x20000u <= u && u <= 0x2ffffu) { // CJK Unified Ideographs Extension B,
                                            // CJK Unified Ideographs Extension C,
                                            // CJK Unified Ideographs Extension D,
                                            // CJK Compatibility Ideographs Supplement
        return true;
      }
    }
  }
  return false;
}

void
nsTextFrame::ClearMetrics(ReflowOutput& aMetrics)
{
  aMetrics.ClearSize();
  aMetrics.SetBlockStartAscent(0);
  mAscent = 0;

  AddStateBits(TEXT_NO_RENDERED_GLYPHS);
}

static int32_t FindChar(const nsTextFragment* frag,
                        int32_t aOffset, int32_t aLength, char16_t ch)
{
  int32_t i = 0;
  if (frag->Is2b()) {
    const char16_t* str = frag->Get2b() + aOffset;
    for (; i < aLength; ++i) {
      if (*str == ch)
        return i + aOffset;
      ++str;
    }
  } else {
    if (uint16_t(ch) <= 0xFF) {
      const char* str = frag->Get1b() + aOffset;
      const void* p = memchr(str, ch, aLength);
      if (p)
        return (static_cast<const char*>(p) - str) + aOffset;
    }
  }
  return -1;
}

static bool IsChineseOrJapanese(const nsTextFrame* aFrame)
{
  if (aFrame->ShouldSuppressLineBreak()) {
    // Always treat ruby as CJ language so that those characters can
    // be expanded properly even when surrounded by other language.
    return true;
  }

  nsAtom* language = aFrame->StyleFont()->mLanguage;
  if (!language) {
    return false;
  }
  return nsStyleUtil::MatchesLanguagePrefix(language, u"ja") ||
         nsStyleUtil::MatchesLanguagePrefix(language, u"zh");
}

#ifdef DEBUG
static bool IsInBounds(const gfxSkipCharsIterator& aStart, int32_t aContentLength,
                       gfxTextRun::Range aRange) {
  if (aStart.GetSkippedOffset() > aRange.start)
    return false;
  if (aContentLength == INT32_MAX)
    return true;
  gfxSkipCharsIterator iter(aStart);
  iter.AdvanceOriginal(aContentLength);
  return iter.GetSkippedOffset() >= aRange.end;
}
#endif

class MOZ_STACK_CLASS PropertyProvider final : public gfxTextRun::PropertyProvider {
  typedef gfxTextRun::Range Range;
  typedef gfxTextRun::HyphenType HyphenType;

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
                   nsTextFrame::TextRunType aWhichTextRun)
    : mTextRun(aTextRun), mFontGroup(nullptr),
      mTextStyle(aTextStyle), mFrag(aFrag),
      mLineContainer(aLineContainer),
      mFrame(aFrame), mStart(aStart), mTempIterator(aStart),
      mTabWidths(nullptr), mTabWidthsAnalyzedLimit(0),
      mLength(aLength),
      mWordSpacing(WordSpacing(aFrame, mTextRun, aTextStyle)),
      mLetterSpacing(LetterSpacing(aFrame, aTextStyle)),
      mHyphenWidth(-1),
      mOffsetFromBlockOriginForTabs(aOffsetFromBlockOriginForTabs),
      mReflowing(true),
      mWhichTextRun(aWhichTextRun)
  {
    NS_ASSERTION(mStart.IsInitialized(), "Start not initialized?");
  }

  /**
   * Use this constructor after the frame has been reflowed and we don't
   * have other data around. Gets everything from the frame. EnsureTextRun
   * *must* be called before this!!!
   */
  PropertyProvider(nsTextFrame* aFrame, const gfxSkipCharsIterator& aStart,
                   nsTextFrame::TextRunType aWhichTextRun)
    : mTextRun(aFrame->GetTextRun(aWhichTextRun)), mFontGroup(nullptr),
      mTextStyle(aFrame->StyleText()),
      mFrag(aFrame->GetContent()->GetText()),
      mLineContainer(nullptr),
      mFrame(aFrame), mStart(aStart), mTempIterator(aStart),
      mTabWidths(nullptr), mTabWidthsAnalyzedLimit(0),
      mLength(aFrame->GetContentLength()),
      mWordSpacing(WordSpacing(aFrame, mTextRun)),
      mLetterSpacing(LetterSpacing(aFrame)),
      mHyphenWidth(-1),
      mOffsetFromBlockOriginForTabs(0),
      mReflowing(false),
      mWhichTextRun(aWhichTextRun)
  {
    NS_ASSERTION(mTextRun, "Textrun not initialized!");
  }

  // Call this after construction if you're not going to reflow the text
  void InitializeForDisplay(bool aTrimAfter);

  void InitializeForMeasure();

  void GetSpacing(Range aRange, Spacing* aSpacing) const;
  gfxFloat GetHyphenWidth() const;
  void GetHyphenationBreaks(Range aRange, HyphenType* aBreakBefore) const;
  StyleHyphens GetHyphensOption() const {
    return mTextStyle->mHyphens;
  }

  already_AddRefed<DrawTarget> GetDrawTarget() const {
    return CreateReferenceDrawTarget(GetFrame());
  }

  uint32_t GetAppUnitsPerDevUnit() const {
    return mTextRun->GetAppUnitsPerDevUnit();
  }

  void GetSpacingInternal(Range aRange, Spacing* aSpacing, bool aIgnoreTabs) const;

  /**
   * Compute the justification information in given DOM range, return
   * justification info and assignments if requested.
   */
  JustificationInfo ComputeJustification(
    Range aRange, nsTArray<JustificationAssignment>* aAssignments = nullptr);

  const nsTextFrame* GetFrame() const { return mFrame; }
  // This may not be equal to the frame offset/length in because we may have
  // adjusted for whitespace trimming according to the state bits set in the frame
  // (for the static provider)
  const gfxSkipCharsIterator& GetStart() const { return mStart; }
  // May return INT32_MAX if that was given to the constructor
  uint32_t GetOriginalLength() const {
    NS_ASSERTION(mLength != INT32_MAX, "Length not known");
    return mLength;
  }
  const nsTextFragment* GetFragment() const { return mFrag; }

  gfxFontGroup* GetFontGroup() const {
    if (!mFontGroup) {
      InitFontGroupAndFontMetrics();
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

  const gfxSkipCharsIterator& GetEndHint() const { return mTempIterator; }

protected:
  void SetupJustificationSpacing(bool aPostReflow);

  void InitFontGroupAndFontMetrics() const {
    float inflation = (mWhichTextRun == nsTextFrame::eInflated)
      ? mFrame->GetFontSizeInflation() : 1.0f;
    mFontGroup = GetFontGroupForFrame(mFrame, inflation,
                                      getter_AddRefs(mFontMetrics));
  }

  const RefPtr<gfxTextRun>        mTextRun;
  mutable gfxFontGroup*           mFontGroup;
  mutable RefPtr<nsFontMetrics>   mFontMetrics;
  const nsStyleText*              mTextStyle;
  const nsTextFragment*           mFrag;
  const nsIFrame*                 mLineContainer;
  nsTextFrame*                    mFrame;
  gfxSkipCharsIterator            mStart;  // Offset in original and transformed string
  const gfxSkipCharsIterator      mTempIterator;

  // Either null, or pointing to the frame's TabWidthProperty.
  mutable TabWidthStore*          mTabWidths;
  // How far we've done tab-width calculation; this is ONLY valid when
  // mTabWidths is nullptr (otherwise rely on mTabWidths->mLimit instead).
  // It's a DOM offset relative to the current frame's offset.
  mutable uint32_t                mTabWidthsAnalyzedLimit;

  int32_t                         mLength;  // DOM string length, may be INT32_MAX
  const gfxFloat                  mWordSpacing; // space for each whitespace char
  const gfxFloat                  mLetterSpacing; // space for each letter
  mutable gfxFloat                mHyphenWidth;
  mutable gfxFloat                mOffsetFromBlockOriginForTabs;

  // The values in mJustificationSpacings corresponds to unskipped
  // characters start from mJustificationArrayStart.
  uint32_t                        mJustificationArrayStart;
  nsTArray<Spacing>               mJustificationSpacings;

  const bool                      mReflowing;
  const nsTextFrame::TextRunType  mWhichTextRun;
};

/**
 * Finds the offset of the first character of the cluster containing aPos
 */
static void FindClusterStart(const gfxTextRun* aTextRun,
                             int32_t aOriginalStart,
                             gfxSkipCharsIterator* aPos)
{
  while (aPos->GetOriginalOffset() > aOriginalStart) {
    if (aPos->IsOriginalCharSkipped() ||
        aTextRun->IsClusterStart(aPos->GetSkippedOffset())) {
      break;
    }
    aPos->AdvanceOriginal(-1);
  }
}

/**
 * Finds the offset of the last character of the cluster containing aPos.
 * If aAllowSplitLigature is false, we also check for a ligature-group
 * start.
 */
static void FindClusterEnd(const gfxTextRun* aTextRun,
                           int32_t aOriginalEnd,
                           gfxSkipCharsIterator* aPos,
                           bool aAllowSplitLigature = true)
{
  NS_PRECONDITION(aPos->GetOriginalOffset() < aOriginalEnd,
                  "character outside string");
  aPos->AdvanceOriginal(1);
  while (aPos->GetOriginalOffset() < aOriginalEnd) {
    if (aPos->IsOriginalCharSkipped() ||
        (aTextRun->IsClusterStart(aPos->GetSkippedOffset()) &&
         (aAllowSplitLigature ||
          aTextRun->IsLigatureGroupStart(aPos->GetSkippedOffset())))) {
      break;
    }
    aPos->AdvanceOriginal(1);
  }
  aPos->AdvanceOriginal(-1);
}

JustificationInfo
PropertyProvider::ComputeJustification(
  Range aRange, nsTArray<JustificationAssignment>* aAssignments)
{
  JustificationInfo info;

  // Horizontal-in-vertical frame is orthogonal to the line, so it
  // doesn't actually include any justification opportunity inside.
  // The spec says such frame should be treated as a U+FFFC. Since we
  // do not insert justification opportunities on the sides of that
  // character, the sides of this frame are not justifiable either.
  if (mFrame->StyleContext()->IsTextCombined()) {
    return info;
  }

  bool isCJ = IsChineseOrJapanese(mFrame);
  nsSkipCharsRunIterator run(
    mStart, nsSkipCharsRunIterator::LENGTH_INCLUDES_SKIPPED, aRange.Length());
  run.SetOriginalOffset(aRange.start);
  mJustificationArrayStart = run.GetSkippedOffset();

  nsTArray<JustificationAssignment> assignments;
  assignments.SetCapacity(aRange.Length());
  while (run.NextRun()) {
    uint32_t originalOffset = run.GetOriginalOffset();
    uint32_t skippedOffset = run.GetSkippedOffset();
    uint32_t length = run.GetRunLength();
    assignments.SetLength(skippedOffset + length - mJustificationArrayStart);

    gfxSkipCharsIterator iter = run.GetPos();
    for (uint32_t i = 0; i < length; ++i) {
      uint32_t offset = originalOffset + i;
      if (!IsJustifiableCharacter(mTextStyle, mFrag, offset, isCJ)) {
        continue;
      }

      iter.SetOriginalOffset(offset);

      FindClusterStart(mTextRun, originalOffset, &iter);
      uint32_t firstCharOffset = iter.GetSkippedOffset();
      uint32_t firstChar = firstCharOffset > mJustificationArrayStart ?
        firstCharOffset - mJustificationArrayStart : 0;
      if (!firstChar) {
        info.mIsStartJustifiable = true;
      } else {
        auto& assign = assignments[firstChar];
        auto& prevAssign = assignments[firstChar - 1];
        if (prevAssign.mGapsAtEnd) {
          prevAssign.mGapsAtEnd = 1;
          assign.mGapsAtStart = 1;
        } else {
          assign.mGapsAtStart = 2;
          info.mInnerOpportunities++;
        }
      }

      FindClusterEnd(mTextRun, originalOffset + length, &iter);
      uint32_t lastChar = iter.GetSkippedOffset() - mJustificationArrayStart;
      // Assign the two gaps temporary to the last char. If the next cluster is
      // justifiable as well, one of the gaps will be removed by code above.
      assignments[lastChar].mGapsAtEnd = 2;
      info.mInnerOpportunities++;

      // Skip the whole cluster
      i = iter.GetOriginalOffset() - originalOffset;
    }
  }

  if (!assignments.IsEmpty() && assignments.LastElement().mGapsAtEnd) {
    // We counted the expansion opportunity after the last character,
    // but it is not an inner opportunity.
    MOZ_ASSERT(info.mInnerOpportunities > 0);
    info.mInnerOpportunities--;
    info.mIsEndJustifiable = true;
  }

  if (aAssignments) {
    *aAssignments = Move(assignments);
  }
  return info;
}

// aStart, aLength in transformed string offsets
void
PropertyProvider::GetSpacing(Range aRange, Spacing* aSpacing) const
{
  GetSpacingInternal(aRange, aSpacing,
                     !(mTextRun->GetFlags2() & nsTextFrameUtils::Flags::TEXT_HAS_TAB));
}

static bool
CanAddSpacingAfter(const gfxTextRun* aTextRun, uint32_t aOffset)
{
  if (aOffset + 1 >= aTextRun->GetLength())
    return true;
  return aTextRun->IsClusterStart(aOffset + 1) &&
    aTextRun->IsLigatureGroupStart(aOffset + 1);
}

static gfxFloat
ComputeTabWidthAppUnits(const nsIFrame* aFrame, gfxTextRun* aTextRun)
{
  const nsStyleText* textStyle = aFrame->StyleText();
  if (textStyle->mTabSize.GetUnit() != eStyleUnit_Factor) {
    nscoord w = textStyle->mTabSize.GetCoordValue();
    MOZ_ASSERT(w >= 0);
    return w;
  }

  gfxFloat spaces = textStyle->mTabSize.GetFactorValue();
  MOZ_ASSERT(spaces >= 0);

  // Round the space width when converting to appunits the same way
  // textruns do.
  gfxFloat spaceWidthAppUnits =
    NS_round(GetFirstFontMetrics(aTextRun->GetFontGroup(),
                                 aTextRun->IsVertical()).spaceWidth *
             aTextRun->GetAppUnitsPerDevUnit());
  return spaces * spaceWidthAppUnits;
}

void
PropertyProvider::GetSpacingInternal(Range aRange, Spacing* aSpacing,
                                     bool aIgnoreTabs) const
{
  NS_PRECONDITION(IsInBounds(mStart, mLength, aRange), "Range out of bounds");

  uint32_t index;
  for (index = 0; index < aRange.Length(); ++index) {
    aSpacing[index].mBefore = 0.0;
    aSpacing[index].mAfter = 0.0;
  }

  if (mFrame->StyleContext()->IsTextCombined()) {
    return;
  }

  // Find our offset into the original+transformed string
  gfxSkipCharsIterator start(mStart);
  start.SetSkippedOffset(aRange.start);

  // First, compute the word and letter spacing
  if (mWordSpacing || mLetterSpacing) {
    // Iterate over non-skipped characters
    nsSkipCharsRunIterator run(
        start, nsSkipCharsRunIterator::LENGTH_UNSKIPPED_ONLY, aRange.Length());
    while (run.NextRun()) {
      uint32_t runOffsetInSubstring = run.GetSkippedOffset() - aRange.start;
      gfxSkipCharsIterator iter = run.GetPos();
      for (int32_t i = 0; i < run.GetRunLength(); ++i) {
        if (CanAddSpacingAfter(mTextRun, run.GetSkippedOffset() + i)) {
          // End of a cluster, not in a ligature: put letter-spacing after it
          aSpacing[runOffsetInSubstring + i].mAfter += mLetterSpacing;
        }
        if (IsCSSWordSpacingSpace(mFrag, i + run.GetOriginalOffset(),
                                  mFrame, mTextStyle)) {
          // It kinda sucks, but space characters can be part of clusters,
          // and even still be whitespace (I think!)
          iter.SetSkippedOffset(run.GetSkippedOffset() + i);
          FindClusterEnd(mTextRun, run.GetOriginalOffset() + run.GetRunLength(),
                         &iter);
          uint32_t runOffset = iter.GetSkippedOffset() - aRange.start;
          aSpacing[runOffset].mAfter += mWordSpacing;
        }
      }
    }
  }

  // Now add tab spacing, if there is any
  if (!aIgnoreTabs) {
    gfxFloat tabWidth = ComputeTabWidthAppUnits(mFrame, mTextRun);
    if (tabWidth > 0) {
      CalcTabWidths(aRange, tabWidth);
      if (mTabWidths) {
        mTabWidths->ApplySpacing(aSpacing,
                                 aRange.start - mStart.GetSkippedOffset(),
                                 aRange.Length());
      }
    }
  }

  // Now add in justification spacing
  if (mJustificationSpacings.Length() > 0) {
    // If there is any spaces trimmed at the end, aStart + aLength may
    // be larger than the flags array. When that happens, we can simply
    // ignore those spaces.
    auto arrayEnd = mJustificationArrayStart +
      static_cast<uint32_t>(mJustificationSpacings.Length());
    auto end = std::min(aRange.end, arrayEnd);
    MOZ_ASSERT(aRange.start >= mJustificationArrayStart);
    for (auto i = aRange.start; i < end; i++) {
      const auto& spacing =
        mJustificationSpacings[i - mJustificationArrayStart];
      uint32_t offset = i - aRange.start;
      aSpacing[offset].mBefore += spacing.mBefore;
      aSpacing[offset].mAfter += spacing.mAfter;
    }
  }
}

// aX and the result are in whole appunits.
static gfxFloat
AdvanceToNextTab(gfxFloat aX, gfxFloat aTabWidth)
{

  // Advance aX to the next multiple of *aCachedTabWidth. We must advance
  // by at least 1 appunit.
  // XXX should we make this 1 CSS pixel?
  return ceil((aX + 1) / aTabWidth) * aTabWidth;
}

void
PropertyProvider::CalcTabWidths(Range aRange, gfxFloat aTabWidth) const
{
  MOZ_ASSERT(aTabWidth > 0);

  if (!mTabWidths) {
    if (mReflowing && !mLineContainer) {
      // Intrinsic width computation does its own tab processing. We
      // just don't do anything here.
      return;
    }
    if (!mReflowing) {
      mTabWidths = mFrame->GetProperty(TabWidthProperty());
#ifdef DEBUG
      // If we're not reflowing, we should have already computed the
      // tab widths; check that they're available as far as the last
      // tab character present (if any)
      for (uint32_t i = aRange.end; i > aRange.start; --i) {
        if (mTextRun->CharIsTab(i - 1)) {
          uint32_t startOffset = mStart.GetSkippedOffset();
          NS_ASSERTION(mTabWidths && mTabWidths->mLimit + startOffset >= i,
                       "Precomputed tab widths are missing!");
          break;
        }
      }
#endif
      return;
    }
  }

  uint32_t startOffset = mStart.GetSkippedOffset();
  MOZ_ASSERT(aRange.start >= startOffset, "wrong start offset");
  MOZ_ASSERT(aRange.end <= startOffset + mLength, "beyond the end");
  uint32_t tabsEnd =
    (mTabWidths ? mTabWidths->mLimit : mTabWidthsAnalyzedLimit) + startOffset;
  if (tabsEnd < aRange.end) {
    NS_ASSERTION(mReflowing,
                 "We need precomputed tab widths, but don't have enough.");

    for (uint32_t i = tabsEnd; i < aRange.end; ++i) {
      Spacing spacing;
      GetSpacingInternal(Range(i, i + 1), &spacing, true);
      mOffsetFromBlockOriginForTabs += spacing.mBefore;

      if (!mTextRun->CharIsTab(i)) {
        if (mTextRun->IsClusterStart(i)) {
          uint32_t clusterEnd = i + 1;
          while (clusterEnd < mTextRun->GetLength() &&
                 !mTextRun->IsClusterStart(clusterEnd)) {
            ++clusterEnd;
          }
          mOffsetFromBlockOriginForTabs +=
            mTextRun->GetAdvanceWidth(Range(i, clusterEnd), nullptr);
        }
      } else {
        if (!mTabWidths) {
          mTabWidths = new TabWidthStore(mFrame->GetContentOffset());
          mFrame->SetProperty(TabWidthProperty(), mTabWidths);
        }
        double nextTab = AdvanceToNextTab(mOffsetFromBlockOriginForTabs,
                                          aTabWidth);
        mTabWidths->mWidths.AppendElement(TabWidth(i - startOffset,
                NSToIntRound(nextTab - mOffsetFromBlockOriginForTabs)));
        mOffsetFromBlockOriginForTabs = nextTab;
      }

      mOffsetFromBlockOriginForTabs += spacing.mAfter;
    }

    if (mTabWidths) {
      mTabWidths->mLimit = aRange.end - startOffset;
    }
  }

  if (!mTabWidths) {
    // Delete any stale property that may be left on the frame
    mFrame->DeleteProperty(TabWidthProperty());
    mTabWidthsAnalyzedLimit = std::max(mTabWidthsAnalyzedLimit,
                                       aRange.end - startOffset);
  }
}

gfxFloat
PropertyProvider::GetHyphenWidth() const
{
  if (mHyphenWidth < 0) {
    mHyphenWidth = GetFontGroup()->GetHyphenWidth(this);
  }
  return mHyphenWidth + mLetterSpacing;
}

static inline bool
IS_HYPHEN(char16_t u)
{
  return (u == char16_t('-') ||
          u == 0x058A || // ARMENIAN HYPHEN
          u == 0x2010 || // HYPHEN
          u == 0x2012 || // FIGURE DASH
          u == 0x2013);  // EN DASH
}

void
PropertyProvider::GetHyphenationBreaks(Range aRange, HyphenType* aBreakBefore) const
{
  NS_PRECONDITION(IsInBounds(mStart, mLength, aRange), "Range out of bounds");
  NS_PRECONDITION(mLength != INT32_MAX, "Can't call this with undefined length");

  if (!mTextStyle->WhiteSpaceCanWrap(mFrame) ||
      mTextStyle->mHyphens == StyleHyphens::None)
  {
    memset(aBreakBefore, static_cast<uint8_t>(HyphenType::None),
           aRange.Length() * sizeof(HyphenType));
    return;
  }

  // Iterate through the original-string character runs
  nsSkipCharsRunIterator run(
      mStart, nsSkipCharsRunIterator::LENGTH_UNSKIPPED_ONLY, aRange.Length());
  run.SetSkippedOffset(aRange.start);
  // We need to visit skipped characters so that we can detect SHY
  run.SetVisitSkipped();

  int32_t prevTrailingCharOffset = run.GetPos().GetOriginalOffset() - 1;
  bool allowHyphenBreakBeforeNextChar =
    prevTrailingCharOffset >= mStart.GetOriginalOffset() &&
    prevTrailingCharOffset < mStart.GetOriginalOffset() + mLength &&
    mFrag->CharAt(prevTrailingCharOffset) == CH_SHY;

  while (run.NextRun()) {
    NS_ASSERTION(run.GetRunLength() > 0, "Shouldn't return zero-length runs");
    if (run.IsSkipped()) {
      // Check if there's a soft hyphen which would let us hyphenate before
      // the next non-skipped character. Don't look at soft hyphens followed
      // by other skipped characters, we won't use them.
      allowHyphenBreakBeforeNextChar =
        mFrag->CharAt(run.GetOriginalOffset() + run.GetRunLength() - 1) == CH_SHY;
    } else {
      int32_t runOffsetInSubstring = run.GetSkippedOffset() - aRange.start;
      memset(aBreakBefore + runOffsetInSubstring,
             static_cast<uint8_t>(HyphenType::None),
             run.GetRunLength() * sizeof(HyphenType));
      // Don't allow hyphen breaks at the start of the line
      aBreakBefore[runOffsetInSubstring] =
          allowHyphenBreakBeforeNextChar &&
          (!(mFrame->GetStateBits() & TEXT_START_OF_LINE) ||
           run.GetSkippedOffset() > mStart.GetSkippedOffset())
          ? HyphenType::Soft
          : HyphenType::None;
      allowHyphenBreakBeforeNextChar = false;
    }
  }

  if (mTextStyle->mHyphens == StyleHyphens::Auto) {
    uint32_t currentFragOffset = mStart.GetOriginalOffset();
    for (uint32_t i = 0; i < aRange.Length(); ++i) {
      if (IS_HYPHEN(mFrag->CharAt(currentFragOffset + i))) {
        aBreakBefore[i] = HyphenType::Explicit;
        continue;
      }

      if (mTextRun->CanHyphenateBefore(aRange.start + i) &&
          aBreakBefore[i] == HyphenType::None) {
        aBreakBefore[i] = HyphenType::AutoWithoutManualInSameWord;
      }
    }
  }
}

void
PropertyProvider::InitializeForDisplay(bool aTrimAfter)
{
  nsTextFrame::TrimmedOffsets trimmed =
    mFrame->GetTrimmedOffsets(mFrag, aTrimAfter);
  mStart.SetOriginalOffset(trimmed.mStart);
  mLength = trimmed.mLength;
  SetupJustificationSpacing(true);
}

void
PropertyProvider::InitializeForMeasure()
{
  nsTextFrame::TrimmedOffsets trimmed =
    mFrame->GetTrimmedOffsets(mFrag, true, false);
  mStart.SetOriginalOffset(trimmed.mStart);
  mLength = trimmed.mLength;
  SetupJustificationSpacing(false);
}


void
PropertyProvider::SetupJustificationSpacing(bool aPostReflow)
{
  NS_PRECONDITION(mLength != INT32_MAX, "Can't call this with undefined length");

  if (!(mFrame->GetStateBits() & TEXT_JUSTIFICATION_ENABLED)) {
    return;
  }

  gfxSkipCharsIterator start(mStart), end(mStart);
  // We can't just use our mLength here; when InitializeForDisplay is
  // called with false for aTrimAfter, we still shouldn't be assigning
  // justification space to any trailing whitespace.
  nsTextFrame::TrimmedOffsets trimmed =
    mFrame->GetTrimmedOffsets(mFrag, true, aPostReflow);
  end.AdvanceOriginal(trimmed.mLength);
  gfxSkipCharsIterator realEnd(end);

  Range range(uint32_t(start.GetOriginalOffset()),
              uint32_t(end.GetOriginalOffset()));
  nsTArray<JustificationAssignment> assignments;
  JustificationInfo info = ComputeJustification(range, &assignments);

  auto assign = mFrame->GetJustificationAssignment();
  auto totalGaps = JustificationUtils::CountGaps(info, assign);
  if (!totalGaps || assignments.IsEmpty()) {
    // Nothing to do, nothing is justifiable and we shouldn't have any
    // justification space assigned
    return;
  }

  // Remember that textrun measurements are in the run's orientation,
  // so its advance "width" is actually a height in vertical writing modes,
  // corresponding to the inline-direction of the frame.
  gfxFloat naturalWidth =
    mTextRun->GetAdvanceWidth(Range(mStart.GetSkippedOffset(),
                                    realEnd.GetSkippedOffset()), this);
  if (mFrame->GetStateBits() & TEXT_HYPHEN_BREAK) {
    naturalWidth += GetHyphenWidth();
  }
  nscoord totalSpacing = mFrame->ISize() - naturalWidth;
  if (totalSpacing <= 0) {
    // No space available
    return;
  }

  assignments[0].mGapsAtStart = assign.mGapsAtStart;
  assignments.LastElement().mGapsAtEnd = assign.mGapsAtEnd;

  MOZ_ASSERT(mJustificationSpacings.IsEmpty());
  JustificationApplicationState state(totalGaps, totalSpacing);
  mJustificationSpacings.SetCapacity(assignments.Length());
  for (const JustificationAssignment& assign : assignments) {
    Spacing* spacing = mJustificationSpacings.AppendElement();
    spacing->mBefore = state.Consume(assign.mGapsAtStart);
    spacing->mAfter = state.Consume(assign.mGapsAtEnd);
  }
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

nsTextPaintStyle::nsTextPaintStyle(nsTextFrame* aFrame)
  : mFrame(aFrame),
    mPresContext(aFrame->PresContext()),
    mInitCommonColors(false),
    mInitSelectionColorsAndShadow(false),
    mResolveColors(true),
    mHasSelectionShadow(false)
{
  for (uint32_t i = 0; i < ArrayLength(mSelectionStyle); i++)
    mSelectionStyle[i].mInit = false;
}

bool
nsTextPaintStyle::EnsureSufficientContrast(nscolor *aForeColor, nscolor *aBackColor)
{
  InitCommonColors();

  // If the combination of selection background color and frame background color
  // is sufficient contrast, don't exchange the selection colors.
  int32_t backLuminosityDifference =
            NS_LUMINOSITY_DIFFERENCE(*aBackColor, mFrameBackgroundColor);
  if (backLuminosityDifference >= mSufficientContrast)
    return false;

  // Otherwise, we should use the higher-contrast color for the selection
  // background color.
  int32_t foreLuminosityDifference =
            NS_LUMINOSITY_DIFFERENCE(*aForeColor, mFrameBackgroundColor);
  if (backLuminosityDifference < foreLuminosityDifference) {
    nscolor tmpColor = *aForeColor;
    *aForeColor = *aBackColor;
    *aBackColor = tmpColor;
    return true;
  }
  return false;
}

nscolor
nsTextPaintStyle::GetTextColor()
{
  if (nsSVGUtils::IsInSVGTextSubtree(mFrame)) {
    if (!mResolveColors)
      return NS_SAME_AS_FOREGROUND_COLOR;

    const nsStyleSVG* style = mFrame->StyleSVG();
    switch (style->mFill.Type()) {
      case eStyleSVGPaintType_None:
        return NS_RGBA(0, 0, 0, 0);
      case eStyleSVGPaintType_Color:
        return nsLayoutUtils::GetColor(mFrame, &nsStyleSVG::mFill);
      default:
        NS_ERROR("cannot resolve SVG paint to nscolor");
        return NS_RGBA(0, 0, 0, 255);
    }
  }

  return nsLayoutUtils::GetColor(mFrame, &nsStyleText::mWebkitTextFillColor);
}

bool
nsTextPaintStyle::GetSelectionColors(nscolor* aForeColor,
                                     nscolor* aBackColor)
{
  NS_ASSERTION(aForeColor, "aForeColor is null");
  NS_ASSERTION(aBackColor, "aBackColor is null");

  if (!InitSelectionColorsAndShadow())
    return false;

  *aForeColor = mSelectionTextColor;
  *aBackColor = mSelectionBGColor;
  return true;
}

void
nsTextPaintStyle::GetHighlightColors(nscolor* aForeColor,
                                     nscolor* aBackColor)
{
  NS_ASSERTION(aForeColor, "aForeColor is null");
  NS_ASSERTION(aBackColor, "aBackColor is null");

  const nsFrameSelection* frameSelection = mFrame->GetConstFrameSelection();
  const Selection* selection =
    frameSelection->GetSelection(SelectionType::eFind);
  const SelectionCustomColors* customColors = nullptr;
  if (selection) {
    customColors = selection->GetCustomColors();
  }

  if (!customColors) {
    nscolor backColor =
      LookAndFeel::GetColor(LookAndFeel::eColorID_TextHighlightBackground);
    nscolor foreColor =
      LookAndFeel::GetColor(LookAndFeel::eColorID_TextHighlightForeground);
    EnsureSufficientContrast(&foreColor, &backColor);
    *aForeColor = foreColor;
    *aBackColor = backColor;

    return;
  }

  if (customColors->mForegroundColor && customColors->mBackgroundColor) {
    nscolor foreColor = *customColors->mForegroundColor;
    nscolor backColor = *customColors->mBackgroundColor;

    if (EnsureSufficientContrast(&foreColor, &backColor) &&
        customColors->mAltForegroundColor &&
        customColors->mAltBackgroundColor) {
      foreColor = *customColors->mAltForegroundColor;
      backColor = *customColors->mAltBackgroundColor;
    }

    *aForeColor = foreColor;
    *aBackColor = backColor;
    return;
  }

  InitCommonColors();

  if (customColors->mBackgroundColor) {
    // !mForegroundColor means "currentColor"; the current color of the text.
    nscolor foreColor = GetTextColor();
    nscolor backColor = *customColors->mBackgroundColor;

    int32_t luminosityDifference =
              NS_LUMINOSITY_DIFFERENCE(foreColor, backColor);

    if (mSufficientContrast > luminosityDifference &&
        customColors->mAltBackgroundColor) {
      int32_t altLuminosityDifference =
                NS_LUMINOSITY_DIFFERENCE(foreColor, *customColors->mAltBackgroundColor);

      if (luminosityDifference < altLuminosityDifference) {
        backColor = *customColors->mAltBackgroundColor;
      }
    }

    *aForeColor = foreColor;
    *aBackColor = backColor;
    return;
  }

  if (customColors->mForegroundColor) {
    nscolor foreColor = *customColors->mForegroundColor;
    // !mBackgroundColor means "transparent"; the current color of the background.

    int32_t luminosityDifference =
              NS_LUMINOSITY_DIFFERENCE(foreColor, mFrameBackgroundColor);

    if (mSufficientContrast > luminosityDifference &&
        customColors->mAltForegroundColor) {
      int32_t altLuminosityDifference =
                NS_LUMINOSITY_DIFFERENCE(*customColors->mForegroundColor, mFrameBackgroundColor);

      if (luminosityDifference < altLuminosityDifference) {
        foreColor = *customColors->mAltForegroundColor;
      }
    }

    *aForeColor = foreColor;
    *aBackColor = NS_TRANSPARENT;
    return;
  }

  // There are neither mForegroundColor nor mBackgroundColor.
  *aForeColor = GetTextColor();
  *aBackColor = NS_TRANSPARENT;
}

void
nsTextPaintStyle::GetURLSecondaryColor(nscolor* aForeColor)
{
  NS_ASSERTION(aForeColor, "aForeColor is null");

  nscolor textColor = GetTextColor();
  textColor = NS_RGBA(NS_GET_R(textColor),
                      NS_GET_G(textColor),
                      NS_GET_B(textColor),
                      (uint8_t)(255 * 0.5f));
  // Don't use true alpha color for readability.
  InitCommonColors();
  *aForeColor = NS_ComposeColors(mFrameBackgroundColor, textColor);
}

void
nsTextPaintStyle::GetIMESelectionColors(int32_t  aIndex,
                                        nscolor* aForeColor,
                                        nscolor* aBackColor)
{
  NS_ASSERTION(aForeColor, "aForeColor is null");
  NS_ASSERTION(aBackColor, "aBackColor is null");
  NS_ASSERTION(aIndex >= 0 && aIndex < 5, "Index out of range");

  nsSelectionStyle* selectionStyle = GetSelectionStyle(aIndex);
  *aForeColor = selectionStyle->mTextColor;
  *aBackColor = selectionStyle->mBGColor;
}

bool
nsTextPaintStyle::GetSelectionUnderlineForPaint(int32_t  aIndex,
                                                nscolor* aLineColor,
                                                float*   aRelativeSize,
                                                uint8_t* aStyle)
{
  NS_ASSERTION(aLineColor, "aLineColor is null");
  NS_ASSERTION(aRelativeSize, "aRelativeSize is null");
  NS_ASSERTION(aIndex >= 0 && aIndex < 5, "Index out of range");

  nsSelectionStyle* selectionStyle = GetSelectionStyle(aIndex);
  if (selectionStyle->mUnderlineStyle == NS_STYLE_BORDER_STYLE_NONE ||
      selectionStyle->mUnderlineColor == NS_TRANSPARENT ||
      selectionStyle->mUnderlineRelativeSize <= 0.0f)
    return false;

  *aLineColor = selectionStyle->mUnderlineColor;
  *aRelativeSize = selectionStyle->mUnderlineRelativeSize;
  *aStyle = selectionStyle->mUnderlineStyle;
  return true;
}

void
nsTextPaintStyle::InitCommonColors()
{
  if (mInitCommonColors)
    return;

  nsIFrame* bgFrame =
    nsCSSRendering::FindNonTransparentBackgroundFrame(mFrame);
  NS_ASSERTION(bgFrame, "Cannot find NonTransparentBackgroundFrame.");
  nscolor bgColor = bgFrame->
    GetVisitedDependentColor(&nsStyleBackground::mBackgroundColor);

  nscolor defaultBgColor = mPresContext->DefaultBackgroundColor();
  mFrameBackgroundColor = NS_ComposeColors(defaultBgColor, bgColor);

  mSystemFieldForegroundColor =
    LookAndFeel::GetColor(LookAndFeel::eColorID__moz_fieldtext);
  mSystemFieldBackgroundColor =
    LookAndFeel::GetColor(LookAndFeel::eColorID__moz_field);

  if (bgFrame->IsThemed()) {
    // Assume a native widget has sufficient contrast always
    mSufficientContrast = 0;
    mInitCommonColors = true;
    return;
  }

  NS_ASSERTION(NS_GET_A(defaultBgColor) == 255,
               "default background color is not opaque");

  nscolor defaultWindowBackgroundColor =
    LookAndFeel::GetColor(LookAndFeel::eColorID_WindowBackground);
  nscolor selectionTextColor =
    LookAndFeel::GetColor(LookAndFeel::eColorID_TextSelectForeground);
  nscolor selectionBGColor =
    LookAndFeel::GetColor(LookAndFeel::eColorID_TextSelectBackground);

  mSufficientContrast =
    std::min(std::min(NS_SUFFICIENT_LUMINOSITY_DIFFERENCE,
                  NS_LUMINOSITY_DIFFERENCE(selectionTextColor,
                                           selectionBGColor)),
                  NS_LUMINOSITY_DIFFERENCE(defaultWindowBackgroundColor,
                                           selectionBGColor));

  mInitCommonColors = true;
}

nscolor
nsTextPaintStyle::GetSystemFieldForegroundColor()
{
  InitCommonColors();
  return mSystemFieldForegroundColor;
}

nscolor
nsTextPaintStyle::GetSystemFieldBackgroundColor()
{
  InitCommonColors();
  return mSystemFieldBackgroundColor;
}

static Element*
FindElementAncestorForMozSelection(nsIContent* aContent)
{
  NS_ENSURE_TRUE(aContent, nullptr);
  while (aContent && aContent->IsInNativeAnonymousSubtree()) {
    aContent = aContent->GetBindingParent();
  }
  NS_ASSERTION(aContent, "aContent isn't in non-anonymous tree?");
  while (aContent && !aContent->IsElement()) {
    aContent = aContent->GetParent();
  }
  return aContent ? aContent->AsElement() : nullptr;
}

bool
nsTextPaintStyle::InitSelectionColorsAndShadow()
{
  if (mInitSelectionColorsAndShadow)
    return true;

  int16_t selectionFlags;
  int16_t selectionStatus = mFrame->GetSelectionStatus(&selectionFlags);
  if (!(selectionFlags & nsISelectionDisplay::DISPLAY_TEXT) ||
      selectionStatus < nsISelectionController::SELECTION_ON) {
    // Not displaying the normal selection.
    // We're not caching this fact, so every call to GetSelectionColors
    // will come through here. We could avoid this, but it's not really worth it.
    return false;
  }

  mInitSelectionColorsAndShadow = true;

  nsIFrame* nonGeneratedAncestor = nsLayoutUtils::GetNonGeneratedAncestor(mFrame);
  Element* selectionElement =
    FindElementAncestorForMozSelection(nonGeneratedAncestor->GetContent());

  if (selectionElement &&
      selectionStatus == nsISelectionController::SELECTION_ON) {
    RefPtr<nsStyleContext> sc =
      mPresContext->StyleSet()->
        ProbePseudoElementStyle(selectionElement,
                                CSSPseudoElementType::mozSelection,
                                mFrame->StyleContext());
    // Use -moz-selection pseudo class.
    if (sc) {
      mSelectionBGColor =
        sc->GetVisitedDependentColor(&nsStyleBackground::mBackgroundColor);
      mSelectionTextColor =
        sc->GetVisitedDependentColor(&nsStyleText::mWebkitTextFillColor);
      mHasSelectionShadow = true;
      mSelectionShadow = sc->StyleText()->mTextShadow;
      return true;
    }
  }

  nscolor selectionBGColor =
    LookAndFeel::GetColor(LookAndFeel::eColorID_TextSelectBackground);

  if (selectionStatus == nsISelectionController::SELECTION_ATTENTION) {
    mSelectionBGColor =
      LookAndFeel::GetColor(
        LookAndFeel::eColorID_TextSelectBackgroundAttention);
    mSelectionBGColor  = EnsureDifferentColors(mSelectionBGColor,
                                               selectionBGColor);
  } else if (selectionStatus != nsISelectionController::SELECTION_ON) {
    mSelectionBGColor =
      LookAndFeel::GetColor(LookAndFeel::eColorID_TextSelectBackgroundDisabled);
    mSelectionBGColor  = EnsureDifferentColors(mSelectionBGColor,
                                               selectionBGColor);
  } else {
    mSelectionBGColor = selectionBGColor;
  }

  mSelectionTextColor =
    LookAndFeel::GetColor(LookAndFeel::eColorID_TextSelectForeground);

  if (mResolveColors) {
    // On MacOS X, we don't exchange text color and BG color.
    if (mSelectionTextColor == NS_DONT_CHANGE_COLOR) {
      nscolor frameColor = nsSVGUtils::IsInSVGTextSubtree(mFrame)
        ? mFrame->GetVisitedDependentColor(&nsStyleSVG::mFill)
        : mFrame->GetVisitedDependentColor(&nsStyleText::mWebkitTextFillColor);
      mSelectionTextColor = EnsureDifferentColors(frameColor, mSelectionBGColor);
    } else if (mSelectionTextColor == NS_CHANGE_COLOR_IF_SAME_AS_BG) {
      nscolor frameColor = nsSVGUtils::IsInSVGTextSubtree(mFrame)
        ? mFrame->GetVisitedDependentColor(&nsStyleSVG::mFill)
        : mFrame->GetVisitedDependentColor(&nsStyleText::mWebkitTextFillColor);
      if (frameColor == mSelectionBGColor) {
        mSelectionTextColor =
          LookAndFeel::GetColor(LookAndFeel::eColorID_TextSelectForegroundCustom);
      }
    } else {
      EnsureSufficientContrast(&mSelectionTextColor, &mSelectionBGColor);
    }
  } else {
    if (mSelectionTextColor == NS_DONT_CHANGE_COLOR) {
      mSelectionTextColor = NS_SAME_AS_FOREGROUND_COLOR;
    }
  }
  return true;
}

nsTextPaintStyle::nsSelectionStyle*
nsTextPaintStyle::GetSelectionStyle(int32_t aIndex)
{
  InitSelectionStyle(aIndex);
  return &mSelectionStyle[aIndex];
}

struct StyleIDs {
  LookAndFeel::ColorID mForeground, mBackground, mLine;
  LookAndFeel::IntID mLineStyle;
  LookAndFeel::FloatID mLineRelativeSize;
};
static StyleIDs SelectionStyleIDs[] = {
  { LookAndFeel::eColorID_IMERawInputForeground,
    LookAndFeel::eColorID_IMERawInputBackground,
    LookAndFeel::eColorID_IMERawInputUnderline,
    LookAndFeel::eIntID_IMERawInputUnderlineStyle,
    LookAndFeel::eFloatID_IMEUnderlineRelativeSize },
  { LookAndFeel::eColorID_IMESelectedRawTextForeground,
    LookAndFeel::eColorID_IMESelectedRawTextBackground,
    LookAndFeel::eColorID_IMESelectedRawTextUnderline,
    LookAndFeel::eIntID_IMESelectedRawTextUnderlineStyle,
    LookAndFeel::eFloatID_IMEUnderlineRelativeSize },
  { LookAndFeel::eColorID_IMEConvertedTextForeground,
    LookAndFeel::eColorID_IMEConvertedTextBackground,
    LookAndFeel::eColorID_IMEConvertedTextUnderline,
    LookAndFeel::eIntID_IMEConvertedTextUnderlineStyle,
    LookAndFeel::eFloatID_IMEUnderlineRelativeSize },
  { LookAndFeel::eColorID_IMESelectedConvertedTextForeground,
    LookAndFeel::eColorID_IMESelectedConvertedTextBackground,
    LookAndFeel::eColorID_IMESelectedConvertedTextUnderline,
    LookAndFeel::eIntID_IMESelectedConvertedTextUnderline,
    LookAndFeel::eFloatID_IMEUnderlineRelativeSize },
  { LookAndFeel::eColorID_LAST_COLOR,
    LookAndFeel::eColorID_LAST_COLOR,
    LookAndFeel::eColorID_SpellCheckerUnderline,
    LookAndFeel::eIntID_SpellCheckerUnderlineStyle,
    LookAndFeel::eFloatID_SpellCheckerUnderlineRelativeSize }
};

void
nsTextPaintStyle::InitSelectionStyle(int32_t aIndex)
{
  NS_ASSERTION(aIndex >= 0 && aIndex < 5, "aIndex is invalid");
  nsSelectionStyle* selectionStyle = &mSelectionStyle[aIndex];
  if (selectionStyle->mInit)
    return;

  StyleIDs* styleIDs = &SelectionStyleIDs[aIndex];

  nscolor foreColor, backColor;
  if (styleIDs->mForeground == LookAndFeel::eColorID_LAST_COLOR) {
    foreColor = NS_SAME_AS_FOREGROUND_COLOR;
  } else {
    foreColor = LookAndFeel::GetColor(styleIDs->mForeground);
  }
  if (styleIDs->mBackground == LookAndFeel::eColorID_LAST_COLOR) {
    backColor = NS_TRANSPARENT;
  } else {
    backColor = LookAndFeel::GetColor(styleIDs->mBackground);
  }

  // Convert special color to actual color
  NS_ASSERTION(foreColor != NS_TRANSPARENT,
               "foreColor cannot be NS_TRANSPARENT");
  NS_ASSERTION(backColor != NS_SAME_AS_FOREGROUND_COLOR,
               "backColor cannot be NS_SAME_AS_FOREGROUND_COLOR");
  NS_ASSERTION(backColor != NS_40PERCENT_FOREGROUND_COLOR,
               "backColor cannot be NS_40PERCENT_FOREGROUND_COLOR");

  if (mResolveColors) {
    foreColor = GetResolvedForeColor(foreColor, GetTextColor(), backColor);

    if (NS_GET_A(backColor) > 0)
      EnsureSufficientContrast(&foreColor, &backColor);
  }

  nscolor lineColor;
  float relativeSize;
  uint8_t lineStyle;
  GetSelectionUnderline(mPresContext, aIndex,
                        &lineColor, &relativeSize, &lineStyle);

  if (mResolveColors)
    lineColor = GetResolvedForeColor(lineColor, foreColor, backColor);

  selectionStyle->mTextColor       = foreColor;
  selectionStyle->mBGColor         = backColor;
  selectionStyle->mUnderlineColor  = lineColor;
  selectionStyle->mUnderlineStyle  = lineStyle;
  selectionStyle->mUnderlineRelativeSize = relativeSize;
  selectionStyle->mInit            = true;
}

/* static */ bool
nsTextPaintStyle::GetSelectionUnderline(nsPresContext* aPresContext,
                                        int32_t aIndex,
                                        nscolor* aLineColor,
                                        float* aRelativeSize,
                                        uint8_t* aStyle)
{
  NS_ASSERTION(aPresContext, "aPresContext is null");
  NS_ASSERTION(aRelativeSize, "aRelativeSize is null");
  NS_ASSERTION(aStyle, "aStyle is null");
  NS_ASSERTION(aIndex >= 0 && aIndex < 5, "Index out of range");

  StyleIDs& styleID = SelectionStyleIDs[aIndex];

  nscolor color = LookAndFeel::GetColor(styleID.mLine);
  int32_t style = LookAndFeel::GetInt(styleID.mLineStyle);
  if (style > NS_STYLE_TEXT_DECORATION_STYLE_MAX) {
    NS_ERROR("Invalid underline style value is specified");
    style = NS_STYLE_TEXT_DECORATION_STYLE_SOLID;
  }
  float size = LookAndFeel::GetFloat(styleID.mLineRelativeSize);

  NS_ASSERTION(size, "selection underline relative size must be larger than 0");

  if (aLineColor) {
    *aLineColor = color;
  }
  *aRelativeSize = size;
  *aStyle = style;

  return style != NS_STYLE_TEXT_DECORATION_STYLE_NONE &&
         color != NS_TRANSPARENT &&
         size > 0.0f;
}

bool
nsTextPaintStyle::GetSelectionShadow(nsCSSShadowArray** aShadow)
{
  if (!InitSelectionColorsAndShadow()) {
    return false;
  }

  if (mHasSelectionShadow) {
    *aShadow = mSelectionShadow;
    return true;
  }

  return false;
}

inline nscolor Get40PercentColor(nscolor aForeColor, nscolor aBackColor)
{
  nscolor foreColor = NS_RGBA(NS_GET_R(aForeColor),
                              NS_GET_G(aForeColor),
                              NS_GET_B(aForeColor),
                              (uint8_t)(255 * 0.4f));
  // Don't use true alpha color for readability.
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
a11y::AccType
nsTextFrame::AccessibleType()
{
  if (IsEmpty()) {
    RenderedText text = GetRenderedText(0,
        UINT32_MAX, TextOffsetType::OFFSETS_IN_CONTENT_TEXT,
        TrailingWhitespace::DONT_TRIM_TRAILING_WHITESPACE);
    if (text.mString.IsEmpty()) {
      return a11y::eNoType;
    }
  }

  return a11y::eTextLeafType;
}
#endif


//-----------------------------------------------------------------------------
void
nsTextFrame::Init(nsIContent*       aContent,
                  nsContainerFrame* aParent,
                  nsIFrame*         aPrevInFlow)
{
  NS_ASSERTION(!aPrevInFlow, "Can't be a continuation!");
  NS_PRECONDITION(aContent->IsNodeOfType(nsINode::eTEXT),
                  "Bogus content!");

  // Remove any NewlineOffsetProperty or InFlowContentLengthProperty since they
  // might be invalid if the content was modified while there was no frame
  if (aContent->HasFlag(NS_HAS_NEWLINE_PROPERTY)) {
    aContent->DeleteProperty(nsGkAtoms::newline);
    aContent->UnsetFlags(NS_HAS_NEWLINE_PROPERTY);
  }
  if (aContent->HasFlag(NS_HAS_FLOWLENGTH_PROPERTY)) {
    aContent->DeleteProperty(nsGkAtoms::flowlength);
    aContent->UnsetFlags(NS_HAS_FLOWLENGTH_PROPERTY);
  }

  // Since our content has a frame now, this flag is no longer needed.
  aContent->UnsetFlags(NS_CREATE_FRAME_IF_NON_WHITESPACE);

  // We're not a continuing frame.
  // mContentOffset = 0; not necessary since we get zeroed out at init
  nsFrame::Init(aContent, aParent, aPrevInFlow);
}

void
nsTextFrame::ClearFrameOffsetCache()
{
  // See if we need to remove ourselves from the offset cache
  if (GetStateBits() & TEXT_IN_OFFSET_CACHE) {
    nsIFrame* primaryFrame = mContent->GetPrimaryFrame();
    if (primaryFrame) {
      // The primary frame might be null here.  For example, nsLineBox::DeleteLineList
      // just destroys the frames in order, which means that the primary frame is already
      // dead if we're a continuing text frame, in which case, all of its properties are
      // gone, and we don't need to worry about deleting this property here.
      primaryFrame->DeleteProperty(OffsetToFrameProperty());
    }
    RemoveStateBits(TEXT_IN_OFFSET_CACHE);
  }
}

void
nsTextFrame::DestroyFrom(nsIFrame* aDestructRoot, PostDestroyData& aPostDestroyData)
{
  ClearFrameOffsetCache();

  // We might want to clear NS_CREATE_FRAME_IF_NON_WHITESPACE or
  // NS_REFRAME_IF_WHITESPACE on mContent here, since our parent frame
  // type might be changing.  Not clear whether it's worth it.
  ClearTextRuns();
  if (mNextContinuation) {
    mNextContinuation->SetPrevInFlow(nullptr);
  }
  // Let the base class destroy the frame
  nsFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

class nsContinuingTextFrame final : public nsTextFrame
{
public:
  NS_DECL_FRAMEARENA_HELPERS(nsContinuingTextFrame)

  friend nsIFrame* NS_NewContinuingTextFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  void Init(nsIContent* aContent,
            nsContainerFrame* aParent,
            nsIFrame* aPrevInFlow) override;

  void DestroyFrom(nsIFrame* aDestructRoot, PostDestroyData& aPostDestroyData) override;

  nsTextFrame* GetPrevContinuation() const override
  {
    return mPrevContinuation;
  }
  void SetPrevContinuation(nsIFrame* aPrevContinuation) override
  {
    NS_ASSERTION(!aPrevContinuation || Type() == aPrevContinuation->Type(),
                 "setting a prev continuation with incorrect type!");
    NS_ASSERTION (!nsSplittableFrame::IsInPrevContinuationChain(aPrevContinuation, this),
                  "creating a loop in continuation chain!");
    mPrevContinuation = static_cast<nsTextFrame*>(aPrevContinuation);
    RemoveStateBits(NS_FRAME_IS_FLUID_CONTINUATION);
  }
  nsIFrame* GetPrevInFlowVirtual() const override { return GetPrevInFlow(); }
  nsTextFrame* GetPrevInFlow() const
  {
    return (GetStateBits() & NS_FRAME_IS_FLUID_CONTINUATION) ? mPrevContinuation : nullptr;
  }
  void SetPrevInFlow(nsIFrame* aPrevInFlow) override
  {
    NS_ASSERTION(!aPrevInFlow || Type() == aPrevInFlow->Type(),
                 "setting a prev in flow with incorrect type!");
    NS_ASSERTION (!nsSplittableFrame::IsInPrevContinuationChain(aPrevInFlow, this),
                  "creating a loop in continuation chain!");
    mPrevContinuation = static_cast<nsTextFrame*>(aPrevInFlow);
    AddStateBits(NS_FRAME_IS_FLUID_CONTINUATION);
  }
  nsIFrame* FirstInFlow() const override;
  nsIFrame* FirstContinuation() const override;

  void AddInlineMinISize(gfxContext* aRenderingContext,
                         InlineMinISizeData* aData) override;
  void AddInlinePrefISize(gfxContext* aRenderingContext,
                          InlinePrefISizeData* aData) override;

protected:
  explicit nsContinuingTextFrame(nsStyleContext* aContext)
    : nsTextFrame(aContext, kClassID)
  {}

  nsTextFrame* mPrevContinuation;
};

void
nsContinuingTextFrame::Init(nsIContent*       aContent,
                            nsContainerFrame* aParent,
                            nsIFrame*         aPrevInFlow)
{
  NS_ASSERTION(aPrevInFlow, "Must be a continuation!");
  // NOTE: bypassing nsTextFrame::Init!!!
  nsFrame::Init(aContent, aParent, aPrevInFlow);

  nsTextFrame* prev = static_cast<nsTextFrame*>(aPrevInFlow);
  nsTextFrame* nextContinuation = prev->GetNextContinuation();
  // Hook the frame into the flow
  SetPrevInFlow(aPrevInFlow);
  aPrevInFlow->SetNextInFlow(this);
  mContentOffset = prev->GetContentOffset() + prev->GetContentLengthHint();
  NS_ASSERTION(mContentOffset < int32_t(aContent->GetText()->GetLength()),
               "Creating ContinuingTextFrame, but there is no more content");
  if (prev->StyleContext() != StyleContext()) {
    // We're taking part of prev's text, and its style may be different
    // so clear its textrun which may no longer be valid (and don't set ours)
    prev->ClearTextRuns();
  } else {
    float inflation = prev->GetFontSizeInflation();
    SetFontSizeInflation(inflation);
    mTextRun = prev->GetTextRun(nsTextFrame::eInflated);
    if (inflation != 1.0f) {
      gfxTextRun *uninflatedTextRun =
        prev->GetTextRun(nsTextFrame::eNotInflated);
      if (uninflatedTextRun) {
        SetTextRun(uninflatedTextRun, nsTextFrame::eNotInflated, 1.0f);
      }
    }
  }
  if (aPrevInFlow->GetStateBits() & NS_FRAME_IS_BIDI) {
    FrameBidiData bidiData = aPrevInFlow->GetBidiData();
    bidiData.precedingControl = kBidiLevelNone;
    SetProperty(BidiDataProperty(), bidiData);

    if (nextContinuation) {
      SetNextContinuation(nextContinuation);
      nextContinuation->SetPrevContinuation(this);
      // Adjust next-continuations' content offset as needed.
      while (nextContinuation &&
             nextContinuation->GetContentOffset() < mContentOffset) {
#ifdef DEBUG
        FrameBidiData nextBidiData = nextContinuation->GetBidiData();
        NS_ASSERTION(bidiData.embeddingLevel == nextBidiData.embeddingLevel &&
                     bidiData.baseLevel == nextBidiData.baseLevel,
                     "stealing text from different type of BIDI continuation");
        MOZ_ASSERT(nextBidiData.precedingControl == kBidiLevelNone,
                   "There shouldn't be any virtual bidi formatting character "
                   "between continuations");
#endif
        nextContinuation->mContentOffset = mContentOffset;
        nextContinuation = nextContinuation->GetNextContinuation();
      }
    }
    AddStateBits(NS_FRAME_IS_BIDI);
  } // prev frame is bidi
}

void
nsContinuingTextFrame::DestroyFrom(nsIFrame* aDestructRoot, PostDestroyData& aPostDestroyData)
{
  ClearFrameOffsetCache();

  // The text associated with this frame will become associated with our
  // prev-continuation. If that means the text has changed style, then
  // we need to wipe out the text run for the text.
  // Note that mPrevContinuation can be null if we're destroying the whole
  // frame chain from the start to the end.
  // If this frame is mentioned in the userData for a textrun (say
  // because there's a direction change at the start of this frame), then
  // we have to clear the textrun because we're going away and the
  // textrun had better not keep a dangling reference to us.
  if (IsInTextRunUserData() ||
      (mPrevContinuation &&
       mPrevContinuation->StyleContext() != StyleContext())) {
    ClearTextRuns();
    // Clear the previous continuation's text run also, so that it can rebuild
    // the text run to include our text.
    if (mPrevContinuation) {
      mPrevContinuation->ClearTextRuns();
    }
  }
  nsSplittableFrame::RemoveFromFlow(this);
  // Let the base class destroy the frame
  nsFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

nsIFrame*
nsContinuingTextFrame::FirstInFlow() const
{
  // Can't cast to |nsContinuingTextFrame*| because the first one isn't.
  nsIFrame *firstInFlow,
           *previous = const_cast<nsIFrame*>
                                 (static_cast<const nsIFrame*>(this));
  do {
    firstInFlow = previous;
    previous = firstInFlow->GetPrevInFlow();
  } while (previous);
  MOZ_ASSERT(firstInFlow, "post-condition failed");
  return firstInFlow;
}

nsIFrame*
nsContinuingTextFrame::FirstContinuation() const
{
  // Can't cast to |nsContinuingTextFrame*| because the first one isn't.
  nsIFrame *firstContinuation,
  *previous = const_cast<nsIFrame*>
                        (static_cast<const nsIFrame*>(mPrevContinuation));

  NS_ASSERTION(previous, "How can an nsContinuingTextFrame be the first continuation?");

  do {
    firstContinuation = previous;
    previous = firstContinuation->GetPrevContinuation();
  } while (previous);
  MOZ_ASSERT(firstContinuation, "post-condition failed");
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
nsTextFrame::GetMinISize(gfxContext *aRenderingContext)
{
  return nsLayoutUtils::MinISizeFromInline(this, aRenderingContext);
}

// Needed for text frames in XUL.
/* virtual */ nscoord
nsTextFrame::GetPrefISize(gfxContext *aRenderingContext)
{
  return nsLayoutUtils::PrefISizeFromInline(this, aRenderingContext);
}

/* virtual */ void
nsContinuingTextFrame::AddInlineMinISize(gfxContext *aRenderingContext,
                                         InlineMinISizeData *aData)
{
  // Do nothing, since the first-in-flow accounts for everything.
}

/* virtual */ void
nsContinuingTextFrame::AddInlinePrefISize(gfxContext *aRenderingContext,
                                          InlinePrefISizeData *aData)
{
  // Do nothing, since the first-in-flow accounts for everything.
}

//----------------------------------------------------------------------

#if defined(DEBUG_rbs) || defined(DEBUG_bzbarsky)
static void
VerifyNotDirty(nsFrameState state)
{
  bool isZero = state & NS_FRAME_FIRST_REFLOW;
  bool isDirty = state & NS_FRAME_IS_DIRTY;
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

NS_IMPL_FRAMEARENA_HELPERS(nsTextFrame)

nsIFrame*
NS_NewContinuingTextFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsContinuingTextFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsContinuingTextFrame)

nsTextFrame::~nsTextFrame()
{
}

nsresult
nsTextFrame::GetCursor(const nsPoint& aPoint,
                       nsIFrame::Cursor& aCursor)
{
  FillCursorInformationFromStyle(StyleUserInterface(), aCursor);
  if (NS_STYLE_CURSOR_AUTO == aCursor.mCursor) {
    if (!IsSelectable(nullptr)) {
      aCursor.mCursor = NS_STYLE_CURSOR_DEFAULT;
    } else {
      aCursor.mCursor = GetWritingMode().IsVertical()
        ? NS_STYLE_CURSOR_VERTICAL_TEXT : NS_STYLE_CURSOR_TEXT;
    }
    return NS_OK;
  } else {
    return nsFrame::GetCursor(aPoint, aCursor);
  }
}

nsTextFrame*
nsTextFrame::LastInFlow() const
{
  nsTextFrame* lastInFlow = const_cast<nsTextFrame*>(this);
  while (lastInFlow->GetNextInFlow())  {
    lastInFlow = lastInFlow->GetNextInFlow();
  }
  MOZ_ASSERT(lastInFlow, "post-condition failed");
  return lastInFlow;
}

nsTextFrame*
nsTextFrame::LastContinuation() const
{
  nsTextFrame* lastContinuation = const_cast<nsTextFrame*>(this);
  while (lastContinuation->mNextContinuation)  {
    lastContinuation = lastContinuation->mNextContinuation;
  }
  MOZ_ASSERT(lastContinuation, "post-condition failed");
  return lastContinuation;
}

void
nsTextFrame::InvalidateFrame(uint32_t aDisplayItemKey)
{
  if (nsSVGUtils::IsInSVGTextSubtree(this)) {
    nsIFrame* svgTextFrame = nsLayoutUtils::GetClosestFrameOfType(
      GetParent(), LayoutFrameType::SVGText);
    svgTextFrame->InvalidateFrame();
    return;
  }
  nsFrame::InvalidateFrame(aDisplayItemKey);
}

void
nsTextFrame::InvalidateFrameWithRect(const nsRect& aRect, uint32_t aDisplayItemKey)
{
  if (nsSVGUtils::IsInSVGTextSubtree(this)) {
    nsIFrame* svgTextFrame = nsLayoutUtils::GetClosestFrameOfType(
      GetParent(), LayoutFrameType::SVGText);
    svgTextFrame->InvalidateFrame();
    return;
  }
  nsFrame::InvalidateFrameWithRect(aRect, aDisplayItemKey);
}

gfxTextRun*
nsTextFrame::GetUninflatedTextRun()
{
  return GetProperty(UninflatedTextRunProperty());
}

void
nsTextFrame::SetTextRun(gfxTextRun* aTextRun, TextRunType aWhichTextRun,
                        float aInflation)
{
  NS_ASSERTION(aTextRun, "must have text run");

  // Our inflated text run is always stored in mTextRun.  In the cases
  // where our current inflation is not 1.0, however, we store two text
  // runs, and the uninflated one goes in a frame property.  We never
  // store a single text run in both.
  if (aWhichTextRun == eInflated) {
    if (HasFontSizeInflation() && aInflation == 1.0f) {
      // FIXME: Probably shouldn't do this within each SetTextRun
      // method, but it doesn't hurt.
      ClearTextRun(nullptr, nsTextFrame::eNotInflated);
    }
    SetFontSizeInflation(aInflation);
  } else {
    MOZ_ASSERT(aInflation == 1.0f, "unexpected inflation");
    if (HasFontSizeInflation()) {
      // Setting the property will not automatically increment the textrun's
      // reference count, so we need to do it here.
      aTextRun->AddRef();
      SetProperty(UninflatedTextRunProperty(), aTextRun);
      return;
    }
    // fall through to setting mTextRun
  }

  mTextRun = aTextRun;

  // FIXME: Add assertions testing the relationship between
  // GetFontSizeInflation() and whether we have an uninflated text run
  // (but be aware that text runs can go away).
}

bool
nsTextFrame::RemoveTextRun(gfxTextRun* aTextRun)
{
  if (aTextRun == mTextRun) {
    mTextRun = nullptr;
    return true;
  }
  if ((GetStateBits() & TEXT_HAS_FONT_INFLATION) &&
      GetProperty(UninflatedTextRunProperty()) == aTextRun) {
    DeleteProperty(UninflatedTextRunProperty());
    return true;
  }
  return false;
}

void
nsTextFrame::ClearTextRun(nsTextFrame* aStartContinuation,
                          TextRunType aWhichTextRun)
{
  RefPtr<gfxTextRun> textRun = GetTextRun(aWhichTextRun);
  if (!textRun) {
    return;
  }

  DebugOnly<bool> checkmTextrun = textRun == mTextRun;
  UnhookTextRunFromFrames(textRun, aStartContinuation);
  MOZ_ASSERT(checkmTextrun ? !mTextRun
                           : !GetProperty(UninflatedTextRunProperty()));
}

void
nsTextFrame::DisconnectTextRuns()
{
  MOZ_ASSERT(!IsInTextRunUserData(),
             "Textrun mentions this frame in its user data so we can't just disconnect");
  mTextRun = nullptr;
  if ((GetStateBits() & TEXT_HAS_FONT_INFLATION)) {
    DeleteProperty(UninflatedTextRunProperty());
  }
}

nsresult
nsTextFrame::CharacterDataChanged(CharacterDataChangeInfo* aInfo)
{
  if (mContent->HasFlag(NS_HAS_NEWLINE_PROPERTY)) {
    mContent->DeleteProperty(nsGkAtoms::newline);
    mContent->UnsetFlags(NS_HAS_NEWLINE_PROPERTY);
  }
  if (mContent->HasFlag(NS_HAS_FLOWLENGTH_PROPERTY)) {
    mContent->DeleteProperty(nsGkAtoms::flowlength);
    mContent->UnsetFlags(NS_HAS_FLOWLENGTH_PROPERTY);
  }

  // Find the first frame whose text has changed. Frames that are entirely
  // before the text change are completely unaffected.
  nsTextFrame* next;
  nsTextFrame* textFrame = this;
  while (true) {
    next = textFrame->GetNextContinuation();
    if (!next || next->GetContentOffset() > int32_t(aInfo->mChangeStart))
      break;
    textFrame = next;
  }

  int32_t endOfChangedText = aInfo->mChangeStart + aInfo->mReplaceLength;

  // Parent of the last frame that we passed to FrameNeedsReflow (or noticed
  // had already received an earlier FrameNeedsReflow call).
  // (For subsequent frames with this same parent, we can just set their
  // dirty bit without bothering to call FrameNeedsReflow again.)
  nsIFrame* lastDirtiedFrameParent = nullptr;

  nsIPresShell* shell = PresContext()->GetPresShell();
  do {
    // textFrame contained deleted text (or the insertion point,
    // if this was a pure insertion).
    textFrame->RemoveStateBits(TEXT_WHITESPACE_FLAGS);
    textFrame->ClearTextRuns();

    nsIFrame* parentOfTextFrame = textFrame->GetParent();
    bool areAncestorsAwareOfReflowRequest = false;
    if (lastDirtiedFrameParent == parentOfTextFrame) {
      // An earlier iteration of this loop already called
      // FrameNeedsReflow for a sibling of |textFrame|.
      areAncestorsAwareOfReflowRequest = true;
    } else {
      lastDirtiedFrameParent = parentOfTextFrame;
    }

    if (textFrame->mReflowRequestedForCharDataChange) {
      // We already requested a reflow for this frame; nothing to do.
      MOZ_ASSERT(textFrame->HasAnyStateBits(NS_FRAME_IS_DIRTY),
                 "mReflowRequestedForCharDataChange should only be set "
                 "on dirty frames");
    } else {
      // Make sure textFrame is queued up for a reflow.  Also set a flag so we
      // don't waste time doing this again in repeated calls to this method.
      textFrame->mReflowRequestedForCharDataChange = true;
      if (!areAncestorsAwareOfReflowRequest) {
        // Ask the parent frame to reflow me.
        shell->FrameNeedsReflow(textFrame, nsIPresShell::eStyleChange,
                                NS_FRAME_IS_DIRTY);
      } else {
        // We already called FrameNeedsReflow on behalf of an earlier sibling,
        // so we can just mark this frame as dirty and don't need to bother
        // telling its ancestors.
        // Note: if the parent is a block, we're cheating here because we should
        // be marking our line dirty, but we're not. nsTextFrame::SetLength will
        // do that when it gets called during reflow.
        textFrame->AddStateBits(NS_FRAME_IS_DIRTY);
      }
    }
    textFrame->InvalidateFrame();

    // Below, frames that start after the deleted text will be adjusted so that
    // their offsets move with the trailing unchanged text. If this change
    // deletes more text than it inserts, those frame offsets will decrease.
    // We need to maintain the invariant that mContentOffset is non-decreasing
    // along the continuation chain. So we need to ensure that frames that
    // started in the deleted text are all still starting before the
    // unchanged text.
    if (textFrame->mContentOffset > endOfChangedText) {
      textFrame->mContentOffset = endOfChangedText;
    }

    textFrame = textFrame->GetNextContinuation();
  } while (textFrame && textFrame->GetContentOffset() < int32_t(aInfo->mChangeEnd));

  // This is how much the length of the string changed by --- i.e.,
  // how much the trailing unchanged text moved.
  int32_t sizeChange =
    aInfo->mChangeStart + aInfo->mReplaceLength - aInfo->mChangeEnd;

  if (sizeChange) {
    // Fix the offsets of the text frames that start in the trailing
    // unchanged text.
    while (textFrame) {
      textFrame->mContentOffset += sizeChange;
      // XXX we could rescue some text runs by adjusting their user data
      // to reflect the change in DOM offsets
      textFrame->ClearTextRuns();
      textFrame = textFrame->GetNextContinuation();
    }
  }

  return NS_OK;
}

class nsDisplayText : public nsCharClipDisplayItem {
public:
  nsDisplayText(nsDisplayListBuilder* aBuilder, nsTextFrame* aFrame,
                const Maybe<bool>& aIsSelected);
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayText() {
    MOZ_COUNT_DTOR(nsDisplayText);
  }
#endif

  virtual void RestoreState() override
  {
    nsCharClipDisplayItem::RestoreState();
    mOpacity = 1.0f;
  }

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override
  {
    *aSnap = false;
    return mBounds;
  }
  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState,
                       nsTArray<nsIFrame*> *aOutFrames) override {
    if (nsRect(ToReferenceFrame(), mFrame->GetSize()).Intersects(aRect)) {
      aOutFrames->AppendElement(mFrame);
    }
  }
  virtual bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                       mozilla::wr::IpcResourceUpdateQueue& aResources,
                                       const StackingContextHelper& aSc,
                                       WebRenderLayerManager* aManager,
                                       nsDisplayListBuilder* aDisplayListBuilder) override;
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     gfxContext* aCtx) override;
  NS_DISPLAY_DECL_NAME("Text", TYPE_TEXT)

  virtual nsRect GetComponentAlphaBounds(nsDisplayListBuilder* aBuilder) const override
  {
    if (gfxPlatform::GetPlatform()->RespectsFontStyleSmoothing()) {
      // On OS X, web authors can turn off subpixel text rendering using the
      // CSS property -moz-osx-font-smoothing. If they do that, we don't need
      // to use component alpha layers for the affected text.
      if (mFrame->StyleFont()->mFont.smoothing == NS_FONT_SMOOTHING_GRAYSCALE) {
        return nsRect();
      }
    }
    bool snap;
    return GetBounds(aBuilder, &snap);
  }

  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override;

  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion *aInvalidRegion) const override;

  void RenderToContext(gfxContext* aCtx, nsDisplayListBuilder* aBuilder, bool aIsRecording = false);

  bool CanApplyOpacity() const override
  {
    nsTextFrame* f = static_cast<nsTextFrame*>(mFrame);
    if (f->IsSelected()) {
      return false;
    }

    const nsStyleText* textStyle = f->StyleText();
    if (textStyle->mTextShadow) {
      return false;
    }

    nsTextFrame::TextDecorations decorations;
    f->GetTextDecorations(f->PresContext(), nsTextFrame::eResolvedColors, decorations);
    if (decorations.HasDecorationLines()) {
      return false;
    }

    return true;
  }

  void ApplyOpacity(nsDisplayListBuilder* aBuilder,
                    float aOpacity,
                    const DisplayItemClipChain* aClip) override
  {
    NS_ASSERTION(CanApplyOpacity(), "ApplyOpacity should be allowed");
    mOpacity = aOpacity;
    IntersectClip(aBuilder, aClip, false);
  }

  void WriteDebugInfo(std::stringstream& aStream) override
  {
#ifdef DEBUG
    aStream << " (\"";

    nsTextFrame* f = static_cast<nsTextFrame*>(mFrame);
    nsCString buf;
    int32_t totalContentLength;
    f->ToCString(buf, &totalContentLength);

    aStream << buf.get() << "\")";
#endif
  }

  nsRect mBounds;
  float mOpacity;
};

class nsDisplayTextGeometry : public nsCharClipGeometry
{
public:
  nsDisplayTextGeometry(nsDisplayText* aItem, nsDisplayListBuilder* aBuilder)
    : nsCharClipGeometry(aItem, aBuilder)
    , mOpacity(aItem->mOpacity)
  {
    nsTextFrame* f = static_cast<nsTextFrame*>(aItem->Frame());
    f->GetTextDecorations(f->PresContext(), nsTextFrame::eResolvedColors, mDecorations);
  }

  /**
   * We store the computed text decorations here since they are
   * computed using style data from parent frames. Any changes to these
   * styles will only invalidate the parent frame and not this frame.
   */
  nsTextFrame::TextDecorations mDecorations;
  float mOpacity;
};

nsDisplayItemGeometry*
nsDisplayText::AllocateGeometry(nsDisplayListBuilder* aBuilder)
{
  return new nsDisplayTextGeometry(this, aBuilder);
}

void
nsDisplayText::ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion *aInvalidRegion) const
{
  const nsDisplayTextGeometry* geometry = static_cast<const nsDisplayTextGeometry*>(aGeometry);
  nsTextFrame* f = static_cast<nsTextFrame*>(mFrame);

  nsTextFrame::TextDecorations decorations;
  f->GetTextDecorations(f->PresContext(), nsTextFrame::eResolvedColors, decorations);

  bool snap;
  nsRect newRect = geometry->mBounds;
  nsRect oldRect = GetBounds(aBuilder, &snap);
  if (decorations != geometry->mDecorations ||
      mVisIStartEdge != geometry->mVisIStartEdge ||
      mVisIEndEdge != geometry->mVisIEndEdge ||
      !oldRect.IsEqualInterior(newRect) ||
      !geometry->mBorderRect.IsEqualInterior(GetBorderRect()) ||
      mOpacity != geometry->mOpacity) {
    aInvalidRegion->Or(oldRect, newRect);
  }
}

NS_DECLARE_FRAME_PROPERTY_SMALL_VALUE(TextCombineScaleFactorProperty, float)

static float
GetTextCombineScaleFactor(nsTextFrame* aFrame)
{
  float factor = aFrame->GetProperty(TextCombineScaleFactorProperty());
  return factor ? factor : 1.0f;
}

nsDisplayText::nsDisplayText(nsDisplayListBuilder* aBuilder, nsTextFrame* aFrame,
                             const Maybe<bool>& aIsSelected)
  : nsCharClipDisplayItem(aBuilder, aFrame)
  , mOpacity(1.0f)
{
  MOZ_COUNT_CTOR(nsDisplayText);
  mIsFrameSelected = aIsSelected;
  mBounds = mFrame->GetVisualOverflowRectRelativeToSelf() + ToReferenceFrame();
    // Bug 748228
  mBounds.Inflate(mFrame->PresContext()->AppUnitsPerDevPixel());
}

void
nsDisplayText::Paint(nsDisplayListBuilder* aBuilder,
                     gfxContext* aCtx) {
  AUTO_PROFILER_LABEL("nsDisplayText::Paint", GRAPHICS);

  DrawTargetAutoDisableSubpixelAntialiasing disable(aCtx->GetDrawTarget(),
                                                    mDisableSubpixelAA);
  RenderToContext(aCtx, aBuilder);
}

bool
nsDisplayText::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                       mozilla::wr::IpcResourceUpdateQueue& aResources,
                                       const StackingContextHelper& aSc,
                                       WebRenderLayerManager* aManager,
                                       nsDisplayListBuilder* aDisplayListBuilder)
{
  if (!gfxPrefs::LayersAllowTextLayers()) {
    return false;
  }

  if (mBounds.IsEmpty()) {
    return true;
  }

  RefPtr<TextDrawTarget> textDrawer = new TextDrawTarget(aBuilder, aSc, aManager, this, mBounds);
  RefPtr<gfxContext> captureCtx = gfxContext::CreateOrNull(textDrawer);

  RenderToContext(captureCtx, aDisplayListBuilder, true);

  return !textDrawer->HasUnsupportedFeatures();
}

void
nsDisplayText::RenderToContext(gfxContext* aCtx, nsDisplayListBuilder* aBuilder, bool aIsRecording)
{
  nsTextFrame* f = static_cast<nsTextFrame*>(mFrame);

  // Add 1 pixel of dirty area around mVisibleRect to allow us to paint
  // antialiased pixels beyond the measured text extents.
  // This is temporary until we do this in the actual calculation of text extents.
  auto A2D = mFrame->PresContext()->AppUnitsPerDevPixel();
  LayoutDeviceRect extraVisible =
    LayoutDeviceRect::FromAppUnits(mVisibleRect, A2D);
  extraVisible.Inflate(1);

  gfxRect pixelVisible(extraVisible.x, extraVisible.y,
                       extraVisible.width, extraVisible.height);
  pixelVisible.Inflate(2);
  pixelVisible.RoundOut();

  bool willClip = !aBuilder->IsForGenerateGlyphMask() &&
                  !aBuilder->IsForPaintingSelectionBG() &&
                  !aIsRecording;
  if (willClip) {
    aCtx->NewPath();
    aCtx->Rectangle(pixelVisible);
    aCtx->Clip();
  }

  NS_ASSERTION(mVisIStartEdge >= 0, "illegal start edge");
  NS_ASSERTION(mVisIEndEdge >= 0, "illegal end edge");

  gfxContextMatrixAutoSaveRestore matrixSR;

  nsPoint framePt = ToReferenceFrame();
  if (f->StyleContext()->IsTextCombined()) {
    float scaleFactor = GetTextCombineScaleFactor(f);
    if (scaleFactor != 1.0f) {
      matrixSR.SetContext(aCtx);
      // Setup matrix to compress text for text-combine-upright if
      // necessary. This is done here because we want selection be
      // compressed at the same time as text.
      gfxPoint pt = nsLayoutUtils::PointToGfxPoint(framePt, A2D);
      gfxMatrix mat = aCtx->CurrentMatrixDouble()
        .PreTranslate(pt).PreScale(scaleFactor, 1.0).PreTranslate(-pt);
      aCtx->SetMatrixDouble(mat);
    }
  }
  nsTextFrame::PaintTextParams params(aCtx);
  params.framePt = gfx::Point(framePt.x, framePt.y);
  params.dirtyRect = extraVisible;

  if (aBuilder->IsForGenerateGlyphMask()) {
    MOZ_ASSERT(!aBuilder->IsForPaintingSelectionBG());
    params.state = nsTextFrame::PaintTextParams::GenerateTextMask;
  } else if (aBuilder->IsForPaintingSelectionBG()) {
    params.state = nsTextFrame::PaintTextParams::PaintTextBGColor;
  } else {
    params.state = nsTextFrame::PaintTextParams::PaintText;
  }

  f->PaintText(params, *this, mOpacity);

  if (willClip) {
    aCtx->PopClip();
  }
}

void
nsTextFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsDisplayListSet& aLists)
{
  if (!IsVisibleForPainting(aBuilder))
    return;

  DO_GLOBAL_REFLOW_COUNT_DSP("nsTextFrame");

  const nsStyleColor* sc = StyleColor();
  const nsStyleText* st = StyleText();
  bool isTextTransparent =
    NS_GET_A(sc->CalcComplexColor(st->mWebkitTextFillColor)) == 0 &&
    NS_GET_A(sc->CalcComplexColor(st->mWebkitTextStrokeColor)) == 0;
  Maybe<bool> isSelected;
  if (((GetStateBits() & TEXT_NO_RENDERED_GLYPHS) ||
       (isTextTransparent && !StyleText()->HasTextShadow())) &&
      aBuilder->IsForPainting() && !nsSVGUtils::IsInSVGTextSubtree(this)) {
    isSelected.emplace(IsSelected());
    if (!isSelected.value()) {
      TextDecorations textDecs;
      GetTextDecorations(PresContext(), eResolvedColors, textDecs);
      if (!textDecs.HasDecorationLines()) {
        return;
      }
    }
  }

  aLists.Content()->AppendToTop(
    new (aBuilder) nsDisplayText(aBuilder, this, isSelected));
}

static nsIFrame*
GetGeneratedContentOwner(nsIFrame* aFrame, bool* aIsBefore)
{
  *aIsBefore = false;
  while (aFrame && (aFrame->GetStateBits() & NS_FRAME_GENERATED_CONTENT)) {
    if (aFrame->StyleContext()->GetPseudo() == nsCSSPseudoElements::before) {
      *aIsBefore = true;
    }
    aFrame = aFrame->GetParent();
  }
  return aFrame;
}

UniquePtr<SelectionDetails>
nsTextFrame::GetSelectionDetails()
{
  const nsFrameSelection* frameSelection = GetConstFrameSelection();
  if (frameSelection->GetTableCellSelection()) {
    return nullptr;
  }
  if (!(GetStateBits() & NS_FRAME_GENERATED_CONTENT)) {
    UniquePtr<SelectionDetails> details =
      frameSelection->LookUpSelection(mContent, GetContentOffset(),
                                      GetContentLength(), false);
    for (SelectionDetails* sd = details.get(); sd; sd = sd->mNext.get()) {
      sd->mStart += mContentOffset;
      sd->mEnd += mContentOffset;
    }
    return details;
  }

  // Check if the beginning or end of the element is selected, depending on
  // whether we're :before content or :after content.
  bool isBefore;
  nsIFrame* owner = GetGeneratedContentOwner(this, &isBefore);
  if (!owner || !owner->GetContent())
    return nullptr;

  UniquePtr<SelectionDetails> details =
    frameSelection->LookUpSelection(owner->GetContent(),
        isBefore ? 0 : owner->GetContent()->GetChildCount(), 0, false);
  for (SelectionDetails* sd = details.get(); sd; sd = sd->mNext.get()) {
    // The entire text is selected!
    sd->mStart = GetContentOffset();
    sd->mEnd = GetContentEnd();
  }
  return details;
}

static void
PaintSelectionBackground(DrawTarget& aDrawTarget,
                         nscolor aColor,
                         const LayoutDeviceRect& aDirtyRect,
                         const LayoutDeviceRect& aRect,
                         nsTextFrame::DrawPathCallbacks* aCallbacks)
{
  Rect rect = aRect.Intersect(aDirtyRect).ToUnknownRect();
  MaybeSnapToDevicePixels(rect, aDrawTarget);

  if (aCallbacks) {
    aCallbacks->NotifySelectionBackgroundNeedsFill(rect, aColor, aDrawTarget);
  } else {
    ColorPattern color(ToDeviceColor(aColor));
    aDrawTarget.FillRect(rect, color);
  }
}

// Attempt to get the LineBaselineOffset property of aChildFrame
// If not set, calculate this value for all child frames of aBlockFrame
static nscoord
LazyGetLineBaselineOffset(nsIFrame* aChildFrame, nsBlockFrame* aBlockFrame)
{
  bool offsetFound;
  nscoord offset = aChildFrame->GetProperty(
    nsIFrame::LineBaselineOffset(), &offsetFound);

  if (!offsetFound) {
    for (nsBlockFrame::LineIterator line = aBlockFrame->LinesBegin(),
                                    line_end = aBlockFrame->LinesEnd();
         line != line_end; line++) {
      if (line->IsInline()) {
        int32_t n = line->GetChildCount();
        nscoord lineBaseline = line->BStart() + line->GetLogicalAscent();
        for (nsIFrame* lineFrame = line->mFirstChild;
             n > 0; lineFrame = lineFrame->GetNextSibling(), --n) {
          offset = lineBaseline - lineFrame->GetNormalPosition().y;
          lineFrame->SetProperty(nsIFrame::LineBaselineOffset(), offset);
        }
      }
    }
    return aChildFrame->GetProperty(
      nsIFrame::LineBaselineOffset(), &offsetFound);
  } else {
    return offset;
  }
}

static bool IsUnderlineRight(nsIFrame* aFrame)
{
  nsAtom* langAtom = aFrame->StyleFont()->mLanguage;
  if (!langAtom) {
    return false;
  }
  nsAtomString langStr(langAtom);
  return (StringBeginsWith(langStr, NS_LITERAL_STRING("ja")) ||
          StringBeginsWith(langStr, NS_LITERAL_STRING("ko"))) &&
         (langStr.Length() == 2 || langStr[2] == '-');
}

void
nsTextFrame::GetTextDecorations(
                    nsPresContext* aPresContext,
                    nsTextFrame::TextDecorationColorResolution aColorResolution,
                    nsTextFrame::TextDecorations& aDecorations)
{
  const nsCompatibility compatMode = aPresContext->CompatibilityMode();

  bool useOverride = false;
  nscolor overrideColor = NS_RGBA(0, 0, 0, 0);

  bool nearestBlockFound = false;
  // Use writing mode of parent frame for orthogonal text frame to work.
  // See comment in nsTextFrame::DrawTextRunAndDecorations.
  WritingMode wm = GetParent()->GetWritingMode();
  bool vertical = wm.IsVertical();

  nscoord ascent = GetLogicalBaseline(wm);
  // physicalBlockStartOffset represents the offset from our baseline
  // to f's physical block start, which is top in horizontal writing
  // mode, and left in vertical writing modes, in our coordinate space.
  // This physical block start is logical block start in most cases,
  // but for vertical-rl, it is logical block end, and consequently in
  // that case, it starts from the descent instead of ascent.
  nscoord physicalBlockStartOffset =
    wm.IsVerticalRL() ? GetSize().width - ascent : ascent;
  // baselineOffset represents the offset from our baseline to f's baseline or
  // the nearest block's baseline, in our coordinate space, whichever is closest
  // during the particular iteration
  nscoord baselineOffset = 0;

  for (nsIFrame* f = this, *fChild = nullptr;
       f;
       fChild = f,
       f = nsLayoutUtils::GetParentOrPlaceholderFor(f))
  {
    nsStyleContext *const context = f->StyleContext();
    if (!context->HasTextDecorationLines()) {
      break;
    }

    const nsStyleTextReset *const styleText = context->StyleTextReset();
    const uint8_t textDecorations = styleText->mTextDecorationLine;

    if (!useOverride &&
        (NS_STYLE_TEXT_DECORATION_LINE_OVERRIDE_ALL & textDecorations)) {
      // This handles the <a href="blah.html"><font color="green">La
      // la la</font></a> case. The link underline should be green.
      useOverride = true;
      overrideColor =
        nsLayoutUtils::GetColor(f, &nsStyleTextReset::mTextDecorationColor);
    }

    nsBlockFrame* fBlock = nsLayoutUtils::GetAsBlock(f);
    const bool firstBlock = !nearestBlockFound && fBlock;

    // Not updating positions once we hit a parent block is equivalent to
    // the CSS 2.1 spec that blocks should propagate decorations down to their
    // children (albeit the style should be preserved)
    // However, if we're vertically aligned within a block, then we need to
    // recover the correct baseline from the line by querying the FrameProperty
    // that should be set (see nsLineLayout::VerticalAlignLine).
    if (firstBlock) {
      // At this point, fChild can't be null since TextFrames can't be blocks
      if (fChild->VerticalAlignEnum() != NS_STYLE_VERTICAL_ALIGN_BASELINE) {

        // Since offset is the offset in the child's coordinate space, we have
        // to undo the accumulation to bring the transform out of the block's
        // coordinate space
        const nscoord lineBaselineOffset = LazyGetLineBaselineOffset(fChild,
                                                                     fBlock);

        baselineOffset = physicalBlockStartOffset - lineBaselineOffset -
          (vertical ? fChild->GetNormalPosition().x
                    : fChild->GetNormalPosition().y);
      }
    }
    else if (!nearestBlockFound) {
      // offset here is the offset from f's baseline to f's top/left
      // boundary. It's descent for vertical-rl, and ascent otherwise.
      nscoord offset = wm.IsVerticalRL() ?
        f->GetSize().width - f->GetLogicalBaseline(wm) :
        f->GetLogicalBaseline(wm);
      baselineOffset = physicalBlockStartOffset - offset;
    }

    nearestBlockFound = nearestBlockFound || firstBlock;
    physicalBlockStartOffset +=
      vertical ? f->GetNormalPosition().x : f->GetNormalPosition().y;

    const uint8_t style = styleText->mTextDecorationStyle;
    if (textDecorations) {
      nscolor color;
      if (useOverride) {
        color = overrideColor;
      } else if (nsSVGUtils::IsInSVGTextSubtree(this)) {
        // XXX We might want to do something with text-decoration-color when
        //     painting SVG text, but it's not clear what we should do.  We
        //     at least need SVG text decorations to paint with 'fill' if
        //     text-decoration-color has its initial value currentColor.
        //     We could choose to interpret currentColor as "currentFill"
        //     for SVG text, and have e.g. text-decoration-color:red to
        //     override the fill paint of the decoration.
        color = aColorResolution == eResolvedColors ?
                  nsLayoutUtils::GetColor(f, &nsStyleSVG::mFill) :
                  NS_SAME_AS_FOREGROUND_COLOR;
      } else {
        color = nsLayoutUtils::
          GetColor(f, &nsStyleTextReset::mTextDecorationColor);
      }

      bool swapUnderlineAndOverline = vertical && IsUnderlineRight(f);
      const uint8_t kUnderline =
        swapUnderlineAndOverline ? NS_STYLE_TEXT_DECORATION_LINE_OVERLINE :
                                   NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE;
      const uint8_t kOverline =
        swapUnderlineAndOverline ? NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE :
                                   NS_STYLE_TEXT_DECORATION_LINE_OVERLINE;

      if (textDecorations & kUnderline) {
        aDecorations.mUnderlines.AppendElement(
          nsTextFrame::LineDecoration(f, baselineOffset, color, style));
      }
      if (textDecorations & kOverline) {
        aDecorations.mOverlines.AppendElement(
          nsTextFrame::LineDecoration(f, baselineOffset, color, style));
      }
      if (textDecorations & NS_STYLE_TEXT_DECORATION_LINE_LINE_THROUGH) {
        aDecorations.mStrikes.AppendElement(
          nsTextFrame::LineDecoration(f, baselineOffset, color, style));
      }
    }

    // In all modes, if we're on an inline-block or inline-table (or
    // inline-stack, inline-box, inline-grid), we're done.
    // If we're on a ruby frame other than ruby text container, we
    // should continue.
    mozilla::StyleDisplay display = f->GetDisplay();
    if (display != mozilla::StyleDisplay::Inline &&
        (!nsStyleDisplay::IsRubyDisplayType(display) ||
         display == mozilla::StyleDisplay::RubyTextContainer) &&
        nsStyleDisplay::IsDisplayTypeInlineOutside(display)) {
      break;
    }

    // In quirks mode, if we're on an HTML table element, we're done.
    if (compatMode == eCompatibility_NavQuirks &&
        f->GetContent()->IsHTMLElement(nsGkAtoms::table)) {
      break;
    }

    // If we're on an absolutely-positioned element or a floating
    // element, we're done.
    if (f->IsFloating() || f->IsAbsolutelyPositioned()) {
      break;
    }

    // If we're an outer <svg> element, which is classified as an atomic
    // inline-level element, we're done.
    if (f->IsSVGOuterSVGFrame()) {
      break;
    }
  }
}

static float
GetInflationForTextDecorations(nsIFrame* aFrame, nscoord aInflationMinFontSize)
{
  if (nsSVGUtils::IsInSVGTextSubtree(aFrame)) {
    const nsIFrame* container = aFrame;
    while (!container->IsSVGTextFrame()) {
      container = container->GetParent();
    }
    NS_ASSERTION(container, "expected to find an ancestor SVGTextFrame");
    return
      static_cast<const SVGTextFrame*>(container)->GetFontSizeScaleFactor();
  }
  return nsLayoutUtils::FontSizeInflationInner(aFrame, aInflationMinFontSize);
}

struct EmphasisMarkInfo
{
  RefPtr<gfxTextRun> textRun;
  gfxFloat advance;
  gfxFloat baselineOffset;
};

NS_DECLARE_FRAME_PROPERTY_DELETABLE(EmphasisMarkProperty, EmphasisMarkInfo)

already_AddRefed<gfxTextRun>
GenerateTextRunForEmphasisMarks(nsTextFrame* aFrame,
                                nsFontMetrics* aFontMetrics,
                                nsStyleContext* aStyleContext,
                                const nsStyleText* aStyleText)
{
  const nsString& emphasisString = aStyleText->mTextEmphasisStyleString;
  RefPtr<DrawTarget> dt = CreateReferenceDrawTarget(aFrame);
  auto appUnitsPerDevUnit = aFrame->PresContext()->AppUnitsPerDevPixel();
  gfx::ShapedTextFlags flags = nsLayoutUtils::GetTextRunOrientFlagsForStyle(aStyleContext);
  if (flags == gfx::ShapedTextFlags::TEXT_ORIENT_VERTICAL_MIXED) {
    // The emphasis marks should always be rendered upright per spec.
    flags = gfx::ShapedTextFlags::TEXT_ORIENT_VERTICAL_UPRIGHT;
  }
  return aFontMetrics->GetThebesFontGroup()->
    MakeTextRun<char16_t>(emphasisString.get(), emphasisString.Length(),
                          dt, appUnitsPerDevUnit, flags,
                          nsTextFrameUtils::Flags(), nullptr);
}

static nsRubyFrame*
FindFurthestInlineRubyAncestor(nsTextFrame* aFrame)
{
  nsRubyFrame* rubyFrame = nullptr;
  for (nsIFrame* frame = aFrame->GetParent();
       frame && frame->IsFrameOfType(nsIFrame::eLineParticipant);
       frame = frame->GetParent()) {
    if (frame->IsRubyFrame()) {
      rubyFrame = static_cast<nsRubyFrame*>(frame);
    }
  }
  return rubyFrame;
}

nsRect
nsTextFrame::UpdateTextEmphasis(WritingMode aWM, PropertyProvider& aProvider)
{
  const nsStyleText* styleText = StyleText();
  if (!styleText->HasTextEmphasis()) {
    DeleteProperty(EmphasisMarkProperty());
    return nsRect();
  }

  nsStyleContext* styleContext = StyleContext();
  bool isTextCombined = styleContext->IsTextCombined();
  if (isTextCombined) {
    styleContext = GetParent()->StyleContext();
  }
  RefPtr<nsFontMetrics> fm = nsLayoutUtils::
    GetFontMetricsOfEmphasisMarks(styleContext, GetFontSizeInflation());
  EmphasisMarkInfo* info = new EmphasisMarkInfo;
  info->textRun =
    GenerateTextRunForEmphasisMarks(this, fm, styleContext, styleText);
  info->advance = info->textRun->GetAdvanceWidth();

  // Calculate the baseline offset
  LogicalSide side = styleText->TextEmphasisSide(aWM);
  LogicalSize frameSize = GetLogicalSize(aWM);
  // The overflow rect is inflated in the inline direction by half
  // advance of the emphasis mark on each side, so that even if a mark
  // is drawn for a zero-width character, it won't be clipped.
  LogicalRect overflowRect(aWM, -info->advance / 2,
                           /* BStart to be computed below */ 0,
                           frameSize.ISize(aWM) + info->advance,
                           fm->MaxAscent() + fm->MaxDescent());
  RefPtr<nsFontMetrics> baseFontMetrics = isTextCombined
    ? nsLayoutUtils::GetInflatedFontMetricsForFrame(GetParent())
    : do_AddRef(aProvider.GetFontMetrics());
  // When the writing mode is vertical-lr the line is inverted, and thus
  // the ascent and descent are swapped.
  nscoord absOffset = (side == eLogicalSideBStart) != aWM.IsLineInverted() ?
    baseFontMetrics->MaxAscent() + fm->MaxDescent() :
    baseFontMetrics->MaxDescent() + fm->MaxAscent();
  RubyBlockLeadings leadings;
  if (nsRubyFrame* ruby = FindFurthestInlineRubyAncestor(this)) {
    leadings = ruby->GetBlockLeadings();
  }
  if (side == eLogicalSideBStart) {
    info->baselineOffset = -absOffset - leadings.mStart;
    overflowRect.BStart(aWM) = -overflowRect.BSize(aWM) - leadings.mStart;
  } else {
    MOZ_ASSERT(side == eLogicalSideBEnd);
    info->baselineOffset = absOffset + leadings.mEnd;
    overflowRect.BStart(aWM) = frameSize.BSize(aWM) + leadings.mEnd;
  }
  // If text combined, fix the gap between the text frame and its parent.
  if (isTextCombined) {
    nscoord gap = (baseFontMetrics->MaxHeight() - frameSize.BSize(aWM)) / 2;
    overflowRect.BStart(aWM) += gap * (side == eLogicalSideBStart ? -1 : 1);
  }

  SetProperty(EmphasisMarkProperty(), info);
  return overflowRect.GetPhysicalRect(aWM, frameSize.GetPhysicalSize(aWM));
}

void
nsTextFrame::UnionAdditionalOverflow(nsPresContext* aPresContext,
                                     nsIFrame* aBlock,
                                     PropertyProvider& aProvider,
                                     nsRect* aVisualOverflowRect,
                                     bool aIncludeTextDecorations)
{
  const WritingMode wm = GetWritingMode();
  bool verticalRun = mTextRun->IsVertical();
  const gfxFloat appUnitsPerDevUnit = aPresContext->AppUnitsPerDevPixel();

  if (IsFloatingFirstLetterChild()) {
    bool inverted = wm.IsLineInverted();
    // The underline/overline drawable area must be contained in the overflow
    // rect when this is in floating first letter frame at *both* modes.
    // In this case, aBlock is the ::first-letter frame.
    uint8_t decorationStyle = aBlock->StyleContext()->
                                StyleTextReset()->mTextDecorationStyle;
    // If the style is none, let's include decoration line rect as solid style
    // since changing the style from none to solid/dotted/dashed doesn't cause
    // reflow.
    if (decorationStyle == NS_STYLE_TEXT_DECORATION_STYLE_NONE) {
      decorationStyle = NS_STYLE_TEXT_DECORATION_STYLE_SOLID;
    }
    nsFontMetrics* fontMetrics = aProvider.GetFontMetrics();
    nscoord underlineOffset, underlineSize;
    fontMetrics->GetUnderline(underlineOffset, underlineSize);
    nscoord maxAscent = inverted ? fontMetrics->MaxDescent()
                                 : fontMetrics->MaxAscent();

    nsCSSRendering::DecorationRectParams params;
    Float gfxWidth =
      (verticalRun ? aVisualOverflowRect->height
                   : aVisualOverflowRect->width) /
      appUnitsPerDevUnit;
    params.lineSize = Size(gfxWidth, underlineSize / appUnitsPerDevUnit);
    params.ascent = gfxFloat(mAscent) / appUnitsPerDevUnit;
    params.style = decorationStyle;
    params.vertical = verticalRun;
    params.sidewaysLeft = mTextRun->IsSidewaysLeft();

    params.offset = underlineOffset / appUnitsPerDevUnit;
    params.decoration = NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE;
    nsRect underlineRect =
      nsCSSRendering::GetTextDecorationRect(aPresContext, params);
    params.offset = maxAscent / appUnitsPerDevUnit;
    params.decoration = NS_STYLE_TEXT_DECORATION_LINE_OVERLINE;
    nsRect overlineRect =
      nsCSSRendering::GetTextDecorationRect(aPresContext, params);

    aVisualOverflowRect->UnionRect(*aVisualOverflowRect, underlineRect);
    aVisualOverflowRect->UnionRect(*aVisualOverflowRect, overlineRect);

    // XXX If strikeoutSize is much thicker than the underlineSize, it may
    //     cause overflowing from the overflow rect.  However, such case
    //     isn't realistic, we don't need to compute it now.
  }
  if (aIncludeTextDecorations) {
    // Use writing mode of parent frame for orthogonal text frame to
    // work. See comment in nsTextFrame::DrawTextRunAndDecorations.
    WritingMode parentWM = GetParent()->GetWritingMode();
    bool verticalDec = parentWM.IsVertical();
    bool useVerticalMetrics = verticalDec != verticalRun
      ? verticalDec : verticalRun && mTextRun->UseCenterBaseline();

    // Since CSS 2.1 requires that text-decoration defined on ancestors maintain
    // style and position, they can be drawn at virtually any y-offset, so
    // maxima and minima are required to reliably generate the rectangle for
    // them
    TextDecorations textDecs;
    GetTextDecorations(aPresContext, eResolvedColors, textDecs);
    if (textDecs.HasDecorationLines()) {
      nscoord inflationMinFontSize =
        nsLayoutUtils::InflationMinFontSizeFor(aBlock);

      const nscoord measure = verticalDec ? GetSize().height : GetSize().width;
      gfxFloat gfxWidth = measure / appUnitsPerDevUnit;
      gfxFloat ascent = gfxFloat(GetLogicalBaseline(parentWM))
                          / appUnitsPerDevUnit;
      nscoord frameBStart = 0;
      if (parentWM.IsVerticalRL()) {
        frameBStart = GetSize().width;
        ascent = -ascent;
      }

      nsCSSRendering::DecorationRectParams params;
      params.lineSize = Size(gfxWidth, 0);
      params.ascent = ascent;
      params.vertical = verticalDec;
      params.sidewaysLeft = mTextRun->IsSidewaysLeft();

      nscoord topOrLeft(nscoord_MAX), bottomOrRight(nscoord_MIN);
      typedef gfxFont::Metrics Metrics;
      auto accumulateDecorationRect = [&](const LineDecoration& dec,
                                          gfxFloat Metrics::* lineSize,
                                          gfxFloat Metrics::* lineOffset) {
        params.style = dec.mStyle;
        // If the style is solid, let's include decoration line rect of solid
        // style since changing the style from none to solid/dotted/dashed
        // doesn't cause reflow.
        if (params.style == NS_STYLE_TEXT_DECORATION_STYLE_NONE) {
          params.style = NS_STYLE_TEXT_DECORATION_STYLE_SOLID;
        }

        float inflation =
          GetInflationForTextDecorations(dec.mFrame, inflationMinFontSize);
        const Metrics metrics =
          GetFirstFontMetrics(GetFontGroupForFrame(dec.mFrame, inflation),
                              useVerticalMetrics);

        params.lineSize.height = metrics.*lineSize;
        params.offset = metrics.*lineOffset;
        const nsRect decorationRect =
          nsCSSRendering::GetTextDecorationRect(aPresContext, params) +
          (verticalDec ? nsPoint(frameBStart - dec.mBaselineOffset, 0)
                       : nsPoint(0, -dec.mBaselineOffset));

        if (verticalDec) {
          topOrLeft = std::min(decorationRect.x, topOrLeft);
          bottomOrRight = std::max(decorationRect.XMost(), bottomOrRight);
        } else {
          topOrLeft = std::min(decorationRect.y, topOrLeft);
          bottomOrRight = std::max(decorationRect.YMost(), bottomOrRight);
        }
      };

      // Below we loop through all text decorations and compute the rectangle
      // containing all of them, in this frame's coordinate space
      params.decoration = NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE;
      for (const LineDecoration& dec : textDecs.mUnderlines) {
        accumulateDecorationRect(dec, &Metrics::underlineSize,
                                 &Metrics::underlineOffset);
      }
      params.decoration = NS_STYLE_TEXT_DECORATION_LINE_OVERLINE;
      for (const LineDecoration& dec : textDecs.mOverlines) {
        accumulateDecorationRect(dec, &Metrics::underlineSize,
                                 &Metrics::maxAscent);
      }
      params.decoration = NS_STYLE_TEXT_DECORATION_LINE_LINE_THROUGH;
      for (const LineDecoration& dec : textDecs.mStrikes) {
        accumulateDecorationRect(dec, &Metrics::strikeoutSize,
                                 &Metrics::strikeoutOffset);
      }

      aVisualOverflowRect->UnionRect(
        *aVisualOverflowRect,
        verticalDec ? nsRect(topOrLeft, 0, bottomOrRight - topOrLeft, measure)
                    : nsRect(0, topOrLeft, measure, bottomOrRight - topOrLeft));
    }

    aVisualOverflowRect->UnionRect(*aVisualOverflowRect,
                                   UpdateTextEmphasis(parentWM, aProvider));
  }

  // text-stroke overflows
  nscoord textStrokeWidth = StyleText()->mWebkitTextStrokeWidth;
  if (textStrokeWidth > 0) {
    nsRect strokeRect = *aVisualOverflowRect;
    strokeRect.x -= textStrokeWidth;
    strokeRect.y -= textStrokeWidth;
    strokeRect.width += textStrokeWidth;
    strokeRect.height += textStrokeWidth;
    aVisualOverflowRect->UnionRect(*aVisualOverflowRect, strokeRect);
  }

  // Text-shadow overflows
  nsRect shadowRect =
    nsLayoutUtils::GetTextShadowRectsUnion(*aVisualOverflowRect, this);
  aVisualOverflowRect->UnionRect(*aVisualOverflowRect, shadowRect);

  // When this frame is not selected, the text-decoration area must be in
  // frame bounds.
  if (!IsSelected() ||
      !CombineSelectionUnderlineRect(aPresContext, *aVisualOverflowRect))
    return;
  AddStateBits(TEXT_SELECTION_UNDERLINE_OVERFLOWED);
}

gfxFloat
nsTextFrame::ComputeDescentLimitForSelectionUnderline(
               nsPresContext* aPresContext,
               const gfxFont::Metrics& aFontMetrics)
{
  gfxFloat app = aPresContext->AppUnitsPerDevPixel();
  nscoord lineHeightApp =
    ReflowInput::CalcLineHeight(GetContent(),
                                      StyleContext(), NS_AUTOHEIGHT,
                                      GetFontSizeInflation());
  gfxFloat lineHeight = gfxFloat(lineHeightApp) / app;
  if (lineHeight <= aFontMetrics.maxHeight) {
    return aFontMetrics.maxDescent;
  }
  return aFontMetrics.maxDescent + (lineHeight - aFontMetrics.maxHeight) / 2;
}

// Make sure this stays in sync with DrawSelectionDecorations below
static const SelectionTypeMask kSelectionTypesWithDecorations =
  ToSelectionTypeMask(SelectionType::eSpellCheck) |
  ToSelectionTypeMask(SelectionType::eURLStrikeout) |
  ToSelectionTypeMask(SelectionType::eIMERawClause) |
  ToSelectionTypeMask(SelectionType::eIMESelectedRawClause) |
  ToSelectionTypeMask(SelectionType::eIMEConvertedClause) |
  ToSelectionTypeMask(SelectionType::eIMESelectedClause);

/* static */
gfxFloat
nsTextFrame::ComputeSelectionUnderlineHeight(
               nsPresContext* aPresContext,
               const gfxFont::Metrics& aFontMetrics,
               SelectionType aSelectionType)
{
  switch (aSelectionType) {
    case SelectionType::eIMERawClause:
    case SelectionType::eIMESelectedRawClause:
    case SelectionType::eIMEConvertedClause:
    case SelectionType::eIMESelectedClause:
      return aFontMetrics.underlineSize;
    case SelectionType::eSpellCheck: {
      // The thickness of the spellchecker underline shouldn't honor the font
      // metrics.  It should be constant pixels value which is decided from the
      // default font size.  Note that if the actual font size is smaller than
      // the default font size, we should use the actual font size because the
      // computed value from the default font size can be too thick for the
      // current font size.
      nscoord defaultFontSize = aPresContext->GetDefaultFont(
          kPresContext_DefaultVariableFont_ID, nullptr)->size;
      int32_t zoomedFontSize = aPresContext->AppUnitsToDevPixels(
          nsStyleFont::ZoomText(aPresContext, defaultFontSize));
      gfxFloat fontSize = std::min(gfxFloat(zoomedFontSize),
                                   aFontMetrics.emHeight);
      fontSize = std::max(fontSize, 1.0);
      return ceil(fontSize / 20);
    }
    default:
      NS_WARNING("Requested underline style is not valid");
      return aFontMetrics.underlineSize;
  }
}

enum class DecorationType
{
  Normal, Selection
};
struct nsTextFrame::PaintDecorationLineParams
  : nsCSSRendering::DecorationRectParams
{
  gfxContext* context = nullptr;
  LayoutDeviceRect dirtyRect;
  Point pt;
  const nscolor* overrideColor = nullptr;
  nscolor color = NS_RGBA(0, 0, 0, 0);
  gfxFloat icoordInFrame = 0.0f;
  DecorationType decorationType = DecorationType::Normal;
  DrawPathCallbacks* callbacks = nullptr;
};

void
nsTextFrame::PaintDecorationLine(const PaintDecorationLineParams& aParams)
{
  nsCSSRendering::PaintDecorationLineParams params;
  static_cast<nsCSSRendering::DecorationRectParams&>(params) = aParams;
  params.dirtyRect = aParams.dirtyRect.ToUnknownRect();
  params.pt = aParams.pt;
  params.color = aParams.overrideColor ? *aParams.overrideColor : aParams.color;
  params.icoordInFrame = Float(aParams.icoordInFrame);
  if (aParams.callbacks) {
    Rect path = nsCSSRendering::DecorationLineToPath(params);
    if (aParams.decorationType == DecorationType::Normal) {
      aParams.callbacks->PaintDecorationLine(path, params.color);
    } else {
      aParams.callbacks->PaintSelectionDecorationLine(path, params.color);
    }
  } else {
    nsCSSRendering::PaintDecorationLine(
      this, *aParams.context->GetDrawTarget(), params);
  }
}

/**
 * This, plus kSelectionTypesWithDecorations, encapsulates all knowledge
 * about drawing text decoration for selections.
 */
void
nsTextFrame::DrawSelectionDecorations(gfxContext* aContext,
                                      const LayoutDeviceRect& aDirtyRect,
                                      SelectionType aSelectionType,
                                      nsTextPaintStyle& aTextPaintStyle,
                                      const TextRangeStyle &aRangeStyle,
                                      const Point& aPt,
                                      gfxFloat aICoordInFrame,
                                      gfxFloat aWidth,
                                      gfxFloat aAscent,
                                      const gfxFont::Metrics& aFontMetrics,
                                      DrawPathCallbacks* aCallbacks,
                                      bool aVertical,
                                      uint8_t aDecoration)
{
  PaintDecorationLineParams params;
  params.context = aContext;
  params.dirtyRect = aDirtyRect;
  params.pt = aPt;
  params.lineSize.width = aWidth;
  params.ascent = aAscent;
  params.offset = aDecoration == NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE ?
                  aFontMetrics.underlineOffset : aFontMetrics.maxAscent;
  params.decoration = aDecoration;
  params.decorationType = DecorationType::Selection;
  params.callbacks = aCallbacks;
  params.vertical = aVertical;
  params.sidewaysLeft = mTextRun->IsSidewaysLeft();
  params.descentLimit =
    ComputeDescentLimitForSelectionUnderline(aTextPaintStyle.PresContext(),
                                             aFontMetrics);

  float relativeSize;

  switch (aSelectionType) {
    case SelectionType::eIMERawClause:
    case SelectionType::eIMESelectedRawClause:
    case SelectionType::eIMEConvertedClause:
    case SelectionType::eIMESelectedClause:
    case SelectionType::eSpellCheck: {
      int32_t index = nsTextPaintStyle::
        GetUnderlineStyleIndexForSelectionType(aSelectionType);
      bool weDefineSelectionUnderline =
        aTextPaintStyle.GetSelectionUnderlineForPaint(index, &params.color,
                                                      &relativeSize,
                                                      &params.style);
      params.lineSize.height =
        ComputeSelectionUnderlineHeight(aTextPaintStyle.PresContext(),
                                        aFontMetrics, aSelectionType);
      bool isIMEType = aSelectionType != SelectionType::eSpellCheck;

      if (isIMEType) {
        // IME decoration lines should not be drawn on the both ends, i.e., we
        // need to cut both edges of the decoration lines.  Because same style
        // IME selections can adjoin, but the users need to be able to know
        // where are the boundaries of the selections.
        //
        //  X: underline
        //
        //     IME selection #1        IME selection #2      IME selection #3
        //  |                     |                      |
        //  | XXXXXXXXXXXXXXXXXXX | XXXXXXXXXXXXXXXXXXXX | XXXXXXXXXXXXXXXXXXX
        //  +---------------------+----------------------+--------------------
        //   ^                   ^ ^                    ^ ^
        //  gap                  gap                    gap
        params.pt.x += 1.0;
        params.lineSize.width -= 2.0;
      }
      if (isIMEType && aRangeStyle.IsDefined()) {
        // If IME defines the style, that should override our definition.
        if (aRangeStyle.IsLineStyleDefined()) {
          if (aRangeStyle.mLineStyle == TextRangeStyle::LINESTYLE_NONE) {
            return;
          }
          params.style = aRangeStyle.mLineStyle;
          relativeSize = aRangeStyle.mIsBoldLine ? 2.0f : 1.0f;
        } else if (!weDefineSelectionUnderline) {
          // There is no underline style definition.
          return;
        }
        // If underline color is defined and that doesn't depend on the
        // foreground color, we should use the color directly.
        if (aRangeStyle.IsUnderlineColorDefined() &&
            (!aRangeStyle.IsForegroundColorDefined() ||
             aRangeStyle.mUnderlineColor != aRangeStyle.mForegroundColor)) {
          params.color = aRangeStyle.mUnderlineColor;
        }
        // If foreground color or background color is defined, the both colors
        // are computed by GetSelectionTextColors().  Then, we should use its
        // foreground color always.  The color should have sufficient contrast
        // with the background color.
        else if (aRangeStyle.IsForegroundColorDefined() ||
                 aRangeStyle.IsBackgroundColorDefined()) {
          nscolor bg;
          GetSelectionTextColors(aSelectionType, aTextPaintStyle,
                                 aRangeStyle, &params.color, &bg);
        }
        // Otherwise, use the foreground color of the frame.
        else {
          params.color = aTextPaintStyle.GetTextColor();
        }
      } else if (!weDefineSelectionUnderline) {
        // IME doesn't specify the selection style and we don't define selection
        // underline.
        return;
      }
      break;
    }
    case SelectionType::eURLStrikeout: {
      nscoord inflationMinFontSize =
        nsLayoutUtils::InflationMinFontSizeFor(this);
      float inflation =
        GetInflationForTextDecorations(this, inflationMinFontSize);
      const gfxFont::Metrics metrics =
        GetFirstFontMetrics(GetFontGroupForFrame(this, inflation), aVertical);

      relativeSize = 2.0f;
      aTextPaintStyle.GetURLSecondaryColor(&params.color);
      params.style = NS_STYLE_TEXT_DECORATION_STYLE_SOLID;
      params.lineSize.height = metrics.strikeoutSize;
      params.offset = metrics.strikeoutOffset + 0.5;
      params.decoration = NS_STYLE_TEXT_DECORATION_LINE_LINE_THROUGH;
      break;
    }
    default:
      NS_WARNING("Requested selection decorations when there aren't any");
      return;
  }
  params.lineSize.height *= relativeSize;
  params.icoordInFrame = (aVertical ? params.pt.y - aPt.y
                                    : params.pt.x - aPt.x) + aICoordInFrame;
  PaintDecorationLine(params);
}

/* static */
bool
nsTextFrame::GetSelectionTextColors(SelectionType aSelectionType,
                                    nsTextPaintStyle& aTextPaintStyle,
                                    const TextRangeStyle &aRangeStyle,
                                    nscolor* aForeground,
                                    nscolor* aBackground)
{
  switch (aSelectionType) {
    case SelectionType::eNormal:
      return aTextPaintStyle.GetSelectionColors(aForeground, aBackground);
    case SelectionType::eFind:
      aTextPaintStyle.GetHighlightColors(aForeground, aBackground);
      return true;
    case SelectionType::eURLSecondary:
      aTextPaintStyle.GetURLSecondaryColor(aForeground);
      *aBackground = NS_RGBA(0,0,0,0);
      return true;
    case SelectionType::eIMERawClause:
    case SelectionType::eIMESelectedRawClause:
    case SelectionType::eIMEConvertedClause:
    case SelectionType::eIMESelectedClause:
      if (aRangeStyle.IsDefined()) {
        if (!aRangeStyle.IsForegroundColorDefined() &&
            !aRangeStyle.IsBackgroundColorDefined()) {
          *aForeground = aTextPaintStyle.GetTextColor();
          *aBackground = NS_RGBA(0,0,0,0);
          return false;
        }
        if (aRangeStyle.IsForegroundColorDefined()) {
          *aForeground = aRangeStyle.mForegroundColor;
          if (aRangeStyle.IsBackgroundColorDefined()) {
            *aBackground = aRangeStyle.mBackgroundColor;
          } else {
            // If foreground color is defined but background color isn't
            // defined, we can guess that IME must expect that the background
            // color is system's default field background color.
            *aBackground = aTextPaintStyle.GetSystemFieldBackgroundColor();
          }
        } else { // aRangeStyle.IsBackgroundColorDefined() is true
          *aBackground = aRangeStyle.mBackgroundColor;
          // If background color is defined but foreground color isn't defined,
          // we can assume that IME must expect that the foreground color is
          // same as system's field text color.
          *aForeground = aTextPaintStyle.GetSystemFieldForegroundColor();
        }
        return true;
      }
      aTextPaintStyle.GetIMESelectionColors(
        nsTextPaintStyle::GetUnderlineStyleIndexForSelectionType(
          aSelectionType),
        aForeground, aBackground);
      return true;
    default:
      *aForeground = aTextPaintStyle.GetTextColor();
      *aBackground = NS_RGBA(0,0,0,0);
      return false;
  }
}

/**
 * This sets *aShadow to the appropriate shadow, if any, for the given
 * type of selection. Returns true if *aShadow was set.
 * If text-shadow was not specified, *aShadow is left untouched
 * (NOT reset to null), and the function returns false.
 */
static bool GetSelectionTextShadow(nsIFrame* aFrame,
                                   SelectionType aSelectionType,
                                   nsTextPaintStyle& aTextPaintStyle,
                                   nsCSSShadowArray** aShadow)
{
  switch (aSelectionType) {
    case SelectionType::eNormal:
      return aTextPaintStyle.GetSelectionShadow(aShadow);
    default:
      return false;
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
   * aStart and aLength are in the original string. aSelectionDetails is
   * according to the original string.
   * @param aXOffset the offset from the origin of the frame to the start
   * of the text (the left baseline origin for LTR, the right baseline origin
   * for RTL)
   */
  SelectionIterator(SelectionDetails** aSelectionDetails,
                    gfxTextRun::Range aRange, PropertyProvider& aProvider,
                    gfxTextRun* aTextRun, gfxFloat aXOffset);

  /**
   * Returns the next segment of uniformly selected (or not) text.
   * @param aXOffset the offset from the origin of the frame to the start
   * of the text (the left baseline origin for LTR, the right baseline origin
   * for RTL)
   * @param aRange the transformed string range of the text for this segment
   * @param aHyphenWidth if a hyphen is to be rendered after the text, the
   * width of the hyphen, otherwise zero
   * @param aSelectionType the selection type for this segment
   * @param aStyle the selection style for this segment
   * @return false if there are no more segments
   */
  bool GetNextSegment(gfxFloat* aXOffset, gfxTextRun::Range* aRange,
                      gfxFloat* aHyphenWidth,
                      SelectionType* aSelectionType,
                      TextRangeStyle* aStyle);
  void UpdateWithAdvance(gfxFloat aAdvance) {
    mXOffset += aAdvance*mTextRun->GetDirection();
  }

private:
  SelectionDetails**      mSelectionDetails;
  PropertyProvider&       mProvider;
  RefPtr<gfxTextRun>      mTextRun;
  gfxSkipCharsIterator    mIterator;
  gfxTextRun::Range       mOriginalRange;
  gfxFloat                mXOffset;
};

SelectionIterator::SelectionIterator(SelectionDetails** aSelectionDetails,
                                     gfxTextRun::Range aRange,
                                     PropertyProvider& aProvider,
                                     gfxTextRun* aTextRun, gfxFloat aXOffset)
  : mSelectionDetails(aSelectionDetails), mProvider(aProvider),
    mTextRun(aTextRun), mIterator(aProvider.GetStart()),
    mOriginalRange(aRange), mXOffset(aXOffset)
{
  mIterator.SetOriginalOffset(aRange.start);
}

bool SelectionIterator::GetNextSegment(gfxFloat* aXOffset,
                                       gfxTextRun::Range* aRange,
                                       gfxFloat* aHyphenWidth,
                                       SelectionType* aSelectionType,
                                       TextRangeStyle* aStyle)
{
  if (mIterator.GetOriginalOffset() >= int32_t(mOriginalRange.end))
    return false;

  // save offset into transformed string now
  uint32_t runOffset = mIterator.GetSkippedOffset();

  uint32_t index = mIterator.GetOriginalOffset() - mOriginalRange.start;
  SelectionDetails* sdptr = mSelectionDetails[index];
  SelectionType selectionType =
    sdptr ? sdptr->mSelectionType : SelectionType::eNone;
  TextRangeStyle style;
  if (sdptr) {
    style = sdptr->mTextRangeStyle;
  }
  for (++index; index < mOriginalRange.Length(); ++index) {
    if (sdptr != mSelectionDetails[index])
      break;
  }
  mIterator.SetOriginalOffset(index + mOriginalRange.start);

  // Advance to the next cluster boundary
  while (mIterator.GetOriginalOffset() < int32_t(mOriginalRange.end) &&
         !mIterator.IsOriginalCharSkipped() &&
         !mTextRun->IsClusterStart(mIterator.GetSkippedOffset())) {
    mIterator.AdvanceOriginal(1);
  }

  bool haveHyphenBreak =
    (mProvider.GetFrame()->GetStateBits() & TEXT_HYPHEN_BREAK) != 0;
  aRange->start = runOffset;
  aRange->end = mIterator.GetSkippedOffset();
  *aXOffset = mXOffset;
  *aHyphenWidth = 0;
  if (mIterator.GetOriginalOffset() == int32_t(mOriginalRange.end) &&
      haveHyphenBreak) {
    *aHyphenWidth = mProvider.GetHyphenWidth();
  }
  *aSelectionType = selectionType;
  *aStyle = style;
  return true;
}

static void
AddHyphenToMetrics(nsTextFrame* aTextFrame, const gfxTextRun* aBaseTextRun,
                   gfxTextRun::Metrics* aMetrics,
                   gfxFont::BoundingBoxType aBoundingBoxType,
                   DrawTarget* aDrawTarget)
{
  // Fix up metrics to include hyphen
  RefPtr<gfxTextRun> hyphenTextRun =
    GetHyphenTextRun(aBaseTextRun, aDrawTarget, aTextFrame);
  if (!hyphenTextRun) {
    return;
  }

  gfxTextRun::Metrics hyphenMetrics =
    hyphenTextRun->MeasureText(aBoundingBoxType, aDrawTarget);
  if (aTextFrame->GetWritingMode().IsLineInverted()) {
    hyphenMetrics.mBoundingBox.y = -hyphenMetrics.mBoundingBox.YMost();
  }
  aMetrics->CombineWith(hyphenMetrics, aBaseTextRun->IsRightToLeft());
}

void
nsTextFrame::PaintOneShadow(const PaintShadowParams& aParams,
                            nsCSSShadowItem* aShadowDetails,
                            gfxRect& aBoundingBox, uint32_t aBlurFlags)
{
  AUTO_PROFILER_LABEL("nsTextFrame::PaintOneShadow", GRAPHICS);

  gfx::Point shadowOffset(aShadowDetails->mXOffset, aShadowDetails->mYOffset);
  nscoord blurRadius = std::max(aShadowDetails->mRadius, 0);

  nscolor shadowColor = aShadowDetails->mHasColor ? aShadowDetails->mColor
                                                  : aParams.foregroundColor;

  if (auto* textDrawer = aParams.context->GetTextDrawer()) {
    wr::Shadow wrShadow;

    wrShadow.offset = {
      PresContext()->AppUnitsToFloatDevPixels(aShadowDetails->mXOffset),
      PresContext()->AppUnitsToFloatDevPixels(aShadowDetails->mYOffset)
    };

    wrShadow.blur_radius = PresContext()->AppUnitsToFloatDevPixels(blurRadius);
    wrShadow.color = wr::ToColorF(ToDeviceColor(shadowColor));

    textDrawer->AppendShadow(wrShadow);
    return;
  }

  // This rect is the box which is equivalent to where the shadow will be painted.
  // The origin of aBoundingBox is the text baseline left, so we must translate it by
  // that much in order to make the origin the top-left corner of the text bounding box.
  // Note that aLeftSideOffset is line-left, so actually means top offset in
  // vertical writing modes.
  gfxRect shadowGfxRect;
  WritingMode wm = GetWritingMode();
  if (wm.IsVertical()) {
    shadowGfxRect = aBoundingBox;
    if (wm.IsVerticalRL()) {
      // for vertical-RL, reverse direction of x-coords of bounding box
      shadowGfxRect.x = -shadowGfxRect.XMost();
    }
    shadowGfxRect += gfxPoint(aParams.textBaselinePt.x,
                              aParams.framePt.y + aParams.leftSideOffset);
  } else {
    shadowGfxRect =
      aBoundingBox + gfxPoint(aParams.framePt.x + aParams.leftSideOffset,
                              aParams.textBaselinePt.y);
  }
  shadowGfxRect += gfxPoint(shadowOffset.x, shadowOffset.y);

  nsRect shadowRect(NSToCoordRound(shadowGfxRect.X()),
                    NSToCoordRound(shadowGfxRect.Y()),
                    NSToCoordRound(shadowGfxRect.Width()),
                    NSToCoordRound(shadowGfxRect.Height()));

  nsContextBoxBlur contextBoxBlur;
  const auto A2D = PresContext()->AppUnitsPerDevPixel();
  gfxContext* shadowContext = contextBoxBlur.Init(
    shadowRect, 0, blurRadius, A2D, aParams.context,
    LayoutDevicePixel::ToAppUnits(aParams.dirtyRect, A2D), nullptr, aBlurFlags);
  if (!shadowContext)
    return;

  aParams.context->Save();
  aParams.context->SetColor(Color::FromABGR(shadowColor));

  // Draw the text onto our alpha-only surface to capture the alpha values.
  // Remember that the box blur context has a device offset on it, so we don't need to
  // translate any coordinates to fit on the surface.
  gfxFloat advanceWidth;
  nsTextPaintStyle textPaintStyle(this);
  DrawTextParams params(shadowContext);
  params.advanceWidth = &advanceWidth;
  params.dirtyRect = aParams.dirtyRect;
  params.framePt = aParams.framePt + shadowOffset;
  params.provider = aParams.provider;
  params.textStyle = &textPaintStyle;
  params.textColor =
    aParams.context == shadowContext ? shadowColor : NS_RGB(0, 0, 0);
  params.clipEdges = aParams.clipEdges;
  params.drawSoftHyphen = (GetStateBits() & TEXT_HYPHEN_BREAK) != 0;
  // Multi-color shadow is not allowed, so we use the same color of the text color.
  params.decorationOverrideColor = &params.textColor;
  DrawText(aParams.range, aParams.textBaselinePt + shadowOffset, params);

  contextBoxBlur.DoPaint();
  aParams.context->Restore();
}

// Paints selection backgrounds and text in the correct colors. Also computes
// aAllTypes, the union of all selection types that are applying to this text.
bool
nsTextFrame::PaintTextWithSelectionColors(
    const PaintTextSelectionParams& aParams,
    const UniquePtr<SelectionDetails>& aDetails,
    SelectionTypeMask* aAllSelectionTypeMask,
    const nsCharClipDisplayItem::ClipEdges& aClipEdges)
{
  const gfxTextRun::Range& contentRange = aParams.contentRange;

  // Figure out which selections control the colors to use for each character.
  // Note: prevailingSelectionsBuffer is keeping extra raw pointers to
  // uniquely-owned resources, but it's safe because it's temporary and the
  // resources are owned by the caller. Therefore, they'll outlive this object.
  AutoTArray<SelectionDetails*,BIG_TEXT_NODE_SIZE> prevailingSelectionsBuffer;
  SelectionDetails** prevailingSelections =
    prevailingSelectionsBuffer.AppendElements(contentRange.Length(), fallible);
  if (!prevailingSelections) {
    return false;
  }

  SelectionTypeMask allSelectionTypeMask = 0;
  for (uint32_t i = 0; i < contentRange.Length(); ++i) {
    prevailingSelections[i] = nullptr;
  }

  bool anyBackgrounds = false;
  for (SelectionDetails* sdptr = aDetails.get(); sdptr; sdptr = sdptr->mNext.get()) {
    int32_t start = std::max(0, sdptr->mStart - int32_t(contentRange.start));
    int32_t end = std::min(int32_t(contentRange.Length()),
                           sdptr->mEnd - int32_t(contentRange.start));
    SelectionType selectionType = sdptr->mSelectionType;
    if (start < end) {
      allSelectionTypeMask |= ToSelectionTypeMask(selectionType);
      // Ignore selections that don't set colors
      nscolor foreground, background;
      if (GetSelectionTextColors(selectionType, *aParams.textPaintStyle,
                                 sdptr->mTextRangeStyle,
                                 &foreground, &background)) {
        if (NS_GET_A(background) > 0) {
          anyBackgrounds = true;
        }
        for (int32_t i = start; i < end; ++i) {
          // Favour normal selection over IME selections
          if (!prevailingSelections[i] ||
              selectionType < prevailingSelections[i]->mSelectionType) {
            prevailingSelections[i] = sdptr;
          }
        }
      }
    }
  }
  *aAllSelectionTypeMask = allSelectionTypeMask;

  if (!allSelectionTypeMask) {
    // Nothing is selected in the given text range. XXX can this still occur?
    return false;
  }

  bool vertical = mTextRun->IsVertical();
  const gfxFloat startIOffset = vertical ?
    aParams.textBaselinePt.y - aParams.framePt.y :
    aParams.textBaselinePt.x - aParams.framePt.x;
  gfxFloat iOffset, hyphenWidth;
  Range range; // in transformed string
  TextRangeStyle rangeStyle;
  // Draw background colors

  auto* textDrawer = aParams.context->GetTextDrawer();

  if (anyBackgrounds && !aParams.IsGenerateTextMask()) {
    int32_t appUnitsPerDevPixel =
      aParams.textPaintStyle->PresContext()->AppUnitsPerDevPixel();
    SelectionIterator iterator(prevailingSelections, contentRange,
                               *aParams.provider, mTextRun, startIOffset);
    SelectionType selectionType;
    while (iterator.GetNextSegment(&iOffset, &range, &hyphenWidth,
                                   &selectionType, &rangeStyle)) {
      nscolor foreground, background;
      GetSelectionTextColors(selectionType, *aParams.textPaintStyle,
                             rangeStyle, &foreground, &background);
      // Draw background color
      gfxFloat advance = hyphenWidth +
        mTextRun->GetAdvanceWidth(range, aParams.provider);
      if (NS_GET_A(background) > 0) {
        nsRect bgRect;
        gfxFloat offs = iOffset - (mTextRun->IsInlineReversed() ? advance : 0);
        if (vertical) {
          bgRect = nsRect(aParams.framePt.x, aParams.framePt.y + offs,
                          GetSize().width, advance);
        } else {
          bgRect = nsRect(aParams.framePt.x + offs, aParams.framePt.y,
                          advance, GetSize().height);
        }

        LayoutDeviceRect selectionRect =
          LayoutDeviceRect::FromAppUnits(bgRect, appUnitsPerDevPixel);

        if (textDrawer) {
          textDrawer->AppendSelectionRect(selectionRect, ToDeviceColor(background));
        } else {
          PaintSelectionBackground(
            *aParams.context->GetDrawTarget(), background, aParams.dirtyRect,
            selectionRect, aParams.callbacks);
        }
      }
      iterator.UpdateWithAdvance(advance);
    }
  }

  if (aParams.IsPaintBGColor()) {
    return true;
  }

  gfxFloat advance;
  DrawTextParams params(aParams.context);
  params.dirtyRect = aParams.dirtyRect;
  params.framePt = aParams.framePt;
  params.provider = aParams.provider;
  params.textStyle = aParams.textPaintStyle;
  params.clipEdges = &aClipEdges;
  params.advanceWidth = &advance;
  params.callbacks = aParams.callbacks;

  PaintShadowParams shadowParams(aParams);
  shadowParams.provider = aParams.provider;
  shadowParams.clipEdges = &aClipEdges;

  // Draw text
  const nsStyleText* textStyle = StyleText();
  SelectionIterator iterator(prevailingSelections, contentRange,
                             *aParams.provider, mTextRun, startIOffset);
  SelectionType selectionType;
  while (iterator.GetNextSegment(&iOffset, &range, &hyphenWidth,
                                 &selectionType, &rangeStyle)) {
    nscolor foreground, background;
    if (aParams.IsGenerateTextMask()) {
      foreground = NS_RGBA(0, 0, 0, 255);
    } else {
      GetSelectionTextColors(selectionType, *aParams.textPaintStyle,
                             rangeStyle, &foreground, &background);
    }

    gfx::Point textBaselinePt = vertical ?
      gfx::Point(aParams.textBaselinePt.x, aParams.framePt.y + iOffset) :
      gfx::Point(aParams.framePt.x + iOffset, aParams.textBaselinePt.y);

    // Determine what shadow, if any, to draw - either from textStyle
    // or from the ::-moz-selection pseudo-class if specified there
    nsCSSShadowArray* shadow = textStyle->GetTextShadow();
    GetSelectionTextShadow(this, selectionType, *aParams.textPaintStyle,
                           &shadow);
    if (shadow) {
      nscoord startEdge = iOffset;
      if (mTextRun->IsInlineReversed()) {
        startEdge -= hyphenWidth +
          mTextRun->GetAdvanceWidth(range, aParams.provider);
      }
      shadowParams.range = range;
      shadowParams.textBaselinePt = textBaselinePt;
      shadowParams.foregroundColor = foreground;
      shadowParams.leftSideOffset = startEdge;
      PaintShadows(shadow, shadowParams);
    }

    // Draw text segment
    params.textColor = foreground;
    params.textStrokeColor = aParams.textPaintStyle->GetWebkitTextStrokeColor();
    params.textStrokeWidth = aParams.textPaintStyle->GetWebkitTextStrokeWidth();
    params.drawSoftHyphen = hyphenWidth > 0;
    DrawText(range, textBaselinePt, params);
    advance += hyphenWidth;
    iterator.UpdateWithAdvance(advance);
  }
  return true;
}

void
nsTextFrame::PaintTextSelectionDecorations(
    const PaintTextSelectionParams& aParams,
    const UniquePtr<SelectionDetails>& aDetails,
    SelectionType aSelectionType)
{
  // Hide text decorations if we're currently hiding @font-face fallback text
  if (aParams.provider->GetFontGroup()->ShouldSkipDrawing())
    return;

  // Figure out which characters will be decorated for this selection.
  // Note: selectedCharsBuffer is keeping extra raw pointers to
  // uniquely-owned resources, but it's safe because it's temporary and the
  // resources are owned by the caller. Therefore, they'll outlive this object.
  const gfxTextRun::Range& contentRange = aParams.contentRange;
  AutoTArray<SelectionDetails*, BIG_TEXT_NODE_SIZE> selectedCharsBuffer;
  SelectionDetails** selectedChars =
    selectedCharsBuffer.AppendElements(contentRange.Length(), fallible);
  if (!selectedChars) {
    return;
  }
  for (uint32_t i = 0; i < contentRange.Length(); ++i) {
    selectedChars[i] = nullptr;
  }

  for (SelectionDetails* sdptr = aDetails.get(); sdptr; sdptr = sdptr->mNext.get()) {
    if (sdptr->mSelectionType == aSelectionType) {
      int32_t start = std::max(0, sdptr->mStart - int32_t(contentRange.start));
      int32_t end = std::min(int32_t(contentRange.Length()),
                             sdptr->mEnd - int32_t(contentRange.start));
      for (int32_t i = start; i < end; ++i) {
        selectedChars[i] = sdptr;
      }
    }
  }

  gfxFont* firstFont = aParams.provider->GetFontGroup()->GetFirstValidFont();
  bool verticalRun = mTextRun->IsVertical();
  bool rightUnderline = verticalRun && IsUnderlineRight(this);
  const uint8_t kDecoration =
    rightUnderline ? NS_STYLE_TEXT_DECORATION_LINE_OVERLINE :
                     NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE;
  bool useVerticalMetrics = verticalRun && mTextRun->UseCenterBaseline();
  gfxFont::Metrics
    decorationMetrics(firstFont->GetMetrics(useVerticalMetrics ?
      gfxFont::eVertical : gfxFont::eHorizontal));
  if (!useVerticalMetrics) {
    // The potential adjustment from using gfxFontGroup::GetUnderlineOffset
    // is only valid for horizontal font metrics.
    decorationMetrics.underlineOffset =
      aParams.provider->GetFontGroup()->GetUnderlineOffset();
  }

  gfxFloat startIOffset = verticalRun ?
    aParams.textBaselinePt.y - aParams.framePt.y :
    aParams.textBaselinePt.x - aParams.framePt.x;
  SelectionIterator iterator(selectedChars, contentRange,
                             *aParams.provider, mTextRun, startIOffset);
  gfxFloat iOffset, hyphenWidth;
  Range range;
  int32_t app = aParams.textPaintStyle->PresContext()->AppUnitsPerDevPixel();
  // XXX aTextBaselinePt is in AppUnits, shouldn't it be nsFloatPoint?
  Point pt;
  if (verticalRun) {
    pt.x = (aParams.textBaselinePt.x - mAscent) / app;
  } else {
    pt.y = (aParams.textBaselinePt.y - mAscent) / app;
  }
  SelectionType nextSelectionType;
  TextRangeStyle selectedStyle;

  while (iterator.GetNextSegment(&iOffset, &range, &hyphenWidth,
                                 &nextSelectionType, &selectedStyle)) {
    gfxFloat advance = hyphenWidth +
      mTextRun->GetAdvanceWidth(range, aParams.provider);
    if (nextSelectionType == aSelectionType) {
      if (verticalRun) {
        pt.y = (aParams.framePt.y + iOffset -
               (mTextRun->IsInlineReversed() ? advance : 0)) / app;
      } else {
        pt.x = (aParams.framePt.x + iOffset -
               (mTextRun->IsInlineReversed() ? advance : 0)) / app;
      }
      gfxFloat width = Abs(advance) / app;
      gfxFloat xInFrame = pt.x - (aParams.framePt.x / app);
      DrawSelectionDecorations(
        aParams.context, aParams.dirtyRect, aSelectionType,
        *aParams.textPaintStyle, selectedStyle, pt, xInFrame,
        width, mAscent / app, decorationMetrics, aParams.callbacks,
        verticalRun, kDecoration);
    }
    iterator.UpdateWithAdvance(advance);
  }
}

bool
nsTextFrame::PaintTextWithSelection(
    const PaintTextSelectionParams& aParams,
    const nsCharClipDisplayItem::ClipEdges& aClipEdges)
{
  NS_ASSERTION(GetContent()->IsSelectionDescendant(), "wrong paint path");

  UniquePtr<SelectionDetails> details = GetSelectionDetails();
  if (!details) {
    return false;
  }

  SelectionTypeMask allSelectionTypeMask;
  if (!PaintTextWithSelectionColors(aParams, details, &allSelectionTypeMask,
                                    aClipEdges)) {
    return false;
  }
  // Iterate through just the selection rawSelectionTypes that paint decorations
  // and paint decorations for any that actually occur in this frame. Paint
  // higher-numbered selection rawSelectionTypes below lower-numered ones on the
  // general principal that lower-numbered selections are higher priority.
  allSelectionTypeMask &= kSelectionTypesWithDecorations;
  MOZ_ASSERT(kPresentSelectionTypes[0] == SelectionType::eNormal,
             "The following for loop assumes that the first item of "
             "kPresentSelectionTypes is SelectionType::eNormal");
  for (size_t i = ArrayLength(kPresentSelectionTypes) - 1; i >= 1; --i) {
    SelectionType selectionType = kPresentSelectionTypes[i];
    if (ToSelectionTypeMask(selectionType) & allSelectionTypeMask) {
      // There is some selection of this selectionType. Try to paint its
      // decorations (there might not be any for this type but that's OK,
      // PaintTextSelectionDecorations will exit early).
      PaintTextSelectionDecorations(aParams, details, selectionType);
    }
  }

  return true;
}

void
nsTextFrame::DrawEmphasisMarks(gfxContext* aContext,
                               WritingMode aWM,
                               const gfx::Point& aTextBaselinePt,
                               const gfx::Point& aFramePt, Range aRange,
                               const nscolor* aDecorationOverrideColor,
                               PropertyProvider* aProvider)
{
  const EmphasisMarkInfo* info = GetProperty(EmphasisMarkProperty());
  if (!info) {
    return;
  }

  bool isTextCombined = StyleContext()->IsTextCombined();
  nscolor color = aDecorationOverrideColor ? *aDecorationOverrideColor :
    nsLayoutUtils::GetColor(this, &nsStyleText::mTextEmphasisColor);
  aContext->SetColor(Color::FromABGR(color));
  gfx::Point pt;
  if (!isTextCombined) {
    pt = aTextBaselinePt;
  } else {
    MOZ_ASSERT(aWM.IsVertical());
    pt = aFramePt;
    if (aWM.IsVerticalRL()) {
      pt.x += GetSize().width - GetLogicalBaseline(aWM);
    } else {
      pt.x += GetLogicalBaseline(aWM);
    }
  }
  if (!aWM.IsVertical()) {
    pt.y += info->baselineOffset;
  } else {
    if (aWM.IsVerticalRL()) {
      pt.x -= info->baselineOffset;
    } else {
      pt.x += info->baselineOffset;
    }
  }
  if (!isTextCombined) {
    mTextRun->DrawEmphasisMarks(aContext, info->textRun.get(),
                                info->advance, pt, aRange, aProvider);
  } else {
    pt.y += (GetSize().height - info->advance) / 2;
    gfxTextRun::DrawParams params(aContext);
    info->textRun->Draw(Range(info->textRun.get()), pt,
                        params);
  }
}

nscolor
nsTextFrame::GetCaretColorAt(int32_t aOffset)
{
  NS_PRECONDITION(aOffset >= 0, "aOffset must be positive");

  nscolor result = nsFrame::GetCaretColorAt(aOffset);
  gfxSkipCharsIterator iter = EnsureTextRun(nsTextFrame::eInflated);
  PropertyProvider provider(this, iter, nsTextFrame::eInflated);
  int32_t contentOffset = provider.GetStart().GetOriginalOffset();
  int32_t contentLength = provider.GetOriginalLength();
  NS_PRECONDITION(aOffset >= contentOffset &&
                  aOffset <= contentOffset + contentLength,
                  "aOffset must be in the frame's range");
  int32_t offsetInFrame = aOffset - contentOffset;
  if (offsetInFrame < 0 || offsetInFrame >= contentLength) {
    return result;
  }

  bool isSolidTextColor = true;
  if (nsSVGUtils::IsInSVGTextSubtree(this)) {
    const nsStyleSVG* style = StyleSVG();
    if (style->mFill.Type() != eStyleSVGPaintType_None &&
        style->mFill.Type() != eStyleSVGPaintType_Color) {
      isSolidTextColor = false;
    }
  }

  nsTextPaintStyle textPaintStyle(this);
  textPaintStyle.SetResolveColors(isSolidTextColor);
  UniquePtr<SelectionDetails> details = GetSelectionDetails();
  SelectionType selectionType = SelectionType::eNone;
  for (SelectionDetails* sdptr = details.get(); sdptr; sdptr = sdptr->mNext.get()) {
    int32_t start = std::max(0, sdptr->mStart - contentOffset);
    int32_t end = std::min(contentLength, sdptr->mEnd - contentOffset);
    if (start <= offsetInFrame && offsetInFrame < end &&
        (selectionType == SelectionType::eNone ||
         sdptr->mSelectionType < selectionType)) {
      nscolor foreground, background;
      if (GetSelectionTextColors(sdptr->mSelectionType, textPaintStyle,
                                 sdptr->mTextRangeStyle,
                                 &foreground, &background)) {
        if (!isSolidTextColor &&
            NS_IS_SELECTION_SPECIAL_COLOR(foreground)) {
          result = NS_RGBA(0, 0, 0, 255);
        } else {
          result = foreground;
        }
        selectionType = sdptr->mSelectionType;
      }
    }
  }

  return result;
}

static gfxTextRun::Range
ComputeTransformedRange(PropertyProvider& aProvider)
{
  gfxSkipCharsIterator iter(aProvider.GetStart());
  uint32_t start = iter.GetSkippedOffset();
  iter.AdvanceOriginal(aProvider.GetOriginalLength());
  return gfxTextRun::Range(start, iter.GetSkippedOffset());
}

bool
nsTextFrame::MeasureCharClippedText(nscoord aVisIStartEdge,
                                    nscoord aVisIEndEdge,
                                    nscoord* aSnappedStartEdge,
                                    nscoord* aSnappedEndEdge)
{
  // We need a *reference* rendering context (not one that might have a
  // transform), so we don't have a rendering context argument.
  // XXX get the block and line passed to us somehow! This is slow!
  gfxSkipCharsIterator iter = EnsureTextRun(nsTextFrame::eInflated);
  if (!mTextRun)
    return false;

  PropertyProvider provider(this, iter, nsTextFrame::eInflated);
  // Trim trailing whitespace
  provider.InitializeForDisplay(true);

  Range range = ComputeTransformedRange(provider);
  uint32_t startOffset = range.start;
  uint32_t maxLength = range.Length();
  return MeasureCharClippedText(provider, aVisIStartEdge, aVisIEndEdge,
                                &startOffset, &maxLength,
                                aSnappedStartEdge, aSnappedEndEdge);
}

static uint32_t GetClusterLength(const gfxTextRun* aTextRun,
                                 uint32_t    aStartOffset,
                                 uint32_t    aMaxLength,
                                 bool        aIsRTL)
{
  uint32_t clusterLength = aIsRTL ? 0 : 1;
  while (clusterLength < aMaxLength) {
    if (aTextRun->IsClusterStart(aStartOffset + clusterLength)) {
      if (aIsRTL) {
        ++clusterLength;
      }
      break;
    }
    ++clusterLength;
  }
  return clusterLength;
}

bool
nsTextFrame::MeasureCharClippedText(PropertyProvider& aProvider,
                                    nscoord aVisIStartEdge,
                                    nscoord aVisIEndEdge,
                                    uint32_t* aStartOffset,
                                    uint32_t* aMaxLength,
                                    nscoord*  aSnappedStartEdge,
                                    nscoord*  aSnappedEndEdge)
{
  *aSnappedStartEdge = 0;
  *aSnappedEndEdge = 0;
  if (aVisIStartEdge <= 0 && aVisIEndEdge <= 0) {
    return true;
  }

  uint32_t offset = *aStartOffset;
  uint32_t maxLength = *aMaxLength;
  const nscoord frameISize = ISize();
  const bool rtl = mTextRun->IsRightToLeft();
  gfxFloat advanceWidth = 0;
  const nscoord startEdge = rtl ? aVisIEndEdge : aVisIStartEdge;
  if (startEdge > 0) {
    const gfxFloat maxAdvance = gfxFloat(startEdge);
    while (maxLength > 0) {
      uint32_t clusterLength =
        GetClusterLength(mTextRun, offset, maxLength, rtl);
      advanceWidth += mTextRun->
        GetAdvanceWidth(Range(offset, offset + clusterLength), &aProvider);
      maxLength -= clusterLength;
      offset += clusterLength;
      if (advanceWidth >= maxAdvance) {
        break;
      }
    }
    nscoord* snappedStartEdge = rtl ? aSnappedEndEdge : aSnappedStartEdge;
    *snappedStartEdge = NSToCoordFloor(advanceWidth);
    *aStartOffset = offset;
  }

  const nscoord endEdge = rtl ? aVisIStartEdge : aVisIEndEdge;
  if (endEdge > 0) {
    const gfxFloat maxAdvance = gfxFloat(frameISize - endEdge);
    while (maxLength > 0) {
      uint32_t clusterLength =
        GetClusterLength(mTextRun, offset, maxLength, rtl);
      gfxFloat nextAdvance = advanceWidth + mTextRun->GetAdvanceWidth(
          Range(offset, offset + clusterLength), &aProvider);
      if (nextAdvance > maxAdvance) {
        break;
      }
      // This cluster fits, include it.
      advanceWidth = nextAdvance;
      maxLength -= clusterLength;
      offset += clusterLength;
    }
    maxLength = offset - *aStartOffset;
    nscoord* snappedEndEdge = rtl ? aSnappedStartEdge : aSnappedEndEdge;
    *snappedEndEdge = NSToCoordFloor(gfxFloat(frameISize) - advanceWidth);
  }
  *aMaxLength = maxLength;
  return maxLength != 0;
}

void
nsTextFrame::PaintShadows(nsCSSShadowArray* aShadow,
                          const PaintShadowParams& aParams)
{
  if (!aShadow) {
    return;
  }

  gfxTextRun::Metrics shadowMetrics =
    mTextRun->MeasureText(aParams.range, gfxFont::LOOSE_INK_EXTENTS,
                          nullptr, aParams.provider);
  if (GetWritingMode().IsLineInverted()) {
    Swap(shadowMetrics.mAscent, shadowMetrics.mDescent);
    shadowMetrics.mBoundingBox.y = -shadowMetrics.mBoundingBox.YMost();
  }
  if (GetStateBits() & TEXT_HYPHEN_BREAK) {
    AddHyphenToMetrics(this, mTextRun, &shadowMetrics,
                       gfxFont::LOOSE_INK_EXTENTS,
                       aParams.context->GetDrawTarget());
  }
  // Add bounds of text decorations
  gfxRect decorationRect(0, -shadowMetrics.mAscent,
      shadowMetrics.mAdvanceWidth, shadowMetrics.mAscent + shadowMetrics.mDescent);
  shadowMetrics.mBoundingBox.UnionRect(shadowMetrics.mBoundingBox,
                                       decorationRect);

  // If the textrun uses any color or SVG fonts, we need to force use of a mask
  // for shadow rendering even if blur radius is zero.
  // Force disable hardware acceleration for text shadows since it's usually
  // more expensive than just doing it on the CPU.
  uint32_t blurFlags = nsContextBoxBlur::DISABLE_HARDWARE_ACCELERATION_BLUR;
  uint32_t numGlyphRuns;
  const gfxTextRun::GlyphRun* run = mTextRun->GetGlyphRuns(&numGlyphRuns);
  while (numGlyphRuns-- > 0) {
    if (run->mFont->AlwaysNeedsMaskForShadow()) {
      blurFlags |= nsContextBoxBlur::FORCE_MASK;
      break;
    }
    run++;
  }

  if (mTextRun->IsVertical()) {
    Swap(shadowMetrics.mBoundingBox.x, shadowMetrics.mBoundingBox.y);
    Swap(shadowMetrics.mBoundingBox.width, shadowMetrics.mBoundingBox.height);
  }

  for (uint32_t i = aShadow->Length(); i > 0; --i) {
    PaintOneShadow(aParams, aShadow->ShadowAt(i - 1),
                   shadowMetrics.mBoundingBox, blurFlags);
  }
}

static bool
ShouldDrawSelection(const nsIFrame* aFrame)
{
  // Normal text-with-selection rendering sequence is:
  //   * Paint background > Paint text-selection-color > Paint text
  // When we have an parent frame with background-clip-text style, rendering
  // sequence changes to:
  //   * Paint text-selection-color > Paint background > Paint text
  //
  // If there is a parent frame has background-clip:text style,
  // text-selection-color should be drawn with the background of that parent
  // frame, so we should not draw it again while painting text frames.

  if (!aFrame) {
    return true;
  }

  const nsStyleBackground* bg = aFrame->StyleContext()->StyleBackground();
  const nsStyleImageLayers& layers = bg->mImage;
  NS_FOR_VISIBLE_IMAGE_LAYERS_BACK_TO_FRONT(i, layers) {
    if (layers.mLayers[i].mClip == StyleGeometryBox::Text) {
      return false;
    }
  }

  return ShouldDrawSelection(aFrame->GetParent());
}

void
nsTextFrame::PaintText(const PaintTextParams& aParams,
                       const nsCharClipDisplayItem& aItem,
                       float aOpacity /* = 1.0f */)
{
  // Don't pass in the rendering context here, because we need a
  // *reference* context and rendering context might have some transform
  // in it
  // XXX get the block and line passed to us somehow! This is slow!
  gfxSkipCharsIterator iter = EnsureTextRun(nsTextFrame::eInflated);
  if (!mTextRun)
    return;

  PropertyProvider provider(this, iter, nsTextFrame::eInflated);
  if (aItem.mIsFrameSelected.isNothing()) {
    aItem.mIsFrameSelected.emplace(IsSelected());
  }
  // Trim trailing whitespace, unless we're painting a selection highlight,
  // which should include trailing spaces if present (bug 1146754).
  provider.InitializeForDisplay(!aItem.mIsFrameSelected.value());

  const bool reversed = mTextRun->IsInlineReversed();
  const bool verticalRun = mTextRun->IsVertical();
  WritingMode wm = GetWritingMode();
  const float frameWidth = GetSize().width;
  const float frameHeight = GetSize().height;
  gfx::Point textBaselinePt;
  if (verticalRun) {
    if (wm.IsVerticalLR()) {
      textBaselinePt.x = nsLayoutUtils::GetSnappedBaselineX(
        this, aParams.context, nscoord(aParams.framePt.x), mAscent);
    } else {
      textBaselinePt.x = nsLayoutUtils::GetSnappedBaselineX(
        this, aParams.context, nscoord(aParams.framePt.x) + frameWidth,
        -mAscent);
    }
    textBaselinePt.y = reversed ? aParams.framePt.y + frameHeight
                                : aParams.framePt.y;
  } else {
    textBaselinePt =
      gfx::Point(reversed ? aParams.framePt.x + frameWidth : aParams.framePt.x,
                 nsLayoutUtils::GetSnappedBaselineY(
                   this, aParams.context, aParams.framePt.y, mAscent));
  }
  Range range = ComputeTransformedRange(provider);
  uint32_t startOffset = range.start;
  uint32_t maxLength = range.Length();
  nscoord snappedStartEdge, snappedEndEdge;
  if (!MeasureCharClippedText(provider, aItem.mVisIStartEdge, aItem.mVisIEndEdge,
         &startOffset, &maxLength, &snappedStartEdge, &snappedEndEdge)) {
    return;
  }
  if (verticalRun) {
    textBaselinePt.y += reversed ? -snappedEndEdge : snappedStartEdge;
  } else {
    textBaselinePt.x += reversed ? -snappedEndEdge : snappedStartEdge;
  }
  nsCharClipDisplayItem::ClipEdges clipEdges(aItem, snappedStartEdge,
                                             snappedEndEdge);
  nsTextPaintStyle textPaintStyle(this);
  textPaintStyle.SetResolveColors(!aParams.callbacks);

  // Fork off to the (slower) paint-with-selection path if necessary.
  if (aItem.mIsFrameSelected.value() &&
      (aParams.IsPaintBGColor() || ShouldDrawSelection(this->GetParent()))) {
    MOZ_ASSERT(aOpacity == 1.0f, "We don't support opacity with selections!");
    gfxSkipCharsIterator tmp(provider.GetStart());
    Range contentRange(
      uint32_t(tmp.ConvertSkippedToOriginal(startOffset)),
      uint32_t(tmp.ConvertSkippedToOriginal(startOffset + maxLength)));
    PaintTextSelectionParams params(aParams);
    params.textBaselinePt = textBaselinePt;
    params.provider = &provider;
    params.contentRange = contentRange;
    params.textPaintStyle = &textPaintStyle;
    if (PaintTextWithSelection(params, clipEdges)) {
      return;
    }
  }

  if (aParams.IsPaintBGColor()) {
    return;
  }

  nscolor foregroundColor = aParams.IsGenerateTextMask()
                            ? NS_RGBA(0, 0, 0, 255)
                            : textPaintStyle.GetTextColor();
  if (aOpacity != 1.0f) {
    gfx::Color gfxColor = gfx::Color::FromABGR(foregroundColor);
    gfxColor.a *= aOpacity;
    foregroundColor = gfxColor.ToABGR();
  }

  nscolor textStrokeColor = aParams.IsGenerateTextMask()
                            ? NS_RGBA(0, 0, 0, 255)
                            : textPaintStyle.GetWebkitTextStrokeColor();
  if (aOpacity != 1.0f) {
    gfx::Color gfxColor = gfx::Color::FromABGR(textStrokeColor);
    gfxColor.a *= aOpacity;
    textStrokeColor = gfxColor.ToABGR();
  }

  range = Range(startOffset, startOffset + maxLength);
  if (!aParams.callbacks && aParams.IsPaintText()) {
    const nsStyleText* textStyle = StyleText();
    PaintShadowParams shadowParams(aParams);
    shadowParams.range = range;
    shadowParams.textBaselinePt = textBaselinePt;
    shadowParams.leftSideOffset = snappedStartEdge;
    shadowParams.provider = &provider;
    shadowParams.foregroundColor = foregroundColor;
    shadowParams.clipEdges = &clipEdges;
    PaintShadows(textStyle->mTextShadow, shadowParams);
  }

  gfxFloat advanceWidth;
  DrawTextParams params(aParams.context);
  params.dirtyRect = aParams.dirtyRect;
  params.framePt = aParams.framePt;
  params.provider = &provider;
  params.advanceWidth = &advanceWidth;
  params.textStyle = &textPaintStyle;
  params.textColor = foregroundColor;
  params.textStrokeColor = textStrokeColor;
  params.textStrokeWidth = textPaintStyle.GetWebkitTextStrokeWidth();
  params.clipEdges = &clipEdges;
  params.drawSoftHyphen = (GetStateBits() & TEXT_HYPHEN_BREAK) != 0;
  params.contextPaint = aParams.contextPaint;
  params.callbacks = aParams.callbacks;
  DrawText(range, textBaselinePt, params);
}

static void
DrawTextRun(const gfxTextRun* aTextRun,
            const gfx::Point& aTextBaselinePt,
            gfxTextRun::Range aRange,
            const nsTextFrame::DrawTextRunParams& aParams)
{
  gfxTextRun::DrawParams params(aParams.context);
  params.provider = aParams.provider;
  params.advanceWidth = aParams.advanceWidth;
  params.contextPaint = aParams.contextPaint;
  params.callbacks = aParams.callbacks;
  if (aParams.callbacks) {
    aParams.callbacks->NotifyBeforeText(aParams.textColor);
    params.drawMode = DrawMode::GLYPH_PATH;
    aTextRun->Draw(aRange, aTextBaselinePt, params);
    aParams.callbacks->NotifyAfterText();
  } else {
    auto* textDrawer = aParams.context->GetTextDrawer();
    if (NS_GET_A(aParams.textColor) != 0 || textDrawer) {
      aParams.context->SetColor(Color::FromABGR(aParams.textColor));
    } else {
      params.drawMode = DrawMode::GLYPH_STROKE;
    }

    if ((NS_GET_A(aParams.textStrokeColor) != 0 || textDrawer) &&
        aParams.textStrokeWidth != 0.0f) {
      if (textDrawer) {
        textDrawer->FoundUnsupportedFeature();
        return;
      }
      StrokeOptions strokeOpts;
      params.drawMode |= DrawMode::GLYPH_STROKE;
      params.textStrokeColor = aParams.textStrokeColor;
      strokeOpts.mLineWidth = aParams.textStrokeWidth;
      params.strokeOpts = &strokeOpts;
      aTextRun->Draw(aRange, aTextBaselinePt, params);
    } else {
      aTextRun->Draw(aRange, aTextBaselinePt, params);
    }
  }
}

void
nsTextFrame::DrawTextRun(Range aRange, const gfx::Point& aTextBaselinePt,
                         const DrawTextRunParams& aParams)
{
  MOZ_ASSERT(aParams.advanceWidth, "Must provide advanceWidth");

  ::DrawTextRun(mTextRun, aTextBaselinePt, aRange, aParams);

  if (aParams.drawSoftHyphen) {
    // Don't use ctx as the context, because we need a reference context here,
    // ctx may be transformed.
    RefPtr<gfxTextRun> hyphenTextRun =
      GetHyphenTextRun(mTextRun, nullptr, this);
    if (hyphenTextRun) {
      // For right-to-left text runs, the soft-hyphen is positioned at the left
      // of the text, minus its own width
      float hyphenBaselineX = aTextBaselinePt.x +
        mTextRun->GetDirection() * (*aParams.advanceWidth) -
        (mTextRun->IsRightToLeft() ? hyphenTextRun->GetAdvanceWidth() : 0);
      DrawTextRunParams params = aParams;
      params.provider = nullptr;
      params.advanceWidth = nullptr;
      ::DrawTextRun(hyphenTextRun.get(),
                    gfx::Point(hyphenBaselineX, aTextBaselinePt.y),
                    Range(hyphenTextRun.get()), params);
    }
  }
}

void
nsTextFrame::DrawTextRunAndDecorations(Range aRange,
                                       const gfx::Point& aTextBaselinePt,
                                       const DrawTextParams& aParams,
                                       const TextDecorations& aDecorations)
{
    const gfxFloat app =
      aParams.textStyle->PresContext()->AppUnitsPerDevPixel();
    // Writing mode of parent frame is used because the text frame may
    // be orthogonal to its parent when text-combine-upright is used or
    // its parent has "display: contents", and in those cases, we want
    // to draw the decoration lines according to parents' direction
    // rather than ours.
    const WritingMode wm = GetParent()->GetWritingMode();
    bool verticalDec = wm.IsVertical();
    bool verticalRun = mTextRun->IsVertical();
    // If the text run and the decoration is orthogonal, we choose the
    // metrics for decoration so that decoration line won't be broken.
    bool useVerticalMetrics = verticalDec != verticalRun
      ? verticalDec : verticalRun && mTextRun->UseCenterBaseline();

    // XXX aFramePt is in AppUnits, shouldn't it be nsFloatPoint?
    nscoord x = NSToCoordRound(aParams.framePt.x);
    nscoord y = NSToCoordRound(aParams.framePt.y);

    // 'measure' here is textrun-relative, so for a horizontal run it's the
    // width, while for a vertical run it's the height of the decoration
    const nsSize frameSize = GetSize();
    nscoord measure = verticalDec ? frameSize.height : frameSize.width;

    if (verticalDec) {
      aParams.clipEdges->Intersect(&y, &measure);
    } else {
      aParams.clipEdges->Intersect(&x, &measure);
    }

    // decSize is a textrun-relative size, so its 'width' field is actually
    // the run-relative measure, and 'height' will be the line thickness
    gfxFloat ascent = gfxFloat(GetLogicalBaseline(wm)) / app;
    // The starting edge of the frame in block direction
    gfxFloat frameBStart = verticalDec ? aParams.framePt.x : aParams.framePt.y;

    // In vertical-rl mode, block coordinates are measured from the
    // right, so we need to adjust here.
    if (wm.IsVerticalRL()) {
      frameBStart += frameSize.width;
      ascent = -ascent;
    }

    nscoord inflationMinFontSize =
      nsLayoutUtils::InflationMinFontSizeFor(this);

    PaintDecorationLineParams params;
    params.context = aParams.context;
    params.dirtyRect = aParams.dirtyRect;
    params.overrideColor = aParams.decorationOverrideColor;
    params.callbacks = aParams.callbacks;
    // pt is the physical point where the decoration is to be drawn,
    // relative to the frame; one of its coordinates will be updated below.
    params.pt = Point(x / app, y / app);
    Float& bCoord = verticalDec ? params.pt.x : params.pt.y;
    params.lineSize = Size(measure / app, 0);
    params.ascent = ascent;
    params.vertical = verticalDec;
    params.sidewaysLeft = mTextRun->IsSidewaysLeft();

    // The matrix of the context may have been altered for text-combine-
    // upright. However, we want to draw decoration lines unscaled, thus
    // we need to revert the scaling here.
    gfxContextMatrixAutoSaveRestore scaledRestorer;
    if (StyleContext()->IsTextCombined()) {
      float scaleFactor = GetTextCombineScaleFactor(this);
      if (scaleFactor != 1.0f) {
        scaledRestorer.SetContext(aParams.context);
        gfxMatrix unscaled = aParams.context->CurrentMatrixDouble();
        gfxPoint pt(x / app, y / app);
        unscaled.PreTranslate(pt).PreScale(1.0f / scaleFactor, 1.0f).PreTranslate(-pt);
        aParams.context->SetMatrixDouble(unscaled);
      }
    }

    typedef gfxFont::Metrics Metrics;
    auto paintDecorationLine = [&](const LineDecoration& dec,
                                   gfxFloat Metrics::* lineSize,
                                   gfxFloat Metrics::* lineOffset) {
      if (dec.mStyle == NS_STYLE_TEXT_DECORATION_STYLE_NONE) {
        return;
      }

      float inflation =
        GetInflationForTextDecorations(dec.mFrame, inflationMinFontSize);
      const Metrics metrics =
        GetFirstFontMetrics(GetFontGroupForFrame(dec.mFrame, inflation),
                            useVerticalMetrics);

      params.lineSize.height = metrics.*lineSize;
      bCoord = (frameBStart - dec.mBaselineOffset) / app;

      params.color = dec.mColor;
      params.offset = metrics.*lineOffset;
      params.style = dec.mStyle;
      PaintDecorationLine(params);
    };

    // We create a clip region in order to draw the decoration lines only in the
    // range of the text. Restricting the draw area prevents the decoration lines
    // to be drawn multiple times when a part of the text is selected.

    // We skip clipping for the following cases:
    // - drawing the whole text
    // - having different orientation of the text and the writing-mode, such as
    //   "text-combine-upright" (Bug 1408825)
    bool skipClipping = aRange.Length() == mTextRun->GetLength() ||
                        verticalDec != verticalRun;

    gfxRect clipRect;
    if (!skipClipping) {
      // Get the inline-size according to the specified range.
      gfxFloat clipLength = mTextRun->GetAdvanceWidth(aRange, aParams.provider);
      nsRect visualRect = GetVisualOverflowRect();

      const bool isInlineReversed = mTextRun->IsInlineReversed();
      if (verticalDec) {
        clipRect.x = aParams.framePt.x + visualRect.x;
        clipRect.y = isInlineReversed ? aTextBaselinePt.y - clipLength
                                      : aTextBaselinePt.y;
        clipRect.width = visualRect.width;
        clipRect.height = clipLength;
      } else {
        clipRect.x = isInlineReversed ? aTextBaselinePt.x - clipLength
                                      : aTextBaselinePt.x;
        clipRect.y = aParams.framePt.y + visualRect.y;
        clipRect.width = clipLength;
        clipRect.height = visualRect.height;
      }

      clipRect.Scale(1 / app);
      clipRect.Round();
      params.context->Clip(clipRect);
    }

    // Underlines
    params.decoration = NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE;
    for (const LineDecoration& dec : Reversed(aDecorations.mUnderlines)) {
      paintDecorationLine(dec, &Metrics::underlineSize,
                          &Metrics::underlineOffset);
    }

    // Overlines
    params.decoration = NS_STYLE_TEXT_DECORATION_LINE_OVERLINE;
    for (const LineDecoration& dec : Reversed(aDecorations.mOverlines)) {
      paintDecorationLine(dec, &Metrics::underlineSize, &Metrics::maxAscent);
    }

    // Some glyphs and emphasis marks may extend outside the region, so we reset
    // the clip region here. For an example, italic glyphs.
    if (!skipClipping) {
      params.context->PopClip();
    }

    {
      gfxContextMatrixAutoSaveRestore unscaledRestorer;
      if (scaledRestorer.HasMatrix()) {
        unscaledRestorer.SetContext(aParams.context);
        aParams.context->SetMatrix(scaledRestorer.Matrix());
      }

      // CSS 2.1 mandates that text be painted after over/underlines,
      // and *then* line-throughs
      DrawTextRun(aRange, aTextBaselinePt, aParams);
    }

    // Emphasis marks
    DrawEmphasisMarks(aParams.context, wm,
                      aTextBaselinePt, aParams.framePt, aRange,
                      aParams.decorationOverrideColor, aParams.provider);

    // Re-apply the clip region when the line-through is being drawn.
    if (!skipClipping) {
      params.context->Clip(clipRect);
    }

    // Line-throughs
    params.decoration = NS_STYLE_TEXT_DECORATION_LINE_LINE_THROUGH;
    for (const LineDecoration& dec : Reversed(aDecorations.mStrikes)) {
      paintDecorationLine(dec, &Metrics::strikeoutSize,
                          &Metrics::strikeoutOffset);
    }

    if (!skipClipping) {
      params.context->PopClip();
    }
}

void
nsTextFrame::DrawText(Range aRange, const gfx::Point& aTextBaselinePt,
                      const DrawTextParams& aParams)
{
  TextDecorations decorations;
  GetTextDecorations(aParams.textStyle->PresContext(),
                     aParams.callbacks ? eUnresolvedColors : eResolvedColors,
                     decorations);

  // Hide text decorations if we're currently hiding @font-face fallback text
  const bool drawDecorations =
    !aParams.provider->GetFontGroup()->ShouldSkipDrawing() &&
    (decorations.HasDecorationLines() || StyleText()->HasTextEmphasis());
  if (drawDecorations) {
    DrawTextRunAndDecorations(aRange, aTextBaselinePt, aParams, decorations);
  } else {
    DrawTextRun(aRange, aTextBaselinePt, aParams);
  }

  if (auto* textDrawer = aParams.context->GetTextDrawer()) {
    textDrawer->TerminateShadows();
  }
}

int16_t
nsTextFrame::GetSelectionStatus(int16_t* aSelectionFlags)
{
  // get the selection controller
  nsCOMPtr<nsISelectionController> selectionController;
  nsresult rv = GetSelectionController(PresContext(),
                                       getter_AddRefs(selectionController));
  if (NS_FAILED(rv) || !selectionController)
    return nsISelectionController::SELECTION_OFF;

  selectionController->GetSelectionFlags(aSelectionFlags);

  int16_t selectionValue;
  selectionController->GetDisplaySelection(&selectionValue);

  return selectionValue;
}

bool
nsTextFrame::IsVisibleInSelection(nsISelection* aSelection)
{
  // Check the quick way first
  if (!GetContent()->IsSelectionDescendant())
    return false;

  UniquePtr<SelectionDetails> details = GetSelectionDetails();
  bool found = false;

  // where are the selection points "really"
  for (SelectionDetails* sdptr = details.get(); sdptr; sdptr = sdptr->mNext.get()) {
    if (sdptr->mEnd > GetContentOffset() &&
        sdptr->mStart < GetContentEnd() &&
        sdptr->mSelectionType == SelectionType::eNormal) {
      found = true;
      break;
    }
  }

  return found;
}

/**
 * Compute the longest prefix of text whose width is <= aWidth. Return
 * the length of the prefix. Also returns the width of the prefix in aFitWidth.
 */
static uint32_t
CountCharsFit(const gfxTextRun* aTextRun, gfxTextRun::Range aRange,
              gfxFloat aWidth, PropertyProvider* aProvider,
              gfxFloat* aFitWidth)
{
  uint32_t last = 0;
  gfxFloat width = 0;
  for (uint32_t i = 1; i <= aRange.Length(); ++i) {
    if (i == aRange.Length() || aTextRun->IsClusterStart(aRange.start + i)) {
      gfxTextRun::Range range(aRange.start + last, aRange.start + i);
      gfxFloat nextWidth = width + aTextRun->GetAdvanceWidth(range, aProvider);
      if (nextWidth > aWidth)
        break;
      last = i;
      width = nextWidth;
    }
  }
  *aFitWidth = width;
  return last;
}

nsIFrame::ContentOffsets
nsTextFrame::CalcContentOffsetsFromFramePoint(const nsPoint& aPoint)
{
  return GetCharacterOffsetAtFramePointInternal(aPoint, true);
}

nsIFrame::ContentOffsets
nsTextFrame::GetCharacterOffsetAtFramePoint(const nsPoint &aPoint)
{
  return GetCharacterOffsetAtFramePointInternal(aPoint, false);
}

nsIFrame::ContentOffsets
nsTextFrame::GetCharacterOffsetAtFramePointInternal(const nsPoint& aPoint,
                                                    bool aForInsertionPoint)
{
  ContentOffsets offsets;

  gfxSkipCharsIterator iter = EnsureTextRun(nsTextFrame::eInflated);
  if (!mTextRun)
    return offsets;

  PropertyProvider provider(this, iter, nsTextFrame::eInflated);
  // Trim leading but not trailing whitespace if possible
  provider.InitializeForDisplay(false);
  gfxFloat width = mTextRun->IsVertical()
    ? (mTextRun->IsInlineReversed() ? mRect.height - aPoint.y : aPoint.y)
    : (mTextRun->IsInlineReversed() ? mRect.width - aPoint.x : aPoint.x);
  if (StyleContext()->IsTextCombined()) {
    width /= GetTextCombineScaleFactor(this);
  }
  gfxFloat fitWidth;
  Range skippedRange = ComputeTransformedRange(provider);

  uint32_t charsFit = CountCharsFit(mTextRun, skippedRange,
                                    width, &provider, &fitWidth);

  int32_t selectedOffset;
  if (charsFit < skippedRange.Length()) {
    // charsFit characters fitted, but no more could fit. See if we're
    // more than halfway through the cluster.. If we are, choose the next
    // cluster.
    gfxSkipCharsIterator extraCluster(provider.GetStart());
    extraCluster.AdvanceSkipped(charsFit);

    bool allowSplitLigature = true; // Allow selection of partial ligature...

    // ...but don't let selection/insertion-point split two Regional Indicator
    // chars that are ligated in the textrun to form a single flag symbol.
    uint32_t offs = extraCluster.GetOriginalOffset();
    const nsTextFragment* frag = GetContent()->GetText();
    if (offs + 1 < frag->GetLength() &&
        NS_IS_HIGH_SURROGATE(frag->CharAt(offs)) &&
        NS_IS_LOW_SURROGATE(frag->CharAt(offs + 1)) &&
        gfxFontUtils::IsRegionalIndicator
          (SURROGATE_TO_UCS4(frag->CharAt(offs), frag->CharAt(offs + 1)))) {
      allowSplitLigature = false;
      if (extraCluster.GetSkippedOffset() > 1 &&
          !mTextRun->IsLigatureGroupStart(extraCluster.GetSkippedOffset())) {
        // CountCharsFit() left us in the middle of the flag; back up over the
        // first character of the ligature, and adjust fitWidth accordingly.
        extraCluster.AdvanceSkipped(-2); // it's a surrogate pair: 2 code units
        fitWidth -= mTextRun->GetAdvanceWidth(
          Range(extraCluster.GetSkippedOffset(),
                extraCluster.GetSkippedOffset() + 2), &provider);
      }
    }

    gfxSkipCharsIterator extraClusterLastChar(extraCluster);
    FindClusterEnd(mTextRun,
                   provider.GetStart().GetOriginalOffset() + provider.GetOriginalLength(),
                   &extraClusterLastChar, allowSplitLigature);
    PropertyProvider::Spacing spacing;
    Range extraClusterRange(extraCluster.GetSkippedOffset(),
                            extraClusterLastChar.GetSkippedOffset() + 1);
    gfxFloat charWidth =
        mTextRun->GetAdvanceWidth(extraClusterRange, &provider, &spacing);
    charWidth -= spacing.mBefore + spacing.mAfter;
    selectedOffset = !aForInsertionPoint ||
      width <= fitWidth + spacing.mBefore + charWidth/2
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
    // If we're at the end of a preformatted line which has a terminating
    // linefeed, we want to reduce the offset by one to make sure that the
    // selection is placed before the linefeed character.
    if (HasSignificantTerminalNewline()) {
      --selectedOffset;
    }
  }

  offsets.content = GetContent();
  offsets.offset = offsets.secondaryOffset = selectedOffset;
  offsets.associate =
    mContentOffset == offsets.offset ? CARET_ASSOCIATE_AFTER : CARET_ASSOCIATE_BEFORE;
  return offsets;
}

bool
nsTextFrame::CombineSelectionUnderlineRect(nsPresContext* aPresContext,
                                           nsRect& aRect)
{
  if (aRect.IsEmpty())
    return false;

  nsRect givenRect = aRect;

  RefPtr<nsFontMetrics> fm =
    nsLayoutUtils::GetFontMetricsForFrame(this, GetFontSizeInflation());
  gfxFontGroup* fontGroup = fm->GetThebesFontGroup();
  gfxFont* firstFont = fontGroup->GetFirstValidFont();
  WritingMode wm = GetWritingMode();
  bool verticalRun = wm.IsVertical();
  bool useVerticalMetrics = verticalRun && !wm.IsSideways();
  const gfxFont::Metrics& metrics =
    firstFont->GetMetrics(useVerticalMetrics ? gfxFont::eVertical
                                             : gfxFont::eHorizontal);

  nsCSSRendering::DecorationRectParams params;
  params.ascent = aPresContext->AppUnitsToGfxUnits(mAscent);
  params.offset = fontGroup->GetUnderlineOffset();
  params.descentLimit =
    ComputeDescentLimitForSelectionUnderline(aPresContext, metrics);
  params.vertical = verticalRun;

  EnsureTextRun(nsTextFrame::eInflated);
  params.sidewaysLeft = mTextRun ? mTextRun->IsSidewaysLeft() : false;

  UniquePtr<SelectionDetails> details = GetSelectionDetails();
  for (SelectionDetails* sd = details.get(); sd; sd = sd->mNext.get()) {
    if (sd->mStart == sd->mEnd ||
        sd->mSelectionType == SelectionType::eInvalid ||
        !(ToSelectionTypeMask(sd->mSelectionType) &
            kSelectionTypesWithDecorations) ||
        // URL strikeout does not use underline.
        sd->mSelectionType == SelectionType::eURLStrikeout) {
      continue;
    }

    float relativeSize;
    int32_t index =
      nsTextPaintStyle::GetUnderlineStyleIndexForSelectionType(
        sd->mSelectionType);
    if (sd->mSelectionType == SelectionType::eSpellCheck) {
      if (!nsTextPaintStyle::GetSelectionUnderline(aPresContext, index, nullptr,
                                                   &relativeSize,
                                                   &params.style)) {
        continue;
      }
    } else {
      // IME selections
      TextRangeStyle& rangeStyle = sd->mTextRangeStyle;
      if (rangeStyle.IsDefined()) {
        if (!rangeStyle.IsLineStyleDefined() ||
            rangeStyle.mLineStyle == TextRangeStyle::LINESTYLE_NONE) {
          continue;
        }
        params.style = rangeStyle.mLineStyle;
        relativeSize = rangeStyle.mIsBoldLine ? 2.0f : 1.0f;
      } else if (!nsTextPaintStyle::GetSelectionUnderline(aPresContext, index,
                                                          nullptr, &relativeSize,
                                                          &params.style)) {
        continue;
      }
    }
    nsRect decorationArea;

    params.lineSize =
      Size(aPresContext->AppUnitsToGfxUnits(aRect.width),
           ComputeSelectionUnderlineHeight(aPresContext, metrics,
                                           sd->mSelectionType));
    relativeSize = std::max(relativeSize, 1.0f);
    params.lineSize.height *= relativeSize;
    decorationArea =
      nsCSSRendering::GetTextDecorationRect(aPresContext, params);
    aRect.UnionRect(aRect, decorationArea);
  }

  return !aRect.IsEmpty() && !givenRect.Contains(aRect);
}

bool
nsTextFrame::IsFrameSelected() const
{
  NS_ASSERTION(!GetContent() || GetContent()->IsSelectionDescendant(),
               "use the public IsSelected() instead");
  return nsRange::IsNodeSelected(GetContent(), GetContentOffset(),
                                 GetContentEnd());
}

void
nsTextFrame::SetSelectedRange(uint32_t aStart, uint32_t aEnd, bool aSelected,
                              SelectionType aSelectionType)
{
  NS_ASSERTION(!GetPrevContinuation(), "Should only be called for primary frame");
  DEBUG_VERIFY_NOT_DIRTY(mState);

  // Selection is collapsed, which can't affect text frame rendering
  if (aStart == aEnd)
    return;

  nsTextFrame* f = this;
  while (f && f->GetContentEnd() <= int32_t(aStart)) {
    f = f->GetNextContinuation();
  }

  nsPresContext* presContext = PresContext();
  while (f && f->GetContentOffset() < int32_t(aEnd)) {
    // We may need to reflow to recompute the overflow area for
    // spellchecking or IME underline if their underline is thicker than
    // the normal decoration line.
    if (ToSelectionTypeMask(aSelectionType) & kSelectionTypesWithDecorations) {
      bool didHaveOverflowingSelection =
        (f->GetStateBits() & TEXT_SELECTION_UNDERLINE_OVERFLOWED) != 0;
      nsRect r(nsPoint(0, 0), GetSize());
      bool willHaveOverflowingSelection =
        aSelected && f->CombineSelectionUnderlineRect(presContext, r);
      if (didHaveOverflowingSelection || willHaveOverflowingSelection) {
        presContext->PresShell()->FrameNeedsReflow(f,
                                                   nsIPresShell::eStyleChange,
                                                   NS_FRAME_IS_DIRTY);
      }
    }
    // Selection might change anything. Invalidate the overflow area.
    f->InvalidateFrame();

    f = f->GetNextContinuation();
  }
}

void
nsTextFrame::UpdateIteratorFromOffset(const PropertyProvider& aProperties,
                                      int32_t& aInOffset,
                                      gfxSkipCharsIterator& aIter)
{
  if (aInOffset < GetContentOffset()){
    NS_WARNING("offset before this frame's content");
    aInOffset = GetContentOffset();
  } else if (aInOffset > GetContentEnd()) {
    NS_WARNING("offset after this frame's content");
    aInOffset = GetContentEnd();
  }

  int32_t trimmedOffset = aProperties.GetStart().GetOriginalOffset();
  int32_t trimmedEnd = trimmedOffset + aProperties.GetOriginalLength();
  aInOffset = std::max(aInOffset, trimmedOffset);
  aInOffset = std::min(aInOffset, trimmedEnd);

  aIter.SetOriginalOffset(aInOffset);

  if (aInOffset < trimmedEnd &&
      !aIter.IsOriginalCharSkipped() &&
      !mTextRun->IsClusterStart(aIter.GetSkippedOffset())) {
    NS_WARNING("called for non-cluster boundary");
    FindClusterStart(mTextRun, trimmedOffset, &aIter);
  }
}

nsPoint
nsTextFrame::GetPointFromIterator(const gfxSkipCharsIterator& aIter,
                                  PropertyProvider& aProperties)
{
  Range range(aProperties.GetStart().GetSkippedOffset(),
              aIter.GetSkippedOffset());
  gfxFloat advance = mTextRun->GetAdvanceWidth(range, &aProperties);
  nscoord iSize = NSToCoordCeilClamped(advance);
  nsPoint point;

  if (mTextRun->IsVertical()) {
    point.x = 0;
    if (mTextRun->IsInlineReversed()) {
      point.y = mRect.height - iSize;
    } else {
      point.y = iSize;
    }
  } else {
    point.y = 0;
    if (mTextRun->IsInlineReversed()) {
      point.x = mRect.width - iSize;
    } else {
      point.x = iSize;
    }
    if (StyleContext()->IsTextCombined()) {
      point.x *= GetTextCombineScaleFactor(this);
    }
  }
  return point;
}

nsresult
nsTextFrame::GetPointFromOffset(int32_t inOffset,
                                nsPoint* outPoint)
{
  if (!outPoint)
    return NS_ERROR_NULL_POINTER;

  DEBUG_VERIFY_NOT_DIRTY(mState);
  if (mState & NS_FRAME_IS_DIRTY)
    return NS_ERROR_UNEXPECTED;

  if (GetContentLength() <= 0) {
    outPoint->x = 0;
    outPoint->y = 0;
    return NS_OK;
  }

  gfxSkipCharsIterator iter = EnsureTextRun(nsTextFrame::eInflated);
  if (!mTextRun)
    return NS_ERROR_FAILURE;

  PropertyProvider properties(this, iter, nsTextFrame::eInflated);
  // Don't trim trailing whitespace, we want the caret to appear in the right
  // place if it's positioned there
  properties.InitializeForDisplay(false);

  UpdateIteratorFromOffset(properties, inOffset, iter);

  *outPoint = GetPointFromIterator(iter, properties);

  return NS_OK;
}

nsresult
nsTextFrame::GetCharacterRectsInRange(int32_t aInOffset,
                                      int32_t aLength,
                                      nsTArray<nsRect>& aRects)
{
  DEBUG_VERIFY_NOT_DIRTY(mState);
  if (mState & NS_FRAME_IS_DIRTY) {
    return NS_ERROR_UNEXPECTED;
  }

  if (GetContentLength() <= 0) {
    return NS_OK;
  }

  if (!mTextRun) {
    return NS_ERROR_FAILURE;
  }

  gfxSkipCharsIterator iter = EnsureTextRun(nsTextFrame::eInflated);
  PropertyProvider properties(this, iter, nsTextFrame::eInflated);
  // Don't trim trailing whitespace, we want the caret to appear in the right
  // place if it's positioned there
  properties.InitializeForDisplay(false);

  UpdateIteratorFromOffset(properties, aInOffset, iter);

  const int32_t kContentEnd = GetContentEnd();
  const int32_t kEndOffset = std::min(aInOffset + aLength, kContentEnd);
  while (aInOffset < kEndOffset) {
    if (!iter.IsOriginalCharSkipped() &&
        !mTextRun->IsClusterStart(iter.GetSkippedOffset())) {
      FindClusterStart(mTextRun,
                       properties.GetStart().GetOriginalOffset() +
                         properties.GetOriginalLength(),
                       &iter);
    }

    nsPoint point = GetPointFromIterator(iter, properties);
    nsRect rect;
    rect.x = point.x;
    rect.y = point.y;

    nscoord iSize = 0;
    if (aInOffset < kContentEnd) {
      gfxSkipCharsIterator nextIter(iter);
      nextIter.AdvanceOriginal(1);
      if (!nextIter.IsOriginalCharSkipped() &&
          !mTextRun->IsClusterStart(nextIter.GetSkippedOffset())) {
        FindClusterEnd(mTextRun, kContentEnd, &nextIter);
      }

      gfxFloat advance =
        mTextRun->GetAdvanceWidth(Range(iter.GetSkippedOffset(),
                                        nextIter.GetSkippedOffset()),
                                  &properties);
      iSize = NSToCoordCeilClamped(advance);
    }

    if (mTextRun->IsVertical()) {
      rect.width = mRect.width;
      rect.height = iSize;
    } else {
      rect.width = iSize;
      rect.height = mRect.height;

      if (StyleContext()->IsTextCombined()) {
        rect.width *= GetTextCombineScaleFactor(this);
      }
    }
    aRects.AppendElement(rect);
    aInOffset++;
    // Don't advance iter if we've reached the end
    if (aInOffset < kEndOffset) {
      iter.AdvanceOriginal(1);
    }
  }

  return NS_OK;
}

nsresult
nsTextFrame::GetChildFrameContainingOffset(int32_t   aContentOffset,
                                           bool      aHint,
                                           int32_t*  aOutOffset,
                                           nsIFrame**aOutFrame)
{
  DEBUG_VERIFY_NOT_DIRTY(mState);
#if 0 //XXXrbs disable due to bug 310227
  if (mState & NS_FRAME_IS_DIRTY)
    return NS_ERROR_UNEXPECTED;
#endif

  NS_ASSERTION(aOutOffset && aOutFrame, "Bad out parameters");
  NS_ASSERTION(aContentOffset >= 0, "Negative content offset, existing code was very broken!");
  nsIFrame* primaryFrame = mContent->GetPrimaryFrame();
  if (this != primaryFrame) {
    // This call needs to happen on the primary frame
    return primaryFrame->GetChildFrameContainingOffset(aContentOffset, aHint,
                                                       aOutOffset, aOutFrame);
  }

  nsTextFrame* f = this;
  int32_t offset = mContentOffset;

  // Try to look up the offset to frame property
  nsTextFrame* cachedFrame = GetProperty(OffsetToFrameProperty());

  if (cachedFrame) {
    f = cachedFrame;
    offset = f->GetContentOffset();

    f->RemoveStateBits(TEXT_IN_OFFSET_CACHE);
  }

  if ((aContentOffset >= offset) &&
      (aHint || aContentOffset != offset)) {
    while (true) {
      nsTextFrame* next = f->GetNextContinuation();
      if (!next || aContentOffset < next->GetContentOffset())
        break;
      if (aContentOffset == next->GetContentOffset()) {
        if (aHint) {
          f = next;
          if (f->GetContentLength() == 0) {
            continue; // use the last of the empty frames with this offset
          }
        }
        break;
      }
      f = next;
    }
  } else {
    while (true) {
      nsTextFrame* prev = f->GetPrevContinuation();
      if (!prev || aContentOffset > f->GetContentOffset())
        break;
      if (aContentOffset == f->GetContentOffset()) {
        if (!aHint) {
          f = prev;
          if (f->GetContentLength() == 0) {
            continue; // use the first of the empty frames with this offset
          }
        }
        break;
      }
      f = prev;
    }
  }

  *aOutOffset = aContentOffset - f->GetContentOffset();
  *aOutFrame = f;

  // cache the frame we found
  SetProperty(OffsetToFrameProperty(), f);
  f->AddStateBits(TEXT_IN_OFFSET_CACHE);

  return NS_OK;
}

nsIFrame::FrameSearchResult
nsTextFrame::PeekOffsetNoAmount(bool aForward, int32_t* aOffset)
{
  NS_ASSERTION(aOffset && *aOffset <= GetContentLength(), "aOffset out of range");

  gfxSkipCharsIterator iter = EnsureTextRun(nsTextFrame::eInflated);
  if (!mTextRun)
    return CONTINUE_EMPTY;

  TrimmedOffsets trimmed = GetTrimmedOffsets(mContent->GetText(), true);
  // Check whether there are nonskipped characters in the trimmmed range
  return (iter.ConvertOriginalToSkipped(trimmed.GetEnd()) >
         iter.ConvertOriginalToSkipped(trimmed.mStart)) ? FOUND : CONTINUE;
}

/**
 * This class iterates through the clusters before or after the given
 * aPosition (which is a content offset). You can test each cluster
 * to see if it's whitespace (as far as selection/caret movement is concerned),
 * or punctuation, or if there is a word break before the cluster. ("Before"
 * is interpreted according to aDirection, so if aDirection is -1, "before"
 * means actually *after* the cluster content.)
 */
class MOZ_STACK_CLASS ClusterIterator {
public:
  ClusterIterator(nsTextFrame* aTextFrame, int32_t aPosition, int32_t aDirection,
                  nsString& aContext);

  bool NextCluster();
  bool IsWhitespace();
  bool IsPunctuation();
  bool HaveWordBreakBefore() { return mHaveWordBreak; }
  int32_t GetAfterOffset();
  int32_t GetBeforeOffset();

private:
  gfxSkipCharsIterator        mIterator;
  const nsTextFragment*       mFrag;
  nsTextFrame*                mTextFrame;
  int32_t                     mDirection;
  int32_t                     mCharIndex;
  nsTextFrame::TrimmedOffsets mTrimmed;
  nsTArray<bool>      mWordBreaks;
  bool                        mHaveWordBreak;
};

static bool
IsAcceptableCaretPosition(const gfxSkipCharsIterator& aIter,
                          bool aRespectClusters,
                          const gfxTextRun* aTextRun,
                          nsIFrame* aFrame)
{
  if (aIter.IsOriginalCharSkipped())
    return false;
  uint32_t index = aIter.GetSkippedOffset();
  if (aRespectClusters && !aTextRun->IsClusterStart(index))
    return false;
  if (index > 0) {
    // Check whether the proposed position is in between the two halves of a
    // surrogate pair, or before a Variation Selector character;
    // if so, this is not a valid character boundary.
    // (In the case where we are respecting clusters, we won't actually get
    // this far because the low surrogate is also marked as non-clusterStart
    // so we'll return FALSE above.)
    uint32_t offs = aIter.GetOriginalOffset();
    const nsTextFragment* frag = aFrame->GetContent()->GetText();
    uint32_t ch = frag->CharAt(offs);

    if (gfxFontUtils::IsVarSelector(ch) ||
        (NS_IS_LOW_SURROGATE(ch) && offs > 0 &&
         NS_IS_HIGH_SURROGATE(frag->CharAt(offs - 1)))) {
      return false;
    }

    // If the proposed position is before a high surrogate, we need to decode
    // the surrogate pair (if valid) and check the resulting character.
    if (NS_IS_HIGH_SURROGATE(ch) && offs + 1 < frag->GetLength()) {
      uint32_t ch2 = frag->CharAt(offs + 1);
      if (NS_IS_LOW_SURROGATE(ch2)) {
        ch = SURROGATE_TO_UCS4(ch, ch2);
        // If the character is a (Plane-14) variation selector,
        // or a Regional Indicator character that is ligated with the previous
        // character, this is not a valid boundary.
        if (gfxFontUtils::IsVarSelector(ch) ||
            (gfxFontUtils::IsRegionalIndicator(ch) &&
             !aTextRun->IsLigatureGroupStart(index))) {
          return false;
        }
      }
    }
  }
  return true;
}

nsIFrame::FrameSearchResult
nsTextFrame::PeekOffsetCharacter(bool aForward, int32_t* aOffset,
                                 PeekOffsetCharacterOptions aOptions)
{
  int32_t contentLength = GetContentLength();
  NS_ASSERTION(aOffset && *aOffset <= contentLength, "aOffset out of range");

  if (!aOptions.mIgnoreUserStyleAll) {
    StyleUserSelect selectStyle;
    IsSelectable(&selectStyle);
    if (selectStyle == StyleUserSelect::All) {
      return CONTINUE_UNSELECTABLE;
    }
  }

  gfxSkipCharsIterator iter = EnsureTextRun(nsTextFrame::eInflated);
  if (!mTextRun)
    return CONTINUE_EMPTY;

  TrimmedOffsets trimmed = GetTrimmedOffsets(mContent->GetText(), false);

  // A negative offset means "end of frame".
  int32_t startOffset = GetContentOffset() + (*aOffset < 0 ? contentLength : *aOffset);

  if (!aForward) {
    // If at the beginning of the line, look at the previous continuation
    for (int32_t i = std::min(trimmed.GetEnd(), startOffset) - 1;
         i >= trimmed.mStart; --i) {
      iter.SetOriginalOffset(i);
      if (IsAcceptableCaretPosition(iter, aOptions.mRespectClusters, mTextRun,
                                    this)) {
        *aOffset = i - mContentOffset;
        return FOUND;
      }
    }
    *aOffset = 0;
  } else {
    // If we're at the end of a line, look at the next continuation
    iter.SetOriginalOffset(startOffset);
    if (startOffset <= trimmed.GetEnd() &&
        !(startOffset < trimmed.GetEnd() &&
          StyleText()->NewlineIsSignificant(this) &&
          iter.GetSkippedOffset() < mTextRun->GetLength() &&
          mTextRun->CharIsNewline(iter.GetSkippedOffset()))) {
      for (int32_t i = startOffset + 1; i <= trimmed.GetEnd(); ++i) {
        iter.SetOriginalOffset(i);
        if (i == trimmed.GetEnd() ||
            IsAcceptableCaretPosition(iter, aOptions.mRespectClusters, mTextRun,
                                      this)) {
          *aOffset = i - mContentOffset;
          return FOUND;
        }
      }
    }
    *aOffset = contentLength;
  }

  return CONTINUE;
}

bool
ClusterIterator::IsWhitespace()
{
  NS_ASSERTION(mCharIndex >= 0, "No cluster selected");
  return IsSelectionSpace(mFrag, mCharIndex);
}

bool
ClusterIterator::IsPunctuation()
{
  NS_ASSERTION(mCharIndex >= 0, "No cluster selected");
  // Return true for all Punctuation categories (Unicode general category P?),
  // and also for Symbol categories (S?) except for Modifier Symbol, which is
  // kept together with any adjacent letter/number. (Bug 1066756)
  uint8_t cat = unicode::GetGeneralCategory(mFrag->CharAt(mCharIndex));
  switch (cat) {
    case HB_UNICODE_GENERAL_CATEGORY_CONNECT_PUNCTUATION: /* Pc */
    case HB_UNICODE_GENERAL_CATEGORY_DASH_PUNCTUATION:    /* Pd */
    case HB_UNICODE_GENERAL_CATEGORY_CLOSE_PUNCTUATION:   /* Pe */
    case HB_UNICODE_GENERAL_CATEGORY_FINAL_PUNCTUATION:   /* Pf */
    case HB_UNICODE_GENERAL_CATEGORY_INITIAL_PUNCTUATION: /* Pi */
    case HB_UNICODE_GENERAL_CATEGORY_OTHER_PUNCTUATION:   /* Po */
    case HB_UNICODE_GENERAL_CATEGORY_OPEN_PUNCTUATION:    /* Ps */
    case HB_UNICODE_GENERAL_CATEGORY_CURRENCY_SYMBOL:     /* Sc */
    // Deliberately omitted:
    // case HB_UNICODE_GENERAL_CATEGORY_MODIFIER_SYMBOL:     /* Sk */
    case HB_UNICODE_GENERAL_CATEGORY_MATH_SYMBOL:         /* Sm */
    case HB_UNICODE_GENERAL_CATEGORY_OTHER_SYMBOL:        /* So */
      return true;
    default:
      return false;
  }
}

int32_t
ClusterIterator::GetBeforeOffset()
{
  NS_ASSERTION(mCharIndex >= 0, "No cluster selected");
  return mCharIndex + (mDirection > 0 ? 0 : 1);
}

int32_t
ClusterIterator::GetAfterOffset()
{
  NS_ASSERTION(mCharIndex >= 0, "No cluster selected");
  return mCharIndex + (mDirection > 0 ? 1 : 0);
}

bool
ClusterIterator::NextCluster()
{
  if (!mDirection)
    return false;
  const gfxTextRun* textRun = mTextFrame->GetTextRun(nsTextFrame::eInflated);

  mHaveWordBreak = false;
  while (true) {
    bool keepGoing = false;
    if (mDirection > 0) {
      if (mIterator.GetOriginalOffset() >= mTrimmed.GetEnd())
        return false;
      keepGoing = mIterator.IsOriginalCharSkipped() ||
          mIterator.GetOriginalOffset() < mTrimmed.mStart ||
          !textRun->IsClusterStart(mIterator.GetSkippedOffset());
      mCharIndex = mIterator.GetOriginalOffset();
      mIterator.AdvanceOriginal(1);
    } else {
      if (mIterator.GetOriginalOffset() <= mTrimmed.mStart)
        return false;
      mIterator.AdvanceOriginal(-1);
      keepGoing = mIterator.IsOriginalCharSkipped() ||
          mIterator.GetOriginalOffset() >= mTrimmed.GetEnd() ||
          !textRun->IsClusterStart(mIterator.GetSkippedOffset());
      mCharIndex = mIterator.GetOriginalOffset();
    }

    if (mWordBreaks[GetBeforeOffset() - mTextFrame->GetContentOffset()]) {
      mHaveWordBreak = true;
    }
    if (!keepGoing)
      return true;
  }
}

ClusterIterator::ClusterIterator(nsTextFrame* aTextFrame, int32_t aPosition,
                                 int32_t aDirection, nsString& aContext)
  : mTextFrame(aTextFrame), mDirection(aDirection), mCharIndex(-1)
{
  mIterator = aTextFrame->EnsureTextRun(nsTextFrame::eInflated);
  if (!aTextFrame->GetTextRun(nsTextFrame::eInflated)) {
    mDirection = 0; // signal failure
    return;
  }
  mIterator.SetOriginalOffset(aPosition);

  mFrag = aTextFrame->GetContent()->GetText();
  mTrimmed = aTextFrame->GetTrimmedOffsets(mFrag, true);

  int32_t textOffset = aTextFrame->GetContentOffset();
  int32_t textLen = aTextFrame->GetContentLength();
  if (!mWordBreaks.AppendElements(textLen + 1)) {
    mDirection = 0; // signal failure
    return;
  }
  memset(mWordBreaks.Elements(), false, (textLen + 1)*sizeof(bool));
  int32_t textStart;
  if (aDirection > 0) {
    if (aContext.IsEmpty()) {
      // No previous context, so it must be the start of a line or text run
      mWordBreaks[0] = true;
    }
    textStart = aContext.Length();
    mFrag->AppendTo(aContext, textOffset, textLen);
  } else {
    if (aContext.IsEmpty()) {
      // No following context, so it must be the end of a line or text run
      mWordBreaks[textLen] = true;
    }
    textStart = 0;
    nsAutoString str;
    mFrag->AppendTo(str, textOffset, textLen);
    aContext.Insert(str, 0);
  }
  mozilla::intl::WordBreaker* wordBreaker = nsContentUtils::WordBreaker();
  for (int32_t i = 0; i <= textLen; ++i) {
    int32_t indexInText = i + textStart;
    mWordBreaks[i] |=
      wordBreaker->BreakInBetween(aContext.get(), indexInText,
                                  aContext.get() + indexInText,
                                  aContext.Length() - indexInText);
  }
}

nsIFrame::FrameSearchResult
nsTextFrame::PeekOffsetWord(bool aForward, bool aWordSelectEatSpace, bool aIsKeyboardSelect,
                            int32_t* aOffset, PeekWordState* aState)
{
  int32_t contentLength = GetContentLength();
  NS_ASSERTION (aOffset && *aOffset <= contentLength, "aOffset out of range");

  StyleUserSelect selectStyle;
  IsSelectable(&selectStyle);
  if (selectStyle == StyleUserSelect::All)
    return CONTINUE_UNSELECTABLE;

  int32_t offset = GetContentOffset() + (*aOffset < 0 ? contentLength : *aOffset);
  ClusterIterator cIter(this, offset, aForward ? 1 : -1, aState->mContext);

  if (!cIter.NextCluster())
    return CONTINUE_EMPTY;

  do {
    bool isPunctuation = cIter.IsPunctuation();
    bool isWhitespace = cIter.IsWhitespace();
    bool isWordBreakBefore = cIter.HaveWordBreakBefore();
    if (aWordSelectEatSpace == isWhitespace && !aState->mSawBeforeType) {
      aState->SetSawBeforeType();
      aState->Update(isPunctuation, isWhitespace);
      continue;
    }
    // See if we can break before the current cluster
    if (!aState->mAtStart) {
      bool canBreak;
      if (isPunctuation != aState->mLastCharWasPunctuation) {
        canBreak = BreakWordBetweenPunctuation(aState, aForward,
                     isPunctuation, isWhitespace, aIsKeyboardSelect);
      } else if (!aState->mLastCharWasWhitespace &&
                 !isWhitespace && !isPunctuation && isWordBreakBefore) {
        // if both the previous and the current character are not white
        // space but this can be word break before, we don't need to eat
        // a white space in this case. This case happens in some languages
        // that their words are not separated by white spaces. E.g.,
        // Japanese and Chinese.
        canBreak = true;
      } else {
        canBreak = isWordBreakBefore && aState->mSawBeforeType &&
          (aWordSelectEatSpace != isWhitespace);
      }
      if (canBreak) {
        *aOffset = cIter.GetBeforeOffset() - mContentOffset;
        return FOUND;
      }
    }
    aState->Update(isPunctuation, isWhitespace);
  } while (cIter.NextCluster());

  *aOffset = cIter.GetAfterOffset() - mContentOffset;
  return CONTINUE;
}

 // TODO this needs to be deCOMtaminated with the interface fixed in
// nsIFrame.h, but we won't do that until the old textframe is gone.
nsresult
nsTextFrame::CheckVisibility(nsPresContext* aContext, int32_t aStartIndex,
    int32_t aEndIndex, bool aRecurse, bool *aFinished, bool *aRetval)
{
  if (!aRetval)
    return NS_ERROR_NULL_POINTER;

  // Text in the range is visible if there is at least one character in the range
  // that is not skipped and is mapped by this frame (which is the primary frame)
  // or one of its continuations.
  for (nsTextFrame* f = this; f; f = f->GetNextContinuation()) {
    int32_t dummyOffset = 0;
    if (f->PeekOffsetNoAmount(true, &dummyOffset) == FOUND) {
      *aRetval = true;
      return NS_OK;
    }
  }

  *aRetval = false;
  return NS_OK;
}

nsresult
nsTextFrame::GetOffsets(int32_t &start, int32_t &end) const
{
  start = GetContentOffset();
  end = GetContentEnd();
  return NS_OK;
}

static int32_t
FindEndOfPunctuationRun(const nsTextFragment* aFrag,
                        const gfxTextRun* aTextRun,
                        gfxSkipCharsIterator* aIter,
                        int32_t aOffset,
                        int32_t aStart,
                        int32_t aEnd)
{
  int32_t i;

  for (i = aStart; i < aEnd - aOffset; ++i) {
    if (nsContentUtils::IsFirstLetterPunctuationAt(aFrag, aOffset + i)) {
      aIter->SetOriginalOffset(aOffset + i);
      FindClusterEnd(aTextRun, aEnd, aIter);
      i = aIter->GetOriginalOffset() - aOffset;
    } else {
      break;
    }
  }
  return i;
}

/**
 * Returns true if this text frame completes the first-letter, false
 * if it does not contain a true "letter".
 * If returns true, then it also updates aLength to cover just the first-letter
 * text.
 *
 * XXX :first-letter should be handled during frame construction
 * (and it has a good bit in common with nextBidi)
 *
 * @param aLength an in/out parameter: on entry contains the maximum length to
 * return, on exit returns length of the first-letter fragment (which may
 * include leading and trailing punctuation, for example)
 */
static bool
FindFirstLetterRange(const nsTextFragment* aFrag,
                     const gfxTextRun* aTextRun,
                     int32_t aOffset, const gfxSkipCharsIterator& aIter,
                     int32_t* aLength)
{
  int32_t i;
  int32_t length = *aLength;
  int32_t endOffset = aOffset + length;
  gfxSkipCharsIterator iter(aIter);

  // skip leading whitespace, then consume clusters that start with punctuation
  i = FindEndOfPunctuationRun(aFrag, aTextRun, &iter, aOffset,
                              GetTrimmableWhitespaceCount(aFrag, aOffset, length, 1),
                              endOffset);
  if (i == length)
    return false;

  // If the next character is not a letter or number, there is no first-letter.
  // Return true so that we don't go on looking, but set aLength to 0.
  if (!nsContentUtils::IsAlphanumericAt(aFrag, aOffset + i)) {
    *aLength = 0;
    return true;
  }

  // consume another cluster (the actual first letter)

  // For complex scripts such as Indic and SEAsian, where first-letter
  // should extend to entire orthographic "syllable" clusters, we don't
  // want to allow this to split a ligature.
  bool allowSplitLigature;

  typedef unicode::Script Script;
  switch (unicode::GetScriptCode(aFrag->CharAt(aOffset + i))) {
    default:
      allowSplitLigature = true;
      break;

    // For now, lacking any definitive specification of when to apply this
    // behavior, we'll base the decision on the HarfBuzz shaping engine
    // used for each script: those that are handled by the Indic, Tibetan,
    // Myanmar and SEAsian shapers will apply the "don't split ligatures"
    // rule.

    // Indic
    case Script::BENGALI:
    case Script::DEVANAGARI:
    case Script::GUJARATI:
    case Script::GURMUKHI:
    case Script::KANNADA:
    case Script::MALAYALAM:
    case Script::ORIYA:
    case Script::TAMIL:
    case Script::TELUGU:
    case Script::SINHALA:
    case Script::BALINESE:
    case Script::LEPCHA:
    case Script::REJANG:
    case Script::SUNDANESE:
    case Script::JAVANESE:
    case Script::KAITHI:
    case Script::MEETEI_MAYEK:
    case Script::CHAKMA:
    case Script::SHARADA:
    case Script::TAKRI:
    case Script::KHMER:

    // Tibetan
    case Script::TIBETAN:

    // Myanmar
    case Script::MYANMAR:

    // Other SEAsian
    case Script::BUGINESE:
    case Script::NEW_TAI_LUE:
    case Script::CHAM:
    case Script::TAI_THAM:

    // What about Thai/Lao - any special handling needed?
    // Should we special-case Arabic lam-alef?

      allowSplitLigature = false;
      break;
  }

  iter.SetOriginalOffset(aOffset + i);
  FindClusterEnd(aTextRun, endOffset, &iter, allowSplitLigature);

  i = iter.GetOriginalOffset() - aOffset;
  if (i + 1 == length)
    return true;

  // consume clusters that start with punctuation
  i = FindEndOfPunctuationRun(aFrag, aTextRun, &iter, aOffset, i + 1, endOffset);
  if (i < length)
    *aLength = i;
  return true;
}

static uint32_t
FindStartAfterSkippingWhitespace(PropertyProvider* aProvider,
                                 nsIFrame::InlineIntrinsicISizeData* aData,
                                 const nsStyleText* aTextStyle,
                                 gfxSkipCharsIterator* aIterator,
                                 uint32_t aFlowEndInTextRun)
{
  if (aData->mSkipWhitespace) {
    while (aIterator->GetSkippedOffset() < aFlowEndInTextRun &&
           IsTrimmableSpace(aProvider->GetFragment(), aIterator->GetOriginalOffset(), aTextStyle)) {
      aIterator->AdvanceOriginal(1);
    }
  }
  return aIterator->GetSkippedOffset();
}

float
nsTextFrame::GetFontSizeInflation() const
{
  if (!HasFontSizeInflation()) {
    return 1.0f;
  }
  return GetProperty(FontSizeInflationProperty());
}

void
nsTextFrame::SetFontSizeInflation(float aInflation)
{
  if (aInflation == 1.0f) {
    if (HasFontSizeInflation()) {
      RemoveStateBits(TEXT_HAS_FONT_INFLATION);
      DeleteProperty(FontSizeInflationProperty());
    }
    return;
  }

  AddStateBits(TEXT_HAS_FONT_INFLATION);
  SetProperty(FontSizeInflationProperty(), aInflation);
}

/* virtual */
void nsTextFrame::MarkIntrinsicISizesDirty()
{
  ClearTextRuns();
  nsFrame::MarkIntrinsicISizesDirty();
}

// XXX this doesn't handle characters shaped by line endings. We need to
// temporarily override the "current line ending" settings.
void
nsTextFrame::AddInlineMinISizeForFlow(gfxContext *aRenderingContext,
                                      nsIFrame::InlineMinISizeData *aData,
                                      TextRunType aTextRunType)
{
  uint32_t flowEndInTextRun;
  gfxSkipCharsIterator iter =
    EnsureTextRun(aTextRunType, aRenderingContext->GetDrawTarget(),
                  aData->LineContainer(), aData->mLine, &flowEndInTextRun);
  gfxTextRun *textRun = GetTextRun(aTextRunType);
  if (!textRun)
    return;

  // Pass null for the line container. This will disable tab spacing, but that's
  // OK since we can't really handle tabs for intrinsic sizing anyway.
  const nsStyleText* textStyle = StyleText();
  const nsTextFragment* frag = mContent->GetText();

  // If we're hyphenating, the PropertyProvider needs the actual length;
  // otherwise we can just pass INT32_MAX to mean "all the text"
  int32_t len = INT32_MAX;
  bool hyphenating = frag->GetLength() > 0 &&
    (textStyle->mHyphens == StyleHyphens::Auto ||
     (textStyle->mHyphens == StyleHyphens::Manual &&
      !!(textRun->GetFlags() & gfx::ShapedTextFlags::TEXT_ENABLE_HYPHEN_BREAKS)));
  if (hyphenating) {
    gfxSkipCharsIterator tmp(iter);
    len = std::min<int32_t>(GetContentOffset() + GetInFlowContentLength(),
                 tmp.ConvertSkippedToOriginal(flowEndInTextRun)) - iter.GetOriginalOffset();
  }
  PropertyProvider provider(textRun, textStyle, frag, this,
                            iter, len, nullptr, 0, aTextRunType);

  bool collapseWhitespace = !textStyle->WhiteSpaceIsSignificant();
  bool preformatNewlines = textStyle->NewlineIsSignificant(this);
  bool preformatTabs = textStyle->WhiteSpaceIsSignificant();
  gfxFloat tabWidth = -1;
  uint32_t start =
    FindStartAfterSkippingWhitespace(&provider, aData, textStyle, &iter, flowEndInTextRun);

  // text-combine-upright frame is constantly 1em on inline-axis.
  if (StyleContext()->IsTextCombined()) {
    if (start < flowEndInTextRun && textRun->CanBreakLineBefore(start)) {
      aData->OptionallyBreak();
    }
    aData->mCurrentLine += provider.GetFontMetrics()->EmHeight();
    aData->mTrailingWhitespace = 0;
    return;
  }

  AutoTArray<gfxTextRun::HyphenType, BIG_TEXT_NODE_SIZE> hyphBuffer;
  if (hyphenating) {
    if (hyphBuffer.AppendElements(flowEndInTextRun - start, fallible)) {
      provider.GetHyphenationBreaks(Range(start, flowEndInTextRun),
                                    hyphBuffer.Elements());
    } else {
      hyphenating = false;
    }
  }

  for (uint32_t i = start, wordStart = start; i <= flowEndInTextRun; ++i) {
    bool preformattedNewline = false;
    bool preformattedTab = false;
    if (i < flowEndInTextRun) {
      // XXXldb Shouldn't we be including the newline as part of the
      // segment that it ends rather than part of the segment that it
      // starts?
      preformattedNewline = preformatNewlines && textRun->CharIsNewline(i);
      preformattedTab = preformatTabs && textRun->CharIsTab(i);
      if (!textRun->CanBreakLineBefore(i) &&
          !preformattedNewline &&
          !preformattedTab &&
          (!hyphenating ||
           hyphBuffer[i - start] == gfxTextRun::HyphenType::None))
      {
        // we can't break here (and it's not the end of the flow)
        continue;
      }
    }

    if (i > wordStart) {
      nscoord width = NSToCoordCeilClamped(
        textRun->GetAdvanceWidth(Range(wordStart, i), &provider));
      width = std::max(0, width);
      aData->mCurrentLine = NSCoordSaturatingAdd(aData->mCurrentLine, width);
      aData->mAtStartOfLine = false;

      if (collapseWhitespace) {
        uint32_t trimStart = GetEndOfTrimmedText(frag, textStyle, wordStart, i, &iter);
        if (trimStart == start) {
          // This is *all* trimmable whitespace, so whatever trailingWhitespace
          // we saw previously is still trailing...
          aData->mTrailingWhitespace += width;
        } else {
          // Some non-whitespace so the old trailingWhitespace is no longer trailing
          nscoord wsWidth = NSToCoordCeilClamped(
            textRun->GetAdvanceWidth(Range(trimStart, i), &provider));
          aData->mTrailingWhitespace = std::max(0, wsWidth);
        }
      } else {
        aData->mTrailingWhitespace = 0;
      }
    }

    if (preformattedTab) {
      PropertyProvider::Spacing spacing;
      provider.GetSpacing(Range(i, i + 1), &spacing);
      aData->mCurrentLine += nscoord(spacing.mBefore);
      if (tabWidth < 0) {
        tabWidth = ComputeTabWidthAppUnits(this, textRun);
      }
      gfxFloat afterTab =
        AdvanceToNextTab(aData->mCurrentLine, tabWidth);
      aData->mCurrentLine = nscoord(afterTab + spacing.mAfter);
      wordStart = i + 1;
    } else if (i < flowEndInTextRun ||
        (i == textRun->GetLength() &&
         (textRun->GetFlags2() & nsTextFrameUtils::Flags::TEXT_HAS_TRAILING_BREAK))) {
      if (preformattedNewline) {
        aData->ForceBreak();
      } else if (i < flowEndInTextRun && hyphenating &&
                 hyphBuffer[i - start] != gfxTextRun::HyphenType::None) {
        aData->OptionallyBreak(NSToCoordRound(provider.GetHyphenWidth()));
      } else {
        aData->OptionallyBreak();
      }
      wordStart = i;
    }
  }

  if (start < flowEndInTextRun) {
    // Check if we have collapsible whitespace at the end
    aData->mSkipWhitespace =
      IsTrimmableSpace(provider.GetFragment(),
                       iter.ConvertSkippedToOriginal(flowEndInTextRun - 1),
                       textStyle);
  }
}

bool nsTextFrame::IsCurrentFontInflation(float aInflation) const {
  return fabsf(aInflation - GetFontSizeInflation()) < 1e-6;
}

// XXX Need to do something here to avoid incremental reflow bugs due to
// first-line and first-letter changing min-width
/* virtual */ void
nsTextFrame::AddInlineMinISize(gfxContext *aRenderingContext,
                               nsIFrame::InlineMinISizeData *aData)
{
  float inflation = nsLayoutUtils::FontSizeInflationFor(this);
  TextRunType trtype = (inflation == 1.0f) ? eNotInflated : eInflated;

  if (trtype == eInflated && !IsCurrentFontInflation(inflation)) {
    // FIXME: Ideally, if we already have a text run, we'd move it to be
    // the uninflated text run.
    ClearTextRun(nullptr, nsTextFrame::eInflated);
  }

  nsTextFrame* f;
  const gfxTextRun* lastTextRun = nullptr;
  // nsContinuingTextFrame does nothing for AddInlineMinISize; all text frames
  // in the flow are handled right here.
  for (f = this; f; f = f->GetNextContinuation()) {
    // f->GetTextRun(nsTextFrame::eNotInflated) could be null if we
    // haven't set up textruns yet for f.  Except in OOM situations,
    // lastTextRun will only be null for the first text frame.
    if (f == this || f->GetTextRun(trtype) != lastTextRun) {
      nsIFrame* lc;
      if (aData->LineContainer() &&
          aData->LineContainer() != (lc = FindLineContainer(f))) {
        NS_ASSERTION(f != this, "wrong InlineMinISizeData container"
                                " for first continuation");
        aData->mLine = nullptr;
        aData->SetLineContainer(lc);
      }

      // This will process all the text frames that share the same textrun as f.
      f->AddInlineMinISizeForFlow(aRenderingContext, aData, trtype);
      lastTextRun = f->GetTextRun(trtype);
    }
  }
}

// XXX this doesn't handle characters shaped by line endings. We need to
// temporarily override the "current line ending" settings.
void
nsTextFrame::AddInlinePrefISizeForFlow(gfxContext *aRenderingContext,
                                       nsIFrame::InlinePrefISizeData *aData,
                                       TextRunType aTextRunType)
{
  uint32_t flowEndInTextRun;
  gfxSkipCharsIterator iter =
    EnsureTextRun(aTextRunType, aRenderingContext->GetDrawTarget(),
                  aData->LineContainer(), aData->mLine, &flowEndInTextRun);
  gfxTextRun *textRun = GetTextRun(aTextRunType);
  if (!textRun)
    return;

  // Pass null for the line container. This will disable tab spacing, but that's
  // OK since we can't really handle tabs for intrinsic sizing anyway.

  const nsStyleText* textStyle = StyleText();
  const nsTextFragment* frag = mContent->GetText();
  PropertyProvider provider(textRun, textStyle, frag, this,
                            iter, INT32_MAX, nullptr, 0, aTextRunType);

  // text-combine-upright frame is constantly 1em on inline-axis.
  if (StyleContext()->IsTextCombined()) {
    aData->mCurrentLine += provider.GetFontMetrics()->EmHeight();
    aData->mTrailingWhitespace = 0;
    aData->mLineIsEmpty = false;
    return;
  }

  bool collapseWhitespace = !textStyle->WhiteSpaceIsSignificant();
  bool preformatNewlines = textStyle->NewlineIsSignificant(this);
  bool preformatTabs = textStyle->TabIsSignificant();
  gfxFloat tabWidth = -1;
  uint32_t start =
    FindStartAfterSkippingWhitespace(&provider, aData, textStyle, &iter, flowEndInTextRun);

  // XXX Should we consider hyphenation here?
  // If newlines and tabs aren't preformatted, nothing to do inside
  // the loop so make i skip to the end
  uint32_t loopStart = (preformatNewlines || preformatTabs) ? start : flowEndInTextRun;
  for (uint32_t i = loopStart, lineStart = start; i <= flowEndInTextRun; ++i) {
    bool preformattedNewline = false;
    bool preformattedTab = false;
    if (i < flowEndInTextRun) {
      // XXXldb Shouldn't we be including the newline as part of the
      // segment that it ends rather than part of the segment that it
      // starts?
      NS_ASSERTION(preformatNewlines || preformatTabs,
                   "We can't be here unless newlines are "
                   "hard breaks or there are tabs");
      preformattedNewline = preformatNewlines && textRun->CharIsNewline(i);
      preformattedTab = preformatTabs && textRun->CharIsTab(i);
      if (!preformattedNewline && !preformattedTab) {
        // we needn't break here (and it's not the end of the flow)
        continue;
      }
    }

    if (i > lineStart) {
      nscoord width = NSToCoordCeilClamped(
        textRun->GetAdvanceWidth(Range(lineStart, i), &provider));
      width = std::max(0, width);
      aData->mCurrentLine = NSCoordSaturatingAdd(aData->mCurrentLine, width);
      aData->mLineIsEmpty = false;

      if (collapseWhitespace) {
        uint32_t trimStart = GetEndOfTrimmedText(frag, textStyle, lineStart, i, &iter);
        if (trimStart == start) {
          // This is *all* trimmable whitespace, so whatever trailingWhitespace
          // we saw previously is still trailing...
          aData->mTrailingWhitespace += width;
        } else {
          // Some non-whitespace so the old trailingWhitespace is no longer trailing
          nscoord wsWidth = NSToCoordCeilClamped(
            textRun->GetAdvanceWidth(Range(trimStart, i), &provider));
          aData->mTrailingWhitespace = std::max(0, wsWidth);
        }
      } else {
        aData->mTrailingWhitespace = 0;
      }
    }

    if (preformattedTab) {
      PropertyProvider::Spacing spacing;
      provider.GetSpacing(Range(i, i + 1), &spacing);
      aData->mCurrentLine += nscoord(spacing.mBefore);
      if (tabWidth < 0) {
        tabWidth = ComputeTabWidthAppUnits(this, textRun);
      }
      gfxFloat afterTab =
        AdvanceToNextTab(aData->mCurrentLine, tabWidth);
      aData->mCurrentLine = nscoord(afterTab + spacing.mAfter);
      aData->mLineIsEmpty = false;
      lineStart = i + 1;
    } else if (preformattedNewline) {
      aData->ForceBreak();
      lineStart = i;
    }
  }

  // Check if we have collapsible whitespace at the end
  if (start < flowEndInTextRun) {
    aData->mSkipWhitespace =
      IsTrimmableSpace(provider.GetFragment(),
                       iter.ConvertSkippedToOriginal(flowEndInTextRun - 1),
                       textStyle);
  }
}

// XXX Need to do something here to avoid incremental reflow bugs due to
// first-line and first-letter changing pref-width
/* virtual */ void
nsTextFrame::AddInlinePrefISize(gfxContext *aRenderingContext,
                                nsIFrame::InlinePrefISizeData *aData)
{
  float inflation = nsLayoutUtils::FontSizeInflationFor(this);
  TextRunType trtype = (inflation == 1.0f) ? eNotInflated : eInflated;

  if (trtype == eInflated && !IsCurrentFontInflation(inflation)) {
    // FIXME: Ideally, if we already have a text run, we'd move it to be
    // the uninflated text run.
    ClearTextRun(nullptr, nsTextFrame::eInflated);
  }

  nsTextFrame* f;
  const gfxTextRun* lastTextRun = nullptr;
  // nsContinuingTextFrame does nothing for AddInlineMinISize; all text frames
  // in the flow are handled right here.
  for (f = this; f; f = f->GetNextContinuation()) {
    // f->GetTextRun(nsTextFrame::eNotInflated) could be null if we
    // haven't set up textruns yet for f.  Except in OOM situations,
    // lastTextRun will only be null for the first text frame.
    if (f == this || f->GetTextRun(trtype) != lastTextRun) {
      nsIFrame* lc;
      if (aData->LineContainer() &&
          aData->LineContainer() != (lc = FindLineContainer(f))) {
        NS_ASSERTION(f != this, "wrong InlinePrefISizeData container"
                                " for first continuation");
        aData->mLine = nullptr;
        aData->SetLineContainer(lc);
      }

      // This will process all the text frames that share the same textrun as f.
      f->AddInlinePrefISizeForFlow(aRenderingContext, aData, trtype);
      lastTextRun = f->GetTextRun(trtype);
    }
  }
}

/* virtual */
LogicalSize
nsTextFrame::ComputeSize(gfxContext *aRenderingContext,
                         WritingMode aWM,
                         const LogicalSize& aCBSize,
                         nscoord aAvailableISize,
                         const LogicalSize& aMargin,
                         const LogicalSize& aBorder,
                         const LogicalSize& aPadding,
                         ComputeSizeFlags aFlags)
{
  // Inlines and text don't compute size before reflow.
  return LogicalSize(aWM, NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
}

static nsRect
RoundOut(const gfxRect& aRect)
{
  nsRect r;
  r.x = NSToCoordFloor(aRect.X());
  r.y = NSToCoordFloor(aRect.Y());
  r.width = NSToCoordCeil(aRect.XMost()) - r.x;
  r.height = NSToCoordCeil(aRect.YMost()) - r.y;
  return r;
}

nsRect
nsTextFrame::ComputeTightBounds(DrawTarget* aDrawTarget) const
{
  if (StyleContext()->HasTextDecorationLines() ||
      (GetStateBits() & TEXT_HYPHEN_BREAK)) {
    // This is conservative, but OK.
    return GetVisualOverflowRect();
  }

  gfxSkipCharsIterator iter =
    const_cast<nsTextFrame*>(this)->EnsureTextRun(nsTextFrame::eInflated);
  if (!mTextRun)
    return nsRect(0, 0, 0, 0);

  PropertyProvider provider(const_cast<nsTextFrame*>(this), iter,
                            nsTextFrame::eInflated);
  // Trim trailing whitespace
  provider.InitializeForDisplay(true);

  gfxTextRun::Metrics metrics =
        mTextRun->MeasureText(ComputeTransformedRange(provider),
                              gfxFont::TIGHT_HINTED_OUTLINE_EXTENTS,
                              aDrawTarget, &provider);
  if (GetWritingMode().IsLineInverted()) {
    metrics.mBoundingBox.y = -metrics.mBoundingBox.YMost();
  }
  // mAscent should be the same as metrics.mAscent, but it's what we use to
  // paint so that's the one we'll use.
  nsRect boundingBox = RoundOut(metrics.mBoundingBox);
  boundingBox += nsPoint(0, mAscent);
  if (mTextRun->IsVertical()) {
    // Swap line-relative textMetrics dimensions to physical coordinates.
    Swap(boundingBox.x, boundingBox.y);
    Swap(boundingBox.width, boundingBox.height);
  }
  return boundingBox;
}

/* virtual */ nsresult
nsTextFrame::GetPrefWidthTightBounds(gfxContext* aContext,
                                     nscoord* aX,
                                     nscoord* aXMost)
{
  gfxSkipCharsIterator iter =
    const_cast<nsTextFrame*>(this)->EnsureTextRun(nsTextFrame::eInflated);
  if (!mTextRun)
    return NS_ERROR_FAILURE;

  PropertyProvider provider(const_cast<nsTextFrame*>(this), iter,
                            nsTextFrame::eInflated);
  provider.InitializeForMeasure();

  gfxTextRun::Metrics metrics =
        mTextRun->MeasureText(ComputeTransformedRange(provider),
                              gfxFont::TIGHT_HINTED_OUTLINE_EXTENTS,
                              aContext->GetDrawTarget(), &provider);
  // Round it like nsTextFrame::ComputeTightBounds() to ensure consistency.
  *aX = NSToCoordFloor(metrics.mBoundingBox.x);
  *aXMost = NSToCoordCeil(metrics.mBoundingBox.XMost());

  return NS_OK;
}

static bool
HasSoftHyphenBefore(const nsTextFragment* aFrag, const gfxTextRun* aTextRun,
                    int32_t aStartOffset, const gfxSkipCharsIterator& aIter)
{
  if (aIter.GetSkippedOffset() < aTextRun->GetLength() &&
      aTextRun->CanHyphenateBefore(aIter.GetSkippedOffset())) {
    return true;
  }
  if (!(aTextRun->GetFlags2() & nsTextFrameUtils::Flags::TEXT_HAS_SHY))
    return false;
  gfxSkipCharsIterator iter = aIter;
  while (iter.GetOriginalOffset() > aStartOffset) {
    iter.AdvanceOriginal(-1);
    if (!iter.IsOriginalCharSkipped())
      break;
    if (aFrag->CharAt(iter.GetOriginalOffset()) == CH_SHY)
      return true;
  }
  return false;
}

/**
 * Removes all frames from aFrame up to (but not including) aFirstToNotRemove,
 * because their text has all been taken and reflowed by earlier frames.
 */
static void
RemoveEmptyInFlows(nsTextFrame* aFrame, nsTextFrame* aFirstToNotRemove)
{
  NS_PRECONDITION(aFrame != aFirstToNotRemove, "This will go very badly");
  // We have to be careful here, because some RemoveFrame implementations
  // remove and destroy not only the passed-in frame but also all its following
  // in-flows (and sometimes all its following continuations in general).  So
  // we remove |f| and everything up to but not including firstToNotRemove from
  // the flow first, to make sure that only the things we want destroyed are
  // destroyed.

  // This sadly duplicates some of the logic from
  // nsSplittableFrame::RemoveFromFlow.  We can get away with not duplicating
  // all of it, because we know that the prev-continuation links of
  // firstToNotRemove and f are fluid, and non-null.
  NS_ASSERTION(aFirstToNotRemove->GetPrevContinuation() ==
               aFirstToNotRemove->GetPrevInFlow() &&
               aFirstToNotRemove->GetPrevInFlow() != nullptr,
               "aFirstToNotRemove should have a fluid prev continuation");
  NS_ASSERTION(aFrame->GetPrevContinuation() ==
               aFrame->GetPrevInFlow() &&
               aFrame->GetPrevInFlow() != nullptr,
               "aFrame should have a fluid prev continuation");

  nsTextFrame* prevContinuation = aFrame->GetPrevContinuation();
  nsTextFrame* lastRemoved = aFirstToNotRemove->GetPrevContinuation();

  for (nsTextFrame* f = aFrame; f != aFirstToNotRemove;
       f = f->GetNextContinuation()) {
    // f is going to be destroyed soon, after it is unlinked from the
    // continuation chain. If its textrun is going to be destroyed we need to
    // do it now, before we unlink the frames to remove from the flow,
    // because DestroyFrom calls ClearTextRuns() and that will start at the
    // first frame with the text run and walk the continuations.
    if (f->IsInTextRunUserData()) {
      f->ClearTextRuns();
    } else {
      f->DisconnectTextRuns();
    }
  }

  prevContinuation->SetNextInFlow(aFirstToNotRemove);
  aFirstToNotRemove->SetPrevInFlow(prevContinuation);

  aFrame->SetPrevInFlow(nullptr);
  lastRemoved->SetNextInFlow(nullptr);

  nsContainerFrame* parent = aFrame->GetParent();
  nsBlockFrame* parentBlock = nsLayoutUtils::GetAsBlock(parent);
  if (parentBlock) {
    // Manually call DoRemoveFrame so we can tell it that we're
    // removing empty frames; this will keep it from blowing away
    // text runs.
    parentBlock->DoRemoveFrame(aFrame, nsBlockFrame::FRAMES_ARE_EMPTY);
  } else {
    // Just remove it normally; use kNoReflowPrincipalList to avoid posting
    // new reflows.
    parent->RemoveFrame(nsIFrame::kNoReflowPrincipalList, aFrame);
  }
}

void
nsTextFrame::SetLength(int32_t aLength, nsLineLayout* aLineLayout,
                       uint32_t aSetLengthFlags)
{
  mContentLengthHint = aLength;
  int32_t end = GetContentOffset() + aLength;
  nsTextFrame* f = GetNextInFlow();
  if (!f)
    return;

  // If our end offset is moving, then even if frames are not being pushed or
  // pulled, content is moving to or from the next line and the next line
  // must be reflowed.
  // If the next-continuation is dirty, then we should dirty the next line now
  // because we may have skipped doing it if we dirtied it in
  // CharacterDataChanged. This is ugly but teaching FrameNeedsReflow
  // and ChildIsDirty to handle a range of frames would be worse.
  if (aLineLayout &&
      (end != f->mContentOffset || (f->GetStateBits() & NS_FRAME_IS_DIRTY))) {
    aLineLayout->SetDirtyNextLine();
  }

  if (end < f->mContentOffset) {
    // Our frame is shrinking. Give the text to our next in flow.
    if (aLineLayout && HasSignificantTerminalNewline() &&
        !GetParent()->IsLetterFrame() &&
        (aSetLengthFlags & ALLOW_FRAME_CREATION_AND_DESTRUCTION)) {
      // Whatever text we hand to our next-in-flow will end up in a frame all of
      // its own, since it ends in a forced linebreak.  Might as well just put
      // it in a separate frame now.  This is important to prevent text run
      // churn; if we did not do that, then we'd likely end up rebuilding
      // textruns for all our following continuations.
      // We skip this optimization when the parent is a first-letter frame
      // because it doesn't deal well with more than one child frame.
      // We also skip this optimization if we were called during bidi
      // resolution, so as not to create a new frame which doesn't appear in
      // the bidi resolver's list of frames
      nsPresContext* presContext = PresContext();
      nsIFrame* newFrame = presContext->PresShell()->FrameConstructor()->
        CreateContinuingFrame(presContext, this, GetParent());
      nsTextFrame* next = static_cast<nsTextFrame*>(newFrame);
      nsFrameList temp(next, next);
      GetParent()->InsertFrames(kNoReflowPrincipalList, this, temp);
      f = next;
    }

    f->mContentOffset = end;
    if (f->GetTextRun(nsTextFrame::eInflated) != mTextRun) {
      ClearTextRuns();
      f->ClearTextRuns();
    }
    return;
  }
  // Our frame is growing. Take text from our in-flow(s).
  // We can take text from frames in lines beyond just the next line.
  // We don't dirty those lines. That's OK, because when we reflow
  // our empty next-in-flow, it will take text from its next-in-flow and
  // dirty that line.

  // Note that in the process we may end up removing some frames from
  // the flow if they end up empty.
  nsTextFrame* framesToRemove = nullptr;
  while (f && f->mContentOffset < end) {
    f->mContentOffset = end;
    if (f->GetTextRun(nsTextFrame::eInflated) != mTextRun) {
      ClearTextRuns();
      f->ClearTextRuns();
    }
    nsTextFrame* next = f->GetNextInFlow();
    // Note: the "f->GetNextSibling() == next" check below is to restrict
    // this optimization to the case where they are on the same child list.
    // Otherwise we might remove the only child of a nsFirstLetterFrame
    // for example and it can't handle that.  See bug 597627 for details.
    if (next && next->mContentOffset <= end && f->GetNextSibling() == next &&
        (aSetLengthFlags & ALLOW_FRAME_CREATION_AND_DESTRUCTION)) {
      // |f| is now empty.  We may as well remove it, instead of copying all
      // the text from |next| into it instead; the latter leads to use
      // rebuilding textruns for all following continuations.
      // We skip this optimization if we were called during bidi resolution,
      // since the bidi resolver may try to handle the destroyed frame later
      // and crash
      if (!framesToRemove) {
        // Remember that we have to remove this frame.
        framesToRemove = f;
      }
    } else if (framesToRemove) {
      RemoveEmptyInFlows(framesToRemove, f);
      framesToRemove = nullptr;
    }
    f = next;
  }

  MOZ_ASSERT(!framesToRemove || (f && f->mContentOffset == end),
             "How did we exit the loop if we null out framesToRemove if "
             "!next || next->mContentOffset > end ?");

  if (framesToRemove) {
    // We are guaranteed that we exited the loop with f not null, per the
    // postcondition above
    RemoveEmptyInFlows(framesToRemove, f);
  }

#ifdef DEBUG
  f = this;
  int32_t iterations = 0;
  while (f && iterations < 10) {
    f->GetContentLength(); // Assert if negative length
    f = f->GetNextContinuation();
    ++iterations;
  }
  f = this;
  iterations = 0;
  while (f && iterations < 10) {
    f->GetContentLength(); // Assert if negative length
    f = f->GetPrevContinuation();
    ++iterations;
  }
#endif
}

bool
nsTextFrame::IsFloatingFirstLetterChild() const
{
  nsIFrame* frame = GetParent();
  return frame && frame->IsFloating() && frame->IsLetterFrame();
}

bool
nsTextFrame::IsInitialLetterChild() const
{
  nsIFrame* frame = GetParent();
  return frame && frame->StyleTextReset()->mInitialLetterSize != 0.0f &&
         frame->IsLetterFrame();
}

struct NewlineProperty {
  int32_t mStartOffset;
  // The offset of the first \n after mStartOffset, or -1 if there is none
  int32_t mNewlineOffset;
};

void
nsTextFrame::Reflow(nsPresContext*           aPresContext,
                    ReflowOutput&     aMetrics,
                    const ReflowInput& aReflowInput,
                    nsReflowStatus&          aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsTextFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aMetrics, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

  // XXX If there's no line layout, we shouldn't even have created this
  // frame. This may happen if, for example, this is text inside a table
  // but not inside a cell. For now, just don't reflow.
  if (!aReflowInput.mLineLayout) {
    ClearMetrics(aMetrics);
    return;
  }

  ReflowText(*aReflowInput.mLineLayout, aReflowInput.AvailableWidth(),
             aReflowInput.mRenderingContext->GetDrawTarget(), aMetrics, aStatus);

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aMetrics);
}

#ifdef ACCESSIBILITY
/**
 * Notifies accessibility about text reflow. Used by nsTextFrame::ReflowText.
 */
class MOZ_STACK_CLASS ReflowTextA11yNotifier
{
public:
  ReflowTextA11yNotifier(nsPresContext* aPresContext, nsIContent* aContent) :
    mContent(aContent), mPresContext(aPresContext)
  {
  }
  ~ReflowTextA11yNotifier()
  {
    nsAccessibilityService* accService = nsIPresShell::AccService();
    if (accService) {
      accService->UpdateText(mPresContext->PresShell(), mContent);
    }
  }
private:
  ReflowTextA11yNotifier();
  ReflowTextA11yNotifier(const ReflowTextA11yNotifier&);
  ReflowTextA11yNotifier& operator =(const ReflowTextA11yNotifier&);

  nsIContent* mContent;
  nsPresContext* mPresContext;
};
#endif

void
nsTextFrame::ReflowText(nsLineLayout& aLineLayout, nscoord aAvailableWidth,
                        DrawTarget* aDrawTarget,
                        ReflowOutput& aMetrics,
                        nsReflowStatus& aStatus)
{
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

#ifdef NOISY_REFLOW
  ListTag(stdout);
  printf(": BeginReflow: availableWidth=%d\n", aAvailableWidth);
#endif

  nsPresContext* presContext = PresContext();

#ifdef ACCESSIBILITY
  // Schedule the update of accessible tree since rendered text might be changed.
  if (StyleVisibility()->IsVisible()) {
    ReflowTextA11yNotifier(presContext, mContent);
  }
#endif

  /////////////////////////////////////////////////////////////////////
  // Set up flags and clear out state
  /////////////////////////////////////////////////////////////////////

  // Clear out the reflow state flags in mState. We also clear the whitespace
  // flags because this can change whether the frame maps whitespace-only text
  // or not. We also clear the flag that tracks whether we had a pending
  // reflow request from CharacterDataChanged (since we're reflowing now).
  RemoveStateBits(TEXT_REFLOW_FLAGS | TEXT_WHITESPACE_FLAGS);
  mReflowRequestedForCharDataChange = false;

  // Temporarily map all possible content while we construct our new textrun.
  // so that when doing reflow our styles prevail over any part of the
  // textrun we look at. Note that next-in-flows may be mapping the same
  // content; gfxTextRun construction logic will ensure that we take priority.
  int32_t maxContentLength = GetInFlowContentLength();

  // We don't need to reflow if there is no content.
  if (!maxContentLength) {
    ClearMetrics(aMetrics);
    return;
  }

#ifdef NOISY_BIDI
    printf("Reflowed textframe\n");
#endif

  const nsStyleText* textStyle = StyleText();

  bool atStartOfLine = aLineLayout.LineAtStart();
  if (atStartOfLine) {
    AddStateBits(TEXT_START_OF_LINE);
  }

  uint32_t flowEndInTextRun;
  nsIFrame* lineContainer = aLineLayout.LineContainerFrame();
  const nsTextFragment* frag = mContent->GetText();

  // DOM offsets of the text range we need to measure, after trimming
  // whitespace, restricting to first-letter, and restricting preformatted text
  // to nearest newline
  int32_t length = maxContentLength;
  int32_t offset = GetContentOffset();

  // Restrict preformatted text to the nearest newline
  int32_t newLineOffset = -1; // this will be -1 or a content offset
  int32_t contentNewLineOffset = -1;
  // Pointer to the nsGkAtoms::newline set on this frame's element
  NewlineProperty* cachedNewlineOffset = nullptr;
  if (textStyle->NewlineIsSignificant(this)) {
    cachedNewlineOffset =
      mContent->HasFlag(NS_HAS_NEWLINE_PROPERTY)
      ? static_cast<NewlineProperty*>(mContent->GetProperty(nsGkAtoms::newline))
      : nullptr;
    if (cachedNewlineOffset && cachedNewlineOffset->mStartOffset <= offset &&
        (cachedNewlineOffset->mNewlineOffset == -1 ||
         cachedNewlineOffset->mNewlineOffset >= offset)) {
      contentNewLineOffset = cachedNewlineOffset->mNewlineOffset;
    } else {
      contentNewLineOffset = FindChar(frag, offset,
                                      mContent->TextLength() - offset, '\n');
    }
    if (contentNewLineOffset < offset + length) {
      /*
        The new line offset could be outside this frame if the frame has been
        split by bidi resolution. In that case we won't use it in this reflow
        (newLineOffset will remain -1), but we will still cache it in mContent
      */
      newLineOffset = contentNewLineOffset;
    }
    if (newLineOffset >= 0) {
      length = newLineOffset + 1 - offset;
    }
  }
  if ((atStartOfLine && !textStyle->WhiteSpaceIsSignificant()) ||
      (GetStateBits() & TEXT_IS_IN_TOKEN_MATHML)) {
    // Skip leading whitespace. Make sure we don't skip a 'pre-line'
    // newline if there is one.
    int32_t skipLength = newLineOffset >= 0 ? length - 1 : length;
    int32_t whitespaceCount =
      GetTrimmableWhitespaceCount(frag, offset, skipLength, 1);
    if (whitespaceCount) {
      offset += whitespaceCount;
      length -= whitespaceCount;
      // Make sure this frame maps the trimmable whitespace.
      if (MOZ_UNLIKELY(offset > GetContentEnd())) {
        SetLength(offset - GetContentOffset(), &aLineLayout,
                  ALLOW_FRAME_CREATION_AND_DESTRUCTION);
      }
    }
  }

  // If trimming whitespace left us with nothing to do, return early.
  if (length == 0) {
    ClearMetrics(aMetrics);
    return;
  }

  bool completedFirstLetter = false;
  // Layout dependent styles are a problem because we need to reconstruct
  // the gfxTextRun based on our layout.
  if (aLineLayout.GetInFirstLetter() || aLineLayout.GetInFirstLine()) {
    SetLength(maxContentLength, &aLineLayout,
              ALLOW_FRAME_CREATION_AND_DESTRUCTION);

    if (aLineLayout.GetInFirstLetter()) {
      // floating first-letter boundaries are significant in textrun
      // construction, so clear the textrun out every time we hit a first-letter
      // and have changed our length (which controls the first-letter boundary)
      ClearTextRuns();
      // Find the length of the first-letter. We need a textrun for this.
      // REVIEW: maybe-bogus inflation should be ok (fixed below)
      gfxSkipCharsIterator iter =
        EnsureTextRun(nsTextFrame::eInflated, aDrawTarget,
                      lineContainer, aLineLayout.GetLine(),
                      &flowEndInTextRun);

      if (mTextRun) {
        int32_t firstLetterLength = length;
        if (aLineLayout.GetFirstLetterStyleOK()) {
          completedFirstLetter =
            FindFirstLetterRange(frag, mTextRun, offset, iter, &firstLetterLength);
          if (newLineOffset >= 0) {
            // Don't allow a preformatted newline to be part of a first-letter.
            firstLetterLength = std::min(firstLetterLength, length - 1);
            if (length == 1) {
              // There is no text to be consumed by the first-letter before the
              // preformatted newline. Note that the first letter is therefore
              // complete (FindFirstLetterRange will have returned false).
              completedFirstLetter = true;
            }
          }
        } else {
          // We're in a first-letter frame's first in flow, so if there
          // was a first-letter, we'd be it. However, for one reason
          // or another (e.g., preformatted line break before this text),
          // we're not actually supposed to have first-letter style. So
          // just make a zero-length first-letter.
          firstLetterLength = 0;
          completedFirstLetter = true;
        }
        length = firstLetterLength;
        if (length) {
          AddStateBits(TEXT_FIRST_LETTER);
        }
        // Change this frame's length to the first-letter length right now
        // so that when we rebuild the textrun it will be built with the
        // right first-letter boundary
        SetLength(offset + length - GetContentOffset(), &aLineLayout,
                  ALLOW_FRAME_CREATION_AND_DESTRUCTION);
        // Ensure that the textrun will be rebuilt
        ClearTextRuns();
      }
    }
  }

  float fontSizeInflation = nsLayoutUtils::FontSizeInflationFor(this);

  if (!IsCurrentFontInflation(fontSizeInflation)) {
    // FIXME: Ideally, if we already have a text run, we'd move it to be
    // the uninflated text run.
    ClearTextRun(nullptr, nsTextFrame::eInflated);
  }

  gfxSkipCharsIterator iter =
    EnsureTextRun(nsTextFrame::eInflated, aDrawTarget,
                  lineContainer, aLineLayout.GetLine(), &flowEndInTextRun);

  NS_ASSERTION(IsCurrentFontInflation(fontSizeInflation),
               "EnsureTextRun should have set font size inflation");

  if (mTextRun && iter.GetOriginalEnd() < offset + length) {
    // The textrun does not map enough text for this frame. This can happen
    // when the textrun was ended in the middle of a text node because a
    // preformatted newline was encountered, and prev-in-flow frames have
    // consumed all the text of the textrun. We need a new textrun.
    ClearTextRuns();
    iter = EnsureTextRun(nsTextFrame::eInflated, aDrawTarget,
                         lineContainer, aLineLayout.GetLine(),
                         &flowEndInTextRun);
  }

  if (!mTextRun) {
    ClearMetrics(aMetrics);
    return;
  }

  NS_ASSERTION(gfxSkipCharsIterator(iter).ConvertOriginalToSkipped(offset + length)
                    <= mTextRun->GetLength(),
               "Text run does not map enough text for our reflow");

  /////////////////////////////////////////////////////////////////////
  // See how much text should belong to this text frame, and measure it
  /////////////////////////////////////////////////////////////////////

  iter.SetOriginalOffset(offset);
  nscoord xOffsetForTabs = (mTextRun->GetFlags2() & nsTextFrameUtils::Flags::TEXT_HAS_TAB) ?
    (aLineLayout.GetCurrentFrameInlineDistanceFromBlock() -
       lineContainer->GetUsedBorderAndPadding().left)
    : -1;
  PropertyProvider provider(mTextRun, textStyle, frag, this, iter, length,
      lineContainer, xOffsetForTabs, nsTextFrame::eInflated);

  uint32_t transformedOffset = provider.GetStart().GetSkippedOffset();

  // The metrics for the text go in here
  gfxTextRun::Metrics textMetrics;
  gfxFont::BoundingBoxType boundingBoxType =
    IsFloatingFirstLetterChild() || IsInitialLetterChild()
    ? gfxFont::TIGHT_HINTED_OUTLINE_EXTENTS
    : gfxFont::LOOSE_INK_EXTENTS;
  NS_ASSERTION(!(NS_REFLOW_CALC_BOUNDING_METRICS & aMetrics.mFlags),
               "We shouldn't be passed NS_REFLOW_CALC_BOUNDING_METRICS anymore");

  int32_t limitLength = length;
  int32_t forceBreak = aLineLayout.GetForcedBreakPosition(this);
  bool forceBreakAfter = false;
  if (forceBreak >= length) {
    forceBreakAfter = forceBreak == length;
    // The break is not within the text considered for this textframe.
    forceBreak = -1;
  }
  if (forceBreak >= 0) {
    limitLength = forceBreak;
  }
  // This is the heart of text reflow right here! We don't know where
  // to break, so we need to see how much text fits in the available width.
  uint32_t transformedLength;
  if (offset + limitLength >= int32_t(frag->GetLength())) {
    NS_ASSERTION(offset + limitLength == int32_t(frag->GetLength()),
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
  uint32_t transformedLastBreak = 0;
  bool usedHyphenation;
  gfxFloat trimmedWidth = 0;
  gfxFloat availWidth = aAvailableWidth;
  if (StyleContext()->IsTextCombined()) {
    // If text-combine-upright is 'all', we would compress whatever long
    // text into ~1em width, so there is no limited on the avail width.
    availWidth = std::numeric_limits<gfxFloat>::infinity();
  }
  bool canTrimTrailingWhitespace = !textStyle->WhiteSpaceIsSignificant() ||
                                   (GetStateBits() & TEXT_IS_IN_TOKEN_MATHML);
  // allow whitespace to overflow the container
  bool whitespaceCanHang = textStyle->WhiteSpaceCanWrapStyle() &&
                           textStyle->WhiteSpaceIsSignificant();
  gfxBreakPriority breakPriority = aLineLayout.LastOptionalBreakPriority();
  gfxTextRun::SuppressBreak suppressBreak = gfxTextRun::eNoSuppressBreak;
  bool shouldSuppressLineBreak = ShouldSuppressLineBreak();
  if (shouldSuppressLineBreak) {
    suppressBreak = gfxTextRun::eSuppressAllBreaks;
  } else if (!aLineLayout.LineIsBreakable()) {
    suppressBreak = gfxTextRun::eSuppressInitialBreak;
  }
  uint32_t transformedCharsFit =
    mTextRun->BreakAndMeasureText(transformedOffset, transformedLength,
                                  (GetStateBits() & TEXT_START_OF_LINE) != 0,
                                  availWidth,
                                  &provider, suppressBreak,
                                  canTrimTrailingWhitespace ? &trimmedWidth : nullptr,
                                  whitespaceCanHang,
                                  &textMetrics, boundingBoxType,
                                  aDrawTarget,
                                  &usedHyphenation, &transformedLastBreak,
                                  textStyle->WordCanWrap(this), &breakPriority);
  if (!length && !textMetrics.mAscent && !textMetrics.mDescent) {
    // If we're measuring a zero-length piece of text, update
    // the height manually.
    nsFontMetrics* fm = provider.GetFontMetrics();
    if (fm) {
      textMetrics.mAscent = gfxFloat(fm->MaxAscent());
      textMetrics.mDescent = gfxFloat(fm->MaxDescent());
    }
  }
  if (GetWritingMode().IsLineInverted()) {
    Swap(textMetrics.mAscent, textMetrics.mDescent);
    textMetrics.mBoundingBox.y = -textMetrics.mBoundingBox.YMost();
  }
  // The "end" iterator points to the first character after the string mapped
  // by this frame. Basically, its original-string offset is offset+charsFit
  // after we've computed charsFit.
  gfxSkipCharsIterator end(provider.GetEndHint());
  end.SetSkippedOffset(transformedOffset + transformedCharsFit);
  int32_t charsFit = end.GetOriginalOffset() - offset;
  if (offset + charsFit == newLineOffset) {
    // We broke before a trailing preformatted '\n'. The newline should
    // be assigned to this frame. Note that newLineOffset will be -1 if
    // there was no preformatted newline, so we wouldn't get here in that
    // case.
    ++charsFit;
  }
  // That might have taken us beyond our assigned content range (because
  // we might have advanced over some skipped chars that extend outside
  // this frame), so get back in.
  int32_t lastBreak = -1;
  if (charsFit >= limitLength) {
    charsFit = limitLength;
    if (transformedLastBreak != UINT32_MAX) {
      // lastBreak is needed.
      // This may set lastBreak greater than 'length', but that's OK
      lastBreak = end.ConvertSkippedToOriginal(transformedOffset + transformedLastBreak);
    }
    end.SetOriginalOffset(offset + charsFit);
    // If we were forced to fit, and the break position is after a soft hyphen,
    // note that this is a hyphenation break.
    if ((forceBreak >= 0 || forceBreakAfter) &&
        HasSoftHyphenBefore(frag, mTextRun, offset, end)) {
      usedHyphenation = true;
    }
  }
  if (usedHyphenation) {
    // Fix up metrics to include hyphen
    AddHyphenToMetrics(this, mTextRun, &textMetrics, boundingBoxType,
                       aDrawTarget);
    AddStateBits(TEXT_HYPHEN_BREAK | TEXT_HAS_NONCOLLAPSED_CHARACTERS);
  }
  if (textMetrics.mBoundingBox.IsEmpty()) {
    AddStateBits(TEXT_NO_RENDERED_GLYPHS);
  }

  gfxFloat trimmableWidth = 0;
  bool brokeText = forceBreak >= 0 || transformedCharsFit < transformedLength;
  if (canTrimTrailingWhitespace) {
    // Optimization: if we trimmed trailing whitespace, and we can be sure
    // this frame will be at the end of the line, then leave it trimmed off.
    // Otherwise we have to undo the trimming, in case we're not at the end of
    // the line. (If we actually do end up at the end of the line, we'll have
    // to trim it off again in TrimTrailingWhiteSpace, and we'd like to avoid
    // having to re-do it.)
    if (brokeText ||
        (GetStateBits() & TEXT_IS_IN_TOKEN_MATHML)) {
      // We're definitely going to break so our trailing whitespace should
      // definitely be trimmed. Record that we've already done it.
      AddStateBits(TEXT_TRIMMED_TRAILING_WHITESPACE);
    } else if (!(GetStateBits() & TEXT_IS_IN_TOKEN_MATHML)) {
      // We might not be at the end of the line. (Note that even if this frame
      // ends in breakable whitespace, it might not be at the end of the line
      // because it might be followed by breakable, but preformatted, whitespace.)
      // Undo the trimming.
      textMetrics.mAdvanceWidth += trimmedWidth;
      trimmableWidth = trimmedWidth;
      if (mTextRun->IsRightToLeft()) {
        // Space comes before text, so the bounding box is moved to the
        // right by trimmdWidth
        textMetrics.mBoundingBox.MoveBy(gfxPoint(trimmedWidth, 0));
      }
    }
  }

  if (!brokeText && lastBreak >= 0) {
    // Since everything fit and no break was forced,
    // record the last break opportunity
    NS_ASSERTION(textMetrics.mAdvanceWidth - trimmableWidth <= availWidth,
                 "If the text doesn't fit, and we have a break opportunity, why didn't MeasureText use it?");
    MOZ_ASSERT(lastBreak >= offset, "Strange break position");
    aLineLayout.NotifyOptionalBreakPosition(this, lastBreak - offset,
                                            true, breakPriority);
  }

  int32_t contentLength = offset + charsFit - GetContentOffset();

  /////////////////////////////////////////////////////////////////////
  // Compute output metrics
  /////////////////////////////////////////////////////////////////////

  // first-letter frames should use the tight bounding box metrics for ascent/descent
  // for good drop-cap effects
  if (GetStateBits() & TEXT_FIRST_LETTER) {
    textMetrics.mAscent = std::max(gfxFloat(0.0), -textMetrics.mBoundingBox.Y());
    textMetrics.mDescent = std::max(gfxFloat(0.0), textMetrics.mBoundingBox.YMost());
  }

  // Setup metrics for caller
  // Disallow negative widths
  WritingMode wm = GetWritingMode();
  LogicalSize finalSize(wm);
  finalSize.ISize(wm) = NSToCoordCeil(std::max(gfxFloat(0.0),
                                               textMetrics.mAdvanceWidth));

  if (transformedCharsFit == 0 && !usedHyphenation) {
    aMetrics.SetBlockStartAscent(0);
    finalSize.BSize(wm) = 0;
  } else if (boundingBoxType != gfxFont::LOOSE_INK_EXTENTS) {
    // Use actual text metrics for floating first letter frame.
    aMetrics.SetBlockStartAscent(NSToCoordCeil(textMetrics.mAscent));
    finalSize.BSize(wm) = aMetrics.BlockStartAscent() +
      NSToCoordCeil(textMetrics.mDescent);
  } else {
    // Otherwise, ascent should contain the overline drawable area.
    // And also descent should contain the underline drawable area.
    // nsFontMetrics::GetMaxAscent/GetMaxDescent contains them.
    nsFontMetrics* fm = provider.GetFontMetrics();
    nscoord fontAscent =
      wm.IsLineInverted() ? fm->MaxDescent() : fm->MaxAscent();
    nscoord fontDescent =
      wm.IsLineInverted() ? fm->MaxAscent() : fm->MaxDescent();
    aMetrics.SetBlockStartAscent(std::max(NSToCoordCeil(textMetrics.mAscent), fontAscent));
    nscoord descent = std::max(NSToCoordCeil(textMetrics.mDescent), fontDescent);
    finalSize.BSize(wm) = aMetrics.BlockStartAscent() + descent;
  }
  if (StyleContext()->IsTextCombined()) {
    nsFontMetrics* fm = provider.GetFontMetrics();
    gfxFloat width = finalSize.ISize(wm);
    gfxFloat em = fm->EmHeight();
    // Compress the characters in horizontal axis if necessary.
    if (width <= em) {
      RemoveProperty(TextCombineScaleFactorProperty());
    } else {
      SetProperty(TextCombineScaleFactorProperty(), em / width);
      finalSize.ISize(wm) = em;
    }
    // Make the characters be in an 1em square.
    if (finalSize.BSize(wm) != em) {
      aMetrics.SetBlockStartAscent(aMetrics.BlockStartAscent() +
                                   (em - finalSize.BSize(wm)) / 2);
      finalSize.BSize(wm) = em;
    }
  }
  aMetrics.SetSize(wm, finalSize);

  NS_ASSERTION(aMetrics.BlockStartAscent() >= 0,
               "Negative ascent???");
  NS_ASSERTION((StyleContext()->IsTextCombined()
                ? aMetrics.ISize(aMetrics.GetWritingMode())
                : aMetrics.BSize(aMetrics.GetWritingMode())) -
               aMetrics.BlockStartAscent() >= 0,
               "Negative descent???");

  mAscent = aMetrics.BlockStartAscent();

  // Handle text that runs outside its normal bounds.
  nsRect boundingBox = RoundOut(textMetrics.mBoundingBox);
  if (mTextRun->IsVertical()) {
    // Swap line-relative textMetrics dimensions to physical coordinates.
    Swap(boundingBox.x, boundingBox.y);
    Swap(boundingBox.width, boundingBox.height);
    if (GetWritingMode().IsVerticalRL()) {
      boundingBox.x = -boundingBox.XMost();
      boundingBox.x += aMetrics.Width() - mAscent;
    } else {
      boundingBox.x += mAscent;
    }
  } else {
    boundingBox.y += mAscent;
  }
  aMetrics.SetOverflowAreasToDesiredBounds();
  aMetrics.VisualOverflow().UnionRect(aMetrics.VisualOverflow(), boundingBox);

  // When we have text decorations, we don't need to compute their overflow now
  // because we're guaranteed to do it later
  // (see nsLineLayout::RelativePositionFrames)
  UnionAdditionalOverflow(presContext, aLineLayout.LineContainerRI()->mFrame,
                          provider, &aMetrics.VisualOverflow(), false);

  /////////////////////////////////////////////////////////////////////
  // Clean up, update state
  /////////////////////////////////////////////////////////////////////

  // If all our characters are discarded or collapsed, then trimmable width
  // from the last textframe should be preserved. Otherwise the trimmable width
  // from this textframe overrides. (Currently in CSS trimmable width can be
  // at most one space so there's no way for trimmable width from a previous
  // frame to accumulate with trimmable width from this frame.)
  if (transformedCharsFit > 0) {
    aLineLayout.SetTrimmableISize(NSToCoordFloor(trimmableWidth));
    AddStateBits(TEXT_HAS_NONCOLLAPSED_CHARACTERS);
  }
  bool breakAfter = forceBreakAfter;
  if (!shouldSuppressLineBreak) {
    if (charsFit > 0 && charsFit == length &&
        textStyle->mHyphens != StyleHyphens::None &&
        HasSoftHyphenBefore(frag, mTextRun, offset, end)) {
      bool fits =
        textMetrics.mAdvanceWidth + provider.GetHyphenWidth() <= availWidth;
      // Record a potential break after final soft hyphen
      aLineLayout.NotifyOptionalBreakPosition(this, length, fits,
                                              gfxBreakPriority::eNormalBreak);
    }
    // length == 0 means either the text is empty or it's all collapsed away
    bool emptyTextAtStartOfLine = atStartOfLine && length == 0;
    if (!breakAfter && charsFit == length && !emptyTextAtStartOfLine &&
        transformedOffset + transformedLength == mTextRun->GetLength() &&
        (mTextRun->GetFlags2() & nsTextFrameUtils::Flags::TEXT_HAS_TRAILING_BREAK)) {
      // We placed all the text in the textrun and we have a break opportunity
      // at the end of the textrun. We need to record it because the following
      // content may not care about nsLineBreaker.

      // Note that because we didn't break, we can be sure that (thanks to the
      // code up above) textMetrics.mAdvanceWidth includes the width of any
      // trailing whitespace. So we need to subtract trimmableWidth here
      // because if we did break at this point, that much width would be
      // trimmed.
      if (textMetrics.mAdvanceWidth - trimmableWidth > availWidth) {
        breakAfter = true;
      } else {
        aLineLayout.NotifyOptionalBreakPosition(this, length, true,
                                                gfxBreakPriority::eNormalBreak);
      }
    }
  }

  // Compute reflow status
  if (contentLength != maxContentLength) {
    aStatus.SetIncomplete();
  }

  if (charsFit == 0 && length > 0 && !usedHyphenation) {
    // Couldn't place any text
    aStatus.SetInlineLineBreakBeforeAndReset();
  } else if (contentLength > 0 && mContentOffset + contentLength - 1 == newLineOffset) {
    // Ends in \n
    aStatus.SetInlineLineBreakAfter();
    aLineLayout.SetLineEndsInBR(true);
  } else if (breakAfter) {
    aStatus.SetInlineLineBreakAfter();
  }
  if (completedFirstLetter) {
    aLineLayout.SetFirstLetterStyleOK(false);
    aStatus.SetFirstLetterComplete();
  }

  // Updated the cached NewlineProperty, or delete it.
  if (contentLength < maxContentLength &&
      textStyle->NewlineIsSignificant(this) &&
      (contentNewLineOffset < 0 ||
       mContentOffset + contentLength <= contentNewLineOffset)) {
    if (!cachedNewlineOffset) {
      cachedNewlineOffset = new NewlineProperty;
      if (NS_FAILED(mContent->SetProperty(nsGkAtoms::newline, cachedNewlineOffset,
                                          nsINode::DeleteProperty<NewlineProperty>))) {
        delete cachedNewlineOffset;
        cachedNewlineOffset = nullptr;
      }
      mContent->SetFlags(NS_HAS_NEWLINE_PROPERTY);
    }
    if (cachedNewlineOffset) {
      cachedNewlineOffset->mStartOffset = offset;
      cachedNewlineOffset->mNewlineOffset = contentNewLineOffset;
    }
  } else if (cachedNewlineOffset) {
    mContent->DeleteProperty(nsGkAtoms::newline);
    mContent->UnsetFlags(NS_HAS_NEWLINE_PROPERTY);
  }

  // Compute space and letter counts for justification, if required
  if (!textStyle->WhiteSpaceIsSignificant() &&
      (lineContainer->StyleText()->mTextAlign == NS_STYLE_TEXT_ALIGN_JUSTIFY ||
       lineContainer->StyleText()->mTextAlignLast == NS_STYLE_TEXT_ALIGN_JUSTIFY ||
       shouldSuppressLineBreak) &&
      !nsSVGUtils::IsInSVGTextSubtree(lineContainer)) {
    AddStateBits(TEXT_JUSTIFICATION_ENABLED);
    Range range(uint32_t(offset), uint32_t(offset + charsFit));
    aLineLayout.SetJustificationInfo(provider.ComputeJustification(range));
  }

  SetLength(contentLength, &aLineLayout, ALLOW_FRAME_CREATION_AND_DESTRUCTION);

  InvalidateFrame();

#ifdef NOISY_REFLOW
  ListTag(stdout);
  printf(": desiredSize=%d,%d(b=%d) status=%x\n",
         aMetrics.Width(), aMetrics.Height(), aMetrics.BlockStartAscent(),
         aStatus);
#endif
}

/* virtual */ bool
nsTextFrame::CanContinueTextRun() const
{
  // We can continue a text run through a text frame
  return true;
}

nsTextFrame::TrimOutput
nsTextFrame::TrimTrailingWhiteSpace(DrawTarget* aDrawTarget)
{
  MOZ_ASSERT(!HasAnyStateBits(NS_FRAME_IS_DIRTY | NS_FRAME_FIRST_REFLOW),
             "frame should have been reflowed");

  TrimOutput result;
  result.mChanged = false;
  result.mDeltaWidth = 0;

  AddStateBits(TEXT_END_OF_LINE);

  if (!GetTextRun(nsTextFrame::eInflated)) {
    // If reflow didn't create a textrun, there must have been no content once
    // leading whitespace was trimmed, so nothing more to do here.
    return result;
  }

  int32_t contentLength = GetContentLength();
  if (!contentLength)
    return result;

  gfxSkipCharsIterator start =
    EnsureTextRun(nsTextFrame::eInflated, aDrawTarget);
  NS_ENSURE_TRUE(mTextRun, result);

  uint32_t trimmedStart = start.GetSkippedOffset();

  const nsTextFragment* frag = mContent->GetText();
  TrimmedOffsets trimmed = GetTrimmedOffsets(frag, true);
  gfxSkipCharsIterator trimmedEndIter = start;
  const nsStyleText* textStyle = StyleText();
  gfxFloat delta = 0;
  uint32_t trimmedEnd = trimmedEndIter.ConvertOriginalToSkipped(trimmed.GetEnd());

  if (!(GetStateBits() & TEXT_TRIMMED_TRAILING_WHITESPACE) &&
      trimmed.GetEnd() < GetContentEnd()) {
    gfxSkipCharsIterator end = trimmedEndIter;
    uint32_t endOffset = end.ConvertOriginalToSkipped(GetContentOffset() + contentLength);
    if (trimmedEnd < endOffset) {
      // We can't be dealing with tabs here ... they wouldn't be trimmed. So it's
      // OK to pass null for the line container.
      PropertyProvider provider(mTextRun, textStyle, frag, this, start, contentLength,
                                nullptr, 0, nsTextFrame::eInflated);
      delta = mTextRun->
        GetAdvanceWidth(Range(trimmedEnd, endOffset), &provider);
      result.mChanged = true;
    }
  }

  gfxFloat advanceDelta;
  mTextRun->SetLineBreaks(Range(trimmedStart, trimmedEnd),
                          (GetStateBits() & TEXT_START_OF_LINE) != 0, true,
                          &advanceDelta);
  if (advanceDelta != 0) {
    result.mChanged = true;
  }

  // aDeltaWidth is *subtracted* from our width.
  // If advanceDelta is positive then setting the line break made us longer,
  // so aDeltaWidth could go negative.
  result.mDeltaWidth = NSToCoordFloor(delta - advanceDelta);
  // If aDeltaWidth goes negative, that means this frame might not actually fit
  // anymore!!! We need higher level line layout to recover somehow.
  // If it's because the frame has a soft hyphen that is now being displayed,
  // this should actually be OK, because our reflow recorded the break
  // opportunity that allowed the soft hyphen to be used, and we wouldn't
  // have recorded the opportunity unless the hyphen fit (or was the first
  // opportunity on the line).
  // Otherwise this can/ really only happen when we have glyphs with special
  // shapes at the end of lines, I think. Breaking inside a kerning pair won't
  // do it because that would mean we broke inside this textrun, and
  // BreakAndMeasureText should make sure the resulting shaped substring fits.
  // Maybe if we passed a maxTextLength? But that only happens at direction
  // changes (so we wouldn't kern across the boundary) or for first-letter
  // (which always fits because it starts the line!).
  NS_WARNING_ASSERTION(result.mDeltaWidth >= 0,
                       "Negative deltawidth, something odd is happening");

#ifdef NOISY_TRIM
  ListTag(stdout);
  printf(": trim => %d\n", result.mDeltaWidth);
#endif
  return result;
}

nsOverflowAreas
nsTextFrame::RecomputeOverflow(nsIFrame* aBlockFrame)
{
  nsRect bounds(nsPoint(0, 0), GetSize());
  nsOverflowAreas result(bounds, bounds);

  gfxSkipCharsIterator iter = EnsureTextRun(nsTextFrame::eInflated);
  if (!mTextRun)
    return result;

  PropertyProvider provider(this, iter, nsTextFrame::eInflated);
  // Don't trim trailing space, in case we need to paint it as selected.
  provider.InitializeForDisplay(false);

  gfxTextRun::Metrics textMetrics =
    mTextRun->MeasureText(ComputeTransformedRange(provider),
                          gfxFont::LOOSE_INK_EXTENTS, nullptr,
                          &provider);
  if (GetWritingMode().IsLineInverted()) {
    textMetrics.mBoundingBox.y = -textMetrics.mBoundingBox.YMost();
  }
  nsRect boundingBox = RoundOut(textMetrics.mBoundingBox);
  boundingBox += nsPoint(0, mAscent);
  if (mTextRun->IsVertical()) {
    // Swap line-relative textMetrics dimensions to physical coordinates.
    Swap(boundingBox.x, boundingBox.y);
    Swap(boundingBox.width, boundingBox.height);
  }
  nsRect &vis = result.VisualOverflow();
  vis.UnionRect(vis, boundingBox);
  UnionAdditionalOverflow(PresContext(), aBlockFrame, provider, &vis, true);
  return result;
}

static void TransformChars(nsTextFrame* aFrame, const nsStyleText* aStyle,
                           const gfxTextRun* aTextRun, uint32_t aSkippedOffset,
                           const nsTextFragment* aFrag, int32_t aFragOffset,
                           int32_t aFragLen, nsAString& aOut)
{
  nsAutoString fragString;
  char16_t* out;
  if (aStyle->mTextTransform == NS_STYLE_TEXT_TRANSFORM_NONE) {
    // No text-transform, so we can copy directly to the output string.
    aOut.SetLength(aOut.Length() + aFragLen);
    out = aOut.EndWriting() - aFragLen;
  } else {
    // Use a temporary string as source for the transform.
    fragString.SetLength(aFragLen);
    out = fragString.BeginWriting();
  }

  // Copy the text, with \n and \t replaced by <space> if appropriate.
  for (int32_t i = 0; i < aFragLen; ++i) {
    char16_t ch = aFrag->CharAt(aFragOffset + i);
    if ((ch == '\n' && !aStyle->NewlineIsSignificant(aFrame)) ||
        (ch == '\t' && !aStyle->TabIsSignificant())) {
      ch = ' ';
    }
    out[i] = ch;
  }

  if (aStyle->mTextTransform != NS_STYLE_TEXT_TRANSFORM_NONE) {
    MOZ_ASSERT(aTextRun->GetFlags2() & nsTextFrameUtils::Flags::TEXT_IS_TRANSFORMED);
    if (aTextRun->GetFlags2() & nsTextFrameUtils::Flags::TEXT_IS_TRANSFORMED) {
      // Apply text-transform according to style in the transformed run.
      auto transformedTextRun =
        static_cast<const nsTransformedTextRun*>(aTextRun);
      nsAutoString convertedString;
      AutoTArray<bool,50> charsToMergeArray;
      AutoTArray<bool,50> deletedCharsArray;
      nsCaseTransformTextRunFactory::TransformString(fragString,
                                                     convertedString,
                                                     false, nullptr,
                                                     charsToMergeArray,
                                                     deletedCharsArray,
                                                     transformedTextRun,
                                                     aSkippedOffset);
      aOut.Append(convertedString);
    } else {
      // Should not happen (see assertion above), but as a fallback...
      aOut.Append(fragString);
    }
  }
}

static bool
LineEndsInHardLineBreak(nsTextFrame* aFrame, nsBlockFrame* aLineContainer)
{
  bool foundValidLine;
  nsBlockInFlowLineIterator iter(aLineContainer, aFrame, &foundValidLine);
  if (!foundValidLine) {
    NS_ERROR("Invalid line!");
    return true;
  }
  return !iter.GetLine()->IsLineWrapped();
}

nsIFrame::RenderedText
nsTextFrame::GetRenderedText(uint32_t aStartOffset,
                             uint32_t aEndOffset,
                             TextOffsetType aOffsetType,
                             TrailingWhitespace aTrimTrailingWhitespace)
{
  MOZ_ASSERT(aStartOffset <= aEndOffset, "bogus offsets");
  MOZ_ASSERT(!GetPrevContinuation() ||
             (aOffsetType == TextOffsetType::OFFSETS_IN_CONTENT_TEXT &&
              aStartOffset >= (uint32_t)GetContentOffset() &&
              aEndOffset <= (uint32_t)GetContentEnd()),
             "Must be called on first-in-flow, or content offsets must be "
             "given and be within this frame.");

  // The handling of offsets could be more efficient...
  RenderedText result;
  nsBlockFrame* lineContainer = nullptr;
  nsTextFrame* textFrame;
  const nsTextFragment* textFrag = mContent->GetText();
  uint32_t offsetInRenderedString = 0;
  bool haveOffsets = false;

  Maybe<nsBlockFrame::AutoLineCursorSetup> autoLineCursor;
  for (textFrame = this; textFrame;
       textFrame = textFrame->GetNextContinuation()) {
    if (textFrame->GetStateBits() & NS_FRAME_IS_DIRTY) {
      // We don't trust dirty frames, especially when computing rendered text.
      break;
    }

    // Ensure the text run and grab the gfxSkipCharsIterator for it
    gfxSkipCharsIterator iter =
      textFrame->EnsureTextRun(nsTextFrame::eInflated);
    if (!textFrame->mTextRun) {
      break;
    }
    gfxSkipCharsIterator tmpIter = iter;

    // Whether we need to trim whitespaces after the text frame.
    bool trimAfter;
    if (!textFrame->IsAtEndOfLine() ||
        aTrimTrailingWhitespace !=
          TrailingWhitespace::TRIM_TRAILING_WHITESPACE) {
      trimAfter = false;
    } else if (nsBlockFrame* thisLc =
               do_QueryFrame(FindLineContainer(textFrame))) {
      if (thisLc != lineContainer) {
        // Setup line cursor when needed.
        lineContainer = thisLc;
        autoLineCursor.reset();
        autoLineCursor.emplace(lineContainer);
      }
      trimAfter = LineEndsInHardLineBreak(textFrame, lineContainer);
    } else {
      // Weird situation where we have a line layout without a block.
      // No soft breaks occur in this situation.
      trimAfter = true;
    }

    // Skip to the start of the text run, past ignored chars at start of line
    TrimmedOffsets trimmedOffsets =
        textFrame->GetTrimmedOffsets(textFrag, trimAfter);
    bool trimmedSignificantNewline =
        trimmedOffsets.GetEnd() < GetContentEnd() &&
        HasSignificantTerminalNewline();
    uint32_t skippedToRenderedStringOffset = offsetInRenderedString -
        tmpIter.ConvertOriginalToSkipped(trimmedOffsets.mStart);
    uint32_t nextOffsetInRenderedString =
        tmpIter.ConvertOriginalToSkipped(trimmedOffsets.GetEnd()) +
        (trimmedSignificantNewline ? 1 : 0) + skippedToRenderedStringOffset;

    if (aOffsetType == TextOffsetType::OFFSETS_IN_RENDERED_TEXT) {
      if (nextOffsetInRenderedString <= aStartOffset) {
        offsetInRenderedString = nextOffsetInRenderedString;
        continue;
      }
      if (!haveOffsets) {
        result.mOffsetWithinNodeText =
            tmpIter.ConvertSkippedToOriginal(aStartOffset - skippedToRenderedStringOffset);
        result.mOffsetWithinNodeRenderedText = aStartOffset;
        haveOffsets = true;
      }
      if (offsetInRenderedString >= aEndOffset) {
        break;
      }
    } else {
      if (uint32_t(textFrame->GetContentEnd()) <= aStartOffset) {
        offsetInRenderedString = nextOffsetInRenderedString;
        continue;
      }
      if (!haveOffsets) {
        result.mOffsetWithinNodeText = aStartOffset;
        // Skip trimmed space when computed the rendered text offset.
        int32_t clamped = std::max<int32_t>(aStartOffset, trimmedOffsets.mStart);
        result.mOffsetWithinNodeRenderedText =
            tmpIter.ConvertOriginalToSkipped(clamped) + skippedToRenderedStringOffset;
        MOZ_ASSERT(result.mOffsetWithinNodeRenderedText >= offsetInRenderedString &&
                   result.mOffsetWithinNodeRenderedText <= INT32_MAX,
                   "Bad offset within rendered text");
        haveOffsets = true;
      }
      if (uint32_t(textFrame->mContentOffset) >= aEndOffset) {
        break;
      }
    }

    int32_t startOffset;
    int32_t endOffset;
    if (aOffsetType == TextOffsetType::OFFSETS_IN_RENDERED_TEXT) {
      startOffset =
        tmpIter.ConvertSkippedToOriginal(aStartOffset - skippedToRenderedStringOffset);
      endOffset =
        tmpIter.ConvertSkippedToOriginal(aEndOffset - skippedToRenderedStringOffset);
    } else {
      startOffset = aStartOffset;
      endOffset = std::min<uint32_t>(INT32_MAX, aEndOffset);
    }
    trimmedOffsets.mStart = std::max<uint32_t>(trimmedOffsets.mStart,
        startOffset);
    trimmedOffsets.mLength = std::min<uint32_t>(trimmedOffsets.GetEnd(),
        endOffset) - trimmedOffsets.mStart;
    if (trimmedOffsets.mLength <= 0) {
      offsetInRenderedString = nextOffsetInRenderedString;
      continue;
    }

    const nsStyleText* textStyle = textFrame->StyleText();
    iter.SetOriginalOffset(trimmedOffsets.mStart);
    while (iter.GetOriginalOffset() < trimmedOffsets.GetEnd()) {
      int32_t runLength;
      bool isSkipped = iter.IsOriginalCharSkipped(&runLength);
      runLength = std::min(runLength,
                           trimmedOffsets.GetEnd() - iter.GetOriginalOffset());
      if (isSkipped) {
        for (int32_t i = 0; i < runLength; ++i) {
          char16_t ch = textFrag->CharAt(iter.GetOriginalOffset() + i);
          if (ch == CH_SHY) {
            // We should preserve soft hyphens. They can't be transformed.
            result.mString.Append(ch);
          }
        }
      } else {
        TransformChars(textFrame, textStyle, textFrame->mTextRun,
                       iter.GetSkippedOffset(), textFrag,
                       iter.GetOriginalOffset(), runLength, result.mString);
      }
      iter.AdvanceOriginal(runLength);
    }

    if (trimmedSignificantNewline && GetContentEnd() <= endOffset) {
      // A significant newline was trimmed off (we must be
      // white-space:pre-line). Put it back.
      result.mString.Append('\n');
    }
    offsetInRenderedString = nextOffsetInRenderedString;
  }

  if (!haveOffsets) {
    result.mOffsetWithinNodeText = textFrag->GetLength();
    result.mOffsetWithinNodeRenderedText = offsetInRenderedString;
  }
  return result;
}

/* virtual */ bool
nsTextFrame::IsEmpty()
{
  NS_ASSERTION(!(mState & TEXT_IS_ONLY_WHITESPACE) ||
               !(mState & TEXT_ISNOT_ONLY_WHITESPACE),
               "Invalid state");

  // XXXldb Should this check compatibility mode as well???
  const nsStyleText* textStyle = StyleText();
  if (textStyle->WhiteSpaceIsSignificant()) {
    // XXX shouldn't we return true if the length is zero?
    return false;
  }

  if (mState & TEXT_ISNOT_ONLY_WHITESPACE) {
    return false;
  }

  if (mState & TEXT_IS_ONLY_WHITESPACE) {
    return true;
  }

  bool isEmpty =
    IsAllWhitespace(mContent->GetText(),
                    textStyle->mWhiteSpace != mozilla::StyleWhiteSpace::PreLine);
  AddStateBits(isEmpty ? TEXT_IS_ONLY_WHITESPACE : TEXT_ISNOT_ONLY_WHITESPACE);
  return isEmpty;
}

#ifdef DEBUG_FRAME_DUMP
// Translate the mapped content into a string that's printable
void
nsTextFrame::ToCString(nsCString& aBuf, int32_t* aTotalContentLength) const
{
  // Get the frames text content
  const nsTextFragment* frag = mContent->GetText();
  if (!frag) {
    return;
  }

  // Compute the total length of the text content.
  *aTotalContentLength = frag->GetLength();

  int32_t contentLength = GetContentLength();
  // Set current fragment and current fragment offset
  if (0 == contentLength) {
    return;
  }
  int32_t fragOffset = GetContentOffset();
  int32_t n = fragOffset + contentLength;
  while (fragOffset < n) {
    char16_t ch = frag->CharAt(fragOffset++);
    if (ch == '\r') {
      aBuf.AppendLiteral("\\r");
    } else if (ch == '\n') {
      aBuf.AppendLiteral("\\n");
    } else if (ch == '\t') {
      aBuf.AppendLiteral("\\t");
    } else if ((ch < ' ') || (ch >= 127)) {
      aBuf.Append(nsPrintfCString("\\u%04x", ch));
    } else {
      aBuf.Append(ch);
    }
  }
}

nsresult
nsTextFrame::GetFrameName(nsAString& aResult) const
{
  MakeFrameName(NS_LITERAL_STRING("Text"), aResult);
  int32_t totalContentLength;
  nsAutoCString tmp;
  ToCString(tmp, &totalContentLength);
  tmp.SetLength(std::min(tmp.Length(), 50u));
  aResult += NS_LITERAL_STRING("\"") + NS_ConvertASCIItoUTF16(tmp) + NS_LITERAL_STRING("\"");
  return NS_OK;
}

void
nsTextFrame::List(FILE* out, const char* aPrefix, uint32_t aFlags) const
{
  nsCString str;
  ListGeneric(str, aPrefix, aFlags);

  str += nsPrintfCString(" [run=%p]", static_cast<void*>(mTextRun));

  // Output the first/last content offset and prev/next in flow info
  bool isComplete = uint32_t(GetContentEnd()) == GetContent()->TextLength();
  str += nsPrintfCString("[%d,%d,%c] ", GetContentOffset(), GetContentLength(),
          isComplete ? 'T':'F');

  if (IsSelected()) {
    str += " SELECTED";
  }
  fprintf_stderr(out, "%s\n", str.get());
}
#endif

#ifdef DEBUG
nsFrameState
nsTextFrame::GetDebugStateBits() const
{
  // mask out our emptystate flags; those are just caches
  return nsFrame::GetDebugStateBits() &
    ~(TEXT_WHITESPACE_FLAGS | TEXT_REFLOW_FLAGS);
}
#endif

void
nsTextFrame::AdjustOffsetsForBidi(int32_t aStart, int32_t aEnd)
{
  AddStateBits(NS_FRAME_IS_BIDI);
  if (mContent->HasFlag(NS_HAS_FLOWLENGTH_PROPERTY)) {
    mContent->DeleteProperty(nsGkAtoms::flowlength);
    mContent->UnsetFlags(NS_HAS_FLOWLENGTH_PROPERTY);
  }

  /*
   * After Bidi resolution we may need to reassign text runs.
   * This is called during bidi resolution from the block container, so we
   * shouldn't be holding a local reference to a textrun anywhere.
   */
  ClearTextRuns();

  nsTextFrame* prev = GetPrevContinuation();
  if (prev) {
    // the bidi resolver can be very evil when columns/pages are involved. Don't
    // let it violate our invariants.
    int32_t prevOffset = prev->GetContentOffset();
    aStart = std::max(aStart, prevOffset);
    aEnd = std::max(aEnd, prevOffset);
    prev->ClearTextRuns();
  }

  mContentOffset = aStart;
  SetLength(aEnd - aStart, nullptr, 0);
}

/**
 * @return true if this text frame ends with a newline character.  It should return
 * false if it is not a text frame.
 */
bool
nsTextFrame::HasSignificantTerminalNewline() const
{
  return ::HasTerminalNewline(this) && StyleText()->NewlineIsSignificant(this);
}

bool
nsTextFrame::IsAtEndOfLine() const
{
  return (GetStateBits() & TEXT_END_OF_LINE) != 0;
}

nscoord
nsTextFrame::GetLogicalBaseline(WritingMode aWM) const
{
  if (!aWM.IsOrthogonalTo(GetWritingMode())) {
    return mAscent;
  }

  // When the text frame has a writing mode orthogonal to the desired
  // writing mode, return a baseline coincides its parent frame.
  nsIFrame* parent = GetParent();
  nsPoint position = GetNormalPosition();
  nscoord parentAscent = parent->GetLogicalBaseline(aWM);
  if (aWM.IsVerticalRL()) {
    nscoord parentDescent = parent->GetSize().width - parentAscent;
    nscoord descent = parentDescent - position.x;
    return GetSize().width - descent;
  }
  return parentAscent - (aWM.IsVertical() ? position.x : position.y);
}

bool
nsTextFrame::HasAnyNoncollapsedCharacters()
{
  gfxSkipCharsIterator iter = EnsureTextRun(nsTextFrame::eInflated);
  int32_t offset = GetContentOffset(),
          offsetEnd = GetContentEnd();
  int32_t skippedOffset = iter.ConvertOriginalToSkipped(offset);
  int32_t skippedOffsetEnd = iter.ConvertOriginalToSkipped(offsetEnd);
  return skippedOffset != skippedOffsetEnd;
}

bool
nsTextFrame::ComputeCustomOverflow(nsOverflowAreas& aOverflowAreas)
{
  if (GetStateBits() & NS_FRAME_FIRST_REFLOW) {
    return true;
  }

  nsIFrame* decorationsBlock;
  if (IsFloatingFirstLetterChild()) {
    decorationsBlock = GetParent();
  } else {
    nsIFrame* f = this;
    for (;;) {
      nsBlockFrame* fBlock = nsLayoutUtils::GetAsBlock(f);
      if (fBlock) {
        decorationsBlock = fBlock;
        break;
      }

      f = f->GetParent();
      if (!f) {
        NS_ERROR("Couldn't find any block ancestor (for text decorations)");
        return nsFrame::ComputeCustomOverflow(aOverflowAreas);
      }
    }
  }

  aOverflowAreas = RecomputeOverflow(decorationsBlock);
  return nsFrame::ComputeCustomOverflow(aOverflowAreas);
}

NS_DECLARE_FRAME_PROPERTY_SMALL_VALUE(JustificationAssignmentProperty, int32_t)

void
nsTextFrame::AssignJustificationGaps(
    const mozilla::JustificationAssignment& aAssign)
{
  int32_t encoded = (aAssign.mGapsAtStart << 8) | aAssign.mGapsAtEnd;
  static_assert(sizeof(aAssign) == 1,
                "The encoding might be broken if JustificationAssignment "
                "is larger than 1 byte");
  SetProperty(JustificationAssignmentProperty(), encoded);
}

mozilla::JustificationAssignment
nsTextFrame::GetJustificationAssignment() const
{
  int32_t encoded = GetProperty(JustificationAssignmentProperty());
  mozilla::JustificationAssignment result;
  result.mGapsAtStart = encoded >> 8;
  result.mGapsAtEnd = encoded & 0xFF;
  return result;
}

uint32_t
nsTextFrame::CountGraphemeClusters() const
{
  const nsTextFragment* frag = GetContent()->GetText();
  MOZ_ASSERT(frag, "Text frame must have text fragment");
  nsAutoString content;
  frag->AppendTo(content, GetContentOffset(), GetContentLength());
  return unicode::CountGraphemeClusters(content.Data(), content.Length());
}

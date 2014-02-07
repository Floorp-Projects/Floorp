/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for textual content of elements */

#include "nsTextFrame.h"

#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Likely.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/TextEvents.h"

#include "nsCOMPtr.h"
#include "nsBlockFrame.h"
#include "nsCRT.h"
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
#include "nsRenderingContext.h"
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
#include "nsExpirationTracker.h"
#include "nsUnicodeProperties.h"

#include "nsTextFragment.h"
#include "nsGkAtoms.h"
#include "nsFrameSelection.h"
#include "nsRange.h"
#include "nsCSSRendering.h"
#include "nsContentUtils.h"
#include "nsLineBreaker.h"
#include "nsIWordBreaker.h"
#include "nsGenericDOMDataNode.h"
#include "nsIFrameInlines.h"

#include <algorithm>
#ifdef ACCESSIBILITY
#include "nsAccessibilityService.h"
#endif
#include "nsAutoPtr.h"

#include "nsPrintfCString.h"

#include "gfxFont.h"
#include "gfxContext.h"

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

struct TabWidth {
  TabWidth(uint32_t aOffset, uint32_t aWidth)
    : mOffset(aOffset), mWidth(float(aWidth))
  { }

  uint32_t mOffset; // DOM offset relative to the current frame's offset.
  float    mWidth;  // extra space to be added at this position (in app units)
};

struct TabWidthStore {
  TabWidthStore(int32_t aValidForContentOffset)
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

void
TabWidthStore::ApplySpacing(gfxTextRun::PropertyProvider::Spacing *aSpacing,
                            uint32_t aOffset, uint32_t aLength)
{
  uint32_t i = 0, len = mWidths.Length();

  // If aOffset is non-zero, do a binary search to find where to start
  // processing the tab widths, in case the list is really long. (See bug
  // 953247.)
  // We need to start from the first entry where mOffset >= aOffset.
  if (aOffset > 0) {
    uint32_t lo = 0, hi = len;
    while (lo < hi) {
      i = (lo + hi) / 2;
      const TabWidth& tw = mWidths[i];
      if (tw.mOffset < aOffset) {
        // mWidths[i] precedes the target range; new search range
        // will be [i+1, hi)
        lo = ++i;
        continue;
      }
      if (tw.mOffset > aOffset) {
        // mWidths[i] is within (or beyond) the target range;
        // new search range is [lo, i). If it turns out that
        // mWidths[i] was the first entry within the range,
        // we'll never move hi any further, and end up exiting
        // when i == lo == this value of hi.
        hi = i;
        continue;
      }
      // Found an exact match for aOffset, so end search now
      break;
    }
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

static void DestroyTabWidth(void* aPropertyValue)
{
  delete static_cast<TabWidthStore*>(aPropertyValue);
}

NS_DECLARE_FRAME_PROPERTY(TabWidthProperty, DestroyTabWidth)

NS_DECLARE_FRAME_PROPERTY(OffsetToFrameProperty, nullptr)

// text runs are destroyed by the text run cache
NS_DECLARE_FRAME_PROPERTY(UninflatedTextRunProperty, nullptr)

NS_DECLARE_FRAME_PROPERTY(FontSizeInflationProperty, nullptr)

class GlyphObserver : public gfxFont::GlyphChangeObserver {
public:
  GlyphObserver(gfxFont* aFont, nsTextFrame* aFrame)
    : gfxFont::GlyphChangeObserver(aFont), mFrame(aFrame) {}
  virtual void NotifyGlyphsChanged() MOZ_OVERRIDE;
private:
  nsTextFrame* mFrame;
};

static void DestroyGlyphObserverList(void* aPropertyValue)
{
  delete static_cast<nsTArray<nsAutoPtr<GlyphObserver> >*>(aPropertyValue);
}

/**
 * This property is set on text frames with TEXT_IN_TEXTRUN_USER_DATA set that
 * have potentially-animated glyphs.
 * The only reason this list is in a property is to automatically destroy the
 * list when the frame is deleted, unregistering the observers.
 */
NS_DECLARE_FRAME_PROPERTY(TextFrameGlyphObservers, DestroyGlyphObserverList);

#define TEXT_REFLOW_FLAGS    \
  (TEXT_FIRST_LETTER|TEXT_START_OF_LINE|TEXT_END_OF_LINE|TEXT_HYPHEN_BREAK| \
   TEXT_TRIMMED_TRAILING_WHITESPACE|TEXT_JUSTIFICATION_ENABLED| \
   TEXT_HAS_NONCOLLAPSED_CHARACTERS|TEXT_SELECTION_UNDERLINE_OVERFLOWED)

#define TEXT_WHITESPACE_FLAGS      (TEXT_IS_ONLY_WHITESPACE | \
                                    TEXT_ISNOT_ONLY_WHITESPACE)

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
 * Our convention is that unsigned integer character offsets are offsets into
 * the transformed string. Signed integer character offsets are offsets into
 * the DOM string.
 * 
 * XXX currently we don't handle hyphenated breaks between text frames where the
 * hyphen occurs at the end of the first text frame, e.g.
 *   <b>Kit&shy;</b>ty
 */

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
 * This is our user data for the textrun, when textRun->GetFlags() does not
 * have TEXT_IS_SIMPLE_FLOW set. When TEXT_IS_SIMPLE_FLOW is set, there is
 * just one flow, the textrun's user data pointer is a pointer to mStartFrame
 * for that flow, mDOMOffsetToBeforeTransformOffset is zero, and mContentLength
 * is the length of the text node.
 */
struct TextRunUserData {
  TextRunMappedFlow* mMappedFlows;
  uint32_t           mMappedFlowCount;
  uint32_t           mLastFlowIndex;
};

/**
 * This helper object computes colors used for painting, and also IME
 * underline information. The data is computed lazily and cached as necessary.
 * These live for just the duration of one paint operation.
 */
class nsTextPaintStyle {
public:
  nsTextPaintStyle(nsTextFrame* aFrame);

  void SetResolveColors(bool aResolveColors) {
    NS_ASSERTION(mFrame->IsSVGText() || aResolveColors,
                 "must resolve colors is frame is not for SVG text");
    mResolveColors = aResolveColors;
  }

  nscolor GetTextColor();
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

  static int32_t GetUnderlineStyleIndexForSelectionType(int32_t aSelectionType)
  {
    switch (aSelectionType) {
      case nsISelectionController::SELECTION_IME_RAWINPUT:
        return eIndexRawInput;
      case nsISelectionController::SELECTION_IME_SELECTEDRAWTEXT:
        return eIndexSelRawText;
      case nsISelectionController::SELECTION_IME_CONVERTEDTEXT:
        return eIndexConvText;
      case nsISelectionController::SELECTION_IME_SELECTEDCONVERTEDTEXT:
        return eIndexSelConvText;
      case nsISelectionController::SELECTION_SPELLCHECK:
        return eIndexSpellChecker;
      default:
        NS_WARNING("non-IME selection type");
        return eIndexRawInput;
    }
  }

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
  nsRefPtr<nsCSSShadowArray> mSelectionShadow;
  bool                       mHasSelectionShadow;

  // Common data

  int32_t mSufficientContrast;
  nscolor mFrameBackgroundColor;

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

static void
DestroyUserData(void* aUserData)
{
  TextRunUserData* userData = static_cast<TextRunUserData*>(aUserData);
  if (userData) {
    nsMemory::Free(userData);
  }
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
      NS_ASSERTION(aFrame->GetType() == nsGkAtoms::textFrame, "Bad frame");
      aFrame = static_cast<nsTextFrame*>(aFrame->GetNextContinuation());
    } while (aFrame && aFrame != aStartContinuation);
  }
  bool found = aStartContinuation == aFrame;
  while (aFrame) {
    NS_ASSERTION(aFrame->GetType() == nsGkAtoms::textFrame, "Bad frame");
    if (!aFrame->RemoveTextRun(aTextRun)) {
      break;
    }
    aFrame = static_cast<nsTextFrame*>(aFrame->GetNextContinuation());
  }
  NS_POSTCONDITION(!found || aStartContinuation, "how did we find null?");
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
  if (!aTextRun->GetUserData())
    return;

  if (aTextRun->GetFlags() & nsTextFrameUtils::TEXT_IS_SIMPLE_FLOW) {
    nsTextFrame* userDataFrame = static_cast<nsTextFrame*>(
      static_cast<nsIFrame*>(aTextRun->GetUserData()));
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
      aTextRun->SetUserData(nullptr);
    }
  } else {
    TextRunUserData* userData =
      static_cast<TextRunUserData*>(aTextRun->GetUserData());
    int32_t destroyFromIndex = aStartContinuation ? -1 : 0;
    for (uint32_t i = 0; i < userData->mMappedFlowCount; ++i) {
      nsTextFrame* userDataFrame = userData->mMappedFlows[i].mStartFrame;
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
        }
        else {
          destroyFromIndex = i;
        }
        aStartContinuation = nullptr;
      }
    }
    NS_ASSERTION(destroyFromIndex >= 0,
                 "aStartContinuation wasn't found in multi flow text run");
    if (destroyFromIndex == 0) {
      DestroyUserData(userData);
      aTextRun->SetUserData(nullptr);
    }
    else {
      userData->mMappedFlowCount = uint32_t(destroyFromIndex);
      if (userData->mLastFlowIndex >= uint32_t(destroyFromIndex)) {
        userData->mLastFlowIndex = uint32_t(destroyFromIndex) - 1;
      }
    }
  }
}

void
GlyphObserver::NotifyGlyphsChanged()
{
  nsIPresShell* shell = mFrame->PresContext()->PresShell();
  for (nsIFrame* f = mFrame; f;
       f = nsLayoutUtils::GetNextContinuationOrIBSplitSibling(f)) {
    if (f != mFrame && f->HasAnyStateBits(TEXT_IN_TEXTRUN_USER_DATA)) {
      // f will have its own GlyphObserver (if needed) so we can stop here.
      break;
    }
    f->InvalidateFrame();
    // Theoretically we could just update overflow areas, perhaps using
    // OverflowChangedTracker, but that would do a bunch of work eagerly that
    // we should probably do lazily here since there could be a lot
    // of text frames affected and we'd like to coalesce the work. So that's
    // not easy to do well.
    shell->FrameNeedsReflow(f, nsIPresShell::eResize, NS_FRAME_IS_DIRTY);
  }
}

class FrameTextRunCache;

static FrameTextRunCache *gTextRuns = nullptr;

/*
 * Cache textruns and expire them after 3*10 seconds of no use.
 */
class FrameTextRunCache MOZ_FINAL : public nsExpirationTracker<gfxTextRun,3> {
public:
  enum { TIMEOUT_SECONDS = 10 };
  FrameTextRunCache()
      : nsExpirationTracker<gfxTextRun,3>(TIMEOUT_SECONDS*1000) {}
  ~FrameTextRunCache() {
    AgeAllGenerations();
  }

  void RemoveFromCache(gfxTextRun* aTextRun) {
    if (aTextRun->GetExpirationState()->IsTracked()) {
      RemoveObject(aTextRun);
    }
  }

  // This gets called when the timeout has expired on a gfxTextRun
  virtual void NotifyExpired(gfxTextRun* aTextRun) {
    UnhookTextRunFromFrames(aTextRun, nullptr);
    RemoveFromCache(aTextRun);
    delete aTextRun;
  }
};

// Helper to create a textrun and remember it in the textframe cache,
// for either 8-bit or 16-bit text strings
template<typename T>
gfxTextRun *
MakeTextRun(const T *aText, uint32_t aLength,
            gfxFontGroup *aFontGroup, const gfxFontGroup::Parameters* aParams,
            uint32_t aFlags)
{
    nsAutoPtr<gfxTextRun> textRun(aFontGroup->MakeTextRun(aText, aLength,
                                                          aParams, aFlags));
    if (!textRun) {
        return nullptr;
    }
    nsresult rv = gTextRuns->AddObject(textRun);
    if (NS_FAILED(rv)) {
        gTextRuns->RemoveFromCache(textRun);
        return nullptr;
    }
#ifdef NOISY_BIDI
    printf("Created textrun\n");
#endif
    return textRun.forget();
}

void
nsTextFrameTextRunCache::Init() {
    gTextRuns = new FrameTextRunCache();
}

void
nsTextFrameTextRunCache::Shutdown() {
    delete gTextRuns;
    gTextRuns = nullptr;
}

int32_t nsTextFrame::GetContentEnd() const {
  nsTextFrame* next = static_cast<nsTextFrame*>(GetNextContinuation());
  return next ? next->GetContentOffset() : mContent->GetText()->GetLength();
}

struct FlowLengthProperty {
  int32_t mStartOffset;
  // The offset of the next fixed continuation after mStartOffset, or
  // of the end of the text if there is none
  int32_t mEndFlowOffset;

  static void Destroy(void* aObject, nsIAtom* aPropertyName,
                      void* aPropertyValue, void* aData)
  {
    delete static_cast<FlowLengthProperty*>(aPropertyValue);
  }
};

int32_t nsTextFrame::GetInFlowContentLength() {
  if (!(mState & NS_FRAME_IS_BIDI)) {
    return mContent->TextLength() - mContentOffset;
  }

  FlowLengthProperty* flowLength =
    static_cast<FlowLengthProperty*>(mContent->GetProperty(nsGkAtoms::flowlength));

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

  nsTextFrame* nextBidi = static_cast<nsTextFrame*>(LastInFlow()->GetNextContinuation());
  int32_t endFlow = nextBidi ? nextBidi->GetContentOffset() : mContent->TextLength();

  if (!flowLength) {
    flowLength = new FlowLengthProperty;
    if (NS_FAILED(mContent->SetProperty(nsGkAtoms::flowlength, flowLength,
                                        FlowLengthProperty::Destroy))) {
      delete flowLength;
      flowLength = nullptr;
    }
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
static bool IsCSSWordSpacingSpace(const nsTextFragment* aFrag,
                                    uint32_t aPos, const nsStyleText* aStyleText)
{
  NS_ASSERTION(aPos < aFrag->GetLength(), "No text for IsSpace!");

  char16_t ch = aFrag->CharAt(aPos);
  switch (ch) {
  case ' ':
  case CH_NBSP:
    return !IsSpaceCombiningSequenceTail(aFrag, aPos + 1);
  case '\r':
  case '\t': return !aStyleText->WhiteSpaceIsSignificant();
  case '\n': return !aStyleText->NewlineIsSignificant() &&
                    !aStyleText->NewlineIsDiscarded();
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
  case '\n': return !aStyleText->NewlineIsSignificant() &&
                    !aStyleText->NewlineIsDiscarded();
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

static bool
IsAllNewlines(const nsTextFragment* aFrag)
{
  if (aFrag->Is2b())
    return false;
  int32_t len = aFrag->GetLength();
  const char* str = aFrag->Get1b();
  for (int32_t i = 0; i < len; ++i) {
    char ch = str[i];
    if (ch != '\n')
      return false;
  }
  return true;
}

static void
CreateObserverForAnimatedGlyphs(nsTextFrame* aFrame, const nsTArray<gfxFont*>& aFonts)
{
  if (!(aFrame->GetStateBits() & TEXT_IN_TEXTRUN_USER_DATA)) {
    // Maybe the textrun was created for uninflated text.
    return;
  }

  nsTArray<nsAutoPtr<GlyphObserver> >* observers =
    new nsTArray<nsAutoPtr<GlyphObserver> >();
  for (uint32_t i = 0, count = aFonts.Length(); i < count; ++i) {
    observers->AppendElement(new GlyphObserver(aFonts[i], aFrame));
  }
  aFrame->Properties().Set(TextFrameGlyphObservers(), observers);
  // We are lazy and don't try to remove a property value that might be
  // obsolete due to style changes or font selection changes. That is
  // likely to be rarely needed, and we don't want to eat the overhead of
  // doing it for the overwhelmingly common case of no property existing.
  // (And we're out of state bits to conveniently use for a fast property
  // existence check.) The only downside is that in some rare cases we might
  // keep fonts alive for longer than necessary, or unnecessarily invalidate
  // frames.
}

static void
CreateObserversForAnimatedGlyphs(gfxTextRun* aTextRun)
{
  if (!aTextRun->GetUserData()) {
    return;
  }
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
    return;
  }

  if (aTextRun->GetFlags() & nsTextFrameUtils::TEXT_IS_SIMPLE_FLOW) {
    CreateObserverForAnimatedGlyphs(static_cast<nsTextFrame*>(
      static_cast<nsIFrame*>(aTextRun->GetUserData())), fontsWithAnimatedGlyphs);
  } else {
    TextRunUserData* userData =
      static_cast<TextRunUserData*>(aTextRun->GetUserData());
    for (uint32_t i = 0; i < userData->mMappedFlowCount; ++i) {
      CreateObserverForAnimatedGlyphs(userData->mMappedFlows[i].mStartFrame,
                                      fontsWithAnimatedGlyphs);
    }
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
  BuildTextRunsScanner(nsPresContext* aPresContext, gfxContext* aContext,
      nsIFrame* aLineContainer, nsTextFrame::TextRunType aWhichTextRun) :
    mCurrentFramesAllSameTextRun(nullptr),
    mContext(aContext),
    mLineContainer(aLineContainer),
    mBidiEnabled(aPresContext->BidiEnabled()),
    mSkipIncompleteTextRuns(false),
    mWhichTextRun(aWhichTextRun),
    mNextRunContextInfo(nsTextFrameUtils::INCOMING_NONE),
    mCurrentRunContextInfo(nsTextFrameUtils::INCOMING_NONE) {
    ResetRunInfo();
  }
  ~BuildTextRunsScanner() {
    NS_ASSERTION(mBreakSinks.IsEmpty(), "Should have been cleared");
    NS_ASSERTION(mTextRunsToDelete.IsEmpty(), "Should have been cleared");
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
  bool IsTextRunValidForMappedFlows(gfxTextRun* aTextRun);
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
  gfxTextRun* BuildTextRunForFrames(void* aTextBuffer);
  bool SetupLineBreakerContext(gfxTextRun *aTextRun);
  void AssignTextRun(gfxTextRun* aTextRun, float aInflation);
  nsTextFrame* GetNextBreakBeforeFrame(uint32_t* aIndex);
  enum SetupBreakSinksFlags {
    SBS_DOUBLE_BYTE =      (1 << 0),
    SBS_EXISTING_TEXTRUN = (1 << 1),
    SBS_SUPPRESS_SINK    = (1 << 2)
  };
  void SetupBreakSinksForTextRun(gfxTextRun* aTextRun,
                                 const void* aTextPtr,
                                 uint32_t    aFlags);
  struct FindBoundaryState {
    nsIFrame*    mStopAtFrame;
    nsTextFrame* mFirstTextFrame;
    nsTextFrame* mLastTextFrame;
    bool mSeenTextRunBoundaryOnLaterLine;
    bool mSeenTextRunBoundaryOnThisLine;
    bool mSeenSpaceForLineBreakingOnThisLine;
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

  class BreakSink MOZ_FINAL : public nsILineBreakSink {
  public:
    BreakSink(gfxTextRun* aTextRun, gfxContext* aContext, uint32_t aOffsetIntoTextRun,
              bool aExistingTextRun) :
                mTextRun(aTextRun), mContext(aContext),
                mOffsetIntoTextRun(aOffsetIntoTextRun),
                mChangedBreaks(false), mExistingTextRun(aExistingTextRun) {}

    virtual void SetBreaks(uint32_t aOffset, uint32_t aLength,
                           uint8_t* aBreakBefore) {
      if (mTextRun->SetPotentialLineBreaks(aOffset + mOffsetIntoTextRun, aLength,
                                           aBreakBefore, mContext)) {
        mChangedBreaks = true;
        // Be conservative and assume that some breaks have been set
        mTextRun->ClearFlagBits(nsTextFrameUtils::TEXT_NO_BREAKS);
      }
    }
    
    virtual void SetCapitalization(uint32_t aOffset, uint32_t aLength,
                                   bool* aCapitalize) {
      NS_ASSERTION(mTextRun->GetFlags() & nsTextFrameUtils::TEXT_IS_TRANSFORMED,
                   "Text run should be transformed!");
      nsTransformedTextRun* transformedTextRun =
        static_cast<nsTransformedTextRun*>(mTextRun);
      transformedTextRun->SetCapitalization(aOffset + mOffsetIntoTextRun, aLength,
                                            aCapitalize, mContext);
    }

    void Finish() {
      NS_ASSERTION(!(mTextRun->GetFlags() &
                     (gfxTextRunFactory::TEXT_UNUSED_FLAGS |
                      nsTextFrameUtils::TEXT_UNUSED_FLAG)),
                   "Flag set that should never be set! (memory safety error?)");
      if (mTextRun->GetFlags() & nsTextFrameUtils::TEXT_IS_TRANSFORMED) {
        nsTransformedTextRun* transformedTextRun =
          static_cast<nsTransformedTextRun*>(mTextRun);
        transformedTextRun->FinishSettingProperties(mContext);
      }
      // The way nsTransformedTextRun is implemented, its glyph runs aren't
      // available until after nsTransformedTextRun::FinishSettingProperties()
      // is called. So that's why we defer checking for animated glyphs to here.
      CreateObserversForAnimatedGlyphs(mTextRun);
    }

    gfxTextRun*  mTextRun;
    gfxContext*  mContext;
    uint32_t     mOffsetIntoTextRun;
    bool mChangedBreaks;
    bool mExistingTextRun;
  };

private:
  nsAutoTArray<MappedFlow,10>   mMappedFlows;
  nsAutoTArray<nsTextFrame*,50> mLineBreakBeforeFrames;
  nsAutoTArray<nsAutoPtr<BreakSink>,10> mBreakSinks;
  nsAutoTArray<gfxTextRun*,5>   mTextRunsToDelete;
  nsLineBreaker                 mLineBreaker;
  gfxTextRun*                   mCurrentFramesAllSameTextRun;
  gfxContext*                   mContext;
  nsIFrame*                     mLineContainer;
  nsTextFrame*                  mLastFrame;
  // The common ancestor of the current frame and the previous leaf frame
  // on the line, or null if there was no previous leaf frame.
  nsIFrame*                     mCommonAncestorWithLastFrame;
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
  while (aFrame && aFrame->CanContinueTextRun()) {
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

struct FrameTextTraversal {
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
CanTextCrossFrameBoundary(nsIFrame* aFrame, nsIAtom* aType)
{
  NS_ASSERTION(aType == aFrame->GetType(), "Wrong type");

  FrameTextTraversal result;

  bool continuesTextRun = aFrame->CanContinueTextRun();
  if (aType == nsGkAtoms::placeholderFrame) {
    // placeholders are "invisible", so a text run should be able to span
    // across one. But don't descend into the out-of-flow.
    result.mLineBreakerCanCrossFrameBoundary = true;
    result.mOverflowFrameToScan = nullptr;
    if (continuesTextRun) {
      // ... Except for first-letter floats, which are really in-flow
      // from the point of view of capitalization etc, so we'd better
      // descend into them. But we actually need to break the textrun for
      // first-letter floats since things look bad if, say, we try to make a
      // ligature across the float boundary.
      result.mFrameToScan =
        (static_cast<nsPlaceholderFrame*>(aFrame))->GetOutOfFlowFrame();
      result.mScanSiblings = false;
      result.mTextRunCanCrossFrameBoundary = false;
    } else {
      result.mFrameToScan = nullptr;
      result.mTextRunCanCrossFrameBoundary = true;
    }
  } else {
    if (continuesTextRun) {
      result.mFrameToScan = aFrame->GetFirstPrincipalChild();
      result.mOverflowFrameToScan =
        aFrame->GetFirstChild(nsIFrame::kOverflowList);
      NS_WARN_IF_FALSE(!result.mOverflowFrameToScan,
                       "Scanning overflow inline frames is something we should avoid");
      result.mScanSiblings = true;
      result.mTextRunCanCrossFrameBoundary = true;
      result.mLineBreakerCanCrossFrameBoundary = true;
    } else {
      result.mFrameToScan = nullptr;
      result.mOverflowFrameToScan = nullptr;
      result.mTextRunCanCrossFrameBoundary = false;
      result.mLineBreakerCanCrossFrameBoundary = false;
    }
  }    
  return result;
}

BuildTextRunsScanner::FindBoundaryResult
BuildTextRunsScanner::FindBoundaries(nsIFrame* aFrame, FindBoundaryState* aState)
{
  nsIAtom* frameType = aFrame->GetType();
  nsTextFrame* textFrame = frameType == nsGkAtoms::textFrame
    ? static_cast<nsTextFrame*>(aFrame) : nullptr;
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
    if (!aState->mSeenSpaceForLineBreakingOnThisLine) {
      const nsTextFragment* frag = textFrame->GetContent()->GetText();
      uint32_t start = textFrame->GetContentOffset();
      const void* text = frag->Is2b()
          ? static_cast<const void*>(frag->Get2b() + start)
          : static_cast<const void*>(frag->Get1b() + start);
      if (TextContainsLineBreakerWhiteSpace(text, textFrame->GetContentLength(),
                                            frag->Is2b())) {
        aState->mSeenSpaceForLineBreakingOnThisLine = true;
        if (aState->mSeenTextRunBoundaryOnLaterLine)
          return FB_FOUND_VALID_TEXTRUN_BOUNDARY;
      }
    }
    return FB_CONTINUE; 
  }

  FrameTextTraversal traversal =
    CanTextCrossFrameBoundary(aFrame, frameType);
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
BuildTextRuns(gfxContext* aContext, nsTextFrame* aForFrame,
              nsIFrame* aLineContainer,
              const nsLineList::iterator* aForFrameLine,
              nsTextFrame::TextRunType aWhichTextRun)
{
  NS_ASSERTION(aForFrame || aLineContainer,
               "One of aForFrame or aLineContainer must be set!");
  NS_ASSERTION(!aForFrameLine || aLineContainer,
               "line but no line container");
  
  nsIFrame* lineContainerChild = aForFrame;
  if (!aLineContainer) {
    if (aForFrame->IsFloatingFirstLetterChild()) {
      lineContainerChild = aForFrame->PresContext()->PresShell()->
        GetPlaceholderFrameFor(aForFrame->GetParent());
    }
    aLineContainer = FindLineContainer(lineContainerChild);
  } else {
    NS_ASSERTION(!aForFrame ||
                 (aLineContainer == FindLineContainer(aForFrame) ||
                  (aLineContainer->GetType() == nsGkAtoms::letterFrame &&
                   aLineContainer->IsFloating())),
                 "Wrong line container hint");
  }

  if (aForFrame) {
    if (aForFrame->HasAnyStateBits(TEXT_IS_IN_TOKEN_MATHML)) {
      aLineContainer->AddStateBits(TEXT_IS_IN_TOKEN_MATHML);
      if (aForFrame->HasAnyStateBits(NS_FRAME_IS_IN_SINGLE_CHAR_MI)) {
        aLineContainer->AddStateBits(NS_FRAME_IS_IN_SINGLE_CHAR_MI);
      }
    }
    if (aForFrame->HasAnyStateBits(NS_FRAME_MATHML_SCRIPT_DESCENDANT)) {
      aLineContainer->AddStateBits(NS_FRAME_MATHML_SCRIPT_DESCENDANT);
    }
  }

  nsPresContext* presContext = aLineContainer->PresContext();
  BuildTextRunsScanner scanner(presContext, aContext, aLineContainer,
                               aWhichTextRun);

  nsBlockFrame* block = nsLayoutUtils::GetAsBlock(aLineContainer);

  if (!block) {
    NS_ASSERTION(!aLineContainer->GetPrevInFlow() && !aLineContainer->GetNextInFlow(),
                 "Breakable non-block line containers not supported");
    // Just loop through all the children of the linecontainer ... it's really
    // just one line
    scanner.SetAtStartOfLine();
    scanner.SetCommonAncestorWithLastFrame(nullptr);
    nsIFrame* child = aLineContainer->GetFirstPrincipalChild();
    while (child) {
      scanner.ScanFrame(child);
      child = child->GetNextSibling();
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
  nsBlockFrame::line_iterator startLine = backIterator.GetLine();

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
  bool seenTextRunBoundaryOnLaterLine = false;
  bool mayBeginInTextRun = true;
  while (true) {
    forwardIterator = backIterator;
    nsBlockFrame::line_iterator line = backIterator.GetLine();
    if (!backIterator.Prev() || backIterator.GetLine()->IsBlock()) {
      mayBeginInTextRun = false;
      break;
    }

    BuildTextRunsScanner::FindBoundaryState state = { stopAtFrame, nullptr, nullptr,
      bool(seenTextRunBoundaryOnLaterLine), false, false };
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
    nsBlockFrame::line_iterator line = forwardIterator.GetLine();
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

bool BuildTextRunsScanner::IsTextRunValidForMappedFlows(gfxTextRun* aTextRun)
{
  if (aTextRun->GetFlags() & nsTextFrameUtils::TEXT_IS_SIMPLE_FLOW)
    return mMappedFlows.Length() == 1 &&
      mMappedFlows[0].mStartFrame == static_cast<nsTextFrame*>(aTextRun->GetUserData()) &&
      mMappedFlows[0].mEndFrame == nullptr;

  TextRunUserData* userData = static_cast<TextRunUserData*>(aTextRun->GetUserData());
  if (userData->mMappedFlowCount != mMappedFlows.Length())
    return false;
  for (uint32_t i = 0; i < mMappedFlows.Length(); ++i) {
    if (userData->mMappedFlows[i].mStartFrame != mMappedFlows[i].mStartFrame ||
        int32_t(userData->mMappedFlows[i].mContentLength) !=
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
  gfxTextRun* textRun = nullptr;
  if (!mMappedFlows.IsEmpty()) {
    if (!mSkipIncompleteTextRuns && mCurrentFramesAllSameTextRun &&
        ((mCurrentFramesAllSameTextRun->GetFlags() & nsTextFrameUtils::TEXT_INCOMING_WHITESPACE) != 0) ==
        ((mCurrentRunContextInfo & nsTextFrameUtils::INCOMING_WHITESPACE) != 0) &&
        ((mCurrentFramesAllSameTextRun->GetFlags() & gfxTextRunFactory::TEXT_INCOMING_ARABICCHAR) != 0) ==
        ((mCurrentRunContextInfo & nsTextFrameUtils::INCOMING_ARABICCHAR) != 0) &&
        IsTextRunValidForMappedFlows(mCurrentFramesAllSameTextRun)) {
      // Optimization: We do not need to (re)build the textrun.
      textRun = mCurrentFramesAllSameTextRun;

      // Feed this run's text into the linebreaker to provide context.
      if (!SetupLineBreakerContext(textRun)) {
        return;
      }
 
      // Update mNextRunContextInfo appropriately
      mNextRunContextInfo = nsTextFrameUtils::INCOMING_NONE;
      if (textRun->GetFlags() & nsTextFrameUtils::TEXT_TRAILING_WHITESPACE) {
        mNextRunContextInfo |= nsTextFrameUtils::INCOMING_WHITESPACE;
      }
      if (textRun->GetFlags() & gfxTextRunFactory::TEXT_TRAILING_ARABICCHAR) {
        mNextRunContextInfo |= nsTextFrameUtils::INCOMING_ARABICCHAR;
      }
    } else {
      AutoFallibleTArray<uint8_t,BIG_TEXT_NODE_SIZE> buffer;
      uint32_t bufferSize = mMaxTextLength*(mDoubleByteText ? 2 : 1);
      if (bufferSize < mMaxTextLength || bufferSize == UINT32_MAX ||
          !buffer.AppendElements(bufferSize)) {
        return;
      }
      textRun = BuildTextRunForFrames(buffer.Elements());
    }
  }

  if (aFlushLineBreaks) {
    FlushLineBreaks(aSuppressTrailingBreak ? nullptr : textRun);
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
    aTrailingTextRun->SetFlagBits(nsTextFrameUtils::TEXT_HAS_TRAILING_BREAK);
  }

  for (uint32_t i = 0; i < mBreakSinks.Length(); ++i) {
    if (!mBreakSinks[i]->mExistingTextRun || mBreakSinks[i]->mChangedBreaks) {
      // TODO cause frames associated with the textrun to be reflowed, if they
      // aren't being reflowed already!
    }
    mBreakSinks[i]->Finish();
  }
  mBreakSinks.Clear();

  for (uint32_t i = 0; i < mTextRunsToDelete.Length(); ++i) {
    gfxTextRun* deleteTextRun = mTextRunsToDelete[i];
    gTextRuns->RemoveFromCache(deleteTextRun);
    delete deleteTextRun;
  }
  mTextRunsToDelete.Clear();
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
  mappedFlow->mEndFrame = static_cast<nsTextFrame*>(aFrame->GetNextContinuation());
  if (mCurrentFramesAllSameTextRun != aFrame->GetTextRun(mWhichTextRun)) {
    mCurrentFramesAllSameTextRun = nullptr;
  }

  if (mStartOfLine) {
    mLineBreakBeforeFrames.AppendElement(aFrame);
    mStartOfLine = false;
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

static bool
HasTerminalNewline(const nsTextFrame* aFrame)
{
  if (aFrame->GetContentLength() == 0)
    return false;
  const nsTextFragment* frag = aFrame->GetContent()->GetText();
  return frag->CharAt(aFrame->GetContentEnd() - 1) == '\n';
}

static nscoord
LetterSpacing(nsIFrame* aFrame, const nsStyleText* aStyleText = nullptr)
{
  if (aFrame->IsSVGText()) {
    return 0;
  }
  if (!aStyleText) {
    aStyleText = aFrame->StyleText();
  }
  return StyleToCoord(aStyleText->mLetterSpacing);
}

static nscoord
WordSpacing(nsIFrame* aFrame, const nsStyleText* aStyleText = nullptr)
{
  if (aFrame->IsSVGText()) {
    return 0;
  }
  if (!aStyleText) {
    aStyleText = aFrame->StyleText();
  }
  return aStyleText->mWordSpacing;
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
  if (mBidiEnabled &&
      (NS_GET_EMBEDDING_LEVEL(aFrame1) != NS_GET_EMBEDDING_LEVEL(aFrame2) ||
       NS_GET_PARAGRAPH_DEPTH(aFrame1) != NS_GET_PARAGRAPH_DEPTH(aFrame2)))
    return false;

  nsStyleContext* sc1 = aFrame1->StyleContext();
  const nsStyleText* textStyle1 = sc1->StyleText();
  // If the first frame ends in a preformatted newline, then we end the textrun
  // here. This avoids creating giant textruns for an entire plain text file.
  // Note that we create a single text frame for a preformatted text node,
  // even if it has newlines in it, so typically we won't see trailing newlines
  // until after reflow has broken up the frame into one (or more) frames per
  // line. That's OK though.
  if (textStyle1->NewlineIsSignificant() && HasTerminalNewline(aFrame1))
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
  return fontStyle1->mFont.BaseEquals(fontStyle2->mFont) &&
    sc1->StyleFont()->mLanguage == sc2->StyleFont()->mLanguage &&
    textStyle1->mTextTransform == textStyle2->mTextTransform &&
    nsLayoutUtils::GetTextRunFlagsForStyle(sc1, fontStyle1, letterSpacing1) ==
      nsLayoutUtils::GetTextRunFlagsForStyle(sc2, fontStyle2, letterSpacing2);
}

void BuildTextRunsScanner::ScanFrame(nsIFrame* aFrame)
{
  // First check if we can extend the current mapped frame block. This is common.
  if (mMappedFlows.Length() > 0) {
    MappedFlow* mappedFlow = &mMappedFlows[mMappedFlows.Length() - 1];
    if (mappedFlow->mEndFrame == aFrame &&
        (aFrame->GetStateBits() & NS_FRAME_IS_FLUID_CONTINUATION)) {
      NS_ASSERTION(aFrame->GetType() == nsGkAtoms::textFrame,
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

  nsIAtom* frameType = aFrame->GetType();
  // Now see if we can add a new set of frames to the current textrun
  if (frameType == nsGkAtoms::textFrame) {
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

  FrameTextTraversal traversal =
    CanTextCrossFrameBoundary(aFrame, frameType);
  bool isBR = frameType == nsGkAtoms::brFrame;
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

static uint32_t
GetSpacingFlags(nscoord spacing)
{
  return spacing ? gfxTextRunFactory::TEXT_ENABLE_SPACING : 0;
}

static gfxFontGroup*
GetFontGroupForFrame(nsIFrame* aFrame, float aFontSizeInflation,
                     nsFontMetrics** aOutFontMetrics = nullptr)
{
  if (aOutFontMetrics)
    *aOutFontMetrics = nullptr;

  nsRefPtr<nsFontMetrics> metrics;
  nsLayoutUtils::GetFontMetricsForFrame(aFrame, getter_AddRefs(metrics),
                                        aFontSizeInflation);

  if (!metrics)
    return nullptr;

  if (aOutFontMetrics) {
    *aOutFontMetrics = metrics;
    NS_ADDREF(*aOutFontMetrics);
  }
  // XXX this is a bit bogus, we're releasing 'metrics' so the
  // returned font-group might actually be torn down, although because
  // of the way the device context caches font metrics, this seems to
  // not actually happen. But we should fix this.
  return metrics->GetThebesFontGroup();
}

static already_AddRefed<gfxContext>
GetReferenceRenderingContext(nsTextFrame* aTextFrame, nsRenderingContext* aRC)
{
  nsRefPtr<nsRenderingContext> tmp = aRC;
  if (!tmp) {
    tmp = aTextFrame->PresContext()->PresShell()->GetReferenceRenderingContext();
    if (!tmp)
      return nullptr;
  }

  nsRefPtr<gfxContext> ctx = tmp->ThebesContext();
  return ctx.forget();
}

/**
 * The returned textrun must be deleted when no longer needed.
 */
static gfxTextRun*
GetHyphenTextRun(gfxTextRun* aTextRun, gfxContext* aContext, nsTextFrame* aTextFrame)
{
  nsRefPtr<gfxContext> ctx = aContext;
  if (!ctx) {
    ctx = GetReferenceRenderingContext(aTextFrame, nullptr);
  }
  if (!ctx)
    return nullptr;

  return aTextRun->GetFontGroup()->
    MakeHyphenTextRun(ctx, aTextRun->GetAppUnitsPerDevUnit());
}

static gfxFont::Metrics
GetFirstFontMetrics(gfxFontGroup* aFontGroup)
{
  if (!aFontGroup)
    return gfxFont::Metrics();
  gfxFont* font = aFontGroup->GetFontAt(0);
  if (!font)
    return gfxFont::Metrics();
  return font->GetMetrics();
}

PR_STATIC_ASSERT(NS_STYLE_WHITESPACE_NORMAL == 0);
PR_STATIC_ASSERT(NS_STYLE_WHITESPACE_PRE == 1);
PR_STATIC_ASSERT(NS_STYLE_WHITESPACE_NOWRAP == 2);
PR_STATIC_ASSERT(NS_STYLE_WHITESPACE_PRE_WRAP == 3);
PR_STATIC_ASSERT(NS_STYLE_WHITESPACE_PRE_LINE == 4);
PR_STATIC_ASSERT(NS_STYLE_WHITESPACE_PRE_DISCARD_NEWLINES == 5);

static const nsTextFrameUtils::CompressionMode CSSWhitespaceToCompressionMode[] =
{
  nsTextFrameUtils::COMPRESS_WHITESPACE_NEWLINE, // normal
  nsTextFrameUtils::COMPRESS_NONE,               // pre
  nsTextFrameUtils::COMPRESS_WHITESPACE_NEWLINE, // nowrap
  nsTextFrameUtils::COMPRESS_NONE,               // pre-wrap
  nsTextFrameUtils::COMPRESS_WHITESPACE,         // pre-line
  nsTextFrameUtils::DISCARD_NEWLINE              // -moz-pre-discard-newlines
};

gfxTextRun*
BuildTextRunsScanner::BuildTextRunForFrames(void* aTextBuffer)
{
  gfxSkipChars skipChars;

  const void* textPtr = aTextBuffer;
  bool anySmallcapsStyle = false;
  bool anyTextTransformStyle = false;
  bool anyMathMLStyling = false;
  uint8_t sstyScriptLevel = 0;
  uint32_t textFlags = nsTextFrameUtils::TEXT_NO_BREAKS;

  if (mCurrentRunContextInfo & nsTextFrameUtils::INCOMING_WHITESPACE) {
    textFlags |= nsTextFrameUtils::TEXT_INCOMING_WHITESPACE;
  }
  if (mCurrentRunContextInfo & nsTextFrameUtils::INCOMING_ARABICCHAR) {
    textFlags |= gfxTextRunFactory::TEXT_INCOMING_ARABICCHAR;
  }

  nsAutoTArray<int32_t,50> textBreakPoints;
  TextRunUserData dummyData;
  TextRunMappedFlow dummyMappedFlow;

  TextRunUserData* userData;
  TextRunUserData* userDataToDestroy;
  // If the situation is particularly simple (and common) we don't need to
  // allocate userData.
  if (mMappedFlows.Length() == 1 && !mMappedFlows[0].mEndFrame &&
      mMappedFlows[0].mStartFrame->GetContentOffset() == 0) {
    userData = &dummyData;
    userDataToDestroy = nullptr;
    dummyData.mMappedFlows = &dummyMappedFlow;
  } else {
    userData = static_cast<TextRunUserData*>
      (nsMemory::Alloc(sizeof(TextRunUserData) + mMappedFlows.Length()*sizeof(TextRunMappedFlow)));
    userDataToDestroy = userData;
    userData->mMappedFlows = reinterpret_cast<TextRunMappedFlow*>(userData + 1);
  }
  userData->mMappedFlowCount = mMappedFlows.Length();
  userData->mLastFlowIndex = 0;

  uint32_t currentTransformedTextOffset = 0;

  uint32_t nextBreakIndex = 0;
  nsTextFrame* nextBreakBeforeFrame = GetNextBreakBeforeFrame(&nextBreakIndex);
  bool enabledJustification = mLineContainer &&
    (mLineContainer->StyleText()->mTextAlign == NS_STYLE_TEXT_ALIGN_JUSTIFY ||
     mLineContainer->StyleText()->mTextAlignLast == NS_STYLE_TEXT_ALIGN_JUSTIFY) &&
    !mLineContainer->IsSVGText();

  // for word-break style
  switch (mLineContainer->StyleText()->mWordBreak) {
    case NS_STYLE_WORDBREAK_BREAK_ALL:
      mLineBreaker.SetWordBreak(nsILineBreaker::kWordBreak_BreakAll);
      break;
    case NS_STYLE_WORDBREAK_KEEP_ALL:
      mLineBreaker.SetWordBreak(nsILineBreaker::kWordBreak_KeepAll);
      break;
    default:
      mLineBreaker.SetWordBreak(nsILineBreaker::kWordBreak_Normal);
      break;
  }

  const nsStyleText* textStyle = nullptr;
  const nsStyleFont* fontStyle = nullptr;
  nsStyleContext* lastStyleContext = nullptr;
  for (uint32_t i = 0; i < mMappedFlows.Length(); ++i) {
    MappedFlow* mappedFlow = &mMappedFlows[i];
    nsTextFrame* f = mappedFlow->mStartFrame;

    lastStyleContext = f->StyleContext();
    // Detect use of text-transform or font-variant anywhere in the run
    textStyle = f->StyleText();
    if (NS_STYLE_TEXT_TRANSFORM_NONE != textStyle->mTextTransform) {
      anyTextTransformStyle = true;
    }
    textFlags |= GetSpacingFlags(LetterSpacing(f));
    textFlags |= GetSpacingFlags(WordSpacing(f));
    nsTextFrameUtils::CompressionMode compression =
      CSSWhitespaceToCompressionMode[textStyle->mWhiteSpace];
    if (enabledJustification && !textStyle->WhiteSpaceIsSignificant()) {
      textFlags |= gfxTextRunFactory::TEXT_ENABLE_SPACING;
    }
    fontStyle = f->StyleFont();
    if (NS_STYLE_FONT_VARIANT_SMALL_CAPS == fontStyle->mFont.variant) {
      anySmallcapsStyle = true;
    }
    if (NS_MATHML_MATHVARIANT_NONE != fontStyle->mMathVariant) {
      anyMathMLStyling = true;
    } else if (mLineContainer->GetStateBits() & NS_FRAME_IS_IN_SINGLE_CHAR_MI) {
      textFlags |= nsTextFrameUtils::TEXT_IS_SINGLE_CHAR_MI;
      anyMathMLStyling = true;
    }
    nsIFrame* parent = mLineContainer->GetParent();
    if (mLineContainer->HasAnyStateBits(TEXT_IS_IN_TOKEN_MATHML)) {
      // All MathML tokens except <mtext> use 'math' script.
      if (!(parent && parent->GetContent() &&
          parent->GetContent()->Tag() == nsGkAtoms::mtext_)) {
        textFlags |= gfxTextRunFactory::TEXT_USE_MATH_SCRIPT;
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

    TextRunMappedFlow* newFlow = &userData->mMappedFlows[i];
    newFlow->mStartFrame = mappedFlow->mStartFrame;
    newFlow->mDOMOffsetToBeforeTransformOffset = skipChars.GetOriginalCharCount() -
      mappedFlow->mStartFrame->GetContentOffset();
    newFlow->mContentLength = contentLength;

    while (nextBreakBeforeFrame && nextBreakBeforeFrame->GetContent() == content) {
      textBreakPoints.AppendElement(
          nextBreakBeforeFrame->GetContentOffset() + newFlow->mDOMOffsetToBeforeTransformOffset);
      nextBreakBeforeFrame = GetNextBreakBeforeFrame(&nextBreakIndex);
    }

    uint32_t analysisFlags;
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
        AutoFallibleTArray<uint8_t,BIG_TEXT_NODE_SIZE> tempBuf;
        uint8_t* bufStart = tempBuf.AppendElements(contentLength);
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
    textFlags |= analysisFlags;
  }

  void* finalUserData;
  if (userData == &dummyData) {
    textFlags |= nsTextFrameUtils::TEXT_IS_SIMPLE_FLOW;
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

  if (textFlags & nsTextFrameUtils::TEXT_HAS_TAB) {
    textFlags |= gfxTextRunFactory::TEXT_ENABLE_SPACING;
  }
  if (textFlags & nsTextFrameUtils::TEXT_HAS_SHY) {
    textFlags |= gfxTextRunFactory::TEXT_ENABLE_HYPHEN_BREAKS;
  }
  if (mBidiEnabled && (NS_GET_EMBEDDING_LEVEL(firstFrame) & 1)) {
    textFlags |= gfxTextRunFactory::TEXT_IS_RTL;
  }
  if (mNextRunContextInfo & nsTextFrameUtils::INCOMING_WHITESPACE) {
    textFlags |= nsTextFrameUtils::TEXT_TRAILING_WHITESPACE;
  }
  if (mNextRunContextInfo & nsTextFrameUtils::INCOMING_ARABICCHAR) {
    textFlags |= gfxTextRunFactory::TEXT_TRAILING_ARABICCHAR;
  }
  // ContinueTextRunAcrossFrames guarantees that it doesn't matter which
  // frame's style is used, so we use a mixture of the first frame and
  // last frame's style
  textFlags |= nsLayoutUtils::GetTextRunFlagsForStyle(lastStyleContext,
      fontStyle, LetterSpacing(firstFrame, textStyle));
  // XXX this is a bit of a hack. For performance reasons, if we're favouring
  // performance over quality, don't try to get accurate glyph extents.
  if (!(textFlags & gfxTextRunFactory::TEXT_OPTIMIZE_SPEED)) {
    textFlags |= gfxTextRunFactory::TEXT_NEED_BOUNDING_BOX;
  }

  // Convert linebreak coordinates to transformed string offsets
  NS_ASSERTION(nextBreakIndex == mLineBreakBeforeFrames.Length(),
               "Didn't find all the frames to break-before...");
  gfxSkipCharsIterator iter(skipChars);
  nsAutoTArray<uint32_t,50> textBreakPointsAfterTransform;
  for (uint32_t i = 0; i < textBreakPoints.Length(); ++i) {
    nsTextFrameUtils::AppendLineBreakOffset(&textBreakPointsAfterTransform, 
            iter.ConvertOriginalToSkipped(textBreakPoints[i]));
  }
  if (mStartOfLine) {
    nsTextFrameUtils::AppendLineBreakOffset(&textBreakPointsAfterTransform,
                                            transformedLength);
  }

  // Setup factory chain
  nsAutoPtr<nsTransformingTextRunFactory> transformingFactory;
  if (anySmallcapsStyle) {
    transformingFactory = new nsFontVariantTextRunFactory();
  }
  if (anyTextTransformStyle) {
    transformingFactory =
      new nsCaseTransformTextRunFactory(transformingFactory.forget());
  }
  if (anyMathMLStyling) {
    transformingFactory =
      new MathMLTextRunFactory(transformingFactory.forget(), sstyScriptLevel);
  }
  nsTArray<nsStyleContext*> styles;
  if (transformingFactory) {
    iter.SetOriginalOffset(0);
    for (uint32_t i = 0; i < mMappedFlows.Length(); ++i) {
      MappedFlow* mappedFlow = &mMappedFlows[i];
      nsTextFrame* f;
      for (f = mappedFlow->mStartFrame; f != mappedFlow->mEndFrame;
           f = static_cast<nsTextFrame*>(f->GetNextContinuation())) {
        uint32_t offset = iter.GetSkippedOffset();
        iter.AdvanceOriginal(f->GetContentLength());
        uint32_t end = iter.GetSkippedOffset();
        nsStyleContext* sc = f->StyleContext();
        uint32_t j;
        for (j = offset; j < end; ++j) {
          styles.AppendElement(sc);
        }
      }
    }
    textFlags |= nsTextFrameUtils::TEXT_IS_TRANSFORMED;
    NS_ASSERTION(iter.GetSkippedOffset() == transformedLength,
                 "We didn't cover all the characters in the text run!");
  }

  gfxTextRun* textRun;
  gfxTextRunFactory::Parameters params =
      { mContext, finalUserData, &skipChars,
        textBreakPointsAfterTransform.Elements(),
        textBreakPointsAfterTransform.Length(),
        int32_t(firstFrame->PresContext()->AppUnitsPerDevPixel())};

  if (mDoubleByteText) {
    const char16_t* text = static_cast<const char16_t*>(textPtr);
    if (transformingFactory) {
      textRun = transformingFactory->MakeTextRun(text, transformedLength, &params,
                                                 fontGroup, textFlags, styles.Elements());
      if (textRun) {
        // ownership of the factory has passed to the textrun
        transformingFactory.forget();
      }
    } else {
      textRun = MakeTextRun(text, transformedLength, fontGroup, &params, textFlags);
    }
  } else {
    const uint8_t* text = static_cast<const uint8_t*>(textPtr);
    textFlags |= gfxFontGroup::TEXT_IS_8BIT;
    if (transformingFactory) {
      textRun = transformingFactory->MakeTextRun(text, transformedLength, &params,
                                                 fontGroup, textFlags, styles.Elements());
      if (textRun) {
        // ownership of the factory has passed to the textrun
        transformingFactory.forget();
      }
    } else {
      textRun = MakeTextRun(text, transformedLength, fontGroup, &params, textFlags);
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
  uint32_t flags = 0;
  if (mDoubleByteText) {
    flags |= SBS_DOUBLE_BYTE;
  }
  if (mSkipIncompleteTextRuns) {
    flags |= SBS_SUPPRESS_SINK;
  }
  SetupBreakSinksForTextRun(textRun, textPtr, flags);

  if (mSkipIncompleteTextRuns) {
    mSkipIncompleteTextRuns = !TextContainsLineBreakerWhiteSpace(textPtr,
        transformedLength, mDoubleByteText);
    // Arrange for this textrun to be deleted the next time the linebreaker
    // is flushed out
    mTextRunsToDelete.AppendElement(textRun);
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
  AssignTextRun(textRun, fontInflation);
  return textRun;
}

// This is a cut-down version of BuildTextRunForFrames used to set up
// context for the line-breaker, when the textrun has already been created.
// So it does the same walk over the mMappedFlows, but doesn't actually
// build a new textrun.
bool
BuildTextRunsScanner::SetupLineBreakerContext(gfxTextRun *aTextRun)
{
  AutoFallibleTArray<uint8_t,BIG_TEXT_NODE_SIZE> buffer;
  uint32_t bufferSize = mMaxTextLength*(mDoubleByteText ? 2 : 1);
  if (bufferSize < mMaxTextLength || bufferSize == UINT32_MAX) {
    return false;
  }
  void *textPtr = buffer.AppendElements(bufferSize);
  if (!textPtr) {
    return false;
  }

  gfxSkipChars skipChars;

  nsAutoTArray<int32_t,50> textBreakPoints;
  TextRunUserData dummyData;
  TextRunMappedFlow dummyMappedFlow;

  TextRunUserData* userData;
  TextRunUserData* userDataToDestroy;
  // If the situation is particularly simple (and common) we don't need to
  // allocate userData.
  if (mMappedFlows.Length() == 1 && !mMappedFlows[0].mEndFrame &&
      mMappedFlows[0].mStartFrame->GetContentOffset() == 0) {
    userData = &dummyData;
    userDataToDestroy = nullptr;
    dummyData.mMappedFlows = &dummyMappedFlow;
  } else {
    userData = static_cast<TextRunUserData*>
      (nsMemory::Alloc(sizeof(TextRunUserData) + mMappedFlows.Length()*sizeof(TextRunMappedFlow)));
    userDataToDestroy = userData;
    userData->mMappedFlows = reinterpret_cast<TextRunMappedFlow*>(userData + 1);
  }
  userData->mMappedFlowCount = mMappedFlows.Length();
  userData->mLastFlowIndex = 0;

  uint32_t nextBreakIndex = 0;
  nsTextFrame* nextBreakBeforeFrame = GetNextBreakBeforeFrame(&nextBreakIndex);

  const nsStyleText* textStyle = nullptr;
  for (uint32_t i = 0; i < mMappedFlows.Length(); ++i) {
    MappedFlow* mappedFlow = &mMappedFlows[i];
    nsTextFrame* f = mappedFlow->mStartFrame;

    textStyle = f->StyleText();
    nsTextFrameUtils::CompressionMode compression =
      CSSWhitespaceToCompressionMode[textStyle->mWhiteSpace];

    // Figure out what content is included in this flow.
    nsIContent* content = f->GetContent();
    const nsTextFragment* frag = content->GetText();
    int32_t contentStart = mappedFlow->mStartFrame->GetContentOffset();
    int32_t contentEnd = mappedFlow->GetContentEnd();
    int32_t contentLength = contentEnd - contentStart;

    TextRunMappedFlow* newFlow = &userData->mMappedFlows[i];
    newFlow->mStartFrame = mappedFlow->mStartFrame;
    newFlow->mDOMOffsetToBeforeTransformOffset = skipChars.GetOriginalCharCount() -
      mappedFlow->mStartFrame->GetContentOffset();
    newFlow->mContentLength = contentLength;

    while (nextBreakBeforeFrame && nextBreakBeforeFrame->GetContent() == content) {
      textBreakPoints.AppendElement(
          nextBreakBeforeFrame->GetContentOffset() + newFlow->mDOMOffsetToBeforeTransformOffset);
      nextBreakBeforeFrame = GetNextBreakBeforeFrame(&nextBreakIndex);
    }

    uint32_t analysisFlags;
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
        AutoFallibleTArray<uint8_t,BIG_TEXT_NODE_SIZE> tempBuf;
        uint8_t* bufStart = tempBuf.AppendElements(contentLength);
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
  uint32_t flags = 0;
  if (mDoubleByteText) {
    flags |= SBS_DOUBLE_BYTE;
  }
  if (mSkipIncompleteTextRuns) {
    flags |= SBS_SUPPRESS_SINK;
  }
  SetupBreakSinksForTextRun(aTextRun, buffer.Elements(), flags);

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
                                                const void* aTextPtr,
                                                uint32_t    aFlags)
{
  // textruns have uniform language
  const nsStyleFont *styleFont = mMappedFlows[0].mStartFrame->StyleFont();
  // We should only use a language for hyphenation if it was specified
  // explicitly.
  nsIAtom* hyphenationLanguage =
    styleFont->mExplicitLanguage ? styleFont->mLanguage : nullptr;
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

    nsAutoPtr<BreakSink>* breakSink = mBreakSinks.AppendElement(
      new BreakSink(aTextRun, mContext, offset,
                    (aFlags & SBS_EXISTING_TEXTRUN) != 0));
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
    if (aTextRun->GetFlags() & nsTextFrameUtils::TEXT_NO_BREAKS) {
      flags |= nsLineBreaker::BREAK_SKIP_SETTING_NO_BREAKS;
    }
    if (textStyle->mTextTransform == NS_STYLE_TEXT_TRANSFORM_CAPITALIZE) {
      flags |= nsLineBreaker::BREAK_NEED_CAPITALIZATION;
    }
    if (textStyle->mHyphens == NS_STYLE_HYPHENS_AUTO) {
      flags |= nsLineBreaker::BREAK_USE_AUTO_HYPHENATION;
    }

    if (HasCompressedLeadingWhitespace(startFrame, textStyle,
                                       mappedFlow->GetContentEnd(), iter)) {
      mLineBreaker.AppendInvisibleWhitespace(flags);
    }

    if (length > 0) {
      BreakSink* sink =
        (aFlags & SBS_SUPPRESS_SINK) ? nullptr : (*breakSink).get();
      if (aFlags & SBS_DOUBLE_BYTE) {
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

// Find the flow corresponding to aContent in aUserData
static inline TextRunMappedFlow*
FindFlowForContent(TextRunUserData* aUserData, nsIContent* aContent)
{
  // Find the flow that contains us
  int32_t i = aUserData->mLastFlowIndex;
  int32_t delta = 1;
  int32_t sign = 1;
  // Search starting at the current position and examine close-by
  // positions first, moving further and further away as we go.
  while (i >= 0 && uint32_t(i) < aUserData->mMappedFlowCount) {
    TextRunMappedFlow* flow = &aUserData->mMappedFlows[i];
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
      TextRunMappedFlow* flow = &aUserData->mMappedFlows[i];
      if (flow->mStartFrame->GetContent() == aContent) {
        return flow;
      }
    }
  } else {
    for (; i >= 0; --i) {
      TextRunMappedFlow* flow = &aUserData->mMappedFlows[i];
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
    for (f = startFrame; f != endFrame;
         f = static_cast<nsTextFrame*>(f->GetNextContinuation())) {
#ifdef DEBUG_roc
      if (f->GetTextRun(mWhichTextRun)) {
        gfxTextRun* textRun = f->GetTextRun(mWhichTextRun);
        if (textRun->GetFlags() & nsTextFrameUtils::TEXT_IS_SIMPLE_FLOW) {
          if (mMappedFlows[0].mStartFrame != static_cast<nsTextFrame*>(textRun->GetUserData())) {
            NS_WARNING("REASSIGNING SIMPLE FLOW TEXT RUN!");
          }
        } else {
          TextRunUserData* userData =
            static_cast<TextRunUserData*>(textRun->GetUserData());
         
          if (userData->mMappedFlowCount >= mMappedFlows.Length() ||
              userData->mMappedFlows[userData->mMappedFlowCount - 1].mStartFrame !=
              mMappedFlows[userData->mMappedFlowCount - 1].mStartFrame) {
            NS_WARNING("REASSIGNING MULTIFLOW TEXT RUN (not append)!");
          }
        }
      }
#endif

      gfxTextRun* oldTextRun = f->GetTextRun(mWhichTextRun);
      if (oldTextRun) {
        nsTextFrame* firstFrame = nullptr;
        uint32_t startOffset = 0;
        if (oldTextRun->GetFlags() & nsTextFrameUtils::TEXT_IS_SIMPLE_FLOW) {
          firstFrame = static_cast<nsTextFrame*>(oldTextRun->GetUserData());
        }
        else {
          TextRunUserData* userData = static_cast<TextRunUserData*>(oldTextRun->GetUserData());
          firstFrame = userData->mMappedFlows[0].mStartFrame;
          if (MOZ_UNLIKELY(f != firstFrame)) {
            TextRunMappedFlow* flow = FindFlowForContent(userData, f->GetContent());
            if (flow) {
              startOffset = flow->mDOMOffsetToBeforeTransformOffset;
            }
            else {
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
NS_QUERYFRAME_TAIL_INHERITING(nsTextFrameBase)

gfxSkipCharsIterator
nsTextFrame::EnsureTextRun(TextRunType aWhichTextRun,
                           gfxContext* aReferenceContext,
                           nsIFrame* aLineContainer,
                           const nsLineList::iterator* aLine,
                           uint32_t* aFlowEndInTextRun)
{
  gfxTextRun *textRun = GetTextRun(aWhichTextRun);
  if (textRun && (!aLine || !(*aLine)->GetInvalidateTextRuns())) {
    if (textRun->GetExpirationState()->IsTracked()) {
      gTextRuns->MarkUsed(textRun);
    }
  } else {
    nsRefPtr<gfxContext> ctx = aReferenceContext;
    if (!ctx) {
      ctx = GetReferenceRenderingContext(this, nullptr);
    }
    if (ctx) {
      BuildTextRuns(ctx, this, aLineContainer, aLine, aWhichTextRun);
    }
    textRun = GetTextRun(aWhichTextRun);
    if (!textRun) {
      // A text run was not constructed for this frame. This is bad. The caller
      // will check mTextRun.
      static const gfxSkipChars emptySkipChars;
      return gfxSkipCharsIterator(emptySkipChars, 0);
    }
    TabWidthStore* tabWidths =
      static_cast<TabWidthStore*>(Properties().Get(TabWidthProperty()));
    if (tabWidths && tabWidths->mValidForContentOffset != GetContentOffset()) {
      Properties().Delete(TabWidthProperty());
    }
  }

  if (textRun->GetFlags() & nsTextFrameUtils::TEXT_IS_SIMPLE_FLOW) {
    if (aFlowEndInTextRun) {
      *aFlowEndInTextRun = textRun->GetLength();
    }
    return gfxSkipCharsIterator(textRun->GetSkipChars(), 0, mContentOffset);
  }

  TextRunUserData* userData = static_cast<TextRunUserData*>(textRun->GetUserData());
  TextRunMappedFlow* flow = FindFlowForContent(userData, mContent);
  if (flow) {
    // Since textruns can only contain one flow for a given content element,
    // this must be our flow.
    uint32_t flowIndex = flow - userData->mMappedFlows;
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
  static const gfxSkipChars emptySkipChars;
  return gfxSkipCharsIterator(emptySkipChars, 0);
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
                               bool aTrimAfter, bool aPostReflow)
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

/*
 * Currently only Unicode characters below 0x10000 have their spacing modified
 * by justification. If characters above 0x10000 turn out to need
 * justification spacing, that will require extra work. Currently,
 * this function must not include 0xd800 to 0xdbff because these characters
 * are surrogates.
 */
static bool IsJustifiableCharacter(const nsTextFragment* aFrag, int32_t aPos,
                                     bool aLangIsCJ)
{
  char16_t ch = aFrag->CharAt(aPos);
  if (ch == '\n' || ch == '\t' || ch == '\r')
    return true;
  if (ch == ' ' || ch == CH_NBSP) {
    // Don't justify spaces that are combined with diacriticals
    if (!aFrag->Is2b())
      return true;
    return !nsTextFrameUtils::IsSpaceCombiningSequenceTail(
        aFrag->Get2b() + aPos + 1, aFrag->GetLength() - (aPos + 1));
  }
  if (ch < 0x2150u)
    return false;
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
    return true;
  return false;
}

void
nsTextFrame::ClearMetrics(nsHTMLReflowMetrics& aMetrics)
{
  aMetrics.Width() = 0;
  aMetrics.Height() = 0;
  aMetrics.SetTopAscent(0);
  mAscent = 0;
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

static bool IsChineseOrJapanese(nsIFrame* aFrame)
{
  nsIAtom* language = aFrame->StyleFont()->mLanguage;
  if (!language) {
    return false;
  }
  const char16_t *lang = language->GetUTF16String();
  return (!nsCRT::strncmp(lang, MOZ_UTF16("ja"), 2) ||
          !nsCRT::strncmp(lang, MOZ_UTF16("zh"), 2)) &&
         (language->GetLength() == 2 || lang[2] == '-');
}

#ifdef DEBUG
static bool IsInBounds(const gfxSkipCharsIterator& aStart, int32_t aContentLength,
                         uint32_t aOffset, uint32_t aLength) {
  if (aStart.GetSkippedOffset() > aOffset)
    return false;
  if (aContentLength == INT32_MAX)
    return true;
  gfxSkipCharsIterator iter(aStart);
  iter.AdvanceOriginal(aContentLength);
  return iter.GetSkippedOffset() >= aOffset + aLength;
}
#endif

class MOZ_STACK_CLASS PropertyProvider : public gfxTextRun::PropertyProvider {
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
      mWordSpacing(WordSpacing(aFrame, aTextStyle)),
      mLetterSpacing(LetterSpacing(aFrame, aTextStyle)),
      mJustificationSpacing(0),
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
      mWordSpacing(WordSpacing(aFrame)),
      mLetterSpacing(LetterSpacing(aFrame)),
      mJustificationSpacing(0),
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

  virtual void GetSpacing(uint32_t aStart, uint32_t aLength, Spacing* aSpacing);
  virtual gfxFloat GetHyphenWidth();
  virtual void GetHyphenationBreaks(uint32_t aStart, uint32_t aLength,
                                    bool* aBreakBefore);
  virtual int8_t GetHyphensOption() {
    return mTextStyle->mHyphens;
  }

  void GetSpacingInternal(uint32_t aStart, uint32_t aLength, Spacing* aSpacing,
                          bool aIgnoreTabs);

  /**
   * Count the number of justifiable characters in the given DOM range
   */
  uint32_t ComputeJustifiableCharacters(int32_t aOffset, int32_t aLength);
  /**
   * Find the start and end of the justifiable characters. Does not depend on the
   * position of aStart or aEnd, although it's most efficient if they are near the
   * start and end of the text frame.
   */
  void FindJustificationRange(gfxSkipCharsIterator* aStart,
                              gfxSkipCharsIterator* aEnd);

  const nsStyleText* StyleText() { return mTextStyle; }
  nsTextFrame* GetFrame() { return mFrame; }
  // This may not be equal to the frame offset/length in because we may have
  // adjusted for whitespace trimming according to the state bits set in the frame
  // (for the static provider)
  const gfxSkipCharsIterator& GetStart() { return mStart; }
  // May return INT32_MAX if that was given to the constructor
  uint32_t GetOriginalLength() {
    NS_ASSERTION(mLength != INT32_MAX, "Length not known");
    return mLength;
  }
  const nsTextFragment* GetFragment() { return mFrag; }

  gfxFontGroup* GetFontGroup() {
    if (!mFontGroup)
      InitFontGroupAndFontMetrics();
    return mFontGroup;
  }

  nsFontMetrics* GetFontMetrics() {
    if (!mFontMetrics)
      InitFontGroupAndFontMetrics();
    return mFontMetrics;
  }

  void CalcTabWidths(uint32_t aTransformedStart, uint32_t aTransformedLength);

  const gfxSkipCharsIterator& GetEndHint() { return mTempIterator; }

protected:
  void SetupJustificationSpacing();

  void InitFontGroupAndFontMetrics() {
    float inflation = (mWhichTextRun == nsTextFrame::eInflated)
      ? mFrame->GetFontSizeInflation() : 1.0f;
    mFontGroup = GetFontGroupForFrame(mFrame, inflation,
                                      getter_AddRefs(mFontMetrics));
  }

  gfxTextRun*           mTextRun;
  gfxFontGroup*         mFontGroup;
  nsRefPtr<nsFontMetrics> mFontMetrics;
  const nsStyleText*    mTextStyle;
  const nsTextFragment* mFrag;
  nsIFrame*             mLineContainer;
  nsTextFrame*          mFrame;
  gfxSkipCharsIterator  mStart;  // Offset in original and transformed string
  gfxSkipCharsIterator  mTempIterator;
  
  // Either null, or pointing to the frame's TabWidthProperty.
  TabWidthStore*        mTabWidths;
  // How far we've done tab-width calculation; this is ONLY valid when
  // mTabWidths is nullptr (otherwise rely on mTabWidths->mLimit instead).
  // It's a DOM offset relative to the current frame's offset.
  uint32_t              mTabWidthsAnalyzedLimit;

  int32_t               mLength; // DOM string length, may be INT32_MAX
  gfxFloat              mWordSpacing;     // space for each whitespace char
  gfxFloat              mLetterSpacing;   // space for each letter
  gfxFloat              mJustificationSpacing;
  gfxFloat              mHyphenWidth;
  gfxFloat              mOffsetFromBlockOriginForTabs;
  bool                  mReflowing;
  nsTextFrame::TextRunType mWhichTextRun;
};

uint32_t
PropertyProvider::ComputeJustifiableCharacters(int32_t aOffset, int32_t aLength)
{
  // Scan non-skipped characters and count justifiable chars.
  nsSkipCharsRunIterator
    run(mStart, nsSkipCharsRunIterator::LENGTH_INCLUDES_SKIPPED, aLength);
  run.SetOriginalOffset(aOffset);
  uint32_t justifiableChars = 0;
  bool isCJK = IsChineseOrJapanese(mFrame);
  while (run.NextRun()) {
    for (int32_t i = 0; i < run.GetRunLength(); ++i) {
      justifiableChars +=
        IsJustifiableCharacter(mFrag, run.GetOriginalOffset() + i, isCJK);
    }
  }
  return justifiableChars;
}

/**
 * Finds the offset of the first character of the cluster containing aPos
 */
static void FindClusterStart(gfxTextRun* aTextRun, int32_t aOriginalStart,
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
 * Finds the offset of the last character of the cluster containing aPos
 */
static void FindClusterEnd(gfxTextRun* aTextRun, int32_t aOriginalEnd,
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
PropertyProvider::GetSpacing(uint32_t aStart, uint32_t aLength,
                             Spacing* aSpacing)
{
  GetSpacingInternal(aStart, aLength, aSpacing,
                     (mTextRun->GetFlags() & nsTextFrameUtils::TEXT_HAS_TAB) == 0);
}

static bool
CanAddSpacingAfter(gfxTextRun* aTextRun, uint32_t aOffset)
{
  if (aOffset + 1 >= aTextRun->GetLength())
    return true;
  return aTextRun->IsClusterStart(aOffset + 1) &&
    aTextRun->IsLigatureGroupStart(aOffset + 1);
}

void
PropertyProvider::GetSpacingInternal(uint32_t aStart, uint32_t aLength,
                                     Spacing* aSpacing, bool aIgnoreTabs)
{
  NS_PRECONDITION(IsInBounds(mStart, mLength, aStart, aLength), "Range out of bounds");

  uint32_t index;
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
      uint32_t runOffsetInSubstring = run.GetSkippedOffset() - aStart;
      gfxSkipCharsIterator iter = run.GetPos();
      for (int32_t i = 0; i < run.GetRunLength(); ++i) {
        if (CanAddSpacingAfter(mTextRun, run.GetSkippedOffset() + i)) {
          // End of a cluster, not in a ligature: put letter-spacing after it
          aSpacing[runOffsetInSubstring + i].mAfter += mLetterSpacing;
        }
        if (IsCSSWordSpacingSpace(mFrag, i + run.GetOriginalOffset(),
                                  mTextStyle)) {
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

  // Ignore tab spacing rather than computing it, if the tab size is 0
  if (!aIgnoreTabs)
    aIgnoreTabs = mFrame->StyleText()->mTabSize == 0;

  // Now add tab spacing, if there is any
  if (!aIgnoreTabs) {
    CalcTabWidths(aStart, aLength);
    if (mTabWidths) {
      mTabWidths->ApplySpacing(aSpacing,
                               aStart - mStart.GetSkippedOffset(), aLength);
    }
  }

  // Now add in justification spacing
  if (mJustificationSpacing) {
    gfxFloat halfJustificationSpace = mJustificationSpacing/2;
    // Scan non-skipped characters and adjust justifiable chars, adding
    // justification space on either side of the cluster
    bool isCJK = IsChineseOrJapanese(mFrame);
    gfxSkipCharsIterator justificationStart(mStart), justificationEnd(mStart);
    FindJustificationRange(&justificationStart, &justificationEnd);

    nsSkipCharsRunIterator
      run(start, nsSkipCharsRunIterator::LENGTH_UNSKIPPED_ONLY, aLength);
    while (run.NextRun()) {
      gfxSkipCharsIterator iter = run.GetPos();
      int32_t runOriginalOffset = run.GetOriginalOffset();
      for (int32_t i = 0; i < run.GetRunLength(); ++i) {
        int32_t iterOriginalOffset = runOriginalOffset + i;
        if (IsJustifiableCharacter(mFrag, iterOriginalOffset, isCJK)) {
          iter.SetOriginalOffset(iterOriginalOffset);
          FindClusterStart(mTextRun, runOriginalOffset, &iter);
          uint32_t clusterFirstChar = iter.GetSkippedOffset();
          FindClusterEnd(mTextRun, runOriginalOffset + run.GetRunLength(), &iter);
          uint32_t clusterLastChar = iter.GetSkippedOffset();
          // Only apply justification to characters before justificationEnd
          if (clusterFirstChar >= justificationStart.GetSkippedOffset() &&
              clusterLastChar < justificationEnd.GetSkippedOffset()) {
            aSpacing[clusterFirstChar - aStart].mBefore += halfJustificationSpace;
            aSpacing[clusterLastChar - aStart].mAfter += halfJustificationSpace;
          }
        }
      }
    }
  }
}

static gfxFloat
ComputeTabWidthAppUnits(nsIFrame* aFrame, gfxTextRun* aTextRun)
{
  // Get the number of spaces from CSS -moz-tab-size
  const nsStyleText* textStyle = aFrame->StyleText();
  
  // Round the space width when converting to appunits the same way
  // textruns do
  gfxFloat spaceWidthAppUnits =
    NS_round(GetFirstFontMetrics(aTextRun->GetFontGroup()).spaceWidth *
              aTextRun->GetAppUnitsPerDevUnit());
  return textStyle->mTabSize * spaceWidthAppUnits;
}

// aX and the result are in whole appunits.
static gfxFloat
AdvanceToNextTab(gfxFloat aX, nsIFrame* aFrame,
                 gfxTextRun* aTextRun, gfxFloat* aCachedTabWidth)
{
  if (*aCachedTabWidth < 0) {
    *aCachedTabWidth = ComputeTabWidthAppUnits(aFrame, aTextRun);
  }

  // Advance aX to the next multiple of *aCachedTabWidth. We must advance
  // by at least 1 appunit.
  // XXX should we make this 1 CSS pixel?
  return ceil((aX + 1)/(*aCachedTabWidth))*(*aCachedTabWidth);
}

void
PropertyProvider::CalcTabWidths(uint32_t aStart, uint32_t aLength)
{
  if (!mTabWidths) {
    if (mReflowing && !mLineContainer) {
      // Intrinsic width computation does its own tab processing. We
      // just don't do anything here.
      return;
    }
    if (!mReflowing) {
      mTabWidths = static_cast<TabWidthStore*>
        (mFrame->Properties().Get(TabWidthProperty()));
#ifdef DEBUG
      // If we're not reflowing, we should have already computed the
      // tab widths; check that they're available as far as the last
      // tab character present (if any)
      for (uint32_t i = aStart + aLength; i > aStart; --i) {
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
  MOZ_ASSERT(aStart >= startOffset, "wrong start offset");
  MOZ_ASSERT(aStart + aLength <= startOffset + mLength, "beyond the end");
  uint32_t tabsEnd =
    (mTabWidths ? mTabWidths->mLimit : mTabWidthsAnalyzedLimit) + startOffset;
  if (tabsEnd < aStart + aLength) {
    NS_ASSERTION(mReflowing,
                 "We need precomputed tab widths, but don't have enough.");

    gfxFloat tabWidth = -1;
    for (uint32_t i = tabsEnd; i < aStart + aLength; ++i) {
      Spacing spacing;
      GetSpacingInternal(i, 1, &spacing, true);
      mOffsetFromBlockOriginForTabs += spacing.mBefore;

      if (!mTextRun->CharIsTab(i)) {
        if (mTextRun->IsClusterStart(i)) {
          uint32_t clusterEnd = i + 1;
          while (clusterEnd < mTextRun->GetLength() &&
                 !mTextRun->IsClusterStart(clusterEnd)) {
            ++clusterEnd;
          }
          mOffsetFromBlockOriginForTabs +=
            mTextRun->GetAdvanceWidth(i, clusterEnd - i, nullptr);
        }
      } else {
        if (!mTabWidths) {
          mTabWidths = new TabWidthStore(mFrame->GetContentOffset());
          mFrame->Properties().Set(TabWidthProperty(), mTabWidths);
        }
        double nextTab = AdvanceToNextTab(mOffsetFromBlockOriginForTabs,
                mFrame, mTextRun, &tabWidth);
        mTabWidths->mWidths.AppendElement(TabWidth(i - startOffset, 
                NSToIntRound(nextTab - mOffsetFromBlockOriginForTabs)));
        mOffsetFromBlockOriginForTabs = nextTab;
      }

      mOffsetFromBlockOriginForTabs += spacing.mAfter;
    }

    if (mTabWidths) {
      mTabWidths->mLimit = aStart + aLength - startOffset;
    }
  }

  if (!mTabWidths) {
    // Delete any stale property that may be left on the frame
    mFrame->Properties().Delete(TabWidthProperty());
    mTabWidthsAnalyzedLimit = std::max(mTabWidthsAnalyzedLimit,
                                       aStart + aLength - startOffset);
  }
}

gfxFloat
PropertyProvider::GetHyphenWidth()
{
  if (mHyphenWidth < 0) {
    mHyphenWidth = mLetterSpacing;
    nsRefPtr<gfxContext> context(GetReferenceRenderingContext(GetFrame(),
                                                              nullptr));
    if (context) {
      mHyphenWidth +=
        GetFontGroup()->GetHyphenWidth(context,
                                       mTextRun->GetAppUnitsPerDevUnit());
    }
  }
  return mHyphenWidth;
}

void
PropertyProvider::GetHyphenationBreaks(uint32_t aStart, uint32_t aLength,
                                       bool* aBreakBefore)
{
  NS_PRECONDITION(IsInBounds(mStart, mLength, aStart, aLength), "Range out of bounds");
  NS_PRECONDITION(mLength != INT32_MAX, "Can't call this with undefined length");

  if (!mTextStyle->WhiteSpaceCanWrap(mFrame) ||
      mTextStyle->mHyphens == NS_STYLE_HYPHENS_NONE)
  {
    memset(aBreakBefore, false, aLength*sizeof(bool));
    return;
  }

  // Iterate through the original-string character runs
  nsSkipCharsRunIterator
    run(mStart, nsSkipCharsRunIterator::LENGTH_UNSKIPPED_ONLY, aLength);
  run.SetSkippedOffset(aStart);
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
      int32_t runOffsetInSubstring = run.GetSkippedOffset() - aStart;
      memset(aBreakBefore + runOffsetInSubstring, false, run.GetRunLength()*sizeof(bool));
      // Don't allow hyphen breaks at the start of the line
      aBreakBefore[runOffsetInSubstring] = allowHyphenBreakBeforeNextChar &&
          (!(mFrame->GetStateBits() & TEXT_START_OF_LINE) ||
           run.GetSkippedOffset() > mStart.GetSkippedOffset());
      allowHyphenBreakBeforeNextChar = false;
    }
  }

  if (mTextStyle->mHyphens == NS_STYLE_HYPHENS_AUTO) {
    for (uint32_t i = 0; i < aLength; ++i) {
      if (mTextRun->CanHyphenateBefore(aStart + i)) {
        aBreakBefore[i] = true;
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
  SetupJustificationSpacing();
}

void
PropertyProvider::InitializeForMeasure()
{
  nsTextFrame::TrimmedOffsets trimmed =
    mFrame->GetTrimmedOffsets(mFrag, true, false);
  mStart.SetOriginalOffset(trimmed.mStart);
  mLength = trimmed.mLength;
  SetupJustificationSpacing();
}


static uint32_t GetSkippedDistance(const gfxSkipCharsIterator& aStart,
                                   const gfxSkipCharsIterator& aEnd)
{
  return aEnd.GetSkippedOffset() - aStart.GetSkippedOffset();
}

void
PropertyProvider::FindJustificationRange(gfxSkipCharsIterator* aStart,
                                         gfxSkipCharsIterator* aEnd)
{
  NS_PRECONDITION(mLength != INT32_MAX, "Can't call this with undefined length");
  NS_ASSERTION(aStart && aEnd, "aStart or/and aEnd is null");

  aStart->SetOriginalOffset(mStart.GetOriginalOffset());
  aEnd->SetOriginalOffset(mStart.GetOriginalOffset() + mLength);

  // Ignore first cluster at start of line for justification purposes
  if (mFrame->GetStateBits() & TEXT_START_OF_LINE) {
    while (aStart->GetOriginalOffset() < aEnd->GetOriginalOffset()) {
      aStart->AdvanceOriginal(1);
      if (!aStart->IsOriginalCharSkipped() &&
          mTextRun->IsClusterStart(aStart->GetSkippedOffset()))
        break;
    }
  }

  // Ignore trailing cluster at end of line for justification purposes
  if (mFrame->GetStateBits() & TEXT_END_OF_LINE) {
    while (aEnd->GetOriginalOffset() > aStart->GetOriginalOffset()) {
      aEnd->AdvanceOriginal(-1);
      if (!aEnd->IsOriginalCharSkipped() &&
          mTextRun->IsClusterStart(aEnd->GetSkippedOffset()))
        break;
    }
  }
}

void
PropertyProvider::SetupJustificationSpacing()
{
  NS_PRECONDITION(mLength != INT32_MAX, "Can't call this with undefined length");

  if (!(mFrame->GetStateBits() & TEXT_JUSTIFICATION_ENABLED))
    return;

  gfxSkipCharsIterator start(mStart), end(mStart);
  // We can't just use our mLength here; when InitializeForDisplay is
  // called with false for aTrimAfter, we still shouldn't be assigning
  // justification space to any trailing whitespace.
  nsTextFrame::TrimmedOffsets trimmed =
    mFrame->GetTrimmedOffsets(mFrag, true);
  end.AdvanceOriginal(trimmed.mLength);
  gfxSkipCharsIterator realEnd(end);
  FindJustificationRange(&start, &end);

  int32_t justifiableCharacters =
    ComputeJustifiableCharacters(start.GetOriginalOffset(),
                                 end.GetOriginalOffset() - start.GetOriginalOffset());
  if (justifiableCharacters == 0) {
    // Nothing to do, nothing is justifiable and we shouldn't have any
    // justification space assigned
    return;
  }

  gfxFloat naturalWidth =
    mTextRun->GetAdvanceWidth(mStart.GetSkippedOffset(),
                              GetSkippedDistance(mStart, realEnd), this);
  if (mFrame->GetStateBits() & TEXT_HYPHEN_BREAK) {
    naturalWidth += GetHyphenWidth();
  }
  gfxFloat totalJustificationSpace = mFrame->GetSize().width - naturalWidth;
  if (totalJustificationSpace <= 0) {
    // No space available
    return;
  }
  
  mJustificationSpacing = totalJustificationSpace/justifiableCharacters;
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
  if (mFrame->IsSVGText()) {
    if (!mResolveColors)
      return NS_SAME_AS_FOREGROUND_COLOR;

    const nsStyleSVG* style = mFrame->StyleSVG();
    switch (style->mFill.mType) {
      case eStyleSVGPaintType_None:
        return NS_RGBA(0, 0, 0, 0);
      case eStyleSVGPaintType_Color:
        return nsLayoutUtils::GetColor(mFrame, eCSSProperty_fill);
      default:
        NS_ERROR("cannot resolve SVG paint to nscolor");
        return NS_RGBA(0, 0, 0, 255);
    }
  }
  return nsLayoutUtils::GetColor(mFrame, eCSSProperty_color);
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
  
  nscolor backColor =
    LookAndFeel::GetColor(LookAndFeel::eColorID_TextHighlightBackground);
  nscolor foreColor =
    LookAndFeel::GetColor(LookAndFeel::eColorID_TextHighlightForeground);
  EnsureSufficientContrast(&foreColor, &backColor);
  *aForeColor = foreColor;
  *aBackColor = backColor;
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
  nscolor bgColor =
    bgFrame->GetVisitedDependentColor(eCSSProperty_background_color);

  nscolor defaultBgColor = mPresContext->DefaultBackgroundColor();
  mFrameBackgroundColor = NS_ComposeColors(defaultBgColor, bgColor);

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
    nsRefPtr<nsStyleContext> sc = nullptr;
    sc = mPresContext->StyleSet()->
      ProbePseudoElementStyle(selectionElement,
                              nsCSSPseudoElements::ePseudo_mozSelection,
                              mFrame->StyleContext());
    // Use -moz-selection pseudo class.
    if (sc) {
      mSelectionBGColor =
        sc->GetVisitedDependentColor(eCSSProperty_background_color);
      mSelectionTextColor = sc->GetVisitedDependentColor(eCSSProperty_color);
      mHasSelectionShadow =
        nsRuleNode::HasAuthorSpecifiedRules(sc,
                                            NS_AUTHOR_SPECIFIED_TEXT_SHADOW,
                                            true);
      if (mHasSelectionShadow) {
        mSelectionShadow = sc->StyleText()->mTextShadow;
      }
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
      nsCSSProperty property = mFrame->IsSVGText() ? eCSSProperty_fill :
                                                     eCSSProperty_color;
      nscoord frameColor = mFrame->GetVisitedDependentColor(property);
      mSelectionTextColor = EnsureDifferentColors(frameColor, mSelectionBGColor);
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
    nsAutoString renderedWhitespace;
    GetRenderedText(&renderedWhitespace, nullptr, nullptr, 0, 1);
    if (renderedWhitespace.IsEmpty()) {
      return a11y::eNoType;
    }
  }

  return a11y::eTextLeafType;
}
#endif


//-----------------------------------------------------------------------------
void
nsTextFrame::Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow)
{
  NS_ASSERTION(!aPrevInFlow, "Can't be a continuation!");
  NS_PRECONDITION(aContent->IsNodeOfType(nsINode::eTEXT),
                  "Bogus content!");

  // Remove any NewlineOffsetProperty or InFlowContentLengthProperty since they
  // might be invalid if the content was modified while there was no frame
  aContent->DeleteProperty(nsGkAtoms::newline);
  if (PresContext()->BidiEnabled()) {
    aContent->DeleteProperty(nsGkAtoms::flowlength);
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
      primaryFrame->Properties().Delete(OffsetToFrameProperty());
    }
    RemoveStateBits(TEXT_IN_OFFSET_CACHE);
  }
}

void
nsTextFrame::DestroyFrom(nsIFrame* aDestructRoot)
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
  nsFrame::DestroyFrom(aDestructRoot);
}

class nsContinuingTextFrame : public nsTextFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewContinuingTextFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  virtual void Init(nsIContent*      aContent,
                    nsIFrame*        aParent,
                    nsIFrame*        aPrevInFlow) MOZ_OVERRIDE;

  virtual void DestroyFrom(nsIFrame* aDestructRoot);

  virtual nsIFrame* GetPrevContinuation() const {
    return mPrevContinuation;
  }
  virtual void SetPrevContinuation(nsIFrame* aPrevContinuation) {
    NS_ASSERTION (!aPrevContinuation || GetType() == aPrevContinuation->GetType(),
                  "setting a prev continuation with incorrect type!");
    NS_ASSERTION (!nsSplittableFrame::IsInPrevContinuationChain(aPrevContinuation, this),
                  "creating a loop in continuation chain!");
    mPrevContinuation = aPrevContinuation;
    RemoveStateBits(NS_FRAME_IS_FLUID_CONTINUATION);
  }
  virtual nsIFrame* GetPrevInFlowVirtual() const { return GetPrevInFlow(); }
  nsIFrame* GetPrevInFlow() const {
    return (GetStateBits() & NS_FRAME_IS_FLUID_CONTINUATION) ? mPrevContinuation : nullptr;
  }
  virtual void SetPrevInFlow(nsIFrame* aPrevInFlow) MOZ_OVERRIDE {
    NS_ASSERTION (!aPrevInFlow || GetType() == aPrevInFlow->GetType(),
                  "setting a prev in flow with incorrect type!");
    NS_ASSERTION (!nsSplittableFrame::IsInPrevContinuationChain(aPrevInFlow, this),
                  "creating a loop in continuation chain!");
    mPrevContinuation = aPrevInFlow;
    AddStateBits(NS_FRAME_IS_FLUID_CONTINUATION);
  }
  virtual nsIFrame* FirstInFlow() const MOZ_OVERRIDE;
  virtual nsIFrame* FirstContinuation() const MOZ_OVERRIDE;

  virtual void AddInlineMinWidth(nsRenderingContext *aRenderingContext,
                                 InlineMinWidthData *aData);
  virtual void AddInlinePrefWidth(nsRenderingContext *aRenderingContext,
                                  InlinePrefWidthData *aData);
  
  virtual nsresult GetRenderedText(nsAString* aString = nullptr,
                                   gfxSkipChars* aSkipChars = nullptr,
                                   gfxSkipCharsIterator* aSkipIter = nullptr,
                                   uint32_t aSkippedStartOffset = 0,
                                   uint32_t aSkippedMaxLength = UINT32_MAX)
  { return NS_ERROR_NOT_IMPLEMENTED; } // Call on a primary text frame only

protected:
  nsContinuingTextFrame(nsStyleContext* aContext) : nsTextFrame(aContext) {}
  nsIFrame* mPrevContinuation;
};

void
nsContinuingTextFrame::Init(nsIContent* aContent,
                            nsIFrame*   aParent,
                            nsIFrame*   aPrevInFlow)
{
  NS_ASSERTION(aPrevInFlow, "Must be a continuation!");
  // NOTE: bypassing nsTextFrame::Init!!!
  nsFrame::Init(aContent, aParent, aPrevInFlow);

#ifdef IBMBIDI
  nsTextFrame* nextContinuation =
    static_cast<nsTextFrame*>(aPrevInFlow->GetNextContinuation());
#endif // IBMBIDI
  // Hook the frame into the flow
  SetPrevInFlow(aPrevInFlow);
  aPrevInFlow->SetNextInFlow(this);
  nsTextFrame* prev = static_cast<nsTextFrame*>(aPrevInFlow);
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
#ifdef IBMBIDI
  if (aPrevInFlow->GetStateBits() & NS_FRAME_IS_BIDI) {
    FramePropertyTable *propTable = PresContext()->PropertyTable();
    // Get all the properties from the prev-in-flow first to take
    // advantage of the propTable's cache and simplify the assertion below
    void* embeddingLevel = propTable->Get(aPrevInFlow, EmbeddingLevelProperty());
    void* baseLevel = propTable->Get(aPrevInFlow, BaseLevelProperty());
    void* paragraphDepth = propTable->Get(aPrevInFlow, ParagraphDepthProperty());
    propTable->Set(this, EmbeddingLevelProperty(), embeddingLevel);
    propTable->Set(this, BaseLevelProperty(), baseLevel);
    propTable->Set(this, ParagraphDepthProperty(), paragraphDepth);

    if (nextContinuation) {
      SetNextContinuation(nextContinuation);
      nextContinuation->SetPrevContinuation(this);
      // Adjust next-continuations' content offset as needed.
      while (nextContinuation &&
             nextContinuation->GetContentOffset() < mContentOffset) {
        NS_ASSERTION(
          embeddingLevel == propTable->Get(nextContinuation, EmbeddingLevelProperty()) &&
          baseLevel == propTable->Get(nextContinuation, BaseLevelProperty()) &&
          paragraphDepth == propTable->Get(nextContinuation, ParagraphDepthProperty()),
          "stealing text from different type of BIDI continuation");
        nextContinuation->mContentOffset = mContentOffset;
        nextContinuation = static_cast<nsTextFrame*>(nextContinuation->GetNextContinuation());
      }
    }
    mState |= NS_FRAME_IS_BIDI;
  } // prev frame is bidi
#endif // IBMBIDI
}

void
nsContinuingTextFrame::DestroyFrom(nsIFrame* aDestructRoot)
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
  if ((GetStateBits() & TEXT_IN_TEXTRUN_USER_DATA) ||
      (GetStateBits() & TEXT_IN_UNINFLATED_TEXTRUN_USER_DATA) ||
      (!mPrevContinuation &&
       !(GetStateBits() & TEXT_STYLE_MATCHES_PREV_CONTINUATION)) ||
      (mPrevContinuation &&
       mPrevContinuation->StyleContext() != StyleContext())) {
    ClearTextRuns();
    // Clear the previous continuation's text run also, so that it can rebuild
    // the text run to include our text.
    if (mPrevContinuation) {
      nsTextFrame *prevContinuationText =
        static_cast<nsTextFrame*>(mPrevContinuation);
      prevContinuationText->ClearTextRuns();
    }
  }
  nsSplittableFrame::RemoveFromFlow(this);
  // Let the base class destroy the frame
  nsFrame::DestroyFrom(aDestructRoot);
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
nsTextFrame::GetMinWidth(nsRenderingContext *aRenderingContext)
{
  return nsLayoutUtils::MinWidthFromInline(this, aRenderingContext);
}

// Needed for text frames in XUL.
/* virtual */ nscoord
nsTextFrame::GetPrefWidth(nsRenderingContext *aRenderingContext)
{
  return nsLayoutUtils::PrefWidthFromInline(this, aRenderingContext);
}

/* virtual */ void
nsContinuingTextFrame::AddInlineMinWidth(nsRenderingContext *aRenderingContext,
                                         InlineMinWidthData *aData)
{
  // Do nothing, since the first-in-flow accounts for everything.
  return;
}

/* virtual */ void
nsContinuingTextFrame::AddInlinePrefWidth(nsRenderingContext *aRenderingContext,
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

NS_IMETHODIMP
nsTextFrame::GetCursor(const nsPoint& aPoint,
                       nsIFrame::Cursor& aCursor)
{
  FillCursorInformationFromStyle(StyleUserInterface(), aCursor);  
  if (NS_STYLE_CURSOR_AUTO == aCursor.mCursor) {
    aCursor.mCursor = NS_STYLE_CURSOR_TEXT;
    // If this is editable, we should ignore tabindex value.
    if (mContent->IsEditable()) {
      return NS_OK;
    }

    // If tabindex >= 0, use default cursor to indicate it's not selectable
    nsIFrame *ancestorFrame = this;
    while ((ancestorFrame = ancestorFrame->GetParent()) != nullptr) {
      nsIContent *ancestorContent = ancestorFrame->GetContent();
      if (ancestorContent && ancestorContent->HasAttr(kNameSpaceID_None, nsGkAtoms::tabindex)) {
        nsAutoString tabIndexStr;
        ancestorContent->GetAttr(kNameSpaceID_None, nsGkAtoms::tabindex, tabIndexStr);
        if (!tabIndexStr.IsEmpty()) {
          nsresult rv;
          int32_t tabIndexVal = tabIndexStr.ToInteger(&rv);
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
nsTextFrame::LastInFlow() const
{
  nsTextFrame* lastInFlow = const_cast<nsTextFrame*>(this);
  while (lastInFlow->GetNextInFlow())  {
    lastInFlow = static_cast<nsTextFrame*>(lastInFlow->GetNextInFlow());
  }
  MOZ_ASSERT(lastInFlow, "post-condition failed");
  return lastInFlow;
}

nsIFrame*
nsTextFrame::LastContinuation() const
{
  nsTextFrame* lastContinuation = const_cast<nsTextFrame*>(this);
  while (lastContinuation->mNextContinuation)  {
    lastContinuation =
      static_cast<nsTextFrame*>(lastContinuation->mNextContinuation);
  }
  MOZ_ASSERT(lastContinuation, "post-condition failed");
  return lastContinuation;
}

void
nsTextFrame::InvalidateFrame(uint32_t aDisplayItemKey)
{
  if (IsSVGText()) {
    nsIFrame* svgTextFrame =
      nsLayoutUtils::GetClosestFrameOfType(GetParent(),
                                           nsGkAtoms::svgTextFrame);
    svgTextFrame->InvalidateFrame();
    return;
  }
  nsTextFrameBase::InvalidateFrame(aDisplayItemKey);
}

void
nsTextFrame::InvalidateFrameWithRect(const nsRect& aRect, uint32_t aDisplayItemKey)
{
  if (IsSVGText()) {
    nsIFrame* svgTextFrame =
      nsLayoutUtils::GetClosestFrameOfType(GetParent(),
                                           nsGkAtoms::svgTextFrame);
    svgTextFrame->InvalidateFrame();
    return;
  }
  nsTextFrameBase::InvalidateFrameWithRect(aRect, aDisplayItemKey);
}

gfxTextRun*
nsTextFrame::GetUninflatedTextRun()
{
  return static_cast<gfxTextRun*>(
           Properties().Get(UninflatedTextRunProperty()));
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
    NS_ABORT_IF_FALSE(aInflation == 1.0f, "unexpected inflation");
    if (HasFontSizeInflation()) {
      Properties().Set(UninflatedTextRunProperty(), aTextRun);
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
  FrameProperties props = Properties();
  if ((GetStateBits() & TEXT_HAS_FONT_INFLATION) &&
      props.Get(UninflatedTextRunProperty()) == aTextRun) {
    props.Delete(UninflatedTextRunProperty());
    return true;
  }
  return false;
}

void
nsTextFrame::ClearTextRun(nsTextFrame* aStartContinuation,
                          TextRunType aWhichTextRun)
{
  gfxTextRun* textRun = GetTextRun(aWhichTextRun);
  if (!textRun) {
    return;
  }

  DebugOnly<bool> checkmTextrun = textRun == mTextRun;
  UnhookTextRunFromFrames(textRun, aStartContinuation);
  MOZ_ASSERT(checkmTextrun ? !mTextRun
                           : !Properties().Get(UninflatedTextRunProperty()));

  // see comments in BuildTextRunForFrames...
//  if (textRun->GetFlags() & gfxFontGroup::TEXT_IS_PERSISTENT) {
//    NS_ERROR("Shouldn't reach here for now...");
//    // the textrun's text may be referencing a DOM node that has changed,
//    // so we'd better kill this textrun now.
//    if (textRun->GetExpirationState()->IsTracked()) {
//      gTextRuns->RemoveFromCache(textRun);
//    }
//    delete textRun;
//    return;
//  }

  if (!textRun->GetUserData()) {
    // Remove it now because it's not doing anything useful
    gTextRuns->RemoveFromCache(textRun);
    delete textRun;
  }
}

NS_IMETHODIMP
nsTextFrame::CharacterDataChanged(CharacterDataChangeInfo* aInfo)
{
  mContent->DeleteProperty(nsGkAtoms::newline);
  if (PresContext()->BidiEnabled()) {
    mContent->DeleteProperty(nsGkAtoms::flowlength);
  }

  // Find the first frame whose text has changed. Frames that are entirely
  // before the text change are completely unaffected.
  nsTextFrame* next;
  nsTextFrame* textFrame = this;
  while (true) {
    next = static_cast<nsTextFrame*>(textFrame->GetNextContinuation());
    if (!next || next->GetContentOffset() > int32_t(aInfo->mChangeStart))
      break;
    textFrame = next;
  }

  int32_t endOfChangedText = aInfo->mChangeStart + aInfo->mReplaceLength;
  nsTextFrame* lastDirtiedFrame = nullptr;

  nsIPresShell* shell = PresContext()->GetPresShell();
  do {
    // textFrame contained deleted text (or the insertion point,
    // if this was a pure insertion).
    textFrame->mState &= ~TEXT_WHITESPACE_FLAGS;
    textFrame->ClearTextRuns();
    if (!lastDirtiedFrame ||
        lastDirtiedFrame->GetParent() != textFrame->GetParent()) {
      // Ask the parent frame to reflow me.
      shell->FrameNeedsReflow(textFrame, nsIPresShell::eStyleChange,
                              NS_FRAME_IS_DIRTY);
      lastDirtiedFrame = textFrame;
    } else {
      // if the parent is a block, we're cheating here because we should
      // be marking our line dirty, but we're not. nsTextFrame::SetLength
      // will do that when it gets called during reflow.
      textFrame->AddStateBits(NS_FRAME_IS_DIRTY);
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

    textFrame = static_cast<nsTextFrame*>(textFrame->GetNextContinuation());
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
      textFrame = static_cast<nsTextFrame*>(textFrame->GetNextContinuation());
    }
  }

  return NS_OK;
}

/* virtual */ void
nsTextFrame::DidSetStyleContext(nsStyleContext* aOldStyleContext)
{
  nsFrame::DidSetStyleContext(aOldStyleContext);
  ClearTextRuns();
}

class nsDisplayTextGeometry : public nsDisplayItemGenericGeometry
{
public:
  nsDisplayTextGeometry(nsDisplayItem* aItem, nsDisplayListBuilder* aBuilder)
    : nsDisplayItemGenericGeometry(aItem, aBuilder)
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
};

class nsDisplayText : public nsCharClipDisplayItem {
public:
  nsDisplayText(nsDisplayListBuilder* aBuilder, nsTextFrame* aFrame) :
    nsCharClipDisplayItem(aBuilder, aFrame),
    mDisableSubpixelAA(false) {
    MOZ_COUNT_CTOR(nsDisplayText);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayText() {
    MOZ_COUNT_DTOR(nsDisplayText);
  }
#endif

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) {
    *aSnap = false;
    nsRect temp = mFrame->GetVisualOverflowRectRelativeToSelf() + ToReferenceFrame();
    // Bug 748228
    temp.Inflate(mFrame->PresContext()->AppUnitsPerDevPixel());
    return temp;
  }
  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) {
    if (nsRect(ToReferenceFrame(), mFrame->GetSize()).Intersects(aRect)) {
      aOutFrames->AppendElement(mFrame);
    }
  }
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx);
  NS_DISPLAY_DECL_NAME("Text", TYPE_TEXT)

  virtual nsRect GetComponentAlphaBounds(nsDisplayListBuilder* aBuilder)
  {
    bool snap;
    return GetBounds(aBuilder, &snap);
  }

  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder)
  {
    return new nsDisplayTextGeometry(this, aBuilder);
  }

  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion *aInvalidRegion)
  {
    const nsDisplayTextGeometry* geometry = static_cast<const nsDisplayTextGeometry*>(aGeometry);
    nsTextFrame* f = static_cast<nsTextFrame*>(mFrame);

    nsTextFrame::TextDecorations decorations;
    f->GetTextDecorations(f->PresContext(), nsTextFrame::eResolvedColors, decorations);

    bool snap;
    nsRect newRect = geometry->mBounds;
    nsRect oldRect = GetBounds(aBuilder, &snap);
    if (decorations != geometry->mDecorations ||
        !oldRect.IsEqualInterior(newRect) ||
        !geometry->mBorderRect.IsEqualInterior(GetBorderRect())) {
      aInvalidRegion->Or(oldRect, newRect);
    }
  }
  
  virtual void DisableComponentAlpha() { mDisableSubpixelAA = true; }

  bool mDisableSubpixelAA;
};

void
nsDisplayText::Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx) {
  PROFILER_LABEL("nsDisplayText", "Paint");
  // Add 1 pixel of dirty area around mVisibleRect to allow us to paint
  // antialiased pixels beyond the measured text extents.
  // This is temporary until we do this in the actual calculation of text extents.
  nsRect extraVisible = mVisibleRect;
  nscoord appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  extraVisible.Inflate(appUnitsPerDevPixel, appUnitsPerDevPixel);
  nsTextFrame* f = static_cast<nsTextFrame*>(mFrame);

  gfxContextAutoDisableSubpixelAntialiasing disable(aCtx->ThebesContext(),
                                                    mDisableSubpixelAA);
  NS_ASSERTION(mLeftEdge >= 0, "illegal left edge");
  NS_ASSERTION(mRightEdge >= 0, "illegal right edge");
  f->PaintText(aCtx, ToReferenceFrame(), extraVisible, *this);
}

void
nsTextFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists)
{
  if (!IsVisibleForPainting(aBuilder))
    return;
  
  DO_GLOBAL_REFLOW_COUNT_DSP("nsTextFrame");

  aLists.Content()->AppendNewToTop(
    new (aBuilder) nsDisplayText(aBuilder, this));
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

SelectionDetails*
nsTextFrame::GetSelectionDetails()
{
  const nsFrameSelection* frameSelection = GetConstFrameSelection();
  if (frameSelection->GetTableCellSelection()) {
    return nullptr;
  }
  if (!(GetStateBits() & NS_FRAME_GENERATED_CONTENT)) {
    SelectionDetails* details =
      frameSelection->LookUpSelection(mContent, GetContentOffset(),
                                      GetContentLength(), false);
    SelectionDetails* sd;
    for (sd = details; sd; sd = sd->mNext) {
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

  SelectionDetails* details =
    frameSelection->LookUpSelection(owner->GetContent(),
        isBefore ? 0 : owner->GetContent()->GetChildCount(), 0, false);
  SelectionDetails* sd;
  for (sd = details; sd; sd = sd->mNext) {
    // The entire text is selected!
    sd->mStart = GetContentOffset();
    sd->mEnd = GetContentEnd();
  }
  return details;
}

static void
PaintSelectionBackground(gfxContext* aCtx, nsPresContext* aPresContext,
                         nscolor aColor, const gfxRect& aDirtyRect,
                         const gfxRect& aRect,
                         nsTextFrame::DrawPathCallbacks* aCallbacks)
{
  if (aCallbacks) {
    aCallbacks->NotifyBeforeSelectionBackground(aColor);
  }

  gfxRect r = aRect.Intersect(aDirtyRect);
  // For now, we need to put this in pixel coordinates
  int32_t app = aPresContext->AppUnitsPerDevPixel();
  aCtx->NewPath();
  // pixel-snap
  aCtx->Rectangle(gfxRect(r.X() / app, r.Y() / app,
                          r.Width() / app, r.Height() / app), true);

  if (aCallbacks) {
    aCallbacks->NotifySelectionBackgroundPathEmitted();
  } else {
    aCtx->SetColor(gfxRGBA(aColor));
    aCtx->Fill();
  }
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

  // frameTopOffset represents the offset to f's top from our baseline in our
  // coordinate space
  // baselineOffset represents the offset from our baseline to f's baseline or
  // the nearest block's baseline, in our coordinate space, whichever is closest
  // during the particular iteration
  nscoord frameTopOffset = mAscent,
          baselineOffset = 0;

  bool nearestBlockFound = false;

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
        nsLayoutUtils::GetColor(f, eCSSProperty_text_decoration_color);
    }

    const bool firstBlock = !nearestBlockFound && nsLayoutUtils::GetAsBlock(f);

    // Not updating positions once we hit a parent block is equivalent to
    // the CSS 2.1 spec that blocks should propagate decorations down to their
    // children (albeit the style should be preserved)
    // However, if we're vertically aligned within a block, then we need to
    // recover the right baseline from the line by querying the FrameProperty
    // that should be set (see nsLineLayout::VerticalAlignLine).
    if (firstBlock) {
      // At this point, fChild can't be null since TextFrames can't be blocks
      if (fChild->VerticalAlignEnum() != NS_STYLE_VERTICAL_ALIGN_BASELINE) {
        // Since offset is the offset in the child's coordinate space, we have
        // to undo the accumulation to bring the transform out of the block's
        // coordinate space
        baselineOffset =
          frameTopOffset - fChild->GetNormalPosition().y
          - NS_PTR_TO_INT32(
              fChild->Properties().Get(nsIFrame::LineBaselineOffset()));
      }
    }
    else if (!nearestBlockFound) {
      baselineOffset = frameTopOffset - f->GetBaseline();
    }

    nearestBlockFound = nearestBlockFound || firstBlock;
    frameTopOffset += f->GetNormalPosition().y;

    const uint8_t style = styleText->GetDecorationStyle();
    if (textDecorations) {
      nscolor color;
      if (useOverride) {
        color = overrideColor;
      } else if (IsSVGText()) {
        // XXX We might want to do something with text-decoration-color when
        //     painting SVG text, but it's not clear what we should do.  We
        //     at least need SVG text decorations to paint with 'fill' if
        //     text-decoration-color has its initial value currentColor.
        //     We could choose to interpret currentColor as "currentFill"
        //     for SVG text, and have e.g. text-decoration-color:red to
        //     override the fill paint of the decoration.
        color = aColorResolution == eResolvedColors ?
                  nsLayoutUtils::GetColor(f, eCSSProperty_fill) :
                  NS_SAME_AS_FOREGROUND_COLOR;
      } else {
        color = nsLayoutUtils::GetColor(f, eCSSProperty_text_decoration_color);
      }

      if (textDecorations & NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE) {
        aDecorations.mUnderlines.AppendElement(
          nsTextFrame::LineDecoration(f, baselineOffset, color, style));
      }
      if (textDecorations & NS_STYLE_TEXT_DECORATION_LINE_OVERLINE) {
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
    uint8_t display = f->GetDisplay();
    if (display != NS_STYLE_DISPLAY_INLINE &&
        nsStyleDisplay::IsDisplayTypeInlineOutside(display)) {
      break;
    }

    if (compatMode == eCompatibility_NavQuirks) {
      // In quirks mode, if we're on an HTML table element, we're done.
      if (f->GetContent()->IsHTML(nsGkAtoms::table)) {
        break;
      }
    } else {
      // In standards/almost-standards mode, if we're on an
      // absolutely-positioned element or a floating element, we're done.
      if (f->IsFloating() || f->IsAbsolutelyPositioned()) {
        break;
      }
    }
  }
}

static float
GetInflationForTextDecorations(nsIFrame* aFrame, nscoord aInflationMinFontSize)
{
  if (aFrame->IsSVGText()) {
    const nsIFrame* container = aFrame;
    while (container->GetType() != nsGkAtoms::svgTextFrame) {
      container = container->GetParent();
    }
    NS_ASSERTION(container, "expected to find an ancestor SVGTextFrame");
    return
      static_cast<const SVGTextFrame*>(container)->GetFontSizeScaleFactor();
  }
  return nsLayoutUtils::FontSizeInflationInner(aFrame, aInflationMinFontSize);
}

void
nsTextFrame::UnionAdditionalOverflow(nsPresContext* aPresContext,
                                     const nsHTMLReflowState& aBlockReflowState,
                                     PropertyProvider& aProvider,
                                     nsRect* aVisualOverflowRect,
                                     bool aIncludeTextDecorations)
{
  // Text-shadow overflows
  nsRect shadowRect =
    nsLayoutUtils::GetTextShadowRectsUnion(*aVisualOverflowRect, this);
  aVisualOverflowRect->UnionRect(*aVisualOverflowRect, shadowRect);

  if (IsFloatingFirstLetterChild()) {
    // The underline/overline drawable area must be contained in the overflow
    // rect when this is in floating first letter frame at *both* modes.
    nsIFrame* firstLetterFrame = aBlockReflowState.frame;
    uint8_t decorationStyle = firstLetterFrame->StyleContext()->
                                StyleTextReset()->GetDecorationStyle();
    // If the style is none, let's include decoration line rect as solid style
    // since changing the style from none to solid/dotted/dashed doesn't cause
    // reflow.
    if (decorationStyle == NS_STYLE_TEXT_DECORATION_STYLE_NONE) {
      decorationStyle = NS_STYLE_TEXT_DECORATION_STYLE_SOLID;
    }
    nsFontMetrics* fontMetrics = aProvider.GetFontMetrics();
    nscoord underlineOffset, underlineSize;
    fontMetrics->GetUnderline(underlineOffset, underlineSize);
    nscoord maxAscent = fontMetrics->MaxAscent();

    gfxFloat appUnitsPerDevUnit = aPresContext->AppUnitsPerDevPixel();
    gfxFloat gfxWidth = aVisualOverflowRect->width / appUnitsPerDevUnit;
    gfxFloat gfxAscent = gfxFloat(mAscent) / appUnitsPerDevUnit;
    gfxFloat gfxMaxAscent = maxAscent / appUnitsPerDevUnit;
    gfxFloat gfxUnderlineSize = underlineSize / appUnitsPerDevUnit;
    gfxFloat gfxUnderlineOffset = underlineOffset / appUnitsPerDevUnit;
    nsRect underlineRect =
      nsCSSRendering::GetTextDecorationRect(aPresContext,
        gfxSize(gfxWidth, gfxUnderlineSize), gfxAscent, gfxUnderlineOffset,
        NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE, decorationStyle);
    nsRect overlineRect =
      nsCSSRendering::GetTextDecorationRect(aPresContext,
        gfxSize(gfxWidth, gfxUnderlineSize), gfxAscent, gfxMaxAscent,
        NS_STYLE_TEXT_DECORATION_LINE_OVERLINE, decorationStyle);

    aVisualOverflowRect->UnionRect(*aVisualOverflowRect, underlineRect);
    aVisualOverflowRect->UnionRect(*aVisualOverflowRect, overlineRect);

    // XXX If strikeoutSize is much thicker than the underlineSize, it may
    //     cause overflowing from the overflow rect.  However, such case
    //     isn't realistic, we don't need to compute it now.
  }
  if (aIncludeTextDecorations) {
    // Since CSS 2.1 requires that text-decoration defined on ancestors maintain
    // style and position, they can be drawn at virtually any y-offset, so
    // maxima and minima are required to reliably generate the rectangle for
    // them
    TextDecorations textDecs;
    GetTextDecorations(aPresContext, eResolvedColors, textDecs);
    if (textDecs.HasDecorationLines()) {
      nscoord inflationMinFontSize =
        nsLayoutUtils::InflationMinFontSizeFor(aBlockReflowState.frame);

      const nscoord width = GetSize().width;
      const gfxFloat appUnitsPerDevUnit = aPresContext->AppUnitsPerDevPixel(),
                     gfxWidth = width / appUnitsPerDevUnit,
                     ascent = gfxFloat(mAscent) / appUnitsPerDevUnit;
      nscoord top(nscoord_MAX), bottom(nscoord_MIN);
      // Below we loop through all text decorations and compute the rectangle
      // containing all of them, in this frame's coordinate space
      for (uint32_t i = 0; i < textDecs.mUnderlines.Length(); ++i) {
        const LineDecoration& dec = textDecs.mUnderlines[i];
        uint8_t decorationStyle = dec.mStyle;
        // If the style is solid, let's include decoration line rect of solid
        // style since changing the style from none to solid/dotted/dashed
        // doesn't cause reflow.
        if (decorationStyle == NS_STYLE_TEXT_DECORATION_STYLE_NONE) {
          decorationStyle = NS_STYLE_TEXT_DECORATION_STYLE_SOLID;
        }

        float inflation =
          GetInflationForTextDecorations(dec.mFrame, inflationMinFontSize);
        const gfxFont::Metrics metrics =
          GetFirstFontMetrics(GetFontGroupForFrame(dec.mFrame, inflation));

        const nsRect decorationRect =
          nsCSSRendering::GetTextDecorationRect(aPresContext,
            gfxSize(gfxWidth, metrics.underlineSize),
            ascent, metrics.underlineOffset,
            NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE, decorationStyle) +
          nsPoint(0, -dec.mBaselineOffset);

        top = std::min(decorationRect.y, top);
        bottom = std::max(decorationRect.YMost(), bottom);
      }
      for (uint32_t i = 0; i < textDecs.mOverlines.Length(); ++i) {
        const LineDecoration& dec = textDecs.mOverlines[i];
        uint8_t decorationStyle = dec.mStyle;
        // If the style is solid, let's include decoration line rect of solid
        // style since changing the style from none to solid/dotted/dashed
        // doesn't cause reflow.
        if (decorationStyle == NS_STYLE_TEXT_DECORATION_STYLE_NONE) {
          decorationStyle = NS_STYLE_TEXT_DECORATION_STYLE_SOLID;
        }

        float inflation =
          GetInflationForTextDecorations(dec.mFrame, inflationMinFontSize);
        const gfxFont::Metrics metrics =
          GetFirstFontMetrics(GetFontGroupForFrame(dec.mFrame, inflation));

        const nsRect decorationRect =
          nsCSSRendering::GetTextDecorationRect(aPresContext,
            gfxSize(gfxWidth, metrics.underlineSize),
            ascent, metrics.maxAscent,
            NS_STYLE_TEXT_DECORATION_LINE_OVERLINE, decorationStyle) +
          nsPoint(0, -dec.mBaselineOffset);

        top = std::min(decorationRect.y, top);
        bottom = std::max(decorationRect.YMost(), bottom);
      }
      for (uint32_t i = 0; i < textDecs.mStrikes.Length(); ++i) {
        const LineDecoration& dec = textDecs.mStrikes[i];
        uint8_t decorationStyle = dec.mStyle;
        // If the style is solid, let's include decoration line rect of solid
        // style since changing the style from none to solid/dotted/dashed
        // doesn't cause reflow.
        if (decorationStyle == NS_STYLE_TEXT_DECORATION_STYLE_NONE) {
          decorationStyle = NS_STYLE_TEXT_DECORATION_STYLE_SOLID;
        }

        float inflation =
          GetInflationForTextDecorations(dec.mFrame, inflationMinFontSize);
        const gfxFont::Metrics metrics =
          GetFirstFontMetrics(GetFontGroupForFrame(dec.mFrame, inflation));

        const nsRect decorationRect =
          nsCSSRendering::GetTextDecorationRect(aPresContext,
            gfxSize(gfxWidth, metrics.strikeoutSize),
            ascent, metrics.strikeoutOffset,
            NS_STYLE_TEXT_DECORATION_LINE_LINE_THROUGH, decorationStyle) +
          nsPoint(0, -dec.mBaselineOffset);
        top = std::min(decorationRect.y, top);
        bottom = std::max(decorationRect.YMost(), bottom);
      }

      aVisualOverflowRect->UnionRect(*aVisualOverflowRect,
                                     nsRect(0, top, width, bottom - top));
    }
  }
  // When this frame is not selected, the text-decoration area must be in
  // frame bounds.
  if (!IsSelected() ||
      !CombineSelectionUnderlineRect(aPresContext, *aVisualOverflowRect))
    return;
  AddStateBits(TEXT_SELECTION_UNDERLINE_OVERFLOWED);
}

static gfxFloat
ComputeDescentLimitForSelectionUnderline(nsPresContext* aPresContext,
                                         nsTextFrame* aFrame,
                                         const gfxFont::Metrics& aFontMetrics)
{
  gfxFloat app = aPresContext->AppUnitsPerDevPixel();
  nscoord lineHeightApp =
    nsHTMLReflowState::CalcLineHeight(aFrame->StyleContext(), NS_AUTOHEIGHT,
                                      aFrame->GetFontSizeInflation());
  gfxFloat lineHeight = gfxFloat(lineHeightApp) / app;
  if (lineHeight <= aFontMetrics.maxHeight) {
    return aFontMetrics.maxDescent;
  }
  return aFontMetrics.maxDescent + (lineHeight - aFontMetrics.maxHeight) / 2;
}


// Make sure this stays in sync with DrawSelectionDecorations below
static const SelectionType SelectionTypesWithDecorations =
  nsISelectionController::SELECTION_SPELLCHECK |
  nsISelectionController::SELECTION_IME_RAWINPUT |
  nsISelectionController::SELECTION_IME_SELECTEDRAWTEXT |
  nsISelectionController::SELECTION_IME_CONVERTEDTEXT |
  nsISelectionController::SELECTION_IME_SELECTEDCONVERTEDTEXT;

static gfxFloat
ComputeSelectionUnderlineHeight(nsPresContext* aPresContext,
                                const gfxFont::Metrics& aFontMetrics,
                                SelectionType aSelectionType)
{
  switch (aSelectionType) {
    case nsISelectionController::SELECTION_IME_RAWINPUT:
    case nsISelectionController::SELECTION_IME_SELECTEDRAWTEXT:
    case nsISelectionController::SELECTION_IME_CONVERTEDTEXT:
    case nsISelectionController::SELECTION_IME_SELECTEDCONVERTEDTEXT:
      return aFontMetrics.underlineSize;
    case nsISelectionController::SELECTION_SPELLCHECK: {
      // The thickness of the spellchecker underline shouldn't honor the font
      // metrics.  It should be constant pixels value which is decided from the
      // default font size.  Note that if the actual font size is smaller than
      // the default font size, we should use the actual font size because the
      // computed value from the default font size can be too thick for the
      // current font size.
      int32_t defaultFontSize =
        aPresContext->AppUnitsToDevPixels(nsStyleFont(aPresContext).mFont.size);
      gfxFloat fontSize = std::min(gfxFloat(defaultFontSize),
                                 aFontMetrics.emHeight);
      fontSize = std::max(fontSize, 1.0);
      return ceil(fontSize / 20);
    }
    default:
      NS_WARNING("Requested underline style is not valid");
      return aFontMetrics.underlineSize;
  }
}

enum DecorationType {
  eNormalDecoration,
  eSelectionDecoration
};

static void
PaintDecorationLine(nsIFrame* aFrame,
                    gfxContext* const aCtx,
                    const gfxRect& aDirtyRect,
                    nscolor aColor,
                    const nscolor* aOverrideColor,
                    const gfxPoint& aPt,
                    gfxFloat aXInFrame,
                    const gfxSize& aLineSize,
                    gfxFloat aAscent,
                    gfxFloat aOffset,
                    uint8_t aDecoration,
                    uint8_t aStyle,
                    DecorationType aDecorationType,
                    nsTextFrame::DrawPathCallbacks* aCallbacks,
                    gfxFloat aDescentLimit = -1.0)
{
  nscolor lineColor = aOverrideColor ? *aOverrideColor : aColor;
  if (aCallbacks) {
    if (aDecorationType == eNormalDecoration) {
      aCallbacks->NotifyBeforeDecorationLine(lineColor);
    } else {
      aCallbacks->NotifyBeforeSelectionDecorationLine(lineColor);
    }
    nsCSSRendering::DecorationLineToPath(aFrame, aCtx, aDirtyRect, lineColor,
      aPt, aXInFrame, aLineSize, aAscent, aOffset, aDecoration, aStyle,
      aDescentLimit);
    if (aDecorationType == eNormalDecoration) {
      aCallbacks->NotifyDecorationLinePathEmitted();
    } else {
      aCallbacks->NotifySelectionDecorationLinePathEmitted();
    }
  } else {
    nsCSSRendering::PaintDecorationLine(aFrame, aCtx, aDirtyRect, lineColor,
      aPt, aXInFrame, aLineSize, aAscent, aOffset, aDecoration, aStyle,
      aDescentLimit);
  }
}

/**
 * This, plus SelectionTypesWithDecorations, encapsulates all knowledge about
 * drawing text decoration for selections.
 */
static void DrawSelectionDecorations(gfxContext* aContext,
    const gfxRect& aDirtyRect,
    SelectionType aType,
    nsTextFrame* aFrame,
    nsTextPaintStyle& aTextPaintStyle,
    const TextRangeStyle &aRangeStyle,
    const gfxPoint& aPt, gfxFloat aXInFrame, gfxFloat aWidth,
    gfxFloat aAscent, const gfxFont::Metrics& aFontMetrics,
    nsTextFrame::DrawPathCallbacks* aCallbacks)
{
  gfxPoint pt(aPt);
  gfxSize size(aWidth,
               ComputeSelectionUnderlineHeight(aTextPaintStyle.PresContext(),
                                               aFontMetrics, aType));
  gfxFloat descentLimit =
    ComputeDescentLimitForSelectionUnderline(aTextPaintStyle.PresContext(),
                                             aFrame, aFontMetrics);

  float relativeSize;
  uint8_t style;
  nscolor color;
  int32_t index =
    nsTextPaintStyle::GetUnderlineStyleIndexForSelectionType(aType);
  bool weDefineSelectionUnderline =
    aTextPaintStyle.GetSelectionUnderlineForPaint(index, &color,
                                                  &relativeSize, &style);

  switch (aType) {
    case nsISelectionController::SELECTION_IME_RAWINPUT:
    case nsISelectionController::SELECTION_IME_SELECTEDRAWTEXT:
    case nsISelectionController::SELECTION_IME_CONVERTEDTEXT:
    case nsISelectionController::SELECTION_IME_SELECTEDCONVERTEDTEXT: {
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
      pt.x += 1.0;
      size.width -= 2.0;
      if (aRangeStyle.IsDefined()) {
        // If IME defines the style, that should override our definition.
        if (aRangeStyle.IsLineStyleDefined()) {
          if (aRangeStyle.mLineStyle == TextRangeStyle::LINESTYLE_NONE) {
            return;
          }
          style = aRangeStyle.mLineStyle;
          relativeSize = aRangeStyle.mIsBoldLine ? 2.0f : 1.0f;
        } else if (!weDefineSelectionUnderline) {
          // There is no underline style definition.
          return;
        }
        if (aRangeStyle.IsUnderlineColorDefined()) {
          color = aRangeStyle.mUnderlineColor;
        } else if (aRangeStyle.IsForegroundColorDefined()) {
          color = aRangeStyle.mForegroundColor;
        } else {
          NS_ASSERTION(!aRangeStyle.IsBackgroundColorDefined(),
                       "Only the background color is defined");
          color = aTextPaintStyle.GetTextColor();
        }
      } else if (!weDefineSelectionUnderline) {
        // IME doesn't specify the selection style and we don't define selection
        // underline.
        return;
      }
      break;
    }
    case nsISelectionController::SELECTION_SPELLCHECK:
      if (!weDefineSelectionUnderline)
        return;
      break;
    default:
      NS_WARNING("Requested selection decorations when there aren't any");
      return;
  }
  size.height *= relativeSize;
  PaintDecorationLine(aFrame, aContext, aDirtyRect, color, nullptr, pt,
    pt.x - aPt.x + aXInFrame, size, aAscent, aFontMetrics.underlineOffset,
    NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE, style, eSelectionDecoration,
    aCallbacks, descentLimit);
}

/**
 * This function encapsulates all knowledge of how selections affect foreground
 * and background colors.
 * @return true if the selection affects colors, false otherwise
 * @param aForeground the foreground color to use
 * @param aBackground the background color to use, or RGBA(0,0,0,0) if no
 * background should be painted
 */
static bool GetSelectionTextColors(SelectionType aType,
                                     nsTextPaintStyle& aTextPaintStyle,
                                     const TextRangeStyle &aRangeStyle,
                                     nscolor* aForeground, nscolor* aBackground)
{
  switch (aType) {
    case nsISelectionController::SELECTION_NORMAL:
      return aTextPaintStyle.GetSelectionColors(aForeground, aBackground);
    case nsISelectionController::SELECTION_FIND:
      aTextPaintStyle.GetHighlightColors(aForeground, aBackground);
      return true;
    case nsISelectionController::SELECTION_URLSECONDARY:
      aTextPaintStyle.GetURLSecondaryColor(aForeground);
      *aBackground = NS_RGBA(0,0,0,0);
      return true;
    case nsISelectionController::SELECTION_IME_RAWINPUT:
    case nsISelectionController::SELECTION_IME_SELECTEDRAWTEXT:
    case nsISelectionController::SELECTION_IME_CONVERTEDTEXT:
    case nsISelectionController::SELECTION_IME_SELECTEDCONVERTEDTEXT:
      if (aRangeStyle.IsDefined()) {
        *aForeground = aTextPaintStyle.GetTextColor();
        *aBackground = NS_RGBA(0,0,0,0);
        if (!aRangeStyle.IsForegroundColorDefined() &&
            !aRangeStyle.IsBackgroundColorDefined()) {
          return false;
        }
        if (aRangeStyle.IsForegroundColorDefined()) {
          *aForeground = aRangeStyle.mForegroundColor;
        }
        if (aRangeStyle.IsBackgroundColorDefined()) {
          *aBackground = aRangeStyle.mBackgroundColor;
        }
        return true;
      }
      aTextPaintStyle.GetIMESelectionColors(
        nsTextPaintStyle::GetUnderlineStyleIndexForSelectionType(aType),
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
                                   SelectionType aType,
                                   nsTextPaintStyle& aTextPaintStyle,
                                   nsCSSShadowArray** aShadow)
{
  switch (aType) {
    case nsISelectionController::SELECTION_NORMAL:
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
                    int32_t aStart, int32_t aLength,
                    PropertyProvider& aProvider, gfxTextRun* aTextRun,
                    gfxFloat aXOffset);

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
   * @param aStyle the selection style for this segment
   * @return false if there are no more segments
   */
  bool GetNextSegment(gfxFloat* aXOffset, uint32_t* aOffset, uint32_t* aLength,
                        gfxFloat* aHyphenWidth, SelectionType* aType,
                        TextRangeStyle* aStyle);
  void UpdateWithAdvance(gfxFloat aAdvance) {
    mXOffset += aAdvance*mTextRun->GetDirection();
  }

private:
  SelectionDetails**      mSelectionDetails;
  PropertyProvider&       mProvider;
  gfxTextRun*             mTextRun;
  gfxSkipCharsIterator    mIterator;
  int32_t                 mOriginalStart;
  int32_t                 mOriginalEnd;
  gfxFloat                mXOffset;
};

SelectionIterator::SelectionIterator(SelectionDetails** aSelectionDetails,
    int32_t aStart, int32_t aLength, PropertyProvider& aProvider,
    gfxTextRun* aTextRun, gfxFloat aXOffset)
  : mSelectionDetails(aSelectionDetails), mProvider(aProvider),
    mTextRun(aTextRun), mIterator(aProvider.GetStart()),
    mOriginalStart(aStart), mOriginalEnd(aStart + aLength),
    mXOffset(aXOffset)
{
  mIterator.SetOriginalOffset(aStart);
}

bool SelectionIterator::GetNextSegment(gfxFloat* aXOffset,
    uint32_t* aOffset, uint32_t* aLength, gfxFloat* aHyphenWidth,
    SelectionType* aType, TextRangeStyle* aStyle)
{
  if (mIterator.GetOriginalOffset() >= mOriginalEnd)
    return false;
  
  // save offset into transformed string now
  uint32_t runOffset = mIterator.GetSkippedOffset();
  
  int32_t index = mIterator.GetOriginalOffset() - mOriginalStart;
  SelectionDetails* sdptr = mSelectionDetails[index];
  SelectionType type =
    sdptr ? sdptr->mType : nsISelectionController::SELECTION_NONE;
  TextRangeStyle style;
  if (sdptr) {
    style = sdptr->mTextRangeStyle;
  }
  for (++index; mOriginalStart + index < mOriginalEnd; ++index) {
    if (sdptr != mSelectionDetails[index])
      break;
  }
  mIterator.SetOriginalOffset(index + mOriginalStart);

  // Advance to the next cluster boundary
  while (mIterator.GetOriginalOffset() < mOriginalEnd &&
         !mIterator.IsOriginalCharSkipped() &&
         !mTextRun->IsClusterStart(mIterator.GetSkippedOffset())) {
    mIterator.AdvanceOriginal(1);
  }

  bool haveHyphenBreak =
    (mProvider.GetFrame()->GetStateBits() & TEXT_HYPHEN_BREAK) != 0;
  *aOffset = runOffset;
  *aLength = mIterator.GetSkippedOffset() - runOffset;
  *aXOffset = mXOffset;
  *aHyphenWidth = 0;
  if (mIterator.GetOriginalOffset() == mOriginalEnd && haveHyphenBreak) {
    *aHyphenWidth = mProvider.GetHyphenWidth();
  }
  *aType = type;
  *aStyle = style;
  return true;
}

static void
AddHyphenToMetrics(nsTextFrame* aTextFrame, gfxTextRun* aBaseTextRun,
                   gfxTextRun::Metrics* aMetrics,
                   gfxFont::BoundingBoxType aBoundingBoxType,
                   gfxContext* aContext)
{
  // Fix up metrics to include hyphen
  nsAutoPtr<gfxTextRun> hyphenTextRun(
    GetHyphenTextRun(aBaseTextRun, aContext, aTextFrame));
  if (!hyphenTextRun.get())
    return;

  gfxTextRun::Metrics hyphenMetrics =
    hyphenTextRun->MeasureText(0, hyphenTextRun->GetLength(),
                               aBoundingBoxType, aContext, nullptr);
  aMetrics->CombineWith(hyphenMetrics, aBaseTextRun->IsRightToLeft());
}

void
nsTextFrame::PaintOneShadow(uint32_t aOffset, uint32_t aLength,
                            nsCSSShadowItem* aShadowDetails,
                            PropertyProvider* aProvider, const nsRect& aDirtyRect,
                            const gfxPoint& aFramePt, const gfxPoint& aTextBaselinePt,
                            gfxContext* aCtx, const nscolor& aForegroundColor,
                            const nsCharClipDisplayItem::ClipEdges& aClipEdges,
                            nscoord aLeftSideOffset, gfxRect& aBoundingBox)
{
  PROFILER_LABEL("nsTextFrame", "PaintOneShadow");
  gfxPoint shadowOffset(aShadowDetails->mXOffset, aShadowDetails->mYOffset);
  nscoord blurRadius = std::max(aShadowDetails->mRadius, 0);

  // This rect is the box which is equivalent to where the shadow will be painted.
  // The origin of aBoundingBox is the text baseline left, so we must translate it by
  // that much in order to make the origin the top-left corner of the text bounding box.
  gfxRect shadowGfxRect = aBoundingBox +
    gfxPoint(aFramePt.x + aLeftSideOffset, aTextBaselinePt.y) + shadowOffset;
  nsRect shadowRect(NSToCoordRound(shadowGfxRect.X()),
                    NSToCoordRound(shadowGfxRect.Y()),
                    NSToCoordRound(shadowGfxRect.Width()),
                    NSToCoordRound(shadowGfxRect.Height()));

  nsContextBoxBlur contextBoxBlur;
  gfxContext* shadowContext = contextBoxBlur.Init(shadowRect, 0, blurRadius,
                                                  PresContext()->AppUnitsPerDevPixel(),
                                                  aCtx, aDirtyRect, nullptr);
  if (!shadowContext)
    return;

  nscolor shadowColor;
  const nscolor* decorationOverrideColor;
  if (aShadowDetails->mHasColor) {
    shadowColor = aShadowDetails->mColor;
    decorationOverrideColor = &shadowColor;
  } else {
    shadowColor = aForegroundColor;
    decorationOverrideColor = nullptr;
  }

  aCtx->Save();
  aCtx->NewPath();
  aCtx->SetColor(gfxRGBA(shadowColor));

  // Draw the text onto our alpha-only surface to capture the alpha values.
  // Remember that the box blur context has a device offset on it, so we don't need to
  // translate any coordinates to fit on the surface.
  gfxFloat advanceWidth;
  gfxRect dirtyRect(aDirtyRect.x, aDirtyRect.y,
                    aDirtyRect.width, aDirtyRect.height);
  DrawText(shadowContext, dirtyRect, aFramePt + shadowOffset,
           aTextBaselinePt + shadowOffset, aOffset, aLength, *aProvider,
           nsTextPaintStyle(this),
           aCtx == shadowContext ? shadowColor : NS_RGB(0, 0, 0), aClipEdges,
           advanceWidth, (GetStateBits() & TEXT_HYPHEN_BREAK) != 0,
           decorationOverrideColor);

  contextBoxBlur.DoPaint();
  aCtx->Restore();
}

// Paints selection backgrounds and text in the correct colors. Also computes
// aAllTypes, the union of all selection types that are applying to this text.
bool
nsTextFrame::PaintTextWithSelectionColors(gfxContext* aCtx,
    const gfxPoint& aFramePt, const gfxPoint& aTextBaselinePt,
    const gfxRect& aDirtyRect,
    PropertyProvider& aProvider,
    uint32_t aContentOffset, uint32_t aContentLength,
    nsTextPaintStyle& aTextPaintStyle, SelectionDetails* aDetails,
    SelectionType* aAllTypes,
    const nsCharClipDisplayItem::ClipEdges& aClipEdges,
    nsTextFrame::DrawPathCallbacks* aCallbacks)
{
  // Figure out which selections control the colors to use for each character.
  AutoFallibleTArray<SelectionDetails*,BIG_TEXT_NODE_SIZE> prevailingSelectionsBuffer;
  SelectionDetails** prevailingSelections =
    prevailingSelectionsBuffer.AppendElements(aContentLength);
  if (!prevailingSelections) {
    return false;
  }

  SelectionType allTypes = 0;
  for (uint32_t i = 0; i < aContentLength; ++i) {
    prevailingSelections[i] = nullptr;
  }

  SelectionDetails *sdptr = aDetails;
  bool anyBackgrounds = false;
  while (sdptr) {
    int32_t start = std::max(0, sdptr->mStart - int32_t(aContentOffset));
    int32_t end = std::min(int32_t(aContentLength),
                         sdptr->mEnd - int32_t(aContentOffset));
    SelectionType type = sdptr->mType;
    if (start < end) {
      allTypes |= type;
      // Ignore selections that don't set colors
      nscolor foreground, background;
      if (GetSelectionTextColors(type, aTextPaintStyle, sdptr->mTextRangeStyle,
                                 &foreground, &background)) {
        if (NS_GET_A(background) > 0) {
          anyBackgrounds = true;
        }
        for (int32_t i = start; i < end; ++i) {
          // Favour normal selection over IME selections
          if (!prevailingSelections[i] ||
              type < prevailingSelections[i]->mType) {
            prevailingSelections[i] = sdptr;
          }
        }
      }
    }
    sdptr = sdptr->mNext;
  }
  *aAllTypes = allTypes;

  if (!allTypes) {
    // Nothing is selected in the given text range. XXX can this still occur?
    return false;
  }

  const gfxFloat startXOffset = aTextBaselinePt.x - aFramePt.x;
  gfxFloat xOffset, hyphenWidth;
  uint32_t offset, length; // in transformed string
  SelectionType type;
  TextRangeStyle rangeStyle;
  // Draw background colors
  if (anyBackgrounds) {
    SelectionIterator iterator(prevailingSelections, aContentOffset, aContentLength,
                               aProvider, mTextRun, startXOffset);
    while (iterator.GetNextSegment(&xOffset, &offset, &length, &hyphenWidth,
                                   &type, &rangeStyle)) {
      nscolor foreground, background;
      GetSelectionTextColors(type, aTextPaintStyle, rangeStyle,
                             &foreground, &background);
      // Draw background color
      gfxFloat advance = hyphenWidth +
        mTextRun->GetAdvanceWidth(offset, length, &aProvider);
      if (NS_GET_A(background) > 0) {
        gfxFloat x = xOffset - (mTextRun->IsRightToLeft() ? advance : 0);
        PaintSelectionBackground(aCtx, aTextPaintStyle.PresContext(),
                                 background, aDirtyRect,
                                 gfxRect(aFramePt.x + x, aFramePt.y, advance,
                                 GetSize().height), aCallbacks);
      }
      iterator.UpdateWithAdvance(advance);
    }
  }
  
  // Draw text
  const nsStyleText* textStyle = StyleText();
  nsRect dirtyRect(aDirtyRect.x, aDirtyRect.y,
                   aDirtyRect.width, aDirtyRect.height);
  SelectionIterator iterator(prevailingSelections, aContentOffset, aContentLength,
                             aProvider, mTextRun, startXOffset);
  while (iterator.GetNextSegment(&xOffset, &offset, &length, &hyphenWidth,
                                 &type, &rangeStyle)) {
    nscolor foreground, background;
    GetSelectionTextColors(type, aTextPaintStyle, rangeStyle,
                           &foreground, &background);
    gfxPoint textBaselinePt(aFramePt.x + xOffset, aTextBaselinePt.y);

    // Determine what shadow, if any, to draw - either from textStyle
    // or from the ::-moz-selection pseudo-class if specified there
    nsCSSShadowArray* shadow = textStyle->GetTextShadow();
    GetSelectionTextShadow(this, type, aTextPaintStyle, &shadow);

    // Draw shadows, if any
    if (shadow) {
      gfxTextRun::Metrics shadowMetrics =
        mTextRun->MeasureText(offset, length, gfxFont::LOOSE_INK_EXTENTS,
                              nullptr, &aProvider);
      if (GetStateBits() & TEXT_HYPHEN_BREAK) {
        AddHyphenToMetrics(this, mTextRun, &shadowMetrics,
                           gfxFont::LOOSE_INK_EXTENTS, aCtx);
      }
      for (uint32_t i = shadow->Length(); i > 0; --i) {
        PaintOneShadow(offset, length,
                       shadow->ShadowAt(i - 1), &aProvider,
                       dirtyRect, aFramePt, textBaselinePt, aCtx,
                       foreground, aClipEdges, 
                       xOffset - (mTextRun->IsRightToLeft() ?
                                  shadowMetrics.mBoundingBox.width : 0),
                       shadowMetrics.mBoundingBox);
      }
    }

    // Draw text segment
    gfxFloat advance;

    DrawText(aCtx, aDirtyRect, aFramePt, textBaselinePt,
             offset, length, aProvider, aTextPaintStyle, foreground, aClipEdges,
             advance, hyphenWidth > 0, nullptr, nullptr, aCallbacks);
    if (hyphenWidth) {
      advance += hyphenWidth;
    }
    iterator.UpdateWithAdvance(advance);
  }
  return true;
}

void
nsTextFrame::PaintTextSelectionDecorations(gfxContext* aCtx,
    const gfxPoint& aFramePt,
    const gfxPoint& aTextBaselinePt, const gfxRect& aDirtyRect,
    PropertyProvider& aProvider,
    uint32_t aContentOffset, uint32_t aContentLength,
    nsTextPaintStyle& aTextPaintStyle, SelectionDetails* aDetails,
    SelectionType aSelectionType,
    nsTextFrame::DrawPathCallbacks* aCallbacks)
{
  // Hide text decorations if we're currently hiding @font-face fallback text
  if (aProvider.GetFontGroup()->ShouldSkipDrawing())
    return;

  // Figure out which characters will be decorated for this selection.
  AutoFallibleTArray<SelectionDetails*, BIG_TEXT_NODE_SIZE> selectedCharsBuffer;
  SelectionDetails** selectedChars =
    selectedCharsBuffer.AppendElements(aContentLength);
  if (!selectedChars) {
    return;
  }
  for (uint32_t i = 0; i < aContentLength; ++i) {
    selectedChars[i] = nullptr;
  }

  SelectionDetails *sdptr = aDetails;
  while (sdptr) {
    if (sdptr->mType == aSelectionType) {
      int32_t start = std::max(0, sdptr->mStart - int32_t(aContentOffset));
      int32_t end = std::min(int32_t(aContentLength),
                           sdptr->mEnd - int32_t(aContentOffset));
      for (int32_t i = start; i < end; ++i) {
        selectedChars[i] = sdptr;
      }
    }
    sdptr = sdptr->mNext;
  }

  gfxFont* firstFont = aProvider.GetFontGroup()->GetFontAt(0);
  if (!firstFont)
    return; // OOM
  gfxFont::Metrics decorationMetrics(firstFont->GetMetrics());
  decorationMetrics.underlineOffset =
    aProvider.GetFontGroup()->GetUnderlineOffset();

  gfxFloat startXOffset = aTextBaselinePt.x - aFramePt.x;
  SelectionIterator iterator(selectedChars, aContentOffset, aContentLength,
                             aProvider, mTextRun, startXOffset);
  gfxFloat xOffset, hyphenWidth;
  uint32_t offset, length;
  int32_t app = aTextPaintStyle.PresContext()->AppUnitsPerDevPixel();
  // XXX aTextBaselinePt is in AppUnits, shouldn't it be nsFloatPoint?
  gfxPoint pt(0.0, (aTextBaselinePt.y - mAscent) / app);
  gfxRect dirtyRect(aDirtyRect.x / app, aDirtyRect.y / app,
                    aDirtyRect.width / app, aDirtyRect.height / app);
  SelectionType type;
  TextRangeStyle selectedStyle;
  while (iterator.GetNextSegment(&xOffset, &offset, &length, &hyphenWidth,
                                 &type, &selectedStyle)) {
    gfxFloat advance = hyphenWidth +
      mTextRun->GetAdvanceWidth(offset, length, &aProvider);
    if (type == aSelectionType) {
      pt.x = (aFramePt.x + xOffset -
             (mTextRun->IsRightToLeft() ? advance : 0)) / app;
      gfxFloat width = Abs(advance) / app;
      gfxFloat xInFrame = pt.x - (aFramePt.x / app);
      DrawSelectionDecorations(aCtx, dirtyRect, aSelectionType, this,
                               aTextPaintStyle, selectedStyle, pt, xInFrame,
                               width, mAscent / app, decorationMetrics,
                               aCallbacks);
    }
    iterator.UpdateWithAdvance(advance);
  }
}

bool
nsTextFrame::PaintTextWithSelection(gfxContext* aCtx,
    const gfxPoint& aFramePt,
    const gfxPoint& aTextBaselinePt, const gfxRect& aDirtyRect,
    PropertyProvider& aProvider,
    uint32_t aContentOffset, uint32_t aContentLength,
    nsTextPaintStyle& aTextPaintStyle,
    const nsCharClipDisplayItem::ClipEdges& aClipEdges,
    gfxTextContextPaint* aContextPaint,
    nsTextFrame::DrawPathCallbacks* aCallbacks)
{
  NS_ASSERTION(GetContent()->IsSelectionDescendant(), "wrong paint path");

  SelectionDetails* details = GetSelectionDetails();
  if (!details) {
    return false;
  }

  SelectionType allTypes;
  if (!PaintTextWithSelectionColors(aCtx, aFramePt, aTextBaselinePt, aDirtyRect,
                                    aProvider, aContentOffset, aContentLength,
                                    aTextPaintStyle, details, &allTypes,
                                    aClipEdges, aCallbacks)) {
    DestroySelectionDetails(details);
    return false;
  }
  // Iterate through just the selection types that paint decorations and
  // paint decorations for any that actually occur in this frame. Paint
  // higher-numbered selection types below lower-numered ones on the
  // general principal that lower-numbered selections are higher priority.
  allTypes &= SelectionTypesWithDecorations;
  for (int32_t i = nsISelectionController::NUM_SELECTIONTYPES - 1;
       i >= 1; --i) {
    SelectionType type = 1 << (i - 1);
    if (allTypes & type) {
      // There is some selection of this type. Try to paint its decorations
      // (there might not be any for this type but that's OK,
      // PaintTextSelectionDecorations will exit early).
      PaintTextSelectionDecorations(aCtx, aFramePt, aTextBaselinePt, aDirtyRect,
                                    aProvider, aContentOffset, aContentLength,
                                    aTextPaintStyle, details, type,
                                    aCallbacks);
    }
  }

  DestroySelectionDetails(details);
  return true;
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
  if (IsSVGText()) {
    const nsStyleSVG* style = StyleSVG();
    if (style->mFill.mType != eStyleSVGPaintType_None &&
        style->mFill.mType != eStyleSVGPaintType_Color) {
      isSolidTextColor = false;
    }
  }

  nsTextPaintStyle textPaintStyle(this);
  textPaintStyle.SetResolveColors(isSolidTextColor);
  SelectionDetails* details = GetSelectionDetails();
  SelectionDetails* sdptr = details;
  SelectionType type = 0;
  while (sdptr) {
    int32_t start = std::max(0, sdptr->mStart - contentOffset);
    int32_t end = std::min(contentLength, sdptr->mEnd - contentOffset);
    if (start <= offsetInFrame && offsetInFrame < end &&
        (type == 0 || sdptr->mType < type)) {
      nscolor foreground, background;
      if (GetSelectionTextColors(sdptr->mType, textPaintStyle,
                                 sdptr->mTextRangeStyle,
                                 &foreground, &background)) {
        if (!isSolidTextColor &&
            NS_IS_SELECTION_SPECIAL_COLOR(foreground)) {
          result = NS_RGBA(0, 0, 0, 255);
        } else {
          result = foreground;
        }
        type = sdptr->mType;
      }
    }
    sdptr = sdptr->mNext;
  }

  DestroySelectionDetails(details);
  return result;
}

static uint32_t
ComputeTransformedLength(PropertyProvider& aProvider)
{
  gfxSkipCharsIterator iter(aProvider.GetStart());
  uint32_t start = iter.GetSkippedOffset();
  iter.AdvanceOriginal(aProvider.GetOriginalLength());
  return iter.GetSkippedOffset() - start;
}

bool
nsTextFrame::MeasureCharClippedText(nscoord aLeftEdge, nscoord aRightEdge,
                                    nscoord* aSnappedLeftEdge,
                                    nscoord* aSnappedRightEdge)
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

  uint32_t startOffset = provider.GetStart().GetSkippedOffset();
  uint32_t maxLength = ComputeTransformedLength(provider);
  return MeasureCharClippedText(provider, aLeftEdge, aRightEdge,
                                &startOffset, &maxLength,
                                aSnappedLeftEdge, aSnappedRightEdge);
}

static uint32_t GetClusterLength(gfxTextRun* aTextRun,
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
                                    nscoord aLeftEdge, nscoord aRightEdge,
                                    uint32_t* aStartOffset,
                                    uint32_t* aMaxLength,
                                    nscoord*  aSnappedLeftEdge,
                                    nscoord*  aSnappedRightEdge)
{
  *aSnappedLeftEdge = 0;
  *aSnappedRightEdge = 0;
  if (aLeftEdge <= 0 && aRightEdge <= 0) {
    return true;
  }

  uint32_t offset = *aStartOffset;
  uint32_t maxLength = *aMaxLength;
  const nscoord frameWidth = GetSize().width;
  const bool rtl = mTextRun->IsRightToLeft();
  gfxFloat advanceWidth = 0;
  const nscoord startEdge = rtl ? aRightEdge : aLeftEdge;
  if (startEdge > 0) {
    const gfxFloat maxAdvance = gfxFloat(startEdge);
    while (maxLength > 0) {
      uint32_t clusterLength =
        GetClusterLength(mTextRun, offset, maxLength, rtl);
      advanceWidth +=
        mTextRun->GetAdvanceWidth(offset, clusterLength, &aProvider);
      maxLength -= clusterLength;
      offset += clusterLength;
      if (advanceWidth >= maxAdvance) {
        break;
      }
    }
    nscoord* snappedStartEdge = rtl ? aSnappedRightEdge : aSnappedLeftEdge;
    *snappedStartEdge = NSToCoordFloor(advanceWidth);
    *aStartOffset = offset;
  }

  const nscoord endEdge = rtl ? aLeftEdge : aRightEdge;
  if (endEdge > 0) {
    const gfxFloat maxAdvance = gfxFloat(frameWidth - endEdge);
    while (maxLength > 0) {
      uint32_t clusterLength =
        GetClusterLength(mTextRun, offset, maxLength, rtl);
      gfxFloat nextAdvance = advanceWidth +
        mTextRun->GetAdvanceWidth(offset, clusterLength, &aProvider);
      if (nextAdvance > maxAdvance) {
        break;
      }
      // This cluster fits, include it.
      advanceWidth = nextAdvance;
      maxLength -= clusterLength;
      offset += clusterLength;
    }
    maxLength = offset - *aStartOffset;
    nscoord* snappedEndEdge = rtl ? aSnappedLeftEdge : aSnappedRightEdge;
    *snappedEndEdge = NSToCoordFloor(gfxFloat(frameWidth) - advanceWidth);
  }
  *aMaxLength = maxLength;
  return maxLength != 0;
}

void
nsTextFrame::PaintText(nsRenderingContext* aRenderingContext, nsPoint aPt,
                       const nsRect& aDirtyRect,
                       const nsCharClipDisplayItem& aItem,
                       gfxTextContextPaint* aContextPaint,
                       nsTextFrame::DrawPathCallbacks* aCallbacks)
{
  // Don't pass in aRenderingContext here, because we need a *reference*
  // context and aRenderingContext might have some transform in it
  // XXX get the block and line passed to us somehow! This is slow!
  gfxSkipCharsIterator iter = EnsureTextRun(nsTextFrame::eInflated);
  if (!mTextRun)
    return;

  PropertyProvider provider(this, iter, nsTextFrame::eInflated);
  // Trim trailing whitespace
  provider.InitializeForDisplay(true);

  gfxContext* ctx = aRenderingContext->ThebesContext();
  const bool rtl = mTextRun->IsRightToLeft();
  const nscoord frameWidth = GetSize().width;
  gfxPoint framePt(aPt.x, aPt.y);
  gfxPoint textBaselinePt(rtl ? gfxFloat(aPt.x + frameWidth) : framePt.x,
             nsLayoutUtils::GetSnappedBaselineY(this, ctx, aPt.y, mAscent));
  uint32_t startOffset = provider.GetStart().GetSkippedOffset();
  uint32_t maxLength = ComputeTransformedLength(provider);
  nscoord snappedLeftEdge, snappedRightEdge;
  if (!MeasureCharClippedText(provider, aItem.mLeftEdge, aItem.mRightEdge,
         &startOffset, &maxLength, &snappedLeftEdge, &snappedRightEdge)) {
    return;
  }
  textBaselinePt.x += rtl ? -snappedRightEdge : snappedLeftEdge;
  nsCharClipDisplayItem::ClipEdges clipEdges(aItem, snappedLeftEdge,
                                             snappedRightEdge);
  nsTextPaintStyle textPaintStyle(this);
  textPaintStyle.SetResolveColors(!aCallbacks);

  gfxRect dirtyRect(aDirtyRect.x, aDirtyRect.y,
                    aDirtyRect.width, aDirtyRect.height);
  // Fork off to the (slower) paint-with-selection path if necessary.
  if (IsSelected()) {
    gfxSkipCharsIterator tmp(provider.GetStart());
    int32_t contentOffset = tmp.ConvertSkippedToOriginal(startOffset);
    int32_t contentLength =
      tmp.ConvertSkippedToOriginal(startOffset + maxLength) - contentOffset;
    if (PaintTextWithSelection(ctx, framePt, textBaselinePt, dirtyRect,
                               provider, contentOffset, contentLength,
                               textPaintStyle, clipEdges, aContextPaint,
                               aCallbacks)) {
      return;
    }
  }

  nscolor foregroundColor = textPaintStyle.GetTextColor();
  if (!aCallbacks) {
    const nsStyleText* textStyle = StyleText();
    if (textStyle->HasTextShadow()) {
      // Text shadow happens with the last value being painted at the back,
      // ie. it is painted first.
      gfxTextRun::Metrics shadowMetrics = 
        mTextRun->MeasureText(startOffset, maxLength, gfxFont::LOOSE_INK_EXTENTS,
                              nullptr, &provider);
      for (uint32_t i = textStyle->mTextShadow->Length(); i > 0; --i) {
        PaintOneShadow(startOffset, maxLength,
                       textStyle->mTextShadow->ShadowAt(i - 1), &provider,
                       aDirtyRect, framePt, textBaselinePt, ctx,
                       foregroundColor, clipEdges,
                       snappedLeftEdge, shadowMetrics.mBoundingBox);
      }
    }
  }

  gfxFloat advanceWidth;
  DrawText(ctx, dirtyRect, framePt, textBaselinePt, startOffset, maxLength, provider,
           textPaintStyle, foregroundColor, clipEdges, advanceWidth,
           (GetStateBits() & TEXT_HYPHEN_BREAK) != 0,
           nullptr, aContextPaint, aCallbacks);
}

static void
DrawTextRun(gfxTextRun* aTextRun,
            gfxContext* const aCtx,
            const gfxPoint& aTextBaselinePt,
            uint32_t aOffset, uint32_t aLength,
            PropertyProvider* aProvider,
            nscolor aTextColor,
            gfxFloat* aAdvanceWidth,
            gfxTextContextPaint* aContextPaint,
            nsTextFrame::DrawPathCallbacks* aCallbacks)
{
  DrawMode drawMode = aCallbacks ? DrawMode::GLYPH_PATH :
                                   DrawMode::GLYPH_FILL;
  if (aCallbacks) {
    aCallbacks->NotifyBeforeText(aTextColor);
    aTextRun->Draw(aCtx, aTextBaselinePt, drawMode, aOffset, aLength,
                   aProvider, aAdvanceWidth, aContextPaint, aCallbacks);
    aCallbacks->NotifyAfterText();
  } else {
    aCtx->SetColor(gfxRGBA(aTextColor));
    aTextRun->Draw(aCtx, aTextBaselinePt, drawMode, aOffset, aLength,
                   aProvider, aAdvanceWidth, aContextPaint);
  }
}

void
nsTextFrame::DrawTextRun(gfxContext* const aCtx,
                         const gfxPoint& aTextBaselinePt,
                         uint32_t aOffset, uint32_t aLength,
                         PropertyProvider& aProvider,
                         nscolor aTextColor,
                         gfxFloat& aAdvanceWidth,
                         bool aDrawSoftHyphen,
                         gfxTextContextPaint* aContextPaint,
                         nsTextFrame::DrawPathCallbacks* aCallbacks)
{
  ::DrawTextRun(mTextRun, aCtx, aTextBaselinePt, aOffset, aLength, &aProvider,
                aTextColor, &aAdvanceWidth, aContextPaint, aCallbacks);

  if (aDrawSoftHyphen) {
    // Don't use ctx as the context, because we need a reference context here,
    // ctx may be transformed.
    nsAutoPtr<gfxTextRun> hyphenTextRun(GetHyphenTextRun(mTextRun, nullptr, this));
    if (hyphenTextRun.get()) {
      // For right-to-left text runs, the soft-hyphen is positioned at the left
      // of the text, minus its own width
      gfxFloat hyphenBaselineX = aTextBaselinePt.x + mTextRun->GetDirection() * aAdvanceWidth -
        (mTextRun->IsRightToLeft() ? hyphenTextRun->GetAdvanceWidth(0, hyphenTextRun->GetLength(), nullptr) : 0);
      ::DrawTextRun(hyphenTextRun.get(), aCtx,
                    gfxPoint(hyphenBaselineX, aTextBaselinePt.y),
                    0, hyphenTextRun->GetLength(),
                    nullptr, aTextColor, nullptr, aContextPaint, aCallbacks);
    }
  }
}

void
nsTextFrame::DrawTextRunAndDecorations(
    gfxContext* const aCtx, const gfxRect& aDirtyRect,
    const gfxPoint& aFramePt, const gfxPoint& aTextBaselinePt,
    uint32_t aOffset, uint32_t aLength,
    PropertyProvider& aProvider,
    const nsTextPaintStyle& aTextStyle,
    nscolor aTextColor,
    const nsCharClipDisplayItem::ClipEdges& aClipEdges,
    gfxFloat& aAdvanceWidth,
    bool aDrawSoftHyphen,
    const TextDecorations& aDecorations,
    const nscolor* const aDecorationOverrideColor,
    gfxTextContextPaint* aContextPaint,
    nsTextFrame::DrawPathCallbacks* aCallbacks)
{
    const gfxFloat app = aTextStyle.PresContext()->AppUnitsPerDevPixel();

    // XXX aFramePt is in AppUnits, shouldn't it be nsFloatPoint?
    nscoord x = NSToCoordRound(aFramePt.x);
    nscoord width = GetRect().width;
    aClipEdges.Intersect(&x, &width);

    gfxPoint decPt(x / app, 0);
    gfxSize decSize(width / app, 0);
    const gfxFloat ascent = gfxFloat(mAscent) / app;
    const gfxFloat frameTop = aFramePt.y;

    gfxRect dirtyRect(aDirtyRect.x / app, aDirtyRect.y / app,
                      aDirtyRect.Width() / app, aDirtyRect.Height() / app);

    nscoord inflationMinFontSize =
      nsLayoutUtils::InflationMinFontSizeFor(this);

    // Underlines
    for (uint32_t i = aDecorations.mUnderlines.Length(); i-- > 0; ) {
      const LineDecoration& dec = aDecorations.mUnderlines[i];
      if (dec.mStyle == NS_STYLE_TEXT_DECORATION_STYLE_NONE) {
        continue;
      }

      float inflation =
        GetInflationForTextDecorations(dec.mFrame, inflationMinFontSize);
      const gfxFont::Metrics metrics =
        GetFirstFontMetrics(GetFontGroupForFrame(dec.mFrame, inflation));

      decSize.height = metrics.underlineSize;
      decPt.y = (frameTop - dec.mBaselineOffset) / app;

      PaintDecorationLine(this, aCtx, dirtyRect, dec.mColor,
        aDecorationOverrideColor, decPt, 0.0, decSize, ascent,
        metrics.underlineOffset, NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE,
        dec.mStyle, eNormalDecoration, aCallbacks);
    }
    // Overlines
    for (uint32_t i = aDecorations.mOverlines.Length(); i-- > 0; ) {
      const LineDecoration& dec = aDecorations.mOverlines[i];
      if (dec.mStyle == NS_STYLE_TEXT_DECORATION_STYLE_NONE) {
        continue;
      }

      float inflation =
        GetInflationForTextDecorations(dec.mFrame, inflationMinFontSize);
      const gfxFont::Metrics metrics =
        GetFirstFontMetrics(GetFontGroupForFrame(dec.mFrame, inflation));

      decSize.height = metrics.underlineSize;
      decPt.y = (frameTop - dec.mBaselineOffset) / app;

      PaintDecorationLine(this, aCtx, dirtyRect, dec.mColor,
        aDecorationOverrideColor, decPt, 0.0, decSize, ascent,
        metrics.maxAscent, NS_STYLE_TEXT_DECORATION_LINE_OVERLINE, dec.mStyle,
        eNormalDecoration, aCallbacks);
    }

    // CSS 2.1 mandates that text be painted after over/underlines, and *then*
    // line-throughs
    DrawTextRun(aCtx, aTextBaselinePt, aOffset, aLength, aProvider, aTextColor,
                aAdvanceWidth, aDrawSoftHyphen, aContextPaint, aCallbacks);

    // Line-throughs
    for (uint32_t i = aDecorations.mStrikes.Length(); i-- > 0; ) {
      const LineDecoration& dec = aDecorations.mStrikes[i];
      if (dec.mStyle == NS_STYLE_TEXT_DECORATION_STYLE_NONE) {
        continue;
      }

      float inflation =
        GetInflationForTextDecorations(dec.mFrame, inflationMinFontSize);
      const gfxFont::Metrics metrics =
        GetFirstFontMetrics(GetFontGroupForFrame(dec.mFrame, inflation));

      decSize.height = metrics.strikeoutSize;
      decPt.y = (frameTop - dec.mBaselineOffset) / app;

      PaintDecorationLine(this, aCtx, dirtyRect, dec.mColor,
        aDecorationOverrideColor, decPt, 0.0, decSize, ascent,
        metrics.strikeoutOffset, NS_STYLE_TEXT_DECORATION_LINE_LINE_THROUGH,
        dec.mStyle, eNormalDecoration, aCallbacks);
    }
}

void
nsTextFrame::DrawText(
    gfxContext* const aCtx, const gfxRect& aDirtyRect,
    const gfxPoint& aFramePt, const gfxPoint& aTextBaselinePt,
    uint32_t aOffset, uint32_t aLength,
    PropertyProvider& aProvider,
    const nsTextPaintStyle& aTextStyle,
    nscolor aTextColor,
    const nsCharClipDisplayItem::ClipEdges& aClipEdges,
    gfxFloat& aAdvanceWidth,
    bool aDrawSoftHyphen,
    const nscolor* const aDecorationOverrideColor,
    gfxTextContextPaint* aContextPaint,
    nsTextFrame::DrawPathCallbacks* aCallbacks)
{
  TextDecorations decorations;
  GetTextDecorations(aTextStyle.PresContext(),
                     aCallbacks ? eUnresolvedColors : eResolvedColors,
                     decorations);

  // Hide text decorations if we're currently hiding @font-face fallback text
  const bool drawDecorations = !aProvider.GetFontGroup()->ShouldSkipDrawing() &&
                               decorations.HasDecorationLines();
  if (drawDecorations) {
    DrawTextRunAndDecorations(aCtx, aDirtyRect, aFramePt, aTextBaselinePt, aOffset, aLength,
                              aProvider, aTextStyle, aTextColor, aClipEdges, aAdvanceWidth,
                              aDrawSoftHyphen, decorations,
                              aDecorationOverrideColor, aContextPaint, aCallbacks);
  } else {
    DrawTextRun(aCtx, aTextBaselinePt, aOffset, aLength, aProvider,
                aTextColor, aAdvanceWidth, aDrawSoftHyphen, aContextPaint, aCallbacks);
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
    
  SelectionDetails* details = GetSelectionDetails();
  bool found = false;
    
  // where are the selection points "really"
  SelectionDetails *sdptr = details;
  while (sdptr) {
    if (sdptr->mEnd > GetContentOffset() &&
        sdptr->mStart < GetContentEnd() &&
        sdptr->mType == nsISelectionController::SELECTION_NORMAL) {
      found = true;
      break;
    }
    sdptr = sdptr->mNext;
  }
  DestroySelectionDetails(details);

  return found;
}

/**
 * Compute the longest prefix of text whose width is <= aWidth. Return
 * the length of the prefix. Also returns the width of the prefix in aFitWidth.
 */
static uint32_t
CountCharsFit(gfxTextRun* aTextRun, uint32_t aStart, uint32_t aLength,
              gfxFloat aWidth, PropertyProvider* aProvider,
              gfxFloat* aFitWidth)
{
  uint32_t last = 0;
  gfxFloat width = 0;
  for (uint32_t i = 1; i <= aLength; ++i) {
    if (i == aLength || aTextRun->IsClusterStart(aStart + i)) {
      gfxFloat nextWidth = width +
          aTextRun->GetAdvanceWidth(aStart + last, i - last, aProvider);
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
nsTextFrame::CalcContentOffsetsFromFramePoint(nsPoint aPoint)
{
  return GetCharacterOffsetAtFramePointInternal(aPoint, true);
}

nsIFrame::ContentOffsets
nsTextFrame::GetCharacterOffsetAtFramePoint(const nsPoint &aPoint)
{
  return GetCharacterOffsetAtFramePointInternal(aPoint, false);
}

nsIFrame::ContentOffsets
nsTextFrame::GetCharacterOffsetAtFramePointInternal(nsPoint aPoint,
                                                    bool aForInsertionPoint)
{
  ContentOffsets offsets;
  
  gfxSkipCharsIterator iter = EnsureTextRun(nsTextFrame::eInflated);
  if (!mTextRun)
    return offsets;
  
  PropertyProvider provider(this, iter, nsTextFrame::eInflated);
  // Trim leading but not trailing whitespace if possible
  provider.InitializeForDisplay(false);
  gfxFloat width = mTextRun->IsRightToLeft() ? mRect.width - aPoint.x : aPoint.x;
  gfxFloat fitWidth;
  uint32_t skippedLength = ComputeTransformedLength(provider);

  uint32_t charsFit = CountCharsFit(mTextRun,
      provider.GetStart().GetSkippedOffset(), skippedLength, width, &provider, &fitWidth);

  int32_t selectedOffset;
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
    selectedOffset = !aForInsertionPoint || width <= fitWidth + charWidth/2
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
  offsets.associateWithNext = mContentOffset == offsets.offset;
  return offsets;
}

bool
nsTextFrame::CombineSelectionUnderlineRect(nsPresContext* aPresContext,
                                           nsRect& aRect)
{
  if (aRect.IsEmpty())
    return false;

  nsRect givenRect = aRect;

  nsRefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(this, getter_AddRefs(fm),
                                        GetFontSizeInflation());
  gfxFontGroup* fontGroup = fm->GetThebesFontGroup();
  gfxFont* firstFont = fontGroup->GetFontAt(0);
  if (!firstFont)
    return false; // OOM
  const gfxFont::Metrics& metrics = firstFont->GetMetrics();
  gfxFloat underlineOffset = fontGroup->GetUnderlineOffset();
  gfxFloat ascent = aPresContext->AppUnitsToGfxUnits(mAscent);
  gfxFloat descentLimit =
    ComputeDescentLimitForSelectionUnderline(aPresContext, this, metrics);

  SelectionDetails *details = GetSelectionDetails();
  for (SelectionDetails *sd = details; sd; sd = sd->mNext) {
    if (sd->mStart == sd->mEnd || !(sd->mType & SelectionTypesWithDecorations))
      continue;

    uint8_t style;
    float relativeSize;
    int32_t index =
      nsTextPaintStyle::GetUnderlineStyleIndexForSelectionType(sd->mType);
    if (sd->mType == nsISelectionController::SELECTION_SPELLCHECK) {
      if (!nsTextPaintStyle::GetSelectionUnderline(aPresContext, index, nullptr,
                                                   &relativeSize, &style)) {
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
        style = rangeStyle.mLineStyle;
        relativeSize = rangeStyle.mIsBoldLine ? 2.0f : 1.0f;
      } else if (!nsTextPaintStyle::GetSelectionUnderline(aPresContext, index,
                                                          nullptr, &relativeSize,
                                                          &style)) {
        continue;
      }
    }
    nsRect decorationArea;
    gfxSize size(aPresContext->AppUnitsToGfxUnits(aRect.width),
                 ComputeSelectionUnderlineHeight(aPresContext,
                                                 metrics, sd->mType));
    relativeSize = std::max(relativeSize, 1.0f);
    size.height *= relativeSize;
    decorationArea =
      nsCSSRendering::GetTextDecorationRect(aPresContext, size,
        ascent, underlineOffset, NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE,
        style, descentLimit);
    aRect.UnionRect(aRect, decorationArea);
  }
  DestroySelectionDetails(details);

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
                              SelectionType aType)
{
  NS_ASSERTION(!GetPrevContinuation(), "Should only be called for primary frame");
  DEBUG_VERIFY_NOT_DIRTY(mState);

  // Selection is collapsed, which can't affect text frame rendering
  if (aStart == aEnd)
    return;

  nsTextFrame* f = this;
  while (f && f->GetContentEnd() <= int32_t(aStart)) {
    f = static_cast<nsTextFrame*>(f->GetNextContinuation());
  }

  nsPresContext* presContext = PresContext();
  while (f && f->GetContentOffset() < int32_t(aEnd)) {
    // We may need to reflow to recompute the overflow area for
    // spellchecking or IME underline if their underline is thicker than
    // the normal decoration line.
    if (aType & SelectionTypesWithDecorations) {
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

    f = static_cast<nsTextFrame*>(f->GetNextContinuation());
  }
}

NS_IMETHODIMP
nsTextFrame::GetPointFromOffset(int32_t inOffset,
                                nsPoint* outPoint)
{
  if (!outPoint)
    return NS_ERROR_NULL_POINTER;

  outPoint->x = 0;
  outPoint->y = 0;

  DEBUG_VERIFY_NOT_DIRTY(mState);
  if (mState & NS_FRAME_IS_DIRTY)
    return NS_ERROR_UNEXPECTED;

  if (GetContentLength() <= 0) {
    return NS_OK;
  }

  gfxSkipCharsIterator iter = EnsureTextRun(nsTextFrame::eInflated);
  if (!mTextRun)
    return NS_ERROR_FAILURE;

  PropertyProvider properties(this, iter, nsTextFrame::eInflated);
  // Don't trim trailing whitespace, we want the caret to appear in the right
  // place if it's positioned there
  properties.InitializeForDisplay(false);  

  if (inOffset < GetContentOffset()){
    NS_WARNING("offset before this frame's content");
    inOffset = GetContentOffset();
  } else if (inOffset > GetContentEnd()) {
    NS_WARNING("offset after this frame's content");
    inOffset = GetContentEnd();
  }
  int32_t trimmedOffset = properties.GetStart().GetOriginalOffset();
  int32_t trimmedEnd = trimmedOffset + properties.GetOriginalLength();
  inOffset = std::max(inOffset, trimmedOffset);
  inOffset = std::min(inOffset, trimmedEnd);

  iter.SetOriginalOffset(inOffset);

  if (inOffset < trimmedEnd &&
      !iter.IsOriginalCharSkipped() &&
      !mTextRun->IsClusterStart(iter.GetSkippedOffset())) {
    NS_WARNING("GetPointFromOffset called for non-cluster boundary");
    FindClusterStart(mTextRun, trimmedOffset, &iter);
  }

  gfxFloat advanceWidth =
    mTextRun->GetAdvanceWidth(properties.GetStart().GetSkippedOffset(),
                              GetSkippedDistance(properties.GetStart(), iter),
                              &properties);
  nscoord width = NSToCoordCeilClamped(advanceWidth);

  if (mTextRun->IsRightToLeft()) {
    outPoint->x = mRect.width - width;
  } else {
    outPoint->x = width;
  }
  outPoint->y = 0;

  return NS_OK;
}

NS_IMETHODIMP
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
  nsTextFrame* cachedFrame = static_cast<nsTextFrame*>
    (Properties().Get(OffsetToFrameProperty()));

  if (cachedFrame) {
    f = cachedFrame;
    offset = f->GetContentOffset();

    f->RemoveStateBits(TEXT_IN_OFFSET_CACHE);
  }

  if ((aContentOffset >= offset) &&
      (aHint || aContentOffset != offset)) {
    while (true) {
      nsTextFrame* next = static_cast<nsTextFrame*>(f->GetNextContinuation());
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
      nsTextFrame* prev = static_cast<nsTextFrame*>(f->GetPrevContinuation());
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
  Properties().Set(OffsetToFrameProperty(), f);
  f->AddStateBits(TEXT_IN_OFFSET_CACHE);

  return NS_OK;
}

bool
nsTextFrame::PeekOffsetNoAmount(bool aForward, int32_t* aOffset)
{
  NS_ASSERTION(aOffset && *aOffset <= GetContentLength(), "aOffset out of range");

  gfxSkipCharsIterator iter = EnsureTextRun(nsTextFrame::eInflated);
  if (!mTextRun)
    return false;

  TrimmedOffsets trimmed = GetTrimmedOffsets(mContent->GetText(), true);
  // Check whether there are nonskipped characters in the trimmmed range
  return iter.ConvertOriginalToSkipped(trimmed.GetEnd()) >
         iter.ConvertOriginalToSkipped(trimmed.mStart);
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
                          gfxTextRun* aTextRun,
                          nsIFrame* aFrame)
{
  if (aIter.IsOriginalCharSkipped())
    return false;
  uint32_t index = aIter.GetSkippedOffset();
  if (aRespectClusters && !aTextRun->IsClusterStart(index))
    return false;
  if (index > 0) {
    // Check whether the proposed position is in between the two halves of a
    // surrogate pair; if so, this is not a valid character boundary.
    // (In the case where we are respecting clusters, we won't actually get
    // this far because the low surrogate is also marked as non-clusterStart
    // so we'll return FALSE above.)
    if (aTextRun->CharIsLowSurrogate(index)) {
      return false;
    }
  }
  return true;
}

bool
nsTextFrame::PeekOffsetCharacter(bool aForward, int32_t* aOffset,
                                 bool aRespectClusters)
{
  int32_t contentLength = GetContentLength();
  NS_ASSERTION(aOffset && *aOffset <= contentLength, "aOffset out of range");

  bool selectable;
  uint8_t selectStyle;  
  IsSelectable(&selectable, &selectStyle);
  if (selectStyle == NS_STYLE_USER_SELECT_ALL)
    return false;

  gfxSkipCharsIterator iter = EnsureTextRun(nsTextFrame::eInflated);
  if (!mTextRun)
    return false;

  TrimmedOffsets trimmed = GetTrimmedOffsets(mContent->GetText(), false);

  // A negative offset means "end of frame".
  int32_t startOffset = GetContentOffset() + (*aOffset < 0 ? contentLength : *aOffset);

  if (!aForward) {
    // If at the beginning of the line, look at the previous continuation
    for (int32_t i = std::min(trimmed.GetEnd(), startOffset) - 1;
         i >= trimmed.mStart; --i) {
      iter.SetOriginalOffset(i);
      if (IsAcceptableCaretPosition(iter, aRespectClusters, mTextRun, this)) {
        *aOffset = i - mContentOffset;
        return true;
      }
    }
    *aOffset = 0;
  } else {
    // If we're at the end of a line, look at the next continuation
    iter.SetOriginalOffset(startOffset);
    if (startOffset <= trimmed.GetEnd() &&
        !(startOffset < trimmed.GetEnd() &&
          StyleText()->NewlineIsSignificant() &&
          iter.GetSkippedOffset() < mTextRun->GetLength() &&
          mTextRun->CharIsNewline(iter.GetSkippedOffset()))) {
      for (int32_t i = startOffset + 1; i <= trimmed.GetEnd(); ++i) {
        iter.SetOriginalOffset(i);
        if (i == trimmed.GetEnd() ||
            IsAcceptableCaretPosition(iter, aRespectClusters, mTextRun, this)) {
          *aOffset = i - mContentOffset;
          return true;
        }
      }
    }
    *aOffset = contentLength;
  }
  
  return false;
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
  nsIUGenCategory::nsUGenCategory c =
    mozilla::unicode::GetGenCategory(mFrag->CharAt(mCharIndex));
  return c == nsIUGenCategory::kPunctuation || c == nsIUGenCategory::kSymbol;
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
  gfxTextRun* textRun = mTextFrame->GetTextRun(nsTextFrame::eInflated);

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
  nsIWordBreaker* wordBreaker = nsContentUtils::WordBreaker();
  for (int32_t i = 0; i <= textLen; ++i) {
    int32_t indexInText = i + textStart;
    mWordBreaks[i] |=
      wordBreaker->BreakInBetween(aContext.get(), indexInText,
                                  aContext.get() + indexInText,
                                  aContext.Length() - indexInText);
  }
}

bool
nsTextFrame::PeekOffsetWord(bool aForward, bool aWordSelectEatSpace, bool aIsKeyboardSelect,
                            int32_t* aOffset, PeekWordState* aState)
{
  int32_t contentLength = GetContentLength();
  NS_ASSERTION (aOffset && *aOffset <= contentLength, "aOffset out of range");

  bool selectable;
  uint8_t selectStyle;
  IsSelectable(&selectable, &selectStyle);
  if (selectStyle == NS_STYLE_USER_SELECT_ALL)
    return false;

  int32_t offset = GetContentOffset() + (*aOffset < 0 ? contentLength : *aOffset);
  ClusterIterator cIter(this, offset, aForward ? 1 : -1, aState->mContext);

  if (!cIter.NextCluster())
    return false;

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
        return true;
      }
    }
    aState->Update(isPunctuation, isWhitespace);
  } while (cIter.NextCluster());

  *aOffset = cIter.GetAfterOffset() - mContentOffset;
  return false;
}

 // TODO this needs to be deCOMtaminated with the interface fixed in
// nsIFrame.h, but we won't do that until the old textframe is gone.
NS_IMETHODIMP
nsTextFrame::CheckVisibility(nsPresContext* aContext, int32_t aStartIndex,
    int32_t aEndIndex, bool aRecurse, bool *aFinished, bool *aRetval)
{
  if (!aRetval)
    return NS_ERROR_NULL_POINTER;

  // Text in the range is visible if there is at least one character in the range
  // that is not skipped and is mapped by this frame (which is the primary frame)
  // or one of its continuations.
  for (nsTextFrame* f = this; f;
       f = static_cast<nsTextFrame*>(GetNextContinuation())) {
    int32_t dummyOffset = 0;
    if (f->PeekOffsetNoAmount(true, &dummyOffset)) {
      *aRetval = true;
      return NS_OK;
    }
  }

  *aRetval = false;
  return NS_OK;
}

NS_IMETHODIMP
nsTextFrame::GetOffsets(int32_t &start, int32_t &end) const
{
  start = GetContentOffset();
  end = GetContentEnd();
  return NS_OK;
}

static int32_t
FindEndOfPunctuationRun(const nsTextFragment* aFrag,
                        gfxTextRun* aTextRun,
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
                     gfxTextRun* aTextRun,
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
  iter.SetOriginalOffset(aOffset + i);
  FindClusterEnd(aTextRun, endOffset, &iter);
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
                                 nsIFrame::InlineIntrinsicWidthData* aData,
                                 const nsStyleText* aTextStyle,
                                 gfxSkipCharsIterator* aIterator,
                                 uint32_t aFlowEndInTextRun)
{
  if (aData->skipWhitespace) {
    while (aIterator->GetSkippedOffset() < aFlowEndInTextRun &&
           IsTrimmableSpace(aProvider->GetFragment(), aIterator->GetOriginalOffset(), aTextStyle)) {
      aIterator->AdvanceOriginal(1);
    }
  }
  return aIterator->GetSkippedOffset();
}

union VoidPtrOrFloat {
  VoidPtrOrFloat() : p(nullptr) {}

  void *p;
  float f;
};

float
nsTextFrame::GetFontSizeInflation() const
{
  if (!HasFontSizeInflation()) {
    return 1.0f;
  }
  VoidPtrOrFloat u;
  u.p = Properties().Get(FontSizeInflationProperty());
  return u.f;
}

void
nsTextFrame::SetFontSizeInflation(float aInflation)
{
  if (aInflation == 1.0f) {
    if (HasFontSizeInflation()) {
      RemoveStateBits(TEXT_HAS_FONT_INFLATION);
      Properties().Delete(FontSizeInflationProperty());
    }
    return;
  }

  AddStateBits(TEXT_HAS_FONT_INFLATION);
  VoidPtrOrFloat u;
  u.f = aInflation;
  Properties().Set(FontSizeInflationProperty(), u.p);
}

/* virtual */ 
void nsTextFrame::MarkIntrinsicWidthsDirty()
{
  ClearTextRuns();
  nsFrame::MarkIntrinsicWidthsDirty();
}

// XXX this doesn't handle characters shaped by line endings. We need to
// temporarily override the "current line ending" settings.
void
nsTextFrame::AddInlineMinWidthForFlow(nsRenderingContext *aRenderingContext,
                                      nsIFrame::InlineMinWidthData *aData,
                                      TextRunType aTextRunType)
{
  uint32_t flowEndInTextRun;
  gfxContext* ctx = aRenderingContext->ThebesContext();
  gfxSkipCharsIterator iter =
    EnsureTextRun(aTextRunType, ctx, aData->lineContainer,
                  aData->line, &flowEndInTextRun);
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
    (textStyle->mHyphens == NS_STYLE_HYPHENS_AUTO ||
     (textStyle->mHyphens == NS_STYLE_HYPHENS_MANUAL &&
      (textRun->GetFlags() & gfxTextRunFactory::TEXT_ENABLE_HYPHEN_BREAKS) != 0));
  if (hyphenating) {
    gfxSkipCharsIterator tmp(iter);
    len = std::min<int32_t>(GetContentOffset() + GetInFlowContentLength(),
                 tmp.ConvertSkippedToOriginal(flowEndInTextRun)) - iter.GetOriginalOffset();
  }
  PropertyProvider provider(textRun, textStyle, frag, this,
                            iter, len, nullptr, 0, aTextRunType);

  bool collapseWhitespace = !textStyle->WhiteSpaceIsSignificant();
  bool preformatNewlines = textStyle->NewlineIsSignificant();
  bool preformatTabs = textStyle->WhiteSpaceIsSignificant();
  gfxFloat tabWidth = -1;
  uint32_t start =
    FindStartAfterSkippingWhitespace(&provider, aData, textStyle, &iter, flowEndInTextRun);

  AutoFallibleTArray<bool,BIG_TEXT_NODE_SIZE> hyphBuffer;
  bool *hyphBreakBefore = nullptr;
  if (hyphenating) {
    hyphBreakBefore = hyphBuffer.AppendElements(flowEndInTextRun - start);
    if (hyphBreakBefore) {
      provider.GetHyphenationBreaks(start, flowEndInTextRun - start,
                                    hyphBreakBefore);
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
          (!hyphBreakBefore || !hyphBreakBefore[i - start]))
      {
        // we can't break here (and it's not the end of the flow)
        continue;
      }
    }

    if (i > wordStart) {
      nscoord width =
        NSToCoordCeilClamped(textRun->GetAdvanceWidth(wordStart, i - wordStart, &provider));
      aData->currentLine = NSCoordSaturatingAdd(aData->currentLine, width);
      aData->atStartOfLine = false;

      if (collapseWhitespace) {
        uint32_t trimStart = GetEndOfTrimmedText(frag, textStyle, wordStart, i, &iter);
        if (trimStart == start) {
          // This is *all* trimmable whitespace, so whatever trailingWhitespace
          // we saw previously is still trailing...
          aData->trailingWhitespace += width;
        } else {
          // Some non-whitespace so the old trailingWhitespace is no longer trailing
          aData->trailingWhitespace =
            NSToCoordCeilClamped(textRun->GetAdvanceWidth(trimStart, i - trimStart, &provider));
        }
      } else {
        aData->trailingWhitespace = 0;
      }
    }

    if (preformattedTab) {
      PropertyProvider::Spacing spacing;
      provider.GetSpacing(i, 1, &spacing);
      aData->currentLine += nscoord(spacing.mBefore);
      gfxFloat afterTab =
        AdvanceToNextTab(aData->currentLine, this,
                         textRun, &tabWidth);
      aData->currentLine = nscoord(afterTab + spacing.mAfter);
      wordStart = i + 1;
    } else if (i < flowEndInTextRun ||
        (i == textRun->GetLength() &&
         (textRun->GetFlags() & nsTextFrameUtils::TEXT_HAS_TRAILING_BREAK))) {
      if (preformattedNewline) {
        aData->ForceBreak(aRenderingContext);
      } else if (i < flowEndInTextRun && hyphBreakBefore &&
                 hyphBreakBefore[i - start])
      {
        aData->OptionallyBreak(aRenderingContext, 
                               NSToCoordRound(provider.GetHyphenWidth()));
      } else {
        aData->OptionallyBreak(aRenderingContext);
      }
      wordStart = i;
    }
  }

  if (start < flowEndInTextRun) {
    // Check if we have collapsible whitespace at the end
    aData->skipWhitespace =
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
nsTextFrame::AddInlineMinWidth(nsRenderingContext *aRenderingContext,
                               nsIFrame::InlineMinWidthData *aData)
{
  float inflation = nsLayoutUtils::FontSizeInflationFor(this);
  TextRunType trtype = (inflation == 1.0f) ? eNotInflated : eInflated;

  if (trtype == eInflated && !IsCurrentFontInflation(inflation)) {
    // FIXME: Ideally, if we already have a text run, we'd move it to be
    // the uninflated text run.
    ClearTextRun(nullptr, nsTextFrame::eInflated);
  }

  nsTextFrame* f;
  gfxTextRun* lastTextRun = nullptr;
  // nsContinuingTextFrame does nothing for AddInlineMinWidth; all text frames
  // in the flow are handled right here.
  for (f = this; f; f = static_cast<nsTextFrame*>(f->GetNextContinuation())) {
    // f->GetTextRun(nsTextFrame::eNotInflated) could be null if we
    // haven't set up textruns yet for f.  Except in OOM situations,
    // lastTextRun will only be null for the first text frame.
    if (f == this || f->GetTextRun(trtype) != lastTextRun) {
      nsIFrame* lc;
      if (aData->lineContainer &&
          aData->lineContainer != (lc = FindLineContainer(f))) {
        NS_ASSERTION(f != this, "wrong InlineMinWidthData container"
                                " for first continuation");
        aData->line = nullptr;
        aData->lineContainer = lc;
      }

      // This will process all the text frames that share the same textrun as f.
      f->AddInlineMinWidthForFlow(aRenderingContext, aData, trtype);
      lastTextRun = f->GetTextRun(trtype);
    }
  }
}

// XXX this doesn't handle characters shaped by line endings. We need to
// temporarily override the "current line ending" settings.
void
nsTextFrame::AddInlinePrefWidthForFlow(nsRenderingContext *aRenderingContext,
                                       nsIFrame::InlinePrefWidthData *aData,
                                       TextRunType aTextRunType)
{
  uint32_t flowEndInTextRun;
  gfxContext* ctx = aRenderingContext->ThebesContext();
  gfxSkipCharsIterator iter =
    EnsureTextRun(aTextRunType, ctx, aData->lineContainer,
                  aData->line, &flowEndInTextRun);
  gfxTextRun *textRun = GetTextRun(aTextRunType);
  if (!textRun)
    return;

  // Pass null for the line container. This will disable tab spacing, but that's
  // OK since we can't really handle tabs for intrinsic sizing anyway.
  
  const nsStyleText* textStyle = StyleText();
  const nsTextFragment* frag = mContent->GetText();
  PropertyProvider provider(textRun, textStyle, frag, this,
                            iter, INT32_MAX, nullptr, 0, aTextRunType);

  bool collapseWhitespace = !textStyle->WhiteSpaceIsSignificant();
  bool preformatNewlines = textStyle->NewlineIsSignificant();
  bool preformatTabs = textStyle->WhiteSpaceIsSignificant();
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
      NS_ASSERTION(preformatNewlines || textStyle->NewlineIsDiscarded(),
                   "We can't be here unless newlines are hard breaks or are discarded");
      preformattedNewline = preformatNewlines && textRun->CharIsNewline(i);
      preformattedTab = preformatTabs && textRun->CharIsTab(i);
      if (!preformattedNewline && !preformattedTab) {
        // we needn't break here (and it's not the end of the flow)
        continue;
      }
    }

    if (i > lineStart) {
      nscoord width =
        NSToCoordCeilClamped(textRun->GetAdvanceWidth(lineStart, i - lineStart, &provider));
      aData->currentLine = NSCoordSaturatingAdd(aData->currentLine, width);

      if (collapseWhitespace) {
        uint32_t trimStart = GetEndOfTrimmedText(frag, textStyle, lineStart, i, &iter);
        if (trimStart == start) {
          // This is *all* trimmable whitespace, so whatever trailingWhitespace
          // we saw previously is still trailing...
          aData->trailingWhitespace += width;
        } else {
          // Some non-whitespace so the old trailingWhitespace is no longer trailing
          aData->trailingWhitespace =
            NSToCoordCeilClamped(textRun->GetAdvanceWidth(trimStart, i - trimStart, &provider));
        }
      } else {
        aData->trailingWhitespace = 0;
      }
    }

    if (preformattedTab) {
      PropertyProvider::Spacing spacing;
      provider.GetSpacing(i, 1, &spacing);
      aData->currentLine += nscoord(spacing.mBefore);
      gfxFloat afterTab =
        AdvanceToNextTab(aData->currentLine, this,
                         textRun, &tabWidth);
      aData->currentLine = nscoord(afterTab + spacing.mAfter);
      lineStart = i + 1;
    } else if (preformattedNewline) {
      aData->ForceBreak(aRenderingContext);
      lineStart = i;
    }
  }

  // Check if we have collapsible whitespace at the end
  if (start < flowEndInTextRun) {
    aData->skipWhitespace =
      IsTrimmableSpace(provider.GetFragment(),
                       iter.ConvertSkippedToOriginal(flowEndInTextRun - 1),
                       textStyle);
  }
}

// XXX Need to do something here to avoid incremental reflow bugs due to
// first-line and first-letter changing pref-width
/* virtual */ void
nsTextFrame::AddInlinePrefWidth(nsRenderingContext *aRenderingContext,
                                nsIFrame::InlinePrefWidthData *aData)
{
  float inflation = nsLayoutUtils::FontSizeInflationFor(this);
  TextRunType trtype = (inflation == 1.0f) ? eNotInflated : eInflated;

  if (trtype == eInflated && !IsCurrentFontInflation(inflation)) {
    // FIXME: Ideally, if we already have a text run, we'd move it to be
    // the uninflated text run.
    ClearTextRun(nullptr, nsTextFrame::eInflated);
  }

  nsTextFrame* f;
  gfxTextRun* lastTextRun = nullptr;
  // nsContinuingTextFrame does nothing for AddInlineMinWidth; all text frames
  // in the flow are handled right here.
  for (f = this; f; f = static_cast<nsTextFrame*>(f->GetNextContinuation())) {
    // f->GetTextRun(nsTextFrame::eNotInflated) could be null if we
    // haven't set up textruns yet for f.  Except in OOM situations,
    // lastTextRun will only be null for the first text frame.
    if (f == this || f->GetTextRun(trtype) != lastTextRun) {
      nsIFrame* lc;
      if (aData->lineContainer &&
          aData->lineContainer != (lc = FindLineContainer(f))) {
        NS_ASSERTION(f != this, "wrong InlinePrefWidthData container"
                                " for first continuation");
        aData->line = nullptr;
        aData->lineContainer = lc;
      }

      // This will process all the text frames that share the same textrun as f.
      f->AddInlinePrefWidthForFlow(aRenderingContext, aData, trtype);
      lastTextRun = f->GetTextRun(trtype);
    }
  }
}

/* virtual */ nsSize
nsTextFrame::ComputeSize(nsRenderingContext *aRenderingContext,
                         nsSize aCBSize, nscoord aAvailableWidth,
                         nsSize aMargin, nsSize aBorder, nsSize aPadding,
                         uint32_t aFlags)
{
  // Inlines and text don't compute size before reflow.
  return nsSize(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
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
nsTextFrame::ComputeTightBounds(gfxContext* aContext) const
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
        mTextRun->MeasureText(provider.GetStart().GetSkippedOffset(),
                              ComputeTransformedLength(provider),
                              gfxFont::TIGHT_HINTED_OUTLINE_EXTENTS,
                              aContext, &provider);
  // mAscent should be the same as metrics.mAscent, but it's what we use to
  // paint so that's the one we'll use.
  return RoundOut(metrics.mBoundingBox) + nsPoint(0, mAscent);
}

/* virtual */ nsresult
nsTextFrame::GetPrefWidthTightBounds(nsRenderingContext* aContext,
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
        mTextRun->MeasureText(provider.GetStart().GetSkippedOffset(),
                              ComputeTransformedLength(provider),
                              gfxFont::TIGHT_HINTED_OUTLINE_EXTENTS,
                              aContext->ThebesContext(), &provider);
  // Round it like nsTextFrame::ComputeTightBounds() to ensure consistency.
  *aX = NSToCoordFloor(metrics.mBoundingBox.x);
  *aXMost = NSToCoordCeil(metrics.mBoundingBox.XMost());

  return NS_OK;
}

static bool
HasSoftHyphenBefore(const nsTextFragment* aFrag, gfxTextRun* aTextRun,
                    int32_t aStartOffset, const gfxSkipCharsIterator& aIter)
{
  if (aIter.GetSkippedOffset() < aTextRun->GetLength() &&
      aTextRun->CanHyphenateBefore(aIter.GetSkippedOffset())) {
    return true;
  }
  if (!(aTextRun->GetFlags() & nsTextFrameUtils::TEXT_HAS_SHY))
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

static void
RemoveInFlows(nsTextFrame* aFrame, nsTextFrame* aFirstToNotRemove)
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
  
  nsIFrame* prevContinuation = aFrame->GetPrevContinuation();
  nsIFrame* lastRemoved = aFirstToNotRemove->GetPrevContinuation();

  // Clear the text run on the first frame we'll remove to make sure none of
  // the frames we keep shares its text run.  We need to do this now, before
  // we unlink the frames to remove from the flow, because DestroyFrom calls
  // ClearTextRuns() and that will start at the first frame with the text
  // run and walk the continuations.  We only need to care about the first
  // and last frames we remove since text runs are contiguous.
  aFrame->ClearTextRuns();
  if (aFrame != lastRemoved) {
    // Clear the text run on the last frame we'll remove for the same reason.
    static_cast<nsTextFrame*>(lastRemoved)->ClearTextRuns();
  }

  prevContinuation->SetNextInFlow(aFirstToNotRemove);
  aFirstToNotRemove->SetPrevInFlow(prevContinuation);

  aFrame->SetPrevInFlow(nullptr);
  lastRemoved->SetNextInFlow(nullptr);

  nsIFrame* parent = aFrame->GetParent();
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
  nsTextFrame* f = static_cast<nsTextFrame*>(GetNextInFlow());
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
    if (aLineLayout &&
        HasSignificantTerminalNewline() &&
        GetParent()->GetType() != nsGkAtoms::letterFrame &&
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
    nsTextFrame* next = static_cast<nsTextFrame*>(f->GetNextInFlow());
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

      // Important: if |f| has the same style context as its prev continuation,
      // mark it accordingly so we can skip clearing textruns as needed.  Note
      // that at this point f always has a prev continuation.
      if (f->StyleContext() == f->GetPrevContinuation()->StyleContext()) {
        f->AddStateBits(TEXT_STYLE_MATCHES_PREV_CONTINUATION);
      }
    } else if (framesToRemove) {
      RemoveInFlows(framesToRemove, f);
      framesToRemove = nullptr;
    }
    f = next;
  }
  NS_POSTCONDITION(!framesToRemove || (f && f->mContentOffset == end),
                   "How did we exit the loop if we null out framesToRemove if "
                   "!next || next->mContentOffset > end ?");
  if (framesToRemove) {
    // We are guaranteed that we exited the loop with f not null, per the
    // postcondition above
    RemoveInFlows(framesToRemove, f);
  }

#ifdef DEBUG
  f = this;
  int32_t iterations = 0;
  while (f && iterations < 10) {
    f->GetContentLength(); // Assert if negative length
    f = static_cast<nsTextFrame*>(f->GetNextContinuation());
    ++iterations;
  }
  f = this;
  iterations = 0;
  while (f && iterations < 10) {
    f->GetContentLength(); // Assert if negative length
    f = static_cast<nsTextFrame*>(f->GetPrevContinuation());
    ++iterations;
  }
#endif
}

bool
nsTextFrame::IsFloatingFirstLetterChild() const
{
  nsIFrame* frame = GetParent();
  return frame && frame->IsFloating() &&
         frame->GetType() == nsGkAtoms::letterFrame;
}

struct NewlineProperty {
  int32_t mStartOffset;
  // The offset of the first \n after mStartOffset, or -1 if there is none
  int32_t mNewlineOffset;

  static void Destroy(void* aObject, nsIAtom* aPropertyName,
                      void* aPropertyValue, void* aData)
  {
    delete static_cast<NewlineProperty*>(aPropertyValue);
  }
};

NS_IMETHODIMP
nsTextFrame::Reflow(nsPresContext*           aPresContext,
                    nsHTMLReflowMetrics&     aMetrics,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsTextFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aMetrics, aStatus);

  // XXX If there's no line layout, we shouldn't even have created this
  // frame. This may happen if, for example, this is text inside a table
  // but not inside a cell. For now, just don't reflow.
  if (!aReflowState.mLineLayout) {
    ClearMetrics(aMetrics);
    aStatus = NS_FRAME_COMPLETE;
    return NS_OK;
  }

  ReflowText(*aReflowState.mLineLayout, aReflowState.AvailableWidth(),
             aReflowState.rendContext, aMetrics, aStatus);

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aMetrics);
  return NS_OK;
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
                        nsRenderingContext* aRenderingContext,
                        nsHTMLReflowMetrics& aMetrics,
                        nsReflowStatus& aStatus)
{
#ifdef NOISY_REFLOW
  ListTag(stdout);
  printf(": BeginReflow: availableWidth=%d\n", aAvailableWidth);
#endif

  nsPresContext* presContext = PresContext();

#ifdef ACCESSIBILITY
  // Schedule the update of accessible tree since rendered text might be changed.
  ReflowTextA11yNotifier(presContext, mContent);
#endif

  /////////////////////////////////////////////////////////////////////
  // Set up flags and clear out state
  /////////////////////////////////////////////////////////////////////

  // Clear out the reflow state flags in mState. We also clear the whitespace
  // flags because this can change whether the frame maps whitespace-only text
  // or not.
  RemoveStateBits(TEXT_REFLOW_FLAGS | TEXT_WHITESPACE_FLAGS);

  // Temporarily map all possible content while we construct our new textrun.
  // so that when doing reflow our styles prevail over any part of the
  // textrun we look at. Note that next-in-flows may be mapping the same
  // content; gfxTextRun construction logic will ensure that we take priority.
  int32_t maxContentLength = GetInFlowContentLength();

  // We don't need to reflow if there is no content.
  if (!maxContentLength) {
    ClearMetrics(aMetrics);
    aStatus = NS_FRAME_COMPLETE;
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
  gfxContext* ctx = aRenderingContext->ThebesContext();
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
  if (textStyle->NewlineIsSignificant()) {
    cachedNewlineOffset =
      static_cast<NewlineProperty*>(mContent->GetProperty(nsGkAtoms::newline));
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
        EnsureTextRun(nsTextFrame::eInflated, ctx,
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
    EnsureTextRun(nsTextFrame::eInflated, ctx,
                  lineContainer, aLineLayout.GetLine(), &flowEndInTextRun);

  NS_ASSERTION(IsCurrentFontInflation(fontSizeInflation),
               "EnsureTextRun should have set font size inflation");

  if (mTextRun && iter.GetOriginalEnd() < offset + length) {
    // The textrun does not map enough text for this frame. This can happen
    // when the textrun was ended in the middle of a text node because a
    // preformatted newline was encountered, and prev-in-flow frames have
    // consumed all the text of the textrun. We need a new textrun.
    ClearTextRuns();
    iter = EnsureTextRun(nsTextFrame::eInflated, ctx,
                         lineContainer, aLineLayout.GetLine(),
                         &flowEndInTextRun);
  }

  if (!mTextRun) {
    ClearMetrics(aMetrics);
    aStatus = NS_FRAME_COMPLETE;
    return;
  }

  NS_ASSERTION(gfxSkipCharsIterator(iter).ConvertOriginalToSkipped(offset + length)
                    <= mTextRun->GetLength(),
               "Text run does not map enough text for our reflow");

  /////////////////////////////////////////////////////////////////////
  // See how much text should belong to this text frame, and measure it
  /////////////////////////////////////////////////////////////////////
  
  iter.SetOriginalOffset(offset);
  nscoord xOffsetForTabs = (mTextRun->GetFlags() & nsTextFrameUtils::TEXT_HAS_TAB) ?
    (aLineLayout.GetCurrentFrameXDistanceFromBlock() -
       lineContainer->GetUsedBorderAndPadding().left)
    : -1;
  PropertyProvider provider(mTextRun, textStyle, frag, this, iter, length,
      lineContainer, xOffsetForTabs, nsTextFrame::eInflated);

  uint32_t transformedOffset = provider.GetStart().GetSkippedOffset();

  // The metrics for the text go in here
  gfxTextRun::Metrics textMetrics;
  gfxFont::BoundingBoxType boundingBoxType = IsFloatingFirstLetterChild() ?
                                               gfxFont::TIGHT_HINTED_OUTLINE_EXTENTS :
                                               gfxFont::LOOSE_INK_EXTENTS;
  NS_ASSERTION(!(NS_REFLOW_CALC_BOUNDING_METRICS & aMetrics.mFlags),
               "We shouldn't be passed NS_REFLOW_CALC_BOUNDING_METRICS anymore");

  int32_t limitLength = length;
  int32_t forceBreak = aLineLayout.GetForcedBreakPosition(mContent);
  bool forceBreakAfter = false;
  if (forceBreak >= offset + length) {
    forceBreakAfter = forceBreak == offset + length;
    // The break is not within the text considered for this textframe.
    forceBreak = -1;
  }
  if (forceBreak >= 0) {
    limitLength = forceBreak - offset;
    NS_ASSERTION(limitLength >= 0, "Weird break found!");
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
  bool canTrimTrailingWhitespace = !textStyle->WhiteSpaceIsSignificant() ||
                                   (GetStateBits() & TEXT_IS_IN_TOKEN_MATHML);
  int32_t unusedOffset;  
  gfxBreakPriority breakPriority;
  aLineLayout.GetLastOptionalBreakPosition(&unusedOffset, &breakPriority);
  uint32_t transformedCharsFit =
    mTextRun->BreakAndMeasureText(transformedOffset, transformedLength,
                                  (GetStateBits() & TEXT_START_OF_LINE) != 0,
                                  availWidth,
                                  &provider, !aLineLayout.LineIsBreakable(),
                                  canTrimTrailingWhitespace ? &trimmedWidth : nullptr,
                                  &textMetrics, boundingBoxType, ctx,
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
    AddHyphenToMetrics(this, mTextRun, &textMetrics, boundingBoxType, ctx);
    AddStateBits(TEXT_HYPHEN_BREAK | TEXT_HAS_NONCOLLAPSED_CHARACTERS);
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
    NS_ASSERTION(textMetrics.mAdvanceWidth - trimmableWidth <= aAvailableWidth,
                 "If the text doesn't fit, and we have a break opportunity, why didn't MeasureText use it?");
    aLineLayout.NotifyOptionalBreakPosition(mContent, lastBreak, true, breakPriority);
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
  aMetrics.Width() = NSToCoordCeil(std::max(gfxFloat(0.0), textMetrics.mAdvanceWidth));

  if (transformedCharsFit == 0 && !usedHyphenation) {
    aMetrics.SetTopAscent(0);
    aMetrics.Height() = 0;
  } else if (boundingBoxType != gfxFont::LOOSE_INK_EXTENTS) {
    // Use actual text metrics for floating first letter frame.
    aMetrics.SetTopAscent(NSToCoordCeil(textMetrics.mAscent));
    aMetrics.Height() = aMetrics.TopAscent() + NSToCoordCeil(textMetrics.mDescent);
  } else {
    // Otherwise, ascent should contain the overline drawable area.
    // And also descent should contain the underline drawable area.
    // nsFontMetrics::GetMaxAscent/GetMaxDescent contains them.
    nsFontMetrics* fm = provider.GetFontMetrics();
    nscoord fontAscent = fm->MaxAscent();
    nscoord fontDescent = fm->MaxDescent();
    aMetrics.SetTopAscent(std::max(NSToCoordCeil(textMetrics.mAscent), fontAscent));
    nscoord descent = std::max(NSToCoordCeil(textMetrics.mDescent), fontDescent);
    aMetrics.Height() = aMetrics.TopAscent() + descent;
  }

  NS_ASSERTION(aMetrics.TopAscent() >= 0, "Negative ascent???");
  NS_ASSERTION(aMetrics.Height() - aMetrics.TopAscent() >= 0, "Negative descent???");

  mAscent = aMetrics.TopAscent();

  // Handle text that runs outside its normal bounds.
  nsRect boundingBox = RoundOut(textMetrics.mBoundingBox) + nsPoint(0, mAscent);
  aMetrics.SetOverflowAreasToDesiredBounds();
  aMetrics.VisualOverflow().UnionRect(aMetrics.VisualOverflow(), boundingBox);

  // When we have text decorations, we don't need to compute their overflow now
  // because we're guaranteed to do it later
  // (see nsLineLayout::RelativePositionFrames)
  UnionAdditionalOverflow(presContext, *aLineLayout.LineContainerRS(),
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
    aLineLayout.SetTrimmableWidth(NSToCoordFloor(trimmableWidth));
    AddStateBits(TEXT_HAS_NONCOLLAPSED_CHARACTERS);
  }
  if (charsFit > 0 && charsFit == length &&
      textStyle->mHyphens != NS_STYLE_HYPHENS_NONE &&
      HasSoftHyphenBefore(frag, mTextRun, offset, end)) {
    // Record a potential break after final soft hyphen
    aLineLayout.NotifyOptionalBreakPosition(mContent, offset + length,
        textMetrics.mAdvanceWidth + provider.GetHyphenWidth() <= availWidth,
                                           gfxBreakPriority::eNormalBreak);
  }
  bool breakAfter = forceBreakAfter;
  // length == 0 means either the text is empty or it's all collapsed away
  bool emptyTextAtStartOfLine = atStartOfLine && length == 0;
  if (!breakAfter && charsFit == length && !emptyTextAtStartOfLine &&
      transformedOffset + transformedLength == mTextRun->GetLength() &&
      (mTextRun->GetFlags() & nsTextFrameUtils::TEXT_HAS_TRAILING_BREAK)) {
    // We placed all the text in the textrun and we have a break opportunity at
    // the end of the textrun. We need to record it because the following
    // content may not care about nsLineBreaker.

    // Note that because we didn't break, we can be sure that (thanks to the
    // code up above) textMetrics.mAdvanceWidth includes the width of any
    // trailing whitespace. So we need to subtract trimmableWidth here
    // because if we did break at this point, that much width would be trimmed.
    if (textMetrics.mAdvanceWidth - trimmableWidth > availWidth) {
      breakAfter = true;
    } else {
      aLineLayout.NotifyOptionalBreakPosition(mContent, offset + length,
                                              true, gfxBreakPriority::eNormalBreak);
    }
  }

  // Compute reflow status
  aStatus = contentLength == maxContentLength
    ? NS_FRAME_COMPLETE : NS_FRAME_NOT_COMPLETE;

  if (charsFit == 0 && length > 0 && !usedHyphenation) {
    // Couldn't place any text
    aStatus = NS_INLINE_LINE_BREAK_BEFORE();
  } else if (contentLength > 0 && mContentOffset + contentLength - 1 == newLineOffset) {
    // Ends in \n
    aStatus = NS_INLINE_LINE_BREAK_AFTER(aStatus);
    aLineLayout.SetLineEndsInBR(true);
  } else if (breakAfter) {
    aStatus = NS_INLINE_LINE_BREAK_AFTER(aStatus);
  }
  if (completedFirstLetter) {
    aLineLayout.SetFirstLetterStyleOK(false);
    aStatus |= NS_INLINE_BREAK_FIRST_LETTER_COMPLETE;
  }

  // Updated the cached NewlineProperty, or delete it.
  if (contentLength < maxContentLength &&
      textStyle->NewlineIsSignificant() &&
      (contentNewLineOffset < 0 ||
       mContentOffset + contentLength <= contentNewLineOffset)) {
    if (!cachedNewlineOffset) {
      cachedNewlineOffset = new NewlineProperty;
      if (NS_FAILED(mContent->SetProperty(nsGkAtoms::newline, cachedNewlineOffset,
                                          NewlineProperty::Destroy))) {
        delete cachedNewlineOffset;
        cachedNewlineOffset = nullptr;
      }
    }
    if (cachedNewlineOffset) {
      cachedNewlineOffset->mStartOffset = offset;
      cachedNewlineOffset->mNewlineOffset = contentNewLineOffset;
    }
  } else if (cachedNewlineOffset) {
    mContent->DeleteProperty(nsGkAtoms::newline);
  }

  // Compute space and letter counts for justification, if required
  if (!textStyle->WhiteSpaceIsSignificant() &&
      (lineContainer->StyleText()->mTextAlign == NS_STYLE_TEXT_ALIGN_JUSTIFY ||
       lineContainer->StyleText()->mTextAlignLast == NS_STYLE_TEXT_ALIGN_JUSTIFY) &&
      !lineContainer->IsSVGText()) {
    AddStateBits(TEXT_JUSTIFICATION_ENABLED);    // This will include a space for trailing whitespace, if any is present.
    // This is corrected for in nsLineLayout::TrimWhiteSpaceIn.
    int32_t numJustifiableCharacters =
      provider.ComputeJustifiableCharacters(offset, charsFit);

    NS_ASSERTION(numJustifiableCharacters <= charsFit,
                 "Bad justifiable character count");
    aLineLayout.SetTextJustificationWeights(numJustifiableCharacters,
        charsFit - numJustifiableCharacters);
  }

  SetLength(contentLength, &aLineLayout, ALLOW_FRAME_CREATION_AND_DESTRUCTION);

  InvalidateFrame();

#ifdef NOISY_REFLOW
  ListTag(stdout);
  printf(": desiredSize=%d,%d(b=%d) status=%x\n",
         aMetrics.Width(), aMetrics.Height(), aMetrics.TopAscent(),
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
nsTextFrame::TrimTrailingWhiteSpace(nsRenderingContext* aRC)
{
  TrimOutput result;
  result.mChanged = false;
  result.mLastCharIsJustifiable = false;
  result.mDeltaWidth = 0;

  AddStateBits(TEXT_END_OF_LINE);

  int32_t contentLength = GetContentLength();
  if (!contentLength)
    return result;

  gfxContext* ctx = aRC->ThebesContext();
  gfxSkipCharsIterator start =
    EnsureTextRun(nsTextFrame::eInflated, ctx);
  NS_ENSURE_TRUE(mTextRun, result);

  uint32_t trimmedStart = start.GetSkippedOffset();

  const nsTextFragment* frag = mContent->GetText();
  TrimmedOffsets trimmed = GetTrimmedOffsets(frag, true);
  gfxSkipCharsIterator trimmedEndIter = start;
  const nsStyleText* textStyle = StyleText();
  gfxFloat delta = 0;
  uint32_t trimmedEnd = trimmedEndIter.ConvertOriginalToSkipped(trimmed.GetEnd());
  
  if (GetStateBits() & TEXT_TRIMMED_TRAILING_WHITESPACE) {
    // We pre-trimmed this frame, so the last character is justifiable
    result.mLastCharIsJustifiable = true;
  } else if (trimmed.GetEnd() < GetContentEnd()) {
    gfxSkipCharsIterator end = trimmedEndIter;
    uint32_t endOffset = end.ConvertOriginalToSkipped(GetContentOffset() + contentLength);
    if (trimmedEnd < endOffset) {
      // We can't be dealing with tabs here ... they wouldn't be trimmed. So it's
      // OK to pass null for the line container.
      PropertyProvider provider(mTextRun, textStyle, frag, this, start, contentLength,
                                nullptr, 0, nsTextFrame::eInflated);
      delta = mTextRun->GetAdvanceWidth(trimmedEnd, endOffset - trimmedEnd, &provider);
      // non-compressed whitespace being skipped at end of line -> justifiable
      // XXX should we actually *count* justifiable characters that should be
      // removed from the overall count? I think so...
      result.mLastCharIsJustifiable = true;
      result.mChanged = true;
    }
  }

  if (!result.mLastCharIsJustifiable &&
      (GetStateBits() & TEXT_JUSTIFICATION_ENABLED)) {
    // Check if any character in the last cluster is justifiable
    PropertyProvider provider(mTextRun, textStyle, frag, this, start, contentLength,
                              nullptr, 0, nsTextFrame::eInflated);
    bool isCJK = IsChineseOrJapanese(this);
    gfxSkipCharsIterator justificationStart(start), justificationEnd(trimmedEndIter);
    provider.FindJustificationRange(&justificationStart, &justificationEnd);

    for (int32_t i = justificationEnd.GetOriginalOffset();
         i < trimmed.GetEnd(); ++i) {
      if (IsJustifiableCharacter(frag, i, isCJK)) {
        result.mLastCharIsJustifiable = true;
      }
    }
  }

  gfxFloat advanceDelta;
  mTextRun->SetLineBreaks(trimmedStart, trimmedEnd - trimmedStart,
                          (GetStateBits() & TEXT_START_OF_LINE) != 0, true,
                          &advanceDelta, ctx);
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
  NS_WARN_IF_FALSE(result.mDeltaWidth >= 0,
                   "Negative deltawidth, something odd is happening");

#ifdef NOISY_TRIM
  ListTag(stdout);
  printf(": trim => %d\n", result.mDeltaWidth);
#endif
  return result;
}

nsOverflowAreas
nsTextFrame::RecomputeOverflow(const nsHTMLReflowState& aBlockReflowState)
{
  nsRect bounds(nsPoint(0, 0), GetSize());
  nsOverflowAreas result(bounds, bounds);

  gfxSkipCharsIterator iter = EnsureTextRun(nsTextFrame::eInflated);
  if (!mTextRun)
    return result;

  PropertyProvider provider(this, iter, nsTextFrame::eInflated);
  provider.InitializeForDisplay(true);

  gfxTextRun::Metrics textMetrics =
    mTextRun->MeasureText(provider.GetStart().GetSkippedOffset(),
                          ComputeTransformedLength(provider),
                          gfxFont::LOOSE_INK_EXTENTS, nullptr,
                          &provider);
  nsRect &vis = result.VisualOverflow();
  vis.UnionRect(vis, RoundOut(textMetrics.mBoundingBox) + nsPoint(0, mAscent));
  UnionAdditionalOverflow(PresContext(), aBlockReflowState, provider,
                          &vis, true);
  return result;
}
static char16_t TransformChar(const nsStyleText* aStyle, gfxTextRun* aTextRun,
                               uint32_t aSkippedOffset, char16_t aChar)
{
  if (aChar == '\n') {
    return aStyle->NewlineIsSignificant() || aStyle->NewlineIsDiscarded() ?
             aChar : ' ';
  }
  switch (aStyle->mTextTransform) {
  case NS_STYLE_TEXT_TRANSFORM_LOWERCASE:
    aChar = ToLowerCase(aChar);
    break;
  case NS_STYLE_TEXT_TRANSFORM_UPPERCASE:
    aChar = ToUpperCase(aChar);
    break;
  case NS_STYLE_TEXT_TRANSFORM_CAPITALIZE:
    if (aTextRun->CanBreakLineBefore(aSkippedOffset)) {
      aChar = ToTitleCase(aChar);
    }
    break;
  }

  return aChar;
}

nsresult nsTextFrame::GetRenderedText(nsAString* aAppendToString,
                                      gfxSkipChars* aSkipChars,
                                      gfxSkipCharsIterator* aSkipIter,
                                      uint32_t aSkippedStartOffset,
                                      uint32_t aSkippedMaxLength)
{
  // The handling of aSkippedStartOffset and aSkippedMaxLength could be more efficient...
  gfxSkipChars skipChars;
  nsTextFrame* textFrame;
  const nsTextFragment* textFrag = mContent->GetText();
  uint32_t keptCharsLength = 0;
  uint32_t validCharsLength = 0;

  // Build skipChars and copy text, for each text frame in this continuation block
  for (textFrame = this; textFrame;
       textFrame = static_cast<nsTextFrame*>(textFrame->GetNextContinuation())) {
    // For each text frame continuation in this block ...

    if (textFrame->GetStateBits() & NS_FRAME_IS_DIRTY) {
      // We don't trust dirty frames, expecially when computing rendered text.
      break;
    }

    // Ensure the text run and grab the gfxSkipCharsIterator for it
    gfxSkipCharsIterator iter =
      textFrame->EnsureTextRun(nsTextFrame::eInflated);
    if (!textFrame->mTextRun)
      return NS_ERROR_FAILURE;

    // Skip to the start of the text run, past ignored chars at start of line
    // XXX In the future we may decide to trim extra spaces before a hard line
    // break, in which case we need to accurately detect those sitations and 
    // call GetTrimmedOffsets() with true to trim whitespace at the line's end
    TrimmedOffsets trimmedContentOffsets = textFrame->GetTrimmedOffsets(textFrag, false);
    int32_t startOfLineSkipChars = trimmedContentOffsets.mStart - textFrame->mContentOffset;
    if (startOfLineSkipChars > 0) {
      skipChars.SkipChars(startOfLineSkipChars);
      iter.SetOriginalOffset(trimmedContentOffsets.mStart);
    }

    // Keep and copy the appropriate chars withing the caller's requested range
    const nsStyleText* textStyle = textFrame->StyleText();
    while (iter.GetOriginalOffset() < trimmedContentOffsets.GetEnd() &&
           keptCharsLength < aSkippedMaxLength) {
      // For each original char from content text
      if (iter.IsOriginalCharSkipped() || ++validCharsLength <= aSkippedStartOffset) {
        skipChars.SkipChar();
      } else {
        ++keptCharsLength;
        skipChars.KeepChar();
        if (aAppendToString) {
          aAppendToString->Append(
              TransformChar(textStyle, textFrame->mTextRun, iter.GetSkippedOffset(),
                            textFrag->CharAt(iter.GetOriginalOffset())));
        }
      }
      iter.AdvanceOriginal(1);
    }
    if (keptCharsLength >= aSkippedMaxLength) {
      break; // Already past the end, don't build string or gfxSkipCharsIter anymore
    }
  }
  
  if (aSkipChars) {
    aSkipChars->TakeFrom(&skipChars); // Copy skipChars into aSkipChars
    if (aSkipIter) {
      // Caller must provide both pointers in order to retrieve a gfxSkipCharsIterator,
      // because the gfxSkipCharsIterator holds a weak pointer to the gfxSkipChars.
      *aSkipIter = gfxSkipCharsIterator(*aSkipChars, GetContentLength());
    }
  }

  return NS_OK;
}

nsIAtom*
nsTextFrame::GetType() const
{
  return nsGkAtoms::textFrame;
}

/* virtual */ bool
nsTextFrame::IsEmpty()
{
  NS_ASSERTION(!(mState & TEXT_IS_ONLY_WHITESPACE) ||
               !(mState & TEXT_ISNOT_ONLY_WHITESPACE),
               "Invalid state");
  
  // XXXldb Should this check compatibility mode as well???
  const nsStyleText* textStyle = StyleText();
  if (textStyle->WhiteSpaceIsSignificant() &&
      textStyle->mWhiteSpace != NS_STYLE_WHITESPACE_PRE_DISCARD_NEWLINES) {
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
    textStyle->mWhiteSpace == NS_STYLE_WHITESPACE_PRE_DISCARD_NEWLINES ?
      IsAllNewlines(mContent->GetText()) :
      IsAllWhitespace(mContent->GetText(),
                      textStyle->mWhiteSpace != NS_STYLE_WHITESPACE_PRE_LINE);
  mState |= (isEmpty ? TEXT_IS_ONLY_WHITESPACE : TEXT_ISNOT_ONLY_WHITESPACE);
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

NS_IMETHODIMP
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
NS_IMETHODIMP_(nsFrameState)
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
  mContent->DeleteProperty(nsGkAtoms::flowlength);

  /*
   * After Bidi resolution we may need to reassign text runs.
   * This is called during bidi resolution from the block container, so we
   * shouldn't be holding a local reference to a textrun anywhere.
   */
  ClearTextRuns();

  nsTextFrame* prev = static_cast<nsTextFrame*>(GetPrevContinuation());
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

  /**
   * After inserting text the caret Bidi level must be set to the level of the
   * inserted text.This is difficult, because we cannot know what the level is
   * until after the Bidi algorithm is applied to the whole paragraph.
   *
   * So we set the caret Bidi level to UNDEFINED here, and the caret code will
   * set it correctly later
   */
  nsRefPtr<nsFrameSelection> frameSelection = GetFrameSelection();
  if (frameSelection) {
    frameSelection->UndefineCaretBidiLevel();
  }
}

/**
 * @return true if this text frame ends with a newline character.  It should return
 * false if it is not a text frame.
 */
bool
nsTextFrame::HasSignificantTerminalNewline() const
{
  return ::HasTerminalNewline(this) && StyleText()->NewlineIsSignificant();
}

bool
nsTextFrame::IsAtEndOfLine() const
{
  return (GetStateBits() & TEXT_END_OF_LINE) != 0;
}

nscoord
nsTextFrame::GetBaseline() const
{
  return mAscent;
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

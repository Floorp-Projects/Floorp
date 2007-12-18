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
#include "nsVoidArray.h"
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
#include "nsTextFrameUtils.h"
#include "nsTextRunTransformations.h"
#include "nsFrameManager.h"
#include "nsTextFrameTextRunCache.h"
#include "nsExpirationTracker.h"
#include "nsTextFrame.h"
#include "nsICaseConversion.h"
#include "nsIUGenCategory.h"
#include "nsUnicharUtilCIID.h"

#include "nsTextFragment.h"
#include "nsGkAtoms.h"
#include "nsFrameSelection.h"
#include "nsISelection.h"
#include "nsIDOMRange.h"
#include "nsILookAndFeel.h"
#include "nsCSSRendering.h"
#include "nsContentUtils.h"
#include "nsLineBreaker.h"
#include "nsIWordBreaker.h"

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
#include "gfxTextRunWordCache.h"

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
  PRInt32      mDOMOffsetToBeforeTransformOffset;
  // The text mapped starts at mStartFrame->GetContentOffset() and is this long
  PRUint32     mContentLength;
};

/**
 * This is our user data for the textrun, when textRun->GetFlags() does not
 * have TEXT_SIMPLE_FLOW set. When TEXT_SIMPLE_FLOW is set, there is just one
 * flow, the textrun's user data pointer is a pointer to mStartFrame
 * for that flow, mDOMOffsetToBeforeTransformOffset is zero, and mContentLength
 * is the length of the text node.
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
                         float*   aRelativeSize,
                         PRUint8* aStyle);

  nsPresContext* PresContext() { return mPresContext; }

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
  struct nsIMEStyle {
    PRBool mInit;
    nscolor mTextColor;
    nscolor mBGColor;
    nscolor mUnderlineColor;
    PRUint8 mUnderlineStyle;
  };
  nsIMEStyle mIMEStyle[4];
  // indices
  float mIMEUnderlineRelativeSize;

  // Color initializations
  void InitCommonColors();
  PRBool InitSelectionColors();

  nsIMEStyle* GetIMEStyle(PRInt32 aIndex);
  void InitIMEStyle(PRInt32 aIndex);

  PRBool EnsureSufficientContrast(nscolor *aForeColor, nscolor *aBackColor);

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

// Remove the textrun from the frame continuation chain starting at aFrame,
// which should be marked as a textrun owner.
static void
ClearAllTextRunReferences(nsTextFrame* aFrame, gfxTextRun* aTextRun)
{
  while (aFrame) {
    if (aFrame->GetTextRun() != aTextRun)
      break;
    aFrame->SetTextRun(nsnull);
    aFrame = static_cast<nsTextFrame*>(aFrame->GetNextContinuation());
  }
}

// Figure out which frames 
static void
UnhookTextRunFromFrames(gfxTextRun* aTextRun)
{
  if (!aTextRun->GetUserData())
    return;

  // Kill all references to the textrun. It could be referenced by any of its
  // owners, and all their in-flows.
  if (aTextRun->GetFlags() & nsTextFrameUtils::TEXT_IS_SIMPLE_FLOW) {
    nsIFrame* firstInFlow = static_cast<nsIFrame*>(aTextRun->GetUserData());
    ClearAllTextRunReferences(static_cast<nsTextFrame*>(firstInFlow), aTextRun);
  } else {
    TextRunUserData* userData =
      static_cast<TextRunUserData*>(aTextRun->GetUserData());
    PRInt32 i;
    for (i = 0; i < userData->mMappedFlowCount; ++i) {
      ClearAllTextRunReferences(userData->mMappedFlows[i].mStartFrame, aTextRun);
    }
    DestroyUserData(userData);
  }
  aTextRun->SetUserData(nsnull);  
}

class FrameTextRunCache;

static FrameTextRunCache *gTextRuns = nsnull;

/*
 * Cache textruns and expire them after 3*10 seconds of no use.
 */
class FrameTextRunCache : public nsExpirationTracker<gfxTextRun,3> {
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
    if (aTextRun->GetFlags() & gfxTextRunWordCache::TEXT_IN_CACHE) {
      gfxTextRunWordCache::RemoveTextRun(aTextRun);
    }
  }

  // This gets called when the timeout has expired on a gfxTextRun
  virtual void NotifyExpired(gfxTextRun* aTextRun) {
    UnhookTextRunFromFrames(aTextRun);
    RemoveFromCache(aTextRun);
    delete aTextRun;
  }
};

static gfxTextRun *
MakeTextRun(const PRUnichar *aText, PRUint32 aLength,
            gfxFontGroup *aFontGroup, const gfxFontGroup::Parameters* aParams,
            PRUint32 aFlags)
{
    nsAutoPtr<gfxTextRun> textRun;
    if (aLength == 0) {
        textRun = aFontGroup->MakeEmptyTextRun(aParams, aFlags);
    } else if (aLength == 1 && aText[0] == ' ') {
        textRun = aFontGroup->MakeSpaceTextRun(aParams, aFlags);
    } else {
        textRun = gfxTextRunWordCache::MakeTextRun(aText, aLength, aFontGroup,
            aParams, aFlags);
    }
    if (!textRun)
        return nsnull;
    nsresult rv = gTextRuns->AddObject(textRun);
    if (NS_FAILED(rv)) {
        gTextRuns->RemoveFromCache(textRun);
        return nsnull;
    }
    return textRun.forget();
}

static gfxTextRun *
MakeTextRun(const PRUint8 *aText, PRUint32 aLength,
            gfxFontGroup *aFontGroup, const gfxFontGroup::Parameters* aParams,
            PRUint32 aFlags)
{
    nsAutoPtr<gfxTextRun> textRun;
    if (aLength == 0) {
        textRun = aFontGroup->MakeEmptyTextRun(aParams, aFlags);
    } else if (aLength == 1 && aText[0] == ' ') {
        textRun = aFontGroup->MakeSpaceTextRun(aParams, aFlags);
    } else {
        textRun = gfxTextRunWordCache::MakeTextRun(aText, aLength, aFontGroup,
            aParams, aFlags);
    }
    if (!textRun)
        return nsnull;
    nsresult rv = gTextRuns->AddObject(textRun);
    if (NS_FAILED(rv)) {
        gTextRuns->RemoveFromCache(textRun);
        return nsnull;
    }
    return textRun.forget();
}

nsresult
nsTextFrameTextRunCache::Init() {
    gTextRuns = new FrameTextRunCache();
    return gTextRuns ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

void
nsTextFrameTextRunCache::Shutdown() {
    delete gTextRuns;
    gTextRuns = nsnull;
}

PRInt32 nsTextFrame::GetContentEnd() const {
  nsTextFrame* next = static_cast<nsTextFrame*>(GetNextContinuation());
  return next ? next->GetContentOffset() : mContent->GetText()->GetLength();
}

PRInt32 nsTextFrame::GetInFlowContentLength() {
#ifdef IBMBIDI
  nsTextFrame* nextBidi = nsnull;
  PRInt32      start = -1, end;

  if (mState & NS_FRAME_IS_BIDI) {
    nextBidi = static_cast<nsTextFrame*>(GetLastInFlow()->GetNextContinuation());
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

static PRBool IsSpaceCombiningSequenceTail(const nsTextFragment* aFrag, PRUint32 aPos)
{
  NS_ASSERTION(aPos <= aFrag->GetLength(), "Bad offset");
  if (!aFrag->Is2b())
    return PR_FALSE;
  return nsTextFrameUtils::IsSpaceCombiningSequenceTail(
    aFrag->Get2b() + aPos, aFrag->GetLength() - aPos);
}

// Check whether aPos is a space for CSS 'word-spacing' purposes
static PRBool IsCSSWordSpacingSpace(const nsTextFragment* aFrag, PRUint32 aPos)
{
  NS_ASSERTION(aPos < aFrag->GetLength(), "No text for IsSpace!");
  PRUnichar ch = aFrag->CharAt(aPos);
  if (ch == ' ' || ch == CH_CJKSP)
    return !IsSpaceCombiningSequenceTail(aFrag, aPos + 1);
  return ch == '\t' || ch == '\n' || ch == '\f';
}

// Check whether the string aChars/aLength starts with a space that's
// trimmable according to CSS 'white-space'.
static PRBool IsTrimmableSpace(const PRUnichar* aChars, PRUint32 aLength)
{
  NS_ASSERTION(aLength > 0, "No text for IsSpace!");
  PRUnichar ch = *aChars;
  if (ch == ' ')
    return !nsTextFrameUtils::IsSpaceCombiningSequenceTail(aChars + 1, aLength - 1);
  return ch == '\t' || ch == '\n' || ch == '\f';
}

// Check whether the character aCh is trimmable according to CSS 'white-space'
static PRBool IsTrimmableSpace(char aCh)
{
  return aCh == ' ' || aCh == '\t' || aCh == '\n' || aCh == '\f';
}

static PRBool IsTrimmableSpace(const nsTextFragment* aFrag, PRUint32 aPos)
{
  NS_ASSERTION(aPos < aFrag->GetLength(), "No text for IsSpace!");
  PRUnichar ch = aFrag->CharAt(aPos);
  if (ch == ' ')
    return !IsSpaceCombiningSequenceTail(aFrag, aPos + 1);
  return ch == '\t' || ch == '\n' || ch == '\f';
}

static PRBool IsTrimmableSpace(const nsTextFragment* aFrag,
                               const nsStyleText* aText, PRUint32 aPos)
{
  if (aText->WhiteSpaceIsSignificant())
    return PR_FALSE;
  return IsTrimmableSpace(aFrag, aPos);
}

static PRBool IsSelectionSpace(const nsTextFragment* aFrag, PRUint32 aPos)
{
  NS_ASSERTION(aPos < aFrag->GetLength(), "No text for IsSpace!");
  PRUnichar ch = aFrag->CharAt(aPos);
  if (ch == ' ' || ch == CH_NBSP)
    return !IsSpaceCombiningSequenceTail(aFrag, aPos + 1);
  return ch == '\t' || ch == '\n' || ch == '\f';
}

// Count the amount of trimmable whitespace in a text fragment. The first
// character is at offset aStartOffset; the maximum number of characters
// to check is aLength. aDirection is -1 or 1 depending on whether we should
// progress backwards or forwards.
static PRUint32
GetTrimmableWhitespaceCount(const nsTextFragment* aFrag, PRInt32 aStartOffset,
                            PRInt32 aLength, PRInt32 aDirection)
{
  PRInt32 count = 0;
  if (aFrag->Is2b()) {
    const PRUnichar* str = aFrag->Get2b() + aStartOffset;
    PRInt32 fragLen = aFrag->GetLength() - aStartOffset;
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
      nsIFrame* aLineContainer) :
    mCurrentFramesAllSameTextRun(nsnull),
    mContext(aContext),
    mLineContainer(aLineContainer),
    mBidiEnabled(aPresContext->BidiEnabled()),    
    mTrimNextRunLeadingWhitespace(PR_FALSE), mSkipIncompleteTextRuns(PR_FALSE) {
    ResetRunInfo();
  }

  void SetAtStartOfLine() {
    mStartOfLine = PR_TRUE;
    mCanStopOnThisLine = PR_FALSE;
  }
  void SetSkipIncompleteTextRuns(PRBool aSkip) {
    mSkipIncompleteTextRuns = aSkip;
  }
  void SetCommonAncestorWithLastFrame(nsIFrame* aFrame) {
    mCommonAncestorWithLastFrame = aFrame;
  }
  PRBool CanStopOnThisLine() {
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
  PRBool IsTextRunValidForMappedFlows(gfxTextRun* aTextRun);
  void FlushFrames(PRBool aFlushLineBreaks, PRBool aSuppressTrailingBreak);
  void ResetRunInfo() {
    mLastFrame = nsnull;
    mMappedFlows.Clear();
    mLineBreakBeforeFrames.Clear();
    mMaxTextLength = 0;
    mDoubleByteText = PR_FALSE;
  }
  void ResetLineBreaker() {
    PRBool trailingBreak;
    mLineBreaker.Reset(&trailingBreak);
  }
  void AccumulateRunInfo(nsTextFrame* aFrame);
  /**
   * @return null to indicate either textrun construction failed or
   * we constructed just a partial textrun to set up linebreaker and other
   * state for following textruns.
   */
  gfxTextRun* BuildTextRunForFrames(void* aTextBuffer);
  void AssignTextRun(gfxTextRun* aTextRun);
  nsTextFrame* GetNextBreakBeforeFrame(PRUint32* aIndex);
  void SetupBreakSinksForTextRun(gfxTextRun* aTextRun, PRBool aIsExistingTextRun,
                                 PRBool aSuppressSink);
  struct FindBoundaryState {
    nsIFrame*    mStopAtFrame;
    nsTextFrame* mFirstTextFrame;
    nsTextFrame* mLastTextFrame;
    PRPackedBool mSeenTextRunBoundaryOnLaterLine;
    PRPackedBool mSeenTextRunBoundaryOnThisLine;
    PRPackedBool mSeenSpaceForLineBreakingOnThisLine;
  };
  enum FindBoundaryResult {
    FB_CONTINUE,
    FB_STOPPED_AT_STOP_FRAME,
    FB_FOUND_VALID_TEXTRUN_BOUNDARY
  };
  FindBoundaryResult FindBoundaries(nsIFrame* aFrame, FindBoundaryState* aState);

  PRBool ContinueTextRunAcrossFrames(nsTextFrame* aFrame1, nsTextFrame* aFrame2);

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
    
    PRInt32 GetContentEnd() {
      return mEndFrame ? mEndFrame->GetContentOffset()
          : mStartFrame->GetContent()->GetText()->GetLength();
    }
  };

  class BreakSink : public nsILineBreakSink {
  public:
    BreakSink(gfxTextRun* aTextRun, gfxContext* aContext, PRUint32 aOffsetIntoTextRun,
              PRBool aExistingTextRun) :
                mTextRun(aTextRun), mContext(aContext),
                mOffsetIntoTextRun(aOffsetIntoTextRun),
                mChangedBreaks(PR_FALSE), mExistingTextRun(aExistingTextRun) {}

    virtual void SetBreaks(PRUint32 aOffset, PRUint32 aLength,
                           PRPackedBool* aBreakBefore) {
      if (mTextRun->SetPotentialLineBreaks(aOffset + mOffsetIntoTextRun, aLength,
                                           aBreakBefore, mContext)) {
        mChangedBreaks = PR_TRUE;
        // Be conservative and assume that some breaks have been set
        mTextRun->ClearFlagBits(nsTextFrameUtils::TEXT_NO_BREAKS);
      }
    }
    
    virtual void SetCapitalization(PRUint32 aOffset, PRUint32 aLength,
                                   PRPackedBool* aCapitalize) {
      NS_ASSERTION(mTextRun->GetFlags() & nsTextFrameUtils::TEXT_IS_TRANSFORMED,
                   "Text run should be transformed!");
      nsTransformedTextRun* transformedTextRun =
        static_cast<nsTransformedTextRun*>(mTextRun);
      transformedTextRun->SetCapitalization(aOffset + mOffsetIntoTextRun, aLength,
                                            aCapitalize, mContext);
    }

    gfxTextRun*  mTextRun;
    gfxContext*  mContext;
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
  gfxContext*                   mContext;
  nsIFrame*                     mLineContainer;
  nsTextFrame*                  mLastFrame;
  // The common ancestor of the current frame and the previous leaf frame
  // on the line, or null if there was no previous leaf frame.
  nsIFrame*                     mCommonAncestorWithLastFrame;
  // mMaxTextLength is an upper bound on the size of the text in all mapped frames
  PRUint32                      mMaxTextLength;
  PRPackedBool                  mDoubleByteText;
  PRPackedBool                  mBidiEnabled;
  PRPackedBool                  mStartOfLine;
  PRPackedBool                  mTrimNextRunLeadingWhitespace;
  PRPackedBool                  mCurrentRunTrimLeadingWhitespace;
  PRPackedBool                  mSkipIncompleteTextRuns;
  PRPackedBool                  mCanStopOnThisLine;
};

static nsIFrame*
FindLineContainer(nsIFrame* aFrame)
{
  while (aFrame && aFrame->IsFrameOfType(nsIFrame::eLineParticipant)) {
    aFrame = aFrame->GetParent();
  }
  return aFrame;
}

static PRBool
TextContainsLineBreakerWhiteSpace(const void* aText, PRUint32 aLength,
                                  PRBool aIsDoubleByte)
{
  PRUint32 i;
  if (aIsDoubleByte) {
    const PRUnichar* chars = static_cast<const PRUnichar*>(aText);
    for (i = 0; i < aLength; ++i) {
      if (nsLineBreaker::IsSpace(chars[i]))
        return PR_TRUE;
    }
    return PR_FALSE;
  } else {
    const PRUint8* chars = static_cast<const PRUint8*>(aText);
    for (i = 0; i < aLength; ++i) {
      if (nsLineBreaker::IsSpace(chars[i]))
        return PR_TRUE;
    }
    return PR_FALSE;
  }
}

static PRBool
CanTextRunCrossFrameBoundary(nsIFrame* aFrame)
{
  // placeholders are "invisible", so a text run should be able to span
  // across one. The text in the out-of-flow, if any, will not be included
  // in this textrun of course.
  return aFrame->CanContinueTextRun() ||
    aFrame->GetType() == nsGkAtoms::placeholderFrame;
}

BuildTextRunsScanner::FindBoundaryResult
BuildTextRunsScanner::FindBoundaries(nsIFrame* aFrame, FindBoundaryState* aState)
{
  nsTextFrame* textFrame = aFrame->GetType() == nsGkAtoms::textFrame
    ? static_cast<nsTextFrame*>(aFrame) : nsnull;
  if (textFrame) {
    if (aState->mLastTextFrame &&
        textFrame != aState->mLastTextFrame->GetNextInFlow() &&
        !ContinueTextRunAcrossFrames(aState->mLastTextFrame, textFrame)) {
      aState->mSeenTextRunBoundaryOnThisLine = PR_TRUE;
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
      PRUint32 start = textFrame->GetContentOffset();
      const void* text = frag->Is2b()
          ? static_cast<const void*>(frag->Get2b() + start)
          : static_cast<const void*>(frag->Get1b() + start);
      if (TextContainsLineBreakerWhiteSpace(text, textFrame->GetContentLength(),
                                            frag->Is2b())) {
        aState->mSeenSpaceForLineBreakingOnThisLine = PR_TRUE;
        if (aState->mSeenTextRunBoundaryOnLaterLine)
          return FB_FOUND_VALID_TEXTRUN_BOUNDARY;
      }
    }
    return FB_CONTINUE; 
  }

  PRBool continueTextRun = CanTextRunCrossFrameBoundary(aFrame);
  PRBool descendInto = PR_TRUE;
  if (!continueTextRun) {
    // XXX do we need this? are there frames we need to descend into that aren't
    // float-containing-blocks?
    descendInto = !aFrame->IsFloatContainingBlock();
    aState->mSeenTextRunBoundaryOnThisLine = PR_TRUE;
    if (aState->mSeenSpaceForLineBreakingOnThisLine)
      return FB_FOUND_VALID_TEXTRUN_BOUNDARY;
  }
  
  if (descendInto) {
    nsIFrame* child = aFrame->GetFirstChild(nsnull);
    while (child) {
      FindBoundaryResult result = FindBoundaries(child, aState);
      if (result != FB_CONTINUE)
        return result;
      child = child->GetNextSibling();
    }
  }

  if (!continueTextRun) {
    aState->mSeenTextRunBoundaryOnThisLine = PR_TRUE;
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
 * @param aForFrameLine the line containing aForFrame; if null, we'll figure
 * out the line (slowly)
 * @param aBlockFrame the block containing aForFrame; if null, we'll figure
 * out the block (slowly)
 */
static void
BuildTextRuns(gfxContext* aContext, nsTextFrame* aForFrame,
              nsIFrame* aLineContainer, const nsLineList::iterator* aForFrameLine)
{
  if (!aLineContainer) {
    aLineContainer = FindLineContainer(aForFrame);
  } else {
    NS_ASSERTION(!aForFrame || aLineContainer == FindLineContainer(aForFrame), "Wrong line container hint");
  }

  nsPresContext* presContext = aLineContainer->PresContext();
  BuildTextRunsScanner scanner(presContext, aContext, aLineContainer);

  nsBlockFrame* block = nsnull;
  aLineContainer->QueryInterface(kBlockFrameCID, (void**)&block);

  if (!block) {
    NS_ASSERTION(!aLineContainer->GetPrevInFlow() && !aLineContainer->GetNextInFlow(),
                 "Breakable non-block line containers not supported");
    // Just loop through all the children of the linecontainer ... it's really
    // just one line
    scanner.SetAtStartOfLine();
    scanner.SetCommonAncestorWithLastFrame(nsnull);
    nsIFrame* child = aLineContainer->GetFirstChild(nsnull);
    while (child) {
      scanner.ScanFrame(child);
      child = child->GetNextSibling();
    }
    // Set mStartOfLine so FlushFrames knows its textrun ends a line
    scanner.SetAtStartOfLine();
    scanner.FlushFrames(PR_TRUE, PR_FALSE);
    return;
  }

  // Find the line containing aForFrame
  nsBlockFrame::line_iterator startLine;
  if (aForFrameLine) {
    startLine = *aForFrameLine;
  } else {
    NS_ASSERTION(aForFrame, "One of aForFrame or aForFrameLine must be set!");
    nsIFrame* immediateChild =
      nsLayoutUtils::FindChildContainingDescendant(block, aForFrame);
    // This may be a float e.g. for a floated first-letter
    if (immediateChild->GetStateBits() & NS_FRAME_OUT_OF_FLOW) {
      immediateChild =
        nsLayoutUtils::FindChildContainingDescendant(block,
          presContext->FrameManager()->GetPlaceholderFrameFor(immediateChild));
    }
    startLine = block->FindLineFor(immediateChild);
    NS_ASSERTION(startLine != block->end_lines(),
                 "Frame is not in the block!!!");
  }

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
  nsBlockInFlowLineIterator backIterator(block, startLine, PR_FALSE);
  nsTextFrame* stopAtFrame = aForFrame;
  nsTextFrame* nextLineFirstTextFrame = nsnull;
  PRBool seenTextRunBoundaryOnLaterLine = PR_FALSE;
  PRBool mayBeginInTextRun = PR_TRUE;
  PRBool inOverflow = PR_FALSE;
  nsBlockFrame::line_iterator line;
  while (PR_TRUE) {
    line = backIterator.GetLine();
    block = backIterator.GetContainer();
    inOverflow = backIterator.GetInOverflow();
    if (!backIterator.Prev() || backIterator.GetLine()->IsBlock()) {
      mayBeginInTextRun = PR_FALSE;
      break;
    }

    BuildTextRunsScanner::FindBoundaryState state = { stopAtFrame, nsnull, nsnull,
      seenTextRunBoundaryOnLaterLine, PR_FALSE, PR_FALSE };
    nsIFrame* child = line->mFirstChild;
    PRBool foundBoundary = PR_FALSE;
    PRInt32 i;
    for (i = line->GetChildCount() - 1; i >= 0; --i) {
      BuildTextRunsScanner::FindBoundaryResult result =
          scanner.FindBoundaries(child, &state);
      if (result == BuildTextRunsScanner::FB_FOUND_VALID_TEXTRUN_BOUNDARY) {
        foundBoundary = PR_TRUE;
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
      seenTextRunBoundaryOnLaterLine = PR_TRUE;
    } else if (state.mSeenTextRunBoundaryOnThisLine) {
      seenTextRunBoundaryOnLaterLine = PR_TRUE;
    }
    stopAtFrame = nsnull;
    if (state.mFirstTextFrame) {
      nextLineFirstTextFrame = state.mFirstTextFrame;
    }
  }
  scanner.SetSkipIncompleteTextRuns(mayBeginInTextRun);

  // Now iterate over all text frames starting from the current line. First-in-flow
  // text frames will be accumulated into textRunFrames as we go. When a
  // text run boundary is required we flush textRunFrames ((re)building their
  // gfxTextRuns as necessary).
  nsBlockInFlowLineIterator forwardIterator(block, line, inOverflow);
  PRBool seenStartLine = PR_FALSE;
  PRUint32 linesAfterStartLine = 0;
  do {
    line = forwardIterator.GetLine();
    if (line->IsBlock())
      break;
    line->SetInvalidateTextRuns(PR_FALSE);
    scanner.SetAtStartOfLine();
    scanner.SetCommonAncestorWithLastFrame(nsnull);
    nsIFrame* child = line->mFirstChild;
    PRInt32 i;
    for (i = line->GetChildCount() - 1; i >= 0; --i) {
      scanner.ScanFrame(child);
      child = child->GetNextSibling();
    }
    if (forwardIterator.GetContainer() == aLineContainer && line == startLine) {
      seenStartLine = PR_TRUE;
    }
    if (seenStartLine) {
      ++linesAfterStartLine;
      if (linesAfterStartLine >= NUM_LINES_TO_BUILD_TEXT_RUNS && scanner.CanStopOnThisLine()) {
        // Don't flush; we may be in the middle of a textrun that we can't
        // end here. That's OK, we just won't build it.
        // Note that we must already have finished the textrun for aForFrame,
        // because we've seen the end of a textrun in a line after the line
        // containing aForFrame.
        scanner.ResetLineBreaker();
        return;
      }
    }
  } while (forwardIterator.Next());

  // Set mStartOfLine so FlushFrames knows its textrun ends a line
  scanner.SetAtStartOfLine();
  scanner.FlushFrames(PR_TRUE, PR_FALSE);
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

PRBool BuildTextRunsScanner::IsTextRunValidForMappedFlows(gfxTextRun* aTextRun)
{
  if (aTextRun->GetFlags() & nsTextFrameUtils::TEXT_IS_SIMPLE_FLOW)
    return mMappedFlows.Length() == 1 &&
      mMappedFlows[0].mStartFrame == static_cast<nsTextFrame*>(aTextRun->GetUserData()) &&
      mMappedFlows[0].mEndFrame == nsnull;

  TextRunUserData* userData = static_cast<TextRunUserData*>(aTextRun->GetUserData());
  if (userData->mMappedFlowCount != PRInt32(mMappedFlows.Length()))
    return PR_FALSE;
  PRUint32 i;
  for (i = 0; i < mMappedFlows.Length(); ++i) {
    if (userData->mMappedFlows[i].mStartFrame != mMappedFlows[i].mStartFrame ||
        PRInt32(userData->mMappedFlows[i].mContentLength) !=
            mMappedFlows[i].GetContentEnd() - mMappedFlows[i].mStartFrame->GetContentOffset())
      return PR_FALSE;
  }
  return PR_TRUE;
}

/**
 * This gets called when we need to make a text run for the current list of
 * frames.
 */
void BuildTextRunsScanner::FlushFrames(PRBool aFlushLineBreaks, PRBool aSuppressTrailingBreak)
{
  if (mMappedFlows.Length() == 0)
    return;

  gfxTextRun* textRun;
  if (!mSkipIncompleteTextRuns && mCurrentFramesAllSameTextRun &&
      ((mCurrentFramesAllSameTextRun->GetFlags() & nsTextFrameUtils::TEXT_INCOMING_WHITESPACE) != 0) ==
      mCurrentRunTrimLeadingWhitespace &&
      IsTextRunValidForMappedFlows(mCurrentFramesAllSameTextRun)) {
    // Optimization: We do not need to (re)build the textrun.
    textRun = mCurrentFramesAllSameTextRun;

    // Feed this run's text into the linebreaker to provide context. This also
    // updates mTrimNextRunLeadingWhitespace appropriately.
    SetupBreakSinksForTextRun(textRun, PR_TRUE, PR_FALSE);
    mTrimNextRunLeadingWhitespace =
      (textRun->GetFlags() & nsTextFrameUtils::TEXT_TRAILING_WHITESPACE) != 0;
  } else {
    nsAutoTArray<PRUint8,BIG_TEXT_NODE_SIZE> buffer;
    if (!buffer.AppendElements(mMaxTextLength*(mDoubleByteText ? 2 : 1)))
      return;
    textRun = BuildTextRunForFrames(buffer.Elements());
  }

  if (aFlushLineBreaks) {
    PRBool trailingLineBreak;
    nsresult rv = mLineBreaker.Reset(&trailingLineBreak);
    // textRun may be null for various reasons, including because we constructed
    // a partial textrun just to get the linebreaker and other state set up
    // to build the next textrun.
    if (NS_SUCCEEDED(rv) && trailingLineBreak && textRun && !aSuppressTrailingBreak) {
      textRun->SetFlagBits(nsTextFrameUtils::TEXT_HAS_TRAILING_BREAK);
    }
    PRUint32 i;
    for (i = 0; i < mBreakSinks.Length(); ++i) {
      if (!mBreakSinks[i]->mExistingTextRun || mBreakSinks[i]->mChangedBreaks) {
        // TODO cause frames associated with the textrun to be reflowed, if they
        // aren't being reflowed already!
      }
    }
    mBreakSinks.Clear();
  }

  mCanStopOnThisLine = PR_TRUE;
  ResetRunInfo();
}

void BuildTextRunsScanner::AccumulateRunInfo(nsTextFrame* aFrame)
{
  mMaxTextLength += aFrame->GetContentLength();
  mDoubleByteText |= aFrame->GetContent()->GetText()->Is2b();
  mLastFrame = aFrame;
  mCommonAncestorWithLastFrame = aFrame->GetParent();

  MappedFlow* mappedFlow = &mMappedFlows[mMappedFlows.Length() - 1];
  NS_ASSERTION(mappedFlow->mStartFrame == aFrame ||
               mappedFlow->GetContentEnd() == aFrame->GetContentOffset(),
               "Overlapping or discontiguous frames => BAD");
  mappedFlow->mEndFrame = static_cast<nsTextFrame*>(aFrame->GetNextContinuation());
  if (mCurrentFramesAllSameTextRun != aFrame->GetTextRun()) {
    mCurrentFramesAllSameTextRun = nsnull;
  }

  if (mStartOfLine) {
    mLineBreakBeforeFrames.AppendElement(aFrame);
    mStartOfLine = PR_FALSE;
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

static PRBool
HasTerminalNewline(const nsTextFrame* aFrame)
{
  if (aFrame->GetContentLength() == 0)
    return PR_FALSE;
  const nsTextFragment* frag = aFrame->GetContent()->GetText();
  return frag->CharAt(aFrame->GetContentEnd() - 1) == '\n';
}

PRBool
BuildTextRunsScanner::ContinueTextRunAcrossFrames(nsTextFrame* aFrame1, nsTextFrame* aFrame2)
{
  if (mBidiEnabled &&
      NS_GET_EMBEDDING_LEVEL(aFrame1) != NS_GET_EMBEDDING_LEVEL(aFrame2))
    return PR_FALSE;

  nsStyleContext* sc1 = aFrame1->GetStyleContext();
  const nsStyleText* textStyle1 = sc1->GetStyleText();
  // If the first frame ends in a preformatted newline, then we end the textrun
  // here. This avoids creating giant textruns for an entire plain text file.
  // Note that we create a single text frame for a preformatted text node,
  // even if it has newlines in it, so typically we won't see trailing newlines
  // until after reflow has broken up the frame into one (or more) frames per
  // line. That's OK though.
  if (textStyle1->WhiteSpaceIsSignificant() && HasTerminalNewline(aFrame1))
    return PR_FALSE;

  if (aFrame1->GetContent() == aFrame2->GetContent() &&
      aFrame1->GetNextInFlow() != aFrame2) {
    // aFrame2 must be a non-fluid continuation of aFrame1. This can happen
    // sometimes when the unicode-bidi property is used; the bidi resolver
    // breaks text into different frames even though the text has the same
    // direction. We can't allow these two frames to share the same textrun
    // because that would violate our invariant that two flows in the same
    // textrun have different content elements.
    return PR_FALSE;
  }

  nsStyleContext* sc2 = aFrame2->GetStyleContext();
  if (sc1 == sc2)
    return PR_TRUE;
  const nsStyleFont* fontStyle1 = sc1->GetStyleFont();
  const nsStyleFont* fontStyle2 = sc2->GetStyleFont();
  const nsStyleText* textStyle2 = sc2->GetStyleText();
  return fontStyle1->mFont.BaseEquals(fontStyle2->mFont) &&
    sc1->GetStyleVisibility()->mLangGroup == sc2->GetStyleVisibility()->mLangGroup &&
    nsLayoutUtils::GetTextRunFlagsForStyle(sc1, textStyle1, fontStyle1) ==
      nsLayoutUtils::GetTextRunFlagsForStyle(sc2, textStyle2, fontStyle2);
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
      if (mLastFrame->GetStyleContext() == aFrame->GetStyleContext() &&
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
        FlushFrames(PR_FALSE, PR_FALSE);
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
      mCurrentFramesAllSameTextRun = frame->GetTextRun();
      mCurrentRunTrimLeadingWhitespace = mTrimNextRunLeadingWhitespace;
    }
    return;
  }

  PRBool continueTextRun = CanTextRunCrossFrameBoundary(aFrame);
  PRBool descendInto = PR_TRUE;
  PRBool isBR = frameType == nsGkAtoms::brFrame;
  if (!continueTextRun) {
    // BR frames are special. We do not need or want to record a break opportunity
    // before a BR frame.
    FlushFrames(PR_TRUE, isBR);
    mCommonAncestorWithLastFrame = aFrame;
    mTrimNextRunLeadingWhitespace = PR_FALSE;
    // XXX do we need this? are there frames we need to descend into that aren't
    // float-containing-blocks?
    descendInto = !aFrame->IsFloatContainingBlock();
    mStartOfLine = PR_FALSE;
  }

  if (descendInto) {
    nsIFrame* f;
    for (f = aFrame->GetFirstChild(nsnull); f; f = f->GetNextSibling()) {
      ScanFrame(f);
    }
  }

  if (!continueTextRun) {
    // Really if we're a BR frame this is unnecessary since descendInto will be
    // false. In fact this whole "if" statement should move into the descendInto.
    FlushFrames(PR_TRUE, isBR);
    mCommonAncestorWithLastFrame = aFrame;
    mTrimNextRunLeadingWhitespace = PR_FALSE;
  }

  LiftCommonAncestorWithLastFrameToParent(aFrame->GetParent());
}

nsTextFrame*
BuildTextRunsScanner::GetNextBreakBeforeFrame(PRUint32* aIndex)
{
  PRUint32 index = *aIndex;
  if (index >= mLineBreakBeforeFrames.Length())
    return nsnull;
  *aIndex = index + 1;
  return static_cast<nsTextFrame*>(mLineBreakBeforeFrames.ElementAt(index));
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
  nsCOMPtr<nsIFontMetrics> metrics;
  nsLayoutUtils::GetFontMetricsForFrame(aFrame, getter_AddRefs(metrics));

  if (!metrics)
    return nsnull;

  nsIFontMetrics* metricsRaw = metrics;
  nsIThebesFontMetrics* fm = static_cast<nsIThebesFontMetrics*>(metricsRaw);
  // XXX this is a bit bogus, we're releasing 'metrics' so the returned font-group
  // might actually be torn down, although because of the way the device context
  // caches font metrics, this seems to not actually happen. But we should fix
  // this.
  return fm->GetThebesFontGroup();
}

static already_AddRefed<gfxContext>
GetReferenceRenderingContext(nsTextFrame* aTextFrame, nsIRenderingContext* aRC)
{
  nsCOMPtr<nsIRenderingContext> tmp = aRC;
  if (!tmp) {
    nsresult rv = aTextFrame->PresContext()->PresShell()->
      CreateRenderingContext(aTextFrame, getter_AddRefs(tmp));
    if (NS_FAILED(rv))
      return nsnull;
  }

  gfxContext* ctx = tmp->ThebesContext();
  NS_ADDREF(ctx);
  return ctx;
}

/**
 * The returned textrun must be released via gfxTextRunCache::ReleaseTextRun
 * or gfxTextRunCache::AutoTextRun.
 */
static gfxTextRun*
GetHyphenTextRun(gfxTextRun* aTextRun, gfxContext* aContext, nsTextFrame* aTextFrame)
{
  nsRefPtr<gfxContext> ctx = aContext;
  if (!ctx) {
    ctx = GetReferenceRenderingContext(aTextFrame, nsnull);
  }
  if (!ctx)
    return nsnull;

  gfxFontGroup* fontGroup = aTextRun->GetFontGroup();
  PRUint32 flags = gfxFontGroup::TEXT_IS_PERSISTENT;

  static const PRUnichar unicodeHyphen = 0x2010;
  gfxTextRun* textRun =
    gfxTextRunCache::MakeTextRun(&unicodeHyphen, 1, fontGroup, ctx,
                                 aTextRun->GetAppUnitsPerDevUnit(), flags);
  if (textRun && textRun->CountMissingGlyphs() == 0)
    return textRun;

  gfxTextRunCache::ReleaseTextRun(textRun);

  static const PRUint8 dash = '-';
  return gfxTextRunCache::MakeTextRun(&dash, 1, fontGroup, ctx,
                                      aTextRun->GetAppUnitsPerDevUnit(),
                                      flags);
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

gfxTextRun*
BuildTextRunsScanner::BuildTextRunForFrames(void* aTextBuffer)
{
  gfxSkipCharsBuilder builder;

  const void* textPtr = aTextBuffer;
  PRBool anySmallcapsStyle = PR_FALSE;
  PRBool anyTextTransformStyle = PR_FALSE;
  nsIContent* lastContent = nsnull;
  PRInt32 endOfLastContent = 0;
  PRUint32 textFlags = nsTextFrameUtils::TEXT_NO_BREAKS;

  if (mCurrentRunTrimLeadingWhitespace) {
    textFlags |= nsTextFrameUtils::TEXT_INCOMING_WHITESPACE;
  }

  nsAutoTArray<PRInt32,50> textBreakPoints;
  TextRunUserData dummyData;
  TextRunMappedFlow dummyMappedFlow;

  TextRunUserData* userData;
  // If the situation is particularly simple (and common) we don't need to
  // allocate userData.
  if (mMappedFlows.Length() == 1 && !mMappedFlows[0].mEndFrame &&
      mMappedFlows[0].mStartFrame->GetContentOffset() == 0) {
    userData = &dummyData;
    dummyData.mMappedFlows = &dummyMappedFlow;
  } else {
    userData = static_cast<TextRunUserData*>
      (nsMemory::Alloc(sizeof(TextRunUserData) + mMappedFlows.Length()*sizeof(TextRunMappedFlow)));
    userData->mMappedFlows = reinterpret_cast<TextRunMappedFlow*>(userData + 1);
  }
  userData->mMappedFlowCount = mMappedFlows.Length();
  userData->mLastFlowIndex = 0;

  PRUint32 currentTransformedTextOffset = 0;

  PRUint32 nextBreakIndex = 0;
  nsTextFrame* nextBreakBeforeFrame = GetNextBreakBeforeFrame(&nextBreakIndex);

  PRUint32 i;
  const nsStyleText* textStyle = nsnull;
  const nsStyleFont* fontStyle = nsnull;
  nsStyleContext* lastStyleContext = nsnull;
  for (i = 0; i < mMappedFlows.Length(); ++i) {
    MappedFlow* mappedFlow = &mMappedFlows[i];
    nsTextFrame* f = mappedFlow->mStartFrame;

    lastStyleContext = f->GetStyleContext();
    // Detect use of text-transform or font-variant anywhere in the run
    textStyle = f->GetStyleText();
    if (NS_STYLE_TEXT_TRANSFORM_NONE != textStyle->mTextTransform) {
      anyTextTransformStyle = PR_TRUE;
    }
    textFlags |= GetSpacingFlags(textStyle->mLetterSpacing);
    textFlags |= GetSpacingFlags(textStyle->mWordSpacing);
    PRBool compressWhitespace = !textStyle->WhiteSpaceIsSignificant();
    if (NS_STYLE_TEXT_ALIGN_JUSTIFY == textStyle->mTextAlign && compressWhitespace) {
      textFlags |= gfxTextRunFactory::TEXT_ENABLE_SPACING;
    }
    fontStyle = f->GetStyleFont();
    if (NS_STYLE_FONT_VARIANT_SMALL_CAPS == fontStyle->mFont.variant) {
      anySmallcapsStyle = PR_TRUE;
    }

    // Figure out what content is included in this flow.
    nsIContent* content = f->GetContent();
    const nsTextFragment* frag = content->GetText();
    PRInt32 contentStart = mappedFlow->mStartFrame->GetContentOffset();
    PRInt32 contentEnd = mappedFlow->GetContentEnd();
    PRInt32 contentLength = contentEnd - contentStart;

    TextRunMappedFlow* newFlow = &userData->mMappedFlows[i];
    newFlow->mStartFrame = mappedFlow->mStartFrame;
    newFlow->mDOMOffsetToBeforeTransformOffset = builder.GetCharCount() -
      mappedFlow->mStartFrame->GetContentOffset();
    newFlow->mContentLength = contentLength;

    while (nextBreakBeforeFrame && nextBreakBeforeFrame->GetContent() == content) {
      textBreakPoints.AppendElement(
          nextBreakBeforeFrame->GetContentOffset() + newFlow->mDOMOffsetToBeforeTransformOffset);
      nextBreakBeforeFrame = GetNextBreakBeforeFrame(&nextBreakIndex);
    }

    PRUint32 analysisFlags;
    if (frag->Is2b()) {
      NS_ASSERTION(mDoubleByteText, "Wrong buffer char size!");
      PRUnichar* bufStart = static_cast<PRUnichar*>(aTextBuffer);
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
          return nsnull;
        }
        PRUint8* bufStart = tempBuf.Elements();
        PRUint8* end = nsTextFrameUtils::TransformText(
            reinterpret_cast<const PRUint8*>(frag->Get1b()) + contentStart, contentLength,
            bufStart, compressWhitespace, &mTrimNextRunLeadingWhitespace,
            &builder, &analysisFlags);
        aTextBuffer = ExpandBuffer(static_cast<PRUnichar*>(aTextBuffer),
                                   tempBuf.Elements(), end - tempBuf.Elements());
      } else {
        PRUint8* bufStart = static_cast<PRUint8*>(aTextBuffer);
        PRUint8* end = nsTextFrameUtils::TransformText(
            reinterpret_cast<const PRUint8*>(frag->Get1b()) + contentStart, contentLength,
            bufStart,
            compressWhitespace, &mTrimNextRunLeadingWhitespace, &builder, &analysisFlags);
        aTextBuffer = end;
      }
    }
    // In CSS 2.1, we do not compress a space that is preceded by a non-compressible
    // space.
    if (!compressWhitespace) {
      mTrimNextRunLeadingWhitespace = PR_FALSE;
    }
    textFlags |= analysisFlags;

    currentTransformedTextOffset =
      (static_cast<const PRUint8*>(aTextBuffer) - static_cast<const PRUint8*>(textPtr)) >> mDoubleByteText;

    lastContent = content;
    endOfLastContent = contentEnd;
  }

  // Check for out-of-memory in gfxSkipCharsBuilder
  if (!builder.IsOK()) {
    DestroyUserData(userData);
    return nsnull;
  }

  void* finalUserData;
  if (userData == &dummyData) {
    textFlags |= nsTextFrameUtils::TEXT_IS_SIMPLE_FLOW;
    userData = nsnull;
    finalUserData = mMappedFlows[0].mStartFrame;
  } else {
    finalUserData = userData;
  }

  PRUint32 transformedLength = currentTransformedTextOffset;

//  Disable this because it breaks the word cache. Disable at least until
//  we have a CharacterDataWillChange notification.
//
//  if (!(textFlags & nsTextFrameUtils::TEXT_WAS_TRANSFORMED) &&
//      mMappedFlows.Length() == 1) {
//    // The textrun maps one continuous, unmodified run of DOM text. It can
//    // point to the DOM text directly.
//    const nsTextFragment* frag = lastContent->GetText();
//    if (frag->Is2b()) {
//      textPtr = frag->Get2b() + mMappedFlows[0].mContentOffset;
//    } else {
//      textPtr = frag->Get1b() + mMappedFlows[0].mContentOffset;
//    }
//    textFlags |= gfxTextRunFactory::TEXT_IS_PERSISTENT;
//  }
//
  // Now build the textrun
  nsTextFrame* firstFrame = mMappedFlows[0].mStartFrame;
  gfxFontGroup* fontGroup = GetFontGroupForFrame(firstFrame);
  if (!fontGroup) {
    DestroyUserData(userData);
    return nsnull;
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
  if (mTrimNextRunLeadingWhitespace) {
    textFlags |= nsTextFrameUtils::TEXT_TRAILING_WHITESPACE;
  }
  // ContinueTextRunAcrossFrames guarantees that it doesn't matter which
  // frame's style is used, so use the last frame's
  textFlags |= nsLayoutUtils::GetTextRunFlagsForStyle(lastStyleContext,
      textStyle, fontStyle);
  // XXX this is a bit of a hack. For performance reasons, if we're favouring
  // performance over quality, don't try to get accurate glyph extents.
  if (!(textFlags & gfxTextRunFactory::TEXT_OPTIMIZE_SPEED)) {
    textFlags |= gfxTextRunFactory::TEXT_NEED_BOUNDING_BOX;
  }

  gfxSkipChars skipChars;
  skipChars.TakeFrom(&builder);
  // Convert linebreak coordinates to transformed string offsets
  NS_ASSERTION(nextBreakIndex == mLineBreakBeforeFrames.Length(),
               "Didn't find all the frames to break-before...");
  gfxSkipCharsIterator iter(skipChars);
  nsAutoTArray<PRUint32,50> textBreakPointsAfterTransform;
  for (i = 0; i < textBreakPoints.Length(); ++i) {
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
  nsTArray<nsStyleContext*> styles;
  if (transformingFactory) {
    iter.SetOriginalOffset(0);
    for (i = 0; i < mMappedFlows.Length(); ++i) {
      MappedFlow* mappedFlow = &mMappedFlows[i];
      nsTextFrame* f;
      for (f = mappedFlow->mStartFrame; f != mappedFlow->mEndFrame;
           f = static_cast<nsTextFrame*>(f->GetNextContinuation())) {
        PRUint32 offset = iter.GetSkippedOffset();
        iter.AdvanceOriginal(f->GetContentLength());
        PRUint32 end = iter.GetSkippedOffset();
        nsStyleContext* sc = f->GetStyleContext();
        PRUint32 j;
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
        textBreakPointsAfterTransform.Elements(), textBreakPointsAfterTransform.Length(),
        firstFrame->PresContext()->AppUnitsPerDevPixel() };

  if (mDoubleByteText) {
    const PRUnichar* text = static_cast<const PRUnichar*>(textPtr);
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
    const PRUint8* text = static_cast<const PRUint8*>(textPtr);
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
    DestroyUserData(userData);
    return nsnull;
  }

  // We have to set these up after we've created the textrun, because
  // the breaks may be stored in the textrun during this very call.
  // This is a bit annoying because it requires another loop over the frames
  // making up the textrun, but I don't see a way to avoid this.
  SetupBreakSinksForTextRun(textRun, PR_FALSE, mSkipIncompleteTextRuns);

  if (mSkipIncompleteTextRuns) {
    mSkipIncompleteTextRuns = !TextContainsLineBreakerWhiteSpace(textPtr,
        transformedLength, mDoubleByteText);
    
    // Nuke the textrun
    gTextRuns->RemoveFromCache(textRun);
    delete textRun;
    DestroyUserData(userData);
    return nsnull;
  }

  // Actually wipe out the textruns associated with the mapped frames and associate
  // those frames with this text run.
  AssignTextRun(textRun);
  return textRun;
}

static PRBool
HasCompressedLeadingWhitespace(nsTextFrame* aFrame, PRInt32 aContentEndOffset,
                               const gfxSkipCharsIterator& aIterator)
{
  if (!aIterator.IsOriginalCharSkipped())
    return PR_FALSE;

  gfxSkipCharsIterator iter = aIterator;
  PRInt32 frameContentOffset = aFrame->GetContentOffset();
  const nsTextFragment* frag = aFrame->GetContent()->GetText();
  while (frameContentOffset < aContentEndOffset && iter.IsOriginalCharSkipped()) {
    if (IsTrimmableSpace(frag, frameContentOffset))
      return PR_TRUE;
    ++frameContentOffset;
    iter.AdvanceOriginal(1);
  }
  return PR_FALSE;
}

void
BuildTextRunsScanner::SetupBreakSinksForTextRun(gfxTextRun* aTextRun,
                                                PRBool aIsExistingTextRun,
                                                PRBool aSuppressSink)
{
  // textruns have uniform language
  nsIAtom* lang = mMappedFlows[0].mStartFrame->GetStyleVisibility()->mLangGroup;
  // We keep this pointed at the skip-chars data for the current mappedFlow.
  // This lets us cheaply check whether the flow has compressed initial
  // whitespace...
  gfxSkipCharsIterator iter(aTextRun->GetSkipChars());

  PRUint32 i;
  for (i = 0; i < mMappedFlows.Length(); ++i) {
    MappedFlow* mappedFlow = &mMappedFlows[i];
    PRUint32 offset = iter.GetSkippedOffset();
    gfxSkipCharsIterator iterNext = iter;
    iterNext.AdvanceOriginal(mappedFlow->GetContentEnd() -
            mappedFlow->mStartFrame->GetContentOffset());

    nsAutoPtr<BreakSink>* breakSink = mBreakSinks.AppendElement(
      new BreakSink(aTextRun, mContext, offset, aIsExistingTextRun));
    if (!breakSink || !*breakSink)
      return;

    PRUint32 length = iterNext.GetSkippedOffset() - offset;
    PRUint32 flags = 0;
    nsIFrame* initialBreakController = mappedFlow->mAncestorControllingInitialBreak;
    if (!initialBreakController) {
      initialBreakController = mLineContainer;
    }
    if (!initialBreakController->GetStyleText()->WhiteSpaceCanWrap()) {
      flags |= nsLineBreaker::BREAK_SUPPRESS_INITIAL;
    }
    nsTextFrame* startFrame = mappedFlow->mStartFrame;
    const nsStyleText* textStyle = startFrame->GetStyleText();
    if (!textStyle->WhiteSpaceCanWrap()) {
      flags |= nsLineBreaker::BREAK_SUPPRESS_INSIDE;
    }
    if (aTextRun->GetFlags() & nsTextFrameUtils::TEXT_NO_BREAKS) {
      flags |= nsLineBreaker::BREAK_SKIP_SETTING_NO_BREAKS;
    }
    if (textStyle->mTextTransform == NS_STYLE_TEXT_TRANSFORM_CAPITALIZE) {
      flags |= nsLineBreaker::BREAK_NEED_CAPITALIZATION;
    }

    if (HasCompressedLeadingWhitespace(startFrame, mappedFlow->GetContentEnd(), iter)) {
      mLineBreaker.AppendInvisibleWhitespace(flags);
    }

    if (length > 0) {
      BreakSink* sink = aSuppressSink ? nsnull : (*breakSink).get();
      if (aTextRun->GetFlags() & gfxFontGroup::TEXT_IS_8BIT) {
        mLineBreaker.AppendText(lang, aTextRun->GetText8Bit() + offset,
                                length, flags, sink);
      } else {
        mLineBreaker.AppendText(lang, aTextRun->GetTextUnicode() + offset,
                                length, flags, sink);
      }
    }
    
    iter = iterNext;
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
         f = static_cast<nsTextFrame*>(f->GetNextContinuation())) {
#ifdef DEBUG_roc
      if (f->GetTextRun()) {
        gfxTextRun* textRun = f->GetTextRun();
        if (textRun->GetFlags() & nsTextFrameUtils::TEXT_IS_SIMPLE_FLOW) {
          if (mMappedFlows[0].mStartFrame != static_cast<nsTextFrame*>(textRun->GetUserData())) {
            NS_WARNING("REASSIGNING SIMPLE FLOW TEXT RUN!");
          }
        } else {
          TextRunUserData* userData =
            static_cast<TextRunUserData*>(textRun->GetUserData());
         
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
    // BuildTextRunForFrames mashes together mapped flows for the same element,
    // so we do that here too.
    lastContent = startFrame->GetContent();
  }
}

gfxSkipCharsIterator
nsTextFrame::EnsureTextRun(gfxContext* aReferenceContext, nsIFrame* aLineContainer,
                           const nsLineList::iterator* aLine,
                           PRUint32* aFlowEndInTextRun)
{
  if (mTextRun && (!aLine || !(*aLine)->GetInvalidateTextRuns())) {
    if (mTextRun->GetExpirationState()->IsTracked()) {
      gTextRuns->MarkUsed(mTextRun);
    }
  } else {
    nsRefPtr<gfxContext> ctx = aReferenceContext;
    if (!ctx) {
      ctx = GetReferenceRenderingContext(this, nsnull);
    }
    if (ctx) {
      BuildTextRuns(ctx, this, aLineContainer, aLine);
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

  TextRunUserData* userData = static_cast<TextRunUserData*>(mTextRun->GetUserData());
  // Find the flow that contains us
  PRInt32 direction;
  PRInt32 startAt = userData->mLastFlowIndex;
  // Search first forward and then backward from the current position
  for (direction = 1; direction >= -1; direction -= 2) {
    PRInt32 i;
    for (i = startAt; 0 <= i && i < userData->mMappedFlowCount; i += direction) {
      TextRunMappedFlow* flow = &userData->mMappedFlows[i];
      if (flow->mStartFrame->GetContent() == mContent) {
        // Since textruns can only contain one flow for a given content element,
        // this must be our flow.
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
GetEndOfTrimmedText(const nsTextFragment* aFrag,
                    PRUint32 aStart, PRUint32 aEnd,
                    gfxSkipCharsIterator* aIterator)
{
  aIterator->SetSkippedOffset(aEnd);
  while (aIterator->GetSkippedOffset() > aStart) {
    aIterator->AdvanceSkipped(-1);
    if (!IsTrimmableSpace(aFrag, aIterator->GetOriginalOffset()))
      return aIterator->GetSkippedOffset() + 1;
  }
  return aStart;
}

nsTextFrame::TrimmedOffsets
nsTextFrame::GetTrimmedOffsets(const nsTextFragment* aFrag,
                               PRBool aTrimAfter)
{
  NS_ASSERTION(mTextRun, "Need textrun here");

  TrimmedOffsets offsets = { GetContentOffset(), GetContentLength() };
  const nsStyleText* textStyle = GetStyleText();
  if (textStyle->WhiteSpaceIsSignificant())
    return offsets;

  if (GetStateBits() & TEXT_START_OF_LINE) {
    PRInt32 whitespaceCount =
      GetTrimmableWhitespaceCount(aFrag, offsets.mStart, offsets.mLength, 1);
    offsets.mStart += whitespaceCount;
    offsets.mLength -= whitespaceCount;
  }

  if (aTrimAfter && (GetStateBits() & TEXT_END_OF_LINE)) {
    PRInt32 whitespaceCount =
      GetTrimmableWhitespaceCount(aFrag, offsets.GetEnd() - 1,
                                  offsets.mLength, -1);
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
  if (ch == '\n' || ch == '\t')
    return PR_TRUE;
  if (ch == ' ') {
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
      const void* p = memchr(str, ch, aLength);
      if (p)
        return (static_cast<const char*>(p) - str) + aOffset;
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
  if (aContentLength == PR_INT32_MAX)
    return PR_TRUE;
  gfxSkipCharsIterator iter(aStart);
  iter.AdvanceOriginal(aContentLength);
  return iter.GetSkippedOffset() >= aOffset + aLength;
}
#endif

class PropertyProvider : public gfxTextRun::PropertyProvider {
public:
  /**
   * Use this constructor for reflow, when we don't know what text is
   * really mapped by the frame and we have a lot of other data around.
   * 
   * @param aLength can be PR_INT32_MAX to indicate we cover all the text
   * associated with aFrame up to where its flow chain ends in the given
   * textrun. If PR_INT32_MAX is passed, justification and hyphen-related methods
   * cannot be called, nor can GetOriginalLength().
   */
  PropertyProvider(gfxTextRun* aTextRun, const nsStyleText* aTextStyle,
                   const nsTextFragment* aFrag, nsTextFrame* aFrame,
                   const gfxSkipCharsIterator& aStart, PRInt32 aLength,
                   nsIFrame* aLineContainer,
                   nscoord aOffsetFromBlockOriginForTabs)
    : mTextRun(aTextRun), mFontGroup(nsnull), mTextStyle(aTextStyle), mFrag(aFrag),
      mLineContainer(aLineContainer),
      mFrame(aFrame), mStart(aStart), mTempIterator(aStart),
      mTabWidths(nsnull), mLength(aLength),
      mWordSpacing(StyleToCoord(mTextStyle->mWordSpacing)),
      mLetterSpacing(StyleToCoord(mTextStyle->mLetterSpacing)),
      mJustificationSpacing(0),
      mHyphenWidth(-1),
      mOffsetFromBlockOriginForTabs(aOffsetFromBlockOriginForTabs),
      mReflowing(PR_TRUE)
  {
    NS_ASSERTION(mStart.IsInitialized(), "Start not initialized?");
  }

  /**
   * Use this constructor after the frame has been reflowed and we don't
   * have other data around. Gets everything from the frame. EnsureTextRun
   * *must* be called before this!!!
   */
  PropertyProvider(nsTextFrame* aFrame, const gfxSkipCharsIterator& aStart)
    : mTextRun(aFrame->GetTextRun()), mFontGroup(nsnull),
      mTextStyle(aFrame->GetStyleText()),
      mFrag(aFrame->GetContent()->GetText()),
      mLineContainer(nsnull),
      mFrame(aFrame), mStart(aStart), mTempIterator(aStart),
      mTabWidths(nsnull),
      mLength(aFrame->GetContentLength()),
      mWordSpacing(StyleToCoord(mTextStyle->mWordSpacing)),
      mLetterSpacing(StyleToCoord(mTextStyle->mLetterSpacing)),
      mJustificationSpacing(0),
      mHyphenWidth(-1),
      mOffsetFromBlockOriginForTabs(0),
      mReflowing(PR_FALSE)
  {
    NS_ASSERTION(mTextRun, "Textrun not initialized!");
  }

  // Call this after construction if you're not going to reflow the text
  void InitializeForDisplay(PRBool aTrimAfter);

  virtual void GetSpacing(PRUint32 aStart, PRUint32 aLength, Spacing* aSpacing);
  virtual gfxFloat GetHyphenWidth();
  virtual void GetHyphenationBreaks(PRUint32 aStart, PRUint32 aLength,
                                    PRPackedBool* aBreakBefore);

  void GetSpacingInternal(PRUint32 aStart, PRUint32 aLength, Spacing* aSpacing,
                          PRBool aIgnoreTabs);

  /**
   * Count the number of justifiable characters in the given DOM range
   */
  PRUint32 ComputeJustifiableCharacters(PRInt32 aOffset, PRInt32 aLength);
  /**
   * Find the start and end of the justifiable characters. Does not depend on the
   * position of aStart or aEnd, although it's most efficient if they are near the
   * start and end of the text frame.
   */
  void FindJustificationRange(gfxSkipCharsIterator* aStart,
                              gfxSkipCharsIterator* aEnd);

  const nsStyleText* GetStyleText() { return mTextStyle; }
  nsTextFrame* GetFrame() { return mFrame; }
  // This may not be equal to the frame offset/length in because we may have
  // adjusted for whitespace trimming according to the state bits set in the frame
  // (for the static provider)
  const gfxSkipCharsIterator& GetStart() { return mStart; }
  // May return PR_INT32_MAX if that was given to the constructor
  PRUint32 GetOriginalLength() {
    NS_ASSERTION(mLength != PR_INT32_MAX, "Length not known");
    return mLength;
  }
  const nsTextFragment* GetFragment() { return mFrag; }

  gfxFontGroup* GetFontGroup() {
    if (!mFontGroup) {
      mFontGroup = GetFontGroupForFrame(mFrame);
    }
    return mFontGroup;
  }

  gfxFloat* GetTabWidths(PRUint32 aTransformedStart, PRUint32 aTransformedLength);

  const gfxSkipCharsIterator& GetEndHint() { return mTempIterator; }

protected:
  void SetupJustificationSpacing();
  
  gfxTextRun*           mTextRun;
  gfxFontGroup*         mFontGroup;
  const nsStyleText*    mTextStyle;
  const nsTextFragment* mFrag;
  nsIFrame*             mLineContainer;
  nsTextFrame*          mFrame;
  gfxSkipCharsIterator  mStart;  // Offset in original and transformed string
  gfxSkipCharsIterator  mTempIterator;
  nsTArray<gfxFloat>*   mTabWidths;  // widths for each transformed string character
  PRInt32               mLength; // DOM string length, may be PR_INT32_MAX
  gfxFloat              mWordSpacing;     // space for each whitespace char
  gfxFloat              mLetterSpacing;   // space for each letter
  gfxFloat              mJustificationSpacing;
  gfxFloat              mHyphenWidth;
  gfxFloat              mOffsetFromBlockOriginForTabs;
  PRPackedBool          mReflowing;
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
  GetSpacingInternal(aStart, aLength, aSpacing,
                     (mTextRun->GetFlags() & nsTextFrameUtils::TEXT_HAS_TAB) == 0);
}

static PRBool
CanAddSpacingAfter(gfxTextRun* aTextRun, PRUint32 aOffset)
{
  if (aOffset + 1 >= aTextRun->GetLength())
    return PR_TRUE;
  return aTextRun->IsClusterStart(aOffset + 1) &&
    aTextRun->IsLigatureGroupStart(aOffset + 1);
}

void
PropertyProvider::GetSpacingInternal(PRUint32 aStart, PRUint32 aLength,
                                     Spacing* aSpacing, PRBool aIgnoreTabs)
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
        if (CanAddSpacingAfter(mTextRun, run.GetSkippedOffset() + i)) {
          // End of a cluster, not in a ligature: put letter-spacing after it
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
  if (!aIgnoreTabs) {
    gfxFloat* tabs = GetTabWidths(aStart, aLength);
    if (tabs) {
      for (index = 0; index < aLength; ++index) {
        aSpacing[index].mAfter += tabs[index];
      }
    }
  }

  // Now add in justification spacing
  if (mJustificationSpacing) {
    gfxFloat halfJustificationSpace = mJustificationSpacing/2;
    // Scan non-skipped characters and adjust justifiable chars, adding
    // justification space on either side of the cluster
    PRBool isCJK = IsChineseJapaneseLangGroup(mFrame);
    gfxSkipCharsIterator justificationStart(mStart), justificationEnd(mStart);
    FindJustificationRange(&justificationStart, &justificationEnd);

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

static void TabWidthDestructor(void* aObject, nsIAtom* aProp, void* aValue,
                               void* aData)
{
  delete static_cast<nsTArray<gfxFloat>*>(aValue);
}

gfxFloat*
PropertyProvider::GetTabWidths(PRUint32 aStart, PRUint32 aLength)
{
  if (!mTabWidths) {
    if (!mReflowing) {
      mTabWidths = static_cast<nsTArray<gfxFloat>*>
                              (mFrame->GetProperty(nsGkAtoms::tabWidthProperty));
      if (!mTabWidths) {
        NS_WARNING("We need precomputed tab widths, but they're not here...");
        return nsnull;
      }
    } else {
      nsAutoPtr<nsTArray<gfxFloat> > tabs(new nsTArray<gfxFloat>());
      if (!tabs)
        return nsnull;
      nsresult rv = mFrame->SetProperty(nsGkAtoms::tabWidthProperty, tabs,
                                        TabWidthDestructor, nsnull);
      if (NS_FAILED(rv))
        return nsnull;
      mTabWidths = tabs.forget();
    }
  }

  PRUint32 startOffset = mStart.GetSkippedOffset();
  PRUint32 tabsEnd = startOffset + mTabWidths->Length();
  if (tabsEnd < aStart + aLength) {
    if (!mReflowing) {
      NS_WARNING("We need precomputed tab widths, but we don't have enough...");
      return nsnull;
    }
    
    if (!mTabWidths->AppendElements(aStart + aLength - tabsEnd))
      return nsnull;
    
    PRUint32 i;
    if (!mLineContainer) {
      NS_WARNING("Tabs encountered in a situation where we don't support tabbing");
      for (i = tabsEnd; i < aStart + aLength; ++i) {
        (*mTabWidths)[i - startOffset] = 0;
      }
    } else {
      gfxFloat tabWidth = NS_round(8*mTextRun->GetAppUnitsPerDevUnit()*
        GetFontMetrics(GetFontGroupForFrame(mLineContainer)).spaceWidth);
      
      for (i = tabsEnd; i < aStart + aLength; ++i) {
        Spacing spacing;
        GetSpacingInternal(i, 1, &spacing, PR_TRUE);
        mOffsetFromBlockOriginForTabs += spacing.mBefore;
  
        if (mTextRun->GetChar(i) != '\t') {
          (*mTabWidths)[i - startOffset] = 0;
          if (mTextRun->IsClusterStart(i)) {
            PRUint32 clusterEnd = i + 1;
            while (clusterEnd < mTextRun->GetLength() &&
                   !mTextRun->IsClusterStart(clusterEnd)) {
              ++clusterEnd;
            }
            mOffsetFromBlockOriginForTabs +=
              mTextRun->GetAdvanceWidth(i, clusterEnd - i, nsnull);
          }
        } else {
          // Advance mOffsetFromBlockOriginForTabs to the next multiple of tabWidth
          // Ensure that if it's just epsilon less than a multiple of tabWidth, we still
          // advance by tabWidth.
          static const double EPSILON = 0.000001;
          double nextTab = NS_ceil(mOffsetFromBlockOriginForTabs/tabWidth)*tabWidth;
          if (nextTab < mOffsetFromBlockOriginForTabs + EPSILON) {
            nextTab += tabWidth;
          }
          (*mTabWidths)[i - startOffset] = nextTab - mOffsetFromBlockOriginForTabs;
          mOffsetFromBlockOriginForTabs = nextTab;
        }
  
        mOffsetFromBlockOriginForTabs += spacing.mAfter;
      }
    }
  }

  return mTabWidths->Elements() + aStart - startOffset;
}

gfxFloat
PropertyProvider::GetHyphenWidth()
{
  if (mHyphenWidth < 0) {
    gfxTextRunCache::AutoTextRun hyphenTextRun(GetHyphenTextRun(mTextRun, nsnull, mFrame));
    mHyphenWidth = mLetterSpacing;
    if (hyphenTextRun.get()) {
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
  NS_PRECONDITION(mLength != PR_INT32_MAX, "Can't call this with undefined length");

  if (!mTextStyle->WhiteSpaceCanWrap()) {
    memset(aBreakBefore, PR_FALSE, aLength);
    return;
  }

  // Iterate through the original-string character runs
  nsSkipCharsRunIterator
    run(mStart, nsSkipCharsRunIterator::LENGTH_UNSKIPPED_ONLY, aLength);
  run.SetSkippedOffset(aStart);
  // We need to visit skipped characters so that we can detect SHY
  run.SetVisitSkipped();

  PRInt32 prevTrailingCharOffset = run.GetPos().GetOriginalOffset() - 1;
  PRBool allowHyphenBreakBeforeNextChar =
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
      PRInt32 runOffsetInSubstring = run.GetSkippedOffset() - aStart;
      memset(aBreakBefore + runOffsetInSubstring, 0, run.GetRunLength());
      // Don't allow hyphen breaks at the start of the line
      aBreakBefore[runOffsetInSubstring] = allowHyphenBreakBeforeNextChar &&
          (!(mFrame->GetStateBits() & TEXT_START_OF_LINE) ||
           run.GetSkippedOffset() > mStart.GetSkippedOffset());
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
PropertyProvider::FindJustificationRange(gfxSkipCharsIterator* aStart,
                                         gfxSkipCharsIterator* aEnd)
{
  NS_PRECONDITION(mLength != PR_INT32_MAX, "Can't call this with undefined length");
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
  NS_PRECONDITION(mLength != PR_INT32_MAX, "Can't call this with undefined length");

  if (NS_STYLE_TEXT_ALIGN_JUSTIFY != mTextStyle->mTextAlign ||
      mTextStyle->WhiteSpaceIsSignificant())
    return;

  gfxSkipCharsIterator start(mStart), end(mStart);
  end.AdvanceOriginal(mLength);
  gfxSkipCharsIterator realEnd(end);
  FindJustificationRange(&start, &end);

  PRInt32 justifiableCharacters =
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
    gfxTextRunCache::AutoTextRun hyphenTextRun(GetHyphenTextRun(mTextRun, nsnull, mFrame));
    if (hyphenTextRun.get()) {
      naturalWidth +=
        hyphenTextRun->GetAdvanceWidth(0, hyphenTextRun->GetLength(), nsnull);
    }
  }
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
    mTimer = nsnull;
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
    mPresContext(aFrame->PresContext()),
    mInitCommonColors(PR_FALSE),
    mInitSelectionColors(PR_FALSE)
{
  for (int i = 0; i < 4; i++)
    mIMEStyle[i].mInit = PR_FALSE;
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

  nsIMEStyle* IMEStyle = GetIMEStyle(aIndex);
  *aForeColor = IMEStyle->mTextColor;
  *aBackColor = IMEStyle->mBGColor;
}

PRBool
nsTextPaintStyle::GetIMEUnderline(PRInt32  aIndex,
                                  nscolor* aLineColor,
                                  float*   aRelativeSize,
                                  PRUint8* aStyle)
{
  NS_ASSERTION(aLineColor, "aLineColor is null");
  NS_ASSERTION(aRelativeSize, "aRelativeSize is null");
  NS_ASSERTION(aIndex >= 0 && aIndex < 4, "Index out of range");

  nsIMEStyle* IMEStyle = GetIMEStyle(aIndex);
  if (IMEStyle->mUnderlineStyle == NS_STYLE_BORDER_STYLE_NONE ||
      IMEStyle->mUnderlineColor == NS_TRANSPARENT ||
      mIMEUnderlineRelativeSize <= 0.0f)
    return PR_FALSE;

  *aLineColor = IMEStyle->mUnderlineColor;
  *aRelativeSize = mIMEUnderlineRelativeSize;
  *aStyle = IMEStyle->mUnderlineStyle;
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
  return static_cast<nsIContent*>(aNode);
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

nsTextPaintStyle::nsIMEStyle*
nsTextPaintStyle::GetIMEStyle(PRInt32 aIndex)
{
  InitIMEStyle(aIndex);
  return &mIMEStyle[aIndex];
}

struct StyleIDs {
  nsILookAndFeel::nsColorID mForeground, mBackground, mLine;
  nsILookAndFeel::nsMetricID mLineStyle;
};
static StyleIDs IMEStyleIDs[] = {
  { nsILookAndFeel::eColor_IMERawInputForeground,
    nsILookAndFeel::eColor_IMERawInputBackground,
    nsILookAndFeel::eColor_IMERawInputUnderline,
    nsILookAndFeel::eMetric_IMERawInputUnderlineStyle },
  { nsILookAndFeel::eColor_IMESelectedRawTextForeground,
    nsILookAndFeel::eColor_IMESelectedRawTextBackground,
    nsILookAndFeel::eColor_IMESelectedRawTextUnderline,
    nsILookAndFeel::eMetric_IMESelectedRawTextUnderlineStyle },
  { nsILookAndFeel::eColor_IMEConvertedTextForeground,
    nsILookAndFeel::eColor_IMEConvertedTextBackground,
    nsILookAndFeel::eColor_IMEConvertedTextUnderline,
    nsILookAndFeel::eMetric_IMEConvertedTextUnderlineStyle },
  { nsILookAndFeel::eColor_IMESelectedConvertedTextForeground,
    nsILookAndFeel::eColor_IMESelectedConvertedTextBackground,
    nsILookAndFeel::eColor_IMESelectedConvertedTextUnderline,
    nsILookAndFeel::eMetric_IMESelectedConvertedTextUnderline }
};

static PRUint8 sUnderlineStyles[] = {
  NS_STYLE_BORDER_STYLE_NONE,   // NS_UNDERLINE_STYLE_NONE   0
  NS_STYLE_BORDER_STYLE_DOTTED, // NS_UNDERLINE_STYLE_DOTTED 1
  NS_STYLE_BORDER_STYLE_DASHED, // NS_UNDERLINE_STYLE_DASHED 2
  NS_STYLE_BORDER_STYLE_SOLID,  // NS_UNDERLINE_STYLE_SOLID  3
  NS_STYLE_BORDER_STYLE_DOUBLE  // NS_UNDERLINE_STYLE_DOUBLE 4
};

void
nsTextPaintStyle::InitIMEStyle(PRInt32 aIndex)
{
  nsIMEStyle* IMEStyle = &mIMEStyle[aIndex];
  if (IMEStyle->mInit)
    return;

  StyleIDs* styleIDs = &IMEStyleIDs[aIndex];

  nsILookAndFeel* look = mPresContext->LookAndFeel();
  nscolor foreColor, backColor, lineColor;
  PRInt32 lineStyle;
  look->GetColor(styleIDs->mForeground, foreColor);
  look->GetColor(styleIDs->mBackground, backColor);
  look->GetColor(styleIDs->mLine, lineColor);
  look->GetMetric(styleIDs->mLineStyle, lineStyle);

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

  if (!NS_IS_VALID_UNDERLINE_STYLE(lineStyle))
    lineStyle = NS_UNDERLINE_STYLE_SOLID;

  IMEStyle->mTextColor       = foreColor;
  IMEStyle->mBGColor         = backColor;
  IMEStyle->mUnderlineColor  = lineColor;
  IMEStyle->mUnderlineStyle  = sUnderlineStyles[lineStyle];
  IMEStyle->mInit            = PR_TRUE;

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
NS_IMETHODIMP nsTextFrame::GetAccessible(nsIAccessible** aAccessible)
{
  if (!IsEmpty() || GetNextInFlow()) {

    nsCOMPtr<nsIAccessibilityService> accService = do_GetService("@mozilla.org/accessibilityService;1");

    if (accService) {
      return accService->CreateHTMLTextAccessible(static_cast<nsIFrame*>(this), aAccessible);
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
  // We're not a continuing frame.
  // mContentOffset = 0; not necessary since we get zeroed out at init
  return nsFrame::Init(aContent, aParent, aPrevInFlow);
}

void
nsTextFrame::Destroy()
{
  ClearTextRun();
  if (mNextContinuation) {
    mNextContinuation->SetPrevInFlow(nsnull);
  }
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
  
  virtual nsresult GetRenderedText(nsAString* aString = nsnull,
                                   gfxSkipChars* aSkipChars = nsnull,
                                   gfxSkipCharsIterator* aSkipIter = nsnull,
                                   PRUint32 aSkippedStartOffset = 0,
                                   PRUint32 aSkippedMaxLength = PR_UINT32_MAX)
  { return NS_ERROR_NOT_IMPLEMENTED; } // Call on a primary text frame only

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
  nsTextFrame* prev = static_cast<nsTextFrame*>(aPrevInFlow);
  mContentOffset = prev->GetContentOffset() + prev->GetContentLengthHint();
  NS_ASSERTION(mContentOffset < aContent->GetText()->GetLength(),
               "Creating ContinuingTextFrame, but there is no more content");
  if (prev->GetStyleContext() != GetStyleContext()) {
    // We're taking part of prev's text, and its style may be different
    // so clear its textrun which may no longer be valid (and don't set ours)
    prev->ClearTextRun();
  } else {
    mTextRun = prev->GetTextRun();
  }
#ifdef IBMBIDI
  if (aPrevInFlow->GetStateBits() & NS_FRAME_IS_BIDI) {
    PRInt32 start, end;
    aPrevInFlow->GetOffsets(start, mContentOffset);

    nsPropertyTable *propTable = PresContext()->PropertyTable();
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
    }
    mState |= NS_FRAME_IS_BIDI;
  } // prev frame is bidi
#endif // IBMBIDI

  return rv;
}

void
nsContinuingTextFrame::Destroy()
{
  ClearTextRun();
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
           *previous = const_cast<nsIFrame*>
                                 (static_cast<const nsIFrame*>(this));
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
  *previous = const_cast<nsIFrame*>
                        (static_cast<const nsIFrame*>(mPrevContinuation));
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
  nsTextFrame* lastInFlow = const_cast<nsTextFrame*>(this);
  while (lastInFlow->GetNextInFlow())  {
    lastInFlow = static_cast<nsTextFrame*>(lastInFlow->GetNextInFlow());
  }
  NS_POSTCONDITION(lastInFlow, "illegal state in flow chain.");
  return lastInFlow;
}
nsIFrame*
nsTextFrame::GetLastContinuation() const
{
  nsTextFrame* lastInFlow = const_cast<nsTextFrame*>(this);
  while (lastInFlow->mNextContinuation)  {
    lastInFlow = static_cast<nsTextFrame*>(lastInFlow->mNextContinuation);
  }
  NS_POSTCONDITION(lastInFlow, "illegal state in continuation chain.");
  return lastInFlow;
}

void
nsTextFrame::ClearTextRun()
{
  // save textrun because ClearAllTextRunReferences will clear ours
  gfxTextRun* textRun = mTextRun;
  
  if (!textRun)
    return;

  UnhookTextRunFromFrames(textRun);
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

  if (!(textRun->GetFlags() & gfxTextRunWordCache::TEXT_IN_CACHE)) {
    // Remove it now because it's not doing anything useful
    gTextRuns->RemoveFromCache(textRun);
    delete textRun;
  }
}

static void
ClearTextRunsInFlowChain(nsTextFrame* aFrame)
{
  nsTextFrame* f;
  for (f = aFrame; f; f = static_cast<nsTextFrame*>(f->GetNextInFlow())) {
    f->ClearTextRun();
  }
}

NS_IMETHODIMP
nsTextFrame::CharacterDataChanged(nsPresContext* aPresContext,
                                  nsIContent*     aChild,
                                  PRBool          aAppend)
{
  ClearTextRunsInFlowChain(this);

  nsTextFrame* targetTextFrame;
  PRInt32 nodeLength = mContent->GetText()->GetLength();

  if (aAppend) {
    targetTextFrame = static_cast<nsTextFrame*>(GetLastContinuation());
    targetTextFrame->mState &= ~TEXT_WHITESPACE_FLAGS;
  } else {
    // Mark all the continuation frames as dirty, and fix up content offsets to
    // be valid.
    // Don't set NS_FRAME_IS_DIRTY on |this|, since we call FrameNeedsReflow
    // below.
    nsTextFrame* textFrame = this;
    PRInt32 newLength = nodeLength;
    do {
      textFrame->mState &= ~TEXT_WHITESPACE_FLAGS;
      // If the text node has shrunk, clip the frame contentlength as necessary
      if (textFrame->mContentOffset > newLength) {
        textFrame->mContentOffset = newLength;
      }
      textFrame = static_cast<nsTextFrame*>(textFrame->GetNextContinuation());
      if (!textFrame) {
        break;
      }
      textFrame->mState |= NS_FRAME_IS_DIRTY;
    } while (1);
    targetTextFrame = this;
  }

  // Ask the parent frame to reflow me.
  aPresContext->GetPresShell()->FrameNeedsReflow(targetTextFrame,
                                                 nsIPresShell::eStyleChange,
                                                 NS_FRAME_IS_DIRTY);

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
  virtual nsIFrame* HitTest(nsDisplayListBuilder* aBuilder, nsPoint aPt) {
    return nsRect(aBuilder->ToReferenceFrame(mFrame), mFrame->GetSize()).Contains(aPt) ? mFrame : nsnull;
  }
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx,
     const nsRect& aDirtyRect);
  NS_DISPLAY_DECL_NAME("Text")
};

void
nsDisplayText::Paint(nsDisplayListBuilder* aBuilder,
     nsIRenderingContext* aCtx, const nsRect& aDirtyRect) {
  static_cast<nsTextFrame*>(mFrame)->
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
  const nsFrameSelection* frameSelection = GetConstFrameSelection();
  if (!(GetStateBits() & NS_FRAME_GENERATED_CONTENT)) {
    SelectionDetails* details =
      frameSelection->LookUpSelection(mContent, GetContentOffset(),
                                      GetContentLength(), PR_FALSE);
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
    frameSelection->LookUpSelection(owner->GetContent(),
        isBefore ? 0 : owner->GetContent()->GetChildCount(), 0, PR_FALSE);
  SelectionDetails* sd;
  for (sd = details; sd; sd = sd->mNext) {
    // The entire text is selected!
    sd->mStart = GetContentOffset();
    sd->mEnd = GetContentEnd();
  }
  return details;
}

static void
FillClippedRect(gfxContext* aCtx, nsPresContext* aPresContext,
                nscolor aColor, const gfxRect& aDirtyRect, const gfxRect& aRect)
{
  gfxRect r = aRect.Intersect(aDirtyRect);
  // For now, we need to put this in pixel coordinates
  PRInt32 app = aPresContext->AppUnitsPerDevPixel();
  aCtx->NewPath();
  // pixel-snap
  aCtx->Rectangle(gfxRect(r.X() / app, r.Y() / app,
                          r.Width() / app, r.Height() / app), PR_TRUE);
  aCtx->SetColor(gfxRGBA(aColor));
  aCtx->Fill();
}

void 
nsTextFrame::PaintTextDecorations(gfxContext* aCtx, const gfxRect& aDirtyRect,
                                  const gfxPoint& aFramePt,
                                  const gfxPoint& aTextBaselinePt,
                                  nsTextPaintStyle& aTextPaintStyle,
                                  PropertyProvider& aProvider)
{
  // Quirks mode text decoration are rendered by children; see bug 1777
  // In non-quirks mode, nsHTMLContainer::Paint and nsBlockFrame::Paint
  // does the painting of text decorations.
  if (eCompatibility_NavQuirks != aTextPaintStyle.PresContext()->CompatibilityMode())
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
  gfxFloat app = aTextPaintStyle.PresContext()->AppUnitsPerDevPixel();

  // XXX aFramePt is in AppUnits, shouldn't it be nsFloatPoint?
  gfxPoint pt(aFramePt.x / app, (aTextBaselinePt.y - mAscent) / app);
  gfxSize size(GetRect().width / app, 0);
  gfxFloat ascent = gfxFloat(mAscent) / app;

  if (decorations & NS_FONT_DECORATION_OVERLINE) {
    size.height = fontMetrics.underlineSize;
    nsCSSRendering::PaintDecorationLine(
      aCtx, overColor, pt, size, ascent, ascent, size.height,
      NS_STYLE_TEXT_DECORATION_OVERLINE, NS_STYLE_BORDER_STYLE_SOLID,
      mTextRun->IsRightToLeft());
  }
  if (decorations & NS_FONT_DECORATION_UNDERLINE) {
    size.height = fontMetrics.underlineSize;
    gfxFloat offset = fontMetrics.underlineOffset;
    nsCSSRendering::PaintDecorationLine(
      aCtx, underColor, pt, size, ascent, offset, size.height,
      NS_STYLE_TEXT_DECORATION_UNDERLINE, NS_STYLE_BORDER_STYLE_SOLID,
      mTextRun->IsRightToLeft());
  }
  if (decorations & NS_FONT_DECORATION_LINE_THROUGH) {
    size.height = fontMetrics.strikeoutSize;
    gfxFloat offset = fontMetrics.strikeoutOffset;
    nsCSSRendering::PaintDecorationLine(
      aCtx, strikeColor, pt, size, ascent, offset, size.height,
      NS_STYLE_TEXT_DECORATION_UNDERLINE, NS_STYLE_BORDER_STYLE_SOLID,
      mTextRun->IsRightToLeft());
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
    nsTextPaintStyle& aTextPaintStyle, const gfxPoint& aPt, gfxFloat aWidth,
    gfxFloat aAscent, gfxFloat aSize, gfxFloat aOffset, PRBool aIsRTL)
{
  nscolor color;
  float relativeSize;
  PRUint8 style;
  if (!aTextPaintStyle.GetIMEUnderline(aIndex, &color, &relativeSize, &style))
    return;

  gfxFloat actualSize = relativeSize * aSize;
  gfxFloat width = PR_MAX(0, aWidth - 2.0 * aSize);
  gfxPoint pt(aPt.x + 1.0, aPt.y);
  nsCSSRendering::PaintDecorationLine(
    aContext, color, pt, gfxSize(width, actualSize), aAscent, aOffset, aSize,
    NS_STYLE_TEXT_DECORATION_UNDERLINE, style, aIsRTL);
}

/**
 * This, plus SelectionTypesWithDecorations, encapsulates all knowledge about
 * drawing text decoration for selections.
 */
static void DrawSelectionDecorations(gfxContext* aContext, SelectionType aType,
    nsTextPaintStyle& aTextPaintStyle, const gfxPoint& aPt, gfxFloat aWidth,
    gfxFloat aAscent, const gfxFont::Metrics& aFontMetrics, PRBool aIsRTL)
{
  gfxSize size(aWidth, aFontMetrics.underlineSize);

  switch (aType) {
    case nsISelectionController::SELECTION_SPELLCHECK: {
      nsCSSRendering::PaintDecorationLine(
        aContext, NS_RGB(255,0,0),
        aPt, size, aAscent, aFontMetrics.underlineOffset, size.height,
        NS_STYLE_TEXT_DECORATION_UNDERLINE, NS_STYLE_BORDER_STYLE_DOTTED,
        aIsRTL);
      break;
    }

    case nsISelectionController::SELECTION_IME_RAWINPUT:
      DrawIMEUnderline(aContext, nsTextPaintStyle::eIndexRawInput,
                       aTextPaintStyle, aPt, aWidth, aAscent, size.height,
                       aFontMetrics.underlineOffset, aIsRTL);
      break;
    case nsISelectionController::SELECTION_IME_SELECTEDRAWTEXT:
      DrawIMEUnderline(aContext, nsTextPaintStyle::eIndexSelRawText,
                       aTextPaintStyle, aPt, aWidth, aAscent, size.height,
                       aFontMetrics.underlineOffset, aIsRTL);
      break;
    case nsISelectionController::SELECTION_IME_CONVERTEDTEXT:
      DrawIMEUnderline(aContext, nsTextPaintStyle::eIndexConvText,
                       aTextPaintStyle, aPt, aWidth, aAscent, size.height,
                       aFontMetrics.underlineOffset, aIsRTL);
      break;
    case nsISelectionController::SELECTION_IME_SELECTEDCONVERTEDTEXT:
      DrawIMEUnderline(aContext, nsTextPaintStyle::eIndexSelConvText,
                       aTextPaintStyle, aPt, aWidth, aAscent, size.height,
                       aFontMetrics.underlineOffset, aIsRTL);
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
  if (mIterator.GetOriginalOffset() >= mOriginalEnd)
    return PR_FALSE;
  
  // save offset into transformed string now
  PRUint32 runOffset = mIterator.GetSkippedOffset();
  
  PRInt32 index = mIterator.GetOriginalOffset() - mOriginalStart;
  SelectionType type = mSelectionBuffer[index];
  for (++index; mOriginalStart + index < mOriginalEnd; ++index) {
    if (mSelectionBuffer[index] != type)
      break;
  }
  mIterator.SetOriginalOffset(index + mOriginalStart);
  
  // Advance to the next cluster boundary
  while (mIterator.GetOriginalOffset() < mOriginalEnd &&
         !mIterator.IsOriginalCharSkipped() &&
         !mTextRun->IsClusterStart(mIterator.GetSkippedOffset())) {
    mIterator.AdvanceOriginal(1);
  }

  PRBool haveHyphenBreak =
    (mProvider.GetFrame()->GetStateBits() & TEXT_HYPHEN_BREAK) != 0;
  *aOffset = runOffset;
  *aLength = mIterator.GetSkippedOffset() - runOffset;
  *aXOffset = mXOffset;
  *aHyphenWidth = 0;
  if (mIterator.GetOriginalOffset() == mOriginalEnd && haveHyphenBreak) {
    *aHyphenWidth = mProvider.GetHyphenWidth();
  }
  *aType = type;
  return PR_TRUE;
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
        FillClippedRect(aCtx, aTextPaintStyle.PresContext(),
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
      // Get a reference rendering context because aCtx might not have the
      // reference matrix currently set
      gfxTextRunCache::AutoTextRun hyphenTextRun(GetHyphenTextRun(mTextRun, nsnull, this));
      if (hyphenTextRun.get()) {
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
  PRInt32 app = aTextPaintStyle.PresContext()->AppUnitsPerDevPixel();
  // XXX aTextBaselinePt is in AppUnits, shouldn't it be nsFloatPoint?
  gfxPoint pt(0.0, (aTextBaselinePt.y - mAscent) / app);
  SelectionType type;
  while (iterator.GetNextSegment(&xOffset, &offset, &length, &hyphenWidth, &type)) {
    gfxFloat advance = hyphenWidth +
      mTextRun->GetAdvanceWidth(offset, length, &aProvider);
    if (type == aSelectionType) {
      pt.x = (aTextBaselinePt.x + xOffset) / app;
      gfxFloat width = PR_ABS(advance) / app;
      DrawSelectionDecorations(aCtx, aSelectionType, aTextPaintStyle,
                               pt, width, mAscent / app, decorationMetrics,
                               mTextRun->IsRightToLeft());
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
  PaintTextDecorations(aCtx, aDirtyRect, aFramePt, aTextBaselinePt,
                       aTextPaintStyle, aProvider);
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

gfxFloat
nsTextFrame::GetSnappedBaselineY(gfxContext* aContext, gfxFloat aY)
{
  gfxFloat appUnitsPerDevUnit = mTextRun->GetAppUnitsPerDevUnit();
  gfxFloat baseline = aY + mAscent;
  gfxRect putativeRect(0, baseline/appUnitsPerDevUnit, 1, 1);
  if (!aContext->UserToDevicePixelSnapped(putativeRect))
    return baseline;
  return aContext->DeviceToUser(putativeRect.pos).y*appUnitsPerDevUnit;
}

void
nsTextFrame::PaintText(nsIRenderingContext* aRenderingContext, nsPoint aPt,
                       const nsRect& aDirtyRect)
{
  // Don't pass in aRenderingContext here, because we need a *reference*
  // context and aRenderingContext might have some transform in it
  // XXX get the block and line passed to us somehow! This is slow!
  gfxSkipCharsIterator iter = EnsureTextRun();
  if (!mTextRun)
    return;

  nsTextPaintStyle textPaintStyle(this);
  PropertyProvider provider(this, iter);
  // Trim trailing whitespace
  provider.InitializeForDisplay(PR_TRUE);

  gfxContext* ctx = aRenderingContext->ThebesContext();

  gfxPoint framePt(aPt.x, aPt.y);
  gfxPoint textBaselinePt(
      mTextRun->IsRightToLeft() ? gfxFloat(aPt.x + GetSize().width) : framePt.x,
      GetSnappedBaselineY(ctx, aPt.y));

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
    // Don't use ctx as the context, because we need a reference context here,
    // ctx may be transformed.
    gfxTextRunCache::AutoTextRun hyphenTextRun(GetHyphenTextRun(mTextRun, nsnull, this));
    if (hyphenTextRun.get()) {
      hyphenTextRun->Draw(ctx, gfxPoint(hyphenBaselineX, textBaselinePt.y),
                          0, hyphenTextRun->GetLength(), &dirtyRect, nsnull, nsnull);
    }
  }
  PaintTextDecorations(ctx, dirtyRect, framePt, textBaselinePt,
                       textPaintStyle, provider);
}

PRInt16
nsTextFrame::GetSelectionStatus(PRInt16* aSelectionFlags)
{
  // get the selection controller
  nsCOMPtr<nsISelectionController> selectionController;
  nsresult rv = GetSelectionController(PresContext(),
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
    if (sdptr->mEnd > GetContentOffset() &&
        sdptr->mStart < GetContentEnd() &&
        sdptr->mType == nsISelectionController::SELECTION_NORMAL) {
      found = PR_TRUE;
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
static PRUint32
CountCharsFit(gfxTextRun* aTextRun, PRUint32 aStart, PRUint32 aLength,
              gfxFloat aWidth, PropertyProvider* aProvider,
              gfxFloat* aFitWidth)
{
  PRUint32 last = 0;
  gfxFloat width = 0;
  PRUint32 i;
  for (i = 1; i <= aLength; ++i) {
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
      if (GetContentEnd() >= startOffset)
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
nsTextFrame::GetPointFromOffset(PRInt32 inOffset,
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

  gfxSkipCharsIterator iter = EnsureTextRun();
  if (!mTextRun)
    return NS_ERROR_FAILURE;

  PropertyProvider properties(this, iter);
  // Don't trim trailing whitespace, we want the caret to appear in the right
  // place if it's positioned there
  properties.InitializeForDisplay(PR_FALSE);  

  if (inOffset < GetContentOffset()){
    NS_WARNING("offset before this frame's content");
    inOffset = GetContentOffset();
  } else if (inOffset > GetContentEnd()) {
    NS_WARNING("offset after this frame's content");
    inOffset = GetContentEnd();
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
nsTextFrame::GetChildFrameContainingOffset(PRInt32   aContentOffset,
                                           PRBool    aHint,
                                           PRInt32*  aOutOffset,
                                           nsIFrame**aOutFrame)
{
  DEBUG_VERIFY_NOT_DIRTY(mState);
#if 0 //XXXrbs disable due to bug 310227
  if (mState & NS_FRAME_IS_DIRTY)
    return NS_ERROR_UNEXPECTED;
#endif

  NS_ASSERTION(aOutOffset && aOutFrame, "Bad out parameters");
  NS_ASSERTION(aContentOffset >= 0, "Negative content offset, existing code was very broken!");

  nsTextFrame* f = this;
  if (aContentOffset >= mContentOffset) {
    while (PR_TRUE) {
      nsTextFrame* next = static_cast<nsTextFrame*>(f->GetNextContinuation());
      if (!next || aContentOffset < next->GetContentOffset())
        break;
      if (aContentOffset == next->GetContentOffset()) {
        if (aHint) {
          f = next;
        }
        break;
      }
      f = next;
    }
  } else {
    while (PR_TRUE) {
      nsTextFrame* prev = static_cast<nsTextFrame*>(f->GetPrevContinuation());
      if (!prev || aContentOffset > f->GetContentOffset())
        break;
      if (aContentOffset == f->GetContentOffset()) {
        if (!aHint) {
          f = prev;
        }
        break;
      }
      f = prev;
    }
  }
  
  *aOutOffset = aContentOffset - f->GetContentOffset();
  *aOutFrame = f;
  return NS_OK;
}

PRBool
nsTextFrame::PeekOffsetNoAmount(PRBool aForward, PRInt32* aOffset)
{
  NS_ASSERTION(aOffset && *aOffset <= GetContentLength(), "aOffset out of range");

  gfxSkipCharsIterator iter = EnsureTextRun();
  if (!mTextRun)
    return PR_FALSE;

  TrimmedOffsets trimmed = GetTrimmedOffsets(mContent->GetText(), PR_TRUE);
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
class ClusterIterator {
public:
  ClusterIterator(nsTextFrame* aTextFrame, PRInt32 aPosition, PRInt32 aDirection,
                  nsString& aContext);

  PRBool NextCluster();
  PRBool IsWhitespace();
  PRBool IsPunctuation();
  PRBool HaveWordBreakBefore() { return mHaveWordBreak; }
  PRInt32 GetAfterOffset();
  PRInt32 GetBeforeOffset();

private:
  nsCOMPtr<nsIUGenCategory>   mCategories;
  gfxSkipCharsIterator        mIterator;
  const nsTextFragment*       mFrag;
  nsTextFrame*                mTextFrame;
  PRInt32                     mDirection;
  PRInt32                     mCharIndex;
  nsTextFrame::TrimmedOffsets mTrimmed;
  nsTArray<PRPackedBool>      mWordBreaks;
  PRPackedBool                mHaveWordBreak;
};

static PRBool
IsAcceptableCaretPosition(const gfxSkipCharsIterator& aIter, gfxTextRun* aTextRun,
                          nsIFrame* aFrame)
{
  if (aIter.IsOriginalCharSkipped())
    return PR_FALSE;
  PRUint32 index = aIter.GetSkippedOffset();
  if (!aTextRun->IsClusterStart(index))
    return PR_FALSE;
  return !(aFrame->GetStyleText()->WhiteSpaceIsSignificant() &&
           aTextRun->GetChar(index) == '\n');
}

PRBool
nsTextFrame::PeekOffsetCharacter(PRBool aForward, PRInt32* aOffset)
{
  PRInt32 contentLength = GetContentLength();
  NS_ASSERTION(aOffset && *aOffset <= contentLength, "aOffset out of range");

  PRBool selectable;
  PRUint8 selectStyle;  
  IsSelectable(&selectable, &selectStyle);
  if (selectStyle == NS_STYLE_USER_SELECT_ALL)
    return PR_FALSE;

  gfxSkipCharsIterator iter = EnsureTextRun();
  if (!mTextRun)
    return PR_FALSE;

  TrimmedOffsets trimmed = GetTrimmedOffsets(mContent->GetText(), PR_FALSE);

  // A negative offset means "end of frame".
  PRInt32 startOffset = GetContentOffset() + (*aOffset < 0 ? contentLength : *aOffset);

  if (!aForward) {
    PRInt32 i;
    for (i = PR_MIN(trimmed.GetEnd(), startOffset) - 1;
         i >= trimmed.mStart; --i) {
      iter.SetOriginalOffset(i);
      if (IsAcceptableCaretPosition(iter, mTextRun, this)) {
        *aOffset = i - mContentOffset;
        return PR_TRUE;
      }
    }
    *aOffset = 0;
  } else {
    PRInt32 i;
    for (i = startOffset + 1; i <= trimmed.GetEnd(); ++i) {
      iter.SetOriginalOffset(i);
      // XXX we can't necessarily stop at the end of this frame,
      // but we really have no choice right now. We need to do a deeper
      // fix/restructuring of PeekOffsetCharacter
      if (i == trimmed.GetEnd() ||
          IsAcceptableCaretPosition(iter, mTextRun, this)) {
        *aOffset = i - mContentOffset;
        return PR_TRUE;
      }
    }
    *aOffset = contentLength;
  }
  
  return PR_FALSE;
}

PRBool
ClusterIterator::IsWhitespace()
{
  NS_ASSERTION(mCharIndex >= 0, "No cluster selected");
  return IsSelectionSpace(mFrag, mCharIndex);
}

PRBool
ClusterIterator::IsPunctuation()
{
  NS_ASSERTION(mCharIndex >= 0, "No cluster selected");
  if (!mCategories)
    return PR_FALSE;
  nsIUGenCategory::nsUGenCategory c = mCategories->Get(mFrag->CharAt(mCharIndex));
  return c == nsIUGenCategory::kPunctuation || c == nsIUGenCategory::kSymbol;
}

PRInt32
ClusterIterator::GetBeforeOffset()
{
  NS_ASSERTION(mCharIndex >= 0, "No cluster selected");
  return mCharIndex + (mDirection > 0 ? 0 : 1);
}

PRInt32
ClusterIterator::GetAfterOffset()
{
  NS_ASSERTION(mCharIndex >= 0, "No cluster selected");
  return mCharIndex + (mDirection > 0 ? 1 : 0);
}

PRBool
ClusterIterator::NextCluster()
{
  if (!mDirection)
    return PR_FALSE;
  gfxTextRun* textRun = mTextFrame->GetTextRun();

  mHaveWordBreak = PR_FALSE;
  while (PR_TRUE) {
    PRBool keepGoing = PR_FALSE;
    if (mDirection > 0) {
      if (mIterator.GetOriginalOffset() >= mTrimmed.GetEnd())
        return PR_FALSE;
      keepGoing = mIterator.IsOriginalCharSkipped() ||
          mIterator.GetOriginalOffset() < mTrimmed.mStart ||
          !textRun->IsClusterStart(mIterator.GetSkippedOffset());
      mCharIndex = mIterator.GetOriginalOffset();
      mIterator.AdvanceOriginal(1);
    } else {
      if (mIterator.GetOriginalOffset() <= mTrimmed.mStart)
        return PR_FALSE;
      mIterator.AdvanceOriginal(-1);
      keepGoing = mIterator.IsOriginalCharSkipped() ||
          mIterator.GetOriginalOffset() >= mTrimmed.GetEnd() ||
          !textRun->IsClusterStart(mIterator.GetSkippedOffset());
      mCharIndex = mIterator.GetOriginalOffset();
    }

    if (mWordBreaks[GetBeforeOffset() - mTextFrame->GetContentOffset()]) {
      mHaveWordBreak = PR_TRUE;
    }
    if (!keepGoing)
      return PR_TRUE;
  }
}

ClusterIterator::ClusterIterator(nsTextFrame* aTextFrame, PRInt32 aPosition,
                                 PRInt32 aDirection, nsString& aContext)
  : mTextFrame(aTextFrame), mDirection(aDirection), mCharIndex(-1)
{
  mIterator = aTextFrame->EnsureTextRun();
  if (!aTextFrame->GetTextRun()) {
    mDirection = 0; // signal failure
    return;
  }
  mIterator.SetOriginalOffset(aPosition);

  mCategories = do_GetService(NS_UNICHARCATEGORY_CONTRACTID);
  
  mFrag = aTextFrame->GetContent()->GetText();
  mTrimmed = aTextFrame->GetTrimmedOffsets(mFrag, PR_TRUE);

  PRInt32 textOffset = aTextFrame->GetContentOffset();
  PRInt32 textLen = aTextFrame->GetContentLength();
  if (!mWordBreaks.AppendElements(textLen + 1)) {
    mDirection = 0; // signal failure
    return;
  }
  memset(mWordBreaks.Elements(), PR_FALSE, textLen + 1);
  PRInt32 textStart;
  if (aDirection > 0) {
    if (aContext.IsEmpty()) {
      // No previous context, so it must be the start of a line or text run
      mWordBreaks[0] = PR_TRUE;
    }
    textStart = aContext.Length();
    mFrag->AppendTo(aContext, textOffset, textLen);
  } else {
    if (aContext.IsEmpty()) {
      // No following context, so it must be the end of a line or text run
      mWordBreaks[textLen] = PR_TRUE;
    }
    textStart = 0;
    nsAutoString str;
    mFrag->AppendTo(str, textOffset, textLen);
    aContext.Insert(str, 0);
  }
  nsIWordBreaker* wordBreaker = nsContentUtils::WordBreaker();
  PRInt32 i;
  for (i = 0; i <= textLen; ++i) {
    PRInt32 indexInText = i + textStart;
    mWordBreaks[i] |=
      wordBreaker->BreakInBetween(aContext.get(), indexInText,
                                  aContext.get() + indexInText,
                                  aContext.Length() - indexInText);
  }
}

PRBool
nsTextFrame::PeekOffsetWord(PRBool aForward, PRBool aWordSelectEatSpace, PRBool aIsKeyboardSelect,
                            PRInt32* aOffset, PeekWordState* aState)
{
  PRInt32 contentLength = GetContentLength();
  NS_ASSERTION (aOffset && *aOffset <= contentLength, "aOffset out of range");

  PRBool selectable;
  PRUint8 selectStyle;
  IsSelectable(&selectable, &selectStyle);
  if (selectStyle == NS_STYLE_USER_SELECT_ALL)
    return PR_FALSE;

  PRInt32 offset = GetContentOffset() + (*aOffset < 0 ? contentLength : *aOffset);
  ClusterIterator cIter(this, offset, aForward ? 1 : -1, aState->mContext);

  if (!cIter.NextCluster())
    return PR_FALSE;
  
  do {
    PRBool isPunctuation = cIter.IsPunctuation();
    if (aWordSelectEatSpace == cIter.IsWhitespace() && !aState->mSawBeforeType) {
      aState->SetSawBeforeType();
      aState->Update(isPunctuation);
      continue;
    }
    // See if we can break before the current cluster
    if (!aState->mAtStart) {
      PRBool canBreak = isPunctuation != aState->mLastCharWasPunctuation
        ? BreakWordBetweenPunctuation(aForward ? aState->mLastCharWasPunctuation : isPunctuation,
                                      aIsKeyboardSelect)
        : cIter.HaveWordBreakBefore() && aState->mSawBeforeType;
      if (canBreak) {
        *aOffset = cIter.GetBeforeOffset() - mContentOffset;
        return PR_TRUE;
      }
    }
    aState->Update(isPunctuation);
  } while (cIter.NextCluster());

  *aOffset = cIter.GetAfterOffset() - mContentOffset;
  return PR_FALSE;
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
       f = static_cast<nsTextFrame*>(GetNextContinuation())) {
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
  start = GetContentOffset();
  end = GetContentEnd();
  return NS_OK;
}

/**
 * Returns PR_TRUE if this text frame completes the first-letter, PR_FALSE
 * if it does not contain a true "letter".
 * If returns PR_TRUE, then it also updates aLength to cover just the first-letter
 * text.
 *
 * XXX :first-letter should be handled during frame construction
 * (and it has a good bit in common with nextBidi)
 * 
 * @param aLength an in/out parameter: on entry contains the maximum length to
 * return, on exit returns length of the first-letter fragment (which may
 * include leading punctuation, for example)
 */
static PRBool
FindFirstLetterRange(const nsTextFragment* aFrag,
                     gfxTextRun* aTextRun,
                     PRInt32 aOffset, const gfxSkipCharsIterator& aIter,
                     PRInt32* aLength)
{
  // Find first non-whitespace, non-punctuation cluster, and stop after it
  PRInt32 i;
  PRInt32 length = *aLength;
  for (i = 0; i < length; ++i) {
    if (!IsTrimmableSpace(aFrag, aOffset + i) &&
        !nsContentUtils::IsPunctuationMark(aFrag->CharAt(aOffset + i)))
      break;
  }

  if (i == length)
    return PR_FALSE;

  // Advance to the end of the cluster
  gfxSkipCharsIterator iter(aIter);
  PRInt32 nextClusterStart;
  for (nextClusterStart = i + 1; nextClusterStart < length; ++nextClusterStart) {
    iter.SetOriginalOffset(aOffset + nextClusterStart);
    if (iter.IsOriginalCharSkipped() ||
        aTextRun->IsClusterStart(iter.GetSkippedOffset()))
      break;
  }
  *aLength = nextClusterStart;
  return PR_TRUE;
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
           IsTrimmableSpace(aProvider->GetFragment(), aIterator->GetOriginalOffset())) {
      aIterator->AdvanceOriginal(1);
    }
  }
  return aIterator->GetSkippedOffset();
}

/* virtual */ 
void nsTextFrame::MarkIntrinsicWidthsDirty()
{
  ClearTextRun();
  nsFrame::MarkIntrinsicWidthsDirty();
}

// XXX this doesn't handle characters shaped by line endings. We need to
// temporarily override the "current line ending" settings.
void
nsTextFrame::AddInlineMinWidthForFlow(nsIRenderingContext *aRenderingContext,
                                      nsIFrame::InlineMinWidthData *aData)
{
  PRUint32 flowEndInTextRun;
  gfxContext* ctx = aRenderingContext->ThebesContext();
  gfxSkipCharsIterator iter =
    EnsureTextRun(ctx, nsnull, aData->line, &flowEndInTextRun);
  if (!mTextRun)
    return;

  // Pass null for the line container. This will disable tab spacing, but that's
  // OK since we can't really handle tabs for intrinsic sizing anyway.
  const nsTextFragment* frag = mContent->GetText();
  PropertyProvider provider(mTextRun, GetStyleText(), frag, this,
                            iter, PR_INT32_MAX, nsnull, 0);

  PRBool collapseWhitespace = !provider.GetStyleText()->WhiteSpaceIsSignificant();
  PRUint32 start =
    FindStartAfterSkippingWhitespace(&provider, aData, collapseWhitespace,
                                     &iter, flowEndInTextRun);
  if (start >= flowEndInTextRun)
    return;

  // XXX Should we consider hyphenation here?
  for (PRUint32 i = start, wordStart = start; i <= flowEndInTextRun; ++i) {
    PRBool preformattedNewline = PR_FALSE;
    if (i < flowEndInTextRun) {
      // XXXldb Shouldn't we be including the newline as part of the
      // segment that it ends rather than part of the segment that it
      // starts?
      preformattedNewline = !collapseWhitespace && mTextRun->GetChar(i) == '\n';
      if (!mTextRun->CanBreakLineBefore(i) && !preformattedNewline) {
        // we can't break here (and it's not the end of the flow)
        continue;
      }
    }

    if (i > wordStart) {
      nscoord width =
        NSToCoordCeil(mTextRun->GetAdvanceWidth(wordStart, i - wordStart, &provider));
      aData->currentLine += width;
      aData->atStartOfLine = PR_FALSE;

      if (collapseWhitespace) {
        PRUint32 trimStart = GetEndOfTrimmedText(frag, wordStart, i, &iter);
        if (trimStart == start) {
          // This is *all* trimmable whitespace, so whatever trailingWhitespace
          // we saw previously is still trailing...
          aData->trailingWhitespace += width;
        } else {
          // Some non-whitespace so the old trailingWhitespace is no longer trailing
          aData->trailingWhitespace =
            NSToCoordCeil(mTextRun->GetAdvanceWidth(trimStart, i - trimStart, &provider));
        }
      } else {
        aData->trailingWhitespace = 0;
      }
    }

    if (i < flowEndInTextRun ||
        (i == mTextRun->GetLength() &&
         (mTextRun->GetFlags() & nsTextFrameUtils::TEXT_HAS_TRAILING_BREAK))) {
      if (preformattedNewline) {
        aData->ForceBreak(aRenderingContext);
      } else {
        aData->OptionallyBreak(aRenderingContext);
      }
      wordStart = i;
    }
  }

  // Check if we have collapsible whitespace at the end
  aData->skipWhitespace =
    IsTrimmableSpace(provider.GetFragment(), provider.GetStyleText(),
                     iter.ConvertSkippedToOriginal(flowEndInTextRun - 1));
}

// XXX Need to do something here to avoid incremental reflow bugs due to
// first-line and first-letter changing min-width
/* virtual */ void
nsTextFrame::AddInlineMinWidth(nsIRenderingContext *aRenderingContext,
                               nsIFrame::InlineMinWidthData *aData)
{
  nsTextFrame* f;
  gfxTextRun* lastTextRun = nsnull;
  // nsContinuingTextFrame does nothing for AddInlineMinWidth; all text frames
  // in the flow are handled right here.
  for (f = this; f; f = static_cast<nsTextFrame*>(f->GetNextContinuation())) {
    // f->mTextRun could be null if we haven't set up textruns yet for f.
    // Except in OOM situations, lastTextRun will only be null for the first
    // text frame.
    if (f == this || f->mTextRun != lastTextRun) {
      // This will process all the text frames that share the same textrun as f.
      f->AddInlineMinWidthForFlow(aRenderingContext, aData);
      lastTextRun = f->mTextRun;
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
  gfxContext* ctx = aRenderingContext->ThebesContext();
  gfxSkipCharsIterator iter =
    EnsureTextRun(ctx, nsnull, aData->line, &flowEndInTextRun);
  if (!mTextRun)
    return;

  // Pass null for the line container. This will disable tab spacing, but that's
  // OK since we can't really handle tabs for intrinsic sizing anyway.
  PropertyProvider provider(mTextRun, GetStyleText(), mContent->GetText(), this,
                            iter, PR_INT32_MAX, nsnull, 0);

  PRBool collapseWhitespace = !provider.GetStyleText()->WhiteSpaceIsSignificant();
  PRUint32 start =
    FindStartAfterSkippingWhitespace(&provider, aData, collapseWhitespace,
                                     &iter, flowEndInTextRun);
  if (start >= flowEndInTextRun)
    return;

  if (collapseWhitespace) {
    // \n line breaks are not honoured, so everything would like to go
    // onto one line, so just measure it
    nscoord width =
      NSToCoordCeil(mTextRun->GetAdvanceWidth(start, flowEndInTextRun - start, &provider));
    aData->currentLine = NSCoordSaturatingAdd(aData->currentLine, width);

    PRUint32 trimStart = GetEndOfTrimmedText(provider.GetFragment(), start,
                                             flowEndInTextRun, &iter);
    if (trimStart == start) {
      // This is *all* trimmable whitespace, so whatever trailingWhitespace
      // we saw previously is still trailing...
      aData->trailingWhitespace += width;
    } else {
      // Some non-whitespace so the old trailingWhitespace is no longer trailing
      aData->trailingWhitespace =
        NSToCoordCeil(mTextRun->GetAdvanceWidth(trimStart, flowEndInTextRun - trimStart, &provider));
    }
  } else {
    // We respect line breaks, so measure off each line (or part of line).
    aData->trailingWhitespace = 0;
    PRUint32 i;
    PRUint32 startRun = start;
    for (i = start; i <= flowEndInTextRun; ++i) {
      if (i < flowEndInTextRun && mTextRun->GetChar(i) != '\n')
        continue;
        
      aData->currentLine +=
        NSToCoordCeil(mTextRun->GetAdvanceWidth(startRun, i - startRun, &provider));
      if (i < flowEndInTextRun) {
        aData->ForceBreak(aRenderingContext);
        startRun = i;
      }
    }
  }

  // Check if we have collapsible whitespace at the end
  aData->skipWhitespace =
    IsTrimmableSpace(provider.GetFragment(), provider.GetStyleText(),
                     iter.ConvertSkippedToOriginal(flowEndInTextRun - 1));
}

// XXX Need to do something here to avoid incremental reflow bugs due to
// first-line and first-letter changing pref-width
/* virtual */ void
nsTextFrame::AddInlinePrefWidth(nsIRenderingContext *aRenderingContext,
                                nsIFrame::InlinePrefWidthData *aData)
{
  nsTextFrame* f;
  gfxTextRun* lastTextRun = nsnull;
  // nsContinuingTextFrame does nothing for AddInlineMinWidth; all text frames
  // in the flow are handled right here.
  for (f = this; f; f = static_cast<nsTextFrame*>(f->GetNextContinuation())) {
    // f->mTextRun could be null if we haven't set up textruns yet for f.
    // Except in OOM situations, lastTextRun will only be null for the first
    // text frame.
    if (f == this || f->mTextRun != lastTextRun) {
      // This will process all the text frames that share the same textrun as f.
      f->AddInlinePrefWidthForFlow(aRenderingContext, aData);
      lastTextRun = f->mTextRun;
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
  if ((GetStyleContext()->HasTextDecorations() &&
       eCompatibility_NavQuirks == PresContext()->CompatibilityMode()) ||
      (GetStateBits() & TEXT_HYPHEN_BREAK)) {
    // This is conservative, but OK.
    return GetOverflowRect();
  }

  gfxSkipCharsIterator iter = const_cast<nsTextFrame*>(this)->EnsureTextRun();
  if (!mTextRun)
    return nsRect(0, 0, 0, 0);

  PropertyProvider provider(const_cast<nsTextFrame*>(this), iter);
  // Trim trailing whitespace
  provider.InitializeForDisplay(PR_TRUE);

  gfxTextRun::Metrics metrics =
        mTextRun->MeasureText(provider.GetStart().GetSkippedOffset(),
                              ComputeTransformedLength(provider), PR_TRUE,
                              aContext, &provider);
  // mAscent should be the same as metrics.mAscent, but it's what we use to
  // paint so that's the one we'll use.
  return RoundOut(metrics.mBoundingBox) + nsPoint(0, mAscent);
}

static void
AddCharToMetrics(gfxTextRun* aCharTextRun, gfxTextRun* aBaseTextRun,
                 gfxTextRun::Metrics* aMetrics, PRBool aTightBoundingBox,
                 gfxContext* aContext)
{
  gfxRect charRect;
  // assume char does not overflow font metrics!!!
  gfxFloat width = aCharTextRun->GetAdvanceWidth(0, aCharTextRun->GetLength(), nsnull);
  if (aTightBoundingBox) {
    gfxTextRun::Metrics charMetrics =
        aCharTextRun->MeasureText(0, aCharTextRun->GetLength(), PR_TRUE, aContext, nsnull);
    charRect = charMetrics.mBoundingBox;
  } else {
    charRect = gfxRect(0, -aMetrics->mAscent, width,
                       aMetrics->mAscent + aMetrics->mDescent);
  }
  if (aBaseTextRun->IsRightToLeft()) {
    // Char comes before text, so the bounding box is moved to the
    // right by aWidth
    aMetrics->mBoundingBox.MoveBy(gfxPoint(width, 0));
  } else {
    // char is moved to the right by mAdvanceWidth
    charRect.MoveBy(gfxPoint(width, 0));
  }
  aMetrics->mBoundingBox = aMetrics->mBoundingBox.Union(charRect);

  aMetrics->mAdvanceWidth += width;
}

static PRBool
HasSoftHyphenBefore(const nsTextFragment* aFrag, gfxTextRun* aTextRun,
                    PRInt32 aStartOffset, const gfxSkipCharsIterator& aIter)
{
  if (!(aTextRun->GetFlags() & nsTextFrameUtils::TEXT_HAS_SHY))
    return PR_FALSE;
  gfxSkipCharsIterator iter = aIter;
  while (iter.GetOriginalOffset() > aStartOffset) {
    iter.AdvanceOriginal(-1);
    if (!iter.IsOriginalCharSkipped())
      break;
    if (aFrag->CharAt(iter.GetOriginalOffset()) == CH_SHY)
      return PR_TRUE;
  }
  return PR_FALSE;
}

void
nsTextFrame::SetLength(PRInt32 aLength)
{
  mContentLengthHint = aLength;
  PRInt32 end = GetContentOffset() + aLength;
  nsTextFrame* f = static_cast<nsTextFrame*>(GetNextInFlow());
  if (!f)
    return;
  if (end < f->mContentOffset) {
    // Our frame is shrinking. Give the text to our next in flow.
    f->mContentOffset = end;
    if (f->GetTextRun() != mTextRun) {
      ClearTextRun();
      f->ClearTextRun();
    }
    return;
  }
  while (f && f->mContentOffset < end) {
    // Our frame is growing. Take text from our in-flow.
    f->mContentOffset = end;
    if (f->GetTextRun() != mTextRun) {
      ClearTextRun();
      f->ClearTextRun();
    }
    f = static_cast<nsTextFrame*>(f->GetNextInFlow());
  }
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

  // Temporarily map all possible content while we construct our new textrun.
  // so that when doing reflow our styles prevail over any part of the
  // textrun we look at. Note that next-in-flows may be mapping the same
  // content; gfxTextRun construction logic will ensure that we take priority.
  PRInt32 maxContentLength = GetInFlowContentLength();

  // XXX If there's no line layout, we shouldn't even have created this
  // frame. This may happen if, for example, this is text inside a table
  // but not inside a cell. For now, just don't reflow. We also don't need to
  // reflow if there is no content.
  if (!aReflowState.mLineLayout || !maxContentLength) {
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

  // Layout dependent styles are a problem because we need to reconstruct
  // the gfxTextRun based on our layout.
  PRBool layoutDependentTextRun =
    lineLayout.GetFirstLetterStyleOK() || lineLayout.GetInFirstLine();
  if (layoutDependentTextRun) {
    SetLength(maxContentLength);
  }

  const nsTextFragment* frag = mContent->GetText();
  // DOM offsets of the text range we need to measure, after trimming
  // whitespace, restricting to first-letter, and restricting preformatted text
  // to nearest newline
  PRInt32 length = maxContentLength;
  PRInt32 offset = GetContentOffset();

  // Restrict preformatted text to the nearest newline
  PRInt32 newLineOffset = -1;
  if (textStyle->WhiteSpaceIsSignificant()) {
    newLineOffset = FindChar(frag, offset, length, '\n');
    if (newLineOffset >= 0) {
      length = newLineOffset + 1 - offset;
      newLineOffset -= mContentOffset;
    }
  } else {
    if (atStartOfLine) {
      // Skip leading whitespace
      PRInt32 whitespaceCount = GetTrimmableWhitespaceCount(frag, offset, length, 1);
      offset += whitespaceCount;
      length -= whitespaceCount;
    }
  }

  PRUint32 flowEndInTextRun;
  nsIFrame* lineContainer = lineLayout.GetLineContainerFrame();
  gfxContext* ctx = aReflowState.rendContext->ThebesContext();
  gfxSkipCharsIterator iter =
    EnsureTextRun(ctx, lineContainer, lineLayout.GetLine(), &flowEndInTextRun);

  PRInt32 skippedRunLength;
  if (mTextRun && mTextRun->GetLength() == iter.GetSkippedOffset() &&
      length > 0 &&
      (!iter.IsOriginalCharSkipped(&skippedRunLength) || skippedRunLength < length)) {
    // The textrun does not map enough text for this frame. This can happen
    // when the textrun was ended in the middle of a text node because a
    // preformatted newline was encountered, and prev-in-flow frames have
    // consumed all the text of the textrun. We need a new textrun.
    ClearTextRun();
    iter = EnsureTextRun(ctx, lineContainer,
                         lineLayout.GetLine(), &flowEndInTextRun);
  }
  
  if (!mTextRun) {
    ClearMetrics(aMetrics);
    aStatus = NS_FRAME_COMPLETE;
    return NS_OK;
  }

  NS_ASSERTION(gfxSkipCharsIterator(iter).ConvertOriginalToSkipped(offset + length)
                    <= mTextRun->GetLength(),
               "Text run does not map enough text for our reflow");

  // Restrict to just the first-letter if necessary
  PRBool completedFirstLetter = PR_FALSE;
  if (lineLayout.GetFirstLetterStyleOK()) {
    AddStateBits(TEXT_FIRST_LETTER);
    completedFirstLetter = FindFirstLetterRange(frag, mTextRun, offset, iter, &length);
  }

  /////////////////////////////////////////////////////////////////////
  // See how much text should belong to this text frame, and measure it
  /////////////////////////////////////////////////////////////////////
  
  iter.SetOriginalOffset(offset);
  nscoord xOffsetForTabs = (mTextRun->GetFlags() & nsTextFrameUtils::TEXT_HAS_TAB) ?
         lineLayout.GetCurrentFrameXDistanceFromBlock() : -1;
  PropertyProvider provider(mTextRun, textStyle, frag, this, iter, length,
      lineContainer, xOffsetForTabs);

  PRUint32 transformedOffset = provider.GetStart().GetSkippedOffset();

  // The metrics for the text go in here
  gfxTextRun::Metrics textMetrics;
  PRBool needTightBoundingBox = (GetStateBits() & TEXT_FIRST_LETTER) != 0;
#ifdef MOZ_MATHML
  NS_ASSERTION(!(NS_REFLOW_CALC_BOUNDING_METRICS & aMetrics.mFlags),
               "We shouldn't be passed NS_REFLOW_CALC_BOUNDING_METRICS anymore");
#endif

  PRInt32 limitLength = length;
  PRInt32 forceBreak = lineLayout.GetForcedBreakPosition(mContent);
  PRBool forceBreakAfter = PR_FALSE;
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
  gfxFloat trimmedWidth = 0;
  gfxFloat availWidth = aReflowState.availableWidth;
  PRBool canTrimTrailingWhitespace = !textStyle->WhiteSpaceIsSignificant();
  PRUint32 transformedCharsFit =
    mTextRun->BreakAndMeasureText(transformedOffset, transformedLength,
                                  (GetStateBits() & TEXT_START_OF_LINE) != 0,
                                  availWidth,
                                  &provider, !lineLayout.LineIsBreakable(),
                                  canTrimTrailingWhitespace ? &trimmedWidth : nsnull,
                                  &textMetrics, needTightBoundingBox, ctx,
                                  &usedHyphenation, &transformedLastBreak);
  // The "end" iterator points to the first character after the string mapped
  // by this frame. Basically, its original-string offset is offset+charsFit
  // after we've computed charsFit.
  gfxSkipCharsIterator end(provider.GetEndHint());
  end.SetSkippedOffset(transformedOffset + transformedCharsFit);
  PRInt32 charsFit = end.GetOriginalOffset() - offset;
  // That might have taken us beyond our assigned content range (because
  // we might have advanced over some skipped chars that extend outside
  // this frame), so get back in.
  PRInt32 lastBreak = -1;
  if (charsFit >= limitLength) {
    charsFit = limitLength;
    if (transformedLastBreak != PR_UINT32_MAX) {
      // lastBreak is needed.
      // This may set lastBreak greater than 'length', but that's OK
      lastBreak = end.ConvertSkippedToOriginal(transformedOffset + transformedLastBreak);
    }
    end.SetOriginalOffset(offset + charsFit);
    // If we were forced to fit, and the break position is after a soft hyphen,
    // note that this is a hyphenation break.
    if (forceBreak >= 0 && HasSoftHyphenBefore(frag, mTextRun, offset, end)) {
      usedHyphenation = PR_TRUE;
    }
  }
  if (usedHyphenation) {
    // Fix up metrics to include hyphen
    gfxTextRunCache::AutoTextRun hyphenTextRun(GetHyphenTextRun(mTextRun, ctx, this));
    if (hyphenTextRun.get()) {
      AddCharToMetrics(hyphenTextRun.get(),
                       mTextRun, &textMetrics, needTightBoundingBox, ctx);
    }
    AddStateBits(TEXT_HYPHEN_BREAK);
  }

  gfxFloat trimmableWidth = 0;
  PRBool brokeText = forceBreak >= 0 || transformedCharsFit < transformedLength;
  if (canTrimTrailingWhitespace) {
    // Optimization: if we trimmed trailing whitespace, and we can be sure
    // this frame will be at the end of the line, then leave it trimmed off.
    // Otherwise we have to undo the trimming, in case we're not at the end of
    // the line. (If we actually do end up at the end of the line, we'll have
    // to trim it off again in TrimTrailingWhiteSpace, and we'd like to avoid
    // having to re-do it.)
    if (brokeText) {
      // We're definitely going to break so our trailing whitespace should
      // definitely be timmed. Record that we've already done it.
      AddStateBits(TEXT_TRIMMED_TRAILING_WHITESPACE);
    } else {
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
    NS_ASSERTION(textMetrics.mAdvanceWidth - trimmableWidth <= aReflowState.availableWidth,
                 "If the text doesn't fit, and we have a break opportunity, why didn't MeasureText use it?");
    lineLayout.NotifyOptionalBreakPosition(mContent, lastBreak, PR_TRUE);
  }

  PRInt32 contentLength = offset + charsFit - GetContentOffset();

  /////////////////////////////////////////////////////////////////////
  // Compute output metrics
  /////////////////////////////////////////////////////////////////////

  // first-letter frames should use the tight bounding box metrics for ascent/descent
  // for good drop-cap effects
  if (GetStateBits() & TEXT_FIRST_LETTER) {
    textMetrics.mAscent = PR_MAX(0, -textMetrics.mBoundingBox.Y());
    textMetrics.mDescent = PR_MAX(0, textMetrics.mBoundingBox.YMost());
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

  /////////////////////////////////////////////////////////////////////
  // Clean up, update state
  /////////////////////////////////////////////////////////////////////

  // If all our characters are discarded or collapsed, then trimmable width
  // from the last textframe should be preserved. Otherwise the trimmable width
  // from this textframe overrides. (Currently in CSS trimmable width can be
  // at most one space so there's no way for trimmable width from a previous
  // frame to accumulate with trimmable width from this frame.)
  if (transformedCharsFit > 0) {
    lineLayout.SetTrimmableWidth(NSToCoordFloor(trimmableWidth));
  }
  if (charsFit > 0 && charsFit == length &&
      HasSoftHyphenBefore(frag, mTextRun, offset, end)) {
    // Record a potential break after final soft hyphen
    lineLayout.NotifyOptionalBreakPosition(mContent, offset + length,
        textMetrics.mAdvanceWidth + provider.GetHyphenWidth() <= availWidth);
  }
  PRBool breakAfter = forceBreakAfter;
  if (!breakAfter && charsFit == length &&
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
      breakAfter = PR_TRUE;
    } else {
      lineLayout.NotifyOptionalBreakPosition(mContent, offset + length, PR_TRUE);
    }
  }
  if (completedFirstLetter) {
    lineLayout.SetFirstLetterStyleOK(PR_FALSE);
  }

  // Compute reflow status
  aStatus = contentLength == maxContentLength
    ? NS_FRAME_COMPLETE : NS_FRAME_NOT_COMPLETE;

  if (charsFit == 0 && length > 0) {
    // Couldn't place any text
    aStatus = NS_INLINE_LINE_BREAK_BEFORE();
  } else if (contentLength > 0 && contentLength - 1 == newLineOffset) {
    // Ends in \n
    aStatus = NS_INLINE_LINE_BREAK_AFTER(aStatus);
    lineLayout.SetLineEndsInBR(PR_TRUE);
  } else if (breakAfter) {
    aStatus = NS_INLINE_LINE_BREAK_AFTER(aStatus);
  }

  // Compute space and letter counts for justification, if required
  if (NS_STYLE_TEXT_ALIGN_JUSTIFY == textStyle->mTextAlign &&
      !textStyle->WhiteSpaceIsSignificant()) {
    // This will include a space for trailing whitespace, if any is present.
    // This is corrected for in nsLineLayout::TrimWhiteSpaceIn.
    PRInt32 numJustifiableCharacters =
      provider.ComputeJustifiableCharacters(offset, charsFit);

    NS_ASSERTION(numJustifiableCharacters <= charsFit,
                 "Bad justifiable character count");
    lineLayout.SetTextJustificationWeights(numJustifiableCharacters,
        charsFit - numJustifiableCharacters);
  }

  SetLength(contentLength);

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

nsTextFrame::TrimOutput
nsTextFrame::TrimTrailingWhiteSpace(nsIRenderingContext* aRC)
{
  TrimOutput result;
  result.mChanged = PR_FALSE;
  result.mLastCharIsJustifiable = PR_FALSE;
  result.mDeltaWidth = 0;

  AddStateBits(TEXT_END_OF_LINE);

  PRInt32 contentLength = GetContentLength();
  if (!contentLength)
    return result;

  gfxContext* ctx = aRC->ThebesContext();
  gfxSkipCharsIterator start = EnsureTextRun(ctx);
  NS_ENSURE_TRUE(mTextRun, result);

  PRUint32 trimmedStart = start.GetSkippedOffset();

  const nsTextFragment* frag = mContent->GetText();
  TrimmedOffsets trimmed = GetTrimmedOffsets(frag, PR_TRUE);
  gfxSkipCharsIterator trimmedEndIter = start;
  const nsStyleText* textStyle = GetStyleText();
  gfxFloat delta = 0;
  PRUint32 trimmedEnd = trimmedEndIter.ConvertOriginalToSkipped(trimmed.GetEnd());
  
  if (GetStateBits() & TEXT_TRIMMED_TRAILING_WHITESPACE) {
    // We pre-trimmed this frame, so the last character is justifiable
    result.mLastCharIsJustifiable = PR_TRUE;
  } else if (trimmed.GetEnd() < GetContentEnd()) {
    gfxSkipCharsIterator end = trimmedEndIter;
    PRUint32 endOffset = end.ConvertOriginalToSkipped(GetContentOffset() + contentLength);
    if (trimmedEnd < endOffset) {
      // We can't be dealing with tabs here ... they wouldn't be trimmed. So it's
      // OK to pass null for the line container.
      PropertyProvider provider(mTextRun, textStyle, frag, this, start, contentLength,
                                nsnull, 0);
      delta = mTextRun->GetAdvanceWidth(trimmedEnd, endOffset - trimmedEnd, &provider);
      // non-compressed whitespace being skipped at end of line -> justifiable
      // XXX should we actually *count* justifiable characters that should be
      // removed from the overall count? I think so...
      result.mLastCharIsJustifiable = PR_TRUE;
      result.mChanged = PR_TRUE;
    }
  }

  if (trimmed.GetEnd() == GetContentEnd() &&
      HasSoftHyphenBefore(frag, mTextRun, trimmed.mStart, trimmedEndIter)) {
    // This is a soft hyphen break.
    // Fix up metrics to include hyphen
    result.mChanged = PR_TRUE;
    gfxTextRunCache::AutoTextRun hyphenTextRun(GetHyphenTextRun(mTextRun, ctx, this));
    if (hyphenTextRun.get()) {
      delta = -hyphenTextRun->GetAdvanceWidth(0, hyphenTextRun->GetLength(), nsnull);
    }
    AddStateBits(TEXT_HYPHEN_BREAK);
  }

  if (!result.mLastCharIsJustifiable &&
      NS_STYLE_TEXT_ALIGN_JUSTIFY == textStyle->mTextAlign) {
    // Check if any character in the last cluster is justifiable
    PropertyProvider provider(mTextRun, textStyle, frag, this, start, contentLength,
                              nsnull, 0);
    PRBool isCJK = IsChineseJapaneseLangGroup(this);
    gfxSkipCharsIterator justificationStart(start), justificationEnd(trimmedEndIter);
    provider.FindJustificationRange(&justificationStart, &justificationEnd);

    PRInt32 i;
    for (i = justificationEnd.GetOriginalOffset(); i < trimmed.GetEnd(); ++i) {
      if (IsJustifiableCharacter(frag, i, isCJK)) {
        result.mLastCharIsJustifiable = PR_TRUE;
      }
    }
  }

  gfxFloat advanceDelta;
  mTextRun->SetLineBreaks(trimmedStart, trimmedEnd - trimmedStart,
                          (GetStateBits() & TEXT_START_OF_LINE) != 0, PR_TRUE,
                          &advanceDelta, ctx);
  if (advanceDelta != 0) {
    result.mChanged = PR_TRUE;
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
  NS_WARN_IF_FALSE(result.mDeltaWidth >= 0 || (GetStateBits() & TEXT_HYPHEN_BREAK),
                   "Negative deltawidth in a non-hyphen case, something odd is happening");

#ifdef NOISY_TRIM
  ListTag(stdout);
  printf(": trim => %d\n", result.mDeltaWidth);
#endif
  return result;
}

nsRect
nsTextFrame::RecomputeOverflowRect()
{
  gfxSkipCharsIterator iter = EnsureTextRun();
  if (!mTextRun)
    return nsRect(nsPoint(0,0), GetSize());

  PropertyProvider provider(this, iter);
  provider.InitializeForDisplay(PR_TRUE);

  gfxTextRun::Metrics textMetrics =
    mTextRun->MeasureText(provider.GetStart().GetSkippedOffset(),
                          ComputeTransformedLength(provider), PR_FALSE, nsnull,
                          &provider);

  nsRect boundingBox =
    ConvertGfxRectOutward(textMetrics.mBoundingBox + gfxPoint(0, textMetrics.mAscent));
  boundingBox.UnionRect(boundingBox,
                        nsRect(nsPoint(0,0), GetSize()));
  return boundingBox;
}

static PRUnichar TransformChar(const nsStyleText* aStyle, gfxTextRun* aTextRun,
                               PRUint32 aSkippedOffset, PRUnichar aChar)
{
  if (aChar == '\n' || aChar == '\r') {
    return aStyle->WhiteSpaceIsSignificant() ? aChar : ' ';
  }
  switch (aStyle->mTextTransform) {
  case NS_STYLE_TEXT_TRANSFORM_LOWERCASE:
    nsContentUtils::GetCaseConv()->ToLower(aChar, &aChar);
    break;
  case NS_STYLE_TEXT_TRANSFORM_UPPERCASE:
    nsContentUtils::GetCaseConv()->ToUpper(aChar, &aChar);
    break;
  case NS_STYLE_TEXT_TRANSFORM_CAPITALIZE:
    if (aTextRun->CanBreakLineBefore(aSkippedOffset)) {
      nsContentUtils::GetCaseConv()->ToTitle(aChar, &aChar);
    }
    break;
  }

  return aChar;
}

nsresult nsTextFrame::GetRenderedText(nsAString* aAppendToString,
                                      gfxSkipChars* aSkipChars,
                                      gfxSkipCharsIterator* aSkipIter,
                                      PRUint32 aSkippedStartOffset,
                                      PRUint32 aSkippedMaxLength)
{
  // The handling of aSkippedStartOffset and aSkippedMaxLength could be more efficient...
  gfxSkipCharsBuilder skipCharsBuilder;
  nsTextFrame* textFrame;
  const nsTextFragment* textFrag = mContent->GetText();
  PRUint32 keptCharsLength = 0;
  PRUint32 validCharsLength = 0;

  // Build skipChars and copy text, for each text frame in this continuation block
  for (textFrame = this; textFrame;
       textFrame = static_cast<nsTextFrame*>(textFrame->GetNextContinuation())) {
    // For each text frame continuation in this block ...

    // Ensure the text run and grab the gfxSkipCharsIterator for it
    gfxSkipCharsIterator iter = textFrame->EnsureTextRun();
    if (!textFrame->mTextRun)
      return NS_ERROR_FAILURE;

    // Skip to the start of the text run, past ignored chars at start of line
    // XXX In the future we may decide to trim extra spaces before a hard line
    // break, in which case we need to accurately detect those sitations and 
    // call GetTrimmedOffsets() with PR_TRUE to trim whitespace at the line's end
    TrimmedOffsets trimmedContentOffsets = textFrame->GetTrimmedOffsets(textFrag, PR_FALSE);
    PRInt32 startOfLineSkipChars = trimmedContentOffsets.mStart - textFrame->mContentOffset;
    if (startOfLineSkipChars > 0) {
      skipCharsBuilder.SkipChars(startOfLineSkipChars);
      iter.SetOriginalOffset(trimmedContentOffsets.mStart);
    }

    // Keep and copy the appropriate chars withing the caller's requested range
    const nsStyleText* textStyle = textFrame->GetStyleText();
    while (iter.GetOriginalOffset() < trimmedContentOffsets.GetEnd() &&
           keptCharsLength < aSkippedMaxLength) {
      // For each original char from content text
      if (iter.IsOriginalCharSkipped() || ++validCharsLength <= aSkippedStartOffset) {
        skipCharsBuilder.SkipChar();
      } else {
        ++keptCharsLength;
        skipCharsBuilder.KeepChar();
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
    aSkipChars->TakeFrom(&skipCharsBuilder); // Copy skipChars into aSkipChars
    if (aSkipIter) {
      // Caller must provide both pointers in order to retrieve a gfxSkipCharsIterator,
      // because the gfxSkipCharsIterator holds a weak pointer to the gfxSkipCars.
      *aSkipIter = gfxSkipCharsIterator(*aSkipChars, GetContentLength());
    }
  }

  return NS_OK;
}

#ifdef DEBUG
// Translate the mapped content into a string that's printable
void
nsTextFrame::ToCString(nsCString& aBuf, PRInt32* aTotalContentLength) const
{
  // Get the frames text content
  const nsTextFragment* frag = mContent->GetText();
  if (!frag) {
    return;
  }

  // Compute the total length of the text content.
  *aTotalContentLength = frag->GetLength();

  PRInt32 contentLength = GetContentLength();
  // Set current fragment and current fragment offset
  if (0 == contentLength) {
    return;
  }
  PRInt32 fragOffset = GetContentOffset();
  PRInt32 n = fragOffset + contentLength;
  while (fragOffset < n) {
    PRUnichar ch = frag->CharAt(fragOffset++);
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
    fprintf(out, " [view=%p]", static_cast<void*>(GetView()));
  }

  PRInt32 totalContentLength;
  nsCAutoString tmp;
  ToCString(tmp, &totalContentLength);

  // Output the first/last content offset and prev/next in flow info
  PRBool isComplete = GetContentEnd() == totalContentLength;
  fprintf(out, "[%d,%d,%c] ", 
          GetContentOffset(), GetContentLength(),
          isComplete ? 'T':'F');
  
  if (nsnull != mNextSibling) {
    fprintf(out, " next=%p", static_cast<void*>(mNextSibling));
  }
  nsIFrame* prevContinuation = GetPrevContinuation();
  if (nsnull != prevContinuation) {
    fprintf(out, " prev-continuation=%p", static_cast<void*>(prevContinuation));
  }
  if (nsnull != mNextContinuation) {
    fprintf(out, " next-continuation=%p", static_cast<void*>(mNextContinuation));
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
  fprintf(out, " [content=%p]", static_cast<void*>(mContent));
  nsRect* overflowArea = const_cast<nsTextFrame*>(this)->GetOverflowAreaProperty(PR_FALSE);
  if (overflowArea) {
    fprintf(out, " [overflow=%d,%d,%d,%d]", overflowArea->x, overflowArea->y,
            overflowArea->width, overflowArea->height);
  }
  fprintf(out, " sc=%p", static_cast<void*>(mStyleContext));
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
  fputs(tmp.get(), out);
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

  /*
   * After Bidi resolution we may need to reassign text runs.
   * This is called during bidi resolution from the block container, so we
   * shouldn't be holding a local reference to a textrun anywhere.
   */
  ClearTextRun();

  nsTextFrame* prev = static_cast<nsTextFrame*>(GetPrevContinuation());
  if (prev) {
    // the bidi resolver can be very evil when columns/pages are involved. Don't
    // let it violate our invariants.
    PRInt32 prevOffset = prev->GetContentOffset();
    aStart = PR_MAX(aStart, prevOffset);
    aEnd = PR_MAX(aEnd, prevOffset);
    prev->ClearTextRun();
  }
  if (mContentOffset != aStart) {
    mContentOffset = aStart;
  }

  SetLength(aEnd - aStart);
}

/**
 * @return PR_TRUE if this text frame ends with a newline character.  It should return
 * PR_FALSE if it is not a text frame.
 */
PRBool
nsTextFrame::HasTerminalNewline() const
{
  return ::HasTerminalNewline(this);
}

PRBool
nsTextFrame::IsAtEndOfLine() const
{
  return (GetStateBits() & TEXT_END_OF_LINE) != 0;
}

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Novell code.
 *
 * The Initial Developer of the Original Code is Novell Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   robert@ocallahan.org
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsTextRunTransformations.h"

#include "nsTextTransformer.h"
#include "nsTextFrameUtils.h"
#include "gfxSkipChars.h"

#include "nsICaseConversion.h"
#include "nsStyleConsts.h"
#include "nsStyleContext.h"
#include "gfxContext.h"

#define SZLIG 0x00DF

// ==============================
// First define some useful textrun wrapper implementations
// ==============================

class TextProviderWrapper : public gfxTextRun::TextProvider {
public:
  virtual void ForceRememberText() {
    NS_ERROR("We should never be asked to recover text, we already forced the textrun to remember it");
  }
};

class ProviderWrapper : public gfxTextRun::PropertyProvider {
public:
  ProviderWrapper(PropertyProvider* aProvider)
    : mInner(aProvider) {}

  virtual void ForceRememberText() {
    NS_ERROR("We should never be asked to recover text, we already forced the textrun to remember it");
  }

  // Simple forwardings
  virtual gfxFloat GetHyphenWidth()
  { return mInner->GetHyphenWidth(); }

protected:
  PropertyProvider* mInner;
};

class WrapperTextRun : public gfxTextRun {
public:
  WrapperTextRun(gfxTextRun* aInner, gfxTextRunFactory::Parameters* aParams)
  // WrapperTextRuns always take Unicode
    : gfxTextRun(aParams), mInner(aInner) {}

protected:
  nsAutoPtr<gfxTextRun> mInner;
};

/**
 * SZLIG is a problem. When uppercased it turns into "SS", yes, two S's.
 * 
 * This adds a special wrinkle because normally DOM text only shrinks when
 * being converted for display. Instead of burdening general DOM<->displaytext
 * string offset conversion with the possibility of SZLIG-driven growth, we put
 * all the pain right here in a special text run that knows where extra characters
 * have been inserted in a string and makes the client believe that they're not
 * there by adjusting character offsets.
 * 
 * In general this class could be used to handle text runs that are derived from
 * some base string by inserting characters.
 */
class nsGermanTextRun : public WrapperTextRun {
public:
  /**
   * The gfxSkipCharsBuilder used here represents the characters that should
   * be removed from the tranformed text in aInnerTextRun to obtain the
   * original string.
   */
  nsGermanTextRun(gfxTextRun* aInnerTextRun, gfxSkipChars* aSkipChars,
                  gfxTextRunFactory::Parameters* aParams)
    : WrapperTextRun(aInnerTextRun, aParams)
  {
    mTransform.TakeFrom(aSkipChars);
  }
  
  class GermanProviderWrapper : public ProviderWrapper {
  public:
    GermanProviderWrapper(PropertyProvider* aProvider, const nsGermanTextRun& aRun)
      : ProviderWrapper(aProvider), mRun(aRun) {}

    virtual void GetHyphenationBreaks(PRUint32 aStart, PRUint32 aLength,
                                      PRPackedBool* aBreakBefore);
    virtual void GetSpacing(PRUint32 aStart, PRUint32 aLength,
                            Spacing* aSpacing);

  protected:
    const nsGermanTextRun& mRun;
  };

  // Fairly simple forwarding when just offsets or a provider have to be translated
  virtual PRUint8 GetCharFlags(PRUint32 aOffset)
  {
    TranslateOffset(&aOffset);
    return mInner->GetCharFlags(aOffset);
  }
  virtual PRUint32 GetLength()
  {
    // mTransform's 'original' characters are actually the *expanded* characters
    return mTransform.GetOriginalCharCount();
  }
  virtual void Draw(gfxContext* aContext, gfxPoint aPt,
                    PRUint32 aStart, PRUint32 aLength,
                    const gfxRect* aDirtyRect,
                    PropertyProvider* aBreakProvider,
                    gfxFloat* aAdvanceWidth)
  {
    GermanProviderWrapper wrapper(aBreakProvider, *this);
    TranslateSubstring(&aStart, &aLength);
    mInner->Draw(aContext, aPt, aStart, aLength, aDirtyRect, &wrapper, aAdvanceWidth);
  }
  virtual void DrawToPath(gfxContext* aContext, gfxPoint aPt,
                          PRUint32 aStart, PRUint32 aLength,
                          PropertyProvider* aBreakProvider,
                          gfxFloat* aAdvanceWidth)
  {
    GermanProviderWrapper wrapper(aBreakProvider, *this);
    TranslateSubstring(&aStart, &aLength);
    mInner->DrawToPath(aContext, aPt, aStart, aLength, &wrapper, aAdvanceWidth);
  }
  virtual Metrics MeasureText(PRUint32 aStart, PRUint32 aLength,
                              PRBool aTightBoundingBox,
                              PropertyProvider* aBreakProvider)
  {
    GermanProviderWrapper wrapper(aBreakProvider, *this);
    TranslateSubstring(&aStart, &aLength);
    return mInner->MeasureText(aStart, aLength, aTightBoundingBox, &wrapper);
  }
  virtual void SetLineBreaks(PRUint32 aStart, PRUint32 aLength,
                             PRBool aLineBreakBefore, PRBool aLineBreakAfter,
                             TextProvider* aProvider,
                             gfxFloat* aAdvanceWidthDelta)
  {
    TextProviderWrapper dummyTextProvider;
    TranslateSubstring(&aStart, &aLength);
    return mInner->SetLineBreaks(aStart, aLength, aLineBreakBefore, aLineBreakAfter,
                                 &dummyTextProvider, aAdvanceWidthDelta);
  }
  virtual gfxFloat GetAdvanceWidth(PRUint32 aStart, PRUint32 aLength,
                                   PropertyProvider* aBreakProvider)
  {
    GermanProviderWrapper wrapper(aBreakProvider, *this);
    TranslateSubstring(&aStart, &aLength);
    return mInner->GetAdvanceWidth(aStart, aLength, &wrapper);
  }

  // Tricky forwarding where output conversion is required
  virtual void GetCharFlags(PRUint32 aStart, PRUint32 aLength, PRUint8* aFlags);
  virtual PRBool SetPotentialLineBreaks(PRUint32 aStart, PRUint32 aLength,
                                        PRPackedBool* aBreakBefore);
  virtual PRUint32 BreakAndMeasureText(PRUint32 aStart, PRUint32 aMaxLength,
                                       PRBool aBreakBefore, gfxFloat aWidth,
                                       PropertyProvider* aBreakProvider,
                                       PRBool aSuppressInitialBreak,
                                       Metrics* aMetrics, PRBool aTightBoundingBox,
                                       PRBool* aUsedHyphenation,
                                       PRUint32* aLastBreak);

  friend class GermanProviderWrapper;

private:
  // Translates an original string offset to an offset in the converted string
  void TranslateOffset(PRUint32* aOffset);
  // Translates original string substring offsets to substring offsets in the
  // converted string
  void TranslateSubstring(PRUint32* aStart, PRUint32* aLength);
  // Translates converted string substring offsets to substring offsets in the
  // original string
  void UntranslateSubstring(PRUint32* aStart, PRUint32* aLength) const;

  gfxSkipChars mTransform;
};

void
nsGermanTextRun::TranslateOffset(PRUint32* aOffset)
{
  gfxSkipCharsIterator iter(mTransform);
  iter.SetSkippedOffset(*aOffset);
  *aOffset = iter.GetOriginalOffset();
}

void
nsGermanTextRun::TranslateSubstring(PRUint32* aStart, PRUint32* aLength)
{
  gfxSkipCharsIterator iter(mTransform);
  PRUint32 end = *aStart + *aLength;
  iter.SetSkippedOffset(*aStart);
  *aStart = iter.GetOriginalOffset();
  iter.SetSkippedOffset(end);
  *aLength = iter.GetOriginalOffset() - *aStart;
}

void
nsGermanTextRun::UntranslateSubstring(PRUint32* aStart, PRUint32* aLength) const
{
  gfxSkipCharsIterator iter(mTransform);
  PRUint32 end = *aStart + *aLength;
  iter.SetOriginalOffset(*aStart);
  *aStart = iter.GetSkippedOffset();
  iter.SetOriginalOffset(end);
  *aLength = iter.GetSkippedOffset() - *aStart;
}

void
nsGermanTextRun::GetCharFlags(PRUint32 aStart, PRUint32 aLength,
                              PRUint8* aFlags)
{
  PRUint32 convertedStart = aStart;
  PRUint32 convertedLength = aLength;
  TranslateSubstring(&convertedStart, &convertedLength);

  nsAutoTArray<PRUint8,BIG_TEXT_NODE_SIZE> buffer;
  if (!buffer.AppendElements(convertedLength)) {
    memset(aFlags, 0, aLength);
    return;
  }
  mInner->GetCharFlags(convertedStart, convertedLength, buffer.Elements());

  gfxSkipCharsIterator iter(mTransform);
  nsSkipCharsRunIterator
    run(iter, nsSkipCharsRunIterator::LENGTH_UNSKIPPED_ONLY, aLength);
  // Iterate over the original-string runs. The client simply never sees
  // flags for the extra Ss, since they don't exist as far as the client knows.
  while (run.NextRun()) {
    memcpy(aFlags + run.GetSkippedOffset() - aStart,
           buffer.Elements() + run.GetOriginalOffset() - convertedStart,
           run.GetRunLength());
  }
}

PRBool
nsGermanTextRun::SetPotentialLineBreaks(PRUint32 aStart, PRUint32 aLength,
                                        PRPackedBool* aBreakBefore)
{
  PRUint32 convertedStart = aStart;
  PRUint32 convertedLength = aLength;
  TranslateSubstring(&convertedStart, &convertedLength);

  nsAutoTArray<PRPackedBool,BIG_TEXT_NODE_SIZE> buffer;
  if (!buffer.AppendElements(convertedLength))
    return PR_TRUE;

  gfxSkipCharsIterator iter(mTransform);
  nsSkipCharsRunIterator
    run(iter, nsSkipCharsRunIterator::LENGTH_UNSKIPPED_ONLY, aLength);
  while (run.NextRun()) {
    memcpy(buffer.Elements() + run.GetOriginalOffset() - convertedStart,
           aBreakBefore + run.GetSkippedOffset() - aStart,
           run.GetRunLength());
  }

  return mInner->SetPotentialLineBreaks(convertedStart, convertedLength, buffer.Elements());
}

PRUint32
nsGermanTextRun::BreakAndMeasureText(PRUint32 aStart, PRUint32 aMaxLength,
                                     PRBool aLineBreakBefore, gfxFloat aWidth,
                                     PropertyProvider* aBreakProvider,
                                     PRBool aSuppressInitialBreak,
                                     Metrics* aMetrics, PRBool aTightBoundingBox,
                                     PRBool* aUsedHyphenation,
                                     PRUint32* aLastBreak)
{
  GermanProviderWrapper wrapper(aBreakProvider, *this);
  PRUint32 convertedStart = aStart;
  PRUint32 convertedLength = aMaxLength;
  TranslateSubstring(&convertedStart, &convertedLength);
  PRUint32 lastBreak;
  PRUint32 result =
    mInner->BreakAndMeasureText(convertedStart, convertedLength, aLineBreakBefore,
        aWidth, &wrapper, aSuppressInitialBreak, aMetrics, aTightBoundingBox,
        aUsedHyphenation, &lastBreak);
  gfxSkipCharsIterator iter(mTransform);
  iter.SetOriginalOffset(convertedStart + lastBreak);
  *aLastBreak = iter.GetSkippedOffset() - aStart;
  iter.SetOriginalOffset(convertedStart + result);
  return iter.GetSkippedOffset() - aStart;
}

void
nsGermanTextRun::GermanProviderWrapper::GetHyphenationBreaks(PRUint32 aStart, PRUint32 aLength,
                                                             PRPackedBool* aBreakBefore)
{
  PRUint32 unconvertedStart = aStart;
  PRUint32 unconvertedLength = aLength;
  mRun.UntranslateSubstring(&unconvertedStart, &unconvertedLength);

  // I'm being a bit clever here and reusing aBreakBefore since it's at least
  // as big as the temporary buffer we need. We put our temporary buffer
  // at the end of aBreakBefore so that when we start filling aBreakBefore
  // below, we don't overwrite data we need --- until the very last memmove,
  // which will be in-place.
  NS_ASSERTION(aLength >= unconvertedLength,
               "We only handle *growing* strings here");
  PRPackedBool* buffer = aBreakBefore + aLength - unconvertedLength;
  mInner->GetHyphenationBreaks(unconvertedStart, unconvertedLength, buffer);

  gfxSkipCharsIterator iter(mRun.mTransform, aStart);
  nsSkipCharsRunIterator
    run(iter, nsSkipCharsRunIterator::LENGTH_INCLUDES_SKIPPED, aLength);
  run.SetVisitSkipped();
  while (run.NextRun()) {
    if (run.IsSkipped()) {
      // Disallow hyphenation inside the SS
      memset(aBreakBefore + run.GetOriginalOffset() - aStart, PR_FALSE,
             run.GetRunLength());
    } else {
      memmove(aBreakBefore + run.GetOriginalOffset() - aStart,
              buffer + run.GetSkippedOffset() - unconvertedStart,
              run.GetRunLength());
    }
  }
}

void
nsGermanTextRun::GermanProviderWrapper::GetSpacing(PRUint32 aStart, PRUint32 aLength,
                                                   Spacing* aSpacing)
{
  PRUint32 unconvertedStart = aStart;
  PRUint32 unconvertedLength = aLength;
  mRun.UntranslateSubstring(&unconvertedStart, &unconvertedLength);

  // I'm being a bit clever here and reusing aSpacing since it's at least
  // as big as the temporary buffer we need. We put our temporary buffer
  // at the end of aSpacing so that when we start filling aSpacing
  // below, we don't overwrite data we need --- until the very last memmove,
  // which will be in-place.
  NS_ASSERTION(aLength >= unconvertedLength,
               "We only handle *growing* strings here");
  Spacing* buffer = aSpacing + aLength - unconvertedLength;
  mInner->GetSpacing(unconvertedStart, unconvertedLength, buffer);

  gfxSkipCharsIterator iter(mRun.mTransform, aStart);
  nsSkipCharsRunIterator
    run(iter, nsSkipCharsRunIterator::LENGTH_INCLUDES_SKIPPED, aLength);
  run.SetVisitSkipped();
  // The following code relies on the fact that the only spacing that can be
  // added after a SZLIG is letter-spacing, and the same space should be added
  // after both S's as was addeded after the SZLIG.
  gfxFloat lastSpaceAfter = 0;
  while (run.NextRun()) {
    if (run.IsSkipped()) {
      PRUint32 offset = run.GetOriginalOffset() - aStart;
      PRInt32 i;
      for (i = 0; i < run.GetRunLength(); ++i) {
        aSpacing[offset + i].mBefore = 0;
        aSpacing[offset + i].mAfter = lastSpaceAfter;
      }
    } else {
      memcpy(aSpacing + run.GetOriginalOffset() - aStart,
             buffer + run.GetSkippedOffset() - unconvertedStart,
             run.GetRunLength()*sizeof(Spacing));
      lastSpaceAfter =
        aSpacing[run.GetOriginalOffset() - aStart + run.GetRunLength() - 1].mAfter;
    }
  }
}

/**
 * This textrun class encapsulates the concatenation of a list of text runs. This
 * allows text with runs of different styles (e.g., fonts) to be presented
 * as a single text run.
 */
class nsMultiTextRun : public WrapperTextRun {
public:
  nsMultiTextRun(gfxTextRun* aBaseTextRun, gfxTextRunFactory::Parameters* aParams)
    : WrapperTextRun(aBaseTextRun, aParams) {}

  struct ChildRun {
    void Set(gfxTextRun* aChild, PRUint32 aOffset, PRUint32 aLength) {
      mChild = aChild;
      mOffset = aOffset;
      mLength = aLength;
    }

    nsAutoPtr<gfxTextRun> mChild;
    PRUint32              mOffset;
    PRUint32              mLength;
  };

  // Creates a textrun and adds it to our list.
  nsresult AddChildRun(gfxTextRunFactory* aFactory, const PRUnichar* aText, PRUint32 aLength,
                       gfxTextRunFactory::Parameters* aParams);
  // Clears the list of child runs so that it can be regenerated.
  void ClearChildren() { mRuns.Clear(); }
  nsresult AddChildRun(gfxTextRun* aChild, PRUint32 aOffset, PRUint32 aLength) {
    ChildRun* run = mRuns.AppendElement();
    if (!run)
      return NS_ERROR_OUT_OF_MEMORY;
    run->Set(aChild, aOffset, aLength);
    return NS_OK;
  }

  class MultiProviderWrapper : public ProviderWrapper {
  public:
    MultiProviderWrapper(PropertyProvider* aProvider, const nsMultiTextRun& aRun)
      : ProviderWrapper(aProvider), mRun(aRun) {}

    void SetChildRun(ChildRun* aRun) { mChild = aRun; }

    virtual void GetHyphenationBreaks(PRUint32 aStart, PRUint32 aLength,
                                      PRPackedBool* aBreakBefore)
    {
      mInner->GetHyphenationBreaks(mChild->mOffset + aStart, aLength, aBreakBefore);
    }

    virtual void GetSpacing(PRUint32 aStart, PRUint32 aLength,
                            Spacing* aSpacing)
    {
      mInner->GetSpacing(mChild->mOffset + aStart, aLength, aSpacing);
    }

  protected:
    const nsMultiTextRun& mRun;
    ChildRun*             mChild;
  };

  virtual PRUint8 GetCharFlags(PRUint32 aOffset);
  virtual PRUint32 GetLength();

  virtual PRBool SetPotentialLineBreaks(PRUint32 aStart, PRUint32 aLength,
                                        PRPackedBool* aBreakBefore);
  virtual void Draw(gfxContext* aContext, gfxPoint aPt,
                    PRUint32 aStart, PRUint32 aLength,
                    const gfxRect* aDirtyRect,
                    PropertyProvider* aBreakProvider,
                    gfxFloat* aAdvanceWidth);
  virtual void DrawToPath(gfxContext* aContext, gfxPoint aPt,
                          PRUint32 aStart, PRUint32 aLength,
                          PropertyProvider* aBreakProvider,
                          gfxFloat* aAdvanceWidth);
  virtual Metrics MeasureText(PRUint32 aStart, PRUint32 aLength,
                              PRBool aTightBoundingBox,
                              PropertyProvider* aBreakProvider);
  virtual void SetLineBreaks(PRUint32 aStart, PRUint32 aLength,
                             PRBool aLineBreakBefore, PRBool aLineBreakAfter,
                             TextProvider* aProvider,
                             gfxFloat* aAdvanceWidthDelta);
  virtual gfxFloat GetAdvanceWidth(PRUint32 aStart, PRUint32 aLength,
                                   PropertyProvider* aBreakProvider);
  virtual void GetCharFlags(PRUint32 aStart, PRUint32 aLength, PRUint8* aFlags);
  virtual PRUint32 BreakAndMeasureText(PRUint32 aStart, PRUint32 aMaxLength,
                                       PRBool aLineBreakBefore, gfxFloat aWidth,
                                       PropertyProvider* aBreakProvider,
                                       PRBool aSuppressInitialBreak,
                                       Metrics* aMetrics, PRBool aTightBoundingBox,
                                       PRBool* aUsedHyphenation,
                                       PRUint32* aLastBreak);

  friend class MultiProviderWrapper;

private:
  PRUint32 FindChildIndexContaining(PRUint32 aOffset);
  ChildRun* FindChildContaining(PRUint32 aOffset) {
    return &mRuns[FindChildIndexContaining(aOffset)];
  }

  class ChildIterator {
  public:
    ChildIterator(nsMultiTextRun& aMulti, PRUint32 aOffset, PRUint32 aLength,
                  MultiProviderWrapper* aWrapper = nsnull);
    PRBool NextChild();
    ChildRun* GetChild() { return mChild; }
    gfxTextRun* GetTextRun() { return mChild->mChild; }
    PRUint32 GetChildOffset() { return mChildOffset; }
    PRUint32 GetChildLength() { return mChildLength; }
    PRUint32 GetOffset() { return mOffset; }
  private:
    nsMultiTextRun&       mMulti;
    MultiProviderWrapper* mWrapper;
    ChildRun*             mChild;
    PRUint32              mIndex;
    PRUint32              mOffset;
    PRUint32              mRemaining;
    PRUint32              mChildOffset;
    PRUint32              mChildLength;
  };
  friend class ChildIterator;
  
  void CombineMetrics(gfxTextRun::Metrics* aRunning, const gfxTextRun::Metrics& aMetrics);

  nsTArray<ChildRun> mRuns;
};

nsMultiTextRun::ChildIterator::ChildIterator(nsMultiTextRun& aMulti,
                                             PRUint32 aOffset, PRUint32 aLength,
                                             MultiProviderWrapper* aWrapper)
  : mMulti(aMulti), mWrapper(aWrapper), mRemaining(aLength)
{
  mIndex = mMulti.FindChildIndexContaining(aOffset);
}

PRBool
nsMultiTextRun::ChildIterator::NextChild()
{
  if (mRemaining <= 0)
    return PR_FALSE;
  NS_ASSERTION(mIndex < mMulti.mRuns.Length(), "substring overrun");
  mChild = &mMulti.mRuns[mIndex];
  ++mIndex;

  mChildOffset = PR_MAX(mChild->mOffset, mOffset) - mChild->mOffset;
  mChildLength = PR_MIN(mChild->mLength, mChildOffset + mRemaining);

  mOffset += mChildLength;
  mRemaining -= mChildLength;
  if (mWrapper) {
    mWrapper->SetChildRun(mChild);
  }
  return PR_TRUE;
}

PRUint32
nsMultiTextRun::GetLength()
{
  if (mRuns.Length() == 0)
    return 0;
  ChildRun* last = &mRuns[mRuns.Length() - 1];
  return last->mOffset + last->mLength;
}

nsresult
nsMultiTextRun::AddChildRun(gfxTextRunFactory* aFactory, const PRUnichar* aText,
                            PRUint32 aLength,
                            gfxTextRunFactory::Parameters* aParams)
{
  PRUint32 offset = GetLength();

  // Update line break list in aParams for the creation of the child. We maintain
  // the invariant that aParams->mInitialBreaks points to the breaks for the
  // next child to be added and subsequent children.
  PRUint32 breakCount = 0;
  PRUint32 currentBreakCount = aParams->mInitialBreakCount;
  while (breakCount < currentBreakCount &&
         aParams->mInitialBreaks[breakCount] <= offset + aLength) {
    // Get the break position into child-textrun coordinates
    aParams->mInitialBreaks[breakCount] -= offset;
    ++breakCount;
  }
  aParams->mInitialBreakCount = breakCount;

  gfxTextRun* run = aFactory->MakeTextRun(aText, aLength, aParams);
  if (!run)
    return PR_FALSE;
  // force child to remember its text so we don't have to support recovering it
  run->RememberText(aText, aLength);

  // Now update line break list in aParams for the next iteration
  // If there's a break at the end of this child, leave it in the list so
  // that the next child will see it at its start.
  if (breakCount > 0 && aParams->mInitialBreaks[breakCount - 1] == aLength) {
    // restore to global-textrun coordinates. The rest of the breaks are
    // skipped, never to be used again, so we don't need to restore them.
    aParams->mInitialBreaks[breakCount - 1] = offset + aLength;
    --breakCount;
  }
  aParams->mInitialBreaks += breakCount;
  aParams->mInitialBreakCount -= breakCount;

  return AddChildRun(run, offset, aLength);
}

PRUint32
nsMultiTextRun::FindChildIndexContaining(PRUint32 aOffset)
{
  PRUint32 start = 0;
  PRUint32 end = mRuns.Length();
  while (end - start > 1) {
    PRUint32 mid = (start + end)/2;
    if (mRuns[mid].mOffset <= aOffset) {
      start = mid;
    } else {
      end = mid;
    }
  }
  NS_ASSERTION(mRuns[start].mOffset + mRuns[start].mLength > aOffset,
               "Substring offset out of range!");
  return start;
}

void
nsMultiTextRun::CombineMetrics(gfxTextRun::Metrics* aRunning,
                               const gfxTextRun::Metrics& aMetrics)
{
  if (IsRightToLeft()) {
    // 'aRunning' is to the right of 'aMetrics'
    gfxTextRun::Metrics tmp = aMetrics;
    tmp.CombineWith(*aRunning);
    *aRunning = tmp;
  } else {
    // 'aRunning' is to the left of 'aMetrics'
    aRunning->CombineWith(aMetrics);
  }
}

PRUint8
nsMultiTextRun::GetCharFlags(PRUint32 aOffset)
{
  ChildRun* child = FindChildContaining(aOffset);
  return mInner->GetCharFlags(aOffset - child->mOffset);
}

void
nsMultiTextRun::GetCharFlags(PRUint32 aStart, PRUint32 aLength, PRUint8* aFlags)
{
  ChildIterator iter(*this, aStart, aLength);
  while (iter.NextChild()) {
    iter.GetTextRun()->GetCharFlags(iter.GetChildOffset(), iter.GetChildLength(),
                                    aFlags);
    aFlags += iter.GetChildLength();
  }
}

PRBool
nsMultiTextRun::SetPotentialLineBreaks(PRUint32 aStart, PRUint32 aLength,
                                       PRPackedBool* aBreakBefore)
{
  ChildIterator iter(*this, aStart, aLength);
  PRBool changed = PR_FALSE;
  while (iter.NextChild()) {
    changed |= iter.GetTextRun()->SetPotentialLineBreaks(iter.GetChildOffset(), iter.GetChildLength(),
                                                         aBreakBefore);
    aBreakBefore += iter.GetChildLength();
  }
  return changed;
}

void
nsMultiTextRun::Draw(gfxContext* aContext, gfxPoint aPt,
                     PRUint32 aStart, PRUint32 aLength,
                     const gfxRect* aDirtyRect, PropertyProvider* aBreakProvider,
                     gfxFloat* aAdvanceWidth)
{
  MultiProviderWrapper wrapper(aBreakProvider, *this);
  ChildIterator iter(*this, aStart, aLength, &wrapper);
  gfxFloat totalAdvance = 0;
  while (iter.NextChild()) {
    gfxFloat advance;
    gfxPoint pt = gfxPoint(aPt.x + iter.GetTextRun()->GetDirection()*totalAdvance, aPt.y);
    iter.GetTextRun()->Draw(aContext, pt,
                            iter.GetChildOffset(), iter.GetChildLength(),
                            aDirtyRect, &wrapper, &advance);
    totalAdvance += advance;
  }
  if (aAdvanceWidth) {
    *aAdvanceWidth = totalAdvance;
  }
}

void
nsMultiTextRun::DrawToPath(gfxContext* aContext, gfxPoint aPt,
                           PRUint32 aStart, PRUint32 aLength,
                           PropertyProvider* aBreakProvider,
                           gfxFloat* aAdvanceWidth)
{
  MultiProviderWrapper wrapper(aBreakProvider, *this);
  ChildIterator iter(*this, aStart, aLength, &wrapper);
  gfxFloat totalAdvance = 0;
  while (iter.NextChild()) {
    gfxFloat advance;
    gfxPoint pt = gfxPoint(aPt.x + iter.GetTextRun()->GetDirection()*totalAdvance, aPt.y);
    iter.GetTextRun()->DrawToPath(aContext, pt,
                                  iter.GetChildOffset(), iter.GetChildLength(),
                                  &wrapper, &advance);
    totalAdvance += advance;
  }
  if (aAdvanceWidth) {
    *aAdvanceWidth = totalAdvance;
  }
}

gfxTextRun::Metrics
nsMultiTextRun::MeasureText(PRUint32 aStart, PRUint32 aLength,
                            PRBool aTightBoundingBox,
                            PropertyProvider* aBreakProvider)
{
  MultiProviderWrapper wrapper(aBreakProvider, *this);
  ChildIterator iter(*this, aStart, aLength, &wrapper);
  gfxTextRun::Metrics result;
  while (iter.NextChild()) {
    gfxTextRun::Metrics metrics = iter.GetTextRun()->MeasureText(
        iter.GetChildOffset(), iter.GetChildLength(), aTightBoundingBox, &wrapper);
    CombineMetrics(&result, metrics);
  }
  return result;
}

PRUint32
nsMultiTextRun::BreakAndMeasureText(PRUint32 aStart, PRUint32 aMaxLength,
                                    PRBool aBreakBefore, gfxFloat aWidth,
                                    PropertyProvider* aBreakProvider,
                                    PRBool aSuppressInitialBreak,
                                    Metrics* aMetrics, PRBool aTightBoundingBox,
                                    PRBool* aUsedHyphenation,
                                    PRUint32* aLastBreak)
{
  MultiProviderWrapper wrapper(aBreakProvider, *this);
  ChildIterator iter(*this, aStart, aMaxLength, &wrapper);
  PRUint32 fitSoFar = 0;
  gfxTextRun::Metrics totalMetrics;
  PRUint32 lastBreak = aMaxLength;

  while (iter.NextChild()) {
    gfxTextRun::Metrics metrics;
    PRUint32 childLastBreak;
    PRUint32 fits = iter.GetTextRun()->BreakAndMeasureText(
        iter.GetChildOffset(), iter.GetChildLength(), aBreakBefore,
        aWidth - aMetrics->mAdvanceWidth, &wrapper, aSuppressInitialBreak,
        &metrics, aTightBoundingBox, aUsedHyphenation, &childLastBreak);

    CombineMetrics(&totalMetrics, metrics);
    if (childLastBreak < iter.GetChildLength()) {
      lastBreak = fitSoFar + childLastBreak;
    }
    fitSoFar += fits;

    if (fits < iter.GetChildLength())
      break;
    aBreakBefore = PR_FALSE;
    aSuppressInitialBreak = PR_FALSE;
  }

  if (aMetrics) {
    *aMetrics = totalMetrics;
  }
  if (aLastBreak) {
    *aLastBreak = lastBreak;
  }
  return fitSoFar;
}

void
nsMultiTextRun::SetLineBreaks(PRUint32 aStart, PRUint32 aLength,
                              PRBool aLineBreakBefore, PRBool aLineBreakAfter,
                              TextProvider* aProvider,
                              gfxFloat* aAdvanceWidthDelta)
{
  TextProviderWrapper dummyTextProvider;
  ChildIterator iter(*this, aStart, aLength);
  gfxFloat delta = 0;
  while (iter.NextChild()) {
    gfxFloat thisDelta;
    iter.GetTextRun()->SetLineBreaks(
        iter.GetChildOffset(), iter.GetChildLength(),
        aLineBreakBefore && iter.GetOffset() == aStart,
        aLineBreakAfter && iter.GetOffset() + iter.GetChildLength() == aStart + aLength,
        &dummyTextProvider, &thisDelta);
    delta += thisDelta;
  }
  *aAdvanceWidthDelta = delta;
}

gfxFloat
nsMultiTextRun::GetAdvanceWidth(PRUint32 aStart, PRUint32 aLength,
                                PropertyProvider* aBreakProvider)
{
  MultiProviderWrapper wrapper(aBreakProvider, *this);
  ChildIterator iter(*this, aStart, aLength, &wrapper);
  gfxFloat result;
  while (iter.NextChild()) {
    result += iter.GetTextRun()->GetAdvanceWidth(
        iter.GetChildOffset(), iter.GetChildLength(), &wrapper);
  }
  return result;
}

// ==============================
// Now define factories that use the above textruns
// ==============================

static gfxTextRunFactory::Parameters
GetParametersForInner(const gfxTextRunFactory::Parameters& aParams,
                      gfxSkipChars* aDummy)
{
  gfxTextRunFactory::Parameters params =
    { aParams.mContext, nsnull, aParams.mLangGroup, aDummy,
      aParams.mInitialBreaks, aParams.mInitialBreakCount,
      aParams.mAppUnitsPerDevUnit, aParams.mFlags };
  return params;
}

gfxTextRun*
nsTransformingTextRunFactory::MakeTextRun(const PRUint8* aString, PRUint32 aLength,
                                          gfxTextRunFactory::Parameters* aParams)
{
  // We'll only have a Unicode code path to minimize the amount of code needed
  // for this rarely used feature
  NS_ConvertASCIItoUTF16 unicodeString(NS_REINTERPRET_CAST(const char*, aString), aLength);
  Parameters params = *aParams;
  params.mFlags &= ~gfxTextRunFactory::TEXT_IS_PERSISTENT;
  return MakeTextRun(unicodeString.get(), aLength, &params);
}

/**
 * For smallcaps we divide the text into runs of lowercase and non-lowercase
 * clusters and build a nsMultiTextRun for the separate runs. For the lowercase
 * runs we use nsCaseTransformTextRunFactory to convert to uppercase.
 */
gfxTextRun*
nsFontVariantTextRunFactory::MakeTextRun(const PRUnichar* aString, PRUint32 aLength,
                                         gfxTextRunFactory::Parameters* aParams)
{
  nsICaseConversion* converter = nsTextTransformer::GetCaseConv();
  if (!converter || !mStyles)
    return nsnull;

  gfxFontStyle fontStyle = *mFontGroup->GetStyle();
  fontStyle.size *= 0.8;
  nsRefPtr<gfxFontGroup> smallFont = mFontGroup->Copy(&fontStyle);
  if (!smallFont)
    return nsnull;

  gfxSkipChars dummy;
  gfxTextRunFactory::Parameters innerParams = GetParametersForInner(*aParams, &dummy);
  gfxTextRun* inner = mFontGroup->MakeTextRun(aString, aLength, &innerParams);
  if (!inner)
    return nsnull;
  // force the inner to remember the text, so we don't have to worry about
  // reconstructing it later.
  inner->RememberText(aString, aLength);

  nsAutoPtr<nsMultiTextRun> textRun;
  textRun = new nsMultiTextRun(inner, aParams);
  if (!textRun)
    return nsnull;

  nsRefPtr<nsCaseTransformTextRunFactory> uppercaseFactory =
    new nsCaseTransformTextRunFactory(smallFont, nsnull, PR_TRUE);
  if (!uppercaseFactory)
    return nsnull;

  PRUint32 i;
  PRUint32 runStart = 0;
  PRPackedBool runIsLowercase = PR_FALSE;
  for (i = 0; i <= aLength; ++i) {
    PRBool isLowercase = PR_FALSE;
    if (i < aLength) {
      // Characters that aren't the start of a cluster are ignored here. They
      // get added to whatever lowercase/non-lowercase run we're in.
      if (!(inner->GetCharFlags(i) & gfxTextRun::CLUSTER_START))
        continue;

      if (mStyles[i]->GetStyleFont()->mFont.variant == NS_STYLE_FONT_VARIANT_SMALL_CAPS) {
        PRUnichar ch = aString[i];
        PRUnichar ch2;
        converter->ToUpper(ch, &ch2);
        isLowercase = ch != ch2;
      } else {
        // Don't transform the character! I.e., pretend that it's not lowercase
      }
    }

    if ((i == aLength || runIsLowercase != isLowercase) && runStart < i) {
      gfxTextRunFactory* factory;
      if (runIsLowercase) {
        factory = uppercaseFactory;
        if (mInnerTransformingTextRunFactory) {
          mInnerTransformingTextRunFactory->SetStyles(mStyles + runStart);
        }
      } else {
        factory = mFontGroup;
      }
      nsresult rv = textRun->AddChildRun(factory, aString + runStart, i - runStart, &innerParams);
      if (NS_FAILED(rv))
        return nsnull;
      runStart = i;
      runIsLowercase = isLowercase;
    }
  }

  return textRun.forget();
}

class nsCaseTransformingTextRun : public nsMultiTextRun {
public:
  nsCaseTransformingTextRun(gfxTextRun* aBaseTextRun,
                            gfxTextRunFactory::Parameters* aParams,
                            nsCaseTransformTextRunFactory* aFactory,
                            const PRUnichar* aString, PRUint32 aLength)
    : nsMultiTextRun(aBaseTextRun, aParams), mString(aString, aLength),
      mFactory(aFactory), mRefContext(aParams->mContext),
      mLangGroup(aParams->mLangGroup),
      mAppUnitsPerDevUnit(aParams->mAppUnitsPerDevUnit),
      mFlags(aParams->mFlags)
  {
    PRUint32 i;
    for (i = 0; i < aLength; ++i) {
      mLineBreakOpportunities.AppendElement(PR_FALSE);
      if (!aFactory->IsAllUppercase()) {
        mStyles.AppendElement(aFactory->GetStyles()[i]);
      }
    }
    for (i = 0; i < aParams->mInitialBreakCount; ++i) {
      mLineBreaks.AppendElement(aParams->mInitialBreaks[i]);
    }
  }

  virtual PRBool SetPotentialLineBreaks(PRUint32 aStart, PRUint32 aLength,
                                        PRPackedBool* aBreakBefore);
  virtual void SetLineBreaks(PRUint32 aStart, PRUint32 aLength,
                             PRBool aLineBreakBefore, PRBool aLineBreakAfter,
                             TextProvider* aProvider,
                             gfxFloat* aAdvanceWidthDelta);
  nsresult Build();

private:
  nsString                                mString;
  nsRefPtr<nsCaseTransformTextRunFactory> mFactory;
  nsRefPtr<gfxContext>                    mRefContext;
  nsCOMPtr<nsIAtom>                       mLangGroup;
  nsTArray<PRUint32>                      mLineBreaks;
  nsTArray<PRPackedBool>                  mLineBreakOpportunities;
  nsTArray<nsRefPtr<nsStyleContext> >     mStyles;
  gfxFloat                                mAppUnitsPerDevUnit;
  PRUint32                                mFlags;
};

nsresult
nsCaseTransformingTextRun::Build()
{
  nsICaseConversion* converter = nsTextTransformer::GetCaseConv();
  if (!converter)
    return NS_ERROR_FAILURE;

  nsAutoString convertedString;
  PRUint32 i;
  // skipBuilder represents a translation from the *transformed* string to the
  // *original* string, because in this case, that's the way deletions go
  gfxSkipCharsBuilder skipBuilder;
  nsAutoTArray<nsStyleContext*,1000> styles;

  for (i = 0; i < mString.Length(); ++i) {
    PRUnichar ch = mString.CharAt(i);

    // We're definitely going to keep at least one char in the hypothetical
    // "transformation" from original to transformed string
    skipBuilder.KeepChar();
    
    PRUint8 style = mFactory->IsAllUppercase() ? NS_STYLE_TEXT_TRANSFORM_UPPERCASE
      : mStyles[i]->GetStyleText()->mTextTransform;

    switch (style) {
    case NS_STYLE_TEXT_TRANSFORM_LOWERCASE:
      converter->ToLower(ch, &ch);
      break;
    case NS_STYLE_TEXT_TRANSFORM_UPPERCASE:
      if (ch == SZLIG) {
        convertedString.Append('S');
        skipBuilder.SkipChar();
        ch = 'S';
      } else {
        converter->ToUpper(ch, &ch);
      }
      break;
    case NS_STYLE_TEXT_TRANSFORM_CAPITALIZE:
      if (mLineBreakOpportunities[i]) {
        if (ch == SZLIG) {
          styles.AppendElement(mStyles[i]);
          convertedString.Append('S');
          skipBuilder.SkipChar();
          ch = 'S';
        } else {
          converter->ToTitle(ch, &ch);
        }
      }
      break;
    default:
      break;
    }

    styles.AppendElement(mStyles[i]);
    convertedString.Append(ch);
  }

  PRBool allCharsKept = skipBuilder.GetAllCharsKept();
  gfxSkipChars skipChars;
  skipChars.TakeFrom(&skipBuilder);

  // Convert line break offsets
  nsAutoTArray<PRUint32,50> lineBreaks;
  if (!lineBreaks.AppendElements(mLineBreaks.Length()))
    return NS_ERROR_OUT_OF_MEMORY;
  gfxSkipCharsIterator iter(skipChars);
  for (i = 0; i < mLineBreaks.Length(); ++i) {
    lineBreaks[i] = iter.ConvertSkippedToOriginal(mLineBreaks[i]);
  }

  // Create a textrun for the transformed string
  gfxSkipChars dummy;
  gfxTextRunFactory::Parameters innerParams = 
    { mRefContext, nsnull, mLangGroup, &dummy, mLineBreaks.Elements(),
      mLineBreaks.Length(), PRUint32(mAppUnitsPerDevUnit),
      mFlags | gfxTextRunFactory::TEXT_IS_PERSISTENT };

  if (mFactory->GetInnerTransformingTextRunFactory()) {
    mFactory->GetInnerTransformingTextRunFactory()->SetStyles(styles.Elements());
  }

  nsAutoPtr<gfxTextRun> innerTextRun;
  innerTextRun =
    mFactory->GetInnerTextRunFactory()->MakeTextRun(convertedString.get(), convertedString.Length(),
                                                    &innerParams);
  if (!innerTextRun)
    return NS_ERROR_FAILURE;
  // Make sure the inner textrun never calls ForceRememberText, because we don't
  // implement it
  innerTextRun->RememberText(convertedString.get(), convertedString.Length());

  if (!allCharsKept) {
    // We need to adjust offsets because characters were inserted.
    gfxTextRun* wrapper = new nsGermanTextRun(innerTextRun, &skipChars, &innerParams);
    if (!wrapper)
      return NS_ERROR_FAILURE;
    innerTextRun.forget();
    innerTextRun = wrapper;
  }

  ClearChildren();
  nsresult rv = AddChildRun(innerTextRun, 0, convertedString.Length());
  if (NS_SUCCEEDED(rv)) {
    innerTextRun.forget();
  }
  return rv;
}

PRBool
nsCaseTransformingTextRun::SetPotentialLineBreaks(PRUint32 aStart, PRUint32 aLength,
                                                  PRPackedBool* aBreakBefore)
{
  PRUint32 i;
  PRBool changed = PR_FALSE;
  PRBool changedStyle = PR_FALSE;
  for (i = 0; i < aLength; ++i) {
    if (mLineBreakOpportunities[aStart + i] != aBreakBefore[i]) {
      changed = PR_TRUE;
      if (mStyles[i]->GetStyleText()->mTextTransform == NS_STYLE_TEXT_TRANSFORM_CAPITALIZE) {
        changedStyle = PR_TRUE;
      }
      mLineBreakOpportunities[aStart + i] = aBreakBefore[i];
    }
  }
  if (!changedStyle)
    return nsMultiTextRun::SetPotentialLineBreaks(aStart, aLength, aBreakBefore);

  Build();
  return changed;
}

void
nsCaseTransformingTextRun::SetLineBreaks(PRUint32 aStart, PRUint32 aLength,
                                         PRBool aLineBreakBefore, PRBool aLineBreakAfter,
                                         TextProvider* aProvider,
                                         gfxFloat* aAdvanceWidthDelta)
{
  nsTArray<PRUint32> newBreaks;
  for (PRUint32 i = 0; i < mLineBreaks.Length(); ++i) {
    PRUint32 b = mLineBreaks[i];
    if (b == aStart && !aLineBreakBefore)
      continue;
    if (b > aStart && aLineBreakBefore) {
      newBreaks.AppendElement(aStart);
      aLineBreakBefore = PR_FALSE;
    }
    if (b == aStart + aLength && !aLineBreakAfter)
      continue;
    if (b > aStart + aLength && aLineBreakAfter) {
      newBreaks.AppendElement(aStart + aLength);
      aLineBreakAfter = PR_FALSE;
    }
    newBreaks.AppendElement(b);
  }
  mLineBreaks.SwapElements(newBreaks);
  nsMultiTextRun::SetLineBreaks(aStart, aLength, aLineBreakBefore, aLineBreakAfter,
                                aProvider, aAdvanceWidthDelta);
}

/**
 * When doing case conversion we first transform the string. If the string
 * ends up with no extra characters inserted, we can just create a textrun
 * from the transformed string and hand it back to the caller, because string
 * offsets are the same. If extra characters are inserted due to
 * uppercased SZLIG, we have to create a special text run, nsGermanTextRun,
 * which adjusts character offsets for us.
 */
gfxTextRun*
nsCaseTransformTextRunFactory::MakeTextRun(const PRUnichar* aString, PRUint32 aLength,
                                           gfxTextRunFactory::Parameters* aParams)
{
  // Create a textrun for the transformed string
  gfxSkipChars dummy;
  gfxTextRunFactory::Parameters innerParams = GetParametersForInner(*aParams, &dummy);
  innerParams.mFlags &= ~gfxTextRunFactory::TEXT_IS_PERSISTENT;

  if (mInnerTransformingTextRunFactory) {
    mInnerTransformingTextRunFactory->SetStyles(mStyles);
  }

  // This is the dummy that nsMultiTextRun needs
  nsAutoPtr<gfxTextRun> innerTextRun;
  innerTextRun =
    mInnerTextRunFactory->MakeTextRun(aString, aLength, &innerParams);
  if (!innerTextRun)
    return nsnull;
  // Make sure the inner textrun never calls ForceRememberText, because we don't
  // implement it
  innerTextRun->RememberText(aString, aLength);

  nsAutoPtr<nsCaseTransformingTextRun> textRun;
  textRun =
    new nsCaseTransformingTextRun(innerTextRun, aParams, this, aString, aLength);
  if (!textRun)
    return nsnull;
  innerTextRun.forget();
  nsresult rv = textRun->Build();
  if (NS_FAILED(rv))
    return nsnull;
  return textRun.forget();
}

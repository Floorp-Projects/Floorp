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

/**
 * So that we can reshape as necessary, we store enough information
 * to fully rebuild the textrun contents.
 */
class nsTransformedTextRun : public gfxTextRun {
public:
  nsTransformedTextRun(gfxTextRunFactory::Parameters* aParams,
                       nsTransformingTextRunFactory* aFactory,
                       const PRUnichar* aString, PRUint32 aLength,
                       nsStyleContext** aStyles)
    : gfxTextRun(aParams, aLength), mString(aString, aLength),
      mFactory(aFactory), mRefContext(aParams->mContext),
      mLangGroup(aParams->mLangGroup)
  {
    PRUint32 i;
    for (i = 0; i < aLength; ++i) {
      mStyles.AppendElement(aStyles[i]);
    }
    for (i = 0; i < aParams->mInitialBreakCount; ++i) {
      mLineBreaks.AppendElement(aParams->mInitialBreaks[i]);
    }
  }

  virtual PRBool SetPotentialLineBreaks(PRUint32 aStart, PRUint32 aLength,
                                        PRPackedBool* aBreakBefore)
  {
    PRBool changed = gfxTextRun::SetPotentialLineBreaks(aStart, aLength, aBreakBefore);
    mFactory->RebuildTextRun(this);
    return changed;
  }
  virtual PRBool SetLineBreaks(PRUint32 aStart, PRUint32 aLength,
                               PRBool aLineBreakBefore, PRBool aLineBreakAfter,
                               PropertyProvider* aProvider,
                               gfxFloat* aAdvanceWidthDelta);

  nsString                                mString;
  nsAutoPtr<nsTransformingTextRunFactory> mFactory;
  nsRefPtr<gfxContext>                    mRefContext;
  nsCOMPtr<nsIAtom>                       mLangGroup;
  nsTArray<PRUint32>                      mLineBreaks;
  nsTArray<nsRefPtr<nsStyleContext> >     mStyles;
};

PRBool
nsTransformedTextRun::SetLineBreaks(PRUint32 aStart, PRUint32 aLength,
                                    PRBool aLineBreakBefore, PRBool aLineBreakAfter,
                                    PropertyProvider* aProvider,
                                    gfxFloat* aAdvanceWidthDelta)
{
  nsTArray<PRUint32> newBreaks;
  PRUint32 i;
  PRBool changed = PR_FALSE;
  for (i = 0; i < mLineBreaks.Length(); ++i) {
    PRUint32 pos = mLineBreaks[i];
    if (pos >= aStart)
      break;
    newBreaks.AppendElement(pos);
  }
  if (aLineBreakBefore != (i < mLineBreaks.Length() &&
                           mLineBreaks[i] == aStart)) {
    changed = PR_TRUE;
  }
  if (aLineBreakBefore) {
    newBreaks.AppendElement(aStart);
  }
  if (aLineBreakAfter != (i + 1 < mLineBreaks.Length() &&
                          mLineBreaks[i + 1] == aStart + aLength)) {
    changed = PR_TRUE;
  }
  if (aLineBreakAfter) {
    newBreaks.AppendElement(aStart + aLength);
  }
  for (; i < mLineBreaks.Length(); ++i) {
    if (mLineBreaks[i] > aStart + aLength)
      break;
    changed = PR_TRUE;
  }
  if (!changed) {
    if (aAdvanceWidthDelta) {
      *aAdvanceWidthDelta = 0;
    }
    return PR_FALSE;
  }

  newBreaks.AppendElements(mLineBreaks.Elements() + i, mLineBreaks.Length() - i);
  mLineBreaks.SwapElements(newBreaks);

  gfxFloat currentAdvance = GetAdvanceWidth(aStart, aLength, aProvider);
  mFactory->RebuildTextRun(this);
  if (aAdvanceWidthDelta) {
    *aAdvanceWidthDelta = GetAdvanceWidth(aStart, aLength, aProvider) - currentAdvance;
  }
  return PR_TRUE;
}

gfxTextRun*
nsTransformingTextRunFactory::MakeTextRun(const PRUnichar* aString, PRUint32 aLength,
                                          gfxTextRunFactory::Parameters* aParams,
                                          nsStyleContext** aStyles)
{
  nsTransformedTextRun* textRun =
    new nsTransformedTextRun(aParams, this, aString, aLength, aStyles);
  if (!textRun)
    return nsnull;
  RebuildTextRun(textRun);
  return textRun;
}

gfxTextRun*
nsTransformingTextRunFactory::MakeTextRun(const PRUint8* aString, PRUint32 aLength,
                                          gfxTextRunFactory::Parameters* aParams,
                                          nsStyleContext** aStyles)
{
  // We'll only have a Unicode code path to minimize the amount of code needed
  // for these rarely used features
  NS_ConvertASCIItoUTF16 unicodeString(NS_REINTERPRET_CAST(const char*, aString), aLength);
  gfxTextRunFactory::Parameters params = *aParams;
  params.mFlags &= ~gfxTextRunFactory::TEXT_IS_PERSISTENT;
  return MakeTextRun(unicodeString.get(), aLength, &params, aStyles);
}

static PRUint32
CountGlyphs(const gfxTextRun::DetailedGlyph* aDetails) {
  PRUint32 glyphCount;
  for (glyphCount = 0; !aDetails[glyphCount].mIsLastGlyph; ++glyphCount) {
  }
  return glyphCount + 1;
}

/**
 * Concatenate textruns together just by copying their glyphrun data
 */
static void
AppendTextRun(gfxTextRun* aDest, gfxTextRun* aSrc, PRUint32 aOffset)
{
  PRUint32 numGlyphRuns;
  const gfxTextRun::GlyphRun* glyphRuns = aSrc->GetGlyphRuns(&numGlyphRuns);
  PRUint32 j;
  PRUint32 offset = aOffset;
  for (j = 0; j < numGlyphRuns; ++j) {
    PRUint32 runOffset = glyphRuns[j].mCharacterOffset;
    PRUint32 len =
      (j + 1 < numGlyphRuns ? glyphRuns[j + 1].mCharacterOffset : aSrc->GetLength()) -
      runOffset;
    nsresult rv = aDest->AddGlyphRun(glyphRuns[j].mFont, offset);
    if (NS_FAILED(rv))
      return;

    PRUint32 k;
    for (k = 0; k < len; ++k) {
      gfxTextRun::CompressedGlyph g = aSrc->GetCharacterGlyphs()[runOffset + k];
      if (g.IsComplexCluster()) {
        const gfxTextRun::DetailedGlyph* details = aSrc->GetDetailedGlyphs(runOffset + k);
        aDest->SetDetailedGlyphs(offset, details, CountGlyphs(details));
      } else {
        aDest->SetCharacterGlyph(offset, g);
      }
      ++offset;
    }
  }
  NS_ASSERTION(offset - aOffset == aSrc->GetLength(),
               "Something went wrong in our length calculations...");
}

/**
 * Copy a given textrun, but merge certain characters into a single logical
 * character. Glyphs for a character are added to the glyph list for the previous
 * character and then the merged character is eliminated. Visually the results
 * are identical.
 * 
 * This is used for text-transform:uppercase when we encounter a SZLIG,
 * whose uppercase form is "SS".
 * 
 * This function is unable to merge characters when they occur in different
 * glyph runs. It's hard to see how this could happen, but if it does, we just
 * discard the characters-to-merge.
 * 
 * @param aCharsToMerge when aCharsToMerge[i] is true, this character is
 * merged into the previous character
 */
static void
MergeCharactersInTextRun(gfxTextRun* aDest, gfxTextRun* aSrc,
                         PRPackedBool* aCharsToMerge)
{
  aDest->ResetGlyphRuns();

  PRUint32 numGlyphRuns;
  const gfxTextRun::GlyphRun* glyphRuns = aSrc->GetGlyphRuns(&numGlyphRuns);
  PRUint32 offset = 0;
  PRUint32 j;
  for (j = 0; j < numGlyphRuns; ++j) {
    PRUint32 runOffset = glyphRuns[j].mCharacterOffset;
    PRUint32 len =
      (j + 1 < numGlyphRuns ? glyphRuns[j + 1].mCharacterOffset : aSrc->GetLength()) -
      runOffset;
    nsresult rv = aDest->AddGlyphRun(glyphRuns[j].mFont, offset);
    if (NS_FAILED(rv))
      return;

    PRUint32 k;
    for (k = 0; k < len; ++k) {
      if (aCharsToMerge[runOffset + k])
        continue;

      gfxTextRun::CompressedGlyph g = aSrc->GetCharacterGlyphs()[runOffset + k];
      if (g.IsSimpleGlyph() || g.IsComplexCluster()) {
        PRUint32 mergedCount = 1;
        PRBool multipleGlyphs = PR_FALSE;
        while (k + mergedCount < len) {
          gfxTextRun::CompressedGlyph h = aSrc->GetCharacterGlyphs()[runOffset + k + mergedCount];
          if (!aCharsToMerge[runOffset + k + mergedCount] &&
              !h.IsClusterContinuation() && !h.IsLigatureContinuation())
            break;
          if (h.IsComplexCluster() || h.IsSimpleGlyph()) {
            multipleGlyphs = PR_TRUE;
          }
          ++mergedCount;
        }
        if (g.IsSimpleGlyph() && !multipleGlyphs) {
          aDest->SetCharacterGlyph(offset, g);
        } else {
          // We have something complex to do.
          nsAutoTArray<gfxTextRun::DetailedGlyph,2> detailedGlyphs;
          PRUint32 m;
          for (m = 0; m < mergedCount; ++m) {
            gfxTextRun::CompressedGlyph h = aSrc->GetCharacterGlyphs()[runOffset + k + m];
            if (h.IsSimpleGlyph()) {
              gfxTextRun::DetailedGlyph* details = detailedGlyphs.AppendElement();
              if (!details)
                return;
              details->mGlyphID = h.GetSimpleGlyph();
              details->mAdvance = h.GetSimpleAdvance();
              details->mXOffset = 0;
              details->mYOffset = 0;
            } else if (h.IsComplexCluster()) {
              const gfxTextRun::DetailedGlyph* srcDetails = aSrc->GetDetailedGlyphs(runOffset + k + m);
              detailedGlyphs.AppendElements(srcDetails, CountGlyphs(srcDetails));
            }
            detailedGlyphs[detailedGlyphs.Length() - 1].mIsLastGlyph = PR_FALSE;
          }
          detailedGlyphs[detailedGlyphs.Length() - 1].mIsLastGlyph = PR_TRUE;
          aDest->SetDetailedGlyphs(offset, detailedGlyphs.Elements(), detailedGlyphs.Length());
        }
      } else {
        aDest->SetCharacterGlyph(offset, g);
      }
      ++offset;
    }
  }
  NS_ASSERTION(offset == aDest->GetLength(), "Bad offset calculations");
}

static gfxTextRunFactory::Parameters
GetParametersForInner(nsTransformedTextRun* aTextRun,
                      gfxSkipChars* aDummy)
{
  gfxTextRunFactory::Parameters params =
    { aTextRun->mRefContext, nsnull, aTextRun->mLangGroup, aDummy,
      nsnull, nsnull, PRUint32(aTextRun->GetAppUnitsPerDevUnit()),
      aTextRun->GetFlags() };
  return params;
}

void
nsFontVariantTextRunFactory::RebuildTextRun(nsTransformedTextRun* aTextRun)
{
  nsICaseConversion* converter = nsTextTransformer::GetCaseConv();
  if (!converter)
    return;

  gfxFontStyle fontStyle = *mFontGroup->GetStyle();
  fontStyle.size *= 0.8;
  nsRefPtr<gfxFontGroup> smallFont = mFontGroup->Copy(&fontStyle);
  if (!smallFont)
    return;

  gfxSkipChars dummy;
  gfxTextRunFactory::Parameters innerParams = GetParametersForInner(aTextRun, &dummy);

  PRUint32 length = aTextRun->GetLength();
  const PRUnichar* str = aTextRun->mString.BeginReading();
  nsRefPtr<nsStyleContext>* styles = aTextRun->mStyles.Elements();
  // Create a textrun so we can check cluster-start properties
  nsAutoPtr<gfxTextRun> inner;
  inner = mFontGroup->MakeTextRun(str, length, &innerParams);
  if (!inner)
    return;

  nsCaseTransformTextRunFactory uppercaseFactory(smallFont, nsnull, PR_TRUE);

  aTextRun->ResetGlyphRuns();

  PRUint32 runStart = 0;
  PRPackedBool runIsLowercase = PR_FALSE;
  nsAutoTArray<nsStyleContext*,50> styleArray;
  nsAutoTArray<PRPackedBool,50> canBreakBeforeArray;
  nsAutoTArray<PRUint32,10> lineBreakBeforeArray;

  PRUint32 nextLineBreak = 0;
  PRUint32 i;
  for (i = 0; i <= length; ++i) {
    if (nextLineBreak < aTextRun->mLineBreaks.Length() &&
        aTextRun->mLineBreaks[nextLineBreak] == i) {
      lineBreakBeforeArray.AppendElement(i - runStart);
      ++nextLineBreak;
    }

    PRBool isLowercase = PR_FALSE;
    if (i < length) {
      // Characters that aren't the start of a cluster are ignored here. They
      // get added to whatever lowercase/non-lowercase run we're in.
      if (!inner->IsClusterStart(i))
        continue;

      if (styles[i]->GetStyleFont()->mFont.variant == NS_STYLE_FONT_VARIANT_SMALL_CAPS) {
        PRUnichar ch = str[i];
        PRUnichar ch2;
        converter->ToUpper(ch, &ch2);
        isLowercase = ch != ch2 || ch == SZLIG;
      } else {
        // Don't transform the character! I.e., pretend that it's not lowercase
      }
    }

    if ((i == length || runIsLowercase != isLowercase) && runStart < i) {
      nsAutoPtr<gfxTextRun> child;
      // Setup actual line break data for child (which may affect shaping)
      innerParams.mInitialBreaks = lineBreakBeforeArray.Elements();
      innerParams.mInitialBreakCount = lineBreakBeforeArray.Length();
      if (runIsLowercase) {
        child = uppercaseFactory.MakeTextRun(str + runStart, i - runStart,
                                             &innerParams, styleArray.Elements());
      } else {
        child = mFontGroup->MakeTextRun(str + runStart, i - runStart, &innerParams);
      }
      if (!child)
        return;
      // Copy potential linebreaks into child so they're preserved
      // (and also child will be shaped appropriately)
      NS_ASSERTION(canBreakBeforeArray.Length() == i - runStart,
                   "lost some break-before values?");
      child->SetPotentialLineBreaks(0, canBreakBeforeArray.Length(), canBreakBeforeArray.Elements());
      AppendTextRun(aTextRun, child, runStart);

      runStart = i;
      runIsLowercase = isLowercase;
      styleArray.Clear();
      canBreakBeforeArray.Clear();
      lineBreakBeforeArray.Clear();
      if (nextLineBreak > 0 && aTextRun->mLineBreaks[nextLineBreak - 1] == i) {
        lineBreakBeforeArray.AppendElement(0);
      }
    }

    if (i < length) {
      styleArray.AppendElement(styles[i]);
      canBreakBeforeArray.AppendElement(aTextRun->CanBreakLineBefore(i));
    }
  }
  NS_ASSERTION(nextLineBreak == aTextRun->mLineBreaks.Length(),
               "lost track of line breaks somehow");
}

void
nsCaseTransformTextRunFactory::RebuildTextRun(nsTransformedTextRun* aTextRun)
{
  nsICaseConversion* converter = nsTextTransformer::GetCaseConv();
  if (!converter)
    return;

  PRUint32 length = aTextRun->GetLength();
  const PRUnichar* str = aTextRun->mString.BeginReading();
  nsRefPtr<nsStyleContext>* styles = aTextRun->mStyles.Elements();

  nsAutoString convertedString;
  nsAutoTArray<PRPackedBool,50> charsToMergeArray;
  nsAutoTArray<nsStyleContext*,50> styleArray;
  nsAutoTArray<PRPackedBool,50> canBreakBeforeArray;
  nsAutoTArray<PRUint32,10> lineBreakBeforeArray;
  PRUint32 nextLineBreak = 0;
  PRUint32 extraCharsCount = 0;

  PRUint32 i;
  for (i = 0; i < length; ++i) {
    PRUnichar ch = str[i];

    charsToMergeArray.AppendElement(PR_FALSE);
    styleArray.AppendElement(styles[i]);
    canBreakBeforeArray.AppendElement(aTextRun->CanBreakLineBefore(i));
    if (nextLineBreak < aTextRun->mLineBreaks.Length() &&
        aTextRun->mLineBreaks[nextLineBreak] == i) {
      lineBreakBeforeArray.AppendElement(i + extraCharsCount);
      ++nextLineBreak;
    }

    PRUint8 style = mAllUppercase ? NS_STYLE_TEXT_TRANSFORM_UPPERCASE
      : styles[i]->GetStyleText()->mTextTransform;
    PRBool extraChar = PR_FALSE;

    switch (style) {
    case NS_STYLE_TEXT_TRANSFORM_LOWERCASE:
      converter->ToLower(ch, &ch);
      break;
    case NS_STYLE_TEXT_TRANSFORM_UPPERCASE:
      if (ch == SZLIG) {
        convertedString.Append('S');
        extraChar = PR_TRUE;
        ch = 'S';
      } else {
        converter->ToUpper(ch, &ch);
      }
      break;
    case NS_STYLE_TEXT_TRANSFORM_CAPITALIZE:
      if (aTextRun->CanBreakLineBefore(i)) {
        if (ch == SZLIG) {
          convertedString.Append('S');
          extraChar = PR_TRUE;
          ch = 'S';
        } else {
          converter->ToTitle(ch, &ch);
        }
      }
      break;
    default:
      break;
    }

    convertedString.Append(ch);
    if (extraChar) {
      ++extraCharsCount;
      charsToMergeArray.AppendElement(PR_TRUE);
      styleArray.AppendElement(styles[i]);
      canBreakBeforeArray.AppendElement(PR_FALSE);
    }
  }
  if (nextLineBreak < aTextRun->mLineBreaks.Length() &&
      aTextRun->mLineBreaks[nextLineBreak] == length) {
    lineBreakBeforeArray.AppendElement(length + extraCharsCount);
    ++nextLineBreak;
  }
  NS_ASSERTION(nextLineBreak == aTextRun->mLineBreaks.Length(),
               "lost track of line breaks somehow");

  gfxSkipChars dummy;
  gfxTextRunFactory::Parameters innerParams = GetParametersForInner(aTextRun, &dummy);

  nsAutoPtr<gfxTextRun> child;
  // Setup actual line break data for child (which may affect shaping)
  innerParams.mInitialBreaks = lineBreakBeforeArray.Elements();
  innerParams.mInitialBreakCount = lineBreakBeforeArray.Length();
  if (mInnerTransformingTextRunFactory) {
    child = mInnerTransformingTextRunFactory->MakeTextRun(
        convertedString.BeginReading(), convertedString.Length(),
        &innerParams, styleArray.Elements());
  } else {
    child = mFontGroup->MakeTextRun(
        convertedString.BeginReading(), convertedString.Length(), &innerParams);
  }
  if (!child)
    return;
  // Copy potential linebreaks into child so they're preserved
  // (and also child will be shaped appropriately)
  NS_ASSERTION(convertedString.Length() == canBreakBeforeArray.Length(),
               "Dropped characters or break-before values somewhere!");
  child->SetPotentialLineBreaks(0, canBreakBeforeArray.Length(), canBreakBeforeArray.Elements());
  // Now merge multiple characters into one multi-glyph character as required
  MergeCharactersInTextRun(aTextRun, child, charsToMergeArray.Elements());
}

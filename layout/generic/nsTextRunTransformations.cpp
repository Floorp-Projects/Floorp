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

#include "nsTextFrameUtils.h"
#include "gfxSkipChars.h"

#include "nsStyleConsts.h"
#include "nsStyleContext.h"
#include "gfxContext.h"
#include "nsContentUtils.h"
#include "nsUnicharUtils.h"

#define SZLIG 0x00DF

nsTransformedTextRun *
nsTransformedTextRun::Create(const gfxTextRunFactory::Parameters* aParams,
                             nsTransformingTextRunFactory* aFactory,
                             gfxFontGroup* aFontGroup,
                             const PRUnichar* aString, PRUint32 aLength,
                             const PRUint32 aFlags, nsStyleContext** aStyles,
                             PRBool aOwnsFactory)
{
  NS_ASSERTION(!(aFlags & gfxTextRunFactory::TEXT_IS_8BIT),
               "didn't expect text to be marked as 8-bit here");

  // Note that AllocateStorage MAY modify the textPtr parameter,
  // if the text is not persistent and therefore a private copy is created
  const void *textPtr = aString;
  CompressedGlyph *glyphStorage = AllocateStorage(textPtr, aLength, aFlags);
  if (!glyphStorage) {
    return nsnull;
  }

  return new nsTransformedTextRun(aParams, aFactory, aFontGroup,
                                  static_cast<const PRUnichar*>(textPtr), aLength,
                                  aFlags, aStyles, aOwnsFactory, glyphStorage);
}

void
nsTransformedTextRun::SetCapitalization(PRUint32 aStart, PRUint32 aLength,
                                        PRPackedBool* aCapitalization,
                                        gfxContext* aRefContext)
{
  if (mCapitalize.IsEmpty()) {
    if (!mCapitalize.AppendElements(GetLength()))
      return;
    memset(mCapitalize.Elements(), 0, GetLength()*sizeof(PRPackedBool));
  }
  memcpy(mCapitalize.Elements() + aStart, aCapitalization, aLength*sizeof(PRPackedBool));
  mNeedsRebuild = PR_TRUE;
}

PRBool
nsTransformedTextRun::SetPotentialLineBreaks(PRUint32 aStart, PRUint32 aLength,
                                             PRUint8* aBreakBefore,
                                             gfxContext* aRefContext)
{
  PRBool changed = gfxTextRun::SetPotentialLineBreaks(aStart, aLength,
      aBreakBefore, aRefContext);
  if (changed) {
    mNeedsRebuild = PR_TRUE;
  }
  return changed;
}

nsTransformedTextRun*
nsTransformingTextRunFactory::MakeTextRun(const PRUnichar* aString, PRUint32 aLength,
                                          const gfxTextRunFactory::Parameters* aParams,
                                          gfxFontGroup* aFontGroup, PRUint32 aFlags,
                                          nsStyleContext** aStyles, PRBool aOwnsFactory)
{
  return nsTransformedTextRun::Create(aParams, this, aFontGroup,
                                      aString, aLength, aFlags, aStyles, aOwnsFactory);
}

nsTransformedTextRun*
nsTransformingTextRunFactory::MakeTextRun(const PRUint8* aString, PRUint32 aLength,
                                          const gfxTextRunFactory::Parameters* aParams,
                                          gfxFontGroup* aFontGroup, PRUint32 aFlags,
                                          nsStyleContext** aStyles, PRBool aOwnsFactory)
{
  // We'll only have a Unicode code path to minimize the amount of code needed
  // for these rarely used features
  NS_ConvertASCIItoUTF16 unicodeString(reinterpret_cast<const char*>(aString), aLength);
  return MakeTextRun(unicodeString.get(), aLength, aParams, aFontGroup,
                     aFlags & ~(gfxFontGroup::TEXT_IS_PERSISTENT | gfxFontGroup::TEXT_IS_8BIT),
                     aStyles, aOwnsFactory);
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
 * For simplicity, this produces a textrun containing all DetailedGlyphs,
 * no simple glyphs. So don't call it unless you really have merging to do.
 * 
 * @param aCharsToMerge when aCharsToMerge[i] is true, this character is
 * merged into the previous character
 */
static void
MergeCharactersInTextRun(gfxTextRun* aDest, gfxTextRun* aSrc,
                         PRPackedBool* aCharsToMerge)
{
  aDest->ResetGlyphRuns();

  gfxTextRun::GlyphRunIterator iter(aSrc, 0, aSrc->GetLength());
  PRUint32 offset = 0;
  nsAutoTArray<gfxTextRun::DetailedGlyph,2> glyphs;
  while (iter.NextRun()) {
    gfxTextRun::GlyphRun* run = iter.GetGlyphRun();
    nsresult rv = aDest->AddGlyphRun(run->mFont, run->mMatchType,
                                     offset, PR_FALSE);
    if (NS_FAILED(rv))
      return;

    PRBool anyMissing = PR_FALSE;
    PRUint32 mergeRunStart = iter.GetStringStart();
    PRUint32 k;
    for (k = iter.GetStringStart(); k < iter.GetStringEnd(); ++k) {
      gfxTextRun::CompressedGlyph g = aSrc->GetCharacterGlyphs()[k];
      if (g.IsSimpleGlyph()) {
        if (!anyMissing) {
          gfxTextRun::DetailedGlyph details;
          details.mGlyphID = g.GetSimpleGlyph();
          details.mAdvance = g.GetSimpleAdvance();
          details.mXOffset = 0;
          details.mYOffset = 0;
          glyphs.AppendElement(details);
        }
      } else {
        if (g.IsMissing()) {
          anyMissing = PR_TRUE;
          glyphs.Clear();
        }
        if (g.GetGlyphCount() > 0) {
          glyphs.AppendElements(aSrc->GetDetailedGlyphs(k), g.GetGlyphCount());
        }
      }

      // We could teach this method to handle merging of characters that aren't
      // cluster starts or ligature group starts, but this is really only used
      // to merge S's (uppercase &szlig;), so it's not worth it.

      if (k + 1 < iter.GetStringEnd() && aCharsToMerge[k + 1]) {
        NS_ASSERTION(g.IsClusterStart() && g.IsLigatureGroupStart(),
                     "Don't know how to merge this stuff");
        continue;
      }

      NS_ASSERTION(mergeRunStart == k ||
                   (g.IsClusterStart() && g.IsLigatureGroupStart()),
                   "Don't know how to merge this stuff");

      // If the start of the merge run is actually a character that should
      // have been merged with the previous character (this can happen
      // if there's a font change in the middle of a szlig, for example),
      // just discard the entire merge run. See comment at start of this
      // function.
      if (!aCharsToMerge[mergeRunStart]) {
        if (anyMissing) {
          g.SetMissing(glyphs.Length());
        } else {
          g.SetComplex(PR_TRUE, PR_TRUE, glyphs.Length());
        }
        aDest->SetGlyphs(offset, g, glyphs.Elements());
        ++offset;
      }

      glyphs.Clear();
      anyMissing = PR_FALSE;
      mergeRunStart = k + 1;
    }
    NS_ASSERTION(glyphs.Length() == 0,
                 "Leftover glyphs, don't request merging of the last character with its next!");  
  }
  NS_ASSERTION(offset == aDest->GetLength(), "Bad offset calculations");
}

static gfxTextRunFactory::Parameters
GetParametersForInner(nsTransformedTextRun* aTextRun, PRUint32* aFlags,
    gfxContext* aRefContext)
{
  gfxTextRunFactory::Parameters params =
    { aRefContext, nsnull, nsnull,
      nsnull, 0, aTextRun->GetAppUnitsPerDevUnit()
    };
  *aFlags = aTextRun->GetFlags() & ~gfxFontGroup::TEXT_IS_PERSISTENT;
  return params;
}

void
nsFontVariantTextRunFactory::RebuildTextRun(nsTransformedTextRun* aTextRun,
    gfxContext* aRefContext)
{
  gfxFontGroup* fontGroup = aTextRun->GetFontGroup();
  gfxFontStyle fontStyle = *fontGroup->GetStyle();
  fontStyle.size *= 0.8;
  nsRefPtr<gfxFontGroup> smallFont = fontGroup->Copy(&fontStyle);
  if (!smallFont)
    return;

  PRUint32 flags;
  gfxTextRunFactory::Parameters innerParams =
      GetParametersForInner(aTextRun, &flags, aRefContext);

  PRUint32 length = aTextRun->GetLength();
  const PRUnichar* str = aTextRun->GetTextUnicode();
  nsRefPtr<nsStyleContext>* styles = aTextRun->mStyles.Elements();
  // Create a textrun so we can check cluster-start properties
  gfxTextRunCache::AutoTextRun inner(
      gfxTextRunCache::MakeTextRun(str, length, fontGroup, &innerParams, flags));
  if (!inner.get())
    return;

  nsCaseTransformTextRunFactory uppercaseFactory(nsnull, PR_TRUE);

  aTextRun->ResetGlyphRuns();

  PRUint32 runStart = 0;
  PRBool runIsLowercase = PR_FALSE;
  nsAutoTArray<nsStyleContext*,50> styleArray;
  nsAutoTArray<PRUint8,50> canBreakBeforeArray;

  PRUint32 i;
  for (i = 0; i <= length; ++i) {
    PRBool isLowercase = PR_FALSE;
    if (i < length) {
      // Characters that aren't the start of a cluster are ignored here. They
      // get added to whatever lowercase/non-lowercase run we're in.
      if (!inner->IsClusterStart(i)) {
        isLowercase = runIsLowercase;
      } else {
        if (styles[i]->GetStyleFont()->mFont.variant == NS_STYLE_FONT_VARIANT_SMALL_CAPS) {
          PRUnichar ch = str[i];
          PRUnichar ch2;
          ch2 = ToUpperCase(ch);
          isLowercase = ch != ch2 || ch == SZLIG;
        } else {
          // Don't transform the character! I.e., pretend that it's not lowercase
        }
      }
    }

    if ((i == length || runIsLowercase != isLowercase) && runStart < i) {
      nsAutoPtr<nsTransformedTextRun> transformedChild;
      gfxTextRunCache::AutoTextRun cachedChild;
      gfxTextRun* child;

      if (runIsLowercase) {
        transformedChild = uppercaseFactory.MakeTextRun(str + runStart, i - runStart,
            &innerParams, smallFont, flags, styleArray.Elements(), PR_FALSE);
        child = transformedChild;
      } else {
        cachedChild =
          gfxTextRunCache::MakeTextRun(str + runStart, i - runStart, fontGroup,
              &innerParams, flags);
        child = cachedChild.get();
      }
      if (!child)
        return;
      // Copy potential linebreaks into child so they're preserved
      // (and also child will be shaped appropriately)
      NS_ASSERTION(canBreakBeforeArray.Length() == i - runStart,
                   "lost some break-before values?");
      child->SetPotentialLineBreaks(0, canBreakBeforeArray.Length(),
          canBreakBeforeArray.Elements(), aRefContext);
      if (transformedChild) {
        transformedChild->FinishSettingProperties(aRefContext);
      }
      aTextRun->CopyGlyphDataFrom(child, 0, child->GetLength(), runStart);

      runStart = i;
      styleArray.Clear();
      canBreakBeforeArray.Clear();
    }

    if (i < length) {
      runIsLowercase = isLowercase;
      styleArray.AppendElement(styles[i]);
      canBreakBeforeArray.AppendElement(aTextRun->CanBreakLineBefore(i));
    }
  }
}

void
nsCaseTransformTextRunFactory::RebuildTextRun(nsTransformedTextRun* aTextRun,
    gfxContext* aRefContext)
{
  PRUint32 length = aTextRun->GetLength();
  const PRUnichar* str = aTextRun->GetTextUnicode();
  nsRefPtr<nsStyleContext>* styles = aTextRun->mStyles.Elements();

  nsAutoString convertedString;
  nsAutoTArray<PRPackedBool,50> charsToMergeArray;
  nsAutoTArray<nsStyleContext*,50> styleArray;
  nsAutoTArray<PRUint8,50> canBreakBeforeArray;
  PRUint32 extraCharsCount = 0;

  PRUint32 i;
  for (i = 0; i < length; ++i) {
    PRUnichar ch = str[i];

    charsToMergeArray.AppendElement(PR_FALSE);
    styleArray.AppendElement(styles[i]);
    canBreakBeforeArray.AppendElement(aTextRun->CanBreakLineBefore(i));

    PRUint8 style = mAllUppercase ? NS_STYLE_TEXT_TRANSFORM_UPPERCASE
      : styles[i]->GetStyleText()->mTextTransform;
    PRBool extraChar = PR_FALSE;

    switch (style) {
    case NS_STYLE_TEXT_TRANSFORM_LOWERCASE:
      ch = ToLowerCase(ch);
      break;
    case NS_STYLE_TEXT_TRANSFORM_UPPERCASE:
      if (ch == SZLIG) {
        convertedString.Append('S');
        extraChar = PR_TRUE;
        ch = 'S';
      } else {
        ch = ToUpperCase(ch);
      }
      break;
    case NS_STYLE_TEXT_TRANSFORM_CAPITALIZE:
      if (i < aTextRun->mCapitalize.Length() && aTextRun->mCapitalize[i]) {
        if (ch == SZLIG) {
          convertedString.Append('S');
          extraChar = PR_TRUE;
          ch = 'S';
        } else {
          ch = ToTitleCase(ch);
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

  PRUint32 flags;
  gfxTextRunFactory::Parameters innerParams =
      GetParametersForInner(aTextRun, &flags, aRefContext);
  gfxFontGroup* fontGroup = aTextRun->GetFontGroup();

  nsAutoPtr<nsTransformedTextRun> transformedChild;
  gfxTextRunCache::AutoTextRun cachedChild;
  gfxTextRun* child;

  if (mInnerTransformingTextRunFactory) {
    transformedChild = mInnerTransformingTextRunFactory->MakeTextRun(
        convertedString.BeginReading(), convertedString.Length(),
        &innerParams, fontGroup, flags, styleArray.Elements(), PR_FALSE);
    child = transformedChild.get();
  } else {
    cachedChild = gfxTextRunCache::MakeTextRun(
        convertedString.BeginReading(), convertedString.Length(), fontGroup,
        &innerParams, flags);
    child = cachedChild.get();
  }
  if (!child)
    return;
  // Copy potential linebreaks into child so they're preserved
  // (and also child will be shaped appropriately)
  NS_ASSERTION(convertedString.Length() == canBreakBeforeArray.Length(),
               "Dropped characters or break-before values somewhere!");
  child->SetPotentialLineBreaks(0, canBreakBeforeArray.Length(),
      canBreakBeforeArray.Elements(), aRefContext);
  if (transformedChild) {
    transformedChild->FinishSettingProperties(aRefContext);
  }

  if (extraCharsCount > 0) {
    // Now merge multiple characters into one multi-glyph character as required
    MergeCharactersInTextRun(aTextRun, child, charsToMergeArray.Elements());
  } else {
    // No merging to do, so just copy; this produces a more optimized textrun.
    // We can't steal the data because the child may be cached and stealing
    // the data would break the cache.
    aTextRun->ResetGlyphRuns();
    aTextRun->CopyGlyphDataFrom(child, 0, child->GetLength(), 0);
  }
}

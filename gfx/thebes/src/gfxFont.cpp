/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Foundation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart@mozilla.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
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

#include "nsIPref.h"
#include "nsServiceManagerUtils.h"
#include "nsReadableUtils.h"

#include "gfxFont.h"
#include "gfxPlatform.h"

#include "prtypes.h"
#include "gfxTypes.h"
#include "gfxContext.h"

#include "cairo.h"
#include "gfxFontTest.h"

#include "nsCRT.h"

gfxFont::gfxFont(const nsAString &aName, const gfxFontStyle *aFontStyle) :
    mName(aName), mStyle(aFontStyle)
{
}

void
gfxFont::Draw(gfxTextRun *aTextRun, PRUint32 aStart, PRUint32 aEnd,
              gfxContext *aContext, PRBool aDrawToPath, gfxPoint *aPt,
              Spacing *aSpacing)
{
    if (aStart >= aEnd)
        return;

    double appUnitsToPixels = 1.0/aTextRun->GetAppUnitsPerDevUnit();
    const gfxTextRun::CompressedGlyph *charGlyphs = aTextRun->GetCharacterGlyphs();
    double direction = aTextRun->GetDirection();

    nsAutoTArray<cairo_glyph_t,200> glyphBuffer;
    PRUint32 i;
    double x = aPt->x;
    double y = aPt->y;

    PRBool isRTL = aTextRun->IsRightToLeft();

    if (aSpacing) {
        x += direction*aSpacing[0].mBefore*appUnitsToPixels;
    }
    for (i = aStart; i < aEnd; ++i) {
        const gfxTextRun::CompressedGlyph *glyphData = &charGlyphs[i];
        if (glyphData->IsSimpleGlyph()) {
            cairo_glyph_t *glyph = glyphBuffer.AppendElement();
            if (!glyph)
                return;
            glyph->index = glyphData->GetSimpleGlyph();
            double advance = glyphData->GetSimpleAdvance();
            glyph->x = x;
            glyph->y = y;
            if (isRTL) {
                glyph->x -= advance;
                x -= advance;
            } else {
                x += advance;
            }
        } else if (glyphData->IsComplexCluster()) {
            const gfxTextRun::DetailedGlyph *details = aTextRun->GetDetailedGlyphs(i);
            for (;;) {
                cairo_glyph_t *glyph = glyphBuffer.AppendElement();
                if (!glyph)
                    return;
                glyph->index = details->mGlyphID;
                glyph->x = x + details->mXOffset;
                glyph->y = y + details->mYOffset;
                double advance = details->mAdvance;
                if (isRTL) {
                    glyph->x -= advance;
                }
                x += direction*advance;
                if (details->mIsLastGlyph)
                    break;
                ++details;
            }
        }
        // Every other glyph type (including missing glyphs) is ignored
        if (aSpacing) {
            double space = aSpacing[i - aStart].mAfter;
            if (i + 1 < aEnd) {
                space += aSpacing[i + 1 - aStart].mBefore;
            }
            x += direction*space*appUnitsToPixels;
        }
    }

    *aPt = gfxPoint(x, y);

    // XXX is this needed? what does it do? Mac code was doing it.
    // gfxFloat offsetX, offsetY;
    // nsRefPtr<gfxASurface> surf = aContext->CurrentSurface (&offsetX, &offsetY);

    cairo_t *cr = aContext->GetCairo();
    SetupCairoFont(cr);
    if (aDrawToPath) {
        cairo_glyph_path(cr, glyphBuffer.Elements(), glyphBuffer.Length());
    } else {
        cairo_show_glyphs(cr, glyphBuffer.Elements(), glyphBuffer.Length());
    }

    if (gfxFontTestStore::CurrentStore()) {
        gfxFontTestStore::CurrentStore()->AddItem(GetUniqueName(),
                                                  glyphBuffer.Elements(), glyphBuffer.Length());
    }
}

gfxFont::RunMetrics
gfxFont::Measure(gfxTextRun *aTextRun,
                 PRUint32 aStart, PRUint32 aEnd,
                 PRBool aTightBoundingBox,
                 Spacing *aSpacing)
{
    // XXX temporary code, does not handle glyphs outside the font-box
    NS_ASSERTION(!(aTextRun->GetFlags() & gfxTextRunFactory::TEXT_NEED_BOUNDING_BOX),
                 "Glyph extents not yet supported");
    gfxFloat advancePixels = 0;
    PRUint32 i;
    const gfxTextRun::CompressedGlyph *charGlyphs = aTextRun->GetCharacterGlyphs();
    PRUint32 clusterCount = 0;
    for (i = aStart; i < aEnd; ++i) {
        gfxTextRun::CompressedGlyph g = charGlyphs[i];
        if (g.IsClusterStart()) {
            ++clusterCount;
            if (g.IsSimpleGlyph()) {
                advancePixels += charGlyphs[i].GetSimpleAdvance();
            } else if (g.IsComplexCluster()) {
                const gfxTextRun::DetailedGlyph *details = aTextRun->GetDetailedGlyphs(i);
                for (;;) {
                    advancePixels += details->mAdvance;
                    if (details->mIsLastGlyph)
                        break;
                    ++details;
                }
            }
        }
    }
    gfxFloat dev2app = aTextRun->GetAppUnitsPerDevUnit();
    gfxFloat advance = advancePixels*dev2app;
    if (aSpacing) {
        for (i = 0; i < aEnd - aStart; ++i) {
            advance += aSpacing[i].mBefore + aSpacing[i].mAfter;
        }
    }
    RunMetrics metrics;
    const gfxFont::Metrics& fontMetrics = GetMetrics();
    metrics.mAdvanceWidth = advance;
    metrics.mAscent = fontMetrics.maxAscent*dev2app;
    metrics.mDescent = fontMetrics.maxDescent*dev2app;
    metrics.mBoundingBox =
        gfxRect(0, -metrics.mAscent, advance, metrics.mAscent + metrics.mDescent);
    metrics.mClusterCount = clusterCount;
    return metrics;
}

gfxFontGroup::gfxFontGroup(const nsAString& aFamilies, const gfxFontStyle *aStyle)
    : mFamilies(aFamilies), mStyle(*aStyle)
{

}

PRBool
gfxFontGroup::ForEachFont(FontCreationCallback fc,
                          void *closure)
{
    return ForEachFontInternal(mFamilies, mStyle.langGroup,
                               PR_TRUE, fc, closure);
}

PRBool
gfxFontGroup::ForEachFont(const nsAString& aFamilies,
                          const nsACString& aLangGroup,
                          FontCreationCallback fc,
                          void *closure)
{
    return ForEachFontInternal(aFamilies, aLangGroup,
                               PR_FALSE, fc, closure);
}

struct ResolveData {
    ResolveData(gfxFontGroup::FontCreationCallback aCallback,
                nsACString& aGenericFamily,
                void *aClosure) :
        mCallback(aCallback),
        mGenericFamily(aGenericFamily),
        mClosure(aClosure) {
    };
    gfxFontGroup::FontCreationCallback mCallback;
    nsCString mGenericFamily;
    void *mClosure;
};

PRBool
gfxFontGroup::ForEachFontInternal(const nsAString& aFamilies,
                                  const nsACString& aLangGroup,
                                  PRBool aResolveGeneric,
                                  FontCreationCallback fc,
                                  void *closure)
{
    const PRUnichar kSingleQuote  = PRUnichar('\'');
    const PRUnichar kDoubleQuote  = PRUnichar('\"');
    const PRUnichar kComma        = PRUnichar(',');

    nsCOMPtr<nsIPref> prefs;
    prefs = do_GetService(NS_PREF_CONTRACTID);

    nsPromiseFlatString families(aFamilies);
    const PRUnichar *p, *p_end;
    families.BeginReading(p);
    families.EndReading(p_end);
    nsAutoString family;
    nsCAutoString lcFamily;
    nsAutoString genericFamily;
    nsCAutoString lang(aLangGroup);
    if (lang.IsEmpty())
        lang.Assign("x-unicode"); // XXX or should use "x-user-def"?

    while (p < p_end) {
        while (nsCRT::IsAsciiSpace(*p))
            if (++p == p_end)
                return PR_TRUE;

        PRBool generic;
        if (*p == kSingleQuote || *p == kDoubleQuote) {
            // quoted font family
            PRUnichar quoteMark = *p;
            if (++p == p_end)
                return PR_TRUE;
            const PRUnichar *nameStart = p;

            // XXX What about CSS character escapes?
            while (*p != quoteMark)
                if (++p == p_end)
                    return PR_TRUE;

            family = Substring(nameStart, p);
            generic = PR_FALSE;
            genericFamily.SetIsVoid(PR_TRUE);

            while (++p != p_end && *p != kComma)
                /* nothing */ ;

        } else {
            // unquoted font family
            const PRUnichar *nameStart = p;
            while (++p != p_end && *p != kComma)
                /* nothing */ ;

            family = Substring(nameStart, p);
            family.CompressWhitespace(PR_FALSE, PR_TRUE);

            if (aResolveGeneric &&
                (family.LowerCaseEqualsLiteral("serif") ||
                 family.LowerCaseEqualsLiteral("sans-serif") ||
                 family.LowerCaseEqualsLiteral("monospace") ||
                 family.LowerCaseEqualsLiteral("cursive") ||
                 family.LowerCaseEqualsLiteral("fantasy")))
            {
                generic = PR_TRUE;

                ToLowerCase(NS_LossyConvertUTF16toASCII(family), lcFamily);

                nsCAutoString prefName("font.name.");
                prefName.Append(lcFamily);
                prefName.AppendLiteral(".");
                prefName.Append(lang);

                // prefs file always uses (must use) UTF-8 so that we can use
                // |GetCharPref| and treat the result as a UTF-8 string.
                nsXPIDLString value;
                nsresult rv = prefs->CopyUnicharPref(prefName.get(), getter_Copies(value));
                if (NS_SUCCEEDED(rv)) {
                    CopyASCIItoUTF16(lcFamily, genericFamily);
                    family = value;
                }
            } else {
                generic = PR_FALSE;
                genericFamily.SetIsVoid(PR_TRUE);
            }
        }
        
        if (!family.IsEmpty()) {
            NS_LossyConvertUTF16toASCII gf(genericFamily);
            ResolveData data(fc, gf, closure);
            PRBool aborted;
            gfxPlatform *pf = gfxPlatform::GetPlatform();
            nsresult rv = pf->ResolveFontName(family,
                                              gfxFontGroup::FontResolverProc,
                                              &data, aborted);
            if (NS_FAILED(rv) || aborted)
                return PR_FALSE;
        }

        if (generic && aResolveGeneric) {
            nsCAutoString prefName("font.name-list.");
            prefName.Append(lcFamily);
            prefName.AppendLiteral(".");
            prefName.Append(aLangGroup);
            nsXPIDLString value;
            nsresult rv = prefs->CopyUnicharPref(prefName.get(), getter_Copies(value));
            if (NS_SUCCEEDED(rv)) {
                ForEachFontInternal(value, lang, PR_FALSE, fc, closure);
            }
        }

        ++p; // may advance past p_end
    }

    return PR_TRUE;
}

PRBool
gfxFontGroup::FontResolverProc(const nsAString& aName, void *aClosure)
{
    ResolveData *data = reinterpret_cast<ResolveData*>(aClosure);
    return (data->mCallback)(aName, data->mGenericFamily, data->mClosure);
}

void
gfxFontGroup::FindGenericFontFromStyle(FontCreationCallback fc,
                                       void *closure)
{
    nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID));
    if (!prefs)
        return;

    nsCAutoString prefName;
    nsXPIDLString genericName;
    nsXPIDLString familyName;

    // add the default font to the end of the list
    prefName.AssignLiteral("font.default.");
    prefName.Append(mStyle.langGroup);
    nsresult rv = prefs->CopyUnicharPref(prefName.get(), getter_Copies(genericName));
    if (NS_SUCCEEDED(rv)) {
        prefName.AssignLiteral("font.name.");
        prefName.Append(NS_LossyConvertUTF16toASCII(genericName));
        prefName.AppendLiteral(".");
        prefName.Append(mStyle.langGroup);

        rv = prefs->CopyUnicharPref(prefName.get(), getter_Copies(familyName));
        if (NS_SUCCEEDED(rv)) {
            ForEachFontInternal(familyName, mStyle.langGroup,
                                PR_FALSE, fc, closure);
        }

        prefName.AssignLiteral("font.name-list.");
        prefName.Append(NS_LossyConvertUTF16toASCII(genericName));
        prefName.AppendLiteral(".");
        prefName.Append(mStyle.langGroup);

        rv = prefs->CopyUnicharPref(prefName.get(), getter_Copies(familyName));
        if (NS_SUCCEEDED(rv)) {
            ForEachFontInternal(familyName, mStyle.langGroup,
                                PR_FALSE, fc, closure);
        }
    }
}

gfxTextRun *
gfxFontGroup::GetSpecialStringTextRun(SpecialString aString,
                                      gfxTextRun *aTemplate)
{
    NS_ASSERTION(aString <= gfxFontGroup::STRING_MAX,
                 "Bad special string index");

    if (mSpecialStrings[aString] &&
        mSpecialStrings[aString]->GetAppUnitsPerDevUnit() ==
            aTemplate->GetAppUnitsPerDevUnit())
        return mSpecialStrings[aString];

    static const PRUnichar unicodeHyphen = 0x2010;
    static const PRUnichar unicodeEllipsis = 0x2026;
    static const PRUint8 space = ' ';

    gfxTextRunFactory::Parameters params = {
        nsnull, nsnull, nsnull, nsnull, nsnull, 0,
        PRUint32(aTemplate->GetAppUnitsPerDevUnit()), TEXT_IS_PERSISTENT
    };
    gfxTextRun* textRun;

    switch (aString) {
        case STRING_ELLIPSIS:
            textRun = MakeTextRun(&unicodeHyphen, 1, &params);
            break;
        case STRING_HYPHEN:
            textRun = MakeTextRun(&unicodeEllipsis, 1, &params);
            break;
        default:
        case STRING_SPACE:
            textRun = MakeTextRun(&space, 1, &params);
            break;
    }
    if (!textRun)
        return nsnull;
    
    if (textRun->CountMissingGlyphs() > 0) {
        static const PRUint8 ASCIIEllipsis[] = {'.', '.', '.'};
        static const PRUint8 ASCIIHyphen = '-';

        switch (aString) {
            case STRING_ELLIPSIS:
                textRun = MakeTextRun(ASCIIEllipsis, NS_ARRAY_LENGTH(ASCIIEllipsis), &params);
                break;
            case STRING_HYPHEN:
                textRun = MakeTextRun(&ASCIIHyphen, 1, &params);
                break;
            default:
                NS_WARNING("This font doesn't support the space character? That's messed up");
                break;
        }
    }

    mSpecialStrings[aString] = textRun;
    return textRun;
}

gfxFontStyle::gfxFontStyle(PRUint8 aStyle, PRUint8 aVariant,
                           PRUint16 aWeight, PRUint8 aDecoration, gfxFloat aSize,
                           const nsACString& aLangGroup,
                           float aSizeAdjust, PRPackedBool aSystemFont,
                           PRPackedBool aFamilyNameQuirks) :
    style(aStyle), systemFont(aSystemFont), variant(aVariant),
    familyNameQuirks(aFamilyNameQuirks), weight(aWeight),
    decorations(aDecoration), size(PR_MIN(aSize, 5000)), langGroup(aLangGroup), sizeAdjust(aSizeAdjust)
{
    if (weight > 900)
        weight = 900;
    if (weight < 100)
        weight = 100;

    if (langGroup.IsEmpty()) {
        NS_WARNING("empty langgroup");
        langGroup.Assign("x-western");
    }
}

void
gfxFontStyle::ComputeWeightAndOffset(PRInt8 *outBaseWeight, PRInt8 *outOffset) const
{
    PRInt8 baseWeight = (weight + 50) / 100;
    PRInt8 offset = weight - baseWeight * 100;

    if (baseWeight < 0)
        baseWeight = 0;
    if (baseWeight > 9)
        baseWeight = 9;

    if (outBaseWeight)
        *outBaseWeight = baseWeight;
    if (outOffset)
        *outOffset = offset;
}

PRBool
gfxTextRun::GlyphRunIterator::NextRun()  {
    if (mNextIndex >= mTextRun->mGlyphRuns.Length())
        return PR_FALSE;
    mGlyphRun = &mTextRun->mGlyphRuns[mNextIndex];
    if (mGlyphRun->mCharacterOffset >= mEndOffset)
        return PR_FALSE;

    mStringStart = PR_MAX(mStartOffset, mGlyphRun->mCharacterOffset);
    PRUint32 last = mNextIndex + 1 < mTextRun->mGlyphRuns.Length()
        ? mTextRun->mGlyphRuns[mNextIndex + 1].mCharacterOffset : mTextRun->mCharacterCount;
    mStringEnd = PR_MIN(mEndOffset, last);

    ++mNextIndex;
    return PR_TRUE;
}

gfxTextRun::gfxTextRun(gfxTextRunFactory::Parameters *aParams,
                       PRUint32 aLength)
  : mUserData(aParams->mUserData),
    mAppUnitsPerDevUnit(aParams->mAppUnitsPerDevUnit),
    mFlags(aParams->mFlags),
    mCharacterCount(aLength)
{
    if (aParams->mSkipChars) {
        mSkipChars.TakeFrom(aParams->mSkipChars);
    }
    mCharacterGlyphs = new CompressedGlyph[aLength];
    if (mCharacterGlyphs) {
        memset(mCharacterGlyphs, 0, sizeof(CompressedGlyph)*aLength);
    }
}

PRBool
gfxTextRun::SetPotentialLineBreaks(PRUint32 aStart, PRUint32 aLength,
                                   PRPackedBool *aBreakBefore)
{
    NS_ASSERTION(aStart + aLength <= mCharacterCount, "Overflow");

    if (!mCharacterGlyphs)
        return PR_TRUE;
    PRUint32 changed = 0;
    PRUint32 i;
    for (i = 0; i < aLength; ++i) {
        NS_ASSERTION(!aBreakBefore[i] ||
                     mCharacterGlyphs[aStart + i].IsClusterStart(),
                     "Break suggested inside cluster!");
        changed |= mCharacterGlyphs[aStart + i].SetCanBreakBefore(aBreakBefore[i]);
    }
    return changed != 0;
}

gfxFloat
gfxTextRun::ComputeClusterAdvance(PRUint32 aClusterOffset)
{
    CompressedGlyph *glyphData = &mCharacterGlyphs[aClusterOffset];
    if (glyphData->IsSimpleGlyph())
        return glyphData->GetSimpleAdvance();
    NS_ASSERTION(glyphData->IsComplexCluster(), "Unknown character type!");
    NS_ASSERTION(mDetailedGlyphs, "Complex cluster but no details array!");
    gfxFloat advance = 0;
    DetailedGlyph *details = mDetailedGlyphs[aClusterOffset];
    NS_ASSERTION(details, "Complex cluster but no details!");
    for (;;) {
        advance += details->mAdvance;
        if (details->mIsLastGlyph)
            return advance;
        ++details;
    }
}

gfxTextRun::LigatureData
gfxTextRun::ComputeLigatureData(PRUint32 aPartOffset, PropertyProvider *aProvider)
{
    LigatureData result;

    PRUint32 ligStart = aPartOffset;
    CompressedGlyph *charGlyphs = mCharacterGlyphs;
    while (charGlyphs[ligStart].IsLigatureContinuation()) {
        do {
            NS_ASSERTION(ligStart > 0, "Ligature at the start of the run??");
            --ligStart;
        } while (!charGlyphs[ligStart].IsClusterStart());
    }
    result.mStartOffset = ligStart;
    result.mLigatureWidth = ComputeClusterAdvance(ligStart)*mAppUnitsPerDevUnit;
    result.mPartClusterIndex = PR_UINT32_MAX;

    PRUint32 charIndex = ligStart;
    // Count the number of started clusters we have seen
    PRUint32 clusterCount = 0;
    while (charIndex < mCharacterCount) {
        if (charIndex == aPartOffset) {
            result.mPartClusterIndex = clusterCount;
        }
        if (mCharacterGlyphs[charIndex].IsClusterStart()) {
            if (charIndex > ligStart &&
                !mCharacterGlyphs[charIndex].IsLigatureContinuation())
                break;
            ++clusterCount;
        }
        ++charIndex;
    }
    result.mClusterCount = clusterCount;
    result.mEndOffset = charIndex;

    if (aProvider && (mFlags & gfxTextRunFactory::TEXT_ENABLE_SPACING)) {
        PropertyProvider::Spacing spacing;
        aProvider->GetSpacing(ligStart, 1, &spacing);
        result.mBeforeSpacing = spacing.mBefore;
        aProvider->GetSpacing(charIndex - 1, 1, &spacing);
        result.mAfterSpacing = spacing.mAfter;
    } else {
        result.mBeforeSpacing = result.mAfterSpacing = 0;
    }

    NS_ASSERTION(result.mPartClusterIndex < PR_UINT32_MAX, "Didn't find cluster part???");
    return result;
}

void
gfxTextRun::GetAdjustedSpacing(PRUint32 aStart, PRUint32 aEnd,
                               PropertyProvider *aProvider,
                               PropertyProvider::Spacing *aSpacing)
{
    if (aStart >= aEnd)
        return;

    aProvider->GetSpacing(aStart, aEnd - aStart, aSpacing);

    // XXX the following loop could be avoided if we add some kind of
    // TEXT_HAS_LIGATURES flag
    CompressedGlyph *charGlyphs = mCharacterGlyphs;
    PRUint32 i;
    PRUint32 end = PR_MIN(aEnd, mCharacterCount - 1);
    for (i = aStart; i <= end; ++i) {
        if (charGlyphs[i].IsLigatureContinuation()) {
            if (i < aEnd) {
                aSpacing[i - aStart].mBefore = 0;
            }
            if (i > aStart) {
                aSpacing[i - 1 - aStart].mAfter = 0;
            }
        }
    }
    
    if (mFlags & gfxTextRunFactory::TEXT_ABSOLUTE_SPACING) {
        // Subtract character widths from mAfter at the end of clusters/ligatures to
        // relativize spacing. This is a bit sad since we're going to add
        // them in again below when we actually use the spacing, but this
        // produces simpler code and absolute spacing is rarely required.
        
        // The width of the last nonligature cluster, in appunits
        gfxFloat clusterWidth = 0.0;
        for (i = aStart; i < aEnd; ++i) {
            CompressedGlyph *glyphData = &charGlyphs[i];
            
            if (glyphData->IsSimpleGlyph()) {
                if (i > aStart) {
                    aSpacing[i - 1 - aStart].mAfter -= clusterWidth;
                }
                clusterWidth = glyphData->GetSimpleAdvance()*mAppUnitsPerDevUnit;
            } else if (glyphData->IsComplexCluster()) {
                NS_ASSERTION(mDetailedGlyphs, "No details but we have a complex cluster...");
                if (i > aStart) {
                    aSpacing[i - 1 - aStart].mAfter -= clusterWidth;
                }
                DetailedGlyph *details = mDetailedGlyphs[i];
                clusterWidth = 0.0;
                for (;;) {
                    clusterWidth += details->mAdvance;
                    if (details->mIsLastGlyph)
                        break;
                    ++details;
                }
                clusterWidth *= mAppUnitsPerDevUnit;
            }
        }
        aSpacing[aEnd - 1 - aStart].mAfter -= clusterWidth;
    }
}

PRBool
gfxTextRun::GetAdjustedSpacingArray(PRUint32 aStart, PRUint32 aEnd,
                                    PropertyProvider *aProvider,
                                    nsTArray<PropertyProvider::Spacing> *aSpacing)
{
    if (!(mFlags & gfxTextRunFactory::TEXT_ENABLE_SPACING))
        return PR_FALSE;
    if (!aSpacing->AppendElements(aEnd - aStart))
        return PR_FALSE;
    GetAdjustedSpacing(aStart, aEnd, aProvider, aSpacing->Elements());
    return PR_TRUE;
}

void
gfxTextRun::ShrinkToLigatureBoundaries(PRUint32 *aStart, PRUint32 *aEnd)
{
    if (*aStart >= *aEnd)
        return;
  
    CompressedGlyph *charGlyphs = mCharacterGlyphs;

    NS_ASSERTION(charGlyphs[*aStart].IsClusterStart(),
                 "Started in the middle of a cluster...");
    NS_ASSERTION(*aEnd == mCharacterCount || charGlyphs[*aEnd].IsClusterStart(),
                 "Ended in the middle of a cluster...");

    if (charGlyphs[*aStart].IsLigatureContinuation()) {
        LigatureData data = ComputeLigatureData(*aStart, nsnull);
        *aStart = PR_MIN(*aEnd, data.mEndOffset);
    }
    if (*aEnd < mCharacterCount && charGlyphs[*aEnd].IsLigatureContinuation()) {
        LigatureData data = ComputeLigatureData(*aEnd, nsnull);
        // We may be ending in the same ligature as we started in, in which case
        // we want to make *aEnd == *aStart because the range between ligatures
        // should be empty.
        *aEnd = PR_MAX(*aStart, data.mStartOffset);
    }
}

void
gfxTextRun::DrawGlyphs(gfxFont *aFont, gfxContext *aContext,
                       PRBool aDrawToPath, gfxPoint *aPt,
                       PRUint32 aStart, PRUint32 aEnd,
                       PropertyProvider *aProvider)
{
    nsAutoTArray<PropertyProvider::Spacing,200> spacingBuffer;
    PRBool haveSpacing = GetAdjustedSpacingArray(aStart, aEnd, aProvider, &spacingBuffer);
    aFont->Draw(this, aStart, aEnd, aContext, aDrawToPath, aPt,
                haveSpacing ? spacingBuffer.Elements() : nsnull);
}

gfxFloat
gfxTextRun::GetPartialLigatureWidth(PRUint32 aStart, PRUint32 aEnd,
                                    PropertyProvider *aProvider)
{
    if (aStart >= aEnd)
        return 0;

    LigatureData data = ComputeLigatureData(aStart, aProvider);
    PRUint32 clusterCount = 0;
    PRUint32 i;
    for (i = aStart; i < aEnd; ++i) {
        if (mCharacterGlyphs[i].IsClusterStart()) {
            ++clusterCount;
        }
    }

    gfxFloat result = data.mLigatureWidth*clusterCount/data.mClusterCount;
    if (aStart == data.mStartOffset) {
        result += data.mBeforeSpacing;
    }
    if (aEnd == data.mEndOffset) {
        result += data.mAfterSpacing;
    }
    return result;
}

void
gfxTextRun::DrawPartialLigature(gfxFont *aFont, gfxContext *aCtx, PRUint32 aOffset,
                                const gfxRect *aDirtyRect, gfxPoint *aPt,
                                PropertyProvider *aProvider)
{
    NS_ASSERTION(aDirtyRect, "Cannot draw partial ligatures without a dirty rect");

    if (!mCharacterGlyphs[aOffset].IsClusterStart() || !aDirtyRect)
        return;

    gfxFloat appUnitsToPixels = 1.0/mAppUnitsPerDevUnit;

    // Draw partial ligature. We hack this by clipping the ligature.
    LigatureData data = ComputeLigatureData(aOffset, aProvider);
    // Width of a cluster in the ligature, in device pixels
    gfxFloat clusterWidth = data.mLigatureWidth*appUnitsToPixels/data.mClusterCount;

    gfxFloat direction = GetDirection();
    gfxFloat left = aDirtyRect->X()*appUnitsToPixels;
    gfxFloat right = aDirtyRect->XMost()*appUnitsToPixels;
    // The advance to the start of this cluster in the drawn ligature, in device pixels
    gfxFloat widthBeforeCluster;
    // Any spacing that should be included after the cluster, in device pixels
    gfxFloat afterSpace;
    if (data.mStartOffset < aOffset) {
        // Not the start of the ligature; need to clip the ligature before the current cluster
        if (IsRightToLeft()) {
            right = PR_MIN(right, aPt->x);
        } else {
            left = PR_MAX(left, aPt->x);
        }
        widthBeforeCluster = clusterWidth*data.mPartClusterIndex +
            data.mBeforeSpacing*appUnitsToPixels;
    } else {
        // We're drawing the start of the ligature, so our cluster includes any
        // before-spacing.
        widthBeforeCluster = 0;
    }
    if (aOffset < data.mEndOffset) {
        // Not the end of the ligature; need to clip the ligature after the current cluster
        gfxFloat endEdge = aPt->x + clusterWidth;
        if (IsRightToLeft()) {
            left = PR_MAX(left, endEdge);
        } else {
            right = PR_MIN(right, endEdge);
        }
        afterSpace = 0;
    } else {
        afterSpace = data.mAfterSpacing*appUnitsToPixels;
    }

    aCtx->Save();
    aCtx->Clip(gfxRect(left, aDirtyRect->Y()*appUnitsToPixels, right - left,
               aDirtyRect->Height()*appUnitsToPixels));
    gfxPoint pt(aPt->x - direction*widthBeforeCluster, aPt->y);
    DrawGlyphs(aFont, aCtx, PR_FALSE, &pt, data.mStartOffset,
               data.mEndOffset, aProvider);
    aCtx->Restore();

    aPt->x += direction*(clusterWidth + afterSpace);
}

void
gfxTextRun::Draw(gfxContext *aContext, gfxPoint aPt,
                 PRUint32 aStart, PRUint32 aLength, const gfxRect *aDirtyRect,
                 PropertyProvider *aProvider, gfxFloat *aAdvanceWidth)
{
    NS_ASSERTION(aStart + aLength <= mCharacterCount, "Substring out of range");

    gfxFloat appUnitsToPixels = 1/mAppUnitsPerDevUnit;
    CompressedGlyph *charGlyphs = mCharacterGlyphs;
    gfxFloat direction = GetDirection();

    gfxPoint pt(aPt.x*appUnitsToPixels, aPt.y*appUnitsToPixels);
    gfxFloat startX = pt.x;

    GlyphRunIterator iter(this, aStart, aLength);
    while (iter.NextRun()) {
        gfxFont *font = iter.GetGlyphRun()->mFont;
        PRUint32 start = iter.GetStringStart();
        PRUint32 end = iter.GetStringEnd();
        NS_ASSERTION(charGlyphs[start].IsClusterStart(),
                     "Started drawing in the middle of a cluster...");
        NS_ASSERTION(end == mCharacterCount || charGlyphs[end].IsClusterStart(),
                     "Ended drawing in the middle of a cluster...");

        PRUint32 ligatureRunStart = start;
        PRUint32 ligatureRunEnd = end;
        ShrinkToLigatureBoundaries(&ligatureRunStart, &ligatureRunEnd);

        PRUint32 i;
        for (i = start; i < ligatureRunStart; ++i) {
            DrawPartialLigature(font, aContext, i, aDirtyRect, &pt, aProvider);
        }

        DrawGlyphs(font, aContext, PR_FALSE, &pt, ligatureRunStart,
                   ligatureRunEnd, aProvider);

        for (i = ligatureRunEnd; i < end; ++i) {
            DrawPartialLigature(font, aContext, i, aDirtyRect, &pt, aProvider);
        }
    }

    if (aAdvanceWidth) {
        *aAdvanceWidth = (pt.x - startX)*direction*mAppUnitsPerDevUnit;
    }
}

void
gfxTextRun::DrawToPath(gfxContext *aContext, gfxPoint aPt,
                       PRUint32 aStart, PRUint32 aLength,
                       PropertyProvider *aProvider, gfxFloat *aAdvanceWidth)
{
    NS_ASSERTION(aStart + aLength <= mCharacterCount, "Substring out of range");

    gfxFloat appUnitsToPixels = 1/mAppUnitsPerDevUnit;
    CompressedGlyph *charGlyphs = mCharacterGlyphs;
    gfxFloat direction = GetDirection();

    gfxPoint pt(aPt.x*appUnitsToPixels, aPt.y*appUnitsToPixels);
    gfxFloat startX = pt.x;

    GlyphRunIterator iter(this, aStart, aLength);
    while (iter.NextRun()) {
        gfxFont *font = iter.GetGlyphRun()->mFont;
        PRUint32 start = iter.GetStringStart();
        PRUint32 end = iter.GetStringEnd();
        NS_ASSERTION(charGlyphs[start].IsClusterStart(),
                     "Started drawing path in the middle of a cluster...");
        NS_ASSERTION(!charGlyphs[start].IsLigatureContinuation(),
                     "Can't draw path starting inside ligature");
        NS_ASSERTION(end == mCharacterCount || charGlyphs[end].IsClusterStart(),
                     "Ended drawing path in the middle of a cluster...");
        NS_ASSERTION(end == mCharacterCount || !charGlyphs[end].IsLigatureContinuation(),
                     "Can't end drawing path inside ligature");

        DrawGlyphs(font, aContext, PR_TRUE, &pt, start, end, aProvider);
    }

    if (aAdvanceWidth) {
        *aAdvanceWidth = (pt.x - startX)*direction*mAppUnitsPerDevUnit;
    }
}


void
gfxTextRun::AccumulateMetricsForRun(gfxFont *aFont, PRUint32 aStart,
                                    PRUint32 aEnd,
                                    PRBool aTight, PropertyProvider *aProvider,
                                    Metrics *aMetrics)
{
    nsAutoTArray<PropertyProvider::Spacing,200> spacingBuffer;
    PRBool haveSpacing = GetAdjustedSpacingArray(aStart, aEnd, aProvider, &spacingBuffer);
    Metrics metrics = aFont->Measure(this, aStart, aEnd, aTight,
                                     haveSpacing ? spacingBuffer.Elements() : nsnull);
 
    if (IsRightToLeft()) {
        metrics.CombineWith(*aMetrics);
        *aMetrics = metrics;
    } else {
        aMetrics->CombineWith(metrics);
    }
}

void
gfxTextRun::AccumulatePartialLigatureMetrics(gfxFont *aFont,
    PRUint32 aOffset, PRBool aTight, PropertyProvider *aProvider, Metrics *aMetrics)
{
    if (!mCharacterGlyphs[aOffset].IsClusterStart())
        return;

    // Measure partial ligature. We hack this by clipping the metrics in the
    // same way we clip the drawing.
    LigatureData data = ComputeLigatureData(aOffset, aProvider);

    // First measure the complete ligature
    Metrics metrics;
    AccumulateMetricsForRun(aFont, data.mStartOffset, data.mEndOffset,
                            aTight, aProvider, &metrics);
    gfxFloat clusterWidth = data.mLigatureWidth/data.mClusterCount;

    gfxFloat bboxStart;
    if (IsRightToLeft()) {
        bboxStart = metrics.mAdvanceWidth - metrics.mBoundingBox.XMost();
    } else {
        bboxStart = metrics.mBoundingBox.X();
    }
    gfxFloat bboxEnd = bboxStart + metrics.mBoundingBox.size.width;

    gfxFloat widthBeforeCluster;
    gfxFloat totalWidth = clusterWidth;
    if (data.mStartOffset < aOffset) {
        widthBeforeCluster =
            clusterWidth*data.mPartClusterIndex + data.mBeforeSpacing;
        // Not the start of the ligature; need to clip the boundingBox start
        bboxStart = PR_MAX(bboxStart, widthBeforeCluster);
    } else {
        // We're at the start of the ligature, so our cluster includes any
        // before-spacing and no clipping is required on this edge
        widthBeforeCluster = 0;
        totalWidth += data.mBeforeSpacing;
    }
    if (aOffset < data.mEndOffset) {
        // Not the end of the ligature; need to clip the boundingBox end
        gfxFloat endEdge = widthBeforeCluster + clusterWidth;
        bboxEnd = PR_MIN(bboxEnd, endEdge);
    } else {
        totalWidth += data.mAfterSpacing;
    }
    bboxStart -= widthBeforeCluster;
    bboxEnd -= widthBeforeCluster;
    if (IsRightToLeft()) {
        metrics.mBoundingBox.pos.x = metrics.mAdvanceWidth - bboxEnd;
    } else {
        metrics.mBoundingBox.pos.x = bboxStart;
    }
    metrics.mBoundingBox.size.width = bboxEnd - bboxStart;

    // We want metrics for just one cluster of the ligature
    metrics.mAdvanceWidth = totalWidth;
    metrics.mClusterCount = 1;

    if (IsRightToLeft()) {
        metrics.CombineWith(*aMetrics);
        *aMetrics = metrics;
    } else {
        aMetrics->CombineWith(metrics);
    }
}

gfxTextRun::Metrics
gfxTextRun::MeasureText(PRUint32 aStart, PRUint32 aLength,
                        PRBool aTightBoundingBox,
                        PropertyProvider *aProvider)
{
    CompressedGlyph *charGlyphs = mCharacterGlyphs;

    NS_ASSERTION(aStart + aLength <= mCharacterCount, "Substring out of range");
    NS_ASSERTION(aStart == mCharacterCount || charGlyphs[aStart].IsClusterStart(),
                 "MeasureText called, not starting at cluster boundary");
    NS_ASSERTION(aStart + aLength == mCharacterCount ||
                 charGlyphs[aStart + aLength].IsClusterStart(),
                 "MeasureText called, not ending at cluster boundary");

    Metrics accumulatedMetrics;
    GlyphRunIterator iter(this, aStart, aLength);
    while (iter.NextRun()) {
        gfxFont *font = iter.GetGlyphRun()->mFont;
        PRUint32 ligatureRunStart = iter.GetStringStart();
        PRUint32 ligatureRunEnd = iter.GetStringEnd();
        ShrinkToLigatureBoundaries(&ligatureRunStart, &ligatureRunEnd);

        PRUint32 i;
        for (i = iter.GetStringStart(); i < ligatureRunStart; ++i) {
            AccumulatePartialLigatureMetrics(font, i, aTightBoundingBox,
                                             aProvider, &accumulatedMetrics);
        }

        // XXX This sucks. We have to get glyph extents just so we can detect
        // glyphs outside the font box, even when aTightBoundingBox is false,
        // even though in almost all cases we could get correct results just
        // by getting some ascent/descent from the font and using our stored
        // advance widths.
        AccumulateMetricsForRun(font,
            ligatureRunStart, ligatureRunEnd, aTightBoundingBox, aProvider,
            &accumulatedMetrics);

        for (i = ligatureRunEnd; i < iter.GetStringEnd(); ++i) {
            AccumulatePartialLigatureMetrics(font, i, aTightBoundingBox,
                                             aProvider, &accumulatedMetrics);
        }
    }

    return accumulatedMetrics;
}

#define MEASUREMENT_BUFFER_SIZE 100

PRUint32
gfxTextRun::BreakAndMeasureText(PRUint32 aStart, PRUint32 aMaxLength,
                                PRBool aLineBreakBefore, gfxFloat aWidth,
                                PropertyProvider *aProvider,
                                PRBool aSuppressInitialBreak,
                                Metrics *aMetrics, PRBool aTightBoundingBox,
                                PRBool *aUsedHyphenation,
                                PRUint32 *aLastBreak)
{
    CompressedGlyph *charGlyphs = mCharacterGlyphs;

    aMaxLength = PR_MIN(aMaxLength, mCharacterCount - aStart);

    NS_ASSERTION(aStart + aMaxLength <= mCharacterCount, "Substring out of range");
    NS_ASSERTION(aStart == mCharacterCount || charGlyphs[aStart].IsClusterStart(),
                 "BreakAndMeasureText called, not starting at cluster boundary");
    NS_ASSERTION(aStart + aMaxLength == mCharacterCount ||
                 charGlyphs[aStart + aMaxLength].IsClusterStart(),
                 "BreakAndMeasureText called, not ending at cluster boundary");

    PRUint32 bufferStart = aStart;
    PRUint32 bufferLength = PR_MIN(aMaxLength, MEASUREMENT_BUFFER_SIZE);
    PropertyProvider::Spacing spacingBuffer[MEASUREMENT_BUFFER_SIZE];
    PRBool haveSpacing = (mFlags & gfxTextRunFactory::TEXT_ENABLE_SPACING) != 0;
    if (haveSpacing) {
        GetAdjustedSpacing(bufferStart, bufferStart + bufferLength, aProvider,
                           spacingBuffer);
    }
    PRPackedBool hyphenBuffer[MEASUREMENT_BUFFER_SIZE];
    PRBool haveHyphenation = (mFlags & gfxTextRunFactory::TEXT_ENABLE_HYPHEN_BREAKS) != 0;
    if (haveHyphenation) {
        aProvider->GetHyphenationBreaks(bufferStart, bufferStart + bufferLength,
                                        hyphenBuffer);
    }

    gfxFloat width = 0;
    PRUint32 pixelAdvance = 0;
    gfxFloat floatAdvanceUnits = 0;
    PRInt32 lastBreak = -1;
    PRBool aborted = PR_FALSE;
    PRUint32 end = aStart + aMaxLength;
    PRBool lastBreakUsedHyphenation = PR_FALSE;

    PRUint32 ligatureRunStart = aStart;
    PRUint32 ligatureRunEnd = end;
    ShrinkToLigatureBoundaries(&ligatureRunStart, &ligatureRunEnd);

    PRUint32 i;
    for (i = aStart; i < end; ++i) {
        if (i >= bufferStart + bufferLength) {
            // Fetch more spacing and hyphenation data
            bufferStart = i;
            bufferLength = PR_MIN(aStart + aMaxLength, i + MEASUREMENT_BUFFER_SIZE) - i;
            if (haveSpacing) {
                GetAdjustedSpacing(bufferStart, bufferStart + bufferLength, aProvider,
                                   spacingBuffer);
            }
            if (haveHyphenation) {
                aProvider->GetHyphenationBreaks(bufferStart, bufferStart + bufferLength,
                                                hyphenBuffer);
            }
        }

        PRBool lineBreakHere = mCharacterGlyphs[i].CanBreakBefore() &&
            (!aSuppressInitialBreak || i > aStart);
        if (lineBreakHere || (haveHyphenation && hyphenBuffer[i - bufferStart])) {
            gfxFloat advance = gfxFloat(pixelAdvance)*mAppUnitsPerDevUnit + floatAdvanceUnits;
            gfxFloat hyphenatedAdvance = advance;
            PRBool hyphenation = !lineBreakHere;
            if (hyphenation) {
                hyphenatedAdvance += aProvider->GetHyphenWidth();
            }
            pixelAdvance = 0;
            floatAdvanceUnits = 0;

            if (lastBreak < 0 || width + hyphenatedAdvance <= aWidth) {
                // We can break here.
                lastBreak = i;
                lastBreakUsedHyphenation = hyphenation;
            }

            width += advance;
            if (width > aWidth) {
                // No more text fits. Abort
                aborted = PR_TRUE;
                break;
            }
        }
        
        if (i >= ligatureRunStart && i < ligatureRunEnd) {
            CompressedGlyph *glyphData = &charGlyphs[i];
            if (glyphData->IsSimpleGlyph()) {
                pixelAdvance += glyphData->GetSimpleAdvance();
            } else if (glyphData->IsComplexCluster()) {
                NS_ASSERTION(mDetailedGlyphs, "No details but we have a complex cluster...");
                DetailedGlyph *details = mDetailedGlyphs[i];
                for (;;) {
                    floatAdvanceUnits += details->mAdvance*mAppUnitsPerDevUnit;
                    if (details->mIsLastGlyph)
                        break;
                    ++details;
                }
            }
            if (haveSpacing) {
                PropertyProvider::Spacing *space = &spacingBuffer[i - bufferStart];
                floatAdvanceUnits += space->mBefore + space->mAfter;
            }
        } else {
            floatAdvanceUnits += GetPartialLigatureWidth(i, i + 1, aProvider);
        }
    }

    if (!aborted) {
        gfxFloat advance = gfxFloat(pixelAdvance)*mAppUnitsPerDevUnit + floatAdvanceUnits;
        width += advance;
    }

    // There are three possibilities:
    // 1) all the text fit (width <= aWidth)
    // 2) some of the text fit up to a break opportunity (width > aWidth && lastBreak >= 0)
    // 3) none of the text fits before a break opportunity (width > aWidth && lastBreak < 0)
    PRUint32 charsFit;
    if (width <= aWidth) {
        charsFit = aMaxLength;
    } else if (lastBreak >= 0) {
        charsFit = lastBreak - aStart;
    } else {
        charsFit = aMaxLength;
    }

    if (aMetrics) {
        *aMetrics = MeasureText(aStart, charsFit, aTightBoundingBox, aProvider);
    }
    if (aUsedHyphenation) {
        *aUsedHyphenation = lastBreakUsedHyphenation;
    }
    if (aLastBreak && charsFit == aMaxLength) {
        if (lastBreak < 0) {
            *aLastBreak = PR_UINT32_MAX;
        } else {
            *aLastBreak = lastBreak - aStart;
        }
    }

    return charsFit;
}

gfxFloat
gfxTextRun::GetAdvanceWidth(PRUint32 aStart, PRUint32 aLength,
                            PropertyProvider *aProvider)
{
    CompressedGlyph *charGlyphs = mCharacterGlyphs;

    NS_ASSERTION(aStart + aLength <= mCharacterCount, "Substring out of range");
    NS_ASSERTION(aStart == mCharacterCount || charGlyphs[aStart].IsClusterStart(),
                 "GetAdvanceWidth called, not starting at cluster boundary");
    NS_ASSERTION(aStart + aLength == mCharacterCount ||
                 charGlyphs[aStart + aLength].IsClusterStart(),
                 "GetAdvanceWidth called, not ending at cluster boundary");

    gfxFloat result = 0; // app units

    // Account for all spacing here. This is more efficient than processing it
    // along with the glyphs.
    if (mFlags & gfxTextRunFactory::TEXT_ENABLE_SPACING) {
        PRUint32 i;
        nsAutoTArray<PropertyProvider::Spacing,200> spacingBuffer;
        if (spacingBuffer.AppendElements(aLength)) {
            GetAdjustedSpacing(aStart, aStart + aLength, aProvider,
                               spacingBuffer.Elements());
            for (i = 0; i < aLength; ++i) {
                PropertyProvider::Spacing *space = &spacingBuffer[i];
                result += space->mBefore + space->mAfter;
            }
        }
    }

    PRUint32 pixelAdvance = 0;
    PRUint32 ligatureRunStart = aStart;
    PRUint32 ligatureRunEnd = aStart + aLength;
    ShrinkToLigatureBoundaries(&ligatureRunStart, &ligatureRunEnd);

    result += GetPartialLigatureWidth(aStart, ligatureRunStart, aProvider) +
              GetPartialLigatureWidth(ligatureRunEnd, aStart + aLength, aProvider);

    PRUint32 i;
    for (i = ligatureRunStart; i < ligatureRunEnd; ++i) {
        CompressedGlyph *glyphData = &charGlyphs[i];
        if (glyphData->IsSimpleGlyph()) {
            pixelAdvance += glyphData->GetSimpleAdvance();
        } else if (glyphData->IsComplexCluster()) {
            NS_ASSERTION(mDetailedGlyphs, "No details but we have a complex cluster...");
            DetailedGlyph *details = mDetailedGlyphs[i];
            for (;;) {
                result += details->mAdvance*mAppUnitsPerDevUnit;
                if (details->mIsLastGlyph)
                    break;
                ++details;
            }
        }
    }

    return result + gfxFloat(pixelAdvance)*mAppUnitsPerDevUnit;
}


void
gfxTextRun::SetLineBreaks(PRUint32 aStart, PRUint32 aLength,
                          PRBool aLineBreakBefore, PRBool aLineBreakAfter,
                          TextProvider *aProvider,
                          gfxFloat *aAdvanceWidthDelta)
{
    // Do nothing because our shaping does not currently take linebreaks into
    // account. There is no change in advance width.
    if (aAdvanceWidthDelta) {
        *aAdvanceWidthDelta = 0;
    }
}

PRUint32
gfxTextRun::FindFirstGlyphRunContaining(PRUint32 aOffset)
{
    NS_ASSERTION(aOffset <= mCharacterCount, "Bad offset looking for glyphrun");
    if (aOffset == mCharacterCount)
        return mGlyphRuns.Length();

    PRUint32 start = 0;
    PRUint32 end = mGlyphRuns.Length();
    while (end - start > 1) {
        PRUint32 mid = (start + end)/2;
        if (mGlyphRuns[mid].mCharacterOffset <= aOffset) {
            start = mid;
        } else {
            end = mid;
        }
    }
    NS_ASSERTION(mGlyphRuns[start].mCharacterOffset <= aOffset,
                 "Hmm, something went wrong, aOffset should have been found");
    return start;
}

nsresult
gfxTextRun::AddGlyphRun(gfxFont *aFont, PRUint32 aUTF16Offset)
{
    NS_ASSERTION(mGlyphRuns.Length() > 0 || aUTF16Offset == 0,
                 "First run doesn't cover the first character?");
    GlyphRun *glyphRun = mGlyphRuns.AppendElement();
    if (!glyphRun)
        return NS_ERROR_OUT_OF_MEMORY;
    glyphRun->mFont = aFont;
    glyphRun->mCharacterOffset = aUTF16Offset;
    return NS_OK;
}

PRUint32
gfxTextRun::CountMissingGlyphs()
{
    PRUint32 i;
    PRUint32 count = 0;
    for (i = 0; i < mCharacterCount; ++i) {
        if (mCharacterGlyphs[i].IsMissing()) {
            ++count;
        }
    }
    return count;
}

void
gfxTextRun::SetDetailedGlyphs(PRUint32 aIndex, DetailedGlyph *aGlyphs,
                              PRUint32 aCount)
{
    if (!mCharacterGlyphs)
        return;

    if (!mDetailedGlyphs) {
        mDetailedGlyphs = new nsAutoArrayPtr<DetailedGlyph>[mCharacterCount];
        if (!mDetailedGlyphs) {
            mCharacterGlyphs[aIndex].SetMissing();
            return;
        }
    }
    DetailedGlyph *details = new DetailedGlyph[aCount];
    if (!details) {
        mCharacterGlyphs[aIndex].SetMissing();
        return;
    }
    memcpy(details, aGlyphs, sizeof(DetailedGlyph)*aCount);
    
    mDetailedGlyphs[aIndex] = details;
    mCharacterGlyphs[aIndex].SetComplexCluster();
}

void
gfxTextRun::RecordSurrogates(const PRUnichar *aString)
{
    if (!(mFlags & gfxTextRunFactory::TEXT_HAS_SURROGATES))
        return;

    // Remember which characters are low surrogates (the second half of
    // a surrogate pair).
    PRUint32 i;
    gfxTextRun::CompressedGlyph g;
    for (i = 0; i < mCharacterCount; ++i) {
        if (NS_IS_LOW_SURROGATE(aString[i])) {
            SetCharacterGlyph(i, g.SetLowSurrogate());
        }
    }
}
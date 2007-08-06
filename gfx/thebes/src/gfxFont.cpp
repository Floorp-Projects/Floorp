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
#include "nsExpirationTracker.h"

#include "gfxFont.h"
#include "gfxPlatform.h"

#include "prtypes.h"
#include "gfxTypes.h"
#include "gfxContext.h"
#include "gfxFontMissingGlyphs.h"
#include "nsMathUtils.h"

#include "cairo.h"
#include "gfxFontTest.h"

#include "nsCRT.h"

gfxFontCache *gfxFontCache::gGlobalCache = nsnull;

#ifdef DEBUG_roc
#define DEBUG_TEXT_RUN_STORAGE_METRICS
#endif

#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
static PRUint32 gTextRunStorageHighWaterMark = 0;
static PRUint32 gTextRunStorage = 0;
#endif

nsresult
gfxFontCache::Init()
{
    NS_ASSERTION(!gGlobalCache, "Where did this come from?");
    gGlobalCache = new gfxFontCache();
    return gGlobalCache ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

void
gfxFontCache::Shutdown()
{
    delete gGlobalCache;
    gGlobalCache = nsnull;

#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
    printf("Textrun storage high water mark=%d\n", gTextRunStorageHighWaterMark);
#endif
}

PRBool
gfxFontCache::HashEntry::KeyEquals(const KeyTypePointer aKey) const
{
    return aKey->mString.Equals(mFont->GetName()) &&
           aKey->mStyle->Equals(*mFont->GetStyle());
}

already_AddRefed<gfxFont>
gfxFontCache::Lookup(const nsAString &aName,
                     const gfxFontStyle *aStyle)
{
    Key key(aName, aStyle);
    HashEntry *entry = mFonts.GetEntry(key);
    if (!entry)
        return nsnull;

    gfxFont *font = entry->mFont;
    NS_ADDREF(font);
    if (font->GetExpirationState()->IsTracked()) {
        RemoveObject(font);
    }
    return font;
}

void
gfxFontCache::AddNew(gfxFont *aFont)
{
    Key key(aFont->GetName(), aFont->GetStyle());
    HashEntry *entry = mFonts.PutEntry(key);
    if (!entry)
        return;
    if (entry->mFont) {
        // This is weird. Someone's asking us to overwrite an existing font.
        // Oh well, make it happen ... just ensure that we're not tracking
        // the old font
        if (entry->mFont->GetExpirationState()->IsTracked()) {
            RemoveObject(entry->mFont);
        }
    }
    entry->mFont = aFont;
}

void
gfxFontCache::NotifyReleased(gfxFont *aFont)
{
    nsresult rv = AddObject(aFont);
    if (NS_FAILED(rv)) {
        // We couldn't track it for some reason. Kill it now.
        DestroyFont(aFont);
    }
    // Note that we might have fonts that aren't in the hashtable, perhaps because
    // of OOM adding to the hashtable or because someone did an AddNew where
    // we already had a font. These fonts are added to the expiration tracker
    // anyway, even though Lookup can't resurrect them. Eventually they will
    // expire and be deleted.
}

void
gfxFontCache::NotifyExpired(gfxFont *aFont)
{
    RemoveObject(aFont);
    DestroyFont(aFont);
}

void
gfxFontCache::DestroyFont(gfxFont *aFont)
{
    Key key(aFont->GetName(), aFont->GetStyle());
    mFonts.RemoveEntry(key);
    delete aFont;
}

gfxFont::gfxFont(const nsAString &aName, const gfxFontStyle *aFontStyle) :
    mName(aName), mStyle(*aFontStyle)
{
}

/**
 * A helper function in case we need to do any rounding or other
 * processing here.
 */
#define ToDeviceUnits(aAppUnits, aDevUnitsPerAppUnit)   (double(aAppUnits)*double(aDevUnitsPerAppUnit))

struct GlyphBuffer {
#define GLYPH_BUFFER_SIZE (2048/sizeof(cairo_glyph_t))
    cairo_glyph_t mGlyphBuffer[GLYPH_BUFFER_SIZE];
    unsigned int mNumGlyphs;

    GlyphBuffer()
        : mNumGlyphs(0) { }

    cairo_glyph_t *AppendGlyph() {
        return &mGlyphBuffer[mNumGlyphs++];
    }

    void Flush(cairo_t *cr, PRBool drawToPath, PRBool finish = PR_FALSE) {
        if (!finish && mNumGlyphs != GLYPH_BUFFER_SIZE)
            return;

        if (drawToPath)
            cairo_glyph_path(cr, mGlyphBuffer, mNumGlyphs);
        else
            cairo_show_glyphs(cr, mGlyphBuffer, mNumGlyphs);

        mNumGlyphs = 0;
    }
#undef GLYPH_BUFFER_SIZE
};


void
gfxFont::Draw(gfxTextRun *aTextRun, PRUint32 aStart, PRUint32 aEnd,
              gfxContext *aContext, PRBool aDrawToPath, gfxPoint *aPt,
              Spacing *aSpacing)
{
    if (aStart >= aEnd)
        return;

    const gfxTextRun::CompressedGlyph *charGlyphs = aTextRun->GetCharacterGlyphs();
    const PRUint32 appUnitsPerDevUnit = aTextRun->GetAppUnitsPerDevUnit();
    const double devUnitsPerAppUnit = 1.0/double(appUnitsPerDevUnit);
    PRBool isRTL = aTextRun->IsRightToLeft();
    double direction = aTextRun->GetDirection();
    PRUint32 i;
    // Current position in appunits
    double x = aPt->x;
    double y = aPt->y;

    cairo_t *cr = aContext->GetCairo();
    PRBool success = SetupCairoFont(cr);
    if (NS_UNLIKELY(!success))
        return;

    GlyphBuffer glyphs;
    cairo_glyph_t *glyph;
    
    if (aSpacing) {
        x += direction*aSpacing[0].mBefore;
    }
    for (i = aStart; i < aEnd; ++i) {
        const gfxTextRun::CompressedGlyph *glyphData = &charGlyphs[i];
        if (glyphData->IsSimpleGlyph()) {
            glyph = glyphs.AppendGlyph();
            glyph->index = glyphData->GetSimpleGlyph();
            double advance = glyphData->GetSimpleAdvance();
            // Perhaps we should put a scale in the cairo context instead of
            // doing this scaling here...
            // Multiplying by the reciprocal may introduce tiny error here,
            // but we assume cairo is going to round coordinates at some stage
            // and this is faster
            glyph->x = ToDeviceUnits(x, devUnitsPerAppUnit);
            glyph->y = ToDeviceUnits(y, devUnitsPerAppUnit);
            if (isRTL) {
                glyph->x -= ToDeviceUnits(advance, devUnitsPerAppUnit);
                x -= advance;
            } else {
                x += advance;
            }
            glyphs.Flush(cr, aDrawToPath);
        } else if (glyphData->IsComplexCluster()) {
            const gfxTextRun::DetailedGlyph *details = aTextRun->GetDetailedGlyphs(i);
            for (;;) {
                glyph = glyphs.AppendGlyph();
                glyph->index = details->mGlyphID;
                glyph->x = ToDeviceUnits(x + details->mXOffset, devUnitsPerAppUnit);
                glyph->y = ToDeviceUnits(y + details->mYOffset, devUnitsPerAppUnit);
                double advance = details->mAdvance;
                if (isRTL) {
                    glyph->x -= ToDeviceUnits(advance, devUnitsPerAppUnit);
                }
                x += direction*advance;

                glyphs.Flush(cr, aDrawToPath);

                if (details->mIsLastGlyph)
                    break;
                ++details;
            }
        } else if (glyphData->IsMissing()) {
            const gfxTextRun::DetailedGlyph *details = aTextRun->GetDetailedGlyphs(i);
            if (details) {
                double advance = details->mAdvance;
                if (!aDrawToPath) {
                    gfxPoint pt(ToDeviceUnits(x, devUnitsPerAppUnit),
                                ToDeviceUnits(y, devUnitsPerAppUnit));
                    gfxFloat advanceDevUnits = ToDeviceUnits(advance, devUnitsPerAppUnit);
                    if (isRTL) {
                        pt.x -= advanceDevUnits;
                    }
                    gfxFloat height = GetMetrics().maxAscent;
                    gfxRect glyphRect(pt.x, pt.y - height, advanceDevUnits, height);
                    gfxFontMissingGlyphs::DrawMissingGlyph(aContext, glyphRect, details->mGlyphID);
                }
                x += direction*advance;
            }
        }
        // Every other glyph type is ignored
        if (aSpacing) {
            double space = aSpacing[i - aStart].mAfter;
            if (i + 1 < aEnd) {
                space += aSpacing[i + 1 - aStart].mBefore;
            }
            x += direction*space;
        }
    }

    if (gfxFontTestStore::CurrentStore()) {
        /* This assumes that the tests won't have anything that results
         * in more than GLYPH_BUFFER_SIZE glyphs.  Do this before we
         * flush, since that'll blow away the num_glyphs.
         */
        gfxFontTestStore::CurrentStore()->AddItem(GetUniqueName(),
                                                  glyphs.mGlyphBuffer, glyphs.mNumGlyphs);
    }

    // draw any remaining glyphs
    glyphs.Flush(cr, aDrawToPath, PR_TRUE);

    *aPt = gfxPoint(x, y);
}

gfxFont::RunMetrics
gfxFont::Measure(gfxTextRun *aTextRun,
                 PRUint32 aStart, PRUint32 aEnd,
                 PRBool aTightBoundingBox,
                 Spacing *aSpacing)
{
    // XXX temporary code, does not handle glyphs outside the font-box
    // XXX comment out the assertion for now since it fires too much
    // NS_ASSERTION(!(aTextRun->GetFlags() & gfxTextRunFactory::TEXT_NEED_BOUNDING_BOX),
    //              "Glyph extents not yet supported");
    PRInt32 advance = 0;
    PRUint32 i;
    const gfxTextRun::CompressedGlyph *charGlyphs = aTextRun->GetCharacterGlyphs();
    PRUint32 clusterCount = 0;
    for (i = aStart; i < aEnd; ++i) {
        gfxTextRun::CompressedGlyph g = charGlyphs[i];
        if (g.IsClusterStart()) {
            ++clusterCount;
            if (g.IsSimpleGlyph()) {
                advance += charGlyphs[i].GetSimpleAdvance();
            } else if (g.IsComplexOrMissing()) {
                const gfxTextRun::DetailedGlyph *details = aTextRun->GetDetailedGlyphs(i);
                while (details) {
                    advance += details->mAdvance;
                    if (details->mIsLastGlyph)
                        break;
                    ++details;
                }
            }
        }
    }

    gfxFloat floatAdvance = advance;
    if (aSpacing) {
        for (i = 0; i < aEnd - aStart; ++i) {
            floatAdvance += aSpacing[i].mBefore + aSpacing[i].mAfter;
        }
    }
    RunMetrics metrics;
    const gfxFont::Metrics& fontMetrics = GetMetrics();
    metrics.mAdvanceWidth = floatAdvance;
    const PRUint32 appUnitsPerDevUnit = aTextRun->GetAppUnitsPerDevUnit();
    metrics.mAscent = fontMetrics.maxAscent*appUnitsPerDevUnit;
    metrics.mDescent = fontMetrics.maxDescent*appUnitsPerDevUnit;
    metrics.mBoundingBox =
        gfxRect(0, -metrics.mAscent, floatAdvance, metrics.mAscent + metrics.mDescent);
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
    }
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
gfxFontGroup::MakeEmptyTextRun(const Parameters *aParams, PRUint32 aFlags)
{
    aFlags |= TEXT_IS_8BIT | TEXT_IS_ASCII | TEXT_IS_PERSISTENT;
    return new gfxTextRun(aParams, nsnull, 0, this, aFlags);
}

gfxTextRun *
gfxFontGroup::MakeSpaceTextRun(const Parameters *aParams, PRUint32 aFlags)
{
    aFlags |= TEXT_IS_8BIT | TEXT_IS_ASCII | TEXT_IS_PERSISTENT;
    static const PRUint8 space = ' ';

    nsAutoPtr<gfxTextRun> textRun;
    textRun = new gfxTextRun(aParams, &space, 1, this, aFlags);
    if (!textRun || !textRun->GetCharacterGlyphs())
        return nsnull;

    gfxFont *font = GetFontAt(0);
    textRun->SetSpaceGlyph(font, aParams->mContext, 0);
    return textRun.forget();
}

gfxFontStyle::gfxFontStyle(PRUint8 aStyle, PRUint16 aWeight, gfxFloat aSize,
                           const nsACString& aLangGroup,
                           float aSizeAdjust, PRPackedBool aSystemFont,
                           PRPackedBool aFamilyNameQuirks) :
    style(aStyle), systemFont(aSystemFont),
    familyNameQuirks(aFamilyNameQuirks), weight(aWeight),
    size(aSize), langGroup(aLangGroup), sizeAdjust(aSizeAdjust)
{
    if (weight > 900)
        weight = 900;
    if (weight < 100)
        weight = 100;

    if (size >= FONT_MAX_SIZE) {
        size = FONT_MAX_SIZE;
        sizeAdjust = 0.0;
    } else if (size < 0.0) {
        NS_WARNING("negative font size");
        size = 0.0;
    }

    if (langGroup.IsEmpty()) {
        NS_WARNING("empty langgroup");
        langGroup.Assign("x-western");
    }
}

gfxFontStyle::gfxFontStyle(const gfxFontStyle& aStyle) :
    style(aStyle.style), systemFont(aStyle.systemFont),
    familyNameQuirks(aStyle.familyNameQuirks), weight(aStyle.weight),
    size(aStyle.size), langGroup(aStyle.langGroup),
    sizeAdjust(aStyle.sizeAdjust)
{
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

#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
static void
AccountStorageForTextRun(gfxTextRun *aTextRun, PRInt32 aSign)
{
    // Ignores detailed glyphs... we don't know when those have been constructed
    // Also ignores gfxSkipChars dynamic storage (which won't be anything
    // for preformatted text)
    // Also ignores GlyphRun array, again because it hasn't been constructed
    // by the time this gets called. If there's only one glyphrun that's stored
    // directly in the textrun anyway so no additional overhead.
    PRInt32 bytesPerChar = sizeof(gfxTextRun::CompressedGlyph);
    if (aTextRun->GetFlags() & gfxTextRunFactory::TEXT_IS_PERSISTENT) {
      bytesPerChar += (aTextRun->GetFlags() & gfxTextRunFactory::TEXT_IS_8BIT) ? 1 : 2;
    }
    PRInt32 bytes = sizeof(gfxTextRun) + aTextRun->GetLength()*bytesPerChar;
    gTextRunStorage += bytes*aSign;
    gTextRunStorageHighWaterMark = PR_MAX(gTextRunStorageHighWaterMark, gTextRunStorage);
}
#endif

gfxTextRun::gfxTextRun(const gfxTextRunFactory::Parameters *aParams, const void *aText,
                       PRUint32 aLength, gfxFontGroup *aFontGroup, PRUint32 aFlags)
  : mUserData(aParams->mUserData),
    mFontGroup(aFontGroup),
    mAppUnitsPerDevUnit(aParams->mAppUnitsPerDevUnit),
    mFlags(aFlags), mCharacterCount(aLength), mHashCode(0)
{
    NS_ASSERTION(mAppUnitsPerDevUnit != 0, "Invalid app unit scale");
    MOZ_COUNT_CTOR(gfxTextRun);
    NS_ADDREF(mFontGroup);
    if (aParams->mSkipChars) {
        mSkipChars.TakeFrom(aParams->mSkipChars);
    }
    if (aLength > 0) {
        mCharacterGlyphs = new CompressedGlyph[aLength];
        if (mCharacterGlyphs) {
            memset(mCharacterGlyphs, 0, sizeof(CompressedGlyph)*aLength);
        }
    }
    if (mFlags & gfxTextRunFactory::TEXT_IS_8BIT) {
        mText.mSingle = static_cast<const PRUint8 *>(aText);
        if (!(mFlags & gfxTextRunFactory::TEXT_IS_PERSISTENT)) {
            PRUint8 *newText = new PRUint8[aLength];
            if (!newText) {
                // indicate textrun failure
                mCharacterGlyphs = nsnull;
            } else {
                memcpy(newText, aText, aLength);
            }
            mText.mSingle = newText;    
        }
    } else {
        mText.mDouble = static_cast<const PRUnichar *>(aText);
        if (!(mFlags & gfxTextRunFactory::TEXT_IS_PERSISTENT)) {
            PRUnichar *newText = new PRUnichar[aLength];
            if (!newText) {
                // indicate textrun failure
                mCharacterGlyphs = nsnull;
            } else {
                memcpy(newText, aText, aLength*sizeof(PRUnichar));
            }
            mText.mDouble = newText;    
        }
    }
#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
    AccountStorageForTextRun(this, 1);
#endif
}

gfxTextRun::~gfxTextRun()
{
#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
    AccountStorageForTextRun(this, -1);
#endif
    if (!(mFlags & gfxTextRunFactory::TEXT_IS_PERSISTENT)) {
        if (mFlags & gfxTextRunFactory::TEXT_IS_8BIT) {
            delete[] mText.mSingle;
        } else {
            delete[] mText.mDouble;
        }
    }
    NS_RELEASE(mFontGroup);
    MOZ_COUNT_DTOR(gfxTextRun);
}

gfxTextRun *
gfxTextRun::Clone(const gfxTextRunFactory::Parameters *aParams, const void *aText,
                  PRUint32 aLength, gfxFontGroup *aFontGroup, PRUint32 aFlags)
{
    if (!mCharacterGlyphs)
        return nsnull;

    nsAutoPtr<gfxTextRun> textRun;
    textRun = new gfxTextRun(aParams, aText, aLength, aFontGroup, aFlags);
    if (!textRun || !textRun->mCharacterGlyphs)
        return nsnull;

    textRun->CopyGlyphDataFrom(this, 0, mCharacterCount, 0, PR_FALSE);
    return textRun.forget();
}

PRBool
gfxTextRun::SetPotentialLineBreaks(PRUint32 aStart, PRUint32 aLength,
                                   PRPackedBool *aBreakBefore,
                                   gfxContext *aRefContext)
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

PRInt32
gfxTextRun::ComputeClusterAdvance(PRUint32 aClusterOffset)
{
    CompressedGlyph *glyphData = &mCharacterGlyphs[aClusterOffset];
    if (glyphData->IsSimpleGlyph())
        return glyphData->GetSimpleAdvance();

    const DetailedGlyph *details = GetDetailedGlyphs(aClusterOffset);
    if (!details)
        return 0;

    PRInt32 advance = 0;
    while (1) {
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
    result.mLigatureWidth = ComputeClusterAdvance(ligStart);
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

    CompressedGlyph *charGlyphs = mCharacterGlyphs;
    PRUint32 i;

    if (mFlags & gfxTextRunFactory::TEXT_ABSOLUTE_SPACING) {
        // Subtract character widths from mAfter at the end of clusters/ligatures to
        // relativize spacing. This is a bit sad since we're going to add
        // them in again below when we actually use the spacing, but this
        // produces simpler code and absolute spacing is rarely required.
        
        // The width of the last nonligature cluster, in appunits
        PRInt32 clusterWidth = 0;
        for (i = aStart; i < aEnd; ++i) {
            CompressedGlyph *glyphData = &charGlyphs[i];
            
            if (glyphData->IsSimpleGlyph()) {
                if (i > aStart) {
                    aSpacing[i - 1 - aStart].mAfter -= clusterWidth;
                }
                clusterWidth = glyphData->GetSimpleAdvance();
            } else if (glyphData->IsComplexOrMissing()) {
                if (i > aStart) {
                    aSpacing[i - 1 - aStart].mAfter -= clusterWidth;
                }
                clusterWidth = 0;
                const DetailedGlyph *details = GetDetailedGlyphs(i);
                if (details) {
                    while (1) {
                        clusterWidth += details->mAdvance;
                        if (details->mIsLastGlyph)
                            break;
                        ++details;
                    }
                }
            }
        }
        aSpacing[aEnd - 1 - aStart].mAfter -= clusterWidth;
    }

#ifdef DEBUG
    // Check to see if we have spacing inside ligatures
    for (i = aStart; i < aEnd; ++i) {
        if (charGlyphs[i].IsLigatureContinuation()) {
            NS_ASSERTION(i == aStart || aSpacing[i - aStart].mBefore == 0,
                         "Before-spacing inside a ligature!");
            NS_ASSERTION(i - 1 <= aStart || aSpacing[i - 1 - aStart].mAfter == 0,
                         "After-spacing inside a ligature!");
        }
    }
#endif
}

PRBool
gfxTextRun::GetAdjustedSpacingArray(PRUint32 aStart, PRUint32 aEnd,
                                    PropertyProvider *aProvider,
                                    nsTArray<PropertyProvider::Spacing> *aSpacing)
{
    if (!aProvider || !(mFlags & gfxTextRunFactory::TEXT_ENABLE_SPACING))
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

    gfxFloat result = gfxFloat(data.mLigatureWidth)*clusterCount/data.mClusterCount;
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

    // Draw partial ligature. We hack this by clipping the ligature.
    LigatureData data = ComputeLigatureData(aOffset, aProvider);
    // Width of a cluster in the ligature, in appunits
    gfxFloat clusterWidth = data.mLigatureWidth/data.mClusterCount;

    gfxFloat direction = GetDirection();
    gfxFloat left = aDirtyRect->X();
    gfxFloat right = aDirtyRect->XMost();
    // The advance to the start of this cluster in the drawn ligature, in appunits
    gfxFloat widthBeforeCluster;
    // Any spacing that should be included after the cluster, in appunits
    gfxFloat afterSpace;
    if (data.mStartOffset < aOffset) {
        // Not the start of the ligature; need to clip the ligature before the current cluster
        if (IsRightToLeft()) {
            right = PR_MIN(right, aPt->x);
        } else {
            left = PR_MAX(left, aPt->x);
        }
        widthBeforeCluster = clusterWidth*data.mPartClusterIndex +
            data.mBeforeSpacing;
    } else {
        // We're drawing the start of the ligature, so our cluster includes any
        // before-spacing.
        widthBeforeCluster = 0;
    }
    if (aOffset < data.mEndOffset - 1) {
        // Not the end of the ligature; need to clip the ligature after the current cluster
        gfxFloat endEdge = aPt->x + clusterWidth;
        if (IsRightToLeft()) {
            left = PR_MAX(left, endEdge);
        } else {
            right = PR_MIN(right, endEdge);
        }
        afterSpace = 0;
    } else {
        afterSpace = data.mAfterSpacing;
    }

    aCtx->Save();
    aCtx->NewPath();
    // use division here to ensure that when the rect is aligned on multiples
    // of mAppUnitsPerDevUnit, we clip to true device unit boundaries.
    // Also, make sure we snap the rectangle to device pixels.
    aCtx->Rectangle(gfxRect(left/mAppUnitsPerDevUnit,
                            aDirtyRect->Y()/mAppUnitsPerDevUnit,
                            (right - left)/mAppUnitsPerDevUnit,
                            aDirtyRect->Height()/mAppUnitsPerDevUnit), PR_TRUE);
    aCtx->Clip();
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

    CompressedGlyph *charGlyphs = mCharacterGlyphs;
    gfxFloat direction = GetDirection();
    gfxPoint pt = aPt;

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
        *aAdvanceWidth = (pt.x - aPt.x)*direction;
    }
}

void
gfxTextRun::DrawToPath(gfxContext *aContext, gfxPoint aPt,
                       PRUint32 aStart, PRUint32 aLength,
                       PropertyProvider *aProvider, gfxFloat *aAdvanceWidth)
{
    NS_ASSERTION(aStart + aLength <= mCharacterCount, "Substring out of range");

    CompressedGlyph *charGlyphs = mCharacterGlyphs;
    gfxFloat direction = GetDirection();
    gfxPoint pt = aPt;

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
        *aAdvanceWidth = (pt.x - aPt.x)*direction;
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
                                gfxFloat *aTrimWhitespace,
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
    PRBool haveSpacing = aProvider && (mFlags & gfxTextRunFactory::TEXT_ENABLE_SPACING) != 0;
    if (haveSpacing) {
        GetAdjustedSpacing(bufferStart, bufferStart + bufferLength, aProvider,
                           spacingBuffer);
    }
    PRPackedBool hyphenBuffer[MEASUREMENT_BUFFER_SIZE];
    PRBool haveHyphenation = (mFlags & gfxTextRunFactory::TEXT_ENABLE_HYPHEN_BREAKS) != 0;
    if (haveHyphenation) {
        aProvider->GetHyphenationBreaks(bufferStart, bufferLength,
                                        hyphenBuffer);
    }

    gfxFloat width = 0;
    gfxFloat advance = 0;
    // The number of space characters that can be trimmed
    PRUint32 trimmableChars = 0;
    // The amount of space removed by ignoring trimmableChars
    gfxFloat trimmableAdvance = 0;
    PRInt32 lastBreak = -1;
    PRInt32 lastBreakTrimmableChars = -1;
    gfxFloat lastBreakTrimmableAdvance = -1;
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
                aProvider->GetHyphenationBreaks(bufferStart, bufferLength,
                                                hyphenBuffer);
            }
        }

        PRBool lineBreakHere = mCharacterGlyphs[i].CanBreakBefore() &&
            (!aSuppressInitialBreak || i > aStart);
        if (lineBreakHere || (haveHyphenation && hyphenBuffer[i - bufferStart])) {
            gfxFloat hyphenatedAdvance = advance;
            PRBool hyphenation = !lineBreakHere;
            if (hyphenation) {
                hyphenatedAdvance += aProvider->GetHyphenWidth();
            }
            
            if (lastBreak < 0 || width + hyphenatedAdvance - trimmableAdvance <= aWidth) {
                // We can break here.
                lastBreak = i;
                lastBreakTrimmableChars = trimmableChars;
                lastBreakTrimmableAdvance = trimmableAdvance;
                lastBreakUsedHyphenation = hyphenation;
            }

            width += advance;
            advance = 0;
            if (width - trimmableAdvance > aWidth) {
                // No more text fits. Abort
                aborted = PR_TRUE;
                break;
            }
        }
        
        gfxFloat charAdvance = 0;
        if (i >= ligatureRunStart && i < ligatureRunEnd) {
            CompressedGlyph *glyphData = &charGlyphs[i];
            if (glyphData->IsSimpleGlyph()) {
                charAdvance = glyphData->GetSimpleAdvance();
            } else if (glyphData->IsComplexOrMissing()) {
                const DetailedGlyph *details = GetDetailedGlyphs(i);
                if (details) {
                    while (1) {
                        charAdvance += details->mAdvance;
                        if (details->mIsLastGlyph)
                            break;
                        ++details;
                    }
                }
            }
            if (haveSpacing) {
                PropertyProvider::Spacing *space = &spacingBuffer[i - bufferStart];
                charAdvance += space->mBefore + space->mAfter;
            }
        } else {
            charAdvance += GetPartialLigatureWidth(i, i + 1, aProvider);
        }
        
        advance += charAdvance;
        if (aTrimWhitespace) {
            if (GetChar(i) == ' ') {
                ++trimmableChars;
                trimmableAdvance += charAdvance;
            } else {
                trimmableAdvance = 0;
                trimmableChars = 0;
            }
        }
    }

    if (!aborted) {
        width += advance;
    }

    // There are three possibilities:
    // 1) all the text fit (width <= aWidth)
    // 2) some of the text fit up to a break opportunity (width > aWidth && lastBreak >= 0)
    // 3) none of the text fits before a break opportunity (width > aWidth && lastBreak < 0)
    PRUint32 charsFit;
    PRBool usedHyphenation = PR_FALSE;
    if (width - trimmableAdvance <= aWidth) {
        charsFit = aMaxLength;
    } else if (lastBreak >= 0) {
        charsFit = lastBreak - aStart;
        trimmableChars = lastBreakTrimmableChars;
        trimmableAdvance = lastBreakTrimmableAdvance;
        usedHyphenation = lastBreakUsedHyphenation;
    } else {
        charsFit = aMaxLength;
    }

    if (aMetrics) {
        *aMetrics = MeasureText(aStart, charsFit - trimmableChars, aTightBoundingBox, aProvider);
    }
    if (aTrimWhitespace) {
        *aTrimWhitespace = trimmableAdvance;
    }
    if (aUsedHyphenation) {
        *aUsedHyphenation = usedHyphenation;
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

    PRUint32 ligatureRunStart = aStart;
    PRUint32 ligatureRunEnd = aStart + aLength;
    ShrinkToLigatureBoundaries(&ligatureRunStart, &ligatureRunEnd);

    gfxFloat result = GetPartialLigatureWidth(aStart, ligatureRunStart, aProvider) +
                      GetPartialLigatureWidth(ligatureRunEnd, aStart + aLength, aProvider);

    // Account for all remaining spacing here. This is more efficient than
    // processing it along with the glyphs.
    if (aProvider && (mFlags & gfxTextRunFactory::TEXT_ENABLE_SPACING)) {
        PRUint32 i;
        nsAutoTArray<PropertyProvider::Spacing,200> spacingBuffer;
        if (spacingBuffer.AppendElements(aLength)) {
            GetAdjustedSpacing(ligatureRunStart, ligatureRunEnd, aProvider,
                               spacingBuffer.Elements());
            for (i = 0; i < ligatureRunEnd - ligatureRunStart; ++i) {
                PropertyProvider::Spacing *space = &spacingBuffer[i];
                result += space->mBefore + space->mAfter;
            }
        }
    }

    PRUint32 i;
    for (i = ligatureRunStart; i < ligatureRunEnd; ++i) {
        CompressedGlyph *glyphData = &charGlyphs[i];
        if (glyphData->IsSimpleGlyph()) {
            result += glyphData->GetSimpleAdvance();
        } else if (glyphData->IsComplexOrMissing()) {
            const DetailedGlyph *details = GetDetailedGlyphs(i);
            if (details) {
                while (1) {
                    result += details->mAdvance;
                    if (details->mIsLastGlyph)
                        break;
                    ++details;
                }
            }
        }
    }

    return result;
}

PRBool
gfxTextRun::SetLineBreaks(PRUint32 aStart, PRUint32 aLength,
                          PRBool aLineBreakBefore, PRBool aLineBreakAfter,
                          gfxFloat *aAdvanceWidthDelta,
                          gfxContext *aRefContext)
{
    // Do nothing because our shaping does not currently take linebreaks into
    // account. There is no change in advance width.
    if (aAdvanceWidthDelta) {
        *aAdvanceWidthDelta = 0;
    }
    return PR_FALSE;
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
gfxTextRun::AddGlyphRun(gfxFont *aFont, PRUint32 aUTF16Offset, PRBool aForceNewRun)
{
    PRUint32 numGlyphRuns = mGlyphRuns.Length();
    if (!aForceNewRun &&
        numGlyphRuns > 0)
    {
        GlyphRun *lastGlyphRun = &mGlyphRuns[numGlyphRuns - 1];

        NS_ASSERTION(lastGlyphRun->mCharacterOffset <= aUTF16Offset,
                     "Glyph runs out of order (and run not forced)");

        if (lastGlyphRun->mFont == aFont)
            return NS_OK;
        if (lastGlyphRun->mCharacterOffset == aUTF16Offset) {
            lastGlyphRun->mFont = aFont;
            return NS_OK;
        }
    }

    NS_ASSERTION(aForceNewRun || numGlyphRuns > 0 || aUTF16Offset == 0,
                 "First run doesn't cover the first character (and run not forced)?");

    GlyphRun *glyphRun = mGlyphRuns.AppendElement();
    if (!glyphRun)
        return NS_ERROR_OUT_OF_MEMORY;
    glyphRun->mFont = aFont;
    glyphRun->mCharacterOffset = aUTF16Offset;
    return NS_OK;
}

void
gfxTextRun::SortGlyphRuns()
{
    GlyphRunOffsetComparator comp;
    mGlyphRuns.Sort(comp);
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

gfxTextRun::DetailedGlyph *
gfxTextRun::AllocateDetailedGlyphs(PRUint32 aIndex, PRUint32 aCount)
{
    if (!mCharacterGlyphs)
        return nsnull;

    if (!mDetailedGlyphs) {
        mDetailedGlyphs = new nsAutoArrayPtr<DetailedGlyph>[mCharacterCount];
        if (!mDetailedGlyphs) {
            mCharacterGlyphs[aIndex].SetMissing();
            return nsnull;
        }
    }
    DetailedGlyph *details = new DetailedGlyph[aCount];
    if (!details) {
        mCharacterGlyphs[aIndex].SetMissing();
        return nsnull;
    }
    mDetailedGlyphs[aIndex] = details;
    return details;
}

void
gfxTextRun::SetDetailedGlyphs(PRUint32 aIndex, const DetailedGlyph *aGlyphs,
                              PRUint32 aCount)
{
    NS_ASSERTION(aCount > 0, "Can't set zero detailed glyphs");
    NS_ASSERTION(aGlyphs[aCount - 1].mIsLastGlyph, "Failed to set last glyph flag");

    DetailedGlyph *details = AllocateDetailedGlyphs(aIndex, aCount);
    if (!details)
        return;

    memcpy(details, aGlyphs, sizeof(DetailedGlyph)*aCount);
    mCharacterGlyphs[aIndex].SetComplexCluster();
}
  
void
gfxTextRun::SetMissingGlyph(PRUint32 aIndex, PRUnichar aChar)
{
    DetailedGlyph *details = AllocateDetailedGlyphs(aIndex, 1);
    if (!details)
        return;

    details->mIsLastGlyph = PR_TRUE;
    details->mGlyphID = aChar;
    GlyphRun *glyphRun = &mGlyphRuns[FindFirstGlyphRunContaining(aIndex)];
    gfxFloat width = PR_MAX(glyphRun->mFont->GetMetrics().aveCharWidth,
                            gfxFontMissingGlyphs::GetDesiredMinWidth());
    details->mAdvance = PRUint32(width*GetAppUnitsPerDevUnit());
    details->mXOffset = 0;
    details->mYOffset = 0;
    mCharacterGlyphs[aIndex].SetMissing();
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

static PRUint32
CountDetailedGlyphs(gfxTextRun::DetailedGlyph *aGlyphs)
{
    PRUint32 i = 0;
    while (!aGlyphs[i].mIsLastGlyph) {
        ++i;
    }
    return i + 1;
}

static void
ClearCharacters(gfxTextRun::CompressedGlyph *aGlyphs, PRUint32 aLength)
{
    gfxTextRun::CompressedGlyph g;
    g.SetMissing();
    PRUint32 i;
    for (i = 0; i < aLength; ++i) {
        aGlyphs[i] = g;
    }
}

void
gfxTextRun::CopyGlyphDataFrom(gfxTextRun *aSource, PRUint32 aStart,
                              PRUint32 aLength, PRUint32 aDest,
                              PRBool aStealData)
{
    NS_ASSERTION(aStart + aLength <= aSource->GetLength(),
                 "Source substring out of range");
    NS_ASSERTION(aDest + aLength <= GetLength(),
                 "Destination substring out of range");

    PRUint32 i;
    for (i = 0; i < aLength; ++i) {
        CompressedGlyph g = aSource->mCharacterGlyphs[i + aStart];
        g.SetCanBreakBefore(mCharacterGlyphs[i + aDest].CanBreakBefore());
        mCharacterGlyphs[i + aDest] = g;
        if (aStealData) {
            aSource->mCharacterGlyphs[i + aStart].SetMissing();
        }
    }

    if (aSource->mDetailedGlyphs) {
        for (i = 0; i < aLength; ++i) {
            DetailedGlyph *details = aSource->mDetailedGlyphs[i + aStart];
            if (details) {
                if (aStealData) {
                    if (!mDetailedGlyphs) {
                        mDetailedGlyphs = new nsAutoArrayPtr<DetailedGlyph>[mCharacterCount];
                        if (!mDetailedGlyphs) {
                            ClearCharacters(&mCharacterGlyphs[aDest], aLength);
                            return;
                        }
                    }        
                    mDetailedGlyphs[i + aDest] = details;
                    aSource->mDetailedGlyphs[i + aStart].forget();
                } else {
                    PRUint32 glyphCount = CountDetailedGlyphs(details);
                    DetailedGlyph *dest = AllocateDetailedGlyphs(i + aDest, glyphCount);
                    if (!dest) {
                        ClearCharacters(&mCharacterGlyphs[aDest], aLength);
                        return;
                    }
                    memcpy(dest, details, sizeof(DetailedGlyph)*glyphCount);
                }
            } else if (mDetailedGlyphs) {
                mDetailedGlyphs[i + aDest] = nsnull;
            }
        }
    } else if (mDetailedGlyphs) {
        for (i = 0; i < aLength; ++i) {
            mDetailedGlyphs[i + aDest] = nsnull;
        }
    }

    GlyphRunIterator iter(aSource, aStart, aLength);
    while (iter.NextRun()) {
        gfxFont *font = iter.GetGlyphRun()->mFont;
        PRUint32 start = iter.GetStringStart();
        PRUint32 end = iter.GetStringEnd();
        NS_ASSERTION(aSource->IsClusterStart(start),
                     "Started word in the middle of a cluster...");
        NS_ASSERTION(end == aSource->GetLength() || aSource->IsClusterStart(end),
                     "Ended word in the middle of a cluster...");

        nsresult rv = AddGlyphRun(font, start - aStart + aDest);
        if (NS_FAILED(rv))
            return;
    }
}

void
gfxTextRun::SetSpaceGlyph(gfxFont *aFont, gfxContext *aContext, PRUint32 aCharIndex)
{
    PRUint32 spaceGlyph = aFont->GetSpaceGlyph();
    float spaceWidth = aFont->GetMetrics().spaceWidth;
    PRUint32 spaceWidthAppUnits = NS_lroundf(spaceWidth*mAppUnitsPerDevUnit);
    if (!spaceGlyph ||
        !CompressedGlyph::IsSimpleGlyphID(spaceGlyph) ||
        !CompressedGlyph::IsSimpleAdvance(spaceWidthAppUnits)) {
        gfxTextRunFactory::Parameters params = {
            aContext, nsnull, nsnull, nsnull, 0, mAppUnitsPerDevUnit
        };
        static const PRUint8 space = ' ';
        nsAutoPtr<gfxTextRun> textRun;
        textRun = mFontGroup->MakeTextRun(&space, 1, &params,
            gfxTextRunFactory::TEXT_IS_8BIT | gfxTextRunFactory::TEXT_IS_ASCII |
            gfxTextRunFactory::TEXT_IS_PERSISTENT);
        if (!textRun || !textRun->mCharacterGlyphs)
            return;
        CopyGlyphDataFrom(textRun, 0, 1, aCharIndex, PR_TRUE);
        return;
    }

    AddGlyphRun(aFont, aCharIndex);
    CompressedGlyph g;
    g.SetSimpleGlyph(spaceWidthAppUnits, spaceGlyph);
    SetCharacterGlyph(aCharIndex, g);
}

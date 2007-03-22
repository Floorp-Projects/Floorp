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
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#include "prtypes.h"
#include "prmem.h"
#include "nsString.h"

#include "gfxTypes.h"

#include "nsPromiseFlatString.h"

#include "gfxContext.h"
#include "gfxAtsuiFonts.h"

#include "gfxFontTest.h"

#include "cairo-atsui.h"

#include "gfxQuartzSurface.h"
#include "gfxQuartzFontCache.h"

// Uncomment this to dump all text runs created to stdout
// #define DUMP_TEXT_RUNS

#define ROUND(x) (floor((x) + 0.5))

/* We might still need this for fast-pathing, but we'll see */
#if 0
OSStatus ATSUGetStyleGroup(ATSUStyle style, void **styleGroup);
OSStatus ATSUDisposeStyleGroup(void *styleGroup);
OSStatus ATSUConvertCharToGlyphs(void *styleGroup,
                                 PRunichar *buffer
                                 unsigned int bufferLength,
                                 void *glyphVector);
OSStatus ATSInitializeGlyphVector(int size, void *glyphVectorPtr);
OSStatus ATSClearGlyphVector(void *glyphVectorPtr);
#endif

gfxAtsuiFont::gfxAtsuiFont(ATSUFontID fontID,
                           const gfxFontStyle *fontStyle)
    : gfxFont(EmptyString(), fontStyle),
      mFontStyle(fontStyle), mATSUFontID(fontID), mATSUStyle(nsnull),
      mAdjustedSize(0)
{
    ATSFontRef fontRef = FMGetATSFontRefFromFont(fontID);

    InitMetrics(fontID, fontRef);

    mFontFace = cairo_atsui_font_face_create_for_atsu_font_id(mATSUFontID);

    cairo_matrix_t sizeMatrix, ctm;
    cairo_matrix_init_identity(&ctm);
    cairo_matrix_init_scale(&sizeMatrix, mAdjustedSize, mAdjustedSize);

    cairo_font_options_t *fontOptions = cairo_font_options_create();
    mScaledFont = cairo_scaled_font_create(mFontFace, &sizeMatrix, &ctm, fontOptions);
    cairo_font_options_destroy(fontOptions);
}

void
gfxAtsuiFont::InitMetrics(ATSUFontID aFontID, ATSFontRef aFontRef)
{
    /* Create the ATSUStyle */

    ATSUAttributeTag styleTags[] = {
        kATSUFontTag,
        kATSUSizeTag,
        kATSUFontMatrixTag,
        kATSUKerningInhibitFactorTag
    };

    ByteCount styleArgSizes[] = {
        sizeof(ATSUFontID),
        sizeof(Fixed),
        sizeof(CGAffineTransform),
        sizeof(Fract)
    };

    gfxFloat size =
        PR_MAX(((mAdjustedSize != 0) ? mAdjustedSize : mStyle->size), 1.0f);

    //fprintf (stderr, "string: '%s', size: %f\n", NS_ConvertUTF16toUTF8(aString).get(), size);

    // fSize is in points (72dpi)
    Fixed fSize = FloatToFixed(size);
    ATSUFontID fid = aFontID;
    // make the font render right-side up
    CGAffineTransform transform = CGAffineTransformMakeScale(1, -1);
    // we can't do kerning until layout draws what it measures, instead of splitting things up
    Fract inhibitKerningFactor = FloatToFract(1.0);

    ATSUAttributeValuePtr styleArgs[] = {
        &fid,
        &fSize,
        &transform,
        &inhibitKerningFactor
    };

    if (mATSUStyle)
        ATSUDisposeStyle(mATSUStyle);

    ATSUCreateStyle(&mATSUStyle);
    ATSUSetAttributes(mATSUStyle,
                      sizeof(styleTags)/sizeof(ATSUAttributeTag),
                      styleTags,
                      styleArgSizes,
                      styleArgs);

    /* Now pull out the metrics */

    ATSFontMetrics atsMetrics;
    ATSFontGetHorizontalMetrics(aFontRef, kATSOptionFlagsDefault,
                                &atsMetrics);

    if (atsMetrics.xHeight)
        mMetrics.xHeight = atsMetrics.xHeight * size;
    else
        mMetrics.xHeight = GetCharHeight('x');

    if (mAdjustedSize == 0) {
        if (mStyle->sizeAdjust != 0) {
            gfxFloat aspect = mMetrics.xHeight / size;
            mAdjustedSize =
                PR_MAX(ROUND(size * (mStyle->sizeAdjust / aspect)), 1.0f);
            InitMetrics(aFontID, aFontRef);
            return;
        }
        mAdjustedSize = size;
    }

    mMetrics.emHeight = size;

    mMetrics.maxAscent = atsMetrics.ascent * size;
    mMetrics.maxDescent = - (atsMetrics.descent * size);

    mMetrics.maxHeight = mMetrics.maxAscent + mMetrics.maxDescent;

    if (mMetrics.maxHeight - mMetrics.emHeight > 0)
        mMetrics.internalLeading = mMetrics.maxHeight - mMetrics.emHeight; 
    else
        mMetrics.internalLeading = 0.0;
    mMetrics.externalLeading = atsMetrics.leading * size;

    mMetrics.emAscent = mMetrics.maxAscent * mMetrics.emHeight / mMetrics.maxHeight;
    mMetrics.emDescent = mMetrics.emHeight - mMetrics.emAscent;

    mMetrics.maxAdvance = atsMetrics.maxAdvanceWidth * size;

    if (atsMetrics.avgAdvanceWidth != 0.0)
        mMetrics.aveCharWidth =
            PR_MIN(atsMetrics.avgAdvanceWidth * size, GetCharWidth('x'));
    else
        mMetrics.aveCharWidth = GetCharWidth('x');

    mMetrics.underlineOffset = atsMetrics.underlinePosition * size;
    // ATSUI sometimes returns 0 for underline thickness, see bug 361576.
    mMetrics.underlineSize = PR_MAX(1.0f, atsMetrics.underlineThickness * size);

    mMetrics.subscriptOffset = mMetrics.xHeight;
    mMetrics.superscriptOffset = mMetrics.xHeight;

    mMetrics.strikeoutOffset = mMetrics.xHeight / 2.0;
    mMetrics.strikeoutSize = mMetrics.underlineSize;

    mMetrics.spaceWidth = GetCharWidth(' ');

#if 0
    fprintf (stderr, "Font: %p size: %f", this, size);
    fprintf (stderr, "    emHeight: %f emAscent: %f emDescent: %f\n", mMetrics.emHeight, mMetrics.emAscent, mMetrics.emDescent);
    fprintf (stderr, "    maxAscent: %f maxDescent: %f maxAdvance: %f\n", mMetrics.maxAscent, mMetrics.maxDescent, mMetrics.maxAdvance);
    fprintf (stderr, "    internalLeading: %f externalLeading: %f\n", mMetrics.externalLeading, mMetrics.internalLeading);
    fprintf (stderr, "    spaceWidth: %f aveCharWidth: %f xHeight: %f\n", mMetrics.spaceWidth, mMetrics.aveCharWidth, mMetrics.xHeight);
    fprintf (stderr, "    uOff: %f uSize: %f stOff: %f stSize: %f suOff: %f suSize: %f\n", mMetrics.underlineOffset, mMetrics.underlineSize, mMetrics.strikeoutOffset, mMetrics.strikeoutSize, mMetrics.superscriptOffset, mMetrics.subscriptOffset);
#endif
}

nsString
gfxAtsuiFont::GetUniqueName()
{
    return gfxQuartzFontCache::SharedFontCache()->GetPostscriptNameForFontID(mATSUFontID);
}

float
gfxAtsuiFont::GetCharWidth(PRUnichar c)
{
    // this sucks.  There is a faster way to go from a char -> glyphs, but it
    // requires using oodles of apple private interfaces.  If we start caching
    // gfxAtsuiFonts, then it might make sense to do that.
    ATSUTextLayout layout;

    UniCharCount one = 1;
    ATSUCreateTextLayoutWithTextPtr(&c, 0, 1, 1, 1, &one, &mATSUStyle, &layout);

    ATSTrapezoid trap;
    ItemCount numBounds;
    ATSUGetGlyphBounds(layout, FloatToFixed(0.0), FloatToFixed(0.0),
                       0, 1, kATSUseFractionalOrigins, 1, &trap, &numBounds);

    float f =
        FixedToFloat(PR_MAX(trap.upperRight.x, trap.lowerRight.x)) -
        FixedToFloat(PR_MIN(trap.upperLeft.x, trap.lowerLeft.x));

    ATSUDisposeTextLayout(layout);

    return f;
}

float
gfxAtsuiFont::GetCharHeight(PRUnichar c)
{
    // this sucks.  There is a faster way to go from a char -> glyphs, but it
    // requires using oodles of apple private interfaces.  If we start caching
    // gfxAtsuiFonts, then it might make sense to do that.
    ATSUTextLayout layout;

    UniCharCount one = 1;
    ATSUCreateTextLayoutWithTextPtr(&c, 0, 1, 1, 1, &one, &mATSUStyle, &layout);

    Rect rect;
    ATSUMeasureTextImage(layout, 0, 1, 0, 0, &rect);

    ATSUDisposeTextLayout(layout);

    return rect.bottom - rect.top;
}

gfxAtsuiFont::~gfxAtsuiFont()
{
    cairo_scaled_font_destroy(mScaledFont);
    cairo_font_face_destroy(mFontFace);

    ATSUDisposeStyle(mATSUStyle);
}

const gfxFont::Metrics&
gfxAtsuiFont::GetMetrics()
{
    return mMetrics;
}

gfxAtsuiFontGroup::gfxAtsuiFontGroup(const nsAString& families,
                                     const gfxFontStyle *aStyle)
    : gfxFontGroup(families, aStyle)
{
    ForEachFont(FindATSUFont, this);

    if (mFonts.Length() == 0) {
        // XXX this will generate a list of the lang groups for which we have no
        // default fonts for on the mac; we should fix this!
        // Known:
        // ja x-beng x-devanagari x-tamil x-geor x-ethi x-gujr x-mlym x-armn

        //fprintf (stderr, "gfxAtsuiFontGroup: %s [%s] -> %d fonts found\n", NS_ConvertUTF16toUTF8(families).get(), aStyle->langGroup.get(), mFonts.Length());

        // If we get here, we most likely didn't have a default font for
        // a specific langGroup.  Let's just pick the default OSX
        // user font.
        ATSUFontID fontID = gfxQuartzFontCache::SharedFontCache()->GetDefaultATSUFontID (aStyle);
        mFonts.AppendElement(new gfxAtsuiFont(fontID, aStyle));
    }

    // Create the fallback structure
    ATSUCreateFontFallbacks(&mFallbacks);

#define NUM_STATIC_FIDS 16
    ATSUFontID static_fids[NUM_STATIC_FIDS];
    ATSUFontID *fids;
    if (mFonts.Length() > NUM_STATIC_FIDS)
        fids = (ATSUFontID *) PR_Malloc(sizeof(ATSUFontID) * mFonts.Length());
    else
        fids = static_fids;

    for (unsigned int i = 0; i < mFonts.Length(); i++) {
        gfxAtsuiFont* atsuiFont = NS_STATIC_CAST(gfxAtsuiFont*, NS_STATIC_CAST(gfxFont*, mFonts[i]));
        fids[i] = atsuiFont->GetATSUFontID();
    }
    ATSUSetObjFontFallbacks(mFallbacks,
                            mFonts.Length(),
                            fids,
                            kATSUSequentialFallbacksPreferred /* kATSUSequentialFallbacksExclusive? */);

    if (fids != static_fids)
        PR_Free(fids);
}

PRBool
gfxAtsuiFontGroup::FindATSUFont(const nsAString& aName,
                                const nsACString& aGenericName,
                                void *closure)
{
    gfxAtsuiFontGroup *fontGroup = (gfxAtsuiFontGroup*) closure;
    const gfxFontStyle *fontStyle = fontGroup->GetStyle();

    gfxQuartzFontCache *fc = gfxQuartzFontCache::SharedFontCache();
    ATSUFontID fontID = fc->FindATSUFontIDForFamilyAndStyle (aName, fontStyle);

    if (fontID != kATSUInvalidFontID) {
        //printf ("FindATSUFont! %s %d -> %d\n", NS_ConvertUTF16toUTF8(aName).get(), fontStyle->weight, (int)fontID);
        fontGroup->mFonts.AppendElement(new gfxAtsuiFont(fontID, fontStyle));
    }

    return PR_TRUE;
}

gfxAtsuiFontGroup::~gfxAtsuiFontGroup()
{
    ATSUDisposeFontFallbacks(mFallbacks);
}

gfxFontGroup *
gfxAtsuiFontGroup::Copy(const gfxFontStyle *aStyle)
{
    return new gfxAtsuiFontGroup(mFamilies, aStyle);
}

static void
SetupClusterBoundaries(gfxTextRun *aTextRun, const PRUnichar *aString, PRUint32 aLength)
{
    TextBreakLocatorRef locator;
    OSStatus status = UCCreateTextBreakLocator(NULL, 0, kUCTextBreakClusterMask,
                                               &locator);
    if (status != noErr)
        return;
    UniCharArrayOffset breakOffset;
    status = UCFindTextBreak(locator, kUCTextBreakClusterMask, 0, aString, aLength,
                             0, &breakOffset);
    if (status != noErr) {
        UCDisposeTextBreakLocator(&locator);
        return;
    }
    NS_ASSERTION(breakOffset == 0, "Cluster should start at offset zero");
    gfxTextRun::CompressedGlyph g;
    while (breakOffset < aLength) {
        PRUint32 curOffset = breakOffset;
        status = UCFindTextBreak(locator, kUCTextBreakClusterMask,
                                 kUCTextBreakIterateMask,
                                 aString, aLength, curOffset, &breakOffset);
        if (status != noErr) {
            UCDisposeTextBreakLocator(&locator);
            return;
        }
        PRUint32 j;
        for (j = curOffset + 1; j < breakOffset; ++j) {
            aTextRun->SetCharacterGlyph(j, g.SetClusterContinuation());
        }
    }
    NS_ASSERTION(breakOffset == aLength, "Should have found a final break");
    UCDisposeTextBreakLocator(&locator);
}

gfxTextRun *
gfxAtsuiFontGroup::MakeTextRunInternal(const PRUnichar *aString, PRUint32 aLength,
                                       Parameters *aParams, PRUint32 aHeaderChars)
{
    // NS_ASSERTION(!(aParams->mFlags & TEXT_NEED_BOUNDING_BOX),
    //              "Glyph extents not yet supported");

    gfxTextRun *textRun = new gfxTextRun(aParams, aLength);
    if (!textRun)
        return nsnull;

    // There's a one-char header in the string and a one-char trailer
    textRun->RecordSurrogates(aString + aHeaderChars);
    if (!(aParams->mFlags & TEXT_IS_8BIT)) {
        SetupClusterBoundaries(textRun, aString + aHeaderChars, aLength);
    }

    InitTextRun(textRun, aString, aLength, aHeaderChars);
    return textRun;
}

#define UNICODE_LRO 0x202d
#define UNICODE_RLO 0x202e
#define UNICODE_PDF 0x202c

static void
AppendDirectionalIndicator(PRUint32 aFlags, nsAString& aString)
{
    static const PRUnichar overrides[2] = { UNICODE_LRO, UNICODE_RLO };
    aString.Append(overrides[(aFlags & gfxTextRunFactory::TEXT_IS_RTL) != 0]);
}

gfxTextRun *
gfxAtsuiFontGroup::MakeTextRun(const PRUnichar *aString, PRUint32 aLength,
                               Parameters *aParams)
{
    nsAutoString utf16;
    AppendDirectionalIndicator(aParams->mFlags, utf16);
    utf16.Append(aString, aLength);
    utf16.Append(UNICODE_PDF);
    return MakeTextRunInternal(utf16.get(), aLength, aParams, 1);
}

gfxTextRun *
gfxAtsuiFontGroup::MakeTextRun(const PRUint8 *aString, PRUint32 aLength,
                               Parameters *aParams)
{
    aParams->mFlags |= TEXT_IS_8BIT;
    nsDependentCSubstring cString(reinterpret_cast<const char*>(aString),
                                  reinterpret_cast<const char*>(aString + aLength));
    nsAutoString utf16;
    PRUint32 headerChars = 0;
    if (aParams->mFlags & TEXT_IS_RTL) {
      AppendDirectionalIndicator(aParams->mFlags, utf16);
      headerChars = 1;
    }
    AppendASCIItoUTF16(cString, utf16);
    if (aParams->mFlags & TEXT_IS_RTL) {
      utf16.Append(UNICODE_PDF);
    }
    return MakeTextRunInternal(utf16.get(), aLength, aParams, headerChars);
}

gfxAtsuiFont*
gfxAtsuiFontGroup::FindFontFor(ATSUFontID fid)
{
    gfxAtsuiFont *font;

    // In most cases, this will just be 1 -- maybe a
    // small number, so no need for any more complex
    // lookup
    for (PRUint32 i = 0; i < FontListLength(); i++) {
        font = GetFontAt(i);
        if (font->GetATSUFontID() == fid)
            return font;
    }

    font = new gfxAtsuiFont(fid, GetStyle());
    mFonts.AppendElement(font);

    return font;
}

/**
 * Simple wrapper for ATSU "direct data arrays"
 */
class AutoLayoutDataArrayPtr {
public:
    AutoLayoutDataArrayPtr(ATSULineRef aLineRef,
                           ATSUDirectDataSelector aSelector)
        : mLineRef(aLineRef), mSelector(aSelector)
    {
        OSStatus status =
            ATSUDirectGetLayoutDataArrayPtrFromLineRef(aLineRef,
                aSelector, PR_FALSE, &mArray, &mItemCount);
        if (status != noErr) {
            mArray = NULL;
            mItemCount = 0;
        }
    }
    ~AutoLayoutDataArrayPtr() {
        if (mArray) {
            ATSUDirectReleaseLayoutDataArrayPtr(mLineRef, mSelector, &mArray);
        }
    }

    void     *mArray;
    ItemCount mItemCount;

private:
    ATSULineRef            mLineRef;
    ATSUDirectDataSelector mSelector;
};

#define ATSUI_SPECIAL_GLYPH_ID 0xFFFF

/**
 * Calculate the advance in appunits of a run of ATSUI glyphs
 */
static PRInt32
GetAdvanceAppUnits(ATSLayoutRecord *aGlyphs, PRUint32 aGlyphCount,
                   PRUint32 aAppUnitsPerDevUnit)
{
    Fixed fixedAdvance = aGlyphs[aGlyphCount].realPos - aGlyphs->realPos;
    return PRInt32((PRInt64(fixedAdvance)*aAppUnitsPerDevUnit + (1 << 15)) >> 16);
}

/**
 * Given a run of ATSUI glyphs that should be treated as a single cluster/ligature,
 * store them in the textrun at the appropriate character and set the
 * other characters involved to be ligature/cluster continuations as appropriate.
 */ 
static void
SetGlyphsForCharacterGroup(ATSLayoutRecord *aGlyphs, PRUint32 aGlyphCount,
                           Fixed *aBaselineDeltas, PRUint32 aAppUnitsPerDevUnit,
                           gfxTextRun *aRun)
{
    NS_ASSERTION(aGlyphCount > 0, "Must set at least one glyph");
    PRUint32 firstOffset = aGlyphs[0].originalOffset;
    PRUint32 lastOffset = firstOffset;
    PRUint32 i;
    PRUint32 regularGlyphCount = 0;
    ATSLayoutRecord *displayGlyph = nsnull;
    PRBool inOrder = PR_TRUE;

    for (i = 0; i < aGlyphCount; ++i) {
        ATSLayoutRecord *glyph = &aGlyphs[i];
        PRUint32 offset = glyph->originalOffset;
        firstOffset = PR_MIN(firstOffset, offset);
        lastOffset = PR_MAX(lastOffset, offset);
        if (glyph->glyphID != ATSUI_SPECIAL_GLYPH_ID) {
            ++regularGlyphCount;
            displayGlyph = glyph;
        }
        if (i > 0 && aRun->IsRightToLeft() != (offset < aGlyphs[i - 1].originalOffset)) {
            inOrder = PR_FALSE;
        }
    }

    gfxTextRun::CompressedGlyph g;
    PRUint32 offset;
    for (offset = firstOffset + 2; offset <= lastOffset; offset += 2) {
        PRUint32 index = offset/2;
        if (!inOrder) {
            // Because the characters in this group were not in the textrun's
            // required order, we must make the entire group an indivisible cluster
            aRun->SetCharacterGlyph(index, g.SetClusterContinuation());
        } else if (!aRun->GetCharacterGlyphs()[index].IsClusterContinuation()) {
            aRun->SetCharacterGlyph(index, g.SetLigatureContinuation());
        }
    }

    // Grab total advance for all glyphs
    PRInt32 advance = GetAdvanceAppUnits(aGlyphs, aGlyphCount, aAppUnitsPerDevUnit);
    PRUint32 index = firstOffset/2;
    if (regularGlyphCount == 1) {
        if (advance >= 0 &&
            (!aBaselineDeltas || aBaselineDeltas[displayGlyph - aGlyphs] == 0) &&
            gfxTextRun::CompressedGlyph::IsSimpleAdvance(advance) &&
            gfxTextRun::CompressedGlyph::IsSimpleGlyphID(displayGlyph->glyphID)) {
            aRun->SetCharacterGlyph(index, g.SetSimpleGlyph(advance, displayGlyph->glyphID));
            return;
        }
    }

    nsAutoTArray<gfxTextRun::DetailedGlyph,10> detailedGlyphs;
    ATSLayoutRecord *advanceStart = aGlyphs;
    for (i = 0; i < aGlyphCount; ++i) {
        ATSLayoutRecord *glyph = &aGlyphs[i];
        if (glyph->glyphID != ATSUI_SPECIAL_GLYPH_ID || regularGlyphCount == 0) {
            if (detailedGlyphs.Length() > 0) {
                detailedGlyphs[detailedGlyphs.Length() - 1].mAdvance =
                    GetAdvanceAppUnits(advanceStart, glyph - advanceStart, aAppUnitsPerDevUnit);
                advanceStart = glyph;
            }

            gfxTextRun::DetailedGlyph *details = detailedGlyphs.AppendElement();
            if (!details)
                return;
            details->mIsLastGlyph = PR_FALSE;
            details->mGlyphID = glyph->glyphID;
            details->mXOffset = 0;
            details->mYOffset = !aBaselineDeltas ? 0.0f
                : FixedToFloat(aBaselineDeltas[i])*aAppUnitsPerDevUnit;
        }
    }
    if (detailedGlyphs.Length() == 0) {
        NS_WARNING("No glyphs visible at all!");
        aRun->SetCharacterGlyph(index, g.SetMissing());
        return;
    }

    detailedGlyphs[detailedGlyphs.Length() - 1].mIsLastGlyph = PR_TRUE;
    detailedGlyphs[detailedGlyphs.Length() - 1].mAdvance =
        GetAdvanceAppUnits(advanceStart, aGlyphs + aGlyphCount - advanceStart, aAppUnitsPerDevUnit);
    // Should pass unmatchedness here but for now we'll just not tell the textrun
    // whether these are "missing glyph" glyphs or not
    aRun->SetDetailedGlyphs(index, detailedGlyphs.Elements(), detailedGlyphs.Length());    
}

static void
PostLayoutCallback(ATSULineRef aLine, gfxTextRun *aRun,
                   const PRUnichar *aString, const PRPackedBool *aUnmatched)
{
    // AutoLayoutDataArrayPtr advanceDeltasArray(aLine, kATSUDirectDataAdvanceDeltaFixedArray);
    // Fixed *advanceDeltas = NS_STATIC_CAST(Fixed *, advanceDeltasArray.mArray);
    // AutoLayoutDataArrayPtr deviceDeltasArray(aLine, kATSUDirectDataDeviceDeltaSInt16Array);
    AutoLayoutDataArrayPtr baselineDeltasArray(aLine, kATSUDirectDataBaselineDeltaFixedArray);
    Fixed *baselineDeltas = NS_STATIC_CAST(Fixed *, baselineDeltasArray.mArray);
    AutoLayoutDataArrayPtr glyphRecordsArray(aLine, kATSUDirectDataLayoutRecordATSLayoutRecordCurrent);

    PRUint32 numGlyphs = glyphRecordsArray.mItemCount;
    if (numGlyphs == 0 || !glyphRecordsArray.mArray) {
        NS_WARNING("Failed to retrieve key glyph data");
        return;
    }
    ATSLayoutRecord *glyphRecords = NS_STATIC_CAST(ATSLayoutRecord *, glyphRecordsArray.mArray);
    NS_ASSERTION(!baselineDeltas || baselineDeltasArray.mItemCount == numGlyphs,
                 "Mismatched glyph counts");
    NS_ASSERTION(glyphRecords[numGlyphs - 1].flags & kATSGlyphInfoTerminatorGlyph,
                 "Last glyph should be a terminator glyph");
    --numGlyphs;
    if (numGlyphs == 0)
        return;

    PRUint32 appUnitsPerDevUnit = aRun->GetAppUnitsPerDevUnit();

    // ATSUI seems to have a bug where trailing whitespace in a run,
    // even after we've forced the direction with LRO/RLO/PDF, does not
    // necessarily get the required direction.
    // A specific testcase is "RLO space space PDF"; we get
    // glyphRecords[0] = { originalOffset:0, realPos:0; }
    // glyphRecords[1] = { originalOffset:2, realPos:>0 }
    // We count the number of glyphs that appear to be this sort of erroneous
    // whitespace.
    // In RTL situations, the bug manifests as the trailing whitespace characters
    // being rendered in LTR order at the end of the glyph array.
    // In LTR situations, the bug manifests as the trailing whitespace characters
    // being rendered in RTL order at the start of the glyph array.
    // Compensate for this bug now by detecting those characters, setting up
    // the glyphs for those characters, and then chopping those glyphs off
    // the glyph array we need to look at.
    PRUint32 stringTailOffset = aRun->GetLength() - 1;
    PRBool isRTL = aRun->IsRightToLeft();
    if (isRTL) {
        while (numGlyphs > 0 &&
               glyphRecords[numGlyphs - 1].originalOffset == stringTailOffset*2 &&
               aString[stringTailOffset] == ' ') {
            SetGlyphsForCharacterGroup(glyphRecords + numGlyphs - 1, 1,
                                       baselineDeltas ? baselineDeltas + numGlyphs - 1 : nsnull,
                                       appUnitsPerDevUnit, aRun);
            --stringTailOffset;
            --numGlyphs;
        }
    } else {
        while (numGlyphs > 0 &&
               glyphRecords[0].originalOffset == stringTailOffset*2 &&
               aString[stringTailOffset] == ' ') {
            SetGlyphsForCharacterGroup(glyphRecords, 1,
                                       baselineDeltas,
                                       appUnitsPerDevUnit, aRun);
            --stringTailOffset;
            --numGlyphs;
            ++glyphRecords;
        }
    }

    // Now process the rest of the glyphs, which should basically be in
    // the textrun's desired order, so process them in textrun order
    PRInt32 direction = PRInt32(aRun->GetDirection());
    while (numGlyphs > 0) {
        PRUint32 glyphIndex = isRTL ? numGlyphs - 1 : 0;
        PRUint32 lastOffset = glyphRecords[glyphIndex].originalOffset;
        PRUint32 glyphCount = 1;
        // Determine the glyphs for this group
        while (glyphCount < numGlyphs) {
            ATSLayoutRecord *glyph = &glyphRecords[glyphIndex + direction*glyphCount];
            PRUint32 glyphOffset = glyph->originalOffset;
            // Always add the current glyph to the group if it's for the same
            // character as a character whose glyph is already in the group,
            // or an earlier character. The latter can happen because ATSUI
            // sometimes visually reorders glyphs; e.g. DEVANAGARI VOWEL I
            // can have its glyph displayed before the glyph for the consonant that's
            // it's logically after (even though this is all left-to-right text).
            // In this case we need to make sure the glyph for the consonant
            // is added to the group containing the vowel.
            if (lastOffset < glyphOffset) {
                // We could be at the end of a character group
                if (glyph->glyphID != ATSUI_SPECIAL_GLYPH_ID) {
                    // Next character is a normal character, stop the group here
                    break;
                }
                if (aUnmatched && aUnmatched[glyphOffset/2]) {
                    // Next character is ummatched, so definitely stop the group here
                    break;
                }
                // Otherwise the next glyph is, we assume, a ligature continuation.
                // Record that this character too is part of the group
                lastOffset = glyphOffset;
            }
            ++glyphCount;
        }
        if (isRTL) {
            SetGlyphsForCharacterGroup(glyphRecords + numGlyphs - glyphCount,
                                       glyphCount,
                                       baselineDeltas ? baselineDeltas + numGlyphs - glyphCount : nsnull,
                                       appUnitsPerDevUnit, aRun);
        } else {
            SetGlyphsForCharacterGroup(glyphRecords,
                                       glyphCount, baselineDeltas,
                                       appUnitsPerDevUnit, aRun);
            glyphRecords += glyphCount;
            if (baselineDeltas) {
                baselineDeltas += glyphCount;
            }
        }
        numGlyphs -= glyphCount;
    }
}

struct PostLayoutCallbackClosure {
    gfxTextRun                  *mTextRun;
    const PRUnichar             *mString;
    // Either null or an array of stringlength booleans set to true for
    // each character that did not match any fonts
    nsAutoArrayPtr<PRPackedBool> mUnmatchedChars;
};

// This is really disgusting, but the ATSUI refCon thing is also disgusting
static PostLayoutCallbackClosure *gCallbackClosure = nsnull;

static OSStatus
PostLayoutOperationCallback(ATSULayoutOperationSelector iCurrentOperation, 
                            ATSULineRef iLineRef, 
                            UInt32 iRefCon, 
                            void *iOperationCallbackParameterPtr, 
                            ATSULayoutOperationCallbackStatus *oCallbackStatus)
{
    PostLayoutCallback(iLineRef, gCallbackClosure->mTextRun,
                       gCallbackClosure->mString, gCallbackClosure->mUnmatchedChars);
    *oCallbackStatus = kATSULayoutOperationCallbackStatusContinue;
    return noErr;
}

void
gfxAtsuiFontGroup::InitTextRun(gfxTextRun *aRun,
                               const PRUnichar *aString, PRUint32 aLength,
                               PRUint32 aHeaderChars)
{
    OSStatus status;
    gfxAtsuiFont *atsuiFont = GetFontAt(0);
    ATSUStyle mainStyle = atsuiFont->GetATSUStyle();
    nsTArray<ATSUStyle> stylesToDispose;

#ifdef DUMP_TEXT_RUNS
    NS_ConvertUTF16toUTF8 str(aString + 1, aLength);
    NS_ConvertUTF16toUTF8 families(mFamilies);
    printf("%p(%s) TEXTRUN \"%s\" ENDTEXTRUN\n", this, families.get(), str.get());
#endif

    UniCharCount runLengths = aLength;
    ATSUTextLayout layout;
    // The string is actually aLength + 2*aHeaderChars chars, with optionally
    // a header char to set the direction and a trailer char to pop it. So
    // create the text layout giving the whole string as context, although we
    // only want glyphs for the inner substring.
    status = ATSUCreateTextLayoutWithTextPtr
        (aString,
         aHeaderChars,
         aLength,
         aLength + aHeaderChars*2,
         1,
         &runLengths,
         &mainStyle,
         &layout);
    // XXX error check here?

    PostLayoutCallbackClosure closure;
    closure.mTextRun = aRun;
    // Pass the real string to the closure, ignoring the header
    closure.mString = aString + aHeaderChars;
    NS_ASSERTION(!gCallbackClosure, "Reentering InitTextRun? Expect disaster!");
    gCallbackClosure = &closure;

    ATSULayoutOperationOverrideSpecifier override;
    override.operationSelector = kATSULayoutOperationPostLayoutAdjustment;
    override.overrideUPP = PostLayoutOperationCallback;

    // Set up our layout attributes
    ATSLineLayoutOptions lineLayoutOptions = kATSLineKeepSpacesOutOfMargin | kATSLineHasNoHangers;

    static ATSUAttributeTag layoutTags[] = {
        kATSULineLayoutOptionsTag,
        kATSULineFontFallbacksTag,
        kATSULayoutOperationOverrideTag
    };
    static ByteCount layoutArgSizes[] = {
        sizeof(ATSLineLayoutOptions),
        sizeof(ATSUFontFallbacks),
        sizeof(ATSULayoutOperationOverrideSpecifier)
    };

    ATSUAttributeValuePtr layoutArgs[] = {
        &lineLayoutOptions,
        GetATSUFontFallbacksPtr(),
        &override
    };
    ATSUSetLayoutControls(layout,
                          NS_ARRAY_LENGTH(layoutTags),
                          layoutTags,
                          layoutArgSizes,
                          layoutArgs);

    /* Now go through and update the styles for the text, based on font matching. */

    UniCharArrayOffset runStart = aHeaderChars;
    UniCharCount totalLength = aLength + aHeaderChars;
    UniCharCount runLength = aLength;

    //fprintf (stderr, "==== Starting font maching [string length: %d]\n", totalLength);
    while (runStart < totalLength) {
        ATSUFontID substituteFontID;
        UniCharArrayOffset changedOffset;
        UniCharCount changedLength;

        OSStatus status = ATSUMatchFontsToText (layout, runStart, runLength,
                                                &substituteFontID, &changedOffset, &changedLength);
        if (status == noErr) {
            //fprintf (stderr, "ATSUMatchFontsToText returned noErr\n");
            // everything's good, finish up
            aRun->AddGlyphRun(atsuiFont, runStart - aHeaderChars);
            break;
        } else if (status == kATSUFontsMatched) {
            //fprintf (stderr, "ATSUMatchFontsToText returned kATSUFontsMatched: FID %d\n", substituteFontID);

            ATSUStyle subStyle;
            ATSUCreateStyle (&subStyle);
            ATSUCopyAttributes (mainStyle, subStyle);

            ATSUAttributeTag fontTags[] = { kATSUFontTag };
            ByteCount fontArgSizes[] = { sizeof(ATSUFontID) };
            ATSUAttributeValuePtr fontArgs[] = { &substituteFontID };

            ATSUSetAttributes (subStyle, 1, fontTags, fontArgSizes, fontArgs);

            if (changedOffset > runStart) {
                aRun->AddGlyphRun(atsuiFont, runStart - aHeaderChars);
            }

            ATSUSetRunStyle (layout, subStyle, changedOffset, changedLength);

            gfxAtsuiFont *font = FindFontFor(substituteFontID);
            if (font) {
                aRun->AddGlyphRun(font, changedOffset - aHeaderChars);
            }
            
            stylesToDispose.AppendElement(subStyle);
        } else if (status == kATSUFontsNotMatched) {
            //fprintf (stderr, "ATSUMatchFontsToText returned kATSUFontsNotMatched\n");
            /* I need to select the last resort font; how the heck do I do that? */
            // Record which font is associated with these glyphs, anyway
            aRun->AddGlyphRun(atsuiFont, runStart - aHeaderChars);
            
            if (!closure.mUnmatchedChars) {
                closure.mUnmatchedChars = new PRPackedBool[aLength];
                if (closure.mUnmatchedChars) {
                    memset(closure.mUnmatchedChars.get(), PR_FALSE, aLength);
                }
            }
            if (closure.mUnmatchedChars) {
                memset(closure.mUnmatchedChars.get() + changedOffset - aHeaderChars,
                       PR_TRUE, changedLength);
            }
        }

        //fprintf (stderr, "total length: %d changedOffset: %d changedLength: %d\p=n",  runLength, changedOffset, changedLength);

        runStart = changedOffset + changedLength;
        runLength = totalLength - runStart;
    }

    // Trigger layout so that our callback fires. We don't actually care about
    // the result of this call.
    ATSTrapezoid trap;
    ItemCount trapCount;
    ATSUGetGlyphBounds(layout, 0, 0, aHeaderChars, aLength,
                       kATSUseFractionalOrigins, 1, &trap, &trapCount); 

    ATSUDisposeTextLayout(layout);

    //fprintf (stderr, "==== End font matching\n");
    PRUint32 i;
    for (i = 0; i < stylesToDispose.Length(); ++i) {
        ATSUDisposeStyle(stylesToDispose[i]);
    }
    gCallbackClosure = nsnull;
}

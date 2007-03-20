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
                                       Parameters *aParams)
{
    // NS_ASSERTION(!(aParams->mFlags & TEXT_NEED_BOUNDING_BOX),
    //              "Glyph extents not yet supported");

    gfxTextRun *textRun = new gfxTextRun(aParams, aLength);
    if (!textRun)
        return nsnull;

    // There's a one-char header in the string and a one-char trailer
    textRun->RecordSurrogates(aString + 1);
    if (!(aParams->mFlags & TEXT_IS_8BIT)) {
        SetupClusterBoundaries(textRun, aString + 1, aLength);
    }

    InitTextRun(textRun, aString, aLength);
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
    return MakeTextRunInternal(utf16.get(), aLength, aParams);
}

gfxTextRun *
gfxAtsuiFontGroup::MakeTextRun(const PRUint8 *aString, PRUint32 aLength,
                               Parameters *aParams)
{
    aParams->mFlags |= TEXT_IS_8BIT;
    nsDependentCSubstring cString(reinterpret_cast<const char*>(aString),
                                  reinterpret_cast<const char*>(aString + aLength));
    nsAutoString utf16;
    AppendDirectionalIndicator(aParams->mFlags, utf16);
    AppendASCIItoUTF16(cString, utf16);
    utf16.Append(UNICODE_PDF);
    return MakeTextRunInternal(utf16.get(), aLength, aParams);
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

/**
 * Abstraction to iterate through an array in strange order. It just remaps
 * array indices. Tragically we mainly need this because of ATSUI bugs.
 * There are four cases we need to handle. Note that 0 <= i < L in all cases.
 * 1) f(i) = i (LTR, no bug, most common)
 * 2) f(i) = L - 1 - i (RTL, no bug, next most common)
 * 3) f(i) = i < N ? N - 1 - i : i; (compensate for ATSUI bug in RTL mode)
 * 4) f(i) = i < N ? i + L - N : L - 1 - i (compensate for ATSUI bug in LTR mode)
 * We collapse 2) into 3) by setting N=L.
 * We collapse 1) into 4) by setting N=L.
 */
class GlyphMapper {
public:
    GlyphMapper(PRBool aIsRTL, PRUint32 aL, PRUint32 aN)
        : mL(aL), mN(aN), mRTL(aIsRTL) {}
    PRUint32 MapIndex(PRUint32 aIndex)
    {
        if (mRTL) {
            return aIndex < mN ? mN - 1 - aIndex : aIndex;
        } else {
            return aIndex < mN ? aIndex + mL - mN : mL - 1 - aIndex;
        }
    }
private:
    PRUint32 mL, mN;
    PRBool   mRTL;
};

static void
PostLayoutCallback(ATSULineRef aLine, gfxTextRun *aRun,
                   const PRUnichar *aString)
{
    AutoLayoutDataArrayPtr advanceDeltasArray(aLine, kATSUDirectDataAdvanceDeltaFixedArray);
    AutoLayoutDataArrayPtr baselineDeltasArray(aLine, kATSUDirectDataBaselineDeltaFixedArray);
    AutoLayoutDataArrayPtr deviceDeltasArray(aLine, kATSUDirectDataDeviceDeltaSInt16Array);
    AutoLayoutDataArrayPtr glyphRecordsArray(aLine, kATSUDirectDataLayoutRecordATSLayoutRecordCurrent);

    PRUint32 numGlyphs = glyphRecordsArray.mItemCount;
    if (numGlyphs == 0 || !glyphRecordsArray.mArray) {
        NS_WARNING("Failed to retrieve key glyph data");
        return;
    }
    Fixed *advanceDeltas = NS_STATIC_CAST(Fixed *, advanceDeltasArray.mArray);
    Fixed *baselineDeltas = NS_STATIC_CAST(Fixed *, baselineDeltasArray.mArray);
    ATSLayoutRecord *glyphRecords = NS_STATIC_CAST(ATSLayoutRecord *, glyphRecordsArray.mArray);
    NS_ASSERTION((!advanceDeltas || advanceDeltasArray.mItemCount == numGlyphs) &&
                 (!baselineDeltas || baselineDeltasArray.mItemCount == numGlyphs),
                 "Mismatched glyph counts");
    NS_ASSERTION(glyphRecords[numGlyphs - 1].flags & kATSGlyphInfoTerminatorGlyph,
                 "Last glyph should be a terminator glyph");
    --numGlyphs;
    if (numGlyphs == 0)
        return;

    PRUint32 index = 0;
    ItemCount k = 0;
    PRUint32 length = aRun->GetLength();
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
    PRUint32 incorrectDirectionGlyphCount = 0;
    PRUint32 stringTailOffset = length - 1;
    while (glyphRecords[aRun->IsRightToLeft() ? numGlyphs - 1 - incorrectDirectionGlyphCount
                                              : incorrectDirectionGlyphCount].originalOffset == stringTailOffset*2 &&
           aString[stringTailOffset] == ' ') {
        ++incorrectDirectionGlyphCount;
        stringTailOffset--;
        if (incorrectDirectionGlyphCount == numGlyphs)
            break;
    }
    GlyphMapper mapper(aRun->IsRightToLeft(), numGlyphs, 
                       numGlyphs - incorrectDirectionGlyphCount);
    gfxTextRun::CompressedGlyph g;
    nsAutoTArray<gfxTextRun::DetailedGlyph,1> detailedGlyphs;
    PRBool gotRealCharacter = PR_FALSE;
    while (index < length) {
        ATSLayoutRecord *glyph = nsnull;
        // ATSUI sometimes moves glyphs around in visual order to handle
        // situations such as DEVANAGARI VOWEL I appearing before its base
        // character (even though it follows the base character in the text).
        // To handle this we make the reordered glyph(s) and the characters
        // they're reordered around into a single cluster.
        PRUint32 forceClusterGlyphs = 0;
        PRUint32 nextIndex = index + 1;
        if (k < numGlyphs) {
            glyph = &glyphRecords[mapper.MapIndex(k)];
            // originalOffset is in bytes, so we need to adjust index by 2 for comparisons
            if (glyph->originalOffset > 2*index) {
                // Detect the situation above. We assume that at most
                // two glyphs have been moved from after a run of characters
                // to visually before the run of characters, so the following glyph
                // or the next glyph is a glyph for the current character.
                // We can tweak this up if necessary...
                const PRUint32 MAX_GLYPHS_REORDERED = 10;
                PRUint32 reorderedGlyphs = 0;
                PRUint32 i;
                PRUint32 maxOffset = glyph->originalOffset;
                for (i = 1; k + i < numGlyphs; ++i) {
                    ATSLayoutRecord *nextGlyph = &glyphRecords[mapper.MapIndex(k + i)];
                    if (i == MAX_GLYPHS_REORDERED || nextGlyph->originalOffset == 2*index) {
                        reorderedGlyphs = i;
                        break;
                    }
                    maxOffset = PR_MAX(maxOffset, nextGlyph->originalOffset);
                }
                if (reorderedGlyphs > 0) {
                    forceClusterGlyphs = reorderedGlyphs;
                    while (k + forceClusterGlyphs < numGlyphs) {
                        ATSLayoutRecord *nextGlyph = &glyphRecords[mapper.MapIndex(k + forceClusterGlyphs)];
                        if (nextGlyph->originalOffset > maxOffset)
                            break;
                        ++forceClusterGlyphs;
                    }
                    nextIndex = maxOffset/2 + 1;
                }
            }
        }

        if (k == numGlyphs ||
            (glyph->originalOffset > 2*index && !forceClusterGlyphs)) {
            NS_ASSERTION(index > 0, "Continuation at the start of a run??");
            if (!aRun->GetCharacterGlyphs()[index].IsClusterContinuation()) {
                if (gotRealCharacter) {
                    // No glyphs for character 'index', it must be a ligature continuation
                    aRun->SetCharacterGlyph(index, g.SetLigatureContinuation());
                } else {
                    // Don't allow ligature continuations until we've actually
                    // got something for them to be a continuation of
                    aRun->SetCharacterGlyph(index, g.SetMissing());
                }
            }
        } else {
            NS_ASSERTION(glyph->originalOffset >= 2*index, "Lost some glyphs");
            PRUint32 glyphCount = 1;
            if (forceClusterGlyphs) {
                glyphCount = forceClusterGlyphs;
                PRUint32 i;
                // Mark all the characters that we forced to cluster, other
                // than this leading character, as cluster continuations.
                for (i = 0; i < forceClusterGlyphs; ++i) {
                    PRUint32 offset = glyphRecords[mapper.MapIndex(k + i)].originalOffset;
                    if (offset != 2*index) {
                        aRun->SetCharacterGlyph(offset/2, g.SetClusterContinuation());
                    }
                }
            } else {
                // Find all the glyphs associated with this character
                while (k + glyphCount < numGlyphs &&
                       glyphRecords[mapper.MapIndex(k + glyphCount)].originalOffset == 2*index) {
                    ++glyphCount;
                }
            }
            Fixed advance = glyph[1].realPos - glyph->realPos;
            PRUint32 advancePixels = advance >> 16;
            // "Fixed" values have their fraction in the low 16 bits
            if (glyphCount == 1 && advance >= 0 && (advance & 0xFFFF) == 0 &&
                (!baselineDeltas || baselineDeltas[mapper.MapIndex(k)] == 0) &&
                gfxTextRun::CompressedGlyph::IsSimpleAdvancePixels(advancePixels) &&
                gfxTextRun::CompressedGlyph::IsSimpleGlyphID(glyph->glyphID)) {
                aRun->SetCharacterGlyph(index, g.SetSimpleGlyph(advancePixels, glyph->glyphID));
            } else {
                if (detailedGlyphs.Length() < glyphCount) {
                    if (!detailedGlyphs.AppendElements(glyphCount - detailedGlyphs.Length()))
                        return;
                }
                PRUint32 i;
                float appUnitsPerDevUnit = aRun->GetAppUnitsPerDevUnit();
                for (i = 0; i < glyphCount; ++i) {
                    gfxTextRun::DetailedGlyph *details = &detailedGlyphs[i];
                    glyph = &glyphRecords[mapper.MapIndex(k + i)];
                    details->mIsLastGlyph = i == glyphCount - 1;
                    details->mGlyphID = glyph->glyphID;
                    float advanceAppUnits =
                      FixedToFloat(glyph[1].realPos - glyph->realPos)*appUnitsPerDevUnit;
                    details->mAdvance = ceilf(advanceAppUnits)/appUnitsPerDevUnit;
                    details->mXOffset = 0;
                    details->mYOffset = !baselineDeltas ? 0.0f
                        : FixedToFloat(baselineDeltas[mapper.MapIndex(k + i)]);
                }
                aRun->SetDetailedGlyphs(index, detailedGlyphs.Elements(), glyphCount);
            }
            gotRealCharacter = PR_TRUE;
            k += glyphCount;
        }
        
        index = nextIndex;
        if (index + 1 < length && NS_IS_HIGH_SURROGATE(aString[index])) {
            ++index;
        }
    }
}

struct PostLayoutCallbackClosure {
    gfxTextRun      *mTextRun;
    const PRUnichar *mString;
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
                       gCallbackClosure->mString);
    *oCallbackStatus = kATSULayoutOperationCallbackStatusContinue;
    return noErr;
}

void
gfxAtsuiFontGroup::InitTextRun(gfxTextRun *aRun,
                               const PRUnichar *aString, PRUint32 aLength)
{
    OSStatus status;
    gfxAtsuiFont *atsuiFont = GetFontAt(0);
    ATSUStyle mainStyle = atsuiFont->GetATSUStyle();
    nsTArray<ATSUStyle> stylesToDispose;

    UniCharCount runLengths = aLength;
    ATSUTextLayout layout;
    // The string is actually aLength + 2 chars, with a header char to set
    // the direction and a trailer char to pop it. So create the text layout
    // giving the whole string as context, although we only want glyphs for
    // the inner substring.
    status = ATSUCreateTextLayoutWithTextPtr
        (aString,
         1,
         aLength,
         aLength + 2,
         1,
         &runLengths,
         &mainStyle,
         &layout);
    // XXX error check here?

    PostLayoutCallbackClosure closure;
    closure.mTextRun = aRun;
    // Pass the real string to the closure, ignoring the header
    closure.mString = aString + 1;
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

    UniCharArrayOffset runStart = 1;
    UniCharCount totalLength = aLength + 1;
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
            aRun->AddGlyphRun(atsuiFont, runStart - 1);
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
                aRun->AddGlyphRun(atsuiFont, runStart - 1);
            }

            ATSUSetRunStyle (layout, subStyle, changedOffset, changedLength);

            gfxAtsuiFont *font = FindFontFor(substituteFontID);
            if (font) {
                aRun->AddGlyphRun(font, changedOffset - 1);
            }
            
            stylesToDispose.AppendElement(subStyle);
        } else if (status == kATSUFontsNotMatched) {
            //fprintf (stderr, "ATSUMatchFontsToText returned kATSUFontsNotMatched\n");
            /* I need to select the last resort font; how the heck do I do that? */
            // Record which font is associated with these glyphs, anyway
            aRun->AddGlyphRun(atsuiFont, runStart - 1);
        }

        //fprintf (stderr, "total length: %d changedOffset: %d changedLength: %d\p=n",  runLength, changedOffset, changedLength);

        runStart = changedOffset + changedLength;
        runLength = totalLength - runStart;
    }

    // Trigger layout so that our callback fires. We don't actually care about
    // the result of this call.
    ATSTrapezoid trap;
    ItemCount trapCount;
    ATSUGetGlyphBounds(layout, 0, 0, 1, aLength, kATSUseDeviceOrigins, 1, &trap, &trapCount); 

    ATSUDisposeTextLayout(layout);

    //fprintf (stderr, "==== End font matching\n");
    PRUint32 i;
    for (i = 0; i < stylesToDispose.Length(); ++i) {
        ATSUDisposeStyle(stylesToDispose[i]);
    }
    gCallbackClosure = nsnull;
}

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
#include "nsBidiUtils.h"

#include "gfxTypes.h"

#include "nsPromiseFlatString.h"

#include "gfxContext.h"
#include "gfxPlatform.h"
#include "gfxAtsuiFonts.h"

#include "gfxFontTest.h"

#include "cairo-atsui.h"

#include "gfxQuartzSurface.h"
#include "gfxQuartzFontCache.h"

#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsServiceManagerUtils.h"
#include "nsUnicodeRange.h"
#include "nsCRT.h"

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
                           const nsAString& name,
                           const gfxFontStyle *fontStyle)
    : gfxFont(name, fontStyle),
      mFontStyle(fontStyle), mATSUFontID(fontID), mATSUStyle(nsnull),
      mHasMirroring(PR_FALSE), mHasMirroringLookedUp(PR_FALSE), mAdjustedSize(0)
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
    NS_ASSERTION(cairo_scaled_font_status(mScaledFont) == CAIRO_STATUS_SUCCESS,
                 "Failed to create scaled font");
}

void
gfxAtsuiFont::InitMetrics(ATSUFontID aFontID, ATSFontRef aFontRef)
{
    /* Create the ATSUStyle */

    ATSUAttributeTag styleTags[] = {
        kATSUFontTag,
        kATSUSizeTag,
        kATSUFontMatrixTag
    };

    ByteCount styleArgSizes[] = {
        sizeof(ATSUFontID),
        sizeof(Fixed),
        sizeof(CGAffineTransform),
        sizeof(Fract)
    };

    gfxFloat size =
        PR_MAX(((mAdjustedSize != 0) ? mAdjustedSize : GetStyle()->size), 1.0f);

    //fprintf (stderr, "string: '%s', size: %f\n", NS_ConvertUTF16toUTF8(aString).get(), size);

    // fSize is in points (72dpi)
    Fixed fSize = FloatToFixed(size);
    ATSUFontID fid = aFontID;
    // make the font render right-side up
    CGAffineTransform transform = CGAffineTransformMakeScale(1, -1);

    ATSUAttributeValuePtr styleArgs[] = {
        &fid,
        &fSize,
        &transform,
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
        if (GetStyle()->sizeAdjust != 0) {
            gfxFloat aspect = mMetrics.xHeight / size;
            mAdjustedSize = GetStyle()->GetAdjustedSize(aspect);
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

    float xWidth = GetCharWidth('x');
    if (atsMetrics.avgAdvanceWidth != 0.0)
        mMetrics.aveCharWidth =
            PR_MIN(atsMetrics.avgAdvanceWidth * size, xWidth);
    else
        mMetrics.aveCharWidth = xWidth;

    if (gfxQuartzFontCache::SharedFontCache()->IsFixedPitch(aFontID)) {
        // Some Quartz fonts are fixed pitch, but there's some glyph with a bigger
        // advance than the average character width... this forces
        // those fonts to be recognized like fixed pitch fonts by layout.
        mMetrics.maxAdvance = mMetrics.aveCharWidth;
    }

    mMetrics.underlineOffset = -mMetrics.maxDescent - atsMetrics.underlinePosition * size;
    // ATSUI sometimes returns 0 for underline thickness, see bug 361576.
    mMetrics.underlineSize = PR_MAX(1.0, atsMetrics.underlineThickness * size);

    mMetrics.subscriptOffset = mMetrics.xHeight;
    mMetrics.superscriptOffset = mMetrics.xHeight;

    mMetrics.strikeoutOffset = mMetrics.xHeight / 2.0;
    mMetrics.strikeoutSize = mMetrics.underlineSize;

    PRUint32 glyphID;
    mMetrics.spaceWidth = GetCharWidth(' ', &glyphID);
    mSpaceGlyph = glyphID;

#if 0
    fprintf (stderr, "Font: %p size: %f (fixed: %d)", this, size, gfxQuartzFontCache::SharedFontCache()->IsFixedPitch(aFontID));
    fprintf (stderr, "    emHeight: %f emAscent: %f emDescent: %f\n", mMetrics.emHeight, mMetrics.emAscent, mMetrics.emDescent);
    fprintf (stderr, "    maxAscent: %f maxDescent: %f maxAdvance: %f\n", mMetrics.maxAscent, mMetrics.maxDescent, mMetrics.maxAdvance);
    fprintf (stderr, "    internalLeading: %f externalLeading: %f\n", mMetrics.externalLeading, mMetrics.internalLeading);
    fprintf (stderr, "    spaceWidth: %f aveCharWidth: %f xHeight: %f\n", mMetrics.spaceWidth, mMetrics.aveCharWidth, mMetrics.xHeight);
    fprintf (stderr, "    uOff: %f uSize: %f stOff: %f stSize: %f suOff: %f suSize: %f\n", mMetrics.underlineOffset, mMetrics.underlineSize, mMetrics.strikeoutOffset, mMetrics.strikeoutSize, mMetrics.superscriptOffset, mMetrics.subscriptOffset);
#endif
}

PRBool
gfxAtsuiFont::SetupCairoFont(cairo_t *aCR)
{
    cairo_scaled_font_t *scaledFont = CairoScaledFont();
    if (cairo_scaled_font_status(scaledFont) != CAIRO_STATUS_SUCCESS) {
        // Don't cairo_set_scaled_font as that would propagate the error to
        // the cairo_t, precluding any further drawing.
        return PR_FALSE;
    }
    cairo_set_scaled_font(aCR, scaledFont);
    return PR_TRUE;
}

nsString
gfxAtsuiFont::GetUniqueName()
{
    return mName;
}

float
gfxAtsuiFont::GetCharWidth(PRUnichar c, PRUint32 *aGlyphID)
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

    if (aGlyphID) {
        ATSUGlyphInfoArray glyphInfo;
        ByteCount bytes = sizeof(glyphInfo);
        ATSUGetGlyphInfo(layout, 0, 1, &bytes, &glyphInfo);
        *aGlyphID = glyphInfo.glyphs[0].glyphID;
    }

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

static nsresult
CreateFontFallbacksFromFontList(nsTArray< nsRefPtr<gfxFont> > *aFonts,
                                ATSUFontFallbacks *aFallbacks,
                                ATSUFontFallbackMethod aMethod)
{
    // Create the fallback structure
    OSStatus status = ::ATSUCreateFontFallbacks(aFallbacks);
    NS_ENSURE_TRUE(status == noErr, NS_ERROR_FAILURE);

    nsAutoTArray<ATSUFontID,16> fids;

    for (unsigned int i = 0; i < aFonts->Length(); i++) {
        gfxAtsuiFont* atsuiFont = static_cast<gfxAtsuiFont*>(aFonts->ElementAt(i).get());
        fids.AppendElement(atsuiFont->GetATSUFontID());
    }
    status = ::ATSUSetObjFontFallbacks(*aFallbacks, fids.Length(), fids.Elements(), aMethod);

    return status == noErr ? NS_OK : NS_ERROR_FAILURE;
}

PRBool 
gfxAtsuiFont::HasMirroringInfo()
{
    if (!mHasMirroringLookedUp) {
        OSStatus status;
        ByteCount size;
        
        // 361695 - if the font has a 'prop' table, assume that ATSUI will handle glyph mirroring
        status = ATSFontGetTable(GetATSUFontID(), 'prop', 0, 0, 0, &size);
        mHasMirroring = (status == noErr);
        mHasMirroringLookedUp = PR_TRUE;
    }
    
    return mHasMirroring;
}


/**
 * Look up the font in the gfxFont cache. If we don't find it, create one.
 * In either case, add a ref, append it to the aFonts array, and return it ---
 * except for OOM in which case we do nothing and return null.
 */
static gfxAtsuiFont *
GetOrMakeFont(ATSUFontID aFontID, const gfxFontStyle *aStyle,
              nsTArray<nsRefPtr<gfxFont> > *aFonts)
{
    const nsAString& name =
        gfxQuartzFontCache::SharedFontCache()->GetPostscriptNameForFontID(aFontID);
    nsRefPtr<gfxFont> font = gfxFontCache::GetCache()->Lookup(name, aStyle);
    if (!font) {
        font = new gfxAtsuiFont(aFontID, name, aStyle);
        if (!font)
            return nsnull;
        gfxFontCache::GetCache()->AddNew(font);
    }
    // Add it to aFonts without unncessary refcount adjustment
    nsRefPtr<gfxFont> *destination = aFonts->AppendElement();
    if (!destination)
        return nsnull;
    destination->swap(font);
    gfxFont *f = *destination;
    return static_cast<gfxAtsuiFont *>(f);
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
        GetOrMakeFont(fontID, aStyle, &mFonts);
    }

    CreateFontFallbacksFromFontList(&mFonts, &mFallbacks, kATSULastResortOnlyFallback);
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
        //fprintf (stderr, "..FindATSUFont: %s\n", NS_ConvertUTF16toUTF8(aName).get());
        GetOrMakeFont(fontID, fontStyle, &fontGroup->mFonts);
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
SetupClusterBoundaries(gfxTextRun *aTextRun, const PRUnichar *aString)
{
    TextBreakLocatorRef locator;
    OSStatus status = UCCreateTextBreakLocator(NULL, 0, kUCTextBreakClusterMask,
                                               &locator);
    if (status != noErr)
        return;
    UniCharArrayOffset breakOffset = 0;
    UCTextBreakOptions options = kUCTextBreakLeadingEdgeMask;
    PRUint32 length = aTextRun->GetLength();
    while (breakOffset < length) {
        UniCharArrayOffset next;
        status = UCFindTextBreak(locator, kUCTextBreakClusterMask, options,
                                 aString, length, breakOffset, &next);
        if (status != noErr)
            break;
        options |= kUCTextBreakIterateMask;
        PRUint32 i;
        for (i = breakOffset + 1; i < next; ++i) {
            gfxTextRun::CompressedGlyph g;
            aTextRun->SetCharacterGlyph(i, g.SetClusterContinuation());
        }
        breakOffset = next;
    }
    UCDisposeTextBreakLocator(&locator);
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

/**
 * Given a textrun and an offset into that textrun, we need to choose a length
 * for the substring of the textrun that we should analyze next. The length
 * should be <= aMaxLength if possible. It must always end at a cluster
 * boundary and it should end at the end of the textrun or at the
 * boundary of a space if possible.
 */
static PRUint32
FindTextRunSegmentLength(gfxTextRun *aTextRun, PRUint32 aOffset, PRUint32 aMaxLength)
{
    if (aOffset + aMaxLength >= aTextRun->GetLength()) {
        // The remaining part of the textrun fits within the max length,
        // so just use it.
        return aTextRun->GetLength() - aOffset;
    }

    // Try to end the segment before or after a space, since spaces don't kern
    // or ligate.
    PRUint32 end;
    for (end = aOffset + aMaxLength; end > aOffset; --end) {
        if (aTextRun->IsClusterStart(end) &&
            (aTextRun->GetChar(end) == ' ' || aTextRun->GetChar(end - 1) == ' '))
            return end - aOffset;
    }

    // Try to end the segment at the last cluster boundary.
    for (end = aOffset + aMaxLength; end > aOffset; --end) {
        if (aTextRun->IsClusterStart(end))
            return end - aOffset;
    }

    // I guess we just have to return a segment that's the entire cluster
    // starting at aOffset.
    for (end = aOffset + 1; end < aTextRun->GetLength(); ++end) {
        if (aTextRun->IsClusterStart(end))
            return end - aOffset;
    }
    return aTextRun->GetLength() - aOffset;
}

PRUint32
gfxAtsuiFontGroup::GuessMaximumStringLength()
{
    // ATSUI can't handle offsets of more than 32K pixels. This function
    // guesses a string length that ATSUI will be able to handle. We want to
    // get the right answer most of the time, but if we're wrong in either
    // direction, we won't break things: if we guess too large, our glyph
    // processing will detect ATSUI's failure and retry with a smaller limit.
    // If we guess too small, we'll just break the string into more pieces
    // than we strictly needed to.
    // The basic calculation is just 32k pixels divided by the font max-advance,
    // but we need to be a bit careful to avoid math errors.
    PRUint32 maxAdvance = PRUint32(GetFontAt(0)->GetMetrics().maxAdvance);
    PRUint32 chars = 0x7FFF/PR_MAX(1, maxAdvance);
    return PR_MAX(1, chars);
}

/*
 * ATSUI can't handle more than 32K pixels of text. We can easily have
 * textruns longer than that. Our strategy here is to divide the textrun up
 * into pieces each of which is less than 32K pixels wide. We pick a number
 * of characters 'maxLen' such that the first font's max-advance times that
 * number of characters is less than 32K pixels; then we try glyph conversion
 * of the string broken up into chunks each with no more than 'maxLen'
 * characters. That could fail (e.g. if fallback fonts are used); if it does,
 * we retry with a smaller maxLen. When breaking up the string into chunks
 * we prefer to break at space boundaries because spaces don't kern or ligate
 * with other characters, usually. We insist on breaking at cluster boundaries.
 * If the font size is incredibly huge and/or clusters are very large, this
 * could mean that we actually put more than 'maxLen' characters in a chunk.
 */

gfxTextRun *
gfxAtsuiFontGroup::MakeTextRun(const PRUnichar *aString, PRUint32 aLength,
                               const Parameters *aParams, PRUint32 aFlags)
{
    gfxTextRun *textRun = new gfxTextRun(aParams, aString, aLength, this, aFlags);
    if (!textRun)
        return nsnull;

    textRun->RecordSurrogates(aString);
    SetupClusterBoundaries(textRun, aString);

    PRUint32 maxLen;
    nsAutoString utf16;
    for (maxLen = GuessMaximumStringLength(); maxLen > 0; maxLen /= 2) {
        PRUint32 start = 0;
        while (start < aLength) {
            PRUint32 len = FindTextRunSegmentLength(textRun, start, maxLen);

            utf16.Truncate();
            AppendDirectionalIndicator(aFlags, utf16);
            utf16.Append(aString + start, len);
            // Ensure that none of the whitespace in the run is considered "trailing"
            // by ATSUI's bidi algorithm
            utf16.Append('.');
            utf16.Append(UNICODE_PDF);
            if (!InitTextRun(textRun, utf16.get(), utf16.Length(), PR_TRUE,
                             start, len) && maxLen > 1)
                break;
            start += len;
        }
        if (start == aLength)
            break;
        textRun->ResetGlyphRuns();
    }

    return textRun;
}

gfxTextRun *
gfxAtsuiFontGroup::MakeTextRun(const PRUint8 *aString, PRUint32 aLength,
                               const Parameters *aParams, PRUint32 aFlags)
{
    NS_ASSERTION(aFlags & TEXT_IS_8BIT, "should be marked 8bit");
    gfxTextRun *textRun = new gfxTextRun(aParams, aString, aLength, this, aFlags);
    if (!textRun)
        return nsnull;

    PRUint32 maxLen;
    nsAutoString utf16;
    for (maxLen = GuessMaximumStringLength(); maxLen > 0; maxLen /= 2) {
        PRUint32 start = 0;
        while (start < aLength) {
            PRUint32 len = FindTextRunSegmentLength(textRun, start, maxLen);

            nsDependentCSubstring cString(reinterpret_cast<const char*>(aString + start),
                                          reinterpret_cast<const char*>(aString + start + len));
            utf16.Truncate();
            PRBool wrapBidi = (aFlags & TEXT_IS_RTL) != 0;
            if (wrapBidi) {
              AppendDirectionalIndicator(aFlags, utf16);
            }
            AppendASCIItoUTF16(cString, utf16);
            if (wrapBidi) {
              utf16.Append('.');
              utf16.Append(UNICODE_PDF);
            }
            if (!InitTextRun(textRun, utf16.get(), utf16.Length(), wrapBidi,
                             start, len) && maxLen > 1)
                break;
            start += len;
        }
        if (start == aLength)
            break;
        textRun->ResetGlyphRuns();
    }

    return textRun;
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

    return GetOrMakeFont(fid, GetStyle(), &mFonts);
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

#define ATSUI_SPECIAL_GLYPH_ID       0xFFFF
/**
 * This flag seems to be set on glyphs that have overrun the 32K pixel
 * limit in ATSUI.
 */
#define ATSUI_OVERRUNNING_GLYPH_FLAG 0x100000

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
                           gfxTextRun *aRun, PRUint32 aSegmentStart,
                           const PRPackedBool *aUnmatched,
                           const PRUnichar *aString)
{
    NS_ASSERTION(aGlyphCount > 0, "Must set at least one glyph");
    PRUint32 firstOffset = aGlyphs[0].originalOffset;
    PRUint32 lastOffset = firstOffset;
    PRUint32 i;
    PRUint32 regularGlyphCount = 0;
    ATSLayoutRecord *displayGlyph = nsnull;
    PRBool inOrder = PR_TRUE;
    PRBool allMatched = PR_TRUE;

    for (i = 0; i < aGlyphCount; ++i) {
        ATSLayoutRecord *glyph = &aGlyphs[i];
        PRUint32 offset = glyph->originalOffset;
        firstOffset = PR_MIN(firstOffset, offset);
        lastOffset = PR_MAX(lastOffset, offset);
        if (aUnmatched && aUnmatched[offset/2]) {
            allMatched = PR_FALSE;
        }
        if (glyph->glyphID != ATSUI_SPECIAL_GLYPH_ID) {
            ++regularGlyphCount;
            displayGlyph = glyph;
        }
        if (i > 0 && aRun->IsRightToLeft() != (offset < aGlyphs[i - 1].originalOffset)) { // XXXkt allow == in RTL
            inOrder = PR_FALSE;
        }
    }

    NS_ASSERTION(!gfxFontGroup::IsInvalidChar(aString[firstOffset/2]),
                 "Invalid char passed in");

    if (!allMatched) {
        for (i = firstOffset; i <= lastOffset; ++i) {
            PRUint32 index = i/2;
            aRun->SetMissingGlyph(aSegmentStart + index, aString[index]);
        }
        return;
    }

    gfxTextRun::CompressedGlyph g;
    PRUint32 offset;
    for (offset = firstOffset + 2; offset <= lastOffset; offset += 2) {
        PRUint32 index = offset/2;
        if (!inOrder) {
            // Because the characters in this group were not in the textrun's
            // required order, we must make the entire group an indivisible cluster
            aRun->SetCharacterGlyph(aSegmentStart + index, g.SetClusterContinuation());
        } else if (!aRun->GetCharacterGlyphs()[index].IsClusterContinuation()) {
            aRun->SetCharacterGlyph(aSegmentStart + index, g.SetLigatureContinuation());
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
            aRun->SetCharacterGlyph(aSegmentStart + index, g.SetSimpleGlyph(advance, displayGlyph->glyphID));
            return;
        }
    }

    nsAutoTArray<gfxTextRun::DetailedGlyph,10> detailedGlyphs;
    ATSLayoutRecord *advanceStart = aGlyphs;
    for (i = 0; i < aGlyphCount; ++i) {
        ATSLayoutRecord *glyph = &aGlyphs[i];
        if (glyph->glyphID != ATSUI_SPECIAL_GLYPH_ID) {
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
        aRun->SetCharacterGlyph(aSegmentStart + index, g.SetMissing());
        return;
    }

    detailedGlyphs[detailedGlyphs.Length() - 1].mIsLastGlyph = PR_TRUE;
    detailedGlyphs[detailedGlyphs.Length() - 1].mAdvance =
        GetAdvanceAppUnits(advanceStart, aGlyphs + aGlyphCount - advanceStart, aAppUnitsPerDevUnit);
    aRun->SetDetailedGlyphs(aSegmentStart + index, detailedGlyphs.Elements(), detailedGlyphs.Length());    
}

/**
 * Returns true if there are overrunning glyphs
 */
static PRBool
PostLayoutCallback(ATSULineRef aLine, gfxTextRun *aRun,
                   const PRUnichar *aString, PRBool aWrapped,
                   const PRPackedBool *aUnmatched,
                   PRUint32 aSegmentStart, PRUint32 aSegmentLength)
{
    // AutoLayoutDataArrayPtr advanceDeltasArray(aLine, kATSUDirectDataAdvanceDeltaFixedArray);
    // Fixed *advanceDeltas = static_cast<Fixed *>(advanceDeltasArray.mArray);
    // AutoLayoutDataArrayPtr deviceDeltasArray(aLine, kATSUDirectDataDeviceDeltaSInt16Array);
    AutoLayoutDataArrayPtr baselineDeltasArray(aLine, kATSUDirectDataBaselineDeltaFixedArray);
    Fixed *baselineDeltas = static_cast<Fixed *>(baselineDeltasArray.mArray);
    AutoLayoutDataArrayPtr glyphRecordsArray(aLine, kATSUDirectDataLayoutRecordATSLayoutRecordCurrent);

    PRUint32 numGlyphs = glyphRecordsArray.mItemCount;
    if (numGlyphs == 0 || !glyphRecordsArray.mArray) {
        NS_WARNING("Failed to retrieve key glyph data");
        return PR_FALSE;
    }
    ATSLayoutRecord *glyphRecords = static_cast<ATSLayoutRecord *>(glyphRecordsArray.mArray);
    NS_ASSERTION(!baselineDeltas || baselineDeltasArray.mItemCount == numGlyphs,
                 "Mismatched glyph counts");
    NS_ASSERTION(glyphRecords[numGlyphs - 1].flags & kATSGlyphInfoTerminatorGlyph,
                 "Last glyph should be a terminator glyph");
    --numGlyphs;
    if (numGlyphs == 0)
        return PR_FALSE;

    PRUint32 appUnitsPerDevUnit = aRun->GetAppUnitsPerDevUnit();
    PRBool isRTL = aRun->IsRightToLeft();

    if (aWrapped) {
        // The glyph array includes a glyph for the artificial trailing
        // non-whitespace character. Strip that glyph from the array now.
        if (isRTL) {
            NS_ASSERTION(glyphRecords[0].originalOffset == aSegmentLength*2,
                         "Couldn't find glyph for trailing marker");
            glyphRecords++;
        } else {
            NS_ASSERTION(glyphRecords[numGlyphs - 1].originalOffset == aSegmentLength*2,
                         "Couldn't find glyph for trailing marker");
        }
        --numGlyphs;
        if (numGlyphs == 0)
            return PR_FALSE;
    }

    PRUint32 allFlags = 0;
    // Now process the glyphs, which should basically be in
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
            allFlags |= glyph->flags;
            // Always add the current glyph to the group if it's for the same
            // character as a character whose glyph is already in the group,
            // or an earlier character. The latter can happen because ATSUI
            // sometimes visually reorders glyphs; e.g. DEVANAGARI VOWEL I
            // can have its glyph displayed before the glyph for the consonant that's
            // it's logically after (even though this is all left-to-right text).
            // In this case we need to make sure the glyph for the consonant
            // is added to the group containing the vowel.
            if (lastOffset < glyphOffset) {
                if (!aRun->IsClusterStart(aSegmentStart + glyphOffset/2)) {
                    // next character is a cluster continuation,
                    // add it to the current group
                    lastOffset = glyphOffset;
                    continue;
                }
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
                                       appUnitsPerDevUnit, aRun, aSegmentStart,
                                       aUnmatched, aString);
        } else {
            SetGlyphsForCharacterGroup(glyphRecords,
                                       glyphCount, baselineDeltas,
                                       appUnitsPerDevUnit, aRun, aSegmentStart,
                                       aUnmatched, aString);
            glyphRecords += glyphCount;
            if (baselineDeltas) {
                baselineDeltas += glyphCount;
            }
        }
        numGlyphs -= glyphCount;
    }
    
    return (allFlags & ATSUI_OVERRUNNING_GLYPH_FLAG) != 0;
}

struct PostLayoutCallbackClosure {
    gfxTextRun                  *mTextRun;
    const PRUnichar             *mString;
    // Either null or an array of stringlength booleans set to true for
    // each character that did not match any fonts
    nsAutoArrayPtr<PRPackedBool> mUnmatchedChars;
    PRUint32                     mSegmentStart;
    PRUint32                     mSegmentLength;
    // This is true when we inserted an artifical trailing character at the
    // end of the string when computing the ATSUI layout.
    PRPackedBool                 mWrapped;
    // The callback *sets* this to indicate whether there were overrunning glyphs
    PRPackedBool                 mOverrunningGlyphs;
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
    gCallbackClosure->mOverrunningGlyphs =
        PostLayoutCallback(iLineRef, gCallbackClosure->mTextRun,
                           gCallbackClosure->mString, gCallbackClosure->mWrapped,
                           gCallbackClosure->mUnmatchedChars,
                           gCallbackClosure->mSegmentStart,
                           gCallbackClosure->mSegmentLength);
    *oCallbackStatus = kATSULayoutOperationCallbackStatusContinue;
    return noErr;
}

// The lang IDs for font prefs
enum eFontPrefLang {
    eFontPrefLang_Western     =  0,
    eFontPrefLang_CentEuro    =  1,
    eFontPrefLang_Japanese    =  2,
    eFontPrefLang_ChineseTW   =  3,
    eFontPrefLang_ChineseCN   =  4,
    eFontPrefLang_ChineseHK   =  5,
    eFontPrefLang_Korean      =  6,
    eFontPrefLang_Cyrillic    =  7,
    eFontPrefLang_Baltic      =  8,
    eFontPrefLang_Greek       =  9,
    eFontPrefLang_Turkish     = 10,
    eFontPrefLang_Thai        = 11,
    eFontPrefLang_Hebrew      = 12,
    eFontPrefLang_Arabic      = 13,
    eFontPrefLang_Devanagari  = 14,
    eFontPrefLang_Tamil       = 15,
    eFontPrefLang_Armenian    = 16,
    eFontPrefLang_Bengali     = 17,
    eFontPrefLang_Canadian    = 18,
    eFontPrefLang_Ethiopic    = 19,
    eFontPrefLang_Georgian    = 20,
    eFontPrefLang_Gujarati    = 21,
    eFontPrefLang_Gurmukhi    = 22,
    eFontPrefLang_Khmer       = 23,
    eFontPrefLang_Malayalam   = 24,

    eFontPrefLang_LangCount   = 25, // except Others and UserDefined.

    eFontPrefLang_Others      = 25, // x-unicode
    eFontPrefLang_UserDefined = 26,

    eFontPrefLang_CJKSet      = 27, // special code for CJK set
    eFontPrefLang_AllCount    = 28
};

// The lang names for eFontPrefLang
static const char *gPrefLangNames[] = {
    "x-western",
    "x-central-euro",
    "ja",
    "zh-TW",
    "zh-CN",
    "zh-HK",
    "ko",
    "x-cyrillic",
    "x-baltic",
    "el",
    "tr",
    "th",
    "he",
    "ar",
    "x-devanagari",
    "x-tamil",
    "x-armn",
    "x-beng",
    "x-cans",
    "x-ethi",
    "x-geor",
    "x-gujr",
    "x-guru",
    "x-khmr",
    "x-mlym",
    "x-unicode",
    "x-user-def"
};

static eFontPrefLang
GetFontPrefLangFor(const char* aLang)
{
    if (!aLang || aLang[0])
        return eFontPrefLang_Others;
    for (PRUint32 i = 0; i < PRUint32(eFontPrefLang_LangCount); ++i) {
        if (!PL_strcasecmp(gPrefLangNames[i], aLang))
            return eFontPrefLang(i);
    }
    return eFontPrefLang_Others;
}

eFontPrefLang
GetFontPrefLangFor(PRUint8 aUnicodeRange)
{
    switch (aUnicodeRange) {
        case kRangeCyrillic:   return eFontPrefLang_Cyrillic;
        case kRangeGreek:      return eFontPrefLang_Greek;
        case kRangeTurkish:    return eFontPrefLang_Turkish;
        case kRangeHebrew:     return eFontPrefLang_Hebrew;
        case kRangeArabic:     return eFontPrefLang_Arabic;
        case kRangeBaltic:     return eFontPrefLang_Baltic;
        case kRangeThai:       return eFontPrefLang_Thai;
        case kRangeKorean:     return eFontPrefLang_Korean;
        case kRangeJapanese:   return eFontPrefLang_Japanese;
        case kRangeSChinese:   return eFontPrefLang_ChineseCN;
        case kRangeTChinese:   return eFontPrefLang_ChineseTW;
        case kRangeDevanagari: return eFontPrefLang_Devanagari;
        case kRangeTamil:      return eFontPrefLang_Tamil;
        case kRangeArmenian:   return eFontPrefLang_Armenian;
        case kRangeBengali:    return eFontPrefLang_Bengali;
        case kRangeCanadian:   return eFontPrefLang_Canadian;
        case kRangeEthiopic:   return eFontPrefLang_Ethiopic;
        case kRangeGeorgian:   return eFontPrefLang_Georgian;
        case kRangeGujarati:   return eFontPrefLang_Gujarati;
        case kRangeGurmukhi:   return eFontPrefLang_Gurmukhi;
        case kRangeKhmer:      return eFontPrefLang_Khmer;
        case kRangeMalayalam:  return eFontPrefLang_Malayalam;
        case kRangeSetCJK:     return eFontPrefLang_CJKSet;
        default:               return eFontPrefLang_Others;
    }
}

static const char*
GetPrefLangName(eFontPrefLang aLang)
{
    if (PRUint32(aLang) < PRUint32(eFontPrefLang_AllCount))
        return gPrefLangNames[PRUint32(aLang)];
    return nsnull;
}

struct AFLClosure {
    const gfxFontStyle *style;
    nsTArray<nsRefPtr<gfxFont> > *fontArray;
};

PRBool
AppendFontToList(const nsAString& aName,
                 const nsACString& aGenericName,
                 void *closure)
{
    struct AFLClosure *afl = (struct AFLClosure *) closure;

    gfxQuartzFontCache *fc = gfxQuartzFontCache::SharedFontCache();
    ATSUFontID fontID = fc->FindATSUFontIDForFamilyAndStyle (aName, afl->style);

    if (fontID != kATSUInvalidFontID) {
        //fprintf (stderr, "..AppendFontToList: %s\n", NS_ConvertUTF16toUTF8(aName).get());
        GetOrMakeFont(fontID, afl->style, afl->fontArray);
    }

    return PR_TRUE;
}

static nsresult
AppendPrefFonts(nsTArray<nsRefPtr<gfxFont> > *aFonts,
                eFontPrefLang aLang,
                PRUint32& didAppendBits,
                const gfxFontStyle *aStyle)
{
    if (didAppendBits & (1 << aLang))
        return NS_OK;

    didAppendBits |= (1 << aLang);

    const char* langGroup = GetPrefLangName(aLang);
    if (!langGroup || !langGroup[0]) {
        NS_ERROR("The langGroup is null");
        return NS_ERROR_FAILURE;
    }
    gfxPlatform *platform = gfxPlatform::GetPlatform();
    NS_ENSURE_TRUE(platform, NS_ERROR_OUT_OF_MEMORY);
    nsString fonts;
    platform->GetPrefFonts(langGroup, fonts, PR_FALSE);
    if (fonts.IsEmpty())
        return NS_OK;

    struct AFLClosure afl = { aStyle, aFonts };
    gfxFontGroup::ForEachFont(fonts, nsDependentCString(langGroup),
                              AppendFontToList, &afl);
    return NS_OK;
}

static nsresult
AppendCJKPrefFonts(nsTArray<nsRefPtr<gfxFont> > *aFonts,
                   PRUint32& didAppendBits,
                   const gfxFontStyle *aStyle)
{
    nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    NS_ENSURE_TRUE(prefs, NS_ERROR_OUT_OF_MEMORY);

    nsCOMPtr<nsIPrefBranch> prefBranch;
    prefs->GetBranch(nsnull, getter_AddRefs(prefBranch));
    NS_ENSURE_TRUE(prefBranch, NS_ERROR_OUT_OF_MEMORY);

    // Add the CJK pref fonts from accept languages, the order should be same order
    nsXPIDLCString list;
    nsresult rv = prefBranch->GetCharPref("intl.accept_languages", getter_Copies(list));
    if (NS_SUCCEEDED(rv) && !list.IsEmpty()) {
        const char kComma = ',';
        const char *p, *p_end;
        list.BeginReading(p);
        list.EndReading(p_end);
        while (p < p_end) {
            while (nsCRT::IsAsciiSpace(*p)) {
                if (++p == p_end)
                    break;
            }
            if (p == p_end)
                break;
            const char *start = p;
            while (++p != p_end && *p != kComma)
                /* nothing */ ;
            nsCAutoString lang(Substring(start, p));
            lang.CompressWhitespace(PR_FALSE, PR_TRUE);
            eFontPrefLang fpl = GetFontPrefLangFor(lang.get());
            switch (fpl) {
                case eFontPrefLang_Japanese:
                case eFontPrefLang_Korean:
                case eFontPrefLang_ChineseCN:
                case eFontPrefLang_ChineseHK:
                case eFontPrefLang_ChineseTW:
                    rv = AppendPrefFonts(aFonts, fpl, didAppendBits, aStyle);
                    NS_ENSURE_SUCCESS(rv, rv);
                    break;
                default:
                    break;
            }
            p++;
        }
    }

    // Prefer the system locale if it is CJK.
    ScriptCode sysScript = ::GetScriptManagerVariable(smSysScript);
    // XXX Is not there the HK locale?
    switch (sysScript) {
        case smJapanese:    rv = AppendPrefFonts(aFonts, eFontPrefLang_Japanese, didAppendBits, aStyle);  break;
        case smTradChinese: rv = AppendPrefFonts(aFonts, eFontPrefLang_ChineseTW, didAppendBits, aStyle); break;
        case smKorean:      rv = AppendPrefFonts(aFonts, eFontPrefLang_Korean, didAppendBits, aStyle);    break;
        case smSimpChinese: rv = AppendPrefFonts(aFonts, eFontPrefLang_ChineseCN, didAppendBits, aStyle); break;
        default:            rv = NS_OK;
    }
    NS_ENSURE_SUCCESS(rv, rv);

    // last resort... (the order is same as old gfx.)
    rv = AppendPrefFonts(aFonts, eFontPrefLang_Japanese, didAppendBits, aStyle);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = AppendPrefFonts(aFonts, eFontPrefLang_Korean, didAppendBits, aStyle);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = AppendPrefFonts(aFonts, eFontPrefLang_ChineseCN, didAppendBits, aStyle);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = AppendPrefFonts(aFonts, eFontPrefLang_ChineseHK, didAppendBits, aStyle);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = AppendPrefFonts(aFonts, eFontPrefLang_ChineseTW, didAppendBits, aStyle);
    return rv;
}

static void
AddGlyphRun(gfxTextRun *aRun, gfxAtsuiFont *aFont, PRUint32 aOffset)
{
    //fprintf (stderr, "+ AddGlyphRun: %d %s\n", aOffset, NS_ConvertUTF16toUTF8(aFont->GetUniqueName()).get());
    aRun->AddGlyphRun(aFont, aOffset, PR_TRUE);
    if (!aRun->IsClusterStart(aOffset)) {
        // Glyph runs must start at cluster boundaries. However, sometimes
        // ATSUI matches different fonts for characters in the same cluster.
        // If this happens, break up the cluster. It's not clear what else
        // we can do.
        NS_WARNING("Font mismatch inside cluster");
        gfxTextRun::CompressedGlyph g;
        aRun->SetCharacterGlyph(aOffset, g.SetMissing());
    }
}

static void
DisableOptionalLigaturesInStyle(ATSUStyle aStyle)
{
    static ATSUFontFeatureType selectors[] = {
        kCommonLigaturesOffSelector,
        kRareLigaturesOffSelector,
        kLogosOffSelector,
        kRebusPicturesOffSelector,
        kDiphthongLigaturesOffSelector,
        kSquaredLigaturesOffSelector,
        kAbbrevSquaredLigaturesOffSelector,
        kSymbolLigaturesOffSelector
    };
    static ATSUFontFeatureType types[NS_ARRAY_LENGTH(selectors)] = {
        kLigaturesType,
        kLigaturesType,
        kLigaturesType,
        kLigaturesType,
        kLigaturesType,
        kLigaturesType,
        kLigaturesType,
        kLigaturesType
    };
    ATSUSetFontFeatures(aStyle, NS_ARRAY_LENGTH(selectors), types, selectors);
}

// 361695 - ATSUI only does glyph mirroring when the font contains a 'prop' table
// with glyph mirroring info, the character mirroring has to be done manually in the 
// fallback case.  Only used for RTL text runs.  The autoptr for the mirrored copy
// is owned by the calling routine.

// MirrorSubstring - Do Unicode mirroring on characters within a substring.  If mirroring 
// needs to be done, copy the original string and change the ATSUI layout to use the mirrored copy.
//
//   @param layout      ATSUI layout for the entire text run
//   @param mirrorStr   container used for mirror string, null until a mirrored character is found
//   @param aString     original string
//   @param aLength     length of the original string
//   @param runStart    start offset of substring to be mirrored
//   @param runLength   length of substring to be mirrored

static void MirrorSubstring(ATSUTextLayout layout, nsAutoArrayPtr<PRUnichar>& mirroredStr,
                            const PRUnichar *aString, PRUint32 aLength,
                            UniCharArrayOffset runStart, UniCharCount runLength)
{
    UniCharArrayOffset  off;
    
    // do the mirroring manually!!
    for (off = runStart; off < runStart + runLength; off++) {
        PRUnichar  mirroredChar;
        
        mirroredChar = (PRUnichar) SymmSwap(aString[off]);
        if (mirroredChar != aString[off]) {
            // string contains characters that need to be mirrored
            if (mirroredStr == NULL) {
            
                // copy the string
                mirroredStr = new PRUnichar[aLength];
                memcpy(mirroredStr, aString, sizeof(PRUnichar) * aLength);
                
                // adjust the layout
                ATSUTextMoved(layout, mirroredStr);
                
            }
            mirroredStr[off] = mirroredChar;
        }
    }
}

PRBool
gfxAtsuiFontGroup::InitTextRun(gfxTextRun *aRun,
                               const PRUnichar *aString, PRUint32 aLength,
                               PRBool aWrapped, PRUint32 aSegmentStart,
                               PRUint32 aSegmentLength)
{
    OSStatus status;
    gfxAtsuiFont *firstFont = GetFontAt(0);
    ATSUStyle mainStyle = firstFont->GetATSUStyle();
    nsTArray<ATSUStyle> stylesToDispose;
    PRUint32 headerChars = aWrapped ? 1 : 0;
    const PRUnichar *realString = aString + headerChars;
    NS_ASSERTION(aSegmentLength == aLength - (aWrapped ? 3 : 0),
                 "Length mismatch");

#ifdef DUMP_TEXT_RUNS
    NS_ConvertUTF16toUTF8 str(realString, aSegmentLength);
    NS_ConvertUTF16toUTF8 families(mFamilies);
    printf("%p(%s) TEXTRUN \"%s\" ENDTEXTRUN\n", this, families.get(), str.get());
#endif

    if (aRun->GetFlags() & TEXT_DISABLE_OPTIONAL_LIGATURES) {
        status = ATSUCreateAndCopyStyle(mainStyle, &mainStyle);
        if (status == noErr) {
            stylesToDispose.AppendElement(mainStyle);
            DisableOptionalLigaturesInStyle(mainStyle);
        }
    }

    UniCharCount runLengths = aSegmentLength;
    ATSUTextLayout layout;
    // Create the text layout for the whole string, but only produce glyphs
    // for the text inside LRO/RLO - PDF, if present. For wrapped strings
    // we do need to produce glyphs for the trailing non-whitespace
    // character to ensure that ATSUI treats all whitespace as non-trailing.
    status = ATSUCreateTextLayoutWithTextPtr
        (aString,
         headerChars,
         aSegmentLength + (aWrapped ? 1 : 0),
         aLength,
         1,
         &runLengths,
         &mainStyle,
         &layout);
    // XXX error check here?

    PostLayoutCallbackClosure closure;
    closure.mTextRun = aRun;
    closure.mString = realString;
    closure.mWrapped = aWrapped;
    closure.mSegmentStart = aSegmentStart;
    closure.mSegmentLength = aSegmentLength;
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

    nsAutoArrayPtr<PRUnichar> mirroredStr;
    nsTArray<PRUint32> missingOffsetsAndLengths;
    nsTArray<ATSUFontFallbacks> fallbacksToDispose;

    PRBool firstTime = PR_TRUE;

    UniCharArrayOffset runStart = headerChars;
    UniCharCount runLength = aSegmentLength;
    UniCharCount totalLength = headerChars + aSegmentLength;
    
    nsresult rv;

    //fprintf (stderr, "==== Starting font maching [string length: %d (%s)]\n", totalLength, NS_ConvertUTF16toUTF8(aString, aLength).get());
    do {
        PRUint32 missingRanges = missingOffsetsAndLengths.Length() / 2;
        UniCharArrayOffset missingStart = 0;
        UniCharCount missingLength = 0;

        //fprintf (stderr, "(loop start, missing ranges: %d)\n", missingRanges);

        if (missingRanges > 0) {
            // we had some misses.  Let's figure out what languages we need
            // and pull in the fallback fonts from prefs.

            missingStart = missingOffsetsAndLengths[(missingRanges-1) * 2];
            missingLength = missingOffsetsAndLengths[(missingRanges-1) * 2 + 1];
            missingOffsetsAndLengths.RemoveElementsAt((missingRanges-1) * 2, 2);

            runStart = missingStart;
            runLength = missingLength;

            //fprintf (stderr, ".. range: %d %d\n", runStart, runLength);

            totalLength = runStart + runLength;

            PRUint32 didAppendFonts = 0;
            PRUint32 lastRange = kRangeTableBase;

            nsAutoTArray<nsRefPtr<gfxFont>,3> mLangFonts;

            for (PRUint32 j = 0; j < runLength; j++) {
                PRUint32 unicodeRange = FindCharUnicodeRange(aString[j+runStart]);
                if (unicodeRange == lastRange)
                    continue;

                lastRange = unicodeRange;
                eFontPrefLang prefLang = GetFontPrefLangFor(unicodeRange);

                if (prefLang == eFontPrefLang_CJKSet)
                    rv = AppendCJKPrefFonts(&mLangFonts, didAppendFonts, GetStyle());
                else
                    rv = AppendPrefFonts(&mLangFonts, prefLang, didAppendFonts, GetStyle());
                if (NS_FAILED(rv))
                    break;
            }

            if (NS_FAILED(rv))
                break;

            // always append Unicode
            rv = AppendPrefFonts(&mLangFonts, eFontPrefLang_Others, didAppendFonts, GetStyle());
            if (NS_FAILED(rv))
                break;

            // now we have a list of fonts in mLangFonts (potentially -- 0-length is ok)
            ATSUFontFallbacks langFallbacks;
            rv = CreateFontFallbacksFromFontList(&mLangFonts, &langFallbacks, kATSUSequentialFallbacksPreferred);
            if (NS_FAILED(rv))
                break;

            fallbacksToDispose.AppendElement(langFallbacks);

            static ATSUAttributeTag fallbackTags[] = { kATSULineFontFallbacksTag };
            static ByteCount fallbackArgSizes[] = { sizeof(ATSUFontFallbacks) };
            ATSUAttributeValuePtr fallbackArgs[] = { &langFallbacks };

            ATSUSetLayoutControls(layout,
                                  NS_ARRAY_LENGTH(fallbackTags),
                                  fallbackTags, fallbackArgSizes, fallbackArgs);
        }

        while (runStart < totalLength) {
            ATSUFontID substituteFontID;
            UniCharArrayOffset changedOffset;
            UniCharCount changedLength;
            UniCharCount foundCharacters;

            OSStatus status = ATSUMatchFontsToText(layout, runStart, runLength,
                                                   &substituteFontID, &changedOffset, &changedLength);

            if (status == noErr) {
                foundCharacters = runLength;
                changedLength = 0;
            } else {
                foundCharacters = changedOffset - runStart;
            }

            // first, handle any characters that were matched with firstFont
            if (foundCharacters) {
                //fprintf (stderr, "... glyphs found in first: %d %d\n", runStart, foundCharacters);
                // glyphs exist for all characters in the [runStart, runStart + foundCharacters) substring

                // in the RTL case, handle fallback mirroring
                if (aRun->IsRightToLeft() && !firstFont->HasMirroringInfo()) {
                    MirrorSubstring(layout, mirroredStr, aString, aLength, runStart, runLength);
                }
            
                // add a glyph run for the entire substring
                AddGlyphRun(aRun, firstFont, aSegmentStart + runStart - headerChars);

                // do we have any more work to do?
                if (status == noErr)
                    break;
            }

            // then, handle any chars that were found in the fallback list
            if (status == kATSUFontsMatched) {
                // substitute font will be used in [changedOffset, changedOffset + changedLength)
                gfxAtsuiFont *font = FindFontFor(substituteFontID);

                //fprintf (stderr, "... glyphs matched in fallback: %d %d (%s)\n", changedOffset, changedLength, NS_ConvertUTF16toUTF8(font->GetUniqueName()).get());

                if (font) {
                    // create a new style for the substitute font
                    ATSUStyle subStyle;
                    ATSUCreateStyle (&subStyle);
                    ATSUCopyAttributes (mainStyle, subStyle);

                    ATSUAttributeTag fontTags[] = { kATSUFontTag };
                    ByteCount fontArgSizes[] = { sizeof(ATSUFontID) };
                    ATSUAttributeValuePtr fontArgs[] = { &substituteFontID };

                    ATSUSetAttributes (subStyle, 1, fontTags, fontArgSizes, fontArgs);

                    // apply the new style to the layout for the changed substring
                    ATSUSetRunStyle (layout, subStyle, changedOffset, changedLength);

                    stylesToDispose.AppendElement(subStyle);

                    // add a glyph run for [changedOffset, changedOffset + changedLength) with the
                    // substituted font

                    // in the RTL case, handle fallback mirroring
                    if (aRun->IsRightToLeft() && !font->HasMirroringInfo()) {
                        MirrorSubstring(layout, mirroredStr, aString, aLength, changedOffset, 
                                        changedLength);
                    }
                
                    AddGlyphRun(aRun, font, aSegmentStart + changedOffset - headerChars);
                } else {
                    // We could hit this case if we decided to ignore the
                    // font when enumerating at startup; pretend that these are
                    // missing glyphs.
                    // XXX - wait, why did it even end up in the fallback list,
                    // then?

                    status = kATSUFontsNotMatched;
                }
            }

            if (status == kATSUFontsNotMatched) {
                //fprintf (stderr, "... glyphs not found: %d %d\n", changedOffset, changedLength);

                // If this is our first time through (no fallback),
                // or if we're missing a different range than before,
                // add it to the list of ranges to examine
                if (firstTime ||
                    (changedOffset != missingStart && changedLength != missingLength))
                {
                    //fprintf (stderr, " (will look again at %d %d)\n", changedOffset, changedLength);
                    missingOffsetsAndLengths.AppendElement(changedOffset);
                    missingOffsetsAndLengths.AppendElement(changedLength);
                } else {
                    AddGlyphRun(aRun, firstFont, aSegmentStart + changedOffset - headerChars);

                    if (!closure.mUnmatchedChars) {
                        closure.mUnmatchedChars = new PRPackedBool[aLength];
                        if (closure.mUnmatchedChars) {
                            memset(closure.mUnmatchedChars.get(), PR_FALSE, aLength);
                        }
                    }

                    if (closure.mUnmatchedChars) {
                        memset(closure.mUnmatchedChars.get() + changedOffset - headerChars,
                               PR_TRUE, changedLength);
                    }
                }
            }

            //fprintf (stderr, "total length: %d changedOffset: %d changedLength: %d\n",  runLength, changedOffset, changedLength);

            runStart = changedOffset + changedLength;
            runLength = totalLength - runStart;
        }

        firstTime = PR_FALSE;
    } while (missingOffsetsAndLengths.Length() > 0);

    // put back the fallback object, because we'll be disposing
    // of any of the ones we created while doing fallback
    if (fallbacksToDispose.Length() > 0) {
        static ATSUAttributeTag fallbackTags[] = { kATSULineFontFallbacksTag };
        static ByteCount fallbackArgSizes[] = { sizeof(ATSUFontFallbacks) };
        ATSUAttributeValuePtr fallbackArgs[] = { GetATSUFontFallbacksPtr() };

        ATSUSetLayoutControls(layout,
                              NS_ARRAY_LENGTH(fallbackTags),
                              fallbackTags, fallbackArgSizes, fallbackArgs);
    }

    for (PRUint32 i = 0; i < fallbacksToDispose.Length(); i++)
        ATSUDisposeFontFallbacks(fallbacksToDispose[i]);

    // sort our glyph runs; we may have added
    // some out of order while doing fallback font matching
    aRun->SortGlyphRuns();

    //fprintf (stderr, "==== End font matching\n");

    // Trigger layout so that our callback fires. We don't actually care about
    // the result of this call.
    ATSTrapezoid trap;
    ItemCount trapCount;
    ATSUGetGlyphBounds(layout, 0, 0, headerChars, aSegmentLength,
                       kATSUseFractionalOrigins, 1, &trap, &trapCount); 

    ATSUDisposeTextLayout(layout);

    PRUint32 i;
    for (i = 0; i < stylesToDispose.Length(); ++i) {
        ATSUDisposeStyle(stylesToDispose[i]);
    }
    gCallbackClosure = nsnull;
    return !closure.mOverrunningGlyphs;
}

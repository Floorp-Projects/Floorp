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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006-2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   John Daggett <jdaggett@mozilla.com>
 *   Jonathan Kew <jfkthame@gmail.com>
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
#include "gfxPlatformMac.h"
#include "gfxCoreTextFonts.h"

#include "gfxFontTest.h"
#include "gfxFontUtils.h"

#include "cairo-quartz.h"

#include "gfxQuartzSurface.h"
#include "gfxMacPlatformFontList.h"
#include "gfxUserFontSet.h"

#include "nsUnicodeRange.h"

// Uncomment this to dump all text runs created to the log (if enabled)
//#define DUMP_TEXT_RUNS

#ifdef DUMP_TEXT_RUNS
static PRLogModuleInfo *gCoreTextTextRunLog = PR_NewLogModule("coreTextTextRun");
#endif

#define ROUND(x) (floor((x) + 0.5))


// standard font descriptors that we construct the first time they're needed
CTFontDescriptorRef gfxCoreTextFont::sDefaultFeaturesDescriptor = NULL;
CTFontDescriptorRef gfxCoreTextFont::sDisableLigaturesDescriptor = NULL;

#ifdef DEBUG_jonathan
static void dumpFontDescCallback(const void *key, const void *value, void *context)
{
    CFStringRef attribute = (CFStringRef)key;
    CFTypeRef setting = (CFTypeRef)value;
    fprintf(stderr, "attr: "); CFShow(attribute);
    fprintf(stderr, "    = "); CFShow(setting);
    fprintf(stderr, "\n");
}

static void
dumpFontDescriptor(CTFontRef font)
{
    CTFontDescriptorRef desc = CTFontCopyFontDescriptor(font);
    CFDictionaryRef dict = CTFontDescriptorCopyAttributes(desc);
    CFRelease(desc);
    CFDictionaryApplyFunction(dict, &dumpFontDescCallback, 0);
    CFRelease(dict);
}
#endif

gfxCoreTextFont::gfxCoreTextFont(MacOSFontEntry *aFontEntry,
                                 const gfxFontStyle *aFontStyle,
                                 PRBool aNeedsBold)
    : gfxFont(aFontEntry, aFontStyle),
      mFontStyle(aFontStyle),
      mCTFont(nsnull),
      mAttributesDict(nsnull),
      mHasMetrics(PR_FALSE),
      mFontFace(nsnull),
      mScaledFont(nsnull),
      mAdjustedSize(0.0f)
{
    mATSFont = aFontEntry->GetFontRef();

    // determine whether synthetic bolding is needed
    PRInt8 baseWeight, weightDistance;
    mFontStyle->ComputeWeightAndOffset(&baseWeight, &weightDistance);
    PRUint16 targetWeight = (baseWeight * 100) + (weightDistance * 100);

    // synthetic bolding occurs when font itself is not a bold-face and either the absolute weight
    // is at least 600 or the relative weight (e.g. 402) implies a darker face than the ones available.
    // note: this means that (1) lighter styles *never* synthetic bold and (2) synthetic bolding always occurs
    // at the first bolder step beyond available faces, no matter how light the boldest face
    if (!aFontEntry->IsBold()
        && ((weightDistance == 0 && targetWeight >= 600) || (weightDistance > 0 && aNeedsBold)))
    {
        mSyntheticBoldOffset = 1;  // devunit offset when double-striking text to fake boldness
    }

    // InitMetrics will create the mCTFont (possibly taking account of sizeAdjust)
    InitMetrics();
    if (!mIsValid) {
        return;
    }

    // Set up the default attribute dictionary that we will need each time we create a CFAttributedString
    mAttributesDict =
        ::CFDictionaryCreate(kCFAllocatorDefault,
                             (const void**) &kCTFontAttributeName,
                             (const void**) &mCTFont,
                             1, // count of attributes
                             &kCFTypeDictionaryKeyCallBacks,
                             &kCFTypeDictionaryValueCallBacks);

    // Remaining initialization is largely based on CommonInit() in the gfxAtsuiFont code
    CGFontRef cgFont = ::CGFontCreateWithPlatformFont(&mATSFont);
    mFontFace = cairo_quartz_font_face_create_for_cgfont(cgFont);
    ::CGFontRelease(cgFont);

    cairo_status_t cairoerr = cairo_font_face_status(mFontFace);
    if (cairoerr != CAIRO_STATUS_SUCCESS) {
        mIsValid = PR_FALSE;
#ifdef DEBUG
        char warnBuf[1024];
        sprintf(warnBuf, "Failed to create Cairo font face: %s status: %d",
                NS_ConvertUTF16toUTF8(GetName()).get(), cairoerr);
        NS_WARNING(warnBuf);
#endif
        return;
    }

    cairo_matrix_t sizeMatrix, ctm;
    cairo_matrix_init_identity(&ctm);
    cairo_matrix_init_scale(&sizeMatrix, mAdjustedSize, mAdjustedSize);

    // synthetic oblique by skewing via the font matrix
    PRBool needsOblique =
        (mFontEntry != NULL) &&
        (!mFontEntry->IsItalic() && (mFontStyle->style & (FONT_STYLE_ITALIC | FONT_STYLE_OBLIQUE)));

    if (needsOblique) {
        double skewfactor = (needsOblique ? Fix2X(kATSItalicQDSkew) : 0);

        cairo_matrix_t style;
        cairo_matrix_init(&style,
                          1,                //xx
                          0,                //yx
                          -1 * skewfactor,   //xy
                          1,                //yy
                          0,                //x0
                          0);               //y0
        cairo_matrix_multiply(&sizeMatrix, &sizeMatrix, &style);
    }

    cairo_font_options_t *fontOptions = cairo_font_options_create();
    // if this fails (out of memory), the pointer is still safe to use
    // although we're almost certainly going to fail below anyway

    // turn off font anti-aliasing based on user pref setting
    if (mAdjustedSize <= (float) gfxPlatformMac::GetPlatform()->GetAntiAliasingThreshold()) {
        cairo_font_options_set_antialias(fontOptions, CAIRO_ANTIALIAS_NONE);
        //printf("font: %s, size: %f, disabling anti-aliasing\n", NS_ConvertUTF16toUTF8(GetName()).get(), mAdjustedSize);
    }

    mScaledFont = cairo_scaled_font_create(mFontFace, &sizeMatrix, &ctm, fontOptions);
    cairo_font_options_destroy(fontOptions);

    cairoerr = cairo_scaled_font_status(mScaledFont);
    if (cairoerr != CAIRO_STATUS_SUCCESS) {
        mIsValid = PR_FALSE;
#ifdef DEBUG
        char warnBuf[1024];
        sprintf(warnBuf, "Failed to create scaled font: %s status: %d",
                NS_ConvertUTF16toUTF8(GetName()).get(), cairoerr);
        NS_WARNING(warnBuf);
#endif
    }
}

static double
RoundToNearestMultiple(double aValue, double aFraction)
{
  return floor(aValue/aFraction + 0.5)*aFraction;
}

PRBool
gfxCoreTextFont::SetupCairoFont(gfxContext *aContext)
{
    cairo_scaled_font_t *scaledFont = CairoScaledFont();
    if (cairo_scaled_font_status(scaledFont) != CAIRO_STATUS_SUCCESS) {
        // Don't cairo_set_scaled_font as that would propagate the error to
        // the cairo_t, precluding any further drawing.
        return PR_FALSE;
    }
    cairo_set_scaled_font(aContext->GetCairo(), scaledFont);
    return PR_TRUE;
}

float
gfxCoreTextFont::GetCharWidth(PRUnichar aUniChar, PRUint32 *aGlyphID)
{
    UniChar c = aUniChar;
    CGGlyph glyph;
    if (::CTFontGetGlyphsForCharacters(mCTFont, &c, &glyph, 1)) {
        CGSize advance;
        ::CTFontGetAdvancesForGlyphs(mCTFont,
                                     kCTFontHorizontalOrientation,
                                     &glyph,
                                     &advance,
                                     1);
        if (aGlyphID != nsnull)
            *aGlyphID = glyph;
        return advance.width;
    }

    // couldn't get glyph for the char
    if (aGlyphID != nsnull)
        *aGlyphID = 0;
    return 0;
}

float
gfxCoreTextFont::GetCharHeight(PRUnichar aUniChar)
{
    UniChar c = aUniChar;
    CGGlyph glyph;
    if (::CTFontGetGlyphsForCharacters(mCTFont, &c, &glyph, 1)) {
        CGRect boundingRect;
        ::CTFontGetBoundingRectsForGlyphs(mCTFont,
                                          kCTFontHorizontalOrientation,
                                          &glyph,
                                          &boundingRect,
                                          1);
        return boundingRect.size.height;
    }

    // couldn't get glyph for the char
    return 0;
}

gfxCoreTextFont::~gfxCoreTextFont()
{
    if (mScaledFont)
        cairo_scaled_font_destroy(mScaledFont);
    if (mFontFace)
        cairo_font_face_destroy(mFontFace);

    if (mAttributesDict)
        ::CFRelease(mAttributesDict);
    if (mCTFont)
        ::CFRelease(mCTFont);
}

MacOSFontEntry*
gfxCoreTextFont::GetFontEntry()
{
    return static_cast<MacOSFontEntry*>(mFontEntry.get());
}

PRBool
gfxCoreTextFont::TestCharacterMap(PRUint32 aCh)
{
    return mIsValid && GetFontEntry()->TestCharacterMap(aCh);
}

void
gfxCoreTextFont::InitMetrics()
{
    if (mHasMetrics)
        return;

    gfxFloat size =
        PR_MAX(((mAdjustedSize != 0.0f) ? mAdjustedSize : GetStyle()->size), 1.0f);

    if (mCTFont != NULL) {
        ::CFRelease(mCTFont);
        mCTFont = NULL;
    }

    ATSFontMetrics atsMetrics;
    OSStatus err;

    err = ::ATSFontGetHorizontalMetrics(mATSFont, kATSOptionFlagsDefault,
                                        &atsMetrics);
    if (err != noErr) {
        mIsValid = PR_FALSE;

#ifdef DEBUG
        char warnBuf[1024];
        sprintf(warnBuf, "Bad font metrics for: %s err: %8.8x",
                NS_ConvertUTF16toUTF8(GetName()).get(), PRUint32(err));
        NS_WARNING(warnBuf);
#endif
        return;
    }

    // prefer to get xHeight from ATS metrics (unhinted) rather than Core Text (hinted),
    // see bug 429605.
    if (atsMetrics.xHeight > 0) {
        mMetrics.xHeight = atsMetrics.xHeight * size;
    } else {
        mCTFont = ::CTFontCreateWithPlatformFont(mATSFont, size, NULL, GetDefaultFeaturesDescriptor());
        mMetrics.xHeight = ::CTFontGetXHeight(mCTFont);
    }

    if (mAdjustedSize == 0.0f) {
        if (mMetrics.xHeight != 0.0f && GetStyle()->sizeAdjust != 0.0f) {
            gfxFloat aspect = mMetrics.xHeight / size;
            mAdjustedSize = GetStyle()->GetAdjustedSize(aspect);

            // the recursive call to InitMetrics will re-create mCTFont, with adjusted size,
            // and then continue to set up the rest of the metrics fields
            InitMetrics();
            return;
        }
        mAdjustedSize = size;
    }

    // create the CTFontRef if we didn't already do so above
    if (mCTFont == NULL)
        mCTFont = ::CTFontCreateWithPlatformFont(mATSFont, size, NULL, GetDefaultFeaturesDescriptor());

    mMetrics.superscriptOffset = mMetrics.xHeight;
    mMetrics.subscriptOffset = mMetrics.xHeight;
    mMetrics.underlineSize = ::CTFontGetUnderlineThickness(mCTFont);
    mMetrics.underlineOffset = ::CTFontGetUnderlinePosition(mCTFont);
    mMetrics.strikeoutSize = mMetrics.underlineSize;
    mMetrics.strikeoutOffset = mMetrics.xHeight / 2;

    mMetrics.externalLeading = ::CTFontGetLeading(mCTFont);
    mMetrics.emHeight = size;
//    mMetrics.maxAscent = CTFontGetAscent(mCTFont);
//    mMetrics.maxDescent = CTFontGetDescent(mCTFont);
    // using the ATS metrics rather than CT gives us results more consistent with the ATSUI path
    mMetrics.maxAscent =
      NS_ceil(RoundToNearestMultiple(atsMetrics.ascent * size, 1/1024.0));
    mMetrics.maxDescent =
      NS_ceil(-RoundToNearestMultiple(atsMetrics.descent * size, 1/1024.0));

    mMetrics.maxHeight = mMetrics.maxAscent + mMetrics.maxDescent;
    if (mMetrics.maxHeight - mMetrics.emHeight > 0.0)
        mMetrics.internalLeading = mMetrics.maxHeight - mMetrics.emHeight;
    else
        mMetrics.internalLeading = 0.0;

    mMetrics.maxAdvance = atsMetrics.maxAdvanceWidth * size + mSyntheticBoldOffset;

    mMetrics.emAscent = mMetrics.maxAscent * mMetrics.emHeight / mMetrics.maxHeight;
    mMetrics.emDescent = mMetrics.emHeight - mMetrics.emAscent;

    PRUint32 glyphID;
    float xWidth = GetCharWidth('x', &glyphID);
    if (atsMetrics.avgAdvanceWidth != 0.0)
        mMetrics.aveCharWidth = PR_MIN(atsMetrics.avgAdvanceWidth * size, xWidth);
    else if (glyphID != 0)
        mMetrics.aveCharWidth = xWidth;
    else
        mMetrics.aveCharWidth = mMetrics.maxAdvance;
    mMetrics.aveCharWidth += mSyntheticBoldOffset;

    if (GetFontEntry()->IsFixedPitch()) {
        // Some Quartz fonts are fixed pitch, but there's some glyph with a bigger
        // advance than the average character width... this forces
        // those fonts to be recognized like fixed pitch fonts by layout.
        mMetrics.maxAdvance = mMetrics.aveCharWidth;
    }

    mMetrics.spaceWidth = GetCharWidth(' ', &glyphID);
    mSpaceGlyph = glyphID;

    mMetrics.zeroOrAveCharWidth = GetCharWidth('0', &glyphID);
    if (glyphID == 0)
        mMetrics.zeroOrAveCharWidth = mMetrics.aveCharWidth;

    mHasMetrics = PR_TRUE;

    SanitizeMetrics(&mMetrics, GetFontEntry()->mIsBadUnderlineFont);

#if 0
    fprintf (stderr, "Font: %p (%s) size: %f\n", this,
             NS_ConvertUTF16toUTF8(GetName()).get(), mStyle.size);
//    fprintf (stderr, "    fbounds.origin.x %f y %f size.width %f height %f\n", fbounds.origin.x, fbounds.origin.y, fbounds.size.width, fbounds.size.height);
    fprintf (stderr, "    emHeight: %f emAscent: %f emDescent: %f\n", mMetrics.emHeight, mMetrics.emAscent, mMetrics.emDescent);
    fprintf (stderr, "    maxAscent: %f maxDescent: %f maxAdvance: %f\n", mMetrics.maxAscent, mMetrics.maxDescent, mMetrics.maxAdvance);
    fprintf (stderr, "    internalLeading: %f externalLeading: %f\n", mMetrics.internalLeading, mMetrics.externalLeading);
    fprintf (stderr, "    spaceWidth: %f aveCharWidth: %f xHeight: %f\n", mMetrics.spaceWidth, mMetrics.aveCharWidth, mMetrics.xHeight);
    fprintf (stderr, "    uOff: %f uSize: %f stOff: %f stSize: %f suOff: %f suSize: %f\n", mMetrics.underlineOffset, mMetrics.underlineSize,
                              mMetrics.strikeoutOffset, mMetrics.strikeoutSize, mMetrics.superscriptOffset, mMetrics.subscriptOffset);
#endif
}

void
gfxCoreTextFont::InitTextRun(gfxTextRun *aTextRun,
                             const PRUnichar *aString,
                             PRUint32 aRunStart,
                             PRUint32 aRunLength)
{
    // aRunStart and aRunLength define the section of the textRun and of aString
    // that is to be drawn with this particular font

    PRBool disableLigatures = (aTextRun->GetFlags() & gfxTextRunFactory::TEXT_DISABLE_OPTIONAL_LIGATURES) != 0;

    // Create a CFAttributedString with text and style info, so we can use CoreText to lay it out.

    PRBool isRTL = aTextRun->IsRightToLeft();

    // we need to bidi-wrap the text if the run is RTL,
    // or if it is an LTR run but may contain (overridden) RTL chars
    PRBool bidiWrap = isRTL;
    if (!bidiWrap && (aTextRun->GetFlags() & gfxTextRunFactory::TEXT_IS_8BIT) == 0) {
        PRUint32 i;
        for (i = aRunStart; i < aRunStart + aRunLength; ++i) {
            if (gfxFontUtils::PotentialRTLChar(aString[i])) {
                bidiWrap = PR_TRUE;
                break;
            }
        }
    }

    // If there's a possibility of any bidi, we wrap the text with direction overrides
    // to ensure neutrals or characters that were bidi-overridden in HTML behave properly.
    const UniChar beginLTR[]    = { 0x202d, 0x20 };
    const UniChar beginRTL[]    = { 0x202e, 0x20 };
    const UniChar endBidiWrap[] = { 0x20, 0x202c };

    PRUint32 startOffset;
    CFStringRef stringObj;
    if (bidiWrap) {
        startOffset = isRTL ?
            sizeof(beginRTL) / sizeof(beginRTL[0]) : sizeof(beginLTR) / sizeof(beginLTR[0]);
        CFMutableStringRef mutableString =
            ::CFStringCreateMutable(kCFAllocatorDefault,
                                    aRunLength + startOffset +
                                    sizeof(endBidiWrap) / sizeof(endBidiWrap[0]));
        ::CFStringAppendCharacters(mutableString,
                                   isRTL ? beginRTL : beginLTR,
                                   startOffset);
        ::CFStringAppendCharacters(mutableString,
                                   aString + aRunStart, aRunLength);
        ::CFStringAppendCharacters(mutableString,
                                   endBidiWrap,
                                   sizeof(endBidiWrap) / sizeof(endBidiWrap[0]));
        stringObj = mutableString;
    } else {
        startOffset = 0;
        stringObj = ::CFStringCreateWithCharactersNoCopy(kCFAllocatorDefault,
                                                         aString + aRunStart,
                                                         aRunLength,
                                                         kCFAllocatorNull);
    }

    CFDictionaryRef attrObj;
    if (disableLigatures) {
        // For letterspacing (or maybe other situations) we need to make a copy of the CTFont
        // with the ligature feature disabled
        CTFontRef ctFont =
            gfxCoreTextFont::CreateCTFontWithDisabledLigatures(GetATSFont(),
                                                               ::CTFontGetSize(GetCTFont()));

        attrObj =
            ::CFDictionaryCreate(kCFAllocatorDefault,
                                 (const void**) &kCTFontAttributeName,
                                 (const void**) &ctFont,
                                 1, // count of attributes
                                 &kCFTypeDictionaryKeyCallBacks,
                                 &kCFTypeDictionaryValueCallBacks);
        // Having created the dict, we're finished with our ligature-disabled CTFontRef
        ::CFRelease(ctFont);
    } else {
        attrObj = GetAttributesDictionary();
        ::CFRetain(attrObj);
    }

    // Now we can create an attributed string
    CFAttributedStringRef attrStringObj =
        ::CFAttributedStringCreate(kCFAllocatorDefault, stringObj, attrObj);
    ::CFRelease(stringObj);
    ::CFRelease(attrObj);

    // Create the CoreText line from our string, then we're done with it
    CTLineRef line = ::CTLineCreateWithAttributedString(attrStringObj);
    ::CFRelease(attrStringObj);

    // and finally retrieve the glyph data and store into the gfxTextRun
    CFArrayRef glyphRuns = ::CTLineGetGlyphRuns(line);
    PRUint32 numRuns = ::CFArrayGetCount(glyphRuns);

    // Iterate through the glyph runs.
    // Note that this includes the bidi wrapper, so we have to be careful
    // not to include the extra glyphs from there
    for (PRUint32 runIndex = 0; runIndex < numRuns; runIndex++) {
        CTRunRef aCTRun = (CTRunRef)::CFArrayGetValueAtIndex(glyphRuns, runIndex);
        if (SetGlyphsFromRun(aTextRun, aCTRun, startOffset, aRunStart, aRunLength) != NS_OK)
            break;
    }

    ::CFRelease(line);
}

#define SMALL_GLYPH_RUN 128 // preallocated size of our auto arrays for per-glyph data;
                            // some testing indicates that 90%+ of glyph runs will fit
                            // without requiring a separate allocation

nsresult
gfxCoreTextFont::SetGlyphsFromRun(gfxTextRun *aTextRun,
                                  CTRunRef aCTRun,
                                  PRInt32 aStringOffset, // offset in the string used to build the CTLine
                                  PRInt32 aRunStart,     // starting offset of this font run in the gfxTextRun
                                  PRInt32 aRunLength)    // length of this font run in characters
{
    // The textRun has been bidi-wrapped; aStringOffset is the number
    // of chars at the beginning of the CTLine that we should skip.
    // aRunStart and aRunLength define the range of characters
    // within the textRun that are "real" data we need to handle.
    // aCTRun is a glyph run from the CoreText layout process.

    PRBool isLTR = !aTextRun->IsRightToLeft();
    PRInt32 direction = isLTR ? 1 : -1;

    PRInt32 numGlyphs = ::CTRunGetGlyphCount(aCTRun);
    if (numGlyphs == 0)
        return NS_OK;

    // character offsets get really confusing here, as we have to keep track of
    // (a) the text in the actual textRun we're constructing
    // (b) the "font run" being rendered with the current font, defined by aRunStart and aRunLength
    //     parameters to InitTextRun
    // (c) the string that was handed to CoreText, which contains the text of the font run
    //     plus directional-override padding
    // (d) the CTRun currently being processed, which may be a sub-run of the CoreText line
    //     (but may extend beyond the actual font run into the bidi wrapping text).
    //     aStringOffset tells us how many initial characters of the line to ignore.

    // get the source string range within the CTLine's text
    CFRange stringRange = ::CTRunGetStringRange(aCTRun);
    // skip the run if it is entirely outside the actual range of the font run
    if (stringRange.location - aStringOffset + stringRange.length <= 0 ||
        stringRange.location - aStringOffset >= aRunLength)
        return NS_OK;

    // retrieve the laid-out glyph data from the CTRun
    nsAutoArrayPtr<CGGlyph> glyphsArray;
    nsAutoArrayPtr<CGPoint> positionsArray;
    nsAutoArrayPtr<CFIndex> glyphToCharArray;
    const CGGlyph* glyphs = NULL;
    const CGPoint* positions = NULL;
    const CFIndex* glyphToChar = NULL;

    // Testing indicates that CTRunGetGlyphsPtr (almost?) always succeeds,
    // and so allocating a new array and copying data with CTRunGetGlyphs
    // will be extremely rare.
    // If this were not the case, we could use an nsAutoTArray<> to
    // try and avoid the heap allocation for small runs.
    // It's possible that some future change to CoreText will mean that
    // CTRunGetGlyphsPtr fails more often; if this happens, nsAutoTArray<>
    // may become an attractive option.
    glyphs = ::CTRunGetGlyphsPtr(aCTRun);
    if (!glyphs) {
        glyphsArray = new (std::nothrow) CGGlyph[numGlyphs];
        if (!glyphsArray)
            return NS_ERROR_OUT_OF_MEMORY;
        ::CTRunGetGlyphs(aCTRun, ::CFRangeMake(0, 0), glyphsArray.get());
        glyphs = glyphsArray.get();
    }

    positions = ::CTRunGetPositionsPtr(aCTRun);
    if (!positions) {
        positionsArray = new (std::nothrow) CGPoint[numGlyphs];
        if (!positionsArray)
            return NS_ERROR_OUT_OF_MEMORY;
        ::CTRunGetPositions(aCTRun, ::CFRangeMake(0, 0), positionsArray.get());
        positions = positionsArray.get();
    }

    // Remember that the glyphToChar indices relate to the CoreText line
    // not to the beginning of the textRun, the font run, or the stringRange of the glyph run
    glyphToChar = ::CTRunGetStringIndicesPtr(aCTRun);
    if (!glyphToChar) {
        glyphToCharArray = new (std::nothrow) CFIndex[numGlyphs];
        if (!glyphToCharArray)
            return NS_ERROR_OUT_OF_MEMORY;
        ::CTRunGetStringIndices(aCTRun, ::CFRangeMake(0, 0), glyphToCharArray.get());
        glyphToChar = glyphToCharArray.get();
    }

    double runWidth = ::CTRunGetTypographicBounds(aCTRun, ::CFRangeMake(0, 0), NULL, NULL, NULL);

    nsAutoTArray<gfxTextRun::DetailedGlyph,1> detailedGlyphs;
    gfxTextRun::CompressedGlyph g;
    const PRUint32 appUnitsPerDevUnit = aTextRun->GetAppUnitsPerDevUnit();

    // CoreText gives us the glyphindex-to-charindex mapping, which relates each glyph
    // to a source text character; we also need the charindex-to-glyphindex mapping to
    // find the glyph for a given char. Note that some chars may not map to any glyph
    // (ligature continuations), and some may map to several glyphs (eg Indic split vowels).
    // We set the glyph index to NO_GLYPH for chars that have no associated glyph, and we
    // record the last glyph index for cases where the char maps to several glyphs,
    // so that our clumping will include all the glyph fragments for the character.

    // The charToGlyph array is indexed by char position within the stringRange of the glyph run.

    static const PRInt32 NO_GLYPH = -1;
    nsAutoTArray<PRInt32,SMALL_GLYPH_RUN> charToGlyphArray;
    if (!charToGlyphArray.SetLength(stringRange.length))
        return NS_ERROR_OUT_OF_MEMORY;
    PRInt32 *charToGlyph = charToGlyphArray.Elements();
    for (PRInt32 offset = 0; offset < stringRange.length; ++offset)
        charToGlyph[offset] = NO_GLYPH;
    for (PRInt32 g = 0; g < numGlyphs; ++g) {
        PRInt32 loc = glyphToChar[g] - stringRange.location;
        if (loc >= 0 && loc < stringRange.length) {
            charToGlyph[loc] = g;
        }
    }

    // Find character and glyph clumps that correspond, allowing for ligatures,
    // indic reordering, split glyphs, etc.
    //
    // The idea is that we'll find a character sequence starting at the first char of stringRange,
    // and extend it until it includes the character associated with the first glyph;
    // we also extend it as long as there are "holes" in the range of glyphs. So we
    // will eventually have a contiguous sequence of characters, starting at the beginning
    // of the range, that map to a contiguous sequence of glyphs, starting at the beginning
    // of the glyph array. That's a clump; then we update the starting positions and repeat.
    //
    // NB: In the case of RTL layouts, we iterate over the stringRange in reverse.
    //
    // This may find characters that fall outside the range aRunStart:aRunLength,
    // so we won't necessarily use everything we find here.

    PRInt32 glyphStart = 0; // looking for a clump that starts at this glyph index
    PRInt32 charStart = isLTR ?
        0 : stringRange.length-1; // and this char index (in the stringRange of the glyph run)

    while (glyphStart < numGlyphs) { // keep finding groups until all glyphs are accounted for

        PRBool inOrder = PR_TRUE;
        PRInt32 charEnd = glyphToChar[glyphStart] - stringRange.location;
        PRInt32 glyphEnd = glyphStart;
        PRInt32 charLimit = isLTR ? stringRange.length : -1;
        do {
            // This is normally executed once for each iteration of the outer loop,
            // but in unusual cases where the character/glyph association is complex,
            // the initial character range might correspond to a non-contiguous
            // glyph range with "holes" in it. If so, we will repeat this loop to
            // extend the character range until we have a contiguous glyph sequence.
            charEnd += direction;
            while (charEnd != charLimit && charToGlyph[charEnd - stringRange.location] == NO_GLYPH) {
                charEnd += direction;
            }

            // find the maximum glyph index covered by the clump so far
            for (PRInt32 i = charStart; i != charEnd; i += direction) {
                if (charToGlyph[i] != NO_GLYPH) {
                    glyphEnd = PR_MAX(glyphEnd, charToGlyph[i] + 1); // update extent of glyph range
                }
            }

            if (glyphEnd == glyphStart + 1) {
                // for the common case of a single-glyph clump, we can skip the following checks
                break;
            }

            if (glyphEnd == glyphStart) {
                // no glyphs, try to extend the clump
                continue;
            }

            // check whether all glyphs in the range are associated with the characters
            // in our clump; if not, we have a discontinuous range, and should extend it
            // unless we've reached the end of the text
            PRBool allGlyphsAreWithinCluster = PR_TRUE;
            PRInt32 prevGlyphCharIndex = charStart;
            for (PRInt32 i = glyphStart; i < glyphEnd; ++i) {
                PRInt32 glyphCharIndex = glyphToChar[i] - stringRange.location;
                if (isLTR) {
                    if (glyphCharIndex < charStart || glyphCharIndex >= charEnd) {
                        allGlyphsAreWithinCluster = PR_FALSE;
                        break;
                    }
                    if (glyphCharIndex < prevGlyphCharIndex) {
                        inOrder = PR_FALSE;
                    }
                    prevGlyphCharIndex = glyphCharIndex;
                } else {
                    if (glyphCharIndex > charStart || glyphCharIndex <= charEnd) {
                        allGlyphsAreWithinCluster = PR_FALSE;
                        break;
                    }
                    if (glyphCharIndex > prevGlyphCharIndex) {
                        inOrder = PR_FALSE;
                    }
                    prevGlyphCharIndex = glyphCharIndex;
                }
            }
            if (allGlyphsAreWithinCluster) {
                break;
            }
        } while (charEnd != charLimit);

        NS_ASSERTION(glyphStart < glyphEnd, "character/glyph clump contains no glyphs!");
        NS_ASSERTION(charStart != charEnd, "character/glyph contains no characters!");

        // Now charStart..charEnd is a ligature clump, corresponding to glyphStart..glyphEnd;
        // Set baseCharIndex to the char we'll actually attach the glyphs to (1st of ligature),
        // and endCharIndex to the limit (position beyond the last char),
        // adjusting for the offset of the stringRange relative to the textRun.
        PRInt32 baseCharIndex, endCharIndex;
        if (isLTR) {
            while (charEnd < stringRange.length && charToGlyph[charEnd] == NO_GLYPH)
                charEnd++;
            baseCharIndex = charStart + stringRange.location - aStringOffset + aRunStart;
            endCharIndex = charEnd + stringRange.location - aStringOffset + aRunStart;
        } else {
            while (charEnd >= 0 && charToGlyph[charEnd] == NO_GLYPH)
                charEnd--;
            baseCharIndex = charEnd + stringRange.location - aStringOffset + aRunStart + 1;
            endCharIndex = charStart + stringRange.location - aStringOffset + aRunStart + 1;
        }

        // Then we check if the clump falls outside our actual string range; if so, just go to the next.
        if (endCharIndex <= aRunStart || baseCharIndex >= aRunStart + aRunLength) {
            glyphStart = glyphEnd;
            charStart = charEnd;
            continue;
        }
        // Ensure we won't try to go beyond the valid length of the textRun's text
        baseCharIndex = PR_MAX(baseCharIndex, aRunStart);
        endCharIndex = PR_MIN(endCharIndex, aRunStart + aRunLength);

        // Now we're ready to set the glyph info in the textRun; measure the glyph width
        // of the first (perhaps only) glyph, to see if it is "Simple"
        double toNextGlyph;
        if (glyphStart < numGlyphs-1)
            toNextGlyph = positions[glyphStart+1].x - positions[glyphStart].x;
        else
            toNextGlyph = positions[0].x + runWidth - positions[glyphStart].x;
        PRInt32 advance = PRInt32(toNextGlyph * appUnitsPerDevUnit);

        // Check if it's a simple one-to-one mapping
        PRInt32 glyphsInClump = glyphEnd - glyphStart;
        if (glyphsInClump == 1 &&
            gfxTextRun::CompressedGlyph::IsSimpleGlyphID(glyphs[glyphStart]) &&
            gfxTextRun::CompressedGlyph::IsSimpleAdvance(advance) &&
            aTextRun->IsClusterStart(baseCharIndex) &&
            positions[glyphStart].y == 0.0)
        {
            aTextRun->SetSimpleGlyph(baseCharIndex,
                                     g.SetSimpleGlyph(advance, glyphs[glyphStart]));
        } else {
            // collect all glyphs in a list to be assigned to the first char;
            // there must be at least one in the clump, and we already measured its advance,
            // hence the placement of the loop-exit test and the measurement of the next glyph
            while (1) {
                gfxTextRun::DetailedGlyph *details = detailedGlyphs.AppendElement();
                details->mGlyphID = glyphs[glyphStart];
                details->mXOffset = 0;
                details->mYOffset = -positions[glyphStart].y * appUnitsPerDevUnit;
                details->mAdvance = advance;
                if (++glyphStart >= glyphEnd)
                   break;
                if (glyphStart < numGlyphs-1)
                    toNextGlyph = positions[glyphStart+1].x - positions[glyphStart].x;
                else
                    toNextGlyph = positions[0].x + runWidth - positions[glyphStart].x;
                advance = PRInt32(toNextGlyph * appUnitsPerDevUnit);
            }

            gfxTextRun::CompressedGlyph g;
            g.SetComplex(aTextRun->IsClusterStart(baseCharIndex),
                         PR_TRUE, detailedGlyphs.Length());
            aTextRun->SetGlyphs(baseCharIndex, g, detailedGlyphs.Elements());

            detailedGlyphs.Clear();
        }

        // the rest of the chars in the group are ligature continuations, no associated glyphs
        while (++baseCharIndex != endCharIndex && baseCharIndex < aRunStart + aRunLength) {
            g.SetComplex(inOrder && aTextRun->IsClusterStart(baseCharIndex),
                         PR_FALSE, 0);
            aTextRun->SetGlyphs(baseCharIndex, g, nsnull);
        }

        glyphStart = glyphEnd;
        charStart = charEnd;
    }

    return NS_OK;
}

// Construct the font attribute descriptor that we'll apply by default when creating a CTFontRef.
// This will turn off line-edge swashes by default, because we don't know the actual line breaks
// when doing glyph shaping.
void
gfxCoreTextFont::CreateDefaultFeaturesDescriptor()
{
    if (sDefaultFeaturesDescriptor != NULL)
        return;

    SInt16 val = kSmartSwashType;
    CFNumberRef swashesType =
        ::CFNumberCreate(kCFAllocatorDefault,
                         kCFNumberSInt16Type,
                         &val);
    val = kLineInitialSwashesOffSelector;
    CFNumberRef lineInitialsOffSelector =
        ::CFNumberCreate(kCFAllocatorDefault,
                         kCFNumberSInt16Type,
                         &val);

    CFTypeRef keys[]   = { kCTFontFeatureTypeIdentifierKey,
                           kCTFontFeatureSelectorIdentifierKey };
    CFTypeRef values[] = { swashesType,
                           lineInitialsOffSelector };
    CFDictionaryRef featureSettings[2];
    featureSettings[0] =
        ::CFDictionaryCreate(kCFAllocatorDefault,
                             (const void **) keys,
                             (const void **) values,
                             NS_ARRAY_LENGTH(keys),
                             &kCFTypeDictionaryKeyCallBacks,
                             &kCFTypeDictionaryValueCallBacks);
    ::CFRelease(lineInitialsOffSelector);

    val = kLineFinalSwashesOffSelector;
    CFNumberRef lineFinalsOffSelector =
        ::CFNumberCreate(kCFAllocatorDefault,
                         kCFNumberSInt16Type,
                         &val);
    values[1] = lineFinalsOffSelector;
    featureSettings[1] =
        ::CFDictionaryCreate(kCFAllocatorDefault,
                             (const void **) keys,
                             (const void **) values,
                             NS_ARRAY_LENGTH(keys),
                             &kCFTypeDictionaryKeyCallBacks,
                             &kCFTypeDictionaryValueCallBacks);
    ::CFRelease(lineFinalsOffSelector);
    ::CFRelease(swashesType);

    CFArrayRef featuresArray =
        ::CFArrayCreate(kCFAllocatorDefault,
                        (const void **) featureSettings,
                        NS_ARRAY_LENGTH(featureSettings),
                        &kCFTypeArrayCallBacks);
    ::CFRelease(featureSettings[0]);
    ::CFRelease(featureSettings[1]);

    const CFTypeRef attrKeys[]   = { kCTFontFeatureSettingsAttribute };
    const CFTypeRef attrValues[] = { featuresArray };
    CFDictionaryRef attributesDict =
        ::CFDictionaryCreate(kCFAllocatorDefault,
                             (const void **) attrKeys,
                             (const void **) attrValues,
                             NS_ARRAY_LENGTH(attrKeys),
                             &kCFTypeDictionaryKeyCallBacks,
                             &kCFTypeDictionaryValueCallBacks);
    ::CFRelease(featuresArray);

    sDefaultFeaturesDescriptor =
        ::CTFontDescriptorCreateWithAttributes(attributesDict);
    ::CFRelease(attributesDict);
}

// Create a CTFontRef, with the Common Ligatures feature disabled [static]
CTFontRef
gfxCoreTextFont::CreateCTFontWithDisabledLigatures(ATSFontRef aFontRef, CGFloat aSize)
{
    if (sDisableLigaturesDescriptor == NULL) {
        // initialize cached descriptor to turn off the Common Ligatures feature
        SInt16 val = kLigaturesType;
        CFNumberRef ligaturesType =
            ::CFNumberCreate(kCFAllocatorDefault,
                             kCFNumberSInt16Type,
                             &val);
        val = kCommonLigaturesOffSelector;
        CFNumberRef commonLigaturesOffSelector =
            ::CFNumberCreate(kCFAllocatorDefault,
                             kCFNumberSInt16Type,
                             &val);

        const CFTypeRef keys[]   = { kCTFontFeatureTypeIdentifierKey,
                                     kCTFontFeatureSelectorIdentifierKey };
        const CFTypeRef values[] = { ligaturesType,
                                     commonLigaturesOffSelector };
        CFDictionaryRef featureSettingDict =
            ::CFDictionaryCreate(kCFAllocatorDefault,
                                 (const void **) keys,
                                 (const void **) values,
                                 NS_ARRAY_LENGTH(keys),
                                 &kCFTypeDictionaryKeyCallBacks,
                                 &kCFTypeDictionaryValueCallBacks);
        ::CFRelease(ligaturesType);
        ::CFRelease(commonLigaturesOffSelector);

        CFArrayRef featuresArray =
            ::CFArrayCreate(kCFAllocatorDefault,
                            (const void **) &featureSettingDict,
                            1,
                            &kCFTypeArrayCallBacks);
        ::CFRelease(featureSettingDict);

        CFDictionaryRef attributesDict =
            ::CFDictionaryCreate(kCFAllocatorDefault,
                                 (const void **) &kCTFontFeatureSettingsAttribute,
                                 (const void **) &featuresArray,
                                 1, // count of keys & values
                                 &kCFTypeDictionaryKeyCallBacks,
                                 &kCFTypeDictionaryValueCallBacks);
        ::CFRelease(featuresArray);

        sDisableLigaturesDescriptor =
            ::CTFontDescriptorCreateCopyWithAttributes(GetDefaultFeaturesDescriptor(), attributesDict);
        ::CFRelease(attributesDict);
    }
    
    return ::CTFontCreateWithPlatformFont(aFontRef, aSize, NULL, sDisableLigaturesDescriptor);
}

void
gfxCoreTextFont::Shutdown() // [static]
{
    if (sDisableLigaturesDescriptor != NULL) {
        ::CFRelease(sDisableLigaturesDescriptor);
        sDisableLigaturesDescriptor = NULL;
    }        
    if (sDefaultFeaturesDescriptor != NULL) {
        ::CFRelease(sDefaultFeaturesDescriptor);
        sDefaultFeaturesDescriptor = NULL;
    }
}

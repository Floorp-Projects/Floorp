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

#ifndef __LP64__ /* don't compile any of this on 64-bit as ATSUI is not available */

#include "prtypes.h"
#include "prmem.h"
#include "nsString.h"
#include "nsBidiUtils.h"

#include "gfxTypes.h"

#include "nsPromiseFlatString.h"

#include "gfxContext.h"
#include "gfxPlatform.h"
#include "gfxPlatformMac.h"
#include "gfxAtsuiFonts.h"

#include "gfxFontTest.h"
#include "gfxFontUtils.h"

#include "cairo-quartz.h"

#include "gfxQuartzSurface.h"
#include "gfxMacPlatformFontList.h"
#include "gfxUserFontSet.h"

#include "nsUnicodeRange.h"

// Uncomment this to dump all text runs created to stdout
// #define DUMP_TEXT_RUNS

#ifdef DUMP_TEXT_RUNS
static PRLogModuleInfo *gAtsuiTextRunLog = PR_NewLogModule("atsuiTextRun");
#endif

#define ROUND(x) (floor((x) + 0.5))

/* 10.5 SDK includes a funky new definition of FloatToFixed, so reset to old-style definition */
#ifdef FloatToFixed
#undef FloatToFixed
#define FloatToFixed(a)     ((Fixed)((float)(a) * fixed1))
#endif

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

gfxAtsuiFont::gfxAtsuiFont(MacOSFontEntry *aFontEntry,
                           const gfxFontStyle *fontStyle, PRBool aNeedsBold)
    : gfxFont(aFontEntry, fontStyle),
      mFontStyle(fontStyle), mATSUStyle(nsnull),
      mHasMirroring(PR_FALSE), mHasMirroringLookedUp(PR_FALSE),
      mFontFace(nsnull), mScaledFont(nsnull), mAdjustedSize(0.0f)
{
    ATSFontRef fontRef = aFontEntry->GetFontRef();
    ATSUFontID fontID = FMGetFontFromATSFontRef(fontRef);

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

    InitMetrics(fontID, fontRef);
    if (!mIsValid) {
        return;
    }

    mFontFace = cairo_quartz_font_face_create_for_atsu_font_id(fontID);

    cairo_matrix_t sizeMatrix, ctm;
    cairo_matrix_init_identity(&ctm);
    cairo_matrix_init_scale(&sizeMatrix, mAdjustedSize, mAdjustedSize);

    // synthetic oblique by skewing via the font matrix
    PRBool needsOblique = (!aFontEntry->IsItalic() && (mFontStyle->style & (FONT_STYLE_ITALIC | FONT_STYLE_OBLIQUE)));

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

    // turn off font anti-aliasing based on user pref setting
    if (mAdjustedSize <= (float) gfxPlatformMac::GetPlatform()->GetAntiAliasingThreshold()) {
        cairo_font_options_set_antialias(fontOptions, CAIRO_ANTIALIAS_NONE); 
        //printf("font: %s, size: %f, disabling anti-aliasing\n", NS_ConvertUTF16toUTF8(GetName()).get(), mAdjustedSize);
    }

    mScaledFont = cairo_scaled_font_create(mFontFace, &sizeMatrix, &ctm, fontOptions);
    cairo_font_options_destroy(fontOptions);

    cairo_status_t cairoerr = cairo_scaled_font_status(mScaledFont);
    if (cairoerr != CAIRO_STATUS_SUCCESS) {
        mIsValid = PR_FALSE;

#ifdef DEBUG        
        char warnBuf[1024];
        sprintf(warnBuf, "Failed to create scaled font: %s status: %d", NS_ConvertUTF16toUTF8(GetName()).get(), cairoerr);
        NS_WARNING(warnBuf);
#endif
    }
}


ATSFontRef gfxAtsuiFont::GetATSFontRef()
{
    return GetFontEntry()->GetFontRef();
}

static void
DisableUncommonLigaturesAndLineBoundarySwashes(ATSUStyle aStyle)
{
    static const ATSUFontFeatureType types[] = {
        kLigaturesType,
        kLigaturesType,
        kLigaturesType,
        kLigaturesType,
        kLigaturesType,
        kLigaturesType,
        kSmartSwashType,
        kSmartSwashType
    };
    static const ATSUFontFeatureType selectors[NS_ARRAY_LENGTH(types)] = {
        kRareLigaturesOffSelector,
        kLogosOffSelector,
        kRebusPicturesOffSelector,
        kDiphthongLigaturesOffSelector,
        kSquaredLigaturesOffSelector,
        kAbbrevSquaredLigaturesOffSelector,
        kLineInitialSwashesOffSelector,
        kLineFinalSwashesOffSelector
    };
    ATSUSetFontFeatures(aStyle, NS_ARRAY_LENGTH(types), types, selectors);
}

static void
DisableCommonLigatures(ATSUStyle aStyle)
{
    static const ATSUFontFeatureType types[] = {
        kLigaturesType
    };
    static const ATSUFontFeatureType selectors[NS_ARRAY_LENGTH(types)] = {
        kCommonLigaturesOffSelector
    };
    ATSUSetFontFeatures(aStyle, NS_ARRAY_LENGTH(types), types, selectors);
}

static double
RoundToNearestMultiple(double aValue, double aFraction)
{
  return floor(aValue/aFraction + 0.5)*aFraction;
}

void
gfxAtsuiFont::InitMetrics(ATSUFontID aFontID, ATSFontRef aFontRef)
{
    /* Create the ATSUStyle */

    gfxFloat size =
        PR_MAX(((mAdjustedSize != 0.0f) ? mAdjustedSize : GetStyle()->size), 1.0f);

    //fprintf (stderr, "string: '%s', size: %f\n", NS_ConvertUTF16toUTF8(aString).get(), size);

    if (mATSUStyle)
      ATSUDisposeStyle(mATSUStyle);

    ATSUFontID fid = aFontID;
    // fSize is in points (72dpi)
    Fixed fSize = FloatToFixed(size);
    // make the font render right-side up
    CGAffineTransform transform = CGAffineTransformMakeScale(1, -1);

    static const ATSUAttributeTag styleTags[] = {
        kATSUFontTag,
        kATSUSizeTag,
        kATSUFontMatrixTag
    };
    const ATSUAttributeValuePtr styleArgs[NS_ARRAY_LENGTH(styleTags)] = {
        &fid,
        &fSize,
        &transform
    };
    static const ByteCount styleArgSizes[NS_ARRAY_LENGTH(styleTags)] = {
        sizeof(ATSUFontID),
        sizeof(Fixed),
        sizeof(CGAffineTransform)
    };

    ATSUCreateStyle(&mATSUStyle);
    ATSUSetAttributes(mATSUStyle,
                      NS_ARRAY_LENGTH(styleTags),
                      styleTags,
                      styleArgSizes,
                      styleArgs);
    // Disable uncommon ligatures, but *don't* enable common ones;
    // the font may have default settings that disable common ligatures
    // and we want to respect that.
    // Also disable line boundary swashes because we can't handle them properly;
    // we don't know where the line-breaks are at the time we're applying shaping,
    // and it would be bad to put words with line-end swashes into the text-run
    // cache until we have a way to distinguish them from mid-line occurrences.
    DisableUncommonLigaturesAndLineBoundarySwashes(mATSUStyle);

    /* Now pull out the metrics */

    ATSFontMetrics atsMetrics;
    OSStatus err;
    
    err = ATSFontGetHorizontalMetrics(aFontRef, kATSOptionFlagsDefault,
                                &atsMetrics);
                                
    if (err != noErr) {
        mIsValid = PR_FALSE;
        
#ifdef DEBUG        
        char warnBuf[1024];
        sprintf(warnBuf, "Bad font metrics for: %s err: %8.8x", NS_ConvertUTF16toUTF8(GetName()).get(), PRUint32(err));
        NS_WARNING(warnBuf);
#endif
        return;
    }

    if (atsMetrics.xHeight)
        mMetrics.xHeight = atsMetrics.xHeight * size;
    else
        mMetrics.xHeight = GetCharHeight('x');

    if (mAdjustedSize == 0.0f) {
        if (mMetrics.xHeight != 0.0f && GetStyle()->sizeAdjust != 0.0f) {
            gfxFloat aspect = mMetrics.xHeight / size;
            mAdjustedSize = GetStyle()->GetAdjustedSize(aspect);
            InitMetrics(aFontID, aFontRef);
            return;
        }
        mAdjustedSize = size;
    }

    mMetrics.emHeight = size;

    mMetrics.maxAscent =
      NS_ceil(RoundToNearestMultiple(atsMetrics.ascent*size, 1/1024.0));
    mMetrics.maxDescent =
      NS_ceil(-RoundToNearestMultiple(atsMetrics.descent*size, 1/1024.0));

    mMetrics.maxHeight = mMetrics.maxAscent + mMetrics.maxDescent;

    if (mMetrics.maxHeight - mMetrics.emHeight > 0)
        mMetrics.internalLeading = mMetrics.maxHeight - mMetrics.emHeight; 
    else
        mMetrics.internalLeading = 0.0;
    mMetrics.externalLeading = atsMetrics.leading * size;

    mMetrics.emAscent = mMetrics.maxAscent * mMetrics.emHeight / mMetrics.maxHeight;
    mMetrics.emDescent = mMetrics.emHeight - mMetrics.emAscent;

    mMetrics.maxAdvance = atsMetrics.maxAdvanceWidth * size + mSyntheticBoldOffset;

    float xWidth = GetCharWidth('x');
    if (atsMetrics.avgAdvanceWidth != 0.0)
        mMetrics.aveCharWidth =
            PR_MIN(atsMetrics.avgAdvanceWidth * size, xWidth);
    else
        mMetrics.aveCharWidth = xWidth;

    mMetrics.aveCharWidth += mSyntheticBoldOffset;

    if (GetFontEntry()->IsFixedPitch()) {
        // Some Quartz fonts are fixed pitch, but there's some glyph with a bigger
        // advance than the average character width... this forces
        // those fonts to be recognized like fixed pitch fonts by layout.
        mMetrics.maxAdvance = mMetrics.aveCharWidth;
    }

    mMetrics.underlineOffset = atsMetrics.underlinePosition * size;
    mMetrics.underlineSize = atsMetrics.underlineThickness * size;

    mMetrics.subscriptOffset = mMetrics.xHeight;
    mMetrics.superscriptOffset = mMetrics.xHeight;

    mMetrics.strikeoutOffset = mMetrics.xHeight / 2.0;
    mMetrics.strikeoutSize = mMetrics.underlineSize;

    PRUint32 glyphID;
    mMetrics.spaceWidth = GetCharWidth(' ', &glyphID);
    mSpaceGlyph = glyphID;

    mMetrics.zeroOrAveCharWidth = GetCharWidth('0', &glyphID);
    if (glyphID == 0) // no zero in this font
        mMetrics.zeroOrAveCharWidth = mMetrics.aveCharWidth;

    SanitizeMetrics(&mMetrics, GetFontEntry()->mIsBadUnderlineFont);

#if 0
    fprintf (stderr, "Font: %p (%s) size: %f\n", this,
             NS_ConvertUTF16toUTF8(GetName()).get(), mStyle.size);
    fprintf (stderr, "    emHeight: %f emAscent: %f emDescent: %f\n", mMetrics.emHeight, mMetrics.emAscent, mMetrics.emDescent);
    fprintf (stderr, "    maxAscent: %f maxDescent: %f maxAdvance: %f\n", mMetrics.maxAscent, mMetrics.maxDescent, mMetrics.maxAdvance);
    fprintf (stderr, "    internalLeading: %f externalLeading: %f\n", mMetrics.internalLeading, mMetrics.externalLeading);
    fprintf (stderr, "    spaceWidth: %f aveCharWidth: %f xHeight: %f\n", mMetrics.spaceWidth, mMetrics.aveCharWidth, mMetrics.xHeight);
    fprintf (stderr, "    uOff: %f uSize: %f stOff: %f stSize: %f suOff: %f suSize: %f\n", mMetrics.underlineOffset, mMetrics.underlineSize, mMetrics.strikeoutOffset, mMetrics.strikeoutSize, mMetrics.superscriptOffset, mMetrics.subscriptOffset);
#endif
}

PRBool
gfxAtsuiFont::SetupCairoFont(gfxContext *aContext)
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

nsString
gfxAtsuiFont::GetUniqueName()
{
    return GetName();
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
    if (mScaledFont)
        cairo_scaled_font_destroy(mScaledFont);
    if (mFontFace)
        cairo_font_face_destroy(mFontFace);

    if (mATSUStyle)
        ATSUDisposeStyle(mATSUStyle);
}

const gfxFont::Metrics&
gfxAtsuiFont::GetMetrics()
{
    return mMetrics;
}

void
gfxAtsuiFont::SetupGlyphExtents(gfxContext *aContext, PRUint32 aGlyphID,
        PRBool aNeedTight, gfxGlyphExtents *aExtents)
{
    ATSGlyphScreenMetrics metrics;
    GlyphID glyph = aGlyphID;
    OSStatus err = ATSUGlyphGetScreenMetrics(mATSUStyle, 1, &glyph, 0, false, false,
                                             &metrics);
    if (err != noErr)
        return;
    PRUint32 appUnitsPerDevUnit = aExtents->GetAppUnitsPerDevUnit();

    if (!aNeedTight && metrics.topLeft.x >= 0 &&
        -metrics.topLeft.y + metrics.height <= mMetrics.maxAscent &&
        metrics.topLeft.y <= mMetrics.maxDescent) {
        PRUint32 appUnitsWidth =
            PRUint32(NS_ceil((metrics.topLeft.x + metrics.width)*appUnitsPerDevUnit));
        if (appUnitsWidth < gfxGlyphExtents::INVALID_WIDTH) {
            aExtents->SetContainedGlyphWidthAppUnits(aGlyphID, PRUint16(appUnitsWidth));
            return;
        }
    }

    double d2a = appUnitsPerDevUnit;
    gfxRect bounds(metrics.topLeft.x*d2a, (metrics.topLeft.y - metrics.height)*d2a,
                   metrics.width*d2a, metrics.height*d2a);
    aExtents->SetTightGlyphExtents(aGlyphID, bounds);
}

PRBool 
gfxAtsuiFont::HasMirroringInfo()
{
    if (!mHasMirroringLookedUp) {
        OSStatus status;
        ByteCount size;
        
        // 361695 - if the font has a 'prop' table, assume that ATSUI will handle glyph mirroring
        status = ATSFontGetTable(GetATSFontRef(), TRUETYPE_TAG('p','r','o','p'), 0, 0, 0, &size);
        mHasMirroring = (status == noErr);
        mHasMirroringLookedUp = PR_TRUE;
    }

    return mHasMirroring;
}

PRBool gfxAtsuiFont::TestCharacterMap(PRUint32 aCh) {
    if (!mIsValid) return PR_FALSE;
    return GetFontEntry()->TestCharacterMap(aCh);
}

MacOSFontEntry*
gfxAtsuiFont::GetFontEntry()
{
    return static_cast<MacOSFontEntry*> (mFontEntry.get());
}

/**
 * Look up the font in the gfxFont cache. If we don't find it, create one.
 * In either case, add a ref and return it ---
 * except for OOM in which case we do nothing and return null.
 */
 
static already_AddRefed<gfxAtsuiFont>
GetOrMakeFont(MacOSFontEntry *aFontEntry, const gfxFontStyle *aStyle, PRBool aNeedsBold)
{
    // the font entry name is the psname, not the family name
    nsRefPtr<gfxFont> font = gfxFontCache::GetCache()->Lookup(aFontEntry->Name(), aStyle);
    if (!font) {
        gfxAtsuiFont *newFont = new gfxAtsuiFont(aFontEntry, aStyle, aNeedsBold);
        if (!newFont)
            return nsnull;
        if (!newFont->Valid()) {
            delete newFont;
            return nsnull;
        }
        font = newFont;
        gfxFontCache::GetCache()->AddNew(font);
    }
    gfxFont *f = nsnull;
    font.swap(f);
    return static_cast<gfxAtsuiFont *>(f);
}


gfxAtsuiFontGroup::gfxAtsuiFontGroup(const nsAString& families,
                                     const gfxFontStyle *aStyle,
                                     gfxUserFontSet *aUserFontSet)
    : gfxFontGroup(families, aStyle, aUserFontSet)
{
    mPageLang = gfxPlatform::GetFontPrefLangFor(mStyle.langGroup.get());

    InitFontList();
}

PRBool
gfxAtsuiFontGroup::FindATSFont(const nsAString& aName,
                               const nsACString& aGenericName,
                               void *aClosure)
{
    gfxAtsuiFontGroup *fontGroup = static_cast<gfxAtsuiFontGroup*>(aClosure);
    const gfxFontStyle *fontStyle = fontGroup->GetStyle();


    PRBool needsBold;
    MacOSFontEntry *fe = nsnull;
    
    // first, look up in the user font set
    gfxUserFontSet *fs = fontGroup->GetUserFontSet();
    gfxFontEntry *gfe;
    if (fs && (gfe = fs->FindFontEntry(aName, *fontStyle, needsBold))) {
        // assume for now platform font if not SVG
        fe = static_cast<MacOSFontEntry*> (gfe);
    }
    
    // nothing in the user font set ==> check system fonts
    if (!fe) {
        fe = static_cast<MacOSFontEntry*>
            (gfxMacPlatformFontList::PlatformFontList()->FindFontForFamily(aName, fontStyle, needsBold));
    }

    if (fe && !fontGroup->HasFont(fe->GetFontRef())) {
        nsRefPtr<gfxAtsuiFont> font = GetOrMakeFont(fe, fontStyle, needsBold);
        if (font) {
            fontGroup->mFonts.AppendElement(font);
        }
    }

    return PR_TRUE;
}

gfxFontGroup *
gfxAtsuiFontGroup::Copy(const gfxFontStyle *aStyle)
{
    return new gfxAtsuiFontGroup(mFamilies, aStyle, mUserFontSet);
}

#define UNICODE_LRO 0x202d
#define UNICODE_RLO 0x202e
#define UNICODE_PDF 0x202c

static void
AppendDirectionalIndicatorStart(PRUint32 aFlags, nsAString& aString)
{
    static const PRUnichar overrides[2] = { UNICODE_LRO, UNICODE_RLO };
    aString.Append(overrides[(aFlags & gfxTextRunFactory::TEXT_IS_RTL) != 0]);
}

// Returns the number of trailing characters that should be part of the
// layout but should be ignored
static PRUint32
AppendDirectionalIndicatorEnd(PRBool aNeedDirection, nsAString& aString)
{
    // Ensure that we compute the full advance width for the last character
    // in the string --- we don't want that character to form a kerning
    // pair (or a ligature) with the '.' we may append next,
    // so we append a space now.
    // Even if the character is the last character in the layout,
    // we want its width to be determined as if it had a space after it,
    // for consistency with the bidi path and during textrun caching etc.
    aString.Append(' ');
    if (!aNeedDirection)
        return 1;

    // Ensure that none of the whitespace in the run is considered "trailing"
    // by ATSUI's bidi algorithm
    aString.Append('.');
    aString.Append(UNICODE_PDF);
    return 2;
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
    
    PRUint32 realGuessMax = PR_MAX(1, chars);
    
    // bug 436663 - ATSUI crashes on 10.5.3 with certain character sequences 
    // at around 512 characters, so for safety sake max out at 500 characters
    // bug 480134 - Do this for all OSX versions now, because there may be
    // other related bugs on 10.4.
    realGuessMax = PR_MIN(500, realGuessMax);

    return realGuessMax;
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
    NS_ASSERTION(aLength > 0, "should use MakeEmptyTextRun for zero-length text");
    gfxTextRun *textRun = gfxTextRun::Create(aParams, aString, aLength, this, aFlags);
    if (!textRun)
        return nsnull;

    gfxPlatformMac::SetupClusterBoundaries(textRun, aString);

    PRUint32 maxLen;
    nsAutoString utf16;
    for (maxLen = GuessMaximumStringLength(); maxLen > 0; maxLen /= 2) {
        PRUint32 start = 0;
        while (start < aLength) {
            PRUint32 len = FindTextRunSegmentLength(textRun, start, maxLen);

            utf16.Truncate();
            AppendDirectionalIndicatorStart(aFlags, utf16);
            PRUint32 layoutStart = utf16.Length();
            utf16.Append(aString + start, len);
            // Ensure that none of the whitespace in the run is considered "trailing"
            // by ATSUI's bidi algorithm
            PRUint32 trailingCharsToIgnore =
                AppendDirectionalIndicatorEnd(PR_TRUE, utf16);
            PRUint32 layoutLength = len + trailingCharsToIgnore;
            if (!InitTextRun(textRun, utf16.get(), utf16.Length(),
                             layoutStart, layoutLength,
                             start, len) && maxLen > 1)
                break;
            start += len;
        }
        if (start == aLength)
            break;
        textRun->ResetGlyphRuns();
    }

    textRun->FetchGlyphExtents(aParams->mContext);

    return textRun;
}

gfxTextRun *
gfxAtsuiFontGroup::MakeTextRun(const PRUint8 *aString, PRUint32 aLength,
                               const Parameters *aParams, PRUint32 aFlags)
{
    NS_ASSERTION(aLength > 0, "should use MakeEmptyTextRun for zero-length text");
    NS_ASSERTION(aFlags & TEXT_IS_8BIT, "should be marked 8bit");
    gfxTextRun *textRun = gfxTextRun::Create(aParams, aString, aLength, this, aFlags);
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
                AppendDirectionalIndicatorStart(aFlags, utf16);
            }
            PRUint32 layoutStart = utf16.Length();
            AppendASCIItoUTF16(cString, utf16);
            PRUint32 trailingCharsToIgnore =
                AppendDirectionalIndicatorEnd(wrapBidi, utf16);
            PRUint32 layoutLength = len + trailingCharsToIgnore;
            if (!InitTextRun(textRun, utf16.get(), utf16.Length(),
                             layoutStart, layoutLength,
                             start, len) && maxLen > 1)
                break;
            start += len;
        }
        if (start == aLength)
            break;
        textRun->ResetGlyphRuns();
    }

    textRun->FetchGlyphExtents(aParams->mContext);

    return textRun;
}

PRBool
gfxAtsuiFontGroup::HasFont(ATSFontRef aFontRef)
{
    for (PRUint32 i = 0; i < mFonts.Length(); ++i) {
        if (aFontRef == static_cast<gfxAtsuiFont *>(mFonts.ElementAt(i).get())->GetATSFontRef())
            return PR_TRUE;
    }
    return PR_FALSE;
}

struct PrefFontCallbackData {
    PrefFontCallbackData(nsTArray<nsRefPtr<gfxFontFamily> >& aFamiliesArray) 
        : mPrefFamilies(aFamiliesArray)
    {}

    nsTArray<nsRefPtr<gfxFontFamily> >& mPrefFamilies;

    static PRBool AddFontFamilyEntry(eFontPrefLang aLang, const nsAString& aName, void *aClosure)
    {
        PrefFontCallbackData *prefFontData = static_cast<PrefFontCallbackData*>(aClosure);
        
        gfxFontFamily *family = gfxMacPlatformFontList::PlatformFontList()->FindFamily(aName);
        if (family) {
            prefFontData->mPrefFamilies.AppendElement(family);
        }
        return PR_TRUE;
    }
};


already_AddRefed<gfxFont>
gfxAtsuiFontGroup::WhichPrefFontSupportsChar(PRUint32 aCh)
{
    gfxFont *font;

    // FindCharUnicodeRange only supports BMP character points and there are no non-BMP fonts in prefs
    if (aCh > 0xFFFF)
        return nsnull;

    // get the pref font list if it hasn't been set up already
    PRUint32 unicodeRange = FindCharUnicodeRange(aCh);
    eFontPrefLang charLang = gfxPlatformMac::GetFontPrefLangFor(unicodeRange);

    // if the last pref font was the first family in the pref list, no need to recheck through a list of families
    if (mLastPrefFont && charLang == mLastPrefLang && mLastPrefFirstFont && mLastPrefFont->TestCharacterMap(aCh)) {
        font = mLastPrefFont;
        NS_ADDREF(font);
        return font;
    }

    // based on char lang and page lang, set up list of pref lang fonts to check
    eFontPrefLang prefLangs[kMaxLenPrefLangList];
    PRUint32 i, numLangs = 0;

    gfxPlatformMac *macPlatform = gfxPlatformMac::GetPlatform();
    macPlatform->GetLangPrefs(prefLangs, numLangs, charLang, mPageLang);

    for (i = 0; i < numLangs; i++) {
        nsAutoTArray<nsRefPtr<gfxFontFamily>, 5> families;
        eFontPrefLang currentLang = prefLangs[i];
        
        gfxMacPlatformFontList *fc = gfxMacPlatformFontList::PlatformFontList();

        // get the pref families for a single pref lang
        if (!fc->GetPrefFontFamilyEntries(currentLang, &families)) {
            eFontPrefLang prefLangsToSearch[1] = { currentLang };
            PrefFontCallbackData prefFontData(families);
            gfxPlatform::ForEachPrefFont(prefLangsToSearch, 1, PrefFontCallbackData::AddFontFamilyEntry,
                                           &prefFontData);
            fc->SetPrefFontFamilyEntries(currentLang, families);
        }

        // find the first pref font that includes the character
        PRUint32  i, numPrefs;
        numPrefs = families.Length();
        for (i = 0; i < numPrefs; i++) {
            // look up the appropriate face
            gfxFontFamily *family = families[i];
            if (!family) continue;
            
            // if a pref font is used, it's likely to be used again in the same text run.
            // the style doesn't change so the face lookup can be cached rather than calling
            // GetOrMakeFont repeatedly.  speeds up FindFontForChar lookup times for subsequent
            // pref font lookups
            if (family == mLastPrefFamily && mLastPrefFont->TestCharacterMap(aCh)) {
                font = mLastPrefFont;
                NS_ADDREF(font);
                return font;
            }
            
            PRBool needsBold;
            MacOSFontEntry *fe =
                static_cast<MacOSFontEntry*>(family->FindFontForStyle(mStyle, needsBold));
            // if ch in cmap, create and return a gfxFont
            if (fe && fe->TestCharacterMap(aCh)) {
                nsRefPtr<gfxAtsuiFont> prefFont = GetOrMakeFont(fe, &mStyle, needsBold);
                if (!prefFont) continue;
                mLastPrefFamily = family;
                mLastPrefFont = prefFont;
                mLastPrefLang = charLang;
                mLastPrefFirstFont = (i == 0);
                nsRefPtr<gfxFont> font2 = prefFont.get();
                return font2.forget();
            }

        }
    }

    return nsnull;
}

already_AddRefed<gfxFont> 
gfxAtsuiFontGroup::WhichSystemFontSupportsChar(PRUint32 aCh)
{
    MacOSFontEntry *fe;

    fe = static_cast<MacOSFontEntry*>
        (gfxMacPlatformFontList::PlatformFontList()->FindFontForChar(aCh, GetFontAt(0)));
    if (fe) {
        nsRefPtr<gfxAtsuiFont> atsuiFont = GetOrMakeFont(fe, &mStyle, PR_FALSE); // ignore bolder considerations in system fallback case...
        nsRefPtr<gfxFont> font = atsuiFont.get(); 
        return font.forget();
    }

    return nsnull;
}

void
gfxAtsuiFontGroup::UpdateFontList()
{
    // if user font set is set, check to see if font list needs updating
    if (mUserFontSet && mCurrGeneration != GetGeneration()) {
        // xxx - can probably improve this to detect when all fonts were found, so no need to update list
        mFonts.Clear();
        mUnderlineOffset = UNDERLINE_OFFSET_NOT_SET;
        InitFontList();
        mCurrGeneration = GetGeneration();
    }
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
 * This flag seems to be set on glyphs that have overrun the 32K pixel
 * limit in ATSUI.
 */
#define ATSUI_OVERRUNNING_GLYPH_FLAG 0x100000

/**
 * Returns true if there are overrunning glyphs
 */
static PRBool
PostLayoutCallback(ATSULineRef aLine, gfxTextRun *aTextRun,
                   const PRUnichar *aString, PRInt32 aLayoutLength,
                   PRInt32 aOffsetInTextRun, PRInt32 aLengthInTextRun,
                   const PRPackedBool *aUnmatched)
{
    AutoLayoutDataArrayPtr baselineDeltasArray(aLine, kATSUDirectDataBaselineDeltaFixedArray);
    Fixed *baselineDeltas = static_cast<Fixed *>(baselineDeltasArray.mArray);
    AutoLayoutDataArrayPtr glyphRecordsArray(aLine, kATSUDirectDataLayoutRecordATSLayoutRecordCurrent);

    PRInt32 numGlyphs = glyphRecordsArray.mItemCount;
    if (numGlyphs == 0 || !glyphRecordsArray.mArray) {
        NS_WARNING("Failed to retrieve key glyph data");
        return PR_FALSE;
    }
    ATSLayoutRecord *glyphRecords = static_cast<ATSLayoutRecord *>(glyphRecordsArray.mArray);
    NS_ASSERTION(!baselineDeltas || baselineDeltasArray.mItemCount == (PRUint32)numGlyphs,
                 "Mismatched glyph counts");
    NS_ASSERTION(glyphRecords[numGlyphs - 1].flags & kATSGlyphInfoTerminatorGlyph,
                 "Last glyph should be a terminator glyph");
    --numGlyphs;
    if (numGlyphs == 0)
        return PR_FALSE;

    PRBool isLTR = !aTextRun->IsRightToLeft();

    PRUint32 trailingCharactersToIgnore = aLayoutLength - aLengthInTextRun;
    if (trailingCharactersToIgnore > 0) {
        // The glyph array includes a glyph for the artificial trailing
        // non-whitespace character. Strip that glyph from the array now.
        if (isLTR) {
            NS_ASSERTION((PRInt32)glyphRecords[numGlyphs - trailingCharactersToIgnore].originalOffset == aLengthInTextRun*2,
                         "Couldn't find glyph for trailing marker");
        } else {
            NS_ASSERTION((PRInt32)glyphRecords[trailingCharactersToIgnore - 1].originalOffset == aLengthInTextRun*2,
                         "Couldn't find glyph for trailing marker");
            glyphRecords += trailingCharactersToIgnore;
        }
        numGlyphs -= trailingCharactersToIgnore;
        if (numGlyphs == 0)
            return PR_FALSE;
    }

    PRUint32 allFlags = 0;
    PRUint32 appUnitsPerDevUnit = aTextRun->GetAppUnitsPerDevUnit();

    nsAutoTArray<gfxTextRun::DetailedGlyph,8> detailedGlyphs;
    gfxTextRun::CompressedGlyph g;

    Fixed runWidth = glyphRecords[numGlyphs].realPos - glyphRecords[0].realPos;

    static const PRInt32 NO_GLYPH = -1;
    nsAutoTArray<PRInt32,128> charToGlyphArray;
    if (!charToGlyphArray.SetLength(aLengthInTextRun))
        return NS_ERROR_OUT_OF_MEMORY;

    PRInt32 *charToGlyph = charToGlyphArray.Elements();
    for (PRInt32 offset = 0; offset < aLengthInTextRun; ++offset) {
        charToGlyph[offset] = NO_GLYPH;
    }
    for (PRInt32 g = 0; g < numGlyphs; ++g) {
        // Note that ATSUI's "originalOffset" is measured in bytes, hence the use of
        // originalOffset/2 throughout this function
        charToGlyph[glyphRecords[g].originalOffset/2] = g;
    }

    // Find character and glyph clumps that correspond, allowing for ligatures,
    // indic reordering, split glyphs, etc.
    //
    // The idea is that we'll find a character sequence starting at the first char of aString,
    // and reaching far enough to include the character associated with the first glyph;
    // we also extend it as long as there are "holes" in the range of glyphs. So we
    // will eventually have a contiguous sequence of characters, starting at the beginning
    // of the range, that map to a contiguous sequence of glyphs, starting at the beginning
    // of the glyph array. That's a clump; then we update the starting positions and repeat.
    // (In many cases, of course, the clump is simply one character, one glyph.)
    //
    // NB: In the case of RTL layouts, we iterate over the string in reverse.
    //
    // This may find characters that fall outside the range 0:aLengthInTextRun,
    // so we won't necessarily use everything we find here.

    PRInt32 glyphStart = 0; // looking for a clump that starts at this glyph index
    PRInt32 charStart = isLTR ? 0 : aLengthInTextRun-1; // and this char index
    PRInt32 direction = isLTR ? 1 : -1; // increment to use for iterating through characters

    while (glyphStart < numGlyphs) { // keep finding groups until all glyphs are accounted for

        PRInt32 charEnd = (PRInt32)glyphRecords[glyphStart].originalOffset/2;
        PRInt32 charLimit = isLTR ? aLengthInTextRun : -1;
        PRInt32 glyphEnd = glyphStart;
        PRBool inOrder = PR_TRUE;
        do {
            // This is normally executed once for each iteration of the outer loop,
            // but in unusual cases where the character/glyph association is complex,
            // the initial character range might correspond to a non-contiguous
            // glyph range with "holes" in it. If so, we will repeat this loop to
            // extend the character range until we have a contiguous glyph sequence.
            charEnd += direction;
            while (charEnd != charLimit && charToGlyph[charEnd] == NO_GLYPH) {
                charEnd += direction;
            }
            // in RTL, back up if we ended at a "deleted" low surrogate
            // (belongs with the next clump)
            if (!isLTR && charToGlyph[charEnd+1] == NO_GLYPH &&
                NS_IS_LOW_SURROGATE(aString[charEnd+1])) {
                charEnd += 1;
            }

            // find the maximum glyph index covered by the clump so far
            for (PRInt32 i = charStart; i != charEnd; i += direction) {
                if (charToGlyph[i] != NO_GLYPH) {
                    glyphEnd = PR_MAX(glyphEnd, charToGlyph[i] + 1); // update extent of glyph range
                }
            }

            // if next glyph is an ATSUI deleted-glyph code, extend range to include its char
            PRBool extendedCharRange = PR_FALSE;
            while (glyphEnd < numGlyphs && glyphRecords[glyphEnd].glyphID == 0xffff) {
                if (isLTR) {
                    if ((PRInt32)glyphRecords[glyphEnd].originalOffset/2 >= charEnd) {
                        // point at the char, not beyond, as this will be incremented
                        // when we repeat the outer loop
                        charEnd = (PRInt32)glyphRecords[glyphEnd].originalOffset/2;
                        extendedCharRange = PR_TRUE;
                    }
                } else {
                    if ((PRInt32)glyphRecords[glyphEnd].originalOffset/2 <= charEnd) {
                        charEnd = (PRInt32)glyphRecords[glyphEnd].originalOffset/2;
                        extendedCharRange = PR_TRUE;
                    }
                }
                ++glyphEnd;
            }
            if (extendedCharRange) {
                // with sufficiently bizarre reordering, this might cause us to include more glyphs
                // (if the deleted-glyph code was associated with a distant character)
                continue;
            }

            if (glyphEnd == glyphStart + 1) {
                // for the common case of a single-glyph clump, we can skip the following check
                break;
            }

            // check whether all glyphs in the range are associated with the characters
            // in our clump; if not, we have a discontinuous range, and should extend it
            // unless we've reached the end of the text
            PRBool allGlyphsAreWithinCluster = PR_TRUE;
            PRInt32 prevGlyphCharIndex = charStart;
            for (PRInt32 i = glyphStart; i < glyphEnd; ++i) {
                PRInt32 glyphCharIndex = (PRInt32)glyphRecords[i].originalOffset/2;
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
        // and endCharIndex to the limit (position beyond the last char).
        PRInt32 baseCharIndex, endCharIndex;
        if (isLTR) {
            baseCharIndex = charStart;
            endCharIndex = charEnd;
        } else {
            baseCharIndex = charEnd + 1;
            endCharIndex = charStart + 1;
        }

        // Check if the clump falls outside our range; if so, just go to the next.
        if (baseCharIndex >= aLayoutLength || endCharIndex <= 0) {
            glyphStart = glyphEnd;
            charStart = charEnd;
            continue;
        }

        // charIndex might be < 0 if we had a leading combining mark, for example,
        // that got ligated with the space that was prefixed to the string
        baseCharIndex = PR_MAX(baseCharIndex, 0);
        endCharIndex = PR_MIN(endCharIndex, aLayoutLength);

        // record missing glyphs in the textRun
        if (aUnmatched && aUnmatched[baseCharIndex]) {
            for (PRInt32 i = baseCharIndex; i < endCharIndex; ++i) {
                if (NS_IS_HIGH_SURROGATE(aString[i]) &&
                    i + 1 < aLayoutLength &&
                    NS_IS_LOW_SURROGATE(aString[i + 1])) {
                    aTextRun->SetMissingGlyph(aOffsetInTextRun + i,
                                              SURROGATE_TO_UCS4(aString[i],
                                                                aString[i + 1]));
                    ++i;
                } else {
                    aTextRun->SetMissingGlyph(aOffsetInTextRun + i, aString[i]);
                }
            }
            glyphStart = glyphEnd;
            charStart = charEnd;
            continue;
        }

        // Now we're ready to set the glyph info in the textRun; measure the glyph width
        // of the first (perhaps only) glyph, to see if it is "Simple"
        double toNextGlyph;
        if (glyphStart < numGlyphs-1) {
            toNextGlyph = FixedToFloat(glyphRecords[glyphStart+1].realPos -
                                       glyphRecords[glyphStart].realPos);
        } else {
            toNextGlyph = FixedToFloat(glyphRecords[0].realPos + runWidth -
                                       glyphRecords[glyphStart].realPos);
        }
        PRInt32 advance = PRInt32(toNextGlyph * appUnitsPerDevUnit);

        // update base- and endCharIndex to be absolute within the textRun/
        // for setting glyph info
        baseCharIndex += aOffsetInTextRun;
        endCharIndex += aOffsetInTextRun;

        // Check if it's a simple one-to-one mapping
        PRInt32 glyphsInClump = glyphEnd - glyphStart;
        if (glyphsInClump == 1 &&
            gfxTextRun::CompressedGlyph::IsSimpleGlyphID(glyphRecords[glyphStart].glyphID) &&
            gfxTextRun::CompressedGlyph::IsSimpleAdvance(advance) &&
            aTextRun->IsClusterStart(baseCharIndex) &&
            (!baselineDeltas || baselineDeltas[glyphStart] == 0))
        {
            aTextRun->SetSimpleGlyph(baseCharIndex,
                                     g.SetSimpleGlyph(advance, glyphRecords[glyphStart].glyphID));
        } else {
            // collect all glyphs in a list to be assigned to the first char;
            // there must be at least one in the clump, and we already measured its advance,
            // hence the placement of the loop-exit test and the measurement of the next glyph
            while (1) {
                gfxTextRun::DetailedGlyph *details = detailedGlyphs.AppendElement();
                details->mGlyphID = glyphRecords[glyphStart].glyphID;
                details->mXOffset = 0;
                details->mYOffset = !baselineDeltas ? 0.0f
                    : - FixedToFloat(baselineDeltas[glyphStart]) * appUnitsPerDevUnit;
                details->mAdvance = advance;
                if (++glyphStart >= glyphEnd) {
                    break;
                }
                if (glyphStart < numGlyphs-1) {
                    toNextGlyph = FixedToFloat(glyphRecords[glyphStart+1].realPos -
                                               glyphRecords[glyphStart].realPos);
                } else {
                    toNextGlyph = FixedToFloat(glyphRecords[0].realPos + runWidth -
                                               glyphRecords[glyphStart].realPos);
                }
                advance = PRInt32(toNextGlyph * appUnitsPerDevUnit);
            }

            gfxTextRun::CompressedGlyph g;
            g.SetComplex(aTextRun->IsClusterStart(baseCharIndex),
                         PR_TRUE, detailedGlyphs.Length());
            aTextRun->SetGlyphs(baseCharIndex, g, detailedGlyphs.Elements());

            detailedGlyphs.Clear();
        }

        // the rest of the chars in the group are ligature continuations, no associated glyphs
        while (++baseCharIndex != endCharIndex) {
            g.SetComplex(inOrder && aTextRun->IsClusterStart(baseCharIndex),
                         PR_FALSE, 0);
            aTextRun->SetGlyphs(baseCharIndex, g, nsnull);
        }

        glyphStart = glyphEnd;
        charStart = charEnd;
    }

    return (allFlags & ATSUI_OVERRUNNING_GLYPH_FLAG) != 0;
}

struct PostLayoutCallbackClosure {
    gfxTextRun                  *mTextRun;
    const PRUnichar             *mString;
    PRUint32                     mLayoutLength;
    PRUint32                     mOffsetInTextRun;
    PRUint32                     mLengthInTextRun;
    // Either null or an array of stringlength booleans set to true for
    // each character that did not match any fonts
    nsAutoArrayPtr<PRPackedBool> mUnmatchedChars;
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
                           gCallbackClosure->mString,
                           gCallbackClosure->mLayoutLength,
                           gCallbackClosure->mOffsetInTextRun,
                           gCallbackClosure->mLengthInTextRun,
                           gCallbackClosure->mUnmatchedChars);
    *oCallbackStatus = kATSULayoutOperationCallbackStatusContinue;
    return noErr;
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



static ATSUStyle
SetLayoutRangeToFont(ATSUTextLayout layout, ATSUStyle mainStyle, UniCharArrayOffset offset,
                      UniCharCount length, ATSUFontID fontID)
{
    ATSUStyle subStyle;
    ATSUCreateStyle (&subStyle);
    ATSUCopyAttributes (mainStyle, subStyle);

    ATSUAttributeTag fontTags[] = { kATSUFontTag };
    ByteCount fontArgSizes[] = { sizeof(ATSUFontID) };
    ATSUAttributeValuePtr fontArgs[] = { &fontID };

    ATSUSetAttributes (subStyle, 1, fontTags, fontArgSizes, fontArgs);

    // apply the new style to the layout for the changed substring
    ATSUSetRunStyle (layout, subStyle, offset, length);

    return subStyle;
}

PRBool
gfxAtsuiFontGroup::InitTextRun(gfxTextRun *aRun,
                               const PRUnichar *aString, PRUint32 aLength,
                               PRUint32 aLayoutStart, PRUint32 aLayoutLength,
                               PRUint32 aOffsetInTextRun, PRUint32 aLengthInTextRun)
{
    OSStatus status;
    gfxAtsuiFont *firstFont = GetFontAt(0);
    ATSUStyle mainStyle = firstFont->GetATSUStyle();
    nsTArray<ATSUStyle> stylesToDispose;
    const PRUnichar *layoutString = aString + aLayoutStart;

#ifdef DUMP_TEXT_RUNS
    NS_ConvertUTF16toUTF8 str(layoutString, aLengthInTextRun);
    NS_ConvertUTF16toUTF8 families(mFamilies);
    PR_LOG(gAtsuiTextRunLog, PR_LOG_DEBUG,\
           ("InitTextRun %p fontgroup %p (%s) lang: %s len %d TEXTRUN \"%s\" ENDTEXTRUN\n",
            aRun, this, families.get(), mStyle.langGroup.get(), aLengthInTextRun, str.get()) );
//    PR_LOG(gAtsuiTextRunLog, PR_LOG_DEBUG,
//           ("InitTextRun font: %s user font set: %p (%8.8x)\n",
//            NS_ConvertUTF16toUTF8(firstFont->GetUniqueName()).get(), mUserFontSet, PRUint32(mCurrGeneration)) );
#endif

    if (aRun->GetFlags() & TEXT_DISABLE_OPTIONAL_LIGATURES) {
        status = ATSUCreateAndCopyStyle(mainStyle, &mainStyle);
        if (status == noErr) {
            stylesToDispose.AppendElement(mainStyle);
            DisableCommonLigatures(mainStyle);
        }
    }

    UniCharCount runLengths = aLengthInTextRun;
    ATSUTextLayout layout;
    // Create the text layout for the whole string, but only produce glyphs
    // for the text inside LRO/RLO - PDF, if present. For wrapped strings
    // we do need to produce glyphs for the trailing non-whitespace
    // character to ensure that ATSUI treats all whitespace as non-trailing.
    status = ATSUCreateTextLayoutWithTextPtr
        (aString,
         aLayoutStart,
         aLayoutLength,
         aLength,
         1,
         &runLengths,
         &mainStyle,
         &layout);
    // XXX error check here?

    PostLayoutCallbackClosure closure;
    closure.mTextRun = aRun;
    closure.mString = layoutString;
    closure.mLayoutLength = aLayoutLength;
    closure.mOffsetInTextRun = aOffsetInTextRun;
    closure.mLengthInTextRun = aLengthInTextRun;
    NS_ASSERTION(!gCallbackClosure, "Reentering InitTextRun? Expect disaster!");
    gCallbackClosure = &closure;

    ATSULayoutOperationOverrideSpecifier override;
    override.operationSelector = kATSULayoutOperationPostLayoutAdjustment;
    override.overrideUPP = PostLayoutOperationCallback;

    // Set up our layout attributes
    ATSLineLayoutOptions lineLayoutOptions = kATSLineKeepSpacesOutOfMargin | kATSLineHasNoHangers;

    static ATSUAttributeTag layoutTags[] = {
        kATSULineLayoutOptionsTag,
        kATSULayoutOperationOverrideTag
    };
    static ByteCount layoutArgSizes[] = {
        sizeof(ATSLineLayoutOptions),
        sizeof(ATSULayoutOperationOverrideSpecifier)
    };

    ATSUAttributeValuePtr layoutArgs[] = {
        &lineLayoutOptions,
        &override
    };
    ATSUSetLayoutControls(layout,
                          NS_ARRAY_LENGTH(layoutTags),
                          layoutTags,
                          layoutArgSizes,
                          layoutArgs);

    /* Now go through and update the styles for the text, based on font matching. */

    nsAutoArrayPtr<PRUnichar> mirroredStr;

    UniCharArrayOffset runStart = aLayoutStart;
    UniCharCount runLength = aLengthInTextRun;

    /// ---- match fonts using cmap info instead of ATSUI ----

    nsTArray<gfxTextRange> fontRanges;

    ComputeRanges(fontRanges, aString, runStart, runStart + runLength);

    PRUint32 r, numRanges = fontRanges.Length();

    for (r = 0; r < numRanges; r++) {
        const gfxTextRange& range = fontRanges[r];
   
        gfxAtsuiFont *matchedFont;
        UniCharCount  matchedLength;
        
        // match a range of text
        matchedLength = range.Length();
        matchedFont = static_cast<gfxAtsuiFont*> (range.font ? range.font.get() : nsnull);

#ifdef DUMP_TEXT_RUNS
        PR_LOG(gAtsuiTextRunLog, PR_LOG_DEBUG, ("InitTextRun %p fontgroup %p font %p match %s (%d-%d)", aRun, this, matchedFont, (matchedFont ? NS_ConvertUTF16toUTF8(matchedFont->GetUniqueName()).get() : "<null>"), runStart, matchedLength));
#endif
        //printf("Matched: %s [%d, %d)\n", (matchedFont ? NS_ConvertUTF16toUTF8(matchedFont->GetUniqueName()).get() : "<null>"), runStart, runStart + matchedLength);
        
        // in the RTL case, handle fallback mirroring
        if (aRun->IsRightToLeft() && matchedFont && !matchedFont->HasMirroringInfo()) {
            MirrorSubstring(layout, mirroredStr, aString, aLength, runStart, runLength);
        }       

        // if no matched font, mark as unmatched
        if (!matchedFont) {
        
            aRun->AddGlyphRun(firstFont, aOffsetInTextRun + runStart - aLayoutStart, PR_TRUE);
            
            if (!closure.mUnmatchedChars) {
                closure.mUnmatchedChars = new PRPackedBool[aLength];
                if (closure.mUnmatchedChars) {
                    //printf("initializing %d\n", aLength);
                    memset(closure.mUnmatchedChars.get(), PR_FALSE, aLength);
                }
            }

            if (closure.mUnmatchedChars) {
                //printf("setting %d unmatched from %d\n", matchedLength, runStart - headerChars);
                memset(closure.mUnmatchedChars.get() + runStart - aLayoutStart,
                       PR_TRUE, matchedLength);
            }
            
        } else {
        
            if (matchedFont != firstFont) {
                // create a new sub-style and add it to the layout
                ATSUStyle subStyle = SetLayoutRangeToFont(layout, mainStyle, runStart,
                                                          matchedLength, matchedFont->GetATSFontRef());
                stylesToDispose.AppendElement(subStyle);
            }

            // add a glyph run for the matched substring
            aRun->AddGlyphRun(matchedFont, aOffsetInTextRun + runStart - aLayoutStart, PR_TRUE);
        }
        
        runStart += matchedLength;
        runLength -= matchedLength;    
    }


    /// -------------------------------------------------

    // xxx - for some reason, this call appears to be needed to avoid assertions about glyph runs not being coalesced properly
    //       this appears to occur when there are unmatched characters in the text run
    aRun->SortGlyphRuns();

    // Trigger layout so that our callback fires. We don't actually care about
    // the result of this call.
    ATSTrapezoid trap;
    ItemCount trapCount;
    ATSUGetGlyphBounds(layout, 0, 0, aLayoutStart, aLengthInTextRun,
                       kATSUseFractionalOrigins, 1, &trap, &trapCount); 

    ATSUDisposeTextLayout(layout);

    aRun->AdjustAdvancesForSyntheticBold(aOffsetInTextRun, aLengthInTextRun);

    PRUint32 i;
    for (i = 0; i < stylesToDispose.Length(); ++i) {
        ATSUDisposeStyle(stylesToDispose[i]);
    }
    gCallbackClosure = nsnull;
    return !closure.mOverrunningGlyphs;
}

void
gfxAtsuiFontGroup::InitFontList()
{
    ForEachFont(FindATSFont, this);

    if (mFonts.Length() == 0) {
        // XXX this will generate a list of the lang groups for which we have no
        // default fonts for on the mac; we should fix this!
        // Known:
        // ja x-beng x-devanagari x-tamil x-geor x-ethi x-gujr x-mlym x-armn
        // x-orya x-telu x-knda x-sinh

        //fprintf (stderr, "gfxAtsuiFontGroup: %s [%s] -> %d fonts found\n", NS_ConvertUTF16toUTF8(families).get(), mStyle.langGroup.get(), mFonts.Length());

        // If we get here, we most likely didn't have a default font for
        // a specific langGroup.  Let's just pick the default OSX
        // user font.

        PRBool needsBold;
        MacOSFontEntry *defaultFont = static_cast<MacOSFontEntry*>
            (gfxMacPlatformFontList::PlatformFontList()->GetDefaultFont(&mStyle, needsBold));
        NS_ASSERTION(defaultFont, "invalid default font returned by GetDefaultFont");

        nsRefPtr<gfxAtsuiFont> font = GetOrMakeFont(defaultFont, &mStyle, needsBold);

        if (font) {
            mFonts.AppendElement(font);
        }
    }

    if (!mStyle.systemFont) {
        for (PRUint32 i = 0; i < mFonts.Length(); ++i) {
            gfxAtsuiFont* font = static_cast<gfxAtsuiFont*>(mFonts[i].get());
            if (font->GetFontEntry()->mIsBadUnderlineFont) {
                gfxFloat first = mFonts[0]->GetMetrics().underlineOffset;
                gfxFloat bad = font->GetMetrics().underlineOffset;
                mUnderlineOffset = PR_MIN(first, bad);
                break;
            }
        }
    }
}

#endif /* not __LP64__ */

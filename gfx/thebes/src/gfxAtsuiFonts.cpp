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
 *   John Daggett <jdaggett@mozilla.com>
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
#include "gfxAtsuiFonts.h"

#include "gfxFontTest.h"
#include "gfxFontUtils.h"

#include "cairo-quartz.h"

#include "gfxQuartzSurface.h"
#include "gfxQuartzFontCache.h"
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

eFontPrefLang GetFontPrefLangFor(PRUint8 aUnicodeRange);

gfxAtsuiFont::gfxAtsuiFont(MacOSFontEntry *aFontEntry,
                           const gfxFontStyle *fontStyle, PRBool aNeedsBold)
    : gfxFont(aFontEntry, fontStyle),
      mFontStyle(fontStyle), mATSUStyle(nsnull),
      mHasMirroring(PR_FALSE), mHasMirroringLookedUp(PR_FALSE), mAdjustedSize(0.0f)
{
    ATSUFontID fontID = aFontEntry->GetFontID();
    ATSFontRef fontRef = FMGetATSFontRefFromFont(fontID);

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


ATSUFontID gfxAtsuiFont::GetATSUFontID()
{
    return GetFontEntry()->GetFontID();
}

static void
DisableUncommonLigatures(ATSUStyle aStyle)
{
    static const ATSUFontFeatureType types[] = {
        kLigaturesType,
        kLigaturesType,
        kLigaturesType,
        kLigaturesType,
        kLigaturesType,
        kLigaturesType
    };
    static const ATSUFontFeatureType selectors[NS_ARRAY_LENGTH(types)] = {
        kRareLigaturesOffSelector,
        kLogosOffSelector,
        kRebusPicturesOffSelector,
        kDiphthongLigaturesOffSelector,
        kSquaredLigaturesOffSelector,
        kAbbrevSquaredLigaturesOffSelector
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
    DisableUncommonLigatures(mATSUStyle);

    /* Now pull out the metrics */

    ATSFontMetrics atsMetrics;
    ATSFontGetHorizontalMetrics(aFontRef, kATSOptionFlagsDefault,
                                &atsMetrics);

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
    fprintf (stderr, "Font: %p size: %f (fixed: %d)", this, size, gfxQuartzFontCache::SharedFontCache()->IsFixedPitch(aFontID));
    fprintf (stderr, "    emHeight: %f emAscent: %f emDescent: %f\n", mMetrics.emHeight, mMetrics.emAscent, mMetrics.emDescent);
    fprintf (stderr, "    maxAscent: %f maxDescent: %f maxAdvance: %f\n", mMetrics.maxAscent, mMetrics.maxDescent, mMetrics.maxAdvance);
    fprintf (stderr, "    internalLeading: %f externalLeading: %f\n", mMetrics.externalLeading, mMetrics.internalLeading);
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
    cairo_scaled_font_destroy(mScaledFont);
    cairo_font_face_destroy(mFontFace);

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
        status = ATSFontGetTable(GetATSUFontID(), 'prop', 0, 0, 0, &size);
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
    return static_cast< MacOSFontEntry*> (mFontEntry.get());
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
    ForEachFont(FindATSUFont, this);

    if (mFonts.Length() == 0) {
        // XXX this will generate a list of the lang groups for which we have no
        // default fonts for on the mac; we should fix this!
        // Known:
        // ja x-beng x-devanagari x-tamil x-geor x-ethi x-gujr x-mlym x-armn
        // x-orya x-telu x-knda x-sinh

        //fprintf (stderr, "gfxAtsuiFontGroup: %s [%s] -> %d fonts found\n", NS_ConvertUTF16toUTF8(families).get(), aStyle->langGroup.get(), mFonts.Length());

        // If we get here, we most likely didn't have a default font for
        // a specific langGroup.  Let's just pick the default OSX
        // user font.

        PRBool needsBold;
        MacOSFontEntry *defaultFont = gfxQuartzFontCache::SharedFontCache()->GetDefaultFont(aStyle, needsBold);
        NS_ASSERTION(defaultFont, "invalid default font returned by GetDefaultFont");

        nsRefPtr<gfxAtsuiFont> font = GetOrMakeFont(defaultFont, aStyle, needsBold);

        if (font) {
            mFonts.AppendElement(font);
        }
    }

    mPageLang = gfxPlatform::GetFontPrefLangFor(mStyle.langGroup.get());

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

PRBool
gfxAtsuiFontGroup::FindATSUFont(const nsAString& aName,
                                const nsACString& aGenericName,
                                void *closure)
{
    gfxAtsuiFontGroup *fontGroup = (gfxAtsuiFontGroup*) closure;
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
        gfxQuartzFontCache *fc = gfxQuartzFontCache::SharedFontCache();
        fe = fc->FindFontForFamily(aName, fontStyle, needsBold);
    }

    if (fe && !fontGroup->HasFont(fe->GetFontID())) {
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
            // Remember that this character is not the start of a cluster by
            // setting its glyph data to "not a cluster start", "is a
            // ligature start", with no glyphs.
            aTextRun->SetGlyphs(i, g.SetComplex(PR_FALSE, PR_TRUE, 0), nsnull);
        }
        breakOffset = next;
    }
    UCDisposeTextBreakLocator(&locator);
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
    if (gfxPlatformMac::GetPlatform()->OSXVersion() >= MAC_OS_X_VERSION_10_5_HEX) {
        realGuessMax = PR_MIN(500, realGuessMax);
    }

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
    gfxTextRun *textRun = gfxTextRun::Create(aParams, aString, aLength, this, aFlags);
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
gfxAtsuiFontGroup::HasFont(ATSUFontID fid)
{
    for (PRUint32 i = 0; i < mFonts.Length(); ++i) {
        if (fid == static_cast<gfxAtsuiFont *>(mFonts.ElementAt(i).get())->GetATSUFontID())
            return PR_TRUE;
    }
    return PR_FALSE;
}

struct PrefFontCallbackData {
    PrefFontCallbackData(nsTArray<nsRefPtr<MacOSFamilyEntry> >& aFamiliesArray) 
        : mPrefFamilies(aFamiliesArray)
    {}

    nsTArray<nsRefPtr<MacOSFamilyEntry> >& mPrefFamilies;

    static PRBool AddFontFamilyEntry(eFontPrefLang aLang, const nsAString& aName, void *aClosure)
    {
        PrefFontCallbackData *prefFontData = (PrefFontCallbackData*) aClosure;
        
        MacOSFamilyEntry *family = gfxQuartzFontCache::SharedFontCache()->FindFamily(aName);
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
    eFontPrefLang charLang = GetFontPrefLangFor(unicodeRange);

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
        nsAutoTArray<nsRefPtr<MacOSFamilyEntry>, 5> families;
        eFontPrefLang currentLang = prefLangs[i];
        
        gfxQuartzFontCache *fc = gfxQuartzFontCache::SharedFontCache();

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
            MacOSFamilyEntry *family = families[i];
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
            MacOSFontEntry *fe = family->FindFont(&mStyle, needsBold);
            // if ch in cmap, create and return a gfxFont
            if (fe && fe->TestCharacterMap(aCh)) {
                nsRefPtr<gfxAtsuiFont> prefFont = GetOrMakeFont(fe, &mStyle, needsBold);
                if (!prefFont) continue;
                mLastPrefFamily = family;
                mLastPrefFont = prefFont;
                mLastPrefLang = charLang;
                mLastPrefFirstFont = (i == 0);
                nsRefPtr<gfxFont> font2 = (gfxFont*) prefFont;
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

    fe = gfxQuartzFontCache::SharedFontCache()->FindFontForChar(aCh, GetFontAt(0));
    if (fe) {
        nsRefPtr<gfxAtsuiFont> atsuiFont = GetOrMakeFont(fe, &mStyle, PR_FALSE); // ignore bolder considerations in system fallback case...
        nsRefPtr<gfxFont> font = (gfxFont*) atsuiFont; 
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
        ForEachFont(FindATSUFont, this);
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
                           gfxTextRun *aRun, PRUint32 aOffsetInTextRun,
                           const PRPackedBool *aUnmatched,
                           const PRUnichar *aString,
                           const PRUint32 aLength)
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
            if (NS_IS_HIGH_SURROGATE(aString[index]) &&
                index + 1 < aLength &&
                NS_IS_LOW_SURROGATE(aString[index + 1])) {
                aRun->SetMissingGlyph(aOffsetInTextRun + index,
                                      SURROGATE_TO_UCS4(aString[index],
                                                        aString[index + 1]));
            } else {
                aRun->SetMissingGlyph(aOffsetInTextRun + index, aString[index]);
            }
        }
        return;
    }

    gfxTextRun::CompressedGlyph g;
    PRUint32 offset;
    // Make all but the first character in the group NOT be a ligature boundary,
    // i.e. fuse the group into a ligature.
    // Also make them not be cluster boundaries, i.e., fuse them into a cluster,
    // if the glyphs are out of character order.
    for (offset = firstOffset + 2; offset <= lastOffset; offset += 2) {
        PRUint32 charIndex = aOffsetInTextRun + offset/2;
        PRBool makeClusterStart = inOrder && aRun->IsClusterStart(charIndex);
        g.SetComplex(makeClusterStart, PR_FALSE, 0);
        aRun->SetGlyphs(charIndex, g, nsnull);
    }

    // Grab total advance for all glyphs
    PRInt32 advance = GetAdvanceAppUnits(aGlyphs, aGlyphCount, aAppUnitsPerDevUnit);
    PRUint32 charIndex = aOffsetInTextRun + firstOffset/2;
    if (regularGlyphCount == 1) {
        if (advance >= 0 &&
            (!aBaselineDeltas || aBaselineDeltas[displayGlyph - aGlyphs] == 0) &&
            gfxTextRun::CompressedGlyph::IsSimpleAdvance(advance) &&
            gfxTextRun::CompressedGlyph::IsSimpleGlyphID(displayGlyph->glyphID) &&
            aRun->IsClusterStart(charIndex)) {
            aRun->SetSimpleGlyph(charIndex, g.SetSimpleGlyph(advance, displayGlyph->glyphID));
            return;
        }
    }

    nsAutoTArray<gfxTextRun::DetailedGlyph,10> detailedGlyphs;
    ATSLayoutRecord *advanceStart = aGlyphs;
    for (i = 0; i < aGlyphCount; ++i) {
        ATSLayoutRecord *glyph = &aGlyphs[i];
        if (glyph->glyphID != ATSUI_SPECIAL_GLYPH_ID) {
            if (glyph->originalOffset > firstOffset) {
                PRUint32 glyphCharIndex = aOffsetInTextRun + glyph->originalOffset/2;
                PRUint32 glyphRunIndex = aRun->FindFirstGlyphRunContaining(glyphCharIndex);
                PRUint32 numGlyphRuns;
                const gfxTextRun::GlyphRun *glyphRun = aRun->GetGlyphRuns(&numGlyphRuns) + glyphRunIndex;

                if (glyphRun->mCharacterOffset > charIndex) {
                    // The font has changed inside the character group. This might
                    // happen in some weird situations, e.g. if
                    // ATSUI decides in LTR text to put the glyph for character
                    // 1 before the glyph for character 0, AND decides to
                    // give character 1's glyph a different font from character
                    // 0. This sucks because we can't then safely move this
                    // glyph to be associated with our first character.
                    // To handle this we'd have to do some funky hacking with
                    // glyph advances and offsets so that the glyphs stay
                    // associated with the right characters but they are
                    // displayed out of order. Let's not do this for now,
                    // in the hope that it doesn't come up. If it does come up,
                    // at least we can fix it right here without changing
                    // any other code.
                    NS_ERROR("Font change inside character group!");
                    // Be safe, just throw out this glyph
                    continue;
                }
            }

            gfxTextRun::DetailedGlyph *details = detailedGlyphs.AppendElement();
            if (!details)
                return;
            details->mAdvance = 0;
            details->mGlyphID = glyph->glyphID;
            details->mXOffset = 0;
            if (detailedGlyphs.Length() > 1) {
                details->mXOffset +=
                    GetAdvanceAppUnits(advanceStart, glyph - advanceStart,
                                       aAppUnitsPerDevUnit);
            }
            details->mYOffset = !aBaselineDeltas ? 0.0f
                : FixedToFloat(aBaselineDeltas[i])*aAppUnitsPerDevUnit;
        }
    }
    if (detailedGlyphs.Length() == 0) {
        NS_WARNING("No glyphs visible at all!");
        aRun->SetGlyphs(aOffsetInTextRun + charIndex, g.SetMissing(0), nsnull);
        return;
    }

    // The advance width for the whole cluster
    PRInt32 clusterAdvance = GetAdvanceAppUnits(aGlyphs, aGlyphCount, aAppUnitsPerDevUnit);
    if (aRun->IsRightToLeft())
        detailedGlyphs[0].mAdvance = clusterAdvance;
    else
        detailedGlyphs[detailedGlyphs.Length() - 1].mAdvance = clusterAdvance;
    g.SetComplex(aRun->IsClusterStart(charIndex), PR_TRUE, detailedGlyphs.Length());
    aRun->SetGlyphs(charIndex, g, detailedGlyphs.Elements());
}

/**
 * Returns true if there are overrunning glyphs
 */
static PRBool
PostLayoutCallback(ATSULineRef aLine, gfxTextRun *aRun,
                   const PRUnichar *aString, PRUint32 aLayoutLength,
                   PRUint32 aOffsetInTextRun, PRUint32 aLengthInTextRun,
                   const PRPackedBool *aUnmatched)
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

    PRUint32 trailingCharactersToIgnore = aLayoutLength - aLengthInTextRun;
    if (trailingCharactersToIgnore > 0) {
        // The glyph array includes a glyph for the artificial trailing
        // non-whitespace character. Strip that glyph from the array now.
        if (isRTL) {
            NS_ASSERTION(glyphRecords[trailingCharactersToIgnore - 1].originalOffset == aLengthInTextRun*2,
                         "Couldn't find glyph for trailing marker");
            glyphRecords += trailingCharactersToIgnore;
        } else {
            NS_ASSERTION(glyphRecords[numGlyphs - trailingCharactersToIgnore].originalOffset == aLengthInTextRun*2,
                         "Couldn't find glyph for trailing marker");
        }
        numGlyphs -= trailingCharactersToIgnore;
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
        // Determine the glyphs for this ligature group
        while (glyphCount < numGlyphs) {
            ATSLayoutRecord *glyph = &glyphRecords[glyphIndex + direction*glyphCount];
            PRUint32 glyphOffset = glyph->originalOffset;
            allFlags |= glyph->flags;
            if (glyphOffset <= lastOffset) {
                // Always add the current glyph to the ligature group if it's for the same
                // character as a character whose glyph is already in the group,
                // or an earlier character. The latter can happen because ATSUI
                // sometimes visually reorders glyphs; e.g. DEVANAGARI VOWEL I
                // can have its glyph displayed before the glyph for the consonant that's
                // it's logically after (even though this is all left-to-right text).
                // In this case we need to make sure the glyph for the consonant
                // is added to the group containing the vowel.
            } else {
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
                                       appUnitsPerDevUnit, aRun, aOffsetInTextRun,
                                       aUnmatched, aString, aLengthInTextRun);
        } else {
            SetGlyphsForCharacterGroup(glyphRecords,
                                       glyphCount, baselineDeltas,
                                       appUnitsPerDevUnit, aRun, aOffsetInTextRun,
                                       aUnmatched, aString, aLengthInTextRun);
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

// xxx - leaving this here for now, probably belongs in platform code somewhere

eFontPrefLang
GetFontPrefLangFor(PRUint8 aUnicodeRange)
{
    switch (aUnicodeRange) {
        case kRangeSetLatin:   return eFontPrefLang_Western;
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
    PR_LOG(gAtsuiTextRunLog, PR_LOG_DEBUG,
           ("InitTextRun font: %s user font set: %p (%8.8x)\n",
            NS_ConvertUTF16toUTF8(firstFont->GetUniqueName()).get(), mUserFontSet, PRUint32(mCurrGeneration)) );
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
                ATSUStyle subStyle = SetLayoutRangeToFont(layout, mainStyle, runStart, matchedLength, matchedFont->GetATSUFontID());
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


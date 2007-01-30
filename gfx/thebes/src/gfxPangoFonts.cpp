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
 *   Vladimir Vukicevic <vladimir@mozilla.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *
 * based on nsFontMetricsPango.cpp by
 *   Christopher Blizzard <blizzard@mozilla.org>
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

#ifdef XP_BEOS
#define THEBES_USE_PANGO_CAIRO
#endif

#define PANGO_ENABLE_ENGINE
#define PANGO_ENABLE_BACKEND

#include "prtypes.h"
#include "prlink.h"
#include "gfxTypes.h"

#include "nsUnicodeRange.h"

#include "nsIPref.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsServiceManagerUtils.h"
#include "nsUnitConversion.h"

#include "nsVoidArray.h"
#include "nsPromiseFlatString.h"

#include "gfxContext.h"
#include "gfxPangoFonts.h"

#include "nsCRT.h"

#include "cairo.h"

#ifndef THEBES_USE_PANGO_CAIRO
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkpango.h>


#include <freetype/tttables.h>
#include <fontconfig/fontconfig.h>

#include <pango/pango-font.h>
#include <pango/pangoxft.h>

#include "cairo-ft.h"

#include "gfxPlatformGtk.h"

#else // THEBES_USE_PANGO_CAIRO

#include <pango/pangocairo.h>

#endif // THEBES_USE_PANGO_CAIRO

#include <math.h>

#define FLOAT_PANGO_SCALE ((gfxFloat)PANGO_SCALE)
#define NSToCoordRound(x) (floor((x) + 0.5))

static PangoLanguage *GetPangoLanguage(const nsACString& aLangGroup);
static void GetMozLanguage(const PangoLanguage *aLang, nsACString &aMozLang);

/**
 ** gfxPangoFontGroup
 **/

static int
FFRECountHyphens (const nsAString &aFFREName)
{
    int h = 0;
    PRInt32 hyphen = 0;
    while ((hyphen = aFFREName.FindChar('-', hyphen)) >= 0) {
        ++h;
        ++hyphen;
    }
    return h;
}

PRBool
gfxPangoFontGroup::FontCallback (const nsAString& fontName,
                                 const nsACString& genericName,
                                 void *closure)
{
    nsStringArray *sa = NS_STATIC_CAST(nsStringArray*, closure);

    if (FFRECountHyphens(fontName) < 3 && sa->IndexOf(fontName) < 0) {
        sa->AppendString(fontName);
    }

    return PR_TRUE;
}

gfxPangoFontGroup::gfxPangoFontGroup (const nsAString& families,
                                      const gfxFontStyle *aStyle)
    : gfxFontGroup(families, aStyle)
{
    g_type_init();

    nsStringArray familyArray;

    mFontCache.Init(15);

    ForEachFont (FontCallback, &familyArray);

    FindGenericFontFromStyle (FontCallback, &familyArray);

    // XXX If there are no actual fonts, we should use dummy family.
    // Pango will resolve from this.
    if (familyArray.Count() == 0) {
        // printf("%s(%s)\n", NS_ConvertUTF16toUTF8(families).get(),
        //                    aStyle->langGroup.get());
        familyArray.AppendString(NS_LITERAL_STRING("sans-serif"));
    }

    for (int i = 0; i < familyArray.Count(); i++)
        mFonts.AppendElement(new gfxPangoFont(*familyArray[i], &mStyle));
}

gfxPangoFontGroup::~gfxPangoFontGroup()
{
}

gfxFontGroup *
gfxPangoFontGroup::Copy(const gfxFontStyle *aStyle)
{
    return new gfxPangoFontGroup(mFamilies, aStyle);
}

gfxTextRun *
gfxPangoFontGroup::MakeTextRun(const PRUnichar *aString, PRUint32 aLength,
                               Parameters *aParams)
{
    return new gfxPangoTextRun(this, aString, aLength, aParams);
}

gfxTextRun *
gfxPangoFontGroup::MakeTextRun(const PRUint8 *aString, PRUint32 aLength,
                               Parameters *aParams)
{
#ifdef USE_XFT_FOR_ASCII
    return new gfxXftTextRun(aString, aLength, aPersistentString);
#else
    return new gfxPangoTextRun(this, aString, aLength, aParams);
#endif
}

/**
 ** gfxPangoFont
 **/

// Glue to avoid build/runtime dependencies on Pango > 1.6,
// because we like living in 1999

#ifndef THEBES_USE_PANGO_CAIRO
static void
(* PTR_pango_font_description_set_absolute_size)(PangoFontDescription*, double)
    = nsnull;

static void InitPangoLib()
{
    static PRBool initialized = PR_FALSE;
    if (initialized)
        return;
    initialized = PR_TRUE;

    g_type_init();

    PRLibrary *lib = PR_LoadLibrary("libpango-1.0.so");
    if (!lib)
        return;

    PTR_pango_font_description_set_absolute_size =
        (void (*)(PangoFontDescription*, double))
        PR_FindFunctionSymbol(lib, "pango_font_description_set_absolute_size");

    // leak lib deliberately
}

static void
MOZ_pango_font_description_set_absolute_size(PangoFontDescription *desc,
                                             double size)
{
    if (PTR_pango_font_description_set_absolute_size) {
        PTR_pango_font_description_set_absolute_size(desc, size);
    } else {
        pango_font_description_set_size(desc,
                                        (gint)(size * 72.0 /
                                               gfxPlatformGtk::DPI()));
    }
}
#else
static void InitPangoLib()
{
}

static void
MOZ_pango_font_description_set_absolute_size(PangoFontDescription *desc, double size)
{
    pango_font_description_set_absolute_size(desc, size);
}
#endif

gfxPangoFont::gfxPangoFont(const nsAString &aName,
                           const gfxFontStyle *aFontStyle)
    : gfxFont(aName, aFontStyle),
    mPangoFontDesc(nsnull), mPangoCtx(nsnull),
    mXftFont(nsnull), mHasMetrics(PR_FALSE),
    mAdjustedSize(0)
{
    InitPangoLib();
}

gfxPangoFont::~gfxPangoFont()
{
    if (mPangoCtx)
        g_object_unref(mPangoCtx);

    if (mPangoFontDesc)
        pango_font_description_free(mPangoFontDesc);
}

static PangoStyle
ThebesStyleToPangoStyle (const gfxFontStyle *fs)
{
    if (fs->style == FONT_STYLE_ITALIC)
        return PANGO_STYLE_ITALIC;
    if (fs->style == FONT_STYLE_OBLIQUE)
        return PANGO_STYLE_OBLIQUE;

    return PANGO_STYLE_NORMAL;
}

static PangoWeight
ThebesStyleToPangoWeight (const gfxFontStyle *fs)
{
    PRInt32 w = fs->weight;

    /*
     * weights come in two parts crammed into one
     * integer -- the "base" weight is weight / 100,
     * the rest of the value is the "offset" from that
     * weight -- the number of steps to move to adjust
     * the weight in the list of supported font weights,
     * this value can be negative or positive.
     */
    PRInt32 baseWeight = (w + 50) / 100;
    PRInt32 offset = w - baseWeight * 100;

    /* clip weights to range 0 to 9 */
    if (baseWeight < 0)
        baseWeight = 0;
    if (baseWeight > 9)
        baseWeight = 9;

    /* Map from weight value to fcWeights index */
    static int fcWeightLookup[10] = {
        0, 0, 0, 0, 1, 1, 2, 3, 3, 4,
    };

    PRInt32 fcWeight = fcWeightLookup[baseWeight];

    /*
     * adjust by the offset value, make sure we stay inside the 
     * fcWeights table
     */
    fcWeight += offset;

    if (fcWeight < 0)
        fcWeight = 0;
    if (fcWeight > 4)
        fcWeight = 4;

    /* Map to final PANGO_WEIGHT value */
    static int fcWeights[5] = {
        349,
        499,
        649,
        749,
        999
    };

    return (PangoWeight)fcWeights[fcWeight];
}

void
gfxPangoFont::RealizeFont(PRBool force)
{
    // already realized?
    if (!force && mPangoFontDesc)
        return;

    if (mPangoCtx)
        g_object_unref(mPangoCtx);
    if (mPangoFontDesc)
        pango_font_description_free(mPangoFontDesc);

    mPangoFontDesc = pango_font_description_new();

    pango_font_description_set_family(mPangoFontDesc, NS_ConvertUTF16toUTF8(mName).get());
    gfxFloat size = mAdjustedSize ? mAdjustedSize : mStyle->size;
    MOZ_pango_font_description_set_absolute_size(mPangoFontDesc, size * PANGO_SCALE);
    pango_font_description_set_style(mPangoFontDesc, ThebesStyleToPangoStyle(mStyle));
    pango_font_description_set_weight(mPangoFontDesc, ThebesStyleToPangoWeight(mStyle));

    //printf ("%s, %f, %d, %d\n", NS_ConvertUTF16toUTF8(mName).get(), mStyle->size, ThebesStyleToPangoStyle(mStyle), ThebesStyleToPangoWeight(mStyle));
#ifndef THEBES_USE_PANGO_CAIRO
    mPangoCtx = pango_xft_get_context(GDK_DISPLAY(), 0);
    gdk_pango_context_set_colormap(mPangoCtx, gdk_rgb_get_cmap());
#else
    mPangoCtx = pango_cairo_font_map_create_context(PANGO_CAIRO_FONT_MAP(pango_cairo_font_map_get_default()));
#endif

    if (!mStyle->langGroup.IsEmpty())
        pango_context_set_language(mPangoCtx, GetPangoLanguage(mStyle->langGroup));

    pango_context_set_font_description(mPangoCtx, mPangoFontDesc);

    mHasMetrics = PR_FALSE;

    if (mAdjustedSize != 0)
        return;

    mAdjustedSize = mStyle->size;
    if (mStyle->sizeAdjust == 0)
        return;

    gfxSize isz, lsz;
    GetSize("x", 1, isz, lsz);
    gfxFloat aspect = isz.height / mStyle->size;
    mAdjustedSize =
        PR_MAX(NSToCoordRound(mStyle->size*(mStyle->sizeAdjust/aspect)), 1.0f);
    RealizeFont(PR_TRUE);
}

void
gfxPangoFont::RealizeXftFont(PRBool force)
{
    // already realized?
    if (!force && mXftFont)
        return;
    if (GDK_DISPLAY() == 0) {
        mXftFont = nsnull;
        return;
    }

    PangoFcFont *fcfont = PANGO_FC_FONT(GetPangoFont());
    mXftFont = pango_xft_font_get_font(PANGO_FONT(fcfont));
}

void
gfxPangoFont::GetSize(const char *aCharString, PRUint32 aLength, gfxSize& inkSize, gfxSize& logSize)
{
    RealizeFont();

    PangoAttrList *al = pango_attr_list_new();
    GList *items = pango_itemize(mPangoCtx, aCharString, 0, aLength, al, NULL);
    pango_attr_list_unref(al);

    if (!items || g_list_length(items) != 1)
        return;

    PangoItem *item = (PangoItem*) items->data;

    PangoGlyphString *glstr = pango_glyph_string_new();
    pango_shape (aCharString, aLength, &(item->analysis), glstr);

    PangoRectangle ink_rect, log_rect;
    pango_glyph_string_extents (glstr, item->analysis.font, &ink_rect, &log_rect);

    inkSize.width = ink_rect.width / FLOAT_PANGO_SCALE;
    inkSize.height = ink_rect.height / FLOAT_PANGO_SCALE;

    logSize.width = log_rect.width / FLOAT_PANGO_SCALE;
    logSize.height = log_rect.height / FLOAT_PANGO_SCALE;

    pango_glyph_string_free(glstr);
    pango_item_free(item);
    g_list_free(items);
}

// rounding and truncation functions for a Freetype floating point number 
// (FT26Dot6) stored in a 32bit integer with high 26 bits for the integer
// part and low 6 bits for the fractional part. 
#define MOZ_FT_ROUND(x) (((x) + 32) & ~63) // 63 = 2^6 - 1
#define MOZ_FT_TRUNC(x) ((x) >> 6)
#define CONVERT_DESIGN_UNITS_TO_PIXELS(v, s) \
        MOZ_FT_TRUNC(MOZ_FT_ROUND(FT_MulFix((v) , (s))))

const gfxFont::Metrics&
gfxPangoFont::GetMetrics()
{
    if (mHasMetrics)
        return mMetrics;

    RealizeFont();

    PangoAttrList *al = pango_attr_list_new();
    GList *items = pango_itemize(mPangoCtx, "a", 0, 1, al, NULL);
    pango_attr_list_unref(al);

    if (!items || g_list_length(items) != 1)
        return mMetrics;        // XXX error

#ifndef THEBES_USE_PANGO_CAIRO
    float val;

    PangoItem *item = (PangoItem*)items->data;
    PangoFcFont *fcfont = PANGO_FC_FONT(item->analysis.font);

    XftFont *xftFont = pango_xft_font_get_font(PANGO_FONT(fcfont));
    if (!xftFont)
        return mMetrics;        // XXX error

    FT_Face face = XftLockFace(xftFont);
    if (!face)
        return mMetrics;        // XXX error

    int size;
    if (FcPatternGetInteger(fcfont->font_pattern, FC_PIXEL_SIZE, 0, &size) != FcResultMatch)
        size = 12;
    mMetrics.emHeight = PR_MAX(1, size);

    mMetrics.maxAscent = xftFont->ascent;
    mMetrics.maxDescent = xftFont->descent;

    double lineHeight = mMetrics.maxAscent + mMetrics.maxDescent;

    if (lineHeight > mMetrics.emHeight)
        mMetrics.internalLeading = lineHeight - mMetrics.emHeight;
    else
        mMetrics.internalLeading = 0;
    mMetrics.externalLeading = 0;

    mMetrics.maxHeight = lineHeight;
    mMetrics.emAscent = mMetrics.maxAscent * mMetrics.emHeight / lineHeight;
    mMetrics.emDescent = mMetrics.emHeight - mMetrics.emAscent;
    mMetrics.maxAdvance = xftFont->max_advance_width;

    gfxSize isz, lsz;
    GetSize(" ", 1, isz, lsz);
    mMetrics.spaceWidth = lsz.width;

    // XXX do some FcCharSetHasChar work here to make sure
    // we have an "x"
    GetSize("x", 1, isz, lsz);
    mMetrics.xHeight = isz.height;
    mMetrics.aveCharWidth = isz.width;

    val = CONVERT_DESIGN_UNITS_TO_PIXELS(face->underline_position,
                                         face->size->metrics.y_scale);
    if (!val)
        val = - PR_MAX(1, floor(0.1 * xftFont->height + 0.5));

    mMetrics.underlineOffset = val;

    val = CONVERT_DESIGN_UNITS_TO_PIXELS(face->underline_thickness,
                                         face->size->metrics.y_scale);
    if (!val)
        val = floor(0.05 * xftFont->height + 0.5);

    mMetrics.underlineSize = PR_MAX(1, val);

    TT_OS2 *os2 = (TT_OS2 *) FT_Get_Sfnt_Table(face, ft_sfnt_os2);

    if (os2 && os2->ySuperscriptYOffset) {
        val = CONVERT_DESIGN_UNITS_TO_PIXELS(os2->ySuperscriptYOffset,
                                             face->size->metrics.y_scale);
        mMetrics.superscriptOffset = PR_MAX(1, val);
    } else {
        mMetrics.superscriptOffset = mMetrics.xHeight;
    }

    // mSubscriptOffset
    if (os2 && os2->ySubscriptYOffset) {
        val = CONVERT_DESIGN_UNITS_TO_PIXELS(os2->ySubscriptYOffset,
                                             face->size->metrics.y_scale);
        // some fonts have the incorrect sign. 
        val = (val < 0) ? -val : val;
        mMetrics.subscriptOffset = PR_MAX(1, val);
    } else {
        mMetrics.subscriptOffset = mMetrics.xHeight;
    }

    mMetrics.strikeoutOffset = mMetrics.xHeight / 2.0;
    mMetrics.strikeoutSize = mMetrics.underlineSize;

    XftUnlockFace(xftFont);
#else
    /* pango_cairo case; try to get all the metrics from pango itself */
    PangoItem *item = (PangoItem*)items->data;
    PangoFont *font = item->analysis.font;

    PangoFontMetrics *pfm = pango_font_get_metrics (font, NULL);

    // ??
    mMetrics.emHeight = mAdjustedSize ? mAdjustedSize : mStyle->size;

    mMetrics.maxAscent = pango_font_metrics_get_ascent(pfm) / FLOAT_PANGO_SCALE;
    mMetrics.maxDescent = pango_font_metrics_get_descent(pfm) / FLOAT_PANGO_SCALE;

    gfxFloat lineHeight = mMetrics.maxAscent + mMetrics.maxDescent;

    if (lineHeight > mMetrics.emHeight)
        mMetrics.externalLeading = lineHeight - mMetrics.emHeight;
    else
        mMetrics.externalLeading = 0;
    mMetrics.internalLeading = 0;

    mMetrics.maxHeight = lineHeight;

    mMetrics.emAscent = mMetrics.maxAscent * mMetrics.emHeight / lineHeight;
    mMetrics.emDescent = mMetrics.emHeight - mMetrics.emAscent;

    mMetrics.maxAdvance = pango_font_metrics_get_approximate_char_width(pfm) / FLOAT_PANGO_SCALE; // XXX

    gfxSize isz, lsz;
    GetSize(" ", 1, isz, lsz);
    mMetrics.spaceWidth = lsz.width;
    GetSize("x", 1, isz, lsz);
    mMetrics.xHeight = isz.height;

    mMetrics.aveCharWidth = pango_font_metrics_get_approximate_char_width(pfm) / FLOAT_PANGO_SCALE;

    mMetrics.underlineOffset = pango_font_metrics_get_underline_position(pfm) / FLOAT_PANGO_SCALE;
    mMetrics.underlineSize = pango_font_metrics_get_underline_thickness(pfm) / FLOAT_PANGO_SCALE;

    mMetrics.strikeoutOffset = pango_font_metrics_get_strikethrough_position(pfm) / FLOAT_PANGO_SCALE;
    mMetrics.strikeoutSize = pango_font_metrics_get_strikethrough_thickness(pfm) / FLOAT_PANGO_SCALE;

    // these are specified by the so-called OS2 SFNT info, but
    // pango doesn't expose this to us.  This really sucks,
    // so we just assume it's the xHeight
    mMetrics.superscriptOffset = mMetrics.xHeight;
    mMetrics.subscriptOffset = mMetrics.xHeight;

    pango_font_metrics_unref (pfm);
#endif

#if 0
    fprintf (stderr, "Font: %s\n", NS_ConvertUTF16toUTF8(mName).get());
    fprintf (stderr, "    emHeight: %f emAscent: %f emDescent: %f\n", mMetrics.emHeight, mMetrics.emAscent, mMetrics.emDescent);
    fprintf (stderr, "    maxAscent: %f maxDescent: %f\n", mMetrics.maxAscent, mMetrics.maxDescent);
    fprintf (stderr, "    internalLeading: %f externalLeading: %f\n", mMetrics.externalLeading, mMetrics.internalLeading);
    fprintf (stderr, "    spaceWidth: %f aveCharWidth: %f xHeight: %f\n", mMetrics.spaceWidth, mMetrics.aveCharWidth, mMetrics.xHeight);
    fprintf (stderr, "    uOff: %f uSize: %f stOff: %f stSize: %f suOff: %f suSize: %f\n", mMetrics.underlineOffset, mMetrics.underlineSize, mMetrics.strikeoutOffset, mMetrics.strikeoutSize, mMetrics.superscriptOffset, mMetrics.subscriptOffset);
#endif

    mHasMetrics = PR_TRUE;
    return mMetrics;
}

void
gfxPangoFont::GetMozLang(nsACString &aMozLang)
{
    aMozLang.Assign(mStyle->langGroup);
}

PangoFont *
gfxPangoFont::GetPangoFont()
{
    RealizeFont();
    return pango_context_load_font(mPangoCtx, mPangoFontDesc);
}

static const char *sCJKLangGroup[] = {
    "ja",
    "ko",
    "zh-CN",
    "zh-HK",
    "zh-TW"
};

#define COUNT_OF_CJK_LANG_GROUP 5
#define CJK_LANG_JA    sCJKLangGroup[0]
#define CJK_LANG_KO    sCJKLangGroup[1]
#define CJK_LANG_ZH_CN sCJKLangGroup[2]
#define CJK_LANG_ZH_HK sCJKLangGroup[3]
#define CJK_LANG_ZH_TW sCJKLangGroup[4]

static PRInt32
GetCJKLangGroupIndex(const char *aLangGroup)
{
    PRInt32 i;
    for (i = 0; i < COUNT_OF_CJK_LANG_GROUP; i++) {
        if (!PL_strcasecmp(aLangGroup, sCJKLangGroup[i]))
            return i;
    }
    return -1;
}

/**
 ** gfxPangoTextRun
 * 
 * Some serious problems:
 *
 * -- We draw with a font that's hinted for the CTM, but we measure with a font
 * hinted to the identity matrix, so our "bounding metrics" may not be accurate.
 * 
 * -- CreateScaledFont doesn't necessarily give us the font that the Pango
 * metrics assume.
 * 
 **/

PRBool
gfxPangoTextRun::GlyphRunIterator::NextRun()  {
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

/**
 * We use this to append an LTR or RTL Override character to the start of the
 * string. This forces Pango to honour our direction even if there are neutral characters
 * in the string.
 */
static PRInt32 AppendDirectionalIndicatorUTF8(PRBool aIsRTL, nsACString& aString)
{
    static const PRUnichar overrides[2][2] =
      { { 0x202d, 0 }, { 0x202e, 0 }}; // LRO, RLO
    AppendUTF16toUTF8(overrides[aIsRTL], aString);
    return 3; // both overrides map to 3 bytes in UTF8
}

gfxPangoTextRun::gfxPangoTextRun(gfxPangoFontGroup *aGroup,
                                 const PRUint8 *aString, PRUint32 aLength,
                                 gfxTextRunFactory::Parameters *aParams)
  : gfxTextRun(aParams, PR_TRUE), mFontGroup(aGroup), mCharacterCount(aLength)
{
    mCharacterGlyphs = new CompressedGlyph[aLength];
    if (!mCharacterGlyphs)
        return;

    NS_ASSERTION(mFlags & gfxTextRunFactory::TEXT_IS_8BIT,
                 "Someone should have set the 8-bit flag already");

    const gchar *utf8Chars = NS_REINTERPRET_CAST(const gchar*, aString);

    PRBool isRTL = IsRightToLeft();
    if (!isRTL) {
        // We don't need to send an override character here, the characters must be all
        // LTR
        Init(aParams, utf8Chars, aLength, 0, nsnull, 0);
    } else {
        // XXX this could be more efficient
        NS_ConvertASCIItoUTF16 unicodeString(utf8Chars, aLength);
        nsCAutoString utf8;
        PRInt32 headerLen = AppendDirectionalIndicatorUTF8(isRTL, utf8);
        AppendUTF16toUTF8(unicodeString, utf8);
        Init(aParams, utf8.get(), utf8.Length(), headerLen, nsnull, 0);
    }
}

gfxPangoTextRun::gfxPangoTextRun(gfxPangoFontGroup *aGroup,
                                 const PRUnichar *aString, PRUint32 aLength,
                                 gfxTextRunFactory::Parameters *aParams)
  : gfxTextRun(aParams, PR_FALSE), mFontGroup(aGroup), mCharacterCount(aLength)
{
    mCharacterGlyphs = new CompressedGlyph[aLength];
    if (!mCharacterGlyphs)
        return;

    if (mFlags & gfxTextRunFactory::TEXT_HAS_SURROGATES) {
        // Record surrogates as parts of clusters now. This helps us
        // convert UTF8/UTF16 coordinates
        PRUint32 i;
        for (i = 0; i < aLength; ++i) {
            if (NS_IS_LOW_SURROGATE(aString[i])) {
                mCharacterGlyphs[i].SetComplex(CompressedGlyph::TAG_LOW_SURROGATE);
            }
        }
    }

    nsCAutoString utf8;
    PRInt32 headerLen = AppendDirectionalIndicatorUTF8(IsRightToLeft(), utf8);
    AppendUTF16toUTF8(Substring(aString, aString + aLength), utf8);
    Init(aParams, utf8.get(), utf8.Length(), headerLen, aString, aLength);
}

gfxPangoTextRun::~gfxPangoTextRun()
{
}

void
gfxPangoTextRun::Init(gfxTextRunFactory::Parameters *aParams, const gchar *aUTF8Text,
                      PRUint32 aUTF8Length, PRUint32 aUTF8HeaderLength,
                      const PRUnichar *aUTF16Text, PRUint32 aUTF16Length)
{
#if defined(ENABLE_XFT_FAST_PATH_ALWAYS)
    CreateGlyphRunsXft(aUTF8Text + aUTF8HeaderLength, aUTF8Length - aUTF8HeaderLength);
#else
#if defined(ENABLE_XFT_FAST_PATH_8BIT)
    if (mFlags & gfxTextRunFactory::TEXT_IS_8BIT) {
        CreateGlyphRunsXft(aUTF8Text + aUTF8HeaderLength, aUTF8Length - aUTF8HeaderLength);
        return;
    }
#endif
    SetupPangoContextDirection();
    nsresult rv = CreateGlyphRunsFast(aUTF8Text + aUTF8HeaderLength,
                                      aUTF8Length - aUTF8HeaderLength, aUTF16Text, aUTF16Length);
    if (rv == NS_ERROR_FAILURE) {
        CreateGlyphRunsItemizing(aUTF8Text, aUTF8Length, aUTF8HeaderLength);
    }
#endif
}

void
gfxPangoTextRun::SetupPangoContextDirection()
{
    pango_context_set_base_dir(mFontGroup->GetFontAt(0)->GetPangoContext(),
                               (mFlags & gfxTextRunFactory::TEXT_IS_RTL)
                                  ? PANGO_DIRECTION_RTL : PANGO_DIRECTION_LTR);
}

static PRUint8
GetFlagsForCharacter(gfxPangoTextRun::CompressedGlyph* aData)
{
  return (aData->IsClusterStart() ? gfxTextRun::CLUSTER_START : 0)
       | (aData->IsLigatureContinuation() ? gfxTextRun::CONTINUES_LIGATURE : 0)
       | (aData->CanBreakBefore() ? gfxTextRun::LINE_BREAK_BEFORE : 0);
}

void
gfxPangoTextRun::GetCharFlags(PRUint32 aStart, PRUint32 aLength,
                              PRUint8 *aFlags)
{
    NS_ASSERTION(aStart + aLength <= mCharacterCount, "Substring out of range");

    if (!mCharacterGlyphs) {
        memset(aFlags, CLUSTER_START, aLength);
        return;
    }
    PRUint32 i;
    for (i = 0; i < aLength; ++i) {
        aFlags[i] = GetFlagsForCharacter(&mCharacterGlyphs[aStart + i]);
    }
}

PRUint8
gfxPangoTextRun::GetCharFlags(PRUint32 aOffset)
{
    NS_ASSERTION(aOffset < mCharacterCount, "Character out of range");

    if (!mCharacterGlyphs)
      return 0;
    return GetFlagsForCharacter(&mCharacterGlyphs[aOffset]);
}

PRUint32
gfxPangoTextRun::GetLength()
{
    return mCharacterCount;
}

PRBool
gfxPangoTextRun::SetPotentialLineBreaks(PRUint32 aStart, PRUint32 aLength,
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
gfxPangoTextRun::ComputeClusterAdvance(PRUint32 aClusterOffset)
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

gfxPangoTextRun::LigatureData
gfxPangoTextRun::ComputeLigatureData(PRUint32 aPartOffset, PropertyProvider *aProvider)
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
    result.mLigatureWidth = ComputeClusterAdvance(ligStart)*mPixelsToAppUnits;
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
gfxPangoTextRun::GetAdjustedSpacing(PRUint32 aStart, PRUint32 aEnd,
                                    PropertyProvider *aProvider,
                                    PropertyProvider::Spacing *aSpacing)
{
    if (aStart >= aEnd)
        return;

    aProvider->GetSpacing(aStart, aEnd, aSpacing);

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
                clusterWidth = glyphData->GetSimpleAdvance()*mPixelsToAppUnits;
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
                clusterWidth *= mPixelsToAppUnits;
            }
        }
        aSpacing[aEnd - 1 - aStart].mAfter -= clusterWidth;
    }
}

PRBool
gfxPangoTextRun::GetAdjustedSpacingArray(PRUint32 aStart, PRUint32 aEnd,
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
gfxPangoTextRun::ProcessCairoGlyphsWithSpacing(CairoGlyphProcessorCallback aCB, void *aClosure,
                                               gfxPoint *aPt, PRUint32 aStart, PRUint32 aEnd,
                                               PropertyProvider::Spacing *aSpacing)
{
    if (aStart >= aEnd)
        return;

    double appUnitsToPixels = 1/mPixelsToAppUnits;
    CompressedGlyph *charGlyphs = mCharacterGlyphs;
    double direction = GetDirection();

    nsAutoTArray<cairo_glyph_t,200> glyphBuffer;
    PRUint32 i;
    double x = aPt->x;
    double y = aPt->y;

    PRBool isRTL = IsRightToLeft();

    if (aSpacing) {
        x += direction*aSpacing[0].mBefore*appUnitsToPixels;
    }
    for (i = aStart; i < aEnd; ++i) {
        CompressedGlyph *glyphData = &charGlyphs[i];
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
            NS_ASSERTION(mDetailedGlyphs, "No details but we have a complex cluster...");
            DetailedGlyph *details = mDetailedGlyphs[i];
            for (;;) {
                // Don't try to rendering missing glyphs, this causes weird problems
                double advance = details->mAdvance;
                if (details->mGlyphID != DetailedGlyph::DETAILED_MISSING_GLYPH) {
                    cairo_glyph_t *glyph = glyphBuffer.AppendElement();
                    if (!glyph)
                        return;
                    glyph->index = details->mGlyphID;
                    glyph->x = x + details->mXOffset;
                    glyph->y = y + details->mYOffset;
                    if (isRTL) {
                        glyph->x -= advance;
                    }
                }
                x += direction*advance;
                if (details->mIsLastGlyph)
                    break;
                ++details;
            }
        }
        if (aSpacing) {
            double space = aSpacing[i - aStart].mAfter;
            if (i + 1 < aEnd) {
                space += aSpacing[i + 1 - aStart].mBefore;
            }
            x += direction*space*appUnitsToPixels;
        }
    }
    if (aSpacing) {
        x += direction*aSpacing[aEnd - 1 - aStart].mAfter*appUnitsToPixels;
    }

    *aPt = gfxPoint(x, y);

    aCB(aClosure, glyphBuffer.Elements(), glyphBuffer.Length());
}

void
gfxPangoTextRun::ShrinkToLigatureBoundaries(PRUint32 *aStart, PRUint32 *aEnd)
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
        *aStart = data.mEndOffset;
    }
    if (*aEnd < mCharacterCount && charGlyphs[*aEnd].IsLigatureContinuation()) {
        LigatureData data = ComputeLigatureData(*aEnd, nsnull);
        *aEnd = data.mStartOffset;
    }
}

void
gfxPangoTextRun::CairoShowGlyphs(void *aClosure, cairo_glyph_t *aGlyphs, int aNumGlyphs)
{
    cairo_show_glyphs(NS_STATIC_CAST(cairo_t*, aClosure), aGlyphs, aNumGlyphs);
}

void
gfxPangoTextRun::CairoGlyphsToPath(void *aClosure, cairo_glyph_t *aGlyphs, int aNumGlyphs)
{
    cairo_glyph_path(NS_STATIC_CAST(cairo_t*, aClosure), aGlyphs, aNumGlyphs);
}

void
gfxPangoTextRun::ProcessCairoGlyphs(CairoGlyphProcessorCallback aCB, void *aClosure,
                                    gfxPoint *aPt, PRUint32 aStart, PRUint32 aEnd,
                                    PropertyProvider *aProvider)
{
    if (!(mFlags & gfxTextRunFactory::TEXT_ENABLE_SPACING)) {
        ProcessCairoGlyphsWithSpacing(aCB, aClosure, aPt, aStart, aEnd, nsnull);
        return;
    }

    nsAutoTArray<PropertyProvider::Spacing,200> spacingBuffer;
    if (!spacingBuffer.AppendElements(aEnd - aStart))
        return;
    GetAdjustedSpacing(aStart, aEnd, aProvider, spacingBuffer.Elements());
    ProcessCairoGlyphsWithSpacing(aCB, aClosure, aPt, aStart, aEnd, spacingBuffer.Elements());
}

gfxFloat
gfxPangoTextRun::GetPartialLigatureWidth(PRUint32 aStart, PRUint32 aEnd,
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
gfxPangoTextRun::DrawPartialLigature(gfxContext *aCtx, PRUint32 aOffset,
                                     const gfxRect *aDirtyRect, gfxPoint *aPt,
                                     PropertyProvider *aProvider)
{
    NS_ASSERTION(aDirtyRect, "Cannot draw partial ligatures without a dirty rect");

    if (!mCharacterGlyphs[aOffset].IsClusterStart() || !aDirtyRect)
        return;

    gfxFloat appUnitsToPixels = 1.0/mPixelsToAppUnits;

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
            data.mBeforeSpacing/mPixelsToAppUnits;
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
        afterSpace = data.mAfterSpacing/mPixelsToAppUnits;
    }

    aCtx->Save();
    aCtx->Clip(gfxRect(left, aDirtyRect->Y()*appUnitsToPixels, right - left,
               aDirtyRect->Height()*appUnitsToPixels));
    gfxPoint pt(aPt->x - direction*widthBeforeCluster, aPt->y);
    ProcessCairoGlyphs(CairoShowGlyphs, aCtx->GetCairo(), &pt, data.mStartOffset,
                       data.mEndOffset, aProvider);
    aCtx->Restore();

    aPt->x += direction*(clusterWidth + afterSpace);
}

void
gfxPangoTextRun::Draw(gfxContext *aContext, gfxPoint aPt,
                      PRUint32 aStart, PRUint32 aLength, const gfxRect *aDirtyRect,
                      PropertyProvider *aProvider, gfxFloat *aAdvanceWidth)
{
    NS_ASSERTION(aStart + aLength <= mCharacterCount, "Substring out of range");

    gfxFloat appUnitsToPixels = 1/mPixelsToAppUnits;
    CompressedGlyph *charGlyphs = mCharacterGlyphs;
    gfxFloat direction = GetDirection();

    gfxPoint pt(NSToCoordRound(aPt.x*appUnitsToPixels),
                NSToCoordRound(aPt.y*appUnitsToPixels));
    gfxFloat startX = pt.x;

    GlyphRunIterator iter(this, aStart, aLength);
    while (iter.NextRun()) {
        cairo_t *cr = aContext->GetCairo();
        SetupCairoFont(cr, iter.GetGlyphRun());

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
            DrawPartialLigature(aContext, i, aDirtyRect, &pt, aProvider);
        }

        ProcessCairoGlyphs(CairoShowGlyphs, cr, &pt, ligatureRunStart,
                           ligatureRunEnd, aProvider);

        for (i = ligatureRunEnd; i < end; ++i) {
            DrawPartialLigature(aContext, i, aDirtyRect, &pt, aProvider);
        }
    }

    if (aAdvanceWidth) {
        *aAdvanceWidth = (pt.x - startX)*direction*mPixelsToAppUnits;
    }
}

void
gfxPangoTextRun::DrawToPath(gfxContext *aContext, gfxPoint aPt,
                            PRUint32 aStart, PRUint32 aLength,
                            PropertyProvider *aProvider, gfxFloat *aAdvanceWidth)
{
    NS_ASSERTION(aStart + aLength <= mCharacterCount, "Substring out of range");

    gfxFloat appUnitsToPixels = 1/mPixelsToAppUnits;
    CompressedGlyph *charGlyphs = mCharacterGlyphs;
    gfxFloat direction = GetDirection();

    gfxPoint pt(NSToCoordRound(aPt.x*appUnitsToPixels),
                NSToCoordRound(aPt.y*appUnitsToPixels));
    gfxFloat startX = pt.x;

    GlyphRunIterator iter(this, aStart, aLength);
    while (iter.NextRun()) {
        cairo_t *cr = aContext->GetCairo();
        SetupCairoFont(cr, iter.GetGlyphRun());

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

        ProcessCairoGlyphs(CairoGlyphsToPath, cr, &pt, start, end, aProvider);
    }

    if (aAdvanceWidth) {
        *aAdvanceWidth = (pt.x - startX)*direction*mPixelsToAppUnits;
    }
}

static gfxTextRun::Metrics
GetPangoMetrics(PangoGlyphString *aGlyphs, PangoFont *aPangoFont,
                gfxFloat aPixelsToUnits, PRUint32 aClusterCount)
{
    PangoRectangle inkRect;
    PangoRectangle logicalRect;
    pango_glyph_string_extents(aGlyphs, aPangoFont, &inkRect, &logicalRect);

    gfxFloat scale = aPixelsToUnits/PANGO_SCALE;

    gfxTextRun::Metrics metrics;
    NS_ASSERTION(logicalRect.x == 0, "Weird logical rect...");
    metrics.mAdvanceWidth = logicalRect.width*scale;
    metrics.mAscent = PANGO_ASCENT(logicalRect)*scale;
    metrics.mDescent = PANGO_DESCENT(logicalRect)*scale;
    gfxFloat x = inkRect.x*scale;
    gfxFloat y = inkRect.y*scale;
    metrics.mBoundingBox = gfxRect(x, y, (inkRect.x + inkRect.width)*scale - x,
                                         (inkRect.y + inkRect.height)*scale - y);
    metrics.mClusterCount = aClusterCount;
    return metrics;
}

void
gfxPangoTextRun::AccumulatePangoMetricsForRun(PangoFont *aPangoFont, PRUint32 aStart,
    PRUint32 aEnd, PropertyProvider *aProvider, Metrics *aMetrics)
{
    if (aEnd <= aStart)
        return;

    CompressedGlyph *charGlyphs = mCharacterGlyphs;

    nsAutoTArray<PangoGlyphInfo,200> glyphBuffer;

    nsAutoTArray<PropertyProvider::Spacing,200> spacingBuffer;
    PRBool haveSpacing = GetAdjustedSpacingArray(aStart, aEnd, aProvider, &spacingBuffer);
    gfxFloat appUnitsToPango = gfxFloat(PANGO_SCALE)/mPixelsToAppUnits;

    // We start by assuming every character is a cluster and subtract off
    // characters where that's not true
    PRUint32 clusterCount = aEnd - aStart;

    PRUint32 i;
    for (i = aStart; i < aEnd; ++i) {
        PRUint32 leftSpacePango = 0;
        PRUint32 rightSpacePango = 0;
        if (haveSpacing) {
            PropertyProvider::Spacing *space = &spacingBuffer[i];
            leftSpacePango =
                NSToIntRound((IsRightToLeft() ? space->mAfter : space->mBefore)*appUnitsToPango);
            rightSpacePango =
                NSToIntRound((IsRightToLeft() ? space->mBefore : space->mAfter)*appUnitsToPango);
        }
        CompressedGlyph *glyphData = &charGlyphs[i];
        if (glyphData->IsSimpleGlyph()) {
            PangoGlyphInfo *glyphInfo = glyphBuffer.AppendElement();
            if (!glyphInfo)
                return;
            glyphInfo->attr.is_cluster_start = 0;
            glyphInfo->glyph = glyphData->GetSimpleGlyph();
            glyphInfo->geometry.width = leftSpacePango + rightSpacePango +
                glyphData->GetSimpleAdvance()*PANGO_SCALE;
            glyphInfo->geometry.x_offset = leftSpacePango;
            glyphInfo->geometry.y_offset = 0;
        } else if (glyphData->IsComplexCluster()) {
            NS_ASSERTION(mDetailedGlyphs, "No details but we have a complex cluster...");
            DetailedGlyph *details = mDetailedGlyphs[i];
            PRBool firstGlyph = PR_FALSE;
            for (;;) {
                PangoGlyphInfo *glyphInfo = glyphBuffer.AppendElement();
                if (!glyphInfo)
                    return;
                glyphInfo->attr.is_cluster_start = 0;
                glyphInfo->glyph = details->mGlyphID;
                glyphInfo->geometry.width = NSToIntRound(details->mAdvance*PANGO_SCALE);
                glyphInfo->geometry.x_offset = NSToIntRound(details->mXOffset*PANGO_SCALE);
                if (firstGlyph) {
                    glyphInfo->geometry.width += leftSpacePango;
                    glyphInfo->geometry.x_offset += leftSpacePango;
                    firstGlyph = PR_FALSE;
                }
                glyphInfo->geometry.y_offset = NSToIntRound(details->mYOffset*PANGO_SCALE);
                if (details->mIsLastGlyph) {
                    glyphInfo->geometry.width += rightSpacePango;
                    break;
                }
                ++details;
            }
        } else if (!glyphData->IsClusterStart()) {
            --clusterCount;
        }
    }

    if (IsRightToLeft()) {
        // Reverse glyph order
        for (i = 0; i < (glyphBuffer.Length() >> 1); ++i) {
            PangoGlyphInfo *a = &glyphBuffer[i];
            PangoGlyphInfo *b = &glyphBuffer[glyphBuffer.Length() - 1 - i];
            PangoGlyphInfo tmp = *a;
            *a = *b;
            *b = tmp;
        }
    }

    PangoGlyphString glyphs;
    glyphs.num_glyphs = glyphBuffer.Length();
    glyphs.glyphs = glyphBuffer.Elements();
    glyphs.log_clusters = nsnull;
    glyphs.space = glyphBuffer.Length();
    Metrics metrics = GetPangoMetrics(&glyphs, aPangoFont, mPixelsToAppUnits, clusterCount);

    if (IsRightToLeft()) {
        metrics.CombineWith(*aMetrics);
        *aMetrics = metrics;
    } else {
        aMetrics->CombineWith(metrics);
    }
}

void
gfxPangoTextRun::AccumulatePartialLigatureMetrics(PangoFont *aPangoFont,
    PRUint32 aOffset, PropertyProvider *aProvider, Metrics *aMetrics)
{
    if (!mCharacterGlyphs[aOffset].IsClusterStart())
        return;

    // Measure partial ligature. We hack this by clipping the metrics in the
    // same way we clip the drawing.
    LigatureData data = ComputeLigatureData(aOffset, aProvider);

    // First measure the complete ligature
    Metrics metrics;
    AccumulatePangoMetricsForRun(aPangoFont, data.mStartOffset, data.mEndOffset,
                                 aProvider, &metrics);
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
gfxPangoTextRun::MeasureText(PRUint32 aStart, PRUint32 aLength,
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
        PangoFont *font = iter.GetGlyphRun()->mPangoFont;
        PRUint32 ligatureRunStart = iter.GetStringStart();
        PRUint32 ligatureRunEnd = iter.GetStringEnd();
        ShrinkToLigatureBoundaries(&ligatureRunStart, &ligatureRunEnd);

        PRUint32 i;
        for (i = iter.GetStringStart(); i < ligatureRunStart; ++i) {
            AccumulatePartialLigatureMetrics(font, i, aProvider, &accumulatedMetrics);
        }

        // XXX This sucks. We have to make up the Pango glyphstring and call
        // pango_glyph_string_extents just so we can detect glyphs outside the font
        // box, even when aTightBoundingBox is false, even though in almost all
        // cases we could get correct results just by getting some ascent/descent
        // from the font and using our stored advance widths.
        AccumulatePangoMetricsForRun(font,
            ligatureRunStart, ligatureRunEnd, aProvider, &accumulatedMetrics);

        for (i = ligatureRunEnd; i < iter.GetStringEnd(); ++i) {
            AccumulatePartialLigatureMetrics(font, i, aProvider, &accumulatedMetrics);
        }
    }

    return accumulatedMetrics;
}

#define MEASUREMENT_BUFFER_SIZE 100

PRUint32
gfxPangoTextRun::BreakAndMeasureText(PRUint32 aStart, PRUint32 aMaxLength,
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
            gfxFloat advance = gfxFloat(pixelAdvance)*mPixelsToAppUnits + floatAdvanceUnits;
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
                    floatAdvanceUnits += details->mAdvance*mPixelsToAppUnits;
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
        gfxFloat advance = gfxFloat(pixelAdvance)*mPixelsToAppUnits + floatAdvanceUnits;
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
gfxPangoTextRun::GetAdvanceWidth(PRUint32 aStart, PRUint32 aLength,
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
                result += details->mAdvance*mPixelsToAppUnits;
                if (details->mIsLastGlyph)
                    break;
                ++details;
            }
        }
    }

    return result + gfxFloat(pixelAdvance)*mPixelsToAppUnits;
}

#define IS_MISSING_GLYPH(g) (((g) & 0x10000000) || (g) == 0x0FFFFFFF)

static PangoGlyphString*
GetPangoGlyphsFor(gfxPangoFont *aFont, const char *aText)
{
    PangoAnalysis analysis;
    analysis.font         = aFont->GetPangoFont();
    analysis.level        = 0;
    analysis.lang_engine  = nsnull;
    analysis.extra_attrs  = nsnull;
    analysis.language     = pango_language_from_string("en");
    analysis.shape_engine = pango_font_find_shaper(analysis.font,
                                                   analysis.language,
                                                   'a');

    PangoGlyphString *glyphString = pango_glyph_string_new();
    pango_shape(aText, strlen(aText), &analysis, glyphString);

    PRUint32 i;
    for (i = 0; i < PRUint32(glyphString->num_glyphs); ++i) {
        if (IS_MISSING_GLYPH(glyphString->glyphs[i].glyph)) {
            pango_glyph_string_free(glyphString);
            return nsnull;
        }
    }

    return glyphString;
}

static const gfxPangoFontGroup::SpecialStringData*
GetSpecialStringData(gfxTextRun::SpecialString aString, gfxPangoFontGroup *aFontGroup)
{
    NS_ASSERTION(aString <= gfxTextRun::STRING_MAX, "Unknown special string");
    gfxPangoFontGroup::SpecialStringData *data = &aFontGroup->mSpecialStrings[aString];
    if (!data->mGlyphs) {
        static PRUint8 utf8Hyphen[] = { 0xE2, 0x80, 0x90, 0 }; // U+2010
        static PRUint8 utf8Ellipsis[] = { 0xE2, 0x80, 0xA6, 0 }; // U+2026

        const char *text;
        switch (aString) {
            case gfxTextRun::STRING_ELLIPSIS:
                text = NS_REINTERPRET_CAST(const char*, utf8Ellipsis);
                break;
            case gfxTextRun::STRING_HYPHEN:
                text = NS_REINTERPRET_CAST(const char*, utf8Hyphen);
                break;
            default:
            case gfxTextRun::STRING_SPACE: text = " "; break;
        }

        gfxPangoFont *font = aFontGroup->GetFontAt(0);
        data->mGlyphs = GetPangoGlyphsFor(font, text);
        if (!data->mGlyphs) {
            switch (aString) {
                case gfxTextRun::STRING_ELLIPSIS: text = "..."; break;
                case gfxTextRun::STRING_HYPHEN: text = "-"; break;
                default:
                    NS_WARNING("This font doesn't support the space character? That's messed up");
                    return nsnull;
            }
            data->mGlyphs = GetPangoGlyphsFor(font, text);
            if (!data->mGlyphs) {
                NS_WARNING("This font doesn't support hyphen-minus or full-stop characters? That's messed up");
                return nsnull;
            }
        }

        PangoRectangle logicalRect;
        pango_glyph_string_extents(data->mGlyphs, font->GetPangoFont(), nsnull, &logicalRect);
        data->mAdvance = gfxFloat(logicalRect.width)/PANGO_SCALE;
    }
    return data;
}

static cairo_scaled_font_t*
CreateScaledFont(cairo_t *aCR, cairo_matrix_t *aCTM, PangoFont *aPangoFont)
{
    // XXX is this safe really? We should probably check the font type or something.
    // XXX does this really create the same font that Pango used for measurement?
    // We probably need to work harder here. We should pay particular attention
    // to the font options.
    PangoFcFont *fcfont = PANGO_FC_FONT(aPangoFont);
    cairo_font_face_t *face = cairo_ft_font_face_create_for_pattern(fcfont->font_pattern);
    double size;
    if (FcPatternGetDouble(fcfont->font_pattern, FC_PIXEL_SIZE, 0, &size) != FcResultMatch)
        size = 12.0;
    cairo_matrix_t fontMatrix;
    cairo_matrix_init_scale(&fontMatrix, size, size);
    cairo_font_options_t *fontOptions = cairo_font_options_create();
    cairo_get_font_options(aCR, fontOptions);
    cairo_scaled_font_t *scaledFont =
        cairo_scaled_font_create(face, &fontMatrix, aCTM, fontOptions);
    cairo_font_options_destroy(fontOptions);
    return scaledFont;
}

gfxTextRun::Metrics
gfxPangoTextRun::MeasureTextSpecialString(SpecialString aString,
                                          PRBool aTightBoundingBox)
{
    const gfxPangoFontGroup::SpecialStringData *data = GetSpecialStringData(aString, mFontGroup);
    if (!data)
        return Metrics();

    PangoFont *pangoFont = mFontGroup->GetFontAt(0)->GetPangoFont();
    return GetPangoMetrics(data->mGlyphs, pangoFont, mPixelsToAppUnits, 1);
}

void
gfxPangoTextRun::DrawSpecialString(gfxContext *aContext, gfxPoint aPt,
                                   SpecialString aString)
{
    gfxFloat appUnitsToPixels = 1/mPixelsToAppUnits;
    gfxPoint pt(NSToCoordRound(aPt.x*appUnitsToPixels),
                NSToCoordRound(aPt.y*appUnitsToPixels));
    const gfxPangoFontGroup::SpecialStringData *data = GetSpecialStringData(aString, mFontGroup);
    if (!data)
        return;

    if (IsRightToLeft()) {
        pt.x -= data->mAdvance;
    }
    aContext->MoveTo(pt);

    cairo_glyph_t glyphs[3];
    PRUint32 glyphCount = data->mGlyphs->num_glyphs;
    if (glyphCount > NS_ARRAY_LENGTH(glyphs)) {
        NS_WARNING("Special string requires more than 3 glyphs ... that is bogus");
        return;
    }

    PRUint32 i;
    double x = 0;
    for (i = 0; i < glyphCount; ++i) {
        PangoGlyphInfo *pangoGlyph = &data->mGlyphs->glyphs[i];
        cairo_glyph_t glyph =
            { pangoGlyph->glyph,
              pangoGlyph->geometry.x_offset + x, pangoGlyph->geometry.y_offset };
        x += pangoGlyph->geometry.width;
        // The glyphs in special-strings are always drawn left to right
        glyphs[i] = glyph;
    }

    cairo_t *cr = aContext->GetCairo();
    PangoFont *pangoFont = mFontGroup->GetFontAt(0)->GetPangoFont();
    if (mGlyphRuns[0].mPangoFont == pangoFont) {
        // Use cached font if possible
        SetupCairoFont(cr, &mGlyphRuns[0]);
    } else {
        cairo_matrix_t matrix;
        cairo_get_matrix(cr, &matrix);
        cairo_scaled_font_t *font = CreateScaledFont(cr, &matrix, pangoFont);
        cairo_set_scaled_font(cr, font);
        cairo_scaled_font_destroy(font);
    }

    cairo_show_glyphs(cr, glyphs, glyphCount);
}

gfxFloat
gfxPangoTextRun::GetAdvanceWidthSpecialString(SpecialString aString)
{
    return GetSpecialStringData(aString, mFontGroup)->mAdvance*mPixelsToAppUnits;
}

gfxFont::Metrics
gfxPangoTextRun::GetDecorationMetrics()
{
    gfxFont::Metrics metrics = mFontGroup->GetFontAt(0)->GetMetrics();
    metrics.xHeight *= mPixelsToAppUnits;
    metrics.superscriptOffset *= mPixelsToAppUnits;
    metrics.subscriptOffset *= mPixelsToAppUnits;
    metrics.strikeoutSize *= mPixelsToAppUnits;
    metrics.strikeoutOffset *= mPixelsToAppUnits;
    metrics.underlineSize *= mPixelsToAppUnits;
    metrics.underlineOffset *= mPixelsToAppUnits;
    metrics.height *= mPixelsToAppUnits;
    metrics.internalLeading *= mPixelsToAppUnits;
    metrics.externalLeading *= mPixelsToAppUnits;
    metrics.emHeight *= mPixelsToAppUnits;
    metrics.emAscent *= mPixelsToAppUnits;
    metrics.emDescent *= mPixelsToAppUnits;
    metrics.maxHeight *= mPixelsToAppUnits;
    metrics.maxAscent *= mPixelsToAppUnits;
    metrics.maxDescent *= mPixelsToAppUnits;
    metrics.maxAdvance *= mPixelsToAppUnits;
    metrics.aveCharWidth *= mPixelsToAppUnits;
    metrics.spaceWidth *= mPixelsToAppUnits;
    return metrics;
}

void
gfxPangoTextRun::FlushSpacingCache(PRUint32 aStart)
{
    // Do nothing for now because we don't cache spacing
}

void
gfxPangoTextRun::SetLineBreaks(PRUint32 aStart, PRUint32 aLength,
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

void
gfxPangoTextRun::SetupCairoFont(cairo_t *aCR, GlyphRun *aGlyphRun)
{
    cairo_matrix_t currentCTM;
    cairo_get_matrix(aCR, &currentCTM);

    if (aGlyphRun->mCairoFont) {
        // Need to validate that its CTM is OK
        cairo_matrix_t fontCTM;
        cairo_scaled_font_get_ctm(aGlyphRun->mCairoFont, &fontCTM);
        if (fontCTM.xx == currentCTM.xx && fontCTM.yy == currentCTM.yy &&
            fontCTM.xy == currentCTM.xy && fontCTM.yx == currentCTM.yx) {
            cairo_set_scaled_font(aCR, aGlyphRun->mCairoFont);
            return;
        }

        // Just recreate it from scratch, simplest way
        cairo_scaled_font_destroy(aGlyphRun->mCairoFont);
    }

    aGlyphRun->mCairoFont = CreateScaledFont(aCR, &currentCTM, aGlyphRun->mPangoFont);
    cairo_set_scaled_font(aCR, aGlyphRun->mCairoFont);
}

PRUint32
gfxPangoTextRun::FindFirstGlyphRunContaining(PRUint32 aOffset)
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

void
gfxPangoTextRun::SetupClusterBoundaries(const gchar *aUTF8, PRUint32 aUTF8Length,
                                        PRUint32 aUTF16Offset, PangoAnalysis *aAnalysis)
{
    if (mFlags & gfxTextRunFactory::TEXT_IS_8BIT) {
        // 8-bit text doesn't have clusters.
        // XXX is this true in all languages???
        return;
    }

    nsAutoTArray<PangoLogAttr,2000> buffer;
    if (!buffer.AppendElements(aUTF8Length + 1))
        return;

    // Pango says "the array of PangoLogAttr passed in must have at least N+1
    // elements, if there are N characters in the text being broken"
    pango_break(aUTF8, aUTF8Length, aAnalysis, buffer.Elements(), aUTF8Length + 1);

    PRUint32 index = 0;
    while (index < aUTF8Length) {
        if (!buffer[index].is_cursor_position) {
            mCharacterGlyphs[aUTF16Offset].SetClusterContinuation();
        }
        ++aUTF16Offset;
        if (aUTF16Offset < mCharacterCount &&
            mCharacterGlyphs[aUTF16Offset].IsLowSurrogate()) {
          ++aUTF16Offset;
        }
        // We produced this utf8 so we don't need to worry about malformed stuff
        index = g_utf8_next_char(aUTF8 + index) - aUTF8;
    }
}

nsresult
gfxPangoTextRun::AddGlyphRun(PangoFont *aFont, PRUint32 aUTF16Offset)
{
    GlyphRun *glyphRun = mGlyphRuns.AppendElement();
    if (!glyphRun)
        return NS_ERROR_OUT_OF_MEMORY;

    g_object_ref(aFont);
    glyphRun->mPangoFont = aFont;
    glyphRun->mCairoFont = nsnull;
    glyphRun->mCharacterOffset = aUTF16Offset;
    return NS_OK;
}

gfxPangoTextRun::DetailedGlyph*
gfxPangoTextRun::AllocateDetailedGlyphs(PRUint32 aIndex, PRUint32 aCount)
{
    if (!mDetailedGlyphs) {
        mDetailedGlyphs = new nsAutoArrayPtr<DetailedGlyph>[mCharacterCount];
        if (!mDetailedGlyphs)
            return nsnull;
    }
    DetailedGlyph *details = new DetailedGlyph[aCount];
    if (!details)
        return nsnull;
    mDetailedGlyphs[aIndex] = details;
    return details;
}

static void
SetSkipMissingGlyph(gfxPangoTextRun::DetailedGlyph *aDetails)
{
    aDetails->mIsLastGlyph = PR_TRUE;
    aDetails->mGlyphID = gfxPangoTextRun::DetailedGlyph::DETAILED_MISSING_GLYPH;
    aDetails->mAdvance = 0;
    aDetails->mXOffset = 0;
    aDetails->mYOffset = 0;
}

nsresult
gfxPangoTextRun::SetGlyphs(const gchar *aUTF8, PRUint32 aUTF8Length,
                           PRUint32 *aUTF16Offset, PangoGlyphString *aGlyphs,
                           PangoGlyphUnit aOverrideSpaceWidth,
                           PRBool aAbortOnMissingGlyph)
{
    PRUint32 utf16Offset = *aUTF16Offset;
    PRUint32 index = 0;
    // glyphIndex is the first glyph that belongs to characters at "index"
    // or later
    PRUint32 numGlyphs = aGlyphs->num_glyphs;
    gint *logClusters = aGlyphs->log_clusters;
    PRUint32 glyphCount = 0;
    PRUint32 glyphIndex = IsRightToLeft() ? numGlyphs - 1 : 0;
    PRInt32 direction = IsRightToLeft() ? -1 : 1;

    while (index < aUTF8Length) {
        // Clear existing glyph details
        if (mDetailedGlyphs) {
            mDetailedGlyphs[utf16Offset] = nsnull; 
        }

        if (aUTF8[index] == 0) {
            // treat this null byte like a missing glyph with no advance
            DetailedGlyph *details = AllocateDetailedGlyphs(utf16Offset, 1);
            if (!details)
                return NS_ERROR_OUT_OF_MEMORY;
            SetSkipMissingGlyph(details);
        } else if (glyphCount == numGlyphs ||
                   PRUint32(logClusters[glyphIndex]) > index) {
            // No glyphs for this cluster, and it's not a null byte. It must be a ligature.
            mCharacterGlyphs[utf16Offset].SetLigatureContinuation();
        } else {
            PangoGlyphInfo *glyph = &aGlyphs->glyphs[glyphIndex];
            if (aAbortOnMissingGlyph && IS_MISSING_GLYPH(glyph->glyph))
                return NS_ERROR_FAILURE;

            // One or more glyphs. See if we fit in the compressed area.
            NS_ASSERTION(PRUint32(logClusters[glyphIndex]) == index,
                         "Um, we left some glyphs behind previously");
            PRUint32 glyphClusterCount = 1;
            for (;;) {
                ++glyphCount;
                if (aAbortOnMissingGlyph && IS_MISSING_GLYPH(aGlyphs->glyphs[glyphIndex].glyph))
                    return NS_ERROR_FAILURE;
                glyphIndex += direction;
                if (glyphCount == numGlyphs ||
                    PRUint32(logClusters[glyphIndex]) != index)
                    break;
                ++glyphClusterCount;
            }

            PangoGlyphUnit width = glyph->geometry.width;

            // Override the width of a space, but only for spaces that aren't
            // clustered with something else (like a freestanding diacritical mark)
            if (aOverrideSpaceWidth && aUTF8[index] == ' ' &&
                (utf16Offset + 1 == mCharacterCount ||
                 mCharacterGlyphs[utf16Offset].IsClusterStart())) {
                width = aOverrideSpaceWidth;
            }

            PRInt32 downscaledWidth = width/PANGO_SCALE;
            if (glyphClusterCount == 1 &&
                glyph->geometry.x_offset == 0 && glyph->geometry.y_offset == 0 &&
                width >= 0 && downscaledWidth*PANGO_SCALE == width &&
                CompressedGlyph::IsSimpleAdvance(downscaledWidth) &&
                CompressedGlyph::IsSimpleGlyphID(glyph->glyph)) {
                mCharacterGlyphs[utf16Offset].SetSimpleGlyph(downscaledWidth, glyph->glyph);
            } else {
                // Note that missing-glyph IDs are not simple glyph IDs, so we'll
                // always get here when a glyph is missing
                DetailedGlyph *details = AllocateDetailedGlyphs(utf16Offset, glyphClusterCount);
                if (!details)
                    return NS_ERROR_OUT_OF_MEMORY;
                PRUint32 i;
                for (i = 0; i < glyphClusterCount; ++i) {
                    details->mIsLastGlyph = i == glyphClusterCount - 1;
                    details->mGlyphID = glyph->glyph;
                    NS_ASSERTION(details->mGlyphID == glyph->glyph,
                                 "Seriously weird glyph ID detected!");
                    if (IS_MISSING_GLYPH(glyph->glyph)) {
                        details->mGlyphID = DetailedGlyph::DETAILED_MISSING_GLYPH;
                    }
                    details->mAdvance = float(glyph->geometry.width)/PANGO_SCALE;
                    details->mXOffset = float(glyph->geometry.x_offset)/PANGO_SCALE;
                    details->mYOffset = float(glyph->geometry.y_offset)/PANGO_SCALE;
                    glyph += direction;
                    ++details;
                }
            }
        }

        ++utf16Offset;
        if (utf16Offset < mCharacterCount &&
            mCharacterGlyphs[utf16Offset].IsLowSurrogate()) {
            ++utf16Offset;
        }
        // We produced this UTF8 so we don't need to worry about malformed stuff
        index = g_utf8_next_char(aUTF8 + index) - aUTF8;
    }

    *aUTF16Offset = utf16Offset;
    return NS_OK;
}

#if defined(ENABLE_XFT_FAST_PATH_8BIT) || defined(ENABLE_XFT_FAST_PATH_ALWAYS)
void
gfxPangoTextRun::CreateGlyphRunsXft(const gchar *aUTF8, PRUint32 aUTF8Length)
{
    const gchar *p = aUTF8;
    Display *dpy = GDK_DISPLAY();
    gfxPangoFont *font = mFontGroup->GetFontAt(0);
    XftFont *xfont = font->GetXftFont();
    PRUint32 utf16Offset = 0;

    while (p < aUTF8 + aUTF8Length) {
        gunichar ch = g_utf8_get_char(p);
        p = g_utf8_next_char(p);
        
        if (ch == 0) {
            // treat this null byte like a missing glyph with no advance
            DetailedGlyph *details = AllocateDetailedGlyphs(utf16Offset, 1);
            if (!details)
                return;
            SetSkipMissingGlyph(details);
        } else {
            FT_UInt glyph = XftCharIndex(dpy, xfont, ch);
            XGlyphInfo info;
            XftGlyphExtents(dpy, xfont, &glyph, 1, &info);
            if (info.yOff > 0) {
                NS_WARNING("vertical offsets not supported");
            }
            
            if (info.xOff >= 0 &&
                CompressedGlyph::IsSimpleAdvance(info.xOff) &&
                CompressedGlyph::IsSimpleGlyphID(glyph)) {
                mCharacterGlyphs[utf16Offset].SetSimpleGlyph(info.xOff, glyph);
            } else {
                // Note that missing-glyph IDs are not simple glyph IDs, so we'll
                // always get here when a glyph is missing
                DetailedGlyph *details = AllocateDetailedGlyphs(utf16Offset, 1);
                if (!details)
                    return;
                details->mIsLastGlyph = PR_TRUE;
                details->mGlyphID = glyph;
                NS_ASSERTION(details->mGlyphID == glyph,
                             "Seriously weird glyph ID detected!");
                if (IS_MISSING_GLYPH(glyph)) {
                    details->mGlyphID = DetailedGlyph::DETAILED_MISSING_GLYPH;
                }
                details->mAdvance = float(info.xOff);
                details->mXOffset = 0;
                details->mYOffset = 0;
            }
        }

        ++utf16Offset;
        if (utf16Offset < mCharacterCount &&
            mCharacterGlyphs[utf16Offset].IsLowSurrogate()) {
            ++utf16Offset;
        }
    }
    AddGlyphRun(font->GetPangoFont(), 0);    
}
#endif

nsresult
gfxPangoTextRun::CreateGlyphRunsFast(const gchar *aUTF8, PRUint32 aUTF8Length,
                                     const PRUnichar *aUTF16, PRUint32 aUTF16Length)
{
    gfxPangoFont *font = mFontGroup->GetFontAt(0);
    PangoAnalysis analysis;
    analysis.font        = font->GetPangoFont();
    analysis.level       = IsRightToLeft() ? 1 : 0;
    analysis.lang_engine = nsnull;
    analysis.extra_attrs = nsnull;

    // If we're passed an empty string for some reason, ensure that at least
    // a single glyphrun object is created, because DrawSpecialString assumes
    // there is one. (So does FindFirstGlyphRunContaining, but that shouldn't
    // be called on an empty string.)
    if (!aUTF8Length)
      return AddGlyphRun(analysis.font, 0);

    // Find non-ASCII character for finding the language of the script.
    guint32 ch = 'a';
    PRUint8 unicharRange = kRangeSetLatin;
    if (aUTF16) {
        PRUint32 i;
        for (i = 0; i < aUTF16Length; ++i) {
            PRUnichar utf16Char = aUTF16[i];
            if (utf16Char > 0x100) {
                ch = utf16Char;
                unicharRange = FindCharUnicodeRange(utf16Char);
                break;
            }
        }
    }

    // Determin the language for finding the shaper.
    nsCAutoString lang;
    font->GetMozLang(lang);
    switch (unicharRange) {
        case kRangeSetLatin:
            lang.Assign("x-western");
            break;
        case kRangeSetCJK:
            if (GetCJKLangGroupIndex(lang.get()) < 0)
                return NS_ERROR_FAILURE; // try with itemizing
            break;
        default:
            lang.Assign(LangGroupFromUnicodeRange(unicharRange));
            break;
    }

    if (lang.IsEmpty() || lang.Equals("x-unicode") || lang.Equals("x-user-def"))
        return NS_ERROR_FAILURE; // try with itemizing

    analysis.language     = GetPangoLanguage(lang);
    analysis.shape_engine = pango_font_find_shaper(analysis.font,
                                                   analysis.language,
                                                   ch);

    SetupClusterBoundaries(aUTF8, aUTF8Length, 0, &analysis);

    PangoGlyphString *glyphString = pango_glyph_string_new();

    pango_shape(aUTF8, aUTF8Length, &analysis, glyphString);

    PRUint32 utf16Offset = 0;
    nsresult rv = SetGlyphs(aUTF8, aUTF8Length, &utf16Offset, glyphString, 0, PR_TRUE);

    pango_glyph_string_free(glyphString);

    if (NS_FAILED(rv))
      return rv;

    return AddGlyphRun(analysis.font, 0);
}

class FontSelector
{
public:
    FontSelector(const gchar *aString, PRInt32 aLength,
                 gfxPangoFontGroup *aGroup, gfxPangoTextRun *aTextRun,
                 PangoItem *aItem, PRUint32 aUTF16Offset, PRPackedBool aIsRTL) :
        mItem(aItem),
        mGroup(aGroup), mTextRun(aTextRun), mString(aString),
        mFontIndex(0), mLength(aLength), mSegmentOffset(0), mUTF16Offset(aUTF16Offset),
        mTriedPrefFonts(0), mTriedOtherFonts(0), mIsRTL(aIsRTL)
    {
        for (PRUint32 i = 0; i < mGroup->FontListLength(); ++i)
            mFonts.AppendElement(mGroup->GetFontAt(i));
        mSpaceWidth = NSToIntRound(mGroup->GetFontAt(0)->GetMetrics().spaceWidth * FLOAT_PANGO_SCALE);
    }
    
    void Run()
    {
        InitSegments(0, mLength, mFontIndex);
    }

    PRUint32 GetUTF16Offset() { return mUTF16Offset; }

    static PRBool ExistsFont(FontSelector *aFs,
                             const nsAString &aName) {
        PRUint32 len = aFs->mFonts.Length();
        for (PRUint32 i = 0; i < len; ++i) {
            if (aName.Equals(aFs->mFonts[i]->GetName()))
                return PR_TRUE;
        }
        return PR_FALSE;
    }

    static PRBool AddFontCallback(const nsAString &aName,
                                  const nsACString &aGenericName,
                                  void *closure) {
        if (aName.IsEmpty())
            return PR_TRUE;

        FontSelector *fs = NS_STATIC_CAST(FontSelector*, closure);

        // XXX do something better than this to remove dups
        if (ExistsFont(fs, aName))
            return PR_TRUE;

        nsRefPtr<gfxPangoFont> font = fs->mGroup->GetCachedFont(aName);
        if (!font) {
            font = new gfxPangoFont(aName, fs->mGroup->GetStyle());
            fs->mGroup->PutCachedFont(aName, font);
        }
        fs->mFonts.AppendElement(font);

        return PR_TRUE;
    }

    PRBool AppendNextSegment(gfxPangoFont *aFont, PRUint32 aUTF8Length,
                             PangoGlyphString *aGlyphs, PRBool aGotGlyphs)
    {
        PangoFont *pf = aFont->GetPangoFont();
        PRUint32 incomingUTF16Offset = mUTF16Offset;

        if (!aGotGlyphs) {
            // we can't use the existing glyphstring.
            PangoFont *tmpFont = mItem->analysis.font;
            mItem->analysis.font = pf;
            pango_shape(mString + mSegmentOffset, aUTF8Length, &mItem->analysis, aGlyphs);
            mItem->analysis.font = tmpFont;
        }

        mTextRun->SetGlyphs(mString + mSegmentOffset, aUTF8Length, &mUTF16Offset,
                            aGlyphs, mSpaceWidth, PR_FALSE);

        mSegmentOffset += aUTF8Length;
        return mTextRun->AddGlyphRun(pf, incomingUTF16Offset);
    }

private:
    PangoItem *mItem;

    nsTArray< nsRefPtr<gfxPangoFont> > mFonts;

    gfxPangoFontGroup *mGroup;
    gfxPangoTextRun   *mTextRun;
    const char        *mString; // UTF-8
    PRUint32           mFontIndex;
    PRInt32            mLength;
    PRUint32           mSegmentOffset;
    PRUint32           mUTF16Offset;
    PRUint32           mSpaceWidth;

    PRPackedBool mTriedPrefFonts;
    PRPackedBool mTriedOtherFonts;
    PRPackedBool mIsRTL;

    void InitSegments(const PRUint32 aOffset,
                      const PRUint32 aLength,
                      const PRUint32 aFontIndex) {
        mFontIndex = aFontIndex;

        const char *current = mString + aOffset;
        PRBool checkMissingGlyph = PR_TRUE;

        // for RTL, if we cannot find the font that has all glyphs,
        // we should use better font.
        PRUint32 betterFontIndex  = 0;
        PRUint32 foundGlyphs      = 0;

        PangoGlyphString *glyphString = pango_glyph_string_new();

RetryNextFont:
        nsRefPtr<gfxPangoFont> font = GetNextFont();

        // If we cannot found the font that has the current character glyph,
        // we should return default font's missing data.
        if (!font) {
            font = mFonts[betterFontIndex];
            checkMissingGlyph = PR_FALSE;
        }

        // Shaping
        PangoFont *tmpFont = mItem->analysis.font;
        mItem->analysis.font = font->GetPangoFont();
        pango_shape(current, aLength, &mItem->analysis, glyphString);
        mItem->analysis.font = tmpFont;

        gint num_glyphs     = glyphString->num_glyphs;
        gint *clusters      = glyphString->log_clusters;
        PRUint32 offset     = aOffset;
        PRUint32 skipLength = 0;
        if (checkMissingGlyph) {
            for (PRInt32 i = 0; i < num_glyphs; ++i) {
                PangoGlyphInfo *info = &glyphString->glyphs[i];
                if (IS_MISSING_GLYPH(info->glyph)) {
                    // XXX Note that we don't support the segment separation
                    // in RTL text. Because the Arabic characters changes the
                    // glyphs by the position of the context. I think that the
                    // languages of RTL doesn't have *very *many characters, so,
                    // the Arabic/Hebrew font may have all glyphs in a font.
                    if (mIsRTL) {
                        PRUint32 found = i;
                        for (PRInt32 j = i; j < num_glyphs; ++j) {
                            info = &glyphString->glyphs[j];
                            if (!IS_MISSING_GLYPH(info->glyph))
                                found++;
                        }
                        if (found > foundGlyphs) {
                            // we find better font!
                            foundGlyphs = found;
                            betterFontIndex = mFontIndex - 1;
                        }
                        goto RetryNextFont;
                    }

                    // The glyph is missing, separate segment here.
                    PRUint32 missingLength = aLength - clusters[i];
                    PRInt32 j;
                    for (j = i + 1; j < num_glyphs; ++j) {
                        info = &glyphString->glyphs[j];
                        if (!IS_MISSING_GLYPH(info->glyph)) {
                            missingLength = aOffset + clusters[j] - offset;
                            break;
                        }
                    }

                    if (i != 0) {
                        // found glyphs
                        AppendNextSegment(font, offset - (aOffset + skipLength),
                                          glyphString, PR_FALSE);
                    }

                    // missing glyphs
                    PRUint32 fontIndex = mFontIndex;
                    InitSegments(offset, missingLength, mFontIndex);
                    mFontIndex = fontIndex;

                    PRUint32 next = offset + missingLength;
                    if (next >= aLength) {
                        pango_glyph_string_free(glyphString);
                        return;
                    }

                    // remains, continue this loop
                    i = j;
                    skipLength = next - aOffset;
                }
                if (i + 1 < num_glyphs)
                    offset = aOffset + clusters[i + 1];
                else
                    offset = aOffset + aLength;
            }
        } else {
            offset = aOffset + aLength;
        }

        AppendNextSegment(font, aLength - skipLength, glyphString, skipLength == 0);
        pango_glyph_string_free(glyphString);
    }

    gfxPangoFont *GetNextFont() {
TRY_AGAIN_HOPE_FOR_THE_BEST_2:
        if (mFontIndex < mFonts.Length()) {
            return mFonts[mFontIndex++];
        } else if (!mTriedPrefFonts) {
            mTriedPrefFonts = PR_TRUE;
            nsCAutoString mozLang;
            GetMozLanguage(mItem->analysis.language, mozLang);
            if (!mozLang.IsEmpty()) {
                PRInt32 index = GetCJKLangGroupIndex(mozLang.get());
                if (index >= 0)
                    AppendCJKPrefFonts();
                else
                    AppendPrefFonts(mozLang.get());
            } else {
                NS_ConvertUTF8toUTF16 str(mString);
                PRBool appenedCJKFonts = PR_FALSE;
                for (PRUint32 i = 0; i < str.Length(); ++i) {
                    const PRUnichar ch = str[i];
                    PRUint32 unicodeRange = FindCharUnicodeRange(ch);

                    /* special case CJK */
                    if (unicodeRange == kRangeSetCJK) {
                        if (!appenedCJKFonts) {
                            appenedCJKFonts = PR_TRUE;
                            AppendCJKPrefFonts();
                        }
                    } else {
                        const char *langGroup =
                            LangGroupFromUnicodeRange(unicodeRange);
                        if (langGroup)
                            AppendPrefFonts(langGroup);
                    }
                }
            }
            goto TRY_AGAIN_HOPE_FOR_THE_BEST_2;
        } else if (!mTriedOtherFonts) {
            mTriedOtherFonts = PR_TRUE;
            // XXX we should try by all system fonts
            goto TRY_AGAIN_HOPE_FOR_THE_BEST_2;
        }
        return nsnull;
    }

    void AppendPrefFonts(const char *aLangGroup) {
        NS_ASSERTION(aLangGroup, "aLangGroup is null");
        gfxPlatform *platform = gfxPlatform::GetPlatform();
        nsString fonts;
        platform->GetPrefFonts(aLangGroup, fonts);
        if (fonts.IsEmpty())
            return;
        gfxFontGroup::ForEachFont(fonts, nsDependentCString(aLangGroup),
                                  FontSelector::AddFontCallback, this);
        return;
   }

   void AppendCJKPrefFonts() {
       nsCOMPtr<nsIPrefService> prefs =
           do_GetService(NS_PREFSERVICE_CONTRACTID);
       if (!prefs)
           return;

       nsCOMPtr<nsIPrefBranch> prefBranch;
       prefs->GetBranch(0, getter_AddRefs(prefBranch));
       if (!prefBranch)
           return;

       // Add by the order of accept languages.
       nsXPIDLCString list;
       nsresult rv = prefBranch->GetCharPref("intl.accept_languages",
                                             getter_Copies(list));
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
               PRInt32 index = GetCJKLangGroupIndex(lang.get());
               if (index >= 0)
                   AppendPrefFonts(sCJKLangGroup[index]);
               p++;
           }
       }

       // XXX I think that we should append system locale here if it is CJK.

       // last resort...
       AppendPrefFonts(CJK_LANG_JA);
       AppendPrefFonts(CJK_LANG_KO);
       AppendPrefFonts(CJK_LANG_ZH_CN);
       AppendPrefFonts(CJK_LANG_ZH_HK);
       AppendPrefFonts(CJK_LANG_ZH_TW);
    }
};

void 
gfxPangoTextRun::CreateGlyphRunsItemizing(const gchar *aUTF8, PRUint32 aUTF8Length,
                                          PRUint32 aUTF8HeaderLen)
{
    GList *items = pango_itemize(mFontGroup->GetFontAt(0)->GetPangoContext(), aUTF8, 0,
                                 aUTF8Length, nsnull, nsnull);
    
    PRUint32 utf16Offset = 0;
    PRBool isRTL = IsRightToLeft();
    for (; items && items->data; items = items->next) {
        PangoItem *item = (PangoItem *)items->data;
        NS_ASSERTION(isRTL == item->analysis.level % 2, "RTL assumption mismatch");

        PRUint32 offset = item->offset;
        PRUint32 length = item->length;
        if (offset < aUTF8HeaderLen) {
          if (offset + length <= aUTF8HeaderLen)
            continue;
          length -= aUTF8HeaderLen - offset;
          offset = aUTF8HeaderLen;
        }
        
        SetupClusterBoundaries(aUTF8 + offset, length, utf16Offset, &item->analysis);
        FontSelector fs(aUTF8 + offset, length, mFontGroup, this, item, utf16Offset, isRTL);
        fs.Run(); // appends GlyphRuns
        utf16Offset = fs.GetUTF16Offset();
    }

    NS_ASSERTION(utf16Offset == mCharacterCount,
                 "Didn't resolve all characters");
  
    if (items)
        g_list_free(items);
}

/**
 ** language group helpers
 **/

struct MozPangoLangGroup {
    const char *mozLangGroup;
    const char *PangoLang;
};

static const MozPangoLangGroup MozPangoLangGroups[] = {
    { "x-western",      "en"    },
    { "x-central-euro", "pl"    },
    { "ja",             "ja"    },
    { "zh-TW",          "zh-tw" },
    { "zh-CN",          "zh-cn" },
    { "zh-HK",          "zh-hk" },
    { "ko",             "ko"    },
    { "x-cyrillic",     "ru"    },
    { "x-baltic",       "lv"    },
    { "el",             "el"    },
    { "tr",             "tr"    },
    { "th",             "th"    },
    { "he",             "he"    },
    { "ar",             "ar"    },
    { "x-devanagari",   "hi"    },
    { "x-tamil",        "ta"    },
    { "x-armn",         "ar"    },
    { "x-beng",         "bn"    },
    { "x-ethi",         "et"    },
    { "x-geor",         "ka"    },
    { "x-gujr",         "gu"    },
    { "x-guru",         "pa"    },
    { "x-khmr",         "km"    },
    { "x-mlym",         "ml"    },
    { "x-cans",         "iu"    },
    { "x-unicode",      0       },
    { "x-user-def",     0       },
};

#define NUM_PANGO_LANG_GROUPS (sizeof (MozPangoLangGroups) / \
                               sizeof (MozPangoLangGroups[0]))

/* static */
PangoLanguage *
GetPangoLanguage(const nsACString& cname)
{
    // see if the lang group needs to be translated from mozilla's
    // internal mapping into fontconfig's
    const struct MozPangoLangGroup *langGroup = nsnull;

    for (unsigned int i=0; i < NUM_PANGO_LANG_GROUPS; ++i) {
        if (cname.Equals(MozPangoLangGroups[i].mozLangGroup,
                         nsCaseInsensitiveCStringComparator())) {
            langGroup = &MozPangoLangGroups[i];
            break;
        }
    }

    // if there's no lang group, just use the lang group as it was
    // passed to us
    //
    // we're casting away the const here for the strings - should be
    // safe.
    if (!langGroup)
        return pango_language_from_string(nsPromiseFlatCString(cname).get());
    else if (langGroup->PangoLang) 
        return pango_language_from_string(langGroup->PangoLang);

    return pango_language_from_string("en");
}

// See pango-script-lang-table.h in pango.
static const MozPangoLangGroup PangoAllLangGroup[] = {
    { "x-western",      "aa"    },
    { "x-cyrillic",     "ab"    },
    { "x-western",      "af"    },
    { "x-ethi",         "am"    },
    { "ar",             "ar"    },
    { "x-western",      "ast"   },
    { "x-cyrillic",     "ava"   },
    { "x-western",      "ay"    },
    { "x-western",      "az"    },
    { "x-cyrillic",     "ba"    },
    { "x-western",      "bam"   },
    { "x-cyrillic",     "be"    },
    { "x-cyrillic",     "bg"    },
    { "x-devanagari",   "bh"    },
    { "x-devanagari",   "bho"   },
    { "x-western",      "bi"    },
    { "x-western",      "bin"   },
    { "x-beng",         "bn"    },
    { 0,                "bo"    }, // PANGO_SCRIPT_TIBETAN
    { "x-western",      "br"    },
    { "x-western",      "bs"    },
    { "x-cyrillic",     "bua"   },
    { "x-western",      "ca"    },
    { "x-cyrillic",     "ce"    },
    { "x-western",      "ch"    },
    { "x-cyrillic",     "chm"   },
    { 0,                "chr"   }, // PANGO_SCRIPT_CHEROKEE
    { "x-western",      "co"    },
    { "x-central-euro", "cs"    }, // PANGO_SCRIPT_LATIN
    { "x-cyrillic",     "cu"    },
    { "x-cyrillic",     "cv"    },
    { "x-western",      "cy"    },
    { "x-western",      "da"    },
    { "x-central-euro", "de"    }, // PANGO_SCRIPT_LATIN
    { 0,                "dz"    }, // PANGO_SCRIPT_TIBETAN
    { "el",             "el"    },
    { "x-western",      "en"    },
    { "x-western",      "eo"    },
    { "x-western",      "es"    },
    { "x-western",      "et"    },
    { "x-western",      "eu"    },
    { "ar",             "fa"    },
    { "x-western",      "fi"    },
    { "x-western",      "fj"    },
    { "x-western",      "fo"    },
    { "x-western",      "fr"    },
    { "x-western",      "ful"   },
    { "x-western",      "fur"   },
    { "x-western",      "fy"    },
    { "x-western",      "ga"    },
    { "x-western",      "gd"    },
    { "x-ethi",         "gez"   },
    { "x-western",      "gl"    },
    { "x-western",      "gn"    },
    { "x-gujr",         "gu"    },
    { "x-western",      "gv"    },
    { "x-western",      "ha"    },
    { "x-western",      "haw"   },
    { "he",             "he"    },
    { "x-devanagari",   "hi"    },
    { "x-western",      "ho"    },
    { "x-central-euro", "hr"    }, // PANGO_SCRIPT_LATIN
    { "x-western",      "hu"    },
    { "x-armn",         "hy"    },
    { "x-western",      "ia"    },
    { "x-western",      "ibo"   },
    { "x-western",      "id"    },
    { "x-western",      "ie"    },
    { "x-cyrillic",     "ik"    },
    { "x-western",      "io"    },
    { "x-western",      "is"    },
    { "x-western",      "it"    },
    { "x-cans",         "iu"    },
    { "ja",             "ja"    },
    { "x-geor",         "ka"    },
    { "x-cyrillic",     "kaa"   },
    { "x-western",      "ki"    },
    { "x-cyrillic",     "kk"    },
    { "x-western",      "kl"    },
    { "x-khmr",         "km"    },
    { 0,                "kn"    }, // PANGO_SCRIPT_KANNADA
    { "ko",             "ko"    },
    { "x-devanagari",   "kok"   },
    { "x-devanagari",   "ks"    },
    { "x-cyrillic",     "ku"    },
    { "x-cyrillic",     "kum"   },
    { "x-cyrillic",     "kv"    },
    { "x-western",      "kw"    },
    { "x-cyrillic",     "ky"    },
    { "x-western",      "la"    },
    { "x-western",      "lb"    },
    { "x-cyrillic",     "lez"   },
    { 0,                "lo"    }, // PANGO_SCRIPT_LAO
    { "x-western",      "lt"    },
    { "x-western",      "lv"    },
    { "x-western",      "mg"    },
    { "x-western",      "mh"    },
    { "x-western",      "mi"    },
    { "x-cyrillic",     "mk"    },
    { "x-mlym",         "ml"    },
    { 0,                "mn"    }, // PANGO_SCRIPT_MONGOLIAN
    { "x-western",      "mo"    },
    { "x-devanagari",   "mr"    },
    { "x-western",      "mt"    },
    { 0,                "my"    }, // PANGO_SCRIPT_MYANMAR
    { "x-western",      "nb"    },
    { "x-devanagari",   "ne"    },
    { "x-western",      "nl"    },
    { "x-western",      "nn"    },
    { "x-western",      "no"    },
    { "x-western",      "ny"    },
    { "x-western",      "oc"    },
    { "x-western",      "om"    },
    { 0,                "or"    }, // PANGO_SCRIPT_ORIYA
    { "x-cyrillic",     "os"    },
    { "x-central-euro", "pl"    }, // PANGO_SCRIPT_LATIN
    { "x-western",      "pt"    },
    { "x-western",      "rm"    },
    { "x-western",      "ro"    },
    { "x-cyrillic",     "ru"    },
    { "x-devanagari",   "sa"    },
    { "x-cyrillic",     "sah"   },
    { "x-western",      "sco"   },
    { "x-western",      "se"    },
    { "x-cyrillic",     "sel"   },
    { "x-cyrillic",     "sh"    },
    { 0,                "si"    }, // PANGO_SCRIPT_SINHALA
    { "x-central-euro", "sk"    }, // PANGO_SCRIPT_LATIN
    { "x-central-euro", "sl"    }, // PANGO_SCRIPT_LATIN
    { "x-western",      "sm"    },
    { "x-western",      "sma"   },
    { "x-western",      "smj"   },
    { "x-western",      "smn"   },
    { "x-western",      "sms"   },
    { "x-western",      "so"    },
    { "x-western",      "sq"    },
    { "x-cyrillic",     "sr"    },
    { "x-western",      "sv"    },
    { "x-western",      "sw"    },
    { 0,                "syr"   }, // PANGO_SCRIPT_SYRIAC
    { "x-tamil",        "ta"    },
    { 0,                "te"    }, // PANGO_SCRIPT_TELUGU
    { "x-cyrillic",     "tg"    },
    { "th",             "th"    },
    { "x-ethi",         "ti-er" },
    { "x-ethi",         "ti-et" },
    { "x-ethi",         "tig"   },
    { "x-cyrillic",     "tk"    },
    { 0,                "tl"    }, // PANGO_SCRIPT_TAGALOG
    { "x-western",      "tn"    },
    { "x-western",      "to"    },
    { "x-western",      "tr"    },
    { "x-western",      "ts"    },
    { "x-cyrillic",     "tt"    },
    { "x-western",      "tw"    },
    { "x-cyrillic",     "tyv"   },
    { "ar",             "ug"    },
    { "x-cyrillic",     "uk"    },
    { "ar",             "ur"    },
    { "x-cyrillic",     "uz"    },
    { "x-western",      "ven"   },
    { "x-western",      "vi"    },
    { "x-western",      "vo"    },
    { "x-western",      "vot"   },
    { "x-western",      "wa"    },
    { "x-western",      "wen"   },
    { "x-western",      "wo"    },
    { "x-western",      "xh"    },
    { "x-western",      "yap"   },
    { "he",             "yi"    },
    { "x-western",      "yo"    },
    { "zh-CN",          "zh-cn" },
    { "zh-HK",          "zh-hk" },
    { "zh-HK",          "zh-mo" },
    { "zh-CN",          "zh-sg" },
    { "zh-TW",          "zh-tw" },
    { "x-western",      "zu"    },
};

#define NUM_PANGO_ALL_LANG_GROUPS (sizeof (PangoAllLangGroup) / \
                                   sizeof (PangoAllLangGroup[0]))

/* static */
void
GetMozLanguage(const PangoLanguage *aLang, nsACString &aMozLang)
{
    aMozLang.Truncate();
    if (!aLang)
        return;

    nsCAutoString lang(pango_language_to_string(aLang));
    if (lang.IsEmpty() || lang.Equals("xx"))
        return;

    while (1) {
        for (PRUint32 i = 0; i < NUM_PANGO_ALL_LANG_GROUPS; ++i) {
            if (lang.Equals(PangoAllLangGroup[i].PangoLang)) {
                if (PangoAllLangGroup[i].mozLangGroup)
                    aMozLang.Assign(PangoAllLangGroup[i].mozLangGroup);
                return;
            }
        }

        PRInt32 hyphen = lang.FindChar('-');
        if (hyphen != kNotFound) {
            lang.Cut(hyphen, lang.Length());
            continue;
        }
        break;
    }
}

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
#include "nsMathUtils.h"

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

    lib = nsnull;
    int *xft_max_freetype_files_ptr = nsnull;
    xft_max_freetype_files_ptr = (int*) PR_FindSymbolAndLibrary("XftMaxFreeTypeFiles", &lib);
    if (xft_max_freetype_files_ptr && *xft_max_freetype_files_ptr < 50)
        *xft_max_freetype_files_ptr = 50;
    if (lib)
        PR_UnloadLibrary(lib);
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
    mXftFont(nsnull), mCairoFont(nsnull), mHasMetrics(PR_FALSE),
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

    if (mCairoFont)
        cairo_scaled_font_destroy(mCairoFont);
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
        PR_MAX(NS_round(mStyle->size*(mStyle->sizeAdjust/aspect)), 1.0);
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
 ** gfxTextRun
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

gfxTextRun *
gfxPangoFontGroup::MakeTextRun(const PRUint8 *aString, PRUint32 aLength,
                               Parameters *aParams)
{
    aParams->mFlags |= TEXT_IS_8BIT;
    gfxTextRun *run = new gfxTextRun(aParams, aLength);
    if (!run)
        return nsnull;

    const gchar *utf8Chars = NS_REINTERPRET_CAST(const gchar*, aString);

    PRBool isRTL = run->IsRightToLeft();
    if (!isRTL) {
        // We don't need to send an override character here, the characters must be all
        // LTR
        InitTextRun(run, utf8Chars, aLength, 0, nsnull, 0);
    } else {
        // XXX this could be more efficient
        NS_ConvertASCIItoUTF16 unicodeString(utf8Chars, aLength);
        nsCAutoString utf8;
        PRInt32 headerLen = AppendDirectionalIndicatorUTF8(isRTL, utf8);
        AppendUTF16toUTF8(unicodeString, utf8);
        InitTextRun(run, utf8.get(), utf8.Length(), headerLen, nsnull, 0);
    }
    return run;
}

gfxTextRun *
gfxPangoFontGroup::MakeTextRun(const PRUnichar *aString, PRUint32 aLength,
                               Parameters *aParams)
{
    gfxTextRun *run = new gfxTextRun(aParams, aLength);
    if (!run)
        return nsnull;

    run->RecordSurrogates(aString);

    nsCAutoString utf8;
    PRInt32 headerLen = AppendDirectionalIndicatorUTF8(run->IsRightToLeft(), utf8);
    AppendUTF16toUTF8(Substring(aString, aString + aLength), utf8);
    InitTextRun(run, utf8.get(), utf8.Length(), headerLen, aString, aLength);
    return run;
}

void
gfxPangoFontGroup::InitTextRun(gfxTextRun *aTextRun, const gchar *aUTF8Text,
                               PRUint32 aUTF8Length, PRUint32 aUTF8HeaderLength,
                               const PRUnichar *aUTF16Text, PRUint32 aUTF16Length)
{
#if defined(ENABLE_XFT_FAST_PATH_ALWAYS)
    CreateGlyphRunsXft(aTextRun, aUTF8Text + aUTF8HeaderLength, aUTF8Length - aUTF8HeaderLength);
#else
#if defined(ENABLE_XFT_FAST_PATH_8BIT)
    if (aTextRun->GetFlags() & gfxTextRunFactory::TEXT_IS_8BIT) {
        CreateGlyphRunsXft(aTextRun, aUTF8Text + aUTF8HeaderLength, aUTF8Length - aUTF8HeaderLength);
        return;
    }
#endif

    pango_context_set_base_dir(GetFontAt(0)->GetPangoContext(),
                               (aTextRun->IsRightToLeft()
                                  ? PANGO_DIRECTION_RTL : PANGO_DIRECTION_LTR));

    nsresult rv = CreateGlyphRunsFast(aTextRun, aUTF8Text + aUTF8HeaderLength,
                                      aUTF8Length - aUTF8HeaderLength, aUTF16Text, aUTF16Length);
    if (rv == NS_ERROR_FAILURE) {
        CreateGlyphRunsItemizing(aTextRun, aUTF8Text, aUTF8Length, aUTF8HeaderLength);
    }
#endif
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

gfxTextRun::Metrics
gfxPangoFont::Measure(gfxTextRun *aTextRun,
                      PRUint32 aStart, PRUint32 aEnd,
                      PRBool aTightBoundingBox,
                      gfxTextRun::PropertyProvider::Spacing *aSpacing)
{
    if (aEnd <= aStart)
        return gfxTextRun::Metrics();

    const gfxTextRun::CompressedGlyph *charGlyphs = aTextRun->GetCharacterGlyphs();
    nsAutoTArray<PangoGlyphInfo,200> glyphBuffer;
    gfxFloat appUnitsToPango = gfxFloat(PANGO_SCALE)/aTextRun->GetAppUnitsPerDevUnit();

    // We start by assuming every character is a cluster and subtract off
    // characters where that's not true
    PRUint32 clusterCount = aEnd - aStart;
    PRBool isRTL = aTextRun->IsRightToLeft();

    PRUint32 i;
    for (i = aStart; i < aEnd; ++i) {
        PRUint32 leftSpacePango = 0;
        PRUint32 rightSpacePango = 0;
        if (aSpacing) {
            gfxTextRun::PropertyProvider::Spacing *space = &aSpacing[i];
            leftSpacePango =
                NS_lround((isRTL ? space->mAfter : space->mBefore)*appUnitsToPango);
            rightSpacePango =
                NS_lround((isRTL ? space->mBefore : space->mAfter)*appUnitsToPango);
        }
        const gfxTextRun::CompressedGlyph *glyphData = &charGlyphs[i];
        if (glyphData->IsSimpleGlyph()) {
            PangoGlyphInfo *glyphInfo = glyphBuffer.AppendElement();
            if (!glyphInfo)
                return gfxTextRun::Metrics();
            glyphInfo->attr.is_cluster_start = 0;
            glyphInfo->glyph = glyphData->GetSimpleGlyph();
            glyphInfo->geometry.width = leftSpacePango + rightSpacePango +
                glyphData->GetSimpleAdvance()*PANGO_SCALE;
            glyphInfo->geometry.x_offset = leftSpacePango;
            glyphInfo->geometry.y_offset = 0;
        } else if (glyphData->IsComplexCluster()) {
            const gfxTextRun::DetailedGlyph *details = aTextRun->GetDetailedGlyphs(i);
            PRBool firstGlyph = PR_FALSE;
            for (;;) {
                PangoGlyphInfo *glyphInfo = glyphBuffer.AppendElement();
                if (!glyphInfo)
                    return gfxTextRun::Metrics();
                glyphInfo->attr.is_cluster_start = 0;
                glyphInfo->glyph = details->mGlyphID;
                glyphInfo->geometry.width = NS_lround(details->mAdvance*PANGO_SCALE);
                glyphInfo->geometry.x_offset = NS_lround(details->mXOffset*PANGO_SCALE);
                if (firstGlyph) {
                    glyphInfo->geometry.width += leftSpacePango;
                    glyphInfo->geometry.x_offset += leftSpacePango;
                    firstGlyph = PR_FALSE;
                }
                glyphInfo->geometry.y_offset = NS_lround(details->mYOffset*PANGO_SCALE);
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

    if (aTextRun->IsRightToLeft()) {
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
    return GetPangoMetrics(&glyphs, GetPangoFont(), aTextRun->GetAppUnitsPerDevUnit(), clusterCount);
}

#define IS_MISSING_GLYPH(g) (((g) & 0x10000000) || (g) == 0x0FFFFFFF)

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

void
gfxPangoFont::SetupCairoFont(cairo_t *aCR)
{
    cairo_matrix_t currentCTM;
    cairo_get_matrix(aCR, &currentCTM);

    if (mCairoFont) {
        // Need to validate that its CTM is OK
        cairo_matrix_t fontCTM;
        cairo_scaled_font_get_ctm(mCairoFont, &fontCTM);
        if (fontCTM.xx == currentCTM.xx && fontCTM.yy == currentCTM.yy &&
            fontCTM.xy == currentCTM.xy && fontCTM.yx == currentCTM.yx) {
            cairo_set_scaled_font(aCR, mCairoFont);
            return;
        }

        // Just recreate it from scratch, simplest way
        cairo_scaled_font_destroy(mCairoFont);
    }

    mCairoFont = CreateScaledFont(aCR, &currentCTM, GetPangoFont());
    cairo_set_scaled_font(aCR, mCairoFont);
}

static void
SetupClusterBoundaries(gfxTextRun* aTextRun, const gchar *aUTF8, PRUint32 aUTF8Length,
                       PRUint32 aUTF16Offset, PangoAnalysis *aAnalysis)
{
    if (aTextRun->GetFlags() & gfxTextRunFactory::TEXT_IS_8BIT) {
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

    const gchar *p = aUTF8;
    const gchar *end = aUTF8 + aUTF8Length;
    gfxTextRun::CompressedGlyph g;
    while (p < end) {
        if (!buffer[p - aUTF8].is_cursor_position) {
            aTextRun->SetCharacterGlyph(aUTF16Offset, g.SetClusterContinuation());
        }
        ++aUTF16Offset;
        
        gunichar ch = g_utf8_get_char(p);
        NS_ASSERTION(!IS_SURROGATE(ch), "Shouldn't have surrogates in UTF8");
        if (ch >= 0x10000) {
            ++aUTF16Offset;
        }
        // We produced this utf8 so we don't need to worry about malformed stuff
        p = g_utf8_next_char(p);
    }
}

nsresult
gfxPangoFontGroup::SetGlyphs(gfxTextRun* aTextRun,
                             const gchar *aUTF8, PRUint32 aUTF8Length,
                             PRUint32 *aUTF16Offset, PangoGlyphString *aGlyphs,
                             PangoGlyphUnit aOverrideSpaceWidth,
                             PRBool aAbortOnMissingGlyph)
{
    PRUint32 utf16Offset = *aUTF16Offset;
    PRUint32 textRunLength = aTextRun->GetLength();
    PRUint32 index = 0;
    // glyphIndex is the first glyph that belongs to characters at "index"
    // or later
    PRUint32 numGlyphs = aGlyphs->num_glyphs;
    gint *logClusters = aGlyphs->log_clusters;
    PRUint32 glyphCount = 0;
    PRUint32 glyphIndex = aTextRun->IsRightToLeft() ? numGlyphs - 1 : 0;
    PRInt32 direction = aTextRun->IsRightToLeft() ? -1 : 1;
    gfxTextRun::CompressedGlyph g;
    nsAutoTArray<gfxTextRun::DetailedGlyph,1> detailedGlyphs;

    while (index < aUTF8Length) {
        if (aUTF8[index] == 0) {
            // treat this null byte as a missing glyph
            aTextRun->SetCharacterGlyph(utf16Offset, g.SetMissing());
        } else if (glyphCount == numGlyphs ||
                   PRUint32(logClusters[glyphIndex]) > index) {
            // No glyphs for this cluster, and it's not a null byte.
            if (!aTextRun->GetCharacterGlyphs()[utf16Offset].IsClusterContinuation()) {
                // It must be part of a ligature.
                aTextRun->SetCharacterGlyph(utf16Offset, g.SetLigatureContinuation());
            }
        } else {
            PangoGlyphInfo *glyph = &aGlyphs->glyphs[glyphIndex];
            PRBool haveMissingGlyph = PR_FALSE;

            // One or more glyphs. See if we fit in the compressed area.
            NS_ASSERTION(PRUint32(logClusters[glyphIndex]) == index,
                         "Um, we left some glyphs behind previously");
            PRUint32 glyphClusterCount = 1;
            for (;;) {
                ++glyphCount;
                if (IS_MISSING_GLYPH(aGlyphs->glyphs[glyphIndex].glyph)) {
                    haveMissingGlyph = PR_TRUE;
                }
                glyphIndex += direction;
                if (glyphCount == numGlyphs ||
                    PRUint32(logClusters[glyphIndex]) != index)
                    break;
                ++glyphClusterCount;
            }
            if (haveMissingGlyph && aAbortOnMissingGlyph)
                return NS_ERROR_FAILURE;

            PangoGlyphUnit width = glyph->geometry.width;

            // Override the width of a space, but only for spaces that aren't
            // clustered with something else (like a freestanding diacritical mark)
            if (aOverrideSpaceWidth && aUTF8[index] == ' ' &&
                (utf16Offset + 1 == textRunLength ||
                 aTextRun->GetCharacterGlyphs()[utf16Offset].IsClusterStart())) {
                width = aOverrideSpaceWidth;
            }

            PRInt32 downscaledWidth = width/PANGO_SCALE;
            if (glyphClusterCount == 1 &&
                glyph->geometry.x_offset == 0 && glyph->geometry.y_offset == 0 &&
                width >= 0 && downscaledWidth*PANGO_SCALE == width &&
                gfxTextRun::CompressedGlyph::IsSimpleAdvancePixels(downscaledWidth) &&
                gfxTextRun::CompressedGlyph::IsSimpleGlyphID(glyph->glyph)) {
                aTextRun->SetCharacterGlyph(utf16Offset, g.SetSimpleGlyph(downscaledWidth, glyph->glyph));
            } else if (haveMissingGlyph) {
                // Note that missing-glyph IDs are not simple glyph IDs, so we'll
                // always get here when a glyph is missing
                aTextRun->SetCharacterGlyph(utf16Offset, g.SetMissing());
            } else {
                if (detailedGlyphs.Length() < glyphClusterCount) {
                    if (!detailedGlyphs.AppendElements(glyphClusterCount - detailedGlyphs.Length()))
                        return NS_ERROR_OUT_OF_MEMORY;
                }
                PRUint32 i;
                for (i = 0; i < glyphClusterCount; ++i) {
                    gfxTextRun::DetailedGlyph *details = &detailedGlyphs[i];
                    details->mIsLastGlyph = i == glyphClusterCount - 1;
                    details->mGlyphID = glyph->glyph;
                    NS_ASSERTION(details->mGlyphID == glyph->glyph,
                                 "Seriously weird glyph ID detected!");
                    details->mAdvance = float(glyph->geometry.width)/PANGO_SCALE;
                    details->mXOffset = float(glyph->geometry.x_offset)/PANGO_SCALE;
                    details->mYOffset = float(glyph->geometry.y_offset)/PANGO_SCALE;
                    glyph += direction;
                }
                aTextRun->SetDetailedGlyphs(utf16Offset, detailedGlyphs.Elements(), glyphClusterCount);
            }
        }

        gunichar ch = g_utf8_get_char(aUTF8 + index);
        ++utf16Offset;
        NS_ASSERTION(!IS_SURROGATE(ch), "surrogates should not appear in UTF8");
        if (ch >= 0x10000) {
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
gfxPangoFontGroup::CreateGlyphRunsXft(gfxTextRun *aTextRun,
                                      const gchar *aUTF8, PRUint32 aUTF8Length)
{
    const gchar *p = aUTF8;
    Display *dpy = GDK_DISPLAY();
    gfxPangoFont *font = GetFontAt(0);
    XftFont *xfont = font->GetXftFont();
    PRUint32 utf16Offset = 0;
    gfxTextRun::CompressedGlyph g;

    while (p < aUTF8 + aUTF8Length) {
        gunichar ch = g_utf8_get_char(p);
        p = g_utf8_next_char(p);
        
        if (ch == 0) {
            // treat this null byte as a missing glyph
            aTextRun->SetCharacterGlyph(utf16Offset, g.SetMissing());
        } else {
            FT_UInt glyph = XftCharIndex(dpy, xfont, ch);
            XGlyphInfo info;
            XftGlyphExtents(dpy, xfont, &glyph, 1, &info);
            if (info.yOff > 0) {
                NS_WARNING("vertical offsets not supported");
            }
            
            if (info.xOff >= 0 &&
                gfxTextRun::CompressedGlyph::IsSimpleAdvancePixels(info.xOff) &&
                gfxTextRun::CompressedGlyph::IsSimpleGlyphID(glyph)) {
                aTextRun->SetCharacterGlyph(utf16Offset,
                                            g.SetSimpleGlyph(info.xOff, glyph));
            } else if (IS_MISSING_GLYPH(glyph)) {
                // Note that missing-glyph IDs are not simple glyph IDs, so we'll
                // always get here when a glyph is missing
                aTextRun->SetCharacterGlyph(utf16Offset, g.SetMissing());
            } else {
                gfxTextRun::DetailedGlyph details;
                details.mIsLastGlyph = PR_TRUE;
                details.mGlyphID = glyph;
                NS_ASSERTION(details.mGlyphID == glyph,
                             "Seriously weird glyph ID detected!");
                details.mAdvance = float(info.xOff);
                details.mXOffset = 0;
                details.mYOffset = 0;
                aTextRun->SetDetailedGlyphs(utf16Offset, &details, 1);
            }
            
            NS_ASSERTION(!IS_SURROGATE(ch), "Surrogates shouldn't appear in UTF8");
            if (ch >= 0x10000) {
                // This character is a surrogate pair in UTF16
                ++utf16Offset;
            }
        }

        ++utf16Offset;
    }
    aTextRun->AddGlyphRun(font, 0);    
}
#endif

nsresult
gfxPangoFontGroup::CreateGlyphRunsFast(gfxTextRun *aTextRun,
                                       const gchar *aUTF8, PRUint32 aUTF8Length,
                                       const PRUnichar *aUTF16, PRUint32 aUTF16Length)
{
    gfxPangoFont *font = GetFontAt(0);
    PangoAnalysis analysis;
    analysis.font        = font->GetPangoFont();
    analysis.level       = aTextRun->IsRightToLeft() ? 1 : 0;
    analysis.lang_engine = nsnull;
    analysis.extra_attrs = nsnull;

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

    SetupClusterBoundaries(aTextRun, aUTF8, aUTF8Length, 0, &analysis);

    PangoGlyphString *glyphString = pango_glyph_string_new();

    pango_shape(aUTF8, aUTF8Length, &analysis, glyphString);

    PRUint32 utf16Offset = 0;
    nsresult rv = SetGlyphs(aTextRun, aUTF8, aUTF8Length, &utf16Offset, glyphString, 0, PR_TRUE);

    pango_glyph_string_free(glyphString);

    if (NS_FAILED(rv))
      return rv;

    return aTextRun->AddGlyphRun(font, 0);
}

class FontSelector
{
public:
    FontSelector(const gchar *aString, PRInt32 aLength,
                 gfxPangoFontGroup *aGroup, gfxTextRun *aTextRun,
                 PangoItem *aItem, PRUint32 aUTF16Offset, PRPackedBool aIsRTL) :
        mItem(aItem),
        mGroup(aGroup), mTextRun(aTextRun), mString(aString),
        mFontIndex(0), mLength(aLength), mSegmentOffset(0), mUTF16Offset(aUTF16Offset),
        mTriedPrefFonts(0), mTriedOtherFonts(0), mIsRTL(aIsRTL)
    {
        for (PRUint32 i = 0; i < mGroup->FontListLength(); ++i)
            mFonts.AppendElement(mGroup->GetFontAt(i));
        mSpaceWidth = NS_lround(mGroup->GetFontAt(0)->GetMetrics().spaceWidth * FLOAT_PANGO_SCALE);
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

        mGroup->SetGlyphs(mTextRun, mString + mSegmentOffset, aUTF8Length, &mUTF16Offset,
                          aGlyphs, mSpaceWidth, PR_FALSE);

        mSegmentOffset += aUTF8Length;
        return mTextRun->AddGlyphRun(aFont, incomingUTF16Offset);
    }

private:
    PangoItem *mItem;

    nsTArray< nsRefPtr<gfxPangoFont> > mFonts;

    gfxPangoFontGroup *mGroup;
    gfxTextRun   *mTextRun;
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
gfxPangoFontGroup::CreateGlyphRunsItemizing(gfxTextRun *aTextRun,
                                            const gchar *aUTF8, PRUint32 aUTF8Length,
                                            PRUint32 aUTF8HeaderLen)
{
    GList *items = pango_itemize(GetFontAt(0)->GetPangoContext(), aUTF8, 0,
                                 aUTF8Length, nsnull, nsnull);
    
    PRUint32 utf16Offset = 0;
    PRBool isRTL = aTextRun->IsRightToLeft();
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
        
        SetupClusterBoundaries(aTextRun, aUTF8 + offset, length, utf16Offset, &item->analysis);
        FontSelector fs(aUTF8 + offset, length, this, aTextRun, item, utf16Offset, isRTL);
        fs.Run(); // appends GlyphRuns
        utf16Offset = fs.GetUTF16Offset();
    }

    NS_ASSERTION(utf16Offset == aTextRun->GetLength(),
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

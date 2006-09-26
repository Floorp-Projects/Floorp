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

#define PANGO_ENABLE_BACKEND
#define PANGO_ENABLE_ENGINE

//#define DISABLE_PANGO_FAST

#ifdef XP_BEOS
#define THEBES_USE_PANGO_CAIRO
#endif

#include "prtypes.h"
#include "prlink.h"
#include "gfxTypes.h"

#include "nsUnicodeRange.h"

#include "nsIPref.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsServiceManagerUtils.h"

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
                                 const nsACString& aLangGroup,
                                 void *closure)
{
    nsStringArray *sa = NS_STATIC_CAST(nsStringArray*, closure);

    if (FFRECountHyphens(fontName) < 3) {
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

    for (int i = 0; i < familyArray.Count(); i++)
        mFonts.AppendElement(new gfxPangoFont(*familyArray[i], &mStyle));
}

gfxPangoFontGroup::~gfxPangoFontGroup()
{
}


static PRBool
IsOutsideASCII(const nsAString& aName)
{
    PRUint32 len = aName.Length();
    const PRUnichar* str = aName.BeginReading();
    for (PRUint32 i = 0; i < len; i++) {
        /*
         * is outside 7 bit ASCII
         */
        if (str[i] > 0x7E) {
            return PR_TRUE;
        }
    }
  
    return PR_FALSE;
}

#define USE_XFT_FOR_ASCII

gfxTextRun*
gfxPangoFontGroup::MakeTextRun(const nsAString& aString)
{
#ifdef USE_XFT_FOR_ASCII
    if (IsOutsideASCII(aString))
        return new gfxPangoTextRun(aString, this);

    return new gfxXftTextRun(aString, this);
#else
    return new gfxPangoTextRun(aString, this);
#endif
}

gfxTextRun*
gfxPangoFontGroup::MakeTextRun(const nsACString& aString)
{
#ifdef USE_XFT_FOR_ASCII
    return new gfxXftTextRun(aString, this);
#else
    return new gfxPangoTextRun(NS_ConvertASCIItoUTF16(aString), this);
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

    PRLibrary* lib = PR_LoadLibrary("libpango-1.0.so");
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
                           const gfxFontStyle *aFontStyle,
                           PangoLanguage* aPangoLang)
    : gfxFont(aName, aFontStyle)
{
    InitPangoLib();

    mPangoFontDesc = nsnull;
    mPangoCtx = nsnull;
    mHasMetrics = PR_FALSE;
    mXftFont = nsnull;
    mPangoLang = aPangoLang;
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
    MOZ_pango_font_description_set_absolute_size(mPangoFontDesc, mStyle->size * PANGO_SCALE);
    pango_font_description_set_style(mPangoFontDesc, ThebesStyleToPangoStyle(mStyle));
    pango_font_description_set_weight(mPangoFontDesc, ThebesStyleToPangoWeight(mStyle));

    //printf ("%s, %f, %d, %d\n", NS_ConvertUTF16toUTF8(mName).get(), mStyle->size, ThebesStyleToPangoStyle(mStyle), ThebesStyleToPangoWeight(mStyle));
#ifndef THEBES_USE_PANGO_CAIRO
    mPangoCtx = pango_xft_get_context(GDK_DISPLAY(), 0);
    gdk_pango_context_set_colormap(mPangoCtx, gdk_rgb_get_cmap());
#else
    mPangoCtx = pango_cairo_font_map_create_context(PANGO_CAIRO_FONT_MAP(pango_cairo_font_map_get_default()));
#endif

    if (mPangoLang)
        pango_context_set_language(mPangoCtx, mPangoLang);
    else if (!mStyle->langGroup.IsEmpty())
        pango_context_set_language(mPangoCtx, GetPangoLanguage(mStyle->langGroup));

    pango_context_set_font_description(mPangoCtx, mPangoFontDesc);

    mHasMetrics = PR_FALSE;
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
    mMetrics.emHeight = mStyle->size;

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
    if (!mMozLang.IsEmpty()) {
        aMozLang.Assign(mMozLang);
        return;
    }
    if (mPangoLang) {
        GetMozLanguage(mPangoLang, mMozLang);
        aMozLang.Assign(mMozLang);
        return;
    }
    aMozLang.Assign(mStyle->langGroup);
}

void
gfxPangoFont::GetActualFontFamily(nsACString &aFamily)
{
    if (!mActualFontFamily.IsEmpty()) {
        aFamily.Assign(mActualFontFamily);
        return;
    }

    PangoFont* font = GetPangoFont();
    FcChar8 *family;
    FcPatternGetString(PANGO_FC_FONT(font)->font_pattern,
                       FC_FAMILY, 0, &family);
    mActualFontFamily.Assign((char *)family);
    aFamily.Assign(mActualFontFamily);
}

PangoFont*
gfxPangoFont::GetPangoFont()
{
    RealizeFont();

    PangoFontMap* map = pango_context_get_font_map(mPangoCtx);
    return pango_font_map_load_font(map, mPangoCtx, mPangoFontDesc);
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
 ** gfxXftTextRun
 **/

gfxXftTextRun::gfxXftTextRun(const nsAString& aString, gfxPangoFontGroup *aGroup)
    : mWString(aString, 0), mIsWide(PR_TRUE), mGroup(aGroup), mWidth(-1), mHeight(-1)

{
}

gfxXftTextRun::gfxXftTextRun(const nsACString& aString, gfxPangoFontGroup *aGroup)
    : mCString(aString, 0), mIsWide(PR_FALSE), mGroup(aGroup), mWidth(-1), mHeight(-1)
{
}

gfxXftTextRun::~gfxXftTextRun()
{
}

#define AUTO_GLYPHBUF_SIZE 100

void
gfxXftTextRun::Draw(gfxContext *aContext, gfxPoint pt)
{
    gfxMatrix mat = aContext->CurrentMatrix();

    nsRefPtr<gfxPangoFont> pf = mGroup->GetFontAt(0);
    //printf("2. %s\n", nsPromiseFlatCString(mString).get());
    XftFont * xfont = pf->GetXftFont();
    //XftDraw * xdraw = pf->GetXftDraw();

    
    cairo_font_face_t* font = cairo_ft_font_face_create_for_pattern(xfont->pattern);
    cairo_set_font_face(aContext->GetCairo(), font);

    double size;
    if (FcPatternGetDouble(xfont->pattern, FC_PIXEL_SIZE, 0, &size) != FcResultMatch)
        size = 12.0;

    cairo_set_font_size(aContext->GetCairo(), size);

    //aContext->MoveTo(pt);
    
    size_t len = mIsWide ? mWString.Length() : mCString.Length();
    
   
    gfxFloat offset = 0;
    cairo_glyph_t autoGlyphs[AUTO_GLYPHBUF_SIZE];
    cairo_glyph_t* glyphs = autoGlyphs;
    if (len > AUTO_GLYPHBUF_SIZE)
        glyphs = new cairo_glyph_t[len];

    for (size_t i = 0; i < len; i++) {
        FT_UInt glyph = mIsWide ?
            XftCharIndex(GDK_DISPLAY(), xfont, mWString[i]) :
            XftCharIndex(GDK_DISPLAY(), xfont, mCString[i]);

        glyphs[i].index = glyph;
        glyphs[i].x = pt.x + offset;
        glyphs[i].y = pt.y;

        XGlyphInfo info;                        
        XftGlyphExtents(GDK_DISPLAY(), xfont, &glyph, 1, &info);
        offset += !mUTF8Spacing.IsEmpty() ? mUTF8Spacing[i] : info.xOff;
    }    

    cairo_show_glyphs(aContext->GetCairo(), glyphs, len);

    if (len > AUTO_GLYPHBUF_SIZE)
        delete [] glyphs;
    

    /*
    if (mIsWide)
        cairo_show_text (aContext->GetCairo(), (const char *) NS_ConvertUTF16toUTF8(mWString).Data());
    else 
        cairo_show_text (aContext->GetCairo(), nsCString(mCString).Data());
    */

    cairo_font_face_destroy(font);

    //aContext->SetMatrix(mat);
}

gfxFloat
gfxXftTextRun::Measure(gfxContext *aContext)
{
    nsRefPtr<gfxPangoFont> pf = mGroup->GetFontAt(0);

    XftFont * font = pf->GetXftFont();
    if (font)
    {
        XGlyphInfo extents;
        Display * dpy = GDK_DISPLAY ();
        if (dpy) {
            if (mIsWide) {
                XftTextExtents16(dpy, font, (FcChar16 *) mWString.Data(), mWString.Length(), &extents);
            } else {
                XftTextExtents8(dpy, font, (FcChar8 *) mCString.Data(), mCString.Length(), &extents);
            }
            mWidth = extents.xOff;
        } else {
            NS_ERROR ("Textruns with no Display");
        }
        
    } else {
        printf ("didn't get font!\n");
        mWidth = 1;
    }

    return mWidth;
}

void
gfxXftTextRun::SetSpacing(const nsTArray<gfxFloat>& spacingArray)
{
    mSpacing = spacingArray;

    //size_t len = mWString.Length();

    if (mIsWide) {
        NS_ConvertUTF16toUTF8 str(mWString);

        mUTF8Spacing.Clear();
        const char *curChar = str.get();
        const char *prevChar = curChar;
        for (unsigned int i = 0; i < mWString.Length(); i++) {
            for (; prevChar + 1 < curChar; prevChar++)
                mUTF8Spacing.AppendElement(0);
            mUTF8Spacing.AppendElement((PRInt32)NSToCoordRound(mSpacing[i]));
            if (IS_HIGH_SURROGATE(mWString[i]))
                i++;
            prevChar = curChar;
            curChar = g_utf8_find_next_char(curChar, NULL);
        }
    }
}

const nsTArray<gfxFloat> *const
gfxXftTextRun::GetSpacing() const
{
    return &mSpacing;
}



/**
 ** gfxPangoTextRun
 **/

#ifndef THEBES_USE_PANGO_CAIRO

#define AUTO_GLYPHBUF_SIZE 100

static gint
DrawCairoGlyphs(gfxContext* ctx,
                PangoFont* aFont,
                const gfxPoint& pt,
                PangoGlyphString* aGlyphs)
{
    PangoFcFont* fcfont = PANGO_FC_FONT(aFont);
    cairo_font_face_t* font = cairo_ft_font_face_create_for_pattern(fcfont->font_pattern);
    cairo_set_font_face(ctx->GetCairo(), font);

    double size;
    if (FcPatternGetDouble(fcfont->font_pattern, FC_PIXEL_SIZE, 0, &size) != FcResultMatch)
        size = 12.0;

    cairo_set_font_size(ctx->GetCairo(), size);

    cairo_glyph_t autoGlyphs[AUTO_GLYPHBUF_SIZE];
    cairo_glyph_t* glyphs = autoGlyphs;
    if (aGlyphs->num_glyphs > AUTO_GLYPHBUF_SIZE)
        glyphs = new cairo_glyph_t[aGlyphs->num_glyphs];

    PangoGlyphUnit offset = 0;
    int num_invalid_glyphs = 0;
    for (gint i = 0; i < aGlyphs->num_glyphs; ++i) {
        PangoGlyphInfo* info = &aGlyphs->glyphs[i];
        // Skip glyph when it is PANGO_GLYPH_EMPTY (0x0FFFFFFF) or has
        // PANGO_GLYPH_UNKNOWN_FLAG (0x10000000) bit set.
        if ((info->glyph & 0x10000000) || info->glyph == 0x0FFFFFFF) {
            // XXX we should to render a slug for the invalid glyph instead of just skipping it
            num_invalid_glyphs++;
        } else {
            glyphs[i-num_invalid_glyphs].index = info->glyph;
            glyphs[i-num_invalid_glyphs].x = pt.x + (offset + info->geometry.x_offset)/FLOAT_PANGO_SCALE;
            glyphs[i-num_invalid_glyphs].y = pt.y + (info->geometry.y_offset)/FLOAT_PANGO_SCALE;
        }

        offset += info->geometry.width;
    }

    cairo_show_glyphs(ctx->GetCairo(), glyphs, aGlyphs->num_glyphs-num_invalid_glyphs);

    if (aGlyphs->num_glyphs > AUTO_GLYPHBUF_SIZE)
        delete [] glyphs;

    cairo_font_face_destroy(font);

    return offset;
}
#endif

gfxPangoTextRun::gfxPangoTextRun(const nsAString& aString, gfxPangoFontGroup *aGroup)
    : mString(aString), mGroup(aGroup), mWidth(-1)
{
}

gfxPangoTextRun::~gfxPangoTextRun()
{
}

void
gfxPangoTextRun::Draw(gfxContext *aContext, gfxPoint pt)
{
    MeasureOrDraw(aContext, PR_TRUE, pt);
}

gfxFloat
gfxPangoTextRun::Measure(gfxContext *aContext)
{
    if (mWidth == -1) {
        static const gfxPoint kZeroZero(0, 0);
        mWidth = MeasureOrDraw(aContext, PR_FALSE, kZeroZero);
    }

    return mWidth/FLOAT_PANGO_SCALE;
}

int
gfxPangoTextRun::MeasureOrDraw(gfxContext *aContext,
                               PRBool     aDraw,
                               gfxPoint   aPt)
{
    gfxMatrix mat;
    nsRefPtr<gfxPangoFont> pf = mGroup->GetFontAt(0);
    NS_ConvertUTF16toUTF8 u8str(mString);
    PRBool isRTL = IsRightToLeft();
    pango_context_set_base_dir(pf->GetPangoContext(),
                               isRTL ? PANGO_DIRECTION_RTL :
                                       PANGO_DIRECTION_LTR);

    if (aDraw) {
        mat = aContext->CurrentMatrix();

        // note that to make this work with pango_cairo, changing
        // this Translate to a MoveTo makes things render in the right place.
        // -I don't know why-.  I mean, I assume it means that the path
        // then starts in the right place, but that makes no sense,
        // since a translate should do the exact same thing.  Since
        // pango-cairo is not going to be the default case, we'll
        // just leave this in here for now and puzzle over it later.
#ifndef THEBES_USE_PANGO_CAIRO
        aContext->Translate(aPt);
#else
        pango_cairo_update_context(aContext->GetCairo(), pf->GetPangoContext());
#endif
    }

    int result;

#ifdef DISABLE_PANGO_FAST
    result = MeasureOrDrawItemizing(aContext, aDraw, aPt, u8str, isRTL, pf);
#else
    result = MeasureOrDrawFast(aContext, aDraw, aPt, u8str, isRTL, pf);
    if (result < 0)
        result = MeasureOrDrawItemizing(aContext, aDraw, aPt, u8str, isRTL, pf);
#endif

    if (aDraw)
        aContext->SetMatrix(mat);

    return result;
}

#define IS_MISSING_GLYPH(g) (((g) & 0x10000000) || (g) == 0x0FFFFFFF)

int
gfxPangoTextRun::MeasureOrDrawFast(gfxContext     *aContext,
                                   PRBool         aDraw,
                                   gfxPoint       aPt,
                                   nsAFlatCString &aUTF8Str,
                                   PRBool         aIsRTL,
                                   gfxPangoFont   *aFont)
{
    PangoAnalysis analysis;
    analysis.font        = aFont->GetPangoFont();
    analysis.level       = aIsRTL ? 1 : 0;
    analysis.lang_engine = nsnull;
    analysis.extra_attrs = nsnull;

    // Find non-ASCII character for finding the language of the script.
    guint32 ch = 'a';
    PRUint8 unicharRange = kRangeSetLatin;
    const PRUint16 *c = mString.get();
    const PRUint16 *end = c + mString.Length();
    for (; c < end; ++c) {
        if (*c > 0x100) {
            ch = *c;
            unicharRange = FindCharUnicodeRange(PRUnichar(ch));
            break;
        }
    }

    // Determin the language for finding the shaper.
    nsCAutoString lang;
    aFont->GetMozLang(lang);
    switch (unicharRange) {
        case kRangeSetLatin:
            lang.Assign("x-western");
            break;
        case kRangeSetCJK:
            if (GetCJKLangGroupIndex(lang.get()) < 0)
                return -1; // try with itemizing
            break;
        default:
            lang.Assign(LangGroupFromUnicodeRange(unicharRange));
            break;
    }

    if (lang.IsEmpty() || lang.Equals("x-unicode") || lang.Equals("x-user-def"))
        return -1; // try with itemizing

    analysis.language     = GetPangoLanguage(lang);
    analysis.shape_engine = pango_font_find_shaper(analysis.font,
                                                   analysis.language,
                                                   ch);

    PangoGlyphString* glyphString = pango_glyph_string_new();
    pango_shape(aUTF8Str.get(), aUTF8Str.Length(), &analysis, glyphString);

    gint num_glyphs = glyphString->num_glyphs;
    gint *clusters = glyphString->log_clusters;
    gint32 spaceWidth = (gint32)
        NSToCoordRound(aFont->GetMetrics().spaceWidth * FLOAT_PANGO_SCALE);
    int width = 0;
    int spacingOffset = 0;
    PRBool spacing = aDraw && !mUTF8Spacing.IsEmpty();
    for (PRInt32 i = 0; i < num_glyphs; ++i) {
        PangoGlyphInfo* info = &glyphString->glyphs[i];
        if (IS_MISSING_GLYPH(info->glyph)) {
            pango_glyph_string_free(glyphString);
            return -1; // try with itemizing
        }

        int j = clusters[i];
        PangoGlyphGeometry *geometry = &glyphString->glyphs[i].geometry;
        if (spacing) {
            geometry->x_offset = spacingOffset;
            spacingOffset += mUTF8Spacing[j] - geometry->width;
        }
        if (aUTF8Str[j] == ' ')
            geometry->width = spaceWidth;
        width += geometry->width;
    }

    // drawing
    if (aDraw && num_glyphs > 0) {
#ifndef THEBES_USE_PANGO_CAIRO
        DrawCairoGlyphs(aContext, analysis.font,
                        gfxPoint(0.0, 0.0),
                        glyphString);
#else
        aContext->MoveTo(aPt); // XXX Do we need?
        pango_cairo_show_glyph_string(aContext->GetCairo(), analysis.font,
                                      glyphString);
#endif
    }

    pango_glyph_string_free(glyphString);
    return width;
}

struct TextSegment {
    PangoFont        *mFont;
    PRUint32          mOffset;
    PRUint32          mLength;
    PangoGlyphString *mGlyphString;
};

class FontSelector
{
public:
    FontSelector(gfxContext *aContext, const char* aString, PRInt32 aLength,
                 gfxPangoFontGroup *aGroup, PangoItem *aItem,
                 PRPackedBool aIsRTL) :
        mContext(aContext), mItem(aItem),
        mGroup(aGroup), mFontIndex(0),
        mString(aString), mLength(aLength),
        mSegmentOffset(0), mSegmentIndex(0),
        mTriedPrefFonts(0), mTriedOtherFonts(0), mIsRTL(aIsRTL)
    {
        for (PRUint32 i = 0; i < mGroup->FontListLength(); ++i)
            mFonts.AppendElement(mGroup->GetFontAt(i));
        InitSegments(0, mLength, mFontIndex);
    }
    ~FontSelector() {
        for (PRUint32 i = 0; i < mSegmentStack.Length(); ++i) {
            SegmentData segment = mSegmentStack[i];
            if (segment.mGlyphString)
                pango_glyph_string_free(segment.mGlyphString);
        }
        mSegmentStack.Clear();
        mFonts.Clear();
    }

    static PRBool ExistsFont(FontSelector *aFs,
                             nsACString &aName) {
        PRUint32 len = aFs->mFonts.Length();
        for (PRUint32 i = 0; i < len; ++i) {
            nsCAutoString family;
            aFs->mFonts[i]->GetActualFontFamily(family);
            if (aName.Equals(family))
                return PR_TRUE;
        }
        return PR_FALSE;
    }

    static PRBool IsAliasFontName(const nsACString &aName) {
        return aName.Equals("serif",
                            nsCaseInsensitiveCStringComparator()) ||
               aName.Equals("sans-serif",
                            nsCaseInsensitiveCStringComparator()) ||
               aName.Equals("sans",
                            nsCaseInsensitiveCStringComparator()) ||
               aName.Equals("monospace",
                            nsCaseInsensitiveCStringComparator());
    }

    static PRBool AddFontCallback(const nsAString &aName,
                                  const nsACString &aGenericName,
                                  const nsACString &aLangGroup,
                                  void *closure) {
        if (aName.IsEmpty())
            return PR_TRUE;

        NS_ConvertUTF16toUTF8 name(aName);
        FontSelector *fs = NS_STATIC_CAST(FontSelector*, closure);

        PRBool isASCIIFontName = IsASCII(name);

        // XXX do something better than this to remove dups
        if (isASCIIFontName && !IsAliasFontName(name) && ExistsFont(fs, name))
            return PR_TRUE;

        nsRefPtr<gfxPangoFont> font = fs->mGroup->GetCachedFont(aName);
        if (!font) {
            const gfxFontStyle *style = fs->mGroup->GetStyle();
            font = new gfxPangoFont(aName, style, GetPangoLanguage(aLangGroup));

            nsCAutoString family;
            font->GetActualFontFamily(family);
            if (!family.Equals(name) && ExistsFont(fs, family))
                return PR_TRUE;

            // XXX Asume that the alias name is ASCII in fontconfig.
            //     Maybe, it is worong, but it is enough in general cases.
            if (!isASCIIFontName)
                fs->mGroup->PutCachedFont(aName, font);
            else
                fs->mGroup->PutCachedFont(NS_ConvertUTF8toUTF16(family), font);
        }
        fs->mFonts.AppendElement(font);

        return PR_TRUE;
    }

    PRBool GetNextSegment(TextSegment *aTextSegment) {
        if (mSegmentIndex >= mSegmentStack.Length())
            return PR_FALSE;

        SegmentData segment = mSegmentStack[mSegmentIndex++];

        aTextSegment->mFont        = segment.mFont;
        aTextSegment->mOffset      = mSegmentOffset;
        aTextSegment->mLength      = segment.mLength;
        aTextSegment->mGlyphString = segment.mGlyphString;
        mSegmentOffset += segment.mLength;
        if (aTextSegment->mGlyphString)
            return PR_TRUE;

        aTextSegment->mGlyphString = pango_glyph_string_new();
        PangoFont *tmpFont = mItem->analysis.font;
        mItem->analysis.font = segment.mFont;
        pango_shape(mString + aTextSegment->mOffset, aTextSegment->mLength,
                    &mItem->analysis, aTextSegment->mGlyphString);
        mItem->analysis.font = tmpFont;
        return PR_TRUE;
    }
private:
    nsRefPtr<gfxContext> mContext;
    PangoItem *mItem;

    nsTArray< nsRefPtr<gfxPangoFont> > mFonts;

    gfxPangoFontGroup *mGroup;
    PRUint32           mFontIndex;

    const char *mString; // UTF-8
    PRInt32     mLength;

    struct SegmentData {
        PRUint32          mLength;
        PangoFont        *mFont;
        PangoGlyphString *mGlyphString;
    };
    PRUint32              mSegmentOffset;
    PRUint32              mSegmentIndex;
    nsTArray<SegmentData> mSegmentStack;

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

RetryNextFont:
        PangoFont *font = GetNextFont();

        // If we cannot found the font that has the current character glyph,
        // we should return default font's missing data.
        if (!font) {
            font = mFonts[betterFontIndex]->GetPangoFont();
            checkMissingGlyph = PR_FALSE;
        }

        // Shaping
        PangoGlyphString *glyphString = pango_glyph_string_new();
        PangoFont *tmpFont = mItem->analysis.font;
        mItem->analysis.font = font;
        pango_shape(current, aLength, &mItem->analysis, glyphString);
        mItem->analysis.font = tmpFont;

        gint num_glyphs     = glyphString->num_glyphs;
        gint *clusters      = glyphString->log_clusters;
        PRUint32 offset     = aOffset;
        PRUint32 skipLength = 0;
        if (checkMissingGlyph) {
            for (PRInt32 i = 0; i < num_glyphs; ++i) {
                PangoGlyphInfo* info = &glyphString->glyphs[i];
                if (IS_MISSING_GLYPH(info->glyph)) {
                    // XXX Note that we don't support the segment separation
                    // in RTL text. Because the Arabic characters changes the
                    // glyphs by the position of the context. I think that the
                    // languages of RTL doesn't have *very* many characters, so,
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
                        pango_glyph_string_free(glyphString);
                        goto RetryNextFont;
                    }

                    // The glyph is missing, separate segment here.
                    PRUint32 missingLength = aLength - clusters[i];
                    PRInt32 j;
                    for (j = i + 1; j < num_glyphs; ++j) {
                        info = &glyphString->glyphs[j];
                        if (!IS_MISSING_GLYPH(info->glyph)) {
                            missingLength = clusters[j] - offset;
                            break;
                        }
                    }

                    if (i != 0) {
                        // found glyphs
                        SegmentData segment;
                        segment.mLength      = offset - (aOffset + skipLength);
                        segment.mGlyphString = nsnull;
                        segment.mFont        = font;
                        mSegmentStack.AppendElement(segment);
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

        SegmentData segment;
        segment.mLength      = aLength - skipLength;
        if (skipLength == 0)
            segment.mGlyphString = glyphString;
        else {
            segment.mGlyphString = nsnull;
            pango_glyph_string_free(glyphString);
        }
        segment.mFont        = font;
        mSegmentStack.AppendElement(segment);
    }

    PangoFont *GetNextFont() {
TRY_AGAIN_HOPE_FOR_THE_BEST_2:
        if (mFontIndex < mFonts.Length()) {
            nsRefPtr<gfxPangoFont> font = mFonts[mFontIndex++];
            return font->GetPangoFont();
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

int
gfxPangoTextRun::MeasureOrDrawItemizing(gfxContext     *aContext,
                                        PRBool         aDraw,
                                        gfxPoint       aPt,
                                        nsAFlatCString &aUTF8Str,
                                        PRBool         aIsRTL,
                                        gfxPangoFont   *aFont)
{
    GList *items = pango_itemize(aFont->GetPangoContext(), aUTF8Str.get(), 0,
                                 aUTF8Str.Length(), NULL, NULL);
    if (items) {
        GList *tmp = items;
        items = pango_reorder_items(tmp);
        g_list_free(tmp);
    }
    gint32 spaceWidth = (gint32)
        NSToCoordRound(aFont->GetMetrics().spaceWidth * FLOAT_PANGO_SCALE);
    int width = 0;
    int spacingOffset = 0;
    int drawingOffset = 0;
    PRBool spacing = aDraw && !mUTF8Spacing.IsEmpty();
    PRBool isRTL = aIsRTL;
    for (; items && items->data; items = items->next) {
        PangoItem *item = (PangoItem *)items->data;
        PRBool itemIsRTL = item->analysis.level % 2;
        if ((itemIsRTL && !isRTL) || (!itemIsRTL && isRTL)) {
            isRTL = itemIsRTL;
            pango_context_set_base_dir(aFont->GetPangoContext(),
                                       isRTL ? PANGO_DIRECTION_RTL :
                                               PANGO_DIRECTION_LTR);
        }
        FontSelector fs(aContext, aUTF8Str.get() + item->offset,
                        item->length, mGroup, item, isRTL);
        TextSegment segment;
        while (fs.GetNextSegment(&segment)) {
            PangoGlyphString *glyphString = segment.mGlyphString;
            int currentWidth = 0;
            for (int i = 0; i < glyphString->num_glyphs; i++) {
                // Adjust spacing and fix the space width
                int j = glyphString->log_clusters[i] +
                            item->offset + segment.mOffset;
                PangoGlyphGeometry *geometry = &glyphString->glyphs[i].geometry;
                if (spacing) {
                    geometry->x_offset = spacingOffset;
                    spacingOffset += mUTF8Spacing[j] - geometry->width;
                }
                if (aUTF8Str[j] == ' ')
                    geometry->width = spaceWidth;
                currentWidth += geometry->width;
            }

            // drawing
            if (aDraw && glyphString->num_glyphs > 0) {
#ifndef THEBES_USE_PANGO_CAIRO
                DrawCairoGlyphs(aContext, segment.mFont,
                                gfxPoint(drawingOffset/FLOAT_PANGO_SCALE, 0.0),
                                glyphString);
#else
                aContext->MoveTo(aPt); // XXX Do we need?
                glyphString->glyphs[0].geometry.x_offset += drawingOffset;
                pango_cairo_show_glyph_string(aContext->GetCairo(),
                                              segment.mFont,
                                              glyphString);
#endif
                drawingOffset += currentWidth;
            }

            width += currentWidth;
        }

        pango_item_free(item);
     }

    if (items)
        g_list_free(items);

    return width;
}

void
gfxPangoTextRun::SetSpacing(const nsTArray<gfxFloat> &spacingArray)
{
    mSpacing = spacingArray;
    NS_ConvertUTF16toUTF8 str(mString);
    mUTF8Spacing.Clear();
    const char *curChar = str.get();
    const char *prevChar = curChar;
    for (unsigned int i = 0; i < mString.Length(); i++) {
        for (; prevChar + 1 < curChar; prevChar++)
            mUTF8Spacing.AppendElement(0);
        mUTF8Spacing.AppendElement((PRInt32)NSToCoordRound(mSpacing[i] * FLOAT_PANGO_SCALE));
        if (IS_HIGH_SURROGATE(mString[i]))
            i++;
        prevChar = curChar;
        curChar = g_utf8_find_next_char(curChar, NULL);
    }
}

const nsTArray<gfxFloat> *const
gfxPangoTextRun::GetSpacing() const
{
    return &mSpacing;
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
    if (lang.Equals("xx"))
        return;

    do {
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
    } while (0);
}


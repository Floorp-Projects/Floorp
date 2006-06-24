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

#ifdef XP_BEOS
#define THEBES_USE_PANGO_CAIRO
#endif

#include "prtypes.h"
#include "prlink.h"
#include "gfxTypes.h"

#include "nsIPref.h"
#include "nsServiceManagerUtils.h"

#include "nsVoidArray.h"
#include "nsPromiseFlatString.h"

#include "gfxContext.h"
#include "gfxPangoFonts.h"

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

static PRBool
IsASCIIFontName(const nsAString& aName)
{
    PRUint32 len = aName.Length();
    const PRUnichar* str = aName.BeginReading();
    for (PRUint32 i = 0; i < len; i++) {
        /*
         * X font names are printable ASCII, ignore others (for now)
         */
        if ((str[i] < 0x20) || (str[i] > 0x7E)) {
            return PR_FALSE;
        }
    }
  
    return PR_TRUE;
}

PRBool
gfxPangoFontGroup::FontCallback (const nsAString& fontName,
                                 const nsACString& genericName,
                                 void *closure)
{
    nsStringArray *sa = NS_STATIC_CAST(nsStringArray*, closure);

    if (IsASCIIFontName(fontName) && FFRECountHyphens(fontName) < 3) {
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

    ForEachFont (FontCallback, &familyArray);

    FindGenericFontFromStyle (FontCallback, &familyArray);

    // now join them up with commas again, so that pango
    // can split them up later
    nsString fixedFamilies;
    for (int i = 0; i < familyArray.Count(); i++) {
        fixedFamilies.Append(*familyArray[i]);
        fixedFamilies.AppendLiteral(",");
    }
    if (fixedFamilies.Length() > 0)
      fixedFamilies.Truncate(fixedFamilies.Length() - 1); // remove final comma

    mFonts.AppendElement(new gfxPangoFont(fixedFamilies, GetStyle()));
}

gfxPangoFontGroup::~gfxPangoFontGroup()
{
}

gfxTextRun*
gfxPangoFontGroup::MakeTextRun(const nsAString& aString)
{
    return new gfxPangoTextRun(aString, this);
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

gfxPangoFont::gfxPangoFont(const nsAString &aName, const gfxFontStyle *aFontStyle)
    : gfxFont(aName, aFontStyle)
{
    InitPangoLib();

    mPangoFontDesc = nsnull;
    mPangoCtx = nsnull;
    mHasMetrics = PR_FALSE;
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
    mMetrics.height = lineHeight;

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
    mMetrics.height = lineHeight; // XXX should go away

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
    fprintf (stderr, "    height: %f internalLeading: %f externalLeading: %f\n", mMetrics.height, mMetrics.externalLeading, mMetrics.internalLeading);
    fprintf (stderr, "    spaceWidth: %f aveCharWidth: %f xHeight: %f\n", mMetrics.spaceWidth, mMetrics.aveCharWidth, mMetrics.xHeight);
    fprintf (stderr, "    uOff: %f uSize: %f stOff: %f stSize: %f suOff: %f suSize: %f\n", mMetrics.underlineOffset, mMetrics.underlineSize, mMetrics.strikeoutOffset, mMetrics.strikeoutSize, mMetrics.superscriptOffset, mMetrics.subscriptOffset);
#endif

    mHasMetrics = PR_TRUE;
    return mMetrics;
}



/**
 ** gfxPangoTextRun
 **/

THEBES_IMPL_REFCOUNTING(gfxPangoTextRun)

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
    : mString(aString), mGroup(aGroup), mPangoLayout(nsnull), mWidth(-1), mHeight(-1)
{
}

gfxPangoTextRun::~gfxPangoTextRun()
{
    if (mPangoLayout) {
        g_object_unref (mPangoLayout);
        mPangoLayout = nsnull;
    }
}

void
gfxPangoTextRun::EnsurePangoLayout(gfxContext *aContext)
{
    nsRefPtr<gfxPangoFont> pf = mGroup->GetFontAt(0);

    if (mPangoLayout == nsnull) {
        NS_ConvertUTF16toUTF8 u8str(mString);

        mPangoLayout = pango_layout_new (pf->GetPangoContext());
        pango_layout_set_text (mPangoLayout, u8str.Data(), u8str.Length());

        if (pango_layout_get_line_count(mPangoLayout) != 1) {
            NS_WARNING("gfxPangoFonts: more than one line in layout!\n");
        }

        // fix up the space width
        PangoLayoutLine *line = pango_layout_get_line(mPangoLayout, 0);
        gint32 spaceWidth =
            NSToCoordRound(pf->GetMetrics().spaceWidth * FLOAT_PANGO_SCALE);
        for (GSList *tmpList = line->runs;
             tmpList && tmpList->data; tmpList = tmpList->next)
        {
            PangoLayoutRun *layoutRun = (PangoLayoutRun *)tmpList->data;
            for (gint i=0; i < layoutRun->glyphs->num_glyphs; i++) {
                gint j = (gint)layoutRun->glyphs->log_clusters[i] +
                         layoutRun->item->offset;
                if (u8str[j] == ' ')
                    layoutRun->glyphs->glyphs[i].geometry.width = spaceWidth;
            }
        }
    }

#ifdef THEBES_USE_PANGO_CAIRO
    if (aContext) {
        pango_cairo_update_context (aContext->GetCairo(), pf->GetPangoContext());
    }
    pango_layout_context_changed (mPangoLayout);
#endif
}

void
gfxPangoTextRun::Draw(gfxContext *aContext, gfxPoint pt)
{
    gfxMatrix mat = aContext->CurrentMatrix();

    // note that to make this work with pango_cairo, changing
    // this Translate to a MoveTo makes things render in the right place.
    // -I don't know why-.  I mean, I assume it means that the path
    // then starts in the right place, but that makes no sense,
    // since a translate should do the exact same thing.  Since
    // pango-cairo is not going to be the default case, we'll
    // just leave this in here for now and puzzle over it later.
#ifndef THEBES_USE_PANGO_CAIRO
    aContext->Translate(pt);
#else
    aContext->MoveTo(pt);
#endif

    // we draw only the first layout line, because
    // we can then position by baseline (which is what pt is at);
    // using show_layout expects top-left point.
    EnsurePangoLayout(aContext);

#if 0
    Measure(aContext);
    if (mWidth != -1) {
        aContext->Save();
        aContext->SetColor(gfxRGBA(1.0, 0.0, 0.0, 0.5));
        aContext->NewPath();
        aContext->Rectangle(gfxRect(0.0, -mHeight/FLOAT_PANGO_SCALE, mWidth/FLOAT_PANGO_SCALE, mHeight/FLOAT_PANGO_SCALE));
        aContext->Fill();
        aContext->Restore();
    }
#endif

    PangoLayoutLine *line = pango_layout_get_line(mPangoLayout, 0);

    if (!mUTF8Spacing.IsEmpty()) {
        gint offset = 0;
        for (GSList *tmpList = line->runs;
             tmpList && tmpList->data;
             tmpList = tmpList->next)
        {
            PangoLayoutRun *layoutRun = (PangoLayoutRun *)tmpList->data;
            PangoGlyphString *glyphString = layoutRun->glyphs;
            for (gint i = 0; i < glyphString->num_glyphs; i++) {
                PangoGlyphGeometry* geometry = &glyphString->glyphs[i].geometry;
                geometry->x_offset = offset;
                gint index =
                    glyphString->log_clusters[i] + layoutRun->item->offset;
                offset += mUTF8Spacing[index] - geometry->width;
            }
        }
    }

#ifndef THEBES_USE_PANGO_CAIRO
    gint offset = 0;
    for (GSList *tmpList = line->runs;
         tmpList && tmpList->data;
         tmpList = tmpList->next)
    {
        PangoLayoutRun *layoutRun = (PangoLayoutRun *)tmpList->data;

        offset += DrawCairoGlyphs (aContext, layoutRun->item->analysis.font,
                                   gfxPoint(offset / FLOAT_PANGO_SCALE, 0.0),
                                   layoutRun->glyphs);
    }
#else
    pango_cairo_show_layout_line (aContext->GetCairo(), line);
#endif

    aContext->SetMatrix(mat);
}

gfxFloat
gfxPangoTextRun::Measure(gfxContext *aContext)
{
    if (mWidth == -1) {
        EnsurePangoLayout(aContext);
        pango_layout_get_size (mPangoLayout, &mWidth, &mHeight);
    }

    return mWidth/FLOAT_PANGO_SCALE;
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
        mUTF8Spacing.AppendElement(NSToCoordRound(mSpacing[i] * FLOAT_PANGO_SCALE));
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
    { "x-western",      "en" },
    { "x-central-euro", "pl" },
    { "x-cyrillic",     "ru" },
    { "x-baltic",       "lv" },
    { "x-devanagari",   "hi" },
    { "x-tamil",        "ta" },
    { "x-unicode",      0    },
    { "x-user-def",     0    },
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


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
#include "gfxTypes.h"

#include "nsPromiseFlatString.h"

#include "gfxContext.h"
#include "gfxPangoFonts.h"

#include <pango/pangocairo.h>

static PangoLanguage *GetPangoLanguage(const nsACString& aLangGroup);

gfxPangoFont::gfxPangoFont(const nsAString &aName, const gfxFontGroup *aFontGroup)
    : mName(aName), mFontGroup(aFontGroup)
{
    mFontStyle = mFontGroup->GetStyle();

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
    pango_font_description_set_absolute_size(mPangoFontDesc, mFontStyle->size * PANGO_SCALE);
    pango_font_description_set_style(mPangoFontDesc, ThebesStyleToPangoStyle(mFontStyle));
    pango_font_description_set_weight(mPangoFontDesc, ThebesStyleToPangoWeight(mFontStyle));

    mPangoCtx = pango_cairo_font_map_create_context(PANGO_CAIRO_FONT_MAP(pango_cairo_font_map_get_default()));

    if (!mFontStyle->langGroup.IsEmpty())
        pango_context_set_language(mPangoCtx, GetPangoLanguage(mFontStyle->langGroup));

    pango_context_set_font_description(mPangoCtx, mPangoFontDesc);

    mHasMetrics = PR_FALSE;
}

void
gfxPangoFont::UpdateContext(gfxContext *ctx)
{
    RealizeFont();
    pango_cairo_update_context (ctx->GetCairo(), mPangoCtx);
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

    inkSize.width = ink_rect.width / PANGO_SCALE;
    inkSize.height = ink_rect.height / PANGO_SCALE;

    logSize.width = log_rect.width / PANGO_SCALE;
    logSize.height = log_rect.height / PANGO_SCALE;

    pango_glyph_string_free(glstr);
    pango_item_free(item);
    g_list_free(items);
}

void
gfxPangoFont::DrawString(gfxContext *ctx, const char *aString, PRUint32 aLength, gfxPoint pt)
{
    //fprintf (stderr, "DrawString: '%s'\n", aString);
    gfxMatrix mat = ctx->CurrentMatrix();

    PangoLayout *layout = pango_layout_new (mPangoCtx);
    pango_layout_set_text (layout, aString, aLength);

    if (pango_layout_get_line_count(layout) == 1) {
        // we draw only the first layout line, because
        // we can then position by baseline (which is what pt is at);
        // using show_layout expects top-left point.
        ctx->MoveTo(pt);
        UpdateContext(ctx);
        PangoLayoutLine *line = pango_layout_get_line(layout, 0);
        pango_cairo_show_layout_line (ctx->GetCairo(), line);
    } else {
        //printf("**** gfxPangoFonts: more than one line in layout!\n");
        // we really should never hit this
        ctx->MoveTo(gfxPoint(pt.x, pt.y - mMetrics.height));
        UpdateContext(ctx);
        pango_cairo_show_layout (ctx->GetCairo(), layout);
    }
    g_object_unref (layout);

    ctx->SetMatrix(mat);
}

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

    PangoItem *item = (PangoItem*)items->data;
    PangoFont *font = item->analysis.font;

    PangoFontMetrics *pfm = pango_font_get_metrics (font, NULL);

    // ??
    mMetrics.emHeight = mFontStyle->size;

    mMetrics.maxAscent = pango_font_metrics_get_ascent(pfm) / PANGO_SCALE;
    mMetrics.maxDescent = pango_font_metrics_get_descent(pfm) / PANGO_SCALE;

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

    mMetrics.maxAdvance = pango_font_metrics_get_approximate_char_width(pfm) / PANGO_SCALE; // XXX

    gfxSize isz, lsz;
    GetSize(" ", 1, isz, lsz);
    mMetrics.spaceWidth = lsz.width;
    GetSize("x", 1, isz, lsz);
    mMetrics.xHeight = isz.height;

    mMetrics.aveCharWidth = pango_font_metrics_get_approximate_char_width(pfm) / PANGO_SCALE;

    mMetrics.underlineOffset = pango_font_metrics_get_underline_position(pfm) / PANGO_SCALE;
    mMetrics.underlineSize = pango_font_metrics_get_underline_thickness(pfm) / PANGO_SCALE;

    mMetrics.strikeoutOffset = pango_font_metrics_get_strikethrough_position(pfm) / PANGO_SCALE;
    mMetrics.strikeoutSize = pango_font_metrics_get_strikethrough_thickness(pfm) / PANGO_SCALE;

    // these are specified by the so-called OS2 SFNT info, but
    // pango doesn't expose this to us.  This really sucks,
    // so we just assume it's the xHeight
    mMetrics.superscriptOffset = mMetrics.xHeight;
    mMetrics.subscriptOffset = mMetrics.xHeight;

    pango_font_metrics_unref (pfm);

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

gfxPangoFontGroup::gfxPangoFontGroup (const nsAString& families,
                                      const gfxFontStyle *aStyle)
    : gfxFontGroup(families, aStyle)
{
    gfxFont *f = new gfxPangoFont(families, this);
    mFonts.push_back(f);
}

gfxPangoFontGroup::~gfxPangoFont()
{
}

void
gfxPangoFontGroup::DrawString (gfxContext *aContext,
                               const nsAString& aString,
                               gfxPoint pt)
{
    gfxPangoFont *pf = ((gfxPangoFont*) mFonts[0]);
    NS_ConvertUTF16toUTF8 u8str(aString);
    pf->DrawString(aContext, u8str.Data(), u8str.Length(), pt);
}

gfxFloat
gfxPangoFontGroup::MeasureText (gfxContext *aContext,
                                const nsAString& aString)
{
    gfxPangoFont *pf = ((gfxPangoFont*) mFonts[0]);
    NS_ConvertUTF16toUTF8 u8str(aString);
    int pw, ph;

    //fprintf (stderr, "SizeString: '%s'\n", u8str.Data());

    pf->UpdateContext(aContext);
    PangoLayout *layout = pango_layout_new (pf->GetContext());
    pango_layout_set_text (layout, u8str.Data(), u8str.Length());
    pango_layout_get_size(layout, &pw, &ph);
    g_object_unref (layout);

    return pw/PANGO_SCALE;
}

gfxFont*
gfxPangoFontGroup::MakeFont(const nsAString& aName)
{
    return nsnull;
}

/** language group helpers **/
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


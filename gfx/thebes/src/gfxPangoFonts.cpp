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
 *   Behdad Esfahbod <behdad@gnome.org>
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *   Karl Tomlinson <karlt+@karlt.net>, Mozilla Corporation
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
#include "gfxPlatformGtk.h"
#include "gfxPangoFonts.h"

#include "nsCRT.h"

#include <locale.h>
#include <freetype/tttables.h>

#include <cairo.h>
#include <cairo-ft.h>

#include <pango/pango.h>
#include <pango/pangocairo.h>
#include <pango/pangofc-fontmap.h>

#include <gdk/gdkpango.h>

#include <math.h>

#define FLOAT_PANGO_SCALE ((gfxFloat)PANGO_SCALE)

#ifndef PANGO_VERSION_CHECK
#define PANGO_VERSION_CHECK(x,y,z) 0
#endif
#ifndef PANGO_GLYPH_UNKNOWN_FLAG
#define PANGO_GLYPH_UNKNOWN_FLAG ((PangoGlyph)0x10000000)
#endif
#ifndef PANGO_GLYPH_EMPTY
#define PANGO_GLYPH_EMPTY           ((PangoGlyph)0)
#endif
// For g a PangoGlyph,
#define IS_MISSING_GLYPH(g) ((g) & PANGO_GLYPH_UNKNOWN_FLAG)
#define IS_EMPTY_GLYPH(g) ((g) == PANGO_GLYPH_EMPTY)

static PangoLanguage *GetPangoLanguage(const nsACString& aLangGroup);

/* static */ gfxPangoFontCache* gfxPangoFontCache::sPangoFontCache = nsnull;

/**
 * gfxPangoFontset: An implementation of a PangoFontset for gfxPangoFontMap
 */

#define GFX_TYPE_PANGO_FONTSET              (gfx_pango_fontset_get_type())
#define GFX_PANGO_FONTSET(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GFX_TYPE_PANGO_FONTSET, gfxPangoFontset))
#define GFX_IS_PANGO_FONTSET(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GFX_TYPE_PANGO_FONTSET))

/* static */
GType gfx_pango_fontset_get_type (void);

#define GFX_PANGO_FONTSET_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GFX_TYPE_PANGO_FONTSET, gfxPangoFontsetClass))
#define GFX_IS_PANGO_FONTSET_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GFX_TYPE_PANGO_FONTSET))
#define GFX_PANGO_FONTSET_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GFX_TYPE_PANGO_FONTSET, gfxPangoFontsetClass))

// This struct is POD so that it can be used as a GObject.
struct gfxPangoFontset {
    PangoFontset parent_instance;

    PangoContext *mContext;
    PangoFontDescription *mFontDesc;
    PangoLanguage *mLanguage;
    PangoFont *mBaseFont;
    PangoFontMap *mFontMap;
    PangoFontset *mChildFontset;

    static PangoFontset *
    NewFontset(PangoContext *aContext,
               const PangoFontDescription *aFontDesc,
               PangoLanguage *aLanguage,
               PangoFont *aBaseFont, PangoFontMap *aFontMap)
    {
        gfxPangoFontset *fontset = static_cast<gfxPangoFontset *>
            (g_object_new(GFX_TYPE_PANGO_FONTSET, NULL));

        fontset->mContext = aContext;
        g_object_ref(aContext);

        fontset->mFontDesc = pango_font_description_copy(aFontDesc);
        fontset->mLanguage = aLanguage;

        fontset->mBaseFont = aBaseFont;
        if(aBaseFont)
            g_object_ref(aBaseFont);

        fontset->mFontMap = aFontMap;
        g_object_ref(aFontMap);

        return PANGO_FONTSET(fontset);
    }
};

struct gfxPangoFontsetClass {
    PangoFontsetClass parent_class;
};

G_DEFINE_TYPE (gfxPangoFontset, gfx_pango_fontset, PANGO_TYPE_FONTSET)

static void
gfx_pango_fontset_init(gfxPangoFontset *fontset)
{
    fontset->mContext = NULL;
    fontset->mFontDesc = NULL;
    fontset->mLanguage = NULL;
    fontset->mBaseFont = NULL;
    fontset->mFontMap = NULL;
    fontset->mChildFontset = NULL;
}


static void
gfx_pango_fontset_finalize(GObject *object)
{
    gfxPangoFontset *self = GFX_PANGO_FONTSET(object);

    if (self->mContext)
        g_object_unref(self->mContext);
    if (self->mFontDesc)
        pango_font_description_free(self->mFontDesc);
    if (self->mBaseFont)
        g_object_unref(self->mBaseFont);
    if (self->mFontMap)
        g_object_unref(self->mFontMap);
    if (self->mChildFontset)
        g_object_unref(self->mChildFontset);

    G_OBJECT_CLASS(gfx_pango_fontset_parent_class)->finalize(object);
}

static PangoLanguage *
gfx_pango_fontset_get_language(PangoFontset *fontset)
{
    gfxPangoFontset *self = GFX_PANGO_FONTSET(fontset);
    return self->mLanguage;
}

struct ForeachExceptBaseData {
    PangoFont *mBaseFont;
    PangoFontset *mFontset;
    PangoFontsetForeachFunc mFunc;
    gpointer mData;
};

static gboolean
foreach_except_base_cb(PangoFontset *fontset, PangoFont *font, gpointer data)
{
    ForeachExceptBaseData *baseData =
        static_cast<ForeachExceptBaseData *>(data);
    
    // returning false means continue with the other fonts in the set
    return font != baseData->mBaseFont &&
        (*baseData->mFunc)(baseData->mFontset, font, baseData->mData);
}

static PangoFontset *
EnsureChildFontset(gfxPangoFontset *self)
{
    if (!self->mChildFontset) {
        // To consider:
        //
        // * If this is happening often (e.g. Chinese/Japanese pagess where a
        //   Latin font is specified first), and Pango's 64-entry pattern
        //   cache is not large enough, then a fontset cache here could be
        //   helpful.  Ideally we'd only cache the fonts that are actually
        //   accessed rather than all the fonts from the FcFontSort.
        //
        // * Mozilla's langGroup font prefs could be used to specify preferred
        //   fallback fonts for the script of the characters (as indicated by
        //   Pango in mLanguage), by doing the conversion from gfxFontGroup
        //   "families" to PangoFcFontMap "family" here.
        self->mChildFontset =
            pango_font_map_load_fontset(self->mFontMap, self->mContext,
                                        self->mFontDesc, self->mLanguage);
    }
    return self->mChildFontset;
}

static void
gfx_pango_fontset_foreach(PangoFontset *fontset, PangoFontsetForeachFunc func,
                          gpointer data)
{
    gfxPangoFontset *self = GFX_PANGO_FONTSET(fontset);

    if (self->mBaseFont && (*func)(fontset, self->mBaseFont, data))
        return;

    // Falling back to secondary fonts
    PangoFontset *childFontset = EnsureChildFontset(self);
    ForeachExceptBaseData baseData = { self->mBaseFont, fontset, func, data };
    pango_fontset_foreach(childFontset, foreach_except_base_cb, &baseData);
}

static PangoFont *
gfx_pango_fontset_get_font(PangoFontset *fontset, guint wc)
{
    gfxPangoFontset *self = GFX_PANGO_FONTSET(fontset);

    PangoCoverageLevel baseLevel = PANGO_COVERAGE_NONE;
    if (self->mBaseFont) {
        // PangoFcFontMap caches this:
        PangoCoverage *coverage =
            pango_font_get_coverage(self->mBaseFont, self->mLanguage);
        if (coverage) {
            baseLevel = pango_coverage_get(coverage, wc);
            pango_coverage_unref(coverage);
        }
    }

    if (baseLevel != PANGO_COVERAGE_EXACT) {
        PangoFontset *childFontset = EnsureChildFontset(self);
        PangoFont *childFont = pango_fontset_get_font(childFontset, wc);
        if (!self->mBaseFont || childFont == self->mBaseFont)
            return childFont;

        if (childFont) {
            PangoCoverage *coverage =
                pango_font_get_coverage(childFont, self->mLanguage);
            if (coverage) {
                PangoCoverageLevel childLevel =
                    pango_coverage_get(coverage, wc);
                pango_coverage_unref(coverage);

                // Only use the child font if better than the base font.
                if (childLevel > baseLevel)
                    return childFont;
            }
            g_object_unref(childFont);
        }
    }

    g_object_ref(self->mBaseFont);
    return self->mBaseFont;
}

static void
gfx_pango_fontset_class_init (gfxPangoFontsetClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    PangoFontsetClass *fontset_class = PANGO_FONTSET_CLASS (klass);

    object_class->finalize = gfx_pango_fontset_finalize;
    fontset_class->get_font = gfx_pango_fontset_get_font;
    // inherit fontset_class->get_metrics (which won't be used anyway)
    fontset_class->get_language = gfx_pango_fontset_get_language;
    fontset_class->foreach = gfx_pango_fontset_foreach;
}

/**
 * gfxPangoFontMap: An implementation of a PangoFontMap.
 *
 * This allows the primary (base) font to be specified.  There are two
 * advantages to this:
 *
 * 1. Always using the same base font irrespective of the language that Pango
 *    chooses for the script means that PANGO_SCRIPT_COMMON characters are
 *    consistently rendered with the same font.  (Bug 339513 and bug 416725)
 *
 * 2. We normally have the base font from the gfxFont cache so this saves a
 *    FcFontSort when the entry has expired from Pango's much smaller pattern
 *    cache.
 *
 * This object references a child font map rather than deriving so that
 * the cache of the child font map is shared.
 */

#define GFX_TYPE_PANGO_FONT_MAP              (gfx_pango_font_map_get_type())
#define GFX_PANGO_FONT_MAP(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GFX_TYPE_PANGO_FONT_MAP, gfxPangoFontMap))
#define GFX_IS_PANGO_FONT_MAP(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GFX_TYPE_PANGO_FONT_MAP))

GType gfx_pango_font_map_get_type (void);

#define GFX_PANGO_FONT_MAP_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GFX_TYPE_PANGO_FONT_MAP, gfxPangoFontMapClass))
#define GFX_IS_PANGO_FONT_MAP_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GFX_TYPE_PANGO_FONT_MAP))
#define GFX_PANGO_FONT_MAP_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GFX_TYPE_PANGO_FONT_MAP, gfxPangoFontMapClass))

// Do not instantiate this class directly, but use NewFontMap.
// This struct is POD so that it can be used as a GObject.
struct gfxPangoFontMap {
    PangoFontMap parent_instance;

    PangoFontMap *mChildFontMap;
    PangoFont *mBaseFont;

    static PangoFontMap *
    NewFontMap(PangoFontMap *aChildFontMap, PangoFont *aBaseFont)
    {
        NS_ASSERTION(strcmp(pango_font_map_get_shape_engine_type(aChildFontMap), 
                            PANGO_RENDER_TYPE_FC) == 0,
                     "Unexpected child PangoFontMap shape engine type");

        gfxPangoFontMap *fontmap = static_cast<gfxPangoFontMap *>
            (g_object_new(GFX_TYPE_PANGO_FONT_MAP, NULL));

        fontmap->mChildFontMap = aChildFontMap;
        g_object_ref(aChildFontMap);

        fontmap->mBaseFont = aBaseFont;
        if(aBaseFont)
            g_object_ref(aBaseFont);

        return PANGO_FONT_MAP(fontmap);
    }

    void
    SetBaseFont(PangoFont *aBaseFont)
    {
        if (mBaseFont)
            g_object_unref(mBaseFont);

        mBaseFont = aBaseFont;

        if (aBaseFont)
            g_object_ref(aBaseFont);
    }
};

struct gfxPangoFontMapClass {
    PangoFontMapClass parent_class;
};

G_DEFINE_TYPE (gfxPangoFontMap, gfx_pango_font_map, PANGO_TYPE_FONT_MAP)

static void
gfx_pango_font_map_init(gfxPangoFontMap *fontset)
{
    fontset->mChildFontMap = NULL;    
    fontset->mBaseFont = NULL;
}

static void
gfx_pango_font_map_finalize(GObject *object)
{
    gfxPangoFontMap *self = GFX_PANGO_FONT_MAP(object);

    if (self->mChildFontMap)
        g_object_unref(self->mChildFontMap);
    if (self->mBaseFont)
        g_object_unref(self->mBaseFont);

    G_OBJECT_CLASS(gfx_pango_font_map_parent_class)->finalize(object);
}

static PangoFont *
gfx_pango_font_map_load_font(PangoFontMap *fontmap, PangoContext *context,
                             const PangoFontDescription *description)
{
    gfxPangoFontMap *self = GFX_PANGO_FONT_MAP(fontmap);
    if (self->mBaseFont) {
        g_object_ref(self->mBaseFont);
        return self->mBaseFont;
    }

    return pango_font_map_load_font(self->mChildFontMap, context, description);
}

static PangoFontset *
gfx_pango_font_map_load_fontset(PangoFontMap *fontmap, PangoContext *context,
                               const PangoFontDescription *desc,
                               PangoLanguage *language)
{
    gfxPangoFontMap *self = GFX_PANGO_FONT_MAP(fontmap);
    return gfxPangoFontset::NewFontset(context, desc, language,
                                       self->mBaseFont, self->mChildFontMap);
}

static void
gfx_pango_font_map_list_families(PangoFontMap *fontmap,
                                 PangoFontFamily ***families, int *n_families)
{
    gfxPangoFontMap *self = GFX_PANGO_FONT_MAP(fontmap);
    pango_font_map_list_families(self->mChildFontMap, families, n_families);
}

static void
gfx_pango_font_map_class_init(gfxPangoFontMapClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    PangoFontMapClass *fontmap_class = PANGO_FONT_MAP_CLASS (klass);

    object_class->finalize = gfx_pango_font_map_finalize;
    fontmap_class->load_font = gfx_pango_font_map_load_font;
    fontmap_class->load_fontset = gfx_pango_font_map_load_fontset;
    fontmap_class->list_families = gfx_pango_font_map_list_families;
    fontmap_class->shape_engine_type = PANGO_RENDER_TYPE_FC;
}

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
    nsStringArray *sa = static_cast<nsStringArray*>(closure);

    // We ignore prefs that have three hypens since they are X style prefs.
    if (genericName.Length() && FFRECountHyphens(fontName) >= 3)
        return PR_TRUE;

    if (sa->IndexOf(fontName) < 0) {
        sa->AppendString(fontName);
    }

    return PR_TRUE;
}

/**
 * Look up the font in the gfxFont cache. If we don't find it, create one.
 * In either case, add a ref, append it to the aFonts array, and return it ---
 * except for OOM in which case we do nothing and return null.
 */
static already_AddRefed<gfxPangoFont>
GetOrMakeFont(const nsAString& aName, const gfxFontStyle *aStyle)
{
    nsRefPtr<gfxFont> font = gfxFontCache::GetCache()->Lookup(aName, aStyle);
    if (!font) {
        font = new gfxPangoFont(aName, aStyle);
        if (!font)
            return nsnull;
        gfxFontCache::GetCache()->AddNew(font);
    }
    gfxFont *f = nsnull;
    font.swap(f);
    return static_cast<gfxPangoFont *>(f);
}

gfxPangoFontGroup::gfxPangoFontGroup (const nsAString& families,
                                      const gfxFontStyle *aStyle)
    : gfxFontGroup(families, aStyle)
{
    g_type_init();

    nsStringArray familyArray;

    // Leave non-existing fonts in the list so that fontconfig can get the
    // best match.
    ForEachFontInternal(families, aStyle->langGroup, PR_TRUE, PR_FALSE,
                        FontCallback, &familyArray);

    // Construct a string suitable for fontconfig
    nsAutoString fcFamilies;
    if (familyArray.Count()) {
        int i = 0;
        while (1) {
            fcFamilies.Append(*familyArray[i]);
            ++i;
            if (i >= familyArray.Count())
                break;
            fcFamilies.Append(NS_LITERAL_STRING(","));
        }
    }
    else {
        // XXX If there are no fonts, we should use dummy family.
        // Pango will resolve from this.
        // behdad: yep, looks good.
        // printf("%s(%s)\n", NS_ConvertUTF16toUTF8(families).get(),
        //                    aStyle->langGroup.get());
        fcFamilies.Append(NS_LITERAL_STRING("sans-serif"));
    }

    nsRefPtr<gfxPangoFont> font = GetOrMakeFont(fcFamilies, &mStyle);
    if (font) {
        mFonts.AppendElement(font);
    }
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

gfxPangoFont::gfxPangoFont(const nsAString &aName,
                           const gfxFontStyle *aFontStyle)
    : gfxFont(aName, aFontStyle),
      mPangoFont(nsnull), mCairoFont(nsnull),
      mHasMetrics(PR_FALSE), mAdjustedSize(0)
{
}

// key for locating a gfxPangoFont corresponding to a PangoFont
static GQuark GetFontQuark()
{
    // Not using g_quark_from_static_string() because this module may be
    // unloaded (which would leave a dangling pointer).  Using
    // g_quark_from_string() instead, which creates a small shutdown leak.
    static GQuark quark = g_quark_from_string("moz-gfxFont");
    return quark;
}

gfxPangoFont::gfxPangoFont(PangoFont *aPangoFont, const nsAString &aName,
                           const gfxFontStyle *aFontStyle)
    : gfxFont(aName, aFontStyle),
      mPangoFont(aPangoFont), mCairoFont(nsnull),
      mHasMetrics(PR_FALSE), mAdjustedSize(aFontStyle->size)
{
    g_object_ref(mPangoFont);
    g_object_set_qdata(G_OBJECT(mPangoFont), GetFontQuark(), this);
}

gfxPangoFont::~gfxPangoFont()
{
    if (mPangoFont) {
        if (g_object_get_qdata(G_OBJECT(mPangoFont), GetFontQuark()) == this)
            g_object_set_qdata(G_OBJECT(mPangoFont), GetFontQuark(), NULL);
        g_object_unref(mPangoFont);
    }

    if (mCairoFont)
        cairo_scaled_font_destroy(mCairoFont);
}

/* static */ void
gfxPangoFont::Shutdown()
{
    gfxPangoFontCache::Shutdown();

    // This just cleans up memory used by Pango's caches and may cause an
    // assert and crash in cairo (Bug 399556), so only do this when we care
    // about cleaning up memory on shutdown.
#if defined(DEBUG) || defined(NS_BUILD_REFCNT_LOGGING) || defined(NS_TRACE_MALLOC)
    PangoFontMap *fontmap = pango_cairo_font_map_get_default ();
    if (PANGO_IS_FC_FONT_MAP (fontmap))
        pango_fc_font_map_shutdown (PANGO_FC_FONT_MAP (fontmap));
#endif
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

static PRUint8
PangoStyleToThebesStyle (PangoStyle aPangoStyle)
{
    if (aPangoStyle == PANGO_STYLE_ITALIC)
        return FONT_STYLE_ITALIC;
    if (aPangoStyle == FONT_STYLE_OBLIQUE)
        return FONT_STYLE_OBLIQUE;

    return FONT_STYLE_NORMAL;
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
    static const int fcWeightLookup[10] = {
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
    static const int fcWeights[5] = {
        349,
        449,
        649,
        749,
        900
    };

    return (PangoWeight)fcWeights[fcWeight];
}

/* Note this doesn't check sizeAdjust */
static PangoFontDescription *
NewPangoFontDescription(const nsAString &aName, const gfxFontStyle *aFontStyle)
{
    PangoFontDescription *fontDesc = pango_font_description_new();

    pango_font_description_set_family(fontDesc,
                                      NS_ConvertUTF16toUTF8(aName).get());
    pango_font_description_set_absolute_size(fontDesc,
                                             aFontStyle->size * PANGO_SCALE);
    pango_font_description_set_style(fontDesc,
                                     ThebesStyleToPangoStyle(aFontStyle));
    pango_font_description_set_weight(fontDesc,
                                      ThebesStyleToPangoWeight(aFontStyle));
    return fontDesc;
}

/**
 * The following gfxPangoFonts are accessed from the PangoFont, not from the
 * gfxFontCache hash table.  The gfxFontCache hash table is keyed by desired
 * family and style, whereas here we only know actual family and style.  There
 * may be more than one of these fonts with the same family and style, but
 * different PangoFont and actual font face.
 * 
 * The point of this is to record the exact font face for gfxTextRun glyph
 * indices.  The style of this font does not necessarily represent the exact
 * gfxFontStyle used to build the text run.  Notably, the language is not
 * recorded, but is used for GetMetrics().aveCharWidth.  However, the font
 * that should be used for aveCharWidth is gfxPangoFontGroup::GetFontAt(0),
 * which is not constructed here.
 */

/* static */
already_AddRefed<gfxPangoFont>
gfxPangoFont::GetOrMakeFont(PangoFont *aPangoFont)
{
    gfxPangoFont *font = static_cast<gfxPangoFont*>
        (g_object_get_qdata(G_OBJECT(aPangoFont), GetFontQuark()));

    if (!font) {
        // pango_font_describe_with_absolute_size requires Pango-1.14
        PangoFontDescription *desc = pango_font_describe(aPangoFont);

        PangoFcFont *fcfont = PANGO_FC_FONT(aPangoFont);
        double size;
        if (FcPatternGetDouble(fcfont->font_pattern, FC_PIXEL_SIZE, 0, &size)
            != FcResultMatch)
            size = pango_font_description_get_size(desc) / FLOAT_PANGO_SCALE;

        // Shouldn't actually need to take too much care about the correct
        // family or style, as size is the only thing expected to be
        // important.
        PRUint8 style =
            PangoStyleToThebesStyle(pango_font_description_get_style(desc));
        PRUint16 weight = pango_font_description_get_weight(desc);
        NS_NAMED_LITERAL_CSTRING(langGroup, "x-unicode");
        gfxFontStyle fontStyle(style, weight, size, langGroup, 0.0,
                               PR_TRUE, PR_FALSE);

        // (The PangoFontDescription owns the family string)
        const char *family = pango_font_description_get_family(desc);
        font = new gfxPangoFont(aPangoFont,
                                NS_ConvertUTF8toUTF16(family), &fontStyle);

        pango_font_description_free(desc);
        if (!font)
            return nsnull;

        // Do not add this font to the gfxFontCache hash table as this may not
        // be the PangoFont that fontconfig chooses for this style.
    }
    NS_ADDREF(font);
    return font;
}

static PangoFont*
LoadPangoFont(PangoContext *aPangoCtx, const PangoFontDescription *aPangoFontDesc)
{
    gfxPangoFontCache *cache = gfxPangoFontCache::GetPangoFontCache();
    if (!cache)
        return nsnull; // Error
    PangoFont* pangoFont = cache->Get(aPangoFontDesc);
    if (!pangoFont) {
        pangoFont = pango_context_load_font(aPangoCtx, aPangoFontDesc);
        if (pangoFont) {
            cache->Put(aPangoFontDesc, pangoFont);
        }
    }
    return pangoFont;
}

void
gfxPangoFont::RealizePangoFont()
{
    // already realized?
    if (mPangoFont)
        return;

    PangoFontDescription *pangoFontDesc =
        NewPangoFontDescription(mName, GetStyle());

    PangoContext *pangoCtx = gdk_pango_context_get();

    if (!GetStyle()->langGroup.IsEmpty()) {
        PangoLanguage *lang = GetPangoLanguage(GetStyle()->langGroup);
        if (lang)
            pango_context_set_language(pangoCtx, lang);
    }

    mPangoFont = LoadPangoFont(pangoCtx, pangoFontDesc);

    gfxFloat size = GetStyle()->size;
    // Checking mPangoFont to avoid infinite recursion through GetCharSize
    if (size != 0.0 && GetStyle()->sizeAdjust != 0.0 && mPangoFont) {
        // Could try xHeight from TrueType/OpenType fonts.
        gfxSize isz, lsz;
        GetCharSize('x', isz, lsz);
        if (isz.height != 0.0) {
            gfxFloat aspect = isz.height / size;
            size = GetStyle()->GetAdjustedSize(aspect);

            pango_font_description_set_absolute_size(pangoFontDesc,
                                                     size * PANGO_SCALE);
            g_object_unref(mPangoFont);
            mPangoFont = LoadPangoFont(pangoCtx, pangoFontDesc);
        }
    }

    NS_ASSERTION(mHasMetrics == PR_FALSE, "metrics will be invalid...");
    mAdjustedSize = size;
    if (!g_object_get_qdata(G_OBJECT(mPangoFont), GetFontQuark()))
        g_object_set_qdata(G_OBJECT(mPangoFont), GetFontQuark(), this);

    if (pangoFontDesc)
        pango_font_description_free(pangoFontDesc);
    if (pangoCtx)
        g_object_unref(pangoCtx);
}

void
gfxPangoFont::GetCharSize(char aChar, gfxSize& aInkSize, gfxSize& aLogSize,
                          PRUint32 *aGlyphID)
{
    if (NS_UNLIKELY(GetStyle()->size == 0.0)) {
        if (aGlyphID)
            *aGlyphID = 0;
        aInkSize.SizeTo(0.0, 0.0);
        aLogSize.SizeTo(0.0, 0.0);
        return;
    }

    // XXXkt: Why not use pango_font_get_glyph_extents?  This function isn't
    // currently being used for characters likely to involve glyph clusters.
    // I don't think pango_shape will fallback to other fonts.
    PangoAnalysis analysis;
    // Initialize new fields, gravity and flags in pango 1.16
    // (or padding in 1.14).
    // Use memset instead of { 0 } aggregate initialization or placement new
    // default initialization so that padding (which may have meaning in other
    // versions) is initialized.
    memset(&analysis, 0, sizeof(analysis));
    analysis.font = GetPangoFont();
    analysis.language = pango_language_from_string("en");
    analysis.shape_engine = pango_font_find_shaper(analysis.font, analysis.language, aChar);

    PangoGlyphString *glstr = pango_glyph_string_new();
    pango_shape (&aChar, 1, &analysis, glstr);

    if (aGlyphID) {
        *aGlyphID = 0;
        if (glstr->num_glyphs == 1) {
            PangoGlyph glyph = glstr->glyphs[0].glyph;
            if (!IS_MISSING_GLYPH(glyph) && !IS_EMPTY_GLYPH(glyph)) {
                *aGlyphID = glyph;
            }
        }
    }

    PangoRectangle ink_rect, log_rect;
    pango_glyph_string_extents(glstr, analysis.font, &ink_rect, &log_rect);

    aInkSize.width = ink_rect.width / FLOAT_PANGO_SCALE;
    aInkSize.height = ink_rect.height / FLOAT_PANGO_SCALE;

    aLogSize.width = log_rect.width / FLOAT_PANGO_SCALE;
    aLogSize.height = log_rect.height / FLOAT_PANGO_SCALE;

    pango_glyph_string_free(glstr);
}

// rounding and truncation functions for a Freetype fixed point number 
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

    /* pango_cairo case; try to get all the metrics from pango itself */
    PangoFont *font;
    PangoFontMetrics *pfm;
    if (NS_LIKELY(GetStyle()->size > 0.0)) {
        font = GetPangoFont(); // RealizePangoFont is called here.
        PangoLanguage *lang = GetPangoLanguage(GetStyle()->langGroup);
        // If lang is NULL, Pango will measure a string of many languages,
        // which will require many FcFontSorts, but we don't want to go to
        // that much trouble.
        // pango_language_get_default() is available from Pango-1.16.
        if (!lang)
            lang = pango_language_from_string(setlocale(LC_CTYPE, NULL));

        pfm = pango_font_get_metrics(font, lang);
    } else {
        // Don't ask pango when the font-size is zero since it causes
        // some versions of libpango to crash (bug 404112).
        font = NULL;
        pfm = NULL;
    }

    if (NS_LIKELY(pfm)) {
        mMetrics.maxAscent =
            pango_font_metrics_get_ascent(pfm) / FLOAT_PANGO_SCALE;

        mMetrics.maxDescent =
            pango_font_metrics_get_descent(pfm) / FLOAT_PANGO_SCALE;

        // This is used for the width of text input elements so be liberal
        // rather than conservative in the estimate.
        mMetrics.aveCharWidth =
            PR_MAX(pango_font_metrics_get_approximate_char_width(pfm),
                   pango_font_metrics_get_approximate_digit_width(pfm))
            / FLOAT_PANGO_SCALE;

        mMetrics.underlineOffset =
            pango_font_metrics_get_underline_position(pfm) / FLOAT_PANGO_SCALE;

        mMetrics.underlineSize =
            pango_font_metrics_get_underline_thickness(pfm) / FLOAT_PANGO_SCALE;

        mMetrics.strikeoutOffset =
            pango_font_metrics_get_strikethrough_position(pfm) / FLOAT_PANGO_SCALE;

        mMetrics.strikeoutSize =
            pango_font_metrics_get_strikethrough_thickness(pfm) / FLOAT_PANGO_SCALE;

        // We're going to overwrite this below if we have a FT_Face
        // (which we normally should have...).
        mMetrics.maxAdvance = mMetrics.aveCharWidth;
    } else {
        mMetrics.maxAscent = 0.0;
        mMetrics.maxDescent = 0.0;
        mMetrics.aveCharWidth = 0.0;
        mMetrics.underlineOffset = -1.0;
        mMetrics.underlineSize = 0.0;
        mMetrics.strikeoutOffset = 0.0;
        mMetrics.strikeoutSize = 0.0;
        mMetrics.maxAdvance = 0.0;
    }

    // ??
    mMetrics.emHeight = mAdjustedSize;

    gfxFloat lineHeight = mMetrics.maxAscent + mMetrics.maxDescent;
    if (lineHeight > mMetrics.emHeight)
        mMetrics.externalLeading = lineHeight - mMetrics.emHeight;
    else
        mMetrics.externalLeading = 0;
    mMetrics.internalLeading = 0;

    mMetrics.maxHeight = lineHeight;

    mMetrics.emAscent = lineHeight > 0.0 ?
        mMetrics.maxAscent * mMetrics.emHeight / lineHeight : 0.0;
    mMetrics.emDescent = mMetrics.emHeight - mMetrics.emAscent;

    gfxSize isz, lsz;
    GetCharSize(' ', isz, lsz, &mSpaceGlyph);
    mMetrics.spaceWidth = lsz.width;
    GetCharSize('x', isz, lsz);
    mMetrics.xHeight = isz.height;

    FT_Face face = NULL;
    if (pfm && PANGO_IS_FC_FONT(font))
        face = pango_fc_font_lock_face(PANGO_FC_FONT(font));

    if (face) {
        mMetrics.maxAdvance = face->size->metrics.max_advance / 64.0; // 26.6

        float val;

        TT_OS2 *os2 = (TT_OS2 *) FT_Get_Sfnt_Table(face, ft_sfnt_os2);
    
        if (os2 && os2->ySuperscriptYOffset) {
            val = CONVERT_DESIGN_UNITS_TO_PIXELS(os2->ySuperscriptYOffset,
                                                 face->size->metrics.y_scale);
            mMetrics.superscriptOffset = PR_MAX(1, val);
        } else {
            mMetrics.superscriptOffset = mMetrics.xHeight;
        }
    
        if (os2 && os2->ySubscriptYOffset) {
            val = CONVERT_DESIGN_UNITS_TO_PIXELS(os2->ySubscriptYOffset,
                                                 face->size->metrics.y_scale);
            // some fonts have the incorrect sign. 
            val = (val < 0) ? -val : val;
            mMetrics.subscriptOffset = PR_MAX(1, val);
        } else {
            mMetrics.subscriptOffset = mMetrics.xHeight;
        }

        pango_fc_font_unlock_face(PANGO_FC_FONT(font));
    } else {
        mMetrics.superscriptOffset = mMetrics.xHeight;
        mMetrics.subscriptOffset = mMetrics.xHeight;
    }

    SanitizeMetrics(&mMetrics, PR_FALSE);

#if 0
    //    printf("font name: %s %f %f\n", NS_ConvertUTF16toUTF8(mName).get(), GetStyle()->size, mAdjustedSize);
    //    printf ("pango font %s\n", pango_font_description_to_string (pango_font_describe (font)));

    fprintf (stderr, "Font: %s\n", NS_ConvertUTF16toUTF8(mName).get());
    fprintf (stderr, "    emHeight: %f emAscent: %f emDescent: %f\n", mMetrics.emHeight, mMetrics.emAscent, mMetrics.emDescent);
    fprintf (stderr, "    maxAscent: %f maxDescent: %f\n", mMetrics.maxAscent, mMetrics.maxDescent);
    fprintf (stderr, "    internalLeading: %f externalLeading: %f\n", mMetrics.externalLeading, mMetrics.internalLeading);
    fprintf (stderr, "    spaceWidth: %f aveCharWidth: %f xHeight: %f\n", mMetrics.spaceWidth, mMetrics.aveCharWidth, mMetrics.xHeight);
    fprintf (stderr, "    uOff: %f uSize: %f stOff: %f stSize: %f suOff: %f suSize: %f\n", mMetrics.underlineOffset, mMetrics.underlineSize, mMetrics.strikeoutOffset, mMetrics.strikeoutSize, mMetrics.superscriptOffset, mMetrics.subscriptOffset);
#endif

    if (pfm)
        pango_font_metrics_unref(pfm);

    mHasMetrics = PR_TRUE;
    return mMetrics;
}

PRUint32
gfxPangoFont::GetGlyph(const PRUint32 aChar)
{
    // Ensure that null character should be missing.
    if (aChar == 0)
        return 0;
    return pango_fc_font_get_glyph(PANGO_FC_FONT(GetPangoFont()), aChar);
}

nsString
gfxPangoFont::GetUniqueName()
{
    PangoFont *font = GetPangoFont();
    PangoFontDescription *desc = pango_font_describe(font);
    pango_font_description_unset_fields (desc, PANGO_FONT_MASK_SIZE);
    char *str = pango_font_description_to_string(desc);
    pango_font_description_free (desc);

    nsString result;
    CopyUTF8toUTF16(str, result);
    g_free(str);
    return result;
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
                               const Parameters *aParams, PRUint32 aFlags)
{
    NS_ASSERTION(aFlags & TEXT_IS_8BIT, "8bit should have been set");
    gfxTextRun *run = gfxTextRun::Create(aParams, aString, aLength, this, aFlags);
    if (!run)
        return nsnull;

    PRBool isRTL = run->IsRightToLeft();
    if ((aFlags & TEXT_IS_ASCII) && !isRTL) {
        // We don't need to send an override character here, the characters must be all LTR
        const gchar *utf8Chars = reinterpret_cast<const gchar*>(aString);
        InitTextRun(run, utf8Chars, aLength, 0, PR_TRUE);
    } else {
        // this is really gross...
        const char *chars = reinterpret_cast<const char*>(aString);
        NS_ConvertASCIItoUTF16 unicodeString(chars, aLength);
        nsCAutoString utf8;
        PRInt32 headerLen = AppendDirectionalIndicatorUTF8(isRTL, utf8);
        AppendUTF16toUTF8(unicodeString, utf8);
        InitTextRun(run, utf8.get(), utf8.Length(), headerLen, PR_TRUE);
    }
    run->FetchGlyphExtents(aParams->mContext);
    return run;
}

#if defined(ENABLE_FAST_PATH_8BIT)
PRBool
gfxPangoFontGroup::CanTakeFastPath(PRUint32 aFlags)
{
    // Can take fast path only if OPTIMIZE_SPEED is set and IS_RTL isn't.
    // We need to always use Pango for RTL text, in case glyph mirroring is
    // required.
    PRBool speed = aFlags & gfxTextRunFactory::TEXT_OPTIMIZE_SPEED;
    PRBool isRTL = aFlags & gfxTextRunFactory::TEXT_IS_RTL;
    return speed && !isRTL && PANGO_IS_FC_FONT(GetFontAt(0)->GetPangoFont());
}
#endif

gfxTextRun *
gfxPangoFontGroup::MakeTextRun(const PRUnichar *aString, PRUint32 aLength,
                               const Parameters *aParams, PRUint32 aFlags)
{
    gfxTextRun *run = gfxTextRun::Create(aParams, aString, aLength, this, aFlags);
    if (!run)
        return nsnull;

    run->RecordSurrogates(aString);

    nsCAutoString utf8;
    PRInt32 headerLen = AppendDirectionalIndicatorUTF8(run->IsRightToLeft(), utf8);
    AppendUTF16toUTF8(Substring(aString, aString + aLength), utf8);
    PRBool is8Bit = PR_FALSE;

#if defined(ENABLE_FAST_PATH_8BIT)
    if (CanTakeFastPath(aFlags)) {
        PRUint32 allBits = 0;
        PRUint32 i;
        for (i = 0; i < aLength; ++i) {
            allBits |= aString[i];
        }
        is8Bit = (allBits & 0xFF00) == 0;
    }
#endif
    InitTextRun(run, utf8.get(), utf8.Length(), headerLen, is8Bit);
    run->FetchGlyphExtents(aParams->mContext);
    return run;
}

void
gfxPangoFontGroup::InitTextRun(gfxTextRun *aTextRun, const gchar *aUTF8Text,
                               PRUint32 aUTF8Length, PRUint32 aUTF8HeaderLength,
                               PRBool aTake8BitPath)
{
#if defined(ENABLE_FAST_PATH_ALWAYS)
    CreateGlyphRunsFast(aTextRun, aUTF8Text + aUTF8HeaderLength, aUTF8Length - aUTF8HeaderLength);
#else
#if defined(ENABLE_FAST_PATH_8BIT)
    if (aTake8BitPath && CanTakeFastPath(aTextRun->GetFlags())) {
        nsresult rv = CreateGlyphRunsFast(aTextRun, aUTF8Text + aUTF8HeaderLength, aUTF8Length - aUTF8HeaderLength);
        if (NS_SUCCEEDED(rv))
            return;
    }
#endif

    CreateGlyphRunsItemizing(aTextRun, aUTF8Text, aUTF8Length, aUTF8HeaderLength);
#endif
}

static cairo_scaled_font_t*
CreateScaledFont(cairo_t *aCR, cairo_matrix_t *aCTM, PangoFont *aPangoFont)
{
// XXX this needs to also check that we're using system cairo
// otherwise this causes bad problems.
#if 0
//#if PANGO_VERSION_CHECK(1,17,5)
    // Lets just use pango_cairo_font_get_scaled_font() for now.  it's only
    // available in pango 1.17.x though :(
    return cairo_scaled_font_reference (pango_cairo_font_get_scaled_font (PANGO_CAIRO_FONT (aPangoFont)));
#else
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
    FcMatrix *fcMatrix;
    if (FcPatternGetMatrix(fcfont->font_pattern, FC_MATRIX, 0, &fcMatrix) == FcResultMatch)
        cairo_matrix_init(&fontMatrix, fcMatrix->xx, -fcMatrix->yx, -fcMatrix->xy, fcMatrix->yy, 0, 0);
    else
        cairo_matrix_init_identity(&fontMatrix);
    cairo_matrix_scale(&fontMatrix, size, size);
    cairo_font_options_t *fontOptions = cairo_font_options_create();
    cairo_get_font_options(aCR, fontOptions);
    cairo_scaled_font_t *scaledFont =
        cairo_scaled_font_create(face, &fontMatrix, aCTM, fontOptions);
    cairo_font_options_destroy(fontOptions);
    cairo_font_face_destroy(face);
    NS_ASSERTION(cairo_scaled_font_status(scaledFont) == CAIRO_STATUS_SUCCESS,
                 "Failed to create scaled font");
    return scaledFont;
#endif
}

PRBool
gfxPangoFont::SetupCairoFont(gfxContext *aContext)
{
    cairo_t *cr = aContext->GetCairo();
    cairo_matrix_t currentCTM;
    cairo_get_matrix(cr, &currentCTM);

    if (mCairoFont) {
        // Need to validate that its CTM is OK
        cairo_matrix_t fontCTM;
        cairo_scaled_font_get_ctm(mCairoFont, &fontCTM);
        if (fontCTM.xx != currentCTM.xx || fontCTM.yy != currentCTM.yy ||
            fontCTM.xy != currentCTM.xy || fontCTM.yx != currentCTM.yx) {
            // Just recreate it from scratch, simplest way
            cairo_scaled_font_destroy(mCairoFont);
            mCairoFont = nsnull;
        }
    }
    if (!mCairoFont) {
        mCairoFont = CreateScaledFont(cr, &currentCTM, GetPangoFont());
    }
    if (cairo_scaled_font_status(mCairoFont) != CAIRO_STATUS_SUCCESS) {
        // Don't cairo_set_scaled_font as that would propagate the error to
        // the cairo_t, precluding any further drawing.
        return PR_FALSE;
    }
    cairo_set_scaled_font(cr, mCairoFont);
    return PR_TRUE;
}

static void
SetupClusterBoundaries(gfxTextRun* aTextRun, const gchar *aUTF8, PRUint32 aUTF8Length,
                       PRUint32 aUTF16Offset, PangoAnalysis *aAnalysis)
{
    if (aTextRun->GetFlags() & gfxTextRunFactory::TEXT_IS_8BIT) {
        // 8-bit text doesn't have clusters.
        // XXX is this true in all languages???
        // behdad: don't think so.  Czech for example IIRC has a
        // 'ch' grapheme.
        return;
    }

    // Pango says "the array of PangoLogAttr passed in must have at least N+1
    // elements, if there are N characters in the text being broken".
    // Could use g_utf8_strlen(aUTF8, aUTF8Length) + 1 but the memory savings
    // may not be worth the call.
    nsAutoTArray<PangoLogAttr,2000> buffer;
    if (!buffer.AppendElements(aUTF8Length + 1))
        return;

    pango_break(aUTF8, aUTF8Length, aAnalysis,
                buffer.Elements(), buffer.Length());

    const gchar *p = aUTF8;
    const gchar *end = aUTF8 + aUTF8Length;
    const PangoLogAttr *attr = buffer.Elements();
    gfxTextRun::CompressedGlyph g;
    while (p < end) {
        if (!attr->is_cursor_position) {
            aTextRun->SetGlyphs(aUTF16Offset, g.SetComplex(PR_FALSE, PR_TRUE, 0), nsnull);
        }
        ++aUTF16Offset;
        
        gunichar ch = g_utf8_get_char(p);
        NS_ASSERTION(ch != 0, "Shouldn't have NUL in pango_break");
        NS_ASSERTION(!IS_SURROGATE(ch), "Shouldn't have surrogates in UTF8");
        if (ch >= 0x10000) {
            ++aUTF16Offset;
        }
        // We produced this utf8 so we don't need to worry about malformed stuff
        p = g_utf8_next_char(p);
        ++attr;
    }
}

static PRInt32
ConvertPangoToAppUnits(PRInt32 aCoordinate, PRUint32 aAppUnitsPerDevUnit)
{
    PRInt64 v = (PRInt64(aCoordinate)*aAppUnitsPerDevUnit + PANGO_SCALE/2)/PANGO_SCALE;
    return PRInt32(v);
}

/**
 * Given a run of Pango glyphs that should be treated as a single
 * cluster/ligature, store them in the textrun at the appropriate character
 * and set the other characters involved to be ligature/cluster continuations
 * as appropriate.
 */ 
static nsresult
SetGlyphsForCharacterGroup(const PangoGlyphInfo *aGlyphs, PRUint32 aGlyphCount,
                           gfxTextRun *aTextRun,
                           const gchar *aUTF8, PRUint32 aUTF8Length,
                           PRUint32 *aUTF16Offset,
                           PangoGlyphUnit aOverrideSpaceWidth)
{
    PRUint32 utf16Offset = *aUTF16Offset;
    PRUint32 textRunLength = aTextRun->GetLength();
    const PRUint32 appUnitsPerDevUnit = aTextRun->GetAppUnitsPerDevUnit();
    const gfxTextRun::CompressedGlyph *charGlyphs = aTextRun->GetCharacterGlyphs();

    // Override the width of a space, but only for spaces that aren't
    // clustered with something else (like a freestanding diacritical mark)
    PangoGlyphUnit width = aGlyphs[0].geometry.width;
    if (aOverrideSpaceWidth && aUTF8[0] == ' ' &&
        (utf16Offset + 1 == textRunLength ||
         charGlyphs[utf16Offset].IsClusterStart())) {
        width = aOverrideSpaceWidth;
    }
    PRInt32 advance = ConvertPangoToAppUnits(width, appUnitsPerDevUnit);

    gfxTextRun::CompressedGlyph g;
    PRBool atClusterStart = aTextRun->IsClusterStart(utf16Offset);
    // See if we fit in the compressed area.
    if (aGlyphCount == 1 && advance >= 0 && atClusterStart &&
        aGlyphs[0].geometry.x_offset == 0 &&
        aGlyphs[0].geometry.y_offset == 0 &&
        gfxTextRun::CompressedGlyph::IsSimpleAdvance(advance) &&
        gfxTextRun::CompressedGlyph::IsSimpleGlyphID(aGlyphs[0].glyph)) {
        aTextRun->SetSimpleGlyph(utf16Offset,
                                 g.SetSimpleGlyph(advance, aGlyphs[0].glyph));
    } else {
        nsAutoTArray<gfxTextRun::DetailedGlyph,10> detailedGlyphs;
        if (!detailedGlyphs.AppendElements(aGlyphCount))
            return NS_ERROR_OUT_OF_MEMORY;

        PRUint32 i;
        for (i = 0; i < aGlyphCount; ++i) {
            gfxTextRun::DetailedGlyph *details = &detailedGlyphs[i];
            PRUint32 j = (aTextRun->IsRightToLeft()) ? aGlyphCount - 1 - i : i; 
            const PangoGlyphInfo &glyph = aGlyphs[j];
            details->mGlyphID = glyph.glyph;
            NS_ASSERTION(details->mGlyphID == glyph.glyph,
                         "Seriously weird glyph ID detected!");
            details->mAdvance =
                ConvertPangoToAppUnits(glyph.geometry.width,
                                       appUnitsPerDevUnit);
            details->mXOffset =
                float(glyph.geometry.x_offset)*appUnitsPerDevUnit/PANGO_SCALE;
            details->mYOffset =
                float(glyph.geometry.y_offset)*appUnitsPerDevUnit/PANGO_SCALE;
        }
        g.SetComplex(atClusterStart, PR_TRUE, aGlyphCount);
        aTextRun->SetGlyphs(utf16Offset, g, detailedGlyphs.Elements());
    }

    // Check for ligatures and set *aUTF16Offset.
    const gchar *p = aUTF8;
    const gchar *end = aUTF8 + aUTF8Length;
    while (1) {
        // Skip the CompressedGlyph that we have added, but check if the
        // character was supposed to be ignored. If it's supposed to be ignored,
        // overwrite the textrun entry with an invisible missing-glyph.
        gunichar ch = g_utf8_get_char(p);
        NS_ASSERTION(!IS_SURROGATE(ch), "surrogates should not appear in UTF8");
        if (ch >= 0x10000) {
            // Skip surrogate
            ++utf16Offset;
        }
        NS_ASSERTION(!gfxFontGroup::IsInvalidChar(PRUnichar(ch)),
                     "Invalid character detected");
        ++utf16Offset;

        // We produced this UTF8 so we don't need to worry about malformed stuff
        p = g_utf8_next_char(p);
        if (p >= end)
            break;

        if (utf16Offset >= textRunLength) {
            NS_ERROR("Someone has added too many glyphs!");
            return NS_ERROR_FAILURE;
        }

        g.SetComplex(aTextRun->IsClusterStart(utf16Offset), PR_FALSE, 0);
        aTextRun->SetGlyphs(utf16Offset, g, nsnull);
    }
    *aUTF16Offset = utf16Offset;
    return NS_OK;
}

nsresult
gfxPangoFontGroup::SetGlyphs(gfxTextRun *aTextRun, gfxPangoFont *aFont,
                             const gchar *aUTF8, PRUint32 aUTF8Length,
                             PRUint32 *aUTF16Offset, PangoGlyphString *aGlyphs,
                             PangoGlyphUnit aOverrideSpaceWidth,
                             PRBool aAbortOnMissingGlyph)
{
    gint numGlyphs = aGlyphs->num_glyphs;
    PangoGlyphInfo *glyphs = aGlyphs->glyphs;
    const gint *logClusters = aGlyphs->log_clusters;
    // We cannot make any assumptions about the order of glyph clusters
    // provided by pango_shape (see 375864), so we work through the UTF8 text
    // and process the glyph clusters in logical order.

    // logGlyphs is like an inverse of logClusters.  For each UTF8 byte:
    //   >= 0 indicates that the byte is first in a cluster and
    //        gives the position of the starting glyph for the cluster.
    //     -1 indicates that the byte does not start a cluster.
    nsAutoTArray<gint,2000> logGlyphs;
    if (!logGlyphs.AppendElements(aUTF8Length + 1))
        return NS_ERROR_OUT_OF_MEMORY;
    PRUint32 utf8Index = 0;
    for(; utf8Index < aUTF8Length; ++utf8Index)
        logGlyphs[utf8Index] = -1;
    logGlyphs[aUTF8Length] = numGlyphs;

    gint lastCluster = -1; // != utf8Index
    for (gint glyphIndex = 0; glyphIndex < numGlyphs; ++glyphIndex) {
        gint thisCluster = logClusters[glyphIndex];
        if (thisCluster != lastCluster) {
            lastCluster = thisCluster;
            NS_ASSERTION(0 <= thisCluster && thisCluster < gint(aUTF8Length),
                         "garbage from pango_shape - this is bad");
            logGlyphs[thisCluster] = glyphIndex;
        }
    }

    PRUint32 utf16Offset = *aUTF16Offset;
    PRUint32 textRunLength = aTextRun->GetLength();
    utf8Index = 0;
    // The next glyph cluster in logical order. 
    gint nextGlyphClusterStart = logGlyphs[utf8Index];
    NS_ASSERTION(nextGlyphClusterStart >= 0, "No glyphs! - NUL in string?");
    while (utf8Index < aUTF8Length) {
        if (utf16Offset >= textRunLength) {
          NS_ERROR("Someone has added too many glyphs!");
          return NS_ERROR_FAILURE;
        }
        gint glyphClusterStart = nextGlyphClusterStart;
        // Find the utf8 text associated with this glyph cluster.
        PRUint32 clusterUTF8Start = utf8Index;
        // Check we are consistent with pango_break data.
        NS_ASSERTION(aTextRun->GetCharacterGlyphs()->IsClusterStart(),
                     "Glyph cluster not aligned on character cluster.");
        do {
            ++utf8Index;
            nextGlyphClusterStart = logGlyphs[utf8Index];
        } while (nextGlyphClusterStart < 0);
        const gchar *clusterUTF8 = &aUTF8[clusterUTF8Start];
        PRUint32 clusterUTF8Length = utf8Index - clusterUTF8Start;

        PRBool haveMissingGlyph = PR_FALSE;
        gint glyphIndex = glyphClusterStart;

        // It's now unncecessary to do NUL handling here.
        do {
            if (IS_EMPTY_GLYPH(glyphs[glyphIndex].glyph)) {
                // The zero width characters return empty glyph ID at
                // shaping, we should override it.
                glyphs[glyphIndex].glyph = aFont->GetGlyph(' ');
                glyphs[glyphIndex].geometry.width = 0;
            } else if (IS_MISSING_GLYPH(glyphs[glyphIndex].glyph)) {
                // Does pango ever provide more than one glyph in the
                // cluster if there is a missing glyph?
                // behdad: yes
                haveMissingGlyph = PR_TRUE;
            }
            glyphIndex++;
        } while (glyphIndex < numGlyphs && 
                 logClusters[glyphIndex] == gint(clusterUTF8Start));

        if (haveMissingGlyph && aAbortOnMissingGlyph)
            return NS_ERROR_FAILURE;

        nsresult rv;
        if (haveMissingGlyph) {
            rv = SetMissingGlyphs(aTextRun, clusterUTF8, clusterUTF8Length,
                             &utf16Offset);
        } else {
            rv = SetGlyphsForCharacterGroup(&glyphs[glyphClusterStart],
                                            glyphIndex - glyphClusterStart,
                                            aTextRun,
                                            clusterUTF8, clusterUTF8Length,
                                            &utf16Offset, aOverrideSpaceWidth);
        }
        NS_ENSURE_SUCCESS(rv,rv);
    }
    *aUTF16Offset = utf16Offset;
    return NS_OK;
}

nsresult
gfxPangoFontGroup::SetMissingGlyphs(gfxTextRun *aTextRun,
                                    const gchar *aUTF8, PRUint32 aUTF8Length,
                                    PRUint32 *aUTF16Offset)
{
    PRUint32 utf16Offset = *aUTF16Offset;
    PRUint32 textRunLength = aTextRun->GetLength();
    for (PRUint32 index = 0; index < aUTF8Length;) {
        if (utf16Offset >= textRunLength) {
            NS_ERROR("Someone has added too many glyphs!");
            break;
        }
        gunichar ch = g_utf8_get_char(aUTF8 + index);
        aTextRun->SetMissingGlyph(utf16Offset, ch);

        ++utf16Offset;
        NS_ASSERTION(!IS_SURROGATE(ch), "surrogates should not appear in UTF8");
        if (ch >= 0x10000)
            ++utf16Offset;
        // We produced this UTF8 so we don't need to worry about malformed stuff
        index = g_utf8_next_char(aUTF8 + index) - aUTF8;
    }

    *aUTF16Offset = utf16Offset;
    return NS_OK;
}

#if defined(ENABLE_FAST_PATH_8BIT) || defined(ENABLE_FAST_PATH_ALWAYS)
nsresult
gfxPangoFontGroup::CreateGlyphRunsFast(gfxTextRun *aTextRun,
                                       const gchar *aUTF8, PRUint32 aUTF8Length)
{
    const gchar *p = aUTF8;
    gfxPangoFont *font = GetFontAt(0);
    PangoFont *pangofont = font->GetPangoFont();
    PangoFcFont *fcfont = PANGO_FC_FONT (pangofont);
    PRUint32 utf16Offset = 0;
    gfxTextRun::CompressedGlyph g;
    const PRUint32 appUnitsPerDevUnit = aTextRun->GetAppUnitsPerDevUnit();

    aTextRun->AddGlyphRun(font, 0);

    while (p < aUTF8 + aUTF8Length) {
        // glib-2.12.9: "If p does not point to a valid UTF-8 encoded
        // character, results are undefined." so it is not easy to assert that
        // aUTF8 in fact points to UTF8 data but asserting
        // g_unichar_validate(ch) may be mildly useful.
        gunichar ch = g_utf8_get_char(p);
        p = g_utf8_next_char(p);
        
        if (ch == 0) {
            // treat this null byte as a missing glyph. Pango
            // doesn't create glyphs for these, not even missing-glyphs.
            aTextRun->SetMissingGlyph(utf16Offset, 0);
        } else {
            NS_ASSERTION(!IsInvalidChar(ch), "Invalid char detected");
            FT_UInt glyph = pango_fc_font_get_glyph (fcfont, ch);
            if (!glyph)                  // character not in font,
                return NS_ERROR_FAILURE; // fallback to CreateGlyphRunsItemizing

            PangoRectangle rect;
            pango_font_get_glyph_extents (pangofont, glyph, NULL, &rect);

            PRInt32 advance = PANGO_PIXELS (rect.width * appUnitsPerDevUnit);
            if (advance >= 0 &&
                gfxTextRun::CompressedGlyph::IsSimpleAdvance(advance) &&
                gfxTextRun::CompressedGlyph::IsSimpleGlyphID(glyph)) {
                aTextRun->SetSimpleGlyph(utf16Offset,
                                         g.SetSimpleGlyph(advance, glyph));
            } else {
                gfxTextRun::DetailedGlyph details;
                details.mGlyphID = glyph;
                NS_ASSERTION(details.mGlyphID == glyph,
                             "Seriously weird glyph ID detected!");
                details.mAdvance = advance;
                details.mXOffset = 0;
                details.mYOffset = 0;
                g.SetComplex(aTextRun->IsClusterStart(utf16Offset), PR_TRUE, 1);
                aTextRun->SetGlyphs(utf16Offset, g, &details);
            }

            NS_ASSERTION(!IS_SURROGATE(ch), "Surrogates shouldn't appear in UTF8");
            if (ch >= 0x10000) {
                // This character is a surrogate pair in UTF16
                ++utf16Offset;
            }
        }

        ++utf16Offset;
    }
    return NS_OK;
}
#endif

static void
SetBaseFont(PangoContext *aContext, PangoFont *aBaseFont)
{
    PangoFontMap *fontmap = pango_context_get_font_map(aContext);
    if (GFX_IS_PANGO_FONT_MAP(fontmap)) {
        // Update the base font in the gfxPangoFontMap
        GFX_PANGO_FONT_MAP(fontmap)->SetBaseFont(aBaseFont);
    }
    else if (aBaseFont) {
        // Change the font map to record and activate the base font
        fontmap = gfxPangoFontMap::NewFontMap(fontmap, aBaseFont);
        pango_context_set_font_map(aContext, fontmap);
        g_object_unref(fontmap);
    }
}

void 
gfxPangoFontGroup::CreateGlyphRunsItemizing(gfxTextRun *aTextRun,
                                            const gchar *aUTF8, PRUint32 aUTF8Length,
                                            PRUint32 aUTF8HeaderLen)
{

    PangoContext *context = gdk_pango_context_get();

    PangoFontDescription *fontDesc =
        NewPangoFontDescription(GetFontAt(0)->GetName(), GetStyle());
    if (GetStyle()->sizeAdjust != 0.0) {
        gfxFloat size = 
            static_cast<gfxPangoFont*>(GetFontAt(0))->GetAdjustedSize();
        pango_font_description_set_absolute_size(fontDesc, size * PANGO_SCALE);
    }

    pango_context_set_font_description(context, fontDesc);
    pango_font_description_free(fontDesc);

    PangoLanguage *lang = GetPangoLanguage(GetStyle()->langGroup);

    // we should set this to null if we don't have a text language from the page...
    // except that we almost always have something...
    pango_context_set_language(context, lang);

    // Set the primary font for consistent font selection for common
    // characters, but use the default Pango behavior
    // (selecting generic fonts from the script of the characters)
    // in two situations:
    //   1. When we don't have a language to make a good choice for the
    //      primary font.
    //   2. For system fonts, use the default Pango behavior
    //      to give consistency with other apps.
    if (lang && !GetStyle()->systemFont) {
        SetBaseFont(context, GetFontAt(0)->GetPangoFont());
    }

    PangoDirection dir = aTextRun->IsRightToLeft() ? PANGO_DIRECTION_RTL : PANGO_DIRECTION_LTR;
    GList *items = pango_itemize_with_base_dir(context, dir, aUTF8, 0, aUTF8Length, nsnull, nsnull);

    PRUint32 utf16Offset = 0;
    PRBool isRTL = aTextRun->IsRightToLeft();
    GList *pos = items;
    PangoGlyphString *glyphString = pango_glyph_string_new();
    if (!glyphString)
        goto out; // OOM

    for (; pos && pos->data; pos = pos->next) {
        PangoItem *item = (PangoItem *)pos->data;
        NS_ASSERTION(isRTL == item->analysis.level % 2, "RTL assumption mismatch");

        PRUint32 offset = item->offset;
        PRUint32 length = item->length;
        if (offset < aUTF8HeaderLen) {
            if (offset + length <= aUTF8HeaderLen)
                continue;

            length -= aUTF8HeaderLen - offset;
            offset = aUTF8HeaderLen;
        }

        /* look up the gfxPangoFont from the PangoFont */
        nsRefPtr<gfxPangoFont> font =
            gfxPangoFont::GetOrMakeFont(item->analysis.font);

        nsresult rv = aTextRun->AddGlyphRun(font, utf16Offset, PR_TRUE);
        if (NS_FAILED(rv)) {
            NS_ERROR("AddGlyphRun Failed");
            goto out;
        }

        PRUint32 spaceWidth = NS_lround(font->GetMetrics().spaceWidth * FLOAT_PANGO_SCALE);

        const gchar *p = aUTF8 + offset;
        const gchar *end = p + length;
        while (p < end) {
            if (*p == 0) {
                aTextRun->SetMissingGlyph(utf16Offset, 0);
                ++p;
                ++utf16Offset;
                continue;
            }

            // It's necessary to loop over pango_shape as it treats
            // NULs as string terminators
            const gchar *text = p;
            do {
                ++p;
            } while(p < end && *p != 0);
            gint len = p - text;

            pango_shape(text, len, &item->analysis, glyphString);
            SetupClusterBoundaries(aTextRun, text, len, utf16Offset, &item->analysis);
            SetGlyphs(aTextRun, font, text, len, &utf16Offset, glyphString, spaceWidth, PR_FALSE);
        }
    }

    aTextRun->SortGlyphRuns();

out:
    if (glyphString)
        pango_glyph_string_free(glyphString);

    for (pos = items; pos; pos = pos->next)
        pango_item_free((PangoItem *)pos->data);

    if (items)
        g_list_free(items);

    g_object_unref(context);
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

    return nsnull;
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

#define NUM_PANGO_ALL_LANG_GROUPS (G_N_ELEMENTS (PangoAllLangGroup))

gfxPangoFontCache::gfxPangoFontCache()
{
    mPangoFonts.Init(500);
}

gfxPangoFontCache::~gfxPangoFontCache()
{
}

void
gfxPangoFontCache::Put(const PangoFontDescription *aFontDesc, PangoFont *aPangoFont)
{
    if (mPangoFonts.Count() > 5000)
        mPangoFonts.Clear();
    PRUint32 key = pango_font_description_hash(aFontDesc);
    gfxPangoFontWrapper *value = new gfxPangoFontWrapper(aPangoFont);
    if (!value)
        return;
    mPangoFonts.Put(key, value);
}

PangoFont*
gfxPangoFontCache::Get(const PangoFontDescription *aFontDesc)
{
    PRUint32 key = pango_font_description_hash(aFontDesc);
    gfxPangoFontWrapper *value;
    if (!mPangoFonts.Get(key, &value))
        return nsnull;
    PangoFont *font = value->Get();
    g_object_ref(font);
    return font;
}

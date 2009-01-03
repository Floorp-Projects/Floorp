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

#include "nsMathUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsILanguageAtomService.h"

#include "gfxContext.h"
#include "gfxPlatformGtk.h"
#include "gfxPangoFonts.h"
#include "gfxFontconfigUtils.h"
#include "gfxUserFontSet.h"

#include <freetype/tttables.h>

#include <cairo.h>
#include <cairo-ft.h>

#include <fontconfig/fcfreetype.h>
#include <pango/pango.h>
#include <pango/pangofc-fontmap.h>

#ifdef MOZ_WIDGET_GTK2
#include <gdk/gdk.h>
#endif

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

// Same as pango_units_from_double from Pango 1.16 (but not in older versions)
int moz_pango_units_from_double(double d) {
    return NS_lround(d * FLOAT_PANGO_SCALE);
}

static PangoLanguage *GuessPangoLanguage(const nsACString& aLangGroup);

static cairo_scaled_font_t *CreateScaledFont(FcPattern *aPattern);

static PangoFontMap *gPangoFontMap;
static PangoFontMap *GetPangoFontMap();

static FT_Library gFTLibrary;
static nsILanguageAtomService* gLangService;

NS_SPECIALIZE_TEMPLATE
class nsAutoRefTraits<PangoFont> : public gfxGObjectRefTraits<PangoFont> { };

NS_SPECIALIZE_TEMPLATE
class nsAutoRefTraits<PangoCoverage>
    : public nsPointerRefTraits<PangoCoverage> {
public:
    static void Release(PangoCoverage *aPtr) { pango_coverage_unref(aPtr); }
    static void AddRef(PangoCoverage *aPtr) { pango_coverage_ref(aPtr); }
};


// FC_FAMILYLANG and FC_FULLNAME were introduced in fontconfig-2.2.97
// and so fontconfig-2.3.0 (2005).
#ifndef FC_FAMILYLANG
#define FC_FAMILYLANG "familylang"
#endif
#ifndef FC_FULLNAME
#define FC_FULLNAME "fullname"
#endif

// Rounding and truncation functions for a FreeType fixed point number 
// (FT26Dot6) stored in a 32bit integer with high 26 bits for the integer
// part and low 6 bits for the fractional part. 
#define FLOAT_FROM_26_6(x) ((x) / 64.0)
#define FLOAT_FROM_16_16(x) ((x) / 65536.0)
#define ROUND_26_6_TO_INT(x) ((x) >= 0 ?  ((32 + (x)) >> 6) \
                                       : -((32 - (x)) >> 6))
// aScale is intended for a 16.16 x/y_scale of an FT_Size_Metrics
static inline FT_Long
ScaleRoundDesignUnits(FT_Short aDesignMetric, FT_Fixed aScale)
{
    FT_Long fixed26dot6 = FT_MulFix(aDesignMetric, aScale);
    return ROUND_26_6_TO_INT(fixed26dot6);
}

// A namespace for @font-face family names in FcPatterns so that fontconfig
// aliases do not pick up families from @font-face rules and so that
// fontconfig rules can distinguish between web fonts and platform fonts.
// http://lists.freedesktop.org/archives/fontconfig/2008-November/003037.html
#define FONT_FACE_FAMILY_PREFIX "@font-face:"

/**
 * gfxFcFontEntry:
 *
 * An abstract class for objects in a gfxUserFontSet that can provide
 * FcPattern* handles to fonts.
 *
 * Separate implementations of this class support local fonts from src:local()
 * and web fonts from src:url().
 */

// There is a one-to-one correspondence between gfxFcFontEntry objects and
// @font-face rules, but sometimes a one-to-many correspondence between font
// entries and font patterns.
//
// http://www.w3.org/TR/2002/WD-css3-webfonts-20020802#font-descriptions
// provided a font-size descriptor to specify the sizes supported by the face,
// but the "Editor's Draft 27 June 2008"
// http://dev.w3.org/csswg/css3-fonts/#font-resources does not provide such a
// descriptor, and Mozilla does not recognize such a descriptor.
//
// Font face names used in src:local() also do not usually specify a size.
//
// PCF format fonts have each size in a different file, and each of these
// files is referenced by its own pattern, but really these are each
// different sizes of one face with one name.
//
// Multiple patterns in an entry also effectively deals with a set of
// PostScript Type 1 font files that all have the same face name but are in
// several files because of the limit on the number of glyphs in a Type 1 font
// file.  (e.g. Computer Modern.)

class gfxFcFontEntry : public gfxFontEntry {
public:
    const nsTArray< nsCountedRef<FcPattern> >& GetPatterns()
    {
        return mPatterns;
    }

protected:
    gfxFcFontEntry(const gfxProxyFontEntry &aProxyEntry)
        // store the family name
        : gfxFontEntry(aProxyEntry.mFamily->Name())
    {
        mItalic = aProxyEntry.mItalic;
        mWeight = aProxyEntry.mWeight;
        mStretch = aProxyEntry.mStretch;
    }

    // Helper function to change a pattern so that it matches the CSS style
    // descriptors and so gets properly sorted in font selection.  This also
    // avoids synthetic style effects being added by the renderer when the
    // style of the font itself does not match the descriptor provided by the
    // author.
    void AdjustPatternToCSS(FcPattern *aPattern);

    nsAutoTArray<nsCountedRef<FcPattern>,1> mPatterns;
};

void
gfxFcFontEntry::AdjustPatternToCSS(FcPattern *aPattern)
{
    int fontWeight = -1;
    FcPatternGetInteger(aPattern, FC_WEIGHT, 0, &fontWeight);
    int cssWeight = gfxFontconfigUtils::FcWeightForBaseWeight(mWeight);
    if (cssWeight != fontWeight) {
        FcPatternDel(aPattern, FC_WEIGHT);
        FcPatternAddInteger(aPattern, FC_WEIGHT, cssWeight);
    }

    int fontSlant;
    FcResult res = FcPatternGetInteger(aPattern, FC_SLANT, 0, &fontSlant);
    // gfxFontEntry doesn't understand the difference between oblique
    // and italic.
    if (res != FcResultMatch ||
        IsItalic() != (fontSlant != FC_SLANT_ROMAN)) {
        FcPatternDel(aPattern, FC_SLANT);
        FcPatternAddInteger(aPattern, FC_SLANT,
                            IsItalic() ? FC_SLANT_OBLIQUE : FC_SLANT_ROMAN);
    }

    // Ensure that there is a fullname property (if there is a family
    // property) so that fontconfig rules can identify the real name of the
    // font, because the family property will be replaced.
    FcChar8 *unused;
    if (FcPatternGetString(aPattern,
                           FC_FULLNAME, 0, &unused) == FcResultNoMatch) {
        nsCAutoString fullname;
        if (gfxFontconfigUtils::GetFullnameFromFamilyAndStyle(aPattern,
                                                              &fullname)) {
            FcPatternAddString(aPattern, FC_FULLNAME,
                               gfxFontconfigUtils::ToFcChar8(fullname));
        }
    }

    nsCAutoString family;
    family.Append(FONT_FACE_FAMILY_PREFIX);
    AppendUTF16toUTF8(Name(), family);

    FcPatternDel(aPattern, FC_FAMILY);
    FcPatternDel(aPattern, FC_FAMILYLANG);
    FcPatternAddString(aPattern, FC_FAMILY,
                       gfxFontconfigUtils::ToFcChar8(family));
}

/**
 * gfxLocalFcFontEntry:
 *
 * An implementation of gfxFcFontEntry for local fonts from src:local().
 */

class gfxLocalFcFontEntry : public gfxFcFontEntry {
public:
    gfxLocalFcFontEntry(const gfxProxyFontEntry &aProxyEntry,
                        const nsTArray< nsCountedRef<FcPattern> >& aPatterns)
        : gfxFcFontEntry(aProxyEntry)
    {
        if (!mPatterns.SetCapacity(aPatterns.Length()))
            return; // OOM

        for (PRUint32 i = 0; i < aPatterns.Length(); ++i) {
            FcPattern *pattern = FcPatternDuplicate(aPatterns.ElementAt(i));
            if (!pattern)
                return; // OOM

            AdjustPatternToCSS(pattern);

            mPatterns.AppendElement();
            mPatterns[i].own(pattern);
        }
    }
};

/**
 * gfxDownloadedFcFontEntry:
 *
 * An implementation of gfxFcFontEntry for web fonts from src:url().
 */

class gfxDownloadedFcFontEntry : public gfxFcFontEntry {
public:
    // This takes ownership of the face.
    gfxDownloadedFcFontEntry(const gfxProxyFontEntry &aProxyEntry,
                             nsISupports *aLoader, FT_Face aFace)
        : gfxFcFontEntry(aProxyEntry), mLoader(aLoader), mFace(aFace)
    {
        NS_PRECONDITION(aFace != NULL, "aFace is NULL!");
        InitPattern();
    }

    virtual ~gfxDownloadedFcFontEntry();

    // Returns a PangoCoverage owned by the FontEntry.  The caller must add a
    // reference if it wishes to keep the PangoCoverage longer than the
    // lifetime of the FontEntry.
    PangoCoverage *GetPangoCoverage();

protected:
    virtual void InitPattern();

    // mLoader holds a reference to memory used by mFace.
    nsCOMPtr<nsISupports> mLoader;
    FT_Face mFace;
    // mPangoCoverage is the charset property of the pattern translated to a
    // format that Pango understands.  A reference is kept here so that it can
    // be shared by multiple PangoFonts (of different sizes).
    nsAutoRef<PangoCoverage> mPangoCoverage;
};

// A property for recording gfxDownloadedFcFontEntrys on FcPatterns.
static const char *kFontEntryFcProp = "-moz-font-entry";

static FcBool AddDownloadedFontEntry(FcPattern *aPattern,
                                     gfxDownloadedFcFontEntry *aFontEntry)
{
    FcValue value;
    value.type = FcTypeFTFace; // void* field of union
    value.u.f = aFontEntry;

    return FcPatternAdd(aPattern, kFontEntryFcProp, value, FcFalse);
}

static FcBool DelDownloadedFontEntry(FcPattern *aPattern)
{
    return FcPatternDel(aPattern, kFontEntryFcProp);
}

static gfxDownloadedFcFontEntry *GetDownloadedFontEntry(FcPattern *aPattern)
{
    FcValue value;
    if (FcPatternGet(aPattern, kFontEntryFcProp, 0, &value) != FcResultMatch)
        return nsnull;

    if (value.type != FcTypeFTFace) {
        NS_NOTREACHED("Wrong type for -moz-font-entry font property");
        return nsnull;
    }

    return static_cast<gfxDownloadedFcFontEntry*>(value.u.f);
}

gfxDownloadedFcFontEntry::~gfxDownloadedFcFontEntry()
{
    if (mPatterns.Length() != 0) {
        // Remove back reference to this font entry and the face in case
        // anyone holds a reference to the pattern.
        NS_ASSERTION(mPatterns.Length() == 1,
                     "More than one pattern in gfxDownloadedFcFontEntry!");
        DelDownloadedFontEntry(mPatterns[0]);
        FcPatternDel(mPatterns[0], FC_FT_FACE);
    }
    FT_Done_Face(mFace);
}

typedef FcPattern* (*QueryFaceFunction)(const FT_Face face,
                                        const FcChar8 *file, int id,
                                        FcBlanks *blanks);

static QueryFaceFunction
GetFcFreeTypeQueryFace()
{
    PRLibrary *lib = nsnull;
    PRFuncPtr result =
        PR_FindFunctionSymbolAndLibrary("FcFreeTypeQueryFace", &lib);
    if (lib) {
        PR_UnloadLibrary(lib);
    }

    return reinterpret_cast<QueryFaceFunction>(result);
}

void
gfxDownloadedFcFontEntry::InitPattern()
{
    static QueryFaceFunction sQueryFacePtr = GetFcFreeTypeQueryFace();
    FcPattern *pattern;

    // FcFreeTypeQueryFace is the same function used to construct patterns for
    // system fonts and so is the preferred function to use for this purpose.
    // This will set up the langset property, which helps with sorting, and
    // the foundry, fullname, and fontversion properties, which properly
    // identify the font to fontconfig rules.  However, FcFreeTypeQueryFace is
    // available only from fontconfig-2.4.2 (December 2006).  (CentOS 5.0 has
    // fontconfig-2.4.1.)
    if (sQueryFacePtr) {
        // The "file" argument cannot be NULL (in fontconfig-2.6.0 at least).
        // The dummy file passed here is removed below.
        //
        // When fontconfig scans the system fonts, FcConfigGetBlanks(NULL) is
        // passed as the "blanks" argument, which provides that unexpectedly
        // blank glyphs are elided.  Here, however, we pass NULL for "blanks",
        // effectively assuming that, if the font has a blank glyph, then the
        // author intends any associated character to be rendered blank.
        pattern =
            (*sQueryFacePtr)(mFace, gfxFontconfigUtils::ToFcChar8(""), 0, NULL);
        if (!pattern)
            // Either OOM, or fontconfig chose to skip this font because it
            // has "no encoded characters", which I think means "BDF and PCF
            // fonts which are not in Unicode (or the effectively equivalent
            // ISO Latin-1) encoding".
            return;

        // These properties don't make sense for this face without a file.
        FcPatternDel(pattern, FC_FILE);
        FcPatternDel(pattern, FC_INDEX);

    } else {
        // Do the minimum necessary to construct a pattern for sorting.

        // FC_CHARSET is vital to determine which characters are supported.
        nsAutoRef<FcCharSet> charset(FcFreeTypeCharSet(mFace, NULL));
        // If there are no characters then assume we don't know how to read
        // this font.
        if (!charset || FcCharSetCount(charset) == 0)
            return;

        pattern = FcPatternCreate();
        FcPatternAddCharSet(pattern, FC_CHARSET, charset);

        // FC_PIXEL_SIZE can be important for font selection of fixed-size
        // fonts.
        if (!(mFace->face_flags & FT_FACE_FLAG_SCALABLE)) {
            for (FT_Int i = 0; i < mFace->num_fixed_sizes; ++i) {
#if HAVE_FT_BITMAP_SIZE_Y_PPEM
                double size = FLOAT_FROM_26_6(mFace->available_sizes[i].y_ppem);
#else
                double size = mFace->available_sizes[i].height;
#endif
                FcPatternAddDouble (pattern, FC_PIXEL_SIZE, size);
            }

            // Not sure whether this is important;
            // imitating FcFreeTypeQueryFace:
            FcPatternAddBool (pattern, FC_ANTIALIAS, FcFalse);
        }

        // Setting up the FC_LANGSET property is very difficult with the APIs
        // available prior to FcFreeTypeQueryFace.  Having no FC_LANGSET
        // property seems better than having a property with an empty LangSet.
        // With no FC_LANGSET property, fontconfig sort functions will
        // consider this face to have the same priority as (otherwise equal)
        // faces that have support for the primary requested language, but
        // will not consider any language to have been satisfied (and so will
        // continue to look for a face with language support in fallback
        // fonts).
    }

    AdjustPatternToCSS(pattern);

    FcPatternAddFTFace(pattern, FC_FT_FACE, mFace);
    AddDownloadedFontEntry(pattern, this);

    // There is never more than one pattern
    mPatterns.AppendElement();
    mPatterns[0].own(pattern);
}

static PangoCoverage *NewPangoCoverage(FcPattern *aFont)
{
    // This uses g_slice_alloc which will abort on OOM rather than return NULL.
    PangoCoverage *coverage = pango_coverage_new();

    FcCharSet *charset;
    if (FcPatternGetCharSet(aFont, FC_CHARSET, 0, &charset) != FcResultMatch)
        return coverage; // empty

    FcChar32 base;
    FcChar32 map[FC_CHARSET_MAP_SIZE];
    FcChar32 next;
    for (base = FcCharSetFirstPage(charset, map, &next);
         base != FC_CHARSET_DONE;
         base = FcCharSetNextPage(charset, map, &next)) {
        for (PRUint32 i = 0; i < FC_CHARSET_MAP_SIZE; ++i) {
            PRUint32 offset = 0;
            FcChar32 bitmap = map[i];
            for (; bitmap; bitmap >>= 1) {
                if (bitmap & 1) {
                    pango_coverage_set(coverage, base + offset,
                                       PANGO_COVERAGE_EXACT);
                }
                ++offset;
            }
            base += 32;
        }
    }
    return coverage;
}

PangoCoverage *
gfxDownloadedFcFontEntry::GetPangoCoverage()
{
    NS_ASSERTION(mPatterns.Length() != 0,
                 "Can't get coverage without a pattern!");
    if (!mPangoCoverage) {
        mPangoCoverage.own(NewPangoCoverage(mPatterns[0]));
    }
    return mPangoCoverage;
}

/*
 * gfxFcFont
 *
 * This is a gfxFont implementation using a CAIRO_FONT_TYPE_FT
 * cairo_scaled_font created from an FcPattern.
 */

class gfxFcFont : public gfxFont {
public:
    virtual ~gfxFcFont ();
    static already_AddRefed<gfxFcFont> GetOrMakeFont(FcPattern *aPattern);

    virtual const gfxFont::Metrics& GetMetrics();

    virtual nsString GetUniqueName();

    // Get the glyphID of a space
    virtual PRUint32 GetSpaceGlyph() {
        NS_ASSERTION(GetStyle()->size != 0,
                     "forgot to short-circuit a text run with zero-sized font?");
        GetMetrics();
        return mSpaceGlyph;
    }

    cairo_scaled_font_t *CairoScaledFont() { return mCairoFont; }
    void GetGlyphExtents(PRUint32 aGlyph, cairo_text_extents_t* aExtents);

protected:
    cairo_scaled_font_t *mCairoFont;

    PRUint32 mSpaceGlyph;
    Metrics mMetrics;
    PRPackedBool mHasMetrics;

    gfxFcFont(cairo_scaled_font_t *aCairoFont,
              gfxFontEntry *aFontEntry, const gfxFontStyle *aFontStyle);

    virtual PRBool SetupCairoFont(gfxContext *aContext);

    // key for locating a gfxFcFont corresponding to a cairo_scaled_font
    static cairo_user_data_key_t sGfxFontKey;
};

class LockedFTFace {
public:
    LockedFTFace(gfxFcFont *aFont)
        : mGfxFont(aFont),
          mFace(cairo_ft_scaled_font_lock_face(aFont->CairoScaledFont()))
    {
    }

    ~LockedFTFace()
    {
        if (mFace) {
            cairo_ft_scaled_font_unlock_face(mGfxFont->CairoScaledFont());
        }
    }

    FT_Face get()
    {
        return mFace;
    }

    /**
     * Get extents for a simple character representable by a single glyph.
     * The return value is the glyph id of that glyph or zero if no such glyph
     * exists.  aExtents is only set when this returns a non-zero glyph id.
     */
    PRUint32 GetCharExtents(char aChar, cairo_text_extents_t* aExtents);

    void GetMetrics(gfxFont::Metrics* aMetrics, PRUint32* aSpaceGlyph);

private:
    nsRefPtr<gfxFcFont> mGfxFont;
    FT_Face mFace;
};

/**
 * gfxPangoFcFont:
 *
 * An implementation of PangoFcFont that wraps a gfxFont so that it can be
 * passed to PangoRenderFc shapers.
 *
 * Many of these will be created for pango_itemize, but most will only be
 * tested for coverage of individual characters (and sometimes not even that).
 * Therefore the gfxFont is only constructed if and when needed.
 */

#define GFX_TYPE_PANGO_FC_FONT              (gfx_pango_fc_font_get_type())
#define GFX_PANGO_FC_FONT(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GFX_TYPE_PANGO_FC_FONT, gfxPangoFcFont))
#define GFX_IS_PANGO_FC_FONT(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GFX_TYPE_PANGO_FC_FONT))

/* static */
GType gfx_pango_fc_font_get_type (void);

#define GFX_PANGO_FC_FONT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GFX_TYPE_PANGO_FC_FONT, gfxPangoFcFontClass))
#define GFX_IS_PANGO_FC_FONT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GFX_TYPE_PANGO_FC_FONT))
#define GFX_PANGO_FC_FONT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GFX_TYPE_PANGO_FC_FONT, gfxPangoFcFontClass))

// This struct is POD so that it can be used as a GObject.
struct gfxPangoFcFont {
    PangoFcFont parent_instance;

    FcPattern *mRequestedPattern;
    PangoCoverage *mCoverage;
    gfxFcFont *mGfxFont;

    static nsReturnRef<PangoFont>
    NewFont(FcPattern *aRequestedPattern, FcPattern *aFontPattern)
    {
        // A pattern is needed for pango_fc_font_finalize.
        //
        // Adding a ref to the requested pattern and one of fontconfig's
        // patterns uses much less memory than using the fully resolved
        // pattern here, and saves calling FcFontRenderPrepare when the
        // PangoFont is only tested for character coverage.
        //
        // Normally the is_hinted field of the PangoFcFont is set based on the
        // FC_HINTING property on the pattern at construction, but this
        // property is not known until after RenderPrepare.  is_hinted is used
        // by pango_fc_font_kern_glyphs, which is sometimes used by
        // pango_ot_buffer_output.  is_hinted will be set when the gfxFont is
        // constructed for PangoFcFont::lock_face.
        gfxPangoFcFont *font = static_cast<gfxPangoFcFont*>
            (g_object_new(GFX_TYPE_PANGO_FC_FONT,
                          "pattern", aFontPattern, NULL));

        // Save the requested pattern for FcFontRenderPrepare.
        FcPatternReference(aRequestedPattern);
        font->mRequestedPattern = aRequestedPattern;

        // PangoFcFont::get_coverage wants an FcFontMap.  (PangoFcFontMap
        // usually sets this after calling PangoFcFontMap::create_font().)
        PangoFcFont *fc_font = &font->parent_instance;
        fc_font->fontmap = GetPangoFontMap();
        g_object_ref(fc_font->fontmap);

        return nsReturnRef<PangoFont>(PANGO_FONT(font));
    }

    static gfxFcFont *GfxFont(gfxPangoFcFont *self)
    {
        if (!self->mGfxFont) {
            PangoFcFont *fc_font = &self->parent_instance;

            if (NS_LIKELY(self->mRequestedPattern)) {
                // Created with gfxPangoFcFont::NewFont()
                nsAutoRef<FcPattern> renderPattern
                    (FcFontRenderPrepare(NULL, self->mRequestedPattern,
                                         fc_font->font_pattern));
                if (!renderPattern)
                    return nsnull;

                FcBool hinting = FcTrue;
                FcPatternGetBool(renderPattern, FC_HINTING, 0, &hinting);
                fc_font->is_hinted = hinting;

                // is_transformed does not appear to be used anywhere but looks
                // like it should be set.
                FcMatrix *matrix;
                FcResult result = FcPatternGetMatrix(renderPattern,
                                                     FC_MATRIX, 0, &matrix);
                fc_font->is_transformed =
                    result == FcResultMatch &&
                    (matrix->xy != 0.0 || matrix->yx != 0.0 ||
                     matrix->xx != 1.0 || matrix->yy != 1.0);

                self->mGfxFont = gfxFcFont::GetOrMakeFont(renderPattern).get();
                if (self->mGfxFont) {
                    // Finished with the requested pattern
                    FcPatternDestroy(self->mRequestedPattern);
                    self->mRequestedPattern = NULL;
                }

            } else {
                // Created with gfxPangoFontMap::create_font()
                self->mGfxFont =
                    gfxFcFont::GetOrMakeFont(fc_font->font_pattern).get();
            }                
        }
        return self->mGfxFont;
    }

    static cairo_scaled_font_t *CairoFont(gfxPangoFcFont *self)
    {
        return gfxPangoFcFont::GfxFont(self)->CairoScaledFont();
    }
};

struct gfxPangoFcFontClass {
    PangoFcFontClass parent_class;
};

G_DEFINE_TYPE (gfxPangoFcFont, gfx_pango_fc_font, PANGO_TYPE_FC_FONT)

static void
gfx_pango_fc_font_init(gfxPangoFcFont *font)
{
}


static void
gfx_pango_fc_font_finalize(GObject *object)
{
    gfxPangoFcFont *self = GFX_PANGO_FC_FONT(object);

    if (self->mRequestedPattern)
        FcPatternDestroy(self->mRequestedPattern);
    if (self->mCoverage)
        pango_coverage_unref(self->mCoverage);
    NS_IF_RELEASE(self->mGfxFont);

    // The parent class removes the reference to parent_instance->fontmap.

    G_OBJECT_CLASS(gfx_pango_fc_font_parent_class)->finalize(object);
}

static PangoCoverage *
gfx_pango_fc_font_get_coverage(PangoFont *font, PangoLanguage *lang)
{
    gfxPangoFcFont *self = GFX_PANGO_FC_FONT(font);

    // The coverage is requested often enough that it is worth holding a
    // reference on the font.
    if (!self->mCoverage) {
        FcPattern *pattern = self->parent_instance.font_pattern;
        gfxDownloadedFcFontEntry *downloadedFontEntry =
            GetDownloadedFontEntry(pattern);
        // The parent class implementation requires the font pattern to have
        // a file and caches results against that filename.  This is not
        // suitable for web fonts.
        if (!downloadedFontEntry) {
            self->mCoverage =
                PANGO_FONT_CLASS(gfx_pango_fc_font_parent_class)->
                get_coverage(font, lang);
        } else {
            self->mCoverage =
                pango_coverage_ref(downloadedFontEntry->GetPangoCoverage());
        }
    }

    return pango_coverage_ref(self->mCoverage);
}

static PangoFontDescription *
gfx_pango_fc_font_describe(PangoFont *font)
{
    gfxPangoFcFont *self = GFX_PANGO_FC_FONT(font);
    PangoFcFont *fcFont = &self->parent_instance;
    PangoFontDescription *result =
        pango_font_description_copy(fcFont->description);

    gfxFcFont *gfxFont = gfxPangoFcFont::GfxFont(self);
    if (gfxFont) {
        double pixelsize = gfxFont->GetStyle()->size;
        double dpi = gfxPlatformGtk::DPI();
        gint size = moz_pango_units_from_double(pixelsize * dpi / 72.0);
        pango_font_description_set_size(result, size);
    }
    return result;
}

static PangoFontDescription *
gfx_pango_fc_font_describe_absolute(PangoFont *font)
{
    gfxPangoFcFont *self = GFX_PANGO_FC_FONT(font);
    PangoFcFont *fcFont = &self->parent_instance;
    PangoFontDescription *result =
        pango_font_description_copy(fcFont->description);

    gfxFcFont *gfxFont = gfxPangoFcFont::GfxFont(self);
    if (gfxFont) {
        double size = gfxFont->GetStyle()->size * PANGO_SCALE;
        pango_font_description_set_absolute_size(result, size);
    }
    return result;
}

static void
gfx_pango_fc_font_get_glyph_extents(PangoFont *font, PangoGlyph glyph,
                                    PangoRectangle *ink_rect,
                                    PangoRectangle *logical_rect)
{
    gfxPangoFcFont *self = GFX_PANGO_FC_FONT(font);
    gfxFcFont *gfxFont = gfxPangoFcFont::GfxFont(self);

    if (IS_MISSING_GLYPH(glyph)) {
        const gfxFont::Metrics& metrics = gfxFont->GetMetrics();

        PangoRectangle rect;
        rect.x = 0;
        rect.y = moz_pango_units_from_double(-metrics.maxAscent);
        rect.width = moz_pango_units_from_double(metrics.aveCharWidth);
        rect.height = moz_pango_units_from_double(metrics.maxHeight);
        if (ink_rect) {
            *ink_rect = rect;
        }
        if (logical_rect) {
            *logical_rect = rect;
        }
        return;
    }

    if (logical_rect) {
        // logical_rect.width is possibly used by pango_ot_buffer_output (used
        // by many shapers) and used by fallback_engine_shape (possibly used
        // by pango_shape and pango_itemize when no glyphs are found).  I
        // doubt the other fields will be used but we won't have any way to
        // detecting if they are so we'd better set them.
        const gfxFont::Metrics& metrics = gfxFont->GetMetrics();
        logical_rect->y = moz_pango_units_from_double(-metrics.maxAscent);
        logical_rect->height = moz_pango_units_from_double(metrics.maxHeight);
    }

    cairo_text_extents_t extents;
    if (IS_EMPTY_GLYPH(glyph)) {
        new (&extents) cairo_text_extents_t(); // zero
    } else {
        gfxFont->GetGlyphExtents(glyph, &extents);
    }

    if (ink_rect) {
        ink_rect->x = moz_pango_units_from_double(extents.x_bearing);
        ink_rect->y = moz_pango_units_from_double(extents.y_bearing);
        ink_rect->width = moz_pango_units_from_double(extents.width);
        ink_rect->height = moz_pango_units_from_double(extents.height);
    }
    if (logical_rect) {
        logical_rect->x = 0;
        logical_rect->width = moz_pango_units_from_double(extents.x_advance);
    }
}

static PangoFontMetrics *
gfx_pango_fc_font_get_metrics(PangoFont *font, PangoLanguage *language)
{
    gfxPangoFcFont *self = GFX_PANGO_FC_FONT(font);

    // This uses g_slice_alloc which will abort on OOM rather than return NULL.
    PangoFontMetrics *result = pango_font_metrics_new();

    gfxFcFont *gfxFont = gfxPangoFcFont::GfxFont(self);
    if (gfxFont) {
        const gfxFont::Metrics& metrics = gfxFont->GetMetrics();

        result->ascent = moz_pango_units_from_double(metrics.maxAscent);
        result->descent = moz_pango_units_from_double(metrics.maxDescent);
        result->approximate_char_width =
            moz_pango_units_from_double(metrics.aveCharWidth);
        result->approximate_digit_width =
            moz_pango_units_from_double(metrics.zeroOrAveCharWidth);
        result->underline_position =
            moz_pango_units_from_double(metrics.underlineOffset);
        result->underline_thickness =
            moz_pango_units_from_double(metrics.underlineSize);
        result->strikethrough_position =
            moz_pango_units_from_double(metrics.strikeoutOffset);
        result->strikethrough_thickness =
            moz_pango_units_from_double(metrics.strikeoutSize);
    }
    return result;
}

static FT_Face
gfx_pango_fc_font_lock_face(PangoFcFont *font)
{
    gfxPangoFcFont *self = GFX_PANGO_FC_FONT(font);
    return cairo_ft_scaled_font_lock_face(gfxPangoFcFont::CairoFont(self));
}

static void
gfx_pango_fc_font_unlock_face(PangoFcFont *font)
{
    gfxPangoFcFont *self = GFX_PANGO_FC_FONT(font);
    cairo_ft_scaled_font_unlock_face(gfxPangoFcFont::CairoFont(self));
}

static void
gfx_pango_fc_font_class_init (gfxPangoFcFontClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    PangoFontClass *font_class = PANGO_FONT_CLASS (klass);
    PangoFcFontClass *fc_font_class = PANGO_FC_FONT_CLASS (klass);

    object_class->finalize = gfx_pango_fc_font_finalize;

    font_class->get_coverage = gfx_pango_fc_font_get_coverage;
    // describe is called on errors in pango_shape.
    font_class->describe = gfx_pango_fc_font_describe;
    font_class->get_glyph_extents = gfx_pango_fc_font_get_glyph_extents;
    // get_metrics and describe_absolute are not likely to be used but
    //   implemented because the class makes them available.
    font_class->get_metrics = gfx_pango_fc_font_get_metrics;
    font_class->describe_absolute = gfx_pango_fc_font_describe_absolute;
    // font_class->find_shaper,get_font_map are inherited from PangoFcFontClass

    // fc_font_class->has_char,get_glyph are inherited
    fc_font_class->lock_face = gfx_pango_fc_font_lock_face;
    fc_font_class->unlock_face = gfx_pango_fc_font_unlock_face;
}

/**
 * Recording a gfxPangoFontGroup on a PangoContext
 */

static GQuark GetFontGroupQuark()
{
    // Not using g_quark_from_static_string() because this module may be
    // unloaded (which would leave a dangling pointer).  Using
    // g_quark_from_string() instead, which creates a small shutdown leak.
    static GQuark quark = g_quark_from_string("moz-font-group");
    return quark;
}

static void
gfxFontGroup_unref(gpointer data)
{
    gfxPangoFontGroup *fontGroup = static_cast<gfxPangoFontGroup*>(data);
    NS_RELEASE(fontGroup);
}

static void
SetFontGroup(PangoContext *aContext, gfxPangoFontGroup *aFontGroup)
{
    NS_ADDREF(aFontGroup);
    g_object_set_qdata_full(G_OBJECT(aContext), GetFontGroupQuark(),
                            aFontGroup, gfxFontGroup_unref);
}

static gfxPangoFontGroup *
GetFontGroup(PangoContext *aContext)
{
    return static_cast<gfxPangoFontGroup*>
        (g_object_get_qdata(G_OBJECT(aContext), GetFontGroupQuark()));
}

/**
 * gfxFcPangoFontSet:
 *
 * Translation from a desired FcPattern to a sorted set of font references
 * (fontconfig cache data) and (when needed) PangoFonts.
 */

class gfxFcPangoFontSet {
public:
    THEBES_INLINE_DECL_REFCOUNTING(gfxFcPangoFontSet)
    
    explicit gfxFcPangoFontSet(FcPattern *aPattern,
                               gfxUserFontSet *aUserFontSet)
        : mSortPattern(aPattern), mUserFontSet(aUserFontSet),
          mFcFontSet(SortPreferredFonts()), mFcFontsTrimmed(0),
          mHaveFallbackFonts(PR_FALSE)
    {
    }

    // A reference is held by the FontSet.
    // The caller may add a ref to keep the font alive longer than the FontSet.
    PangoFont *GetFontAt(PRUint32 i)
    {
        if (i >= mFonts.Length() || !mFonts[i].mFont) { 
            // GetFontPatternAt sets up mFonts
            FcPattern *fontPattern = GetFontPatternAt(i);
            if (!fontPattern)
                return NULL;

            mFonts[i].mFont =
                gfxPangoFcFont::NewFont(mSortPattern, fontPattern);
        }
        return mFonts[i].mFont;
    }

    FcPattern *GetFontPatternAt(PRUint32 i);

private:
    nsReturnRef<FcFontSet> SortPreferredFonts();
    nsReturnRef<FcFontSet> SortFallbackFonts();

    struct FontEntry {
        explicit FontEntry(FcPattern *aPattern) : mPattern(aPattern) {}
        nsCountedRef<FcPattern> mPattern;
        nsCountedRef<PangoFont> mFont;
    };

    struct LangSupportEntry {
        LangSupportEntry(FcChar8 *aLang, FcLangResult aSupport) :
            mLang(aLang), mBestSupport(aSupport) {}
        FcChar8 *mLang;
        FcLangResult mBestSupport;
    };

public:
    // public for nsTArray
    class LangComparator {
    public:
        PRBool Equals(const LangSupportEntry& a, const FcChar8 *b) const
        {
            return FcStrCmpIgnoreCase(a.mLang, b) == 0;
        }
    };

private:
    // The requested pattern
    nsCountedRef<FcPattern> mSortPattern;
    // Fonts from @font-face rules
    nsRefPtr<gfxUserFontSet> mUserFontSet;
    // A (trimmed) list of font patterns and PangoFonts that is built up as
    // required.
    nsTArray<FontEntry> mFonts;
    // Holds a list of font patterns that will be trimmed.  This is first set
    // to a list of preferred fonts.  Then, if/when all the preferred fonts
    // have been trimmed and added to mFonts, this is set to a list of
    // fallback fonts.
    nsAutoRef<FcFontSet> mFcFontSet;
    // The set of characters supported by the fonts in mFonts.
    nsAutoRef<FcCharSet> mCharSet;
    // The index of the next font in mFcFontSet that has not yet been
    // considered for mFonts.
    int mFcFontsTrimmed;
    // True iff fallback fonts are either stored in mFcFontSet or have been
    // trimmed and added to mFonts (so that mFcFontSet is NULL).
    PRPackedBool mHaveFallbackFonts;
};

// Find the FcPattern for an @font-face font suitable for CSS family |aFamily|
// and style |aStyle| properties.
static const nsTArray< nsCountedRef<FcPattern> >*
FindFontPatterns(gfxUserFontSet *mUserFontSet,
                const nsACString &aFamily, PRUint8 aStyle, PRUint16 aWeight)
{
    // Convert to UTF16
    NS_ConvertUTF8toUTF16 utf16Family(aFamily);

    // needsBold is not used here.  Instead synthetic bold is enabled through
    // FcFontRenderPrepare when the weight in the requested pattern is
    // compared against the weight in the font pattern.
    PRBool needsBold;

    gfxFontStyle style;
    style.style = aStyle;
    style.weight = aWeight;

    gfxFcFontEntry *fontEntry = static_cast<gfxFcFontEntry*>
        (mUserFontSet->FindFontEntry(utf16Family, style, needsBold));

    // Accept synthetic oblique for italic and oblique.
    if (!fontEntry && aStyle != FONT_STYLE_NORMAL) {
        style.style = FONT_STYLE_NORMAL;
        fontEntry = static_cast<gfxFcFontEntry*>
            (mUserFontSet->FindFontEntry(utf16Family, style, needsBold));
    }

    if (!fontEntry)
        return NULL;

    return &fontEntry->GetPatterns();
}

typedef FcBool (*FcPatternRemoveFunction)(FcPattern *p, const char *object,
                                          int id);

static FcPatternRemoveFunction
GetFcPatternRemove()
{
    PRLibrary *lib = nsnull;
    PRFuncPtr result =
        PR_FindFunctionSymbolAndLibrary("FcPatternRemove", &lib);
    if (lib) {
        PR_UnloadLibrary(lib);
    }

    return reinterpret_cast<FcPatternRemoveFunction>(result);
}

// FcPatternRemove is available in fontconfig-2.3.0 (2005)
static FcBool
moz_FcPatternRemove(FcPattern *p, const char *object, int id)
{
    static FcPatternRemoveFunction sFcPatternRemovePtr = GetFcPatternRemove();

    if (!sFcPatternRemovePtr)
        return FcFalse;

    return (*sFcPatternRemovePtr)(p, object, id);
}

// fontconfig always prefers a matching family to a matching slant, but CSS
// mostly prioritizes slant.  The logic here is from CSS 2.1.
static PRBool
SlantIsAcceptable(FcPattern *aFont, int aRequestedSlant)
{
    // CSS accepts (possibly synthetic) oblique for italic.
    if (aRequestedSlant == FC_SLANT_ITALIC)
        return PR_TRUE;

    int slant;
    FcResult result = FcPatternGetInteger(aFont, FC_SLANT, 0, &slant);
    // Not having a value would be strange.
    // fontconfig sort and match functions would consider no value a match.
    if (result != FcResultMatch)
        return PR_TRUE;

    switch (aRequestedSlant) {
        case FC_SLANT_ROMAN:
            // CSS requires an exact match
            return slant == aRequestedSlant;
        case FC_SLANT_OBLIQUE:
            // Accept synthetic oblique from Roman,
            // but CSS doesn't accept italic.
            return slant != FC_SLANT_ITALIC;
    }

    return PR_TRUE;
}

// fontconfig prefers a matching family or lang to pixelsize of bitmap
// fonts.  CSS suggests a tolerance of 20% on pixelsize.
static PRBool
SizeIsAcceptable(FcPattern *aFont, double aRequestedSize)
{
    double size;
    int v = 0;
    while (FcPatternGetDouble(aFont,
                              FC_PIXEL_SIZE, v, &size) == FcResultMatch) {
        ++v;
        if (5.0 * fabs(size - aRequestedSize) < aRequestedSize)
            return PR_TRUE;
    }

    // No size means scalable
    return v == 0;
}

// Sorting only the preferred fonts first usually saves having to sort through
// every font on the system.
nsReturnRef<FcFontSet>
gfxFcPangoFontSet::SortPreferredFonts()
{
    gfxFontconfigUtils *utils = gfxFontconfigUtils::GetFontconfigUtils();
    if (!utils)
        return nsReturnRef<FcFontSet>();

    // The list of families in mSortPattern has values with both weak and
    // strong bindings.  Values with strong bindings should be preferred.
    // Values with weak bindings are default fonts that should be considered
    // only when the font provides the best support for a requested language
    // or after other fonts have satisfied all the requested languages.
    //
    // There are no direct fontconfig APIs to get the binding type.  The
    // binding only takes effect in the sort and match functions.

    // |requiredLangs| is a list of requested languages that have not yet been
    // satisfied.  gfxFontconfigUtils only sets one FC_LANG property value,
    // but FcConfigSubstitute may add more values (e.g. prepending "en" to
    // "ja" will use western fonts to render Latin/Arabic numerals in Japanese
    // text.)
    nsAutoTArray<LangSupportEntry,10> requiredLangs;
    for (int v = 0; ; ++v) {
        FcChar8 *lang;
        FcResult result = FcPatternGetString(mSortPattern, FC_LANG, v, &lang);
        if (result != FcResultMatch) {
            // No need to check FcPatternGetLangSet() because
            // gfxFontconfigUtils sets only a string value for FC_LANG and
            // FcConfigSubstitute cannot add LangSets.
            NS_ASSERTION(result != FcResultTypeMismatch,
                         "Expected a string for FC_LANG");
            break;
        }

        if (!requiredLangs.Contains(lang, LangComparator())) {
            FcLangResult bestLangSupport = utils->GetBestLangSupport(lang);
            if (bestLangSupport != FcLangDifferentLang) {
                requiredLangs.
                    AppendElement(LangSupportEntry(lang, bestLangSupport));
            }
        }
    }

    nsAutoRef<FcFontSet> fontSet(FcFontSetCreate());
    if (!fontSet)
        return fontSet.out();

    // FcDefaultSubstitute() ensures a slant on mSortPattern, but, if that ever
    // doesn't happen, Roman will be used.
    int requestedSlant = FC_SLANT_ROMAN;
    FcPatternGetInteger(mSortPattern, FC_SLANT, 0, &requestedSlant);
    double requestedSize = -1.0;
    FcPatternGetDouble(mSortPattern, FC_PIXEL_SIZE, 0, &requestedSize);

    nsTHashtable<gfxFontconfigUtils::DepFcStrEntry> existingFamilies;
    existingFamilies.Init(50);
    FcChar8 *family;
    for (int v = 0;
         FcPatternGetString(mSortPattern,
                            FC_FAMILY, v, &family) == FcResultMatch; ++v) {
        const nsTArray< nsCountedRef<FcPattern> > *familyFonts = nsnull;

        // Is this an @font-face family?
        PRBool isUserFont = PR_FALSE;
        if (mUserFontSet) {
            // Have some @font-face definitions

            nsDependentCString cFamily(gfxFontconfigUtils::ToCString(family));
            NS_NAMED_LITERAL_CSTRING(userPrefix, FONT_FACE_FAMILY_PREFIX);

            if (StringBeginsWith(cFamily, userPrefix)) {
                isUserFont = PR_TRUE;

                // Trim off the prefix
                nsDependentCSubstring cssFamily(cFamily, userPrefix.Length());

                PRUint8 thebesStyle =
                    gfxFontconfigUtils::FcSlantToThebesStyle(requestedSlant);
                PRUint16 thebesWeight =
                    gfxFontconfigUtils::GetThebesWeight(mSortPattern);

                familyFonts = FindFontPatterns(mUserFontSet, cssFamily,
                                               thebesStyle, thebesWeight);
            }
        }

        if (!isUserFont) {
            familyFonts = &utils->GetFontsForFamily(family);
        }

        if (!familyFonts || familyFonts->Length() == 0) {
            // There are no fonts matching this family, so there is no point
            // in searching for this family in the FontSort.
            //
            // Perhaps the original pattern should be retained for
            // FcFontRenderPrepare.  However, the only a useful config
            // substitution test against missing families that i can imagine
            // would only be interested in the preferred family
            // (qual="first"), so always keep the first family and use the
            // same pattern for Sort and RenderPrepare.
            if (v != 0 && moz_FcPatternRemove(mSortPattern, FC_FAMILY, v)) {
                --v;
            }
            continue;
        }

        // Aliases seem to often end up occurring more than once, but
        // duplicate families can't be removed from the sort pattern without
        // knowing whether duplicates have the same binding.
        gfxFontconfigUtils::DepFcStrEntry *entry =
            existingFamilies.PutEntry(family);
        if (entry) {
            if (entry->mKey) // old entry
                continue;

            entry->mKey = family; // initialize new entry
        }

        for (PRUint32 f = 0; f < familyFonts->Length(); ++f) {
            FcPattern *font = familyFonts->ElementAt(f);

            // User fonts are already filtered by slant (but not size) in
            // mUserFontSet->FindFontEntry().
            if (!isUserFont && !SlantIsAcceptable(font, requestedSlant))
                continue;
            if (requestedSize != -1.0 && !SizeIsAcceptable(font, requestedSize))
                continue;

            for (PRUint32 r = 0; r < requiredLangs.Length(); ++r) {
                const LangSupportEntry& entry = requiredLangs[r];
                FcLangResult support =
                    gfxFontconfigUtils::GetLangSupport(font, entry.mLang);
                if (support <= entry.mBestSupport) { // lower is better
                    requiredLangs.RemoveElementAt(r);
                    --r;
                }
            }

            // FcFontSetDestroy will remove a reference but FcFontSetAdd
            // does _not_ take a reference!
            if (FcFontSetAdd(fontSet, font)) {
                FcPatternReference(font);
            }
        }
    }

    FcPattern *truncateMarker = NULL;
    for (PRUint32 r = 0; r < requiredLangs.Length(); ++r) {
        const nsTArray< nsCountedRef<FcPattern> >& langFonts =
            utils->GetFontsForLang(requiredLangs[r].mLang);

        PRBool haveLangFont = PR_FALSE;
        for (PRUint32 f = 0; f < langFonts.Length(); ++f) {
            FcPattern *font = langFonts[f];
            if (!SlantIsAcceptable(font, requestedSlant))
                continue;
            if (requestedSize != -1.0 && !SizeIsAcceptable(font, requestedSize))
                continue;

            haveLangFont = PR_TRUE;
            if (FcFontSetAdd(fontSet, font)) {
                FcPatternReference(font);
            }
        }

        if (!haveLangFont && langFonts.Length() > 0) {
            // There is a font that supports this language but it didn't pass
            // the slant and size criteria.  Weak default font families should
            // not be considered until the language has been satisfied.
            //
            // Insert a font that supports the language so that it will mark
            // the position of fonts from weak families in the sorted set and
            // they can be removed.  The language and weak families will be
            // considered in the fallback fonts, which use fontconfig's
            // algorithm.
            //
            // Of the fonts that don't meet slant and size criteria, strong
            // default font families should be considered before (other) fonts
            // for this language, so this marker font will be removed (as well
            // as the fonts from weak families), and strong families will be
            // reconsidered in the fallback fonts.
            FcPattern *font = langFonts[0];
            if (FcFontSetAdd(fontSet, font)) {
                FcPatternReference(font);
                truncateMarker = font;
            }
            break;
        }
    }

    FcFontSet *sets[1] = { fontSet };
    FcResult result;
    fontSet.own(FcFontSetSort(NULL, sets, 1, mSortPattern,
                              FcFalse, NULL, &result));

    if (truncateMarker != NULL && fontSet) {
        nsAutoRef<FcFontSet> truncatedSet(FcFontSetCreate());

        for (int f = 0; f < fontSet->nfont; ++f) {
            FcPattern *font = fontSet->fonts[f];
            if (font == truncateMarker)
                break;

            if (FcFontSetAdd(truncatedSet, font)) {
                FcPatternReference(font);
            }
        }

        fontSet.steal(truncatedSet);
    }

    return fontSet.out();
}

nsReturnRef<FcFontSet>
gfxFcPangoFontSet::SortFallbackFonts()
{
    // Setting trim to FcTrue would provide a much smaller (~ 1/10) FcFontSet,
    // but would take much longer due to comparing all the character sets.
    //
    // The references to fonts in this FcFontSet are almost free
    // as they are pointers into mmaped cache files.
    //
    // GetFontPatternAt() will trim lazily if and as needed, which will also
    // remove duplicates of preferred fonts.
    FcResult result;
    return nsReturnRef<FcFontSet>(FcFontSort(NULL, mSortPattern,
                                             FcFalse, NULL, &result));
}

// GetFontAt relies on this setting up all patterns up to |i|.
FcPattern *
gfxFcPangoFontSet::GetFontPatternAt(PRUint32 i)
{
    while (i >= mFonts.Length()) {
        while (!mFcFontSet) {
            if (mHaveFallbackFonts)
                return nsnull;

            mFcFontSet = SortFallbackFonts();
            mHaveFallbackFonts = PR_TRUE;
            mFcFontsTrimmed = 0;
            // Loop to test that mFcFontSet is non-NULL.
        }

        while (mFcFontsTrimmed < mFcFontSet->nfont) {
            FcPattern *font = mFcFontSet->fonts[mFcFontsTrimmed];
            ++mFcFontsTrimmed;

            if (mFonts.Length() != 0) {
                // See if the next font provides support for any extra
                // characters.  Most often the next font is not going to
                // support more characters so check for a SubSet first before
                // allocating a new CharSet with Union.
                FcCharSet *supportedChars = mCharSet;
                if (!supportedChars) {
                    FcPatternGetCharSet(mFonts[mFonts.Length() - 1].mPattern,
                                        FC_CHARSET, 0, &supportedChars);
                }

                if (supportedChars) {
                    FcCharSet *newChars = NULL;
                    FcPatternGetCharSet(font, FC_CHARSET, 0, &newChars);
                    if (newChars) {
                        if (FcCharSetIsSubset(newChars, supportedChars))
                            continue;

                        mCharSet.own(FcCharSetUnion(supportedChars, newChars));
                    } else if (!mCharSet) {
                        mCharSet.own(FcCharSetCopy(supportedChars));
                    }
                }
            }

            mFonts.AppendElement(font);
            if (mFonts.Length() >= i)
                break;
        }

        if (mFcFontsTrimmed == mFcFontSet->nfont) {
            // finished with this font set
            mFcFontSet.reset();
        }
    }

    return mFonts[i].mPattern;
}

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

    PangoLanguage *mLanguage;
    gfxFcPangoFontSet *mGfxFontSet;
    PangoFont *mBaseFont;
    gfxPangoFontGroup *mFontGroup;

    static PangoFontset *
    NewFontset(gfxPangoFontGroup *aFontGroup,
               PangoLanguage *aLanguage)
    {
        gfxPangoFontset *fontset = static_cast<gfxPangoFontset *>
            (g_object_new(GFX_TYPE_PANGO_FONTSET, NULL));

        fontset->mLanguage = aLanguage;

        // Use the font group's fontset if the language matches
        if (aFontGroup->GetPangoLanguage() == aLanguage) {
            fontset->mGfxFontSet = aFontGroup->GetFontSet();
            NS_IF_ADDREF(fontset->mGfxFontSet);

        } else {
            // Otherwise, fallback fonts depend on the language so get
            // another font-set for the language if/when the base font is
            // not suitable.  Save the font group for this.
            fontset->mFontGroup = aFontGroup;
            NS_ADDREF(fontset->mFontGroup);

            // Using the same base font irrespective of the language that
            // Pango chooses for the script means that PANGO_SCRIPT_COMMON
            // characters are consistently rendered with the same font.
            // (Bug 339513 and bug 416725).
            //
            // However, use the default Pango behavior (selecting generic
            // fonts from the script of the characters) in two situations:
            //
            //   1. When we don't have a language to make a good choice for
            //      the primary font.
            //
            //   2. For system fonts, use the default Pango behavior to give
            //      consistency with other apps.  (This probably wouldn't be
            //      necessary but for bug 91190.)
            if (aFontGroup->GetPangoLanguage() &&
                !aFontGroup->GetStyle()->systemFont) {
                fontset->mBaseFont = aFontGroup->GetBasePangoFont();
                if (fontset->mBaseFont)
                    g_object_ref(fontset->mBaseFont);
            }
        }

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
}

static void
gfx_pango_fontset_finalize(GObject *object)
{
    gfxPangoFontset *self = GFX_PANGO_FONTSET(object);

    if (self->mBaseFont)
        g_object_unref(self->mBaseFont);
    NS_IF_RELEASE(self->mGfxFontSet);
    NS_IF_RELEASE(self->mFontGroup);

    G_OBJECT_CLASS(gfx_pango_fontset_parent_class)->finalize(object);
}

static PangoLanguage *
gfx_pango_fontset_get_language(PangoFontset *fontset)
{
    gfxPangoFontset *self = GFX_PANGO_FONTSET(fontset);
    return self->mLanguage;
}

static gfxFcPangoFontSet *
GetGfxFontSet(gfxPangoFontset *self)
{
    if (!self->mGfxFontSet && self->mFontGroup) {
        self->mGfxFontSet = self->mFontGroup->GetFontSet(self->mLanguage);
        // Finished with the font group
        NS_RELEASE(self->mFontGroup);

        if (!self->mGfxFontSet)
            return nsnull;

        NS_ADDREF(self->mGfxFontSet);
    }
    return self->mGfxFontSet;
}

static void
gfx_pango_fontset_foreach(PangoFontset *fontset, PangoFontsetForeachFunc func,
                          gpointer data)
{
    gfxPangoFontset *self = GFX_PANGO_FONTSET(fontset);

    FcPattern *baseFontPattern = NULL;
    if (self->mBaseFont) {
        if ((*func)(fontset, self->mBaseFont, data))
            return;

        baseFontPattern = PANGO_FC_FONT(self->mBaseFont)->font_pattern;
    }        

    // Falling back to secondary fonts
    gfxFcPangoFontSet *gfxFontSet = GetGfxFontSet(self);
    if (!gfxFontSet)
        return;

    for (PRUint32 i = 0;
         FcPattern *pattern = gfxFontSet->GetFontPatternAt(i);
         ++i) {
        // Skip this font if it is the same face as the base font
        if (pattern == baseFontPattern) {
            continue;
        }
        PangoFont *font = gfxFontSet->GetFontAt(i);
        if (font) {
            if ((*func)(fontset, font, data))
                return;
        }
    }
}

static PRBool HasChar(FcPattern *aFont, FcChar32 wc)
{
    FcCharSet *charset = NULL;
    FcPatternGetCharSet(aFont, FC_CHARSET, 0, &charset);

    return charset && FcCharSetHasChar(charset, wc);
}

static PangoFont *
gfx_pango_fontset_get_font(PangoFontset *fontset, guint wc)
{
    gfxPangoFontset *self = GFX_PANGO_FONTSET(fontset);

    PangoFont *result = NULL;

    FcPattern *baseFontPattern = NULL;
    if (self->mBaseFont) {
        baseFontPattern = PANGO_FC_FONT(self->mBaseFont)->font_pattern;

        if (HasChar(baseFontPattern, wc)) {
            result = self->mBaseFont;
        }
    }

    if (!result) {
        // Falling back to secondary fonts
        gfxFcPangoFontSet *gfxFontSet = GetGfxFontSet(self);

        if (gfxFontSet) {
            for (PRUint32 i = 0;
                 FcPattern *pattern = gfxFontSet->GetFontPatternAt(i);
                 ++i) {
                // Skip this font if it is the same face as the base font
                if (pattern == baseFontPattern) {
                    continue;
                }

                if (HasChar(pattern, wc)) {
                    result = gfxFontSet->GetFontAt(i);
                    break;
                }
            }
        }

        if (!result) {
            // Nothing found.  Return the first font.
            if (self->mBaseFont) {
                result = self->mBaseFont;
            } else if (gfxFontSet) {
                result = gfxFontSet->GetFontAt(0);
            }
        }
    }

    if (!result)
        return NULL;

    g_object_ref(result);
    return result;
}

static void
gfx_pango_fontset_class_init (gfxPangoFontsetClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    PangoFontsetClass *fontset_class = PANGO_FONTSET_CLASS (klass);

    object_class->finalize = gfx_pango_fontset_finalize;
    // get_font is not likely to be used but implemented because the class
    //   makes it available.
    fontset_class->get_font = gfx_pango_fontset_get_font;
    // inherit fontset_class->get_metrics (which is not likely to be used)
    fontset_class->get_language = gfx_pango_fontset_get_language;
    fontset_class->foreach = gfx_pango_fontset_foreach;
}

/**
 * gfxPangoFontMap: An implementation of a PangoFontMap.
 *
 * This is passed to pango_itemize() through the PangoContext parameter, and
 * provides font selection through the gfxPangoFontGroup.
 *
 * It is intended that the font group is recorded on the PangoContext with
 * SetFontGroup().  The font group is then queried for fonts, with
 * gfxFcPangoFontSet doing the font selection.
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
    PangoFcFontMap parent_instance;

    static PangoFontMap *
    NewFontMap()
    {
        gfxPangoFontMap *fontmap = static_cast<gfxPangoFontMap *>
            (g_object_new(GFX_TYPE_PANGO_FONT_MAP, NULL));

        return PANGO_FONT_MAP(fontmap);
    }
};

struct gfxPangoFontMapClass {
    PangoFcFontMapClass parent_class;
};

G_DEFINE_TYPE (gfxPangoFontMap, gfx_pango_font_map, PANGO_TYPE_FC_FONT_MAP)

static void
gfx_pango_font_map_init(gfxPangoFontMap *fontset)
{
}

static PangoFont *
gfx_pango_font_map_load_font(PangoFontMap *fontmap, PangoContext *context,
                             const PangoFontDescription *description)
{
    gfxPangoFontGroup *fontGroup = GetFontGroup(context);
    if (NS_UNLIKELY(!fontGroup)) {
        return PANGO_FONT_MAP_CLASS(gfx_pango_font_map_parent_class)->
            load_font(fontmap, context, description);
    }
        
    PangoFont *baseFont = fontGroup->GetBasePangoFont();
    if (NS_LIKELY(baseFont)) {
        g_object_ref(baseFont);
    }
    return baseFont;
}

static PangoFontset *
gfx_pango_font_map_load_fontset(PangoFontMap *fontmap, PangoContext *context,
                                const PangoFontDescription *desc,
                                PangoLanguage *language)
{
    gfxPangoFontGroup *fontGroup = GetFontGroup(context);
    if (NS_UNLIKELY(!fontGroup)) {
        return PANGO_FONT_MAP_CLASS(gfx_pango_font_map_parent_class)->
            load_fontset(fontmap, context, desc, language);
    }

    return gfxPangoFontset::NewFontset(fontGroup, language);
}

static double
gfx_pango_font_map_get_resolution(PangoFcFontMap *fcfontmap,
                                  PangoContext *context)
{
    // This merely enables the FC_SIZE field of the pattern to be accurate.
    // We use gfxPlatformGtk::DPI() much of the time...
    return gfxPlatformGtk::DPI();
}

#ifdef MOZ_WIDGET_GTK2
static void ApplyGdkScreenFontOptions(FcPattern *aPattern);
#endif

// Apply user settings and defaults to pattern in preparation for matching.
static void
PrepareSortPattern(FcPattern *aPattern, double aFallbackSize,
                   double aSizeAdjustFactor, PRBool aIsPrinterFont)
{
    FcConfigSubstitute(NULL, aPattern, FcMatchPattern);

    // This gets cairo_font_options_t for the Screen.  We should have
    // different font options for printing (no hinting) but we are not told
    // what we are measuring for.
    //
    // If cairo adds support for lcd_filter, gdk will not provide the default
    // setting for that option.  We could get the default setting by creating
    // an xlib surface once, recording its font_options, and then merging the
    // gdk options.
    //
    // Using an xlib surface would also be an option to get Screen font
    // options for non-GTK X11 toolkits, but less efficient than using GDK to
    // pick up dynamic changes.
    if(aIsPrinterFont) {
       cairo_font_options_t *options = cairo_font_options_create();
       cairo_font_options_set_hint_style (options, CAIRO_HINT_STYLE_NONE);
       cairo_font_options_set_antialias (options, CAIRO_ANTIALIAS_GRAY);
       cairo_ft_font_options_substitute(options, aPattern);
       cairo_font_options_destroy(options);
    }
#ifdef MOZ_WIDGET_GTK2
    else {
       ApplyGdkScreenFontOptions(aPattern);
    }
#endif

    // Protect against any fontconfig settings that may have incorrectly
    // modified the pixelsize, and consider aSizeAdjustFactor.
    double size = aFallbackSize;
    if (FcPatternGetDouble(aPattern, FC_PIXEL_SIZE, 0, &size) != FcResultMatch
        || aSizeAdjustFactor != 1.0) {
        FcPatternDel(aPattern, FC_PIXEL_SIZE);
        FcPatternAddDouble(aPattern, FC_PIXEL_SIZE, size * aSizeAdjustFactor);
    }

    FcDefaultSubstitute(aPattern);
}

static void
gfx_pango_font_map_context_substitute(PangoFcFontMap *fontmap,
                                      PangoContext *context,
                                      FcPattern *pattern)
{
    // owned by the context
    PangoFontDescription *desc = pango_context_get_font_description(context);
    double size = pango_font_description_get_size(desc) / FLOAT_PANGO_SCALE;
    gfxPangoFontGroup *fontGroup = GetFontGroup(context);
    PRBool usePrinterFont = fontGroup && fontGroup->GetStyle()->printerFont;
    PrepareSortPattern(pattern, size, 1.0, usePrinterFont);
}

static PangoFcFont *
gfx_pango_font_map_create_font(PangoFcFontMap *fontmap,
                               PangoContext *context,
                               const PangoFontDescription *desc,
                               FcPattern *pattern)
{
    return PANGO_FC_FONT(g_object_new(GFX_TYPE_PANGO_FC_FONT,
                                      "pattern", pattern, NULL));
}

static void
gfx_pango_font_map_class_init(gfxPangoFontMapClass *klass)
{
    // inherit GObjectClass::finalize from parent as this class adds no data.

    PangoFontMapClass *fontmap_class = PANGO_FONT_MAP_CLASS (klass);
    fontmap_class->load_font = gfx_pango_font_map_load_font;
    // inherit fontmap_class->list_families (which is not likely to be used)
    //   from PangoFcFontMap
    fontmap_class->load_fontset = gfx_pango_font_map_load_fontset;
    // inherit fontmap_class->shape_engine_type from PangoFcFontMap

    PangoFcFontMapClass *fcfontmap_class = PANGO_FC_FONT_MAP_CLASS (klass);
    fcfontmap_class->get_resolution = gfx_pango_font_map_get_resolution;
    // context_key_* virtual functions are only necessary if we want to
    // dynamically respond to changes in the screen cairo_font_options_t.

    // context_substitute and get_font are not likely to be used but
    //   implemented because the class makes them available.
    fcfontmap_class->context_substitute = gfx_pango_font_map_context_substitute;
    fcfontmap_class->create_font = gfx_pango_font_map_create_font;
}

/**
 ** gfxPangoFontGroup
 **/

struct FamilyCallbackData {
    FamilyCallbackData(nsTArray<nsString> *aFcFamilyList,
                       gfxUserFontSet *aUserFontSet)
        : mFcFamilyList(aFcFamilyList), mUserFontSet(aUserFontSet)
    {
    }
    nsTArray<nsString> *mFcFamilyList;
    const gfxUserFontSet *mUserFontSet;
};

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
FamilyCallback (const nsAString& fontName, const nsACString& genericName,
                void *closure)
{
    FamilyCallbackData *data = static_cast<FamilyCallbackData*>(closure);
    nsTArray<nsString> *list = data->mFcFamilyList;

    // We ignore prefs that have three hypens since they are X style prefs.
    if (genericName.Length() && FFRECountHyphens(fontName) >= 3)
        return PR_TRUE;

    if (!list->Contains(fontName)) {
        // The family properties of FcPatterns for @font-face fonts have a
        // namespace to identify them among system fonts.  (see
        // FONT_FACE_FAMILY_PREFIX.)  The CSS family name can match either the
        // @font-face family or the system font family so both names are added
        // here.
        //
        // http://www.w3.org/TR/2002/WD-css3-webfonts-20020802 required
        // looking for locally-installed fonts matching requested properties
        // before checking the src descriptor in @font-face rules.
        // http://www.w3.org/TR/2008/REC-CSS2-20080411/fonts.html#algorithm
        // also only checks src descriptors if there is no local font matching
        // the requested properties.
        //
        // Similarly "Editor's Draft 27 June 2008"
        // http://dev.w3.org/csswg/css3-fonts/#font-matching says "The user
        // agent attempts to find the family name among fonts available on the
        // system and then among fonts defined via @font-face rules."
        // However, this is contradicted by "if [the name from the font-family
        // descriptor] is the same as a font family available in a given
        // user's environment, it effectively hides the underlying font for
        // documents that use the stylesheet."
        //
        // Windows and Mac code currently prioritizes fonts from @font-face
        // rules.  The order of families here reflects the priorities on those
        // platforms.
        const gfxUserFontSet *userFontSet = data->mUserFontSet;
        if (genericName.Length() == 0 &&
            userFontSet && userFontSet->HasFamily(fontName)) {
            nsAutoString userFontName =
                NS_LITERAL_STRING(FONT_FACE_FAMILY_PREFIX) + fontName;
            list->AppendElement(userFontName);
        }

        list->AppendElement(fontName);
    }

    return PR_TRUE;
}

gfxPangoFontGroup::gfxPangoFontGroup (const nsAString& families,
                                      const gfxFontStyle *aStyle,
                                      gfxUserFontSet *aUserFontSet)
    : gfxFontGroup(families, aStyle, aUserFontSet),
      mPangoLanguage(GuessPangoLanguage(aStyle->langGroup))
{
    mFonts.AppendElements(1);
}

gfxPangoFontGroup::~gfxPangoFontGroup()
{
}

gfxFontGroup *
gfxPangoFontGroup::Copy(const gfxFontStyle *aStyle)
{
    return new gfxPangoFontGroup(mFamilies, aStyle, mUserFontSet);
}

// An array of family names suitable for fontconfig
void
gfxPangoFontGroup::GetFcFamilies(nsTArray<nsString> *aFcFamilyList,
                                 const nsACString& aLangGroup)
{
    FamilyCallbackData data(aFcFamilyList, mUserFontSet);
    // Leave non-existing fonts in the list so that fontconfig can get the
    // best match.
    ForEachFontInternal(mFamilies, aLangGroup, PR_TRUE, PR_FALSE,
                        FamilyCallback, &data);
}

PangoFont *
gfxPangoFontGroup::GetBasePangoFont()
{
    return GetBaseFontSet()->GetFontAt(0);
}

gfxFont *
gfxPangoFontGroup::GetFontAt(PRInt32 i) {
    // If it turns out to be hard for all clients that cache font
    // groups to call UpdateFontList at appropriate times, we could
    // instead consider just calling UpdateFontList from someplace
    // more central (such as here).
    NS_ASSERTION(!mUserFontSet || mCurrGeneration == GetGeneration(),
                 "Whoever was caching this font group should have "
                 "called UpdateFontList on it");

    NS_PRECONDITION(i == 0, "Only have one font");

    if (!mFonts[0]) {
        PangoFont *pangoFont = GetBasePangoFont();
        mFonts[0] = gfxPangoFcFont::GfxFont(GFX_PANGO_FC_FONT(pangoFont));
    }

    return mFonts[0];
}

void
gfxPangoFontGroup::UpdateFontList()
{
    if (!mUserFontSet)
        return;

    PRUint64 newGeneration = mUserFontSet->GetGeneration();
    if (newGeneration == mCurrGeneration)
        return;

    mFonts[0] = NULL;
    mFontSets.Clear();
    mCurrGeneration = newGeneration;
}

already_AddRefed<gfxFcPangoFontSet>
gfxPangoFontGroup::MakeFontSet(PangoLanguage *aLang, gfxFloat aSizeAdjustFactor,
                               nsAutoRef<FcPattern> *aMatchPattern)
{
    const char *lang = pango_language_to_string(aLang);

    const char *langGroup = nsnull;
    if (aLang != mPangoLanguage) {
        // Set up langGroup for Mozilla's font prefs.
        if (!gLangService) {
            CallGetService(NS_LANGUAGEATOMSERVICE_CONTRACTID, &gLangService);
        }
        if (gLangService) {
            nsIAtom *atom =
                gLangService->LookupLanguage(NS_ConvertUTF8toUTF16(lang));
            if (atom) {
                atom->GetUTF8String(&langGroup);
            }
        }
    }

    nsAutoTArray<nsString, 20> fcFamilyList;
    GetFcFamilies(&fcFamilyList,
                  langGroup ? nsDependentCString(langGroup) : mStyle.langGroup);

    // To consider: A fontset cache here could be helpful.

    // Get a pattern suitable for matching.
    nsAutoRef<FcPattern> pattern
        (gfxFontconfigUtils::NewPattern(fcFamilyList, mStyle, lang));

    PrepareSortPattern(pattern, mStyle.size, aSizeAdjustFactor, mStyle.printerFont);

    nsRefPtr<gfxFcPangoFontSet> fontset =
        new gfxFcPangoFontSet(pattern, mUserFontSet);

    if (aMatchPattern)
        aMatchPattern->steal(pattern);

    return fontset.forget();
}

gfxPangoFontGroup::
FontSetByLangEntry::FontSetByLangEntry(PangoLanguage *aLang,
                                       gfxFcPangoFontSet *aFontSet)
    : mLang(aLang), mFontSet(aFontSet)
{
}

gfxFcPangoFontSet *
gfxPangoFontGroup::GetFontSet(PangoLanguage *aLang)
{
    GetBaseFontSet(); // sets mSizeAdjustFactor and mFontSets[0]

    if (!aLang)
        return mFontSets[0].mFontSet;

    for (PRUint32 i = 0; i < mFontSets.Length(); ++i) {
        if (mFontSets[i].mLang == aLang)
            return mFontSets[i].mFontSet;
    }

    nsRefPtr<gfxFcPangoFontSet> fontSet =
        MakeFontSet(aLang, mSizeAdjustFactor);
    mFontSets.AppendElement(FontSetByLangEntry(aLang, fontSet));

    return fontSet;
}

/**
 ** gfxFcFont
 **/

cairo_user_data_key_t gfxFcFont::sGfxFontKey;

gfxFcFont::gfxFcFont(cairo_scaled_font_t *aCairoFont,
                     gfxFontEntry *aFontEntry,
                     const gfxFontStyle *aFontStyle)
    : gfxFont(aFontEntry, aFontStyle),
      mCairoFont(aCairoFont),
      mHasMetrics(PR_FALSE)
{
    cairo_scaled_font_reference(mCairoFont);
    cairo_scaled_font_set_user_data(mCairoFont, &sGfxFontKey, this, NULL);
}

gfxFcFont::~gfxFcFont()
{
    cairo_scaled_font_set_user_data(mCairoFont, &sGfxFontKey, NULL, NULL);
    cairo_scaled_font_destroy(mCairoFont);
}

/* static */ void
gfxPangoFontGroup::Shutdown()
{
    if (gPangoFontMap) {
        if (PANGO_IS_FC_FONT_MAP (gPangoFontMap)) {
            // This clears circular references from the fontmap to itself
            // through its fonts.
            pango_fc_font_map_shutdown(PANGO_FC_FONT_MAP(gPangoFontMap));
        }
        g_object_unref(gPangoFontMap);
        gPangoFontMap = NULL;
    }

    // Resetting gFTLibrary in case this is wanted again after a
    // cairo_debug_reset_static_data.
    gFTLibrary = NULL;

    NS_IF_RELEASE(gLangService);
}

/* static */ gfxFontEntry *
gfxPangoFontGroup::NewFontEntry(const gfxProxyFontEntry &aProxyEntry,
                                const nsAString& aFullname)
{
    gfxFontconfigUtils *utils = gfxFontconfigUtils::GetFontconfigUtils();
    if (!utils)
        return nsnull;

    // The font face name from @font-face { src: local() } is not well
    // defined.
    //
    // On MS Windows, this name gets compared with
    // ENUMLOGFONTEXW::elfFullName, which for OpenType fonts seems to be the
    // full font name from the name table.  For CFF OpenType fonts this is the
    // same as the PostScript name, but for TrueType fonts it is usually
    // different.
    //
    // On Mac, the font face name is compared with the PostScript name, even
    // for TrueType fonts.
    //
    // Fontconfig only records the full font names, so the behavior here
    // follows that on MS Windows.  However, to provide the possibility
    // of aliases to compensate for variations, the font face name is passed
    // through FcConfigSubstitute.

    nsAutoRef<FcPattern> pattern(FcPatternCreate());
    if (!pattern)
        return nsnull;

    NS_ConvertUTF16toUTF8 fullname(aFullname);
    FcPatternAddString(pattern, FC_FULLNAME,
                       gfxFontconfigUtils::ToFcChar8(fullname));
    FcConfigSubstitute(NULL, pattern, FcMatchPattern);

    FcChar8 *name;
    for (int v = 0;
         FcPatternGetString(pattern, FC_FULLNAME, v, &name) == FcResultMatch;
         ++v) {
        const nsTArray< nsCountedRef<FcPattern> >& fonts =
            utils->GetFontsForFullname(name);

        if (fonts.Length() != 0)
            return new gfxLocalFcFontEntry(aProxyEntry, fonts);
    }

    return nsnull;
}

static FT_Library
GetFTLibrary()
{
    if (!gFTLibrary) {
        // Use cairo's FT_Library so that cairo takes care of shutdown of the
        // FT_Library after it has destroyed its font_faces, and FT_Done_Face
        // has been called on each FT_Face, at least until this bug is fixed:
        // https://bugs.freedesktop.org/show_bug.cgi?id=18857
        //
        // Cairo's FT_Library can be obtained from any cairo_scaled_font.  The
        // font properties requested here are chosen to get an FT_Face that is
        // likely to be also used elsewhere.
        gfxFontStyle style;
        nsRefPtr<gfxPangoFontGroup> fontGroup =
            new gfxPangoFontGroup(NS_LITERAL_STRING("sans-serif"),
                                  &style, nsnull);

        gfxFcFont *font = static_cast<gfxFcFont*>(fontGroup->GetFontAt(0));
        if (!font)
            return NULL;

        LockedFTFace face(font);
        if (!face.get())
            return NULL;

        gFTLibrary = face.get()->glyph->library;
    }

    return gFTLibrary;
}

/* static */ gfxFontEntry *
gfxPangoFontGroup::NewFontEntry(const gfxProxyFontEntry &aProxyEntry,
                                nsISupports *aLoader,
                                const PRUint8 *aFontData, PRUint32 aLength)
{
    // Using face_index = 0 for the first face in the font, as we have no
    // other information.  FT_New_Memory_Face checks for a NULL FT_Library.
    FT_Face face;
    FT_Error error =
        FT_New_Memory_Face(GetFTLibrary(), aFontData, aLength, 0, &face);
    if (error != 0)
        return nsnull;

    return new gfxDownloadedFcFontEntry(aProxyEntry, aLoader, face);
}


static double
GetPixelSize(FcPattern *aPattern)
{
    double size;
    if (FcPatternGetDouble(aPattern,
                           FC_PIXEL_SIZE, 0, &size) == FcResultMatch)
        return size;

    NS_NOTREACHED("No size on pattern");
    return 0.0;
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
 * recorded.
 */

/* static */
already_AddRefed<gfxFcFont>
gfxFcFont::GetOrMakeFont(FcPattern *aPattern)
{
    cairo_scaled_font_t *cairoFont = CreateScaledFont(aPattern);

    nsRefPtr<gfxFcFont> font = static_cast<gfxFcFont*>
        (cairo_scaled_font_get_user_data(cairoFont, &sGfxFontKey));

    if (!font) {
        gfxFloat size = GetPixelSize(aPattern);

        // Shouldn't actually need to take too much care about the correct
        // name or style, as size is the only thing expected to be important.
        PRUint8 style = gfxFontconfigUtils::GetThebesStyle(aPattern);
        PRUint16 weight = gfxFontconfigUtils::GetThebesWeight(aPattern);

        // The LangSet in the FcPattern does not have an order so there is no
        // one particular language to choose and converting the set to a
        // string through FcNameUnparse() is more trouble than it's worth.
        NS_NAMED_LITERAL_CSTRING(langGroup, "x-unicode");
        gfxFontStyle fontStyle(style, weight, size, langGroup, 0.0,
                               PR_TRUE, PR_FALSE, PR_FALSE);

        nsRefPtr<gfxFontEntry> fe;
        FcChar8 *fc_file;
        if (FcPatternGetString(aPattern,
                               FC_FILE, 0, &fc_file) == FcResultMatch) {
            int index;
            if (FcPatternGetInteger(aPattern,
                                    FC_INDEX, 0, &index) != FcResultMatch) {
                // cairo won't know what to do with this pattern.
                NS_NOTREACHED("No index in pattern for font face from file");
                index = 0;
            }

            // Get a unique name for the font face data from the file and id.
            nsAutoString name;
            AppendUTF8toUTF16(gfxFontconfigUtils::ToCString(fc_file), name);
            if (index != 0) {
                name.AppendLiteral("/");
                name.AppendInt(index);
            }

            fe = new gfxFontEntry(name);
        } else {
            fe = GetDownloadedFontEntry(aPattern);
            if (!fe) {
                // cairo won't know which font to open without a file.
                // (We don't create fonts from an FT_Face.)
                NS_NOTREACHED("Fonts without a file is not a web font!?");
                fe = new gfxFontEntry(nsString());
            }
        }

        // Note that a file/index pair (or FT_Face) and the gfxFontStyle are
        // not necessarily enough to provide a key that will describe a unique
        // font.  cairoFont contains information from aPattern, which is a
        // fully resolved pattern from FcFontRenderPrepare.
        // FcFontRenderPrepare takes the requested pattern and the face
        // pattern as input and can modify elements of the resulting pattern
        // that affect rendering but are not included in the gfxFontStyle.
        font = new gfxFcFont(cairoFont, fe, &fontStyle);
    }

    cairo_scaled_font_destroy(cairoFont);
    return font.forget();
}

static PangoFontMap *
GetPangoFontMap()
{
    if (!gPangoFontMap) {
        gPangoFontMap = gfxPangoFontMap::NewFontMap();
    }
    return gPangoFontMap;
}

static PangoContext *
GetPangoContext()
{
    PangoContext *context = pango_context_new();
    pango_context_set_font_map(context, GetPangoFontMap());
    return context;
}

gfxFcPangoFontSet *
gfxPangoFontGroup::GetBaseFontSet()
{
    if (mFontSets.Length() > 0)
        return mFontSets[0].mFontSet;

    mSizeAdjustFactor = 1.0; // will be adjusted below if necessary
    nsAutoRef<FcPattern> pattern;
    nsRefPtr<gfxFcPangoFontSet> fontSet =
        MakeFontSet(mPangoLanguage, mSizeAdjustFactor, &pattern);

    double size = GetPixelSize(pattern);
    if (size != 0.0 && mStyle.sizeAdjust != 0.0) {
        gfxFcFont *font =
            gfxPangoFcFont::GfxFont(GFX_PANGO_FC_FONT(fontSet->GetFontAt(0)));
        if (font) {
            const gfxFont::Metrics& metrics = font->GetMetrics();

            // The factor of 0.1 ensures that xHeight is sane so fonts don't
            // become huge.  Strictly ">" ensures that xHeight and emHeight are
            // not both zero.
            if (metrics.xHeight > 0.1 * metrics.emHeight) {
                mSizeAdjustFactor =
                    mStyle.sizeAdjust * metrics.emHeight / metrics.xHeight;

                size *= mSizeAdjustFactor;
                FcPatternDel(pattern, FC_PIXEL_SIZE);
                FcPatternAddDouble(pattern, FC_PIXEL_SIZE, size);

                fontSet = new gfxFcPangoFontSet(pattern, mUserFontSet);
            }
        }
    }

    PangoLanguage *pangoLang = mPangoLanguage;
    FcChar8 *fcLang;
    if (!pangoLang &&
        FcPatternGetString(pattern, FC_LANG, 0, &fcLang) == FcResultMatch) {
        pangoLang =
            pango_language_from_string(gfxFontconfigUtils::ToCString(fcLang));
    }

    mFontSets.AppendElement(FontSetByLangEntry(pangoLang, fontSet));

    return fontSet;
}

void
gfxFcFont::GetGlyphExtents(PRUint32 aGlyph, cairo_text_extents_t* aExtents)
{
    NS_PRECONDITION(aExtents != NULL, "aExtents must not be NULL");

    cairo_glyph_t glyphs[1];
    glyphs[0].index = aGlyph;
    glyphs[0].x = 0.0;
    glyphs[0].y = 0.0;
    // cairo does some caching for us here but perhaps a small gain could be
    // made by caching more.  It is usually only the advance that is needed,
    // so caching only the advance could allow many requests to be cached with
    // little memory use.  Ideally this cache would be merged with
    // gfxGlyphExtents.
    cairo_scaled_font_glyph_extents(CairoScaledFont(), glyphs, 1, aExtents);
}

PRUint32
LockedFTFace::GetCharExtents(char aChar,
                             cairo_text_extents_t* aExtents)
{
    NS_PRECONDITION(aExtents != NULL, "aExtents must not be NULL");

    if (!mFace)
        return 0;

    // pango_fc_font_real_get_glyph uses FcFreeTypeCharIndex which may change
    // the charmap currently selected on the FT_Face, so, while
    // pango_fc_font_real_get_glyph might be used, we should use the same
    // function so as to search the charmaps.
    //
    // Unfortunately this considers the mac/roman cmap even when there is a
    // unicode cmap, which will be bad for symbol fonts, so we should do this
    // ourselves, perhaps with a lightweight cache like
    // pango_fc_font_real_get_glyph uses.
    FT_UInt gid = FcFreeTypeCharIndex(mFace, aChar); // glyph id
    if (gid) {
        mGfxFont->GetGlyphExtents(gid, aExtents);
    }

    return gid;
}

// Snap a line to pixels while keeping the center and size of the line as
// close to the original position as possible.
//
// Pango does similar snapping for underline and strikethrough when fonts are
// hinted, but nsCSSRendering::GetTextDecorationRectInternal always snaps the
// top and size of lines.  Optimizing the distance between the line and
// baseline is probably good for the gap between text and underline, but
// optimizing the center of the line is better for positioning strikethough.
static void
SnapLineToPixels(gfxFloat& aOffset, gfxFloat& aSize)
{
    gfxFloat snappedSize = PR_MAX(NS_floor(aSize + 0.5), 1.0);
    // Correct offset for change in size
    gfxFloat offset = aOffset - 0.5 * (aSize - snappedSize);
    // Snap offset
    aOffset = NS_floor(offset + 0.5);
    aSize = snappedSize;
}

void
LockedFTFace::GetMetrics(gfxFont::Metrics* aMetrics, PRUint32* aSpaceGlyph)
{
    NS_PRECONDITION(aMetrics != NULL, "aMetrics must not be NULL");
    NS_PRECONDITION(aSpaceGlyph != NULL, "aSpaceGlyph must not be NULL");

    if (NS_UNLIKELY(!mFace)) {
        // No face.  This unfortunate situation might happen if the font
        // file is (re)moved at the wrong time.
        aMetrics->emHeight = mGfxFont->GetStyle()->size;
        aMetrics->emAscent = 0.8 * aMetrics->emHeight;
        aMetrics->emDescent = 0.2 * aMetrics->emHeight;
        aMetrics->maxAscent = aMetrics->emAscent;
        aMetrics->maxDescent = aMetrics->maxDescent;
        aMetrics->maxHeight = aMetrics->emHeight;
        aMetrics->internalLeading = 0.0;
        aMetrics->externalLeading = 0.2 * aMetrics->emHeight;
        aSpaceGlyph = 0;
        aMetrics->spaceWidth = 0.5 * aMetrics->emHeight;
        aMetrics->maxAdvance = aMetrics->spaceWidth;
        aMetrics->aveCharWidth = aMetrics->spaceWidth;
        aMetrics->zeroOrAveCharWidth = aMetrics->spaceWidth;
        aMetrics->xHeight = 0.5 * aMetrics->emHeight;
        aMetrics->underlineSize = aMetrics->emHeight / 14.0;
        aMetrics->underlineOffset = -aMetrics->underlineSize;
        aMetrics->strikeoutOffset = 0.25 * aMetrics->emHeight;
        aMetrics->strikeoutSize = aMetrics->underlineSize;
        aMetrics->superscriptOffset = aMetrics->xHeight;
        aMetrics->subscriptOffset = aMetrics->xHeight;

        return;
    }

    const FT_Size_Metrics& ftMetrics = mFace->size->metrics;

    gfxFloat emHeight;
    // Scale for vertical design metric conversion: pixels per design unit.
    gfxFloat yScale;
    if (FT_IS_SCALABLE(mFace)) {
        // Prefer FT_Size_Metrics::x_scale to x_ppem as x_ppem does not
        // have subpixel accuracy.
        //
        // FT_Size_Metrics::y_scale is in 16.16 fixed point format.  Its
        // (fractional) value is a factor that converts vertical metrics from
        // design units to units of 1/64 pixels, so that the result may be
        // interpreted as pixels in 26.6 fixed point format.
        yScale = FLOAT_FROM_26_6(FLOAT_FROM_16_16(ftMetrics.y_scale));
        emHeight = mFace->units_per_EM * yScale;
    } else { // Not scalable.
        // FT_Size_Metrics doc says x_scale is "only relevant for scalable
        // font formats".
        gfxFloat emUnit = mFace->units_per_EM;
        emHeight = ftMetrics.y_ppem;
        yScale = emHeight / emUnit;
    }

    TT_OS2 *os2 =
        static_cast<TT_OS2*>(FT_Get_Sfnt_Table(mFace, ft_sfnt_os2));

    aMetrics->maxAscent = FLOAT_FROM_26_6(ftMetrics.ascender);
    aMetrics->maxDescent = -FLOAT_FROM_26_6(ftMetrics.descender);
    aMetrics->maxAdvance = FLOAT_FROM_26_6(ftMetrics.max_advance);

    gfxFloat lineHeight;
    if (os2 && os2->sTypoAscender) {
        aMetrics->emAscent = os2->sTypoAscender * yScale;
        aMetrics->emDescent = -os2->sTypoDescender * yScale;
        FT_Short typoHeight =
            os2->sTypoAscender - os2->sTypoDescender + os2->sTypoLineGap;
        lineHeight = typoHeight * yScale;

        // maxAscent/maxDescent get used for frame heights, and some fonts
        // don't have the HHEA table ascent/descent set (bug 279032).
        if (aMetrics->emAscent > aMetrics->maxAscent)
            aMetrics->maxAscent = aMetrics->emAscent;
        if (aMetrics->emDescent > aMetrics->maxDescent)
            aMetrics->maxDescent = aMetrics->emDescent;
    } else {
        aMetrics->emAscent = aMetrics->maxAscent;
        aMetrics->emDescent = aMetrics->maxDescent;
        lineHeight = FLOAT_FROM_26_6(ftMetrics.height);
    }

    cairo_text_extents_t extents;
    *aSpaceGlyph = GetCharExtents(' ', &extents);
    if (*aSpaceGlyph) {
        aMetrics->spaceWidth = extents.x_advance;
    } else {
        aMetrics->spaceWidth = aMetrics->maxAdvance; // guess
    }

    aMetrics->zeroOrAveCharWidth = 0.0;
    if (GetCharExtents('0', &extents)) {
        aMetrics->zeroOrAveCharWidth = extents.x_advance;
    }

    // Prefering a measured x over sxHeight because sxHeight doesn't consider
    // hinting, but maybe the x extents are not quite right in some fancy
    // script fonts.  CSS 2.1 suggests possibly using the height of an "o",
    // which would have a more consistent glyph across fonts.
    if (GetCharExtents('x', &extents) && extents.y_bearing < 0.0) {
        aMetrics->xHeight = -extents.y_bearing;
        aMetrics->aveCharWidth = extents.x_advance;
    } else {
        if (os2 && os2->sxHeight) {
            aMetrics->xHeight = os2->sxHeight * yScale;
        } else {
            // CSS 2.1, section 4.3.2 Lengths: "In the cases where it is
            // impossible or impractical to determine the x-height, a value of
            // 0.5em should be used."
            aMetrics->xHeight = 0.5 * emHeight;
        }
        aMetrics->aveCharWidth = 0.0; // updated below
    }
    // aveCharWidth is used for the width of text input elements so be
    // liberal rather than conservative in the estimate.
    if (os2 && os2->xAvgCharWidth) {
        // Round to pixels as this is compared with maxAdvance to guess
        // whether this is a fixed width font.
        gfxFloat avgCharWidth =
            ScaleRoundDesignUnits(os2->xAvgCharWidth, ftMetrics.x_scale);
        aMetrics->aveCharWidth =
            PR_MAX(aMetrics->aveCharWidth, avgCharWidth);
    }
    aMetrics->aveCharWidth =
        PR_MAX(aMetrics->aveCharWidth, aMetrics->zeroOrAveCharWidth);
    if (aMetrics->aveCharWidth == 0.0) {
        aMetrics->aveCharWidth = aMetrics->spaceWidth;
    }
    if (aMetrics->zeroOrAveCharWidth == 0.0) {
        aMetrics->zeroOrAveCharWidth = aMetrics->aveCharWidth;
    }
    // Apparently hinting can mean that max_advance is not always accurate.
    aMetrics->maxAdvance =
        PR_MAX(aMetrics->maxAdvance, aMetrics->aveCharWidth);

    // gfxFont::Metrics::underlineOffset is the position of the top of the
    // underline.
    //
    // FT_FaceRec documentation describes underline_position as "the
    // center of the underlining stem".  This was the original definition
    // of the PostScript metric, but in the PostScript table of OpenType
    // fonts the metric is "the top of the underline"
    // (http://www.microsoft.com/typography/otspec/post.htm), and FreeType
    // (up to version 2.3.7) doesn't make any adjustment.
    //
    // Therefore get the underline position directly from the table
    // ourselves when this table exists.  Use FreeType's metrics for
    // other (including older PostScript) fonts.
    if (mFace->underline_position && mFace->underline_thickness) {
        aMetrics->underlineSize = mFace->underline_thickness * yScale;
        TT_Postscript *post = static_cast<TT_Postscript*>
            (FT_Get_Sfnt_Table(mFace, ft_sfnt_post));
        if (post && post->underlinePosition) {
            aMetrics->underlineOffset = post->underlinePosition * yScale;
        } else {
            aMetrics->underlineOffset = mFace->underline_position * yScale
                + 0.5 * aMetrics->underlineSize;
        }
    } else { // No underline info.
        // Imitate Pango.
        aMetrics->underlineSize = emHeight / 14.0;
        aMetrics->underlineOffset = -aMetrics->underlineSize;
    }

    if (os2 && os2->yStrikeoutSize && os2->yStrikeoutPosition) {
        aMetrics->strikeoutSize = os2->yStrikeoutSize * yScale;
        aMetrics->strikeoutOffset = os2->yStrikeoutPosition * yScale;
    } else { // No strikeout info.
        aMetrics->strikeoutSize = aMetrics->underlineSize;
        // Use OpenType spec's suggested position for Roman font.
        aMetrics->strikeoutOffset = emHeight * 409.0 / 2048.0
            + 0.5 * aMetrics->strikeoutSize;
    }
    SnapLineToPixels(aMetrics->strikeoutOffset, aMetrics->strikeoutSize);

    if (os2 && os2->ySuperscriptYOffset) {
        gfxFloat val = ScaleRoundDesignUnits(os2->ySuperscriptYOffset,
                                             ftMetrics.y_scale);
        aMetrics->superscriptOffset = PR_MAX(1.0, val);
    } else {
        aMetrics->superscriptOffset = aMetrics->xHeight;
    }
    
    if (os2 && os2->ySubscriptYOffset) {
        gfxFloat val = ScaleRoundDesignUnits(os2->ySubscriptYOffset,
                                             ftMetrics.y_scale);
        // some fonts have the incorrect sign. 
        val = fabs(val);
        aMetrics->subscriptOffset = PR_MAX(1.0, val);
    } else {
        aMetrics->subscriptOffset = aMetrics->xHeight;
    }

    aMetrics->maxHeight = aMetrics->maxAscent + aMetrics->maxDescent;

    // Make the line height an integer number of pixels so that lines will be
    // equally spaced (rather than just being snapped to pixels, some up and
    // some down).  Layout calculates line height from the emHeight +
    // internalLeading + externalLeading, but first each of these is rounded
    // to layout units.  To ensure that the result is an integer number of
    // pixels, round each of the components to pixels.
    aMetrics->emHeight = NS_floor(emHeight + 0.5);

    // maxHeight will normally be an integer, but round anyway in case
    // FreeType is configured differently.
    aMetrics->internalLeading =
        NS_floor(aMetrics->maxHeight - aMetrics->emHeight + 0.5);

    // Text input boxes currently don't work well with lineHeight
    // significantly less than maxHeight (with Verdana, for example).
    lineHeight = NS_floor(PR_MAX(lineHeight, aMetrics->maxHeight) + 0.5);
    aMetrics->externalLeading =
        lineHeight - aMetrics->internalLeading - aMetrics->emHeight;

    // Ensure emAscent + emDescent == emHeight
    gfxFloat sum = aMetrics->emAscent + aMetrics->emDescent;
    aMetrics->emAscent = sum > 0.0 ?
        aMetrics->emAscent * aMetrics->emHeight / sum : 0.0;
    aMetrics->emDescent = aMetrics->emHeight - aMetrics->emAscent;
}

const gfxFont::Metrics&
gfxFcFont::GetMetrics()
{
    if (mHasMetrics)
        return mMetrics;

    if (NS_UNLIKELY(GetStyle()->size <= 0.0)) {
        new(&mMetrics) gfxFont::Metrics(); // zero initialize
        mSpaceGlyph = 0;
    } else {
        LockedFTFace(this).GetMetrics(&mMetrics, &mSpaceGlyph);
    }

    SanitizeMetrics(&mMetrics, PR_FALSE);

#if 0
    //    printf("font name: %s %f\n", NS_ConvertUTF16toUTF8(GetName()).get(), GetStyle()->size);
    //    printf ("pango font %s\n", pango_font_description_to_string (pango_font_describe (font)));

    fprintf (stderr, "Font: %s\n", NS_ConvertUTF16toUTF8(GetName()).get());
    fprintf (stderr, "    emHeight: %f emAscent: %f emDescent: %f\n", mMetrics.emHeight, mMetrics.emAscent, mMetrics.emDescent);
    fprintf (stderr, "    maxAscent: %f maxDescent: %f\n", mMetrics.maxAscent, mMetrics.maxDescent);
    fprintf (stderr, "    internalLeading: %f externalLeading: %f\n", mMetrics.externalLeading, mMetrics.internalLeading);
    fprintf (stderr, "    spaceWidth: %f aveCharWidth: %f xHeight: %f\n", mMetrics.spaceWidth, mMetrics.aveCharWidth, mMetrics.xHeight);
    fprintf (stderr, "    uOff: %f uSize: %f stOff: %f stSize: %f suOff: %f suSize: %f\n", mMetrics.underlineOffset, mMetrics.underlineSize, mMetrics.strikeoutOffset, mMetrics.strikeoutSize, mMetrics.superscriptOffset, mMetrics.subscriptOffset);
#endif

    mHasMetrics = PR_TRUE;
    return mMetrics;
}

nsString
gfxFcFont::GetUniqueName()
{
    return GetName();
}

/**
 ** gfxTextRun
 * 
 * A serious problem:
 *
 * -- We draw with a font that's hinted for the CTM, but we measure with a font
 * hinted to the identity matrix, so our "bounding metrics" may not be accurate.
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
    return speed && !isRTL && PANGO_IS_FC_FONT(GetBasePangoFont());
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

static void ReleaseDownloadedFontEntry(void *data)
{
    gfxDownloadedFcFontEntry *downloadedFontEntry =
        static_cast<gfxDownloadedFcFontEntry*>(data);
    NS_RELEASE(downloadedFontEntry);
}

// This will fetch an existing scaled_font if one exists.
static cairo_scaled_font_t *
CreateScaledFont(FcPattern *aPattern)
{
    cairo_font_face_t *face = cairo_ft_font_face_create_for_pattern(aPattern);

    // If the face is created from a web font entry, hold a reference to the
    // font entry to keep the font face data.
    gfxDownloadedFcFontEntry *downloadedFontEntry =
        GetDownloadedFontEntry(aPattern);
    if (downloadedFontEntry &&
        cairo_font_face_status(face) == CAIRO_STATUS_SUCCESS) {
        static cairo_user_data_key_t sFontEntryKey;

        // Check whether this is a new cairo face
        void *currentEntry =
            cairo_font_face_get_user_data(face, &sFontEntryKey);
        if (!currentEntry) {
            NS_ADDREF(downloadedFontEntry);
            cairo_font_face_set_user_data(face, &sFontEntryKey,
                                          downloadedFontEntry,
                                          ReleaseDownloadedFontEntry);
        } else {
            NS_ASSERTION(currentEntry == downloadedFontEntry,
                         "Unexpected cairo font face!");
        }
    }

    double size = GetPixelSize(aPattern);
        
    cairo_matrix_t fontMatrix;
    FcMatrix *fcMatrix;
    if (FcPatternGetMatrix(aPattern, FC_MATRIX, 0, &fcMatrix) == FcResultMatch)
        cairo_matrix_init(&fontMatrix, fcMatrix->xx, -fcMatrix->yx, -fcMatrix->xy, fcMatrix->yy, 0, 0);
    else
        cairo_matrix_init_identity(&fontMatrix);
    cairo_matrix_scale(&fontMatrix, size, size);

    // The cairo_scaled_font is created with a unit ctm so that metrics and
    // positions are in user space, but this means that hinting effects will
    // not be estimated accurately for non-unit transformations.
    cairo_matrix_t identityMatrix;
    cairo_matrix_init_identity(&identityMatrix);

    // Font options are set explicitly here to improve cairo's caching
    // behavior and to record the relevant parts of the pattern for
    // SetupCairoFont (so that the pattern can be released).
    //
    // Most font_options have already been set as defaults on the FcPattern
    // with cairo_ft_font_options_substitute(), then user and system
    // fontconfig configurations were applied.  The resulting font_options
    // have been recorded on the face during
    // cairo_ft_font_face_create_for_pattern().
    //
    // None of the settings here cause this scaled_font to behave any
    // differently from how it would behave if it were created from the same
    // face with default font_options.
    //
    // We set options explicitly so that the same scaled_font will be found in
    // the cairo_scaled_font_map when cairo loads glyphs from a context with
    // the same font_face, font_matrix, ctm, and surface font_options.
    //
    // Unfortunately, _cairo_scaled_font_keys_equal doesn't know about the
    // font_options on the cairo_ft_font_face, and doesn't consider default
    // option values to not match any explicit values.
    //
    // Even after cairo_set_scaled_font is used to set font_options for the
    // cairo context, when cairo looks for a scaled_font for the context, it
    // will look for a font with some option values from the target surface if
    // any values are left default on the context font_options.  If this
    // scaled_font is created with default font_options, cairo will not find
    // it.
    cairo_font_options_t *fontOptions = cairo_font_options_create();

    // The one option not recorded in the pattern is hint_metrics, which will
    // affect glyph metrics.  The default behaves as CAIRO_HINT_METRICS_ON.
    // We should be considering the font_options of the surface on which this
    // font will be used, but currently we don't have different gfxFonts for
    // different surface font_options, so we'll create a font suitable for the
    // Screen. Image and xlib surfaces default to CAIRO_HINT_METRICS_ON.
    cairo_font_options_set_hint_metrics(fontOptions, CAIRO_HINT_METRICS_ON);

    // The remaining options have been recorded on the pattern and the face.
    // _cairo_ft_options_merge has some logic to decide which options from the
    // scaled_font or from the cairo_ft_font_face take priority in the way the
    // font behaves.
    //
    // In the majority of cases, _cairo_ft_options_merge uses the options from
    // the cairo_ft_font_face, so sometimes it is not so important which
    // values are set here so long as they are not defaults, but we'll set
    // them to the exact values that we expect from the font, to be consistent
    // and to protect against changes in cairo.
    //
    // In some cases, _cairo_ft_options_merge uses some options from the
    // scaled_font's font_options rather than options on the
    // cairo_ft_font_face (from fontconfig).
    // https://bugs.freedesktop.org/show_bug.cgi?id=11838
    //
    // Surface font options were set on the pattern in
    // cairo_ft_font_options_substitute.  If fontconfig has changed the
    // hint_style then that is what the user (or distribution) wants, so we
    // use the setting from the FcPattern.
    //
    // Fallback values here mirror treatment of defaults in cairo-ft-font.c.
    FcBool hinting;
    if (FcPatternGetBool(aPattern, FC_HINTING, 0, &hinting) != FcResultMatch) {
        hinting = FcTrue;
    }
    cairo_hint_style_t hint_style;
    if (!hinting) {
        hint_style = CAIRO_HINT_STYLE_NONE;
    } else {
#ifdef FC_HINT_STYLE  // FC_HINT_STYLE is available from fontconfig 2.2.91.
        int fc_hintstyle;
        if (FcPatternGetInteger(aPattern, FC_HINT_STYLE,
                                0, &fc_hintstyle        ) != FcResultMatch) {
            fc_hintstyle = FC_HINT_FULL;
        }
        switch (fc_hintstyle) {
            case FC_HINT_NONE:
                hint_style = CAIRO_HINT_STYLE_NONE;
                break;
            case FC_HINT_SLIGHT:
                hint_style = CAIRO_HINT_STYLE_SLIGHT;
                break;
            case FC_HINT_MEDIUM:
            default: // This fallback mirrors _get_pattern_ft_options in cairo.
                hint_style = CAIRO_HINT_STYLE_MEDIUM;
                break;
            case FC_HINT_FULL:
                hint_style = CAIRO_HINT_STYLE_FULL;
                break;
        }
#else // no FC_HINT_STYLE
        hint_style = CAIRO_HINT_STYLE_FULL;
#endif
    }
    cairo_font_options_set_hint_style(fontOptions, hint_style);

    int rgba;
    if (FcPatternGetInteger(aPattern,
                            FC_RGBA, 0, &rgba) != FcResultMatch) {
        rgba = FC_RGBA_UNKNOWN;
    }
    cairo_subpixel_order_t subpixel_order = CAIRO_SUBPIXEL_ORDER_DEFAULT;
    switch (rgba) {
        case FC_RGBA_UNKNOWN:
        case FC_RGBA_NONE:
        default:
            // There is no CAIRO_SUBPIXEL_ORDER_NONE.  Subpixel antialiasing
            // is disabled through cairo_antialias_t.
            rgba = FC_RGBA_NONE;
            // subpixel_order won't be used by the font as we won't use
            // CAIRO_ANTIALIAS_SUBPIXEL, but don't leave it at default for
            // caching reasons described above.  Fall through:
        case FC_RGBA_RGB:
            subpixel_order = CAIRO_SUBPIXEL_ORDER_RGB;
            break;
        case FC_RGBA_BGR:
            subpixel_order = CAIRO_SUBPIXEL_ORDER_BGR;
            break;
        case FC_RGBA_VRGB:
            subpixel_order = CAIRO_SUBPIXEL_ORDER_VRGB;
            break;
        case FC_RGBA_VBGR:
            subpixel_order = CAIRO_SUBPIXEL_ORDER_VBGR;
            break;
    }
    cairo_font_options_set_subpixel_order(fontOptions, subpixel_order);

    FcBool fc_antialias;
    if (FcPatternGetBool(aPattern,
                         FC_ANTIALIAS, 0, &fc_antialias) != FcResultMatch) {
        fc_antialias = FcTrue;
    }
    cairo_antialias_t antialias;
    if (!fc_antialias) {
        antialias = CAIRO_ANTIALIAS_NONE;
    } else if (rgba == FC_RGBA_NONE) {
        antialias = CAIRO_ANTIALIAS_GRAY;
    } else {
        antialias = CAIRO_ANTIALIAS_SUBPIXEL;
    }
    cairo_font_options_set_antialias(fontOptions, antialias);

    cairo_scaled_font_t *scaledFont =
        cairo_scaled_font_create(face, &fontMatrix, &identityMatrix,
                                 fontOptions);

    cairo_font_options_destroy(fontOptions);
    cairo_font_face_destroy(face);

    NS_ASSERTION(cairo_scaled_font_status(scaledFont) == CAIRO_STATUS_SUCCESS,
                 "Failed to create scaled font");
    return scaledFont;
}

PRBool
gfxFcFont::SetupCairoFont(gfxContext *aContext)
{
    cairo_t *cr = aContext->GetCairo();

    // The scaled font ctm is not relevant right here because
    // cairo_set_scaled_font does not record the scaled font itself, but
    // merely the font_face, font_matrix, font_options.  The scaled_font used
    // for the target can be different from the scaled_font passed to
    // cairo_set_scaled_font.  (Unfortunately we have measured only for an
    // identity ctm.)
    cairo_scaled_font_t *cairoFont = CairoScaledFont();

    if (cairo_scaled_font_status(cairoFont) != CAIRO_STATUS_SUCCESS) {
        // Don't cairo_set_scaled_font as that would propagate the error to
        // the cairo_t, precluding any further drawing.
        return PR_FALSE;
    }
    // Thoughts on which font_options to set on the context:
    //
    // cairoFont has been created for screen rendering.
    //
    // When the context is being used for screen rendering, we should set
    // font_options such that the same scaled_font gets used (when the ctm is
    // the same).  The use of explicit font_options recorded in
    // CreateScaledFont ensures that this will happen.
    //
    // XXXkt: For pdf and ps surfaces, I don't know whether it's better to
    // remove surface-specific options, or try to draw with the same
    // scaled_font that was used to measure.  As the same font_face is being
    // used, its font_options will often override some values anyway (unless
    // perhaps we remove those from the FcPattern at face creation).
    //
    // I can't see any significant difference in printing, irrespective of
    // what is set here.  It's too late to change things here as measuring has
    // already taken place.  We should really be measuring with a different
    // font for pdf and ps surfaces (bug 403513).
    cairo_set_scaled_font(cr, cairoFont);
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
            // set glyph info for the UTF-16 low surrogate
            aTextRun->SetGlyphs(aUTF16Offset, g.SetComplex(PR_FALSE, PR_FALSE, 0), nsnull);
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
        !IS_EMPTY_GLYPH(aGlyphs[0].glyph) &&
        gfxTextRun::CompressedGlyph::IsSimpleAdvance(advance) &&
        gfxTextRun::CompressedGlyph::IsSimpleGlyphID(aGlyphs[0].glyph)) {
        aTextRun->SetSimpleGlyph(utf16Offset,
                                 g.SetSimpleGlyph(advance, aGlyphs[0].glyph));
    } else {
        nsAutoTArray<gfxTextRun::DetailedGlyph,10> detailedGlyphs;
        if (!detailedGlyphs.AppendElements(aGlyphCount))
            return NS_ERROR_OUT_OF_MEMORY;

        PRInt32 direction = aTextRun->IsRightToLeft() ? -1 : 1;
        PRUint32 pangoIndex = direction > 0 ? 0 : aGlyphCount - 1;
        PRUint32 detailedIndex = 0;
        for (PRUint32 i = 0; i < aGlyphCount; ++i) {
            const PangoGlyphInfo &glyph = aGlyphs[pangoIndex];
            pangoIndex += direction;
            // The zero width characters return empty glyph ID at
            // shaping; we should skip these.
            if (IS_EMPTY_GLYPH(glyph.glyph))
                continue;

            gfxTextRun::DetailedGlyph *details = &detailedGlyphs[detailedIndex];
            ++detailedIndex;

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
        g.SetComplex(atClusterStart, PR_TRUE, detailedIndex);
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
gfxPangoFontGroup::SetGlyphs(gfxTextRun *aTextRun,
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
            if (IS_MISSING_GLYPH(glyphs[glyphIndex].glyph)) {
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
    PangoFont *pangofont = GetBasePangoFont();
    PangoFcFont *fcfont = PANGO_FC_FONT (pangofont);
    gfxFcFont *gfxFont = gfxPangoFcFont::GfxFont(GFX_PANGO_FC_FONT(pangofont));
    PRUint32 utf16Offset = 0;
    gfxTextRun::CompressedGlyph g;
    const PRUint32 appUnitsPerDevUnit = aTextRun->GetAppUnitsPerDevUnit();

    aTextRun->AddGlyphRun(gfxFont, 0);

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

            cairo_text_extents_t extents;
            gfxFont->GetGlyphExtents(glyph, &extents);

            PRInt32 advance = NS_lround(extents.x_advance * appUnitsPerDevUnit);
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

void 
gfxPangoFontGroup::CreateGlyphRunsItemizing(gfxTextRun *aTextRun,
                                            const gchar *aUTF8, PRUint32 aUTF8Length,
                                            PRUint32 aUTF8HeaderLen)
{
    // This font group and gfxPangoFontMap are recorded on the PangoContext
    // passed to pango_itemize_with_base_dir().
    //
    // pango_itemize_with_base_dir() divides the string into substrings for
    // each language, and queries gfxPangoFontMap::load_fontset() to provide
    // ordered lists of fonts for each language.  gfxPangoFontMap passes the
    // request back to this font group, which returns a gfxFcPangoFontSet
    // handling the font sorting/selection.
    //
    // For each character, pango_itemize_with_base_dir searches through these
    // lists of fonts for a font with support for the character.  The
    // PangoItems returned represent substrings (or runs) of consectutive
    // characters to be shaped with the same PangoFont and having the same
    // script.
    //
    // The PangoFonts in the PangoItems are from the gfxPangoFontMap and so
    // each have a gfxFont.  This gfxFont represents the same face as the
    // PangoFont and so can render the same glyphs in the same way as
    // pango_shape measures.

    PangoContext *context = GetPangoContext();
    // we should set this to null if we don't have a text language from the page...
    // except that we almost always have something...
    pango_context_set_language(context, mPangoLanguage);
    SetFontGroup(context, this);

    PangoDirection dir = aTextRun->IsRightToLeft() ? PANGO_DIRECTION_RTL : PANGO_DIRECTION_LTR;
    GList *items = pango_itemize_with_base_dir(context, dir, aUTF8, 0, aUTF8Length, nsnull, nsnull);

    PRUint32 utf16Offset = 0;
#ifdef DEBUG
    PRBool isRTL = aTextRun->IsRightToLeft();
#endif
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

        gfxFcFont *font =
            gfxPangoFcFont::GfxFont(GFX_PANGO_FC_FONT(item->analysis.font));

        nsresult rv = aTextRun->AddGlyphRun(font, utf16Offset);
        if (NS_FAILED(rv)) {
            NS_ERROR("AddGlyphRun Failed");
            goto out;
        }

        PRUint32 spaceWidth =
            moz_pango_units_from_double(font->GetMetrics().spaceWidth);

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
            SetGlyphs(aTextRun, text, len, &utf16Offset, glyphString, spaceWidth, PR_FALSE);
        }
    }

out:
    if (glyphString)
        pango_glyph_string_free(glyphString);

    for (pos = items; pos; pos = pos->next)
        pango_item_free((PangoItem *)pos->data);

    if (items)
        g_list_free(items);

    g_object_unref(context);
}

/* static */
PangoLanguage *
GuessPangoLanguage(const nsACString& aLangGroup)
{
    // See if the lang group needs to be translated from Mozilla's
    // internal mapping into fontconfig's
    nsCAutoString lang;
    gfxFontconfigUtils::GetSampleLangForGroup(aLangGroup, &lang);

    if (lang.IsEmpty())
        return NULL;

    return pango_language_from_string(lang.get());
}

#ifdef MOZ_WIDGET_GTK2
/***************************************************************************
 *
 * This function must be last in the file because it uses the system cairo
 * library.  Above this point the cairo library used is the tree cairo if
 * MOZ_TREE_CAIRO.
 */

#if MOZ_TREE_CAIRO
// Tree cairo symbols have different names.  Disable their activation through
// preprocessor macros.
#undef cairo_ft_font_options_substitute

// The system cairo functions are not declared because the include paths cause
// the gdk headers to pick up the tree cairo.h.
extern "C" {
NS_VISIBILITY_DEFAULT void
cairo_ft_font_options_substitute (const cairo_font_options_t *options,
                                  FcPattern                  *pattern);
}
#endif

static void
ApplyGdkScreenFontOptions(FcPattern *aPattern)
{
    const cairo_font_options_t *options =
        gdk_screen_get_font_options(gdk_screen_get_default());

    cairo_ft_font_options_substitute(options, aPattern);
}

#endif // MOZ_WIDGET_GTK2

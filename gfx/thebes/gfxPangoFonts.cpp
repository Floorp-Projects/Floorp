/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#define PANGO_ENABLE_BACKEND
#define PANGO_ENABLE_ENGINE

#include "prtypes.h"
#include "prlink.h"
#include "gfxTypes.h"

#include "nsTArray.h"

#include "gfxContext.h"
#ifdef MOZ_WIDGET_GTK2
#include "gfxPlatformGtk.h"
#endif
#ifdef MOZ_WIDGET_QT
#include "gfxQtPlatform.h"
#endif
#include "gfxPangoFonts.h"
#include "gfxFT2FontBase.h"
#include "gfxFT2Utils.h"
#include "harfbuzz/hb.h"
#include "harfbuzz/hb-ot.h"
#include "gfxHarfBuzzShaper.h"
#ifdef MOZ_GRAPHITE
#include "gfxGraphiteShaper.h"
#endif
#include "nsUnicodeProperties.h"
#include "nsUnicodeScriptCodes.h"
#include "gfxFontconfigUtils.h"
#include "gfxUserFontSet.h"

#include <cairo.h>
#include <cairo-ft.h>

#include <fontconfig/fcfreetype.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>
#include <pango/pango-modules.h>
#include <pango/pangofc-fontmap.h>

#ifdef MOZ_WIDGET_GTK2
#include <gdk/gdk.h>
#endif

#include <math.h>

using namespace mozilla;
using namespace mozilla::unicode;

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

#define PRINTING_FC_PROPERTY "gfx.printing"

struct gfxPangoFcFont;

// Same as pango_units_from_double from Pango 1.16 (but not in older versions)
int moz_pango_units_from_double(double d) {
    return NS_lround(d * FLOAT_PANGO_SCALE);
}

static PangoLanguage *GuessPangoLanguage(nsIAtom *aLanguage);

static cairo_scaled_font_t *
CreateScaledFont(FcPattern *aPattern, cairo_font_face_t *aFace);
static void SetMissingGlyphs(gfxShapedWord *aShapedWord, const gchar *aUTF8,
                             PRUint32 aUTF8Length, PRUint32 *aUTF16Offset,
                             gfxFont *aFont);

static PangoFontMap *gPangoFontMap;
static PangoFontMap *GetPangoFontMap();
static bool gUseFontMapProperty;

static FT_Library gFTLibrary;

template <class T>
class gfxGObjectRefTraits : public nsPointerRefTraits<T> {
public:
    static void Release(T *aPtr) { g_object_unref(aPtr); }
    static void AddRef(T *aPtr) { g_object_ref(aPtr); }
};

template <>
class nsAutoRefTraits<PangoFont> : public gfxGObjectRefTraits<PangoFont> { };

template <>
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

static PRFuncPtr
FindFunctionSymbol(const char *name)
{
    PRLibrary *lib = nsnull;
    PRFuncPtr result = PR_FindFunctionSymbolAndLibrary(name, &lib);
    if (lib) {
        PR_UnloadLibrary(lib);
    }

    return result;
}

static bool HasChar(FcPattern *aFont, FcChar32 wc)
{
    FcCharSet *charset = NULL;
    FcPatternGetCharSet(aFont, FC_CHARSET, 0, &charset);

    return charset && FcCharSetHasChar(charset, wc);
}

/**
 * gfxFcFontEntry:
 *
 * An abstract base class of for gfxFontEntry implementations used by
 * gfxFcFont and gfxUserFontSet.
 */

class gfxFcFontEntry : public gfxFontEntry {
public:
    // For all FontEntrys attached to gfxFcFonts, there will be only one
    // pattern in this array.  This is always a font pattern, not a fully
    // resolved pattern.  gfxFcFont only uses this to construct a PangoFont.
    //
    // FontEntrys for src:local() fonts in gfxUserFontSet may return more than
    // one pattern.  (See comment in gfxUserFcFontEntry.)
    const nsTArray< nsCountedRef<FcPattern> >& GetPatterns()
    {
        return mPatterns;
    }

    bool ShouldUseHarfBuzz(PRInt32 aRunScript);
    void SkipHarfBuzz() { mSkipHarfBuzz = true; }

    static gfxFcFontEntry *LookupFontEntry(cairo_font_face_t *aFace)
    {
        return static_cast<gfxFcFontEntry*>
            (cairo_font_face_get_user_data(aFace, &sFontEntryKey));
    }

    // override the default impl in gfxFontEntry because we don't organize
    // gfxFcFontEntries in families; just read the name from fontconfig
    virtual nsString FamilyName() const;

    // override the gfxFontEntry impl to read the name from fontconfig
    // instead of trying to get the 'name' table, as we don't implement
    // GetFontTable() here
    virtual nsString RealFaceName();

    // This is needed to make gfxFontEntry::HasCharacter(aCh) work.
    virtual bool TestCharacterMap(PRUint32 aCh)
    {
        for (PRUint32 i = 0; i < mPatterns.Length(); ++i) {
            if (HasChar(mPatterns[i], aCh)) {
                return true;
            }
        }
        return false;
    }

protected:
    gfxFcFontEntry(const nsAString& aName)
        : gfxFontEntry(aName),
          mSkipHarfBuzz(false), mSkipGraphiteCheck(false)
    {
    }

#ifdef MOZ_GRAPHITE
    virtual void CheckForGraphiteTables();
#endif

    // One pattern is the common case and some subclasses rely on successful
    // addition of the first element to the array.
    nsAutoTArray<nsCountedRef<FcPattern>,1> mPatterns;
    bool mSkipHarfBuzz;
    bool mSkipGraphiteCheck;

    static cairo_user_data_key_t sFontEntryKey;
};

cairo_user_data_key_t gfxFcFontEntry::sFontEntryKey;

nsString
gfxFcFontEntry::FamilyName() const
{
    if (mIsUserFont) {
        // for user fonts, we want the name of the family
        // as specified in the user font set
        return gfxFontEntry::FamilyName();
    }
    FcChar8 *familyname;
    if (!mPatterns.IsEmpty() &&
        FcPatternGetString(mPatterns[0],
                           FC_FAMILY, 0, &familyname) == FcResultMatch) {
        return NS_ConvertUTF8toUTF16((const char*)familyname);
    }
    return gfxFontEntry::FamilyName();
}

nsString
gfxFcFontEntry::RealFaceName()
{
    FcChar8 *name;
    if (!mPatterns.IsEmpty()) {
        if (FcPatternGetString(mPatterns[0],
                               FC_FULLNAME, 0, &name) == FcResultMatch) {
            return NS_ConvertUTF8toUTF16((const char*)name);
        }
        if (FcPatternGetString(mPatterns[0],
                               FC_FAMILY, 0, &name) == FcResultMatch) {
            NS_ConvertUTF8toUTF16 result((const char*)name);
            if (FcPatternGetString(mPatterns[0],
                                   FC_STYLE, 0, &name) == FcResultMatch) {
                result.AppendLiteral(" ");
                AppendUTF8toUTF16((const char*)name, result);
            }
            return result;
        }
    }
    return gfxFontEntry::RealFaceName();
}

#ifdef MOZ_GRAPHITE
void
gfxFcFontEntry::CheckForGraphiteTables()
{
    FcChar8 *capability;
    mHasGraphiteTables =
        !mPatterns.IsEmpty() &&
        FcPatternGetString(mPatterns[0],
                           FC_CAPABILITY, 0, &capability) == FcResultMatch &&
        FcStrStr(capability, gfxFontconfigUtils::ToFcChar8("ttable:Silf"));
}
#endif

bool
gfxFcFontEntry::ShouldUseHarfBuzz(PRInt32 aRunScript) {
    if (mSkipHarfBuzz ||
        !gfxPlatform::GetPlatform()->UseHarfBuzzForScript(aRunScript))
    {
        return false;
    }

    if (mSkipGraphiteCheck) {
        return true;
    }

    // Check whether to fall back to Pango for Graphite shaping.
    // pango-graphite checks for ttable:Silf.
    FcChar8 *capability;
    // FontEntries used at shaping have only one pattern.
    if (mPatterns.IsEmpty() ||
        FcPatternGetString(mPatterns[0],
                           FC_CAPABILITY, 0, &capability) == FcResultNoMatch ||
        !FcStrStr(capability, gfxFontconfigUtils::ToFcChar8("ttable:Silf")))
    {
        mSkipGraphiteCheck = true;
        return true;
    }

    // Mimicing gfxHarfBuzzShaper::ShapeWord
    hb_script_t script = (aRunScript <= MOZ_SCRIPT_INHERITED) ?
        HB_SCRIPT_LATIN :
        hb_script_t(GetScriptTagForCode(aRunScript));

    // Prefer HarfBuzz if the font also has support for OpenType shaping of
    // this script.
    const FcChar8 otCapTemplate[] = "otlayout:XXXX";
    FcChar8 otCap[NS_ARRAY_LENGTH(otCapTemplate)];
    memcpy(otCap, otCapTemplate, ArrayLength(otCapTemplate));
    // Subtract 5, for 4 characters and NUL. 
    const PRUint32 scriptOffset = ArrayLength(otCapTemplate) - 5;

    hb_tag_t tags[2];
    hb_ot_tags_from_script(script, &tags[0], &tags[1]);
    for (int i = 0; i < 2; ++i) {
        hb_tag_t scriptTag = tags[i];
        if (scriptTag == HB_TAG('D','F','L','T')) { // e.g. HB_SCRIPT_UNKNOWN
            continue;
        }

        // FcChar8 is unsigned so truncates appropriately.
        otCap[scriptOffset + 0] = scriptTag >> 24;
        otCap[scriptOffset + 1] = scriptTag >> 16;
        otCap[scriptOffset + 2] = scriptTag >> 8;
        otCap[scriptOffset + 3] = scriptTag;
        if (FcStrStr(capability, otCap)) {
            return true;
        }
    }

    return false; // use Pango for Graphite
}

/**
 * gfxSystemFcFontEntry:
 *
 * An implementation of gfxFcFontEntry used by gfxFcFonts for system fonts,
 * including those from regular family-name based font selection as well as
 * those from src:local().
 *
 * All gfxFcFonts using the same cairo_font_face_t share the same FontEntry. 
 */

class gfxSystemFcFontEntry : public gfxFcFontEntry {
public:
    // For memory efficiency, aFontPattern should be a font pattern,
    // not a fully resolved pattern.
    gfxSystemFcFontEntry(cairo_font_face_t *aFontFace,
                         FcPattern *aFontPattern,
                         const nsAString& aName)
        : gfxFcFontEntry(aName), mFontFace(aFontFace)
    {
        cairo_font_face_reference(mFontFace);
        cairo_font_face_set_user_data(mFontFace, &sFontEntryKey, this, NULL);
        mPatterns.AppendElement();
        // mPatterns is an nsAutoTArray with 1 space always available, so the
        // AppendElement always succeeds.
        mPatterns[0] = aFontPattern;
    }

    ~gfxSystemFcFontEntry()
    {
        cairo_font_face_set_user_data(mFontFace, &sFontEntryKey, NULL, NULL);
        cairo_font_face_destroy(mFontFace);
    }
private:
    cairo_font_face_t *mFontFace;
};

// A namespace for @font-face family names in FcPatterns so that fontconfig
// aliases do not pick up families from @font-face rules and so that
// fontconfig rules can distinguish between web fonts and platform fonts.
// http://lists.freedesktop.org/archives/fontconfig/2008-November/003037.html
#define FONT_FACE_FAMILY_PREFIX "@font-face:"

/**
 * gfxUserFcFontEntry:
 *
 * An abstract class for objects in a gfxUserFontSet that can provide
 * FcPattern* handles to fonts.
 *
 * Separate implementations of this class support local fonts from src:local()
 * and web fonts from src:url().
 */

// There is a one-to-one correspondence between gfxUserFcFontEntry objects and
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

class gfxUserFcFontEntry : public gfxFcFontEntry {
protected:
    gfxUserFcFontEntry(const gfxProxyFontEntry &aProxyEntry)
        // store the family name
        : gfxFcFontEntry(aProxyEntry.mFamily->Name())
    {
        mItalic = aProxyEntry.mItalic;
        mWeight = aProxyEntry.mWeight;
        mStretch = aProxyEntry.mStretch;
        mIsUserFont = true;
    }

    // Helper function to change a pattern so that it matches the CSS style
    // descriptors and so gets properly sorted in font selection.  This also
    // avoids synthetic style effects being added by the renderer when the
    // style of the font itself does not match the descriptor provided by the
    // author.
    void AdjustPatternToCSS(FcPattern *aPattern);
};

void
gfxUserFcFontEntry::AdjustPatternToCSS(FcPattern *aPattern)
{
    int fontWeight = -1;
    FcPatternGetInteger(aPattern, FC_WEIGHT, 0, &fontWeight);
    int cssWeight = gfxFontconfigUtils::FcWeightForBaseWeight(mWeight / 100);
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

    int fontWidth = -1;
    FcPatternGetInteger(aPattern, FC_WIDTH, 0, &fontWidth);
    int cssWidth = gfxFontconfigUtils::FcWidthForThebesStretch(mStretch);
    if (cssWidth != fontWidth) {
        FcPatternDel(aPattern, FC_WIDTH);
        FcPatternAddInteger(aPattern, FC_WIDTH, cssWidth);
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
 * An implementation of gfxUserFcFontEntry for local fonts from src:local().
 *
 * This class is used only in gfxUserFontSet and for providing FcPattern*
 * handles to system fonts for font selection.  gfxFcFonts created from these
 * patterns will use gfxSystemFcFontEntrys, which may be shared with
 * gfxFcFonts from regular family-name based font selection.
 */

class gfxLocalFcFontEntry : public gfxUserFcFontEntry {
public:
    gfxLocalFcFontEntry(const gfxProxyFontEntry &aProxyEntry,
                        const nsTArray< nsCountedRef<FcPattern> >& aPatterns)
        : gfxUserFcFontEntry(aProxyEntry)
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
        mIsLocalUserFont = true;
    }
};

/**
 * gfxDownloadedFcFontEntry:
 *
 * An implementation of gfxFcFontEntry for web fonts from src:url().
 * 
 * When a cairo_font_face_t is created for these fonts, the cairo_font_face_t
 * keeps a reference to the FontEntry to keep the font data alive.
 */

class gfxDownloadedFcFontEntry : public gfxUserFcFontEntry {
public:
    // This takes ownership of the face and its underlying data
    gfxDownloadedFcFontEntry(const gfxProxyFontEntry &aProxyEntry,
                             const PRUint8 *aData, FT_Face aFace)
        : gfxUserFcFontEntry(aProxyEntry), mFontData(aData), mFace(aFace)
    {
        NS_PRECONDITION(aFace != NULL, "aFace is NULL!");
        InitPattern();
    }

    virtual ~gfxDownloadedFcFontEntry();

    // Returns true on success
    bool SetCairoFace(cairo_font_face_t *aFace);

    // Returns a PangoCoverage owned by the FontEntry.  The caller must add a
    // reference if it wishes to keep the PangoCoverage longer than the
    // lifetime of the FontEntry.
    PangoCoverage *GetPangoCoverage();

protected:
    void InitPattern();

    // mFontData holds the data used to instantiate the FT_Face;
    // this has to persist until we are finished with the face,
    // then be released with NS_Free().
    const PRUint8* mFontData;

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
    NS_Free((void*)mFontData);
}

typedef FcPattern* (*QueryFaceFunction)(const FT_Face face,
                                        const FcChar8 *file, int id,
                                        FcBlanks *blanks);

void
gfxDownloadedFcFontEntry::InitPattern()
{
    static QueryFaceFunction sQueryFacePtr =
        reinterpret_cast<QueryFaceFunction>
        (FindFunctionSymbol("FcFreeTypeQueryFace"));
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

static void ReleaseDownloadedFontEntry(void *data)
{
    gfxDownloadedFcFontEntry *downloadedFontEntry =
        static_cast<gfxDownloadedFcFontEntry*>(data);
    NS_RELEASE(downloadedFontEntry);
}

bool gfxDownloadedFcFontEntry::SetCairoFace(cairo_font_face_t *aFace)
{
    if (CAIRO_STATUS_SUCCESS !=
        cairo_font_face_set_user_data(aFace, &sFontEntryKey, this,
                                      ReleaseDownloadedFontEntry))
        return false;

    // Hold a reference to this font entry to keep the font face data.
    NS_ADDREF(this);
    return true;
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

class gfxFcFont : public gfxFT2FontBase {
public:
    virtual ~gfxFcFont();
    static already_AddRefed<gfxFcFont>
    GetOrMakeFont(FcPattern *aRequestedPattern, FcPattern *aFontPattern,
                  const gfxFontStyle *aFontStyle);

    // The PangoFont returned is owned by the gfxFcFont
    PangoFont *GetPangoFont() {
        if (!mPangoFont) {
            MakePangoFont();
        }
        return mPangoFont;
    }

protected:
    virtual bool ShapeWord(gfxContext *aContext,
                           gfxShapedWord *aShapedWord,
                           const PRUnichar *aString,
                           bool aPreferPlatformShaping);

    bool InitGlyphRunWithPango(gfxShapedWord *aTextRun,
                               const PRUnichar *aString);

private:
    gfxFcFont(cairo_scaled_font_t *aCairoFont, gfxFcFontEntry *aFontEntry,
              const gfxFontStyle *aFontStyle);

    void MakePangoFont();

    PangoFont *mPangoFont;

    // key for locating a gfxFcFont corresponding to a cairo_scaled_font
    static cairo_user_data_key_t sGfxFontKey;
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

    PangoCoverage *mCoverage;
    gfxFcFont *mGfxFont;

    // The caller promises to ensure that |aGfxFont| remains valid until the
    // new gfxPangoFcFont is destroyed.  See PangoFontToggleNotify.
    //
    // The gfxPangoFcFont holds a reference to |aFontPattern|.
    // Providing one of fontconfig's font patterns uses much less memory than
    // using a fully resolved pattern, because fontconfig's font patterns are
    // shared and will exist anyway.
    static nsReturnRef<PangoFont>
    NewFont(gfxFcFont *aGfxFont, FcPattern *aFontPattern);

    gfxFcFont *GfxFont() { return mGfxFont; }

    cairo_scaled_font_t *CairoFont()
    {
        return GfxFont()->CairoScaledFont();
    }

private:
    void SetFontMap();
};

struct gfxPangoFcFontClass {
    PangoFcFontClass parent_class;
};

G_DEFINE_TYPE (gfxPangoFcFont, gfx_pango_fc_font, PANGO_TYPE_FC_FONT)

/* static */ nsReturnRef<PangoFont>
gfxPangoFcFont::NewFont(gfxFcFont *aGfxFont, FcPattern *aFontPattern)
{
    // The font pattern is needed for pango_fc_font_finalize.
    gfxPangoFcFont *font = static_cast<gfxPangoFcFont*>
        (g_object_new(GFX_TYPE_PANGO_FC_FONT, "pattern", aFontPattern, NULL));

    font->mGfxFont = aGfxFont;
    font->SetFontMap();

    PangoFcFont *fc_font = &font->parent_instance;
    cairo_scaled_font_t *scaled_font = aGfxFont->CairoScaledFont();
    // Normally the is_hinted field of PangoFcFont is set based on the
    // FC_HINTING property on the pattern at construction, but this property
    // is not on an unresolved aFontPattern.  is_hinted is used by
    // pango_fc_font_kern_glyphs, which is sometimes used by
    // pango_ot_buffer_output.
    cairo_font_options_t *options = cairo_font_options_create();
    cairo_scaled_font_get_font_options(scaled_font, options);
    cairo_hint_style_t hint_style = cairo_font_options_get_hint_style(options);
    cairo_font_options_destroy(options);
    fc_font->is_hinted = hint_style != CAIRO_HINT_STYLE_NONE;

    // is_transformed does not appear to be used anywhere but looks
    // like it should be set.
    cairo_matrix_t matrix;
    cairo_scaled_font_get_font_matrix(scaled_font, &matrix);
    fc_font->is_transformed = (matrix.xy != 0.0 || matrix.yx != 0.0 ||
                               matrix.xx != matrix.yy);

    return nsReturnRef<PangoFont>(PANGO_FONT(font));
}

void
gfxPangoFcFont::SetFontMap()
{
    // PangoFcFont::get_coverage wants a PangoFcFontMap.  (PangoFcFontMap
    // would usually set this after calling PangoFcFontMap::create_font()
    // or new_font().)
    PangoFontMap *fontmap = GetPangoFontMap();
    // In Pango-1.24.4, we can use the "fontmap" property; by setting the
    // property, the PangoFcFont base class manages the pointer (as a weak
    // reference).
    PangoFcFont *fc_font = &parent_instance;
    if (gUseFontMapProperty) {
        g_object_set(this, "fontmap", fontmap, NULL);
    } else {
        // In Pango versions up to 1.20.5, the parent class will decrement
        // the reference count of the fontmap during shutdown() or
        // finalize() of the font.  In Pango versions from 1.22.0 this no
        // longer happens, so we'll end up leaking the (singleton)
        // fontmap.
        fc_font->fontmap = fontmap;
        g_object_ref(fc_font->fontmap);
    }
}

static void
gfx_pango_fc_font_init(gfxPangoFcFont *font)
{
}

static void
gfx_pango_fc_font_finalize(GObject *object)
{
    gfxPangoFcFont *self = GFX_PANGO_FC_FONT(object);

    if (self->mCoverage)
        pango_coverage_unref(self->mCoverage);

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

static PRInt32
GetDPI()
{
#if defined(MOZ_WIDGET_GTK2)
    return gfxPlatformGtk::GetDPI();
#elif defined(MOZ_WIDGET_QT)
    return gfxQtPlatform::GetDPI();
#else
    return 96;
#endif
}

static PangoFontDescription *
gfx_pango_fc_font_describe(PangoFont *font)
{
    gfxPangoFcFont *self = GFX_PANGO_FC_FONT(font);
    PangoFcFont *fcFont = &self->parent_instance;
    PangoFontDescription *result =
        pango_font_description_copy(fcFont->description);

    gfxFcFont *gfxFont = self->GfxFont();
    if (gfxFont) {
        double pixelsize = gfxFont->GetStyle()->size;
        double dpi = GetDPI();
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

    gfxFcFont *gfxFont = self->GfxFont();
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
    gfxFcFont *gfxFont = self->GfxFont();

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

    gfxFcFont *gfxFont = self->GfxFont();
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
    return cairo_ft_scaled_font_lock_face(self->CairoFont());
}

static void
gfx_pango_fc_font_unlock_face(PangoFcFont *font)
{
    gfxPangoFcFont *self = GFX_PANGO_FC_FONT(font);
    cairo_ft_scaled_font_unlock_face(self->CairoFont());
}

static guint
gfx_pango_fc_font_get_glyph(PangoFcFont *font, gunichar wc)
{
    gfxPangoFcFont *self = GFX_PANGO_FC_FONT(font);
    gfxFcFont *gfxFont = self->GfxFont();
    return gfxFont->GetGlyph(wc);
}

typedef int (*PangoVersionFunction)();

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

    // fc_font_class->has_char is inherited
    fc_font_class->lock_face = gfx_pango_fc_font_lock_face;
    fc_font_class->unlock_face = gfx_pango_fc_font_unlock_face;
    fc_font_class->get_glyph = gfx_pango_fc_font_get_glyph;

    // The "fontmap" property on PangoFcFont was introduced for Pango-1.24.0
    // but versions prior to Pango-1.24.4 leaked weak pointers for every font,
    // which would causes crashes when shutting down the FontMap.  For the
    // early Pango-1.24.x versions we're better off setting the fontmap member
    // ourselves, which will not create weak pointers to leak, and instead
    // we'll leak the FontMap on shutdown.  pango_version() and
    // PANGO_VERSION_ENCODE require Pango-1.16.
    PangoVersionFunction pango_version =
        reinterpret_cast<PangoVersionFunction>
        (FindFunctionSymbol("pango_version"));
    gUseFontMapProperty = pango_version && (*pango_version)() >= 12404;
}

/**
 * gfxFcFontSet:
 *
 * Translation from a desired FcPattern to a sorted set of font references
 * (fontconfig cache data) and (when needed) fonts.
 */

class gfxFcFontSet {
public:
    NS_INLINE_DECL_REFCOUNTING(gfxFcFontSet)
    
    explicit gfxFcFontSet(FcPattern *aPattern,
                               gfxUserFontSet *aUserFontSet)
        : mSortPattern(aPattern), mUserFontSet(aUserFontSet),
          mFcFontsTrimmed(0),
          mHaveFallbackFonts(false)
    {
        bool waitForUserFont;
        mFcFontSet = SortPreferredFonts(waitForUserFont);
        mWaitingForUserFont = waitForUserFont;
    }

    // A reference is held by the FontSet.
    // The caller may add a ref to keep the font alive longer than the FontSet.
    gfxFcFont *GetFontAt(PRUint32 i, const gfxFontStyle *aFontStyle)
    {
        if (i >= mFonts.Length() || !mFonts[i].mFont) { 
            // GetFontPatternAt sets up mFonts
            FcPattern *fontPattern = GetFontPatternAt(i);
            if (!fontPattern)
                return NULL;

            mFonts[i].mFont =
                gfxFcFont::GetOrMakeFont(mSortPattern, fontPattern,
                                         aFontStyle);
        }
        return mFonts[i].mFont;
    }

    FcPattern *GetFontPatternAt(PRUint32 i);

    bool WaitingForUserFont() const {
        return mWaitingForUserFont;
    }

private:
    nsReturnRef<FcFontSet> SortPreferredFonts(bool& aWaitForUserFont);
    nsReturnRef<FcFontSet> SortFallbackFonts();

    struct FontEntry {
        explicit FontEntry(FcPattern *aPattern) : mPattern(aPattern) {}
        nsCountedRef<FcPattern> mPattern;
        nsRefPtr<gfxFcFont> mFont;
        nsCountedRef<PangoFont> mPangoFont;
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
        bool Equals(const LangSupportEntry& a, const FcChar8 *b) const
        {
            return FcStrCmpIgnoreCase(a.mLang, b) == 0;
        }
    };

private:
    // The requested pattern
    nsCountedRef<FcPattern> mSortPattern;
    // Fonts from @font-face rules
    nsRefPtr<gfxUserFontSet> mUserFontSet;
    // A (trimmed) list of font patterns and fonts that is built up as
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
    bool mHaveFallbackFonts;
    // True iff there was a user font set with pending downloads,
    // so the set may be updated when downloads complete
    bool mWaitingForUserFont;
};

// Find the FcPattern for an @font-face font suitable for CSS family |aFamily|
// and style |aStyle| properties.
static const nsTArray< nsCountedRef<FcPattern> >*
FindFontPatterns(gfxUserFontSet *mUserFontSet,
                 const nsACString &aFamily, PRUint8 aStyle,
                 PRUint16 aWeight, PRInt16 aStretch,
                 bool& aFoundFamily, bool& aWaitForUserFont)
{
    // Convert to UTF16
    NS_ConvertUTF8toUTF16 utf16Family(aFamily);

    // needsBold is not used here.  Instead synthetic bold is enabled through
    // FcFontRenderPrepare when the weight in the requested pattern is
    // compared against the weight in the font pattern.
    bool needsBold;

    gfxFontStyle style;
    style.style = aStyle;
    style.weight = aWeight;
    style.stretch = aStretch;

    gfxUserFcFontEntry *fontEntry = static_cast<gfxUserFcFontEntry*>
        (mUserFontSet->FindFontEntry(utf16Family, style, aFoundFamily,
                                     needsBold, aWaitForUserFont));

    // Accept synthetic oblique for italic and oblique.
    if (!fontEntry && aStyle != NS_FONT_STYLE_NORMAL) {
        style.style = NS_FONT_STYLE_NORMAL;
        fontEntry = static_cast<gfxUserFcFontEntry*>
            (mUserFontSet->FindFontEntry(utf16Family, style, aFoundFamily,
                                         needsBold, aWaitForUserFont));
    }

    if (!fontEntry)
        return NULL;

    return &fontEntry->GetPatterns();
}

typedef FcBool (*FcPatternRemoveFunction)(FcPattern *p, const char *object,
                                          int id);

// FcPatternRemove is available in fontconfig-2.3.0 (2005)
static FcBool
moz_FcPatternRemove(FcPattern *p, const char *object, int id)
{
    static FcPatternRemoveFunction sFcPatternRemovePtr =
        reinterpret_cast<FcPatternRemoveFunction>
        (FindFunctionSymbol("FcPatternRemove"));

    if (!sFcPatternRemovePtr)
        return FcFalse;

    return (*sFcPatternRemovePtr)(p, object, id);
}

// fontconfig always prefers a matching family to a matching slant, but CSS
// mostly prioritizes slant.  The logic here is from CSS 2.1.
static bool
SlantIsAcceptable(FcPattern *aFont, int aRequestedSlant)
{
    // CSS accepts (possibly synthetic) oblique for italic.
    if (aRequestedSlant == FC_SLANT_ITALIC)
        return true;

    int slant;
    FcResult result = FcPatternGetInteger(aFont, FC_SLANT, 0, &slant);
    // Not having a value would be strange.
    // fontconfig sort and match functions would consider no value a match.
    if (result != FcResultMatch)
        return true;

    switch (aRequestedSlant) {
        case FC_SLANT_ROMAN:
            // CSS requires an exact match
            return slant == aRequestedSlant;
        case FC_SLANT_OBLIQUE:
            // Accept synthetic oblique from Roman,
            // but CSS doesn't accept italic.
            return slant != FC_SLANT_ITALIC;
    }

    return true;
}

// fontconfig prefers a matching family or lang to pixelsize of bitmap
// fonts.  CSS suggests a tolerance of 20% on pixelsize.
static bool
SizeIsAcceptable(FcPattern *aFont, double aRequestedSize)
{
    double size;
    int v = 0;
    while (FcPatternGetDouble(aFont,
                              FC_PIXEL_SIZE, v, &size) == FcResultMatch) {
        ++v;
        if (5.0 * fabs(size - aRequestedSize) < aRequestedSize)
            return true;
    }

    // No size means scalable
    return v == 0;
}

// Sorting only the preferred fonts first usually saves having to sort through
// every font on the system.
nsReturnRef<FcFontSet>
gfxFcFontSet::SortPreferredFonts(bool &aWaitForUserFont)
{
    aWaitForUserFont = false;

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
        bool isUserFont = false;
        if (mUserFontSet) {
            // Have some @font-face definitions

            nsDependentCString cFamily(gfxFontconfigUtils::ToCString(family));
            NS_NAMED_LITERAL_CSTRING(userPrefix, FONT_FACE_FAMILY_PREFIX);

            if (StringBeginsWith(cFamily, userPrefix)) {
                isUserFont = true;

                // Trim off the prefix
                nsDependentCSubstring cssFamily(cFamily, userPrefix.Length());

                PRUint8 thebesStyle =
                    gfxFontconfigUtils::FcSlantToThebesStyle(requestedSlant);
                PRUint16 thebesWeight =
                    gfxFontconfigUtils::GetThebesWeight(mSortPattern);
                PRInt16 thebesStretch =
                    gfxFontconfigUtils::GetThebesStretch(mSortPattern);

                bool foundFamily, waitForUserFont;
                familyFonts = FindFontPatterns(mUserFontSet, cssFamily,
                                               thebesStyle,
                                               thebesWeight, thebesStretch,
                                               foundFamily, waitForUserFont);
                if (waitForUserFont) {
                    aWaitForUserFont = true;
                }
                NS_ASSERTION(foundFamily,
                             "expected to find a user font, but it's missing!");
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

        bool haveLangFont = false;
        for (PRUint32 f = 0; f < langFonts.Length(); ++f) {
            FcPattern *font = langFonts[f];
            if (!SlantIsAcceptable(font, requestedSlant))
                continue;
            if (requestedSize != -1.0 && !SizeIsAcceptable(font, requestedSize))
                continue;

            haveLangFont = true;
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
#ifdef SOLARIS
    // Get around a crash of FcFontSetSort when FcConfig is NULL
    // Solaris's FcFontSetSort needs an FcConfig (bug 474758)
    fontSet.own(FcFontSetSort(FcConfigGetCurrent(), sets, 1, mSortPattern,
                              FcFalse, NULL, &result));
#else
    fontSet.own(FcFontSetSort(NULL, sets, 1, mSortPattern,
                              FcFalse, NULL, &result));
#endif

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
gfxFcFontSet::SortFallbackFonts()
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
gfxFcFontSet::GetFontPatternAt(PRUint32 i)
{
    while (i >= mFonts.Length()) {
        while (!mFcFontSet) {
            if (mHaveFallbackFonts)
                return nsnull;

            mFcFontSet = SortFallbackFonts();
            mHaveFallbackFonts = true;
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
 * gfxPangoFontMap: An implementation of a PangoFontMap.
 *
 * This is a PangoFcFontMap for gfxPangoFcFont.  It will only ever be used if
 * some day pango_cairo_font_map_get_default() does not return a
 * PangoFcFontMap.
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

static PangoFcFont *
gfx_pango_font_map_new_font(PangoFcFontMap *fontmap,
                            FcPattern *pattern)
{
    // new_font is not likely to be used, but the class makes the method
    // available and shapers have access to the class through the font.  Not
    // bothering to make an effort here because this will only ever be used if
    // pango_cairo_font_map_get_default() does not return a PangoFcFontMap and
    // a shaper tried to create a different font from the one it was provided.
    // Only a basic implementation is provided that simply refuses to
    // create a new font.  PangoFcFontMap allows NULL return values here.
    return NULL;
}

static void
gfx_pango_font_map_class_init(gfxPangoFontMapClass *klass)
{
    // inherit GObjectClass::finalize from parent as this class adds no data.

    // inherit PangoFontMap::load_font (which is not likely to be used)
    //   from PangoFcFontMap
    // inherit PangoFontMap::list_families (which is not likely to be used)
    //   from PangoFcFontMap
    // inherit PangoFontMap::load_fontset (which is not likely to be used)
    //   from PangoFcFontMap
    // inherit PangoFontMap::shape_engine_type from PangoFcFontMap

    PangoFcFontMapClass *fcfontmap_class = PANGO_FC_FONT_MAP_CLASS (klass);
    // default_substitute is not required.
    // The API for create_font changed between Pango 1.22 and 1.24 so new_font
    // is provided instead.
    fcfontmap_class->new_font = gfx_pango_font_map_new_font;
    // get_resolution is not required.
    // context_key_* virtual functions are only necessary if we want to
    // dynamically respond to changes in the screen cairo_font_options_t.
}

#ifdef MOZ_WIDGET_GTK2
static void ApplyGdkScreenFontOptions(FcPattern *aPattern);
#endif

// Apply user settings and defaults to pattern in preparation for matching.
static void
PrepareSortPattern(FcPattern *aPattern, double aFallbackSize,
                   double aSizeAdjustFactor, bool aIsPrinterFont)
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
       FcPatternAddBool(aPattern, PRINTING_FC_PROPERTY, FcTrue);
    } else {
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
       cairo_font_options_t *options = cairo_font_options_create();
       cairo_font_options_set_hint_style(options, CAIRO_HINT_STYLE_NONE);
       cairo_ft_font_options_substitute(options, aPattern);
       cairo_font_options_destroy(options);
#endif
#ifdef MOZ_WIDGET_GTK2
       ApplyGdkScreenFontOptions(aPattern);
#endif
    }

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

static bool
FamilyCallback (const nsAString& fontName, const nsACString& genericName,
                bool aUseFontSet, void *closure)
{
    FamilyCallbackData *data = static_cast<FamilyCallbackData*>(closure);
    nsTArray<nsString> *list = data->mFcFamilyList;

    // We ignore prefs that have three hypens since they are X style prefs.
    if (genericName.Length() && FFRECountHyphens(fontName) >= 3)
        return true;

    if (!list->Contains(fontName)) {
        // The family properties of FcPatterns for @font-face fonts have a
        // namespace to identify them among system fonts.  (see
        // FONT_FACE_FAMILY_PREFIX.)
        //
        // Earlier versions of this code allowed the CSS family name to match
        // either the @font-face family or the system font family, so both
        // were added here. This was in accordance with earlier versions of
        // the W3C specifications regarding @font-face.
        //
        // The current (2011-02-27) draft of CSS3 Fonts says
        //
        // (Section 4.2: Font family: the font-family descriptor):
        // "If the font family name is the same as a font family available in
        // a given user's environment, it effectively hides the underlying
        // font for documents that use the stylesheet."
        //
        // (Section 5: Font matching algorithm)
        // "... the user agent attempts to find the family name among fonts
        // defined via @font-face rules and then among available system fonts,
        // .... If a font family defined via @font-face rules contains only
        // invalid font data, it should be considered as if a font was present
        // but contained an empty character map; matching a platform font with
        // the same name must not occur in this case."
        //
        // Therefore, for names present in the user font set, this code no
        // longer includes the family name for matching against system fonts.
        //
        const gfxUserFontSet *userFontSet = data->mUserFontSet;
        if (aUseFontSet && genericName.Length() == 0 &&
            userFontSet && userFontSet->HasFamily(fontName)) {
            nsAutoString userFontName =
                NS_LITERAL_STRING(FONT_FACE_FAMILY_PREFIX) + fontName;
            list->AppendElement(userFontName);
        } else {
            list->AppendElement(fontName);
        }
    }

    return true;
}

gfxPangoFontGroup::gfxPangoFontGroup (const nsAString& families,
                                      const gfxFontStyle *aStyle,
                                      gfxUserFontSet *aUserFontSet)
    : gfxFontGroup(families, aStyle, aUserFontSet),
      mPangoLanguage(GuessPangoLanguage(aStyle->language))
{
    // This language is passed to the font for shaping.
    // Shaping doesn't know about lang groups so make it a real language.
    if (mPangoLanguage) {
        mStyle.language = do_GetAtom(pango_language_to_string(mPangoLanguage));
    }

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
                                 nsIAtom *aLanguage)
{
    FamilyCallbackData data(aFcFamilyList, mUserFontSet);
    // Leave non-existing fonts in the list so that fontconfig can get the
    // best match.
    ForEachFontInternal(mFamilies, aLanguage, true, false, true,
                        FamilyCallback, &data);
}

gfxFcFont *
gfxPangoFontGroup::GetBaseFont()
{
    if (!mFonts[0]) {
        mFonts[0] = GetBaseFontSet()->GetFontAt(0, GetStyle());
    }

    return static_cast<gfxFcFont*>(mFonts[0].get());
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

    return GetBaseFont();
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
    mUnderlineOffset = UNDERLINE_OFFSET_NOT_SET;
    mCurrGeneration = newGeneration;
    mSkipDrawing = false;
}

already_AddRefed<gfxFcFontSet>
gfxPangoFontGroup::MakeFontSet(PangoLanguage *aLang, gfxFloat aSizeAdjustFactor,
                               nsAutoRef<FcPattern> *aMatchPattern)
{
    const char *lang = pango_language_to_string(aLang);

    nsRefPtr <nsIAtom> langGroup;
    if (aLang != mPangoLanguage) {
        // Set up langGroup for Mozilla's font prefs.
        langGroup = do_GetAtom(lang);
    }

    nsAutoTArray<nsString, 20> fcFamilyList;
    GetFcFamilies(&fcFamilyList,
                  langGroup ? langGroup.get() : mStyle.language.get());

    // To consider: A fontset cache here could be helpful.

    // Get a pattern suitable for matching.
    nsAutoRef<FcPattern> pattern
        (gfxFontconfigUtils::NewPattern(fcFamilyList, mStyle, lang));

    PrepareSortPattern(pattern, mStyle.size, aSizeAdjustFactor, mStyle.printerFont);

    nsRefPtr<gfxFcFontSet> fontset =
        new gfxFcFontSet(pattern, mUserFontSet);

    mSkipDrawing = fontset->WaitingForUserFont();

    if (aMatchPattern)
        aMatchPattern->steal(pattern);

    return fontset.forget();
}

gfxPangoFontGroup::
FontSetByLangEntry::FontSetByLangEntry(PangoLanguage *aLang,
                                       gfxFcFontSet *aFontSet)
    : mLang(aLang), mFontSet(aFontSet)
{
}

gfxFcFontSet *
gfxPangoFontGroup::GetFontSet(PangoLanguage *aLang)
{
    GetBaseFontSet(); // sets mSizeAdjustFactor and mFontSets[0]

    if (!aLang)
        return mFontSets[0].mFontSet;

    for (PRUint32 i = 0; i < mFontSets.Length(); ++i) {
        if (mFontSets[i].mLang == aLang)
            return mFontSets[i].mFontSet;
    }

    nsRefPtr<gfxFcFontSet> fontSet =
        MakeFontSet(aLang, mSizeAdjustFactor);
    mFontSets.AppendElement(FontSetByLangEntry(aLang, fontSet));

    return fontSet;
}

already_AddRefed<gfxFont>
gfxPangoFontGroup::FindFontForChar(PRUint32 aCh, PRUint32 aPrevCh,
                                   PRInt32 aRunScript,
                                   gfxFont *aPrevMatchedFont,
                                   PRUint8 *aMatchType)
{
    if (aPrevMatchedFont) {
        // Don't switch fonts for control characters, regardless of
        // whether they are present in the current font, as they won't
        // actually be rendered (see bug 716229)
        PRUint8 category = GetGeneralCategory(aCh);
        if (category == HB_UNICODE_GENERAL_CATEGORY_CONTROL) {
            return nsRefPtr<gfxFont>(aPrevMatchedFont).forget();
        }

        // if this character is a join-control or the previous is a join-causer,
        // use the same font as the previous range if we can
        if (gfxFontUtils::IsJoinControl(aCh) ||
            gfxFontUtils::IsJoinCauser(aPrevCh)) {
            if (aPrevMatchedFont->HasCharacter(aCh)) {
                return nsRefPtr<gfxFont>(aPrevMatchedFont).forget();
            }
        }
    }

    // if this character is a variation selector,
    // use the previous font regardless of whether it supports VS or not.
    // otherwise the text run will be divided.
    if (gfxFontUtils::IsVarSelector(aCh)) {
        if (aPrevMatchedFont) {
            return nsRefPtr<gfxFont>(aPrevMatchedFont).forget();
        }
        // VS alone. it's meaningless to search different fonts
        return nsnull;
    }

    // The real fonts that fontconfig provides for generic/fallback families
    // depend on the language used, so a different FontSet is used for each
    // language (except for the variation below).
    //
    //   With most fontconfig configurations any real family names prior to a
    //   fontconfig generic with corresponding fonts installed will still lead
    //   to the same leading fonts in each FontSet.
    //
    //   There is an inefficiency here therefore because the same base FontSet
    //   could often be used if these real families support the character.
    //   However, with fontconfig aliases, it is difficult to distinguish
    //   where exactly alias fonts end and generic/fallback fonts begin.
    //
    // The variation from pure language-based matching used here is that the
    // same primary/base font is always used irrespective of the language.
    // This provides that SCRIPT_COMMON characters are consistently rendered
    // with the same font (bug 339513 and bug 416725).  This is particularly
    // important with the word cache as script can't be reliably determined
    // from surrounding words.  It also often avoids the unnecessary extra
    // FontSet efficiency mentioned above.
    //
    // However, in two situations, the base font is not checked before the
    // language-specific FontSet.
    //
    //   1. When we don't have a language to make a good choice for
    //      the base font.
    //
    //   2. For system fonts, use the default Pango behavior to give
    //      consistency with other apps.  This is relevant when un-localized
    //      builds are run in non-Latin locales.  This special-case probably
    //      wouldn't be necessary but for bug 91190.

    gfxFcFontSet *fontSet = GetBaseFontSet();
    PRUint32 nextFont = 0;
    FcPattern *basePattern = NULL;
    if (!mStyle.systemFont && mPangoLanguage) {
        basePattern = fontSet->GetFontPatternAt(0);
        if (HasChar(basePattern, aCh)) {
            *aMatchType = gfxTextRange::kFontGroup;
            return nsRefPtr<gfxFont>(GetBaseFont()).forget();
        }

        nextFont = 1;
    }

    // Pango, GLib, and Thebes (but not harfbuzz!) all happen to use the same
    // script codes, so we can just cast the value here.
    const PangoScript script = static_cast<PangoScript>(aRunScript);
    // Might be nice to call pango_language_includes_script only once for the
    // run rather than for each character.
    PangoLanguage *scriptLang;
    if ((!basePattern ||
         !pango_language_includes_script(mPangoLanguage, script)) &&
        (scriptLang = pango_script_get_sample_language(script))) {
        fontSet = GetFontSet(scriptLang);
        nextFont = 0;
    }

    for (PRUint32 i = nextFont;
         FcPattern *pattern = fontSet->GetFontPatternAt(i);
         ++i) {
        if (pattern == basePattern) {
            continue; // already checked basePattern
        }

        if (HasChar(pattern, aCh)) {
            *aMatchType = gfxTextRange::kFontGroup;
            return nsRefPtr<gfxFont>(fontSet->GetFontAt(i, GetStyle())).forget();
        }
    }

    return nsnull;
}

// Sanity-check: spot-check a few constants to confirm that Thebes and
// Pango script codes really do match
PR_STATIC_ASSERT(MOZ_SCRIPT_COMMON    == PANGO_SCRIPT_COMMON);
PR_STATIC_ASSERT(MOZ_SCRIPT_INHERITED == PANGO_SCRIPT_INHERITED);
PR_STATIC_ASSERT(MOZ_SCRIPT_ARABIC    == PANGO_SCRIPT_ARABIC);
PR_STATIC_ASSERT(MOZ_SCRIPT_LATIN     == PANGO_SCRIPT_LATIN);
PR_STATIC_ASSERT(MOZ_SCRIPT_UNKNOWN   == PANGO_SCRIPT_UNKNOWN);
PR_STATIC_ASSERT(MOZ_SCRIPT_NKO       == PANGO_SCRIPT_NKO);

/**
 ** gfxFcFont
 **/

cairo_user_data_key_t gfxFcFont::sGfxFontKey;

gfxFcFont::gfxFcFont(cairo_scaled_font_t *aCairoFont,
                     gfxFcFontEntry *aFontEntry,
                     const gfxFontStyle *aFontStyle)
    : gfxFT2FontBase(aCairoFont, aFontEntry, aFontStyle),
      mPangoFont()
{
    cairo_scaled_font_set_user_data(mScaledFont, &sGfxFontKey, this, NULL);
}

// The gfxFcFont keeps (only) a toggle_ref on mPangoFont.
// While mPangoFont has other references, a reference to the
// gfxFcFont is held.  While mPangoFont has no other references, the reference
// to the gfxFcFont is removed.
static void
PangoFontToggleNotify(gpointer data, GObject* object, gboolean is_last_ref)
{
    gfxFcFont *font = static_cast<gfxFcFont*>(data);
    if (is_last_ref) { // gfxFcFont has last ref to PangoFont
        NS_RELEASE(font);
    } else {
        NS_ADDREF(font);
    }
}

void
gfxFcFont::MakePangoFont()
{
    // Switch from a normal reference to a toggle_ref.
    gfxFcFontEntry *fe = static_cast<gfxFcFontEntry*>(mFontEntry.get());
    nsAutoRef<PangoFont> pangoFont
        (gfxPangoFcFont::NewFont(this, fe->GetPatterns()[0]));
    mPangoFont = pangoFont;
    g_object_add_toggle_ref(G_OBJECT(mPangoFont), PangoFontToggleNotify, this);
    // This self-reference gets removed when the normal reference to the
    // PangoFont is removed as the nsAutoRef goes out of scope.
    NS_ADDREF(this);
}

gfxFcFont::~gfxFcFont()
{
    cairo_scaled_font_set_user_data(mScaledFont, &sGfxFontKey, NULL, NULL);
    if (mPangoFont) {
        g_object_remove_toggle_ref(G_OBJECT(mPangoFont),
                                   PangoFontToggleNotify, this);
    }
}

bool
gfxFcFont::ShapeWord(gfxContext *aContext,
                     gfxShapedWord *aShapedWord,
                     const PRUnichar *aString,
                     bool aPreferPlatformShaping)
{
    gfxFcFontEntry *fontEntry = static_cast<gfxFcFontEntry*>(GetFontEntry());

#ifdef MOZ_GRAPHITE
    if (FontCanSupportGraphite()) {
        if (gfxPlatform::GetPlatform()->UseGraphiteShaping()) {
            if (!mGraphiteShaper) {
                mGraphiteShaper = new gfxGraphiteShaper(this);
            }
            if (mGraphiteShaper->ShapeWord(aContext, aShapedWord, aString)) {
                return true;
            }
        }
    }
#endif

    if (fontEntry->ShouldUseHarfBuzz(aShapedWord->Script())) {
        if (!mHarfBuzzShaper) {
            gfxFT2LockedFace face(this);
            mHarfBuzzShaper = new gfxHarfBuzzShaper(this);
            // Used by gfxHarfBuzzShaper, currently only for kerning
            mFUnitsConvFactor = face.XScale();
        }
        if (mHarfBuzzShaper->ShapeWord(aContext, aShapedWord, aString)) {
            return true;
        }

        // Wrong font type for HarfBuzz
        fontEntry->SkipHarfBuzz();
        mHarfBuzzShaper = nsnull;
    }

    bool ok = InitGlyphRunWithPango(aShapedWord, aString);

    NS_WARN_IF_FALSE(ok, "shaper failed, expect scrambled or missing text");
    return ok;
}

/* static */ void
gfxPangoFontGroup::Shutdown()
{
    if (gPangoFontMap) {
        g_object_unref(gPangoFontMap);
        gPangoFontMap = NULL;
    }

    // Resetting gFTLibrary in case this is wanted again after a
    // cairo_debug_reset_static_data.
    gFTLibrary = NULL;
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

/* static */ FT_Library
gfxPangoFontGroup::GetFTLibrary()
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

        gfxFcFont *font = fontGroup->GetBaseFont();
        if (!font)
            return NULL;

        gfxFT2LockedFace face(font);
        if (!face.get())
            return NULL;

        gFTLibrary = face.get()->glyph->library;
    }

    return gFTLibrary;
}

/* static */ gfxFontEntry *
gfxPangoFontGroup::NewFontEntry(const gfxProxyFontEntry &aProxyEntry,
                                const PRUint8 *aFontData, PRUint32 aLength)
{
    // Ownership of aFontData is passed in here, and transferred to the
    // new fontEntry, which will release it when no longer needed.

    // Using face_index = 0 for the first face in the font, as we have no
    // other information.  FT_New_Memory_Face checks for a NULL FT_Library.
    FT_Face face;
    FT_Error error =
        FT_New_Memory_Face(GetFTLibrary(), aFontData, aLength, 0, &face);
    if (error != 0) {
        NS_Free((void*)aFontData);
        return nsnull;
    }

    return new gfxDownloadedFcFontEntry(aProxyEntry, aFontData, face);
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
 * The following gfxFcFonts are accessed from the cairo_scaled_font or created
 * from the FcPattern, not from the gfxFontCache hash table.  The gfxFontCache
 * hash table is keyed by desired family and style, whereas here we only know
 * actual family and style.  There may be more than one of these fonts with
 * the same family and style, but different PangoFont and actual font face.
 * 
 * The point of this is to record the exact font face for gfxTextRun glyph
 * indices.  The style of this font does not necessarily represent the exact
 * gfxFontStyle used to build the text run.  Notably, the language is not
 * recorded.
 */

/* static */
already_AddRefed<gfxFcFont>
gfxFcFont::GetOrMakeFont(FcPattern *aRequestedPattern, FcPattern *aFontPattern,
                         const gfxFontStyle *aFontStyle)
{
    nsAutoRef<FcPattern> renderPattern
        (FcFontRenderPrepare(NULL, aRequestedPattern, aFontPattern));
    cairo_font_face_t *face =
        cairo_ft_font_face_create_for_pattern(renderPattern);

    // Reuse an existing font entry if available.
    nsRefPtr<gfxFcFontEntry> fe = gfxFcFontEntry::LookupFontEntry(face);
    if (!fe) {
        gfxDownloadedFcFontEntry *downloadedFontEntry =
            GetDownloadedFontEntry(aFontPattern);
        if (downloadedFontEntry) {
            // Web font
            fe = downloadedFontEntry;
            if (cairo_font_face_status(face) == CAIRO_STATUS_SUCCESS) {
                // cairo_font_face_t is using the web font data.
                // Hold a reference to the font entry to keep the font face
                // data.
                if (!downloadedFontEntry->SetCairoFace(face)) {
                    // OOM.  Let cairo pick a fallback font
                    cairo_font_face_destroy(face);
                    face = cairo_ft_font_face_create_for_pattern(aRequestedPattern);
                    fe = gfxFcFontEntry::LookupFontEntry(face);
                }
            }
        }
        if (!fe) {
            // Get a unique name for the font face from the file and id.
            nsAutoString name;
            FcChar8 *fc_file;
            if (FcPatternGetString(renderPattern,
                                   FC_FILE, 0, &fc_file) == FcResultMatch) {
                int index;
                if (FcPatternGetInteger(renderPattern,
                                        FC_INDEX, 0, &index) != FcResultMatch) {
                    // cairo defaults to 0.
                    index = 0;
                }

                AppendUTF8toUTF16(gfxFontconfigUtils::ToCString(fc_file), name);
                if (index != 0) {
                    name.AppendLiteral("/");
                    name.AppendInt(index);
                }
            }

            fe = new gfxSystemFcFontEntry(face, aFontPattern, name);
        }
    }

    gfxFontStyle style(*aFontStyle);
    style.size = GetPixelSize(renderPattern);
    style.style = gfxFontconfigUtils::GetThebesStyle(renderPattern);
    style.weight = gfxFontconfigUtils::GetThebesWeight(renderPattern);

    nsRefPtr<gfxFont> font = gfxFontCache::GetCache()->Lookup(fe, &style);
    if (!font) {
        // Note that a file/index pair (or FT_Face) and the gfxFontStyle are
        // not necessarily enough to provide a key that will describe a unique
        // font.  cairoFont contains information from renderPattern, which is a
        // fully resolved pattern from FcFontRenderPrepare.
        // FcFontRenderPrepare takes the requested pattern and the face
        // pattern as input and can modify elements of the resulting pattern
        // that affect rendering but are not included in the gfxFontStyle.
        cairo_scaled_font_t *cairoFont = CreateScaledFont(renderPattern, face);
        font = new gfxFcFont(cairoFont, fe, &style);
        gfxFontCache::GetCache()->AddNew(font);
        cairo_scaled_font_destroy(cairoFont);
    }

    cairo_font_face_destroy(face);

    nsRefPtr<gfxFcFont> retval(static_cast<gfxFcFont*>(font.get()));
    return retval.forget();
}

static PangoFontMap *
GetPangoFontMap()
{
    if (!gPangoFontMap) {
        // This is the same FontMap used by GDK, so that the same
        // PangoCoverage cache is shared.
        gPangoFontMap = pango_cairo_font_map_get_default();

        if (PANGO_IS_FC_FONT_MAP(gPangoFontMap)) {
            g_object_ref(gPangoFontMap);
        } else {
            // Future proofing: We need a PangoFcFontMap for gfxPangoFcFont.
            // pango_cairo_font_map_get_default() is expected to return a
            // PangoFcFontMap on Linux systems, but, just in case this ever
            // changes, we provide our own basic implementation.
            gPangoFontMap = gfxPangoFontMap::NewFontMap();
        }
    }
    return gPangoFontMap;
}

gfxFcFontSet *
gfxPangoFontGroup::GetBaseFontSet()
{
    if (mFontSets.Length() > 0)
        return mFontSets[0].mFontSet;

    mSizeAdjustFactor = 1.0; // will be adjusted below if necessary
    nsAutoRef<FcPattern> pattern;
    nsRefPtr<gfxFcFontSet> fontSet =
        MakeFontSet(mPangoLanguage, mSizeAdjustFactor, &pattern);

    double size = GetPixelSize(pattern);
    if (size != 0.0 && mStyle.sizeAdjust != 0.0) {
        gfxFcFont *font = fontSet->GetFontAt(0, GetStyle());
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

                fontSet = new gfxFcFontSet(pattern, mUserFontSet);
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

/**
 ** gfxTextRun
 * 
 * A serious problem:
 *
 * -- We draw with a font that's hinted for the CTM, but we measure with a font
 * hinted to the identity matrix, so our "bounding metrics" may not be accurate.
 * 
 **/

// This will fetch an existing scaled_font if one exists.
static cairo_scaled_font_t *
CreateScaledFont(FcPattern *aPattern, cairo_font_face_t *aFace)
{
    double size = GetPixelSize(aPattern);
        
    cairo_matrix_t fontMatrix;
    FcMatrix *fcMatrix;
    if (FcPatternGetMatrix(aPattern, FC_MATRIX, 0, &fcMatrix) == FcResultMatch)
        cairo_matrix_init(&fontMatrix, fcMatrix->xx, -fcMatrix->yx, -fcMatrix->xy, fcMatrix->yy, 0, 0);
    else
        cairo_matrix_init_identity(&fontMatrix);
    cairo_matrix_scale(&fontMatrix, size, size);

    FcBool printing;
    if (FcPatternGetBool(aPattern, PRINTING_FC_PROPERTY, 0, &printing) != FcResultMatch) {
        printing = FcFalse;
    }

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
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
    cairo_font_options_set_hint_metrics(fontOptions, CAIRO_HINT_METRICS_OFF);
#else
    if (printing) {
        cairo_font_options_set_hint_metrics(fontOptions, CAIRO_HINT_METRICS_OFF);
    } else {
        cairo_font_options_set_hint_metrics(fontOptions, CAIRO_HINT_METRICS_ON);
    }
#endif

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
    FcBool hinting = FcFalse;
#ifndef MOZ_GFX_OPTIMIZE_MOBILE
    if (FcPatternGetBool(aPattern, FC_HINTING, 0, &hinting) != FcResultMatch) {
        hinting = FcTrue;
    }
#endif
    cairo_hint_style_t hint_style;
    if (printing || !hinting) {
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
        cairo_scaled_font_create(aFace, &fontMatrix, &identityMatrix,
                                 fontOptions);

    cairo_font_options_destroy(fontOptions);

    NS_ASSERTION(cairo_scaled_font_status(scaledFont) == CAIRO_STATUS_SUCCESS,
                 "Failed to create scaled font");
    return scaledFont;
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
                           gfxShapedWord *aShapedWord,
                           const gchar *aUTF8, PRUint32 aUTF8Length,
                           PRUint32 *aUTF16Offset,
                           PangoGlyphUnit aOverrideSpaceWidth)
{
    PRUint32 utf16Offset = *aUTF16Offset;
    PRUint32 wordLength = aShapedWord->Length();
    const PRUint32 appUnitsPerDevUnit = aShapedWord->AppUnitsPerDevUnit();

    // Override the width of a space, but only for spaces that aren't
    // clustered with something else (like a freestanding diacritical mark)
    PangoGlyphUnit width = aGlyphs[0].geometry.width;
    if (aOverrideSpaceWidth && aUTF8[0] == ' ' &&
        (utf16Offset + 1 == wordLength ||
         aShapedWord->IsClusterStart(utf16Offset))) {
        width = aOverrideSpaceWidth;
    }
    PRInt32 advance = ConvertPangoToAppUnits(width, appUnitsPerDevUnit);

    gfxShapedWord::CompressedGlyph g;
    bool atClusterStart = aShapedWord->IsClusterStart(utf16Offset);
    // See if we fit in the compressed area.
    if (aGlyphCount == 1 && advance >= 0 && atClusterStart &&
        aGlyphs[0].geometry.x_offset == 0 &&
        aGlyphs[0].geometry.y_offset == 0 &&
        !IS_EMPTY_GLYPH(aGlyphs[0].glyph) &&
        gfxShapedWord::CompressedGlyph::IsSimpleAdvance(advance) &&
        gfxShapedWord::CompressedGlyph::IsSimpleGlyphID(aGlyphs[0].glyph)) {
        aShapedWord->SetSimpleGlyph(utf16Offset,
                                    g.SetSimpleGlyph(advance, aGlyphs[0].glyph));
    } else {
        nsAutoTArray<gfxShapedWord::DetailedGlyph,10> detailedGlyphs;
        if (!detailedGlyphs.AppendElements(aGlyphCount))
            return NS_ERROR_OUT_OF_MEMORY;

        PRInt32 direction = aShapedWord->IsRightToLeft() ? -1 : 1;
        PRUint32 pangoIndex = direction > 0 ? 0 : aGlyphCount - 1;
        PRUint32 detailedIndex = 0;
        for (PRUint32 i = 0; i < aGlyphCount; ++i) {
            const PangoGlyphInfo &glyph = aGlyphs[pangoIndex];
            pangoIndex += direction;
            // The zero width characters return empty glyph ID at
            // shaping; we should skip these.
            if (IS_EMPTY_GLYPH(glyph.glyph))
                continue;

            gfxShapedWord::DetailedGlyph *details = &detailedGlyphs[detailedIndex];
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
        g.SetComplex(atClusterStart, true, detailedIndex);
        aShapedWord->SetGlyphs(utf16Offset, g, detailedGlyphs.Elements());
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

        if (utf16Offset >= wordLength) {
            NS_ERROR("Someone has added too many glyphs!");
            return NS_ERROR_FAILURE;
        }

        g.SetComplex(aShapedWord->IsClusterStart(utf16Offset), false, 0);
        aShapedWord->SetGlyphs(utf16Offset, g, nsnull);
    }
    *aUTF16Offset = utf16Offset;
    return NS_OK;
}

static nsresult
SetGlyphs(gfxShapedWord *aShapedWord, const gchar *aUTF8, PRUint32 aUTF8Length,
          PRUint32 *aUTF16Offset, PangoGlyphString *aGlyphs,
          PangoGlyphUnit aOverrideSpaceWidth,
          gfxFont *aFont)
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
    PRUint32 wordLength = aShapedWord->Length();
    utf8Index = 0;
    // The next glyph cluster in logical order. 
    gint nextGlyphClusterStart = logGlyphs[utf8Index];
    NS_ASSERTION(nextGlyphClusterStart >= 0, "No glyphs! - NUL in string?");
    while (utf8Index < aUTF8Length) {
        if (utf16Offset >= wordLength) {
          NS_ERROR("Someone has added too many glyphs!");
          return NS_ERROR_FAILURE;
        }
        gint glyphClusterStart = nextGlyphClusterStart;
        // Find the utf8 text associated with this glyph cluster.
        PRUint32 clusterUTF8Start = utf8Index;
        // Check whether we are consistent with pango_break data.
        NS_WARN_IF_FALSE(aShapedWord->IsClusterStart(utf16Offset),
                         "Glyph cluster not aligned on character cluster.");
        do {
            ++utf8Index;
            nextGlyphClusterStart = logGlyphs[utf8Index];
        } while (nextGlyphClusterStart < 0);
        const gchar *clusterUTF8 = &aUTF8[clusterUTF8Start];
        PRUint32 clusterUTF8Length = utf8Index - clusterUTF8Start;

        bool haveMissingGlyph = false;
        gint glyphIndex = glyphClusterStart;

        // It's now unncecessary to do NUL handling here.
        do {
            if (IS_MISSING_GLYPH(glyphs[glyphIndex].glyph)) {
                // Does pango ever provide more than one glyph in the
                // cluster if there is a missing glyph?
                // behdad: yes
                haveMissingGlyph = true;
            }
            glyphIndex++;
        } while (glyphIndex < numGlyphs && 
                 logClusters[glyphIndex] == gint(clusterUTF8Start));

        nsresult rv;
        if (haveMissingGlyph) {
            SetMissingGlyphs(aShapedWord, clusterUTF8, clusterUTF8Length,
                             &utf16Offset, aFont);
        } else {
            rv = SetGlyphsForCharacterGroup(&glyphs[glyphClusterStart],
                                            glyphIndex - glyphClusterStart,
                                            aShapedWord,
                                            clusterUTF8, clusterUTF8Length,
                                            &utf16Offset, aOverrideSpaceWidth);
            NS_ENSURE_SUCCESS(rv,rv);
        }
    }
    *aUTF16Offset = utf16Offset;
    return NS_OK;
}

static void
SetMissingGlyphs(gfxShapedWord *aShapedWord, const gchar *aUTF8,
                 PRUint32 aUTF8Length, PRUint32 *aUTF16Offset,
                 gfxFont *aFont)
{
    PRUint32 utf16Offset = *aUTF16Offset;
    PRUint32 wordLength = aShapedWord->Length();
    for (PRUint32 index = 0; index < aUTF8Length;) {
        if (utf16Offset >= wordLength) {
            NS_ERROR("Someone has added too many glyphs!");
            break;
        }
        gunichar ch = g_utf8_get_char(aUTF8 + index);
        aShapedWord->SetMissingGlyph(utf16Offset, ch, aFont);

        ++utf16Offset;
        NS_ASSERTION(!IS_SURROGATE(ch), "surrogates should not appear in UTF8");
        if (ch >= 0x10000)
            ++utf16Offset;
        // We produced this UTF8 so we don't need to worry about malformed stuff
        index = g_utf8_next_char(aUTF8 + index) - aUTF8;
    }

    *aUTF16Offset = utf16Offset;
}

static void
InitGlyphRunWithPangoAnalysis(gfxShapedWord *aShapedWord,
                              const gchar *aUTF8, PRUint32 aUTF8Length,
                              PangoAnalysis *aAnalysis,
                              PangoGlyphUnit aOverrideSpaceWidth,
                              gfxFont *aFont)
{
    PRUint32 utf16Offset = 0;
    PangoGlyphString *glyphString = pango_glyph_string_new();

    const gchar *p = aUTF8;
    const gchar *end = p + aUTF8Length;
    while (p < end) {
        if (*p == 0) {
            aShapedWord->SetMissingGlyph(utf16Offset, 0, aFont);
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

        pango_shape(text, len, aAnalysis, glyphString);
        SetGlyphs(aShapedWord, text, len, &utf16Offset, glyphString,
                  aOverrideSpaceWidth, aFont);
    }

    pango_glyph_string_free(glyphString);
}

// PangoAnalysis is part of Pango's ABI but over time extra fields have been
// inserted into padding.  This union is used so that the code here can be
// compiled against older Pango versions but run against newer versions.
typedef union {
    PangoAnalysis pango;
    // This struct matches PangoAnalysis from Pango version
    // 1.16.5 to 1.28.1 (at least).
    struct {
        PangoEngineShape *shape_engine;
        PangoEngineLang  *lang_engine;
        PangoFont *font;
        guint8 level;
        guint8 gravity; /* PangoGravity */
        guint8 flags;
        guint8 script; /* PangoScript */
        PangoLanguage *language;
        GSList *extra_attrs;
    } local;
} PangoAnalysisUnion;

bool
gfxFcFont::InitGlyphRunWithPango(gfxShapedWord *aShapedWord,
                                 const PRUnichar *aString)
{
    const PangoScript script = static_cast<PangoScript>(aShapedWord->Script());
    NS_ConvertUTF16toUTF8 utf8(aString, aShapedWord->Length());

    PangoFont *font = GetPangoFont();

    hb_language_t languageOverride = NULL;
    if (GetStyle()->languageOverride) {
        languageOverride =
            hb_ot_tag_to_language(GetStyle()->languageOverride);
    } else if (GetFontEntry()->mLanguageOverride) {
        languageOverride =
            hb_ot_tag_to_language(GetFontEntry()->mLanguageOverride);
    }

    PangoLanguage *language;
    if (languageOverride) {
        language =
            pango_language_from_string(hb_language_to_string(languageOverride));
    } else {
#if 0 // FIXME ??
        language = fontGroup->GetPangoLanguage();
#endif
        // FIXME: should probably cache this in the gfxFcFont
        language = GuessPangoLanguage(GetStyle()->language);

        // The language that we have here is often not as good an indicator for
        // the run as the script.  This is not so important for the PangoMaps
        // here as all the default Pango shape and lang engines are selected
        // by script only (not language) anyway, but may be important in the
        // PangoAnalysis as the shaper sometimes accesses language-specific
        // tables.
        PangoLanguage *scriptLang;
        if ((!language ||
             !pango_language_includes_script(language, script)) &&
            (scriptLang = pango_script_get_sample_language(script))) {
            language = scriptLang;
        }
    }

    static GQuark engineLangId =
        g_quark_from_static_string(PANGO_ENGINE_TYPE_LANG);
    static GQuark renderNoneId =
        g_quark_from_static_string(PANGO_RENDER_TYPE_NONE);
    PangoMap *langMap = pango_find_map(language, engineLangId, renderNoneId);

    static GQuark engineShapeId =
        g_quark_from_static_string(PANGO_ENGINE_TYPE_SHAPE);
    static GQuark renderFcId =
        g_quark_from_static_string(PANGO_RENDER_TYPE_FC);
    PangoMap *shapeMap = pango_find_map(language, engineShapeId, renderFcId);
    if (!shapeMap) {
        return false;
    }

    // The preferred shape engine for language and script
    PangoEngineShape *shapeEngine =
        PANGO_ENGINE_SHAPE(pango_map_get_engine(shapeMap, script));
    if (!shapeEngine) {
        return false;
    }

    PangoEngineShapeClass *shapeClass = static_cast<PangoEngineShapeClass*>
        (g_type_class_peek(PANGO_TYPE_ENGINE_SHAPE));

    // The |covers| method in the PangoEngineShape base class, which is the
    // method used by Pango shapers, merely copies the fontconfig coverage map
    // to a PangoCoverage and checks that the character is supported.  We've
    // already checked for character support, so we can avoid this copy for
    // these shapers.
    //
    // With SIL Graphite shapers, however, |covers| also checks that the font
    // is a Graphite font.  (bug 397860)
    if (!shapeClass ||
        PANGO_ENGINE_SHAPE_GET_CLASS(shapeEngine)->covers != shapeClass->covers)
    {
        GSList *exact_engines;
        GSList *fallback_engines;
        pango_map_get_engines(shapeMap, script,
                              &exact_engines, &fallback_engines);

        GSList *engines = g_slist_concat(exact_engines, fallback_engines);
        for (GSList *link = engines; link; link = link->next) {
            PangoEngineShape *engine = PANGO_ENGINE_SHAPE(link->data);
            PangoCoverageLevel (*covers)(PangoEngineShape*, PangoFont*,
                                         PangoLanguage*, gunichar) =
                PANGO_ENGINE_SHAPE_GET_CLASS(shapeEngine)->covers;

            if ((shapeClass && covers == shapeClass->covers) ||
                covers(engine, font, language, ' ') != PANGO_COVERAGE_NONE)
            {
                shapeEngine = engine;
                break;
            }
        }
        g_slist_free(engines); // Frees exact and fallback links
    }

    PangoAnalysisUnion analysis;
    memset(&analysis, 0, sizeof(analysis));

    // For pango_shape
    analysis.local.shape_engine = shapeEngine;
    // For pango_break
    analysis.local.lang_engine =
        PANGO_ENGINE_LANG(pango_map_get_engine(langMap, script));

    analysis.local.font = font;
    analysis.local.level = aShapedWord->IsRightToLeft() ? 1 : 0;
    // gravity and flags are used in Pango 1.14.10 and newer.
    //
    // PANGO_GRAVITY_SOUTH is what we want for upright horizontal text.  The
    // constant is not available when compiling with older Pango versions, but
    // is zero so the zero memset initialization is sufficient.
    //
    // Pango uses non-zero flags for vertical gravities only
    // (up to version 1.28 at least), so using zero is fine for flags too.
#if 0
    analysis.local.gravity = PANGO_GRAVITY_SOUTH;
    analysis.local.flags = 0;
#endif
    // Only used in Pango 1.16.5 and newer.
    analysis.local.script = script;

    analysis.local.language = language;
    // Non-font attributes.  Not used here.
    analysis.local.extra_attrs = NULL;

    PangoGlyphUnit spaceWidth =
        moz_pango_units_from_double(GetMetrics().spaceWidth);

    InitGlyphRunWithPangoAnalysis(aShapedWord, utf8.get(), utf8.Length(),
                                  &analysis.pango, spaceWidth, this);
    return true;
}

/* static */
PangoLanguage *
GuessPangoLanguage(nsIAtom *aLanguage)
{
    if (!aLanguage)
        return NULL;

    // Pango and fontconfig won't understand mozilla's internal langGroups, so
    // find a real language.
    nsCAutoString lang;
    gfxFontconfigUtils::GetSampleLangForGroup(aLanguage, &lang);
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

/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prlink.h"
#include "gfxTypes.h"

#include "nsTArray.h"

#include "gfxContext.h"
#ifdef MOZ_WIDGET_GTK
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
#include "nsUnicodeProperties.h"
#include "nsUnicodeScriptCodes.h"
#include "gfxFontconfigUtils.h"
#include "gfxUserFontSet.h"
#include "gfxFontConstants.h"

#include <cairo.h>
#include <cairo-ft.h>

#include <fontconfig/fcfreetype.h>
#include <pango/pango.h>

#include FT_TRUETYPE_TABLES_H

#ifdef MOZ_WIDGET_GTK
#include <gdk/gdk.h>
#endif

#include <math.h>

using namespace mozilla;
using namespace mozilla::unicode;

#define PRINTING_FC_PROPERTY "gfx.printing"

static PangoLanguage *GuessPangoLanguage(nsIAtom *aLanguage);

static cairo_scaled_font_t *
CreateScaledFont(FcPattern *aPattern, cairo_font_face_t *aFace);

static FT_Library gFTLibrary;

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
    PRLibrary *lib = nullptr;
    PRFuncPtr result = PR_FindFunctionSymbolAndLibrary(name, &lib);
    if (lib) {
        PR_UnloadLibrary(lib);
    }

    return result;
}

static bool HasChar(FcPattern *aFont, FcChar32 wc)
{
    FcCharSet *charset = nullptr;
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

    static gfxFcFontEntry *LookupFontEntry(cairo_font_face_t *aFace)
    {
        return static_cast<gfxFcFontEntry*>
            (cairo_font_face_get_user_data(aFace, &sFontEntryKey));
    }

    // override the gfxFontEntry impl to read the name from fontconfig
    // instead of trying to get the 'name' table, as we don't implement
    // GetFontTable() here
    virtual nsString RealFaceName();

    // This is needed to make gfxFontEntry::HasCharacter(aCh) work.
    virtual bool TestCharacterMap(uint32_t aCh)
    {
        for (uint32_t i = 0; i < mPatterns.Length(); ++i) {
            if (HasChar(mPatterns[i], aCh)) {
                return true;
            }
        }
        return false;
    }

protected:
    explicit gfxFcFontEntry(const nsAString& aName)
        : gfxFontEntry(aName)
    {
    }

    // One pattern is the common case and some subclasses rely on successful
    // addition of the first element to the array.
    AutoFallibleTArray<nsCountedRef<FcPattern>,1> mPatterns;

    static cairo_user_data_key_t sFontEntryKey;
};

cairo_user_data_key_t gfxFcFontEntry::sFontEntryKey;

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
                result.Append(' ');
                AppendUTF8toUTF16((const char*)name, result);
            }
            return result;
        }
    }
    // fall back to gfxFontEntry implementation (only works for sfnt fonts)
    return gfxFontEntry::RealFaceName();
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
        : gfxFcFontEntry(aName), mFontFace(aFontFace),
          mFTFace(nullptr), mFTFaceInitialized(false)
    {
        cairo_font_face_reference(mFontFace);
        cairo_font_face_set_user_data(mFontFace, &sFontEntryKey, this, nullptr);
        mPatterns.AppendElement();
        // mPatterns is an nsAutoTArray with 1 space always available, so the
        // AppendElement always succeeds.
        mPatterns[0] = aFontPattern;

        FcChar8 *name;
        if (FcPatternGetString(aFontPattern,
                               FC_FAMILY, 0, &name) == FcResultMatch) {
            mFamilyName = NS_ConvertUTF8toUTF16((const char*)name);
        }
    }

    ~gfxSystemFcFontEntry()
    {
        cairo_font_face_set_user_data(mFontFace,
                                      &sFontEntryKey,
                                      nullptr,
                                      nullptr);
        cairo_font_face_destroy(mFontFace);
    }

    virtual void ForgetHBFace() override;
    virtual void ReleaseGrFace(gr_face* aFace) override;

protected:
    virtual nsresult
    CopyFontTable(uint32_t aTableTag, FallibleTArray<uint8_t>& aBuffer) override;

    void MaybeReleaseFTFace();

private:
    cairo_font_face_t *mFontFace;
    FT_Face            mFTFace;
    bool               mFTFaceInitialized;
};

nsresult
gfxSystemFcFontEntry::CopyFontTable(uint32_t aTableTag,
                                    FallibleTArray<uint8_t>& aBuffer)
{
    if (!mFTFaceInitialized) {
        mFTFaceInitialized = true;
        FcChar8 *filename;
        if (FcPatternGetString(mPatterns[0], FC_FILE, 0, &filename) != FcResultMatch) {
            return NS_ERROR_FAILURE;
        }
        int index;
        if (FcPatternGetInteger(mPatterns[0], FC_INDEX, 0, &index) != FcResultMatch) {
            index = 0; // default to 0 if not found in pattern
        }
        if (FT_New_Face(gfxPangoFontGroup::GetFTLibrary(),
                        (const char*)filename, index, &mFTFace) != 0) {
            return NS_ERROR_FAILURE;
        }
    }

    if (!mFTFace) {
        return NS_ERROR_NOT_AVAILABLE;
    }

    FT_ULong length = 0;
    if (FT_Load_Sfnt_Table(mFTFace, aTableTag, 0, nullptr, &length) != 0) {
        return NS_ERROR_NOT_AVAILABLE;
    }
    if (!aBuffer.SetLength(length)) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    if (FT_Load_Sfnt_Table(mFTFace, aTableTag, 0, aBuffer.Elements(), &length) != 0) {
        aBuffer.Clear();
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

void
gfxSystemFcFontEntry::MaybeReleaseFTFace()
{
    // don't release if either HB or Gr face still exists
    if (mHBFace || mGrFace) {
        return;
    }
    if (mFTFace) {
        FT_Done_Face(mFTFace);
        mFTFace = nullptr;
    }
    mFTFaceInitialized = false;
}

void
gfxSystemFcFontEntry::ForgetHBFace()
{
    gfxFontEntry::ForgetHBFace();
    MaybeReleaseFTFace();
}

void
gfxSystemFcFontEntry::ReleaseGrFace(gr_face* aFace)
{
    gfxFontEntry::ReleaseGrFace(aFace);
    MaybeReleaseFTFace();
}

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
    explicit gfxUserFcFontEntry(const nsAString& aFontName,
                       uint16_t aWeight,
                       int16_t aStretch,
                       bool aItalic)
        : gfxFcFontEntry(aFontName)
    {
        mItalic = aItalic;
        mWeight = aWeight;
        mStretch = aStretch;
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
        nsAutoCString fullname;
        if (gfxFontconfigUtils::GetFullnameFromFamilyAndStyle(aPattern,
                                                              &fullname)) {
            FcPatternAddString(aPattern, FC_FULLNAME,
                               gfxFontconfigUtils::ToFcChar8(fullname));
        }
    }

    nsAutoCString family;
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
    gfxLocalFcFontEntry(const nsAString& aFontName,
                        uint16_t aWeight,
                        int16_t aStretch,
                        bool aItalic,
                        const nsTArray< nsCountedRef<FcPattern> >& aPatterns)
        : gfxUserFcFontEntry(aFontName, aWeight, aStretch, aItalic)
    {
        if (!mPatterns.SetCapacity(aPatterns.Length()))
            return; // OOM

        for (uint32_t i = 0; i < aPatterns.Length(); ++i) {
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
    gfxDownloadedFcFontEntry(const nsAString& aFontName,
                             uint16_t aWeight,
                             int16_t aStretch,
                             bool aItalic,
                             const uint8_t *aData, FT_Face aFace)
        : gfxUserFcFontEntry(aFontName, aWeight, aStretch, aItalic),
          mFontData(aData), mFace(aFace)
    {
        NS_PRECONDITION(aFace != nullptr, "aFace is NULL!");
        mIsDataUserFont = true;
        InitPattern();
    }

    virtual ~gfxDownloadedFcFontEntry();

    // Returns true on success
    bool SetCairoFace(cairo_font_face_t *aFace);

    virtual hb_blob_t* GetFontTable(uint32_t aTableTag) override;

protected:
    void InitPattern();

    // mFontData holds the data used to instantiate the FT_Face;
    // this has to persist until we are finished with the face,
    // then be released with free().
    const uint8_t* mFontData;

    FT_Face mFace;
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
        return nullptr;

    if (value.type != FcTypeFTFace) {
        NS_NOTREACHED("Wrong type for -moz-font-entry font property");
        return nullptr;
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
    free((void*)mFontData);
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
        // The "file" argument cannot be nullptr (in fontconfig-2.6.0 at
        // least). The dummy file passed here is removed below.
        //
        // When fontconfig scans the system fonts, FcConfigGetBlanks(nullptr)
        // is passed as the "blanks" argument, which provides that unexpectedly
        // blank glyphs are elided.  Here, however, we pass nullptr for
        // "blanks", effectively assuming that, if the font has a blank glyph,
        // then the author intends any associated character to be rendered
        // blank.
        pattern =
            (*sQueryFacePtr)(mFace,
                             gfxFontconfigUtils::ToFcChar8(""),
                             0,
                             nullptr);
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
        nsAutoRef<FcCharSet> charset(FcFreeTypeCharSet(mFace, nullptr));
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

hb_blob_t *
gfxDownloadedFcFontEntry::GetFontTable(uint32_t aTableTag)
{
    // The entry already owns the (sanitized) sfnt data in mFontData,
    // so we can just return a blob that "wraps" the appropriate chunk of it.
    // The blob should not attempt to free its data, as the entire sfnt data
    // will be freed when the font entry is deleted.
    return GetTableFromFontData(mFontData, aTableTag);
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

#ifdef USE_SKIA
    virtual mozilla::TemporaryRef<mozilla::gfx::GlyphRenderingOptions>
        GetGlyphRenderingOptions(const TextRunDrawParams* aRunParams = nullptr) override;
#endif

    // return a cloned font resized and offset to simulate sub/superscript glyphs
    virtual already_AddRefed<gfxFont>
    GetSubSuperscriptFont(int32_t aAppUnitsPerDevPixel) override;

protected:
    virtual already_AddRefed<gfxFont> MakeScaledFont(gfxFontStyle *aFontStyle,
                                                     gfxFloat aFontScale);
    virtual already_AddRefed<gfxFont> GetSmallCapsFont() override;

private:
    gfxFcFont(cairo_scaled_font_t *aCairoFont, gfxFcFontEntry *aFontEntry,
              const gfxFontStyle *aFontStyle);

    // key for locating a gfxFcFont corresponding to a cairo_scaled_font
    static cairo_user_data_key_t sGfxFontKey;
};

/**
 * gfxFcFontSet:
 *
 * Translation from a desired FcPattern to a sorted set of font references
 * (fontconfig cache data) and (when needed) fonts.
 */

class gfxFcFontSet final {
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
    gfxFcFont *GetFontAt(uint32_t i, const gfxFontStyle *aFontStyle)
    {
        if (i >= mFonts.Length() || !mFonts[i].mFont) { 
            // GetFontPatternAt sets up mFonts
            FcPattern *fontPattern = GetFontPatternAt(i);
            if (!fontPattern)
                return nullptr;

            mFonts[i].mFont =
                gfxFcFont::GetOrMakeFont(mSortPattern, fontPattern,
                                         aFontStyle);
        }
        return mFonts[i].mFont;
    }

    FcPattern *GetFontPatternAt(uint32_t i);

    bool WaitingForUserFont() const {
        return mWaitingForUserFont;
    }

private:
    // Private destructor, to discourage deletion outside of Release():
    ~gfxFcFontSet()
    {
    }

    nsReturnRef<FcFontSet> SortPreferredFonts(bool& aWaitForUserFont);
    nsReturnRef<FcFontSet> SortFallbackFonts();

    struct FontEntry {
        explicit FontEntry(FcPattern *aPattern) : mPattern(aPattern) {}
        nsCountedRef<FcPattern> mPattern;
        nsRefPtr<gfxFcFont> mFont;
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
    // trimmed and added to mFonts (so that mFcFontSet is nullptr).
    bool mHaveFallbackFonts;
    // True iff there was a user font set with pending downloads,
    // so the set may be updated when downloads complete
    bool mWaitingForUserFont;
};

// Find the FcPattern for an @font-face font suitable for CSS family |aFamily|
// and style |aStyle| properties.
static const nsTArray< nsCountedRef<FcPattern> >*
FindFontPatterns(gfxUserFontSet *mUserFontSet,
                 const nsACString &aFamily, uint8_t aStyle,
                 uint16_t aWeight, int16_t aStretch,
                 bool& aWaitForUserFont)
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

    gfxUserFcFontEntry *fontEntry = nullptr;
    gfxFontFamily *family = mUserFontSet->LookupFamily(utf16Family);
    if (family) {
        gfxUserFontEntry* userFontEntry =
            mUserFontSet->FindUserFontEntryAndLoad(family, style, needsBold,
                                                   aWaitForUserFont);
        if (userFontEntry) {
            fontEntry = static_cast<gfxUserFcFontEntry*>
                (userFontEntry->GetPlatformFontEntry());
        }

        // Accept synthetic oblique for italic and oblique.
        // xxx - this isn't really ideal behavior, for docs that only use a
        //       single italic face it will also pull down the normal face
        //       and probably never use it
        if (!fontEntry && aStyle != NS_FONT_STYLE_NORMAL) {
            style.style = NS_FONT_STYLE_NORMAL;
            userFontEntry =
                mUserFontSet->FindUserFontEntryAndLoad(family, style,
                                                       needsBold,
                                                       aWaitForUserFont);
            if (userFontEntry) {
                fontEntry = static_cast<gfxUserFcFontEntry*>
                    (userFontEntry->GetPlatformFontEntry());
            }
        }
    }

    if (!fontEntry) {
        return nullptr;
    }

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

    nsTHashtable<gfxFontconfigUtils::DepFcStrEntry> existingFamilies(32);
    FcChar8 *family;
    for (int v = 0;
         FcPatternGetString(mSortPattern,
                            FC_FAMILY, v, &family) == FcResultMatch; ++v) {
        const nsTArray< nsCountedRef<FcPattern> > *familyFonts = nullptr;

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

                uint8_t thebesStyle =
                    gfxFontconfigUtils::FcSlantToThebesStyle(requestedSlant);
                uint16_t thebesWeight =
                    gfxFontconfigUtils::GetThebesWeight(mSortPattern);
                int16_t thebesStretch =
                    gfxFontconfigUtils::GetThebesStretch(mSortPattern);

                bool waitForUserFont;
                familyFonts = FindFontPatterns(mUserFontSet, cssFamily,
                                               thebesStyle,
                                               thebesWeight, thebesStretch,
                                               waitForUserFont);
                if (waitForUserFont) {
                    aWaitForUserFont = true;
                }
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

        for (uint32_t f = 0; f < familyFonts->Length(); ++f) {
            FcPattern *font = familyFonts->ElementAt(f);

            // Fix up the family name of user-font patterns, as the same
            // font entry may be used (via the UserFontCache) for multiple
            // CSS family names
            if (isUserFont) {
                font = FcPatternDuplicate(font);
                FcPatternDel(font, FC_FAMILY);
                FcPatternAddString(font, FC_FAMILY, family);
            }

            // User fonts are already filtered by slant (but not size) in
            // mUserFontSet->FindUserFontEntry().
            if (requestedSize != -1.0 && !SizeIsAcceptable(font, requestedSize))
                continue;

            for (uint32_t r = 0; r < requiredLangs.Length(); ++r) {
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
                // We don't add a reference here for user fonts, because we're
                // using a local clone of the pattern (see above) in order to
                // override the family name
                if (!isUserFont) {
                    FcPatternReference(font);
                }
            }
        }
    }

    FcPattern *truncateMarker = nullptr;
    for (uint32_t r = 0; r < requiredLangs.Length(); ++r) {
        const nsTArray< nsCountedRef<FcPattern> >& langFonts =
            utils->GetFontsForLang(requiredLangs[r].mLang);

        bool haveLangFont = false;
        for (uint32_t f = 0; f < langFonts.Length(); ++f) {
            FcPattern *font = langFonts[f];
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
    // Get around a crash of FcFontSetSort when FcConfig is nullptr
    // Solaris's FcFontSetSort needs an FcConfig (bug 474758)
    fontSet.own(FcFontSetSort(FcConfigGetCurrent(), sets, 1, mSortPattern,
                              FcFalse, nullptr, &result));
#else
    fontSet.own(FcFontSetSort(nullptr, sets, 1, mSortPattern,
                              FcFalse, nullptr, &result));
#endif

    if (truncateMarker != nullptr && fontSet) {
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
    return nsReturnRef<FcFontSet>(FcFontSort(nullptr, mSortPattern,
                                             FcFalse, nullptr, &result));
}

// GetFontAt relies on this setting up all patterns up to |i|.
FcPattern *
gfxFcFontSet::GetFontPatternAt(uint32_t i)
{
    while (i >= mFonts.Length()) {
        while (!mFcFontSet) {
            if (mHaveFallbackFonts)
                return nullptr;

            mFcFontSet = SortFallbackFonts();
            mHaveFallbackFonts = true;
            mFcFontsTrimmed = 0;
            // Loop to test that mFcFontSet is non-nullptr.
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
                    FcCharSet *newChars = nullptr;
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

#ifdef MOZ_WIDGET_GTK
static void ApplyGdkScreenFontOptions(FcPattern *aPattern);
#endif

// Apply user settings and defaults to pattern in preparation for matching.
static void
PrepareSortPattern(FcPattern *aPattern, double aFallbackSize,
                   double aSizeAdjustFactor, bool aIsPrinterFont)
{
    FcConfigSubstitute(nullptr, aPattern, FcMatchPattern);

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
#ifdef MOZ_WIDGET_GTK
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

gfxPangoFontGroup::gfxPangoFontGroup(const FontFamilyList& aFontFamilyList,
                                     const gfxFontStyle *aStyle,
                                     gfxUserFontSet *aUserFontSet)
    : gfxFontGroup(aFontFamilyList, aStyle, aUserFontSet),
      mPangoLanguage(GuessPangoLanguage(aStyle->language))
{
    // This language is passed to the font for shaping.
    // Shaping doesn't know about lang groups so make it a real language.
    if (mPangoLanguage) {
        mStyle.language = do_GetAtom(pango_language_to_string(mPangoLanguage));
    }

    // dummy entry, will be replaced when actually needed
    mFonts.AppendElement(FamilyFace());
    mSkipUpdateUserFonts = true;
}

gfxPangoFontGroup::~gfxPangoFontGroup()
{
}

gfxFontGroup *
gfxPangoFontGroup::Copy(const gfxFontStyle *aStyle)
{
    return new gfxPangoFontGroup(mFamilyList, aStyle, mUserFontSet);
}

void
gfxPangoFontGroup::FindPlatformFont(const nsAString& fontName,
                                    bool aUseFontSet,
                                    void *aClosure)
{
    nsTArray<nsString> *list = static_cast<nsTArray<nsString>*>(aClosure);

    if (!list->Contains(fontName)) {
        // names present in the user fontset are not matched against system fonts
        if (aUseFontSet && mUserFontSet && mUserFontSet->HasFamily(fontName)) {
            nsAutoString userFontName =
                NS_LITERAL_STRING(FONT_FACE_FAMILY_PREFIX) + fontName;
            list->AppendElement(userFontName);
        } else {
            list->AppendElement(fontName);
        }
    }
}

gfxFcFont *
gfxPangoFontGroup::GetBaseFont()
{
    if (mFonts[0].Font() == nullptr) {
        gfxFont* font = GetBaseFontSet()->GetFontAt(0, GetStyle());
        mFonts[0] = FamilyFace(nullptr, font);
    }

    return static_cast<gfxFcFont*>(mFonts[0].Font());
}

gfxFont*
gfxPangoFontGroup::GetFirstValidFont(uint32_t aCh)
{
    return GetFontAt(0);
}

gfxFont *
gfxPangoFontGroup::GetFontAt(int32_t i, uint32_t aCh)
{
    // If it turns out to be hard for all clients that cache font
    // groups to call UpdateUserFonts at appropriate times, we could
    // instead consider just calling UpdateUserFonts from someplace
    // more central (such as here).
    NS_ASSERTION(!mUserFontSet || mCurrGeneration == GetGeneration(),
                 "Whoever was caching this font group should have "
                 "called UpdateUserFonts on it");

    NS_PRECONDITION(i == 0, "Only have one font");

    return GetBaseFont();
}

void
gfxPangoFontGroup::UpdateUserFonts()
{
    uint64_t newGeneration = GetGeneration();
    if (newGeneration == mCurrGeneration)
        return;

    mFonts[0] = FamilyFace();
    mFontSets.Clear();
    mCachedEllipsisTextRun = nullptr;
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
    EnumerateFontList(langGroup ? langGroup.get() : mStyle.language.get(),
                      &fcFamilyList);

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

    for (uint32_t i = 0; i < mFontSets.Length(); ++i) {
        if (mFontSets[i].mLang == aLang)
            return mFontSets[i].mFontSet;
    }

    nsRefPtr<gfxFcFontSet> fontSet =
        MakeFontSet(aLang, mSizeAdjustFactor);
    mFontSets.AppendElement(FontSetByLangEntry(aLang, fontSet));

    return fontSet;
}

already_AddRefed<gfxFont>
gfxPangoFontGroup::FindFontForChar(uint32_t aCh, uint32_t aPrevCh,
                                   uint32_t aNextCh, int32_t aRunScript,
                                   gfxFont *aPrevMatchedFont,
                                   uint8_t *aMatchType)
{
    if (aPrevMatchedFont) {
        // Don't switch fonts for control characters, regardless of
        // whether they are present in the current font, as they won't
        // actually be rendered (see bug 716229)
        uint8_t category = GetGeneralCategory(aCh);
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
        return nullptr;
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
    uint32_t nextFont = 0;
    FcPattern *basePattern = nullptr;
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

    for (uint32_t i = nextFont;
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

    return nullptr;
}

// Sanity-check: spot-check a few constants to confirm that Thebes and
// Pango script codes really do match
#define CHECK_SCRIPT_CODE(script) \
    PR_STATIC_ASSERT(int32_t(MOZ_SCRIPT_##script) == \
                     int32_t(PANGO_SCRIPT_##script))

CHECK_SCRIPT_CODE(COMMON);
CHECK_SCRIPT_CODE(INHERITED);
CHECK_SCRIPT_CODE(ARABIC);
CHECK_SCRIPT_CODE(LATIN);
CHECK_SCRIPT_CODE(UNKNOWN);
CHECK_SCRIPT_CODE(NKO);

/**
 ** gfxFcFont
 **/

cairo_user_data_key_t gfxFcFont::sGfxFontKey;

gfxFcFont::gfxFcFont(cairo_scaled_font_t *aCairoFont,
                     gfxFcFontEntry *aFontEntry,
                     const gfxFontStyle *aFontStyle)
    : gfxFT2FontBase(aCairoFont, aFontEntry, aFontStyle)
{
    cairo_scaled_font_set_user_data(mScaledFont, &sGfxFontKey, this, nullptr);
}

gfxFcFont::~gfxFcFont()
{
    cairo_scaled_font_set_user_data(mScaledFont,
                                    &sGfxFontKey,
                                    nullptr,
                                    nullptr);
}

already_AddRefed<gfxFont>
gfxFcFont::GetSubSuperscriptFont(int32_t aAppUnitsPerDevPixel)
{
    gfxFontStyle style(*GetStyle());
    style.AdjustForSubSuperscript(aAppUnitsPerDevPixel);
    return MakeScaledFont(&style, style.size / GetStyle()->size);
}

already_AddRefed<gfxFont>
gfxFcFont::MakeScaledFont(gfxFontStyle *aFontStyle, gfxFloat aScaleFactor)
{
    gfxFcFontEntry* fe = static_cast<gfxFcFontEntry*>(GetFontEntry());
    nsRefPtr<gfxFont> font = gfxFontCache::GetCache()->Lookup(fe, aFontStyle);
    if (font) {
        return font.forget();
    }

    cairo_matrix_t fontMatrix;
    cairo_scaled_font_get_font_matrix(mScaledFont, &fontMatrix);
    cairo_matrix_scale(&fontMatrix, aScaleFactor, aScaleFactor);

    cairo_matrix_t ctm;
    cairo_scaled_font_get_ctm(mScaledFont, &ctm);

    cairo_font_options_t *options = cairo_font_options_create();
    cairo_scaled_font_get_font_options(mScaledFont, options);

    cairo_scaled_font_t *newFont =
        cairo_scaled_font_create(cairo_scaled_font_get_font_face(mScaledFont),
                                 &fontMatrix, &ctm, options);
    cairo_font_options_destroy(options);

    font = new gfxFcFont(newFont, fe, aFontStyle);
    gfxFontCache::GetCache()->AddNew(font);
    cairo_scaled_font_destroy(newFont);

    return font.forget();
}

already_AddRefed<gfxFont>
gfxFcFont::GetSmallCapsFont()
{
    gfxFontStyle style(*GetStyle());
    style.size *= SMALL_CAPS_SCALE_FACTOR;
    style.variantCaps = NS_FONT_VARIANT_CAPS_NORMAL;
    return MakeScaledFont(&style, SMALL_CAPS_SCALE_FACTOR);
}

/* static */ void
gfxPangoFontGroup::Shutdown()
{
    // Resetting gFTLibrary in case this is wanted again after a
    // cairo_debug_reset_static_data.
    gFTLibrary = nullptr;
}

/* static */ gfxFontEntry *
gfxPangoFontGroup::NewFontEntry(const nsAString& aFontName,
                                uint16_t aWeight,
                                int16_t aStretch,
                                bool aItalic)
{
    gfxFontconfigUtils *utils = gfxFontconfigUtils::GetFontconfigUtils();
    if (!utils)
        return nullptr;

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
        return nullptr;

    NS_ConvertUTF16toUTF8 fullname(aFontName);
    FcPatternAddString(pattern, FC_FULLNAME,
                       gfxFontconfigUtils::ToFcChar8(fullname));
    FcConfigSubstitute(nullptr, pattern, FcMatchPattern);

    FcChar8 *name;
    for (int v = 0;
         FcPatternGetString(pattern, FC_FULLNAME, v, &name) == FcResultMatch;
         ++v) {
        const nsTArray< nsCountedRef<FcPattern> >& fonts =
            utils->GetFontsForFullname(name);

        if (fonts.Length() != 0)
            return new gfxLocalFcFontEntry(aFontName,
                                           aWeight,
                                           aStretch,
                                           aItalic,
                                           fonts);
    }

    return nullptr;
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
            new gfxPangoFontGroup(FontFamilyList(eFamily_sans_serif),
                                  &style, nullptr);

        gfxFcFont *font = fontGroup->GetBaseFont();
        if (!font)
            return nullptr;

        gfxFT2LockedFace face(font);
        if (!face.get())
            return nullptr;

        gFTLibrary = face.get()->glyph->library;
    }

    return gFTLibrary;
}

/* static */ gfxFontEntry *
gfxPangoFontGroup::NewFontEntry(const nsAString& aFontName,
                                uint16_t aWeight,
                                int16_t aStretch,
                                bool aItalic,
                                const uint8_t* aFontData,
                                uint32_t aLength)
{
    // Ownership of aFontData is passed in here, and transferred to the
    // new fontEntry, which will release it when no longer needed.

    // Using face_index = 0 for the first face in the font, as we have no
    // other information.  FT_New_Memory_Face checks for a nullptr FT_Library.
    FT_Face face;
    FT_Error error =
        FT_New_Memory_Face(GetFTLibrary(), aFontData, aLength, 0, &face);
    if (error != 0) {
        free((void*)aFontData);
        return nullptr;
    }

    return new gfxDownloadedFcFontEntry(aFontName, aWeight,
                                        aStretch, aItalic,
                                        aFontData, face);
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
        (FcFontRenderPrepare(nullptr, aRequestedPattern, aFontPattern));

    // If synthetic bold/italic is not allowed by the style, adjust the
    // resulting pattern to match the actual properties of the font.
    if (!aFontStyle->allowSyntheticWeight) {
        int weight;
        if (FcPatternGetInteger(aFontPattern, FC_WEIGHT, 0,
                                &weight) == FcResultMatch) {
            FcPatternDel(renderPattern, FC_WEIGHT);
            FcPatternAddInteger(renderPattern, FC_WEIGHT, weight);
        }
    }
    if (!aFontStyle->allowSyntheticStyle) {
        int slant;
        if (FcPatternGetInteger(aFontPattern, FC_SLANT, 0,
                                &slant) == FcResultMatch) {
            FcPatternDel(renderPattern, FC_SLANT);
            FcPatternAddInteger(renderPattern, FC_SLANT, slant);
        }
    }

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
                    name.Append('/');
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
    if (size != 0.0 && mStyle.sizeAdjust > 0.0) {
        gfxFcFont *font = fontSet->GetFontAt(0, GetStyle());
        if (font) {
            const gfxFont::Metrics& metrics =
                font->GetMetrics(gfxFont::eHorizontal); // XXX vertical?

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
    if (printing) {
        cairo_font_options_set_hint_metrics(fontOptions, CAIRO_HINT_METRICS_OFF);
    } else {
        cairo_font_options_set_hint_metrics(fontOptions, CAIRO_HINT_METRICS_ON);
    }

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
    if (FcPatternGetBool(aPattern, FC_HINTING, 0, &hinting) != FcResultMatch) {
        hinting = FcTrue;
    }

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

/* static */
PangoLanguage *
GuessPangoLanguage(nsIAtom *aLanguage)
{
    if (!aLanguage)
        return nullptr;

    // Pango and fontconfig won't understand mozilla's internal langGroups, so
    // find a real language.
    nsAutoCString lang;
    gfxFontconfigUtils::GetSampleLangForGroup(aLanguage, &lang);
    if (lang.IsEmpty())
        return nullptr;

    return pango_language_from_string(lang.get());
}

#ifdef MOZ_WIDGET_GTK
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

#ifdef USE_SKIA
mozilla::TemporaryRef<mozilla::gfx::GlyphRenderingOptions>
gfxFcFont::GetGlyphRenderingOptions(const TextRunDrawParams* aRunParams)
{
  cairo_scaled_font_t *scaled_font = CairoScaledFont();
  cairo_font_options_t *options = cairo_font_options_create();
  cairo_scaled_font_get_font_options(scaled_font, options);
  cairo_hint_style_t hint_style = cairo_font_options_get_hint_style(options);     
  cairo_font_options_destroy(options);

  mozilla::gfx::FontHinting hinting;

  switch (hint_style) {
    case CAIRO_HINT_STYLE_NONE:
      hinting = mozilla::gfx::FontHinting::NONE;
      break;
    case CAIRO_HINT_STYLE_SLIGHT:
      hinting = mozilla::gfx::FontHinting::LIGHT;
      break;
    case CAIRO_HINT_STYLE_FULL:
      hinting = mozilla::gfx::FontHinting::FULL;
      break;
    default:
      hinting = mozilla::gfx::FontHinting::NORMAL;
      break;
  }

  // We don't want to force the use of the autohinter over the font's built in hints
  return mozilla::gfx::Factory::CreateCairoGlyphRenderingOptions(hinting, false);
}
#endif


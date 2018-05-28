/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"

#include "gfxFcPlatformFontList.h"
#include "gfxFont.h"
#include "gfxFontConstants.h"
#include "gfxFontFamilyList.h"
#include "gfxFT2Utils.h"
#include "gfxPlatform.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/Preferences.h"
#include "mozilla/Sprintf.h"
#include "mozilla/TimeStamp.h"
#include "nsGkAtoms.h"
#include "nsUnicodeProperties.h"
#include "nsUnicodeRange.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsXULAppAPI.h"

#include "mozilla/gfx/HelpersCairo.h"

#include <fontconfig/fcfreetype.h>
#include <dlfcn.h>
#include <unistd.h>

#ifdef MOZ_WIDGET_GTK
#include <gdk/gdk.h>
#include "gfxPlatformGtk.h"
#endif

#ifdef MOZ_X11
#include "mozilla/X11Util.h"
#endif

#ifdef MOZ_CONTENT_SANDBOX
#include "mozilla/SandboxBrokerPolicyFactory.h"
#include "mozilla/SandboxSettings.h"
#endif

#include FT_MULTIPLE_MASTERS_H

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::unicode;

using mozilla::dom::SystemFontListEntry;
using mozilla::dom::FontPatternListEntry;

#ifndef FC_POSTSCRIPT_NAME
#define FC_POSTSCRIPT_NAME  "postscriptname"      /* String */
#endif
#ifndef FC_VARIABLE
#define FC_VARIABLE         "variable"            /* Bool */
#endif

#define PRINTING_FC_PROPERTY "gfx.printing"

#define LOG_FONTLIST(args) MOZ_LOG(gfxPlatform::GetLog(eGfxLog_fontlist), \
                               LogLevel::Debug, args)
#define LOG_FONTLIST_ENABLED() MOZ_LOG_TEST( \
                                   gfxPlatform::GetLog(eGfxLog_fontlist), \
                                   LogLevel::Debug)
#define LOG_CMAPDATA_ENABLED() MOZ_LOG_TEST( \
                                   gfxPlatform::GetLog(eGfxLog_cmapdata), \
                                   LogLevel::Debug)

template <>
class nsAutoRefTraits<FcFontSet> : public nsPointerRefTraits<FcFontSet>
{
public:
    static void Release(FcFontSet *ptr) { FcFontSetDestroy(ptr); }
};

template <>
class nsAutoRefTraits<FcObjectSet> : public nsPointerRefTraits<FcObjectSet>
{
public:
    static void Release(FcObjectSet *ptr) { FcObjectSetDestroy(ptr); }
};

static const FcChar8*
ToFcChar8Ptr(const char* aStr)
{
    return reinterpret_cast<const FcChar8*>(aStr);
}

static const char*
ToCharPtr(const FcChar8 *aStr)
{
    return reinterpret_cast<const char*>(aStr);
}

FT_Library gfxFcPlatformFontList::sCairoFTLibrary = nullptr;

static cairo_user_data_key_t sFcFontlistUserFontDataKey;

// canonical name ==> first en name or first name if no en name
// This is the required logic for fullname lookups as per CSS3 Fonts spec.
static uint32_t
FindCanonicalNameIndex(FcPattern* aFont, const char* aLangField)
{
    uint32_t n = 0, en = 0;
    FcChar8* lang;
    while (FcPatternGetString(aFont, aLangField, n, &lang) == FcResultMatch) {
        // look for 'en' or variants, en-US, en-JP etc.
        uint32_t len = strlen(ToCharPtr(lang));
        bool enPrefix = (strncmp(ToCharPtr(lang), "en", 2) == 0);
        if (enPrefix && (len == 2 || (len > 2 && aLangField[2] == '-'))) {
            en = n;
            break;
        }
        n++;
    }
    return en;
}

static void
GetFaceNames(FcPattern* aFont, const nsAString& aFamilyName,
             nsAString& aPostscriptName, nsAString& aFullname)
{
    // get the Postscript name
    FcChar8* psname;
    if (FcPatternGetString(aFont, FC_POSTSCRIPT_NAME, 0, &psname) == FcResultMatch) {
        AppendUTF8toUTF16(ToCharPtr(psname), aPostscriptName);
    }

    // get the canonical fullname (i.e. en name or first name)
    uint32_t en = FindCanonicalNameIndex(aFont, FC_FULLNAMELANG);
    FcChar8* fullname;
    if (FcPatternGetString(aFont, FC_FULLNAME, en, &fullname) == FcResultMatch) {
        AppendUTF8toUTF16(ToCharPtr(fullname), aFullname);
    }

    // if have fullname, done
    if (!aFullname.IsEmpty()) {
        return;
    }

    // otherwise, set the fullname to family + style name [en] and use that
    aFullname.Append(aFamilyName);

    // figure out the en style name
    en = FindCanonicalNameIndex(aFont, FC_STYLELANG);
    nsAutoString style;
    FcChar8* stylename = nullptr;
    FcPatternGetString(aFont, FC_STYLE, en, &stylename);
    if (stylename) {
        AppendUTF8toUTF16(ToCharPtr(stylename), style);
    }

    if (!style.IsEmpty() && !style.EqualsLiteral("Regular")) {
        aFullname.Append(' ');
        aFullname.Append(style);
    }
}

static FontWeight
MapFcWeight(int aFcWeight)
{
  if (aFcWeight <= (FC_WEIGHT_THIN + FC_WEIGHT_EXTRALIGHT) / 2) {
    return FontWeight(100);
  }
  if (aFcWeight <= (FC_WEIGHT_EXTRALIGHT + FC_WEIGHT_LIGHT) / 2) {
    return FontWeight(200);
  }
  if (aFcWeight <= (FC_WEIGHT_LIGHT + FC_WEIGHT_BOOK) / 2) {
    return FontWeight(300);
  }
  if (aFcWeight <= (FC_WEIGHT_REGULAR + FC_WEIGHT_MEDIUM) / 2) {
    // This includes FC_WEIGHT_BOOK
    return FontWeight(400);
  }
  if (aFcWeight <= (FC_WEIGHT_MEDIUM + FC_WEIGHT_DEMIBOLD) / 2) {
    return FontWeight(500);
  }
  if (aFcWeight <= (FC_WEIGHT_DEMIBOLD + FC_WEIGHT_BOLD) / 2) {
    return FontWeight(600);
  }
  if (aFcWeight <= (FC_WEIGHT_BOLD + FC_WEIGHT_EXTRABOLD) / 2) {
    return FontWeight(700);
  }
  if (aFcWeight <= (FC_WEIGHT_EXTRABOLD + FC_WEIGHT_BLACK) / 2) {
    return FontWeight(800);
  }
  if (aFcWeight <= FC_WEIGHT_BLACK) {
    return FontWeight(900);
  }

  // including FC_WEIGHT_EXTRABLACK
  return FontWeight(901);
}

// TODO(emilio, jfkthame): I think this can now be more fine-grained.
static FontStretch
MapFcWidth(int aFcWidth)
{
    if (aFcWidth <= (FC_WIDTH_ULTRACONDENSED + FC_WIDTH_EXTRACONDENSED) / 2) {
        return FontStretch::UltraCondensed();
    }
    if (aFcWidth <= (FC_WIDTH_EXTRACONDENSED + FC_WIDTH_CONDENSED) / 2) {
        return FontStretch::ExtraCondensed();
    }
    if (aFcWidth <= (FC_WIDTH_CONDENSED + FC_WIDTH_SEMICONDENSED) / 2) {
        return FontStretch::Condensed();
    }
    if (aFcWidth <= (FC_WIDTH_SEMICONDENSED + FC_WIDTH_NORMAL) / 2) {
        return FontStretch::SemiCondensed();
    }
    if (aFcWidth <= (FC_WIDTH_NORMAL + FC_WIDTH_SEMIEXPANDED) / 2) {
        return FontStretch::Normal();
    }
    if (aFcWidth <= (FC_WIDTH_SEMIEXPANDED + FC_WIDTH_EXPANDED) / 2) {
        return FontStretch::SemiExpanded();
    }
    if (aFcWidth <= (FC_WIDTH_EXPANDED + FC_WIDTH_EXTRAEXPANDED) / 2) {
        return FontStretch::Expanded();
    }
    if (aFcWidth <= (FC_WIDTH_EXTRAEXPANDED + FC_WIDTH_ULTRAEXPANDED) / 2) {
        return FontStretch::ExtraExpanded();
    }
    return FontStretch::UltraExpanded();
}

gfxFontconfigFontEntry::gfxFontconfigFontEntry(const nsAString& aFaceName,
                                               FcPattern* aFontPattern,
                                               bool aIgnoreFcCharmap)
        : gfxFontEntry(aFaceName), mFontPattern(aFontPattern),
          mFTFace(nullptr), mFTFaceInitialized(false),
          mIgnoreFcCharmap(aIgnoreFcCharmap),
          mHasVariationsInitialized(false),
          mAspect(0.0), mFontData(nullptr), mLength(0)
{
    // italic
    int slant;
    if (FcPatternGetInteger(aFontPattern, FC_SLANT, 0, &slant) != FcResultMatch) {
        slant = FC_SLANT_ROMAN;
    }
    if (slant == FC_SLANT_OBLIQUE) {
        mStyleRange = SlantStyleRange(FontSlantStyle::Oblique());
    } else if (slant > 0) {
        mStyleRange = SlantStyleRange(FontSlantStyle::Italic());
    }

    // weight
    int weight;
    if (FcPatternGetInteger(aFontPattern, FC_WEIGHT, 0, &weight) != FcResultMatch) {
        weight = FC_WEIGHT_REGULAR;
    }
    mWeightRange = WeightRange(MapFcWeight(weight));

    // width
    int width;
    if (FcPatternGetInteger(aFontPattern, FC_WIDTH, 0, &width) != FcResultMatch) {
        width = FC_WIDTH_NORMAL;
    }
    mStretchRange = StretchRange(MapFcWidth(width));
}

gfxFontEntry*
gfxFontconfigFontEntry::Clone() const
{
    MOZ_ASSERT(!IsUserFont(), "we can only clone installed fonts!");
    return new gfxFontconfigFontEntry(Name(), mFontPattern, mIgnoreFcCharmap);
}

static FcPattern*
CreatePatternForFace(FT_Face aFace)
{
    // Use fontconfig to fill out the pattern from the FTFace.
    // The "file" argument cannot be nullptr (in fontconfig-2.6.0 at
    // least). The dummy file passed here is removed below.
    //
    // When fontconfig scans the system fonts, FcConfigGetBlanks(nullptr)
    // is passed as the "blanks" argument, which provides that unexpectedly
    // blank glyphs are elided.  Here, however, we pass nullptr for
    // "blanks", effectively assuming that, if the font has a blank glyph,
    // then the author intends any associated character to be rendered
    // blank.
    FcPattern* pattern =
        FcFreeTypeQueryFace(aFace, ToFcChar8Ptr(""), 0, nullptr);
    // given that we have a FT_Face, not really sure this is possible...
    if (!pattern) {
        pattern = FcPatternCreate();
    }
    FcPatternDel(pattern, FC_FILE);
    FcPatternDel(pattern, FC_INDEX);

    // Make a new pattern and store the face in it so that cairo uses
    // that when creating a cairo font face.
    FcPatternAddFTFace(pattern, FC_FT_FACE, aFace);

    return pattern;
}

static FT_Face
CreateFaceForPattern(FcPattern* aPattern)
{
    FcChar8 *filename;
    if (FcPatternGetString(aPattern, FC_FILE, 0, &filename) != FcResultMatch) {
        return nullptr;
    }
    int index;
    if (FcPatternGetInteger(aPattern, FC_INDEX, 0, &index) != FcResultMatch) {
        index = 0; // default to 0 if not found in pattern
    }
    return Factory::NewFTFace(nullptr, ToCharPtr(filename), index);
}

gfxFontconfigFontEntry::gfxFontconfigFontEntry(const nsAString& aFaceName,
                                               WeightRange aWeight,
                                               StretchRange aStretch,
                                               SlantStyleRange aStyle,
                                               const uint8_t *aData,
                                               uint32_t aLength,
                                               FT_Face aFace)
    : gfxFontEntry(aFaceName),
      mFTFace(aFace), mFTFaceInitialized(true),
      mIgnoreFcCharmap(true),
      mHasVariationsInitialized(false),
      mAspect(0.0), mFontData(aData), mLength(aLength)
{
    mWeightRange = aWeight;
    mStyleRange = aStyle;
    mStretchRange = aStretch;
    mIsDataUserFont = true;

    mFontPattern = CreatePatternForFace(mFTFace);

    mUserFontData = new FTUserFontData(mFTFace, mFontData);
}

gfxFontconfigFontEntry::gfxFontconfigFontEntry(const nsAString& aFaceName,
                                               FcPattern* aFontPattern,
                                               WeightRange aWeight,
                                               StretchRange aStretch,
                                               SlantStyleRange aStyle)
        : gfxFontEntry(aFaceName), mFontPattern(aFontPattern),
          mFTFace(nullptr), mFTFaceInitialized(false),
          mHasVariationsInitialized(false),
          mAspect(0.0), mFontData(nullptr), mLength(0)
{
    mWeightRange = aWeight;
    mStyleRange = aStyle;
    mStretchRange = aStretch;
    mIsLocalUserFont = true;

    // The proper setting of mIgnoreFcCharmap is tricky for fonts loaded
    // via src:local()...
    // If the local font happens to come from the application fontset,
    // we want to set it to true so that color/svg fonts will work even
    // if the default glyphs are blank; but if the local font is a non-
    // sfnt face (e.g. legacy type 1) then we need to set it to false
    // because our cmap-reading code will fail and we depend on FT+Fc to
    // determine the coverage.
    // We set the flag here, but may flip it the first time TestCharacterMap
    // is called, at which point we'll look to see whether a 'cmap' is
    // actually present in the font.
    mIgnoreFcCharmap = true;
}

typedef FT_Error (*GetVarFunc)(FT_Face, FT_MM_Var**);
typedef FT_Error (*DoneVarFunc)(FT_Library, FT_MM_Var*);
static GetVarFunc sGetVar;
static DoneVarFunc sDoneVar;
static bool sInitializedVarFuncs = false;

static void
InitializeVarFuncs()
{
    if (sInitializedVarFuncs) {
        return;
    }
    sInitializedVarFuncs = true;
#if MOZ_TREE_FREETYPE
    sGetVar = &FT_Get_MM_Var;
    sDoneVar = &FT_Done_MM_Var;
#else
    sGetVar = (GetVarFunc)dlsym(RTLD_DEFAULT, "FT_Get_MM_Var");
    sDoneVar = (DoneVarFunc)dlsym(RTLD_DEFAULT, "FT_Done_MM_Var");
#endif
}

gfxFontconfigFontEntry::~gfxFontconfigFontEntry()
{
    if (mMMVar) {
        // Prior to freetype 2.9, there was no specific function to free the
        // FT_MM_Var record, and the docs just said to use free().
        // InitializeVarFuncs must have been called in order for mMMVar to be
        // non-null here, so we don't need to do it again.
        if (sDoneVar) {
            MOZ_ASSERT(mFTFace, "How did mMMVar get set without a face?");
            (*sDoneVar)(mFTFace->glyph->library, mMMVar);
        } else {
            free(mMMVar);
        }
    }
}

nsresult
gfxFontconfigFontEntry::ReadCMAP(FontInfoData *aFontInfoData)
{
    // attempt this once, if errors occur leave a blank cmap
    if (mCharacterMap) {
        return NS_OK;
    }

    RefPtr<gfxCharacterMap> charmap;
    nsresult rv;

    if (aFontInfoData && (charmap = GetCMAPFromFontInfo(aFontInfoData,
                                                        mUVSOffset))) {
        rv = NS_OK;
    } else {
        uint32_t kCMAP = TRUETYPE_TAG('c','m','a','p');
        charmap = new gfxCharacterMap();
        AutoTable cmapTable(this, kCMAP);

        if (cmapTable) {
            uint32_t cmapLen;
            const uint8_t* cmapData =
                reinterpret_cast<const uint8_t*>(hb_blob_get_data(cmapTable,
                                                                  &cmapLen));
            rv = gfxFontUtils::ReadCMAP(cmapData, cmapLen,
                                        *charmap, mUVSOffset);
        } else {
            rv = NS_ERROR_NOT_AVAILABLE;
        }
    }

    mHasCmapTable = NS_SUCCEEDED(rv);
    if (mHasCmapTable) {
        gfxPlatformFontList *pfl = gfxPlatformFontList::PlatformFontList();
        mCharacterMap = pfl->FindCharMap(charmap);
    } else {
        // if error occurred, initialize to null cmap
        mCharacterMap = new gfxCharacterMap();
    }

    LOG_FONTLIST(("(fontlist-cmap) name: %s, size: %zu hash: %8.8x%s\n",
                  NS_ConvertUTF16toUTF8(mName).get(),
                  charmap->SizeOfIncludingThis(moz_malloc_size_of),
                  charmap->mHash, mCharacterMap == charmap ? " new" : ""));
    if (LOG_CMAPDATA_ENABLED()) {
        char prefix[256];
        SprintfLiteral(prefix, "(cmapdata) name: %.220s",
                       NS_ConvertUTF16toUTF8(mName).get());
        charmap->Dump(prefix, eGfxLog_cmapdata);
    }

    return rv;
}

static bool
HasChar(FcPattern *aFont, FcChar32 aCh)
{
    FcCharSet *charset = nullptr;
    FcPatternGetCharSet(aFont, FC_CHARSET, 0, &charset);
    return charset && FcCharSetHasChar(charset, aCh);
}

bool
gfxFontconfigFontEntry::TestCharacterMap(uint32_t aCh)
{
    // For user fonts, or for fonts bundled with the app (which might include
    // color/svg glyphs where the default glyphs may be blank, and thus confuse
    // fontconfig/freetype's char map checking), we instead check the cmap
    // directly for character coverage.
    if (mIgnoreFcCharmap) {
        // If it does not actually have a cmap, switch our strategy to use
        // fontconfig's charmap after all (except for data fonts, which must
        // always have a cmap to have passed OTS validation).
        if (!mIsDataUserFont && !HasFontTable(TRUETYPE_TAG('c','m','a','p'))) {
            mIgnoreFcCharmap = false;
            // ...and continue with HasChar() below.
        } else {
            return gfxFontEntry::TestCharacterMap(aCh);
        }
    }
    // otherwise (for system fonts), use the charmap in the pattern
    return HasChar(mFontPattern, aCh);
}

hb_blob_t*
gfxFontconfigFontEntry::GetFontTable(uint32_t aTableTag)
{
    // for data fonts, read directly from the font data
    if (mFontData) {
        return gfxFontUtils::GetTableFromFontData(mFontData, aTableTag);
    }

    return gfxFontEntry::GetFontTable(aTableTag);
}

void
gfxFontconfigFontEntry::MaybeReleaseFTFace()
{
    // don't release if either HB or Gr face still exists
    if (mHBFace || mGrFace) {
        return;
    }
    // only close out FT_Face for system fonts, not for data fonts
    if (!mIsDataUserFont) {
        if (mFTFace) {
            if (mMMVar) {
                if (sDoneVar) {
                    (*sDoneVar)(mFTFace->glyph->library, mMMVar);
                } else {
                    free(mMMVar);
                }
                mMMVar = nullptr;
            }
            Factory::ReleaseFTFace(mFTFace);
            mFTFace = nullptr;
        }
        mFTFaceInitialized = false;
    }
}

void
gfxFontconfigFontEntry::ForgetHBFace()
{
    gfxFontEntry::ForgetHBFace();
    MaybeReleaseFTFace();
}

void
gfxFontconfigFontEntry::ReleaseGrFace(gr_face* aFace)
{
    gfxFontEntry::ReleaseGrFace(aFace);
    MaybeReleaseFTFace();
}

double
gfxFontconfigFontEntry::GetAspect()
{
    if (mAspect != 0.0) {
        return mAspect;
    }

    // try to compute aspect from OS/2 metrics if available
    AutoTable os2Table(this, TRUETYPE_TAG('O','S','/','2'));
    if (os2Table) {
        uint16_t upem = UnitsPerEm();
        if (upem != kInvalidUPEM) {
            uint32_t len;
            auto os2 = reinterpret_cast<const OS2Table*>
                (hb_blob_get_data(os2Table, &len));
            if (uint16_t(os2->version) >= 2) {
                if (len >= offsetof(OS2Table, sxHeight) + sizeof(int16_t) &&
                    int16_t(os2->sxHeight) > 0.1 * upem) {
                    mAspect = double(int16_t(os2->sxHeight)) / upem;
                    return mAspect;
                }
            }
        }
    }

    // default to aspect = 0.5 if the code below fails
    mAspect = 0.5;

    // create a font to calculate x-height / em-height
    gfxFontStyle s;
    s.size = 100.0; // pick large size to avoid possible hinting artifacts
    RefPtr<gfxFont> font = FindOrMakeFont(&s);
    if (font) {
        const gfxFont::Metrics& metrics =
            font->GetMetrics(gfxFont::eHorizontal);

        // The factor of 0.1 ensures that xHeight is sane so fonts don't
        // become huge.  Strictly ">" ensures that xHeight and emHeight are
        // not both zero.
        if (metrics.xHeight > 0.1 * metrics.emHeight) {
            mAspect = metrics.xHeight / metrics.emHeight;
        }
    }

    return mAspect;
}

static void
PrepareFontOptions(FcPattern* aPattern,
                   cairo_font_options_t* aFontOptions)
{
    NS_ASSERTION(aFontOptions, "null font options passed to PrepareFontOptions");

    // xxx - taken from the gfxFontconfigFonts code, needs to be reviewed

    FcBool printing;
    if (FcPatternGetBool(aPattern, PRINTING_FC_PROPERTY, 0, &printing) !=
            FcResultMatch) {
        printing = FcFalse;
    }

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
    //
    // The one option not recorded in the pattern is hint_metrics, which will
    // affect glyph metrics.  The default behaves as CAIRO_HINT_METRICS_ON.
    // We should be considering the font_options of the surface on which this
    // font will be used, but currently we don't have different gfxFonts for
    // different surface font_options, so we'll create a font suitable for the
    // Screen. Image and xlib surfaces default to CAIRO_HINT_METRICS_ON.
    if (printing) {
        cairo_font_options_set_hint_metrics(aFontOptions, CAIRO_HINT_METRICS_OFF);
    } else {
        cairo_font_options_set_hint_metrics(aFontOptions, CAIRO_HINT_METRICS_ON);
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
        int fc_hintstyle;
        if (FcPatternGetInteger(aPattern, FC_HINT_STYLE,
                                0, &fc_hintstyle) != FcResultMatch) {
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
    }
    cairo_font_options_set_hint_style(aFontOptions, hint_style);

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
            MOZ_FALLTHROUGH;
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
    cairo_font_options_set_subpixel_order(aFontOptions, subpixel_order);

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
    cairo_font_options_set_antialias(aFontOptions, antialias);
}

static void
ReleaseFTUserFontData(void* aData)
{
  static_cast<FTUserFontData*>(aData)->Release();
}

cairo_scaled_font_t*
gfxFontconfigFontEntry::CreateScaledFont(FcPattern* aRenderPattern,
                                         gfxFloat aAdjustedSize,
                                         const gfxFontStyle *aStyle)
{
    if (aStyle->NeedsSyntheticBold(this)) {
        FcPatternAddBool(aRenderPattern, FC_EMBOLDEN, FcTrue);
    }

    // will synthetic oblique be applied using a transform?
    bool needsOblique = IsUpright() &&
                        aStyle->style != FontSlantStyle::Normal() &&
                        aStyle->allowSyntheticStyle;

    if (needsOblique) {
        // disable embedded bitmaps (mimics behavior in 90-synthetic.conf)
        FcPatternDel(aRenderPattern, FC_EMBEDDED_BITMAP);
        FcPatternAddBool(aRenderPattern, FC_EMBEDDED_BITMAP, FcFalse);
    }

    AutoTArray<FT_Fixed,8> coords;
    if (HasVariations()) {
        FT_Face ftFace = GetFTFace();
        if (ftFace) {
            AutoTArray<gfxFontVariation,8> settings;
            GetVariationsForStyle(settings, *aStyle);
            gfxFT2FontBase::SetupVarCoords(ftFace, settings, &coords);
        }
    }

    cairo_font_face_t *face =
        cairo_ft_font_face_create_for_pattern(aRenderPattern,
                                              coords.Elements(),
                                              coords.Length());

    if (mFontData) {
        // for data fonts, add the face/data pointer to the cairo font face
        // so that it gets deleted whenever cairo decides
        NS_ASSERTION(mFTFace, "FT_Face is null when setting user data");
        NS_ASSERTION(mUserFontData, "user font data is null when setting user data");
        mUserFontData.get()->AddRef();
        if (cairo_font_face_set_user_data(face,
                                          &sFcFontlistUserFontDataKey,
                                          mUserFontData,
                                          ReleaseFTUserFontData) != CAIRO_STATUS_SUCCESS) {
            NS_WARNING("Failed binding FTUserFontData to Cairo font face");
            mUserFontData.get()->Release();
            cairo_font_face_destroy(face);
            return nullptr;
        }
    }

    cairo_scaled_font_t *scaledFont = nullptr;

    cairo_matrix_t sizeMatrix;
    cairo_matrix_t identityMatrix;

    cairo_matrix_init_scale(&sizeMatrix, aAdjustedSize, aAdjustedSize);
    cairo_matrix_init_identity(&identityMatrix);

    cairo_font_options_t *fontOptions = cairo_font_options_create();
    PrepareFontOptions(aRenderPattern, fontOptions);

    scaledFont = cairo_scaled_font_create(face, &sizeMatrix,
                                          &identityMatrix, fontOptions);
    cairo_font_options_destroy(fontOptions);

    NS_ASSERTION(cairo_scaled_font_status(scaledFont) == CAIRO_STATUS_SUCCESS,
                 "Failed to make scaled font");

    cairo_font_face_destroy(face);

    return scaledFont;
}

#ifdef MOZ_WIDGET_GTK
// defintion included below
static void ApplyGdkScreenFontOptions(FcPattern *aPattern);
#endif

#ifdef MOZ_X11
static bool
GetXftInt(Display* aDisplay, const char* aName, int* aResult)
{
    if (!aDisplay) {
        return false;
    }
    char* value = XGetDefault(aDisplay, "Xft", aName);
    if (!value) {
        return false;
    }
    if (FcNameConstant(const_cast<FcChar8*>(ToFcChar8Ptr(value)), aResult)) {
        return true;
    }
    char* end;
    *aResult = strtol(value, &end, 0);
    if (end != value) {
        return true;
    }
    return false;
}
#endif

static void
PreparePattern(FcPattern* aPattern, bool aIsPrinterFont)
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
    } else if (!gfxPlatform::IsHeadless()) {
#ifdef MOZ_WIDGET_GTK
        ApplyGdkScreenFontOptions(aPattern);

#ifdef MOZ_X11
        FcValue value;
        int lcdfilter;
        if (FcPatternGet(aPattern, FC_LCD_FILTER, 0, &value) == FcResultNoMatch) {
            GdkDisplay* dpy = gdk_display_get_default();
            if (GDK_IS_X11_DISPLAY(dpy) &&
                GetXftInt(GDK_DISPLAY_XDISPLAY(dpy), "lcdfilter", &lcdfilter)) {
                FcPatternAddInteger(aPattern, FC_LCD_FILTER, lcdfilter);
            }
        }
#endif // MOZ_X11
#endif // MOZ_WIDGET_GTK
    }

    FcDefaultSubstitute(aPattern);
}

void
gfxFontconfigFontEntry::UnscaledFontCache::MoveToFront(size_t aIndex) {
    if (aIndex > 0) {
        ThreadSafeWeakPtr<UnscaledFontFontconfig> front =
            Move(mUnscaledFonts[aIndex]);
        for (size_t i = aIndex; i > 0; i--) {
            mUnscaledFonts[i] = Move(mUnscaledFonts[i-1]);
        }
        mUnscaledFonts[0] = Move(front);
    }
}

already_AddRefed<UnscaledFontFontconfig>
gfxFontconfigFontEntry::UnscaledFontCache::Lookup(const char* aFile, uint32_t aIndex) {
    for (size_t i = 0; i < kNumEntries; i++) {
        RefPtr<UnscaledFontFontconfig> entry(mUnscaledFonts[i]);
        if (entry &&
            !strcmp(entry->GetFile(), aFile) &&
            entry->GetIndex() == aIndex) {
            MoveToFront(i);
            return entry.forget();
        }
    }
    return nullptr;
}

static inline gfxFloat
SizeForStyle(gfxFontconfigFontEntry* aEntry, const gfxFontStyle& aStyle)
{
    return aStyle.sizeAdjust >= 0.0 ?
                aStyle.GetAdjustedSize(aEntry->GetAspect()) :
                aStyle.size;
}

static double
ChooseFontSize(gfxFontconfigFontEntry* aEntry,
               const gfxFontStyle& aStyle)
{
    double requestedSize = SizeForStyle(aEntry, aStyle);
    double bestDist = -1.0;
    double bestSize = requestedSize;
    double size;
    int v = 0;
    while (FcPatternGetDouble(aEntry->GetPattern(),
                              FC_PIXEL_SIZE, v, &size) == FcResultMatch) {
        ++v;
        double dist = fabs(size - requestedSize);
        if (bestDist < 0.0 || dist < bestDist) {
            bestDist = dist;
            bestSize = size;
        }
    }
    // If the font has bitmaps but wants to be scaled, then let it scale.
    if (bestSize >= 0.0) {
        FcBool scalable;
        if (FcPatternGetBool(aEntry->GetPattern(),
                             FC_SCALABLE, 0, &scalable) == FcResultMatch &&
            scalable) {
            return requestedSize;
        }
    }
    return bestSize;
}

gfxFont*
gfxFontconfigFontEntry::CreateFontInstance(const gfxFontStyle *aFontStyle)
{
    nsAutoRef<FcPattern> pattern(FcPatternCreate());
    if (!pattern) {
        NS_WARNING("Failed to create Fontconfig pattern for font instance");
        return nullptr;
    }

    double size = ChooseFontSize(this, *aFontStyle);
    FcPatternAddDouble(pattern, FC_PIXEL_SIZE, size);

    FT_Face face = mFTFace;
    FcPattern* fontPattern = mFontPattern;
    if (face && face->face_flags & FT_FACE_FLAG_MULTIPLE_MASTERS) {
        // For variation fonts, we create a new FT_Face and FcPattern here
        // so that variation coordinates from the style can be applied
        // without affecting other font instances created from the same
        // entry (font resource).
        if (mFontData) {
            // For user fonts: create a new FT_Face from the font data, and then
            // make a pattern from that.
            face = Factory::NewFTFaceFromData(nullptr, mFontData, mLength, 0);
            fontPattern = CreatePatternForFace(face);
        } else {
            // For system fonts: create a new FT_Face and store it in a copy of
            // the original mFontPattern.
            fontPattern = FcPatternDuplicate(mFontPattern);
            face = CreateFaceForPattern(fontPattern);
            if (face) {
                FcPatternAddFTFace(fontPattern, FC_FT_FACE, face);
            } else {
                // I don't think CreateFaceForPattern above should ever fail,
                // but just in case let's fall back here.
                face = mFTFace;
            }
        }
    }

    PreparePattern(pattern, aFontStyle->printerFont);
    nsAutoRef<FcPattern> renderPattern
        (FcFontRenderPrepare(nullptr, pattern, fontPattern));
    if (fontPattern != mFontPattern) {
        // Discard temporary pattern used for variation support
        FcPatternDestroy(fontPattern);
    }
    if (!renderPattern) {
        NS_WARNING("Failed to prepare Fontconfig pattern for font instance");
        return nullptr;
    }

    cairo_scaled_font_t* scaledFont =
        CreateScaledFont(renderPattern, size, aFontStyle);

    const FcChar8* file = ToFcChar8Ptr("");
    int index = 0;
    if (!mFontData) {
        if (FcPatternGetString(renderPattern, FC_FILE, 0,
                               const_cast<FcChar8**>(&file)) != FcResultMatch ||
            FcPatternGetInteger(renderPattern, FC_INDEX, 0, &index) != FcResultMatch) {
            NS_WARNING("No file in Fontconfig pattern for font instance");
            return nullptr;
        }
    }

    RefPtr<UnscaledFontFontconfig> unscaledFont =
        mUnscaledFontCache.Lookup(ToCharPtr(file), index);
    if (!unscaledFont) {
        unscaledFont =
            mFontData ?
                new UnscaledFontFontconfig(face) :
                new UnscaledFontFontconfig(ToCharPtr(file), index);
        mUnscaledFontCache.Add(unscaledFont);
    }

    gfxFont* newFont =
        new gfxFontconfigFont(unscaledFont, scaledFont,
                              renderPattern, size,
                              this, aFontStyle);
    cairo_scaled_font_destroy(scaledFont);

    return newFont;
}

FT_Face
gfxFontconfigFontEntry::GetFTFace()
{
    if (!mFTFaceInitialized) {
        mFTFaceInitialized = true;
        mFTFace = CreateFaceForPattern(mFontPattern);
    }
    return mFTFace;
}

bool
gfxFontconfigFontEntry::HasVariations()
{
    if (mHasVariationsInitialized) {
        return mHasVariations;
    }
    mHasVariationsInitialized = true;
    mHasVariations = false;

    if (!gfxPlatform::GetPlatform()->HasVariationFontSupport()) {
        return mHasVariations;
    }

    // For installed fonts, query the fontconfig pattern rather than paying
    // the cost of loading a FT_Face that we otherwise might never need.
    if (!IsUserFont() || IsLocalUserFont()) {
        FcBool variable;
        if ((FcPatternGetBool(mFontPattern, FC_VARIABLE, 0,
                              &variable) == FcResultMatch) && variable) {
            mHasVariations = true;
        }
    } else {
        FT_Face face = GetFTFace();
        if (face) {
            mHasVariations = face->face_flags & FT_FACE_FLAG_MULTIPLE_MASTERS;
        }
    }

    return mHasVariations;
}

FT_MM_Var*
gfxFontconfigFontEntry::GetMMVar()
{
    if (mMMVarInitialized) {
        return mMMVar;
    }
    mMMVarInitialized = true;
    InitializeVarFuncs();
    if (!sGetVar) {
        return nullptr;
    }
    FT_Face face = GetFTFace();
    if (!face) {
        return nullptr;
    }
    if (FT_Err_Ok != (*sGetVar)(face, &mMMVar)) {
        mMMVar = nullptr;
    }
    return mMMVar;
}

void
gfxFontconfigFontEntry::GetVariationAxes(nsTArray<gfxFontVariationAxis>& aAxes)
{
    if (!HasVariations()) {
        return;
    }
    gfxFT2Utils::GetVariationAxes(GetMMVar(), aAxes);
}

void
gfxFontconfigFontEntry::GetVariationInstances(
    nsTArray<gfxFontVariationInstance>& aInstances)
{
    if (!HasVariations()) {
        return;
    }
    gfxFT2Utils::GetVariationInstances(this, GetMMVar(), aInstances);
}

nsresult
gfxFontconfigFontEntry::CopyFontTable(uint32_t aTableTag,
                                      nsTArray<uint8_t>& aBuffer)
{
    NS_ASSERTION(!mIsDataUserFont,
                 "data fonts should be reading tables directly from memory");

    FT_Face face = GetFTFace();
    if (!face) {
        return NS_ERROR_NOT_AVAILABLE;
    }

    FT_ULong length = 0;
    if (FT_Load_Sfnt_Table(face, aTableTag, 0, nullptr, &length) != 0) {
        return NS_ERROR_NOT_AVAILABLE;
    }
    if (!aBuffer.SetLength(length, fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    if (FT_Load_Sfnt_Table(face, aTableTag, 0, aBuffer.Elements(), &length) != 0) {
        aBuffer.Clear();
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

void
gfxFontconfigFontFamily::FindStyleVariations(FontInfoData *aFontInfoData)
{
    if (mHasStyles) {
        return;
    }

    // add font entries for each of the faces
    uint32_t numFonts = mFontPatterns.Length();
    NS_ASSERTION(numFonts, "font family containing no faces!!");
    uint32_t numRegularFaces = 0;
    for (uint32_t i = 0; i < numFonts; i++) {
        FcPattern* face = mFontPatterns[i];

        // figure out the psname/fullname and choose which to use as the facename
        nsAutoString psname, fullname;
        GetFaceNames(face, mName, psname, fullname);
        const nsAutoString& faceName = !psname.IsEmpty() ? psname : fullname;

        gfxFontconfigFontEntry *fontEntry =
            new gfxFontconfigFontEntry(faceName, face, mContainsAppFonts);

        if (gfxPlatform::GetPlatform()->HasVariationFontSupport()) {
            fontEntry->SetupVariationRanges();
        }

        AddFontEntry(fontEntry);

        if (fontEntry->IsNormalStyle()) {
            numRegularFaces++;
        }

        if (LOG_FONTLIST_ENABLED()) {
            nsAutoCString weightString;
            fontEntry->Weight().ToString(weightString);
            nsAutoCString stretchString;
            fontEntry->Stretch().ToString(stretchString);
            nsAutoCString styleString;
            fontEntry->SlantStyle().ToString(styleString);
            LOG_FONTLIST(("(fontlist) added (%s) to family (%s)"
                 " with style: %s weight: %s stretch: %s"
                 " psname: %s fullname: %s",
                 NS_ConvertUTF16toUTF8(fontEntry->Name()).get(),
                 NS_ConvertUTF16toUTF8(Name()).get(),
                 styleString.get(),
                 weightString.get(),
                 stretchString.get(),
                 NS_ConvertUTF16toUTF8(psname).get(),
                 NS_ConvertUTF16toUTF8(fullname).get()));
        }
    }

    // somewhat arbitrary, but define a family with two or more regular
    // faces as a family for which intra-family fallback should be used
    if (numRegularFaces > 1) {
        mCheckForFallbackFaces = true;
    }
    mFaceNamesInitialized = true;
    mFontPatterns.Clear();
    SetHasStyles(true);
}

void
gfxFontconfigFontFamily::AddFontPattern(FcPattern* aFontPattern)
{
    NS_ASSERTION(!mHasStyles,
                 "font patterns must not be added to already enumerated families");

    FcBool outline;
    if (FcPatternGetBool(aFontPattern, FC_OUTLINE, 0, &outline) != FcResultMatch ||
        !outline) {
        mHasNonScalableFaces = true;

        FcBool scalable;
        if (FcPatternGetBool(aFontPattern, FC_SCALABLE, 0, &scalable) == FcResultMatch &&
            scalable) {
            mForceScalable = true;
        }
    }

    nsCountedRef<FcPattern> pattern(aFontPattern);
    mFontPatterns.AppendElement(pattern);
}

static const double kRejectDistance = 10000.0;

// Calculate a distance score representing the size disparity between the
// requested style's size and the font entry's size.
static double
SizeDistance(gfxFontconfigFontEntry* aEntry,
             const gfxFontStyle& aStyle,
             bool aForceScalable)
{
    double requestedSize = SizeForStyle(aEntry, aStyle);
    double bestDist = -1.0;
    double size;
    int v = 0;
    while (FcPatternGetDouble(aEntry->GetPattern(),
                              FC_PIXEL_SIZE, v, &size) == FcResultMatch) {
        ++v;
        double dist = fabs(size - requestedSize);
        if (bestDist < 0.0 || dist < bestDist) {
            bestDist = dist;
        }
    }
    if (bestDist < 0.0) {
        // No size means scalable
        return -1.0;
    } else if (aForceScalable || 5.0 * bestDist < requestedSize) {
        // fontconfig prefers a matching family or lang to pixelsize of bitmap
        // fonts. CSS suggests a tolerance of 20% on pixelsize.
        return bestDist;
    } else {
        // Reject any non-scalable fonts that are not within tolerance.
        return kRejectDistance;
    }
}

void
gfxFontconfigFontFamily::FindAllFontsForStyle(const gfxFontStyle& aFontStyle,
                                              nsTArray<gfxFontEntry*>& aFontEntryList,
                                              bool aIgnoreSizeTolerance)
{
    gfxFontFamily::FindAllFontsForStyle(aFontStyle,
                                        aFontEntryList,
                                        aIgnoreSizeTolerance);

    if (!mHasNonScalableFaces) {
        return;
    }

    // Iterate over the the available fonts while compacting any groups
    // of unscalable fonts with matching styles into a single entry
    // corresponding to the closest available size. If the closest
    // available size is rejected for being outside tolerance, then the
    // entire group will be skipped.
    size_t skipped = 0;
    gfxFontconfigFontEntry* bestEntry = nullptr;
    double bestDist = -1.0;
    for (size_t i = 0; i < aFontEntryList.Length(); i++) {
        gfxFontconfigFontEntry* entry =
            static_cast<gfxFontconfigFontEntry*>(aFontEntryList[i]);
        double dist = SizeDistance(entry, aFontStyle,
                                   mForceScalable || aIgnoreSizeTolerance);
        // If the entry is scalable or has a style that does not match
        // the group of unscalable fonts, then start a new group.
        if (dist < 0.0 ||
            !bestEntry ||
            bestEntry->Stretch() != entry->Stretch() ||
            bestEntry->Weight() != entry->Weight() ||
            bestEntry->SlantStyle() != entry->SlantStyle()) {
            // If the best entry in this group is still outside the tolerance,
            // then skip the entire group.
            if (bestDist >= kRejectDistance) {
                skipped++;
            }
            // Remove any compacted entries from the previous group.
            if (skipped) {
                i -= skipped;
                aFontEntryList.RemoveElementsAt(i, skipped);
                skipped = 0;
            }
            // Mark the start of the new group.
            bestEntry = entry;
            bestDist = dist;
        } else {
            // If this entry more closely matches the requested size than the
            // current best in the group, then take this entry instead.
            if (dist < bestDist) {
                aFontEntryList[i-1-skipped] = entry;
                bestEntry = entry;
                bestDist = dist;
            }
            skipped++;
        }
    }
    // If the best entry in this group is still outside the tolerance,
    // then skip the entire group.
    if (bestDist >= kRejectDistance) {
        skipped++;
    }
    // Remove any compacted entries from the current group.
    if (skipped) {
        aFontEntryList.TruncateLength(aFontEntryList.Length() - skipped);
    }
}

static bool
PatternHasLang(const FcPattern *aPattern, const FcChar8 *aLang)
{
    FcLangSet *langset;

    if (FcPatternGetLangSet(aPattern, FC_LANG, 0, &langset) != FcResultMatch) {
        return false;
    }

    if (FcLangSetHasLang(langset, aLang) != FcLangDifferentLang) {
        return true;
    }
    return false;
}

bool
gfxFontconfigFontFamily::SupportsLangGroup(nsAtom *aLangGroup) const
{
    if (!aLangGroup || aLangGroup == nsGkAtoms::Unicode) {
        return true;
    }

    nsAutoCString fcLang;
    gfxFcPlatformFontList* pfl = gfxFcPlatformFontList::PlatformFontList();
    pfl->GetSampleLangForGroup(aLangGroup, fcLang);
    if (fcLang.IsEmpty()) {
        return true;
    }

    // Before FindStyleVariations has been called, mFontPatterns will contain
    // the font patterns.  Afterward, it'll be empty, but mAvailableFonts
    // will contain the font entries, each of which holds a reference to its
    // pattern.  We only check the first pattern in each list, because support
    // for langs is considered to be consistent across all faces in a family.
    FcPattern* fontPattern;
    if (mFontPatterns.Length()) {
        fontPattern = mFontPatterns[0];
    } else if (mAvailableFonts.Length()) {
        fontPattern = static_cast<gfxFontconfigFontEntry*>
                      (mAvailableFonts[0].get())->GetPattern();
    } else {
        return true;
    }

    // is lang included in the underlying pattern?
    return PatternHasLang(fontPattern, ToFcChar8Ptr(fcLang.get()));
}

/* virtual */
gfxFontconfigFontFamily::~gfxFontconfigFontFamily()
 {
    // Should not be dropped by stylo
    MOZ_ASSERT(NS_IsMainThread());
}

template<typename Func>
void
gfxFontconfigFontFamily::AddFacesToFontList(Func aAddPatternFunc)
{
    if (HasStyles()) {
        for (auto& fe : mAvailableFonts) {
            if (!fe) {
                continue;
            }
            auto fce = static_cast<gfxFontconfigFontEntry*>(fe.get());
            aAddPatternFunc(fce->GetPattern(), mContainsAppFonts);
        }
    } else {
        for (auto& pat : mFontPatterns) {
            aAddPatternFunc(pat, mContainsAppFonts);
        }
    }
}

gfxFontconfigFont::gfxFontconfigFont(const RefPtr<UnscaledFontFontconfig>& aUnscaledFont,
                                     cairo_scaled_font_t *aScaledFont,
                                     FcPattern *aPattern,
                                     gfxFloat aAdjustedSize,
                                     gfxFontEntry *aFontEntry,
                                     const gfxFontStyle *aFontStyle)
    : gfxFT2FontBase(aUnscaledFont, aScaledFont, aFontEntry, aFontStyle)
    , mPattern(aPattern)
{
    mAdjustedSize = aAdjustedSize;
}

gfxFontconfigFont::~gfxFontconfigFont()
{
}

already_AddRefed<ScaledFont>
gfxFontconfigFont::GetScaledFont(mozilla::gfx::DrawTarget *aTarget)
{
    if (!mAzureScaledFont) {
        mAzureScaledFont =
            Factory::CreateScaledFontForFontconfigFont(GetCairoScaledFont(),
                                                       GetPattern(),
                                                       GetUnscaledFont(),
                                                       GetAdjustedSize());
    }

    RefPtr<ScaledFont> scaledFont(mAzureScaledFont);
    return scaledFont.forget();
}

gfxFcPlatformFontList::gfxFcPlatformFontList()
    : mLocalNames(64)
    , mGenericMappings(32)
    , mFcSubstituteCache(64)
    , mLastConfig(nullptr)
    , mAlwaysUseFontconfigGenerics(true)
{
    if (XRE_IsParentProcess()) {
        // if the rescan interval is set, start the timer
        int rescanInterval = FcConfigGetRescanInterval(nullptr);
        if (rescanInterval) {
            mLastConfig = FcConfigGetCurrent();
            NS_NewTimerWithFuncCallback(getter_AddRefs(mCheckFontUpdatesTimer),
                                        CheckFontUpdates,
                                        this,
                                        (rescanInterval + 1) * 1000,
                                        nsITimer::TYPE_REPEATING_SLACK,
                                        "gfxFcPlatformFontList::gfxFcPlatformFontList");
            if (!mCheckFontUpdatesTimer) {
                NS_WARNING("Failure to create font updates timer");
            }
        }
    }

#ifdef MOZ_BUNDLED_FONTS
    mBundledFontsInitialized = false;
#endif
}

gfxFcPlatformFontList::~gfxFcPlatformFontList()
{
    if (mCheckFontUpdatesTimer) {
        mCheckFontUpdatesTimer->Cancel();
        mCheckFontUpdatesTimer = nullptr;
    }
}

void
gfxFcPlatformFontList::AddFontSetFamilies(FcFontSet* aFontSet,
                                          const SandboxPolicy* aPolicy,
                                          bool aAppFonts)
{
    // This iterates over the fonts in a font set and adds in gfxFontFamily
    // objects for each family. Individual gfxFontEntry objects for each face
    // are not created here; the patterns are just stored in the family. When
    // a family is actually used, it will be populated with gfxFontEntry
    // records and the patterns moved to those.

    if (!aFontSet) {
        NS_WARNING("AddFontSetFamilies called with a null font set.");
        return;
    }

    FcChar8* lastFamilyName = (FcChar8*)"";
    RefPtr<gfxFontconfigFontFamily> fontFamily;
    nsAutoString familyName;
    for (int f = 0; f < aFontSet->nfont; f++) {
        FcPattern* pattern = aFontSet->fonts[f];

        // Skip any fonts that aren't readable for us (e.g. due to restrictive
        // file ownership/permissions).
        FcChar8* path;
        if (FcPatternGetString(pattern, FC_FILE, 0, &path) != FcResultMatch) {
            continue;
        }
        if (access(reinterpret_cast<const char*>(path), F_OK | R_OK) != 0) {
            continue;
        }

#ifdef MOZ_CONTENT_SANDBOX
        // Skip any fonts that will be blocked by the content-process sandbox
        // policy.
        if (aPolicy && !(aPolicy->Lookup(reinterpret_cast<const char*>(path)) &
                         SandboxBroker::Perms::MAY_READ)) {
            continue;
        }
#endif

        AddPatternToFontList(pattern, lastFamilyName,
                             familyName, fontFamily, aAppFonts);
    }
}

void
gfxFcPlatformFontList::AddPatternToFontList(FcPattern* aFont,
                                            FcChar8*& aLastFamilyName,
                                            nsAString& aFamilyName,
                                            RefPtr<gfxFontconfigFontFamily>& aFontFamily,
                                            bool aAppFonts)
{
    // get canonical name
    uint32_t cIndex = FindCanonicalNameIndex(aFont, FC_FAMILYLANG);
    FcChar8* canonical = nullptr;
    FcPatternGetString(aFont, FC_FAMILY, cIndex, &canonical);
    if (!canonical) {
        return;
    }

    // same as the last one? no need to add a new family, skip
    if (FcStrCmp(canonical, aLastFamilyName) != 0) {
        aLastFamilyName = canonical;

        // add new family if one doesn't already exist
        aFamilyName.Truncate();
        AppendUTF8toUTF16(ToCharPtr(canonical), aFamilyName);
        nsAutoString keyName(aFamilyName);
        ToLowerCase(keyName);

        aFontFamily = static_cast<gfxFontconfigFontFamily*>
            (mFontFamilies.GetWeak(keyName));
        if (!aFontFamily) {
            aFontFamily = new gfxFontconfigFontFamily(aFamilyName);
            mFontFamilies.Put(keyName, aFontFamily);
        }
        // Record if the family contains fonts from the app font set
        // (in which case we won't rely on fontconfig's charmap, due to
        // bug 1276594).
        if (aAppFonts) {
            aFontFamily->SetFamilyContainsAppFonts(true);
        }

        // Add pointers to other localized family names. Most fonts
        // only have a single name, so the first call to GetString
        // will usually not match
        FcChar8* otherName;
        int n = (cIndex == 0 ? 1 : 0);
        while (FcPatternGetString(aFont, FC_FAMILY, n, &otherName) ==
               FcResultMatch) {
            NS_ConvertUTF8toUTF16 otherFamilyName(ToCharPtr(otherName));
            AddOtherFamilyName(aFontFamily, otherFamilyName);
            n++;
            if (n == int(cIndex)) {
                n++; // skip over canonical name
            }
        }
    }

    MOZ_ASSERT(aFontFamily, "font must belong to a font family");
    aFontFamily->AddFontPattern(aFont);

    // map the psname, fullname ==> font family for local font lookups
    nsAutoString psname, fullname;
    GetFaceNames(aFont, aFamilyName, psname, fullname);
    if (!psname.IsEmpty()) {
        ToLowerCase(psname);
        mLocalNames.Put(psname, aFont);
    }
    if (!fullname.IsEmpty()) {
        ToLowerCase(fullname);
        mLocalNames.Put(fullname, aFont);
    }
}

nsresult
gfxFcPlatformFontList::InitFontListForPlatform()
{
#ifdef MOZ_BUNDLED_FONTS
    ActivateBundledFonts();
#endif

    mLocalNames.Clear();
    mFcSubstituteCache.Clear();

    mAlwaysUseFontconfigGenerics = PrefFontListsUseOnlyGenerics();
    mOtherFamilyNamesInitialized = true;

    if (XRE_IsContentProcess()) {
        // Content process: use the font list passed from the chrome process,
        // because we can't rely on fontconfig in the presence of sandboxing;
        // it may report fonts that we can't actually access.

        FcChar8* lastFamilyName = (FcChar8*)"";
        RefPtr<gfxFontconfigFontFamily> fontFamily;
        nsAutoString familyName;

        // Get font list that was passed during XPCOM startup
        // or in an UpdateFontList message.
        auto& fontList = dom::ContentChild::GetSingleton()->SystemFontList();

        // For fontconfig versions between 2.10.94 and 2.11.1 inclusive,
        // we need to escape any leading space in the charset element,
        // otherwise FcNameParse will fail. :(
        //
        // The bug was introduced on 2013-05-24 by
        //   https://cgit.freedesktop.org/fontconfig/commit/?id=cd9b1033a68816a7acfbba1718ba0aa5888f6ec7
        //   "Bug 64906 - FcNameParse() should ignore leading whitespace in parameters"
        // because ignoring a leading space in the encoded value of charset
        // causes erroneous decoding of the whole element.
        // This first shipped in version 2.10.94, and was eventually fixed as
        // a side-effect of switching to the "human-readable" representation of
        // charsets on 2014-07-03 in
        //   https://cgit.freedesktop.org/fontconfig/commit/?id=e708e97c351d3bc9f7030ef22ac2f007d5114730
        //   "Change charset parse/unparse format to be human readable"
        // (with a followup fix next day) which means a leading space is no
        // longer significant. This fix landed after 2.11.1 had been shipped,
        // so the first version tag without the bug is 2.11.91.
        int fcVersion = FcGetVersion();
        bool fcCharsetParseBug = fcVersion >= 21094 && fcVersion <= 21101;

        for (SystemFontListEntry& fle : fontList) {
            MOZ_ASSERT(fle.type() ==
                       SystemFontListEntry::Type::TFontPatternListEntry);
            FontPatternListEntry& fpe(fle);
            nsCString& patternStr = fpe.pattern();
            if (fcCharsetParseBug) {
                int32_t index = patternStr.Find(":charset= ");
                if (index != kNotFound) {
                    // insert backslash after the =, before the space
                    patternStr.Insert('\\', index + 9);
                }
            }
            FcPattern* pattern =
                FcNameParse((const FcChar8*)patternStr.get());
            AddPatternToFontList(pattern, lastFamilyName, familyName,
                                 fontFamily, fpe.appFontFamily());
            FcPatternDestroy(pattern);
        }

        LOG_FONTLIST(("got font list from chrome process: "
                      "%u faces in %u families",
                      (unsigned)fontList.Length(), mFontFamilies.Count()));

        fontList.Clear();

        return NS_OK;
    }

    mLastConfig = FcConfigGetCurrent();

    UniquePtr<SandboxPolicy> policy;

#ifdef MOZ_CONTENT_SANDBOX
    // If read sandboxing is enabled, create a temporary SandboxPolicy to
    // check font paths; use a fake PID to avoid picking up any PID-specific
    // rules by accident.
    SandboxBrokerPolicyFactory policyFactory;
    if (GetEffectiveContentSandboxLevel() > 2 &&
        !PR_GetEnv("MOZ_DISABLE_CONTENT_SANDBOX")) {
        policy = policyFactory.GetContentPolicy(-1, false);
    }
#endif

    // iterate over available fonts
    FcFontSet* systemFonts = FcConfigGetFonts(nullptr, FcSetSystem);
    AddFontSetFamilies(systemFonts, policy.get(), /* aAppFonts = */ false);

#ifdef MOZ_BUNDLED_FONTS
    FcFontSet* appFonts = FcConfigGetFonts(nullptr, FcSetApplication);
    AddFontSetFamilies(appFonts, policy.get(), /* aAppFonts = */ true);
#endif

    return NS_OK;
}

void
gfxFcPlatformFontList::ReadSystemFontList(
    InfallibleTArray<SystemFontListEntry>* retValue)
{
    // Fontconfig versions below 2.9 drop the FC_FILE element in FcNameUnparse
    // (see https://bugs.freedesktop.org/show_bug.cgi?id=26718), so when using
    // an older version, we manually append it to the unparsed pattern.
    if (FcGetVersion() < 20900) {
        for (auto iter = mFontFamilies.Iter(); !iter.Done(); iter.Next()) {
            auto family =
                static_cast<gfxFontconfigFontFamily*>(iter.Data().get());
            family->AddFacesToFontList([&](FcPattern* aPat, bool aAppFonts) {
                char* s = (char*)FcNameUnparse(aPat);
                nsAutoCString patternStr(s);
                free(s);
                if (FcResultMatch ==
                    FcPatternGetString(aPat, FC_FILE, 0, (FcChar8**)&s)) {
                    patternStr.Append(":file=");
                    patternStr.Append(s);
                }
                retValue->AppendElement(FontPatternListEntry(patternStr,
                                                             aAppFonts));
            });
        }
    } else {
        for (auto iter = mFontFamilies.Iter(); !iter.Done(); iter.Next()) {
            auto family =
                static_cast<gfxFontconfigFontFamily*>(iter.Data().get());
            family->AddFacesToFontList([&](FcPattern* aPat, bool aAppFonts) {
                char* s = (char*)FcNameUnparse(aPat);
                nsDependentCString patternStr(s);
                retValue->AppendElement(FontPatternListEntry(patternStr,
                                                             aAppFonts));
                free(s);
            });
        }
    }
}

// For displaying the fontlist in UI, use explicit call to FcFontList. Using
// FcFontList results in the list containing the localized names as dictated
// by system defaults.
static void
GetSystemFontList(nsTArray<nsString>& aListOfFonts, nsAtom *aLangGroup)
{
    aListOfFonts.Clear();

    nsAutoRef<FcPattern> pat(FcPatternCreate());
    if (!pat) {
        return;
    }

    nsAutoRef<FcObjectSet> os(FcObjectSetBuild(FC_FAMILY, nullptr));
    if (!os) {
        return;
    }

    // add the lang to the pattern
    nsAutoCString fcLang;
    gfxFcPlatformFontList* pfl = gfxFcPlatformFontList::PlatformFontList();
    pfl->GetSampleLangForGroup(aLangGroup, fcLang,
                               /*aForFontEnumerationThread*/ true);
    if (!fcLang.IsEmpty()) {
        FcPatternAddString(pat, FC_LANG, ToFcChar8Ptr(fcLang.get()));
    }

    nsAutoRef<FcFontSet> fs(FcFontList(nullptr, pat, os));
    if (!fs) {
        return;
    }

    for (int i = 0; i < fs->nfont; i++) {
        char *family;

        if (FcPatternGetString(fs->fonts[i], FC_FAMILY, 0,
                               (FcChar8 **) &family) != FcResultMatch)
        {
            continue;
        }

        // Remove duplicates...
        nsAutoString strFamily;
        AppendUTF8toUTF16(family, strFamily);
        if (aListOfFonts.Contains(strFamily)) {
            continue;
        }

        aListOfFonts.AppendElement(strFamily);
    }

    aListOfFonts.Sort();
}

void
gfxFcPlatformFontList::GetFontList(nsAtom *aLangGroup,
                                   const nsACString& aGenericFamily,
                                   nsTArray<nsString>& aListOfFonts)
{
    // Get the list of font family names using fontconfig
    GetSystemFontList(aListOfFonts, aLangGroup);

    // Under Linux, the generics "serif", "sans-serif" and "monospace"
    // are included in the pref fontlist. These map to whatever fontconfig
    // decides they should be for a given language, rather than one of the
    // fonts listed in the prefs font lists (e.g. font.name.*, font.name-list.*)
    bool serif = false, sansSerif = false, monospace = false;
    if (aGenericFamily.IsEmpty())
        serif = sansSerif = monospace = true;
    else if (aGenericFamily.LowerCaseEqualsLiteral("serif"))
        serif = true;
    else if (aGenericFamily.LowerCaseEqualsLiteral("sans-serif"))
        sansSerif = true;
    else if (aGenericFamily.LowerCaseEqualsLiteral("monospace"))
        monospace = true;
    else if (aGenericFamily.LowerCaseEqualsLiteral("cursive") ||
             aGenericFamily.LowerCaseEqualsLiteral("fantasy"))
        serif = sansSerif = true;
    else
        NS_NOTREACHED("unexpected CSS generic font family");

    // The first in the list becomes the default in
    // FontBuilder.readFontSelection() if the preference-selected font is not
    // available, so put system configured defaults first.
    if (monospace)
        aListOfFonts.InsertElementAt(0, NS_LITERAL_STRING("monospace"));
    if (sansSerif)
        aListOfFonts.InsertElementAt(0, NS_LITERAL_STRING("sans-serif"));
    if (serif)
        aListOfFonts.InsertElementAt(0, NS_LITERAL_STRING("serif"));
}

gfxFontFamily*
gfxFcPlatformFontList::GetDefaultFontForPlatform(const gfxFontStyle* aStyle)
{
    // Get the default font by using a fake name to retrieve the first
    // scalable font that fontconfig suggests for the given language.
    PrefFontList* prefFonts =
        FindGenericFamilies(NS_LITERAL_STRING("-moz-default"), aStyle->language);
    NS_ASSERTION(prefFonts, "null list of generic fonts");
    if (prefFonts && !prefFonts->IsEmpty()) {
        return (*prefFonts)[0];
    }
    return nullptr;
}

gfxFontEntry*
gfxFcPlatformFontList::LookupLocalFont(const nsAString& aFontName,
                                       WeightRange aWeightForEntry,
                                       StretchRange aStretchForEntry,
                                       SlantStyleRange aStyleForEntry)
{
    nsAutoString keyName(aFontName);
    ToLowerCase(keyName);

    // if name is not in the global list, done
    FcPattern* fontPattern = mLocalNames.Get(keyName);
    if (!fontPattern) {
        return nullptr;
    }

    return new gfxFontconfigFontEntry(aFontName, fontPattern,
                                      aWeightForEntry,
                                      aStretchForEntry,
                                      aStyleForEntry);
}

gfxFontEntry*
gfxFcPlatformFontList::MakePlatformFont(const nsAString& aFontName,
                                        WeightRange aWeightForEntry,
                                        StretchRange aStretchForEntry,
                                        SlantStyleRange aStyleForEntry,
                                        const uint8_t* aFontData,
                                        uint32_t aLength)
{
    FT_Face face = Factory::NewFTFaceFromData(nullptr, aFontData, aLength, 0);
    if (!face) {
        free((void*)aFontData);
        return nullptr;
    }
    if (FT_Err_Ok != FT_Select_Charmap(face, FT_ENCODING_UNICODE)) {
        Factory::ReleaseFTFace(face);
        free((void*)aFontData);
        return nullptr;
    }

    return new gfxFontconfigFontEntry(aFontName,
                                      aWeightForEntry,
                                      aStretchForEntry,
                                      aStyleForEntry,
                                      aFontData, aLength, face);
}

bool
gfxFcPlatformFontList::FindAndAddFamilies(const nsAString& aFamily,
                                          nsTArray<gfxFontFamily*>* aOutput,
                                          FindFamiliesFlags aFlags,
                                          gfxFontStyle* aStyle,
                                          gfxFloat aDevToCssSize)
{
    nsAutoString familyName(aFamily);
    ToLowerCase(familyName);
    nsAtom* language = (aStyle ? aStyle->language.get() : nullptr);

    // deprecated generic names are explicitly converted to standard generics
    bool isDeprecatedGeneric = false;
    if (familyName.EqualsLiteral("sans") ||
        familyName.EqualsLiteral("sans serif")) {
        familyName.AssignLiteral("sans-serif");
        isDeprecatedGeneric = true;
    } else if (familyName.EqualsLiteral("mono")) {
        familyName.AssignLiteral("monospace");
        isDeprecatedGeneric = true;
    }

    // fontconfig generics? use fontconfig to determine the family for lang
    if (isDeprecatedGeneric ||
        mozilla::FontFamilyName::Convert(familyName).IsGeneric()) {
        PrefFontList* prefFonts = FindGenericFamilies(familyName, language);
        if (prefFonts && !prefFonts->IsEmpty()) {
            aOutput->AppendElements(*prefFonts);
            return true;
        }
        return false;
    }

    // fontconfig allows conditional substitutions in such a way that it's
    // difficult to distinguish an explicit substitution from other suggested
    // choices. To sniff out explicit substitutions, compare the substitutions
    // for "font, -moz-sentinel" to "-moz-sentinel" to sniff out the
    // substitutions
    //
    // Example:
    //
    //   serif ==> DejaVu Serif, ...
    //   Helvetica, serif ==> Helvetica, TeX Gyre Heros, Nimbus Sans L, DejaVu Serif
    //
    // In this case fontconfig is including Tex Gyre Heros and
    // Nimbus Sans L as alternatives for Helvetica.

    // Because the FcConfigSubstitute call is quite expensive, we cache the
    // actual font families found via this process. So check the cache first:
    NS_ConvertUTF16toUTF8 familyToFind(familyName);
    AutoTArray<gfxFontFamily*,10> cachedFamilies;
    if (mFcSubstituteCache.Get(familyToFind, &cachedFamilies)) {
        if (cachedFamilies.IsEmpty()) {
            return false;
        }
        aOutput->AppendElements(cachedFamilies);
        return true;
    }

    // It wasn't in the cache, so we need to ask fontconfig...
    const FcChar8* kSentinelName = ToFcChar8Ptr("-moz-sentinel");
    FcChar8* sentinelFirstFamily = nullptr;
    nsAutoRef<FcPattern> sentinelSubst(FcPatternCreate());
    FcPatternAddString(sentinelSubst, FC_FAMILY, kSentinelName);
    FcConfigSubstitute(nullptr, sentinelSubst, FcMatchPattern);
    FcPatternGetString(sentinelSubst, FC_FAMILY, 0, &sentinelFirstFamily);

    // substitutions for font, -moz-sentinel pattern
    nsAutoRef<FcPattern> fontWithSentinel(FcPatternCreate());
    FcPatternAddString(fontWithSentinel, FC_FAMILY,
                       ToFcChar8Ptr(familyToFind.get()));
    FcPatternAddString(fontWithSentinel, FC_FAMILY, kSentinelName);
    FcConfigSubstitute(nullptr, fontWithSentinel, FcMatchPattern);

    // Add all font family matches until reaching the sentinel.
    FcChar8* substName = nullptr;
    for (int i = 0;
         FcPatternGetString(fontWithSentinel, FC_FAMILY,
                            i, &substName) == FcResultMatch;
         i++)
    {
        NS_ConvertUTF8toUTF16 subst(ToCharPtr(substName));
        if (sentinelFirstFamily &&
            FcStrCmp(substName, sentinelFirstFamily) == 0) {
            break;
        }
        gfxPlatformFontList::FindAndAddFamilies(subst,
                                                &cachedFamilies,
                                                aFlags);
    }

    // Cache the resulting list, so we don't have to do this again.
    mFcSubstituteCache.Put(familyToFind, cachedFamilies);

    if (cachedFamilies.IsEmpty()) {
        return false;
    }
    aOutput->AppendElements(cachedFamilies);
    return true;
}

bool
gfxFcPlatformFontList::GetStandardFamilyName(const nsAString& aFontName,
                                             nsAString& aFamilyName)
{
    aFamilyName.Truncate();

    // The fontconfig list of fonts includes generic family names in the
    // font list. For these, just use the generic name.
    if (aFontName.EqualsLiteral("serif") ||
        aFontName.EqualsLiteral("sans-serif") ||
        aFontName.EqualsLiteral("monospace")) {
        aFamilyName.Assign(aFontName);
        return true;
    }

    nsAutoRef<FcPattern> pat(FcPatternCreate());
    if (!pat) {
        return true;
    }

    nsAutoRef<FcObjectSet> os(FcObjectSetBuild(FC_FAMILY, nullptr));
    if (!os) {
        return true;
    }

    // add the family name to the pattern
    NS_ConvertUTF16toUTF8 familyName(aFontName);
    FcPatternAddString(pat, FC_FAMILY, ToFcChar8Ptr(familyName.get()));

    nsAutoRef<FcFontSet> givenFS(FcFontList(nullptr, pat, os));
    if (!givenFS) {
        return true;
    }

    // See if there is a font face with first family equal to the given family
    // (needs to be in sync with names coming from GetFontList())
    nsTArray<nsCString> candidates;
    for (int i = 0; i < givenFS->nfont; i++) {
        char* firstFamily;

        if (FcPatternGetString(givenFS->fonts[i], FC_FAMILY, 0,
                               (FcChar8 **) &firstFamily) != FcResultMatch)
        {
            continue;
        }

        nsDependentCString first(firstFamily);
        if (!candidates.Contains(first)) {
            candidates.AppendElement(first);

            if (familyName.Equals(first)) {
                aFamilyName.Assign(aFontName);
                return true;
            }
        }
    }

    // Because fontconfig conflates different family name types, need to
    // double check that the candidate name is not simply a different
    // name type. For example, if a font with nameID=16 "Minion Pro" and
    // nameID=21 "Minion Pro Caption" exists, calling FcFontList with
    // family="Minion Pro" will return a set of patterns some of which
    // will have a first family of "Minion Pro Caption". Ignore these
    // patterns and use the first candidate that maps to a font set with
    // the same number of faces and an identical set of patterns.
    for (uint32_t j = 0; j < candidates.Length(); ++j) {
        FcPatternDel(pat, FC_FAMILY);
        FcPatternAddString(pat, FC_FAMILY, (FcChar8 *)candidates[j].get());

        nsAutoRef<FcFontSet> candidateFS(FcFontList(nullptr, pat, os));
        if (!candidateFS) {
            return true;
        }

        if (candidateFS->nfont != givenFS->nfont) {
            continue;
        }

        bool equal = true;
        for (int i = 0; i < givenFS->nfont; ++i) {
            if (!FcPatternEqual(candidateFS->fonts[i], givenFS->fonts[i])) {
                equal = false;
                break;
            }
        }
        if (equal) {
            AppendUTF8toUTF16(candidates[j], aFamilyName);
            return true;
        }
    }

    // didn't find localized name, leave family name blank
    return true;
}

void
gfxFcPlatformFontList::AddGenericFonts(mozilla::FontFamilyType aGenericType,
                                       nsAtom* aLanguage,
                                       nsTArray<gfxFontFamily*>& aFamilyList)
{
    bool usePrefFontList = false;

    // treat -moz-fixed as monospace
    if (aGenericType == eFamily_moz_fixed) {
        aGenericType = eFamily_monospace;
    }

    const char* generic = GetGenericName(aGenericType);
    NS_ASSERTION(generic, "weird generic font type");
    if (!generic) {
        return;
    }

    // By default, most font prefs on Linux map to "use fontconfig"
    // keywords. So only need to explicitly lookup font pref if
    // non-default settings exist
    NS_ConvertASCIItoUTF16 genericToLookup(generic);
    if ((!mAlwaysUseFontconfigGenerics && aLanguage) ||
        aLanguage == nsGkAtoms::x_math) {
        nsAtom* langGroup = GetLangGroup(aLanguage);
        nsAutoString fontlistValue;
        Preferences::GetString(NamePref(generic, langGroup).get(),
                               fontlistValue);
        nsresult rv;
        if (fontlistValue.IsEmpty()) {
            // The font name list may have two or more family names as comma
            // separated list.  In such case, not matching with generic font
            // name is fine because if the list prefers specific font, we
            // should try to use the pref with complicated path.
            rv = Preferences::GetString(NameListPref(generic, langGroup).get(),
                                        fontlistValue);
        } else {
            rv = NS_OK;
        }
        if (NS_SUCCEEDED(rv)) {
            if (!fontlistValue.EqualsLiteral("serif") &&
                !fontlistValue.EqualsLiteral("sans-serif") &&
                !fontlistValue.EqualsLiteral("monospace")) {
                usePrefFontList = true;
            } else {
                // serif, sans-serif or monospace was specified
                genericToLookup.Assign(fontlistValue);
            }
        }
    }

    // when pref fonts exist, use standard pref font lookup
    if (usePrefFontList) {
        return gfxPlatformFontList::AddGenericFonts(aGenericType,
                                                    aLanguage,
                                                    aFamilyList);
    }

    PrefFontList* prefFonts = FindGenericFamilies(genericToLookup, aLanguage);
    NS_ASSERTION(prefFonts, "null generic font list");
    aFamilyList.AppendElements(*prefFonts);
}

void
gfxFcPlatformFontList::ClearLangGroupPrefFonts()
{
    ClearGenericMappings();
    gfxPlatformFontList::ClearLangGroupPrefFonts();
    mAlwaysUseFontconfigGenerics = PrefFontListsUseOnlyGenerics();
}

/* static */ FT_Library
gfxFcPlatformFontList::GetFTLibrary()
{
    if (!sCairoFTLibrary) {
        // Use cairo's FT_Library so that cairo takes care of shutdown of the
        // FT_Library after it has destroyed its font_faces, and FT_Done_Face
        // has been called on each FT_Face, at least until this bug is fixed:
        // https://bugs.freedesktop.org/show_bug.cgi?id=18857
        //
        // Cairo keeps it's own FT_Library object for creating FT_Face
        // instances, so use that. There's no simple API for accessing this
        // so use the hacky method below of making a font and extracting
        // the library pointer from that.

        FcPattern* pat =
            FcPatternBuild(0, FC_FAMILY, FcTypeString, "serif", (char*)0);
        cairo_font_face_t* face =
            cairo_ft_font_face_create_for_pattern(pat, nullptr, 0);
        FcPatternDestroy(pat);

        cairo_matrix_t identity;
        cairo_matrix_init_identity(&identity);
        cairo_font_options_t* options = cairo_font_options_create();
        cairo_scaled_font_t* sf =
            cairo_scaled_font_create(face, &identity, &identity, options);
        cairo_font_options_destroy(options);
        cairo_font_face_destroy(face);

        FT_Face ft = cairo_ft_scaled_font_lock_face(sf);

        sCairoFTLibrary = ft->glyph->library;

        cairo_ft_scaled_font_unlock_face(sf);
        cairo_scaled_font_destroy(sf);
    }

    return sCairoFTLibrary;
}

gfxPlatformFontList::PrefFontList*
gfxFcPlatformFontList::FindGenericFamilies(const nsAString& aGeneric,
                                           nsAtom* aLanguage)
{
    // set up name
    NS_ConvertUTF16toUTF8 generic(aGeneric);

    nsAutoCString fcLang;
    GetSampleLangForGroup(aLanguage, fcLang);
    ToLowerCase(fcLang);

    nsAutoCString genericLang(generic);
    if (fcLang.Length() > 0) {
        genericLang.Append('-');
    }
    genericLang.Append(fcLang);

    // try to get the family from the cache
    PrefFontList* prefFonts = mGenericMappings.Get(genericLang);
    if (prefFonts) {
        return prefFonts;
    }

    // if not found, ask fontconfig to pick the appropriate font
    nsAutoRef<FcPattern> genericPattern(FcPatternCreate());
    FcPatternAddString(genericPattern, FC_FAMILY,
                       ToFcChar8Ptr(generic.get()));

    // -- prefer scalable fonts
    FcPatternAddBool(genericPattern, FC_SCALABLE, FcTrue);

    // -- add the lang to the pattern
    if (!fcLang.IsEmpty()) {
        FcPatternAddString(genericPattern, FC_LANG,
                           ToFcChar8Ptr(fcLang.get()));
    }

    // -- perform substitutions
    FcConfigSubstitute(nullptr, genericPattern, FcMatchPattern);
    FcDefaultSubstitute(genericPattern);

    // -- sort to get the closest matches
    FcResult result;
    nsAutoRef<FcFontSet> faces(FcFontSort(nullptr, genericPattern, FcFalse,
                                          nullptr, &result));

    if (!faces) {
      return nullptr;
    }

    // -- select the fonts to be used for the generic
    prefFonts = new PrefFontList; // can be empty but in practice won't happen
    uint32_t limit = gfxPlatformGtk::GetPlatform()->MaxGenericSubstitions();
    bool foundFontWithLang = false;
    for (int i = 0; i < faces->nfont; i++) {
        FcPattern* font = faces->fonts[i];
        FcChar8* mappedGeneric = nullptr;

        FcPatternGetString(font, FC_FAMILY, 0, &mappedGeneric);
        if (mappedGeneric) {
            NS_ConvertUTF8toUTF16 mappedGenericName(ToCharPtr(mappedGeneric));
            AutoTArray<gfxFontFamily*,1> genericFamilies;
            if (gfxPlatformFontList::FindAndAddFamilies(mappedGenericName,
                                                        &genericFamilies,
                                                        FindFamiliesFlags(0))) {
                MOZ_ASSERT(genericFamilies.Length() == 1,
                           "expected a single family");
                if (!prefFonts->Contains(genericFamilies[0])) {
                    prefFonts->AppendElement(genericFamilies[0]);
                    bool foundLang =
                        !fcLang.IsEmpty() &&
                        PatternHasLang(font, ToFcChar8Ptr(fcLang.get()));
                    foundFontWithLang = foundFontWithLang || foundLang;
                    // check to see if the list is full
                    if (prefFonts->Length() >= limit) {
                        break;
                    }
                }
            }
        }
    }

    // if no font in the list matches the lang, trim all but the first one
    if (!prefFonts->IsEmpty() && !foundFontWithLang) {
        prefFonts->TruncateLength(1);
    }

    mGenericMappings.Put(genericLang, prefFonts);
    return prefFonts;
}

bool
gfxFcPlatformFontList::PrefFontListsUseOnlyGenerics()
{
    static const char kFontNamePrefix[] = "font.name.";

    bool prefFontsUseOnlyGenerics = true;
    uint32_t count;
    char** names;
    nsresult rv = Preferences::GetRootBranch()->
        GetChildList(kFontNamePrefix, &count, &names);
    if (NS_SUCCEEDED(rv) && count) {
        for (size_t i = 0; i < count; i++) {
            // Check whether all font.name prefs map to generic keywords
            // and that the pref name and keyword match.
            //   Ex: font.name.serif.ar ==> "serif" (ok)
            //   Ex: font.name.serif.ar ==> "monospace" (return false)
            //   Ex: font.name.serif.ar ==> "DejaVu Serif" (return false)
            //   Ex: font.name.serif.ar ==> "" and
            //       font.name-list.serif.ar ==> "serif" (ok)
            //   Ex: font.name.serif.ar ==> "" and
            //       font.name-list.serif.ar ==> "Something, serif"
            //                                           (return false)

            nsDependentCString prefName(names[i] +
                                        ArrayLength(kFontNamePrefix) - 1);
            nsCCharSeparatedTokenizer tokenizer(prefName, '.');
            const nsDependentCSubstring& generic = tokenizer.nextToken();
            const nsDependentCSubstring& langGroup = tokenizer.nextToken();
            nsAutoCString fontPrefValue;
            Preferences::GetCString(names[i], fontPrefValue);
            if (fontPrefValue.IsEmpty()) {
                // The font name list may have two or more family names as comma
                // separated list.  In such case, not matching with generic font
                // name is fine because if the list prefers specific font, this
                // should return false.
                Preferences::GetCString(NameListPref(generic, langGroup).get(),
                                        fontPrefValue);
            }

            if (!langGroup.EqualsLiteral("x-math") &&
                !generic.Equals(fontPrefValue)) {
                prefFontsUseOnlyGenerics = false;
                break;
            }
        }
        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(count, names);
    }
    return prefFontsUseOnlyGenerics;
}

/* static */ void
gfxFcPlatformFontList::CheckFontUpdates(nsITimer *aTimer, void *aThis)
{
    // A content process is not supposed to check this directly;
    // it will be notified by the parent when the font list changes.
    MOZ_ASSERT(XRE_IsParentProcess());

    // check for font updates
    FcInitBringUptoDate();

    // update fontlist if current config changed
    gfxFcPlatformFontList *pfl = static_cast<gfxFcPlatformFontList*>(aThis);
    FcConfig* current = FcConfigGetCurrent();
    if (current != pfl->GetLastConfig()) {
        pfl->UpdateFontList();
        pfl->ForceGlobalReflow();

        mozilla::dom::ContentParent::NotifyUpdatedFonts();
    }
}

gfxFontFamily*
gfxFcPlatformFontList::CreateFontFamily(const nsAString& aName) const
{
    return new gfxFontconfigFontFamily(aName);
}

// mapping of moz lang groups ==> default lang
struct MozLangGroupData {
    nsAtom* const& mozLangGroup;
    const char *defaultLang;
};

const MozLangGroupData MozLangGroups[] = {
    { nsGkAtoms::x_western,      "en" },
    { nsGkAtoms::x_cyrillic,     "ru" },
    { nsGkAtoms::x_devanagari,   "hi" },
    { nsGkAtoms::x_tamil,        "ta" },
    { nsGkAtoms::x_armn,         "hy" },
    { nsGkAtoms::x_beng,         "bn" },
    { nsGkAtoms::x_cans,         "iu" },
    { nsGkAtoms::x_ethi,         "am" },
    { nsGkAtoms::x_geor,         "ka" },
    { nsGkAtoms::x_gujr,         "gu" },
    { nsGkAtoms::x_guru,         "pa" },
    { nsGkAtoms::x_khmr,         "km" },
    { nsGkAtoms::x_knda,         "kn" },
    { nsGkAtoms::x_mlym,         "ml" },
    { nsGkAtoms::x_orya,         "or" },
    { nsGkAtoms::x_sinh,         "si" },
    { nsGkAtoms::x_tamil,        "ta" },
    { nsGkAtoms::x_telu,         "te" },
    { nsGkAtoms::x_tibt,         "bo" },
    { nsGkAtoms::Unicode,        0    }
};

bool
gfxFcPlatformFontList::TryLangForGroup(const nsACString& aOSLang,
                                       nsAtom* aLangGroup,
                                       nsACString& aFcLang,
                                       bool aForFontEnumerationThread)
{
    // Truncate at '.' or '@' from aOSLang, and convert '_' to '-'.
    // aOSLang is in the form "language[_territory][.codeset][@modifier]".
    // fontconfig takes languages in the form "language-territory".
    // nsLanguageAtomService takes languages in the form language-subtag,
    // where subtag may be a territory.  fontconfig and nsLanguageAtomService
    // handle case-conversion for us.
    const char *pos, *end;
    aOSLang.BeginReading(pos);
    aOSLang.EndReading(end);
    aFcLang.Truncate();
    while (pos < end) {
        switch (*pos) {
            case '.':
            case '@':
                end = pos;
                break;
            case '_':
                aFcLang.Append('-');
                break;
            default:
                aFcLang.Append(*pos);
        }
        ++pos;
    }

    if (!aForFontEnumerationThread) {
        nsAtom *atom = mLangService->LookupLanguage(aFcLang);
        return atom == aLangGroup;
    }

    // If we were called by the font enumeration thread, we can't use
    // mLangService->LookupLanguage because it is not thread-safe.
    // Use GetUncachedLanguageGroup to avoid unsafe access to the lang-group
    // mapping cache hashtable.
    nsAutoCString lowered(aFcLang);
    ToLowerCase(lowered);
    RefPtr<nsAtom> lang = NS_Atomize(lowered);
    RefPtr<nsAtom> group = mLangService->GetUncachedLanguageGroup(lang);
    return group.get() == aLangGroup;
}

void
gfxFcPlatformFontList::GetSampleLangForGroup(nsAtom* aLanguage,
                                             nsACString& aLangStr,
                                             bool aForFontEnumerationThread)
{
    aLangStr.Truncate();
    if (!aLanguage) {
        return;
    }

    // set up lang string
    const MozLangGroupData *mozLangGroup = nullptr;

    // -- look it up in the list of moz lang groups
    for (unsigned int i = 0; i < ArrayLength(MozLangGroups); ++i) {
        if (aLanguage == MozLangGroups[i].mozLangGroup) {
            mozLangGroup = &MozLangGroups[i];
            break;
        }
    }

    // -- not a mozilla lang group? Just return the BCP47 string
    //    representation of the lang group
    if (!mozLangGroup) {
        // Not a special mozilla language group.
        // Use aLanguage as a language code.
        aLanguage->ToUTF8String(aLangStr);
        return;
    }

    // -- check the environment for the user's preferred language that
    //    corresponds to this mozilla lang group.
    const char *languages = getenv("LANGUAGE");
    if (languages) {
        const char separator = ':';

        for (const char *pos = languages; true; ++pos) {
            if (*pos == '\0' || *pos == separator) {
                if (languages < pos &&
                    TryLangForGroup(Substring(languages, pos),
                                    aLanguage, aLangStr,
                                    aForFontEnumerationThread)) {
                    return;
                }

                if (*pos == '\0') {
                    break;
                }

                languages = pos + 1;
            }
        }
    }
    const char *ctype = setlocale(LC_CTYPE, nullptr);
    if (ctype &&
        TryLangForGroup(nsDependentCString(ctype), aLanguage, aLangStr,
                        aForFontEnumerationThread)) {
        return;
    }

    if (mozLangGroup->defaultLang) {
        aLangStr.Assign(mozLangGroup->defaultLang);
    } else {
        aLangStr.Truncate();
    }
}

#ifdef MOZ_BUNDLED_FONTS
void
gfxFcPlatformFontList::ActivateBundledFonts()
{
    if (!mBundledFontsInitialized) {
        mBundledFontsInitialized = true;
        nsCOMPtr<nsIFile> localDir;
        nsresult rv = NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(localDir));
        if (NS_FAILED(rv)) {
            return;
        }
        if (NS_FAILED(localDir->Append(NS_LITERAL_STRING("fonts")))) {
            return;
        }
        bool isDir;
        if (NS_FAILED(localDir->IsDirectory(&isDir)) || !isDir) {
            return;
        }
        if (NS_FAILED(localDir->GetNativePath(mBundledFontsPath))) {
            return;
        }
    }
    if (!mBundledFontsPath.IsEmpty()) {
        FcConfigAppFontAddDir(nullptr, ToFcChar8Ptr(mBundledFontsPath.get()));
    }
}
#endif

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

#endif // MOZ_WIDGET_GTK

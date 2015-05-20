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
#include "mozilla/TimeStamp.h"
#include "nsGkAtoms.h"
#include "nsILanguageAtomService.h"
#include "nsUnicodeProperties.h"
#include "nsUnicodeRange.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"

#include <fontconfig/fcfreetype.h>

#ifdef MOZ_WIDGET_GTK
#include <gdk/gdk.h>
#endif

using namespace mozilla;
using namespace mozilla::unicode;

#ifndef FC_POSTSCRIPT_NAME
#define FC_POSTSCRIPT_NAME  "postscriptname"      /* String */
#endif

#define PRINTING_FC_PROPERTY "gfx.printing"

#define LOG_FONTLIST(args) PR_LOG(gfxPlatform::GetLog(eGfxLog_fontlist), \
                               PR_LOG_DEBUG, args)
#define LOG_FONTLIST_ENABLED() PR_LOG_TEST( \
                                   gfxPlatform::GetLog(eGfxLog_fontlist), \
                                   PR_LOG_DEBUG)
#define LOG_CMAPDATA_ENABLED() PR_LOG_TEST( \
                                   gfxPlatform::GetLog(eGfxLog_cmapdata), \
                                   PR_LOG_DEBUG)

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

static uint16_t
MapFcWeight(int aFcWeight)
{
    if (aFcWeight <= (FC_WEIGHT_THIN + FC_WEIGHT_EXTRALIGHT) / 2) {
        return 100;
    } else if (aFcWeight <= (FC_WEIGHT_EXTRALIGHT + FC_WEIGHT_LIGHT) / 2) {
        return 200;
    } else if (aFcWeight <= (FC_WEIGHT_LIGHT + FC_WEIGHT_BOOK) / 2) {
        return 300;
    } else if (aFcWeight <= (FC_WEIGHT_REGULAR + FC_WEIGHT_MEDIUM) / 2) {
        // This includes FC_WEIGHT_BOOK
        return 400;
    } else if (aFcWeight <= (FC_WEIGHT_MEDIUM + FC_WEIGHT_DEMIBOLD) / 2) {
        return 500;
    } else if (aFcWeight <= (FC_WEIGHT_DEMIBOLD + FC_WEIGHT_BOLD) / 2) {
        return 600;
    } else if (aFcWeight <= (FC_WEIGHT_BOLD + FC_WEIGHT_EXTRABOLD) / 2) {
        return 700;
    } else if (aFcWeight <= (FC_WEIGHT_EXTRABOLD + FC_WEIGHT_BLACK) / 2) {
        return 800;
    } else if (aFcWeight <= FC_WEIGHT_BLACK) {
        return 900;
    }

    // including FC_WEIGHT_EXTRABLACK
    return 901;
}

static int16_t
MapFcWidth(int aFcWidth)
{
    if (aFcWidth <= (FC_WIDTH_ULTRACONDENSED + FC_WIDTH_EXTRACONDENSED) / 2) {
        return NS_FONT_STRETCH_ULTRA_CONDENSED;
    }
    if (aFcWidth <= (FC_WIDTH_EXTRACONDENSED + FC_WIDTH_CONDENSED) / 2) {
        return NS_FONT_STRETCH_EXTRA_CONDENSED;
    }
    if (aFcWidth <= (FC_WIDTH_CONDENSED + FC_WIDTH_SEMICONDENSED) / 2) {
        return NS_FONT_STRETCH_CONDENSED;
    }
    if (aFcWidth <= (FC_WIDTH_SEMICONDENSED + FC_WIDTH_NORMAL) / 2) {
        return NS_FONT_STRETCH_SEMI_CONDENSED;
    }
    if (aFcWidth <= (FC_WIDTH_NORMAL + FC_WIDTH_SEMIEXPANDED) / 2) {
        return NS_FONT_STRETCH_NORMAL;
    }
    if (aFcWidth <= (FC_WIDTH_SEMIEXPANDED + FC_WIDTH_EXPANDED) / 2) {
        return NS_FONT_STRETCH_SEMI_EXPANDED;
    }
    if (aFcWidth <= (FC_WIDTH_EXPANDED + FC_WIDTH_EXTRAEXPANDED) / 2) {
        return NS_FONT_STRETCH_EXPANDED;
    }
    if (aFcWidth <= (FC_WIDTH_EXTRAEXPANDED + FC_WIDTH_ULTRAEXPANDED) / 2) {
        return NS_FONT_STRETCH_EXTRA_EXPANDED;
    }
    return NS_FONT_STRETCH_ULTRA_EXPANDED;
}

// mapping of moz lang groups ==> default lang
struct MozLangGroupData {
    nsIAtom* const& mozLangGroup;
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

static void
GetSampleLangForGroup(nsIAtom* aLanguage, nsACString& aLangStr)
{
    aLangStr.Truncate();
    if (aLanguage) {
        // set up lang string
        const MozLangGroupData *mozLangGroup = nullptr;

        // -- look it up in the list of moz lang groups
        for (unsigned int i = 0; i < ArrayLength(MozLangGroups); ++i) {
            if (aLanguage == MozLangGroups[i].mozLangGroup) {
                mozLangGroup = &MozLangGroups[i];
                break;
            }
        }

        // xxx - Is this sufficient? The code in
        // gfxFontconfigUtils::GetSampleLangForGroup has logic for sniffing the
        // LANGUAGE environment to try and map langGroup ==> closest user language
        // but I'm guessing that's not really all that useful. For now, just use
        // the default lang mapping.

        // -- get the BCP47 string representation of the lang group
        if (mozLangGroup) {
            if (mozLangGroup->defaultLang) {
                aLangStr.Assign(mozLangGroup->defaultLang);
            }
        } else {
            // Not a special mozilla language group.
            // Use aLangGroup as a language code.
            aLanguage->ToUTF8String(aLangStr);
        }
    }
}

gfxFontconfigFontEntry::gfxFontconfigFontEntry(const nsAString& aFaceName,
                                               FcPattern* aFontPattern)
        : gfxFontEntry(aFaceName), mFontPattern(aFontPattern),
          mFTFace(nullptr), mFTFaceInitialized(false),
          mAspect(0.0), mFontData(nullptr)
{
    // italic
    int slant;
    if (FcPatternGetInteger(aFontPattern, FC_SLANT, 0, &slant) != FcResultMatch) {
        slant = FC_SLANT_ROMAN;
    }
    if (slant > 0) {
        mItalic = true;
    }

    // weight
    int weight;
    if (FcPatternGetInteger(aFontPattern, FC_WEIGHT, 0, &weight) != FcResultMatch) {
        weight = FC_WEIGHT_REGULAR;
    }
    mWeight = MapFcWeight(weight);

    // width
    int width;
    if (FcPatternGetInteger(aFontPattern, FC_WIDTH, 0, &width) != FcResultMatch) {
        width = FC_WIDTH_NORMAL;
    }
    mStretch = MapFcWidth(width);
}

gfxFontconfigFontEntry::gfxFontconfigFontEntry(const nsAString& aFaceName,
                                               uint16_t aWeight,
                                               int16_t aStretch,
                                               bool aItalic,
                                               const uint8_t *aData,
                                               FT_Face aFace)
    : gfxFontEntry(aFaceName), mFontPattern(FcPatternCreate()),
      mFTFace(aFace), mFTFaceInitialized(true),
      mAspect(0.0), mFontData(aData)
{
    mWeight = aWeight;
    mItalic = aItalic;
    mStretch = aStretch;
    mIsDataUserFont = true;

    // Make a new pattern and store the face in it so that cairo uses
    // that when creating a cairo font face.
    FcPatternAddFTFace(mFontPattern, FC_FT_FACE, mFTFace);

    mUserFontData = new FTUserFontData(mFTFace, mFontData);
}

gfxFontconfigFontEntry::gfxFontconfigFontEntry(const nsAString& aFaceName,
                                               FcPattern* aFontPattern,
                                               uint16_t aWeight,
                                               int16_t aStretch,
                                               bool aItalic)
        : gfxFontEntry(aFaceName), mFontPattern(aFontPattern),
          mFTFace(nullptr), mFTFaceInitialized(false),
          mAspect(0.0), mFontData(nullptr)
{
    mWeight = aWeight;
    mItalic = aItalic;
    mStretch = aStretch;
    mIsLocalUserFont = true;
}

gfxFontconfigFontEntry::~gfxFontconfigFontEntry()
{
}

bool
gfxFontconfigFontEntry::SupportsLangGroup(nsIAtom *aLangGroup) const
{
    if (!aLangGroup || aLangGroup == nsGkAtoms::Unicode) {
        return true;
    }

    nsAutoCString fcLang;
    GetSampleLangForGroup(aLangGroup, fcLang);
    if (fcLang.IsEmpty()) {
        return true;
    }

    // is lang included in the underlying pattern?
    FcLangSet *langset;
    if (FcPatternGetLangSet(mFontPattern, FC_LANG, 0, &langset) != FcResultMatch) {
        return false;
    }

    if (FcLangSetHasLang(langset, (FcChar8 *)fcLang.get()) != FcLangDifferentLang) {
        return true;
    }

    return false;
}

nsresult
gfxFontconfigFontEntry::ReadCMAP(FontInfoData *aFontInfoData)
{
    // attempt this once, if errors occur leave a blank cmap
    if (mCharacterMap) {
        return NS_OK;
    }

    nsRefPtr<gfxCharacterMap> charmap;
    nsresult rv;
    bool symbolFont;

    if (aFontInfoData && (charmap = GetCMAPFromFontInfo(aFontInfoData,
                                                        mUVSOffset,
                                                        symbolFont))) {
        rv = NS_OK;
    } else {
        uint32_t kCMAP = TRUETYPE_TAG('c','m','a','p');
        charmap = new gfxCharacterMap();
        AutoTable cmapTable(this, kCMAP);

        if (cmapTable) {
            bool unicodeFont = false, symbolFont = false; // currently ignored
            uint32_t cmapLen;
            const uint8_t* cmapData =
                reinterpret_cast<const uint8_t*>(hb_blob_get_data(cmapTable,
                                                                  &cmapLen));
            rv = gfxFontUtils::ReadCMAP(cmapData, cmapLen,
                                        *charmap, mUVSOffset,
                                        unicodeFont, symbolFont);
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

    LOG_FONTLIST(("(fontlist-cmap) name: %s, size: %d hash: %8.8x%s\n",
                  NS_ConvertUTF16toUTF8(mName).get(),
                  charmap->SizeOfIncludingThis(moz_malloc_size_of),
                  charmap->mHash, mCharacterMap == charmap ? " new" : ""));
    if (LOG_CMAPDATA_ENABLED()) {
        char prefix[256];
        sprintf(prefix, "(cmapdata) name: %.220s",
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
    // for system fonts, use the charmap in the pattern
    if (!mIsDataUserFont) {
        return HasChar(mFontPattern, aCh);
    }
    return gfxFontEntry::TestCharacterMap(aCh);
}

hb_blob_t*
gfxFontconfigFontEntry::GetFontTable(uint32_t aTableTag)
{
    // for data fonts, read directly from the font data
    if (mFontData) {
        return GetTableFromFontData(mFontData, aTableTag);
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
            FT_Done_Face(mFTFace);
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
    if (mAspect == 0.0) {
        // default to aspect = 0.5
        mAspect = 0.5;

        // create a font to calculate x-height / em-height
        gfxFontStyle s;
        s.size = 100.0; // pick large size to avoid possible hinting artifacts
        nsRefPtr<gfxFont> font = FindOrMakeFont(&s, false);
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
    }
    return mAspect;
}

static void
PrepareFontOptions(FcPattern* aPattern,
                   cairo_font_options_t* aFontOptions)
{
    NS_ASSERTION(aFontOptions, "null font options passed to PrepareFontOptions");

    // xxx - taken from the gfxPangoFonts code, needs to be reviewed

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

cairo_scaled_font_t*
gfxFontconfigFontEntry::CreateScaledFont(FcPattern* aRenderPattern,
                                         const gfxFontStyle *aStyle,
                                         bool aNeedsBold)
{
    if (aNeedsBold) {
        FcPatternAddBool(aRenderPattern, FC_EMBOLDEN, FcTrue);
    }

    // synthetic oblique by skewing via the font matrix
    bool needsOblique = !IsItalic() &&
            (aStyle->style & (NS_FONT_STYLE_ITALIC | NS_FONT_STYLE_OBLIQUE)) &&
            aStyle->allowSyntheticStyle;

    if (needsOblique) {
        // disable embedded bitmaps (mimics behavior in 90-synthetic.conf)
        FcPatternDel(aRenderPattern, FC_EMBEDDED_BITMAP);
        FcPatternAddBool(aRenderPattern, FC_EMBEDDED_BITMAP, FcFalse);
    }

    cairo_font_face_t *face =
        cairo_ft_font_face_create_for_pattern(aRenderPattern);

    if (mFontData) {
        // for data fonts, add the face/data pointer to the cairo font face
        // so that it gets deleted whenever cairo decides
        NS_ASSERTION(mFTFace, "FT_Face is null when setting user data");
        NS_ASSERTION(mUserFontData, "user font data is null when setting user data");
        cairo_font_face_set_user_data(face,
                                      &sFcFontlistUserFontDataKey,
                                      new FTUserFontDataRef(mUserFontData),
                                      FTUserFontDataRef::Destroy);
    }

    cairo_scaled_font_t *scaledFont = nullptr;

    cairo_matrix_t sizeMatrix;
    cairo_matrix_t identityMatrix;

    double adjustedSize = aStyle->size;
    if (aStyle->sizeAdjust >= 0.0) {
        adjustedSize = aStyle->GetAdjustedSize(GetAspect());
    }
    cairo_matrix_init_scale(&sizeMatrix, adjustedSize, adjustedSize);
    cairo_matrix_init_identity(&identityMatrix);

    if (needsOblique) {
        const double kSkewFactor = OBLIQUE_SKEW_FACTOR;

        cairo_matrix_t style;
        cairo_matrix_init(&style,
                          1,                //xx
                          0,                //yx
                          -1 * kSkewFactor,  //xy
                          1,                //yy
                          0,                //x0
                          0);               //y0
        cairo_matrix_multiply(&sizeMatrix, &sizeMatrix, &style);
    }

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
    } else {
#ifdef MOZ_WIDGET_GTK
       ApplyGdkScreenFontOptions(aPattern);
#endif
    }

    FcDefaultSubstitute(aPattern);
}

gfxFont*
gfxFontconfigFontEntry::CreateFontInstance(const gfxFontStyle *aFontStyle,
                                           bool aNeedsBold)
{
    nsAutoRef<FcPattern> pattern(FcPatternCreate());
    FcPatternAddDouble(pattern, FC_PIXEL_SIZE, aFontStyle->size);

    PreparePattern(pattern, aFontStyle->printerFont);
    nsAutoRef<FcPattern> renderPattern
        (FcFontRenderPrepare(nullptr, pattern, mFontPattern));

    cairo_scaled_font_t* scaledFont =
        CreateScaledFont(renderPattern, aFontStyle, aNeedsBold);
    gfxFont* newFont =
        new gfxFontconfigFont(scaledFont, this, aFontStyle, aNeedsBold);
    cairo_scaled_font_destroy(scaledFont);

    return newFont;
}

nsresult
gfxFontconfigFontEntry::CopyFontTable(uint32_t aTableTag,
                                      FallibleTArray<uint8_t>& aBuffer)
{
    NS_ASSERTION(!mIsDataUserFont,
                 "data fonts should be reading tables directly from memory");

    if (!mFTFaceInitialized) {
        mFTFaceInitialized = true;
        FcChar8 *filename;
        if (FcPatternGetString(mFontPattern, FC_FILE, 0, &filename) != FcResultMatch) {
            return NS_ERROR_FAILURE;
        }
        int index;
        if (FcPatternGetInteger(mFontPattern, FC_INDEX, 0, &index) != FcResultMatch) {
            index = 0; // default to 0 if not found in pattern
        }
        if (FT_New_Face(gfxFcPlatformFontList::GetFTLibrary(),
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
gfxFontconfigFontFamily::FindStyleVariations(FontInfoData *aFontInfoData)
{
    if (mHasStyles) {
        return;
    }

    // add font entries for each of the faces
    gfxPlatformFontList *fp = gfxPlatformFontList::PlatformFontList();
    uint32_t numFonts = mFontPatterns.Length();
    NS_ASSERTION(numFonts, "font family containing no faces!!");
    for (uint32_t i = 0; i < numFonts; i++) {
        FcPattern* face = mFontPatterns[i];

        // figure out the psname/fullname and choose which to use as the facename
        nsAutoString psname, fullname;
        GetFaceNames(face, mName, psname, fullname);
        const nsAutoString& faceName = !psname.IsEmpty() ? psname : fullname;

        gfxFontconfigFontEntry *fontEntry =
            new gfxFontconfigFontEntry(faceName, face);
        AddFontEntry(fontEntry);

        // add entry to local name lists
        if (!psname.IsEmpty()) {
            fp->AddPostscriptName(fontEntry, psname);
        }
        NS_ASSERTION(!fullname.IsEmpty(), "empty font fullname");
        if (!fullname.IsEmpty()) {
            fp->AddFullname(fontEntry, fullname);
        }

        if (LOG_FONTLIST_ENABLED()) {
            LOG_FONTLIST(("(fontlist) added (%s) to family (%s)"
                 " with style: %s weight: %d stretch: %d"
                 " psname: %s fullname: %s",
                 NS_ConvertUTF16toUTF8(fontEntry->Name()).get(),
                 NS_ConvertUTF16toUTF8(Name()).get(),
                 fontEntry->IsItalic() ? "italic" : "normal",
                 fontEntry->Weight(), fontEntry->Stretch(),
                 NS_ConvertUTF16toUTF8(psname).get(),
                 NS_ConvertUTF16toUTF8(fullname).get()));
        }
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

    nsCountedRef<FcPattern> pattern(aFontPattern);
    mFontPatterns.AppendElement(pattern);
}

gfxFontconfigFont::gfxFontconfigFont(cairo_scaled_font_t *aScaledFont,
                                     gfxFontEntry *aFontEntry,
                                     const gfxFontStyle *aFontStyle,
                                     bool aNeedsBold) :
    gfxFT2FontBase(aScaledFont, aFontEntry, aFontStyle)
{
}

gfxFontconfigFont::~gfxFontconfigFont()
{
}

#ifdef USE_SKIA
mozilla::TemporaryRef<mozilla::gfx::GlyphRenderingOptions>
gfxFontconfigFont::GetGlyphRenderingOptions(const TextRunDrawParams* aRunParams)
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

gfxFcPlatformFontList::gfxFcPlatformFontList()
    : mLocalNames(64), mGenericMappings(32), mLastConfig(nullptr)
{
    // if the rescan interval is set, start the timer
    int rescanInterval = FcConfigGetRescanInterval(nullptr);
    if (rescanInterval) {
        mLastConfig = FcConfigGetCurrent();
        mCheckFontUpdatesTimer = do_CreateInstance("@mozilla.org/timer;1");
        if (mCheckFontUpdatesTimer) {
            mCheckFontUpdatesTimer->
                InitWithFuncCallback(CheckFontUpdates, this,
                                     (rescanInterval + 1) * 1000,
                                     nsITimer::TYPE_REPEATING_SLACK);
        } else {
            NS_WARNING("Failure to create font updates timer");
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
gfxFcPlatformFontList::AddFontSetFamilies(FcFontSet* aFontSet)
{
    // This iterates over the fonts in a font set and adds in gfxFontFamily
    // objects for each family. The patterns for individual fonts are not
    // copied here. When a family is actually used, the fonts in the family
    // are enumerated and the patterns copied. Note that we're explicitly
    // excluding non-scalable fonts such as X11 bitmap fonts, which
    // Chrome Skia/Webkit code does also.

    if (!aFontSet) {
        NS_WARNING("AddFontSetFamilies called with a null font set.");
        return;
    }

    FcChar8* lastFamilyName = (FcChar8*)"";
    gfxFontFamily* fontFamily = nullptr;
    nsAutoString familyName;
    for (int f = 0; f < aFontSet->nfont; f++) {
        FcPattern* font = aFontSet->fonts[f];

        // not scalable? skip...
        FcBool scalable;
        if (FcPatternGetBool(font, FC_SCALABLE, 0, &scalable) != FcResultMatch ||
            !scalable) {
            continue;
        }

        // get canonical name
        uint32_t cIndex = FindCanonicalNameIndex(font, FC_FAMILYLANG);
        FcChar8* canonical = nullptr;
        FcPatternGetString(font, FC_FAMILY, cIndex, &canonical);
        if (!canonical) {
            continue;
        }

        // same as the last one? no need to add a new family, skip
        if (FcStrCmp(canonical, lastFamilyName) != 0) {
            lastFamilyName = canonical;

            // add new family if one doesn't already exist
            familyName.Truncate();
            AppendUTF8toUTF16(ToCharPtr(canonical), familyName);
            nsAutoString keyName(familyName);
            ToLowerCase(keyName);

            fontFamily = mFontFamilies.GetWeak(keyName);
            if (!fontFamily) {
                fontFamily = new gfxFontconfigFontFamily(familyName);
                mFontFamilies.Put(keyName, fontFamily);
            }

            // Add pointers to other localized family names. Most fonts
            // only have a single name, so the first call to GetString
            // will usually not match
            FcChar8* otherName;
            int n = (cIndex == 0 ? 1 : 0);
            while (FcPatternGetString(font, FC_FAMILY, n, &otherName) == FcResultMatch) {
                NS_ConvertUTF8toUTF16 otherFamilyName(ToCharPtr(otherName));
                AddOtherFamilyName(fontFamily, otherFamilyName);
                n++;
                if (n == int(cIndex)) {
                    n++; // skip over canonical name
                }
            }
        }

        NS_ASSERTION(fontFamily, "font must belong to a font family");
        gfxFontconfigFontFamily* fcFamily =
            static_cast<gfxFontconfigFontFamily*>(fontFamily);
        fcFamily->AddFontPattern(font);

        // map the psname, fullname ==> font family for local font lookups
        nsAutoString psname, fullname;
        GetFaceNames(font, familyName, psname, fullname);
        if (!psname.IsEmpty()) {
            ToLowerCase(psname);
            mLocalNames.Put(psname, fontFamily);
        }
        if (!fullname.IsEmpty()) {
            ToLowerCase(fullname);
            mLocalNames.Put(fullname, fontFamily);
        }
    }
}

nsresult
gfxFcPlatformFontList::InitFontList()
{
    mLastConfig = FcConfigGetCurrent();

    // reset font lists
    gfxPlatformFontList::InitFontList();

    mLocalNames.Clear();
    mGenericMappings.Clear();

    // iterate over available fonts
    FcFontSet* systemFonts = FcConfigGetFonts(nullptr, FcSetSystem);
    AddFontSetFamilies(systemFonts);

#ifdef MOZ_BUNDLED_FONTS
    ActivateBundledFonts();
    FcFontSet* appFonts = FcConfigGetFonts(nullptr, FcSetApplication);
    AddFontSetFamilies(appFonts);
#endif

    mOtherFamilyNamesInitialized = true;
    return NS_OK;
}

// For displaying the fontlist in UI, use explicit call to FcFontList. Using
// FcFontList results in the list containing the localized names as dictated
// by system defaults.
static void
GetSystemFontList(nsTArray<nsString>& aListOfFonts, nsIAtom *aLangGroup)
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
    GetSampleLangForGroup(aLangGroup, fcLang);
    if (!fcLang.IsEmpty()) {
        FcPatternAddString(pat, FC_LANG, ToFcChar8Ptr(fcLang.get()));
    }

    // ignore size-specific fonts
    FcPatternAddBool(pat, FC_SCALABLE, FcTrue);

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
gfxFcPlatformFontList::GetFontList(nsIAtom *aLangGroup,
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
    // gFontsDialog.readFontSelection() if the preference-selected font is not
    // available, so put system configured defaults first.
    if (monospace)
        aListOfFonts.InsertElementAt(0, NS_LITERAL_STRING("monospace"));
    if (sansSerif)
        aListOfFonts.InsertElementAt(0, NS_LITERAL_STRING("sans-serif"));
    if (serif)
        aListOfFonts.InsertElementAt(0, NS_LITERAL_STRING("serif"));
}

gfxFontFamily*
gfxFcPlatformFontList::GetDefaultFont(const gfxFontStyle* aStyle)
{
    return FindGenericFamily(NS_LITERAL_STRING("serif"), nsGkAtoms::x_western);
}

gfxFontEntry*
gfxFcPlatformFontList::LookupLocalFont(const nsAString& aFontName,
                                       uint16_t aWeight,
                                       int16_t aStretch,
                                       bool aItalic)
{
    gfxFontEntry* lookup;

    // first, lookup in face name lists
    lookup = FindFaceName(aFontName);
    if (!lookup) {
        // if not found, check in global facename ==> family list
        nsAutoString keyName(aFontName);
        ToLowerCase(keyName);
        gfxFontFamily* fontFamily = mLocalNames.GetWeak(keyName);

        // name is not in the global list, done
        if (!fontFamily) {
            return nullptr;
        }

        // name is in global list but family needs enumeration
        fontFamily->FindStyleVariations();

        // facename ==> font entry should now be in the list
        lookup = FindFaceName(aFontName);
        NS_ASSERTION(lookup, "facename to family mapping failure");
        if (!lookup) {
            return nullptr;
        }
    }

    gfxFontconfigFontEntry* fcFontEntry =
        static_cast<gfxFontconfigFontEntry*>(lookup);

    return new gfxFontconfigFontEntry(fcFontEntry->Name(),
                                      fcFontEntry->GetPattern(),
                                      aWeight, aStretch, aItalic);
}

gfxFontEntry*
gfxFcPlatformFontList::MakePlatformFont(const nsAString& aFontName,
                                        uint16_t aWeight,
                                        int16_t aStretch,
                                        bool aItalic,
                                        const uint8_t* aFontData,
                                        uint32_t aLength)
{
    FT_Face face;
    FT_Error error =
        FT_New_Memory_Face(gfxFcPlatformFontList::GetFTLibrary(),
                           aFontData, aLength, 0, &face);
    if (error != FT_Err_Ok) {
        NS_Free((void*)aFontData);
        return nullptr;
    }
    if (FT_Err_Ok != FT_Select_Charmap(face, FT_ENCODING_UNICODE)) {
        FT_Done_Face(face);
        NS_Free((void*)aFontData);
        return nullptr;
    }

    return new gfxFontconfigFontEntry(aFontName, aWeight, aStretch, aItalic,
                                      aFontData, face);
}

gfxFontFamily*
gfxFcPlatformFontList::FindFamily(const nsAString& aFamily,
                                  nsIAtom* aLanguage,
                                  bool aUseSystemFonts)
{
    nsAutoString familyName(aFamily);
    ToLowerCase(familyName);

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
        return FindGenericFamily(familyName, aLanguage);
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

    // substitutions for serif pattern
    const FcChar8* kSentinelName = ToFcChar8Ptr("-moz-sentinel");
    nsAutoRef<FcPattern> sentinelSubst(FcPatternCreate());
    FcPatternAddString(sentinelSubst, FC_FAMILY, kSentinelName);
    FcConfigSubstitute(nullptr, sentinelSubst, FcMatchPattern);
    FcChar8* sentinelFirstFamily = nullptr;
    FcPatternGetString(sentinelSubst, FC_FAMILY, 0, &sentinelFirstFamily);

    // substitutions for font, -moz-sentinel pattern
    nsAutoRef<FcPattern> fontWithSentinel(FcPatternCreate());
    NS_ConvertUTF16toUTF8 familyToFind(familyName);
    FcPatternAddString(fontWithSentinel, FC_FAMILY, ToFcChar8Ptr(familyToFind.get()));
    FcPatternAddString(fontWithSentinel, FC_FAMILY, kSentinelName);
    FcConfigSubstitute(nullptr, fontWithSentinel, FcMatchPattern);

    // iterate through substitutions until hitting the first serif font
    FcChar8* substName = nullptr;
    for (int i = 0;
         FcPatternGetString(fontWithSentinel, FC_FAMILY,
                            i, &substName) == FcResultMatch;
         i++)
    {
        NS_ConvertUTF8toUTF16 subst(ToCharPtr(substName));
        if (sentinelFirstFamily && FcStrCmp(substName, sentinelFirstFamily) == 0) {
            break;
        }
        gfxFontFamily* foundFamily = gfxPlatformFontList::FindFamily(subst);
        if (foundFamily) {
            return foundFamily;
        }
    }

    return nullptr;
}

bool
gfxFcPlatformFontList::GetStandardFamilyName(const nsAString& aFontName,
                                             nsAString& aFamilyName)
{
    // The fontconfig list of fonts includes generic family names in the
    // font list. For these, just use the generic name.
    if (aFontName.EqualsLiteral("serif") ||
        aFontName.EqualsLiteral("sans-serif") ||
        aFontName.EqualsLiteral("monospace")) {
        aFamilyName.Assign(aFontName);
        return true;
    }

    gfxFontFamily *family = FindFamily(aFontName);
    if (family) {
        family->LocalizedName(aFamilyName);
        return true;
    }

    return false;
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

        bool needsBold;
        gfxFontStyle style;
        gfxPlatformFontList* pfl = gfxPlatformFontList::PlatformFontList();
        gfxFontFamily* family = pfl->GetDefaultFont(&style);
        NS_ASSERTION(family, "couldn't find a default font family");
        gfxFontEntry* fe = family->FindFontForStyle(style, needsBold);
        if (!fe) {
            return nullptr;
        }
        nsRefPtr<gfxFont> font = fe->FindOrMakeFont(&style, false);
        if (!font) {
            return nullptr;
        }

        gfxFT2FontBase* ft2Font = reinterpret_cast<gfxFT2FontBase*>(font.get());
        gfxFT2LockedFace face(ft2Font);
        if (!face.get()) {
            return nullptr;
        }

        sCairoFTLibrary = face.get()->glyph->library;
    }

    return sCairoFTLibrary;
}

gfxFontFamily*
gfxFcPlatformFontList::FindGenericFamily(const nsAString& aGeneric,
                                         nsIAtom* aLanguage)
{
    // set up name
    NS_ConvertUTF16toUTF8 generic(aGeneric);

    nsAutoCString fcLang;
    GetSampleLangForGroup(aLanguage, fcLang);

    nsAutoCString genericLang(generic);
    genericLang.Append(fcLang);

    // try to get the family from the cache
    gfxFontFamily *genericFamily = mGenericMappings.GetWeak(genericLang);
    if (genericFamily) {
        return genericFamily;
    }

    // if not found, ask fontconfig to pick the appropriate font
    nsAutoRef<FcPattern> genericPattern(FcPatternCreate());
    FcPatternAddString(genericPattern, FC_FAMILY,
                       ToFcChar8Ptr(generic.get()));

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

    // -- pick the first font for which a font family exists
    for (int i = 0; i < faces->nfont; i++) {
        FcPattern* font = faces->fonts[i];
        FcChar8* mappedGeneric = nullptr;

        // not scalable? skip...
        FcBool scalable;
        if (FcPatternGetBool(font, FC_SCALABLE, 0, &scalable) != FcResultMatch ||
            !scalable) {
            continue;
        }

        FcPatternGetString(font, FC_FAMILY, 0, &mappedGeneric);
        if (mappedGeneric) {
            NS_ConvertUTF8toUTF16 mappedGenericName(ToCharPtr(mappedGeneric));
            genericFamily = gfxPlatformFontList::FindFamily(mappedGenericName);
            if (genericFamily) {
                //printf("generic %s ==> %s\n", genericLang.get(), (const char*)mappedGeneric);
                mGenericMappings.Put(genericLang, genericFamily);
                break;
            }
        }
    }

    return genericFamily;
}

/* static */ void
gfxFcPlatformFontList::CheckFontUpdates(nsITimer *aTimer, void *aThis)
{
    // check for font updates
    FcInitBringUptoDate();

    // update fontlist if current config changed
    gfxFcPlatformFontList *pfl = static_cast<gfxFcPlatformFontList*>(aThis);
    FcConfig* current = FcConfigGetCurrent();
    if (current != pfl->GetLastConfig()) {
        pfl->UpdateFontList();
        pfl->ForceGlobalReflow();
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

#endif // MOZ_WIDGET_GTK2



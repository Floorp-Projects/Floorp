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
#include "mozilla/Preferences.h"
#include "mozilla/Sprintf.h"
#include "mozilla/TimeStamp.h"
#include "nsGkAtoms.h"
#include "nsILanguageAtomService.h"
#include "nsUnicodeProperties.h"
#include "nsUnicodeRange.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsCharSeparatedTokenizer.h"

#include "mozilla/gfx/HelpersCairo.h"

#include <fontconfig/fcfreetype.h>

#ifdef MOZ_WIDGET_GTK
#include <gdk/gdk.h>
#include "gfxPlatformGtk.h"
#endif

#ifdef MOZ_X11
#include "mozilla/X11Util.h"
#endif

using namespace mozilla;
using namespace mozilla::unicode;

#ifndef FC_POSTSCRIPT_NAME
#define FC_POSTSCRIPT_NAME  "postscriptname"      /* String */
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

gfxFontconfigFontEntry::gfxFontconfigFontEntry(const nsAString& aFaceName,
                                               FcPattern* aFontPattern,
                                               bool aIgnoreFcCharmap)
        : gfxFontEntry(aFaceName), mFontPattern(aFontPattern),
          mFTFace(nullptr), mFTFaceInitialized(false),
          mIgnoreFcCharmap(aIgnoreFcCharmap),
          mAspect(0.0), mFontData(nullptr)
{
    // italic
    int slant;
    if (FcPatternGetInteger(aFontPattern, FC_SLANT, 0, &slant) != FcResultMatch) {
        slant = FC_SLANT_ROMAN;
    }
    if (slant == FC_SLANT_OBLIQUE) {
        mStyle = NS_FONT_STYLE_OBLIQUE;
    } else if (slant > 0) {
        mStyle = NS_FONT_STYLE_ITALIC;
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
                                               uint8_t aStyle,
                                               const uint8_t *aData,
                                               FT_Face aFace)
    : gfxFontEntry(aFaceName),
      mFTFace(aFace), mFTFaceInitialized(true),
      mIgnoreFcCharmap(true),
      mAspect(0.0), mFontData(aData)
{
    mWeight = aWeight;
    mStyle = aStyle;
    mStretch = aStretch;
    mIsDataUserFont = true;

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
    mFontPattern = FcFreeTypeQueryFace(mFTFace, ToFcChar8Ptr(""), 0, nullptr);
    // given that we have a FT_Face, not really sure this is possible...
    if (!mFontPattern) {
        mFontPattern = FcPatternCreate();
    }
    FcPatternDel(mFontPattern, FC_FILE);
    FcPatternDel(mFontPattern, FC_INDEX);

    // Make a new pattern and store the face in it so that cairo uses
    // that when creating a cairo font face.
    FcPatternAddFTFace(mFontPattern, FC_FT_FACE, mFTFace);

    mUserFontData = new FTUserFontData(mFTFace, mFontData);
}

gfxFontconfigFontEntry::gfxFontconfigFontEntry(const nsAString& aFaceName,
                                               FcPattern* aFontPattern,
                                               uint16_t aWeight,
                                               int16_t aStretch,
                                               uint8_t aStyle)
        : gfxFontEntry(aFaceName), mFontPattern(aFontPattern),
          mFTFace(nullptr), mFTFaceInitialized(false),
          mAspect(0.0), mFontData(nullptr)
{
    mWeight = aWeight;
    mStyle = aStyle;
    mStretch = aStretch;
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

gfxFontconfigFontEntry::~gfxFontconfigFontEntry()
{
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
gfxFontconfigFontEntry::SupportsLangGroup(nsIAtom *aLangGroup) const
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

    // is lang included in the underlying pattern?
    return PatternHasLang(mFontPattern, ToFcChar8Ptr(fcLang.get()));
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
    bool symbolFont = false; // currently ignored

    if (aFontInfoData && (charmap = GetCMAPFromFontInfo(aFontInfoData,
                                                        mUVSOffset,
                                                        symbolFont))) {
        rv = NS_OK;
    } else {
        uint32_t kCMAP = TRUETYPE_TAG('c','m','a','p');
        charmap = new gfxCharacterMap();
        AutoTable cmapTable(this, kCMAP);

        if (cmapTable) {
            bool unicodeFont = false; // currently ignored
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
        RefPtr<gfxFont> font = FindOrMakeFont(&s, false);
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

cairo_scaled_font_t*
gfxFontconfigFontEntry::CreateScaledFont(FcPattern* aRenderPattern,
                                         const gfxFontStyle *aStyle,
                                         bool aNeedsBold)
{
    if (aNeedsBold) {
        FcPatternAddBool(aRenderPattern, FC_EMBOLDEN, FcTrue);
    }

    // synthetic oblique by skewing via the font matrix
    bool needsOblique = IsUpright() &&
                        aStyle->style != NS_FONT_STYLE_NORMAL &&
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
    } else {
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

gfxFont*
gfxFontconfigFontEntry::CreateFontInstance(const gfxFontStyle *aFontStyle,
                                           bool aNeedsBold)
{
    nsAutoRef<FcPattern> pattern(FcPatternCreate());
    if (!pattern) {
        NS_WARNING("Failed to create Fontconfig pattern for font instance");
        return nullptr;
    }
    FcPatternAddDouble(pattern, FC_PIXEL_SIZE, aFontStyle->size);

    PreparePattern(pattern, aFontStyle->printerFont);
    nsAutoRef<FcPattern> renderPattern
        (FcFontRenderPrepare(nullptr, pattern, mFontPattern));
    if (!renderPattern) {
        NS_WARNING("Failed to prepare Fontconfig pattern for font instance");
        return nullptr;
    }

    cairo_scaled_font_t* scaledFont =
        CreateScaledFont(renderPattern, aFontStyle, aNeedsBold);
    gfxFont* newFont =
        new gfxFontconfigFont(scaledFont, renderPattern, this, aFontStyle, aNeedsBold);
    cairo_scaled_font_destroy(scaledFont);

    return newFont;
}

nsresult
gfxFontconfigFontEntry::CopyFontTable(uint32_t aTableTag,
                                      nsTArray<uint8_t>& aBuffer)
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
    if (!aBuffer.SetLength(length, fallible)) {
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
        AddFontEntry(fontEntry);

        if (fontEntry->IsUpright() &&
            fontEntry->Weight() == NS_FONT_WEIGHT_NORMAL &&
            fontEntry->Stretch() == NS_FONT_STRETCH_NORMAL) {
            numRegularFaces++;
        }

        if (LOG_FONTLIST_ENABLED()) {
            LOG_FONTLIST(("(fontlist) added (%s) to family (%s)"
                 " with style: %s weight: %d stretch: %d"
                 " psname: %s fullname: %s",
                 NS_ConvertUTF16toUTF8(fontEntry->Name()).get(),
                 NS_ConvertUTF16toUTF8(Name()).get(),
                 (fontEntry->IsItalic()) ?
                  "italic" : (fontEntry->IsOblique() ? "oblique" : "normal"),
                 fontEntry->Weight(), fontEntry->Stretch(),
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

    nsCountedRef<FcPattern> pattern(aFontPattern);
    mFontPatterns.AppendElement(pattern);
}

gfxFontconfigFont::gfxFontconfigFont(cairo_scaled_font_t *aScaledFont,
                                     FcPattern *aPattern,
                                     gfxFontEntry *aFontEntry,
                                     const gfxFontStyle *aFontStyle,
                                     bool aNeedsBold) :
    gfxFontconfigFontBase(aScaledFont, aPattern, aFontEntry, aFontStyle)
{
}

gfxFontconfigFont::~gfxFontconfigFont()
{
}

gfxFcPlatformFontList::gfxFcPlatformFontList()
    : mLocalNames(64)
    , mGenericMappings(32)
    , mFcSubstituteCache(64)
    , mLastConfig(nullptr)
    , mAlwaysUseFontconfigGenerics(true)
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
gfxFcPlatformFontList::AddFontSetFamilies(FcFontSet* aFontSet, bool aAppFonts)
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
    RefPtr<gfxFontconfigFontFamily> fontFamily;
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

            fontFamily = static_cast<gfxFontconfigFontFamily*>
                             (mFontFamilies.GetWeak(keyName));
            if (!fontFamily) {
                fontFamily = new gfxFontconfigFontFamily(familyName);
                mFontFamilies.Put(keyName, fontFamily);
            }
            // Record if the family contains fonts from the app font set
            // (in which case we won't rely on fontconfig's charmap, due to
            // bug 1276594).
            if (aAppFonts) {
                fontFamily->SetFamilyContainsAppFonts(true);
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
        fontFamily->AddFontPattern(font);

        // map the psname, fullname ==> font family for local font lookups
        nsAutoString psname, fullname;
        GetFaceNames(font, familyName, psname, fullname);
        if (!psname.IsEmpty()) {
            ToLowerCase(psname);
            mLocalNames.Put(psname, font);
        }
        if (!fullname.IsEmpty()) {
            ToLowerCase(fullname);
            mLocalNames.Put(fullname, font);
        }
    }
}

nsresult
gfxFcPlatformFontList::InitFontListForPlatform()
{
    mLastConfig = FcConfigGetCurrent();

    mLocalNames.Clear();
    mFcSubstituteCache.Clear();

    // iterate over available fonts
    FcFontSet* systemFonts = FcConfigGetFonts(nullptr, FcSetSystem);
    AddFontSetFamilies(systemFonts, /* aAppFonts = */ false);
    mAlwaysUseFontconfigGenerics = PrefFontListsUseOnlyGenerics();

#ifdef MOZ_BUNDLED_FONTS
    ActivateBundledFonts();
    FcFontSet* appFonts = FcConfigGetFonts(nullptr, FcSetApplication);
    AddFontSetFamilies(appFonts, /* aAppFonts = */ true);
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
    gfxFcPlatformFontList* pfl = gfxFcPlatformFontList::PlatformFontList();
    pfl->GetSampleLangForGroup(aLangGroup, fcLang);
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
                                       uint16_t aWeight,
                                       int16_t aStretch,
                                       uint8_t aStyle)
{
    nsAutoString keyName(aFontName);
    ToLowerCase(keyName);

    // if name is not in the global list, done
    FcPattern* fontPattern = mLocalNames.Get(keyName);
    if (!fontPattern) {
        return nullptr;
    }

    return new gfxFontconfigFontEntry(aFontName, fontPattern,
                                      aWeight, aStretch, aStyle);
}

gfxFontEntry*
gfxFcPlatformFontList::MakePlatformFont(const nsAString& aFontName,
                                        uint16_t aWeight,
                                        int16_t aStretch,
                                        uint8_t aStyle,
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

    return new gfxFontconfigFontEntry(aFontName, aWeight, aStretch,
                                      aStyle, aFontData, face);
}

bool
gfxFcPlatformFontList::FindAndAddFamilies(const nsAString& aFamily,
                                          nsTArray<gfxFontFamily*>* aOutput,
                                          gfxFontStyle* aStyle,
                                          gfxFloat aDevToCssSize)
{
    nsAutoString familyName(aFamily);
    ToLowerCase(familyName);
    nsIAtom* language = (aStyle ? aStyle->language.get() : nullptr);

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
        gfxPlatformFontList::FindAndAddFamilies(subst, &cachedFamilies);
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

    // ignore size-specific fonts
    FcPatternAddBool(pat, FC_SCALABLE, FcTrue);

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

static const char kFontNamePrefix[] = "font.name.";

void
gfxFcPlatformFontList::AddGenericFonts(mozilla::FontFamilyType aGenericType,
                                       nsIAtom* aLanguage,
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
        nsIAtom* langGroup = GetLangGroup(aLanguage);
        nsAutoCString langGroupStr;
        if (langGroup) {
            langGroup->ToUTF8String(langGroupStr);
        }
        nsAutoCString prefFontName(kFontNamePrefix);
        prefFontName.Append(generic);
        prefFontName.Append('.');
        prefFontName.Append(langGroupStr);
        nsAdoptingString fontlistValue = Preferences::GetString(prefFontName.get());
        if (fontlistValue) {
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

        bool needsBold;
        gfxFontStyle style;
        gfxPlatformFontList* pfl = gfxPlatformFontList::PlatformFontList();
        gfxFontFamily* family = pfl->GetDefaultFont(&style);
        NS_ASSERTION(family, "couldn't find a default font family");
        gfxFontEntry* fe = family->FindFontForStyle(style, needsBold);
        if (!fe) {
            return nullptr;
        }
        RefPtr<gfxFont> font = fe->FindOrMakeFont(&style, false);
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

gfxPlatformFontList::PrefFontList*
gfxFcPlatformFontList::FindGenericFamilies(const nsAString& aGeneric,
                                           nsIAtom* aLanguage)
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

        // not scalable? skip...
        FcBool scalable;
        if (FcPatternGetBool(font, FC_SCALABLE, 0, &scalable) != FcResultMatch ||
            !scalable) {
            continue;
        }

        FcPatternGetString(font, FC_FAMILY, 0, &mappedGeneric);
        if (mappedGeneric) {
            NS_ConvertUTF8toUTF16 mappedGenericName(ToCharPtr(mappedGeneric));
            AutoTArray<gfxFontFamily*,1> genericFamilies;
            if (gfxPlatformFontList::FindAndAddFamilies(mappedGenericName,
                                                        &genericFamilies)) {
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

            nsDependentCString prefName(names[i] +
                                        ArrayLength(kFontNamePrefix) - 1);
            nsCCharSeparatedTokenizer tokenizer(prefName, '.');
            const nsDependentCSubstring& generic = tokenizer.nextToken();
            const nsDependentCSubstring& langGroup = tokenizer.nextToken();
            nsAdoptingCString fontPrefValue = Preferences::GetCString(names[i]);

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

#endif // MOZ_WIDGET_GTK

/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "nsGkAtoms.h"
#include "nsUnicodeProperties.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsXULAppAPI.h"
#include "SharedFontList-impl.h"
#include "StandardFonts-linux.inc"

#include "mozilla/gfx/HelpersCairo.h"

#include <cairo-ft.h>
#include <fontconfig/fcfreetype.h>
#include <dlfcn.h>
#include <unistd.h>

#ifdef MOZ_WIDGET_GTK
#  include <gdk/gdk.h>
#  include "gfxPlatformGtk.h"
#  include "mozilla/WidgetUtilsGtk.h"
#endif

#ifdef MOZ_X11
#  include "mozilla/X11Util.h"
#endif

#if defined(MOZ_SANDBOX) && defined(XP_LINUX)
#  include "mozilla/SandboxBrokerPolicyFactory.h"
#  include "mozilla/SandboxSettings.h"
#endif

#include FT_MULTIPLE_MASTERS_H

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::unicode;

#ifndef FC_POSTSCRIPT_NAME
#  define FC_POSTSCRIPT_NAME "postscriptname" /* String */
#endif
#ifndef FC_VARIABLE
#  define FC_VARIABLE "variable" /* Bool */
#endif

#define PRINTING_FC_PROPERTY "gfx.printing"

#define LOG_FONTLIST(args) \
  MOZ_LOG(gfxPlatform::GetLog(eGfxLog_fontlist), LogLevel::Debug, args)
#define LOG_FONTLIST_ENABLED() \
  MOZ_LOG_TEST(gfxPlatform::GetLog(eGfxLog_fontlist), LogLevel::Debug)
#define LOG_CMAPDATA_ENABLED() \
  MOZ_LOG_TEST(gfxPlatform::GetLog(eGfxLog_cmapdata), LogLevel::Debug)

static const FcChar8* ToFcChar8Ptr(const char* aStr) {
  return reinterpret_cast<const FcChar8*>(aStr);
}

static const char* ToCharPtr(const FcChar8* aStr) {
  return reinterpret_cast<const char*>(aStr);
}

// canonical name ==> first en name or first name if no en name
// This is the required logic for fullname lookups as per CSS3 Fonts spec.
static uint32_t FindCanonicalNameIndex(FcPattern* aFont,
                                       const char* aLangField) {
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

static void GetFaceNames(FcPattern* aFont, const nsACString& aFamilyName,
                         nsACString& aPostscriptName, nsACString& aFullname) {
  // get the Postscript name
  FcChar8* psname;
  if (FcPatternGetString(aFont, FC_POSTSCRIPT_NAME, 0, &psname) ==
      FcResultMatch) {
    aPostscriptName = ToCharPtr(psname);
  }

  // get the canonical fullname (i.e. en name or first name)
  uint32_t en = FindCanonicalNameIndex(aFont, FC_FULLNAMELANG);
  FcChar8* fullname;
  if (FcPatternGetString(aFont, FC_FULLNAME, en, &fullname) == FcResultMatch) {
    aFullname = ToCharPtr(fullname);
  }

  // if have fullname, done
  if (!aFullname.IsEmpty()) {
    return;
  }

  // otherwise, set the fullname to family + style name [en] and use that
  aFullname = aFamilyName;

  // figure out the en style name
  en = FindCanonicalNameIndex(aFont, FC_STYLELANG);
  nsAutoCString style;
  FcChar8* stylename = nullptr;
  FcPatternGetString(aFont, FC_STYLE, en, &stylename);
  if (stylename) {
    style = ToCharPtr(stylename);
  }

  if (!style.IsEmpty() && !style.EqualsLiteral("Regular")) {
    aFullname.Append(' ');
    aFullname.Append(style);
  }
}

static FontWeight MapFcWeight(int aFcWeight) {
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
static FontStretch MapFcWidth(int aFcWidth) {
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

static void GetFontProperties(FcPattern* aFontPattern, WeightRange* aWeight,
                              StretchRange* aStretch,
                              SlantStyleRange* aSlantStyle) {
  // weight
  int weight;
  if (FcPatternGetInteger(aFontPattern, FC_WEIGHT, 0, &weight) !=
      FcResultMatch) {
    weight = FC_WEIGHT_REGULAR;
  }
  *aWeight = WeightRange(MapFcWeight(weight));

  // width
  int width;
  if (FcPatternGetInteger(aFontPattern, FC_WIDTH, 0, &width) != FcResultMatch) {
    width = FC_WIDTH_NORMAL;
  }
  *aStretch = StretchRange(MapFcWidth(width));

  // italic
  int slant;
  if (FcPatternGetInteger(aFontPattern, FC_SLANT, 0, &slant) != FcResultMatch) {
    slant = FC_SLANT_ROMAN;
  }
  if (slant == FC_SLANT_OBLIQUE) {
    *aSlantStyle = SlantStyleRange(FontSlantStyle::Oblique());
  } else if (slant > 0) {
    *aSlantStyle = SlantStyleRange(FontSlantStyle::Italic());
  }
}

gfxFontconfigFontEntry::gfxFontconfigFontEntry(const nsACString& aFaceName,
                                               FcPattern* aFontPattern,
                                               bool aIgnoreFcCharmap)
    : gfxFT2FontEntryBase(aFaceName),
      mFontPattern(aFontPattern),
      mFTFaceInitialized(false),
      mIgnoreFcCharmap(aIgnoreFcCharmap),
      mHasVariationsInitialized(false),
      mAspect(0.0) {
  GetFontProperties(aFontPattern, &mWeightRange, &mStretchRange, &mStyleRange);
}

gfxFontEntry* gfxFontconfigFontEntry::Clone() const {
  MOZ_ASSERT(!IsUserFont(), "we can only clone installed fonts!");
  return new gfxFontconfigFontEntry(Name(), mFontPattern, mIgnoreFcCharmap);
}

static already_AddRefed<FcPattern> CreatePatternForFace(FT_Face aFace) {
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
  RefPtr<FcPattern> pattern =
      dont_AddRef(FcFreeTypeQueryFace(aFace, ToFcChar8Ptr(""), 0, nullptr));
  // given that we have a FT_Face, not really sure this is possible...
  if (!pattern) {
    pattern = dont_AddRef(FcPatternCreate());
  }
  FcPatternDel(pattern, FC_FILE);
  FcPatternDel(pattern, FC_INDEX);

  // Make a new pattern and store the face in it so that cairo uses
  // that when creating a cairo font face.
  FcPatternAddFTFace(pattern, FC_FT_FACE, aFace);

  return pattern.forget();
}

static already_AddRefed<SharedFTFace> CreateFaceForPattern(
    FcPattern* aPattern) {
  FcChar8* filename;
  if (FcPatternGetString(aPattern, FC_FILE, 0, &filename) != FcResultMatch) {
    return nullptr;
  }
  int index;
  if (FcPatternGetInteger(aPattern, FC_INDEX, 0, &index) != FcResultMatch) {
    index = 0;  // default to 0 if not found in pattern
  }
  return Factory::NewSharedFTFace(nullptr, ToCharPtr(filename), index);
}

gfxFontconfigFontEntry::gfxFontconfigFontEntry(const nsACString& aFaceName,
                                               WeightRange aWeight,
                                               StretchRange aStretch,
                                               SlantStyleRange aStyle,
                                               RefPtr<SharedFTFace>&& aFace)
    : gfxFT2FontEntryBase(aFaceName),
      mFTFace(std::move(aFace)),
      mFTFaceInitialized(true),
      mIgnoreFcCharmap(true),
      mHasVariationsInitialized(false),
      mAspect(0.0) {
  mWeightRange = aWeight;
  mStyleRange = aStyle;
  mStretchRange = aStretch;
  mIsDataUserFont = true;

  mFontPattern = CreatePatternForFace(mFTFace->GetFace());
}

gfxFontconfigFontEntry::gfxFontconfigFontEntry(const nsACString& aFaceName,
                                               FcPattern* aFontPattern,
                                               WeightRange aWeight,
                                               StretchRange aStretch,
                                               SlantStyleRange aStyle)
    : gfxFT2FontEntryBase(aFaceName),
      mFontPattern(aFontPattern),
      mFTFaceInitialized(false),
      mHasVariationsInitialized(false),
      mAspect(0.0) {
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

static void InitializeVarFuncs() {
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

gfxFontconfigFontEntry::~gfxFontconfigFontEntry() {
  if (mMMVar) {
    // Prior to freetype 2.9, there was no specific function to free the
    // FT_MM_Var record, and the docs just said to use free().
    // InitializeVarFuncs must have been called in order for mMMVar to be
    // non-null here, so we don't need to do it again.
    if (sDoneVar) {
      MOZ_ASSERT(mFTFace, "How did mMMVar get set without a face?");
      (*sDoneVar)(mFTFace->GetFace()->glyph->library, mMMVar);
    } else {
      free(mMMVar);
    }
  }
}

nsresult gfxFontconfigFontEntry::ReadCMAP(FontInfoData* aFontInfoData) {
  // attempt this once, if errors occur leave a blank cmap
  if (mCharacterMap) {
    return NS_OK;
  }

  RefPtr<gfxCharacterMap> charmap;
  nsresult rv;

  if (aFontInfoData &&
      (charmap = GetCMAPFromFontInfo(aFontInfoData, mUVSOffset))) {
    rv = NS_OK;
  } else {
    uint32_t kCMAP = TRUETYPE_TAG('c', 'm', 'a', 'p');
    charmap = new gfxCharacterMap();
    AutoTable cmapTable(this, kCMAP);

    if (cmapTable) {
      uint32_t cmapLen;
      const uint8_t* cmapData = reinterpret_cast<const uint8_t*>(
          hb_blob_get_data(cmapTable, &cmapLen));
      rv = gfxFontUtils::ReadCMAP(cmapData, cmapLen, *charmap, mUVSOffset);
    } else {
      rv = NS_ERROR_NOT_AVAILABLE;
    }
  }

  mHasCmapTable = NS_SUCCEEDED(rv);
  if (mHasCmapTable) {
    gfxPlatformFontList* pfl = gfxPlatformFontList::PlatformFontList();
    fontlist::FontList* sharedFontList = pfl->SharedFontList();
    if (!IsUserFont() && mShmemFace) {
      mShmemFace->SetCharacterMap(sharedFontList, charmap);  // async
      if (!TrySetShmemCharacterMap()) {
        // Temporarily retain charmap, until the shared version is
        // ready for use.
        mCharacterMap = charmap;
      }
    } else {
      mCharacterMap = pfl->FindCharMap(charmap);
    }
  } else {
    // if error occurred, initialize to null cmap
    mCharacterMap = new gfxCharacterMap();
  }

  LOG_FONTLIST(("(fontlist-cmap) name: %s, size: %zu hash: %8.8x%s\n",
                mName.get(), charmap->SizeOfIncludingThis(moz_malloc_size_of),
                charmap->mHash, mCharacterMap == charmap ? " new" : ""));
  if (LOG_CMAPDATA_ENABLED()) {
    char prefix[256];
    SprintfLiteral(prefix, "(cmapdata) name: %.220s", mName.get());
    charmap->Dump(prefix, eGfxLog_cmapdata);
  }

  return rv;
}

static bool HasChar(FcPattern* aFont, FcChar32 aCh) {
  FcCharSet* charset = nullptr;
  FcPatternGetCharSet(aFont, FC_CHARSET, 0, &charset);
  return charset && FcCharSetHasChar(charset, aCh);
}

bool gfxFontconfigFontEntry::TestCharacterMap(uint32_t aCh) {
  // For user fonts, or for fonts bundled with the app (which might include
  // color/svg glyphs where the default glyphs may be blank, and thus confuse
  // fontconfig/freetype's char map checking), we instead check the cmap
  // directly for character coverage.
  if (mIgnoreFcCharmap) {
    // If it does not actually have a cmap, switch our strategy to use
    // fontconfig's charmap after all (except for data fonts, which must
    // always have a cmap to have passed OTS validation).
    if (!mIsDataUserFont && !HasFontTable(TRUETYPE_TAG('c', 'm', 'a', 'p'))) {
      mIgnoreFcCharmap = false;
      // ...and continue with HasChar() below.
    } else {
      return gfxFontEntry::TestCharacterMap(aCh);
    }
  }
  // otherwise (for system fonts), use the charmap in the pattern
  return HasChar(mFontPattern, aCh);
}

bool gfxFontconfigFontEntry::HasFontTable(uint32_t aTableTag) {
  if (FTUserFontData* ufd = GetUserFontData()) {
    return !!gfxFontUtils::FindTableDirEntry(ufd->FontData(), aTableTag);
  }
  return gfxFT2FontEntryBase::FaceHasTable(GetFTFace(), aTableTag);
}

hb_blob_t* gfxFontconfigFontEntry::GetFontTable(uint32_t aTableTag) {
  // for data fonts, read directly from the font data
  if (FTUserFontData* ufd = GetUserFontData()) {
    return gfxFontUtils::GetTableFromFontData(ufd->FontData(), aTableTag);
  }

  return gfxFontEntry::GetFontTable(aTableTag);
}

double gfxFontconfigFontEntry::GetAspect() {
  if (mAspect != 0.0) {
    return mAspect;
  }

  // try to compute aspect from OS/2 metrics if available
  AutoTable os2Table(this, TRUETYPE_TAG('O', 'S', '/', '2'));
  if (os2Table) {
    uint16_t upem = UnitsPerEm();
    if (upem != kInvalidUPEM) {
      uint32_t len;
      auto os2 =
          reinterpret_cast<const OS2Table*>(hb_blob_get_data(os2Table, &len));
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
  s.size = 100.0;  // pick large size to avoid possible hinting artifacts
  RefPtr<gfxFont> font = FindOrMakeFont(&s);
  if (font) {
    const gfxFont::Metrics& metrics =
        font->GetMetrics(nsFontMetrics::eHorizontal);

    // The factor of 0.1 ensures that xHeight is sane so fonts don't
    // become huge.  Strictly ">" ensures that xHeight and emHeight are
    // not both zero.
    if (metrics.xHeight > 0.1 * metrics.emHeight) {
      mAspect = metrics.xHeight / metrics.emHeight;
    }
  }

  return mAspect;
}

static void PrepareFontOptions(FcPattern* aPattern, int* aOutLoadFlags,
                               unsigned int* aOutSynthFlags) {
  int loadFlags = FT_LOAD_DEFAULT;
  unsigned int synthFlags = 0;

  // xxx - taken from the gfxFontconfigFonts code, needs to be reviewed

  FcBool printing;
  if (FcPatternGetBool(aPattern, PRINTING_FC_PROPERTY, 0, &printing) !=
      FcResultMatch) {
    printing = FcFalse;
  }

  // Font options are set explicitly here to improve cairo's caching
  // behavior and to record the relevant parts of the pattern so that
  // the pattern can be released.
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

  int fc_hintstyle = FC_HINT_NONE;
  if ((!printing || hinting) &&
      FcPatternGetInteger(aPattern, FC_HINT_STYLE, 0, &fc_hintstyle) !=
          FcResultMatch) {
    fc_hintstyle = FC_HINT_FULL;
  }
  switch (fc_hintstyle) {
    case FC_HINT_NONE:
      loadFlags = FT_LOAD_NO_HINTING;
      break;
    case FC_HINT_SLIGHT:
      loadFlags = FT_LOAD_TARGET_LIGHT;
      break;
  }

  FcBool fc_antialias;
  if (FcPatternGetBool(aPattern, FC_ANTIALIAS, 0, &fc_antialias) !=
      FcResultMatch) {
    fc_antialias = FcTrue;
  }
  if (!fc_antialias) {
    if (fc_hintstyle != FC_HINT_NONE) {
      loadFlags = FT_LOAD_TARGET_MONO;
    }
    loadFlags |= FT_LOAD_MONOCHROME;
  } else if (fc_hintstyle == FC_HINT_FULL) {
    int fc_rgba;
    if (FcPatternGetInteger(aPattern, FC_RGBA, 0, &fc_rgba) != FcResultMatch) {
      fc_rgba = FC_RGBA_UNKNOWN;
    }
    switch (fc_rgba) {
      case FC_RGBA_RGB:
      case FC_RGBA_BGR:
        loadFlags = FT_LOAD_TARGET_LCD;
        break;
      case FC_RGBA_VRGB:
      case FC_RGBA_VBGR:
        loadFlags = FT_LOAD_TARGET_LCD_V;
        break;
    }
  }

  FcBool bitmap;
  if (FcPatternGetBool(aPattern, FC_EMBEDDED_BITMAP, 0, &bitmap) !=
      FcResultMatch) {
    bitmap = FcFalse;
  }
  if (fc_antialias && (fc_hintstyle == FC_HINT_NONE || !bitmap)) {
    loadFlags |= FT_LOAD_NO_BITMAP;
  }

  FcBool autohint;
  if (FcPatternGetBool(aPattern, FC_AUTOHINT, 0, &autohint) == FcResultMatch &&
      autohint) {
    loadFlags |= FT_LOAD_FORCE_AUTOHINT;
  }

  FcBool embolden;
  if (FcPatternGetBool(aPattern, FC_EMBOLDEN, 0, &embolden) == FcResultMatch &&
      embolden) {
    synthFlags |= CAIRO_FT_SYNTHESIZE_BOLD;
  }

  *aOutLoadFlags = loadFlags;
  *aOutSynthFlags = synthFlags;
}

#ifdef MOZ_WIDGET_GTK
// defintion included below
static void ApplyGdkScreenFontOptions(FcPattern* aPattern);
#endif

#ifdef MOZ_X11
static bool GetXftInt(Display* aDisplay, const char* aName, int* aResult) {
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

static void PreparePattern(FcPattern* aPattern, bool aIsPrinterFont) {
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
  if (aIsPrinterFont) {
    cairo_font_options_t* options = cairo_font_options_create();
    cairo_font_options_set_hint_style(options, CAIRO_HINT_STYLE_NONE);
    cairo_font_options_set_antialias(options, CAIRO_ANTIALIAS_GRAY);
    cairo_ft_font_options_substitute(options, aPattern);
    cairo_font_options_destroy(options);
    FcPatternAddBool(aPattern, PRINTING_FC_PROPERTY, FcTrue);
  } else if (!gfxPlatform::IsHeadless()) {
#ifdef MOZ_WIDGET_GTK
    ApplyGdkScreenFontOptions(aPattern);

#  ifdef MOZ_X11
    FcValue value;
    int lcdfilter;
    if (FcPatternGet(aPattern, FC_LCD_FILTER, 0, &value) == FcResultNoMatch) {
      GdkDisplay* dpy = gdk_display_get_default();
      if (mozilla::widget::GdkIsX11Display(dpy) &&
          GetXftInt(GDK_DISPLAY_XDISPLAY(dpy), "lcdfilter", &lcdfilter)) {
        FcPatternAddInteger(aPattern, FC_LCD_FILTER, lcdfilter);
      }
    }
#  endif  // MOZ_X11
#endif    // MOZ_WIDGET_GTK
  }

  FcDefaultSubstitute(aPattern);
}

void gfxFontconfigFontEntry::UnscaledFontCache::MoveToFront(size_t aIndex) {
  if (aIndex > 0) {
    ThreadSafeWeakPtr<UnscaledFontFontconfig> front =
        std::move(mUnscaledFonts[aIndex]);
    for (size_t i = aIndex; i > 0; i--) {
      mUnscaledFonts[i] = std::move(mUnscaledFonts[i - 1]);
    }
    mUnscaledFonts[0] = std::move(front);
  }
}

already_AddRefed<UnscaledFontFontconfig>
gfxFontconfigFontEntry::UnscaledFontCache::Lookup(const std::string& aFile,
                                                  uint32_t aIndex) {
  for (size_t i = 0; i < kNumEntries; i++) {
    RefPtr<UnscaledFontFontconfig> entry(mUnscaledFonts[i]);
    if (entry && entry->GetFile() == aFile && entry->GetIndex() == aIndex) {
      MoveToFront(i);
      return entry.forget();
    }
  }
  return nullptr;
}

static inline gfxFloat SizeForStyle(gfxFontconfigFontEntry* aEntry,
                                    const gfxFontStyle& aStyle) {
  return aStyle.sizeAdjust >= 0.0 ? aStyle.GetAdjustedSize(aEntry->GetAspect())
                                  : aStyle.size;
}

static double ChooseFontSize(gfxFontconfigFontEntry* aEntry,
                             const gfxFontStyle& aStyle) {
  double requestedSize = SizeForStyle(aEntry, aStyle);
  double bestDist = -1.0;
  double bestSize = requestedSize;
  double size;
  int v = 0;
  while (FcPatternGetDouble(aEntry->GetPattern(), FC_PIXEL_SIZE, v, &size) ==
         FcResultMatch) {
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
    if (FcPatternGetBool(aEntry->GetPattern(), FC_SCALABLE, 0, &scalable) ==
            FcResultMatch &&
        scalable) {
      return requestedSize;
    }
  }
  return bestSize;
}

gfxFont* gfxFontconfigFontEntry::CreateFontInstance(
    const gfxFontStyle* aFontStyle) {
  RefPtr<FcPattern> pattern = dont_AddRef(FcPatternCreate());
  if (!pattern) {
    NS_WARNING("Failed to create Fontconfig pattern for font instance");
    return nullptr;
  }

  double size = ChooseFontSize(this, *aFontStyle);
  FcPatternAddDouble(pattern, FC_PIXEL_SIZE, size);

  RefPtr<SharedFTFace> face = GetFTFace();
  if (!face) {
    NS_WARNING("Failed to get FreeType face for pattern");
    return nullptr;
  }
  if (HasVariations()) {
    // For variation fonts, we create a new FT_Face here so that
    // variation coordinates from the style can be applied without
    // affecting other font instances created from the same entry
    // (font resource).
    // For user fonts: create a new FT_Face from the font data, and then make
    // a pattern from that.
    // For system fonts: create a new FT_Face and store it in a copy of the
    // original mFontPattern.
    RefPtr<SharedFTFace> varFace = face->GetData()
                                       ? face->GetData()->CloneFace()
                                       : CreateFaceForPattern(mFontPattern);
    if (varFace) {
      AutoTArray<gfxFontVariation, 8> settings;
      GetVariationsForStyle(settings, *aFontStyle);
      gfxFT2FontBase::SetupVarCoords(GetMMVar(), settings, varFace->GetFace());
      face = std::move(varFace);
    }
  }

  PreparePattern(pattern, aFontStyle->printerFont);
  RefPtr<FcPattern> renderPattern =
      dont_AddRef(FcFontRenderPrepare(nullptr, pattern, mFontPattern));
  if (!renderPattern) {
    NS_WARNING("Failed to prepare Fontconfig pattern for font instance");
    return nullptr;
  }

  if (aFontStyle->NeedsSyntheticBold(this)) {
    FcPatternAddBool(renderPattern, FC_EMBOLDEN, FcTrue);
  }

  // will synthetic oblique be applied using a transform?
  if (IsUpright() && aFontStyle->style != FontSlantStyle::Normal() &&
      aFontStyle->allowSyntheticStyle) {
    // disable embedded bitmaps (mimics behavior in 90-synthetic.conf)
    FcPatternDel(renderPattern, FC_EMBEDDED_BITMAP);
    FcPatternAddBool(renderPattern, FC_EMBEDDED_BITMAP, FcFalse);
  }

  int loadFlags;
  unsigned int synthFlags;
  PrepareFontOptions(renderPattern, &loadFlags, &synthFlags);

  std::string file;
  int index = 0;
  if (!face->GetData()) {
    const FcChar8* fcFile;
    if (FcPatternGetString(renderPattern, FC_FILE, 0,
                           const_cast<FcChar8**>(&fcFile)) != FcResultMatch ||
        FcPatternGetInteger(renderPattern, FC_INDEX, 0, &index) !=
            FcResultMatch) {
      NS_WARNING("No file in Fontconfig pattern for font instance");
      return nullptr;
    }
    file = ToCharPtr(fcFile);
  }

  RefPtr<UnscaledFontFontconfig> unscaledFont =
      mUnscaledFontCache.Lookup(file, index);
  if (!unscaledFont) {
    unscaledFont = mFTFace->GetData() ? new UnscaledFontFontconfig(mFTFace)
                                      : new UnscaledFontFontconfig(
                                            std::move(file), index, mFTFace);
    mUnscaledFontCache.Add(unscaledFont);
  }

  gfxFont* newFont = new gfxFontconfigFont(
      unscaledFont, std::move(face), renderPattern, size, this, aFontStyle,
      loadFlags, (synthFlags & CAIRO_FT_SYNTHESIZE_BOLD) != 0);

  return newFont;
}

const RefPtr<SharedFTFace>& gfxFontconfigFontEntry::GetFTFace() {
  if (!mFTFaceInitialized) {
    mFTFaceInitialized = true;
    mFTFace = CreateFaceForPattern(mFontPattern);
  }
  return mFTFace;
}

FTUserFontData* gfxFontconfigFontEntry::GetUserFontData() {
  if (mFTFace && mFTFace->GetData()) {
    return static_cast<FTUserFontData*>(mFTFace->GetData());
  }
  return nullptr;
}

bool gfxFontconfigFontEntry::HasVariations() {
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
    if ((FcPatternGetBool(mFontPattern, FC_VARIABLE, 0, &variable) ==
         FcResultMatch) &&
        variable) {
      mHasVariations = true;
    }
  } else {
    if (GetFTFace()) {
      mHasVariations =
          mFTFace->GetFace()->face_flags & FT_FACE_FLAG_MULTIPLE_MASTERS;
    }
  }

  return mHasVariations;
}

FT_MM_Var* gfxFontconfigFontEntry::GetMMVar() {
  if (mMMVarInitialized) {
    return mMMVar;
  }
  mMMVarInitialized = true;
  InitializeVarFuncs();
  if (!sGetVar) {
    return nullptr;
  }
  if (!GetFTFace()) {
    return nullptr;
  }
  if (FT_Err_Ok != (*sGetVar)(mFTFace->GetFace(), &mMMVar)) {
    mMMVar = nullptr;
  }
  return mMMVar;
}

void gfxFontconfigFontEntry::GetVariationAxes(
    nsTArray<gfxFontVariationAxis>& aAxes) {
  if (!HasVariations()) {
    return;
  }
  gfxFT2Utils::GetVariationAxes(GetMMVar(), aAxes);
}

void gfxFontconfigFontEntry::GetVariationInstances(
    nsTArray<gfxFontVariationInstance>& aInstances) {
  if (!HasVariations()) {
    return;
  }
  gfxFT2Utils::GetVariationInstances(this, GetMMVar(), aInstances);
}

nsresult gfxFontconfigFontEntry::CopyFontTable(uint32_t aTableTag,
                                               nsTArray<uint8_t>& aBuffer) {
  NS_ASSERTION(!mIsDataUserFont,
               "data fonts should be reading tables directly from memory");
  return gfxFT2FontEntryBase::CopyFaceTable(GetFTFace(), aTableTag, aBuffer);
}

void gfxFontconfigFontFamily::FindStyleVariations(FontInfoData* aFontInfoData) {
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
    nsAutoCString psname, fullname;
    GetFaceNames(face, mName, psname, fullname);
    const nsAutoCString& faceName = !psname.IsEmpty() ? psname : fullname;

    gfxFontconfigFontEntry* fontEntry =
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
      LOG_FONTLIST(
          ("(fontlist) added (%s) to family (%s)"
           " with style: %s weight: %s stretch: %s"
           " psname: %s fullname: %s",
           fontEntry->Name().get(), Name().get(), styleString.get(),
           weightString.get(), stretchString.get(), psname.get(),
           fullname.get()));
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

  CheckForSimpleFamily();
}

void gfxFontconfigFontFamily::AddFontPattern(FcPattern* aFontPattern) {
  NS_ASSERTION(
      !mHasStyles,
      "font patterns must not be added to already enumerated families");

  FcBool outline;
  if (FcPatternGetBool(aFontPattern, FC_OUTLINE, 0, &outline) !=
          FcResultMatch ||
      !outline) {
    mHasNonScalableFaces = true;

    FcBool scalable;
    if (FcPatternGetBool(aFontPattern, FC_SCALABLE, 0, &scalable) ==
            FcResultMatch &&
        scalable) {
      mForceScalable = true;
    }
  }

  mFontPatterns.AppendElement(aFontPattern);
}

static const double kRejectDistance = 10000.0;

// Calculate a distance score representing the size disparity between the
// requested style's size and the font entry's size.
static double SizeDistance(gfxFontconfigFontEntry* aEntry,
                           const gfxFontStyle& aStyle, bool aForceScalable) {
  double requestedSize = SizeForStyle(aEntry, aStyle);
  double bestDist = -1.0;
  double size;
  int v = 0;
  while (FcPatternGetDouble(aEntry->GetPattern(), FC_PIXEL_SIZE, v, &size) ==
         FcResultMatch) {
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

void gfxFontconfigFontFamily::FindAllFontsForStyle(
    const gfxFontStyle& aFontStyle, nsTArray<gfxFontEntry*>& aFontEntryList,
    bool aIgnoreSizeTolerance) {
  gfxFontFamily::FindAllFontsForStyle(aFontStyle, aFontEntryList,
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
    double dist =
        SizeDistance(entry, aFontStyle, mForceScalable || aIgnoreSizeTolerance);
    // If the entry is scalable or has a style that does not match
    // the group of unscalable fonts, then start a new group.
    if (dist < 0.0 || !bestEntry || bestEntry->Stretch() != entry->Stretch() ||
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
        aFontEntryList[i - 1 - skipped] = entry;
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

static bool PatternHasLang(const FcPattern* aPattern, const FcChar8* aLang) {
  FcLangSet* langset;

  if (FcPatternGetLangSet(aPattern, FC_LANG, 0, &langset) != FcResultMatch) {
    return false;
  }

  if (FcLangSetHasLang(langset, aLang) != FcLangDifferentLang) {
    return true;
  }
  return false;
}

bool gfxFontconfigFontFamily::SupportsLangGroup(nsAtom* aLangGroup) const {
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
    fontPattern = static_cast<gfxFontconfigFontEntry*>(mAvailableFonts[0].get())
                      ->GetPattern();
  } else {
    return true;
  }

  // is lang included in the underlying pattern?
  return PatternHasLang(fontPattern, ToFcChar8Ptr(fcLang.get()));
}

/* virtual */
gfxFontconfigFontFamily::~gfxFontconfigFontFamily() {
  // Should not be dropped by stylo
  MOZ_ASSERT(NS_IsMainThread());
}

template <typename Func>
void gfxFontconfigFontFamily::AddFacesToFontList(Func aAddPatternFunc) {
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

gfxFontconfigFont::gfxFontconfigFont(
    const RefPtr<UnscaledFontFontconfig>& aUnscaledFont,
    RefPtr<SharedFTFace>&& aFTFace, FcPattern* aPattern, gfxFloat aAdjustedSize,
    gfxFontEntry* aFontEntry, const gfxFontStyle* aFontStyle, int aLoadFlags,
    bool aEmbolden)
    : gfxFT2FontBase(aUnscaledFont, std::move(aFTFace), aFontEntry, aFontStyle,
                     aLoadFlags, aEmbolden),
      mPattern(aPattern) {
  mAdjustedSize = aAdjustedSize;
  InitMetrics();
}

gfxFontconfigFont::~gfxFontconfigFont() = default;

already_AddRefed<ScaledFont> gfxFontconfigFont::GetScaledFont(
    mozilla::gfx::DrawTarget* aTarget) {
  if (!mAzureScaledFont) {
    mAzureScaledFont = Factory::CreateScaledFontForFontconfigFont(
        GetUnscaledFont(), GetAdjustedSize(), mFTFace, GetPattern());
    InitializeScaledFont();
  }

  RefPtr<ScaledFont> scaledFont(mAzureScaledFont);
  return scaledFont.forget();
}

bool gfxFontconfigFont::ShouldHintMetrics() const {
  return !GetStyle()->printerFont;
}

gfxFcPlatformFontList::gfxFcPlatformFontList()
    : mLocalNames(64),
      mGenericMappings(32),
      mFcSubstituteCache(64),
      mLastConfig(nullptr),
      mAlwaysUseFontconfigGenerics(true) {
  CheckFamilyList(kBaseFonts_Ubuntu_20_04,
                  ArrayLength(kBaseFonts_Ubuntu_20_04));
  CheckFamilyList(kLangFonts_Ubuntu_20_04,
                  ArrayLength(kLangFonts_Ubuntu_20_04));
  CheckFamilyList(kBaseFonts_Fedora_32, ArrayLength(kBaseFonts_Fedora_32));
  mLastConfig = FcConfigGetCurrent();
  if (XRE_IsParentProcess()) {
    // if the rescan interval is set, start the timer
    int rescanInterval = FcConfigGetRescanInterval(nullptr);
    if (rescanInterval) {
      NS_NewTimerWithFuncCallback(
          getter_AddRefs(mCheckFontUpdatesTimer), CheckFontUpdates, this,
          (rescanInterval + 1) * 1000, nsITimer::TYPE_REPEATING_SLACK,
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

gfxFcPlatformFontList::~gfxFcPlatformFontList() {
  if (mCheckFontUpdatesTimer) {
    mCheckFontUpdatesTimer->Cancel();
    mCheckFontUpdatesTimer = nullptr;
  }
}

void gfxFcPlatformFontList::AddFontSetFamilies(FcFontSet* aFontSet,
                                               const SandboxPolicy* aPolicy,
                                               bool aAppFonts) {
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
  nsAutoCString familyName;
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

#if defined(MOZ_SANDBOX) && defined(XP_LINUX)
    // Skip any fonts that will be blocked by the content-process sandbox
    // policy.
    if (aPolicy && !(aPolicy->Lookup(reinterpret_cast<const char*>(path)) &
                     SandboxBroker::Perms::MAY_READ)) {
      continue;
    }
#endif

    AddPatternToFontList(pattern, lastFamilyName, familyName, fontFamily,
                         aAppFonts);
  }
}

void gfxFcPlatformFontList::AddPatternToFontList(
    FcPattern* aFont, FcChar8*& aLastFamilyName, nsACString& aFamilyName,
    RefPtr<gfxFontconfigFontFamily>& aFontFamily, bool aAppFonts) {
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
    aFamilyName = ToCharPtr(canonical);
    nsAutoCString keyName(aFamilyName);
    ToLowerCase(keyName);

    aFontFamily = static_cast<gfxFontconfigFontFamily*>(
        mFontFamilies
            .LookupOrInsertWith(keyName,
                                [&] {
                                  FontVisibility visibility =
                                      aAppFonts
                                          ? FontVisibility::Base
                                          : GetVisibilityForFamily(keyName);
                                  return MakeRefPtr<gfxFontconfigFontFamily>(
                                      aFamilyName, visibility);
                                })
            .get());
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
      nsAutoCString otherFamilyName(ToCharPtr(otherName));
      AddOtherFamilyName(aFontFamily, otherFamilyName);
      n++;
      if (n == int(cIndex)) {
        n++;  // skip over canonical name
      }
    }
  }

  MOZ_ASSERT(aFontFamily, "font must belong to a font family");
  aFontFamily->AddFontPattern(aFont);

  // map the psname, fullname ==> font family for local font lookups
  nsAutoCString psname, fullname;
  GetFaceNames(aFont, aFamilyName, psname, fullname);
  if (!psname.IsEmpty()) {
    ToLowerCase(psname);
    mLocalNames.InsertOrUpdate(psname, RefPtr{aFont});
  }
  if (!fullname.IsEmpty()) {
    ToLowerCase(fullname);
    mLocalNames.InsertOrUpdate(fullname, RefPtr{aFont});
  }
}

nsresult gfxFcPlatformFontList::InitFontListForPlatform() {
#ifdef MOZ_BUNDLED_FONTS
  if (StaticPrefs::gfx_bundled_fonts_activate_AtStartup() != 0) {
    ActivateBundledFonts();
  }
#endif

  mLocalNames.Clear();
  mFcSubstituteCache.Clear();

  mAlwaysUseFontconfigGenerics = PrefFontListsUseOnlyGenerics();
  mOtherFamilyNamesInitialized = true;

  mLastConfig = FcConfigGetCurrent();

  if (XRE_IsContentProcess()) {
    // Content process: use the font list passed from the chrome process,
    // because we can't rely on fontconfig in the presence of sandboxing;
    // it may report fonts that we can't actually access.

    FcChar8* lastFamilyName = (FcChar8*)"";
    RefPtr<gfxFontconfigFontFamily> fontFamily;
    nsAutoCString familyName;

    // Get font list that was passed during XPCOM startup
    // or in an UpdateFontList message.
    auto& fontList = dom::ContentChild::GetSingleton()->SystemFontList();

    // For fontconfig versions between 2.10.94 and 2.11.1 inclusive,
    // we need to escape any leading space in the charset element,
    // otherwise FcNameParse will fail. :(
    //
    // The bug was introduced on 2013-05-24 by
    //   https://cgit.freedesktop.org/fontconfig/commit/?id=cd9b1033a68816a7acfbba1718ba0aa5888f6ec7
    //   "Bug 64906 - FcNameParse() should ignore leading whitespace in
    //   parameters"
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

    for (FontPatternListEntry& fpe : fontList) {
      nsCString& patternStr = fpe.pattern();
      if (fcCharsetParseBug) {
        int32_t index = patternStr.Find(":charset= ");
        if (index != kNotFound) {
          // insert backslash after the =, before the space
          patternStr.Insert('\\', index + 9);
        }
      }
      FcPattern* pattern = FcNameParse((const FcChar8*)patternStr.get());
      AddPatternToFontList(pattern, lastFamilyName, familyName, fontFamily,
                           fpe.appFontFamily());
      FcPatternDestroy(pattern);
    }

    LOG_FONTLIST(
        ("got font list from chrome process: "
         "%u faces in %u families",
         (unsigned)fontList.Length(), mFontFamilies.Count()));

    fontList.Clear();

    return NS_OK;
  }

  UniquePtr<SandboxPolicy> policy;

#if defined(MOZ_SANDBOX) && defined(XP_LINUX)
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
  if (StaticPrefs::gfx_bundled_fonts_activate_AtStartup() != 0) {
    FcFontSet* appFonts = FcConfigGetFonts(nullptr, FcSetApplication);
    AddFontSetFamilies(appFonts, policy.get(), /* aAppFonts = */ true);
  }
#endif

  return NS_OK;
}

void gfxFcPlatformFontList::ReadSystemFontList(
    nsTArray<FontPatternListEntry>* retValue) {
  // Fontconfig versions below 2.9 drop the FC_FILE element in FcNameUnparse
  // (see https://bugs.freedesktop.org/show_bug.cgi?id=26718), so when using
  // an older version, we manually append it to the unparsed pattern.
  if (FcGetVersion() < 20900) {
    for (auto iter = mFontFamilies.Iter(); !iter.Done(); iter.Next()) {
      auto family = static_cast<gfxFontconfigFontFamily*>(iter.Data().get());
      family->AddFacesToFontList([&](FcPattern* aPat, bool aAppFonts) {
        char* s = (char*)FcNameUnparse(aPat);
        nsDependentCString patternStr(s);
        char* file = nullptr;
        if (FcResultMatch ==
            FcPatternGetString(aPat, FC_FILE, 0, (FcChar8**)&file)) {
          patternStr.Append(":file=");
          patternStr.Append(file);
        }
        retValue->AppendElement(FontPatternListEntry(patternStr, aAppFonts));
        free(s);
      });
    }
  } else {
    for (auto iter = mFontFamilies.Iter(); !iter.Done(); iter.Next()) {
      auto family = static_cast<gfxFontconfigFontFamily*>(iter.Data().get());
      family->AddFacesToFontList([&](FcPattern* aPat, bool aAppFonts) {
        char* s = (char*)FcNameUnparse(aPat);
        nsDependentCString patternStr(s);
        retValue->AppendElement(FontPatternListEntry(patternStr, aAppFonts));
        free(s);
      });
    }
  }
}

void gfxFcPlatformFontList::InitSharedFontListForPlatform() {
  mLocalNames.Clear();
  mFcSubstituteCache.Clear();

  mAlwaysUseFontconfigGenerics = PrefFontListsUseOnlyGenerics();
  mOtherFamilyNamesInitialized = true;

  mLastConfig = FcConfigGetCurrent();

  if (!XRE_IsParentProcess()) {
    // Content processes will access the shared-memory data created by the
    // parent, so they do not need to query fontconfig for the available
    // fonts themselves.
    return;
  }

#ifdef MOZ_BUNDLED_FONTS
  if (StaticPrefs::gfx_bundled_fonts_activate_AtStartup() != 0) {
    TimeStamp start = TimeStamp::Now();
    ActivateBundledFonts();
    TimeStamp end = TimeStamp::Now();
    Telemetry::Accumulate(Telemetry::FONTLIST_BUNDLEDFONTS_ACTIVATE,
                          (end - start).ToMilliseconds());
  }
#endif

  UniquePtr<SandboxPolicy> policy;

#if defined(MOZ_CONTENT_SANDBOX) && defined(XP_LINUX)
  // If read sandboxing is enabled, create a temporary SandboxPolicy to
  // check font paths; use a fake PID to avoid picking up any PID-specific
  // rules by accident.
  SandboxBrokerPolicyFactory policyFactory;
  if (GetEffectiveContentSandboxLevel() > 2 &&
      !PR_GetEnv("MOZ_DISABLE_CONTENT_SANDBOX")) {
    policy = policyFactory.GetContentPolicy(-1, false);
  }
#endif

  nsTArray<fontlist::Family::InitData> families;

  using FaceInitArray = nsTArray<fontlist::Face::InitData>;
  nsClassHashtable<nsCStringHashKey, FaceInitArray> faces;

  // Do we need to work around the fontconfig FcNameParse/FcNameUnparse bug
  // (present in versions between 2.10.94 and 2.11.1 inclusive)? See comment
  // in InitFontListForPlatform for details.
  int fcVersion = FcGetVersion();
  bool fcCharsetParseBug = fcVersion >= 21094 && fcVersion <= 21101;

  auto addPattern = [this, fcCharsetParseBug, &families, &faces](
                        FcPattern* aPattern, FcChar8*& aLastFamilyName,
                        nsCString& aFamilyName, bool aAppFont) -> void {
    // get canonical name
    uint32_t cIndex = FindCanonicalNameIndex(aPattern, FC_FAMILYLANG);
    FcChar8* canonical = nullptr;
    FcPatternGetString(aPattern, FC_FAMILY, cIndex, &canonical);
    if (!canonical) {
      return;
    }

    nsAutoCString keyName;
    keyName = ToCharPtr(canonical);
    ToLowerCase(keyName);
    FaceInitArray* faceListPtr = nullptr;

    // Same canonical family name as the last one? Definitely no need to add a
    // new family record.
    if (FcStrCmp(canonical, aLastFamilyName) == 0) {
      faceListPtr = faces.Get(keyName);
      MOZ_ASSERT(faceListPtr);
    } else {
      aLastFamilyName = canonical;
      aFamilyName = ToCharPtr(canonical);

      // Add new family record if one doesn't already exist.
      faceListPtr =
          faces
              .LookupOrInsertWith(
                  keyName,
                  [&] {
                    FontVisibility visibility =
                        aAppFont ? FontVisibility::Base
                                 : GetVisibilityForFamily(keyName);
                    families.AppendElement(fontlist::Family::InitData(
                        keyName, aFamilyName, fontlist::Family::kNoIndex,
                        visibility,
                        /*bundled*/ aAppFont, /*badUnderline*/ false));

                    return MakeUnique<FaceInitArray>();
                  })
              .get();
    }

    char* s = (char*)FcNameUnparse(aPattern);
    nsAutoCString descriptor(s);
    free(s);

    if (fcCharsetParseBug) {
      // Escape any leading space in charset to work around FcNameParse bug.
      int32_t index = descriptor.Find(":charset= ");
      if (index != kNotFound) {
        // insert backslash after the =, before the space
        descriptor.Insert('\\', index + 9);
      }
    }

    WeightRange weight(FontWeight::Normal());
    StretchRange stretch(FontStretch::Normal());
    SlantStyleRange style(FontSlantStyle::Normal());
    GetFontProperties(aPattern, &weight, &stretch, &style);

    faceListPtr->AppendElement(
        fontlist::Face::InitData{descriptor, 0, false, weight, stretch, style});
    // map the psname, fullname ==> font family for local font lookups
    nsAutoCString psname, fullname;
    GetFaceNames(aPattern, aFamilyName, psname, fullname);
    if (!psname.IsEmpty()) {
      ToLowerCase(psname);
      mLocalNameTable.InsertOrUpdate(
          psname, fontlist::LocalFaceRec::InitData(keyName, descriptor));
    }
    if (!fullname.IsEmpty()) {
      ToLowerCase(fullname);
      if (fullname != psname) {
        mLocalNameTable.InsertOrUpdate(
            fullname, fontlist::LocalFaceRec::InitData(keyName, descriptor));
      }
    }

    // Add entries for any other localized family names. (Most fonts only have
    // a single family name, so the first call to GetString will usually fail).
    FcChar8* otherName;
    int n = (cIndex == 0 ? 1 : 0);
    while (FcPatternGetString(aPattern, FC_FAMILY, n, &otherName) ==
           FcResultMatch) {
      nsAutoCString otherFamilyName(ToCharPtr(otherName));
      keyName = otherFamilyName;
      ToLowerCase(keyName);

      faces
          .LookupOrInsertWith(
              keyName,
              [&] {
                FontVisibility visibility =
                    aAppFont ? FontVisibility::Base
                             : GetVisibilityForFamily(keyName);
                families.AppendElement(fontlist::Family::InitData(
                    keyName, otherFamilyName, fontlist::Family::kNoIndex,
                    visibility,
                    /*bundled*/ aAppFont, /*badUnderline*/ false));

                return MakeUnique<FaceInitArray>();
              })
          ->AppendElement(fontlist::Face::InitData{descriptor, 0, false, weight,
                                                   stretch, style});

      n++;
      if (n == int(cIndex)) {
        n++;  // skip over canonical name
      }
    }
  };

  auto addFontSetFamilies = [&addPattern](FcFontSet* aFontSet,
                                          SandboxPolicy* aPolicy,
                                          bool aAppFonts) -> void {
    FcChar8* lastFamilyName = (FcChar8*)"";
    RefPtr<gfxFontconfigFontFamily> fontFamily;
    nsAutoCString familyName;
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

#if defined(MOZ_CONTENT_SANDBOX) && defined(XP_LINUX)
      // Skip any fonts that will be blocked by the content-process sandbox
      // policy.
      if (aPolicy && !(aPolicy->Lookup(reinterpret_cast<const char*>(path)) &
                       SandboxBroker::Perms::MAY_READ)) {
        continue;
      }
#endif

      addPattern(pattern, lastFamilyName, familyName, aAppFonts);
    }
  };

  // iterate over available fonts
  FcFontSet* systemFonts = FcConfigGetFonts(nullptr, FcSetSystem);
  addFontSetFamilies(systemFonts, policy.get(), /* aAppFonts = */ false);

#ifdef MOZ_BUNDLED_FONTS
  if (StaticPrefs::gfx_bundled_fonts_activate_AtStartup() != 0) {
    FcFontSet* appFonts = FcConfigGetFonts(nullptr, FcSetApplication);
    addFontSetFamilies(appFonts, policy.get(), /* aAppFonts = */ true);
  }
#endif

  mozilla::fontlist::FontList* list = SharedFontList();
  list->SetFamilyNames(families);

  for (uint32_t i = 0; i < families.Length(); i++) {
    list->Families()[i].AddFaces(list, *faces.Get(families[i].mKey));
  }
}

gfxFcPlatformFontList::DistroID gfxFcPlatformFontList::GetDistroID() const {
  // Helper called to initialize sResult the first time this is used.
  auto getDistroID = []() {
    DistroID result = DistroID::Unknown;
    FILE* fp = fopen("/etc/os-release", "r");
    if (fp) {
      char buf[512];
      while (fgets(buf, sizeof(buf), fp)) {
        if (strncmp(buf, "ID=", 3) == 0) {
          if (strncmp(buf + 3, "ubuntu", 6) == 0) {
            result = DistroID::Ubuntu;
          } else if (strncmp(buf + 3, "fedora", 6) == 0) {
            result = DistroID::Fedora;
          }
          break;
        }
      }
      fclose(fp);
    }
    return result;
  };
  static DistroID sResult = getDistroID();
  return sResult;
}

FontVisibility gfxFcPlatformFontList::GetVisibilityForFamily(
    const nsACString& aName) const {
  switch (GetDistroID()) {
    case DistroID::Ubuntu:
      if (FamilyInList(aName, kBaseFonts_Ubuntu_20_04,
                       ArrayLength(kBaseFonts_Ubuntu_20_04))) {
        return FontVisibility::Base;
      }
      if (FamilyInList(aName, kLangFonts_Ubuntu_20_04,
                       ArrayLength(kLangFonts_Ubuntu_20_04))) {
        return FontVisibility::LangPack;
      }
      return FontVisibility::User;
    case DistroID::Fedora:
      if (FamilyInList(aName, kBaseFonts_Fedora_32,
                       ArrayLength(kBaseFonts_Fedora_32))) {
        return FontVisibility::Base;
      }
      return FontVisibility::User;
    default:
      // We don't know how to categorize fonts on this system
      return FontVisibility::Unknown;
  }
}

gfxFontEntry* gfxFcPlatformFontList::CreateFontEntry(
    fontlist::Face* aFace, const fontlist::Family* aFamily) {
  nsAutoCString desc(aFace->mDescriptor.AsString(SharedFontList()));
  FcPattern* pattern = FcNameParse((const FcChar8*)desc.get());
  auto* fe = new gfxFontconfigFontEntry(desc, pattern, true);
  FcPatternDestroy(pattern);
  fe->InitializeFrom(aFace, aFamily);
  return fe;
}

// For displaying the fontlist in UI, use explicit call to FcFontList. Using
// FcFontList results in the list containing the localized names as dictated
// by system defaults.
static void GetSystemFontList(nsTArray<nsString>& aListOfFonts,
                              nsAtom* aLangGroup) {
  aListOfFonts.Clear();

  RefPtr<FcPattern> pat = dont_AddRef(FcPatternCreate());
  if (!pat) {
    return;
  }

  UniquePtr<FcObjectSet> os(FcObjectSetBuild(FC_FAMILY, nullptr));
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

  UniquePtr<FcFontSet> fs(FcFontList(nullptr, pat, os.get()));
  if (!fs) {
    return;
  }

  for (int i = 0; i < fs->nfont; i++) {
    char* family;

    if (FcPatternGetString(fs->fonts[i], FC_FAMILY, 0, (FcChar8**)&family) !=
        FcResultMatch) {
      continue;
    }

    // Remove duplicates...
    nsAutoString strFamily;
    AppendUTF8toUTF16(MakeStringSpan(family), strFamily);
    if (aListOfFonts.Contains(strFamily)) {
      continue;
    }

    aListOfFonts.AppendElement(strFamily);
  }

  aListOfFonts.Sort();
}

void gfxFcPlatformFontList::GetFontList(nsAtom* aLangGroup,
                                        const nsACString& aGenericFamily,
                                        nsTArray<nsString>& aListOfFonts) {
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
    MOZ_ASSERT_UNREACHABLE("unexpected CSS generic font family");

  // The first in the list becomes the default in
  // FontBuilder.readFontSelection() if the preference-selected font is not
  // available, so put system configured defaults first.
  if (monospace) aListOfFonts.InsertElementAt(0, u"monospace"_ns);
  if (sansSerif) aListOfFonts.InsertElementAt(0, u"sans-serif"_ns);
  if (serif) aListOfFonts.InsertElementAt(0, u"serif"_ns);
}

FontFamily gfxFcPlatformFontList::GetDefaultFontForPlatform(
    const gfxFontStyle* aStyle, nsAtom* aLanguage) {
  // Get the default font by using a fake name to retrieve the first
  // scalable font that fontconfig suggests for the given language.
  PrefFontList* prefFonts = FindGenericFamilies(
      "-moz-default"_ns, aLanguage ? aLanguage : nsGkAtoms::x_western);
  NS_ASSERTION(prefFonts, "null list of generic fonts");
  if (prefFonts && !prefFonts->IsEmpty()) {
    return (*prefFonts)[0];
  }
  return FontFamily();
}

gfxFontEntry* gfxFcPlatformFontList::LookupLocalFont(
    const nsACString& aFontName, WeightRange aWeightForEntry,
    StretchRange aStretchForEntry, SlantStyleRange aStyleForEntry) {
  nsAutoCString keyName(aFontName);
  ToLowerCase(keyName);

  if (SharedFontList()) {
    return LookupInSharedFaceNameList(aFontName, aWeightForEntry,
                                      aStretchForEntry, aStyleForEntry);
  }

  // if name is not in the global list, done
  const auto fontPattern = mLocalNames.Lookup(keyName);
  if (!fontPattern) {
    return nullptr;
  }

  return new gfxFontconfigFontEntry(aFontName, *fontPattern, aWeightForEntry,
                                    aStretchForEntry, aStyleForEntry);
}

gfxFontEntry* gfxFcPlatformFontList::MakePlatformFont(
    const nsACString& aFontName, WeightRange aWeightForEntry,
    StretchRange aStretchForEntry, SlantStyleRange aStyleForEntry,
    const uint8_t* aFontData, uint32_t aLength) {
  RefPtr<FTUserFontData> ufd = new FTUserFontData(aFontData, aLength);
  RefPtr<SharedFTFace> face = ufd->CloneFace();
  if (!face) {
    return nullptr;
  }
  return new gfxFontconfigFontEntry(aFontName, aWeightForEntry,
                                    aStretchForEntry, aStyleForEntry,
                                    std::move(face));
}

bool gfxFcPlatformFontList::FindAndAddFamilies(
    StyleGenericFontFamily aGeneric, const nsACString& aFamily,
    nsTArray<FamilyAndGeneric>* aOutput, FindFamiliesFlags aFlags,
    gfxFontStyle* aStyle, nsAtom* aLanguage, gfxFloat aDevToCssSize) {
  nsAutoCString familyName(aFamily);
  ToLowerCase(familyName);

  if (!(aFlags & FindFamiliesFlags::eQuotedFamilyName)) {
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
      PrefFontList* prefFonts = FindGenericFamilies(familyName, aLanguage);
      if (prefFonts && !prefFonts->IsEmpty()) {
        aOutput->AppendElements(*prefFonts);
        return true;
      }
      return false;
    }
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
  //   Helvetica, serif ==> Helvetica, TeX Gyre Heros, Nimbus Sans L, DejaVu
  //   Serif
  //
  // In this case fontconfig is including Tex Gyre Heros and
  // Nimbus Sans L as alternatives for Helvetica.

  // Because the FcConfigSubstitute call is quite expensive, we cache the
  // actual font families found via this process. So check the cache first:
  if (auto cachedFamilies = mFcSubstituteCache.Lookup(familyName)) {
    if (cachedFamilies->IsEmpty()) {
      return false;
    }
    aOutput->AppendElements(*cachedFamilies);
    return true;
  }

  // It wasn't in the cache, so we need to ask fontconfig...
  const FcChar8* kSentinelName = ToFcChar8Ptr("-moz-sentinel");
  FcChar8* sentinelFirstFamily = nullptr;
  RefPtr<FcPattern> sentinelSubst = dont_AddRef(FcPatternCreate());
  FcPatternAddString(sentinelSubst, FC_FAMILY, kSentinelName);
  FcConfigSubstitute(nullptr, sentinelSubst, FcMatchPattern);
  FcPatternGetString(sentinelSubst, FC_FAMILY, 0, &sentinelFirstFamily);

  // substitutions for font, -moz-sentinel pattern
  RefPtr<FcPattern> fontWithSentinel = dont_AddRef(FcPatternCreate());
  FcPatternAddString(fontWithSentinel, FC_FAMILY,
                     ToFcChar8Ptr(familyName.get()));
  FcPatternAddString(fontWithSentinel, FC_FAMILY, kSentinelName);
  FcConfigSubstitute(nullptr, fontWithSentinel, FcMatchPattern);

  // Add all font family matches until reaching the sentinel.
  AutoTArray<FamilyAndGeneric, 10> cachedFamilies;
  FcChar8* substName = nullptr;
  for (int i = 0; FcPatternGetString(fontWithSentinel, FC_FAMILY, i,
                                     &substName) == FcResultMatch;
       i++) {
    if (sentinelFirstFamily && FcStrCmp(substName, sentinelFirstFamily) == 0) {
      break;
    }
    gfxPlatformFontList::FindAndAddFamilies(
        aGeneric, nsDependentCString(ToCharPtr(substName)), &cachedFamilies,
        aFlags, aStyle, aLanguage);
  }

  // Cache the resulting list, so we don't have to do this again.
  const auto& insertedCachedFamilies =
      mFcSubstituteCache.InsertOrUpdate(familyName, std::move(cachedFamilies));

  if (insertedCachedFamilies.IsEmpty()) {
    return false;
  }
  aOutput->AppendElements(insertedCachedFamilies);
  return true;
}

bool gfxFcPlatformFontList::GetStandardFamilyName(const nsCString& aFontName,
                                                  nsACString& aFamilyName) {
  aFamilyName.Truncate();

  // The fontconfig list of fonts includes generic family names in the
  // font list. For these, just use the generic name.
  if (aFontName.EqualsLiteral("serif") ||
      aFontName.EqualsLiteral("sans-serif") ||
      aFontName.EqualsLiteral("monospace")) {
    aFamilyName.Assign(aFontName);
    return true;
  }

  RefPtr<FcPattern> pat = dont_AddRef(FcPatternCreate());
  if (!pat) {
    return true;
  }

  UniquePtr<FcObjectSet> os(FcObjectSetBuild(FC_FAMILY, nullptr));
  if (!os) {
    return true;
  }

  // add the family name to the pattern
  FcPatternAddString(pat, FC_FAMILY, ToFcChar8Ptr(aFontName.get()));

  UniquePtr<FcFontSet> givenFS(FcFontList(nullptr, pat, os.get()));
  if (!givenFS) {
    return true;
  }

  // See if there is a font face with first family equal to the given family
  // (needs to be in sync with names coming from GetFontList())
  nsTArray<nsCString> candidates;
  for (int i = 0; i < givenFS->nfont; i++) {
    char* firstFamily;

    if (FcPatternGetString(givenFS->fonts[i], FC_FAMILY, 0,
                           (FcChar8**)&firstFamily) != FcResultMatch) {
      continue;
    }

    nsDependentCString first(firstFamily);
    if (!candidates.Contains(first)) {
      candidates.AppendElement(first);

      if (aFontName.Equals(first)) {
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
    FcPatternAddString(pat, FC_FAMILY, (FcChar8*)candidates[j].get());

    UniquePtr<FcFontSet> candidateFS(FcFontList(nullptr, pat, os.get()));
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
      aFamilyName = candidates[j];
      return true;
    }
  }

  // didn't find localized name, leave family name blank
  return true;
}

void gfxFcPlatformFontList::AddGenericFonts(
    StyleGenericFontFamily aGenericType, nsAtom* aLanguage,
    nsTArray<FamilyAndGeneric>& aFamilyList) {
  bool usePrefFontList = false;

  const char* generic = GetGenericName(aGenericType);
  NS_ASSERTION(generic, "weird generic font type");
  if (!generic) {
    return;
  }

  // By default, most font prefs on Linux map to "use fontconfig"
  // keywords. So only need to explicitly lookup font pref if
  // non-default settings exist
  nsAutoCString genericToLookup(generic);
  if ((!mAlwaysUseFontconfigGenerics && aLanguage) ||
      aLanguage == nsGkAtoms::x_math) {
    nsAtom* langGroup = GetLangGroup(aLanguage);
    nsAutoString fontlistValue;
    Preferences::GetString(NamePref(generic, langGroup).get(), fontlistValue);
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
        genericToLookup.Truncate();
        AppendUTF16toUTF8(fontlistValue, genericToLookup);
      }
    }
  }

  // when pref fonts exist, use standard pref font lookup
  if (usePrefFontList) {
    return gfxPlatformFontList::AddGenericFonts(aGenericType, aLanguage,
                                                aFamilyList);
  }

  PrefFontList* prefFonts = FindGenericFamilies(genericToLookup, aLanguage);
  NS_ASSERTION(prefFonts, "null generic font list");
  aFamilyList.SetCapacity(aFamilyList.Length() + prefFonts->Length());
  for (auto& f : *prefFonts) {
    aFamilyList.AppendElement(FamilyAndGeneric(f, aGenericType));
  }
}

void gfxFcPlatformFontList::ClearLangGroupPrefFonts() {
  ClearGenericMappings();
  gfxPlatformFontList::ClearLangGroupPrefFonts();
  mAlwaysUseFontconfigGenerics = PrefFontListsUseOnlyGenerics();
}

gfxPlatformFontList::PrefFontList* gfxFcPlatformFontList::FindGenericFamilies(
    const nsCString& aGeneric, nsAtom* aLanguage) {
  // set up name
  nsAutoCString fcLang;
  GetSampleLangForGroup(aLanguage, fcLang);
  ToLowerCase(fcLang);

  nsAutoCString genericLang(aGeneric);
  if (fcLang.Length() > 0) {
    genericLang.Append('-');
  }
  genericLang.Append(fcLang);

  // try to get the family from the cache
  return mGenericMappings.WithEntryHandle(
      genericLang, [&](auto&& entry) -> PrefFontList* {
        if (!entry) {
          // if not found, ask fontconfig to pick the appropriate font
          RefPtr<FcPattern> genericPattern = dont_AddRef(FcPatternCreate());
          FcPatternAddString(genericPattern, FC_FAMILY,
                             ToFcChar8Ptr(aGeneric.get()));

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
          UniquePtr<FcFontSet> faces(
              FcFontSort(nullptr, genericPattern, FcFalse, nullptr, &result));

          if (!faces) {
            return nullptr;
          }

          // -- select the fonts to be used for the generic
          auto prefFonts = MakeUnique<PrefFontList>();  // can be empty but in
                                                        // practice won't happen
          uint32_t limit =
              gfxPlatformGtk::GetPlatform()->MaxGenericSubstitions();
          bool foundFontWithLang = false;
          for (int i = 0; i < faces->nfont; i++) {
            FcPattern* font = faces->fonts[i];
            FcChar8* mappedGeneric = nullptr;

            FcPatternGetString(font, FC_FAMILY, 0, &mappedGeneric);
            if (mappedGeneric) {
              nsAutoCString mappedGenericName(ToCharPtr(mappedGeneric));
              AutoTArray<FamilyAndGeneric, 1> genericFamilies;
              if (gfxPlatformFontList::FindAndAddFamilies(
                      StyleGenericFontFamily::None, mappedGenericName,
                      &genericFamilies, FindFamiliesFlags(0))) {
                MOZ_ASSERT(genericFamilies.Length() == 1,
                           "expected a single family");
                if (!prefFonts->Contains(genericFamilies[0].mFamily)) {
                  prefFonts->AppendElement(genericFamilies[0].mFamily);
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

          entry.Insert(std::move(prefFonts));
        }
        return entry->get();
      });
}

bool gfxFcPlatformFontList::PrefFontListsUseOnlyGenerics() {
  static const char kFontNamePrefix[] = "font.name.";

  bool prefFontsUseOnlyGenerics = true;
  nsTArray<nsCString> names;
  nsresult rv =
      Preferences::GetRootBranch()->GetChildList(kFontNamePrefix, names);
  if (NS_SUCCEEDED(rv)) {
    for (auto& name : names) {
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

      nsDependentCSubstring prefName =
          Substring(name, ArrayLength(kFontNamePrefix) - 1);
      nsCCharSeparatedTokenizer tokenizer(prefName, '.');
      const nsDependentCSubstring& generic = tokenizer.nextToken();
      const nsDependentCSubstring& langGroup = tokenizer.nextToken();
      nsAutoCString fontPrefValue;
      Preferences::GetCString(name.get(), fontPrefValue);
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
  }
  return prefFontsUseOnlyGenerics;
}

/* static */
void gfxFcPlatformFontList::CheckFontUpdates(nsITimer* aTimer, void* aThis) {
  // A content process is not supposed to check this directly;
  // it will be notified by the parent when the font list changes.
  MOZ_ASSERT(XRE_IsParentProcess());

  // check for font updates
  FcInitBringUptoDate();

  // update fontlist if current config changed
  gfxFcPlatformFontList* pfl = static_cast<gfxFcPlatformFontList*>(aThis);
  FcConfig* current = FcConfigGetCurrent();
  if (current != pfl->GetLastConfig()) {
    pfl->UpdateFontList();
    pfl->ForceGlobalReflow();

    mozilla::dom::ContentParent::NotifyUpdatedFonts(true);
  }
}

gfxFontFamily* gfxFcPlatformFontList::CreateFontFamily(
    const nsACString& aName, FontVisibility aVisibility) const {
  return new gfxFontconfigFontFamily(aName, aVisibility);
}

// mapping of moz lang groups ==> default lang
struct MozLangGroupData {
  nsAtom* const& mozLangGroup;
  const char* defaultLang;
};

const MozLangGroupData MozLangGroups[] = {
    {nsGkAtoms::x_western, "en"},    {nsGkAtoms::x_cyrillic, "ru"},
    {nsGkAtoms::x_devanagari, "hi"}, {nsGkAtoms::x_tamil, "ta"},
    {nsGkAtoms::x_armn, "hy"},       {nsGkAtoms::x_beng, "bn"},
    {nsGkAtoms::x_cans, "iu"},       {nsGkAtoms::x_ethi, "am"},
    {nsGkAtoms::x_geor, "ka"},       {nsGkAtoms::x_gujr, "gu"},
    {nsGkAtoms::x_guru, "pa"},       {nsGkAtoms::x_khmr, "km"},
    {nsGkAtoms::x_knda, "kn"},       {nsGkAtoms::x_mlym, "ml"},
    {nsGkAtoms::x_orya, "or"},       {nsGkAtoms::x_sinh, "si"},
    {nsGkAtoms::x_tamil, "ta"},      {nsGkAtoms::x_telu, "te"},
    {nsGkAtoms::x_tibt, "bo"},       {nsGkAtoms::Unicode, 0}};

bool gfxFcPlatformFontList::TryLangForGroup(const nsACString& aOSLang,
                                            nsAtom* aLangGroup,
                                            nsACString& aFcLang,
                                            bool aForFontEnumerationThread) {
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
    nsAtom* atom = mLangService->LookupLanguage(aFcLang);
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

void gfxFcPlatformFontList::GetSampleLangForGroup(
    nsAtom* aLanguage, nsACString& aLangStr, bool aForFontEnumerationThread) {
  aLangStr.Truncate();
  if (!aLanguage) {
    return;
  }

  // set up lang string
  const MozLangGroupData* mozLangGroup = nullptr;

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
  const char* languages = getenv("LANGUAGE");
  if (languages) {
    const char separator = ':';

    for (const char* pos = languages; true; ++pos) {
      if (*pos == '\0' || *pos == separator) {
        if (languages < pos &&
            TryLangForGroup(Substring(languages, pos), aLanguage, aLangStr,
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
  const char* ctype = setlocale(LC_CTYPE, nullptr);
  if (ctype && TryLangForGroup(nsDependentCString(ctype), aLanguage, aLangStr,
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
void gfxFcPlatformFontList::ActivateBundledFonts() {
  if (!mBundledFontsInitialized) {
    mBundledFontsInitialized = true;
    nsCOMPtr<nsIFile> localDir;
    nsresult rv = NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(localDir));
    if (NS_FAILED(rv)) {
      return;
    }
    if (NS_FAILED(localDir->Append(u"fonts"_ns))) {
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

#  if MOZ_TREE_CAIRO
// Tree cairo symbols have different names.  Disable their activation through
// preprocessor macros.
#    undef cairo_ft_font_options_substitute

// The system cairo functions are not declared because the include paths cause
// the gdk headers to pick up the tree cairo.h.
extern "C" {
NS_VISIBILITY_DEFAULT void cairo_ft_font_options_substitute(
    const cairo_font_options_t* options, FcPattern* pattern);
}
#  endif

static void ApplyGdkScreenFontOptions(FcPattern* aPattern) {
  const cairo_font_options_t* options =
      gdk_screen_get_font_options(gdk_screen_get_default());

  cairo_ft_font_options_substitute(options, aPattern);
}

#endif  // MOZ_WIDGET_GTK

/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"
#include <algorithm>

#include "mozilla/Logging.h"
#include "mozilla/Sprintf.h"

#include "gfxGDIFontList.h"
#include "gfxWindowsPlatform.h"
#include "gfxUserFontSet.h"
#include "gfxFontUtils.h"
#include "gfxGDIFont.h"

#include "nsServiceManagerUtils.h"
#include "nsTArray.h"
#include "nsUnicharUtils.h"

#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsISimpleEnumerator.h"
#include "nsIWindowsRegKey.h"
#include "gfxFontConstants.h"
#include "GeckoProfiler.h"

#include "mozilla/MemoryReporting.h"
#include "mozilla/Telemetry.h"

#include <usp10.h>

using namespace mozilla;
using namespace mozilla::gfx;

#define ROUND(x) floor((x) + 0.5)

#define LOG_FONTLIST(args) \
  MOZ_LOG(gfxPlatform::GetLog(eGfxLog_fontlist), LogLevel::Debug, args)
#define LOG_FONTLIST_ENABLED() \
  MOZ_LOG_TEST(gfxPlatform::GetLog(eGfxLog_fontlist), LogLevel::Debug)

#define LOG_CMAPDATA_ENABLED() \
  MOZ_LOG_TEST(gfxPlatform::GetLog(eGfxLog_cmapdata), LogLevel::Debug)

static __inline void BuildKeyNameFromFontName(nsAString& aName) {
  if (aName.Length() >= LF_FACESIZE) aName.Truncate(LF_FACESIZE - 1);
  ToLowerCase(aName);
}

// Implementation of gfxPlatformFontList for Win32 GDI,
// using GDI font enumeration APIs to get the list of fonts

class WinUserFontData : public gfxUserFontData {
 public:
  explicit WinUserFontData(HANDLE aFontRef) : mFontRef(aFontRef) {}

  virtual ~WinUserFontData() {
    DebugOnly<BOOL> success;
    success = RemoveFontMemResourceEx(mFontRef);
#if DEBUG
    if (!success) {
      char buf[256];
      SprintfLiteral(
          buf,
          "error deleting font handle (%p) - RemoveFontMemResourceEx failed",
          mFontRef);
      NS_ASSERTION(success, buf);
    }
#endif
  }

  HANDLE mFontRef;
};

BYTE FontTypeToOutPrecision(uint8_t fontType) {
  BYTE ret;
  switch (fontType) {
    case GFX_FONT_TYPE_TT_OPENTYPE:
    case GFX_FONT_TYPE_TRUETYPE:
      ret = OUT_TT_ONLY_PRECIS;
      break;
    case GFX_FONT_TYPE_PS_OPENTYPE:
      ret = OUT_PS_ONLY_PRECIS;
      break;
    case GFX_FONT_TYPE_TYPE1:
      ret = OUT_OUTLINE_PRECIS;
      break;
    case GFX_FONT_TYPE_RASTER:
      ret = OUT_RASTER_PRECIS;
      break;
    case GFX_FONT_TYPE_DEVICE:
      ret = OUT_DEVICE_PRECIS;
      break;
    default:
      ret = OUT_DEFAULT_PRECIS;
  }
  return ret;
}

/***************************************************************
 *
 * GDIFontEntry
 *
 */

GDIFontEntry::GDIFontEntry(const nsACString& aFaceName,
                           gfxWindowsFontType aFontType, SlantStyleRange aStyle,
                           WeightRange aWeight, StretchRange aStretch,
                           gfxUserFontData* aUserFontData)
    : gfxFontEntry(aFaceName),
      mFontType(aFontType),
      mForceGDI(false),
      mUnicodeRanges() {
  mUserFontData.reset(aUserFontData);
  mStyleRange = aStyle;
  mWeightRange = aWeight;
  mStretchRange = aStretch;
  if (IsType1()) mForceGDI = true;
  mIsDataUserFont = aUserFontData != nullptr;

  InitLogFont(aFaceName, aFontType);
}

gfxFontEntry* GDIFontEntry::Clone() const {
  MOZ_ASSERT(!IsUserFont(), "we can only clone installed fonts!");
  return new GDIFontEntry(Name(), mFontType, SlantStyle(), Weight(), Stretch(),
                          nullptr);
}

nsresult GDIFontEntry::ReadCMAP(FontInfoData* aFontInfoData) {
  AUTO_PROFILER_LABEL("GDIFontEntry::ReadCMAP", OTHER);

  // attempt this once, if errors occur leave a blank cmap
  if (mCharacterMap) {
    return NS_OK;
  }

  // skip non-SFNT fonts completely
  if (mFontType != GFX_FONT_TYPE_PS_OPENTYPE &&
      mFontType != GFX_FONT_TYPE_TT_OPENTYPE &&
      mFontType != GFX_FONT_TYPE_TRUETYPE) {
    mCharacterMap = new gfxCharacterMap();
    mCharacterMap->mBuildOnTheFly = true;
    return NS_ERROR_FAILURE;
  }

  RefPtr<gfxCharacterMap> charmap;
  nsresult rv;

  if (aFontInfoData &&
      (charmap = GetCMAPFromFontInfo(aFontInfoData, mUVSOffset))) {
    rv = NS_OK;
  } else {
    uint32_t kCMAP = TRUETYPE_TAG('c', 'm', 'a', 'p');
    charmap = new gfxCharacterMap();
    AutoTArray<uint8_t, 16384> cmap;
    rv = CopyFontTable(kCMAP, cmap);

    if (NS_SUCCEEDED(rv)) {
      rv = gfxFontUtils::ReadCMAP(cmap.Elements(), cmap.Length(), *charmap,
                                  mUVSOffset);
    }
  }

  mHasCmapTable = NS_SUCCEEDED(rv);
  if (mHasCmapTable) {
    gfxPlatformFontList* pfl = gfxPlatformFontList::PlatformFontList();
    mCharacterMap = pfl->FindCharMap(charmap);
  } else {
    // if error occurred, initialize to null cmap
    mCharacterMap = new gfxCharacterMap();
    // For fonts where we failed to read the character map,
    // we can take a slow path to look up glyphs character by character
    mCharacterMap->mBuildOnTheFly = true;
  }

  LOG_FONTLIST(("(fontlist-cmap) name: %s, size: %d hash: %8.8x%s\n",
                mName.get(), charmap->SizeOfIncludingThis(moz_malloc_size_of),
                charmap->mHash, mCharacterMap == charmap ? " new" : ""));
  if (LOG_CMAPDATA_ENABLED()) {
    char prefix[256];
    SprintfLiteral(prefix, "(cmapdata) name: %.220s", mName.get());
    charmap->Dump(prefix, eGfxLog_cmapdata);
  }

  return rv;
}

gfxFont* GDIFontEntry::CreateFontInstance(const gfxFontStyle* aFontStyle) {
  return new gfxGDIFont(this, aFontStyle);
}

nsresult GDIFontEntry::CopyFontTable(uint32_t aTableTag,
                                     nsTArray<uint8_t>& aBuffer) {
  if (!IsTrueType()) {
    return NS_ERROR_FAILURE;
  }

  AutoDC dc;
  AutoSelectFont font(dc.GetDC(), &mLogFont);
  if (font.IsValid()) {
    uint32_t tableSize = ::GetFontData(
        dc.GetDC(), NativeEndian::swapToBigEndian(aTableTag), 0, nullptr, 0);
    if (tableSize != GDI_ERROR) {
      if (aBuffer.SetLength(tableSize, fallible)) {
        ::GetFontData(dc.GetDC(), NativeEndian::swapToBigEndian(aTableTag), 0,
                      aBuffer.Elements(), tableSize);
        return NS_OK;
      }
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  return NS_ERROR_FAILURE;
}

already_AddRefed<UnscaledFontGDI> GDIFontEntry::LookupUnscaledFont(
    HFONT aFont) {
  RefPtr<UnscaledFontGDI> unscaledFont(mUnscaledFont);
  if (!unscaledFont) {
    LOGFONT lf;
    GetObject(aFont, sizeof(LOGFONT), &lf);
    unscaledFont = new UnscaledFontGDI(lf);
    mUnscaledFont = unscaledFont;
  }

  return unscaledFont.forget();
}

void GDIFontEntry::FillLogFont(LOGFONTW* aLogFont, LONG aWeight,
                               gfxFloat aSize) {
  memcpy(aLogFont, &mLogFont, sizeof(LOGFONTW));

  aLogFont->lfHeight = (LONG)-ROUND(aSize);

  if (aLogFont->lfHeight == 0) {
    aLogFont->lfHeight = -1;
  }

  // If a non-zero weight is passed in, use this to override the original
  // weight in the entry's logfont. This is used to control synthetic bolding
  // for installed families with no bold face, and for downloaded fonts
  // (but NOT for local user fonts, because it could cause a different,
  // glyph-incompatible face to be used)
  if (aWeight != 0) {
    aLogFont->lfWeight = aWeight;
  }

  // for non-local() user fonts, we never want to apply italics here;
  // if the face is described as italic, we should use it as-is,
  // and if it's not, but then the element is styled italic, we'll use
  // a cairo transform to create fake italic (oblique)
  if (mIsDataUserFont) {
    aLogFont->lfItalic = 0;
  }
}

#define MISSING_GLYPH \
  0x1F  // glyph index returned for missing characters
        // on WinXP with .fon fonts, but not Type1 (.pfb)

bool GDIFontEntry::TestCharacterMap(uint32_t aCh) {
  if (!mCharacterMap) {
    ReadCMAP();
    NS_ASSERTION(mCharacterMap, "failed to initialize a character map");
  }

  if (mCharacterMap->mBuildOnTheFly) {
    if (aCh > 0xFFFF) return false;

    // previous code was using the group style
    gfxFontStyle fakeStyle;
    if (!IsUpright()) {
      fakeStyle.style = FontSlantStyle::Italic();
    }
    fakeStyle.weight = Weight().Min();

    RefPtr<gfxFont> tempFont = FindOrMakeFont(&fakeStyle, nullptr);
    if (!tempFont || !tempFont->Valid()) return false;
    gfxGDIFont* font = static_cast<gfxGDIFont*>(tempFont.get());

    HDC dc = GetDC((HWND) nullptr);
    SetGraphicsMode(dc, GM_ADVANCED);
    HFONT hfont = font->GetHFONT();
    HFONT oldFont = (HFONT)SelectObject(dc, hfont);

    wchar_t str[1] = {(wchar_t)aCh};
    WORD glyph[1];

    bool hasGlyph = false;

    // Bug 573038 - in some cases GetGlyphIndicesW returns 0xFFFF for a
    // missing glyph or 0x1F in other cases to indicate the "invalid"
    // glyph.  Map both cases to "not found"
    if (IsType1() || mForceGDI) {
      // Type1 fonts and uniscribe APIs don't get along.
      // ScriptGetCMap will return E_HANDLE
      DWORD ret =
          GetGlyphIndicesW(dc, str, 1, glyph, GGI_MARK_NONEXISTING_GLYPHS);
      if (ret != GDI_ERROR && glyph[0] != 0xFFFF &&
          (IsType1() || glyph[0] != MISSING_GLYPH)) {
        hasGlyph = true;
      }
    } else {
      // ScriptGetCMap works better than GetGlyphIndicesW
      // for things like bitmap/vector fonts
      SCRIPT_CACHE sc = nullptr;
      HRESULT rv = ScriptGetCMap(dc, &sc, str, 1, 0, glyph);
      if (rv == S_OK) hasGlyph = true;
    }

    SelectObject(dc, oldFont);
    ReleaseDC(nullptr, dc);

    if (hasGlyph) {
      mCharacterMap->set(aCh);
      return true;
    }
  } else {
    // font had a cmap so simply check that
    return mCharacterMap->test(aCh);
  }

  return false;
}

void GDIFontEntry::InitLogFont(const nsACString& aName,
                               gfxWindowsFontType aFontType) {
#define CLIP_TURNOFF_FONTASSOCIATION 0x40

  mLogFont.lfHeight = -1;

  // Fill in logFont structure
  mLogFont.lfWidth = 0;
  mLogFont.lfEscapement = 0;
  mLogFont.lfOrientation = 0;
  mLogFont.lfUnderline = FALSE;
  mLogFont.lfStrikeOut = FALSE;
  mLogFont.lfCharSet = DEFAULT_CHARSET;
  mLogFont.lfOutPrecision = FontTypeToOutPrecision(aFontType);
  mLogFont.lfClipPrecision = CLIP_TURNOFF_FONTASSOCIATION;
  mLogFont.lfQuality = DEFAULT_QUALITY;
  mLogFont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
  // always force lfItalic if we want it.  Font selection code will
  // do its best to give us an italic font entry, but if no face exists
  // it may give us a regular one based on weight.  Windows should
  // do fake italic for us in that case.
  mLogFont.lfItalic = !IsUpright();
  mLogFont.lfWeight = Weight().Min().ToIntRounded();

  NS_ConvertUTF8toUTF16 name(aName);
  int len = std::min<int>(name.Length(), LF_FACESIZE - 1);
  memcpy(&mLogFont.lfFaceName, name.BeginReading(), len * sizeof(char16_t));
  mLogFont.lfFaceName[len] = '\0';
}

GDIFontEntry* GDIFontEntry::CreateFontEntry(const nsACString& aName,
                                            gfxWindowsFontType aFontType,
                                            SlantStyleRange aStyle,
                                            WeightRange aWeight,
                                            StretchRange aStretch,
                                            gfxUserFontData* aUserFontData) {
  // jtdfix - need to set charset, unicode ranges, pitch/family

  GDIFontEntry* fe = new GDIFontEntry(aName, aFontType, aStyle, aWeight,
                                      aStretch, aUserFontData);

  return fe;
}

void GDIFontEntry::AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf,
                                          FontListSizes* aSizes) const {
  aSizes->mFontListSize += aMallocSizeOf(this);
  AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
}

/***************************************************************
 *
 * GDIFontFamily
 *
 */

static bool ShouldIgnoreItalicStyle(const nsACString& aName) {
  // Ignore italic style's "Meiryo" because "Meiryo (Bold) Italic" has
  // non-italic style glyphs as Japanese characters.  However, using it
  // causes serious problem if web pages wants some elements to be
  // different style from others only with font-style.  For example,
  // <em> and <i> should be rendered as italic in the default style.
  return aName.EqualsLiteral("Meiryo") ||
         aName.EqualsLiteral(
             "\xe3\x83\xa1\xe3\x82\xa4\xe3\x83\xaa\xe3\x82\xaa");
}

int CALLBACK GDIFontFamily::FamilyAddStylesProc(
    const ENUMLOGFONTEXW* lpelfe, const NEWTEXTMETRICEXW* nmetrics,
    DWORD fontType, LPARAM data) {
  const NEWTEXTMETRICW& metrics = nmetrics->ntmTm;
  LOGFONTW logFont = lpelfe->elfLogFont;
  GDIFontFamily* ff = reinterpret_cast<GDIFontFamily*>(data);

  if (logFont.lfItalic && ShouldIgnoreItalicStyle(ff->mName)) {
    return 1;
  }

  // Some fonts claim to support things > 900, but we don't so clamp the sizes
  logFont.lfWeight = clamped(logFont.lfWeight, LONG(100), LONG(900));

  gfxWindowsFontType feType =
      GDIFontEntry::DetermineFontType(metrics, fontType);

  GDIFontEntry* fe = nullptr;
  for (uint32_t i = 0; i < ff->mAvailableFonts.Length(); ++i) {
    fe = static_cast<GDIFontEntry*>(ff->mAvailableFonts[i].get());
    if (feType > fe->mFontType) {
      // if the new type is better than the old one, remove the old entries
      ff->mAvailableFonts.RemoveElementAt(i);
      --i;
    } else if (feType < fe->mFontType) {
      // otherwise if the new type is worse, skip it
      return 1;
    }
  }

  for (uint32_t i = 0; i < ff->mAvailableFonts.Length(); ++i) {
    fe = static_cast<GDIFontEntry*>(ff->mAvailableFonts[i].get());
    // check if we already know about this face
    if (fe->Weight().Min() == FontWeight(int32_t(logFont.lfWeight)) &&
        fe->IsItalic() == (logFont.lfItalic == 0xFF)) {
      // update the charset bit here since this could be different
      // XXX Can we still do this now that we store mCharset
      // on the font family rather than the font entry?
      ff->mCharset.set(metrics.tmCharSet);
      return 1;
    }
  }

  // We can't set the hasItalicFace flag correctly here,
  // because we might not have seen the family's italic face(s) yet.
  // So we'll set that flag for all members after loading all the faces.
  auto italicStyle = (logFont.lfItalic == 0xFF ? FontSlantStyle::Italic()
                                               : FontSlantStyle::Normal());
  fe = GDIFontEntry::CreateFontEntry(
      NS_ConvertUTF16toUTF8(lpelfe->elfFullName), feType,
      SlantStyleRange(italicStyle),
      WeightRange(FontWeight(int32_t(logFont.lfWeight))),
      StretchRange(FontStretch::Normal()), nullptr);
  if (!fe) return 1;

  ff->AddFontEntry(fe);

  if (nmetrics->ntmFontSig.fsUsb[0] != 0x00000000 &&
      nmetrics->ntmFontSig.fsUsb[1] != 0x00000000 &&
      nmetrics->ntmFontSig.fsUsb[2] != 0x00000000 &&
      nmetrics->ntmFontSig.fsUsb[3] != 0x00000000) {
    // set the unicode ranges
    uint32_t x = 0;
    for (uint32_t i = 0; i < 4; ++i) {
      DWORD range = nmetrics->ntmFontSig.fsUsb[i];
      for (uint32_t k = 0; k < 32; ++k) {
        fe->mUnicodeRanges.set(x++, (range & (1 << k)) != 0);
      }
    }
  }

  if (LOG_FONTLIST_ENABLED()) {
    LOG_FONTLIST(
        ("(fontlist) added (%s) to family (%s)"
         " with style: %s weight: %d stretch: normal",
         fe->Name().get(), ff->Name().get(),
         (logFont.lfItalic == 0xff) ? "italic" : "normal", logFont.lfWeight));
  }
  return 1;
}

void GDIFontFamily::FindStyleVariations(FontInfoData* aFontInfoData) {
  if (mHasStyles) return;
  mHasStyles = true;

  HDC hdc = GetDC(nullptr);
  SetGraphicsMode(hdc, GM_ADVANCED);

  LOGFONTW logFont;
  memset(&logFont, 0, sizeof(LOGFONTW));
  logFont.lfCharSet = DEFAULT_CHARSET;
  logFont.lfPitchAndFamily = 0;
  NS_ConvertUTF8toUTF16 name(mName);
  uint32_t l = std::min<uint32_t>(name.Length(), LF_FACESIZE - 1);
  memcpy(logFont.lfFaceName, name.get(), l * sizeof(char16_t));

  EnumFontFamiliesExW(hdc, &logFont,
                      (FONTENUMPROCW)GDIFontFamily::FamilyAddStylesProc,
                      (LPARAM)this, 0);
  if (LOG_FONTLIST_ENABLED() && mAvailableFonts.Length() == 0) {
    LOG_FONTLIST(
        ("(fontlist) no styles available in family \"%s\"", mName.get()));
  }

  ReleaseDC(nullptr, hdc);

  if (mIsBadUnderlineFamily) {
    SetBadUnderlineFonts();
  }
}

/***************************************************************
 *
 * gfxGDIFontList
 *
 */

gfxGDIFontList::gfxGDIFontList() : mFontSubstitutes(32) {
#ifdef MOZ_BUNDLED_FONTS
  ActivateBundledFonts();
#endif
}

static void RemoveCharsetFromFontSubstitute(nsAString& aName) {
  int32_t comma = aName.FindChar(char16_t(','));
  if (comma >= 0) aName.Truncate(comma);
}

#define MAX_VALUE_NAME 512
#define MAX_VALUE_DATA 512

nsresult gfxGDIFontList::GetFontSubstitutes() {
  HKEY hKey;
  DWORD i, rv, lenAlias, lenActual, valueType;
  WCHAR aliasName[MAX_VALUE_NAME];
  WCHAR actualName[MAX_VALUE_DATA];

  if (RegOpenKeyExW(
          HKEY_LOCAL_MACHINE,
          L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\FontSubstitutes",
          0, KEY_READ, &hKey) != ERROR_SUCCESS) {
    return NS_ERROR_FAILURE;
  }

  for (i = 0, rv = ERROR_SUCCESS; rv != ERROR_NO_MORE_ITEMS; i++) {
    aliasName[0] = 0;
    lenAlias = ArrayLength(aliasName);
    actualName[0] = 0;
    lenActual = sizeof(actualName);
    rv = RegEnumValueW(hKey, i, aliasName, &lenAlias, nullptr, &valueType,
                       (LPBYTE)actualName, &lenActual);

    if (rv != ERROR_SUCCESS || valueType != REG_SZ || lenAlias == 0) {
      continue;
    }

    if (aliasName[0] == WCHAR('@')) {
      continue;
    }

    nsAutoString substituteName((char16_t*)aliasName);
    nsAutoString actualFontName((char16_t*)actualName);
    RemoveCharsetFromFontSubstitute(substituteName);
    BuildKeyNameFromFontName(substituteName);
    RemoveCharsetFromFontSubstitute(actualFontName);
    BuildKeyNameFromFontName(actualFontName);
    gfxFontFamily* ff;
    NS_ConvertUTF16toUTF8 substitute(substituteName);
    NS_ConvertUTF16toUTF8 actual(actualFontName);
    if (!actual.IsEmpty() && (ff = mFontFamilies.GetWeak(actual))) {
      mFontSubstitutes.Put(substitute, ff);
    } else {
      mNonExistingFonts.AppendElement(substitute);
    }
  }

  // "Courier" on a default Windows install is an ugly bitmap font.
  // If there is no substitution for Courier in the registry
  // substitute "Courier" with "Courier New".
  nsAutoString substituteName;
  substituteName.AssignLiteral("Courier");
  BuildKeyNameFromFontName(substituteName);
  NS_ConvertUTF16toUTF8 substitute(substituteName);
  if (!mFontSubstitutes.GetWeak(substitute)) {
    gfxFontFamily* ff;
    nsAutoString actualFontName;
    actualFontName.AssignLiteral("Courier New");
    BuildKeyNameFromFontName(actualFontName);
    NS_ConvertUTF16toUTF8 actual(actualFontName);
    ff = mFontFamilies.GetWeak(actual);
    if (ff) {
      mFontSubstitutes.Put(substitute, ff);
    }
  }
  return NS_OK;
}

nsresult gfxGDIFontList::InitFontListForPlatform() {
  Telemetry::AutoTimer<Telemetry::GDI_INITFONTLIST_TOTAL> timer;

  mFontSubstitutes.Clear();
  mNonExistingFonts.Clear();

  // iterate over available families
  LOGFONTW logfont;
  memset(&logfont, 0, sizeof(logfont));
  logfont.lfCharSet = DEFAULT_CHARSET;

  AutoDC hdc;
  (void)EnumFontFamiliesExW(hdc.GetDC(), &logfont,
                            (FONTENUMPROCW)&EnumFontFamExProc, 0, 0);

  GetFontSubstitutes();

  GetPrefsAndStartLoader();

  return NS_OK;
}

int CALLBACK gfxGDIFontList::EnumFontFamExProc(ENUMLOGFONTEXW* lpelfe,
                                               NEWTEXTMETRICEXW* lpntme,
                                               DWORD fontType, LPARAM lParam) {
  const NEWTEXTMETRICW& metrics = lpntme->ntmTm;
  const LOGFONTW& lf = lpelfe->elfLogFont;

  if (lf.lfFaceName[0] == '@') {
    return 1;
  }

  nsAutoString name(lf.lfFaceName);
  BuildKeyNameFromFontName(name);

  NS_ConvertUTF16toUTF8 key(name);

  gfxGDIFontList* fontList = PlatformFontList();

  if (!fontList->mFontFamilies.GetWeak(key)) {
    NS_ConvertUTF16toUTF8 faceName(lf.lfFaceName);
    RefPtr<GDIFontFamily> family = new GDIFontFamily(faceName);
    fontList->mFontFamilies.Put(key, family);

    // if locale is such that CJK font names are the default coming from
    // GDI, then if a family name is non-ASCII immediately read in other
    // family names.  This assures that MS Gothic, MS Mincho are all found
    // before lookups begin.
    if (!IsASCII(faceName)) {
      family->ReadOtherFamilyNames(gfxPlatformFontList::PlatformFontList());
    }

    if (fontList->mBadUnderlineFamilyNames.ContainsSorted(key)) {
      family->SetBadUnderlineFamily();
    }

    family->mWindowsFamily = lf.lfPitchAndFamily & 0xF0;
    family->mWindowsPitch = lf.lfPitchAndFamily & 0x0F;

    // mark the charset bit
    family->mCharset.set(metrics.tmCharSet);
  }

  return 1;
}

gfxFontEntry* gfxGDIFontList::LookupLocalFont(const nsACString& aFontName,
                                              WeightRange aWeightForEntry,
                                              StretchRange aStretchForEntry,
                                              SlantStyleRange aStyleForEntry) {
  gfxFontEntry* lookup;

  lookup = LookupInFaceNameLists(aFontName);
  if (!lookup) {
    return nullptr;
  }

  bool isCFF = false;  // jtdfix -- need to determine this

  // use the face name from the lookup font entry, which will be the localized
  // face name which GDI mapping tables use (e.g. with the system locale set to
  // Dutch, a fullname of 'Arial Bold' will find a font entry with the face name
  // 'Arial Vet' which can be used as a key in GDI font lookups).
  GDIFontEntry* fe = GDIFontEntry::CreateFontEntry(
      lookup->Name(),
      gfxWindowsFontType(isCFF ? GFX_FONT_TYPE_PS_OPENTYPE
                               : GFX_FONT_TYPE_TRUETYPE) /*type*/,
      lookup->SlantStyle(), lookup->Weight(), aStretchForEntry, nullptr);

  if (!fe) return nullptr;

  fe->mIsLocalUserFont = true;

  // make the new font entry match the userfont entry style characteristics
  fe->mWeightRange = aWeightForEntry;
  fe->mStyleRange = aStyleForEntry;
  fe->mStretchRange = aStretchForEntry;

  return fe;
}

// If aFontData contains only a MS/Symbol cmap subtable, not MS/Unicode,
// we modify the subtable header to mark it as Unicode instead, because
// otherwise GDI will refuse to load the font.
// NOTE that this function does not bounds-check every access to the font data.
// This is OK because we only use it on data that has already been validated
// by OTS, and therefore we will not hit out-of-bounds accesses here.
static bool FixupSymbolEncodedFont(uint8_t* aFontData, uint32_t aLength) {
  struct CmapHeader {
    AutoSwap_PRUint16 version;
    AutoSwap_PRUint16 numTables;
  };
  struct CmapEncodingRecord {
    AutoSwap_PRUint16 platformID;
    AutoSwap_PRUint16 encodingID;
    AutoSwap_PRUint32 offset;
  };
  const uint32_t kCMAP = TRUETYPE_TAG('c', 'm', 'a', 'p');
  const TableDirEntry* dir = gfxFontUtils::FindTableDirEntry(aFontData, kCMAP);
  if (dir && uint32_t(dir->length) >= sizeof(CmapHeader)) {
    CmapHeader* cmap =
        reinterpret_cast<CmapHeader*>(aFontData + uint32_t(dir->offset));
    CmapEncodingRecord* encRec =
        reinterpret_cast<CmapEncodingRecord*>(cmap + 1);
    int32_t symbolSubtable = -1;
    for (uint32_t i = 0; i < (uint16_t)cmap->numTables; ++i) {
      if (uint16_t(encRec[i].platformID) !=
          gfxFontUtils::PLATFORM_ID_MICROSOFT) {
        continue;  // only interested in MS platform
      }
      if (uint16_t(encRec[i].encodingID) ==
          gfxFontUtils::ENCODING_ID_MICROSOFT_UNICODEBMP) {
        // We've got a Microsoft/Unicode table, so don't interfere.
        symbolSubtable = -1;
        break;
      }
      if (uint16_t(encRec[i].encodingID) ==
          gfxFontUtils::ENCODING_ID_MICROSOFT_SYMBOL) {
        // Found a symbol subtable; remember it for possible fixup,
        // but if we subsequently find a Microsoft/Unicode subtable,
        // we'll cancel this.
        symbolSubtable = i;
      }
    }
    if (symbolSubtable != -1) {
      // We found a windows/symbol cmap table, and no windows/unicode one;
      // change the encoding ID so that AddFontMemResourceEx will accept it
      encRec[symbolSubtable].encodingID =
          gfxFontUtils::ENCODING_ID_MICROSOFT_UNICODEBMP;
      return true;
    }
  }
  return false;
}

gfxFontEntry* gfxGDIFontList::MakePlatformFont(const nsACString& aFontName,
                                               WeightRange aWeightForEntry,
                                               StretchRange aStretchForEntry,
                                               SlantStyleRange aStyleForEntry,
                                               const uint8_t* aFontData,
                                               uint32_t aLength) {
  // MakePlatformFont is responsible for deleting the font data with free
  // so we set up a stack object to ensure it is freed even if we take an
  // early exit
  struct FontDataDeleter {
    explicit FontDataDeleter(const uint8_t* aFontData) : mFontData(aFontData) {}
    ~FontDataDeleter() { free((void*)mFontData); }
    const uint8_t* mFontData;
  };
  FontDataDeleter autoDelete(aFontData);

  bool isCFF = gfxFontUtils::IsCffFont(aFontData);

  nsresult rv;
  HANDLE fontRef = nullptr;

  nsAutoString uniqueName;
  rv = gfxFontUtils::MakeUniqueUserFontName(uniqueName);
  if (NS_FAILED(rv)) return nullptr;

  FallibleTArray<uint8_t> newFontData;

  rv = gfxFontUtils::RenameFont(uniqueName, aFontData, aLength, &newFontData);

  if (NS_FAILED(rv)) return nullptr;

  DWORD numFonts = 0;

  uint8_t* fontData = reinterpret_cast<uint8_t*>(newFontData.Elements());
  uint32_t fontLength = newFontData.Length();
  NS_ASSERTION(fontData, "null font data after renaming");

  // http://msdn.microsoft.com/en-us/library/ms533942(VS.85).aspx
  // "A font that is added by AddFontMemResourceEx is always private
  //  to the process that made the call and is not enumerable."
  fontRef =
      AddFontMemResourceEx(fontData, fontLength, 0 /* reserved */, &numFonts);
  if (!fontRef) {
    if (FixupSymbolEncodedFont(fontData, fontLength)) {
      fontRef = AddFontMemResourceEx(fontData, fontLength, 0, &numFonts);
    }
  }
  if (!fontRef) {
    return nullptr;
  }

  // only load fonts with a single face contained in the data
  // AddFontMemResourceEx generates an additional face name for
  // vertical text if the font supports vertical writing but since
  // the font is referenced via the name this can be ignored
  if (fontRef && numFonts > 2) {
    RemoveFontMemResourceEx(fontRef);
    return nullptr;
  }

  // make a new font entry using the unique name
  WinUserFontData* winUserFontData = new WinUserFontData(fontRef);
  GDIFontEntry* fe = GDIFontEntry::CreateFontEntry(
      NS_ConvertUTF16toUTF8(uniqueName),
      gfxWindowsFontType(isCFF ? GFX_FONT_TYPE_PS_OPENTYPE
                               : GFX_FONT_TYPE_TRUETYPE) /*type*/,
      aStyleForEntry, aWeightForEntry, aStretchForEntry, winUserFontData);

  if (fe) {
    fe->mIsDataUserFont = true;
  }

  return fe;
}

bool gfxGDIFontList::FindAndAddFamilies(StyleGenericFontFamily aGeneric,
                                        const nsACString& aFamily,
                                        nsTArray<FamilyAndGeneric>* aOutput,
                                        FindFamiliesFlags aFlags,
                                        gfxFontStyle* aStyle,
                                        gfxFloat aDevToCssSize) {
  NS_ConvertUTF8toUTF16 key16(aFamily);
  BuildKeyNameFromFontName(key16);
  NS_ConvertUTF16toUTF8 keyName(key16);

  gfxFontFamily* ff = mFontSubstitutes.GetWeak(keyName);
  if (ff) {
    aOutput->AppendElement(FamilyAndGeneric(ff, aGeneric));
    return true;
  }

  if (mNonExistingFonts.Contains(keyName)) {
    return false;
  }

  return gfxPlatformFontList::FindAndAddFamilies(aGeneric, aFamily, aOutput,
                                                 aFlags, aStyle, aDevToCssSize);
}

FontFamily gfxGDIFontList::GetDefaultFontForPlatform(
    const gfxFontStyle* aStyle) {
  FontFamily ff;

  // this really shouldn't fail to find a font....
  NONCLIENTMETRICSW ncm;
  ncm.cbSize = sizeof(ncm);
  BOOL status =
      ::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
  if (status) {
    ff = FindFamily(NS_ConvertUTF16toUTF8(ncm.lfMessageFont.lfFaceName));
    if (!ff.IsNull()) {
      return ff;
    }
  }

  // ...but just in case, try another (long-deprecated) approach as well
  HGDIOBJ hGDI = ::GetStockObject(DEFAULT_GUI_FONT);
  LOGFONTW logFont;
  if (hGDI && ::GetObjectW(hGDI, sizeof(logFont), &logFont)) {
    ff = FindFamily(NS_ConvertUTF16toUTF8(logFont.lfFaceName));
  }

  return ff;
}

void gfxGDIFontList::AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                                            FontListSizes* aSizes) const {
  gfxPlatformFontList::AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
  aSizes->mFontListSize +=
      SizeOfFontFamilyTableExcludingThis(mFontSubstitutes, aMallocSizeOf);
  aSizes->mFontListSize +=
      mNonExistingFonts.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (uint32_t i = 0; i < mNonExistingFonts.Length(); ++i) {
    aSizes->mFontListSize +=
        mNonExistingFonts[i].SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  }
}

void gfxGDIFontList::AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf,
                                            FontListSizes* aSizes) const {
  aSizes->mFontListSize += aMallocSizeOf(this);
  AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
}

// used to load system-wide font info on off-main thread
class GDIFontInfo : public FontInfoData {
 public:
  GDIFontInfo(bool aLoadOtherNames, bool aLoadFaceNames, bool aLoadCmaps)
      : FontInfoData(aLoadOtherNames, aLoadFaceNames, aLoadCmaps) {}

  virtual ~GDIFontInfo() = default;

  virtual void Load() {
    mHdc = GetDC(nullptr);
    SetGraphicsMode(mHdc, GM_ADVANCED);
    FontInfoData::Load();
    ReleaseDC(nullptr, mHdc);
  }

  // loads font data for all members of a given family
  virtual void LoadFontFamilyData(const nsACString& aFamilyName);

  // callback for GDI EnumFontFamiliesExW call
  static int CALLBACK EnumerateFontsForFamily(const ENUMLOGFONTEXW* lpelfe,
                                              const NEWTEXTMETRICEXW* nmetrics,
                                              DWORD fontType, LPARAM data);

  HDC mHdc;
};

struct EnumerateFontsForFamilyData {
  EnumerateFontsForFamilyData(const nsACString& aFamilyName,
                              GDIFontInfo& aFontInfo)
      : mFamilyName(aFamilyName), mFontInfo(aFontInfo) {}

  nsCString mFamilyName;
  nsTArray<nsCString> mOtherFamilyNames;
  GDIFontInfo& mFontInfo;
  nsCString mPreviousFontName;
};

int CALLBACK GDIFontInfo::EnumerateFontsForFamily(
    const ENUMLOGFONTEXW* lpelfe, const NEWTEXTMETRICEXW* nmetrics,
    DWORD fontType, LPARAM data) {
  EnumerateFontsForFamilyData* famData =
      reinterpret_cast<EnumerateFontsForFamilyData*>(data);
  HDC hdc = famData->mFontInfo.mHdc;
  LOGFONTW logFont = lpelfe->elfLogFont;
  const NEWTEXTMETRICW& metrics = nmetrics->ntmTm;

  AutoSelectFont font(hdc, &logFont);
  if (!font.IsValid()) {
    return 1;
  }

  FontFaceData fontData;
  NS_ConvertUTF16toUTF8 fontName(lpelfe->elfFullName);

  // callback called for each style-charset so return if style already seen
  if (fontName.Equals(famData->mPreviousFontName)) {
    return 1;
  }
  famData->mPreviousFontName = fontName;
  famData->mFontInfo.mLoadStats.fonts++;

  // read name table info
  bool nameDataLoaded = false;
  if (famData->mFontInfo.mLoadFaceNames || famData->mFontInfo.mLoadOtherNames) {
    uint32_t kNAME =
        NativeEndian::swapToBigEndian(TRUETYPE_TAG('n', 'a', 'm', 'e'));
    uint32_t nameSize;
    AutoTArray<uint8_t, 1024> nameData;

    nameSize = ::GetFontData(hdc, kNAME, 0, nullptr, 0);
    if (nameSize != GDI_ERROR && nameSize > 0 &&
        nameData.SetLength(nameSize, fallible)) {
      ::GetFontData(hdc, kNAME, 0, nameData.Elements(), nameSize);

      // face names
      if (famData->mFontInfo.mLoadFaceNames) {
        gfxFontUtils::ReadCanonicalName((const char*)(nameData.Elements()),
                                        nameSize, gfxFontUtils::NAME_ID_FULL,
                                        fontData.mFullName);
        gfxFontUtils::ReadCanonicalName(
            (const char*)(nameData.Elements()), nameSize,
            gfxFontUtils::NAME_ID_POSTSCRIPT, fontData.mPostscriptName);
        nameDataLoaded = true;
        famData->mFontInfo.mLoadStats.facenames++;
      }

      // other family names
      if (famData->mFontInfo.mLoadOtherNames) {
        gfxFontUtils::ReadOtherFamilyNamesForFace(
            famData->mFamilyName, (const char*)(nameData.Elements()), nameSize,
            famData->mOtherFamilyNames, false);
      }
    }
  }

  // read cmap
  bool cmapLoaded = false;
  gfxWindowsFontType feType =
      GDIFontEntry::DetermineFontType(metrics, fontType);
  if (famData->mFontInfo.mLoadCmaps && (feType == GFX_FONT_TYPE_PS_OPENTYPE ||
                                        feType == GFX_FONT_TYPE_TT_OPENTYPE ||
                                        feType == GFX_FONT_TYPE_TRUETYPE)) {
    uint32_t kCMAP =
        NativeEndian::swapToBigEndian(TRUETYPE_TAG('c', 'm', 'a', 'p'));
    uint32_t cmapSize;
    AutoTArray<uint8_t, 1024> cmapData;

    cmapSize = ::GetFontData(hdc, kCMAP, 0, nullptr, 0);
    if (cmapSize != GDI_ERROR && cmapSize > 0 &&
        cmapData.SetLength(cmapSize, fallible)) {
      ::GetFontData(hdc, kCMAP, 0, cmapData.Elements(), cmapSize);
      bool cmapLoaded = false;
      RefPtr<gfxCharacterMap> charmap = new gfxCharacterMap();
      uint32_t offset;

      if (NS_SUCCEEDED(gfxFontUtils::ReadCMAP(cmapData.Elements(), cmapSize,
                                              *charmap, offset))) {
        fontData.mCharacterMap = charmap;
        fontData.mUVSOffset = offset;
        cmapLoaded = true;
        famData->mFontInfo.mLoadStats.cmaps++;
      }
    }
  }

  if (cmapLoaded || nameDataLoaded) {
    famData->mFontInfo.mFontFaceData.Put(fontName, fontData);
  }

  return famData->mFontInfo.mCanceled ? 0 : 1;
}

void GDIFontInfo::LoadFontFamilyData(const nsACString& aFamilyName) {
  // iterate over the family
  LOGFONTW logFont;
  memset(&logFont, 0, sizeof(LOGFONTW));
  logFont.lfCharSet = DEFAULT_CHARSET;
  logFont.lfPitchAndFamily = 0;
  NS_ConvertUTF8toUTF16 name(aFamilyName);
  uint32_t l = std::min<uint32_t>(name.Length(), LF_FACESIZE - 1);
  memcpy(logFont.lfFaceName, name.BeginReading(), l * sizeof(char16_t));

  EnumerateFontsForFamilyData data(aFamilyName, *this);

  EnumFontFamiliesExW(mHdc, &logFont,
                      (FONTENUMPROCW)GDIFontInfo::EnumerateFontsForFamily,
                      (LPARAM)(&data), 0);

  // if found other names, insert them
  if (data.mOtherFamilyNames.Length() != 0) {
    mOtherFamilyNames.Put(aFamilyName, data.mOtherFamilyNames);
    mLoadStats.othernames += data.mOtherFamilyNames.Length();
  }
}

already_AddRefed<FontInfoData> gfxGDIFontList::CreateFontInfoData() {
  bool loadCmaps = !UsesSystemFallback() ||
                   gfxPlatform::GetPlatform()->UseCmapsDuringSystemFallback();

  RefPtr<GDIFontInfo> fi =
      new GDIFontInfo(true, NeedFullnamePostscriptNames(), loadCmaps);

  return fi.forget();
}

gfxFontFamily* gfxGDIFontList::CreateFontFamily(const nsACString& aName) const {
  return new GDIFontFamily(aName);
}

#ifdef MOZ_BUNDLED_FONTS

void gfxGDIFontList::ActivateBundledFonts() {
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

  nsCOMPtr<nsIDirectoryEnumerator> e;
  rv = localDir->GetDirectoryEntries(getter_AddRefs(e));
  if (NS_FAILED(rv)) {
    return;
  }

  nsCOMPtr<nsIFile> file;
  while (NS_SUCCEEDED(e->GetNextFile(getter_AddRefs(file))) && file) {
    nsAutoString path;
    if (NS_FAILED(file->GetPath(path))) {
      continue;
    }
    AddFontResourceExW(path.get(), FR_PRIVATE, nullptr);
  }
}

#endif

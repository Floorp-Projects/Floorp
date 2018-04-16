/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/intl/OSPreferences.h"

#include "gfxDWriteFontList.h"
#include "gfxDWriteFonts.h"
#include "nsUnicharUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsCharSeparatedTokenizer.h"
#include "mozilla/Preferences.h"
#include "mozilla/Sprintf.h"
#include "mozilla/Telemetry.h"
#include "nsDirectoryServiceUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsISimpleEnumerator.h"

#include "gfxGDIFontList.h"

#include "nsIWindowsRegKey.h"

#include "harfbuzz/hb.h"

using namespace mozilla;
using namespace mozilla::gfx;
using mozilla::intl::OSPreferences;

#define LOG_FONTLIST(args) MOZ_LOG(gfxPlatform::GetLog(eGfxLog_fontlist), \
                               LogLevel::Debug, args)
#define LOG_FONTLIST_ENABLED() MOZ_LOG_TEST( \
                                   gfxPlatform::GetLog(eGfxLog_fontlist), \
                                   LogLevel::Debug)

#define LOG_FONTINIT(args) MOZ_LOG(gfxPlatform::GetLog(eGfxLog_fontinit), \
                               LogLevel::Debug, args)
#define LOG_FONTINIT_ENABLED() MOZ_LOG_TEST( \
                                   gfxPlatform::GetLog(eGfxLog_fontinit), \
                                   LogLevel::Debug)

#define LOG_CMAPDATA_ENABLED() MOZ_LOG_TEST( \
                                   gfxPlatform::GetLog(eGfxLog_cmapdata), \
                                   LogLevel::Debug)

static __inline void
BuildKeyNameFromFontName(nsAString &aName)
{
    if (aName.Length() >= LF_FACESIZE)
        aName.Truncate(LF_FACESIZE - 1);
    ToLowerCase(aName);
}

////////////////////////////////////////////////////////////////////////////////
// gfxDWriteFontFamily

gfxDWriteFontFamily::~gfxDWriteFontFamily()
{
}

static bool
GetEnglishOrFirstName(nsAString& aName, IDWriteLocalizedStrings* aStrings)
{
    UINT32 englishIdx = 0;
    BOOL exists;
    HRESULT hr = aStrings->FindLocaleName(L"en-us", &englishIdx, &exists);
    if (FAILED(hr)) {
        return false;
    }
    if (!exists) {
        // Use 0 index if english is not found.
        englishIdx = 0;
    }
    AutoTArray<WCHAR, 32> enName;
    UINT32 length;
    hr = aStrings->GetStringLength(englishIdx, &length);
    if (FAILED(hr)) {
        return false;
    }
    if (!enName.SetLength(length + 1, fallible)) {
        // Eeep - running out of memory. Unlikely to end well.
        return false;
    }
    hr = aStrings->GetString(englishIdx, enName.Elements(), length + 1);
    if (FAILED(hr)) {
        return false;
    }
    aName.Assign(enName.Elements());
    return true;
}

static HRESULT
GetDirectWriteFontName(IDWriteFont *aFont, nsAString& aFontName)
{
    HRESULT hr;

    RefPtr<IDWriteLocalizedStrings> names;
    hr = aFont->GetFaceNames(getter_AddRefs(names));
    if (FAILED(hr)) {
        return hr;
    }

    if (!GetEnglishOrFirstName(aFontName, names)) {
        return E_FAIL;
    }

    return S_OK;
}

#define FULLNAME_ID   DWRITE_INFORMATIONAL_STRING_FULL_NAME
#define PSNAME_ID     DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_NAME

// for use in reading postscript or fullname
static HRESULT
GetDirectWriteFaceName(IDWriteFont *aFont,
                       DWRITE_INFORMATIONAL_STRING_ID aWhichName,
                       nsAString& aFontName)
{
    HRESULT hr;

    BOOL exists;
    RefPtr<IDWriteLocalizedStrings> infostrings;
    hr = aFont->GetInformationalStrings(aWhichName, getter_AddRefs(infostrings), &exists);
    if (FAILED(hr) || !exists) {
        return E_FAIL;
    }

    if (!GetEnglishOrFirstName(aFontName, infostrings)) {
        return E_FAIL;
    }

    return S_OK;
}

void
gfxDWriteFontFamily::FindStyleVariations(FontInfoData *aFontInfoData)
{
    HRESULT hr;
    if (mHasStyles) {
        return;
    }
    mHasStyles = true;

    gfxPlatformFontList *fp = gfxPlatformFontList::PlatformFontList();

    bool skipFaceNames = mFaceNamesInitialized ||
                         !fp->NeedFullnamePostscriptNames();
    bool fontInfoShouldHaveFaceNames = !mFaceNamesInitialized &&
                                       fp->NeedFullnamePostscriptNames() &&
                                       aFontInfoData;

    for (UINT32 i = 0; i < mDWFamily->GetFontCount(); i++) {
        RefPtr<IDWriteFont> font;
        hr = mDWFamily->GetFont(i, getter_AddRefs(font));
        if (FAILED(hr)) {
            // This should never happen.
            NS_WARNING("Failed to get existing font from family.");
            continue;
        }

        if (font->GetSimulations() != DWRITE_FONT_SIMULATIONS_NONE) {
            // We don't want these in the font list; we'll apply simulations
            // on the fly when appropriate.
            continue;
        }

        // name
        nsString fullID(mName);
        nsAutoString faceName;
        hr = GetDirectWriteFontName(font, faceName);
        if (FAILED(hr)) {
            continue;
        }
        fullID.Append(' ');
        fullID.Append(faceName);

        // Ignore italic style's "Meiryo" because "Meiryo (Bold) Italic" has
        // non-italic style glyphs as Japanese characters.  However, using it
        // causes serious problem if web pages wants some elements to be
        // different style from others only with font-style.  For example,
        // <em> and <i> should be rendered as italic in the default style.
        if (fullID.EqualsLiteral("Meiryo Italic") ||
            fullID.EqualsLiteral("Meiryo Bold Italic")) {
            continue;
        }

        gfxDWriteFontEntry *fe = new gfxDWriteFontEntry(fullID, font, mIsSystemFontFamily);
        fe->SetForceGDIClassic(mForceGDIClassic);
        AddFontEntry(fe);

        // postscript/fullname if needed
        nsAutoString psname, fullname;
        if (fontInfoShouldHaveFaceNames) {
            aFontInfoData->GetFaceNames(fe->Name(), fullname, psname);
            if (!fullname.IsEmpty()) {
                fp->AddFullname(fe, fullname);
            }
            if (!psname.IsEmpty()) {
                fp->AddPostscriptName(fe, psname);
            }
        } else if (!skipFaceNames) {
            hr = GetDirectWriteFaceName(font, PSNAME_ID, psname);
            if (FAILED(hr)) {
                skipFaceNames = true;
            } else if (psname.Length() > 0) {
                fp->AddPostscriptName(fe, psname);
            }

            hr = GetDirectWriteFaceName(font, FULLNAME_ID, fullname);
            if (FAILED(hr)) {
                skipFaceNames = true;
            } else if (fullname.Length() > 0) {
                fp->AddFullname(fe, fullname);
            }
        }

        if (LOG_FONTLIST_ENABLED()) {
            LOG_FONTLIST(("(fontlist) added (%s) to family (%s)"
                 " with style: %s weight: %g stretch: %d psname: %s fullname: %s",
                 NS_ConvertUTF16toUTF8(fe->Name()).get(),
                 NS_ConvertUTF16toUTF8(Name()).get(),
                 (fe->IsItalic()) ?
                  "italic" : (fe->IsOblique() ? "oblique" : "normal"),
                 fe->Weight().ToFloat(), fe->Stretch(),
                 NS_ConvertUTF16toUTF8(psname).get(),
                 NS_ConvertUTF16toUTF8(fullname).get()));
        }
    }

    // assume that if no error, all postscript/fullnames were initialized
    if (!skipFaceNames) {
        mFaceNamesInitialized = true;
    }

    if (!mAvailableFonts.Length()) {
        NS_WARNING("Family with no font faces in it.");
    }

    if (mIsBadUnderlineFamily) {
        SetBadUnderlineFonts();
    }
}

void
gfxDWriteFontFamily::ReadFaceNames(gfxPlatformFontList *aPlatformFontList,
                                   bool aNeedFullnamePostscriptNames,
                                   FontInfoData *aFontInfoData)
{
    // if all needed names have already been read, skip
    if (mOtherFamilyNamesInitialized &&
        (mFaceNamesInitialized || !aNeedFullnamePostscriptNames)) {
        return;
    }

    // If we've been passed a FontInfoData, we skip the DWrite implementation
    // here and fall back to the generic code which will use that info.
    if (!aFontInfoData) {
        // DirectWrite version of this will try to read
        // postscript/fullnames via DirectWrite API
        FindStyleVariations();
    }

    // fallback to looking up via name table
    if (!mOtherFamilyNamesInitialized || !mFaceNamesInitialized) {
        gfxFontFamily::ReadFaceNames(aPlatformFontList,
                                     aNeedFullnamePostscriptNames,
                                     aFontInfoData);
    }
}

void
gfxDWriteFontFamily::LocalizedName(nsAString &aLocalizedName)
{
    aLocalizedName = Name(); // just return canonical name in case of failure

    if (!mDWFamily) {
        return;
    }

    HRESULT hr;
    nsAutoCString locale;
    // We use system locale here because it's what user expects to see.
    // See bug 1349454 for details.
    OSPreferences::GetInstance()->GetSystemLocale(locale);

    RefPtr<IDWriteLocalizedStrings> names;

    hr = mDWFamily->GetFamilyNames(getter_AddRefs(names));
    if (FAILED(hr)) {
        return;
    }
    UINT32 idx = 0;
    BOOL exists;
    hr = names->FindLocaleName(NS_ConvertUTF8toUTF16(locale).get(),
                               &idx,
                               &exists);
    if (FAILED(hr)) {
        return;
    }
    if (!exists) {
        // Use english is localized is not found.
        hr = names->FindLocaleName(L"en-us", &idx, &exists);
        if (FAILED(hr)) {
            return;
        }
        if (!exists) {
            // Use 0 index if english is not found.
            idx = 0;
        }
    }
    AutoTArray<WCHAR, 32> famName;
    UINT32 length;
    
    hr = names->GetStringLength(idx, &length);
    if (FAILED(hr)) {
        return;
    }
    
    if (!famName.SetLength(length + 1, fallible)) {
        // Eeep - running out of memory. Unlikely to end well.
        return;
    }

    hr = names->GetString(idx, famName.Elements(), length + 1);
    if (FAILED(hr)) {
        return;
    }

    aLocalizedName = nsDependentString(famName.Elements());
}

bool
gfxDWriteFontFamily::IsSymbolFontFamily() const
{
    // Just check the first font in the family
    if (mDWFamily->GetFontCount() > 0) {
        RefPtr<IDWriteFont> font;
        if (SUCCEEDED(mDWFamily->GetFont(0, getter_AddRefs(font)))) {
            return font->IsSymbolFont();
        }
    }
    return false;
}

void
gfxDWriteFontFamily::AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                                            FontListSizes* aSizes) const
{
    gfxFontFamily::AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
    // TODO:
    // This doesn't currently account for |mDWFamily|
}

void
gfxDWriteFontFamily::AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf,
                                            FontListSizes* aSizes) const
{
    aSizes->mFontListSize += aMallocSizeOf(this);
    AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
}

////////////////////////////////////////////////////////////////////////////////
// gfxDWriteFontEntry

gfxFontEntry*
gfxDWriteFontEntry::Clone() const
{
    MOZ_ASSERT(!IsUserFont(), "we can only clone installed fonts!");
    return new gfxDWriteFontEntry(Name(), mFont);
}

gfxDWriteFontEntry::~gfxDWriteFontEntry()
{
}

static bool
UsingArabicOrHebrewScriptSystemLocale()
{
    LANGID langid = PRIMARYLANGID(::GetSystemDefaultLangID());
    switch (langid) {
    case LANG_ARABIC:
    case LANG_DARI:
    case LANG_PASHTO:
    case LANG_PERSIAN:
    case LANG_SINDHI:
    case LANG_UIGHUR:
    case LANG_URDU:
    case LANG_HEBREW:
        return true;
    default:
        return false;
    }
}

nsresult
gfxDWriteFontEntry::CopyFontTable(uint32_t aTableTag,
                                  nsTArray<uint8_t> &aBuffer)
{
    gfxDWriteFontList *pFontList = gfxDWriteFontList::PlatformFontList();
    const uint32_t tagBE = NativeEndian::swapToBigEndian(aTableTag);

    // Don't use GDI table loading for symbol fonts or for
    // italic fonts in Arabic-script system locales because of
    // potential cmap discrepancies, see bug 629386.
    // Ditto for Hebrew, bug 837498.
    if (mFont && pFontList->UseGDIFontTableAccess() &&
        !(mStyle && UsingArabicOrHebrewScriptSystemLocale()) &&
        !mFont->IsSymbolFont())
    {
        LOGFONTW logfont = { 0 };
        if (InitLogFont(mFont, &logfont)) {
            AutoDC dc;
            AutoSelectFont font(dc.GetDC(), &logfont);
            if (font.IsValid()) {
                uint32_t tableSize =
                    ::GetFontData(dc.GetDC(), tagBE, 0, nullptr, 0);
                if (tableSize != GDI_ERROR) {
                    if (aBuffer.SetLength(tableSize, fallible)) {
                        ::GetFontData(dc.GetDC(), tagBE, 0,
                                      aBuffer.Elements(), aBuffer.Length());
                        return NS_OK;
                    }
                    return NS_ERROR_OUT_OF_MEMORY;
                }
            }
        }
    }

    RefPtr<IDWriteFontFace> fontFace;
    nsresult rv = CreateFontFace(getter_AddRefs(fontFace));
    if (NS_FAILED(rv)) {
        return rv;
    }

    uint8_t *tableData;
    uint32_t len;
    void *tableContext = nullptr;
    BOOL exists;
    HRESULT hr =
        fontFace->TryGetFontTable(tagBE, (const void**)&tableData, &len,
                                  &tableContext, &exists);
    if (FAILED(hr) || !exists) {
        return NS_ERROR_FAILURE;
    }

    if (aBuffer.SetLength(len, fallible)) {
        memcpy(aBuffer.Elements(), tableData, len);
        rv = NS_OK;
    } else {
        rv = NS_ERROR_OUT_OF_MEMORY;
    }

    if (tableContext) {
        fontFace->ReleaseFontTable(&tableContext);
    }

    return rv;
}

// Access to font tables packaged in hb_blob_t form

// object attached to the Harfbuzz blob, used to release
// the table when the blob is destroyed
class FontTableRec {
public:
    FontTableRec(IDWriteFontFace *aFontFace, void *aContext)
        : mFontFace(aFontFace), mContext(aContext)
    {
        MOZ_COUNT_CTOR(FontTableRec);
    }

    ~FontTableRec() {
        MOZ_COUNT_DTOR(FontTableRec);
        mFontFace->ReleaseFontTable(mContext);
    }

private:
    RefPtr<IDWriteFontFace> mFontFace;
    void            *mContext;
};

static void
DestroyBlobFunc(void* aUserData)
{
    FontTableRec *ftr = static_cast<FontTableRec*>(aUserData);
    delete ftr;
}

hb_blob_t *
gfxDWriteFontEntry::GetFontTable(uint32_t aTag)
{
    // try to avoid potentially expensive DWrite call if we haven't actually
    // created the font face yet, by using the gfxFontEntry method that will
    // use CopyFontTable and then cache the data
    if (!mFontFace) {
        return gfxFontEntry::GetFontTable(aTag);
    }

    const void *data;
    UINT32      size;
    void       *context;
    BOOL        exists;
    HRESULT hr = mFontFace->TryGetFontTable(NativeEndian::swapToBigEndian(aTag),
                                            &data, &size, &context, &exists);
    if (SUCCEEDED(hr) && exists) {
        FontTableRec *ftr = new FontTableRec(mFontFace, context);
        return hb_blob_create(static_cast<const char*>(data), size,
                              HB_MEMORY_MODE_READONLY,
                              ftr, DestroyBlobFunc);
    }

    return nullptr;
}

nsresult
gfxDWriteFontEntry::ReadCMAP(FontInfoData *aFontInfoData)
{
    AUTO_PROFILER_LABEL("gfxDWriteFontEntry::ReadCMAP", GRAPHICS);

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
        // Bug 969504: exclude U+25B6 from Segoe UI family, because it's used
        // by sites to represent a "Play" icon, but the glyph in Segoe UI Light
        // and Semibold on Windows 7 is too thin. (Ditto for leftward U+25C0.)
        // Fallback to Segoe UI Symbol is preferred.
        if (FamilyName().EqualsLiteral("Segoe UI")) {
            charmap->clear(0x25b6);
            charmap->clear(0x25c0);
        }
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

bool
gfxDWriteFontEntry::HasVariations()
{
    if (mHasVariationsInitialized) {
        return mHasVariations;
    }
    mHasVariationsInitialized = true;
    if (!mFontFace) {
        // CreateFontFace will initialize the mFontFace field, and also
        // mFontFace5 if available on the current DWrite version.
        RefPtr<IDWriteFontFace> fontFace;
        if (NS_FAILED(CreateFontFace(getter_AddRefs(fontFace)))) {
            return false;
        }
    }
    if (mFontFace5) {
        mHasVariations = mFontFace5->HasVariations();
    }
    return mHasVariations;
}

void
gfxDWriteFontEntry::GetVariationAxes(nsTArray<gfxFontVariationAxis>& aAxes)
{
    if (!HasVariations()) {
        return;
    }
    // HasVariations() will have ensured the mFontFace5 interface is available;
    // so we can get an IDWriteFontResource and ask it for the axis info.
    RefPtr<IDWriteFontResource> resource;
    HRESULT hr = mFontFace5->GetFontResource(getter_AddRefs(resource));
    MOZ_ASSERT(SUCCEEDED(hr));

    uint32_t count = resource->GetFontAxisCount();
    AutoTArray<DWRITE_FONT_AXIS_VALUE, 4> defaultValues;
    AutoTArray<DWRITE_FONT_AXIS_RANGE, 4> ranges;
    defaultValues.SetLength(count);
    ranges.SetLength(count);
    resource->GetDefaultFontAxisValues(defaultValues.Elements(), count);
    resource->GetFontAxisRanges(ranges.Elements(), count);
    for (uint32_t i = 0; i < count; ++i) {
        gfxFontVariationAxis axis;
        MOZ_ASSERT(ranges[i].axisTag == defaultValues[i].axisTag);
        DWRITE_FONT_AXIS_ATTRIBUTES attrs = resource->GetFontAxisAttributes(i);
        if (attrs & DWRITE_FONT_AXIS_ATTRIBUTES_HIDDEN) {
            continue;
        }
        if (!(attrs & DWRITE_FONT_AXIS_ATTRIBUTES_VARIABLE)) {
            continue;
        }
        // Extract the 4 chars of the tag from DWrite's packed version,
        // and reassemble them in the order we use for TRUETYPE_TAG.
        uint32_t t = defaultValues[i].axisTag;
        axis.mTag = TRUETYPE_TAG(t & 0xff,
                                 (t >> 8) & 0xff,
                                 (t >> 16) & 0xff,
                                 (t >> 24) & 0xff);
        // Try to get a human-friendly name (may not be present)
        RefPtr<IDWriteLocalizedStrings> names;
        resource->GetAxisNames(i, getter_AddRefs(names));
        if (names) {
            GetEnglishOrFirstName(axis.mName, names);
        }
        axis.mMinValue = ranges[i].minValue;
        axis.mMaxValue = ranges[i].maxValue;
        axis.mDefaultValue = defaultValues[i].value;
        aAxes.AppendElement(axis);
    }
}

void
gfxDWriteFontEntry::GetVariationInstances(
    nsTArray<gfxFontVariationInstance>& aInstances)
{
    gfxFontUtils::GetVariationInstances(this, aInstances);
}

gfxFont *
gfxDWriteFontEntry::CreateFontInstance(const gfxFontStyle* aFontStyle,
                                       bool aNeedsBold)
{
    DWRITE_FONT_SIMULATIONS sims =
        aNeedsBold ? DWRITE_FONT_SIMULATIONS_BOLD : DWRITE_FONT_SIMULATIONS_NONE;
    if (HasVariations() && !aFontStyle->variationSettings.IsEmpty()) {
        // If we need to apply variations, we can't use the cached mUnscaledFont
        // or mUnscaledFontBold here.
        // XXX todo: consider caching a small number of variation instances?
        RefPtr<IDWriteFontFace> fontFace;
        nsresult rv = CreateFontFace(getter_AddRefs(fontFace),
                                     &aFontStyle->variationSettings,
                                     sims);
        if (NS_FAILED(rv)) {
            return nullptr;
        }
        RefPtr<UnscaledFontDWrite> unscaledFont =
            new UnscaledFontDWrite(fontFace, mIsSystemFont ? mFont : nullptr, sims);
        return new gfxDWriteFont(unscaledFont, this, aFontStyle, aNeedsBold);
    }

    ThreadSafeWeakPtr<UnscaledFontDWrite>& unscaledFontPtr =
        aNeedsBold ? mUnscaledFontBold : mUnscaledFont;
    RefPtr<UnscaledFontDWrite> unscaledFont(unscaledFontPtr);
    if (!unscaledFont) {
        RefPtr<IDWriteFontFace> fontFace;
        nsresult rv = CreateFontFace(getter_AddRefs(fontFace), nullptr, sims);
        if (NS_FAILED(rv)) {
            return nullptr;
        }
        unscaledFont =
            new UnscaledFontDWrite(fontFace,
                                   mIsSystemFont ? mFont : nullptr, sims);
        unscaledFontPtr = unscaledFont;
    }
    return new gfxDWriteFont(unscaledFont, this, aFontStyle, aNeedsBold);
}

nsresult
gfxDWriteFontEntry::CreateFontFace(IDWriteFontFace **aFontFace,
                                   const nsTArray<gfxFontVariation>* aVariations,
                                   DWRITE_FONT_SIMULATIONS aSimulations)
{
    // Convert an OpenType font tag from our uint32_t representation
    // (as constructed by TRUETYPE_TAG(...)) to the order DWrite wants.
    auto makeDWriteAxisTag = [](uint32_t aTag) {
        return DWRITE_MAKE_FONT_AXIS_TAG((aTag >> 24) & 0xff,
                                         (aTag >> 16) & 0xff,
                                         (aTag >> 8) & 0xff,
                                          aTag & 0xff);
    };

    // initialize mFontFace if this hasn't been done before
    if (!mFontFace) {
        HRESULT hr;
        if (mFont) {
            hr = mFont->CreateFontFace(getter_AddRefs(mFontFace));
        } else if (mFontFile) {
            IDWriteFontFile *fontFile = mFontFile.get();
            hr = Factory::GetDWriteFactory()->
                CreateFontFace(mFaceType,
                               1,
                               &fontFile,
                               0,
                               DWRITE_FONT_SIMULATIONS_NONE,
                               getter_AddRefs(mFontFace));
        } else {
            NS_NOTREACHED("invalid font entry");
            return NS_ERROR_FAILURE;
        }
        if (FAILED(hr)) {
            return NS_ERROR_FAILURE;
        }
        // Also get the IDWriteFontFace5 interface if we're running on a
        // sufficiently new DWrite version where it is available.
        if (mFontFace) {
            mFontFace->QueryInterface(__uuidof(IDWriteFontFace5),
                (void**)getter_AddRefs(mFontFace5));
            if (!mVariationSettings.IsEmpty()) {
                // If the font entry has variations specified, mFontFace5 will
                // be a distinct face that has the variations applied.
                RefPtr<IDWriteFontResource> resource;
                HRESULT hr =
                    mFontFace5->GetFontResource(getter_AddRefs(resource));
                MOZ_ASSERT(SUCCEEDED(hr));
                AutoTArray<DWRITE_FONT_AXIS_VALUE, 4> fontAxisValues;
                for (const auto& v : mVariationSettings) {
                    DWRITE_FONT_AXIS_VALUE axisValue = {
                        makeDWriteAxisTag(v.mTag),
                        v.mValue
                    };
                    fontAxisValues.AppendElement(axisValue);
                }
                resource->CreateFontFace(mFontFace->GetSimulations(),
                                         fontAxisValues.Elements(),
                                         fontAxisValues.Length(),
                                         getter_AddRefs(mFontFace5));
            }
        }
    }

    // Do we need to modify DWrite simulations from what mFontFace has?
    bool needSimulations =
        (aSimulations & DWRITE_FONT_SIMULATIONS_BOLD) &&
        !(mFontFace->GetSimulations() & DWRITE_FONT_SIMULATIONS_BOLD);

    // If the IDWriteFontFace5 interface is available, we can go via
    // IDWriteFontResource to create a new modified face.
    if (mFontFace5 && (aVariations && !aVariations->IsEmpty() ||
                       needSimulations)) {
        RefPtr<IDWriteFontResource> resource;
        HRESULT hr = mFontFace5->GetFontResource(getter_AddRefs(resource));
        MOZ_ASSERT(SUCCEEDED(hr));
        AutoTArray<DWRITE_FONT_AXIS_VALUE, 4> fontAxisValues;
        if (aVariations) {
            // Merge mVariationSettings and *aVariations if both present
            const nsTArray<gfxFontVariation>* vars;
            AutoTArray<gfxFontVariation,4> mergedSettings;
            if (!aVariations) {
                vars = &mVariationSettings;
            } else  {
                if (mVariationSettings.IsEmpty()) {
                    vars = aVariations;
                } else {
                    gfxFontUtils::MergeVariations(mVariationSettings,
                                                  *aVariations,
                                                  &mergedSettings);
                    vars = &mergedSettings;
                }
            }
            for (const auto& v : *vars) {
                DWRITE_FONT_AXIS_VALUE axisValue = {
                    makeDWriteAxisTag(v.mTag),
                    v.mValue
                };
                fontAxisValues.AppendElement(axisValue);
            }
        }
        IDWriteFontFace5* ff5;
        resource->CreateFontFace(aSimulations,
                                 fontAxisValues.Elements(),
                                 fontAxisValues.Length(),
                                 &ff5);
        if (ff5) {
            *aFontFace = ff5;
        }
        return FAILED(hr) ? NS_ERROR_FAILURE : NS_OK;
    }

    // Do we need to add DWrite simulations to the face?
    if (needSimulations) {
        // if so, we need to return not mFontFace itself but a version that
        // has the Bold simulation - unfortunately, old DWrite doesn't provide
        // a simple API for this
        UINT32 numberOfFiles = 0;
        if (FAILED(mFontFace->GetFiles(&numberOfFiles, nullptr))) {
            return NS_ERROR_FAILURE;
        }
        AutoTArray<IDWriteFontFile*,1> files;
        files.AppendElements(numberOfFiles);
        if (FAILED(mFontFace->GetFiles(&numberOfFiles, files.Elements()))) {
            return NS_ERROR_FAILURE;
        }
        HRESULT hr = Factory::GetDWriteFactory()->
            CreateFontFace(mFontFace->GetType(),
                           numberOfFiles,
                           files.Elements(),
                           mFontFace->GetIndex(),
                           aSimulations,
                           aFontFace);
        for (UINT32 i = 0; i < numberOfFiles; ++i) {
            files[i]->Release();
        }
        return FAILED(hr) ? NS_ERROR_FAILURE : NS_OK;
    }

    // no simulation: we can just add a reference to mFontFace5 (if present)
    // or mFontFace (otherwise) and return that
    if (mFontFace5) {
        *aFontFace = mFontFace5;
    } else {
        *aFontFace = mFontFace;
    }
    (*aFontFace)->AddRef();
    return NS_OK;
}

bool
gfxDWriteFontEntry::InitLogFont(IDWriteFont *aFont, LOGFONTW *aLogFont)
{
    HRESULT hr;

    BOOL isInSystemCollection;
    IDWriteGdiInterop *gdi = 
        gfxDWriteFontList::PlatformFontList()->GetGDIInterop();
    hr = gdi->ConvertFontToLOGFONT(aFont, aLogFont, &isInSystemCollection);
    // If the font is not in the system collection, GDI will be unable to
    // select it and load its tables, so we return false here to indicate
    // failure, and let CopyFontTable fall back to DWrite native methods.
    return (SUCCEEDED(hr) && isInSystemCollection);
}

bool
gfxDWriteFontEntry::IsCJKFont()
{
    if (mIsCJK != UNINITIALIZED_VALUE) {
        return mIsCJK;
    }

    mIsCJK = false;

    const uint32_t kOS2Tag = TRUETYPE_TAG('O','S','/','2');
    hb_blob_t* blob = GetFontTable(kOS2Tag);
    if (!blob) {
        return mIsCJK;
    }
    // |blob| is an owning reference, but is not RAII-managed, so it must be
    // explicitly freed using |hb_blob_destroy| before we return. (Beware of
    // adding any early-return codepaths!)

    uint32_t len;
    const OS2Table* os2 =
        reinterpret_cast<const OS2Table*>(hb_blob_get_data(blob, &len));
    // ulCodePageRange bit definitions for the CJK codepages,
    // from http://www.microsoft.com/typography/otspec/os2.htm#cpr
    const uint32_t CJK_CODEPAGE_BITS =
        (1 << 17) | // codepage 932 - JIS/Japan
        (1 << 18) | // codepage 936 - Chinese (simplified)
        (1 << 19) | // codepage 949 - Korean Wansung
        (1 << 20) | // codepage 950 - Chinese (traditional)
        (1 << 21);  // codepage 1361 - Korean Johab
    if (len >= offsetof(OS2Table, sxHeight)) {
        if ((uint32_t(os2->codePageRange1) & CJK_CODEPAGE_BITS) != 0) {
            mIsCJK = true;
        }
    }
    hb_blob_destroy(blob);

    return mIsCJK;
}

void
gfxDWriteFontEntry::AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                                           FontListSizes* aSizes) const
{
    gfxFontEntry::AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
    // TODO:
    // This doesn't currently account for the |mFont| and |mFontFile| members
}

void
gfxDWriteFontEntry::AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf,
                                           FontListSizes* aSizes) const
{
    aSizes->mFontListSize += aMallocSizeOf(this);
    AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
}

////////////////////////////////////////////////////////////////////////////////
// gfxDWriteFontList

gfxDWriteFontList::gfxDWriteFontList()
    : mForceGDIClassicMaxFontSize(0.0)
{
}

// bug 602792 - CJK systems default to large CJK fonts which cause excessive
//   I/O strain during cold startup due to dwrite caching bugs.  Default to
//   Arial to avoid this.

gfxFontFamily *
gfxDWriteFontList::GetDefaultFontForPlatform(const gfxFontStyle *aStyle)
{
    nsAutoString resolvedName;

    // try Arial first
    gfxFontFamily *ff;
    if ((ff = FindFamily(NS_LITERAL_STRING("Arial")))) {
        return ff;
    }

    // otherwise, use local default
    NONCLIENTMETRICSW ncm;
    ncm.cbSize = sizeof(ncm);
    BOOL status = ::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, 
                                          sizeof(ncm), &ncm, 0);

    if (status) {
        ff = FindFamily(nsDependentString(ncm.lfMessageFont.lfFaceName));
        if (ff) {
            return ff;
        }
    }

    return nullptr;
}

gfxFontEntry *
gfxDWriteFontList::LookupLocalFont(const nsAString& aFontName,
                                   FontWeight aWeight,
                                   uint16_t aStretch,
                                   uint8_t aStyle)
{
    gfxFontEntry *lookup;

    lookup = LookupInFaceNameLists(aFontName);
    if (!lookup) {
        return nullptr;
    }

    gfxDWriteFontEntry* dwriteLookup = static_cast<gfxDWriteFontEntry*>(lookup);
    gfxDWriteFontEntry *fe =
        new gfxDWriteFontEntry(lookup->Name(),
                               dwriteLookup->mFont,
                               aWeight,
                               aStretch,
                               aStyle);
    fe->SetForceGDIClassic(dwriteLookup->GetForceGDIClassic());
    return fe;
}

gfxFontEntry *
gfxDWriteFontList::MakePlatformFont(const nsAString& aFontName,
                                    FontWeight aWeight,
                                    uint16_t aStretch,
                                    uint8_t aStyle,
                                    const uint8_t* aFontData,
                                    uint32_t aLength)
{
    RefPtr<IDWriteFontFileStream> fontFileStream;
    RefPtr<IDWriteFontFile> fontFile;
    HRESULT hr =
      gfxDWriteFontFileLoader::CreateCustomFontFile(aFontData, aLength,
                                                    getter_AddRefs(fontFile),
                                                    getter_AddRefs(fontFileStream));
    free((void*)aFontData);
    if (FAILED(hr)) {
        NS_WARNING("Failed to create custom font file reference.");
        return nullptr;
    }

    nsAutoString uniqueName;
    nsresult rv = gfxFontUtils::MakeUniqueUserFontName(uniqueName);
    if (NS_FAILED(rv)) {
        return nullptr;
    }

    BOOL isSupported;
    DWRITE_FONT_FILE_TYPE fileType;
    UINT32 numFaces;

    gfxDWriteFontEntry *entry = 
        new gfxDWriteFontEntry(uniqueName,
                               fontFile,
                               fontFileStream,
                               aWeight,
                               aStretch,
                               aStyle);

    fontFile->Analyze(&isSupported, &fileType, &entry->mFaceType, &numFaces);
    if (!isSupported || numFaces > 1) {
        // We don't know how to deal with 0 faces either.
        delete entry;
        return nullptr;
    }

    return entry;
}

enum DWriteInitError {
    errGDIInterop = 1,
    errSystemFontCollection = 2,
    errNoFonts = 3
};

nsresult
gfxDWriteFontList::InitFontListForPlatform()
{
    LARGE_INTEGER frequency;          // ticks per second
    LARGE_INTEGER t1, t2, t3, t4, t5; // ticks
    double elapsedTime, upTime;
    char nowTime[256], nowDate[256];

    if (LOG_FONTINIT_ENABLED()) {
        GetTimeFormatA(LOCALE_INVARIANT, TIME_FORCE24HOURFORMAT,
                      nullptr, nullptr, nowTime, 256);
        GetDateFormatA(LOCALE_INVARIANT, 0, nullptr, nullptr, nowDate, 256);
        upTime = (double) GetTickCount();
    }
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&t1); // start

    HRESULT hr;
    mGDIFontTableAccess =
        Preferences::GetBool("gfx.font_rendering.directwrite.use_gdi_table_loading",
                             false);

    mFontSubstitutes.Clear();
    mNonExistingFonts.Clear();

    hr = Factory::GetDWriteFactory()->
        GetGdiInterop(getter_AddRefs(mGDIInterop));
    if (FAILED(hr)) {
        Telemetry::Accumulate(Telemetry::DWRITEFONT_INIT_PROBLEM,
                              uint32_t(errGDIInterop));
        return NS_ERROR_FAILURE;
    }

    QueryPerformanceCounter(&t2); // base-class/interop initialization

    RefPtr<IDWriteFactory> factory =
        Factory::GetDWriteFactory();

    hr = factory->GetSystemFontCollection(getter_AddRefs(mSystemFonts));
    NS_ASSERTION(SUCCEEDED(hr), "GetSystemFontCollection failed!");

    if (FAILED(hr)) {
        Telemetry::Accumulate(Telemetry::DWRITEFONT_INIT_PROBLEM,
                              uint32_t(errSystemFontCollection));
        return NS_ERROR_FAILURE;
    }

    QueryPerformanceCounter(&t3); // system font collection

    GetFontsFromCollection(mSystemFonts);

    // if no fonts found, something is out of whack, bail and use GDI backend
    NS_ASSERTION(mFontFamilies.Count() != 0,
                 "no fonts found in the system fontlist -- holy crap batman!");
    if (mFontFamilies.Count() == 0) {
        Telemetry::Accumulate(Telemetry::DWRITEFONT_INIT_PROBLEM,
                              uint32_t(errNoFonts));
        return NS_ERROR_FAILURE;
    }

    QueryPerformanceCounter(&t4); // iterate over system fonts

#ifdef MOZ_BUNDLED_FONTS
    mBundledFonts = CreateBundledFontsCollection(factory);
    if (mBundledFonts) {
        GetFontsFromCollection(mBundledFonts);
    }
#endif

    mOtherFamilyNamesInitialized = true;
    GetFontSubstitutes();

    // bug 642093 - DirectWrite does not support old bitmap (.fon)
    // font files, but a few of these such as "Courier" and "MS Sans Serif"
    // are frequently specified in shoddy CSS, without appropriate fallbacks.
    // By mapping these to TrueType equivalents, we provide better consistency
    // with both pre-DW systems and with IE9, which appears to do the same.
    GetDirectWriteSubstitutes();

    // bug 551313 - DirectWrite creates a Gill Sans family out of
    // poorly named members of the Gill Sans MT family containing
    // only Ultra Bold weights.  This causes big problems for pages
    // using Gill Sans which is usually only available on OSX

    nsAutoString nameGillSans(L"Gill Sans");
    nsAutoString nameGillSansMT(L"Gill Sans MT");
    BuildKeyNameFromFontName(nameGillSans);
    BuildKeyNameFromFontName(nameGillSansMT);

    gfxFontFamily *gillSansFamily = mFontFamilies.GetWeak(nameGillSans);
    gfxFontFamily *gillSansMTFamily = mFontFamilies.GetWeak(nameGillSansMT);

    if (gillSansFamily && gillSansMTFamily) {
        gillSansFamily->FindStyleVariations();
        nsTArray<RefPtr<gfxFontEntry> >& faces = gillSansFamily->GetFontList();
        uint32_t i;

        bool allUltraBold = true;
        for (i = 0; i < faces.Length(); i++) {
            // does the face have 'Ultra Bold' in the name?
            if (faces[i]->Name().Find(NS_LITERAL_STRING("Ultra Bold")) == -1) {
                allUltraBold = false;
                break;
            }
        }

        // if all the Gill Sans faces are Ultra Bold ==> move faces
        // for Gill Sans into Gill Sans MT family
        if (allUltraBold) {

            // add faces to Gill Sans MT
            for (i = 0; i < faces.Length(); i++) {
                // change the entry's family name to match its adoptive family
                faces[i]->mFamilyName = gillSansMTFamily->Name();
                gillSansMTFamily->AddFontEntry(faces[i]);

                if (LOG_FONTLIST_ENABLED()) {
                    gfxFontEntry *fe = faces[i];
                    LOG_FONTLIST(("(fontlist) moved (%s) to family (%s)"
                         " with style: %s weight: %g stretch: %d",
                         NS_ConvertUTF16toUTF8(fe->Name()).get(),
                         NS_ConvertUTF16toUTF8(gillSansMTFamily->Name()).get(),
                         (fe->IsItalic()) ?
                          "italic" : (fe->IsOblique() ? "oblique" : "normal"),
                         fe->Weight().ToFloat(), fe->Stretch()));
                }
            }

            // remove Gills Sans
            mFontFamilies.Remove(nameGillSans);
        }
    }

    nsAutoCString classicFamilies;
    nsresult rv = Preferences::GetCString(
      "gfx.font_rendering.cleartype_params.force_gdi_classic_for_families",
      classicFamilies);
    if (NS_SUCCEEDED(rv)) {
        nsCCharSeparatedTokenizer tokenizer(classicFamilies, ',');
        while (tokenizer.hasMoreTokens()) {
            NS_ConvertUTF8toUTF16 name(tokenizer.nextToken());
            BuildKeyNameFromFontName(name);
            gfxFontFamily *family = mFontFamilies.GetWeak(name);
            if (family) {
                static_cast<gfxDWriteFontFamily*>(family)->SetForceGDIClassic(true);
            }
        }
    }
    mForceGDIClassicMaxFontSize =
        Preferences::GetInt("gfx.font_rendering.cleartype_params.force_gdi_classic_max_size",
                            mForceGDIClassicMaxFontSize);

    GetPrefsAndStartLoader();

    QueryPerformanceCounter(&t5); // misc initialization

    if (LOG_FONTINIT_ENABLED()) {
        // determine dwrite version
        nsAutoString dwriteVers;
        gfxWindowsPlatform::GetDLLVersion(L"dwrite.dll", dwriteVers);
        LOG_FONTINIT(("(fontinit) Start: %s %s\n", nowDate, nowTime));
        LOG_FONTINIT(("(fontinit) Uptime: %9.3f s\n", upTime/1000));
        LOG_FONTINIT(("(fontinit) dwrite version: %s\n",
                      NS_ConvertUTF16toUTF8(dwriteVers).get()));
    }

    elapsedTime = (t5.QuadPart - t1.QuadPart) * 1000.0 / frequency.QuadPart;
    Telemetry::Accumulate(Telemetry::DWRITEFONT_DELAYEDINITFONTLIST_TOTAL, elapsedTime);
    Telemetry::Accumulate(Telemetry::DWRITEFONT_DELAYEDINITFONTLIST_COUNT,
                          mSystemFonts->GetFontFamilyCount());
    LOG_FONTINIT((
       "(fontinit) Total time in InitFontList:    %9.3f ms (families: %d, %s)\n",
       elapsedTime, mSystemFonts->GetFontFamilyCount(),
       (mGDIFontTableAccess ? "gdi table access" : "dwrite table access")));

    elapsedTime = (t2.QuadPart - t1.QuadPart) * 1000.0 / frequency.QuadPart;
    LOG_FONTINIT(("(fontinit)  --- base/interop obj initialization init: %9.3f ms\n", elapsedTime));

    elapsedTime = (t3.QuadPart - t2.QuadPart) * 1000.0 / frequency.QuadPart;
    Telemetry::Accumulate(Telemetry::DWRITEFONT_DELAYEDINITFONTLIST_COLLECT, elapsedTime);
    LOG_FONTINIT(("(fontinit)  --- GetSystemFontCollection:  %9.3f ms\n", elapsedTime));

    elapsedTime = (t4.QuadPart - t3.QuadPart) * 1000.0 / frequency.QuadPart;
    LOG_FONTINIT(("(fontinit)  --- iterate over families:    %9.3f ms\n", elapsedTime));

    elapsedTime = (t5.QuadPart - t4.QuadPart) * 1000.0 / frequency.QuadPart;
    LOG_FONTINIT(("(fontinit)  --- misc initialization:    %9.3f ms\n", elapsedTime));

    return NS_OK;
}

void
gfxDWriteFontList::GetFontsFromCollection(IDWriteFontCollection* aCollection)
{
    for (UINT32 i = 0; i < aCollection->GetFontFamilyCount(); i++) {
        RefPtr<IDWriteFontFamily> family;
        aCollection->GetFontFamily(i, getter_AddRefs(family));

        RefPtr<IDWriteLocalizedStrings> names;
        HRESULT hr = family->GetFamilyNames(getter_AddRefs(names));
        if (FAILED(hr)) {
            continue;
        }

        nsAutoString name;
        if (!GetEnglishOrFirstName(name, names)) {
            continue;
        }
        nsAutoString familyName(name); // keep a copy before we lowercase it as a key

        BuildKeyNameFromFontName(name);

        RefPtr<gfxFontFamily> fam;

        if (mFontFamilies.GetWeak(name)) {
            continue;
        }

        fam = new gfxDWriteFontFamily(familyName, family, aCollection == mSystemFonts);
        if (!fam) {
            continue;
        }

        if (mBadUnderlineFamilyNames.Contains(name)) {
            fam->SetBadUnderlineFamily();
        }
        mFontFamilies.Put(name, fam);

        // now add other family name localizations, if present
        uint32_t nameCount = names->GetCount();
        uint32_t nameIndex;

        if (nameCount > 1) {
            UINT32 englishIdx = 0;
            BOOL exists;
            // if this fails/doesn't exist, we'll have used name index 0,
            // so that's the one we'll want to skip here
            names->FindLocaleName(L"en-us", &englishIdx, &exists);

            for (nameIndex = 0; nameIndex < nameCount; nameIndex++) {
                UINT32 nameLen;
                AutoTArray<WCHAR, 32> localizedName;

                // only add other names
                if (nameIndex == englishIdx) {
                    continue;
                }

                hr = names->GetStringLength(nameIndex, &nameLen);
                if (FAILED(hr)) {
                    continue;
                }

                if (!localizedName.SetLength(nameLen + 1, fallible)) {
                    continue;
                }

                hr = names->GetString(nameIndex, localizedName.Elements(),
                                      nameLen + 1);
                if (FAILED(hr)) {
                    continue;
                }

                nsDependentString locName(localizedName.Elements());

                if (!familyName.Equals(locName)) {
                    AddOtherFamilyName(fam, locName);
                }
            }
        }

        // at this point, all family names have been read in
        fam->SetOtherFamilyNamesInitialized();
    }
}

static void
RemoveCharsetFromFontSubstitute(nsAString &aName)
{
    int32_t comma = aName.FindChar(char16_t(','));
    if (comma >= 0)
        aName.Truncate(comma);
}

#define MAX_VALUE_NAME 512
#define MAX_VALUE_DATA 512

nsresult
gfxDWriteFontList::GetFontSubstitutes()
{
    HKEY hKey;
    DWORD i, rv, lenAlias, lenActual, valueType;
    WCHAR aliasName[MAX_VALUE_NAME];
    WCHAR actualName[MAX_VALUE_DATA];

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, 
          L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\FontSubstitutes",
          0, KEY_READ, &hKey) != ERROR_SUCCESS)
    {
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

        nsAutoString substituteName((char16_t*) aliasName);
        nsAutoString actualFontName((char16_t*) actualName);
        RemoveCharsetFromFontSubstitute(substituteName);
        BuildKeyNameFromFontName(substituteName);
        RemoveCharsetFromFontSubstitute(actualFontName);
        BuildKeyNameFromFontName(actualFontName);
        gfxFontFamily *ff;
        if (!actualFontName.IsEmpty() && 
            (ff = mFontFamilies.GetWeak(actualFontName))) {
            mFontSubstitutes.Put(substituteName, ff);
        } else {
            mNonExistingFonts.AppendElement(substituteName);
        }
    }
    return NS_OK;
}

struct FontSubstitution {
    const WCHAR* aliasName;
    const WCHAR* actualName;
};

static const FontSubstitution sDirectWriteSubs[] = {
    { L"MS Sans Serif", L"Microsoft Sans Serif" },
    { L"MS Serif", L"Times New Roman" },
    { L"Courier", L"Courier New" },
    { L"Small Fonts", L"Arial" },
    { L"Roman", L"Times New Roman" },
    { L"Script", L"Mistral" }
};

void
gfxDWriteFontList::GetDirectWriteSubstitutes()
{
    for (uint32_t i = 0; i < ArrayLength(sDirectWriteSubs); ++i) {
        const FontSubstitution& sub(sDirectWriteSubs[i]);
        nsAutoString substituteName((char16_t*)sub.aliasName);
        BuildKeyNameFromFontName(substituteName);
        if (nullptr != mFontFamilies.GetWeak(substituteName)) {
            // don't do the substitution if user actually has a usable font
            // with this name installed
            continue;
        }
        nsAutoString actualFontName((char16_t*)sub.actualName);
        BuildKeyNameFromFontName(actualFontName);
        gfxFontFamily *ff;
        if (nullptr != (ff = mFontFamilies.GetWeak(actualFontName))) {
            mFontSubstitutes.Put(substituteName, ff);
        } else {
            mNonExistingFonts.AppendElement(substituteName);
        }
    }
}

bool
gfxDWriteFontList::GetStandardFamilyName(const nsAString& aFontName,
                                         nsAString& aFamilyName)
{
    gfxFontFamily *family = FindFamily(aFontName);
    if (family) {
        family->LocalizedName(aFamilyName);
        return true;
    }

    return false;
}

bool
gfxDWriteFontList::FindAndAddFamilies(const nsAString& aFamily,
                                      nsTArray<gfxFontFamily*>* aOutput,
                                      FindFamiliesFlags aFlags,
                                      gfxFontStyle* aStyle,
                                      gfxFloat aDevToCssSize)
{
    nsAutoString keyName(aFamily);
    BuildKeyNameFromFontName(keyName);

    gfxFontFamily *ff = mFontSubstitutes.GetWeak(keyName);
    if (ff) {
        aOutput->AppendElement(ff);
        return true;
    }

    if (mNonExistingFonts.Contains(keyName)) {
        return false;
    }

    return gfxPlatformFontList::FindAndAddFamilies(aFamily,
                                                   aOutput,
                                                   aFlags,
                                                   aStyle,
                                                   aDevToCssSize);
}

void
gfxDWriteFontList::AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                                          FontListSizes* aSizes) const
{
    gfxPlatformFontList::AddSizeOfExcludingThis(aMallocSizeOf, aSizes);

    // We are a singleton, so include the font loader singleton's memory.
    MOZ_ASSERT(static_cast<const gfxPlatformFontList*>(this) == gfxPlatformFontList::PlatformFontList());
    gfxDWriteFontFileLoader* loader =
      static_cast<gfxDWriteFontFileLoader*>(gfxDWriteFontFileLoader::Instance());
    aSizes->mLoaderSize += loader->SizeOfIncludingThis(aMallocSizeOf);

    aSizes->mFontListSize +=
        SizeOfFontFamilyTableExcludingThis(mFontSubstitutes, aMallocSizeOf);

    aSizes->mFontListSize +=
        mNonExistingFonts.ShallowSizeOfExcludingThis(aMallocSizeOf);
    for (uint32_t i = 0; i < mNonExistingFonts.Length(); ++i) {
        aSizes->mFontListSize +=
            mNonExistingFonts[i].SizeOfExcludingThisIfUnshared(aMallocSizeOf);
    }
}

void
gfxDWriteFontList::AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf,
                                          FontListSizes* aSizes) const
{
    aSizes->mFontListSize += aMallocSizeOf(this);
    AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
}

static HRESULT GetFamilyName(IDWriteFont *aFont, nsString& aFamilyName)
{
    HRESULT hr;
    RefPtr<IDWriteFontFamily> family;

    // clean out previous value
    aFamilyName.Truncate();

    hr = aFont->GetFontFamily(getter_AddRefs(family));
    if (FAILED(hr)) {
        return hr;
    }

    RefPtr<IDWriteLocalizedStrings> familyNames;

    hr = family->GetFamilyNames(getter_AddRefs(familyNames));
    if (FAILED(hr)) {
        return hr;
    }

    if (!GetEnglishOrFirstName(aFamilyName, familyNames)) {
        return E_FAIL;
    }

    return S_OK;
}

// bug 705594 - the method below doesn't actually do any "drawing", it's only
// used to invoke the DirectWrite layout engine to determine the fallback font
// for a given character.

IFACEMETHODIMP DWriteFontFallbackRenderer::DrawGlyphRun(
    void* clientDrawingContext,
    FLOAT baselineOriginX,
    FLOAT baselineOriginY,
    DWRITE_MEASURING_MODE measuringMode,
    DWRITE_GLYPH_RUN const* glyphRun,
    DWRITE_GLYPH_RUN_DESCRIPTION const* glyphRunDescription,
    IUnknown* clientDrawingEffect
    )
{
    if (!mSystemFonts) {
        return E_FAIL;
    }

    HRESULT hr = S_OK;

    RefPtr<IDWriteFont> font;
    hr = mSystemFonts->GetFontFromFontFace(glyphRun->fontFace,
                                           getter_AddRefs(font));
    if (FAILED(hr)) {
        return hr;
    }

    // copy the family name
    hr = GetFamilyName(font, mFamilyName);
    if (FAILED(hr)) {
        return hr;
    }

    // Arial is used as the default fallback font
    // so if it matches ==> no font found
    if (mFamilyName.EqualsLiteral("Arial")) {
        mFamilyName.Truncate();
        return E_FAIL;
    }
    return hr;
}

gfxFontEntry*
gfxDWriteFontList::PlatformGlobalFontFallback(const uint32_t aCh,
                                              Script aRunScript,
                                              const gfxFontStyle* aMatchStyle,
                                              gfxFontFamily** aMatchedFamily)
{
    HRESULT hr;

    RefPtr<IDWriteFactory> dwFactory =
        Factory::GetDWriteFactory();
    if (!dwFactory) {
        return nullptr;
    }

    // initialize fallback renderer
    if (!mFallbackRenderer) {
        mFallbackRenderer = new DWriteFontFallbackRenderer(dwFactory);
    }

    // initialize text format
    if (!mFallbackFormat) {
        hr = dwFactory->CreateTextFormat(L"Arial", nullptr,
                                         DWRITE_FONT_WEIGHT_REGULAR,
                                         DWRITE_FONT_STYLE_NORMAL,
                                         DWRITE_FONT_STRETCH_NORMAL,
                                         72.0f, L"en-us",
                                         getter_AddRefs(mFallbackFormat));
        if (FAILED(hr)) {
            return nullptr;
        }
    }

    // set up string with fallback character
    wchar_t str[16];
    uint32_t strLen;

    if (IS_IN_BMP(aCh)) {
        str[0] = static_cast<wchar_t> (aCh);
        str[1] = 0;
        strLen = 1;
    } else {
        str[0] = static_cast<wchar_t> (H_SURROGATE(aCh));
        str[1] = static_cast<wchar_t> (L_SURROGATE(aCh));
        str[2] = 0;
        strLen = 2;
    }

    // set up layout
    RefPtr<IDWriteTextLayout> fallbackLayout;

    hr = dwFactory->CreateTextLayout(str, strLen, mFallbackFormat,
                                     200.0f, 200.0f,
                                     getter_AddRefs(fallbackLayout));
    if (FAILED(hr)) {
        return nullptr;
    }

    // call the draw method to invoke the DirectWrite layout functions
    // which determine the fallback font
    hr = fallbackLayout->Draw(nullptr, mFallbackRenderer, 50.0f, 50.0f);
    if (FAILED(hr)) {
        return nullptr;
    }

    gfxFontFamily *family = FindFamily(mFallbackRenderer->FallbackFamilyName());
    if (family) {
        gfxFontEntry *fontEntry;
        bool needsBold;  // ignored in the system fallback case
        fontEntry = family->FindFontForStyle(*aMatchStyle, needsBold);
        if (fontEntry && fontEntry->HasCharacter(aCh)) {
            *aMatchedFamily = family;
            return fontEntry;
        }
        Telemetry::Accumulate(Telemetry::BAD_FALLBACK_FONT, true);
    }

    return nullptr;
}

// used to load system-wide font info on off-main thread
class DirectWriteFontInfo : public FontInfoData {
public:
    DirectWriteFontInfo(bool aLoadOtherNames,
                        bool aLoadFaceNames,
                        bool aLoadCmaps,
                        IDWriteFontCollection* aSystemFonts
#ifdef MOZ_BUNDLED_FONTS
                        , IDWriteFontCollection* aBundledFonts
#endif
                       ) :
        FontInfoData(aLoadOtherNames, aLoadFaceNames, aLoadCmaps)
        , mSystemFonts(aSystemFonts)
#ifdef MOZ_BUNDLED_FONTS
        , mBundledFonts(aBundledFonts)
#endif
    {}

    virtual ~DirectWriteFontInfo() {}

    // loads font data for all members of a given family
    virtual void LoadFontFamilyData(const nsAString& aFamilyName);

private:
    RefPtr<IDWriteFontCollection> mSystemFonts;
#ifdef MOZ_BUNDLED_FONTS
    RefPtr<IDWriteFontCollection> mBundledFonts;
#endif
};

void
DirectWriteFontInfo::LoadFontFamilyData(const nsAString& aFamilyName)
{
    // lookup the family
    AutoTArray<wchar_t, 32> famName;

    uint32_t len = aFamilyName.Length();
    if(!famName.SetLength(len + 1, fallible)) {
        return;
    }
    memcpy(famName.Elements(), aFamilyName.BeginReading(), len * sizeof(char16_t));
    famName[len] = 0;

    HRESULT hr;
    BOOL exists = false;

    uint32_t index;
    RefPtr<IDWriteFontFamily> family;
    hr = mSystemFonts->FindFamilyName(famName.Elements(), &index, &exists);
    if (SUCCEEDED(hr) && exists) {
        mSystemFonts->GetFontFamily(index, getter_AddRefs(family));
        if (!family) {
            return;
        }
    }

#ifdef MOZ_BUNDLED_FONTS
    if (!family && mBundledFonts) {
        hr = mBundledFonts->FindFamilyName(famName.Elements(), &index, &exists);
        if (SUCCEEDED(hr) && exists) {
            mBundledFonts->GetFontFamily(index, getter_AddRefs(family));
        }
    }
#endif

    if (!family) {
        return;
    }

    // later versions of DirectWrite support querying the fullname/psname
    bool loadFaceNamesUsingDirectWrite = mLoadFaceNames;

    for (uint32_t i = 0; i < family->GetFontCount(); i++) {
        // get the font
        RefPtr<IDWriteFont> dwFont;
        hr = family->GetFont(i, getter_AddRefs(dwFont));
        if (FAILED(hr)) {
            // This should never happen.
            NS_WARNING("Failed to get existing font from family.");
            continue;
        }

        if (dwFont->GetSimulations() != DWRITE_FONT_SIMULATIONS_NONE) {
            // We don't want these in the font list; we'll apply simulations
            // on the fly when appropriate.
            continue;
        }

        mLoadStats.fonts++;

        // get the name of the face
        nsString fullID(aFamilyName);
        nsAutoString fontName;
        hr = GetDirectWriteFontName(dwFont, fontName);
        if (FAILED(hr)) {
            continue;
        }
        fullID.Append(' ');
        fullID.Append(fontName);

        FontFaceData fontData;
        bool haveData = true;
        RefPtr<IDWriteFontFace> dwFontFace;

        if (mLoadFaceNames) {
            // try to load using DirectWrite first
            if (loadFaceNamesUsingDirectWrite) {
                hr = GetDirectWriteFaceName(dwFont, PSNAME_ID, fontData.mPostscriptName);
                if (FAILED(hr)) {
                    loadFaceNamesUsingDirectWrite = false;
                }
                hr = GetDirectWriteFaceName(dwFont, FULLNAME_ID, fontData.mFullName);
                if (FAILED(hr)) {
                    loadFaceNamesUsingDirectWrite = false;
                }
            }

            // if DirectWrite read fails, load directly from name table
            if (!loadFaceNamesUsingDirectWrite) {
                hr = dwFont->CreateFontFace(getter_AddRefs(dwFontFace));
                if (SUCCEEDED(hr)) {
                    uint32_t kNAME =
                        NativeEndian::swapToBigEndian(TRUETYPE_TAG('n','a','m','e'));
                    const char *nameData;
                    BOOL exists;
                    void* ctx;
                    uint32_t nameSize;

                    hr = dwFontFace->TryGetFontTable(
                             kNAME,
                             (const void**)&nameData, &nameSize, &ctx, &exists);

                    if (SUCCEEDED(hr) && nameData && nameSize > 0) {
                        gfxFontUtils::ReadCanonicalName(nameData, nameSize,
                            gfxFontUtils::NAME_ID_FULL,
                            fontData.mFullName);
                        gfxFontUtils::ReadCanonicalName(nameData, nameSize,
                            gfxFontUtils::NAME_ID_POSTSCRIPT,
                            fontData.mPostscriptName);
                        dwFontFace->ReleaseFontTable(ctx);
                    }
                }
            }

            haveData = !fontData.mPostscriptName.IsEmpty() ||
                       !fontData.mFullName.IsEmpty();
            if (haveData) {
                mLoadStats.facenames++;
            }
        }

        // cmaps
        if (mLoadCmaps) {
            if (!dwFontFace) {
                hr = dwFont->CreateFontFace(getter_AddRefs(dwFontFace));
                if (!SUCCEEDED(hr)) {
                    continue;
                }
            }

            uint32_t kCMAP =
                NativeEndian::swapToBigEndian(TRUETYPE_TAG('c','m','a','p'));
            const uint8_t *cmapData;
            BOOL exists;
            void* ctx;
            uint32_t cmapSize;

            hr = dwFontFace->TryGetFontTable(kCMAP,
                     (const void**)&cmapData, &cmapSize, &ctx, &exists);

            if (SUCCEEDED(hr)) {
                bool cmapLoaded = false;
                RefPtr<gfxCharacterMap> charmap = new gfxCharacterMap();
                uint32_t offset;

                if (cmapData &&
                    cmapSize > 0 &&
                    NS_SUCCEEDED(
                        gfxFontUtils::ReadCMAP(cmapData, cmapSize, *charmap,
                                               offset))) {
                    fontData.mCharacterMap = charmap;
                    fontData.mUVSOffset = offset;
                    cmapLoaded = true;
                    mLoadStats.cmaps++;
                }
                dwFontFace->ReleaseFontTable(ctx);
                haveData = haveData || cmapLoaded;
           }
        }

        // if have data, load
        if (haveData) {
            mFontFaceData.Put(fullID, fontData);
        }
    }
}

already_AddRefed<FontInfoData>
gfxDWriteFontList::CreateFontInfoData()
{
    bool loadCmaps = !UsesSystemFallback() ||
        gfxPlatform::GetPlatform()->UseCmapsDuringSystemFallback();

    RefPtr<DirectWriteFontInfo> fi =
        new DirectWriteFontInfo(false, NeedFullnamePostscriptNames(), loadCmaps,
                                mSystemFonts
#ifdef MOZ_BUNDLED_FONTS
                                , mBundledFonts
#endif
                               );

    return fi.forget();
}

gfxFontFamily*
gfxDWriteFontList::CreateFontFamily(const nsAString& aName) const
{
    return new gfxDWriteFontFamily(aName, nullptr);
}


#ifdef MOZ_BUNDLED_FONTS

#define IMPL_QI_FOR_DWRITE(_interface)                                        \
    public:                                                                   \
        IFACEMETHOD(QueryInterface) (IID const& riid, void** ppvObject)       \
        {                                                                     \
            if (__uuidof(_interface) == riid) {                               \
                *ppvObject = this;                                            \
            } else if (__uuidof(IUnknown) == riid) {                          \
                *ppvObject = this;                                            \
            } else {                                                          \
                *ppvObject = nullptr;                                         \
                return E_NOINTERFACE;                                         \
            }                                                                 \
            this->AddRef();                                                   \
            return S_OK;                                                      \
        }

class BundledFontFileEnumerator
    : public IDWriteFontFileEnumerator
{
    IMPL_QI_FOR_DWRITE(IDWriteFontFileEnumerator)

    NS_INLINE_DECL_REFCOUNTING(BundledFontFileEnumerator)

public:
    BundledFontFileEnumerator(IDWriteFactory *aFactory,
                              nsIFile        *aFontDir);

    IFACEMETHODIMP MoveNext(BOOL * hasCurrentFile);

    IFACEMETHODIMP GetCurrentFontFile(IDWriteFontFile ** fontFile);

private:
    BundledFontFileEnumerator() = delete;
    BundledFontFileEnumerator(const BundledFontFileEnumerator&) = delete;
    BundledFontFileEnumerator& operator=(const BundledFontFileEnumerator&) = delete;
    virtual ~BundledFontFileEnumerator() {}

    RefPtr<IDWriteFactory>      mFactory;

    nsCOMPtr<nsIFile>             mFontDir;
    nsCOMPtr<nsISimpleEnumerator> mEntries;
    nsCOMPtr<nsISupports>         mCurrent;
};

BundledFontFileEnumerator::BundledFontFileEnumerator(IDWriteFactory *aFactory,
                                                     nsIFile        *aFontDir)
    : mFactory(aFactory)
    , mFontDir(aFontDir)
{
    mFontDir->GetDirectoryEntries(getter_AddRefs(mEntries));
}

IFACEMETHODIMP
BundledFontFileEnumerator::MoveNext(BOOL * aHasCurrentFile)
{
    bool hasMore = false;
    if (mEntries) {
        if (NS_SUCCEEDED(mEntries->HasMoreElements(&hasMore)) && hasMore) {
            if (NS_SUCCEEDED(mEntries->GetNext(getter_AddRefs(mCurrent)))) {
                hasMore = true;
            }
        }
    }
    *aHasCurrentFile = hasMore;
    return S_OK;
}

IFACEMETHODIMP
BundledFontFileEnumerator::GetCurrentFontFile(IDWriteFontFile ** aFontFile)
{
    nsCOMPtr<nsIFile> file = do_QueryInterface(mCurrent);
    if (!file) {
        return E_FAIL;
    }
    nsString path;
    if (NS_FAILED(file->GetPath(path))) {
        return E_FAIL;
    }
    return mFactory->CreateFontFileReference((const WCHAR*)path.get(),
                                             nullptr, aFontFile);
}

class BundledFontLoader
    : public IDWriteFontCollectionLoader
{
    IMPL_QI_FOR_DWRITE(IDWriteFontCollectionLoader)

    NS_INLINE_DECL_REFCOUNTING(BundledFontLoader)

public:
    BundledFontLoader()
    {
    }

    IFACEMETHODIMP CreateEnumeratorFromKey(
        IDWriteFactory *aFactory,
        const void *aCollectionKey,
        UINT32 aCollectionKeySize,
        IDWriteFontFileEnumerator **aFontFileEnumerator);

private:
    BundledFontLoader(const BundledFontLoader&) = delete;
    BundledFontLoader& operator=(const BundledFontLoader&) = delete;
    virtual ~BundledFontLoader() { }
};

IFACEMETHODIMP
BundledFontLoader::CreateEnumeratorFromKey(
    IDWriteFactory *aFactory,
    const void *aCollectionKey,
    UINT32  aCollectionKeySize,
    IDWriteFontFileEnumerator **aFontFileEnumerator)
{
    nsIFile *fontDir = *(nsIFile**)aCollectionKey;
    *aFontFileEnumerator = new BundledFontFileEnumerator(aFactory, fontDir);
    NS_ADDREF(*aFontFileEnumerator);
    return S_OK;
}

already_AddRefed<IDWriteFontCollection>
gfxDWriteFontList::CreateBundledFontsCollection(IDWriteFactory* aFactory)
{
    nsCOMPtr<nsIFile> localDir;
    nsresult rv = NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(localDir));
    if (NS_FAILED(rv)) {
        return nullptr;
    }
    if (NS_FAILED(localDir->Append(NS_LITERAL_STRING("fonts")))) {
        return nullptr;
    }
    bool isDir;
    if (NS_FAILED(localDir->IsDirectory(&isDir)) || !isDir) {
        return nullptr;
    }

    RefPtr<BundledFontLoader> loader = new BundledFontLoader();
    if (FAILED(aFactory->RegisterFontCollectionLoader(loader))) {
        return nullptr;
    }

    const void *key = localDir.get();
    RefPtr<IDWriteFontCollection> collection;
    HRESULT hr =
        aFactory->CreateCustomFontCollection(loader, &key, sizeof(key),
                                             getter_AddRefs(collection));

    aFactory->UnregisterFontCollectionLoader(loader);

    if (FAILED(hr)) {
        return nullptr;
    } else {
        return collection.forget();
    }
}

#endif

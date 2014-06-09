/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"
#include "mozilla/MathAlgorithms.h"

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif
#include "prlog.h"

#include "nsServiceManagerUtils.h"
#include "nsExpirationTracker.h"
#include "nsILanguageAtomService.h"
#include "nsITimer.h"

#include "gfxFont.h"
#include "gfxPlatform.h"
#include "nsGkAtoms.h"

#include "gfxTypes.h"
#include "gfxContext.h"
#include "gfxFontMissingGlyphs.h"
#include "gfxHarfBuzzShaper.h"
#include "gfxUserFontSet.h"
#include "gfxPlatformFontList.h"
#include "gfxScriptItemizer.h"
#include "nsSpecialCasingData.h"
#include "nsTextRunTransformations.h"
#include "nsUnicodeProperties.h"
#include "nsMathUtils.h"
#include "nsBidiUtils.h"
#include "nsUnicodeRange.h"
#include "nsStyleConsts.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Likely.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/Telemetry.h"
#include "gfxSVGGlyphs.h"
#include "gfxMathTable.h"
#include "gfx2DGlue.h"

#include "GreekCasing.h"

#if defined(XP_MACOSX)
#include "nsCocoaFeatures.h"
#endif

#include "cairo.h"
#include "gfxFontTest.h"

#include "harfbuzz/hb.h"
#include "harfbuzz/hb-ot.h"
#include "graphite2/Font.h"

#include "nsCRT.h"
#include "GeckoProfiler.h"
#include "gfxFontConstants.h"

#include <algorithm>

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::unicode;
using mozilla::services::GetObserverService;

gfxFontCache *gfxFontCache::gGlobalCache = nullptr;

static const char16_t kEllipsisChar[] = { 0x2026, 0x0 };
static const char16_t kASCIIPeriodsChar[] = { '.', '.', '.', 0x0 };

#ifdef DEBUG_roc
#define DEBUG_TEXT_RUN_STORAGE_METRICS
#endif

#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
static uint32_t gTextRunStorageHighWaterMark = 0;
static uint32_t gTextRunStorage = 0;
static uint32_t gFontCount = 0;
static uint32_t gGlyphExtentsCount = 0;
static uint32_t gGlyphExtentsWidthsTotalSize = 0;
static uint32_t gGlyphExtentsSetupEagerSimple = 0;
static uint32_t gGlyphExtentsSetupEagerTight = 0;
static uint32_t gGlyphExtentsSetupLazyTight = 0;
static uint32_t gGlyphExtentsSetupFallBackToTight = 0;
#endif

#ifdef PR_LOGGING
#define LOG_FONTINIT(args) PR_LOG(gfxPlatform::GetLog(eGfxLog_fontinit), \
                                  PR_LOG_DEBUG, args)
#define LOG_FONTINIT_ENABLED() PR_LOG_TEST( \
                                        gfxPlatform::GetLog(eGfxLog_fontinit), \
                                        PR_LOG_DEBUG)
#endif // PR_LOGGING

void
gfxCharacterMap::NotifyReleased()
{
    gfxPlatformFontList *fontlist = gfxPlatformFontList::PlatformFontList();
    if (mShared) {
        fontlist->RemoveCmap(this);
    }
    delete this;
}

gfxFontEntry::gfxFontEntry() :
    mItalic(false), mFixedPitch(false),
    mIsProxy(false), mIsValid(true),
    mIsBadUnderlineFont(false),
    mIsUserFont(false),
    mIsLocalUserFont(false),
    mStandardFace(false),
    mSymbolFont(false),
    mIgnoreGDEF(false),
    mIgnoreGSUB(false),
    mSVGInitialized(false),
    mMathInitialized(false),
    mHasSpaceFeaturesInitialized(false),
    mHasSpaceFeatures(false),
    mHasSpaceFeaturesKerning(false),
    mHasSpaceFeaturesNonKerning(false),
    mSkipDefaultFeatureSpaceCheck(false),
    mCheckedForGraphiteTables(false),
    mCheckedForGraphiteSmallCaps(false),
    mHasCmapTable(false),
    mGrFaceInitialized(false),
    mCheckedForColorGlyph(false),
    mWeight(500), mStretch(NS_FONT_STRETCH_NORMAL),
    mUVSOffset(0), mUVSData(nullptr),
    mLanguageOverride(NO_FONT_LANGUAGE_OVERRIDE),
    mCOLR(nullptr),
    mCPAL(nullptr),
    mUnitsPerEm(0),
    mHBFace(nullptr),
    mGrFace(nullptr),
    mGrFaceRefCnt(0)
{
    memset(&mDefaultSubSpaceFeatures, 0, sizeof(mDefaultSubSpaceFeatures));
    memset(&mNonDefaultSubSpaceFeatures, 0, sizeof(mNonDefaultSubSpaceFeatures));
}

gfxFontEntry::gfxFontEntry(const nsAString& aName, bool aIsStandardFace) :
    mName(aName), mItalic(false), mFixedPitch(false),
    mIsProxy(false), mIsValid(true),
    mIsBadUnderlineFont(false), mIsUserFont(false),
    mIsLocalUserFont(false), mStandardFace(aIsStandardFace),
    mSymbolFont(false),
    mIgnoreGDEF(false),
    mIgnoreGSUB(false),
    mSVGInitialized(false),
    mMathInitialized(false),
    mHasSpaceFeaturesInitialized(false),
    mHasSpaceFeatures(false),
    mHasSpaceFeaturesKerning(false),
    mHasSpaceFeaturesNonKerning(false),
    mSkipDefaultFeatureSpaceCheck(false),
    mCheckedForGraphiteTables(false),
    mCheckedForGraphiteSmallCaps(false),
    mHasCmapTable(false),
    mGrFaceInitialized(false),
    mCheckedForColorGlyph(false),
    mWeight(500), mStretch(NS_FONT_STRETCH_NORMAL),
    mUVSOffset(0), mUVSData(nullptr),
    mLanguageOverride(NO_FONT_LANGUAGE_OVERRIDE),
    mCOLR(nullptr),
    mCPAL(nullptr),
    mUnitsPerEm(0),
    mHBFace(nullptr),
    mGrFace(nullptr),
    mGrFaceRefCnt(0)
{
    memset(&mDefaultSubSpaceFeatures, 0, sizeof(mDefaultSubSpaceFeatures));
    memset(&mNonDefaultSubSpaceFeatures, 0, sizeof(mNonDefaultSubSpaceFeatures));
}

gfxFontEntry::~gfxFontEntry()
{
    if (mCOLR) {
        hb_blob_destroy(mCOLR);
    }

    if (mCPAL) {
        hb_blob_destroy(mCPAL);
    }

    // For downloaded fonts, we need to tell the user font cache that this
    // entry is being deleted.
    if (!mIsProxy && IsUserFont() && !IsLocalUserFont()) {
        gfxUserFontSet::UserFontCache::ForgetFont(this);
    }

    // By the time the entry is destroyed, all font instances that were
    // using it should already have been deleted, and so the HB and/or Gr
    // face objects should have been released.
    MOZ_ASSERT(!mHBFace);
    MOZ_ASSERT(!mGrFaceInitialized);
}

bool gfxFontEntry::IsSymbolFont() 
{
    return mSymbolFont;
}

bool gfxFontEntry::TestCharacterMap(uint32_t aCh)
{
    if (!mCharacterMap) {
        ReadCMAP();
        NS_ASSERTION(mCharacterMap, "failed to initialize character map");
    }
    return mCharacterMap->test(aCh);
}

nsresult gfxFontEntry::InitializeUVSMap()
{
    // mUVSOffset will not be initialized
    // until cmap is initialized.
    if (!mCharacterMap) {
        ReadCMAP();
        NS_ASSERTION(mCharacterMap, "failed to initialize character map");
    }

    if (!mUVSOffset) {
        return NS_ERROR_FAILURE;
    }

    if (!mUVSData) {
        const uint32_t kCmapTag = TRUETYPE_TAG('c','m','a','p');
        AutoTable cmapTable(this, kCmapTag);
        if (!cmapTable) {
            mUVSOffset = 0; // don't bother to read the table again
            return NS_ERROR_FAILURE;
        }

        uint8_t* uvsData;
        unsigned int cmapLen;
        const char* cmapData = hb_blob_get_data(cmapTable, &cmapLen);
        nsresult rv = gfxFontUtils::ReadCMAPTableFormat14(
                          (const uint8_t*)cmapData + mUVSOffset,
                          cmapLen - mUVSOffset, uvsData);

        if (NS_FAILED(rv)) {
            mUVSOffset = 0; // don't bother to read the table again
            return rv;
        }

        mUVSData = uvsData;
    }

    return NS_OK;
}

uint16_t gfxFontEntry::GetUVSGlyph(uint32_t aCh, uint32_t aVS)
{
    InitializeUVSMap();

    if (mUVSData) {
        return gfxFontUtils::MapUVSToGlyphFormat14(mUVSData, aCh, aVS);
    }

    return 0;
}

bool gfxFontEntry::SupportsScriptInGSUB(const hb_tag_t* aScriptTags)
{
    hb_face_t *face = GetHBFace();
    if (!face) {
        return false;
    }

    unsigned int index;
    hb_tag_t     chosenScript;
    bool found =
        hb_ot_layout_table_choose_script(face, TRUETYPE_TAG('G','S','U','B'),
                                         aScriptTags, &index, &chosenScript);
    hb_face_destroy(face);

    return found && chosenScript != TRUETYPE_TAG('D','F','L','T');
}

nsresult gfxFontEntry::ReadCMAP(FontInfoData *aFontInfoData)
{
    NS_ASSERTION(false, "using default no-op implementation of ReadCMAP");
    mCharacterMap = new gfxCharacterMap();
    return NS_OK;
}

nsString
gfxFontEntry::RealFaceName()
{
    AutoTable nameTable(this, TRUETYPE_TAG('n','a','m','e'));
    if (nameTable) {
        nsAutoString name;
        nsresult rv = gfxFontUtils::GetFullNameFromTable(nameTable, name);
        if (NS_SUCCEEDED(rv)) {
            return name;
        }
    }
    return Name();
}

already_AddRefed<gfxFont>
gfxFontEntry::FindOrMakeFont(const gfxFontStyle *aStyle, bool aNeedsBold)
{
    // the font entry name is the psname, not the family name
    nsRefPtr<gfxFont> font = gfxFontCache::GetCache()->Lookup(this, aStyle);

    if (!font) {
        gfxFont *newFont = CreateFontInstance(aStyle, aNeedsBold);
        if (!newFont)
            return nullptr;
        if (!newFont->Valid()) {
            delete newFont;
            return nullptr;
        }
        font = newFont;
        gfxFontCache::GetCache()->AddNew(font);
    }
    return font.forget();
}

uint16_t
gfxFontEntry::UnitsPerEm()
{
    if (!mUnitsPerEm) {
        AutoTable headTable(this, TRUETYPE_TAG('h','e','a','d'));
        if (headTable) {
            uint32_t len;
            const HeadTable* head =
                reinterpret_cast<const HeadTable*>(hb_blob_get_data(headTable,
                                                                    &len));
            if (len >= sizeof(HeadTable)) {
                mUnitsPerEm = head->unitsPerEm;
            }
        }

        // if we didn't find a usable 'head' table, or if the value was
        // outside the valid range, record it as invalid
        if (mUnitsPerEm < kMinUPEM || mUnitsPerEm > kMaxUPEM) {
            mUnitsPerEm = kInvalidUPEM;
        }
    }
    return mUnitsPerEm;
}

bool
gfxFontEntry::HasSVGGlyph(uint32_t aGlyphId)
{
    NS_ASSERTION(mSVGInitialized, "SVG data has not yet been loaded. TryGetSVGData() first.");
    return mSVGGlyphs->HasSVGGlyph(aGlyphId);
}

bool
gfxFontEntry::GetSVGGlyphExtents(gfxContext *aContext, uint32_t aGlyphId,
                                 gfxRect *aResult)
{
    NS_ABORT_IF_FALSE(mSVGInitialized,
                      "SVG data has not yet been loaded. TryGetSVGData() first.");
    NS_ABORT_IF_FALSE(mUnitsPerEm >= kMinUPEM && mUnitsPerEm <= kMaxUPEM,
                      "font has invalid unitsPerEm");

    gfxContextAutoSaveRestore matrixRestore(aContext);
    cairo_matrix_t fontMatrix;
    cairo_get_font_matrix(aContext->GetCairo(), &fontMatrix);

    gfxMatrix svgToAppSpace = *reinterpret_cast<gfxMatrix*>(&fontMatrix);
    svgToAppSpace.Scale(1.0f / mUnitsPerEm, 1.0f / mUnitsPerEm);

    return mSVGGlyphs->GetGlyphExtents(aGlyphId, svgToAppSpace, aResult);
}

bool
gfxFontEntry::RenderSVGGlyph(gfxContext *aContext, uint32_t aGlyphId,
                             int aDrawMode, gfxTextContextPaint *aContextPaint)
{
    NS_ASSERTION(mSVGInitialized, "SVG data has not yet been loaded. TryGetSVGData() first.");
    return mSVGGlyphs->RenderGlyph(aContext, aGlyphId, DrawMode(aDrawMode),
                                   aContextPaint);
}

bool
gfxFontEntry::TryGetSVGData(gfxFont* aFont)
{
    if (!gfxPlatform::GetPlatform()->OpenTypeSVGEnabled()) {
        return false;
    }

    if (!mSVGInitialized) {
        mSVGInitialized = true;

        // If UnitsPerEm is not known/valid, we can't use SVG glyphs
        if (UnitsPerEm() == kInvalidUPEM) {
            return false;
        }

        // We don't use AutoTable here because we'll pass ownership of this
        // blob to the gfxSVGGlyphs, once we've confirmed the table exists
        hb_blob_t *svgTable = GetFontTable(TRUETYPE_TAG('S','V','G',' '));
        if (!svgTable) {
            return false;
        }

        // gfxSVGGlyphs will hb_blob_destroy() the table when it is finished
        // with it.
        mSVGGlyphs = new gfxSVGGlyphs(svgTable, this);
    }

    if (!mFontsUsingSVGGlyphs.Contains(aFont)) {
        mFontsUsingSVGGlyphs.AppendElement(aFont);
    }

    return !!mSVGGlyphs;
}

void
gfxFontEntry::NotifyFontDestroyed(gfxFont* aFont)
{
    mFontsUsingSVGGlyphs.RemoveElement(aFont);
}

void
gfxFontEntry::NotifyGlyphsChanged()
{
    for (uint32_t i = 0, count = mFontsUsingSVGGlyphs.Length(); i < count; ++i) {
        gfxFont* font = mFontsUsingSVGGlyphs[i];
        font->NotifyGlyphsChanged();
    }
}

bool
gfxFontEntry::TryGetMathTable(gfxFont* aFont)
{
    if (!mMathInitialized) {
        mMathInitialized = true;

        // If UnitsPerEm is not known/valid, we can't use MATH table
        if (UnitsPerEm() == kInvalidUPEM) {
            return false;
        }

        // We don't use AutoTable here because we'll pass ownership of this
        // blob to the gfxMathTable, once we've confirmed the table exists
        hb_blob_t *mathTable = GetFontTable(TRUETYPE_TAG('M','A','T','H'));
        if (!mathTable) {
            return false;
        }

        // gfxMathTable will hb_blob_destroy() the table when it is finished
        // with it.
        mMathTable = new gfxMathTable(mathTable);
        if (!mMathTable->HasValidHeaders()) {
            mMathTable = nullptr;
            return false;
        }
    }

    return !!mMathTable;
}

gfxFloat
gfxFontEntry::GetMathConstant(gfxFontEntry::MathConstant aConstant)
{
    NS_ASSERTION(mMathTable, "Math data has not yet been loaded. TryGetMathData() first.");
    gfxFloat value = mMathTable->GetMathConstant(aConstant);
    if (aConstant == gfxFontEntry::ScriptPercentScaleDown ||
        aConstant == gfxFontEntry::ScriptScriptPercentScaleDown ||
        aConstant == gfxFontEntry::RadicalDegreeBottomRaisePercent) {
        return value / 100.0;
    }
    return value / mUnitsPerEm;
}

bool
gfxFontEntry::GetMathItalicsCorrection(uint32_t aGlyphID,
                                       gfxFloat* aItalicCorrection)
{
    NS_ASSERTION(mMathTable, "Math data has not yet been loaded. TryGetMathData() first.");
    int16_t italicCorrection;
    if (!mMathTable->GetMathItalicsCorrection(aGlyphID, &italicCorrection)) {
        return false;
    }
    *aItalicCorrection = gfxFloat(italicCorrection) / mUnitsPerEm;
    return true;
}

uint32_t
gfxFontEntry::GetMathVariantsSize(uint32_t aGlyphID, bool aVertical,
                                  uint16_t aSize)
{
    NS_ASSERTION(mMathTable, "Math data has not yet been loaded. TryGetMathData() first.");
    return mMathTable->GetMathVariantsSize(aGlyphID, aVertical, aSize);
}

bool
gfxFontEntry::GetMathVariantsParts(uint32_t aGlyphID, bool aVertical,
                                   uint32_t aGlyphs[4])
{
    NS_ASSERTION(mMathTable, "Math data has not yet been loaded. TryGetMathData() first.");
    return mMathTable->GetMathVariantsParts(aGlyphID, aVertical, aGlyphs);
}

bool
gfxFontEntry::TryGetColorGlyphs()
{
    if (mCheckedForColorGlyph) {
        return (mCOLR && mCPAL);
    }

    mCheckedForColorGlyph = true;

    mCOLR = GetFontTable(TRUETYPE_TAG('C', 'O', 'L', 'R'));
    if (!mCOLR) {
        return false;
    }

    mCPAL = GetFontTable(TRUETYPE_TAG('C', 'P', 'A', 'L'));
    if (!mCPAL) {
        hb_blob_destroy(mCOLR);
        mCOLR = nullptr;
        return false;
    }

    // validation COLR and CPAL table
    if (gfxFontUtils::ValidateColorGlyphs(mCOLR, mCPAL)) {
        return true;
    }

    hb_blob_destroy(mCOLR);
    hb_blob_destroy(mCPAL);
    mCOLR = nullptr;
    mCPAL = nullptr;
    return false;
}

/**
 * FontTableBlobData
 *
 * See FontTableHashEntry for the general strategy.
 */

class gfxFontEntry::FontTableBlobData {
public:
    // Adopts the content of aBuffer.
    FontTableBlobData(FallibleTArray<uint8_t>& aBuffer)
        : mHashtable(nullptr), mHashKey(0)
    {
        MOZ_COUNT_CTOR(FontTableBlobData);
        mTableData.SwapElements(aBuffer);
    }

    ~FontTableBlobData() {
        MOZ_COUNT_DTOR(FontTableBlobData);
        if (mHashtable && mHashKey) {
            mHashtable->RemoveEntry(mHashKey);
        }
    }

    // Useful for creating blobs
    const char *GetTable() const
    {
        return reinterpret_cast<const char*>(mTableData.Elements());
    }
    uint32_t GetTableLength() const { return mTableData.Length(); }

    // Tell this FontTableBlobData to remove the HashEntry when this is
    // destroyed.
    void ManageHashEntry(nsTHashtable<FontTableHashEntry> *aHashtable,
                         uint32_t aHashKey)
    {
        mHashtable = aHashtable;
        mHashKey = aHashKey;
    }

    // Disconnect from the HashEntry (because the blob has already been
    // removed from the hashtable).
    void ForgetHashEntry()
    {
        mHashtable = nullptr;
        mHashKey = 0;
    }

    size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
        return mTableData.SizeOfExcludingThis(aMallocSizeOf);
    }
    size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
        return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
    }

private:
    // The font table data block, owned (via adoption)
    FallibleTArray<uint8_t> mTableData;

    // The blob destroy function needs to know the owning hashtable
    // and the hashtable key, so that it can remove the entry.
    nsTHashtable<FontTableHashEntry> *mHashtable;
    uint32_t                          mHashKey;

    // not implemented
    FontTableBlobData(const FontTableBlobData&);
};

hb_blob_t *
gfxFontEntry::FontTableHashEntry::
ShareTableAndGetBlob(FallibleTArray<uint8_t>& aTable,
                     nsTHashtable<FontTableHashEntry> *aHashtable)
{
    Clear();
    // adopts elements of aTable
    mSharedBlobData = new FontTableBlobData(aTable);
    mBlob = hb_blob_create(mSharedBlobData->GetTable(),
                           mSharedBlobData->GetTableLength(),
                           HB_MEMORY_MODE_READONLY,
                           mSharedBlobData, DeleteFontTableBlobData);
    if (!mSharedBlobData) {
        // The FontTableBlobData was destroyed during hb_blob_create().
        // The (empty) blob is still be held in the hashtable with a strong
        // reference.
        return hb_blob_reference(mBlob);
    }

    // Tell the FontTableBlobData to remove this hash entry when destroyed.
    // The hashtable does not keep a strong reference.
    mSharedBlobData->ManageHashEntry(aHashtable, GetKey());
    return mBlob;
}

void
gfxFontEntry::FontTableHashEntry::Clear()
{
    // If the FontTableBlobData is managing the hash entry, then the blob is
    // not owned by this HashEntry; otherwise there is strong reference to the
    // blob that must be removed.
    if (mSharedBlobData) {
        mSharedBlobData->ForgetHashEntry();
        mSharedBlobData = nullptr;
    } else if (mBlob) {
        hb_blob_destroy(mBlob);
    }
    mBlob = nullptr;
}

// a hb_destroy_func for hb_blob_create

/* static */ void
gfxFontEntry::FontTableHashEntry::DeleteFontTableBlobData(void *aBlobData)
{
    delete static_cast<FontTableBlobData*>(aBlobData);
}

hb_blob_t *
gfxFontEntry::FontTableHashEntry::GetBlob() const
{
    return hb_blob_reference(mBlob);
}

bool
gfxFontEntry::GetExistingFontTable(uint32_t aTag, hb_blob_t **aBlob)
{
    if (!mFontTableCache) {
        // we do this here rather than on fontEntry construction
        // because not all shapers will access the table cache at all
        mFontTableCache = new nsTHashtable<FontTableHashEntry>(10);
    }

    FontTableHashEntry *entry = mFontTableCache->GetEntry(aTag);
    if (!entry) {
        return false;
    }

    *aBlob = entry->GetBlob();
    return true;
}

hb_blob_t *
gfxFontEntry::ShareFontTableAndGetBlob(uint32_t aTag,
                                       FallibleTArray<uint8_t>* aBuffer)
{
    if (MOZ_UNLIKELY(!mFontTableCache)) {
        // we do this here rather than on fontEntry construction
        // because not all shapers will access the table cache at all
      mFontTableCache = new nsTHashtable<FontTableHashEntry>(10);
    }

    FontTableHashEntry *entry = mFontTableCache->PutEntry(aTag);
    if (MOZ_UNLIKELY(!entry)) { // OOM
        return nullptr;
    }

    if (!aBuffer) {
        // ensure the entry is null
        entry->Clear();
        return nullptr;
    }

    return entry->ShareTableAndGetBlob(*aBuffer, mFontTableCache);
}

static int
DirEntryCmp(const void* aKey, const void* aItem)
{
    int32_t tag = *static_cast<const int32_t*>(aKey);
    const TableDirEntry* entry = static_cast<const TableDirEntry*>(aItem);
    return tag - int32_t(entry->tag);
}

hb_blob_t*
gfxFontEntry::GetTableFromFontData(const void* aFontData, uint32_t aTableTag)
{
    const SFNTHeader* header =
        reinterpret_cast<const SFNTHeader*>(aFontData);
    const TableDirEntry* dir =
        reinterpret_cast<const TableDirEntry*>(header + 1);
    dir = static_cast<const TableDirEntry*>
        (bsearch(&aTableTag, dir, uint16_t(header->numTables),
                 sizeof(TableDirEntry), DirEntryCmp));
    if (dir) {
        return hb_blob_create(reinterpret_cast<const char*>(aFontData) +
                                  dir->offset, dir->length,
                              HB_MEMORY_MODE_READONLY, nullptr, nullptr);

    }
    return nullptr;
}

already_AddRefed<gfxCharacterMap>
gfxFontEntry::GetCMAPFromFontInfo(FontInfoData *aFontInfoData,
                                  uint32_t& aUVSOffset,
                                  bool& aSymbolFont)
{
    if (!aFontInfoData || !aFontInfoData->mLoadCmaps) {
        return nullptr;
    }

    return aFontInfoData->GetCMAP(mName, aUVSOffset, aSymbolFont);
}

hb_blob_t *
gfxFontEntry::GetFontTable(uint32_t aTag)
{
    hb_blob_t *blob;
    if (GetExistingFontTable(aTag, &blob)) {
        return blob;
    }

    FallibleTArray<uint8_t> buffer;
    bool haveTable = NS_SUCCEEDED(CopyFontTable(aTag, buffer));

    return ShareFontTableAndGetBlob(aTag, haveTable ? &buffer : nullptr);
}

// callback for HarfBuzz to get a font table (in hb_blob_t form)
// from the font entry (passed as aUserData)
/*static*/ hb_blob_t *
gfxFontEntry::HBGetTable(hb_face_t *face, uint32_t aTag, void *aUserData)
{
    gfxFontEntry *fontEntry = static_cast<gfxFontEntry*>(aUserData);

    // bug 589682 - ignore the GDEF table in buggy fonts (applies to
    // Italic and BoldItalic faces of Times New Roman)
    if (aTag == TRUETYPE_TAG('G','D','E','F') &&
        fontEntry->IgnoreGDEF()) {
        return nullptr;
    }

    // bug 721719 - ignore the GSUB table in buggy fonts (applies to Roboto,
    // at least on some Android ICS devices; set in gfxFT2FontList.cpp)
    if (aTag == TRUETYPE_TAG('G','S','U','B') &&
        fontEntry->IgnoreGSUB()) {
        return nullptr;
    }

    return fontEntry->GetFontTable(aTag);
}

/*static*/ void
gfxFontEntry::HBFaceDeletedCallback(void *aUserData)
{
    gfxFontEntry *fe = static_cast<gfxFontEntry*>(aUserData);
    fe->ForgetHBFace();
}

void
gfxFontEntry::ForgetHBFace()
{
    mHBFace = nullptr;
}

hb_face_t*
gfxFontEntry::GetHBFace()
{
    if (!mHBFace) {
        mHBFace = hb_face_create_for_tables(HBGetTable, this,
                                            HBFaceDeletedCallback);
        return mHBFace;
    }
    return hb_face_reference(mHBFace);
}

/*static*/ const void*
gfxFontEntry::GrGetTable(const void *aAppFaceHandle, unsigned int aName,
                         size_t *aLen)
{
    gfxFontEntry *fontEntry =
        static_cast<gfxFontEntry*>(const_cast<void*>(aAppFaceHandle));
    hb_blob_t *blob = fontEntry->GetFontTable(aName);
    if (blob) {
        unsigned int blobLength;
        const void *tableData = hb_blob_get_data(blob, &blobLength);
        fontEntry->mGrTableMap->Put(tableData, blob);
        *aLen = blobLength;
        return tableData;
    }
    *aLen = 0;
    return nullptr;
}

/*static*/ void
gfxFontEntry::GrReleaseTable(const void *aAppFaceHandle,
                             const void *aTableBuffer)
{
    gfxFontEntry *fontEntry =
        static_cast<gfxFontEntry*>(const_cast<void*>(aAppFaceHandle));
    void *data;
    if (fontEntry->mGrTableMap->Get(aTableBuffer, &data)) {
        fontEntry->mGrTableMap->Remove(aTableBuffer);
        hb_blob_destroy(static_cast<hb_blob_t*>(data));
    }
}

gr_face*
gfxFontEntry::GetGrFace()
{
    if (!mGrFaceInitialized) {
        gr_face_ops faceOps = {
            sizeof(gr_face_ops),
            GrGetTable,
            GrReleaseTable
        };
        mGrTableMap = new nsDataHashtable<nsPtrHashKey<const void>,void*>;
        mGrFace = gr_make_face_with_ops(this, &faceOps, gr_face_default);
        mGrFaceInitialized = true;
    }
    ++mGrFaceRefCnt;
    return mGrFace;
}

void
gfxFontEntry::ReleaseGrFace(gr_face *aFace)
{
    MOZ_ASSERT(aFace == mGrFace); // sanity-check
    MOZ_ASSERT(mGrFaceRefCnt > 0);
    if (--mGrFaceRefCnt == 0) {
        gr_face_destroy(mGrFace);
        mGrFace = nullptr;
        mGrFaceInitialized = false;
        delete mGrTableMap;
        mGrTableMap = nullptr;
    }
}

void
gfxFontEntry::DisconnectSVG()
{
    if (mSVGInitialized && mSVGGlyphs) {
        mSVGGlyphs = nullptr;
        mSVGInitialized = false;
    }
}

bool
gfxFontEntry::HasFontTable(uint32_t aTableTag)
{
    AutoTable table(this, aTableTag);
    return table && hb_blob_get_length(table) > 0;
}

void
gfxFontEntry::CheckForGraphiteTables()
{
    mHasGraphiteTables = HasFontTable(TRUETYPE_TAG('S','i','l','f'));
}

bool
gfxFontEntry::SupportsOpenTypeSmallCaps(int32_t aScript)
{
    if (!mSmallCapsSupport) {
        mSmallCapsSupport = new nsDataHashtable<nsUint32HashKey,bool>();
    }

    bool result;
    if (mSmallCapsSupport->Get(uint32_t(aScript), &result)) {
        return result;
    }

    result = false;

    hb_face_t *face = GetHBFace();

    if (hb_ot_layout_has_substitution(face)) {
        // Decide what harfbuzz script code will be used for shaping
        hb_script_t hbScript;
        if (aScript <= MOZ_SCRIPT_INHERITED) {
            // For unresolved "common" or "inherited" runs, default to Latin
            // for now. (Compare gfxHarfBuzzShaper.)
            hbScript = HB_SCRIPT_LATIN;
        } else {
            hbScript = hb_script_t(GetScriptTagForCode(aScript));
        }

        // Get the OpenType tag(s) that match this script code
        hb_tag_t scriptTags[4] = {
            HB_TAG_NONE,
            HB_TAG_NONE,
            HB_TAG_NONE,
            HB_TAG_NONE
        };
        hb_ot_tags_from_script(hbScript, &scriptTags[0], &scriptTags[1]);

        // Replace the first remaining NONE with DEFAULT
        hb_tag_t* scriptTag = &scriptTags[0];
        while (*scriptTag != HB_TAG_NONE) {
            ++scriptTag;
        }
        *scriptTag = HB_OT_TAG_DEFAULT_SCRIPT;

        // Now check for 'smcp' under the first of those scripts that is present
        const hb_tag_t kGSUB = HB_TAG('G','S','U','B');
        const hb_tag_t kSMCP = HB_TAG('s','m','c','p');
        scriptTag = &scriptTags[0];
        while (*scriptTag != HB_TAG_NONE) {
            unsigned int scriptIndex;
            if (hb_ot_layout_table_find_script(face, kGSUB, *scriptTag,
                                               &scriptIndex)) {
                if (hb_ot_layout_language_find_feature(face, kGSUB,
                                                       scriptIndex,
                                           HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX,
                                                       kSMCP, nullptr)) {
                    result = true;
                }
                break;
            }
            ++scriptTag;
        }
    }

    hb_face_destroy(face);

    mSmallCapsSupport->Put(uint32_t(aScript), result);

    return result;
}

bool
gfxFontEntry::SupportsGraphiteSmallCaps()
{
    if (!mCheckedForGraphiteSmallCaps) {
        gr_face* face = GetGrFace();
        mHasGraphiteSmallCaps =
            gr_face_find_fref(face, TRUETYPE_TAG('s','m','c','p')) != nullptr;
        ReleaseGrFace(face);
        mCheckedForGraphiteSmallCaps = true;
    }
    return mHasGraphiteSmallCaps;
}

bool
gfxFontEntry::GetColorLayersInfo(uint32_t aGlyphId,
                            nsTArray<uint16_t>& aLayerGlyphs,
                            nsTArray<mozilla::gfx::Color>& aLayerColors)
{
    return gfxFontUtils::GetColorGlyphLayers(mCOLR,
                                             mCPAL,
                                             aGlyphId,
                                             aLayerGlyphs,
                                             aLayerColors);
}

/* static */ size_t
gfxFontEntry::FontTableHashEntry::SizeOfEntryExcludingThis
    (FontTableHashEntry *aEntry,
     MallocSizeOf aMallocSizeOf,
     void* aUserArg)
{
    size_t n = 0;
    if (aEntry->mBlob) {
        n += aMallocSizeOf(aEntry->mBlob);
    }
    if (aEntry->mSharedBlobData) {
        n += aEntry->mSharedBlobData->SizeOfIncludingThis(aMallocSizeOf);
    }
    return n;
}

void
gfxFontEntry::AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                                     FontListSizes* aSizes) const
{
    aSizes->mFontListSize += mName.SizeOfExcludingThisIfUnshared(aMallocSizeOf);

    // cmaps are shared so only non-shared cmaps are included here
    if (mCharacterMap && mCharacterMap->mBuildOnTheFly) {
        aSizes->mCharMapsSize +=
            mCharacterMap->SizeOfIncludingThis(aMallocSizeOf);
    }
    if (mFontTableCache) {
        aSizes->mFontTableCacheSize +=
            mFontTableCache->SizeOfIncludingThis(
                FontTableHashEntry::SizeOfEntryExcludingThis,
                aMallocSizeOf);
    }
}

void
gfxFontEntry::AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf,
                                     FontListSizes* aSizes) const
{
    aSizes->mFontListSize += aMallocSizeOf(this);
    AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
}

//////////////////////////////////////////////////////////////////////////////
//
// class gfxFontFamily
//
//////////////////////////////////////////////////////////////////////////////

// we consider faces with mStandardFace == true to be "greater than" those with false,
// because during style matching, later entries will replace earlier ones
class FontEntryStandardFaceComparator {
  public:
    bool Equals(const nsRefPtr<gfxFontEntry>& a, const nsRefPtr<gfxFontEntry>& b) const {
        return a->mStandardFace == b->mStandardFace;
    }
    bool LessThan(const nsRefPtr<gfxFontEntry>& a, const nsRefPtr<gfxFontEntry>& b) const {
        return (a->mStandardFace == false && b->mStandardFace == true);
    }
};

void
gfxFontFamily::SortAvailableFonts()
{
    mAvailableFonts.Sort(FontEntryStandardFaceComparator());
}

bool
gfxFontFamily::HasOtherFamilyNames()
{
    // need to read in other family names to determine this
    if (!mOtherFamilyNamesInitialized) {
        ReadOtherFamilyNames(gfxPlatformFontList::PlatformFontList());  // sets mHasOtherFamilyNames
    }
    return mHasOtherFamilyNames;
}

gfxFontEntry*
gfxFontFamily::FindFontForStyle(const gfxFontStyle& aFontStyle, 
                                bool& aNeedsSyntheticBold)
{
    if (!mHasStyles)
        FindStyleVariations(); // collect faces for the family, if not already done

    NS_ASSERTION(mAvailableFonts.Length() > 0, "font family with no faces!");

    aNeedsSyntheticBold = false;

    int8_t baseWeight = aFontStyle.ComputeWeight();
    bool wantBold = baseWeight >= 6;

    // If the family has only one face, we simply return it; no further checking needed
    if (mAvailableFonts.Length() == 1) {
        gfxFontEntry *fe = mAvailableFonts[0];
        aNeedsSyntheticBold = wantBold && !fe->IsBold();
        return fe;
    }

    bool wantItalic = (aFontStyle.style &
                       (NS_FONT_STYLE_ITALIC | NS_FONT_STYLE_OBLIQUE)) != 0;

    // Most families are "simple", having just Regular/Bold/Italic/BoldItalic,
    // or some subset of these. In this case, we have exactly 4 entries in mAvailableFonts,
    // stored in the above order; note that some of the entries may be nullptr.
    // We can then pick the required entry based on whether the request is for
    // bold or non-bold, italic or non-italic, without running the more complex
    // matching algorithm used for larger families with many weights and/or widths.

    if (mIsSimpleFamily) {
        // Family has no more than the "standard" 4 faces, at fixed indexes;
        // calculate which one we want.
        // Note that we cannot simply return it as not all 4 faces are necessarily present.
        uint8_t faceIndex = (wantItalic ? kItalicMask : 0) |
                            (wantBold ? kBoldMask : 0);

        // if the desired style is available, return it directly
        gfxFontEntry *fe = mAvailableFonts[faceIndex];
        if (fe) {
            // no need to set aNeedsSyntheticBold here as we matched the boldness request
            return fe;
        }

        // order to check fallback faces in a simple family, depending on requested style
        static const uint8_t simpleFallbacks[4][3] = {
            { kBoldFaceIndex, kItalicFaceIndex, kBoldItalicFaceIndex },   // fallbacks for Regular
            { kRegularFaceIndex, kBoldItalicFaceIndex, kItalicFaceIndex },// Bold
            { kBoldItalicFaceIndex, kRegularFaceIndex, kBoldFaceIndex },  // Italic
            { kItalicFaceIndex, kBoldFaceIndex, kRegularFaceIndex }       // BoldItalic
        };
        const uint8_t *order = simpleFallbacks[faceIndex];

        for (uint8_t trial = 0; trial < 3; ++trial) {
            // check remaining faces in order of preference to find the first that actually exists
            fe = mAvailableFonts[order[trial]];
            if (fe) {
                aNeedsSyntheticBold = wantBold && !fe->IsBold();
                return fe;
            }
        }

        // this can't happen unless we have totally broken the font-list manager!
        NS_NOTREACHED("no face found in simple font family!");
        return nullptr;
    }

    // This is a large/rich font family, so we do full style- and weight-matching:
    // first collect a list of weights that are the best match for the requested
    // font-stretch and font-style, then pick the best weight match among those
    // available.

    gfxFontEntry *weightList[10] = { 0 };
    bool foundWeights = FindWeightsForStyle(weightList, wantItalic, aFontStyle.stretch);
    if (!foundWeights) {
        return nullptr;
    }

    // First find a match for the best weight
    int8_t matchBaseWeight = 0;
    int8_t i = baseWeight;

    // Need to special case when normal face doesn't exist but medium does.
    // In that case, use medium otherwise weights < 400
    if (baseWeight == 4 && !weightList[4]) {
        i = 5; // medium
    }

    // Loop through weights, since one exists loop will terminate
    int8_t direction = (baseWeight > 5) ? 1 : -1;
    for (; ; i += direction) {
        if (weightList[i]) {
            matchBaseWeight = i;
            break;
        }

        // If we've reached one side without finding a font,
        // start over and go the other direction until we find a match
        if (i == 1 || i == 9) {
            i = baseWeight;
            direction = -direction;
        }
    }

    NS_ASSERTION(matchBaseWeight != 0, 
                 "weight mapping should always find at least one font in a family");

    gfxFontEntry *matchFE = weightList[matchBaseWeight];

    NS_ASSERTION(matchFE,
                 "weight mapping should always find at least one font in a family");

    if (!matchFE->IsBold() && baseWeight >= 6)
    {
        aNeedsSyntheticBold = true;
    }

    return matchFE;
}

void
gfxFontFamily::CheckForSimpleFamily()
{
    // already checked this family
    if (mIsSimpleFamily) {
        return;
    };

    uint32_t count = mAvailableFonts.Length();
    if (count > 4 || count == 0) {
        return; // can't be "simple" if there are >4 faces;
                // if none then the family is unusable anyway
    }

    if (count == 1) {
        mIsSimpleFamily = true;
        return;
    }

    int16_t firstStretch = mAvailableFonts[0]->Stretch();

    gfxFontEntry *faces[4] = { 0 };
    for (uint8_t i = 0; i < count; ++i) {
        gfxFontEntry *fe = mAvailableFonts[i];
        if (fe->Stretch() != firstStretch) {
            return; // font-stretch doesn't match, don't treat as simple family
        }
        uint8_t faceIndex = (fe->IsItalic() ? kItalicMask : 0) |
                            (fe->Weight() >= 600 ? kBoldMask : 0);
        if (faces[faceIndex]) {
            return; // two faces resolve to the same slot; family isn't "simple"
        }
        faces[faceIndex] = fe;
    }

    // we have successfully slotted the available faces into the standard
    // 4-face framework
    mAvailableFonts.SetLength(4);
    for (uint8_t i = 0; i < 4; ++i) {
        if (mAvailableFonts[i].get() != faces[i]) {
            mAvailableFonts[i].swap(faces[i]);
        }
    }

    mIsSimpleFamily = true;
}

static inline uint32_t
StyleDistance(gfxFontEntry *aFontEntry,
              bool anItalic, int16_t aStretch)
{
    // Compute a measure of the "distance" between the requested style
    // and the given fontEntry,
    // considering italicness and font-stretch but not weight.

    int32_t distance = 0;
    if (aStretch != aFontEntry->mStretch) {
        // stretch values are in the range -4 .. +4
        // if aStretch is positive, we prefer more-positive values;
        // if zero or negative, prefer more-negative
        if (aStretch > 0) {
            distance = (aFontEntry->mStretch - aStretch) * 2;
        } else {
            distance = (aStretch - aFontEntry->mStretch) * 2;
        }
        // if the computed "distance" here is negative, it means that
        // aFontEntry lies in the "non-preferred" direction from aStretch,
        // so we treat that as larger than any preferred-direction distance
        // (max possible is 8) by adding an extra 10 to the absolute value
        if (distance < 0) {
            distance = -distance + 10;
        }
    }
    if (aFontEntry->IsItalic() != anItalic) {
        distance += 1;
    }
    return uint32_t(distance);
}

bool
gfxFontFamily::FindWeightsForStyle(gfxFontEntry* aFontsForWeights[],
                                   bool anItalic, int16_t aStretch)
{
    uint32_t foundWeights = 0;
    uint32_t bestMatchDistance = 0xffffffff;

    uint32_t count = mAvailableFonts.Length();
    for (uint32_t i = 0; i < count; i++) {
        // this is not called for "simple" families, and therefore it does not
        // need to check the mAvailableFonts entries for nullptr.
        gfxFontEntry *fe = mAvailableFonts[i];
        uint32_t distance = StyleDistance(fe, anItalic, aStretch);
        if (distance <= bestMatchDistance) {
            int8_t wt = fe->mWeight / 100;
            NS_ASSERTION(wt >= 1 && wt < 10, "invalid weight in fontEntry");
            if (!aFontsForWeights[wt]) {
                // record this as a possible candidate for weight matching
                aFontsForWeights[wt] = fe;
                ++foundWeights;
            } else {
                uint32_t prevDistance =
                    StyleDistance(aFontsForWeights[wt], anItalic, aStretch);
                if (prevDistance >= distance) {
                    // replacing a weight we already found,
                    // so don't increment foundWeights
                    aFontsForWeights[wt] = fe;
                }
            }
            bestMatchDistance = distance;
        }
    }

    NS_ASSERTION(foundWeights > 0, "Font family containing no faces?");

    if (foundWeights == 1) {
        // no need to cull entries if we only found one weight
        return true;
    }

    // we might have recorded some faces that were a partial style match, but later found
    // others that were closer; in this case, we need to cull the poorer matches from the
    // weight list we'll return
    for (uint32_t i = 0; i < 10; ++i) {
        if (aFontsForWeights[i] &&
            StyleDistance(aFontsForWeights[i], anItalic, aStretch) > bestMatchDistance)
        {
            aFontsForWeights[i] = 0;
        }
    }

    return (foundWeights > 0);
}


void gfxFontFamily::LocalizedName(nsAString& aLocalizedName)
{
    // just return the primary name; subclasses should override
    aLocalizedName = mName;
}

// metric for how close a given font matches a style
static int32_t
CalcStyleMatch(gfxFontEntry *aFontEntry, const gfxFontStyle *aStyle)
{
    int32_t rank = 0;
    if (aStyle) {
         // italics
         bool wantItalic =
             (aStyle->style & (NS_FONT_STYLE_ITALIC | NS_FONT_STYLE_OBLIQUE)) != 0;
         if (aFontEntry->IsItalic() == wantItalic) {
             rank += 10;
         }

        // measure of closeness of weight to the desired value
        rank += 9 - DeprecatedAbs(aFontEntry->Weight() / 100 - aStyle->ComputeWeight());
    } else {
        // if no font to match, prefer non-bold, non-italic fonts
        if (!aFontEntry->IsItalic()) {
            rank += 3;
        }
        if (!aFontEntry->IsBold()) {
            rank += 2;
        }
    }

    return rank;
}

#define RANK_MATCHED_CMAP   20

void
gfxFontFamily::FindFontForChar(GlobalFontMatch *aMatchData)
{
    if (mFamilyCharacterMapInitialized && !TestCharacterMap(aMatchData->mCh)) {
        // none of the faces in the family support the required char,
        // so bail out immediately
        return;
    }

    bool needsBold;
    gfxFontStyle normal;
    gfxFontEntry *fe = FindFontForStyle(
                  (aMatchData->mStyle == nullptr) ? *aMatchData->mStyle : normal,
                  needsBold);

    if (fe && !fe->SkipDuringSystemFallback()) {
        int32_t rank = 0;

        if (fe->TestCharacterMap(aMatchData->mCh)) {
            rank += RANK_MATCHED_CMAP;
            aMatchData->mCount++;
#ifdef PR_LOGGING
            PRLogModuleInfo *log = gfxPlatform::GetLog(eGfxLog_textrun);

            if (MOZ_UNLIKELY(PR_LOG_TEST(log, PR_LOG_DEBUG))) {
                uint32_t unicodeRange = FindCharUnicodeRange(aMatchData->mCh);
                uint32_t script = GetScriptCode(aMatchData->mCh);
                PR_LOG(log, PR_LOG_DEBUG,\
                       ("(textrun-systemfallback-fonts) char: u+%6.6x "
                        "unicode-range: %d script: %d match: [%s]\n",
                        aMatchData->mCh,
                        unicodeRange, script,
                        NS_ConvertUTF16toUTF8(fe->Name()).get()));
            }
#endif
        }

        aMatchData->mCmapsTested++;
        if (rank == 0) {
            return;
        }

         // omitting from original windows code -- family name, lang group, pitch
         // not available in current FontEntry implementation
        rank += CalcStyleMatch(fe, aMatchData->mStyle);

        // xxx - add whether AAT font with morphing info for specific lang groups

        if (rank > aMatchData->mMatchRank
            || (rank == aMatchData->mMatchRank &&
                Compare(fe->Name(), aMatchData->mBestMatch->Name()) > 0))
        {
            aMatchData->mBestMatch = fe;
            aMatchData->mMatchedFamily = this;
            aMatchData->mMatchRank = rank;
        }
    }
}

void
gfxFontFamily::SearchAllFontsForChar(GlobalFontMatch *aMatchData)
{
    uint32_t i, numFonts = mAvailableFonts.Length();
    for (i = 0; i < numFonts; i++) {
        gfxFontEntry *fe = mAvailableFonts[i];
        if (fe && fe->TestCharacterMap(aMatchData->mCh)) {
            int32_t rank = RANK_MATCHED_CMAP;
            rank += CalcStyleMatch(fe, aMatchData->mStyle);
            if (rank > aMatchData->mMatchRank
                || (rank == aMatchData->mMatchRank &&
                    Compare(fe->Name(), aMatchData->mBestMatch->Name()) > 0))
            {
                aMatchData->mBestMatch = fe;
                aMatchData->mMatchedFamily = this;
                aMatchData->mMatchRank = rank;
            }
        }
    }
}

/*static*/ void
gfxFontFamily::ReadOtherFamilyNamesForFace(const nsAString& aFamilyName,
                                           const char *aNameData,
                                           uint32_t aDataLength,
                                           nsTArray<nsString>& aOtherFamilyNames,
                                           bool useFullName)
{
    const gfxFontUtils::NameHeader *nameHeader =
        reinterpret_cast<const gfxFontUtils::NameHeader*>(aNameData);

    uint32_t nameCount = nameHeader->count;
    if (nameCount * sizeof(gfxFontUtils::NameRecord) > aDataLength) {
        NS_WARNING("invalid font (name records)");
        return;
    }
    
    const gfxFontUtils::NameRecord *nameRecord =
        reinterpret_cast<const gfxFontUtils::NameRecord*>(aNameData + sizeof(gfxFontUtils::NameHeader));
    uint32_t stringsBase = uint32_t(nameHeader->stringOffset);

    for (uint32_t i = 0; i < nameCount; i++, nameRecord++) {
        uint32_t nameLen = nameRecord->length;
        uint32_t nameOff = nameRecord->offset;  // offset from base of string storage

        if (stringsBase + nameOff + nameLen > aDataLength) {
            NS_WARNING("invalid font (name table strings)");
            return;
        }

        uint16_t nameID = nameRecord->nameID;
        if ((useFullName && nameID == gfxFontUtils::NAME_ID_FULL) ||
            (!useFullName && (nameID == gfxFontUtils::NAME_ID_FAMILY ||
                              nameID == gfxFontUtils::NAME_ID_PREFERRED_FAMILY))) {
            nsAutoString otherFamilyName;
            bool ok = gfxFontUtils::DecodeFontName(aNameData + stringsBase + nameOff,
                                                     nameLen,
                                                     uint32_t(nameRecord->platformID),
                                                     uint32_t(nameRecord->encodingID),
                                                     uint32_t(nameRecord->languageID),
                                                     otherFamilyName);
            // add if not same as canonical family name
            if (ok && otherFamilyName != aFamilyName) {
                aOtherFamilyNames.AppendElement(otherFamilyName);
            }
        }
    }
}

// returns true if other names were found, false otherwise
bool
gfxFontFamily::ReadOtherFamilyNamesForFace(gfxPlatformFontList *aPlatformFontList,
                                           hb_blob_t           *aNameTable,
                                           bool                 useFullName)
{
    uint32_t dataLength;
    const char *nameData = hb_blob_get_data(aNameTable, &dataLength);
    nsAutoTArray<nsString,4> otherFamilyNames;

    ReadOtherFamilyNamesForFace(mName, nameData, dataLength,
                                otherFamilyNames, useFullName);

    uint32_t n = otherFamilyNames.Length();
    for (uint32_t i = 0; i < n; i++) {
        aPlatformFontList->AddOtherFamilyName(this, otherFamilyNames[i]);
    }

    return n != 0;
}

void
gfxFontFamily::ReadOtherFamilyNames(gfxPlatformFontList *aPlatformFontList)
{
    if (mOtherFamilyNamesInitialized) 
        return;
    mOtherFamilyNamesInitialized = true;

    FindStyleVariations();

    // read in other family names for the first face in the list
    uint32_t i, numFonts = mAvailableFonts.Length();
    const uint32_t kNAME = TRUETYPE_TAG('n','a','m','e');

    for (i = 0; i < numFonts; ++i) {
        gfxFontEntry *fe = mAvailableFonts[i];
        if (!fe) {
            continue;
        }
        gfxFontEntry::AutoTable nameTable(fe, kNAME);
        if (!nameTable) {
            continue;
        }
        mHasOtherFamilyNames = ReadOtherFamilyNamesForFace(aPlatformFontList,
                                                           nameTable);
        break;
    }

    // read in other names for the first face in the list with the assumption
    // that if extra names don't exist in that face then they don't exist in
    // other faces for the same font
    if (!mHasOtherFamilyNames) 
        return;

    // read in names for all faces, needed to catch cases where fonts have
    // family names for individual weights (e.g. Hiragino Kaku Gothic Pro W6)
    for ( ; i < numFonts; i++) {
        gfxFontEntry *fe = mAvailableFonts[i];
        if (!fe) {
            continue;
        }
        gfxFontEntry::AutoTable nameTable(fe, kNAME);
        if (!nameTable) {
            continue;
        }
        ReadOtherFamilyNamesForFace(aPlatformFontList, nameTable);
    }
}

void
gfxFontFamily::ReadFaceNames(gfxPlatformFontList *aPlatformFontList, 
                             bool aNeedFullnamePostscriptNames,
                             FontInfoData *aFontInfoData)
{
    // if all needed names have already been read, skip
    if (mOtherFamilyNamesInitialized &&
        (mFaceNamesInitialized || !aNeedFullnamePostscriptNames))
        return;

    bool asyncFontLoaderDisabled = false;

#if defined(XP_MACOSX)
    // bug 975460 - async font loader crashes sometimes under 10.6, disable
    if (!nsCocoaFeatures::OnLionOrLater()) {
        asyncFontLoaderDisabled = true;
    }
#endif

    if (!mOtherFamilyNamesInitialized &&
        aFontInfoData &&
        aFontInfoData->mLoadOtherNames &&
        !asyncFontLoaderDisabled)
    {
        nsAutoTArray<nsString,4> otherFamilyNames;
        bool foundOtherNames =
            aFontInfoData->GetOtherFamilyNames(mName, otherFamilyNames);
        if (foundOtherNames) {
            uint32_t i, n = otherFamilyNames.Length();
            for (i = 0; i < n; i++) {
                aPlatformFontList->AddOtherFamilyName(this, otherFamilyNames[i]);
            }
        }
        mOtherFamilyNamesInitialized = true;
    }

    // if all needed data has been initialized, return
    if (mOtherFamilyNamesInitialized &&
        (mFaceNamesInitialized || !aNeedFullnamePostscriptNames)) {
        return;
    }

    FindStyleVariations(aFontInfoData);

    // check again, as style enumeration code may have loaded names
    if (mOtherFamilyNamesInitialized &&
        (mFaceNamesInitialized || !aNeedFullnamePostscriptNames)) {
        return;
    }

    uint32_t i, numFonts = mAvailableFonts.Length();
    const uint32_t kNAME = TRUETYPE_TAG('n','a','m','e');

    bool firstTime = true, readAllFaces = false;
    for (i = 0; i < numFonts; ++i) {
        gfxFontEntry *fe = mAvailableFonts[i];
        if (!fe) {
            continue;
        }

        nsAutoString fullname, psname;
        bool foundFaceNames = false;
        if (!mFaceNamesInitialized &&
            aNeedFullnamePostscriptNames &&
            aFontInfoData &&
            aFontInfoData->mLoadFaceNames) {
            aFontInfoData->GetFaceNames(fe->Name(), fullname, psname);
            if (!fullname.IsEmpty()) {
                aPlatformFontList->AddFullname(fe, fullname);
            }
            if (!psname.IsEmpty()) {
                aPlatformFontList->AddPostscriptName(fe, psname);
            }
            foundFaceNames = true;

            // found everything needed? skip to next font
            if (mOtherFamilyNamesInitialized) {
                continue;
            }
        }

        // load directly from the name table
        gfxFontEntry::AutoTable nameTable(fe, kNAME);
        if (!nameTable) {
            continue;
        }

        if (aNeedFullnamePostscriptNames && !foundFaceNames) {
            if (gfxFontUtils::ReadCanonicalName(
                    nameTable, gfxFontUtils::NAME_ID_FULL, fullname) == NS_OK)
            {
                aPlatformFontList->AddFullname(fe, fullname);
            }

            if (gfxFontUtils::ReadCanonicalName(
                    nameTable, gfxFontUtils::NAME_ID_POSTSCRIPT, psname) == NS_OK)
            {
                aPlatformFontList->AddPostscriptName(fe, psname);
            }
        }

        if (!mOtherFamilyNamesInitialized && (firstTime || readAllFaces)) {
            bool foundOtherName = ReadOtherFamilyNamesForFace(aPlatformFontList,
                                                              nameTable);

            // if the first face has a different name, scan all faces, otherwise
            // assume the family doesn't have other names
            if (firstTime && foundOtherName) {
                mHasOtherFamilyNames = true;
                readAllFaces = true;
            }
            firstTime = false;
        }

        // if not reading in any more names, skip other faces
        if (!readAllFaces && !aNeedFullnamePostscriptNames) {
            break;
        }
    }

    mFaceNamesInitialized = true;
    mOtherFamilyNamesInitialized = true;
}


gfxFontEntry*
gfxFontFamily::FindFont(const nsAString& aPostscriptName)
{
    // find the font using a simple linear search
    uint32_t numFonts = mAvailableFonts.Length();
    for (uint32_t i = 0; i < numFonts; i++) {
        gfxFontEntry *fe = mAvailableFonts[i].get();
        if (fe && fe->Name() == aPostscriptName)
            return fe;
    }
    return nullptr;
}

void
gfxFontFamily::ReadAllCMAPs(FontInfoData *aFontInfoData)
{
    FindStyleVariations(aFontInfoData);

    uint32_t i, numFonts = mAvailableFonts.Length();
    for (i = 0; i < numFonts; i++) {
        gfxFontEntry *fe = mAvailableFonts[i];
        // don't try to load cmaps for downloadable fonts not yet loaded
        if (!fe || fe->mIsProxy) {
            continue;
        }
        fe->ReadCMAP(aFontInfoData);
        mFamilyCharacterMap.Union(*(fe->mCharacterMap));
    }
    mFamilyCharacterMap.Compact();
    mFamilyCharacterMapInitialized = true;
}

void
gfxFontFamily::AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                                      FontListSizes* aSizes) const
{
    aSizes->mFontListSize +=
        mName.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
    aSizes->mCharMapsSize +=
        mFamilyCharacterMap.SizeOfExcludingThis(aMallocSizeOf);

    aSizes->mFontListSize +=
        mAvailableFonts.SizeOfExcludingThis(aMallocSizeOf);
    for (uint32_t i = 0; i < mAvailableFonts.Length(); ++i) {
        gfxFontEntry *fe = mAvailableFonts[i];
        if (fe) {
            fe->AddSizeOfIncludingThis(aMallocSizeOf, aSizes);
        }
    }
}

void
gfxFontFamily::AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf,
                                      FontListSizes* aSizes) const
{
    aSizes->mFontListSize += aMallocSizeOf(this);
    AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
}

/*
 * gfxFontCache - global cache of gfxFont instances.
 * Expires unused fonts after a short interval;
 * notifies fonts to age their cached shaped-word records;
 * observes memory-pressure notification and tells fonts to clear their
 * shaped-word caches to free up memory.
 */

MOZ_DEFINE_MALLOC_SIZE_OF(FontCacheMallocSizeOf)

NS_IMPL_ISUPPORTS(gfxFontCache::MemoryReporter, nsIMemoryReporter)

NS_IMETHODIMP
gfxFontCache::MemoryReporter::CollectReports
    (nsIMemoryReporterCallback* aCb,
     nsISupports* aClosure)
{
    FontCacheSizes sizes;

    gfxFontCache::GetCache()->AddSizeOfIncludingThis(&FontCacheMallocSizeOf,
                                                     &sizes);

    aCb->Callback(EmptyCString(),
                  NS_LITERAL_CSTRING("explicit/gfx/font-cache"),
                  KIND_HEAP, UNITS_BYTES, sizes.mFontInstances,
                  NS_LITERAL_CSTRING("Memory used for active font instances."),
                  aClosure);

    aCb->Callback(EmptyCString(),
                  NS_LITERAL_CSTRING("explicit/gfx/font-shaped-words"),
                  KIND_HEAP, UNITS_BYTES, sizes.mShapedWords,
                  NS_LITERAL_CSTRING("Memory used to cache shaped glyph data."),
                  aClosure);

    return NS_OK;
}

NS_IMPL_ISUPPORTS(gfxFontCache::Observer, nsIObserver)

NS_IMETHODIMP
gfxFontCache::Observer::Observe(nsISupports *aSubject,
                                const char *aTopic,
                                const char16_t *someData)
{
    if (!nsCRT::strcmp(aTopic, "memory-pressure")) {
        gfxFontCache *fontCache = gfxFontCache::GetCache();
        if (fontCache) {
            fontCache->FlushShapedWordCaches();
        }
    } else {
        NS_NOTREACHED("unexpected notification topic");
    }
    return NS_OK;
}

nsresult
gfxFontCache::Init()
{
    NS_ASSERTION(!gGlobalCache, "Where did this come from?");
    gGlobalCache = new gfxFontCache();
    if (!gGlobalCache) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    RegisterStrongMemoryReporter(new MemoryReporter());
    return NS_OK;
}

void
gfxFontCache::Shutdown()
{
    delete gGlobalCache;
    gGlobalCache = nullptr;

#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
    printf("Textrun storage high water mark=%d\n", gTextRunStorageHighWaterMark);
    printf("Total number of fonts=%d\n", gFontCount);
    printf("Total glyph extents allocated=%d (size %d)\n", gGlyphExtentsCount,
            int(gGlyphExtentsCount*sizeof(gfxGlyphExtents)));
    printf("Total glyph extents width-storage size allocated=%d\n", gGlyphExtentsWidthsTotalSize);
    printf("Number of simple glyph extents eagerly requested=%d\n", gGlyphExtentsSetupEagerSimple);
    printf("Number of tight glyph extents eagerly requested=%d\n", gGlyphExtentsSetupEagerTight);
    printf("Number of tight glyph extents lazily requested=%d\n", gGlyphExtentsSetupLazyTight);
    printf("Number of simple glyph extent setups that fell back to tight=%d\n", gGlyphExtentsSetupFallBackToTight);
#endif
}

gfxFontCache::gfxFontCache()
    : nsExpirationTracker<gfxFont,3>(FONT_TIMEOUT_SECONDS * 1000)
{
    nsCOMPtr<nsIObserverService> obs = GetObserverService();
    if (obs) {
        obs->AddObserver(new Observer, "memory-pressure", false);
    }

#ifndef RELEASE_BUILD
    // Currently disabled for release builds, due to unexplained crashes
    // during expiration; see bug 717175 & 894798.
    mWordCacheExpirationTimer = do_CreateInstance("@mozilla.org/timer;1");
    if (mWordCacheExpirationTimer) {
        mWordCacheExpirationTimer->
            InitWithFuncCallback(WordCacheExpirationTimerCallback, this,
                                 SHAPED_WORD_TIMEOUT_SECONDS * 1000,
                                 nsITimer::TYPE_REPEATING_SLACK);
    }
#endif
}

gfxFontCache::~gfxFontCache()
{
    // Ensure the user font cache releases its references to font entries,
    // so they aren't kept alive after the font instances and font-list
    // have been shut down.
    gfxUserFontSet::UserFontCache::Shutdown();

    if (mWordCacheExpirationTimer) {
        mWordCacheExpirationTimer->Cancel();
        mWordCacheExpirationTimer = nullptr;
    }

    // Expire everything that has a zero refcount, so we don't leak them.
    AgeAllGenerations();
    // All fonts should be gone.
    NS_WARN_IF_FALSE(mFonts.Count() == 0,
                     "Fonts still alive while shutting down gfxFontCache");
    // Note that we have to delete everything through the expiration
    // tracker, since there might be fonts not in the hashtable but in
    // the tracker.
}

bool
gfxFontCache::HashEntry::KeyEquals(const KeyTypePointer aKey) const
{
    return aKey->mFontEntry == mFont->GetFontEntry() &&
           aKey->mStyle->Equals(*mFont->GetStyle());
}

already_AddRefed<gfxFont>
gfxFontCache::Lookup(const gfxFontEntry *aFontEntry,
                     const gfxFontStyle *aStyle)
{
    Key key(aFontEntry, aStyle);
    HashEntry *entry = mFonts.GetEntry(key);

    Telemetry::Accumulate(Telemetry::FONT_CACHE_HIT, entry != nullptr);
    if (!entry)
        return nullptr;

    nsRefPtr<gfxFont> font = entry->mFont;
    return font.forget();
}

void
gfxFontCache::AddNew(gfxFont *aFont)
{
    Key key(aFont->GetFontEntry(), aFont->GetStyle());
    HashEntry *entry = mFonts.PutEntry(key);
    if (!entry)
        return;
    gfxFont *oldFont = entry->mFont;
    entry->mFont = aFont;
    // Assert that we can find the entry we just put in (this fails if the key
    // has a NaN float value in it, e.g. 'sizeAdjust').
    MOZ_ASSERT(entry == mFonts.GetEntry(key));
    // If someone's asked us to replace an existing font entry, then that's a
    // bit weird, but let it happen, and expire the old font if it's not used.
    if (oldFont && oldFont->GetExpirationState()->IsTracked()) {
        // if oldFont == aFont, recount should be > 0,
        // so we shouldn't be here.
        NS_ASSERTION(aFont != oldFont, "new font is tracked for expiry!");
        NotifyExpired(oldFont);
    }
}

void
gfxFontCache::NotifyReleased(gfxFont *aFont)
{
    nsresult rv = AddObject(aFont);
    if (NS_FAILED(rv)) {
        // We couldn't track it for some reason. Kill it now.
        DestroyFont(aFont);
    }
    // Note that we might have fonts that aren't in the hashtable, perhaps because
    // of OOM adding to the hashtable or because someone did an AddNew where
    // we already had a font. These fonts are added to the expiration tracker
    // anyway, even though Lookup can't resurrect them. Eventually they will
    // expire and be deleted.
}

void
gfxFontCache::NotifyExpired(gfxFont *aFont)
{
    aFont->ClearCachedWords();
    RemoveObject(aFont);
    DestroyFont(aFont);
}

void
gfxFontCache::DestroyFont(gfxFont *aFont)
{
    Key key(aFont->GetFontEntry(), aFont->GetStyle());
    HashEntry *entry = mFonts.GetEntry(key);
    if (entry && entry->mFont == aFont) {
        mFonts.RemoveEntry(key);
    }
    NS_ASSERTION(aFont->GetRefCount() == 0,
                 "Destroying with non-zero ref count!");
    delete aFont;
}

/*static*/
PLDHashOperator
gfxFontCache::AgeCachedWordsForFont(HashEntry* aHashEntry, void* aUserData)
{
    aHashEntry->mFont->AgeCachedWords();
    return PL_DHASH_NEXT;
}

/*static*/
void
gfxFontCache::WordCacheExpirationTimerCallback(nsITimer* aTimer, void* aCache)
{
    gfxFontCache* cache = static_cast<gfxFontCache*>(aCache);
    cache->mFonts.EnumerateEntries(AgeCachedWordsForFont, nullptr);
}

/*static*/
PLDHashOperator
gfxFontCache::ClearCachedWordsForFont(HashEntry* aHashEntry, void* aUserData)
{
    aHashEntry->mFont->ClearCachedWords();
    return PL_DHASH_NEXT;
}

/*static*/
size_t
gfxFontCache::AddSizeOfFontEntryExcludingThis(HashEntry* aHashEntry,
                                              MallocSizeOf aMallocSizeOf,
                                              void* aUserArg)
{
    HashEntry *entry = static_cast<HashEntry*>(aHashEntry);
    FontCacheSizes *sizes = static_cast<FontCacheSizes*>(aUserArg);
    entry->mFont->AddSizeOfExcludingThis(aMallocSizeOf, sizes);

    // The entry's size is recorded in the |sizes| parameter, so we return zero
    // here to the hashtable enumerator.
    return 0;
}

void
gfxFontCache::AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                                     FontCacheSizes* aSizes) const
{
    // TODO: add the overhead of the expiration tracker (generation arrays)

    aSizes->mFontInstances +=
        mFonts.SizeOfExcludingThis(AddSizeOfFontEntryExcludingThis,
                                   aMallocSizeOf, aSizes);
}

void
gfxFontCache::AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf,
                                     FontCacheSizes* aSizes) const
{
    aSizes->mFontInstances += aMallocSizeOf(this);
    AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
}

#define MAX_SSXX_VALUE 99
#define MAX_CVXX_VALUE 99

static void
LookupAlternateValues(gfxFontFeatureValueSet *featureLookup,
                      const nsAString& aFamily,
                      const nsTArray<gfxAlternateValue>& altValue,
                      nsTArray<gfxFontFeature>& aFontFeatures)
{
    uint32_t numAlternates = altValue.Length();
    for (uint32_t i = 0; i < numAlternates; i++) {
        const gfxAlternateValue& av = altValue.ElementAt(i);
        nsAutoTArray<uint32_t,4> values;

        // map <family, name, feature> ==> <values>
        bool found =
            featureLookup->GetFontFeatureValuesFor(aFamily, av.alternate,
                                                   av.value, values);
        uint32_t numValues = values.Length();

        // nothing defined, skip
        if (!found || numValues == 0) {
            continue;
        }

        gfxFontFeature feature;
        if (av.alternate == NS_FONT_VARIANT_ALTERNATES_CHARACTER_VARIANT) {
            NS_ASSERTION(numValues <= 2,
                         "too many values allowed for character-variant");
            // character-variant(12 3) ==> 'cv12' = 3
            uint32_t nn = values.ElementAt(0);
            // ignore values greater than 99
            if (nn == 0 || nn > MAX_CVXX_VALUE) {
                continue;
            }
            feature.mValue = 1;
            if (numValues > 1) {
                feature.mValue = values.ElementAt(1);
            }
            feature.mTag = HB_TAG('c','v',('0' + nn / 10), ('0' + nn % 10));
            aFontFeatures.AppendElement(feature);

        } else if (av.alternate == NS_FONT_VARIANT_ALTERNATES_STYLESET) {
            // styleset(1 2 7) ==> 'ss01' = 1, 'ss02' = 1, 'ss07' = 1
            feature.mValue = 1;
            for (uint32_t v = 0; v < numValues; v++) {
                uint32_t nn = values.ElementAt(v);
                if (nn == 0 || nn > MAX_SSXX_VALUE) {
                    continue;
                }
                feature.mTag = HB_TAG('s','s',('0' + nn / 10), ('0' + nn % 10));
                aFontFeatures.AppendElement(feature);
            }

        } else {
            NS_ASSERTION(numValues == 1,
                   "too many values for font-specific font-variant-alternates");
            feature.mValue = values.ElementAt(0);

            switch (av.alternate) {
                case NS_FONT_VARIANT_ALTERNATES_STYLISTIC:  // salt
                    feature.mTag = HB_TAG('s','a','l','t');
                    break;
                case NS_FONT_VARIANT_ALTERNATES_SWASH:  // swsh, cswh
                    feature.mTag = HB_TAG('s','w','s','h');
                    aFontFeatures.AppendElement(feature);
                    feature.mTag = HB_TAG('c','s','w','h');
                    break;
                case NS_FONT_VARIANT_ALTERNATES_ORNAMENTS: // ornm
                    feature.mTag = HB_TAG('o','r','n','m');
                    break;
                case NS_FONT_VARIANT_ALTERNATES_ANNOTATION: // nalt
                    feature.mTag = HB_TAG('n','a','l','t');
                    break;
                default:
                    feature.mTag = 0;
                    break;
            }

            NS_ASSERTION(feature.mTag, "unsupported alternate type");
            if (!feature.mTag) {
                continue;
            }
            aFontFeatures.AppendElement(feature);
        }
    }
}

/* static */ bool
gfxFontShaper::MergeFontFeatures(
    const gfxFontStyle *aStyle,
    const nsTArray<gfxFontFeature>& aFontFeatures,
    bool aDisableLigatures,
    const nsAString& aFamilyName,
    nsDataHashtable<nsUint32HashKey,uint32_t>& aMergedFeatures)
{
    uint32_t numAlts = aStyle->alternateValues.Length();
    const nsTArray<gfxFontFeature>& styleRuleFeatures =
        aStyle->featureSettings;

    // bail immediately if nothing to do
    if (styleRuleFeatures.IsEmpty() &&
        aFontFeatures.IsEmpty() &&
        !aDisableLigatures &&
        !aStyle->smallCaps &&
        numAlts == 0) {
        return false;
    }

    // Ligature features are enabled by default in the generic shaper,
    // so we explicitly turn them off if necessary (for letter-spacing)
    if (aDisableLigatures) {
        aMergedFeatures.Put(HB_TAG('l','i','g','a'), 0);
        aMergedFeatures.Put(HB_TAG('c','l','i','g'), 0);
    }

    if (aStyle->smallCaps) {
        aMergedFeatures.Put(HB_TAG('s','m','c','p'), 1);
    }

    // add feature values from font
    uint32_t i, count;

    count = aFontFeatures.Length();
    for (i = 0; i < count; i++) {
        const gfxFontFeature& feature = aFontFeatures.ElementAt(i);
        aMergedFeatures.Put(feature.mTag, feature.mValue);
    }

    // add font-specific feature values from style rules
    if (aStyle->featureValueLookup && numAlts > 0) {
        nsAutoTArray<gfxFontFeature,4> featureList;

        // insert list of alternate feature settings
        LookupAlternateValues(aStyle->featureValueLookup, aFamilyName,
                              aStyle->alternateValues, featureList);

        count = featureList.Length();
        for (i = 0; i < count; i++) {
            const gfxFontFeature& feature = featureList.ElementAt(i);
            aMergedFeatures.Put(feature.mTag, feature.mValue);
        }
    }

    // add feature values from style rules
    count = styleRuleFeatures.Length();
    for (i = 0; i < count; i++) {
        const gfxFontFeature& feature = styleRuleFeatures.ElementAt(i);
        aMergedFeatures.Put(feature.mTag, feature.mValue);
    }

    return aMergedFeatures.Count() != 0;
}

void
gfxFont::RunMetrics::CombineWith(const RunMetrics& aOther, bool aOtherIsOnLeft)
{
    mAscent = std::max(mAscent, aOther.mAscent);
    mDescent = std::max(mDescent, aOther.mDescent);
    if (aOtherIsOnLeft) {
        mBoundingBox =
            (mBoundingBox + gfxPoint(aOther.mAdvanceWidth, 0)).Union(aOther.mBoundingBox);
    } else {
        mBoundingBox =
            mBoundingBox.Union(aOther.mBoundingBox + gfxPoint(mAdvanceWidth, 0));
    }
    mAdvanceWidth += aOther.mAdvanceWidth;
}

gfxFont::gfxFont(gfxFontEntry *aFontEntry, const gfxFontStyle *aFontStyle,
                 AntialiasOption anAAOption, cairo_scaled_font_t *aScaledFont) :
    mScaledFont(aScaledFont),
    mFontEntry(aFontEntry), mIsValid(true),
    mApplySyntheticBold(false),
    mStyle(*aFontStyle),
    mAdjustedSize(0.0),
    mFUnitsConvFactor(0.0f),
    mAntialiasOption(anAAOption)
{
#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
    ++gFontCount;
#endif
    mKerningSet = HasFeatureSet(HB_TAG('k','e','r','n'), mKerningEnabled);
}

static PLDHashOperator
NotifyFontDestroyed(nsPtrHashKey<gfxFont::GlyphChangeObserver>* aKey,
                    void* aClosure)
{
    aKey->GetKey()->ForgetFont();
    return PL_DHASH_NEXT;
}

gfxFont::~gfxFont()
{
    uint32_t i, count = mGlyphExtentsArray.Length();
    // We destroy the contents of mGlyphExtentsArray explicitly instead of
    // using nsAutoPtr because VC++ can't deal with nsTArrays of nsAutoPtrs
    // of classes that lack a proper copy constructor
    for (i = 0; i < count; ++i) {
        delete mGlyphExtentsArray[i];
    }

    mFontEntry->NotifyFontDestroyed(this);

    if (mGlyphChangeObservers) {
        mGlyphChangeObservers->EnumerateEntries(NotifyFontDestroyed, nullptr);
    }
}

gfxFloat
gfxFont::GetGlyphHAdvance(gfxContext *aCtx, uint16_t aGID)
{
    if (!SetupCairoFont(aCtx)) {
        return 0;
    }
    if (ProvidesGlyphWidths()) {
        return GetGlyphWidth(aCtx, aGID) / 65536.0;
    }
    if (mFUnitsConvFactor == 0.0f) {
        GetMetrics();
    }
    NS_ASSERTION(mFUnitsConvFactor > 0.0f,
                 "missing font unit conversion factor");
    if (!mHarfBuzzShaper) {
        mHarfBuzzShaper = new gfxHarfBuzzShaper(this);
    }
    gfxHarfBuzzShaper* shaper =
        static_cast<gfxHarfBuzzShaper*>(mHarfBuzzShaper.get());
    if (!shaper->Initialize()) {
        return 0;
    }
    return shaper->GetGlyphHAdvance(aCtx, aGID) / 65536.0;
}

/*static*/
PLDHashOperator
gfxFont::AgeCacheEntry(CacheHashEntry *aEntry, void *aUserData)
{
    if (!aEntry->mShapedWord) {
        NS_ASSERTION(aEntry->mShapedWord, "cache entry has no gfxShapedWord!");
        return PL_DHASH_REMOVE;
    }
    if (aEntry->mShapedWord->IncrementAge() == kShapedWordCacheMaxAge) {
        return PL_DHASH_REMOVE;
    }
    return PL_DHASH_NEXT;
}

static void
CollectLookupsByFeature(hb_face_t *aFace, hb_tag_t aTableTag,
                        uint32_t aFeatureIndex, hb_set_t *aLookups)
{
    uint32_t lookups[32];
    uint32_t i, len, offset;

    offset = 0;
    do {
        len = ArrayLength(lookups);
        hb_ot_layout_feature_get_lookups(aFace, aTableTag, aFeatureIndex,
                                         offset, &len, lookups);
        for (i = 0; i < len; i++) {
            hb_set_add(aLookups, lookups[i]);
        }
        offset += len;
    } while (len == ArrayLength(lookups));
}

static void
CollectLookupsByLanguage(hb_face_t *aFace, hb_tag_t aTableTag,
                         const nsTHashtable<nsUint32HashKey>&
                             aSpecificFeatures,
                         hb_set_t *aOtherLookups,
                         hb_set_t *aSpecificFeatureLookups,
                         uint32_t aScriptIndex, uint32_t aLangIndex)
{
    uint32_t reqFeatureIndex;
    if (hb_ot_layout_language_get_required_feature_index(aFace, aTableTag,
                                                         aScriptIndex,
                                                         aLangIndex,
                                                         &reqFeatureIndex)) {
        CollectLookupsByFeature(aFace, aTableTag, reqFeatureIndex,
                                aOtherLookups);
    }

    uint32_t featureIndexes[32];
    uint32_t i, len, offset;

    offset = 0;
    do {
        len = ArrayLength(featureIndexes);
        hb_ot_layout_language_get_feature_indexes(aFace, aTableTag,
                                                  aScriptIndex, aLangIndex,
                                                  offset, &len, featureIndexes);

        for (i = 0; i < len; i++) {
            uint32_t featureIndex = featureIndexes[i];

            // get the feature tag
            hb_tag_t featureTag;
            uint32_t tagLen = 1;
            hb_ot_layout_language_get_feature_tags(aFace, aTableTag,
                                                   aScriptIndex, aLangIndex,
                                                   offset + i, &tagLen,
                                                   &featureTag);

            // collect lookups
            hb_set_t *lookups = aSpecificFeatures.GetEntry(featureTag) ?
                                    aSpecificFeatureLookups : aOtherLookups;
            CollectLookupsByFeature(aFace, aTableTag, featureIndex, lookups);
        }
        offset += len;
    } while (len == ArrayLength(featureIndexes));
}

static bool
HasLookupRuleWithGlyphByScript(hb_face_t *aFace, hb_tag_t aTableTag,
                               hb_tag_t aScriptTag, uint32_t aScriptIndex,
                               uint16_t aGlyph,
                               const nsTHashtable<nsUint32HashKey>&
                                   aDefaultFeatures,
                               bool& aHasDefaultFeatureWithGlyph)
{
    uint32_t numLangs, lang;
    hb_set_t *defaultFeatureLookups = hb_set_create();
    hb_set_t *nonDefaultFeatureLookups = hb_set_create();

    // default lang
    CollectLookupsByLanguage(aFace, aTableTag, aDefaultFeatures,
                             nonDefaultFeatureLookups, defaultFeatureLookups,
                             aScriptIndex,
                             HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX);

    // iterate over langs
    numLangs = hb_ot_layout_script_get_language_tags(aFace, aTableTag,
                                                     aScriptIndex, 0,
                                                     nullptr, nullptr);
    for (lang = 0; lang < numLangs; lang++) {
        CollectLookupsByLanguage(aFace, aTableTag, aDefaultFeatures,
                                 nonDefaultFeatureLookups,
                                 defaultFeatureLookups,
                                 aScriptIndex, lang);
    }

    // look for the glyph among default feature lookups
    aHasDefaultFeatureWithGlyph = false;
    hb_set_t *glyphs = hb_set_create();
    hb_codepoint_t index = -1;
    while (hb_set_next(defaultFeatureLookups, &index)) {
        hb_ot_layout_lookup_collect_glyphs(aFace, aTableTag, index,
                                           glyphs, glyphs, glyphs,
                                           glyphs);
        if (hb_set_has(glyphs, aGlyph)) {
            aHasDefaultFeatureWithGlyph = true;
            break;
        }
    }

    // look for the glyph among non-default feature lookups
    // if no default feature lookups contained spaces
    bool hasNonDefaultFeatureWithGlyph = false;
    if (!aHasDefaultFeatureWithGlyph) {
        hb_set_clear(glyphs);
        index = -1;
        while (hb_set_next(nonDefaultFeatureLookups, &index)) {
            hb_ot_layout_lookup_collect_glyphs(aFace, aTableTag, index,
                                               glyphs, glyphs, glyphs,
                                               glyphs);
            if (hb_set_has(glyphs, aGlyph)) {
                hasNonDefaultFeatureWithGlyph = true;
                break;
            }
        }
    }

    hb_set_destroy(glyphs);
    hb_set_destroy(defaultFeatureLookups);
    hb_set_destroy(nonDefaultFeatureLookups);

    return aHasDefaultFeatureWithGlyph || hasNonDefaultFeatureWithGlyph;
}

static void
HasLookupRuleWithGlyph(hb_face_t *aFace, hb_tag_t aTableTag, bool& aHasGlyph,
                       hb_tag_t aSpecificFeature, bool& aHasGlyphSpecific,
                       uint16_t aGlyph)
{
    // iterate over the scripts in the font
    uint32_t numScripts, numLangs, script, lang;
    hb_set_t *otherLookups = hb_set_create();
    hb_set_t *specificFeatureLookups = hb_set_create();
    nsTHashtable<nsUint32HashKey> specificFeature;

    specificFeature.PutEntry(aSpecificFeature);

    numScripts = hb_ot_layout_table_get_script_tags(aFace, aTableTag, 0,
                                                    nullptr, nullptr);

    for (script = 0; script < numScripts; script++) {
        // default lang
        CollectLookupsByLanguage(aFace, aTableTag, specificFeature,
                                 otherLookups, specificFeatureLookups,
                                 script, HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX);

        // iterate over langs
        numLangs = hb_ot_layout_script_get_language_tags(aFace, HB_OT_TAG_GPOS,
                                                         script, 0,
                                                         nullptr, nullptr);
        for (lang = 0; lang < numLangs; lang++) {
            CollectLookupsByLanguage(aFace, aTableTag, specificFeature,
                                     otherLookups, specificFeatureLookups,
                                     script, lang);
        }
    }

    // look for the glyph among non-specific feature lookups
    hb_set_t *glyphs = hb_set_create();
    hb_codepoint_t index = -1;
    while (hb_set_next(otherLookups, &index)) {
        hb_ot_layout_lookup_collect_glyphs(aFace, aTableTag, index,
                                           glyphs, glyphs, glyphs,
                                           glyphs);
        if (hb_set_has(glyphs, aGlyph)) {
            aHasGlyph = true;
            break;
        }
    }

    // look for the glyph among specific feature lookups
    hb_set_clear(glyphs);
    index = -1;
    while (hb_set_next(specificFeatureLookups, &index)) {
        hb_ot_layout_lookup_collect_glyphs(aFace, aTableTag, index,
                                           glyphs, glyphs, glyphs,
                                           glyphs);
        if (hb_set_has(glyphs, aGlyph)) {
            aHasGlyphSpecific = true;
            break;
        }
    }

    hb_set_destroy(glyphs);
    hb_set_destroy(specificFeatureLookups);
    hb_set_destroy(otherLookups);
}

nsDataHashtable<nsUint32HashKey, int32_t> *gfxFont::sScriptTagToCode = nullptr;
nsTHashtable<nsUint32HashKey>             *gfxFont::sDefaultFeatures = nullptr;

static inline bool
HasSubstitution(uint32_t *aBitVector, uint32_t aBit) {
    return (aBitVector[aBit >> 5] & (1 << (aBit & 0x1f))) != 0;
}

// union of all default substitution features across scripts
static const hb_tag_t defaultFeatures[] = {
    HB_TAG('a','b','v','f'),
    HB_TAG('a','b','v','s'),
    HB_TAG('a','k','h','n'),
    HB_TAG('b','l','w','f'),
    HB_TAG('b','l','w','s'),
    HB_TAG('c','a','l','t'),
    HB_TAG('c','c','m','p'),
    HB_TAG('c','f','a','r'),
    HB_TAG('c','j','c','t'),
    HB_TAG('c','l','i','g'),
    HB_TAG('f','i','n','2'),
    HB_TAG('f','i','n','3'),
    HB_TAG('f','i','n','a'),
    HB_TAG('h','a','l','f'),
    HB_TAG('h','a','l','n'),
    HB_TAG('i','n','i','t'),
    HB_TAG('i','s','o','l'),
    HB_TAG('l','i','g','a'),
    HB_TAG('l','j','m','o'),
    HB_TAG('l','o','c','l'),
    HB_TAG('l','t','r','a'),
    HB_TAG('l','t','r','m'),
    HB_TAG('m','e','d','2'),
    HB_TAG('m','e','d','i'),
    HB_TAG('m','s','e','t'),
    HB_TAG('n','u','k','t'),
    HB_TAG('p','r','e','f'),
    HB_TAG('p','r','e','s'),
    HB_TAG('p','s','t','f'),
    HB_TAG('p','s','t','s'),
    HB_TAG('r','c','l','t'),
    HB_TAG('r','l','i','g'),
    HB_TAG('r','k','r','f'),
    HB_TAG('r','p','h','f'),
    HB_TAG('r','t','l','a'),
    HB_TAG('r','t','l','m'),
    HB_TAG('t','j','m','o'),
    HB_TAG('v','a','t','u'),
    HB_TAG('v','e','r','t'),
    HB_TAG('v','j','m','o')
};

void
gfxFont::CheckForFeaturesInvolvingSpace()
{
    mFontEntry->mHasSpaceFeaturesInitialized = true;

#ifdef PR_LOGGING
    bool log = LOG_FONTINIT_ENABLED();
    TimeStamp start;
    if (MOZ_UNLIKELY(log)) {
        start = TimeStamp::Now();
    }
#endif

    bool result = false;

    uint32_t spaceGlyph = GetSpaceGlyph();
    if (!spaceGlyph) {
        return;
    }

    hb_face_t *face = GetFontEntry()->GetHBFace();

    // GSUB lookups - examine per script
    if (hb_ot_layout_has_substitution(face)) {

        // set up the script ==> code hashtable if needed
        if (!sScriptTagToCode) {
            sScriptTagToCode =
                new nsDataHashtable<nsUint32HashKey,
                                    int32_t>(MOZ_NUM_SCRIPT_CODES);
            sScriptTagToCode->Put(HB_TAG('D','F','L','T'), MOZ_SCRIPT_COMMON);
            for (int32_t s = MOZ_SCRIPT_ARABIC; s < MOZ_NUM_SCRIPT_CODES; s++) {
                hb_script_t scriptTag = hb_script_t(GetScriptTagForCode(s));
                hb_tag_t s1, s2;
                hb_ot_tags_from_script(scriptTag, &s1, &s2);
                sScriptTagToCode->Put(s1, s);
                if (s2 != HB_OT_TAG_DEFAULT_SCRIPT) {
                    sScriptTagToCode->Put(s2, s);
                }
            }

            uint32_t numDefaultFeatures = ArrayLength(defaultFeatures);
            sDefaultFeatures =
                new nsTHashtable<nsUint32HashKey>(numDefaultFeatures);
            for (uint32_t i = 0; i < numDefaultFeatures; i++) {
                sDefaultFeatures->PutEntry(defaultFeatures[i]);
            }
        }

        // iterate over the scripts in the font
        hb_tag_t scriptTags[8];

        uint32_t len, offset = 0;
        do {
            len = ArrayLength(scriptTags);
            hb_ot_layout_table_get_script_tags(face, HB_OT_TAG_GSUB, offset,
                                               &len, scriptTags);
            for (uint32_t i = 0; i < len; i++) {
                bool isDefaultFeature = false;
                int32_t s;
                if (!HasLookupRuleWithGlyphByScript(face, HB_OT_TAG_GSUB,
                                                    scriptTags[i], offset + i,
                                                    spaceGlyph,
                                                    *sDefaultFeatures,
                                                    isDefaultFeature) ||
                    !sScriptTagToCode->Get(scriptTags[i], &s))
                {
                    continue;
                }
                result = true;
                uint32_t index = s >> 5;
                uint32_t bit = s & 0x1f;
                if (isDefaultFeature) {
                    mFontEntry->mDefaultSubSpaceFeatures[index] |= (1 << bit);
                } else {
                    mFontEntry->mNonDefaultSubSpaceFeatures[index] |= (1 << bit);
                }
            }
            offset += len;
        } while (len == ArrayLength(scriptTags));
    }

    // spaces in default features of default script?
    // ==> can't use word cache, skip GPOS analysis
    bool canUseWordCache = true;
    if (HasSubstitution(mFontEntry->mDefaultSubSpaceFeatures,
                        MOZ_SCRIPT_COMMON)) {
        canUseWordCache = false;
    }

    // GPOS lookups - distinguish kerning from non-kerning features
    mFontEntry->mHasSpaceFeaturesKerning = false;
    mFontEntry->mHasSpaceFeaturesNonKerning = false;

    if (canUseWordCache && hb_ot_layout_has_positioning(face)) {
        bool hasKerning = false, hasNonKerning = false;
        HasLookupRuleWithGlyph(face, HB_OT_TAG_GPOS, hasNonKerning,
                               HB_TAG('k','e','r','n'), hasKerning, spaceGlyph);
        if (hasKerning || hasNonKerning) {
            result = true;
        }
        mFontEntry->mHasSpaceFeaturesKerning = hasKerning;
        mFontEntry->mHasSpaceFeaturesNonKerning = hasNonKerning;
    }

    hb_face_destroy(face);
    mFontEntry->mHasSpaceFeatures = result;

#ifdef PR_LOGGING
    if (MOZ_UNLIKELY(log)) {
        TimeDuration elapsed = TimeStamp::Now() - start;
        LOG_FONTINIT((
            "(fontinit-spacelookups) font: %s - "
            "subst default: %8.8x %8.8x %8.8x %8.8x "
            "subst non-default: %8.8x %8.8x %8.8x %8.8x "
            "kerning: %s non-kerning: %s time: %6.3f\n",
            NS_ConvertUTF16toUTF8(mFontEntry->Name()).get(),
            mFontEntry->mDefaultSubSpaceFeatures[3],
            mFontEntry->mDefaultSubSpaceFeatures[2],
            mFontEntry->mDefaultSubSpaceFeatures[1],
            mFontEntry->mDefaultSubSpaceFeatures[0],
            mFontEntry->mNonDefaultSubSpaceFeatures[3],
            mFontEntry->mNonDefaultSubSpaceFeatures[2],
            mFontEntry->mNonDefaultSubSpaceFeatures[1],
            mFontEntry->mNonDefaultSubSpaceFeatures[0],
            (mFontEntry->mHasSpaceFeaturesKerning ? "true" : "false"),
            (mFontEntry->mHasSpaceFeaturesNonKerning ? "true" : "false"),
            elapsed.ToMilliseconds()
        ));
    }
#endif
}

bool
gfxFont::HasSubstitutionRulesWithSpaceLookups(int32_t aRunScript)
{
    NS_ASSERTION(GetFontEntry()->mHasSpaceFeaturesInitialized,
                 "need to initialize space lookup flags");
    NS_ASSERTION(aRunScript < MOZ_NUM_SCRIPT_CODES, "weird script code");
    if (aRunScript == MOZ_SCRIPT_INVALID ||
        aRunScript >= MOZ_NUM_SCRIPT_CODES) {
        return false;
    }

    // default features have space lookups ==> true
    if (HasSubstitution(mFontEntry->mDefaultSubSpaceFeatures,
                        MOZ_SCRIPT_COMMON) ||
        HasSubstitution(mFontEntry->mDefaultSubSpaceFeatures,
                        aRunScript))
    {
        return true;
    }

    // non-default features have space lookups and some type of
    // font feature, in font or style is specified ==> true
    if ((HasSubstitution(mFontEntry->mNonDefaultSubSpaceFeatures,
                         MOZ_SCRIPT_COMMON) ||
         HasSubstitution(mFontEntry->mNonDefaultSubSpaceFeatures,
                         aRunScript)) &&
        (!mStyle.featureSettings.IsEmpty() ||
         !mFontEntry->mFeatureSettings.IsEmpty()))
    {
        return true;
    }

    return false;
}

bool
gfxFont::SpaceMayParticipateInShaping(int32_t aRunScript)
{
    // avoid checking fonts known not to include default space-dependent features
    if (MOZ_UNLIKELY(mFontEntry->mSkipDefaultFeatureSpaceCheck)) {
        if (!mKerningSet && mStyle.featureSettings.IsEmpty() &&
            mFontEntry->mFeatureSettings.IsEmpty()) {
            return false;
        }
    }

    // We record the presence of space-dependent features in the font entry
    // so that subsequent instantiations for the same font face won't
    // require us to re-check the tables; however, the actual check is done
    // by gfxFont because not all font entry subclasses know how to create
    // a harfbuzz face for introspection.
    if (!mFontEntry->mHasSpaceFeaturesInitialized) {
        CheckForFeaturesInvolvingSpace();
    }

    if (!mFontEntry->mHasSpaceFeatures) {
        return false;
    }

    // if font has substitution rules or non-kerning positioning rules
    // that involve spaces, bypass
    if (HasSubstitutionRulesWithSpaceLookups(aRunScript) ||
        mFontEntry->mHasSpaceFeaturesNonKerning) {
        return true;
    }

    // if kerning explicitly enabled/disabled via font-feature-settings or
    // font-kerning and kerning rules use spaces, only bypass when enabled
    if (mKerningSet && mFontEntry->mHasSpaceFeaturesKerning) {
        return mKerningEnabled;
    }

    return false;
}

bool
gfxFont::SupportsSmallCaps(int32_t aScript)
{
    if (mGraphiteShaper && gfxPlatform::GetPlatform()->UseGraphiteShaping()) {
        return GetFontEntry()->SupportsGraphiteSmallCaps();
    }

    return GetFontEntry()->SupportsOpenTypeSmallCaps(aScript);
}

bool
gfxFont::HasFeatureSet(uint32_t aFeature, bool& aFeatureOn)
{
    aFeatureOn = false;

    if (mStyle.featureSettings.IsEmpty() &&
        GetFontEntry()->mFeatureSettings.IsEmpty()) {
        return false;
    }

    // add feature values from font
    bool featureSet = false;
    uint32_t i, count;

    nsTArray<gfxFontFeature>& fontFeatures = GetFontEntry()->mFeatureSettings;
    count = fontFeatures.Length();
    for (i = 0; i < count; i++) {
        const gfxFontFeature& feature = fontFeatures.ElementAt(i);
        if (feature.mTag == aFeature) {
            featureSet = true;
            aFeatureOn = (feature.mValue != 0);
        }
    }

    // add feature values from style rules
    nsTArray<gfxFontFeature>& styleFeatures = mStyle.featureSettings;
    count = styleFeatures.Length();
    for (i = 0; i < count; i++) {
        const gfxFontFeature& feature = styleFeatures.ElementAt(i);
        if (feature.mTag == aFeature) {
            featureSet = true;
            aFeatureOn = (feature.mValue != 0);
        }
    }

    return featureSet;
}

/**
 * A helper function in case we need to do any rounding or other
 * processing here.
 */
#define ToDeviceUnits(aAppUnits, aDevUnitsPerAppUnit) \
    (double(aAppUnits)*double(aDevUnitsPerAppUnit))

struct GlyphBuffer {
#define GLYPH_BUFFER_SIZE (2048/sizeof(cairo_glyph_t))
    cairo_glyph_t mGlyphBuffer[GLYPH_BUFFER_SIZE];
    unsigned int mNumGlyphs;

    GlyphBuffer()
        : mNumGlyphs(0) { }

    cairo_glyph_t *AppendGlyph() {
        return &mGlyphBuffer[mNumGlyphs++];
    }

    void Flush(cairo_t *aCR, DrawMode aDrawMode, bool aReverse,
               gfxTextContextPaint *aContextPaint,
               const gfxMatrix& aGlobalMatrix, bool aFinish = false) {
        // Ensure there's enough room for a glyph to be added to the buffer
        // and we actually have glyphs to draw
        if ((!aFinish && mNumGlyphs < GLYPH_BUFFER_SIZE) || !mNumGlyphs) {
            return;
        }

        if (aReverse) {
            for (uint32_t i = 0; i < mNumGlyphs/2; ++i) {
                cairo_glyph_t tmp = mGlyphBuffer[i];
                mGlyphBuffer[i] = mGlyphBuffer[mNumGlyphs - 1 - i];
                mGlyphBuffer[mNumGlyphs - 1 - i] = tmp;
            }
        }

        if (aDrawMode == DrawMode::GLYPH_PATH) {
            cairo_glyph_path(aCR, mGlyphBuffer, mNumGlyphs);
        } else {
            if ((int(aDrawMode) & (int(DrawMode::GLYPH_STROKE) | int(DrawMode::GLYPH_STROKE_UNDERNEATH))) ==
                                  (int(DrawMode::GLYPH_STROKE) | int(DrawMode::GLYPH_STROKE_UNDERNEATH))) {
                FlushStroke(aCR, aContextPaint, aGlobalMatrix);
            }
            if (int(aDrawMode) & int(DrawMode::GLYPH_FILL)) {
                PROFILER_LABEL("GlyphBuffer", "Flush::cairo_show_glyphs",
                    js::ProfileEntry::Category::GRAPHICS);

                nsRefPtr<gfxPattern> pattern;
                if (aContextPaint &&
                    !!(pattern = aContextPaint->GetFillPattern(aGlobalMatrix))) {
                    cairo_save(aCR);
                    cairo_set_source(aCR, pattern->CairoPattern());
                }

                cairo_show_glyphs(aCR, mGlyphBuffer, mNumGlyphs);

                if (pattern) {
                    cairo_restore(aCR);
                }
            }
            if ((int(aDrawMode) & (int(DrawMode::GLYPH_STROKE) | int(DrawMode::GLYPH_STROKE_UNDERNEATH))) ==
                                  int(DrawMode::GLYPH_STROKE)) {
                FlushStroke(aCR, aContextPaint, aGlobalMatrix);
            }
        }

        mNumGlyphs = 0;
    }

private:
    void FlushStroke(cairo_t *aCR, gfxTextContextPaint *aContextPaint,
                     const gfxMatrix& aGlobalMatrix) {
        nsRefPtr<gfxPattern> pattern;
        if (aContextPaint &&
            !!(pattern = aContextPaint->GetStrokePattern(aGlobalMatrix))) {
            cairo_save(aCR);
            cairo_set_source(aCR, pattern->CairoPattern());
        }

        cairo_new_path(aCR);
        cairo_glyph_path(aCR, mGlyphBuffer, mNumGlyphs);
        cairo_stroke(aCR);

        if (pattern) {
            cairo_restore(aCR);
        }
    }

#undef GLYPH_BUFFER_SIZE
};

static AntialiasMode Get2DAAMode(gfxFont::AntialiasOption aAAOption) {
  switch (aAAOption) {
  case gfxFont::kAntialiasSubpixel:
    return AntialiasMode::SUBPIXEL;
  case gfxFont::kAntialiasGrayscale:
    return AntialiasMode::GRAY;
  case gfxFont::kAntialiasNone:
    return AntialiasMode::NONE;
  default:
    return AntialiasMode::DEFAULT;
  }
}

struct GlyphBufferAzure {
#define GLYPH_BUFFER_SIZE (2048/sizeof(Glyph))
    Glyph mGlyphBuffer[GLYPH_BUFFER_SIZE];
    unsigned int mNumGlyphs;

    GlyphBufferAzure()
        : mNumGlyphs(0) { }

    Glyph *AppendGlyph() {
        return &mGlyphBuffer[mNumGlyphs++];
    }

    void Flush(DrawTarget *aDT, gfxTextContextPaint *aContextPaint, ScaledFont *aFont,
               DrawMode aDrawMode, bool aReverse, const GlyphRenderingOptions *aOptions,
               gfxContext *aThebesContext, const Matrix *aInvFontMatrix, const DrawOptions &aDrawOptions,
               bool aFinish = false)
    {
        // Ensure there's enough room for a glyph to be added to the buffer
        if ((!aFinish && mNumGlyphs < GLYPH_BUFFER_SIZE) || !mNumGlyphs) {
            return;
        }

        if (aReverse) {
            Glyph *begin = &mGlyphBuffer[0];
            Glyph *end = &mGlyphBuffer[mNumGlyphs];
            std::reverse(begin, end);
        }
        
        gfx::GlyphBuffer buf;
        buf.mGlyphs = mGlyphBuffer;
        buf.mNumGlyphs = mNumGlyphs;

        gfxContext::AzureState state = aThebesContext->CurrentState();
        if ((int(aDrawMode) & (int(DrawMode::GLYPH_STROKE) | int(DrawMode::GLYPH_STROKE_UNDERNEATH))) ==
                              (int(DrawMode::GLYPH_STROKE) | int(DrawMode::GLYPH_STROKE_UNDERNEATH))) {
            FlushStroke(aDT, aContextPaint, aFont, aThebesContext, buf, state);
        }
        if (int(aDrawMode) & int(DrawMode::GLYPH_FILL)) {
            if (state.pattern || aContextPaint) {
                Pattern *pat;

                nsRefPtr<gfxPattern> fillPattern;
                if (!aContextPaint ||
                    !(fillPattern = aContextPaint->GetFillPattern(aThebesContext->CurrentMatrix()))) {
                    if (state.pattern) {
                        pat = state.pattern->GetPattern(aDT, state.patternTransformChanged ? &state.patternTransform : nullptr);
                    } else {
                        pat = nullptr;
                    }
                } else {
                    pat = fillPattern->GetPattern(aDT);
                }

                if (pat) {
                    Matrix saved;
                    Matrix *mat = nullptr;
                    if (aInvFontMatrix) {
                        // The brush matrix needs to be multiplied with the inverted matrix
                        // as well, to move the brush into the space of the glyphs. Before
                        // the render target transformation

                        // This relies on the returned Pattern not to be reused by
                        // others, but regenerated on GetPattern calls. This is true!
                        if (pat->GetType() == PatternType::LINEAR_GRADIENT) {
                            mat = &static_cast<LinearGradientPattern*>(pat)->mMatrix;
                        } else if (pat->GetType() == PatternType::RADIAL_GRADIENT) {
                            mat = &static_cast<RadialGradientPattern*>(pat)->mMatrix;
                        } else if (pat->GetType() == PatternType::SURFACE) {
                            mat = &static_cast<SurfacePattern*>(pat)->mMatrix;
                        }

                        if (mat) {
                            saved = *mat;
                            *mat = (*mat) * (*aInvFontMatrix);
                        }
                    }

                    aDT->FillGlyphs(aFont, buf, *pat,
                                    aDrawOptions, aOptions);

                    if (mat) {
                        *mat = saved;
                    }
                }
            } else if (state.sourceSurface) {
                aDT->FillGlyphs(aFont, buf, SurfacePattern(state.sourceSurface,
                                                           ExtendMode::CLAMP,
                                                           state.surfTransform),
                                aDrawOptions, aOptions);
            } else {
                aDT->FillGlyphs(aFont, buf, ColorPattern(state.color),
                                aDrawOptions, aOptions);
            }
        }
        if (int(aDrawMode) & int(DrawMode::GLYPH_PATH)) {
            aThebesContext->EnsurePathBuilder();
			Matrix mat = aDT->GetTransform();
            aFont->CopyGlyphsToBuilder(buf, aThebesContext->mPathBuilder, aDT->GetType(), &mat);
        }
        if ((int(aDrawMode) & (int(DrawMode::GLYPH_STROKE) | int(DrawMode::GLYPH_STROKE_UNDERNEATH))) ==
                              int(DrawMode::GLYPH_STROKE)) {
            FlushStroke(aDT, aContextPaint, aFont, aThebesContext, buf, state);
        }

        mNumGlyphs = 0;
    }

private:
    void FlushStroke(DrawTarget *aDT, gfxTextContextPaint *aContextPaint,
                     ScaledFont *aFont, gfxContext *aThebesContext,
                     gfx::GlyphBuffer& aBuf, gfxContext::AzureState& aState)
    {
        RefPtr<Path> path = aFont->GetPathForGlyphs(aBuf, aDT);
        if (aContextPaint) {
            nsRefPtr<gfxPattern> strokePattern =
              aContextPaint->GetStrokePattern(aThebesContext->CurrentMatrix());
            if (strokePattern) {
                aDT->Stroke(path, *strokePattern->GetPattern(aDT), aState.strokeOptions);
            }
        }
    }

#undef GLYPH_BUFFER_SIZE
};

// Bug 674909. When synthetic bolding text by drawing twice, need to
// render using a pixel offset in device pixels, otherwise text
// doesn't appear bolded, it appears as if a bad text shadow exists
// when a non-identity transform exists.  Use an offset factor so that
// the second draw occurs at a constant offset in device pixels.

double
gfxFont::CalcXScale(gfxContext *aContext)
{
    // determine magnitude of a 1px x offset in device space
    gfxSize t = aContext->UserToDevice(gfxSize(1.0, 0.0));
    if (t.width == 1.0 && t.height == 0.0) {
        // short-circuit the most common case to avoid sqrt() and division
        return 1.0;
    }

    double m = sqrt(t.width * t.width + t.height * t.height);

    NS_ASSERTION(m != 0.0, "degenerate transform while synthetic bolding");
    if (m == 0.0) {
        return 0.0; // effectively disables offset
    }

    // scale factor so that offsets are 1px in device pixels
    return 1.0 / m;
}

static DrawMode
ForcePaintingDrawMode(DrawMode aDrawMode)
{
    return aDrawMode == DrawMode::GLYPH_PATH ?
        DrawMode(int(DrawMode::GLYPH_FILL) | int(DrawMode::GLYPH_STROKE)) :
        aDrawMode;
}

void
gfxFont::Draw(gfxTextRun *aTextRun, uint32_t aStart, uint32_t aEnd,
              gfxContext *aContext, DrawMode aDrawMode, gfxPoint *aPt,
              Spacing *aSpacing, gfxTextContextPaint *aContextPaint,
              gfxTextRunDrawCallbacks *aCallbacks)
{
    NS_ASSERTION(aDrawMode == DrawMode::GLYPH_PATH || !(int(aDrawMode) & int(DrawMode::GLYPH_PATH)),
                 "GLYPH_PATH cannot be used with GLYPH_FILL, GLYPH_STROKE or GLYPH_STROKE_UNDERNEATH");

    if (aStart >= aEnd)
        return;

    const gfxTextRun::CompressedGlyph *charGlyphs = aTextRun->GetCharacterGlyphs();
    const int32_t appUnitsPerDevUnit = aTextRun->GetAppUnitsPerDevUnit();
    const double devUnitsPerAppUnit = 1.0/double(appUnitsPerDevUnit);
    bool isRTL = aTextRun->IsRightToLeft();
    double direction = aTextRun->GetDirection();
    gfxMatrix globalMatrix = aContext->CurrentMatrix();

    bool haveSVGGlyphs = GetFontEntry()->TryGetSVGData(this);
    bool haveColorGlyphs = GetFontEntry()->TryGetColorGlyphs();
    nsAutoPtr<gfxTextContextPaint> contextPaint;
    if (haveSVGGlyphs && !aContextPaint) {
        // If no pattern is specified for fill, use the current pattern
        NS_ASSERTION((int(aDrawMode) & int(DrawMode::GLYPH_STROKE)) == 0, "no pattern supplied for stroking text");
        nsRefPtr<gfxPattern> fillPattern = aContext->GetPattern();
        contextPaint = new SimpleTextContextPaint(fillPattern, nullptr,
                                                 aContext->CurrentMatrix());
        aContextPaint = contextPaint;
    }

    // synthetic-bold strikes are each offset one device pixel in run direction
    // (these values are only needed if IsSyntheticBold() is true)
    double synBoldOnePixelOffset = 0;
    int32_t strikes = 1;
    if (IsSyntheticBold()) {
        double xscale = CalcXScale(aContext);
        synBoldOnePixelOffset = direction * xscale;
        if (xscale != 0.0) {
            // use as many strikes as needed for the the increased advance
            strikes = NS_lroundf(GetSyntheticBoldOffset() / xscale);
        }
    }

    uint32_t i;
    // Current position in appunits
    double x = aPt->x;
    double y = aPt->y;

    cairo_t *cr = aContext->GetCairo();
    RefPtr<DrawTarget> dt = aContext->GetDrawTarget();

    bool paintSVGGlyphs = !aCallbacks || aCallbacks->mShouldPaintSVGGlyphs;
    bool emittedGlyphs = false;

    if (aContext->IsCairo()) {
      bool success = SetupCairoFont(aContext);
      if (MOZ_UNLIKELY(!success))
          return;

      ::GlyphBuffer glyphs;
      cairo_glyph_t *glyph;

      if (aSpacing) {
          x += direction*aSpacing[0].mBefore;
      }
      for (i = aStart; i < aEnd; ++i) {
          const gfxTextRun::CompressedGlyph *glyphData = &charGlyphs[i];
          if (glyphData->IsSimpleGlyph()) {
              double advance = glyphData->GetSimpleAdvance();
              double glyphX;
              if (isRTL) {
                  x -= advance;
                  glyphX = x;
              } else {
                  glyphX = x;
                  x += advance;
              }

              if (haveSVGGlyphs) {
                  if (!paintSVGGlyphs) {
                      continue;
                  }
                  gfxPoint point(ToDeviceUnits(glyphX, devUnitsPerAppUnit),
                                 ToDeviceUnits(y, devUnitsPerAppUnit));
                  DrawMode mode = ForcePaintingDrawMode(aDrawMode);
                  if (RenderSVGGlyph(aContext, point, mode,
                                     glyphData->GetSimpleGlyph(), aContextPaint,
                                     aCallbacks, emittedGlyphs)) {
                      continue;
                  }
              }

              if (haveColorGlyphs) {
                  gfxPoint point(ToDeviceUnits(glyphX, devUnitsPerAppUnit),
                                 ToDeviceUnits(y, devUnitsPerAppUnit));
                  if (RenderColorGlyph(aContext, point, glyphData->GetSimpleGlyph())) {
                      continue;
                  }
              }

              // Perhaps we should put a scale in the cairo context instead of
              // doing this scaling here...
              // Multiplying by the reciprocal may introduce tiny error here,
              // but we assume cairo is going to round coordinates at some stage
              // and this is faster
              glyph = glyphs.AppendGlyph();
              glyph->index = glyphData->GetSimpleGlyph();
              glyph->x = ToDeviceUnits(glyphX, devUnitsPerAppUnit);
              glyph->y = ToDeviceUnits(y, devUnitsPerAppUnit);
              glyphs.Flush(cr, aDrawMode, isRTL, aContextPaint, globalMatrix);
            
              // synthetic bolding by multi-striking with 1-pixel offsets
              // at least once, more if there's room (large font sizes)
              if (IsSyntheticBold()) {
                  double strikeOffset = synBoldOnePixelOffset;
                  int32_t strikeCount = strikes;
                  do {
                      cairo_glyph_t *doubleglyph;
                      doubleglyph = glyphs.AppendGlyph();
                      doubleglyph->index = glyph->index;
                      doubleglyph->x =
                          ToDeviceUnits(glyphX + strikeOffset * appUnitsPerDevUnit,
                                        devUnitsPerAppUnit);
                      doubleglyph->y = glyph->y;
                      strikeOffset += synBoldOnePixelOffset;
                      glyphs.Flush(cr, aDrawMode, isRTL, aContextPaint, globalMatrix);
                  } while (--strikeCount > 0);
              }
              emittedGlyphs = true;
          } else {
              uint32_t glyphCount = glyphData->GetGlyphCount();
              if (glyphCount > 0) {
                  const gfxTextRun::DetailedGlyph *details =
                      aTextRun->GetDetailedGlyphs(i);
                  NS_ASSERTION(details, "detailedGlyph should not be missing!");
                  double advance;
                  for (uint32_t j = 0; j < glyphCount; ++j, ++details, x += direction * advance) {
                      advance = details->mAdvance;
                      if (glyphData->IsMissing()) {
                          // default ignorable characters will have zero advance width.
                          // we don't have to draw the hexbox for them
                          if (aDrawMode != DrawMode::GLYPH_PATH && advance > 0) {
                              double glyphX = x;
                              if (isRTL) {
                                  glyphX -= advance;
                              }
                              gfxPoint pt(ToDeviceUnits(glyphX, devUnitsPerAppUnit),
                                          ToDeviceUnits(y, devUnitsPerAppUnit));
                              gfxFloat advanceDevUnits = ToDeviceUnits(advance, devUnitsPerAppUnit);
                              gfxFloat height = GetMetrics().maxAscent;
                              gfxRect glyphRect(pt.x, pt.y - height, advanceDevUnits, height);
                              gfxFontMissingGlyphs::DrawMissingGlyph(aContext,
                                                                     glyphRect,
                                                                     details->mGlyphID,
                                                                     appUnitsPerDevUnit);
                          }
                      } else {
                          double glyphX = x + details->mXOffset;
                          if (isRTL) {
                              glyphX -= advance;
                          }

                          if (haveSVGGlyphs) {
                              if (!paintSVGGlyphs) {
                                  continue;
                              }

                              gfxPoint point(ToDeviceUnits(glyphX, devUnitsPerAppUnit),
                                             ToDeviceUnits(y, devUnitsPerAppUnit));

                              DrawMode mode = ForcePaintingDrawMode(aDrawMode);
                              if (RenderSVGGlyph(aContext, point, mode,
                                                  details->mGlyphID,
                                                  aContextPaint, aCallbacks,
                                                  emittedGlyphs)) {
                                  continue;
                              }
                          }

                          if (haveColorGlyphs) {
                              gfxPoint point(ToDeviceUnits(glyphX,
                                                           devUnitsPerAppUnit),
                                             ToDeviceUnits(y + details->mYOffset,
                                                           devUnitsPerAppUnit));
                              if (RenderColorGlyph(aContext, point,
                                                   details->mGlyphID)) {
                                  continue;
                              }
                          }

                          glyph = glyphs.AppendGlyph();
                          glyph->index = details->mGlyphID;
                          glyph->x = ToDeviceUnits(glyphX, devUnitsPerAppUnit);
                          glyph->y = ToDeviceUnits(y + details->mYOffset, devUnitsPerAppUnit);
                          glyphs.Flush(cr, aDrawMode, isRTL, aContextPaint, globalMatrix);

                          if (IsSyntheticBold()) {
                              double strikeOffset = synBoldOnePixelOffset;
                              int32_t strikeCount = strikes;
                              do {
                                  cairo_glyph_t *doubleglyph;
                                  doubleglyph = glyphs.AppendGlyph();
                                  doubleglyph->index = glyph->index;
                                  doubleglyph->x =
                                      ToDeviceUnits(glyphX + strikeOffset *
                                              appUnitsPerDevUnit,
                                              devUnitsPerAppUnit);
                                  doubleglyph->y = glyph->y;
                                  strikeOffset += synBoldOnePixelOffset;
                                  glyphs.Flush(cr, aDrawMode, isRTL, aContextPaint, globalMatrix);
                              } while (--strikeCount > 0);
                          }
                          emittedGlyphs = true;
                      }
                  }
              }
          }

          if (aSpacing) {
              double space = aSpacing[i - aStart].mAfter;
              if (i + 1 < aEnd) {
                  space += aSpacing[i + 1 - aStart].mBefore;
              }
              x += direction*space;
          }
      }

      if (gfxFontTestStore::CurrentStore()) {
          /* This assumes that the tests won't have anything that results
           * in more than GLYPH_BUFFER_SIZE glyphs.  Do this before we
           * flush, since that'll blow away the num_glyphs.
           */
          gfxFontTestStore::CurrentStore()->AddItem(GetName(),
                                                    glyphs.mGlyphBuffer,
                                                    glyphs.mNumGlyphs);
      }

      // draw any remaining glyphs
      glyphs.Flush(cr, aDrawMode, isRTL, aContextPaint, globalMatrix, true);
      if (aCallbacks && emittedGlyphs) {
          aCallbacks->NotifyGlyphPathEmitted();
      }

    } else {
      RefPtr<ScaledFont> scaledFont = GetScaledFont(dt);

      if (!scaledFont) {
        return;
      }

      bool oldSubpixelAA = dt->GetPermitSubpixelAA();

      if (!AllowSubpixelAA()) {
          dt->SetPermitSubpixelAA(false);
      }

      GlyphBufferAzure glyphs;
      Glyph *glyph;

      Matrix mat, matInv;
      Matrix oldMat = dt->GetTransform();

      // This is nullptr when we have inverse-transformed glyphs and we need
      // to transform the Brush inside flush.
      Matrix *passedInvMatrix = nullptr;

      RefPtr<GlyphRenderingOptions> renderingOptions =
        GetGlyphRenderingOptions();

      DrawOptions drawOptions;
      drawOptions.mAntialiasMode = Get2DAAMode(mAntialiasOption);

      // The cairo DrawTarget backend uses the cairo_scaled_font directly
      // and so has the font skew matrix applied already.
      if (mScaledFont &&
          dt->GetType() != BackendType::CAIRO) {
        cairo_matrix_t matrix;
        cairo_scaled_font_get_font_matrix(mScaledFont, &matrix);
        if (matrix.xy != 0) {
          // If this matrix applies a skew, which can happen when drawing
          // oblique fonts, we will set the DrawTarget matrix to apply the
          // skew. We'll need to move the glyphs by the inverse of the skew to
          // get the glyphs positioned correctly in the new device space
          // though, since the font matrix should only be applied to drawing
          // the glyphs, and not to their position.
          mat = ToMatrix(*reinterpret_cast<gfxMatrix*>(&matrix));

          mat._11 = mat._22 = 1.0;
          float adjustedSize = mAdjustedSize > 0 ? mAdjustedSize : GetStyle()->size;
          mat._21 /= adjustedSize;

          dt->SetTransform(mat * oldMat);

          matInv = mat;
          matInv.Invert();

          passedInvMatrix = &matInv;
        }
      }

      if (aSpacing) {
          x += direction*aSpacing[0].mBefore;
      }
      for (i = aStart; i < aEnd; ++i) {
          const gfxTextRun::CompressedGlyph *glyphData = &charGlyphs[i];
          if (glyphData->IsSimpleGlyph()) {
              double advance = glyphData->GetSimpleAdvance();
              double glyphX;
              if (isRTL) {
                  x -= advance;
                  glyphX = x;
              } else {
                  glyphX = x;
                  x += advance;
              }

              if (haveSVGGlyphs) {
                  if (!paintSVGGlyphs) {
                      continue;
                  }
                  gfxPoint point(ToDeviceUnits(glyphX, devUnitsPerAppUnit),
                                 ToDeviceUnits(y, devUnitsPerAppUnit));
                  DrawMode mode = ForcePaintingDrawMode(aDrawMode);
                  if (RenderSVGGlyph(aContext, point, mode,
                                     glyphData->GetSimpleGlyph(), aContextPaint,
                                     aCallbacks, emittedGlyphs)) {
                      continue;
                  }
              }

              if (haveColorGlyphs) {
                  mozilla::gfx::Point point(ToDeviceUnits(glyphX,
                                                          devUnitsPerAppUnit),
                                            ToDeviceUnits(y,
                                                          devUnitsPerAppUnit));
                  if (RenderColorGlyph(aContext, scaledFont, renderingOptions,
                                       drawOptions, matInv * point,
                                       glyphData->GetSimpleGlyph())) {
                      continue;
                  }
              }

              // Perhaps we should put a scale in the cairo context instead of
              // doing this scaling here...
              // Multiplying by the reciprocal may introduce tiny error here,
              // but we assume cairo is going to round coordinates at some stage
              // and this is faster
              glyph = glyphs.AppendGlyph();
              glyph->mIndex = glyphData->GetSimpleGlyph();
              glyph->mPosition.x = ToDeviceUnits(glyphX, devUnitsPerAppUnit);
              glyph->mPosition.y = ToDeviceUnits(y, devUnitsPerAppUnit);
              glyph->mPosition = matInv * glyph->mPosition;
              glyphs.Flush(dt, aContextPaint, scaledFont,
                           aDrawMode, isRTL, renderingOptions,
                           aContext, passedInvMatrix,
                           drawOptions);

              // synthetic bolding by multi-striking with 1-pixel offsets
              // at least once, more if there's room (large font sizes)
              if (IsSyntheticBold()) {
                  double strikeOffset = synBoldOnePixelOffset;
                  int32_t strikeCount = strikes;
                  do {
                      Glyph *doubleglyph;
                      doubleglyph = glyphs.AppendGlyph();
                      doubleglyph->mIndex = glyph->mIndex;
                      doubleglyph->mPosition.x =
                          ToDeviceUnits(glyphX + strikeOffset * appUnitsPerDevUnit,
                                        devUnitsPerAppUnit);
                      doubleglyph->mPosition.y = glyph->mPosition.y;
                      doubleglyph->mPosition = matInv * doubleglyph->mPosition;
                      strikeOffset += synBoldOnePixelOffset;
                      glyphs.Flush(dt, aContextPaint, scaledFont,
                                   aDrawMode, isRTL, renderingOptions,
                                   aContext, passedInvMatrix,
                                   drawOptions);
                  } while (--strikeCount > 0);
              }
              emittedGlyphs = true;
          } else {
              uint32_t glyphCount = glyphData->GetGlyphCount();
              if (glyphCount > 0) {
                  const gfxTextRun::DetailedGlyph *details =
                      aTextRun->GetDetailedGlyphs(i);
                  NS_ASSERTION(details, "detailedGlyph should not be missing!");
                  double advance;
                  for (uint32_t j = 0; j < glyphCount; ++j, ++details, x += direction * advance) {
                      advance = details->mAdvance;
                      if (glyphData->IsMissing()) {
                          // default ignorable characters will have zero advance width.
                          // we don't have to draw the hexbox for them
                          if (aDrawMode != DrawMode::GLYPH_PATH && advance > 0) {
                              double glyphX = x;
                              if (isRTL) {
                                  glyphX -= advance;
                              }
                              gfxPoint pt(ToDeviceUnits(glyphX, devUnitsPerAppUnit),
                                          ToDeviceUnits(y, devUnitsPerAppUnit));
                              gfxFloat advanceDevUnits = ToDeviceUnits(advance, devUnitsPerAppUnit);
                              gfxFloat height = GetMetrics().maxAscent;
                              gfxRect glyphRect(pt.x, pt.y - height, advanceDevUnits, height);
                              gfxFontMissingGlyphs::DrawMissingGlyph(aContext,
                                                                     glyphRect,
                                                                     details->mGlyphID,
                                                                     appUnitsPerDevUnit);
                          }
                      } else {
                          double glyphX = x + details->mXOffset;
                          if (isRTL) {
                              glyphX -= advance;
                          }

                          gfxPoint point(ToDeviceUnits(glyphX, devUnitsPerAppUnit),
                                         ToDeviceUnits(y, devUnitsPerAppUnit));

                          if (haveSVGGlyphs) {
                              if (!paintSVGGlyphs) {
                                  continue;
                              }
                              DrawMode mode = ForcePaintingDrawMode(aDrawMode);
                              if (RenderSVGGlyph(aContext, point, mode,
                                                 details->mGlyphID,
                                                 aContextPaint, aCallbacks,
                                                 emittedGlyphs)) {
                                  continue;
                              }
                          }

                          if (haveColorGlyphs) {
                              mozilla::gfx::Point point(ToDeviceUnits(glyphX,
                                                                      devUnitsPerAppUnit),
                                                        ToDeviceUnits(y + details->mYOffset,
                                                                      devUnitsPerAppUnit));
                              if (RenderColorGlyph(aContext, scaledFont,
                                                   renderingOptions,
                                                   drawOptions, matInv * point,
                                                   details->mGlyphID)) {
                                  continue;
                              }
                          }

                          glyph = glyphs.AppendGlyph();
                          glyph->mIndex = details->mGlyphID;
                          glyph->mPosition.x = ToDeviceUnits(glyphX, devUnitsPerAppUnit);
                          glyph->mPosition.y = ToDeviceUnits(y + details->mYOffset, devUnitsPerAppUnit);
                          glyph->mPosition = matInv * glyph->mPosition;
                          glyphs.Flush(dt, aContextPaint, scaledFont, aDrawMode,
                                       isRTL, renderingOptions, aContext, passedInvMatrix,
                                       drawOptions);

                          if (IsSyntheticBold()) {
                              double strikeOffset = synBoldOnePixelOffset;
                              int32_t strikeCount = strikes;
                              do {
                                  Glyph *doubleglyph;
                                  doubleglyph = glyphs.AppendGlyph();
                                  doubleglyph->mIndex = glyph->mIndex;
                                  doubleglyph->mPosition.x =
                                      ToDeviceUnits(glyphX + strikeOffset *
                                                    appUnitsPerDevUnit,
                                                    devUnitsPerAppUnit);
                                  doubleglyph->mPosition.y = glyph->mPosition.y;
                                  strikeOffset += synBoldOnePixelOffset;
                                  doubleglyph->mPosition = matInv * doubleglyph->mPosition;
                                  glyphs.Flush(dt, aContextPaint, scaledFont,
                                               aDrawMode, isRTL, renderingOptions,
                                               aContext, passedInvMatrix, drawOptions);
                              } while (--strikeCount > 0);
                          }
                          emittedGlyphs = true;
                      }
                  }
              }
          }

          if (aSpacing) {
              double space = aSpacing[i - aStart].mAfter;
              if (i + 1 < aEnd) {
                  space += aSpacing[i + 1 - aStart].mBefore;
              }
              x += direction*space;
          }
      }

      glyphs.Flush(dt, aContextPaint, scaledFont, aDrawMode, isRTL,
                   renderingOptions, aContext, passedInvMatrix,
                   drawOptions, true);
      if (aCallbacks && emittedGlyphs) {
          aCallbacks->NotifyGlyphPathEmitted();
      }

      dt->SetTransform(oldMat);

      dt->SetPermitSubpixelAA(oldSubpixelAA);
    }

    *aPt = gfxPoint(x, y);
}

bool
gfxFont::RenderSVGGlyph(gfxContext *aContext, gfxPoint aPoint, DrawMode aDrawMode,
                        uint32_t aGlyphId, gfxTextContextPaint *aContextPaint)
{
    if (!GetFontEntry()->HasSVGGlyph(aGlyphId)) {
        return false;
    }

    const gfxFloat devUnitsPerSVGUnit =
        GetAdjustedSize() / GetFontEntry()->UnitsPerEm();
    gfxContextMatrixAutoSaveRestore matrixRestore(aContext);

    aContext->Translate(gfxPoint(aPoint.x, aPoint.y));
    aContext->Scale(devUnitsPerSVGUnit, devUnitsPerSVGUnit);

    aContextPaint->InitStrokeGeometry(aContext, devUnitsPerSVGUnit);

    return GetFontEntry()->RenderSVGGlyph(aContext, aGlyphId, int(aDrawMode),
                                          aContextPaint);
}

bool
gfxFont::RenderSVGGlyph(gfxContext *aContext, gfxPoint aPoint, DrawMode aDrawMode,
                        uint32_t aGlyphId, gfxTextContextPaint *aContextPaint,
                        gfxTextRunDrawCallbacks *aCallbacks,
                        bool& aEmittedGlyphs)
{
    if (aCallbacks) {
        if (aEmittedGlyphs) {
            aCallbacks->NotifyGlyphPathEmitted();
            aEmittedGlyphs = false;
        }
        aCallbacks->NotifyBeforeSVGGlyphPainted();
    }
    bool rendered = RenderSVGGlyph(aContext, aPoint, aDrawMode, aGlyphId,
                                   aContextPaint);
    if (aCallbacks) {
        aCallbacks->NotifyAfterSVGGlyphPainted();
    }
    return rendered;
}

bool
gfxFont::RenderColorGlyph(gfxContext* aContext, gfxPoint& point,
                          uint32_t aGlyphId)
{
    nsAutoTArray<uint16_t, 8> layerGlyphs;
    nsAutoTArray<mozilla::gfx::Color, 8> layerColors;

    if (!GetFontEntry()->GetColorLayersInfo(aGlyphId, layerGlyphs, layerColors)) {
        return false;
    }

    cairo_t* cr = aContext->GetCairo();
    cairo_save(cr);
    for (uint32_t layerIndex = 0; layerIndex < layerGlyphs.Length();
         layerIndex++) {

        cairo_glyph_t glyph;
        glyph.index = layerGlyphs[layerIndex];
        glyph.x = point.x;
        glyph.y = point.y;

        mozilla::gfx::Color &color = layerColors[layerIndex];
        cairo_pattern_t* pattern =
            cairo_pattern_create_rgba(color.r, color.g, color.b, color.a);

        cairo_set_source(cr, pattern);
        cairo_show_glyphs(cr, &glyph, 1);
        cairo_pattern_destroy(pattern);
    }
    cairo_restore(cr);

    return true;
}

bool
gfxFont::RenderColorGlyph(gfxContext* aContext,
                          mozilla::gfx::ScaledFont* scaledFont,
                          GlyphRenderingOptions* aRenderingOptions,
                          mozilla::gfx::DrawOptions aDrawOptions,
                          const mozilla::gfx::Point& aPoint,
                          uint32_t aGlyphId)
{
    nsAutoTArray<uint16_t, 8> layerGlyphs;
    nsAutoTArray<mozilla::gfx::Color, 8> layerColors;

    if (!GetFontEntry()->GetColorLayersInfo(aGlyphId, layerGlyphs, layerColors)) {
        return false;
    }

    RefPtr<DrawTarget> dt = aContext->GetDrawTarget();
    for (uint32_t layerIndex = 0; layerIndex < layerGlyphs.Length();
         layerIndex++) {
        Glyph glyph;
        glyph.mIndex = layerGlyphs[layerIndex];
        glyph.mPosition = aPoint;

        mozilla::gfx::GlyphBuffer buffer;
        buffer.mGlyphs = &glyph;
        buffer.mNumGlyphs = 1;

        dt->FillGlyphs(scaledFont, buffer,
                       ColorPattern(layerColors[layerIndex]),
                       aDrawOptions, aRenderingOptions);
    }
    return true;
}

static void
UnionRange(gfxFloat aX, gfxFloat* aDestMin, gfxFloat* aDestMax)
{
    *aDestMin = std::min(*aDestMin, aX);
    *aDestMax = std::max(*aDestMax, aX);
}

// We get precise glyph extents if the textrun creator requested them, or
// if the font is a user font --- in which case the author may be relying
// on overflowing glyphs.
static bool
NeedsGlyphExtents(gfxFont *aFont, gfxTextRun *aTextRun)
{
    return (aTextRun->GetFlags() & gfxTextRunFactory::TEXT_NEED_BOUNDING_BOX) ||
        aFont->GetFontEntry()->IsUserFont();
}

static bool
NeedsGlyphExtents(gfxTextRun *aTextRun)
{
    if (aTextRun->GetFlags() & gfxTextRunFactory::TEXT_NEED_BOUNDING_BOX)
        return true;
    uint32_t numRuns;
    const gfxTextRun::GlyphRun *glyphRuns = aTextRun->GetGlyphRuns(&numRuns);
    for (uint32_t i = 0; i < numRuns; ++i) {
        if (glyphRuns[i].mFont->GetFontEntry()->IsUserFont())
            return true;
    }
    return false;
}

gfxFont::RunMetrics
gfxFont::Measure(gfxTextRun *aTextRun,
                 uint32_t aStart, uint32_t aEnd,
                 BoundingBoxType aBoundingBoxType,
                 gfxContext *aRefContext,
                 Spacing *aSpacing)
{
    // If aBoundingBoxType is TIGHT_HINTED_OUTLINE_EXTENTS
    // and the underlying cairo font may be antialiased,
    // we need to create a copy in order to avoid getting cached extents.
    // This is only used by MathML layout at present.
    if (aBoundingBoxType == TIGHT_HINTED_OUTLINE_EXTENTS &&
        mAntialiasOption != kAntialiasNone) {
        if (!mNonAAFont) {
            mNonAAFont = CopyWithAntialiasOption(kAntialiasNone);
        }
        // if font subclass doesn't implement CopyWithAntialiasOption(),
        // it will return null and we'll proceed to use the existing font
        if (mNonAAFont) {
            return mNonAAFont->Measure(aTextRun, aStart, aEnd,
                                       TIGHT_HINTED_OUTLINE_EXTENTS,
                                       aRefContext, aSpacing);
        }
    }

    const int32_t appUnitsPerDevUnit = aTextRun->GetAppUnitsPerDevUnit();
    // Current position in appunits
    const gfxFont::Metrics& fontMetrics = GetMetrics();

    RunMetrics metrics;
    metrics.mAscent = fontMetrics.maxAscent*appUnitsPerDevUnit;
    metrics.mDescent = fontMetrics.maxDescent*appUnitsPerDevUnit;
    if (aStart == aEnd) {
        // exit now before we look at aSpacing[0], which is undefined
        metrics.mBoundingBox = gfxRect(0, -metrics.mAscent, 0, metrics.mAscent + metrics.mDescent);
        return metrics;
    }

    gfxFloat advanceMin = 0, advanceMax = 0;
    const gfxTextRun::CompressedGlyph *charGlyphs = aTextRun->GetCharacterGlyphs();
    bool isRTL = aTextRun->IsRightToLeft();
    double direction = aTextRun->GetDirection();
    bool needsGlyphExtents = NeedsGlyphExtents(this, aTextRun);
    gfxGlyphExtents *extents =
        (aBoundingBoxType == LOOSE_INK_EXTENTS &&
            !needsGlyphExtents &&
            !aTextRun->HasDetailedGlyphs()) ? nullptr
        : GetOrCreateGlyphExtents(aTextRun->GetAppUnitsPerDevUnit());
    double x = 0;
    if (aSpacing) {
        x += direction*aSpacing[0].mBefore;
    }
    uint32_t i;
    for (i = aStart; i < aEnd; ++i) {
        const gfxTextRun::CompressedGlyph *glyphData = &charGlyphs[i];
        if (glyphData->IsSimpleGlyph()) {
            double advance = glyphData->GetSimpleAdvance();
            // Only get the real glyph horizontal extent if we were asked
            // for the tight bounding box or we're in quality mode
            if ((aBoundingBoxType != LOOSE_INK_EXTENTS || needsGlyphExtents) &&
                extents) {
                uint32_t glyphIndex = glyphData->GetSimpleGlyph();
                uint16_t extentsWidth = extents->GetContainedGlyphWidthAppUnits(glyphIndex);
                if (extentsWidth != gfxGlyphExtents::INVALID_WIDTH &&
                    aBoundingBoxType == LOOSE_INK_EXTENTS) {
                    UnionRange(x, &advanceMin, &advanceMax);
                    UnionRange(x + direction*extentsWidth, &advanceMin, &advanceMax);
                } else {
                    gfxRect glyphRect;
                    if (!extents->GetTightGlyphExtentsAppUnits(this,
                            aRefContext, glyphIndex, &glyphRect)) {
                        glyphRect = gfxRect(0, metrics.mBoundingBox.Y(),
                            advance, metrics.mBoundingBox.Height());
                    }
                    if (isRTL) {
                        glyphRect -= gfxPoint(advance, 0);
                    }
                    glyphRect += gfxPoint(x, 0);
                    metrics.mBoundingBox = metrics.mBoundingBox.Union(glyphRect);
                }
            }
            x += direction*advance;
        } else {
            uint32_t glyphCount = glyphData->GetGlyphCount();
            if (glyphCount > 0) {
                const gfxTextRun::DetailedGlyph *details =
                    aTextRun->GetDetailedGlyphs(i);
                NS_ASSERTION(details != nullptr,
                             "detaiedGlyph record should not be missing!");
                uint32_t j;
                for (j = 0; j < glyphCount; ++j, ++details) {
                    uint32_t glyphIndex = details->mGlyphID;
                    gfxPoint glyphPt(x + details->mXOffset, details->mYOffset);
                    double advance = details->mAdvance;
                    gfxRect glyphRect;
                    if (glyphData->IsMissing() || !extents ||
                        !extents->GetTightGlyphExtentsAppUnits(this,
                                aRefContext, glyphIndex, &glyphRect)) {
                        // We might have failed to get glyph extents due to
                        // OOM or something
                        glyphRect = gfxRect(0, -metrics.mAscent,
                            advance, metrics.mAscent + metrics.mDescent);
                    }
                    if (isRTL) {
                        glyphRect -= gfxPoint(advance, 0);
                    }
                    glyphRect += gfxPoint(x, 0);
                    metrics.mBoundingBox = metrics.mBoundingBox.Union(glyphRect);
                    x += direction*advance;
                }
            }
        }
        // Every other glyph type is ignored
        if (aSpacing) {
            double space = aSpacing[i - aStart].mAfter;
            if (i + 1 < aEnd) {
                space += aSpacing[i + 1 - aStart].mBefore;
            }
            x += direction*space;
        }
    }

    if (aBoundingBoxType == LOOSE_INK_EXTENTS) {
        UnionRange(x, &advanceMin, &advanceMax);
        gfxRect fontBox(advanceMin, -metrics.mAscent,
                        advanceMax - advanceMin, metrics.mAscent + metrics.mDescent);
        metrics.mBoundingBox = metrics.mBoundingBox.Union(fontBox);
    }
    if (isRTL) {
        metrics.mBoundingBox -= gfxPoint(x, 0);
    }

    metrics.mAdvanceWidth = x*direction;
    return metrics;
}

static PLDHashOperator
NotifyGlyphChangeObservers(nsPtrHashKey<gfxFont::GlyphChangeObserver>* aKey,
                           void* aClosure)
{
    aKey->GetKey()->NotifyGlyphsChanged();
    return PL_DHASH_NEXT;
}

void
gfxFont::NotifyGlyphsChanged()
{
    uint32_t i, count = mGlyphExtentsArray.Length();
    for (i = 0; i < count; ++i) {
        // Flush cached extents array
        mGlyphExtentsArray[i]->NotifyGlyphsChanged();
    }

    if (mGlyphChangeObservers) {
        mGlyphChangeObservers->EnumerateEntries(NotifyGlyphChangeObservers, nullptr);
    }
}

static bool
IsBoundarySpace(char16_t aChar, char16_t aNextChar)
{
    return (aChar == ' ' || aChar == 0x00A0) && !IsClusterExtender(aNextChar);
}

static inline uint32_t
HashMix(uint32_t aHash, char16_t aCh)
{
    return (aHash >> 28) ^ (aHash << 4) ^ aCh;
}

#ifdef __GNUC__
#define GFX_MAYBE_UNUSED __attribute__((unused))
#else
#define GFX_MAYBE_UNUSED
#endif

template<typename T>
gfxShapedWord*
gfxFont::GetShapedWord(gfxContext *aContext,
                       const T    *aText,
                       uint32_t    aLength,
                       uint32_t    aHash,
                       int32_t     aRunScript,
                       int32_t     aAppUnitsPerDevUnit,
                       uint32_t    aFlags,
                       gfxTextPerfMetrics *aTextPerf GFX_MAYBE_UNUSED)
{
    // if the cache is getting too big, flush it and start over
    uint32_t wordCacheMaxEntries =
        gfxPlatform::GetPlatform()->WordCacheMaxEntries();
    if (mWordCache->Count() > wordCacheMaxEntries) {
        NS_WARNING("flushing shaped-word cache");
        ClearCachedWords();
    }

    // if there's a cached entry for this word, just return it
    CacheHashKey key(aText, aLength, aHash,
                     aRunScript,
                     aAppUnitsPerDevUnit,
                     aFlags);

    CacheHashEntry *entry = mWordCache->PutEntry(key);
    if (!entry) {
        NS_WARNING("failed to create word cache entry - expect missing text");
        return nullptr;
    }
    gfxShapedWord *sw = entry->mShapedWord;

    bool isContent = !mStyle.systemFont;

    if (sw) {
        sw->ResetAge();
        Telemetry::Accumulate((isContent ? Telemetry::WORD_CACHE_HITS_CONTENT :
                                   Telemetry::WORD_CACHE_HITS_CHROME),
                              aLength);
#ifndef RELEASE_BUILD
        if (aTextPerf) {
            aTextPerf->current.wordCacheHit++;
        }
#endif
        return sw;
    }

    Telemetry::Accumulate((isContent ? Telemetry::WORD_CACHE_MISSES_CONTENT :
                               Telemetry::WORD_CACHE_MISSES_CHROME),
                          aLength);
#ifndef RELEASE_BUILD
    if (aTextPerf) {
        aTextPerf->current.wordCacheMiss++;
    }
#endif

    sw = entry->mShapedWord = gfxShapedWord::Create(aText, aLength,
                                                    aRunScript,
                                                    aAppUnitsPerDevUnit,
                                                    aFlags);
    if (!sw) {
        NS_WARNING("failed to create gfxShapedWord - expect missing text");
        return nullptr;
    }

    DebugOnly<bool> ok =
        ShapeText(aContext, aText, 0, aLength, aRunScript, sw);

    NS_WARN_IF_FALSE(ok, "failed to shape word - expect garbled text");

    return sw;
}

bool
gfxFont::CacheHashEntry::KeyEquals(const KeyTypePointer aKey) const
{
    const gfxShapedWord *sw = mShapedWord;
    if (!sw) {
        return false;
    }
    if (sw->GetLength() != aKey->mLength ||
        sw->Flags() != aKey->mFlags ||
        sw->GetAppUnitsPerDevUnit() != aKey->mAppUnitsPerDevUnit ||
        sw->Script() != aKey->mScript) {
        return false;
    }
    if (sw->TextIs8Bit()) {
        if (aKey->mTextIs8Bit) {
            return (0 == memcmp(sw->Text8Bit(), aKey->mText.mSingle,
                                aKey->mLength * sizeof(uint8_t)));
        }
        // The key has 16-bit text, even though all the characters are < 256,
        // so the TEXT_IS_8BIT flag was set and the cached ShapedWord we're
        // comparing with will have 8-bit text.
        const uint8_t   *s1 = sw->Text8Bit();
        const char16_t *s2 = aKey->mText.mDouble;
        const char16_t *s2end = s2 + aKey->mLength;
        while (s2 < s2end) {
            if (*s1++ != *s2++) {
                return false;
            }
        }
        return true;
    }
    NS_ASSERTION((aKey->mFlags & gfxTextRunFactory::TEXT_IS_8BIT) == 0 &&
                 !aKey->mTextIs8Bit, "didn't expect 8-bit text here");
    return (0 == memcmp(sw->TextUnicode(), aKey->mText.mDouble,
                        aKey->mLength * sizeof(char16_t)));
}

bool
gfxFont::ShapeText(gfxContext    *aContext,
                   const uint8_t *aText,
                   uint32_t       aOffset,
                   uint32_t       aLength,
                   int32_t        aScript,
                   gfxShapedText *aShapedText,
                   bool           aPreferPlatformShaping)
{
    nsDependentCSubstring ascii((const char*)aText, aLength);
    nsAutoString utf16;
    AppendASCIItoUTF16(ascii, utf16);
    if (utf16.Length() != aLength) {
        return false;
    }
    return ShapeText(aContext, utf16.BeginReading(), aOffset, aLength,
                     aScript, aShapedText, aPreferPlatformShaping);
}

bool
gfxFont::ShapeText(gfxContext      *aContext,
                   const char16_t *aText,
                   uint32_t         aOffset,
                   uint32_t         aLength,
                   int32_t          aScript,
                   gfxShapedText   *aShapedText,
                   bool             aPreferPlatformShaping)
{
    bool ok = false;

    if (mGraphiteShaper && gfxPlatform::GetPlatform()->UseGraphiteShaping()) {
        ok = mGraphiteShaper->ShapeText(aContext, aText, aOffset, aLength,
                                        aScript, aShapedText);
    }

    if (!ok && mHarfBuzzShaper && !aPreferPlatformShaping) {
        if (gfxPlatform::GetPlatform()->UseHarfBuzzForScript(aScript)) {
            ok = mHarfBuzzShaper->ShapeText(aContext, aText, aOffset, aLength,
                                            aScript, aShapedText);
        }
    }

    if (!ok) {
        if (!mPlatformShaper) {
            CreatePlatformShaper();
            NS_ASSERTION(mPlatformShaper, "no platform shaper available!");
        }
        if (mPlatformShaper) {
            ok = mPlatformShaper->ShapeText(aContext, aText, aOffset, aLength,
                                            aScript, aShapedText);
        }
    }

    PostShapingFixup(aContext, aText, aOffset, aLength, aShapedText);

    return ok;
}

void
gfxFont::PostShapingFixup(gfxContext      *aContext,
                          const char16_t *aText,
                          uint32_t         aOffset,
                          uint32_t         aLength,
                          gfxShapedText   *aShapedText)
{
    if (IsSyntheticBold()) {
        float synBoldOffset =
                GetSyntheticBoldOffset() * CalcXScale(aContext);
        aShapedText->AdjustAdvancesForSyntheticBold(synBoldOffset,
                                                    aOffset, aLength);
    }
}

#define MAX_SHAPING_LENGTH  32760 // slightly less than 32K, trying to avoid
                                  // over-stressing platform shapers
#define BACKTRACK_LIMIT     16 // backtrack this far looking for a good place
                               // to split into fragments for separate shaping

template<typename T>
bool
gfxFont::ShapeFragmentWithoutWordCache(gfxContext *aContext,
                                       const T    *aText,
                                       uint32_t    aOffset,
                                       uint32_t    aLength,
                                       int32_t     aScript,
                                       gfxTextRun *aTextRun)
{
    aTextRun->SetupClusterBoundaries(aOffset, aText, aLength);

    bool ok = true;

    while (ok && aLength > 0) {
        uint32_t fragLen = aLength;

        // limit the length of text we pass to shapers in a single call
        if (fragLen > MAX_SHAPING_LENGTH) {
            fragLen = MAX_SHAPING_LENGTH;

            // in the 8-bit case, there are no multi-char clusters,
            // so we don't need to do this check
            if (sizeof(T) == sizeof(char16_t)) {
                uint32_t i;
                for (i = 0; i < BACKTRACK_LIMIT; ++i) {
                    if (aTextRun->IsClusterStart(aOffset + fragLen - i)) {
                        fragLen -= i;
                        break;
                    }
                }
                if (i == BACKTRACK_LIMIT) {
                    // if we didn't find any cluster start while backtracking,
                    // just check that we're not in the middle of a surrogate
                    // pair; back up by one code unit if we are.
                    if (NS_IS_LOW_SURROGATE(aText[fragLen]) &&
                        NS_IS_HIGH_SURROGATE(aText[fragLen - 1])) {
                        --fragLen;
                    }
                }
            }
        }

        ok = ShapeText(aContext, aText, aOffset, fragLen, aScript, aTextRun);

        aText += fragLen;
        aOffset += fragLen;
        aLength -= fragLen;
    }

    return ok;
}

// Check if aCh is an unhandled control character that should be displayed
// as a hexbox rather than rendered by some random font on the system.
// We exclude \r as stray &#13;s are rather common (bug 941940).
// Note that \n and \t don't come through here, as they have specific
// meanings that have already been handled.
static bool
IsInvalidControlChar(uint32_t aCh)
{
    return aCh != '\r' && ((aCh & 0x7f) < 0x20 || aCh == 0x7f);
}

template<typename T>
bool
gfxFont::ShapeTextWithoutWordCache(gfxContext *aContext,
                                   const T    *aText,
                                   uint32_t    aOffset,
                                   uint32_t    aLength,
                                   int32_t     aScript,
                                   gfxTextRun *aTextRun)
{
    uint32_t fragStart = 0;
    bool ok = true;

    for (uint32_t i = 0; i <= aLength && ok; ++i) {
        T ch = (i < aLength) ? aText[i] : '\n';
        bool invalid = gfxFontGroup::IsInvalidChar(ch);
        uint32_t length = i - fragStart;

        // break into separate fragments when we hit an invalid char
        if (!invalid) {
            continue;
        }

        if (length > 0) {
            ok = ShapeFragmentWithoutWordCache(aContext, aText + fragStart,
                                               aOffset + fragStart, length,
                                               aScript, aTextRun);
        }

        if (i == aLength) {
            break;
        }

        // fragment was terminated by an invalid char: skip it,
        // unless it's a control char that we want to show as a hexbox,
        // but record where TAB or NEWLINE occur
        if (ch == '\t') {
            aTextRun->SetIsTab(aOffset + i);
        } else if (ch == '\n') {
            aTextRun->SetIsNewline(aOffset + i);
        } else if (IsInvalidControlChar(ch) &&
            !(aTextRun->GetFlags() & gfxTextRunFactory::TEXT_HIDE_CONTROL_CHARACTERS)) {
            aTextRun->SetMissingGlyph(aOffset + i, ch, this);
        }
        fragStart = i + 1;
    }

    NS_WARN_IF_FALSE(ok, "failed to shape text - expect garbled text");
    return ok;
}

#ifndef RELEASE_BUILD
#define TEXT_PERF_INCR(tp, m) (tp ? (tp)->current.m++ : 0)
#else
#define TEXT_PERF_INCR(tp, m)
#endif

inline static bool IsChar8Bit(uint8_t /*aCh*/) { return true; }
inline static bool IsChar8Bit(char16_t aCh) { return aCh < 0x100; }

inline static bool HasSpaces(const uint8_t *aString, uint32_t aLen)
{
    return memchr(aString, 0x20, aLen) != nullptr;
}

inline static bool HasSpaces(const char16_t *aString, uint32_t aLen)
{
    for (const char16_t *ch = aString; ch < aString + aLen; ch++) {
        if (*ch == 0x20) {
            return true;
        }
    }
    return false;
}

template<typename T>
bool
gfxFont::SplitAndInitTextRun(gfxContext *aContext,
                             gfxTextRun *aTextRun,
                             const T *aString, // text for this font run
                             uint32_t aRunStart, // position in the textrun
                             uint32_t aRunLength,
                             int32_t aRunScript)
{
    if (aRunLength == 0) {
        return true;
    }

    gfxTextPerfMetrics *tp = nullptr;

#ifndef RELEASE_BUILD
    tp = aTextRun->GetFontGroup()->GetTextPerfMetrics();
    if (tp) {
        if (mStyle.systemFont) {
            tp->current.numChromeTextRuns++;
        } else {
            tp->current.numContentTextRuns++;
        }
        tp->current.numChars += aRunLength;
        if (aRunLength > tp->current.maxTextRunLen) {
            tp->current.maxTextRunLen = aRunLength;
        }
    }
#endif

    uint32_t wordCacheCharLimit =
        gfxPlatform::GetPlatform()->WordCacheCharLimit();

    // If spaces can participate in shaping (e.g. within lookups for automatic
    // fractions), need to shape without using the word cache which segments
    // textruns on space boundaries. Word cache can be used if the textrun
    // is short enough to fit in the word cache and it lacks spaces.
    if (SpaceMayParticipateInShaping(aRunScript)) {
        if (aRunLength > wordCacheCharLimit ||
            HasSpaces(aString, aRunLength)) {
            TEXT_PERF_INCR(tp, wordCacheSpaceRules);
            return ShapeTextWithoutWordCache(aContext, aString,
                                             aRunStart, aRunLength, aRunScript,
                                             aTextRun);
        }
    }

    InitWordCache();

    // the only flags we care about for ShapedWord construction/caching
    uint32_t flags = aTextRun->GetFlags();
    flags &= (gfxTextRunFactory::TEXT_IS_RTL |
              gfxTextRunFactory::TEXT_DISABLE_OPTIONAL_LIGATURES |
              gfxTextRunFactory::TEXT_USE_MATH_SCRIPT);
    if (sizeof(T) == sizeof(uint8_t)) {
        flags |= gfxTextRunFactory::TEXT_IS_8BIT;
    }

    uint32_t wordStart = 0;
    uint32_t hash = 0;
    bool wordIs8Bit = true;
    int32_t appUnitsPerDevUnit = aTextRun->GetAppUnitsPerDevUnit();

    T nextCh = aString[0];
    for (uint32_t i = 0; i <= aRunLength; ++i) {
        T ch = nextCh;
        nextCh = (i < aRunLength - 1) ? aString[i + 1] : '\n';
        bool boundary = IsBoundarySpace(ch, nextCh);
        bool invalid = !boundary && gfxFontGroup::IsInvalidChar(ch);
        uint32_t length = i - wordStart;

        // break into separate ShapedWords when we hit an invalid char,
        // or a boundary space (always handled individually),
        // or the first non-space after a space
        if (!boundary && !invalid) {
            if (!IsChar8Bit(ch)) {
                wordIs8Bit = false;
            }
            // include this character in the hash, and move on to next
            hash = HashMix(hash, ch);
            continue;
        }

        // We've decided to break here (i.e. we're at the end of a "word");
        // shape the word and add it to the textrun.
        // For words longer than the limit, we don't use the
        // font's word cache but just shape directly into the textrun.
        if (length > wordCacheCharLimit) {
            TEXT_PERF_INCR(tp, wordCacheLong);
            bool ok = ShapeFragmentWithoutWordCache(aContext,
                                                    aString + wordStart,
                                                    aRunStart + wordStart,
                                                    length,
                                                    aRunScript,
                                                    aTextRun);
            if (!ok) {
                return false;
            }
        } else if (length > 0) {
            uint32_t wordFlags = flags;
            // in the 8-bit version of this method, TEXT_IS_8BIT was
            // already set as part of |flags|, so no need for a per-word
            // adjustment here
            if (sizeof(T) == sizeof(char16_t)) {
                if (wordIs8Bit) {
                    wordFlags |= gfxTextRunFactory::TEXT_IS_8BIT;
                }
            }
            gfxShapedWord *sw = GetShapedWord(aContext,
                                              aString + wordStart, length,
                                              hash, aRunScript,
                                              appUnitsPerDevUnit,
                                              wordFlags, tp);
            if (sw) {
                aTextRun->CopyGlyphDataFrom(sw, aRunStart + wordStart);
            } else {
                return false; // failed, presumably out of memory?
            }
        }

        if (boundary) {
            // word was terminated by a space: add that to the textrun
            if (!aTextRun->SetSpaceGlyphIfSimple(this, aContext,
                                                 aRunStart + i, ch))
            {
                static const uint8_t space = ' ';
                gfxShapedWord *sw =
                    GetShapedWord(aContext,
                                  &space, 1,
                                  HashMix(0, ' '), aRunScript,
                                  appUnitsPerDevUnit,
                                  flags | gfxTextRunFactory::TEXT_IS_8BIT, tp);
                if (sw) {
                    aTextRun->CopyGlyphDataFrom(sw, aRunStart + i);
                } else {
                    return false;
                }
            }
            hash = 0;
            wordStart = i + 1;
            wordIs8Bit = true;
            continue;
        }

        if (i == aRunLength) {
            break;
        }

        NS_ASSERTION(invalid,
                     "how did we get here except via an invalid char?");

        // word was terminated by an invalid char: skip it,
        // unless it's a control char that we want to show as a hexbox,
        // but record where TAB or NEWLINE occur
        if (ch == '\t') {
            aTextRun->SetIsTab(aRunStart + i);
        } else if (ch == '\n') {
            aTextRun->SetIsNewline(aRunStart + i);
        } else if (IsInvalidControlChar(ch) &&
            !(aTextRun->GetFlags() & gfxTextRunFactory::TEXT_HIDE_CONTROL_CHARACTERS)) {
            aTextRun->SetMissingGlyph(aRunStart + i, ch, this);
        }

        hash = 0;
        wordStart = i + 1;
        wordIs8Bit = true;
    }

    return true;
}

gfxGlyphExtents *
gfxFont::GetOrCreateGlyphExtents(int32_t aAppUnitsPerDevUnit) {
    uint32_t i, count = mGlyphExtentsArray.Length();
    for (i = 0; i < count; ++i) {
        if (mGlyphExtentsArray[i]->GetAppUnitsPerDevUnit() == aAppUnitsPerDevUnit)
            return mGlyphExtentsArray[i];
    }
    gfxGlyphExtents *glyphExtents = new gfxGlyphExtents(aAppUnitsPerDevUnit);
    if (glyphExtents) {
        mGlyphExtentsArray.AppendElement(glyphExtents);
        // Initialize the extents of a space glyph, assuming that spaces don't
        // render anything!
        glyphExtents->SetContainedGlyphWidthAppUnits(GetSpaceGlyph(), 0);
    }
    return glyphExtents;
}

void
gfxFont::SetupGlyphExtents(gfxContext *aContext, uint32_t aGlyphID, bool aNeedTight,
                           gfxGlyphExtents *aExtents)
{
    gfxContextMatrixAutoSaveRestore matrixRestore(aContext);
    aContext->IdentityMatrix();

    gfxRect svgBounds;
    if (mFontEntry->TryGetSVGData(this) && mFontEntry->HasSVGGlyph(aGlyphID) &&
        mFontEntry->GetSVGGlyphExtents(aContext, aGlyphID, &svgBounds)) {
        gfxFloat d2a = aExtents->GetAppUnitsPerDevUnit();
        aExtents->SetTightGlyphExtents(aGlyphID,
                                       gfxRect(svgBounds.x * d2a,
                                               svgBounds.y * d2a,
                                               svgBounds.width * d2a,
                                               svgBounds.height * d2a));
        return;
    }

    cairo_glyph_t glyph;
    glyph.index = aGlyphID;
    glyph.x = 0;
    glyph.y = 0;
    cairo_text_extents_t extents;
    cairo_glyph_extents(aContext->GetCairo(), &glyph, 1, &extents);

    const Metrics& fontMetrics = GetMetrics();
    int32_t appUnitsPerDevUnit = aExtents->GetAppUnitsPerDevUnit();
    if (!aNeedTight && extents.x_bearing >= 0 &&
        extents.y_bearing >= -fontMetrics.maxAscent &&
        extents.height + extents.y_bearing <= fontMetrics.maxDescent) {
        uint32_t appUnitsWidth =
            uint32_t(ceil((extents.x_bearing + extents.width)*appUnitsPerDevUnit));
        if (appUnitsWidth < gfxGlyphExtents::INVALID_WIDTH) {
            aExtents->SetContainedGlyphWidthAppUnits(aGlyphID, uint16_t(appUnitsWidth));
            return;
        }
    }
#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
    if (!aNeedTight) {
        ++gGlyphExtentsSetupFallBackToTight;
    }
#endif

    gfxFloat d2a = appUnitsPerDevUnit;
    gfxRect bounds(extents.x_bearing*d2a, extents.y_bearing*d2a,
                   extents.width*d2a, extents.height*d2a);
    aExtents->SetTightGlyphExtents(aGlyphID, bounds);
}

// Try to initialize font metrics by reading sfnt tables directly;
// set mIsValid=TRUE and return TRUE on success.
// Return FALSE if the gfxFontEntry subclass does not
// implement GetFontTable(), or for non-sfnt fonts where tables are
// not available.
// If this returns TRUE without setting the mIsValid flag, then we -did-
// apparently find an sfnt, but it was too broken to be used.
bool
gfxFont::InitMetricsFromSfntTables(Metrics& aMetrics)
{
    mIsValid = false; // font is NOT valid in case of early return

    const uint32_t kHheaTableTag = TRUETYPE_TAG('h','h','e','a');
    const uint32_t kPostTableTag = TRUETYPE_TAG('p','o','s','t');
    const uint32_t kOS_2TableTag = TRUETYPE_TAG('O','S','/','2');

    uint32_t len;

    if (mFUnitsConvFactor == 0.0) {
        // If the conversion factor from FUnits is not yet set,
        // get the unitsPerEm from the 'head' table via the font entry
        uint16_t unitsPerEm = GetFontEntry()->UnitsPerEm();
        if (unitsPerEm == gfxFontEntry::kInvalidUPEM) {
            return false;
        }
        mFUnitsConvFactor = mAdjustedSize / unitsPerEm;
    }

    // 'hhea' table is required to get vertical extents
    gfxFontEntry::AutoTable hheaTable(mFontEntry, kHheaTableTag);
    if (!hheaTable) {
        return false; // no 'hhea' table -> not an sfnt
    }
    const HheaTable* hhea =
        reinterpret_cast<const HheaTable*>(hb_blob_get_data(hheaTable, &len));
    if (len < sizeof(HheaTable)) {
        return false;
    }

#define SET_UNSIGNED(field,src) aMetrics.field = uint16_t(src) * mFUnitsConvFactor
#define SET_SIGNED(field,src)   aMetrics.field = int16_t(src) * mFUnitsConvFactor

    SET_UNSIGNED(maxAdvance, hhea->advanceWidthMax);
    SET_SIGNED(maxAscent, hhea->ascender);
    SET_SIGNED(maxDescent, -int16_t(hhea->descender));
    SET_SIGNED(externalLeading, hhea->lineGap);

    // 'post' table is required for underline metrics
    gfxFontEntry::AutoTable postTable(mFontEntry, kPostTableTag);
    if (!postTable) {
        return true; // no 'post' table -> sfnt is not valid
    }
    const PostTable *post =
        reinterpret_cast<const PostTable*>(hb_blob_get_data(postTable, &len));
    if (len < offsetof(PostTable, underlineThickness) + sizeof(uint16_t)) {
        return true; // bad post table -> sfnt is not valid
    }

    SET_SIGNED(underlineOffset, post->underlinePosition);
    SET_UNSIGNED(underlineSize, post->underlineThickness);

    // 'OS/2' table is optional, if not found we'll estimate xHeight
    // and aveCharWidth by measuring glyphs
    gfxFontEntry::AutoTable os2Table(mFontEntry, kOS_2TableTag);
    if (os2Table) {
        const OS2Table *os2 =
            reinterpret_cast<const OS2Table*>(hb_blob_get_data(os2Table, &len));
        if (len >= offsetof(OS2Table, sxHeight) + sizeof(int16_t) &&
            uint16_t(os2->version) >= 2) {
            // version 2 and later includes the x-height field
            SET_SIGNED(xHeight, os2->sxHeight);
            // Abs because of negative xHeight seen in Kokonor (Tibetan) font
            aMetrics.xHeight = Abs(aMetrics.xHeight);
        }
        // this should always be present in any valid OS/2 of any version
        if (len >= offsetof(OS2Table, sTypoLineGap) + sizeof(int16_t)) {
            SET_SIGNED(aveCharWidth, os2->xAvgCharWidth);
            SET_SIGNED(subscriptOffset, os2->ySubscriptYOffset);
            SET_SIGNED(superscriptOffset, os2->ySuperscriptYOffset);
            SET_SIGNED(strikeoutSize, os2->yStrikeoutSize);
            SET_SIGNED(strikeoutOffset, os2->yStrikeoutPosition);

            // for fonts with USE_TYPO_METRICS set in the fsSelection field,
            // and for all OpenType math fonts (having a 'MATH' table),
            // let the OS/2 sTypo* metrics override those from the hhea table
            // (see http://www.microsoft.com/typography/otspec/os2.htm#fss)
            const uint16_t kUseTypoMetricsMask = 1 << 7;
            if ((uint16_t(os2->fsSelection) & kUseTypoMetricsMask) ||
                mFontEntry->HasFontTable(TRUETYPE_TAG('M','A','T','H'))) {
                SET_SIGNED(maxAscent, os2->sTypoAscender);
                SET_SIGNED(maxDescent, - int16_t(os2->sTypoDescender));
                SET_SIGNED(externalLeading, os2->sTypoLineGap);
            }
        }
    }

    mIsValid = true;

    return true;
}

static double
RoundToNearestMultiple(double aValue, double aFraction)
{
    return floor(aValue/aFraction + 0.5) * aFraction;
}

void gfxFont::CalculateDerivedMetrics(Metrics& aMetrics)
{
    aMetrics.maxAscent =
        ceil(RoundToNearestMultiple(aMetrics.maxAscent, 1/1024.0));
    aMetrics.maxDescent =
        ceil(RoundToNearestMultiple(aMetrics.maxDescent, 1/1024.0));

    if (aMetrics.xHeight <= 0) {
        // only happens if we couldn't find either font metrics
        // or a char to measure;
        // pick an arbitrary value that's better than zero
        aMetrics.xHeight = aMetrics.maxAscent * DEFAULT_XHEIGHT_FACTOR;
    }

    aMetrics.maxHeight = aMetrics.maxAscent + aMetrics.maxDescent;

    if (aMetrics.maxHeight - aMetrics.emHeight > 0.0) {
        aMetrics.internalLeading = aMetrics.maxHeight - aMetrics.emHeight;
    } else {
        aMetrics.internalLeading = 0.0;
    }

    aMetrics.emAscent = aMetrics.maxAscent * aMetrics.emHeight
                            / aMetrics.maxHeight;
    aMetrics.emDescent = aMetrics.emHeight - aMetrics.emAscent;

    if (GetFontEntry()->IsFixedPitch()) {
        // Some Quartz fonts are fixed pitch, but there's some glyph with a bigger
        // advance than the average character width... this forces
        // those fonts to be recognized like fixed pitch fonts by layout.
        aMetrics.maxAdvance = aMetrics.aveCharWidth;
    }

    if (!aMetrics.subscriptOffset) {
        aMetrics.subscriptOffset = aMetrics.xHeight;
    }
    if (!aMetrics.superscriptOffset) {
        aMetrics.superscriptOffset = aMetrics.xHeight;
    }

    if (!aMetrics.strikeoutOffset) {
        aMetrics.strikeoutOffset = aMetrics.xHeight * 0.5;
    }
    if (!aMetrics.strikeoutSize) {
        aMetrics.strikeoutSize = aMetrics.underlineSize;
    }
}

void
gfxFont::SanitizeMetrics(gfxFont::Metrics *aMetrics, bool aIsBadUnderlineFont)
{
    // Even if this font size is zero, this font is created with non-zero size.
    // However, for layout and others, we should return the metrics of zero size font.
    if (mStyle.size == 0.0) {
        memset(aMetrics, 0, sizeof(gfxFont::Metrics));
        return;
    }

    // MS (P)Gothic and MS (P)Mincho are not having suitable values in their super script offset.
    // If the values are not suitable, we should use x-height instead of them.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=353632
    if (aMetrics->superscriptOffset <= 0 ||
        aMetrics->superscriptOffset >= aMetrics->maxAscent) {
        aMetrics->superscriptOffset = aMetrics->xHeight;
    }
    // And also checking the case of sub script offset. The old gfx for win has checked this too.
    if (aMetrics->subscriptOffset <= 0 ||
        aMetrics->subscriptOffset >= aMetrics->maxAscent) {
        aMetrics->subscriptOffset = aMetrics->xHeight;
    }

    aMetrics->underlineSize = std::max(1.0, aMetrics->underlineSize);
    aMetrics->strikeoutSize = std::max(1.0, aMetrics->strikeoutSize);

    aMetrics->underlineOffset = std::min(aMetrics->underlineOffset, -1.0);

    if (aMetrics->maxAscent < 1.0) {
        // We cannot draw strikeout line and overline in the ascent...
        aMetrics->underlineSize = 0;
        aMetrics->underlineOffset = 0;
        aMetrics->strikeoutSize = 0;
        aMetrics->strikeoutOffset = 0;
        return;
    }

    /**
     * Some CJK fonts have bad underline offset. Therefore, if this is such font,
     * we need to lower the underline offset to bottom of *em* descent.
     * However, if this is system font, we should not do this for the rendering compatibility with
     * another application's UI on the platform.
     * XXX Should not use this hack if the font size is too small?
     *     Such text cannot be read, this might be used for tight CSS rendering? (E.g., Acid2)
     */
    if (!mStyle.systemFont && aIsBadUnderlineFont) {
        // First, we need 2 pixels between baseline and underline at least. Because many CJK characters
        // put their glyphs on the baseline, so, 1 pixel is too close for CJK characters.
        aMetrics->underlineOffset = std::min(aMetrics->underlineOffset, -2.0);

        // Next, we put the underline to bottom of below of the descent space.
        if (aMetrics->internalLeading + aMetrics->externalLeading > aMetrics->underlineSize) {
            aMetrics->underlineOffset = std::min(aMetrics->underlineOffset, -aMetrics->emDescent);
        } else {
            aMetrics->underlineOffset = std::min(aMetrics->underlineOffset,
                                               aMetrics->underlineSize - aMetrics->emDescent);
        }
    }
    // If underline positioned is too far from the text, descent position is preferred so that underline
    // will stay within the boundary.
    else if (aMetrics->underlineSize - aMetrics->underlineOffset > aMetrics->maxDescent) {
        if (aMetrics->underlineSize > aMetrics->maxDescent)
            aMetrics->underlineSize = std::max(aMetrics->maxDescent, 1.0);
        // The max underlineOffset is 1px (the min underlineSize is 1px, and min maxDescent is 0px.)
        aMetrics->underlineOffset = aMetrics->underlineSize - aMetrics->maxDescent;
    }

    // If strikeout line is overflowed from the ascent, the line should be resized and moved for
    // that being in the ascent space.
    // Note that the strikeoutOffset is *middle* of the strikeout line position.
    gfxFloat halfOfStrikeoutSize = floor(aMetrics->strikeoutSize / 2.0 + 0.5);
    if (halfOfStrikeoutSize + aMetrics->strikeoutOffset > aMetrics->maxAscent) {
        if (aMetrics->strikeoutSize > aMetrics->maxAscent) {
            aMetrics->strikeoutSize = std::max(aMetrics->maxAscent, 1.0);
            halfOfStrikeoutSize = floor(aMetrics->strikeoutSize / 2.0 + 0.5);
        }
        gfxFloat ascent = floor(aMetrics->maxAscent + 0.5);
        aMetrics->strikeoutOffset = std::max(halfOfStrikeoutSize, ascent / 2.0);
    }

    // If overline is larger than the ascent, the line should be resized.
    if (aMetrics->underlineSize > aMetrics->maxAscent) {
        aMetrics->underlineSize = aMetrics->maxAscent;
    }
}

gfxFloat
gfxFont::SynthesizeSpaceWidth(uint32_t aCh)
{
    // return an appropriate width for various Unicode space characters
    // that we "fake" if they're not actually present in the font;
    // returns negative value if the char is not a known space.
    switch (aCh) {
    case 0x2000:                                 // en quad
    case 0x2002: return GetAdjustedSize() / 2;   // en space
    case 0x2001:                                 // em quad
    case 0x2003: return GetAdjustedSize();       // em space
    case 0x2004: return GetAdjustedSize() / 3;   // three-per-em space
    case 0x2005: return GetAdjustedSize() / 4;   // four-per-em space
    case 0x2006: return GetAdjustedSize() / 6;   // six-per-em space
    case 0x2007: return GetMetrics().zeroOrAveCharWidth; // figure space
    case 0x2008: return GetMetrics().spaceWidth; // punctuation space 
    case 0x2009: return GetAdjustedSize() / 5;   // thin space
    case 0x200a: return GetAdjustedSize() / 10;  // hair space
    case 0x202f: return GetAdjustedSize() / 5;   // narrow no-break space
    default: return -1.0;
    }
}

/*static*/ size_t
gfxFont::WordCacheEntrySizeOfExcludingThis(CacheHashEntry*   aHashEntry,
                                           MallocSizeOf aMallocSizeOf,
                                           void*             aUserArg)
{
    return aMallocSizeOf(aHashEntry->mShapedWord.get());
}

void
gfxFont::AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                                FontCacheSizes* aSizes) const
{
    for (uint32_t i = 0; i < mGlyphExtentsArray.Length(); ++i) {
        aSizes->mFontInstances +=
            mGlyphExtentsArray[i]->SizeOfIncludingThis(aMallocSizeOf);
    }
    if (mWordCache) {
        aSizes->mShapedWords +=
            mWordCache->SizeOfIncludingThis(WordCacheEntrySizeOfExcludingThis,
                                            aMallocSizeOf);
    }
}

void
gfxFont::AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf,
                                FontCacheSizes* aSizes) const
{
    aSizes->mFontInstances += aMallocSizeOf(this);
    AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
}

void
gfxFont::AddGlyphChangeObserver(GlyphChangeObserver *aObserver)
{
    if (!mGlyphChangeObservers) {
        mGlyphChangeObservers = new nsTHashtable<nsPtrHashKey<GlyphChangeObserver> >;
    }
    mGlyphChangeObservers->PutEntry(aObserver);
}

void
gfxFont::RemoveGlyphChangeObserver(GlyphChangeObserver *aObserver)
{
    NS_ASSERTION(mGlyphChangeObservers, "No observers registered");
    NS_ASSERTION(mGlyphChangeObservers->Contains(aObserver), "Observer not registered");
    mGlyphChangeObservers->RemoveEntry(aObserver);
}

gfxGlyphExtents::~gfxGlyphExtents()
{
#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
    gGlyphExtentsWidthsTotalSize +=
        mContainedGlyphWidths.SizeOfExcludingThis(&FontCacheMallocSizeOf);
    gGlyphExtentsCount++;
#endif
    MOZ_COUNT_DTOR(gfxGlyphExtents);
}

bool
gfxGlyphExtents::GetTightGlyphExtentsAppUnits(gfxFont *aFont,
    gfxContext *aContext, uint32_t aGlyphID, gfxRect *aExtents)
{
    HashEntry *entry = mTightGlyphExtents.GetEntry(aGlyphID);
    if (!entry) {
        if (!aContext) {
            NS_WARNING("Could not get glyph extents (no aContext)");
            return false;
        }

        if (aFont->SetupCairoFont(aContext)) {
#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
            ++gGlyphExtentsSetupLazyTight;
#endif
            aFont->SetupGlyphExtents(aContext, aGlyphID, true, this);
            entry = mTightGlyphExtents.GetEntry(aGlyphID);
        }
        if (!entry) {
            NS_WARNING("Could not get glyph extents");
            return false;
        }
    }

    *aExtents = gfxRect(entry->x, entry->y, entry->width, entry->height);
    return true;
}

gfxGlyphExtents::GlyphWidths::~GlyphWidths()
{
    uint32_t i, count = mBlocks.Length();
    for (i = 0; i < count; ++i) {
        uintptr_t bits = mBlocks[i];
        if (bits && !(bits & 0x1)) {
            delete[] reinterpret_cast<uint16_t *>(bits);
        }
    }
}

uint32_t
gfxGlyphExtents::GlyphWidths::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
    uint32_t i;
    uint32_t size = mBlocks.SizeOfExcludingThis(aMallocSizeOf);
    for (i = 0; i < mBlocks.Length(); ++i) {
        uintptr_t bits = mBlocks[i];
        if (bits && !(bits & 0x1)) {
            size += aMallocSizeOf(reinterpret_cast<void*>(bits));
        }
    }
    return size;
}

void
gfxGlyphExtents::GlyphWidths::Set(uint32_t aGlyphID, uint16_t aWidth)
{
    uint32_t block = aGlyphID >> BLOCK_SIZE_BITS;
    uint32_t len = mBlocks.Length();
    if (block >= len) {
        uintptr_t *elems = mBlocks.AppendElements(block + 1 - len);
        if (!elems)
            return;
        memset(elems, 0, sizeof(uintptr_t)*(block + 1 - len));
    }

    uintptr_t bits = mBlocks[block];
    uint32_t glyphOffset = aGlyphID & (BLOCK_SIZE - 1);
    if (!bits) {
        mBlocks[block] = MakeSingle(glyphOffset, aWidth);
        return;
    }

    uint16_t *newBlock;
    if (bits & 0x1) {
        // Expand the block to a real block. We could avoid this by checking
        // glyphOffset == GetGlyphOffset(bits), but that never happens so don't bother
        newBlock = new uint16_t[BLOCK_SIZE];
        if (!newBlock)
            return;
        uint32_t i;
        for (i = 0; i < BLOCK_SIZE; ++i) {
            newBlock[i] = INVALID_WIDTH;
        }
        newBlock[GetGlyphOffset(bits)] = GetWidth(bits);
        mBlocks[block] = reinterpret_cast<uintptr_t>(newBlock);
    } else {
        newBlock = reinterpret_cast<uint16_t *>(bits);
    }
    newBlock[glyphOffset] = aWidth;
}

void
gfxGlyphExtents::SetTightGlyphExtents(uint32_t aGlyphID, const gfxRect& aExtentsAppUnits)
{
    HashEntry *entry = mTightGlyphExtents.PutEntry(aGlyphID);
    if (!entry)
        return;
    entry->x = aExtentsAppUnits.X();
    entry->y = aExtentsAppUnits.Y();
    entry->width = aExtentsAppUnits.Width();
    entry->height = aExtentsAppUnits.Height();
}

size_t
gfxGlyphExtents::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
    return mContainedGlyphWidths.SizeOfExcludingThis(aMallocSizeOf) +
        mTightGlyphExtents.SizeOfExcludingThis(nullptr, aMallocSizeOf);
}

size_t
gfxGlyphExtents::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

gfxFontGroup::gfxFontGroup(const FontFamilyList& aFontFamilyList,
                           const gfxFontStyle *aStyle,
                           gfxUserFontSet *aUserFontSet)
    : mFamilyList(aFontFamilyList)
    , mStyle(*aStyle)
    , mUnderlineOffset(UNDERLINE_OFFSET_NOT_SET)
    , mHyphenWidth(-1)
    , mUserFontSet(aUserFontSet)
    , mTextPerf(nullptr)
    , mPageLang(gfxPlatform::GetFontPrefLangFor(aStyle->language))
    , mSkipDrawing(false)
{
    // We don't use SetUserFontSet() here, as we want to unconditionally call
    // BuildFontList() rather than only do UpdateFontList() if it changed.
    mCurrGeneration = GetGeneration();
    BuildFontList();
}

void
gfxFontGroup::FindGenericFonts(FontFamilyType aGenericType,
                               nsIAtom *aLanguage,
                               void *aClosure)
{
    nsAutoTArray<nsString, 5> resolvedGenerics;
    ResolveGenericFontNames(aGenericType, aLanguage, resolvedGenerics);
    uint32_t g = 0, numGenerics = resolvedGenerics.Length();
    for (g = 0; g < numGenerics; g++) {
        FindPlatformFont(resolvedGenerics[g], false, aClosure);
    }
}

/* static */ void
gfxFontGroup::ResolveGenericFontNames(FontFamilyType aGenericType,
                                      nsIAtom *aLanguage,
                                      nsTArray<nsString>& aGenericFamilies)
{
    static const char kGeneric_serif[] = "serif";
    static const char kGeneric_sans_serif[] = "sans-serif";
    static const char kGeneric_monospace[] = "monospace";
    static const char kGeneric_cursive[] = "cursive";
    static const char kGeneric_fantasy[] = "fantasy";

    // type should be standard generic type at this point
    NS_ASSERTION(aGenericType >= eFamily_serif &&
                 aGenericType <= eFamily_fantasy,
                 "standard generic font family type required");

    // create the lang string
    nsIAtom *langGroupAtom = nullptr;
    nsAutoCString langGroupString;
    if (aLanguage) {
        if (!gLangService) {
            CallGetService(NS_LANGUAGEATOMSERVICE_CONTRACTID, &gLangService);
        }
        if (gLangService) {
            nsresult rv;
            langGroupAtom = gLangService->GetLanguageGroup(aLanguage, &rv);
        }
    }
    if (!langGroupAtom) {
        langGroupAtom = nsGkAtoms::Unicode;
    }
    langGroupAtom->ToUTF8String(langGroupString);

    // map generic type to string
    const char *generic = nullptr;
    switch (aGenericType) {
        case eFamily_serif:
            generic = kGeneric_serif;
            break;
        case eFamily_sans_serif:
            generic = kGeneric_sans_serif;
            break;
        case eFamily_monospace:
            generic = kGeneric_monospace;
            break;
        case eFamily_cursive:
            generic = kGeneric_cursive;
            break;
        case eFamily_fantasy:
            generic = kGeneric_fantasy;
            break;
        default:
            break;
    }

    if (!generic) {
        return;
    }

    aGenericFamilies.Clear();

    // load family for "font.name.generic.lang"
    nsAutoCString prefFontName("font.name.");
    prefFontName.Append(generic);
    prefFontName.Append('.');
    prefFontName.Append(langGroupString);
    gfxFontUtils::AppendPrefsFontList(prefFontName.get(),
                                      aGenericFamilies);

    // if lang has pref fonts, also load fonts for "font.name-list.generic.lang"
    if (!aGenericFamilies.IsEmpty()) {
        nsAutoCString prefFontListName("font.name-list.");
        prefFontListName.Append(generic);
        prefFontListName.Append('.');
        prefFontListName.Append(langGroupString);
        gfxFontUtils::AppendPrefsFontList(prefFontListName.get(),
                                          aGenericFamilies);
    }

#if 0  // dump out generic mappings
    printf("%s ===> ", prefFontName.get());
    for (uint32_t k = 0; k < aGenericFamilies.Length(); k++) {
        if (k > 0) printf(", ");
        printf("%s", NS_ConvertUTF16toUTF8(aGenericFamilies[k]).get());
    }
    printf("\n");
#endif
}

void gfxFontGroup::EnumerateFontList(nsIAtom *aLanguage, void *aClosure)
{
    // initialize fonts in the font family list
    const nsTArray<FontFamilyName>& fontlist = mFamilyList.GetFontlist();

    // lookup fonts in the fontlist
    uint32_t i, numFonts = fontlist.Length();
    for (i = 0; i < numFonts; i++) {
        const FontFamilyName& name = fontlist[i];
        if (name.IsNamed()) {
            FindPlatformFont(name.mName, true, aClosure);
        } else {
            FindGenericFonts(name.mType, aLanguage, aClosure);
        }
    }

    // if necessary, append default generic onto the end
    if (mFamilyList.GetDefaultFontType() != eFamily_none &&
        !mFamilyList.HasDefaultGeneric()) {
        FindGenericFonts(mFamilyList.GetDefaultFontType(),
                         aLanguage,
                         aClosure);
    }
}

void
gfxFontGroup::BuildFontList()
{
// gfxPangoFontGroup behaves differently, so this method is a no-op on that platform
#if defined(XP_MACOSX) || defined(XP_WIN) || defined(ANDROID)

    EnumerateFontList(mStyle.language);

    // at this point, fontlist should have been filled in
    // get a default font if none exists
    if (mFonts.Length() == 0) {
        bool needsBold;
        gfxPlatformFontList *pfl = gfxPlatformFontList::PlatformFontList();
        gfxFontFamily *defaultFamily = pfl->GetDefaultFont(&mStyle);
        NS_ASSERTION(defaultFamily,
                     "invalid default font returned by GetDefaultFont");

        if (defaultFamily) {
            gfxFontEntry *fe = defaultFamily->FindFontForStyle(mStyle,
                                                               needsBold);
            if (fe) {
                nsRefPtr<gfxFont> font = fe->FindOrMakeFont(&mStyle,
                                                            needsBold);
                if (font) {
                    mFonts.AppendElement(FamilyFace(defaultFamily, font));
                }
            }
        }

        if (mFonts.Length() == 0) {
            // Try for a "font of last resort...."
            // Because an empty font list would be Really Bad for later code
            // that assumes it will be able to get valid metrics for layout,
            // just look for the first usable font and put in the list.
            // (see bug 554544)
            nsAutoTArray<nsRefPtr<gfxFontFamily>,200> families;
            pfl->GetFontFamilyList(families);
            uint32_t count = families.Length();
            for (uint32_t i = 0; i < count; ++i) {
                gfxFontEntry *fe = families[i]->FindFontForStyle(mStyle,
                                                                 needsBold);
                if (fe) {
                    nsRefPtr<gfxFont> font = fe->FindOrMakeFont(&mStyle,
                                                                needsBold);
                    if (font) {
                        mFonts.AppendElement(FamilyFace(families[i], font));
                        break;
                    }
                }
            }
        }

        if (mFonts.Length() == 0) {
            // an empty font list at this point is fatal; we're not going to
            // be able to do even the most basic layout operations
            char msg[256]; // CHECK buffer length if revising message below
            nsAutoString families;
            mFamilyList.ToString(families);
            sprintf(msg, "unable to find a usable font (%.220s)",
                    NS_ConvertUTF16toUTF8(families).get());
            NS_RUNTIMEABORT(msg);
        }
    }

    if (!mStyle.systemFont) {
        uint32_t count = mFonts.Length();
        for (uint32_t i = 0; i < count; ++i) {
            gfxFont* font = mFonts[i].Font();
            if (font->GetFontEntry()->mIsBadUnderlineFont) {
                gfxFloat first = mFonts[0].Font()->GetMetrics().underlineOffset;
                gfxFloat bad = font->GetMetrics().underlineOffset;
                mUnderlineOffset = std::min(first, bad);
                break;
            }
        }
    }
#endif
}

void
gfxFontGroup::FindPlatformFont(const nsAString& aName,
                               bool aUseFontSet,
                               void *aClosure)
{
    bool needsBold;
    gfxFontFamily *family = nullptr;
    gfxFontEntry *fe = nullptr;

    if (aUseFontSet) {
        // First, look up in the user font set...
        // If the fontSet matches the family, we must not look for a platform
        // font of the same name, even if we fail to actually get a fontEntry
        // here; we'll fall back to the next name in the CSS font-family list.
        if (mUserFontSet) {
            // If the fontSet matches the family, but the font has not yet finished
            // loading (nor has its load timeout fired), the fontGroup should wait
            // for the download, and not actually draw its text yet.
            family = mUserFontSet->GetFamily(aName);
            if (family) {
                bool waitForUserFont = false;
                fe = mUserFontSet->FindFontEntry(family, mStyle,
                                                 needsBold, waitForUserFont);
                if (!fe && waitForUserFont) {
                    mSkipDrawing = true;
                }
            }
        }
    }

    // Not known in the user font set ==> check system fonts
    if (!family) {
        gfxPlatformFontList *fontList = gfxPlatformFontList::PlatformFontList();
        family = fontList->FindFamily(aName);
        if (family) {
            fe = family->FindFontForStyle(mStyle, needsBold);
        }
    }

    // add to the font group, unless it's already there
    if (fe && !HasFont(fe)) {
        nsRefPtr<gfxFont> font = fe->FindOrMakeFont(&mStyle, needsBold);
        if (font) {
            mFonts.AppendElement(FamilyFace(family, font));
        }
    }
}

bool
gfxFontGroup::HasFont(const gfxFontEntry *aFontEntry)
{
    uint32_t count = mFonts.Length();
    for (uint32_t i = 0; i < count; ++i) {
        if (mFonts[i].Font()->GetFontEntry() == aFontEntry)
            return true;
    }
    return false;
}

gfxFontGroup::~gfxFontGroup()
{
    mFonts.Clear();
}

gfxFontGroup *
gfxFontGroup::Copy(const gfxFontStyle *aStyle)
{
    gfxFontGroup *fg = new gfxFontGroup(mFamilyList, aStyle, mUserFontSet);
    fg->SetTextPerfMetrics(mTextPerf);
    return fg;
}

bool 
gfxFontGroup::IsInvalidChar(uint8_t ch)
{
    return ((ch & 0x7f) < 0x20 || ch == 0x7f);
}

bool 
gfxFontGroup::IsInvalidChar(char16_t ch)
{
    // All printable 7-bit ASCII values are OK
    if (ch >= ' ' && ch < 0x7f) {
        return false;
    }
    // No point in sending non-printing control chars through font shaping
    if (ch <= 0x9f) {
        return true;
    }
    return (((ch & 0xFF00) == 0x2000 /* Unicode control character */ &&
             (ch == 0x200B/*ZWSP*/ || ch == 0x2028/*LSEP*/ || ch == 0x2029/*PSEP*/)) ||
            IsBidiControl(ch));
}

gfxTextRun *
gfxFontGroup::MakeEmptyTextRun(const Parameters *aParams, uint32_t aFlags)
{
    aFlags |= TEXT_IS_8BIT | TEXT_IS_ASCII | TEXT_IS_PERSISTENT;
    return gfxTextRun::Create(aParams, 0, this, aFlags);
}

gfxTextRun *
gfxFontGroup::MakeSpaceTextRun(const Parameters *aParams, uint32_t aFlags)
{
    aFlags |= TEXT_IS_8BIT | TEXT_IS_ASCII | TEXT_IS_PERSISTENT;

    gfxTextRun *textRun = gfxTextRun::Create(aParams, 1, this, aFlags);
    if (!textRun) {
        return nullptr;
    }

    gfxFont *font = GetFontAt(0);
    if (MOZ_UNLIKELY(GetStyle()->size == 0)) {
        // Short-circuit for size-0 fonts, as Windows and ATSUI can't handle
        // them, and always create at least size 1 fonts, i.e. they still
        // render something for size 0 fonts.
        textRun->AddGlyphRun(font, gfxTextRange::kFontGroup, 0, false);
    }
    else {
        if (font->GetSpaceGlyph()) {
            // Normally, the font has a cached space glyph, so we can avoid
            // the cost of calling FindFontForChar.
            textRun->SetSpaceGlyph(font, aParams->mContext, 0);
        } else {
            // In case the primary font doesn't have <space> (bug 970891),
            // find one that does.
            uint8_t matchType;
            nsRefPtr<gfxFont> spaceFont =
                FindFontForChar(' ', 0, MOZ_SCRIPT_LATIN, nullptr, &matchType);
            if (spaceFont) {
                textRun->SetSpaceGlyph(spaceFont, aParams->mContext, 0);
            }
        }
    }

    // Note that the gfxGlyphExtents glyph bounds storage for the font will
    // always contain an entry for the font's space glyph, so we don't have
    // to call FetchGlyphExtents here.
    return textRun;
}

gfxTextRun *
gfxFontGroup::MakeBlankTextRun(uint32_t aLength,
                               const Parameters *aParams, uint32_t aFlags)
{
    gfxTextRun *textRun =
        gfxTextRun::Create(aParams, aLength, this, aFlags);
    if (!textRun) {
        return nullptr;
    }

    textRun->AddGlyphRun(GetFontAt(0), gfxTextRange::kFontGroup, 0, false);
    return textRun;
}

gfxTextRun *
gfxFontGroup::MakeHyphenTextRun(gfxContext *aCtx, uint32_t aAppUnitsPerDevUnit)
{
    // only use U+2010 if it is supported by the first font in the group;
    // it's better to use ASCII '-' from the primary font than to fall back to
    // U+2010 from some other, possibly poorly-matching face
    static const char16_t hyphen = 0x2010;
    gfxFont *font = GetFontAt(0);
    if (font && font->HasCharacter(hyphen)) {
        return MakeTextRun(&hyphen, 1, aCtx, aAppUnitsPerDevUnit,
                           gfxFontGroup::TEXT_IS_PERSISTENT);
    }

    static const uint8_t dash = '-';
    return MakeTextRun(&dash, 1, aCtx, aAppUnitsPerDevUnit,
                       gfxFontGroup::TEXT_IS_PERSISTENT);
}

gfxFloat
gfxFontGroup::GetHyphenWidth(gfxTextRun::PropertyProvider *aProvider)
{
    if (mHyphenWidth < 0) {
        nsRefPtr<gfxContext> ctx(aProvider->GetContext());
        if (ctx) {
            nsAutoPtr<gfxTextRun>
                hyphRun(MakeHyphenTextRun(ctx,
                                          aProvider->GetAppUnitsPerDevUnit()));
            mHyphenWidth = hyphRun.get() ?
                hyphRun->GetAdvanceWidth(0, hyphRun->GetLength(), nullptr) : 0;
        }
    }
    return mHyphenWidth;
}

gfxTextRun *
gfxFontGroup::MakeTextRun(const uint8_t *aString, uint32_t aLength,
                          const Parameters *aParams, uint32_t aFlags)
{
    if (aLength == 0) {
        return MakeEmptyTextRun(aParams, aFlags);
    }
    if (aLength == 1 && aString[0] == ' ') {
        return MakeSpaceTextRun(aParams, aFlags);
    }

    aFlags |= TEXT_IS_8BIT;

    if (GetStyle()->size == 0) {
        // Short-circuit for size-0 fonts, as Windows and ATSUI can't handle
        // them, and always create at least size 1 fonts, i.e. they still
        // render something for size 0 fonts.
        return MakeBlankTextRun(aLength, aParams, aFlags);
    }

    gfxTextRun *textRun = gfxTextRun::Create(aParams, aLength,
                                             this, aFlags);
    if (!textRun) {
        return nullptr;
    }

    InitTextRun(aParams->mContext, textRun, aString, aLength);

    textRun->FetchGlyphExtents(aParams->mContext);

    return textRun;
}

gfxTextRun *
gfxFontGroup::MakeTextRun(const char16_t *aString, uint32_t aLength,
                          const Parameters *aParams, uint32_t aFlags)
{
    if (aLength == 0) {
        return MakeEmptyTextRun(aParams, aFlags);
    }
    if (aLength == 1 && aString[0] == ' ') {
        return MakeSpaceTextRun(aParams, aFlags);
    }
    if (GetStyle()->size == 0) {
        return MakeBlankTextRun(aLength, aParams, aFlags);
    }

    gfxTextRun *textRun = gfxTextRun::Create(aParams, aLength,
                                             this, aFlags);
    if (!textRun) {
        return nullptr;
    }

    InitTextRun(aParams->mContext, textRun, aString, aLength);

    textRun->FetchGlyphExtents(aParams->mContext);

    return textRun;
}

template<typename T>
void
gfxFontGroup::InitTextRun(gfxContext *aContext,
                          gfxTextRun *aTextRun,
                          const T *aString,
                          uint32_t aLength)
{
    NS_ASSERTION(aLength > 0, "don't call InitTextRun for a zero-length run");

    // we need to do numeral processing even on 8-bit text,
    // in case we're converting Western to Hindi/Arabic digits
    int32_t numOption = gfxPlatform::GetPlatform()->GetBidiNumeralOption();
    nsAutoArrayPtr<char16_t> transformedString;
    if (numOption != IBMBIDI_NUMERAL_NOMINAL) {
        // scan the string for numerals that may need to be transformed;
        // if we find any, we'll make a local copy here and use that for
        // font matching and glyph generation/shaping
        bool prevIsArabic =
            (aTextRun->GetFlags() & gfxTextRunFactory::TEXT_INCOMING_ARABICCHAR) != 0;
        for (uint32_t i = 0; i < aLength; ++i) {
            char16_t origCh = aString[i];
            char16_t newCh = HandleNumberInChar(origCh, prevIsArabic, numOption);
            if (newCh != origCh) {
                if (!transformedString) {
                    transformedString = new char16_t[aLength];
                    if (sizeof(T) == sizeof(char16_t)) {
                        memcpy(transformedString.get(), aString, i * sizeof(char16_t));
                    } else {
                        for (uint32_t j = 0; j < i; ++j) {
                            transformedString[j] = aString[j];
                        }
                    }
                }
            }
            if (transformedString) {
                transformedString[i] = newCh;
            }
            prevIsArabic = IS_ARABIC_CHAR(newCh);
        }
    }

#ifdef PR_LOGGING
    PRLogModuleInfo *log = (mStyle.systemFont ?
                            gfxPlatform::GetLog(eGfxLog_textrunui) :
                            gfxPlatform::GetLog(eGfxLog_textrun));
#endif

    if (sizeof(T) == sizeof(uint8_t) && !transformedString) {

#ifdef PR_LOGGING
        if (MOZ_UNLIKELY(PR_LOG_TEST(log, PR_LOG_WARNING))) {
            nsAutoCString lang;
            mStyle.language->ToUTF8String(lang);
            nsAutoString families;
            mFamilyList.ToString(families);
            nsAutoCString str((const char*)aString, aLength);
            PR_LOG(log, PR_LOG_WARNING,\
                   ("(%s) fontgroup: [%s] default: %s lang: %s script: %d "
                    "len %d weight: %d width: %d style: %s size: %6.2f %d-byte "
                    "TEXTRUN [%s] ENDTEXTRUN\n",
                    (mStyle.systemFont ? "textrunui" : "textrun"),
                    NS_ConvertUTF16toUTF8(families).get(),
                    (mFamilyList.GetDefaultFontType() == eFamily_serif ?
                     "serif" :
                     (mFamilyList.GetDefaultFontType() == eFamily_sans_serif ?
                      "sans-serif" : "none")),
                    lang.get(), MOZ_SCRIPT_LATIN, aLength,
                    uint32_t(mStyle.weight), uint32_t(mStyle.stretch),
                    (mStyle.style & NS_FONT_STYLE_ITALIC ? "italic" :
                    (mStyle.style & NS_FONT_STYLE_OBLIQUE ? "oblique" :
                                                            "normal")),
                    mStyle.size,
                    sizeof(T),
                    str.get()));
        }
#endif

        // the text is still purely 8-bit; bypass the script-run itemizer
        // and treat it as a single Latin run
        InitScriptRun(aContext, aTextRun, aString,
                      0, aLength, MOZ_SCRIPT_LATIN);
    } else {
        const char16_t *textPtr;
        if (transformedString) {
            textPtr = transformedString.get();
        } else {
            // typecast to avoid compilation error for the 8-bit version,
            // even though this is dead code in that case
            textPtr = reinterpret_cast<const char16_t*>(aString);
        }

        // split into script runs so that script can potentially influence
        // the font matching process below
        gfxScriptItemizer scriptRuns(textPtr, aLength);

        uint32_t runStart = 0, runLimit = aLength;
        int32_t runScript = MOZ_SCRIPT_LATIN;
        while (scriptRuns.Next(runStart, runLimit, runScript)) {

#ifdef PR_LOGGING
            if (MOZ_UNLIKELY(PR_LOG_TEST(log, PR_LOG_WARNING))) {
                nsAutoCString lang;
                mStyle.language->ToUTF8String(lang);
                nsAutoString families;
                mFamilyList.ToString(families);
                uint32_t runLen = runLimit - runStart;
                PR_LOG(log, PR_LOG_WARNING,\
                       ("(%s) fontgroup: [%s] default: %s lang: %s script: %d "
                        "len %d weight: %d width: %d style: %s size: %6.2f "
                        "%d-byte TEXTRUN [%s] ENDTEXTRUN\n",
                        (mStyle.systemFont ? "textrunui" : "textrun"),
                        NS_ConvertUTF16toUTF8(families).get(),
                        (mFamilyList.GetDefaultFontType() == eFamily_serif ?
                         "serif" :
                         (mFamilyList.GetDefaultFontType() == eFamily_sans_serif ?
                          "sans-serif" : "none")),
                        lang.get(), runScript, runLen,
                        uint32_t(mStyle.weight), uint32_t(mStyle.stretch),
                        (mStyle.style & NS_FONT_STYLE_ITALIC ? "italic" :
                        (mStyle.style & NS_FONT_STYLE_OBLIQUE ? "oblique" :
                                                                "normal")),
                        mStyle.size,
                        sizeof(T),
                        NS_ConvertUTF16toUTF8(textPtr + runStart, runLen).get()));
            }
#endif

            InitScriptRun(aContext, aTextRun, textPtr + runStart,
                          runStart, runLimit - runStart, runScript);
        }
    }

    if (sizeof(T) == sizeof(char16_t) && aLength > 0) {
        gfxTextRun::CompressedGlyph *glyph = aTextRun->GetCharacterGlyphs();
        if (!glyph->IsSimpleGlyph()) {
            glyph->SetClusterStart(true);
        }
    }

    // It's possible for CoreText to omit glyph runs if it decides they contain
    // only invisibles (e.g., U+FEFF, see reftest 474417-1). In this case, we
    // need to eliminate them from the glyph run array to avoid drawing "partial
    // ligatures" with the wrong font.
    // We don't do this during InitScriptRun (or gfxFont::InitTextRun) because
    // it will iterate back over all glyphruns in the textrun, which leads to
    // pathologically-bad perf in the case where a textrun contains many script
    // changes (see bug 680402) - we'd end up re-sanitizing all the earlier runs
    // every time a new script subrun is processed.
    aTextRun->SanitizeGlyphRuns();

    aTextRun->SortGlyphRuns();
}

template<typename T>
void
gfxFontGroup::InitScriptRun(gfxContext *aContext,
                            gfxTextRun *aTextRun,
                            const T *aString, // text for this script run,
                                              // not the entire textrun
                            uint32_t aOffset, // position of the script run
                                              // within the textrun
                            uint32_t aLength, // length of the script run
                            int32_t aRunScript)
{
    NS_ASSERTION(aLength > 0, "don't call InitScriptRun for a 0-length run");

    gfxFont *mainFont = GetFontAt(0);

    uint32_t runStart = 0;
    nsAutoTArray<gfxTextRange,3> fontRanges;
    ComputeRanges(fontRanges, aString, aLength, aRunScript);
    uint32_t numRanges = fontRanges.Length();

    for (uint32_t r = 0; r < numRanges; r++) {
        const gfxTextRange& range = fontRanges[r];
        uint32_t matchedLength = range.Length();
        gfxFont *matchedFont = range.font;

        // create the glyph run for this range
        if (matchedFont) {
            if (mStyle.smallCaps &&
                !matchedFont->SupportsSmallCaps(aRunScript)) {
                if (!matchedFont->InitFakeSmallCapsRun(aContext, aTextRun,
                                                       aString + runStart,
                                                       aOffset + runStart,
                                                       matchedLength,
                                                       range.matchType,
                                                       aRunScript)) {
                    matchedFont = nullptr;
                }
            } else {
                aTextRun->AddGlyphRun(matchedFont, range.matchType,
                                      aOffset + runStart, (matchedLength > 0));
                // do glyph layout and record the resulting positioned glyphs
                if (!matchedFont->SplitAndInitTextRun(aContext, aTextRun,
                                                      aString + runStart,
                                                      aOffset + runStart,
                                                      matchedLength,
                                                      aRunScript)) {
                    // glyph layout failed! treat as missing glyphs
                    matchedFont = nullptr;
                }
            }
        } else {
            aTextRun->AddGlyphRun(mainFont, gfxTextRange::kFontGroup,
                                  aOffset + runStart, (matchedLength > 0));
        }

        if (!matchedFont) {
            // We need to set cluster boundaries (and mark spaces) so that
            // surrogate pairs, combining characters, etc behave properly,
            // even if we don't have glyphs for them
            aTextRun->SetupClusterBoundaries(aOffset + runStart, aString + runStart,
                                             matchedLength);

            // various "missing" characters may need special handling,
            // so we check for them here
            uint32_t runLimit = runStart + matchedLength;
            for (uint32_t index = runStart; index < runLimit; index++) {
                T ch = aString[index];

                // tab and newline are not to be displayed as hexboxes,
                // but do need to be recorded in the textrun
                if (ch == '\n') {
                    aTextRun->SetIsNewline(aOffset + index);
                    continue;
                }
                if (ch == '\t') {
                    aTextRun->SetIsTab(aOffset + index);
                    continue;
                }

                // for 16-bit textruns only, check for surrogate pairs and
                // special Unicode spaces; omit these checks in 8-bit runs
                if (sizeof(T) == sizeof(char16_t)) {
                    if (NS_IS_HIGH_SURROGATE(ch) &&
                        index + 1 < aLength &&
                        NS_IS_LOW_SURROGATE(aString[index + 1]))
                    {
                        aTextRun->SetMissingGlyph(aOffset + index,
                                                  SURROGATE_TO_UCS4(ch,
                                                                    aString[index + 1]),
                                                  mainFont);
                        index++;
                        continue;
                    }

                    // check if this is a known Unicode whitespace character that
                    // we can render using the space glyph with a custom width
                    gfxFloat wid = mainFont->SynthesizeSpaceWidth(ch);
                    if (wid >= 0.0) {
                        nscoord advance =
                            aTextRun->GetAppUnitsPerDevUnit() * floor(wid + 0.5);
                        if (gfxShapedText::CompressedGlyph::IsSimpleAdvance(advance)) {
                            aTextRun->GetCharacterGlyphs()[aOffset + index].
                                SetSimpleGlyph(advance,
                                               mainFont->GetSpaceGlyph());
                        } else {
                            gfxTextRun::DetailedGlyph detailedGlyph;
                            detailedGlyph.mGlyphID = mainFont->GetSpaceGlyph();
                            detailedGlyph.mAdvance = advance;
                            detailedGlyph.mXOffset = detailedGlyph.mYOffset = 0;
                            gfxShapedText::CompressedGlyph g;
                            g.SetComplex(true, true, 1);
                            aTextRun->SetGlyphs(aOffset + index,
                                                g, &detailedGlyph);
                        }
                        continue;
                    }
                }

                if (IsInvalidChar(ch)) {
                    // invalid chars are left as zero-width/invisible
                    continue;
                }

                // record char code so we can draw a box with the Unicode value
                aTextRun->SetMissingGlyph(aOffset + index, ch, mainFont);
            }
        }

        runStart += matchedLength;
    }
}

bool
gfxFont::InitFakeSmallCapsRun(gfxContext     *aContext,
                              gfxTextRun     *aTextRun,
                              const uint8_t  *aText,
                              uint32_t        aOffset,
                              uint32_t        aLength,
                              uint8_t         aMatchType,
                              int32_t         aScript)
{
    NS_ConvertASCIItoUTF16 unicodeString(reinterpret_cast<const char*>(aText),
                                         aLength);
    return InitFakeSmallCapsRun(aContext, aTextRun, unicodeString.get(),
                                aOffset, aLength, aMatchType, aScript);
}

bool
gfxFont::InitFakeSmallCapsRun(gfxContext     *aContext,
                              gfxTextRun     *aTextRun,
                              const char16_t *aText,
                              uint32_t        aOffset,
                              uint32_t        aLength,
                              uint8_t         aMatchType,
                              int32_t         aScript)
{
    bool ok = true;

    nsRefPtr<gfxFont> smallCapsFont = GetSmallCapsFont();

    enum RunCaseState {
        kUpperOrCaseless, // will be untouched by font-variant:small-caps
        kLowercase,       // will be uppercased and reduced
        kSpecialUpper     // specials: don't shrink, but apply uppercase mapping
    };
    RunCaseState runCase = kUpperOrCaseless;
    uint32_t runStart = 0;

    for (uint32_t i = 0; i <= aLength; ++i) {
        RunCaseState chCase = kUpperOrCaseless;
        // Unless we're at the end, figure out what treatment the current
        // character will need.
        if (i < aLength) {
            uint32_t ch = aText[i];
            if (NS_IS_HIGH_SURROGATE(ch) && i < aLength - 1 &&
                NS_IS_LOW_SURROGATE(aText[i + 1])) {
                ch = SURROGATE_TO_UCS4(ch, aText[i + 1]);
            }
            // Characters that aren't the start of a cluster are ignored here.
            // They get added to whatever lowercase/non-lowercase run we're in.
            if (IsClusterExtender(ch)) {
                chCase = runCase;
            } else {
                uint32_t ch2 = ToUpperCase(ch);
                if (ch != ch2 || mozilla::unicode::SpecialUpper(ch)) {
                    chCase = kLowercase;
                }
                else if (mStyle.language == nsGkAtoms::el) {
                    // In Greek, check for characters that will be modified by
                    // the GreekUpperCase mapping - this catches accented
                    // capitals where the accent is to be removed (bug 307039).
                    // These are handled by using the full-size font with the
                    // uppercasing transform.
                    mozilla::GreekCasing::State state;
                    ch2 = mozilla::GreekCasing::UpperCase(ch, state);
                    if (ch != ch2) {
                        chCase = kSpecialUpper;
                    }
                }
            }
        }

        // At the end of the text or when the current character needs different
        // casing treatment from the current run, finish the run-in-progress
        // and prepare to accumulate a new run.
        // Note that we do not look at any source data for offset [i] here,
        // as that would be invalid in the case where i==length.
        if ((i == aLength || runCase != chCase) && runStart < i) {
            uint32_t runLength = i - runStart;
            gfxFont* f = this;
            switch (runCase) {
            case kUpperOrCaseless:
                // just use the current font and the existing string
                aTextRun->AddGlyphRun(f, aMatchType, aOffset + runStart, true);
                if (!f->SplitAndInitTextRun(aContext, aTextRun,
                                            aText + runStart,
                                            aOffset + runStart, runLength,
                                            aScript)) {
                    ok = false;
                }
                break;

            case kLowercase:
                // use reduced-size font, fall through to uppercase the text
                f = smallCapsFont;

            case kSpecialUpper:
                // apply uppercase transform to the string
                nsDependentSubstring origString(aText + runStart, runLength);
                nsAutoString convertedString;
                nsAutoTArray<bool,50> charsToMergeArray;
                nsAutoTArray<bool,50> deletedCharsArray;

                bool mergeNeeded = nsCaseTransformTextRunFactory::
                    TransformString(origString,
                                    convertedString,
                                    true,
                                    mStyle.language,
                                    charsToMergeArray,
                                    deletedCharsArray);

                if (mergeNeeded) {
                    // This is the hard case: the transformation caused chars
                    // to be inserted or deleted, so we can't shape directly
                    // into the destination textrun but have to handle the
                    // mismatch of character positions.
                    gfxTextRunFactory::Parameters params = {
                        aContext, nullptr, nullptr, nullptr, 0,
                        aTextRun->GetAppUnitsPerDevUnit()
                    };
                    nsAutoPtr<gfxTextRun> tempRun;
                    tempRun =
                        gfxTextRun::Create(&params, convertedString.Length(),
                                           aTextRun->GetFontGroup(), 0);
                    tempRun->AddGlyphRun(f, aMatchType, 0, true);
                    if (!f->SplitAndInitTextRun(aContext, tempRun,
                                                convertedString.BeginReading(),
                                                0, convertedString.Length(),
                                                aScript)) {
                        ok = false;
                    } else {
                        nsAutoPtr<gfxTextRun> mergedRun;
                        mergedRun =
                            gfxTextRun::Create(&params, runLength,
                                               aTextRun->GetFontGroup(), 0);
                        MergeCharactersInTextRun(mergedRun, tempRun,
                                                 charsToMergeArray.Elements(),
                                                 deletedCharsArray.Elements());
                        aTextRun->CopyGlyphDataFrom(mergedRun, 0, runLength,
                                                    aOffset + runStart);
                    }
                } else {
                    aTextRun->AddGlyphRun(f, aMatchType, aOffset + runStart,
                                          true);
                    if (!f->SplitAndInitTextRun(aContext, aTextRun,
                                                convertedString.BeginReading(),
                                                aOffset + runStart, runLength,
                                                aScript)) {
                        ok = false;
                    }
                }
                break;
            }

            runStart = i;
        }

        if (i < aLength) {
            runCase = chCase;
        }
    }

    return ok;
}

already_AddRefed<gfxFont>
gfxFont::GetSmallCapsFont()
{
    gfxFontStyle style(*GetStyle());
    style.size *= SMALL_CAPS_SCALE_FACTOR;
    style.smallCaps = false;
    gfxFontEntry* fe = GetFontEntry();
    bool needsBold = style.weight >= 600 && !fe->IsBold();
    return fe->FindOrMakeFont(&style, needsBold);
}

gfxTextRun *
gfxFontGroup::GetEllipsisTextRun(int32_t aAppUnitsPerDevPixel,
                                 LazyReferenceContextGetter& aRefContextGetter)
{
    if (mCachedEllipsisTextRun &&
        mCachedEllipsisTextRun->GetAppUnitsPerDevUnit() == aAppUnitsPerDevPixel) {
        return mCachedEllipsisTextRun;
    }

    // Use a Unicode ellipsis if the font supports it,
    // otherwise use three ASCII periods as fallback.
    gfxFont* firstFont = GetFontAt(0);
    nsString ellipsis = firstFont->HasCharacter(kEllipsisChar[0])
        ? nsDependentString(kEllipsisChar,
                            ArrayLength(kEllipsisChar) - 1)
        : nsDependentString(kASCIIPeriodsChar,
                            ArrayLength(kASCIIPeriodsChar) - 1);

    nsRefPtr<gfxContext> refCtx = aRefContextGetter.GetRefContext();
    Parameters params = {
        refCtx, nullptr, nullptr, nullptr, 0, aAppUnitsPerDevPixel
    };
    gfxTextRun* textRun =
        MakeTextRun(ellipsis.get(), ellipsis.Length(), &params, TEXT_IS_PERSISTENT);
    if (!textRun) {
        return nullptr;
    }
    mCachedEllipsisTextRun = textRun;
    textRun->ReleaseFontGroup(); // don't let the presence of a cached ellipsis
                                 // textrun prolong the fontgroup's life
    return textRun;
}

already_AddRefed<gfxFont>
gfxFontGroup::TryAllFamilyMembers(gfxFontFamily* aFamily, uint32_t aCh)
{
    if (!aFamily->TestCharacterMap(aCh)) {
        return nullptr;
    }

    // Note that we don't need the actual runScript in matchData for
    // gfxFontFamily::SearchAllFontsForChar, it's only used for the
    // system-fallback case. So we can just set it to 0 here.
    GlobalFontMatch matchData(aCh, 0, &mStyle);
    aFamily->SearchAllFontsForChar(&matchData);
    gfxFontEntry *fe = matchData.mBestMatch;
    if (!fe) {
        return nullptr;
    }

    bool needsBold = mStyle.weight >= 600 && !fe->IsBold();
    nsRefPtr<gfxFont> font = fe->FindOrMakeFont(&mStyle, needsBold);
    return font.forget();
}

already_AddRefed<gfxFont>
gfxFontGroup::FindFontForChar(uint32_t aCh, uint32_t aPrevCh,
                              int32_t aRunScript, gfxFont *aPrevMatchedFont,
                              uint8_t *aMatchType)
{
    // To optimize common cases, try the first font in the font-group
    // before going into the more detailed checks below
    uint32_t nextIndex = 0;
    bool isJoinControl = gfxFontUtils::IsJoinControl(aCh);
    bool wasJoinCauser = gfxFontUtils::IsJoinCauser(aPrevCh);
    bool isVarSelector = gfxFontUtils::IsVarSelector(aCh);

    if (!isJoinControl && !wasJoinCauser && !isVarSelector) {
        nsRefPtr<gfxFont> firstFont = mFonts[0].Font();
        if (firstFont->HasCharacter(aCh)) {
            *aMatchType = gfxTextRange::kFontGroup;
            return firstFont.forget();
        }
        // It's possible that another font in the family (e.g. regular face,
        // where the requested style was italic) will support the character
        nsRefPtr<gfxFont> font = TryAllFamilyMembers(mFonts[0].Family(), aCh);
        if (font) {
            *aMatchType = gfxTextRange::kFontGroup;
            return font.forget();
        }
        // we don't need to check the first font again below
        ++nextIndex;
    }

    if (aPrevMatchedFont) {
        // Don't switch fonts for control characters, regardless of
        // whether they are present in the current font, as they won't
        // actually be rendered (see bug 716229)
        if (isJoinControl ||
            GetGeneralCategory(aCh) == HB_UNICODE_GENERAL_CATEGORY_CONTROL) {
            nsRefPtr<gfxFont> ret = aPrevMatchedFont;
            return ret.forget();
        }

        // if previous character was a join-causer (ZWJ),
        // use the same font as the previous range if we can
        if (wasJoinCauser) {
            if (aPrevMatchedFont->HasCharacter(aCh)) {
                nsRefPtr<gfxFont> ret = aPrevMatchedFont;
                return ret.forget();
            }
        }
    }

    // if this character is a variation selector,
    // use the previous font regardless of whether it supports VS or not.
    // otherwise the text run will be divided.
    if (isVarSelector) {
        if (aPrevMatchedFont) {
            nsRefPtr<gfxFont> ret = aPrevMatchedFont;
            return ret.forget();
        }
        // VS alone. it's meaningless to search different fonts
        return nullptr;
    }

    // 1. check remaining fonts in the font group
    uint32_t fontListLength = FontListLength();
    for (uint32_t i = nextIndex; i < fontListLength; i++) {
        nsRefPtr<gfxFont> font = mFonts[i].Font();
        if (font->HasCharacter(aCh)) {
            *aMatchType = gfxTextRange::kFontGroup;
            return font.forget();
        }

        font = TryAllFamilyMembers(mFonts[i].Family(), aCh);
        if (font) {
            *aMatchType = gfxTextRange::kFontGroup;
            return font.forget();
        }
    }

    // if character is in Private Use Area, don't do matching against pref or system fonts
    if ((aCh >= 0xE000  && aCh <= 0xF8FF) || (aCh >= 0xF0000 && aCh <= 0x10FFFD))
        return nullptr;

    // 2. search pref fonts
    nsRefPtr<gfxFont> font = WhichPrefFontSupportsChar(aCh);
    if (font) {
        *aMatchType = gfxTextRange::kPrefsFallback;
        return font.forget();
    }

    // 3. use fallback fonts
    // -- before searching for something else check the font used for the previous character
    if (aPrevMatchedFont && aPrevMatchedFont->HasCharacter(aCh)) {
        *aMatchType = gfxTextRange::kSystemFallback;
        nsRefPtr<gfxFont> ret = aPrevMatchedFont;
        return ret.forget();
    }

    // never fall back for characters from unknown scripts
    if (aRunScript == HB_SCRIPT_UNKNOWN) {
        return nullptr;
    }

    // for known "space" characters, don't do a full system-fallback search;
    // we'll synthesize appropriate-width spaces instead of missing-glyph boxes
    if (GetGeneralCategory(aCh) ==
            HB_UNICODE_GENERAL_CATEGORY_SPACE_SEPARATOR &&
        GetFontAt(0)->SynthesizeSpaceWidth(aCh) >= 0.0)
    {
        return nullptr;
    }

    // -- otherwise look for other stuff
    *aMatchType = gfxTextRange::kSystemFallback;
    font = WhichSystemFontSupportsChar(aCh, aRunScript);
    return font.forget();
}

template<typename T>
void gfxFontGroup::ComputeRanges(nsTArray<gfxTextRange>& aRanges,
                                 const T *aString, uint32_t aLength,
                                 int32_t aRunScript)
{
    NS_ASSERTION(aRanges.Length() == 0, "aRanges must be initially empty");
    NS_ASSERTION(aLength > 0, "don't call ComputeRanges for zero-length text");

    uint32_t prevCh = 0;
    int32_t lastRangeIndex = -1;

    // initialize prevFont to the group's primary font, so that this will be
    // used for string-initial control chars, etc rather than risk hitting font
    // fallback for these (bug 716229)
    gfxFont *prevFont = GetFontAt(0);

    // if we use the initial value of prevFont, we treat this as a match from
    // the font group; fixes bug 978313
    uint8_t matchType = gfxTextRange::kFontGroup;

    for (uint32_t i = 0; i < aLength; i++) {

        const uint32_t origI = i; // save off in case we increase for surrogate

        // set up current ch
        uint32_t ch = aString[i];

        // in 16-bit case only, check for surrogate pair
        if (sizeof(T) == sizeof(char16_t)) {
            if ((i + 1 < aLength) && NS_IS_HIGH_SURROGATE(ch) &&
                                 NS_IS_LOW_SURROGATE(aString[i + 1])) {
                i++;
                ch = SURROGATE_TO_UCS4(ch, aString[i]);
            }
        }

        if (ch == 0xa0) {
            ch = ' ';
        }

        // find the font for this char
        nsRefPtr<gfxFont> font =
            FindFontForChar(ch, prevCh, aRunScript, prevFont, &matchType);

#ifndef RELEASE_BUILD
        if (MOZ_UNLIKELY(mTextPerf)) {
            if (matchType == gfxTextRange::kPrefsFallback) {
                mTextPerf->current.fallbackPrefs++;
            } else if (matchType == gfxTextRange::kSystemFallback) {
                mTextPerf->current.fallbackSystem++;
            }
        }
#endif

        prevCh = ch;

        if (lastRangeIndex == -1) {
            // first char ==> make a new range
            aRanges.AppendElement(gfxTextRange(0, 1, font, matchType));
            lastRangeIndex++;
            prevFont = font;
        } else {
            // if font has changed, make a new range
            gfxTextRange& prevRange = aRanges[lastRangeIndex];
            if (prevRange.font != font || prevRange.matchType != matchType) {
                // close out the previous range
                prevRange.end = origI;
                aRanges.AppendElement(gfxTextRange(origI, i + 1,
                                                   font, matchType));
                lastRangeIndex++;

                // update prevFont for the next match, *unless* we switched
                // fonts on a ZWJ, in which case propagating the changed font
                // is probably not a good idea (see bug 619511)
                if (sizeof(T) == sizeof(uint8_t) ||
                    !gfxFontUtils::IsJoinCauser(ch))
                {
                    prevFont = font;
                }
            }
        }
    }

    aRanges[lastRangeIndex].end = aLength;
}

gfxUserFontSet* 
gfxFontGroup::GetUserFontSet()
{
    return mUserFontSet;
}

void 
gfxFontGroup::SetUserFontSet(gfxUserFontSet *aUserFontSet)
{
    if (aUserFontSet == mUserFontSet) {
        return;
    }
    mUserFontSet = aUserFontSet;
    mCurrGeneration = GetGeneration() - 1;
    UpdateFontList();
}

uint64_t
gfxFontGroup::GetGeneration()
{
    if (!mUserFontSet)
        return 0;
    return mUserFontSet->GetGeneration();
}

// note: gfxPangoFontGroup overrides UpdateFontList, such that
//       BuildFontList is never used
void
gfxFontGroup::UpdateFontList()
{
    if (mCurrGeneration != GetGeneration()) {
        // xxx - can probably improve this to detect when all fonts were found, so no need to update list
        mFonts.Clear();
        mUnderlineOffset = UNDERLINE_OFFSET_NOT_SET;
        mSkipDrawing = false;
        BuildFontList();
        mCurrGeneration = GetGeneration();
        mCachedEllipsisTextRun = nullptr;
    }
}

struct PrefFontCallbackData {
    PrefFontCallbackData(nsTArray<nsRefPtr<gfxFontFamily> >& aFamiliesArray)
        : mPrefFamilies(aFamiliesArray)
    {}

    nsTArray<nsRefPtr<gfxFontFamily> >& mPrefFamilies;

    static bool AddFontFamilyEntry(eFontPrefLang aLang, const nsAString& aName, void *aClosure)
    {
        PrefFontCallbackData *prefFontData = static_cast<PrefFontCallbackData*>(aClosure);

        gfxFontFamily *family = gfxPlatformFontList::PlatformFontList()->FindFamily(aName);
        if (family) {
            prefFontData->mPrefFamilies.AppendElement(family);
        }
        return true;
    }
};

already_AddRefed<gfxFont>
gfxFontGroup::WhichPrefFontSupportsChar(uint32_t aCh)
{
    nsRefPtr<gfxFont> font;

    // get the pref font list if it hasn't been set up already
    uint32_t unicodeRange = FindCharUnicodeRange(aCh);
    eFontPrefLang charLang = gfxPlatform::GetPlatform()->GetFontPrefLangFor(unicodeRange);

    // if the last pref font was the first family in the pref list, no need to recheck through a list of families
    if (mLastPrefFont && charLang == mLastPrefLang &&
        mLastPrefFirstFont && mLastPrefFont->HasCharacter(aCh)) {
        font = mLastPrefFont;
        return font.forget();
    }

    // based on char lang and page lang, set up list of pref lang fonts to check
    eFontPrefLang prefLangs[kMaxLenPrefLangList];
    uint32_t i, numLangs = 0;

    gfxPlatform::GetPlatform()->GetLangPrefs(prefLangs, numLangs, charLang, mPageLang);

    for (i = 0; i < numLangs; i++) {
        nsAutoTArray<nsRefPtr<gfxFontFamily>, 5> families;
        eFontPrefLang currentLang = prefLangs[i];

        gfxPlatformFontList *fontList = gfxPlatformFontList::PlatformFontList();

        // get the pref families for a single pref lang
        if (!fontList->GetPrefFontFamilyEntries(currentLang, &families)) {
            eFontPrefLang prefLangsToSearch[1] = { currentLang };
            PrefFontCallbackData prefFontData(families);
            gfxPlatform::ForEachPrefFont(prefLangsToSearch, 1, PrefFontCallbackData::AddFontFamilyEntry,
                                           &prefFontData);
            fontList->SetPrefFontFamilyEntries(currentLang, families);
        }

        // find the first pref font that includes the character
        uint32_t  j, numPrefs;
        numPrefs = families.Length();
        for (j = 0; j < numPrefs; j++) {
            // look up the appropriate face
            gfxFontFamily *family = families[j];
            if (!family) continue;

            // if a pref font is used, it's likely to be used again in the same text run.
            // the style doesn't change so the face lookup can be cached rather than calling
            // FindOrMakeFont repeatedly.  speeds up FindFontForChar lookup times for subsequent
            // pref font lookups
            if (family == mLastPrefFamily && mLastPrefFont->HasCharacter(aCh)) {
                font = mLastPrefFont;
                return font.forget();
            }

            bool needsBold;
            gfxFontEntry *fe = family->FindFontForStyle(mStyle, needsBold);
            // if ch in cmap, create and return a gfxFont
            if (fe && fe->TestCharacterMap(aCh)) {
                nsRefPtr<gfxFont> prefFont = fe->FindOrMakeFont(&mStyle, needsBold);
                if (!prefFont) continue;
                mLastPrefFamily = family;
                mLastPrefFont = prefFont;
                mLastPrefLang = charLang;
                mLastPrefFirstFont = (i == 0 && j == 0);
                return prefFont.forget();
            }

        }
    }

    return nullptr;
}

already_AddRefed<gfxFont>
gfxFontGroup::WhichSystemFontSupportsChar(uint32_t aCh, int32_t aRunScript)
{
    gfxFontEntry *fe = 
        gfxPlatformFontList::PlatformFontList()->
            SystemFindFontForChar(aCh, aRunScript, &mStyle);
    if (fe) {
        bool wantBold = mStyle.ComputeWeight() >= 6;
        nsRefPtr<gfxFont> font =
            fe->FindOrMakeFont(&mStyle, wantBold && !fe->IsBold());
        return font.forget();
    }

    return nullptr;
}

/*static*/ void
gfxFontGroup::Shutdown()
{
    NS_IF_RELEASE(gLangService);
}

nsILanguageAtomService* gfxFontGroup::gLangService = nullptr;


#define DEFAULT_PIXEL_FONT_SIZE 16.0f

/*static*/ uint32_t
gfxFontStyle::ParseFontLanguageOverride(const nsString& aLangTag)
{
  if (!aLangTag.Length() || aLangTag.Length() > 4) {
    return NO_FONT_LANGUAGE_OVERRIDE;
  }
  uint32_t index, result = 0;
  for (index = 0; index < aLangTag.Length(); ++index) {
    char16_t ch = aLangTag[index];
    if (!nsCRT::IsAscii(ch)) { // valid tags are pure ASCII
      return NO_FONT_LANGUAGE_OVERRIDE;
    }
    result = (result << 8) + ch;
  }
  while (index++ < 4) {
    result = (result << 8) + 0x20;
  }
  return result;
}

gfxFontStyle::gfxFontStyle() :
    language(nsGkAtoms::x_western),
    size(DEFAULT_PIXEL_FONT_SIZE), sizeAdjust(0.0f),
    languageOverride(NO_FONT_LANGUAGE_OVERRIDE),
    weight(NS_FONT_WEIGHT_NORMAL), stretch(NS_FONT_STRETCH_NORMAL),
    systemFont(true), printerFont(false), useGrayscaleAntialiasing(false),
    smallCaps(false),
    style(NS_FONT_STYLE_NORMAL)
{
}

gfxFontStyle::gfxFontStyle(uint8_t aStyle, uint16_t aWeight, int16_t aStretch,
                           gfxFloat aSize, nsIAtom *aLanguage,
                           float aSizeAdjust, bool aSystemFont,
                           bool aPrinterFont, bool aSmallCaps,
                           const nsString& aLanguageOverride):
    language(aLanguage),
    size(aSize), sizeAdjust(aSizeAdjust),
    languageOverride(ParseFontLanguageOverride(aLanguageOverride)),
    weight(aWeight), stretch(aStretch),
    systemFont(aSystemFont), printerFont(aPrinterFont),
    useGrayscaleAntialiasing(false),
    smallCaps(aSmallCaps),
    style(aStyle)
{
    MOZ_ASSERT(!mozilla::IsNaN(size));
    MOZ_ASSERT(!mozilla::IsNaN(sizeAdjust));

    if (weight > 900)
        weight = 900;
    if (weight < 100)
        weight = 100;

    if (size >= FONT_MAX_SIZE) {
        size = FONT_MAX_SIZE;
        sizeAdjust = 0.0;
    } else if (size < 0.0) {
        NS_WARNING("negative font size");
        size = 0.0;
    }

    if (!language) {
        NS_WARNING("null language");
        language = nsGkAtoms::x_western;
    }
}

gfxFontStyle::gfxFontStyle(const gfxFontStyle& aStyle) :
    language(aStyle.language),
    featureValueLookup(aStyle.featureValueLookup),
    size(aStyle.size), sizeAdjust(aStyle.sizeAdjust),
    languageOverride(aStyle.languageOverride),
    weight(aStyle.weight), stretch(aStyle.stretch),
    systemFont(aStyle.systemFont), printerFont(aStyle.printerFont),
    useGrayscaleAntialiasing(aStyle.useGrayscaleAntialiasing),
    smallCaps(aStyle.smallCaps),
    style(aStyle.style)
{
    featureSettings.AppendElements(aStyle.featureSettings);
    alternateValues.AppendElements(aStyle.alternateValues);
}

int8_t
gfxFontStyle::ComputeWeight() const
{
    int8_t baseWeight = (weight + 50) / 100;

    if (baseWeight < 0)
        baseWeight = 0;
    if (baseWeight > 9)
        baseWeight = 9;

    return baseWeight;
}

void
gfxShapedText::SetupClusterBoundaries(uint32_t         aOffset,
                                      const char16_t *aString,
                                      uint32_t         aLength)
{
    CompressedGlyph *glyphs = GetCharacterGlyphs() + aOffset;

    gfxTextRun::CompressedGlyph extendCluster;
    extendCluster.SetComplex(false, true, 0);

    ClusterIterator iter(aString, aLength);

    // the ClusterIterator won't be able to tell us if the string
    // _begins_ with a cluster-extender, so we handle that here
    if (aLength && IsClusterExtender(*aString)) {
        *glyphs = extendCluster;
    }

    while (!iter.AtEnd()) {
        if (*iter == char16_t(' ')) {
            glyphs->SetIsSpace();
        }
        // advance iter to the next cluster-start (or end of text)
        iter.Next();
        // step past the first char of the cluster
        aString++;
        glyphs++;
        // mark all the rest as cluster-continuations
        while (aString < iter) {
            *glyphs = extendCluster;
            if (NS_IS_LOW_SURROGATE(*aString)) {
                glyphs->SetIsLowSurrogate();
            }
            glyphs++;
            aString++;
        }
    }
}

void
gfxShapedText::SetupClusterBoundaries(uint32_t       aOffset,
                                      const uint8_t *aString,
                                      uint32_t       aLength)
{
    CompressedGlyph *glyphs = GetCharacterGlyphs() + aOffset;
    const uint8_t *limit = aString + aLength;

    while (aString < limit) {
        if (*aString == uint8_t(' ')) {
            glyphs->SetIsSpace();
        }
        aString++;
        glyphs++;
    }
}

gfxShapedText::DetailedGlyph *
gfxShapedText::AllocateDetailedGlyphs(uint32_t aIndex, uint32_t aCount)
{
    NS_ASSERTION(aIndex < GetLength(), "Index out of range");

    if (!mDetailedGlyphs) {
        mDetailedGlyphs = new DetailedGlyphStore();
    }

    DetailedGlyph *details = mDetailedGlyphs->Allocate(aIndex, aCount);
    if (!details) {
        GetCharacterGlyphs()[aIndex].SetMissing(0);
        return nullptr;
    }

    return details;
}

void
gfxShapedText::SetGlyphs(uint32_t aIndex, CompressedGlyph aGlyph,
                         const DetailedGlyph *aGlyphs)
{
    NS_ASSERTION(!aGlyph.IsSimpleGlyph(), "Simple glyphs not handled here");
    NS_ASSERTION(aIndex > 0 || aGlyph.IsLigatureGroupStart(),
                 "First character can't be a ligature continuation!");

    uint32_t glyphCount = aGlyph.GetGlyphCount();
    if (glyphCount > 0) {
        DetailedGlyph *details = AllocateDetailedGlyphs(aIndex, glyphCount);
        if (!details) {
            return;
        }
        memcpy(details, aGlyphs, sizeof(DetailedGlyph)*glyphCount);
    }
    GetCharacterGlyphs()[aIndex] = aGlyph;
}

#define ZWNJ 0x200C
#define ZWJ  0x200D
// U+061C ARABIC LETTER MARK is expected to be added to XIDMOD_DEFAULT_IGNORABLE
// in a future Unicode update. Add it manually for now
#define ALM  0x061C
static inline bool
IsDefaultIgnorable(uint32_t aChar)
{
    return GetIdentifierModification(aChar) == XIDMOD_DEFAULT_IGNORABLE ||
           aChar == ZWNJ || aChar == ZWJ || aChar == ALM;
}

void
gfxShapedText::SetMissingGlyph(uint32_t aIndex, uint32_t aChar, gfxFont *aFont)
{
    uint8_t category = GetGeneralCategory(aChar);
    if (category >= HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK &&
        category <= HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK)
    {
        GetCharacterGlyphs()[aIndex].SetComplex(false, true, 0);
    }

    DetailedGlyph *details = AllocateDetailedGlyphs(aIndex, 1);
    if (!details) {
        return;
    }

    details->mGlyphID = aChar;
    if (IsDefaultIgnorable(aChar)) {
        // Setting advance width to zero will prevent drawing the hexbox
        details->mAdvance = 0;
    } else {
        gfxFloat width =
            std::max(aFont->GetMetrics().aveCharWidth,
                     gfxFontMissingGlyphs::GetDesiredMinWidth(aChar,
                         mAppUnitsPerDevUnit));
        details->mAdvance = uint32_t(width * mAppUnitsPerDevUnit);
    }
    details->mXOffset = 0;
    details->mYOffset = 0;
    GetCharacterGlyphs()[aIndex].SetMissing(1);
}

bool
gfxShapedText::FilterIfIgnorable(uint32_t aIndex, uint32_t aCh)
{
    if (IsDefaultIgnorable(aCh)) {
        DetailedGlyph *details = AllocateDetailedGlyphs(aIndex, 1);
        if (details) {
            details->mGlyphID = aCh;
            details->mAdvance = 0;
            details->mXOffset = 0;
            details->mYOffset = 0;
            GetCharacterGlyphs()[aIndex].SetMissing(1);
            return true;
        }
    }
    return false;
}

void
gfxShapedText::AdjustAdvancesForSyntheticBold(float aSynBoldOffset,
                                              uint32_t aOffset,
                                              uint32_t aLength)
{
    uint32_t synAppUnitOffset = aSynBoldOffset * mAppUnitsPerDevUnit;
    CompressedGlyph *charGlyphs = GetCharacterGlyphs();
    for (uint32_t i = aOffset; i < aOffset + aLength; ++i) {
         CompressedGlyph *glyphData = charGlyphs + i;
         if (glyphData->IsSimpleGlyph()) {
             // simple glyphs ==> just add the advance
             int32_t advance = glyphData->GetSimpleAdvance() + synAppUnitOffset;
             if (CompressedGlyph::IsSimpleAdvance(advance)) {
                 glyphData->SetSimpleGlyph(advance, glyphData->GetSimpleGlyph());
             } else {
                 // rare case, tested by making this the default
                 uint32_t glyphIndex = glyphData->GetSimpleGlyph();
                 glyphData->SetComplex(true, true, 1);
                 DetailedGlyph detail = {glyphIndex, advance, 0, 0};
                 SetGlyphs(i, *glyphData, &detail);
             }
         } else {
             // complex glyphs ==> add offset at cluster/ligature boundaries
             uint32_t detailedLength = glyphData->GetGlyphCount();
             if (detailedLength) {
                 DetailedGlyph *details = GetDetailedGlyphs(i);
                 if (!details) {
                     continue;
                 }
                 if (IsRightToLeft()) {
                     details[0].mAdvance += synAppUnitOffset;
                 } else {
                     details[detailedLength - 1].mAdvance += synAppUnitOffset;
                 }
             }
         }
    }
}

bool
gfxTextRun::GlyphRunIterator::NextRun()  {
    if (mNextIndex >= mTextRun->mGlyphRuns.Length())
        return false;
    mGlyphRun = &mTextRun->mGlyphRuns[mNextIndex];
    if (mGlyphRun->mCharacterOffset >= mEndOffset)
        return false;

    mStringStart = std::max(mStartOffset, mGlyphRun->mCharacterOffset);
    uint32_t last = mNextIndex + 1 < mTextRun->mGlyphRuns.Length()
        ? mTextRun->mGlyphRuns[mNextIndex + 1].mCharacterOffset : mTextRun->GetLength();
    mStringEnd = std::min(mEndOffset, last);

    ++mNextIndex;
    return true;
}

#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
static void
AccountStorageForTextRun(gfxTextRun *aTextRun, int32_t aSign)
{
    // Ignores detailed glyphs... we don't know when those have been constructed
    // Also ignores gfxSkipChars dynamic storage (which won't be anything
    // for preformatted text)
    // Also ignores GlyphRun array, again because it hasn't been constructed
    // by the time this gets called. If there's only one glyphrun that's stored
    // directly in the textrun anyway so no additional overhead.
    uint32_t length = aTextRun->GetLength();
    int32_t bytes = length * sizeof(gfxTextRun::CompressedGlyph);
    bytes += sizeof(gfxTextRun);
    gTextRunStorage += bytes*aSign;
    gTextRunStorageHighWaterMark = std::max(gTextRunStorageHighWaterMark, gTextRunStorage);
}
#endif

// Helper for textRun creation to preallocate storage for glyph records;
// this function returns a pointer to the newly-allocated glyph storage.
// Returns nullptr if allocation fails.
void *
gfxTextRun::AllocateStorageForTextRun(size_t aSize, uint32_t aLength)
{
    // Allocate the storage we need, returning nullptr on failure rather than
    // throwing an exception (because web content can create huge runs).
    void *storage = moz_malloc(aSize + aLength * sizeof(CompressedGlyph));
    if (!storage) {
        NS_WARNING("failed to allocate storage for text run!");
        return nullptr;
    }

    // Initialize the glyph storage (beyond aSize) to zero
    memset(reinterpret_cast<char*>(storage) + aSize, 0,
           aLength * sizeof(CompressedGlyph));

    return storage;
}

gfxTextRun *
gfxTextRun::Create(const gfxTextRunFactory::Parameters *aParams,
                   uint32_t aLength, gfxFontGroup *aFontGroup, uint32_t aFlags)
{
    void *storage = AllocateStorageForTextRun(sizeof(gfxTextRun), aLength);
    if (!storage) {
        return nullptr;
    }

    return new (storage) gfxTextRun(aParams, aLength, aFontGroup, aFlags);
}

gfxTextRun::gfxTextRun(const gfxTextRunFactory::Parameters *aParams,
                       uint32_t aLength, gfxFontGroup *aFontGroup, uint32_t aFlags)
    : gfxShapedText(aLength, aFlags, aParams->mAppUnitsPerDevUnit)
    , mUserData(aParams->mUserData)
    , mFontGroup(aFontGroup)
    , mReleasedFontGroup(false)
{
    NS_ASSERTION(mAppUnitsPerDevUnit > 0, "Invalid app unit scale");
    MOZ_COUNT_CTOR(gfxTextRun);
    NS_ADDREF(mFontGroup);

#ifndef RELEASE_BUILD
    gfxTextPerfMetrics *tp = aFontGroup->GetTextPerfMetrics();
    if (tp) {
        tp->current.textrunConst++;
    }
#endif

    mCharacterGlyphs = reinterpret_cast<CompressedGlyph*>(this + 1);

    if (aParams->mSkipChars) {
        mSkipChars.TakeFrom(aParams->mSkipChars);
    }

#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
    AccountStorageForTextRun(this, 1);
#endif

    mSkipDrawing = mFontGroup->ShouldSkipDrawing();
}

gfxTextRun::~gfxTextRun()
{
#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
    AccountStorageForTextRun(this, -1);
#endif
#ifdef DEBUG
    // Make it easy to detect a dead text run
    mFlags = 0xFFFFFFFF;
#endif

    // The cached ellipsis textrun (if any) in a fontgroup will have already
    // been told to release its reference to the group, so we mustn't do that
    // again here.
    if (!mReleasedFontGroup) {
#ifndef RELEASE_BUILD
        gfxTextPerfMetrics *tp = mFontGroup->GetTextPerfMetrics();
        if (tp) {
            tp->current.textrunDestr++;
        }
#endif
        NS_RELEASE(mFontGroup);
    }

    MOZ_COUNT_DTOR(gfxTextRun);
}

void
gfxTextRun::ReleaseFontGroup()
{
    NS_ASSERTION(!mReleasedFontGroup, "doubly released!");
    NS_RELEASE(mFontGroup);
    mReleasedFontGroup = true;
}

bool
gfxTextRun::SetPotentialLineBreaks(uint32_t aStart, uint32_t aLength,
                                   uint8_t *aBreakBefore,
                                   gfxContext *aRefContext)
{
    NS_ASSERTION(aStart + aLength <= GetLength(), "Overflow");

    uint32_t changed = 0;
    uint32_t i;
    CompressedGlyph *charGlyphs = mCharacterGlyphs + aStart;
    for (i = 0; i < aLength; ++i) {
        uint8_t canBreak = aBreakBefore[i];
        if (canBreak && !charGlyphs[i].IsClusterStart()) {
            // This can happen ... there is no guarantee that our linebreaking rules
            // align with the platform's idea of what constitutes a cluster.
            NS_WARNING("Break suggested inside cluster!");
            canBreak = CompressedGlyph::FLAG_BREAK_TYPE_NONE;
        }
        changed |= charGlyphs[i].SetCanBreakBefore(canBreak);
    }
    return changed != 0;
}

gfxTextRun::LigatureData
gfxTextRun::ComputeLigatureData(uint32_t aPartStart, uint32_t aPartEnd,
                                PropertyProvider *aProvider)
{
    NS_ASSERTION(aPartStart < aPartEnd, "Computing ligature data for empty range");
    NS_ASSERTION(aPartEnd <= GetLength(), "Character length overflow");
  
    LigatureData result;
    CompressedGlyph *charGlyphs = mCharacterGlyphs;

    uint32_t i;
    for (i = aPartStart; !charGlyphs[i].IsLigatureGroupStart(); --i) {
        NS_ASSERTION(i > 0, "Ligature at the start of the run??");
    }
    result.mLigatureStart = i;
    for (i = aPartStart + 1; i < GetLength() && !charGlyphs[i].IsLigatureGroupStart(); ++i) {
    }
    result.mLigatureEnd = i;

    int32_t ligatureWidth =
        GetAdvanceForGlyphs(result.mLigatureStart, result.mLigatureEnd);
    // Count the number of started clusters we have seen
    uint32_t totalClusterCount = 0;
    uint32_t partClusterIndex = 0;
    uint32_t partClusterCount = 0;
    for (i = result.mLigatureStart; i < result.mLigatureEnd; ++i) {
        // Treat the first character of the ligature as the start of a
        // cluster for our purposes of allocating ligature width to its
        // characters.
        if (i == result.mLigatureStart || charGlyphs[i].IsClusterStart()) {
            ++totalClusterCount;
            if (i < aPartStart) {
                ++partClusterIndex;
            } else if (i < aPartEnd) {
                ++partClusterCount;
            }
        }
    }
    NS_ASSERTION(totalClusterCount > 0, "Ligature involving no clusters??");
    result.mPartAdvance = partClusterIndex * (ligatureWidth / totalClusterCount);
    result.mPartWidth = partClusterCount * (ligatureWidth / totalClusterCount);

    // Any rounding errors are apportioned to the final part of the ligature,
    // so that measuring all parts of a ligature and summing them is equal to
    // the ligature width.
    if (aPartEnd == result.mLigatureEnd) {
        gfxFloat allParts = totalClusterCount * (ligatureWidth / totalClusterCount);
        result.mPartWidth += ligatureWidth - allParts;
    }

    if (partClusterCount == 0) {
        // nothing to draw
        result.mClipBeforePart = result.mClipAfterPart = true;
    } else {
        // Determine whether we should clip before or after this part when
        // drawing its slice of the ligature.
        // We need to clip before the part if any cluster is drawn before
        // this part.
        result.mClipBeforePart = partClusterIndex > 0;
        // We need to clip after the part if any cluster is drawn after
        // this part.
        result.mClipAfterPart = partClusterIndex + partClusterCount < totalClusterCount;
    }

    if (aProvider && (mFlags & gfxTextRunFactory::TEXT_ENABLE_SPACING)) {
        gfxFont::Spacing spacing;
        if (aPartStart == result.mLigatureStart) {
            aProvider->GetSpacing(aPartStart, 1, &spacing);
            result.mPartWidth += spacing.mBefore;
        }
        if (aPartEnd == result.mLigatureEnd) {
            aProvider->GetSpacing(aPartEnd - 1, 1, &spacing);
            result.mPartWidth += spacing.mAfter;
        }
    }

    return result;
}

gfxFloat
gfxTextRun::ComputePartialLigatureWidth(uint32_t aPartStart, uint32_t aPartEnd,
                                        PropertyProvider *aProvider)
{
    if (aPartStart >= aPartEnd)
        return 0;
    LigatureData data = ComputeLigatureData(aPartStart, aPartEnd, aProvider);
    return data.mPartWidth;
}

int32_t
gfxTextRun::GetAdvanceForGlyphs(uint32_t aStart, uint32_t aEnd)
{
    const CompressedGlyph *glyphData = mCharacterGlyphs + aStart;
    int32_t advance = 0;
    uint32_t i;
    for (i = aStart; i < aEnd; ++i, ++glyphData) {
        if (glyphData->IsSimpleGlyph()) {
            advance += glyphData->GetSimpleAdvance();   
        } else {
            uint32_t glyphCount = glyphData->GetGlyphCount();
            if (glyphCount == 0) {
                continue;
            }
            const DetailedGlyph *details = GetDetailedGlyphs(i);
            if (details) {
                uint32_t j;
                for (j = 0; j < glyphCount; ++j, ++details) {
                    advance += details->mAdvance;
                }
            }
        }
    }
    return advance;
}

static void
GetAdjustedSpacing(gfxTextRun *aTextRun, uint32_t aStart, uint32_t aEnd,
                   gfxTextRun::PropertyProvider *aProvider,
                   gfxTextRun::PropertyProvider::Spacing *aSpacing)
{
    if (aStart >= aEnd)
        return;

    aProvider->GetSpacing(aStart, aEnd - aStart, aSpacing);

#ifdef DEBUG
    // Check to see if we have spacing inside ligatures

    const gfxTextRun::CompressedGlyph *charGlyphs = aTextRun->GetCharacterGlyphs();
    uint32_t i;

    for (i = aStart; i < aEnd; ++i) {
        if (!charGlyphs[i].IsLigatureGroupStart()) {
            NS_ASSERTION(i == aStart || aSpacing[i - aStart].mBefore == 0,
                         "Before-spacing inside a ligature!");
            NS_ASSERTION(i - 1 <= aStart || aSpacing[i - 1 - aStart].mAfter == 0,
                         "After-spacing inside a ligature!");
        }
    }
#endif
}

bool
gfxTextRun::GetAdjustedSpacingArray(uint32_t aStart, uint32_t aEnd,
                                    PropertyProvider *aProvider,
                                    uint32_t aSpacingStart, uint32_t aSpacingEnd,
                                    nsTArray<PropertyProvider::Spacing> *aSpacing)
{
    if (!aProvider || !(mFlags & gfxTextRunFactory::TEXT_ENABLE_SPACING))
        return false;
    if (!aSpacing->AppendElements(aEnd - aStart))
        return false;
    memset(aSpacing->Elements(), 0, sizeof(gfxFont::Spacing)*(aSpacingStart - aStart));
    GetAdjustedSpacing(this, aSpacingStart, aSpacingEnd, aProvider,
                       aSpacing->Elements() + aSpacingStart - aStart);
    memset(aSpacing->Elements() + aSpacingEnd - aStart, 0, sizeof(gfxFont::Spacing)*(aEnd - aSpacingEnd));
    return true;
}

void
gfxTextRun::ShrinkToLigatureBoundaries(uint32_t *aStart, uint32_t *aEnd)
{
    if (*aStart >= *aEnd)
        return;
  
    CompressedGlyph *charGlyphs = mCharacterGlyphs;

    while (*aStart < *aEnd && !charGlyphs[*aStart].IsLigatureGroupStart()) {
        ++(*aStart);
    }
    if (*aEnd < GetLength()) {
        while (*aEnd > *aStart && !charGlyphs[*aEnd].IsLigatureGroupStart()) {
            --(*aEnd);
        }
    }
}

void
gfxTextRun::DrawGlyphs(gfxFont *aFont, gfxContext *aContext,
                       DrawMode aDrawMode, gfxPoint *aPt,
                       gfxTextContextPaint *aContextPaint,
                       uint32_t aStart, uint32_t aEnd,
                       PropertyProvider *aProvider,
                       uint32_t aSpacingStart, uint32_t aSpacingEnd,
                       gfxTextRunDrawCallbacks *aCallbacks)
{
    nsAutoTArray<PropertyProvider::Spacing,200> spacingBuffer;
    bool haveSpacing = GetAdjustedSpacingArray(aStart, aEnd, aProvider,
        aSpacingStart, aSpacingEnd, &spacingBuffer);
    aFont->Draw(this, aStart, aEnd, aContext, aDrawMode, aPt,
                haveSpacing ? spacingBuffer.Elements() : nullptr, aContextPaint,
                aCallbacks);
}

static void
ClipPartialLigature(gfxTextRun *aTextRun, gfxFloat *aLeft, gfxFloat *aRight,
                    gfxFloat aXOrigin, gfxTextRun::LigatureData *aLigature)
{
    if (aLigature->mClipBeforePart) {
        if (aTextRun->IsRightToLeft()) {
            *aRight = std::min(*aRight, aXOrigin);
        } else {
            *aLeft = std::max(*aLeft, aXOrigin);
        }
    }
    if (aLigature->mClipAfterPart) {
        gfxFloat endEdge = aXOrigin + aTextRun->GetDirection()*aLigature->mPartWidth;
        if (aTextRun->IsRightToLeft()) {
            *aLeft = std::max(*aLeft, endEdge);
        } else {
            *aRight = std::min(*aRight, endEdge);
        }
    }    
}

void
gfxTextRun::DrawPartialLigature(gfxFont *aFont, gfxContext *aCtx,
                                uint32_t aStart, uint32_t aEnd,
                                gfxPoint *aPt,
                                PropertyProvider *aProvider,
                                gfxTextRunDrawCallbacks *aCallbacks)
{
    if (aStart >= aEnd)
        return;

    // Draw partial ligature. We hack this by clipping the ligature.
    LigatureData data = ComputeLigatureData(aStart, aEnd, aProvider);
    gfxRect clipExtents = aCtx->GetClipExtents();
    gfxFloat left = clipExtents.X()*mAppUnitsPerDevUnit;
    gfxFloat right = clipExtents.XMost()*mAppUnitsPerDevUnit;
    ClipPartialLigature(this, &left, &right, aPt->x, &data);

    {
      // Need to preserve the path, otherwise this can break canvas text-on-path;
      // in general it seems like a good thing, as naive callers probably won't
      // expect gfxTextRun::Draw to implicitly destroy the current path.
      gfxContextPathAutoSaveRestore savePath(aCtx);

      // use division here to ensure that when the rect is aligned on multiples
      // of mAppUnitsPerDevUnit, we clip to true device unit boundaries.
      // Also, make sure we snap the rectangle to device pixels.
      aCtx->Save();
      aCtx->NewPath();
      aCtx->Rectangle(gfxRect(left / mAppUnitsPerDevUnit,
                              clipExtents.Y(),
                              (right - left) / mAppUnitsPerDevUnit,
                              clipExtents.Height()), true);
      aCtx->Clip();
    }

    gfxFloat direction = GetDirection();
    gfxPoint pt(aPt->x - direction*data.mPartAdvance, aPt->y);
    DrawGlyphs(aFont, aCtx,
               aCallbacks ? DrawMode::GLYPH_PATH : DrawMode::GLYPH_FILL, &pt,
               nullptr, data.mLigatureStart, data.mLigatureEnd, aProvider,
               aStart, aEnd, aCallbacks);
    aCtx->Restore();

    aPt->x += direction*data.mPartWidth;
}

// returns true if a glyph run is using a font with synthetic bolding enabled, false otherwise
static bool
HasSyntheticBold(gfxTextRun *aRun, uint32_t aStart, uint32_t aLength)
{
    gfxTextRun::GlyphRunIterator iter(aRun, aStart, aLength);
    while (iter.NextRun()) {
        gfxFont *font = iter.GetGlyphRun()->mFont;
        if (font && font->IsSyntheticBold()) {
            return true;
        }
    }

    return false;
}

// returns true if color is non-opaque (i.e. alpha != 1.0) or completely transparent, false otherwise
// if true, color is set on output
static bool
HasNonOpaqueColor(gfxContext *aContext, gfxRGBA& aCurrentColor)
{
    if (aContext->GetDeviceColor(aCurrentColor)) {
        if (aCurrentColor.a < 1.0 && aCurrentColor.a > 0.0) {
            return true;
        }
    }
        
    return false;
}

// helper class for double-buffering drawing with non-opaque color
struct BufferAlphaColor {
    BufferAlphaColor(gfxContext *aContext)
        : mContext(aContext)
    {

    }

    ~BufferAlphaColor() {}

    void PushSolidColor(const gfxRect& aBounds, const gfxRGBA& aAlphaColor, uint32_t appsPerDevUnit)
    {
        mContext->Save();
        mContext->NewPath();
        mContext->Rectangle(gfxRect(aBounds.X() / appsPerDevUnit,
                    aBounds.Y() / appsPerDevUnit,
                    aBounds.Width() / appsPerDevUnit,
                    aBounds.Height() / appsPerDevUnit), true);
        mContext->Clip();
        mContext->SetColor(gfxRGBA(aAlphaColor.r, aAlphaColor.g, aAlphaColor.b));
        mContext->PushGroup(gfxContentType::COLOR_ALPHA);
        mAlpha = aAlphaColor.a;
    }

    void PopAlpha()
    {
        // pop the text, using the color alpha as the opacity
        mContext->PopGroupToSource();
        mContext->SetOperator(gfxContext::OPERATOR_OVER);
        mContext->Paint(mAlpha);
        mContext->Restore();
    }

    gfxContext *mContext;
    gfxFloat mAlpha;
};

void
gfxTextRun::Draw(gfxContext *aContext, gfxPoint aPt, DrawMode aDrawMode,
                 uint32_t aStart, uint32_t aLength,
                 PropertyProvider *aProvider, gfxFloat *aAdvanceWidth,
                 gfxTextContextPaint *aContextPaint,
                 gfxTextRunDrawCallbacks *aCallbacks)
{
    NS_ASSERTION(aStart + aLength <= GetLength(), "Substring out of range");
    NS_ASSERTION(aDrawMode == DrawMode::GLYPH_PATH || !(int(aDrawMode) & int(DrawMode::GLYPH_PATH)),
                 "GLYPH_PATH cannot be used with GLYPH_FILL, GLYPH_STROKE or GLYPH_STROKE_UNDERNEATH");
    NS_ASSERTION(aDrawMode == DrawMode::GLYPH_PATH || !aCallbacks, "callback must not be specified unless using GLYPH_PATH");

    bool skipDrawing = mSkipDrawing;
    if (aDrawMode == DrawMode::GLYPH_FILL) {
        gfxRGBA currentColor;
        if (aContext->GetDeviceColor(currentColor) && currentColor.a == 0) {
            skipDrawing = true;
        }
    }

    gfxFloat direction = GetDirection();

    if (skipDrawing) {
        // We don't need to draw anything;
        // but if the caller wants advance width, we need to compute it here
        if (aAdvanceWidth) {
            gfxTextRun::Metrics metrics = MeasureText(aStart, aLength,
                                                      gfxFont::LOOSE_INK_EXTENTS,
                                                      aContext, aProvider);
            *aAdvanceWidth = metrics.mAdvanceWidth * direction;
        }

        // return without drawing
        return;
    }

    gfxPoint pt = aPt;

    // synthetic bolding draws glyphs twice ==> colors with opacity won't draw correctly unless first drawn without alpha
    BufferAlphaColor syntheticBoldBuffer(aContext);
    gfxRGBA currentColor;
    bool needToRestore = false;

    if (aDrawMode == DrawMode::GLYPH_FILL && HasNonOpaqueColor(aContext, currentColor)
                                          && HasSyntheticBold(this, aStart, aLength)) {
        needToRestore = true;
        // measure text, use the bounding box
        gfxTextRun::Metrics metrics = MeasureText(aStart, aLength, gfxFont::LOOSE_INK_EXTENTS,
                                                  aContext, aProvider);
        metrics.mBoundingBox.MoveBy(aPt);
        syntheticBoldBuffer.PushSolidColor(metrics.mBoundingBox, currentColor, GetAppUnitsPerDevUnit());
    }

    GlyphRunIterator iter(this, aStart, aLength);
    while (iter.NextRun()) {
        gfxFont *font = iter.GetGlyphRun()->mFont;
        uint32_t start = iter.GetStringStart();
        uint32_t end = iter.GetStringEnd();
        uint32_t ligatureRunStart = start;
        uint32_t ligatureRunEnd = end;
        ShrinkToLigatureBoundaries(&ligatureRunStart, &ligatureRunEnd);
        
        bool drawPartial = aDrawMode == DrawMode::GLYPH_FILL ||
                           (aDrawMode == DrawMode::GLYPH_PATH && aCallbacks);

        if (drawPartial) {
            DrawPartialLigature(font, aContext, start, ligatureRunStart, &pt,
                                aProvider, aCallbacks);
        }

        DrawGlyphs(font, aContext, aDrawMode, &pt, aContextPaint, ligatureRunStart,
                   ligatureRunEnd, aProvider, ligatureRunStart, ligatureRunEnd,
                   aCallbacks);

        if (drawPartial) {
            DrawPartialLigature(font, aContext, ligatureRunEnd, end, &pt,
                                aProvider, aCallbacks);
        }
    }

    // composite result when synthetic bolding used
    if (needToRestore) {
        syntheticBoldBuffer.PopAlpha();
    }

    if (aAdvanceWidth) {
        *aAdvanceWidth = (pt.x - aPt.x)*direction;
    }
}

void
gfxTextRun::AccumulateMetricsForRun(gfxFont *aFont,
                                    uint32_t aStart, uint32_t aEnd,
                                    gfxFont::BoundingBoxType aBoundingBoxType,
                                    gfxContext *aRefContext,
                                    PropertyProvider *aProvider,
                                    uint32_t aSpacingStart, uint32_t aSpacingEnd,
                                    Metrics *aMetrics)
{
    nsAutoTArray<PropertyProvider::Spacing,200> spacingBuffer;
    bool haveSpacing = GetAdjustedSpacingArray(aStart, aEnd, aProvider,
        aSpacingStart, aSpacingEnd, &spacingBuffer);
    Metrics metrics = aFont->Measure(this, aStart, aEnd, aBoundingBoxType, aRefContext,
                                     haveSpacing ? spacingBuffer.Elements() : nullptr);
    aMetrics->CombineWith(metrics, IsRightToLeft());
}

void
gfxTextRun::AccumulatePartialLigatureMetrics(gfxFont *aFont,
    uint32_t aStart, uint32_t aEnd,
    gfxFont::BoundingBoxType aBoundingBoxType, gfxContext *aRefContext,
    PropertyProvider *aProvider, Metrics *aMetrics)
{
    if (aStart >= aEnd)
        return;

    // Measure partial ligature. We hack this by clipping the metrics in the
    // same way we clip the drawing.
    LigatureData data = ComputeLigatureData(aStart, aEnd, aProvider);

    // First measure the complete ligature
    Metrics metrics;
    AccumulateMetricsForRun(aFont, data.mLigatureStart, data.mLigatureEnd,
                            aBoundingBoxType, aRefContext,
                            aProvider, aStart, aEnd, &metrics);

    // Clip the bounding box to the ligature part
    gfxFloat bboxLeft = metrics.mBoundingBox.X();
    gfxFloat bboxRight = metrics.mBoundingBox.XMost();
    // Where we are going to start "drawing" relative to our left baseline origin
    gfxFloat origin = IsRightToLeft() ? metrics.mAdvanceWidth - data.mPartAdvance : 0;
    ClipPartialLigature(this, &bboxLeft, &bboxRight, origin, &data);
    metrics.mBoundingBox.x = bboxLeft;
    metrics.mBoundingBox.width = bboxRight - bboxLeft;

    // mBoundingBox is now relative to the left baseline origin for the entire
    // ligature. Shift it left.
    metrics.mBoundingBox.x -=
        IsRightToLeft() ? metrics.mAdvanceWidth - (data.mPartAdvance + data.mPartWidth)
            : data.mPartAdvance;    
    metrics.mAdvanceWidth = data.mPartWidth;

    aMetrics->CombineWith(metrics, IsRightToLeft());
}

gfxTextRun::Metrics
gfxTextRun::MeasureText(uint32_t aStart, uint32_t aLength,
                        gfxFont::BoundingBoxType aBoundingBoxType,
                        gfxContext *aRefContext,
                        PropertyProvider *aProvider)
{
    NS_ASSERTION(aStart + aLength <= GetLength(), "Substring out of range");

    Metrics accumulatedMetrics;
    GlyphRunIterator iter(this, aStart, aLength);
    while (iter.NextRun()) {
        gfxFont *font = iter.GetGlyphRun()->mFont;
        uint32_t start = iter.GetStringStart();
        uint32_t end = iter.GetStringEnd();
        uint32_t ligatureRunStart = start;
        uint32_t ligatureRunEnd = end;
        ShrinkToLigatureBoundaries(&ligatureRunStart, &ligatureRunEnd);

        AccumulatePartialLigatureMetrics(font, start, ligatureRunStart,
            aBoundingBoxType, aRefContext, aProvider, &accumulatedMetrics);

        // XXX This sucks. We have to get glyph extents just so we can detect
        // glyphs outside the font box, even when aBoundingBoxType is LOOSE,
        // even though in almost all cases we could get correct results just
        // by getting some ascent/descent from the font and using our stored
        // advance widths.
        AccumulateMetricsForRun(font,
            ligatureRunStart, ligatureRunEnd, aBoundingBoxType,
            aRefContext, aProvider, ligatureRunStart, ligatureRunEnd,
            &accumulatedMetrics);

        AccumulatePartialLigatureMetrics(font, ligatureRunEnd, end,
            aBoundingBoxType, aRefContext, aProvider, &accumulatedMetrics);
    }

    return accumulatedMetrics;
}

#define MEASUREMENT_BUFFER_SIZE 100

uint32_t
gfxTextRun::BreakAndMeasureText(uint32_t aStart, uint32_t aMaxLength,
                                bool aLineBreakBefore, gfxFloat aWidth,
                                PropertyProvider *aProvider,
                                bool aSuppressInitialBreak,
                                gfxFloat *aTrimWhitespace,
                                Metrics *aMetrics,
                                gfxFont::BoundingBoxType aBoundingBoxType,
                                gfxContext *aRefContext,
                                bool *aUsedHyphenation,
                                uint32_t *aLastBreak,
                                bool aCanWordWrap,
                                gfxBreakPriority *aBreakPriority)
{
    aMaxLength = std::min(aMaxLength, GetLength() - aStart);

    NS_ASSERTION(aStart + aMaxLength <= GetLength(), "Substring out of range");

    uint32_t bufferStart = aStart;
    uint32_t bufferLength = std::min<uint32_t>(aMaxLength, MEASUREMENT_BUFFER_SIZE);
    PropertyProvider::Spacing spacingBuffer[MEASUREMENT_BUFFER_SIZE];
    bool haveSpacing = aProvider && (mFlags & gfxTextRunFactory::TEXT_ENABLE_SPACING) != 0;
    if (haveSpacing) {
        GetAdjustedSpacing(this, bufferStart, bufferStart + bufferLength, aProvider,
                           spacingBuffer);
    }
    bool hyphenBuffer[MEASUREMENT_BUFFER_SIZE];
    bool haveHyphenation = aProvider &&
        (aProvider->GetHyphensOption() == NS_STYLE_HYPHENS_AUTO ||
         (aProvider->GetHyphensOption() == NS_STYLE_HYPHENS_MANUAL &&
          (mFlags & gfxTextRunFactory::TEXT_ENABLE_HYPHEN_BREAKS) != 0));
    if (haveHyphenation) {
        aProvider->GetHyphenationBreaks(bufferStart, bufferLength,
                                        hyphenBuffer);
    }

    gfxFloat width = 0;
    gfxFloat advance = 0;
    // The number of space characters that can be trimmed
    uint32_t trimmableChars = 0;
    // The amount of space removed by ignoring trimmableChars
    gfxFloat trimmableAdvance = 0;
    int32_t lastBreak = -1;
    int32_t lastBreakTrimmableChars = -1;
    gfxFloat lastBreakTrimmableAdvance = -1;
    bool aborted = false;
    uint32_t end = aStart + aMaxLength;
    bool lastBreakUsedHyphenation = false;

    uint32_t ligatureRunStart = aStart;
    uint32_t ligatureRunEnd = end;
    ShrinkToLigatureBoundaries(&ligatureRunStart, &ligatureRunEnd);

    uint32_t i;
    for (i = aStart; i < end; ++i) {
        if (i >= bufferStart + bufferLength) {
            // Fetch more spacing and hyphenation data
            bufferStart = i;
            bufferLength = std::min(aStart + aMaxLength, i + MEASUREMENT_BUFFER_SIZE) - i;
            if (haveSpacing) {
                GetAdjustedSpacing(this, bufferStart, bufferStart + bufferLength, aProvider,
                                   spacingBuffer);
            }
            if (haveHyphenation) {
                aProvider->GetHyphenationBreaks(bufferStart, bufferLength,
                                                hyphenBuffer);
            }
        }

        // There can't be a word-wrap break opportunity at the beginning of the
        // line: if the width is too small for even one character to fit, it 
        // could be the first and last break opportunity on the line, and that
        // would trigger an infinite loop.
        if (!aSuppressInitialBreak || i > aStart) {
            bool atNaturalBreak = mCharacterGlyphs[i].CanBreakBefore() == 1;
            bool atHyphenationBreak =
                !atNaturalBreak && haveHyphenation && hyphenBuffer[i - bufferStart];
            bool atBreak = atNaturalBreak || atHyphenationBreak;
            bool wordWrapping =
                aCanWordWrap && mCharacterGlyphs[i].IsClusterStart() &&
                *aBreakPriority <= gfxBreakPriority::eWordWrapBreak;

            if (atBreak || wordWrapping) {
                gfxFloat hyphenatedAdvance = advance;
                if (atHyphenationBreak) {
                    hyphenatedAdvance += aProvider->GetHyphenWidth();
                }
            
                if (lastBreak < 0 || width + hyphenatedAdvance - trimmableAdvance <= aWidth) {
                    // We can break here.
                    lastBreak = i;
                    lastBreakTrimmableChars = trimmableChars;
                    lastBreakTrimmableAdvance = trimmableAdvance;
                    lastBreakUsedHyphenation = atHyphenationBreak;
                    *aBreakPriority = atBreak ? gfxBreakPriority::eNormalBreak
                                              : gfxBreakPriority::eWordWrapBreak;
                }

                width += advance;
                advance = 0;
                if (width - trimmableAdvance > aWidth) {
                    // No more text fits. Abort
                    aborted = true;
                    break;
                }
            }
        }
        
        gfxFloat charAdvance;
        if (i >= ligatureRunStart && i < ligatureRunEnd) {
            charAdvance = GetAdvanceForGlyphs(i, i + 1);
            if (haveSpacing) {
                PropertyProvider::Spacing *space = &spacingBuffer[i - bufferStart];
                charAdvance += space->mBefore + space->mAfter;
            }
        } else {
            charAdvance = ComputePartialLigatureWidth(i, i + 1, aProvider);
        }
        
        advance += charAdvance;
        if (aTrimWhitespace) {
            if (mCharacterGlyphs[i].CharIsSpace()) {
                ++trimmableChars;
                trimmableAdvance += charAdvance;
            } else {
                trimmableAdvance = 0;
                trimmableChars = 0;
            }
        }
    }

    if (!aborted) {
        width += advance;
    }

    // There are three possibilities:
    // 1) all the text fit (width <= aWidth)
    // 2) some of the text fit up to a break opportunity (width > aWidth && lastBreak >= 0)
    // 3) none of the text fits before a break opportunity (width > aWidth && lastBreak < 0)
    uint32_t charsFit;
    bool usedHyphenation = false;
    if (width - trimmableAdvance <= aWidth) {
        charsFit = aMaxLength;
    } else if (lastBreak >= 0) {
        charsFit = lastBreak - aStart;
        trimmableChars = lastBreakTrimmableChars;
        trimmableAdvance = lastBreakTrimmableAdvance;
        usedHyphenation = lastBreakUsedHyphenation;
    } else {
        charsFit = aMaxLength;
    }

    if (aMetrics) {
        *aMetrics = MeasureText(aStart, charsFit - trimmableChars,
            aBoundingBoxType, aRefContext, aProvider);
    }
    if (aTrimWhitespace) {
        *aTrimWhitespace = trimmableAdvance;
    }
    if (aUsedHyphenation) {
        *aUsedHyphenation = usedHyphenation;
    }
    if (aLastBreak && charsFit == aMaxLength) {
        if (lastBreak < 0) {
            *aLastBreak = UINT32_MAX;
        } else {
            *aLastBreak = lastBreak - aStart;
        }
    }

    return charsFit;
}

gfxFloat
gfxTextRun::GetAdvanceWidth(uint32_t aStart, uint32_t aLength,
                            PropertyProvider *aProvider)
{
    NS_ASSERTION(aStart + aLength <= GetLength(), "Substring out of range");

    uint32_t ligatureRunStart = aStart;
    uint32_t ligatureRunEnd = aStart + aLength;
    ShrinkToLigatureBoundaries(&ligatureRunStart, &ligatureRunEnd);

    gfxFloat result = ComputePartialLigatureWidth(aStart, ligatureRunStart, aProvider) +
                      ComputePartialLigatureWidth(ligatureRunEnd, aStart + aLength, aProvider);

    // Account for all remaining spacing here. This is more efficient than
    // processing it along with the glyphs.
    if (aProvider && (mFlags & gfxTextRunFactory::TEXT_ENABLE_SPACING)) {
        uint32_t i;
        nsAutoTArray<PropertyProvider::Spacing,200> spacingBuffer;
        if (spacingBuffer.AppendElements(aLength)) {
            GetAdjustedSpacing(this, ligatureRunStart, ligatureRunEnd, aProvider,
                               spacingBuffer.Elements());
            for (i = 0; i < ligatureRunEnd - ligatureRunStart; ++i) {
                PropertyProvider::Spacing *space = &spacingBuffer[i];
                result += space->mBefore + space->mAfter;
            }
        }
    }

    return result + GetAdvanceForGlyphs(ligatureRunStart, ligatureRunEnd);
}

bool
gfxTextRun::SetLineBreaks(uint32_t aStart, uint32_t aLength,
                          bool aLineBreakBefore, bool aLineBreakAfter,
                          gfxFloat *aAdvanceWidthDelta,
                          gfxContext *aRefContext)
{
    // Do nothing because our shaping does not currently take linebreaks into
    // account. There is no change in advance width.
    if (aAdvanceWidthDelta) {
        *aAdvanceWidthDelta = 0;
    }
    return false;
}

uint32_t
gfxTextRun::FindFirstGlyphRunContaining(uint32_t aOffset)
{
    NS_ASSERTION(aOffset <= GetLength(), "Bad offset looking for glyphrun");
    NS_ASSERTION(GetLength() == 0 || mGlyphRuns.Length() > 0,
                 "non-empty text but no glyph runs present!");
    if (aOffset == GetLength())
        return mGlyphRuns.Length();
    uint32_t start = 0;
    uint32_t end = mGlyphRuns.Length();
    while (end - start > 1) {
        uint32_t mid = (start + end)/2;
        if (mGlyphRuns[mid].mCharacterOffset <= aOffset) {
            start = mid;
        } else {
            end = mid;
        }
    }
    NS_ASSERTION(mGlyphRuns[start].mCharacterOffset <= aOffset,
                 "Hmm, something went wrong, aOffset should have been found");
    return start;
}

nsresult
gfxTextRun::AddGlyphRun(gfxFont *aFont, uint8_t aMatchType,
                        uint32_t aUTF16Offset, bool aForceNewRun)
{
    NS_ASSERTION(aFont, "adding glyph run for null font!");
    if (!aFont) {
        return NS_OK;
    }    
    uint32_t numGlyphRuns = mGlyphRuns.Length();
    if (!aForceNewRun && numGlyphRuns > 0) {
        GlyphRun *lastGlyphRun = &mGlyphRuns[numGlyphRuns - 1];

        NS_ASSERTION(lastGlyphRun->mCharacterOffset <= aUTF16Offset,
                     "Glyph runs out of order (and run not forced)");

        // Don't append a run if the font is already the one we want
        if (lastGlyphRun->mFont == aFont &&
            lastGlyphRun->mMatchType == aMatchType)
        {
            return NS_OK;
        }

        // If the offset has not changed, avoid leaving a zero-length run
        // by overwriting the last entry instead of appending...
        if (lastGlyphRun->mCharacterOffset == aUTF16Offset) {

            // ...except that if the run before the last entry had the same
            // font as the new one wants, merge with it instead of creating
            // adjacent runs with the same font
            if (numGlyphRuns > 1 &&
                mGlyphRuns[numGlyphRuns - 2].mFont == aFont &&
                mGlyphRuns[numGlyphRuns - 2].mMatchType == aMatchType)
            {
                mGlyphRuns.TruncateLength(numGlyphRuns - 1);
                return NS_OK;
            }

            lastGlyphRun->mFont = aFont;
            lastGlyphRun->mMatchType = aMatchType;
            return NS_OK;
        }
    }

    NS_ASSERTION(aForceNewRun || numGlyphRuns > 0 || aUTF16Offset == 0,
                 "First run doesn't cover the first character (and run not forced)?");

    GlyphRun *glyphRun = mGlyphRuns.AppendElement();
    if (!glyphRun)
        return NS_ERROR_OUT_OF_MEMORY;
    glyphRun->mFont = aFont;
    glyphRun->mCharacterOffset = aUTF16Offset;
    glyphRun->mMatchType = aMatchType;
    return NS_OK;
}

void
gfxTextRun::SortGlyphRuns()
{
    if (mGlyphRuns.Length() <= 1)
        return;

    nsTArray<GlyphRun> runs(mGlyphRuns);
    GlyphRunOffsetComparator comp;
    runs.Sort(comp);

    // Now copy back, coalescing adjacent glyph runs that have the same font
    mGlyphRuns.Clear();
    uint32_t i, count = runs.Length();
    for (i = 0; i < count; ++i) {
        // a GlyphRun with the same font as the previous GlyphRun can just
        // be skipped; the last GlyphRun will cover its character range.
        if (i == 0 || runs[i].mFont != runs[i - 1].mFont) {
            mGlyphRuns.AppendElement(runs[i]);
            // If two fonts have the same character offset, Sort() will have
            // randomized the order.
            NS_ASSERTION(i == 0 ||
                         runs[i].mCharacterOffset !=
                         runs[i - 1].mCharacterOffset,
                         "Two fonts for the same run, glyph indices may not match the font");
        }
    }
}

// Note that SanitizeGlyphRuns scans all glyph runs in the textrun;
// therefore we only call it once, at the end of textrun construction,
// NOT incrementally as each glyph run is added (bug 680402).
void
gfxTextRun::SanitizeGlyphRuns()
{
    if (mGlyphRuns.Length() <= 1)
        return;

    // If any glyph run starts with ligature-continuation characters, we need to advance it
    // to the first "real" character to avoid drawing partial ligature glyphs from wrong font
    // (seen with U+FEFF in reftest 474417-1, as Core Text eliminates the glyph, which makes
    // it appear as if a ligature has been formed)
    int32_t i, lastRunIndex = mGlyphRuns.Length() - 1;
    const CompressedGlyph *charGlyphs = mCharacterGlyphs;
    for (i = lastRunIndex; i >= 0; --i) {
        GlyphRun& run = mGlyphRuns[i];
        while (charGlyphs[run.mCharacterOffset].IsLigatureContinuation() &&
               run.mCharacterOffset < GetLength()) {
            run.mCharacterOffset++;
        }
        // if the run has become empty, eliminate it
        if ((i < lastRunIndex &&
             run.mCharacterOffset >= mGlyphRuns[i+1].mCharacterOffset) ||
            (i == lastRunIndex && run.mCharacterOffset == GetLength())) {
            mGlyphRuns.RemoveElementAt(i);
            --lastRunIndex;
        }
    }
}

uint32_t
gfxTextRun::CountMissingGlyphs()
{
    uint32_t i;
    uint32_t count = 0;
    for (i = 0; i < GetLength(); ++i) {
        if (mCharacterGlyphs[i].IsMissing()) {
            ++count;
        }
    }
    return count;
}

gfxTextRun::DetailedGlyph *
gfxTextRun::AllocateDetailedGlyphs(uint32_t aIndex, uint32_t aCount)
{
    NS_ASSERTION(aIndex < GetLength(), "Index out of range");

    if (!mDetailedGlyphs) {
        mDetailedGlyphs = new DetailedGlyphStore();
    }

    DetailedGlyph *details = mDetailedGlyphs->Allocate(aIndex, aCount);
    if (!details) {
        mCharacterGlyphs[aIndex].SetMissing(0);
        return nullptr;
    }

    return details;
}

void
gfxTextRun::CopyGlyphDataFrom(gfxShapedWord *aShapedWord, uint32_t aOffset)
{
    uint32_t wordLen = aShapedWord->GetLength();
    NS_ASSERTION(aOffset + wordLen <= GetLength(),
                 "word overruns end of textrun!");

    CompressedGlyph *charGlyphs = GetCharacterGlyphs();
    const CompressedGlyph *wordGlyphs = aShapedWord->GetCharacterGlyphs();
    if (aShapedWord->HasDetailedGlyphs()) {
        for (uint32_t i = 0; i < wordLen; ++i, ++aOffset) {
            const CompressedGlyph& g = wordGlyphs[i];
            if (g.IsSimpleGlyph()) {
                charGlyphs[aOffset] = g;
            } else {
                const DetailedGlyph *details =
                    g.GetGlyphCount() > 0 ?
                        aShapedWord->GetDetailedGlyphs(i) : nullptr;
                SetGlyphs(aOffset, g, details);
            }
        }
    } else {
        memcpy(charGlyphs + aOffset, wordGlyphs,
               wordLen * sizeof(CompressedGlyph));
    }
}

void
gfxTextRun::CopyGlyphDataFrom(gfxTextRun *aSource, uint32_t aStart,
                              uint32_t aLength, uint32_t aDest)
{
    NS_ASSERTION(aStart + aLength <= aSource->GetLength(),
                 "Source substring out of range");
    NS_ASSERTION(aDest + aLength <= GetLength(),
                 "Destination substring out of range");

    if (aSource->mSkipDrawing) {
        mSkipDrawing = true;
    }

    // Copy base glyph data, and DetailedGlyph data where present
    const CompressedGlyph *srcGlyphs = aSource->mCharacterGlyphs + aStart;
    CompressedGlyph *dstGlyphs = mCharacterGlyphs + aDest;
    for (uint32_t i = 0; i < aLength; ++i) {
        CompressedGlyph g = srcGlyphs[i];
        g.SetCanBreakBefore(!g.IsClusterStart() ?
            CompressedGlyph::FLAG_BREAK_TYPE_NONE :
            dstGlyphs[i].CanBreakBefore());
        if (!g.IsSimpleGlyph()) {
            uint32_t count = g.GetGlyphCount();
            if (count > 0) {
                DetailedGlyph *dst = AllocateDetailedGlyphs(i + aDest, count);
                if (dst) {
                    DetailedGlyph *src = aSource->GetDetailedGlyphs(i + aStart);
                    if (src) {
                        ::memcpy(dst, src, count * sizeof(DetailedGlyph));
                    } else {
                        g.SetMissing(0);
                    }
                } else {
                    g.SetMissing(0);
                }
            }
        }
        dstGlyphs[i] = g;
    }

    // Copy glyph runs
    GlyphRunIterator iter(aSource, aStart, aLength);
#ifdef DEBUG
    gfxFont *lastFont = nullptr;
#endif
    while (iter.NextRun()) {
        gfxFont *font = iter.GetGlyphRun()->mFont;
        NS_ASSERTION(font != lastFont, "Glyphruns not coalesced?");
#ifdef DEBUG
        lastFont = font;
        uint32_t end = iter.GetStringEnd();
#endif
        uint32_t start = iter.GetStringStart();

        // These used to be NS_ASSERTION()s, but WARNING is more appropriate.
        // Although it's unusual (and not desirable), it's possible for us to assign
        // different fonts to a base character and a following diacritic.
        // Example on OSX 10.5/10.6 with default fonts installed:
        //     data:text/html,<p style="font-family:helvetica, arial, sans-serif;">
        //                    &%23x043E;&%23x0486;&%23x20;&%23x043E;&%23x0486;
        // This means the rendering of the cluster will probably not be very good,
        // but it's the best we can do for now if the specified font only covered the
        // initial base character and not its applied marks.
        NS_WARN_IF_FALSE(aSource->IsClusterStart(start),
                         "Started font run in the middle of a cluster");
        NS_WARN_IF_FALSE(end == aSource->GetLength() || aSource->IsClusterStart(end),
                         "Ended font run in the middle of a cluster");

        nsresult rv = AddGlyphRun(font, iter.GetGlyphRun()->mMatchType,
                                  start - aStart + aDest, false);
        if (NS_FAILED(rv))
            return;
    }
}

void
gfxTextRun::SetSpaceGlyph(gfxFont *aFont, gfxContext *aContext,
                          uint32_t aCharIndex)
{
    if (SetSpaceGlyphIfSimple(aFont, aContext, aCharIndex, ' ')) {
        return;
    }

    aFont->InitWordCache();
    static const uint8_t space = ' ';
    gfxShapedWord *sw = aFont->GetShapedWord(aContext,
                                             &space, 1,
                                             HashMix(0, ' '), 
                                             MOZ_SCRIPT_LATIN,
                                             mAppUnitsPerDevUnit,
                                             gfxTextRunFactory::TEXT_IS_8BIT |
                                             gfxTextRunFactory::TEXT_IS_ASCII |
                                             gfxTextRunFactory::TEXT_IS_PERSISTENT,
                                             nullptr);
    if (sw) {
        AddGlyphRun(aFont, gfxTextRange::kFontGroup, aCharIndex, false);
        CopyGlyphDataFrom(sw, aCharIndex);
    }
}

bool
gfxTextRun::SetSpaceGlyphIfSimple(gfxFont *aFont, gfxContext *aContext,
                                  uint32_t aCharIndex, char16_t aSpaceChar)
{
    uint32_t spaceGlyph = aFont->GetSpaceGlyph();
    if (!spaceGlyph || !CompressedGlyph::IsSimpleGlyphID(spaceGlyph)) {
        return false;
    }

    uint32_t spaceWidthAppUnits =
        NS_lroundf(aFont->GetMetrics().spaceWidth * mAppUnitsPerDevUnit);
    if (!CompressedGlyph::IsSimpleAdvance(spaceWidthAppUnits)) {
        return false;
    }

    AddGlyphRun(aFont, gfxTextRange::kFontGroup, aCharIndex, false);
    CompressedGlyph g;
    g.SetSimpleGlyph(spaceWidthAppUnits, spaceGlyph);
    if (aSpaceChar == ' ') {
        g.SetIsSpace();
    }
    GetCharacterGlyphs()[aCharIndex] = g;
    return true;
}

void
gfxTextRun::FetchGlyphExtents(gfxContext *aRefContext)
{
    bool needsGlyphExtents = NeedsGlyphExtents(this);
    if (!needsGlyphExtents && !mDetailedGlyphs)
        return;

    uint32_t i, runCount = mGlyphRuns.Length();
    CompressedGlyph *charGlyphs = mCharacterGlyphs;
    for (i = 0; i < runCount; ++i) {
        const GlyphRun& run = mGlyphRuns[i];
        gfxFont *font = run.mFont;
        uint32_t start = run.mCharacterOffset;
        uint32_t end = i + 1 < runCount ?
            mGlyphRuns[i + 1].mCharacterOffset : GetLength();
        bool fontIsSetup = false;
        uint32_t j;
        gfxGlyphExtents *extents = font->GetOrCreateGlyphExtents(mAppUnitsPerDevUnit);
  
        for (j = start; j < end; ++j) {
            const gfxTextRun::CompressedGlyph *glyphData = &charGlyphs[j];
            if (glyphData->IsSimpleGlyph()) {
                // If we're in speed mode, don't set up glyph extents here; we'll
                // just return "optimistic" glyph bounds later
                if (needsGlyphExtents) {
                    uint32_t glyphIndex = glyphData->GetSimpleGlyph();
                    if (!extents->IsGlyphKnown(glyphIndex)) {
                        if (!fontIsSetup) {
                            if (!font->SetupCairoFont(aRefContext)) {
                                NS_WARNING("failed to set up font for glyph extents");
                                break;
                            }
                            fontIsSetup = true;
                        }
#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
                        ++gGlyphExtentsSetupEagerSimple;
#endif
                        font->SetupGlyphExtents(aRefContext, glyphIndex, false, extents);
                    }
                }
            } else if (!glyphData->IsMissing()) {
                uint32_t glyphCount = glyphData->GetGlyphCount();
                if (glyphCount == 0) {
                    continue;
                }
                const gfxTextRun::DetailedGlyph *details = GetDetailedGlyphs(j);
                if (!details) {
                    continue;
                }
                for (uint32_t k = 0; k < glyphCount; ++k, ++details) {
                    uint32_t glyphIndex = details->mGlyphID;
                    if (!extents->IsGlyphKnownWithTightExtents(glyphIndex)) {
                        if (!fontIsSetup) {
                            if (!font->SetupCairoFont(aRefContext)) {
                                NS_WARNING("failed to set up font for glyph extents");
                                break;
                            }
                            fontIsSetup = true;
                        }
#ifdef DEBUG_TEXT_RUN_STORAGE_METRICS
                        ++gGlyphExtentsSetupEagerTight;
#endif
                        font->SetupGlyphExtents(aRefContext, glyphIndex, true, extents);
                    }
                }
            }
        }
    }
}


gfxTextRun::ClusterIterator::ClusterIterator(gfxTextRun *aTextRun)
    : mTextRun(aTextRun), mCurrentChar(uint32_t(-1))
{
}

void
gfxTextRun::ClusterIterator::Reset()
{
    mCurrentChar = uint32_t(-1);
}

bool
gfxTextRun::ClusterIterator::NextCluster()
{
    uint32_t len = mTextRun->GetLength();
    while (++mCurrentChar < len) {
        if (mTextRun->IsClusterStart(mCurrentChar)) {
            return true;
        }
    }

    mCurrentChar = uint32_t(-1);
    return false;
}

uint32_t
gfxTextRun::ClusterIterator::ClusterLength() const
{
    if (mCurrentChar == uint32_t(-1)) {
        return 0;
    }

    uint32_t i = mCurrentChar,
             len = mTextRun->GetLength();
    while (++i < len) {
        if (mTextRun->IsClusterStart(i)) {
            break;
        }
    }

    return i - mCurrentChar;
}

gfxFloat
gfxTextRun::ClusterIterator::ClusterAdvance(PropertyProvider *aProvider) const
{
    if (mCurrentChar == uint32_t(-1)) {
        return 0;
    }

    return mTextRun->GetAdvanceWidth(mCurrentChar, ClusterLength(), aProvider);
}

size_t
gfxTextRun::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf)
{
    // The second arg is how much gfxTextRun::AllocateStorage would have
    // allocated.
    size_t total = mGlyphRuns.SizeOfExcludingThis(aMallocSizeOf);

    if (mDetailedGlyphs) {
        total += mDetailedGlyphs->SizeOfIncludingThis(aMallocSizeOf);
    }

    return total;
}

size_t
gfxTextRun::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf)
{
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}


#ifdef DEBUG
void
gfxTextRun::Dump(FILE* aOutput) {
    if (!aOutput) {
        aOutput = stdout;
    }

    uint32_t i;
    fputc('[', aOutput);
    for (i = 0; i < mGlyphRuns.Length(); ++i) {
        if (i > 0) {
            fputc(',', aOutput);
        }
        gfxFont* font = mGlyphRuns[i].mFont;
        const gfxFontStyle* style = font->GetStyle();
        NS_ConvertUTF16toUTF8 fontName(font->GetName());
        nsAutoCString lang;
        style->language->ToUTF8String(lang);
        fprintf(aOutput, "%d: %s %f/%d/%d/%s", mGlyphRuns[i].mCharacterOffset,
                fontName.get(), style->size,
                style->weight, style->style, lang.get());
    }
    fputc(']', aOutput);
}
#endif

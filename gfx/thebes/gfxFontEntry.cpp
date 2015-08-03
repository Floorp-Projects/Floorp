/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"
#include "mozilla/MathAlgorithms.h"

#include "mozilla/Logging.h"

#include "nsServiceManagerUtils.h"
#include "nsExpirationTracker.h"
#include "nsILanguageAtomService.h"
#include "nsITimer.h"

#include "gfxFontEntry.h"
#include "gfxTextRun.h"
#include "gfxPlatform.h"
#include "nsGkAtoms.h"

#include "gfxTypes.h"
#include "gfxContext.h"
#include "gfxFontConstants.h"
#include "gfxHarfBuzzShaper.h"
#include "gfxUserFontSet.h"
#include "gfxPlatformFontList.h"
#include "nsUnicodeProperties.h"
#include "nsMathUtils.h"
#include "nsBidiUtils.h"
#include "nsUnicodeRange.h"
#include "nsStyleConsts.h"
#include "mozilla/AppUnits.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Likely.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/Telemetry.h"
#include "gfxSVGGlyphs.h"
#include "gfxMathTable.h"
#include "gfx2DGlue.h"

#if defined(XP_MACOSX)
#include "nsCocoaFeatures.h"
#endif

#include "cairo.h"

#include "harfbuzz/hb.h"
#include "harfbuzz/hb-ot.h"
#include "graphite2/Font.h"

#include <algorithm>

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::unicode;
using mozilla::services::GetObserverService;

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
    mIsValid(true),
    mIsBadUnderlineFont(false),
    mIsUserFontContainer(false),
    mIsDataUserFont(false),
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
    mGraphiteSpaceContextualsInitialized(false),
    mHasGraphiteSpaceContextuals(false),
    mSpaceGlyphIsInvisible(false),
    mSpaceGlyphIsInvisibleInitialized(false),
    mCheckedForGraphiteTables(false),
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
    mIsValid(true),
    mIsBadUnderlineFont(false),
    mIsUserFontContainer(false),
    mIsDataUserFont(false),
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
    mGraphiteSpaceContextualsInitialized(false),
    mHasGraphiteSpaceContextuals(false),
    mSpaceGlyphIsInvisible(false),
    mSpaceGlyphIsInvisibleInitialized(false),
    mCheckedForGraphiteTables(false),
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

static PLDHashOperator
DestroyHBSet(const uint32_t& aTag, hb_set_t*& aSet, void *aUserArg)
{
    hb_set_destroy(aSet);
    return PL_DHASH_NEXT;
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
    if (mIsDataUserFont) {
        gfxUserFontSet::UserFontCache::ForgetFont(this);
    }

    if (mFeatureInputs) {
        mFeatureInputs->Enumerate(DestroyHBSet, nullptr);
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
gfxFontEntry::FindOrMakeFont(const gfxFontStyle *aStyle,
                             bool aNeedsBold,
                             gfxCharacterMap* aUnicodeRangeMap)
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
        font->SetUnicodeRangeMap(aUnicodeRangeMap);
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
    MOZ_ASSERT(mSVGInitialized,
               "SVG data has not yet been loaded. TryGetSVGData() first.");
    MOZ_ASSERT(mUnitsPerEm >= kMinUPEM && mUnitsPerEm <= kMaxUPEM,
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
gfxFontEntry::TryGetMathTable()
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
    explicit FontTableBlobData(FallibleTArray<uint8_t>& aBuffer)
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
        return mTableData.ShallowSizeOfExcludingThis(aMallocSizeOf);
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
        mFontTableCache = new nsTHashtable<FontTableHashEntry>(8);
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
      mFontTableCache = new nsTHashtable<FontTableHashEntry>(8);
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
gfxFontEntry::HasGraphiteSpaceContextuals()
{
    if (!mGraphiteSpaceContextualsInitialized) {
        gr_face* face = GetGrFace();
        if (face) {
            const gr_faceinfo* faceInfo = gr_face_info(face, 0);
            mHasGraphiteSpaceContextuals =
                faceInfo->space_contextuals != gr_faceinfo::gr_space_none;
            ReleaseGrFace(face);
        }
        mGraphiteSpaceContextualsInitialized = true;
    }
    return mHasGraphiteSpaceContextuals;
}

#define FEATURE_SCRIPT_MASK 0x000000ff // script index replaces low byte of tag

// check for too many script codes
PR_STATIC_ASSERT(MOZ_NUM_SCRIPT_CODES <= FEATURE_SCRIPT_MASK);

// high-order three bytes of tag with script in low-order byte
#define SCRIPT_FEATURE(s,tag) (((~FEATURE_SCRIPT_MASK) & (tag)) | \
                               ((FEATURE_SCRIPT_MASK) & (s)))

bool
gfxFontEntry::SupportsOpenTypeFeature(int32_t aScript, uint32_t aFeatureTag)
{
    if (!mSupportedFeatures) {
        mSupportedFeatures = new nsDataHashtable<nsUint32HashKey,bool>();
    }

    // note: high-order three bytes *must* be unique for each feature
    // listed below (see SCRIPT_FEATURE macro def'n)
    NS_ASSERTION(aFeatureTag == HB_TAG('s','m','c','p') ||
                 aFeatureTag == HB_TAG('c','2','s','c') ||
                 aFeatureTag == HB_TAG('p','c','a','p') ||
                 aFeatureTag == HB_TAG('c','2','p','c') ||
                 aFeatureTag == HB_TAG('s','u','p','s') ||
                 aFeatureTag == HB_TAG('s','u','b','s') ||
                 aFeatureTag == HB_TAG('v','e','r','t'),
                 "use of unknown feature tag");

    // note: graphite feature support uses the last script index
    NS_ASSERTION(aScript < FEATURE_SCRIPT_MASK - 1,
                 "need to bump the size of the feature shift");

    uint32_t scriptFeature = SCRIPT_FEATURE(aScript, aFeatureTag);
    bool result;
    if (mSupportedFeatures->Get(scriptFeature, &result)) {
        return result;
    }

    result = false;

    hb_face_t *face = GetHBFace();

    if (hb_ot_layout_has_substitution(face)) {
        hb_script_t hbScript =
            gfxHarfBuzzShaper::GetHBScriptUsedForShaping(aScript);

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
        scriptTag = &scriptTags[0];
        while (*scriptTag != HB_TAG_NONE) {
            unsigned int scriptIndex;
            if (hb_ot_layout_table_find_script(face, kGSUB, *scriptTag,
                                               &scriptIndex)) {
                if (hb_ot_layout_language_find_feature(face, kGSUB,
                                                       scriptIndex,
                                           HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX,
                                                       aFeatureTag, nullptr)) {
                    result = true;
                }
                break;
            }
            ++scriptTag;
        }
    }

    hb_face_destroy(face);

    mSupportedFeatures->Put(scriptFeature, result);

    return result;
}

const hb_set_t*
gfxFontEntry::InputsForOpenTypeFeature(int32_t aScript, uint32_t aFeatureTag)
{
    if (!mFeatureInputs) {
        mFeatureInputs = new nsDataHashtable<nsUint32HashKey,hb_set_t*>();
    }

    NS_ASSERTION(aFeatureTag == HB_TAG('s','u','p','s') ||
                 aFeatureTag == HB_TAG('s','u','b','s'),
                 "use of unknown feature tag");

    uint32_t scriptFeature = SCRIPT_FEATURE(aScript, aFeatureTag);
    hb_set_t *inputGlyphs;
    if (mFeatureInputs->Get(scriptFeature, &inputGlyphs)) {
        return inputGlyphs;
    }

    inputGlyphs = hb_set_create();

    hb_face_t *face = GetHBFace();

    if (hb_ot_layout_has_substitution(face)) {
        hb_script_t hbScript =
            gfxHarfBuzzShaper::GetHBScriptUsedForShaping(aScript);

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

        const hb_tag_t kGSUB = HB_TAG('G','S','U','B');
        hb_tag_t features[2] = { aFeatureTag, HB_TAG_NONE };
        hb_set_t *featurelookups = hb_set_create();
        hb_ot_layout_collect_lookups(face, kGSUB, scriptTags, nullptr,
                                     features, featurelookups);
        hb_codepoint_t index = -1;
        while (hb_set_next(featurelookups, &index)) {
            hb_ot_layout_lookup_collect_glyphs(face, kGSUB, index,
                                               nullptr, inputGlyphs,
                                               nullptr, nullptr);
        }
        hb_set_destroy(featurelookups);
    }

    hb_face_destroy(face);

    mFeatureInputs->Put(scriptFeature, inputGlyphs);
    return inputGlyphs;
}

bool
gfxFontEntry::SupportsGraphiteFeature(uint32_t aFeatureTag)
{
    if (!mSupportedFeatures) {
        mSupportedFeatures = new nsDataHashtable<nsUint32HashKey,bool>();
    }

    // note: high-order three bytes *must* be unique for each feature
    // listed below (see SCRIPT_FEATURE macro def'n)
    NS_ASSERTION(aFeatureTag == HB_TAG('s','m','c','p') ||
                 aFeatureTag == HB_TAG('c','2','s','c') ||
                 aFeatureTag == HB_TAG('p','c','a','p') ||
                 aFeatureTag == HB_TAG('c','2','p','c') ||
                 aFeatureTag == HB_TAG('s','u','p','s') ||
                 aFeatureTag == HB_TAG('s','u','b','s'),
                 "use of unknown feature tag");

    // graphite feature check uses the last script slot
    uint32_t scriptFeature = SCRIPT_FEATURE(FEATURE_SCRIPT_MASK, aFeatureTag);
    bool result;
    if (mSupportedFeatures->Get(scriptFeature, &result)) {
        return result;
    }

    gr_face* face = GetGrFace();
    result = gr_face_find_fref(face, aFeatureTag) != nullptr;
    ReleaseGrFace(face);

    mSupportedFeatures->Put(scriptFeature, result);

    return result;
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

size_t
gfxFontEntry::FontTableHashEntry::SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const
{
    size_t n = 0;
    if (mBlob) {
        n += aMallocSizeOf(mBlob);
    }
    if (mSharedBlobData) {
        n += mSharedBlobData->SizeOfIncludingThis(aMallocSizeOf);
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
            mFontTableCache->SizeOfIncludingThis(aMallocSizeOf);
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
    nsAutoTArray<gfxFontEntry*,4> matched;
    FindAllFontsForStyle(aFontStyle, matched, aNeedsSyntheticBold);
    if (!matched.IsEmpty()) {
        return matched[0];
    }
    return nullptr;
}

static inline uint32_t
StyleStretchDistance(gfxFontEntry *aFontEntry, bool aTargetItalic,
                     int16_t aTargetStretch)
{
    // Compute a measure of the "distance" between the requested style
    // and the given fontEntry,
    // considering italicness and font-stretch but not weight.

    int32_t distance = 0;
    if (aTargetStretch != aFontEntry->mStretch) {
        // stretch values are in the range -4 .. +4
        // if aTargetStretch is positive, we prefer more-positive values;
        // if zero or negative, prefer more-negative
        if (aTargetStretch > 0) {
            distance = (aFontEntry->mStretch - aTargetStretch) * 2;
        } else {
            distance = (aTargetStretch - aFontEntry->mStretch) * 2;
        }
        // if the computed "distance" here is negative, it means that
        // aFontEntry lies in the "non-preferred" direction from aTargetStretch,
        // so we treat that as larger than any preferred-direction distance
        // (max possible is 8) by adding an extra 10 to the absolute value
        if (distance < 0) {
            distance = -distance + 10;
        }
    }
    if (aFontEntry->IsItalic() != aTargetItalic) {
        distance += 1;
    }
    return uint32_t(distance);
}

#define NON_DESIRED_DIRECTION_DISTANCE 1000
#define MAX_WEIGHT_DISTANCE            2000

// CSS currently limits font weights to multiples of 100 but the weight
// matching code below does not assume this.
//
// Calculate weight values with range (0..1000). In general, heavier weights
// match towards even heavier weights while lighter weights match towards even
// lighter weights. Target weight values in the range [400..500] are special,
// since they will first match up to 500, then down to 0, then up again
// towards 999.
//
// Example: with target 600 and font weight 800, distance will be 200. With
// target 300 and font weight 600, distance will be 1300, since heavier weights
// are farther away than lighter weights. If the target is 5 and the font weight
// 995, the distance would be 1990 for the same reason.

static inline uint32_t
WeightDistance(uint32_t aTargetWeight, uint32_t aFontWeight)
{
    // Compute a measure of the "distance" between the requested
    // weight and the given fontEntry

    int32_t distance = 0, addedDistance = 0;
    if (aTargetWeight != aFontWeight) {
        if (aTargetWeight > 500) {
            distance = aFontWeight - aTargetWeight;
        } else if (aTargetWeight < 400) {
            distance = aTargetWeight - aFontWeight;
        } else {
            // special case - target is between 400 and 500

            // font weights between 400 and 500 are close
            if (aFontWeight >= 400 && aFontWeight <= 500) {
                if (aFontWeight < aTargetWeight) {
                    distance = 500 - aFontWeight;
                } else {
                    distance = aFontWeight - aTargetWeight;
                }
            } else {
                // font weights outside use rule for target weights < 400 with
                // added distance to separate from font weights in
                // the [400..500] range
                distance = aTargetWeight - aFontWeight;
                addedDistance = 100;
            }
        }
        if (distance < 0) {
            distance = -distance + NON_DESIRED_DIRECTION_DISTANCE;
        }
        distance += addedDistance;
    }
    return uint32_t(distance);
}

void
gfxFontFamily::FindAllFontsForStyle(const gfxFontStyle& aFontStyle,
                                    nsTArray<gfxFontEntry*>& aFontEntryList,
                                    bool& aNeedsSyntheticBold)
{
    if (!mHasStyles) {
        FindStyleVariations(); // collect faces for the family, if not already done
    }

    NS_ASSERTION(mAvailableFonts.Length() > 0, "font family with no faces!");
    NS_ASSERTION(aFontEntryList.IsEmpty(), "non-empty fontlist passed in");

    aNeedsSyntheticBold = false;

    int8_t baseWeight = aFontStyle.ComputeWeight();
    bool wantBold = baseWeight >= 6;
    gfxFontEntry *fe = nullptr;

    // If the family has only one face, we simply return it; no further
    // checking needed
    uint32_t count = mAvailableFonts.Length();
    if (count == 1) {
        fe = mAvailableFonts[0];
        aNeedsSyntheticBold =
            wantBold && !fe->IsBold() && aFontStyle.allowSyntheticWeight;
        aFontEntryList.AppendElement(fe);
        return;
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
        fe = mAvailableFonts[faceIndex];
        if (fe) {
            // no need to set aNeedsSyntheticBold here as we matched the boldness request
            aFontEntryList.AppendElement(fe);
            return;
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
                aNeedsSyntheticBold =
                    wantBold && !fe->IsBold() &&
                    aFontStyle.allowSyntheticWeight;
                aFontEntryList.AppendElement(fe);
                return;
            }
        }

        // this can't happen unless we have totally broken the font-list manager!
        NS_NOTREACHED("no face found in simple font family!");
    }

    // Pick the font(s) that are closest to the desired weight, style, and
    // stretch. Iterate over all fonts, measuring the weight/style distance.
    // Because of unicode-range values, there may be more than one font for a
    // given but the 99% use case is only a single font entry per
    // weight/style/stretch distance value. To optimize this, only add entries
    // to the matched font array when another entry already has the same
    // weight/style/stretch distance and add the last matched font entry. For
    // normal platform fonts with a single font entry for each
    // weight/style/stretch combination, only the last matched font entry will
    // be added.

    uint32_t minDistance = 0xffffffff;
    gfxFontEntry* matched = nullptr;
    // iterate in reverse order so that the last-defined font is the first one
    // in the fontlist used for matching, as per CSS Fonts spec
    for (int32_t i = count - 1; i >= 0; i--) {
        fe = mAvailableFonts[i];
        uint32_t distance =
            WeightDistance(aFontStyle.weight, fe->Weight()) +
            (StyleStretchDistance(fe, wantItalic, aFontStyle.stretch) *
             MAX_WEIGHT_DISTANCE);
        if (distance < minDistance) {
            matched = fe;
            if (!aFontEntryList.IsEmpty()) {
                aFontEntryList.Clear();
            }
            minDistance = distance;
        } else if (distance == minDistance) {
            if (matched) {
                aFontEntryList.AppendElement(matched);
            }
            matched = fe;
        }
    }

    NS_ASSERTION(matched, "didn't match a font within a family");

    if (matched) {
        aFontEntryList.AppendElement(matched);
        if (!matched->IsBold() && aFontStyle.weight >= 600 &&
            aFontStyle.allowSyntheticWeight) {
            aNeedsSyntheticBold = true;
        }
    }
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

#ifdef DEBUG
bool
gfxFontFamily::ContainsFace(gfxFontEntry* aFontEntry) {
    uint32_t i, numFonts = mAvailableFonts.Length();
    for (i = 0; i < numFonts; i++) {
        if (mAvailableFonts[i] == aFontEntry) {
            return true;
        }
        // userfonts contain the actual real font entry
        if (mAvailableFonts[i] && mAvailableFonts[i]->mIsUserFontContainer) {
            gfxUserFontEntry* ufe =
                static_cast<gfxUserFontEntry*>(mAvailableFonts[i].get());
            if (ufe->GetPlatformFontEntry() == aFontEntry) {
                return true;
            }
        }
    }
    return false;
}
#endif

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
    gfxFontEntry *fe =
        FindFontForStyle(aMatchData->mStyle ? *aMatchData->mStyle
                                            : gfxFontStyle(),
                         needsBold);

    if (fe && !fe->SkipDuringSystemFallback()) {
        int32_t rank = 0;

        if (fe->HasCharacter(aMatchData->mCh)) {
            rank += RANK_MATCHED_CMAP;
            aMatchData->mCount++;

            PRLogModuleInfo *log = gfxPlatform::GetLog(eGfxLog_textrun);

            if (MOZ_UNLIKELY(MOZ_LOG_TEST(log, LogLevel::Debug))) {
                uint32_t unicodeRange = FindCharUnicodeRange(aMatchData->mCh);
                uint32_t script = GetScriptCode(aMatchData->mCh);
                MOZ_LOG(log, LogLevel::Debug,\
                       ("(textrun-systemfallback-fonts) char: u+%6.6x "
                        "unicode-range: %d script: %d match: [%s]\n",
                        aMatchData->mCh,
                        unicodeRange, script,
                        NS_ConvertUTF16toUTF8(fe->Name()).get()));
            }
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
        if (fe && fe->HasCharacter(aMatchData->mCh)) {
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
        if (!fe || fe->mIsUserFontContainer) {
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
        mAvailableFonts.ShallowSizeOfExcludingThis(aMallocSizeOf);
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

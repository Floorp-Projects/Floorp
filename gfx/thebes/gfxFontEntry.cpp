/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxFontEntry.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/MathAlgorithms.h"

#include "mozilla/Logging.h"

#include "gfxTextRun.h"
#include "gfxPlatform.h"
#include "nsGkAtoms.h"

#include "gfxTypes.h"
#include "gfxContext.h"
#include "gfxFontConstants.h"
#include "gfxGraphiteShaper.h"
#include "gfxHarfBuzzShaper.h"
#include "gfxUserFontSet.h"
#include "gfxPlatformFontList.h"
#include "nsUnicodeProperties.h"
#include "nsMathUtils.h"
#include "nsBidiUtils.h"
#include "nsStyleConsts.h"
#include "mozilla/AppUnits.h"
#include "mozilla/FloatingPoint.h"
#ifdef MOZ_WASM_SANDBOXING_GRAPHITE
#  include "mozilla/ipc/LibrarySandboxPreload.h"
#endif
#include "mozilla/Likely.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Preferences.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/Telemetry.h"
#include "gfxSVGGlyphs.h"
#include "gfx2DGlue.h"

#include "harfbuzz/hb.h"
#include "harfbuzz/hb-ot.h"
#include "graphite2/Font.h"

#include "ThebesRLBox.h"

#include <algorithm>

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::unicode;
using mozilla::services::GetObserverService;

void gfxCharacterMap::NotifyReleased() {
  if (mShared) {
    gfxPlatformFontList::PlatformFontList()->RemoveCmap(this);
  }
  delete this;
}

gfxFontEntry::gfxFontEntry(const nsACString& aName, bool aIsStandardFace)
    : mName(aName),
      mLock("gfxFontEntry lock"),
      mFeatureInfoLock("gfxFontEntry featureInfo mutex"),
      mFixedPitch(false),
      mIsBadUnderlineFont(false),
      mIsUserFontContainer(false),
      mIsDataUserFont(false),
      mIsLocalUserFont(false),
      mStandardFace(aIsStandardFace),
      mIgnoreGDEF(false),
      mIgnoreGSUB(false),
      mSkipDefaultFeatureSpaceCheck(false),
      mSVGInitialized(false),
      mHasSpaceFeaturesInitialized(false),
      mHasSpaceFeatures(false),
      mHasSpaceFeaturesKerning(false),
      mHasSpaceFeaturesNonKerning(false),
      mGraphiteSpaceContextualsInitialized(false),
      mHasGraphiteSpaceContextuals(false),
      mSpaceGlyphIsInvisible(false),
      mSpaceGlyphIsInvisibleInitialized(false),
      mHasGraphiteTables(false),
      mCheckedForGraphiteTables(false),
      mHasCmapTable(false),
      mGrFaceInitialized(false),
      mCheckedForColorGlyph(false),
      mCheckedForVariationAxes(false),
      mHasColorBitmapTable(false),
      mCheckedForColorBitmapTables(false) {
  mTrakTable.exchange(kTrakTableUninitialized);
  memset(&mDefaultSubSpaceFeatures, 0, sizeof(mDefaultSubSpaceFeatures));
  memset(&mNonDefaultSubSpaceFeatures, 0, sizeof(mNonDefaultSubSpaceFeatures));
}

gfxFontEntry::~gfxFontEntry() {
  // Should not be dropped by stylo
  MOZ_ASSERT(NS_IsMainThread());

  hb_blob_destroy(mCOLR.exchange(nullptr));
  hb_blob_destroy(mCPAL.exchange(nullptr));

  if (TrakTableInitialized()) {
    // Only if it was initialized, so that we don't try to call hb_blob_destroy
    // on the kTrakTableUninitialized flag value!
    hb_blob_destroy(mTrakTable.exchange(nullptr));
  }

  // For downloaded fonts, we need to tell the user font cache that this
  // entry is being deleted.
  if (mIsDataUserFont) {
    gfxUserFontSet::UserFontCache::ForgetFont(this);
  }

  if (mFeatureInputs) {
    for (auto iter = mFeatureInputs->Iter(); !iter.Done(); iter.Next()) {
      hb_set_t*& set = iter.Data();
      hb_set_destroy(set);
    }
  }

  delete mFontTableCache.exchange(nullptr);
  delete mSVGGlyphs.exchange(nullptr);
  delete[] mUVSData.exchange(nullptr);

  gfxCharacterMap* cmap = mCharacterMap.exchange(nullptr);
  NS_IF_RELEASE(cmap);

  // By the time the entry is destroyed, all font instances that were
  // using it should already have been deleted, and so the HB and/or Gr
  // face objects should have been released.
  MOZ_ASSERT(!mHBFace);
  MOZ_ASSERT(!mGrFaceInitialized);
}

// Only used during initialization, before any other thread has a chance to see
// the entry, so locking not required.
void gfxFontEntry::InitializeFrom(fontlist::Face* aFace,
                                  const fontlist::Family* aFamily) {
  mStyleRange = aFace->mStyle;
  mWeightRange = aFace->mWeight;
  mStretchRange = aFace->mStretch;
  mFixedPitch = aFace->mFixedPitch;
  mIsBadUnderlineFont = aFamily->IsBadUnderlineFamily();
  mShmemFace = aFace;
  auto* list = gfxPlatformFontList::PlatformFontList()->SharedFontList();
  mFamilyName = aFamily->DisplayName().AsString(list);
  mHasCmapTable = TrySetShmemCharacterMap();
}

bool gfxFontEntry::TrySetShmemCharacterMap() {
  MOZ_ASSERT(mShmemFace);
  auto list = gfxPlatformFontList::PlatformFontList()->SharedFontList();
  const auto* shmemCmap =
      static_cast<const SharedBitSet*>(mShmemFace->mCharacterMap.ToPtr(list));
  mShmemCharacterMap.exchange(shmemCmap);
  return shmemCmap != nullptr;
}

bool gfxFontEntry::TestCharacterMap(uint32_t aCh) {
  if (!mCharacterMap && !mShmemCharacterMap) {
    ReadCMAP();
    MOZ_ASSERT(mCharacterMap || mShmemCharacterMap,
               "failed to initialize character map");
  }
  return mShmemCharacterMap ? GetShmemCharacterMap()->test(aCh)
                            : GetCharacterMap()->test(aCh);
}

void gfxFontEntry::EnsureUVSMapInitialized() {
  // mUVSOffset will not be initialized
  // until cmap is initialized.
  if (!mCharacterMap && !mShmemCharacterMap) {
    ReadCMAP();
    NS_ASSERTION(mCharacterMap || mShmemCharacterMap,
                 "failed to initialize character map");
  }

  if (!mUVSOffset) {
    return;
  }

  if (!mUVSData) {
    nsresult rv = NS_ERROR_NOT_AVAILABLE;
    const uint32_t kCmapTag = TRUETYPE_TAG('c', 'm', 'a', 'p');
    AutoTable cmapTable(this, kCmapTag);
    if (cmapTable) {
      const uint8_t* uvsData = nullptr;
      unsigned int cmapLen;
      const char* cmapData = hb_blob_get_data(cmapTable, &cmapLen);
      rv = gfxFontUtils::ReadCMAPTableFormat14(
          (const uint8_t*)cmapData + mUVSOffset, cmapLen - mUVSOffset, uvsData);
      if (NS_SUCCEEDED(rv)) {
        if (!mUVSData.compareExchange(nullptr, uvsData)) {
          delete uvsData;
        }
      }
    }
    if (NS_FAILED(rv)) {
      mUVSOffset = 0;  // don't try to read the table again
    }
  }
}

uint16_t gfxFontEntry::GetUVSGlyph(uint32_t aCh, uint32_t aVS) {
  EnsureUVSMapInitialized();

  if (const auto* uvsData = GetUVSData()) {
    return gfxFontUtils::MapUVSToGlyphFormat14(uvsData, aCh, aVS);
  }

  return 0;
}

bool gfxFontEntry::SupportsScriptInGSUB(const hb_tag_t* aScriptTags,
                                        uint32_t aNumTags) {
  hb_face_t* face = GetHBFace();
  if (!face) {
    return false;
  }

  unsigned int index;
  hb_tag_t chosenScript;
  bool found = hb_ot_layout_table_select_script(
      face, TRUETYPE_TAG('G', 'S', 'U', 'B'), aNumTags, aScriptTags, &index,
      &chosenScript);
  hb_face_destroy(face);

  return found && chosenScript != TRUETYPE_TAG('D', 'F', 'L', 'T');
}

nsresult gfxFontEntry::ReadCMAP(FontInfoData* aFontInfoData) {
  MOZ_ASSERT(false, "using default no-op implementation of ReadCMAP");
  RefPtr<gfxCharacterMap> cmap = new gfxCharacterMap();
  if (mCharacterMap.compareExchange(nullptr, cmap.get())) {
    Unused << cmap.forget();  // mCharacterMap now owns the reference
  }
  return NS_OK;
}

nsCString gfxFontEntry::RealFaceName() {
  AutoTable nameTable(this, TRUETYPE_TAG('n', 'a', 'm', 'e'));
  if (nameTable) {
    nsAutoCString name;
    nsresult rv = gfxFontUtils::GetFullNameFromTable(nameTable, name);
    if (NS_SUCCEEDED(rv)) {
      return std::move(name);
    }
  }
  return Name();
}

gfxFont* gfxFontEntry::FindOrMakeFont(const gfxFontStyle* aStyle,
                                      gfxCharacterMap* aUnicodeRangeMap) {
  gfxFont* font =
      gfxFontCache::GetCache()->Lookup(this, aStyle, aUnicodeRangeMap);

  if (!font) {
    gfxFont* newFont = CreateFontInstance(aStyle);
    if (!newFont) {
      return nullptr;
    }
    if (!newFont->Valid()) {
      delete newFont;
      return nullptr;
    }
    font = newFont;
    font->SetUnicodeRangeMap(aUnicodeRangeMap);
    gfxFontCache::GetCache()->AddNew(font);
  }
  return font;
}

uint16_t gfxFontEntry::UnitsPerEm() {
  if (!mUnitsPerEm) {
    AutoTable headTable(this, TRUETYPE_TAG('h', 'e', 'a', 'd'));
    if (headTable) {
      uint32_t len;
      const HeadTable* head =
          reinterpret_cast<const HeadTable*>(hb_blob_get_data(headTable, &len));
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

bool gfxFontEntry::HasSVGGlyph(uint32_t aGlyphId) {
  NS_ASSERTION(mSVGInitialized,
               "SVG data has not yet been loaded. TryGetSVGData() first.");
  return GetSVGGlyphs()->HasSVGGlyph(aGlyphId);
}

bool gfxFontEntry::GetSVGGlyphExtents(DrawTarget* aDrawTarget,
                                      uint32_t aGlyphId, gfxFloat aSize,
                                      gfxRect* aResult) {
  MOZ_ASSERT(mSVGInitialized,
             "SVG data has not yet been loaded. TryGetSVGData() first.");
  MOZ_ASSERT(mUnitsPerEm >= kMinUPEM && mUnitsPerEm <= kMaxUPEM,
             "font has invalid unitsPerEm");

  gfxMatrix svgToApp(aSize / mUnitsPerEm, 0, 0, aSize / mUnitsPerEm, 0, 0);
  return GetSVGGlyphs()->GetGlyphExtents(aGlyphId, svgToApp, aResult);
}

void gfxFontEntry::RenderSVGGlyph(gfxContext* aContext, uint32_t aGlyphId,
                                  SVGContextPaint* aContextPaint) {
  NS_ASSERTION(mSVGInitialized,
               "SVG data has not yet been loaded. TryGetSVGData() first.");
  GetSVGGlyphs()->RenderGlyph(aContext, aGlyphId, aContextPaint);
}

bool gfxFontEntry::TryGetSVGData(const gfxFont* aFont) {
  if (!gfxPlatform::GetPlatform()->OpenTypeSVGEnabled()) {
    return false;
  }

  // We don't support SVG-in-OT glyphs in offscreen-canvas worker threads.
  if (!NS_IsMainThread()) {
    return false;
  }

  if (!mSVGInitialized) {
    // If UnitsPerEm is not known/valid, we can't use SVG glyphs
    if (UnitsPerEm() == kInvalidUPEM) {
      mSVGInitialized = true;
      return false;
    }

    // We don't use AutoTable here because we'll pass ownership of this
    // blob to the gfxSVGGlyphs, once we've confirmed the table exists
    hb_blob_t* svgTable = GetFontTable(TRUETYPE_TAG('S', 'V', 'G', ' '));
    if (!svgTable) {
      mSVGInitialized = true;
      return false;
    }

    // gfxSVGGlyphs will hb_blob_destroy() the table when it is finished
    // with it.
    auto* svgGlyphs = new gfxSVGGlyphs(svgTable, this);
    if (!mSVGGlyphs.compareExchange(nullptr, svgGlyphs)) {
      delete svgGlyphs;
    }
    mSVGInitialized = true;
  }

  if (GetSVGGlyphs()) {
    AutoWriteLock lock(mLock);
    if (!mFontsUsingSVGGlyphs.Contains(aFont)) {
      mFontsUsingSVGGlyphs.AppendElement(aFont);
    }
  }

  return !!GetSVGGlyphs();
}

void gfxFontEntry::NotifyFontDestroyed(gfxFont* aFont) {
  AutoWriteLock lock(mLock);
  mFontsUsingSVGGlyphs.RemoveElement(aFont);
}

void gfxFontEntry::NotifyGlyphsChanged() {
  AutoReadLock lock(mLock);
  for (uint32_t i = 0, count = mFontsUsingSVGGlyphs.Length(); i < count; ++i) {
    const gfxFont* font = mFontsUsingSVGGlyphs[i];
    font->NotifyGlyphsChanged();
  }
}

bool gfxFontEntry::TryGetColorGlyphs() {
  if (mCheckedForColorGlyph) {
    return mCOLR && mCPAL;
  }

  auto* colr = GetFontTable(TRUETYPE_TAG('C', 'O', 'L', 'R'));
  auto* cpal = colr ? GetFontTable(TRUETYPE_TAG('C', 'P', 'A', 'L')) : nullptr;

  if (colr && cpal && gfxFontUtils::ValidateColorGlyphs(colr, cpal)) {
    if (!mCOLR.compareExchange(nullptr, colr)) {
      hb_blob_destroy(colr);
    }
    if (!mCPAL.compareExchange(nullptr, cpal)) {
      hb_blob_destroy(cpal);
    }
  } else {
    hb_blob_destroy(colr);
    hb_blob_destroy(cpal);
  }

  mCheckedForColorGlyph = true;
  return mCOLR && mCPAL;
}

/**
 * FontTableBlobData
 *
 * See FontTableHashEntry for the general strategy.
 */

class gfxFontEntry::FontTableBlobData {
 public:
  explicit FontTableBlobData(nsTArray<uint8_t>&& aBuffer)
      : mTableData(std::move(aBuffer)), mHashtable(nullptr), mHashKey(0) {
    MOZ_COUNT_CTOR(FontTableBlobData);
  }

  ~FontTableBlobData() {
    MOZ_COUNT_DTOR(FontTableBlobData);
    if (mHashtable && mHashKey) {
      mHashtable->RemoveEntry(mHashKey);
    }
  }

  // Useful for creating blobs
  const char* GetTable() const {
    return reinterpret_cast<const char*>(mTableData.Elements());
  }
  uint32_t GetTableLength() const { return mTableData.Length(); }

  // Tell this FontTableBlobData to remove the HashEntry when this is
  // destroyed.
  void ManageHashEntry(nsTHashtable<FontTableHashEntry>* aHashtable,
                       uint32_t aHashKey) {
    mHashtable = aHashtable;
    mHashKey = aHashKey;
  }

  // Disconnect from the HashEntry (because the blob has already been
  // removed from the hashtable).
  void ForgetHashEntry() {
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
  // The font table data block
  nsTArray<uint8_t> mTableData;

  // The blob destroy function needs to know the owning hashtable
  // and the hashtable key, so that it can remove the entry.
  nsTHashtable<FontTableHashEntry>* mHashtable;
  uint32_t mHashKey;

  // not implemented
  FontTableBlobData(const FontTableBlobData&);
};

hb_blob_t* gfxFontEntry::FontTableHashEntry::ShareTableAndGetBlob(
    nsTArray<uint8_t>&& aTable, nsTHashtable<FontTableHashEntry>* aHashtable) {
  Clear();
  // adopts elements of aTable
  mSharedBlobData = new FontTableBlobData(std::move(aTable));

  mBlob = hb_blob_create(
      mSharedBlobData->GetTable(), mSharedBlobData->GetTableLength(),
      HB_MEMORY_MODE_READONLY, mSharedBlobData, DeleteFontTableBlobData);
  if (mBlob == hb_blob_get_empty()) {
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

void gfxFontEntry::FontTableHashEntry::Clear() {
  // If the FontTableBlobData is managing the hash entry, then the blob is
  // not owned by this HashEntry; otherwise there is strong reference to the
  // blob that must be removed.
  if (mSharedBlobData) {
    mSharedBlobData->ForgetHashEntry();
    mSharedBlobData = nullptr;
  } else {
    hb_blob_destroy(mBlob);
  }
  mBlob = nullptr;
}

// a hb_destroy_func for hb_blob_create

/* static */
void gfxFontEntry::FontTableHashEntry::DeleteFontTableBlobData(
    void* aBlobData) {
  delete static_cast<FontTableBlobData*>(aBlobData);
}

hb_blob_t* gfxFontEntry::FontTableHashEntry::GetBlob() const {
  return hb_blob_reference(mBlob);
}

bool gfxFontEntry::GetExistingFontTable(uint32_t aTag, hb_blob_t** aBlob) {
  // Accessing the mFontTableCache pointer is atomic, so we don't need to take
  // a write lock even if we're initializing it here...
  PUSH_IGNORE_THREAD_SAFETY
  if (MOZ_UNLIKELY(!mFontTableCache)) {
    // We do this here rather than on fontEntry construction
    // because not all shapers will access the table cache at all.
    //
    // We're not holding a write lock, so make sure to atomically update
    // the cache pointer.
    auto* newCache = new FontTableCache(8);
    if (MOZ_UNLIKELY(!mFontTableCache.compareExchange(nullptr, newCache))) {
      delete newCache;
    }
  }
  FontTableCache* cache = GetFontTableCache();
  POP_THREAD_SAFETY

  // ...but we do need a lock to read the actual hashtable contents.
  AutoReadLock lock(mLock);
  FontTableHashEntry* entry = cache->GetEntry(aTag);
  if (!entry) {
    return false;
  }

  *aBlob = entry->GetBlob();
  return true;
}

hb_blob_t* gfxFontEntry::ShareFontTableAndGetBlob(uint32_t aTag,
                                                  nsTArray<uint8_t>* aBuffer) {
  PUSH_IGNORE_THREAD_SAFETY
  if (MOZ_UNLIKELY(!mFontTableCache)) {
    auto* newCache = new FontTableCache(8);
    if (MOZ_UNLIKELY(!mFontTableCache.compareExchange(nullptr, newCache))) {
      delete newCache;
    }
  }
  FontTableCache* cache = GetFontTableCache();
  POP_THREAD_SAFETY

  AutoWriteLock lock(mLock);
  FontTableHashEntry* entry = cache->PutEntry(aTag);
  if (MOZ_UNLIKELY(!entry)) {  // OOM
    return nullptr;
  }

  if (!aBuffer) {
    // ensure the entry is null
    entry->Clear();
    return nullptr;
  }

  return entry->ShareTableAndGetBlob(std::move(*aBuffer), cache);
}

already_AddRefed<gfxCharacterMap> gfxFontEntry::GetCMAPFromFontInfo(
    FontInfoData* aFontInfoData, uint32_t& aUVSOffset) {
  if (!aFontInfoData || !aFontInfoData->mLoadCmaps) {
    return nullptr;
  }

  return aFontInfoData->GetCMAP(mName, aUVSOffset);
}

hb_blob_t* gfxFontEntry::GetFontTable(uint32_t aTag) {
  hb_blob_t* blob;
  if (GetExistingFontTable(aTag, &blob)) {
    return blob;
  }

  nsTArray<uint8_t> buffer;
  bool haveTable = NS_SUCCEEDED(CopyFontTable(aTag, buffer));

  return ShareFontTableAndGetBlob(aTag, haveTable ? &buffer : nullptr);
}

// callback for HarfBuzz to get a font table (in hb_blob_t form)
// from the font entry (passed as aUserData)
/*static*/
hb_blob_t* gfxFontEntry::HBGetTable(hb_face_t* face, uint32_t aTag,
                                    void* aUserData) {
  gfxFontEntry* fontEntry = static_cast<gfxFontEntry*>(aUserData);

  // bug 589682 - ignore the GDEF table in buggy fonts (applies to
  // Italic and BoldItalic faces of Times New Roman)
  if (aTag == TRUETYPE_TAG('G', 'D', 'E', 'F') && fontEntry->IgnoreGDEF()) {
    return nullptr;
  }

  // bug 721719 - ignore the GSUB table in buggy fonts (applies to Roboto,
  // at least on some Android ICS devices; set in gfxFT2FontList.cpp)
  if (aTag == TRUETYPE_TAG('G', 'S', 'U', 'B') && fontEntry->IgnoreGSUB()) {
    return nullptr;
  }

  return fontEntry->GetFontTable(aTag);
}

/*static*/
void gfxFontEntry::HBFaceDeletedCallback(void* aUserData) {
  gfxFontEntry* fe = static_cast<gfxFontEntry*>(aUserData);
  fe->ForgetHBFace();
}

void gfxFontEntry::ForgetHBFace() { mHBFace.exchange(nullptr); }

hb_face_t* gfxFontEntry::GetHBFace() {
  if (!mHBFace) {
    hb_face_t* face =
        hb_face_create_for_tables(HBGetTable, this, HBFaceDeletedCallback);
    if (mHBFace.compareExchange(nullptr, face)) {
      return face;
    }
    hb_face_destroy(face);
  }
  return hb_face_reference(mHBFace);
}

struct gfxFontEntry::GrSandboxData {
  rlbox_sandbox_gr sandbox;
  sandbox_callback_gr<const void* (*)(const void*, unsigned int, unsigned int*)>
      grGetTableCallback;
  sandbox_callback_gr<void (*)(const void*, const void*)>
      grReleaseTableCallback;
  // Text Shapers register a callback to get glyph advances
  sandbox_callback_gr<float (*)(const void*, uint16_t)>
      grGetGlyphAdvanceCallback;

  GrSandboxData() {
    sandbox.create_sandbox();
    grGetTableCallback = sandbox.register_callback(GrGetTable);
    grReleaseTableCallback = sandbox.register_callback(GrReleaseTable);
    grGetGlyphAdvanceCallback =
        sandbox.register_callback(gfxGraphiteShaper::GrGetAdvance);
  }

  ~GrSandboxData() {
    grGetTableCallback.unregister();
    grReleaseTableCallback.unregister();
    grGetGlyphAdvanceCallback.unregister();
    sandbox.destroy_sandbox();
  }
};

static thread_local gfxFontEntry* tl_grGetFontTableCallbackData = nullptr;

/*static*/
tainted_opaque_gr<const void*> gfxFontEntry::GrGetTable(
    rlbox_sandbox_gr& sandbox,
    tainted_opaque_gr<const void*> /* aAppFaceHandle */,
    tainted_opaque_gr<unsigned int> aName,
    tainted_opaque_gr<unsigned int*> aLen) {
  gfxFontEntry* fontEntry = tl_grGetFontTableCallbackData;
  tainted_gr<unsigned int*> t_aLen = rlbox::from_opaque(aLen);
  *t_aLen = 0;
  tainted_gr<const void*> ret = nullptr;

  if (fontEntry) {
    unsigned int fontTableKey =
        rlbox::from_opaque(aName).unverified_safe_because(
            "This is only being used to index into a hashmap, which is robust "
            "for any value. No checks needed.");
    hb_blob_t* blob = fontEntry->GetFontTable(fontTableKey);

    if (blob) {
      unsigned int blobLength;
      const void* tableData = hb_blob_get_data(blob, &blobLength);
      // tableData is read-only data shared with the sandbox.
      // Making a copy in sandbox memory
      tainted_gr<void*> t_tableData = rlbox::sandbox_reinterpret_cast<void*>(
          sandbox.malloc_in_sandbox<char>(blobLength));
      if (t_tableData) {
        rlbox::memcpy(sandbox, t_tableData, tableData, blobLength);
        *t_aLen = blobLength;
        ret = rlbox::sandbox_const_cast<const void*>(t_tableData);
      }
      hb_blob_destroy(blob);
    }
  }

  return ret.to_opaque();
}

/*static*/
void gfxFontEntry::GrReleaseTable(
    rlbox_sandbox_gr& sandbox,
    tainted_opaque_gr<const void*> /* aAppFaceHandle */,
    tainted_opaque_gr<const void*> aTableBuffer) {
  sandbox.free_in_sandbox(rlbox::from_opaque(aTableBuffer));
}

rlbox_sandbox_gr* gfxFontEntry::GetGrSandbox() {
  AutoReadLock lock(mLock);
  MOZ_ASSERT(mSandboxData != nullptr);
  return &mSandboxData->sandbox;
}

sandbox_callback_gr<float (*)(const void*, uint16_t)>*
gfxFontEntry::GetGrSandboxAdvanceCallbackHandle() {
  AutoReadLock lock(mLock);
  MOZ_ASSERT(mSandboxData != nullptr);
  return &mSandboxData->grGetGlyphAdvanceCallback;
}

tainted_opaque_gr<gr_face*> gfxFontEntry::GetGrFace() {
  if (!mGrFaceInitialized) {
    // When possible, the below code will use WASM as a sandboxing mechanism.
    // At this time the wasm sandbox does not support threads.
    // If Thebes is updated to make callst to the sandbox on multiple threaads,
    // we need to make sure the underlying sandbox supports threading.
    MOZ_ASSERT(NS_IsMainThread());

    mSandboxData = new GrSandboxData();

    auto p_faceOps = mSandboxData->sandbox.malloc_in_sandbox<gr_face_ops>();
    if (!p_faceOps) {
      MOZ_CRASH("Graphite sandbox memory allocation failed");
    }
    p_faceOps->size = sizeof(*p_faceOps);
    p_faceOps->get_table = mSandboxData->grGetTableCallback;
    p_faceOps->release_table = mSandboxData->grReleaseTableCallback;

    tl_grGetFontTableCallbackData = this;
    auto face = sandbox_invoke(
        mSandboxData->sandbox, gr_make_face_with_ops,
        // For security, we do not pass the callback data to this arg, and use
        // a TLS var instead. However, gr_make_face_with_ops expects this to
        // be a non null ptr. Therefore,  we should pass some dummy non null
        // pointer which will be passed to callbacks, but never used. Let's just
        // pass p_faceOps again, as this is a non-null tainted pointer.
        p_faceOps /* appFaceHandle */, p_faceOps, gr_face_default);
    tl_grGetFontTableCallbackData = nullptr;
    mGrFace = face.to_opaque();
    mGrFaceInitialized = true;
    mSandboxData->sandbox.free_in_sandbox(p_faceOps);
  }
  ++mGrFaceRefCnt;
  return mGrFace;
}

void gfxFontEntry::ReleaseGrFace(tainted_opaque_gr<gr_face*> aFace) {
  MOZ_ASSERT(
      (rlbox::from_opaque(aFace) == rlbox::from_opaque(mGrFace))
          .unverified_safe_because(
              "This is safe as the only thing we are doing is comparing "
              "addresses of two tainted pointers. Furthermore this is used "
              "merely as a debugging aid in the debug builds. This function is "
              "called only from the trusted Firefox code rather than the "
              "untrusted libGraphite."));  // sanity-check
  MOZ_ASSERT(mGrFaceRefCnt > 0);
  if (--mGrFaceRefCnt == 0) {
    auto t_mGrFace = rlbox::from_opaque(mGrFace);

    tl_grGetFontTableCallbackData = this;
    sandbox_invoke(mSandboxData->sandbox, gr_face_destroy, t_mGrFace);
    tl_grGetFontTableCallbackData = nullptr;

    t_mGrFace = nullptr;
    mGrFace = t_mGrFace.to_opaque();

    delete mSandboxData;
    mSandboxData = nullptr;

    mGrFaceInitialized = false;
  }
}

void gfxFontEntry::DisconnectSVG() {
  if (mSVGInitialized && mSVGGlyphs) {
    mSVGGlyphs = nullptr;
    mSVGInitialized = false;
  }
}

bool gfxFontEntry::HasFontTable(uint32_t aTableTag) {
  AutoTable table(this, aTableTag);
  return table && hb_blob_get_length(table) > 0;
}

void gfxFontEntry::CheckForGraphiteTables() {
  mHasGraphiteTables = HasFontTable(TRUETYPE_TAG('S', 'i', 'l', 'f'));
}

tainted_boolean_hint gfxFontEntry::HasGraphiteSpaceContextuals() {
  if (!mGraphiteSpaceContextualsInitialized) {
    auto face = GetGrFace();
    auto t_face = rlbox::from_opaque(face);
    if (t_face) {
      tainted_gr<const gr_faceinfo*> faceInfo =
          sandbox_invoke(mSandboxData->sandbox, gr_face_info, t_face, 0);
      // Comparison with a value in sandboxed memory returns a
      // tainted_boolean_hint, i.e. a "hint", since the value could be changed
      // maliciously at any moment.
      tainted_boolean_hint is_not_none =
          faceInfo->space_contextuals != gr_faceinfo::gr_space_none;
      mHasGraphiteSpaceContextuals = is_not_none.unverified_safe_because(
          "Note ideally mHasGraphiteSpaceContextuals would be "
          "tainted_boolean_hint, but RLBox does not yet support bitfields, so "
          "it is not wrapped. However, its value is only ever accessed through "
          "this function which returns a tainted_boolean_hint, so unwrapping "
          "temporarily is safe. We remove the wrapper now and re-add it "
          "below.");
    }
    ReleaseGrFace(face);  // always balance GetGrFace, even if face is null
    mGraphiteSpaceContextualsInitialized = true;
  }

  bool ret = mHasGraphiteSpaceContextuals;
  return tainted_boolean_hint(ret);
}

#define FEATURE_SCRIPT_MASK 0x000000ff  // script index replaces low byte of tag

static_assert(int(intl::Script::NUM_SCRIPT_CODES) <= FEATURE_SCRIPT_MASK,
              "Too many script codes");

// high-order three bytes of tag with script in low-order byte
#define SCRIPT_FEATURE(s, tag)        \
  (((~FEATURE_SCRIPT_MASK) & (tag)) | \
   ((FEATURE_SCRIPT_MASK) & static_cast<uint32_t>(s)))

bool gfxFontEntry::SupportsOpenTypeFeature(Script aScript,
                                           uint32_t aFeatureTag) {
  MutexAutoLock lock(mFeatureInfoLock);
  if (!mSupportedFeatures) {
    mSupportedFeatures = MakeUnique<nsTHashMap<nsUint32HashKey, bool>>();
  }

  // note: high-order three bytes *must* be unique for each feature
  // listed below (see SCRIPT_FEATURE macro def'n)
  NS_ASSERTION(aFeatureTag == HB_TAG('s', 'm', 'c', 'p') ||
                   aFeatureTag == HB_TAG('c', '2', 's', 'c') ||
                   aFeatureTag == HB_TAG('p', 'c', 'a', 'p') ||
                   aFeatureTag == HB_TAG('c', '2', 'p', 'c') ||
                   aFeatureTag == HB_TAG('s', 'u', 'p', 's') ||
                   aFeatureTag == HB_TAG('s', 'u', 'b', 's') ||
                   aFeatureTag == HB_TAG('v', 'e', 'r', 't'),
               "use of unknown feature tag");

  // note: graphite feature support uses the last script index
  NS_ASSERTION(int(aScript) < FEATURE_SCRIPT_MASK - 1,
               "need to bump the size of the feature shift");

  uint32_t scriptFeature = SCRIPT_FEATURE(aScript, aFeatureTag);
  return mSupportedFeatures->LookupOrInsertWith(scriptFeature, [&] {
    bool result = false;
    hb_face_t* face = GetHBFace();

    if (hb_ot_layout_has_substitution(face)) {
      hb_script_t hbScript =
          gfxHarfBuzzShaper::GetHBScriptUsedForShaping(aScript);

      // Get the OpenType tag(s) that match this script code
      unsigned int scriptCount = 4;
      hb_tag_t scriptTags[4];
      hb_ot_tags_from_script_and_language(hbScript, HB_LANGUAGE_INVALID,
                                          &scriptCount, scriptTags, nullptr,
                                          nullptr);

      // Append DEFAULT to the returned tags, if room
      if (scriptCount < 4) {
        scriptTags[scriptCount++] = HB_OT_TAG_DEFAULT_SCRIPT;
      }

      // Now check for 'smcp' under the first of those scripts that is present
      const hb_tag_t kGSUB = HB_TAG('G', 'S', 'U', 'B');
      result = std::any_of(scriptTags, scriptTags + scriptCount,
                           [&](const hb_tag_t& scriptTag) {
                             unsigned int scriptIndex;
                             return hb_ot_layout_table_find_script(
                                        face, kGSUB, scriptTag, &scriptIndex) &&
                                    hb_ot_layout_language_find_feature(
                                        face, kGSUB, scriptIndex,
                                        HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX,
                                        aFeatureTag, nullptr);
                           });
    }

    hb_face_destroy(face);

    return result;
  });
}

const hb_set_t* gfxFontEntry::InputsForOpenTypeFeature(Script aScript,
                                                       uint32_t aFeatureTag) {
  MutexAutoLock lock(mFeatureInfoLock);
  if (!mFeatureInputs) {
    mFeatureInputs = MakeUnique<nsTHashMap<nsUint32HashKey, hb_set_t*>>();
  }

  NS_ASSERTION(aFeatureTag == HB_TAG('s', 'u', 'p', 's') ||
                   aFeatureTag == HB_TAG('s', 'u', 'b', 's') ||
                   aFeatureTag == HB_TAG('v', 'e', 'r', 't'),
               "use of unknown feature tag");

  uint32_t scriptFeature = SCRIPT_FEATURE(aScript, aFeatureTag);
  hb_set_t* inputGlyphs;
  if (mFeatureInputs->Get(scriptFeature, &inputGlyphs)) {
    return inputGlyphs;
  }

  inputGlyphs = hb_set_create();

  hb_face_t* face = GetHBFace();

  if (hb_ot_layout_has_substitution(face)) {
    hb_script_t hbScript =
        gfxHarfBuzzShaper::GetHBScriptUsedForShaping(aScript);

    // Get the OpenType tag(s) that match this script code
    unsigned int scriptCount = 4;
    hb_tag_t scriptTags[5];  // space for null terminator
    hb_ot_tags_from_script_and_language(hbScript, HB_LANGUAGE_INVALID,
                                        &scriptCount, scriptTags, nullptr,
                                        nullptr);

    // Append DEFAULT to the returned tags, if room
    if (scriptCount < 4) {
      scriptTags[scriptCount++] = HB_OT_TAG_DEFAULT_SCRIPT;
    }
    scriptTags[scriptCount++] = 0;

    const hb_tag_t kGSUB = HB_TAG('G', 'S', 'U', 'B');
    hb_tag_t features[2] = {aFeatureTag, HB_TAG_NONE};
    hb_set_t* featurelookups = hb_set_create();
    hb_ot_layout_collect_lookups(face, kGSUB, scriptTags, nullptr, features,
                                 featurelookups);
    hb_codepoint_t index = -1;
    while (hb_set_next(featurelookups, &index)) {
      hb_ot_layout_lookup_collect_glyphs(face, kGSUB, index, nullptr,
                                         inputGlyphs, nullptr, nullptr);
    }
    hb_set_destroy(featurelookups);
  }

  hb_face_destroy(face);

  mFeatureInputs->InsertOrUpdate(scriptFeature, inputGlyphs);
  return inputGlyphs;
}

bool gfxFontEntry::SupportsGraphiteFeature(uint32_t aFeatureTag) {
  MutexAutoLock lock(mFeatureInfoLock);

  if (!mSupportedFeatures) {
    mSupportedFeatures = MakeUnique<nsTHashMap<nsUint32HashKey, bool>>();
  }

  // note: high-order three bytes *must* be unique for each feature
  // listed below (see SCRIPT_FEATURE macro def'n)
  NS_ASSERTION(aFeatureTag == HB_TAG('s', 'm', 'c', 'p') ||
                   aFeatureTag == HB_TAG('c', '2', 's', 'c') ||
                   aFeatureTag == HB_TAG('p', 'c', 'a', 'p') ||
                   aFeatureTag == HB_TAG('c', '2', 'p', 'c') ||
                   aFeatureTag == HB_TAG('s', 'u', 'p', 's') ||
                   aFeatureTag == HB_TAG('s', 'u', 'b', 's'),
               "use of unknown feature tag");

  // graphite feature check uses the last script slot
  uint32_t scriptFeature = SCRIPT_FEATURE(FEATURE_SCRIPT_MASK, aFeatureTag);
  bool result;
  if (mSupportedFeatures->Get(scriptFeature, &result)) {
    return result;
  }

  auto face = GetGrFace();
  auto t_face = rlbox::from_opaque(face);
  result = t_face ? sandbox_invoke(mSandboxData->sandbox, gr_face_find_fref,
                                   t_face, aFeatureTag) != nullptr
                  : false;
  ReleaseGrFace(face);

  mSupportedFeatures->InsertOrUpdate(scriptFeature, result);

  return result;
}

void gfxFontEntry::GetFeatureInfo(nsTArray<gfxFontFeatureInfo>& aFeatureInfo) {
  // TODO: implement alternative code path for graphite fonts

  hb_face_t* face = GetHBFace();

  // Get the list of features for a specific <script,langSys> pair and
  // append them to aFeatureInfo.
  auto collectForLang = [=, &aFeatureInfo](
                            hb_tag_t aTableTag, unsigned int aScript,
                            hb_tag_t aScriptTag, unsigned int aLang,
                            hb_tag_t aLangTag) {
    unsigned int featCount = hb_ot_layout_language_get_feature_tags(
        face, aTableTag, aScript, aLang, 0, nullptr, nullptr);
    AutoTArray<hb_tag_t, 32> featTags;
    featTags.SetLength(featCount);
    hb_ot_layout_language_get_feature_tags(face, aTableTag, aScript, aLang, 0,
                                           &featCount, featTags.Elements());
    MOZ_ASSERT(featCount <= featTags.Length());
    // Just in case HB didn't fill featTags (i.e. in case it returned fewer
    // tags than it promised), we truncate at the length it says it filled:
    featTags.SetLength(featCount);
    for (hb_tag_t t : featTags) {
      aFeatureInfo.AppendElement(gfxFontFeatureInfo{t, aScriptTag, aLangTag});
    }
  };

  // Iterate over the language systems supported by a given script,
  // and call collectForLang for each of them.
  auto collectForScript = [=](hb_tag_t aTableTag, unsigned int aScript,
                              hb_tag_t aScriptTag) {
    collectForLang(aTableTag, aScript, aScriptTag,
                   HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX,
                   HB_TAG('d', 'f', 'l', 't'));
    unsigned int langCount = hb_ot_layout_script_get_language_tags(
        face, aTableTag, aScript, 0, nullptr, nullptr);
    AutoTArray<hb_tag_t, 32> langTags;
    langTags.SetLength(langCount);
    hb_ot_layout_script_get_language_tags(face, aTableTag, aScript, 0,
                                          &langCount, langTags.Elements());
    MOZ_ASSERT(langCount <= langTags.Length());
    langTags.SetLength(langCount);
    for (unsigned int lang = 0; lang < langCount; ++lang) {
      collectForLang(aTableTag, aScript, aScriptTag, lang, langTags[lang]);
    }
  };

  // Iterate over the scripts supported by a table (GSUB or GPOS), and call
  // collectForScript for each of them.
  auto collectForTable = [=](hb_tag_t aTableTag) {
    unsigned int scriptCount = hb_ot_layout_table_get_script_tags(
        face, aTableTag, 0, nullptr, nullptr);
    AutoTArray<hb_tag_t, 32> scriptTags;
    scriptTags.SetLength(scriptCount);
    hb_ot_layout_table_get_script_tags(face, aTableTag, 0, &scriptCount,
                                       scriptTags.Elements());
    MOZ_ASSERT(scriptCount <= scriptTags.Length());
    scriptTags.SetLength(scriptCount);
    for (unsigned int script = 0; script < scriptCount; ++script) {
      collectForScript(aTableTag, script, scriptTags[script]);
    }
  };

  // Collect all OpenType Layout features, both substitution and positioning,
  // supported by the font resource.
  collectForTable(HB_TAG('G', 'S', 'U', 'B'));
  collectForTable(HB_TAG('G', 'P', 'O', 'S'));

  hb_face_destroy(face);
}

bool gfxFontEntry::GetColorLayersInfo(
    uint32_t aGlyphId, const mozilla::gfx::DeviceColor& aDefaultColor,
    nsTArray<uint16_t>& aLayerGlyphs,
    nsTArray<mozilla::gfx::DeviceColor>& aLayerColors) {
  return gfxFontUtils::GetColorGlyphLayers(GetCOLR(), GetCPAL(), aGlyphId,
                                           aDefaultColor, aLayerGlyphs,
                                           aLayerColors);
}

typedef struct {
  AutoSwap_PRUint32 version;
  AutoSwap_PRUint16 format;
  AutoSwap_PRUint16 horizOffset;
  AutoSwap_PRUint16 vertOffset;
  AutoSwap_PRUint16 reserved;
  //  TrackData horizData;
  //  TrackData vertData;
} TrakHeader;

typedef struct {
  AutoSwap_PRUint16 nTracks;
  AutoSwap_PRUint16 nSizes;
  AutoSwap_PRUint32 sizeTableOffset;
  //  trackTableEntry trackTable[];
  //  fixed32 sizeTable[];
} TrackData;

typedef struct {
  AutoSwap_PRUint32 track;
  AutoSwap_PRUint16 nameIndex;
  AutoSwap_PRUint16 offset;
} TrackTableEntry;

bool gfxFontEntry::HasTrackingTable() {
  if (!TrakTableInitialized()) {
    hb_blob_t* trak = GetFontTable(TRUETYPE_TAG('t', 'r', 'a', 'k'));
    if (trak) {
      // mTrakTable itself is atomic, but we also want to set the auxiliary
      // pointers mTrakValues and mTrakSizeTable, so we take a lock here to
      // avoid racing with another thread also initializing the same values.
      AutoWriteLock lock(mLock);
      if (!mTrakTable.compareExchange(kTrakTableUninitialized, trak)) {
        hb_blob_destroy(trak);
      } else if (!ParseTrakTable()) {
        hb_blob_destroy(mTrakTable.exchange(nullptr));
      }
    } else {
      mTrakTable.exchange(nullptr);
    }
  }
  return GetTrakTable() != nullptr;
}

bool gfxFontEntry::ParseTrakTable() {
  // Check table validity and set up the subtable pointers we need;
  // if 'trak' table is invalid, or doesn't contain a 'normal' track,
  // return false to tell the caller not to try using it.
  unsigned int len;
  const char* data = hb_blob_get_data(GetTrakTable(), &len);
  if (len < sizeof(TrakHeader)) {
    return false;
  }
  auto trak = reinterpret_cast<const TrakHeader*>(data);
  uint16_t horizOffset = trak->horizOffset;
  if (trak->version != 0x00010000 || uint16_t(trak->format) != 0 ||
      horizOffset == 0 || uint16_t(trak->reserved) != 0) {
    return false;
  }
  // Find the horizontal trackData, and check it doesn't overrun the buffer.
  if (horizOffset > len - sizeof(TrackData)) {
    return false;
  }
  auto trackData = reinterpret_cast<const TrackData*>(data + horizOffset);
  uint16_t nTracks = trackData->nTracks;
  mNumTrakSizes = trackData->nSizes;
  if (nTracks == 0 || mNumTrakSizes < 2) {
    return false;
  }
  uint32_t sizeTableOffset = trackData->sizeTableOffset;
  // Find the trackTable, and check it doesn't overrun the buffer.
  if (horizOffset >
      len - (sizeof(TrackData) + nTracks * sizeof(TrackTableEntry))) {
    return false;
  }
  auto trackTable = reinterpret_cast<const TrackTableEntry*>(
      data + horizOffset + sizeof(TrackData));
  // Look for 'normal' tracking, bail out if no such track is present.
  unsigned trackIndex;
  for (trackIndex = 0; trackIndex < nTracks; ++trackIndex) {
    if (trackTable[trackIndex].track == 0x00000000) {
      break;
    }
  }
  if (trackIndex == nTracks) {
    return false;
  }
  // Find list of tracking values, and check they won't overrun.
  uint16_t offset = trackTable[trackIndex].offset;
  if (offset > len - mNumTrakSizes * sizeof(uint16_t)) {
    return false;
  }
  mTrakValues = reinterpret_cast<const AutoSwap_PRInt16*>(data + offset);
  // Find the size subtable, and check it doesn't overrun the buffer.
  mTrakSizeTable =
      reinterpret_cast<const AutoSwap_PRInt32*>(data + sizeTableOffset);
  if (mTrakSizeTable + mNumTrakSizes >
      reinterpret_cast<const AutoSwap_PRInt32*>(data + len)) {
    return false;
  }
  return true;
}

float gfxFontEntry::TrackingForCSSPx(float aSize) const {
  // No locking because this does read-only access of fields that are inert
  // once initialized.
  MOZ_ASSERT(TrakTableInitialized() && mTrakTable && mTrakValues &&
             mTrakSizeTable);

  // Find index of first sizeTable entry that is >= the requested size.
  int32_t fixedSize = int32_t(aSize * 65536.0);  // float -> 16.16 fixed-point
  unsigned sizeIndex;
  for (sizeIndex = 0; sizeIndex < mNumTrakSizes; ++sizeIndex) {
    if (mTrakSizeTable[sizeIndex] >= fixedSize) {
      break;
    }
  }
  // Return the tracking value for the requested size, or an interpolated
  // value if the exact size isn't found.
  if (sizeIndex == mNumTrakSizes) {
    // Request is larger than last entry in the table, so just use that.
    // (We don't attempt to extrapolate more extreme tracking values than
    // the largest or smallest present in the table.)
    return int16_t(mTrakValues[mNumTrakSizes - 1]);
  }
  if (sizeIndex == 0 || mTrakSizeTable[sizeIndex] == fixedSize) {
    // Found an exact match, or size was smaller than the first entry.
    return int16_t(mTrakValues[sizeIndex]);
  }
  // Requested size falls between two entries: interpolate value.
  double s0 = mTrakSizeTable[sizeIndex - 1] / 65536.0;  // 16.16 -> float
  double s1 = mTrakSizeTable[sizeIndex] / 65536.0;
  double t = (aSize - s0) / (s1 - s0);
  return (1.0 - t) * int16_t(mTrakValues[sizeIndex - 1]) +
         t * int16_t(mTrakValues[sizeIndex]);
}

void gfxFontEntry::SetupVariationRanges() {
  // No locking because this is done during initialization before any other
  // thread has access to the entry.
  if (!gfxPlatform::GetPlatform()->HasVariationFontSupport() ||
      !StaticPrefs::layout_css_font_variations_enabled() || !HasVariations() ||
      IsUserFont()) {
    return;
  }
  AutoTArray<gfxFontVariationAxis, 4> axes;
  GetVariationAxes(axes);
  for (const auto& axis : axes) {
    switch (axis.mTag) {
      case HB_TAG('w', 'g', 'h', 't'):
        // If the axis range looks like it doesn't fit the CSS font-weight
        // scale, we don't hook up the high-level property, and we mark
        // the face (in mRangeFlags) as having non-standard weight. This
        // means we won't map CSS font-weight to the axis. Setting 'wght'
        // with font-variation-settings will still work.
        // Strictly speaking, the min value should be checked against 1.0,
        // not 0.0, but we'll allow font makers that amount of leeway, as
        // in practice a number of fonts seem to use 0..1000.
        if (axis.mMinValue >= 0.0f && axis.mMaxValue <= 1000.0f &&
            // If axis.mMaxValue is less than the default weight we already
            // set up, assume the axis has a non-standard range (like Skia)
            // and don't try to map it.
            Weight().Min() <= FontWeight(axis.mMaxValue)) {
          if (FontWeight(axis.mDefaultValue) != Weight().Min()) {
            mStandardFace = false;
          }
          mWeightRange = WeightRange(FontWeight(std::max(1.0f, axis.mMinValue)),
                                     FontWeight(axis.mMaxValue));
        } else {
          mRangeFlags |= RangeFlags::eNonCSSWeight;
        }
        break;

      case HB_TAG('w', 'd', 't', 'h'):
        if (axis.mMinValue >= 0.0f && axis.mMaxValue <= 1000.0f &&
            Stretch().Min() <= FontStretch(axis.mMaxValue)) {
          if (FontStretch(axis.mDefaultValue) != Stretch().Min()) {
            mStandardFace = false;
          }
          mStretchRange = StretchRange(FontStretch(axis.mMinValue),
                                       FontStretch(axis.mMaxValue));
        } else {
          mRangeFlags |= RangeFlags::eNonCSSStretch;
        }
        break;

      case HB_TAG('s', 'l', 'n', 't'):
        if (axis.mMinValue >= -90.0f && axis.mMaxValue <= 90.0f) {
          if (FontSlantStyle::Oblique(axis.mDefaultValue) !=
              SlantStyle().Min()) {
            mStandardFace = false;
          }
          // OpenType and CSS measure angles in opposite directions, so we
          // have to flip signs and swap min/max when setting up the CSS
          // font-style range here.
          mStyleRange =
              SlantStyleRange(FontSlantStyle::Oblique(-axis.mMaxValue),
                              FontSlantStyle::Oblique(-axis.mMinValue));
        }
        break;

      case HB_TAG('i', 't', 'a', 'l'):
        if (axis.mMinValue <= 0.0f && axis.mMaxValue >= 1.0f) {
          if (axis.mDefaultValue != 0.0f) {
            mStandardFace = false;
          }
          mStyleRange = SlantStyleRange(FontSlantStyle::Normal(),
                                        FontSlantStyle::Italic());
        }
        break;

      default:
        continue;
    }
  }
}

void gfxFontEntry::CheckForVariationAxes() {
  if (mCheckedForVariationAxes) {
    return;
  }
  mCheckedForVariationAxes = true;
  if (HasVariations()) {
    AutoTArray<gfxFontVariationAxis, 4> axes;
    GetVariationAxes(axes);
    for (const auto& axis : axes) {
      if (axis.mTag == HB_TAG('w', 'g', 'h', 't') && axis.mMaxValue >= 600.0f) {
        mRangeFlags |= RangeFlags::eBoldVariableWeight;
      } else if (axis.mTag == HB_TAG('i', 't', 'a', 'l') &&
                 axis.mMaxValue >= 1.0f) {
        mRangeFlags |= RangeFlags::eItalicVariation;
      } else if (axis.mTag == HB_TAG('o', 'p', 's', 'z')) {
        mRangeFlags |= RangeFlags::eOpticalSize;
      }
    }
  }
}

bool gfxFontEntry::HasBoldVariableWeight() {
  MOZ_ASSERT(!mIsUserFontContainer,
             "should not be called for user-font containers!");
  CheckForVariationAxes();
  return bool(mRangeFlags & RangeFlags::eBoldVariableWeight);
}

bool gfxFontEntry::HasItalicVariation() {
  MOZ_ASSERT(!mIsUserFontContainer,
             "should not be called for user-font containers!");
  CheckForVariationAxes();
  return bool(mRangeFlags & RangeFlags::eItalicVariation);
}

bool gfxFontEntry::HasOpticalSize() {
  MOZ_ASSERT(!mIsUserFontContainer,
             "should not be called for user-font containers!");
  CheckForVariationAxes();
  return bool(mRangeFlags & RangeFlags::eOpticalSize);
}

void gfxFontEntry::GetVariationsForStyle(nsTArray<gfxFontVariation>& aResult,
                                         const gfxFontStyle& aStyle) {
  if (!gfxPlatform::GetPlatform()->HasVariationFontSupport() ||
      !StaticPrefs::layout_css_font_variations_enabled()) {
    return;
  }

  if (!HasVariations()) {
    return;
  }

  // Resolve high-level CSS properties from the requested style
  // (font-{style,weight,stretch}) to the appropriate variations.
  // The value used is clamped to the range available in the font face,
  // unless the face is a user font where no explicit descriptor was
  // given, indicated by the corresponding 'auto' range-flag.

  // We don't do these mappings if the font entry has weight and/or stretch
  // ranges that do not appear to use the CSS property scale. Some older
  // fonts created for QuickDrawGX/AAT may use "normalized" values where the
  // standard variation is 1.0 rather than 400.0 (weight) or 100.0 (stretch).

  if (!(mRangeFlags & RangeFlags::eNonCSSWeight)) {
    float weight = (IsUserFont() && (mRangeFlags & RangeFlags::eAutoWeight))
                       ? aStyle.weight.ToFloat()
                       : Weight().Clamp(aStyle.weight).ToFloat();
    aResult.AppendElement(gfxFontVariation{HB_TAG('w', 'g', 'h', 't'), weight});
  }

  if (!(mRangeFlags & RangeFlags::eNonCSSStretch)) {
    float stretch = (IsUserFont() && (mRangeFlags & RangeFlags::eAutoStretch))
                        ? aStyle.stretch.Percentage()
                        : Stretch().Clamp(aStyle.stretch).Percentage();
    aResult.AppendElement(
        gfxFontVariation{HB_TAG('w', 'd', 't', 'h'), stretch});
  }

  if (aStyle.style.IsItalic() && SupportsItalic()) {
    // The 'ital' axis is normally a binary toggle; intermediate values
    // can only be set using font-variation-settings.
    aResult.AppendElement(gfxFontVariation{HB_TAG('i', 't', 'a', 'l'), 1.0f});
  } else if (SlantStyle().Min().IsOblique()) {
    // Figure out what slant angle we should try to match from the
    // requested style.
    float angle = aStyle.style.IsNormal() ? 0.0f
                  : aStyle.style.IsItalic()
                      ? FontSlantStyle::Oblique().ObliqueAngle()
                      : aStyle.style.ObliqueAngle();
    // Clamp to the available range, unless the face is a user font
    // with no explicit descriptor.
    if (!(IsUserFont() && (mRangeFlags & RangeFlags::eAutoSlantStyle))) {
      angle = SlantStyle().Clamp(FontSlantStyle::Oblique(angle)).ObliqueAngle();
    }
    // OpenType and CSS measure angles in opposite directions, so we have to
    // invert the sign of the CSS oblique value when setting OpenType 'slnt'.
    aResult.AppendElement(gfxFontVariation{HB_TAG('s', 'l', 'n', 't'), -angle});
  }

  struct TagEquals {
    bool Equals(const gfxFontVariation& aIter, uint32_t aTag) const {
      return aIter.mTag == aTag;
    }
  };

  auto replaceOrAppend = [&aResult](const gfxFontVariation& aSetting) {
    auto index = aResult.IndexOf(aSetting.mTag, 0, TagEquals());
    if (index == aResult.NoIndex) {
      aResult.AppendElement(aSetting);
    } else {
      aResult[index].mValue = aSetting.mValue;
    }
  };

  // The low-level font-variation-settings descriptor from @font-face,
  // if present, takes precedence over automatic variation settings
  // from high-level properties.
  for (const auto& v : mVariationSettings) {
    replaceOrAppend(v);
  }

  // And the low-level font-variation-settings property takes precedence
  // over the descriptor.
  for (const auto& v : aStyle.variationSettings) {
    replaceOrAppend(v);
  }

  // If there's no explicit opsz in the settings, apply 'auto' value.
  if (HasOpticalSize() && aStyle.autoOpticalSize >= 0.0f) {
    const uint32_t kOpszTag = HB_TAG('o', 'p', 's', 'z');
    auto index = aResult.IndexOf(kOpszTag, 0, TagEquals());
    if (index == aResult.NoIndex) {
      float value = aStyle.autoOpticalSize * mSizeAdjust;
      aResult.AppendElement(gfxFontVariation{kOpszTag, value});
    }
  }
}

size_t gfxFontEntry::FontTableHashEntry::SizeOfExcludingThis(
    mozilla::MallocSizeOf aMallocSizeOf) const {
  size_t n = 0;
  if (mBlob) {
    n += aMallocSizeOf(mBlob);
  }
  if (mSharedBlobData) {
    n += mSharedBlobData->SizeOfIncludingThis(aMallocSizeOf);
  }
  return n;
}

void gfxFontEntry::AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                                          FontListSizes* aSizes) const {
  aSizes->mFontListSize += mName.SizeOfExcludingThisIfUnshared(aMallocSizeOf);

  // cmaps are shared so only non-shared cmaps are included here
  if (mCharacterMap && GetCharacterMap()->mBuildOnTheFly) {
    aSizes->mCharMapsSize +=
        GetCharacterMap()->SizeOfIncludingThis(aMallocSizeOf);
  }
  {
    AutoReadLock lock(mLock);
    if (mFontTableCache) {
      aSizes->mFontTableCacheSize +=
          GetFontTableCache()->SizeOfIncludingThis(aMallocSizeOf);
    }
  }

  // If the font has UVS data, we count that as part of the character map.
  if (mUVSData) {
    aSizes->mCharMapsSize += aMallocSizeOf(GetUVSData());
  }

  // The following, if present, are essentially cached forms of font table
  // data, so we'll accumulate them together with the basic table cache.
  if (mUserFontData) {
    aSizes->mFontTableCacheSize +=
        mUserFontData->SizeOfIncludingThis(aMallocSizeOf);
  }
  if (mSVGGlyphs) {
    aSizes->mFontTableCacheSize +=
        GetSVGGlyphs()->SizeOfIncludingThis(aMallocSizeOf);
  }

  {
    MutexAutoLock lock(mFeatureInfoLock);
    if (mSupportedFeatures) {
      aSizes->mFontTableCacheSize +=
          mSupportedFeatures->ShallowSizeOfIncludingThis(aMallocSizeOf);
    }
    if (mFeatureInputs) {
      aSizes->mFontTableCacheSize +=
          mFeatureInputs->ShallowSizeOfIncludingThis(aMallocSizeOf);
      // XXX Can't this simply be
      // aSizes->mFontTableCacheSize += 8192 * mFeatureInputs->Count();
      for (auto iter = mFeatureInputs->ConstIter(); !iter.Done(); iter.Next()) {
        // There's no API to get the real size of an hb_set, so we'll use
        // an approximation based on knowledge of the implementation.
        aSizes->mFontTableCacheSize += 8192;  // vector of 64K bits
      }
    }
  }
  // We don't include the size of mCOLR/mCPAL here, because (depending on the
  // font backend implementation) they will either wrap blocks of data owned
  // by the system (and potentially shared), or tables that are in our font
  // table cache and therefore already counted.
}

void gfxFontEntry::AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf,
                                          FontListSizes* aSizes) const {
  aSizes->mFontListSize += aMallocSizeOf(this);
  AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
}

// This is used to report the size of an individual downloaded font in the
// user font cache. (Fonts that are part of the platform font list accumulate
// their sizes to the font list's reporter using the AddSizeOf... methods
// above.)
size_t gfxFontEntry::ComputedSizeOfExcludingThis(
    MallocSizeOf aMallocSizeOf) const {
  FontListSizes s = {0};
  AddSizeOfExcludingThis(aMallocSizeOf, &s);

  // When reporting memory used for the main platform font list,
  // where we're typically summing the totals for a few hundred font faces,
  // we report the fields of FontListSizes separately.
  // But for downloaded user fonts, the actual resource data (added below)
  // will dominate, and the minor overhead of these pieces isn't worth
  // splitting out for an individual font.
  size_t result = s.mFontListSize + s.mFontTableCacheSize + s.mCharMapsSize;

  if (mIsDataUserFont) {
    MOZ_ASSERT(mComputedSizeOfUserFont > 0, "user font with no data?");
    result += mComputedSizeOfUserFont;
  }

  return result;
}

//////////////////////////////////////////////////////////////////////////////
//
// class gfxFontFamily
//
//////////////////////////////////////////////////////////////////////////////

// we consider faces with mStandardFace == true to be "less than" those with
// false, because during style matching, earlier entries are tried first
class FontEntryStandardFaceComparator {
 public:
  bool Equals(const RefPtr<gfxFontEntry>& a,
              const RefPtr<gfxFontEntry>& b) const {
    return a->mStandardFace == b->mStandardFace;
  }
  bool LessThan(const RefPtr<gfxFontEntry>& a,
                const RefPtr<gfxFontEntry>& b) const {
    return (a->mStandardFace == true && b->mStandardFace == false);
  }
};

void gfxFontFamily::SortAvailableFonts() {
  MOZ_ASSERT(mLock.LockedForWritingByCurrentThread());
  mAvailableFonts.Sort(FontEntryStandardFaceComparator());
}

bool gfxFontFamily::HasOtherFamilyNames() {
  // need to read in other family names to determine this
  if (!mOtherFamilyNamesInitialized) {
    ReadOtherFamilyNames(
        gfxPlatformFontList::PlatformFontList());  // sets mHasOtherFamilyNames
  }
  return mHasOtherFamilyNames;
}

gfxFontEntry* gfxFontFamily::FindFontForStyle(const gfxFontStyle& aFontStyle,
                                              bool aIgnoreSizeTolerance) {
  AutoTArray<gfxFontEntry*, 4> matched;
  FindAllFontsForStyle(aFontStyle, matched, aIgnoreSizeTolerance);
  if (!matched.IsEmpty()) {
    return matched[0];
  }
  return nullptr;
}

static inline double WeightStyleStretchDistance(
    gfxFontEntry* aFontEntry, const gfxFontStyle& aTargetStyle) {
  double stretchDist =
      StretchDistance(aFontEntry->Stretch(), aTargetStyle.stretch);
  double styleDist =
      StyleDistance(aFontEntry->SlantStyle(), aTargetStyle.style);
  double weightDist = WeightDistance(aFontEntry->Weight(), aTargetStyle.weight);

  // Sanity-check that the distances are within the expected range
  // (update if implementation of the distance functions is changed).
  MOZ_ASSERT(stretchDist >= 0.0 && stretchDist <= 2000.0);
  MOZ_ASSERT(styleDist >= 0.0 && styleDist <= 500.0);
  MOZ_ASSERT(weightDist >= 0.0 && weightDist <= 1600.0);

  // weight/style/stretch priority: stretch >> style >> weight
  // so we multiply the stretch and style values to make them dominate
  // the result
  return stretchDist * kStretchFactor + styleDist * kStyleFactor +
         weightDist * kWeightFactor;
}

void gfxFontFamily::FindAllFontsForStyle(
    const gfxFontStyle& aFontStyle, nsTArray<gfxFontEntry*>& aFontEntryList,
    bool aIgnoreSizeTolerance) {
  if (!mHasStyles) {
    FindStyleVariations();  // collect faces for the family, if not already
                            // done
  }

  AutoReadLock lock(mLock);

  NS_ASSERTION(mAvailableFonts.Length() > 0, "font family with no faces!");
  NS_ASSERTION(aFontEntryList.IsEmpty(), "non-empty fontlist passed in");

  gfxFontEntry* fe = nullptr;

  // If the family has only one face, we simply return it; no further
  // checking needed
  uint32_t count = mAvailableFonts.Length();
  if (count == 1) {
    fe = mAvailableFonts[0];
    aFontEntryList.AppendElement(fe);
    return;
  }

  // Most families are "simple", having just Regular/Bold/Italic/BoldItalic,
  // or some subset of these. In this case, we have exactly 4 entries in
  // mAvailableFonts, stored in the above order; note that some of the entries
  // may be nullptr. We can then pick the required entry based on whether the
  // request is for bold or non-bold, italic or non-italic, without running the
  // more complex matching algorithm used for larger families with many weights
  // and/or widths.

  if (mIsSimpleFamily) {
    // Family has no more than the "standard" 4 faces, at fixed indexes;
    // calculate which one we want.
    // Note that we cannot simply return it as not all 4 faces are necessarily
    // present.
    bool wantBold = aFontStyle.weight >= FontWeight(600);
    bool wantItalic = !aFontStyle.style.IsNormal();
    uint8_t faceIndex =
        (wantItalic ? kItalicMask : 0) | (wantBold ? kBoldMask : 0);

    // if the desired style is available, return it directly
    fe = mAvailableFonts[faceIndex];
    if (fe) {
      aFontEntryList.AppendElement(fe);
      return;
    }

    // order to check fallback faces in a simple family, depending on requested
    // style
    static const uint8_t simpleFallbacks[4][3] = {
        {kBoldFaceIndex, kItalicFaceIndex,
         kBoldItalicFaceIndex},  // fallbacks for Regular
        {kRegularFaceIndex, kBoldItalicFaceIndex, kItalicFaceIndex},  // Bold
        {kBoldItalicFaceIndex, kRegularFaceIndex, kBoldFaceIndex},    // Italic
        {kItalicFaceIndex, kBoldFaceIndex, kRegularFaceIndex}  // BoldItalic
    };
    const uint8_t* order = simpleFallbacks[faceIndex];

    for (uint8_t trial = 0; trial < 3; ++trial) {
      // check remaining faces in order of preference to find the first that
      // actually exists
      fe = mAvailableFonts[order[trial]];
      if (fe) {
        aFontEntryList.AppendElement(fe);
        return;
      }
    }

    // this can't happen unless we have totally broken the font-list manager!
    MOZ_ASSERT_UNREACHABLE("no face found in simple font family!");
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

  double minDistance = INFINITY;
  gfxFontEntry* matched = nullptr;
  // iterate in forward order so that faces like 'Bold' are matched before
  // matching style distance faces such as 'Bold Outline' (see bug 1185812)
  for (uint32_t i = 0; i < count; i++) {
    fe = mAvailableFonts[i];
    // weight/style/stretch priority: stretch >> style >> weight
    double distance = WeightStyleStretchDistance(fe, aFontStyle);
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
  }
}

void gfxFontFamily::CheckForSimpleFamily() {
  MOZ_ASSERT(mLock.LockedForWritingByCurrentThread());
  // already checked this family
  if (mIsSimpleFamily) {
    return;
  }

  uint32_t count = mAvailableFonts.Length();
  if (count > 4 || count == 0) {
    return;  // can't be "simple" if there are >4 faces;
             // if none then the family is unusable anyway
  }

  if (count == 1) {
    mIsSimpleFamily = true;
    return;
  }

  StretchRange firstStretch = mAvailableFonts[0]->Stretch();
  if (!firstStretch.IsSingle()) {
    return;  // family with variation fonts is not considered "simple"
  }

  gfxFontEntry* faces[4] = {0};
  for (uint8_t i = 0; i < count; ++i) {
    gfxFontEntry* fe = mAvailableFonts[i];
    if (fe->Stretch() != firstStretch || fe->IsOblique()) {
      // simple families don't have varying font-stretch or oblique
      return;
    }
    if (!fe->Weight().IsSingle() || !fe->SlantStyle().IsSingle()) {
      return;  // family with variation fonts is not considered "simple"
    }
    uint8_t faceIndex = (fe->IsItalic() ? kItalicMask : 0) |
                        (fe->SupportsBold() ? kBoldMask : 0);
    if (faces[faceIndex]) {
      return;  // two faces resolve to the same slot; family isn't "simple"
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
bool gfxFontFamily::ContainsFace(gfxFontEntry* aFontEntry) {
  AutoReadLock lock(mLock);

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

void gfxFontFamily::LocalizedName(nsACString& aLocalizedName) {
  // just return the primary name; subclasses should override
  aLocalizedName = mName;
}

void gfxFontFamily::FindFontForChar(GlobalFontMatch* aMatchData) {
  gfxPlatformFontList::PlatformFontList()->mLock.AssertCurrentThreadIn();

  {
    AutoReadLock lock(mLock);
    if (mFamilyCharacterMapInitialized && !TestCharacterMap(aMatchData->mCh)) {
      // none of the faces in the family support the required char,
      // so bail out immediately
      return;
    }
  }

  nsCString charAndName;
  if (profiler_thread_is_being_profiled(
          Combine(ThreadProfilingFeatures::Sampling,
                  ThreadProfilingFeatures::Markers))) {
    charAndName = nsPrintfCString("\\u%x %s", aMatchData->mCh, mName.get());
  }
  AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING("gfxFontFamily::FindFontForChar",
                                        LAYOUT, charAndName);

  AutoTArray<gfxFontEntry*, 4> entries;
  FindAllFontsForStyle(aMatchData->mStyle, entries,
                       /*aIgnoreSizeTolerance*/ true);
  if (entries.IsEmpty()) {
    return;
  }

  gfxFontEntry* fe = nullptr;
  float distance = INFINITY;

  for (auto e : entries) {
    if (e->SkipDuringSystemFallback()) {
      continue;
    }

    aMatchData->mCmapsTested++;
    if (e->HasCharacter(aMatchData->mCh)) {
      aMatchData->mCount++;

      LogModule* log = gfxPlatform::GetLog(eGfxLog_textrun);

      if (MOZ_UNLIKELY(MOZ_LOG_TEST(log, LogLevel::Debug))) {
        intl::Script script =
            intl::UnicodeProperties::GetScriptCode(aMatchData->mCh);
        MOZ_LOG(log, LogLevel::Debug,
                ("(textrun-systemfallback-fonts) char: u+%6.6x "
                 "script: %d match: [%s]\n",
                 aMatchData->mCh, int(script), e->Name().get()));
      }

      fe = e;
      distance = WeightStyleStretchDistance(fe, aMatchData->mStyle);
      if (aMatchData->mPresentation != eFontPresentation::Any) {
        RefPtr<gfxFont> font = fe->FindOrMakeFont(&aMatchData->mStyle);
        if (!font) {
          continue;
        }
        bool hasColorGlyph =
            font->HasColorGlyphFor(aMatchData->mCh, aMatchData->mNextCh);
        if (hasColorGlyph != PrefersColor(aMatchData->mPresentation)) {
          distance += kPresentationMismatch;
        }
      }
      break;
    }
  }

  if (!fe && !aMatchData->mStyle.IsNormalStyle()) {
    // If style/weight/stretch was not Normal, see if we can
    // fall back to a next-best face (e.g. Arial Black -> Bold,
    // or Arial Narrow -> Regular).
    GlobalFontMatch data(aMatchData->mCh, aMatchData->mNextCh,
                         aMatchData->mStyle, aMatchData->mPresentation);
    SearchAllFontsForChar(&data);
    if (!data.mBestMatch) {
      return;
    }
    fe = data.mBestMatch;
    distance = data.mMatchDistance;
  }

  if (!fe) {
    return;
  }

  if (distance < aMatchData->mMatchDistance ||
      (distance == aMatchData->mMatchDistance &&
       Compare(fe->Name(), aMatchData->mBestMatch->Name()) > 0)) {
    aMatchData->mBestMatch = fe;
    aMatchData->mMatchedFamily = this;
    aMatchData->mMatchDistance = distance;
  }
}

void gfxFontFamily::SearchAllFontsForChar(GlobalFontMatch* aMatchData) {
  if (!mFamilyCharacterMapInitialized) {
    ReadAllCMAPs();
  }
  AutoReadLock lock(mLock);
  if (!mFamilyCharacterMap.test(aMatchData->mCh)) {
    return;
  }
  uint32_t i, numFonts = mAvailableFonts.Length();
  for (i = 0; i < numFonts; i++) {
    gfxFontEntry* fe = mAvailableFonts[i];
    if (fe && fe->HasCharacter(aMatchData->mCh)) {
      float distance = WeightStyleStretchDistance(fe, aMatchData->mStyle);
      if (aMatchData->mPresentation != eFontPresentation::Any) {
        RefPtr<gfxFont> font = fe->FindOrMakeFont(&aMatchData->mStyle);
        if (!font) {
          continue;
        }
        bool hasColorGlyph =
            font->HasColorGlyphFor(aMatchData->mCh, aMatchData->mNextCh);
        if (hasColorGlyph != PrefersColor(aMatchData->mPresentation)) {
          distance += kPresentationMismatch;
        }
      }
      if (distance < aMatchData->mMatchDistance ||
          (distance == aMatchData->mMatchDistance &&
           Compare(fe->Name(), aMatchData->mBestMatch->Name()) > 0)) {
        aMatchData->mBestMatch = fe;
        aMatchData->mMatchedFamily = this;
        aMatchData->mMatchDistance = distance;
      }
    }
  }
}

/*virtual*/
gfxFontFamily::~gfxFontFamily() {
  // Should not be dropped by stylo, but the InitFontList thread might use
  // a transient gfxFontFamily and that's OK.
  MOZ_ASSERT(NS_IsMainThread() || gfxPlatformFontList::IsInitFontListThread());
}

// returns true if other names were found, false otherwise
bool gfxFontFamily::ReadOtherFamilyNamesForFace(
    gfxPlatformFontList* aPlatformFontList, hb_blob_t* aNameTable,
    bool useFullName) {
  uint32_t dataLength;
  const char* nameData = hb_blob_get_data(aNameTable, &dataLength);
  AutoTArray<nsCString, 4> otherFamilyNames;

  gfxFontUtils::ReadOtherFamilyNamesForFace(mName, nameData, dataLength,
                                            otherFamilyNames, useFullName);

  if (!otherFamilyNames.IsEmpty()) {
    aPlatformFontList->AddOtherFamilyNames(this, otherFamilyNames);
  }

  return !otherFamilyNames.IsEmpty();
}

void gfxFontFamily::ReadOtherFamilyNames(
    gfxPlatformFontList* aPlatformFontList) {
  AutoWriteLock lock(mLock);
  if (mOtherFamilyNamesInitialized) {
    return;
  }

  mOtherFamilyNamesInitialized = true;

  FindStyleVariationsLocked();

  // read in other family names for the first face in the list
  uint32_t i, numFonts = mAvailableFonts.Length();
  const uint32_t kNAME = TRUETYPE_TAG('n', 'a', 'm', 'e');

  for (i = 0; i < numFonts; ++i) {
    gfxFontEntry* fe = mAvailableFonts[i];
    if (!fe) {
      continue;
    }
    gfxFontEntry::AutoTable nameTable(fe, kNAME);
    if (!nameTable) {
      continue;
    }
    mHasOtherFamilyNames =
        ReadOtherFamilyNamesForFace(aPlatformFontList, nameTable);
    break;
  }

  // read in other names for the first face in the list with the assumption
  // that if extra names don't exist in that face then they don't exist in
  // other faces for the same font
  if (!mHasOtherFamilyNames) {
    return;
  }

  // read in names for all faces, needed to catch cases where fonts have
  // family names for individual weights (e.g. Hiragino Kaku Gothic Pro W6)
  for (; i < numFonts; i++) {
    gfxFontEntry* fe = mAvailableFonts[i];
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

static bool LookForLegacyFamilyName(const nsACString& aCanonicalName,
                                    const char* aNameData, uint32_t aDataLength,
                                    nsACString& aLegacyName /* outparam */) {
  const gfxFontUtils::NameHeader* nameHeader =
      reinterpret_cast<const gfxFontUtils::NameHeader*>(aNameData);

  uint32_t nameCount = nameHeader->count;
  if (nameCount * sizeof(gfxFontUtils::NameRecord) > aDataLength) {
    NS_WARNING("invalid font (name records)");
    return false;
  }

  const gfxFontUtils::NameRecord* nameRecord =
      reinterpret_cast<const gfxFontUtils::NameRecord*>(
          aNameData + sizeof(gfxFontUtils::NameHeader));
  uint32_t stringsBase = uint32_t(nameHeader->stringOffset);

  for (uint32_t i = 0; i < nameCount; i++, nameRecord++) {
    uint32_t nameLen = nameRecord->length;
    uint32_t nameOff = nameRecord->offset;

    if (stringsBase + nameOff + nameLen > aDataLength) {
      NS_WARNING("invalid font (name table strings)");
      return false;
    }

    if (uint16_t(nameRecord->nameID) == gfxFontUtils::NAME_ID_FAMILY) {
      bool ok = gfxFontUtils::DecodeFontName(
          aNameData + stringsBase + nameOff, nameLen,
          uint32_t(nameRecord->platformID), uint32_t(nameRecord->encodingID),
          uint32_t(nameRecord->languageID), aLegacyName);
      // It's only a legacy name if it case-insensitively differs from the
      // canonical name (otherwise it would map to the same key).
      if (ok && !aLegacyName.Equals(aCanonicalName,
                                    nsCaseInsensitiveCStringComparator)) {
        return true;
      }
    }
  }
  return false;
}

bool gfxFontFamily::CheckForLegacyFamilyNames(gfxPlatformFontList* aFontList) {
  aFontList->mLock.AssertCurrentThreadIn();
  if (mCheckedForLegacyFamilyNames) {
    // we already did this, so there's nothing more to add
    return false;
  }
  mCheckedForLegacyFamilyNames = true;
  bool added = false;
  const uint32_t kNAME = TRUETYPE_TAG('n', 'a', 'm', 'e');
  AutoTArray<RefPtr<gfxFontEntry>, 16> faces;
  {
    // Take a local copy of the array of font entries, because it's possible
    // AddWithLegacyFamilyName will mutate it (and it needs to be able to take
    // an exclusive lock on the family to do so, so we release the read lock
    // here).
    AutoReadLock lock(mLock);
    faces.AppendElements(mAvailableFonts);
  }
  for (const auto& fe : faces) {
    if (!fe) {
      continue;
    }
    gfxFontEntry::AutoTable nameTable(fe, kNAME);
    if (!nameTable) {
      continue;
    }
    nsAutoCString legacyName;
    uint32_t dataLength;
    const char* nameData = hb_blob_get_data(nameTable, &dataLength);
    if (LookForLegacyFamilyName(Name(), nameData, dataLength, legacyName)) {
      if (aFontList->AddWithLegacyFamilyName(legacyName, fe, mVisibility)) {
        added = true;
      }
    }
  }
  return added;
}

void gfxFontFamily::ReadFaceNames(gfxPlatformFontList* aPlatformFontList,
                                  bool aNeedFullnamePostscriptNames,
                                  FontInfoData* aFontInfoData) {
  aPlatformFontList->mLock.AssertCurrentThreadIn();

  // if all needed names have already been read, skip
  if (mOtherFamilyNamesInitialized &&
      (mFaceNamesInitialized || !aNeedFullnamePostscriptNames)) {
    return;
  }

  AutoWriteLock lock(mLock);

  bool asyncFontLoaderDisabled = false;

  if (!mOtherFamilyNamesInitialized && aFontInfoData &&
      aFontInfoData->mLoadOtherNames && !asyncFontLoaderDisabled) {
    const auto* otherFamilyNames = aFontInfoData->GetOtherFamilyNames(mName);
    if (otherFamilyNames && otherFamilyNames->Length()) {
      aPlatformFontList->AddOtherFamilyNames(this, *otherFamilyNames);
    }
    mOtherFamilyNamesInitialized = true;
  }

  // if all needed data has been initialized, return
  if (mOtherFamilyNamesInitialized &&
      (mFaceNamesInitialized || !aNeedFullnamePostscriptNames)) {
    return;
  }

  FindStyleVariationsLocked(aFontInfoData);

  // check again, as style enumeration code may have loaded names
  if (mOtherFamilyNamesInitialized &&
      (mFaceNamesInitialized || !aNeedFullnamePostscriptNames)) {
    return;
  }

  uint32_t i, numFonts = mAvailableFonts.Length();
  const uint32_t kNAME = TRUETYPE_TAG('n', 'a', 'm', 'e');

  bool firstTime = true, readAllFaces = false;
  for (i = 0; i < numFonts; ++i) {
    gfxFontEntry* fe = mAvailableFonts[i];
    if (!fe) {
      continue;
    }

    nsAutoCString fullname, psname;
    bool foundFaceNames = false;
    if (!mFaceNamesInitialized && aNeedFullnamePostscriptNames &&
        aFontInfoData && aFontInfoData->mLoadFaceNames) {
      aFontInfoData->GetFaceNames(fe->Name(), fullname, psname);
      if (!fullname.IsEmpty()) {
        aPlatformFontList->AddFullnameLocked(fe, fullname);
      }
      if (!psname.IsEmpty()) {
        aPlatformFontList->AddPostscriptNameLocked(fe, psname);
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
      if (gfxFontUtils::ReadCanonicalName(nameTable, gfxFontUtils::NAME_ID_FULL,
                                          fullname) == NS_OK) {
        aPlatformFontList->AddFullnameLocked(fe, fullname);
      }

      if (gfxFontUtils::ReadCanonicalName(
              nameTable, gfxFontUtils::NAME_ID_POSTSCRIPT, psname) == NS_OK) {
        aPlatformFontList->AddPostscriptNameLocked(fe, psname);
      }
    }

    if (!mOtherFamilyNamesInitialized && (firstTime || readAllFaces)) {
      bool foundOtherName =
          ReadOtherFamilyNamesForFace(aPlatformFontList, nameTable);

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

gfxFontEntry* gfxFontFamily::FindFont(const nsACString& aFontName,
                                      const nsCStringComparator& aCmp) const {
  // find the font using a simple linear search
  AutoReadLock lock(mLock);
  uint32_t numFonts = mAvailableFonts.Length();
  for (uint32_t i = 0; i < numFonts; i++) {
    gfxFontEntry* fe = mAvailableFonts[i].get();
    if (fe && fe->Name().Equals(aFontName, aCmp)) {
      return fe;
    }
  }
  return nullptr;
}

void gfxFontFamily::ReadAllCMAPs(FontInfoData* aFontInfoData) {
  AutoWriteLock lock(mLock);
  FindStyleVariationsLocked(aFontInfoData);

  uint32_t i, numFonts = mAvailableFonts.Length();
  for (i = 0; i < numFonts; i++) {
    gfxFontEntry* fe = mAvailableFonts[i];
    // don't try to load cmaps for downloadable fonts not yet loaded
    if (!fe || fe->mIsUserFontContainer) {
      continue;
    }
    fe->ReadCMAP(aFontInfoData);
    mFamilyCharacterMap.Union(*(fe->GetCharacterMap()));
  }
  mFamilyCharacterMap.Compact();
  mFamilyCharacterMapInitialized = true;
}

void gfxFontFamily::AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                                           FontListSizes* aSizes) const {
  AutoReadLock lock(mLock);
  aSizes->mFontListSize += mName.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  aSizes->mCharMapsSize +=
      mFamilyCharacterMap.SizeOfExcludingThis(aMallocSizeOf);

  aSizes->mFontListSize +=
      mAvailableFonts.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (uint32_t i = 0; i < mAvailableFonts.Length(); ++i) {
    gfxFontEntry* fe = mAvailableFonts[i];
    if (fe) {
      fe->AddSizeOfIncludingThis(aMallocSizeOf, aSizes);
    }
  }
}

void gfxFontFamily::AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf,
                                           FontListSizes* aSizes) const {
  aSizes->mFontListSize += aMallocSizeOf(this);
  AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
}

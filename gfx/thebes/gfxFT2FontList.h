/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FT2FONTLIST_H
#define GFX_FT2FONTLIST_H

#include "mozilla/MemoryReporting.h"
#include "gfxFT2FontBase.h"
#include "gfxPlatformFontList.h"
#include "nsTHashSet.h"

namespace mozilla {
namespace dom {
class SystemFontListEntry;
};
};  // namespace mozilla

class FontNameCache;
typedef struct FT_FaceRec_* FT_Face;
class nsZipArchive;
class WillShutdownObserver;
class FTUserFontData;

class FT2FontEntry final : public gfxFT2FontEntryBase {
  friend class gfxFT2FontList;

  using FontListEntry = mozilla::dom::SystemFontListEntry;

 public:
  explicit FT2FontEntry(const nsACString& aFaceName)
      : gfxFT2FontEntryBase(aFaceName), mFTFontIndex(0) {}

  ~FT2FontEntry();

  gfxFontEntry* Clone() const override;

  const nsCString& GetName() const { return Name(); }

  // create a font entry for a downloaded font
  static FT2FontEntry* CreateFontEntry(
      const nsACString& aFontName, WeightRange aWeight, StretchRange aStretch,
      SlantStyleRange aStyle, const uint8_t* aFontData, uint32_t aLength);

  // create a font entry representing an installed font, identified by
  // a FontListEntry; the freetype and cairo faces will not be instantiated
  // until actually needed
  static FT2FontEntry* CreateFontEntry(const FontListEntry& aFLE);

  // Create a font entry with the given name; if it is an installed font,
  // also record the filename and index.
  // If a non-null harfbuzz face is passed, also set style/weight/stretch
  // properties of the entry from the values in the face.
  static FT2FontEntry* CreateFontEntry(const nsACString& aName,
                                       const char* aFilename, uint8_t aIndex,
                                       const hb_face_t* aFace);

  gfxFont* CreateFontInstance(const gfxFontStyle* aStyle) override;

  nsresult ReadCMAP(FontInfoData* aFontInfoData = nullptr) override;

  hb_blob_t* GetFontTable(uint32_t aTableTag) override;

  bool HasFontTable(uint32_t aTableTag) override;
  nsresult CopyFontTable(uint32_t aTableTag, nsTArray<uint8_t>&) override;

  bool HasVariations() override;
  void GetVariationAxes(
      nsTArray<gfxFontVariationAxis>& aVariationAxes) override;
  void GetVariationInstances(
      nsTArray<gfxFontVariationInstance>& aInstances) override;

  // Check for various kinds of brokenness, and set flags on the entry
  // accordingly so that we avoid using bad font tables
  void CheckForBrokenFont(gfxFontFamily* aFamily);
  void CheckForBrokenFont(const nsACString& aFamilyKey);

  already_AddRefed<mozilla::gfx::SharedFTFace> GetFTFace(bool aCommit = false);
  FTUserFontData* GetUserFontData();

  FT_MM_Var* GetMMVar() override;

  // Get a harfbuzz face for this font, if possible. The caller is responsible
  // to destroy the face when no longer needed.
  // This may be a bit expensive, so avoid calling multiple times if the same
  // face can be re-used for several purposes instead.
  hb_face_t* CreateHBFace() const;

  /**
   * Append this face's metadata to aFaceList for storage in the FontNameCache
   * (for faster startup).
   * The aPSName and aFullName parameters here can in principle be empty,
   * but if they are missing for a given face then src:local() lookups will
   * not be able to find it when the shared font list is in use.
   */
  void AppendToFaceList(nsCString& aFaceList, const nsACString& aFamilyName,
                        const nsACString& aPSName, const nsACString& aFullName,
                        FontVisibility aVisibility);

  void AddSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                              FontListSizes* aSizes) const override;
  void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                              FontListSizes* aSizes) const override;

  RefPtr<mozilla::gfx::SharedFTFace> mFTFace;

  FT_MM_Var* mMMVar = nullptr;

  nsCString mFilename;
  uint8_t mFTFontIndex;

  mozilla::ThreadSafeWeakPtr<mozilla::gfx::UnscaledFontFreeType> mUnscaledFont;

  nsTHashSet<uint32_t> mAvailableTables;

  bool mHasVariations = false;
  bool mHasVariationsInitialized = false;
  bool mMMVarInitialized = false;
};

class FT2FontFamily final : public gfxFontFamily {
  using FontListEntry = mozilla::dom::SystemFontListEntry;

 public:
  explicit FT2FontFamily(const nsACString& aName, FontVisibility aVisibility)
      : gfxFontFamily(aName, aVisibility) {}

  // Append this family's faces to the IPC fontlist
  void AddFacesToFontList(nsTArray<FontListEntry>* aFontList);
};

class gfxFT2FontList final : public gfxPlatformFontList {
  using FontListEntry = mozilla::dom::SystemFontListEntry;

 public:
  gfxFT2FontList();
  virtual ~gfxFT2FontList();

  gfxFontEntry* CreateFontEntry(
      mozilla::fontlist::Face* aFace,
      const mozilla::fontlist::Family* aFamily) override;

  gfxFontEntry* LookupLocalFont(const nsACString& aFontName,
                                WeightRange aWeightForEntry,
                                StretchRange aStretchForEntry,
                                SlantStyleRange aStyleForEntry) override;

  gfxFontEntry* MakePlatformFont(const nsACString& aFontName,
                                 WeightRange aWeightForEntry,
                                 StretchRange aStretchForEntry,
                                 SlantStyleRange aStyleForEntry,
                                 const uint8_t* aFontData,
                                 uint32_t aLength) override;

  void WriteCache();

  void ReadSystemFontList(mozilla::dom::SystemFontList*);

  static gfxFT2FontList* PlatformFontList() {
    return static_cast<gfxFT2FontList*>(
        gfxPlatformFontList::PlatformFontList());
  }

  gfxFontFamily* CreateFontFamily(const nsACString& aName,
                                  FontVisibility aVisibility) const override;

  void WillShutdown();

 protected:
  typedef enum { kUnknown, kStandard } StandardFile;

  // initialize font lists
  nsresult InitFontListForPlatform() override;

  void AppendFaceFromFontListEntry(const FontListEntry& aFLE,
                                   StandardFile aStdFile);

  void AppendFacesFromBlob(const nsCString& aFileName, StandardFile aStdFile,
                           hb_blob_t* aBlob, FontNameCache* aCache,
                           uint32_t aTimestamp, uint32_t aFilesize);

  void AppendFacesFromFontFile(const nsCString& aFileName,
                               FontNameCache* aCache, StandardFile aStdFile);

  void AppendFacesFromOmnijarEntry(nsZipArchive* aReader,
                                   const nsCString& aEntryName,
                                   FontNameCache* aCache, bool aJarChanged);

  void InitSharedFontListForPlatform() override;
  void CollectInitData(const FontListEntry& aFLE, const nsCString& aPSName,
                       const nsCString& aFullName, StandardFile aStdFile);

  /**
   * Callback passed to AppendFacesFromCachedFaceList to collect family/face
   * information in either the unshared or shared list we're building.
   */
  typedef void (*CollectFunc)(const FontListEntry& aFLE,
                              const nsCString& aPSName,
                              const nsCString& aFullName,
                              StandardFile aStdFile);

  /**
   * Append faces from the face-list record for a specific file.
   * aCollectFace is a callback that will store the face(s) in either the
   * unshared mFontFamilies list or the mFamilyInitData/mFaceInitData tables
   * that will be used to initialize the shared list.
   * Returns true if it is able to read at least one face entry; false if no
   * usable face entry was found.
   */
  bool AppendFacesFromCachedFaceList(CollectFunc aCollectFace,
                                     const nsCString& aFileName,
                                     const nsCString& aFaceList,
                                     StandardFile aStdFile);

  void AddFaceToList(const nsCString& aEntryName, uint32_t aIndex,
                     StandardFile aStdFile, hb_face_t* aFace,
                     nsCString& aFaceList);

  void FindFonts();

  void FindFontsInOmnijar(FontNameCache* aCache);

  void FindFontsInDir(const nsCString& aDir, FontNameCache* aFNC);

  FontFamily GetDefaultFontForPlatform(const gfxFontStyle* aStyle,
                                       nsAtom* aLanguage = nullptr) override;

  nsTHashSet<nsCString> mSkipSpaceLookupCheckFamilies;

 private:
  mozilla::UniquePtr<FontNameCache> mFontNameCache;
  int64_t mJarModifiedTime;
  RefPtr<WillShutdownObserver> mObserver;

  nsTArray<mozilla::fontlist::Family::InitData> mFamilyInitData;
  nsClassHashtable<nsCStringHashKey,
                   nsTArray<mozilla::fontlist::Face::InitData>>
      mFaceInitData;
};

#endif /* GFX_FT2FONTLIST_H */

/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FT2FONTLIST_H
#define GFX_FT2FONTLIST_H

#include "mozilla/MemoryReporting.h"
#include "gfxPlatformFontList.h"
#include "mozilla/gfx/UnscaledFontFreeType.h"

namespace mozilla {
namespace dom {
class FontListEntry;
};
};  // namespace mozilla
using mozilla::dom::FontListEntry;

class FontNameCache;
typedef struct FT_FaceRec_* FT_Face;
class nsZipArchive;
class WillShutdownObserver;

class FT2FontEntry : public gfxFontEntry {
 public:
  explicit FT2FontEntry(const nsACString& aFaceName)
      : gfxFontEntry(aFaceName),
        mFTFace(nullptr),
        mFontFace(nullptr),
        mFTFontIndex(0) {}

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

  // Create a font entry for a given freetype face; if it is an installed font,
  // also record the filename and index.
  // aFontData (if non-nullptr) is NS_Malloc'ed data that aFace depends on,
  // to be freed after the face is destroyed.
  // aLength is the length of aFontData.
  static FT2FontEntry* CreateFontEntry(FT_Face aFace, const char* aFilename,
                                       uint8_t aIndex, const nsACString& aName,
                                       const uint8_t* aFontData = nullptr,
                                       uint32_t aLength = 0);

  gfxFont* CreateFontInstance(const gfxFontStyle* aFontStyle) override;

  // Create (if necessary) and return the cairo_font_face for this font.
  // This may fail and return null, so caller must be prepared to handle this.
  // If a style is passed, any variationSettings in the style will be applied
  // to the resulting font face.
  cairo_font_face_t* CairoFontFace(const gfxFontStyle* aStyle = nullptr);

  // Create a cairo_scaled_font for this face, with the given style.
  // This may fail and return null, so caller must be prepared to handle this.
  cairo_scaled_font_t* CreateScaledFont(const gfxFontStyle* aStyle);

  nsresult ReadCMAP(FontInfoData* aFontInfoData = nullptr) override;

  hb_blob_t* GetFontTable(uint32_t aTableTag) override;

  nsresult CopyFontTable(uint32_t aTableTag,
                         nsTArray<uint8_t>& aBuffer) override;

  bool HasVariations() override;
  void GetVariationAxes(
      nsTArray<gfxFontVariationAxis>& aVariationAxes) override;
  void GetVariationInstances(
      nsTArray<gfxFontVariationInstance>& aInstances) override;

  // Check for various kinds of brokenness, and set flags on the entry
  // accordingly so that we avoid using bad font tables
  void CheckForBrokenFont(gfxFontFamily* aFamily);

  FT_MM_Var* GetMMVar() override;

  void AddSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                              FontListSizes* aSizes) const override;
  void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                              FontListSizes* aSizes) const override;

  FT_Face mFTFace;
  cairo_font_face_t* mFontFace;

  FT_MM_Var* mMMVar = nullptr;

  nsCString mFilename;
  uint8_t mFTFontIndex;

  mozilla::ThreadSafeWeakPtr<mozilla::gfx::UnscaledFontFreeType> mUnscaledFont;

  bool mHasVariations = false;
  bool mHasVariationsInitialized = false;
  bool mMMVarInitialized = false;
};

class FT2FontFamily : public gfxFontFamily {
 public:
  explicit FT2FontFamily(const nsACString& aName) : gfxFontFamily(aName) {}

  // Append this family's faces to the IPC fontlist
  void AddFacesToFontList(InfallibleTArray<FontListEntry>* aFontList);
};

class gfxFT2FontList : public gfxPlatformFontList {
 public:
  gfxFT2FontList();
  virtual ~gfxFT2FontList();

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

  void GetSystemFontList(InfallibleTArray<FontListEntry>* retValue);

  static gfxFT2FontList* PlatformFontList() {
    return static_cast<gfxFT2FontList*>(
        gfxPlatformFontList::PlatformFontList());
  }

  void GetFontFamilyList(
      nsTArray<RefPtr<gfxFontFamily> >& aFamilyArray) override;

  gfxFontFamily* CreateFontFamily(const nsACString& aName) const override;

  void WillShutdown();

 protected:
  typedef enum { kUnknown, kStandard } StandardFile;

  // initialize font lists
  nsresult InitFontListForPlatform() override;

  void AppendFaceFromFontListEntry(const FontListEntry& aFLE,
                                   StandardFile aStdFile);

  void AppendFacesFromFontFile(const nsCString& aFileName,
                               FontNameCache* aCache, StandardFile aStdFile);

  void AppendFacesFromOmnijarEntry(nsZipArchive* aReader,
                                   const nsCString& aEntryName,
                                   FontNameCache* aCache, bool aJarChanged);

  // the defaults here are suitable for reading bundled fonts from omnijar
  void AppendFacesFromCachedFaceList(const nsCString& aFileName,
                                     const nsCString& aFaceList,
                                     StandardFile aStdFile = kStandard);

  void AddFaceToList(const nsCString& aEntryName, uint32_t aIndex,
                     StandardFile aStdFile, FT_Face aFace,
                     nsCString& aFaceList);

  void FindFonts();

  void FindFontsInOmnijar(FontNameCache* aCache);

  void FindFontsInDir(const nsCString& aDir, FontNameCache* aFNC);

  FontFamily GetDefaultFontForPlatform(const gfxFontStyle* aStyle) override;

  nsTHashtable<nsCStringHashKey> mSkipSpaceLookupCheckFamilies;

 private:
  mozilla::UniquePtr<FontNameCache> mFontNameCache;
  int64_t mJarModifiedTime;
  RefPtr<WillShutdownObserver> mObserver;
};

#endif /* GFX_FT2FONTLIST_H */

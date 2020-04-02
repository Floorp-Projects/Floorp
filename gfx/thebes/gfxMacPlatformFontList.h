/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gfxMacPlatformFontList_H_
#define gfxMacPlatformFontList_H_

#include <CoreFoundation/CoreFoundation.h>

#include "mozilla/FontPropertyTypes.h"
#include "mozilla/MemoryReporting.h"
#include "nsDataHashtable.h"
#include "nsRefPtrHashtable.h"

#include "gfxPlatformFontList.h"
#include "gfxPlatform.h"
#include "gfxPlatformMac.h"

#include "nsUnicharUtils.h"
#include "nsTArray.h"
#include "mozilla/LookAndFeel.h"

#include "mozilla/gfx/UnscaledFontMac.h"

class gfxMacPlatformFontList;

// a single member of a font family (i.e. a single face, such as Times Italic)
class MacOSFontEntry final : public gfxFontEntry {
 public:
  friend class gfxMacPlatformFontList;
  friend class gfxMacFont;

  MacOSFontEntry(const nsACString& aPostscriptName, WeightRange aWeight,
                 bool aIsStandardFace = false, double aSizeHint = 0.0);

  // for use with data fonts
  MacOSFontEntry(const nsACString& aPostscriptName, CGFontRef aFontRef,
                 WeightRange aWeight, StretchRange aStretch,
                 SlantStyleRange aStyle, bool aIsDataUserFont, bool aIsLocal);

  virtual ~MacOSFontEntry() { ::CGFontRelease(mFontRef); }

  gfxFontEntry* Clone() const override;

  CGFontRef GetFontRef();

  // override gfxFontEntry table access function to bypass table cache,
  // use CGFontRef API to get direct access to system font data
  hb_blob_t* GetFontTable(uint32_t aTag) override;

  void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                              FontListSizes* aSizes) const override;

  nsresult ReadCMAP(FontInfoData* aFontInfoData = nullptr) override;

  bool RequiresAATLayout() const { return mRequiresAAT; }

  bool HasVariations() override;
  void GetVariationAxes(
      nsTArray<gfxFontVariationAxis>& aVariationAxes) override;
  void GetVariationInstances(
      nsTArray<gfxFontVariationInstance>& aInstances) override;

  bool IsCFF();

  bool SupportsOpenTypeFeature(Script aScript, uint32_t aFeatureTag) override;

 protected:
  gfxFont* CreateFontInstance(const gfxFontStyle* aFontStyle) override;

  bool HasFontTable(uint32_t aTableTag) override;

  static void DestroyBlobFunc(void* aUserData);

  CGFontRef
      mFontRef;  // owning reference to the CGFont, released on destruction

  double mSizeHint;

  bool mFontRefInitialized;
  bool mRequiresAAT;
  bool mIsCFF;
  bool mIsCFFInitialized;
  bool mHasVariations;
  bool mHasVariationsInitialized;
  bool mHasAATSmallCaps;
  bool mHasAATSmallCapsInitialized;

  // To work around Core Text's mishandling of the default value for 'opsz',
  // we need to record whether the font has an a optical size axis, what its
  // range and default values are, and a usable close-to-default alternative.
  // (See bug 1457417 for details.)
  // These fields are used by gfxMacFont, but stored in the font entry so
  // that only a single font instance needs to inspect the available
  // variations.
  bool mCheckedForOpszAxis;
  bool mHasOpszAxis;
  gfxFontVariationAxis mOpszAxis;
  float mAdjustedDefaultOpsz;

  nsTHashtable<nsUint32HashKey> mAvailableTables;

  mozilla::ThreadSafeWeakPtr<mozilla::gfx::UnscaledFontMac> mUnscaledFont;
};

class gfxMacPlatformFontList final : public gfxPlatformFontList {
  using FontFamilyListEntry = mozilla::dom::SystemFontListEntry;

 public:
  static gfxMacPlatformFontList* PlatformFontList() {
    return static_cast<gfxMacPlatformFontList*>(sPlatformFontList);
  }

  gfxFontFamily* CreateFontFamily(const nsACString& aName,
                                  FontVisibility aVisibility) const override;

  static int32_t AppleWeightToCSSWeight(int32_t aAppleWeight);

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

  bool FindAndAddFamilies(mozilla::StyleGenericFontFamily aGeneric,
                          const nsACString& aFamily,
                          nsTArray<FamilyAndGeneric>* aOutput,
                          FindFamiliesFlags aFlags,
                          gfxFontStyle* aStyle = nullptr,
                          gfxFloat aDevToCssSize = 1.0) override;

  // lookup the system font for a particular system font type and set
  // the name and style characteristics
  void LookupSystemFont(mozilla::LookAndFeel::FontID aSystemFontID,
                        nsACString& aSystemFontName, gfxFontStyle& aFontStyle);

  // Values for the entryType field in FontFamilyListEntry records passed
  // from chrome to content process.
  enum FontFamilyEntryType {
    kStandardFontFamily = 0,          // a standard installed font family
    kTextSizeSystemFontFamily = 1,    // name of 'system' font at text sizes
    kDisplaySizeSystemFontFamily = 2  // 'system' font at display sizes
  };
  void ReadSystemFontList(nsTArray<FontFamilyListEntry>* aList);

 protected:
  FontFamily GetDefaultFontForPlatform(const gfxFontStyle* aStyle) override;

 private:
  friend class gfxPlatformMac;

  gfxMacPlatformFontList();
  virtual ~gfxMacPlatformFontList();

  // initialize font lists
  nsresult InitFontListForPlatform() override;
  void InitSharedFontListForPlatform() override;

  // special case font faces treated as font families (set via prefs)
  void InitSingleFaceList();
  void InitAliasesForSingleFaceList();

  // initialize system fonts
  void InitSystemFontNames();

  // helper function to lookup in both hidden system fonts and normal fonts
  gfxFontFamily* FindSystemFontFamily(const nsACString& aFamily);

  static void RegisteredFontsChangedNotificationCallback(
      CFNotificationCenterRef center, void* observer, CFStringRef name,
      const void* object, CFDictionaryRef userInfo);

  // attempt to use platform-specific fallback for the given character
  // return null if no usable result found
  gfxFontEntry* PlatformGlobalFontFallback(const uint32_t aCh,
                                           Script aRunScript,
                                           const gfxFontStyle* aMatchStyle,
                                           FontFamily* aMatchedFamily) override;

  bool UsesSystemFallback() override { return true; }

  already_AddRefed<FontInfoData> CreateFontInfoData() override;

  // Add the specified family to mFontFamilies.
  // Ideally we'd use NSString* instead of CFStringRef here, but this header
  // file is included in .cpp files, so we can't use objective C classes here.
  // But CFStringRef and NSString* are the same thing anyway (they're
  // toll-free bridged).
  void AddFamily(CFStringRef aFamily);

  void AddFamily(const nsACString& aFamilyName, FontVisibility aVisibility);

  void ActivateFontsFromDir(nsIFile* aDir);

  gfxFontEntry* CreateFontEntry(
      mozilla::fontlist::Face* aFace,
      const mozilla::fontlist::Family* aFamily) override;

  void GetFacesInitDataForFamily(
      const mozilla::fontlist::Family* aFamily,
      nsTArray<mozilla::fontlist::Face::InitData>& aFaces) const override;

  void ReadFaceNamesForFamily(mozilla::fontlist::Family* aFamily,
                              bool aNeedFullnamePostscriptNames) override;

#ifdef MOZ_BUNDLED_FONTS
  void ActivateBundledFonts();
#endif

  enum { kATSGenerationInitial = -1 };

  // default font for use with system-wide font fallback
  CTFontRef mDefaultFont;

  // font families that -apple-system maps to
  // Pre-10.11 this was always a single font family, such as Lucida Grande
  // or Helvetica Neue. For OSX 10.11, Apple uses pair of families
  // for the UI, one for text sizes and another for display sizes
  bool mUseSizeSensitiveSystemFont;
  nsCString mSystemTextFontFamilyName;
  nsCString mSystemDisplayFontFamilyName;  // only used on OSX 10.11
};

#endif /* gfxMacPlatformFontList_H_ */

/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_USER_FONT_SET_H
#define GFX_USER_FONT_SET_H

#include <new>
#include "PLDHashTable.h"
#include "gfxFontEntry.h"
#include "gfxFontUtils.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Attributes.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsHashKeys.h"
#include "nsIMemoryReporter.h"
#include "nsIObserver.h"
#include "nsIScriptError.h"
#include "nsISupports.h"
#include "nsRefPtrHashtable.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nscore.h"

// Only needed for function bodies.
#include <utility>                // for move, forward
#include "MainThreadUtils.h"      // for NS_IsMainThread
#include "gfxFontFeatures.h"      // for gfxFontFeature
#include "gfxFontSrcPrincipal.h"  // for gfxFontSrcPrincipal
#include "gfxFontSrcURI.h"        // for gfxFontSrcURI
#include "mozilla/Assertions.h"  // for AssertionConditionType, MOZ_ASSERT_HELPER2, MOZ_ASSERT, MOZ_ASSERT_UNREACHABLE, MOZ_ASSER...
#include "mozilla/HashFunctions.h"      // for HashBytes, HashGeneric
#include "mozilla/TimeStamp.h"          // for TimeStamp
#include "mozilla/gfx/FontVariation.h"  // for FontVariation
#include "nsDebug.h"                    // for NS_WARNING
#include "nsIReferrerInfo.h"            // for nsIReferrerInfo

class gfxFont;
class gfxUserFontSet;
class nsIFontLoadCompleteCallback;
class nsIRunnable;
struct gfxFontStyle;
struct gfxFontVariationAxis;
struct gfxFontVariationInstance;
template <class T>
class nsMainThreadPtrHandle;

namespace mozilla {
class LogModule;
class PostTraversalTask;
enum class StyleFontDisplay : uint8_t;
}  // namespace mozilla
class nsFontFaceLoader;

//#define DEBUG_USERFONT_CACHE

class gfxFontFaceBufferSource {
  NS_INLINE_DECL_REFCOUNTING(gfxFontFaceBufferSource)
 public:
  virtual void TakeBuffer(uint8_t*& aBuffer, uint32_t& aLength) = 0;

 protected:
  virtual ~gfxFontFaceBufferSource() = default;
};

// parsed CSS @font-face rule information
// lifetime: from when @font-face rule processed until font is loaded
struct gfxFontFaceSrc {
  enum SourceType { eSourceType_Local, eSourceType_URL, eSourceType_Buffer };

  SourceType mSourceType;

  // if url, whether to use the origin principal or not
  bool mUseOriginPrincipal;

  // format hint flags, union of all possible formats
  // (e.g. TrueType, EOT, SVG, etc.)
  // see FLAG_FORMAT_* enum values below
  uint32_t mFormatFlags;

  nsCString mLocalName;                          // full font name if local
  RefPtr<gfxFontSrcURI> mURI;                    // uri if url
  nsCOMPtr<nsIReferrerInfo> mReferrerInfo;       // referrer info if url
  RefPtr<gfxFontSrcPrincipal> mOriginPrincipal;  // principal if url

  RefPtr<gfxFontFaceBufferSource> mBuffer;

  // The principal that should be used for the load. Should only be used for
  // URL sources.
  gfxFontSrcPrincipal* LoadPrincipal(const gfxUserFontSet&) const;
};

inline bool operator==(const gfxFontFaceSrc& a, const gfxFontFaceSrc& b) {
  // The mReferrer and mOriginPrincipal comparisons aren't safe OMT.
  MOZ_ASSERT(NS_IsMainThread());

  if (a.mSourceType != b.mSourceType) {
    return false;
  }
  switch (a.mSourceType) {
    case gfxFontFaceSrc::eSourceType_Local:
      return a.mLocalName == b.mLocalName;
    case gfxFontFaceSrc::eSourceType_URL: {
      bool equals;
      return a.mUseOriginPrincipal == b.mUseOriginPrincipal &&
             a.mFormatFlags == b.mFormatFlags &&
             (a.mURI == b.mURI || a.mURI->Equals(b.mURI)) &&
             NS_SUCCEEDED(a.mReferrerInfo->Equals(b.mReferrerInfo, &equals)) &&
             equals && a.mOriginPrincipal->Equals(b.mOriginPrincipal);
    }
    case gfxFontFaceSrc::eSourceType_Buffer:
      return a.mBuffer == b.mBuffer;
  }
  NS_WARNING("unexpected mSourceType");
  return false;
}

// Subclassed to store platform-specific code cleaned out when font entry is
// deleted.
// Lifetime: from when platform font is created until it is deactivated.
// If the platform does not need to add any platform-specific code/data here,
// then the gfxUserFontSet will allocate a base gfxUserFontData and attach
// to the entry to track the basic user font info fields here.
class gfxUserFontData {
 public:
  gfxUserFontData()
      : mSrcIndex(0),
        mFormat(0),
        mMetaOrigLen(0),
        mCompression(kUnknownCompression),
        mPrivate(false),
        mIsBuffer(false) {}
  virtual ~gfxUserFontData() = default;

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  nsTArray<uint8_t> mMetadata;  // woff metadata block (compressed), if any
  RefPtr<gfxFontSrcURI> mURI;   // URI of the source, if it was url()
  RefPtr<gfxFontSrcPrincipal>
      mPrincipal;         // principal for the download, if url()
  nsCString mLocalName;   // font name used for the source, if local()
  nsCString mRealName;    // original fullname from the font resource
  uint32_t mSrcIndex;     // index in the rule's source list
  uint32_t mFormat;       // format hint for the source used, if any
  uint32_t mMetaOrigLen;  // length needed to decompress metadata
  uint8_t mCompression;   // compression type
  bool mPrivate;          // whether font belongs to a private window
  bool mIsBuffer;         // whether the font source was a buffer

  enum {
    kUnknownCompression = 0,
    kZlibCompression = 1,
    kBrotliCompression = 2
  };
};

// initially contains a set of userfont font entry objects, replaced with
// platform/user fonts as downloaded

class gfxUserFontFamily : public gfxFontFamily {
 public:
  friend class gfxUserFontSet;

  explicit gfxUserFontFamily(const nsACString& aName)
      : gfxFontFamily(aName, FontVisibility::Webfont) {}

  virtual ~gfxUserFontFamily();

  // add the given font entry to the end of the family's list
  void AddFontEntry(gfxFontEntry* aFontEntry) {
    MOZ_ASSERT(!mIsSimpleFamily, "not valid for user-font families");
    // keep ref while removing existing entry
    RefPtr<gfxFontEntry> fe = aFontEntry;
    // remove existing entry, if already present
    mAvailableFonts.RemoveElement(aFontEntry);
    // insert at the beginning so that the last-defined font is the first
    // one in the fontlist used for matching, as per CSS Fonts spec
    mAvailableFonts.InsertElementAt(0, aFontEntry);

    if (aFontEntry->mFamilyName.IsEmpty()) {
      aFontEntry->mFamilyName = Name();
    } else {
#ifdef DEBUG
      nsCString thisName = Name();
      nsCString entryName = aFontEntry->mFamilyName;
      ToLowerCase(thisName);
      ToLowerCase(entryName);
      MOZ_ASSERT(thisName.Equals(entryName));
#endif
    }
    ResetCharacterMap();
  }

  void RemoveFontEntry(gfxFontEntry* aFontEntry) {
    MOZ_ASSERT(!mIsSimpleFamily, "not valid for user-font families");
    mAvailableFonts.RemoveElement(aFontEntry);
  }

  // Remove all font entries from the family
  void DetachFontEntries() { mAvailableFonts.Clear(); }
};

class gfxUserFontEntry;
class gfxOTSMessageContext;

class gfxUserFontSet {
  friend class gfxUserFontEntry;
  friend class gfxOTSMessageContext;

 public:
  typedef mozilla::FontStretch FontStretch;
  typedef mozilla::StretchRange StretchRange;
  typedef mozilla::FontSlantStyle FontSlantStyle;
  typedef mozilla::SlantStyleRange SlantStyleRange;
  typedef mozilla::FontWeight FontWeight;
  typedef mozilla::WeightRange WeightRange;
  typedef gfxFontEntry::RangeFlags RangeFlags;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(gfxUserFontSet)

  gfxUserFontSet();

  enum {
    // no flags ==> no hint set
    // unknown ==> unknown format hint set
    FLAG_FORMAT_UNKNOWN = 1,
    FLAG_FORMAT_OPENTYPE = 1 << 1,
    FLAG_FORMAT_TRUETYPE = 1 << 2,
    FLAG_FORMAT_TRUETYPE_AAT = 1 << 3,
    FLAG_FORMAT_EOT = 1 << 4,
    FLAG_FORMAT_SVG = 1 << 5,
    FLAG_FORMAT_WOFF = 1 << 6,
    FLAG_FORMAT_WOFF2 = 1 << 7,

    FLAG_FORMAT_OPENTYPE_VARIATIONS = 1 << 8,
    FLAG_FORMAT_TRUETYPE_VARIATIONS = 1 << 9,
    FLAG_FORMAT_WOFF_VARIATIONS = 1 << 10,
    FLAG_FORMAT_WOFF2_VARIATIONS = 1 << 11,

    // the common formats that we support everywhere
    FLAG_FORMATS_COMMON =
        FLAG_FORMAT_OPENTYPE | FLAG_FORMAT_TRUETYPE | FLAG_FORMAT_WOFF |
        FLAG_FORMAT_WOFF2 | FLAG_FORMAT_OPENTYPE_VARIATIONS |
        FLAG_FORMAT_TRUETYPE_VARIATIONS | FLAG_FORMAT_WOFF_VARIATIONS |
        FLAG_FORMAT_WOFF2_VARIATIONS,

    // mask of all unused bits, update when adding new formats
    FLAG_FORMAT_NOT_USED = ~((1 << 12) - 1)
  };

  // creates a font face without adding it to a particular family
  // weight - [100, 900] (multiples of 100)
  // stretch = [FontStretch::UltraCondensed(), FontStretch::UltraExpanded()]
  // italic style = constants in gfxFontConstants.h, e.g. NS_FONT_STYLE_NORMAL
  // language override = result of calling
  // nsLayoutUtils::ParseFontLanguageOverride
  // TODO: support for unicode ranges not yet implemented
  virtual already_AddRefed<gfxUserFontEntry> CreateUserFontEntry(
      const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList, WeightRange aWeight,
      StretchRange aStretch, SlantStyleRange aStyle,
      const nsTArray<gfxFontFeature>& aFeatureSettings,
      const nsTArray<mozilla::gfx::FontVariation>& aVariationSettings,
      uint32_t aLanguageOverride, gfxCharacterMap* aUnicodeRanges,
      mozilla::StyleFontDisplay aFontDisplay, RangeFlags aRangeFlags,
      float aAscentOverride, float aDescentOverride, float aLineGapOverride,
      float aSizeAdjust) = 0;

  // creates a font face for the specified family, or returns an existing
  // matching entry on the family if there is one
  already_AddRefed<gfxUserFontEntry> FindOrCreateUserFontEntry(
      const nsACString& aFamilyName,
      const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList, WeightRange aWeight,
      StretchRange aStretch, SlantStyleRange aStyle,
      const nsTArray<gfxFontFeature>& aFeatureSettings,
      const nsTArray<mozilla::gfx::FontVariation>& aVariationSettings,
      uint32_t aLanguageOverride, gfxCharacterMap* aUnicodeRanges,
      mozilla::StyleFontDisplay aFontDisplay, RangeFlags aRangeFlags,
      float aAscentOverride, float aDescentOverride, float aLineGapOverride,
      float aSizeAdjust);

  // add in a font face for which we have the gfxUserFontEntry already
  void AddUserFontEntry(const nsCString& aFamilyName,
                        gfxUserFontEntry* aUserFontEntry);

  // Whether there is a face with this family name
  bool HasFamily(const nsACString& aFamilyName) const {
    return LookupFamily(aFamilyName) != nullptr;
  }

  // Look up and return the gfxUserFontFamily in mFontFamilies with
  // the given name
  gfxUserFontFamily* LookupFamily(const nsACString& aName) const;

  virtual gfxFontSrcPrincipal* GetStandardFontLoadPrincipal() const = 0;
  virtual nsPresContext* GetPresContext() const = 0;

  // check whether content policies allow the given URI to load.
  virtual bool IsFontLoadAllowed(const gfxFontFaceSrc&) = 0;

  // Dispatches all of the specified runnables to the font face set's
  // document's event queue.
  virtual void DispatchFontLoadViolations(
      nsTArray<nsCOMPtr<nsIRunnable>>& aViolations) = 0;

  // initialize the process that loads external font data, which upon
  // completion will call FontDataDownloadComplete method
  virtual nsresult StartLoad(gfxUserFontEntry* aUserFontEntry,
                             uint32_t aSrcIndex) = 0;

  // generation - each time a face is loaded, generation is
  // incremented so that the change can be recognized
  uint64_t GetGeneration() { return mGeneration; }

  // increment the generation on font load
  void IncrementGeneration(bool aIsRebuild = false);

  // Generation is bumped on font loads but that doesn't affect name-style
  // mappings. Rebuilds do however affect name-style mappings so need to
  // lookup fontlists again when that happens.
  uint64_t GetRebuildGeneration() { return mRebuildGeneration; }

  // rebuild if local rules have been used
  void RebuildLocalRules();

  // Discard any font entries created for src:local(), so that they will
  // be reloaded next time they're needed. This is called when the platform
  // font list has changed, which means local font entries that were set up
  // may no longer be valid.
  void ForgetLocalFaces();

  class UserFontCache {
   public:
    // Record a loaded user-font in the cache. This requires that the
    // font-entry's userFontData has been set up already, as it relies
    // on the URI and Principal recorded there.
    static void CacheFont(gfxFontEntry* aFontEntry);

    // The given gfxFontEntry is being destroyed, so remove any record that
    // refers to it.
    static void ForgetFont(gfxFontEntry* aFontEntry);

    // Return the gfxFontEntry corresponding to a given URI and principal,
    // and the features of the given userfont entry, or nullptr if none is
    // available. The aPrivate flag is set for requests coming from private
    // windows, so we can avoid leaking fonts cached in private windows mode out
    // to normal windows.
    static gfxFontEntry* GetFont(const gfxFontFaceSrc&,
                                 const gfxUserFontEntry&);

    // Clear everything so that we don't leak URIs and Principals.
    static void Shutdown();

    // Memory-reporting support.
    class MemoryReporter final : public nsIMemoryReporter {
     private:
      ~MemoryReporter() = default;

     public:
      NS_DECL_ISUPPORTS
      NS_DECL_NSIMEMORYREPORTER
    };

#ifdef DEBUG_USERFONT_CACHE
    // dump contents
    static void Dump();
#endif

   private:
    // Helper that we use to observe the empty-cache notification
    // from nsICacheService.
    class Flusher : public nsIObserver {
      virtual ~Flusher() = default;

     public:
      NS_DECL_ISUPPORTS
      NS_DECL_NSIOBSERVER
      Flusher() = default;
    };

    // Key used to look up entries in the user-font cache.
    // Note that key comparison does *not* use the mFontEntry field
    // as a whole; it only compares specific fields within the entry
    // (weight/width/style/features) that could affect font selection
    // or rendering, and that must match between a font-set's userfont
    // entry and the corresponding "real" font entry.
    struct Key {
      RefPtr<gfxFontSrcURI> mURI;
      RefPtr<gfxFontSrcPrincipal> mPrincipal;  // use nullptr with data: URLs
      // The font entry MUST notify the cache when it is destroyed
      // (by calling ForgetFont()).
      gfxFontEntry* MOZ_NON_OWNING_REF mFontEntry;
      bool mPrivate;

      Key(gfxFontSrcURI* aURI, gfxFontSrcPrincipal* aPrincipal,
          gfxFontEntry* aFontEntry, bool aPrivate)
          : mURI(aURI),
            mPrincipal(aPrincipal),
            mFontEntry(aFontEntry),
            mPrivate(aPrivate) {}
    };

    class Entry : public PLDHashEntryHdr {
     public:
      typedef const Key& KeyType;
      typedef const Key* KeyTypePointer;

      explicit Entry(KeyTypePointer aKey)
          : mURI(aKey->mURI),
            mPrincipal(aKey->mPrincipal),
            mFontEntry(aKey->mFontEntry),
            mPrivate(aKey->mPrivate) {}

      Entry(Entry&& aOther)
          : PLDHashEntryHdr(std::move(aOther)),
            mURI(std::move(aOther.mURI)),
            mPrincipal(std::move(aOther.mPrincipal)),
            mFontEntry(std::move(aOther.mFontEntry)),
            mPrivate(std::move(aOther.mPrivate)) {}

      ~Entry() = default;

      bool KeyEquals(const KeyTypePointer aKey) const;

      static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }

      static PLDHashNumber HashKey(const KeyTypePointer aKey) {
        PLDHashNumber principalHash =
            aKey->mPrincipal ? aKey->mPrincipal->Hash() : 0;
        return mozilla::HashGeneric(
            principalHash + int(aKey->mPrivate), aKey->mURI->Hash(),
            HashFeatures(aKey->mFontEntry->mFeatureSettings),
            HashVariations(aKey->mFontEntry->mVariationSettings),
            mozilla::HashString(aKey->mFontEntry->mFamilyName),
            aKey->mFontEntry->Weight().AsScalar(),
            aKey->mFontEntry->SlantStyle().AsScalar(),
            aKey->mFontEntry->Stretch().AsScalar(),
            aKey->mFontEntry->mLanguageOverride);
      }

      enum { ALLOW_MEMMOVE = false };

      gfxFontSrcURI* GetURI() const { return mURI; }
      gfxFontSrcPrincipal* GetPrincipal() const { return mPrincipal; }
      gfxFontEntry* GetFontEntry() const { return mFontEntry; }
      bool IsPrivate() const { return mPrivate; }

      void ReportMemory(nsIHandleReportCallback* aHandleReport,
                        nsISupports* aData, bool aAnonymize);

#ifdef DEBUG_USERFONT_CACHE
      void Dump();
#endif

     private:
      static uint32_t HashFeatures(const nsTArray<gfxFontFeature>& aFeatures) {
        return mozilla::HashBytes(aFeatures.Elements(),
                                  aFeatures.Length() * sizeof(gfxFontFeature));
      }

      static uint32_t HashVariations(
          const nsTArray<mozilla::gfx::FontVariation>& aVariations) {
        return mozilla::HashBytes(
            aVariations.Elements(),
            aVariations.Length() * sizeof(mozilla::gfx::FontVariation));
      }

      RefPtr<gfxFontSrcURI> mURI;
      RefPtr<gfxFontSrcPrincipal> mPrincipal;  // or nullptr for data: URLs

      // The "real" font entry corresponding to this downloaded font.
      // The font entry MUST notify the cache when it is destroyed
      // (by calling ForgetFont()).
      gfxFontEntry* MOZ_NON_OWNING_REF mFontEntry;

      // Whether this font was loaded from a private window.
      bool mPrivate;
    };

    static nsTHashtable<Entry>* sUserFonts;
  };

  void SetLocalRulesUsed() { mLocalRulesUsed = true; }

  static mozilla::LogModule* GetUserFontsLog();

  // record statistics about font completion
  virtual void RecordFontLoadDone(uint32_t aFontSize,
                                  mozilla::TimeStamp aDoneTime) {}

  void GetLoadStatistics(uint32_t& aLoadCount, uint64_t& aLoadSize) const {
    aLoadCount = mDownloadCount;
    aLoadSize = mDownloadSize;
  }

 protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~gfxUserFontSet();

  // Return whether the font set is associated with a private-browsing tab.
  virtual bool GetPrivateBrowsing() = 0;

  // Return whether the font set is associated with a document that was
  // shift-reloaded, for example, and thus should bypass the font cache.
  virtual bool BypassCache() = 0;

  // parse data for a data URL
  virtual nsresult SyncLoadFontData(gfxUserFontEntry* aFontToLoad,
                                    const gfxFontFaceSrc* aFontFaceSrc,
                                    uint8_t*& aBuffer,
                                    uint32_t& aBufferLength) = 0;

  // report a problem of some kind (implemented in nsUserFontSet)
  virtual nsresult LogMessage(gfxUserFontEntry* aUserFontEntry,
                              uint32_t aSrcIndex, const char* aMessage,
                              uint32_t aFlags = nsIScriptError::errorFlag,
                              nsresult aStatus = NS_OK) = 0;

  // helper method for performing the actual userfont set rebuild
  virtual void DoRebuildUserFontSet() = 0;

  // helper method for FindOrCreateUserFontEntry
  gfxUserFontEntry* FindExistingUserFontEntry(
      gfxUserFontFamily* aFamily,
      const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList, WeightRange aWeight,
      StretchRange aStretch, SlantStyleRange aStyle,
      const nsTArray<gfxFontFeature>& aFeatureSettings,
      const nsTArray<mozilla::gfx::FontVariation>& aVariationSettings,
      uint32_t aLanguageOverride, gfxCharacterMap* aUnicodeRanges,
      mozilla::StyleFontDisplay aFontDisplay, RangeFlags aRangeFlags,
      float aAscentOverride, float aDescentOverride, float aLineGapOverride,
      float aSizeAdjust);

  // creates a new gfxUserFontFamily in mFontFamilies, or returns an existing
  // family if there is one
  gfxUserFontFamily* GetFamily(const nsACString& aFamilyName);

  // font families defined by @font-face rules
  nsRefPtrHashtable<nsCStringHashKey, gfxUserFontFamily> mFontFamilies;

  uint64_t mGeneration;         // bumped on any font load change
  uint64_t mRebuildGeneration;  // only bumped on rebuilds

  // true when local names have been looked up, false otherwise
  bool mLocalRulesUsed;

  // true when rules using local names need to be redone
  bool mRebuildLocalRules;

  // performance stats
  uint32_t mDownloadCount;
  uint64_t mDownloadSize;
};

// acts a placeholder until the real font is downloaded

class gfxUserFontEntry : public gfxFontEntry {
  friend class mozilla::PostTraversalTask;
  friend class gfxUserFontSet;
  friend class nsUserFontSet;
  friend class nsFontFaceLoader;
  friend class gfxOTSMessageContext;

 public:
  enum UserFontLoadState {
    STATUS_NOT_LOADED = 0,
    STATUS_LOAD_PENDING,
    STATUS_LOADING,
    STATUS_LOADED,
    STATUS_FAILED
  };

  gfxUserFontEntry(
      gfxUserFontSet* aFontSet,
      const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList, WeightRange aWeight,
      StretchRange aStretch, SlantStyleRange aStyle,
      const nsTArray<gfxFontFeature>& aFeatureSettings,
      const nsTArray<mozilla::gfx::FontVariation>& aVariationSettings,
      uint32_t aLanguageOverride, gfxCharacterMap* aUnicodeRanges,
      mozilla::StyleFontDisplay aFontDisplay, RangeFlags aRangeFlags,
      float aAscentOverride, float aDescentOverride, float aLineGapOverride,
      float aSizeAdjust);

  virtual ~gfxUserFontEntry();

  // Update the attributes of the entry to the given values, without disturbing
  // the associated platform font entry or in-progress downloads.
  void UpdateAttributes(
      WeightRange aWeight, StretchRange aStretch, SlantStyleRange aStyle,
      const nsTArray<gfxFontFeature>& aFeatureSettings,
      const nsTArray<mozilla::gfx::FontVariation>& aVariationSettings,
      uint32_t aLanguageOverride, gfxCharacterMap* aUnicodeRanges,
      mozilla::StyleFontDisplay aFontDisplay, RangeFlags aRangeFlags,
      float aAscentOverride, float aDescentOverride, float aLineGapOverride,
      float aSizeAdjust);

  // Return whether the entry matches the given list of attributes
  bool Matches(const nsTArray<gfxFontFaceSrc>& aFontFaceSrcList,
               WeightRange aWeight, StretchRange aStretch,
               SlantStyleRange aStyle,
               const nsTArray<gfxFontFeature>& aFeatureSettings,
               const nsTArray<mozilla::gfx::FontVariation>& aVariationSettings,
               uint32_t aLanguageOverride, gfxCharacterMap* aUnicodeRanges,
               mozilla::StyleFontDisplay aFontDisplay, RangeFlags aRangeFlags,
               float aAscentOverride, float aDescentOverride,
               float aLineGapOverride, float aSizeAdjust);

  gfxFont* CreateFontInstance(const gfxFontStyle* aFontStyle) override;

  gfxFontEntry* GetPlatformFontEntry() const { return mPlatformFontEntry; }

  // is the font loading or loaded, or did it fail?
  UserFontLoadState LoadState() const { return mUserFontLoadState; }

  void LoadCanceled() {
    mUserFontLoadState = STATUS_NOT_LOADED;
    mFontDataLoadingState = NOT_LOADING;
    mLoader = nullptr;
    // Reset mCurrentSrcIndex so that all potential sources are re-considered.
    mCurrentSrcIndex = 0;
    mSeenLocalSource = false;
  }

  // whether to wait before using fallback font or not
  bool WaitForUserFont() const {
    return (mUserFontLoadState == STATUS_LOAD_PENDING ||
            mUserFontLoadState == STATUS_LOADING) &&
           mFontDataLoadingState < LOADING_SLOWLY;
  }

  // for userfonts, cmap is used to store the unicode range data
  // no cmap ==> all codepoints permitted
  bool CharacterInUnicodeRange(uint32_t ch) const {
    if (mCharacterMap) {
      return mCharacterMap->test(ch);
    }
    return true;
  }

  gfxCharacterMap* GetUnicodeRangeMap() const { return mCharacterMap.get(); }

  mozilla::StyleFontDisplay GetFontDisplay() const { return mFontDisplay; }

  // load the font - starts the loading of sources which continues until
  // a valid font resource is found or all sources fail
  void Load();

  // methods to expose some information to FontFaceSet::UserFontSet
  // since we can't make that class a friend
  void SetLoader(nsFontFaceLoader* aLoader) { mLoader = aLoader; }
  nsFontFaceLoader* GetLoader() const { return mLoader; }
  gfxFontSrcPrincipal* GetPrincipal() const { return mPrincipal; }
  void GetFamilyNameAndURIForLogging(uint32_t aSrcIndex,
                                     nsACString& aFamilyName, nsACString& aURI);

  gfxFontEntry* Clone() const override {
    MOZ_ASSERT_UNREACHABLE("cannot Clone user fonts");
    return nullptr;
  }

#ifdef DEBUG
  gfxUserFontSet* GetUserFontSet() const { return mFontSet; }
#endif

  const nsTArray<gfxFontFaceSrc>& SourceList() const { return mSrcList; }

  // Returns a weak reference to the requested source record, which is owned
  // by the gfxUserFontEntry.
  const gfxFontFaceSrc& SourceAt(uint32_t aSrcIndex) const {
    return mSrcList[aSrcIndex];
  }

  // The variation-query APIs should not be called on placeholders.
  bool HasVariations() override {
    MOZ_ASSERT_UNREACHABLE("not meaningful for a userfont placeholder");
    return false;
  }
  void GetVariationAxes(nsTArray<gfxFontVariationAxis>&) override {
    MOZ_ASSERT_UNREACHABLE("not meaningful for a userfont placeholder");
  }
  void GetVariationInstances(nsTArray<gfxFontVariationInstance>&) override {
    MOZ_ASSERT_UNREACHABLE("not meaningful for a userfont placeholder");
  }

 protected:
  struct OTSMessage {
    nsCString mMessage;
    int mLevel;  // see OTSContext in gfx/ots/include/opentype-sanitizer.h
  };

  const uint8_t* SanitizeOpenTypeData(const uint8_t* aData, uint32_t aLength,
                                      uint32_t& aSaneLength,
                                      gfxUserFontType& aFontType,
                                      nsTArray<OTSMessage>& aMessages);

  // attempt to load the next resource in the src list.
  void LoadNextSrc();
  void ContinueLoad();
  void DoLoadNextSrc(bool aForceAsync);

  // change the load state
  virtual void SetLoadState(UserFontLoadState aLoadState);

  // when download has been completed, pass back data here
  // aDownloadStatus == NS_OK ==> download succeeded, error otherwise
  // Ownership of aFontData is passed in here; the font set must
  // ensure that it is eventually deleted with free().
  void FontDataDownloadComplete(uint32_t aSrcIndex, const uint8_t* aFontData,
                                uint32_t aLength, nsresult aDownloadStatus,
                                nsIFontLoadCompleteCallback* aCallback);

  // helper method for creating a platform font
  // returns true if platform font creation successful
  // Ownership of aFontData is passed in here; the font must
  // ensure that it is eventually deleted with free().
  bool LoadPlatformFontSync(uint32_t aSrcIndex, const uint8_t* aFontData,
                            uint32_t aLength);

  void LoadPlatformFontAsync(uint32_t aSrcIndex, const uint8_t* aFontData,
                             uint32_t aLength,
                             nsIFontLoadCompleteCallback* aCallback);

  // helper method for LoadPlatformFontAsync; runs on a background thread
  void StartPlatformFontLoadOnBackgroundThread(
      uint32_t aSrcIndex, const uint8_t* aFontData, uint32_t aLength,
      nsMainThreadPtrHandle<nsIFontLoadCompleteCallback> aCallback);

  // helper method for LoadPlatformFontAsync; runs on the main thread
  void ContinuePlatformFontLoadOnMainThread(
      uint32_t aSrcIndex, const uint8_t* aOriginalFontData,
      uint32_t aOriginalLength, gfxUserFontType aFontType,
      const uint8_t* aSanitizedFontData, uint32_t aSanitizedLength,
      nsTArray<OTSMessage>&& aMessages,
      nsMainThreadPtrHandle<nsIFontLoadCompleteCallback> aCallback);

  // helper method for LoadPlatformFontSync and
  // ContinuePlatformFontLoadOnMainThread; runs on the main thread
  bool LoadPlatformFont(uint32_t aSrcIndex, const uint8_t* aOriginalFontData,
                        uint32_t aOriginalLength, gfxUserFontType aFontType,
                        const uint8_t* aSanitizedFontData,
                        uint32_t aSanitizedLength,
                        nsTArray<OTSMessage>&& aMessages);

  // helper method for FontDataDownloadComplete and
  // ContinuePlatformFontLoadOnMainThread; runs on the main thread
  void FontLoadFailed(nsIFontLoadCompleteCallback* aCallback);

  // store metadata and src details for current src into aFontEntry
  void StoreUserFontData(gfxFontEntry* aFontEntry, uint32_t aSrcIndex,
                         bool aPrivate, const nsACString& aOriginalName,
                         FallibleTArray<uint8_t>* aMetadata,
                         uint32_t aMetaOrigLen, uint8_t aCompression);

  // Clears and then adds to aResult all of the user font sets that this user
  // font entry has been added to.  This will at least include mFontSet, the
  // owner of this user font entry.
  virtual void GetUserFontSets(nsTArray<gfxUserFontSet*>& aResult);

  // Calls IncrementGeneration() on all user font sets that contain this
  // user font entry.
  void IncrementGeneration();

  // general load state
  UserFontLoadState mUserFontLoadState;

  // detailed load state while font data is loading
  // used to determine whether to use fallback font or not
  // note that code depends on the ordering of these values!
  enum FontDataLoadingState {
    NOT_LOADING = 0,      // not started to load any font resources yet
    LOADING_STARTED,      // loading has started; hide fallback font
    LOADING_ALMOST_DONE,  // timeout happened but we're nearly done,
                          // so keep hiding fallback font
    LOADING_SLOWLY,       // timeout happened and we're not nearly done,
                          // so use the fallback font
    LOADING_TIMED_OUT,    // font load took too long
    LOADING_FAILED        // failed to load any source: use fallback
  };
  FontDataLoadingState mFontDataLoadingState;

  bool mSeenLocalSource;
  bool mUnsupportedFormat;
  mozilla::StyleFontDisplay mFontDisplay;  // timing of userfont fallback

  RefPtr<gfxFontEntry> mPlatformFontEntry;
  nsTArray<gfxFontFaceSrc> mSrcList;
  uint32_t mCurrentSrcIndex;  // index of src item to be loaded next
  // This field is managed by the nsFontFaceLoader. In the destructor and
  // Cancel() methods of nsFontFaceLoader this reference is nulled out.
  nsFontFaceLoader* MOZ_NON_OWNING_REF
      mLoader;  // current loader for this entry, if any
  gfxUserFontSet* MOZ_NON_OWNING_REF
      mFontSet;  // font-set which owns this userfont entry
  RefPtr<gfxFontSrcPrincipal> mPrincipal;
};

#endif /* GFX_USER_FONT_SET_H */

/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FONTENTRY_H
#define GFX_FONTENTRY_H

#include <math.h>
#include <new>
#include <utility>
#include "ThebesRLBoxTypes.h"
#include "gfxFontUtils.h"
#include "gfxFontVariations.h"
#include "gfxRect.h"
#include "gfxTypes.h"
#include "harfbuzz/hb.h"
#include "ipc/EnumSerializer.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TypedEnumBits.h"
#include "mozilla/UniquePtr.h"
#include "nsTHashMap.h"
#include "nsDebug.h"
#include "nsHashKeys.h"
#include "nsISupports.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "nsUnicodeScriptCodes.h"
#include "nscore.h"

class FontInfoData;
class gfxContext;
class gfxFont;
class gfxFontFamily;
class gfxPlatformFontList;
class gfxSVGGlyphs;
class gfxUserFontData;
class nsAtom;
struct FontListSizes;
struct gfxFontFeature;
struct gfxFontStyle;
enum class eFontPresentation : uint8_t;

namespace IPC {
template <class P>
struct ParamTraits;
}

namespace mozilla {
class SVGContextPaint;
namespace fontlist {
struct Face;
struct Family;
}  // namespace fontlist
namespace gfx {
struct DeviceColor;
}
}  // namespace mozilla

typedef struct gr_face gr_face;
typedef struct FT_MM_Var_ FT_MM_Var;

#define NO_FONT_LANGUAGE_OVERRIDE 0

class gfxCharacterMap : public gfxSparseBitSet {
 public:
  nsrefcnt AddRef() {
    MOZ_ASSERT(int32_t(mRefCnt) >= 0, "illegal refcnt");
    ++mRefCnt;
    NS_LOG_ADDREF(this, mRefCnt, "gfxCharacterMap", sizeof(*this));
    return mRefCnt;
  }

  nsrefcnt Release() {
    MOZ_ASSERT(0 != mRefCnt, "dup release");
    --mRefCnt;
    NS_LOG_RELEASE(this, mRefCnt, "gfxCharacterMap");
    if (mRefCnt == 0) {
      NotifyReleased();
      // |this| has been deleted.
      return 0;
    }
    return mRefCnt;
  }

  gfxCharacterMap() : mHash(0), mBuildOnTheFly(false), mShared(false) {}

  explicit gfxCharacterMap(const gfxSparseBitSet& aOther)
      : gfxSparseBitSet(aOther),
        mHash(0),
        mBuildOnTheFly(false),
        mShared(false) {}

  void CalcHash() { mHash = GetChecksum(); }

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    return gfxSparseBitSet::SizeOfExcludingThis(aMallocSizeOf);
  }

  // hash of the cmap bitvector
  uint32_t mHash;

  // if cmap is built on the fly it's never shared
  bool mBuildOnTheFly;

  // cmap is shared globally
  bool mShared;

 protected:
  void NotifyReleased();

  nsAutoRefCnt mRefCnt;

 private:
  gfxCharacterMap(const gfxCharacterMap&);
  gfxCharacterMap& operator=(const gfxCharacterMap&);
};

// Info on an individual font feature, for reporting available features
// to DevTools via the GetFeatureInfo method.
struct gfxFontFeatureInfo {
  uint32_t mTag;
  uint32_t mScript;
  uint32_t mLangSys;
};

class gfxFontEntry {
 public:
  typedef mozilla::gfx::DrawTarget DrawTarget;
  typedef mozilla::unicode::Script Script;
  typedef mozilla::FontWeight FontWeight;
  typedef mozilla::FontSlantStyle FontSlantStyle;
  typedef mozilla::FontStretch FontStretch;
  typedef mozilla::WeightRange WeightRange;
  typedef mozilla::SlantStyleRange SlantStyleRange;
  typedef mozilla::StretchRange StretchRange;

  // Used by stylo
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(gfxFontEntry)

  explicit gfxFontEntry(const nsACString& aName, bool aIsStandardFace = false);

  // Create a new entry that refers to the same font as this, but without
  // additional state that may have been set up (such as family name).
  // (This is only to be used for system fonts in the platform font list,
  // not user fonts.)
  virtual gfxFontEntry* Clone() const = 0;

  // unique name for the face, *not* the family; not necessarily the
  // "real" or user-friendly name, may be an internal identifier
  const nsCString& Name() const { return mName; }

  // family name
  const nsCString& FamilyName() const { return mFamilyName; }

  // The following two methods may be relatively expensive, as they
  // will (usually, except on Linux) load and parse the 'name' table;
  // they are intended only for the font-inspection API, not for
  // perf-critical layout/drawing work.

  // The "real" name of the face, if available from the font resource;
  // returns Name() if nothing better is available.
  virtual nsCString RealFaceName();

  WeightRange Weight() const { return mWeightRange; }
  StretchRange Stretch() const { return mStretchRange; }
  SlantStyleRange SlantStyle() const { return mStyleRange; }

  bool IsUserFont() const { return mIsDataUserFont || mIsLocalUserFont; }
  bool IsLocalUserFont() const { return mIsLocalUserFont; }
  bool IsFixedPitch() const { return mFixedPitch; }
  bool IsItalic() const { return SlantStyle().Min().IsItalic(); }
  bool IsOblique() const { return SlantStyle().Min().IsOblique(); }
  bool IsUpright() const { return SlantStyle().Min().IsNormal(); }
  inline bool SupportsItalic();
  inline bool SupportsBold();  // defined below, because of RangeFlags use
  bool IgnoreGDEF() const { return mIgnoreGDEF; }
  bool IgnoreGSUB() const { return mIgnoreGSUB; }

  // Return whether the face corresponds to "normal" CSS style properties:
  //    font-style: normal;
  //    font-weight: normal;
  //    font-stretch: normal;
  // If this is false, we might want to fall back to a different face and
  // possibly apply synthetic styling.
  bool IsNormalStyle() const {
    return IsUpright() && Weight().Min() <= FontWeight::Normal() &&
           Weight().Max() >= FontWeight::Normal() &&
           Stretch().Min() <= FontStretch::Normal() &&
           Stretch().Max() >= FontStretch::Normal();
  }

  // whether a feature is supported by the font (limited to a small set
  // of features for which some form of fallback needs to be implemented)
  virtual bool SupportsOpenTypeFeature(Script aScript, uint32_t aFeatureTag);
  bool SupportsGraphiteFeature(uint32_t aFeatureTag);

  // returns a set containing all input glyph ids for a given feature
  const hb_set_t* InputsForOpenTypeFeature(Script aScript,
                                           uint32_t aFeatureTag);

  virtual bool HasFontTable(uint32_t aTableTag);

  inline bool HasGraphiteTables() {
    if (!mCheckedForGraphiteTables) {
      CheckForGraphiteTables();
      mCheckedForGraphiteTables = true;
    }
    return mHasGraphiteTables;
  }

  inline bool HasCmapTable() {
    if (!mCharacterMap && !mShmemCharacterMap) {
      ReadCMAP();
      NS_ASSERTION(mCharacterMap || mShmemCharacterMap,
                   "failed to initialize character map");
    }
    return mHasCmapTable;
  }

  inline bool HasCharacter(uint32_t ch) {
    if (mShmemCharacterMap) {
      return mShmemCharacterMap->test(ch);
    }
    if (mCharacterMap) {
      if (mShmemFace && TrySetShmemCharacterMap()) {
        // Forget our temporary local copy, now we can use the shared cmap
        mCharacterMap = nullptr;
        return mShmemCharacterMap->test(ch);
      }
      if (mCharacterMap->test(ch)) {
        return true;
      }
    }
    return TestCharacterMap(ch);
  }

  virtual bool SkipDuringSystemFallback() { return false; }
  nsresult InitializeUVSMap();
  uint16_t GetUVSGlyph(uint32_t aCh, uint32_t aVS);

  // All concrete gfxFontEntry subclasses (except gfxUserFontEntry) need
  // to override this, otherwise the font will never be used as it will
  // be considered to support no characters.
  // ReadCMAP() must *always* set the mCharacterMap pointer to a valid
  // gfxCharacterMap, even if empty, as other code assumes this pointer
  // can be safely dereferenced.
  virtual nsresult ReadCMAP(FontInfoData* aFontInfoData = nullptr);

  bool TryGetSVGData(gfxFont* aFont);
  bool HasSVGGlyph(uint32_t aGlyphId);
  bool GetSVGGlyphExtents(DrawTarget* aDrawTarget, uint32_t aGlyphId,
                          gfxFloat aSize, gfxRect* aResult);
  void RenderSVGGlyph(gfxContext* aContext, uint32_t aGlyphId,
                      mozilla::SVGContextPaint* aContextPaint);
  // Call this when glyph geometry or rendering has changed
  // (e.g. animated SVG glyphs)
  void NotifyGlyphsChanged();

  bool TryGetColorGlyphs();
  bool GetColorLayersInfo(uint32_t aGlyphId,
                          const mozilla::gfx::DeviceColor& aDefaultColor,
                          nsTArray<uint16_t>& layerGlyphs,
                          nsTArray<mozilla::gfx::DeviceColor>& layerColors);
  bool HasColorLayersForGlyph(uint32_t aGlyphId) {
    MOZ_ASSERT(mCOLR);
    return gfxFontUtils::HasColorLayersForGlyph(mCOLR, aGlyphId);
  }

  bool HasColorBitmapTable() {
    if (!mCheckedForColorBitmapTables) {
      mHasColorBitmapTable = HasFontTable(TRUETYPE_TAG('C', 'B', 'D', 'T')) ||
                             HasFontTable(TRUETYPE_TAG('s', 'b', 'i', 'x'));
      mCheckedForColorBitmapTables = true;
    }
    return mHasColorBitmapTable;
  }

  // Access to raw font table data (needed for Harfbuzz):
  // returns a pointer to data owned by the fontEntry or the OS,
  // which will remain valid until the blob is destroyed.
  // The data MUST be treated as read-only; we may be getting a
  // reference to a shared system font cache.
  //
  // The default implementation uses CopyFontTable to get the data
  // into a byte array, and maintains a cache of loaded tables.
  //
  // Subclasses should override this if they can provide more efficient
  // access than copying table data into our own buffers.
  //
  // Get blob that encapsulates a specific font table, or nullptr if
  // the table doesn't exist in the font.
  //
  // Caller is responsible to call hb_blob_destroy() on the returned blob
  // (if non-nullptr) when no longer required. For transient access to a
  // table, use of AutoTable (below) is generally preferred.
  virtual hb_blob_t* GetFontTable(uint32_t aTag);

  // Stack-based utility to return a specified table, automatically releasing
  // the blob when the AutoTable goes out of scope.
  class AutoTable {
   public:
    AutoTable(gfxFontEntry* aFontEntry, uint32_t aTag) {
      mBlob = aFontEntry->GetFontTable(aTag);
    }
    ~AutoTable() { hb_blob_destroy(mBlob); }
    operator hb_blob_t*() const { return mBlob; }

   private:
    hb_blob_t* mBlob;
    // not implemented:
    AutoTable(const AutoTable&) = delete;
    AutoTable& operator=(const AutoTable&) = delete;
  };

  // Return a font instance for a particular style. This may be a newly-
  // created instance, or a font already in the global cache.
  // We can't return a UniquePtr here, because we may be returning a shared
  // cached instance; but we also don't return already_AddRefed, because
  // the caller may only need to use the font temporarily and doesn't need
  // a strong reference.
  gfxFont* FindOrMakeFont(const gfxFontStyle* aStyle,
                          gfxCharacterMap* aUnicodeRangeMap = nullptr);

  // Get an existing font table cache entry in aBlob if it has been
  // registered, or return false if not.  Callers must call
  // hb_blob_destroy on aBlob if true is returned.
  //
  // Note that some gfxFont implementations may not call this at all,
  // if it is more efficient to get the table from the OS at that level.
  bool GetExistingFontTable(uint32_t aTag, hb_blob_t** aBlob);

  // Elements of aTable are transferred (not copied) to and returned in a
  // new hb_blob_t which is registered on the gfxFontEntry, but the initial
  // reference is owned by the caller.  Removing the last reference
  // unregisters the table from the font entry.
  //
  // Pass nullptr for aBuffer to indicate that the table is not present and
  // nullptr will be returned.  Also returns nullptr on OOM.
  hb_blob_t* ShareFontTableAndGetBlob(uint32_t aTag, nsTArray<uint8_t>* aTable);

  // Get the font's unitsPerEm from the 'head' table, in the case of an
  // sfnt resource. Will return kInvalidUPEM for non-sfnt fonts,
  // if present on the platform.
  uint16_t UnitsPerEm();
  enum {
    kMinUPEM = 16,     // Limits on valid unitsPerEm range, from the
    kMaxUPEM = 16384,  // OpenType spec
    kInvalidUPEM = uint16_t(-1)
  };

  // Shaper face accessors:
  // NOTE that harfbuzz and graphite handle ownership/lifetime of the face
  // object in completely different ways.

  // Get HarfBuzz face corresponding to this font file.
  // Caller must release with hb_face_destroy() when finished with it,
  // and the font entry will be notified via ForgetHBFace.
  hb_face_t* GetHBFace();
  void ForgetHBFace();

  // Get the sandbox instance that graphite is running in.
  rlbox_sandbox_gr* GetGrSandbox();

  // Register and get the callback handle for the glyph advance firefox callback
  // Since the sandbox instance is shared with multiple test shapers, callback
  // registration must be handled centrally to ensure multiple instances don't
  // register the same callback.
  sandbox_callback_gr<float (*)(const void*, uint16_t)>*
  GetGrSandboxAdvanceCallbackHandle();

  // Get Graphite face corresponding to this font file.
  // Caller must call gfxFontEntry::ReleaseGrFace when finished with it.
  // Graphite is run in a sandbox
  tainted_opaque_gr<gr_face*> GetGrFace();
  void ReleaseGrFace(tainted_opaque_gr<gr_face*> aFace);

  // Does the font have graphite contextuals that involve the space glyph
  // (and therefore we should bypass the word cache)?
  // Since this function inspects data from libGraphite stored in sandbox memory
  // it can only return a "hint" to the correct return value. This is because
  // a compromised libGraphite could change the sandbox memory maliciously at
  // any moment. The caller must ensure the calling code performs safe actions
  // independent of the value returned, to unwrap this return.
  tainted_boolean_hint HasGraphiteSpaceContextuals();

  // Release any SVG-glyphs document this font may have loaded.
  void DisconnectSVG();

  // Called to notify that aFont is being destroyed. Needed when we're tracking
  // the fonts belonging to this font entry.
  void NotifyFontDestroyed(gfxFont* aFont);

  // For memory reporting of the platform font list.
  virtual void AddSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                      FontListSizes* aSizes) const;
  virtual void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                      FontListSizes* aSizes) const;

  // Used for reporting on individual font entries in the user font cache,
  // which are not present in the platform font list.
  size_t ComputedSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  // Used when checking for complex script support, to mask off cmap ranges
  struct ScriptRange {
    uint32_t rangeStart;
    uint32_t rangeEnd;
    uint32_t numTags;  // number of entries in the tags[] array
    hb_tag_t tags[3];  // up to three OpenType script tags to check
  };

  bool SupportsScriptInGSUB(const hb_tag_t* aScriptTags, uint32_t aNumTags);

  /**
   * Font-variation query methods.
   *
   * Font backends that don't support variations should provide empty
   * implementations.
   */
  virtual bool HasVariations() = 0;

  virtual void GetVariationAxes(
      nsTArray<gfxFontVariationAxis>& aVariationAxes) = 0;

  virtual void GetVariationInstances(
      nsTArray<gfxFontVariationInstance>& aInstances) = 0;

  bool HasBoldVariableWeight();
  bool HasItalicVariation();
  bool HasOpticalSize();

  void CheckForVariationAxes();

  // Set up the entry's weight/stretch/style ranges according to axes found
  // by GetVariationAxes (for installed fonts; do NOT call this for user
  // fonts, where the ranges are provided by @font-face descriptors).
  void SetupVariationRanges();

  // Get variation axis settings that should be used to implement a particular
  // font style using this resource.
  void GetVariationsForStyle(nsTArray<gfxFontVariation>& aResult,
                             const gfxFontStyle& aStyle);

  // Get the font's list of features (if any) for DevTools support.
  void GetFeatureInfo(nsTArray<gfxFontFeatureInfo>& aFeatureInfo);

  // This is only called on platforms where we use FreeType.
  virtual FT_MM_Var* GetMMVar() { return nullptr; }

  // Return true if the font has a 'trak' table (and we can successfully
  // interpret it), otherwise false. This will load and cache the table
  // the first time it is called.
  bool HasTrackingTable();

  // Return the tracking (in font units) to be applied for the given size.
  // (This is a floating-point number because of possible interpolation.)
  float TrackingForCSSPx(float aSize) const;

  nsCString mName;
  nsCString mFamilyName;

  RefPtr<gfxCharacterMap> mCharacterMap;

  mozilla::fontlist::Face* mShmemFace = nullptr;
  const SharedBitSet* mShmemCharacterMap = nullptr;

  mozilla::UniquePtr<uint8_t[]> mUVSData;
  mozilla::UniquePtr<gfxUserFontData> mUserFontData;
  mozilla::UniquePtr<gfxSVGGlyphs> mSVGGlyphs;
  // list of gfxFonts that are using SVG glyphs
  nsTArray<gfxFont*> mFontsUsingSVGGlyphs;
  nsTArray<gfxFontFeature> mFeatureSettings;
  nsTArray<gfxFontVariation> mVariationSettings;
  mozilla::UniquePtr<nsTHashMap<nsUint32HashKey, bool>> mSupportedFeatures;
  mozilla::UniquePtr<nsTHashMap<nsUint32HashKey, hb_set_t*>> mFeatureInputs;

  // Color Layer font support
  hb_blob_t* mCOLR = nullptr;
  hb_blob_t* mCPAL = nullptr;

  // bitvector of substitution space features per script, one each
  // for default and non-default features
  uint32_t mDefaultSubSpaceFeatures[(int(Script::NUM_SCRIPT_CODES) + 31) / 32];
  uint32_t
      mNonDefaultSubSpaceFeatures[(int(Script::NUM_SCRIPT_CODES) + 31) / 32];

  uint32_t mUVSOffset = 0;

  uint32_t mLanguageOverride = NO_FONT_LANGUAGE_OVERRIDE;

  WeightRange mWeightRange = WeightRange(FontWeight(500));
  StretchRange mStretchRange = StretchRange(FontStretch::Normal());
  SlantStyleRange mStyleRange = SlantStyleRange(FontSlantStyle::Normal());

  // Font metrics overrides (as multiples of used font size); negative values
  // indicate no override to be applied.
  float mAscentOverride = -1.0;
  float mDescentOverride = -1.0;
  float mLineGapOverride = -1.0;

  // Scaling factor to be applied to the font size.
  float mSizeAdjust = 1.0;

  // For user fonts (only), we need to record whether or not weight/stretch/
  // slant variations should be clamped to the range specified in the entry
  // properties. When the @font-face rule omitted one or more of these
  // descriptors, it is treated as the initial value for font-matching (and
  // so that is what we record in the font entry), but when rendering the
  // range is NOT clamped.
  enum class RangeFlags : uint8_t {
    eNoFlags = 0,
    eAutoWeight = (1 << 0),
    eAutoStretch = (1 << 1),
    eAutoSlantStyle = (1 << 2),

    // Flag to record whether the face has a variable "wght" axis
    // that supports "bold" values, used to disable the application
    // of synthetic-bold effects.
    eBoldVariableWeight = (1 << 3),
    // Whether the face has an 'ital' axis.
    eItalicVariation = (1 << 4),

    // Flags to record if the face uses a non-CSS-compatible scale
    // for weight and/or stretch, in which case we won't map the
    // properties to the variation axes (though they can still be
    // explicitly set using font-variation-settings).
    eNonCSSWeight = (1 << 5),
    eNonCSSStretch = (1 << 6),

    // Whether the font has an 'opsz' axis.
    eOpticalSize = (1 << 7)
  };
  RangeFlags mRangeFlags = RangeFlags::eNoFlags;

  bool mFixedPitch : 1;
  bool mIsBadUnderlineFont : 1;
  bool mIsUserFontContainer : 1;  // userfont entry
  bool mIsDataUserFont : 1;       // platform font entry (data)
  bool mIsLocalUserFont : 1;      // platform font entry (local)
  bool mStandardFace : 1;
  bool mIgnoreGDEF : 1;
  bool mIgnoreGSUB : 1;
  bool mSVGInitialized : 1;
  bool mHasSpaceFeaturesInitialized : 1;
  bool mHasSpaceFeatures : 1;
  bool mHasSpaceFeaturesKerning : 1;
  bool mHasSpaceFeaturesNonKerning : 1;
  bool mSkipDefaultFeatureSpaceCheck : 1;
  bool mGraphiteSpaceContextualsInitialized : 1;
  bool mHasGraphiteSpaceContextuals : 1;
  bool mSpaceGlyphIsInvisible : 1;
  bool mSpaceGlyphIsInvisibleInitialized : 1;
  bool mHasGraphiteTables : 1;
  bool mCheckedForGraphiteTables : 1;
  bool mHasCmapTable : 1;
  bool mGrFaceInitialized : 1;
  bool mCheckedForColorGlyph : 1;
  bool mCheckedForVariationAxes : 1;
  bool mHasColorBitmapTable : 1;
  bool mCheckedForColorBitmapTables : 1;

 protected:
  friend class gfxPlatformFontList;
  friend class gfxFontFamily;
  friend class gfxUserFontEntry;

  gfxFontEntry();

  // Protected destructor, to discourage deletion outside of Release():
  virtual ~gfxFontEntry();

  virtual gfxFont* CreateFontInstance(const gfxFontStyle* aFontStyle) = 0;

  virtual void CheckForGraphiteTables();

  // Copy a font table into aBuffer.
  // The caller will be responsible for ownership of the data.
  virtual nsresult CopyFontTable(uint32_t aTableTag,
                                 nsTArray<uint8_t>& aBuffer) {
    MOZ_ASSERT_UNREACHABLE(
        "forgot to override either GetFontTable or "
        "CopyFontTable?");
    return NS_ERROR_FAILURE;
  }

  // Helper for HasTrackingTable; check/parse the table and cache pointers
  // to the subtables we need. Returns false on failure, in which case the
  // table is unusable.
  bool ParseTrakTable();

  // lookup the cmap in cached font data
  virtual already_AddRefed<gfxCharacterMap> GetCMAPFromFontInfo(
      FontInfoData* aFontInfoData, uint32_t& aUVSOffset);

  // helper for HasCharacter(), which is what client code should call
  virtual bool TestCharacterMap(uint32_t aCh);

  // Try to set mShmemCharacterMap, based on the char map in mShmemFace;
  // return true if successful, false if it remains null (maybe the parent
  // hasn't handled our SetCharacterMap message yet).
  bool TrySetShmemCharacterMap();

  // Helper for gfxPlatformFontList::CreateFontEntry methods: set properties
  // of the gfxFontEntry based on shared Face and Family records.
  void InitializeFrom(mozilla::fontlist::Face* aFace,
                      const mozilla::fontlist::Family* aFamily);

  // Shaper-specific face objects, shared by all instantiations of the same
  // physical font, regardless of size.
  // Usually, only one of these will actually be created for any given font
  // entry, depending on the font tables that are present.

  // hb_face_t is refcounted internally, so each shaper that's using it will
  // bump the ref count when it acquires the face, and "destroy" (release) it
  // in its destructor. The font entry has only this non-owning reference to
  // the face; when the face is deleted, it will tell the font entry to forget
  // it, so that a new face will be created next time it is needed.
  hb_face_t* mHBFace = nullptr;

  static hb_blob_t* HBGetTable(hb_face_t* face, uint32_t aTag, void* aUserData);

  // Callback that the hb_face will use to tell us when it is being deleted.
  static void HBFaceDeletedCallback(void* aUserData);

  // All libGraphite functionality is sandboxed in an rlbox sandbox. This
  // contains data for the sandbox instance.
  struct GrSandboxData;
  GrSandboxData* mSandboxData = nullptr;

  // gr_face is -not- refcounted, so it will be owned directly by the font
  // entry, and we'll keep a count of how many references we've handed out;
  // each shaper is responsible to call ReleaseGrFace on its entry when
  // finished with it, so that we know when it can be deleted.
  tainted_opaque_gr<gr_face*> mGrFace;

  // For AAT font, a strong reference to the 'trak' table (if present).
  hb_blob_t* const kTrakTableUninitialized = (hb_blob_t*)(intptr_t(-1));
  hb_blob_t* mTrakTable = kTrakTableUninitialized;
  bool TrakTableInitialized() const {
    return mTrakTable != kTrakTableUninitialized;
  }

  // Cached pointers to tables within 'trak', initialized by ParseTrakTable.
  const mozilla::AutoSwap_PRInt16* mTrakValues;
  const mozilla::AutoSwap_PRInt32* mTrakSizeTable;

  // number of current users of this entry's mGrFace
  nsrefcnt mGrFaceRefCnt = 0;

  static tainted_opaque_gr<const void*> GrGetTable(
      rlbox_sandbox_gr& sandbox, tainted_opaque_gr<const void*> aAppFaceHandle,
      tainted_opaque_gr<unsigned int> aName,
      tainted_opaque_gr<unsigned int*> aLen);
  static void GrReleaseTable(rlbox_sandbox_gr& sandbox,
                             tainted_opaque_gr<const void*> aAppFaceHandle,
                             tainted_opaque_gr<const void*> aTableBuffer);

  // For memory reporting: size of user-font data belonging to this entry.
  // We record this in the font entry because the actual data block may be
  // handed over to platform APIs, so that it would become difficult (and
  // platform-specific) to measure it directly at report-gathering time.
  uint32_t mComputedSizeOfUserFont = 0;

  // Font's unitsPerEm from the 'head' table, if available (will be set to
  // kInvalidUPEM for non-sfnt font formats)
  uint16_t mUnitsPerEm = 0;

  uint16_t mNumTrakSizes;

 private:
  /**
   * Font table hashtable, to support GetFontTable for harfbuzz.
   *
   * The harfbuzz shaper (and potentially other clients) needs access to raw
   * font table data. This needs to be cached so that it can be used
   * repeatedly (each time we construct a text run; in some cases, for
   * each character/glyph within the run) without re-fetching large tables
   * every time.
   *
   * Because we may instantiate many gfxFonts for the same physical font
   * file (at different sizes), we should ensure that they can share a
   * single cached copy of the font tables. To do this, we implement table
   * access and sharing on the fontEntry rather than the font itself.
   *
   * The default implementation uses GetFontTable() to read font table
   * data into byte arrays, and wraps them in blobs which are registered in
   * a hashtable.  The hashtable can then return pre-existing blobs to
   * harfbuzz.
   *
   * Harfbuzz will "destroy" the blobs when it is finished with them.  When
   * the last blob reference is removed, the FontTableBlobData user data
   * will remove the blob from the hashtable if still registered.
   */

  class FontTableBlobData;

  /**
   * FontTableHashEntry manages the entries of hb_blob_t's containing font
   * table data.
   *
   * This is used to share font tables across fonts with the same
   * font entry (but different sizes) for use by HarfBuzz.  The hashtable
   * does not own a strong reference to the blob, but keeps a weak pointer,
   * managed by FontTableBlobData.  Similarly FontTableBlobData keeps only a
   * weak pointer to the hashtable, managed by FontTableHashEntry.
   */

  class FontTableHashEntry : public nsUint32HashKey {
   public:
    // Declarations for nsTHashtable

    typedef nsUint32HashKey KeyClass;
    typedef KeyClass::KeyType KeyType;
    typedef KeyClass::KeyTypePointer KeyTypePointer;

    explicit FontTableHashEntry(KeyTypePointer aTag)
        : KeyClass(aTag), mSharedBlobData(nullptr), mBlob(nullptr) {}

    // NOTE: This assumes the new entry belongs to the same hashtable as
    // the old, because the mHashtable pointer in mSharedBlobData (if
    // present) will not be updated.
    FontTableHashEntry(FontTableHashEntry&& toMove)
        : KeyClass(std::move(toMove)),
          mSharedBlobData(std::move(toMove.mSharedBlobData)),
          mBlob(std::move(toMove.mBlob)) {
      toMove.mSharedBlobData = nullptr;
      toMove.mBlob = nullptr;
    }

    ~FontTableHashEntry() { Clear(); }

    // FontTable/Blob API

    // Transfer (not copy) elements of aTable to a new hb_blob_t and
    // return ownership to the caller.  A weak reference to the blob is
    // recorded in the hashtable entry so that others may use the same
    // table.
    hb_blob_t* ShareTableAndGetBlob(
        nsTArray<uint8_t>&& aTable,
        nsTHashtable<FontTableHashEntry>* aHashtable);

    // Return a strong reference to the blob.
    // Callers must hb_blob_destroy the returned blob.
    hb_blob_t* GetBlob() const;

    void Clear();

    size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

   private:
    static void DeleteFontTableBlobData(void* aBlobData);
    // not implemented
    FontTableHashEntry& operator=(FontTableHashEntry& toCopy);

    FontTableBlobData* mSharedBlobData;
    hb_blob_t* mBlob;
  };

  mozilla::UniquePtr<nsTHashtable<FontTableHashEntry>> mFontTableCache;

  gfxFontEntry(const gfxFontEntry&);
  gfxFontEntry& operator=(const gfxFontEntry&);
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(gfxFontEntry::RangeFlags)

inline bool gfxFontEntry::SupportsItalic() {
  return SlantStyle().Max().IsItalic() ||
         ((mRangeFlags & RangeFlags::eAutoSlantStyle) ==
              RangeFlags::eAutoSlantStyle &&
          HasItalicVariation());
}

inline bool gfxFontEntry::SupportsBold() {
  // bold == weights 600 and above
  // We return true if the face has a max weight descriptor >= 600,
  // OR if it's a user font with auto-weight (no descriptor) and has
  // a weight axis that supports values >= 600
  return Weight().Max().IsBold() ||
         ((mRangeFlags & RangeFlags::eAutoWeight) == RangeFlags::eAutoWeight &&
          HasBoldVariableWeight());
}

// used when iterating over all fonts looking for a match for a given character
struct GlobalFontMatch {
  GlobalFontMatch(uint32_t aCharacter, uint32_t aNextCh,
                  const gfxFontStyle& aStyle, eFontPresentation aPresentation)
      : mStyle(aStyle),
        mCh(aCharacter),
        mNextCh(aNextCh),
        mPresentation(aPresentation) {}

  RefPtr<gfxFontEntry> mBestMatch;       // current best match
  RefPtr<gfxFontFamily> mMatchedFamily;  // the family it belongs to
  mozilla::fontlist::Family* mMatchedSharedFamily = nullptr;
  const gfxFontStyle& mStyle;  // style to match
  const uint32_t mCh;          // codepoint to be matched
  const uint32_t mNextCh;      // following codepoint (or zero)
  eFontPresentation mPresentation;
  uint32_t mCount = 0;               // number of fonts matched
  uint32_t mCmapsTested = 0;         // number of cmaps tested
  double mMatchDistance = INFINITY;  // metric indicating closest match
};

// The actual FontVisibility enum is defined in gfxTypes.h
namespace IPC {
template <>
struct ParamTraits<FontVisibility>
    : public ContiguousEnumSerializer<FontVisibility, FontVisibility::Unknown,
                                      FontVisibility::Count> {};
}  // namespace IPC

class gfxFontFamily {
 public:
  // Used by stylo
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(gfxFontFamily)

  gfxFontFamily(const nsACString& aName, FontVisibility aVisibility)
      : mName(aName),
        mVisibility(aVisibility),
        mOtherFamilyNamesInitialized(false),
        mHasOtherFamilyNames(false),
        mFaceNamesInitialized(false),
        mHasStyles(false),
        mIsSimpleFamily(false),
        mIsBadUnderlineFamily(false),
        mFamilyCharacterMapInitialized(false),
        mSkipDefaultFeatureSpaceCheck(false),
        mCheckForFallbackFaces(false),
        mCheckedForLegacyFamilyNames(false) {}

  const nsCString& Name() const { return mName; }

  virtual void LocalizedName(nsACString& aLocalizedName);
  virtual bool HasOtherFamilyNames();

  // See https://bugzilla.mozilla.org/show_bug.cgi?id=835204:
  // check the font's 'name' table to see if it has a legacy family name
  // that would have been used by GDI (e.g. to split extra-bold or light
  // faces in a large family into separate "styled families" because of
  // GDI's 4-faces-per-family limitation). If found, the styled family
  // name will be added to the font list's "other family names" table.
  bool CheckForLegacyFamilyNames(gfxPlatformFontList* aFontList);

  nsTArray<RefPtr<gfxFontEntry>>& GetFontList() { return mAvailableFonts; }

  void AddFontEntry(RefPtr<gfxFontEntry> aFontEntry) {
    // bug 589682 - set the IgnoreGDEF flag on entries for Italic faces
    // of Times New Roman, because of buggy table in those fonts
    if (aFontEntry->IsItalic() && !aFontEntry->IsUserFont() &&
        Name().EqualsLiteral("Times New Roman")) {
      aFontEntry->mIgnoreGDEF = true;
    }
    if (aFontEntry->mFamilyName.IsEmpty()) {
      aFontEntry->mFamilyName = Name();
    } else {
      MOZ_ASSERT(aFontEntry->mFamilyName.Equals(Name()));
    }
    aFontEntry->mSkipDefaultFeatureSpaceCheck = mSkipDefaultFeatureSpaceCheck;
    mAvailableFonts.AppendElement(aFontEntry);

    // If we're adding a face to a family that has been marked as "simple",
    // we need to ensure any null entries are removed, as well as clearing
    // the flag (which may be set again later).
    if (mIsSimpleFamily) {
      mAvailableFonts.RemoveElementsBy([](const auto& font) { return !font; });
      mIsSimpleFamily = false;
    }
  }

  // note that the styles for this family have been added
  bool HasStyles() { return mHasStyles; }
  void SetHasStyles(bool aHasStyles) { mHasStyles = aHasStyles; }

  // choose a specific face to match a style using CSS font matching
  // rules (weight matching occurs here).  may return a face that doesn't
  // precisely match (e.g. normal face when no italic face exists).
  gfxFontEntry* FindFontForStyle(const gfxFontStyle& aFontStyle,
                                 bool aIgnoreSizeTolerance = false);

  virtual void FindAllFontsForStyle(const gfxFontStyle& aFontStyle,
                                    nsTArray<gfxFontEntry*>& aFontEntryList,
                                    bool aIgnoreSizeTolerance = false);

  // checks for a matching font within the family
  // used as part of the font fallback process
  void FindFontForChar(GlobalFontMatch* aMatchData);

  // checks all fonts for a matching font within the family
  void SearchAllFontsForChar(GlobalFontMatch* aMatchData);

  // read in other family names, if any, and use functor to add each into cache
  virtual void ReadOtherFamilyNames(gfxPlatformFontList* aPlatformFontList);

  // set when other family names have been read in
  void SetOtherFamilyNamesInitialized() { mOtherFamilyNamesInitialized = true; }

  // read in other localized family names, fullnames and Postscript names
  // for all faces and append to lookup tables
  virtual void ReadFaceNames(gfxPlatformFontList* aPlatformFontList,
                             bool aNeedFullnamePostscriptNames,
                             FontInfoData* aFontInfoData = nullptr);

  // find faces belonging to this family (platform implementations override
  // this; should be made pure virtual once all subclasses have been updated)
  virtual void FindStyleVariations(FontInfoData* aFontInfoData = nullptr) {}

  // search for a specific face using the Postscript name
  gfxFontEntry* FindFont(const nsACString& aPostscriptName);

  // read in cmaps for all the faces
  void ReadAllCMAPs(FontInfoData* aFontInfoData = nullptr);

  bool TestCharacterMap(uint32_t aCh) {
    if (!mFamilyCharacterMapInitialized) {
      ReadAllCMAPs();
    }
    return mFamilyCharacterMap.test(aCh);
  }

  void ResetCharacterMap() {
    mFamilyCharacterMap.reset();
    mFamilyCharacterMapInitialized = false;
  }

  // mark this family as being in the "bad" underline offset blocklist
  void SetBadUnderlineFamily() {
    mIsBadUnderlineFamily = true;
    if (mHasStyles) {
      SetBadUnderlineFonts();
    }
  }

  virtual bool IsSingleFaceFamily() const { return false; }

  bool IsBadUnderlineFamily() const { return mIsBadUnderlineFamily; }
  bool CheckForFallbackFaces() const { return mCheckForFallbackFaces; }

  // sort available fonts to put preferred (standard) faces towards the end
  void SortAvailableFonts();

  // check whether the family fits into the simple 4-face model,
  // so we can use simplified style-matching;
  // if so set the mIsSimpleFamily flag (defaults to False before we've checked)
  void CheckForSimpleFamily();

  // For memory reporter
  virtual void AddSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                      FontListSizes* aSizes) const;
  virtual void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                      FontListSizes* aSizes) const;

#ifdef DEBUG
  // Only used for debugging checks - does a linear search
  bool ContainsFace(gfxFontEntry* aFontEntry);
#endif

  void SetSkipSpaceFeatureCheck(bool aSkipCheck) {
    mSkipDefaultFeatureSpaceCheck = aSkipCheck;
  }

  // Check whether this family is appropriate to include in the Preferences
  // font list for the given langGroup and CSS generic, if the platform lets
  // us determine this.
  // Return true if the family should be included in the list, false to omit.
  // Default implementation returns true for everything, so no filtering
  // will occur; individual platforms may override.
  virtual bool FilterForFontList(nsAtom* aLangGroup,
                                 const nsACString& aGeneric) const {
    return true;
  }

  FontVisibility Visibility() const { return mVisibility; }
  bool IsHidden() const { return Visibility() == FontVisibility::Hidden; }
  bool IsWebFontFamily() const {
    return Visibility() == FontVisibility::Webfont;
  }

 protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~gfxFontFamily();

  bool ReadOtherFamilyNamesForFace(gfxPlatformFontList* aPlatformFontList,
                                   hb_blob_t* aNameTable,
                                   bool useFullName = false);

  // set whether this font family is in "bad" underline offset blocklist.
  void SetBadUnderlineFonts() {
    uint32_t i, numFonts = mAvailableFonts.Length();
    for (i = 0; i < numFonts; i++) {
      if (mAvailableFonts[i]) {
        mAvailableFonts[i]->mIsBadUnderlineFont = true;
      }
    }
  }

  nsCString mName;
  nsTArray<RefPtr<gfxFontEntry>> mAvailableFonts;
  gfxSparseBitSet mFamilyCharacterMap;

  FontVisibility mVisibility;

  bool mOtherFamilyNamesInitialized : 1;
  bool mHasOtherFamilyNames : 1;
  bool mFaceNamesInitialized : 1;
  bool mHasStyles : 1;
  bool mIsSimpleFamily : 1;
  bool mIsBadUnderlineFamily : 1;
  bool mFamilyCharacterMapInitialized : 1;
  bool mSkipDefaultFeatureSpaceCheck : 1;
  bool mCheckForFallbackFaces : 1;  // check other faces for character
  bool mCheckedForLegacyFamilyNames : 1;

  enum {
    // for "simple" families, the faces are stored in mAvailableFonts
    // with fixed positions:
    kRegularFaceIndex = 0,
    kBoldFaceIndex = 1,
    kItalicFaceIndex = 2,
    kBoldItalicFaceIndex = 3,
    // mask values for selecting face with bold and/or italic attributes
    kBoldMask = 0x01,
    kItalicMask = 0x02
  };
};

// Wrapper for either a mozilla::fontlist::Family in the shared font list or an
// unshared gfxFontFamily that belongs just to the current process. This does
// not own a reference, it just wraps a raw pointer and records the type.
struct FontFamily {
  FontFamily() : mUnshared(nullptr), mIsShared(false) {}

  FontFamily(const FontFamily& aOther) = default;

  explicit FontFamily(gfxFontFamily* aFamily)
      : mUnshared(aFamily), mIsShared(false) {}

  explicit FontFamily(mozilla::fontlist::Family* aFamily)
      : mShared(aFamily), mIsShared(true) {}

  bool operator==(const FontFamily& aOther) const {
    return mIsShared == aOther.mIsShared &&
           (mIsShared ? mShared == aOther.mShared
                      : mUnshared == aOther.mUnshared);
  }

  bool IsNull() const { return mIsShared ? !mShared : !mUnshared; }

  union {
    gfxFontFamily* mUnshared;
    mozilla::fontlist::Family* mShared;
  };
  bool mIsShared;
};

// Struct used in the gfxFontGroup font list to keep track of a font family
// together with the CSS generic (if any) that was mapped to it in this
// particular case (so it can be reported to the DevTools font inspector).
struct FamilyAndGeneric final {
  FamilyAndGeneric()
      : mFamily(), mGeneric(mozilla::StyleGenericFontFamily(0)) {}
  FamilyAndGeneric(const FamilyAndGeneric& aOther) = default;
  explicit FamilyAndGeneric(gfxFontFamily* aFamily,
                            mozilla::StyleGenericFontFamily aGeneric =
                                mozilla::StyleGenericFontFamily(0))
      : mFamily(aFamily), mGeneric(aGeneric) {}
  explicit FamilyAndGeneric(mozilla::fontlist::Family* aFamily,
                            mozilla::StyleGenericFontFamily aGeneric =
                                mozilla::StyleGenericFontFamily(0))
      : mFamily(aFamily), mGeneric(aGeneric) {}
  explicit FamilyAndGeneric(const FontFamily& aFamily,
                            mozilla::StyleGenericFontFamily aGeneric =
                                mozilla::StyleGenericFontFamily(0))
      : mFamily(aFamily), mGeneric(aGeneric) {}

  bool operator==(const FamilyAndGeneric& aOther) const {
    return mFamily == aOther.mFamily && mGeneric == aOther.mGeneric;
  }

  FontFamily mFamily;
  mozilla::StyleGenericFontFamily mGeneric;
};

#endif

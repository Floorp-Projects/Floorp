/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FONTENTRY_H
#define GFX_FONTENTRY_H

#include <limits>
#include <math.h>
#include <new>
#include <utility>
#include "COLRFonts.h"
#include "ThebesRLBoxTypes.h"
#include "gfxFontUtils.h"
#include "gfxFontVariations.h"
#include "gfxRect.h"
#include "gfxTypes.h"
#include "harfbuzz/hb.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "mozilla/RWLock.h"
#include "mozilla/TypedEnumBits.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/intl/UnicodeScriptCodes.h"
#include "nsTHashMap.h"
#include "nsDebug.h"
#include "nsHashKeys.h"
#include "nsISupports.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
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
}  // namespace mozilla

typedef struct gr_face gr_face;
typedef struct FT_MM_Var_ FT_MM_Var;

#define NO_FONT_LANGUAGE_OVERRIDE 0

class gfxCharacterMap : public gfxSparseBitSet {
 public:
  // gfxCharacterMap instances may be shared across multiple threads via a
  // global table managed by gfxPlatformFontList. Once a gfxCharacterMap is
  // inserted in the global table, its mShared flag will be TRUE, and we
  // cannot safely delete it except from gfxPlatformFontList (which will
  // use a lock to ensure entries are removed from its table and deleted
  // safely).

  // AddRef() is pretty much standard. We don't return the refcount as our
  // users don't care about it.
  void AddRef() {
    MOZ_ASSERT_TYPE_OK_FOR_REFCOUNTING(gfxCharacterMap);
    MOZ_ASSERT(int32_t(mRefCnt) >= 0, "illegal refcnt");
    [[maybe_unused]] nsrefcnt count = ++mRefCnt;
    NS_LOG_ADDREF(this, count, "gfxCharacterMap", sizeof(*this));
  }

  // Custom Release(): if the object is referenced from the global shared
  // table, and we're releasing the last *other* reference to it, then we
  // notify the global table to consider also releasing its ref. (That may
  // not actually happen, if another thread is racing with us and takes a
  // new reference, or completes the release first!)
  void Release() {
    MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");
    // We can't safely read this after we've decremented mRefCnt, so save it
    // in a local variable here. Note that the value is never reset to false
    // once it has been set to true (when recording the cmap in the shared
    // table), so there's no risk of this resulting in a "false positive" when
    // tested later. A "false negative" is possible but harmless; it would
    // just mean we miss an opportunity to release a reference from the shared
    // cmap table.
    bool isShared = mShared;

    // Ensure we only access mRefCnt once, for consistency if the object is
    // being used by multiple threads.
    nsrefcnt count = --mRefCnt;
    NS_LOG_RELEASE(this, count, "gfxCharacterMap");

    // If isShared was true, this object has been shared across threads. In
    // that case, if the refcount went to 1, we notify the shared table so
    // it can drop its reference and delete the object.
    if (isShared) {
      MOZ_ASSERT(count > 0);
      if (count == 1) {
        NotifyMaybeReleased(this);
      }
      return;
    }

    // Otherwise, this object hasn't been shared and we can safely delete it
    // as we must have been holding the only reference. (Note that if we were
    // holding the only reference, there's no other owner who can have set
    // mShared to true since we read it above.)
    if (count == 0) {
      delete this;
    }
  }

  gfxCharacterMap() = default;

  explicit gfxCharacterMap(const gfxSparseBitSet& aOther)
      : gfxSparseBitSet(aOther) {}

  size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    return gfxSparseBitSet::SizeOfExcludingThis(aMallocSizeOf);
  }

  // hash of the cmap bitvector
  uint32_t mHash = 0;

  // if cmap is built on the fly it's never shared
  bool mBuildOnTheFly = false;

  // Character map is shared globally. This can only be set by the thread that
  // originally created the map, as no other thread can get a reference until
  // it has been shared via the global table.
  bool mShared = false;

 protected:
  friend class gfxPlatformFontList;

  // Destructor should not be called except via Release().
  // (Note that our "friend" gfxPlatformFontList also accesses this from its
  // MaybeRemoveCmap method.)
  ~gfxCharacterMap() = default;

  nsrefcnt RefCount() const { return mRefCnt; }

  void CalcHash() { mHash = GetChecksum(); }

  static void NotifyMaybeReleased(gfxCharacterMap* aCmap);

  // Only used when clearing the shared-cmap hashtable during shutdown.
  void ClearSharedFlag() {
    MOZ_ASSERT(NS_IsMainThread());
    mShared = false;
  }

  mozilla::ThreadSafeAutoRefCnt mRefCnt;

 private:
  gfxCharacterMap(const gfxCharacterMap&) = delete;
  gfxCharacterMap& operator=(const gfxCharacterMap&) = delete;
};

// Info on an individual font feature, for reporting available features
// to DevTools via the GetFeatureInfo method.
struct gfxFontFeatureInfo {
  uint32_t mTag;
  uint32_t mScript;
  uint32_t mLangSys;
};

class gfxFontEntryCallbacks;

class gfxFontEntry {
 public:
  typedef mozilla::gfx::DrawTarget DrawTarget;
  typedef mozilla::intl::Script Script;
  typedef mozilla::FontWeight FontWeight;
  typedef mozilla::FontSlantStyle FontSlantStyle;
  typedef mozilla::FontStretch FontStretch;
  typedef mozilla::WeightRange WeightRange;
  typedef mozilla::SlantStyleRange SlantStyleRange;
  typedef mozilla::StretchRange StretchRange;

  // Used by stylo
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(gfxFontEntry)

  explicit gfxFontEntry(const nsACString& aName, bool aIsStandardFace = false);

  gfxFontEntry() = delete;
  gfxFontEntry(const gfxFontEntry&) = delete;
  gfxFontEntry& operator=(const gfxFontEntry&) = delete;

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
  inline bool SupportsItalic();  // defined below, because of RangeFlags use
  inline bool SupportsBold();
  inline bool MayUseSyntheticSlant();
  bool IgnoreGDEF() const { return mIgnoreGDEF; }
  bool IgnoreGSUB() const { return mIgnoreGSUB; }

  // Return whether the face corresponds to "normal" CSS style properties:
  //    font-style: normal;
  //    font-weight: normal;
  //    font-stretch: normal;
  // If this is false, we might want to fall back to a different face and
  // possibly apply synthetic styling.
  bool IsNormalStyle() const {
    return IsUpright() && Weight().Min() <= FontWeight::NORMAL &&
           Weight().Max() >= FontWeight::NORMAL &&
           Stretch().Min() <= FontStretch::NORMAL &&
           Stretch().Max() >= FontStretch::NORMAL;
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
    LazyFlag flag = mHasGraphiteTables;
    if (flag == LazyFlag::Uninitialized) {
      flag = CheckForGraphiteTables() ? LazyFlag::Yes : LazyFlag::No;
      mHasGraphiteTables = flag;
    }
    return flag == LazyFlag::Yes;
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
      return GetShmemCharacterMap()->test(ch);
    }
    if (mCharacterMap) {
      if (mShmemFace && TrySetShmemCharacterMap()) {
        // Forget our temporary local copy, now we can use the shared cmap
        auto* oldCmap = mCharacterMap.exchange(nullptr);
        NS_IF_RELEASE(oldCmap);
        return GetShmemCharacterMap()->test(ch);
      }
      if (GetCharacterMap()->test(ch)) {
        return true;
      }
    }
    return TestCharacterMap(ch);
  }

  virtual bool SkipDuringSystemFallback() { return false; }
  void EnsureUVSMapInitialized();
  uint16_t GetUVSGlyph(uint32_t aCh, uint32_t aVS);

  // All concrete gfxFontEntry subclasses (except gfxUserFontEntry) need
  // to override this, otherwise the font will never be used as it will
  // be considered to support no characters.
  // ReadCMAP() must *always* set the mCharacterMap pointer to a valid
  // gfxCharacterMap, even if empty, as other code assumes this pointer
  // can be safely dereferenced.
  virtual nsresult ReadCMAP(FontInfoData* aFontInfoData = nullptr);

  bool TryGetSVGData(const gfxFont* aFont);
  bool HasSVGGlyph(uint32_t aGlyphId);
  bool GetSVGGlyphExtents(DrawTarget* aDrawTarget, uint32_t aGlyphId,
                          gfxFloat aSize, gfxRect* aResult);
  void RenderSVGGlyph(gfxContext* aContext, uint32_t aGlyphId,
                      mozilla::SVGContextPaint* aContextPaint);
  // Call this when glyph geometry or rendering has changed
  // (e.g. animated SVG glyphs)
  void NotifyGlyphsChanged();

  bool TryGetColorGlyphs();

  bool HasColorBitmapTable() {
    LazyFlag flag = mHasColorBitmapTable;
    if (flag == LazyFlag::Uninitialized) {
      flag = HasFontTable(TRUETYPE_TAG('C', 'B', 'D', 'T')) ||
                     HasFontTable(TRUETYPE_TAG('s', 'b', 'i', 'x'))
                 ? LazyFlag::Yes
                 : LazyFlag::No;
      mHasColorBitmapTable = flag;
    }
    return flag == LazyFlag::Yes;
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
  already_AddRefed<gfxFont> FindOrMakeFont(
      const gfxFontStyle* aStyle, gfxCharacterMap* aUnicodeRangeMap = nullptr);

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

  // Create a HarfBuzz face corresponding to this font file.
  // Our reference to the underlying hb_face_t will be released when the
  // returned AutoHBFace goes out of scope, but the hb_face_t itself may
  // be kept alive by other references (e.g. if an hb_font_t has been
  // instantiated for it).
  class MOZ_STACK_CLASS AutoHBFace {
   public:
    explicit AutoHBFace(hb_face_t* aFace) : mFace(aFace) {}
    ~AutoHBFace() { hb_face_destroy(mFace); }

    operator hb_face_t*() const { return mFace; }

    // Not default-constructible, not copyable.
    AutoHBFace() = delete;
    AutoHBFace(const AutoHBFace&) = delete;
    AutoHBFace& operator=(const AutoHBFace&) = delete;

   private:
    hb_face_t* mFace;
  };

  AutoHBFace GetHBFace() {
    return AutoHBFace(hb_face_create_for_tables(HBGetTable, this, nullptr));
  }

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
  bool HasSlantVariation();
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
  gfxFloat TrackingForCSSPx(gfxFloat aSize) const;

  mozilla::gfx::Rect GetFontExtents(float aFUnitScaleFactor) const {
    // Flip the y-axis here to match the orientation of Gecko's coordinates.
    return mozilla::gfx::Rect(float(mXMin) * aFUnitScaleFactor,
                              float(-mYMax) * aFUnitScaleFactor,
                              float(mXMax - mXMin) * aFUnitScaleFactor,
                              float(mYMax - mYMin) * aFUnitScaleFactor);
  }

  nsCString mName;
  nsCString mFamilyName;

  // These are mutable so that we can take a read lock within a const method.
  mutable mozilla::RWLock mLock;
  mutable mozilla::Mutex mFeatureInfoLock;

  mozilla::Atomic<gfxCharacterMap*> mCharacterMap;  // strong ref
  gfxCharacterMap* GetCharacterMap() const { return mCharacterMap; }

  mozilla::fontlist::Face* mShmemFace = nullptr;

  mozilla::Atomic<const SharedBitSet*> mShmemCharacterMap;
  const SharedBitSet* GetShmemCharacterMap() const {
    return mShmemCharacterMap;
  }

  mozilla::Atomic<const uint8_t*> mUVSData;
  const uint8_t* GetUVSData() const { return mUVSData; }

  mozilla::UniquePtr<gfxUserFontData> mUserFontData;

  mozilla::Atomic<gfxSVGGlyphs*> mSVGGlyphs;
  gfxSVGGlyphs* GetSVGGlyphs() const { return mSVGGlyphs; }

  // list of gfxFonts that are using SVG glyphs
  nsTArray<const gfxFont*> mFontsUsingSVGGlyphs MOZ_GUARDED_BY(mLock);
  nsTArray<gfxFontFeature> mFeatureSettings;
  nsTArray<gfxFontVariation> mVariationSettings;

  mozilla::UniquePtr<nsTHashMap<nsUint32HashKey, bool>> mSupportedFeatures
      MOZ_GUARDED_BY(mFeatureInfoLock);
  mozilla::UniquePtr<nsTHashMap<nsUint32HashKey, hb_set_t*>> mFeatureInputs
      MOZ_GUARDED_BY(mFeatureInfoLock);

  // Color Layer font support. These tables are inert once loaded, so we don't
  // need to hold a lock when reading them.
  mozilla::Atomic<hb_blob_t*> mCOLR;
  mozilla::Atomic<hb_blob_t*> mCPAL;
  hb_blob_t* GetCOLR() const { return mCOLR; }
  hb_blob_t* GetCPAL() const { return mCPAL; }

  // bitvector of substitution space features per script, one each
  // for default and non-default features
  uint32_t mDefaultSubSpaceFeatures[(int(Script::NUM_SCRIPT_CODES) + 31) / 32];
  uint32_t
      mNonDefaultSubSpaceFeatures[(int(Script::NUM_SCRIPT_CODES) + 31) / 32];

  mozilla::Atomic<uint32_t> mUVSOffset;

  uint32_t mLanguageOverride = NO_FONT_LANGUAGE_OVERRIDE;

  WeightRange mWeightRange = WeightRange(FontWeight::FromInt(500));
  StretchRange mStretchRange = StretchRange(FontStretch::NORMAL);
  SlantStyleRange mStyleRange = SlantStyleRange(FontSlantStyle::NORMAL);

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
  enum class RangeFlags : uint16_t {
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
    // Whether the face has a 'slnt' axis.
    eSlantVariation = (1 << 5),

    // Flags to record if the face uses a non-CSS-compatible scale
    // for weight and/or stretch, in which case we won't map the
    // properties to the variation axes (though they can still be
    // explicitly set using font-variation-settings).
    eNonCSSWeight = (1 << 6),
    eNonCSSStretch = (1 << 7),

    // Whether the font has an 'opsz' axis.
    eOpticalSize = (1 << 8)
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
  bool mSkipDefaultFeatureSpaceCheck : 1;

  mozilla::Atomic<bool> mSVGInitialized;
  mozilla::Atomic<bool> mHasCmapTable;
  mozilla::Atomic<bool> mGrFaceInitialized;
  mozilla::Atomic<bool> mCheckedForColorGlyph;
  mozilla::Atomic<bool> mCheckedForVariationAxes;

  // Atomic flags that are lazily evaluated - initially set to UNINITIALIZED,
  // changed to NO or YES once we determine the actual value.
  enum class LazyFlag : uint8_t { Uninitialized = 0xff, No = 0, Yes = 1 };

  std::atomic<LazyFlag> mSpaceGlyphIsInvisible;
  std::atomic<LazyFlag> mHasGraphiteTables;
  std::atomic<LazyFlag> mHasGraphiteSpaceContextuals;
  std::atomic<LazyFlag> mHasColorBitmapTable;

  enum class SpaceFeatures : uint8_t {
    Uninitialized = 0xff,
    None = 0,
    HasFeatures = 1 << 0,
    Kerning = 1 << 1,
    NonKerning = 1 << 2
  };

  std::atomic<SpaceFeatures> mHasSpaceFeatures;

 protected:
  friend class gfxPlatformFontList;
  friend class gfxFontFamily;
  friend class gfxUserFontEntry;

  // Protected destructor, to discourage deletion outside of Release():
  virtual ~gfxFontEntry();

  virtual gfxFont* CreateFontInstance(const gfxFontStyle* aFontStyle) = 0;

  inline bool CheckForGraphiteTables() {
    return HasFontTable(TRUETYPE_TAG('S', 'i', 'l', 'f'));
  }

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
  bool ParseTrakTable() MOZ_REQUIRES(mLock);

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
  mozilla::Atomic<hb_face_t*> mHBFace;

  static hb_blob_t* HBGetTable(hb_face_t* face, uint32_t aTag, void* aUserData);

  // Callback that the hb_face will use to tell us when it is being deleted.
  static void HBFaceDeletedCallback(void* aUserData);

  // All libGraphite functionality is sandboxed in an rlbox sandbox. This
  // contains data for the sandbox instance.
  // Currently graphite shaping is only supported on the main thread.
  struct GrSandboxData;
  GrSandboxData* mSandboxData = nullptr;

  // gr_face is -not- refcounted, so it will be owned directly by the font
  // entry, and we'll keep a count of how many references we've handed out;
  // each shaper is responsible to call ReleaseGrFace on its entry when
  // finished with it, so that we know when it can be deleted.
  tainted_opaque_gr<gr_face*> mGrFace;

  // For AAT font, a strong reference to the 'trak' table (if present).
  hb_blob_t* const kTrakTableUninitialized = (hb_blob_t*)(intptr_t(-1));
  mozilla::Atomic<hb_blob_t*> mTrakTable;
  hb_blob_t* GetTrakTable() const { return mTrakTable; }
  bool TrakTableInitialized() const {
    return mTrakTable != kTrakTableUninitialized;
  }

  // Cached pointers to tables within 'trak', initialized by ParseTrakTable.
  // This data is inert once loaded, so locking is not required to read it.
  const mozilla::AutoSwap_PRInt16* mTrakValues = nullptr;
  const mozilla::AutoSwap_PRInt32* mTrakSizeTable = nullptr;

  // number of current users of this entry's mGrFace
  nsrefcnt mGrFaceRefCnt = 0;

  friend class gfxFontEntryCallbacks;

  // For memory reporting: size of user-font data belonging to this entry.
  // We record this in the font entry because the actual data block may be
  // handed over to platform APIs, so that it would become difficult (and
  // platform-specific) to measure it directly at report-gathering time.
  uint32_t mComputedSizeOfUserFont = 0;

  // Font's unitsPerEm from the 'head' table, if available (will be set to
  // kInvalidUPEM for non-sfnt font formats)
  uint16_t mUnitsPerEm = 0;

  uint16_t mNumTrakSizes = 0;

  // Font extents in FUnits. (To be set from the 'head' table; default to
  // "huge" to avoid any clipping if real extents not available.)
  int16_t mXMin = std::numeric_limits<int16_t>::min();
  int16_t mYMin = std::numeric_limits<int16_t>::min();
  int16_t mXMax = std::numeric_limits<int16_t>::max();
  int16_t mYMax = std::numeric_limits<int16_t>::max();

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

  using FontTableCache = nsTHashtable<FontTableHashEntry>;
  mozilla::Atomic<FontTableCache*> mFontTableCache;
  FontTableCache* GetFontTableCache() const { return mFontTableCache; }
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(gfxFontEntry::RangeFlags)
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(gfxFontEntry::SpaceFeatures)

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

inline bool gfxFontEntry::MayUseSyntheticSlant() {
  if (!IsUpright()) {
    return false;  // The resource is already non-upright.
  }
  if (HasSlantVariation()) {
    if (mRangeFlags & RangeFlags::eAutoSlantStyle) {
      return false;
    }
    if (!SlantStyle().IsSingle()) {
      return false;  // The resource has a 'slnt' axis, and has not been
                     // clamped to just its upright setting.
    }
  }
  return true;
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

class gfxFontFamily {
 public:
  // Used by stylo
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(gfxFontFamily)

  gfxFontFamily(const nsACString& aName, FontVisibility aVisibility)
      : mName(aName),
        mLock("gfxFontFamily lock"),
        mVisibility(aVisibility),
        mIsSimpleFamily(false),
        mIsBadUnderlineFamily(false),
        mSkipDefaultFeatureSpaceCheck(false),
        mCheckForFallbackFaces(false) {}

  const nsCString& Name() const { return mName; }

  virtual void LocalizedName(nsACString& aLocalizedName);
  virtual bool HasOtherFamilyNames();

  // See https://bugzilla.mozilla.org/show_bug.cgi?id=835204:
  // check the font's 'name' table to see if it has a legacy family name
  // that would have been used by GDI (e.g. to split extra-bold or light
  // faces in a large family into separate "styled families" because of
  // GDI's 4-faces-per-family limitation). If found, the styled family
  // name will be added to the font list's "other family names" table.
  // Note that the caller must already hold the gfxPlatformFontList lock.
  bool CheckForLegacyFamilyNames(gfxPlatformFontList* aFontList);

  // Callers must hold a read-lock for as long as they're using the list.
  const nsTArray<RefPtr<gfxFontEntry>>& GetFontList()
      MOZ_REQUIRES_SHARED(mLock) {
    return mAvailableFonts;
  }
  void ReadLock() MOZ_ACQUIRE_SHARED(mLock) { mLock.ReadLock(); }
  void ReadUnlock() MOZ_RELEASE_SHARED(mLock) { mLock.ReadUnlock(); }

  uint32_t FontListLength() const {
    mozilla::AutoReadLock lock(mLock);
    return mAvailableFonts.Length();
  }

  void AddFontEntry(RefPtr<gfxFontEntry> aFontEntry) {
    mozilla::AutoWriteLock lock(mLock);
    AddFontEntryLocked(aFontEntry);
  }

  void AddFontEntryLocked(RefPtr<gfxFontEntry> aFontEntry) MOZ_REQUIRES(mLock) {
    // Avoid potentially duplicating entries.
    if (mAvailableFonts.Contains(aFontEntry)) {
      return;
    }
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
  bool HasStyles() const { return mHasStyles; }
  void SetHasStyles(bool aHasStyles) { mHasStyles = aHasStyles; }

  void SetCheckedForLegacyFamilyNames(bool aChecked) {
    mCheckedForLegacyFamilyNames = aChecked;
  }

  // choose a specific face to match a style using CSS font matching
  // rules (weight matching occurs here).  may return a face that doesn't
  // precisely match (e.g. normal face when no italic face exists).
  gfxFontEntry* FindFontForStyle(const gfxFontStyle& aFontStyle,
                                 bool aIgnoreSizeTolerance = false);

  virtual void FindAllFontsForStyle(const gfxFontStyle& aFontStyle,
                                    nsTArray<gfxFontEntry*>& aFontEntryList,
                                    bool aIgnoreSizeTolerance = false);

  // Checks for a matching font within the family; used as part of the font
  // fallback process.
  // Note that when this is called, the caller must already be holding the
  // gfxPlatformFontList lock.
  void FindFontForChar(GlobalFontMatch* aMatchData);

  // checks all fonts for a matching font within the family
  void SearchAllFontsForChar(GlobalFontMatch* aMatchData);

  // read in other family names, if any, and use functor to add each into cache
  virtual void ReadOtherFamilyNames(gfxPlatformFontList* aPlatformFontList);

  // set when other family names have been read in
  void SetOtherFamilyNamesInitialized() { mOtherFamilyNamesInitialized = true; }

  // Read in other localized family names, fullnames and Postscript names
  // for all faces and append to lookup tables.
  // Note that when this is called, the caller must already be holding the
  // gfxPlatformFontList lock.
  virtual void ReadFaceNames(gfxPlatformFontList* aPlatformFontList,
                             bool aNeedFullnamePostscriptNames,
                             FontInfoData* aFontInfoData = nullptr);

  // Find faces belonging to this family (platform implementations override).
  // This is a no-op in cases where the family is explicitly populated by other
  // means, rather than being asked to find its faces via system API.
  virtual void FindStyleVariationsLocked(FontInfoData* aFontInfoData = nullptr)
      MOZ_REQUIRES(mLock){};
  void FindStyleVariations(FontInfoData* aFontInfoData = nullptr) {
    if (mHasStyles) {
      return;
    }
    mozilla::AutoWriteLock lock(mLock);
    FindStyleVariationsLocked(aFontInfoData);
  }

  // search for a specific face using the Postscript name
  gfxFontEntry* FindFont(const nsACString& aFontName,
                         const nsCStringComparator& aCmp) const;

  // Read in cmaps for all the faces.
  // Note that when this is called, the caller must already be holding the
  // gfxPlatformFontList lock.
  void ReadAllCMAPs(FontInfoData* aFontInfoData = nullptr);

  bool TestCharacterMap(uint32_t aCh) {
    if (!mFamilyCharacterMapInitialized) {
      ReadAllCMAPs();
    }
    mozilla::AutoReadLock lock(mLock);
    return mFamilyCharacterMap.test(aCh);
  }

  void ResetCharacterMap() MOZ_REQUIRES(mLock) {
    mFamilyCharacterMap.reset();
    mFamilyCharacterMapInitialized = false;
  }

  // mark this family as being in the "bad" underline offset blocklist
  void SetBadUnderlineFamily() {
    mozilla::AutoWriteLock lock(mLock);
    mIsBadUnderlineFamily = true;
    if (mHasStyles) {
      SetBadUnderlineFonts();
    }
  }

  virtual bool IsSingleFaceFamily() const { return false; }

  bool IsBadUnderlineFamily() const { return mIsBadUnderlineFamily; }
  bool CheckForFallbackFaces() const { return mCheckForFallbackFaces; }

  // sort available fonts to put preferred (standard) faces towards the end
  void SortAvailableFonts() MOZ_REQUIRES(mLock);

  // check whether the family fits into the simple 4-face model,
  // so we can use simplified style-matching;
  // if so set the mIsSimpleFamily flag (defaults to False before we've checked)
  void CheckForSimpleFamily() MOZ_REQUIRES(mLock);

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
  void SetBadUnderlineFonts() MOZ_REQUIRES(mLock) {
    for (auto& f : mAvailableFonts) {
      if (f) {
        f->mIsBadUnderlineFont = true;
      }
    }
  }

  nsCString mName;
  nsTArray<RefPtr<gfxFontEntry>> mAvailableFonts MOZ_GUARDED_BY(mLock);
  gfxSparseBitSet mFamilyCharacterMap MOZ_GUARDED_BY(mLock);

  mutable mozilla::RWLock mLock;

  FontVisibility mVisibility;

  mozilla::Atomic<bool> mOtherFamilyNamesInitialized;
  mozilla::Atomic<bool> mFaceNamesInitialized;
  mozilla::Atomic<bool> mHasStyles;
  mozilla::Atomic<bool> mFamilyCharacterMapInitialized;
  mozilla::Atomic<bool> mCheckedForLegacyFamilyNames;
  mozilla::Atomic<bool> mHasOtherFamilyNames;

  bool mIsSimpleFamily : 1 MOZ_GUARDED_BY(mLock);
  bool mIsBadUnderlineFamily : 1;
  bool mSkipDefaultFeatureSpaceCheck : 1;
  bool mCheckForFallbackFaces : 1;  // check other faces for character

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

// Wrapper for either a raw pointer to a mozilla::fontlist::Family in the shared
// font list or a strong pointer to an unshared gfxFontFamily that belongs just
// to the current process.
struct FontFamily {
  FontFamily() = default;
  FontFamily(const FontFamily& aOther) = default;

  explicit FontFamily(RefPtr<gfxFontFamily>&& aFamily)
      : mUnshared(std::move(aFamily)) {}

  explicit FontFamily(gfxFontFamily* aFamily) : mUnshared(aFamily) {}

  explicit FontFamily(mozilla::fontlist::Family* aFamily) : mShared(aFamily) {}

  bool operator==(const FontFamily& aOther) const {
    return mShared == aOther.mShared && mUnshared == aOther.mUnshared;
  }

  bool IsNull() const { return !mShared && !mUnshared; }

  RefPtr<gfxFontFamily> mUnshared;
  mozilla::fontlist::Family* mShared = nullptr;
};

// Struct used in the gfxFontGroup font list to keep track of a font family
// together with the CSS generic (if any) that was mapped to it in this
// particular case (so it can be reported to the DevTools font inspector).
struct FamilyAndGeneric final {
  FamilyAndGeneric() : mGeneric(mozilla::StyleGenericFontFamily(0)) {}
  FamilyAndGeneric(const FamilyAndGeneric& aOther) = default;
  explicit FamilyAndGeneric(gfxFontFamily* aFamily,
                            mozilla::StyleGenericFontFamily aGeneric =
                                mozilla::StyleGenericFontFamily(0))
      : mFamily(aFamily), mGeneric(aGeneric) {}
  explicit FamilyAndGeneric(RefPtr<gfxFontFamily>&& aFamily,
                            mozilla::StyleGenericFontFamily aGeneric =
                                mozilla::StyleGenericFontFamily(0))
      : mFamily(std::move(aFamily)), mGeneric(aGeneric) {}
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

/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FONTENTRY_H
#define GFX_FONTENTRY_H

#include "gfxTypes.h"
#include "nsString.h"
#include "gfxFontConstants.h"
#include "gfxFontFeatures.h"
#include "gfxFontUtils.h"
#include "nsTArray.h"
#include "nsTHashtable.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/MemoryReporting.h"
#include "DrawMode.h"
#include "nsUnicodeScriptCodes.h"
#include "nsDataHashtable.h"
#include "harfbuzz/hb.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/UniquePtr.h"

typedef struct gr_face gr_face;

#ifdef DEBUG
#include <stdio.h>
#endif

struct gfxFontStyle;
class gfxContext;
class gfxFont;
class gfxFontFamily;
class gfxUserFontData;
class gfxSVGGlyphs;
class gfxMathTable;
class gfxTextContextPaint;
class FontInfoData;
struct FontListSizes;
class nsIAtom;

class gfxCharacterMap : public gfxSparseBitSet {
public:
    nsrefcnt AddRef() {
        NS_PRECONDITION(int32_t(mRefCnt) >= 0, "illegal refcnt");
        ++mRefCnt;
        NS_LOG_ADDREF(this, mRefCnt, "gfxCharacterMap", sizeof(*this));
        return mRefCnt;
    }

    nsrefcnt Release() {
        NS_PRECONDITION(0 != mRefCnt, "dup release");
        --mRefCnt;
        NS_LOG_RELEASE(this, mRefCnt, "gfxCharacterMap");
        if (mRefCnt == 0) {
            NotifyReleased();
            // |this| has been deleted.
            return 0;
        }
        return mRefCnt;
    }

    gfxCharacterMap() :
        mHash(0), mBuildOnTheFly(false), mShared(false)
    { }

    explicit gfxCharacterMap(const gfxSparseBitSet& aOther) :
        gfxSparseBitSet(aOther),
        mHash(0), mBuildOnTheFly(false), mShared(false)
    { }

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

class gfxFontEntry {
public:
    typedef mozilla::gfx::DrawTarget DrawTarget;

    NS_INLINE_DECL_REFCOUNTING(gfxFontEntry)

    explicit gfxFontEntry(const nsAString& aName, bool aIsStandardFace = false);

    // unique name for the face, *not* the family; not necessarily the
    // "real" or user-friendly name, may be an internal identifier
    const nsString& Name() const { return mName; }

    // family name
    const nsString& FamilyName() const { return mFamilyName; }

    // The following two methods may be relatively expensive, as they
    // will (usually, except on Linux) load and parse the 'name' table;
    // they are intended only for the font-inspection API, not for
    // perf-critical layout/drawing work.

    // The "real" name of the face, if available from the font resource;
    // returns Name() if nothing better is available.
    virtual nsString RealFaceName();

    uint16_t Weight() const { return mWeight; }
    int16_t Stretch() const { return mStretch; }

    bool IsUserFont() const { return mIsDataUserFont || mIsLocalUserFont; }
    bool IsLocalUserFont() const { return mIsLocalUserFont; }
    bool IsFixedPitch() const { return mFixedPitch; }
    bool IsItalic() const { return mStyle == NS_FONT_STYLE_ITALIC; }
    bool IsOblique() const { return mStyle == NS_FONT_STYLE_OBLIQUE; }
    bool IsUpright() const { return mStyle == NS_FONT_STYLE_NORMAL; }
    bool IsBold() const { return mWeight >= 600; } // bold == weights 600 and above
    bool IgnoreGDEF() const { return mIgnoreGDEF; }
    bool IgnoreGSUB() const { return mIgnoreGSUB; }

    // whether a feature is supported by the font (limited to a small set
    // of features for which some form of fallback needs to be implemented)
    bool SupportsOpenTypeFeature(int32_t aScript, uint32_t aFeatureTag);
    bool SupportsGraphiteFeature(uint32_t aFeatureTag);

    // returns a set containing all input glyph ids for a given feature
    const hb_set_t*
    InputsForOpenTypeFeature(int32_t aScript, uint32_t aFeatureTag);

    virtual bool IsSymbolFont();

    virtual bool HasFontTable(uint32_t aTableTag);

    inline bool HasGraphiteTables() {
        if (!mCheckedForGraphiteTables) {
            CheckForGraphiteTables();
            mCheckedForGraphiteTables = true;
        }
        return mHasGraphiteTables;
    }

    inline bool HasCmapTable() {
        if (!mCharacterMap) {
            ReadCMAP();
            NS_ASSERTION(mCharacterMap, "failed to initialize character map");
        }
        return mHasCmapTable;
    }

    inline bool HasCharacter(uint32_t ch) {
        if (mCharacterMap && mCharacterMap->test(ch)) {
            return true;
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
    virtual nsresult ReadCMAP(FontInfoData *aFontInfoData = nullptr);

    bool TryGetSVGData(gfxFont* aFont);
    bool HasSVGGlyph(uint32_t aGlyphId);
    bool GetSVGGlyphExtents(DrawTarget* aDrawTarget, uint32_t aGlyphId,
                            gfxRect *aResult);
    bool RenderSVGGlyph(gfxContext *aContext, uint32_t aGlyphId, int aDrawMode,
                        gfxTextContextPaint *aContextPaint);
    // Call this when glyph geometry or rendering has changed
    // (e.g. animated SVG glyphs)
    void NotifyGlyphsChanged();

    enum MathConstant {
        // The order of the constants must match the order of the fields
        // defined in the MATH table.
        ScriptPercentScaleDown,
        ScriptScriptPercentScaleDown,
        DelimitedSubFormulaMinHeight,
        DisplayOperatorMinHeight,
        MathLeading,
        AxisHeight,
        AccentBaseHeight,
        FlattenedAccentBaseHeight,
        SubscriptShiftDown,
        SubscriptTopMax,
        SubscriptBaselineDropMin,
        SuperscriptShiftUp,
        SuperscriptShiftUpCramped,
        SuperscriptBottomMin,
        SuperscriptBaselineDropMax,
        SubSuperscriptGapMin,
        SuperscriptBottomMaxWithSubscript,
        SpaceAfterScript,
        UpperLimitGapMin,
        UpperLimitBaselineRiseMin,
        LowerLimitGapMin,
        LowerLimitBaselineDropMin,
        StackTopShiftUp,
        StackTopDisplayStyleShiftUp,
        StackBottomShiftDown,
        StackBottomDisplayStyleShiftDown,
        StackGapMin,
        StackDisplayStyleGapMin,
        StretchStackTopShiftUp,
        StretchStackBottomShiftDown,
        StretchStackGapAboveMin,
        StretchStackGapBelowMin,
        FractionNumeratorShiftUp,
        FractionNumeratorDisplayStyleShiftUp,
        FractionDenominatorShiftDown,
        FractionDenominatorDisplayStyleShiftDown,
        FractionNumeratorGapMin,
        FractionNumDisplayStyleGapMin,
        FractionRuleThickness,
        FractionDenominatorGapMin,
        FractionDenomDisplayStyleGapMin,
        SkewedFractionHorizontalGap,
        SkewedFractionVerticalGap,
        OverbarVerticalGap,
        OverbarRuleThickness,
        OverbarExtraAscender,
        UnderbarVerticalGap,
        UnderbarRuleThickness,
        UnderbarExtraDescender,
        RadicalVerticalGap,
        RadicalDisplayStyleVerticalGap,
        RadicalRuleThickness,
        RadicalExtraAscender,
        RadicalKernBeforeDegree,
        RadicalKernAfterDegree,
        RadicalDegreeBottomRaisePercent
    };

    // Call TryGetMathTable to try to load the Open Type MATH table. The other
    // functions forward the call to the gfxMathTable class. The GetMath...()
    // functions MUST NOT be called unless TryGetMathTable() has returned true.
    bool     TryGetMathTable();
    gfxFloat GetMathConstant(MathConstant aConstant);
    bool     GetMathItalicsCorrection(uint32_t aGlyphID,
                                      gfxFloat* aItalicCorrection);
    uint32_t GetMathVariantsSize(uint32_t aGlyphID, bool aVertical,
                                 uint16_t aSize);
    bool     GetMathVariantsParts(uint32_t aGlyphID, bool aVertical,
                                  uint32_t aGlyphs[4]);

    bool     TryGetColorGlyphs();
    bool     GetColorLayersInfo(uint32_t aGlyphId,
                                nsTArray<uint16_t>& layerGlyphs,
                                nsTArray<mozilla::gfx::Color>& layerColors);

    virtual bool MatchesGenericFamily(const nsACString& aGeneric) const {
        return true;
    }
    virtual bool SupportsLangGroup(nsIAtom *aLangGroup) const {
        return true;
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
    virtual hb_blob_t *GetFontTable(uint32_t aTag);

    // Stack-based utility to return a specified table, automatically releasing
    // the blob when the AutoTable goes out of scope.
    class AutoTable {
    public:
        AutoTable(gfxFontEntry* aFontEntry, uint32_t aTag)
        {
            mBlob = aFontEntry->GetFontTable(aTag);
        }
        ~AutoTable() {
            if (mBlob) {
                hb_blob_destroy(mBlob);
            }
        }
        operator hb_blob_t*() const { return mBlob; }
    private:
        hb_blob_t* mBlob;
        // not implemented:
        AutoTable(const AutoTable&) = delete;
        AutoTable& operator=(const AutoTable&) = delete;
    };

    already_AddRefed<gfxFont>
    FindOrMakeFont(const gfxFontStyle *aStyle,
                   bool aNeedsBold,
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
    hb_blob_t *ShareFontTableAndGetBlob(uint32_t aTag,
                                        FallibleTArray<uint8_t>* aTable);

    // Get the font's unitsPerEm from the 'head' table, in the case of an
    // sfnt resource. Will return kInvalidUPEM for non-sfnt fonts,
    // if present on the platform.
    uint16_t UnitsPerEm();
    enum {
        kMinUPEM = 16,    // Limits on valid unitsPerEm range, from the
        kMaxUPEM = 16384, // OpenType spec
        kInvalidUPEM = uint16_t(-1)
    };

    // Shaper face accessors:
    // NOTE that harfbuzz and graphite handle ownership/lifetime of the face
    // object in completely different ways.

    // Get HarfBuzz face corresponding to this font file.
    // Caller must release with hb_face_destroy() when finished with it,
    // and the font entry will be notified via ForgetHBFace.
    hb_face_t* GetHBFace();
    virtual void ForgetHBFace();

    // Get Graphite face corresponding to this font file.
    // Caller must call gfxFontEntry::ReleaseGrFace when finished with it.
    gr_face* GetGrFace();
    virtual void ReleaseGrFace(gr_face* aFace);

    // Does the font have graphite contextuals that involve the space glyph
    // (and therefore we should bypass the word cache)?
    bool HasGraphiteSpaceContextuals();

    // Release any SVG-glyphs document this font may have loaded.
    void DisconnectSVG();

    // Called to notify that aFont is being destroyed. Needed when we're tracking
    // the fonts belonging to this font entry.
    void NotifyFontDestroyed(gfxFont* aFont);

    // For memory reporting
    virtual void AddSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                        FontListSizes* aSizes) const;
    virtual void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                        FontListSizes* aSizes) const;

    // Used when checking for complex script support, to mask off cmap ranges
    struct ScriptRange {
        uint32_t         rangeStart;
        uint32_t         rangeEnd;
        hb_tag_t         tags[3]; // one or two OpenType script tags to check,
                                  // plus a NULL terminator
    };

    bool SupportsScriptInGSUB(const hb_tag_t* aScriptTags);

    nsString         mName;
    nsString         mFamilyName;

    uint8_t          mStyle       : 2; // italic/oblique
    bool             mFixedPitch  : 1;
    bool             mIsValid     : 1;
    bool             mIsBadUnderlineFont : 1;
    bool             mIsUserFontContainer : 1; // userfont entry
    bool             mIsDataUserFont : 1;      // platform font entry (data)
    bool             mIsLocalUserFont : 1;     // platform font entry (local)
    bool             mStandardFace : 1;
    bool             mSymbolFont  : 1;
    bool             mIgnoreGDEF  : 1;
    bool             mIgnoreGSUB  : 1;
    bool             mSVGInitialized : 1;
    bool             mMathInitialized : 1;
    bool             mHasSpaceFeaturesInitialized : 1;
    bool             mHasSpaceFeatures : 1;
    bool             mHasSpaceFeaturesKerning : 1;
    bool             mHasSpaceFeaturesNonKerning : 1;
    bool             mSkipDefaultFeatureSpaceCheck : 1;
    bool             mGraphiteSpaceContextualsInitialized : 1;
    bool             mHasGraphiteSpaceContextuals : 1;
    bool             mSpaceGlyphIsInvisible : 1;
    bool             mSpaceGlyphIsInvisibleInitialized : 1;
    bool             mHasGraphiteTables : 1;
    bool             mCheckedForGraphiteTables : 1;
    bool             mHasCmapTable : 1;
    bool             mGrFaceInitialized : 1;
    bool             mCheckedForColorGlyph : 1;

    // bitvector of substitution space features per script, one each
    // for default and non-default features
    uint32_t         mDefaultSubSpaceFeatures[(MOZ_NUM_SCRIPT_CODES + 31) / 32];
    uint32_t         mNonDefaultSubSpaceFeatures[(MOZ_NUM_SCRIPT_CODES + 31) / 32];

    uint16_t         mWeight;
    int16_t          mStretch;

    RefPtr<gfxCharacterMap> mCharacterMap;
    uint32_t         mUVSOffset;
    mozilla::UniquePtr<uint8_t[]> mUVSData;
    nsAutoPtr<gfxUserFontData> mUserFontData;
    nsAutoPtr<gfxSVGGlyphs> mSVGGlyphs;
    // list of gfxFonts that are using SVG glyphs
    nsTArray<gfxFont*> mFontsUsingSVGGlyphs;
    nsAutoPtr<gfxMathTable> mMathTable;
    nsTArray<gfxFontFeature> mFeatureSettings;
    nsAutoPtr<nsDataHashtable<nsUint32HashKey,bool>> mSupportedFeatures;
    nsAutoPtr<nsDataHashtable<nsUint32HashKey,hb_set_t*>> mFeatureInputs;
    uint32_t         mLanguageOverride;

    // Color Layer font support
    hb_blob_t*       mCOLR;
    hb_blob_t*       mCPAL;

protected:
    friend class gfxPlatformFontList;
    friend class gfxMacPlatformFontList;
    friend class gfxUserFcFontEntry;
    friend class gfxFontFamily;
    friend class gfxSingleFaceMacFontFamily;

    gfxFontEntry();

    // Protected destructor, to discourage deletion outside of Release():
    virtual ~gfxFontEntry();

    virtual gfxFont *CreateFontInstance(const gfxFontStyle *aFontStyle, bool aNeedsBold) {
        NS_NOTREACHED("oops, somebody didn't override CreateFontInstance");
        return nullptr;
    }

    virtual void CheckForGraphiteTables();

    // Copy a font table into aBuffer.
    // The caller will be responsible for ownership of the data.
    virtual nsresult CopyFontTable(uint32_t aTableTag,
                                   FallibleTArray<uint8_t>& aBuffer) {
        NS_NOTREACHED("forgot to override either GetFontTable or CopyFontTable?");
        return NS_ERROR_FAILURE;
    }

    // Return a blob that wraps a table found within a buffer of font data.
    // The blob does NOT own its data; caller guarantees that the buffer
    // will remain valid at least as long as the blob.
    // Returns null if the specified table is not found.
    // This method assumes aFontData is valid 'sfnt' data; before using this,
    // caller is responsible to do any sanitization/validation necessary.
    hb_blob_t* GetTableFromFontData(const void* aFontData, uint32_t aTableTag);

    // lookup the cmap in cached font data
    virtual already_AddRefed<gfxCharacterMap>
    GetCMAPFromFontInfo(FontInfoData *aFontInfoData,
                        uint32_t& aUVSOffset,
                        bool& aSymbolFont);

    // helper for HasCharacter(), which is what client code should call
    virtual bool TestCharacterMap(uint32_t aCh);

    // Font's unitsPerEm from the 'head' table, if available (will be set to
    // kInvalidUPEM for non-sfnt font formats)
    uint16_t mUnitsPerEm;

    // Shaper-specific face objects, shared by all instantiations of the same
    // physical font, regardless of size.
    // Usually, only one of these will actually be created for any given font
    // entry, depending on the font tables that are present.

    // hb_face_t is refcounted internally, so each shaper that's using it will
    // bump the ref count when it acquires the face, and "destroy" (release) it
    // in its destructor. The font entry has only this non-owning reference to
    // the face; when the face is deleted, it will tell the font entry to forget
    // it, so that a new face will be created next time it is needed.
    hb_face_t* mHBFace;

    static hb_blob_t* HBGetTable(hb_face_t *face, uint32_t aTag, void *aUserData);

    // Callback that the hb_face will use to tell us when it is being deleted.
    static void HBFaceDeletedCallback(void *aUserData);

    // gr_face is -not- refcounted, so it will be owned directly by the font
    // entry, and we'll keep a count of how many references we've handed out;
    // each shaper is responsible to call ReleaseGrFace on its entry when
    // finished with it, so that we know when it can be deleted.
    gr_face*   mGrFace;

    // hashtable to map raw table data ptr back to its owning blob, for use by
    // graphite table-release callback
    nsDataHashtable<nsPtrHashKey<const void>,void*>* mGrTableMap;

    // number of current users of this entry's mGrFace
    nsrefcnt mGrFaceRefCnt;

    static const void* GrGetTable(const void *aAppFaceHandle,
                                  unsigned int aName,
                                  size_t *aLen);
    static void GrReleaseTable(const void *aAppFaceHandle,
                               const void *aTableBuffer);

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

    class FontTableHashEntry : public nsUint32HashKey
    {
    public:
        // Declarations for nsTHashtable

        typedef nsUint32HashKey KeyClass;
        typedef KeyClass::KeyType KeyType;
        typedef KeyClass::KeyTypePointer KeyTypePointer;

        explicit FontTableHashEntry(KeyTypePointer aTag)
            : KeyClass(aTag)
            , mSharedBlobData(nullptr)
            , mBlob(nullptr)
        { }

        // NOTE: This assumes the new entry belongs to the same hashtable as
        // the old, because the mHashtable pointer in mSharedBlobData (if
        // present) will not be updated.
        FontTableHashEntry(FontTableHashEntry&& toMove)
            : KeyClass(mozilla::Move(toMove))
            , mSharedBlobData(mozilla::Move(toMove.mSharedBlobData))
            , mBlob(mozilla::Move(toMove.mBlob))
        {
            toMove.mSharedBlobData = nullptr;
            toMove.mBlob = nullptr;
        }

        ~FontTableHashEntry() { Clear(); }

        // FontTable/Blob API

        // Transfer (not copy) elements of aTable to a new hb_blob_t and
        // return ownership to the caller.  A weak reference to the blob is
        // recorded in the hashtable entry so that others may use the same
        // table.
        hb_blob_t *
        ShareTableAndGetBlob(FallibleTArray<uint8_t>& aTable,
                             nsTHashtable<FontTableHashEntry> *aHashtable);

        // Return a strong reference to the blob.
        // Callers must hb_blob_destroy the returned blob.
        hb_blob_t *GetBlob() const;

        void Clear();

        size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

    private:
        static void DeleteFontTableBlobData(void *aBlobData);
        // not implemented
        FontTableHashEntry& operator=(FontTableHashEntry& toCopy);

        FontTableBlobData *mSharedBlobData;
        hb_blob_t *mBlob;
    };

    nsAutoPtr<nsTHashtable<FontTableHashEntry> > mFontTableCache;

    gfxFontEntry(const gfxFontEntry&);
    gfxFontEntry& operator=(const gfxFontEntry&);
};


// used when iterating over all fonts looking for a match for a given character
struct GlobalFontMatch {
    GlobalFontMatch(const uint32_t aCharacter,
                    int32_t aRunScript,
                    const gfxFontStyle *aStyle) :
        mCh(aCharacter), mRunScript(aRunScript), mStyle(aStyle),
        mMatchRank(0), mCount(0), mCmapsTested(0)
        {

        }

    const uint32_t         mCh;          // codepoint to be matched
    int32_t                mRunScript;   // Unicode script for the codepoint
    const gfxFontStyle*    mStyle;       // style to match
    int32_t                mMatchRank;   // metric indicating closest match
    RefPtr<gfxFontEntry> mBestMatch;   // current best match
    RefPtr<gfxFontFamily> mMatchedFamily; // the family it belongs to
    uint32_t               mCount;       // number of fonts matched
    uint32_t               mCmapsTested; // number of cmaps tested
};

class gfxFontFamily {
public:
    NS_INLINE_DECL_REFCOUNTING(gfxFontFamily)

    explicit gfxFontFamily(const nsAString& aName) :
        mName(aName),
        mOtherFamilyNamesInitialized(false),
        mHasOtherFamilyNames(false),
        mFaceNamesInitialized(false),
        mHasStyles(false),
        mIsSimpleFamily(false),
        mIsBadUnderlineFamily(false),
        mFamilyCharacterMapInitialized(false),
        mSkipDefaultFeatureSpaceCheck(false),
        mCheckForFallbackFaces(false)
        { }

    const nsString& Name() { return mName; }

    virtual void LocalizedName(nsAString& aLocalizedName);
    virtual bool HasOtherFamilyNames();
    
    nsTArray<RefPtr<gfxFontEntry> >& GetFontList() { return mAvailableFonts; }
    
    void AddFontEntry(RefPtr<gfxFontEntry> aFontEntry) {
        // bug 589682 - set the IgnoreGDEF flag on entries for Italic faces
        // of Times New Roman, because of buggy table in those fonts
        if (aFontEntry->IsItalic() && !aFontEntry->IsUserFont() &&
            Name().EqualsLiteral("Times New Roman"))
        {
            aFontEntry->mIgnoreGDEF = true;
        }
        if (aFontEntry->mFamilyName.IsEmpty()) {
            aFontEntry->mFamilyName = Name();
        } else {
            MOZ_ASSERT(aFontEntry->mFamilyName.Equals(Name()));
        }
        aFontEntry->mSkipDefaultFeatureSpaceCheck = mSkipDefaultFeatureSpaceCheck;
        mAvailableFonts.AppendElement(aFontEntry);
    }

    // note that the styles for this family have been added
    bool HasStyles() { return mHasStyles; }
    void SetHasStyles(bool aHasStyles) { mHasStyles = aHasStyles; }

    // choose a specific face to match a style using CSS font matching
    // rules (weight matching occurs here).  may return a face that doesn't
    // precisely match (e.g. normal face when no italic face exists).
    // aNeedsSyntheticBold is set to true when synthetic bolding is
    // needed, false otherwise
    gfxFontEntry *FindFontForStyle(const gfxFontStyle& aFontStyle, 
                                   bool& aNeedsSyntheticBold);

    void
    FindAllFontsForStyle(const gfxFontStyle& aFontStyle,
                         nsTArray<gfxFontEntry*>& aFontEntryList,
                         bool& aNeedsSyntheticBold);

    // checks for a matching font within the family
    // used as part of the font fallback process
    void FindFontForChar(GlobalFontMatch *aMatchData);

    // checks all fonts for a matching font within the family
    void SearchAllFontsForChar(GlobalFontMatch *aMatchData);

    // read in other family names, if any, and use functor to add each into cache
    virtual void ReadOtherFamilyNames(gfxPlatformFontList *aPlatformFontList);

    // helper method for reading localized family names from the name table
    // of a single face
    static void ReadOtherFamilyNamesForFace(const nsAString& aFamilyName,
                                            const char *aNameData,
                                            uint32_t aDataLength,
                                            nsTArray<nsString>& aOtherFamilyNames,
                                            bool useFullName);

    // set when other family names have been read in
    void SetOtherFamilyNamesInitialized() {
        mOtherFamilyNamesInitialized = true;
    }

    // read in other localized family names, fullnames and Postscript names
    // for all faces and append to lookup tables
    virtual void ReadFaceNames(gfxPlatformFontList *aPlatformFontList,
                               bool aNeedFullnamePostscriptNames,
                               FontInfoData *aFontInfoData = nullptr);

    // find faces belonging to this family (platform implementations override this;
    // should be made pure virtual once all subclasses have been updated)
    virtual void FindStyleVariations(FontInfoData *aFontInfoData = nullptr) { }

    // search for a specific face using the Postscript name
    gfxFontEntry* FindFont(const nsAString& aPostscriptName);

    // read in cmaps for all the faces
    void ReadAllCMAPs(FontInfoData *aFontInfoData = nullptr);

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

    // mark this family as being in the "bad" underline offset blacklist
    void SetBadUnderlineFamily() {
        mIsBadUnderlineFamily = true;
        if (mHasStyles) {
            SetBadUnderlineFonts();
        }
    }

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

protected:
    // Protected destructor, to discourage deletion outside of Release():
    virtual ~gfxFontFamily()
    {
    }

    bool ReadOtherFamilyNamesForFace(gfxPlatformFontList *aPlatformFontList,
                                     hb_blob_t           *aNameTable,
                                     bool                 useFullName = false);

    // set whether this font family is in "bad" underline offset blacklist.
    void SetBadUnderlineFonts() {
        uint32_t i, numFonts = mAvailableFonts.Length();
        for (i = 0; i < numFonts; i++) {
            if (mAvailableFonts[i]) {
                mAvailableFonts[i]->mIsBadUnderlineFont = true;
            }
        }
    }

    nsString mName;
    nsTArray<RefPtr<gfxFontEntry> >  mAvailableFonts;
    gfxSparseBitSet mFamilyCharacterMap;
    bool mOtherFamilyNamesInitialized : 1;
    bool mHasOtherFamilyNames : 1;
    bool mFaceNamesInitialized : 1;
    bool mHasStyles : 1;
    bool mIsSimpleFamily : 1;
    bool mIsBadUnderlineFamily : 1;
    bool mFamilyCharacterMapInitialized : 1;
    bool mSkipDefaultFeatureSpaceCheck : 1;
    bool mCheckForFallbackFaces : 1;  // check other faces for character

    enum {
        // for "simple" families, the faces are stored in mAvailableFonts
        // with fixed positions:
        kRegularFaceIndex    = 0,
        kBoldFaceIndex       = 1,
        kItalicFaceIndex     = 2,
        kBoldItalicFaceIndex = 3,
        // mask values for selecting face with bold and/or italic attributes
        kBoldMask   = 0x01,
        kItalicMask = 0x02
    };
};

#endif

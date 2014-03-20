/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FONT_H
#define GFX_FONT_H

#include "gfxTypes.h"
#include "nsString.h"
#include "gfxPoint.h"
#include "gfxFontUtils.h"
#include "nsTArray.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "gfxSkipChars.h"
#include "gfxRect.h"
#include "nsExpirationTracker.h"
#include "gfxPlatform.h"
#include "nsIAtom.h"
#include "mozilla/HashFunctions.h"
#include "nsIMemoryReporter.h"
#include "nsIObserver.h"
#include "gfxFontFeatures.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Attributes.h"
#include <algorithm>
#include "DrawMode.h"
#include "nsUnicodeScriptCodes.h"
#include "nsDataHashtable.h"
#include "harfbuzz/hb.h"
#include "mozilla/gfx/2D.h"

typedef struct _cairo_scaled_font cairo_scaled_font_t;
typedef struct gr_face            gr_face;

#ifdef DEBUG
#include <stdio.h>
#endif

class gfxContext;
class gfxTextRun;
class gfxFont;
class gfxFontFamily;
class gfxFontGroup;
class gfxUserFontSet;
class gfxUserFontData;
class gfxShapedText;
class gfxShapedWord;
class gfxSVGGlyphs;
class gfxTextContextPaint;
class FontInfoData;

class nsILanguageAtomService;

#define FONT_MAX_SIZE                  2000.0

#define NO_FONT_LANGUAGE_OVERRIDE      0

struct FontListSizes;
struct gfxTextRunDrawCallbacks;

namespace mozilla {
namespace gfx {
class GlyphRenderingOptions;
}
}

struct gfxFontStyle {
    gfxFontStyle();
    gfxFontStyle(uint8_t aStyle, uint16_t aWeight, int16_t aStretch,
                 gfxFloat aSize, nsIAtom *aLanguage,
                 float aSizeAdjust, bool aSystemFont,
                 bool aPrinterFont,
                 const nsString& aLanguageOverride);
    gfxFontStyle(const gfxFontStyle& aStyle);

    // the language (may be an internal langGroup code rather than an actual
    // language code) specified in the document or element's lang property,
    // or inferred from the charset
    nsRefPtr<nsIAtom> language;

    // Features are composed of (1) features from style rules (2) features
    // from feature setttings rules and (3) family-specific features.  (1) and
    // (3) are guaranteed to be mutually exclusive

    // custom opentype feature settings
    nsTArray<gfxFontFeature> featureSettings;

    // Some font-variant property values require font-specific settings
    // defined via @font-feature-values rules.  These are resolved after
    // font matching occurs.

    // -- list of value tags for specific alternate features
    nsTArray<gfxAlternateValue> alternateValues;

    // -- object used to look these up once the font is matched
    nsRefPtr<gfxFontFeatureValueSet> featureValueLookup;

    // The logical size of the font, in pixels
    gfxFloat size;

    // The aspect-value (ie., the ratio actualsize:actualxheight) that any
    // actual physical font created from this font structure must have when
    // rendering or measuring a string. A value of 0 means no adjustment
    // needs to be done.
    float sizeAdjust;

    // Language system tag, to override document language;
    // an OpenType "language system" tag represented as a 32-bit integer
    // (see http://www.microsoft.com/typography/otspec/languagetags.htm).
    // Normally 0, so font rendering will use the document or element language
    // (see above) to control any language-specific rendering, but the author
    // can override this for cases where the options implemented in the font
    // do not directly match the actual language. (E.g. lang may be Macedonian,
    // but the font in use does not explicitly support this; the author can
    // use font-language-override to request the Serbian option in the font
    // in order to get correct glyph shapes.)
    uint32_t languageOverride;

    // The weight of the font: 100, 200, ... 900.
    uint16_t weight;

    // The stretch of the font (the sum of various NS_FONT_STRETCH_*
    // constants; see gfxFontConstants.h).
    int8_t stretch;

    // Say that this font is a system font and therefore does not
    // require certain fixup that we do for fonts from untrusted
    // sources.
    bool systemFont : 1;

    // Say that this font is used for print or print preview.
    bool printerFont : 1;

    // Used to imitate -webkit-font-smoothing: antialiased
    bool useGrayscaleAntialiasing : 1;

    // The style of font (normal, italic, oblique)
    uint8_t style : 2;

    // Return the final adjusted font size for the given aspect ratio.
    // Not meant to be called when sizeAdjust = 0.
    gfxFloat GetAdjustedSize(gfxFloat aspect) const {
        NS_ASSERTION(sizeAdjust != 0.0, "Not meant to be called when sizeAdjust = 0");
        gfxFloat adjustedSize = std::max(NS_round(size*(sizeAdjust/aspect)), 1.0);
        return std::min(adjustedSize, FONT_MAX_SIZE);
    }

    PLDHashNumber Hash() const {
        return ((style + (systemFont << 7) +
            (weight << 8)) + uint32_t(size*1000) + uint32_t(sizeAdjust*1000)) ^
            nsISupportsHashKey::HashKey(language);
    }

    int8_t ComputeWeight() const;

    bool Equals(const gfxFontStyle& other) const {
        return
            (*reinterpret_cast<const uint64_t*>(&size) ==
             *reinterpret_cast<const uint64_t*>(&other.size)) &&
            (style == other.style) &&
            (systemFont == other.systemFont) &&
            (printerFont == other.printerFont) &&
            (useGrayscaleAntialiasing == other.useGrayscaleAntialiasing) &&
            (weight == other.weight) &&
            (stretch == other.stretch) &&
            (language == other.language) &&
            (*reinterpret_cast<const uint32_t*>(&sizeAdjust) ==
             *reinterpret_cast<const uint32_t*>(&other.sizeAdjust)) &&
            (featureSettings == other.featureSettings) &&
            (languageOverride == other.languageOverride) &&
            (alternateValues == other.alternateValues) &&
            (featureValueLookup == other.featureValueLookup);
    }

    static void ParseFontFeatureSettings(const nsString& aFeatureString,
                                         nsTArray<gfxFontFeature>& aFeatures);

    static uint32_t ParseFontLanguageOverride(const nsString& aLangTag);
};

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
    NS_INLINE_DECL_REFCOUNTING(gfxFontEntry)

    gfxFontEntry(const nsAString& aName, bool aIsStandardFace = false);
    virtual ~gfxFontEntry();

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

    bool IsUserFont() const { return mIsUserFont; }
    bool IsLocalUserFont() const { return mIsLocalUserFont; }
    bool IsFixedPitch() const { return mFixedPitch; }
    bool IsItalic() const { return mItalic; }
    bool IsBold() const { return mWeight >= 600; } // bold == weights 600 and above
    bool IgnoreGDEF() const { return mIgnoreGDEF; }
    bool IgnoreGSUB() const { return mIgnoreGSUB; }

    virtual bool IsSymbolFont();

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
    virtual bool TestCharacterMap(uint32_t aCh);
    nsresult InitializeUVSMap();
    uint16_t GetUVSGlyph(uint32_t aCh, uint32_t aVS);

    // All concrete gfxFontEntry subclasses (except gfxProxyFontEntry) need
    // to override this, otherwise the font will never be used as it will
    // be considered to support no characters.
    // ReadCMAP() must *always* set the mCharacterMap pointer to a valid
    // gfxCharacterMap, even if empty, as other code assumes this pointer
    // can be safely dereferenced.
    virtual nsresult ReadCMAP(FontInfoData *aFontInfoData = nullptr);

    bool TryGetSVGData(gfxFont* aFont);
    bool HasSVGGlyph(uint32_t aGlyphId);
    bool GetSVGGlyphExtents(gfxContext *aContext, uint32_t aGlyphId,
                            gfxRect *aResult);
    bool RenderSVGGlyph(gfxContext *aContext, uint32_t aGlyphId, int aDrawMode,
                        gfxTextContextPaint *aContextPaint);
    // Call this when glyph geometry or rendering has changed
    // (e.g. animated SVG glyphs)
    void NotifyGlyphsChanged();

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
        AutoTable(const AutoTable&) MOZ_DELETE;
        AutoTable& operator=(const AutoTable&) MOZ_DELETE;
    };

    already_AddRefed<gfxFont> FindOrMakeFont(const gfxFontStyle *aStyle,
                                             bool aNeedsBold);

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

    nsString         mName;
    nsString         mFamilyName;

    bool             mItalic      : 1;
    bool             mFixedPitch  : 1;
    bool             mIsProxy     : 1;
    bool             mIsValid     : 1;
    bool             mIsBadUnderlineFont : 1;
    bool             mIsUserFont  : 1;
    bool             mIsLocalUserFont  : 1;
    bool             mStandardFace : 1;
    bool             mSymbolFont  : 1;
    bool             mIgnoreGDEF  : 1;
    bool             mIgnoreGSUB  : 1;
    bool             mSVGInitialized : 1;
    bool             mHasSpaceFeaturesInitialized : 1;
    bool             mHasSpaceFeatures : 1;
    bool             mHasSpaceFeaturesKerning : 1;
    bool             mHasSpaceFeaturesNonKerning : 1;
    bool             mSkipDefaultFeatureSpaceCheck : 1;
    bool             mHasGraphiteTables : 1;
    bool             mCheckedForGraphiteTables : 1;
    bool             mHasCmapTable : 1;
    bool             mGrFaceInitialized : 1;

    // bitvector of substitution space features per script, one each
    // for default and non-default features
    uint32_t         mDefaultSubSpaceFeatures[(MOZ_NUM_SCRIPT_CODES + 31) / 32];
    uint32_t         mNonDefaultSubSpaceFeatures[(MOZ_NUM_SCRIPT_CODES + 31) / 32];

    uint16_t         mWeight;
    int16_t          mStretch;

    nsRefPtr<gfxCharacterMap> mCharacterMap;
    uint32_t         mUVSOffset;
    nsAutoArrayPtr<uint8_t> mUVSData;
    nsAutoPtr<gfxUserFontData> mUserFontData;
    nsAutoPtr<gfxSVGGlyphs> mSVGGlyphs;
    // list of gfxFonts that are using SVG glyphs
    nsTArray<gfxFont*> mFontsUsingSVGGlyphs;
    nsTArray<gfxFontFeature> mFeatureSettings;
    uint32_t         mLanguageOverride;

protected:
    friend class gfxPlatformFontList;
    friend class gfxMacPlatformFontList;
    friend class gfxUserFcFontEntry;
    friend class gfxFontFamily;
    friend class gfxSingleFaceMacFontFamily;

    gfxFontEntry();

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

        FontTableHashEntry(KeyTypePointer aTag)
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

        static size_t
        SizeOfEntryExcludingThis(FontTableHashEntry *aEntry,
                                 mozilla::MallocSizeOf aMallocSizeOf,
                                 void* aUserArg);

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
    nsRefPtr<gfxFontEntry> mBestMatch;   // current best match
    nsRefPtr<gfxFontFamily> mMatchedFamily; // the family it belongs to
    uint32_t               mCount;       // number of fonts matched
    uint32_t               mCmapsTested; // number of cmaps tested
};

class gfxFontFamily {
public:
    NS_INLINE_DECL_REFCOUNTING(gfxFontFamily)

    gfxFontFamily(const nsAString& aName) :
        mName(aName),
        mOtherFamilyNamesInitialized(false),
        mHasOtherFamilyNames(false),
        mFaceNamesInitialized(false),
        mHasStyles(false),
        mIsSimpleFamily(false),
        mIsBadUnderlineFamily(false),
        mFamilyCharacterMapInitialized(false),
        mSkipDefaultFeatureSpaceCheck(false)
        { }

    virtual ~gfxFontFamily() { }

    const nsString& Name() { return mName; }

    virtual void LocalizedName(nsAString& aLocalizedName);
    virtual bool HasOtherFamilyNames();
    
    nsTArray<nsRefPtr<gfxFontEntry> >& GetFontList() { return mAvailableFonts; }
    
    void AddFontEntry(nsRefPtr<gfxFontEntry> aFontEntry) {
        // bug 589682 - set the IgnoreGDEF flag on entries for Italic faces
        // of Times New Roman, because of buggy table in those fonts
        if (aFontEntry->IsItalic() && !aFontEntry->IsUserFont() &&
            Name().EqualsLiteral("Times New Roman"))
        {
            aFontEntry->mIgnoreGDEF = true;
        }
        aFontEntry->mFamilyName = Name();
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

    // Only used for debugging checks - does a linear search
    bool ContainsFace(gfxFontEntry* aFontEntry) {
        uint32_t i, numFonts = mAvailableFonts.Length();
        for (i = 0; i < numFonts; i++) {
            if (mAvailableFonts[i] == aFontEntry) {
                return true;
            }
        }
        return false;
    }

    void SetSkipSpaceFeatureCheck(bool aSkipCheck) {
        mSkipDefaultFeatureSpaceCheck = aSkipCheck;
    }

protected:
    // fills in an array with weights of faces that match style,
    // returns whether any matching entries found
    virtual bool FindWeightsForStyle(gfxFontEntry* aFontsForWeights[],
                                       bool anItalic, int16_t aStretch);

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
    nsTArray<nsRefPtr<gfxFontEntry> >  mAvailableFonts;
    gfxSparseBitSet mFamilyCharacterMap;
    bool mOtherFamilyNamesInitialized : 1;
    bool mHasOtherFamilyNames : 1;
    bool mFaceNamesInitialized : 1;
    bool mHasStyles : 1;
    bool mIsSimpleFamily : 1;
    bool mIsBadUnderlineFamily : 1;
    bool mFamilyCharacterMapInitialized : 1;
    bool mSkipDefaultFeatureSpaceCheck : 1;

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

struct gfxTextRange {
    enum {
        // flags for recording the kind of font-matching that was used
        kFontGroup      = 0x0001,
        kPrefsFallback  = 0x0002,
        kSystemFallback = 0x0004
    };
    gfxTextRange(uint32_t aStart, uint32_t aEnd,
                 gfxFont* aFont, uint8_t aMatchType)
        : start(aStart),
          end(aEnd),
          font(aFont),
          matchType(aMatchType)
    { }
    uint32_t Length() const { return end - start; }
    uint32_t start, end;
    nsRefPtr<gfxFont> font;
    uint8_t matchType;
};


/**
 * Font cache design:
 * 
 * The mFonts hashtable contains most fonts, indexed by (gfxFontEntry*, style).
 * It does not add a reference to the fonts it contains.
 * When a font's refcount decreases to zero, instead of deleting it we
 * add it to our expiration tracker.
 * The expiration tracker tracks fonts with zero refcount. After a certain
 * period of time, such fonts expire and are deleted.
 *
 * We're using 3 generations with a ten-second generation interval, so
 * zero-refcount fonts will be deleted 20-30 seconds after their refcount
 * goes to zero, if timer events fire in a timely manner.
 *
 * The font cache also handles timed expiration of cached ShapedWords
 * for "persistent" fonts: it has a repeating timer, and notifies
 * each cached font to "age" its shaped words. The words will be released
 * by the fonts if they get aged three times without being re-used in the
 * meantime.
 *
 * Note that the ShapedWord timeout is much larger than the font timeout,
 * so that in the case of a short-lived font, we'll discard the gfxFont
 * completely, with all its words, and avoid the cost of aging the words
 * individually. That only happens with longer-lived fonts.
 */
struct FontCacheSizes {
    FontCacheSizes()
        : mFontInstances(0), mShapedWords(0)
    { }

    size_t mFontInstances; // memory used by instances of gfxFont subclasses
    size_t mShapedWords; // memory used by the per-font shapedWord caches
};

class gfxFontCache MOZ_FINAL : public nsExpirationTracker<gfxFont,3> {
public:
    enum {
        FONT_TIMEOUT_SECONDS = 10,
        SHAPED_WORD_TIMEOUT_SECONDS = 60
    };

    gfxFontCache();
    ~gfxFontCache();

    /*
     * Get the global gfxFontCache.  You must call Init() before
     * calling this method --- the result will not be null.
     */
    static gfxFontCache* GetCache() {
        return gGlobalCache;
    }

    static nsresult Init();
    // It's OK to call this even if Init() has not been called.
    static void Shutdown();

    // Look up a font in the cache. Returns an addrefed pointer, or null
    // if there's nothing matching in the cache
    already_AddRefed<gfxFont> Lookup(const gfxFontEntry *aFontEntry,
                                     const gfxFontStyle *aStyle);
    // We created a new font (presumably because Lookup returned null);
    // put it in the cache. The font's refcount should be nonzero. It is
    // allowable to add a new font even if there is one already in the
    // cache with the same key; we'll forget about the old one.
    void AddNew(gfxFont *aFont);

    // The font's refcount has gone to zero; give ownership of it to
    // the cache. We delete it if it's not acquired again after a certain
    // amount of time.
    void NotifyReleased(gfxFont *aFont);

    // This gets called when the timeout has expired on a zero-refcount
    // font; we just delete it.
    virtual void NotifyExpired(gfxFont *aFont);

    // Cleans out the hashtable and removes expired fonts waiting for cleanup.
    // Other gfxFont objects may be still in use but they will be pushed
    // into the expiration queues and removed.
    void Flush() {
        mFonts.Clear();
        AgeAllGenerations();
    }

    void FlushShapedWordCaches() {
        mFonts.EnumerateEntries(ClearCachedWordsForFont, nullptr);
    }

    void AddSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                FontCacheSizes* aSizes) const;
    void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                FontCacheSizes* aSizes) const;

protected:
    class MemoryReporter MOZ_FINAL : public nsIMemoryReporter
    {
    public:
        NS_DECL_ISUPPORTS
        NS_DECL_NSIMEMORYREPORTER
    };

    // Observer for notifications that the font cache cares about
    class Observer MOZ_FINAL
        : public nsIObserver
    {
    public:
        NS_DECL_ISUPPORTS
        NS_DECL_NSIOBSERVER
    };

    void DestroyFont(gfxFont *aFont);

    static gfxFontCache *gGlobalCache;

    struct Key {
        const gfxFontEntry* mFontEntry;
        const gfxFontStyle* mStyle;
        Key(const gfxFontEntry* aFontEntry, const gfxFontStyle* aStyle)
            : mFontEntry(aFontEntry), mStyle(aStyle) {}
    };

    class HashEntry : public PLDHashEntryHdr {
    public:
        typedef const Key& KeyType;
        typedef const Key* KeyTypePointer;

        // When constructing a new entry in the hashtable, we'll leave this
        // blank. The caller of Put() will fill this in.
        HashEntry(KeyTypePointer aStr) : mFont(nullptr) { }
        HashEntry(const HashEntry& toCopy) : mFont(toCopy.mFont) { }
        ~HashEntry() { }

        bool KeyEquals(const KeyTypePointer aKey) const;
        static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
        static PLDHashNumber HashKey(const KeyTypePointer aKey) {
            return mozilla::HashGeneric(aKey->mStyle->Hash(), aKey->mFontEntry);
        }
        enum { ALLOW_MEMMOVE = true };

        gfxFont* mFont;
    };

    static size_t AddSizeOfFontEntryExcludingThis(HashEntry* aHashEntry,
                                                  mozilla::MallocSizeOf aMallocSizeOf,
                                                  void* aUserArg);

    nsTHashtable<HashEntry> mFonts;

    static PLDHashOperator ClearCachedWordsForFont(HashEntry* aHashEntry, void*);
    static PLDHashOperator AgeCachedWordsForFont(HashEntry* aHashEntry, void*);
    static void WordCacheExpirationTimerCallback(nsITimer* aTimer, void* aCache);
    nsCOMPtr<nsITimer>      mWordCacheExpirationTimer;
};

class gfxTextPerfMetrics {
public:

    struct TextCounts {
        uint32_t    numContentTextRuns;
        uint32_t    numChromeTextRuns;
        uint32_t    numChars;
        uint32_t    maxTextRunLen;
        uint32_t    wordCacheSpaceRules;
        uint32_t    wordCacheLong;
        uint32_t    wordCacheHit;
        uint32_t    wordCacheMiss;
        uint32_t    fallbackPrefs;
        uint32_t    fallbackSystem;
        uint32_t    textrunConst;
        uint32_t    textrunDestr;
    };

    uint32_t reflowCount;

    // counts per reflow operation
    TextCounts current;

    // totals for the lifetime of a document
    TextCounts cumulative;

    gfxTextPerfMetrics() {
        memset(this, 0, sizeof(gfxTextPerfMetrics));
    }

    // add current totals to cumulative ones
    void Accumulate() {
        if (current.numChars == 0) {
            return;
        }
        cumulative.numContentTextRuns += current.numContentTextRuns;
        cumulative.numChromeTextRuns += current.numChromeTextRuns;
        cumulative.numChars += current.numChars;
        if (current.maxTextRunLen > cumulative.maxTextRunLen) {
            cumulative.maxTextRunLen = current.maxTextRunLen;
        }
        cumulative.wordCacheSpaceRules += current.wordCacheSpaceRules;
        cumulative.wordCacheLong += current.wordCacheLong;
        cumulative.wordCacheHit += current.wordCacheHit;
        cumulative.wordCacheMiss += current.wordCacheMiss;
        cumulative.fallbackPrefs += current.fallbackPrefs;
        cumulative.fallbackSystem += current.fallbackSystem;
        cumulative.textrunConst += current.textrunConst;
        cumulative.textrunDestr += current.textrunDestr;
        memset(&current, 0, sizeof(current));
    }
};

class gfxTextRunFactory {
    NS_INLINE_DECL_REFCOUNTING(gfxTextRunFactory)

public:
    // Flags in the mask 0xFFFF0000 are reserved for textrun clients
    // Flags in the mask 0x0000F000 are reserved for per-platform fonts
    // Flags in the mask 0x00000FFF are set by the textrun creator.
    enum {
        CACHE_TEXT_FLAGS    = 0xF0000000,
        USER_TEXT_FLAGS     = 0x0FFF0000,
        PLATFORM_TEXT_FLAGS = 0x0000F000,
        TEXTRUN_TEXT_FLAGS  = 0x00000FFF,
        SETTABLE_FLAGS      = CACHE_TEXT_FLAGS | USER_TEXT_FLAGS,

        /**
         * When set, the text string pointer used to create the text run
         * is guaranteed to be available during the lifetime of the text run.
         */
        TEXT_IS_PERSISTENT           = 0x0001,
        /**
         * When set, the text is known to be all-ASCII (< 128).
         */
        TEXT_IS_ASCII                = 0x0002,
        /**
         * When set, the text is RTL.
         */
        TEXT_IS_RTL                  = 0x0004,
        /**
         * When set, spacing is enabled and the textrun needs to call GetSpacing
         * on the spacing provider.
         */
        TEXT_ENABLE_SPACING          = 0x0008,
        /**
         * When set, GetHyphenationBreaks may return true for some character
         * positions, otherwise it will always return false for all characters.
         */
        TEXT_ENABLE_HYPHEN_BREAKS    = 0x0010,
        /**
         * When set, the text has no characters above 255 and it is stored
         * in the textrun in 8-bit format.
         */
        TEXT_IS_8BIT                 = 0x0020,
        /**
         * When set, the RunMetrics::mBoundingBox field will be initialized
         * properly based on glyph extents, in particular, glyph extents that
         * overflow the standard font-box (the box defined by the ascent, descent
         * and advance width of the glyph). When not set, it may just be the
         * standard font-box even if glyphs overflow.
         */
        TEXT_NEED_BOUNDING_BOX       = 0x0040,
        /**
         * When set, optional ligatures are disabled. Ligatures that are
         * required for legible text should still be enabled.
         */
        TEXT_DISABLE_OPTIONAL_LIGATURES = 0x0080,
        /**
         * When set, the textrun should favour speed of construction over
         * quality. This may involve disabling ligatures and/or kerning or
         * other effects.
         */
        TEXT_OPTIMIZE_SPEED          = 0x0100,
        /**
         * For internal use by the memory reporter when accounting for
         * storage used by textruns.
         * Because the reporter may visit each textrun multiple times while
         * walking the frame trees and textrun cache, it needs to mark
         * textruns that have been seen so as to avoid multiple-accounting.
         */
        TEXT_RUN_SIZE_ACCOUNTED      = 0x0200,
        /**
         * When set, the textrun should discard control characters instead of
         * turning them into hexboxes.
         */
        TEXT_HIDE_CONTROL_CHARACTERS = 0x0400,

        /**
         * nsTextFrameThebes sets these, but they're defined here rather than
         * in nsTextFrameUtils.h because ShapedWord creation/caching also needs
         * to check the _INCOMING flag
         */
        TEXT_TRAILING_ARABICCHAR = 0x20000000,
        /**
         * When set, the previous character for this textrun was an Arabic
         * character.  This is used for the context detection necessary for
         * bidi.numeral implementation.
         */
        TEXT_INCOMING_ARABICCHAR = 0x40000000,

        // Set if the textrun should use the OpenType 'math' script.
        TEXT_USE_MATH_SCRIPT = 0x80000000,

        TEXT_UNUSED_FLAGS = 0x10000000
    };

    /**
     * This record contains all the parameters needed to initialize a textrun.
     */
    struct Parameters {
        // A reference context suggesting where the textrun will be rendered
        gfxContext   *mContext;
        // Pointer to arbitrary user data (which should outlive the textrun)
        void         *mUserData;
        // A description of which characters have been stripped from the original
        // DOM string to produce the characters in the textrun. May be null
        // if that information is not relevant.
        gfxSkipChars *mSkipChars;
        // A list of where linebreaks are currently placed in the textrun. May
        // be null if mInitialBreakCount is zero.
        uint32_t     *mInitialBreaks;
        uint32_t      mInitialBreakCount;
        // The ratio to use to convert device pixels to application layout units
        int32_t       mAppUnitsPerDevUnit;
    };

    virtual ~gfxTextRunFactory() {}
};

/**
 * This stores glyph bounds information for a particular gfxFont, at
 * a particular appunits-per-dev-pixel ratio (because the compressed glyph
 * width array is stored in appunits).
 * 
 * We store a hashtable from glyph IDs to float bounding rects. For the
 * common case where the glyph has no horizontal left bearing, and no
 * y overflow above the font ascent or below the font descent, and tight
 * bounding boxes are not required, we avoid storing the glyph ID in the hashtable
 * and instead consult an array of 16-bit glyph XMost values (in appunits).
 * This array always has an entry for the font's space glyph --- the width is
 * assumed to be zero.
 */
class gfxGlyphExtents {
public:
    gfxGlyphExtents(int32_t aAppUnitsPerDevUnit) :
        mAppUnitsPerDevUnit(aAppUnitsPerDevUnit) {
        MOZ_COUNT_CTOR(gfxGlyphExtents);
    }
    ~gfxGlyphExtents();

    enum { INVALID_WIDTH = 0xFFFF };

    void NotifyGlyphsChanged() {
        mTightGlyphExtents.Clear();
    }

    // returns INVALID_WIDTH => not a contained glyph
    // Otherwise the glyph has no before-bearing or vertical bearings,
    // and the result is its width measured from the baseline origin, in
    // appunits.
    uint16_t GetContainedGlyphWidthAppUnits(uint32_t aGlyphID) const {
        return mContainedGlyphWidths.Get(aGlyphID);
    }

    bool IsGlyphKnown(uint32_t aGlyphID) const {
        return mContainedGlyphWidths.Get(aGlyphID) != INVALID_WIDTH ||
            mTightGlyphExtents.GetEntry(aGlyphID) != nullptr;
    }

    bool IsGlyphKnownWithTightExtents(uint32_t aGlyphID) const {
        return mTightGlyphExtents.GetEntry(aGlyphID) != nullptr;
    }

    // Get glyph extents; a rectangle relative to the left baseline origin
    // Returns true on success. Can fail on OOM or when aContext is null
    // and extents were not (successfully) prefetched.
    bool GetTightGlyphExtentsAppUnits(gfxFont *aFont, gfxContext *aContext,
            uint32_t aGlyphID, gfxRect *aExtents);

    void SetContainedGlyphWidthAppUnits(uint32_t aGlyphID, uint16_t aWidth) {
        mContainedGlyphWidths.Set(aGlyphID, aWidth);
    }
    void SetTightGlyphExtents(uint32_t aGlyphID, const gfxRect& aExtentsAppUnits);

    int32_t GetAppUnitsPerDevUnit() { return mAppUnitsPerDevUnit; }

    size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;
    size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
    class HashEntry : public nsUint32HashKey {
    public:
        // When constructing a new entry in the hashtable, we'll leave this
        // blank. The caller of Put() will fill this in.
        HashEntry(KeyTypePointer aPtr) : nsUint32HashKey(aPtr) {}
        HashEntry(const HashEntry& toCopy) : nsUint32HashKey(toCopy) {
          x = toCopy.x; y = toCopy.y; width = toCopy.width; height = toCopy.height;
        }

        float x, y, width, height;
    };

    enum { BLOCK_SIZE_BITS = 7, BLOCK_SIZE = 1 << BLOCK_SIZE_BITS }; // 128-glyph blocks

    class GlyphWidths {
    public:
        void Set(uint32_t aIndex, uint16_t aValue);
        uint16_t Get(uint32_t aIndex) const {
            uint32_t block = aIndex >> BLOCK_SIZE_BITS;
            if (block >= mBlocks.Length())
                return INVALID_WIDTH;
            uintptr_t bits = mBlocks[block];
            if (!bits)
                return INVALID_WIDTH;
            uint32_t indexInBlock = aIndex & (BLOCK_SIZE - 1);
            if (bits & 0x1) {
                if (GetGlyphOffset(bits) != indexInBlock)
                    return INVALID_WIDTH;
                return GetWidth(bits);
            }
            uint16_t *widths = reinterpret_cast<uint16_t *>(bits);
            return widths[indexInBlock];
        }

        uint32_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;
        
        ~GlyphWidths();

    private:
        static uint32_t GetGlyphOffset(uintptr_t aBits) {
            NS_ASSERTION(aBits & 0x1, "This is really a pointer...");
            return (aBits >> 1) & ((1 << BLOCK_SIZE_BITS) - 1);
        }
        static uint32_t GetWidth(uintptr_t aBits) {
            NS_ASSERTION(aBits & 0x1, "This is really a pointer...");
            return aBits >> (1 + BLOCK_SIZE_BITS);
        }
        static uintptr_t MakeSingle(uint32_t aGlyphOffset, uint16_t aWidth) {
            return (aWidth << (1 + BLOCK_SIZE_BITS)) + (aGlyphOffset << 1) + 1;
        }

        nsTArray<uintptr_t> mBlocks;
    };

    GlyphWidths             mContainedGlyphWidths;
    nsTHashtable<HashEntry> mTightGlyphExtents;
    int32_t                 mAppUnitsPerDevUnit;

private:
    // not implemented:
    gfxGlyphExtents(const gfxGlyphExtents& aOther) MOZ_DELETE;
    gfxGlyphExtents& operator=(const gfxGlyphExtents& aOther) MOZ_DELETE;
};

/**
 * gfxFontShaper
 *
 * This class implements text shaping (character to glyph mapping and
 * glyph layout). There is a gfxFontShaper subclass for each text layout
 * technology (uniscribe, core text, harfbuzz,....) we support.
 *
 * The shaper is responsible for setting up glyph data in gfxTextRuns.
 *
 * A generic, platform-independent shaper relies only on the standard
 * gfxFont interface and can work with any concrete subclass of gfxFont.
 *
 * Platform-specific implementations designed to interface to platform
 * shaping APIs such as Uniscribe or CoreText may rely on features of a
 * specific font subclass to access native font references
 * (such as CTFont, HFONT, DWriteFont, etc).
 */

class gfxFontShaper {
public:
    gfxFontShaper(gfxFont *aFont)
        : mFont(aFont)
    {
        NS_ASSERTION(aFont, "shaper requires a valid font!");
    }

    virtual ~gfxFontShaper() { }

    // Shape a piece of text and store the resulting glyph data into
    // aShapedText. Parameters aOffset/aLength indicate the range of
    // aShapedText to be updated; aLength is also the length of aText.
    virtual bool ShapeText(gfxContext      *aContext,
                           const char16_t *aText,
                           uint32_t         aOffset,
                           uint32_t         aLength,
                           int32_t          aScript,
                           gfxShapedText   *aShapedText) = 0;

    gfxFont *GetFont() const { return mFont; }

    // returns true if features exist in output, false otherwise
    static bool
    MergeFontFeatures(const gfxFontStyle *aStyle,
                      const nsTArray<gfxFontFeature>& aFontFeatures,
                      bool aDisableLigatures,
                      const nsAString& aFamilyName,
                      nsDataHashtable<nsUint32HashKey,uint32_t>& aMergedFeatures);

protected:
    // the font this shaper is working with
    gfxFont * mFont;
};

/* a SPECIFIC single font family */
class gfxFont {
public:
    nsrefcnt AddRef(void) {
        NS_PRECONDITION(int32_t(mRefCnt) >= 0, "illegal refcnt");
        if (mExpirationState.IsTracked()) {
            gfxFontCache::GetCache()->RemoveObject(this);
        }
        ++mRefCnt;
        NS_LOG_ADDREF(this, mRefCnt, "gfxFont", sizeof(*this));
        return mRefCnt;
    }
    nsrefcnt Release(void) {
        NS_PRECONDITION(0 != mRefCnt, "dup release");
        --mRefCnt;
        NS_LOG_RELEASE(this, mRefCnt, "gfxFont");
        if (mRefCnt == 0) {
            NotifyReleased();
            // |this| may have been deleted.
            return 0;
        }
        return mRefCnt;
    }

    int32_t GetRefCount() { return mRefCnt; }

    // options to specify the kind of AA to be used when creating a font
    typedef enum {
        kAntialiasDefault,
        kAntialiasNone,
        kAntialiasGrayscale,
        kAntialiasSubpixel
    } AntialiasOption;

protected:
    nsAutoRefCnt mRefCnt;
    cairo_scaled_font_t *mScaledFont;

    void NotifyReleased() {
        gfxFontCache *cache = gfxFontCache::GetCache();
        if (cache) {
            // Don't delete just yet; return the object to the cache for
            // possibly recycling within some time limit
            cache->NotifyReleased(this);
        } else {
            // The cache may have already been shut down.
            delete this;
        }
    }

    gfxFont(gfxFontEntry *aFontEntry, const gfxFontStyle *aFontStyle,
            AntialiasOption anAAOption = kAntialiasDefault,
            cairo_scaled_font_t *aScaledFont = nullptr);

public:
    virtual ~gfxFont();

    bool Valid() const {
        return mIsValid;
    }

    // options for the kind of bounding box to return from measurement
    typedef enum {
        LOOSE_INK_EXTENTS,
            // A box that encloses all the painted pixels, and may
            // include sidebearings and/or additional ascent/descent
            // within the glyph cell even if the ink is smaller.
        TIGHT_INK_EXTENTS,
            // A box that tightly encloses all the painted pixels
            // (although actually on Windows, at least, it may be
            // slightly larger than strictly necessary because
            // we can't get precise extents with ClearType).
        TIGHT_HINTED_OUTLINE_EXTENTS
            // A box that tightly encloses the glyph outline,
            // ignoring possible antialiasing pixels that extend
            // beyond this.
            // NOTE: The default implementation of gfxFont::Measure(),
            // which works with the glyph extents cache, does not
            // differentiate between this and TIGHT_INK_EXTENTS.
            // Whether the distinction is important depends on the
            // antialiasing behavior of the platform; currently the
            // distinction is only implemented in the gfxWindowsFont
            // subclass, because of ClearType's tendency to paint
            // outside the hinted outline.
            // Also NOTE: it is relatively expensive to request this,
            // as it does not use cached glyph extents in the font.
    } BoundingBoxType;

    const nsString& GetName() const { return mFontEntry->Name(); }
    const gfxFontStyle *GetStyle() const { return &mStyle; }

    virtual cairo_scaled_font_t* GetCairoScaledFont() { return mScaledFont; }

    virtual gfxFont* CopyWithAntialiasOption(AntialiasOption anAAOption) {
        // platforms where this actually matters should override
        return nullptr;
    }

    virtual gfxFloat GetAdjustedSize() {
        return mAdjustedSize > 0.0 ? mAdjustedSize : mStyle.size;
    }

    float FUnitsToDevUnitsFactor() const {
        // check this was set up during font initialization
        NS_ASSERTION(mFUnitsConvFactor > 0.0f, "mFUnitsConvFactor not valid");
        return mFUnitsConvFactor;
    }

    // check whether this is an sfnt we can potentially use with harfbuzz
    bool FontCanSupportHarfBuzz() {
        return mFontEntry->HasCmapTable();
    }

    // check whether this is an sfnt we can potentially use with Graphite
    bool FontCanSupportGraphite() {
        return mFontEntry->HasGraphiteTables();
    }

    // Subclasses may choose to look up glyph ids for characters.
    // If they do not override this, gfxHarfBuzzShaper will fetch the cmap
    // table and use that.
    virtual bool ProvidesGetGlyph() const {
        return false;
    }
    // Map unicode character to glyph ID.
    // Only used if ProvidesGetGlyph() returns true.
    virtual uint32_t GetGlyph(uint32_t unicode, uint32_t variation_selector) {
        return 0;
    }

    // subclasses may provide (possibly hinted) glyph widths (in font units);
    // if they do not override this, harfbuzz will use unhinted widths
    // derived from the font tables
    virtual bool ProvidesGlyphWidths() {
        return false;
    }

    // The return value is interpreted as a horizontal advance in 16.16 fixed
    // point format.
    virtual int32_t GetGlyphWidth(gfxContext *aCtx, uint16_t aGID) {
        return -1;
    }

    // Return Azure GlyphRenderingOptions for drawing this font.
    virtual mozilla::TemporaryRef<mozilla::gfx::GlyphRenderingOptions>
      GetGlyphRenderingOptions() { return nullptr; }

    gfxFloat SynthesizeSpaceWidth(uint32_t aCh);

    // Font metrics
    struct Metrics {
        gfxFloat xHeight;
        gfxFloat superscriptOffset;
        gfxFloat subscriptOffset;
        gfxFloat strikeoutSize;
        gfxFloat strikeoutOffset;
        gfxFloat underlineSize;
        gfxFloat underlineOffset;

        gfxFloat internalLeading;
        gfxFloat externalLeading;

        gfxFloat emHeight;
        gfxFloat emAscent;
        gfxFloat emDescent;
        gfxFloat maxHeight;
        gfxFloat maxAscent;
        gfxFloat maxDescent;
        gfxFloat maxAdvance;

        gfxFloat aveCharWidth;
        gfxFloat spaceWidth;
        gfxFloat zeroOrAveCharWidth;  // width of '0', or if there is
                                      // no '0' glyph in this font,
                                      // equal to .aveCharWidth
    };
    virtual const gfxFont::Metrics& GetMetrics() = 0;

    /**
     * We let layout specify spacing on either side of any
     * character. We need to specify both before and after
     * spacing so that substring measurement can do the right things.
     * These values are in appunits. They're always an integral number of
     * appunits, but we specify them in floats in case very large spacing
     * values are required.
     */
    struct Spacing {
        gfxFloat mBefore;
        gfxFloat mAfter;
    };
    /**
     * Metrics for a particular string
     */
    struct RunMetrics {
        RunMetrics() {
            mAdvanceWidth = mAscent = mDescent = 0.0;
        }

        void CombineWith(const RunMetrics& aOther, bool aOtherIsOnLeft);

        // can be negative (partly due to negative spacing).
        // Advance widths should be additive: the advance width of the
        // (offset1, length1) plus the advance width of (offset1 + length1,
        // length2) should be the advance width of (offset1, length1 + length2)
        gfxFloat mAdvanceWidth;
        
        // For zero-width substrings, these must be zero!
        gfxFloat mAscent;  // always non-negative
        gfxFloat mDescent; // always non-negative
        
        // Bounding box that is guaranteed to include everything drawn.
        // If a tight boundingBox was requested when these metrics were
        // generated, this will tightly wrap the glyphs, otherwise it is
        // "loose" and may be larger than the true bounding box.
        // Coordinates are relative to the baseline left origin, so typically
        // mBoundingBox.y == -mAscent
        gfxRect  mBoundingBox;
    };

    /**
     * Draw a series of glyphs to aContext. The direction of aTextRun must
     * be honoured.
     * @param aStart the first character to draw
     * @param aEnd draw characters up to here
     * @param aBaselineOrigin the baseline origin; the left end of the baseline
     * for LTR textruns, the right end of the baseline for RTL textruns. On return,
     * this should be updated to the other end of the baseline. In application
     * units, really!
     * @param aSpacing spacing to insert before and after characters (for RTL
     * glyphs, before-spacing is inserted to the right of characters). There
     * are aEnd - aStart elements in this array, unless it's null to indicate
     * that there is no spacing.
     * @param aDrawMode specifies whether the fill or stroke of the glyph should be
     * drawn, or if it should be drawn into the current path
     * @param aContextPaint information about how to construct the fill and
     * stroke pattern. Can be nullptr if we are not stroking the text, which
     * indicates that the current source from aContext should be used for filling
     * 
     * Callers guarantee:
     * -- aStart and aEnd are aligned to cluster and ligature boundaries
     * -- all glyphs use this font
     * 
     * The default implementation builds a cairo glyph array and
     * calls cairo_show_glyphs or cairo_glyph_path.
     */
    virtual void Draw(gfxTextRun *aTextRun, uint32_t aStart, uint32_t aEnd,
                      gfxContext *aContext, DrawMode aDrawMode, gfxPoint *aBaselineOrigin,
                      Spacing *aSpacing, gfxTextContextPaint *aContextPaint,
                      gfxTextRunDrawCallbacks *aCallbacks);

    /**
     * Measure a run of characters. See gfxTextRun::Metrics.
     * @param aTight if false, then return the union of the glyph extents
     * with the font-box for the characters (the rectangle with x=0,width=
     * the advance width for the character run,y=-(font ascent), and height=
     * font ascent + font descent). Otherwise, we must return as tight as possible
     * an approximation to the area actually painted by glyphs.
     * @param aContextForTightBoundingBox when aTight is true, this must
     * be non-null.
     * @param aSpacing spacing to insert before and after glyphs. The bounding box
     * need not include the spacing itself, but the spacing affects the glyph
     * positions. null if there is no spacing.
     * 
     * Callers guarantee:
     * -- aStart and aEnd are aligned to cluster and ligature boundaries
     * -- all glyphs use this font
     * 
     * The default implementation just uses font metrics and aTextRun's
     * advances, and assumes no characters fall outside the font box. In
     * general this is insufficient, because that assumption is not always true.
     */
    virtual RunMetrics Measure(gfxTextRun *aTextRun,
                               uint32_t aStart, uint32_t aEnd,
                               BoundingBoxType aBoundingBoxType,
                               gfxContext *aContextForTightBoundingBox,
                               Spacing *aSpacing);
    /**
     * Line breaks have been changed at the beginning and/or end of a substring
     * of the text. Reshaping may be required; glyph updating is permitted.
     * @return true if anything was changed, false otherwise
     */
    bool NotifyLineBreaksChanged(gfxTextRun *aTextRun,
                                   uint32_t aStart, uint32_t aLength)
    { return false; }

    // Expiration tracking
    nsExpirationState *GetExpirationState() { return &mExpirationState; }

    // Get the glyphID of a space
    virtual uint32_t GetSpaceGlyph() = 0;

    gfxGlyphExtents *GetOrCreateGlyphExtents(int32_t aAppUnitsPerDevUnit);

    // You need to call SetupCairoFont on the aCR just before calling this
    virtual void SetupGlyphExtents(gfxContext *aContext, uint32_t aGlyphID,
                                   bool aNeedTight, gfxGlyphExtents *aExtents);

    // This is called by the default Draw() implementation above.
    virtual bool SetupCairoFont(gfxContext *aContext) = 0;

    virtual bool AllowSubpixelAA() { return true; }

    bool IsSyntheticBold() { return mApplySyntheticBold; }

    // Amount by which synthetic bold "fattens" the glyphs:
    // For size S up to a threshold size T, we use (0.25 + 3S / 4T),
    // so that the result ranges from 0.25 to 1.0; thereafter,
    // simply use (S / T).
    gfxFloat GetSyntheticBoldOffset() {
        gfxFloat size = GetAdjustedSize();
        const gfxFloat threshold = 48.0;
        return size < threshold ? (0.25 + 0.75 * size / threshold) :
                                  (size / threshold);
    }

    gfxFontEntry *GetFontEntry() { return mFontEntry.get(); }
    bool HasCharacter(uint32_t ch) {
        if (!mIsValid)
            return false;
        return mFontEntry->HasCharacter(ch); 
    }

    uint16_t GetUVSGlyph(uint32_t aCh, uint32_t aVS) {
        if (!mIsValid) {
            return 0;
        }
        return mFontEntry->GetUVSGlyph(aCh, aVS); 
    }

    // call the (virtual) InitTextRun method to do glyph generation/shaping,
    // limiting the length of text passed by processing the run in multiple
    // segments if necessary
    template<typename T>
    bool SplitAndInitTextRun(gfxContext *aContext,
                             gfxTextRun *aTextRun,
                             const T *aString,
                             uint32_t aRunStart,
                             uint32_t aRunLength,
                             int32_t aRunScript);

    // Get a ShapedWord representing the given text (either 8- or 16-bit)
    // for use in setting up a gfxTextRun.
    template<typename T>
    gfxShapedWord* GetShapedWord(gfxContext *aContext,
                                 const T *aText,
                                 uint32_t aLength,
                                 uint32_t aHash,
                                 int32_t aRunScript,
                                 int32_t aAppUnitsPerDevUnit,
                                 uint32_t aFlags,
                                 gfxTextPerfMetrics *aTextPerf);

    // Ensure the ShapedWord cache is initialized. This MUST be called before
    // any attempt to use GetShapedWord().
    void InitWordCache() {
        if (!mWordCache) {
            mWordCache = new nsTHashtable<CacheHashEntry>;
        }
    }

    // Called by the gfxFontCache timer to increment the age of all the words,
    // so that they'll expire after a sufficient period of non-use
    void AgeCachedWords() {
        if (mWordCache) {
            (void)mWordCache->EnumerateEntries(AgeCacheEntry, this);
        }
    }

    // Discard all cached word records; called on memory-pressure notification.
    void ClearCachedWords() {
        if (mWordCache) {
            mWordCache->Clear();
        }
    }

    // Glyph rendering/geometry has changed, so invalidate data as necessary.
    void NotifyGlyphsChanged();

    virtual void AddSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                        FontCacheSizes* aSizes) const;
    virtual void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                        FontCacheSizes* aSizes) const;

    typedef enum {
        FONT_TYPE_DWRITE,
        FONT_TYPE_GDI,
        FONT_TYPE_FT2,
        FONT_TYPE_MAC,
        FONT_TYPE_OS2,
        FONT_TYPE_CAIRO
    } FontType;

    virtual FontType GetType() const = 0;

    virtual mozilla::TemporaryRef<mozilla::gfx::ScaledFont> GetScaledFont(mozilla::gfx::DrawTarget *aTarget)
    { return gfxPlatform::GetPlatform()->GetScaledFontForFont(aTarget, this); }

    bool KerningDisabled() {
        return mKerningSet && !mKerningEnabled;
    }

    /**
     * Subclass this object to be notified of glyph changes. Delete the object
     * when no longer needed.
     */
    class GlyphChangeObserver {
    public:
        virtual ~GlyphChangeObserver()
        {
            if (mFont) {
                mFont->RemoveGlyphChangeObserver(this);
            }
        }
        // This gets called when the gfxFont dies.
        void ForgetFont() { mFont = nullptr; }
        virtual void NotifyGlyphsChanged() = 0;
    protected:
        GlyphChangeObserver(gfxFont *aFont) : mFont(aFont)
        {
            mFont->AddGlyphChangeObserver(this);
        }
        gfxFont* mFont;
    };
    friend class GlyphChangeObserver;

    bool GlyphsMayChange()
    {
        // Currently only fonts with SVG glyphs can have animated glyphs
        return mFontEntry->TryGetSVGData(this);
    }

    static void DestroySingletons() {
        delete sScriptTagToCode;
        delete sDefaultFeatures;
    }

protected:
    void AddGlyphChangeObserver(GlyphChangeObserver *aObserver);
    void RemoveGlyphChangeObserver(GlyphChangeObserver *aObserver);

    // whether font contains substitution lookups containing spaces
    bool HasSubstitutionRulesWithSpaceLookups(int32_t aRunScript);

    // do spaces participate in shaping rules? if so, can't used word cache
    bool SpaceMayParticipateInShaping(int32_t aRunScript);

    // For 8-bit text, expand to 16-bit and then call the following method.
    bool ShapeText(gfxContext    *aContext,
                   const uint8_t *aText,
                   uint32_t       aOffset, // dest offset in gfxShapedText
                   uint32_t       aLength,
                   int32_t        aScript,
                   gfxShapedText *aShapedText, // where to store the result
                   bool           aPreferPlatformShaping = false);

    // Call the appropriate shaper to generate glyphs for aText and store
    // them into aShapedText.
    virtual bool ShapeText(gfxContext      *aContext,
                           const char16_t *aText,
                           uint32_t         aOffset,
                           uint32_t         aLength,
                           int32_t          aScript,
                           gfxShapedText   *aShapedText,
                           bool             aPreferPlatformShaping = false);

    // Helper to adjust for synthetic bold and set character-type flags
    // in the shaped text; implementations of ShapeText should call this
    // after glyph shaping has been completed.
    void PostShapingFixup(gfxContext      *aContext,
                          const char16_t *aText,
                          uint32_t         aOffset, // position within aShapedText
                          uint32_t         aLength,
                          gfxShapedText   *aShapedText);

    // Shape text directly into a range within a textrun, without using the
    // font's word cache. Intended for use when the font has layout features
    // that involve space, and therefore require shaping complete runs rather
    // than isolated words, or for long strings that are inefficient to cache.
    // This will split the text on "invalid" characters (tab/newline) that are
    // not handled via normal shaping, but does not otherwise divide up the
    // text.
    template<typename T>
    bool ShapeTextWithoutWordCache(gfxContext *aContext,
                                   const T    *aText,
                                   uint32_t    aOffset,
                                   uint32_t    aLength,
                                   int32_t     aScript,
                                   gfxTextRun *aTextRun);

    // Shape a fragment of text (a run that is known to contain only
    // "valid" characters, no newlines/tabs/other control chars).
    // All non-wordcache shaping goes through here; this is the function
    // that will ensure we don't pass excessively long runs to the various
    // platform shapers.
    template<typename T>
    bool ShapeFragmentWithoutWordCache(gfxContext *aContext,
                                       const T    *aText,
                                       uint32_t    aOffset,
                                       uint32_t    aLength,
                                       int32_t     aScript,
                                       gfxTextRun *aTextRun);

    void CheckForFeaturesInvolvingSpace();

    // whether a given feature is included in feature settings from both the
    // font and the style. aFeatureOn set if resolved feature value is non-zero
    bool HasFeatureSet(uint32_t aFeature, bool& aFeatureOn);

    // used when analyzing whether a font has space contextual lookups
    static nsDataHashtable<nsUint32HashKey, int32_t> *sScriptTagToCode;
    static nsTHashtable<nsUint32HashKey>             *sDefaultFeatures;

    nsRefPtr<gfxFontEntry> mFontEntry;

    struct CacheHashKey {
        union {
            const uint8_t   *mSingle;
            const char16_t *mDouble;
        }                mText;
        uint32_t         mLength;
        uint32_t         mFlags;
        int32_t          mScript;
        int32_t          mAppUnitsPerDevUnit;
        PLDHashNumber    mHashKey;
        bool             mTextIs8Bit;

        CacheHashKey(const uint8_t *aText, uint32_t aLength,
                     uint32_t aStringHash,
                     int32_t aScriptCode, int32_t aAppUnitsPerDevUnit,
                     uint32_t aFlags)
            : mLength(aLength),
              mFlags(aFlags),
              mScript(aScriptCode),
              mAppUnitsPerDevUnit(aAppUnitsPerDevUnit),
              mHashKey(aStringHash + aScriptCode +
                  aAppUnitsPerDevUnit * 0x100 + aFlags * 0x10000),
              mTextIs8Bit(true)
        {
            NS_ASSERTION(aFlags & gfxTextRunFactory::TEXT_IS_8BIT,
                         "8-bit flag should have been set");
            mText.mSingle = aText;
        }

        CacheHashKey(const char16_t *aText, uint32_t aLength,
                     uint32_t aStringHash,
                     int32_t aScriptCode, int32_t aAppUnitsPerDevUnit,
                     uint32_t aFlags)
            : mLength(aLength),
              mFlags(aFlags),
              mScript(aScriptCode),
              mAppUnitsPerDevUnit(aAppUnitsPerDevUnit),
              mHashKey(aStringHash + aScriptCode +
                  aAppUnitsPerDevUnit * 0x100 + aFlags * 0x10000),
              mTextIs8Bit(false)
        {
            // We can NOT assert that TEXT_IS_8BIT is false in aFlags here,
            // because this might be an 8bit-only word from a 16-bit textrun,
            // in which case the text we're passed is still in 16-bit form,
            // and we'll have to use an 8-to-16bit comparison in KeyEquals.
            mText.mDouble = aText;
        }
    };

    class CacheHashEntry : public PLDHashEntryHdr {
    public:
        typedef const CacheHashKey &KeyType;
        typedef const CacheHashKey *KeyTypePointer;

        // When constructing a new entry in the hashtable, the caller of Put()
        // will fill us in.
        CacheHashEntry(KeyTypePointer aKey) { }
        CacheHashEntry(const CacheHashEntry& toCopy) { NS_ERROR("Should not be called"); }
        ~CacheHashEntry() { }

        bool KeyEquals(const KeyTypePointer aKey) const;

        static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }

        static PLDHashNumber HashKey(const KeyTypePointer aKey) {
            return aKey->mHashKey;
        }

        enum { ALLOW_MEMMOVE = true };

        nsAutoPtr<gfxShapedWord> mShapedWord;
    };

    static size_t
    WordCacheEntrySizeOfExcludingThis(CacheHashEntry*   aHashEntry,
                                      mozilla::MallocSizeOf aMallocSizeOf,
                                      void*             aUserArg);

    nsAutoPtr<nsTHashtable<CacheHashEntry> > mWordCache;

    static PLDHashOperator AgeCacheEntry(CacheHashEntry *aEntry, void *aUserData);
    static const uint32_t  kShapedWordCacheMaxAge = 3;

    bool                       mIsValid;

    // use synthetic bolding for environments where this is not supported
    // by the platform
    bool                       mApplySyntheticBold;

    bool                       mKerningSet;     // kerning explicitly set?
    bool                       mKerningEnabled; // if set, on or off?

    nsExpirationState          mExpirationState;
    gfxFontStyle               mStyle;
    nsAutoTArray<gfxGlyphExtents*,1> mGlyphExtentsArray;
    nsAutoPtr<nsTHashtable<nsPtrHashKey<GlyphChangeObserver> > > mGlyphChangeObservers;

    gfxFloat                   mAdjustedSize;

    float                      mFUnitsConvFactor; // conversion factor from font units to dev units

    // the AA setting requested for this font - may affect glyph bounds
    AntialiasOption            mAntialiasOption;

    // a copy of the font without antialiasing, if needed for separate
    // measurement by mathml code
    nsAutoPtr<gfxFont>         mNonAAFont;

    // we may switch between these shapers on the fly, based on the script
    // of the text run being shaped
    nsAutoPtr<gfxFontShaper>   mPlatformShaper;
    nsAutoPtr<gfxFontShaper>   mHarfBuzzShaper;
    nsAutoPtr<gfxFontShaper>   mGraphiteShaper;

    mozilla::RefPtr<mozilla::gfx::ScaledFont> mAzureScaledFont;

    // Create a default platform text shaper for this font.
    // (TODO: This should become pure virtual once all font backends have
    // been updated.)
    virtual void CreatePlatformShaper() { }

    // Helper for subclasses that want to initialize standard metrics from the
    // tables of sfnt (TrueType/OpenType) fonts.
    // This will use mFUnitsConvFactor if it is already set, else compute it
    // from mAdjustedSize and the unitsPerEm in the font's 'head' table.
    // Returns TRUE and sets mIsValid=TRUE if successful;
    // Returns TRUE but leaves mIsValid=FALSE if the font seems to be broken.
    // Returns FALSE if the font does not appear to be an sfnt at all,
    // and should be handled (if possible) using other APIs.
    bool InitMetricsFromSfntTables(Metrics& aMetrics);

    // Helper to calculate various derived metrics from the results of
    // InitMetricsFromSfntTables or equivalent platform code
    void CalculateDerivedMetrics(Metrics& aMetrics);

    // some fonts have bad metrics, this method sanitize them.
    // if this font has bad underline offset, aIsBadUnderlineFont should be true.
    void SanitizeMetrics(gfxFont::Metrics *aMetrics, bool aIsBadUnderlineFont);

    bool RenderSVGGlyph(gfxContext *aContext, gfxPoint aPoint, DrawMode aDrawMode,
                        uint32_t aGlyphId, gfxTextContextPaint *aContextPaint);
    bool RenderSVGGlyph(gfxContext *aContext, gfxPoint aPoint, DrawMode aDrawMode,
                        uint32_t aGlyphId, gfxTextContextPaint *aContextPaint,
                        gfxTextRunDrawCallbacks *aCallbacks,
                        bool& aEmittedGlyphs);

    // Bug 674909. When synthetic bolding text by drawing twice, need to
    // render using a pixel offset in device pixels, otherwise text
    // doesn't appear bolded, it appears as if a bad text shadow exists
    // when a non-identity transform exists.  Use an offset factor so that
    // the second draw occurs at a constant offset in device pixels.
    // This helper calculates the scale factor we need to apply to the
    // synthetic-bold offset.
    static double CalcXScale(gfxContext *aContext);
};

// proportion of ascent used for x-height, if unable to read value from font
#define DEFAULT_XHEIGHT_FACTOR 0.56f

/*
 * gfxShapedText is an abstract superclass for gfxShapedWord and gfxTextRun.
 * These are objects that store a list of zero or more glyphs for each character.
 * For each glyph we store the glyph ID, the advance, and possibly x/y-offsets.
 * The idea is that a string is rendered by a loop that draws each glyph
 * at its designated offset from the current point, then advances the current
 * point by the glyph's advance in the direction of the textrun (LTR or RTL).
 * Each glyph advance is always rounded to the nearest appunit; this ensures
 * consistent results when dividing the text in a textrun into multiple text
 * frames (frame boundaries are always aligned to appunits). We optimize
 * for the case where a character has a single glyph and zero xoffset and yoffset,
 * and the glyph ID and advance are in a reasonable range so we can pack all
 * necessary data into 32 bits.
 *
 * gfxFontShaper can shape text into either a gfxShapedWord (cached by a gfxFont)
 * or directly into a gfxTextRun (for cases where we want to shape textruns in
 * their entirety rather than using cached words, because there may be layout
 * features that depend on the inter-word spaces).
 */
class gfxShapedText
{
public:
    gfxShapedText(uint32_t aLength, uint32_t aFlags,
                  int32_t aAppUnitsPerDevUnit)
        : mLength(aLength)
        , mFlags(aFlags)
        , mAppUnitsPerDevUnit(aAppUnitsPerDevUnit)
    { }

    virtual ~gfxShapedText() { }

    /**
     * This class records the information associated with a character in the
     * input string. It's optimized for the case where there is one glyph
     * representing that character alone.
     * 
     * A character can have zero or more associated glyphs. Each glyph
     * has an advance width and an x and y offset.
     * A character may be the start of a cluster.
     * A character may be the start of a ligature group.
     * A character can be "missing", indicating that the system is unable
     * to render the character.
     * 
     * All characters in a ligature group conceptually share all the glyphs
     * associated with the characters in a group.
     */
    class CompressedGlyph {
    public:
        CompressedGlyph() { mValue = 0; }

        enum {
            // Indicates that a cluster and ligature group starts at this
            // character; this character has a single glyph with a reasonable
            // advance and zero offsets. A "reasonable" advance
            // is one that fits in the available bits (currently 12) (specified
            // in appunits).
            FLAG_IS_SIMPLE_GLYPH  = 0x80000000U,

            // Indicates whether a linebreak is allowed before this character;
            // this is a two-bit field that holds a FLAG_BREAK_TYPE_xxx value
            // indicating the kind of linebreak (if any) allowed here.
            FLAGS_CAN_BREAK_BEFORE = 0x60000000U,

            FLAGS_CAN_BREAK_SHIFT = 29,
            FLAG_BREAK_TYPE_NONE   = 0,
            FLAG_BREAK_TYPE_NORMAL = 1,
            FLAG_BREAK_TYPE_HYPHEN = 2,

            FLAG_CHAR_IS_SPACE     = 0x10000000U,

            // The advance is stored in appunits
            ADVANCE_MASK  = 0x0FFF0000U,
            ADVANCE_SHIFT = 16,

            GLYPH_MASK = 0x0000FFFFU,

            // Non-simple glyphs may or may not have glyph data in the
            // corresponding mDetailedGlyphs entry. They have the following
            // flag bits:

            // When NOT set, indicates that this character corresponds to a
            // missing glyph and should be skipped (or possibly, render the character
            // Unicode value in some special way). If there are glyphs,
            // the mGlyphID is actually the UTF16 character code. The bit is
            // inverted so we can memset the array to zero to indicate all missing.
            FLAG_NOT_MISSING              = 0x01,
            FLAG_NOT_CLUSTER_START        = 0x02,
            FLAG_NOT_LIGATURE_GROUP_START = 0x04,

            FLAG_CHAR_IS_TAB              = 0x08,
            FLAG_CHAR_IS_NEWLINE          = 0x10,
            FLAG_CHAR_IS_LOW_SURROGATE    = 0x20,
            CHAR_IDENTITY_FLAGS_MASK      = 0x38,

            GLYPH_COUNT_MASK = 0x00FFFF00U,
            GLYPH_COUNT_SHIFT = 8
        };

        // "Simple glyphs" have a simple glyph ID, simple advance and their
        // x and y offsets are zero. Also the glyph extents do not overflow
        // the font-box defined by the font ascent, descent and glyph advance width.
        // These case is optimized to avoid storing DetailedGlyphs.

        // Returns true if the glyph ID aGlyph fits into the compressed representation
        static bool IsSimpleGlyphID(uint32_t aGlyph) {
            return (aGlyph & GLYPH_MASK) == aGlyph;
        }
        // Returns true if the advance aAdvance fits into the compressed representation.
        // aAdvance is in appunits.
        static bool IsSimpleAdvance(uint32_t aAdvance) {
            return (aAdvance & (ADVANCE_MASK >> ADVANCE_SHIFT)) == aAdvance;
        }

        bool IsSimpleGlyph() const { return (mValue & FLAG_IS_SIMPLE_GLYPH) != 0; }
        uint32_t GetSimpleAdvance() const { return (mValue & ADVANCE_MASK) >> ADVANCE_SHIFT; }
        uint32_t GetSimpleGlyph() const { return mValue & GLYPH_MASK; }

        bool IsMissing() const { return (mValue & (FLAG_NOT_MISSING|FLAG_IS_SIMPLE_GLYPH)) == 0; }
        bool IsClusterStart() const {
            return (mValue & FLAG_IS_SIMPLE_GLYPH) || !(mValue & FLAG_NOT_CLUSTER_START);
        }
        bool IsLigatureGroupStart() const {
            return (mValue & FLAG_IS_SIMPLE_GLYPH) || !(mValue & FLAG_NOT_LIGATURE_GROUP_START);
        }
        bool IsLigatureContinuation() const {
            return (mValue & FLAG_IS_SIMPLE_GLYPH) == 0 &&
                (mValue & (FLAG_NOT_LIGATURE_GROUP_START | FLAG_NOT_MISSING)) ==
                    (FLAG_NOT_LIGATURE_GROUP_START | FLAG_NOT_MISSING);
        }

        // Return true if the original character was a normal (breakable,
        // trimmable) space (U+0020). Not true for other characters that
        // may happen to map to the space glyph (U+00A0).
        bool CharIsSpace() const {
            return (mValue & FLAG_CHAR_IS_SPACE) != 0;
        }

        bool CharIsTab() const {
            return !IsSimpleGlyph() && (mValue & FLAG_CHAR_IS_TAB) != 0;
        }
        bool CharIsNewline() const {
            return !IsSimpleGlyph() && (mValue & FLAG_CHAR_IS_NEWLINE) != 0;
        }
        bool CharIsLowSurrogate() const {
            return !IsSimpleGlyph() && (mValue & FLAG_CHAR_IS_LOW_SURROGATE) != 0;
        }

        uint32_t CharIdentityFlags() const {
            return IsSimpleGlyph() ? 0 : (mValue & CHAR_IDENTITY_FLAGS_MASK);
        }

        void SetClusterStart(bool aIsClusterStart) {
            NS_ASSERTION(!IsSimpleGlyph(),
                         "can't call SetClusterStart on simple glyphs");
            if (aIsClusterStart) {
                mValue &= ~FLAG_NOT_CLUSTER_START;
            } else {
                mValue |= FLAG_NOT_CLUSTER_START;
            }
        }

        uint8_t CanBreakBefore() const {
            return (mValue & FLAGS_CAN_BREAK_BEFORE) >> FLAGS_CAN_BREAK_SHIFT;
        }
        // Returns FLAGS_CAN_BREAK_BEFORE if the setting changed, 0 otherwise
        uint32_t SetCanBreakBefore(uint8_t aCanBreakBefore) {
            NS_ASSERTION(aCanBreakBefore <= 2,
                         "Bogus break-before value!");
            uint32_t breakMask = (uint32_t(aCanBreakBefore) << FLAGS_CAN_BREAK_SHIFT);
            uint32_t toggle = breakMask ^ (mValue & FLAGS_CAN_BREAK_BEFORE);
            mValue ^= toggle;
            return toggle;
        }

        CompressedGlyph& SetSimpleGlyph(uint32_t aAdvanceAppUnits, uint32_t aGlyph) {
            NS_ASSERTION(IsSimpleAdvance(aAdvanceAppUnits), "Advance overflow");
            NS_ASSERTION(IsSimpleGlyphID(aGlyph), "Glyph overflow");
            NS_ASSERTION(!CharIdentityFlags(), "Char identity flags lost");
            mValue = (mValue & (FLAGS_CAN_BREAK_BEFORE | FLAG_CHAR_IS_SPACE)) |
                FLAG_IS_SIMPLE_GLYPH |
                (aAdvanceAppUnits << ADVANCE_SHIFT) | aGlyph;
            return *this;
        }
        CompressedGlyph& SetComplex(bool aClusterStart, bool aLigatureStart,
                uint32_t aGlyphCount) {
            mValue = (mValue & (FLAGS_CAN_BREAK_BEFORE | FLAG_CHAR_IS_SPACE)) |
                FLAG_NOT_MISSING |
                CharIdentityFlags() |
                (aClusterStart ? 0 : FLAG_NOT_CLUSTER_START) |
                (aLigatureStart ? 0 : FLAG_NOT_LIGATURE_GROUP_START) |
                (aGlyphCount << GLYPH_COUNT_SHIFT);
            return *this;
        }
        /**
         * Missing glyphs are treated as ligature group starts; don't mess with
         * the cluster-start flag (see bugs 618870 and 619286).
         */
        CompressedGlyph& SetMissing(uint32_t aGlyphCount) {
            mValue = (mValue & (FLAGS_CAN_BREAK_BEFORE | FLAG_NOT_CLUSTER_START |
                                FLAG_CHAR_IS_SPACE)) |
                CharIdentityFlags() |
                (aGlyphCount << GLYPH_COUNT_SHIFT);
            return *this;
        }
        uint32_t GetGlyphCount() const {
            NS_ASSERTION(!IsSimpleGlyph(), "Expected non-simple-glyph");
            return (mValue & GLYPH_COUNT_MASK) >> GLYPH_COUNT_SHIFT;
        }

        void SetIsSpace() {
            mValue |= FLAG_CHAR_IS_SPACE;
        }
        void SetIsTab() {
            NS_ASSERTION(!IsSimpleGlyph(), "Expected non-simple-glyph");
            mValue |= FLAG_CHAR_IS_TAB;
        }
        void SetIsNewline() {
            NS_ASSERTION(!IsSimpleGlyph(), "Expected non-simple-glyph");
            mValue |= FLAG_CHAR_IS_NEWLINE;
        }
        void SetIsLowSurrogate() {
            NS_ASSERTION(!IsSimpleGlyph(), "Expected non-simple-glyph");
            mValue |= FLAG_CHAR_IS_LOW_SURROGATE;
        }

    private:
        uint32_t mValue;
    };

    // Accessor for the array of CompressedGlyph records, which will be in
    // a different place in gfxShapedWord vs gfxTextRun
    virtual CompressedGlyph *GetCharacterGlyphs() = 0;

    /**
     * When the glyphs for a character don't fit into a CompressedGlyph record
     * in SimpleGlyph format, we use an array of DetailedGlyphs instead.
     */
    struct DetailedGlyph {
        /** The glyphID, or the Unicode character
         * if this is a missing glyph */
        uint32_t mGlyphID;
        /** The advance, x-offset and y-offset of the glyph, in appunits
         *  mAdvance is in the text direction (RTL or LTR)
         *  mXOffset is always from left to right
         *  mYOffset is always from top to bottom */   
        int32_t  mAdvance;
        float    mXOffset, mYOffset;
    };

    void SetGlyphs(uint32_t aCharIndex, CompressedGlyph aGlyph,
                   const DetailedGlyph *aGlyphs);

    void SetMissingGlyph(uint32_t aIndex, uint32_t aChar, gfxFont *aFont);

    void SetIsSpace(uint32_t aIndex) {
        GetCharacterGlyphs()[aIndex].SetIsSpace();
    }

    void SetIsLowSurrogate(uint32_t aIndex) {
        SetGlyphs(aIndex, CompressedGlyph().SetComplex(false, false, 0), nullptr);
        GetCharacterGlyphs()[aIndex].SetIsLowSurrogate();
    }

    bool HasDetailedGlyphs() const {
        return mDetailedGlyphs != nullptr;
    }

    bool IsClusterStart(uint32_t aPos) {
        NS_ASSERTION(aPos < GetLength(), "aPos out of range");
        return GetCharacterGlyphs()[aPos].IsClusterStart();
    }

    bool IsLigatureGroupStart(uint32_t aPos) {
        NS_ASSERTION(aPos < GetLength(), "aPos out of range");
        return GetCharacterGlyphs()[aPos].IsLigatureGroupStart();
    }

    // NOTE that this must not be called for a character offset that does
    // not have any DetailedGlyph records; callers must have verified that
    // GetCharacterGlyphs()[aCharIndex].GetGlyphCount() is greater than zero.
    DetailedGlyph *GetDetailedGlyphs(uint32_t aCharIndex) {
        NS_ASSERTION(GetCharacterGlyphs() && HasDetailedGlyphs() &&
                     !GetCharacterGlyphs()[aCharIndex].IsSimpleGlyph() &&
                     GetCharacterGlyphs()[aCharIndex].GetGlyphCount() > 0,
                     "invalid use of GetDetailedGlyphs; check the caller!");
        return mDetailedGlyphs->Get(aCharIndex);
    }

    void AdjustAdvancesForSyntheticBold(float aSynBoldOffset,
                                        uint32_t aOffset, uint32_t aLength);

    // Mark clusters in the CompressedGlyph records, starting at aOffset,
    // based on the Unicode properties of the text in aString.
    // This is also responsible to set the IsSpace flag for space characters.
    void SetupClusterBoundaries(uint32_t         aOffset,
                                const char16_t *aString,
                                uint32_t         aLength);
    // In 8-bit text, there won't actually be any clusters, but we still need
    // the space-marking functionality.
    void SetupClusterBoundaries(uint32_t       aOffset,
                                const uint8_t *aString,
                                uint32_t       aLength);

    uint32_t Flags() const {
        return mFlags;
    }

    bool IsRightToLeft() const {
        return (Flags() & gfxTextRunFactory::TEXT_IS_RTL) != 0;
    }

    float GetDirection() const {
        return IsRightToLeft() ? -1.0f : 1.0f;
    }

    bool DisableLigatures() const {
        return (Flags() & gfxTextRunFactory::TEXT_DISABLE_OPTIONAL_LIGATURES) != 0;
    }

    bool TextIs8Bit() const {
        return (Flags() & gfxTextRunFactory::TEXT_IS_8BIT) != 0;
    }

    int32_t GetAppUnitsPerDevUnit() const {
        return mAppUnitsPerDevUnit;
    }

    uint32_t GetLength() const {
        return mLength;
    }

    bool FilterIfIgnorable(uint32_t aIndex, uint32_t aCh);

protected:
    // Allocate aCount DetailedGlyphs for the given index
    DetailedGlyph *AllocateDetailedGlyphs(uint32_t aCharIndex,
                                          uint32_t aCount);

    // For characters whose glyph data does not fit the "simple" glyph criteria
    // in CompressedGlyph, we use a sorted array to store the association
    // between the source character offset and an index into an array 
    // DetailedGlyphs. The CompressedGlyph record includes a count of
    // the number of DetailedGlyph records that belong to the character,
    // starting at the given index.
    class DetailedGlyphStore {
    public:
        DetailedGlyphStore()
            : mLastUsed(0)
        { }

        // This is optimized for the most common calling patterns:
        // we rarely need random access to the records, access is most commonly
        // sequential through the textRun, so we record the last-used index
        // and check whether the caller wants the same record again, or the
        // next; if not, it's most likely we're starting over from the start
        // of the run, so we check the first entry before resorting to binary
        // search as a last resort.
        // NOTE that this must not be called for a character offset that does
        // not have any DetailedGlyph records; callers must have verified that
        // mCharacterGlyphs[aOffset].GetGlyphCount() is greater than zero
        // before calling this, otherwise the assertions here will fire (in a
        // debug build), and we'll probably crash.
        DetailedGlyph* Get(uint32_t aOffset) {
            NS_ASSERTION(mOffsetToIndex.Length() > 0,
                         "no detailed glyph records!");
            DetailedGlyph* details = mDetails.Elements();
            // check common cases (fwd iteration, initial entry, etc) first
            if (mLastUsed < mOffsetToIndex.Length() - 1 &&
                aOffset == mOffsetToIndex[mLastUsed + 1].mOffset) {
                ++mLastUsed;
            } else if (aOffset == mOffsetToIndex[0].mOffset) {
                mLastUsed = 0;
            } else if (aOffset == mOffsetToIndex[mLastUsed].mOffset) {
                // do nothing
            } else if (mLastUsed > 0 &&
                       aOffset == mOffsetToIndex[mLastUsed - 1].mOffset) {
                --mLastUsed;
            } else {
                mLastUsed =
                    mOffsetToIndex.BinaryIndexOf(aOffset, CompareToOffset());
            }
            NS_ASSERTION(mLastUsed != nsTArray<DGRec>::NoIndex,
                         "detailed glyph record missing!");
            return details + mOffsetToIndex[mLastUsed].mIndex;
        }

        DetailedGlyph* Allocate(uint32_t aOffset, uint32_t aCount) {
            uint32_t detailIndex = mDetails.Length();
            DetailedGlyph *details = mDetails.AppendElements(aCount);
            if (!details) {
                return nullptr;
            }
            // We normally set up glyph records sequentially, so the common case
            // here is to append new records to the mOffsetToIndex array;
            // test for that before falling back to the InsertElementSorted
            // method.
            if (mOffsetToIndex.Length() == 0 ||
                aOffset > mOffsetToIndex[mOffsetToIndex.Length() - 1].mOffset) {
                if (!mOffsetToIndex.AppendElement(DGRec(aOffset, detailIndex))) {
                    return nullptr;
                }
            } else {
                if (!mOffsetToIndex.InsertElementSorted(DGRec(aOffset, detailIndex),
                                                        CompareRecordOffsets())) {
                    return nullptr;
                }
            }
            return details;
        }

        size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) {
            return aMallocSizeOf(this) +
                mDetails.SizeOfExcludingThis(aMallocSizeOf) +
                mOffsetToIndex.SizeOfExcludingThis(aMallocSizeOf);
        }

    private:
        struct DGRec {
            DGRec(const uint32_t& aOffset, const uint32_t& aIndex)
                : mOffset(aOffset), mIndex(aIndex) { }
            uint32_t mOffset; // source character offset in the textrun
            uint32_t mIndex;  // index where this char's DetailedGlyphs begin
        };

        struct CompareToOffset {
            bool Equals(const DGRec& a, const uint32_t& b) const {
                return a.mOffset == b;
            }
            bool LessThan(const DGRec& a, const uint32_t& b) const {
                return a.mOffset < b;
            }
        };

        struct CompareRecordOffsets {
            bool Equals(const DGRec& a, const DGRec& b) const {
                return a.mOffset == b.mOffset;
            }
            bool LessThan(const DGRec& a, const DGRec& b) const {
                return a.mOffset < b.mOffset;
            }
        };

        // Concatenated array of all the DetailedGlyph records needed for the
        // textRun; individual character offsets are associated with indexes
        // into this array via the mOffsetToIndex table.
        nsTArray<DetailedGlyph>     mDetails;

        // For each character offset that needs DetailedGlyphs, we record the
        // index in mDetails where the list of glyphs begins. This array is
        // sorted by mOffset.
        nsTArray<DGRec>             mOffsetToIndex;

        // Records the most recently used index into mOffsetToIndex, so that
        // we can support sequential access more quickly than just doing
        // a binary search each time.
        nsTArray<DGRec>::index_type mLastUsed;
    };

    nsAutoPtr<DetailedGlyphStore>   mDetailedGlyphs;

    // Number of char16_t characters and CompressedGlyph glyph records
    uint32_t                        mLength;

    // Shaping flags (direction, ligature-suppression)
    uint32_t                        mFlags;

    int32_t                         mAppUnitsPerDevUnit;
};

/*
 * gfxShapedWord: an individual (space-delimited) run of text shaped with a
 * particular font, without regard to external context.
 *
 * The glyph data is copied into gfxTextRuns as needed from the cache of
 * ShapedWords associated with each gfxFont instance.
 */
class gfxShapedWord : public gfxShapedText
{
public:
    // Create a ShapedWord that can hold glyphs for aLength characters,
    // with mCharacterGlyphs sized appropriately.
    //
    // Returns null on allocation failure (does NOT use infallible alloc)
    // so caller must check for success.
    //
    // This does NOT perform shaping, so the returned word contains no
    // glyph data; the caller must call gfxFont::ShapeText() with appropriate
    // parameters to set up the glyphs.
    static gfxShapedWord* Create(const uint8_t *aText, uint32_t aLength,
                                 int32_t aRunScript,
                                 int32_t aAppUnitsPerDevUnit,
                                 uint32_t aFlags) {
        NS_ASSERTION(aLength <= gfxPlatform::GetPlatform()->WordCacheCharLimit(),
                     "excessive length for gfxShapedWord!");

        // Compute size needed including the mCharacterGlyphs array
        // and a copy of the original text
        uint32_t size =
            offsetof(gfxShapedWord, mCharGlyphsStorage) +
            aLength * (sizeof(CompressedGlyph) + sizeof(uint8_t));
        void *storage = moz_malloc(size);
        if (!storage) {
            return nullptr;
        }

        // Construct in the pre-allocated storage, using placement new
        return new (storage) gfxShapedWord(aText, aLength, aRunScript,
                                           aAppUnitsPerDevUnit, aFlags);
    }

    static gfxShapedWord* Create(const char16_t *aText, uint32_t aLength,
                                 int32_t aRunScript,
                                 int32_t aAppUnitsPerDevUnit,
                                 uint32_t aFlags) {
        NS_ASSERTION(aLength <= gfxPlatform::GetPlatform()->WordCacheCharLimit(),
                     "excessive length for gfxShapedWord!");

        // In the 16-bit version of Create, if the TEXT_IS_8BIT flag is set,
        // then we convert the text to an 8-bit version and call the 8-bit
        // Create function instead.
        if (aFlags & gfxTextRunFactory::TEXT_IS_8BIT) {
            nsAutoCString narrowText;
            LossyAppendUTF16toASCII(nsDependentSubstring(aText, aLength),
                                    narrowText);
            return Create((const uint8_t*)(narrowText.BeginReading()),
                          aLength, aRunScript, aAppUnitsPerDevUnit, aFlags);
        }

        uint32_t size =
            offsetof(gfxShapedWord, mCharGlyphsStorage) +
            aLength * (sizeof(CompressedGlyph) + sizeof(char16_t));
        void *storage = moz_malloc(size);
        if (!storage) {
            return nullptr;
        }

        return new (storage) gfxShapedWord(aText, aLength, aRunScript,
                                           aAppUnitsPerDevUnit, aFlags);
    }

    // Override operator delete to properly free the object that was
    // allocated via moz_malloc.
    void operator delete(void* p) {
        moz_free(p);
    }

    CompressedGlyph *GetCharacterGlyphs() {
        return &mCharGlyphsStorage[0];
    }

    const uint8_t* Text8Bit() const {
        NS_ASSERTION(TextIs8Bit(), "invalid use of Text8Bit()");
        return reinterpret_cast<const uint8_t*>(mCharGlyphsStorage + GetLength());
    }

    const char16_t* TextUnicode() const {
        NS_ASSERTION(!TextIs8Bit(), "invalid use of TextUnicode()");
        return reinterpret_cast<const char16_t*>(mCharGlyphsStorage + GetLength());
    }

    char16_t GetCharAt(uint32_t aOffset) const {
        NS_ASSERTION(aOffset < GetLength(), "aOffset out of range");
        return TextIs8Bit() ?
            char16_t(Text8Bit()[aOffset]) : TextUnicode()[aOffset];
    }

    int32_t Script() const {
        return mScript;
    }

    void ResetAge() {
        mAgeCounter = 0;
    }
    uint32_t IncrementAge() {
        return ++mAgeCounter;
    }

private:
    // so that gfxTextRun can share our DetailedGlyphStore class
    friend class gfxTextRun;

    // Construct storage for a ShapedWord, ready to receive glyph data
    gfxShapedWord(const uint8_t *aText, uint32_t aLength,
                  int32_t aRunScript, int32_t aAppUnitsPerDevUnit,
                  uint32_t aFlags)
        : gfxShapedText(aLength, aFlags | gfxTextRunFactory::TEXT_IS_8BIT,
                        aAppUnitsPerDevUnit)
        , mScript(aRunScript)
        , mAgeCounter(0)
    {
        memset(mCharGlyphsStorage, 0, aLength * sizeof(CompressedGlyph));
        uint8_t *text = reinterpret_cast<uint8_t*>(&mCharGlyphsStorage[aLength]);
        memcpy(text, aText, aLength * sizeof(uint8_t));
    }

    gfxShapedWord(const char16_t *aText, uint32_t aLength,
                  int32_t aRunScript, int32_t aAppUnitsPerDevUnit,
                  uint32_t aFlags)
        : gfxShapedText(aLength, aFlags, aAppUnitsPerDevUnit)
        , mScript(aRunScript)
        , mAgeCounter(0)
    {
        memset(mCharGlyphsStorage, 0, aLength * sizeof(CompressedGlyph));
        char16_t *text = reinterpret_cast<char16_t*>(&mCharGlyphsStorage[aLength]);
        memcpy(text, aText, aLength * sizeof(char16_t));
        SetupClusterBoundaries(0, aText, aLength);
    }

    int32_t          mScript;

    uint32_t         mAgeCounter;

    // The mCharGlyphsStorage array is actually a variable-size member;
    // when the ShapedWord is created, its size will be increased as necessary
    // to allow the proper number of glyphs to be stored.
    // The original text, in either 8-bit or 16-bit form, will be stored
    // immediately following the CompressedGlyphs.
    CompressedGlyph  mCharGlyphsStorage[1];
};

/**
 * Callback for Draw() to use when drawing text with mode
 * DrawMode::GLYPH_PATH.
 */
struct gfxTextRunDrawCallbacks {

    /**
     * Constructs a new DrawCallbacks object.
     *
     * @param aShouldPaintSVGGlyphs If true, SVG glyphs will be
     *   painted and the NotifyBeforeSVGGlyphPainted/NotifyAfterSVGGlyphPainted
     *   callbacks will be invoked for each SVG glyph.  If false, SVG glyphs
     *   will not be painted; fallback plain glyphs are not emitted either.
     */
    gfxTextRunDrawCallbacks(bool aShouldPaintSVGGlyphs = false)
      : mShouldPaintSVGGlyphs(aShouldPaintSVGGlyphs)
    {
    }

    /**
     * Called when a path has been emitted to the gfxContext when
     * painting a text run.  This can be called any number of times,
     * due to partial ligatures and intervening SVG glyphs.
     */
    virtual void NotifyGlyphPathEmitted() = 0;

    /**
     * Called just before an SVG glyph has been painted to the gfxContext.
     */
    virtual void NotifyBeforeSVGGlyphPainted() { }

    /**
     * Called just after an SVG glyph has been painted to the gfxContext.
     */
    virtual void NotifyAfterSVGGlyphPainted() { }

    bool mShouldPaintSVGGlyphs;
};

/**
 * gfxTextRun is an abstraction for drawing and measuring substrings of a run
 * of text. It stores runs of positioned glyph data, each run having a single
 * gfxFont. The glyphs are associated with a string of source text, and the
 * gfxTextRun APIs take parameters that are offsets into that source text.
 * 
 * gfxTextRuns are not refcounted. They should be deleted when no longer required.
 * 
 * gfxTextRuns are mostly immutable. The only things that can change are
 * inter-cluster spacing and line break placement. Spacing is always obtained
 * lazily by methods that need it, it is not cached. Line breaks are stored
 * persistently (insofar as they affect the shaping of glyphs; gfxTextRun does
 * not actually do anything to explicitly account for line breaks). Initially
 * there are no line breaks. The textrun can record line breaks before or after
 * any given cluster. (Line breaks specified inside clusters are ignored.)
 * 
 * It is important that zero-length substrings are handled correctly. This will
 * be on the test!
 */
class gfxTextRun : public gfxShapedText {
public:

    // Override operator delete to properly free the object that was
    // allocated via moz_malloc.
    void operator delete(void* p) {
        moz_free(p);
    }

    virtual ~gfxTextRun();

    typedef gfxFont::RunMetrics Metrics;

    // Public textrun API for general use

    bool IsClusterStart(uint32_t aPos) {
        NS_ASSERTION(aPos < GetLength(), "aPos out of range");
        return mCharacterGlyphs[aPos].IsClusterStart();
    }
    bool IsLigatureGroupStart(uint32_t aPos) {
        NS_ASSERTION(aPos < GetLength(), "aPos out of range");
        return mCharacterGlyphs[aPos].IsLigatureGroupStart();
    }
    bool CanBreakLineBefore(uint32_t aPos) {
        NS_ASSERTION(aPos < GetLength(), "aPos out of range");
        return mCharacterGlyphs[aPos].CanBreakBefore() ==
            CompressedGlyph::FLAG_BREAK_TYPE_NORMAL;
    }
    bool CanHyphenateBefore(uint32_t aPos) {
        NS_ASSERTION(aPos < GetLength(), "aPos out of range");
        return mCharacterGlyphs[aPos].CanBreakBefore() ==
            CompressedGlyph::FLAG_BREAK_TYPE_HYPHEN;
    }

    bool CharIsSpace(uint32_t aPos) {
        NS_ASSERTION(aPos < GetLength(), "aPos out of range");
        return mCharacterGlyphs[aPos].CharIsSpace();
    }
    bool CharIsTab(uint32_t aPos) {
        NS_ASSERTION(aPos < GetLength(), "aPos out of range");
        return mCharacterGlyphs[aPos].CharIsTab();
    }
    bool CharIsNewline(uint32_t aPos) {
        NS_ASSERTION(aPos < GetLength(), "aPos out of range");
        return mCharacterGlyphs[aPos].CharIsNewline();
    }
    bool CharIsLowSurrogate(uint32_t aPos) {
        NS_ASSERTION(aPos < GetLength(), "aPos out of range");
        return mCharacterGlyphs[aPos].CharIsLowSurrogate();
    }

    uint32_t GetLength() { return mLength; }

    // All uint32_t aStart, uint32_t aLength ranges below are restricted to
    // grapheme cluster boundaries! All offsets are in terms of the string
    // passed into MakeTextRun.
    
    // All coordinates are in layout/app units

    /**
     * Set the potential linebreaks for a substring of the textrun. These are
     * the "allow break before" points. Initially, there are no potential
     * linebreaks.
     * 
     * This can change glyphs and/or geometry! Some textruns' shapes
     * depend on potential line breaks (e.g., title-case-converting textruns).
     * This function is virtual so that those textruns can reshape themselves.
     * 
     * @return true if this changed the linebreaks, false if the new line
     * breaks are the same as the old
     */
    virtual bool SetPotentialLineBreaks(uint32_t aStart, uint32_t aLength,
                                          uint8_t *aBreakBefore,
                                          gfxContext *aRefContext);

    /**
     * Layout provides PropertyProvider objects. These allow detection of
     * potential line break points and computation of spacing. We pass the data
     * this way to allow lazy data acquisition; for example BreakAndMeasureText
     * will want to only ask for properties of text it's actually looking at.
     * 
     * NOTE that requested spacing may not actually be applied, if the textrun
     * is unable to apply it in some context. Exception: spacing around a
     * whitespace character MUST always be applied.
     */
    class PropertyProvider {
    public:
        // Detect hyphenation break opportunities in the given range; breaks
        // not at cluster boundaries will be ignored.
        virtual void GetHyphenationBreaks(uint32_t aStart, uint32_t aLength,
                                          bool *aBreakBefore) = 0;

        // Returns the provider's hyphenation setting, so callers can decide
        // whether it is necessary to call GetHyphenationBreaks.
        // Result is an NS_STYLE_HYPHENS_* value.
        virtual int8_t GetHyphensOption() = 0;

        // Returns the extra width that will be consumed by a hyphen. This should
        // be constant for a given textrun.
        virtual gfxFloat GetHyphenWidth() = 0;

        typedef gfxFont::Spacing Spacing;

        /**
         * Get the spacing around the indicated characters. Spacing must be zero
         * inside clusters. In other words, if character i is not
         * CLUSTER_START, then character i-1 must have zero after-spacing and
         * character i must have zero before-spacing.
         */
        virtual void GetSpacing(uint32_t aStart, uint32_t aLength,
                                Spacing *aSpacing) = 0;
    };

    class ClusterIterator {
    public:
        ClusterIterator(gfxTextRun *aTextRun);

        void Reset();

        bool NextCluster();

        uint32_t Position() const {
            return mCurrentChar;
        }

        uint32_t ClusterLength() const;

        gfxFloat ClusterAdvance(PropertyProvider *aProvider) const;

    private:
        gfxTextRun *mTextRun;
        uint32_t    mCurrentChar;
    };

    /**
     * Draws a substring. Uses only GetSpacing from aBreakProvider.
     * The provided point is the baseline origin on the left of the string
     * for LTR, on the right of the string for RTL.
     * @param aAdvanceWidth if non-null, the advance width of the substring
     * is returned here.
     * 
     * Drawing should respect advance widths in the sense that for LTR runs,
     * Draw(ctx, pt, offset1, length1, dirty, &provider, &advance) followed by
     * Draw(ctx, gfxPoint(pt.x + advance, pt.y), offset1 + length1, length2,
     *      dirty, &provider, nullptr) should have the same effect as
     * Draw(ctx, pt, offset1, length1+length2, dirty, &provider, nullptr).
     * For RTL runs the rule is:
     * Draw(ctx, pt, offset1 + length1, length2, dirty, &provider, &advance) followed by
     * Draw(ctx, gfxPoint(pt.x + advance, pt.y), offset1, length1,
     *      dirty, &provider, nullptr) should have the same effect as
     * Draw(ctx, pt, offset1, length1+length2, dirty, &provider, nullptr).
     * 
     * Glyphs should be drawn in logical content order, which can be significant
     * if they overlap (perhaps due to negative spacing).
     */
    void Draw(gfxContext *aContext, gfxPoint aPt,
              DrawMode aDrawMode,
              uint32_t aStart, uint32_t aLength,
              PropertyProvider *aProvider,
              gfxFloat *aAdvanceWidth, gfxTextContextPaint *aContextPaint,
              gfxTextRunDrawCallbacks *aCallbacks = nullptr);

    /**
     * Computes the ReflowMetrics for a substring.
     * Uses GetSpacing from aBreakProvider.
     * @param aBoundingBoxType which kind of bounding box (loose/tight)
     */
    Metrics MeasureText(uint32_t aStart, uint32_t aLength,
                        gfxFont::BoundingBoxType aBoundingBoxType,
                        gfxContext *aRefContextForTightBoundingBox,
                        PropertyProvider *aProvider);

    /**
     * Computes just the advance width for a substring.
     * Uses GetSpacing from aBreakProvider.
     */
    gfxFloat GetAdvanceWidth(uint32_t aStart, uint32_t aLength,
                             PropertyProvider *aProvider);

    /**
     * Clear all stored line breaks for the given range (both before and after),
     * and then set the line-break state before aStart to aBreakBefore and
     * after the last cluster to aBreakAfter.
     * 
     * We require that before and after line breaks be consistent. For clusters
     * i and i+1, we require that if there is a break after cluster i, a break
     * will be specified before cluster i+1. This may be temporarily violated
     * (e.g. after reflowing line L and before reflowing line L+1); to handle
     * these temporary violations, we say that there is a break betwen i and i+1
     * if a break is specified after i OR a break is specified before i+1.
     * 
     * This can change textrun geometry! The existence of a linebreak can affect
     * the advance width of the cluster before the break (when kerning) or the
     * geometry of one cluster before the break or any number of clusters
     * after the break. (The one-cluster-before-the-break limit is somewhat
     * arbitrary; if some scripts require breaking it, then we need to
     * alter nsTextFrame::TrimTrailingWhitespace, perhaps drastically becase
     * it could affect the layout of frames before it...)
     * 
     * We return true if glyphs or geometry changed, false otherwise. This
     * function is virtual so that gfxTextRun subclasses can reshape
     * properly.
     * 
     * @param aAdvanceWidthDelta if non-null, returns the change in advance
     * width of the given range.
     */
    virtual bool SetLineBreaks(uint32_t aStart, uint32_t aLength,
                                 bool aLineBreakBefore, bool aLineBreakAfter,
                                 gfxFloat *aAdvanceWidthDelta,
                                 gfxContext *aRefContext);

    /**
     * Finds the longest substring that will fit into the given width.
     * Uses GetHyphenationBreaks and GetSpacing from aBreakProvider.
     * Guarantees the following:
     * -- 0 <= result <= aMaxLength
     * -- result is the maximal value of N such that either
     *       N < aMaxLength && line break at N && GetAdvanceWidth(aStart, N) <= aWidth
     *   OR  N < aMaxLength && hyphen break at N && GetAdvanceWidth(aStart, N) + GetHyphenWidth() <= aWidth
     *   OR  N == aMaxLength && GetAdvanceWidth(aStart, N) <= aWidth
     * where GetAdvanceWidth assumes the effect of
     * SetLineBreaks(aStart, N, aLineBreakBefore, N < aMaxLength, aProvider)
     * -- if no such N exists, then result is the smallest N such that
     *       N < aMaxLength && line break at N
     *   OR  N < aMaxLength && hyphen break at N
     *   OR  N == aMaxLength
     *
     * The call has the effect of
     * SetLineBreaks(aStart, result, aLineBreakBefore, result < aMaxLength, aProvider)
     * and the returned metrics and the invariants above reflect this.
     *
     * @param aMaxLength this can be UINT32_MAX, in which case the length used
     * is up to the end of the string
     * @param aLineBreakBefore set to true if and only if there is an actual
     * line break at the start of this string.
     * @param aSuppressInitialBreak if true, then we assume there is no possible
     * linebreak before aStart. If false, then we will check the internal
     * line break opportunity state before deciding whether to return 0 as the
     * character to break before.
     * @param aTrimWhitespace if non-null, then we allow a trailing run of
     * spaces to be trimmed; the width of the space(s) will not be included in
     * the measured string width for comparison with the limit aWidth, and
     * trimmed spaces will not be included in returned metrics. The width
     * of the trimmed spaces will be returned in aTrimWhitespace.
     * Trimmed spaces are still counted in the "characters fit" result.
     * @param aMetrics if non-null, we fill this in for the returned substring.
     * If a hyphenation break was used, the hyphen is NOT included in the returned metrics.
     * @param aBoundingBoxType whether to make the bounding box in aMetrics tight
     * @param aRefContextForTightBoundingBox a reference context to get the
     * tight bounding box, if requested
     * @param aUsedHyphenation if non-null, records if we selected a hyphenation break
     * @param aLastBreak if non-null and result is aMaxLength, we set this to
     * the maximal N such that
     *       N < aMaxLength && line break at N && GetAdvanceWidth(aStart, N) <= aWidth
     *   OR  N < aMaxLength && hyphen break at N && GetAdvanceWidth(aStart, N) + GetHyphenWidth() <= aWidth
     * or UINT32_MAX if no such N exists, where GetAdvanceWidth assumes
     * the effect of
     * SetLineBreaks(aStart, N, aLineBreakBefore, N < aMaxLength, aProvider)
     *
     * @param aCanWordWrap true if we can break between any two grapheme
     * clusters. This is set by word-wrap: break-word
     *
     * @param aBreakPriority in/out the priority of the break opportunity
     * saved in the line. If we are prioritizing break opportunities, we will
     * not set a break with a lower priority. @see gfxBreakPriority.
     * 
     * Note that negative advance widths are possible especially if negative
     * spacing is provided.
     */
    uint32_t BreakAndMeasureText(uint32_t aStart, uint32_t aMaxLength,
                                 bool aLineBreakBefore, gfxFloat aWidth,
                                 PropertyProvider *aProvider,
                                 bool aSuppressInitialBreak,
                                 gfxFloat *aTrimWhitespace,
                                 Metrics *aMetrics,
                                 gfxFont::BoundingBoxType aBoundingBoxType,
                                 gfxContext *aRefContextForTightBoundingBox,
                                 bool *aUsedHyphenation,
                                 uint32_t *aLastBreak,
                                 bool aCanWordWrap,
                                 gfxBreakPriority *aBreakPriority);

    /**
     * Update the reference context.
     * XXX this is a hack. New text frame does not call this. Use only
     * temporarily for old text frame.
     */
    void SetContext(gfxContext *aContext) {}

    // Utility getters

    gfxFloat GetDirection() const { return (mFlags & gfxTextRunFactory::TEXT_IS_RTL) ? -1.0 : 1.0; }
    void *GetUserData() const { return mUserData; }
    void SetUserData(void *aUserData) { mUserData = aUserData; }
    uint32_t GetFlags() const { return mFlags; }
    void SetFlagBits(uint32_t aFlags) {
      NS_ASSERTION(!(aFlags & ~gfxTextRunFactory::SETTABLE_FLAGS),
                   "Only user flags should be mutable");
      mFlags |= aFlags;
    }
    void ClearFlagBits(uint32_t aFlags) {
      NS_ASSERTION(!(aFlags & ~gfxTextRunFactory::SETTABLE_FLAGS),
                   "Only user flags should be mutable");
      mFlags &= ~aFlags;
    }
    const gfxSkipChars& GetSkipChars() const { return mSkipChars; }
    gfxFontGroup *GetFontGroup() const { return mFontGroup; }


    // Call this, don't call "new gfxTextRun" directly. This does custom
    // allocation and initialization
    static gfxTextRun *Create(const gfxTextRunFactory::Parameters *aParams,
                              uint32_t aLength, gfxFontGroup *aFontGroup,
                              uint32_t aFlags);

    // The text is divided into GlyphRuns as necessary
    struct GlyphRun {
        nsRefPtr<gfxFont> mFont;   // never null
        uint32_t          mCharacterOffset; // into original UTF16 string
        uint8_t           mMatchType;
    };

    class GlyphRunIterator {
    public:
        GlyphRunIterator(gfxTextRun *aTextRun, uint32_t aStart, uint32_t aLength)
          : mTextRun(aTextRun), mStartOffset(aStart), mEndOffset(aStart + aLength) {
            mNextIndex = mTextRun->FindFirstGlyphRunContaining(aStart);
        }
        bool NextRun();
        GlyphRun *GetGlyphRun() { return mGlyphRun; }
        uint32_t GetStringStart() { return mStringStart; }
        uint32_t GetStringEnd() { return mStringEnd; }
    private:
        gfxTextRun *mTextRun;
        GlyphRun   *mGlyphRun;
        uint32_t    mStringStart;
        uint32_t    mStringEnd;
        uint32_t    mNextIndex;
        uint32_t    mStartOffset;
        uint32_t    mEndOffset;
    };

    class GlyphRunOffsetComparator {
    public:
        bool Equals(const GlyphRun& a,
                      const GlyphRun& b) const
        {
            return a.mCharacterOffset == b.mCharacterOffset;
        }

        bool LessThan(const GlyphRun& a,
                        const GlyphRun& b) const
        {
            return a.mCharacterOffset < b.mCharacterOffset;
        }
    };

    friend class GlyphRunIterator;
    friend class FontSelector;

    // API for setting up the textrun glyphs. Should only be called by
    // things that construct textruns.
    /**
     * We've found a run of text that should use a particular font. Call this
     * only during initialization when font substitution has been computed.
     * Call it before setting up the glyphs for the characters in this run;
     * SetMissingGlyph requires that the correct glyphrun be installed.
     *
     * If aForceNewRun, a new glyph run will be added, even if the
     * previously added run uses the same font.  If glyph runs are
     * added out of strictly increasing aStartCharIndex order (via
     * force), then SortGlyphRuns must be called after all glyph runs
     * are added before any further operations are performed with this
     * TextRun.
     */
    nsresult AddGlyphRun(gfxFont *aFont, uint8_t aMatchType,
                         uint32_t aStartCharIndex, bool aForceNewRun);
    void ResetGlyphRuns() { mGlyphRuns.Clear(); }
    void SortGlyphRuns();
    void SanitizeGlyphRuns();

    CompressedGlyph* GetCharacterGlyphs() {
        NS_ASSERTION(mCharacterGlyphs, "failed to initialize mCharacterGlyphs");
        return mCharacterGlyphs;
    }

    void SetSpaceGlyph(gfxFont *aFont, gfxContext *aContext, uint32_t aCharIndex);

    // Set the glyph data for the given character index to the font's
    // space glyph, IF this can be done as a "simple" glyph record
    // (not requiring a DetailedGlyph entry). This avoids the need to call
    // the font shaper and go through the shaped-word cache for most spaces.
    //
    // The parameter aSpaceChar is the original character code for which
    // this space glyph is being used; if this is U+0020, we need to record
    // that it could be trimmed at a run edge, whereas other kinds of space
    // (currently just U+00A0) would not be trimmable/breakable.
    //
    // Returns true if it was able to set simple glyph data for the space;
    // if it returns false, the caller needs to fall back to some other
    // means to create the necessary (detailed) glyph data.
    bool SetSpaceGlyphIfSimple(gfxFont *aFont, gfxContext *aContext,
                               uint32_t aCharIndex, char16_t aSpaceChar);

    // Record the positions of specific characters that layout may need to
    // detect in the textrun, even though it doesn't have an explicit copy
    // of the original text. These are recorded using flag bits in the
    // CompressedGlyph record; if necessary, we convert "simple" glyph records
    // to "complex" ones as the Tab and Newline flags are not present in
    // simple CompressedGlyph records.
    void SetIsTab(uint32_t aIndex) {
        CompressedGlyph *g = &mCharacterGlyphs[aIndex];
        if (g->IsSimpleGlyph()) {
            DetailedGlyph *details = AllocateDetailedGlyphs(aIndex, 1);
            details->mGlyphID = g->GetSimpleGlyph();
            details->mAdvance = g->GetSimpleAdvance();
            details->mXOffset = details->mYOffset = 0;
            SetGlyphs(aIndex, CompressedGlyph().SetComplex(true, true, 1), details);
        }
        g->SetIsTab();
    }
    void SetIsNewline(uint32_t aIndex) {
        CompressedGlyph *g = &mCharacterGlyphs[aIndex];
        if (g->IsSimpleGlyph()) {
            DetailedGlyph *details = AllocateDetailedGlyphs(aIndex, 1);
            details->mGlyphID = g->GetSimpleGlyph();
            details->mAdvance = g->GetSimpleAdvance();
            details->mXOffset = details->mYOffset = 0;
            SetGlyphs(aIndex, CompressedGlyph().SetComplex(true, true, 1), details);
        }
        g->SetIsNewline();
    }
    void SetIsLowSurrogate(uint32_t aIndex) {
        SetGlyphs(aIndex, CompressedGlyph().SetComplex(false, false, 0), nullptr);
        mCharacterGlyphs[aIndex].SetIsLowSurrogate();
    }

    /**
     * Prefetch all the glyph extents needed to ensure that Measure calls
     * on this textrun not requesting tight boundingBoxes will succeed. Note
     * that some glyph extents might not be fetched due to OOM or other
     * errors.
     */
    void FetchGlyphExtents(gfxContext *aRefContext);

    uint32_t CountMissingGlyphs();
    const GlyphRun *GetGlyphRuns(uint32_t *aNumGlyphRuns) {
        *aNumGlyphRuns = mGlyphRuns.Length();
        return mGlyphRuns.Elements();
    }
    // Returns the index of the GlyphRun containing the given offset.
    // Returns mGlyphRuns.Length() when aOffset is mCharacterCount.
    uint32_t FindFirstGlyphRunContaining(uint32_t aOffset);

    // Copy glyph data from a ShapedWord into this textrun.
    void CopyGlyphDataFrom(gfxShapedWord *aSource, uint32_t aStart);

    // Copy glyph data for a range of characters from aSource to this
    // textrun.
    void CopyGlyphDataFrom(gfxTextRun *aSource, uint32_t aStart,
                           uint32_t aLength, uint32_t aDest);

    nsExpirationState *GetExpirationState() { return &mExpirationState; }

    // Tell the textrun to release its reference to its creating gfxFontGroup
    // immediately, rather than on destruction. This is used for textruns
    // that are actually owned by a gfxFontGroup, so that they don't keep it
    // permanently alive due to a circular reference. (The caller of this is
    // taking responsibility for ensuring the textrun will not outlive its
    // mFontGroup.)
    void ReleaseFontGroup();

    struct LigatureData {
        // textrun offsets of the start and end of the containing ligature
        uint32_t mLigatureStart;
        uint32_t mLigatureEnd;
        // appunits advance to the start of the ligature part within the ligature;
        // never includes any spacing
        gfxFloat mPartAdvance;
        // appunits width of the ligature part; includes before-spacing
        // when the part is at the start of the ligature, and after-spacing
        // when the part is as the end of the ligature
        gfxFloat mPartWidth;
        
        bool mClipBeforePart;
        bool mClipAfterPart;
    };
    
    // return storage used by this run, for memory reporter;
    // nsTransformedTextRun needs to override this as it holds additional data
    virtual size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf)
      MOZ_MUST_OVERRIDE;
    virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
      MOZ_MUST_OVERRIDE;

    // Get the size, if it hasn't already been gotten, marking as it goes.
    size_t MaybeSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)  {
        if (mFlags & gfxTextRunFactory::TEXT_RUN_SIZE_ACCOUNTED) {
            return 0;
        }
        mFlags |= gfxTextRunFactory::TEXT_RUN_SIZE_ACCOUNTED;
        return SizeOfIncludingThis(aMallocSizeOf);
    }
    void ResetSizeOfAccountingFlags() {
        mFlags &= ~gfxTextRunFactory::TEXT_RUN_SIZE_ACCOUNTED;
    }

#ifdef DEBUG
    void Dump(FILE* aOutput);
#endif

protected:
    /**
     * Create a textrun, and set its mCharacterGlyphs to point immediately
     * after the base object; this is ONLY used in conjunction with placement
     * new, after allocating a block large enough for the glyph records to
     * follow the base textrun object.
     */
    gfxTextRun(const gfxTextRunFactory::Parameters *aParams,
               uint32_t aLength, gfxFontGroup *aFontGroup, uint32_t aFlags);

    /**
     * Helper for the Create() factory method to allocate the required
     * glyph storage for a textrun object with the basic size aSize,
     * plus room for aLength glyph records.
     */
    static void* AllocateStorageForTextRun(size_t aSize, uint32_t aLength);

    // Pointer to the array of CompressedGlyph records; must be initialized
    // when the object is constructed.
    CompressedGlyph *mCharacterGlyphs;

private:
    // **** general helpers **** 

    // Allocate aCount DetailedGlyphs for the given index
    DetailedGlyph *AllocateDetailedGlyphs(uint32_t aCharIndex, uint32_t aCount);

    // Get the total advance for a range of glyphs.
    int32_t GetAdvanceForGlyphs(uint32_t aStart, uint32_t aEnd);

    // Spacing for characters outside the range aSpacingStart/aSpacingEnd
    // is assumed to be zero; such characters are not passed to aProvider.
    // This is useful to protect aProvider from being passed character indices
    // it is not currently able to handle.
    bool GetAdjustedSpacingArray(uint32_t aStart, uint32_t aEnd,
                                   PropertyProvider *aProvider,
                                   uint32_t aSpacingStart, uint32_t aSpacingEnd,
                                   nsTArray<PropertyProvider::Spacing> *aSpacing);

    //  **** ligature helpers ****
    // (Platforms do the actual ligaturization, but we need to do a bunch of stuff
    // to handle requests that begin or end inside a ligature)

    // if aProvider is null then mBeforeSpacing and mAfterSpacing are set to zero
    LigatureData ComputeLigatureData(uint32_t aPartStart, uint32_t aPartEnd,
                                     PropertyProvider *aProvider);
    gfxFloat ComputePartialLigatureWidth(uint32_t aPartStart, uint32_t aPartEnd,
                                         PropertyProvider *aProvider);
    void DrawPartialLigature(gfxFont *aFont, gfxContext *aCtx,
                             uint32_t aStart, uint32_t aEnd, gfxPoint *aPt,
                             PropertyProvider *aProvider,
                             gfxTextRunDrawCallbacks *aCallbacks);
    // Advance aStart to the start of the nearest ligature; back up aEnd
    // to the nearest ligature end; may result in *aStart == *aEnd
    void ShrinkToLigatureBoundaries(uint32_t *aStart, uint32_t *aEnd);
    // result in appunits
    gfxFloat GetPartialLigatureWidth(uint32_t aStart, uint32_t aEnd, PropertyProvider *aProvider);
    void AccumulatePartialLigatureMetrics(gfxFont *aFont,
                                          uint32_t aStart, uint32_t aEnd,
                                          gfxFont::BoundingBoxType aBoundingBoxType,
                                          gfxContext *aRefContext,
                                          PropertyProvider *aProvider,
                                          Metrics *aMetrics);

    // **** measurement helper ****
    void AccumulateMetricsForRun(gfxFont *aFont, uint32_t aStart, uint32_t aEnd,
                                 gfxFont::BoundingBoxType aBoundingBoxType,
                                 gfxContext *aRefContext,
                                 PropertyProvider *aProvider,
                                 uint32_t aSpacingStart, uint32_t aSpacingEnd,
                                 Metrics *aMetrics);

    // **** drawing helper ****
    void DrawGlyphs(gfxFont *aFont, gfxContext *aContext,
                    DrawMode aDrawMode, gfxPoint *aPt,
                    gfxTextContextPaint *aContextPaint, uint32_t aStart,
                    uint32_t aEnd, PropertyProvider *aProvider,
                    uint32_t aSpacingStart, uint32_t aSpacingEnd,
                    gfxTextRunDrawCallbacks *aCallbacks);

    // XXX this should be changed to a GlyphRun plus a maybe-null GlyphRun*,
    // for smaller size especially in the super-common one-glyphrun case
    nsAutoTArray<GlyphRun,1>        mGlyphRuns;

    void             *mUserData;
    gfxFontGroup     *mFontGroup; // addrefed on creation, but our reference
                                  // may be released by ReleaseFontGroup()
    gfxSkipChars      mSkipChars;
    nsExpirationState mExpirationState;

    bool              mSkipDrawing; // true if the font group we used had a user font
                                    // download that's in progress, so we should hide text
                                    // until the download completes (or timeout fires)
    bool              mReleasedFontGroup; // we already called NS_RELEASE on
                                          // mFontGroup, so don't do it again
};

class gfxFontGroup : public gfxTextRunFactory {
public:
    class FamilyFace {
    public:
        FamilyFace() { }

        FamilyFace(gfxFontFamily* aFamily, gfxFont* aFont)
            : mFamily(aFamily), mFont(aFont)
        {
            NS_ASSERTION(aFont, "font pointer must not be null");
            NS_ASSERTION(!aFamily ||
                         aFamily->ContainsFace(aFont->GetFontEntry()),
                         "font is not a member of the given family");
        }

        gfxFontFamily* Family() const { return mFamily.get(); }
        gfxFont* Font() const { return mFont.get(); }

    private:
        nsRefPtr<gfxFontFamily> mFamily;
        nsRefPtr<gfxFont>       mFont;
    };

    static void Shutdown(); // platform must call this to release the languageAtomService

    gfxFontGroup(const nsAString& aFamilies, const gfxFontStyle *aStyle, gfxUserFontSet *aUserFontSet = nullptr);

    virtual ~gfxFontGroup();

    virtual gfxFont *GetFontAt(int32_t i) {
        // If it turns out to be hard for all clients that cache font
        // groups to call UpdateFontList at appropriate times, we could
        // instead consider just calling UpdateFontList from someplace
        // more central (such as here).
        NS_ASSERTION(!mUserFontSet || mCurrGeneration == GetGeneration(),
                     "Whoever was caching this font group should have "
                     "called UpdateFontList on it");
        NS_ASSERTION(mFonts.Length() > uint32_t(i) && mFonts[i].Font(), 
                     "Requesting a font index that doesn't exist");

        return mFonts[i].Font();
    }

    uint32_t FontListLength() const {
        return mFonts.Length();
    }

    bool Equals(const gfxFontGroup& other) const {
        return mFamilies.Equals(other.mFamilies) &&
            mStyle.Equals(other.mStyle);
    }

    const gfxFontStyle *GetStyle() const { return &mStyle; }

    virtual gfxFontGroup *Copy(const gfxFontStyle *aStyle);

    /**
     * The listed characters should be treated as invisible and zero-width
     * when creating textruns.
     */
    static bool IsInvalidChar(uint8_t ch);
    static bool IsInvalidChar(char16_t ch);

    /**
     * Make a textrun for a given string.
     * If aText is not persistent (aFlags & TEXT_IS_PERSISTENT), the
     * textrun will copy it.
     * This calls FetchGlyphExtents on the textrun.
     */
    virtual gfxTextRun *MakeTextRun(const char16_t *aString, uint32_t aLength,
                                    const Parameters *aParams, uint32_t aFlags);
    /**
     * Make a textrun for a given string.
     * If aText is not persistent (aFlags & TEXT_IS_PERSISTENT), the
     * textrun will copy it.
     * This calls FetchGlyphExtents on the textrun.
     */
    virtual gfxTextRun *MakeTextRun(const uint8_t *aString, uint32_t aLength,
                                    const Parameters *aParams, uint32_t aFlags);

    /**
     * Textrun creation helper for clients that don't want to pass
     * a full Parameters record.
     */
    template<typename T>
    gfxTextRun *MakeTextRun(const T *aString, uint32_t aLength,
                            gfxContext *aRefContext,
                            int32_t aAppUnitsPerDevUnit,
                            uint32_t aFlags)
    {
        gfxTextRunFactory::Parameters params = {
            aRefContext, nullptr, nullptr, nullptr, 0, aAppUnitsPerDevUnit
        };
        return MakeTextRun(aString, aLength, &params, aFlags);
    }

    /**
     * Get the (possibly-cached) width of the hyphen character.
     * The aCtx and aAppUnitsPerDevUnit parameters will be used only if
     * needed to initialize the cached hyphen width; otherwise they are
     * ignored.
     */
    gfxFloat GetHyphenWidth(gfxContext *aCtx, uint32_t aAppUnitsPerDevUnit);

    /**
     * Make a text run representing a single hyphen character.
     * This will use U+2010 HYPHEN if available in the first font,
     * otherwise fall back to U+002D HYPHEN-MINUS.
     * The caller is responsible for deleting the returned text run
     * when no longer required.
     */
    gfxTextRun *MakeHyphenTextRun(gfxContext *aCtx,
                                  uint32_t aAppUnitsPerDevUnit);

    /* helper function for splitting font families on commas and
     * calling a function for each family to fill the mFonts array
     */
    typedef bool (*FontCreationCallback) (const nsAString& aName,
                                            const nsACString& aGenericName,
                                            bool aUseFontSet,
                                            void *closure);
    bool ForEachFont(const nsAString& aFamilies,
                       nsIAtom *aLanguage,
                       FontCreationCallback fc,
                       void *closure);
    bool ForEachFont(FontCreationCallback fc, void *closure);

    /**
     * Check whether a given font (specified by its gfxFontEntry)
     * is already in the fontgroup's list of actual fonts
     */
    bool HasFont(const gfxFontEntry *aFontEntry);

    const nsString& GetFamilies() { return mFamilies; }

    // This returns the preferred underline for this font group.
    // Some CJK fonts have wrong underline offset in its metrics.
    // If this group has such "bad" font, each platform's gfxFontGroup initialized mUnderlineOffset.
    // The value should be lower value of first font's metrics and the bad font's metrics.
    // Otherwise, this returns from first font's metrics.
    enum { UNDERLINE_OFFSET_NOT_SET = INT16_MAX };
    virtual gfxFloat GetUnderlineOffset() {
        if (mUnderlineOffset == UNDERLINE_OFFSET_NOT_SET)
            mUnderlineOffset = GetFontAt(0)->GetMetrics().underlineOffset;
        return mUnderlineOffset;
    }

    virtual already_AddRefed<gfxFont>
        FindFontForChar(uint32_t ch, uint32_t prevCh, int32_t aRunScript,
                        gfxFont *aPrevMatchedFont,
                        uint8_t *aMatchType);

    // search through pref fonts for a character, return nullptr if no matching pref font
    virtual already_AddRefed<gfxFont> WhichPrefFontSupportsChar(uint32_t aCh);

    virtual already_AddRefed<gfxFont>
        WhichSystemFontSupportsChar(uint32_t aCh, int32_t aRunScript);

    template<typename T>
    void ComputeRanges(nsTArray<gfxTextRange>& mRanges,
                       const T *aString, uint32_t aLength,
                       int32_t aRunScript);

    gfxUserFontSet* GetUserFontSet();

    // With downloadable fonts, the composition of the font group can change as fonts are downloaded
    // for each change in state of the user font set, the generation value is bumped to avoid picking up
    // previously created text runs in the text run word cache.  For font groups based on stylesheets
    // with no @font-face rule, this always returns 0.
    uint64_t GetGeneration();

    // used when logging text performance
    gfxTextPerfMetrics *GetTextPerfMetrics() { return mTextPerf; }
    void SetTextPerfMetrics(gfxTextPerfMetrics *aTextPerf) { mTextPerf = aTextPerf; }

    // This will call UpdateFontList() if the user font set is changed.
    void SetUserFontSet(gfxUserFontSet *aUserFontSet);

    // If there is a user font set, check to see whether the font list or any
    // caches need updating.
    virtual void UpdateFontList();

    bool ShouldSkipDrawing() const {
        return mSkipDrawing;
    }

    class LazyReferenceContextGetter {
    public:
      virtual already_AddRefed<gfxContext> GetRefContext() = 0;
    };
    // The gfxFontGroup keeps ownership of this textrun.
    // It is only guaranteed to exist until the next call to GetEllipsisTextRun
    // (which might use a different appUnitsPerDev value) for the font group,
    // or until UpdateFontList is called, or the fontgroup is destroyed.
    // Get it/use it/forget it :) - don't keep a reference that might go stale.
    gfxTextRun* GetEllipsisTextRun(int32_t aAppUnitsPerDevPixel,
                                   LazyReferenceContextGetter& aRefContextGetter);

protected:
    nsString mFamilies;
    gfxFontStyle mStyle;
    nsTArray<FamilyFace> mFonts;
    gfxFloat mUnderlineOffset;
    gfxFloat mHyphenWidth;

    nsRefPtr<gfxUserFontSet> mUserFontSet;
    uint64_t mCurrGeneration;  // track the current user font set generation, rebuild font list if needed

    gfxTextPerfMetrics *mTextPerf;

    // Cache a textrun representing an ellipsis (useful for CSS text-overflow)
    // at a specific appUnitsPerDevPixel size
    nsAutoPtr<gfxTextRun>   mCachedEllipsisTextRun;

    // cache the most recent pref font to avoid general pref font lookup
    nsRefPtr<gfxFontFamily> mLastPrefFamily;
    nsRefPtr<gfxFont>       mLastPrefFont;
    eFontPrefLang           mLastPrefLang;       // lang group for last pref font
    eFontPrefLang           mPageLang;
    bool                    mLastPrefFirstFont;  // is this the first font in the list of pref fonts for this lang group?

    bool                    mSkipDrawing; // hide text while waiting for a font
                                          // download to complete (or fallback
                                          // timer to fire)

    /**
     * Textrun creation short-cuts for special cases where we don't need to
     * call a font shaper to generate glyphs.
     */
    gfxTextRun *MakeEmptyTextRun(const Parameters *aParams, uint32_t aFlags);
    gfxTextRun *MakeSpaceTextRun(const Parameters *aParams, uint32_t aFlags);
    gfxTextRun *MakeBlankTextRun(uint32_t aLength,
                                 const Parameters *aParams, uint32_t aFlags);

    // Initialize the list of fonts
    void BuildFontList();

    // Init this font group's font metrics. If there no bad fonts, you don't need to call this.
    // But if there are one or more bad fonts which have bad underline offset,
    // you should call this with the *first* bad font.
    void InitMetricsForBadFont(gfxFont* aBadFont);

    // Set up the textrun glyphs for an entire text run:
    // find script runs, and then call InitScriptRun for each
    template<typename T>
    void InitTextRun(gfxContext *aContext,
                     gfxTextRun *aTextRun,
                     const T *aString,
                     uint32_t aLength);

    // InitTextRun helper to handle a single script run, by finding font ranges
    // and calling each font's InitTextRun() as appropriate
    template<typename T>
    void InitScriptRun(gfxContext *aContext,
                       gfxTextRun *aTextRun,
                       const T *aString,
                       uint32_t aScriptRunStart,
                       uint32_t aScriptRunEnd,
                       int32_t aRunScript);

    /* If aResolveGeneric is true, then CSS/Gecko generic family names are
     * replaced with preferred fonts.
     *
     * If aResolveFontName is true then fc() is called only for existing fonts
     * and with actual font names.  If false then fc() is called with each
     * family name in aFamilies (after resolving CSS/Gecko generic family names
     * if aResolveGeneric).
     * If aUseFontSet is true, the fontgroup's user font set is checked;
     * if false then it is skipped.
     */
    bool ForEachFontInternal(const nsAString& aFamilies,
                               nsIAtom *aLanguage,
                               bool aResolveGeneric,
                               bool aResolveFontName,
                               bool aUseFontSet,
                               FontCreationCallback fc,
                               void *closure);

    // Helper for font-matching:
    // see if aCh is supported in any of the faces from aFamily;
    // if so return the best style match, else return null.
    already_AddRefed<gfxFont> TryAllFamilyMembers(gfxFontFamily* aFamily,
                                                  uint32_t aCh);

    static bool FontResolverProc(const nsAString& aName, void *aClosure);

    static bool FindPlatformFont(const nsAString& aName,
                                   const nsACString& aGenericName,
                                   bool aUseFontSet,
                                   void *closure);

    static NS_HIDDEN_(nsILanguageAtomService*) gLangService;
};
#endif

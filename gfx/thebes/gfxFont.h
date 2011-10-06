/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Foundation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2005-2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart@mozilla.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   John Daggett <jdaggett@mozilla.com>
 *   Jonathan Kew <jfkthame@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef GFX_FONT_H
#define GFX_FONT_H

#include "prtypes.h"
#include "nsAlgorithm.h"
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
#include "gfxFontConstants.h"
#include "gfxPlatform.h"
#include "nsIAtom.h"
#include "nsISupportsImpl.h"

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

class nsILanguageAtomService;

typedef struct _hb_blob_t hb_blob_t;

// We should eliminate these synonyms when it won't cause many merge conflicts.
#define FONT_STYLE_NORMAL              NS_FONT_STYLE_NORMAL
#define FONT_STYLE_ITALIC              NS_FONT_STYLE_ITALIC
#define FONT_STYLE_OBLIQUE             NS_FONT_STYLE_OBLIQUE

// We should eliminate these synonyms when it won't cause many merge conflicts.
#define FONT_WEIGHT_NORMAL             NS_FONT_WEIGHT_NORMAL
#define FONT_WEIGHT_BOLD               NS_FONT_WEIGHT_BOLD

#define FONT_MAX_SIZE                  2000.0

#define NO_FONT_LANGUAGE_OVERRIDE      0

// An OpenType feature tag and value pair
struct THEBES_API gfxFontFeature {
    PRUint32 mTag; // see http://www.microsoft.com/typography/otspec/featuretags.htm
    PRUint32 mValue; // 0 = off, 1 = on, larger values may be used as parameters
                     // to features that select among multiple alternatives
};

inline bool
operator<(const gfxFontFeature& a, const gfxFontFeature& b)
{
    return (a.mTag < b.mTag) || ((a.mTag == b.mTag) && (a.mValue < b.mValue));
}

inline bool
operator==(const gfxFontFeature& a, const gfxFontFeature& b)
{
    return (a.mTag == b.mTag) && (a.mValue == b.mValue);
}


struct THEBES_API gfxFontStyle {
    gfxFontStyle();
    gfxFontStyle(PRUint8 aStyle, PRUint16 aWeight, PRInt16 aStretch,
                 gfxFloat aSize, nsIAtom *aLanguage,
                 float aSizeAdjust, bool aSystemFont,
                 bool aPrinterFont,
                 const nsString& aFeatureSettings,
                 const nsString& aLanguageOverride);
    gfxFontStyle(const gfxFontStyle& aStyle);

    // The style of font (normal, italic, oblique)
    PRUint8 style : 7;

    // Say that this font is a system font and therefore does not
    // require certain fixup that we do for fonts from untrusted
    // sources.
    bool systemFont : 1;

    // Say that this font is used for print or print preview.
    bool printerFont : 1;

    // The weight of the font: 100, 200, ... 900.
    PRUint16 weight;

    // The stretch of the font (the sum of various NS_FONT_STRETCH_*
    // constants; see gfxFontConstants.h).
    PRInt16 stretch;

    // The logical size of the font, in pixels
    gfxFloat size;

    // The aspect-value (ie., the ratio actualsize:actualxheight) that any
    // actual physical font created from this font structure must have when
    // rendering or measuring a string. A value of 0 means no adjustment
    // needs to be done.
    float sizeAdjust;

    // the language (may be an internal langGroup code rather than an actual
    // language code) specified in the document or element's lang property,
    // or inferred from the charset
    nsRefPtr<nsIAtom> language;

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
    PRUint32 languageOverride;

    // custom opentype feature settings
    nsTArray<gfxFontFeature> featureSettings;

    // Return the final adjusted font size for the given aspect ratio.
    // Not meant to be called when sizeAdjust = 0.
    gfxFloat GetAdjustedSize(gfxFloat aspect) const {
        NS_ASSERTION(sizeAdjust != 0.0, "Not meant to be called when sizeAdjust = 0");
        gfxFloat adjustedSize = NS_MAX(NS_round(size*(sizeAdjust/aspect)), 1.0);
        return NS_MIN(adjustedSize, FONT_MAX_SIZE);
    }

    PLDHashNumber Hash() const {
        return ((style + (systemFont << 7) +
            (weight << 8)) + PRUint32(size*1000) + PRUint32(sizeAdjust*1000)) ^
            nsISupportsHashKey::HashKey(language);
    }

    PRInt8 ComputeWeight() const;

    bool Equals(const gfxFontStyle& other) const {
        return (size == other.size) &&
            (style == other.style) &&
            (systemFont == other.systemFont) &&
            (printerFont == other.printerFont) &&
            (weight == other.weight) &&
            (stretch == other.stretch) &&
            (language == other.language) &&
            (sizeAdjust == other.sizeAdjust) &&
            (featureSettings == other.featureSettings) &&
            (languageOverride == other.languageOverride);
    }

    static void ParseFontFeatureSettings(const nsString& aFeatureString,
                                         nsTArray<gfxFontFeature>& aFeatures);

    static PRUint32 ParseFontLanguageOverride(const nsString& aLangTag);
};

class gfxFontEntry {
public:
    NS_INLINE_DECL_REFCOUNTING(gfxFontEntry)

    gfxFontEntry(const nsAString& aName, gfxFontFamily *aFamily = nsnull,
                 bool aIsStandardFace = false) : 
        mName(aName), mItalic(PR_FALSE), mFixedPitch(PR_FALSE),
        mIsProxy(PR_FALSE), mIsValid(PR_TRUE), 
        mIsBadUnderlineFont(PR_FALSE), mIsUserFont(PR_FALSE),
        mIsLocalUserFont(PR_FALSE), mStandardFace(aIsStandardFace),
        mSymbolFont(PR_FALSE),
        mIgnoreGDEF(PR_FALSE),
        mWeight(500), mStretch(NS_FONT_STRETCH_NORMAL),
        mHasCmapTable(PR_FALSE),
        mCmapInitialized(PR_FALSE),
        mUVSOffset(0), mUVSData(nsnull),
        mUserFontData(nsnull),
        mLanguageOverride(NO_FONT_LANGUAGE_OVERRIDE),
        mFamily(aFamily)
    { }

    virtual ~gfxFontEntry();

    // unique name for the face, *not* the family; not necessarily the
    // "real" or user-friendly name, may be an internal identifier
    const nsString& Name() const { return mName; }

    // the "real" name of the face, if available from the font resource
    // (may be expensive); returns Name() if nothing better is available
    virtual nsString RealFaceName();

    gfxFontFamily* Family() const { return mFamily; }

    PRUint16 Weight() const { return mWeight; }
    PRInt16 Stretch() const { return mStretch; }

    bool IsUserFont() const { return mIsUserFont; }
    bool IsLocalUserFont() const { return mIsLocalUserFont; }
    bool IsFixedPitch() const { return mFixedPitch; }
    bool IsItalic() const { return mItalic; }
    bool IsBold() const { return mWeight >= 600; } // bold == weights 600 and above
    bool IgnoreGDEF() const { return mIgnoreGDEF; }

    virtual bool IsSymbolFont();

    inline bool HasCmapTable() {
        if (!mCmapInitialized) {
            ReadCMAP();
        }
        return mHasCmapTable;
    }

    inline bool HasCharacter(PRUint32 ch) {
        if (mCharacterMap.test(ch))
            return PR_TRUE;

        return TestCharacterMap(ch);
    }

    virtual bool SkipDuringSystemFallback() { return false; }
    virtual bool TestCharacterMap(PRUint32 aCh);
    nsresult InitializeUVSMap();
    PRUint16 GetUVSGlyph(PRUint32 aCh, PRUint32 aVS);
    virtual nsresult ReadCMAP();

    virtual bool MatchesGenericFamily(const nsACString& aGeneric) const {
        return PR_TRUE;
    }
    virtual bool SupportsLangGroup(nsIAtom *aLangGroup) const {
        return PR_TRUE;
    }

    virtual nsresult GetFontTable(PRUint32 aTableTag, FallibleTArray<PRUint8>& aBuffer) {
        return NS_ERROR_FAILURE; // all platform subclasses should reimplement this!
    }

    void SetFamily(gfxFontFamily* aFamily) {
        mFamily = aFamily;
    }

    virtual nsString FamilyName() const;

    already_AddRefed<gfxFont> FindOrMakeFont(const gfxFontStyle *aStyle,
                                             bool aNeedsBold);

    // Get an existing font table cache entry in aBlob if it has been
    // registered, or return PR_FALSE if not.  Callers must call
    // hb_blob_destroy on aBlob if PR_TRUE is returned.
    //
    // Note that some gfxFont implementations may not call this at all,
    // if it is more efficient to get the table from the OS at that level.
    bool GetExistingFontTable(PRUint32 aTag, hb_blob_t** aBlob);

    // Elements of aTable are transferred (not copied) to and returned in a
    // new hb_blob_t which is registered on the gfxFontEntry, but the initial
    // reference is owned by the caller.  Removing the last reference
    // unregisters the table from the font entry.
    //
    // Pass NULL for aBuffer to indicate that the table is not present and
    // NULL will be returned.  Also returns NULL on OOM.
    hb_blob_t *ShareFontTableAndGetBlob(PRUint32 aTag,
                                        FallibleTArray<PRUint8>* aTable);

    nsString         mName;

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

    PRUint16         mWeight;
    PRInt16          mStretch;

    bool             mHasCmapTable;
    bool             mCmapInitialized;
    gfxSparseBitSet  mCharacterMap;
    PRUint32         mUVSOffset;
    nsAutoArrayPtr<PRUint8> mUVSData;
    gfxUserFontData* mUserFontData;

    nsTArray<gfxFontFeature> mFeatureSettings;
    PRUint32         mLanguageOverride;

protected:
    friend class gfxPlatformFontList;
    friend class gfxMacPlatformFontList;
    friend class gfxUserFcFontEntry;
    friend class gfxFontFamily;
    friend class gfxSingleFaceMacFontFamily;

    gfxFontEntry() :
        mItalic(PR_FALSE), mFixedPitch(PR_FALSE),
        mIsProxy(PR_FALSE), mIsValid(PR_TRUE), 
        mIsBadUnderlineFont(PR_FALSE),
        mIsUserFont(PR_FALSE),
        mIsLocalUserFont(PR_FALSE),
        mStandardFace(PR_FALSE),
        mSymbolFont(PR_FALSE),
        mIgnoreGDEF(PR_FALSE),
        mWeight(500), mStretch(NS_FONT_STRETCH_NORMAL),
        mHasCmapTable(PR_FALSE),
        mCmapInitialized(PR_FALSE),
        mUVSOffset(0), mUVSData(nsnull),
        mUserFontData(nsnull),
        mLanguageOverride(NO_FONT_LANGUAGE_OVERRIDE),
        mFamily(nsnull)
    { }

    virtual gfxFont *CreateFontInstance(const gfxFontStyle *aFontStyle, bool aNeedsBold) {
        NS_NOTREACHED("oops, somebody didn't override CreateFontInstance");
        return nsnull;
    }

    gfxFontFamily *mFamily;

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
     * FontTableHashEntry manages the entries of hb_blob_ts for two
     * different situations:
     *
     * The common situation is to share font table across fonts with the same
     * font entry (but different sizes) for use by HarfBuzz.  The hashtable
     * does not own a strong reference to the blob, but keeps a weak pointer,
     * managed by FontTableBlobData.  Similarly FontTableBlobData keeps only a
     * weak pointer to the hashtable, managed by FontTableHashEntry.
     *
     * Some font tables are saved here before they would get stripped by OTS
     * sanitizing.  These are retained for harfbuzz, which does its own
     * sanitizing.  The hashtable owns a reference, so ownership is simple.
     */

    class FontTableHashEntry : public nsUint32HashKey
    {
    public:
        // Declarations for nsTHashtable

        typedef nsUint32HashKey KeyClass;
        typedef KeyClass::KeyType KeyType;
        typedef KeyClass::KeyTypePointer KeyTypePointer;

        FontTableHashEntry(KeyTypePointer aTag)
            : KeyClass(aTag), mBlob() { };
        // Copying transfers blob association.
        FontTableHashEntry(FontTableHashEntry& toCopy)
            : KeyClass(toCopy), mBlob(toCopy.mBlob)
        {
            toCopy.mBlob = nsnull;
        }

        ~FontTableHashEntry() { Clear(); }

        // FontTable/Blob API

        // Transfer (not copy) elements of aTable to a new hb_blob_t and
        // return ownership to the caller.  A weak reference to the blob is
        // recorded in the hashtable entry so that others may use the same
        // table.
        hb_blob_t *
        ShareTableAndGetBlob(FallibleTArray<PRUint8>& aTable,
                             nsTHashtable<FontTableHashEntry> *aHashtable);

        // Transfer (not copy) elements of aTable to a new hb_blob_t that is
        // owned by the hashtable entry.
        void SaveTable(FallibleTArray<PRUint8>& aTable);

        // Return a strong reference to the blob.
        // Callers must hb_blob_destroy the returned blob.
        hb_blob_t *GetBlob() const;

        void Clear();

    private:
        static void DeleteFontTableBlobData(void *aBlobData);
        // not implemented
        FontTableHashEntry& operator=(FontTableHashEntry& toCopy);

        FontTableBlobData *mSharedBlobData;
        hb_blob_t *mBlob;
    };

    nsTHashtable<FontTableHashEntry> mFontTableCache;

    gfxFontEntry(const gfxFontEntry&);
    gfxFontEntry& operator=(const gfxFontEntry&);
};


// used when picking fallback font
struct FontSearch {
    FontSearch(const PRUint32 aCharacter, gfxFont *aFont) :
        mCh(aCharacter), mFontToMatch(aFont), mMatchRank(0), mCount(0) {
    }
    const PRUint32         mCh;
    gfxFont*               mFontToMatch;
    PRInt32                mMatchRank;
    nsRefPtr<gfxFontEntry> mBestMatch;
    PRUint32               mCount;
};

class gfxFontFamily {
public:
    NS_INLINE_DECL_REFCOUNTING(gfxFontFamily)

    gfxFontFamily(const nsAString& aName) :
        mName(aName),
        mOtherFamilyNamesInitialized(PR_FALSE),
        mHasOtherFamilyNames(PR_FALSE),
        mFaceNamesInitialized(PR_FALSE),
        mHasStyles(PR_FALSE),
        mIsSimpleFamily(PR_FALSE),
        mIsBadUnderlineFamily(PR_FALSE)
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
            aFontEntry->mIgnoreGDEF = PR_TRUE;
        }
        mAvailableFonts.AppendElement(aFontEntry);
        aFontEntry->SetFamily(this);
    }

    // note that the styles for this family have been added
    void SetHasStyles(bool aHasStyles) { mHasStyles = aHasStyles; }

    // choose a specific face to match a style using CSS font matching
    // rules (weight matching occurs here).  may return a face that doesn't
    // precisely match (e.g. normal face when no italic face exists).
    // aNeedsSyntheticBold is set to true when synthetic bolding is
    // needed, false otherwise
    gfxFontEntry *FindFontForStyle(const gfxFontStyle& aFontStyle, 
                                   bool& aNeedsSyntheticBold);

    // iterates over faces looking for a match with a given characters
    // used as part of the font fallback process
    void FindFontForChar(FontSearch *aMatchData);

    // read in other family names, if any, and use functor to add each into cache
    virtual void ReadOtherFamilyNames(gfxPlatformFontList *aPlatformFontList);

    // set when other family names have been read in
    void SetOtherFamilyNamesInitialized() {
        mOtherFamilyNamesInitialized = PR_TRUE;
    }

    // read in other localized family names, fullnames and Postscript names
    // for all faces and append to lookup tables
    virtual void ReadFaceNames(gfxPlatformFontList *aPlatformFontList,
                               bool aNeedFullnamePostscriptNames);

    // find faces belonging to this family (platform implementations override this;
    // should be made pure virtual once all subclasses have been updated)
    virtual void FindStyleVariations() { }

    // search for a specific face using the Postscript name
    gfxFontEntry* FindFont(const nsAString& aPostscriptName);

    // read in cmaps for all the faces
    void ReadCMAP() {
        PRUint32 i, numFonts = mAvailableFonts.Length();
        // called from RunLoader BEFORE CheckForSimpleFamily so that there cannot
        // be any NULL entries in mAvailableFonts
        for (i = 0; i < numFonts; i++)
            mAvailableFonts[i]->ReadCMAP();
    }

    // mark this family as being in the "bad" underline offset blacklist
    void SetBadUnderlineFamily() {
        mIsBadUnderlineFamily = PR_TRUE;
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

protected:
    // fills in an array with weights of faces that match style,
    // returns whether any matching entries found
    virtual bool FindWeightsForStyle(gfxFontEntry* aFontsForWeights[],
                                       bool anItalic, PRInt16 aStretch);

    bool ReadOtherFamilyNamesForFace(gfxPlatformFontList *aPlatformFontList,
                                       FallibleTArray<PRUint8>& aNameTable,
                                       bool useFullName = false);

    // set whether this font family is in "bad" underline offset blacklist.
    void SetBadUnderlineFonts() {
        PRUint32 i, numFonts = mAvailableFonts.Length();
        for (i = 0; i < numFonts; i++) {
            if (mAvailableFonts[i]) {
                mAvailableFonts[i]->mIsBadUnderlineFont = PR_TRUE;
            }
        }
    }

    nsString mName;
    nsTArray<nsRefPtr<gfxFontEntry> >  mAvailableFonts;
    bool mOtherFamilyNamesInitialized;
    bool mHasOtherFamilyNames;
    bool mFaceNamesInitialized;
    bool mHasStyles;
    bool mIsSimpleFamily;
    bool mIsBadUnderlineFamily;

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
    gfxTextRange(PRUint32 aStart, PRUint32 aEnd,
                 gfxFont* aFont, PRUint8 aMatchType)
        : start(aStart),
          end(aEnd),
          font(aFont),
          matchType(aMatchType)
    { }
    PRUint32 Length() const { return end - start; }
    PRUint32 start, end;
    nsRefPtr<gfxFont> font;
    PRUint8 matchType;
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
 */
class THEBES_API gfxFontCache : public nsExpirationTracker<gfxFont,3> {
public:
    enum { TIMEOUT_SECONDS = 10 };
    gfxFontCache()
        : nsExpirationTracker<gfxFont,3>(TIMEOUT_SECONDS*1000) { mFonts.Init(); }
    ~gfxFontCache() {
        // Expire everything that has a zero refcount, so we don't leak them.
        AgeAllGenerations();
        // All fonts should be gone.
        NS_WARN_IF_FALSE(mFonts.Count() == 0,
                         "Fonts still alive while shutting down gfxFontCache");
        // Note that we have to delete everything through the expiration
        // tracker, since there might be fonts not in the hashtable but in
        // the tracker.
    }

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
                                     const gfxFontStyle *aFontGroup);
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

protected:
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
        HashEntry(KeyTypePointer aStr) : mFont(nsnull) { }
        HashEntry(const HashEntry& toCopy) : mFont(toCopy.mFont) { }
        ~HashEntry() { }

        bool KeyEquals(const KeyTypePointer aKey) const;
        static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
        static PLDHashNumber HashKey(const KeyTypePointer aKey) {
            return NS_PTR_TO_INT32(aKey->mFontEntry) ^ aKey->mStyle->Hash();
        }
        enum { ALLOW_MEMMOVE = PR_TRUE };

        gfxFont* mFont;
    };

    nsTHashtable<HashEntry> mFonts;
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
class THEBES_API gfxGlyphExtents {
public:
    gfxGlyphExtents(PRUint32 aAppUnitsPerDevUnit) :
        mAppUnitsPerDevUnit(aAppUnitsPerDevUnit) {
        MOZ_COUNT_CTOR(gfxGlyphExtents);
        mTightGlyphExtents.Init();
    }
    ~gfxGlyphExtents();

    enum { INVALID_WIDTH = 0xFFFF };

    // returns INVALID_WIDTH => not a contained glyph
    // Otherwise the glyph has no before-bearing or vertical bearings,
    // and the result is its width measured from the baseline origin, in
    // appunits.
    PRUint16 GetContainedGlyphWidthAppUnits(PRUint32 aGlyphID) const {
        return mContainedGlyphWidths.Get(aGlyphID);
    }

    bool IsGlyphKnown(PRUint32 aGlyphID) const {
        return mContainedGlyphWidths.Get(aGlyphID) != INVALID_WIDTH ||
            mTightGlyphExtents.GetEntry(aGlyphID) != nsnull;
    }

    bool IsGlyphKnownWithTightExtents(PRUint32 aGlyphID) const {
        return mTightGlyphExtents.GetEntry(aGlyphID) != nsnull;
    }

    // Get glyph extents; a rectangle relative to the left baseline origin
    // Returns true on success. Can fail on OOM or when aContext is null
    // and extents were not (successfully) prefetched.
    bool GetTightGlyphExtentsAppUnits(gfxFont *aFont, gfxContext *aContext,
            PRUint32 aGlyphID, gfxRect *aExtents);

    void SetContainedGlyphWidthAppUnits(PRUint32 aGlyphID, PRUint16 aWidth) {
        mContainedGlyphWidths.Set(aGlyphID, aWidth);
    }
    void SetTightGlyphExtents(PRUint32 aGlyphID, const gfxRect& aExtentsAppUnits);

    PRUint32 GetAppUnitsPerDevUnit() { return mAppUnitsPerDevUnit; }

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

    typedef PRUptrdiff PtrBits;
    enum { BLOCK_SIZE_BITS = 7, BLOCK_SIZE = 1 << BLOCK_SIZE_BITS }; // 128-glyph blocks

    class GlyphWidths {
    public:
        void Set(PRUint32 aIndex, PRUint16 aValue);
        PRUint16 Get(PRUint32 aIndex) const {
            PRUint32 block = aIndex >> BLOCK_SIZE_BITS;
            if (block >= mBlocks.Length())
                return INVALID_WIDTH;
            PtrBits bits = mBlocks[block];
            if (!bits)
                return INVALID_WIDTH;
            PRUint32 indexInBlock = aIndex & (BLOCK_SIZE - 1);
            if (bits & 0x1) {
                if (GetGlyphOffset(bits) != indexInBlock)
                    return INVALID_WIDTH;
                return GetWidth(bits);
            }
            PRUint16 *widths = reinterpret_cast<PRUint16 *>(bits);
            return widths[indexInBlock];
        }

#ifdef DEBUG
        PRUint32 ComputeSize();
#endif
        
        ~GlyphWidths();

    private:
        static PRUint32 GetGlyphOffset(PtrBits aBits) {
            NS_ASSERTION(aBits & 0x1, "This is really a pointer...");
            return (aBits >> 1) & ((1 << BLOCK_SIZE_BITS) - 1);
        }
        static PRUint32 GetWidth(PtrBits aBits) {
            NS_ASSERTION(aBits & 0x1, "This is really a pointer...");
            return aBits >> (1 + BLOCK_SIZE_BITS);
        }
        static PtrBits MakeSingle(PRUint32 aGlyphOffset, PRUint16 aWidth) {
            return (aWidth << (1 + BLOCK_SIZE_BITS)) + (aGlyphOffset << 1) + 1;
        }

        nsTArray<PtrBits> mBlocks;
    };
    
    GlyphWidths             mContainedGlyphWidths;
    nsTHashtable<HashEntry> mTightGlyphExtents;
    PRUint32                mAppUnitsPerDevUnit;
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

    virtual bool InitTextRun(gfxContext *aContext,
                               gfxTextRun *aTextRun,
                               const PRUnichar *aString,
                               PRUint32 aRunStart,
                               PRUint32 aRunLength,
                               PRInt32 aRunScript) = 0;

    gfxFont *GetFont() const { return mFont; }

protected:
    // the font this shaper is working with
    gfxFont * mFont;
};

/* a SPECIFIC single font family */
class THEBES_API gfxFont {
public:
    nsrefcnt AddRef(void) {
        NS_PRECONDITION(PRInt32(mRefCnt) >= 0, "illegal refcnt");
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

    PRInt32 GetRefCount() { return mRefCnt; }

    // options to specify the kind of AA to be used when creating a font
    typedef enum {
        kAntialiasDefault,
        kAntialiasNone,
        kAntialiasGrayscale,
        kAntialiasSubpixel
    } AntialiasOption;

protected:
    nsAutoRefCnt mRefCnt;

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
            AntialiasOption anAAOption = kAntialiasDefault);

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

    virtual gfxFont* CopyWithAntialiasOption(AntialiasOption anAAOption) {
        // platforms where this actually matters should override
        return nsnull;
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

    // Access to raw font table data (needed for Harfbuzz):
    // returns a pointer to data owned by the fontEntry or the OS,
    // which will remain valid until released.
    //
    // Default implementations forward to the font entry,
    // and maintain a shared table.
    //
    // Subclasses should override this if they can provide more efficient
    // access than getting tables with mFontEntry->GetFontTable() and sharing
    // them via the entry.
    //
    // Get pointer to a specific font table, or NULL if
    // the table doesn't exist in the font
    virtual hb_blob_t *GetFontTable(PRUint32 aTag);

    // Subclasses may choose to look up glyph ids for characters.
    // If they do not override this, gfxHarfBuzzShaper will fetch the cmap
    // table and use that.
    virtual bool ProvidesGetGlyph() const {
        return PR_FALSE;
    }
    // Map unicode character to glyph ID.
    // Only used if ProvidesGetGlyph() returns PR_TRUE.
    virtual PRUint32 GetGlyph(PRUint32 unicode, PRUint32 variation_selector) {
        return 0;
    }

    // subclasses may provide (possibly hinted) glyph widths (in font units);
    // if they do not override this, harfbuzz will use unhinted widths
    // derived from the font tables
    virtual bool ProvidesGlyphWidths() {
        return PR_FALSE;
    }

    // The return value is interpreted as a horizontal advance in 16.16 fixed
    // point format.
    virtual PRInt32 GetGlyphWidth(gfxContext *aCtx, PRUint16 aGID) {
        return -1;
    }

    gfxFloat SynthesizeSpaceWidth(PRUint32 aCh);

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
    struct THEBES_API RunMetrics {
        RunMetrics() {
            mAdvanceWidth = mAscent = mDescent = 0.0;
            mBoundingBox = gfxRect(0,0,0,0);
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
     * @param aDrawToPath when true, add the glyph outlines to the current path
     * instead of drawing the glyphs
     * 
     * Callers guarantee:
     * -- aStart and aEnd are aligned to cluster and ligature boundaries
     * -- all glyphs use this font
     * 
     * The default implementation builds a cairo glyph array and
     * calls cairo_show_glyphs or cairo_glyph_path.
     */
    virtual void Draw(gfxTextRun *aTextRun, PRUint32 aStart, PRUint32 aEnd,
                      gfxContext *aContext, bool aDrawToPath, gfxPoint *aBaselineOrigin,
                      Spacing *aSpacing);
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
                               PRUint32 aStart, PRUint32 aEnd,
                               BoundingBoxType aBoundingBoxType,
                               gfxContext *aContextForTightBoundingBox,
                               Spacing *aSpacing);
    /**
     * Line breaks have been changed at the beginning and/or end of a substring
     * of the text. Reshaping may be required; glyph updating is permitted.
     * @return true if anything was changed, false otherwise
     */
    bool NotifyLineBreaksChanged(gfxTextRun *aTextRun,
                                   PRUint32 aStart, PRUint32 aLength)
    { return PR_FALSE; }

    // Expiration tracking
    nsExpirationState *GetExpirationState() { return &mExpirationState; }

    // Get the glyphID of a space
    virtual PRUint32 GetSpaceGlyph() = 0;

    gfxGlyphExtents *GetOrCreateGlyphExtents(PRUint32 aAppUnitsPerDevUnit);

    // You need to call SetupCairoFont on the aCR just before calling this
    virtual void SetupGlyphExtents(gfxContext *aContext, PRUint32 aGlyphID,
                                   bool aNeedTight, gfxGlyphExtents *aExtents);

    // This is called by the default Draw() implementation above.
    virtual bool SetupCairoFont(gfxContext *aContext) = 0;

    bool IsSyntheticBold() { return mApplySyntheticBold; }

    // Amount by which synthetic bold "fattens" the glyphs: 1/16 of the em-size
    gfxFloat GetSyntheticBoldOffset() {
        return GetAdjustedSize() * (1.0 / 16.0);
    }

    gfxFontEntry *GetFontEntry() { return mFontEntry.get(); }
    bool HasCharacter(PRUint32 ch) {
        if (!mIsValid)
            return PR_FALSE;
        return mFontEntry->HasCharacter(ch); 
    }

    PRUint16 GetUVSGlyph(PRUint32 aCh, PRUint32 aVS) {
        if (!mIsValid) {
            return 0;
        }
        return mFontEntry->GetUVSGlyph(aCh, aVS); 
    }

    // call the (virtual) InitTextRun method to do glyph generation/shaping,
    // limiting the length of text passed by processing the run in multiple
    // segments if necessary
    bool SplitAndInitTextRun(gfxContext *aContext,
                               gfxTextRun *aTextRun,
                               const PRUnichar *aString,
                               PRUint32 aRunStart,
                               PRUint32 aRunLength,
                               PRInt32 aRunScript);

protected:
    nsRefPtr<gfxFontEntry> mFontEntry;

    bool                       mIsValid;

    // use synthetic bolding for environments where this is not supported
    // by the platform
    bool                       mApplySyntheticBold;

    nsExpirationState          mExpirationState;
    gfxFontStyle               mStyle;
    nsAutoTArray<gfxGlyphExtents*,1> mGlyphExtentsArray;

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

    // Default simply calls m[Platform|HarfBuzz]Shaper->InitTextRun().
    // Override if the font class wants to give special handling
    // to shaper failure.
    // Returns PR_FALSE if shaping failed (though currently we
    // don't have any good way to handle that situation).
    virtual bool InitTextRun(gfxContext *aContext,
                               gfxTextRun *aTextRun,
                               const PRUnichar *aString,
                               PRUint32 aRunStart,
                               PRUint32 aRunLength,
                               PRInt32 aRunScript,
                               bool aPreferPlatformShaping = false);
};

// proportion of ascent used for x-height, if unable to read value from font
#define DEFAULT_XHEIGHT_FACTOR 0.56f

class THEBES_API gfxTextRunFactory {
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
        TEXT_OPTIMIZE_SPEED          = 0x0100
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
        PRUint32     *mInitialBreaks;
        PRUint32      mInitialBreakCount;
        // The ratio to use to convert device pixels to application layout units
        PRUint32      mAppUnitsPerDevUnit;
    };

    virtual ~gfxTextRunFactory() {}
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
 * 
 * gfxTextRun stores a list of zero or more glyphs for each character. For each
 * glyph we store the glyph ID, the advance, and possibly an xoffset and yoffset.
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
 * gfxTextRun methods that measure or draw substrings will associate all the
 * glyphs in a cluster with the first character of the cluster; if that character
 * is in the substring, the glyphs will be measured or drawn, otherwise they
 * won't.
 */
class THEBES_API gfxTextRun {
public:
    virtual ~gfxTextRun();

    typedef gfxFont::RunMetrics Metrics;

    // Public textrun API for general use

    bool IsClusterStart(PRUint32 aPos) {
        NS_ASSERTION(0 <= aPos && aPos < mCharacterCount, "aPos out of range");
        return mCharacterGlyphs[aPos].IsClusterStart();
    }
    bool IsLigatureGroupStart(PRUint32 aPos) {
        NS_ASSERTION(0 <= aPos && aPos < mCharacterCount, "aPos out of range");
        return mCharacterGlyphs[aPos].IsLigatureGroupStart();
    }
    bool CanBreakLineBefore(PRUint32 aPos) {
        NS_ASSERTION(0 <= aPos && aPos < mCharacterCount, "aPos out of range");
        return mCharacterGlyphs[aPos].CanBreakBefore() ==
            CompressedGlyph::FLAG_BREAK_TYPE_NORMAL;
    }
    bool CanHyphenateBefore(PRUint32 aPos) {
        NS_ASSERTION(0 <= aPos && aPos < mCharacterCount, "aPos out of range");
        return mCharacterGlyphs[aPos].CanBreakBefore() ==
            CompressedGlyph::FLAG_BREAK_TYPE_HYPHEN;
    }

    PRUint32 GetLength() { return mCharacterCount; }

    // All PRUint32 aStart, PRUint32 aLength ranges below are restricted to
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
    virtual bool SetPotentialLineBreaks(PRUint32 aStart, PRUint32 aLength,
                                          PRUint8 *aBreakBefore,
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
        virtual void GetHyphenationBreaks(PRUint32 aStart, PRUint32 aLength,
                                          bool *aBreakBefore) = 0;

        // Returns the provider's hyphenation setting, so callers can decide
        // whether it is necessary to call GetHyphenationBreaks.
        // Result is an NS_STYLE_HYPHENS_* value.
        virtual PRInt8 GetHyphensOption() = 0;

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
        virtual void GetSpacing(PRUint32 aStart, PRUint32 aLength,
                                Spacing *aSpacing) = 0;
    };

    class ClusterIterator {
    public:
        ClusterIterator(gfxTextRun *aTextRun);

        void Reset();

        bool NextCluster();

        PRUint32 Position() const {
            return mCurrentChar;
        }

        PRUint32 ClusterLength() const;

        gfxFloat ClusterAdvance(PropertyProvider *aProvider) const;

    private:
        gfxTextRun *mTextRun;
        PRUint32    mCurrentChar;
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
     *      dirty, &provider, nsnull) should have the same effect as
     * Draw(ctx, pt, offset1, length1+length2, dirty, &provider, nsnull).
     * For RTL runs the rule is:
     * Draw(ctx, pt, offset1 + length1, length2, dirty, &provider, &advance) followed by
     * Draw(ctx, gfxPoint(pt.x + advance, pt.y), offset1, length1,
     *      dirty, &provider, nsnull) should have the same effect as
     * Draw(ctx, pt, offset1, length1+length2, dirty, &provider, nsnull).
     * 
     * Glyphs should be drawn in logical content order, which can be significant
     * if they overlap (perhaps due to negative spacing).
     */
    void Draw(gfxContext *aContext, gfxPoint aPt,
              PRUint32 aStart, PRUint32 aLength,
              PropertyProvider *aProvider,
              gfxFloat *aAdvanceWidth);

    /**
     * Renders a substring to a path. Uses only GetSpacing from aBreakProvider.
     * The provided point is the baseline origin on the left of the string
     * for LTR, on the right of the string for RTL.
     * @param aAdvanceWidth if non-null, the advance width of the substring
     * is returned here.
     * 
     * Drawing should respect advance widths in the way that Draw above does.
     * 
     * Glyphs should be drawn in logical content order.
     * 
     * UNLIKE Draw above, this cannot be used to render substrings that start or
     * end inside a ligature.
     */
    void DrawToPath(gfxContext *aContext, gfxPoint aPt,
                    PRUint32 aStart, PRUint32 aLength,
                    PropertyProvider *aBreakProvider,
                    gfxFloat *aAdvanceWidth);

    /**
     * Computes the ReflowMetrics for a substring.
     * Uses GetSpacing from aBreakProvider.
     * @param aBoundingBoxType which kind of bounding box (loose/tight)
     */
    Metrics MeasureText(PRUint32 aStart, PRUint32 aLength,
                        gfxFont::BoundingBoxType aBoundingBoxType,
                        gfxContext *aRefContextForTightBoundingBox,
                        PropertyProvider *aProvider);

    /**
     * Computes just the advance width for a substring.
     * Uses GetSpacing from aBreakProvider.
     */
    gfxFloat GetAdvanceWidth(PRUint32 aStart, PRUint32 aLength,
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
    virtual bool SetLineBreaks(PRUint32 aStart, PRUint32 aLength,
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
     * @param aMaxLength this can be PR_UINT32_MAX, in which case the length used
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
     * or PR_UINT32_MAX if no such N exists, where GetAdvanceWidth assumes
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
    PRUint32 BreakAndMeasureText(PRUint32 aStart, PRUint32 aMaxLength,
                                 bool aLineBreakBefore, gfxFloat aWidth,
                                 PropertyProvider *aProvider,
                                 bool aSuppressInitialBreak,
                                 gfxFloat *aTrimWhitespace,
                                 Metrics *aMetrics,
                                 gfxFont::BoundingBoxType aBoundingBoxType,
                                 gfxContext *aRefContextForTightBoundingBox,
                                 bool *aUsedHyphenation,
                                 PRUint32 *aLastBreak,
                                 bool aCanWordWrap,
                                 gfxBreakPriority *aBreakPriority);

    /**
     * Update the reference context.
     * XXX this is a hack. New text frame does not call this. Use only
     * temporarily for old text frame.
     */
    void SetContext(gfxContext *aContext) {}

    // Utility getters

    bool IsRightToLeft() const { return (mFlags & gfxTextRunFactory::TEXT_IS_RTL) != 0; }
    gfxFloat GetDirection() const { return (mFlags & gfxTextRunFactory::TEXT_IS_RTL) ? -1.0 : 1.0; }
    void *GetUserData() const { return mUserData; }
    void SetUserData(void *aUserData) { mUserData = aUserData; }
    PRUint32 GetFlags() const { return mFlags; }
    void SetFlagBits(PRUint32 aFlags) {
      NS_ASSERTION(!(aFlags & ~gfxTextRunFactory::SETTABLE_FLAGS),
                   "Only user flags should be mutable");
      mFlags |= aFlags;
    }
    void ClearFlagBits(PRUint32 aFlags) {
      NS_ASSERTION(!(aFlags & ~gfxTextRunFactory::SETTABLE_FLAGS),
                   "Only user flags should be mutable");
      mFlags &= ~aFlags;
    }
    const gfxSkipChars& GetSkipChars() const { return mSkipChars; }
    PRUint32 GetAppUnitsPerDevUnit() const { return mAppUnitsPerDevUnit; }
    gfxFontGroup *GetFontGroup() const { return mFontGroup; }
    const PRUint8 *GetText8Bit() const
    { return (mFlags & gfxTextRunFactory::TEXT_IS_8BIT) ? mText.mSingle : nsnull; }
    const PRUnichar *GetTextUnicode() const
    { return (mFlags & gfxTextRunFactory::TEXT_IS_8BIT) ? nsnull : mText.mDouble; }
    const void *GetTextAt(PRUint32 aIndex) {
        return (mFlags & gfxTextRunFactory::TEXT_IS_8BIT)
            ? static_cast<const void *>(mText.mSingle + aIndex)
            : static_cast<const void *>(mText.mDouble + aIndex);
    }
    const PRUnichar GetChar(PRUint32 i) const
    { return (mFlags & gfxTextRunFactory::TEXT_IS_8BIT) ? mText.mSingle[i] : mText.mDouble[i]; }
    PRUint32 GetHashCode() const { return mHashCode; }
    void SetHashCode(PRUint32 aHash) { mHashCode = aHash; }

    // Call this, don't call "new gfxTextRun" directly. This does custom
    // allocation and initialization
    static gfxTextRun *Create(const gfxTextRunFactory::Parameters *aParams,
        const void *aText, PRUint32 aLength, gfxFontGroup *aFontGroup, PRUint32 aFlags);

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
            // is one that fits in the available bits (currently 13) (specified
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

            // The advance is stored in appunits
            ADVANCE_MASK  = 0x1FFF0000U,
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
            
            GLYPH_COUNT_MASK = 0x00FFFF00U,
            GLYPH_COUNT_SHIFT = 8
        };

        // "Simple glyphs" have a simple glyph ID, simple advance and their
        // x and y offsets are zero. Also the glyph extents do not overflow
        // the font-box defined by the font ascent, descent and glyph advance width.
        // These case is optimized to avoid storing DetailedGlyphs.

        // Returns true if the glyph ID aGlyph fits into the compressed representation
        static bool IsSimpleGlyphID(PRUint32 aGlyph) {
            return (aGlyph & GLYPH_MASK) == aGlyph;
        }
        // Returns true if the advance aAdvance fits into the compressed representation.
        // aAdvance is in appunits.
        static bool IsSimpleAdvance(PRUint32 aAdvance) {
            return (aAdvance & (ADVANCE_MASK >> ADVANCE_SHIFT)) == aAdvance;
        }

        bool IsSimpleGlyph() const { return (mValue & FLAG_IS_SIMPLE_GLYPH) != 0; }
        PRUint32 GetSimpleAdvance() const { return (mValue & ADVANCE_MASK) >> ADVANCE_SHIFT; }
        PRUint32 GetSimpleGlyph() const { return mValue & GLYPH_MASK; }

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

        PRUint8 CanBreakBefore() const {
            return (mValue & FLAGS_CAN_BREAK_BEFORE) >> FLAGS_CAN_BREAK_SHIFT;
        }
        // Returns FLAGS_CAN_BREAK_BEFORE if the setting changed, 0 otherwise
        PRUint32 SetCanBreakBefore(PRUint8 aCanBreakBefore) {
            NS_ASSERTION(aCanBreakBefore <= 2,
                         "Bogus break-before value!");
            PRUint32 breakMask = (PRUint32(aCanBreakBefore) << FLAGS_CAN_BREAK_SHIFT);
            PRUint32 toggle = breakMask ^ (mValue & FLAGS_CAN_BREAK_BEFORE);
            mValue ^= toggle;
            return toggle;
        }

        CompressedGlyph& SetSimpleGlyph(PRUint32 aAdvanceAppUnits, PRUint32 aGlyph) {
            NS_ASSERTION(IsSimpleAdvance(aAdvanceAppUnits), "Advance overflow");
            NS_ASSERTION(IsSimpleGlyphID(aGlyph), "Glyph overflow");
            mValue = (mValue & FLAGS_CAN_BREAK_BEFORE) |
                FLAG_IS_SIMPLE_GLYPH |
                (aAdvanceAppUnits << ADVANCE_SHIFT) | aGlyph;
            return *this;
        }
        CompressedGlyph& SetComplex(bool aClusterStart, bool aLigatureStart,
                PRUint32 aGlyphCount) {
            mValue = (mValue & FLAGS_CAN_BREAK_BEFORE) |
                FLAG_NOT_MISSING |
                (aClusterStart ? 0 : FLAG_NOT_CLUSTER_START) |
                (aLigatureStart ? 0 : FLAG_NOT_LIGATURE_GROUP_START) |
                (aGlyphCount << GLYPH_COUNT_SHIFT);
            return *this;
        }
        /**
         * Missing glyphs are treated as ligature group starts; don't mess with
         * the cluster-start flag (see bugs 618870 and 619286).
         */
        CompressedGlyph& SetMissing(PRUint32 aGlyphCount) {
            mValue = (mValue & (FLAGS_CAN_BREAK_BEFORE | FLAG_NOT_CLUSTER_START)) |
                (aGlyphCount << GLYPH_COUNT_SHIFT);
            return *this;
        }
        PRUint32 GetGlyphCount() const {
            NS_ASSERTION(!IsSimpleGlyph(), "Expected non-simple-glyph");
            return (mValue & GLYPH_COUNT_MASK) >> GLYPH_COUNT_SHIFT;
        }

    private:
        PRUint32 mValue;
    };

    /**
     * When the glyphs for a character don't fit into a CompressedGlyph record
     * in SimpleGlyph format, we use an array of DetailedGlyphs instead.
     */
    struct DetailedGlyph {
        /** The glyphID, or the Unicode character
         * if this is a missing glyph */
        PRUint32 mGlyphID;
        /** The advance, x-offset and y-offset of the glyph, in appunits
         *  mAdvance is in the text direction (RTL or LTR)
         *  mXOffset is always from left to right
         *  mYOffset is always from top to bottom */   
        PRInt32  mAdvance;
        float    mXOffset, mYOffset;
    };

    // The text is divided into GlyphRuns as necessary
    struct GlyphRun {
        nsRefPtr<gfxFont> mFont;   // never null
        PRUint32          mCharacterOffset; // into original UTF16 string
        PRUint8           mMatchType;
    };

    class THEBES_API GlyphRunIterator {
    public:
        GlyphRunIterator(gfxTextRun *aTextRun, PRUint32 aStart, PRUint32 aLength)
          : mTextRun(aTextRun), mStartOffset(aStart), mEndOffset(aStart + aLength) {
            mNextIndex = mTextRun->FindFirstGlyphRunContaining(aStart);
        }
        bool NextRun();
        GlyphRun *GetGlyphRun() { return mGlyphRun; }
        PRUint32 GetStringStart() { return mStringStart; }
        PRUint32 GetStringEnd() { return mStringEnd; }
    private:
        gfxTextRun *mTextRun;
        GlyphRun   *mGlyphRun;
        PRUint32    mStringStart;
        PRUint32    mStringEnd;
        PRUint32    mNextIndex;
        PRUint32    mStartOffset;
        PRUint32    mEndOffset;
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
    nsresult AddGlyphRun(gfxFont *aFont, PRUint8 aMatchType,
                         PRUint32 aStartCharIndex, bool aForceNewRun);
    void ResetGlyphRuns() { mGlyphRuns.Clear(); }
    void SortGlyphRuns();
    void SanitizeGlyphRuns();

    // Call the following glyph-setters during initialization or during reshaping
    // only. It is OK to overwrite existing data for a character.
    /**
     * Set the glyph data for a character. aGlyphs may be null if aGlyph is a
     * simple glyph or has no associated glyphs. If non-null the data is copied,
     * the caller retains ownership.
     */
    void SetSimpleGlyph(PRUint32 aCharIndex, CompressedGlyph aGlyph) {
        NS_ASSERTION(aGlyph.IsSimpleGlyph(), "Should be a simple glyph here");
        if (mCharacterGlyphs) {
            mCharacterGlyphs[aCharIndex] = aGlyph;
        }
    }
    void SetGlyphs(PRUint32 aCharIndex, CompressedGlyph aGlyph,
                   const DetailedGlyph *aGlyphs);
    void SetMissingGlyph(PRUint32 aCharIndex, PRUint32 aUnicodeChar);
    void SetSpaceGlyph(gfxFont *aFont, gfxContext *aContext, PRUint32 aCharIndex);

    // If the character at aIndex is default-ignorable, set the glyph
    // to be invisible-missing and return TRUE, else return FALSE
    bool FilterIfIgnorable(PRUint32 aIndex);

    /**
     * Prefetch all the glyph extents needed to ensure that Measure calls
     * on this textrun not requesting tight boundingBoxes will succeed. Note
     * that some glyph extents might not be fetched due to OOM or other
     * errors.
     */
    void FetchGlyphExtents(gfxContext *aRefContext);

    // API for access to the raw glyph data, needed by gfxFont::Draw
    // and gfxFont::GetBoundingBox
    const CompressedGlyph *GetCharacterGlyphs() { return mCharacterGlyphs; }

    // NOTE that this must not be called for a character offset that does
    // not have any DetailedGlyph records; callers must have verified that
    // mCharacterGlyphs[aCharIndex].GetGlyphCount() is greater than zero.
    DetailedGlyph *GetDetailedGlyphs(PRUint32 aCharIndex) {
        NS_ASSERTION(mDetailedGlyphs != nsnull &&
                     !mCharacterGlyphs[aCharIndex].IsSimpleGlyph() &&
                     mCharacterGlyphs[aCharIndex].GetGlyphCount() > 0,
                     "invalid use of GetDetailedGlyphs; check the caller!");
        return mDetailedGlyphs->Get(aCharIndex);
    }

    bool HasDetailedGlyphs() { return mDetailedGlyphs != nsnull; }
    PRUint32 CountMissingGlyphs();
    const GlyphRun *GetGlyphRuns(PRUint32 *aNumGlyphRuns) {
        *aNumGlyphRuns = mGlyphRuns.Length();
        return mGlyphRuns.Elements();
    }
    // Returns the index of the GlyphRun containing the given offset.
    // Returns mGlyphRuns.Length() when aOffset is mCharacterCount.
    PRUint32 FindFirstGlyphRunContaining(PRUint32 aOffset);
    // Copy glyph data for a range of characters from aSource to this
    // textrun.
    virtual void CopyGlyphDataFrom(gfxTextRun *aSource, PRUint32 aStart,
                                   PRUint32 aLength, PRUint32 aDest);

    nsExpirationState *GetExpirationState() { return &mExpirationState; }

    struct LigatureData {
        // textrun offsets of the start and end of the containing ligature
        PRUint32 mLigatureStart;
        PRUint32 mLigatureEnd;
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
    
    // user font set generation when text run was created
    PRUint64 GetUserFontSetGeneration() { return mUserFontSetGeneration; }

#ifdef DEBUG
    // number of entries referencing this textrun in the gfxTextRunWordCache
    PRUint32 mCachedWords;
    // generation of gfxTextRunWordCache that refers to this textrun;
    // if the cache gets cleared, then mCachedWords is no longer meaningful
    PRUint32 mCacheGeneration;
    
    void Dump(FILE* aOutput);
#endif

    // post-process glyph advances to deal with synthetic bolding
    void AdjustAdvancesForSyntheticBold(gfxContext *aContext,
                                        PRUint32 aStart,
                                        PRUint32 aLength);

protected:
    /**
     * Initializes the textrun to blank.
     * @param aGlyphStorage preallocated array of CompressedGlyph[aLength]
     * for the textrun to use; if aText is not persistent, then it has also
     * been appended to this array, so it must NOT be freed separately.
     */
    gfxTextRun(const gfxTextRunFactory::Parameters *aParams, const void *aText,
               PRUint32 aLength, gfxFontGroup *aFontGroup, PRUint32 aFlags,
               CompressedGlyph *aGlyphStorage);

    /**
     * Helper for the Create() factory method to allocate the required
     * glyph storage, and copy the text (modifying the aText parameter)
     * if it is not flagged as persistent.
     */
    static CompressedGlyph* AllocateStorage(const void*& aText,
                                            PRUint32 aLength,
                                            PRUint32 aFlags);

private:
    // **** general helpers **** 

    // Allocate aCount DetailedGlyphs for the given index
    DetailedGlyph *AllocateDetailedGlyphs(PRUint32 aCharIndex, PRUint32 aCount);

    // Get the total advance for a range of glyphs.
    PRInt32 GetAdvanceForGlyphs(PRUint32 aStart, PRUint32 aEnd);

    // Spacing for characters outside the range aSpacingStart/aSpacingEnd
    // is assumed to be zero; such characters are not passed to aProvider.
    // This is useful to protect aProvider from being passed character indices
    // it is not currently able to handle.
    bool GetAdjustedSpacingArray(PRUint32 aStart, PRUint32 aEnd,
                                   PropertyProvider *aProvider,
                                   PRUint32 aSpacingStart, PRUint32 aSpacingEnd,
                                   nsTArray<PropertyProvider::Spacing> *aSpacing);

    //  **** ligature helpers ****
    // (Platforms do the actual ligaturization, but we need to do a bunch of stuff
    // to handle requests that begin or end inside a ligature)

    // if aProvider is null then mBeforeSpacing and mAfterSpacing are set to zero
    LigatureData ComputeLigatureData(PRUint32 aPartStart, PRUint32 aPartEnd,
                                     PropertyProvider *aProvider);
    gfxFloat ComputePartialLigatureWidth(PRUint32 aPartStart, PRUint32 aPartEnd,
                                         PropertyProvider *aProvider);
    void DrawPartialLigature(gfxFont *aFont, gfxContext *aCtx,
                             PRUint32 aStart, PRUint32 aEnd, gfxPoint *aPt,
                             PropertyProvider *aProvider);
    // Advance aStart to the start of the nearest ligature; back up aEnd
    // to the nearest ligature end; may result in *aStart == *aEnd
    void ShrinkToLigatureBoundaries(PRUint32 *aStart, PRUint32 *aEnd);
    // result in appunits
    gfxFloat GetPartialLigatureWidth(PRUint32 aStart, PRUint32 aEnd, PropertyProvider *aProvider);
    void AccumulatePartialLigatureMetrics(gfxFont *aFont,
                                          PRUint32 aStart, PRUint32 aEnd,
                                          gfxFont::BoundingBoxType aBoundingBoxType,
                                          gfxContext *aRefContext,
                                          PropertyProvider *aProvider,
                                          Metrics *aMetrics);

    // **** measurement helper ****
    void AccumulateMetricsForRun(gfxFont *aFont, PRUint32 aStart, PRUint32 aEnd,
                                 gfxFont::BoundingBoxType aBoundingBoxType,
                                 gfxContext *aRefContext,
                                 PropertyProvider *aProvider,
                                 PRUint32 aSpacingStart, PRUint32 aSpacingEnd,
                                 Metrics *aMetrics);

    // **** drawing helper ****
    void DrawGlyphs(gfxFont *aFont, gfxContext *aContext, bool aDrawToPath,
                    gfxPoint *aPt, PRUint32 aStart, PRUint32 aEnd,
                    PropertyProvider *aProvider,
                    PRUint32 aSpacingStart, PRUint32 aSpacingEnd);

    // All our glyph data is in logical order, not visual.
    // mCharacterGlyphs is allocated by the factory that creates the textrun,
    // to avoid the possibility of failure during the constructor;
    // however, ownership passes to the textrun during construction and so
    // it must be deleted in the destructor.
    CompressedGlyph*                               mCharacterGlyphs;

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
        DetailedGlyph* Get(PRUint32 aOffset) {
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

        DetailedGlyph* Allocate(PRUint32 aOffset, PRUint32 aCount) {
            PRUint32 detailIndex = mDetails.Length();
            DetailedGlyph *details = mDetails.AppendElements(aCount);
            if (!details) {
                return nsnull;
            }
            // We normally set up glyph records sequentially, so the common case
            // here is to append new records to the mOffsetToIndex array;
            // test for that before falling back to the InsertElementSorted
            // method.
            if (mOffsetToIndex.Length() == 0 ||
                aOffset > mOffsetToIndex[mOffsetToIndex.Length() - 1].mOffset) {
                if (!mOffsetToIndex.AppendElement(DGRec(aOffset, detailIndex))) {
                    return nsnull;
                }
            } else {
                if (!mOffsetToIndex.InsertElementSorted(DGRec(aOffset, detailIndex),
                                                        CompareRecordOffsets())) {
                    return nsnull;
                }
            }
            return details;
        }

    private:
        struct DGRec {
            DGRec(const PRUint32& aOffset, const PRUint32& aIndex)
                : mOffset(aOffset), mIndex(aIndex) { }
            PRUint32 mOffset; // source character offset in the textrun
            PRUint32 mIndex;  // index where this char's DetailedGlyphs begin
        };

        struct CompareToOffset {
            bool Equals(const DGRec& a, const PRUint32& b) const {
                return a.mOffset == b;
            }
            bool LessThan(const DGRec& a, const PRUint32& b) const {
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

    // XXX this should be changed to a GlyphRun plus a maybe-null GlyphRun*,
    // for smaller size especially in the super-common one-glyphrun case
    nsAutoTArray<GlyphRun,1>        mGlyphRuns;
    // When TEXT_IS_8BIT is set, we use mSingle, otherwise we use mDouble.
    // When TEXT_IS_PERSISTENT is set, we don't own the text, otherwise we
    // own the text. When we own the text, it's allocated fused with the
    // mCharacterGlyphs array, and therefore need not be explicitly deleted.
    // This text is not null-terminated.
    union {
        const PRUint8   *mSingle;
        const PRUnichar *mDouble;
    } mText;
    void             *mUserData;
    gfxFontGroup     *mFontGroup; // addrefed
    gfxSkipChars      mSkipChars;
    nsExpirationState mExpirationState;
    PRUint32          mAppUnitsPerDevUnit;
    PRUint32          mFlags;
    PRUint32          mCharacterCount;
    PRUint32          mHashCode;
    PRUint64          mUserFontSetGeneration; // user font set generation when text run created

    bool              mSkipDrawing; // true if the font group we used had a user font
                                    // download that's in progress, so we should hide text
                                    // until the download completes (or timeout fires)
};

class THEBES_API gfxFontGroup : public gfxTextRunFactory {
public:
    static void Shutdown(); // platform must call this to release the languageAtomService

    gfxFontGroup(const nsAString& aFamilies, const gfxFontStyle *aStyle, gfxUserFontSet *aUserFontSet = nsnull);

    virtual ~gfxFontGroup();

    virtual gfxFont *GetFontAt(PRInt32 i) {
        // If it turns out to be hard for all clients that cache font
        // groups to call UpdateFontList at appropriate times, we could
        // instead consider just calling UpdateFontList from someplace
        // more central (such as here).
        NS_ASSERTION(!mUserFontSet || mCurrGeneration == GetGeneration(),
                     "Whoever was caching this font group should have "
                     "called UpdateFontList on it");
        NS_ASSERTION(mFonts.Length() > PRUint32(i), 
                     "Requesting a font index that doesn't exist");

        return static_cast<gfxFont*>(mFonts[i]);
    }
    virtual PRUint32 FontListLength() const {
        return mFonts.Length();
    }

    bool Equals(const gfxFontGroup& other) const {
        return mFamilies.Equals(other.mFamilies) &&
            mStyle.Equals(other.mStyle);
    }

    const gfxFontStyle *GetStyle() const { return &mStyle; }

    virtual gfxFontGroup *Copy(const gfxFontStyle *aStyle);

    /**
     * The listed characters should not be passed in to MakeTextRun and should
     * be treated as invisible and zero-width.
     */
    static bool IsInvalidChar(PRUnichar ch);
    
    /**
     * Make a textrun for an empty string. This is fast; if you call it,
     * don't bother caching the result.
     */
    gfxTextRun *MakeEmptyTextRun(const Parameters *aParams, PRUint32 aFlags);
    /**
     * Make a textrun for a single ASCII space. This is fast; if you call it,
     * don't bother caching the result.
     */
    gfxTextRun *MakeSpaceTextRun(const Parameters *aParams, PRUint32 aFlags);

    /**
     * Make a textrun for a given string.
     * If aText is not persistent (aFlags & TEXT_IS_PERSISTENT), the
     * textrun will copy it.
     * This calls FetchGlyphExtents on the textrun.
     */
    virtual gfxTextRun *MakeTextRun(const PRUnichar *aString, PRUint32 aLength,
                                    const Parameters *aParams, PRUint32 aFlags);
    /**
     * Make a textrun for a given string.
     * If aText is not persistent (aFlags & TEXT_IS_PERSISTENT), the
     * textrun will copy it.
     * This calls FetchGlyphExtents on the textrun.
     */
    virtual gfxTextRun *MakeTextRun(const PRUint8 *aString, PRUint32 aLength,
                                    const Parameters *aParams, PRUint32 aFlags);

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
    enum { UNDERLINE_OFFSET_NOT_SET = PR_INT16_MAX };
    virtual gfxFloat GetUnderlineOffset() {
        if (mUnderlineOffset == UNDERLINE_OFFSET_NOT_SET)
            mUnderlineOffset = GetFontAt(0)->GetMetrics().underlineOffset;
        return mUnderlineOffset;
    }

    virtual already_AddRefed<gfxFont>
        FindFontForChar(PRUint32 ch, PRUint32 prevCh, PRInt32 aRunScript,
                        gfxFont *aPrevMatchedFont,
                        PRUint8 *aMatchType);

    // search through pref fonts for a character, return nsnull if no matching pref font
    virtual already_AddRefed<gfxFont> WhichPrefFontSupportsChar(PRUint32 aCh);

    virtual already_AddRefed<gfxFont> WhichSystemFontSupportsChar(PRUint32 aCh);

    void ComputeRanges(nsTArray<gfxTextRange>& mRanges,
                       const PRUnichar *aString, PRUint32 begin, PRUint32 end,
                       PRInt32 aRunScript);

    gfxUserFontSet* GetUserFontSet();

    // With downloadable fonts, the composition of the font group can change as fonts are downloaded
    // for each change in state of the user font set, the generation value is bumped to avoid picking up
    // previously created text runs in the text run word cache.  For font groups based on stylesheets
    // with no @font-face rule, this always returns 0.
    PRUint64 GetGeneration();

    // If there is a user font set, check to see whether the font list or any
    // caches need updating.
    virtual void UpdateFontList();

    bool ShouldSkipDrawing() const {
        return mSkipDrawing;
    }

protected:
    nsString mFamilies;
    gfxFontStyle mStyle;
    nsTArray< nsRefPtr<gfxFont> > mFonts;
    gfxFloat mUnderlineOffset;

    gfxUserFontSet* mUserFontSet;
    PRUint64 mCurrGeneration;  // track the current user font set generation, rebuild font list if needed

    // cache the most recent pref font to avoid general pref font lookup
    nsRefPtr<gfxFontFamily> mLastPrefFamily;
    nsRefPtr<gfxFont>       mLastPrefFont;
    eFontPrefLang           mLastPrefLang;       // lang group for last pref font
    eFontPrefLang           mPageLang;
    bool                    mLastPrefFirstFont;  // is this the first font in the list of pref fonts for this lang group?

    bool                    mSkipDrawing; // hide text while waiting for a font
                                          // download to complete (or fallback
                                          // timer to fire)

    // Used for construction/destruction.  Not intended to change the font set
    // as invalidation of font lists and caches is not considered.
    void SetUserFontSet(gfxUserFontSet *aUserFontSet);

    // Initialize the list of fonts
    void BuildFontList();

    // Init this font group's font metrics. If there no bad fonts, you don't need to call this.
    // But if there are one or more bad fonts which have bad underline offset,
    // you should call this with the *first* bad font.
    void InitMetricsForBadFont(gfxFont* aBadFont);

    // Set up the textrun glyphs for an entire text run:
    // find script runs, and then call InitScriptRun for each
    void InitTextRun(gfxContext *aContext,
                     gfxTextRun *aTextRun,
                     const PRUnichar *aString,
                     PRUint32 aLength);

    // InitTextRun helper to handle a single script run, by finding font ranges
    // and calling each font's InitTextRun() as appropriate
    void InitScriptRun(gfxContext *aContext,
                       gfxTextRun *aTextRun,
                       const PRUnichar *aString,
                       PRUint32 aTotalLength,
                       PRUint32 aScriptRunStart,
                       PRUint32 aScriptRunEnd,
                       PRInt32 aRunScript);

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

    static bool FontResolverProc(const nsAString& aName, void *aClosure);

    static bool FindPlatformFont(const nsAString& aName,
                                   const nsACString& aGenericName,
                                   bool aUseFontSet,
                                   void *closure);

    static NS_HIDDEN_(nsILanguageAtomService*) gLangService;
};
#endif

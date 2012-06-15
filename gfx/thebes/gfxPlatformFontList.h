/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFXPLATFORMFONTLIST_H_
#define GFXPLATFORMFONTLIST_H_

#include "nsDataHashtable.h"
#include "nsRefPtrHashtable.h"
#include "nsTHashtable.h"

#include "gfxFontUtils.h"
#include "gfxFont.h"
#include "gfxPlatform.h"

#include "nsIMemoryReporter.h"
#include "mozilla/FunctionTimer.h"
#include "mozilla/Attributes.h"

class CharMapHashKey : public PLDHashEntryHdr
{
public:
    typedef gfxCharacterMap* KeyType;
    typedef const gfxCharacterMap* KeyTypePointer;

    CharMapHashKey(const gfxCharacterMap *aCharMap) :
        mCharMap(const_cast<gfxCharacterMap*>(aCharMap))
    {
        MOZ_COUNT_CTOR(CharMapHashKey);
    }
    CharMapHashKey(const CharMapHashKey& toCopy) :
        mCharMap(toCopy.mCharMap)
    {
        MOZ_COUNT_CTOR(CharMapHashKey);
    }
    ~CharMapHashKey()
    {
        MOZ_COUNT_DTOR(CharMapHashKey);
    }

    gfxCharacterMap* GetKey() const { return mCharMap; }

    bool KeyEquals(const gfxCharacterMap *aCharMap) const {
        NS_ASSERTION(!aCharMap->mBuildOnTheFly && !mCharMap->mBuildOnTheFly,
                     "custom cmap used in shared cmap hashtable");
        // cmaps built on the fly never match
        if (aCharMap->mHash != mCharMap->mHash)
        {
            return false;
        }
        return mCharMap->Equals(aCharMap);
    }

    static const gfxCharacterMap* KeyToPointer(gfxCharacterMap *aCharMap) {
        return aCharMap;
    }
    static PLDHashNumber HashKey(const gfxCharacterMap *aCharMap) {
        return aCharMap->mHash;
    }

    enum { ALLOW_MEMMOVE = true };

protected:
    gfxCharacterMap *mCharMap;
};

// gfxPlatformFontList is an abstract class for the global font list on the system;
// concrete subclasses for each platform implement the actual interface to the system fonts.
// This class exists because we cannot rely on the platform font-finding APIs to behave
// in sensible/similar ways, particularly with rich, complex OpenType families,
// so we do our own font family/style management here instead.

// Much of this is based on the old gfxQuartzFontCache, but adapted for use on all platforms.

struct FontListSizes {
    PRUint32 mFontListSize; // size of the font list and dependent objects
                            // (font family and face names, etc), but NOT
                            // including the font table cache and the cmaps
    PRUint32 mFontTableCacheSize; // memory used for the gfxFontEntry table caches
    PRUint32 mCharMapsSize; // memory used for cmap coverage info
};

class gfxPlatformFontList : protected gfxFontInfoLoader
{
public:
    static gfxPlatformFontList* PlatformFontList() {
        return sPlatformFontList;
    }

    static nsresult Init() {
        NS_TIME_FUNCTION;

        NS_ASSERTION(!sPlatformFontList, "What's this doing here?");
        gfxPlatform::GetPlatform()->CreatePlatformFontList();
        if (!sPlatformFontList) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
        return NS_OK;
    }

    static void Shutdown() {
        delete sPlatformFontList;
        sPlatformFontList = nsnull;
    }

    virtual ~gfxPlatformFontList();

    // initialize font lists
    virtual nsresult InitFontList();

    void GetFontList (nsIAtom *aLangGroup,
                      const nsACString& aGenericFamily,
                      nsTArray<nsString>& aListOfFonts);

    virtual bool ResolveFontName(const nsAString& aFontName,
                                   nsAString& aResolvedFontName);

    void UpdateFontList() { InitFontList(); }

    void ClearPrefFonts() { mPrefFonts.Clear(); }

    virtual void GetFontFamilyList(nsTArray<nsRefPtr<gfxFontFamily> >& aFamilyArray);

    virtual gfxFontEntry*
    SystemFindFontForChar(const PRUint32 aCh,
                          PRInt32 aRunScript,
                          const gfxFontStyle* aStyle);

    // TODO: make this virtual, for lazily adding to the font list
    virtual gfxFontFamily* FindFamily(const nsAString& aFamily);

    gfxFontEntry* FindFontForFamily(const nsAString& aFamily, const gfxFontStyle* aStyle, bool& aNeedsBold);

    bool GetPrefFontFamilyEntries(eFontPrefLang aLangGroup, nsTArray<nsRefPtr<gfxFontFamily> > *array);
    void SetPrefFontFamilyEntries(eFontPrefLang aLangGroup, nsTArray<nsRefPtr<gfxFontFamily> >& array);

    // name lookup table methods

    void AddOtherFamilyName(gfxFontFamily *aFamilyEntry, nsAString& aOtherFamilyName);

    void AddFullname(gfxFontEntry *aFontEntry, nsAString& aFullname);

    void AddPostscriptName(gfxFontEntry *aFontEntry, nsAString& aPostscriptName);

    bool NeedFullnamePostscriptNames() { return mNeedFullnamePostscriptNames; }

    // pure virtual functions, to be provided by concrete subclasses

    // get the system default font
    virtual gfxFontEntry* GetDefaultFont(const gfxFontStyle* aStyle,
                                         bool& aNeedsBold) = 0;

    // look up a font by name on the host platform
    virtual gfxFontEntry* LookupLocalFont(const gfxProxyFontEntry *aProxyEntry,
                                          const nsAString& aFontName) = 0;

    // create a new platform font from downloaded data (@font-face)
    // this method is responsible to ensure aFontData is NS_Free()'d
    virtual gfxFontEntry* MakePlatformFont(const gfxProxyFontEntry *aProxyEntry,
                                           const PRUint8 *aFontData,
                                           PRUint32 aLength) = 0;

    // get the standard family name on the platform for a given font name
    // (platforms may override, eg Mac)
    virtual bool GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName);

    virtual void SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf,
                                     FontListSizes*    aSizes) const;
    virtual void SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf,
                                     FontListSizes*    aSizes) const;

    // search for existing cmap that matches the input
    // return the input if no match is found
    gfxCharacterMap* FindCharMap(gfxCharacterMap *aCmap);

    // add a cmap to the shared cmap set
    gfxCharacterMap* AddCmap(const gfxCharacterMap *aCharMap);

    // remove the cmap from the shared cmap set
    void RemoveCmap(const gfxCharacterMap *aCharMap);

protected:
    class MemoryReporter MOZ_FINAL
        : public nsIMemoryMultiReporter
    {
    public:
        NS_DECL_ISUPPORTS
        NS_DECL_NSIMEMORYMULTIREPORTER
    };

    gfxPlatformFontList(bool aNeedFullnamePostscriptNames = true);

    static gfxPlatformFontList *sPlatformFontList;

    static PLDHashOperator FindFontForCharProc(nsStringHashKey::KeyType aKey,
                                               nsRefPtr<gfxFontFamily>& aFamilyEntry,
                                               void* userArg);

    // returns default font for a given character, null otherwise
    virtual gfxFontEntry* CommonFontFallback(const PRUint32 aCh,
                                             PRInt32 aRunScript,
                                             const gfxFontStyle* aMatchStyle);

    // search fonts system-wide for a given character, null otherwise
    virtual gfxFontEntry* GlobalFontFallback(const PRUint32 aCh,
                                             PRInt32 aRunScript,
                                             const gfxFontStyle* aMatchStyle,
                                             PRUint32& aCmapCount);

    // whether system-based font fallback is used or not
    // if system fallback is used, no need to load all cmaps
    virtual bool UsesSystemFallback() { return false; }

    // separate initialization for reading in name tables, since this is expensive
    void InitOtherFamilyNames();

    static PLDHashOperator InitOtherFamilyNamesProc(nsStringHashKey::KeyType aKey,
                                                    nsRefPtr<gfxFontFamily>& aFamilyEntry,
                                                    void* userArg);

    // read in all fullname/Postscript names for all font faces
    void InitFaceNameLists();

    static PLDHashOperator InitFaceNameListsProc(nsStringHashKey::KeyType aKey,
                                                 nsRefPtr<gfxFontFamily>& aFamilyEntry,
                                                 void* userArg);

    // commonly used fonts for which the name table should be loaded at startup
    virtual void PreloadNamesList();

    // load the bad underline blacklist from pref.
    void LoadBadUnderlineList();

    // explicitly set fixed-pitch flag for all faces
    void SetFixedPitch(const nsAString& aFamilyName);

    void GenerateFontListKey(const nsAString& aKeyName, nsAString& aResult);

    static PLDHashOperator
        HashEnumFuncForFamilies(nsStringHashKey::KeyType aKey,
                                nsRefPtr<gfxFontFamily>& aFamilyEntry,
                                void* aUserArg);

    // gfxFontInfoLoader overrides, used to load in font cmaps
    virtual void InitLoader();
    virtual bool RunLoader();
    virtual void FinishLoader();

    // used by memory reporter to accumulate sizes of family names in the hash
    static size_t
    SizeOfFamilyNameEntryExcludingThis(const nsAString&               aKey,
                                       const nsRefPtr<gfxFontFamily>& aFamily,
                                       nsMallocSizeOfFun              aMallocSizeOf,
                                       void*                          aUserArg);

    // canonical family name ==> family entry (unique, one name per family entry)
    nsRefPtrHashtable<nsStringHashKey, gfxFontFamily> mFontFamilies;

    // other family name ==> family entry (not unique, can have multiple names per
    // family entry, only names *other* than the canonical names are stored here)
    nsRefPtrHashtable<nsStringHashKey, gfxFontFamily> mOtherFamilyNames;

    // flag set after InitOtherFamilyNames is called upon first name lookup miss
    bool mOtherFamilyNamesInitialized;

    // flag set after fullname and Postcript name lists are populated
    bool mFaceNamesInitialized;

    // whether these are needed for a given platform
    bool mNeedFullnamePostscriptNames;

    // fullname ==> font entry (unique, one name per font entry)
    nsRefPtrHashtable<nsStringHashKey, gfxFontEntry> mFullnames;

    // Postscript name ==> font entry (unique, one name per font entry)
    nsRefPtrHashtable<nsStringHashKey, gfxFontEntry> mPostscriptNames;

    // cached pref font lists
    // maps list of family names ==> array of family entries, one per lang group
    nsDataHashtable<nsUint32HashKey, nsTArray<nsRefPtr<gfxFontFamily> > > mPrefFonts;

    // when system-wide font lookup fails for a character, cache it to skip future searches
    gfxSparseBitSet mCodepointsWithNoFonts;

    // the family to use for U+FFFD fallback, to avoid expensive search every time
    // on pages with lots of problems
    nsString mReplacementCharFallbackFamily;

    nsTHashtable<nsStringHashKey> mBadUnderlineFamilyNames;

    // character map data shared across families
    // contains weak ptrs to cmaps shared by font entry objects
    nsTHashtable<CharMapHashKey> mSharedCmaps;

    // data used as part of the font cmap loading process
    nsTArray<nsRefPtr<gfxFontFamily> > mFontFamiliesToLoad;
    PRUint32 mStartIndex;
    PRUint32 mIncrement;
    PRUint32 mNumFamilies;
};

#endif /* GFXPLATFORMFONTLIST_H_ */

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
#include "gfxFontInfoLoader.h"
#include "gfxFont.h"
#include "gfxPlatform.h"

#include "nsIMemoryReporter.h"
#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"

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
    uint32_t mFontListSize; // size of the font list and dependent objects
                            // (font family and face names, etc), but NOT
                            // including the font table cache and the cmaps
    uint32_t mFontTableCacheSize; // memory used for the gfxFontEntry table caches
    uint32_t mCharMapsSize; // memory used for cmap coverage info
};

class gfxUserFontSet;

class gfxPlatformFontList : public gfxFontInfoLoader
{
public:
    static gfxPlatformFontList* PlatformFontList() {
        return sPlatformFontList;
    }

    static nsresult Init() {
        NS_ASSERTION(!sPlatformFontList, "What's this doing here?");
        gfxPlatform::GetPlatform()->CreatePlatformFontList();
        if (!sPlatformFontList) {
            return NS_ERROR_OUT_OF_MEMORY;
        }
        return NS_OK;
    }

    static void Shutdown() {
        delete sPlatformFontList;
        sPlatformFontList = nullptr;
    }

    virtual ~gfxPlatformFontList();

    // initialize font lists
    virtual nsresult InitFontList();

    void GetFontList (nsIAtom *aLangGroup,
                      const nsACString& aGenericFamily,
                      nsTArray<nsString>& aListOfFonts);

    virtual bool ResolveFontName(const nsAString& aFontName,
                                   nsAString& aResolvedFontName);

    void UpdateFontList();

    void ClearPrefFonts() { mPrefFonts.Clear(); }

    virtual void GetFontFamilyList(nsTArray<nsRefPtr<gfxFontFamily> >& aFamilyArray);

    virtual gfxFontEntry*
    SystemFindFontForChar(const uint32_t aCh,
                          int32_t aRunScript,
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

    bool NeedFullnamePostscriptNames() { return mExtraNames != nullptr; }

    // pure virtual functions, to be provided by concrete subclasses

    // get the system default font family
    virtual gfxFontFamily* GetDefaultFont(const gfxFontStyle* aStyle) = 0;

    // look up a font by name on the host platform
    virtual gfxFontEntry* LookupLocalFont(const gfxProxyFontEntry *aProxyEntry,
                                          const nsAString& aFontName) = 0;

    // create a new platform font from downloaded data (@font-face)
    // this method is responsible to ensure aFontData is NS_Free()'d
    virtual gfxFontEntry* MakePlatformFont(const gfxProxyFontEntry *aProxyEntry,
                                           const uint8_t *aFontData,
                                           uint32_t aLength) = 0;

    // get the standard family name on the platform for a given font name
    // (platforms may override, eg Mac)
    virtual bool GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName);

    virtual void AddSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                        FontListSizes* aSizes) const;
    virtual void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                        FontListSizes* aSizes) const;

    // search for existing cmap that matches the input
    // return the input if no match is found
    gfxCharacterMap* FindCharMap(gfxCharacterMap *aCmap);

    // add a cmap to the shared cmap set
    gfxCharacterMap* AddCmap(const gfxCharacterMap *aCharMap);

    // remove the cmap from the shared cmap set
    void RemoveCmap(const gfxCharacterMap *aCharMap);

    // keep track of userfont sets to notify when global fontlist changes occur
    void AddUserFontSet(gfxUserFontSet *aUserFontSet) {
        mUserFontSetList.PutEntry(aUserFontSet);
    }

    void RemoveUserFontSet(gfxUserFontSet *aUserFontSet) {
        mUserFontSetList.RemoveEntry(aUserFontSet);
    }

protected:
    class MemoryReporter MOZ_FINAL : public nsIMemoryReporter
    {
    public:
        NS_DECL_ISUPPORTS
        NS_DECL_NSIMEMORYREPORTER
    };

    gfxPlatformFontList(bool aNeedFullnamePostscriptNames = true);

    static gfxPlatformFontList *sPlatformFontList;

    static PLDHashOperator FindFontForCharProc(nsStringHashKey::KeyType aKey,
                                               nsRefPtr<gfxFontFamily>& aFamilyEntry,
                                               void* userArg);

    // returns default font for a given character, null otherwise
    gfxFontEntry* CommonFontFallback(const uint32_t aCh,
                                     int32_t aRunScript,
                                     const gfxFontStyle* aMatchStyle,
                                     gfxFontFamily** aMatchedFamily);

    // search fonts system-wide for a given character, null otherwise
    virtual gfxFontEntry* GlobalFontFallback(const uint32_t aCh,
                                             int32_t aRunScript,
                                             const gfxFontStyle* aMatchStyle,
                                             uint32_t& aCmapCount,
                                             gfxFontFamily** aMatchedFamily);

    // whether system-based font fallback is used or not
    // if system fallback is used, no need to load all cmaps
    virtual bool UsesSystemFallback() { return false; }

    // verifies that a family contains a non-zero font count
    gfxFontFamily* CheckFamily(gfxFontFamily *aFamily);

    // initialize localized family names
    void InitOtherFamilyNames();

    static PLDHashOperator
    InitOtherFamilyNamesProc(nsStringHashKey::KeyType aKey,
                             nsRefPtr<gfxFontFamily>& aFamilyEntry,
                             void* userArg);

    // search through font families, looking for a given name, initializing
    // facename lists along the way. first checks all families with names
    // close to face name, then searchs all families if not found.
    gfxFontEntry* SearchFamiliesForFaceName(const nsAString& aFaceName);

    static PLDHashOperator
    ReadFaceNamesProc(nsStringHashKey::KeyType aKey,
                      nsRefPtr<gfxFontFamily>& aFamilyEntry,
                      void* userArg);

    // helper method for finding fullname/postscript names in facename lists
    gfxFontEntry* FindFaceName(const nsAString& aFaceName);

    // look up a font by name, for cases where platform font list
    // maintains explicit mappings of fullname/psname ==> font
    virtual gfxFontEntry* LookupInFaceNameLists(const nsAString& aFontName);

    static PLDHashOperator LookupMissedFaceNamesProc(nsStringHashKey *aKey,
                                                     void *aUserArg);

    static PLDHashOperator LookupMissedOtherNamesProc(nsStringHashKey *aKey,
                                                      void *aUserArg);

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

    virtual void GetFontFamilyNames(nsTArray<nsString>& aFontFamilyNames);

    // gfxFontInfoLoader overrides, used to load in font cmaps
    virtual void InitLoader();
    virtual bool LoadFontInfo();
    virtual void CleanupLoader();

    // read the loader initialization prefs, and start it
    void GetPrefsAndStartLoader();

    // for font list changes that affect all documents
    void ForceGlobalReflow();

    // used by memory reporter to accumulate sizes of family names in the hash
    static size_t
    SizeOfFamilyNameEntryExcludingThis(const nsAString&               aKey,
                                       const nsRefPtr<gfxFontFamily>& aFamily,
                                       mozilla::MallocSizeOf          aMallocSizeOf,
                                       void*                          aUserArg);

    // canonical family name ==> family entry (unique, one name per family entry)
    nsRefPtrHashtable<nsStringHashKey, gfxFontFamily> mFontFamilies;

    // other family name ==> family entry (not unique, can have multiple names per
    // family entry, only names *other* than the canonical names are stored here)
    nsRefPtrHashtable<nsStringHashKey, gfxFontFamily> mOtherFamilyNames;

    // flag set after InitOtherFamilyNames is called upon first name lookup miss
    bool mOtherFamilyNamesInitialized;

    // flag set after fullname and Postcript name lists are populated
    bool mFaceNameListsInitialized;

    struct ExtraNames {
      ExtraNames() : mFullnames(100), mPostscriptNames(100) {}
      // fullname ==> font entry (unique, one name per font entry)
      nsRefPtrHashtable<nsStringHashKey, gfxFontEntry> mFullnames;
      // Postscript name ==> font entry (unique, one name per font entry)
      nsRefPtrHashtable<nsStringHashKey, gfxFontEntry> mPostscriptNames;
    };
    nsAutoPtr<ExtraNames> mExtraNames;

    // face names missed when face name loading takes a long time
    nsAutoPtr<nsTHashtable<nsStringHashKey> > mFaceNamesMissed;

    // localized family names missed when face name loading takes a long time
    nsAutoPtr<nsTHashtable<nsStringHashKey> > mOtherNamesMissed;

    // cached pref font lists
    // maps list of family names ==> array of family entries, one per lang group
    nsDataHashtable<nsUint32HashKey, nsTArray<nsRefPtr<gfxFontFamily> > > mPrefFonts;

    // when system-wide font lookup fails for a character, cache it to skip future searches
    gfxSparseBitSet mCodepointsWithNoFonts;

    // the family to use for U+FFFD fallback, to avoid expensive search every time
    // on pages with lots of problems
    nsRefPtr<gfxFontFamily> mReplacementCharFallbackFamily;

    nsTHashtable<nsStringHashKey> mBadUnderlineFamilyNames;

    // character map data shared across families
    // contains weak ptrs to cmaps shared by font entry objects
    nsTHashtable<CharMapHashKey> mSharedCmaps;

    // data used as part of the font cmap loading process
    nsTArray<nsRefPtr<gfxFontFamily> > mFontFamiliesToLoad;
    uint32_t mStartIndex;
    uint32_t mIncrement;
    uint32_t mNumFamilies;

    nsTHashtable<nsPtrHashKey<gfxUserFontSet> > mUserFontSetList;
};

#endif /* GFXPLATFORMFONTLIST_H_ */

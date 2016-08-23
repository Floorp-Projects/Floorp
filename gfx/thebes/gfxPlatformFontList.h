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
#include "gfxFontConstants.h"
#include "gfxPlatform.h"
#include "gfxFontFamilyList.h"

#include "nsIMemoryReporter.h"
#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/RangedArray.h"
#include "nsILanguageAtomService.h"

class CharMapHashKey : public PLDHashEntryHdr
{
public:
    typedef gfxCharacterMap* KeyType;
    typedef const gfxCharacterMap* KeyTypePointer;

    explicit CharMapHashKey(const gfxCharacterMap *aCharMap) :
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
    typedef mozilla::unicode::Script Script;

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
    nsresult InitFontList();

    virtual void GetFontList(nsIAtom *aLangGroup,
                             const nsACString& aGenericFamily,
                             nsTArray<nsString>& aListOfFonts);

    void UpdateFontList();

    virtual void ClearLangGroupPrefFonts();

    virtual void GetFontFamilyList(nsTArray<RefPtr<gfxFontFamily> >& aFamilyArray);

    gfxFontEntry*
    SystemFindFontForChar(uint32_t aCh, uint32_t aNextCh,
                          Script aRunScript,
                          const gfxFontStyle* aStyle);

    // Find family(ies) matching aFamily and append to the aOutput array
    // (there may be multiple results in the case of fontconfig aliases, etc).
    // Return true if any match was found and appended, false if none.
    virtual bool
    FindAndAddFamilies(const nsAString& aFamily,
                       nsTArray<gfxFontFamily*>* aOutput,
                       gfxFontStyle* aStyle = nullptr,
                       gfxFloat aDevToCssSize = 1.0);

    gfxFontEntry* FindFontForFamily(const nsAString& aFamily, const gfxFontStyle* aStyle, bool& aNeedsBold);

    // name lookup table methods

    void AddOtherFamilyName(gfxFontFamily *aFamilyEntry, nsAString& aOtherFamilyName);

    void AddFullname(gfxFontEntry *aFontEntry, nsAString& aFullname);

    void AddPostscriptName(gfxFontEntry *aFontEntry, nsAString& aPostscriptName);

    bool NeedFullnamePostscriptNames() { return mExtraNames != nullptr; }

    // pure virtual functions, to be provided by concrete subclasses

    // get the system default font family
    gfxFontFamily* GetDefaultFont(const gfxFontStyle* aStyle);

    // look up a font by name on the host platform
    virtual gfxFontEntry* LookupLocalFont(const nsAString& aFontName,
                                          uint16_t aWeight,
                                          int16_t aStretch,
                                          uint8_t aStyle) = 0;

    // create a new platform font from downloaded data (@font-face)
    // this method is responsible to ensure aFontData is free()'d
    virtual gfxFontEntry* MakePlatformFont(const nsAString& aFontName,
                                           uint16_t aWeight,
                                           int16_t aStretch,
                                           uint8_t aStyle,
                                           const uint8_t* aFontData,
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

    static const gfxFontEntry::ScriptRange sComplexScriptRanges[];

    void GetFontlistInitInfo(uint32_t& aNumInits, uint32_t& aLoaderState) {
        aNumInits = mFontlistInitCount;
        aLoaderState = (uint32_t) mState;
    }

    virtual void
    AddGenericFonts(mozilla::FontFamilyType aGenericType,
                    nsIAtom* aLanguage,
                    nsTArray<gfxFontFamily*>& aFamilyList);

    nsTArray<RefPtr<gfxFontFamily>>*
    GetPrefFontsLangGroup(mozilla::FontFamilyType aGenericType,
                          eFontPrefLang aPrefLang);

    // in some situations, need to make decisions about ambiguous characters, may need to look at multiple pref langs
    void GetLangPrefs(eFontPrefLang aPrefLangs[], uint32_t &aLen, eFontPrefLang aCharLang, eFontPrefLang aPageLang);

    // convert a lang group to enum constant (i.e. "zh-TW" ==> eFontPrefLang_ChineseTW)
    static eFontPrefLang GetFontPrefLangFor(const char* aLang);

    // convert a lang group atom to enum constant
    static eFontPrefLang GetFontPrefLangFor(nsIAtom *aLang);

    // convert an enum constant to a lang group atom
    static nsIAtom* GetLangGroupForPrefLang(eFontPrefLang aLang);

    // convert a enum constant to lang group string (i.e. eFontPrefLang_ChineseTW ==> "zh-TW")
    static const char* GetPrefLangName(eFontPrefLang aLang);

    // map a Unicode range (based on char code) to a font language for Preferences
    static eFontPrefLang GetFontPrefLangFor(uint8_t aUnicodeRange);

    // returns true if a pref lang is CJK
    static bool IsLangCJK(eFontPrefLang aLang);

    // helper method to add a pref lang to an array, if not already in array
    static void AppendPrefLang(eFontPrefLang aPrefLangs[], uint32_t& aLen, eFontPrefLang aAddLang);

    // default serif/sans-serif choice based on font.default.xxx prefs
    mozilla::FontFamilyType
    GetDefaultGeneric(eFontPrefLang aLang);

    // map lang group ==> lang string
    void GetSampleLangForGroup(nsIAtom* aLanguage, nsACString& aLangStr,
                               bool aCheckEnvironment = true);

    // Returns true if the font family whitelist is not empty.
    bool IsFontFamilyWhitelistActive();

    static void FontWhitelistPrefChanged(const char *aPref, void *aClosure) {
        gfxPlatformFontList::PlatformFontList()->UpdateFontList();
    }

protected:
    class MemoryReporter final : public nsIMemoryReporter
    {
        ~MemoryReporter() {}
    public:
        NS_DECL_ISUPPORTS
        NS_DECL_NSIMEMORYREPORTER
    };

    explicit gfxPlatformFontList(bool aNeedFullnamePostscriptNames = true);

    static gfxPlatformFontList *sPlatformFontList;

    // Convenience method to return the first matching family (if any) as found
    // by FindAndAddFamilies().
    gfxFontFamily*
    FindFamily(const nsAString& aFamily, gfxFontStyle* aStyle = nullptr,
               gfxFloat aDevToCssSize = 1.0)
    {
        AutoTArray<gfxFontFamily*,1> families;
        return FindAndAddFamilies(aFamily, &families, aStyle, aDevToCssSize)
               ? families[0] : nullptr;
    }

    // Lookup family name in global family list without substitutions or
    // localized family name lookup. Used for common font fallback families.
    gfxFontFamily* FindFamilyByCanonicalName(const nsAString& aFamily) {
        nsAutoString key;
        gfxFontFamily *familyEntry;
        GenerateFontListKey(aFamily, key);
        if ((familyEntry = mFontFamilies.GetWeak(key))) {
            return CheckFamily(familyEntry);
        }
        return nullptr;
    }

    // returns default font for a given character, null otherwise
    gfxFontEntry* CommonFontFallback(uint32_t aCh, uint32_t aNextCh,
                                     Script aRunScript,
                                     const gfxFontStyle* aMatchStyle,
                                     gfxFontFamily** aMatchedFamily);

    // search fonts system-wide for a given character, null otherwise
    virtual gfxFontEntry* GlobalFontFallback(const uint32_t aCh,
                                             Script aRunScript,
                                             const gfxFontStyle* aMatchStyle,
                                             uint32_t& aCmapCount,
                                             gfxFontFamily** aMatchedFamily);

    // whether system-based font fallback is used or not
    // if system fallback is used, no need to load all cmaps
    virtual bool UsesSystemFallback() { return false; }

    void AppendCJKPrefLangs(eFontPrefLang aPrefLangs[], uint32_t &aLen,
                            eFontPrefLang aCharLang, eFontPrefLang aPageLang);

    // verifies that a family contains a non-zero font count
    gfxFontFamily* CheckFamily(gfxFontFamily *aFamily);

    // initialize localized family names
    void InitOtherFamilyNames();

    // search through font families, looking for a given name, initializing
    // facename lists along the way. first checks all families with names
    // close to face name, then searchs all families if not found.
    gfxFontEntry* SearchFamiliesForFaceName(const nsAString& aFaceName);

    // helper method for finding fullname/postscript names in facename lists
    gfxFontEntry* FindFaceName(const nsAString& aFaceName);

    // look up a font by name, for cases where platform font list
    // maintains explicit mappings of fullname/psname ==> font
    virtual gfxFontEntry* LookupInFaceNameLists(const nsAString& aFontName);

    // commonly used fonts for which the name table should be loaded at startup
    virtual void PreloadNamesList();

    // load the bad underline blacklist from pref.
    void LoadBadUnderlineList();

    void GenerateFontListKey(const nsAString& aKeyName, nsAString& aResult);

    virtual void GetFontFamilyNames(nsTArray<nsString>& aFontFamilyNames);

    nsILanguageAtomService* GetLangService();

    // helper function to map lang to lang group
    nsIAtom* GetLangGroup(nsIAtom* aLanguage);

    // helper method for finding an appropriate lang string
    bool TryLangForGroup(const nsACString& aOSLang, nsIAtom* aLangGroup,
                         nsACString& aLang);

    static const char* GetGenericName(mozilla::FontFamilyType aGenericType);

    // gfxFontInfoLoader overrides, used to load in font cmaps
    virtual void InitLoader();
    virtual bool LoadFontInfo();
    virtual void CleanupLoader();

    // read the loader initialization prefs, and start it
    void GetPrefsAndStartLoader();

    // for font list changes that affect all documents
    void ForceGlobalReflow();

    void RebuildLocalFonts();

    void
    ResolveGenericFontNames(mozilla::FontFamilyType aGenericType,
                            eFontPrefLang aPrefLang,
                            nsTArray<RefPtr<gfxFontFamily>>* aGenericFamilies);

    virtual nsresult InitFontListForPlatform() = 0;

    void ApplyWhitelist();

    typedef nsRefPtrHashtable<nsStringHashKey, gfxFontFamily> FontFamilyTable;
    typedef nsRefPtrHashtable<nsStringHashKey, gfxFontEntry> FontEntryTable;

    // used by memory reporter to accumulate sizes of family names in the table
    static size_t
    SizeOfFontFamilyTableExcludingThis(const FontFamilyTable& aTable,
                                       mozilla::MallocSizeOf aMallocSizeOf);
    static size_t
    SizeOfFontEntryTableExcludingThis(const FontEntryTable& aTable,
                                      mozilla::MallocSizeOf aMallocSizeOf);

    // Platform-specific helper for GetDefaultFont(...).
    virtual gfxFontFamily*
    GetDefaultFontForPlatform(const gfxFontStyle* aStyle) = 0;

    // canonical family name ==> family entry (unique, one name per family entry)
    FontFamilyTable mFontFamilies;

    // other family name ==> family entry (not unique, can have multiple names per
    // family entry, only names *other* than the canonical names are stored here)
    FontFamilyTable mOtherFamilyNames;

    // flag set after InitOtherFamilyNames is called upon first name lookup miss
    bool mOtherFamilyNamesInitialized;

    // flag set after fullname and Postcript name lists are populated
    bool mFaceNameListsInitialized;

    struct ExtraNames {
      ExtraNames() : mFullnames(64), mPostscriptNames(64) {}

      // fullname ==> font entry (unique, one name per font entry)
      FontEntryTable mFullnames;
      // Postscript name ==> font entry (unique, one name per font entry)
      FontEntryTable mPostscriptNames;
    };
    mozilla::UniquePtr<ExtraNames> mExtraNames;

    // face names missed when face name loading takes a long time
    mozilla::UniquePtr<nsTHashtable<nsStringHashKey> > mFaceNamesMissed;

    // localized family names missed when face name loading takes a long time
    mozilla::UniquePtr<nsTHashtable<nsStringHashKey> > mOtherNamesMissed;

    typedef nsTArray<RefPtr<gfxFontFamily>> PrefFontList;
    typedef mozilla::RangedArray<mozilla::UniquePtr<PrefFontList>,
                                 mozilla::eFamily_generic_first,
                                 mozilla::eFamily_generic_count> PrefFontsForLangGroup;
    mozilla::RangedArray<PrefFontsForLangGroup,
                         eFontPrefLang_First,
                         eFontPrefLang_Count> mLangGroupPrefFonts;

    // when system-wide font lookup fails for a character, cache it to skip future searches
    gfxSparseBitSet mCodepointsWithNoFonts;

    // the family to use for U+FFFD fallback, to avoid expensive search every time
    // on pages with lots of problems
    RefPtr<gfxFontFamily> mReplacementCharFallbackFamily;

    nsTHashtable<nsStringHashKey> mBadUnderlineFamilyNames;

    // character map data shared across families
    // contains weak ptrs to cmaps shared by font entry objects
    nsTHashtable<CharMapHashKey> mSharedCmaps;

    // data used as part of the font cmap loading process
    nsTArray<RefPtr<gfxFontFamily> > mFontFamiliesToLoad;
    uint32_t mStartIndex;
    uint32_t mIncrement;
    uint32_t mNumFamilies;

    // xxx - info for diagnosing no default font aborts
    // see bugs 636957, 1070983, 1189129
    uint32_t mFontlistInitCount; // num times InitFontList called

    nsTHashtable<nsPtrHashKey<gfxUserFontSet> > mUserFontSetList;

    nsCOMPtr<nsILanguageAtomService> mLangService;
    nsTArray<uint32_t> mCJKPrefLangs;
    nsTArray<mozilla::FontFamilyType> mDefaultGenericsLangGroup;

    bool mFontFamilyWhitelistActive;
};

#endif /* GFXPLATFORMFONTLIST_H_ */

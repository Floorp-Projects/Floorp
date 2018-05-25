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
#include "mozilla/FontPropertyTypes.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Mutex.h"
#include "mozilla/RangedArray.h"
#include "nsLanguageAtomService.h"

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
    // charMaps are not owned by the shared cmap cache, but it will be notified
    // by gfxCharacterMap::Release() when an entry is about to be deleted
    gfxCharacterMap* MOZ_NON_OWNING_REF mCharMap;
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
    uint32_t mLoaderSize;   // memory used for (platform-specific) loader
};

class gfxUserFontSet;

class gfxPlatformFontList : public gfxFontInfoLoader
{
    friend class InitOtherFamilyNamesRunnable;

public:
    typedef mozilla::StretchRange StretchRange;
    typedef mozilla::SlantStyleRange SlantStyleRange;
    typedef mozilla::WeightRange WeightRange;
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

    virtual void GetFontList(nsAtom *aLangGroup,
                             const nsACString& aGenericFamily,
                             nsTArray<nsString>& aListOfFonts);

    void UpdateFontList();

    virtual void ClearLangGroupPrefFonts();

    virtual void GetFontFamilyList(nsTArray<RefPtr<gfxFontFamily> >& aFamilyArray);

    gfxFontEntry*
    SystemFindFontForChar(uint32_t aCh, uint32_t aNextCh,
                          Script aRunScript,
                          const gfxFontStyle* aStyle);

    // Flags to control optional behaviors in FindAndAddFamilies. The sense
    // of the bit flags have been chosen such that the default parameter of
    // FindFamiliesFlags(0) in FindFamily will give the most commonly-desired
    // behavior, and only a few callsites need to explicitly pass other values.
    enum class FindFamiliesFlags {
        // If set, "other" (e.g. localized) family names should be loaded
        // immediately; if clear, InitOtherFamilyNames is allowed to defer
        // loading to avoid blocking.
        eForceOtherFamilyNamesLoading = 1 << 0,
        
        // If set, FindAndAddFamilies should not check for legacy "styled
        // family" names to add to the font list. This is used to avoid
        // a recursive search when using FindFamily to find a potential base
        // family name for a styled variant.
        eNoSearchForLegacyFamilyNames = 1 << 1,

        // If set, FindAndAddFamilies will not add a missing entry to mOtherNamesMissed
        eNoAddToNamesMissedWhenSearching = 1 << 2
    };

    // Find family(ies) matching aFamily and append to the aOutput array
    // (there may be multiple results in the case of fontconfig aliases, etc).
    // Return true if any match was found and appended, false if none.
    virtual bool
    FindAndAddFamilies(const nsAString& aFamily,
                       nsTArray<gfxFontFamily*>* aOutput,
                       FindFamiliesFlags aFlags,
                       gfxFontStyle* aStyle = nullptr,
                       gfxFloat aDevToCssSize = 1.0);

    gfxFontEntry* FindFontForFamily(const nsAString& aFamily,
                                    const gfxFontStyle* aStyle);

    // name lookup table methods

    void AddOtherFamilyName(gfxFontFamily *aFamilyEntry, nsAString& aOtherFamilyName);

    void AddFullname(gfxFontEntry *aFontEntry, nsAString& aFullname);

    void AddPostscriptName(gfxFontEntry *aFontEntry, nsAString& aPostscriptName);

    bool NeedFullnamePostscriptNames() { return mExtraNames != nullptr; }

    // pure virtual functions, to be provided by concrete subclasses

    // get the system default font family
    gfxFontFamily* GetDefaultFont(const gfxFontStyle* aStyle);

    /**
     * Look up a font by name on the host platform.
     *
     * Note that the style attributes (weight, stretch, style) are NOT used in
     * selecting the platform font, which is looked up by name only; these are
     * values to be recorded in the new font entry.
     */
    virtual gfxFontEntry* LookupLocalFont(const nsAString& aFontName,
                                          WeightRange aWeightForEntry,
                                          StretchRange aStretchForEntry,
                                          SlantStyleRange aStyleForEntry) = 0;

    /**
     * Create a new platform font from downloaded data (@font-face).
     *
     * Note that the style attributes (weight, stretch, style) are NOT related
     * (necessarily) to any values within the font resource itself; these are
     * values to be recorded in the new font entry and used for face selection,
     * in place of whatever inherent style attributes the resource may have.
     *
     * This method takes ownership of the data block passed in as aFontData,
     * and must ensure it is free()'d when no longer required.
     */
    virtual gfxFontEntry* MakePlatformFont(const nsAString& aFontName,
                                           WeightRange aWeightForEntry,
                                           StretchRange aStretchForEntry,
                                           SlantStyleRange aStyleForEntry,
                                           const uint8_t* aFontData,
                                           uint32_t aLength) = 0;

    // get the standard family name on the platform for a given font name
    // (platforms may override, eg Mac)
    virtual bool GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName);

    // get the default font name which is available on the system from
    // font.name-list.*.  if there are no available fonts in the pref,
    // returns nullptr.
    gfxFontFamily* GetDefaultFontFamily(const nsACString& aLangGroup,
                                        const nsACString& aGenericFamily);

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
                    nsAtom* aLanguage,
                    nsTArray<gfxFontFamily*>& aFamilyList);

    nsTArray<RefPtr<gfxFontFamily>>*
    GetPrefFontsLangGroup(mozilla::FontFamilyType aGenericType,
                          eFontPrefLang aPrefLang);

    // in some situations, need to make decisions about ambiguous characters, may need to look at multiple pref langs
    void GetLangPrefs(eFontPrefLang aPrefLangs[], uint32_t &aLen, eFontPrefLang aCharLang, eFontPrefLang aPageLang);

    // convert a lang group to enum constant (i.e. "zh-TW" ==> eFontPrefLang_ChineseTW)
    static eFontPrefLang GetFontPrefLangFor(const char* aLang);

    // convert a lang group atom to enum constant
    static eFontPrefLang GetFontPrefLangFor(nsAtom *aLang);

    // convert an enum constant to a lang group atom
    static nsAtom* GetLangGroupForPrefLang(eFontPrefLang aLang);

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

    // Returns true if the font family whitelist is not empty.
    bool IsFontFamilyWhitelistActive();

    static void FontWhitelistPrefChanged(const char *aPref, void *aClosure);

    bool AddWithLegacyFamilyName(const nsAString& aLegacyName,
                                 gfxFontEntry* aFontEntry);

protected:
    class InitOtherFamilyNamesRunnable : public mozilla::CancelableRunnable
    {
    public:
        InitOtherFamilyNamesRunnable()
            : CancelableRunnable("gfxPlatformFontList::InitOtherFamilyNamesRunnable")
            , mIsCanceled(false)
        {
        }

        NS_IMETHOD Run() override
        {
            if (mIsCanceled) {
                return NS_OK;
            }

            gfxPlatformFontList* fontList = gfxPlatformFontList::PlatformFontList();
            if (!fontList) {
                return NS_OK;
            }

            fontList->InitOtherFamilyNamesInternal(true);

            return NS_OK;
        }

        virtual nsresult Cancel() override
        {
            mIsCanceled = true;

            return NS_OK;
        }

    private:
        bool mIsCanceled;
    };

    class MemoryReporter final : public nsIMemoryReporter
    {
        ~MemoryReporter() {}
    public:
        NS_DECL_ISUPPORTS
        NS_DECL_NSIMEMORYREPORTER
    };

    template<bool ForNameList>
    class PrefNameMaker final : public nsAutoCString
    {
        void Init(const nsACString& aGeneric, const nsACString& aLangGroup)
        {
            Assign(ForNameList ? NS_LITERAL_CSTRING("font.name-list.")
                               : NS_LITERAL_CSTRING("font.name."));
            Append(aGeneric);
            if (!aLangGroup.IsEmpty()) {
                Append('.');
                Append(aLangGroup);
            }
        }

    public:
        PrefNameMaker(const nsACString& aGeneric,
                      const nsACString& aLangGroup)
        {
            Init(aGeneric, aLangGroup);
        }

        PrefNameMaker(const char* aGeneric,
                      const char* aLangGroup)
        {
            Init(nsDependentCString(aGeneric), nsDependentCString(aLangGroup));
        }

        PrefNameMaker(const char* aGeneric,
                      nsAtom* aLangGroup)
        {
            if (aLangGroup) {
                Init(nsDependentCString(aGeneric), nsAtomCString(aLangGroup));
            } else {
                Init(nsDependentCString(aGeneric), nsAutoCString());
            }
        }
    };

    typedef PrefNameMaker<false> NamePref;
    typedef PrefNameMaker<true>  NameListPref;

    explicit gfxPlatformFontList(bool aNeedFullnamePostscriptNames = true);

    static gfxPlatformFontList *sPlatformFontList;

    // Convenience method to return the first matching family (if any) as found
    // by FindAndAddFamilies().
    gfxFontFamily*
    FindFamily(const nsAString& aFamily,
               FindFamiliesFlags aFlags = FindFamiliesFlags(0),
               gfxFontStyle* aStyle = nullptr,
               gfxFloat aDevToCssSize = 1.0)
    {
        AutoTArray<gfxFontFamily*,1> families;
        return FindAndAddFamilies(aFamily,
                                  &families,
                                  aFlags,
                                  aStyle,
                                  aDevToCssSize) ? families[0] : nullptr;
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

    // Search fonts system-wide for a given character, null if not found.
    gfxFontEntry* GlobalFontFallback(const uint32_t aCh,
                                     Script aRunScript,
                                     const gfxFontStyle* aMatchStyle,
                                     uint32_t& aCmapCount,
                                     gfxFontFamily** aMatchedFamily);

    // Platform-specific implementation of global font fallback, if any;
    // this may return nullptr in which case the default cmap-based fallback
    // will be performed.
    virtual gfxFontEntry*
    PlatformGlobalFontFallback(const uint32_t aCh,
                               Script aRunScript,
                               const gfxFontStyle* aMatchStyle,
                               gfxFontFamily** aMatchedFamily)
    {
        return nullptr;
    }

    // whether system-based font fallback is used or not
    // if system fallback is used, no need to load all cmaps
    virtual bool UsesSystemFallback() { return false; }

    void AppendCJKPrefLangs(eFontPrefLang aPrefLangs[], uint32_t &aLen,
                            eFontPrefLang aCharLang, eFontPrefLang aPageLang);

    // verifies that a family contains a non-zero font count
    gfxFontFamily* CheckFamily(gfxFontFamily *aFamily);

    // initialize localized family names
    void InitOtherFamilyNames(bool aDeferOtherFamilyNamesLoading);
    void InitOtherFamilyNamesInternal(bool aDeferOtherFamilyNamesLoading);
    void CancelInitOtherFamilyNamesTask();

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

    // helper function to map lang to lang group
    nsAtom* GetLangGroup(nsAtom* aLanguage);

    static const char* GetGenericName(mozilla::FontFamilyType aGenericType);

    // gfxFontInfoLoader overrides, used to load in font cmaps
    virtual void InitLoader() override;
    virtual bool LoadFontInfo() override;
    virtual void CleanupLoader() override;

    // read the loader initialization prefs, and start it
    void GetPrefsAndStartLoader();

    // for font list changes that affect all documents
    void ForceGlobalReflow() {
        gfxPlatform::ForceGlobalReflow();
    }

    void RebuildLocalFonts();

    void
    ResolveGenericFontNames(mozilla::FontFamilyType aGenericType,
                            eFontPrefLang aPrefLang,
                            nsTArray<RefPtr<gfxFontFamily>>* aGenericFamilies);

    void
    ResolveEmojiFontNames(nsTArray<RefPtr<gfxFontFamily>>* aGenericFamilies);

    void
    GetFontFamiliesFromGenericFamilies(
        nsTArray<nsString>& aGenericFamilies,
        nsAtom* aLangGroup,
        nsTArray<RefPtr<gfxFontFamily>>* aFontFamilies);

    virtual nsresult InitFontListForPlatform() = 0;

    void ApplyWhitelist();

    // Create a new gfxFontFamily of the appropriate subclass for the platform,
    // used when AddWithLegacyFamilyName needs to create a new family.
    virtual gfxFontFamily* CreateFontFamily(const nsAString& aName) const = 0;

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

    // Protects mFontFamilies.
    mozilla::Mutex mFontFamiliesMutex;

    // canonical family name ==> family entry (unique, one name per family entry)
    FontFamilyTable mFontFamilies;

    // other family name ==> family entry (not unique, can have multiple names per
    // family entry, only names *other* than the canonical names are stored here)
    FontFamilyTable mOtherFamilyNames;

    // flag set after InitOtherFamilyNames is called upon first name lookup miss
    bool mOtherFamilyNamesInitialized;

    // The pending InitOtherFamilyNames() task.
    RefPtr<mozilla::CancelableRunnable> mPendingOtherFamilyNameTask;

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
    mozilla::UniquePtr<PrefFontList> mEmojiPrefFont;

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
    uint32_t mNumFamilies;

    // xxx - info for diagnosing no default font aborts
    // see bugs 636957, 1070983, 1189129
    uint32_t mFontlistInitCount; // num times InitFontList called

    nsTHashtable<nsPtrHashKey<gfxUserFontSet> > mUserFontSetList;

    nsLanguageAtomService* mLangService;

    nsTArray<uint32_t> mCJKPrefLangs;
    nsTArray<mozilla::FontFamilyType> mDefaultGenericsLangGroup;

    bool mFontFamilyWhitelistActive;
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(gfxPlatformFontList::FindFamiliesFlags)

#endif /* GFXPLATFORMFONTLIST_H_ */

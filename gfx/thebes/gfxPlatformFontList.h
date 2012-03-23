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
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008-2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonathan Kew <jfkthame@gmail.com>
 *   John Daggett <jdaggett@mozilla.com>
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

// gfxPlatformFontList is an abstract class for the global font list on the system;
// concrete subclasses for each platform implement the actual interface to the system fonts.
// This class exists because we cannot rely on the platform font-finding APIs to behave
// in sensible/similar ways, particularly with rich, complex OpenType families,
// so we do our own font family/style management here instead.

// Much of this is based on the old gfxQuartzFontCache, but adapted for use on all platforms.

struct FontListSizes {
    FontListSizes()
        : mFontListSize(0), mFontTableCacheSize(0), mCharMapsSize(0)
    { }

    size_t mFontListSize; // size of the font list and dependent objects
                          // (font family and face names, etc), but NOT
                          // including the font table cache and the cmaps
    size_t mFontTableCacheSize; // memory used for the gfxFontEntry table caches
    size_t mCharMapsSize; // memory used for cmap coverage info
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

protected:
    class MemoryReporter
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

    // data used as part of the font cmap loading process
    nsTArray<nsRefPtr<gfxFontFamily> > mFontFamiliesToLoad;
    PRUint32 mStartIndex;
    PRUint32 mIncrement;
    PRUint32 mNumFamilies;
};

#endif /* GFXPLATFORMFONTLIST_H_ */

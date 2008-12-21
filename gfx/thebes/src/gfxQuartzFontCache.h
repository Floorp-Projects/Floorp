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
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
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

#ifndef GFXQUARTZFONTCACHE_H_
#define GFXQUARTZFONTCACHE_H_

#include "nsDataHashtable.h"
#include "nsRefPtrHashtable.h"

#include "gfxFontUtils.h"
#include "gfxAtsuiFonts.h"
#include "gfxPlatform.h"

#include <Carbon/Carbon.h>

#include "nsUnicharUtils.h"
#include "nsVoidArray.h"

// used when picking fallback font
struct FontSearch {
    FontSearch(const PRUint32 aCharacter, gfxFont *aFont) :
        ch(aCharacter), fontToMatch(aFont), matchRank(0) {
    }
    const PRUint32 ch;
    gfxFont *fontToMatch;
    PRInt32 matchRank;
    nsRefPtr<MacOSFontEntry> bestMatch;
};

class MacOSFamilyEntry;
class gfxQuartzFontCache;

// a single member of a font family (i.e. a single face, such as Times Italic)
class MacOSFontEntry : public gfxFontEntry
{
public:
    friend class gfxQuartzFontCache;

    // initialize with Apple-type weight [1..14]
    MacOSFontEntry(const nsAString& aPostscriptName, PRInt32 aAppleWeight, PRUint32 aTraits, 
                    MacOSFamilyEntry *aFamily);

    const nsString& FamilyName();

    PRUint32 Traits() { return mTraits; }
    
    ATSUFontID GetFontID();
    nsresult ReadCMAP();

protected:
    // for use with data fonts
    MacOSFontEntry(const nsAString& aPostscriptName, ATSUFontID aFontID,
                   PRUint16 aWeight, PRUint16 aStretch, PRUint32 aItalicStyle,
                   gfxUserFontData *aUserFontData);

    PRUint32 mTraits;
    MacOSFamilyEntry *mFamily;

    ATSUFontID mATSUFontID;
    PRPackedBool mATSUIDInitialized;
};

// helper class for adding other family names back into font cache
class AddOtherFamilyNameFunctor;

// a single font family, referencing one or more faces 
class MacOSFamilyEntry : public gfxFontFamily
{
public:

    friend class gfxQuartzFontCache;

    // name is canonical font family name returned from NSFontManager
    MacOSFamilyEntry(nsAString &aName) :
        gfxFontFamily(aName), mOtherFamilyNamesInitialized(PR_FALSE), mHasOtherFamilyNames(PR_FALSE)
    {}
  
    virtual ~MacOSFamilyEntry() {}
        
    virtual void LocalizedName(nsAString& aLocalizedName);
    virtual PRBool HasOtherFamilyNames();
    
    nsTArray<nsRefPtr<MacOSFontEntry> >& GetFontList() { return mAvailableFonts; }
    
    void AddFontEntry(nsRefPtr<MacOSFontEntry> aFontEntry) {
        mAvailableFonts.AppendElement(aFontEntry);
    }
    
    // decides the right face for a given style, never fails
    // may return a face that doesn't precisely match (e.g. normal face when no italic face exists)
    // aNeedsBold is set to true when bolder face couldn't be found, false otherwise
    MacOSFontEntry* FindFont(const gfxFontStyle* aStyle, PRBool& aNeedsBold);
    
    // iterates over faces looking for a match with a given characters
    // used as part of the font fallback process
    void FindFontForChar(FontSearch *aMatchData);
    
    // read in other family names, if any, and use functor to add each into cache
    virtual void ReadOtherFamilyNames(AddOtherFamilyNameFunctor& aOtherFamilyFunctor);
    
    // search for a specific face using the Postscript name
    MacOSFontEntry* FindFont(const nsAString& aPostscriptName);

    // read in cmaps for all the faces
    void ReadCMAP() {
        PRUint32 i, numFonts = mAvailableFonts.Length();
        for (i = 0; i < numFonts; i++)
            mAvailableFonts[i]->ReadCMAP();
    }

    // set whether this font family is in "bad" underline offset blacklist.
    void SetBadUnderlineFont(PRBool aIsBadUnderlineFont) {
        PRUint32 i, numFonts = mAvailableFonts.Length();
        for (i = 0; i < numFonts; i++)
            mAvailableFonts[i]->mIsBadUnderlineFont = aIsBadUnderlineFont;
    }

protected:
    
    // add font entries into array that match specified traits, returned in array listed by weight
    // i.e. aFontsForWeights[4] ==> pointer to the font entry for a 400-weight face on return
    // returns true if one or more faces found
    PRBool FindFontsWithTraits(gfxFontEntry* aFontsForWeights[], PRUint32 aPosTraitsMask, 
                                PRUint32 aNegTraitsMask);

    PRBool FindWeightsForStyle(gfxFontEntry* aFontsForWeights[], const gfxFontStyle& aFontStyle);

    nsTArray<nsRefPtr<MacOSFontEntry> >  mAvailableFonts;
    PRPackedBool mOtherFamilyNamesInitialized;
    PRPackedBool mHasOtherFamilyNames;
};

// special-case situation where specific faces need to be treated as separate font family
class SingleFaceFamily : public MacOSFamilyEntry
{
public:
    SingleFaceFamily(nsAString &aName) :
        MacOSFamilyEntry(aName)
    {}
    
    virtual ~SingleFaceFamily() {}
    
    virtual void LocalizedName(nsAString& aLocalizedName);
    
    // read in other family names, if any, and use functor to add each into cache
    virtual void ReadOtherFamilyNames(AddOtherFamilyNameFunctor& aOtherFamilyFunctor);
};

class gfxQuartzFontCache : private gfxFontInfoLoader {
public:
    static gfxQuartzFontCache* SharedFontCache() {
        return sSharedFontCache;
    }

    static nsresult Init() {
        NS_ASSERTION(!sSharedFontCache, "What's this doing here?");
        sSharedFontCache = new gfxQuartzFontCache();
        return sSharedFontCache ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    }
    // It's OK to call Shutdown if we never actually started or if 
    // we already shut down.
    static void Shutdown() {
        delete sSharedFontCache;
        sSharedFontCache = nsnull;
    }

    // methods used by gfxPlatformMac
    
    void GetFontList (const nsACString& aLangGroup,
                      const nsACString& aGenericFamily,
                      nsStringArray& aListOfFonts);
    PRBool ResolveFontName(const nsAString& aFontName,
                           nsAString& aResolvedFontName);
    PRBool GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName);
    void UpdateFontList() { InitFontList(); }

    void GetFontFamilyList(nsTArray<nsRefPtr<MacOSFamilyEntry> >& aFamilyArray);

    MacOSFontEntry* FindFontForChar(const PRUint32 aCh, gfxFont *aPrevFont);

    MacOSFamilyEntry* FindFamily(const nsAString& aFamily);
    
    MacOSFontEntry* FindFontForFamily(const nsAString& aFamily, const gfxFontStyle* aStyle, PRBool& aNeedsBold);
    
    MacOSFontEntry* GetDefaultFont(const gfxFontStyle* aStyle, PRBool& aNeedsBold);

    static PRInt32 AppleWeightToCSSWeight(PRInt32 aAppleWeight);
    
    PRBool GetPrefFontFamilyEntries(eFontPrefLang aLangGroup, nsTArray<nsRefPtr<MacOSFamilyEntry> > *array);
    void SetPrefFontFamilyEntries(eFontPrefLang aLangGroup, nsTArray<nsRefPtr<MacOSFamilyEntry> >& array);
    
    void AddOtherFamilyName(MacOSFamilyEntry *aFamilyEntry, nsAString& aOtherFamilyName);

    gfxFontEntry* LookupLocalFont(const nsAString& aFontName);
    
    gfxFontEntry* MakePlatformFont(const gfxFontEntry *aProxyEntry, const PRUint8 *aFontData, PRUint32 aLength);

private:
    static PLDHashOperator FindFontForCharProc(nsStringHashKey::KeyType aKey,
                                               nsRefPtr<MacOSFamilyEntry>& aFamilyEntry,
                                               void* userArg);

    static gfxQuartzFontCache *sSharedFontCache;

    gfxQuartzFontCache();

    // initialize font lists
    void InitFontList();
    void ReadOtherFamilyNamesForFamily(const nsAString& aFamilyName);
    
    // separate initialization for reading in name tables, since this is expensive
    void InitOtherFamilyNames();
    
    // special case font faces treated as font families (set via prefs)
    void InitSingleFaceList();
    
    // commonly used fonts for which the name table should be loaded at startup
    void PreloadNamesList();

    // initialize the bad underline blacklist from pref.
    void InitBadUnderlineList();

    // eliminate faces which have the same ATSUI id
    void EliminateDuplicateFaces(const nsAString& aFamilyName);
                                                             
    // explicitly set font traits for all faces to fixed-pitch
    void SetFixedPitch(const nsAString& aFamilyName);
                                                             
    static PLDHashOperator InitOtherFamilyNamesProc(nsStringHashKey::KeyType aKey,
                                                    nsRefPtr<MacOSFamilyEntry>& aFamilyEntry,
                                                    void* userArg);

    void GenerateFontListKey(const nsAString& aKeyName, nsAString& aResult);
    static void ATSNotification(ATSFontNotificationInfoRef aInfo, void* aUserArg);
    static int PrefChangedCallback(const char *aPrefName, void *closure);

    static PLDHashOperator
        HashEnumFuncForFamilies(nsStringHashKey::KeyType aKey,
                                nsRefPtr<MacOSFamilyEntry>& aFamilyEntry,
                                void* aUserArg);

    // gfxFontInfoLoader overrides, used to load in font cmaps
    virtual void InitLoader();
    virtual PRBool RunLoader();
    virtual void FinishLoader();

    // canonical family name ==> family entry (unique, one name per family entry)
    nsRefPtrHashtable<nsStringHashKey, MacOSFamilyEntry> mFontFamilies;    

    // other family name ==> family entry (not unique, can have multiple names per 
    // family entry, only names *other* than the canonical names are stored here)
    nsRefPtrHashtable<nsStringHashKey, MacOSFamilyEntry> mOtherFamilyNames;    

    // cached pref font lists
    // maps list of family names ==> array of family entries, one per lang group
    nsDataHashtable<nsUint32HashKey, nsTArray<nsRefPtr<MacOSFamilyEntry> > > mPrefFonts;

    // when system-wide font lookup fails for a character, cache it to skip future searches
    gfxSparseBitSet mCodepointsWithNoFonts;

    // flag set after InitOtherFamilyNames is called upon first name lookup miss
    PRPackedBool mOtherFamilyNamesInitialized;

    // data used as part of the font cmap loading process
    nsTArray<nsRefPtr<MacOSFamilyEntry> > mFontFamiliesToLoad;
    PRUint32 mStartIndex;
    PRUint32 mIncrement;
    PRUint32 mNumFamilies;
    
    // keep track of ATS generation to prevent unneeded updates when loading downloaded fonts
    PRUint32 mATSGeneration;
    
    enum {
        kATSGenerationInitial = -1
    };
};

#endif /* GFXQUARTZFONTCACHE_H_ */

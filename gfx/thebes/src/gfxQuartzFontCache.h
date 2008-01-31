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

#include "gfxFontUtils.h"
#include "gfxAtsuiFonts.h"
#include "gfxPlatform.h"

#include <Carbon/Carbon.h>

#include "nsUnicharUtils.h"
#include "nsVoidArray.h"

// used when picking fallback font
struct FontSearch {
    FontSearch(const PRUint32 aCharacter, gfxAtsuiFont *aFont) :
        ch(aCharacter), fontToMatch(aFont), matchRank(0) {
    }
    const PRUint32 ch;
    gfxAtsuiFont *fontToMatch;
    PRInt32 matchRank;
    nsRefPtr<MacOSFontEntry> bestMatch;
};

class MacOSFamilyEntry;

// a single member of a font family (i.e. a single face, such as Times Italic)
class MacOSFontEntry
{
public:
    THEBES_INLINE_DECL_REFCOUNTING(MacOSFontEntry)

    // initialize with Apple-type weight [1..14]
    MacOSFontEntry(const nsAString& aPostscriptName, PRInt32 aAppleWeight, PRUint32 aTraits, 
                    MacOSFamilyEntry *aFamily);  

    const nsString& Name() { return mPostscriptName; }
    const nsString& FamilyName();
    PRInt32 Weight() { return mWeight; }
    PRUint32 Traits() { return mTraits; }
    
    PRBool IsFixedPitch();
    PRBool IsItalicStyle();
    PRBool IsBold();

    ATSUFontID GetFontID();                   
    nsresult ReadCMAP();
    inline PRBool TestCharacterMap(PRUint32 aCh) {
        if ( !mCmapInitialized ) ReadCMAP();
        return mCharacterMap.test(aCh);
    }
        
protected:
    nsString mPostscriptName;
    PRInt32 mWeight; // CSS-type value: [1..9] which map to 100, 200, ..., 900
    PRUint32 mTraits;
    MacOSFamilyEntry *mFamily;

    ATSUFontID mATSUFontID;
    std::bitset<128> mUnicodeRanges;
    gfxSparseBitSet mCharacterMap;
    
    PRPackedBool mCmapInitialized;
    PRPackedBool mATSUIDInitialized;
};

// a single font family, referencing one or more faces 
class MacOSFamilyEntry
{
public:
    THEBES_INLINE_DECL_REFCOUNTING(MacOSFamilyEntry)

    MacOSFamilyEntry(nsString &aName) :
        mName(aName)
    {
    }

    const nsString& Name() { return mName; }
    void AddFontEntry(nsRefPtr<MacOSFontEntry> aFontEntry) {
        mAvailableFonts.AppendElement(aFontEntry);
    }
    
    // decides the right face for a given style, never fails
    // may return a face that doesn't precisely match (e.g. normal face when no italic face exists)
    MacOSFontEntry* FindFont(const gfxFontStyle* aStyle);
    
    // iterates over faces looking for a match with a given characters
    // used as part of the font fallback process
    void FindFontForChar(FontSearch *aMatchData);
    
protected:
    
    // add font entries into array that match specified traits, returned in array listed by weight
    // i.e. aFontsForWeights[4] ==> pointer to the font entry for a 400-weight face on return
    // returns true if one or more faces found
    PRBool FindFontsWithTraits(MacOSFontEntry* aFontsForWeights[], PRUint32 aPosTraitsMask, 
                                PRUint32 aNegTraitsMask);

    // choose font based on CSS font-weight selection rules, never null
    MacOSFontEntry* FindFontWeight(MacOSFontEntry* aFontsForWeights[], const gfxFontStyle* aStyle);
    
    nsString mName;  // canonical font family name returned from NSFontManager
    nsTArray<nsRefPtr<MacOSFontEntry> >  mAvailableFonts;
};


class gfxQuartzFontCache {
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
    void UpdateFontList() { InitFontList(); }


    MacOSFontEntry* FindFontForChar(const PRUint32 aCh, gfxAtsuiFont *aPrevFont);

    MacOSFamilyEntry* FindFamily(const nsAString& aFamily);
    
    MacOSFontEntry* FindFontForFamily(const nsAString& aFamily, const gfxFontStyle* aStyle);
    
    MacOSFontEntry* GetDefaultFont(const gfxFontStyle* aStyle);

    static PRInt32 AppleWeightToCSSWeight(PRInt32 aAppleWeight);
    
    PRBool GetPrefFontFamilyEntries(eFontPrefLang aLangGroup, nsTArray<nsRefPtr<MacOSFamilyEntry> > *array);
    void SetPrefFontFamilyEntries(eFontPrefLang aLangGroup, nsTArray<nsRefPtr<MacOSFamilyEntry> >& array);

private:
    static PLDHashOperator PR_CALLBACK FindFontForCharProc(nsStringHashKey::KeyType aKey,
                                                             nsRefPtr<MacOSFamilyEntry>& aFamilyEntry,
                                                             void* userArg);
    static gfxQuartzFontCache *sSharedFontCache;

    gfxQuartzFontCache();

    void InitFontList();
    
    void GenerateFontListKey(const nsAString& aKeyName, nsAString& aResult);
    static void ATSNotification(ATSFontNotificationInfoRef aInfo, void* aUserArg);
    static int PR_CALLBACK PrefChangedCallback(const char *aPrefName, void *closure);

    static PLDHashOperator PR_CALLBACK
        HashEnumFuncForFamilies(nsStringHashKey::KeyType aKey,
                                nsRefPtr<MacOSFamilyEntry>& aFamilyEntry,
                                void* aUserArg);

    // canonical family name ==> family entry (unique, one name per family entry)
    nsDataHashtable<nsStringHashKey, nsRefPtr<MacOSFamilyEntry> > mFontFamilies;    

    // localized family name ==> family entry (not unique, can have multiple names per 
    // family entry, only names *other* than the canonical names are stored here)
    nsDataHashtable<nsStringHashKey, nsRefPtr<MacOSFamilyEntry> > mLocalizedFamilies;    

    // cached pref font lists
    // maps list of family names ==> array of family entries, one per lang group
    nsDataHashtable<nsUint32HashKey, nsTArray<nsRefPtr<MacOSFamilyEntry> > > mPrefFonts;

};

#endif /* GFXQUARTZFONTCACHE_H_ */

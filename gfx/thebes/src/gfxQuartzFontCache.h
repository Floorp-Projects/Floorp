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

#include "gfxAtsuiFonts.h"

#include "nsUnicharUtils.h"
#include "nsVoidArray.h"

class NSFontManager;
class NSString;
class NSFont;

class FamilyEntry
{
public:
    THEBES_INLINE_DECL_REFCOUNTING(FamilyEntry)

    FamilyEntry(nsString &aName) :
        mName(aName)
    {
    }

    const nsString& Name() { return mName; }
protected:
    nsString mName;
    // XXX we need to add the variables for generic family and lang group.
};

class FontEntry
{
public:
    THEBES_INLINE_DECL_REFCOUNTING(FontEntry)

    FontEntry(nsString &aName) :
        mName(aName), mWeight(0), mTraits(0)
    {
    }

    const nsString& Name() { return mName; }
    PRInt32 Weight() {
        if (!mWeight)
            RealizeWeightAndTraits();
        return mWeight;
    }
    PRUint32 Traits() {
        if (!mWeight)
            RealizeWeightAndTraits();
        return mTraits;
    }
    PRBool IsFixedPitch();
    PRBool IsItalicStyle();
    PRBool IsBold();
    NSFont* GetNSFont(float aSize);

protected:
    void RealizeWeightAndTraits();
    void GetStringForNSString(const NSString *aSrc, nsAString& aDist);
    NSString* GetNSStringForString(const nsAString& aSrc);

    nsString mName;
    PRInt32 mWeight;
    PRUint32 mTraits;
};

class gfxQuartzFontCache {
public:
    static gfxQuartzFontCache* SharedFontCache() {
        if (!sSharedFontCache)
            sSharedFontCache = new gfxQuartzFontCache();
        return sSharedFontCache;
    }

    ATSUFontID FindATSUFontIDForFamilyAndStyle (const nsAString& aFamily,
                                                const gfxFontStyle* aStyle);

    ATSUFontID GetDefaultATSUFontID (const gfxFontStyle* aStyle);

    void GetFontList (const nsACString& aLangGroup,
                      const nsACString& aGenericFamily,
                      nsStringArray& aListOfFonts);
    PRBool ResolveFontName(const nsAString& aFontName,
                           nsAString& aResolvedFontName);
    void UpdateFontList() { InitFontList(); }

    const nsString& GetPostscriptNameForFontID(ATSUFontID fid);

private:
    static gfxQuartzFontCache *sSharedFontCache;

    gfxQuartzFontCache();

    void InitFontList();
    PRBool AppendFontFamily(NSFontManager *aFontManager,
                            NSString *aName, PRBool aNameIsPostscriptName);
    NSFont* FindFontWeight(NSFontManager *aFontManager,
                           FontEntry *aOriginalFont,
                           NSFont *aFont,
                           const gfxFontStyle *aStyle);
    NSFont* FindAnotherWeightMemberFont(NSFontManager *aFontManager,
                                        FontEntry *aOriginalFont,
                                        NSFont *aFont,
                                        const gfxFontStyle *aStyle,
                                        PRBool aBolder);
    void GenerateFontListKey(const nsAString& aKeyName, nsAString& aResult);
    static void ATSNotification(ATSFontNotificationInfoRef aInfo,
                                void* aUserArg);

    static PLDHashOperator PR_CALLBACK
        HashEnumFuncForFamilies(nsStringHashKey::KeyType aKey,
                                nsRefPtr<FamilyEntry>& aFamilyEntry,
                                void* aUserArg);

    ATSUFontID FindFromSystem (const nsAString& aFamily,
                               const gfxFontStyle* aStyle);

    struct FontAndFamilyContainer {
        FontAndFamilyContainer (const nsAString& family, const gfxFontStyle& style)
            : mFamily(family), mStyle(style)
        {
            ToLowerCase(mFamily);
        }

        FontAndFamilyContainer (const FontAndFamilyContainer& other)
            : mFamily(other.mFamily), mStyle(other.mStyle)
        { }

        nsString mFamily;
        gfxFontStyle mStyle;
    };

    struct FontAndFamilyKey : public PLDHashEntryHdr {
        typedef const FontAndFamilyContainer& KeyType;
        typedef const FontAndFamilyContainer* KeyTypePointer;

        FontAndFamilyKey(KeyTypePointer aObj) : mObj(*aObj) { }
        FontAndFamilyKey(const FontAndFamilyKey& other) : mObj(other.mObj) { }
        ~FontAndFamilyKey() { }

        KeyType GetKey() const { return mObj; }

        PRBool KeyEquals(KeyTypePointer aKey) const {
            return
                aKey->mFamily.Equals(mObj.mFamily) &&
                aKey->mStyle.Equals(mObj.mStyle);
        }

        static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
        static PLDHashNumber HashKey(KeyTypePointer aKey) {
            return HashString(aKey->mFamily);
        }
        enum { ALLOW_MEMMOVE = PR_FALSE };
    private:
        const FontAndFamilyContainer mObj;
    };

    nsDataHashtable<FontAndFamilyKey, ATSUFontID> mCache;
    // The keys are the localized family names of cocoa.
    nsDataHashtable<nsStringHashKey, nsRefPtr<FamilyEntry> > mFamilies;
    // The keys are the localized postscript names.
    nsDataHashtable<nsStringHashKey, nsRefPtr<FontEntry> > mPostscriptFonts;
    // The keys are ATSUI font ID.
    nsDataHashtable<nsUint32HashKey, nsRefPtr<FontEntry> > mFontIDTable;
    // The keys are family names that is cached from all fonts.
    // But at caching time, we cannot resolve to actual font.
    // Therefore, the datas are basic family name of the font.
    // You can resolve the basic family name to |postscript names| with
    // |mFontIDTable|. See |ResolveFontName|.
    nsDataHashtable<nsStringHashKey, nsString> mAppleFamilyNames;
    // The keys are all family names (including alias family names).
    // The datas are postscript name, therefore,
    // you can resolve |all family names| to |postscript nams|
    nsDataHashtable<nsStringHashKey, nsString> mAllFamilyNames;
    // The keys are all font names (including alias font names).
    // The datas are postscript name, therefore,
    // you can resolve |all font names| to |postscript name|
    nsDataHashtable<nsStringHashKey, nsString> mAllFontNames;
    // Note that |mAllFamilyNames| and |mAllFontNames| can have same key
    // for different fonts. Because a family name shared in one or more fonts.
    // So, if you want to resolve the name, you MUST check as following order:
    // 1. mPostscriptFont
    // 2. mAllFontNames
    // 3. mAllFamilyNames

    // The non-existing font names are always lowercased.
    nsStringArray mNonExistingFonts;
};

#endif /* GFXQUARTZFONTCACHE_H_ */

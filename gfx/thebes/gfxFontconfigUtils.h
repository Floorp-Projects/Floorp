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
 * The Original Code is Mozilla Japan code.
 *
 * The Initial Developer of the Original Code is Mozilla Japan.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   Karl Tomlinson <karlt+@karlt.net>, Mozilla Corporation
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

#ifndef GFX_FONTCONFIG_UTILS_H
#define GFX_FONTCONFIG_UTILS_H

#include "gfxPlatform.h"

#include "nsAutoRef.h"
#include "nsTArray.h"
#include "nsTHashtable.h"
#include "nsISupportsImpl.h"

#include <fontconfig/fontconfig.h>


NS_SPECIALIZE_TEMPLATE
class nsAutoRefTraits<FcPattern> : public nsPointerRefTraits<FcPattern>
{
public:
    static void Release(FcPattern *ptr) { FcPatternDestroy(ptr); }
    static void AddRef(FcPattern *ptr) { FcPatternReference(ptr); }
};

NS_SPECIALIZE_TEMPLATE
class nsAutoRefTraits<FcFontSet> : public nsPointerRefTraits<FcFontSet>
{
public:
    static void Release(FcFontSet *ptr) { FcFontSetDestroy(ptr); }
};

NS_SPECIALIZE_TEMPLATE
class nsAutoRefTraits<FcCharSet> : public nsPointerRefTraits<FcCharSet>
{
public:
    static void Release(FcCharSet *ptr) { FcCharSetDestroy(ptr); }
};

class gfxIgnoreCaseCStringComparator
{
  public:
    PRBool Equals(const nsACString& a, const nsACString& b) const
    {
      return nsCString(a).Equals(b, nsCaseInsensitiveCStringComparator());
    }

    PRBool LessThan(const nsACString& a, const nsACString& b) const
    { 
      return a < b;
    }
};

class gfxFontNameList : public nsTArray<nsString>
{
public:
    NS_INLINE_DECL_REFCOUNTING(gfxFontNameList)
    PRBool Exists(nsAString& aName);
};

class gfxFontconfigUtils {
public:
    gfxFontconfigUtils();

    static gfxFontconfigUtils* GetFontconfigUtils() {
        if (!sUtils)
            sUtils = new gfxFontconfigUtils();
        return sUtils;
    }

    static void Shutdown();

    nsresult GetFontList(nsIAtom *aLangGroup,
                         const nsACString& aGenericFamily,
                         nsTArray<nsString>& aListOfFonts);

    nsresult UpdateFontList();

    nsresult ResolveFontName(const nsAString& aFontName,
                             gfxPlatform::FontResolverCallback aCallback,
                             void *aClosure, PRBool& aAborted);

    nsresult GetStandardFamilyName(const nsAString& aFontName, nsAString& aFamilyName);

    const nsTArray< nsCountedRef<FcPattern> >&
    GetFontsForFamily(const FcChar8 *aFamilyName);

    const nsTArray< nsCountedRef<FcPattern> >&
    GetFontsForFullname(const FcChar8 *aFullname);

    // Returns the best support that any font offers for |aLang|. 
    FcLangResult GetBestLangSupport(const FcChar8 *aLang);
    // Returns the fonts offering this best level of support.
    const nsTArray< nsCountedRef<FcPattern> >&
    GetFontsForLang(const FcChar8 *aLang);

    // Retuns the language support for a fontconfig font pattern
    static FcLangResult GetLangSupport(FcPattern *aFont, const FcChar8 *aLang);

    // Conversions between FcChar8*, which is unsigned char*,
    // and (signed) char*, that check the type of the argument.
    static const FcChar8 *ToFcChar8(const char *aCharPtr)
    {
        return reinterpret_cast<const FcChar8*>(aCharPtr);
    }
    static const FcChar8 *ToFcChar8(const nsCString& aCString)
    {
        return ToFcChar8(aCString.get());
    }
    static const char *ToCString(const FcChar8 *aChar8Ptr)
    {
        return reinterpret_cast<const char*>(aChar8Ptr);
    }

    static PRUint8 FcSlantToThebesStyle(int aFcSlant);
    static PRUint8 GetThebesStyle(FcPattern *aPattern); // slant
    static PRUint16 GetThebesWeight(FcPattern *aPattern);
    static PRInt16 GetThebesStretch(FcPattern *aPattern);

    static int GetFcSlant(const gfxFontStyle& aFontStyle);
    // Returns a precise FC_WEIGHT from |aBaseWeight|,
    // which is a CSS absolute weight / 100.
    static int FcWeightForBaseWeight(PRInt8 aBaseWeight);

    static int FcWidthForThebesStretch(PRInt16 aStretch);

    static PRBool GetFullnameFromFamilyAndStyle(FcPattern *aFont,
                                                nsACString *aFullname);

    // This doesn't consider which faces exist, and so initializes the pattern
    // using a guessed weight, and doesn't consider sizeAdjust.
    static nsReturnRef<FcPattern>
    NewPattern(const nsTArray<nsString>& aFamilies,
               const gfxFontStyle& aFontStyle, const char *aLang);

    /**
     * @param aLangGroup [in] a Mozilla langGroup.
     * @param aFcLang [out] returns a language suitable for fontconfig
     *        matching |aLangGroup| or an empty string if no match is found.
     */
    static void GetSampleLangForGroup(nsIAtom *aLangGroup,
                                      nsACString *aFcLang);

protected:
    // Base class for hash table entries with case-insensitive FcChar8
    // string keys.
    class FcStrEntryBase : public PLDHashEntryHdr {
    public:
        typedef const FcChar8 *KeyType;
        typedef const FcChar8 *KeyTypePointer;

        static KeyTypePointer KeyToPointer(KeyType aKey) { return aKey; }
        // Case-insensitive hash.
        //
        // fontconfig always ignores case of ASCII characters in family
        // names and languages, but treatment of whitespace in families is
        // not consistent.  FcFontSort and FcFontMatch ignore whitespace
        // except for whitespace in the first character, while FcFontList
        // and config subsitution tests require whitespace to match
        // exactly.  CSS 2.1 implies that whitespace is important in the
        // font-family property.  FcStrCmpIgnoreCase considers whitespace
        // important.
        static PLDHashNumber HashKey(const FcChar8 *aKey) {
            PRUint32 hash = 0;
            for (const FcChar8 *c = aKey; *c != '\0'; ++c) {
                hash = PR_ROTATE_LEFT32(hash, 3) ^ FcToLower(*c);
            }
            return hash;
        }
        enum { ALLOW_MEMMOVE = PR_TRUE };
    };

public:
    // Hash entry with a dependent const FcChar8* pointer to an external
    // string for a key (and no data).  The user must ensure that the string
    // associated with the pointer is not destroyed.  This entry type is
    // useful for family name keys as the family name string is held in the
    // font pattern.
    class DepFcStrEntry : public FcStrEntryBase {
    public:
        // When constructing a new entry in the hashtable, the key is left
        // NULL.  The caller of PutEntry() must fill in mKey when NULL.  This
        // provides a mechanism for the caller of PutEntry() to determine
        // whether the entry has been initialized.
        DepFcStrEntry(KeyTypePointer aName)
            : mKey(NULL) { }

        DepFcStrEntry(const DepFcStrEntry& toCopy)
            : mKey(toCopy.mKey) { }

        PRBool KeyEquals(KeyTypePointer aKey) const {
            return FcStrCmpIgnoreCase(aKey, mKey) == 0;
        }

        const FcChar8 *mKey;
    };

    // Hash entry that uses a copy of an FcChar8 string to store the key.
    // This entry type is useful for language keys, as languages are usually
    // not stored as strings in font patterns.
    class CopiedFcStrEntry : public FcStrEntryBase {
    public:
        // When constructing a new entry in the hashtable, the key is void.
        // The caller of PutEntry() must call InitKey() when IsKeyInitialized()
        // returns false.  This provides a mechanism for the caller of
        // PutEntry() to determine whether the entry has been initialized.
        CopiedFcStrEntry(KeyTypePointer aName) {
            mKey.SetIsVoid(PR_TRUE);
        }

        CopiedFcStrEntry(const CopiedFcStrEntry& toCopy)
            : mKey(toCopy.mKey) { }

        PRBool KeyEquals(KeyTypePointer aKey) const {
            return FcStrCmpIgnoreCase(aKey, ToFcChar8(mKey)) == 0;
        }

        PRBool IsKeyInitialized() { return !mKey.IsVoid(); }
        void InitKey(const FcChar8* aKey) { mKey.Assign(ToCString(aKey)); }

    private:
        nsCString mKey;
    };

protected:
    class FontsByFcStrEntry : public DepFcStrEntry {
    public:
        FontsByFcStrEntry(KeyTypePointer aName)
            : DepFcStrEntry(aName) { }

        FontsByFcStrEntry(const FontsByFcStrEntry& toCopy)
            : DepFcStrEntry(toCopy), mFonts(toCopy.mFonts) { }

        PRBool AddFont(FcPattern *aFont) {
            return mFonts.AppendElement(aFont) != nsnull;
        }
        const nsTArray< nsCountedRef<FcPattern> >& GetFonts() {
            return mFonts;
        }
    private:
        nsTArray< nsCountedRef<FcPattern> > mFonts;
    };

    // FontsByFullnameEntry is similar to FontsByFcStrEntry (used for
    // mFontsByFamily) except for two differences:
    //
    // * The font does not always contain a single string for the fullname, so
    //   the key is sometimes a combination of family and style.
    //
    // * There is usually only one font.
    class FontsByFullnameEntry : public DepFcStrEntry {
    public:
        // When constructing a new entry in the hashtable, the key is left
        // NULL.  The caller of PutEntry() is must fill in mKey when adding
        // the first font if the key is not derived from the family and style.
        // If the key is derived from family and style, a font must be added.
        FontsByFullnameEntry(KeyTypePointer aName)
            : DepFcStrEntry(aName) { }

        FontsByFullnameEntry(const FontsByFullnameEntry& toCopy)
            : DepFcStrEntry(toCopy), mFonts(toCopy.mFonts) { }

        PRBool KeyEquals(KeyTypePointer aKey) const;

        PRBool AddFont(FcPattern *aFont) {
            return mFonts.AppendElement(aFont) != nsnull;
        }
        const nsTArray< nsCountedRef<FcPattern> >& GetFonts() {
            return mFonts;
        }

        // Don't memmove the nsAutoTArray.
        enum { ALLOW_MEMMOVE = PR_FALSE };
    private:
        // There is usually only one font, but sometimes more.
        nsAutoTArray<nsCountedRef<FcPattern>,1> mFonts;
    };

    class LangSupportEntry : public CopiedFcStrEntry {
    public:
        LangSupportEntry(KeyTypePointer aName)
            : CopiedFcStrEntry(aName) { }

        LangSupportEntry(const LangSupportEntry& toCopy)
            : CopiedFcStrEntry(toCopy), mSupport(toCopy.mSupport) { }

        FcLangResult mSupport;
        nsTArray< nsCountedRef<FcPattern> > mFonts;
    };

    static gfxFontconfigUtils* sUtils;

    PRBool IsExistingFamily(const nsCString& aFamilyName);

    nsresult GetFontListInternal(nsTArray<nsCString>& aListOfFonts,
                                 nsIAtom *aLangGroup);
    nsresult UpdateFontListInternal(PRBool aForce = PR_FALSE);

    void AddFullnameEntries();

    LangSupportEntry *GetLangSupportEntry(const FcChar8 *aLang,
                                          PRBool aWithFonts);

    // mFontsByFamily and mFontsByFullname contain entries only for families
    // and fullnames for which there are fonts.
    nsTHashtable<FontsByFcStrEntry> mFontsByFamily;
    nsTHashtable<FontsByFullnameEntry> mFontsByFullname;
    // mLangSupportTable contains an entry for each language that has been
    // looked up through GetLangSupportEntry, even when the language is not
    // supported.
    nsTHashtable<LangSupportEntry> mLangSupportTable;
    const nsTArray< nsCountedRef<FcPattern> > mEmptyPatternArray;

    nsTArray<nsCString> mAliasForMultiFonts;

    FcConfig *mLastConfig;
};

#endif /* GFX_FONTCONFIG_UTILS_H */

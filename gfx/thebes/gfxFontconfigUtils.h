/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FONTCONFIG_UTILS_H
#define GFX_FONTCONFIG_UTILS_H

#include "gfxPlatform.h"

#include "mozilla/MathAlgorithms.h"
#include "nsAutoRef.h"
#include "nsTArray.h"
#include "nsTHashtable.h"
#include "nsISupportsImpl.h"
#include "gfxFT2FontBase.h"

#include <fontconfig/fontconfig.h>


template <>
class nsAutoRefTraits<FcPattern> : public nsPointerRefTraits<FcPattern>
{
public:
    static void Release(FcPattern *ptr) { FcPatternDestroy(ptr); }
    static void AddRef(FcPattern *ptr) { FcPatternReference(ptr); }
};

template <>
class nsAutoRefTraits<FcFontSet> : public nsPointerRefTraits<FcFontSet>
{
public:
    static void Release(FcFontSet *ptr) { FcFontSetDestroy(ptr); }
};

template <>
class nsAutoRefTraits<FcCharSet> : public nsPointerRefTraits<FcCharSet>
{
public:
    static void Release(FcCharSet *ptr) { FcCharSetDestroy(ptr); }
};

class gfxIgnoreCaseCStringComparator
{
  public:
    bool Equals(const nsACString& a, const nsACString& b) const
    {
      return nsCString(a).Equals(b, nsCaseInsensitiveCStringComparator());
    }

    bool LessThan(const nsACString& a, const nsACString& b) const
    { 
      return a < b;
    }
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

    static uint8_t FcSlantToThebesStyle(int aFcSlant);
    static uint8_t GetThebesStyle(FcPattern *aPattern); // slant
    static uint16_t GetThebesWeight(FcPattern *aPattern);
    static int16_t GetThebesStretch(FcPattern *aPattern);

    static int GetFcSlant(const gfxFontStyle& aFontStyle);
    // Returns a precise FC_WEIGHT from |aBaseWeight|,
    // which is a CSS absolute weight / 100.
    static int FcWeightForBaseWeight(int8_t aBaseWeight);

    static int FcWidthForThebesStretch(int16_t aStretch);

    static bool GetFullnameFromFamilyAndStyle(FcPattern *aFont,
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
            uint32_t hash = 0;
            for (const FcChar8 *c = aKey; *c != '\0'; ++c) {
                hash = mozilla::RotateLeft(hash, 3) ^ FcToLower(*c);
            }
            return hash;
        }
        enum { ALLOW_MEMMOVE = true };
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
        // nullptr.  The caller of PutEntry() must fill in mKey when nullptr.
        // This provides a mechanism for the caller of PutEntry() to determine
        // whether the entry has been initialized.
        explicit DepFcStrEntry(KeyTypePointer aName)
            : mKey(nullptr) { }

        DepFcStrEntry(const DepFcStrEntry& toCopy)
            : mKey(toCopy.mKey) { }

        bool KeyEquals(KeyTypePointer aKey) const {
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
        explicit CopiedFcStrEntry(KeyTypePointer aName) {
            mKey.SetIsVoid(true);
        }

        CopiedFcStrEntry(const CopiedFcStrEntry& toCopy)
            : mKey(toCopy.mKey) { }

        bool KeyEquals(KeyTypePointer aKey) const {
            return FcStrCmpIgnoreCase(aKey, ToFcChar8(mKey)) == 0;
        }

        bool IsKeyInitialized() { return !mKey.IsVoid(); }
        void InitKey(const FcChar8* aKey) { mKey.Assign(ToCString(aKey)); }

    private:
        nsCString mKey;
    };

protected:
    class FontsByFcStrEntry : public DepFcStrEntry {
    public:
        explicit FontsByFcStrEntry(KeyTypePointer aName)
            : DepFcStrEntry(aName) { }

        FontsByFcStrEntry(const FontsByFcStrEntry& toCopy)
            : DepFcStrEntry(toCopy), mFonts(toCopy.mFonts) { }

        bool AddFont(FcPattern *aFont) {
            return mFonts.AppendElement(aFont) != nullptr;
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
        // nullptr.  The caller of PutEntry() is must fill in mKey when adding
        // the first font if the key is not derived from the family and style.
        // If the key is derived from family and style, a font must be added.
        explicit FontsByFullnameEntry(KeyTypePointer aName)
            : DepFcStrEntry(aName) { }

        FontsByFullnameEntry(const FontsByFullnameEntry& toCopy)
            : DepFcStrEntry(toCopy), mFonts(toCopy.mFonts) { }

        bool KeyEquals(KeyTypePointer aKey) const;

        bool AddFont(FcPattern *aFont) {
            return mFonts.AppendElement(aFont) != nullptr;
        }
        const nsTArray< nsCountedRef<FcPattern> >& GetFonts() {
            return mFonts;
        }

        // Don't memmove the AutoTArray.
        enum { ALLOW_MEMMOVE = false };
    private:
        // There is usually only one font, but sometimes more.
        AutoTArray<nsCountedRef<FcPattern>,1> mFonts;
    };

    class LangSupportEntry : public CopiedFcStrEntry {
    public:
        explicit LangSupportEntry(KeyTypePointer aName)
            : CopiedFcStrEntry(aName) { }

        LangSupportEntry(const LangSupportEntry& toCopy)
            : CopiedFcStrEntry(toCopy), mSupport(toCopy.mSupport) { }

        FcLangResult mSupport;
        nsTArray< nsCountedRef<FcPattern> > mFonts;
    };

    static gfxFontconfigUtils* sUtils;

    bool IsExistingFamily(const nsCString& aFamilyName);

    nsresult GetFontListInternal(nsTArray<nsCString>& aListOfFonts,
                                 nsIAtom *aLangGroup);
    nsresult UpdateFontListInternal(bool aForce = false);

    void AddFullnameEntries();

    LangSupportEntry *GetLangSupportEntry(const FcChar8 *aLang,
                                          bool aWithFonts);

    // mFontsByFamily and mFontsByFullname contain entries only for families
    // and fullnames for which there are fonts.
    nsTHashtable<FontsByFcStrEntry> mFontsByFamily;
    nsTHashtable<FontsByFullnameEntry> mFontsByFullname;
    // mLangSupportTable contains an entry for each language that has been
    // looked up through GetLangSupportEntry, even when the language is not
    // supported.
    nsTHashtable<LangSupportEntry> mLangSupportTable;
    const nsTArray< nsCountedRef<FcPattern> > mEmptyPatternArray;

    FcConfig *mLastConfig;

#ifdef MOZ_BUNDLED_FONTS
    void      ActivateBundledFonts();

    nsCString mBundledFontsPath;
    bool      mBundledFontsInitialized;
#endif
};

class gfxFontconfigFontBase : public gfxFT2FontBase {
public:
    gfxFontconfigFontBase(cairo_scaled_font_t *aScaledFont,
                          FcPattern *aPattern,
                          gfxFontEntry *aFontEntry,
                          const gfxFontStyle *aFontStyle);

    virtual FontType GetType() const override { return FONT_TYPE_FONTCONFIG; }
    virtual FcPattern *GetPattern() const { return mPattern; }

private:
    nsCountedRef<FcPattern> mPattern;
};

#endif /* GFX_FONTCONFIG_UTILS_H */

/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FONT_FAMILY_LIST_H
#define GFX_FONT_FAMILY_LIST_H

#include "nsDebug.h"
#include "nsISupportsImpl.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsTArray.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/NotNull.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {

/**
 * type of font family name, either a name (e.g. Helvetica) or a
 * generic (e.g. serif, sans-serif), with the ability to distinguish
 * between unquoted and quoted names for serializaiton
 */ 

enum FontFamilyType : uint32_t {
  eFamily_none = 0,  // used when finding generics

  // explicitly named font family (e.g. Helvetica)
  eFamily_named,
  eFamily_named_quoted,

  // generics
  eFamily_serif,         // pref font code relies on this ordering!!!
  eFamily_sans_serif,
  eFamily_monospace,
  eFamily_cursive,
  eFamily_fantasy,

  // special
  eFamily_moz_variable,
  eFamily_moz_fixed,
  eFamily_moz_emoji,

  eFamily_generic_first = eFamily_serif,
  eFamily_generic_last = eFamily_fantasy,
  eFamily_generic_count = (eFamily_fantasy - eFamily_serif + 1)
};

enum QuotedName { eQuotedName, eUnquotedName };

/**
 * font family name, a string for the name if not a generic and
 * a font type indicated named family or which generic family
 */

struct FontFamilyName final {
    FontFamilyName()
        : mType(eFamily_named)
    {}

    // named font family - e.g. Helvetica
    explicit FontFamilyName(const nsAString& aFamilyName,
                            QuotedName aQuoted = eUnquotedName) {
        mType = (aQuoted == eQuotedName) ? eFamily_named_quoted : eFamily_named;
        mName = aFamilyName;
    }

    // generic font family - e.g. sans-serif
    explicit FontFamilyName(FontFamilyType aType) {
        NS_ASSERTION(aType != eFamily_named &&
                     aType != eFamily_named_quoted &&
                     aType != eFamily_none,
                     "expected a generic font type");
        mName.Truncate();
        mType = aType;
    }

    FontFamilyName(const FontFamilyName& aCopy) {
        mType = aCopy.mType;
        mName = aCopy.mName;
    }

    bool IsNamed() const {
        return mType == eFamily_named || mType == eFamily_named_quoted;
    }

    bool IsGeneric() const {
        return !IsNamed();
    }

    void AppendToString(nsAString& aFamilyList, bool aQuotes = true) const {
        switch (mType) {
            case eFamily_named:
                aFamilyList.Append(mName);
                break;
            case eFamily_named_quoted:
                if (aQuotes) {
                    aFamilyList.Append('"');
                }
                aFamilyList.Append(mName);
                if (aQuotes) {
                    aFamilyList.Append('"');
                }
                break;
            case eFamily_serif:
                aFamilyList.AppendLiteral("serif");
                break;
            case eFamily_sans_serif:
                aFamilyList.AppendLiteral("sans-serif");
                break;
            case eFamily_monospace:
                aFamilyList.AppendLiteral("monospace");
                break;
            case eFamily_cursive:
                aFamilyList.AppendLiteral("cursive");
                break;
            case eFamily_fantasy:
                aFamilyList.AppendLiteral("fantasy");
                break;
            case eFamily_moz_fixed:
                aFamilyList.AppendLiteral("-moz-fixed");
                break;
            default:
                break;
        }
    }

    // helper method that converts generic names to the right enum value
    static FontFamilyName
    Convert(const nsAString& aFamilyOrGenericName) {
        // should only be passed a single font - not entirely correct, a family
        // *could* have a comma in it but in practice never does so
        // for debug purposes this is fine
        NS_ASSERTION(aFamilyOrGenericName.FindChar(',') == -1,
                     "Convert method should only be passed a single family name");

        FontFamilyType genericType = eFamily_none;
        if (aFamilyOrGenericName.LowerCaseEqualsLiteral("serif")) {
            genericType = eFamily_serif;
        } else if (aFamilyOrGenericName.LowerCaseEqualsLiteral("sans-serif")) {
            genericType = eFamily_sans_serif;
        } else if (aFamilyOrGenericName.LowerCaseEqualsLiteral("monospace")) {
            genericType = eFamily_monospace;
        } else if (aFamilyOrGenericName.LowerCaseEqualsLiteral("cursive")) {
            genericType = eFamily_cursive;
        } else if (aFamilyOrGenericName.LowerCaseEqualsLiteral("fantasy")) {
            genericType = eFamily_fantasy;
        } else if (aFamilyOrGenericName.LowerCaseEqualsLiteral("-moz-fixed")) {
            genericType = eFamily_moz_fixed;
        } else {
            return FontFamilyName(aFamilyOrGenericName, eUnquotedName);
        }

        return FontFamilyName(genericType);
    }

    // memory reporting
    size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
        return mName.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
    }

    FontFamilyType mType;
    nsString       mName; // empty if mType != eFamily_named
};

inline bool
operator==(const FontFamilyName& a, const FontFamilyName& b) {
    return a.mType == b.mType && a.mName == b.mName;
}

/**
 * A refcounted array of FontFamilyNames.  We use this to store the specified
 * value (in Servo) and the computed value (in both Gecko and Servo) of the
 * font-family property.
 */
class SharedFontList
{
public:
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SharedFontList);

    SharedFontList()
    {
    }

    explicit SharedFontList(FontFamilyType aGenericType)
        : mNames { FontFamilyName(aGenericType) }
    {
    }

    SharedFontList(const nsAString& aFamilyName, QuotedName aQuoted)
        : mNames { FontFamilyName(aFamilyName, aQuoted) }
    {
    }

    explicit SharedFontList(const FontFamilyName& aName)
        : mNames { aName }
    {
    }

    explicit SharedFontList(nsTArray<FontFamilyName>&& aNames)
        : mNames(Move(aNames))
    {
    }

    FontFamilyType FirstGeneric() const
    {
        for (const FontFamilyName& name : mNames) {
            if (name.IsGeneric()) {
                return name.mType;
            }
        }
        return eFamily_none;
    }

    bool HasGeneric() const
    {
        return FirstGeneric() != eFamily_none;
    }

    size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
    {
      size_t n = 0;
      n += aMallocSizeOf(this);
      n += mNames.ShallowSizeOfExcludingThis(aMallocSizeOf);
      for (const FontFamilyName& name : mNames) {
          n += name.SizeOfExcludingThis(aMallocSizeOf);
      }
      return n;
    }

    size_t SizeOfIncludingThisIfUnshared(MallocSizeOf aMallocSizeOf) const
    {
        size_t n = 0;
        if (mRefCnt.get() == 1) {
          n += SizeOfIncludingThis(aMallocSizeOf);
        }
        return n;
    }

    const nsTArray<FontFamilyName> mNames;

    static void Initialize();
    static void Shutdown();
    static StaticRefPtr<SharedFontList> sEmpty;

private:
    ~SharedFontList() = default;
};

/**
 * font family list, array of font families and a default font type.
 * font family names are either named strings or generics. the default
 * font type is used to preserve the variable font fallback behavior
 */
class FontFamilyList {
public:
    FontFamilyList()
        : mFontlist(WrapNotNull(SharedFontList::sEmpty.get()))
        , mDefaultFontType(eFamily_none)
    {
    }

    explicit FontFamilyList(FontFamilyType aGenericType)
        : mFontlist(MakeNotNull<SharedFontList*>(aGenericType))
        , mDefaultFontType(eFamily_none)
    {
    }

    FontFamilyList(const nsAString& aFamilyName,
                   QuotedName aQuoted)
        : mFontlist(MakeNotNull<SharedFontList*>(aFamilyName, aQuoted))
        , mDefaultFontType(eFamily_none)
    {
    }

    explicit FontFamilyList(const FontFamilyName& aName)
        : mFontlist(MakeNotNull<SharedFontList*>(aName))
        , mDefaultFontType(eFamily_none)
    {
    }

    explicit FontFamilyList(nsTArray<FontFamilyName>&& aNames)
        : mFontlist(MakeNotNull<SharedFontList*>(Move(aNames)))
    {
    }

    FontFamilyList(const FontFamilyList& aOther)
        : mFontlist(aOther.mFontlist)
        , mDefaultFontType(aOther.mDefaultFontType)
    {
    }

    explicit FontFamilyList(NotNull<SharedFontList*> aFontList)
        : mFontlist(aFontList)
        , mDefaultFontType(eFamily_none)
    {
    }

    void SetFontlist(nsTArray<FontFamilyName>&& aNames)
    {
        mFontlist = MakeNotNull<SharedFontList*>(Move(aNames));
    }

    void SetFontlist(NotNull<SharedFontList*> aFontlist)
    {
        mFontlist = aFontlist;
    }

    uint32_t Length() const {
        return mFontlist->mNames.Length();
    }

    bool IsEmpty() const {
        return mFontlist->mNames.IsEmpty();
    }

    NotNull<SharedFontList*> GetFontlist() const {
        return mFontlist;
    }

    bool Equals(const FontFamilyList& aFontlist) const {
        return (mFontlist == aFontlist.mFontlist ||
                mFontlist->mNames == aFontlist.mFontlist->mNames) &&
               mDefaultFontType == aFontlist.mDefaultFontType;
    }

    FontFamilyType FirstGeneric() const {
        return mFontlist->FirstGeneric();
    }

    bool HasGeneric() const
    {
        return mFontlist->HasGeneric();
    }

    bool HasDefaultGeneric() const {
        for (const FontFamilyName& name : mFontlist->mNames) {
            if (name.mType == mDefaultFontType) {
                return true;
            }
        }
        return false;
    }

    // Find the first generic (but ignoring cursive and fantasy, as they are
    // rarely configured in any useful way) in the list.
    // If found, move it to the start and return true; else return false.
    bool PrioritizeFirstGeneric() {
        uint32_t len = mFontlist->mNames.Length();
        for (uint32_t i = 0; i < len; i++) {
            const FontFamilyName name = mFontlist->mNames[i];
            if (name.IsGeneric()) {
                if (name.mType == eFamily_cursive ||
                    name.mType == eFamily_fantasy) {
                    continue;
                }
                if (i > 0) {
                    nsTArray<FontFamilyName> names;
                    names.AppendElements(mFontlist->mNames);
                    names.RemoveElementAt(i);
                    names.InsertElementAt(0, name);
                    SetFontlist(Move(names));
                }
                return true;
            }
        }
        return false;
    }

    void PrependGeneric(FontFamilyType aType) {
        nsTArray<FontFamilyName> names;
        names.AppendElements(mFontlist->mNames);
        names.InsertElementAt(0, FontFamilyName(aType));
        SetFontlist(Move(names));
    }

    void ToString(nsAString& aFamilyList,
                  bool aQuotes = true,
                  bool aIncludeDefault = false) const {
        const nsTArray<FontFamilyName>& names = mFontlist->mNames;
        aFamilyList.Truncate();
        uint32_t len = names.Length();
        for (uint32_t i = 0; i < len; i++) {
            if (i != 0) {
                aFamilyList.Append(',');
            }
            const FontFamilyName& name = names[i];
            name.AppendToString(aFamilyList, aQuotes);
        }
        if (aIncludeDefault && mDefaultFontType != eFamily_none) {
            if (!aFamilyList.IsEmpty()) {
                aFamilyList.Append(',');
            }
            if (mDefaultFontType == eFamily_serif) {
                aFamilyList.AppendLiteral("serif");
            } else {
                aFamilyList.AppendLiteral("sans-serif");
            }
        }
    }

    // searches for a specific non-generic name, lowercase comparison
    bool Contains(const nsAString& aFamilyName) const {
        nsAutoString fam(aFamilyName);
        ToLowerCase(fam);
        for (const FontFamilyName& name : mFontlist->mNames) {
            if (name.mType != eFamily_named &&
                name.mType != eFamily_named_quoted) {
                continue;
            }
            nsAutoString listname(name.mName);
            ToLowerCase(listname);
            if (listname.Equals(fam)) {
                return true;
            }
        }
        return false;
    }

    FontFamilyType GetDefaultFontType() const { return mDefaultFontType; }
    void SetDefaultFontType(FontFamilyType aType) {
        NS_ASSERTION(aType == eFamily_none || aType == eFamily_serif ||
                     aType == eFamily_sans_serif,
                     "default font type must be either serif or sans-serif");
        mDefaultFontType = aType;
    }

    // memory reporting
    size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
        size_t n = 0;
        n += mFontlist->SizeOfIncludingThisIfUnshared(aMallocSizeOf);
        return n;
    }

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
        return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
    }

protected:
    NotNull<RefPtr<SharedFontList>> mFontlist;
    FontFamilyType             mDefaultFontType; // none, serif or sans-serif
};

inline bool
operator==(const FontFamilyList& a, const FontFamilyList& b) {
    return a.Equals(b);
}

inline bool
operator!=(const FontFamilyList& a, const FontFamilyList& b) {
    return !a.Equals(b);
}

} // namespace mozilla

#endif /* GFX_FONT_FAMILY_LIST_H */

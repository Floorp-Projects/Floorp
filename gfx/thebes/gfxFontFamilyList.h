/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FONT_FAMILY_LIST_H
#define GFX_FONT_FAMILY_LIST_H

#include "nsDebug.h"
#include "nsISupportsImpl.h"
#include "nsString.h"
#include "nsTArray.h"
#include "mozilla/MemoryReporting.h"

namespace mozilla {

/**
 * type of font family name, either a name (e.g. Helvetica) or a
 * generic (e.g. serif, sans-serif), with the ability to distinguish
 * between unquoted and quoted names for serializaiton
 */ 

enum FontFamilyType {
  eFamily_none = 0,  // used when finding generics

  // explicitly named font family (e.g. Helvetica)
  eFamily_named,
  eFamily_named_quoted,

  // generics
  eFamily_serif,
  eFamily_sans_serif,
  eFamily_monospace,
  eFamily_cursive,
  eFamily_fantasy,

  // special
  eFamily_moz_variable,
  eFamily_moz_fixed
};

enum QuotedName { eQuotedName, eUnquotedName };

/**
 * font family name, a string for the name if not a generic and
 * a font type indicated named family or which generic family
 */

struct FontFamilyName MOZ_FINAL {
    FontFamilyName()
        : mType(eFamily_named)
    {}

    // named font family - e.g. Helvetica
    FontFamilyName(const nsAString& aFamilyName,
                   QuotedName aQuoted = eUnquotedName) {
        mType = (aQuoted == eQuotedName) ? eFamily_named_quoted : eFamily_named;
        mName = aFamilyName;
    }

    // generic font family - e.g. sans-serif
    FontFamilyName(FontFamilyType aType) {
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

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
        return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
    }

    FontFamilyType mType;
    nsString       mName; // empty if mType != eFamily_named
};

inline bool
operator==(const FontFamilyName& a, const FontFamilyName& b) {
    return a.mType == b.mType && a.mName == b.mName;
}

/**
 * font family list, array of font families and a default font type.
 * font family names are either named strings or generics. the default
 * font type is used to preserve the variable font fallback behavior
 */ 

class FontFamilyList MOZ_FINAL {
public:
    FontFamilyList() : mDefaultFontType(eFamily_none) {
        MOZ_COUNT_CTOR(FontFamilyList);
    }

    FontFamilyList(FontFamilyType aGenericType)
        : mDefaultFontType(eFamily_none)
    {
        Append(FontFamilyName(aGenericType));
        MOZ_COUNT_CTOR(FontFamilyList);
    }

    FontFamilyList(const nsAString& aFamilyName,
                   QuotedName aQuoted)
        : mDefaultFontType(eFamily_none)
    {
        Append(FontFamilyName(aFamilyName, aQuoted));
        MOZ_COUNT_CTOR(FontFamilyList);
    }

    FontFamilyList(const FontFamilyList& aOther)
        : mFontlist(aOther.mFontlist), mDefaultFontType(aOther.mDefaultFontType)
    {
        MOZ_COUNT_CTOR(FontFamilyList);
    }

    ~FontFamilyList() {
        MOZ_COUNT_DTOR(FontFamilyList);
    }

    void Append(const FontFamilyName& aFamilyName) {
        mFontlist.AppendElement(aFamilyName);
    }

    void Append(const nsTArray<nsString>& aFamilyNameList) {
        uint32_t len = aFamilyNameList.Length();
        for (uint32_t i = 0; i < len; i++) {
            mFontlist.AppendElement(FontFamilyName(aFamilyNameList[i],
                                                   eUnquotedName));
        }
    }

    void Clear() {
        mFontlist.Clear();
    }

    uint32_t Length() const {
        return mFontlist.Length();
    }

    bool IsEmpty() const {
      return mFontlist.IsEmpty();
    }

    const nsTArray<FontFamilyName>& GetFontlist() const {
        return mFontlist;
    }

    bool Equals(const FontFamilyList& aFontlist) const {
        return mFontlist == aFontlist.mFontlist &&
               mDefaultFontType == aFontlist.mDefaultFontType;
    }

    FontFamilyType FirstGeneric() const {
        uint32_t len = mFontlist.Length();
        for (uint32_t i = 0; i < len; i++) {
            const FontFamilyName& name = mFontlist[i];
            if (name.IsGeneric()) {
                return name.mType;
            }
        }
        return eFamily_none;
    }

    bool HasGeneric() const {
        return FirstGeneric() != eFamily_none;
    }

    bool HasDefaultGeneric() const {
        uint32_t len = mFontlist.Length();
        for (uint32_t i = 0; i < len; i++) {
            const FontFamilyName& name = mFontlist[i];
            if (name.mType == mDefaultFontType) {
                return true;
            }
        }
        return false;
    }

    void ToString(nsAString& aFamilyList,
                  bool aQuotes = true,
                  bool aIncludeDefault = false) const {
        aFamilyList.Truncate();
        uint32_t len = mFontlist.Length();
        for (uint32_t i = 0; i < len; i++) {
            if (i != 0) {
                aFamilyList.Append(',');
            }
            const FontFamilyName& name = mFontlist[i];
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

    FontFamilyType GetDefaultFontType() const { return mDefaultFontType; }
    void SetDefaultFontType(FontFamilyType aType) {
        NS_ASSERTION(aType == eFamily_none || aType == eFamily_serif ||
                     aType == eFamily_sans_serif,
                     "default font type must be either serif or sans-serif");
        mDefaultFontType = aType;
    }

    // memory reporting
    size_t SizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
        return mFontlist.SizeOfExcludingThis(aMallocSizeOf);
    }

    size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
        return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
    }

    NS_INLINE_DECL_REFCOUNTING(FontFamilyList)

private:
    nsTArray<FontFamilyName>   mFontlist;
    FontFamilyType             mDefaultFontType; // none, serif or sans-serif
};

inline bool
operator==(const FontFamilyList& a, const FontFamilyList& b) {
    return a.Equals(b);
}

} // namespace mozilla

#endif /* GFX_FONT_FAMILY_LIST_H */

/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FONT_FAMILY_LIST_H
#define GFX_FONT_FAMILY_LIST_H

#include "nsAtom.h"
#include "nsDebug.h"
#include "nsISupportsImpl.h"
#include "nsString.h"
#include "nsStyleConsts.h"
#include "nsUnicharUtils.h"
#include "nsTArray.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/NotNull.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TimeStamp.h"

namespace mozilla {

/**
 * font family name, an Atom for the name if not a generic and
 * a font type indicated named family or which generic family
 */

struct FontFamilyName final {
  using Syntax = StyleFontFamilyNameSyntax;

  FontFamilyName() = delete;

  // named font family - e.g. Helvetica
  explicit FontFamilyName(nsAtom* aFamilyName, Syntax aSyntax)
      : mName(aFamilyName), mSyntax(aSyntax) {}

  explicit FontFamilyName(const nsACString& aFamilyName, Syntax aSyntax)
      : mName(NS_Atomize(aFamilyName)), mSyntax(aSyntax) {}

  // generic font family - e.g. sans-serif
  explicit FontFamilyName(StyleGenericFontFamily aGeneric)
      : mGeneric(aGeneric) {
    MOZ_ASSERT(mGeneric != StyleGenericFontFamily::None);
  }

  FontFamilyName(const FontFamilyName&) = default;

  bool IsNamed() const { return !!mName; }

  bool IsGeneric() const { return !IsNamed(); }

  bool IsQuoted() const { return mSyntax == StyleFontFamilyNameSyntax::Quoted; }

  void AppendToString(nsACString& aFamilyList, bool aQuotes = true) const {
    if (IsNamed()) {
      if (mSyntax == Syntax::Identifiers) {
        return aFamilyList.Append(nsAtomCString(mName));
      }
      if (aQuotes) {
        aFamilyList.Append('"');
      }
      aFamilyList.Append(nsAtomCString(mName));
      if (aQuotes) {
        aFamilyList.Append('"');
      }
      return;
    }
    switch (mGeneric) {
      case StyleGenericFontFamily::None:
      case StyleGenericFontFamily::MozEmoji:
        MOZ_FALLTHROUGH_ASSERT("Should never appear in a font-family name!");
      case StyleGenericFontFamily::Serif:
        aFamilyList.AppendLiteral("serif");
        break;
      case StyleGenericFontFamily::SansSerif:
        aFamilyList.AppendLiteral("sans-serif");
        break;
      case StyleGenericFontFamily::Monospace:
        aFamilyList.AppendLiteral("monospace");
        break;
      case StyleGenericFontFamily::Cursive:
        aFamilyList.AppendLiteral("cursive");
        break;
      case StyleGenericFontFamily::Fantasy:
        aFamilyList.AppendLiteral("fantasy");
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Unknown generic font-family!");
        break;
    }
  }

  // helper method that converts generic names to the right enum value
  static FontFamilyName Convert(const nsACString& aFamilyOrGenericName) {
    // should only be passed a single font - not entirely correct, a family
    // *could* have a comma in it but in practice never does so
    // for debug purposes this is fine
    NS_ASSERTION(aFamilyOrGenericName.FindChar(',') == -1,
                 "Convert method should only be passed a single family name");

    auto genericType = StyleGenericFontFamily::None;
    if (aFamilyOrGenericName.LowerCaseEqualsLiteral("serif")) {
      genericType = StyleGenericFontFamily::Serif;
    } else if (aFamilyOrGenericName.LowerCaseEqualsLiteral("sans-serif")) {
      genericType = StyleGenericFontFamily::SansSerif;
    } else if (aFamilyOrGenericName.LowerCaseEqualsLiteral("monospace") ||
               aFamilyOrGenericName.LowerCaseEqualsLiteral("-moz-fixed")) {
      genericType = StyleGenericFontFamily::Monospace;
    } else if (aFamilyOrGenericName.LowerCaseEqualsLiteral("cursive")) {
      genericType = StyleGenericFontFamily::Cursive;
    } else if (aFamilyOrGenericName.LowerCaseEqualsLiteral("fantasy")) {
      genericType = StyleGenericFontFamily::Fantasy;
    } else {
      return FontFamilyName(aFamilyOrGenericName, Syntax::Identifiers);
    }

    return FontFamilyName(genericType);
  }

  RefPtr<nsAtom> mName;  // null if mGeneric != Default
  StyleFontFamilyNameSyntax mSyntax = StyleFontFamilyNameSyntax::Quoted;
  StyleGenericFontFamily mGeneric = StyleGenericFontFamily::None;
};

inline bool operator==(const FontFamilyName& a, const FontFamilyName& b) {
  return a.mName == b.mName && a.mSyntax == b.mSyntax &&
         a.mGeneric == b.mGeneric;
}

/**
 * A refcounted array of FontFamilyNames.  We use this to store the specified
 * and computed value of the font-family property.
 *
 * TODO(heycam): It might better to define this type (and FontFamilyList and
 * FontFamilyName) in Rust.
 */
class SharedFontList {
  using Syntax = StyleFontFamilyNameSyntax;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SharedFontList);

  SharedFontList() = default;

  explicit SharedFontList(StyleGenericFontFamily aGenericType)
      : mNames{FontFamilyName(aGenericType)} {}

  SharedFontList(nsAtom* aFamilyName, Syntax aSyntax)
      : mNames{FontFamilyName(aFamilyName, aSyntax)} {}

  SharedFontList(const nsACString& aFamilyName, Syntax aSyntax)
      : mNames{FontFamilyName(aFamilyName, aSyntax)} {}

  explicit SharedFontList(nsTArray<FontFamilyName>&& aNames)
      : mNames(std::move(aNames)) {}

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
    size_t n = 0;
    n += aMallocSizeOf(this);
    n += mNames.ShallowSizeOfExcludingThis(aMallocSizeOf);
    return n;
  }

  size_t SizeOfIncludingThisIfUnshared(MallocSizeOf aMallocSizeOf) const {
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
  static StaticRefPtr<SharedFontList>
      sSingleGenerics[size_t(StyleGenericFontFamily::MozEmoji)];

 private:
  ~SharedFontList() = default;
};

/**
 * font family list, array of font families and a default font type.
 * font family names are either named strings or generics. the default
 * font type is used to preserve the variable font fallback behavior
 */
class FontFamilyList {
  using Syntax = StyleFontFamilyNameSyntax;

 public:
  FontFamilyList() : mFontlist(WrapNotNull(SharedFontList::sEmpty.get())) {}

  explicit FontFamilyList(StyleGenericFontFamily aGenericType)
      : mFontlist(MakeNotNull<SharedFontList*>(aGenericType)) {}

  FontFamilyList(nsAtom* aFamilyName, Syntax aSyntax)
      : mFontlist(MakeNotNull<SharedFontList*>(aFamilyName, aSyntax)) {}

  FontFamilyList(const nsACString& aFamilyName, Syntax aSyntax)
      : mFontlist(MakeNotNull<SharedFontList*>(aFamilyName, aSyntax)) {}

  explicit FontFamilyList(nsTArray<FontFamilyName>&& aNames)
      : mFontlist(MakeNotNull<SharedFontList*>(std::move(aNames))) {}

  FontFamilyList(const FontFamilyList& aOther)
      : mFontlist(aOther.mFontlist),
        mDefaultFontType(aOther.mDefaultFontType) {}

  explicit FontFamilyList(NotNull<SharedFontList*> aFontList)
      : mFontlist(aFontList) {}

  void SetFontlist(nsTArray<FontFamilyName>&& aNames) {
    mFontlist = MakeNotNull<SharedFontList*>(std::move(aNames));
  }

  void SetFontlist(NotNull<SharedFontList*> aFontlist) {
    mFontlist = aFontlist;
  }

  uint32_t Length() const { return mFontlist->mNames.Length(); }

  bool IsEmpty() const { return mFontlist->mNames.IsEmpty(); }

  NotNull<SharedFontList*> GetFontlist() const { return mFontlist; }

  bool Equals(const FontFamilyList& aFontlist) const {
    return (mFontlist == aFontlist.mFontlist ||
            mFontlist->mNames == aFontlist.mFontlist->mNames) &&
           mDefaultFontType == aFontlist.mDefaultFontType;
  }

  bool HasDefaultGeneric() const {
    if (mDefaultFontType == StyleGenericFontFamily::None) {
      return false;
    }
    for (const FontFamilyName& name : mFontlist->mNames) {
      if (name.mGeneric == mDefaultFontType) {
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
        if (name.mGeneric == StyleGenericFontFamily::Cursive ||
            name.mGeneric == StyleGenericFontFamily::Fantasy) {
          continue;
        }
        if (i > 0) {
          nsTArray<FontFamilyName> names;
          names.AppendElements(mFontlist->mNames);
          names.RemoveElementAt(i);
          names.InsertElementAt(0, name);
          SetFontlist(std::move(names));
        }
        return true;
      }
    }
    return false;
  }

  void PrependGeneric(StyleGenericFontFamily aGeneric) {
    nsTArray<FontFamilyName> names;
    names.AppendElements(mFontlist->mNames);
    names.InsertElementAt(0, FontFamilyName(aGeneric));
    SetFontlist(std::move(names));
  }

  void ToString(nsACString& aFamilyList, bool aQuotes = true,
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
    if (aIncludeDefault && mDefaultFontType != StyleGenericFontFamily::None) {
      if (!aFamilyList.IsEmpty()) {
        aFamilyList.Append(',');
      }
      if (mDefaultFontType == StyleGenericFontFamily::Serif) {
        aFamilyList.AppendLiteral("serif");
      } else {
        aFamilyList.AppendLiteral("sans-serif");
      }
    }
  }

  // searches for a specific non-generic name, case-insensitive comparison
  bool Contains(const nsACString& aFamilyName) const {
    NS_ConvertUTF8toUTF16 fam(aFamilyName);
    for (const FontFamilyName& name : mFontlist->mNames) {
      if (!name.IsNamed()) {
        continue;
      }
      nsDependentAtomString listname(name.mName);
      if (listname.Equals(fam, nsCaseInsensitiveStringComparator())) {
        return true;
      }
    }
    return false;
  }

  StyleGenericFontFamily GetDefaultFontType() const { return mDefaultFontType; }
  void SetDefaultFontType(StyleGenericFontFamily aType) {
    NS_ASSERTION(aType == StyleGenericFontFamily::None ||
                     aType == StyleGenericFontFamily::Serif ||
                     aType == StyleGenericFontFamily::SansSerif,
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
  StyleGenericFontFamily mDefaultFontType =
      StyleGenericFontFamily::None;  // or serif, or sans-serif
};

inline bool operator==(const FontFamilyList& a, const FontFamilyList& b) {
  return a.Equals(b);
}

inline bool operator!=(const FontFamilyList& a, const FontFamilyList& b) {
  return !a.Equals(b);
}

}  // namespace mozilla

#endif /* GFX_FONT_FAMILY_LIST_H */

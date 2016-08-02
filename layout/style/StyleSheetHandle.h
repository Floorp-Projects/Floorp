/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StyleSheetHandle_h
#define mozilla_StyleSheetHandle_h

#include "mozilla/css/SheetParsingMode.h"
#include "mozilla/net/ReferrerPolicy.h"
#include "mozilla/CORSMode.h"
#include "mozilla/HandleRefPtr.h"
#include "mozilla/RefCountType.h"

namespace mozilla {
namespace dom {
class SRIMetadata;
} // namespace dom
class CSSStyleSheet;
class ServoStyleSheet;
class StyleSheet;
} // namespace mozilla
class nsIDocument;
class nsIPrincipal;
class nsIURI;

namespace mozilla {

#define SERVO_BIT 0x1

/**
 * Smart pointer class that can hold a pointer to either a CSSStyleSheet
 * or a ServoStyleSheet.
 */
class StyleSheetHandle
{
public:
  typedef HandleRefPtr<StyleSheetHandle> RefPtr;

  // We define this Ptr class with a StyleSheet API that forwards on to the
  // wrapped pointer, rather than putting these methods on StyleSheetHandle
  // itself, so that we can have StyleSheetHandle behave like a smart pointer
  // and be dereferenced with operator->.
  class Ptr
  {
  public:
    friend class ::mozilla::StyleSheetHandle;

    bool IsGecko() const { return !IsServo(); }
    bool IsServo() const
    {
      MOZ_ASSERT(mValue);
#ifdef MOZ_STYLO
      return mValue & SERVO_BIT;
#else
      return false;
#endif
    }

    inline StyleSheet* AsStyleSheet();
    inline const StyleSheet* AsStyleSheet() const;

    CSSStyleSheet* AsGecko()
    {
      MOZ_ASSERT(IsGecko());
      return reinterpret_cast<CSSStyleSheet*>(mValue);
    }

    ServoStyleSheet* AsServo()
    {
      MOZ_ASSERT(IsServo());
      return reinterpret_cast<ServoStyleSheet*>(mValue & ~SERVO_BIT);
    }

    CSSStyleSheet* GetAsGecko() { return IsGecko() ? AsGecko() : nullptr; }
    ServoStyleSheet* GetAsServo() { return IsServo() ? AsServo() : nullptr; }

    const CSSStyleSheet* AsGecko() const
    {
      return const_cast<Ptr*>(this)->AsGecko();
    }

    const ServoStyleSheet* AsServo() const
    {
      MOZ_ASSERT(IsServo());
      return const_cast<Ptr*>(this)->AsServo();
    }

    void* AsVoidPtr() const
    {
      return reinterpret_cast<void*>(mValue & ~SERVO_BIT);
    }

    const CSSStyleSheet* GetAsGecko() const { return IsGecko() ? AsGecko() : nullptr; }
    const ServoStyleSheet* GetAsServo() const { return IsServo() ? AsServo() : nullptr; }

    // These inline methods are defined in StyleSheetHandleInlines.h.
    inline MozExternalRefCountType AddRef();
    inline MozExternalRefCountType Release();

    // Style sheet interface.  These inline methods are defined in
    // StyleSheetHandleInlines.h and just forward to the underlying
    // CSSStyleSheet or ServoStyleSheet.  See corresponding comments in
    // CSSStyleSheet.h for descriptions of these methods.

    inline bool IsInline() const;
    inline nsIURI* GetSheetURI() const;
    inline nsIURI* GetOriginalURI() const;
    inline nsIURI* GetBaseURI() const;
    inline void SetURIs(nsIURI* aSheetURI, nsIURI* aOriginalSheetURI, nsIURI* aBaseURI);
    inline bool IsApplicable() const;
    inline bool HasRules() const;
    inline nsIDocument* GetOwningDocument() const;
    inline void SetOwningDocument(nsIDocument* aDocument);
    inline nsINode* GetOwnerNode() const;
    inline void SetOwningNode(nsINode* aNode);
    inline StyleSheetHandle GetParentSheet() const;
    inline void AppendStyleSheet(StyleSheetHandle aSheet);
    inline nsIPrincipal* Principal() const;
    inline void SetPrincipal(nsIPrincipal* aPrincipal);
    inline CORSMode GetCORSMode() const;
    inline net::ReferrerPolicy GetReferrerPolicy() const;
    inline void GetIntegrity(dom::SRIMetadata& aResult) const;
    inline size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const;
#ifdef DEBUG
    inline void List(FILE* aOut = stdout, int32_t aIndex = 0) const;
#endif

  private:
    // Stores a pointer to an CSSStyleSheet or a ServoStyleSheet.  The least
    // significant bit is 0 for the former, 1 for the latter.
    uintptr_t mValue;
  };

  MOZ_IMPLICIT StyleSheetHandle(decltype(nullptr) = nullptr)
  {
    mPtr.mValue = 0;
  }
  StyleSheetHandle(const StyleSheetHandle& aOth) {
    mPtr.mValue = aOth.mPtr.mValue;
  }
  MOZ_IMPLICIT StyleSheetHandle(CSSStyleSheet* aSet) { *this = aSet; }
  MOZ_IMPLICIT StyleSheetHandle(ServoStyleSheet* aSet) { *this = aSet; }
  MOZ_IMPLICIT StyleSheetHandle(const ::RefPtr<CSSStyleSheet>& aSet)
  {
    *this = aSet.get();
  }
  MOZ_IMPLICIT StyleSheetHandle(const ::RefPtr<ServoStyleSheet>& aSet)
  {
    *this = aSet.get();
  }

  StyleSheetHandle& operator=(decltype(nullptr)) { mPtr.mValue = 0; return *this; }

  StyleSheetHandle& operator=(CSSStyleSheet* aSheet)
  {
    MOZ_ASSERT(!(reinterpret_cast<uintptr_t>(aSheet) & SERVO_BIT),
               "least significant bit shouldn't be set; we use it for state");
    mPtr.mValue = reinterpret_cast<uintptr_t>(aSheet);
    return *this;
  }

  StyleSheetHandle& operator=(ServoStyleSheet* aSheet)
  {
#ifdef MOZ_STYLO
    MOZ_ASSERT(!(reinterpret_cast<uintptr_t>(aSheet) & SERVO_BIT),
               "least significant bit shouldn't be set; we use it for state");
    mPtr.mValue =
      aSheet ? (reinterpret_cast<uintptr_t>(aSheet) | SERVO_BIT) : 0;
    return *this;
#else
    MOZ_CRASH("should not have a ServoStyleSheet object when MOZ_STYLO is "
              "disabled");
#endif
  }

  // Make StyleSheetHandle usable in boolean contexts.
  explicit operator bool() const { return !!mPtr.mValue; }
  bool operator!() const { return !mPtr.mValue; }

  // Make StyleSheetHandle behave like a smart pointer.
  Ptr* operator->() { return &mPtr; }
  const Ptr* operator->() const { return &mPtr; }

  bool operator==(const StyleSheetHandle& aOther) const
  {
    return mPtr.mValue == aOther.mPtr.mValue;
  }

private:
  Ptr mPtr;
};

#undef SERVO_BIT

} // namespace mozilla

#endif // mozilla_StyleSheetHandle_h

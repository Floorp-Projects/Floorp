/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StyleSetHandle_h
#define mozilla_StyleSetHandle_h

#include "mozilla/EventStates.h"
#include "mozilla/RefPtr.h"
#include "mozilla/SheetType.h"
#include "mozilla/StyleBackendType.h"
#include "mozilla/StyleSheetHandle.h"
#include "nsChangeHint.h"
#include "nsCSSPseudoElements.h"
#include "nsTArray.h"

namespace mozilla {
class CSSStyleSheet;
class ServoStyleSet;
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla
class nsIAtom;
class nsIContent;
class nsIDocument;
class nsStyleContext;
class nsStyleSet;
class nsPresContext;
struct TreeMatchContext;

namespace mozilla {

#define SERVO_BIT 0x1

/**
 * Smart pointer class that can hold a pointer to either an nsStyleSet
 * or a ServoStyleSet.
 */
class StyleSetHandle
{
public:
  // We define this Ptr class with a StyleSet API that forwards on to the
  // wrapped pointer, rather than putting these methods on StyleSetHandle
  // itself, so that we can have StyleSetHandle behave like a smart pointer and
  // be dereferenced with operator->.
  class Ptr
  {
  public:
    friend class ::mozilla::StyleSetHandle;

    bool IsGecko() const { return !IsServo(); }
    bool IsServo() const
    {
      MOZ_ASSERT(mValue, "StyleSetHandle null pointer dereference");
#ifdef MOZ_STYLO
      return mValue & SERVO_BIT;
#else
      return false;
#endif
    }

    StyleBackendType BackendType() const
    {
      return IsGecko() ? StyleBackendType::Gecko :
                         StyleBackendType::Servo;
    }

    nsStyleSet* AsGecko()
    {
      MOZ_ASSERT(IsGecko());
      return reinterpret_cast<nsStyleSet*>(mValue);
    }

    ServoStyleSet* AsServo()
    {
      MOZ_ASSERT(IsServo());
      return reinterpret_cast<ServoStyleSet*>(mValue & ~SERVO_BIT);
    }

    nsStyleSet* GetAsGecko() { return IsGecko() ? AsGecko() : nullptr; }
    ServoStyleSet* GetAsServo() { return IsServo() ? AsServo() : nullptr; }

    const nsStyleSet* AsGecko() const
    {
      return const_cast<Ptr*>(this)->AsGecko();
    }

    const ServoStyleSet* AsServo() const
    {
      MOZ_ASSERT(IsServo());
      return const_cast<Ptr*>(this)->AsServo();
    }

    const nsStyleSet* GetAsGecko() const { return IsGecko() ? AsGecko() : nullptr; }
    const ServoStyleSet* GetAsServo() const { return IsServo() ? AsServo() : nullptr; }

    // These inline methods are defined in StyleSetHandleInlines.h.
    inline void Delete();

    // Style set interface.  These inline methods are defined in
    // StyleSetHandleInlines.h and just forward to the underlying
    // nsStyleSet or ServoStyleSet.  See corresponding comments in
    // nsStyleSet.h for descriptions of these methods.

    inline void Init(nsPresContext* aPresContext);
    inline void BeginShutdown();
    inline void Shutdown();
    inline bool GetAuthorStyleDisabled() const;
    inline nsresult SetAuthorStyleDisabled(bool aStyleDisabled);
    inline void BeginUpdate();
    inline nsresult EndUpdate();
    inline already_AddRefed<nsStyleContext>
    ResolveStyleFor(dom::Element* aElement,
                    nsStyleContext* aParentContext);
    inline already_AddRefed<nsStyleContext>
    ResolveStyleFor(dom::Element* aElement,
                    nsStyleContext* aParentContext,
                    TreeMatchContext& aTreeMatchContext);
    inline already_AddRefed<nsStyleContext>
    ResolveStyleForText(nsIContent* aTextNode,
                        nsStyleContext* aParentContext);
    inline already_AddRefed<nsStyleContext>
    ResolveStyleForOtherNonElement(nsStyleContext* aParentContext);
    inline already_AddRefed<nsStyleContext>
    ResolvePseudoElementStyle(dom::Element* aParentElement,
                              mozilla::CSSPseudoElementType aType,
                              nsStyleContext* aParentContext,
                              dom::Element* aPseudoElement);
    inline already_AddRefed<nsStyleContext>
    ResolveAnonymousBoxStyle(nsIAtom* aPseudoTag, nsStyleContext* aParentContext,
                             uint32_t aFlags = 0);
    inline nsresult AppendStyleSheet(SheetType aType, StyleSheetHandle aSheet);
    inline nsresult PrependStyleSheet(SheetType aType, StyleSheetHandle aSheet);
    inline nsresult RemoveStyleSheet(SheetType aType, StyleSheetHandle aSheet);
    inline nsresult ReplaceSheets(SheetType aType,
                           const nsTArray<StyleSheetHandle::RefPtr>& aNewSheets);
    inline nsresult InsertStyleSheetBefore(SheetType aType,
                                    StyleSheetHandle aNewSheet,
                                    StyleSheetHandle aReferenceSheet);
    inline int32_t SheetCount(SheetType aType) const;
    inline StyleSheetHandle StyleSheetAt(SheetType aType, int32_t aIndex) const;
    inline nsresult RemoveDocStyleSheet(StyleSheetHandle aSheet);
    inline nsresult AddDocStyleSheet(StyleSheetHandle aSheet, nsIDocument* aDocument);
    inline already_AddRefed<nsStyleContext>
    ProbePseudoElementStyle(dom::Element* aParentElement,
                            mozilla::CSSPseudoElementType aType,
                            nsStyleContext* aParentContext);
    inline already_AddRefed<nsStyleContext>
    ProbePseudoElementStyle(dom::Element* aParentElement,
                            mozilla::CSSPseudoElementType aType,
                            nsStyleContext* aParentContext,
                            TreeMatchContext& aTreeMatchContext,
                            dom::Element* aPseudoElement = nullptr);
    inline nsRestyleHint HasStateDependentStyle(dom::Element* aElement,
                                                EventStates aStateMask);
    inline nsRestyleHint HasStateDependentStyle(
        dom::Element* aElement,
        mozilla::CSSPseudoElementType aPseudoType,
        dom::Element* aPseudoElement,
        EventStates aStateMask);

    inline void RootStyleContextAdded();
    inline void RootStyleContextRemoved();

  private:
    // Stores a pointer to an nsStyleSet or a ServoStyleSet.  The least
    // significant bit is 0 for the former, 1 for the latter.  This is
    // valid as the least significant bit will never be used for a pointer
    // value on platforms we care about.
    uintptr_t mValue;
  };

  StyleSetHandle() { mPtr.mValue = 0; }
  StyleSetHandle(const StyleSetHandle& aOth) { mPtr.mValue = aOth.mPtr.mValue; }
  MOZ_IMPLICIT StyleSetHandle(nsStyleSet* aSet) { *this = aSet; }
  MOZ_IMPLICIT StyleSetHandle(ServoStyleSet* aSet) { *this = aSet; }

  StyleSetHandle& operator=(nsStyleSet* aStyleSet)
  {
    MOZ_ASSERT(!(reinterpret_cast<uintptr_t>(aStyleSet) & SERVO_BIT),
               "least significant bit shouldn't be set; we use it for state");
    mPtr.mValue = reinterpret_cast<uintptr_t>(aStyleSet);
    return *this;
  }

  StyleSetHandle& operator=(ServoStyleSet* aStyleSet)
  {
#ifdef MOZ_STYLO
    MOZ_ASSERT(!(reinterpret_cast<uintptr_t>(aStyleSet) & SERVO_BIT),
               "least significant bit shouldn't be set; we use it for state");
    mPtr.mValue =
      aStyleSet ? (reinterpret_cast<uintptr_t>(aStyleSet) | SERVO_BIT) : 0;
    return *this;
#else
    MOZ_CRASH("should not have a ServoStyleSet object when MOZ_STYLO is "
              "disabled");
#endif
  }

  // Make StyleSetHandle usable in boolean contexts.
  explicit operator bool() const { return !!mPtr.mValue; }
  bool operator!() const { return !mPtr.mValue; }

  // Make StyleSetHandle behave like a smart pointer.
  Ptr* operator->() { return &mPtr; }
  const Ptr* operator->() const { return &mPtr; }

private:
  Ptr mPtr;
};

#undef SERVO_BIT

} // namespace mozilla

#endif // mozilla_StyleSetHandle_h

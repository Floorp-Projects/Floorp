/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoStyleContext_h
#define mozilla_ServoStyleContext_h

#include "nsIMemoryReporter.h"
#include "nsStyleContext.h"
#include "nsWindowSizes.h"
#include <algorithm>

#include "mozilla/CachedInheritingStyles.h"

namespace mozilla {

namespace dom {
class Element;
} // namespace dom

MOZ_DEFINE_MALLOC_ENCLOSING_SIZE_OF(ServoComputedValuesMallocEnclosingSizeOf)

class ServoStyleContext final : public nsStyleContext
{
public:
  ServoStyleContext(nsPresContext* aPresContext,
                    nsAtom* aPseudoTag,
                    CSSPseudoElementType aPseudoType,
                    ServoComputedDataForgotten aComputedValues);

  nsPresContext* PresContext() const { return mPresContext; }
  const ServoComputedData* ComputedData() const { return &mSource; }

  void AddRef() { Servo_StyleContext_AddRef(this); }
  void Release() { Servo_StyleContext_Release(this); }

  ServoStyleContext* GetStyleIfVisited() const
  {
    return ComputedData()->visited_style.mPtr;
  }

  bool IsLazilyCascadedPseudoElement() const
  {
    return IsPseudoElement() &&
           !nsCSSPseudoElements::IsEagerlyCascadedInServo(GetPseudoType());
  }

  ServoStyleContext* GetCachedInheritingAnonBoxStyle(nsAtom* aAnonBox) const
  {
    MOZ_ASSERT(nsCSSAnonBoxes::IsInheritingAnonBox(aAnonBox));
    return mCachedInheritingStyles.Lookup(aAnonBox);
  }

  void SetCachedInheritedAnonBoxStyle(nsAtom* aAnonBox, ServoStyleContext* aStyle)
  {
    MOZ_ASSERT(!GetCachedInheritingAnonBoxStyle(aAnonBox));
    mCachedInheritingStyles.Insert(aStyle);
  }

  ServoStyleContext* GetCachedLazyPseudoStyle(CSSPseudoElementType aPseudo) const;

  void SetCachedLazyPseudoStyle(ServoStyleContext* aStyle)
  {
    MOZ_ASSERT(aStyle->GetPseudo() && !aStyle->IsAnonBox());
    MOZ_ASSERT(!GetCachedLazyPseudoStyle(aStyle->GetPseudoType()));
    MOZ_ASSERT(!IsLazilyCascadedPseudoElement(), "lazy pseudos can't inherit lazy pseudos");
    MOZ_ASSERT(aStyle->IsLazilyCascadedPseudoElement());

    // Since we're caching lazy pseudo styles on the ComputedValues of the
    // originating element, we can assume that we either have the same
    // originating element, or that they were at least similar enough to share
    // the same ComputedValues, which means that they would match the same
    // pseudo rules. This allows us to avoid matching selectors and checking
    // the rule node before deciding to share.
    //
    // The one place this optimization breaks is with pseudo-elements that
    // support state (like :hover). So we just avoid sharing in those cases.
    if (nsCSSPseudoElements::PseudoElementSupportsUserActionState(aStyle->GetPseudoType())) {
      return;
    }

    mCachedInheritingStyles.Insert(aStyle);
  }

  /**
   * Makes this context match |aOther| in terms of which style structs have
   * been resolved.
   */
  inline void ResolveSameStructsAs(const ServoStyleContext* aOther);

  // The |aCVsSize| outparam on this function is where the actual CVs size
  // value is added. It's done that way because the callers know which value
  // the size should be added to.
  void AddSizeOfIncludingThis(nsWindowSizes& aSizes, size_t* aCVsSize) const
  {
    // Note: |this| sits within a servo_arc::Arc, i.e. it is preceded by a
    // refcount. So we need to measure it with a function that can handle an
    // interior pointer. We use ServoComputedValuesMallocEnclosingSizeOf to
    // clearly identify in DMD's output the memory measured here.
    *aCVsSize += ServoComputedValuesMallocEnclosingSizeOf(this);
    mSource.AddSizeOfExcludingThis(aSizes);
    mCachedInheritingStyles.AddSizeOfIncludingThis(aSizes, aCVsSize);
  }

private:
  nsPresContext* mPresContext;
  ServoComputedData mSource;

  // A cache of anonymous box and lazy pseudo styles inheriting from this style.
  CachedInheritingStyles mCachedInheritingStyles;
};

} // namespace mozilla

#endif // mozilla_ServoStyleContext_h

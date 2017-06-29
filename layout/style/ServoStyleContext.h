/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoStyleContext_h
#define mozilla_ServoStyleContext_h

#include "nsStyleContext.h"

namespace mozilla {

class ServoStyleContext final : public nsStyleContext {
public:

  static already_AddRefed<ServoStyleContext>
  Create(nsStyleContext* aParentContext,
         nsPresContext* aPresContext,
         nsIAtom* aPseudoTag,
         mozilla::CSSPseudoElementType aPseudoType,
         already_AddRefed<ServoComputedValues> aComputedValues);

  ServoStyleContext(nsStyleContext* aParent,
                    nsPresContext* aPresContext,
                    nsIAtom* aPseudoTag,
                    CSSPseudoElementType aPseudoType,
                    already_AddRefed<ServoComputedValues> aComputedValues);

  nsPresContext* PresContext() const {
    return mPresContext;
  }

  ServoComputedValues* ComputedValues() const {
    return mSource;
  }
  ~ServoStyleContext() {
    Destructor();
  }

  /**
   * Makes this context match |aOther| in terms of which style structs have
   * been resolved.
   */
  void ResolveSameStructsAs(nsPresContext* aPresContext, ServoStyleContext* aOther) {
    // NB: This function is only called on newly-minted style contexts, but
    // those may already have resolved style structs given the SetStyleBits call
    // in FinishConstruction. So we carefully xor out the bits that are new so
    // that we don't call FinishStyle twice.
    uint64_t ourBits = mBits & NS_STYLE_INHERIT_MASK;
    uint64_t otherBits = aOther->mBits & NS_STYLE_INHERIT_MASK;
    MOZ_ASSERT((otherBits | ourBits) == otherBits, "otherBits should be a superset");
    uint64_t newBits = (ourBits ^ otherBits) & NS_STYLE_INHERIT_MASK;

#define STYLE_STRUCT(name_, checkdata_cb)                                             \
    if (nsStyle##name_::kHasFinishStyle && newBits & NS_STYLE_INHERIT_BIT(name_)) {   \
      const nsStyle##name_* data = Servo_GetStyle##name_(ComputedValues());           \
      const_cast<nsStyle##name_*>(data)->FinishStyle(aPresContext);                   \
    }
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

    mBits |= newBits;
  }

private:
  nsPresContext* mPresContext;
  RefPtr<ServoComputedValues> mSource;
};

}

#endif // mozilla_ServoStyleContext_h

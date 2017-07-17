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

  void AddRef() {
    Servo_StyleContext_AddRef(this);
  }

  void Release() {
    Servo_StyleContext_Release(this);
  }

  ~ServoStyleContext() {
    Destructor();
  }

  /**
   * Makes this context match |aOther| in terms of which style structs have
   * been resolved.
   */
  void ResolveSameStructsAs(nsPresContext* aPresContext, const ServoStyleContext* aOther) {
    // Only resolve structs that are not already resolved in this struct.
    uint64_t ourBits = mBits & NS_STYLE_INHERIT_MASK;
    uint64_t otherBits = aOther->mBits & NS_STYLE_INHERIT_MASK;
    uint64_t newBits = otherBits & ~ourBits & NS_STYLE_INHERIT_MASK;

  #define STYLE_STRUCT(name_, checkdata_cb)                                           \
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

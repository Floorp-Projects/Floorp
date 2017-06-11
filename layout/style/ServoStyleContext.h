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

  NonOwningStyleContextSource StyleSource() const {
    return NonOwningStyleContextSource(mSource);
  }
  ServoComputedValues* ComputedValues() const {
    return mSource;
  }
  ~ServoStyleContext() {
    Destructor();
  }
private:
  nsPresContext* mPresContext;
  RefPtr<ServoComputedValues> mSource;
};

}

#endif // mozilla_ServoStyleContext_h

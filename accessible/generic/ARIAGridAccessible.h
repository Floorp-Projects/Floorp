/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_A11Y_ARIAGridAccessible_h_
#define MOZILLA_A11Y_ARIAGridAccessible_h_

#include "HyperTextAccessible.h"

namespace mozilla {
namespace a11y {

/**
 * Accessible for ARIA gridcell and rowheader/columnheader.
 */
class ARIAGridCellAccessible : public HyperTextAccessible {
 public:
  ARIAGridCellAccessible(nsIContent* aContent, DocAccessible* aDoc);

  NS_INLINE_DECL_REFCOUNTING_INHERITED(ARIAGridCellAccessible,
                                       HyperTextAccessible)

  // LocalAccessible
  virtual void ApplyARIAState(uint64_t* aState) const override;
  virtual already_AddRefed<AccAttributes> NativeAttributes() override;

 protected:
  virtual ~ARIAGridCellAccessible() {}
};

}  // namespace a11y
}  // namespace mozilla

#endif

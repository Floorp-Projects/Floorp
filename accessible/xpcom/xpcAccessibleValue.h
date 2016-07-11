/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_xpcAccessibleValue_h_
#define mozilla_a11y_xpcAccessibleValue_h_

#include "nsIAccessibleValue.h"

namespace mozilla {
namespace a11y {

class Accessible;

/**
 * XPCOM nsIAccessibleValue interface implementation, used by
 * xpcAccessibleGeneric class.
 */
class xpcAccessibleValue : public nsIAccessibleValue
{
public:
  NS_IMETHOD GetMaximumValue(double* aValue) final override;
  NS_IMETHOD GetMinimumValue(double* aValue) final override;
  NS_IMETHOD GetCurrentValue(double* aValue) final override;
  NS_IMETHOD SetCurrentValue(double aValue) final override;
  NS_IMETHOD GetMinimumIncrement(double* aMinIncrement) final override;

protected:
  xpcAccessibleValue() { }
  virtual ~xpcAccessibleValue() {}

private:
  AccessibleOrProxy Intl();

  xpcAccessibleValue(const xpcAccessibleValue&) = delete;
  xpcAccessibleValue& operator =(const xpcAccessibleValue&) = delete;
};

} // namespace a11y
} // namespace mozilla
#endif

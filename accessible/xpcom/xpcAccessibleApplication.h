/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_xpcAccessibleApplication_h_
#define mozilla_a11y_xpcAccessibleApplication_h_

#include "nsIAccessibleApplication.h"
#include "ApplicationAccessible.h"
#include "xpcAccessibleGeneric.h"

namespace mozilla {
namespace a11y {

/**
 * XPCOM wrapper around ApplicationAccessible class.
 */
class xpcAccessibleApplication : public xpcAccessibleGeneric,
                                 public nsIAccessibleApplication
{
public:
  explicit xpcAccessibleApplication(Accessible* aIntl) :
    xpcAccessibleGeneric(aIntl) { }

  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessibleApplication
  NS_IMETHOD GetAppName(nsAString& aName) final override;
  NS_IMETHOD GetAppVersion(nsAString& aVersion) final override;
  NS_IMETHOD GetPlatformName(nsAString& aName) final override;
  NS_IMETHOD GetPlatformVersion(nsAString& aVersion) final override;

protected:
  virtual ~xpcAccessibleApplication() {}

private:
  ApplicationAccessible* Intl() { return mIntl.AsAccessible()->AsApplication(); }

  xpcAccessibleApplication(const xpcAccessibleApplication&) = delete;
  xpcAccessibleApplication& operator =(const xpcAccessibleApplication&) = delete;
};

} // namespace a11y
} // namespace mozilla

#endif

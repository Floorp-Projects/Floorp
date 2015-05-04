/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MobileCallForwardingOptions_h
#define mozilla_dom_MobileCallForwardingOptions_h

#include "nsIMobileCallForwardingOptions.h"
#include "nsString.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace dom {
namespace mobileconnection {

class MobileCallForwardingOptions final : public nsIMobileCallForwardingOptions
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMOBILECALLFORWARDINGOPTIONS

  MobileCallForwardingOptions(bool aActive, int16_t aAction,
                              int16_t aReason, const nsAString& aNumber,
                              int16_t aTimeSeconds, int16_t aServiceClass);

private:
  // Don't try to use the default constructor.
  MobileCallForwardingOptions() {}

  ~MobileCallForwardingOptions() {}

  bool mActive;
  int16_t mAction;
  int16_t mReason;
  nsString mNumber;
  int16_t mTimeSeconds;
  int16_t mServiceClass;
};

} // namespace mobileconnection
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MobileCallForwardingOptions_h
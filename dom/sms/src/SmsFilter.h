/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_sms_SmsFilter_h
#define mozilla_dom_sms_SmsFilter_h

#include "mozilla/dom/sms/PSms.h"
#include "nsIDOMSmsFilter.h"
#include "Types.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace dom {
namespace sms {

class SmsFilter MOZ_FINAL : public nsIDOMMozSmsFilter
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMMOZSMSFILTER

  SmsFilter();
  SmsFilter(const SmsFilterData& aData);

  const SmsFilterData& GetData() const;

  static nsresult NewSmsFilter(nsISupports** aSmsFilter);

private:
  SmsFilterData mData;
};

inline const SmsFilterData&
SmsFilter::GetData() const {
  return mData;
}

} // namespace sms
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_sms_SmsFilter_h

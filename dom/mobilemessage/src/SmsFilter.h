/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobilemessage_SmsFilter_h
#define mozilla_dom_mobilemessage_SmsFilter_h

#include "mozilla/dom/mobilemessage/SmsTypes.h"
#include "nsIDOMSmsFilter.h"
#include "Types.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace dom {

class SmsFilter MOZ_FINAL : public nsIDOMMozSmsFilter
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMMOZSMSFILTER

  SmsFilter();
  SmsFilter(const mobilemessage::SmsFilterData& aData);

  const mobilemessage::SmsFilterData& GetData() const;

  static nsresult NewSmsFilter(nsISupports** aSmsFilter);

private:
  mobilemessage::SmsFilterData mData;
};

inline const mobilemessage::SmsFilterData&
SmsFilter::GetData() const {
  return mData;
}

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_mobilemessage_SmsFilter_h

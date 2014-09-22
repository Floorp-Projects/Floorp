/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TelephonyCallback_h
#define mozilla_dom_TelephonyCallback_h

#include "nsCOMPtr.h"
#include "nsITelephonyService.h"

namespace mozilla {
namespace dom {

class Promise;
class Telephony;

namespace telephony {

class TelephonyCallback MOZ_FINAL : public nsITelephonyCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITELEPHONYCALLBACK

  TelephonyCallback(Telephony* aTelephony, Promise* aPromise,
                    uint32_t aServiceId);

private:
  ~TelephonyCallback() {}

  nsRefPtr<Telephony> mTelephony;
  nsRefPtr<Promise> mPromise;
  uint32_t mServiceId;
};

} // namespace telephony
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TelephonyCallback_h

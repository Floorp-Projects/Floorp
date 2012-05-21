/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_sms_SmsService_h
#define mozilla_dom_sms_SmsService_h

#include "nsISmsService.h"
#include "nsCOMPtr.h"
#include "nsIRadioInterfaceLayer.h"

namespace mozilla {
namespace dom {
namespace sms {

class SmsService : public nsISmsService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISMSSERVICE
  SmsService();

protected:
  nsCOMPtr<nsIRadioInterfaceLayer> mRIL;
};

} // namespace sms
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_sms_SmsService_h

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobilemessage_SmsService_h
#define mozilla_dom_mobilemessage_SmsService_h

#include "nsISmsService.h"
#include "nsCOMPtr.h"
#include "nsIRadioInterfaceLayer.h"
#include "nsTArray.h"
#include "nsString.h"

namespace mozilla {
namespace dom {
namespace mobilemessage {

class SmsService : public nsISmsService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISMSSERVICE
  SmsService();

protected:
  // TODO: Bug 854326 - B2G Multi-SIM: support multiple SIM cards for SMS/MMS
  nsCOMPtr<nsIRadioInterface> mRadioInterface;
  nsTArray<nsString> mSilentNumbers;
};

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_mobilemessage_SmsService_h

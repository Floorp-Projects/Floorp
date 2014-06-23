/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobilemessage_MobileMessageService_h
#define mozilla_dom_mobilemessage_MobileMessageService_h

#include "nsIMobileMessageService.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {
namespace dom {
namespace mobilemessage {

class MobileMessageService MOZ_FINAL : public nsIMobileMessageService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMOBILEMESSAGESERVICE

  static already_AddRefed<MobileMessageService> GetInstance();

private:
  ~MobileMessageService() {}

  static StaticRefPtr<MobileMessageService> sSingleton;

};

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_mobilemessage_MobileMessageService_h

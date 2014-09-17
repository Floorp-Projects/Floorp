/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobileconnection_MobileConnectionIPCService_h
#define mozilla_dom_mobileconnection_MobileConnectionIPCService_h

#include "nsCOMPtr.h"
#include "MobileConnectionChild.h"
#include "nsIMobileConnectionService.h"

namespace mozilla {
namespace dom {
namespace mobileconnection {

class MobileConnectionIPCService MOZ_FINAL : public nsIMobileConnectionService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMOBILECONNECTIONSERVICE

  static MobileConnectionIPCService*
  GetSingleton();

private:
  MobileConnectionIPCService();

  ~MobileConnectionIPCService();

  /** Send request */
  nsresult
  SendRequest(uint32_t aClientId, MobileConnectionRequest aRequest,
              nsIMobileConnectionCallback* aRequestCallback);

  nsTArray<nsRefPtr<MobileConnectionChild>> mClients;
};

} // name space mobileconnection
} // name space dom
} // name space mozilla

#endif // mozilla_dom_mobileconnection_MobileConnectionIPCService_h

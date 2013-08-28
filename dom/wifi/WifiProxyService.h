/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WifiProxyService_h
#define WifiProxyService_h

#include "nsIWifiService.h"
#include "nsCOMPtr.h"
#include "nsThread.h"
#include "mozilla/dom/WifiOptionsBinding.h"

namespace mozilla {

class WifiProxyService MOZ_FINAL : public nsIWifiProxyService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWIFIPROXYSERVICE

  static already_AddRefed<WifiProxyService>
  FactoryCreate();

  void DispatchWifiEvent(const nsAString& aEvent);
  void DispatchWifiResult(const mozilla::dom::WifiResultOptions& aOptions);

private:
  WifiProxyService();
  ~WifiProxyService();

  nsCOMPtr<nsIThread> mEventThread;
  nsCOMPtr<nsIThread> mControlThread;
  nsCOMPtr<nsIWifiEventListener> mListener;
};

} // namespace mozilla

#endif // WifiProxyService_h

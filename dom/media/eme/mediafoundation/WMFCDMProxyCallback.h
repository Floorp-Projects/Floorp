/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_EME_MEDIAFOUNDATION_WMFCDMPROXYCALLBACK_H_
#define DOM_MEDIA_EME_MEDIAFOUNDATION_WMFCDMPROXYCALLBACK_H_

#include "mozilla/PMFCDM.h"
#include "nsThreadUtils.h"

namespace mozilla {

class WMFCDMProxy;

// This class is used to notify CDM related events called from MFCDMChild, and
// it will forward the relative calls to WMFCDMProxy on the main thread.
class WMFCDMProxyCallback final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WMFCDMProxyCallback);

  explicit WMFCDMProxyCallback(WMFCDMProxy* aProxy);

  void OnSessionMessage(const MFCDMKeyMessage& aMessage);

  void OnSessionKeyStatusesChange(const MFCDMKeyStatusChange& aKeyStatuses);

  void OnSessionKeyExpiration(const MFCDMKeyExpiration& aExpiration);

  void Shutdown();

 private:
  ~WMFCDMProxyCallback();
  RefPtr<WMFCDMProxy> mProxy;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_EME_MEDIAFOUNDATION_WMFCDMPROXYCALLBACK_H_

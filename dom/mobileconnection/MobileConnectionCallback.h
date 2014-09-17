/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MobileConnectionCallback_h
#define mozilla_dom_MobileConnectionCallback_h

#include "mozilla/dom/DOMRequest.h"
#include "mozilla/dom/MobileConnectionIPCSerializer.h"
#include "nsCOMPtr.h"
#include "nsIMobileConnectionService.h"

namespace mozilla {
namespace dom {

/**
 * A callback object for handling asynchronous request/response. This object is
 * created when an asynchronous request is made and should be destroyed after
 * Notify*Success/Error is called.
 * The modules hold the reference of MobileConnectionCallback in OOP mode and
 * non-OOP mode are different.
 * - OOP mode: MobileConnectionRequestChild
 * - non-OOP mode: MobileConnectionGonkService
 * The reference should be released after Notify*Success/Error is called.
 */
class MobileConnectionCallback MOZ_FINAL : public nsIMobileConnectionCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMOBILECONNECTIONCALLBACK

  MobileConnectionCallback(nsPIDOMWindow* aWindow, DOMRequest* aRequest);

  /**
   * Notify Success for Send/CancelMmi.
   */
  nsresult
  NotifySendCancelMmiSuccess(const nsAString& aServiceCode,
                             const nsAString& aStatusMessage);
  nsresult
  NotifySendCancelMmiSuccess(const nsAString& aServiceCode,
                             const nsAString& aStatusMessage,
                             JS::Handle<JS::Value> aAdditionalInformation);
  nsresult
  NotifySendCancelMmiSuccess(const nsAString& aServiceCode,
                             const nsAString& aStatusMessage,
                             uint16_t aAdditionalInformation);
  nsresult
  NotifySendCancelMmiSuccess(const nsAString& aServiceCode,
                             const nsAString& aStatusMessage,
                             const nsTArray<nsString>& aAdditionalInformation);
  nsresult
  NotifySendCancelMmiSuccess(const nsAString& aServiceCode,
                             const nsAString& aStatusMessage,
                             const nsTArray<IPC::MozCallForwardingOptions>& aAdditionalInformation);
  nsresult
  NotifySendCancelMmiSuccess(const MozMMIResult& aResult);

  /**
   * Notify Success for GetCallForwarding.
   */
  nsresult
  NotifyGetCallForwardingSuccess(const nsTArray<IPC::MozCallForwardingOptions>& aResults);

private:
  ~MobileConnectionCallback() {}

  nsresult
  NotifySuccess(JS::Handle<JS::Value> aResult);

  nsCOMPtr<nsPIDOMWindow> mWindow;
  nsRefPtr<DOMRequest> mRequest;
};

} // name space dom
} // name space mozilla

#endif // mozilla_dom_MobileConnectionCallback_h

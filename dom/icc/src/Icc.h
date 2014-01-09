/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Icc_h
#define mozilla_dom_Icc_h

#include "nsDOMEventTargetHelper.h"
#include "nsIIccProvider.h"

namespace mozilla {
namespace dom {

class Icc MOZ_FINAL : public nsDOMEventTargetHelper
{
public:
  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper)

  Icc(nsPIDOMWindow* aWindow, long aClientId, const nsAString& aIccId);

  void
  Shutdown();

  nsresult
  NotifyEvent(const nsAString& aName);

  nsresult
  NotifyStkEvent(const nsAString& aName, const nsAString& aMessage);

  nsString
  GetIccId()
  {
    return mIccId;
  }

  nsPIDOMWindow*
  GetParentObject() const
  {
    return GetOwner();
  }

  // WrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  // MozIcc WebIDL
  already_AddRefed<nsIDOMMozIccInfo>
  GetIccInfo() const;

  void
  GetCardState(nsString& aCardState) const;

  void
  SendStkResponse(const JSContext* aCx, JS::Handle<JS::Value> aCommand,
                  JS::Handle<JS::Value> aResponse, ErrorResult& aRv);

  void
  SendStkMenuSelection(uint16_t aItemIdentifier, bool aHelpRequested,
                       ErrorResult& aRv);

  void
  SendStkTimerExpiration(const JSContext* aCx, JS::Handle<JS::Value> aTimer,
                         ErrorResult& aRv);

  void
  SendStkEventDownload(const JSContext* aCx, JS::Handle<JS::Value> aEvent,
                       ErrorResult& aRv);

  already_AddRefed<nsISupports>
  GetCardLock(const nsAString& aLockType, ErrorResult& aRv);

  already_AddRefed<nsISupports>
  UnlockCardLock(const JSContext* aCx, JS::Handle<JS::Value> aInfo,
                 ErrorResult& aRv);

  already_AddRefed<nsISupports>
  SetCardLock(const JSContext* aCx, JS::Handle<JS::Value> aInfo,
              ErrorResult& aRv);

  already_AddRefed<nsISupports>
  GetCardLockRetryCount(const nsAString& aLockType, ErrorResult& aRv);

  already_AddRefed<nsISupports>
  ReadContacts(const nsAString& aContactType, ErrorResult& aRv);

  already_AddRefed<nsISupports>
  UpdateContact(const JSContext* aCx, const nsAString& aContactType,
                JS::Handle<JS::Value> aContact, const nsAString& aPin2,
                ErrorResult& aRv);

  already_AddRefed<nsISupports>
  IccOpenChannel(const nsAString& aAid, ErrorResult& aRv);

  already_AddRefed<nsISupports>
  IccExchangeAPDU(const JSContext* aCx, int32_t aChannel,
                  JS::Handle<JS::Value> aApdu, ErrorResult& aRv);

  already_AddRefed<nsISupports>
  IccCloseChannel(int32_t aChannel, ErrorResult& aRv);

  IMPL_EVENT_HANDLER(iccinfochange)
  IMPL_EVENT_HANDLER(cardstatechange)
  IMPL_EVENT_HANDLER(stkcommand)
  IMPL_EVENT_HANDLER(stksessionend)

private:
  bool mLive;
  uint32_t mClientId;
  nsString mIccId;
  // mProvider is a xpcom service and will be released at shutdown, so it
  // doesn't need to be cycle collected.
  nsCOMPtr<nsIIccProvider> mProvider;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_icc_Icc_h

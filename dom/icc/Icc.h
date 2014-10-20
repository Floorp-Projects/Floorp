/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Icc_h
#define mozilla_dom_Icc_h

#include "mozilla/dom/MozIccBinding.h" // For IccCardState
#include "mozilla/DOMEventTargetHelper.h"

class nsIIccInfo;
class nsIIccProvider;

namespace mozilla {
namespace dom {

class DOMRequest;
class OwningMozIccInfoOrMozGsmIccInfoOrMozCdmaIccInfo;

class Icc MOZ_FINAL : public DOMEventTargetHelper
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(Icc, DOMEventTargetHelper)
  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(DOMEventTargetHelper)

  Icc(nsPIDOMWindow* aWindow, long aClientId, nsIIccInfo* aIccInfo);

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

  void
  UpdateIccInfo(nsIIccInfo* aIccInfo);

  nsPIDOMWindow*
  GetParentObject() const
  {
    return GetOwner();
  }

  // WrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  // MozIcc WebIDL
  void
  GetIccInfo(Nullable<OwningMozIccInfoOrMozGsmIccInfoOrMozCdmaIccInfo>& aIccInfo) const;

  Nullable<IccCardState>
  GetCardState() const;

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

  already_AddRefed<DOMRequest>
  GetCardLock(const nsAString& aLockType, ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  UnlockCardLock(const JSContext* aCx, JS::Handle<JS::Value> aInfo,
                 ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  SetCardLock(const JSContext* aCx, JS::Handle<JS::Value> aInfo,
              ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  GetCardLockRetryCount(const nsAString& aLockType, ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  ReadContacts(const nsAString& aContactType, ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  UpdateContact(const JSContext* aCx, const nsAString& aContactType,
                JS::Handle<JS::Value> aContact, const nsAString& aPin2,
                ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  IccOpenChannel(const nsAString& aAid, ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  IccExchangeAPDU(const JSContext* aCx, int32_t aChannel,
                  JS::Handle<JS::Value> aApdu, ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  IccCloseChannel(int32_t aChannel, ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  MatchMvno(const nsAString& aMvnoType, const nsAString& aMatchData,
            ErrorResult& aRv);

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
  Nullable<OwningMozIccInfoOrMozGsmIccInfoOrMozCdmaIccInfo> mIccInfo;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_icc_Icc_h

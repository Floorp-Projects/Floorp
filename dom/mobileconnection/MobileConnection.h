/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MobileConnection_h
#define mozilla_dom_MobileConnection_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/DOMRequest.h"
#include "mozilla/dom/MobileConnectionInfo.h"
#include "mozilla/dom/MobileNetworkInfo.h"
#include "mozilla/dom/MozMobileConnectionBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIIccService.h"
#include "nsIMobileConnectionService.h"
#include "nsWeakPtr.h"

namespace mozilla {
namespace dom {

class MobileConnection final : public DOMEventTargetHelper
                             , private nsIMobileConnectionListener
                             , private nsIIccListener
{
  /**
   * Class MobileConnection doesn't actually expose
   * nsIMobileConnectionListener. Instead, it owns an
   * nsIMobileConnectionListener derived instance mListener and passes it to
   * nsIMobileConnectionService. The onreceived events are first delivered to
   * mListener and then forwarded to its owner, MobileConnection. See also bug
   * 775997 comment #51.
   */
  class Listener;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIMOBILECONNECTIONLISTENER
  NS_DECL_NSIICCLISTENER
  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(DOMEventTargetHelper)
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MobileConnection,
                                           DOMEventTargetHelper)

  MobileConnection(nsPIDOMWindow *aWindow, uint32_t aClientId);

  void
  Shutdown();

  virtual void
  DisconnectFromOwner() override;

  nsPIDOMWindow*
  GetParentObject() const
  {
    return GetOwner();
  }

  // WrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL interface
  void
  GetLastKnownNetwork(nsString& aRetVal) const;

  void
  GetLastKnownHomeNetwork(nsString& aRetVal) const;

  MobileConnectionInfo*
  Voice() const;

  MobileConnectionInfo*
  Data() const;

  void
  GetIccId(nsString& aRetVal) const;

  Nullable<MobileNetworkSelectionMode>
  GetNetworkSelectionMode() const;

  Nullable<MobileRadioState>
  GetRadioState() const;

  void
  GetSupportedNetworkTypes(nsTArray<MobileNetworkType>& aTypes) const;

  already_AddRefed<DOMRequest>
  GetNetworks(ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  SelectNetwork(MobileNetworkInfo& aNetwork, ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  SelectNetworkAutomatically(ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  SetPreferredNetworkType(MobilePreferredNetworkType& aType, ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  GetPreferredNetworkType(ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  SetRoamingPreference(MobileRoamingMode& aMode, ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  GetRoamingPreference(ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  SetVoicePrivacyMode(bool aEnabled, ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  GetVoicePrivacyMode(ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  SetCallForwardingOption(const MozCallForwardingOptions& aOptions,
                          ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  GetCallForwardingOption(uint16_t aReason, ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  SetCallBarringOption(const MozCallBarringOptions& aOptions, ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  GetCallBarringOption(const MozCallBarringOptions& aOptions, ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  ChangeCallBarringPassword(const MozCallBarringOptions& aOptions,
                            ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  SetCallWaitingOption(bool aEnabled, ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  GetCallWaitingOption(ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  SetCallingLineIdRestriction(uint16_t aMode, ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  GetCallingLineIdRestriction(ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  ExitEmergencyCbMode(ErrorResult& aRv);

  already_AddRefed<DOMRequest>
  SetRadioEnabled(bool aEnabled, ErrorResult& aRv);

  IMPL_EVENT_HANDLER(voicechange)
  IMPL_EVENT_HANDLER(datachange)
  IMPL_EVENT_HANDLER(dataerror)
  IMPL_EVENT_HANDLER(cfstatechange)
  IMPL_EVENT_HANDLER(emergencycbmodechange)
  IMPL_EVENT_HANDLER(otastatuschange)
  IMPL_EVENT_HANDLER(iccchange)
  IMPL_EVENT_HANDLER(radiostatechange)
  IMPL_EVENT_HANDLER(clirmodechange)

private:
  ~MobileConnection();

private:
  uint32_t mClientId;
  nsString mIccId;
  nsCOMPtr<nsIMobileConnection> mMobileConnection;
  nsCOMPtr<nsIIcc> mIccHandler;
  nsRefPtr<Listener> mListener;
  nsRefPtr<MobileConnectionInfo> mVoice;
  nsRefPtr<MobileConnectionInfo> mData;

  bool
  CheckPermission(const char* aType) const;

  void
  UpdateVoice();

  void
  UpdateData();

  bool
  UpdateIccId();

  nsresult
  NotifyError(nsIDOMDOMRequest* aRequest, const nsAString& aMessage);

  bool
  IsValidPassword(const nsAString& aPassword);

  bool
  IsValidCallBarringOptions(const MozCallBarringOptions& aOptions, bool isSetting);

  bool
  IsValidCallForwardingOptions(const MozCallForwardingOptions& aOptions);

  bool
  IsValidCallForwardingReason(int32_t aReason);

  bool
  IsValidCallForwardingAction(int32_t aAction);

  bool
  IsValidCallBarringProgram(int32_t aProgram);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MobileConnection_h

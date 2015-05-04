/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CellBroadcast.h"
#include "mozilla/dom/CellBroadcastMessage.h"
#include "mozilla/dom/MozCellBroadcastBinding.h"
#include "mozilla/dom/MozCellBroadcastEvent.h"
#include "nsServiceManagerUtils.h"

// Service instantiation
#include "ipc/CellBroadcastIPCService.h"
#if defined(MOZ_WIDGET_GONK) && defined(MOZ_B2G_RIL)
#include "nsIGonkCellBroadcastService.h"
#endif
#include "nsXULAppAPI.h" // For XRE_GetProcessType()

using namespace mozilla::dom;
using mozilla::ErrorResult;

/**
 * CellBroadcast::Listener Implementation.
 */

class CellBroadcast::Listener final : public nsICellBroadcastListener
{
private:
  CellBroadcast* mCellBroadcast;

public:
  NS_DECL_ISUPPORTS
  NS_FORWARD_SAFE_NSICELLBROADCASTLISTENER(mCellBroadcast)

  explicit Listener(CellBroadcast* aCellBroadcast)
    : mCellBroadcast(aCellBroadcast)
  {
    MOZ_ASSERT(mCellBroadcast);
  }

  void Disconnect()
  {
    MOZ_ASSERT(mCellBroadcast);
    mCellBroadcast = nullptr;
  }

private:
  ~Listener()
  {
    MOZ_ASSERT(!mCellBroadcast);
  }
};

NS_IMPL_ISUPPORTS(CellBroadcast::Listener, nsICellBroadcastListener)

/**
 * CellBroadcast Implementation.
 */

// static
already_AddRefed<CellBroadcast>
CellBroadcast::Create(nsPIDOMWindow* aWindow, ErrorResult& aRv)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aWindow->IsInnerWindow());

  nsCOMPtr<nsICellBroadcastService> service =
    do_GetService(CELLBROADCAST_SERVICE_CONTRACTID);
  if (!service) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<CellBroadcast> cb = new CellBroadcast(aWindow, service);
  return cb.forget();
}

CellBroadcast::CellBroadcast(nsPIDOMWindow *aWindow,
                             nsICellBroadcastService *aService)
  : DOMEventTargetHelper(aWindow)
{
  mListener = new Listener(this);
  DebugOnly<nsresult> rv = aService->RegisterListener(mListener);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                   "Failed registering Cell Broadcast callback");
}

CellBroadcast::~CellBroadcast()
{
  MOZ_ASSERT(mListener);

  mListener->Disconnect();
  nsCOMPtr<nsICellBroadcastService> service =
    do_GetService(CELLBROADCAST_SERVICE_CONTRACTID);
  if (service) {
    service->UnregisterListener(mListener);
  }

  mListener = nullptr;
}

NS_IMPL_ISUPPORTS_INHERITED0(CellBroadcast, DOMEventTargetHelper)

JSObject*
CellBroadcast::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MozCellBroadcastBinding::Wrap(aCx, this, aGivenProto);
}

// Forwarded nsICellBroadcastListener methods

NS_IMETHODIMP
CellBroadcast::NotifyMessageReceived(uint32_t aServiceId,
                                     uint32_t aGsmGeographicalScope,
                                     uint16_t aMessageCode,
                                     uint16_t aMessageId,
                                     const nsAString& aLanguage,
                                     const nsAString& aBody,
                                     uint32_t aMessageClass,
                                     DOMTimeStamp aTimestamp,
                                     uint32_t aCdmaServiceCategory,
                                     bool aHasEtwsInfo,
                                     uint32_t aEtwsWarningType,
                                     bool aEtwsEmergencyUserAlert,
                                     bool aEtwsPopup) {
  MozCellBroadcastEventInit init;
  init.mBubbles = true;
  init.mCancelable = false;
  init.mMessage = new CellBroadcastMessage(GetOwner(),
                                           aServiceId,
                                           aGsmGeographicalScope,
                                           aMessageCode,
                                           aMessageId,
                                           aLanguage,
                                           aBody,
                                           aMessageClass,
                                           aTimestamp,
                                           aCdmaServiceCategory,
                                           aHasEtwsInfo,
                                           aEtwsWarningType,
                                           aEtwsEmergencyUserAlert,
                                           aEtwsPopup);

  nsRefPtr<MozCellBroadcastEvent> event =
    MozCellBroadcastEvent::Constructor(this, NS_LITERAL_STRING("received"), init);
  return DispatchTrustedEvent(event);
}

already_AddRefed<nsICellBroadcastService>
NS_CreateCellBroadcastService()
{
  nsCOMPtr<nsICellBroadcastService> service;

  if (XRE_GetProcessType() == GeckoProcessType_Content) {
    service = new mozilla::dom::cellbroadcast::CellBroadcastIPCService();
#if defined(MOZ_WIDGET_GONK) && defined(MOZ_B2G_RIL)
  } else {
    service = do_GetService(GONK_CELLBROADCAST_SERVICE_CONTRACTID);
#endif
  }

  return service.forget();
}

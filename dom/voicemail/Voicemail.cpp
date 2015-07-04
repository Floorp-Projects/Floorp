/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Voicemail.h"

#include "mozilla/dom/MozVoicemailBinding.h"
#include "mozilla/dom/MozVoicemailEvent.h"
#include "mozilla/dom/MozVoicemailStatusBinding.h"

#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "nsContentUtils.h"
#include "nsServiceManagerUtils.h"

// Service instantiation
#include "ipc/VoicemailIPCService.h"
#if defined(MOZ_WIDGET_GONK) && defined(MOZ_B2G_RIL)
#include "nsIGonkVoicemailService.h"
#endif
#include "nsXULAppAPI.h" // For XRE_GetProcessType()

using namespace mozilla::dom;
using mozilla::ErrorResult;

class Voicemail::Listener final : public nsIVoicemailListener
{
  Voicemail* mVoicemail;

public:
  NS_DECL_ISUPPORTS
  NS_FORWARD_SAFE_NSIVOICEMAILLISTENER(mVoicemail)

  explicit Listener(Voicemail* aVoicemail)
    : mVoicemail(aVoicemail)
  {
    MOZ_ASSERT(mVoicemail);
  }

  void Disconnect()
  {
    MOZ_ASSERT(mVoicemail);
    mVoicemail = nullptr;
  }

private:
  ~Listener()
  {
    MOZ_ASSERT(!mVoicemail);
  }
};

NS_IMPL_ISUPPORTS(Voicemail::Listener, nsIVoicemailListener)

NS_IMPL_CYCLE_COLLECTION_INHERITED(Voicemail, DOMEventTargetHelper,
                                   mStatuses)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(Voicemail)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(Voicemail, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(Voicemail, DOMEventTargetHelper)

/* static */ already_AddRefed<Voicemail>
Voicemail::Create(nsPIDOMWindow* aWindow,
                  ErrorResult& aRv)
{
  nsCOMPtr<nsIVoicemailService> service =
    do_GetService(NS_VOICEMAIL_SERVICE_CONTRACTID);
  if (!service) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsPIDOMWindow* innerWindow = aWindow->IsInnerWindow() ?
    aWindow :
    aWindow->GetCurrentInnerWindow();

  nsRefPtr<Voicemail> voicemail = new Voicemail(innerWindow, service);
  return voicemail.forget();
}

Voicemail::Voicemail(nsPIDOMWindow* aWindow,
                     nsIVoicemailService* aService)
  : DOMEventTargetHelper(aWindow)
  , mService(aService)
{
  MOZ_ASSERT(mService);

  mListener = new Listener(this);
  DebugOnly<nsresult> rv = mService->RegisterListener(mListener);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                   "Failed registering voicemail messages with service");

  uint32_t length = 0;
  if (NS_SUCCEEDED(mService->GetNumItems(&length)) && length != 0) {
    mStatuses.SetLength(length);
  }
}

Voicemail::~Voicemail()
{
  MOZ_ASSERT(!mService && !mListener);
}

void
Voicemail::Shutdown()
{
  mListener->Disconnect();
  mService->UnregisterListener(mListener);

  mListener = nullptr;
  mService = nullptr;
  mStatuses.Clear();
}

JSObject*
Voicemail::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MozVoicemailBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<nsIVoicemailProvider>
Voicemail::GetItemByServiceId(const Optional<uint32_t>& aOptionalServiceId,
                              uint32_t& aActualServiceId) const
{
  if (!mService) {
    return nullptr;
  }

  nsCOMPtr<nsIVoicemailProvider> provider;
  if (aOptionalServiceId.WasPassed()) {
    aActualServiceId = aOptionalServiceId.Value();
    mService->GetItemByServiceId(aActualServiceId,
                                 getter_AddRefs(provider));
  } else {
    mService->GetDefaultItem(getter_AddRefs(provider));
    if (provider) {
      NS_ENSURE_SUCCESS(provider->GetServiceId(&aActualServiceId), nullptr);
    }
  }

  // For all retrieved providers, they should have service id
  // < mStatuses.Length().
  MOZ_ASSERT(!provider || aActualServiceId < mStatuses.Length());
  return provider.forget();
}

already_AddRefed<VoicemailStatus>
Voicemail::GetOrCreateStatus(uint32_t aServiceId,
                             nsIVoicemailProvider* aProvider)
{
  MOZ_ASSERT(aServiceId < mStatuses.Length());
  MOZ_ASSERT(aProvider);

  nsRefPtr<VoicemailStatus> res = mStatuses[aServiceId];
  if (!res) {
    mStatuses[aServiceId] = res = new VoicemailStatus(GetOwner(), aProvider);
  }

  return res.forget();
}

// MozVoicemail WebIDL

already_AddRefed<VoicemailStatus>
Voicemail::GetStatus(const Optional<uint32_t>& aServiceId,
                     ErrorResult& aRv)
{
  uint32_t actualServiceId = 0;
  nsCOMPtr<nsIVoicemailProvider> provider =
    GetItemByServiceId(aServiceId, actualServiceId);
  if (!provider) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  return GetOrCreateStatus(actualServiceId, provider);
}

void
Voicemail::GetNumber(const Optional<uint32_t>& aServiceId,
                     nsString& aNumber,
                     ErrorResult& aRv) const
{
  aNumber.SetIsVoid(true);

  uint32_t unused = 0;
  nsCOMPtr<nsIVoicemailProvider> provider =
    GetItemByServiceId(aServiceId, unused);
  if (!provider) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  aRv = provider->GetNumber(aNumber);
}

void
Voicemail::GetDisplayName(const Optional<uint32_t>& aServiceId,
                          nsString& aDisplayName,
                          ErrorResult& aRv) const
{
  aDisplayName.SetIsVoid(true);

  uint32_t unused = 0;
  nsCOMPtr<nsIVoicemailProvider> provider =
    GetItemByServiceId(aServiceId, unused);
  if (!provider) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  aRv = provider->GetDisplayName(aDisplayName);
}

// nsIVoicemailListener

NS_IMETHODIMP
Voicemail::NotifyInfoChanged(nsIVoicemailProvider* aProvider)
{
  // Ignored.
  return NS_OK;
}

NS_IMETHODIMP
Voicemail::NotifyStatusChanged(nsIVoicemailProvider* aProvider)
{
  NS_ENSURE_ARG_POINTER(aProvider);

  uint32_t serviceId = 0;
  if (NS_FAILED(aProvider->GetServiceId(&serviceId))) {
    return NS_ERROR_UNEXPECTED;
  }

  MozVoicemailEventInit init;
  init.mBubbles = false;
  init.mCancelable = false;
  init.mStatus = GetOrCreateStatus(serviceId, aProvider);

  nsRefPtr<MozVoicemailEvent> event =
    MozVoicemailEvent::Constructor(this, NS_LITERAL_STRING("statuschanged"), init);
  return DispatchTrustedEvent(event);
}

already_AddRefed<nsIVoicemailService>
NS_CreateVoicemailService()
{
  nsCOMPtr<nsIVoicemailService> service;

  if (XRE_IsContentProcess()) {
    service = new mozilla::dom::voicemail::VoicemailIPCService();
  } else {
#if defined(MOZ_B2G_RIL)
#if defined(MOZ_WIDGET_GONK)
    service = do_GetService(GONK_VOICEMAIL_SERVICE_CONTRACTID);
#endif // MOZ_WIDGET_GONK
#endif // MOZ_B2G_RIL
  }

  return service.forget();
}

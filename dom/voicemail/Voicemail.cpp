/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Voicemail.h"
#include "nsIDOMMozVoicemailStatus.h"
#include "nsIDOMMozVoicemailEvent.h"

#include "mozilla/Services.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfo.h"
#include "nsServiceManagerUtils.h"
#include "GeneratedEvents.h"

#define NS_RILCONTENTHELPER_CONTRACTID "@mozilla.org/ril/content-helper;1"

using namespace mozilla::dom;

class Voicemail::Listener : public nsIVoicemailListener
{
  Voicemail* mVoicemail;

public:
  NS_DECL_ISUPPORTS
  NS_FORWARD_SAFE_NSIVOICEMAILLISTENER(mVoicemail)

  Listener(Voicemail* aVoicemail)
    : mVoicemail(aVoicemail)
  {
    MOZ_ASSERT(mVoicemail);
  }

  void Disconnect()
  {
    MOZ_ASSERT(mVoicemail);
    mVoicemail = nullptr;
  }
};

NS_IMPL_ISUPPORTS1(Voicemail::Listener, nsIVoicemailListener)

DOMCI_DATA(MozVoicemail, Voicemail)

NS_INTERFACE_MAP_BEGIN(Voicemail)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozVoicemail)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozVoicemail)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(Voicemail, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(Voicemail, nsDOMEventTargetHelper)

Voicemail::Voicemail(nsPIDOMWindow* aWindow,
                     nsIVoicemailProvider* aProvider)
  : mProvider(aProvider)
{
  BindToOwner(aWindow);

  mListener = new Listener(this);
  DebugOnly<nsresult> rv = mProvider->RegisterVoicemailMsg(mListener);
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                   "Failed registering voicemail messages with provider");
}

Voicemail::~Voicemail()
{
  MOZ_ASSERT(mProvider && mListener);

  mListener->Disconnect();
  mProvider->UnregisterVoicemailMsg(mListener);
}

// nsIDOMMozVoicemail

NS_IMETHODIMP
Voicemail::GetStatus(nsIDOMMozVoicemailStatus** aStatus)
{
  *aStatus = nullptr;

  NS_ENSURE_STATE(mProvider);
  return mProvider->GetVoicemailStatus(aStatus);
}

NS_IMETHODIMP
Voicemail::GetNumber(nsAString& aNumber)
{
  NS_ENSURE_STATE(mProvider);
  aNumber.SetIsVoid(true);

  return mProvider->GetVoicemailNumber(aNumber);
}

NS_IMETHODIMP
Voicemail::GetDisplayName(nsAString& aDisplayName)
{
  NS_ENSURE_STATE(mProvider);
  aDisplayName.SetIsVoid(true);

  return mProvider->GetVoicemailDisplayName(aDisplayName);
}

NS_IMPL_EVENT_HANDLER(Voicemail, statuschanged)

// nsIVoicemailListener

NS_IMETHODIMP
Voicemail::NotifyStatusChanged(nsIDOMMozVoicemailStatus* aStatus)
{
  nsCOMPtr<nsIDOMEvent> event;
  NS_NewDOMMozVoicemailEvent(getter_AddRefs(event), this, nullptr, nullptr);

  nsCOMPtr<nsIDOMMozVoicemailEvent> ce = do_QueryInterface(event);
  nsresult rv = ce->InitMozVoicemailEvent(NS_LITERAL_STRING("statuschanged"),
                                          false, false, aStatus);
  NS_ENSURE_SUCCESS(rv, rv);

  return DispatchTrustedEvent(ce);
}

nsresult
NS_NewVoicemail(nsPIDOMWindow* aWindow, nsIDOMMozVoicemail** aVoicemail)
{
  nsPIDOMWindow* innerWindow = aWindow->IsInnerWindow() ?
    aWindow :
    aWindow->GetCurrentInnerWindow();

  nsCOMPtr<nsIVoicemailProvider> provider =
    do_GetService(NS_RILCONTENTHELPER_CONTRACTID);
  NS_ENSURE_STATE(provider);

  nsRefPtr<mozilla::dom::Voicemail> voicemail =
    new mozilla::dom::Voicemail(innerWindow, provider);
  voicemail.forget(aVoicemail);
  return NS_OK;
}

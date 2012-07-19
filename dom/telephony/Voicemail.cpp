/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Voicemail.h"
#include "nsIDOMVoicemailStatus.h"

#include "mozilla/Services.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfo.h"
#include "nsRadioInterfaceLayer.h"
#include "nsServiceManagerUtils.h"

#include "VoicemailEvent.h"

DOMCI_DATA(MozVoicemail, mozilla::dom::telephony::Voicemail)

USING_TELEPHONY_NAMESPACE

NS_IMPL_CYCLE_COLLECTION_CLASS(Voicemail)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(Voicemail,
                                                  nsDOMEventTargetHelper)
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(statuschanged)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(Voicemail,
                                                nsDOMEventTargetHelper)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(statuschanged)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(Voicemail)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozVoicemail)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozVoicemail)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(Voicemail, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(Voicemail, nsDOMEventTargetHelper)

NS_IMPL_ISUPPORTS1(Voicemail::RILVoicemailCallback, nsIRILVoicemailCallback)

Voicemail::Voicemail(nsPIDOMWindow* aWindow, nsIRILContentHelper* aRIL)
  : mRIL(aRIL)
{
  BindToOwner(aWindow);

  mRILVoicemailCallback = new RILVoicemailCallback(this);

  nsresult rv = aRIL->RegisterVoicemailCallback(mRILVoicemailCallback);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed registering voicemail callback with RIL");
  }
}

Voicemail::~Voicemail()
{
  if (mRIL && mRILVoicemailCallback) {
    mRIL->UnregisterVoicemailCallback(mRILVoicemailCallback);
  }
}

// nsIDOMMozVoicemail

NS_IMETHODIMP
Voicemail::GetStatus(nsIDOMMozVoicemailStatus** aStatus)
{
  *aStatus = nsnull;

  NS_ENSURE_STATE(mRIL);
  return mRIL->GetVoicemailStatus(aStatus);
}

NS_IMPL_EVENT_HANDLER(Voicemail, statuschanged)

// nsIRILVoicemailCallback

NS_IMETHODIMP
Voicemail::VoicemailNotification(nsIDOMMozVoicemailStatus* aStatus)
{
  nsRefPtr<VoicemailEvent> event = new VoicemailEvent(nsnull, nsnull);
  nsresult rv = event->InitVoicemailEvent(NS_LITERAL_STRING("statuschanged"),
                                          false, false, aStatus);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = event->SetTrusted(true);
  NS_ENSURE_SUCCESS(rv, rv);

  bool dummy;
  rv = DispatchEvent(static_cast<nsIDOMMozVoicemailEvent*>(event), &dummy);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
NS_NewVoicemail(nsPIDOMWindow* aWindow, nsIDOMMozVoicemail** aVoicemail)
{
  nsPIDOMWindow* innerWindow = aWindow->IsInnerWindow() ?
    aWindow :
    aWindow->GetCurrentInnerWindow();

  nsCOMPtr<nsIRILContentHelper> ril =
    do_GetService(NS_RILCONTENTHELPER_CONTRACTID);
  NS_ENSURE_STATE(ril);

  nsRefPtr<Voicemail> voicemail = new Voicemail(innerWindow, ril);
  voicemail.forget(aVoicemail);
  return NS_OK;
}

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
#include "nsRadioInterfaceLayer.h"
#include "nsServiceManagerUtils.h"
#include "GeneratedEvents.h"

DOMCI_DATA(MozVoicemail, mozilla::dom::Voicemail)

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED_0(Voicemail, nsDOMEventTargetHelper)

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

  rv = aRIL->RegisterVoicemailMsg();
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed registering voicemail messages with RIL");
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
  *aStatus = nullptr;

  NS_ENSURE_STATE(mRIL);
  return mRIL->GetVoicemailStatus(aStatus);
}

NS_IMETHODIMP
Voicemail::GetNumber(nsAString& aNumber)
{
  NS_ENSURE_STATE(mRIL);
  aNumber.SetIsVoid(true);

  return mRIL->GetVoicemailNumber(aNumber);
}

NS_IMETHODIMP
Voicemail::GetDisplayName(nsAString& aDisplayName)
{
  NS_ENSURE_STATE(mRIL);
  aDisplayName.SetIsVoid(true);

  return mRIL->GetVoicemailDisplayName(aDisplayName);
}

NS_IMPL_EVENT_HANDLER(Voicemail, statuschanged)

// nsIRILVoicemailCallback

NS_IMETHODIMP
Voicemail::VoicemailNotification(nsIDOMMozVoicemailStatus* aStatus)
{
  nsCOMPtr<nsIDOMEvent> event;
  NS_NewDOMMozVoicemailEvent(getter_AddRefs(event), nullptr, nullptr);

  nsCOMPtr<nsIDOMMozVoicemailEvent> ce = do_QueryInterface(event);
  nsresult rv = ce->InitMozVoicemailEvent(NS_LITERAL_STRING("statuschanged"),
                                          false, false, aStatus);
  NS_ENSURE_SUCCESS(rv, rv);

  return DispatchTrustedEvent(ce);
}

} // namespace dom
} // namespace mozilla

nsresult
NS_NewVoicemail(nsPIDOMWindow* aWindow, nsIDOMMozVoicemail** aVoicemail)
{
  nsPIDOMWindow* innerWindow = aWindow->IsInnerWindow() ?
    aWindow :
    aWindow->GetCurrentInnerWindow();

  nsCOMPtr<nsIRILContentHelper> ril =
    do_GetService(NS_RILCONTENTHELPER_CONTRACTID);
  NS_ENSURE_STATE(ril);

  nsRefPtr<mozilla::dom::Voicemail> voicemail =
    new mozilla::dom::Voicemail(innerWindow, ril);
  voicemail.forget(aVoicemail);
  return NS_OK;
}

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MIDIPermissionRequest.h"
#include "mozilla/dom/MIDIAccessManager.h"
#include "nsIGlobalObject.h"
#include "mozilla/Preferences.h"
#include "nsContentUtils.h"

//-------------------------------------------------
// MIDI Permission Requests
//-------------------------------------------------

NS_IMPL_CYCLE_COLLECTION(MIDIPermissionRequest, mWindow, mPromise)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MIDIPermissionRequest)
NS_INTERFACE_MAP_ENTRY(nsIContentPermissionRequest)
NS_INTERFACE_MAP_ENTRY(nsIRunnable)
NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContentPermissionRequest)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(MIDIPermissionRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MIDIPermissionRequest)

MIDIPermissionRequest::MIDIPermissionRequest(nsPIDOMWindowInner* aWindow,
                                             Promise* aPromise,
                                             const MIDIOptions& aOptions)
: mWindow(aWindow),
  mPromise(aPromise),
  mNeedsSysex(aOptions.mSysex),
  mRequester(new nsContentPermissionRequester(mWindow))
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aPromise, "aPromise should not be null!");
  MOZ_ASSERT(aWindow->GetDoc());
  mPrincipal = aWindow->GetDoc()->NodePrincipal();
  MOZ_ASSERT(mPrincipal);
}

MIDIPermissionRequest::~MIDIPermissionRequest()
{
}

NS_IMETHODIMP
MIDIPermissionRequest::GetIsHandlingUserInput(bool* aHandlingInput)
{
  *aHandlingInput = true;
  return NS_OK;
}

NS_IMETHODIMP
MIDIPermissionRequest::GetRequester(nsIContentPermissionRequester** aRequester)
{
  NS_ENSURE_ARG_POINTER(aRequester);
  nsCOMPtr<nsIContentPermissionRequester> requester = mRequester;
  requester.forget(aRequester);
  return NS_OK;
}

NS_IMETHODIMP
MIDIPermissionRequest::GetTypes(nsIArray** aTypes)
{
  NS_ENSURE_ARG_POINTER(aTypes);
  nsTArray<nsString> options;
  if (mNeedsSysex) {
    options.AppendElement(NS_LITERAL_STRING("sysex"));
  }
  return nsContentPermissionUtils::CreatePermissionArray(NS_LITERAL_CSTRING("midi"),
                                                         NS_LITERAL_CSTRING("unused"),
                                                         options,
                                                         aTypes);
}

NS_IMETHODIMP
MIDIPermissionRequest::GetPrincipal(nsIPrincipal** aRequestingPrincipal)
{
  NS_ENSURE_ARG_POINTER(aRequestingPrincipal);
  NS_IF_ADDREF(*aRequestingPrincipal = mPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
MIDIPermissionRequest::GetWindow(mozIDOMWindow** aRequestingWindow)
{
  NS_ENSURE_ARG_POINTER(aRequestingWindow);
  NS_IF_ADDREF(*aRequestingWindow = mWindow);
  return NS_OK;
}

NS_IMETHODIMP
MIDIPermissionRequest::GetElement(Element** aRequestingElement)
{
  NS_ENSURE_ARG_POINTER(aRequestingElement);
  *aRequestingElement = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
MIDIPermissionRequest::Cancel()
{
  mPromise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
  return NS_OK;
}

NS_IMETHODIMP
MIDIPermissionRequest::Allow(JS::HandleValue aChoices)
{
  MOZ_ASSERT(aChoices.isUndefined());
  MIDIAccessManager* mgr = MIDIAccessManager::Get();
  mgr->CreateMIDIAccess(mWindow, mNeedsSysex, mPromise);
  return NS_OK;
}

NS_IMETHODIMP
MIDIPermissionRequest::Run()
{
  // If the testing flag is true, skip dialog
  if (Preferences::GetBool("midi.prompt.testing", false)) {
    bool allow = Preferences::GetBool("media.navigator.permission.disabled", false);
    if (allow) {
      Allow(JS::UndefinedHandleValue);
    } else {
      Cancel();
    }
    return NS_OK;
  }

  // If we already have sysex perms, allow.
  if (nsContentUtils::IsExactSitePermAllow(mPrincipal, "midi-sysex")) {
    Allow(JS::UndefinedHandleValue);
    return NS_OK;
  }

  // If we have no perms, or only have midi and are asking for sysex, pop dialog
  if (NS_FAILED(nsContentPermissionUtils::AskPermission(this, mWindow))) {
    Cancel();
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}


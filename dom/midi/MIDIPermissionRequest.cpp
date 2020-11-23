/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MIDIPermissionRequest.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/MIDIAccessManager.h"
#include "mozilla/dom/MIDIOptionsBinding.h"
#include "nsIGlobalObject.h"
#include "mozilla/Preferences.h"
#include "nsContentUtils.h"

//-------------------------------------------------
// MIDI Permission Requests
//-------------------------------------------------

using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_INHERITED(MIDIPermissionRequest,
                                   ContentPermissionRequestBase, mPromise)

NS_IMPL_QUERY_INTERFACE_CYCLE_COLLECTION_INHERITED(MIDIPermissionRequest,
                                                   ContentPermissionRequestBase,
                                                   nsIRunnable)

NS_IMPL_ADDREF_INHERITED(MIDIPermissionRequest, ContentPermissionRequestBase)
NS_IMPL_RELEASE_INHERITED(MIDIPermissionRequest, ContentPermissionRequestBase)

MIDIPermissionRequest::MIDIPermissionRequest(nsPIDOMWindowInner* aWindow,
                                             Promise* aPromise,
                                             const MIDIOptions& aOptions)
    : ContentPermissionRequestBase(
          aWindow->GetDoc()->NodePrincipal(), aWindow,
          ""_ns,  // We check prefs in a custom way here
          "midi"_ns),
      mPromise(aPromise),
      mNeedsSysex(aOptions.mSysex) {
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aPromise, "aPromise should not be null!");
  MOZ_ASSERT(aWindow->GetDoc());
  mPrincipal = aWindow->GetDoc()->NodePrincipal();
  MOZ_ASSERT(mPrincipal);
}

NS_IMETHODIMP
MIDIPermissionRequest::GetTypes(nsIArray** aTypes) {
  NS_ENSURE_ARG_POINTER(aTypes);
  nsTArray<nsString> options;
  if (mNeedsSysex) {
    options.AppendElement(u"sysex"_ns);
  }
  return nsContentPermissionUtils::CreatePermissionArray(mType, options,
                                                         aTypes);
}

NS_IMETHODIMP
MIDIPermissionRequest::Cancel() {
  mPromise->MaybeReject(NS_ERROR_DOM_SECURITY_ERR);
  return NS_OK;
}

NS_IMETHODIMP
MIDIPermissionRequest::Allow(JS::HandleValue aChoices) {
  MOZ_ASSERT(aChoices.isUndefined());
  MIDIAccessManager* mgr = MIDIAccessManager::Get();
  mgr->CreateMIDIAccess(mWindow, mNeedsSysex, mPromise);
  return NS_OK;
}

NS_IMETHODIMP
MIDIPermissionRequest::Run() {
  // If the testing flag is true, skip dialog
  if (Preferences::GetBool("midi.prompt.testing", false)) {
    bool allow =
        Preferences::GetBool("media.navigator.permission.disabled", false);
    if (allow) {
      Allow(JS::UndefinedHandleValue);
    } else {
      Cancel();
    }
    return NS_OK;
  }

  // If we already have sysex perms, allow.
  if (nsContentUtils::IsExactSitePermAllow(mPrincipal, "midi-sysex"_ns)) {
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

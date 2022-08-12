/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MIDIPermissionRequest.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/MIDIAccessManager.h"
#include "mozilla/dom/MIDIOptionsBinding.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/StaticPrefs_dom.h"
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
  // NB: We always request midi-sysex, and the base |midi| permission is unused.
  // This could be cleaned up at some point.
  options.AppendElement(u"sysex"_ns);
  return nsContentPermissionUtils::CreatePermissionArray(mType, options,
                                                         aTypes);
}

NS_IMETHODIMP
MIDIPermissionRequest::Cancel() {
  mPromise->MaybeRejectWithSecurityError(
      "WebMIDI requires a site permission add-on to activate — see "
      "https://extensionworkshop.com/documentation/publish/"
      "site-permission-add-on/ for details.");
  return NS_OK;
}

NS_IMETHODIMP
MIDIPermissionRequest::Allow(JS::Handle<JS::Value> aChoices) {
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

  // Both the spec and our original implementation of WebMIDI have two
  // conceptual permission levels: with and without sysex functionality.
  // However, our current implementation just has one level, and requires the
  // more-powerful |midi-sysex| permission irrespective of the mode requested in
  // requestMIDIAccess.
  constexpr auto kPermName = "midi-sysex"_ns;

  // First, check for an explicit allow/deny. Note that we want to support
  // granting a permission on the base domain and then using it on a subdomain,
  // which is why we use the non-"Exact" variants of these APIs. See bug
  // 1757218.
  if (nsContentUtils::IsSitePermAllow(mPrincipal, kPermName)) {
    Allow(JS::UndefinedHandleValue);
    return NS_OK;
  }

  if (nsContentUtils::IsSitePermDeny(mPrincipal, kPermName)) {
    Cancel();
    return NS_OK;
  }

  // If the add-on is not installed, auto-deny (except for localhost).
  if (StaticPrefs::dom_webmidi_gated() &&
      !nsContentUtils::HasSitePerm(mPrincipal, kPermName) &&
      !BasePrincipal::Cast(mPrincipal)->IsLoopbackHost()) {
    Cancel();
    return NS_OK;
  }

  // We can only get here for localhost, if add-on gating is disabled or if the
  // add-on is installed but the user has subsequently changed the permission
  // from ALLOW to ASK. In that unusual case, throw up a prompt.
  if (NS_FAILED(nsContentPermissionUtils::AskPermission(this, mWindow))) {
    Cancel();
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

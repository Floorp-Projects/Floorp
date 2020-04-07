/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XRPermissionRequest.h"
#include "nsIGlobalObject.h"
#include "mozilla/Preferences.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

//-------------------------------------------------
// XR Permission Requests
//-------------------------------------------------

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(XRPermissionRequest,
                                               ContentPermissionRequestBase)

NS_IMPL_CYCLE_COLLECTION_INHERITED(XRPermissionRequest,
                                   ContentPermissionRequestBase)

XRPermissionRequest::XRPermissionRequest(nsPIDOMWindowInner* aWindow,
                                         uint64_t aWindowId)
    : ContentPermissionRequestBase(aWindow->GetDoc()->NodePrincipal(), aWindow,
                                   NS_LITERAL_CSTRING("dom.vr"),
                                   NS_LITERAL_CSTRING("xr")),
      mWindowId(aWindowId) {
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aWindow->GetDoc());
  mPrincipal = aWindow->GetDoc()->NodePrincipal();
  MOZ_ASSERT(mPrincipal);
}

NS_IMETHODIMP
XRPermissionRequest::Cancel() {
  nsGlobalWindowInner* window =
      nsGlobalWindowInner::GetInnerWindowWithId(mWindowId);
  if (!window) {
    return NS_OK;
  }
  window->OnXRPermissionRequestCancel();
  return NS_OK;
}

NS_IMETHODIMP
XRPermissionRequest::Allow(JS::HandleValue aChoices) {
  MOZ_ASSERT(aChoices.isUndefined());
  nsGlobalWindowInner* window =
      nsGlobalWindowInner::GetInnerWindowWithId(mWindowId);
  if (!window) {
    return NS_OK;
  }
  window->OnXRPermissionRequestAllow();
  return NS_OK;
}

nsresult XRPermissionRequest::Start() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!CheckPermissionDelegate()) {
    return Cancel();
  }
  PromptResult pr = CheckPromptPrefs();
  if (pr == PromptResult::Granted) {
    return Allow(JS::UndefinedHandleValue);
  }
  if (pr == PromptResult::Denied) {
    return Cancel();
  }

  return nsContentPermissionUtils::AskPermission(this, mWindow);
}

}  // namespace dom
}  // namespace mozilla

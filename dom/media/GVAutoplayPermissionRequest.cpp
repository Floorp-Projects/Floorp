/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GVAutoplayPermissionRequest.h"

namespace mozilla {
namespace dom {

using RType = GVAutoplayRequestType;
using RStatus = GVAutoplayRequestStatus;

NS_IMPL_CYCLE_COLLECTION_INHERITED(GVAutoplayPermissionRequest,
                                   ContentPermissionRequestBase)

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(GVAutoplayPermissionRequest,
                                               ContentPermissionRequestBase)

/* static */
already_AddRefed<GVAutoplayPermissionRequest>
GVAutoplayPermissionRequest::CreateRequest(nsGlobalWindowInner* aWindow,
                                           RType aType) {
  if (!aWindow || !aWindow->GetPrincipal()) {
    return nullptr;
  }
  RefPtr<GVAutoplayPermissionRequest> request =
      new GVAutoplayPermissionRequest(aWindow, aType);
  return request.forget();
}

GVAutoplayPermissionRequest::GVAutoplayPermissionRequest(
    nsGlobalWindowInner* aWindow, RType aType)
    : ContentPermissionRequestBase(
          aWindow->GetPrincipal(), aWindow,
          NS_LITERAL_CSTRING(""),  // No testing pref used in this class
          aType == RType::eAUDIBLE
              ? NS_LITERAL_CSTRING("autoplay-media-audible")
              : NS_LITERAL_CSTRING("autoplay-media-inaudible")),
      mType(aType) {}

GVAutoplayPermissionRequest::~GVAutoplayPermissionRequest() {
  // If user doesn't response to the request before it gets destroyed (ex.
  // request dismissed, tab closed, naviagation to a new page), then we should
  // treat it as a denial.
  Cancel();
}

NS_IMETHODIMP
GVAutoplayPermissionRequest::Cancel() {
  // TODO : It will be implemented in the following patch.
  return NS_OK;
}

NS_IMETHODIMP
GVAutoplayPermissionRequest::Allow(JS::HandleValue aChoices) {
  // TODO : It will be implemented in the following patch.
  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla

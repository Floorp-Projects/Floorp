/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AuthrsService.h"
#include "AuthrsBridge_ffi.h"
#include "nsIWebAuthnService.h"
#include "nsCOMPtr.h"

namespace mozilla::dom {

already_AddRefed<nsIWebAuthnService> NewAuthrsService() {
  nsCOMPtr<nsIWebAuthnService> webauthnService;
  nsresult rv = authrs_service_constructor(getter_AddRefs(webauthnService));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }
  return webauthnService.forget();
}

}  // namespace mozilla::dom

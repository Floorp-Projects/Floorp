/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AuthrsTransport.h"
#include "AuthrsBridge_ffi.h"
#include "nsIWebAuthnController.h"
#include "nsCOMPtr.h"

namespace mozilla::dom {

already_AddRefed<nsIWebAuthnTransport> NewAuthrsTransport() {
  nsCOMPtr<nsIWebAuthnTransport> transport;
  nsresult rv = authrs_transport_constructor(getter_AddRefs(transport));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }
  return transport.forget();
}

}  // namespace mozilla::dom

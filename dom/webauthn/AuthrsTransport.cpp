/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AuthrsTransport.h"
#include "nsIWebAuthnController.h"
#include "nsCOMPtr.h"

namespace {
extern "C" {

// Implemented in Rust
nsresult authrs_transport_constructor(nsIWebAuthnTransport** result);

}  // extern "C"
}  // namespace

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

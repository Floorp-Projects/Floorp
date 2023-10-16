/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_WEBAUTHN_AUTHRS_BRIDGE_H_
#define DOM_WEBAUTHN_AUTHRS_BRIDGE_H_

#include "mozilla/AlreadyAddRefed.h"
#include "nsIWebAuthnController.h"

namespace mozilla::dom {

already_AddRefed<nsIWebAuthnTransport> NewAuthrsTransport();

}  // namespace mozilla::dom

#endif  // DOM_WEBAUTHN_AUTHRS_BRIDGE_H_

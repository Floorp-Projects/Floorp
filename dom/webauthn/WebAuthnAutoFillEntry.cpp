/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebAuthnAutoFillEntry.h"

namespace mozilla::dom {

NS_IMPL_ISUPPORTS(WebAuthnAutoFillEntry, nsIWebAuthnAutoFillEntry)

NS_IMETHODIMP
WebAuthnAutoFillEntry::GetProvider(uint8_t* aProvider) {
  *aProvider = mProvider;
  return NS_OK;
}

NS_IMETHODIMP
WebAuthnAutoFillEntry::GetUserName(nsAString& aUserName) {
  aUserName.Assign(mUserName);
  return NS_OK;
}

NS_IMETHODIMP
WebAuthnAutoFillEntry::GetRpId(nsAString& aRpId) {
  aRpId.Assign(mRpId);
  return NS_OK;
}

NS_IMETHODIMP
WebAuthnAutoFillEntry::GetCredentialId(nsTArray<uint8_t>& aCredentialId) {
  aCredentialId.Assign(mCredentialId);
  return NS_OK;
}

}  // namespace mozilla::dom

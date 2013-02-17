/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "Crypto.h"
#include "nsIDOMClassInfo.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

NS_INTERFACE_MAP_BEGIN(Crypto)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCrypto)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Crypto)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(Crypto)
NS_IMPL_RELEASE(Crypto)

Crypto::Crypto()
{
  MOZ_COUNT_CTOR(Crypto);
}

Crypto::~Crypto()
{
  MOZ_COUNT_DTOR(Crypto);
}

#ifndef MOZ_DISABLE_CRYPTOLEGACY
// Stub out the legacy nsIDOMCrypto methods. The actual
// implementations are in security/manager/ssl/src/nsCrypto.{cpp,h}

NS_IMETHODIMP
Crypto::GetVersion(nsAString & aVersion)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
Crypto::GetEnableSmartCardEvents(bool *aEnableSmartCardEvents)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
Crypto::SetEnableSmartCardEvents(bool aEnableSmartCardEvents)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
Crypto::GenerateCRMFRequest(nsIDOMCRMFObject * *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
Crypto::ImportUserCertificates(const nsAString & nickname,
                               const nsAString & cmmfResponse,
                               bool doForcedBackup, nsAString & _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
Crypto::PopChallengeResponse(const nsAString & challenge,
                             nsAString & _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
Crypto::Random(int32_t numBytes, nsAString & _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
Crypto::SignText(const nsAString & stringToSign, const nsAString & caOption,
                 nsAString & _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
Crypto::Logout()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
Crypto::DisableRightClick()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
#endif

} // namespace dom
} // namespace mozilla

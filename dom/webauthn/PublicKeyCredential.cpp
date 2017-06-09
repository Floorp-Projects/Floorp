/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PublicKeyCredential.h"
#include "mozilla/dom/WebAuthenticationBinding.h"
#include "nsCycleCollectionParticipant.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(PublicKeyCredential, Credential, mResponse)

NS_IMPL_ADDREF_INHERITED(PublicKeyCredential, Credential)
NS_IMPL_RELEASE_INHERITED(PublicKeyCredential, Credential)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(PublicKeyCredential)
NS_INTERFACE_MAP_END_INHERITING(Credential)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(PublicKeyCredential, Credential)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

PublicKeyCredential::PublicKeyCredential(nsPIDOMWindowInner* aParent)
  : Credential(aParent)
{}

PublicKeyCredential::~PublicKeyCredential()
{}

JSObject*
PublicKeyCredential::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto)
{
  return PublicKeyCredentialBinding::Wrap(aCx, this, aGivenProto);
}

void
PublicKeyCredential::GetRawId(JSContext* aCx,
                              JS::MutableHandle<JSObject*> aRetVal) const
{
  aRetVal.set(mRawId.ToUint8Array(aCx));
}

already_AddRefed<AuthenticatorResponse>
PublicKeyCredential::Response() const
{
  RefPtr<AuthenticatorResponse> temp(mResponse);
  return temp.forget();
}

nsresult
PublicKeyCredential::SetRawId(CryptoBuffer& aBuffer)
{
  if (NS_WARN_IF(!mRawId.Assign(aBuffer))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

void
PublicKeyCredential::SetResponse(RefPtr<AuthenticatorResponse> aResponse)
{
  mResponse = aResponse;
}

} // namespace dom
} // namespace mozilla

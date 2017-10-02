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

NS_IMPL_CYCLE_COLLECTION_CLASS(PublicKeyCredential)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(PublicKeyCredential,
                                                Credential)
  tmp->mRawIdCachedObj = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(PublicKeyCredential, Credential)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mRawIdCachedObj)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(PublicKeyCredential, Credential)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(PublicKeyCredential, Credential)
NS_IMPL_RELEASE_INHERITED(PublicKeyCredential, Credential)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PublicKeyCredential)
NS_INTERFACE_MAP_END_INHERITING(Credential)

PublicKeyCredential::PublicKeyCredential(nsPIDOMWindowInner* aParent)
  : Credential(aParent)
  , mRawIdCachedObj(nullptr)
{
  mozilla::HoldJSObjects(this);
}

PublicKeyCredential::~PublicKeyCredential()
{
  mozilla::DropJSObjects(this);
}

JSObject*
PublicKeyCredential::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto)
{
  return PublicKeyCredentialBinding::Wrap(aCx, this, aGivenProto);
}

void
PublicKeyCredential::GetRawId(JSContext* aCx,
                              JS::MutableHandle<JSObject*> aRetVal)
{
  if (!mRawIdCachedObj) {
    mRawIdCachedObj = mRawId.ToArrayBuffer(aCx);
  }
  aRetVal.set(mRawIdCachedObj);
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

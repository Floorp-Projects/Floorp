/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Promise.h"
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
  return PublicKeyCredential_Binding::Wrap(aCx, this, aGivenProto);
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

/* static */ already_AddRefed<Promise>
PublicKeyCredential::IsUserVerifyingPlatformAuthenticatorAvailable(GlobalObject& aGlobal)
{
  nsIGlobalObject* globalObject = xpc::CurrentNativeGlobal(aGlobal.Context());
  if (NS_WARN_IF(!globalObject)) {
    return nullptr;
  }

  ErrorResult rv;
  RefPtr<Promise> promise = Promise::Create(globalObject, rv);
  if (rv.Failed()) {
    return nullptr;
  }

  // https://w3c.github.io/webauthn/#isUserVerifyingPlatformAuthenticatorAvailable
  //
  // We currently implement no platform authenticators, so this would always
  // resolve to false. For those cases, the spec recommends a resolve timeout
  // on the order of 10 minutes to avoid fingerprinting.
  //
  // A simple solution is thus to never resolve the promise, otherwise we'd
  // have to track every single call to this method along with a promise
  // and timer to resolve it after exactly X minutes.
  //
  // A Relying Party has to deal with a non-response in a timely fashion, so
  // we can keep this as-is (and not resolve) even when we support platform
  // authenticators but they're not available, or a user rejects a website's
  // request to use them.
  return promise.forget();
}

void
PublicKeyCredential::GetClientExtensionResults(AuthenticationExtensionsClientOutputs& aResult)
{
  aResult = mClientExtensionOutputs;
}

void
PublicKeyCredential::SetClientExtensionResultAppId(bool aResult)
{
  mClientExtensionOutputs.mAppid.Construct();
  mClientExtensionOutputs.mAppid.Value() = aResult;
}


} // namespace dom
} // namespace mozilla

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Fetch.h"
#include "mozilla/dom/IdentityCredential.h"
#include "mozilla/dom/IdentityNetworkHelpers.h"
#include "mozilla/dom/Request.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/ExpandedPrincipal.h"
#include "mozilla/NullPrincipal.h"
#include "nsEffectiveTLDService.h"
#include "nsITimer.h"
#include "nsIXPConnect.h"
#include "nsNetUtil.h"
#include "nsStringStream.h"
#include "nsURLHelper.h"

namespace mozilla::dom {

IdentityCredential::~IdentityCredential() = default;

JSObject* IdentityCredential::WrapObject(JSContext* aCx,
                                         JS::Handle<JSObject*> aGivenProto) {
  return IdentityCredential_Binding::Wrap(aCx, this, aGivenProto);
}

IdentityCredential::IdentityCredential(nsPIDOMWindowInner* aParent)
    : Credential(aParent) {}

void IdentityCredential::CopyValuesFrom(const IPCIdentityCredential& aOther) {
  this->SetToken(aOther.token());
  this->SetId(aOther.id());
  this->SetType(aOther.type());
}

IPCIdentityCredential IdentityCredential::MakeIPCIdentityCredential() {
  nsString token, id, type;
  GetToken(token);
  GetId(id);
  GetType(type);
  IPCIdentityCredential result;
  result.token() = token;
  result.id() = id;
  result.type() = type;
  return result;
}

void IdentityCredential::GetToken(nsAString& aToken) const {
  aToken.Assign(mToken);
}
void IdentityCredential::SetToken(const nsAString& aToken) {
  mToken.Assign(aToken);
}

// static
RefPtr<IdentityCredential::GetIdentityCredentialPromise>
IdentityCredential::DiscoverFromExternalSource(
    nsPIDOMWindowInner* aParent, const CredentialRequestOptions& aOptions,
    bool aSameOriginWithAncestors) {
  MOZ_ASSERT(XRE_IsContentProcess());
  MOZ_ASSERT(aParent);
  // Prevent origin confusion by requiring no cross domain iframes
  // in this one's ancestry
  if (!aSameOriginWithAncestors) {
    return IdentityCredential::GetIdentityCredentialPromise::CreateAndReject(
        NS_ERROR_DOM_NOT_ALLOWED_ERR, __func__);
  }

  Document* parentDocument = aParent->GetExtantDoc();
  if (!parentDocument) {
    return IdentityCredential::GetIdentityCredentialPromise::CreateAndReject(
        NS_ERROR_FAILURE, __func__);
  }

  RefPtr<IdentityCredential::GetIdentityCredentialPromise::Private> result =
      new IdentityCredential::GetIdentityCredentialPromise::Private(__func__);

  nsCOMPtr<nsITimer> timeout;
  RefPtr<IdentityCredential::GetIdentityCredentialPromise::Private> promise =
      result;
  nsresult rv = NS_NewTimerWithFuncCallback(
      getter_AddRefs(timeout),
      [](nsITimer* aTimer, void* aClosure) -> void {
        auto* promise = static_cast<
            IdentityCredential::GetIdentityCredentialPromise::Private*>(
            aClosure);
        if (!promise->IsResolved()) {
          promise->Reject(NS_ERROR_DOM_NETWORK_ERR, __func__);
        }
        // This releases the promise we forgot when we returned from
        // this function.
        NS_RELEASE(promise);
      },
      promise, 120000, nsITimer::TYPE_ONE_SHOT,
      "IdentityCredentialTimeoutCallback");
  if (NS_WARN_IF(NS_FAILED(rv))) {
    result->Reject(NS_ERROR_FAILURE, __func__);
    return result.forget();
  }

  // Kick the request off to the main process and translate the result to the
  // expected type when we get a result.
  MOZ_ASSERT(aOptions.mIdentity.WasPassed());
  RefPtr<WindowGlobalChild> wgc = aParent->GetWindowGlobalChild();
  MOZ_ASSERT(wgc);
  RefPtr<IdentityCredential> credential = new IdentityCredential(aParent);
  wgc->SendDiscoverIdentityCredentialFromExternalSource(
         aOptions.mIdentity.Value())
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [result, timeout,
           credential](const WindowGlobalChild::
                           DiscoverIdentityCredentialFromExternalSourcePromise::
                               ResolveValueType& aResult) {
            if (aResult.isSome()) {
              credential->CopyValuesFrom(aResult.value());
              result->Resolve(credential, __func__);
            } else if (
                !StaticPrefs::
                    dom_security_credentialmanagement_identity_wait_for_timeout()) {
              result->Reject(NS_ERROR_DOM_UNKNOWN_ERR, __func__);
            }
          },
          [result](const WindowGlobalChild::
                       DiscoverIdentityCredentialFromExternalSourcePromise::
                           RejectValueType& aResult) { return; });

  return result;
}

// static
RefPtr<IdentityCredential::GetIPCIdentityCredentialPromise>
IdentityCredential::DiscoverFromExternalSourceInMainProcess(
    nsIPrincipal* aPrincipal,
    const IdentityCredentialRequestOptions& aOptions) {
  MOZ_ASSERT(XRE_IsParentProcess());

  // Make sure we have exactly one provider.
  if (!aOptions.mProviders.WasPassed() ||
      aOptions.mProviders.Value().Length() != 1) {
    return IdentityCredential::GetIPCIdentityCredentialPromise::CreateAndReject(
        NS_ERROR_DOM_NOT_ALLOWED_ERR, __func__);
  }

  // Get that provider
  IdentityProvider provider(aOptions.mProviders.Value()[0]);

  return IdentityCredential::CreateCredential(aPrincipal, provider);
}

}  // namespace mozilla::dom

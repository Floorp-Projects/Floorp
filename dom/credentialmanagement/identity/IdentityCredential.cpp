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

// static
RefPtr<IdentityCredential::GetIPCIdentityCredentialPromise>
IdentityCredential::CreateCredential(nsIPrincipal* aPrincipal,
                                     const IdentityProvider& aProvider) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(aPrincipal);

  nsCOMPtr<nsIPrincipal> argumentPrincipal = aPrincipal;

  return IdentityCredential::CheckRootManifest(aPrincipal, aProvider)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [aProvider, argumentPrincipal](bool valid) {
            if (valid) {
              return IdentityCredential::FetchInternalManifest(
                  argumentPrincipal, aProvider);
            }
            return IdentityCredential::GetManifestPromise::CreateAndReject(
                NS_ERROR_FAILURE, __func__);
          },
          [](nsresult error) {
            return IdentityCredential::GetManifestPromise::CreateAndReject(
                error, __func__);
          })
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [argumentPrincipal,
           aProvider](const IdentityInternalManifest& manifest) {
            return IdentityCredential::FetchAccountList(argumentPrincipal,
                                                        aProvider, manifest);
          },
          [](nsresult error) {
            return IdentityCredential::GetAccountListPromise::CreateAndReject(
                error, __func__);
          })
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [argumentPrincipal, aProvider](
              const Tuple<IdentityInternalManifest, IdentityAccountList>&
                  promiseResult) {
            IdentityInternalManifest currentManifest;
            IdentityAccountList accountList;
            Tie(currentManifest, accountList) = promiseResult;
            // Bug 1782088: We currently just use the first account
            if (!accountList.mAccounts.WasPassed() ||
                accountList.mAccounts.Value().Length() == 0) {
              return IdentityCredential::GetTokenPromise::CreateAndReject(
                  NS_ERROR_FAILURE, __func__);
            }
            return IdentityCredential::FetchToken(
                argumentPrincipal, aProvider, currentManifest,
                accountList.mAccounts.Value()[0]);
          },
          [](nsresult error) {
            return IdentityCredential::GetTokenPromise::CreateAndReject(
                error, __func__);
          })
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [aProvider](
              const Tuple<IdentityToken, IdentityAccount>& promiseResult) {
            IdentityToken token;
            IdentityAccount account;
            Tie(token, account) = promiseResult;
            IPCIdentityCredential credential;
            credential.token() = token.mToken;
            credential.id() = account.mId;
            credential.type() = u"identity"_ns;
            return IdentityCredential::GetIPCIdentityCredentialPromise::
                CreateAndResolve(credential, __func__);
          },
          [](nsresult error) {
            return IdentityCredential::GetIPCIdentityCredentialPromise::
                CreateAndReject(error, __func__);
          });
}

// static
RefPtr<IdentityCredential::ValidationPromise>
IdentityCredential::CheckRootManifest(nsIPrincipal* aPrincipal,
                                      const IdentityProvider& aProvider) {
  MOZ_ASSERT(XRE_IsParentProcess());
  // Build the URL
  nsString configLocation = aProvider.mConfigURL;
  nsCOMPtr<nsIURI> configURI;
  nsresult rv = NS_NewURI(getter_AddRefs(configURI), configLocation);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IdentityCredential::ValidationPromise::CreateAndReject(rv, __func__);
  }
  RefPtr<nsEffectiveTLDService> etld = nsEffectiveTLDService::GetInstance();
  if (!etld) {
    return IdentityCredential::ValidationPromise::CreateAndReject(
        NS_ERROR_SERVICE_NOT_AVAILABLE, __func__);
  }
  nsCString manifestURIString;
  rv = etld->GetSite(configURI, manifestURIString);
  if (NS_FAILED(rv)) {
    return IdentityCredential::ValidationPromise::CreateAndReject(
        NS_ERROR_INVALID_ARG, __func__);
  }
  manifestURIString.AppendLiteral("/.well-known/web-identity");

  // Create the global
  RefPtr<NullPrincipal> nullPrincipal =
      NullPrincipal::CreateWithInheritedAttributes(aPrincipal);
  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  MOZ_ASSERT(xpc, "This should never be null!");
  nsCOMPtr<nsIGlobalObject> global;
  AutoJSAPI jsapi;
  jsapi.Init();
  JSContext* cx = jsapi.cx();
  JS::Rooted<JSObject*> sandbox(cx);
  rv = xpc->CreateSandbox(cx, nullPrincipal, sandbox.address());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IdentityCredential::ValidationPromise::CreateAndReject(rv, __func__);
  }
  MOZ_ASSERT(JS_IsGlobalObject(sandbox));
  global = xpc::NativeGlobal(sandbox);
  if (NS_WARN_IF(!global)) {
    return IdentityCredential::ValidationPromise::CreateAndReject(
        NS_ERROR_FAILURE, __func__);
  }

  // Create a new request
  nsCString fragment = ""_ns;
  auto internalRequest =
      MakeSafeRefPtr<InternalRequest>(manifestURIString, fragment);
  internalRequest->SetCredentialsMode(RequestCredentials::Omit);
  internalRequest->SetReferrerPolicy(ReferrerPolicy::No_referrer);
  internalRequest->SetMode(RequestMode::Cors);
  internalRequest->SetCacheMode(RequestCache::No_cache);
  internalRequest->SetHeaders(new InternalHeaders(HeadersGuardEnum::Request));
  RefPtr<Request> request =
      new Request(global, std::move(internalRequest), nullptr);

  return FetchJSONStructure<IdentityRootManifest>(request)->Then(
      GetCurrentSerialEventTarget(), __func__,
      [aProvider](const IdentityRootManifest& manifest) {
        // Make sure there is only one provider URL
        if (manifest.mProvider_urls.Length() != 1) {
          return IdentityCredential::ValidationPromise::CreateAndResolve(
              false, __func__);
        }

        // Resolve whether or not that provider URL is the one we were
        // passed as an argument.
        bool correctURL = manifest.mProvider_urls[0] == aProvider.mConfigURL;
        return IdentityCredential::ValidationPromise::CreateAndResolve(
            correctURL, __func__);
      },
      [](nsresult error) {
        return IdentityCredential::ValidationPromise::CreateAndReject(error,
                                                                      __func__);
      });
}

// static
RefPtr<IdentityCredential::GetManifestPromise>
IdentityCredential::FetchInternalManifest(nsIPrincipal* aPrincipal,
                                          const IdentityProvider& aProvider) {
  MOZ_ASSERT(XRE_IsParentProcess());
  // Build the URL
  nsCString configLocation = NS_ConvertUTF16toUTF8(aProvider.mConfigURL);

  // Create the global
  RefPtr<NullPrincipal> nullPrincipal =
      NullPrincipal::CreateWithInheritedAttributes(aPrincipal);
  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  MOZ_ASSERT(xpc, "This should never be null!");
  nsCOMPtr<nsIGlobalObject> global;
  AutoJSAPI jsapi;
  jsapi.Init();
  JSContext* cx = jsapi.cx();
  JS::Rooted<JSObject*> sandbox(cx);
  nsresult rv = xpc->CreateSandbox(cx, nullPrincipal, sandbox.address());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IdentityCredential::GetManifestPromise::CreateAndReject(rv,
                                                                   __func__);
  }
  MOZ_ASSERT(JS_IsGlobalObject(sandbox));
  global = xpc::NativeGlobal(sandbox);
  if (NS_WARN_IF(!global)) {
    return IdentityCredential::GetManifestPromise::CreateAndReject(
        NS_ERROR_FAILURE, __func__);
  }

  // Create a new request
  nsCString fragment = ""_ns;
  auto internalRequest =
      MakeSafeRefPtr<InternalRequest>(configLocation, fragment);
  internalRequest->SetRedirectMode(RequestRedirect::Error);
  internalRequest->SetCredentialsMode(RequestCredentials::Omit);
  internalRequest->SetReferrerPolicy(ReferrerPolicy::No_referrer);
  internalRequest->SetMode(RequestMode::Cors);
  internalRequest->SetCacheMode(RequestCache::No_cache);
  internalRequest->SetHeaders(new InternalHeaders(HeadersGuardEnum::Request));
  RefPtr<Request> request =
      new Request(global, std::move(internalRequest), nullptr);
  return FetchJSONStructure<IdentityInternalManifest>(request);
}

// static
RefPtr<IdentityCredential::GetAccountListPromise>
IdentityCredential::FetchAccountList(
    nsIPrincipal* aPrincipal, const IdentityProvider& aProvider,
    const IdentityInternalManifest& aManifest) {
  MOZ_ASSERT(XRE_IsParentProcess());
  // Build the URL
  nsCString configLocation =
      NS_ConvertUTF16toUTF8(aManifest.mAccounts_endpoint);

  // Build the principal to use for this connection
  // This is an expanded principal! It has the cookies of the IDP because it
  // subsumes the constituent principals. It also has no serializable origin,
  // so it won't send an Origin header even though this is a CORS mode request.
  // It accomplishes this without being a SystemPrincipal too.
  nsCOMPtr<nsIURI> idpURI;
  nsresult rv = NS_NewURI(getter_AddRefs(idpURI), configLocation);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IdentityCredential::GetAccountListPromise::CreateAndReject(rv,
                                                                      __func__);
  }
  nsCOMPtr<nsIPrincipal> idpPrincipal = BasePrincipal::CreateContentPrincipal(
      idpURI, aPrincipal->OriginAttributesRef());
  nsCOMPtr<nsIPrincipal> nullPrincipal =
      NullPrincipal::CreateWithInheritedAttributes(aPrincipal);
  AutoTArray<nsCOMPtr<nsIPrincipal>, 2> allowList = {idpPrincipal,
                                                     nullPrincipal};
  RefPtr<ExpandedPrincipal> expandedPrincipal =
      ExpandedPrincipal::Create(allowList, aPrincipal->OriginAttributesRef());

  // Create the global
  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  MOZ_ASSERT(xpc, "This should never be null!");
  nsCOMPtr<nsIGlobalObject> global;
  AutoJSAPI jsapi;
  jsapi.Init();
  JSContext* cx = jsapi.cx();
  JS::Rooted<JSObject*> sandbox(cx);
  rv = xpc->CreateSandbox(cx, expandedPrincipal, sandbox.address());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IdentityCredential::GetAccountListPromise::CreateAndReject(rv,
                                                                      __func__);
  }
  MOZ_ASSERT(JS_IsGlobalObject(sandbox));
  global = xpc::NativeGlobal(sandbox);
  if (NS_WARN_IF(!global)) {
    return IdentityCredential::GetAccountListPromise::CreateAndReject(
        NS_ERROR_FAILURE, __func__);
  }

  // Create a new request
  nsCString fragment = ""_ns;
  auto internalRequest =
      MakeSafeRefPtr<InternalRequest>(configLocation, fragment);
  internalRequest->SetRedirectMode(RequestRedirect::Error);
  internalRequest->SetCredentialsMode(RequestCredentials::Include);
  internalRequest->SetReferrerPolicy(ReferrerPolicy::No_referrer);
  internalRequest->SetMode(RequestMode::Cors);
  internalRequest->SetCacheMode(RequestCache::No_cache);
  internalRequest->SetHeaders(new InternalHeaders(HeadersGuardEnum::Request));
  RefPtr<Request> request =
      new Request(global, std::move(internalRequest), nullptr);

  return FetchJSONStructure<IdentityAccountList>(request)->Then(
      GetCurrentSerialEventTarget(), __func__,
      [aManifest](const IdentityAccountList& accountList) {
        return IdentityCredential::GetAccountListPromise::CreateAndResolve(
            MakeTuple(aManifest, accountList), __func__);
      },
      [](nsresult error) {
        return IdentityCredential::GetAccountListPromise::CreateAndReject(
            error, __func__);
      });
}

// static
RefPtr<IdentityCredential::GetTokenPromise> IdentityCredential::FetchToken(
    nsIPrincipal* aPrincipal, const IdentityProvider& aProvider,
    const IdentityInternalManifest& aManifest,
    const IdentityAccount& aAccount) {
  MOZ_ASSERT(XRE_IsParentProcess());
  // Build the URL
  nsCString tokenLocation = NS_ConvertUTF16toUTF8(aManifest.mId_token_endpoint);

  // Create the global
  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  MOZ_ASSERT(xpc, "This should never be null!");
  nsCOMPtr<nsIGlobalObject> global;
  AutoJSAPI jsapi;
  jsapi.Init();
  JSContext* cx = jsapi.cx();
  JS::Rooted<JSObject*> sandbox(cx);
  nsresult rv = xpc->CreateSandbox(cx, aPrincipal, sandbox.address());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IdentityCredential::GetTokenPromise::CreateAndReject(rv, __func__);
  }
  MOZ_ASSERT(JS_IsGlobalObject(sandbox));
  global = xpc::NativeGlobal(sandbox);
  if (NS_WARN_IF(!global)) {
    return IdentityCredential::GetTokenPromise::CreateAndReject(
        NS_ERROR_FAILURE, __func__);
  }

  // Create a new request
  nsCString fragment = ""_ns;
  auto internalRequest =
      MakeSafeRefPtr<InternalRequest>(tokenLocation, fragment);
  internalRequest->SetMethod("POST"_ns);
  URLParams bodyValue;
  bodyValue.Set(u"account_id"_ns, aAccount.mId);
  bodyValue.Set(u"client_id"_ns, aProvider.mClientId);
  if (aProvider.mNonce.WasPassed()) {
    bodyValue.Set(u"nonce"_ns, aProvider.mNonce.Value());
  }
  bodyValue.Set(u"disclosure_text_shown"_ns, u"false"_ns);
  nsString bodyString;
  bodyValue.Serialize(bodyString, true);
  nsCString bodyCString = NS_ConvertUTF16toUTF8(bodyString);
  nsCOMPtr<nsIInputStream> streamBody;
  rv = NS_NewCStringInputStream(getter_AddRefs(streamBody), bodyCString);
  if (NS_FAILED(rv)) {
    return IdentityCredential::GetTokenPromise::CreateAndReject(
        NS_ERROR_FAILURE, __func__);
  }

  IgnoredErrorResult error;
  RefPtr<InternalHeaders> internalHeaders =
      new InternalHeaders(HeadersGuardEnum::Request);
  internalHeaders->Set("Content-Type"_ns,
                       "application/x-www-form-urlencoded"_ns, error);
  if (NS_WARN_IF(error.Failed())) {
    return IdentityCredential::GetTokenPromise::CreateAndReject(
        NS_ERROR_FAILURE, __func__);
  }
  internalRequest->SetHeaders(internalHeaders);
  internalRequest->SetBody(streamBody, bodyCString.Length());
  internalRequest->SetRedirectMode(RequestRedirect::Error);
  internalRequest->SetCredentialsMode(RequestCredentials::Include);
  internalRequest->SetReferrerPolicy(ReferrerPolicy::Strict_origin);
  internalRequest->SetMode(RequestMode::Cors);
  internalRequest->SetCacheMode(RequestCache::No_cache);
  RefPtr<Request> request =
      new Request(global, std::move(internalRequest), nullptr);
  return FetchJSONStructure<IdentityToken>(request)->Then(
      GetCurrentSerialEventTarget(), __func__,
      [aAccount](const IdentityToken& token) {
        return IdentityCredential::GetTokenPromise::CreateAndResolve(
            MakeTuple(token, aAccount), __func__);
      },
      [](nsresult error) {
        return IdentityCredential::GetTokenPromise::CreateAndReject(error,
                                                                    __func__);
      });
}

}  // namespace mozilla::dom

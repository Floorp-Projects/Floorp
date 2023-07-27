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
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Promise-inl.h"
#include "mozilla/dom/Request.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/Components.h"
#include "mozilla/ExpandedPrincipal.h"
#include "mozilla/NullPrincipal.h"
#include "nsEffectiveTLDService.h"
#include "nsIGlobalObject.h"
#include "nsIIdentityCredentialPromptService.h"
#include "nsIIdentityCredentialStorageService.h"
#include "nsITimer.h"
#include "nsIXPConnect.h"
#include "nsNetUtil.h"
#include "nsStringStream.h"
#include "nsTArray.h"
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

  // Kick the request off to the main process and translate the result to the
  // expected type when we get a result.
  MOZ_ASSERT(aOptions.mIdentity.WasPassed());
  RefPtr<WindowGlobalChild> wgc = aParent->GetWindowGlobalChild();
  MOZ_ASSERT(wgc);
  RefPtr<IdentityCredential> credential = new IdentityCredential(aParent);
  return wgc
      ->SendDiscoverIdentityCredentialFromExternalSource(
          aOptions.mIdentity.Value())
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [credential](const WindowGlobalChild::
                           DiscoverIdentityCredentialFromExternalSourcePromise::
                               ResolveValueType& aResult) {
            if (aResult.isSome()) {
              credential->CopyValuesFrom(aResult.value());
              return IdentityCredential::GetIdentityCredentialPromise::
                  CreateAndResolve(credential, __func__);
            }
            return IdentityCredential::GetIdentityCredentialPromise::
                CreateAndReject(NS_ERROR_DOM_UNKNOWN_ERR, __func__);
          },
          [](const WindowGlobalChild::
                 DiscoverIdentityCredentialFromExternalSourcePromise::
                     RejectValueType& aResult) {
            return IdentityCredential::GetIdentityCredentialPromise::
                CreateAndReject(NS_ERROR_DOM_UNKNOWN_ERR, __func__);
          });
}

// static
RefPtr<IdentityCredential::GetIPCIdentityCredentialPromise>
IdentityCredential::DiscoverFromExternalSourceInMainProcess(
    nsIPrincipal* aPrincipal, CanonicalBrowsingContext* aBrowsingContext,
    const IdentityCredentialRequestOptions& aOptions) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aBrowsingContext);

  // Make sure we have providers.
  if (!aOptions.mProviders.WasPassed() ||
      aOptions.mProviders.Value().Length() < 1) {
    return IdentityCredential::GetIPCIdentityCredentialPromise::CreateAndReject(
        NS_ERROR_DOM_NOT_ALLOWED_ERR, __func__);
  }

  RefPtr<IdentityCredential::GetIPCIdentityCredentialPromise::Private> result =
      new IdentityCredential::GetIPCIdentityCredentialPromise::Private(
          __func__);

  nsCOMPtr<nsIPrincipal> principal(aPrincipal);
  RefPtr<CanonicalBrowsingContext> browsingContext(aBrowsingContext);

  RefPtr<nsITimer> timeout;
  if (StaticPrefs::
          dom_security_credentialmanagement_identity_reject_delay_enabled()) {
    nsresult rv = NS_NewTimerWithCallback(
        getter_AddRefs(timeout),
        [=](auto) {
          if (!result->IsResolved()) {
            result->Reject(NS_ERROR_DOM_NETWORK_ERR, __func__);
          }
          IdentityCredential::CloseUserInterface(browsingContext);
        },
        StaticPrefs::
            dom_security_credentialmanagement_identity_reject_delay_duration_ms(),
        nsITimer::TYPE_ONE_SHOT, "IdentityCredentialTimeoutCallback");
    if (NS_WARN_IF(NS_FAILED(rv))) {
      result->Reject(NS_ERROR_FAILURE, __func__);
      return result.forget();
    }
  }

  // Construct an array of requests to fetch manifests for every provider.
  // We need this to show their branding information
  nsTArray<RefPtr<GetManifestPromise>> manifestPromises;
  for (const IdentityProviderConfig& provider : aOptions.mProviders.Value()) {
    RefPtr<GetManifestPromise> manifest =
        IdentityCredential::CheckRootManifest(aPrincipal, provider)
            ->Then(
                GetCurrentSerialEventTarget(), __func__,
                [provider, principal](bool valid) {
                  if (valid) {
                    return IdentityCredential::FetchInternalManifest(principal,
                                                                     provider);
                  }
                  return IdentityCredential::GetManifestPromise::
                      CreateAndReject(NS_ERROR_FAILURE, __func__);
                },
                [](nsresult error) {
                  return IdentityCredential::GetManifestPromise::
                      CreateAndReject(error, __func__);
                });
    manifestPromises.AppendElement(manifest);
  }

  // We use AllSettled here so that failures will be included- we use default
  // values there.
  GetManifestPromise::AllSettled(GetCurrentSerialEventTarget(),
                                 manifestPromises)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [browsingContext, aOptions](
              const GetManifestPromise::AllSettledPromiseType::ResolveValueType&
                  aResults) {
            // Convert the
            // GetManifestPromise::AllSettledPromiseType::ResolveValueType to a
            // Sequence<MozPromise>
            CopyableTArray<MozPromise<IdentityProviderAPIConfig, nsresult,
                                      true>::ResolveOrRejectValue>
                results = aResults;
            const Sequence<MozPromise<IdentityProviderAPIConfig, nsresult,
                                      true>::ResolveOrRejectValue>
                resultsSequence(std::move(results));
            // The user picks from the providers
            return PromptUserToSelectProvider(
                browsingContext, aOptions.mProviders.Value(), resultsSequence);
          },
          [](bool error) {
            return IdentityCredential::
                GetIdentityProviderConfigWithManifestPromise::CreateAndReject(
                    NS_ERROR_FAILURE, __func__);
          })
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [principal, browsingContext](
              const IdentityProviderConfigWithManifest& providerAndManifest) {
            IdentityProviderAPIConfig manifest;
            IdentityProviderConfig provider;
            std::tie(provider, manifest) = providerAndManifest;
            return IdentityCredential::CreateCredential(
                principal, browsingContext, provider, manifest);
          },
          [](nsresult error) {
            return IdentityCredential::GetIPCIdentityCredentialPromise::
                CreateAndReject(error, __func__);
          })
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [result, timeout = std::move(timeout)](
              const IdentityCredential::GetIPCIdentityCredentialPromise::
                  ResolveOrRejectValue&& value) {
            // Resolve the result
            result->ResolveOrReject(value, __func__);

            // Cancel the timer (if it is still pending) and
            // release the hold on the variables leaked into the timer.
            if (timeout &&
                StaticPrefs::
                    dom_security_credentialmanagement_identity_reject_delay_enabled()) {
              timeout->Cancel();
            }
          });

  return result;
}

// static
RefPtr<IdentityCredential::GetIPCIdentityCredentialPromise>
IdentityCredential::CreateCredential(
    nsIPrincipal* aPrincipal, BrowsingContext* aBrowsingContext,
    const IdentityProviderConfig& aProvider,
    const IdentityProviderAPIConfig& aManifest) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aBrowsingContext);

  nsCOMPtr<nsIPrincipal> argumentPrincipal = aPrincipal;
  RefPtr<BrowsingContext> browsingContext(aBrowsingContext);

  return IdentityCredential::FetchAccountList(argumentPrincipal, aProvider,
                                              aManifest)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [argumentPrincipal, browsingContext, aProvider](
              const std::tuple<IdentityProviderAPIConfig,
                               IdentityProviderAccountList>& promiseResult) {
            IdentityProviderAPIConfig currentManifest;
            IdentityProviderAccountList accountList;
            std::tie(currentManifest, accountList) = promiseResult;
            if (!accountList.mAccounts.WasPassed() ||
                accountList.mAccounts.Value().Length() == 0) {
              return IdentityCredential::GetAccountPromise::CreateAndReject(
                  NS_ERROR_FAILURE, __func__);
            }
            return PromptUserToSelectAccount(browsingContext, accountList,
                                             aProvider, currentManifest);
          },
          [](nsresult error) {
            return IdentityCredential::GetAccountPromise::CreateAndReject(
                error, __func__);
          })
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [argumentPrincipal, browsingContext, aProvider](
              const std::tuple<IdentityProviderAPIConfig,
                               IdentityProviderAccount>& promiseResult) {
            IdentityProviderAPIConfig currentManifest;
            IdentityProviderAccount account;
            std::tie(currentManifest, account) = promiseResult;
            return IdentityCredential::PromptUserWithPolicy(
                browsingContext, argumentPrincipal, account, currentManifest,
                aProvider);
          },
          [](nsresult error) {
            return IdentityCredential::GetAccountPromise::CreateAndReject(
                error, __func__);
          })
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [argumentPrincipal, aProvider](
              const std::tuple<IdentityProviderAPIConfig,
                               IdentityProviderAccount>& promiseResult) {
            IdentityProviderAPIConfig currentManifest;
            IdentityProviderAccount account;
            std::tie(currentManifest, account) = promiseResult;
            return IdentityCredential::FetchToken(argumentPrincipal, aProvider,
                                                  currentManifest, account);
          },
          [](nsresult error) {
            return IdentityCredential::GetTokenPromise::CreateAndReject(
                error, __func__);
          })
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [aProvider](
              const std::tuple<IdentityProviderToken, IdentityProviderAccount>&
                  promiseResult) {
            IdentityProviderToken token;
            IdentityProviderAccount account;
            std::tie(token, account) = promiseResult;
            IPCIdentityCredential credential;
            credential.token() = token.mToken;
            credential.id() = account.mId;
            credential.type() = u"identity"_ns;
            return IdentityCredential::GetIPCIdentityCredentialPromise::
                CreateAndResolve(credential, __func__);
          },
          [browsingContext](nsresult error) {
            CloseUserInterface(browsingContext);
            return IdentityCredential::GetIPCIdentityCredentialPromise::
                CreateAndReject(error, __func__);
          });
}

// static
RefPtr<IdentityCredential::ValidationPromise>
IdentityCredential::CheckRootManifest(nsIPrincipal* aPrincipal,
                                      const IdentityProviderConfig& aProvider) {
  MOZ_ASSERT(XRE_IsParentProcess());

  if (StaticPrefs::
          dom_security_credentialmanagement_identity_test_ignore_well_known()) {
    return IdentityCredential::ValidationPromise::CreateAndResolve(true,
                                                                   __func__);
  }

  // Build the URL
  nsCString configLocation = aProvider.mConfigURL;
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
  constexpr auto fragment = ""_ns;
  auto internalRequest =
      MakeSafeRefPtr<InternalRequest>(manifestURIString, fragment);
  internalRequest->SetCredentialsMode(RequestCredentials::Omit);
  internalRequest->SetReferrerPolicy(ReferrerPolicy::No_referrer);
  internalRequest->SetMode(RequestMode::Cors);
  internalRequest->SetCacheMode(RequestCache::No_cache);
  internalRequest->SetHeaders(new InternalHeaders(HeadersGuardEnum::Request));
  internalRequest->OverrideContentPolicyType(
      nsContentPolicyType::TYPE_WEB_IDENTITY);
  RefPtr<Request> request =
      new Request(global, std::move(internalRequest), nullptr);

  return FetchJSONStructure<IdentityProviderWellKnown>(request)->Then(
      GetCurrentSerialEventTarget(), __func__,
      [aProvider](const IdentityProviderWellKnown& manifest) {
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
IdentityCredential::FetchInternalManifest(
    nsIPrincipal* aPrincipal, const IdentityProviderConfig& aProvider) {
  MOZ_ASSERT(XRE_IsParentProcess());
  // Build the URL
  nsCString configLocation = aProvider.mConfigURL;

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
  constexpr auto fragment = ""_ns;
  auto internalRequest =
      MakeSafeRefPtr<InternalRequest>(configLocation, fragment);
  internalRequest->SetRedirectMode(RequestRedirect::Error);
  internalRequest->SetCredentialsMode(RequestCredentials::Omit);
  internalRequest->SetReferrerPolicy(ReferrerPolicy::No_referrer);
  internalRequest->SetMode(RequestMode::Cors);
  internalRequest->SetCacheMode(RequestCache::No_cache);
  internalRequest->SetHeaders(new InternalHeaders(HeadersGuardEnum::Request));
  internalRequest->OverrideContentPolicyType(
      nsContentPolicyType::TYPE_WEB_IDENTITY);
  RefPtr<Request> request =
      new Request(global, std::move(internalRequest), nullptr);
  return FetchJSONStructure<IdentityProviderAPIConfig>(request);
}

// static
RefPtr<IdentityCredential::GetAccountListPromise>
IdentityCredential::FetchAccountList(
    nsIPrincipal* aPrincipal, const IdentityProviderConfig& aProvider,
    const IdentityProviderAPIConfig& aManifest) {
  MOZ_ASSERT(XRE_IsParentProcess());
  // Build the URL
  nsCOMPtr<nsIURI> baseURI;
  nsresult rv = NS_NewURI(getter_AddRefs(baseURI), aProvider.mConfigURL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IdentityCredential::GetAccountListPromise::CreateAndReject(rv,
                                                                      __func__);
  }
  nsCOMPtr<nsIURI> idpURI;
  rv = NS_NewURI(getter_AddRefs(idpURI), aManifest.mAccounts_endpoint, nullptr,
                 baseURI);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IdentityCredential::GetAccountListPromise::CreateAndReject(rv,
                                                                      __func__);
  }
  nsCString configLocation;
  rv = idpURI->GetSpec(configLocation);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IdentityCredential::GetAccountListPromise::CreateAndReject(rv,
                                                                      __func__);
  }

  // Build the principal to use for this connection
  // This is an expanded principal! It has the cookies of the IDP because it
  // subsumes the constituent principals. It also has no serializable origin,
  // so it won't send an Origin header even though this is a CORS mode
  // request. It accomplishes this without being a SystemPrincipal too.
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
  constexpr auto fragment = ""_ns;
  auto internalRequest =
      MakeSafeRefPtr<InternalRequest>(configLocation, fragment);
  internalRequest->SetRedirectMode(RequestRedirect::Error);
  internalRequest->SetCredentialsMode(RequestCredentials::Include);
  internalRequest->SetReferrerPolicy(ReferrerPolicy::No_referrer);
  internalRequest->SetMode(RequestMode::Cors);
  internalRequest->SetCacheMode(RequestCache::No_cache);
  internalRequest->SetHeaders(new InternalHeaders(HeadersGuardEnum::Request));
  internalRequest->OverrideContentPolicyType(
      nsContentPolicyType::TYPE_WEB_IDENTITY);
  RefPtr<Request> request =
      new Request(global, std::move(internalRequest), nullptr);

  return FetchJSONStructure<IdentityProviderAccountList>(request)->Then(
      GetCurrentSerialEventTarget(), __func__,
      [aManifest](const IdentityProviderAccountList& accountList) {
        return IdentityCredential::GetAccountListPromise::CreateAndResolve(
            std::make_tuple(aManifest, accountList), __func__);
      },
      [](nsresult error) {
        return IdentityCredential::GetAccountListPromise::CreateAndReject(
            error, __func__);
      });
}

// static
RefPtr<IdentityCredential::GetTokenPromise> IdentityCredential::FetchToken(
    nsIPrincipal* aPrincipal, const IdentityProviderConfig& aProvider,
    const IdentityProviderAPIConfig& aManifest,
    const IdentityProviderAccount& aAccount) {
  MOZ_ASSERT(XRE_IsParentProcess());
  // Build the URL
  nsCOMPtr<nsIURI> baseURI;
  nsCString baseURIString = aProvider.mConfigURL;
  nsresult rv = NS_NewURI(getter_AddRefs(baseURI), baseURIString);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IdentityCredential::GetTokenPromise::CreateAndReject(rv, __func__);
  }
  nsCOMPtr<nsIURI> idpURI;
  nsCString tokenSpec = aManifest.mId_assertion_endpoint;
  rv = NS_NewURI(getter_AddRefs(idpURI), tokenSpec.get(), baseURI);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IdentityCredential::GetTokenPromise::CreateAndReject(rv, __func__);
  }
  nsCString tokenLocation;
  rv = idpURI->GetSpec(tokenLocation);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IdentityCredential::GetTokenPromise::CreateAndReject(rv, __func__);
  }

  // Create the global
  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  MOZ_ASSERT(xpc, "This should never be null!");
  nsCOMPtr<nsIGlobalObject> global;
  AutoJSAPI jsapi;
  jsapi.Init();
  JSContext* cx = jsapi.cx();
  JS::Rooted<JSObject*> sandbox(cx);
  rv = xpc->CreateSandbox(cx, aPrincipal, sandbox.address());
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
  constexpr auto fragment = ""_ns;
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
  internalRequest->OverrideContentPolicyType(
      nsContentPolicyType::TYPE_WEB_IDENTITY);
  RefPtr<Request> request =
      new Request(global, std::move(internalRequest), nullptr);
  return FetchJSONStructure<IdentityProviderToken>(request)->Then(
      GetCurrentSerialEventTarget(), __func__,
      [aAccount](const IdentityProviderToken& token) {
        return IdentityCredential::GetTokenPromise::CreateAndResolve(
            std::make_tuple(token, aAccount), __func__);
      },
      [](nsresult error) {
        return IdentityCredential::GetTokenPromise::CreateAndReject(error,
                                                                    __func__);
      });
}

// static
RefPtr<IdentityCredential::GetMetadataPromise>
IdentityCredential::FetchMetadata(nsIPrincipal* aPrincipal,
                                  const IdentityProviderConfig& aProvider,
                                  const IdentityProviderAPIConfig& aManifest) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(aPrincipal);
  // Build the URL
  nsCOMPtr<nsIURI> baseURI;
  nsCString baseURIString = aProvider.mConfigURL;
  nsresult rv = NS_NewURI(getter_AddRefs(baseURI), baseURIString);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IdentityCredential::GetMetadataPromise::CreateAndReject(rv,
                                                                   __func__);
  }
  nsCOMPtr<nsIURI> idpURI;
  nsCString metadataSpec = aManifest.mClient_metadata_endpoint;
  rv = NS_NewURI(getter_AddRefs(idpURI), metadataSpec.get(), baseURI);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IdentityCredential::GetMetadataPromise::CreateAndReject(rv,
                                                                   __func__);
  }
  nsCString configLocation;
  rv = idpURI->GetSpec(configLocation);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IdentityCredential::GetMetadataPromise::CreateAndReject(rv,
                                                                   __func__);
  }

  // Create the global
  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  MOZ_ASSERT(xpc, "This should never be null!");
  nsCOMPtr<nsIGlobalObject> global;
  AutoJSAPI jsapi;
  jsapi.Init();
  JSContext* cx = jsapi.cx();
  JS::Rooted<JSObject*> sandbox(cx);
  rv = xpc->CreateSandbox(cx, aPrincipal, sandbox.address());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IdentityCredential::GetMetadataPromise::CreateAndReject(rv,
                                                                   __func__);
  }
  MOZ_ASSERT(JS_IsGlobalObject(sandbox));
  global = xpc::NativeGlobal(sandbox);
  if (NS_WARN_IF(!global)) {
    return IdentityCredential::GetMetadataPromise::CreateAndReject(
        NS_ERROR_FAILURE, __func__);
  }

  // Create a new request
  constexpr auto fragment = ""_ns;
  auto internalRequest =
      MakeSafeRefPtr<InternalRequest>(configLocation, fragment);
  internalRequest->SetRedirectMode(RequestRedirect::Error);
  internalRequest->SetCredentialsMode(RequestCredentials::Omit);
  internalRequest->SetReferrerPolicy(ReferrerPolicy::No_referrer);
  internalRequest->SetMode(RequestMode::Cors);
  internalRequest->SetCacheMode(RequestCache::No_cache);
  internalRequest->SetHeaders(new InternalHeaders(HeadersGuardEnum::Request));
  internalRequest->OverrideContentPolicyType(
      nsContentPolicyType::TYPE_WEB_IDENTITY);
  RefPtr<Request> request =
      new Request(global, std::move(internalRequest), nullptr);
  return FetchJSONStructure<IdentityProviderClientMetadata>(request);
}

// static
RefPtr<IdentityCredential::GetIdentityProviderConfigWithManifestPromise>
IdentityCredential::PromptUserToSelectProvider(
    BrowsingContext* aBrowsingContext,
    const Sequence<IdentityProviderConfig>& aProviders,
    const Sequence<GetManifestPromise::ResolveOrRejectValue>& aManifests) {
  MOZ_ASSERT(aBrowsingContext);
  RefPtr<
      IdentityCredential::GetIdentityProviderConfigWithManifestPromise::Private>
      resultPromise = new IdentityCredential::
          GetIdentityProviderConfigWithManifestPromise::Private(__func__);

  if (NS_WARN_IF(!aBrowsingContext)) {
    resultPromise->Reject(NS_ERROR_FAILURE, __func__);
    return resultPromise;
  }

  nsresult error;
  nsCOMPtr<nsIIdentityCredentialPromptService> icPromptService =
      mozilla::components::IdentityCredentialPromptService::Service(&error);
  if (NS_WARN_IF(!icPromptService)) {
    resultPromise->Reject(error, __func__);
    return resultPromise;
  }

  nsCOMPtr<nsIXPConnectWrappedJS> wrapped = do_QueryInterface(icPromptService);
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(wrapped->GetJSObjectGlobal()))) {
    resultPromise->Reject(NS_ERROR_FAILURE, __func__);
    return resultPromise;
  }

  JS::Rooted<JS::Value> providersJS(jsapi.cx());
  bool success = ToJSValue(jsapi.cx(), aProviders, &providersJS);
  if (NS_WARN_IF(!success)) {
    resultPromise->Reject(NS_ERROR_FAILURE, __func__);
    return resultPromise;
  }

  // Convert each settled MozPromise into a Nullable<ResolveValue>
  Sequence<Nullable<IdentityProviderAPIConfig>> manifests;
  for (GetManifestPromise::ResolveOrRejectValue manifest : aManifests) {
    if (manifest.IsResolve()) {
      if (NS_WARN_IF(
              !manifests.AppendElement(manifest.ResolveValue(), fallible))) {
        resultPromise->Reject(NS_ERROR_FAILURE, __func__);
        return resultPromise;
      }
    } else {
      if (NS_WARN_IF(!manifests.AppendElement(
              Nullable<IdentityProviderAPIConfig>(), fallible))) {
        resultPromise->Reject(NS_ERROR_FAILURE, __func__);
        return resultPromise;
      }
    }
  }
  JS::Rooted<JS::Value> manifestsJS(jsapi.cx());
  success = ToJSValue(jsapi.cx(), manifests, &manifestsJS);
  if (NS_WARN_IF(!success)) {
    resultPromise->Reject(NS_ERROR_FAILURE, __func__);
    return resultPromise;
  }

  RefPtr<Promise> showPromptPromise;
  icPromptService->ShowProviderPrompt(aBrowsingContext, providersJS,
                                      manifestsJS,
                                      getter_AddRefs(showPromptPromise));

  showPromptPromise->AddCallbacksWithCycleCollectedArgs(
      [aProviders, aManifests, resultPromise](
          JSContext*, JS::Handle<JS::Value> aValue, ErrorResult&) {
        int32_t result = aValue.toInt32();
        if (result < 0 || (uint32_t)result > aProviders.Length() ||
            (uint32_t)result > aManifests.Length()) {
          resultPromise->Reject(NS_ERROR_FAILURE, __func__);
          return;
        }
        const IdentityProviderConfig& resolvedProvider =
            aProviders.ElementAt(result);
        if (!aManifests.ElementAt(result).IsResolve()) {
          resultPromise->Reject(NS_ERROR_FAILURE, __func__);
          return;
        }
        const IdentityProviderAPIConfig& resolvedManifest =
            aManifests.ElementAt(result).ResolveValue();
        resultPromise->Resolve(
            std::make_tuple(resolvedProvider, resolvedManifest), __func__);
      },
      [resultPromise](JSContext*, JS::Handle<JS::Value> aValue, ErrorResult&) {
        resultPromise->Reject(
            Promise::TryExtractNSResultFromRejectionValue(aValue), __func__);
      });
  // Working around https://gcc.gnu.org/bugzilla/show_bug.cgi?id=85883
  showPromptPromise->AppendNativeHandler(
      new MozPromiseRejectOnDestruction{resultPromise, __func__});

  return resultPromise;
}

// static
RefPtr<IdentityCredential::GetAccountPromise>
IdentityCredential::PromptUserToSelectAccount(
    BrowsingContext* aBrowsingContext,
    const IdentityProviderAccountList& aAccounts,
    const IdentityProviderConfig& aProvider,
    const IdentityProviderAPIConfig& aManifest) {
  MOZ_ASSERT(aBrowsingContext);
  RefPtr<IdentityCredential::GetAccountPromise::Private> resultPromise =
      new IdentityCredential::GetAccountPromise::Private(__func__);

  if (NS_WARN_IF(!aBrowsingContext)) {
    resultPromise->Reject(NS_ERROR_FAILURE, __func__);
    return resultPromise;
  }

  nsresult error;
  nsCOMPtr<nsIIdentityCredentialPromptService> icPromptService =
      mozilla::components::IdentityCredentialPromptService::Service(&error);
  if (NS_WARN_IF(!icPromptService)) {
    resultPromise->Reject(error, __func__);
    return resultPromise;
  }

  nsCOMPtr<nsIXPConnectWrappedJS> wrapped = do_QueryInterface(icPromptService);
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(wrapped->GetJSObjectGlobal()))) {
    resultPromise->Reject(NS_ERROR_FAILURE, __func__);
    return resultPromise;
  }

  JS::Rooted<JS::Value> accountsJS(jsapi.cx());
  bool success = ToJSValue(jsapi.cx(), aAccounts, &accountsJS);
  if (NS_WARN_IF(!success)) {
    resultPromise->Reject(NS_ERROR_FAILURE, __func__);
    return resultPromise;
  }

  JS::Rooted<JS::Value> providerJS(jsapi.cx());
  success = ToJSValue(jsapi.cx(), aProvider, &providerJS);
  if (NS_WARN_IF(!success)) {
    resultPromise->Reject(NS_ERROR_FAILURE, __func__);
    return resultPromise;
  }

  JS::Rooted<JS::Value> manifestJS(jsapi.cx());
  success = ToJSValue(jsapi.cx(), aManifest, &manifestJS);
  if (NS_WARN_IF(!success)) {
    resultPromise->Reject(NS_ERROR_FAILURE, __func__);
    return resultPromise;
  }

  RefPtr<Promise> showPromptPromise;
  icPromptService->ShowAccountListPrompt(aBrowsingContext, accountsJS,
                                         providerJS, manifestJS,
                                         getter_AddRefs(showPromptPromise));

  showPromptPromise->AddCallbacksWithCycleCollectedArgs(
      [aAccounts, resultPromise, aManifest](
          JSContext*, JS::Handle<JS::Value> aValue, ErrorResult&) {
        int32_t result = aValue.toInt32();
        if (!aAccounts.mAccounts.WasPassed() || result < 0 ||
            (uint32_t)result > aAccounts.mAccounts.Value().Length()) {
          resultPromise->Reject(NS_ERROR_FAILURE, __func__);
          return;
        }
        const IdentityProviderAccount& resolved =
            aAccounts.mAccounts.Value().ElementAt(result);
        resultPromise->Resolve(std::make_tuple(aManifest, resolved), __func__);
      },
      [resultPromise](JSContext*, JS::Handle<JS::Value> aValue, ErrorResult&) {
        resultPromise->Reject(
            Promise::TryExtractNSResultFromRejectionValue(aValue), __func__);
      });
  // Working around https://gcc.gnu.org/bugzilla/show_bug.cgi?id=85883
  showPromptPromise->AppendNativeHandler(
      new MozPromiseRejectOnDestruction{resultPromise, __func__});

  return resultPromise;
}

// static
RefPtr<IdentityCredential::GetAccountPromise>
IdentityCredential::PromptUserWithPolicy(
    BrowsingContext* aBrowsingContext, nsIPrincipal* aPrincipal,
    const IdentityProviderAccount& aAccount,
    const IdentityProviderAPIConfig& aManifest,
    const IdentityProviderConfig& aProvider) {
  MOZ_ASSERT(aBrowsingContext);
  MOZ_ASSERT(aPrincipal);

  nsresult error;
  nsCOMPtr<nsIIdentityCredentialStorageService> icStorageService =
      mozilla::components::IdentityCredentialStorageService::Service(&error);
  if (NS_WARN_IF(!icStorageService)) {
    return IdentityCredential::GetAccountPromise::CreateAndReject(error,
                                                                  __func__);
  }

  // Check the storage bit
  nsCString configLocation = aProvider.mConfigURL;
  nsCOMPtr<nsIURI> idpURI;
  error = NS_NewURI(getter_AddRefs(idpURI), configLocation);
  if (NS_WARN_IF(NS_FAILED(error))) {
    return IdentityCredential::GetAccountPromise::CreateAndReject(error,
                                                                  __func__);
  }
  bool registered = false;
  bool allowLogout = false;
  nsCOMPtr<nsIPrincipal> idpPrincipal = BasePrincipal::CreateContentPrincipal(
      idpURI, aPrincipal->OriginAttributesRef());
  error = icStorageService->GetState(aPrincipal, idpPrincipal,
                                     NS_ConvertUTF16toUTF8(aAccount.mId),
                                     &registered, &allowLogout);
  if (NS_WARN_IF(NS_FAILED(error))) {
    return IdentityCredential::GetAccountPromise::CreateAndReject(error,
                                                                  __func__);
  }

  // if registered, mark as logged in and return
  if (registered) {
    icStorageService->SetState(aPrincipal, idpPrincipal,
                               NS_ConvertUTF16toUTF8(aAccount.mId), true, true);
    return IdentityCredential::GetAccountPromise::CreateAndResolve(
        std::make_tuple(aManifest, aAccount), __func__);
  }

  // otherwise, fetch ->Then display ->Then return ->Catch reject
  RefPtr<BrowsingContext> browsingContext(aBrowsingContext);
  nsCOMPtr<nsIPrincipal> argumentPrincipal(aPrincipal);
  return FetchMetadata(aPrincipal, aProvider, aManifest)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [aAccount, aManifest, aProvider, argumentPrincipal, browsingContext,
           icStorageService,
           idpPrincipal](const IdentityProviderClientMetadata& metadata)
              -> RefPtr<GenericPromise> {
            nsresult error;
            nsCOMPtr<nsIIdentityCredentialPromptService> icPromptService =
                mozilla::components::IdentityCredentialPromptService::Service(
                    &error);
            if (NS_WARN_IF(!icPromptService)) {
              return GenericPromise::CreateAndReject(error, __func__);
            }
            nsCOMPtr<nsIXPConnectWrappedJS> wrapped =
                do_QueryInterface(icPromptService);
            AutoJSAPI jsapi;
            if (NS_WARN_IF(!jsapi.Init(wrapped->GetJSObjectGlobal()))) {
              return GenericPromise::CreateAndReject(NS_ERROR_FAILURE,
                                                     __func__);
            }

            JS::Rooted<JS::Value> providerJS(jsapi.cx());
            bool success = ToJSValue(jsapi.cx(), aProvider, &providerJS);
            if (NS_WARN_IF(!success)) {
              return GenericPromise::CreateAndReject(NS_ERROR_FAILURE,
                                                     __func__);
            }
            JS::Rooted<JS::Value> metadataJS(jsapi.cx());
            success = ToJSValue(jsapi.cx(), metadata, &metadataJS);
            if (NS_WARN_IF(!success)) {
              return GenericPromise::CreateAndReject(NS_ERROR_FAILURE,
                                                     __func__);
            }
            JS::Rooted<JS::Value> manifestJS(jsapi.cx());
            success = ToJSValue(jsapi.cx(), aManifest, &manifestJS);
            if (NS_WARN_IF(!success)) {
              return GenericPromise::CreateAndReject(NS_ERROR_FAILURE,
                                                     __func__);
            }

            RefPtr<Promise> showPromptPromise;
            icPromptService->ShowPolicyPrompt(
                browsingContext, providerJS, manifestJS, metadataJS,
                getter_AddRefs(showPromptPromise));

            RefPtr<GenericPromise::Private> resultPromise =
                new GenericPromise::Private(__func__);
            showPromptPromise->AddCallbacksWithCycleCollectedArgs(
                [aAccount, argumentPrincipal, idpPrincipal, resultPromise,
                 icStorageService](JSContext* aCx, JS::Handle<JS::Value> aValue,
                                   ErrorResult&) {
                  bool isBool = aValue.isBoolean();
                  if (!isBool) {
                    resultPromise->Reject(NS_ERROR_FAILURE, __func__);
                    return;
                  }
                  icStorageService->SetState(
                      argumentPrincipal, idpPrincipal,
                      NS_ConvertUTF16toUTF8(aAccount.mId), true, true);
                  resultPromise->Resolve(aValue.toBoolean(), __func__);
                },
                [resultPromise](JSContext*, JS::Handle<JS::Value> aValue,
                                ErrorResult&) {
                  resultPromise->Reject(
                      Promise::TryExtractNSResultFromRejectionValue(aValue),
                      __func__);
                });
            // Working around https://gcc.gnu.org/bugzilla/show_bug.cgi?id=85883
            showPromptPromise->AppendNativeHandler(
                new MozPromiseRejectOnDestruction{resultPromise, __func__});
            return resultPromise;
          },
          [](nsresult error) {
            return GenericPromise::CreateAndReject(error, __func__);
          })
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [aManifest, aAccount](bool success) {
            if (success) {
              return IdentityCredential::GetAccountPromise::CreateAndResolve(
                  std::make_tuple(aManifest, aAccount), __func__);
            }
            return IdentityCredential::GetAccountPromise::CreateAndReject(
                NS_ERROR_FAILURE, __func__);
          },
          [](nsresult error) {
            return IdentityCredential::GetAccountPromise::CreateAndReject(
                error, __func__);
          });
}

// static
void IdentityCredential::CloseUserInterface(BrowsingContext* aBrowsingContext) {
  nsresult error;
  nsCOMPtr<nsIIdentityCredentialPromptService> icPromptService =
      mozilla::components::IdentityCredentialPromptService::Service(&error);
  if (NS_WARN_IF(!icPromptService)) {
    return;
  }
  icPromptService->Close(aBrowsingContext);
}

// static
already_AddRefed<Promise> IdentityCredential::LogoutRPs(
    GlobalObject& aGlobal,
    const Sequence<IdentityCredentialLogoutRPsRequest>& aLogoutRequests,
    ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<Promise> promise = Promise::CreateResolvedWithUndefined(global, aRv);
  NS_ENSURE_FALSE(aRv.Failed(), nullptr);
  nsresult rv;
  nsCOMPtr<nsIIdentityCredentialStorageService> icStorageService =
      components::IdentityCredentialStorageService::Service(&rv);
  if (NS_WARN_IF(!icStorageService)) {
    aRv.Throw(rv);
    return nullptr;
  }

  RefPtr<nsIPrincipal> rpPrincipal = global->PrincipalOrNull();
  for (const auto& request : aLogoutRequests) {
    // Get the current state
    nsCOMPtr<nsIURI> idpURI;
    rv = NS_NewURI(getter_AddRefs(idpURI), request.mUrl);
    if (NS_FAILED(rv)) {
      aRv.ThrowTypeError<MSG_INVALID_URL>(request.mUrl);
      return nullptr;
    }
    nsCOMPtr<nsIPrincipal> idpPrincipal = BasePrincipal::CreateContentPrincipal(
        idpURI, rpPrincipal->OriginAttributesRef());
    bool registered, allowLogout;
    icStorageService->GetState(rpPrincipal, idpPrincipal, request.mAccountId,
                               &registered, &allowLogout);

    // Ignore this request if it isn't permitted
    if (!(registered && allowLogout)) {
      continue;
    }

    // Issue the logout request
    constexpr auto fragment = ""_ns;
    auto internalRequest =
        MakeSafeRefPtr<InternalRequest>(request.mUrl, fragment);
    internalRequest->SetRedirectMode(RequestRedirect::Error);
    internalRequest->SetCredentialsMode(RequestCredentials::Include);
    internalRequest->SetReferrerPolicy(ReferrerPolicy::Strict_origin);
    internalRequest->SetMode(RequestMode::Cors);
    internalRequest->SetCacheMode(RequestCache::No_cache);
    internalRequest->OverrideContentPolicyType(
        nsContentPolicyType::TYPE_WEB_IDENTITY);
    RefPtr<Request> domRequest =
        new Request(global, std::move(internalRequest), nullptr);
    RequestOrUSVString fetchInput;
    fetchInput.SetAsRequest() = domRequest;
    RootedDictionary<RequestInit> requestInit(RootingCx());
    IgnoredErrorResult error;
    RefPtr<Promise> fetchPromise = FetchRequest(global, fetchInput, requestInit,
                                                CallerType::System, error);

    // Change state to disallow more logout requests
    icStorageService->SetState(rpPrincipal, idpPrincipal, request.mAccountId,
                               true, false);
  }
  return promise.forget();
}

}  // namespace mozilla::dom

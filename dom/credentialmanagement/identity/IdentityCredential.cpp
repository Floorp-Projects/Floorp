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
    : Credential(aParent) {
  if (aParent && aParent->GetBrowsingContext() &&
      aParent->GetBrowsingContext()->Top() &&
      aParent->GetBrowsingContext()->Top()->GetDocument()) {
    this->mIdentityProvider =
        aParent->GetBrowsingContext()->Top()->GetDocument()->GetPrincipal();
  }
}

IdentityCredential::IdentityCredential(nsPIDOMWindowInner* aParent,
                                       const IPCIdentityCredential& aOther)
    : Credential(aParent) {
  CopyValuesFrom(aOther);
}

void IdentityCredential::CopyValuesFrom(const IPCIdentityCredential& aOther) {
  this->SetId(aOther.id());
  this->SetType(u"identity"_ns);
  IdentityCredentialInit creationOptions;
  if (aOther.token().isSome()) {
    this->mToken = aOther.token().value();
    creationOptions.mToken.Construct(
        NS_ConvertUTF16toUTF8(aOther.token().value()));
  }
  if (aOther.effectiveQueryURL().isSome()) {
    creationOptions.mEffectiveQueryURL.Construct(
        aOther.effectiveQueryURL().value());
  }
  if (aOther.effectiveOrigins().Length() > 0) {
    creationOptions.mEffectiveOrigins.Construct(
        Sequence(aOther.effectiveOrigins().Clone()));
  }
  creationOptions.mId = aOther.id();
  IdentityCredentialUserData userData;
  if (aOther.name().isSome()) {
    userData.mName = aOther.name()->Data();
  }
  if (aOther.iconURL().isSome()) {
    userData.mIconURL = aOther.iconURL()->Data();
  }
  if (aOther.infoExpiresAt().isSome()) {
    userData.mExpiresAfter.Construct(std::max<uint64_t>(
        aOther.infoExpiresAt().value() - PR_Now() / PR_USEC_PER_MSEC, 0));
  }
  if (aOther.name().isSome() || aOther.iconURL().isSome() ||
      aOther.infoExpiresAt().isSome()) {
    creationOptions.mUiHint.Construct(userData);
  }
  this->mCreationOptions = Some(creationOptions);
  this->mIdentityProvider = aOther.identityProvider();
}

IPCIdentityCredential IdentityCredential::MakeIPCIdentityCredential() const {
  IPCIdentityCredential result;
  result.identityProvider() = mIdentityProvider;
  this->GetId(result.id());
  if (this->mCreationOptions.isSome()) {
    if (this->mCreationOptions->mEffectiveQueryURL.WasPassed()) {
      result.effectiveQueryURL() =
          Some(this->mCreationOptions->mEffectiveQueryURL.Value());
    }
    if (this->mCreationOptions->mEffectiveOrigins.WasPassed()) {
      result.effectiveOrigins() =
          this->mCreationOptions->mEffectiveOrigins.Value();
    }
    if (this->mCreationOptions->mUiHint.WasPassed() &&
        !this->mCreationOptions->mUiHint.Value().mIconURL.IsEmpty()) {
      result.iconURL() = Some(this->mCreationOptions->mUiHint.Value().mIconURL);
    }
    if (this->mCreationOptions->mUiHint.WasPassed() &&
        !this->mCreationOptions->mUiHint.Value().mName.IsEmpty()) {
      result.name() = Some(this->mCreationOptions->mUiHint.Value().mName);
    }
    if (this->mCreationOptions->mUiHint.WasPassed() &&
        this->mCreationOptions->mUiHint.Value().mExpiresAfter.WasPassed()) {
      result.infoExpiresAt() =
          Some(PR_Now() / PR_USEC_PER_MSEC +
               this->mCreationOptions->mUiHint.Value().mExpiresAfter.Value());
    }
    if (this->mCreationOptions->mToken.WasPassed()) {
      result.token() =
          Some(NS_ConvertUTF8toUTF16(this->mCreationOptions->mToken.Value()));
    }
  }
  if (!this->mToken.IsEmpty()) {
    result.token() = Some(this->mToken);
  }

  return result;
}

// static
already_AddRefed<IdentityCredential> IdentityCredential::Constructor(
    const GlobalObject& aGlobal, const IdentityCredentialInit& aInit,
    ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (!global || !global->GetAsInnerWindow() || !global->PrincipalOrNull()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  RefPtr<IdentityCredential> result =
      new IdentityCredential(global->GetAsInnerWindow());
  result->SetId(aInit.mId);
  result->SetType(u"identity"_ns);
  result->mCreationOptions.emplace(aInit);
  result->mIdentityProvider = global->PrincipalOrNull();
  if (aInit.mToken.WasPassed()) {
    result->mToken = NS_ConvertUTF8toUTF16(aInit.mToken.Value());
  }
  return result.forget();
}

void IdentityCredential::GetToken(nsAString& aToken) const {
  aToken.Assign(mToken);
}
void IdentityCredential::SetToken(const nsAString& aToken) {
  mToken.Assign(aToken);
  if (mCreationOptions.isSome()) {
    mCreationOptions->mToken.Construct(NS_ConvertUTF16toUTF8(aToken));
  }
}

void IdentityCredential::GetOrigin(nsACString& aOrigin,
                                   ErrorResult& aError) const {
  nsresult rv = mIdentityProvider->GetWebExposedOriginSerialization(aOrigin);
  if (NS_FAILED(rv)) {
    aOrigin.SetLength(0);
    aError.Throw(rv);
  }
}

// static
RefPtr<IdentityCredential::GetIdentityCredentialsPromise>
IdentityCredential::CollectFromCredentialStore(
    nsPIDOMWindowInner* aParent, const CredentialRequestOptions& aOptions,
    bool aSameOriginWithAncestors) {
  MOZ_ASSERT(XRE_IsContentProcess());
  MOZ_ASSERT(aParent);
  // Prevent origin confusion by requiring no cross domain iframes
  // in this one's ancestry
  if (!aSameOriginWithAncestors) {
    return IdentityCredential::GetIdentityCredentialsPromise::CreateAndReject(
        NS_ERROR_DOM_NOT_ALLOWED_ERR, __func__);
  }

  Document* parentDocument = aParent->GetExtantDoc();
  if (!parentDocument) {
    return IdentityCredential::GetIdentityCredentialsPromise::CreateAndReject(
        NS_ERROR_FAILURE, __func__);
  }

  // Kick the request off to the main process and translate the result to the
  // expected type when we get a result.
  MOZ_ASSERT(aOptions.mIdentity.WasPassed());
  RefPtr<WindowGlobalChild> wgc = aParent->GetWindowGlobalChild();
  MOZ_ASSERT(wgc);
  return wgc
      ->SendCollectIdentityCredentialFromCredentialStore(
          aOptions.mIdentity.Value())
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [wgc](const WindowGlobalChild::
                    CollectIdentityCredentialFromCredentialStorePromise::
                        ResolveValueType& aResult) {
            nsTArray<RefPtr<IdentityCredential>> resultDOMArray;
            for (const IPCIdentityCredential& ipcResult : aResult) {
              resultDOMArray.AppendElement(
                  new IdentityCredential(wgc->GetWindowGlobal(), ipcResult));
            }
            return IdentityCredential::GetIdentityCredentialsPromise::
                CreateAndResolve(std::move(resultDOMArray), __func__);
          },
          [](const WindowGlobalChild::
                 CollectIdentityCredentialFromCredentialStorePromise::
                     RejectValueType& aResult) {
            return IdentityCredential::GetIdentityCredentialsPromise::
                CreateAndReject(NS_ERROR_DOM_UNKNOWN_ERR, __func__);
          });
}

// static
RefPtr<GenericPromise> IdentityCredential::AllowedToCollectCredential(
    nsIPrincipal* aPrincipal, CanonicalBrowsingContext* aBrowsingContext,
    const IdentityCredentialRequestOptions& aOptions,
    IPCIdentityCredential aCredential) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(aPrincipal);
  for (const nsCString& origin : aCredential.effectiveOrigins()) {
    nsCOMPtr<nsIURI> allowURI;
    nsresult rv = NS_NewURI(getter_AddRefs(allowURI), origin);
    if (NS_SUCCEEDED(rv)) {
      if (aPrincipal->IsSameOrigin(allowURI)) {
        return GenericPromise::CreateAndResolve(true, __func__);
      }
    }
  }
  if (aCredential.effectiveQueryURL().isSome()) {
    // Make the url to test, returning the default resolved to false promise if
    // it fails
    nsCOMPtr<nsIURI> dynamicURI;
    nsresult rv = NS_NewURI(getter_AddRefs(dynamicURI),
                            aCredential.effectiveQueryURL().value());
    if (NS_SUCCEEDED(rv) && aOptions.mProviders.WasPassed()) {
      // at this point we need to run through the providers passed as an
      // argument to "navigator.credentials.get" and see if any meet the
      // conditions to effectively query for aCredential based on an HTTP+CORS
      // endpoint. This gives the IDP control over what origins can use each
      // credential they store.
      for (const auto& provider : aOptions.mProviders.Value()) {
        // We only issue requests if the provider provided by the RP has the
        // same URL as the credential that was stored by the IDP.
        if (!provider.mEffectiveQueryURL.WasPassed() ||
            !provider.mEffectiveQueryURL.Value().Equals(
                aCredential.effectiveQueryURL().value())) {
          continue;
        }

        // We are going to do some work here, so build the promise to return
        RefPtr<GenericPromise::Private> resultPromise =
            new GenericPromise::Private(__func__);
        // Create the JS global for use by the fetch, tied to the RP principal
        nsIXPConnect* xpc = nsContentUtils::XPConnect();
        MOZ_ASSERT(xpc, "This should never be null!");
        nsCOMPtr<nsIGlobalObject> global;
        AutoJSAPI jsapi;
        jsapi.Init();
        JSContext* cx = jsapi.cx();
        JS::Rooted<JSObject*> sandbox(cx);
        rv = xpc->CreateSandbox(cx, aPrincipal, sandbox.address());
        if (NS_WARN_IF(NS_FAILED(rv))) {
          resultPromise->Reject(rv, __func__);
          return resultPromise;
        }
        MOZ_ASSERT(JS_IsGlobalObject(sandbox));
        global = xpc::NativeGlobal(sandbox);
        if (NS_WARN_IF(!global)) {
          resultPromise->Reject(NS_ERROR_FAILURE, __func__);
          return resultPromise;
        }
        // Construct the HTTP+CORS request to be made.
        constexpr auto fragment = ""_ns;
        auto internalRequest = MakeSafeRefPtr<InternalRequest>(
            provider.mEffectiveQueryURL.Value(), fragment);
        internalRequest->SetCredentialsMode(RequestCredentials::Omit);
        internalRequest->SetMode(RequestMode::Cors);
        internalRequest->OverrideContentPolicyType(
            nsContentPolicyType::TYPE_WEB_IDENTITY);
        internalRequest->SetHeaders(
            new InternalHeaders(HeadersGuardEnum::Request));
        RefPtr<Request> request =
            new Request(global, std::move(internalRequest), nullptr);
        RequestOrUTF8String fetchInput;
        fetchInput.SetAsRequest() = request;
        RootedDictionary<RequestInit> requestInit(RootingCx());
        ErrorResult error;
        // Issue the fetch and define the callbacks
        RefPtr<Promise> fetchPromise =
            FetchRequest(request->GetParentObject(), fetchInput, requestInit,
                         CallerType::System, error);
        if (NS_WARN_IF(error.Failed())) {
          resultPromise->Reject(error.StealNSResult(), __func__);
          return resultPromise;
        }
        fetchPromise->AddCallbacksWithCycleCollectedArgs(
            [resultPromise](JSContext* aCx, JS::Handle<JS::Value> aValue,
                            ErrorResult&) {
              // Get the Response object from the argument to the callback
              if (NS_WARN_IF(!aValue.isObject())) {
                resultPromise->Reject(NS_ERROR_FAILURE, __func__);
                return;
              }
              JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());
              MOZ_ASSERT(obj);
              Response* response = nullptr;
              if (NS_WARN_IF(
                      NS_FAILED(UNWRAP_OBJECT(Response, &obj, response)))) {
                resultPromise->Reject(NS_ERROR_FAILURE, __func__);
                return;
              }
              // Resolve whether or not the request was a success
              resultPromise->Resolve(response->Ok(), __func__);
            },
            [resultPromise](JSContext*, JS::Handle<JS::Value> aValue,
                            ErrorResult&) {
              resultPromise->Reject(
                  Promise::TryExtractNSResultFromRejectionValue(aValue),
                  __func__);
            });
        return resultPromise;
      }
    }
  }

  return GenericPromise::CreateAndResolve(false, __func__);
}

// static
RefPtr<IdentityCredential::GetIPCIdentityCredentialsPromise>
IdentityCredential::CollectFromCredentialStoreInMainProcess(
    nsIPrincipal* aPrincipal, CanonicalBrowsingContext* aBrowsingContext,
    const IdentityCredentialRequestOptions& aOptions) {
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(aPrincipal);
  nsresult rv;
  nsCOMPtr<nsIIdentityCredentialStorageService> icStorageService =
      mozilla::components::IdentityCredentialStorageService::Service(&rv);
  if (NS_WARN_IF(!icStorageService)) {
    return IdentityCredential::GetIPCIdentityCredentialsPromise::
        CreateAndReject(rv, __func__);
  }
  nsTArray<RefPtr<nsIPrincipal>> idpPrincipals;
  if (aOptions.mProviders.WasPassed()) {
    for (const auto& idpConfig : aOptions.mProviders.Value()) {
      if (idpConfig.mOrigin.WasPassed()) {
        RefPtr<nsIURI> idpURI;
        rv = NS_NewURI(getter_AddRefs(idpURI), idpConfig.mOrigin.Value());
        if (NS_FAILED(rv)) {
          continue;
        }
        RefPtr<nsIPrincipal> idpPrincipal =
            BasePrincipal::CreateContentPrincipal(
                idpURI, aPrincipal->OriginAttributesRef());
        idpPrincipals.AppendElement(idpPrincipal);
      } else if (idpConfig.mLoginURL.WasPassed()) {
        RefPtr<nsIURI> idpURI;
        rv = NS_NewURI(getter_AddRefs(idpURI), idpConfig.mLoginURL.Value());
        if (NS_FAILED(rv)) {
          continue;
        }
        RefPtr<nsIPrincipal> idpPrincipal =
            BasePrincipal::CreateContentPrincipal(
                idpURI, aPrincipal->OriginAttributesRef());
        idpPrincipals.AppendElement(idpPrincipal);
      }
    }
  }

  CopyableTArray<mozilla::dom::IPCIdentityCredential> fromStore;
  rv = icStorageService->GetIdentityCredentials(idpPrincipals, fromStore);
  if (NS_FAILED(rv)) {
    return GetIPCIdentityCredentialsPromise::CreateAndReject(rv, __func__);
  }

  RefPtr<GetIPCIdentityCredentialsPromise::Private> resultPromise =
      new GetIPCIdentityCredentialsPromise::Private(__func__);
  nsTArray<RefPtr<GenericPromise>> promises;
  for (const IPCIdentityCredential& cred : fromStore) {
    promises.AppendElement(AllowedToCollectCredential(
        aPrincipal, aBrowsingContext, aOptions, cred));
  }
  GenericPromise::AllSettled(GetCurrentSerialEventTarget(), promises)
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [resultPromise, fromStore](
              const GenericPromise::AllSettledPromiseType::ResolveValueType&
                  aResults) {
            if (aResults.Length() != fromStore.Length()) {
              resultPromise->Reject(NS_ERROR_FAILURE, __func__);
              return;
            }
            CopyableTArray<mozilla::dom::IPCIdentityCredential> result;
            for (size_t i = 0; i < aResults.Length(); i++) {
              if (aResults.ElementAt(i).IsResolve() &&
                  aResults.ElementAt(i).ResolveValue()) {
                result.AppendElement(fromStore.ElementAt(i));
              }
            }
            resultPromise->Resolve(result, __func__);
          },
          [resultPromise](
              const GenericPromise::AllSettledPromiseType::RejectValueType&
                  aResult) {
            resultPromise->Reject(NS_ERROR_FAILURE, __func__);
          });

  return resultPromise;
}

// static
RefPtr<GenericPromise> IdentityCredential::Store(
    nsPIDOMWindowInner* aParent, const IdentityCredential* aCredential,
    bool aSameOriginWithAncestors) {
  MOZ_ASSERT(XRE_IsContentProcess());
  MOZ_ASSERT(aParent);
  MOZ_ASSERT(aCredential);
  // Prevent origin confusion by requiring no cross domain iframes
  // in this one's ancestry
  if (!aSameOriginWithAncestors) {
    return GenericPromise::CreateAndReject(NS_ERROR_DOM_NOT_ALLOWED_ERR,
                                           __func__);
  }

  Document* parentDocument = aParent->GetExtantDoc();
  if (!parentDocument) {
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  // Kick the request off to the main process and translate the result to the
  // expected type when we get a result.
  RefPtr<WindowGlobalChild> wgc = aParent->GetWindowGlobalChild();
  MOZ_ASSERT(wgc);
  return wgc
      ->SendStoreIdentityCredential(aCredential->MakeIPCIdentityCredential())
      ->Then(
          GetCurrentSerialEventTarget(), __func__,
          [](const WindowGlobalChild::StoreIdentityCredentialPromise::
                 ResolveValueType& aResult) {
            return GenericPromise::CreateAndResolve(true, __func__);
          },
          [](const WindowGlobalChild::StoreIdentityCredentialPromise::
                 RejectValueType& aResult) {
            return GenericPromise::CreateAndReject(NS_ERROR_DOM_UNKNOWN_ERR,
                                                   __func__);
          });
}

// static
RefPtr<GenericPromise> IdentityCredential::StoreInMainProcess(
    nsIPrincipal* aPrincipal, const IPCIdentityCredential& aCredential) {
  if (!aCredential.identityProvider() ||
      !aCredential.identityProvider()->Equals(aPrincipal)) {
    return GenericPromise::CreateAndReject(nsresult::NS_ERROR_FAILURE,
                                           __func__);
  }
  nsresult error;
  nsCOMPtr<nsIIdentityCredentialStorageService> icStorageService =
      mozilla::components::IdentityCredentialStorageService::Service(&error);
  if (NS_WARN_IF(!icStorageService)) {
    return GenericPromise::CreateAndReject(error, __func__);
  }
  error = icStorageService->StoreIdentityCredential(aCredential);
  if (NS_FAILED(error)) {
    return GenericPromise::CreateAndReject(error, __func__);
  }

  return GenericPromise::CreateAndReject(nsresult::NS_ERROR_FAILURE, __func__);
}

// static
RefPtr<IdentityCredential::GetIdentityCredentialPromise>
IdentityCredential::Create(nsPIDOMWindowInner* aParent,
                           const CredentialCreationOptions& aOptions,
                           bool aSameOriginWithAncestors) {
  MOZ_ASSERT(aOptions.mIdentity.WasPassed());
  MOZ_ASSERT(aParent);
  const IdentityCredentialInit& init = aOptions.mIdentity.Value();
  RefPtr<IdentityCredential> result = new IdentityCredential(aParent);
  result->SetId(init.mId);
  result->SetType(u"identity"_ns);
  result->mCreationOptions.emplace(init);
  if (init.mToken.WasPassed()) {
    result->mToken = NS_ConvertUTF8toUTF16(init.mToken.Value());
  }
  return GetIdentityCredentialPromise::CreateAndResolve(result.forget(),
                                                        __func__);
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

  // Make sure we support the set of features needed for this request
  RequestType requestType = DetermineRequestType(aOptions);
  if ((!StaticPrefs::
           dom_security_credentialmanagement_identity_heavyweight_enabled() &&
       requestType == HEAVYWEIGHT) ||
      (!StaticPrefs::
           dom_security_credentialmanagement_identity_lightweight_enabled() &&
       requestType == LIGHTWEIGHT) ||
      requestType == INVALID) {
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
            return IdentityCredential::
                CreateHeavyweightCredentialDuringDiscovery(
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
IdentityCredential::CreateHeavyweightCredentialDuringDiscovery(
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
            credential.token() = Some(token.mToken);
            credential.id() = account.mId;
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
  nsCString configLocation = aProvider.mConfigURL.Value();
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
        bool correctURL =
            manifest.mProvider_urls[0] == aProvider.mConfigURL.Value();
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
  nsCString configLocation = aProvider.mConfigURL.Value();

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
  nsresult rv =
      NS_NewURI(getter_AddRefs(baseURI), aProvider.mConfigURL.Value());
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
  nsCString baseURIString = aProvider.mConfigURL.Value();
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
  bodyValue.Set("account_id"_ns, NS_ConvertUTF16toUTF8(aAccount.mId));
  bodyValue.Set("client_id"_ns, aProvider.mClientId.Value());
  if (aProvider.mNonce.WasPassed()) {
    bodyValue.Set("nonce"_ns, aProvider.mNonce.Value());
  }
  bodyValue.Set("disclosure_text_shown"_ns, "false"_ns);
  nsAutoCString bodyCString;
  bodyValue.Serialize(bodyCString, true);
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
  nsCString baseURIString = aProvider.mConfigURL.Value();
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
  nsCString configLocation = aProvider.mConfigURL.Value();
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
IdentityCredential::RequestType IdentityCredential::DetermineRequestType(
    const IdentityCredentialRequestOptions& aOptions) {
  if (!aOptions.mProviders.WasPassed()) {
    return INVALID;
  }
  for (const IdentityProviderConfig& provider : aOptions.mProviders.Value()) {
    if (provider.mConfigURL.WasPassed()) {
      return HEAVYWEIGHT;
    }
    if (!provider.mOrigin.WasPassed() && !provider.mLoginURL.WasPassed()) {
      return INVALID;
    }
  }
  return LIGHTWEIGHT;
}

}  // namespace mozilla::dom

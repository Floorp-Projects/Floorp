/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/JSProcessActorProtocol.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTING_ADDREF(JSProcessActorProtocol)
NS_IMPL_CYCLE_COLLECTING_RELEASE(JSProcessActorProtocol)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(JSProcessActorProtocol)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(JSProcessActorProtocol)

/* static */ already_AddRefed<JSProcessActorProtocol>
JSProcessActorProtocol::FromIPC(const JSProcessActorInfo& aInfo) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsContentProcess());

  RefPtr<JSProcessActorProtocol> proto =
      new JSProcessActorProtocol(aInfo.name());

  proto->mRemoteTypes = aInfo.remoteTypes().Clone();
  proto->mChild.mModuleURI = aInfo.url();
  proto->mChild.mObservers = aInfo.observers().Clone();

  return proto.forget();
}

JSProcessActorInfo JSProcessActorProtocol::ToIPC() {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());

  JSProcessActorInfo info;
  info.name() = mName;
  info.remoteTypes() = mRemoteTypes.Clone();
  info.url() = mChild.mModuleURI;
  info.observers() = mChild.mObservers.Clone();
  return info;
}

already_AddRefed<JSProcessActorProtocol>
JSProcessActorProtocol::FromWebIDLOptions(const nsACString& aName,
                                          const ProcessActorOptions& aOptions,
                                          ErrorResult& aRv) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());

  RefPtr<JSProcessActorProtocol> proto = new JSProcessActorProtocol(aName);

  if (aOptions.mParent.WasPassed()) {
    proto->mParent.mModuleURI.emplace(aOptions.mParent.Value().mModuleURI);
  }
  if (aOptions.mChild.WasPassed()) {
    proto->mChild.mModuleURI.emplace(aOptions.mChild.Value().mModuleURI);
  }

  if (!aOptions.mChild.WasPassed() && !aOptions.mParent.WasPassed()) {
    aRv.ThrowNotSupportedError(
        "No point registering an actor with neither child nor parent "
        "specifications.");
    return nullptr;
  }

  if (aOptions.mChild.WasPassed() &&
      aOptions.mChild.Value().mObservers.WasPassed()) {
    proto->mChild.mObservers = aOptions.mChild.Value().mObservers.Value();
  }

  return proto.forget();
}

NS_IMETHODIMP JSProcessActorProtocol::Observe(nsISupports* aSubject,
                                              const char* aTopic,
                                              const char16_t* aData) {
  MOZ_ASSERT(nsContentUtils::IsSafeToRunScript());

  ErrorResult error;
  RefPtr<JSProcessActorChild> actor =
      ContentChild::GetSingleton()->GetActor(mName, error);

  // Don't raise an error if creation of our actor was vetoed.
  if (NS_WARN_IF(error.Failed())) {
    nsresult rv = error.StealNSResult();

    if (rv == NS_ERROR_NOT_AVAILABLE) {
      return NS_OK;
    }
    return rv;
  }

  // Build a observer callback.
  JS::Rooted<JSObject*> global(RootingCx(),
                               JS::GetNonCCWObjectGlobal(actor->GetWrapper()));
  RefPtr<MozObserverCallback> observerCallback =
      new MozObserverCallback(actor->GetWrapper(), global, nullptr, nullptr);
  observerCallback->Observe(aSubject, nsDependentCString(aTopic),
                            aData ? nsDependentString(aData) : VoidString());
  return NS_OK;
}

void JSProcessActorProtocol::AddObservers() {
  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  for (auto& topic : mChild.mObservers) {
    // This makes the observer service hold an owning reference to the
    // JSProcessActorProtocol. The JSWindowActorProtocol objects will be living
    // for the full lifetime of the content process, thus the extra strong
    // referencec doesn't have a negative impact.
    os->AddObserver(this, topic.get(), false);
  }
}

void JSProcessActorProtocol::RemoveObservers() {
  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  for (auto& topic : mChild.mObservers) {
    os->RemoveObserver(this, topic.get());
  }
}

bool JSProcessActorProtocol::RemoteTypePrefixMatches(
    const nsDependentSubstring& aRemoteType) {
  for (auto& remoteType : mRemoteTypes) {
    if (StringBeginsWith(aRemoteType, remoteType)) {
      return true;
    }
  }
  return false;
}

bool JSProcessActorProtocol::Matches(const nsAString& aRemoteType) {
  if (!mRemoteTypes.IsEmpty() &&
      !RemoteTypePrefixMatches(RemoteTypePrefix(aRemoteType))) {
    return false;
  }

  return true;
}

}  // namespace dom
}  // namespace mozilla

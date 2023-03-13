/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Worklet.h"
#include "WorkletThread.h"

#include "mozilla/dom/WorkletFetchHandler.h"
#include "mozilla/dom/WorkletImpl.h"
#include "xpcprivate.h"

namespace mozilla::dom {
// ---------------------------------------------------------------------------
// Worklet

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(Worklet)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Worklet)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mOwnedObject)
  tmp->mImpl->NotifyWorkletFinished();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Worklet)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwnedObject)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Worklet)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Worklet)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Worklet)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

Worklet::Worklet(nsPIDOMWindowInner* aWindow, RefPtr<WorkletImpl> aImpl,
                 nsISupports* aOwnedObject)
    : mWindow(aWindow), mOwnedObject(aOwnedObject), mImpl(std::move(aImpl)) {
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(mImpl);
  MOZ_ASSERT(NS_IsMainThread());
}

Worklet::~Worklet() { mImpl->NotifyWorkletFinished(); }

JSObject* Worklet::WrapObject(JSContext* aCx,
                              JS::Handle<JSObject*> aGivenProto) {
  return mImpl->WrapWorklet(aCx, this, aGivenProto);
}

already_AddRefed<Promise> Worklet::AddModule(JSContext* aCx,
                                             const nsAString& aModuleURL,
                                             const WorkletOptions& aOptions,
                                             CallerType aCallerType,
                                             ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());
  return WorkletFetchHandler::AddModule(this, aCx, aModuleURL, aOptions, aRv);
}

WorkletFetchHandler* Worklet::GetImportFetchHandler(const nsACString& aURI) {
  MOZ_ASSERT(NS_IsMainThread());
  return mImportHandlers.GetWeak(aURI);
}

void Worklet::AddImportFetchHandler(const nsACString& aURI,
                                    WorkletFetchHandler* aHandler) {
  MOZ_ASSERT(aHandler);
  MOZ_ASSERT(!mImportHandlers.GetWeak(aURI));
  MOZ_ASSERT(NS_IsMainThread());

  mImportHandlers.InsertOrUpdate(aURI, RefPtr{aHandler});
}

}  // namespace mozilla::dom

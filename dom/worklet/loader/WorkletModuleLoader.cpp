/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkletModuleLoader.h"

#include "js/loader/ModuleLoadRequest.h"
#include "mozilla/dom/Worklet.h"
#include "mozilla/dom/WorkletFetchHandler.h"

using JS::loader::ModuleLoadRequest;

namespace mozilla::dom::loader {

//////////////////////////////////////////////////////////////
// WorkletScriptLoader
//////////////////////////////////////////////////////////////

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WorkletScriptLoader)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(WorkletScriptLoader)

NS_IMPL_CYCLE_COLLECTING_ADDREF(WorkletScriptLoader)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WorkletScriptLoader)

//////////////////////////////////////////////////////////////
// WorkletModuleLoader
//////////////////////////////////////////////////////////////

NS_IMPL_ADDREF_INHERITED(WorkletModuleLoader, ModuleLoaderBase)
NS_IMPL_RELEASE_INHERITED(WorkletModuleLoader, ModuleLoaderBase)

NS_IMPL_CYCLE_COLLECTION_INHERITED(WorkletModuleLoader, ModuleLoaderBase,
                                   mFetchingRequests)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WorkletModuleLoader)
NS_INTERFACE_MAP_END_INHERITING(ModuleLoaderBase)

WorkletModuleLoader::WorkletModuleLoader(WorkletScriptLoader* aScriptLoader,
                                         nsIGlobalObject* aGlobalObject)
    : ModuleLoaderBase(aScriptLoader, aGlobalObject,
                       GetCurrentSerialEventTarget()) {
  // This should be constructed on a worklet thread.
  MOZ_ASSERT(!NS_IsMainThread());
}

already_AddRefed<ModuleLoadRequest> WorkletModuleLoader::CreateStaticImport(
    nsIURI* aURI, ModuleLoadRequest* aParent) {
  return nullptr;
}

already_AddRefed<ModuleLoadRequest> WorkletModuleLoader::CreateDynamicImport(
    JSContext* aCx, nsIURI* aURI, LoadedScript* aMaybeActiveScript,
    JS::Handle<JS::Value> aReferencingPrivate, JS::Handle<JSString*> aSpecifier,
    JS::Handle<JSObject*> aPromise) {
  return nullptr;
}

bool WorkletModuleLoader::CanStartLoad(ModuleLoadRequest* aRequest,
                                       nsresult* aRvOut) {
  return true;
}

nsresult WorkletModuleLoader::StartFetch(ModuleLoadRequest* aRequest) {
  InsertRequest(aRequest->mURI, aRequest);

  RefPtr<StartFetchRunnable> runnable =
      new StartFetchRunnable(aRequest->GetWorkletLoadContext()->GetHandlerRef(),
                             aRequest->mURI, aRequest->mReferrer);
  NS_DispatchToMainThread(runnable.forget());
  return NS_OK;
}

nsresult WorkletModuleLoader::CompileFetchedModule(
    JSContext* aCx, JS::Handle<JSObject*> aGlobal, JS::CompileOptions& aOptions,
    ModuleLoadRequest* aRequest, JS::MutableHandle<JSObject*> aModuleScript) {
  return NS_ERROR_FAILURE;
}

void WorkletModuleLoader::OnModuleLoadComplete(ModuleLoadRequest* aRequest) {}

void WorkletModuleLoader::InsertRequest(nsIURI* aURI,
                                        ModuleLoadRequest* aRequest) {
  mFetchingRequests.InsertOrUpdate(aURI, aRequest);
}

void WorkletModuleLoader::RemoveRequest(nsIURI* aURI) {
  MOZ_ASSERT(mFetchingRequests.Remove(aURI));
}

ModuleLoadRequest* WorkletModuleLoader::GetRequest(nsIURI* aURI) const {
  RefPtr<ModuleLoadRequest> req;
  MOZ_ALWAYS_TRUE(mFetchingRequests.Get(aURI, getter_AddRefs(req)));
  return req;
}

}  // namespace mozilla::dom::loader

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/workerinternals/ScriptLoader.h"
#include "WorkerModuleLoader.h"

#include "nsISupportsImpl.h"

namespace mozilla::dom::workerinternals::loader {

//////////////////////////////////////////////////////////////
// WorkerModuleLoader
//////////////////////////////////////////////////////////////

NS_IMPL_ADDREF_INHERITED(WorkerModuleLoader, JS::loader::ModuleLoaderBase)
NS_IMPL_RELEASE_INHERITED(WorkerModuleLoader, JS::loader::ModuleLoaderBase)

NS_IMPL_CYCLE_COLLECTION_INHERITED(WorkerModuleLoader,
                                   JS::loader::ModuleLoaderBase)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WorkerModuleLoader)
NS_INTERFACE_MAP_END_INHERITING(JS::loader::ModuleLoaderBase)

WorkerModuleLoader::WorkerModuleLoader(WorkerScriptLoader* aScriptLoader,
                                       nsIGlobalObject* aGlobalObject,
                                       nsISerialEventTarget* aEventTarget)
    : ModuleLoaderBase(aScriptLoader, aGlobalObject, aEventTarget) {}

already_AddRefed<ModuleLoadRequest> WorkerModuleLoader::CreateStaticImport(
    nsIURI* aURI, ModuleLoadRequest* aParent) {
  MOZ_CRASH("Not implemented yet");
}

already_AddRefed<ModuleLoadRequest> WorkerModuleLoader::CreateDynamicImport(
    JSContext* aCx, nsIURI* aURI, LoadedScript* aMaybeActiveScript,
    JS::Handle<JS::Value> aReferencingPrivate, JS::Handle<JSString*> aSpecifier,
    JS::Handle<JSObject*> aPromise) {
  // TODO: Implement for Dedicated workers. Not supported for Service Workers.
  return nullptr;
}

bool WorkerModuleLoader::CanStartLoad(ModuleLoadRequest* aRequest,
                                      nsresult* aRvOut) {
  return true;
}

nsresult WorkerModuleLoader::StartFetch(ModuleLoadRequest* aRequest) {
  return NS_ERROR_FAILURE;
}

nsresult WorkerModuleLoader::CompileFetchedModule(
    JSContext* aCx, JS::Handle<JSObject*> aGlobal, JS::CompileOptions& aOptions,
    ModuleLoadRequest* aRequest, JS::MutableHandle<JSObject*> aModuleScript) {
  return NS_ERROR_FAILURE;
}

WorkerScriptLoader* WorkerModuleLoader::GetScriptLoader() {
  return static_cast<WorkerScriptLoader*>(mLoader.get());
}

void WorkerModuleLoader::OnModuleLoadComplete(ModuleLoadRequest* aRequest) {}

}  // namespace mozilla::dom::workerinternals::loader

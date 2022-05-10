/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ComponentModuleLoader.h"

#include "nsISupportsImpl.h"

using namespace JS::loader;

namespace mozilla {
namespace loader {

//////////////////////////////////////////////////////////////
// ComponentScriptLoader
//////////////////////////////////////////////////////////////

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ComponentScriptLoader)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(ComponentScriptLoader)

NS_IMPL_CYCLE_COLLECTING_ADDREF(ComponentScriptLoader)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ComponentScriptLoader)

nsIURI* ComponentScriptLoader::GetBaseURI() const { return nullptr; }

void ComponentScriptLoader::ReportErrorToConsole(ScriptLoadRequest* aRequest,
                                                 nsresult aResult) const {}

void ComponentScriptLoader::ReportWarningToConsole(
    ScriptLoadRequest* aRequest, const char* aMessageName,
    const nsTArray<nsString>& aParams) const {}

nsresult ComponentScriptLoader::FillCompileOptionsForRequest(
    JSContext* cx, ScriptLoadRequest* aRequest, JS::CompileOptions* aOptions,
    JS::MutableHandle<JSScript*> aIntroductionScript) {
  return NS_OK;
}

//////////////////////////////////////////////////////////////
// ComponentModuleLoader
//////////////////////////////////////////////////////////////

NS_IMPL_CYCLE_COLLECTION_INHERITED(ComponentModuleLoader,
                                   JS::loader::ModuleLoaderBase)

ComponentModuleLoader::ComponentModuleLoader(
    ComponentScriptLoader* aScriptLoader, nsIGlobalObject* aGlobalObject)
    : ModuleLoaderBase(aScriptLoader, aGlobalObject) {}

already_AddRefed<ModuleLoadRequest> ComponentModuleLoader::CreateStaticImport(
    nsIURI* aURI, ModuleLoadRequest* aParent) {
  MOZ_CRASH("NYI");
}

already_AddRefed<ModuleLoadRequest> ComponentModuleLoader::CreateDynamicImport(
    JSContext* aCx, nsIURI* aURI, LoadedScript* aMaybeActiveScript,
    JS::Handle<JS::Value> aReferencingPrivate, JS::Handle<JSString*> aSpecifier,
    JS::Handle<JSObject*> aPromise) {
  MOZ_CRASH("NYI");
}

bool ComponentModuleLoader::CanStartLoad(ModuleLoadRequest* aRequest,
                                         nsresult* aRvOut) {
  return true;
}

nsresult ComponentModuleLoader::StartFetch(ModuleLoadRequest* aRequest) {
  return NS_ERROR_FAILURE;
}

nsresult ComponentModuleLoader::CompileFetchedModule(
    JSContext* aCx, JS::Handle<JSObject*> aGlobal, JS::CompileOptions& aOptions,
    ModuleLoadRequest* aRequest, JS::MutableHandle<JSObject*> aModuleScript) {
  return NS_ERROR_FAILURE;
}

void ComponentModuleLoader::OnModuleLoadComplete(ModuleLoadRequest* aRequest) {}

}  // namespace loader
}  // namespace mozilla

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_loader_WorkerModuleLoader_h
#define mozilla_loader_WorkerModuleLoader_h

#include "js/loader/ModuleLoaderBase.h"

namespace mozilla::dom::workerinternals::loader {
class WorkerScriptLoader;

// alias common classes
using ScriptFetchOptions = JS::loader::ScriptFetchOptions;
using ScriptKind = JS::loader::ScriptKind;
using ScriptLoadRequest = JS::loader::ScriptLoadRequest;
using ScriptLoadRequestList = JS::loader::ScriptLoadRequestList;
using ModuleLoadRequest = JS::loader::ModuleLoadRequest;

class WorkerModuleLoader : public JS::loader::ModuleLoaderBase {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(WorkerModuleLoader,
                                           JS::loader::ModuleLoaderBase)

  WorkerModuleLoader(WorkerScriptLoader* aScriptLoader,
                     nsIGlobalObject* aGlobalObject,
                     nsISerialEventTarget* aEventTarget);

 private:
  ~WorkerModuleLoader() = default;

  WorkerScriptLoader* GetScriptLoader();

  already_AddRefed<ModuleLoadRequest> CreateStaticImport(
      nsIURI* aURI, ModuleLoadRequest* aParent) override;

  already_AddRefed<ModuleLoadRequest> CreateDynamicImport(
      JSContext* aCx, nsIURI* aURI, LoadedScript* aMaybeActiveScript,
      JS::Handle<JS::Value> aReferencingPrivate,
      JS::Handle<JSString*> aSpecifier,
      JS::Handle<JSObject*> aPromise) override;

  bool CanStartLoad(ModuleLoadRequest* aRequest, nsresult* aRvOut) override;

  nsresult StartFetch(ModuleLoadRequest* aRequest) override;

  nsresult CompileFetchedModule(
      JSContext* aCx, JS::Handle<JSObject*> aGlobal,
      JS::CompileOptions& aOptions, ModuleLoadRequest* aRequest,
      JS::MutableHandle<JSObject*> aModuleScript) override;

  void OnModuleLoadComplete(ModuleLoadRequest* aRequest) override;
};

}  // namespace mozilla::dom::workerinternals::loader
#endif  // mozilla_loader_WorkerModuleLoader_h

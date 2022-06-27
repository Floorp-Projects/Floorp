/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_loader_ComponentModuleLoader_h
#define mozilla_loader_ComponentModuleLoader_h

#include "js/loader/LoadContextBase.h"
#include "js/loader/ModuleLoaderBase.h"

class mozJSComponentLoader;

namespace mozilla {
namespace loader {

class ComponentScriptLoader : public JS::loader::ScriptLoaderInterface {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(ComponentScriptLoader)

 private:
  ~ComponentScriptLoader() = default;

  nsIURI* GetBaseURI() const override;

  void ReportErrorToConsole(ScriptLoadRequest* aRequest,
                            nsresult aResult) const override;

  void ReportWarningToConsole(ScriptLoadRequest* aRequest,
                              const char* aMessageName,
                              const nsTArray<nsString>& aParams) const override;

  nsresult FillCompileOptionsForRequest(
      JSContext* cx, ScriptLoadRequest* aRequest, JS::CompileOptions* aOptions,
      JS::MutableHandle<JSScript*> aIntroductionScript) override;
};

class ComponentModuleLoader : public JS::loader::ModuleLoaderBase {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ComponentModuleLoader,
                                           JS::loader::ModuleLoaderBase)

  ComponentModuleLoader(ComponentScriptLoader* aScriptLoader,
                        nsIGlobalObject* aGlobalObject);

  [[nodiscard]] nsresult ProcessRequests();

 private:
  // An event target that dispatches runnables by executing them
  // immediately. This is used to drive mozPromise dispatch for
  // ComponentModuleLoader.
  class SyncEventTarget : public nsISerialEventTarget {
   public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIEVENTTARGET_FULL
   private:
    virtual ~SyncEventTarget() = default;
  };

  ~ComponentModuleLoader();

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

  JS::loader::ScriptLoadRequestList mLoadRequests;
};

// Data specific to ComponentModuleLoader that is associated with each load
// request.
class ComponentLoadContext : public JS::loader::LoadContextBase {
 public:
  ComponentLoadContext()
      : LoadContextBase(JS::loader::ContextKind::Component) {}

 public:
  // The result of loading a module script. These fields are used temporarily
  // before being passed to the module loader.
  nsresult mRv;
  JS::PersistentRootedValue mExceptionValue;
  JS::PersistentRootedScript mScript;
};

}  // namespace loader
}  // namespace mozilla

#endif  // mozilla_loader_ComponentModuleLoader_h

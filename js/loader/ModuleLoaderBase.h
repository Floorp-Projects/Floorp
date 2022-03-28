/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_loader_ModuleLoaderBase_h
#define js_loader_ModuleLoaderBase_h

#include "LoadedScript.h"
#include "ScriptLoadRequest.h"

#include "js/TypeDecls.h"  // JS::MutableHandle, JS::Handle, JS::Root
#include "js/Modules.h"
#include "nsRefPtrHashtable.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsILoadInfo.h"  // nsSecurityFlags
#include "nsINode.h"      // nsIURI
#include "nsURIHashKey.h"
#include "mozilla/CORSMode.h"
#include "mozilla/dom/JSExecutionContext.h"
#include "mozilla/MaybeOneOf.h"
#include "mozilla/MozPromise.h"
#include "ModuleMapKey.h"

class nsIURI;

namespace mozilla {

class LazyLogModule;
union Utf8Unit;

}  // namespace mozilla

namespace JS {

class CompileOptions;

template <typename UnitT>
class SourceText;

namespace loader {

class ModuleLoaderBase;
class ModuleLoadRequest;
class ModuleScript;

class ScriptLoaderInterface : public nsISupports {
 public:
  // alias common classes
  using ScriptFetchOptions = JS::loader::ScriptFetchOptions;
  using ScriptKind = JS::loader::ScriptKind;
  using ScriptLoadRequest = JS::loader::ScriptLoadRequest;
  using ScriptLoadRequestList = JS::loader::ScriptLoadRequestList;
  using ModuleLoadRequest = JS::loader::ModuleLoadRequest;

  // In some environments, we will need to default to a base URI
  virtual nsIURI* GetBaseURI() const = 0;

  // Get the global for the associated request.
  virtual already_AddRefed<nsIGlobalObject> GetGlobalForRequest(
      ScriptLoadRequest* aRequest) = 0;

  virtual void ReportErrorToConsole(ScriptLoadRequest* aRequest,
                                    nsresult aResult) const = 0;

  // Fill in CompileOptions, as well as produce the introducer script for
  // subsequent calls to UpdateDebuggerMetadata
  virtual nsresult FillCompileOptionsForRequest(
      JSContext* cx, ScriptLoadRequest* aRequest, JS::CompileOptions* aOptions,
      JS::MutableHandle<JSScript*> aIntroductionScript) = 0;

  virtual void MaybePrepareModuleForBytecodeEncodingBeforeExecute(
      JSContext* aCx, ModuleLoadRequest* aRequest) = 0;

  virtual nsresult MaybePrepareModuleForBytecodeEncodingAfterExecute(
      ModuleLoadRequest* aRequest, nsresult aRv) = 0;

  virtual void MaybeTriggerBytecodeEncoding() = 0;
};

class ModuleLoaderBase : public nsISupports {
 private:
  using GenericNonExclusivePromise = mozilla::GenericNonExclusivePromise;
  using GenericPromise = mozilla::GenericPromise;

  // Module map
  nsRefPtrHashtable<ModuleMapKey, GenericNonExclusivePromise::Private>
      mFetchingModules;
  nsRefPtrHashtable<ModuleMapKey, ModuleScript> mFetchedModules;

  // List of dynamic imports that are currently being loaded.
  ScriptLoadRequestList mDynamicImportRequests;

 protected:
  RefPtr<ScriptLoaderInterface> mLoader;

  virtual ~ModuleLoaderBase();

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(ModuleLoaderBase)
  explicit ModuleLoaderBase(ScriptLoaderInterface* aLoader);

  using LoadedScript = JS::loader::LoadedScript;
  using ScriptFetchOptions = JS::loader::ScriptFetchOptions;
  using ScriptLoadRequest = JS::loader::ScriptLoadRequest;
  using ModuleLoadRequest = JS::loader::ModuleLoadRequest;

  using MaybeSourceText =
      mozilla::MaybeOneOf<JS::SourceText<char16_t>, JS::SourceText<Utf8Unit>>;

  // Methods that must be implemented by an extending class. These are called
  // internally by ModuleLoaderBase.

 private:
  // Create a module load request for a static module import.
  virtual already_AddRefed<ModuleLoadRequest> CreateStaticImport(
      nsIURI* aURI, ModuleLoadRequest* aParent) = 0;

  // Called by HostImportModuleDynamically hook.
  virtual already_AddRefed<ModuleLoadRequest> CreateDynamicImport(
      JSContext* aCx, nsIURI* aURI, LoadedScript* aMaybeActiveScript,
      JS::Handle<JS::Value> aReferencingPrivate,
      JS::Handle<JSString*> aSpecifier, JS::Handle<JSObject*> aPromise) = 0;

  // Check whether we can load a module. May return false with |aRvOut| set to
  // NS_OK to abort load without returning an error.
  virtual bool CanStartLoad(ModuleLoadRequest* aRequest, nsresult* aRvOut) = 0;

  // Start the process of fetching module source or bytecode. This is only
  // called if CanStartLoad returned true.
  virtual nsresult StartFetch(ModuleLoadRequest* aRequest) = 0;

  virtual void ProcessLoadedModuleTree(ModuleLoadRequest* aRequest) = 0;
  virtual nsresult CompileOrFinishModuleScript(
      JSContext* aCx, JS::Handle<JSObject*> aGlobal,
      JS::CompileOptions& aOptions, ModuleLoadRequest* aRequest,
      JS::MutableHandle<JSObject*> aModuleScript) = 0;

  // Public API methods.

 public:
  bool HasPendingDynamicImports() const;
  void CancelDynamicImport(ModuleLoadRequest* aRequest, nsresult aResult);
#ifdef DEBUG
  bool HasDynamicImport(ModuleLoadRequest* aRequest) const;
#endif

  // Start a load for a module script URI. Returns immediately if the module is
  // already being loaded.
  nsresult StartModuleLoad(ModuleLoadRequest* aRequest);
  nsresult RestartModuleLoad(ModuleLoadRequest* aRequest);

  void SetModuleFetchFinishedAndResumeWaitingRequests(
      ModuleLoadRequest* aRequest, nsresult aResult);

  nsresult ProcessFetchedModuleSource(ModuleLoadRequest* aRequest);
  bool InstantiateModuleTree(ModuleLoadRequest* aRequest);

  // Implements https://html.spec.whatwg.org/#run-a-module-script
  nsresult EvaluateModule(nsIGlobalObject* aGlobalObject,
                          ScriptLoadRequest* aRequest);

  void StartDynamicImport(ModuleLoadRequest* aRequest);
  void ProcessDynamicImport(ModuleLoadRequest* aRequest);
  void CancelAndClearDynamicImports();

  // Internal methods.

 private:
  friend class JS::loader::ModuleLoadRequest;

  static ModuleLoaderBase* GetCurrentModuleLoader(JSContext* aCx);
  static LoadedScript* GetLoadedScriptOrNull(
      JSContext* aCx, JS::Handle<JS::Value> aReferencingPrivate);

  static void EnsureModuleHooksInitialized();

  static void DynamicImportPrefChangedCallback(const char* aPrefName,
                                               void* aClosure);

  static JSObject* HostResolveImportedModule(
      JSContext* aCx, JS::Handle<JS::Value> aReferencingPrivate,
      JS::Handle<JSObject*> aModuleRequest);
  static bool HostPopulateImportMeta(JSContext* aCx,
                                     JS::Handle<JS::Value> aReferencingPrivate,
                                     JS::Handle<JSObject*> aMetaObject);
  static bool HostImportModuleDynamically(
      JSContext* aCx, JS::Handle<JS::Value> aReferencingPrivate,
      JS::Handle<JSObject*> aModuleRequest, JS::Handle<JSObject*> aPromise);
  static bool HostGetSupportedImportAssertions(
      JSContext* aCx, JS::ImportAssertionVector& aValues);

  static already_AddRefed<nsIURI> ResolveModuleSpecifier(
      ModuleLoaderBase* aLoader, LoadedScript* aScript,
      const nsAString& aSpecifier);
  static nsresult HandleResolveFailure(JSContext* aCx, LoadedScript* aScript,
                                       const nsAString& aSpecifier,
                                       uint32_t aLineNumber,
                                       uint32_t aColumnNumber,
                                       JS::MutableHandle<JS::Value> errorOut);

  enum class RestartRequest { No, Yes };
  nsresult StartOrRestartModuleLoad(ModuleLoadRequest* aRequest,
                                    RestartRequest aRestart);

  bool ModuleMapContainsURL(nsIURI* aURL, nsIGlobalObject* aGlobal) const;
  bool IsModuleFetching(nsIURI* aURL, nsIGlobalObject* aGlobal) const;
  RefPtr<GenericNonExclusivePromise> WaitForModuleFetch(
      nsIURI* aURL, nsIGlobalObject* aGlobal);
  void SetModuleFetchStarted(ModuleLoadRequest* aRequest);

  ModuleScript* GetFetchedModule(nsIURI* aURL, nsIGlobalObject* aGlobal) const;

  // Helper function to set up the global correctly for dynamic imports.
  nsresult EvaluateModule(ScriptLoadRequest* aRequest);

  JS::Value FindFirstParseError(ModuleLoadRequest* aRequest);
  static nsresult InitDebuggerDataForModuleTree(JSContext* aCx,
                                                ModuleLoadRequest* aRequest);
  static nsresult ResolveRequestedModules(ModuleLoadRequest* aRequest,
                                          nsCOMArray<nsIURI>* aUrlsOut);

  void StartFetchingModuleDependencies(ModuleLoadRequest* aRequest);

  RefPtr<GenericPromise> StartFetchingModuleAndDependencies(
      ModuleLoadRequest* aParent, nsIURI* aURI);

  /**
   * Shorthand Wrapper for JSAPI FinishDynamicImport function for the reject
   * case where we do not have `aEvaluationPromise`. As there is no evaluation
   * Promise, JS::FinishDynamicImport will always reject.
   *
   * @param aRequest
   *        The module load request for the dynamic module.
   * @param aResult
   *        The result of running ModuleEvaluate -- If this is successful, then
   *        we can await the associated EvaluationPromise.
   */
  static void FinishDynamicImportAndReject(ModuleLoadRequest* aRequest,
                                           nsresult aResult);

  /**
   * Wrapper for JSAPI FinishDynamicImport function. Takes an optional argument
   * `aEvaluationPromise` which, if null, exits early.
   *
   * This is the Top Level Await version, which works with modules which return
   * promises.
   *
   * @param aCX
   *        The JSContext for the module.
   * @param aRequest
   *        The module load request for the dynamic module.
   * @param aResult
   *        The result of running ModuleEvaluate -- If this is successful, then
   *        we can await the associated EvaluationPromise.
   * @param aEvaluationPromise
   *        The evaluation promise returned from evaluating the module. If this
   *        is null, JS::FinishDynamicImport will reject the dynamic import
   *        module promise.
   */
  static void FinishDynamicImport(JSContext* aCx, ModuleLoadRequest* aRequest,
                                  nsresult aResult,
                                  JS::Handle<JSObject*> aEvaluationPromise);

  void RemoveDynamicImport(ModuleLoadRequest* aRequest);

  nsresult CreateModuleScript(ModuleLoadRequest* aRequest);

 public:
  static mozilla::LazyLogModule gCspPRLog;
  static mozilla::LazyLogModule gModuleLoaderBaseLog;
};

}  // namespace loader
}  // namespace JS

#endif  // js_loader_ModuleLoaderBase_h

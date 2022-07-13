/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ComponentModuleLoader.h"

#include "nsISupportsImpl.h"

#include "js/loader/ModuleLoadRequest.h"
#include "mozJSModuleLoader.h"

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

NS_IMPL_ADDREF_INHERITED(ComponentModuleLoader, JS::loader::ModuleLoaderBase)
NS_IMPL_RELEASE_INHERITED(ComponentModuleLoader, JS::loader::ModuleLoaderBase)

NS_IMPL_CYCLE_COLLECTION_INHERITED(ComponentModuleLoader,
                                   JS::loader::ModuleLoaderBase, mLoadRequests)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ComponentModuleLoader)
NS_INTERFACE_MAP_END_INHERITING(JS::loader::ModuleLoaderBase)

ComponentModuleLoader::ComponentModuleLoader(
    ComponentScriptLoader* aScriptLoader, nsIGlobalObject* aGlobalObject)
    : ModuleLoaderBase(aScriptLoader, aGlobalObject, new SyncEventTarget()) {}

ComponentModuleLoader::~ComponentModuleLoader() {
  MOZ_ASSERT(mLoadRequests.isEmpty());
}

already_AddRefed<ModuleLoadRequest> ComponentModuleLoader::CreateStaticImport(
    nsIURI* aURI, ModuleLoadRequest* aParent) {
  RefPtr<ComponentLoadContext> context = new ComponentLoadContext();
  RefPtr<ModuleLoadRequest> request = new ModuleLoadRequest(
      aURI, aParent->mFetchOptions, SRIMetadata(), aParent->mURI, context,
      false, /* is top level */
      false, /* is dynamic import */
      this, aParent->mVisitedSet, aParent->GetRootModule());
  return request.forget();
}

already_AddRefed<ModuleLoadRequest> ComponentModuleLoader::CreateDynamicImport(
    JSContext* aCx, nsIURI* aURI, LoadedScript* aMaybeActiveScript,
    JS::Handle<JS::Value> aReferencingPrivate, JS::Handle<JSString*> aSpecifier,
    JS::Handle<JSObject*> aPromise) {
  return nullptr;  // Not yet implemented.
}

bool ComponentModuleLoader::CanStartLoad(ModuleLoadRequest* aRequest,
                                         nsresult* aRvOut) {
  return mozJSModuleLoader::IsTrustedScheme(aRequest->mURI);
}

nsresult ComponentModuleLoader::StartFetch(ModuleLoadRequest* aRequest) {
  MOZ_ASSERT(aRequest->HasLoadContext());

  aRequest->mBaseURL = aRequest->mURI;

  // Loading script source and compilation are intertwined in
  // mozJSModuleLoader. Perform both operations here but only report load
  // failures. Compilation failure is reported in CompileFetchedModule.

  dom::AutoJSAPI jsapi;
  if (!jsapi.Init(GetGlobalObject())) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();
  RootedScript script(cx);
  nsresult rv =
      mozJSModuleLoader::LoadSingleModuleScript(cx, aRequest->mURI, &script);
  MOZ_ASSERT_IF(jsapi.HasException(), NS_FAILED(rv));
  MOZ_ASSERT(bool(script) == NS_SUCCEEDED(rv));

  // Check for failure to load script source and abort.
  bool threwException = jsapi.HasException();
  if (NS_FAILED(rv) && !threwException) {
    return rv;
  }

  // Otherwise remember the results so we can report them later.
  ComponentLoadContext* context = aRequest->GetComponentLoadContext();
  context->mRv = rv;
  if (threwException) {
    context->mExceptionValue.init(cx);
    if (!jsapi.StealException(&context->mExceptionValue)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }
  if (script) {
    context->mScript.init(cx);
    context->mScript = script;
  }

  mLoadRequests.AppendElement(aRequest);

  return NS_OK;
}

nsresult ComponentModuleLoader::CompileFetchedModule(
    JSContext* aCx, JS::Handle<JSObject*> aGlobal, JS::CompileOptions& aOptions,
    ModuleLoadRequest* aRequest, JS::MutableHandle<JSObject*> aModuleOut) {
  // Compilation already happened in StartFetch. Report the result here.
  ComponentLoadContext* context = aRequest->GetComponentLoadContext();
  nsresult rv = context->mRv;
  if (context->mScript) {
    aModuleOut.set(JS::GetModuleObject(context->mScript));
    context->mScript = nullptr;
  }
  if (NS_FAILED(rv)) {
    JS_SetPendingException(aCx, context->mExceptionValue);
    context->mExceptionValue = JS::UndefinedValue();
  }

  MOZ_ASSERT(JS_IsExceptionPending(aCx) == NS_FAILED(rv));
  MOZ_ASSERT(bool(aModuleOut) == NS_SUCCEEDED(rv));

  return rv;
}

void ComponentModuleLoader::OnModuleLoadComplete(ModuleLoadRequest* aRequest) {}

nsresult ComponentModuleLoader::ProcessRequests() {
  // Work list to drive module loader since this is all synchronous.
  while (!mLoadRequests.isEmpty()) {
    RefPtr<ScriptLoadRequest> request = mLoadRequests.StealFirst();
    nsresult rv = OnFetchComplete(request->AsModuleRequest(), NS_OK);
    if (NS_FAILED(rv)) {
      mLoadRequests.CancelRequestsAndClear();
      return rv;
    }
  }

  return NS_OK;
}

//////////////////////////////////////////////////////////////
// ComponentModuleLoader::SyncEventTarget
//////////////////////////////////////////////////////////////

NS_IMPL_ADDREF(ComponentModuleLoader::SyncEventTarget)
NS_IMPL_RELEASE(ComponentModuleLoader::SyncEventTarget)

NS_INTERFACE_MAP_BEGIN(ComponentModuleLoader::SyncEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsISerialEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
ComponentModuleLoader::SyncEventTarget::DispatchFromScript(
    nsIRunnable* aRunnable, uint32_t aFlags) {
  nsCOMPtr<nsIRunnable> event(aRunnable);
  return Dispatch(event.forget(), aFlags);
}

NS_IMETHODIMP
ComponentModuleLoader::SyncEventTarget::Dispatch(
    already_AddRefed<nsIRunnable> aRunnable, uint32_t aFlags) {
  MOZ_ASSERT(IsOnCurrentThreadInfallible());

  nsCOMPtr<nsIRunnable> runnable(aRunnable);
  runnable->Run();

  return NS_OK;
}

NS_IMETHODIMP
ComponentModuleLoader::SyncEventTarget::DelayedDispatch(
    already_AddRefed<nsIRunnable>, uint32_t) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ComponentModuleLoader::SyncEventTarget::RegisterShutdownTask(
    nsITargetShutdownTask* aTask) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ComponentModuleLoader::SyncEventTarget::UnregisterShutdownTask(
    nsITargetShutdownTask* aTask) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ComponentModuleLoader::SyncEventTarget::IsOnCurrentThread(
    bool* aIsOnCurrentThread) {
  MOZ_ASSERT(aIsOnCurrentThread);
  *aIsOnCurrentThread = IsOnCurrentThreadInfallible();
  return NS_OK;
}

NS_IMETHODIMP_(bool)
ComponentModuleLoader::SyncEventTarget::IsOnCurrentThreadInfallible() {
  return NS_IsMainThread();
}

}  // namespace loader
}  // namespace mozilla

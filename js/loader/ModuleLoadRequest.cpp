/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ModuleLoadRequest.h"

#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/ScriptLoadContext.h"

#include "LoadedScript.h"
#include "LoadContextBase.h"
#include "ModuleLoaderBase.h"

namespace JS::loader {

#undef LOG
#define LOG(args)                                                           \
  MOZ_LOG(ModuleLoaderBase::gModuleLoaderBaseLog, mozilla::LogLevel::Debug, \
          args)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ModuleLoadRequest)
NS_INTERFACE_MAP_END_INHERITING(ScriptLoadRequest)

NS_IMPL_CYCLE_COLLECTION_CLASS(ModuleLoadRequest)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(ModuleLoadRequest,
                                                ScriptLoadRequest)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLoader, mModuleScript, mImports, mRootModule)
  tmp->ClearDynamicImport();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ModuleLoadRequest,
                                                  ScriptLoadRequest)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLoader, mModuleScript, mImports,
                                    mRootModule)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(ModuleLoadRequest,
                                               ScriptLoadRequest)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mDynamicReferencingPrivate)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mDynamicSpecifier)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mDynamicPromise)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_ADDREF_INHERITED(ModuleLoadRequest, ScriptLoadRequest)
NS_IMPL_RELEASE_INHERITED(ModuleLoadRequest, ScriptLoadRequest)

/* static */
VisitedURLSet* ModuleLoadRequest::NewVisitedSetForTopLevelImport(nsIURI* aURI) {
  auto set = new VisitedURLSet();
  set->PutEntry(aURI);
  return set;
}

ModuleLoadRequest::ModuleLoadRequest(
    nsIURI* aURI, ScriptFetchOptions* aFetchOptions,
    const mozilla::dom::SRIMetadata& aIntegrity, nsIURI* aReferrer,
    LoadContextBase* aContext, bool aIsTopLevel, bool aIsDynamicImport,
    ModuleLoaderBase* aLoader, VisitedURLSet* aVisitedSet,
    ModuleLoadRequest* aRootModule)
    : ScriptLoadRequest(ScriptKind::eModule, aURI, aFetchOptions, aIntegrity,
                        aReferrer, aContext),
      mIsTopLevel(aIsTopLevel),
      mIsDynamicImport(aIsDynamicImport),
      mLoader(aLoader),
      mRootModule(aRootModule),
      mVisitedSet(aVisitedSet) {
  MOZ_ASSERT(mLoader);
}

nsIGlobalObject* ModuleLoadRequest::GetGlobalObject() {
  return mLoader->GetGlobalObject();
}

void ModuleLoadRequest::Cancel() {
  ScriptLoadRequest::Cancel();
  mModuleScript = nullptr;
  CancelImports();
  mReady.RejectIfExists(NS_ERROR_DOM_ABORT_ERR, __func__);
}

void ModuleLoadRequest::CancelImports() {
  for (size_t i = 0; i < mImports.Length(); i++) {
    mImports[i]->Cancel();
  }
}

void ModuleLoadRequest::SetReady() {
  // Mark a module as ready to execute. This means that this module and all it
  // dependencies have had their source loaded, parsed as a module and the
  // modules instantiated.
  //
  // The mReady promise is used to ensure that when all dependencies of a module
  // have become ready, DependenciesLoaded is called on that module
  // request. This is set up in StartFetchingModuleDependencies.

#ifdef DEBUG
  for (size_t i = 0; i < mImports.Length(); i++) {
    MOZ_ASSERT(mImports[i]->IsReadyToRun());
  }
#endif

  ScriptLoadRequest::SetReady();
  mReady.ResolveIfExists(true, __func__);
}

void ModuleLoadRequest::ModuleLoaded() {
  // A module that was found to be marked as fetching in the module map has now
  // been loaded.

  LOG(("ScriptLoadRequest (%p): Module loaded", this));

  mModuleScript = mLoader->GetFetchedModule(mURI);
  if (!mModuleScript || mModuleScript->HasParseError()) {
    ModuleErrored();
    return;
  }

  mLoader->StartFetchingModuleDependencies(this);
}

void ModuleLoadRequest::ModuleErrored() {
  if (IsCanceled()) {
    return;
  }

  LOG(("ScriptLoadRequest (%p): Module errored", this));

  CheckModuleDependenciesLoaded();
  MOZ_ASSERT(!mModuleScript || mModuleScript->HasParseError());

  CancelImports();
  SetReady();
  LoadFinished();
}

void ModuleLoadRequest::DependenciesLoaded() {
  if (IsCanceled()) {
    return;
  }

  // The module and all of its dependencies have been successfully fetched and
  // compiled.

  LOG(("ScriptLoadRequest (%p): Module dependencies loaded", this));

  MOZ_ASSERT(mModuleScript);

  CheckModuleDependenciesLoaded();
  SetReady();
  LoadFinished();
}

void ModuleLoadRequest::CheckModuleDependenciesLoaded() {
  LOG(("ScriptLoadRequest (%p): Check dependencies loaded", this));

  if (!mModuleScript || mModuleScript->HasParseError()) {
    return;
  }
  for (const auto& childRequest : mImports) {
    ModuleScript* childScript = childRequest->mModuleScript;
    if (!childScript) {
      mModuleScript = nullptr;
      LOG(("ScriptLoadRequest (%p):   %p failed (load error)", this,
           childRequest.get()));
      return;
    }
  }

  LOG(("ScriptLoadRequest (%p):   all ok", this));
}

void ModuleLoadRequest::LoadFailed() {
  // We failed to load the source text or an error occurred unrelated to the
  // content of the module (e.g. OOM).

  LOG(("ScriptLoadRequest (%p): Module load failed", this));

  MOZ_ASSERT(!mModuleScript);

  Cancel();
  LoadFinished();
}

void ModuleLoadRequest::LoadFinished() {
  RefPtr<ModuleLoadRequest> request(this);
  if (IsTopLevel() && IsDynamicImport()) {
    mLoader->RemoveDynamicImport(request);
  }

  mLoader->OnModuleLoadComplete(request);
}

void ModuleLoadRequest::ClearDynamicImport() {
  mDynamicReferencingPrivate = JS::UndefinedValue();
  mDynamicSpecifier = nullptr;
  mDynamicPromise = nullptr;
}

}  // namespace JS::loader

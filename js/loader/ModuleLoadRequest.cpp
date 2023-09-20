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

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(ModuleLoadRequest,
                                               ScriptLoadRequest)

NS_IMPL_CYCLE_COLLECTION_CLASS(ModuleLoadRequest)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(ModuleLoadRequest,
                                                ScriptLoadRequest)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLoader, mRootModule, mModuleScript, mImports,
                                  mWaitingParentRequest)
  tmp->ClearDynamicImport();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ModuleLoadRequest,
                                                  ScriptLoadRequest)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLoader, mRootModule, mModuleScript,
                                    mImports, mWaitingParentRequest)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(ModuleLoadRequest,
                                               ScriptLoadRequest)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mDynamicReferencingPrivate)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mDynamicSpecifier)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mDynamicPromise)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

/* static */
VisitedURLSet* ModuleLoadRequest::NewVisitedSetForTopLevelImport(nsIURI* aURI) {
  auto set = new VisitedURLSet();
  set->PutEntry(aURI);
  return set;
}

ModuleLoadRequest::ModuleLoadRequest(
    nsIURI* aURI, mozilla::dom::ReferrerPolicy aReferrerPolicy,
    ScriptFetchOptions* aFetchOptions,
    const mozilla::dom::SRIMetadata& aIntegrity, nsIURI* aReferrer,
    LoadContextBase* aContext, bool aIsTopLevel, bool aIsDynamicImport,
    ModuleLoaderBase* aLoader, VisitedURLSet* aVisitedSet,
    ModuleLoadRequest* aRootModule)
    : ScriptLoadRequest(ScriptKind::eModule, aURI, aReferrerPolicy,
                        aFetchOptions, aIntegrity, aReferrer, aContext),
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

bool ModuleLoadRequest::IsErrored() const {
  return !mModuleScript || mModuleScript->HasParseError();
}

void ModuleLoadRequest::Cancel() {
  if (IsCanceled()) {
    AssertAllImportsCancelled();
    return;
  }

  if (IsFinished()) {
    return;
  }

  ScriptLoadRequest::Cancel();

  mModuleScript = nullptr;
  CancelImports();

  if (mWaitingParentRequest) {
    ChildLoadComplete(false);
  }
}

void ModuleLoadRequest::SetReady() {
  MOZ_ASSERT(!IsFinished());

  // Mark a module as ready to execute. This means that this module and all it
  // dependencies have had their source loaded, parsed as a module and the
  // modules instantiated.

  AssertAllImportsFinished();

  ScriptLoadRequest::SetReady();

  if (mWaitingParentRequest) {
    ChildLoadComplete(true);
  }
}

void ModuleLoadRequest::ModuleLoaded() {
  // A module that was found to be marked as fetching in the module map has now
  // been loaded.

  LOG(("ScriptLoadRequest (%p): Module loaded", this));

  if (IsCanceled()) {
    AssertAllImportsCancelled();
    return;
  }

  MOZ_ASSERT(IsFetching());

  mModuleScript = mLoader->GetFetchedModule(mURI);
  if (IsErrored()) {
    ModuleErrored();
    return;
  }

  mLoader->StartFetchingModuleDependencies(this);
}

void ModuleLoadRequest::LoadFailed() {
  // We failed to load the source text or an error occurred unrelated to the
  // content of the module (e.g. OOM).

  LOG(("ScriptLoadRequest (%p): Module load failed", this));

  if (IsCanceled()) {
    AssertAllImportsCancelled();
    return;
  }

  MOZ_ASSERT(IsFetching());
  MOZ_ASSERT(!mModuleScript);

  Cancel();
  LoadFinished();
}

void ModuleLoadRequest::ModuleErrored() {
  // Parse error, failure to resolve imported modules or error loading import.

  LOG(("ScriptLoadRequest (%p): Module errored", this));

  if (IsCanceled()) {
    return;
  }

  MOZ_ASSERT(!IsFinished());

  CheckModuleDependenciesLoaded();
  MOZ_ASSERT(IsErrored());

  CancelImports();
  if (IsFinished()) {
    // Cancelling an outstanding import will error this request.
    return;
  }

  SetReady();
  LoadFinished();
}

void ModuleLoadRequest::DependenciesLoaded() {
  // The module and all of its dependencies have been successfully fetched and
  // compiled.

  LOG(("ScriptLoadRequest (%p): Module dependencies loaded", this));

  if (IsCanceled()) {
    return;
  }

  MOZ_ASSERT(IsLoadingImports());
  MOZ_ASSERT(!IsErrored());

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

void ModuleLoadRequest::CancelImports() {
  for (size_t i = 0; i < mImports.Length(); i++) {
    mImports[i]->Cancel();
  }
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

inline void ModuleLoadRequest::AssertAllImportsFinished() const {
#ifdef DEBUG
  for (const auto& request : mImports) {
    MOZ_ASSERT(request->IsFinished());
  }
#endif
}

inline void ModuleLoadRequest::AssertAllImportsCancelled() const {
#ifdef DEBUG
  for (const auto& request : mImports) {
    MOZ_ASSERT(request->IsCanceled());
  }
#endif
}

}  // namespace JS::loader

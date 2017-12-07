/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ModuleLoadRequest.h"
#include "ModuleScript.h"
#include "ScriptLoader.h"

namespace mozilla {
namespace dom {

#undef LOG
#define LOG(args) \
  MOZ_LOG(ScriptLoader::gScriptLoaderLog, mozilla::LogLevel::Debug, args)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ModuleLoadRequest)
NS_INTERFACE_MAP_END_INHERITING(ScriptLoadRequest)

NS_IMPL_CYCLE_COLLECTION_INHERITED(ModuleLoadRequest, ScriptLoadRequest,
                                   mBaseURL,
                                   mLoader,
                                   mParent,
                                   mModuleScript,
                                   mImports)

NS_IMPL_ADDREF_INHERITED(ModuleLoadRequest, ScriptLoadRequest)
NS_IMPL_RELEASE_INHERITED(ModuleLoadRequest, ScriptLoadRequest)

ModuleLoadRequest::ModuleLoadRequest(nsIScriptElement* aElement,
                                     ValidJSVersion aValidJSVersion,
                                     CORSMode aCORSMode,
                                     const SRIMetadata& aIntegrity,
                                     ScriptLoader* aLoader)
  : ScriptLoadRequest(ScriptKind::Module,
                      aElement,
                      aValidJSVersion,
                      aCORSMode,
                      aIntegrity),
    mIsTopLevel(true),
    mLoader(aLoader)
{}

void
ModuleLoadRequest::Cancel()
{
  ScriptLoadRequest::Cancel();
  mModuleScript = nullptr;
  mProgress = ScriptLoadRequest::Progress::Ready;
  CancelImports();
  mReady.RejectIfExists(NS_ERROR_DOM_ABORT_ERR, __func__);
}

void
ModuleLoadRequest::CancelImports()
{
  for (size_t i = 0; i < mImports.Length(); i++) {
    mImports[i]->Cancel();
  }
}

void
ModuleLoadRequest::SetReady()
{
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

void
ModuleLoadRequest::ModuleLoaded()
{
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

void
ModuleLoadRequest::ModuleErrored()
{
  LOG(("ScriptLoadRequest (%p): Module errored", this));

  mLoader->CheckModuleDependenciesLoaded(this);
  MOZ_ASSERT(!mModuleScript || mModuleScript->HasParseError());

  CancelImports();
  SetReady();
  LoadFinished();
}

void
ModuleLoadRequest::DependenciesLoaded()
{
  // The module and all of its dependencies have been successfully fetched and
  // compiled.

  LOG(("ScriptLoadRequest (%p): Module dependencies loaded", this));

  MOZ_ASSERT(mModuleScript);

  mLoader->CheckModuleDependenciesLoaded(this);
  SetReady();
  LoadFinished();
}

void
ModuleLoadRequest::LoadFailed()
{
  // We failed to load the source text or an error occurred unrelated to the
  // content of the module (e.g. OOM).

  LOG(("ScriptLoadRequest (%p): Module load failed", this));

  MOZ_ASSERT(!mModuleScript);

  Cancel();
  LoadFinished();
}

void
ModuleLoadRequest::LoadFinished()
{
  mLoader->ProcessLoadedModuleTree(this);

  mLoader = nullptr;
  mParent = nullptr;
}

} // dom namespace
} // mozilla namespace

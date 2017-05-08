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

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ModuleLoadRequest)
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
                                     uint32_t aVersion,
                                     CORSMode aCORSMode,
                                     const SRIMetadata& aIntegrity,
                                     ScriptLoader* aLoader)
  : ScriptLoadRequest(ScriptKind::Module,
                      aElement,
                      aVersion,
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
  for (size_t i = 0; i < mImports.Length(); i++) {
    mImports[i]->Cancel();
  }
  mReady.RejectIfExists(NS_ERROR_FAILURE, __func__);
}

void
ModuleLoadRequest::SetReady()
{
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

  mModuleScript = mLoader->GetFetchedModule(mURI);
  mLoader->StartFetchingModuleDependencies(this);
}

void
ModuleLoadRequest::DependenciesLoaded()
{
  // The module and all of its dependencies have been successfully fetched and
  // compiled.

  if (!mLoader->InstantiateModuleTree(this)) {
    LoadFailed();
    return;
  }

  SetReady();
  mLoader->ProcessLoadedModuleTree(this);
  mLoader = nullptr;
  mParent = nullptr;
}

void
ModuleLoadRequest::LoadFailed()
{
  Cancel();
  mLoader->ProcessLoadedModuleTree(this);
  mLoader = nullptr;
  mParent = nullptr;
}

} // dom namespace
} // mozilla namespace

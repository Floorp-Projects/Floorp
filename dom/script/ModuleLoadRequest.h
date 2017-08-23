/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ModuleLoadRequest_h
#define mozilla_dom_ModuleLoadRequest_h

#include "ScriptLoadRequest.h"
#include "mozilla/MozPromise.h"

namespace mozilla {
namespace dom {

class ModuleScript;
class ScriptLoader;

// A load request for a module, created for every top level module script and
// every module import.  Load request can share an ModuleScript if there are
// multiple imports of the same module.

class ModuleLoadRequest final : public ScriptLoadRequest
{
  ~ModuleLoadRequest() = default;

  ModuleLoadRequest(const ModuleLoadRequest& aOther) = delete;
  ModuleLoadRequest(ModuleLoadRequest&& aOther) = delete;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ModuleLoadRequest, ScriptLoadRequest)

  ModuleLoadRequest(nsIScriptElement* aElement,
                    uint32_t aVersion,
                    CORSMode aCORSMode,
                    const SRIMetadata& aIntegrity,
                    ScriptLoader* aLoader);

  bool IsTopLevel() const
  {
    return mIsTopLevel;
  }

  void SetReady() override;
  void Cancel() override;

  void ModuleLoaded();
  void ModuleErrored();
  void DependenciesLoaded();
  void LoadFailed();

 private:
  void LoadFinished();
  void CancelImports();

 public:
  // Is this a request for a top level module script or an import?
  bool mIsTopLevel;

  // The base URL used for resolving relative module imports.
  nsCOMPtr<nsIURI> mBaseURL;

  // Pointer to the script loader, used to trigger actions when the module load
  // finishes.
  RefPtr<ScriptLoader> mLoader;

  // The importing module, or nullptr for top level module scripts.  Used to
  // implement the ancestor list checked when fetching module dependencies.
  RefPtr<ModuleLoadRequest> mParent;

  // Set to a module script object after a successful load or nullptr on
  // failure.
  RefPtr<ModuleScript> mModuleScript;

  // A promise that is completed on successful load of this module and all of
  // its dependencies, indicating that the module is ready for instantiation and
  // evaluation.
  MozPromiseHolder<GenericPromise> mReady;

  // Array of imported modules.
  nsTArray<RefPtr<ModuleLoadRequest>> mImports;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_ModuleLoadRequest_h

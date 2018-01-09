/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ModuleLoadRequest_h
#define mozilla_dom_ModuleLoadRequest_h

#include "ScriptLoadRequest.h"
#include "nsURIHashKey.h"
#include "mozilla/MozPromise.h"

namespace mozilla {
namespace dom {

class ModuleScript;
class ScriptLoader;

// A reference counted set of URLs we have visited in the process of loading a
// module graph.
class VisitedURLSet : public nsTHashtable<nsURIHashKey>
{
  NS_INLINE_DECL_REFCOUNTING(VisitedURLSet)

private:
  ~VisitedURLSet() = default;
};

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

  // Create a top-level module load request.
  ModuleLoadRequest(nsIURI* aURI,
                    nsIScriptElement* aElement,
                    CORSMode aCORSMode,
                    const SRIMetadata& aIntegrity,
                    nsIURI* aReferrer,
                    mozilla::net::ReferrerPolicy,
                    ScriptLoader* aLoader);

  // Create a module load request for an imported module.
  ModuleLoadRequest(nsIURI* aURI,
                    ModuleLoadRequest* aParent);

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
  const bool mIsTopLevel;

  // The base URL used for resolving relative module imports.
  nsCOMPtr<nsIURI> mBaseURL;

  // Pointer to the script loader, used to trigger actions when the module load
  // finishes.
  RefPtr<ScriptLoader> mLoader;

  // Set to a module script object after a successful load or nullptr on
  // failure.
  RefPtr<ModuleScript> mModuleScript;

  // A promise that is completed on successful load of this module and all of
  // its dependencies, indicating that the module is ready for instantiation and
  // evaluation.
  MozPromiseHolder<GenericPromise> mReady;

  // Array of imported modules.
  nsTArray<RefPtr<ModuleLoadRequest>> mImports;

  // Set of module URLs visited while fetching the module graph this request is
  // part of.
  RefPtr<VisitedURLSet> mVisitedSet;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_ModuleLoadRequest_h

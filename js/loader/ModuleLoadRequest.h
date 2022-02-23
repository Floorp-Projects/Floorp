/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_loader_ModuleLoadRequest_h
#define js_loader_ModuleLoadRequest_h

#include "ScriptLoadRequest.h"
#include "ModuleLoaderBase.h"
#include "mozilla/MozPromise.h"
#include "js/RootingAPI.h"
#include "js/Value.h"
#include "nsURIHashKey.h"
#include "nsTHashtable.h"

namespace JS::loader {

class ModuleScript;
class ModuleLoaderBase;

// A reference counted set of URLs we have visited in the process of loading a
// module graph.
class VisitedURLSet : public nsTHashtable<nsURIHashKey> {
  NS_INLINE_DECL_REFCOUNTING(VisitedURLSet)

 private:
  ~VisitedURLSet() = default;
};

// A load request for a module, created for every top level module script and
// every module import.  Load request can share an ModuleScript if there are
// multiple imports of the same module.

class ModuleLoadRequest final : public ScriptLoadRequest {
  ~ModuleLoadRequest() = default;

  ModuleLoadRequest(const ModuleLoadRequest& aOther) = delete;
  ModuleLoadRequest(ModuleLoadRequest&& aOther) = delete;

 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(ModuleLoadRequest,
                                                         ScriptLoadRequest)
  using SRIMetadata = mozilla::dom::SRIMetadata;

  template <typename T>
  using MozPromiseHolder = mozilla::MozPromiseHolder<T>;
  using GenericPromise = mozilla::GenericPromise;

  ModuleLoadRequest(nsIURI* aURI, ScriptFetchOptions* aFetchOptions,
                    const SRIMetadata& aIntegrity, nsIURI* aReferrer,
                    mozilla::dom::ScriptLoadContext* aContext, bool aIsTopLevel,
                    bool aIsDynamicImport, ModuleLoaderBase* aLoader,
                    VisitedURLSet* aVisitedSet, ModuleLoadRequest* aRootModule);

  static VisitedURLSet* NewVisitedSetForTopLevelImport(nsIURI* aURI);

  bool IsTopLevel() const override { return mIsTopLevel; }

  bool IsDynamicImport() const { return mIsDynamicImport; }

  void SetReady() override;
  void Cancel() override;
  void ClearDynamicImport();

  void ModuleLoaded();
  void ModuleErrored();
  void DependenciesLoaded();
  void LoadFailed();

  ModuleLoadRequest* GetRootModule() {
    if (!mRootModule) {
      return this;
    }
    return mRootModule;
  }

 private:
  void LoadFinished();
  void CancelImports();
  void CheckModuleDependenciesLoaded();

 public:
  // Is this a request for a top level module script or an import?
  const bool mIsTopLevel;

  // Is this the top level request for a dynamic module import?
  const bool mIsDynamicImport;

  // Pointer to the script loader, used to trigger actions when the module load
  // finishes.
  RefPtr<ModuleLoaderBase> mLoader;

  // Pointer to the top level module of this module tree, nullptr if this is
  // a top level module
  RefPtr<ModuleLoadRequest> mRootModule;

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

  // For dynamic imports, the details to pass to FinishDynamicImport.
  JS::Heap<JS::Value> mDynamicReferencingPrivate;
  JS::Heap<JSString*> mDynamicSpecifier;
  JS::Heap<JSObject*> mDynamicPromise;
};

}  // namespace JS::loader

#endif  // js_loader_ModuleLoadRequest_h

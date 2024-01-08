/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_loader_ScriptLoadRequest_h
#define js_loader_ScriptLoadRequest_h

#include "js/RootingAPI.h"
#include "js/SourceText.h"
#include "js/TypeDecls.h"
#include "mozilla/Atomics.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/SRIMetadata.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Maybe.h"
#include "mozilla/PreloaderBase.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/Variant.h"
#include "mozilla/Vector.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIGlobalObject.h"
#include "LoadedScript.h"
#include "ScriptKind.h"
#include "ScriptFetchOptions.h"
#include "nsIScriptElement.h"

class nsICacheInfoChannel;

namespace mozilla::dom {

class ScriptLoadContext;
class WorkerLoadContext;
class WorkletLoadContext;
enum class RequestPriority : uint8_t;

}  // namespace mozilla::dom

namespace mozilla::loader {
class ComponentLoadContext;
}  // namespace mozilla::loader

namespace JS {
namespace loader {

class LoadContextBase;
class ModuleLoadRequest;
class ScriptLoadRequestList;

/*
 * ScriptLoadRequest
 *
 * ScriptLoadRequest is a generic representation of a JavaScript script that
 * will be loaded by a Script/Module loader. This representation is used by the
 * DOM ScriptLoader and will be used by workers and MOZJSComponentLoader.
 *
 * The ScriptLoadRequest contains information about the kind of script (classic
 * or module), the URI, and the ScriptFetchOptions associated with the script.
 * It is responsible for holding the script data once the fetch is complete, or
 * if the request is cached, the bytecode.
 *
 * Relationship to ScriptLoadContext:
 *
 * ScriptLoadRequest and ScriptLoadContexts have a circular pointer.  A
 * ScriptLoadContext augments the loading of a ScriptLoadRequest by providing
 * additional information regarding the loading and evaluation behavior (see
 * the ScriptLoadContext class for details).  In terms of responsibility,
 * the ScriptLoadRequest represents "What" is being loaded, and the
 * ScriptLoadContext represents "How".
 *
 * TODO: see if we can use it in the jsshell script loader. We need to either
 * remove ISUPPORTS or find a way to encorporate that in the jsshell. We would
 * then only have one implementation of the script loader, and it would be
 * tested whenever jsshell tests are run. This would mean finding another way to
 * create ScriptLoadRequest lists.
 *
 */

class ScriptLoadRequest : public nsISupports,
                          private mozilla::LinkedListElement<ScriptLoadRequest>,
                          public LoadedScriptDelegate<ScriptLoadRequest> {
  using super = LinkedListElement<ScriptLoadRequest>;

  // Allow LinkedListElement<ScriptLoadRequest> to cast us to itself as needed.
  friend class mozilla::LinkedListElement<ScriptLoadRequest>;
  friend class ScriptLoadRequestList;

 protected:
  virtual ~ScriptLoadRequest();

 public:
  using SRIMetadata = mozilla::dom::SRIMetadata;
  ScriptLoadRequest(ScriptKind aKind, nsIURI* aURI,
                    mozilla::dom::ReferrerPolicy aReferrerPolicy,
                    ScriptFetchOptions* aFetchOptions,
                    const SRIMetadata& aIntegrity, nsIURI* aReferrer,
                    LoadContextBase* aContext);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ScriptLoadRequest)

  using super::getNext;
  using super::isInList;

  template <typename T, typename D = JS::DeletePolicy<T>>
  using UniquePtr = mozilla::UniquePtr<T, D>;

  bool IsModuleRequest() const { return mKind == ScriptKind::eModule; }
  bool IsImportMapRequest() const { return mKind == ScriptKind::eImportMap; }

  ModuleLoadRequest* AsModuleRequest();
  const ModuleLoadRequest* AsModuleRequest() const;

  virtual bool IsTopLevel() const { return true; };

  virtual void Cancel();

  virtual void SetReady();

  enum class State : uint8_t {
    CheckingCache,
    PendingFetchingError,
    Fetching,
    Compiling,
    LoadingImports,
    Ready,
    Canceled
  };

  // Before any attempt at fetching resources from the cache we should first
  // make sure that the resource does not yet exists in the cache. In which case
  // we might simply alias its LoadedScript. Otherwise a new one would be
  // created.
  bool IsCheckingCache() const { return mState == State::CheckingCache; }

  // Setup and load resources, to fill the LoadedScript and make it usable by
  // the JavaScript engine.
  bool IsFetching() const { return mState == State::Fetching; }
  bool IsCompiling() const { return mState == State::Compiling; }
  bool IsLoadingImports() const { return mState == State::LoadingImports; }
  bool IsCanceled() const { return mState == State::Canceled; }

  bool IsPendingFetchingError() const {
    return mState == State::PendingFetchingError;
  }

  // Return whether the request has been completed, either successfully or
  // otherwise.
  bool IsFinished() const {
    return mState == State::Ready || mState == State::Canceled;
  }

  mozilla::dom::RequestPriority FetchPriority() const {
    return mFetchOptions->mFetchPriority;
  }

  enum mozilla::dom::ReferrerPolicy ReferrerPolicy() const {
    return mReferrerPolicy;
  }

  void UpdateReferrerPolicy(mozilla::dom::ReferrerPolicy aReferrerPolicy) {
    mReferrerPolicy = aReferrerPolicy;
  }

  enum ParserMetadata ParserMetadata() const {
    return mFetchOptions->mParserMetadata;
  }

  const nsString& Nonce() const { return mFetchOptions->mNonce; }

  nsIPrincipal* TriggeringPrincipal() const {
    return mFetchOptions->mTriggeringPrincipal;
  }

  // Convert a CheckingCache ScriptLoadRequest into a Fetching one, by creating
  // a new LoadedScript which is matching the ScriptKind provided when
  // constructing this ScriptLoadRequest.
  void NoCacheEntryFound();

  void SetPendingFetchingError();

  void MarkForBytecodeEncoding(JSScript* aScript);

  bool IsMarkedForBytecodeEncoding() const;

  mozilla::CORSMode CORSMode() const { return mFetchOptions->mCORSMode; }

  void DropBytecodeCacheReferences();

  bool HasLoadContext() const { return mLoadContext; }
  bool HasScriptLoadContext() const;
  bool HasWorkerLoadContext() const;

  mozilla::dom::ScriptLoadContext* GetScriptLoadContext();

  mozilla::loader::ComponentLoadContext* GetComponentLoadContext();

  mozilla::dom::WorkerLoadContext* GetWorkerLoadContext();

  mozilla::dom::WorkletLoadContext* GetWorkletLoadContext();

  const LoadedScript* getLoadedScript() const { return mLoadedScript.get(); }
  LoadedScript* getLoadedScript() { return mLoadedScript.get(); }

  const ScriptKind mKind;  // Whether this is a classic script or a module
                           // script.

  State mState;           // Are we still waiting for a load to complete?
  bool mFetchSourceOnly;  // Request source, not cached bytecode.

  // The referrer policy used for the initial fetch and for fetching any
  // imported modules
  enum mozilla::dom::ReferrerPolicy mReferrerPolicy;
  RefPtr<ScriptFetchOptions> mFetchOptions;
  const SRIMetadata mIntegrity;
  const nsCOMPtr<nsIURI> mReferrer;
  mozilla::Maybe<nsString>
      mSourceMapURL;  // Holds source map url for loaded scripts

  const nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIPrincipal> mOriginPrincipal;

  // Keep the URI's filename alive during off thread parsing.
  // Also used by workers to report on errors while loading, and used by
  // worklets as the file name in compile options.
  nsAutoCString mURL;

  // The base URL used for resolving relative module imports.
  nsCOMPtr<nsIURI> mBaseURL;

  // The loaded script holds the source / bytecode which is loaded.
  //
  // Currently it is used to hold information which are needed by the Debugger.
  // Soon it would be used as a way to dissociate the LoadRequest from the
  // loaded value, such that multiple request referring to the same content
  // would share the same loaded script.
  RefPtr<LoadedScript> mLoadedScript;

  // Holds the top-level JSScript that corresponds to the current source, once
  // it is parsed, and planned to be saved in the bytecode cache.
  //
  // NOTE: This field is not used for ModuleLoadRequest.
  //       See ModuleLoadRequest::mIsMarkedForBytecodeEncoding.
  JS::Heap<JSScript*> mScriptForBytecodeEncoding;

  // Holds the Cache information, which is used to register the bytecode
  // on the cache entry, such that we can load it the next time.
  nsCOMPtr<nsICacheInfoChannel> mCacheInfo;

  // LoadContext for augmenting the load depending on the loading
  // context (DOM, Worker, etc.)
  RefPtr<LoadContextBase> mLoadContext;

  // EarlyHintRegistrar id to connect the http channel back to the preload, with
  // a default of value of 0 indicating that this request is not an early hints
  // preload.
  uint64_t mEarlyHintPreloaderId;
};

class ScriptLoadRequestList : private mozilla::LinkedList<ScriptLoadRequest> {
  using super = mozilla::LinkedList<ScriptLoadRequest>;

 public:
  ~ScriptLoadRequestList();

  void CancelRequestsAndClear();

#ifdef DEBUG
  bool Contains(ScriptLoadRequest* aElem) const;
#endif  // DEBUG

  using super::getFirst;
  using super::isEmpty;

  void AppendElement(ScriptLoadRequest* aElem) {
    MOZ_ASSERT(!aElem->isInList());
    NS_ADDREF(aElem);
    insertBack(aElem);
  }

  already_AddRefed<ScriptLoadRequest> Steal(ScriptLoadRequest* aElem) {
    aElem->removeFrom(*this);
    return dont_AddRef(aElem);
  }

  already_AddRefed<ScriptLoadRequest> StealFirst() {
    MOZ_ASSERT(!isEmpty());
    return Steal(getFirst());
  }

  void Remove(ScriptLoadRequest* aElem) {
    aElem->removeFrom(*this);
    NS_RELEASE(aElem);
  }
};

inline void ImplCycleCollectionUnlink(ScriptLoadRequestList& aField) {
  while (!aField.isEmpty()) {
    RefPtr<ScriptLoadRequest> first = aField.StealFirst();
  }
}

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback,
    ScriptLoadRequestList& aField, const char* aName, uint32_t aFlags) {
  for (ScriptLoadRequest* request = aField.getFirst(); request;
       request = request->getNext()) {
    CycleCollectionNoteChild(aCallback, request, aName, aFlags);
  }
}

}  // namespace loader
}  // namespace JS

#endif  // js_loader_ScriptLoadRequest_h

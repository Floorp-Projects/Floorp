/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ScriptLoader_h
#define mozilla_dom_ScriptLoader_h

#include "nsCOMPtr.h"
#include "nsRefPtrHashtable.h"
#include "mozilla/Encoding.h"
#include "nsIScriptElement.h"
#include "nsCOMArray.h"
#include "nsCycleCollectionParticipant.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "nsICacheInfoChannel.h"
#include "nsIDocument.h"
#include "nsIIncrementalStreamLoader.h"
#include "nsURIHashKey.h"
#include "mozilla/CORSMode.h"
#include "mozilla/dom/ScriptLoadRequest.h"
#include "mozilla/dom/SRIMetadata.h"
#include "mozilla/dom/SRICheck.h"
#include "mozilla/MozPromise.h"
#include "mozilla/net/ReferrerPolicy.h"
#include "mozilla/Vector.h"

class nsIURI;

namespace JS {
  class SourceBufferHolder;
} // namespace JS

namespace mozilla {
namespace dom {

class AutoJSAPI;
class ModuleLoadRequest;
class ModuleScript;
class ScriptLoadHandler;
class ScriptRequestProcessor;

//////////////////////////////////////////////////////////////
// Script loader implementation
//////////////////////////////////////////////////////////////

class ScriptLoader final : public nsISupports
{
  class MOZ_STACK_CLASS AutoCurrentScriptUpdater
  {
  public:
    AutoCurrentScriptUpdater(ScriptLoader* aScriptLoader,
                             nsIScriptElement* aCurrentScript)
      : mOldScript(aScriptLoader->mCurrentScript)
      , mScriptLoader(aScriptLoader)
    {
      mScriptLoader->mCurrentScript = aCurrentScript;
    }

    ~AutoCurrentScriptUpdater()
    {
      mScriptLoader->mCurrentScript.swap(mOldScript);
    }

  private:
    nsCOMPtr<nsIScriptElement> mOldScript;
    ScriptLoader* mScriptLoader;
  };

  friend class ModuleLoadRequest;
  friend class ScriptRequestProcessor;
  friend class ScriptLoadHandler;
  friend class AutoCurrentScriptUpdater;

public:
  explicit ScriptLoader(nsIDocument* aDocument);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(ScriptLoader)

  /**
   * The loader maintains a weak reference to the document with
   * which it is initialized. This call forces the reference to
   * be dropped.
   */
  void DropDocumentReference()
  {
    mDocument = nullptr;
  }

  /**
   * Add an observer for all scripts loaded through this loader.
   *
   * @param aObserver observer for all script processing.
   */
  nsresult AddObserver(nsIScriptLoaderObserver* aObserver)
  {
    return mObservers.AppendObject(aObserver) ? NS_OK :
      NS_ERROR_OUT_OF_MEMORY;
  }

  /**
   * Remove an observer.
   *
   * @param aObserver observer to be removed
   */
  void RemoveObserver(nsIScriptLoaderObserver* aObserver)
  {
    mObservers.RemoveObject(aObserver);
  }

  /**
   * Process a script element. This will include both loading the
   * source of the element if it is not inline and evaluating
   * the script itself.
   *
   * If the script is an inline script that can be executed immediately
   * (i.e. there are no other scripts pending) then ScriptAvailable
   * and ScriptEvaluated will be called before the function returns.
   *
   * If true is returned the script could not be executed immediately.
   * In this case ScriptAvailable is guaranteed to be called at a later
   * point (as well as possibly ScriptEvaluated).
   *
   * @param aElement The element representing the script to be loaded and
   *        evaluated.
   */
  bool ProcessScriptElement(nsIScriptElement* aElement);

  /**
   * Gets the currently executing script. This is useful if you want to
   * generate a unique key based on the currently executing script.
   */
  nsIScriptElement* GetCurrentScript()
  {
    return mCurrentScript;
  }

  nsIScriptElement* GetCurrentParserInsertedScript()
  {
    return mCurrentParserInsertedScript;
  }

  /**
   * Whether the loader is enabled or not.
   * When disabled, processing of new script elements is disabled.
   * Any call to ProcessScriptElement() will return false. Note that
   * this DOES NOT disable currently loading or executing scripts.
   */
  bool GetEnabled()
  {
    return mEnabled;
  }

  void SetEnabled(bool aEnabled)
  {
    if (!mEnabled && aEnabled) {
      ProcessPendingRequestsAsync();
    }
    mEnabled = aEnabled;
  }

  /**
   * Add/remove a blocker for parser-blocking scripts (and XSLT
   * scripts). Blockers will stop such scripts from executing, but not from
   * loading.
   */
  void AddParserBlockingScriptExecutionBlocker()
  {
    ++mParserBlockingBlockerCount;
  }

  void RemoveParserBlockingScriptExecutionBlocker()
  {
    if (!--mParserBlockingBlockerCount && ReadyToExecuteScripts()) {
      ProcessPendingRequestsAsync();
    }
  }

  /**
   * Add/remove a blocker for execution of all scripts.  Blockers will stop
   * scripts from executing, but not from loading.
   */
  void AddExecuteBlocker()
  {
    ++mBlockerCount;
  }

  void RemoveExecuteBlocker()
  {
    MOZ_ASSERT(mBlockerCount);
    if (!--mBlockerCount) {
      ProcessPendingRequestsAsync();
    }
  }

  /**
   * Convert the given buffer to a UTF-16 string.
   * @param aChannel     Channel corresponding to the data. May be null.
   * @param aData        The data to convert
   * @param aLength      Length of the data
   * @param aHintCharset Hint for the character set (e.g., from a charset
   *                     attribute). May be the empty string.
   * @param aDocument    Document which the data is loaded for. Must not be
   *                     null.
   * @param aBufOut      [out] char16_t array allocated by ConvertToUTF16 and
   *                     containing data converted to unicode.  Caller must
   *                     js_free() this data when no longer needed.
   * @param aLengthOut   [out] Length of array returned in aBufOut in number
   *                     of char16_t code units.
   */
  static nsresult ConvertToUTF16(nsIChannel* aChannel, const uint8_t* aData,
                                 uint32_t aLength,
                                 const nsAString& aHintCharset,
                                 nsIDocument* aDocument,
                                 char16_t*& aBufOut, size_t& aLengthOut);

  static inline nsresult
  ConvertToUTF16(nsIChannel* aChannel, const uint8_t* aData,
                 uint32_t aLength, const nsAString& aHintCharset,
                 nsIDocument* aDocument,
                 JS::UniqueTwoByteChars& aBufOut, size_t& aLengthOut)
  {
    char16_t* bufOut;
    nsresult rv = ConvertToUTF16(aChannel, aData, aLength, aHintCharset,
                                 aDocument, bufOut, aLengthOut);
    if (NS_SUCCEEDED(rv)) {
      aBufOut.reset(bufOut);
    }
    return rv;
  };

  /**
   * Handle the completion of a stream.  This is called by the
   * ScriptLoadHandler object which observes the IncrementalStreamLoader
   * loading the script. The streamed content is expected to be stored on the
   * aRequest argument.
   */
  nsresult OnStreamComplete(nsIIncrementalStreamLoader* aLoader,
                            ScriptLoadRequest* aRequest,
                            nsresult aChannelStatus,
                            nsresult aSRIStatus,
                            mozilla::dom::SRICheckDataVerifier* aSRIDataVerifier);

  /**
   * Returns wether any request is queued, and not executed yet.
   */
  bool HasPendingRequests();

  /**
   * Processes any pending requests that are ready for processing.
   */
  void ProcessPendingRequests();

  /**
   * Starts deferring deferred scripts and puts them in the mDeferredRequests
   * queue instead.
   */
  void BeginDeferringScripts()
  {
    mDeferEnabled = true;
    if (mDocument) {
      mDocument->BlockOnload();
    }
  }

  /**
   * Notifies the script loader that parsing is done.  If aTerminated is true,
   * this will drop any pending scripts that haven't run yet.  Otherwise, it
   * will stops deferring scripts and immediately processes the
   * mDeferredRequests queue.
   *
   * WARNING: This function will synchronously execute content scripts, so be
   * prepared that the world might change around you.
   */
  void ParsingComplete(bool aTerminated);

  /**
   * Returns the number of pending scripts, deferred or not.
   */
  uint32_t HasPendingOrCurrentScripts()
  {
    return mCurrentScript || mParserBlockingRequest;
  }

  /**
   * Adds aURI to the preload list and starts loading it.
   *
   * @param aURI The URI of the external script.
   * @param aCharset The charset parameter for the script.
   * @param aType The type parameter for the script.
   * @param aCrossOrigin The crossorigin attribute for the script.
   *                     Void if not present.
   * @param aIntegrity The expect hash url, if avail, of the request
   * @param aScriptFromHead Whether or not the script was a child of head
   */
  virtual void PreloadURI(nsIURI* aURI, const nsAString& aCharset,
                          const nsAString& aType,
                          const nsAString& aCrossOrigin,
                          const nsAString& aIntegrity,
                          bool aScriptFromHead, bool aAsync, bool aDefer,
                          const mozilla::net::ReferrerPolicy aReferrerPolicy);

  /**
   * Process a request that was deferred so that the script could be compiled
   * off thread.
   */
  nsresult ProcessOffThreadRequest(ScriptLoadRequest* aRequest);

  bool AddPendingChildLoader(ScriptLoader* aChild)
  {
    return mPendingChildLoaders.AppendElement(aChild) != nullptr;
  }

  mozilla::dom::DocGroup* GetDocGroup() const
  {
    return mDocument->GetDocGroup();
  }

  /**
   * Register the fact that we saw the load event, and that we need to save the
   * bytecode at the next loop cycle unless new scripts are waiting in the
   * pipeline.
   */
  void LoadEventFired();

  /**
   * Destroy and prevent the ScriptLoader or the ScriptLoadRequests from owning
   * any references to the JSScript or to the Request which might be used for
   * caching the encoded bytecode.
   */
  void Destroy()
  {
    GiveUpBytecodeEncoding();
  }

private:
  virtual ~ScriptLoader();

  ScriptLoadRequest* CreateLoadRequest(ScriptKind aKind,
                                       nsIScriptElement* aElement,
                                       ValidJSVersion aValidJSVersion,
                                       mozilla::CORSMode aCORSMode,
                                       const mozilla::dom::SRIMetadata& aIntegrity);

  /**
   * Unblocks the creator parser of the parser-blocking scripts.
   */
  void UnblockParser(ScriptLoadRequest* aParserBlockingRequest);

  /**
   * Asynchronously resumes the creator parser of the parser-blocking scripts.
   */
  void ContinueParserAsync(ScriptLoadRequest* aParserBlockingRequest);


  /**
   * Helper function to check the content policy for a given request.
   */
  static nsresult CheckContentPolicy(nsIDocument* aDocument,
                                     nsISupports* aContext,
                                     nsIURI* aURI,
                                     const nsAString& aType,
                                     bool aIsPreLoad);

  /**
   * Start a load for aRequest's URI.
   */
  nsresult StartLoad(ScriptLoadRequest* aRequest);

  /**
   * Abort the current stream, and re-start with a new load request from scratch
   * without requesting any alternate data. Returns NS_BINDING_RETARGETED on
   * success, as this error code is used to abort the input stream.
   */
  nsresult RestartLoad(ScriptLoadRequest* aRequest);

  /**
   * Process any pending requests asynchronously (i.e. off an event) if there
   * are any. Note that this is a no-op if there aren't any currently pending
   * requests.
   *
   * This function is virtual to allow cross-library calls to SetEnabled()
   */
  virtual void ProcessPendingRequestsAsync();

  /**
   * If true, the loader is ready to execute parser-blocking scripts, and so are
   * all its ancestors.  If the loader itself is ready but some ancestor is not,
   * this function will add an execute blocker and ask the ancestor to remove it
   * once it becomes ready.
   */
  bool ReadyToExecuteParserBlockingScripts();

  /**
   * Return whether just this loader is ready to execute parser-blocking
   * scripts.
   */
  bool SelfReadyToExecuteParserBlockingScripts()
  {
    return ReadyToExecuteScripts() && !mParserBlockingBlockerCount;
  }

  /**
   * Return whether this loader is ready to execute scripts in general.
   */
  bool ReadyToExecuteScripts()
  {
    return mEnabled && !mBlockerCount;
  }

  nsresult AttemptAsyncScriptCompile(ScriptLoadRequest* aRequest);
  nsresult ProcessRequest(ScriptLoadRequest* aRequest);
  nsresult CompileOffThreadOrProcessRequest(ScriptLoadRequest* aRequest);
  void FireScriptAvailable(nsresult aResult,
                           ScriptLoadRequest* aRequest);
  void FireScriptEvaluated(nsresult aResult,
                           ScriptLoadRequest* aRequest);
  nsresult EvaluateScript(ScriptLoadRequest* aRequest);

  /**
   * Queue the current script load request to be saved, when the page
   * initialization ends. The page initialization end is defined as being the
   * time when the load event got received, and when no more scripts are waiting
   * to be executed.
   */
  void RegisterForBytecodeEncoding(ScriptLoadRequest* aRequest);

  /**
   * Check if all conditions are met, i-e that the onLoad event fired and that
   * no more script have to be processed.  If all conditions are met, queue an
   * event to encode all the bytecode and save them on the cache.
   */
  void MaybeTriggerBytecodeEncoding();

  /**
   * Iterate over all script load request and save the bytecode of executed
   * functions on the cache provided by the channel.
   */
  void EncodeBytecode();
  void EncodeRequestBytecode(JSContext* aCx, ScriptLoadRequest* aRequest);

  void GiveUpBytecodeEncoding();

  already_AddRefed<nsIScriptGlobalObject> GetScriptGlobalObject();
  nsresult FillCompileOptionsForRequest(const mozilla::dom::AutoJSAPI& jsapi,
                                        ScriptLoadRequest* aRequest,
                                        JS::Handle<JSObject*> aScopeChain,
                                        JS::CompileOptions* aOptions);

  uint32_t NumberOfProcessors();
  nsresult PrepareLoadedRequest(ScriptLoadRequest* aRequest,
                                nsIIncrementalStreamLoader* aLoader,
                                nsresult aStatus);

  void AddDeferRequest(ScriptLoadRequest* aRequest);
  bool MaybeRemovedDeferRequests();

  void MaybeMoveToLoadedList(ScriptLoadRequest* aRequest);

  JS::SourceBufferHolder GetScriptSource(ScriptLoadRequest* aRequest,
                                         nsAutoString& inlineData);

  bool ModuleScriptsEnabled();

  void SetModuleFetchStarted(ModuleLoadRequest *aRequest);
  void SetModuleFetchFinishedAndResumeWaitingRequests(ModuleLoadRequest* aRequest,
                                                      nsresult aResult);

  bool IsFetchingModule(ModuleLoadRequest* aRequest) const;

  bool ModuleMapContainsURL(nsIURI* aURL) const;
  RefPtr<mozilla::GenericPromise> WaitForModuleFetch(nsIURI* aURL);
  ModuleScript* GetFetchedModule(nsIURI* aURL) const;

  friend bool
  HostResolveImportedModule(JSContext* aCx, unsigned argc, JS::Value* vp);

  // Returns wether we should save the bytecode of this script after the
  // execution of the script.
  static bool
  ShouldCacheBytecode(ScriptLoadRequest* aRequest);

  nsresult CreateModuleScript(ModuleLoadRequest* aRequest);
  nsresult ProcessFetchedModuleSource(ModuleLoadRequest* aRequest);
  void CheckModuleDependenciesLoaded(ModuleLoadRequest* aRequest);
  void ProcessLoadedModuleTree(ModuleLoadRequest* aRequest);
  bool InstantiateModuleTree(ModuleLoadRequest* aRequest);
  JS::Value FindFirstParseError(ModuleLoadRequest* aRequest);
  void StartFetchingModuleDependencies(ModuleLoadRequest* aRequest);

  RefPtr<mozilla::GenericPromise>
  StartFetchingModuleAndDependencies(ModuleLoadRequest* aRequest, nsIURI* aURI);

  nsIDocument* mDocument;                   // [WEAK]
  nsCOMArray<nsIScriptLoaderObserver> mObservers;
  ScriptLoadRequestList mNonAsyncExternalScriptInsertedRequests;
  // mLoadingAsyncRequests holds async requests while they're loading; when they
  // have been loaded they are moved to mLoadedAsyncRequests.
  ScriptLoadRequestList mLoadingAsyncRequests;
  ScriptLoadRequestList mLoadedAsyncRequests;
  ScriptLoadRequestList mDeferRequests;
  ScriptLoadRequestList mXSLTRequests;
  RefPtr<ScriptLoadRequest> mParserBlockingRequest;

  // List of script load request that are holding a buffer which has to be saved
  // on the cache.
  ScriptLoadRequestList mBytecodeEncodingQueue;

  // In mRequests, the additional information here is stored by the element.
  struct PreloadInfo
  {
    RefPtr<ScriptLoadRequest> mRequest;
    nsString mCharset;
  };

  friend void ImplCycleCollectionUnlink(ScriptLoader::PreloadInfo& aField);
  friend void ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                                          ScriptLoader::PreloadInfo& aField,
                                          const char* aName, uint32_t aFlags);

  struct PreloadRequestComparator
  {
    bool Equals(const PreloadInfo& aPi, ScriptLoadRequest* const& aRequest) const
    {
      return aRequest == aPi.mRequest;
    }
  };

  struct PreloadURIComparator
  {
    bool Equals(const PreloadInfo& aPi, nsIURI* const &aURI) const;
  };

  nsTArray<PreloadInfo> mPreloads;

  nsCOMPtr<nsIScriptElement> mCurrentScript;
  nsCOMPtr<nsIScriptElement> mCurrentParserInsertedScript;
  nsTArray< RefPtr<ScriptLoader> > mPendingChildLoaders;
  uint32_t mParserBlockingBlockerCount;
  uint32_t mBlockerCount;
  uint32_t mNumberOfProcessors;
  bool mEnabled;
  bool mDeferEnabled;
  bool mDocumentParsingDone;
  bool mBlockingDOMContentLoaded;
  bool mLoadEventFired;
  bool mGiveUpEncoding;

  // Module map
  nsRefPtrHashtable<nsURIHashKey, mozilla::GenericPromise::Private> mFetchingModules;
  nsRefPtrHashtable<nsURIHashKey, ModuleScript> mFetchedModules;

  nsCOMPtr<nsIConsoleReportCollector> mReporter;

  // Logging
  static LazyLogModule gCspPRLog;
  static LazyLogModule gScriptLoaderLog;
};

class nsAutoScriptLoaderDisabler
{
public:
  explicit nsAutoScriptLoaderDisabler(nsIDocument* aDoc)
  {
    mLoader = aDoc->ScriptLoader();
    mWasEnabled = mLoader->GetEnabled();
    if (mWasEnabled) {
      mLoader->SetEnabled(false);
    }
  }

  ~nsAutoScriptLoaderDisabler()
  {
    if (mWasEnabled) {
      mLoader->SetEnabled(true);
    }
  }

  bool mWasEnabled;
  RefPtr<ScriptLoader> mLoader;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ScriptLoader_h

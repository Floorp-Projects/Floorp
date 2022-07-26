/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ScriptLoader_h
#define mozilla_dom_ScriptLoader_h

#include "js/TypeDecls.h"
#include "js/loader/LoadedScript.h"
#include "js/loader/ScriptKind.h"
#include "js/loader/ScriptLoadRequest.h"
#include "mozilla/dom/ScriptLoadContext.h"
#include "nsCOMPtr.h"
#include "nsRefPtrHashtable.h"
#include "nsIScriptElement.h"
#include "nsCOMArray.h"
#include "nsCycleCollectionParticipant.h"
#include "nsTArray.h"
#include "nsILoadInfo.h"  // nsSecurityFlags
#include "nsINode.h"
#include "nsIObserver.h"
#include "nsIScriptLoaderObserver.h"
#include "nsURIHashKey.h"
#include "mozilla/CORSMode.h"
#include "mozilla/dom/JSExecutionContext.h"  // JSExecutionContext
#include "ModuleLoader.h"
#include "mozilla/MaybeOneOf.h"
#include "mozilla/MozPromise.h"

class nsCycleCollectionTraversalCallback;
class nsIChannel;
class nsIConsoleReportCollector;
class nsIContent;
class nsIIncrementalStreamLoader;
class nsIPrincipal;
class nsIScriptGlobalObject;
class nsIURI;

namespace JS {

class CompileOptions;

template <typename UnitT>
class SourceText;

namespace loader {

class LoadedScript;
class ScriptLoaderInterface;
class ModuleLoadRequest;
class ModuleScript;
class ScriptLoadRequest;
class ScriptLoadRequestList;

}  // namespace loader
}  // namespace JS

namespace mozilla {

class LazyLogModule;
union Utf8Unit;

namespace dom {

class AutoJSAPI;
class DocGroup;
class Document;
class ModuleLoader;
class SRICheckDataVerifier;
class SRIMetadata;
class ScriptLoadHandler;
class ScriptLoadContext;
class ScriptLoader;
class ScriptRequestProcessor;

enum class ReferrerPolicy : uint8_t;

class AsyncCompileShutdownObserver final : public nsIObserver {
  ~AsyncCompileShutdownObserver() { Unregister(); }

 public:
  explicit AsyncCompileShutdownObserver(ScriptLoader* aLoader)
      : mScriptLoader(aLoader) {}

  void OnShutdown();
  void Unregister();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

 private:
  // Defined during registration in ScriptLoader constructor, and
  // cleared during destructor, ScriptLoader::Destroy() or Shutdown.
  ScriptLoader* mScriptLoader;
};

//////////////////////////////////////////////////////////////
// Script loader implementation
//////////////////////////////////////////////////////////////

class ScriptLoader final : public JS::loader::ScriptLoaderInterface {
  class MOZ_STACK_CLASS AutoCurrentScriptUpdater {
   public:
    AutoCurrentScriptUpdater(ScriptLoader* aScriptLoader,
                             nsIScriptElement* aCurrentScript)
        : mOldScript(aScriptLoader->mCurrentScript),
          mScriptLoader(aScriptLoader) {
      nsCOMPtr<nsINode> node = do_QueryInterface(aCurrentScript);
      mScriptLoader->mCurrentScript =
          node && !node->IsInShadowTree() ? aCurrentScript : nullptr;
    }

    ~AutoCurrentScriptUpdater() {
      mScriptLoader->mCurrentScript.swap(mOldScript);
    }

   private:
    nsCOMPtr<nsIScriptElement> mOldScript;
    ScriptLoader* mScriptLoader;
  };

  friend class JS::loader::ModuleLoadRequest;
  friend class ScriptRequestProcessor;
  friend class ModuleLoader;
  friend class ScriptLoadHandler;
  friend class AutoCurrentScriptUpdater;

 public:
  explicit ScriptLoader(Document* aDocument);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(ScriptLoader)

  /**
   * Called when the document that owns this script loader changes global. The
   * argument is null when the document is detached from a window.
   */
  void SetGlobalObject(nsIGlobalObject* aGlobalObject);

  /**
   * The loader maintains a weak reference to the document with
   * which it is initialized. This call forces the reference to
   * be dropped.
   */
  void DropDocumentReference() { mDocument = nullptr; }

  /**
   * Add an observer for all scripts loaded through this loader.
   *
   * @param aObserver observer for all script processing.
   */
  nsresult AddObserver(nsIScriptLoaderObserver* aObserver) {
    return mObservers.AppendObject(aObserver) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
  }

  /**
   * Remove an observer.
   *
   * @param aObserver observer to be removed
   */
  void RemoveObserver(nsIScriptLoaderObserver* aObserver) {
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
  nsIScriptElement* GetCurrentScript() { return mCurrentScript; }

  nsIScriptElement* GetCurrentParserInsertedScript() {
    return mCurrentParserInsertedScript;
  }

  /**
   * Whether the loader is enabled or not.
   * When disabled, processing of new script elements is disabled.
   * Any call to ProcessScriptElement() will return false. Note that
   * this DOES NOT disable currently loading or executing scripts.
   */
  bool GetEnabled() { return mEnabled; }

  void SetEnabled(bool aEnabled) {
    if (!mEnabled && aEnabled) {
      ProcessPendingRequestsAsync();
    }
    mEnabled = aEnabled;
  }

  ModuleLoader* GetModuleLoader() { return mModuleLoader; }

  void RegisterContentScriptModuleLoader(ModuleLoader* aLoader);
  void RegisterShadowRealmModuleLoader(ModuleLoader* aLoader);

  /**
   *  Check whether to speculatively OMT parse scripts as soon as
   *  they are fetched, even if not a parser blocking request.
   *  Controlled by
   *  dom.script_loader.external_scripts.speculative_omt_parse_enabled
   */
  bool SpeculativeOMTParsingEnabled() const {
    return mSpeculativeOMTParsingEnabled;
  }

  /**
   * Add/remove a blocker for parser-blocking scripts (and XSLT
   * scripts). Blockers will stop such scripts from executing, but not from
   * loading.
   */
  void AddParserBlockingScriptExecutionBlocker() {
    ++mParserBlockingBlockerCount;
  }

  void RemoveParserBlockingScriptExecutionBlocker() {
    if (!--mParserBlockingBlockerCount && ReadyToExecuteScripts()) {
      ProcessPendingRequestsAsync();
    }
  }

  /**
   * Add/remove a blocker for execution of all scripts.  Blockers will stop
   * scripts from executing, but not from loading.
   */
  void AddExecuteBlocker() { ++mBlockerCount; }

  void RemoveExecuteBlocker() {
    MOZ_ASSERT(mBlockerCount);
    if (!--mBlockerCount) {
      ProcessPendingRequestsAsync();
    }
  }

  /**
   * Convert the given buffer to a UTF-16 string.  If the buffer begins with a
   * BOM, it is interpreted as that encoding; otherwise the first of |aChannel|,
   * |aHintCharset|, or |aDocument| that provides a recognized encoding is used,
   * or Windows-1252 if none of them do.
   *
   * Encoding errors in the buffer are converted to replacement characters, so
   * allocation failure is the only way this function can fail.
   *
   * @param aChannel     Channel corresponding to the data. May be null.
   * @param aData        The data to convert
   * @param aLength      Length of the data
   * @param aHintCharset Character set hint (e.g., from a charset attribute).
   * @param aDocument    Document which the data is loaded for. May be null.
   * @param aBufOut      [out] fresh char16_t array containing data converted to
   *                     Unicode.  Caller must js_free() this data when finished
   *                     with it.
   * @param aLengthOut   [out] Length of array returned in aBufOut in number
   *                     of char16_t code units.
   */
  static nsresult ConvertToUTF16(nsIChannel* aChannel, const uint8_t* aData,
                                 uint32_t aLength,
                                 const nsAString& aHintCharset,
                                 Document* aDocument, char16_t*& aBufOut,
                                 size_t& aLengthOut);

  static inline nsresult ConvertToUTF16(nsIChannel* aChannel,
                                        const uint8_t* aData, uint32_t aLength,
                                        const nsAString& aHintCharset,
                                        Document* aDocument,
                                        JS::UniqueTwoByteChars& aBufOut,
                                        size_t& aLengthOut) {
    char16_t* bufOut;
    nsresult rv = ConvertToUTF16(aChannel, aData, aLength, aHintCharset,
                                 aDocument, bufOut, aLengthOut);
    if (NS_SUCCEEDED(rv)) {
      aBufOut.reset(bufOut);
    }
    return rv;
  };

  /**
   * Convert the given buffer to a UTF-8 string.  If the buffer begins with a
   * BOM, it is interpreted as that encoding; otherwise the first of |aChannel|,
   * |aHintCharset|, or |aDocument| that provides a recognized encoding is used,
   * or Windows-1252 if none of them do.
   *
   * Encoding errors in the buffer are converted to replacement characters, so
   * allocation failure is the only way this function can fail.
   *
   * @param aChannel     Channel corresponding to the data. May be null.
   * @param aData        The data to convert
   * @param aLength      Length of the data
   * @param aHintCharset Character set hint (e.g., from a charset attribute).
   * @param aDocument    Document which the data is loaded for. May be null.
   * @param aBufOut      [out] fresh Utf8Unit array containing data converted to
   *                     Unicode.  Caller must js_free() this data when finished
   *                     with it.
   * @param aLengthOut   [out] Length of array returned in aBufOut in UTF-8 code
   *                     units (i.e. in bytes).
   */
  static nsresult ConvertToUTF8(nsIChannel* aChannel, const uint8_t* aData,
                                uint32_t aLength, const nsAString& aHintCharset,
                                Document* aDocument, Utf8Unit*& aBufOut,
                                size_t& aLengthOut);

  /**
   * Handle the completion of a stream.  This is called by the
   * ScriptLoadHandler object which observes the IncrementalStreamLoader
   * loading the script. The streamed content is expected to be stored on the
   * aRequest argument.
   */
  nsresult OnStreamComplete(nsIIncrementalStreamLoader* aLoader,
                            ScriptLoadRequest* aRequest,
                            nsresult aChannelStatus, nsresult aSRIStatus,
                            SRICheckDataVerifier* aSRIDataVerifier);

  /**
   * Returns wether any request is queued, and not executed yet.
   */
  bool HasPendingRequests() const;

  /**
   * Returns wether there are any dynamic module import requests pending.
   */
  bool HasPendingDynamicImports() const;

  /**
   * Processes any pending requests that are ready for processing.
   */
  void ProcessPendingRequests();

  /**
   * Starts deferring deferred scripts and puts them in the mDeferredRequests
   * queue instead.
   */
  void BeginDeferringScripts();

  /**
   * Notifies the script loader that parsing is done.  If aTerminated is true,
   * this will drop any pending scripts that haven't run yet, otherwise it will
   * do nothing.
   */
  void ParsingComplete(bool aTerminated);

  /**
   * Notifies the script loader that the checkpoint to begin execution of defer
   * scripts has been reached. This is either the end of of the document parse
   * or the end of loading of parser-inserted stylesheets, whatever happens
   * last.
   *
   * Otherwise, it will stop deferring scripts and immediately processes the
   * mDeferredRequests queue.
   *
   * WARNING: This function will synchronously execute content scripts, so be
   * prepared that the world might change around you.
   */
  void DeferCheckpointReached();

  /**
   * Returns the number of pending scripts, deferred or not.
   */
  uint32_t HasPendingOrCurrentScripts() {
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
                          const nsAString& aType, const nsAString& aCrossOrigin,
                          const nsAString& aIntegrity, bool aScriptFromHead,
                          bool aAsync, bool aDefer, bool aNoModule,
                          bool aLinkPreload,
                          const ReferrerPolicy aReferrerPolicy);

  /**
   * Process a request that was deferred so that the script could be compiled
   * off thread.
   */
  nsresult ProcessOffThreadRequest(ScriptLoadRequest* aRequest);

  bool AddPendingChildLoader(ScriptLoader* aChild) {
    // XXX(Bug 1631371) Check if this should use a fallible operation as it
    // pretended earlier. Else, change the return type to void.
    mPendingChildLoaders.AppendElement(aChild);
    return true;
  }

  mozilla::dom::DocGroup* GetDocGroup() const;

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
  void Destroy();

  /*
   * Get the currently active script. This is used as the initiating script when
   * executing timeout handler scripts.
   */
  static JS::loader::LoadedScript* GetActiveScript(JSContext* aCx);

  Document* GetDocument() const { return mDocument; }

  nsIURI* GetBaseURI() const override;

 private:
  ~ScriptLoader();

  already_AddRefed<ScriptLoadRequest> CreateLoadRequest(
      ScriptKind aKind, nsIURI* aURI, nsIScriptElement* aElement,
      nsIPrincipal* aTriggeringPrincipal, mozilla::CORSMode aCORSMode,
      const SRIMetadata& aIntegrity, ReferrerPolicy aReferrerPolicy);

  /**
   * Unblocks the creator parser of the parser-blocking scripts.
   */
  void UnblockParser(ScriptLoadRequest* aParserBlockingRequest);

  /**
   * Asynchronously resumes the creator parser of the parser-blocking scripts.
   */
  void ContinueParserAsync(ScriptLoadRequest* aParserBlockingRequest);

  bool ProcessExternalScript(nsIScriptElement* aElement, ScriptKind aScriptKind,
                             const nsAutoString& aTypeAttr,
                             nsIContent* aScriptContent);

  bool ProcessInlineScript(nsIScriptElement* aElement, ScriptKind aScriptKind);

  JS::loader::ScriptLoadRequest* LookupPreloadRequest(
      nsIScriptElement* aElement, ScriptKind aScriptKind,
      const SRIMetadata& aSRIMetadata);

  void GetSRIMetadata(const nsAString& aIntegrityAttr,
                      SRIMetadata* aMetadataOut);

  /**
   * Given a script element, get the referrer policy should be applied to load
   * requests.
   */
  ReferrerPolicy GetReferrerPolicy(nsIScriptElement* aElement);

  /**
   * Helper function to check the content policy for a given request.
   */
  static nsresult CheckContentPolicy(Document* aDocument, nsISupports* aContext,
                                     const nsAString& aType,
                                     ScriptLoadRequest* aRequest);

  /**
   * Helper function to determine whether an about: page loads a chrome: URI.
   * Please note that this function only returns true if:
   *   * the about: page uses a ContentPrincipal with scheme about:
   *   * the about: page is not linkable from content
   *     (e.g. the function will return false for about:blank or about:srcdoc)
   */
  static bool IsAboutPageLoadingChromeURI(ScriptLoadRequest* aRequest,
                                          Document* aDocument);

  /**
   * Start a load for aRequest's URI.
   */
  nsresult StartLoad(ScriptLoadRequest* aRequest);
  /**
   * Start a load for a classic script URI.
   * Sets up the necessary security flags before calling StartLoadInternal.
   */
  nsresult StartClassicLoad(ScriptLoadRequest* aRequest);

  /**
   * Start a load for a module script URI.
   */
  nsresult StartLoadInternal(ScriptLoadRequest* aRequest,
                             nsSecurityFlags securityFlags);

  /**
   * Abort the current stream, and re-start with a new load request from scratch
   * without requesting any alternate data. Returns NS_BINDING_RETARGETED on
   * success, as this error code is used to abort the input stream.
   */
  nsresult RestartLoad(ScriptLoadRequest* aRequest);

  void HandleLoadError(ScriptLoadRequest* aRequest, nsresult aResult);

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
  bool SelfReadyToExecuteParserBlockingScripts() {
    return ReadyToExecuteScripts() && !mParserBlockingBlockerCount;
  }

  /**
   * Return whether this loader is ready to execute scripts in general.
   */
  bool ReadyToExecuteScripts() { return mEnabled && !mBlockerCount; }

  nsresult VerifySRI(ScriptLoadRequest* aRequest,
                     nsIIncrementalStreamLoader* aLoader, nsresult aSRIStatus,
                     SRICheckDataVerifier* aSRIDataVerifier) const;

  nsresult SaveSRIHash(ScriptLoadRequest* aRequest,
                       SRICheckDataVerifier* aSRIDataVerifier,
                       uint32_t* sriLength) const;

  void ReportErrorToConsole(ScriptLoadRequest* aRequest,
                            nsresult aResult) const override;

  void ReportWarningToConsole(
      ScriptLoadRequest* aRequest, const char* aMessageName,
      const nsTArray<nsString>& aParams = nsTArray<nsString>()) const override;

  void ReportPreloadErrorsToConsole(ScriptLoadRequest* aRequest);

  nsresult AttemptAsyncScriptCompile(ScriptLoadRequest* aRequest,
                                     bool* aCouldCompileOut);
  nsresult ProcessRequest(ScriptLoadRequest* aRequest);
  nsresult CompileOffThreadOrProcessRequest(ScriptLoadRequest* aRequest);
  void FireScriptAvailable(nsresult aResult, ScriptLoadRequest* aRequest);
  // TODO: Convert this to MOZ_CAN_RUN_SCRIPT (bug 1415230)
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void FireScriptEvaluated(
      nsresult aResult, ScriptLoadRequest* aRequest);

  // Implements https://html.spec.whatwg.org/#execute-the-script-block
  nsresult EvaluateScriptElement(ScriptLoadRequest* aRequest);

  // Handles both bytecode and text source scripts; populates exec with a
  // compiled script
  nsresult CompileOrDecodeClassicScript(JSContext* aCx,
                                        JSExecutionContext& aExec,
                                        ScriptLoadRequest* aRequest);

  static nsCString& BytecodeMimeTypeFor(ScriptLoadRequest* aRequest);

  // Decide whether to encode bytecode for given script load request,
  // and store the script into the request if necessary.
  //
  // This method must be called before executing the script.
  void MaybePrepareForBytecodeEncodingBeforeExecute(
      ScriptLoadRequest* aRequest, JS::Handle<JSScript*> aScript);

  // Queue the script load request for bytecode encoding if we decided to
  // encode, or cleanup the script load request fields otherwise.
  //
  // This method must be called after executing the script.
  nsresult MaybePrepareForBytecodeEncodingAfterExecute(
      ScriptLoadRequest* aRequest, nsresult aRv);

  // Returns true if MaybePrepareForBytecodeEncodingAfterExecute is called
  // for given script load request.
  bool IsAlreadyHandledForBytecodeEncodingPreparation(
      ScriptLoadRequest* aRequest);

  void MaybePrepareModuleForBytecodeEncodingBeforeExecute(
      JSContext* aCx, ModuleLoadRequest* aRequest) override;

  nsresult MaybePrepareModuleForBytecodeEncodingAfterExecute(
      ModuleLoadRequest* aRequest, nsresult aRv) override;

  // Implements https://html.spec.whatwg.org/#run-a-classic-script
  nsresult EvaluateScript(nsIGlobalObject* aGlobalObject,
                          ScriptLoadRequest* aRequest);

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
  void MaybeTriggerBytecodeEncoding() override;

  /**
   * Iterate over all script load request and save the bytecode of executed
   * functions on the cache provided by the channel.
   */
  void EncodeBytecode();
  void EncodeRequestBytecode(JSContext* aCx, ScriptLoadRequest* aRequest);

  void GiveUpBytecodeEncoding();

  already_AddRefed<nsIGlobalObject> GetGlobalForRequest(
      ScriptLoadRequest* aRequest);

  already_AddRefed<nsIScriptGlobalObject> GetScriptGlobalObject();

  // Fill in CompileOptions, as well as produce the introducer script for
  // subsequent calls to UpdateDebuggerMetadata
  nsresult FillCompileOptionsForRequest(
      JSContext* aCx, ScriptLoadRequest* aRequest, JS::CompileOptions* aOptions,
      JS::MutableHandle<JSScript*> aIntroductionScript) override;

  uint32_t NumberOfProcessors();
  int32_t PhysicalSizeOfMemoryInGB();

  nsresult PrepareLoadedRequest(ScriptLoadRequest* aRequest,
                                nsIIncrementalStreamLoader* aLoader,
                                nsresult aStatus);

  void AddDeferRequest(ScriptLoadRequest* aRequest);
  void AddAsyncRequest(ScriptLoadRequest* aRequest);
  bool MaybeRemovedDeferRequests();

  bool ShouldApplyDelazifyStrategy(ScriptLoadRequest* aRequest);
  void ApplyDelazifyStrategy(JS::CompileOptions* aOptions);

  bool ShouldCompileOffThread(ScriptLoadRequest* aRequest);

  void MaybeMoveToLoadedList(ScriptLoadRequest* aRequest);

  using MaybeSourceText =
      mozilla::MaybeOneOf<JS::SourceText<char16_t>, JS::SourceText<Utf8Unit>>;

  // Returns wether we should save the bytecode of this script after the
  // execution of the script.
  static bool ShouldCacheBytecode(ScriptLoadRequest* aRequest);

  void RunScriptWhenSafe(ScriptLoadRequest* aRequest);

  /**
   *  Wait for any unused off thread compilations to finish and then
   *  cancel them.
   */
  void CancelScriptLoadRequests();

  Document* mDocument;  // [WEAK]
  nsCOMArray<nsIScriptLoaderObserver> mObservers;
  ScriptLoadRequestList mNonAsyncExternalScriptInsertedRequests;
  // mLoadingAsyncRequests holds async requests while they're loading; when they
  // have been loaded they are moved to mLoadedAsyncRequests.
  ScriptLoadRequestList mLoadingAsyncRequests;
  ScriptLoadRequestList mLoadedAsyncRequests;
  ScriptLoadRequestList mDeferRequests;
  ScriptLoadRequestList mXSLTRequests;
  RefPtr<ScriptLoadRequest> mParserBlockingRequest;
  ScriptLoadRequestList mOffThreadCompilingRequests;

  // List of script load request that are holding a buffer which has to be saved
  // on the cache.
  ScriptLoadRequestList mBytecodeEncodingQueue;

  // In mRequests, the additional information here is stored by the element.
  struct PreloadInfo {
    RefPtr<ScriptLoadRequest> mRequest;
    nsString mCharset;
  };

  friend void ImplCycleCollectionUnlink(ScriptLoader::PreloadInfo& aField);
  friend void ImplCycleCollectionTraverse(
      nsCycleCollectionTraversalCallback& aCallback,
      ScriptLoader::PreloadInfo& aField, const char* aName, uint32_t aFlags);

  struct PreloadRequestComparator {
    bool Equals(const PreloadInfo& aPi,
                ScriptLoadRequest* const& aRequest) const {
      return aRequest == aPi.mRequest;
    }
  };

  struct PreloadURIComparator {
    bool Equals(const PreloadInfo& aPi, nsIURI* const& aURI) const;
  };

  nsTArray<PreloadInfo> mPreloads;

  nsCOMPtr<nsIScriptElement> mCurrentScript;
  nsCOMPtr<nsIScriptElement> mCurrentParserInsertedScript;
  nsTArray<RefPtr<ScriptLoader>> mPendingChildLoaders;
  uint32_t mParserBlockingBlockerCount;
  uint32_t mBlockerCount;
  uint32_t mNumberOfProcessors;
  uint32_t mTotalFullParseSize;
  int32_t mPhysicalSizeOfMemory;
  bool mEnabled;
  bool mDeferEnabled;
  bool mSpeculativeOMTParsingEnabled;
  bool mDeferCheckpointReached;
  bool mBlockingDOMContentLoaded;
  bool mLoadEventFired;
  bool mGiveUpEncoding;

  TimeDuration mMainThreadParseTime;

  nsCOMPtr<nsIConsoleReportCollector> mReporter;

  // ShutdownObserver for off thread compilations
  RefPtr<AsyncCompileShutdownObserver> mShutdownObserver;

  RefPtr<ModuleLoader> mModuleLoader;
  nsTArray<RefPtr<ModuleLoader>> mWebExtModuleLoaders;
  nsTArray<RefPtr<ModuleLoader>> mShadowRealmModuleLoaders;

  // Logging
 public:
  static LazyLogModule gCspPRLog;
  static LazyLogModule gScriptLoaderLog;
};

class nsAutoScriptLoaderDisabler {
 public:
  explicit nsAutoScriptLoaderDisabler(Document* aDoc);

  ~nsAutoScriptLoaderDisabler();

  bool mWasEnabled;
  RefPtr<ScriptLoader> mLoader;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ScriptLoader_h

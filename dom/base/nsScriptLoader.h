/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A class that handles loading and evaluation of <script> elements.
 */

#ifndef __nsScriptLoader_h__
#define __nsScriptLoader_h__

#include "nsCOMPtr.h"
#include "nsRefPtrHashtable.h"
#include "nsIUnicodeDecoder.h"
#include "nsIScriptElement.h"
#include "nsCOMArray.h"
#include "nsCycleCollectionParticipant.h"
#include "nsTArray.h"
#include "nsAutoPtr.h"
#include "nsIDocument.h"
#include "nsIIncrementalStreamLoader.h"
#include "nsIURIClassifier.h"
#include "nsURIHashKey.h"
#include "mozilla/CORSMode.h"
#include "mozilla/dom/SRIMetadata.h"
#include "mozilla/dom/SRICheck.h"
#include "mozilla/LinkedList.h"
#include "mozilla/MozPromise.h"
#include "mozilla/net/ReferrerPolicy.h"
#include "mozilla/Vector.h"

class nsModuleLoadRequest;
class nsModuleScript;
class nsScriptLoadRequestList;
class nsIURI;

namespace JS {
  class SourceBufferHolder;
} // namespace JS

namespace mozilla {
namespace dom {
class AutoJSAPI;
} // namespace dom
} // namespace mozilla

//////////////////////////////////////////////////////////////
// Per-request data structure
//////////////////////////////////////////////////////////////

enum class nsScriptKind {
  Classic,
  Module
};

class nsScriptLoadRequest : public nsISupports,
                            private mozilla::LinkedListElement<nsScriptLoadRequest>
{
  typedef LinkedListElement<nsScriptLoadRequest> super;

  // Allow LinkedListElement<nsScriptLoadRequest> to cast us to itself as needed.
  friend class mozilla::LinkedListElement<nsScriptLoadRequest>;
  friend class nsScriptLoadRequestList;

protected:
  virtual ~nsScriptLoadRequest();

public:
  nsScriptLoadRequest(nsScriptKind aKind,
                      nsIScriptElement* aElement,
                      uint32_t aVersion,
                      mozilla::CORSMode aCORSMode,
                      const mozilla::dom::SRIMetadata &aIntegrity)
    : mKind(aKind),
      mElement(aElement),
      mProgress(Progress::Loading),
      mIsInline(true),
      mHasSourceMapURL(false),
      mIsDefer(false),
      mIsAsync(false),
      mIsNonAsyncScriptInserted(false),
      mIsXSLT(false),
      mIsCanceled(false),
      mWasCompiledOMT(false),
      mIsTracking(false),
      mOffThreadToken(nullptr),
      mScriptTextBuf(nullptr),
      mScriptTextLength(0),
      mJSVersion(aVersion),
      mLineNo(1),
      mCORSMode(aCORSMode),
      mIntegrity(aIntegrity),
      mReferrerPolicy(mozilla::net::RP_Default)
  {
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsScriptLoadRequest)

  bool IsModuleRequest() const
  {
    return mKind == nsScriptKind::Module;
  }

  nsModuleLoadRequest* AsModuleRequest();

  void FireScriptAvailable(nsresult aResult)
  {
    mElement->ScriptAvailable(aResult, mElement, mIsInline, mURI, mLineNo);
  }
  void FireScriptEvaluated(nsresult aResult)
  {
    mElement->ScriptEvaluated(aResult, mElement, mIsInline);
  }

  bool IsPreload()
  {
    return mElement == nullptr;
  }

  virtual void Cancel();

  bool IsCanceled() const
  {
    return mIsCanceled;
  }

  virtual void SetReady();

  void** OffThreadTokenPtr()
  {
    return mOffThreadToken ?  &mOffThreadToken : nullptr;
  }

  bool IsTracking() const
  {
    return mIsTracking;
  }

  enum class Progress {
    Loading,
    Compiling,
    FetchingImports,
    Ready
  };
  bool IsReadyToRun() const {
    return mProgress == Progress::Ready;
  }
  bool IsLoading() const {
    return mProgress == Progress::Loading;
  }
  bool InCompilingStage() const {
    return mProgress == Progress::Compiling ||
           (IsReadyToRun() && mWasCompiledOMT);
  }

  void MaybeCancelOffThreadScript();

  using super::getNext;
  using super::isInList;

  const nsScriptKind mKind;
  nsCOMPtr<nsIScriptElement> mElement;
  Progress mProgress;     // Are we still waiting for a load to complete?
  bool mIsInline;         // Is the script inline or loaded?
  bool mHasSourceMapURL;  // Does the HTTP header have a source map url?
  bool mIsDefer;          // True if we live in mDeferRequests.
  bool mIsAsync;          // True if we live in mLoadingAsyncRequests or mLoadedAsyncRequests.
  bool mIsNonAsyncScriptInserted; // True if we live in mNonAsyncExternalScriptInsertedRequests
  bool mIsXSLT;           // True if we live in mXSLTRequests.
  bool mIsCanceled;       // True if we have been explicitly canceled.
  bool mWasCompiledOMT;   // True if the script has been compiled off main thread.
  bool mIsTracking;       // True if the script comes from a source on our tracking protection list.
  void* mOffThreadToken;  // Off-thread parsing token.
  nsString mSourceMapURL; // Holds source map url for loaded scripts
  char16_t* mScriptTextBuf; // Holds script text for non-inline scripts. Don't
  size_t mScriptTextLength; // use nsString so we can give ownership to jsapi.
  uint32_t mJSVersion;
  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIPrincipal> mOriginPrincipal;
  nsAutoCString mURL;     // Keep the URI's filename alive during off thread parsing.
  int32_t mLineNo;
  const mozilla::CORSMode mCORSMode;
  const mozilla::dom::SRIMetadata mIntegrity;
  mozilla::net::ReferrerPolicy mReferrerPolicy;
};

class nsScriptLoadRequestList : private mozilla::LinkedList<nsScriptLoadRequest>
{
  typedef mozilla::LinkedList<nsScriptLoadRequest> super;

public:
  ~nsScriptLoadRequestList();

  void Clear();

#ifdef DEBUG
  bool Contains(nsScriptLoadRequest* aElem) const;
#endif // DEBUG

  using super::getFirst;
  using super::isEmpty;

  void AppendElement(nsScriptLoadRequest* aElem)
  {
    MOZ_ASSERT(!aElem->isInList());
    NS_ADDREF(aElem);
    insertBack(aElem);
  }

  MOZ_MUST_USE
  already_AddRefed<nsScriptLoadRequest> Steal(nsScriptLoadRequest* aElem)
  {
    aElem->removeFrom(*this);
    return dont_AddRef(aElem);
  }

  MOZ_MUST_USE
  already_AddRefed<nsScriptLoadRequest> StealFirst()
  {
    MOZ_ASSERT(!isEmpty());
    return Steal(getFirst());
  }

  void Remove(nsScriptLoadRequest* aElem)
  {
    aElem->removeFrom(*this);
    NS_RELEASE(aElem);
  }
};

//////////////////////////////////////////////////////////////
// Script loader implementation
//////////////////////////////////////////////////////////////

class nsScriptLoader final : public nsISupports
{
  class MOZ_STACK_CLASS AutoCurrentScriptUpdater
  {
  public:
    AutoCurrentScriptUpdater(nsScriptLoader* aScriptLoader,
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
    nsScriptLoader* mScriptLoader;
  };

  friend class nsModuleLoadRequest;
  friend class nsScriptRequestProcessor;
  friend class nsScriptLoadHandler;
  friend class AutoCurrentScriptUpdater;

public:
  explicit nsScriptLoader(nsIDocument* aDocument);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsScriptLoader)

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
   * Returns the document reference.
   */
  nsIDocument* GetDocument() const
  {
    return mDocument;
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

  /**
   * Handle the completion of a stream.  This is called by the
   * nsScriptLoadHandler object which observes the IncrementalStreamLoader
   * loading the script.
   */
  nsresult OnStreamComplete(nsIChannel* aChannel,
                            nsISupports* aContext,
                            nsresult aChannelStatus,
                            nsresult aSRIStatus,
                            mozilla::Vector<char16_t> &aString,
                            mozilla::dom::SRICheckDataVerifier* aSRIDataVerifier);

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
  virtual void PreloadURI(nsIURI *aURI, const nsAString &aCharset,
                          const nsAString &aType,
                          const nsAString &aCrossOrigin,
                          const nsAString& aIntegrity,
                          bool aScriptFromHead,
                          const mozilla::net::ReferrerPolicy aReferrerPolicy);

  /**
   * Process a request that was deferred so that the script could be compiled
   * off thread.
   */
  nsresult ProcessOffThreadRequest(nsScriptLoadRequest *aRequest);

  bool AddPendingChildLoader(nsScriptLoader* aChild) {
    return mPendingChildLoaders.AppendElement(aChild) != nullptr;
  }

  mozilla::dom::DocGroup* GetDocGroup() const
  {
    return mDocument->GetDocGroup();
  }

private:
  virtual ~nsScriptLoader();

  nsScriptLoadRequest* CreateLoadRequest(
    nsScriptKind aKind,
    nsIScriptElement* aElement,
    uint32_t aVersion,
    mozilla::CORSMode aCORSMode,
    const mozilla::dom::SRIMetadata &aIntegrity);

  /**
   * Unblocks the creator parser of the parser-blocking scripts.
   */
  void UnblockParser(nsScriptLoadRequest* aParserBlockingRequest);

  /**
   * Asynchronously resumes the creator parser of the parser-blocking scripts.
   */
  void ContinueParserAsync(nsScriptLoadRequest* aParserBlockingRequest);


  /**
   * Helper function to check the content policy for a given request.
   */
  static nsresult CheckContentPolicy(nsIDocument* aDocument,
                                     nsISupports *aContext,
                                     nsIURI *aURI,
                                     const nsAString &aType,
                                     bool aIsPreLoad);

  /**
   * Start a load for aRequest's URI.
   */
  nsresult StartLoad(nsScriptLoadRequest *aRequest, const nsAString &aType,
                     bool aScriptFromHead);

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

  nsresult AttemptAsyncScriptCompile(nsScriptLoadRequest* aRequest);
  nsresult ProcessRequest(nsScriptLoadRequest* aRequest);
  nsresult CompileOffThreadOrProcessRequest(nsScriptLoadRequest* aRequest);
  void FireScriptAvailable(nsresult aResult,
                           nsScriptLoadRequest* aRequest);
  void FireScriptEvaluated(nsresult aResult,
                           nsScriptLoadRequest* aRequest);
  nsresult EvaluateScript(nsScriptLoadRequest* aRequest);

  already_AddRefed<nsIScriptGlobalObject> GetScriptGlobalObject();
  nsresult FillCompileOptionsForRequest(const mozilla::dom::AutoJSAPI& jsapi,
                                        nsScriptLoadRequest* aRequest,
                                        JS::Handle<JSObject*> aScopeChain,
                                        JS::CompileOptions* aOptions);

  uint32_t NumberOfProcessors();
  nsresult PrepareLoadedRequest(nsScriptLoadRequest* aRequest,
                                nsIChannel* aChannel,
                                nsresult aStatus,
                                mozilla::Vector<char16_t> &aString);

  void AddDeferRequest(nsScriptLoadRequest* aRequest);
  bool MaybeRemovedDeferRequests();

  void MaybeMoveToLoadedList(nsScriptLoadRequest* aRequest);

  JS::SourceBufferHolder GetScriptSource(nsScriptLoadRequest* aRequest,
                                         nsAutoString& inlineData);

  void SetModuleFetchStarted(nsModuleLoadRequest *aRequest);
  void SetModuleFetchFinishedAndResumeWaitingRequests(nsModuleLoadRequest *aRequest,
                                                      nsresult aResult);

  bool IsFetchingModule(nsModuleLoadRequest *aRequest) const;

  bool ModuleMapContainsModule(nsModuleLoadRequest *aRequest) const;
  RefPtr<mozilla::GenericPromise> WaitForModuleFetch(nsModuleLoadRequest *aRequest);
  nsModuleScript* GetFetchedModule(nsIURI* aURL) const;

  friend bool
  HostResolveImportedModule(JSContext* aCx, unsigned argc, JS::Value* vp);

  nsresult CreateModuleScript(nsModuleLoadRequest* aRequest);
  nsresult ProcessFetchedModuleSource(nsModuleLoadRequest* aRequest);
  void ProcessLoadedModuleTree(nsModuleLoadRequest* aRequest);
  bool InstantiateModuleTree(nsModuleLoadRequest* aRequest);
  void StartFetchingModuleDependencies(nsModuleLoadRequest* aRequest);

  RefPtr<mozilla::GenericPromise>
  StartFetchingModuleAndDependencies(nsModuleLoadRequest* aRequest, nsIURI* aURI);

  nsIDocument* mDocument;                   // [WEAK]
  nsCOMArray<nsIScriptLoaderObserver> mObservers;
  nsScriptLoadRequestList mNonAsyncExternalScriptInsertedRequests;
  // mLoadingAsyncRequests holds async requests while they're loading; when they
  // have been loaded they are moved to mLoadedAsyncRequests.
  nsScriptLoadRequestList mLoadingAsyncRequests;
  nsScriptLoadRequestList mLoadedAsyncRequests;
  nsScriptLoadRequestList mDeferRequests;
  nsScriptLoadRequestList mXSLTRequests;
  RefPtr<nsScriptLoadRequest> mParserBlockingRequest;

  // In mRequests, the additional information here is stored by the element.
  struct PreloadInfo {
    RefPtr<nsScriptLoadRequest> mRequest;
    nsString mCharset;
  };

  friend void ImplCycleCollectionUnlink(nsScriptLoader::PreloadInfo& aField);
  friend void ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                                          nsScriptLoader::PreloadInfo& aField,
                                          const char* aName, uint32_t aFlags);

  struct PreloadRequestComparator {
    bool Equals(const PreloadInfo &aPi, nsScriptLoadRequest * const &aRequest)
        const
    {
      return aRequest == aPi.mRequest;
    }
  };
  struct PreloadURIComparator {
    bool Equals(const PreloadInfo &aPi, nsIURI * const &aURI) const;
  };
  nsTArray<PreloadInfo> mPreloads;

  nsCOMPtr<nsIScriptElement> mCurrentScript;
  nsCOMPtr<nsIScriptElement> mCurrentParserInsertedScript;
  nsTArray< RefPtr<nsScriptLoader> > mPendingChildLoaders;
  uint32_t mParserBlockingBlockerCount;
  uint32_t mBlockerCount;
  uint32_t mNumberOfProcessors;
  bool mEnabled;
  bool mDeferEnabled;
  bool mDocumentParsingDone;
  bool mBlockingDOMContentLoaded;

  // Module map
  nsRefPtrHashtable<nsURIHashKey, mozilla::GenericPromise::Private> mFetchingModules;
  nsRefPtrHashtable<nsURIHashKey, nsModuleScript> mFetchedModules;

  nsCOMPtr<nsIConsoleReportCollector> mReporter;
};

class nsScriptLoadHandler final : public nsIIncrementalStreamLoaderObserver,
                                  private nsIURIClassifierCallback
{
public:
  explicit nsScriptLoadHandler(nsScriptLoader* aScriptLoader,
                               nsScriptLoadRequest *aRequest,
                               mozilla::dom::SRICheckDataVerifier *aSRIDataVerifier);

  nsresult Init();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIINCREMENTALSTREAMLOADEROBSERVER

private:
  NS_DECL_NSIURICLASSIFIERCALLBACK

  virtual ~nsScriptLoadHandler();

  /*
   * Try to decode some raw data.
   */
  nsresult TryDecodeRawData(const uint8_t* aData, uint32_t aDataLength,
                            bool aEndOfStream);

  /*
   * Discover the charset by looking at the stream data, the script
   * tag, and other indicators.  Returns true if charset has been
   * discovered.
   */
  bool EnsureDecoder(nsIIncrementalStreamLoader *aLoader,
                     const uint8_t* aData, uint32_t aDataLength,
                     bool aEndOfStream);

  /*
   * Call the script loader OnClassifyComplete if both the load and the
   * classification have finished.
   */
  nsresult MaybeInvokeOnStreamComplete();

  // ScriptLoader which will handle the parsed script.
  RefPtr<nsScriptLoader>        mScriptLoader;

  // The nsScriptLoadRequest for this load.
  RefPtr<nsScriptLoadRequest>   mRequest;

  // SRI data verifier.
  nsAutoPtr<mozilla::dom::SRICheckDataVerifier> mSRIDataVerifier;

  // Status of the channel load.
  nsresult                      mChannelStatus;

  // Status of SRI data operations.
  nsresult                      mSRIStatus;

  // status of the classification of the script URI.
  nsresult                      mClassificationStatus;

  // Unicode decoder for charset.
  nsCOMPtr<nsIUnicodeDecoder>   mDecoder;

  // Accumulated decoded char buffer.
  mozilla::Vector<char16_t>     mBuffer;

  // The channel we loaded the script from.
  nsCOMPtr<nsIChannel>          mChannel;
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
  RefPtr<nsScriptLoader> mLoader;
};

#endif //__nsScriptLoader_h__

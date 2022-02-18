/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ScriptLoadRequest_h
#define mozilla_dom_ScriptLoadRequest_h

#include "js/AllocPolicy.h"
#include "js/RootingAPI.h"
#include "js/SourceText.h"
#include "js/TypeDecls.h"
#include "mozilla/Atomics.h"
#include "mozilla/Assertions.h"
#include "mozilla/CORSMode.h"
#include "mozilla/dom/SRIMetadata.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Maybe.h"
#include "mozilla/MaybeOneOf.h"
#include "mozilla/PreloaderBase.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/Utf8.h"  // mozilla::Utf8Unit
#include "mozilla/Variant.h"
#include "mozilla/Vector.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIGlobalObject.h"
#include "nsIScriptElement.h"
#include "ScriptKind.h"

class nsICacheInfoChannel;

namespace JS {
class OffThreadToken;
}

namespace mozilla {
namespace dom {

class Element;
class ModuleLoadRequest;
class ScriptLoadRequestList;
class DOMScriptLoadContext;

/*
 * ScriptFetchOptions loosely corresponds to HTML's "script fetch options",
 * https://html.spec.whatwg.org/multipage/webappapis.html#script-fetch-options
 * with the exception of the following properties:
 *   cryptographic nonce
 *      The cryptographic nonce metadata used for the initial fetch and for
 *      fetching any imported modules. This is handled by the principal.
 *   parser metadata
 *      The parser metadata used for the initial fetch and for fetching any
 *      imported modules. This is not implemented.
 *   integrity metadata
 *      The integrity metadata used for the initial fetch. This is
 *      implemented in ScriptLoadRequest, as it changes for every
 *      ScriptLoadRequest.
 *
 * In the case of classic scripts without dynamic import, this object is
 * used once. For modules, this object is propogated throughout the module
 * tree. If there is a dynamically imported module in any type of script,
 * the ScriptFetchOptions object will be propogated from its importer.
 */

class ScriptFetchOptions {
  ~ScriptFetchOptions();

 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(ScriptFetchOptions)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(ScriptFetchOptions)

  ScriptFetchOptions(mozilla::CORSMode aCORSMode,
                     enum ReferrerPolicy aReferrerPolicy,
                     nsIPrincipal* aTriggeringPrincipal);

  /*
   *  The credentials mode used for the initial fetch (for module scripts)
   *  and for fetching any imported modules (for both module scripts and
   *  classic scripts)
   */
  const mozilla::CORSMode mCORSMode;

  /*
   *  The referrer policy used for the initial fetch and for fetching any
   *  imported modules
   */
  const enum ReferrerPolicy mReferrerPolicy;

  /*
   * related to cryptographic nonce, used to determine CSP
   */
  nsCOMPtr<nsIPrincipal> mTriggeringPrincipal;
};

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
 * the DOMScriptLoadContext class for details).  In terms of responsibility,
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

class ScriptLoadRequest
    : public nsISupports,
      private mozilla::LinkedListElement<ScriptLoadRequest> {
  using super = LinkedListElement<ScriptLoadRequest>;

  // Allow LinkedListElement<ScriptLoadRequest> to cast us to itself as needed.
  friend class mozilla::LinkedListElement<ScriptLoadRequest>;
  friend class ScriptLoadRequestList;

 protected:
  virtual ~ScriptLoadRequest();

 public:
  ScriptLoadRequest(ScriptKind aKind, nsIURI* aURI,
                    ScriptFetchOptions* aFetchOptions,
                    const SRIMetadata& aIntegrity, nsIURI* aReferrer,
                    DOMScriptLoadContext* aContext);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ScriptLoadRequest)

  using super::getNext;
  using super::isInList;
  using MaybeSourceText =
      mozilla::MaybeOneOf<JS::SourceText<char16_t>, JS::SourceText<Utf8Unit>>;

  bool IsModuleRequest() const { return mKind == ScriptKind::eModule; }

  ModuleLoadRequest* AsModuleRequest();

  virtual bool IsTopLevel() const { return true; };

  virtual void Cancel();

  bool IsCanceled() const { return mIsCanceled; }

  virtual void SetReady();

  enum class Progress : uint8_t {
    eLoading,         // Request either source or bytecode
    eLoading_Source,  // Explicitly Request source stream
    eCompiling,
    eFetchingImports,
    eReady
  };

  bool IsReadyToRun() const { return mProgress == Progress::eReady; }
  bool IsLoading() const {
    return mProgress == Progress::eLoading ||
           mProgress == Progress::eLoading_Source;
  }

  bool IsLoadingSource() const {
    return mProgress == Progress::eLoading_Source;
  }

  bool InCompilingStage() const { return mProgress == Progress::eCompiling; }

  // Type of data provided by the nsChannel.
  enum class DataType : uint8_t { eUnknown, eTextSource, eBytecode };

  bool IsUnknownDataType() const { return mDataType == DataType::eUnknown; }
  bool IsTextSource() const { return mDataType == DataType::eTextSource; }
  bool IsSource() const { return IsTextSource(); }

  void SetUnknownDataType() {
    mDataType = DataType::eUnknown;
    mScriptData.reset();
  }
  void SetTextSource() {
    MOZ_ASSERT(IsUnknownDataType());
    mDataType = DataType::eTextSource;
    if (StaticPrefs::
            dom_script_loader_external_scripts_utf8_parsing_enabled()) {
      mScriptData.emplace(VariantType<ScriptTextBuffer<Utf8Unit>>());
    } else {
      mScriptData.emplace(VariantType<ScriptTextBuffer<char16_t>>());
    }
  }

  // Use a vector backed by the JS allocator for script text so that contents
  // can be transferred in constant time to the JS engine, not copied in linear
  // time.
  template <typename Unit>
  using ScriptTextBuffer = Vector<Unit, 0, js::MallocAllocPolicy>;

  bool IsUTF16Text() const {
    return mScriptData->is<ScriptTextBuffer<char16_t>>();
  }
  bool IsUTF8Text() const {
    return mScriptData->is<ScriptTextBuffer<Utf8Unit>>();
  }

  template <typename Unit>
  const ScriptTextBuffer<Unit>& ScriptText() const {
    MOZ_ASSERT(IsTextSource());
    return mScriptData->as<ScriptTextBuffer<Unit>>();
  }
  template <typename Unit>
  ScriptTextBuffer<Unit>& ScriptText() {
    MOZ_ASSERT(IsTextSource());
    return mScriptData->as<ScriptTextBuffer<Unit>>();
  }

  size_t ScriptTextLength() const {
    MOZ_ASSERT(IsTextSource());
    return IsUTF16Text() ? ScriptText<char16_t>().length()
                         : ScriptText<Utf8Unit>().length();
  }

  // Get source text.  On success |aMaybeSource| will contain either UTF-8 or
  // UTF-16 source; on failure it will remain in its initial state.
  nsresult GetScriptSource(JSContext* aCx, MaybeSourceText* aMaybeSource);

  void ClearScriptText() {
    MOZ_ASSERT(IsTextSource());
    return IsUTF16Text() ? ScriptText<char16_t>().clearAndFree()
                         : ScriptText<Utf8Unit>().clearAndFree();
  }

  enum ReferrerPolicy ReferrerPolicy() const {
    return mFetchOptions->mReferrerPolicy;
  }

  nsIPrincipal* TriggeringPrincipal() const {
    return mFetchOptions->mTriggeringPrincipal;
  }

  void ClearScriptSource();

  void SetScript(JSScript* aScript);

  bool IsBytecode() const { return mDataType == DataType::eBytecode; }

  void SetBytecode();

  mozilla::CORSMode CORSMode() const { return mFetchOptions->mCORSMode; }

  void DropBytecodeCacheReferences();

  bool HasLoadContext() { return mLoadContext; }

  DOMScriptLoadContext* GetLoadContext() {
    MOZ_ASSERT(mLoadContext);
    return mLoadContext;
  }

  const ScriptKind mKind;  // Whether this is a classic script or a module
                           // script.

  bool mIsCanceled;    // True if we have been explicitly canceled.
  Progress mProgress;  // Are we still waiting for a load to complete?
  DataType mDataType;  // Does this contain Source or Bytecode?
  RefPtr<ScriptFetchOptions> mFetchOptions;
  const SRIMetadata mIntegrity;
  const nsCOMPtr<nsIURI> mReferrer;
  Maybe<nsString> mSourceMapURL;  // Holds source map url for loaded scripts

  // Holds script source data for non-inline scripts.
  Maybe<Variant<ScriptTextBuffer<char16_t>, ScriptTextBuffer<Utf8Unit>>>
      mScriptData;

  // The length of script source text, set when reading completes. This is used
  // since mScriptData is cleared when the source is passed to the JS engine.
  size_t mScriptTextLength;

  // Holds the SRI serialized hash and the script bytecode for non-inline
  // scripts.
  mozilla::Vector<uint8_t> mScriptBytecode;
  uint32_t mBytecodeOffset;  // Offset of the bytecode in mScriptBytecode

  const nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIPrincipal> mOriginPrincipal;
  nsAutoCString
      mURL;  // Keep the URI's filename alive during off thread parsing.

  // The base URL used for resolving relative module imports.
  nsCOMPtr<nsIURI> mBaseURL;

  // Holds the top-level JSScript that corresponds to the current source, once
  // it is parsed, and planned to be saved in the bytecode cache.
  JS::Heap<JSScript*> mScript;

  // Holds the Cache information, which is used to register the bytecode
  // on the cache entry, such that we can load it the next time.
  nsCOMPtr<nsICacheInfoChannel> mCacheInfo;

  // ScriptLoadContext for augmenting the load depending on the loading
  // context (DOM, Worker, etc.)
  RefPtr<DOMScriptLoadContext> mLoadContext;
};

/*
 * DOM specific ScriptLoadContext.
 *
 * ScriptLoadContexts augment the loading of a ScriptLoadRequest. They
 * describe how a ScriptLoadRequests loading and evaluation needs to be
 * augmented, based on the information provided by the loading context. In
 * the case of the DOM, the ScriptLoadContext is used to identify how a script
 * should be loaded according to information found in the HTML document into
 * which it will be loaded. The following fields describe how the
 * ScriptLoadRequest will be loaded.
 *
 *    * mScriptMode
 *        stores the mode (Async, Sync, Deferred), and preload, which
 *        allows the ScriptLoader to decide if the script should be pushed
 *        offThread, or if the preloaded request should be used.
 *    * mScriptFromHead
 *        Set when the script tag is in the head, and should be treated as
 *        a blocking script
 *    * mIsInline
 *        Set for scripts whose bodies are inline in the html. In this case,
 *        the script does not need to be fetched first.
 *    * mIsXSLT
 *        Set if we are in an XSLT request.
 *    * TODO: mIsPreload (will be moved from ScriptFetchOptions)
 *        Set for scripts that are preloaded in a
 *        <link rel="preload" as="script"> element.
 *
 * In addition to describing how the ScriptLoadRequest will be loaded by the
 * DOM ScriptLoader, the ScriptLoadContext contains fields that facilitate
 * those custom behaviors, including support for offthread parsing, pointers
 * to runnables (for cancellation and cleanup if a script is parsed offthread)
 * and preload element specific controls.
 *
 */

class DOMScriptLoadContext : public PreloaderBase {
 protected:
  virtual ~DOMScriptLoadContext();

 public:
  explicit DOMScriptLoadContext(Element* aElement,
                                nsIGlobalObject* aWebExtGlobal = nullptr);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DOMScriptLoadContext)

  void SetRequest(ScriptLoadRequest* aRequest);

  // PreloaderBase
  static void PrioritizeAsPreload(nsIChannel* aChannel);
  virtual void PrioritizeAsPreload() override;

  bool IsPreload() const;

  // This will return nullptr in most cases,
  // unless this is a module being imported by a WebExtension content script.
  // In that case it's the Sandbox global executing that code.
  nsIGlobalObject* GetWebExtGlobal() const;

  bool CompileStarted() const {
    return mRequest->InCompilingStage() ||
           (mRequest->IsReadyToRun() && mWasCompiledOMT);
  }

  JS::OffThreadToken** OffThreadTokenPtr() {
    return mOffThreadToken ? &mOffThreadToken : nullptr;
  }

  bool IsTracking() const { return mIsTracking; }
  void SetIsTracking() {
    MOZ_ASSERT(!mIsTracking);
    mIsTracking = true;
  }

  void BlockOnload(Document* aDocument);

  void MaybeUnblockOnload();

  enum class ScriptMode : uint8_t {
    eBlocking,
    eDeferred,
    eAsync,
    eLinkPreload  // this is a load initiated by <link rel="preload"
                  // as="script"> tag
  };

  void SetScriptMode(bool aDeferAttr, bool aAsyncAttr, bool aLinkPreload);

  bool IsLinkPreloadScript() const {
    return mScriptMode == ScriptMode::eLinkPreload;
  }

  bool IsBlockingScript() const { return mScriptMode == ScriptMode::eBlocking; }

  bool IsDeferredScript() const { return mScriptMode == ScriptMode::eDeferred; }

  bool IsAsyncScript() const { return mScriptMode == ScriptMode::eAsync; }

  nsIScriptElement* GetScriptElement() const;

  // Make this request a preload (speculative) request.
  void SetIsPreloadRequest() {
    MOZ_ASSERT(!GetScriptElement());
    MOZ_ASSERT(!IsPreload());
    mIsPreload = true;
  }

  // Make a preload request into an actual load request for the given element.
  void SetIsLoadRequest(nsIScriptElement* aElement);

  FromParser GetParserCreated() const {
    nsIScriptElement* element = GetScriptElement();
    if (!element) {
      return NOT_FROM_PARSER;
    }
    return element->GetParserCreated();
  }

  // Used to output a string for the Gecko Profiler.
  void GetProfilerLabel(nsACString& aOutString);

  void MaybeCancelOffThreadScript();

  TimeStamp mOffThreadParseStartTime;
  TimeStamp mOffThreadParseStopTime;

  ScriptMode mScriptMode;  // Whether this is a blocking, defer or async script.
  bool mScriptFromHead;    // Synchronous head script block loading of other non
                           // js/css content.
  bool mIsInline;          // Is the script inline or loaded?
  bool mInDeferList;       // True if we live in mDeferRequests.
  bool mInAsyncList;       // True if we live in mLoadingAsyncRequests or
                           // mLoadedAsyncRequests.
  bool mIsNonAsyncScriptInserted;  // True if we live in
                                   // mNonAsyncExternalScriptInsertedRequests
  bool mIsXSLT;                    // True if we live in mXSLTRequests.
  bool mInCompilingList;  // True if we are in mOffThreadCompilingRequests.
  bool mIsTracking;       // True if the script comes from a source on our
                          // tracking protection list.
  bool mWasCompiledOMT;   // True if the script has been compiled off main
                          // thread.

  JS::OffThreadToken* mOffThreadToken;  // Off-thread parsing token.

  Atomic<Runnable*> mRunnable;  // Runnable created when dispatching off thread
                                // compile. Tracked here so that it can be
                                // properly released during cancellation.

  int32_t mLineNo;

  // Set on scripts and top level modules.
  bool mIsPreload;
  nsCOMPtr<Element> mElement;

  /* The Web Extension global -- Only used on DOM Modules.
   *     Specifies a SandBox global with which to associate and run this script.
   *     Propagated throughout the module tree if present.
   */
  nsCOMPtr<nsIGlobalObject> mWebExtGlobal;

  RefPtr<ScriptLoadRequest> mRequest;

  // Non-null if there is a document that this request is blocking from loading.
  RefPtr<Document> mLoadBlockedDocument;

  // For preload requests, we defer reporting errors to the console until the
  // request is used.
  nsresult mUnreportedPreloadError;
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

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ScriptLoadRequest_h

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ScriptLoadRequest_h
#define mozilla_dom_ScriptLoadRequest_h

#include "js/AllocPolicy.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "mozilla/Atomics.h"
#include "mozilla/Assertions.h"
#include "mozilla/CORSMode.h"
#include "mozilla/dom/SRIMetadata.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Maybe.h"
#include "mozilla/PreloaderBase.h"
#include "mozilla/Utf8.h"  // mozilla::Utf8Unit
#include "mozilla/Variant.h"
#include "mozilla/Vector.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
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

/*
 * Some options used when fetching script resources. This only loosely
 * corresponds to HTML's "script fetch options".
 *
 * These are common to all modules in a module graph, and hence a single
 * instance is shared by all ModuleLoadRequest objects in a graph.
 */

class ScriptFetchOptions {
  ~ScriptFetchOptions();

 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(ScriptFetchOptions)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(ScriptFetchOptions)

  ScriptFetchOptions(mozilla::CORSMode aCORSMode,
                     enum ReferrerPolicy aReferrerPolicy, Element* aElement,
                     nsIPrincipal* aTriggeringPrincipal);

  const mozilla::CORSMode mCORSMode;
  const enum ReferrerPolicy mReferrerPolicy;
  bool mIsPreload;
  nsCOMPtr<Element> mElement;
  nsCOMPtr<nsIPrincipal> mTriggeringPrincipal;
};

/*
 * A class that handles loading and evaluation of <script> elements.
 */

class ScriptLoadRequest
    : public PreloaderBase,
      private mozilla::LinkedListElement<ScriptLoadRequest> {
  typedef LinkedListElement<ScriptLoadRequest> super;

  // Allow LinkedListElement<ScriptLoadRequest> to cast us to itself as needed.
  friend class mozilla::LinkedListElement<ScriptLoadRequest>;
  friend class ScriptLoadRequestList;

 protected:
  virtual ~ScriptLoadRequest();

 public:
  ScriptLoadRequest(ScriptKind aKind, nsIURI* aURI,
                    ScriptFetchOptions* aFetchOptions,
                    const SRIMetadata& aIntegrity, nsIURI* aReferrer);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ScriptLoadRequest)

  // PreloaderBase
  static void PrioritizeAsPreload(nsIChannel* aChannel);
  virtual void PrioritizeAsPreload() override;

  bool IsModuleRequest() const { return mKind == ScriptKind::eModule; }

  ModuleLoadRequest* AsModuleRequest();

#ifdef MOZ_GECKO_PROFILER
  TimeStamp mOffThreadParseStartTime;
  TimeStamp mOffThreadParseStopTime;
#endif

  void FireScriptAvailable(nsresult aResult) {
    bool isInlineClassicScript = mIsInline && !IsModuleRequest();
    GetScriptElement()->ScriptAvailable(aResult, GetScriptElement(),
                                        isInlineClassicScript, mURI, mLineNo);
  }
  void FireScriptEvaluated(nsresult aResult) {
    GetScriptElement()->ScriptEvaluated(aResult, GetScriptElement(), mIsInline);
  }

  bool IsPreload() const {
    MOZ_ASSERT_IF(mFetchOptions->mIsPreload, !GetScriptElement());
    return mFetchOptions->mIsPreload;
  }

  virtual void Cancel();

  bool IsCanceled() const { return mIsCanceled; }

  virtual void SetReady();

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
  bool InCompilingStage() const {
    return mProgress == Progress::eCompiling ||
           (IsReadyToRun() && mWasCompiledOMT);
  }

  // Type of data provided by the nsChannel.
  enum class DataType : uint8_t {
    eUnknown,
    eTextSource,
    eBinASTSource,
    eBytecode
  };

  bool IsUnknownDataType() const { return mDataType == DataType::eUnknown; }
  bool IsTextSource() const { return mDataType == DataType::eTextSource; }
  bool IsBinASTSource() const { return false; }
  bool IsSource() const { return IsTextSource() || IsBinASTSource(); }
  bool IsBytecode() const { return mDataType == DataType::eBytecode; }

  void SetUnknownDataType();
  void SetTextSource();
  void SetBinASTSource();
  void SetBytecode();

  // Use a vector backed by the JS allocator for script text so that contents
  // can be transferred in constant time to the JS engine, not copied in linear
  // time.
  template <typename Unit>
  using ScriptTextBuffer = Vector<Unit, 0, js::MallocAllocPolicy>;

  // BinAST data isn't transferred to the JS engine, so it doesn't need to use
  // the JS allocator.
  using BinASTSourceBuffer = Vector<uint8_t>;

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

  const BinASTSourceBuffer& ScriptBinASTData() const {
    MOZ_ASSERT(IsBinASTSource());
    return mScriptData->as<BinASTSourceBuffer>();
  }
  BinASTSourceBuffer& ScriptBinASTData() {
    MOZ_ASSERT(IsBinASTSource());
    return mScriptData->as<BinASTSourceBuffer>();
  }

  size_t ScriptTextLength() const {
    MOZ_ASSERT(IsTextSource());
    return IsUTF16Text() ? ScriptText<char16_t>().length()
                         : ScriptText<Utf8Unit>().length();
  }

  void ClearScriptText() {
    MOZ_ASSERT(IsTextSource());
    return IsUTF16Text() ? ScriptText<char16_t>().clearAndFree()
                         : ScriptText<Utf8Unit>().clearAndFree();
  }

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

  virtual bool IsTopLevel() const {
    // Classic scripts are always top level.
    return true;
  }

  mozilla::CORSMode CORSMode() const { return mFetchOptions->mCORSMode; }
  enum ReferrerPolicy ReferrerPolicy() const {
    return mFetchOptions->mReferrerPolicy;
  }
  nsIScriptElement* GetScriptElement() const;
  nsIPrincipal* TriggeringPrincipal() const {
    return mFetchOptions->mTriggeringPrincipal;
  }

  // Make this request a preload (speculative) request.
  void SetIsPreloadRequest() {
    MOZ_ASSERT(!GetScriptElement());
    MOZ_ASSERT(!IsPreload());
    mFetchOptions->mIsPreload = true;
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

  bool ShouldAcceptBinASTEncoding() const;

  void ClearScriptSource();

  void SetScript(JSScript* aScript);

  void MaybeCancelOffThreadScript();
  void DropBytecodeCacheReferences();

  using super::getNext;
  using super::isInList;

  const ScriptKind mKind;  // Whether this is a classic script or a module
                           // script.
  ScriptMode mScriptMode;  // Whether this is a blocking, defer or async script.
  Progress mProgress;      // Are we still waiting for a load to complete?
  DataType mDataType;      // Does this contain Source or Bytecode?
  bool mScriptFromHead;    // Synchronous head script block loading of other non
                           // js/css content.
  bool mIsInline;          // Is the script inline or loaded?
  bool mInDeferList;       // True if we live in mDeferRequests.
  bool mInAsyncList;       // True if we live in mLoadingAsyncRequests or
                           // mLoadedAsyncRequests.
  bool mIsNonAsyncScriptInserted;  // True if we live in
                                   // mNonAsyncExternalScriptInsertedRequests
  bool mIsXSLT;                    // True if we live in mXSLTRequests.
  bool mIsCanceled;                // True if we have been explicitly canceled.
  bool mWasCompiledOMT;  // True if the script has been compiled off main
                         // thread.
  bool mIsTracking;      // True if the script comes from a source on our
                         // tracking protection list.

  RefPtr<ScriptFetchOptions> mFetchOptions;

  JS::OffThreadToken* mOffThreadToken;  // Off-thread parsing token.
  Maybe<nsString> mSourceMapURL;  // Holds source map url for loaded scripts

  Atomic<Runnable*> mRunnable;  // Runnable created when dispatching off thread
                                // compile. Tracked here so that it can be
                                // properly released during cancellation.

  // Holds the top-level JSScript that corresponds to the current source, once
  // it is parsed, and planned to be saved in the bytecode cache.
  JS::Heap<JSScript*> mScript;

  // Holds script source data for non-inline scripts.
  Maybe<Variant<ScriptTextBuffer<char16_t>, ScriptTextBuffer<Utf8Unit>,
                BinASTSourceBuffer>>
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
  int32_t mLineNo;
  const SRIMetadata mIntegrity;
  const nsCOMPtr<nsIURI> mReferrer;

  // Non-null if there is a document that this request is blocking from loading.
  RefPtr<Document> mLoadBlockedDocument;

  // Holds the Cache information, which is used to register the bytecode
  // on the cache entry, such that we can load it the next time.
  nsCOMPtr<nsICacheInfoChannel> mCacheInfo;

  // The base URL used for resolving relative module imports.
  nsCOMPtr<nsIURI> mBaseURL;

  // For preload requests, we defer reporting errors to the console until the
  // request is used.
  nsresult mUnreportedPreloadError;
};

class ScriptLoadRequestList : private mozilla::LinkedList<ScriptLoadRequest> {
  typedef mozilla::LinkedList<ScriptLoadRequest> super;

 public:
  ~ScriptLoadRequestList();

  void Clear();

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

void ImplCycleCollectionUnlink(ScriptLoadRequestList& aField);

void ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                                 ScriptLoadRequestList& aField,
                                 const char* aName, uint32_t aFlags);

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ScriptLoadRequest_h

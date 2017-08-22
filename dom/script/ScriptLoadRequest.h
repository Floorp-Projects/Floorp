/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ScriptLoadRequest_h
#define mozilla_dom_ScriptLoadRequest_h

#include "mozilla/CORSMode.h"
#include "mozilla/dom/SRIMetadata.h"
#include "mozilla/LinkedList.h"
#include "mozilla/net/ReferrerPolicy.h"
#include "mozilla/Vector.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIScriptElement.h"

class nsICacheInfoChannel;

namespace mozilla {
namespace dom {

class ModuleLoadRequest;
class ScriptLoadRequestList;

enum class ScriptKind {
  Classic,
  Module
};

/*
 * A class that handles loading and evaluation of <script> elements.
 */

class ScriptLoadRequest : public nsISupports,
                          private mozilla::LinkedListElement<ScriptLoadRequest>
{
  typedef LinkedListElement<ScriptLoadRequest> super;

  // Allow LinkedListElement<ScriptLoadRequest> to cast us to itself as needed.
  friend class mozilla::LinkedListElement<ScriptLoadRequest>;
  friend class ScriptLoadRequestList;

protected:
  virtual ~ScriptLoadRequest();

public:
  ScriptLoadRequest(ScriptKind aKind,
                    nsIScriptElement* aElement,
                    uint32_t aVersion,
                    mozilla::CORSMode aCORSMode,
                    const mozilla::dom::SRIMetadata &aIntegrity);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ScriptLoadRequest)

  bool IsModuleRequest() const
  {
    return mKind == ScriptKind::Module;
  }

  ModuleLoadRequest* AsModuleRequest();

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
  void SetIsTracking()
  {
    MOZ_ASSERT(!mIsTracking);
    mIsTracking = true;
  }

  enum class Progress : uint8_t {
    Loading,        // Request either source or bytecode
    Loading_Source, // Explicitly Request source stream
    Compiling,
    FetchingImports,
    Ready
  };

  bool IsReadyToRun() const {
    return mProgress == Progress::Ready;
  }
  bool IsLoading() const {
    return mProgress == Progress::Loading ||
           mProgress == Progress::Loading_Source;
  }
  bool IsLoadingSource() const {
    return mProgress == Progress::Loading_Source;
  }
  bool InCompilingStage() const {
    return mProgress == Progress::Compiling ||
           (IsReadyToRun() && mWasCompiledOMT);
  }

  // Type of data provided by the nsChannel.
  enum class DataType : uint8_t {
    Unknown,
    Source,
    Bytecode
  };

  bool IsUnknownDataType() const {
    return mDataType == DataType::Unknown;
  }
  bool IsSource() const {
    return mDataType == DataType::Source;
  }
  bool IsBytecode() const {
    return mDataType == DataType::Bytecode;
  }

  void MaybeCancelOffThreadScript();
  void DropBytecodeCacheReferences();

  using super::getNext;
  using super::isInList;

  const ScriptKind mKind;
  nsCOMPtr<nsIScriptElement> mElement;
  bool mScriptFromHead;   // Synchronous head script block loading of other non js/css content.
  Progress mProgress;     // Are we still waiting for a load to complete?
  DataType mDataType;     // Does this contain Source or Bytecode?
  bool mIsInline;         // Is the script inline or loaded?
  bool mHasSourceMapURL;  // Does the HTTP header have a source map url?
  bool mIsDefer;          // True if we live in mDeferRequests.
  bool mIsAsync;          // True if we live in mLoadingAsyncRequests or mLoadedAsyncRequests.
  bool mPreloadAsAsync;   // True if this is a preload request and the script is async
  bool mPreloadAsDefer;   // True if this is a preload request and the script is defer
  bool mIsNonAsyncScriptInserted; // True if we live in mNonAsyncExternalScriptInsertedRequests
  bool mIsXSLT;           // True if we live in mXSLTRequests.
  bool mIsCanceled;       // True if we have been explicitly canceled.
  bool mWasCompiledOMT;   // True if the script has been compiled off main thread.
  bool mIsTracking;       // True if the script comes from a source on our tracking protection list.
  void* mOffThreadToken;  // Off-thread parsing token.
  nsString mSourceMapURL; // Holds source map url for loaded scripts

  // Holds the top-level JSScript that corresponds to the current source, once
  // it is parsed, and planned to be saved in the bytecode cache.
  JS::Heap<JSScript*> mScript;

  // Holds script text for non-inline scripts. Don't use nsString so we can give
  // ownership to jsapi.
  mozilla::Vector<char16_t> mScriptText;

  // Holds the SRI serialized hash and the script bytecode for non-inline
  // scripts.
  mozilla::Vector<uint8_t> mScriptBytecode;
  uint32_t mBytecodeOffset; // Offset of the bytecode in mScriptBytecode

  uint32_t mJSVersion;
  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIPrincipal> mOriginPrincipal;
  nsAutoCString mURL;     // Keep the URI's filename alive during off thread parsing.
  int32_t mLineNo;
  const mozilla::CORSMode mCORSMode;
  const mozilla::dom::SRIMetadata mIntegrity;
  mozilla::net::ReferrerPolicy mReferrerPolicy;

  // Holds the Cache information, which is used to register the bytecode
  // on the cache entry, such that we can load it the next time.
  nsCOMPtr<nsICacheInfoChannel> mCacheInfo;
};

class ScriptLoadRequestList : private mozilla::LinkedList<ScriptLoadRequest>
{
  typedef mozilla::LinkedList<ScriptLoadRequest> super;

public:
  ~ScriptLoadRequestList();

  void Clear();

#ifdef DEBUG
  bool Contains(ScriptLoadRequest* aElem) const;
#endif // DEBUG

  using super::getFirst;
  using super::isEmpty;

  void AppendElement(ScriptLoadRequest* aElem)
  {
    MOZ_ASSERT(!aElem->isInList());
    NS_ADDREF(aElem);
    insertBack(aElem);
  }

  MOZ_MUST_USE
  already_AddRefed<ScriptLoadRequest> Steal(ScriptLoadRequest* aElem)
  {
    aElem->removeFrom(*this);
    return dont_AddRef(aElem);
  }

  MOZ_MUST_USE
  already_AddRefed<ScriptLoadRequest> StealFirst()
  {
    MOZ_ASSERT(!isEmpty());
    return Steal(getFirst());
  }

  void Remove(ScriptLoadRequest* aElem)
  {
    aElem->removeFrom(*this);
    NS_RELEASE(aElem);
  }
};

void
ImplCycleCollectionUnlink(ScriptLoadRequestList& aField);

void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            ScriptLoadRequestList& aField,
                            const char* aName,
                            uint32_t aFlags);

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ScriptLoadRequest_h

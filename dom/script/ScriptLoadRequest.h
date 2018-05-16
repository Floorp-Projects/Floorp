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
#include "mozilla/Maybe.h"
#include "mozilla/net/ReferrerPolicy.h"
#include "mozilla/Variant.h"
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
  eClassic,
  eModule
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
                    nsIURI* aURI,
                    nsIScriptElement* aElement,
                    mozilla::CORSMode aCORSMode,
                    const SRIMetadata &aIntegrity,
                    nsIURI* aReferrer,
                    mozilla::net::ReferrerPolicy aReferrerPolicy);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ScriptLoadRequest)

  bool IsModuleRequest() const
  {
    return mKind == ScriptKind::eModule;
  }

  ModuleLoadRequest* AsModuleRequest();

  void FireScriptAvailable(nsresult aResult)
  {
    bool isInlineClassicScript = mIsInline && !IsModuleRequest();
    mElement->ScriptAvailable(aResult, mElement, isInlineClassicScript, mURI,
                              mLineNo);
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

  JS::OffThreadToken** OffThreadTokenPtr()
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
    eLoading,        // Request either source or bytecode
    eLoading_Source, // Explicitly Request source stream
    eCompiling,
    eFetchingImports,
    eReady
  };

  bool IsReadyToRun() const
  {
    return mProgress == Progress::eReady;
  }
  bool IsLoading() const
  {
    return mProgress == Progress::eLoading ||
           mProgress == Progress::eLoading_Source;
  }
  bool IsLoadingSource() const
  {
    return mProgress == Progress::eLoading_Source;
  }
  bool InCompilingStage() const
  {
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

  bool IsUnknownDataType() const
  {
    return mDataType == DataType::eUnknown;
  }
  bool IsTextSource() const
  {
    return mDataType == DataType::eTextSource;
  }
  bool IsBinASTSource() const
  {
#ifdef JS_BUILD_BINAST
    return mDataType == DataType::eBinASTSource;
#else
    return false;
#endif
  }
  bool IsSource() const
  {
    return IsTextSource() || IsBinASTSource();
  }
  bool IsBytecode() const
  {
    return mDataType == DataType::eBytecode;
  }

  void SetUnknownDataType();
  void SetTextSource();
  void SetBinASTSource();
  void SetBytecode();

  const Vector<char16_t>& ScriptText() const {
    MOZ_ASSERT(IsTextSource());
    return mScriptData->as<Vector<char16_t>>();
  }
  Vector<char16_t>& ScriptText() {
    MOZ_ASSERT(IsTextSource());
    return mScriptData->as<Vector<char16_t>>();
  }
  const Vector<uint8_t>& ScriptBinASTData() const {
    MOZ_ASSERT(IsBinASTSource());
    return mScriptData->as<Vector<uint8_t>>();
  }
  Vector<uint8_t>& ScriptBinASTData() {
    MOZ_ASSERT(IsBinASTSource());
    return mScriptData->as<Vector<uint8_t>>();
  }

  enum class ScriptMode : uint8_t {
    eBlocking,
    eDeferred,
    eAsync
  };

  void SetScriptMode(bool aDeferAttr, bool aAsyncAttr);

  bool IsBlockingScript() const
  {
    return mScriptMode == ScriptMode::eBlocking;
  }

  bool IsDeferredScript() const
  {
    return mScriptMode == ScriptMode::eDeferred;
  }

  bool IsAsyncScript() const
  {
    return mScriptMode == ScriptMode::eAsync;
  }

  virtual bool IsTopLevel() const
  {
    // Classic scripts are always top level.
    return true;
  }

  bool ShouldAcceptBinASTEncoding() const;

  void ClearScriptSource();

  void MaybeCancelOffThreadScript();
  void DropBytecodeCacheReferences();

  using super::getNext;
  using super::isInList;

  const ScriptKind mKind;
  nsCOMPtr<nsIScriptElement> mElement;
  bool mScriptFromHead;   // Synchronous head script block loading of other non js/css content.
  Progress mProgress;     // Are we still waiting for a load to complete?
  DataType mDataType;     // Does this contain Source or Bytecode?
  ScriptMode mScriptMode; // Whether this is a blocking, defer or async script.
  bool mIsInline;         // Is the script inline or loaded?
  bool mHasSourceMapURL;  // Does the HTTP header have a source map url?
  bool mInDeferList;      // True if we live in mDeferRequests.
  bool mInAsyncList;      // True if we live in mLoadingAsyncRequests or mLoadedAsyncRequests.
  bool mIsNonAsyncScriptInserted; // True if we live in mNonAsyncExternalScriptInsertedRequests
  bool mIsXSLT;           // True if we live in mXSLTRequests.
  bool mIsCanceled;       // True if we have been explicitly canceled.
  bool mWasCompiledOMT;   // True if the script has been compiled off main thread.
  bool mIsTracking;       // True if the script comes from a source on our tracking protection list.
  JS::OffThreadToken* mOffThreadToken; // Off-thread parsing token.
  nsString mSourceMapURL; // Holds source map url for loaded scripts

  // Holds the top-level JSScript that corresponds to the current source, once
  // it is parsed, and planned to be saved in the bytecode cache.
  JS::Heap<JSScript*> mScript;

  // Holds script source data for non-inline scripts. Don't use nsString so we
  // can give ownership to jsapi. Holds either char16_t source text characters
  // or BinAST encoded bytes depending on mSourceEncoding.
  Maybe<Variant<Vector<char16_t>, Vector<uint8_t>>> mScriptData;

  // Holds the SRI serialized hash and the script bytecode for non-inline
  // scripts.
  mozilla::Vector<uint8_t> mScriptBytecode;
  uint32_t mBytecodeOffset; // Offset of the bytecode in mScriptBytecode

  const nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIPrincipal> mTriggeringPrincipal;
  nsCOMPtr<nsIPrincipal> mOriginPrincipal;
  nsAutoCString mURL;     // Keep the URI's filename alive during off thread parsing.
  int32_t mLineNo;
  const mozilla::CORSMode mCORSMode;
  const SRIMetadata mIntegrity;
  const nsCOMPtr<nsIURI> mReferrer;
  const mozilla::net::ReferrerPolicy mReferrerPolicy;

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

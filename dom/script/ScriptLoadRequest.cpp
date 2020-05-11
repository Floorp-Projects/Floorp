/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScriptLoadRequest.h"

#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/Unused.h"
#include "mozilla/Utf8.h"  // mozilla::Utf8Unit

#include "nsContentUtils.h"
#include "nsIClassOfService.h"
#include "nsISupportsPriority.h"
#include "ScriptLoadRequest.h"
#include "ScriptSettings.h"

namespace mozilla {
namespace dom {

//////////////////////////////////////////////////////////////
// ScriptFetchOptions
//////////////////////////////////////////////////////////////

NS_IMPL_CYCLE_COLLECTION(ScriptFetchOptions, mElement, mTriggeringPrincipal)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(ScriptFetchOptions, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(ScriptFetchOptions, Release)

ScriptFetchOptions::ScriptFetchOptions(mozilla::CORSMode aCORSMode,
                                       ReferrerPolicy aReferrerPolicy,
                                       nsIScriptElement* aElement,
                                       nsIPrincipal* aTriggeringPrincipal)
    : mCORSMode(aCORSMode),
      mReferrerPolicy(aReferrerPolicy),
      mIsPreload(false),
      mElement(aElement),
      mTriggeringPrincipal(aTriggeringPrincipal) {
  MOZ_ASSERT(mTriggeringPrincipal);
}

ScriptFetchOptions::~ScriptFetchOptions() = default;

//////////////////////////////////////////////////////////////
// ScriptLoadRequest
//////////////////////////////////////////////////////////////

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ScriptLoadRequest)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(ScriptLoadRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ScriptLoadRequest)

NS_IMPL_CYCLE_COLLECTION_CLASS(ScriptLoadRequest)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ScriptLoadRequest)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFetchOptions, mCacheInfo)
  tmp->mScript = nullptr;
  tmp->DropBytecodeCacheReferences();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ScriptLoadRequest)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFetchOptions, mCacheInfo)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(ScriptLoadRequest)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mScript)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

ScriptLoadRequest::ScriptLoadRequest(ScriptKind aKind, nsIURI* aURI,
                                     ScriptFetchOptions* aFetchOptions,
                                     const SRIMetadata& aIntegrity,
                                     nsIURI* aReferrer)
    : mKind(aKind),
      mScriptMode(ScriptMode::eBlocking),
      mProgress(Progress::eLoading),
      mDataType(DataType::eUnknown),
      mScriptFromHead(false),
      mIsInline(true),
      mHasSourceMapURL(false),
      mInDeferList(false),
      mInAsyncList(false),
      mIsNonAsyncScriptInserted(false),
      mIsXSLT(false),
      mIsCanceled(false),
      mWasCompiledOMT(false),
      mIsTracking(false),
      mFetchOptions(aFetchOptions),
      mOffThreadToken(nullptr),
      mScriptTextLength(0),
      mScriptBytecode(),
      mBytecodeOffset(0),
      mURI(aURI),
      mLineNo(1),
      mIntegrity(aIntegrity),
      mReferrer(aReferrer),
      mUnreportedPreloadError(NS_OK) {
  MOZ_ASSERT(mFetchOptions);
}

ScriptLoadRequest::~ScriptLoadRequest() {
  // We should always clean up any off-thread script parsing resources.
  MOZ_ASSERT(!mOffThreadToken);

  // But play it safe in release builds and try to clean them up here
  // as a fail safe.
  MaybeCancelOffThreadScript();

  if (mScript) {
    DropBytecodeCacheReferences();
  }

  DropJSObjects(this);
}

void ScriptLoadRequest::SetReady() {
  MOZ_ASSERT(mProgress != Progress::eReady);
  mProgress = Progress::eReady;
}

void ScriptLoadRequest::Cancel() {
  MaybeCancelOffThreadScript();
  mIsCanceled = true;
}

void ScriptLoadRequest::MaybeCancelOffThreadScript() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mOffThreadToken) {
    return;
  }

  JSContext* cx = danger::GetJSContext();
  // Follow the same conditions as ScriptLoader::AttemptAsyncScriptCompile
  if (IsModuleRequest()) {
    JS::CancelOffThreadModule(cx, mOffThreadToken);
  } else if (IsSource()) {
    JS::CancelOffThreadScript(cx, mOffThreadToken);
  } else {
    MOZ_ASSERT(IsBytecode());
    JS::CancelOffThreadScriptDecoder(cx, mOffThreadToken);
  }
  mOffThreadToken = nullptr;
}

void ScriptLoadRequest::DropBytecodeCacheReferences() {
  mCacheInfo = nullptr;
  mScript = nullptr;
  DropJSObjects(this);
}

inline ModuleLoadRequest* ScriptLoadRequest::AsModuleRequest() {
  MOZ_ASSERT(IsModuleRequest());
  return static_cast<ModuleLoadRequest*>(this);
}

void ScriptLoadRequest::SetScriptMode(bool aDeferAttr, bool aAsyncAttr,
                                      bool aLinkPreload) {
  if (aLinkPreload) {
    mScriptMode = ScriptMode::eLinkPreload;
  } else if (aAsyncAttr) {
    mScriptMode = ScriptMode::eAsync;
  } else if (aDeferAttr || IsModuleRequest()) {
    mScriptMode = ScriptMode::eDeferred;
  } else {
    mScriptMode = ScriptMode::eBlocking;
  }
}

void ScriptLoadRequest::SetUnknownDataType() {
  mDataType = DataType::eUnknown;
  mScriptData.reset();
}

void ScriptLoadRequest::SetTextSource() {
  MOZ_ASSERT(IsUnknownDataType());
  mDataType = DataType::eTextSource;
  if (StaticPrefs::dom_script_loader_external_scripts_utf8_parsing_enabled()) {
    mScriptData.emplace(VariantType<ScriptTextBuffer<Utf8Unit>>());
  } else {
    mScriptData.emplace(VariantType<ScriptTextBuffer<char16_t>>());
  }
}

void ScriptLoadRequest::SetBinASTSource() {
#ifdef JS_BUILD_BINAST
  MOZ_ASSERT(IsUnknownDataType());
  mDataType = DataType::eBinASTSource;
  mScriptData.emplace(VariantType<BinASTSourceBuffer>());
#else
  MOZ_CRASH("BinAST not supported");
#endif
}

void ScriptLoadRequest::SetBytecode() {
  MOZ_ASSERT(IsUnknownDataType());
  mDataType = DataType::eBytecode;
}

bool ScriptLoadRequest::ShouldAcceptBinASTEncoding() const {
#ifdef JS_BUILD_BINAST
  // We accept the BinAST encoding if we're using a secure connection.

  if (!mURI->SchemeIs("https")) {
    return false;
  }

  if (StaticPrefs::dom_script_loader_binast_encoding_domain_restrict()) {
    if (!nsContentUtils::IsURIInPrefList(
            mURI, "dom.script_loader.binast_encoding.domain.restrict.list")) {
      return false;
    }
  }

  return true;
#else
  MOZ_CRASH("BinAST not supported");
#endif
}

void ScriptLoadRequest::ClearScriptSource() {
  if (IsTextSource()) {
    ClearScriptText();
  } else if (IsBinASTSource()) {
    ScriptBinASTData().clearAndFree();
  }
}

void ScriptLoadRequest::SetScript(JSScript* aScript) {
  MOZ_ASSERT(!mScript);
  mScript = aScript;
  HoldJSObjects(this);
}

// static
void ScriptLoadRequest::PrioritizeAsPreload(nsIChannel* aChannel) {
  if (nsCOMPtr<nsIClassOfService> cos = do_QueryInterface(aChannel)) {
    cos->AddClassFlags(nsIClassOfService::Unblocked);
  }
  if (nsCOMPtr<nsISupportsPriority> sp = do_QueryInterface(aChannel)) {
    sp->AdjustPriority(nsISupportsPriority::PRIORITY_HIGHEST);
  }
}

void ScriptLoadRequest::PrioritizeAsPreload() {
  if (!IsLinkPreloadScript()) {
    // Do the prioritization only if this request has not already been created
    // as a preload.
    PrioritizeAsPreload(Channel());
  }
}

//////////////////////////////////////////////////////////////
// ScriptLoadRequestList
//////////////////////////////////////////////////////////////

ScriptLoadRequestList::~ScriptLoadRequestList() { Clear(); }

void ScriptLoadRequestList::Clear() {
  while (!isEmpty()) {
    RefPtr<ScriptLoadRequest> first = StealFirst();
    first->Cancel();
    // And just let it go out of scope and die.
  }
}

#ifdef DEBUG
bool ScriptLoadRequestList::Contains(ScriptLoadRequest* aElem) const {
  for (const ScriptLoadRequest* req = getFirst(); req; req = req->getNext()) {
    if (req == aElem) {
      return true;
    }
  }

  return false;
}
#endif  // DEBUG

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

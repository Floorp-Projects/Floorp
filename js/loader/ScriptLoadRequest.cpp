/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScriptLoadRequest.h"
#include "GeckoProfiler.h"

#include "mozilla/dom/Document.h"
#include "mozilla/dom/ScriptLoadContext.h"
#include "mozilla/dom/WorkerLoadContext.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/Unused.h"
#include "mozilla/Utf8.h"  // mozilla::Utf8Unit

#include "js/SourceText.h"

#include "ModuleLoadRequest.h"
#include "nsContentUtils.h"
#include "nsICacheInfoChannel.h"
#include "nsIClassOfService.h"
#include "nsISupportsPriority.h"

using JS::SourceText;

namespace JS::loader {

//////////////////////////////////////////////////////////////
// ScriptFetchOptions
//////////////////////////////////////////////////////////////

NS_IMPL_CYCLE_COLLECTION(ScriptFetchOptions, mTriggeringPrincipal, mElement)

ScriptFetchOptions::ScriptFetchOptions(
    mozilla::CORSMode aCORSMode, const nsAString& aNonce,
    mozilla::dom::RequestPriority aFetchPriority,
    const ParserMetadata aParserMetadata, nsIPrincipal* aTriggeringPrincipal,
    mozilla::dom::Element* aElement)
    : mCORSMode(aCORSMode),
      mNonce(aNonce),
      mFetchPriority(aFetchPriority),
      mParserMetadata(aParserMetadata),
      mTriggeringPrincipal(aTriggeringPrincipal),
      mElement(aElement) {}

ScriptFetchOptions::~ScriptFetchOptions() = default;

//////////////////////////////////////////////////////////////
// ScriptLoadRequest
//////////////////////////////////////////////////////////////

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ScriptLoadRequest)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(ScriptLoadRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ScriptLoadRequest)

NS_IMPL_CYCLE_COLLECTION_CLASS(ScriptLoadRequest)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ScriptLoadRequest)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFetchOptions, mCacheInfo, mLoadContext,
                                  mLoadedScript)
  tmp->mScriptForBytecodeEncoding = nullptr;
  tmp->DropBytecodeCacheReferences();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ScriptLoadRequest)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFetchOptions, mCacheInfo, mLoadContext,
                                    mLoadedScript)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(ScriptLoadRequest)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mScriptForBytecodeEncoding)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

ScriptLoadRequest::ScriptLoadRequest(
    ScriptKind aKind, nsIURI* aURI,
    mozilla::dom::ReferrerPolicy aReferrerPolicy,
    ScriptFetchOptions* aFetchOptions, const SRIMetadata& aIntegrity,
    nsIURI* aReferrer, LoadContextBase* aContext)
    : mKind(aKind),
      mState(State::CheckingCache),
      mFetchSourceOnly(false),
      mReferrerPolicy(aReferrerPolicy),
      mFetchOptions(aFetchOptions),
      mIntegrity(aIntegrity),
      mReferrer(aReferrer),
      mURI(aURI),
      mLoadContext(aContext),
      mEarlyHintPreloaderId(0) {
  MOZ_ASSERT(mFetchOptions);
  if (mLoadContext) {
    mLoadContext->SetRequest(this);
  }
}

ScriptLoadRequest::~ScriptLoadRequest() { DropJSObjects(this); }

void ScriptLoadRequest::SetReady() {
  MOZ_ASSERT(!IsFinished());
  mState = State::Ready;
}

void ScriptLoadRequest::Cancel() {
  mState = State::Canceled;
  if (HasScriptLoadContext()) {
    GetScriptLoadContext()->MaybeCancelOffThreadScript();
  }
}

void ScriptLoadRequest::DropBytecodeCacheReferences() {
  mCacheInfo = nullptr;
  DropJSObjects(this);
}

bool ScriptLoadRequest::HasScriptLoadContext() const {
  return HasLoadContext() && mLoadContext->IsWindowContext();
}

bool ScriptLoadRequest::HasWorkerLoadContext() const {
  return HasLoadContext() && mLoadContext->IsWorkerContext();
}

mozilla::dom::ScriptLoadContext* ScriptLoadRequest::GetScriptLoadContext() {
  MOZ_ASSERT(mLoadContext);
  return mLoadContext->AsWindowContext();
}

mozilla::loader::ComponentLoadContext*
ScriptLoadRequest::GetComponentLoadContext() {
  MOZ_ASSERT(mLoadContext);
  return mLoadContext->AsComponentContext();
}

mozilla::dom::WorkerLoadContext* ScriptLoadRequest::GetWorkerLoadContext() {
  MOZ_ASSERT(mLoadContext);
  return mLoadContext->AsWorkerContext();
}

mozilla::dom::WorkletLoadContext* ScriptLoadRequest::GetWorkletLoadContext() {
  MOZ_ASSERT(mLoadContext);
  return mLoadContext->AsWorkletContext();
}

ModuleLoadRequest* ScriptLoadRequest::AsModuleRequest() {
  MOZ_ASSERT(IsModuleRequest());
  return static_cast<ModuleLoadRequest*>(this);
}

const ModuleLoadRequest* ScriptLoadRequest::AsModuleRequest() const {
  MOZ_ASSERT(IsModuleRequest());
  return static_cast<const ModuleLoadRequest*>(this);
}

void ScriptLoadRequest::NoCacheEntryFound() {
  MOZ_ASSERT(IsCheckingCache());
  MOZ_ASSERT(mURI);
  // At the time where we check in the cache, the mBaseURL is not set, as this
  // is resolved by the network. Thus we use the mURI, for checking the cache
  // and later replace the mBaseURL using what the Channel->GetURI will provide.
  switch (mKind) {
    case ScriptKind::eClassic:
    case ScriptKind::eImportMap:
      mLoadedScript = new ClassicScript(mReferrerPolicy, mFetchOptions, mURI);
      break;
    case ScriptKind::eModule:
      mLoadedScript = new ModuleScript(mReferrerPolicy, mFetchOptions, mURI);
      break;
    case ScriptKind::eEvent:
      MOZ_ASSERT_UNREACHABLE("EventScripts are not using ScriptLoadRequest");
      break;
  }
  mState = State::Fetching;
}

void ScriptLoadRequest::SetPendingFetchingError() {
  MOZ_ASSERT(IsCheckingCache());
  mState = State::PendingFetchingError;
}

void ScriptLoadRequest::MarkForBytecodeEncoding(JSScript* aScript) {
  MOZ_ASSERT(!IsModuleRequest());
  MOZ_ASSERT(!IsMarkedForBytecodeEncoding());
  mScriptForBytecodeEncoding = aScript;
  HoldJSObjects(this);
}

bool ScriptLoadRequest::IsMarkedForBytecodeEncoding() const {
  if (IsModuleRequest()) {
    return AsModuleRequest()->IsModuleMarkedForBytecodeEncoding();
  }

  return !!mScriptForBytecodeEncoding;
}

//////////////////////////////////////////////////////////////
// ScriptLoadRequestList
//////////////////////////////////////////////////////////////

ScriptLoadRequestList::~ScriptLoadRequestList() { CancelRequestsAndClear(); }

void ScriptLoadRequestList::CancelRequestsAndClear() {
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

}  // namespace JS::loader

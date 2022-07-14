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

#include "js/OffThreadScriptCompilation.h"
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

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(ScriptFetchOptions, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(ScriptFetchOptions, Release)

ScriptFetchOptions::ScriptFetchOptions(
    mozilla::CORSMode aCORSMode, mozilla::dom::ReferrerPolicy aReferrerPolicy,
    nsIPrincipal* aTriggeringPrincipal, mozilla::dom::Element* aElement)
    : mCORSMode(aCORSMode),
      mReferrerPolicy(aReferrerPolicy),
      mTriggeringPrincipal(aTriggeringPrincipal),
      mElement(aElement) {}

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
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFetchOptions, mCacheInfo, mLoadContext)
  tmp->mScriptForBytecodeEncoding = nullptr;
  tmp->DropBytecodeCacheReferences();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ScriptLoadRequest)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFetchOptions, mCacheInfo, mLoadContext)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(ScriptLoadRequest)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mScriptForBytecodeEncoding)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

ScriptLoadRequest::ScriptLoadRequest(ScriptKind aKind, nsIURI* aURI,
                                     ScriptFetchOptions* aFetchOptions,
                                     const SRIMetadata& aIntegrity,
                                     nsIURI* aReferrer,
                                     LoadContextBase* aContext)
    : mKind(aKind),
      mState(State::Fetching),
      mFetchSourceOnly(false),
      mDataType(DataType::eUnknown),
      mFetchOptions(aFetchOptions),
      mIntegrity(aIntegrity),
      mReferrer(aReferrer),
      mScriptTextLength(0),
      mScriptBytecode(),
      mBytecodeOffset(0),
      mURI(aURI),
      mLoadContext(aContext) {
  MOZ_ASSERT(mFetchOptions);
  if (mLoadContext) {
    mLoadContext->SetRequest(this);
  }
}

ScriptLoadRequest::~ScriptLoadRequest() {
  if (IsMarkedForBytecodeEncoding()) {
    DropBytecodeCacheReferences();
  }
  mLoadContext = nullptr;
  DropJSObjects(this);
}

void ScriptLoadRequest::SetReady() {
  MOZ_ASSERT(!IsReadyToRun());
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

ModuleLoadRequest* ScriptLoadRequest::AsModuleRequest() {
  MOZ_ASSERT(IsModuleRequest());
  return static_cast<ModuleLoadRequest*>(this);
}

const ModuleLoadRequest* ScriptLoadRequest::AsModuleRequest() const {
  MOZ_ASSERT(IsModuleRequest());
  return static_cast<const ModuleLoadRequest*>(this);
}

void ScriptLoadRequest::SetBytecode() {
  MOZ_ASSERT(IsUnknownDataType());
  mDataType = DataType::eBytecode;
}

bool ScriptLoadRequest::IsUTF8ParsingEnabled() {
  if (HasLoadContext()) {
    if (mLoadContext->IsWindowContext()) {
      return mozilla::StaticPrefs::
          dom_script_loader_external_scripts_utf8_parsing_enabled();
    }
    if (mLoadContext->IsWorkerContext()) {
      return mozilla::StaticPrefs::
          dom_worker_script_loader_utf8_parsing_enabled();
    }
  }
  return false;
}

void ScriptLoadRequest::ClearScriptSource() {
  if (IsTextSource()) {
    ClearScriptText();
  }
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

nsresult ScriptLoadRequest::GetScriptSource(JSContext* aCx,
                                            MaybeSourceText* aMaybeSource) {
  // If there's no script text, we try to get it from the element
  if (HasScriptLoadContext() && GetScriptLoadContext()->mIsInline) {
    nsAutoString inlineData;
    GetScriptLoadContext()->GetScriptElement()->GetScriptText(inlineData);

    size_t nbytes = inlineData.Length() * sizeof(char16_t);
    JS::UniqueTwoByteChars chars(
        static_cast<char16_t*>(JS_malloc(aCx, nbytes)));
    if (!chars) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    memcpy(chars.get(), inlineData.get(), nbytes);

    SourceText<char16_t> srcBuf;
    if (!srcBuf.init(aCx, std::move(chars), inlineData.Length())) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    aMaybeSource->construct<SourceText<char16_t>>(std::move(srcBuf));
    return NS_OK;
  }

  size_t length = ScriptTextLength();
  if (IsUTF16Text()) {
    JS::UniqueTwoByteChars chars;
    chars.reset(ScriptText<char16_t>().extractOrCopyRawBuffer());
    if (!chars) {
      JS_ReportOutOfMemory(aCx);
      return NS_ERROR_OUT_OF_MEMORY;
    }

    SourceText<char16_t> srcBuf;
    if (!srcBuf.init(aCx, std::move(chars), length)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    aMaybeSource->construct<SourceText<char16_t>>(std::move(srcBuf));
    return NS_OK;
  }

  MOZ_ASSERT(IsUTF8Text());
  UniquePtr<Utf8Unit[], JS::FreePolicy> chars;
  chars.reset(ScriptText<Utf8Unit>().extractOrCopyRawBuffer());
  if (!chars) {
    JS_ReportOutOfMemory(aCx);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  SourceText<Utf8Unit> srcBuf;
  if (!srcBuf.init(aCx, std::move(chars), length)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  aMaybeSource->construct<SourceText<Utf8Unit>>(std::move(srcBuf));
  return NS_OK;
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

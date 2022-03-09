/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScriptLoadRequest.h"
#include "GeckoProfiler.h"

#include "mozilla/dom/Document.h"
#include "mozilla/dom/ScriptLoadContext.h"
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

NS_IMPL_CYCLE_COLLECTION(ScriptFetchOptions, mTriggeringPrincipal,
                         mWebExtGlobal)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(ScriptFetchOptions, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(ScriptFetchOptions, Release)

ScriptFetchOptions::ScriptFetchOptions(
    mozilla::CORSMode aCORSMode, mozilla::dom::ReferrerPolicy aReferrerPolicy,
    nsIPrincipal* aTriggeringPrincipal, nsIGlobalObject* aWebExtGlobal)
    : mCORSMode(aCORSMode),
      mReferrerPolicy(aReferrerPolicy),
      mTriggeringPrincipal(aTriggeringPrincipal),
      mWebExtGlobal(aWebExtGlobal) {
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
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFetchOptions, mCacheInfo, mLoadContext)
  tmp->mScript = nullptr;
  tmp->DropBytecodeCacheReferences();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ScriptLoadRequest)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFetchOptions, mCacheInfo, mLoadContext)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(ScriptLoadRequest)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mScript)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

ScriptLoadRequest::ScriptLoadRequest(ScriptKind aKind, nsIURI* aURI,
                                     ScriptFetchOptions* aFetchOptions,
                                     const SRIMetadata& aIntegrity,
                                     nsIURI* aReferrer,
                                     mozilla::dom::ScriptLoadContext* aContext)
    : mKind(aKind),
      mIsCanceled(false),
      mProgress(Progress::eLoading),
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
  if (mScript) {
    DropBytecodeCacheReferences();
  }
  mLoadContext = nullptr;
  DropJSObjects(this);
}

void ScriptLoadRequest::SetReady() {
  MOZ_ASSERT(mProgress != Progress::eReady);
  mProgress = Progress::eReady;
}

void ScriptLoadRequest::Cancel() {
  mIsCanceled = true;
  if (HasLoadContext()) {
    GetLoadContext()->MaybeCancelOffThreadScript();
  }
}

void ScriptLoadRequest::DropBytecodeCacheReferences() {
  mCacheInfo = nullptr;
  DropJSObjects(this);
}

ModuleLoadRequest* ScriptLoadRequest::AsModuleRequest() {
  MOZ_ASSERT(IsModuleRequest());
  return static_cast<ModuleLoadRequest*>(this);
}

void ScriptLoadRequest::SetBytecode() {
  MOZ_ASSERT(IsUnknownDataType());
  mDataType = DataType::eBytecode;
}

void ScriptLoadRequest::ClearScriptSource() {
  if (IsTextSource()) {
    ClearScriptText();
  }
}

void ScriptLoadRequest::SetScript(JSScript* aScript) {
  MOZ_ASSERT(!mScript);
  mScript = aScript;
  HoldJSObjects(this);
}

nsresult ScriptLoadRequest::GetScriptSource(JSContext* aCx,
                                            MaybeSourceText* aMaybeSource) {
  // If there's no script text, we try to get it from the element
  if (HasLoadContext() && GetLoadContext()->mIsInline) {
    nsAutoString inlineData;
    GetLoadContext()->GetScriptElement()->GetScriptText(inlineData);

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

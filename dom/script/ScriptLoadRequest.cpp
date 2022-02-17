/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScriptLoadRequest.h"
#include "GeckoProfiler.h"

#include "mozilla/dom/Document.h"
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
#include "ScriptSettings.h"

using JS::SourceText;

namespace mozilla {
namespace dom {

//////////////////////////////////////////////////////////////
// ScriptFetchOptions
//////////////////////////////////////////////////////////////

NS_IMPL_CYCLE_COLLECTION(ScriptFetchOptions, mTriggeringPrincipal,
                         mWebExtGlobal)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(ScriptFetchOptions, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(ScriptFetchOptions, Release)

ScriptFetchOptions::ScriptFetchOptions(mozilla::CORSMode aCORSMode,
                                       ReferrerPolicy aReferrerPolicy,
                                       nsIPrincipal* aTriggeringPrincipal,
                                       nsIGlobalObject* aWebExtGlobal)
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
                                     nsIURI* aReferrer)
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
      mLineNo(1) {
  MOZ_ASSERT(mFetchOptions);
}

//////////////////////////////////////////////////////////////
// DOMScriptLoadContext
//////////////////////////////////////////////////////////////

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMScriptLoadContext)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMScriptLoadContext)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMScriptLoadContext)

NS_IMPL_CYCLE_COLLECTION_CLASS(DOMScriptLoadContext)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DOMScriptLoadContext)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mElement)
  // XXX missing mLoadBlockedDocument ?
  if (Runnable* runnable = tmp->mRunnable.exchange(nullptr)) {
    runnable->Release();
  }
  tmp->MaybeUnblockOnload();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DOMScriptLoadContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLoadBlockedDocument, mRequest, mElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(DOMScriptLoadContext)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

DOMScriptLoadContext::DOMScriptLoadContext(Element* aElement,
                                           ScriptLoadRequest* aRequest)
    : mScriptMode(ScriptMode::eBlocking),
      mScriptFromHead(false),
      mIsInline(true),
      mInDeferList(false),
      mInAsyncList(false),
      mIsNonAsyncScriptInserted(false),
      mIsXSLT(false),
      mInCompilingList(false),
      mIsTracking(false),
      mWasCompiledOMT(false),
      mOffThreadToken(nullptr),
      mRunnable(nullptr),
      mIsPreload(false),
      mElement(aElement),
      mRequest(aRequest),
      mUnreportedPreloadError(NS_OK) {}

ScriptLoadRequest::~ScriptLoadRequest() {
  if (mScript) {
    DropBytecodeCacheReferences();
  }
  mLoadContext = nullptr;
  DropJSObjects(this);
}

DOMScriptLoadContext::~DOMScriptLoadContext() {
  // When speculative parsing is enabled, it is possible to off-main-thread
  // compile scripts that are never executed.  These should be cleaned up here
  // if they exist.
  mRequest = nullptr;
  MOZ_ASSERT_IF(
      !StaticPrefs::
          dom_script_loader_external_scripts_speculative_omt_parse_enabled(),
      !mOffThreadToken);

  MaybeCancelOffThreadScript();

  MaybeUnblockOnload();
}

void DOMScriptLoadContext::BlockOnload(Document* aDocument) {
  MOZ_ASSERT(!mLoadBlockedDocument);
  aDocument->BlockOnload();
  mLoadBlockedDocument = aDocument;
}

void DOMScriptLoadContext::MaybeUnblockOnload() {
  if (mLoadBlockedDocument) {
    mLoadBlockedDocument->UnblockOnload(false);
    mLoadBlockedDocument = nullptr;
  }
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

void DOMScriptLoadContext::MaybeCancelOffThreadScript() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mOffThreadToken) {
    return;
  }

  JSContext* cx = danger::GetJSContext();
  // Follow the same conditions as ScriptLoader::AttemptAsyncScriptCompile
  if (mRequest->IsModuleRequest()) {
    JS::CancelCompileModuleToStencilOffThread(cx, mOffThreadToken);
  } else if (mRequest->IsSource()) {
    JS::CancelCompileToStencilOffThread(cx, mOffThreadToken);
  } else {
    MOZ_ASSERT(mRequest->IsBytecode());
    JS::CancelDecodeStencilOffThread(cx, mOffThreadToken);
  }

  // Cancellation request above should guarantee removal of the parse task, so
  // releasing the runnable should be safe to do here.
  if (Runnable* runnable = mRunnable.exchange(nullptr)) {
    runnable->Release();
  }

  MaybeUnblockOnload();
  mOffThreadToken = nullptr;
}

void ScriptLoadRequest::DropBytecodeCacheReferences() {
  mCacheInfo = nullptr;
  DropJSObjects(this);
}

ModuleLoadRequest* ScriptLoadRequest::AsModuleRequest() {
  MOZ_ASSERT(IsModuleRequest());
  return static_cast<ModuleLoadRequest*>(this);
}

void DOMScriptLoadContext::SetScriptMode(bool aDeferAttr, bool aAsyncAttr,
                                         bool aLinkPreload) {
  if (aLinkPreload) {
    mScriptMode = ScriptMode::eLinkPreload;
  } else if (aAsyncAttr) {
    mScriptMode = ScriptMode::eAsync;
  } else if (aDeferAttr || mRequest->IsModuleRequest()) {
    mScriptMode = ScriptMode::eDeferred;
  } else {
    mScriptMode = ScriptMode::eBlocking;
  }
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

// static
void DOMScriptLoadContext::PrioritizeAsPreload(nsIChannel* aChannel) {
  if (nsCOMPtr<nsIClassOfService> cos = do_QueryInterface(aChannel)) {
    cos->AddClassFlags(nsIClassOfService::Unblocked);
  }
  if (nsCOMPtr<nsISupportsPriority> sp = do_QueryInterface(aChannel)) {
    sp->AdjustPriority(nsISupportsPriority::PRIORITY_HIGHEST);
  }
}

void DOMScriptLoadContext::PrioritizeAsPreload() {
  if (!IsLinkPreloadScript()) {
    // Do the prioritization only if this request has not already been created
    // as a preload.
    PrioritizeAsPreload(Channel());
  }
}

bool DOMScriptLoadContext::IsPreload() const {
  if (mRequest->IsModuleRequest() && !mRequest->IsTopLevel()) {
    ModuleLoadRequest* root = mRequest->AsModuleRequest()->GetRootModule();
    return root->mLoadContext->IsPreload();
  }

  MOZ_ASSERT_IF(mIsPreload, !GetScriptElement());
  return mIsPreload;
}

nsIScriptElement* DOMScriptLoadContext::GetScriptElement() const {
  if (mRequest->IsModuleRequest() && !mRequest->IsTopLevel()) {
    ModuleLoadRequest* root = mRequest->AsModuleRequest()->GetRootModule();
    return root->mLoadContext->GetScriptElement();
  }
  nsCOMPtr<nsIScriptElement> scriptElement = do_QueryInterface(mElement);
  return scriptElement;
}

void DOMScriptLoadContext::SetIsLoadRequest(nsIScriptElement* aElement) {
  MOZ_ASSERT(aElement);
  MOZ_ASSERT(!GetScriptElement());
  MOZ_ASSERT(IsPreload());
  // TODO: How to allow both to access fetch options
  mElement = do_QueryInterface(aElement);
  mIsPreload = false;
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

void DOMScriptLoadContext::GetProfilerLabel(nsACString& aOutString) {
  if (!profiler_is_active()) {
    aOutString.Append("<script> element");
    return;
  }
  aOutString.Append("<script");
  if (IsAsyncScript()) {
    aOutString.Append(" async");
  } else if (IsDeferredScript()) {
    aOutString.Append(" defer");
  }
  if (mRequest->IsModuleRequest()) {
    aOutString.Append(" type=\"module\"");
  }

  nsAutoCString url;
  if (mRequest->mURI) {
    mRequest->mURI->GetAsciiSpec(url);
  } else {
    url = "<unknown>";
  }

  if (mIsInline) {
    if (GetParserCreated() != NOT_FROM_PARSER) {
      aOutString.Append("> inline at line ");
      aOutString.AppendInt(mRequest->mLineNo);
      aOutString.Append(" of ");
    } else {
      aOutString.Append("> inline (dynamically created) in ");
    }
    aOutString.Append(url);
  } else {
    aOutString.Append(" src=\"");
    aOutString.Append(url);
    aOutString.Append("\">");
  }
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

}  // namespace dom
}  // namespace mozilla

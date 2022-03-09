/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoProfiler.h"

#include "mozilla/dom/Document.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/Unused.h"
#include "mozilla/Utf8.h"  // mozilla::Utf8Unit

#include "js/OffThreadScriptCompilation.h"
#include "js/SourceText.h"
#include "js/loader/ScriptLoadRequest.h"

#include "ModuleLoadRequest.h"
#include "nsContentUtils.h"
#include "nsICacheInfoChannel.h"
#include "nsIClassOfService.h"
#include "nsISupportsPriority.h"

namespace mozilla {
namespace dom {

//////////////////////////////////////////////////////////////
// ScriptLoadContext
//////////////////////////////////////////////////////////////

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ScriptLoadContext)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(ScriptLoadContext)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ScriptLoadContext)

NS_IMPL_CYCLE_COLLECTION_CLASS(ScriptLoadContext)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ScriptLoadContext)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLoadBlockedDocument, mRequest, mElement)
  if (Runnable* runnable = tmp->mRunnable.exchange(nullptr)) {
    runnable->Release();
  }
  tmp->MaybeUnblockOnload();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ScriptLoadContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLoadBlockedDocument, mRequest, mElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(ScriptLoadContext)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

ScriptLoadContext::ScriptLoadContext(Element* aElement)
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
      mLineNo(1),
      mIsPreload(false),
      mElement(aElement),
      mRequest(nullptr),
      mUnreportedPreloadError(NS_OK) {}

ScriptLoadContext::~ScriptLoadContext() {
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

void ScriptLoadContext::BlockOnload(Document* aDocument) {
  MOZ_ASSERT(!mLoadBlockedDocument);
  aDocument->BlockOnload();
  mLoadBlockedDocument = aDocument;
}

void ScriptLoadContext::MaybeUnblockOnload() {
  if (mLoadBlockedDocument) {
    mLoadBlockedDocument->UnblockOnload(false);
    mLoadBlockedDocument = nullptr;
  }
}

void ScriptLoadContext::MaybeCancelOffThreadScript() {
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

void ScriptLoadContext::SetRequest(JS::loader::ScriptLoadRequest* aRequest) {
  MOZ_ASSERT(!mRequest);
  mRequest = aRequest;
}

void ScriptLoadContext::SetScriptMode(bool aDeferAttr, bool aAsyncAttr,
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

// static
void ScriptLoadContext::PrioritizeAsPreload(nsIChannel* aChannel) {
  if (nsCOMPtr<nsIClassOfService> cos = do_QueryInterface(aChannel)) {
    cos->AddClassFlags(nsIClassOfService::Unblocked);
  }
  if (nsCOMPtr<nsISupportsPriority> sp = do_QueryInterface(aChannel)) {
    sp->AdjustPriority(nsISupportsPriority::PRIORITY_HIGHEST);
  }
}

void ScriptLoadContext::PrioritizeAsPreload() {
  if (!IsLinkPreloadScript()) {
    // Do the prioritization only if this request has not already been created
    // as a preload.
    PrioritizeAsPreload(Channel());
  }
}

bool ScriptLoadContext::IsPreload() const {
  if (mRequest->IsModuleRequest() && !mRequest->IsTopLevel()) {
    JS::loader::ModuleLoadRequest* root =
        mRequest->AsModuleRequest()->GetRootModule();
    return root->GetLoadContext()->IsPreload();
  }

  MOZ_ASSERT_IF(mIsPreload, !GetScriptElement());
  return mIsPreload;
}

nsIGlobalObject* ScriptLoadContext::GetWebExtGlobal() const {
  return mRequest->mFetchOptions->mWebExtGlobal;
}

bool ScriptLoadContext::CompileStarted() const {
  return mRequest->InCompilingStage() ||
         (mRequest->IsReadyToRun() && mWasCompiledOMT);
}

nsIScriptElement* ScriptLoadContext::GetScriptElement() const {
  if (mRequest->IsModuleRequest() && !mRequest->IsTopLevel()) {
    JS::loader::ModuleLoadRequest* root =
        mRequest->AsModuleRequest()->GetRootModule();
    return root->GetLoadContext()->GetScriptElement();
  }
  nsCOMPtr<nsIScriptElement> scriptElement = do_QueryInterface(mElement);
  return scriptElement;
}

void ScriptLoadContext::SetIsLoadRequest(nsIScriptElement* aElement) {
  MOZ_ASSERT(aElement);
  MOZ_ASSERT(!GetScriptElement());
  MOZ_ASSERT(IsPreload());
  // TODO: How to allow both to access fetch options
  mElement = do_QueryInterface(aElement);
  mIsPreload = false;
}

void ScriptLoadContext::GetProfilerLabel(nsACString& aOutString) {
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
      aOutString.AppendInt(mLineNo);
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

}  // namespace dom
}  // namespace mozilla

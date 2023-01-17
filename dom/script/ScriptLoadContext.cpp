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
#include "js/loader/LoadContextBase.h"
#include "js/loader/ModuleLoadRequest.h"

#include "ScriptLoadContext.h"
#include "ModuleLoadRequest.h"
#include "nsContentUtils.h"
#include "nsICacheInfoChannel.h"
#include "nsIClassOfService.h"
#include "nsISupportsPriority.h"

namespace mozilla::dom {

//////////////////////////////////////////////////////////////
// ScriptLoadContext
//////////////////////////////////////////////////////////////

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ScriptLoadContext)
NS_INTERFACE_MAP_END_INHERITING(JS::loader::LoadContextBase)

NS_IMPL_CYCLE_COLLECTION_CLASS(ScriptLoadContext)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(ScriptLoadContext,
                                                JS::loader::LoadContextBase)
  if (Runnable* runnable = tmp->mRunnable.exchange(nullptr)) {
    runnable->Release();
  }
  tmp->MaybeUnblockOnload();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ScriptLoadContext,
                                                  JS::loader::LoadContextBase)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLoadBlockedDocument)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(ScriptLoadContext, JS::loader::LoadContextBase)
NS_IMPL_RELEASE_INHERITED(ScriptLoadContext, JS::loader::LoadContextBase)

ScriptLoadContext::ScriptLoadContext()
    : JS::loader::LoadContextBase(JS::loader::ContextKind::Window),
      mScriptMode(ScriptMode::eBlocking),
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
  JS::CancelOffThreadToken(cx, mOffThreadToken);

  // Cancellation request above should guarantee removal of the parse task, so
  // releasing the runnable should be safe to do here.
  if (Runnable* runnable = mRunnable.exchange(nullptr)) {
    runnable->Release();
  }

  MaybeUnblockOnload();
  mOffThreadToken = nullptr;
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
    return root->GetScriptLoadContext()->IsPreload();
  }

  MOZ_ASSERT_IF(mIsPreload, !GetScriptElement());
  return mIsPreload;
}

bool ScriptLoadContext::CompileStarted() const {
  return mRequest->IsCompiling() ||
         (mRequest->IsReadyToRun() && mWasCompiledOMT);
}

nsIScriptElement* ScriptLoadContext::GetScriptElement() const {
  nsCOMPtr<nsIScriptElement> scriptElement =
      do_QueryInterface(mRequest->mFetchOptions->mElement);
  return scriptElement;
}

void ScriptLoadContext::SetIsLoadRequest(nsIScriptElement* aElement) {
  MOZ_ASSERT(aElement);
  MOZ_ASSERT(!GetScriptElement());
  MOZ_ASSERT(IsPreload());
  // We are not tracking our own element, and are relying on the one in
  // FetchOptions.
  mRequest->mFetchOptions->mElement = do_QueryInterface(aElement);
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

}  // namespace mozilla::dom

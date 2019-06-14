/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>

#include "mozilla/Attributes.h"
#include "mozilla/Likely.h"
#include "mozilla/Maybe.h"
#include "mozilla/dom/CSPEvalChecker.h"
#include "mozilla/dom/FunctionBinding.h"
#include "mozilla/dom/LoadedScript.h"
#include "mozilla/dom/ScriptLoader.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsError.h"
#include "nsGlobalWindow.h"
#include "mozilla/dom/Document.h"
#include "nsIScriptTimeoutHandler.h"
#include "nsIXPConnect.h"
#include "nsJSUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

// Our JS nsIScriptTimeoutHandler implementation.
class nsJSScriptTimeoutHandler final : public nsIScriptTimeoutHandler {
 public:
  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsJSScriptTimeoutHandler)

  nsJSScriptTimeoutHandler();
  nsJSScriptTimeoutHandler(JSContext* aCx, nsGlobalWindowInner* aWindow,
                           LoadedScript* aInitiatingScript,
                           const nsAString& aExpression, bool* aAllowEval,
                           ErrorResult& aError);
  nsJSScriptTimeoutHandler(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                           const nsAString& aExpression, bool* aAllowEval,
                           ErrorResult& aRv);

  virtual const nsAString& GetHandlerText() override;

  virtual bool Call(const char* /* unused */) override { return false; }

  virtual void GetLocation(const char** aFileName, uint32_t* aLineNo,
                           uint32_t* aColumn) override {
    *aFileName = mFileName.get();
    *aLineNo = mLineNo;
    *aColumn = mColumn;
  }

  virtual LoadedScript* GetInitiatingScript() override {
    return mInitiatingScript;
  }

  virtual void MarkForCC() override {}

 private:
  ~nsJSScriptTimeoutHandler() = default;

  void Init(JSContext* aCx);

  // filename, line number and JS language version string of the
  // caller of setTimeout()
  nsCString mFileName;
  uint32_t mLineNo;
  uint32_t mColumn;

  // The expression to evaluate or function to call. If mFunction is non-null
  // it should be used, else use mExpr.
  nsString mExpr;

  // Initiating script for use when evaluating mExpr on the main thread.
  RefPtr<LoadedScript> mInitiatingScript;
};

// nsJSScriptTimeoutHandler
// QueryInterface implementation for nsJSScriptTimeoutHandler
NS_IMPL_CYCLE_COLLECTION_CLASS(nsJSScriptTimeoutHandler)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsJSScriptTimeoutHandler)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mInitiatingScript)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INTERNAL(nsJSScriptTimeoutHandler)
  if (MOZ_UNLIKELY(cb.WantDebugInfo())) {
    nsAutoCString name("nsJSScriptTimeoutHandler");
    name.AppendLiteral(" [");
    name.Append(tmp->mFileName);
    name.Append(':');
    name.AppendInt(tmp->mLineNo);
    name.Append(':');
    name.AppendInt(tmp->mColumn);
    name.Append(']');
    cb.DescribeRefCountedNode(tmp->mRefCnt.get(), name.get());
  } else {
    NS_IMPL_CYCLE_COLLECTION_DESCRIBE(nsJSScriptTimeoutHandler,
                                      tmp->mRefCnt.get())
  }

  if (tmp->mInitiatingScript) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mInitiatingScript)
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsJSScriptTimeoutHandler)
  NS_INTERFACE_MAP_ENTRY(nsIScriptTimeoutHandler)
  NS_INTERFACE_MAP_ENTRY(nsITimeoutHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsJSScriptTimeoutHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsJSScriptTimeoutHandler)

nsJSScriptTimeoutHandler::nsJSScriptTimeoutHandler() : mLineNo(0), mColumn(0) {}

nsJSScriptTimeoutHandler::nsJSScriptTimeoutHandler(
    JSContext* aCx, nsGlobalWindowInner* aWindow,
    LoadedScript* aInitiatingScript, const nsAString& aExpression,
    bool* aAllowEval, ErrorResult& aError)
    : mLineNo(0),
      mColumn(0),
      mExpr(aExpression),
      mInitiatingScript(aInitiatingScript) {
  if (!aWindow->GetContextInternal() || !aWindow->HasJSGlobal()) {
    // This window was already closed, or never properly initialized,
    // don't let a timer be scheduled on such a window.
    aError.Throw(NS_ERROR_NOT_INITIALIZED);
    return;
  }

  aError =
      CSPEvalChecker::CheckForWindow(aCx, aWindow, aExpression, aAllowEval);
  if (NS_WARN_IF(aError.Failed()) || !*aAllowEval) {
    return;
  }

  Init(aCx);
}

nsJSScriptTimeoutHandler::nsJSScriptTimeoutHandler(
    JSContext* aCx, WorkerPrivate* aWorkerPrivate, const nsAString& aExpression,
    bool* aAllowEval, ErrorResult& aError)
    : mLineNo(0), mColumn(0), mExpr(aExpression) {
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();

  aError = CSPEvalChecker::CheckForWorker(aCx, aWorkerPrivate, aExpression,
                                          aAllowEval);
  if (NS_WARN_IF(aError.Failed()) || !*aAllowEval) {
    return;
  }

  Init(aCx);
}

void nsJSScriptTimeoutHandler::Init(JSContext* aCx) {
  // Get the calling location.
  nsJSUtils::GetCallingLocation(aCx, mFileName, &mLineNo, &mColumn);
}

const nsAString& nsJSScriptTimeoutHandler::GetHandlerText() { return mExpr; }

already_AddRefed<nsIScriptTimeoutHandler> NS_CreateJSTimeoutHandler(
    JSContext* aCx, nsGlobalWindowInner* aWindow, const nsAString& aExpression,
    ErrorResult& aError) {
  LoadedScript* script = ScriptLoader::GetActiveScript(aCx);

  bool allowEval = false;
  RefPtr<nsJSScriptTimeoutHandler> handler = new nsJSScriptTimeoutHandler(
      aCx, aWindow, script, aExpression, &allowEval, aError);
  if (aError.Failed() || !allowEval) {
    return nullptr;
  }

  return handler.forget();
}

already_AddRefed<nsIScriptTimeoutHandler> NS_CreateJSTimeoutHandler(
    JSContext* aCx, WorkerPrivate* aWorkerPrivate, const nsAString& aExpression,
    ErrorResult& aRv) {
  bool allowEval = false;
  RefPtr<nsJSScriptTimeoutHandler> handler = new nsJSScriptTimeoutHandler(
      aCx, aWorkerPrivate, aExpression, &allowEval, aRv);
  if (aRv.Failed() || !allowEval) {
    return nullptr;
  }

  return handler.forget();
}

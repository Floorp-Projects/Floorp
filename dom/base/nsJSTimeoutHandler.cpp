/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>

#include "mozilla/Attributes.h"
#include "mozilla/Likely.h"
#include "mozilla/Maybe.h"
#include "mozilla/dom/FunctionBinding.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsError.h"
#include "nsGlobalWindow.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIDocument.h"
#include "nsIScriptTimeoutHandler.h"
#include "nsIXPConnect.h"
#include "nsJSUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

// Our JS nsIScriptTimeoutHandler implementation.
class nsJSScriptTimeoutHandler final : public nsIScriptTimeoutHandler
{
public:
  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsJSScriptTimeoutHandler)

  nsJSScriptTimeoutHandler();
  // This will call SwapElements on aArguments with an empty array.
  nsJSScriptTimeoutHandler(JSContext* aCx, nsGlobalWindowInner* aWindow,
                           Function& aFunction,
                           nsTArray<JS::Heap<JS::Value>>&& aArguments,
                           ErrorResult& aError);
  nsJSScriptTimeoutHandler(JSContext* aCx, nsGlobalWindowInner* aWindow,
                           const nsAString& aExpression, bool* aAllowEval,
                           ErrorResult& aError);
  nsJSScriptTimeoutHandler(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                           Function& aFunction,
                           nsTArray<JS::Heap<JS::Value>>&& aArguments);
  nsJSScriptTimeoutHandler(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                           const nsAString& aExpression);

  virtual const nsAString& GetHandlerText() override;

  virtual Function* GetCallback() override
  {
    return mFunction;
  }

  virtual const nsTArray<JS::Value>& GetArgs() override
  {
    return mArgs;
  }

  virtual nsresult Call() override
  {
    return NS_OK;
  }

  virtual void GetLocation(const char** aFileName, uint32_t* aLineNo,
                           uint32_t* aColumn) override
  {
    *aFileName = mFileName.get();
    *aLineNo = mLineNo;
    *aColumn = mColumn;
  }

  virtual void MarkForCC() override
  {
    if (mFunction) {
      mFunction->MarkForCC();
    }
  }

  void ReleaseJSObjects();

private:
  ~nsJSScriptTimeoutHandler();

  void Init(JSContext* aCx,
            nsTArray<JS::Heap<JS::Value>>&& aArguments);
  void Init(JSContext* aCx);

  // filename, line number and JS language version string of the
  // caller of setTimeout()
  nsCString mFileName;
  uint32_t mLineNo;
  uint32_t mColumn;
  nsTArray<JS::Heap<JS::Value>> mArgs;

  // The expression to evaluate or function to call. If mFunction is non-null
  // it should be used, else use mExpr.
  nsString mExpr;
  RefPtr<Function> mFunction;
};


// nsJSScriptTimeoutHandler
// QueryInterface implementation for nsJSScriptTimeoutHandler
NS_IMPL_CYCLE_COLLECTION_CLASS(nsJSScriptTimeoutHandler)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsJSScriptTimeoutHandler)
  tmp->ReleaseJSObjects();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INTERNAL(nsJSScriptTimeoutHandler)
  if (MOZ_UNLIKELY(cb.WantDebugInfo())) {
    nsAutoCString name("nsJSScriptTimeoutHandler");
    if (tmp->mFunction) {
      JSObject* obj = tmp->mFunction->CallablePreserveColor();
      JSFunction* fun = JS_GetObjectFunction(js::UncheckedUnwrapWithoutExpose(obj));
      if (fun && JS_GetFunctionId(fun)) {
        JSFlatString *funId = JS_ASSERT_STRING_IS_FLAT(JS_GetFunctionId(fun));
        size_t size = 1 + JS_PutEscapedFlatString(nullptr, 0, funId, 0);
        char *funIdName = new char[size];
        if (funIdName) {
          JS_PutEscapedFlatString(funIdName, size, funId, 0);
          name.AppendLiteral(" [");
          name.Append(funIdName);
          delete[] funIdName;
          name.Append(']');
        }
      }
    } else {
      name.AppendLiteral(" [");
      name.Append(tmp->mFileName);
      name.Append(':');
      name.AppendInt(tmp->mLineNo);
      name.Append(':');
      name.AppendInt(tmp->mColumn);
      name.Append(']');
    }
    cb.DescribeRefCountedNode(tmp->mRefCnt.get(), name.get());
  }
  else {
    NS_IMPL_CYCLE_COLLECTION_DESCRIBE(nsJSScriptTimeoutHandler,
                                      tmp->mRefCnt.get())
  }

  if (tmp->mFunction) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFunction)
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsJSScriptTimeoutHandler)
  for (uint32_t i = 0; i < tmp->mArgs.Length(); ++i) {
    NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mArgs[i])
  }
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsJSScriptTimeoutHandler)
  NS_INTERFACE_MAP_ENTRY(nsIScriptTimeoutHandler)
  NS_INTERFACE_MAP_ENTRY(nsITimeoutHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsJSScriptTimeoutHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsJSScriptTimeoutHandler)

static bool
CheckCSPForEval(JSContext* aCx, nsGlobalWindowInner* aWindow, ErrorResult& aError)
{
  // if CSP is enabled, and setTimeout/setInterval was called with a string,
  // disable the registration and log an error
  nsCOMPtr<nsIDocument> doc = aWindow->GetExtantDoc();
  if (!doc) {
    // if there's no document, we don't have to do anything.
    return true;
  }

  nsCOMPtr<nsIContentSecurityPolicy> csp;
  aError = doc->NodePrincipal()->GetCsp(getter_AddRefs(csp));
  if (aError.Failed()) {
    return false;
  }

  if (!csp) {
    return true;
  }

  bool allowsEval = true;
  bool reportViolation = false;
  aError = csp->GetAllowsEval(&reportViolation, &allowsEval);
  if (aError.Failed()) {
    return false;
  }

  if (reportViolation) {
    // TODO : need actual script sample in violation report.
    NS_NAMED_LITERAL_STRING(scriptSample,
                            "call to eval() or related function blocked by CSP");

    // Get the calling location.
    uint32_t lineNum = 0;
    uint32_t columnNum = 0;
    nsAutoString fileNameString;
    if (!nsJSUtils::GetCallingLocation(aCx, fileNameString, &lineNum,
                                       &columnNum)) {
      fileNameString.AssignLiteral("unknown");
    }

    csp->LogViolationDetails(nsIContentSecurityPolicy::VIOLATION_TYPE_EVAL,
                             fileNameString, scriptSample, lineNum, columnNum,
                             EmptyString(), EmptyString());
  }

  return allowsEval;
}

nsJSScriptTimeoutHandler::nsJSScriptTimeoutHandler()
  : mLineNo(0)
  , mColumn(0)
{
}

nsJSScriptTimeoutHandler::nsJSScriptTimeoutHandler(JSContext* aCx,
                                                   nsGlobalWindowInner *aWindow,
                                                   Function& aFunction,
                                                   nsTArray<JS::Heap<JS::Value>>&& aArguments,
                                                   ErrorResult& aError)
  : mLineNo(0)
  , mColumn(0)
  , mFunction(&aFunction)
{
  if (!aWindow->GetContextInternal() || !aWindow->FastGetGlobalJSObject()) {
    // This window was already closed, or never properly initialized,
    // don't let a timer be scheduled on such a window.
    aError.Throw(NS_ERROR_NOT_INITIALIZED);
    return;
  }

  Init(aCx, std::move(aArguments));
}

nsJSScriptTimeoutHandler::nsJSScriptTimeoutHandler(JSContext* aCx,
                                                   nsGlobalWindowInner *aWindow,
                                                   const nsAString& aExpression,
                                                   bool* aAllowEval,
                                                   ErrorResult& aError)
  : mLineNo(0)
  , mColumn(0)
  , mExpr(aExpression)
{
  if (!aWindow->GetContextInternal() || !aWindow->FastGetGlobalJSObject()) {
    // This window was already closed, or never properly initialized,
    // don't let a timer be scheduled on such a window.
    aError.Throw(NS_ERROR_NOT_INITIALIZED);
    return;
  }

  *aAllowEval = CheckCSPForEval(aCx, aWindow, aError);
  if (aError.Failed() || !*aAllowEval) {
    return;
  }

  Init(aCx);
}

nsJSScriptTimeoutHandler::nsJSScriptTimeoutHandler(JSContext* aCx,
                                                   WorkerPrivate* aWorkerPrivate,
                                                   Function& aFunction,
                                                   nsTArray<JS::Heap<JS::Value>>&& aArguments)
  : mLineNo(0)
  , mColumn(0)
  , mFunction(&aFunction)
{
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();

  Init(aCx, std::move(aArguments));
}

nsJSScriptTimeoutHandler::nsJSScriptTimeoutHandler(JSContext* aCx,
                                                   WorkerPrivate* aWorkerPrivate,
                                                   const nsAString& aExpression)
  : mLineNo(0)
  , mColumn(0)
  , mExpr(aExpression)
{
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();

  Init(aCx);
}

nsJSScriptTimeoutHandler::~nsJSScriptTimeoutHandler()
{
  ReleaseJSObjects();
}

void
nsJSScriptTimeoutHandler::Init(JSContext* aCx,
                               nsTArray<JS::Heap<JS::Value>>&& aArguments)
{
  mozilla::HoldJSObjects(this);
  mArgs = std::move(aArguments);

  Init(aCx);
}

void
nsJSScriptTimeoutHandler::Init(JSContext* aCx)
{
  // Get the calling location.
  nsJSUtils::GetCallingLocation(aCx, mFileName, &mLineNo, &mColumn);
}

void
nsJSScriptTimeoutHandler::ReleaseJSObjects()
{
  if (mFunction) {
    mFunction = nullptr;
    mArgs.Clear();
    mozilla::DropJSObjects(this);
  }
}

const nsAString&
nsJSScriptTimeoutHandler::GetHandlerText()
{
  NS_ASSERTION(!mFunction, "No expression, so no handler text!");
  return mExpr;
}

already_AddRefed<nsIScriptTimeoutHandler>
NS_CreateJSTimeoutHandler(JSContext *aCx, nsGlobalWindowInner *aWindow,
                          Function& aFunction,
                          const Sequence<JS::Value>& aArguments,
                          ErrorResult& aError)
{
  nsTArray<JS::Heap<JS::Value>> args;
  if (!args.AppendElements(aArguments, fallible)) {
    aError.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  RefPtr<nsJSScriptTimeoutHandler> handler =
    new nsJSScriptTimeoutHandler(aCx, aWindow, aFunction, std::move(args), aError);
  return aError.Failed() ? nullptr : handler.forget();
}

already_AddRefed<nsIScriptTimeoutHandler>
NS_CreateJSTimeoutHandler(JSContext* aCx, nsGlobalWindowInner *aWindow,
                          const nsAString& aExpression, ErrorResult& aError)
{
  bool allowEval = false;
  RefPtr<nsJSScriptTimeoutHandler> handler =
    new nsJSScriptTimeoutHandler(aCx, aWindow, aExpression, &allowEval, aError);
  if (aError.Failed() || !allowEval) {
    return nullptr;
  }

  return handler.forget();
}

already_AddRefed<nsIScriptTimeoutHandler>
NS_CreateJSTimeoutHandler(JSContext *aCx, WorkerPrivate* aWorkerPrivate,
                          Function& aFunction,
                          const Sequence<JS::Value>& aArguments,
                          ErrorResult& aError)
{
  nsTArray<JS::Heap<JS::Value>> args;
  if (!args.AppendElements(aArguments, fallible)) {
    aError.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  RefPtr<nsJSScriptTimeoutHandler> handler =
    new nsJSScriptTimeoutHandler(aCx, aWorkerPrivate, aFunction, std::move(args));
  return handler.forget();
}

already_AddRefed<nsIScriptTimeoutHandler>
NS_CreateJSTimeoutHandler(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                          const nsAString& aExpression)
{
  RefPtr<nsJSScriptTimeoutHandler> handler =
    new nsJSScriptTimeoutHandler(aCx, aWorkerPrivate, aExpression);
  return handler.forget();
}

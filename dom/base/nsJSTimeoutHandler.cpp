/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIScriptTimeoutHandler.h"
#include "nsIXPConnect.h"
#include "nsJSUtils.h"
#include "nsContentUtils.h"
#include "nsError.h"
#include "nsGlobalWindow.h"
#include "nsIContentSecurityPolicy.h"
#include "mozilla/Attributes.h"
#include "mozilla/Likely.h"
#include <algorithm>
#include "mozilla/dom/FunctionBinding.h"
#include "nsAXPCNativeCallContext.h"

static const char kSetIntervalStr[] = "setInterval";
static const char kSetTimeoutStr[] = "setTimeout";

using namespace mozilla;
using namespace mozilla::dom;

// Our JS nsIScriptTimeoutHandler implementation.
class nsJSScriptTimeoutHandler MOZ_FINAL : public nsIScriptTimeoutHandler
{
public:
  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsJSScriptTimeoutHandler)

  nsJSScriptTimeoutHandler();
  // This will call SwapElements on aArguments with an empty array.
  nsJSScriptTimeoutHandler(nsGlobalWindow *aWindow, Function& aFunction,
                           FallibleTArray<JS::Heap<JS::Value> >& aArguments,
                           ErrorResult& aError);
  nsJSScriptTimeoutHandler(JSContext* aCx, nsGlobalWindow *aWindow,
                           const nsAString& aExpression, bool* aAllowEval,
                           ErrorResult& aError);
  ~nsJSScriptTimeoutHandler();

  virtual const PRUnichar *GetHandlerText();
  virtual Function* GetCallback()
  {
    return mFunction;
  }
  virtual void GetLocation(const char **aFileName, uint32_t *aLineNo)
  {
    *aFileName = mFileName.get();
    *aLineNo = mLineNo;
  }

  virtual const nsTArray<JS::Value>& GetArgs()
  {
    return mArgs;
  }

  nsresult Init(nsGlobalWindow *aWindow, bool *aIsInterval,
                int32_t *aInterval, bool* aAllowEval);

  void ReleaseJSObjects();

private:
  // filename, line number and JS language version string of the
  // caller of setTimeout()
  nsCString mFileName;
  uint32_t mLineNo;
  nsTArray<JS::Heap<JS::Value> > mArgs;

  // The expression to evaluate or function to call. If mFunction is non-null
  // it should be used, else use mExpr.
  nsString mExpr;
  nsRefPtr<Function> mFunction;
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
      JSFunction* fun =
        JS_GetObjectFunction(js::UncheckedUnwrap(tmp->mFunction->Callable()));
      if (fun && JS_GetFunctionId(fun)) {
        JSFlatString *funId = JS_ASSERT_STRING_IS_FLAT(JS_GetFunctionId(fun));
        size_t size = 1 + JS_PutEscapedFlatString(nullptr, 0, funId, 0);
        char *funIdName = new char[size];
        if (funIdName) {
          JS_PutEscapedFlatString(funIdName, size, funId, 0);
          name.AppendLiteral(" [");
          name.Append(funIdName);
          delete[] funIdName;
          name.AppendLiteral("]");
        }
      }
    } else {
      name.AppendLiteral(" [");
      name.Append(tmp->mFileName);
      name.AppendLiteral(":");
      name.AppendInt(tmp->mLineNo);
      name.AppendLiteral("]");
    }
    cb.DescribeRefCountedNode(tmp->mRefCnt.get(), name.get());
  }
  else {
    NS_IMPL_CYCLE_COLLECTION_DESCRIBE(nsJSScriptTimeoutHandler,
                                      tmp->mRefCnt.get())
  }

  if (tmp->mFunction) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFunction)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsJSScriptTimeoutHandler)
  for (uint32_t i = 0; i < tmp->mArgs.Length(); ++i) {
    NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mArgs[i])
  }
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsJSScriptTimeoutHandler)
  NS_INTERFACE_MAP_ENTRY(nsIScriptTimeoutHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsJSScriptTimeoutHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsJSScriptTimeoutHandler)

static bool
CheckCSPForEval(JSContext* aCx, nsGlobalWindow* aWindow, ErrorResult& aError)
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
    const char *fileName;
    nsAutoString fileNameString;
    if (nsJSUtils::GetCallingLocation(aCx, &fileName, &lineNum)) {
      AppendUTF8toUTF16(fileName, fileNameString);
    } else {
      fileNameString.AssignLiteral("unknown");
    }

    csp->LogViolationDetails(nsIContentSecurityPolicy::VIOLATION_TYPE_EVAL,
                             fileNameString, scriptSample, lineNum);
  }

  return allowsEval;
}

nsJSScriptTimeoutHandler::nsJSScriptTimeoutHandler() :
  mLineNo(0)
{
}

nsJSScriptTimeoutHandler::nsJSScriptTimeoutHandler(nsGlobalWindow *aWindow,
                                                   Function& aFunction,
                                                   FallibleTArray<JS::Heap<JS::Value> >& aArguments,
                                                   ErrorResult& aError) :
  mLineNo(0),
  mFunction(&aFunction)
{
  if (!aWindow->GetContextInternal() || !aWindow->FastGetGlobalJSObject()) {
    // This window was already closed, or never properly initialized,
    // don't let a timer be scheduled on such a window.
    aError.Throw(NS_ERROR_NOT_INITIALIZED);
    return;
  }

  mozilla::HoldJSObjects(this);
  mArgs.SwapElements(aArguments);
}

nsJSScriptTimeoutHandler::nsJSScriptTimeoutHandler(JSContext* aCx,
                                                   nsGlobalWindow *aWindow,
                                                   const nsAString& aExpression,
                                                   bool* aAllowEval,
                                                   ErrorResult& aError) :
  mLineNo(0),
  mExpr(aExpression)
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

  // Get the calling location.
  const char *filename;
  if (nsJSUtils::GetCallingLocation(aCx, &filename, &mLineNo)) {
    mFileName.Assign(filename);
  }
}

nsJSScriptTimeoutHandler::~nsJSScriptTimeoutHandler()
{
  ReleaseJSObjects();
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

nsresult
nsJSScriptTimeoutHandler::Init(nsGlobalWindow *aWindow, bool *aIsInterval,
                               int32_t *aInterval, bool *aAllowEval)
{
  if (!aWindow->GetContextInternal() || !aWindow->FastGetGlobalJSObject()) {
    // This window was already closed, or never properly initialized,
    // don't let a timer be scheduled on such a window.

    return NS_ERROR_NOT_INITIALIZED;
  }

  nsAXPCNativeCallContext *ncc = nullptr;
  nsresult rv = nsContentUtils::XPConnect()->
    GetCurrentNativeCallContext(&ncc);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!ncc)
    return NS_ERROR_NOT_AVAILABLE;

  JSContext *cx = nullptr;

  rv = ncc->GetJSContext(&cx);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t argc;
  JS::Value *argv = nullptr;

  ncc->GetArgc(&argc);
  ncc->GetArgvPtr(&argv);

  JS::Rooted<JSFlatString*> expr(cx);
  JS::Rooted<JSObject*> funobj(cx);

  if (argc < 1) {
    ::JS_ReportError(cx, "Function %s requires at least 2 parameter",
                     *aIsInterval ? kSetIntervalStr : kSetTimeoutStr);
    return NS_ERROR_DOM_TYPE_ERR;
  }

  int32_t interval = 0;
  if (argc > 1) {
    JS::Rooted<JS::Value> arg(cx, argv[1]);

    if (!JS::ToInt32(cx, arg, &interval)) {
      ::JS_ReportError(cx,
                       "Second argument to %s must be a millisecond interval",
                       aIsInterval ? kSetIntervalStr : kSetTimeoutStr);
      return NS_ERROR_DOM_TYPE_ERR;
    }
  }

  if (argc == 1) {
    // If no interval was specified, treat this like a timeout, to avoid
    // setting an interval of 0 milliseconds.
    *aIsInterval = false;
  }

  switch (::JS_TypeOfValue(cx, argv[0])) {
  case JSTYPE_FUNCTION:
    funobj = JSVAL_TO_OBJECT(argv[0]);
    break;

  case JSTYPE_STRING:
  case JSTYPE_OBJECT:
    {
      JSString *str = ::JS_ValueToString(cx, argv[0]);
      if (!str)
        return NS_ERROR_OUT_OF_MEMORY;

      expr = ::JS_FlattenString(cx, str);
      if (!expr)
          return NS_ERROR_OUT_OF_MEMORY;

      argv[0] = STRING_TO_JSVAL(str);
    }
    break;

  default:
    ::JS_ReportError(cx, "useless %s call (missing quotes around argument?)",
                     *aIsInterval ? kSetIntervalStr : kSetTimeoutStr);

    // Return an error that nsGlobalWindow can recognize and turn into NS_OK.
    return NS_ERROR_DOM_TYPE_ERR;
  }

  if (expr) {
    // if CSP is enabled, and setTimeout/setInterval was called with a string,
    // disable the registration and log an error
    ErrorResult error;
    *aAllowEval = CheckCSPForEval(cx, aWindow, error);
    if (error.Failed() || !*aAllowEval) {
      return error.ErrorCode();
    }

    mExpr.Append(JS_GetFlatStringChars(expr),
                 JS_GetStringLength(JS_FORGET_STRING_FLATNESS(expr)));

    // Get the calling location.
    const char *filename;
    if (nsJSUtils::GetCallingLocation(cx, &filename, &mLineNo)) {
      mFileName.Assign(filename);
    }
  } else if (funobj) {
    *aAllowEval = true;

    mozilla::HoldJSObjects(this);

    mFunction = new Function(funobj);

    // Create our arg array.  argc is the number of arguments passed
    // to setTimeout or setInterval; the first two are our callback
    // and the delay, so only arguments after that need to go in our
    // array.
    // std::max(argc - 2, 0) wouldn't work right because argc is unsigned.
    uint32_t argCount = std::max(argc, 2u) - 2;

    FallibleTArray<JS::Heap<JS::Value> > args;
    if (!args.SetCapacity(argCount)) {
      // No need to drop here, since we already have a non-null mFunction
      return NS_ERROR_OUT_OF_MEMORY;
    }
    for (uint32_t idx = 0; idx < argCount; ++idx) {
      *args.AppendElement() = argv[idx + 2];
    }
    args.SwapElements(mArgs);
  } else {
    NS_WARNING("No func and no expr - why are we here?");
  }
  *aInterval = interval;
  return NS_OK;
}

const PRUnichar *
nsJSScriptTimeoutHandler::GetHandlerText()
{
  NS_ASSERTION(!mFunction, "No expression, so no handler text!");
  return mExpr.get();
}

nsresult NS_CreateJSTimeoutHandler(nsGlobalWindow *aWindow,
                                   bool *aIsInterval,
                                   int32_t *aInterval,
                                   nsIScriptTimeoutHandler **aRet)
{
  *aRet = nullptr;
  nsRefPtr<nsJSScriptTimeoutHandler> handler = new nsJSScriptTimeoutHandler();
  bool allowEval;
  nsresult rv = handler->Init(aWindow, aIsInterval, aInterval, &allowEval);
  if (NS_FAILED(rv) || !allowEval) {
    return rv;
  }

  handler.forget(aRet);

  return NS_OK;
}

already_AddRefed<nsIScriptTimeoutHandler>
NS_CreateJSTimeoutHandler(nsGlobalWindow *aWindow, Function& aFunction,
                          const Sequence<JS::Value>& aArguments,
                          ErrorResult& aError)
{
  FallibleTArray<JS::Heap<JS::Value> > args;
  if (!args.AppendElements(aArguments)) {
    aError.Throw(NS_ERROR_OUT_OF_MEMORY);
    return 0;
  }

  nsRefPtr<nsJSScriptTimeoutHandler> handler =
    new nsJSScriptTimeoutHandler(aWindow, aFunction, args, aError);
  return aError.Failed() ? nullptr : handler.forget();
}

already_AddRefed<nsIScriptTimeoutHandler>
NS_CreateJSTimeoutHandler(JSContext* aCx, nsGlobalWindow *aWindow,
                          const nsAString& aExpression, ErrorResult& aError)
{
  ErrorResult rv;
  bool allowEval = false;
  nsRefPtr<nsJSScriptTimeoutHandler> handler =
    new nsJSScriptTimeoutHandler(aCx, aWindow, aExpression, &allowEval, rv);
  if (rv.Failed() || !allowEval) {
    return nullptr;
  }

  return handler.forget();
}

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsIScriptContext.h"
#include "nsIDocument.h"
#include "nsIArray.h"
#include "nsIScriptTimeoutHandler.h"
#include "nsIXPConnect.h"
#include "nsIJSRuntimeService.h"
#include "nsJSUtils.h"
#include "nsDOMJSUtils.h"
#include "nsContentUtils.h"
#include "nsJSEnvironment.h"
#include "nsServiceManagerUtils.h"
#include "nsError.h"
#include "nsGlobalWindow.h"
#include "nsIContentSecurityPolicy.h"
#include "nsAlgorithm.h"
#include "mozilla/Attributes.h"
#include "mozilla/Likely.h"
#include <algorithm>
#include "mozilla/dom/FunctionBinding.h"

static const char kSetIntervalStr[] = "setInterval";
static const char kSetTimeoutStr[] = "setTimeout";

using namespace mozilla::dom;

// Our JS nsIScriptTimeoutHandler implementation.
class nsJSScriptTimeoutHandler MOZ_FINAL : public nsIScriptTimeoutHandler
{
public:
  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsJSScriptTimeoutHandler)

  nsJSScriptTimeoutHandler();
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
                int32_t *aInterval);

  void ReleaseJSObjects();

private:
  // filename, line number and JS language version string of the
  // caller of setTimeout()
  nsCString mFileName;
  uint32_t mLineNo;
  nsTArray<JS::Value> mArgs;

  // The JS expression to evaluate or function to call, if !mExpr
  JSFlatString *mExpr;
  nsRefPtr<Function> mFunction;
};


// nsJSScriptTimeoutHandler
// QueryInterface implementation for nsJSScriptTimeoutHandler
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsJSScriptTimeoutHandler)
  tmp->ReleaseJSObjects();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INTERNAL(nsJSScriptTimeoutHandler)
  if (MOZ_UNLIKELY(cb.WantDebugInfo())) {
    nsAutoCString name("nsJSScriptTimeoutHandler");
    if (tmp->mExpr) {
      name.AppendLiteral(" [");
      name.Append(tmp->mFileName);
      name.AppendLiteral(":");
      name.AppendInt(tmp->mLineNo);
      name.AppendLiteral("]");
    }
    else if (tmp->mFunction) {
      JSFunction* fun =
        JS_GetObjectFunction(js::UncheckedUnwrap(tmp->mFunction->Callable()));
      if (fun && JS_GetFunctionId(fun)) {
        JSFlatString *funId = JS_ASSERT_STRING_IS_FLAT(JS_GetFunctionId(fun));
        size_t size = 1 + JS_PutEscapedFlatString(NULL, 0, funId, 0);
        char *funIdName = new char[size];
        if (funIdName) {
          JS_PutEscapedFlatString(funIdName, size, funId, 0);
          name.AppendLiteral(" [");
          name.Append(funIdName);
          delete[] funIdName;
          name.AppendLiteral("]");
        }
      }
    }
    cb.DescribeRefCountedNode(tmp->mRefCnt.get(), name.get());
  }
  else {
    NS_IMPL_CYCLE_COLLECTION_DESCRIBE(nsJSScriptTimeoutHandler,
                                      tmp->mRefCnt.get())
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFunction)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsJSScriptTimeoutHandler)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mExpr)
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

nsJSScriptTimeoutHandler::nsJSScriptTimeoutHandler() :
  mLineNo(0),
  mExpr(nullptr)
{
}

nsJSScriptTimeoutHandler::~nsJSScriptTimeoutHandler()
{
  ReleaseJSObjects();
}

void
nsJSScriptTimeoutHandler::ReleaseJSObjects()
{
  if (mExpr) {
    mExpr = nullptr;
  } else {
    mFunction = nullptr;
    mArgs.Clear();
  }
  NS_DROP_JS_OBJECTS(this, nsJSScriptTimeoutHandler);
}

nsresult
nsJSScriptTimeoutHandler::Init(nsGlobalWindow *aWindow, bool *aIsInterval,
                               int32_t *aInterval)
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

  JSAutoRequest ar(cx);

  if (argc < 1) {
    ::JS_ReportError(cx, "Function %s requires at least 2 parameter",
                     *aIsInterval ? kSetIntervalStr : kSetTimeoutStr);
    return NS_ERROR_DOM_TYPE_ERR;
  }

  int32_t interval = 0;
  if (argc > 1 && !::JS_ValueToECMAInt32(cx, argv[1], &interval)) {
    ::JS_ReportError(cx,
                     "Second argument to %s must be a millisecond interval",
                     aIsInterval ? kSetIntervalStr : kSetTimeoutStr);
    return NS_ERROR_DOM_TYPE_ERR;
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
    // if CSP is enabled, and setTimeout/setInterval was called with a string
    // or object, disable the registration and log an error
    nsCOMPtr<nsIDocument> doc = aWindow->GetExtantDoc();

    if (doc) {
      nsCOMPtr<nsIContentSecurityPolicy> csp;
      nsresult rv = doc->NodePrincipal()->GetCsp(getter_AddRefs(csp));
      NS_ENSURE_SUCCESS(rv, rv);

      if (csp) {
        bool allowsEval = true;
        bool reportViolation = false;
        rv = csp->GetAllowsEval(&reportViolation, &allowsEval);
        NS_ENSURE_SUCCESS(rv, rv);

        if (reportViolation) {
          // TODO : FIX DATA in violation report.
          NS_NAMED_LITERAL_STRING(scriptSample, "call to eval() or related function blocked by CSP");

          // Get the calling location.
          uint32_t lineNum = 0;
          const char *fileName;
          nsAutoCString aFileName;
          if (nsJSUtils::GetCallingLocation(cx, &fileName, &lineNum)) {
            aFileName.Assign(fileName);
          } else {
            aFileName.Assign("unknown");
          }

          csp->LogViolationDetails(nsIContentSecurityPolicy::VIOLATION_TYPE_EVAL,
                                  NS_ConvertUTF8toUTF16(aFileName),
                                  scriptSample,
                                  lineNum);
        }

        if (!allowsEval) {
          // Note: Our only caller knows to turn NS_ERROR_DOM_TYPE_ERR into NS_OK.
          return NS_ERROR_DOM_TYPE_ERR;
        }
      }
    } // if there's no document, we don't have to do anything.

    NS_HOLD_JS_OBJECTS(this, nsJSScriptTimeoutHandler);

    mExpr = expr;

    // Get the calling location.
    const char *filename;
    if (nsJSUtils::GetCallingLocation(cx, &filename, &mLineNo)) {
      mFileName.Assign(filename);
    }
  } else if (funobj) {
    NS_HOLD_JS_OBJECTS(this, nsJSScriptTimeoutHandler);

    mFunction = new Function(funobj);

    // Create our arg array.  argc is the number of arguments passed
    // to setTimeout or setInterval; the first two are our callback
    // and the delay, so only arguments after that need to go in our
    // array.
    // std::max(argc - 2, 0) wouldn't work right because argc is unsigned.
    uint32_t argCount = std::max(argc, 2u) - 2;
    FallibleTArray<JS::Value> args;
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
  NS_ASSERTION(mExpr, "No expression, so no handler text!");
  return ::JS_GetFlatStringChars(mExpr);
}

nsresult NS_CreateJSTimeoutHandler(nsGlobalWindow *aWindow,
                                   bool *aIsInterval,
                                   int32_t *aInterval,
                                   nsIScriptTimeoutHandler **aRet)
{
  *aRet = nullptr;
  nsRefPtr<nsJSScriptTimeoutHandler> handler = new nsJSScriptTimeoutHandler();
  nsresult rv = handler->Init(aWindow, aIsInterval, aInterval);
  if (NS_FAILED(rv)) {
    return rv;
  }

  handler.forget(aRet);

  return NS_OK;
}

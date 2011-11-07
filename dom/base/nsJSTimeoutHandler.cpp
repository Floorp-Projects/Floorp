/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Mark Hammond <mhammond@skippinet.com.au>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCOMPtr.h"
#include "nsIScriptContext.h"
#include "nsIArray.h"
#include "nsIScriptTimeoutHandler.h"
#include "nsIXPConnect.h"
#include "nsIJSRuntimeService.h"
#include "nsJSUtils.h"
#include "nsDOMJSUtils.h"
#include "nsContentUtils.h"
#include "nsJSEnvironment.h"
#include "nsServiceManagerUtils.h"
#include "nsDOMError.h"
#include "nsGlobalWindow.h"
#include "nsIContentSecurityPolicy.h"

static const char kSetIntervalStr[] = "setInterval";
static const char kSetTimeoutStr[] = "setTimeout";

// Our JS nsIScriptTimeoutHandler implementation.
class nsJSScriptTimeoutHandler: public nsIScriptTimeoutHandler
{
public:
  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsJSScriptTimeoutHandler)

  nsJSScriptTimeoutHandler();
  ~nsJSScriptTimeoutHandler();

  virtual const PRUnichar *GetHandlerText();
  virtual void *GetScriptObject() {
    return mFunObj;
  }
  virtual void GetLocation(const char **aFileName, PRUint32 *aLineNo) {
    *aFileName = mFileName.get();
    *aLineNo = mLineNo;
  }

  virtual PRUint32 GetScriptTypeID() {
        return nsIProgrammingLanguage::JAVASCRIPT;
  }
  virtual PRUint32 GetScriptVersion() {
        return mVersion;
  }

  virtual nsIArray *GetArgv() {
    return mArgv;
  }
  // Called by the timeout mechanism so the secret 'lateness' arg can be
  // added.
  virtual void SetLateness(PRIntervalTime aHowLate);

  nsresult Init(nsGlobalWindow *aWindow, bool *aIsInterval,
                PRInt32 *aInterval);

  void ReleaseJSObjects();

private:

  nsCOMPtr<nsIScriptContext> mContext;

  // filename, line number and JS language version string of the
  // caller of setTimeout()
  nsCString mFileName;
  PRUint32 mLineNo;
  PRUint32 mVersion;
  nsCOMPtr<nsIArray> mArgv;

  // The JS expression to evaluate or function to call, if !mExpr
  JSFlatString *mExpr;
  JSObject *mFunObj;
};


// nsJSScriptTimeoutHandler
// QueryInterface implementation for nsJSScriptTimeoutHandler
NS_IMPL_CYCLE_COLLECTION_CLASS(nsJSScriptTimeoutHandler)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsJSScriptTimeoutHandler)
  tmp->ReleaseJSObjects();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INTERNAL(nsJSScriptTimeoutHandler)
  if (NS_UNLIKELY(cb.WantDebugInfo())) {
    nsCAutoString foo("nsJSScriptTimeoutHandler");
    if (tmp->mExpr) {
      foo.AppendLiteral(" [");
      foo.Append(tmp->mFileName);
      foo.AppendLiteral(":");
      foo.AppendInt(tmp->mLineNo);
      foo.AppendLiteral("]");
    }
    else if (tmp->mFunObj) {
      JSFunction* fun = JS_GetObjectFunction(tmp->mFunObj);
      if (fun && JS_GetFunctionId(fun)) {
        JSFlatString *funId = JS_ASSERT_STRING_IS_FLAT(JS_GetFunctionId(fun));
        size_t size = 1 + JS_PutEscapedFlatString(NULL, 0, funId, 0);
        char *name = new char[size];
        if (name) {
          JS_PutEscapedFlatString(name, size, funId, 0);
          foo.AppendLiteral(" [");
          foo.Append(name);
          delete[] name;
          foo.AppendLiteral("]");
        }
      }
    }
    cb.DescribeRefCountedNode(tmp->mRefCnt.get(),
                              sizeof(nsJSScriptTimeoutHandler), foo.get());
  }
  else {
    NS_IMPL_CYCLE_COLLECTION_DESCRIBE(nsJSScriptTimeoutHandler,
                                      tmp->mRefCnt.get())
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mArgv)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsJSScriptTimeoutHandler)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mExpr)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mFunObj)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsJSScriptTimeoutHandler)
  NS_INTERFACE_MAP_ENTRY(nsIScriptTimeoutHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsJSScriptTimeoutHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsJSScriptTimeoutHandler)

nsJSScriptTimeoutHandler::nsJSScriptTimeoutHandler() :
  mLineNo(0),
  mVersion(nsnull),
  mExpr(nsnull),
  mFunObj(nsnull)
{
}

nsJSScriptTimeoutHandler::~nsJSScriptTimeoutHandler()
{
  ReleaseJSObjects();
}

void
nsJSScriptTimeoutHandler::ReleaseJSObjects()
{
  if (mExpr || mFunObj) {
    if (mExpr) {
      NS_DROP_JS_OBJECTS(this, nsJSScriptTimeoutHandler);
      mExpr = nsnull;
    } else if (mFunObj) {
      NS_DROP_JS_OBJECTS(this, nsJSScriptTimeoutHandler);
      mFunObj = nsnull;
    } else {
      NS_WARNING("No func and no expr - roots may not have been removed");
    }
  }
}

nsresult
nsJSScriptTimeoutHandler::Init(nsGlobalWindow *aWindow, bool *aIsInterval,
                               PRInt32 *aInterval)
{
  mContext = aWindow->GetContextInternal();
  if (!mContext) {
    // This window was already closed, or never properly initialized,
    // don't let a timer be scheduled on such a window.

    return NS_ERROR_NOT_INITIALIZED;
  }

  nsAXPCNativeCallContext *ncc = nsnull;
  nsresult rv = nsContentUtils::XPConnect()->
    GetCurrentNativeCallContext(&ncc);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!ncc)
    return NS_ERROR_NOT_AVAILABLE;

  JSContext *cx = nsnull;

  rv = ncc->GetJSContext(&cx);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 argc;
  jsval *argv = nsnull;

  ncc->GetArgc(&argc);
  ncc->GetArgvPtr(&argv);

  JSFlatString *expr = nsnull;
  JSObject *funobj = nsnull;
  int32 interval = 0;

  JSAutoRequest ar(cx);

  if (argc < 1) {
    ::JS_ReportError(cx, "Function %s requires at least 2 parameter",
                     *aIsInterval ? kSetIntervalStr : kSetTimeoutStr);
    return NS_ERROR_DOM_TYPE_ERR;
  }

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
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(aWindow->GetExtantDocument());

    if (doc) {
      nsCOMPtr<nsIContentSecurityPolicy> csp;
      nsresult rv = doc->NodePrincipal()->GetCsp(getter_AddRefs(csp));
      NS_ENSURE_SUCCESS(rv, rv);

      if (csp) {
        bool allowsEval;
        // this call will send violation reports as warranted (and return true if
        // reportOnly is set).
        rv = csp->GetAllowsEval(&allowsEval);
        NS_ENSURE_SUCCESS(rv, rv);

        if (!allowsEval) {
          ::JS_ReportError(cx, "call to %s blocked by CSP",
                            *aIsInterval ? kSetIntervalStr : kSetTimeoutStr);

          // Note: Our only caller knows to turn NS_ERROR_DOM_TYPE_ERR into NS_OK.
          return NS_ERROR_DOM_TYPE_ERR;
        }
      }
    } // if there's no document, we don't have to do anything.

    rv = NS_HOLD_JS_OBJECTS(this, nsJSScriptTimeoutHandler);
    NS_ENSURE_SUCCESS(rv, rv);

    mExpr = expr;

    // Get the calling location.
    const char *filename;
    if (nsJSUtils::GetCallingLocation(cx, &filename, &mLineNo)) {
      mFileName.Assign(filename);
    }
  } else if (funobj) {
    rv = NS_HOLD_JS_OBJECTS(this, nsJSScriptTimeoutHandler);
    NS_ENSURE_SUCCESS(rv, rv);

    mFunObj = funobj;

    // Create our arg array - leave an extra slot for a secret final argument
    // that indicates to the called function how "late" the timeout is.  We
    // will fill that in when SetLateness is called.
    nsCOMPtr<nsIArray> array;
    rv = NS_CreateJSArgv(cx, (argc > 1) ? argc - 1 : argc, nsnull,
                         getter_AddRefs(array));
    if (NS_FAILED(rv)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    PRUint32 dummy;
    jsval *jsargv = nsnull;
    nsCOMPtr<nsIJSArgArray> jsarray(do_QueryInterface(array));
    jsarray->GetArgs(&dummy, reinterpret_cast<void **>(&jsargv));

    // must have worked - we own the impl! :)
    NS_ASSERTION(jsargv, "No argv!");
    for (PRInt32 i = 2; (PRUint32)i < argc; ++i) {
      jsargv[i - 2] = argv[i];
    }
    // final arg slot remains null, array has rooted vals.
    mArgv = array;
  } else {
    NS_WARNING("No func and no expr - why are we here?");
  }
  *aInterval = interval;
  return NS_OK;
}

void nsJSScriptTimeoutHandler::SetLateness(PRIntervalTime aHowLate)
{
  nsCOMPtr<nsIJSArgArray> jsarray(do_QueryInterface(mArgv));
  if (jsarray) {
    PRUint32 argc;
    jsval *jsargv;
    nsresult rv = jsarray->GetArgs(&argc, reinterpret_cast<void **>(&jsargv));
    if (NS_SUCCEEDED(rv) && jsargv && argc)
      jsargv[argc-1] = INT_TO_JSVAL((jsint) aHowLate);
  } else {
    NS_ERROR("How can our argv not handle this?");
  }
}

const PRUnichar *
nsJSScriptTimeoutHandler::GetHandlerText()
{
  NS_ASSERTION(mExpr, "No expression, so no handler text!");
  return ::JS_GetFlatStringChars(mExpr);
}

nsresult NS_CreateJSTimeoutHandler(nsGlobalWindow *aWindow,
                                   bool *aIsInterval,
                                   PRInt32 *aInterval,
                                   nsIScriptTimeoutHandler **aRet)
{
  *aRet = nsnull;
  nsJSScriptTimeoutHandler *handler = new nsJSScriptTimeoutHandler();
  if (!handler)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv = handler->Init(aWindow, aIsInterval, aInterval);
  if (NS_FAILED(rv)) {
    delete handler;
    return rv;
  }

  NS_ADDREF(*aRet = handler);

  return NS_OK;
}

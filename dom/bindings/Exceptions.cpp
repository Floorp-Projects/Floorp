/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Exceptions.h"

#include "js/GCAPI.h"
#include "js/TypeDecls.h"
#include "jsapi.h"
#include "jsprf.h"
#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsIProgrammingLanguage.h"
#include "nsPIDOMWindow.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "XPCWrapper.h"
#include "WorkerPrivate.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

bool
ThrowExceptionObject(JSContext* aCx, nsIException* aException)
{
  // See if we really have an Exception.
  nsCOMPtr<Exception> exception = do_QueryInterface(aException);
  if (exception) {
    return ThrowExceptionObject(aCx, exception);
  }

  // We only have an nsIException (probably an XPCWrappedJS).  Fall back on old
  // wrapping.
  MOZ_ASSERT(NS_IsMainThread());

  JS::Rooted<JSObject*> glob(aCx, JS::CurrentGlobalOrNull(aCx));
  if (!glob) {
    // XXXbz Can this really be null here?
    return false;
  }

  JS::Rooted<JS::Value> val(aCx);
  if (!WrapObject(aCx, aException, &NS_GET_IID(nsIException), &val)) {
    return false;
  }

  JS_SetPendingException(aCx, val);

  return true;
}

bool
ThrowExceptionObject(JSContext* aCx, Exception* aException)
{
  JS::Rooted<JS::Value> thrown(aCx);

  // If we stored the original thrown JS value in the exception
  // (see XPCConvert::ConstructException) and we are in a web context
  // (i.e., not chrome), rethrow the original value. This only applies to JS
  // implemented components so we only need to check for this on the main
  // thread.
  if (NS_IsMainThread() && !nsContentUtils::IsCallerChrome() &&
      aException->StealJSVal(thrown.address())) {
    // Now check for the case when thrown is a number which matches
    // aException->GetResult().  This would indicate that what actually got
    // thrown was an nsresult value.  In that situation, we should go back
    // through dom::Throw with that nsresult value, because it will make sure to
    // create the right sort of Exception or DOMException, with the right
    // global.
    if (thrown.isNumber()) {
      nsresult exceptionResult;
      if (NS_SUCCEEDED(aException->GetResult(&exceptionResult)) &&
          double(exceptionResult) == thrown.toNumber()) {
        // The return value semantics here are a bit weird.  Throw() always
        // returns false.  But we want to return true if we managed to throw an
        // exception (otherwise our caller will assume OOM)... which Throw()
        // always will.  So we just return true unconditionally.
        Throw(aCx, exceptionResult);
        return true;
      }
    }
    if (!JS_WrapValue(aCx, &thrown)) {
      return false;
    }
    JS_SetPendingException(aCx, thrown);
    return true;
  }

  JS::Rooted<JSObject*> glob(aCx, JS::CurrentGlobalOrNull(aCx));
  if (!glob) {
    // XXXbz Can this actually be null here?
    return false;
  }

  if (!GetOrCreateDOMReflector(aCx, aException, &thrown)) {
    return false;
  }

  JS_SetPendingException(aCx, thrown);
  return true;
}

bool
Throw(JSContext* aCx, nsresult aRv, const nsACString& aMessage)
{
  if (aRv == NS_ERROR_UNCATCHABLE_EXCEPTION) {
    // Nuke any existing exception on aCx, to make sure we're uncatchable.
    JS_ClearPendingException(aCx);
    return false;
  }

  if (JS_IsExceptionPending(aCx)) {
    // Don't clobber the existing exception.
    return false;
  }

  CycleCollectedJSRuntime* runtime = CycleCollectedJSRuntime::Get();
  nsCOMPtr<nsIException> existingException = runtime->GetPendingException();
  // Make sure to clear the pending exception now.  Either we're going to reuse
  // it (and we already grabbed it), or we plan to throw something else and this
  // pending exception is no longer relevant.
  runtime->SetPendingException(nullptr);

  // Ignore the pending exception if we have a non-default message passed in.
  if (aMessage.IsEmpty() && existingException) {
    nsresult nr;
    if (NS_SUCCEEDED(existingException->GetResult(&nr)) &&
        aRv == nr) {
      // Reuse the existing exception.
      if (!ThrowExceptionObject(aCx, existingException)) {
        // If we weren't able to throw an exception we're
        // most likely out of memory
        JS_ReportOutOfMemory(aCx);
      }
      return false;
    }
  }

  RefPtr<Exception> finalException = CreateException(aCx, aRv, aMessage);

  MOZ_ASSERT(finalException);
  if (!ThrowExceptionObject(aCx, finalException)) {
    // If we weren't able to throw an exception we're
    // most likely out of memory
    JS_ReportOutOfMemory(aCx);
  }

  return false;
}

void
ThrowAndReport(nsPIDOMWindowInner* aWindow, nsresult aRv)
{
  MOZ_ASSERT(aRv != NS_ERROR_UNCATCHABLE_EXCEPTION,
             "Doesn't make sense to report uncatchable exceptions!");
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(aWindow))) {
    return;
  }

  Throw(jsapi.cx(), aRv);
}

already_AddRefed<Exception>
CreateException(JSContext* aCx, nsresult aRv, const nsACString& aMessage)
{
  // Do we use DOM exceptions for this error code?
  switch (NS_ERROR_GET_MODULE(aRv)) {
  case NS_ERROR_MODULE_DOM:
  case NS_ERROR_MODULE_SVG:
  case NS_ERROR_MODULE_DOM_XPATH:
  case NS_ERROR_MODULE_DOM_INDEXEDDB:
  case NS_ERROR_MODULE_DOM_FILEHANDLE:
  case NS_ERROR_MODULE_DOM_BLUETOOTH:
  case NS_ERROR_MODULE_DOM_ANIM:
  case NS_ERROR_MODULE_DOM_PUSH:
    if (aMessage.IsEmpty()) {
      return DOMException::Create(aRv);
    }
    return DOMException::Create(aRv, aMessage);
  default:
    break;
  }

  // If not, use the default.
  RefPtr<Exception> exception =
    new Exception(aMessage, aRv, EmptyCString(), nullptr, nullptr);
  return exception.forget();
}

already_AddRefed<nsIStackFrame>
GetCurrentJSStack(int32_t aMaxDepth)
{
  // is there a current context available?
  JSContext* cx = nsContentUtils::GetCurrentJSContextForThread();

  if (!cx || !js::GetContextCompartment(cx)) {
    return nullptr;
  }

  return dom::exceptions::CreateStack(cx, aMaxDepth);
}

namespace exceptions {

class JSStackFrame : public nsIStackFrame
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(JSStackFrame)
  NS_DECL_NSISTACKFRAME

  // aStack must not be null.
  explicit JSStackFrame(JS::Handle<JSObject*> aStack);

protected:
  int32_t GetLineno(JSContext* aCx);

  int32_t GetColNo(JSContext* aCx);

private:
  virtual ~JSStackFrame();

  JS::Heap<JSObject*> mStack;
  nsString mFormattedStack;

  nsCOMPtr<nsIStackFrame> mCaller;
  nsCOMPtr<nsIStackFrame> mAsyncCaller;
  nsString mFilename;
  nsString mFunname;
  nsString mAsyncCause;
  int32_t mLineno;
  int32_t mColNo;

  bool mFilenameInitialized;
  bool mFunnameInitialized;
  bool mLinenoInitialized;
  bool mColNoInitialized;
  bool mAsyncCauseInitialized;
  bool mAsyncCallerInitialized;
  bool mCallerInitialized;
  bool mFormattedStackInitialized;
};

JSStackFrame::JSStackFrame(JS::Handle<JSObject*> aStack)
  : mStack(aStack)
  , mLineno(0)
  , mColNo(0)
  , mFilenameInitialized(false)
  , mFunnameInitialized(false)
  , mLinenoInitialized(false)
  , mColNoInitialized(false)
  , mAsyncCauseInitialized(false)
  , mAsyncCallerInitialized(false)
  , mCallerInitialized(false)
  , mFormattedStackInitialized(false)
{
  MOZ_ASSERT(mStack);

  mozilla::HoldJSObjects(this);
}

JSStackFrame::~JSStackFrame()
{
  mozilla::DropJSObjects(this);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(JSStackFrame)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(JSStackFrame)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCaller)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAsyncCaller)
  tmp->mStack = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(JSStackFrame)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCaller)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mAsyncCaller)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(JSStackFrame)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mStack)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(JSStackFrame)
NS_IMPL_CYCLE_COLLECTING_RELEASE(JSStackFrame)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(JSStackFrame)
  NS_INTERFACE_MAP_ENTRY(nsIStackFrame)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMETHODIMP JSStackFrame::GetLanguage(uint32_t* aLanguage)
{
  *aLanguage = nsIProgrammingLanguage::JAVASCRIPT;
  return NS_OK;
}

NS_IMETHODIMP JSStackFrame::GetLanguageName(nsACString& aLanguageName)
{
  aLanguageName.AssignLiteral("JavaScript");
  return NS_OK;
}

// Helper method to get the value of a stack property, if it's not already
// cached.  This will make sure we skip the cache if the access is happening
// over Xrays.
//
// @argument aStack the stack we're working with; must be non-null.
// @argument aPropGetter the getter function to call.
// @argument aIsCached whether we've cached this property's value before.
//
// @argument [out] aCanCache whether the value can get cached.
// @argument [out] aUseCachedValue if true, just use the cached value.
// @argument [out] aValue the value we got from the stack.
template<typename ReturnType, typename GetterOutParamType>
static void
GetValueIfNotCached(JSContext* aCx, JSObject* aStack,
                    JS::SavedFrameResult (*aPropGetter)(JSContext*,
                                                        JS::Handle<JSObject*>,
                                                        GetterOutParamType,
                                                        JS::SavedFrameSelfHosted),
                    bool aIsCached, bool* aCanCache, bool* aUseCachedValue,
                    ReturnType aValue)
{
  MOZ_ASSERT(aStack);

  JS::Rooted<JSObject*> stack(aCx, aStack);
  // Allow caching if aCx and stack are same-compartment.  Otherwise take the
  // slow path.
  *aCanCache = js::GetContextCompartment(aCx) == js::GetObjectCompartment(stack);
  if (*aCanCache && aIsCached) {
    *aUseCachedValue = true;
    return;
  }

  *aUseCachedValue = false;
  JS::ExposeObjectToActiveJS(stack);

  aPropGetter(aCx, stack, aValue, JS::SavedFrameSelfHosted::Exclude);
}

NS_IMETHODIMP JSStackFrame::GetFilename(JSContext* aCx, nsAString& aFilename)
{
  if (!mStack) {
    aFilename.Truncate();
    return NS_OK;
  }

  JS::Rooted<JSString*> filename(aCx);
  bool canCache = false, useCachedValue = false;
  GetValueIfNotCached(aCx, mStack, JS::GetSavedFrameSource,
                      mFilenameInitialized,
                      &canCache, &useCachedValue, &filename);
  if (useCachedValue) {
    aFilename = mFilename;
    return NS_OK;
  }

  nsAutoJSString str;
  if (!str.init(aCx, filename)) {
    JS_ClearPendingException(aCx);
    aFilename.Truncate();
    return NS_OK;
  }
  aFilename = str;

  if (canCache) {
    mFilename = str;
    mFilenameInitialized = true;
  }

  return NS_OK;
}

NS_IMETHODIMP JSStackFrame::GetName(JSContext* aCx, nsAString& aFunction)
{
  if (!mStack) {
    aFunction.Truncate();
    return NS_OK;
  }

  JS::Rooted<JSString*> name(aCx);
  bool canCache = false, useCachedValue = false;
  GetValueIfNotCached(aCx, mStack, JS::GetSavedFrameFunctionDisplayName,
                      mFunnameInitialized, &canCache, &useCachedValue,
                      &name);

  if (useCachedValue) {
    aFunction = mFunname;
    return NS_OK;
  }

  if (name) {
    nsAutoJSString str;
    if (!str.init(aCx, name)) {
      JS_ClearPendingException(aCx);
      aFunction.Truncate();
      return NS_OK;
    }
    aFunction = str;
  } else {
    aFunction.SetIsVoid(true);
  }

  if (canCache) {
    mFunname = aFunction;
    mFunnameInitialized = true;
  }

  return NS_OK;
}

int32_t
JSStackFrame::GetLineno(JSContext* aCx)
{
  if (!mStack) {
    return 0;
  }

  uint32_t line;
  bool canCache = false, useCachedValue = false;
  GetValueIfNotCached(aCx, mStack, JS::GetSavedFrameLine, mLinenoInitialized,
                      &canCache, &useCachedValue, &line);

  if (useCachedValue) {
    return mLineno;
  }

  if (canCache) {
    mLineno = line;
    mLinenoInitialized = true;
  }

  return line;
}

NS_IMETHODIMP JSStackFrame::GetLineNumber(JSContext* aCx, int32_t* aLineNumber)
{
  *aLineNumber = GetLineno(aCx);
  return NS_OK;
}

int32_t
JSStackFrame::GetColNo(JSContext* aCx)
{
  if (!mStack) {
    return 0;
  }

  uint32_t col;
  bool canCache = false, useCachedValue = false;
  GetValueIfNotCached(aCx, mStack, JS::GetSavedFrameColumn, mColNoInitialized,
                      &canCache, &useCachedValue, &col);

  if (useCachedValue) {
    return mColNo;
  }

  if (canCache) {
    mColNo = col;
    mColNoInitialized = true;
  }

  return col;
}

NS_IMETHODIMP JSStackFrame::GetColumnNumber(JSContext* aCx,
                                            int32_t* aColumnNumber)
{
  *aColumnNumber = GetColNo(aCx);
  return NS_OK;
}

NS_IMETHODIMP JSStackFrame::GetSourceLine(nsACString& aSourceLine)
{
  aSourceLine.Truncate();
  return NS_OK;
}

NS_IMETHODIMP JSStackFrame::GetAsyncCause(JSContext* aCx,
                                          nsAString& aAsyncCause)
{
  if (!mStack) {
    aAsyncCause.Truncate();
    return NS_OK;
  }

  JS::Rooted<JSString*> asyncCause(aCx);
  bool canCache = false, useCachedValue = false;
  GetValueIfNotCached(aCx, mStack, JS::GetSavedFrameAsyncCause,
                      mAsyncCauseInitialized, &canCache, &useCachedValue,
                      &asyncCause);

  if (useCachedValue) {
    aAsyncCause = mAsyncCause;
    return NS_OK;
  }

  if (asyncCause) {
    nsAutoJSString str;
    if (!str.init(aCx, asyncCause)) {
      JS_ClearPendingException(aCx);
      aAsyncCause.Truncate();
      return NS_OK;
    }
    aAsyncCause = str;
  } else {
    aAsyncCause.SetIsVoid(true);
  }

  if (canCache) {
    mAsyncCause = aAsyncCause;
    mAsyncCauseInitialized = true;
  }

  return NS_OK;
}

NS_IMETHODIMP JSStackFrame::GetAsyncCaller(JSContext* aCx,
                                           nsIStackFrame** aAsyncCaller)
{
  if (!mStack) {
    *aAsyncCaller = nullptr;
    return NS_OK;
  }

  JS::Rooted<JSObject*> asyncCallerObj(aCx);
  bool canCache = false, useCachedValue = false;
  GetValueIfNotCached(aCx, mStack, JS::GetSavedFrameAsyncParent,
                      mAsyncCallerInitialized, &canCache, &useCachedValue,
                      &asyncCallerObj);

  if (useCachedValue) {
    NS_IF_ADDREF(*aAsyncCaller = mAsyncCaller);
    return NS_OK;
  }

  nsCOMPtr<nsIStackFrame> asyncCaller =
    asyncCallerObj ? new JSStackFrame(asyncCallerObj) : nullptr;
  asyncCaller.forget(aAsyncCaller);

  if (canCache) {
    mAsyncCaller = *aAsyncCaller;
    mAsyncCallerInitialized = true;
  }

  return NS_OK;
}

NS_IMETHODIMP JSStackFrame::GetCaller(JSContext* aCx, nsIStackFrame** aCaller)
{
  if (!mStack) {
    *aCaller = nullptr;
    return NS_OK;
  }

  JS::Rooted<JSObject*> callerObj(aCx);
  bool canCache = false, useCachedValue = false;
  GetValueIfNotCached(aCx, mStack, JS::GetSavedFrameParent, mCallerInitialized,
                      &canCache, &useCachedValue, &callerObj);

  if (useCachedValue) {
    NS_IF_ADDREF(*aCaller = mCaller);
    return NS_OK;
  }

  nsCOMPtr<nsIStackFrame> caller =
    callerObj ? new JSStackFrame(callerObj) : nullptr;
  caller.forget(aCaller);

  if (canCache) {
    mCaller = *aCaller;
    mCallerInitialized = true;
  }

  return NS_OK;
}

NS_IMETHODIMP JSStackFrame::GetFormattedStack(JSContext* aCx, nsAString& aStack)
{
  if (!mStack) {
    aStack.Truncate();
    return NS_OK;
  }

  // Sadly we can't use GetValueIfNotCached here, because our getter
  // returns bool, not JS::SavedFrameResult.  Maybe it's possible to
  // make the templates more complicated to deal, but in the meantime
  // let's just inline GetValueIfNotCached here.

  // Allow caching if aCx and stack are same-compartment.  Otherwise take the
  // slow path.
  bool canCache =
    js::GetContextCompartment(aCx) == js::GetObjectCompartment(mStack);
  if (canCache && mFormattedStackInitialized) {
    aStack = mFormattedStack;
    return NS_OK;
  }

  JS::ExposeObjectToActiveJS(mStack);
  JS::Rooted<JSObject*> stack(aCx, mStack);

  JS::Rooted<JSString*> formattedStack(aCx);
  if (!JS::BuildStackString(aCx, stack, &formattedStack)) {
    JS_ClearPendingException(aCx);
    aStack.Truncate();
    return NS_OK;
  }

  nsAutoJSString str;
  if (!str.init(aCx, formattedStack)) {
    JS_ClearPendingException(aCx);
    aStack.Truncate();
    return NS_OK;
  }

  aStack = str;

  if (canCache) {
    mFormattedStack = str;
    mFormattedStackInitialized = true;
  }

  return NS_OK;
}

NS_IMETHODIMP JSStackFrame::GetNativeSavedFrame(JS::MutableHandle<JS::Value> aSavedFrame)
{
  aSavedFrame.setObjectOrNull(mStack);
  return NS_OK;
}

NS_IMETHODIMP JSStackFrame::ToString(JSContext* aCx, nsACString& _retval)
{
  _retval.Truncate();

  nsString filename;
  nsresult rv = GetFilename(aCx, filename);
  NS_ENSURE_SUCCESS(rv, rv);

  if (filename.IsEmpty()) {
    filename.AssignLiteral("<unknown filename>");
  }

  nsString funname;
  rv = GetName(aCx, funname);
  NS_ENSURE_SUCCESS(rv, rv);

  if (funname.IsEmpty()) {
    funname.AssignLiteral("<TOP_LEVEL>");
  }

  int32_t lineno = GetLineno(aCx);

  static const char format[] = "JS frame :: %s :: %s :: line %d";
  _retval.AppendPrintf(format,
                       NS_ConvertUTF16toUTF8(filename).get(),
                       NS_ConvertUTF16toUTF8(funname).get(),
                       lineno);
  return NS_OK;
}

already_AddRefed<nsIStackFrame>
CreateStack(JSContext* aCx, int32_t aMaxDepth)
{
  static const unsigned MAX_FRAMES = 100;
  if (aMaxDepth < 0) {
    aMaxDepth = MAX_FRAMES;
  }

  JS::Rooted<JSObject*> stack(aCx);
  if (!JS::CaptureCurrentStack(aCx, &stack, aMaxDepth)) {
    return nullptr;
  }

  if (!stack) {
    return nullptr;
  }

  nsCOMPtr<nsIStackFrame> frame = new JSStackFrame(stack);
  return frame.forget();
}

} // namespace exceptions
} // namespace dom
} // namespace mozilla

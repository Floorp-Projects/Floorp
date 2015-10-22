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
  if (existingException) {
    nsresult nr;
    if (NS_SUCCEEDED(existingException->GetResult(&nr)) &&
        aRv == nr) {
      // Reuse the existing exception.

      // Clear pending exception
      runtime->SetPendingException(nullptr);

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
ThrowAndReport(nsPIDOMWindow* aWindow, nsresult aRv)
{
  MOZ_ASSERT(aRv != NS_ERROR_UNCATCHABLE_EXCEPTION,
             "Doesn't make sense to report uncatchable exceptions!");
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.InitWithLegacyErrorReporting(aWindow))) {
    return;
  }
  jsapi.TakeOwnershipOfErrorReporting();

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
GetCurrentJSStack()
{
  // is there a current context available?
  JSContext* cx = nullptr;

  if (NS_IsMainThread()) {
    MOZ_ASSERT(nsContentUtils::XPConnect());
    cx = nsContentUtils::GetCurrentJSContext();
  } else {
    cx = workers::GetCurrentThreadJSContext();
  }

  if (!cx || !js::GetContextCompartment(cx)) {
    return nullptr;
  }

  nsCOMPtr<nsIStackFrame> stack = exceptions::CreateStack(cx);
  if (!stack) {
    return nullptr;
  }

  // Note that CreateStack only returns JS frames, so we're done here.
  return stack.forget();
}

AutoForceSetExceptionOnContext::AutoForceSetExceptionOnContext(JSContext* aCx)
  : mCx(aCx)
{
  mOldValue = JS::ContextOptionsRef(mCx).autoJSAPIOwnsErrorReporting();
  JS::ContextOptionsRef(mCx).setAutoJSAPIOwnsErrorReporting(true);
}

AutoForceSetExceptionOnContext::~AutoForceSetExceptionOnContext()
{
  JS::ContextOptionsRef(mCx).setAutoJSAPIOwnsErrorReporting(mOldValue);
}

namespace exceptions {

class StackFrame : public nsIStackFrame
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(StackFrame)
  NS_DECL_NSISTACKFRAME

  StackFrame()
    : mLineno(0)
    , mColNo(0)
    , mLanguage(nsIProgrammingLanguage::UNKNOWN)
  {
  }

protected:
  virtual ~StackFrame();

  virtual bool IsJSFrame() const
  {
    return false;
  }

  virtual int32_t GetLineno()
  {
    return mLineno;
  }

  virtual int32_t GetColNo()
  {
    return mColNo;
  }

  nsCOMPtr<nsIStackFrame> mCaller;
  nsCOMPtr<nsIStackFrame> mAsyncCaller;
  nsString mFilename;
  nsString mFunname;
  nsString mAsyncCause;
  int32_t mLineno;
  int32_t mColNo;
  uint32_t mLanguage;
};

StackFrame::~StackFrame()
{
}

NS_IMPL_CYCLE_COLLECTION(StackFrame, mCaller, mAsyncCaller)
NS_IMPL_CYCLE_COLLECTING_ADDREF(StackFrame)
NS_IMPL_CYCLE_COLLECTING_RELEASE(StackFrame)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(StackFrame)
  NS_INTERFACE_MAP_ENTRY(nsIStackFrame)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

class JSStackFrame : public StackFrame
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(JSStackFrame,
                                                         StackFrame)

  // aStack must not be null.
  explicit JSStackFrame(JS::Handle<JSObject*> aStack);

  static already_AddRefed<nsIStackFrame>
  CreateStack(JSContext* aCx, int32_t aMaxDepth = -1);

  NS_IMETHOD GetLanguageName(nsACString& aLanguageName) override;
  NS_IMETHOD GetFilename(nsAString& aFilename) override;
  NS_IMETHOD GetName(nsAString& aFunction) override;
  NS_IMETHOD GetAsyncCause(nsAString& aAsyncCause) override;
  NS_IMETHOD GetAsyncCaller(nsIStackFrame** aAsyncCaller) override;
  NS_IMETHOD GetCaller(nsIStackFrame** aCaller) override;
  NS_IMETHOD GetFormattedStack(nsAString& aStack) override;
  NS_IMETHOD GetNativeSavedFrame(JS::MutableHandle<JS::Value> aSavedFrame) override;

protected:
  virtual bool IsJSFrame() const override {
    return true;
  }

  virtual int32_t GetLineno() override;
  virtual int32_t GetColNo() override;

private:
  virtual ~JSStackFrame();

  JS::Heap<JSObject*> mStack;
  nsString mFormattedStack;

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
  mLineno = 0;
  mLanguage = nsIProgrammingLanguage::JAVASCRIPT;
}

JSStackFrame::~JSStackFrame()
{
  mozilla::DropJSObjects(this);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(JSStackFrame)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(JSStackFrame, StackFrame)
  tmp->mStack = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(JSStackFrame, StackFrame)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(JSStackFrame, StackFrame)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mStack)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_ADDREF_INHERITED(JSStackFrame, StackFrame)
NS_IMPL_RELEASE_INHERITED(JSStackFrame, StackFrame)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(JSStackFrame)
NS_INTERFACE_MAP_END_INHERITING(StackFrame)

NS_IMETHODIMP StackFrame::GetLanguage(uint32_t* aLanguage)
{
  *aLanguage = mLanguage;
  return NS_OK;
}

NS_IMETHODIMP StackFrame::GetLanguageName(nsACString& aLanguageName)
{
  aLanguageName.AssignLiteral("C++");
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

NS_IMETHODIMP JSStackFrame::GetFilename(nsAString& aFilename)
{
  if (!mStack) {
    aFilename.Truncate();
    return NS_OK;
  }

  ThreadsafeAutoJSContext cx;
  JS::Rooted<JSString*> filename(cx);
  bool canCache = false, useCachedValue = false;
  GetValueIfNotCached(cx, mStack, JS::GetSavedFrameSource, mFilenameInitialized,
                      &canCache, &useCachedValue, &filename);
  if (useCachedValue) {
    return StackFrame::GetFilename(aFilename);
  }

  nsAutoJSString str;
  if (!str.init(cx, filename)) {
    JS_ClearPendingException(cx);
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

NS_IMETHODIMP StackFrame::GetFilename(nsAString& aFilename)
{
  // The filename must be set to null if empty.
  if (mFilename.IsEmpty()) {
    aFilename.SetIsVoid(true);
  } else {
    aFilename.Assign(mFilename);
  }

  return NS_OK;
}

NS_IMETHODIMP JSStackFrame::GetName(nsAString& aFunction)
{
  if (!mStack) {
    aFunction.Truncate();
    return NS_OK;
  }

  ThreadsafeAutoJSContext cx;
  JS::Rooted<JSString*> name(cx);
  bool canCache = false, useCachedValue = false;
  GetValueIfNotCached(cx, mStack, JS::GetSavedFrameFunctionDisplayName,
                      mFunnameInitialized, &canCache, &useCachedValue,
                      &name);

  if (useCachedValue) {
    return StackFrame::GetName(aFunction);
  }

  if (name) {
    nsAutoJSString str;
    if (!str.init(cx, name)) {
      JS_ClearPendingException(cx);
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

NS_IMETHODIMP StackFrame::GetName(nsAString& aFunction)
{
  // The function name must be set to null if empty.
  if (mFunname.IsEmpty()) {
    aFunction.SetIsVoid(true);
  } else {
    aFunction.Assign(mFunname);
  }

  return NS_OK;
}

// virtual
int32_t
JSStackFrame::GetLineno()
{
  if (!mStack) {
    return 0;
  }

  ThreadsafeAutoJSContext cx;
  uint32_t line;
  bool canCache = false, useCachedValue = false;
  GetValueIfNotCached(cx, mStack, JS::GetSavedFrameLine, mLinenoInitialized,
                      &canCache, &useCachedValue, &line);

  if (useCachedValue) {
    return StackFrame::GetLineno();
  }

  if (canCache) {
    mLineno = line;
    mLinenoInitialized = true;
  }

  return line;
}

NS_IMETHODIMP StackFrame::GetLineNumber(int32_t* aLineNumber)
{
  *aLineNumber = GetLineno();
  return NS_OK;
}

// virtual
int32_t
JSStackFrame::GetColNo()
{
  if (!mStack) {
    return 0;
  }

  ThreadsafeAutoJSContext cx;
  uint32_t col;
  bool canCache = false, useCachedValue = false;
  GetValueIfNotCached(cx, mStack, JS::GetSavedFrameColumn, mColNoInitialized,
                      &canCache, &useCachedValue, &col);

  if (useCachedValue) {
    return StackFrame::GetColNo();
  }

  if (canCache) {
    mColNo = col;
    mColNoInitialized = true;
  }

  return col;
}

NS_IMETHODIMP StackFrame::GetColumnNumber(int32_t* aColumnNumber)
{
  *aColumnNumber = GetColNo();
  return NS_OK;
}

NS_IMETHODIMP StackFrame::GetSourceLine(nsACString& aSourceLine)
{
  aSourceLine.Truncate();
  return NS_OK;
}

NS_IMETHODIMP JSStackFrame::GetAsyncCause(nsAString& aAsyncCause)
{
  if (!mStack) {
    aAsyncCause.Truncate();
    return NS_OK;
  }

  ThreadsafeAutoJSContext cx;
  JS::Rooted<JSString*> asyncCause(cx);
  bool canCache = false, useCachedValue = false;
  GetValueIfNotCached(cx, mStack, JS::GetSavedFrameAsyncCause,
                      mAsyncCauseInitialized, &canCache, &useCachedValue,
                      &asyncCause);

  if (useCachedValue) {
    return StackFrame::GetAsyncCause(aAsyncCause);
  }

  if (asyncCause) {
    nsAutoJSString str;
    if (!str.init(cx, asyncCause)) {
      JS_ClearPendingException(cx);
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

NS_IMETHODIMP StackFrame::GetAsyncCause(nsAString& aAsyncCause)
{
  // The async cause must be set to null if empty.
  if (mAsyncCause.IsEmpty()) {
    aAsyncCause.SetIsVoid(true);
  } else {
    aAsyncCause.Assign(mAsyncCause);
  }

  return NS_OK;
}

NS_IMETHODIMP JSStackFrame::GetAsyncCaller(nsIStackFrame** aAsyncCaller)
{
  if (!mStack) {
    *aAsyncCaller = nullptr;
    return NS_OK;
  }

  ThreadsafeAutoJSContext cx;
  JS::Rooted<JSObject*> asyncCallerObj(cx);
  bool canCache = false, useCachedValue = false;
  GetValueIfNotCached(cx, mStack, JS::GetSavedFrameAsyncParent,
                      mAsyncCallerInitialized, &canCache, &useCachedValue,
                      &asyncCallerObj);

  if (useCachedValue) {
    return StackFrame::GetAsyncCaller(aAsyncCaller);
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

NS_IMETHODIMP StackFrame::GetAsyncCaller(nsIStackFrame** aAsyncCaller)
{
  NS_IF_ADDREF(*aAsyncCaller = mAsyncCaller);
  return NS_OK;
}

NS_IMETHODIMP JSStackFrame::GetCaller(nsIStackFrame** aCaller)
{
  if (!mStack) {
    *aCaller = nullptr;
    return NS_OK;
  }

  ThreadsafeAutoJSContext cx;
  JS::Rooted<JSObject*> callerObj(cx);
  bool canCache = false, useCachedValue = false;
  GetValueIfNotCached(cx, mStack, JS::GetSavedFrameParent, mCallerInitialized,
                      &canCache, &useCachedValue, &callerObj);

  if (useCachedValue) {
    return StackFrame::GetCaller(aCaller);
  }

  nsCOMPtr<nsIStackFrame> caller;
  if (callerObj) {
      caller = new JSStackFrame(callerObj);
  } else {
    // Do we really need this dummy frame?  If so, we should document why... I
    // guess for symmetry with the "nothing on the stack" case, which returns
    // a single dummy frame?
    caller = new StackFrame();
  }
  caller.forget(aCaller);

  if (canCache) {
    mCaller = *aCaller;
    mCallerInitialized = true;
  }

  return NS_OK;
}

NS_IMETHODIMP StackFrame::GetCaller(nsIStackFrame** aCaller)
{
  NS_IF_ADDREF(*aCaller = mCaller);
  return NS_OK;
}

NS_IMETHODIMP JSStackFrame::GetFormattedStack(nsAString& aStack)
{
  if (!mStack) {
    aStack.Truncate();
    return NS_OK;
  }

  // Sadly we can't use GetValueIfNotCached here, because our getter
  // returns bool, not JS::SavedFrameResult.  Maybe it's possible to
  // make the templates more complicated to deal, but in the meantime
  // let's just inline GetValueIfNotCached here.
  ThreadsafeAutoJSContext cx;

  // Allow caching if cx and stack are same-compartment.  Otherwise take the
  // slow path.
  bool canCache =
    js::GetContextCompartment(cx) == js::GetObjectCompartment(mStack);
  if (canCache && mFormattedStackInitialized) {
    aStack = mFormattedStack;
    return NS_OK;
  }

  JS::ExposeObjectToActiveJS(mStack);
  JS::Rooted<JSObject*> stack(cx, mStack);

  JS::Rooted<JSString*> formattedStack(cx);
  if (!JS::BuildStackString(cx, stack, &formattedStack)) {
    JS_ClearPendingException(cx);
    aStack.Truncate();
    return NS_OK;
  }

  nsAutoJSString str;
  if (!str.init(cx, formattedStack)) {
    JS_ClearPendingException(cx);
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

NS_IMETHODIMP StackFrame::GetFormattedStack(nsAString& aStack)
{
  aStack.Truncate();
  return NS_OK;
}

NS_IMETHODIMP JSStackFrame::GetNativeSavedFrame(JS::MutableHandle<JS::Value> aSavedFrame)
{
  aSavedFrame.setObjectOrNull(mStack);
  return NS_OK;
}

NS_IMETHODIMP StackFrame::GetNativeSavedFrame(JS::MutableHandle<JS::Value> aSavedFrame)
{
  aSavedFrame.setNull();
  return NS_OK;
}

NS_IMETHODIMP StackFrame::ToString(nsACString& _retval)
{
  _retval.Truncate();

  const char* frametype = IsJSFrame() ? "JS" : "native";

  nsString filename;
  nsresult rv = GetFilename(filename);
  NS_ENSURE_SUCCESS(rv, rv);

  if (filename.IsEmpty()) {
    filename.AssignLiteral("<unknown filename>");
  }

  nsString funname;
  rv = GetName(funname);
  NS_ENSURE_SUCCESS(rv, rv);

  if (funname.IsEmpty()) {
    funname.AssignLiteral("<TOP_LEVEL>");
  }

  int32_t lineno = GetLineno();

  static const char format[] = "%s frame :: %s :: %s :: line %d";
  _retval.AppendPrintf(format, frametype,
                       NS_ConvertUTF16toUTF8(filename).get(),
                       NS_ConvertUTF16toUTF8(funname).get(),
                       lineno);
  return NS_OK;
}

/* static */ already_AddRefed<nsIStackFrame>
JSStackFrame::CreateStack(JSContext* aCx, int32_t aMaxDepth)
{
  static const unsigned MAX_FRAMES = 100;
  if (aMaxDepth < 0) {
    aMaxDepth = MAX_FRAMES;
  }

  JS::Rooted<JSObject*> stack(aCx);
  if (!JS::CaptureCurrentStack(aCx, &stack, aMaxDepth)) {
    return nullptr;
  }

  nsCOMPtr<nsIStackFrame> first;
  if (!stack) {
    first = new StackFrame();
  } else {
    first = new JSStackFrame(stack);
  }
  return first.forget();
}

already_AddRefed<nsIStackFrame>
CreateStack(JSContext* aCx, int32_t aMaxDepth)
{
  return JSStackFrame::CreateStack(aCx, aMaxDepth);
}

} // namespace exceptions
} // namespace dom
} // namespace mozilla

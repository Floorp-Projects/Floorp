/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Exceptions.h"

#include "js/GCAPI.h"
#include "js/OldDebugAPI.h"
#include "jsapi.h"
#include "jsprf.h"
#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/ScriptSettings.h"
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

  if (!WrapNewBindingObject(aCx, aException, &thrown)) {
    return false;
  }

  JS_SetPendingException(aCx, thrown);
  return true;
}

bool
Throw(JSContext* aCx, nsresult aRv, const char* aMessage)
{
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

  nsRefPtr<Exception> finalException = CreateException(aCx, aRv, aMessage);

  MOZ_ASSERT(finalException);
  if (!ThrowExceptionObject(aCx, finalException)) {
    // If we weren't able to throw an exception we're
    // most likely out of memory
    JS_ReportOutOfMemory(aCx);
  }

  return false;
}

void
ThrowAndReport(nsPIDOMWindow* aWindow, nsresult aRv, const char* aMessage)
{
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.InitWithLegacyErrorReporting(aWindow))) {
    return;
  }

  Throw(jsapi.cx(), aRv, aMessage);
  (void) JS_ReportPendingException(jsapi.cx());
}

already_AddRefed<Exception>
CreateException(JSContext* aCx, nsresult aRv, const char* aMessage)
{
  // Do we use DOM exceptions for this error code?
  switch (NS_ERROR_GET_MODULE(aRv)) {
  case NS_ERROR_MODULE_DOM:
  case NS_ERROR_MODULE_SVG:
  case NS_ERROR_MODULE_DOM_XPATH:
  case NS_ERROR_MODULE_DOM_INDEXEDDB:
  case NS_ERROR_MODULE_DOM_FILEHANDLE:
  case NS_ERROR_MODULE_DOM_BLUETOOTH:
    return DOMException::Create(aRv);
  default:
    break;
  }

  // If not, use the default.
  // aMessage can be null, so we can't use nsDependentCString on it.
  nsRefPtr<Exception> exception =
    new Exception(nsCString(aMessage), aRv,
                  EmptyCString(), nullptr, nullptr);
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

  if (!cx) {
    return nullptr;
  }

  nsCOMPtr<nsIStackFrame> stack = exceptions::CreateStack(cx);
  if (!stack) {
    return nullptr;
  }

  // Note that CreateStack only returns JS frames, so we're done here.
  return stack.forget();
}

namespace exceptions {

class StackFrame : public nsIStackFrame
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(StackFrame)
  NS_DECL_NSISTACKFRAME

  StackFrame(uint32_t aLanguage,
             const char* aFilename,
             const char* aFunctionName,
             int32_t aLineNumber,
             nsIStackFrame* aCaller);

  StackFrame()
    : mLineno(0)
    , mLanguage(nsIProgrammingLanguage::UNKNOWN)
  {
  }

  static already_AddRefed<nsIStackFrame>
  CreateStackFrameLocation(uint32_t aLanguage,
                           const char* aFilename,
                           const char* aFunctionName,
                           int32_t aLineNumber,
                           nsIStackFrame* aCaller);
protected:
  virtual ~StackFrame();

  virtual bool IsJSFrame() const
  {
    return false;
  }

  virtual nsresult GetLineno(int32_t* aLineNo)
  {
    *aLineNo = mLineno;
    return NS_OK;
  }

  nsCOMPtr<nsIStackFrame> mCaller;
  nsString mFilename;
  nsString mFunname;
  int32_t mLineno;
  uint32_t mLanguage;
};

StackFrame::StackFrame(uint32_t aLanguage,
                       const char* aFilename,
                       const char* aFunctionName,
                       int32_t aLineNumber,
                       nsIStackFrame* aCaller)
  : mCaller(aCaller)
  , mLineno(aLineNumber)
  , mLanguage(aLanguage)
{
  CopyUTF8toUTF16(aFilename, mFilename);
  CopyUTF8toUTF16(aFunctionName, mFunname);
}

StackFrame::~StackFrame()
{
}

NS_IMPL_CYCLE_COLLECTION(StackFrame, mCaller)
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
  JSStackFrame(JS::Handle<JSObject*> aStack);

  static already_AddRefed<nsIStackFrame>
  CreateStack(JSContext* aCx, int32_t aMaxDepth = -1);

  NS_IMETHOD GetLanguageName(nsACString& aLanguageName) MOZ_OVERRIDE;
  NS_IMETHOD GetFilename(nsAString& aFilename) MOZ_OVERRIDE;
  NS_IMETHOD GetName(nsAString& aFunction) MOZ_OVERRIDE;
  NS_IMETHOD GetCaller(nsIStackFrame** aCaller) MOZ_OVERRIDE;
  NS_IMETHOD GetFormattedStack(nsAString& aStack) MOZ_OVERRIDE;

protected:
  virtual bool IsJSFrame() const MOZ_OVERRIDE {
    return true;
  }

  virtual nsresult GetLineno(int32_t* aLineNo) MOZ_OVERRIDE;

private:
  virtual ~JSStackFrame();

  JS::Heap<JSObject*> mStack;
  nsString mFormattedStack;

  bool mFilenameInitialized;
  bool mFunnameInitialized;
  bool mLinenoInitialized;
  bool mCallerInitialized;
  bool mFormattedStackInitialized;
};

JSStackFrame::JSStackFrame(JS::Handle<JSObject*> aStack)
  : mStack(aStack)
  , mFilenameInitialized(false)
  , mFunnameInitialized(false)
  , mLinenoInitialized(false)
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

/* readonly attribute uint32_t language; */
NS_IMETHODIMP StackFrame::GetLanguage(uint32_t* aLanguage)
{
  *aLanguage = mLanguage;
  return NS_OK;
}

/* readonly attribute string languageName; */
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

/* readonly attribute AString filename; */
NS_IMETHODIMP JSStackFrame::GetFilename(nsAString& aFilename)
{
  // We can get called after unlink; in that case we can't do much
  // about producing a useful value.
  if (!mFilenameInitialized && mStack) {
    ThreadsafeAutoJSContext cx;
    JS::Rooted<JSObject*> stack(cx, mStack);
    JS::ExposeObjectToActiveJS(mStack);
    JSAutoCompartment ac(cx, stack);
    JS::Rooted<JS::Value> filenameVal(cx);
    if (!JS_GetProperty(cx, stack, "source", &filenameVal) ||
        !filenameVal.isString()) {
      return NS_ERROR_UNEXPECTED;
    }
    nsAutoJSString str;
    if (!str.init(cx, filenameVal.toString())) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    mFilename = str;
    mFilenameInitialized = true;
  }

  return StackFrame::GetFilename(aFilename);
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

/* readonly attribute AString name; */
NS_IMETHODIMP JSStackFrame::GetName(nsAString& aFunction)
{
  // We can get called after unlink; in that case we can't do much
  // about producing a useful value.
  if (!mFunnameInitialized && mStack) {
    ThreadsafeAutoJSContext cx;
    JS::Rooted<JSObject*> stack(cx, mStack);
    JS::ExposeObjectToActiveJS(mStack);
    JSAutoCompartment ac(cx, stack);
    JS::Rooted<JS::Value> nameVal(cx);
    // functionDisplayName can be null
    if (!JS_GetProperty(cx, stack, "functionDisplayName", &nameVal) ||
        (!nameVal.isString() && !nameVal.isNull())) {
      return NS_ERROR_UNEXPECTED;
    }
    if (nameVal.isString()) {
      nsAutoJSString str;
      if (!str.init(cx, nameVal.toString())) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      mFunname = str;
    }
    mFunnameInitialized = true;
  }

  return StackFrame::GetName(aFunction);
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
nsresult
JSStackFrame::GetLineno(int32_t* aLineNo)
{
  // We can get called after unlink; in that case we can't do much
  // about producing a useful value.
  if (!mLinenoInitialized && mStack) {
    ThreadsafeAutoJSContext cx;
    JS::Rooted<JSObject*> stack(cx, mStack);
    JS::ExposeObjectToActiveJS(mStack);
    JSAutoCompartment ac(cx, stack);
    JS::Rooted<JS::Value> lineVal(cx);
    if (!JS_GetProperty(cx, stack, "line", &lineVal) ||
        !lineVal.isNumber()) {
      return NS_ERROR_UNEXPECTED;
    }
    mLineno = lineVal.toNumber();
    mLinenoInitialized = true;
  }

  return StackFrame::GetLineno(aLineNo);
}

/* readonly attribute int32_t lineNumber; */
NS_IMETHODIMP StackFrame::GetLineNumber(int32_t* aLineNumber)
{
  return GetLineno(aLineNumber);
}

/* readonly attribute AUTF8String sourceLine; */
NS_IMETHODIMP StackFrame::GetSourceLine(nsACString& aSourceLine)
{
  aSourceLine.Truncate();
  return NS_OK;
}

/* readonly attribute nsIStackFrame caller; */
NS_IMETHODIMP JSStackFrame::GetCaller(nsIStackFrame** aCaller)
{
  // We can get called after unlink; in that case we can't do much
  // about producing a useful value.
  if (!mCallerInitialized && mStack) {
    ThreadsafeAutoJSContext cx;
    JS::Rooted<JSObject*> stack(cx, mStack);
    JS::ExposeObjectToActiveJS(mStack);
    JSAutoCompartment ac(cx, stack);
    JS::Rooted<JS::Value> callerVal(cx);
    if (!JS_GetProperty(cx, stack, "parent", &callerVal) ||
        !callerVal.isObjectOrNull()) {
      return NS_ERROR_UNEXPECTED;
    }

    if (callerVal.isObject()) {
      JS::Rooted<JSObject*> caller(cx, &callerVal.toObject());
      mCaller = new JSStackFrame(caller);
    } else {
      // Do we really need this dummy frame?  If so, we should document why... I
      // guess for symmetry with the "nothing on the stack" case, which returns
      // a single dummy frame?
      mCaller = new StackFrame();
    }
    mCallerInitialized = true;
  }
  return StackFrame::GetCaller(aCaller);
}

NS_IMETHODIMP StackFrame::GetCaller(nsIStackFrame** aCaller)
{
  NS_IF_ADDREF(*aCaller = mCaller);
  return NS_OK;
}

NS_IMETHODIMP JSStackFrame::GetFormattedStack(nsAString& aStack)
{
  // We can get called after unlink; in that case we can't do much
  // about producing a useful value.
  if (!mFormattedStackInitialized && mStack) {
    ThreadsafeAutoJSContext cx;
    JS::Rooted<JS::Value> stack(cx, JS::ObjectValue(*mStack));
    JS::ExposeObjectToActiveJS(mStack);
    JSAutoCompartment ac(cx, mStack);
    JS::Rooted<JSString*> formattedStack(cx, JS::ToString(cx, stack));
    if (!formattedStack) {
      return NS_ERROR_UNEXPECTED;
    }
    nsAutoJSString str;
    if (!str.init(cx, formattedStack)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    mFormattedStack = str;
    mFormattedStackInitialized = true;
  }

  aStack = mFormattedStack;
  return NS_OK;
}

NS_IMETHODIMP StackFrame::GetFormattedStack(nsAString& aStack)
{
  aStack.Truncate();
  return NS_OK;
}

/* AUTF8String toString (); */
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

  int32_t lineno;
  rv = GetLineno(&lineno);
  NS_ENSURE_SUCCESS(rv, rv);

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

/* static */ already_AddRefed<nsIStackFrame>
StackFrame::CreateStackFrameLocation(uint32_t aLanguage,
                                     const char* aFilename,
                                     const char* aFunctionName,
                                     int32_t aLineNumber,
                                     nsIStackFrame* aCaller)
{
  nsRefPtr<StackFrame> self =
    new StackFrame(aLanguage, aFilename, aFunctionName, aLineNumber, aCaller);
  return self.forget();
}

already_AddRefed<nsIStackFrame>
CreateStack(JSContext* aCx, int32_t aMaxDepth)
{
  return JSStackFrame::CreateStack(aCx, aMaxDepth);
}

already_AddRefed<nsIStackFrame>
CreateStackFrameLocation(uint32_t aLanguage,
                         const char* aFilename,
                         const char* aFunctionName,
                         int32_t aLineNumber,
                         nsIStackFrame* aCaller)
{
  return StackFrame::CreateStackFrameLocation(aLanguage, aFilename,
                                              aFunctionName, aLineNumber,
                                              aCaller);
}

} // namespace exceptions
} // namespace dom
} // namespace mozilla

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
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "XPCWrapper.h"
#include "WorkerPrivate.h"
#include "nsContentUtils.h"

namespace {

// We can't use nsContentUtils::IsCallerChrome because it might not exist in
// xpcshell.
bool
IsCallerChrome()
{
  nsCOMPtr<nsIScriptSecurityManager> secMan;
  secMan = XPCWrapper::GetSecurityManager();

  if (!secMan) {
    return false;
  }

  bool isChrome;
  return NS_SUCCEEDED(secMan->SubjectPrincipalIsSystem(&isChrome)) && isChrome;
}

} // anonymous namespace

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
    return false;
  }

  JS::Rooted<JS::Value> val(aCx);
  if (!WrapObject(aCx, glob, aException, &NS_GET_IID(nsIException), &val)) {
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
  if (NS_IsMainThread() && !IsCallerChrome() &&
      aException->StealJSVal(thrown.address())) {
    if (!JS_WrapValue(aCx, &thrown)) {
      return false;
    }
    JS_SetPendingException(aCx, thrown);
    return true;
  }

  JS::Rooted<JSObject*> glob(aCx, JS::CurrentGlobalOrNull(aCx));
  if (!glob) {
    return false;
  }

  if (!WrapNewBindingObject(aCx, glob, aException, &thrown)) {
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

  nsRefPtr<Exception> finalException;

  // Do we use DOM exceptions for this error code?
  switch (NS_ERROR_GET_MODULE(aRv)) {
  case NS_ERROR_MODULE_DOM:
  case NS_ERROR_MODULE_SVG:
  case NS_ERROR_MODULE_DOM_XPATH:
  case NS_ERROR_MODULE_DOM_INDEXEDDB:
  case NS_ERROR_MODULE_DOM_FILEHANDLE:
    finalException = DOMException::Create(aRv);
    break;

  default:
      break;
  }

  // If not, use the default.
  if (!finalException) {
    finalException = new Exception(aMessage, aRv, nullptr, nullptr, nullptr);
  }

  MOZ_ASSERT(finalException);
  if (!ThrowExceptionObject(aCx, finalException)) {
    // If we weren't able to throw an exception we're
    // most likely out of memory
    JS_ReportOutOfMemory(aCx);
  }

  return false;
}

already_AddRefed<nsIStackFrame>
GetCurrentJSStack()
{
  // is there a current context available?
  JSContext* cx = nullptr;

  if (NS_IsMainThread()) {
    // Note, in xpcshell nsContentUtils is never initialized, but we still need
    // to report exceptions.
    if (nsContentUtils::XPConnect()) {
      cx = nsContentUtils::XPConnect()->GetCurrentJSContext();
    } else {
      nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID());
      cx = xpc->GetCurrentJSContext();
    }
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

  // peel off native frames...
  uint32_t language;
  nsCOMPtr<nsIStackFrame> caller;
  while (stack &&
         NS_SUCCEEDED(stack->GetLanguage(&language)) &&
         language != nsIProgrammingLanguage::JAVASCRIPT &&
         NS_SUCCEEDED(stack->GetCaller(getter_AddRefs(caller))) &&
         caller) {
    stack = caller;
  }
  return stack.forget();
}

namespace exceptions {

class StackDescriptionOwner {
public:
  StackDescriptionOwner(JS::StackDescription* aDescription)
    : mDescription(aDescription)
  {
    mozilla::HoldJSObjects(this);
  }

  ~StackDescriptionOwner()
  {
    // Make sure to set mDescription to null before calling DropJSObjects, since
    // in debug builds DropJSObjects try to trace us and we don't want to trace
    // a dead StackDescription.
    if (mDescription) {
      JS::FreeStackDescription(nullptr, mDescription);
      mDescription = nullptr;
    }
    mozilla::DropJSObjects(this);
  }

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(StackDescriptionOwner)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(StackDescriptionOwner)

  JS::FrameDescription& FrameAt(size_t aIndex)
  {
    MOZ_ASSERT(aIndex < mDescription->nframes);
    return mDescription->frames[aIndex];
  }

  unsigned NumFrames()
  {
    return mDescription->nframes;
  }

private:
  JS::StackDescription* mDescription;
};

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(StackDescriptionOwner, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(StackDescriptionOwner, Release)

NS_IMPL_CYCLE_COLLECTION_CLASS(StackDescriptionOwner)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(StackDescriptionOwner)
  if (tmp->mDescription) {
    JS::FreeStackDescription(nullptr, tmp->mDescription);
    tmp->mDescription = nullptr;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(StackDescriptionOwner)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(StackDescriptionOwner)
  JS::StackDescription* desc = tmp->mDescription;
  if (tmp->mDescription) {
    for (size_t i = 0; i < desc->nframes; ++i) {
      NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mDescription->frames[i].script());
      NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mDescription->frames[i].fun());
    }
  }
NS_IMPL_CYCLE_COLLECTION_TRACE_END

class JSStackFrame : public nsIStackFrame
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(JSStackFrame)
  NS_DECL_NSISTACKFRAME

  // A null aStackDescription or an aIndex that's out of range for the
  // number of frames aStackDescription has will mean that the
  // JSStackFrame will never look at the stack description.  Instead,
  // it is expected to be initialized by the caller as needed.
  JSStackFrame(StackDescriptionOwner* aStackDescription, size_t aIndex);
  virtual ~JSStackFrame();

  static already_AddRefed<nsIStackFrame>
  CreateStack(JSContext* cx);
  static already_AddRefed<nsIStackFrame>
  CreateStackFrameLocation(uint32_t aLanguage,
                           const char* aFilename,
                           const char* aFunctionName,
                           int32_t aLineNumber,
                           nsIStackFrame* aCaller);

private:
  bool IsJSFrame() const {
    return mLanguage == nsIProgrammingLanguage::JAVASCRIPT;
  }

  const char* GetFilename();
  const char* GetFunname();
  int32_t GetLineno();

  nsRefPtr<StackDescriptionOwner> mStackDescription;
  nsCOMPtr<nsIStackFrame> mCaller;

  // Cached values
  char* mFilename;
  char* mFunname;
  int32_t mLineno;
  uint32_t mLanguage;

  size_t mIndex;

  bool mFilenameInitialized;
  bool mFunnameInitialized;
  bool mLinenoInitialized;
};

JSStackFrame::JSStackFrame(StackDescriptionOwner* aStackDescription,
                           size_t aIndex)
  : mFilename(nullptr),
    mFunname(nullptr),
    mLineno(0)
{
  if (aStackDescription && aIndex < aStackDescription->NumFrames()) {
    mStackDescription = aStackDescription;
    mIndex = aIndex;
    mFilenameInitialized = false;
    mFunnameInitialized = false;
    mLinenoInitialized = false;
    mLanguage = nsIProgrammingLanguage::JAVASCRIPT;
  } else {
    MOZ_ASSERT(!mStackDescription);
    mIndex = 0;
    mFilenameInitialized = true;
    mFunnameInitialized = true;
    mLinenoInitialized = true;
    mLanguage = nsIProgrammingLanguage::UNKNOWN;
  }
}

JSStackFrame::~JSStackFrame()
{
  if (mFilename) {
    nsMemory::Free(mFilename);
  }
  if (mFunname) {
    nsMemory::Free(mFunname);
  }
}

NS_IMPL_CYCLE_COLLECTION_2(JSStackFrame, mStackDescription, mCaller)

NS_IMPL_CYCLE_COLLECTING_ADDREF(JSStackFrame)
NS_IMPL_CYCLE_COLLECTING_RELEASE(JSStackFrame)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(JSStackFrame)
  NS_INTERFACE_MAP_ENTRY(nsIStackFrame)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/* readonly attribute uint32_t language; */
NS_IMETHODIMP JSStackFrame::GetLanguage(uint32_t* aLanguage)
{
  *aLanguage = mLanguage;
  return NS_OK;
}

/* readonly attribute string languageName; */
NS_IMETHODIMP JSStackFrame::GetLanguageName(char** aLanguageName)
{
  static const char js[] = "JavaScript";
  static const char cpp[] = "C++";

  if (IsJSFrame()) {
    *aLanguageName = (char*) nsMemory::Clone(js, sizeof(js));
  } else {
    *aLanguageName = (char*) nsMemory::Clone(cpp, sizeof(cpp));
  }

  return NS_OK;
}

const char*
JSStackFrame::GetFilename()
{
  if (!mFilenameInitialized) {
    JS::FrameDescription& desc = mStackDescription->FrameAt(mIndex);
    if (desc.script()) {
      AutoJSContext cx;
      JSAutoCompartment ac(cx, desc.script());
      const char* filename = JS_GetScriptFilename(cx, desc.script());
      if (filename) {
        mFilename =
          (char*)nsMemory::Clone(filename, sizeof(char)*(strlen(filename)+1));
      }
    }
    mFilenameInitialized = true;
  }

  return mFilename;
}

/* readonly attribute string filename; */
NS_IMETHODIMP JSStackFrame::GetFilename(char** aFilename)
{
  NS_ENSURE_ARG_POINTER(aFilename);

  const char* filename = GetFilename();
  if (filename) {
    *aFilename = (char*) nsMemory::Clone(filename,
                                         sizeof(char)*(strlen(filename)+1));
  } else {
    *aFilename = nullptr;
  }

  return NS_OK;
}

const char*
JSStackFrame::GetFunname()
{
  if (!mFunnameInitialized) {
    JS::FrameDescription& desc = mStackDescription->FrameAt(mIndex);
    if (desc.fun() && desc.script()) {
      AutoJSContext cx;
      JSAutoCompartment ac(cx, desc.script());
      JS::Rooted<JSFunction*> fun(cx, desc.fun());
      JS::Rooted<JSString*> funid(cx, JS_GetFunctionDisplayId(fun));
      if (funid) {
        size_t length = JS_GetStringEncodingLength(cx, funid);
        if (length != size_t(-1)) {
          mFunname = static_cast<char *>(nsMemory::Alloc(length + 1));
          if (mFunname) {
            JS_EncodeStringToBuffer(cx, funid, mFunname, length);
            mFunname[length] = '\0';
          }
        }
      }
    }
    mFunnameInitialized = true;
  }

  return mFunname;
}

/* readonly attribute string name; */
NS_IMETHODIMP JSStackFrame::GetName(char** aFunction)
{
  NS_ENSURE_ARG_POINTER(aFunction);

  const char* funname = GetFunname();
  if (funname) {
    *aFunction = (char*) nsMemory::Clone(funname,
                                         sizeof(char)*(strlen(funname)+1));
  } else {
    *aFunction = nullptr;
  }

  return NS_OK;
}

int32_t
JSStackFrame::GetLineno()
{
  if (!mLinenoInitialized) {
    JS::FrameDescription& desc = mStackDescription->FrameAt(mIndex);
    mLineno = desc.lineno();
    mLinenoInitialized = true;
  }

  return mLineno;
}

/* readonly attribute int32_t lineNumber; */
NS_IMETHODIMP JSStackFrame::GetLineNumber(int32_t* aLineNumber)
{
  *aLineNumber = GetLineno();
  return NS_OK;
}

/* readonly attribute string sourceLine; */
NS_IMETHODIMP JSStackFrame::GetSourceLine(char** aSourceLine)
{
  *aSourceLine = nullptr;
  return NS_OK;
}

/* readonly attribute nsIStackFrame caller; */
NS_IMETHODIMP JSStackFrame::GetCaller(nsIStackFrame** aCaller)
{
  NS_IF_ADDREF(*aCaller = mCaller);
  return NS_OK;
}

/* string toString (); */
NS_IMETHODIMP JSStackFrame::ToString(char** _retval)
{
  const char* frametype = IsJSFrame() ? "JS" : "native";
  const char* filename = GetFilename();
  if (!filename) {
    filename = "<unknown filename>";
  }
  const char* funname = GetFunname();
  if (!funname) {
    funname = "<TOP_LEVEL>";
  }
  static const char format[] = "%s frame :: %s :: %s :: line %d";
  int len = sizeof(char)*
              (strlen(frametype) + strlen(filename) + strlen(funname)) +
            sizeof(format) + 3 * sizeof(mLineno);

  char* buf = (char*) nsMemory::Alloc(len);
  JS_snprintf(buf, len, format, frametype, filename, funname, GetLineno());
  *_retval = buf;
  return NS_OK;
}

/* static */ already_AddRefed<nsIStackFrame>
JSStackFrame::CreateStack(JSContext* cx)
{
  static const unsigned MAX_FRAMES = 100;

  JS::StackDescription* desc = JS::DescribeStack(cx, MAX_FRAMES);
  if (!desc) {
    return nullptr;
  }

  nsRefPtr<StackDescriptionOwner> descOwner = new StackDescriptionOwner(desc);

  nsRefPtr<JSStackFrame> first;
  nsRefPtr<JSStackFrame> last;

  // We create one dummy frame at the end (why?), so go up through
  // desc->nframes+1
  for (size_t i = 0; i < desc->nframes+1; i++) {
    nsRefPtr<JSStackFrame> frame = new JSStackFrame(descOwner, i);

    if (last) {
      last->mCaller = frame;
    } else {
      MOZ_ASSERT(!first, "How can we have a first but not a last?");
      first = frame;
    }
    last.swap(frame);
  }

  return first.forget();
}

/* static */ already_AddRefed<nsIStackFrame>
JSStackFrame::CreateStackFrameLocation(uint32_t aLanguage,
                                       const char* aFilename,
                                       const char* aFunctionName,
                                       int32_t aLineNumber,
                                       nsIStackFrame* aCaller)
{
  nsRefPtr<JSStackFrame> self = new JSStackFrame(nullptr, 0);

  self->mLanguage = aLanguage;
  self->mLineno = aLineNumber;

  if (aFilename) {
    self->mFilename =
      (char*)nsMemory::Clone(aFilename, sizeof(char)*(strlen(aFilename)+1));
  }

  if (aFunctionName) {
    self->mFunname = 
      (char*)nsMemory::Clone(aFunctionName,
                             sizeof(char)*(strlen(aFunctionName)+1));
  }

  self->mCaller = aCaller;

  return self.forget();
}

already_AddRefed<nsIStackFrame>
CreateStack(JSContext* cx)
{
  return JSStackFrame::CreateStack(cx);
}

already_AddRefed<nsIStackFrame>
CreateStackFrameLocation(uint32_t aLanguage,
                         const char* aFilename,
                         const char* aFunctionName,
                         int32_t aLineNumber,
                         nsIStackFrame* aCaller)
{
  return JSStackFrame::CreateStackFrameLocation(aLanguage, aFilename,
                                                aFunctionName, aLineNumber,
                                                aCaller);
}

} // namespace exceptions
} // namespace dom
} // namespace mozilla

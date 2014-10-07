/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DOMException.h"

#include "jsprf.h"
#include "js/OldDebugAPI.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/Exceptions.h"
#include "nsContentUtils.h"
#include "nsCOMPtr.h"
#include "nsIClassInfoImpl.h"
#include "nsIDocument.h"
#include "nsIDOMDOMException.h"
#include "nsIException.h"
#include "nsIProgrammingLanguage.h"
#include "nsMemory.h"
#include "prprf.h"
#include "xpcprivate.h"

#include "mozilla/dom/DOMExceptionBinding.h"
#include "mozilla/ErrorResult.h"

using namespace mozilla;

enum DOM4ErrorTypeCodeMap {
  /* DOM4 errors from http://dvcs.w3.org/hg/domcore/raw-file/tip/Overview.html#domexception */
  IndexSizeError             = nsIDOMDOMException::INDEX_SIZE_ERR,
  HierarchyRequestError      = nsIDOMDOMException::HIERARCHY_REQUEST_ERR,
  WrongDocumentError         = nsIDOMDOMException::WRONG_DOCUMENT_ERR,
  InvalidCharacterError      = nsIDOMDOMException::INVALID_CHARACTER_ERR,
  NoModificationAllowedError = nsIDOMDOMException::NO_MODIFICATION_ALLOWED_ERR,
  NotFoundError              = nsIDOMDOMException::NOT_FOUND_ERR,
  NotSupportedError          = nsIDOMDOMException::NOT_SUPPORTED_ERR,
  // Can't remove until setNamedItem is removed
  InUseAttributeError        = nsIDOMDOMException::INUSE_ATTRIBUTE_ERR,
  InvalidStateError          = nsIDOMDOMException::INVALID_STATE_ERR,
  SyntaxError                = nsIDOMDOMException::SYNTAX_ERR,
  InvalidModificationError   = nsIDOMDOMException::INVALID_MODIFICATION_ERR,
  NamespaceError             = nsIDOMDOMException::NAMESPACE_ERR,
  InvalidAccessError         = nsIDOMDOMException::INVALID_ACCESS_ERR,
  TypeMismatchError          = nsIDOMDOMException::TYPE_MISMATCH_ERR,
  SecurityError              = nsIDOMDOMException::SECURITY_ERR,
  NetworkError               = nsIDOMDOMException::NETWORK_ERR,
  AbortError                 = nsIDOMDOMException::ABORT_ERR,
  URLMismatchError           = nsIDOMDOMException::URL_MISMATCH_ERR,
  QuotaExceededError         = nsIDOMDOMException::QUOTA_EXCEEDED_ERR,
  TimeoutError               = nsIDOMDOMException::TIMEOUT_ERR,
  InvalidNodeTypeError       = nsIDOMDOMException::INVALID_NODE_TYPE_ERR,
  DataCloneError             = nsIDOMDOMException::DATA_CLONE_ERR,
  InvalidPointerId           = nsIDOMDOMException::INVALID_POINTER_ERR,
  EncodingError              = 0,

  /* XXX Should be JavaScript native errors */
  TypeError                  = 0,
  RangeError                 = 0,

  /* IndexedDB errors http://dvcs.w3.org/hg/IndexedDB/raw-file/tip/Overview.html#exceptions */
  UnknownError             = 0,
  ConstraintError          = 0,
  DataError                = 0,
  TransactionInactiveError = 0,
  ReadOnlyError            = 0,
  VersionError             = 0,

  /* File API errors http://dev.w3.org/2006/webapi/FileAPI/#ErrorAndException */
  NotReadableError         = 0,

  /* FileHandle API errors */
  FileHandleInactiveError = 0,

  /* WebCrypto errors https://dvcs.w3.org/hg/webcrypto-api/raw-file/tip/spec/Overview.html#dfn-DataError */
  OperationError           = 0,

  /* Bluetooth API errors */
  BtFailError              = 0,
  BtNotReadyError          = 0,
  BtNoMemError             = 0,
  BtBusyError              = 0,
  BtDoneError              = 0,
  BtUnsupportedError       = 0,
  BtParmInvalidError       = 0,
  BtUnhandledError         = 0,
  BtAuthFailureError       = 0,
  BtRmtDevDownError        = 0,
  BtAuthRejectedError      = 0,
};

#define DOM4_MSG_DEF(name, message, nsresult) {(nsresult), name, #name, message},
#define DOM_MSG_DEF(val, message) {(val), NS_ERROR_GET_CODE(val), #val, message},

static const struct ResultStruct
{
  nsresult mNSResult;
  uint16_t mCode;
  const char* mName;
  const char* mMessage;
} sDOMErrorMsgMap[] = {
#include "domerr.msg"
};

#undef DOM4_MSG_DEF
#undef DOM_MSG_DEF

static void
NSResultToNameAndMessage(nsresult aNSResult,
                         nsCString& aName,
                         nsCString& aMessage,
                         uint16_t* aCode)
{
  aName.Truncate();
  aMessage.Truncate();
  *aCode = 0;
  for (uint32_t idx = 0; idx < ArrayLength(sDOMErrorMsgMap); idx++) {
    if (aNSResult == sDOMErrorMsgMap[idx].mNSResult) {
      aName.Rebind(sDOMErrorMsgMap[idx].mName,
                   strlen(sDOMErrorMsgMap[idx].mName));
      aMessage.Rebind(sDOMErrorMsgMap[idx].mMessage,
                   strlen(sDOMErrorMsgMap[idx].mMessage));
      *aCode = sDOMErrorMsgMap[idx].mCode;
      return;
    }
  }

  NS_WARNING("Huh, someone is throwing non-DOM errors using the DOM module!");

  return;
}

nsresult
NS_GetNameAndMessageForDOMNSResult(nsresult aNSResult, nsACString& aName,
                                   nsACString& aMessage, uint16_t* aCode)
{
  nsCString name;
  nsCString message;
  uint16_t code = 0;
  NSResultToNameAndMessage(aNSResult, name, message, &code);

  if (!name.IsEmpty() && !message.IsEmpty()) {
    aName = name;
    aMessage = message;
    if (aCode) {
      *aCode = code;
    }
    return NS_OK;
  }

  return NS_ERROR_NOT_AVAILABLE;
}

namespace mozilla {
namespace dom {

bool Exception::sEverMadeOneFromFactory = false;

NS_IMPL_CLASSINFO(Exception, nullptr, nsIClassInfo::DOM_OBJECT,
                  NS_XPCEXCEPTION_CID)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Exception)
  NS_INTERFACE_MAP_ENTRY(nsIException)
  NS_INTERFACE_MAP_ENTRY(nsIXPCException)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIException)
  NS_IMPL_QUERY_CLASSINFO(Exception)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Exception)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Exception)

NS_IMPL_CYCLE_COLLECTION_CLASS(Exception)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Exception)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLocation)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mInner)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(Exception)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mThrownJSVal);
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Exception)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLocation)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mInner)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->mThrownJSVal.setNull();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CI_INTERFACE_GETTER(Exception, nsIXPCException)

Exception::Exception(const nsACString& aMessage,
                     nsresult aResult,
                     const nsACString& aName,
                     nsIStackFrame *aLocation,
                     nsISupports *aData)
: mResult(NS_OK),
  mLineNumber(0),
  mInitialized(false),
  mHoldingJSVal(false)
{
  // A little hack... The nsIGenericModule nsIClassInfo scheme relies on there
  // having been at least one instance made via the factory. Otherwise, the
  // shared factory/classinsance object never gets created and our QI getter
  // for our instance's pointer to our nsIClassInfo will always return null.
  // This is bad because it means that wrapped exceptions will never have a
  // shared prototype. So... We force one to be created via the factory
  // *once* and then go about our business.
  if (!sEverMadeOneFromFactory) {
    nsCOMPtr<nsIXPCException> e =
        do_CreateInstance(XPC_EXCEPTION_CONTRACTID);
    sEverMadeOneFromFactory = true;
  }

  nsCOMPtr<nsIStackFrame> location;
  if (aLocation) {
    location = aLocation;
  } else {
    location = GetCurrentJSStack();
    // it is legal for there to be no active JS stack, if C++ code
    // is operating on a JS-implemented interface pointer without
    // having been called in turn by JS.  This happens in the JS
    // component loader, and will become more common as additional
    // components are implemented in JS.
  }
  // We want to trim off any leading native 'dataless' frames
  if (location) {
    while (1) {
      uint32_t language;
      int32_t lineNumber;
      if (NS_FAILED(location->GetLanguage(&language)) ||
          language == nsIProgrammingLanguage::JAVASCRIPT ||
          NS_FAILED(location->GetLineNumber(&lineNumber)) ||
          lineNumber) {
        break;
      }
      nsCOMPtr<nsIStackFrame> caller;
      if (NS_FAILED(location->GetCaller(getter_AddRefs(caller))) || !caller) {
        break;
      }
      location = caller;
    }
  }

  Initialize(aMessage, aResult, aName, location, aData, nullptr);
}

Exception::Exception()
  : mResult(NS_OK),
    mLineNumber(-1),
    mInitialized(false),
    mHoldingJSVal(false)
{
}

Exception::~Exception()
{
  if (mHoldingJSVal) {
    MOZ_ASSERT(NS_IsMainThread());

    mozilla::DropJSObjects(this);
  }
}

bool
Exception::StealJSVal(JS::Value* aVp)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mHoldingJSVal) {
    *aVp = mThrownJSVal;
    mThrownJSVal.setNull();

    mozilla::DropJSObjects(this);
    mHoldingJSVal = false;
    return true;
  }

  return false;
}

void
Exception::StowJSVal(JS::Value& aVp)
{
  MOZ_ASSERT(NS_IsMainThread());

  mThrownJSVal = aVp;
  if (!mHoldingJSVal) {
    mozilla::HoldJSObjects(this);
    mHoldingJSVal = true;
  }
}

/* readonly attribute AUTF8String message; */
NS_IMETHODIMP
Exception::GetMessageMoz(nsACString& aMessage)
{
  NS_ENSURE_TRUE(mInitialized, NS_ERROR_NOT_INITIALIZED);

  aMessage.Assign(mMessage);
  return NS_OK;
}

/* readonly attribute nsresult result; */
NS_IMETHODIMP
Exception::GetResult(nsresult* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_TRUE(mInitialized, NS_ERROR_NOT_INITIALIZED);

  *aResult = mResult;
  return NS_OK;
}

/* readonly attribute AUTF8String name; */
NS_IMETHODIMP
Exception::GetName(nsACString& aName)
{
  NS_ENSURE_TRUE(mInitialized, NS_ERROR_NOT_INITIALIZED);

  if (!mName.IsEmpty()) {
    aName.Assign(mName);
  } else {
    aName.Truncate();

    const char* name = nullptr;
    nsXPCException::NameAndFormatForNSResult(mResult, &name, nullptr);

    if (name) {
      aName.Assign(name);
    }
  }

  return NS_OK;
}

/* readonly attribute AString filename; */
NS_IMETHODIMP
Exception::GetFilename(nsAString& aFilename)
{
  NS_ENSURE_TRUE(mInitialized, NS_ERROR_NOT_INITIALIZED);

  if (mLocation) {
    return mLocation->GetFilename(aFilename);
  }

  aFilename.Assign(mFilename);
  return NS_OK;
}

/* readonly attribute uint32_t lineNumber; */
NS_IMETHODIMP
Exception::GetLineNumber(uint32_t *aLineNumber)
{
  NS_ENSURE_ARG_POINTER(aLineNumber);
  NS_ENSURE_TRUE(mInitialized, NS_ERROR_NOT_INITIALIZED);

  if (mLocation) {
    int32_t lineno;
    nsresult rv = mLocation->GetLineNumber(&lineno);
    *aLineNumber = lineno;
    return rv;
  }

  *aLineNumber = mLineNumber;
  return NS_OK;
}

/* readonly attribute uint32_t columnNumber; */
NS_IMETHODIMP
Exception::GetColumnNumber(uint32_t* aColumnNumber)
{
  NS_ENSURE_ARG_POINTER(aColumnNumber);
  NS_ENSURE_TRUE(mInitialized, NS_ERROR_NOT_INITIALIZED);

  *aColumnNumber = 0;
  return NS_OK;
}

/* readonly attribute nsIStackFrame location; */
NS_IMETHODIMP
Exception::GetLocation(nsIStackFrame** aLocation)
{
  NS_ENSURE_ARG_POINTER(aLocation);
  NS_ENSURE_TRUE(mInitialized, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIStackFrame> location = mLocation;
  location.forget(aLocation);
  return NS_OK;
}

/* readonly attribute nsISupports data; */
NS_IMETHODIMP
Exception::GetData(nsISupports** aData)
{
  NS_ENSURE_ARG_POINTER(aData);
  NS_ENSURE_TRUE(mInitialized, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsISupports> data = mData;
  data.forget(aData);
  return NS_OK;
}

/* readonly attribute nsIException inner; */
NS_IMETHODIMP
Exception::GetInner(nsIException** aException)
{
  NS_ENSURE_ARG_POINTER(aException);
  NS_ENSURE_TRUE(mInitialized, NS_ERROR_NOT_INITIALIZED);

  nsCOMPtr<nsIException> inner = mInner;
  inner.forget(aException);
  return NS_OK;
}

/* AUTF8String toString (); */
NS_IMETHODIMP
Exception::ToString(nsACString& _retval)
{
  NS_ENSURE_TRUE(mInitialized, NS_ERROR_NOT_INITIALIZED);

  static const char defaultMsg[] = "<no message>";
  static const char defaultLocation[] = "<unknown>";
  static const char format[] =
"[Exception... \"%s\"  nsresult: \"0x%x (%s)\"  location: \"%s\"  data: %s]";

  nsCString location;

  if (mLocation) {
    // we need to free this if it does not fail
    nsresult rv = mLocation->ToString(location);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (location.IsEmpty()) {
    location.Assign(defaultLocation);
  }

  const char* msg = mMessage.IsEmpty() ? nullptr : mMessage.get();

  const char* resultName = mName.IsEmpty() ? nullptr: mName.get();
  if (!resultName &&
      !nsXPCException::NameAndFormatForNSResult(mResult, &resultName,
                                                (!msg) ? &msg : nullptr)) {
    if (!msg) {
      msg = defaultMsg;
    }
    resultName = "<unknown>";
  }
  const char* data = mData ? "yes" : "no";

  _retval.Truncate();
  _retval.AppendPrintf(format, msg, mResult, resultName,
                       location.get(), data);
  return NS_OK;
}

/* void initialize (in AUTF8String aMessage, in nsresult aResult,
 *                  in AUTF8String aName, in nsIStackFrame aLocation,
 *                  in nsISupports aData, in nsIException aInner); */
NS_IMETHODIMP
Exception::Initialize(const nsACString& aMessage, nsresult aResult,
                      const nsACString& aName, nsIStackFrame *aLocation,
                      nsISupports *aData, nsIException *aInner)
{
  NS_ENSURE_FALSE(mInitialized, NS_ERROR_ALREADY_INITIALIZED);

  mMessage = aMessage;
  mName = aName;
  mResult = aResult;

  if (aLocation) {
    mLocation = aLocation;
  } else {
    nsresult rv;
    nsXPConnect* xpc = nsXPConnect::XPConnect();
    rv = xpc->GetCurrentJSStack(getter_AddRefs(mLocation));
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  mData = aData;
  mInner = aInner;

  mInitialized = true;
  return NS_OK;
}

JSObject*
Exception::WrapObject(JSContext* cx)
{
  return ExceptionBinding::Wrap(cx, this);
}

void
Exception::GetMessageMoz(nsString& retval)
{
  nsCString str;
#ifdef DEBUG
  DebugOnly<nsresult> rv =
#endif
  GetMessageMoz(str);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  CopyUTF8toUTF16(str, retval);
}

uint32_t
Exception::Result() const
{
  return (uint32_t)mResult;
}

void
Exception::GetName(nsString& retval)
{
  nsCString str;
#ifdef DEBUG
  DebugOnly<nsresult> rv =
#endif
  GetName(str);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  CopyUTF8toUTF16(str, retval);
}

uint32_t
Exception::LineNumber() const
{
  if (mLocation) {
    int32_t lineno;
    if (NS_SUCCEEDED(mLocation->GetLineNumber(&lineno))) {
      return lineno;
    }
    return 0;
  }

  return mLineNumber;
}

uint32_t
Exception::ColumnNumber() const
{
  return 0;
}

already_AddRefed<nsIStackFrame>
Exception::GetLocation() const
{
  nsCOMPtr<nsIStackFrame> location = mLocation;
  return location.forget();
}

already_AddRefed<nsISupports>
Exception::GetInner() const
{
  nsCOMPtr<nsIException> inner = mInner;
  return inner.forget();
}

already_AddRefed<nsISupports>
Exception::GetData() const
{
  nsCOMPtr<nsISupports> data = mData;
  return data.forget();
}

void
Exception::GetStack(nsAString& aStack, ErrorResult& aRv) const
{
  if (mLocation) {
    aRv = mLocation->GetFormattedStack(aStack);
  }
}

void
Exception::Stringify(nsString& retval)
{
  nsCString str;
#ifdef DEBUG
  DebugOnly<nsresult> rv =
#endif
  ToString(str);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  CopyUTF8toUTF16(str, retval);
}

NS_IMPL_ADDREF_INHERITED(DOMException, Exception)
NS_IMPL_RELEASE_INHERITED(DOMException, Exception)
NS_INTERFACE_MAP_BEGIN(DOMException)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDOMException)
NS_INTERFACE_MAP_END_INHERITING(Exception)

DOMException::DOMException(nsresult aRv, const nsACString& aMessage,
                           const nsACString& aName, uint16_t aCode)
  : Exception(EmptyCString(), aRv, EmptyCString(), nullptr, nullptr),
    mName(aName),
    mMessage(aMessage),
    mCode(aCode)
{
}

NS_IMETHODIMP
DOMException::GetCode(uint16_t* aCode)
{
  NS_ENSURE_ARG_POINTER(aCode);
  *aCode = mCode;

  // Warn only when the code was changed (other than DOM Core)
  // or the code is useless (zero)
  if (NS_ERROR_GET_MODULE(mResult) != NS_ERROR_MODULE_DOM || !mCode) {
    nsCOMPtr<nsIDocument> doc = nsContentUtils::GetDocumentFromCaller();
    if (doc) {
      doc->WarnOnceAbout(nsIDocument::eDOMExceptionCode);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
DOMException::ToString(nsACString& aReturn)
{
  aReturn.Truncate();

  static const char defaultMsg[] = "<no message>";
  static const char defaultLocation[] = "<unknown>";
  static const char defaultName[] = "<unknown>";
  static const char format[] =
    "[Exception... \"%s\"  code: \"%d\" nsresult: \"0x%x (%s)\"  location: \"%s\"]";

  nsAutoCString location;

  if (mInner) {
    nsString filename;
    mInner->GetFilename(filename);

    if (!filename.IsEmpty()) {
      uint32_t line_nr = 0;

      mInner->GetLineNumber(&line_nr);

      char *temp = PR_smprintf("%s Line: %d",
                               NS_ConvertUTF16toUTF8(filename).get(),
                               line_nr);
      if (temp) {
        location.Assign(temp);
        PR_smprintf_free(temp);
      }
    }
  }

  if (location.IsEmpty()) {
    location = defaultLocation;
  }

  const char* msg = !mMessage.IsEmpty() ? mMessage.get() : defaultMsg;
  const char* resultName = !mName.IsEmpty() ? mName.get() : defaultName;

  aReturn.AppendPrintf(format, msg, mCode, mResult, resultName,
                       location.get());

  return NS_OK;
}

void
DOMException::GetName(nsString& retval)
{
  CopyUTF8toUTF16(mName, retval);
}

void
DOMException::GetMessageMoz(nsString& retval)
{
  CopyUTF8toUTF16(mMessage, retval);
}

JSObject*
DOMException::WrapObject(JSContext* aCx)
{
  return DOMExceptionBinding::Wrap(aCx, this);
}

/* static */already_AddRefed<DOMException>
DOMException::Create(nsresult aRv)
{
  nsCString name;
  nsCString message;
  uint16_t code;
  NSResultToNameAndMessage(aRv, name, message, &code);
  nsRefPtr<DOMException> inst =
    new DOMException(aRv, message, name, code);
  return inst.forget();
}

} // namespace dom
} // namespace mozilla

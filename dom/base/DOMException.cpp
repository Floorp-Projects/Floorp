/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DOMException.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/Exceptions.h"
#include "nsContentUtils.h"
#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIException.h"
#include "nsMemory.h"
#include "xpcprivate.h"

#include "mozilla/dom/DOMExceptionBinding.h"
#include "mozilla/ErrorResult.h"

using namespace mozilla;
using namespace mozilla::dom;

enum DOM4ErrorTypeCodeMap {
  /* DOM4 errors from http://dvcs.w3.org/hg/domcore/raw-file/tip/Overview.html#domexception */
  IndexSizeError             = DOMException_Binding::INDEX_SIZE_ERR,
  HierarchyRequestError      = DOMException_Binding::HIERARCHY_REQUEST_ERR,
  WrongDocumentError         = DOMException_Binding::WRONG_DOCUMENT_ERR,
  InvalidCharacterError      = DOMException_Binding::INVALID_CHARACTER_ERR,
  NoModificationAllowedError = DOMException_Binding::NO_MODIFICATION_ALLOWED_ERR,
  NotFoundError              = DOMException_Binding::NOT_FOUND_ERR,
  NotSupportedError          = DOMException_Binding::NOT_SUPPORTED_ERR,
  // Can't remove until setNamedItem is removed
  InUseAttributeError        = DOMException_Binding::INUSE_ATTRIBUTE_ERR,
  InvalidStateError          = DOMException_Binding::INVALID_STATE_ERR,
  SyntaxError                = DOMException_Binding::SYNTAX_ERR,
  InvalidModificationError   = DOMException_Binding::INVALID_MODIFICATION_ERR,
  NamespaceError             = DOMException_Binding::NAMESPACE_ERR,
  InvalidAccessError         = DOMException_Binding::INVALID_ACCESS_ERR,
  TypeMismatchError          = DOMException_Binding::TYPE_MISMATCH_ERR,
  SecurityError              = DOMException_Binding::SECURITY_ERR,
  NetworkError               = DOMException_Binding::NETWORK_ERR,
  AbortError                 = DOMException_Binding::ABORT_ERR,
  URLMismatchError           = DOMException_Binding::URL_MISMATCH_ERR,
  QuotaExceededError         = DOMException_Binding::QUOTA_EXCEEDED_ERR,
  TimeoutError               = DOMException_Binding::TIMEOUT_ERR,
  InvalidNodeTypeError       = DOMException_Binding::INVALID_NODE_TYPE_ERR,
  DataCloneError             = DOMException_Binding::DATA_CLONE_ERR,
  InvalidPointerId           = 0,
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

  /* Push API errors */
  NotAllowedError          = 0,
};

#define DOM4_MSG_DEF(name, message, nsresult) {(nsresult), name, #name, message},
#define DOM_MSG_DEF(val, message) {(val), NS_ERROR_GET_CODE(val), #val, message},

static constexpr struct ResultStruct
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

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Exception)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(Exception)
  NS_INTERFACE_MAP_ENTRY(nsIException)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Exception)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Exception)

NS_IMPL_CYCLE_COLLECTION_CLASS(Exception)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Exception)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mLocation)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mData)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(Exception)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mThrownJSVal)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Exception)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mLocation)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mData)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->mThrownJSVal.setNull();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

Exception::Exception(const nsACString& aMessage,
                     nsresult aResult,
                     const nsACString& aName,
                     nsIStackFrame *aLocation,
                     nsISupports *aData)
  : mMessage(aMessage)
  , mResult(aResult)
  , mName(aName)
  , mData(aData)
  , mHoldingJSVal(false)
{
  if (aLocation) {
    mLocation = aLocation;
  } else {
    mLocation = GetCurrentJSStack();
    // it is legal for there to be no active JS stack, if C++ code
    // is operating on a JS-implemented interface pointer without
    // having been called in turn by JS.  This happens in the JS
    // component loader.
  }
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

void
Exception::GetName(nsAString& aName)
{
  if (!mName.IsEmpty()) {
    CopyUTF8toUTF16(mName, aName);
  } else {
    aName.Truncate();

    const char* name = nullptr;
    nsXPCException::NameAndFormatForNSResult(mResult, &name, nullptr);

    if (name) {
      CopyUTF8toUTF16(name, aName);
    }
  }
}

void
Exception::GetFilename(JSContext* aCx, nsAString& aFilename)
{
  if (mLocation) {
    mLocation->GetFilename(aCx, aFilename);
    return;
  }

  aFilename.Truncate();
}

void
Exception::ToString(JSContext* aCx, nsACString& _retval)
{
  static const char defaultMsg[] = "<no message>";
  static const char defaultLocation[] = "<unknown>";
  static const char format[] =
"[Exception... \"%s\"  nsresult: \"0x%" PRIx32 " (%s)\"  location: \"%s\"  data: %s]";

  nsCString location;

  if (mLocation) {
    // we need to free this if it does not fail
    mLocation->ToString(aCx, location);
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
  _retval.AppendPrintf(format, msg, static_cast<uint32_t>(mResult), resultName,
                       location.get(), data);
}

JSObject*
Exception::WrapObject(JSContext* cx, JS::Handle<JSObject*> aGivenProto)
{
  return Exception_Binding::Wrap(cx, this, aGivenProto);
}

void
Exception::GetMessageMoz(nsString& retval)
{
  CopyUTF8toUTF16(mMessage, retval);
}

uint32_t
Exception::Result() const
{
  return (uint32_t)mResult;
}

uint32_t
Exception::LineNumber(JSContext* aCx) const
{
  if (mLocation) {
    return mLocation->GetLineNumber(aCx);
  }

  return 0;
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

nsISupports*
Exception::GetData() const
{
  return mData;
}

void
Exception::GetStack(JSContext* aCx, nsAString& aStack) const
{
  if (mLocation) {
    mLocation->GetFormattedStack(aCx, aStack);
  }
}

void
Exception::Stringify(JSContext* aCx, nsString& retval)
{
  nsCString str;
  ToString(aCx, str);
  CopyUTF8toUTF16(str, retval);
}

DOMException::DOMException(nsresult aRv, const nsACString& aMessage,
                           const nsACString& aName, uint16_t aCode)
  : Exception(aMessage, aRv, aName, nullptr, nullptr),
    mCode(aCode)
{
}

void
DOMException::ToString(JSContext* aCx, nsACString& aReturn)
{
  aReturn.Truncate();

  static const char defaultMsg[] = "<no message>";
  static const char defaultLocation[] = "<unknown>";
  static const char defaultName[] = "<unknown>";
  static const char format[] =
    "[Exception... \"%s\"  code: \"%d\" nsresult: \"0x%" PRIx32 " (%s)\"  location: \"%s\"]";

  nsAutoCString location;

  if (location.IsEmpty()) {
    location = defaultLocation;
  }

  const char* msg = !mMessage.IsEmpty() ? mMessage.get() : defaultMsg;
  const char* resultName = !mName.IsEmpty() ? mName.get() : defaultName;

  aReturn.AppendPrintf(format, msg, mCode, static_cast<uint32_t>(mResult), resultName,
                       location.get());
}

void
DOMException::GetName(nsString& retval)
{
  CopyUTF8toUTF16(mName, retval);
}

already_AddRefed<DOMException>
DOMException::Constructor(GlobalObject& /* unused */,
                          const nsAString& aMessage,
                          const Optional<nsAString>& aName,
                          ErrorResult& aError)
{
  nsresult exceptionResult = NS_OK;
  uint16_t exceptionCode = 0;
  nsCString name(NS_LITERAL_CSTRING("Error"));

  if (aName.WasPassed()) {
    CopyUTF16toUTF8(aName.Value(), name);
    for (uint32_t idx = 0; idx < ArrayLength(sDOMErrorMsgMap); idx++) {
      if (name.EqualsASCII(sDOMErrorMsgMap[idx].mName)) {
        exceptionResult = sDOMErrorMsgMap[idx].mNSResult;
        exceptionCode = sDOMErrorMsgMap[idx].mCode;
        break;
      }
    }
  }

  RefPtr<DOMException> retval =
    new DOMException(exceptionResult,
                     NS_ConvertUTF16toUTF8(aMessage),
                     name,
                     exceptionCode);
  return retval.forget();
}

JSObject*
DOMException::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return DOMException_Binding::Wrap(aCx, this, aGivenProto);
}

/* static */already_AddRefed<DOMException>
DOMException::Create(nsresult aRv)
{
  nsCString name;
  nsCString message;
  uint16_t code;
  NSResultToNameAndMessage(aRv, name, message, &code);
  RefPtr<DOMException> inst =
    new DOMException(aRv, message, name, code);
  return inst.forget();
}

/* static */already_AddRefed<DOMException>
DOMException::Create(nsresult aRv, const nsACString& aMessage)
{
  nsCString name;
  nsCString message;
  uint16_t code;
  NSResultToNameAndMessage(aRv, name, message, &code);
  RefPtr<DOMException> inst =
    new DOMException(aRv, aMessage, name, code);
  return inst.forget();
}

} // namespace dom
} // namespace mozilla

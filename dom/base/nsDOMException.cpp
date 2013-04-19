/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsCRTGlue.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfoID.h"
#include "nsError.h"
#include "nsDOMException.h"
#include "nsIDOMDOMException.h"
#include "nsIDocument.h"
#include "nsString.h"
#include "prprf.h"

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
  LockedFileInactiveError = 0,
};

#define DOM4_MSG_DEF(name, message, nsresult) {(nsresult), name, #name, message},
#define DOM_MSG_DEF(val, message) {(val), NS_ERROR_GET_CODE(val), #val, message},

static struct ResultStruct
{
  nsresult mNSResult;
  uint16_t mCode;
  const char* mName;
  const char* mMessage;
} gDOMErrorMsgMap[] = {
#include "domerr.msg"
  {NS_OK, 0, nullptr, nullptr}   // sentinel to mark end of array
};

#undef DOM4_MSG_DEF
#undef DOM_MSG_DEF

static void
NSResultToNameAndMessage(nsresult aNSResult,
                         const char** aName,
                         const char** aMessage,
                         uint16_t* aCode)
{
  *aName = nullptr;
  *aMessage = nullptr;
  *aCode = 0;
  ResultStruct* result_struct = gDOMErrorMsgMap;

  while (result_struct->mName) {
    if (aNSResult == result_struct->mNSResult) {
      *aName = result_struct->mName;
      *aMessage = result_struct->mMessage;
      *aCode = result_struct->mCode;
      return;
    }

    ++result_struct;
  }

  NS_WARNING("Huh, someone is throwing non-DOM errors using the DOM module!");

  return;
}

nsresult
NS_GetNameAndMessageForDOMNSResult(nsresult aNSResult, const char** aName,
                                   const char** aMessage, uint16_t* aCode)
{
  const char* name = nullptr;
  const char* message = nullptr;
  uint16_t code = 0;
  NSResultToNameAndMessage(aNSResult, &name, &message, &code);

  if (name && message) {
    *aName = name;
    *aMessage = message;
    if (aCode) {
      *aCode = code;
    }
    return NS_OK;
  }

  return NS_ERROR_NOT_AVAILABLE;
}


class nsDOMException : public nsIException,
                       public nsIDOMDOMException
{
public:
  nsDOMException() {}
  virtual ~nsDOMException() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIEXCEPTION
  NS_IMETHOD Init(nsresult aNSResult, const char* aName,
                  const char* aMessage, uint16_t aCode,
                  nsIException* aDefaultException);
  NS_DECL_NSIDOMDOMEXCEPTION

protected:
  const char* mName;
  const char* mMessage;
  nsCOMPtr<nsIException> mInner;
  nsresult mResult;
  uint16_t mCode;
};

DOMCI_DATA(DOMException, nsDOMException) 

NS_IMPL_ADDREF(nsDOMException)
NS_IMPL_RELEASE(nsDOMException)
NS_INTERFACE_MAP_BEGIN(nsDOMException)
  NS_INTERFACE_MAP_ENTRY(nsIException)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDOMException)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIException)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DOMException)
NS_INTERFACE_MAP_END

nsresult
NS_NewDOMException(nsresult aNSResult, nsIException* aDefaultException,
                   nsIException** aException)
{
  const char* name;
  const char* message;
  uint16_t code;
  NSResultToNameAndMessage(aNSResult, &name, &message, &code);
  nsDOMException* inst = new nsDOMException();
  inst->Init(aNSResult, name, message, code, aDefaultException);
  *aException = inst;
  NS_ADDREF(*aException);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMException::GetCode(uint16_t* aCode)
{
  NS_ENSURE_ARG_POINTER(aCode);
  *aCode = mCode;

  // Warn only when the code was changed (other than DOM Core)
  // or the code is useless (zero)
  if (NS_ERROR_GET_MODULE(mResult) != NS_ERROR_MODULE_DOM || !mCode) {
    nsCOMPtr<nsIDocument> doc =
      do_QueryInterface(nsContentUtils::GetDocumentFromCaller());
    if (doc) {
      doc->WarnOnceAbout(nsIDocument::eDOMExceptionCode);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMException::GetMessageMoz(char **aMessage)
{
  if (mMessage) {
    *aMessage = NS_strdup(mMessage);
  } else {
    *aMessage = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMException::GetResult(nsresult* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = mResult;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMException::GetName(char **aName)
{
  NS_ENSURE_ARG_POINTER(aName);

  if (mName) {
    *aName = NS_strdup(mName);
  } else {
    *aName = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMException::GetFilename(char **aFilename)
{
  if (mInner) {
    return mInner->GetFilename(aFilename);
  }

  NS_ENSURE_ARG_POINTER(aFilename);

  *aFilename = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMException::GetLineNumber(uint32_t *aLineNumber)
{
  if (mInner) {
    return mInner->GetLineNumber(aLineNumber);
  }

  NS_ENSURE_ARG_POINTER(aLineNumber);

  *aLineNumber = 0;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMException::GetColumnNumber(uint32_t *aColumnNumber)
{
  if (mInner) {
    return mInner->GetColumnNumber(aColumnNumber);
  }

  NS_ENSURE_ARG_POINTER(aColumnNumber);

  *aColumnNumber = 0;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMException::GetLocation(nsIStackFrame **aLocation)
{
  if (mInner) {
    return mInner->GetLocation(aLocation);
  }

  NS_ENSURE_ARG_POINTER(aLocation);

  *aLocation = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMException::GetInner(nsIException **aInner)
{
  NS_ENSURE_ARG_POINTER(aInner);

  *aInner = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMException::GetData(nsISupports **aData)
{
  if (mInner) {
    return mInner->GetData(aData);
  }

  NS_ENSURE_ARG_POINTER(aData);

  *aData = nullptr;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMException::ToString(char **aReturn)
{
  *aReturn = nullptr;

  static const char defaultMsg[] = "<no message>";
  static const char defaultLocation[] = "<unknown>";
  static const char defaultName[] = "<unknown>";
  static const char format[] =
    "[Exception... \"%s\"  code: \"%d\" nsresult: \"0x%x (%s)\"  location: \"%s\"]";

  nsAutoCString location;

  if (mInner) {
    nsXPIDLCString filename;

    mInner->GetFilename(getter_Copies(filename));

    if (!filename.IsEmpty()) {
      uint32_t line_nr = 0;

      mInner->GetLineNumber(&line_nr);

      char *temp = PR_smprintf("%s Line: %d", filename.get(), line_nr);
      if (temp) {
        location.Assign(temp);
        PR_smprintf_free(temp);
      }
    }
  }

  if (location.IsEmpty()) {
    location = defaultLocation;
  }

  const char* msg = mMessage ? mMessage : defaultMsg;
  const char* resultName = mName ? mName : defaultName;

  *aReturn = PR_smprintf(format, msg, mCode, mResult, resultName,
                         location.get());

  return *aReturn ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsDOMException::Init(nsresult aNSResult, const char* aName,
                     const char* aMessage, uint16_t aCode,
                     nsIException* aDefaultException)
{
  mResult = aNSResult;
  mName = aName;
  mMessage = aMessage;
  mCode = aCode;
  mInner = aDefaultException;
  return NS_OK;
}

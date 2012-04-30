/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsCRTGlue.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfoID.h"
#include "nsDOMError.h"
#include "nsDOMException.h"
#include "nsIDOMDOMException.h"
#include "nsIDOMSVGException.h"
#include "nsIDOMXPathException.h"
#include "nsIDocument.h"
#include "nsString.h"
#include "prprf.h"

#define IMPL_INTERNAL_DOM_EXCEPTION_HEAD(domname)                            \
class ns##domname : public nsBaseDOMException,                               \
                    public nsIDOM##domname                                   \
{                                                                            \
public:                                                                      \
  ns##domname();                                                             \
  virtual ~ns##domname();                                                    \
                                                                             \
  NS_DECL_ISUPPORTS_INHERITED

#define IMPL_INTERNAL_DOM_EXCEPTION_TAIL(domname)                            \
};                                                                           \
                                                                             \
ns##domname::ns##domname() {}                                                \
ns##domname::~ns##domname() {}                                               \
                                                                             \
DOMCI_DATA(domname, ns##domname)                                             \
                                                                             \
NS_IMPL_ADDREF_INHERITED(ns##domname, nsBaseDOMException)                    \
NS_IMPL_RELEASE_INHERITED(ns##domname, nsBaseDOMException)                   \
NS_INTERFACE_MAP_BEGIN(ns##domname)                                          \
  NS_INTERFACE_MAP_ENTRY(nsIDOM##domname)                                    \
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(domname)                              \
NS_INTERFACE_MAP_END_INHERITING(nsBaseDOMException)                          \
                                                                             \
nsresult                                                                     \
NS_New##domname(nsresult aNSResult, nsIException* aDefaultException,         \
                  nsIException** aException)                                 \
{                                                                            \
  const char* name;                                                          \
  const char* message;                                                       \
  PRUint16 code;                                                             \
  NSResultToNameAndMessage(aNSResult, &name, &message, &code);               \
  ns##domname* inst = new ns##domname();                                     \
  NS_ENSURE_TRUE(inst, NS_ERROR_OUT_OF_MEMORY);                              \
  inst->Init(aNSResult, name, message, code, aDefaultException);             \
  *aException = inst;                                                        \
  NS_ADDREF(*aException);                                                    \
  return NS_OK;                                                              \
}

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

  /* XXX Should be JavaScript native TypeError */
  TypeError                  = 0,

  /* IndexedDB errors http://dvcs.w3.org/hg/IndexedDB/raw-file/tip/Overview.html#exceptions */
  UnknownError             = 0,
  ConstraintError          = 0,
  DataError                = 0,
  TransactionInactiveError = 0,
  ReadOnlyError            = 0,
  VersionError             = 0,

  /* File API errors http://dev.w3.org/2006/webapi/FileAPI/#ErrorAndException */
  NotReadableError         = 0,
};

#define DOM4_MSG_DEF(name, message, nsresult) {(nsresult), name, #name, message},
#define DOM_MSG_DEF(val, message) {(val), NS_ERROR_GET_CODE(val), #val, message},

static struct ResultStruct
{
  nsresult mNSResult;
  PRUint16 mCode;
  const char* mName;
  const char* mMessage;
} gDOMErrorMsgMap[] = {
#include "domerr.msg"
  {0, 0, nsnull, nsnull}   // sentinel to mark end of array
};

#undef DOM4_MSG_DEF
#undef DOM_MSG_DEF

static void
NSResultToNameAndMessage(nsresult aNSResult,
                         const char** aName,
                         const char** aMessage,
                         PRUint16* aCode)
{
  *aName = nsnull;
  *aMessage = nsnull;
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
                                   const char** aMessage, PRUint16* aCode)
{
  const char* name = nsnull;
  const char* message = nsnull;
  PRUint16 code = 0;
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

IMPL_INTERNAL_DOM_EXCEPTION_HEAD(DOMException)
  NS_DECL_NSIDOMDOMEXCEPTION
IMPL_INTERNAL_DOM_EXCEPTION_TAIL(DOMException)

NS_IMETHODIMP
nsDOMException::GetCode(PRUint16* aCode)
{
  NS_ENSURE_ARG_POINTER(aCode);
  *aCode = mCode;

  // Warn only when the code was changed (IndexedDB or File API)
  // or the code is useless (zero)
  if (NS_ERROR_GET_MODULE(mResult) == NS_ERROR_MODULE_DOM_INDEXEDDB ||
      NS_ERROR_GET_MODULE(mResult) == NS_ERROR_MODULE_DOM_FILE ||
      !mCode) {
    nsCOMPtr<nsIDocument> doc =
      do_QueryInterface(nsContentUtils::GetDocumentFromCaller());
    if (doc) {
      doc->WarnOnceAbout(nsIDocument::eDOMExceptionCode);
    }
  }

  return NS_OK;
}

IMPL_INTERNAL_DOM_EXCEPTION_HEAD(SVGException)
  NS_DECL_NSIDOMSVGEXCEPTION
IMPL_INTERNAL_DOM_EXCEPTION_TAIL(SVGException)

NS_IMETHODIMP
nsSVGException::GetCode(PRUint16* aCode)
{
  NS_ENSURE_ARG_POINTER(aCode);
  *aCode = mCode;

  return NS_OK;
}

IMPL_INTERNAL_DOM_EXCEPTION_HEAD(XPathException)
  NS_DECL_NSIDOMXPATHEXCEPTION
IMPL_INTERNAL_DOM_EXCEPTION_TAIL(XPathException)

NS_IMETHODIMP
nsXPathException::GetCode(PRUint16* aCode)
{
  NS_ENSURE_ARG_POINTER(aCode);
  *aCode = mCode;

  return NS_OK;
}

nsBaseDOMException::nsBaseDOMException()
{
}

nsBaseDOMException::~nsBaseDOMException()
{
}

NS_IMPL_ISUPPORTS2(nsBaseDOMException, nsIException, nsIBaseDOMException)

NS_IMETHODIMP
nsBaseDOMException::GetMessageMoz(char **aMessage)
{
  if (mMessage) {
    *aMessage = NS_strdup(mMessage);
  } else {
    *aMessage = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsBaseDOMException::GetResult(PRUint32* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = mResult;

  return NS_OK;
}

NS_IMETHODIMP
nsBaseDOMException::GetName(char **aName)
{
  NS_ENSURE_ARG_POINTER(aName);

  if (mName) {
    *aName = NS_strdup(mName);
  } else {
    *aName = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsBaseDOMException::GetFilename(char **aFilename)
{
  if (mInner) {
    return mInner->GetFilename(aFilename);
  }

  NS_ENSURE_ARG_POINTER(aFilename);

  *aFilename = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsBaseDOMException::GetLineNumber(PRUint32 *aLineNumber)
{
  if (mInner) {
    return mInner->GetLineNumber(aLineNumber);
  }

  NS_ENSURE_ARG_POINTER(aLineNumber);

  *aLineNumber = 0;

  return NS_OK;
}

NS_IMETHODIMP
nsBaseDOMException::GetColumnNumber(PRUint32 *aColumnNumber)
{
  if (mInner) {
    return mInner->GetColumnNumber(aColumnNumber);
  }

  NS_ENSURE_ARG_POINTER(aColumnNumber);

  *aColumnNumber = 0;

  return NS_OK;
}

NS_IMETHODIMP
nsBaseDOMException::GetLocation(nsIStackFrame **aLocation)
{
  if (mInner) {
    return mInner->GetLocation(aLocation);
  }

  NS_ENSURE_ARG_POINTER(aLocation);

  *aLocation = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsBaseDOMException::GetInner(nsIException **aInner)
{
  NS_ENSURE_ARG_POINTER(aInner);

  *aInner = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsBaseDOMException::GetData(nsISupports **aData)
{
  if (mInner) {
    return mInner->GetData(aData);
  }

  NS_ENSURE_ARG_POINTER(aData);

  *aData = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsBaseDOMException::ToString(char **aReturn)
{
  *aReturn = nsnull;

  static const char defaultMsg[] = "<no message>";
  static const char defaultLocation[] = "<unknown>";
  static const char defaultName[] = "<unknown>";
  static const char format[] =
    "[Exception... \"%s\"  code: \"%d\" nsresult: \"0x%x (%s)\"  location: \"%s\"]";

  nsCAutoString location;

  if (mInner) {
    nsXPIDLCString filename;

    mInner->GetFilename(getter_Copies(filename));

    if (!filename.IsEmpty()) {
      PRUint32 line_nr = 0;

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
nsBaseDOMException::Init(nsresult aNSResult, const char* aName,
                         const char* aMessage, PRUint16 aCode,
                         nsIException* aDefaultException)
{
  mResult = aNSResult;
  mName = aName;
  mMessage = aMessage;
  mCode = aCode;
  mInner = aDefaultException;
  return NS_OK;
}

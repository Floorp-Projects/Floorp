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
#include "nsDOMClassInfo.h"
#include "nsDOMError.h"
#include "nsDOMException.h"
#include "nsIDOMDOMException.h"
#include "nsIDOMRangeException.h"
#include "nsIDOMFileException.h"
#include "nsIDOMSVGException.h"
#include "nsIDOMXPathException.h"
#include "nsIIDBDatabaseException.h"
#include "nsString.h"
#include "prprf.h"
#include "nsIDOMEventException.h"

#define DOM_MSG_DEF(val, message) {(val), #val, message},

#define IMPL_INTERNAL_DOM_EXCEPTION_HEAD(classname, ifname)                  \
class classname : public nsBaseDOMException,                                 \
                  public ifname                                              \
{                                                                            \
public:                                                                      \
  classname();                                                               \
  virtual ~classname();                                                      \
                                                                             \
  NS_DECL_ISUPPORTS_INHERITED

#define IMPL_INTERNAL_DOM_EXCEPTION_TAIL(classname, ifname, domname, module, \
                                         mapping_function)                   \
};                                                                           \
                                                                             \
classname::classname() {}                                                    \
classname::~classname() {}                                                   \
                                                                             \
DOMCI_DATA(domname, classname)                                               \
                                                                             \
NS_IMPL_ADDREF_INHERITED(classname, nsBaseDOMException)                      \
NS_IMPL_RELEASE_INHERITED(classname, nsBaseDOMException)                     \
NS_INTERFACE_MAP_BEGIN(classname)                                            \
  NS_INTERFACE_MAP_ENTRY(ifname)                                             \
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(domname)                              \
NS_INTERFACE_MAP_END_INHERITING(nsBaseDOMException)                          \
                                                                             \
nsresult                                                                     \
NS_New##domname(nsresult aNSResult, nsIException* aDefaultException,         \
                nsIException** aException)                                   \
{                                                                            \
  if (!(NS_ERROR_GET_MODULE(aNSResult) == module)) {                         \
    NS_WARNING("Trying to create an exception for the wrong error module."); \
    return NS_ERROR_FAILURE;                                                 \
  }                                                                          \
  const char* name;                                                          \
  const char* message;                                                       \
  mapping_function(aNSResult, &name, &message);                              \
  classname* inst = new classname();                                         \
  NS_ENSURE_TRUE(inst, NS_ERROR_OUT_OF_MEMORY);                              \
  inst->Init(aNSResult, name, message, aDefaultException);                   \
  *aException = inst;                                                        \
  NS_ADDREF(*aException);                                                    \
  return NS_OK;                                                              \
}

static struct ResultStruct
{
  nsresult mNSResult;
  const char* mName;
  const char* mMessage;
} gDOMErrorMsgMap[] = {
#include "domerr.msg"
  {0, nsnull, nsnull}   // sentinel to mark end of array
};

#undef DOM_MSG_DEF

static void
NSResultToNameAndMessage(nsresult aNSResult,
                         const char** aName,
                         const char** aMessage)
{
  *aName = nsnull;
  *aMessage = nsnull;
  ResultStruct* result_struct = gDOMErrorMsgMap;

  while (result_struct->mName) {
    if (aNSResult == result_struct->mNSResult) {
      *aName = result_struct->mName;
      *aMessage = result_struct->mMessage;
      return;
    }

    ++result_struct;
  }

  NS_WARNING("Huh, someone is throwing non-DOM errors using the DOM module!");

  return;
}

nsresult
NS_GetNameAndMessageForDOMNSResult(nsresult aNSResult, const char** aName,
                                   const char** aMessage)
{
  const char* name = nsnull;
  const char* message = nsnull;
  NSResultToNameAndMessage(aNSResult, &name, &message);

  if (name && message) {
    *aName = name;
    *aMessage = message;
    return NS_OK;
  }

  return NS_ERROR_NOT_AVAILABLE;
}

IMPL_INTERNAL_DOM_EXCEPTION_HEAD(nsDOMException, nsIDOMDOMException)
  NS_DECL_NSIDOMDOMEXCEPTION
IMPL_INTERNAL_DOM_EXCEPTION_TAIL(nsDOMException, nsIDOMDOMException,
                                 DOMException, NS_ERROR_MODULE_DOM,
                                 NSResultToNameAndMessage)

NS_IMETHODIMP
nsDOMException::GetCode(PRUint32* aCode)
{
  NS_ENSURE_ARG_POINTER(aCode);
  nsresult result;
  GetResult(&result);
  *aCode = NS_ERROR_GET_CODE(result);

  return NS_OK;
}

IMPL_INTERNAL_DOM_EXCEPTION_HEAD(nsRangeException, nsIDOMRangeException)
  NS_DECL_NSIDOMRANGEEXCEPTION
IMPL_INTERNAL_DOM_EXCEPTION_TAIL(nsRangeException, nsIDOMRangeException,
                                 RangeException, NS_ERROR_MODULE_DOM_RANGE,
                                 NSResultToNameAndMessage)

NS_IMETHODIMP
nsRangeException::GetCode(PRUint16* aCode)
{
  NS_ENSURE_ARG_POINTER(aCode);
  nsresult result;
  GetResult(&result);
  *aCode = NS_ERROR_GET_CODE(result);

  return NS_OK;
}

IMPL_INTERNAL_DOM_EXCEPTION_HEAD(nsSVGException, nsIDOMSVGException)
  NS_DECL_NSIDOMSVGEXCEPTION
IMPL_INTERNAL_DOM_EXCEPTION_TAIL(nsSVGException, nsIDOMSVGException,
                                 SVGException, NS_ERROR_MODULE_SVG,
                                 NSResultToNameAndMessage)

NS_IMETHODIMP
nsSVGException::GetCode(PRUint16* aCode)
{
  NS_ENSURE_ARG_POINTER(aCode);
  nsresult result;
  GetResult(&result);
  *aCode = NS_ERROR_GET_CODE(result);

  return NS_OK;
}

IMPL_INTERNAL_DOM_EXCEPTION_HEAD(nsXPathException, nsIDOMXPathException)
  NS_DECL_NSIDOMXPATHEXCEPTION
IMPL_INTERNAL_DOM_EXCEPTION_TAIL(nsXPathException, nsIDOMXPathException,
                                 XPathException, NS_ERROR_MODULE_DOM_XPATH,
                                 NSResultToNameAndMessage)

NS_IMETHODIMP
nsXPathException::GetCode(PRUint16* aCode)
{
  NS_ENSURE_ARG_POINTER(aCode);
  nsresult result;
  GetResult(&result);
  *aCode = NS_ERROR_GET_CODE(result);

  return NS_OK;
}

IMPL_INTERNAL_DOM_EXCEPTION_HEAD(nsDOMFileException, nsIDOMFileException)
  NS_DECL_NSIDOMFILEEXCEPTION
IMPL_INTERNAL_DOM_EXCEPTION_TAIL(nsDOMFileException, nsIDOMFileException,
                                 FileException, NS_ERROR_MODULE_DOM_FILE,
                                 NSResultToNameAndMessage)

NS_IMETHODIMP
nsDOMFileException::GetCode(PRUint16* aCode)
{
  NS_ENSURE_ARG_POINTER(aCode);
  nsresult result;
  GetResult(&result);
  *aCode = NS_ERROR_GET_CODE(result);

  return NS_OK;
}

IMPL_INTERNAL_DOM_EXCEPTION_HEAD(nsDOMEventException, nsIDOMEventException)
  NS_DECL_NSIDOMEVENTEXCEPTION
IMPL_INTERNAL_DOM_EXCEPTION_TAIL(nsDOMEventException, nsIDOMEventException,
                                 EventException, NS_ERROR_MODULE_DOM_EVENTS,
                                 NSResultToNameAndMessage)

NS_IMETHODIMP
nsDOMEventException::GetCode(PRUint16* aCode)
{
  NS_ENSURE_ARG_POINTER(aCode);
  nsresult result;
  GetResult(&result);
  *aCode = NS_ERROR_GET_CODE(result);

  return NS_OK;
}

IMPL_INTERNAL_DOM_EXCEPTION_HEAD(nsIDBDatabaseException,
                                 nsIIDBDatabaseException)
  NS_DECL_NSIIDBDATABASEEXCEPTION
IMPL_INTERNAL_DOM_EXCEPTION_TAIL(nsIDBDatabaseException,
                                 nsIIDBDatabaseException,
                                 IDBDatabaseException,
                                 NS_ERROR_MODULE_DOM_INDEXEDDB,
                                 NSResultToNameAndMessage)

NS_IMETHODIMP
nsIDBDatabaseException::GetCode(PRUint16* aCode)
{
  NS_ASSERTION(aCode, "Null pointer!");
  nsresult result;
  GetResult(&result);
  *aCode = NS_ERROR_GET_CODE(result);
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
  PRUint32 code = NS_ERROR_GET_CODE(mResult);

  *aReturn = PR_smprintf(format, msg, code, mResult, resultName,
                         location.get());

  return *aReturn ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsBaseDOMException::Init(nsresult aNSResult, const char* aName,
                         const char* aMessage,
                         nsIException* aDefaultException)
{
  mResult = aNSResult;
  mName = aName;
  mMessage = aMessage;
  mInner = aDefaultException;
  return NS_OK;
}

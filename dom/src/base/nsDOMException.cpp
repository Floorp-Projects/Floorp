/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIDOMDOMException.h"
#include "nsDOMError.h"
#include "nsDOMClassInfo.h"
#include "nsCRT.h"
#include "prprf.h"


#define DOM_MSG_DEF(val, message) {(val), #val, message},

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


static const ResultStruct *
NSResultToResultStruct(nsresult aNSResult)
{
  ResultStruct* result_struct = gDOMErrorMsgMap;

  while (result_struct->mName) {
    if (aNSResult == result_struct->mNSResult) {
      return result_struct;
    }

    ++result_struct;
  }

  NS_WARNING("Huh, someone is throwing non-DOM errors using the DOM module!");

  return nsnull;
}


class nsDOMException : public nsIException,
                       public nsIDOMDOMException
{
public:
  nsDOMException(nsresult aNSResult, nsIException* aInner);
  virtual ~nsDOMException();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMDOMEXCEPTION

  // nsIException
  NS_DECL_NSIEXCEPTION

protected:
  nsresult mResult;
  nsCOMPtr<nsIException> mInner;
};

nsresult
NS_NewDOMException(nsresult aNSResult, nsIException* aDefaultException,
                   nsIException** aException)
{
  *aException = new nsDOMException(aNSResult, aDefaultException);
  NS_ENSURE_TRUE(*aException, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*aException);

  return NS_OK;
}


nsDOMException::nsDOMException(nsresult aNSResult, nsIException* aInner)
  : mResult(aNSResult), mInner(aInner)
{
  NS_INIT_ISUPPORTS();
}

nsDOMException::~nsDOMException()
{
}

// XPConnect interface list for nsDOMException
NS_CLASSINFO_MAP_BEGIN(DOMException)
  NS_CLASSINFO_MAP_ENTRY(nsIException)
  NS_CLASSINFO_MAP_ENTRY(nsIDOMDOMException)
NS_CLASSINFO_MAP_END


// QueryInterface implementation for nsDOMException
NS_INTERFACE_MAP_BEGIN(nsDOMException)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIException)
  NS_INTERFACE_MAP_ENTRY(nsIException)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDOMException)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(DOMException)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsDOMException)
NS_IMPL_RELEASE(nsDOMException)


NS_IMETHODIMP
nsDOMException::GetCode(PRUint32* aCode)
{
  if (NS_ERROR_GET_MODULE(mResult) == NS_ERROR_MODULE_DOM) {
    *aCode = NS_ERROR_GET_CODE(mResult);
  } else {
    NS_WARNING("Non DOM nsresult passed to a DOM exception!");

    *aCode = (PRUint32)mResult;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMException::GetMessage(char **aMessage)
{
  const ResultStruct *rs = NSResultToResultStruct(mResult);

  if (rs) {
    *aMessage = nsCRT::strdup(rs->mMessage);
  } else {
    *aMessage = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMException::GetResult(PRUint32* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = mResult;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMException::GetName(char **aName)
{
  NS_ENSURE_ARG_POINTER(aName);

  const ResultStruct *rs = NSResultToResultStruct(mResult);

  if (rs) {
    *aName = nsCRT::strdup(rs->mName);
  } else {
    *aName = nsnull;
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

  *aFilename = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMException::GetLineNumber(PRUint32 *aLineNumber)
{
  if (mInner) {
    return mInner->GetLineNumber(aLineNumber);
  }

  NS_ENSURE_ARG_POINTER(aLineNumber);

  *aLineNumber = 0;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMException::GetColumnNumber(PRUint32 *aColumnNumber)
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

  *aLocation = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMException::GetInner(nsIException **aInner)
{
  NS_ENSURE_ARG_POINTER(aInner);

  *aInner = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMException::GetData(nsISupports **aData)
{
  if (mInner) {
    return mInner->GetData(aData);
  }

  NS_ENSURE_ARG_POINTER(aData);

  *aData = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMException::ToString(char **aReturn)
{
  *aReturn = nsCRT::strdup("foo");

  const ResultStruct *rs = NSResultToResultStruct(mResult);

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

  const char* msg = rs ? rs->mMessage : defaultMsg;
  const char* resultName = rs ? rs->mName : defaultName;
  PRUint32 code;

  GetCode(&code);

  *aReturn = PR_smprintf(format, msg, code, mResult, resultName,
                         location.get());

  return *aReturn ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

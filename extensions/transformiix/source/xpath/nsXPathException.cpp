/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@netscape.com> (original author)
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

#include "nsXPathException.h"
#include "nsCRT.h"
#include "nsIDOMClassInfo.h"
#include "nsString.h"
#include "prprf.h"

NS_IMPL_ISUPPORTS1(nsXPathExceptionProvider, nsIExceptionProvider)

nsXPathExceptionProvider::nsXPathExceptionProvider()
{
    NS_INIT_ISUPPORTS();
}

nsXPathExceptionProvider::~nsXPathExceptionProvider()
{
}

NS_IMETHODIMP
nsXPathExceptionProvider::GetException(nsresult aNSResult,
                                       nsIException *aDefaultException,
                                       nsIException **aException)
{
    NS_ENSURE_ARG_POINTER(aException);

    *aException = new nsXPathException(aNSResult, aDefaultException);
    NS_ENSURE_TRUE(*aException, NS_ERROR_OUT_OF_MEMORY);

    NS_IF_ADDREF(*aException);

    return NS_OK;
}

NS_IMPL_ADDREF(nsXPathException)
NS_IMPL_RELEASE(nsXPathException)
NS_INTERFACE_MAP_BEGIN(nsXPathException)
  NS_INTERFACE_MAP_ENTRY(nsIException)
  NS_INTERFACE_MAP_ENTRY(nsIDOMXPathException)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIException)
  NS_INTERFACE_MAP_ENTRY_EXTERNAL_DOM_CLASSINFO(XPathException)
NS_INTERFACE_MAP_END

nsXPathException::nsXPathException(nsresult aNSResult, nsIException* aInner)
    : mResult(aNSResult),
      mInner(aInner)
{
    NS_INIT_ISUPPORTS();
}

nsXPathException::~nsXPathException()
{
}

NS_IMETHODIMP
nsXPathException::GetCode(PRUint16* aCode)
{
    NS_ENSURE_ARG_POINTER(aCode);

    if (NS_ERROR_GET_MODULE(mResult) == NS_ERROR_MODULE_DOM_XPATH) {
        *aCode = NS_ERROR_GET_CODE(mResult);
    } else {
        NS_WARNING("Non DOM nsresult passed to a DOM exception!");

        *aCode = (PRUint32)mResult;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXPathException::GetMessage(char **aMessage)
{
    NS_ENSURE_ARG_POINTER(aMessage);

    if (mResult == NS_ERROR_DOM_INVALID_EXPRESSION_ERR) {
        *aMessage = nsCRT::strdup(NS_ERROR_DOM_INVALID_EXPRESSION_MSG);
    }
    else if (mResult == NS_ERROR_DOM_TYPE_ERR) {
        *aMessage = nsCRT::strdup(NS_ERROR_DOM_TYPE_MSG);
    }
    else {
        *aMessage = nsnull;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXPathException::GetResult(PRUint32* aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);

    *aResult = mResult;

    return NS_OK;
}

NS_IMETHODIMP
nsXPathException::GetName(char **aName)
{
    NS_ENSURE_ARG_POINTER(aName);

    if (mResult == NS_ERROR_DOM_INVALID_EXPRESSION_ERR) {
        *aName = nsCRT::strdup("NS_ERROR_DOM_INVALID_EXPRESSION_ERR");
    }
    else if (mResult == NS_ERROR_DOM_TYPE_ERR) {
        *aName = nsCRT::strdup("NS_ERROR_DOM_TYPE_ERR");
    }
    else {
        *aName = nsnull;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsXPathException::GetFilename(char **aFilename)
{
    if (mInner) {
        return mInner->GetFilename(aFilename);
    }

    NS_ENSURE_ARG_POINTER(aFilename);

    *aFilename = nsnull;

    return NS_OK;
}

NS_IMETHODIMP
nsXPathException::GetLineNumber(PRUint32 *aLineNumber)
{
    if (mInner) {
        return mInner->GetLineNumber(aLineNumber);
    }

    NS_ENSURE_ARG_POINTER(aLineNumber);

    *aLineNumber = 0;

    return NS_OK;
}

NS_IMETHODIMP
nsXPathException::GetColumnNumber(PRUint32 *aColumnNumber)
{
    if (mInner) {
        return mInner->GetColumnNumber(aColumnNumber);
    }

    NS_ENSURE_ARG_POINTER(aColumnNumber);

    *aColumnNumber = 0;

    return NS_OK;
}

NS_IMETHODIMP
nsXPathException::GetLocation(nsIStackFrame **aLocation)
{
    if (mInner) {
        return mInner->GetLocation(aLocation);
    }

    NS_ENSURE_ARG_POINTER(aLocation);

    *aLocation = nsnull;

    return NS_OK;
}

NS_IMETHODIMP
nsXPathException::GetInner(nsIException **aInner)
{
    NS_ENSURE_ARG_POINTER(aInner);

    *aInner = nsnull;

    return NS_OK;
}

NS_IMETHODIMP
nsXPathException::GetData(nsISupports **aData)
{
    if (mInner) {
        return mInner->GetData(aData);
    }

    NS_ENSURE_ARG_POINTER(aData);

    *aData = nsnull;

    return NS_OK;
}

NS_IMETHODIMP
nsXPathException::ToString(char **aReturn)
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

  char* msg;
  char* resultName;
  PRUint16 code;

  GetMessage(&msg);
  GetName(&resultName);
  GetCode(&code);

  *aReturn = PR_smprintf(format, (msg ? msg : defaultMsg), code, mResult,
                         (resultName ? resultName : defaultName), location.get());

  return *aReturn ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}


/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@propagandism.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsXPathException.h"
#include "nsCRT.h"
#include "nsIDOMClassInfo.h"
#include "nsIBaseDOMException.h"
#include "prprf.h"

static const char kInvalidExpressionErrName[] = "NS_ERROR_DOM_INVALID_EXPRESSION_ERR";
static const char kInvalidExpressionErrMessage[] = NS_ERROR_DOM_INVALID_EXPRESSION_MSG;
static const char kTypeErrName[] = "NS_ERROR_DOM_TYPE_ERR";
static const char kTypeErrMessage[] = NS_ERROR_DOM_TYPE_MSG;

static void
TXResultToNameAndMessage(nsresult aNSResult,
                         const char** aName,
                         const char** aMessage)
{
    if (aNSResult == NS_ERROR_DOM_INVALID_EXPRESSION_ERR) {
        *aName = kInvalidExpressionErrName;
        *aMessage = kInvalidExpressionErrMessage;
    }
    else if (aNSResult == NS_ERROR_DOM_TYPE_ERR) {
        *aName = kTypeErrName;
        *aMessage = kTypeErrMessage;
    }
    else {
        NS_WARNING("Huh, someone is throwing non-XPath DOM errors using the XPath DOM module!");
        *aName = nsnull;
        *aMessage = nsnull;
    }

    return;
}


IMPL_DOM_EXCEPTION_HEAD(nsXPathException, nsIDOMXPathException)
  NS_DECL_NSIDOMXPATHEXCEPTION
IMPL_DOM_EXCEPTION_TAIL(nsXPathException, nsIDOMXPathException, XPathException,
                        NS_ERROR_MODULE_DOM_XPATH, TXResultToNameAndMessage)

NS_IMETHODIMP
nsXPathException::GetCode(PRUint16* aCode)
{
    NS_ENSURE_ARG_POINTER(aCode);
    nsresult result;
    mBase->GetResult(&result);
    *aCode = NS_ERROR_GET_CODE(result);

    return NS_OK;
}


NS_IMPL_ISUPPORTS1(nsXPathExceptionProvider, nsIExceptionProvider)

nsXPathExceptionProvider::nsXPathExceptionProvider()
{
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

    NS_NewXPathException(aNSResult, aDefaultException, aException);
    NS_ENSURE_TRUE(*aException, NS_ERROR_OUT_OF_MEMORY);

    return NS_OK;
}

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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsSOAPException.h"
#include "nsReadableUtils.h"
#include "nsIXPConnect.h"

nsSOAPException::nsSOAPException(nsresult aStatus, const nsAString & aName, 
                                 const nsAString & aMessage, nsIException* aInner) :
                                 mStatus(aStatus),mName(aName),mMessage(aMessage),
                                 mInner(aInner)
{
  nsresult rc;
  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rc));
  if(NS_SUCCEEDED(rc)) {
    xpc->GetCurrentJSStack(getter_AddRefs(mFrame));
  }
}

nsSOAPException::~nsSOAPException()
{
}

NS_IMPL_ISUPPORTS1_CI(nsSOAPException, nsIException)

/* readonly attribute string message; */
NS_IMETHODIMP 
nsSOAPException::GetMessage(char * *aMessage)
{
  NS_ENSURE_ARG_POINTER(aMessage);

  *aMessage = ToNewUTF8String(mMessage);
  return NS_OK;
}

/* readonly attribute nsresult result; */
NS_IMETHODIMP 
nsSOAPException::GetResult(nsresult *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = mStatus;
  return NS_OK;
}

/* readonly attribute string name; */
NS_IMETHODIMP 
nsSOAPException::GetName(char * *aName)
{
  NS_ENSURE_ARG_POINTER(aName);

  *aName = ToNewUTF8String(mName);
  return NS_OK;
}

/* readonly attribute string filename; */
NS_IMETHODIMP 
nsSOAPException::GetFilename(char * *aFilename)
{
  NS_ENSURE_ARG_POINTER(aFilename);
  if (mFrame) {
    return mFrame->GetFilename(aFilename);
  }

  *aFilename = nsnull;
  return NS_OK;
}

/* readonly attribute PRUint32 lineNumber; */
NS_IMETHODIMP 
nsSOAPException::GetLineNumber(PRUint32 *aLineNumber)
{
  NS_ENSURE_ARG_POINTER(aLineNumber);
  if (mFrame) {
    PRInt32 l = 0;
    mFrame->GetLineNumber(&l);
    *aLineNumber = (PRUint32)l;
    return NS_OK;
  }

  *aLineNumber = 0;
  return NS_OK;
}

/* readonly attribute PRUint32 columnNumber; */
NS_IMETHODIMP 
nsSOAPException::GetColumnNumber(PRUint32 *aColumnNumber)
{
  NS_ENSURE_ARG_POINTER(aColumnNumber);

  *aColumnNumber = 0;
  return NS_OK;
}

/* readonly attribute nsIStackFrame location; */
NS_IMETHODIMP 
nsSOAPException::GetLocation(nsIStackFrame * *aLocation)
{
  NS_ENSURE_ARG_POINTER(aLocation);

  *aLocation = mFrame;
  NS_IF_ADDREF(*aLocation);
  return NS_OK;
}

/* readonly attribute nsIException inner; */
NS_IMETHODIMP 
nsSOAPException::GetInner(nsIException * *aInner)
{
  NS_ENSURE_ARG_POINTER(aInner);

  *aInner = mInner;
  NS_IF_ADDREF(*aInner);
  return NS_OK;
}

/* readonly attribute nsISupports data; */
NS_IMETHODIMP 
nsSOAPException::GetData(nsISupports * *aData)
{
  NS_ENSURE_ARG_POINTER(aData);

  *aData = nsnull;
  return NS_OK;
}

/* string toString (); */
NS_IMETHODIMP 
nsSOAPException::ToString(char **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  nsAutoString s;
  s.Append(mName);
  s.AppendLiteral(": ");
  s.Append(mMessage);
  if (mFrame) {
    char* str = nsnull;
    mFrame->ToString(&str);
    if (str) {
      s.AppendLiteral(", called by ");
      nsAutoString i;
      CopyASCIItoUCS2(nsDependentCString(str),i);
      nsMemory::Free(str);
      s.Append(i);
    }
  }
  if (mInner) {
    char* str = nsnull;
    mInner->ToString(&str);
    if (str) {
      nsAutoString i;
      CopyASCIItoUCS2(nsDependentCString(str),i);
      nsMemory::Free(str);
      s.AppendLiteral(", caused by ");
      s.Append(i);
    }
  }

  *_retval = ToNewUTF8String(s);
  return NS_OK;
}

nsresult nsSOAPException::AddException(nsresult aStatus, const nsAString & aName,
  const nsAString & aMessage,PRBool aClear)
{
  nsCOMPtr<nsIExceptionService> xs =
    do_GetService(NS_EXCEPTIONSERVICE_CONTRACTID);
  if (xs) {
    nsCOMPtr<nsIExceptionManager> xm;
    xs->GetCurrentExceptionManager(getter_AddRefs(xm));
    if (xm) {
      nsCOMPtr<nsIException> old;
      if (!aClear)
        xs->GetCurrentException(getter_AddRefs(old));
      nsCOMPtr<nsIException> exception = new nsSOAPException(aStatus, aName, 
        aMessage, old);
      if (exception) {
        xm->SetCurrentException(exception);
      }
    }
  }
  return aStatus;
}

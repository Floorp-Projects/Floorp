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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vidur Apparao (vidur@netscape.com) (Original author)
 *   John Bandhauer (jband@netscape.com)
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

#include "wspprivate.h"

WSPException::WSPException(nsISOAPFault* aFault, nsresult aStatus)
  : mFault(aFault), mData(nsnull), mStatus(aStatus), mMsg(nsnull)
{
}

WSPException::WSPException(nsresult aStatus, const char* aMsg, 
                           nsISupports* aData)
  : mFault(nsnull), mData(aData), mStatus(aStatus), mMsg(nsnull)
{
  if (aMsg) {
    mMsg = (char*) nsMemory::Clone(aMsg, strlen(aMsg)+1);
  }
}


WSPException::~WSPException()
{
  if (mMsg) {
    nsMemory::Free(mMsg);
  }
}

NS_IMPL_ISUPPORTS1_CI(WSPException, nsIException)

/* readonly attribute string message; */
NS_IMETHODIMP 
WSPException::GetMessage(char * *aMessage)
{
  NS_ENSURE_ARG_POINTER(aMessage);

  nsAutoString faultString;
  *aMessage = nsnull;
  if (mFault) {
    mFault->GetFaultString(faultString);
    *aMessage = ToNewUTF8String(faultString);
  }
  else if (mMsg) {
    *aMessage = (char*) nsMemory::Clone(mMsg, strlen(mMsg)+1);
  }

  return NS_OK;

}

/* readonly attribute nsresult result; */
NS_IMETHODIMP 
WSPException::GetResult(nsresult *aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = mStatus;
  return NS_OK;
}

/* readonly attribute string name; */
NS_IMETHODIMP 
WSPException::GetName(char * *aName)
{
  NS_ENSURE_ARG_POINTER(aName);
  nsAutoString faultCode;
  *aName = nsnull;
  if (mFault) {
    mFault->GetFaultCode(faultCode);
    *aName = ToNewUTF8String(faultCode);
  }
  return NS_OK;
}

/* readonly attribute string filename; */
NS_IMETHODIMP 
WSPException::GetFilename(char * *aFilename)
{
  NS_ENSURE_ARG_POINTER(aFilename);
  *aFilename = nsnull;
  return NS_OK;
}

/* readonly attribute PRUint32 lineNumber; */
NS_IMETHODIMP 
WSPException::GetLineNumber(PRUint32 *aLineNumber)
{
  NS_ENSURE_ARG_POINTER(aLineNumber);
  *aLineNumber = 0;
  return NS_OK;
}

/* readonly attribute PRUint32 columnNumber; */
NS_IMETHODIMP 
WSPException::GetColumnNumber(PRUint32 *aColumnNumber)
{
  NS_ENSURE_ARG_POINTER(aColumnNumber);
  *aColumnNumber = 0;
  return NS_OK;
}

/* readonly attribute nsIStackFrame location; */
NS_IMETHODIMP 
WSPException::GetLocation(nsIStackFrame * *aLocation)
{
  NS_ENSURE_ARG_POINTER(aLocation);
  *aLocation = nsnull;
  return NS_OK;
}

/* readonly attribute nsIException inner; */
NS_IMETHODIMP 
WSPException::GetInner(nsIException * *aInner)
{
  NS_ENSURE_ARG_POINTER(aInner);
  *aInner = nsnull;
  return NS_OK;
}

/* readonly attribute nsISupports data; */
NS_IMETHODIMP 
WSPException::GetData(nsISupports * *aData)
{
  NS_ENSURE_ARG_POINTER(aData);
  if (mFault) {
    *aData = mFault;
  }
  else if (mData) {
    *aData = mData;
  }
  else {
    *aData = nsnull;
  }

  NS_IF_ADDREF(*aData);
  return NS_OK;
}

/* string toString (); */
NS_IMETHODIMP 
WSPException::ToString(char **_retval)
{
  if (mFault) {
    return GetName(_retval);
  }
  // else
  return GetMessage(_retval);
}


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
#include "nsCRT.h"
#include "prprf.h"
#include "nsIScriptGlobalObject.h"
#include "nsReadableUtils.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

class nsDOMException : public nsIDOMNSDOMException
{
public:
  nsDOMException(nsresult aResult,
                 const char* aName,
                 const char* aMessage,
                 const char* aLocation);
  virtual ~nsDOMException();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMDOMEXCEPTION
  NS_DECL_NSIDOMNSDOMEXCEPTION

protected:
  nsresult mResult;
  char* mName;
  char* mMessage;
  char* mLocation;
};

nsresult 
NS_NewDOMException(nsIDOMDOMException** aException,
                   nsresult aResult, 
                   const char* aName, 
                   const char* aMessage,
                   const char* aLocation)
{
  NS_PRECONDITION(nsnull != aException, "null ptr");
  if (nsnull == aException) {
    return NS_ERROR_NULL_POINTER;
  }

  nsDOMException* it = new nsDOMException(aResult,
                                          aName,
                                          aMessage,
                                          aLocation);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  return it->QueryInterface(NS_GET_IID(nsIDOMDOMException),
                            (void**)aException);
}


nsDOMException::nsDOMException(nsresult aResult,
                               const char* aName,
                               const char* aMessage,
                               const char* aLocation)
  : mResult(aResult),
    mName(nsnull),
    mMessage(nsnull),
    mLocation(nsnull)
{
  NS_INIT_ISUPPORTS();

  if (aName) {
    mName = nsCRT::strdup(aName);
  }
  
  if (aMessage) {
    mMessage = nsCRT::strdup(aMessage);
  }

  if (aLocation) {
    mLocation = nsCRT::strdup(aLocation);
  }
}

nsDOMException::~nsDOMException()
{
  if (mName) {
    nsCRT::free(mName);
  }

  if (mMessage) {
    nsCRT::free(mMessage);
  }

  if (mLocation) {
    nsCRT::free(mLocation);
  }
}

NS_IMPL_ADDREF(nsDOMException)
NS_IMPL_RELEASE(nsDOMException)

NS_IMETHODIMP
nsDOMException::QueryInterface(const nsIID& aIID,
                               void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMDOMException))) {
    *aInstancePtrResult = (void*) ((nsIDOMDOMException*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*)(nsISupports*)(nsIDOMDOMException*)this;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP    
nsDOMException::GetCode(PRUint32* aCode)
{
  if (NS_ERROR_GET_MODULE(mResult) == NS_ERROR_MODULE_DOM) {
    *aCode = NS_ERROR_GET_CODE(mResult);
  }
  else {
    *aCode = (PRUint32)mResult;
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsDOMException::GetResult(PRUint32* aResult)
{
  *aResult = mResult;

  return NS_OK;
}

NS_IMETHODIMP    
nsDOMException::GetMessage(nsAWritableString& aMessage)
{
  if (mMessage) {
    CopyASCIItoUCS2(nsDependentCString(mMessage), aMessage);
  }
  else {
    aMessage.Truncate();
  }
  
  return NS_OK;
}

NS_IMETHODIMP    
nsDOMException::GetName(nsAWritableString& aName)
{
  if (mName) {
    CopyASCIItoUCS2(nsDependentCString(mName), aName);
  }
  else {
    aName.Truncate();
  }
  
  return NS_OK;
}

NS_IMETHODIMP    
nsDOMException::ToString(nsAWritableString& aReturn)
{
  static const char defaultMsg[] = "<no message>";
  static const char defaultLocation[] = "<unknown>";
  static const char defaultName[] = "<unknown>";
  static const char format[] =
    "[Exception... \"%s\"  code: \"%d\" nsresult: \"0x%x (%s)\"  location: \"%s\"]";
  
  const char* msg = mMessage ? mMessage : defaultMsg;
  const char* location = mLocation ? mLocation : defaultLocation;
  const char* resultName = mName ? mName : defaultName;
  PRUint32 code;

  GetCode(&code);
  char* temp = PR_smprintf(format, msg, code, mResult, resultName, location);
  if (temp) {
    CopyASCIItoUCS2(nsDependentCString(temp), aReturn);
    PR_smprintf_free(temp);
  }

  return NS_OK;
}

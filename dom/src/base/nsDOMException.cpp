/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsIDOMDOMException.h"
#include "nsIScriptObjectOwner.h"
#include "nsDOMError.h"
#include "prprf.h"
#include "nsIScriptGlobalObject.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

class nsDOMException : public nsIDOMDOMException,
                       public nsIScriptObjectOwner
{
public:
  nsDOMException(nsresult aResult,
                 const char* aName,
                 const char* aMessage,
                 const char* aLocation);
  virtual ~nsDOMException();

  NS_DECL_ISUPPORTS
  NS_DECL_IDOMDOMEXCEPTION

  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void* aScriptObject);

protected:
  nsresult mResult;
  char* mName;
  char* mMessage;
  char* mLocation;
  void *mScriptObject;
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
  
  return it->QueryInterface(nsIDOMDOMException::GetIID(),
                            (void**)aException);
}


nsDOMException::nsDOMException(nsresult aResult,
                               const char* aName,
                               const char* aMessage,
                               const char* aLocation)
  : mResult(aResult),
    mName(nsnull),
    mMessage(nsnull),
    mLocation(nsnull),
    mScriptObject(nsnull)
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
  if (aIID.Equals(nsIScriptObjectOwner::GetIID())) {
    *aInstancePtrResult = (void*) ((nsIScriptObjectOwner*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(nsIDOMDOMException::GetIID())) {
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
nsDOMException::GetMessage(nsString& aMessage)
{
  if (mMessage) {
    aMessage.SetString(mMessage);
  }
  else {
    aMessage.Truncate();
  }
  
  return NS_OK;
}

NS_IMETHODIMP    
nsDOMException::GetName(nsString& aName)
{
  if (mName) {
    aName.SetString(mName);
  }
  else {
    aName.Truncate();
  }
  
  return NS_OK;
}

NS_IMETHODIMP    
nsDOMException::ToString(nsString& aReturn)
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
    aReturn.SetString(temp);
    PR_smprintf_free(temp);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMException::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMException::GetScriptObject(nsIScriptContext *aContext, 
                                void** aScriptObject)
{
  NS_PRECONDITION(nsnull != aScriptObject, "null arg");
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    nsIScriptGlobalObject *global = aContext->GetGlobalObject();
    res = NS_NewScriptDOMException(aContext, (nsISupports *)(nsIDOMDOMException *)this, (nsISupports*)global, &mScriptObject);
    NS_IF_RELEASE(global);
  }
  
  *aScriptObject = mScriptObject;
  return res;
}

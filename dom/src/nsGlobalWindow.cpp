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

#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIScriptContext.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindow.h"
#include "nsIDOMDocument.h"
#include "nsHashtable.h"

#include "jsapi.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIDOMWindowIID, NS_IDOMWINDOW_IID);

// Global object for scripting
class GlobalWindowImpl : public nsIScriptObjectOwner, public nsIScriptGlobalObject, public nsIDOMWindow
{
public:
  GlobalWindowImpl();
  ~GlobalWindowImpl();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD ResetScriptObject();

  NS_IMETHOD_(void)       SetContext(nsIScriptContext *aContext);
  NS_IMETHOD_(void)       SetNewDocument(nsIDOMDocument *aDocument);

  NS_IMETHOD    GetDocument(nsIDOMDocument** aDocument);
  NS_IMETHOD    Dump(nsString& aStr);

protected:
  nsIScriptContext *mContext;
  void *mScriptObject;
  nsIDOMDocument *mDocument;
};

GlobalWindowImpl::GlobalWindowImpl()
{
  mContext = nsnull;
  mScriptObject = nsnull;
  mDocument = nsnull;
}

GlobalWindowImpl::~GlobalWindowImpl() 
{
  if (nsnull != mScriptObject) {
    JS_RemoveRoot((JSContext *)mContext->GetNativeContext(), &mScriptObject);
    mScriptObject = nsnull;
  }
  
  if (nsnull != mContext) {
    NS_RELEASE(mContext);
  }

  if (nsnull != mDocument) {
    NS_RELEASE(mDocument);
  }
}

NS_IMPL_ADDREF(GlobalWindowImpl)
NS_IMPL_RELEASE(GlobalWindowImpl)

nsresult 
GlobalWindowImpl::QueryInterface(const nsIID& aIID,
				 void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    *aInstancePtrResult = (void*) ((nsIScriptObjectOwner*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptGlobalObjectIID)) {
    *aInstancePtrResult = (void*) ((nsIScriptGlobalObject*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMWindowIID)) {
    *aInstancePtrResult = (void*) ((nsIDOMWindow*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*)(nsISupports*)(nsIScriptGlobalObject*)this;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult 
GlobalWindowImpl::ResetScriptObject()
{
  mScriptObject = nsnull;
  return NS_OK;
}

nsresult 
GlobalWindowImpl::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  NS_PRECONDITION(nsnull != aScriptObject, "null arg");
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    res = NS_NewScriptWindow(aContext, this, nsnull, &mScriptObject);
    JS_AddNamedRoot((JSContext *)aContext->GetNativeContext(),
		    &mScriptObject, "window_object");
  }
  
  *aScriptObject = mScriptObject;
  return res;
}

NS_IMETHODIMP_(void)       
GlobalWindowImpl::SetContext(nsIScriptContext *aContext)
{
  if (mContext) {
    NS_RELEASE(mContext);
  }

  mContext = aContext;
  NS_ADDREF(mContext);
}

NS_IMETHODIMP_(void)       
GlobalWindowImpl::SetNewDocument(nsIDOMDocument *aDocument)
{
  if (nsnull != mDocument) {
    if (nsnull != mScriptObject) {
      JS_ClearScope((JSContext *)mContext->GetNativeContext(),
		    (JSObject *)mScriptObject);
    }
    
    NS_RELEASE(mDocument);
  }

  mDocument = aDocument;
  
  if (nsnull != mDocument) {
    NS_ADDREF(mDocument);

    if (nsnull != mContext) {
      mContext->InitContext(this);
    }
  }
  else {
    JS_GC((JSContext *)mContext->GetNativeContext());
  }
}

NS_IMETHODIMP    
GlobalWindowImpl::GetDocument(nsIDOMDocument** aDocument)
{
  *aDocument = mDocument;
  NS_ADDREF(mDocument);

  return NS_OK;
}

NS_IMETHODIMP
GlobalWindowImpl::Dump(nsString& aStr)
{
  char *cstr = aStr.ToNewCString();
  
  if (nsnull != cstr) {
    printf("%s", cstr);
    delete [] cstr;
  }
  
  return NS_OK;
}

extern "C" NS_DOM 
NS_NewScriptGlobalObject(nsIScriptGlobalObject **aResult)
{
  if (nsnull == aResult) {  
    return NS_ERROR_NULL_POINTER;  
  }  

  *aResult = NULL;  
  
  GlobalWindowImpl *global = new GlobalWindowImpl();
  if (nsnull == global) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return global->QueryInterface(kIScriptGlobalObjectIID, (void **)aResult);
}

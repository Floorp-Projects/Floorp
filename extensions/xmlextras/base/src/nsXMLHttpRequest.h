/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsXMLHttpRequest_h__
#define nsXMLHttpRequest_h__

#define IMPLEMENT_SYNC_LOAD

#include "nsIXMLHttpRequest.h"
#include "nsISupportsUtils.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIDOMLoadListener.h"
#include "nsIDOMDocument.h"
#include "nsISecurityCheckedComponent.h"
#include "nsIURI.h"
#include "nsIHTTPChannel.h"
#include "nsIDocument.h"
#ifdef IMPLEMENT_SYNC_LOAD
#include "nsIDocShellTreeOwner.h"
#endif
#include "nsWeakReference.h"
#include "nsISupportsArray.h"
#include "jsapi.h"

enum {
  XML_HTTP_REQUEST_INITIALIZED,
  XML_HTTP_REQUEST_OPENED,
  XML_HTTP_REQUEST_SENT,
  XML_HTTP_REQUEST_COMPLETED,
  XML_HTTP_REQUEST_ABORTED
};

class nsXMLHttpRequest : public nsIXMLHttpRequest,
                         public nsIDOMLoadListener,
                         public nsISecurityCheckedComponent,
                         public nsSupportsWeakReference
{
public:
  nsXMLHttpRequest();
  virtual ~nsXMLHttpRequest();

  NS_DECL_ISUPPORTS

  // nsIXMLHttpRequest  
  NS_DECL_NSIXMLHTTPREQUEST

  // nsIDOMEventListener
  virtual nsresult HandleEvent(nsIDOMEvent* aEvent);

  // nsIDOMLoadListener
  virtual nsresult Load(nsIDOMEvent* aEvent);
  virtual nsresult Unload(nsIDOMEvent* aEvent);
  virtual nsresult Abort(nsIDOMEvent* aEvent);
  virtual nsresult Error(nsIDOMEvent* aEvent);

  // nsISecurityCheckedComponent
  NS_DECL_NSISECURITYCHECKEDCOMPONENT

protected:
  nsresult MakeScriptEventListener(nsISupports* aObject,
                                   nsIDOMEventListener** aListener);
  void GetScriptEventListener(nsISupportsArray* aList, 
                              nsIDOMEventListener** aListener);
  PRBool StuffReturnValue(nsIDOMEventListener* aListener);


  nsresult GetStreamForWString(const PRUnichar* aStr,
                               PRInt32 aLength,
                               nsIInputStream** aStream);

  nsCOMPtr<nsIHTTPChannel> mChannel;
  nsCOMPtr<nsIDOMDocument> mDocument;
  nsCOMPtr<nsIURI> mBaseURI;
  nsCOMPtr<nsIDocument> mBaseDocument;
#ifdef IMPLEMENT_SYNC_LOAD
  nsCOMPtr<nsIDocShellTreeOwner> mDocShellTreeOwner;
#endif
  nsCOMPtr<nsISupportsArray> mLoadEventListeners;
  nsCOMPtr<nsISupportsArray> mErrorEventListeners;
  PRInt32 mStatus;
  PRBool mAsync;
};

#define NS_IPRIVATEJSEVENTLISTENER_IID              \
 { /* d47a6550-4327-11d4-9a45-000064657374 */       \
  0xd47a6550, 0x4327, 0x11d4,                       \
 {0x9a, 0x45, 0x00, 0x00, 0x64, 0x65, 0x73, 0x74} }

class nsIPrivateJSEventListener : public nsISupports {
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPRIVATEJSEVENTLISTENER_IID)
    
  NS_IMETHOD GetFunctionObj(JSObject** aObj) = 0;
};

class nsXMLHttpRequestScriptListener : public nsIDOMEventListener,
                                       public nsIPrivateJSEventListener
{
public:
  nsXMLHttpRequestScriptListener(JSObject* aScopeObj, JSObject* aFunctionObj);
  virtual ~nsXMLHttpRequestScriptListener();

  NS_DECL_ISUPPORTS
  
  // nsIDOMEventListener
  virtual nsresult HandleEvent(nsIDOMEvent* aEvent);

  // nsIPrivateJSEventListener
  NS_IMETHOD GetFunctionObj(JSObject** aObj);
  
protected:  
  JSObject* mScopeObj;
  JSObject* mFunctionObj;
};

#endif

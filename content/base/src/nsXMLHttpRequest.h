/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#ifndef nsXMLHttpRequest_h__
#define nsXMLHttpRequest_h__

#define IMPLEMENT_SYNC_LOAD

#include "nsIXMLHttpRequest.h"
#include "nsISupportsUtils.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIDOMLoadListener.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMDocument.h"
#include "nsIURI.h"
#include "nsIHttpChannel.h"
#include "nsIDocument.h"
#include "nsIStreamListener.h"
#ifdef IMPLEMENT_SYNC_LOAD
#include "nsIWebBrowserChrome.h"
#endif
#include "nsWeakReference.h"
#include "nsISupportsArray.h"
#include "jsapi.h"
#include "nsIScriptContext.h"


enum nsXMLHttpRequestState {
  XML_HTTP_REQUEST_UNINITIALIZED = 0,
  XML_HTTP_REQUEST_OPENED, // aka LOADING
  XML_HTTP_REQUEST_LOADED,
  XML_HTTP_REQUEST_INTERACTIVE,
  XML_HTTP_REQUEST_COMPLETED,
  XML_HTTP_REQUEST_SENT, // This is Mozilla-internal only, LOADING in IE and external view
  XML_HTTP_REQUEST_STOPPED // This is Mozilla-internal only, INTERACTIVE in IE and external view
};

class nsXMLHttpRequest : public nsIXMLHttpRequest,
                         public nsIJSXMLHttpRequest,
                         public nsIDOMLoadListener,
                         public nsIDOMEventTarget,
                         public nsIStreamListener,
                         public nsSupportsWeakReference
{
public:
  nsXMLHttpRequest();
  virtual ~nsXMLHttpRequest();

  NS_DECL_ISUPPORTS

  // nsIXMLHttpRequest  
  NS_DECL_NSIXMLHTTPREQUEST

  // nsIJSXMLHttpRequest  
  NS_DECL_NSIJSXMLHTTPREQUEST

  // nsIDOMEventTarget
  NS_DECL_NSIDOMEVENTTARGET

  // nsIDOMEventListener
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

  // nsIDOMLoadListener
  NS_IMETHOD Load(nsIDOMEvent* aEvent);
  NS_IMETHOD Unload(nsIDOMEvent* aEvent);
  NS_IMETHOD Abort(nsIDOMEvent* aEvent);
  NS_IMETHOD Error(nsIDOMEvent* aEvent);

  // nsIStreamListener
  NS_DECL_NSISTREAMLISTENER

  // nsIRequestObserver
  NS_DECL_NSIREQUESTOBSERVER

protected:
  nsresult GetStreamForWString(const PRUnichar* aStr,
                               PRInt32 aLength,
                               nsIInputStream** aStream);
  nsresult DetectCharset(nsAWritableString& aCharset);
  nsresult ConvertBodyToText(PRUnichar **aOutBuffer);
  static NS_METHOD StreamReaderFunc(nsIInputStream* in,
                void* closure,
                const char* fromRawSegment,
                PRUint32 toOffset,
                PRUint32 count,
                PRUint32 *writeCount);
  // Change the state of the object with this. The broadcast member determines
  // if the onreadystatechange listener should be called.
  nsresult ChangeState(nsXMLHttpRequestState aState, PRBool aBroadcast = PR_TRUE);
  nsresult RequestCompleted();

  nsCOMPtr<nsISupports> mContext;
  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsIRequest> mReadRequest;
  nsCOMPtr<nsIDOMDocument> mDocument;
  nsCOMPtr<nsIURI> mBaseURI;
#ifdef IMPLEMENT_SYNC_LOAD
  nsCOMPtr<nsIWebBrowserChrome> mChromeWindow;
#endif

  nsCOMPtr<nsISupportsArray> mLoadEventListeners;
  nsCOMPtr<nsISupportsArray> mErrorEventListeners;
  nsCOMPtr<nsIScriptContext> mScriptContext;

  nsCOMPtr<nsIDOMEventListener> mOnLoadListener;
  nsCOMPtr<nsIDOMEventListener> mOnErrorListener;

  nsCOMPtr<nsIOnReadystatechangeHandler> mOnReadystatechangeListener;

  // used to implement getAllResponseHeaders()
  class nsHeaderVisitor : public nsIHttpHeaderVisitor {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIHTTPHEADERVISITOR
    nsHeaderVisitor() { NS_INIT_ISUPPORTS(); }
    virtual ~nsHeaderVisitor() {}
    const nsACString &Headers() { return mHeaders; }
  private:
    nsCString mHeaders;
  };

  nsCString mResponseBody;
  
  nsCOMPtr<nsIStreamListener> mXMLParserStreamListener;

  PRInt32 mStatus;
  PRBool mAsync;
};

#endif

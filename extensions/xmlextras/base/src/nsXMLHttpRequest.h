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
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#ifndef nsXMLHttpRequest_h__
#define nsXMLHttpRequest_h__

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
#include "nsIEventQueueService.h"
#include "nsWeakReference.h"
#include "nsISupportsArray.h"
#include "jsapi.h"
#include "nsIScriptContext.h"
#include "nsIHttpEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIHttpHeaderVisitor.h"
#include "nsIProgressEventSink.h"

#include "nsIDOMLSProgressEvent.h"

class nsILoadGroup;

class nsXMLHttpRequest : public nsIXMLHttpRequest,
                         public nsIJSXMLHttpRequest,
                         public nsIDOMLoadListener,
                         public nsIDOMEventTarget,
                         public nsIStreamListener,
                         public nsIHttpEventSink,
                         public nsIInterfaceRequestor,
                         public nsIProgressEventSink,
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
  NS_IMETHOD BeforeUnload(nsIDOMEvent* aEvent);
  NS_IMETHOD Unload(nsIDOMEvent* aEvent);
  NS_IMETHOD Abort(nsIDOMEvent* aEvent);
  NS_IMETHOD Error(nsIDOMEvent* aEvent);

  // nsIStreamListener
  NS_DECL_NSISTREAMLISTENER

  // nsIRequestObserver
  NS_DECL_NSIREQUESTOBSERVER

  // nsIHttpEventSink
  NS_DECL_NSIHTTPEVENTSINK

  // nsIProgressEventSink
  NS_DECL_NSIPROGRESSEVENTSINK

  // nsIInterfaceRequestor
  NS_DECL_NSIINTERFACEREQUESTOR
protected:
  nsresult GetStreamForWString(const PRUnichar* aStr,
                               PRInt32 aLength,
                               nsIInputStream** aStream);
  nsresult DetectCharset(nsACString& aCharset);
  nsresult ConvertBodyToText(nsAString& aOutBuffer);
  static NS_METHOD StreamReaderFunc(nsIInputStream* in,
                void* closure,
                const char* fromRawSegment,
                PRUint32 toOffset,
                PRUint32 count,
                PRUint32 *writeCount);
  // Change the state of the object with this. The broadcast argument
  // determines if the onreadystatechange listener should be called.
  // If aClearEventListeners is true, ChangeState will take refs to
  // any event listeners it needs and call ClearEventListeners before
  // making any HandleEvent() calls that could change the listener
  // values.
  nsresult ChangeState(PRUint32 aState, PRBool aBroadcast = PR_TRUE,
                       PRBool aClearEventListeners = PR_FALSE);
  nsresult RequestCompleted();
  nsresult GetLoadGroup(nsILoadGroup **aLoadGroup);
  nsIURI *GetBaseURI();
  nsresult CreateEvent(nsEvent* event, nsIDOMEvent** domevent);
  void NotifyEventListeners(nsIDOMEventListener* aHandler,
                            nsISupportsArray* aListeners, nsIDOMEvent* aEvent);
  void ClearEventListeners();
  already_AddRefed<nsIHttpChannel> GetCurrentHttpChannel();

  nsCOMPtr<nsISupports> mContext;
  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsIRequest> mReadRequest;
  nsCOMPtr<nsIDOMDocument> mDocument;

  nsCOMPtr<nsISupportsArray> mLoadEventListeners;
  nsCOMPtr<nsISupportsArray> mErrorEventListeners;
  nsCOMPtr<nsIScriptContext> mScriptContext;

  nsCOMPtr<nsIDOMEventListener> mOnLoadListener;
  nsCOMPtr<nsIDOMEventListener> mOnErrorListener;
  nsCOMPtr<nsIDOMEventListener> mOnProgressListener;

  nsCOMPtr<nsIOnReadyStateChangeHandler> mOnReadystatechangeListener;

  nsCOMPtr<nsIStreamListener> mXMLParserStreamListener;
  nsCOMPtr<nsIEventQueueService> mEventQService;

  // used to implement getAllResponseHeaders()
  class nsHeaderVisitor : public nsIHttpHeaderVisitor {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIHTTPHEADERVISITOR
    nsHeaderVisitor() { }
    virtual ~nsHeaderVisitor() {}
    const nsACString &Headers() { return mHeaders; }
  private:
    nsCString mHeaders;
  };

  nsCString mResponseBody;

  nsCString mOverrideMimeType;

  PRUint32 mState;
};


// helper class to expose a progress DOM Event

class nsXMLHttpProgressEvent : public nsIDOMLSProgressEvent
{
public:
  nsXMLHttpProgressEvent(nsIDOMEvent * aInner, PRUint32 aCurrentProgress, PRUint32 aMaxProgress);
  virtual ~nsXMLHttpProgressEvent();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMLSPROGRESSEVENT
  NS_DECL_NSIDOMEVENT

protected:
  nsCOMPtr<nsIDOMEvent> mInner;
  PRUint32 mCurProgress;
  PRUint32 mMaxProgress;
};

#endif

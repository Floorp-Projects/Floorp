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
#include "nsIStreamListener.h"
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
                         public nsIStreamListener,
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

  // nsIStreamListener & Observer
  NS_DECL_NSISTREAMOBSERVER
  NS_DECL_NSISTREAMLISTENER

protected:
  nsresult MakeScriptEventListener(nsISupports* aObject,
                                   nsIDOMEventListener** aListener);
  void GetScriptEventListener(nsISupportsArray* aList, 
                              nsIDOMEventListener** aListener);
  PRBool StuffReturnValue(nsIDOMEventListener* aListener);


  nsresult GetStreamForWString(const PRUnichar* aStr,
                               PRInt32 aLength,
                               nsIInputStream** aStream);

  nsCOMPtr<nsISupports> mContext;
  nsCOMPtr<nsIHTTPChannel> mChannel;
  nsCOMPtr<nsIRequest> mReadRequest;
  nsCOMPtr<nsIDOMDocument> mDocument;
  nsCOMPtr<nsIURI> mBaseURI;
  nsCOMPtr<nsIDocument> mBaseDocument;
#ifdef IMPLEMENT_SYNC_LOAD
  nsCOMPtr<nsIDocShellTreeOwner> mDocShellTreeOwner;
#endif
  nsCOMPtr<nsISupportsArray> mLoadEventListeners;
  nsCOMPtr<nsISupportsArray> mErrorEventListeners;
  
  nsresult DetectCharset(nsAWritableString& aCharset);
  nsresult ConvertBodyToText(PRUnichar **aOutBuffer);
  static NS_METHOD StreamReaderFunc(nsIInputStream* in,
                void* closure,
                const char* fromRawSegment,
                PRUint32 toOffset,
                PRUint32 count,
                PRUint32 *writeCount);

#if 1 // When nsCString::Append()/Length() works for strings that contain nulls, remove this buffer impl
  class ResponseBodyBuffer {
  public:
    ResponseBodyBuffer() 
      : mBuffer(0), 
        mBufferSize(0), 
        mBufferFreeIndex(0), 
        mCanBeString(PR_FALSE), 
        mBufferChecked(PR_FALSE) {}
    ~ResponseBodyBuffer() { nsMemory::Free(mBuffer); }
    void Truncate(void) {
      nsMemory::Free(mBuffer);
      mBuffer=nsnull;
      mBufferSize=mBufferFreeIndex=0;
      mCanBeString = PR_FALSE;
      mBufferChecked = PR_FALSE;
    }
    nsresult Append(const char* aBuffer, PRUint32 aBufferLen) {
      if (aBufferLen > (mBufferSize-mBufferFreeIndex)) {
        PRUint32 newBufferSize = (mBufferSize > aBufferLen ? mBufferSize : aBufferLen)<<1;
        char * newBuffer = NS_STATIC_CAST(char*,nsMemory::Realloc(mBuffer,newBufferSize));
        if (!newBuffer) {
          nsMemory::Free(mBuffer);
          mBuffer = nsnull;
          mBufferChecked = PR_FALSE;
          mCanBeString = PR_FALSE;
          return NS_ERROR_OUT_OF_MEMORY;
        }
        mBuffer = newBuffer;
        mBufferSize = newBufferSize;
      }
      memcpy(&mBuffer[mBufferFreeIndex],aBuffer,aBufferLen);
      mBufferFreeIndex += aBufferLen;
      mBufferChecked = PR_FALSE;
      mCanBeString = PR_FALSE;
      return NS_OK;
    }
    const char * GetBuffer(void) { return mBuffer; }
    PRUint32 GetBufferLength(void) { return mBufferFreeIndex; }
    PRBool CanBeString(void) {
      if (!mBufferFreeIndex)
        return PR_TRUE; // Perhaps this is a bit questionable...
      if (!mBufferChecked) {
        mBufferChecked = PR_TRUE;

        mCanBeString = PR_TRUE;

        PRUint32 i;
        for (i = 0; i < mBufferFreeIndex; i++) {
          if (!mBuffer[i]) {
            mCanBeString = PR_FALSE;
            break;
          }
        }
      }
      return mCanBeString;
    }
  private:
    char *mBuffer;
    PRUint32 mBufferSize;
    PRUint32 mBufferFreeIndex;
    PRPackedBool mCanBeString;
    PRPackedBool mBufferChecked;
  };
  ResponseBodyBuffer mResponseBody;
#else
  nsCString mResponseBody;
#endif
  
  nsCOMPtr<nsIStreamListener> mXMLParserStreamListener;

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

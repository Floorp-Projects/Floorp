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

#include "nsXMLHttpRequest.h"
#include "nsISimpleEnumerator.h"
#include "nsIXPConnect.h"
#include "nsICharsetConverterManager.h"
#include "nsLayoutCID.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIURI.h"
#include "nsILoadGroup.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "nsIUploadChannel.h"
#include "nsIDOMSerializer.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIDOMEventReceiver.h"
#include "nsIEventListenerManager.h"
#include "nsGUIEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "prprf.h"
#include "nsIDOMEventListener.h"
#include "nsIJSContextStack.h"
#include "nsIScriptSecurityManager.h"
#include "nsWeakPtr.h"
#include "nsICharsetAlias.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMClassInfo.h"
#include "nsIDOMElement.h"
#include "nsIDOMWindow.h"
#include "nsIVariant.h"
#include "nsIParser.h"
#include "nsLoadListenerProxy.h"
#include "nsIWindowWatcher.h"
#include "nsIAuthPrompt.h"
#include "nsStringStream.h"
#include "nsIStreamConverterService.h"
#include "nsICachingChannel.h"
#include "nsContentUtils.h"
#include "nsEventDispatcher.h"
#include "nsDOMJSUtils.h"
#include "nsCOMArray.h"
#include "nsDOMClassInfo.h"
#include "nsIScriptableUConv.h"
#include "nsCycleCollectionParticipant.h"

#define LOAD_STR "load"
#define ERROR_STR "error"
#define PROGRESS_STR "progress"
#define UPLOADPROGRESS_STR "uploadprogress"
#define READYSTATE_STR "readystatechange"

// CIDs

// State
#define XML_HTTP_REQUEST_UNINITIALIZED  (1 << 0)  // 0
#define XML_HTTP_REQUEST_OPENED         (1 << 1)  // 1 aka LOADING
#define XML_HTTP_REQUEST_LOADED         (1 << 2)  // 2
#define XML_HTTP_REQUEST_INTERACTIVE    (1 << 3)  // 3
#define XML_HTTP_REQUEST_COMPLETED      (1 << 4)  // 4
#define XML_HTTP_REQUEST_SENT           (1 << 5)  // Internal, LOADING in IE and external view
#define XML_HTTP_REQUEST_STOPPED        (1 << 6)  // Internal, INTERACTIVE in IE and external view
// The above states are mutually exclusive, change with ChangeState() only.
// The states below can be combined.
#define XML_HTTP_REQUEST_ABORTED        (1 << 7)  // Internal
#define XML_HTTP_REQUEST_ASYNC          (1 << 8)  // Internal
#define XML_HTTP_REQUEST_PARSEBODY      (1 << 9)  // Internal
#define XML_HTTP_REQUEST_XSITEENABLED   (1 << 10) // Internal
#define XML_HTTP_REQUEST_SYNCLOOPING    (1 << 11) // Internal
#define XML_HTTP_REQUEST_MULTIPART      (1 << 12) // Internal

#define XML_HTTP_REQUEST_LOADSTATES         \
  (XML_HTTP_REQUEST_UNINITIALIZED |         \
   XML_HTTP_REQUEST_OPENED |                \
   XML_HTTP_REQUEST_LOADED |                \
   XML_HTTP_REQUEST_INTERACTIVE |           \
   XML_HTTP_REQUEST_COMPLETED |             \
   XML_HTTP_REQUEST_SENT |                  \
   XML_HTTP_REQUEST_STOPPED)

// This helper function adds the given load flags to the request's existing
// load flags.
static void AddLoadFlags(nsIRequest *request, nsLoadFlags newFlags)
{
  nsLoadFlags flags;
  request->GetLoadFlags(&flags);
  flags |= newFlags;
  request->SetLoadFlags(flags);
}

// Helper proxy class to be used when expecting an
// multipart/x-mixed-replace stream of XML documents.

class nsMultipartProxyListener : public nsIStreamListener
{
public:
  nsMultipartProxyListener(nsIStreamListener *dest);
  virtual ~nsMultipartProxyListener();

  /* additional members */
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER

private:
  nsCOMPtr<nsIStreamListener> mDestListener;
};


nsMultipartProxyListener::nsMultipartProxyListener(nsIStreamListener *dest)
  : mDestListener(dest)
{
}

nsMultipartProxyListener::~nsMultipartProxyListener()
{
}

NS_IMPL_ISUPPORTS2(nsMultipartProxyListener, nsIStreamListener,
                   nsIRequestObserver)

/** nsIRequestObserver methods **/

NS_IMETHODIMP
nsMultipartProxyListener::OnStartRequest(nsIRequest *aRequest,
                                         nsISupports *ctxt)
{
  nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
  NS_ENSURE_TRUE(channel, NS_ERROR_UNEXPECTED);

  nsCAutoString contentType;
  nsresult rv = channel->GetContentType(contentType);

  if (!contentType.EqualsLiteral("multipart/x-mixed-replace")) {
    return NS_ERROR_INVALID_ARG;
  }

  // If multipart/x-mixed-replace content, we'll insert a MIME
  // decoder in the pipeline to handle the content and pass it along
  // to our original listener.

  nsCOMPtr<nsIStreamConverterService> convServ =
    do_GetService("@mozilla.org/streamConverters;1", &rv);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIStreamListener> toListener(mDestListener);
    nsCOMPtr<nsIStreamListener> fromListener;

    rv = convServ->AsyncConvertData("multipart/x-mixed-replace",
                                    "*/*",
                                    toListener,
                                    nsnull,
                                    getter_AddRefs(fromListener));
    NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && fromListener, NS_ERROR_UNEXPECTED);

    mDestListener = fromListener;
  }

  return mDestListener->OnStartRequest(aRequest, ctxt);
}

NS_IMETHODIMP
nsMultipartProxyListener::OnStopRequest(nsIRequest *aRequest,
                                        nsISupports *ctxt,
                                        nsresult status)
{
  return mDestListener->OnStopRequest(aRequest, ctxt, status);
}

/** nsIStreamListener methods **/

NS_IMETHODIMP
nsMultipartProxyListener::OnDataAvailable(nsIRequest *aRequest,
                                          nsISupports *ctxt,
                                          nsIInputStream *inStr,
                                          PRUint32 sourceOffset,
                                          PRUint32 count)
{
  return mDestListener->OnDataAvailable(aRequest, ctxt, inStr, sourceOffset,
                                        count);
}


static nsIScriptContext *
GetCurrentContext()
{
  // Get JSContext from stack.
  nsCOMPtr<nsIJSContextStack> stack =
    do_GetService("@mozilla.org/js/xpc/ContextStack;1");

  if (!stack) {
    return nsnull;
  }

  JSContext *cx;

  if (NS_FAILED(stack->Peek(&cx)) || !cx) {
    return nsnull;
  }

  return GetScriptContextFromJSContext(cx);
}

/**
 * Gets the nsIDocument given the script context. Will return nsnull on failure.
 *
 * @param aScriptContext the script context to get the document for; can be null
 *
 * @return the document associated with the script context
 */
static already_AddRefed<nsIDocument>
GetDocumentFromScriptContext(nsIScriptContext *aScriptContext)
{
  if (!aScriptContext)
    return nsnull;

  nsCOMPtr<nsIDOMWindow> window =
    do_QueryInterface(aScriptContext->GetGlobalObject());
  nsIDocument *doc = nsnull;
  if (window) {
    nsCOMPtr<nsIDOMDocument> domdoc;
    window->GetDocument(getter_AddRefs(domdoc));
    if (domdoc) {
      CallQueryInterface(domdoc, &doc);
    }
  }
  return doc;
}

/////////////////////////////////////////////
//
//
/////////////////////////////////////////////

nsXMLHttpRequest::nsXMLHttpRequest()
  : mState(XML_HTTP_REQUEST_UNINITIALIZED)
{
}

nsXMLHttpRequest::~nsXMLHttpRequest()
{
  if (mState & (XML_HTTP_REQUEST_STOPPED |
                XML_HTTP_REQUEST_SENT |
                XML_HTTP_REQUEST_INTERACTIVE)) {
    Abort();
  }

  NS_ABORT_IF_FALSE(!(mState & XML_HTTP_REQUEST_SYNCLOOPING), "we rather crash than hang");
  mState &= ~XML_HTTP_REQUEST_SYNCLOOPING;

  // Needed to free the listener arrays.
  ClearEventListeners();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXMLHttpRequest)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsXMLHttpRequest, nsIXMLHttpRequest)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mChannel)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mReadRequest)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(mLoadEventListeners);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(mErrorEventListeners);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(mProgressEventListeners);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(mUploadProgressEventListeners);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(mReadystatechangeEventListeners);

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mScriptContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnLoadListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnErrorListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnProgressListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnUploadProgressListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnReadystatechangeListener)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mXMLParserStreamListener)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mChannelEventSink)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mProgressEventSink)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END


NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsXMLHttpRequest, nsIXMLHttpRequest)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mContext)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mChannel)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mReadRequest)

  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMARRAY(mLoadEventListeners);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMARRAY(mErrorEventListeners);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMARRAY(mProgressEventListeners);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMARRAY(mUploadProgressEventListeners);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMARRAY(mReadystatechangeEventListeners);

  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mScriptContext)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnLoadListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnErrorListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnProgressListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnUploadProgressListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnReadystatechangeListener)

  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mXMLParserStreamListener)

  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mChannelEventSink)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mProgressEventSink)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END


// QueryInterface implementation for nsXMLHttpRequest
NS_INTERFACE_MAP_BEGIN(nsXMLHttpRequest)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXMLHttpRequest)
  NS_INTERFACE_MAP_ENTRY(nsIXMLHttpRequest)
  NS_INTERFACE_MAP_ENTRY(nsIJSXMLHttpRequest)
  NS_INTERFACE_MAP_ENTRY(nsIDOMLoadListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIChannelEventSink)
  NS_INTERFACE_MAP_ENTRY(nsIProgressEventSink)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(XMLHttpRequest)
  NS_INTERFACE_MAP_ENTRY_CYCLE_COLLECTION(nsXMLHttpRequest)
NS_INTERFACE_MAP_END


NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(nsXMLHttpRequest, nsIXMLHttpRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS(nsXMLHttpRequest, nsIXMLHttpRequest)


/* void addEventListener (in string type, in nsIDOMEventListener
   listener); */
NS_IMETHODIMP
nsXMLHttpRequest::AddEventListener(const nsAString& type,
                                   nsIDOMEventListener *listener,
                                   PRBool useCapture)
{
  NS_ENSURE_ARG(listener);

  nsCOMArray<nsIDOMEventListener> *array;

#define IMPL_ADD_LISTENER(_type, _member)    \
  if (type.EqualsLiteral(_type)) {           \
    array = &(_member);                      \
  } else

  IMPL_ADD_LISTENER(LOAD_STR, mLoadEventListeners)
  IMPL_ADD_LISTENER(ERROR_STR, mErrorEventListeners)
  IMPL_ADD_LISTENER(PROGRESS_STR, mProgressEventListeners)
  IMPL_ADD_LISTENER(UPLOADPROGRESS_STR, mUploadProgressEventListeners)
  IMPL_ADD_LISTENER(READYSTATE_STR, mReadystatechangeEventListeners)
  {
    return NS_ERROR_INVALID_ARG;
  }

  array->AppendObject(listener);

  mScriptContext = GetCurrentContext();

#undef IMPL_ADD_LISTENER
  
  return NS_OK;
}

/* void removeEventListener (in string type, in nsIDOMEventListener
   listener); */
NS_IMETHODIMP
nsXMLHttpRequest::RemoveEventListener(const nsAString & type,
                                      nsIDOMEventListener *listener,
                                      PRBool useCapture)
{
  NS_ENSURE_ARG(listener);

  nsCOMArray<nsIDOMEventListener> *array;
#define IMPL_REMOVE_LISTENER(_type, _member)  \
  if (type.EqualsLiteral(_type)) {            \
    array = &(_member);                       \
  } else

  IMPL_REMOVE_LISTENER(LOAD_STR, mLoadEventListeners)
  IMPL_REMOVE_LISTENER(ERROR_STR, mErrorEventListeners)
  IMPL_REMOVE_LISTENER(PROGRESS_STR, mProgressEventListeners)
  IMPL_REMOVE_LISTENER(UPLOADPROGRESS_STR, mUploadProgressEventListeners)
  IMPL_REMOVE_LISTENER(READYSTATE_STR, mReadystatechangeEventListeners)
  {
    return NS_ERROR_INVALID_ARG;
  }

  // Allow a caller to remove O(N^2) behavior by removing end-to-start.
  for (PRUint32 i = array->Count() - 1; i != PRUint32(-1); --i) {
    if (array->ObjectAt(i) == listener) {
      array->RemoveObjectAt(i);
      break;
    }
  }

  return NS_OK;
}

/* boolean dispatchEvent (in nsIDOMEvent evt); */
NS_IMETHODIMP
nsXMLHttpRequest::DispatchEvent(nsIDOMEvent *evt, PRBool *_retval)
{
  // Ignored

  return NS_OK;
}

/* attribute nsIDOMEventListener onreadystatechange; */
NS_IMETHODIMP
nsXMLHttpRequest::GetOnreadystatechange(nsIDOMEventListener * *aOnreadystatechange)
{
  NS_ENSURE_ARG_POINTER(aOnreadystatechange);

  NS_IF_ADDREF(*aOnreadystatechange = mOnReadystatechangeListener);

  return NS_OK;
}

NS_IMETHODIMP
nsXMLHttpRequest::SetOnreadystatechange(nsIDOMEventListener * aOnreadystatechange)
{
  mOnReadystatechangeListener = aOnreadystatechange;

  mScriptContext = GetCurrentContext();

  return NS_OK;
}


/* attribute nsIDOMEventListener onload; */
NS_IMETHODIMP
nsXMLHttpRequest::GetOnload(nsIDOMEventListener * *aOnLoad)
{
  NS_ENSURE_ARG_POINTER(aOnLoad);

  NS_IF_ADDREF(*aOnLoad = mOnLoadListener);

  return NS_OK;
}

NS_IMETHODIMP
nsXMLHttpRequest::SetOnload(nsIDOMEventListener * aOnLoad)
{
  mOnLoadListener = aOnLoad;

  mScriptContext = GetCurrentContext();

  return NS_OK;
}

/* attribute nsIDOMEventListener onerror; */
NS_IMETHODIMP
nsXMLHttpRequest::GetOnerror(nsIDOMEventListener * *aOnerror)
{
  NS_ENSURE_ARG_POINTER(aOnerror);

  NS_IF_ADDREF(*aOnerror = mOnErrorListener);

  return NS_OK;
}

NS_IMETHODIMP
nsXMLHttpRequest::SetOnerror(nsIDOMEventListener * aOnerror)
{
  mOnErrorListener = aOnerror;

  mScriptContext = GetCurrentContext();

  return NS_OK;
}

/* attribute nsIDOMEventListener onprogress; */
NS_IMETHODIMP
nsXMLHttpRequest::GetOnprogress(nsIDOMEventListener * *aOnprogress)
{
  NS_ENSURE_ARG_POINTER(aOnprogress);  

  NS_IF_ADDREF(*aOnprogress = mOnProgressListener);

  return NS_OK;
}

NS_IMETHODIMP
nsXMLHttpRequest::SetOnprogress(nsIDOMEventListener * aOnprogress)
{
  mOnProgressListener = aOnprogress;

  mScriptContext = GetCurrentContext();

  return NS_OK;
}

/* attribute nsIDOMEventListener onuploadprogress; */
NS_IMETHODIMP
nsXMLHttpRequest::GetOnuploadprogress(nsIDOMEventListener * *aOnuploadprogress)
{
  NS_ENSURE_ARG_POINTER(aOnuploadprogress);  

  NS_IF_ADDREF(*aOnuploadprogress = mOnUploadProgressListener);

  return NS_OK;
}

NS_IMETHODIMP
nsXMLHttpRequest::SetOnuploadprogress(nsIDOMEventListener * aOnuploadprogress)
{
  mOnUploadProgressListener = aOnuploadprogress;

  mScriptContext = GetCurrentContext();

  return NS_OK;
}

/* readonly attribute nsIChannel channel; */
NS_IMETHODIMP
nsXMLHttpRequest::GetChannel(nsIChannel **aChannel)
{
  NS_ENSURE_ARG_POINTER(aChannel);
  NS_IF_ADDREF(*aChannel = mChannel);

  return NS_OK;
}

/* readonly attribute nsIDOMDocument responseXML; */
NS_IMETHODIMP
nsXMLHttpRequest::GetResponseXML(nsIDOMDocument **aResponseXML)
{
  NS_ENSURE_ARG_POINTER(aResponseXML);
  *aResponseXML = nsnull;
  if ((XML_HTTP_REQUEST_COMPLETED & mState) && mDocument) {
    *aResponseXML = mDocument;
    NS_ADDREF(*aResponseXML);
  }

  return NS_OK;
}

/*
 * This piece copied from nsXMLDocument, we try to get the charset
 * from HTTP headers.
 */
nsresult
nsXMLHttpRequest::DetectCharset(nsACString& aCharset)
{
  aCharset.Truncate();
  nsresult rv;
  nsCAutoString charsetVal;
  nsCOMPtr<nsIChannel> channel(do_QueryInterface(mReadRequest));
  if (!channel) {
    channel = mChannel;
    if (!channel) {
      // There will be no mChannel when we got a necko error in
      // OnStopRequest or if we were never sent.
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  rv = channel->GetContentCharset(charsetVal);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsICharsetAlias> calias(do_GetService(NS_CHARSETALIAS_CONTRACTID,&rv));
    if(NS_SUCCEEDED(rv) && calias) {
      rv = calias->GetPreferred(charsetVal, aCharset);
    }
  }
  return rv;
}

nsresult
nsXMLHttpRequest::ConvertBodyToText(nsAString& aOutBuffer)
{
  // This code here is basically a copy of a similar thing in
  // nsScanner::Append(const char* aBuffer, PRUint32 aLen).
  // If we get illegal characters in the input we replace
  // them and don't just fail.

  PRInt32 dataLen = mResponseBody.Length();
  if (!dataLen)
    return NS_OK;

  nsresult rv = NS_OK;

  nsCAutoString dataCharset;
  nsCOMPtr<nsIDocument> document(do_QueryInterface(mDocument));
  if (document) {
    dataCharset = document->GetDocumentCharacterSet();
  } else {
    if (NS_FAILED(DetectCharset(dataCharset)) || dataCharset.IsEmpty()) {
      // MS documentation states UTF-8 is default for responseText
      dataCharset.AssignLiteral("UTF-8");
    }
  }

  if (dataCharset.EqualsLiteral("ASCII")) {
    CopyASCIItoUTF16(mResponseBody, aOutBuffer);

    return NS_OK;
  }

  nsCOMPtr<nsICharsetConverterManager> ccm =
    do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIUnicodeDecoder> decoder;
  rv = ccm->GetUnicodeDecoderRaw(dataCharset.get(),
                                 getter_AddRefs(decoder));
  if (NS_FAILED(rv))
    return rv;

  const char * inBuffer = mResponseBody.get();
  PRInt32 outBufferLength;
  rv = decoder->GetMaxLength(inBuffer, dataLen, &outBufferLength);
  if (NS_FAILED(rv))
    return rv;

  PRUnichar * outBuffer =
    NS_STATIC_CAST(PRUnichar*, nsMemory::Alloc((outBufferLength + 1) *
                                               sizeof(PRUnichar)));
  if (!outBuffer) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  PRInt32 totalChars = 0,
          outBufferIndex = 0,
          outLen = outBufferLength;

  do {
    PRInt32 inBufferLength = dataLen;
    rv = decoder->Convert(inBuffer,
                          &inBufferLength,
                          &outBuffer[outBufferIndex],
                          &outLen);
    totalChars += outLen;
    if (NS_FAILED(rv)) {
      // We consume one byte, replace it with U+FFFD
      // and try the conversion again.
      outBuffer[outBufferIndex + outLen++] = (PRUnichar)0xFFFD;
      outBufferIndex += outLen;
      outLen = outBufferLength - (++totalChars);

      decoder->Reset();

      if((inBufferLength + 1) > dataLen) {
        inBufferLength = dataLen;
      } else {
        inBufferLength++;
      }

      inBuffer = &inBuffer[inBufferLength];
      dataLen -= inBufferLength;
    }
  } while ( NS_FAILED(rv) && (dataLen > 0) );

  aOutBuffer.Assign(outBuffer, totalChars);
  nsMemory::Free(outBuffer);

  return NS_OK;
}

/* readonly attribute AString responseText; */
NS_IMETHODIMP nsXMLHttpRequest::GetResponseText(nsAString& aResponseText)
{
  nsresult rv = NS_OK;

  aResponseText.Truncate();

  if (mState & (XML_HTTP_REQUEST_COMPLETED |
                XML_HTTP_REQUEST_INTERACTIVE)) {
    rv = ConvertBodyToText(aResponseText);
  }

  return rv;
}

/* readonly attribute unsigned long status; */
NS_IMETHODIMP
nsXMLHttpRequest::GetStatus(PRUint32 *aStatus)
{
  nsCOMPtr<nsIHttpChannel> httpChannel = GetCurrentHttpChannel();

  if (httpChannel) {
    nsresult rv = httpChannel->GetResponseStatus(aStatus);
    if (rv == NS_ERROR_NOT_AVAILABLE) {
      // Someone's calling this before we got a response... Check our
      // ReadyState.  If we're at 3 or 4, then this means the connection
      // errored before we got any data; return a somewhat sensible error code
      // in that case.
      PRInt32 readyState;
      GetReadyState(&readyState);
      if (readyState >= 3) {
        *aStatus = NS_ERROR_NOT_AVAILABLE;
        return NS_OK;
      }
    }

    return rv;
  }
  *aStatus = 0;

  return NS_OK;
}

/* readonly attribute AUTF8String statusText; */
NS_IMETHODIMP
nsXMLHttpRequest::GetStatusText(nsACString& aStatusText)
{
  nsCOMPtr<nsIHttpChannel> httpChannel = GetCurrentHttpChannel();

  aStatusText.Truncate();

  nsresult rv = NS_OK;

  if (httpChannel) {
    rv = httpChannel->GetResponseStatusText(aStatusText);
  }

  return rv;
}

/* void abort (); */
NS_IMETHODIMP
nsXMLHttpRequest::Abort()
{
  if (mReadRequest) {
    mReadRequest->Cancel(NS_BINDING_ABORTED);
  }
  if (mChannel) {
    mChannel->Cancel(NS_BINDING_ABORTED);
  }
  mDocument = nsnull;
  mState |= XML_HTTP_REQUEST_ABORTED;

  ChangeState(XML_HTTP_REQUEST_COMPLETED, PR_TRUE, PR_TRUE);

  // The ChangeState call above calls onreadystatechange handlers which
  // if they load a new url will cause nsXMLHttpRequest::OpenRequest to clear
  // the abort state bit. If this occurs we're not uninitialized (bug 361773).
  if (mState & XML_HTTP_REQUEST_ABORTED) {
    ChangeState(XML_HTTP_REQUEST_UNINITIALIZED, PR_FALSE);  // IE seems to do it
  }

  return NS_OK;
}

/* string getAllResponseHeaders (); */
NS_IMETHODIMP
nsXMLHttpRequest::GetAllResponseHeaders(char **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;

  nsCOMPtr<nsIHttpChannel> httpChannel = GetCurrentHttpChannel();

  if (httpChannel) {
    nsHeaderVisitor *visitor = nsnull;
    NS_NEWXPCOM(visitor, nsHeaderVisitor);
    if (!visitor)
      return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(visitor);

    nsresult rv = httpChannel->VisitResponseHeaders(visitor);
    if (NS_SUCCEEDED(rv))
      *_retval = ToNewCString(visitor->Headers());

    NS_RELEASE(visitor);
    return rv;
  }

  return NS_OK;
}

/* ACString getResponseHeader (in AUTF8String header); */
NS_IMETHODIMP
nsXMLHttpRequest::GetResponseHeader(const nsACString& header,
                                    nsACString& _retval)
{
  nsresult rv = NS_OK;
  _retval.Truncate();

  nsCOMPtr<nsIHttpChannel> httpChannel = GetCurrentHttpChannel();

  if (httpChannel) {
    rv = httpChannel->GetResponseHeader(header, _retval);
  }

  if (rv == NS_ERROR_NOT_AVAILABLE) {
    // Means no header
    _retval.SetIsVoid(PR_TRUE);
    rv = NS_OK;
  }

  return rv;
}

nsresult
nsXMLHttpRequest::GetLoadGroup(nsILoadGroup **aLoadGroup)
{
  NS_ENSURE_ARG_POINTER(aLoadGroup);
  *aLoadGroup = nsnull;

  if (!mScriptContext) {
    mScriptContext = GetCurrentContext();
  }

  nsCOMPtr<nsIDocument> doc = GetDocumentFromScriptContext(mScriptContext);
  if (doc) {
    *aLoadGroup = doc->GetDocumentLoadGroup().get();  // already_AddRefed
  }

  return NS_OK;
}

nsIURI *
nsXMLHttpRequest::GetBaseURI()
{
  if (!mScriptContext) {
    mScriptContext = GetCurrentContext();
    if (!mScriptContext) {
      return nsnull;
    }
  }

  nsCOMPtr<nsIDocument> doc = GetDocumentFromScriptContext(mScriptContext);
  if (!doc) {
    return nsnull;
  }

  return doc->GetBaseURI();
}

nsresult
nsXMLHttpRequest::CreateEvent(const nsAString& aType, nsIDOMEvent** aDOMEvent)
{
  nsresult rv = nsEventDispatcher::CreateEvent(nsnull, nsnull,
                                               NS_LITERAL_STRING("Events"),
                                               aDOMEvent);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIPrivateDOMEvent> privevent(do_QueryInterface(*aDOMEvent));
  if (!privevent) {
    NS_IF_RELEASE(*aDOMEvent);
    return NS_ERROR_FAILURE;
  }

  if (!aType.IsEmpty()) {
    (*aDOMEvent)->InitEvent(aType, PR_FALSE, PR_FALSE);
  }
  
  privevent->SetTarget(this);
  privevent->SetCurrentTarget(this);
  privevent->SetOriginalTarget(this);

  // We assume anyone who managed to call CreateEvent is trusted
  privevent->SetTrusted(PR_TRUE);

  return NS_OK;
}

void
nsXMLHttpRequest::CopyEventListeners(nsCOMPtr<nsIDOMEventListener>& aListener,
                                     const nsCOMArray<nsIDOMEventListener>& aListenerArray,
                                     nsCOMArray<nsIDOMEventListener>& aCopy)
{
  NS_PRECONDITION(aCopy.Count() == 0, "aCopy should start off empty");
  if (aListener)
    aCopy.AppendObject(aListener);

  aCopy.AppendObjects(aListenerArray);
}

void
nsXMLHttpRequest::NotifyEventListeners(const nsCOMArray<nsIDOMEventListener>& aListeners,
                                       nsIDOMEvent* aEvent)
{
  // XXXbz wouldn't it be easier to just have an actual nsEventListenerManager
  // to work with or something?  I feel like we're duplicating code here...
  if (!aEvent)
    return;

  nsCOMPtr<nsIJSContextStack> stack;
  JSContext *cx = nsnull;

  if (mScriptContext) {
    stack = do_GetService("@mozilla.org/js/xpc/ContextStack;1");

    if (stack) {
      cx = (JSContext *)mScriptContext->GetNativeContext();

      if (cx) {
        stack->Push(cx);
      }
    }
  }

  PRInt32 count = aListeners.Count();
  for (PRInt32 index = 0; index < count; ++index) {
    nsIDOMEventListener* listener = aListeners[index];
    
    if (listener) {
      listener->HandleEvent(aEvent);
    }
  }

  if (cx) {
    stack->Pop(&cx);
  }
}

void
nsXMLHttpRequest::ClearEventListeners()
{
  // This isn't *really* needed anymore now that we use a cycle
  // collector, but we may as well keep it for safety (against leaks)
  // and compatibility, and also for the code to clear the first
  // listener arrays (called from the destructor).
  // XXXbz per spec we shouldn't be doing this, actually.  And we
  // don't need to clear the arrays from the destructor now that we're
  // using nsCOMArray.

  mLoadEventListeners.Clear();
  mErrorEventListeners.Clear();
  mProgressEventListeners.Clear();
  mUploadProgressEventListeners.Clear();
  mReadystatechangeEventListeners.Clear();

  mOnLoadListener = nsnull;
  mOnErrorListener = nsnull;
  mOnProgressListener = nsnull;
  mOnUploadProgressListener = nsnull;
  mOnReadystatechangeListener = nsnull;
}

already_AddRefed<nsIHttpChannel>
nsXMLHttpRequest::GetCurrentHttpChannel()
{
  nsIHttpChannel *httpChannel = nsnull;

  if (mReadRequest) {
    CallQueryInterface(mReadRequest, &httpChannel);
  }

  if (!httpChannel && mChannel) {
    CallQueryInterface(mChannel, &httpChannel);
  }

  return httpChannel;
}


/* noscript void openRequest (in AUTF8String method, in AUTF8String url, in boolean async, in AString user, in AString password); */
NS_IMETHODIMP
nsXMLHttpRequest::OpenRequest(const nsACString& method,
                              const nsACString& url,
                              PRBool async,
                              const nsAString& user,
                              const nsAString& password)
{
  NS_ENSURE_ARG(!method.IsEmpty());
  NS_ENSURE_ARG(!url.IsEmpty());

  // Disallow HTTP/1.1 TRACE method (see bug 302489).
  if (method.LowerCaseEqualsASCII("trace")) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv;
  nsCOMPtr<nsIURI> uri;
  PRBool authp = PR_FALSE;

  if (mState & XML_HTTP_REQUEST_ABORTED) {
    // Something caused this request to abort (e.g the current request
    // was caceled, channels closed etc), most likely the abort()
    // function was called by script. Unset our aborted state, and
    // proceed as normal

    mState &= ~XML_HTTP_REQUEST_ABORTED;
  } else if (mState & (XML_HTTP_REQUEST_OPENED |
                       XML_HTTP_REQUEST_LOADED |
                       XML_HTTP_REQUEST_INTERACTIVE |
                       XML_HTTP_REQUEST_SENT |
                       XML_HTTP_REQUEST_STOPPED)) {
    // IE aborts as well
    Abort();

    // XXX We should probably send a warning to the JS console
    //     that load was aborted and event listeners were cleared
    //     since this looks like a situation that could happen
    //     by accident and you could spend a lot of time wondering
    //     why things didn't work.

    return NS_OK;
  }

  if (async) {
    mState |= XML_HTTP_REQUEST_ASYNC;
  } else {
    mState &= ~XML_HTTP_REQUEST_ASYNC;
  }

  rv = NS_NewURI(getter_AddRefs(uri), url, nsnull, GetBaseURI());
  if (NS_FAILED(rv)) return rv;

  if (!user.IsEmpty()) {
    nsCAutoString userpass;
    CopyUTF16toUTF8(user, userpass);
    if (!password.IsEmpty()) {
      userpass.Append(':');
      AppendUTF16toUTF8(password, userpass);
    }
    uri->SetUserPass(userpass);
    authp = PR_TRUE;
  }

  // When we are called from JS we can find the load group for the page,
  // and add ourselves to it. This way any pending requests
  // will be automatically aborted if the user leaves the page.
  nsCOMPtr<nsILoadGroup> loadGroup;
  GetLoadGroup(getter_AddRefs(loadGroup));

  // nsIRequest::LOAD_BACKGROUND prevents throbber from becoming active, which
  // in turn keeps STOP button from becoming active.  If the consumer passed in
  // a progress event handler we must load with nsIRequest::LOAD_NORMAL or
  // necko won't generate any progress notifications
  nsLoadFlags loadFlags;
  if (mOnProgressListener ||
      mOnUploadProgressListener ||
      mProgressEventListeners.Count() != 0 ||
      mUploadProgressEventListeners.Count() != 0) {
    loadFlags = nsIRequest::LOAD_NORMAL;
  } else {
    loadFlags = nsIRequest::LOAD_BACKGROUND;
  }
  rv = NS_NewChannel(getter_AddRefs(mChannel), uri, nsnull, loadGroup, nsnull,
                     loadFlags);
  if (NS_FAILED(rv)) return rv;

  //mChannel->SetAuthTriedWithPrehost(authp);

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mChannel));
  if (httpChannel) {
    rv = httpChannel->SetRequestMethod(method);
  }

  ChangeState(XML_HTTP_REQUEST_OPENED);

  return rv;
}

/* void open (in AUTF8String method, in AUTF8String url); */
NS_IMETHODIMP
nsXMLHttpRequest::Open(const nsACString& method, const nsACString& url)
{
  nsresult rv = NS_OK;
  PRBool async = PR_TRUE;
  nsAutoString user, password;

  nsCOMPtr<nsIXPCNativeCallContext> cc;
  nsIXPConnect *xpc = nsContentUtils::XPConnect();
  if (xpc) {
    rv = xpc->GetCurrentNativeCallContext(getter_AddRefs(cc));
  }

  if (NS_SUCCEEDED(rv) && cc) {
    PRUint32 argc;
    rv = cc->GetArgc(&argc);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    jsval* argv;
    rv = cc->GetArgvPtr(&argv);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    JSContext* cx;
    rv = cc->GetJSContext(&cx);
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIURI> targetURI;
    rv = NS_NewURI(getter_AddRefs(targetURI), url, nsnull, GetBaseURI());
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

    nsIScriptSecurityManager *secMan = nsContentUtils::GetSecurityManager();
    if (!secMan) {
      return NS_ERROR_FAILURE;
    }

    rv = secMan->CheckConnect(cx, targetURI, "XMLHttpRequest","open");
    if (NS_FAILED(rv))
    {
      // Security check failed.
      return NS_OK;
    }

    // Find out if UniversalBrowserRead privileges are enabled
    // we will need this in case of a redirect
    PRBool crossSiteAccessEnabled;
    rv = secMan->IsCapabilityEnabled("UniversalBrowserRead",
                                     &crossSiteAccessEnabled);
    if (NS_FAILED(rv)) return rv;
    if (crossSiteAccessEnabled) {
      mState |= XML_HTTP_REQUEST_XSITEENABLED;
    } else {
      mState &= ~XML_HTTP_REQUEST_XSITEENABLED;
    }

    if (argc > 2) {
      JSAutoRequest ar(cx);
      JSBool asyncBool;
      ::JS_ValueToBoolean(cx, argv[2], &asyncBool);
      async = (PRBool)asyncBool;

      if (argc > 3 && !JSVAL_IS_NULL(argv[3]) && !JSVAL_IS_VOID(argv[3])) {
        JSString* userStr = ::JS_ValueToString(cx, argv[3]);

        if (userStr) {
          user.Assign(NS_REINTERPRET_CAST(PRUnichar *,
                                          ::JS_GetStringChars(userStr)),
                      ::JS_GetStringLength(userStr));
        }

        if (argc > 4 && !JSVAL_IS_NULL(argv[4]) && !JSVAL_IS_VOID(argv[4])) {
          JSString* passwdStr = JS_ValueToString(cx, argv[4]);

          if (passwdStr) {
            password.Assign(NS_REINTERPRET_CAST(PRUnichar *,
                                                ::JS_GetStringChars(passwdStr)),
                            ::JS_GetStringLength(passwdStr));
          }
        }
      }
    }
  }

  return OpenRequest(method, url, async, user, password);
}

/*
 * "Copy" from a stream.
 */
NS_METHOD
nsXMLHttpRequest::StreamReaderFunc(nsIInputStream* in,
                                   void* closure,
                                   const char* fromRawSegment,
                                   PRUint32 toOffset,
                                   PRUint32 count,
                                   PRUint32 *writeCount)
{
  nsXMLHttpRequest* xmlHttpRequest = NS_STATIC_CAST(nsXMLHttpRequest*, closure);
  if (!xmlHttpRequest || !writeCount) {
    NS_WARNING("XMLHttpRequest cannot read from stream: no closure or writeCount");
    return NS_ERROR_FAILURE;
  }

  // Copy for our own use
  xmlHttpRequest->mResponseBody.Append(fromRawSegment,count);

  nsresult rv = NS_OK;

  if (xmlHttpRequest->mState & XML_HTTP_REQUEST_PARSEBODY) {
    // Give the same data to the parser.

    // We need to wrap the data in a new lightweight stream and pass that
    // to the parser, because calling ReadSegments() recursively on the same
    // stream is not supported.
    nsCOMPtr<nsIInputStream> copyStream;
    rv = NS_NewByteInputStream(getter_AddRefs(copyStream), fromRawSegment, count);

    if (NS_SUCCEEDED(rv)) {
      NS_ASSERTION(copyStream, "NS_NewByteInputStream lied");
      nsresult parsingResult = xmlHttpRequest->mXMLParserStreamListener
                                  ->OnDataAvailable(xmlHttpRequest->mReadRequest,
                                                    xmlHttpRequest->mContext,
                                                    copyStream, toOffset, count);

      // No use to continue parsing if we failed here, but we
      // should still finish reading the stream
      if (NS_FAILED(parsingResult)) {
        xmlHttpRequest->mState &= ~XML_HTTP_REQUEST_PARSEBODY;
      }
    }
  }

  xmlHttpRequest->ChangeState(XML_HTTP_REQUEST_INTERACTIVE);

  if (NS_SUCCEEDED(rv)) {
    *writeCount = count;
  } else {
    *writeCount = 0;
  }

  return rv;
}

/* void onDataAvailable (in nsIRequest request, in nsISupports ctxt, in nsIInputStream inStr, in unsigned long sourceOffset, in unsigned long count); */
NS_IMETHODIMP
nsXMLHttpRequest::OnDataAvailable(nsIRequest *request, nsISupports *ctxt, nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count)
{
  NS_ENSURE_ARG_POINTER(inStr);

  NS_ABORT_IF_FALSE(mContext.get() == ctxt,"start context different from OnDataAvailable context");

  PRUint32 totalRead;
  return inStr->ReadSegments(nsXMLHttpRequest::StreamReaderFunc, (void*)this, count, &totalRead);
}

/* void onStartRequest (in nsIRequest request, in nsISupports ctxt); */
NS_IMETHODIMP
nsXMLHttpRequest::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
  // Don't do anything if we have been aborted
  if (mState & XML_HTTP_REQUEST_UNINITIALIZED)
    return NS_OK;

  if (mState & XML_HTTP_REQUEST_ABORTED) {
    NS_ERROR("Ugh, still getting data on an aborted XMLHttpRequest!");

    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIChannel> channel(do_QueryInterface(request));
  NS_ENSURE_TRUE(channel, NS_ERROR_UNEXPECTED);

  mReadRequest = request;
  mContext = ctxt;
  mState |= XML_HTTP_REQUEST_PARSEBODY;
  ChangeState(XML_HTTP_REQUEST_LOADED);

  // XXXbz this is probably all wrong when not called from JS... and possibly
  // even then! Fixing that requires giving XMLHttpRequest some principals
  // when inited.  Until then, cases when we don't actually parse the
  // document will give our mDocument he wrong principal.  I'm just not sure
  // how wrong it can get...  Shouldn't be too bad as long as mScriptContext
  // is sane, I guess.
  nsCOMPtr<nsIDocument> doc = GetDocumentFromScriptContext(mScriptContext);
  nsIURI* uri = GetBaseURI();
  nsIPrincipal* principal = nsnull;
  if (doc) {
    principal = doc->NodePrincipal();
  }

  // Create an empty document from it 
  const nsAString& emptyStr = EmptyString();
  nsresult rv = nsContentUtils::CreateDocument(emptyStr, emptyStr, nsnull, uri,
                                               uri, principal,
                                               getter_AddRefs(mDocument));
  NS_ENSURE_SUCCESS(rv, rv);

  // Reset responseBody
  mResponseBody.Truncate();

  // Register as a load listener on the document
  nsCOMPtr<nsIDOMEventReceiver> target(do_QueryInterface(mDocument));
  if (target) {
    nsWeakPtr requestWeak =
      do_GetWeakReference(NS_STATIC_CAST(nsIXMLHttpRequest*, this));
    nsCOMPtr<nsIDOMEventListener> proxy = new nsLoadListenerProxy(requestWeak);
    if (!proxy) return NS_ERROR_OUT_OF_MEMORY;

    // This will addref the proxy
    rv = target->AddEventListenerByIID(NS_STATIC_CAST(nsIDOMEventListener*,
                                                      proxy),
                                       NS_GET_IID(nsIDOMLoadListener));
    if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
  }

  nsresult status;
  request->GetStatus(&status);

  PRBool parseBody = PR_TRUE;
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mChannel));
  if (httpChannel) {
    nsCAutoString method;
    httpChannel->GetRequestMethod(method);
    parseBody = !method.EqualsLiteral("HEAD");
  }

  if (parseBody && NS_SUCCEEDED(status)) {
    if (!mOverrideMimeType.IsEmpty()) {
      channel->SetContentType(mOverrideMimeType);
    }

    // We can gain a huge performance win by not even trying to
    // parse non-XML data. This also protects us from the situation
    // where we have an XML document and sink, but HTML (or other)
    // parser, which can produce unreliable results.
    nsCAutoString type;
    channel->GetContentType(type);

    if (type.Find("xml") == kNotFound) {
      mState &= ~XML_HTTP_REQUEST_PARSEBODY;
    }
  } else {
    // The request failed, so we shouldn't be parsing anyway
    mState &= ~XML_HTTP_REQUEST_PARSEBODY;
  }

  if (mState & XML_HTTP_REQUEST_PARSEBODY) {
    nsCOMPtr<nsIStreamListener> listener;
    nsCOMPtr<nsILoadGroup> loadGroup;
    channel->GetLoadGroup(getter_AddRefs(loadGroup));

    nsCOMPtr<nsIDocument> document(do_QueryInterface(mDocument));
    if (!document) {
      return NS_ERROR_FAILURE;
    }

    rv = document->StartDocumentLoad(kLoadAsData, channel, loadGroup, nsnull,
                                     getter_AddRefs(listener), PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    mXMLParserStreamListener = listener;
    return mXMLParserStreamListener->OnStartRequest(request, ctxt);
  }

  return NS_OK;
}

/* void onStopRequest (in nsIRequest request, in nsISupports ctxt, in nsresult status, in wstring statusArg); */
NS_IMETHODIMP
nsXMLHttpRequest::OnStopRequest(nsIRequest *request, nsISupports *ctxt, nsresult status)
{
  // Don't do anything if we have been aborted
  if (mState & XML_HTTP_REQUEST_UNINITIALIZED)
    return NS_OK;

  nsresult rv = NS_OK;

  nsCOMPtr<nsIParser> parser;

  // If we're loading a multipart stream of XML documents, we'll get
  // an OnStopRequest() for the last part in the stream, and then
  // another one for the end of the initiating
  // "multipart/x-mixed-replace" stream too. So we must check that we
  // still have an xml parser stream listener before accessing it
  // here.

  // Is this good enough here?
  if (mState & XML_HTTP_REQUEST_PARSEBODY && mXMLParserStreamListener) {
    parser = do_QueryInterface(mXMLParserStreamListener);
    NS_ABORT_IF_FALSE(parser, "stream listener was expected to be a parser");
    rv = mXMLParserStreamListener->OnStopRequest(request, ctxt, status);
  }

  mXMLParserStreamListener = nsnull;
  mReadRequest = nsnull;
  mContext = nsnull;

  nsCOMPtr<nsIChannel> channel(do_QueryInterface(request));
  NS_ENSURE_TRUE(channel, NS_ERROR_UNEXPECTED);

  channel->SetNotificationCallbacks(nsnull);
  mNotificationCallbacks = nsnull;
  mChannelEventSink = nsnull;
  mProgressEventSink = nsnull;

  if (NS_FAILED(status)) {
    // This can happen if the server is unreachable. Other possible
    // reasons are that the user leaves the page or hits the ESC key.
    Error(nsnull);

    // By nulling out channel here we make it so that Send() can test
    // for that and throw. Also calling the various status
    // methods/members will not throw.
    // This matches what IE does.
    mChannel = nsnull;
  } else if (!parser || parser->IsParserEnabled()) {
    // If we don't have a parser, we never attempted to parse the
    // incoming data, and we can proceed to call RequestCompleted().
    // Alternatively, if we do have a parser, its possible that we
    // have given it some data and this caused it to block e.g. by a
    // by a xml-stylesheet PI. In this case, we will have to wait till
    // it gets enabled again and RequestCompleted() must be called
    // later, when we get the load event from the document. If the
    // parser is enabled, it is not blocked and we can still go ahead
    // and call RequestCompleted() and expect everything to get
    // cleaned up immediately.
    RequestCompleted();
  } else {
    ChangeState(XML_HTTP_REQUEST_STOPPED, PR_FALSE);
  }

  if (mScriptContext) {
    // Force a GC since we could be loading a lot of documents
    // (especially if streaming), and not doing anything that would
    // normally trigger a GC.
    mScriptContext->GC();
  }

  mState &= ~XML_HTTP_REQUEST_SYNCLOOPING;

  return rv;
}

nsresult
nsXMLHttpRequest::RequestCompleted()
{
  nsresult rv = NS_OK;

  mState &= ~XML_HTTP_REQUEST_SYNCLOOPING;

  // If we're uninitialized at this point, we encountered an error
  // earlier and listeners have already been notified. Also we do
  // not want to do this if we already completed.
  if (mState & (XML_HTTP_REQUEST_UNINITIALIZED |
                XML_HTTP_REQUEST_COMPLETED)) {
    return NS_OK;
  }

  // Grab hold of the event listeners we will need before we possibly clear
  // them.
  nsCOMArray<nsIDOMEventListener> loadEventListeners;
  CopyEventListeners(mOnLoadListener, mLoadEventListeners, loadEventListeners);

  // We need to create the event before nulling out mDocument
  nsCOMPtr<nsIDOMEvent> domevent;
  if (loadEventListeners.Count()) {
    rv = CreateEvent(NS_LITERAL_STRING(LOAD_STR), getter_AddRefs(domevent));
  }

  // We might have been sent non-XML data. If that was the case,
  // we should null out the document member. The idea in this
  // check here is that if there is no document element it is not
  // an XML document. We might need a fancier check...
  if (mDocument) {
    nsCOMPtr<nsIDOMElement> root;
    mDocument->GetDocumentElement(getter_AddRefs(root));
    if (!root) {
      mDocument = nsnull;
    }
  }

  // Clear listeners here unless we're multipart
  ChangeState(XML_HTTP_REQUEST_COMPLETED, PR_TRUE,
              !(mState & XML_HTTP_REQUEST_MULTIPART));

  if (NS_SUCCEEDED(rv) && domevent) {
    NotifyEventListeners(loadEventListeners, domevent);
  }

  if (mState & XML_HTTP_REQUEST_MULTIPART) {
    // We're a multipart request, so we're not done. Reset to opened.
    ChangeState(XML_HTTP_REQUEST_OPENED);
  }

  return rv;
}

/* void send (in nsIVariant aBody); */
NS_IMETHODIMP
nsXMLHttpRequest::Send(nsIVariant *aBody)
{
  nsresult rv;

  // Return error if we're already processing a request
  if (XML_HTTP_REQUEST_SENT & mState) {
    return NS_ERROR_FAILURE;
  }

  // Make sure we've been opened
  if (!mChannel || !(XML_HTTP_REQUEST_OPENED & mState)) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  // XXX We should probably send a warning to the JS console
  //     if there are no event listeners set and we are doing
  //     an asynchronous call.

  // Ignore argument if method is GET, there is no point in trying to
  // upload anything
  nsCAutoString method;
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mChannel));

  if (httpChannel) {
    httpChannel->GetRequestMethod(method); // If GET, method name will be uppercase

    nsCOMPtr<nsIDocument> doc =
      do_QueryInterface(nsContentUtils::GetDocumentFromCaller());

    if (doc) {
      nsIPrincipal *principal = doc->NodePrincipal();

      if (principal) {
        nsCOMPtr<nsIURI> codebase;
        principal->GetURI(getter_AddRefs(codebase));

        httpChannel->SetReferrer(codebase);
      }
    }
  }

  if (aBody && httpChannel && !method.EqualsLiteral("GET")) {
    nsXPIDLString serial;
    nsCOMPtr<nsIInputStream> postDataStream;

    PRUint16 dataType;
    rv = aBody->GetDataType(&dataType);
    if (NS_FAILED(rv))
      return rv;

    switch (dataType) {
    case nsIDataType::VTYPE_INTERFACE:
    case nsIDataType::VTYPE_INTERFACE_IS:
      {
        nsCOMPtr<nsISupports> supports;
        nsID *iid;
        rv = aBody->GetAsInterface(&iid, getter_AddRefs(supports));
        if (NS_FAILED(rv))
          return rv;
        if (iid)
          nsMemory::Free(iid);

        // document?
        nsCOMPtr<nsIDOMDocument> doc(do_QueryInterface(supports));
        if (doc) {
          nsCOMPtr<nsIDOMSerializer> serializer(do_CreateInstance(NS_XMLSERIALIZER_CONTRACTID, &rv));
          if (NS_FAILED(rv)) return rv;

          // Serialize to a stream so that the encoding used will
          // match the document's.
          nsCOMPtr<nsIInputStream> input;
          nsCOMPtr<nsIOutputStream> output;
          rv = NS_NewPipe(getter_AddRefs(input), getter_AddRefs(output),
                          0, PR_UINT32_MAX);
          NS_ENSURE_SUCCESS(rv, rv);

          // Empty string for encoding means to use document's current
          // encoding.
          rv = serializer->SerializeToStream(doc, output, EmptyCString());
          NS_ENSURE_SUCCESS(rv, rv);

          output->Close();
          postDataStream = input;
        } else {
          // nsISupportsString?
          nsCOMPtr<nsISupportsString> wstr(do_QueryInterface(supports));
          if (wstr) {
            wstr->GetData(serial);
          } else {
            // stream?
            nsCOMPtr<nsIInputStream> stream(do_QueryInterface(supports));
            if (stream) {
              postDataStream = stream;
            }
          }
        }
      }
      break;
    case nsIDataType::VTYPE_VOID:
    case nsIDataType::VTYPE_EMPTY:
      // Makes us act as if !aBody, don't upload anything
      break;
    case nsIDataType::VTYPE_EMPTY_ARRAY:
    case nsIDataType::VTYPE_ARRAY:
      // IE6 throws error here, so we do that as well
      return NS_ERROR_INVALID_ARG;
    default:
      // try variant string
      rv = aBody->GetAsWString(getter_Copies(serial));
      if (NS_FAILED(rv))
        return rv;
      break;
    }

    if (serial) {
      // Convert to a byte stream
      nsCOMPtr<nsIScriptableUnicodeConverter> converter =
        do_CreateInstance("@mozilla.org/intl/scriptableunicodeconverter", &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = converter->SetCharset("UTF-8");
      NS_ENSURE_SUCCESS(rv, rv);

      rv = converter->ConvertToInputStream(serial,
                                           getter_AddRefs(postDataStream));
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (postDataStream) {
      nsCOMPtr<nsIUploadChannel> uploadChannel(do_QueryInterface(httpChannel));
      NS_ASSERTION(uploadChannel, "http must support nsIUploadChannel");

      // If no content type header was set by the client, we set it to
      // application/xml.
      nsCAutoString contentType;
      if (NS_FAILED(httpChannel->
                      GetRequestHeader(NS_LITERAL_CSTRING("Content-Type"),
                                       contentType)) ||
          contentType.IsEmpty()) {
        contentType = NS_LITERAL_CSTRING("application/xml");
      }
      
      rv = uploadChannel->SetUploadStream(postDataStream, contentType, -1);
      // Reset the method to its original value
      if (httpChannel) {
        httpChannel->SetRequestMethod(method);
      }
    }
  }

  // Reset responseBody
  mResponseBody.Truncate();

  // Reset responseXML
  mDocument = nsnull;

  if (!(mState & XML_HTTP_REQUEST_ASYNC)) {
    mState |= XML_HTTP_REQUEST_SYNCLOOPING;
  }

  if (!mScriptContext) {
    // We need a context to check if redirect (if any) is allowed
    mScriptContext = GetCurrentContext();
  }

  // Hook us up to listen to redirects and the like
  mChannel->GetNotificationCallbacks(getter_AddRefs(mNotificationCallbacks));
  mChannel->SetNotificationCallbacks(this);

  nsCOMPtr<nsIStreamListener> listener;
  if (mState & XML_HTTP_REQUEST_MULTIPART) {
    listener = new nsMultipartProxyListener(this);
    if (!listener) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  } else {
    listener = this;
  }

  // Bypass the network cache in cases where it makes no sense:
  // 1) Multipart responses are very large and would likely be doomed by the
  //    cache once they grow too large, so they are not worth caching.
  // 2) POST responses are always unique, and we provide no API that would
  //    allow our consumers to specify a "cache key" to access old POST
  //    responses, so they are not worth caching.
  if ((mState & XML_HTTP_REQUEST_MULTIPART) || method.EqualsLiteral("POST")) {
    AddLoadFlags(mChannel,
        nsIRequest::LOAD_BYPASS_CACHE | nsIRequest::INHIBIT_CACHING);
  }
  // When we are sync loading, we need to bypass the local cache when it would
  // otherwise block us waiting for exclusive access to the cache.  If we don't
  // do this, then we could dead lock in some cases (see bug 309424).
  else if (mState & XML_HTTP_REQUEST_SYNCLOOPING) {
    AddLoadFlags(mChannel,
        nsICachingChannel::LOAD_BYPASS_LOCAL_CACHE_IF_BUSY);
  }

  // Since we expect XML data, set the type hint accordingly
  // This means that we always try to parse local files as XML
  // ignoring return value, as this is not critical
  mChannel->SetContentType(NS_LITERAL_CSTRING("application/xml"));

  // Start reading from the channel
  rv = mChannel->AsyncOpen(listener, nsnull);

  if (NS_FAILED(rv)) {
    // Drop our ref to the channel to avoid cycles
    mChannel = nsnull;
    return rv;
  }

  // Now that we've successfully opened the channel, we can change state.  Note
  // that this needs to come after the AsyncOpen() and rv check, because this
  // can run script that would try to restart this request, and that could end
  // up doing our AsyncOpen on a null channel if the reentered AsyncOpen fails.
  ChangeState(XML_HTTP_REQUEST_SENT);

  // If we're synchronous, spin an event loop here and wait
  if (!(mState & XML_HTTP_REQUEST_ASYNC)) {
    nsIThread *thread = NS_GetCurrentThread();
    while (mState & XML_HTTP_REQUEST_SYNCLOOPING) {
      if (!NS_ProcessNextEvent(thread)) {
        rv = NS_ERROR_UNEXPECTED;
        break;
      }
    }
  }

  if (!mChannel) {
    return NS_ERROR_FAILURE;
  }

  return rv;
}

/* void setRequestHeader (in AUTF8String header, in AUTF8String value); */
NS_IMETHODIMP
nsXMLHttpRequest::SetRequestHeader(const nsACString& header,
                                   const nsACString& value)
{
  if (!mChannel)             // open() initializes mChannel, and open()
    return NS_ERROR_FAILURE; // must be called before first setRequestHeader()

  // Prevent modification to certain HTTP headers (see bug 302263), unless
  // the executing script has UniversalBrowserWrite permission.

  nsIScriptSecurityManager *secMan = nsContentUtils::GetSecurityManager();
  if (!secMan) {
    return NS_ERROR_FAILURE;
  }

  PRBool privileged;
  nsresult rv = secMan->IsCapabilityEnabled("UniversalBrowserWrite",
                                            &privileged);
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  if (!privileged) {
    const char *kInvalidHeaders[] = {
      "host", "content-length", "transfer-encoding", "via", "upgrade"
    };
    for (size_t i = 0; i < NS_ARRAY_LENGTH(kInvalidHeaders); ++i) {
      if (header.LowerCaseEqualsASCII(kInvalidHeaders[i])) {
        NS_WARNING("refusing to set request header");
        return NS_OK;
      }
    }
  }

  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mChannel));

  if (httpChannel) {
    // We need to set, not add to, the header.
    return httpChannel->SetRequestHeader(header, value, PR_FALSE);
  }

  return NS_OK;
}

/* readonly attribute long readyState; */
NS_IMETHODIMP
nsXMLHttpRequest::GetReadyState(PRInt32 *aState)
{
  NS_ENSURE_ARG_POINTER(aState);
  // Translate some of our internal states for external consumers
  if (mState & XML_HTTP_REQUEST_UNINITIALIZED) {
    *aState = 0; // UNINITIALIZED
  } else  if (mState & (XML_HTTP_REQUEST_OPENED | XML_HTTP_REQUEST_SENT)) {
    *aState = 1; // LOADING
  } else if (mState & XML_HTTP_REQUEST_LOADED) {
    *aState = 2; // LOADED
  } else if (mState & (XML_HTTP_REQUEST_INTERACTIVE | XML_HTTP_REQUEST_STOPPED)) {
    *aState = 3; // INTERACTIVE
  } else if (mState & XML_HTTP_REQUEST_COMPLETED) {
    *aState = 4; // COMPLETED
  } else {
    NS_ERROR("Should not happen");
  }

  return NS_OK;
}

/* void   overrideMimeType(in AUTF8String mimetype); */
NS_IMETHODIMP
nsXMLHttpRequest::OverrideMimeType(const nsACString& aMimeType)
{
  // XXX Should we do some validation here?
  mOverrideMimeType.Assign(aMimeType);
  return NS_OK;
}


/* attribute boolean multipart; */
NS_IMETHODIMP
nsXMLHttpRequest::GetMultipart(PRBool *_retval)
{
  *_retval = mState & XML_HTTP_REQUEST_MULTIPART;

  return NS_OK;
}

/* attribute boolean multipart; */
NS_IMETHODIMP
nsXMLHttpRequest::SetMultipart(PRBool aMultipart)
{
  if (!(mState & XML_HTTP_REQUEST_UNINITIALIZED)) {
    // Can't change this while we're in the middle of something.
    return NS_ERROR_IN_PROGRESS;
  }

  if (aMultipart) {
    mState |= XML_HTTP_REQUEST_MULTIPART;
  } else {
    mState &= ~XML_HTTP_REQUEST_MULTIPART;
  }

  return NS_OK;
}


// nsIDOMEventListener
nsresult
nsXMLHttpRequest::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}


// nsIDOMLoadListener
nsresult
nsXMLHttpRequest::Load(nsIDOMEvent* aEvent)
{
  // If we had an XML error in the data, the parser terminated and
  // we received the load event, even though we might still be
  // loading data into responseBody/responseText. We will delay
  // sending the load event until OnStopRequest(). In normal case
  // there is no harm done, we will get OnStopRequest() immediately
  // after the load event.
  //
  // However, if the data we were loading caused the parser to stop,
  // for example when loading external stylesheets, we can receive
  // the OnStopRequest() call before the parser has finished building
  // the document. In that case, we obviously should not fire the event
  // in OnStopRequest(). For those documents, we must wait for the load
  // event from the document to fire our RequestCompleted().
  if (mState & XML_HTTP_REQUEST_STOPPED) {
    RequestCompleted();
  }
  return NS_OK;
}

nsresult
nsXMLHttpRequest::Unload(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

nsresult
nsXMLHttpRequest::BeforeUnload(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

nsresult
nsXMLHttpRequest::Abort(nsIDOMEvent* aEvent)
{
  Abort();

  mState &= ~XML_HTTP_REQUEST_SYNCLOOPING;

  return NS_OK;
}

nsresult
nsXMLHttpRequest::Error(nsIDOMEvent* aEvent)
{
  nsCOMArray<nsIDOMEventListener> errorEventListeners;
  CopyEventListeners(mOnErrorListener, mErrorEventListeners,
                     errorEventListeners);

  // We need to create the event before nulling out mDocument
  nsCOMPtr<nsIDOMEvent> event = aEvent;
  if (!event && errorEventListeners.Count()) {
    CreateEvent(NS_LITERAL_STRING(ERROR_STR), getter_AddRefs(event));
  }

  mDocument = nsnull;
  ChangeState(XML_HTTP_REQUEST_COMPLETED);

  mState &= ~XML_HTTP_REQUEST_SYNCLOOPING;

  ClearEventListeners();
  
  if (event) {
    NotifyEventListeners(errorEventListeners, event);
  }

  return NS_OK;
}

nsresult
nsXMLHttpRequest::ChangeState(PRUint32 aState, PRBool aBroadcast,
                              PRBool aClearEventListeners)
{
  // If we are setting one of the mutually exclusive states,
  // unset those state bits first.
  if (aState & XML_HTTP_REQUEST_LOADSTATES) {
    mState &= ~XML_HTTP_REQUEST_LOADSTATES;
  }
  mState |= aState;
  nsresult rv = NS_OK;

  // Grab private copies of the listeners we need
  nsCOMArray<nsIDOMEventListener> readystatechangeEventListeners;
  CopyEventListeners(mOnReadystatechangeListener,
                     mReadystatechangeEventListeners,
                     readystatechangeEventListeners);

  if (aClearEventListeners) {
    ClearEventListeners();
  }

  if ((mState & XML_HTTP_REQUEST_ASYNC) &&
      (aState & XML_HTTP_REQUEST_LOADSTATES) && // Broadcast load states only
      aBroadcast &&
      readystatechangeEventListeners.Count()) {
    nsCOMPtr<nsIDOMEvent> event;
    rv = CreateEvent(NS_LITERAL_STRING(READYSTATE_STR),
                     getter_AddRefs(event));
    NS_ENSURE_SUCCESS(rv, rv);

    NotifyEventListeners(readystatechangeEventListeners, event);
  }

  return rv;
}

/////////////////////////////////////////////////////
// nsIChannelEventSink methods:
//
NS_IMETHODIMP
nsXMLHttpRequest::OnChannelRedirect(nsIChannel *aOldChannel,
                                    nsIChannel *aNewChannel,
                                    PRUint32    aFlags)
{
  NS_PRECONDITION(aNewChannel, "Redirect without a channel?");

  if (mScriptContext && !(mState & XML_HTTP_REQUEST_XSITEENABLED)) {
    nsresult rv = NS_ERROR_FAILURE;

    nsCOMPtr<nsIJSContextStack> stack(do_GetService("@mozilla.org/js/xpc/ContextStack;1", & rv));
    if (NS_FAILED(rv))
      return rv;

    JSContext *cx = (JSContext *)mScriptContext->GetNativeContext();
    if (!cx)
      return NS_ERROR_UNEXPECTED;

    nsIScriptSecurityManager *secMan = nsContentUtils::GetSecurityManager();
    if (!secMan) {
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIURI> newURI;
    rv = aNewChannel->GetURI(getter_AddRefs(newURI)); // The redirected URI
    if (NS_FAILED(rv))
      return rv;

    stack->Push(cx);

    rv = secMan->CheckSameOrigin(cx, newURI);

    stack->Pop(&cx);

    if (NS_FAILED(rv)) {
      // The security manager set a pending exception.  Since we're
      // running under the event loop, we need to report it.
      ::JS_ReportPendingException(cx);
      return rv;
    }
  }

  if (mChannelEventSink) {
    nsresult rv =
      mChannelEventSink->OnChannelRedirect(aOldChannel, aNewChannel, aFlags);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  mChannel = aNewChannel;

  return NS_OK;
}

/////////////////////////////////////////////////////
// nsIProgressEventSink methods:
//

NS_IMETHODIMP
nsXMLHttpRequest::OnProgress(nsIRequest *aRequest, nsISupports *aContext, PRUint64 aProgress, PRUint64 aProgressMax)
{
  // We're uploading if our state is XML_HTTP_REQUEST_OPENED or
  // XML_HTTP_REQUEST_SENT
  PRBool downloading =
    !((XML_HTTP_REQUEST_OPENED | XML_HTTP_REQUEST_SENT) & mState);
  nsCOMArray<nsIDOMEventListener> progressListeners;
  if (downloading) {
    CopyEventListeners(mOnProgressListener,
                       mProgressEventListeners, progressListeners);
  } else {
    CopyEventListeners(mOnUploadProgressListener,
                       mUploadProgressEventListeners, progressListeners);
  }
  
  if (progressListeners.Count()) {
    nsCOMPtr<nsIDOMEvent> event;
    nsresult rv = CreateEvent(NS_LITERAL_STRING(PROGRESS_STR),
                              getter_AddRefs(event));
    NS_ENSURE_SUCCESS(rv, rv);
    
    nsXMLHttpProgressEvent * progressEvent =
      new nsXMLHttpProgressEvent(event, aProgress, aProgressMax); 
    if (!progressEvent)
      return NS_ERROR_OUT_OF_MEMORY;

    event = progressEvent;
    NotifyEventListeners(progressListeners, event);
  }

  if (mProgressEventSink) {
    mProgressEventSink->OnProgress(aRequest, aContext, aProgress,
                                   aProgressMax);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXMLHttpRequest::OnStatus(nsIRequest *aRequest, nsISupports *aContext, nsresult aStatus, const PRUnichar *aStatusArg)
{
  if (mProgressEventSink) {
    mProgressEventSink->OnStatus(aRequest, aContext, aStatus, aStatusArg);
  }

  return NS_OK;
}

/////////////////////////////////////////////////////
// nsIInterfaceRequestor methods:
//
NS_IMETHODIMP
nsXMLHttpRequest::GetInterface(const nsIID & aIID, void **aResult)
{
  // Make sure to return ourselves for the channel event sink interface and
  // progress event sink interface, no matter what.  We can forward these to
  // mNotificationCallbacks if it wants to get notifications for them.  But we
  // need to see these notifications for proper functioning.
  if (aIID.Equals(NS_GET_IID(nsIChannelEventSink))) {
    mChannelEventSink = do_GetInterface(mNotificationCallbacks);
    *aResult = NS_STATIC_CAST(nsIChannelEventSink*, this);
    NS_ADDREF_THIS();
    return NS_OK;
  } else if (aIID.Equals(NS_GET_IID(nsIProgressEventSink))) {
    mProgressEventSink = do_GetInterface(mNotificationCallbacks);
    *aResult = NS_STATIC_CAST(nsIProgressEventSink*, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  // Now give mNotificationCallbacks (if non-null) a chance to return the
  // desired interface.  Note that this means that it can override our
  // nsIAuthPrompt impl, but that's fine, if it has a better auth prompt idea.
  if (mNotificationCallbacks) {
    nsresult rv = mNotificationCallbacks->GetInterface(aIID, aResult);
    if (NS_SUCCEEDED(rv)) {
      NS_ASSERTION(*aResult, "Lying nsIInterfaceRequestor implementation!");
      return rv;
    }
  }
  
  if (aIID.Equals(NS_GET_IID(nsIAuthPrompt))) {    
    *aResult = nsnull;

    nsresult rv;
    nsCOMPtr<nsIWindowWatcher> ww(do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv));
    if (NS_FAILED(rv))
      return rv;

    nsCOMPtr<nsIAuthPrompt> prompt;
    rv = ww->GetNewAuthPrompter(nsnull, getter_AddRefs(prompt));
    if (NS_FAILED(rv))
      return rv;

    nsIAuthPrompt *p = prompt.get();
    NS_ADDREF(p);
    *aResult = p;
    return NS_OK;
  }

  return QueryInterface(aIID, aResult);
}


NS_IMPL_ISUPPORTS1(nsXMLHttpRequest::nsHeaderVisitor, nsIHttpHeaderVisitor)

NS_IMETHODIMP nsXMLHttpRequest::
nsHeaderVisitor::VisitHeader(const nsACString &header, const nsACString &value)
{
    mHeaders.Append(header);
    mHeaders.Append(": ");
    mHeaders.Append(value);
    mHeaders.Append('\n');
    return NS_OK;
}

// DOM event class to handle progress notifications
nsXMLHttpProgressEvent::nsXMLHttpProgressEvent(nsIDOMEvent * aInner, PRUint64 aCurrentProgress, PRUint64 aMaxProgress)
{
  mInner = aInner; 
  mCurProgress = aCurrentProgress;
  mMaxProgress = aMaxProgress;
}

nsXMLHttpProgressEvent::~nsXMLHttpProgressEvent()
{}

// QueryInterface implementation for nsXMLHttpRequest
NS_INTERFACE_MAP_BEGIN(nsXMLHttpProgressEvent)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMLSProgressEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMLSProgressEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEvent)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(XMLHttpProgressEvent)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsXMLHttpProgressEvent)
NS_IMPL_RELEASE(nsXMLHttpProgressEvent)

NS_IMETHODIMP nsXMLHttpProgressEvent::GetInput(nsIDOMLSInput * *aInput)
{
  *aInput = nsnull;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXMLHttpProgressEvent::GetPosition(PRUint32 *aPosition)
{
  // XXX can we change the iface?
  LL_L2UI(*aPosition, mCurProgress);
  return NS_OK;
}

NS_IMETHODIMP nsXMLHttpProgressEvent::GetTotalSize(PRUint32 *aTotalSize)
{
  // XXX can we change the iface?
  LL_L2UI(*aTotalSize, mMaxProgress);
  return NS_OK;
}

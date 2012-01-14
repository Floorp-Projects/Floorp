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
#include "nsString.h"
#include "nsIDOMDocument.h"
#include "nsIURI.h"
#include "nsIHttpChannel.h"
#include "nsIDocument.h"
#include "nsIStreamListener.h"
#include "nsWeakReference.h"
#include "jsapi.h"
#include "nsIScriptContext.h"
#include "nsIChannelEventSink.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIInterfaceRequestor.h"
#include "nsIHttpHeaderVisitor.h"
#include "nsIProgressEventSink.h"
#include "nsCOMArray.h"
#include "nsJSUtils.h"
#include "nsTArray.h"
#include "nsIJSNativeInitializer.h"
#include "nsIDOMLSProgressEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsITimer.h"
#include "nsIPrivateDOMEvent.h"
#include "nsDOMProgressEvent.h"
#include "nsDOMEventTargetHelper.h"
#include "nsContentUtils.h"
#include "nsDOMFile.h"
#include "nsDOMBlobBuilder.h"
#include "nsIPrincipal.h"
#include "nsIScriptObjectPrincipal.h"
#include "mozilla/dom/bindings/XMLHttpRequestBinding.h"
#include "mozilla/dom/bindings/XMLHttpRequestUploadBinding.h"

#include "mozilla/Assertions.h"

class nsILoadGroup;
class AsyncVerifyRedirectCallbackForwarder;
class nsIUnicodeDecoder;
class nsIDOMFormData;

#define IMPL_EVENT_HANDLER(_lowercase, _capitalized)                    \
  JSObject* GetOn##_lowercase()                                         \
  {                                                                     \
    return GetListenerAsJSObject(mOn##_capitalized##Listener);          \
  }                                                                     \
  void SetOn##_lowercase(JSContext* aCx, JSObject* aCallback, nsresult& aRv) \
  {                                                                     \
    aRv = SetJSObjectListener(aCx, NS_LITERAL_STRING(#_lowercase),      \
                              mOn##_capitalized##Listener,              \
                              aCallback);                               \
  }

class nsXHREventTarget : public nsDOMEventTargetHelper,
                         public nsIXMLHttpRequestEventTarget
{
public:
  typedef mozilla::dom::bindings::prototypes::XMLHttpRequestResponseType::value
          XMLHttpRequestResponseType;

  virtual ~nsXHREventTarget() {}
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsXHREventTarget,
                                           nsDOMEventTargetHelper)
  NS_DECL_NSIXMLHTTPREQUESTEVENTTARGET
  NS_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper::)

  IMPL_EVENT_HANDLER(loadstart, LoadStart)
  IMPL_EVENT_HANDLER(progress, Progress)
  IMPL_EVENT_HANDLER(abort, Abort)
  IMPL_EVENT_HANDLER(error, Error)
  IMPL_EVENT_HANDLER(load, Load)
  IMPL_EVENT_HANDLER(timeout, Timeout)
  IMPL_EVENT_HANDLER(loadend, Loadend)
  
  virtual void DisconnectFromOwner();
protected:
  static inline JSObject* GetListenerAsJSObject(nsDOMEventListenerWrapper* aWrapper)
  {
    if (!aWrapper) {
      return nsnull;
    }

    nsCOMPtr<nsIXPConnectJSObjectHolder> holder =
        do_QueryInterface(aWrapper->GetInner());
    JSObject* obj;
    return holder && NS_SUCCEEDED(holder->GetJSObject(&obj)) ? obj : nsnull;
  }
  inline nsresult SetJSObjectListener(JSContext* aCx,
                                      const nsAString& aType,
                                      nsRefPtr<nsDOMEventListenerWrapper>& aWrapper,
                                      JSObject* aCallback)
  {
    nsCOMPtr<nsIDOMEventListener> listener;
    if (aCallback) {
      nsresult rv =
        nsContentUtils::XPConnect()->WrapJS(aCx,
                                            aCallback,
                                            NS_GET_IID(nsIDOMEventListener),
                                            getter_AddRefs(listener));
      NS_ENSURE_SUCCESS(rv, rv);
    }

    return RemoveAddEventListener(aType, aWrapper, listener);
  }

  nsRefPtr<nsDOMEventListenerWrapper> mOnLoadListener;
  nsRefPtr<nsDOMEventListenerWrapper> mOnErrorListener;
  nsRefPtr<nsDOMEventListenerWrapper> mOnAbortListener;
  nsRefPtr<nsDOMEventListenerWrapper> mOnLoadStartListener;
  nsRefPtr<nsDOMEventListenerWrapper> mOnProgressListener;
  nsRefPtr<nsDOMEventListenerWrapper> mOnLoadendListener;
  nsRefPtr<nsDOMEventListenerWrapper> mOnTimeoutListener;
};

class nsXMLHttpRequestUpload : public nsXHREventTarget,
                               public nsIXMLHttpRequestUpload
{
public:
  nsXMLHttpRequestUpload(nsDOMEventTargetHelper* aOwner)
  {
    BindToOwner(aOwner);
    SetIsDOMBinding();
  }                                         
  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_NSIXMLHTTPREQUESTEVENTTARGET(nsXHREventTarget::)
  NS_FORWARD_NSIDOMEVENTTARGET(nsXHREventTarget::)
  NS_DECL_NSIXMLHTTPREQUESTUPLOAD

  virtual JSObject* WrapObject(JSContext *cx, JSObject *scope,
                               bool *triedToWrap)
  {
    return mozilla::dom::bindings::prototypes::XMLHttpRequestUpload::Wrap(cx, scope, this, triedToWrap);
  }
  nsISupports* GetParentObject()
  {
    return GetOwner();
  }

  bool HasListeners()
  {
    return mListenerManager && mListenerManager->HasListeners();
  }
};

class nsXMLHttpRequest : public nsXHREventTarget,
                         public nsIXMLHttpRequest,
                         public nsIJSXMLHttpRequest,
                         public nsIStreamListener,
                         public nsIChannelEventSink,
                         public nsIProgressEventSink,
                         public nsIInterfaceRequestor,
                         public nsSupportsWeakReference,
                         public nsIJSNativeInitializer,
                         public nsITimerCallback
{
  friend class nsXHRParseEndListener;
public:
  nsXMLHttpRequest();
  virtual ~nsXMLHttpRequest();

  virtual JSObject* WrapObject(JSContext *cx, JSObject *scope,
                               bool *triedToWrap)
  {
    return mozilla::dom::bindings::prototypes::XMLHttpRequest::Wrap(cx, scope, this, triedToWrap);
  }
  nsISupports* GetParentObject()
  {
    return GetOwner();
  }

  // The WebIDL parser converts constructors into methods called _Constructor.
  static already_AddRefed<nsXMLHttpRequest>
  _Constructor(nsISupports* aGlobal, nsresult& aRv)
  {
    nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal);
    nsCOMPtr<nsIScriptObjectPrincipal> principal = do_QueryInterface(aGlobal);
    if (!window || ! principal) {
      aRv = NS_ERROR_FAILURE;
      return NULL;
    }

    nsRefPtr<nsXMLHttpRequest> req = new nsXMLHttpRequest();
    req->Construct(principal->GetPrincipal(), window);
    return req.forget();
  }

  void Construct(nsIPrincipal* aPrincipal,
                 nsPIDOMWindow* aOwnerWindow,
                 nsIURI* aBaseURI = NULL)
  {
    MOZ_ASSERT(aPrincipal);
    MOZ_ASSERT_IF(aOwnerWindow, aOwnerWindow->IsInnerWindow());
    mPrincipal = aPrincipal;
    BindToOwner(aOwnerWindow);
    mBaseURI = aBaseURI;
  }

  NS_DECL_ISUPPORTS_INHERITED

  // nsIXMLHttpRequest
  NS_DECL_NSIXMLHTTPREQUEST

  // nsIJSXMLHttpRequest
  NS_IMETHOD GetOnuploadprogress(nsIDOMEventListener** aOnuploadprogress);
  NS_IMETHOD SetOnuploadprogress(nsIDOMEventListener* aOnuploadprogress);

  NS_FORWARD_NSIXMLHTTPREQUESTEVENTTARGET(nsXHREventTarget::)

  // nsIStreamListener
  NS_DECL_NSISTREAMLISTENER

  // nsIRequestObserver
  NS_DECL_NSIREQUESTOBSERVER

  // nsIChannelEventSink
  NS_DECL_NSICHANNELEVENTSINK

  // nsIProgressEventSink
  NS_DECL_NSIPROGRESSEVENTSINK

  // nsIInterfaceRequestor
  NS_DECL_NSIINTERFACEREQUESTOR

  // nsITimerCallback
  NS_DECL_NSITIMERCALLBACK

  // nsIJSNativeInitializer
  NS_IMETHOD Initialize(nsISupports* aOwner, JSContext* cx, JSObject* obj,
                       PRUint32 argc, jsval* argv);

  NS_FORWARD_NSIDOMEVENTTARGET(nsXHREventTarget::)

#ifdef DEBUG
  void StaticAssertions();
#endif

  // event handler
  IMPL_EVENT_HANDLER(readystatechange, Readystatechange)

  // states
  uint16_t GetReadyState();

  // request
  void Open(const nsAString& aMethod, const nsAString& aUrl, bool aAsync,
            const nsAString& aUser, const nsAString& aPassword, nsresult& aRv)
  {
    aRv = Open(NS_ConvertUTF16toUTF8(aMethod), NS_ConvertUTF16toUTF8(aUrl),
               aAsync, aUser, aPassword);
  }
  void SetRequestHeader(const nsAString& aHeader, const nsAString& aValue,
                        nsresult& aRv)
  {
    aRv = SetRequestHeader(NS_ConvertUTF16toUTF8(aHeader),
                           NS_ConvertUTF16toUTF8(aValue));
  }
  uint32_t GetTimeout()
  {
    return mTimeoutMilliseconds;
  }
  void SetTimeout(uint32_t aTimeout, nsresult& aRv);
  bool GetWithCredentials();
  void SetWithCredentials(bool aWithCredentials, nsresult& aRv);
  nsXMLHttpRequestUpload* GetUpload();

private:
  class RequestBody
  {
  public:
    RequestBody() : mType(Uninitialized)
    {
    }
    RequestBody(JSObject* aArrayBuffer) : mType(ArrayBuffer)
    {
      mValue.mArrayBuffer = aArrayBuffer;
    }
    RequestBody(nsIDOMBlob* aBlob) : mType(Blob)
    {
      mValue.mBlob = aBlob;
    }
    RequestBody(nsIDocument* aDocument) : mType(Document)
    {
      mValue.mDocument = aDocument;
    }
    RequestBody(const nsAString& aString) : mType(DOMString)
    {
      mValue.mString = &aString;
    }
    RequestBody(nsIDOMFormData* aFormData) : mType(FormData)
    {
      mValue.mFormData = aFormData;
    }
    RequestBody(nsIInputStream* aStream) : mType(InputStream)
    {
      mValue.mStream = aStream;
    }

    enum Type {
      Uninitialized,
      ArrayBuffer,
      Blob,
      Document,
      DOMString,
      FormData,
      InputStream
    };
    union Value {
      JSObject* mArrayBuffer;
      nsIDOMBlob* mBlob;
      nsIDocument* mDocument;
      const nsAString* mString;
      nsIDOMFormData* mFormData;
      nsIInputStream* mStream;
    };

    Type GetType() const
    {
      MOZ_ASSERT(mType != Uninitialized);
      return mType;
    }
    Value GetValue() const
    {
      MOZ_ASSERT(mType != Uninitialized);
      return mValue;
    }

  private:
    Type mType;
    Value mValue;
  };

  static nsresult GetRequestBody(nsIVariant* aVariant,
                                 JSContext* aCx,
                                 const Nullable<RequestBody>& aBody,
                                 nsIInputStream** aResult,
                                 nsACString& aContentType,
                                 nsACString& aCharset);

  nsresult Send(JSContext *aCx, nsIVariant* aVariant, const Nullable<RequestBody>& aBody);
  nsresult Send(JSContext *aCx, const Nullable<RequestBody>& aBody)
  {
    return Send(aCx, nsnull, aBody);
  }
  nsresult Send(JSContext *aCx, const RequestBody& aBody)
  {
    return Send(aCx, Nullable<RequestBody>(aBody));
  }

public:
  void Send(JSContext *aCx, nsresult& aRv)
  {
    aRv = Send(aCx, Nullable<RequestBody>());
  }
  void Send(JSContext *aCx, JSObject* aArrayBuffer, nsresult& aRv)
  {
    NS_ASSERTION(aArrayBuffer, "Null should go to string version");
    aRv = Send(aCx, RequestBody(aArrayBuffer));
  }
  void Send(JSContext *aCx, nsIDOMBlob* aBlob, nsresult& aRv)
  {
    NS_ASSERTION(aBlob, "Null should go to string version");
    aRv = Send(aCx, RequestBody(aBlob));
  }
  void Send(JSContext *aCx, nsIDocument* aDoc, nsresult& aRv)
  {
    NS_ASSERTION(aDoc, "Null should go to string version");
    aRv = Send(aCx, RequestBody(aDoc));
  }
  void Send(JSContext *aCx, const nsAString& aString, nsresult& aRv)
  {
    if (DOMStringIsNull(aString)) {
      Send(aCx, aRv);
    }
    else {
      aRv = Send(aCx, RequestBody(aString));
    }
  }
  void Send(JSContext *aCx, nsIDOMFormData* aFormData, nsresult& aRv)
  {
    NS_ASSERTION(aFormData, "Null should go to string version");
    aRv = Send(aCx, RequestBody(aFormData));
  }
  void Send(JSContext *aCx, nsIInputStream* aStream, nsresult& aRv)
  {
    NS_ASSERTION(aStream, "Null should go to string version");
    aRv = Send(aCx, RequestBody(aStream));
  }
  void SendAsBinary(JSContext *aCx, const nsAString& aBody, nsresult& aRv);

  void Abort();

  // response
  uint32_t GetStatus();
  void GetStatusText(nsString& aStatusText);
  void GetResponseHeader(const nsACString& aHeader, nsACString& aResult,
                         nsresult& aRv);
  void GetResponseHeader(const nsAString& aHeader, nsString& aResult,
                         nsresult& aRv)
  {
    nsCString result;
    GetResponseHeader(NS_ConvertUTF16toUTF8(aHeader), result, aRv);
    if (result.IsVoid()) {
      aResult.SetIsVoid(true);
    }
    else {
      // We use UTF8ToNewUnicode here because it truncates after invalid UTF-8
      // characters, CopyUTF8toUTF16 just doesn't copy in that case.
      PRUint32 length;
      PRUnichar* chars = UTF8ToNewUnicode(result, &length);
      aResult.Adopt(chars, length);
    }
  }
  void GetAllResponseHeaders(nsString& aResponseHeaders);
  void OverrideMimeType(const nsAString& aMimeType)
  {
    // XXX Should we do some validation here?
    mOverrideMimeType = aMimeType;
  }
  XMLHttpRequestResponseType GetResponseType()
  {
    return XMLHttpRequestResponseType(mResponseType);
  }
  void SetResponseType(XMLHttpRequestResponseType aType, nsresult& aRv);
  JS::Value GetResponse(JSContext* aCx, nsresult& aRv);
  void GetResponseText(nsString& aResponseText, nsresult& aRv);
  nsIDocument* GetResponseXML(nsresult& aRv);

  bool GetMozBackgroundRequest();
  void SetMozBackgroundRequest(bool aMozBackgroundRequest, nsresult& aRv);
  bool GetMultipart();
  void SetMultipart(bool aMultipart, nsresult& aRv);

  nsIChannel* GetChannel()
  {
    return mChannel;
  }

  // We need a GetInterface callable from JS for chrome JS
  JS::Value GetInterface(JSContext* aCx, nsIJSIID* aIID, nsresult& aRv);

  // This creates a trusted readystatechange event, which is not cancelable and
  // doesn't bubble.
  static nsresult CreateReadystatechangeEvent(nsIDOMEvent** aDOMEvent);
  // For backwards compatibility aPosition should contain the headers for upload
  // and aTotalSize is LL_MAXUINT when unknown. Both those values are
  // used by nsXMLHttpProgressEvent. Normal progress event should not use
  // headers in aLoaded and aTotal is 0 when unknown.
  void DispatchProgressEvent(nsDOMEventTargetHelper* aTarget,
                             const nsAString& aType,
                             // Whether to use nsXMLHttpProgressEvent,
                             // which implements LS Progress Event.
                             bool aUseLSEventWrapper,
                             bool aLengthComputable,
                             // For Progress Events
                             PRUint64 aLoaded, PRUint64 aTotal,
                             // For LS Progress Events
                             PRUint64 aPosition, PRUint64 aTotalSize);
  void DispatchProgressEvent(nsDOMEventTargetHelper* aTarget,
                             const nsAString& aType,
                             bool aLengthComputable,
                             PRUint64 aLoaded, PRUint64 aTotal)
  {
    DispatchProgressEvent(aTarget, aType, false,
                          aLengthComputable, aLoaded, aTotal,
                          aLoaded, aLengthComputable ? aTotal : LL_MAXUINT);
  }

  // Dispatch the "progress" event on the XHR or XHR.upload object if we've
  // received data since the last "progress" event. Also dispatches
  // "uploadprogress" as needed.
  void MaybeDispatchProgressEvents(bool aFinalProgress);

  // This is called by the factory constructor.
  nsresult Init();

  void SetRequestObserver(nsIRequestObserver* aObserver);

  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS_INHERITED(nsXMLHttpRequest,
                                                                   nsXHREventTarget)
  bool AllowUploadProgress();
  void RootResultArrayBuffer();

  virtual void DisconnectFromOwner();
protected:
  friend class nsMultipartProxyListener;

  nsresult DetectCharset();
  nsresult AppendToResponseText(const char * aBuffer, PRUint32 aBufferLen);
  static NS_METHOD StreamReaderFunc(nsIInputStream* in,
                void* closure,
                const char* fromRawSegment,
                PRUint32 toOffset,
                PRUint32 count,
                PRUint32 *writeCount);
  nsresult CreateResponseParsedJSON(JSContext* aCx);
  nsresult CreatePartialBlob(void);
  bool CreateDOMFile(nsIRequest *request);
  // Change the state of the object with this. The broadcast argument
  // determines if the onreadystatechange listener should be called.
  nsresult ChangeState(PRUint32 aState, bool aBroadcast = true);
  already_AddRefed<nsILoadGroup> GetLoadGroup() const;
  nsIURI *GetBaseURI();

  nsresult RemoveAddEventListener(const nsAString& aType,
                                  nsRefPtr<nsDOMEventListenerWrapper>& aCurrent,
                                  nsIDOMEventListener* aNew);

  nsresult GetInnerEventListener(nsRefPtr<nsDOMEventListenerWrapper>& aWrapper,
                                 nsIDOMEventListener** aListener);

  already_AddRefed<nsIHttpChannel> GetCurrentHttpChannel();

  bool IsSystemXHR();

  void ChangeStateToDone();

  /**
   * Check if aChannel is ok for a cross-site request by making sure no
   * inappropriate headers are set, and no username/password is set.
   *
   * Also updates the XML_HTTP_REQUEST_USE_XSITE_AC bit.
   */
  nsresult CheckChannelForCrossSiteRequest(nsIChannel* aChannel);

  void StartProgressEventTimer();

  friend class AsyncVerifyRedirectCallbackForwarder;
  void OnRedirectVerifyCallback(nsresult result);

  nsresult Open(const nsACString& method, const nsACString& url, bool async,
                const nsAString& user, const nsAString& password);

  nsCOMPtr<nsISupports> mContext;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsIChannel> mChannel;
  // mReadRequest is different from mChannel for multipart requests
  nsCOMPtr<nsIRequest> mReadRequest;
  nsCOMPtr<nsIDocument> mResponseXML;
  nsCOMPtr<nsIChannel> mCORSPreflightChannel;
  nsTArray<nsCString> mCORSUnsafeHeaders;

  nsRefPtr<nsDOMEventListenerWrapper> mOnUploadProgressListener;
  nsRefPtr<nsDOMEventListenerWrapper> mOnReadystatechangeListener;

  nsCOMPtr<nsIStreamListener> mXMLParserStreamListener;

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

  // The bytes of our response body. Only used for DEFAULT, ARRAYBUFFER and
  // BLOB responseTypes
  nsCString mResponseBody;

  // The text version of our response body. This is incrementally decoded into
  // as we receive network data. However for the DEFAULT responseType we
  // lazily decode into this from mResponseBody only when .responseText is
  // accessed.
  // Only used for DEFAULT and TEXT responseTypes.
  nsString mResponseText;
  
  // For DEFAULT responseType we use this to keep track of how far we've
  // lazily decoded from mResponseBody to mResponseText
  PRUint32 mResponseBodyDecodedPos;

  // Decoder used for decoding into mResponseText
  // Only used for DEFAULT, TEXT and JSON responseTypes.
  // In cases where we've only received half a surrogate, the decoder itself
  // carries the state to remember this. Next time we receive more data we
  // simply feed the new data into the decoder which will handle the second
  // part of the surrogate.
  nsCOMPtr<nsIUnicodeDecoder> mDecoder;

  nsCString mResponseCharset;

  enum ResponseType {
    XML_HTTP_RESPONSE_TYPE_DEFAULT,
    XML_HTTP_RESPONSE_TYPE_ARRAYBUFFER,
    XML_HTTP_RESPONSE_TYPE_BLOB,
    XML_HTTP_RESPONSE_TYPE_DOCUMENT,
    XML_HTTP_RESPONSE_TYPE_JSON,
    XML_HTTP_RESPONSE_TYPE_TEXT,
    XML_HTTP_RESPONSE_TYPE_CHUNKED_TEXT,
    XML_HTTP_RESPONSE_TYPE_CHUNKED_ARRAYBUFFER,
    XML_HTTP_RESPONSE_TYPE_MOZ_BLOB
  };

  void SetResponseType(nsXMLHttpRequest::ResponseType aType, nsresult& aRv);

  ResponseType mResponseType;

  // It is either a cached blob-response from the last call to GetResponse,
  // but is also explicitly set in OnStopRequest.
  nsCOMPtr<nsIDOMBlob> mResponseBlob;
  // Non-null only when we are able to get a os-file representation of the
  // response, i.e. when loading from a file, or when the http-stream
  // caches into a file or is reading from a cached file.
  nsRefPtr<nsDOMFileBase> mDOMFile;
  // We stream data to mBuilder when response type is "blob" or "moz-blob"
  // and mDOMFile is null.
  nsRefPtr<nsDOMBlobBuilder> mBuilder;

  nsString mOverrideMimeType;

  /**
   * The notification callbacks the channel had when Send() was
   * called.  We want to forward things here as needed.
   */
  nsCOMPtr<nsIInterfaceRequestor> mNotificationCallbacks;
  /**
   * Sink interfaces that we implement that mNotificationCallbacks may
   * want to also be notified for.  These are inited lazily if we're
   * asked for the relevant interface.
   */
  nsCOMPtr<nsIChannelEventSink> mChannelEventSink;
  nsCOMPtr<nsIProgressEventSink> mProgressEventSink;

  nsIRequestObserver* mRequestObserver;

  nsCOMPtr<nsIURI> mBaseURI;

  PRUint32 mState;

  nsRefPtr<nsXMLHttpRequestUpload> mUpload;
  PRUint64 mUploadTransferred;
  PRUint64 mUploadTotal;
  bool mUploadLengthComputable;
  bool mUploadComplete;
  bool mProgressSinceLastProgressEvent;
  PRUint64 mUploadProgress; // For legacy
  PRUint64 mUploadProgressMax; // For legacy

  // Timeout support
  PRTime mRequestSentTime;
  PRUint32 mTimeoutMilliseconds;
  nsCOMPtr<nsITimer> mTimeoutTimer;
  void StartTimeoutTimer();
  void HandleTimeoutCallback();

  bool mErrorLoad;
  bool mWaitingForOnStopRequest;
  bool mProgressTimerIsActive;
  bool mProgressEventWasDelayed;
  bool mIsHtml;
  bool mWarnAboutMultipartHtml;
  bool mWarnAboutSyncHtml;
  bool mLoadLengthComputable;
  PRUint64 mLoadTotal; // 0 if not known.
  PRUint64 mLoadTransferred;
  nsCOMPtr<nsITimer> mProgressNotifier;
  void HandleProgressTimerCallback();

  /**
   * Close the XMLHttpRequest's channels and dispatch appropriate progress
   * events.
   *
   * @param aType The progress event type.
   * @param aFlag A XML_HTTP_REQUEST_* state flag defined in
   *              nsXMLHttpRequest.cpp.
   */
  void CloseRequestWithError(const nsAString& aType, const PRUint32 aFlag);

  bool mFirstStartRequestSeen;
  bool mInLoadProgressEvent;
  
  nsCOMPtr<nsIAsyncVerifyRedirectCallback> mRedirectCallback;
  nsCOMPtr<nsIChannel> mNewRedirectChannel;
  
  jsval mResultJSON;
  JSObject* mResultArrayBuffer;

  void ResetResponse();

  struct RequestHeader
  {
    nsCString header;
    nsCString value;
  };
  nsTArray<RequestHeader> mModifiedRequestHeaders;
};

#undef IMPL_EVENT_HANDLER

// helper class to expose a progress DOM Event

class nsXMLHttpProgressEvent : public nsIDOMProgressEvent,
                               public nsIDOMLSProgressEvent,
                               public nsIDOMNSEvent,
                               public nsIPrivateDOMEvent
{
public:
  nsXMLHttpProgressEvent(nsIDOMProgressEvent* aInner,
                         PRUint64 aCurrentProgress,
                         PRUint64 aMaxProgress,
                         nsPIDOMWindow* aWindow);
  virtual ~nsXMLHttpProgressEvent();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsXMLHttpProgressEvent, nsIDOMNSEvent)
  NS_FORWARD_NSIDOMEVENT(mInner->)
  NS_FORWARD_NSIDOMNSEVENT(mInner->)
  NS_FORWARD_NSIDOMPROGRESSEVENT(mInner->)
  NS_DECL_NSIDOMLSPROGRESSEVENT
  // nsPrivateDOMEvent
  NS_IMETHOD DuplicatePrivateData()
  {
    return mInner->DuplicatePrivateData();
  }
  NS_IMETHOD SetTarget(nsIDOMEventTarget* aTarget)
  {
    return mInner->SetTarget(aTarget);
  }
  NS_IMETHOD_(bool) IsDispatchStopped()
  {
    return mInner->IsDispatchStopped();
  }
  NS_IMETHOD_(nsEvent*) GetInternalNSEvent()
  {
    return mInner->GetInternalNSEvent();
  }
  NS_IMETHOD SetTrusted(bool aTrusted)
  {
    return mInner->SetTrusted(aTrusted);
  }
  virtual void Serialize(IPC::Message* aMsg,
                         bool aSerializeInterfaceType)
  {
    mInner->Serialize(aMsg, aSerializeInterfaceType);
  }
  virtual bool Deserialize(const IPC::Message* aMsg, void** aIter)
  {
    return mInner->Deserialize(aMsg, aIter);
  }

protected:
  void WarnAboutLSProgressEvent(nsIDocument::DeprecatedOperations);

  // Use nsDOMProgressEvent so that we can forward
  // most of the method calls easily.
  nsRefPtr<nsDOMProgressEvent> mInner;
  nsCOMPtr<nsPIDOMWindow> mWindow;
  PRUint64 mCurProgress;
  PRUint64 mMaxProgress;
};

class nsXHRParseEndListener : public nsIDOMEventListener
{
public:
  NS_DECL_ISUPPORTS
  NS_IMETHOD HandleEvent(nsIDOMEvent *event)
  {
    nsCOMPtr<nsIXMLHttpRequest> xhr = do_QueryReferent(mXHR);
    if (xhr) {
      static_cast<nsXMLHttpRequest*>(xhr.get())->ChangeStateToDone();
    }
    mXHR = nsnull;
    return NS_OK;
  }
  nsXHRParseEndListener(nsIXMLHttpRequest* aXHR)
    : mXHR(do_GetWeakReference(aXHR)) {}
  virtual ~nsXHRParseEndListener() {}
private:
  nsWeakPtr mXHR;
};

#endif

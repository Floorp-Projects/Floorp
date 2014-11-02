/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXMLHttpRequest_h__
#define nsXMLHttpRequest_h__

#include "mozilla/Attributes.h"
#include "nsIXMLHttpRequest.h"
#include "nsISupportsUtils.h"
#include "nsString.h"
#include "nsIURI.h"
#include "nsIHttpChannel.h"
#include "nsIDocument.h"
#include "nsIStreamListener.h"
#include "nsWeakReference.h"
#include "nsIChannelEventSink.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIInterfaceRequestor.h"
#include "nsIHttpHeaderVisitor.h"
#include "nsIProgressEventSink.h"
#include "nsJSUtils.h"
#include "nsTArray.h"
#include "nsITimer.h"
#include "nsIPrincipal.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsISizeOfEventTarget.h"
#include "nsIXPConnect.h"
#include "nsIInputStream.h"
#include "mozilla/Assertions.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/XMLHttpRequestBinding.h"

#ifdef Status
/* Xlib headers insist on this for some reason... Nuke it because
   it'll override our member name */
#undef Status
#endif

class AsyncVerifyRedirectCallbackForwarder;
class nsFormData;
class nsIJARChannel;
class nsILoadGroup;
class nsIUnicodeDecoder;
class nsIJSID;

namespace mozilla {

namespace dom {
class BlobSet;
class File;
}

// A helper for building up an ArrayBuffer object's data
// before creating the ArrayBuffer itself.  Will do doubling
// based reallocation, up to an optional maximum growth given.
//
// When all the data has been appended, call getArrayBuffer,
// passing in the JSContext* for which the ArrayBuffer object
// is to be created.  This also implicitly resets the builder,
// or it can be reset explicitly at any point by calling reset().
class ArrayBufferBuilder
{
  uint8_t* mDataPtr;
  uint32_t mCapacity;
  uint32_t mLength;
  void* mMapPtr;
public:
  ArrayBufferBuilder();
  ~ArrayBufferBuilder();

  void reset();

  // Will truncate if aNewCap is < length().
  bool setCapacity(uint32_t aNewCap);

  // Append aDataLen bytes from data to the current buffer.  If we
  // need to grow the buffer, grow by doubling the size up to a
  // maximum of aMaxGrowth (if given).  If aDataLen is greater than
  // what the new capacity would end up as, then grow by aDataLen.
  //
  // The data parameter must not overlap with anything beyond the
  // builder's current valid contents [0..length)
  bool append(const uint8_t* aNewData, uint32_t aDataLen,
              uint32_t aMaxGrowth = 0);

  uint32_t length()   { return mLength; }
  uint32_t capacity() { return mCapacity; }

  JSObject* getArrayBuffer(JSContext* aCx);

  // Memory mapping to starting position of file(aFile) in the zip
  // package(aJarFile).
  //
  // The file in the zip package has to be uncompressed and the starting
  // position of the file must be aligned according to array buffer settings
  // in JS engine.
  nsresult mapToFileInPackage(const nsCString& aFile, nsIFile* aJarFile);

protected:
  static bool areOverlappingRegions(const uint8_t* aStart1, uint32_t aLength1,
                                    const uint8_t* aStart2, uint32_t aLength2);
};

} // namespace mozilla

class nsXHREventTarget : public mozilla::DOMEventTargetHelper,
                         public nsIXMLHttpRequestEventTarget
{
protected:
  explicit nsXHREventTarget(mozilla::DOMEventTargetHelper* aOwner)
    : mozilla::DOMEventTargetHelper(aOwner)
  {
  }

  nsXHREventTarget()
  {
  }

  virtual ~nsXHREventTarget() {}

public:
  typedef mozilla::dom::XMLHttpRequestResponseType
          XMLHttpRequestResponseType;
  typedef mozilla::ErrorResult
          ErrorResult;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsXHREventTarget,
                                           mozilla::DOMEventTargetHelper)
  NS_DECL_NSIXMLHTTPREQUESTEVENTTARGET
  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(mozilla::DOMEventTargetHelper)

  IMPL_EVENT_HANDLER(loadstart)
  IMPL_EVENT_HANDLER(progress)
  IMPL_EVENT_HANDLER(abort)
  IMPL_EVENT_HANDLER(error)
  IMPL_EVENT_HANDLER(load)
  IMPL_EVENT_HANDLER(timeout)
  IMPL_EVENT_HANDLER(loadend)
  
  virtual void DisconnectFromOwner();
};

class nsXMLHttpRequestUpload MOZ_FINAL : public nsXHREventTarget,
                                         public nsIXMLHttpRequestUpload
{
public:
  explicit nsXMLHttpRequestUpload(mozilla::DOMEventTargetHelper* aOwner)
    : nsXHREventTarget(aOwner)
  {
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_NSIXMLHTTPREQUESTEVENTTARGET(nsXHREventTarget::)
  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(nsXHREventTarget)
  NS_DECL_NSIXMLHTTPREQUESTUPLOAD

  virtual JSObject* WrapObject(JSContext *cx) MOZ_OVERRIDE;
  nsISupports* GetParentObject()
  {
    return GetOwner();
  }

  bool HasListeners()
  {
    return mListenerManager && mListenerManager->HasListeners();
  }

private:
  virtual ~nsXMLHttpRequestUpload() {}
};

class nsXMLHttpRequestXPCOMifier;

// Make sure that any non-DOM interfaces added here are also added to
// nsXMLHttpRequestXPCOMifier.
class nsXMLHttpRequest MOZ_FINAL : public nsXHREventTarget,
                                   public nsIXMLHttpRequest,
                                   public nsIJSXMLHttpRequest,
                                   public nsIStreamListener,
                                   public nsIChannelEventSink,
                                   public nsIProgressEventSink,
                                   public nsIInterfaceRequestor,
                                   public nsSupportsWeakReference,
                                   public nsITimerCallback,
                                   public nsISizeOfEventTarget
{
  friend class nsXHRParseEndListener;
  friend class nsXMLHttpRequestXPCOMifier;

public:
  nsXMLHttpRequest();

  virtual JSObject* WrapObject(JSContext *cx) MOZ_OVERRIDE
  {
    return mozilla::dom::XMLHttpRequestBinding::Wrap(cx, this);
  }
  nsISupports* GetParentObject()
  {
    return GetOwner();
  }

  // The WebIDL constructors.
  static already_AddRefed<nsXMLHttpRequest>
  Constructor(const mozilla::dom::GlobalObject& aGlobal,
              JSContext* aCx,
              const mozilla::dom::MozXMLHttpRequestParameters& aParams,
              ErrorResult& aRv)
  {
    nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
    nsCOMPtr<nsIScriptObjectPrincipal> principal =
      do_QueryInterface(aGlobal.GetAsSupports());
    if (!global || ! principal) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    nsRefPtr<nsXMLHttpRequest> req = new nsXMLHttpRequest();
    req->Construct(principal->GetPrincipal(), global);
    req->InitParameters(aParams.mMozAnon, aParams.mMozSystem);
    return req.forget();
  }

  static already_AddRefed<nsXMLHttpRequest>
  Constructor(const mozilla::dom::GlobalObject& aGlobal,
              JSContext* aCx,
              const nsAString& ignored,
              ErrorResult& aRv)
  {
    // Pretend like someone passed null, so we can pick up the default values
    mozilla::dom::MozXMLHttpRequestParameters params;
    if (!params.Init(aCx, JS::NullHandleValue)) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    return Constructor(aGlobal, aCx, params, aRv);
  }

  void Construct(nsIPrincipal* aPrincipal,
                 nsIGlobalObject* aGlobalObject,
                 nsIURI* aBaseURI = nullptr)
  {
    MOZ_ASSERT(aPrincipal);
    MOZ_ASSERT_IF(nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(
      aGlobalObject), win->IsInnerWindow());
    mPrincipal = aPrincipal;
    BindToOwner(aGlobalObject);
    mBaseURI = aBaseURI;
  }

  void InitParameters(bool aAnon, bool aSystem);

  void SetParameters(bool aAnon, bool aSystem)
  {
    mIsAnon = aAnon || aSystem;
    mIsSystem = aSystem;
  }

  NS_DECL_ISUPPORTS_INHERITED

  // nsIXMLHttpRequest
  NS_DECL_NSIXMLHTTPREQUEST

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

  // nsISizeOfEventTarget
  virtual size_t
    SizeOfEventTargetIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(nsXHREventTarget)

#ifdef DEBUG
  void StaticAssertions();
#endif

  // event handler
  IMPL_EVENT_HANDLER(readystatechange)

  // states
  uint16_t ReadyState();

  // request
  void Open(const nsACString& aMethod, const nsAString& aUrl, ErrorResult& aRv)
  {
    Open(aMethod, aUrl, true,
         mozilla::dom::Optional<nsAString>(),
         mozilla::dom::Optional<nsAString>(),
         aRv);
  }
  void Open(const nsACString& aMethod, const nsAString& aUrl, bool aAsync,
            const mozilla::dom::Optional<nsAString>& aUser,
            const mozilla::dom::Optional<nsAString>& aPassword,
            ErrorResult& aRv)
  {
    aRv = Open(aMethod, NS_ConvertUTF16toUTF8(aUrl),
               aAsync, aUser, aPassword);
  }
  void SetRequestHeader(const nsACString& aHeader, const nsACString& aValue,
                        ErrorResult& aRv)
  {
    aRv = SetRequestHeader(aHeader, aValue);
  }
  uint32_t Timeout()
  {
    return mTimeoutMilliseconds;
  }
  void SetTimeout(uint32_t aTimeout, ErrorResult& aRv);
  bool WithCredentials();
  void SetWithCredentials(bool aWithCredentials, ErrorResult& aRv);
  nsXMLHttpRequestUpload* Upload();

private:
  virtual ~nsXMLHttpRequest();

  class RequestBody
  {
  public:
    RequestBody() : mType(Uninitialized)
    {
    }
    explicit RequestBody(const mozilla::dom::ArrayBuffer* aArrayBuffer) : mType(ArrayBuffer)
    {
      mValue.mArrayBuffer = aArrayBuffer;
    }
    explicit RequestBody(const mozilla::dom::ArrayBufferView* aArrayBufferView) : mType(ArrayBufferView)
    {
      mValue.mArrayBufferView = aArrayBufferView;
    }
    explicit RequestBody(mozilla::dom::File& aBlob) : mType(Blob)
    {
      mValue.mBlob = &aBlob;
    }
    explicit RequestBody(nsIDocument* aDocument) : mType(Document)
    {
      mValue.mDocument = aDocument;
    }
    explicit RequestBody(const nsAString& aString) : mType(DOMString)
    {
      mValue.mString = &aString;
    }
    explicit RequestBody(nsFormData& aFormData) : mType(FormData)
    {
      mValue.mFormData = &aFormData;
    }
    explicit RequestBody(nsIInputStream* aStream) : mType(InputStream)
    {
      mValue.mStream = aStream;
    }

    enum Type {
      Uninitialized,
      ArrayBuffer,
      ArrayBufferView,
      Blob,
      Document,
      DOMString,
      FormData,
      InputStream
    };
    union Value {
      const mozilla::dom::ArrayBuffer* mArrayBuffer;
      const mozilla::dom::ArrayBufferView* mArrayBufferView;
      mozilla::dom::File* mBlob;
      nsIDocument* mDocument;
      const nsAString* mString;
      nsFormData* mFormData;
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
                                 const Nullable<RequestBody>& aBody,
                                 nsIInputStream** aResult,
                                 uint64_t* aContentLength,
                                 nsACString& aContentType,
                                 nsACString& aCharset);

  nsresult Send(nsIVariant* aVariant, const Nullable<RequestBody>& aBody);
  nsresult Send(const Nullable<RequestBody>& aBody)
  {
    return Send(nullptr, aBody);
  }
  nsresult Send(const RequestBody& aBody)
  {
    return Send(Nullable<RequestBody>(aBody));
  }

  bool IsDeniedCrossSiteRequest();

public:
  void Send(ErrorResult& aRv)
  {
    aRv = Send(Nullable<RequestBody>());
  }
  void Send(const mozilla::dom::ArrayBuffer& aArrayBuffer, ErrorResult& aRv)
  {
    aRv = Send(RequestBody(&aArrayBuffer));
  }
  void Send(const mozilla::dom::ArrayBufferView& aArrayBufferView,
            ErrorResult& aRv)
  {
    aRv = Send(RequestBody(&aArrayBufferView));
  }
  void Send(mozilla::dom::File& aBlob, ErrorResult& aRv)
  {
    aRv = Send(RequestBody(aBlob));
  }
  void Send(nsIDocument& aDoc, ErrorResult& aRv)
  {
    aRv = Send(RequestBody(&aDoc));
  }
  void Send(const nsAString& aString, ErrorResult& aRv)
  {
    if (DOMStringIsNull(aString)) {
      Send(aRv);
    }
    else {
      aRv = Send(RequestBody(aString));
    }
  }
  void Send(nsFormData& aFormData, ErrorResult& aRv)
  {
    aRv = Send(RequestBody(aFormData));
  }
  void Send(nsIInputStream* aStream, ErrorResult& aRv)
  {
    NS_ASSERTION(aStream, "Null should go to string version");
    nsCOMPtr<nsIXPConnectWrappedJS> wjs = do_QueryInterface(aStream);
    if (wjs) {
      aRv.Throw(NS_ERROR_DOM_TYPE_ERR);
      return;
    }
    aRv = Send(RequestBody(aStream));
  }
  void SendAsBinary(const nsAString& aBody, ErrorResult& aRv);

  void Abort();

  // response
  void GetResponseURL(nsAString& aUrl);
  uint32_t Status();
  void GetStatusText(nsCString& aStatusText);
  void GetResponseHeader(const nsACString& aHeader, nsACString& aResult,
                         ErrorResult& aRv);
  void GetResponseHeader(const nsAString& aHeader, nsString& aResult,
                         ErrorResult& aRv)
  {
    nsCString result;
    GetResponseHeader(NS_ConvertUTF16toUTF8(aHeader), result, aRv);
    if (result.IsVoid()) {
      aResult.SetIsVoid(true);
    }
    else {
      // The result value should be inflated:
      CopyASCIItoUTF16(result, aResult);
    }
  }
  void GetAllResponseHeaders(nsCString& aResponseHeaders);
  bool IsSafeHeader(const nsACString& aHeaderName, nsIHttpChannel* aHttpChannel);
  void OverrideMimeType(const nsAString& aMimeType)
  {
    // XXX Should we do some validation here?
    mOverrideMimeType = aMimeType;
  }
  XMLHttpRequestResponseType ResponseType()
  {
    return XMLHttpRequestResponseType(mResponseType);
  }
  void SetResponseType(XMLHttpRequestResponseType aType, ErrorResult& aRv);
  void GetResponse(JSContext* aCx, JS::MutableHandle<JS::Value> aResponse,
                   ErrorResult& aRv);
  void GetResponseText(nsString& aResponseText, ErrorResult& aRv);
  nsIDocument* GetResponseXML(ErrorResult& aRv);

  bool MozBackgroundRequest();
  void SetMozBackgroundRequest(bool aMozBackgroundRequest, nsresult& aRv);

  bool MozAnon();
  bool MozSystem();

  nsIChannel* GetChannel()
  {
    return mChannel;
  }

  // We need a GetInterface callable from JS for chrome JS
  void GetInterface(JSContext* aCx, nsIJSID* aIID,
                    JS::MutableHandle<JS::Value> aRetval, ErrorResult& aRv);

  // This creates a trusted readystatechange event, which is not cancelable and
  // doesn't bubble.
  nsresult CreateReadystatechangeEvent(nsIDOMEvent** aDOMEvent);
  void DispatchProgressEvent(mozilla::DOMEventTargetHelper* aTarget,
                             const nsAString& aType,
                             bool aLengthComputable,
                             uint64_t aLoaded, uint64_t aTotal);

  // Dispatch the "progress" event on the XHR or XHR.upload object if we've
  // received data since the last "progress" event. Also dispatches
  // "uploadprogress" as needed.
  void MaybeDispatchProgressEvents(bool aFinalProgress);

  // This is called by the factory constructor.
  nsresult Init();

  nsresult init(nsIPrincipal* principal,
                nsIScriptContext* scriptContext,
                nsPIDOMWindow* globalObject,
                nsIURI* baseURI);

  void SetRequestObserver(nsIRequestObserver* aObserver);

  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS_INHERITED(nsXMLHttpRequest,
                                                                   nsXHREventTarget)
  bool AllowUploadProgress();
  void RootJSResultObjects();

  virtual void DisconnectFromOwner() MOZ_OVERRIDE;

  static void SetDontWarnAboutSyncXHR(bool aVal)
  {
    sDontWarnAboutSyncXHR = aVal;
  }
  static bool DontWarnAboutSyncXHR()
  {
    return sDontWarnAboutSyncXHR;
  }
protected:
  nsresult DetectCharset();
  nsresult AppendToResponseText(const char * aBuffer, uint32_t aBufferLen);
  static NS_METHOD StreamReaderFunc(nsIInputStream* in,
                void* closure,
                const char* fromRawSegment,
                uint32_t toOffset,
                uint32_t count,
                uint32_t *writeCount);
  nsresult CreateResponseParsedJSON(JSContext* aCx);
  void CreatePartialBlob();
  bool CreateDOMFile(nsIRequest *request);
  // Change the state of the object with this. The broadcast argument
  // determines if the onreadystatechange listener should be called.
  nsresult ChangeState(uint32_t aState, bool aBroadcast = true);
  already_AddRefed<nsILoadGroup> GetLoadGroup() const;
  nsIURI *GetBaseURI();

  already_AddRefed<nsIHttpChannel> GetCurrentHttpChannel();
  already_AddRefed<nsIJARChannel> GetCurrentJARChannel();

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
                const mozilla::dom::Optional<nsAString>& user,
                const mozilla::dom::Optional<nsAString>& password);

  already_AddRefed<nsXMLHttpRequestXPCOMifier> EnsureXPCOMifier();

  nsCOMPtr<nsISupports> mContext;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsIDocument> mResponseXML;
  nsCOMPtr<nsIChannel> mCORSPreflightChannel;
  nsTArray<nsCString> mCORSUnsafeHeaders;

  nsCOMPtr<nsIStreamListener> mXMLParserStreamListener;

  // used to implement getAllResponseHeaders()
  class nsHeaderVisitor : public nsIHttpHeaderVisitor {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIHTTPHEADERVISITOR
    nsHeaderVisitor(nsXMLHttpRequest* aXMLHttpRequest, nsIHttpChannel* aHttpChannel)
      : mXHR(aXMLHttpRequest), mHttpChannel(aHttpChannel) {}
    const nsACString &Headers() { return mHeaders; }
  private:
    virtual ~nsHeaderVisitor() {}

    nsCString mHeaders;
    nsXMLHttpRequest* mXHR;
    nsCOMPtr<nsIHttpChannel> mHttpChannel;
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
  uint32_t mResponseBodyDecodedPos;

  // Decoder used for decoding into mResponseText
  // Only used for DEFAULT, TEXT and JSON responseTypes.
  // In cases where we've only received half a surrogate, the decoder itself
  // carries the state to remember this. Next time we receive more data we
  // simply feed the new data into the decoder which will handle the second
  // part of the surrogate.
  nsCOMPtr<nsIUnicodeDecoder> mDecoder;

  nsCString mResponseCharset;

  enum ResponseTypeEnum {
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

  void SetResponseType(nsXMLHttpRequest::ResponseTypeEnum aType, ErrorResult& aRv);

  ResponseTypeEnum mResponseType;

  // It is either a cached blob-response from the last call to GetResponse,
  // but is also explicitly set in OnStopRequest.
  nsRefPtr<mozilla::dom::File> mResponseBlob;
  // Non-null only when we are able to get a os-file representation of the
  // response, i.e. when loading from a file.
  nsRefPtr<mozilla::dom::File> mDOMFile;
  // We stream data to mBlobSet when response type is "blob" or "moz-blob"
  // and mDOMFile is null.
  nsAutoPtr<mozilla::dom::BlobSet> mBlobSet;

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

  uint32_t mState;

  nsRefPtr<nsXMLHttpRequestUpload> mUpload;
  uint64_t mUploadTransferred;
  uint64_t mUploadTotal;
  bool mUploadLengthComputable;
  bool mUploadComplete;
  bool mProgressSinceLastProgressEvent;

  // Timeout support
  PRTime mRequestSentTime;
  uint32_t mTimeoutMilliseconds;
  nsCOMPtr<nsITimer> mTimeoutTimer;
  void StartTimeoutTimer();
  void HandleTimeoutCallback();

  bool mErrorLoad;
  bool mWaitingForOnStopRequest;
  bool mProgressTimerIsActive;
  bool mIsHtml;
  bool mWarnAboutMultipartHtml;
  bool mWarnAboutSyncHtml;
  bool mLoadLengthComputable;
  uint64_t mLoadTotal; // 0 if not known.
  // Amount of script-exposed (i.e. after undoing gzip compresion) data
  // received.
  uint64_t mDataAvailable;
  // Number of HTTP message body bytes received so far. This quantity is
  // in the same units as Content-Length and mLoadTotal, and hence counts
  // compressed bytes when the channel has gzip Content-Encoding. If the
  // channel does not have Content-Encoding, this will be the same as
  // mDataReceived except between the OnProgress that changes mLoadTransferred
  // and the corresponding OnDataAvailable (which changes mDataReceived).
  // Ordering of OnProgress and OnDataAvailable is undefined.
  uint64_t mLoadTransferred;
  nsCOMPtr<nsITimer> mProgressNotifier;
  void HandleProgressTimerCallback();

  bool mIsSystem;
  bool mIsAnon;

  /**
   * Close the XMLHttpRequest's channels and dispatch appropriate progress
   * events.
   *
   * @param aType The progress event type.
   * @param aFlag A XML_HTTP_REQUEST_* state flag defined in
   *              nsXMLHttpRequest.cpp.
   */
  void CloseRequestWithError(const nsAString& aType, const uint32_t aFlag);

  bool mFirstStartRequestSeen;
  bool mInLoadProgressEvent;

  nsCOMPtr<nsIAsyncVerifyRedirectCallback> mRedirectCallback;
  nsCOMPtr<nsIChannel> mNewRedirectChannel;

  JS::Heap<JS::Value> mResultJSON;

  mozilla::ArrayBufferBuilder mArrayBufferBuilder;
  JS::Heap<JSObject*> mResultArrayBuffer;
  bool mIsMappedArrayBuffer;

  void ResetResponse();

  struct RequestHeader
  {
    nsCString header;
    nsCString value;
  };
  nsTArray<RequestHeader> mModifiedRequestHeaders;

  nsTHashtable<nsCStringHashKey> mAlreadySetHeaders;

  // Helper object to manage our XPCOM scriptability bits
  nsXMLHttpRequestXPCOMifier* mXPCOMifier;

  static bool sDontWarnAboutSyncXHR;
};

class MOZ_STACK_CLASS AutoDontWarnAboutSyncXHR
{
public:
  AutoDontWarnAboutSyncXHR() : mOldVal(nsXMLHttpRequest::DontWarnAboutSyncXHR())
  {
    nsXMLHttpRequest::SetDontWarnAboutSyncXHR(true);
  }

  ~AutoDontWarnAboutSyncXHR()
  {
    nsXMLHttpRequest::SetDontWarnAboutSyncXHR(mOldVal);
  }

private:
  bool mOldVal;
};

// A shim class designed to expose the non-DOM interfaces of
// XMLHttpRequest via XPCOM stuff.
class nsXMLHttpRequestXPCOMifier MOZ_FINAL : public nsIStreamListener,
                                             public nsIChannelEventSink,
                                             public nsIProgressEventSink,
                                             public nsIInterfaceRequestor,
                                             public nsITimerCallback
{
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsXMLHttpRequestXPCOMifier,
                                           nsIStreamListener)

  explicit nsXMLHttpRequestXPCOMifier(nsXMLHttpRequest* aXHR) :
    mXHR(aXHR)
  {
  }

private:
  ~nsXMLHttpRequestXPCOMifier() {
    if (mXHR) {
      mXHR->mXPCOMifier = nullptr;
    }
  }

public:
  NS_FORWARD_NSISTREAMLISTENER(mXHR->)
  NS_FORWARD_NSIREQUESTOBSERVER(mXHR->)
  NS_FORWARD_NSICHANNELEVENTSINK(mXHR->)
  NS_FORWARD_NSIPROGRESSEVENTSINK(mXHR->)
  NS_FORWARD_NSITIMERCALLBACK(mXHR->)

  NS_DECL_NSIINTERFACEREQUESTOR

private:
  nsRefPtr<nsXMLHttpRequest> mXHR;
};

class nsXHRParseEndListener : public nsIDOMEventListener
{
public:
  NS_DECL_ISUPPORTS
  NS_IMETHOD HandleEvent(nsIDOMEvent *event) MOZ_OVERRIDE
  {
    nsCOMPtr<nsIXMLHttpRequest> xhr = do_QueryReferent(mXHR);
    if (xhr) {
      static_cast<nsXMLHttpRequest*>(xhr.get())->ChangeStateToDone();
    }
    mXHR = nullptr;
    return NS_OK;
  }
  explicit nsXHRParseEndListener(nsIXMLHttpRequest* aXHR)
    : mXHR(do_GetWeakReference(aXHR)) {}
private:
  virtual ~nsXHRParseEndListener() {}

  nsWeakPtr mXHR;
};

#endif

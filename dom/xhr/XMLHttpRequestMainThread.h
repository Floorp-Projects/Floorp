/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XMLHttpRequestMainThread_h
#define mozilla_dom_XMLHttpRequestMainThread_h

#include <bitset>
#include "nsAutoPtr.h"
#include "nsISupportsUtils.h"
#include "nsIURI.h"
#include "nsIHttpChannel.h"
#include "nsIDocument.h"
#include "nsIStreamListener.h"
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
#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/NotNull.h"
#include "mozilla/dom/MutableBlobStorage.h"
#include "mozilla/dom/BodyExtractor.h"
#include "mozilla/dom/ClientInfo.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FormData.h"
#include "mozilla/dom/PerformanceStorage.h"
#include "mozilla/dom/ServiceWorkerDescriptor.h"
#include "mozilla/dom/URLSearchParams.h"
#include "mozilla/dom/XMLHttpRequest.h"
#include "mozilla/dom/XMLHttpRequestBinding.h"
#include "mozilla/dom/XMLHttpRequestEventTarget.h"
#include "mozilla/dom/XMLHttpRequestString.h"
#include "mozilla/Encoding.h"

#ifdef Status
/* Xlib headers insist on this for some reason... Nuke it because
   it'll override our member name */
#undef Status
#endif

class nsIJARChannel;
class nsILoadGroup;
class nsIJSID;

namespace mozilla {
namespace dom {

class DOMString;
class XMLHttpRequestUpload;
struct OriginAttributesDictionary;

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

class nsXMLHttpRequestXPCOMifier;

class RequestHeaders
{
  struct RequestHeader
  {
    nsCString mName;
    nsCString mValue;
  };
  nsTArray<RequestHeader> mHeaders;
  RequestHeader* Find(const nsACString& aName);

public:
  class CharsetIterator
  {
    bool mValid;
    int32_t mCurPos, mCurLen, mCutoff;
    nsACString& mSource;

  public:
    explicit CharsetIterator(nsACString& aSource);
    bool Equals(const nsACString& aOther, const nsCStringComparator& aCmp) const;
    void Replace(const nsACString& aReplacement);
    bool Next();
  };

  bool Has(const char* aName);
  bool Has(const nsACString& aName);
  void Get(const char* aName, nsACString& aValue);
  void Get(const nsACString& aName, nsACString& aValue);
  void Set(const char* aName, const nsACString& aValue);
  void Set(const nsACString& aName, const nsACString& aValue);
  void MergeOrSet(const char* aName, const nsACString& aValue);
  void MergeOrSet(const nsACString& aName, const nsACString& aValue);
  void Clear();
  void ApplyToChannel(nsIHttpChannel* aChannel) const;
  void GetCORSUnsafeHeaders(nsTArray<nsCString>& aArray) const;
};

class nsXHRParseEndListener;

// Make sure that any non-DOM interfaces added here are also added to
// nsXMLHttpRequestXPCOMifier.
class XMLHttpRequestMainThread final : public XMLHttpRequest,
                                       public nsIStreamListener,
                                       public nsIChannelEventSink,
                                       public nsIProgressEventSink,
                                       public nsIInterfaceRequestor,
                                       public nsITimerCallback,
                                       public nsISizeOfEventTarget,
                                       public nsINamed,
                                       public MutableBlobStorageCallback
{
  friend class nsXHRParseEndListener;
  friend class nsXMLHttpRequestXPCOMifier;

public:
  enum class ProgressEventType : uint8_t {
    loadstart,
    progress,
    error,
    abort,
    timeout,
    load,
    loadend,
    ENUM_MAX
  };

  // Make sure that any additions done to ErrorType enum are also mirrored in
  // XHR_ERROR_TYPE enum of TelemetrySend.jsm.
  enum class ErrorType : uint16_t {
    eOK,
    eRequest,
    eUnreachable,
    eChannelOpen,
    eRedirect,
    eTerminated,
    ENUM_MAX
  };

  XMLHttpRequestMainThread();

  void Construct(nsIPrincipal* aPrincipal,
                 nsIGlobalObject* aGlobalObject,
                 nsIURI* aBaseURI = nullptr,
                 nsILoadGroup* aLoadGroup = nullptr,
                 PerformanceStorage* aPerformanceStorage = nullptr)
  {
    MOZ_ASSERT(aPrincipal);
    mPrincipal = aPrincipal;
    BindToOwner(aGlobalObject);
    mBaseURI = aBaseURI;
    mLoadGroup = aLoadGroup;
    mPerformanceStorage = aPerformanceStorage;
  }

  void InitParameters(bool aAnon, bool aSystem);

  void SetParameters(bool aAnon, bool aSystem)
  {
    mIsAnon = aAnon || aSystem;
    mIsSystem = aSystem;
  }

  void SetClientInfoAndController(const ClientInfo& aClientInfo,
                                  const Maybe<ServiceWorkerDescriptor>& aController);

  NS_DECL_ISUPPORTS_INHERITED

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

  // nsINamed
  NS_DECL_NSINAMED

  // nsISizeOfEventTarget
  virtual size_t
    SizeOfEventTargetIncludingThis(MallocSizeOf aMallocSizeOf) const override;

  // states
  virtual uint16_t ReadyState() const override;

  // request
  nsresult CreateChannel();
  nsresult InitiateFetch(already_AddRefed<nsIInputStream> aUploadStream,
                         int64_t aUploadLength,
                         nsACString& aUploadContentType);

  virtual void
  Open(const nsACString& aMethod, const nsAString& aUrl,
       ErrorResult& aRv) override;

  virtual void
  Open(const nsACString& aMethod, const nsAString& aUrl, bool aAsync,
       const nsAString& aUsername, const nsAString& aPassword,
       ErrorResult& aRv) override;

  nsresult
  Open(const nsACString& aMethod,
       const nsACString& aUrl,
       bool aAsync,
       const nsAString& aUsername,
       const nsAString& aPassword);

  virtual void
  SetRequestHeader(const nsACString& aName, const nsACString& aValue,
                   ErrorResult& aRv) override;

  virtual uint32_t
  Timeout() const override
  {
    return mTimeoutMilliseconds;
  }

  virtual void
  SetTimeout(uint32_t aTimeout, ErrorResult& aRv) override;

  virtual bool WithCredentials() const override;

  virtual void
  SetWithCredentials(bool aWithCredentials, ErrorResult& aRv) override;

  virtual XMLHttpRequestUpload*
  GetUpload(ErrorResult& aRv) override;

private:
  virtual ~XMLHttpRequestMainThread();

  nsresult MaybeSilentSendFailure(nsresult aRv);
  nsresult SendInternal(const BodyExtractorBase* aBody);

  bool IsCrossSiteCORSRequest() const;
  bool IsDeniedCrossSiteCORSRequest();

  void UnsuppressEventHandlingAndResume();

  // Check pref "dom.mapped_arraybuffer.enabled" to make sure ArrayBuffer is
  // supported.
  static bool IsMappedArrayBufferEnabled();

  // Check pref "dom.xhr.lowercase_header.enabled" to make sure lowercased
  // response header is supported.
  static bool IsLowercaseResponseHeader();

  void MaybeLowerChannelPriority();

public:
  virtual void
  Send(JSContext* aCx,
       const Nullable<DocumentOrBlobOrArrayBufferViewOrArrayBufferOrFormDataOrURLSearchParamsOrUSVString>& aData,
       ErrorResult& aRv) override;

  virtual void
  SendInputStream(nsIInputStream* aInputStream, ErrorResult& aRv) override
  {
    BodyExtractor<nsIInputStream> body(aInputStream);
    aRv = SendInternal(&body);
  }

  void
  RequestErrorSteps(const ProgressEventType aEventType,
                    const nsresult aOptionalException,
                    ErrorResult& aRv);

  void
  Abort()
  {
    IgnoredErrorResult rv;
    AbortInternal(rv);
    MOZ_ASSERT(!rv.Failed());
  }

  virtual void
  Abort(ErrorResult& aRv) override;

  // response
  virtual void
  GetResponseURL(nsAString& aUrl) override;

  virtual uint32_t
  GetStatus(ErrorResult& aRv) override;

  virtual void
  GetStatusText(nsACString& aStatusText, ErrorResult& aRv) override;

  virtual void
  GetResponseHeader(const nsACString& aHeader, nsACString& aResult,
                    ErrorResult& aRv) override;

  void
  GetResponseHeader(const nsAString& aHeader, nsAString& aResult,
                    ErrorResult& aRv)
  {
    nsAutoCString result;
    GetResponseHeader(NS_ConvertUTF16toUTF8(aHeader), result, aRv);
    if (result.IsVoid()) {
      aResult.SetIsVoid(true);
    }
    else {
      // The result value should be inflated:
      CopyASCIItoUTF16(result, aResult);
    }
  }

  virtual void
  GetAllResponseHeaders(nsACString& aResponseHeaders,
                        ErrorResult& aRv) override;

  bool IsSafeHeader(const nsACString& aHeaderName,
                    NotNull<nsIHttpChannel*> aHttpChannel) const;

  virtual void
  OverrideMimeType(const nsAString& aMimeType, ErrorResult& aRv) override;

  virtual XMLHttpRequestResponseType
  ResponseType() const override
  {
    return XMLHttpRequestResponseType(mResponseType);
  }

  virtual void
  SetResponseType(XMLHttpRequestResponseType aType,
                  ErrorResult& aRv) override;

  virtual void
  GetResponse(JSContext* aCx, JS::MutableHandle<JS::Value> aResponse,
              ErrorResult& aRv) override;

  virtual void
  GetResponseText(DOMString& aResponseText, ErrorResult& aRv) override;

  void
  GetResponseText(XMLHttpRequestStringSnapshot& aSnapshot,
                  ErrorResult& aRv);

  virtual nsIDocument*
  GetResponseXML(ErrorResult& aRv) override;

  virtual bool
  MozBackgroundRequest() const override;

  nsresult
  SetMozBackgroundRequest(bool aMozBackgroundRequest);

  virtual void
  SetMozBackgroundRequest(bool aMozBackgroundRequest, ErrorResult& aRv) override;

  virtual uint16_t
  ErrorCode() const override
  {
    return static_cast<uint16_t>(mErrorLoad);
  }

  virtual bool
  MozAnon() const override;

  virtual bool
  MozSystem() const override;

  virtual nsIChannel*
  GetChannel() const override
  {
    return mChannel;
  }

  // We need a GetInterface callable from JS for chrome JS
  virtual void
  GetInterface(JSContext* aCx, nsIJSID* aIID,
               JS::MutableHandle<JS::Value> aRetval,
               ErrorResult& aRv) override;

  // This fires a trusted readystatechange event, which is not cancelable and
  // doesn't bubble.
  nsresult FireReadystatechangeEvent();
  void DispatchProgressEvent(DOMEventTargetHelper* aTarget,
                             const ProgressEventType aType,
                             int64_t aLoaded, int64_t aTotal);

  void SetRequestObserver(nsIRequestObserver* aObserver);

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(XMLHttpRequestMainThread,
                                                         XMLHttpRequest)
  virtual bool IsCertainlyAliveForCC() const override;

  bool AllowUploadProgress();

  virtual void DisconnectFromOwner() override;

  static void SetDontWarnAboutSyncXHR(bool aVal)
  {
    sDontWarnAboutSyncXHR = aVal;
  }
  static bool DontWarnAboutSyncXHR()
  {
    return sDontWarnAboutSyncXHR;
  }

  virtual void
  SetOriginAttributes(const mozilla::dom::OriginAttributesDictionary& aAttrs) override;

  void BlobStoreCompleted(MutableBlobStorage* aBlobStorage,
                          Blob* aBlob,
                          nsresult aResult) override;

  void
  LocalFileToBlobCompleted(Blob* aBlob);

protected:
  nsresult DetectCharset();
  nsresult AppendToResponseText(const char * aBuffer, uint32_t aBufferLen);
  static nsresult StreamReaderFunc(nsIInputStream* in,
                                   void* closure,
                                   const char* fromRawSegment,
                                   uint32_t toOffset,
                                   uint32_t count,
                                   uint32_t *writeCount);
  nsresult CreateResponseParsedJSON(JSContext* aCx);
  // Change the state of the object with this. The broadcast argument
  // determines if the onreadystatechange listener should be called.
  nsresult ChangeState(uint16_t aState, bool aBroadcast = true);
  already_AddRefed<nsILoadGroup> GetLoadGroup() const;
  nsIURI *GetBaseURI();

  already_AddRefed<nsIHttpChannel> GetCurrentHttpChannel();
  already_AddRefed<nsIJARChannel> GetCurrentJARChannel();

  void TruncateResponseText();

  bool IsSystemXHR() const;
  bool InUploadPhase() const;

  void OnBodyParseEnd();
  void ChangeStateToDone();

  void StartProgressEventTimer();
  void StopProgressEventTimer();

  void MaybeCreateBlobStorage();

  nsresult OnRedirectVerifyCallback(nsresult result);

  void SetTimerEventTarget(nsITimer* aTimer);

  nsresult DispatchToMainThread(already_AddRefed<nsIRunnable> aRunnable);

  void DispatchOrStoreEvent(DOMEventTargetHelper* aTarget, Event* aEvent);

  already_AddRefed<nsXMLHttpRequestXPCOMifier> EnsureXPCOMifier();

  void SuspendEventDispatching();
  void ResumeEventDispatching();

  void
  AbortInternal(ErrorResult& aRv);

  struct PendingEvent
  {
    RefPtr<DOMEventTargetHelper> mTarget;
    RefPtr<Event> mEvent;
  };

  nsTArray<PendingEvent> mPendingEvents;

  nsCOMPtr<nsISupports> mContext;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsIChannel> mChannel;
  nsCString mRequestMethod;
  nsCOMPtr<nsIURI> mRequestURL;
  nsCOMPtr<nsIDocument> mResponseXML;

  nsCOMPtr<nsIStreamListener> mXMLParserStreamListener;

  RefPtr<PerformanceStorage> mPerformanceStorage;

  // used to implement getAllResponseHeaders()
  class nsHeaderVisitor : public nsIHttpHeaderVisitor
  {
    struct HeaderEntry final
    {
      nsCString mName;
      nsCString mValue;

      HeaderEntry(const nsACString& aName, const nsACString& aValue)
        : mName(aName), mValue(aValue)
      {}

      bool
      operator==(const HeaderEntry& aOther) const
      {
        return mName == aOther.mName;
      }

      bool
      operator<(const HeaderEntry& aOther) const
      {
        return mName < aOther.mName;
      }
    };

  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIHTTPHEADERVISITOR
    nsHeaderVisitor(const XMLHttpRequestMainThread& aXMLHttpRequest,
                    NotNull<nsIHttpChannel*> aHttpChannel)
      : mXHR(aXMLHttpRequest), mHttpChannel(aHttpChannel) {}
    const nsACString &Headers()
    {
      for (uint32_t i = 0; i < mHeaderList.Length(); i++) {
        HeaderEntry& header = mHeaderList.ElementAt(i);

        mHeaders.Append(header.mName);
        mHeaders.AppendLiteral(": ");
        mHeaders.Append(header.mValue);
        mHeaders.AppendLiteral("\r\n");
      }
      return mHeaders;
    }

  private:
    virtual ~nsHeaderVisitor() {}

    nsTArray<HeaderEntry> mHeaderList;
    nsCString mHeaders;
    const XMLHttpRequestMainThread& mXHR;
    NotNull<nsCOMPtr<nsIHttpChannel>> mHttpChannel;
  };

  // The bytes of our response body. Only used for DEFAULT, ARRAYBUFFER and
  // BLOB responseTypes
  nsCString mResponseBody;

  // The text version of our response body. This is incrementally decoded into
  // as we receive network data. However for the DEFAULT responseType we
  // lazily decode into this from mResponseBody only when .responseText is
  // accessed.
  // Only used for DEFAULT and TEXT responseTypes.
  XMLHttpRequestString mResponseText;

  // For DEFAULT responseType we use this to keep track of how far we've
  // lazily decoded from mResponseBody to mResponseText
  uint32_t mResponseBodyDecodedPos;

  // Decoder used for decoding into mResponseText
  // Only used for DEFAULT, TEXT and JSON responseTypes.
  // In cases where we've only received half a surrogate, the decoder itself
  // carries the state to remember this. Next time we receive more data we
  // simply feed the new data into the decoder which will handle the second
  // part of the surrogate.
  mozilla::UniquePtr<mozilla::Decoder> mDecoder;

  const Encoding* mResponseCharset;

  void MatchCharsetAndDecoderToResponseDocument();

  XMLHttpRequestResponseType mResponseType;

  // It is either a cached blob-response from the last call to GetResponse,
  // but is also explicitly set in OnStopRequest.
  RefPtr<Blob> mResponseBlob;
  // We stream data to mBlobStorage when response type is "blob".
  RefPtr<MutableBlobStorage> mBlobStorage;

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
  nsCOMPtr<nsILoadGroup> mLoadGroup;

  Maybe<ClientInfo> mClientInfo;
  Maybe<ServiceWorkerDescriptor> mController;

  uint16_t mState;

  bool mFlagSynchronous;
  bool mFlagAborted;
  bool mFlagParseBody;
  bool mFlagSyncLooping;
  bool mFlagBackgroundRequest;
  bool mFlagHadUploadListenersOnSend;
  bool mFlagACwithCredentials;
  bool mFlagTimedOut;
  bool mFlagDeleted;

  // The XHR2 spec's send() flag. Set when the XHR begins uploading, until it
  // finishes downloading (or an error/abort has occurred during either phase).
  // Used to guard against the user trying to alter headers/etc when it's too
  // late, and ensure the XHR only handles one in-flight request at once.
  bool mFlagSend;

  RefPtr<XMLHttpRequestUpload> mUpload;
  int64_t mUploadTransferred;
  int64_t mUploadTotal;
  bool mUploadComplete;
  bool mProgressSinceLastProgressEvent;

  // Timeout support
  PRTime mRequestSentTime;
  uint32_t mTimeoutMilliseconds;
  nsCOMPtr<nsITimer> mTimeoutTimer;
  void StartTimeoutTimer();
  void HandleTimeoutCallback();

  nsCOMPtr<nsIDocument> mSuspendedDoc;
  nsCOMPtr<nsIRunnable> mResumeTimeoutRunnable;

  nsCOMPtr<nsITimer> mSyncTimeoutTimer;

  enum SyncTimeoutType {
    eErrorOrExpired,
    eTimerStarted,
    eNoTimerNeeded
  };

  SyncTimeoutType MaybeStartSyncTimeoutTimer();
  void HandleSyncTimeoutTimer();
  void CancelSyncTimeoutTimer();

  ErrorType mErrorLoad;
  bool mErrorParsingXML;
  bool mWaitingForOnStopRequest;
  bool mProgressTimerIsActive;
  bool mIsHtml;
  bool mWarnAboutSyncHtml;
  int64_t mLoadTotal; // -1 if not known.
  // Number of HTTP message body bytes received so far. This quantity is
  // in the same units as Content-Length and mLoadTotal, and hence counts
  // compressed bytes when the channel has gzip Content-Encoding. If the
  // channel does not have Content-Encoding, this will be the same as
  // mDataReceived except between the OnProgress that changes mLoadTransferred
  // and the corresponding OnDataAvailable (which changes mDataReceived).
  // Ordering of OnProgress and OnDataAvailable is undefined.
  int64_t mLoadTransferred;
  nsCOMPtr<nsITimer> mProgressNotifier;
  void HandleProgressTimerCallback();

  bool mIsSystem;
  bool mIsAnon;

  /**
   * Close the XMLHttpRequest's channels.
   */
  void CloseRequest();

  void TerminateOngoingFetch();

  /**
   * Close the XMLHttpRequest's channels and dispatch appropriate progress
   * events.
   *
   * @param aType The progress event type.
   */
  void CloseRequestWithError(const ProgressEventType aType);

  bool mFirstStartRequestSeen;
  bool mInLoadProgressEvent;

  nsCOMPtr<nsIAsyncVerifyRedirectCallback> mRedirectCallback;
  nsCOMPtr<nsIChannel> mNewRedirectChannel;

  JS::Heap<JS::Value> mResultJSON;

  ArrayBufferBuilder mArrayBufferBuilder;
  JS::Heap<JSObject*> mResultArrayBuffer;
  bool mIsMappedArrayBuffer;

  void ResetResponse();

  bool ShouldBlockAuthPrompt();

  RequestHeaders mAuthorRequestHeaders;

  // Helper object to manage our XPCOM scriptability bits
  nsXMLHttpRequestXPCOMifier* mXPCOMifier;

  // When this is set to true, the event dispatching is suspended. This is
  // useful to change the correct state when XHR is working sync.
  bool mEventDispatchingSuspended;

  // Our parse-end listener, if we are parsing.
  RefPtr<nsXHRParseEndListener> mParseEndListener;

  static bool sDontWarnAboutSyncXHR;
};

class MOZ_STACK_CLASS AutoDontWarnAboutSyncXHR
{
public:
  AutoDontWarnAboutSyncXHR() : mOldVal(XMLHttpRequestMainThread::DontWarnAboutSyncXHR())
  {
    XMLHttpRequestMainThread::SetDontWarnAboutSyncXHR(true);
  }

  ~AutoDontWarnAboutSyncXHR()
  {
    XMLHttpRequestMainThread::SetDontWarnAboutSyncXHR(mOldVal);
  }

private:
  bool mOldVal;
};

// A shim class designed to expose the non-DOM interfaces of
// XMLHttpRequest via XPCOM stuff.
class nsXMLHttpRequestXPCOMifier final : public nsIStreamListener,
                                         public nsIChannelEventSink,
                                         public nsIAsyncVerifyRedirectCallback,
                                         public nsIProgressEventSink,
                                         public nsIInterfaceRequestor,
                                         public nsITimerCallback,
                                         public nsINamed
{
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsXMLHttpRequestXPCOMifier,
                                           nsIStreamListener)

  explicit nsXMLHttpRequestXPCOMifier(XMLHttpRequestMainThread* aXHR) :
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
  NS_FORWARD_NSIASYNCVERIFYREDIRECTCALLBACK(mXHR->)
  NS_FORWARD_NSIPROGRESSEVENTSINK(mXHR->)
  NS_FORWARD_NSITIMERCALLBACK(mXHR->)
  NS_FORWARD_NSINAMED(mXHR->)

  NS_DECL_NSIINTERFACEREQUESTOR

private:
  RefPtr<XMLHttpRequestMainThread> mXHR;
};

class nsXHRParseEndListener : public nsIDOMEventListener
{
public:
  NS_DECL_ISUPPORTS
  NS_IMETHOD HandleEvent(Event *event) override
  {
    if (mXHR) {
      mXHR->OnBodyParseEnd();
    }
    mXHR = nullptr;
    return NS_OK;
  }

  explicit nsXHRParseEndListener(XMLHttpRequestMainThread* aXHR)
    : mXHR(aXHR) {}

  void SetIsStale() {
    mXHR = nullptr;
  }
private:
  virtual ~nsXHRParseEndListener() {}

  XMLHttpRequestMainThread* mXHR;
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_XMLHttpRequestMainThread_h

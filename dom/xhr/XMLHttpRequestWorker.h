/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XMLHttpRequestWorker_h
#define mozilla_dom_XMLHttpRequestWorker_h

#include "XMLHttpRequest.h"
#include "XMLHttpRequestString.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/WorkerHolder.h"

namespace mozilla {
namespace dom {

class Proxy;
class SendRunnable;
class DOMString;
class WorkerPrivate;

class XMLHttpRequestWorker final : public XMLHttpRequest,
                                   public WorkerHolder
{
public:
  struct StateData
  {
    XMLHttpRequestStringSnapshot mResponseText;
    nsString mResponseURL;
    uint32_t mStatus;
    nsCString mStatusText;
    uint16_t mReadyState;
    bool mFlagSend;
    JS::Heap<JS::Value> mResponse;
    nsresult mResponseTextResult;
    nsresult mStatusResult;
    nsresult mResponseResult;

    StateData()
    : mStatus(0), mReadyState(0), mFlagSend(false),
      mResponse(JS::UndefinedValue()), mResponseTextResult(NS_OK),
      mStatusResult(NS_OK), mResponseResult(NS_OK)
    { }

    void trace(JSTracer* trc);
  };

private:
  RefPtr<XMLHttpRequestUpload> mUpload;
  WorkerPrivate* mWorkerPrivate;
  RefPtr<Proxy> mProxy;
  XMLHttpRequestResponseType mResponseType;
  StateData mStateData;

  uint32_t mTimeout;

  bool mRooted;
  bool mBackgroundRequest;
  bool mWithCredentials;
  bool mCanceled;

  bool mMozAnon;
  bool mMozSystem;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(XMLHttpRequestWorker,
                                                         XMLHttpRequest)


  static already_AddRefed<XMLHttpRequest>
  Construct(const GlobalObject& aGlobal,
            const MozXMLHttpRequestParameters& aParams,
            ErrorResult& aRv);

  void
  Unpin();

  bool
  Notify(WorkerStatus aStatus) override;

  virtual uint16_t
  ReadyState() const override
  {
    return mStateData.mReadyState;
  }

  virtual void
  Open(const nsACString& aMethod, const nsAString& aUrl,
       ErrorResult& aRv) override
  {
    Open(aMethod, aUrl, true, Optional<nsAString>(),
         Optional<nsAString>(), aRv);
  }

  virtual void
  Open(const nsACString& aMethod, const nsAString& aUrl, bool aAsync,
       const nsAString& aUsername, const nsAString& aPassword,
       ErrorResult& aRv) override
  {
    Optional<nsAString> username;
    username = &aUsername;
    Optional<nsAString> password;
    password = &aPassword;
    Open(aMethod, aUrl, aAsync, username, password, aRv);
  }

  void
  Open(const nsACString& aMethod, const nsAString& aUrl,
       bool aAsync, const Optional<nsAString>& aUser,
       const Optional<nsAString>& aPassword, ErrorResult& aRv);

  virtual void
  SetRequestHeader(const nsACString& aHeader, const nsACString& aValue,
                   ErrorResult& aRv) override;

  virtual uint32_t
  Timeout() const override
  {
    return mTimeout;
  }

  virtual void
  SetTimeout(uint32_t aTimeout, ErrorResult& aRv) override;

  virtual bool
  WithCredentials() const override
  {
    return mWithCredentials;
  }

  virtual void
  SetWithCredentials(bool aWithCredentials, ErrorResult& aRv) override;

  virtual bool
  MozBackgroundRequest() const override
  {
    return mBackgroundRequest;
  }

  virtual void
  SetMozBackgroundRequest(bool aBackgroundRequest, ErrorResult& aRv) override;

  virtual nsIChannel*
  GetChannel() const override
  {
    MOZ_CRASH("This method cannot be called on workers.");
  }

  virtual XMLHttpRequestUpload*
  GetUpload(ErrorResult& aRv) override;

  virtual void
  Send(JSContext* aCx,
       const Nullable<DocumentOrBlobOrArrayBufferViewOrArrayBufferOrFormDataOrURLSearchParamsOrUSVString>& aData,
       ErrorResult& aRv) override;

  virtual void
  SendInputStream(nsIInputStream* aInputStream, ErrorResult& aRv) override
  {
    MOZ_CRASH("nsIInputStream is not a valid argument for XHR in workers.");
  }

  virtual void
  Abort(ErrorResult& aRv) override;

  virtual void
  GetResponseURL(nsAString& aUrl) override
  {
    aUrl = mStateData.mResponseURL;
  }

  uint32_t
  GetStatus(ErrorResult& aRv) override
  {
    aRv = mStateData.mStatusResult;
    return mStateData.mStatus;
  }

  virtual void
  GetStatusText(nsACString& aStatusText, ErrorResult& aRv) override
  {
    aStatusText = mStateData.mStatusText;
  }

  virtual void
  GetResponseHeader(const nsACString& aHeader, nsACString& aResponseHeader,
                    ErrorResult& aRv) override;

  virtual void
  GetAllResponseHeaders(nsACString& aResponseHeaders,
                        ErrorResult& aRv) override;

  virtual void
  OverrideMimeType(const nsAString& aMimeType, ErrorResult& aRv) override;

  virtual XMLHttpRequestResponseType
  ResponseType() const override
  {
    return mResponseType;
  }

  virtual void
  SetResponseType(XMLHttpRequestResponseType aResponseType,
                  ErrorResult& aRv) override;

  virtual void
  GetResponse(JSContext* /* unused */, JS::MutableHandle<JS::Value> aResponse,
              ErrorResult& aRv) override;

  virtual void
  GetResponseText(DOMString& aResponseText, ErrorResult& aRv) override;

  virtual nsIDocument*
  GetResponseXML(ErrorResult& aRv) override
  {
    MOZ_CRASH("This method should not be called.");
  }

  virtual void
  GetInterface(JSContext* aCx, nsIJSID* aIID,
               JS::MutableHandle<JS::Value> aRetval,
               ErrorResult& aRv) override
  {
    aRv.Throw(NS_ERROR_FAILURE);
  }

  virtual void
  SetOriginAttributes(const mozilla::dom::OriginAttributesDictionary& aAttrs) override
  {
    MOZ_CRASH("This method cannot be called on workers.");
  }

  XMLHttpRequestUpload*
  GetUploadObjectNoCreate() const
  {
    return mUpload;
  }

  void
  UpdateState(const StateData& aStateData, bool aUseCachedArrayBufferResponse);

  void
  NullResponseText()
  {
    mStateData.mResponseText.SetVoid();
    mStateData.mResponse.setNull();
  }

  virtual uint16_t ErrorCode() const override
  {
    return 0; // eOK
  }

  virtual bool MozAnon() const override
  {
    return mMozAnon;
  }

  virtual bool MozSystem() const override
  {
    return mMozSystem;
  }

  bool
  SendInProgress() const
  {
    return mRooted;
  }

private:
  explicit XMLHttpRequestWorker(WorkerPrivate* aWorkerPrivate);
  ~XMLHttpRequestWorker();

  enum ReleaseType { Default, XHRIsGoingAway, WorkerIsGoingAway };

  void
  ReleaseProxy(ReleaseType aType = Default);

  void
  MaybePin(ErrorResult& aRv);

  void
  MaybeDispatchPrematureAbortEvents(ErrorResult& aRv);

  void
  DispatchPrematureAbortEvent(EventTarget* aTarget,
                              const nsAString& aEventType, bool aUploadTarget,
                              ErrorResult& aRv);

  void
  Send(JSContext* aCx, JS::Handle<JSObject*> aBody, ErrorResult& aRv);

  void
  SendInternal(SendRunnable* aRunnable,
               ErrorResult& aRv);
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_workers_xmlhttprequest_h__

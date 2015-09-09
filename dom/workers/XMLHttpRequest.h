/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_xmlhttprequest_h__
#define mozilla_dom_workers_xmlhttprequest_h__

#include "mozilla/dom/workers/bindings/WorkerFeature.h"

// Need this for XMLHttpRequestResponseType.
#include "mozilla/dom/XMLHttpRequestBinding.h"

#include "mozilla/dom/TypedArray.h"

#include "js/StructuredClone.h"
#include "nsXMLHttpRequest.h"

namespace mozilla {
namespace dom {
class Blob;
}
}

BEGIN_WORKERS_NAMESPACE

class Proxy;
class XMLHttpRequestUpload;
class WorkerPrivate;
class WorkerStructuredCloneClosure;

class XMLHttpRequest final: public nsXHREventTarget,
                            public WorkerFeature
{
public:
  struct StateData
  {
    nsString mResponseText;
    nsString mResponseURL;
    uint32_t mStatus;
    nsCString mStatusText;
    uint16_t mReadyState;
    JS::Heap<JS::Value> mResponse;
    nsresult mResponseTextResult;
    nsresult mStatusResult;
    nsresult mResponseResult;

    StateData()
    : mStatus(0), mReadyState(0), mResponse(JS::UndefinedValue()),
      mResponseTextResult(NS_OK), mStatusResult(NS_OK),
      mResponseResult(NS_OK)
    { }
  };

private:
  nsRefPtr<XMLHttpRequestUpload> mUpload;
  WorkerPrivate* mWorkerPrivate;
  nsRefPtr<Proxy> mProxy;
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
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(XMLHttpRequest,
                                                         nsXHREventTarget)

  nsISupports*
  GetParentObject() const
  {
    // There's only one global on a worker, so we don't need to specify.
    return nullptr;
  }

  static already_AddRefed<XMLHttpRequest>
  Constructor(const GlobalObject& aGlobal,
              const MozXMLHttpRequestParameters& aParams,
              ErrorResult& aRv);

  static already_AddRefed<XMLHttpRequest>
  Constructor(const GlobalObject& aGlobal, const nsAString& ignored,
              ErrorResult& aRv)
  {
    // Pretend like someone passed null, so we can pick up the default values
    MozXMLHttpRequestParameters params;
    if (!params.Init(aGlobal.Context(), JS::NullHandleValue)) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    return Constructor(aGlobal, params, aRv);
  }

  void
  Unpin();

  bool
  Notify(JSContext* aCx, Status aStatus) override;

  IMPL_EVENT_HANDLER(readystatechange)

  uint16_t
  ReadyState() const
  {
    return mStateData.mReadyState;
  }

  void Open(const nsACString& aMethod, const nsAString& aUrl, ErrorResult& aRv)
  {
    Open(aMethod, aUrl, true, Optional<nsAString>(),
         Optional<nsAString>(), aRv);
  }
  void
  Open(const nsACString& aMethod, const nsAString& aUrl, bool aAsync,
       const Optional<nsAString>& aUser, const Optional<nsAString>& aPassword,
       ErrorResult& aRv);

  void
  SetRequestHeader(const nsACString& aHeader, const nsACString& aValue,
                   ErrorResult& aRv);

  uint32_t
  Timeout() const
  {
    return mTimeout;
  }

  void
  SetTimeout(uint32_t aTimeout, ErrorResult& aRv);

  bool
  WithCredentials() const
  {
    return mWithCredentials;
  }

  void
  SetWithCredentials(bool aWithCredentials, ErrorResult& aRv);

  bool
  MozBackgroundRequest() const
  {
    return mBackgroundRequest;
  }

  void
  SetMozBackgroundRequest(bool aBackgroundRequest, ErrorResult& aRv);

  XMLHttpRequestUpload*
  GetUpload(ErrorResult& aRv);

  void
  Send(ErrorResult& aRv);

  void
  Send(const nsAString& aBody, ErrorResult& aRv);

  void
  Send(JS::Handle<JSObject*> aBody, ErrorResult& aRv);

  void
  Send(Blob& aBody, ErrorResult& aRv);

  void
  Send(nsFormData& aBody, ErrorResult& aRv);

  void
  Send(const ArrayBuffer& aBody, ErrorResult& aRv);

  void
  Send(const ArrayBufferView& aBody, ErrorResult& aRv);

  void
  Abort(ErrorResult& aRv);

  void
  GetResponseURL(nsAString& aUrl) const
  {
    aUrl = mStateData.mResponseURL;
  }

  uint16_t
  GetStatus(ErrorResult& aRv) const
  {
    aRv = mStateData.mStatusResult;
    return mStateData.mStatus;
  }

  void
  GetStatusText(nsACString& aStatusText) const
  {
    aStatusText = mStateData.mStatusText;
  }

  void
  GetResponseHeader(const nsACString& aHeader, nsACString& aResponseHeader,
                    ErrorResult& aRv);

  void
  GetAllResponseHeaders(nsACString& aResponseHeaders, ErrorResult& aRv);

  void
  OverrideMimeType(const nsAString& aMimeType, ErrorResult& aRv);

  XMLHttpRequestResponseType
  ResponseType() const
  {
    return mResponseType;
  }

  void
  SetResponseType(XMLHttpRequestResponseType aResponseType, ErrorResult& aRv);

  void
  GetResponse(JSContext* /* unused */, JS::MutableHandle<JS::Value> aResponse,
              ErrorResult& aRv);

  void
  GetResponseText(nsAString& aResponseText, ErrorResult& aRv);

  void
  GetInterface(JSContext* cx, JS::Handle<JSObject*> aIID,
               JS::MutableHandle<JS::Value> aRetval, ErrorResult& aRv)
  {
    aRv.Throw(NS_ERROR_FAILURE);
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
    mStateData.mResponseText.SetIsVoid(true);
    mStateData.mResponse = JSVAL_NULL;
  }

  bool MozAnon() const
  {
    return mMozAnon;
  }

  bool MozSystem() const
  {
    return mMozSystem;
  }

  bool
  SendInProgress() const
  {
    return mRooted;
  }

private:
  explicit XMLHttpRequest(WorkerPrivate* aWorkerPrivate);
  ~XMLHttpRequest();

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
  SendInternal(const nsAString& aStringBody,
               JSAutoStructuredCloneBuffer&& aBody,
               WorkerStructuredCloneClosure& aClosure,
               ErrorResult& aRv);
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_xmlhttprequest_h__

/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_xmlhttprequest_h__
#define mozilla_dom_workers_xmlhttprequest_h__

#include "mozilla/dom/workers/bindings/XMLHttpRequestEventTarget.h"
#include "mozilla/dom/workers/bindings/WorkerFeature.h"

// Need this for XMLHttpRequestResponseType.
#include "mozilla/dom/bindings/XMLHttpRequestBinding.h"

BEGIN_WORKERS_NAMESPACE

class Proxy;
class XMLHttpRequestUpload;
class WorkerPrivate;

typedef mozilla::dom::bindings::prototypes::XMLHttpRequestResponseType::value
        XMLHttpRequestResponseType;

class XMLHttpRequest : public XMLHttpRequestEventTarget,
                       public WorkerFeature
{
public:
  struct StateData
  {
    nsString mResponseText;
    uint32_t mStatus;
    nsString mStatusText;
    uint16_t mReadyState;
    jsval mResponse;
    nsresult mResponseTextResult;
    nsresult mStatusResult;
    nsresult mResponseResult;

    StateData()
    : mStatus(0), mReadyState(0), mResponse(JSVAL_VOID),
      mResponseTextResult(NS_OK), mStatusResult(NS_OK),
      mResponseResult(NS_OK)
    { }
  };

private:
  JSObject* mJSObject;
  XMLHttpRequestUpload* mUpload;
  WorkerPrivate* mWorkerPrivate;
  nsRefPtr<Proxy> mProxy;
  XMLHttpRequestResponseType mResponseType;
  StateData mStateData;

  uint32_t mTimeout;

  bool mJSObjectRooted;
  bool mMultipart;
  bool mBackgroundRequest;
  bool mWithCredentials;
  bool mCanceled;

protected:
  XMLHttpRequest(JSContext* aCx, WorkerPrivate* aWorkerPrivate);
  virtual ~XMLHttpRequest();

public:
  virtual void
  _Trace(JSTracer* aTrc) MOZ_OVERRIDE;

  virtual void
  _Finalize(JSFreeOp* aFop) MOZ_OVERRIDE;

  static XMLHttpRequest*
  _Constructor(JSContext* aCx, JSObject* aGlobal, nsresult& aRv);

  void
  Unpin();

  bool
  Notify(JSContext* aCx, Status aStatus) MOZ_OVERRIDE;

#define IMPL_GETTER_AND_SETTER(_type)                                          \
  JSObject*                                                                    \
  GetOn##_type(nsresult& aRv)                                                  \
  {                                                                            \
    return GetEventListener(NS_LITERAL_STRING(#_type), aRv);                   \
  }                                                                            \
                                                                               \
  void                                                                         \
  SetOn##_type(JSObject* aListener, nsresult& aRv)                             \
  {                                                                            \
    SetEventListener(NS_LITERAL_STRING(#_type), aListener, aRv);               \
  }

  IMPL_GETTER_AND_SETTER(readystatechange)

#undef IMPL_GETTER_AND_SETTER

  uint16_t
  GetReadyState() const
  {
    return mStateData.mReadyState;
  }

  void
  Open(const nsAString& aMethod, const nsAString& aUrl, bool aAsync,
       const nsAString& aUser, const nsAString& aPassword, nsresult& aRv);

  void
  SetRequestHeader(const nsAString& aHeader, const nsAString& aValue,
                   nsresult& aRv);

  uint32_t
  GetTimeout(nsresult& aRv) const
  {
    return mTimeout;
  }

  void
  SetTimeout(uint32_t aTimeout, nsresult& aRv);

  bool
  GetWithCredentials(nsresult& aRv) const
  {
    return mWithCredentials;
  }

  void
  SetWithCredentials(bool aWithCredentials, nsresult& aRv);

  bool
  GetMultipart(nsresult& aRv) const
  {
    return mMultipart;
  }

  void
  SetMultipart(bool aMultipart, nsresult& aRv);

  bool
  GetMozBackgroundRequest(nsresult& aRv) const
  {
    return mBackgroundRequest;
  }

  void
  SetMozBackgroundRequest(bool aBackgroundRequest, nsresult& aRv);

  XMLHttpRequestUpload*
  GetUpload(nsresult& aRv);

  void
  Send(nsresult& aRv);

  void
  Send(const nsAString& aBody, nsresult& aRv);

  void
  Send(JSObject* aBody, nsresult& aRv);

  void
  SendAsBinary(const nsAString& aBody, nsresult& aRv);

  void
  Abort(nsresult& aRv);

  uint16_t
  GetStatus(nsresult& aRv) const
  {
    aRv = mStateData.mStatusResult;
    return mStateData.mStatus;
  }

  void
  GetStatusText(nsAString& aStatusText) const
  {
    aStatusText = mStateData.mStatusText;
  }

  void
  GetResponseHeader(const nsAString& aHeader, nsAString& aResponseHeader,
                    nsresult& aRv);

  void
  GetAllResponseHeaders(nsAString& aResponseHeaders, nsresult& aRv);

  void
  OverrideMimeType(const nsAString& aMimeType, nsresult& aRv);

  XMLHttpRequestResponseType
  GetResponseType(nsresult& aRv) const
  {
    return mResponseType;
  }

  void
  SetResponseType(XMLHttpRequestResponseType aResponseType, nsresult& aRv);

  jsval
  GetResponse(nsresult& aRv);

  void
  GetResponseText(nsAString& aResponseText, nsresult& aRv);

  JSObject*
  GetResponseXML(nsresult& aRv) const
  {
    return NULL;
  }

  JSObject*
  GetChannel(nsresult& aRv) const
  {
    return NULL;
  }

  JS::Value
  GetInterface(JSObject* aIID, nsresult& aRv)
  {
    aRv = NS_ERROR_FAILURE;
    return JSVAL_NULL;
  }

  XMLHttpRequestUpload*
  GetUploadObjectNoCreate() const
  {
    return mUpload;
  }

  void
  UpdateState(const StateData& aStateData)
  {
    mStateData = aStateData;
  }

  void
  NullResponseText()
  {
    mStateData.mResponseText.SetIsVoid(true);
    mStateData.mResponse = JSVAL_NULL;
  }

private:
  enum ReleaseType { Default, XHRIsGoingAway, WorkerIsGoingAway };

  void
  ReleaseProxy(ReleaseType aType = Default);

  void
  MaybePin(nsresult& aRv);

  void
  MaybeDispatchPrematureAbortEvents(nsresult& aRv);

  void
  DispatchPrematureAbortEvent(JSObject* aTarget, uint8_t aEventType,
                              bool aUploadTarget, nsresult& aRv);

  bool
  SendInProgress() const
  {
    return mJSObjectRooted;
  }

  void
  SendInternal(const nsAString& aStringBody,
               JSAutoStructuredCloneBuffer& aBody,
               nsTArray<nsCOMPtr<nsISupports> >& aClonedObjects,
               nsresult& aRv);
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_xmlhttprequest_h__

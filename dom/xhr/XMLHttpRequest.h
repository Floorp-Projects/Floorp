/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XMLHttpRequest_h
#define mozilla_dom_XMLHttpRequest_h

#include <iterator>  // std::begin, std::end
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/XMLHttpRequestEventTarget.h"
#include "mozilla/dom/XMLHttpRequestBinding.h"

class nsIInputStream;

namespace mozilla::dom {

class Blob;
class DOMString;
class FormData;
class URLSearchParams;
class XMLHttpRequestUpload;

class XMLHttpRequest : public XMLHttpRequestEventTarget {
 public:
  struct EventType {
    const char* cStr;
    const char16_t* str;

    constexpr EventType(const char* name, const char16_t* uname)
        : cStr(name), str(uname) {}

    constexpr EventType(const EventType& other)
        : cStr(other.cStr), str(other.str) {}

    operator const nsDependentString() const { return nsDependentString(str); }

    friend bool operator==(const EventType& a, const EventType& b) {
      return !strcmp(a.cStr, b.cStr);
    }

    friend bool operator!=(const EventType& a, const EventType& b) {
      return strcmp(a.cStr, b.cStr);
    }

    friend bool operator==(const nsAString& a, const EventType& b) {
      return a.Equals(b.str);
    }

    friend bool operator!=(const nsAString& a, const EventType& b) {
      return !a.Equals(b.str);
    }

    inline bool operator==(const nsString& b) const { return b.Equals(str); }

    inline bool operator!=(const nsString& b) const { return !b.Equals(str); }
  };

  struct ProgressEventType : public EventType {
    constexpr ProgressEventType(const char* name, const char16_t* uname)
        : EventType(name, uname) {}
  };

  struct ErrorProgressEventType : public ProgressEventType {
    const nsresult errorCode;
    constexpr ErrorProgressEventType(const char* name, const char16_t* uname,
                                     const nsresult code)
        : ProgressEventType(name, uname), errorCode(code) {}
  };

#define DECL_EVENT(NAME) static constexpr EventType NAME{#NAME, u## #NAME};

#define DECL_PROGRESSEVENT(NAME) \
  static constexpr ProgressEventType NAME { #NAME, u## #NAME }

#define DECL_ERRORPROGRESSEVENT(NAME, ERR) \
  static constexpr ErrorProgressEventType NAME{#NAME, u## #NAME, ERR};

  struct Events;

  static already_AddRefed<XMLHttpRequest> Constructor(
      const GlobalObject& aGlobal, const MozXMLHttpRequestParameters& aParams,
      ErrorResult& aRv);

  static already_AddRefed<XMLHttpRequest> Constructor(
      const GlobalObject& aGlobal, const nsAString& ignored, ErrorResult& aRv) {
    // Pretend like someone passed null, so we can pick up the default values
    MozXMLHttpRequestParameters params;
    if (!params.Init(aGlobal.Context(), JS::NullHandleValue)) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    return Constructor(aGlobal, params, aRv);
  }

  IMPL_EVENT_HANDLER(readystatechange)

  virtual uint16_t ReadyState() const = 0;

  virtual void Open(const nsACString& aMethod, const nsAString& aUrl,
                    ErrorResult& aRv) = 0;

  virtual void Open(const nsACString& aMethod, const nsAString& aUrl,
                    bool aAsync, const nsAString& aUser,
                    const nsAString& aPassword, ErrorResult& aRv) = 0;

  virtual void SetRequestHeader(const nsACString& aHeader,
                                const nsACString& aValue, ErrorResult& aRv) = 0;

  virtual uint32_t Timeout() const = 0;

  virtual void SetTimeout(uint32_t aTimeout, ErrorResult& aRv) = 0;

  virtual bool WithCredentials() const = 0;

  virtual void SetWithCredentials(bool aWithCredentials, ErrorResult& aRv) = 0;

  virtual XMLHttpRequestUpload* GetUpload(ErrorResult& aRv) = 0;

  virtual void Send(
      const Nullable<
          DocumentOrBlobOrArrayBufferViewOrArrayBufferOrFormDataOrURLSearchParamsOrUSVString>&
          aData,
      ErrorResult& aRv) = 0;

  virtual void SendInputStream(nsIInputStream* aInputStream,
                               ErrorResult& aRv) = 0;

  virtual void Abort(ErrorResult& aRv) = 0;

  virtual void GetResponseURL(nsAString& aUrl) = 0;

  virtual uint32_t GetStatus(ErrorResult& aRv) = 0;

  virtual void GetStatusText(nsACString& aStatusText, ErrorResult& aRv) = 0;

  virtual void GetResponseHeader(const nsACString& aHeader, nsACString& aResult,
                                 ErrorResult& aRv) = 0;

  virtual void GetAllResponseHeaders(nsACString& aResponseHeaders,
                                     ErrorResult& aRv) = 0;

  virtual void OverrideMimeType(const nsAString& aMimeType,
                                ErrorResult& aRv) = 0;

  virtual XMLHttpRequestResponseType ResponseType() const = 0;

  virtual void SetResponseType(XMLHttpRequestResponseType aType,
                               ErrorResult& aRv) = 0;

  virtual void GetResponse(JSContext* aCx,
                           JS::MutableHandle<JS::Value> aResponse,
                           ErrorResult& aRv) = 0;

  virtual void GetResponseText(DOMString& aResponseText, ErrorResult& aRv) = 0;

  virtual Document* GetResponseXML(ErrorResult& aRv) = 0;

  virtual bool MozBackgroundRequest() const = 0;

  virtual void SetMozBackgroundRequest(bool aMozBackgroundRequest,
                                       ErrorResult& aRv) = 0;

  virtual nsIChannel* GetChannel() const = 0;

  // We need a GetInterface callable from JS for chrome JS
  virtual void GetInterface(JSContext* aCx, JS::Handle<JS::Value> aIID,
                            JS::MutableHandle<JS::Value> aRetval,
                            ErrorResult& aRv) = 0;

  virtual void SetOriginAttributes(
      const mozilla::dom::OriginAttributesDictionary& aAttrs) = 0;

  virtual uint16_t ErrorCode() const = 0;

  virtual bool MozAnon() const = 0;

  virtual bool MozSystem() const = 0;

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override {
    return mozilla::dom::XMLHttpRequest_Binding::Wrap(aCx, this, aGivenProto);
  }

 protected:
  explicit XMLHttpRequest(nsIGlobalObject* aGlobalObject)
      : XMLHttpRequestEventTarget(aGlobalObject) {}
};

struct XMLHttpRequest::Events {
  DECL_EVENT(readystatechange);
  DECL_PROGRESSEVENT(loadstart);
  DECL_PROGRESSEVENT(progress);
  DECL_ERRORPROGRESSEVENT(error, NS_ERROR_DOM_NETWORK_ERR);
  DECL_ERRORPROGRESSEVENT(abort, NS_ERROR_DOM_ABORT_ERR);
  DECL_ERRORPROGRESSEVENT(timeout, NS_ERROR_DOM_TIMEOUT_ERR);
  DECL_PROGRESSEVENT(load);
  DECL_PROGRESSEVENT(loadend);

  static inline const EventType* All[]{
      &readystatechange, &loadstart, &progress, &error, &abort,
      &timeout,          &load,      &loadend};

  static inline const EventType* ProgressEvents[]{
      &loadstart, &progress, &error, &abort, &timeout, &load, &loadend};

  static inline const EventType* Find(const nsString& name) {
    for (const EventType* type : Events::All) {
      if (*type == name) {
        return type;
      }
    }
    return nullptr;
  }
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_XMLHttpRequest_h

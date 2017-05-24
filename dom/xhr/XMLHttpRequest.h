/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XMLHttpRequest_h
#define mozilla_dom_XMLHttpRequest_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/XMLHttpRequestEventTarget.h"
#include "mozilla/dom/XMLHttpRequestBinding.h"
#include "nsIXMLHttpRequest.h"

class nsIJSID;

namespace mozilla {
namespace dom {

class Blob;
class DOMString;
class FormData;
class URLSearchParams;
class XMLHttpRequestUpload;

class XMLHttpRequest : public XMLHttpRequestEventTarget
{
public:
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

  IMPL_EVENT_HANDLER(readystatechange)

  virtual uint16_t
  ReadyState() const = 0;

  virtual void
  Open(const nsACString& aMethod, const nsAString& aUrl, ErrorResult& aRv) = 0;

  virtual void
  Open(const nsACString& aMethod, const nsAString& aUrl, bool aAsync,
       const nsAString& aUser, const nsAString& aPassword, ErrorResult& aRv) = 0;

  virtual void
  SetRequestHeader(const nsACString& aHeader, const nsACString& aValue,
                   ErrorResult& aRv) = 0;

  virtual uint32_t
  Timeout() const = 0;

  virtual void
  SetTimeout(uint32_t aTimeout, ErrorResult& aRv) = 0;

  virtual bool
  WithCredentials() const = 0;

  virtual void
  SetWithCredentials(bool aWithCredentials, ErrorResult& aRv) = 0;

  virtual XMLHttpRequestUpload*
  GetUpload(ErrorResult& aRv) = 0;

  virtual void
  Send(JSContext* aCx, ErrorResult& aRv) = 0;

  virtual void
  Send(JSContext* aCx, const ArrayBuffer& aArrayBuffer, ErrorResult& aRv) = 0;

  virtual void
  Send(JSContext* aCx, const ArrayBufferView& aArrayBufferView,
       ErrorResult& aRv) = 0;

  virtual void
  Send(JSContext* aCx, Blob& aBlob, ErrorResult& aRv) = 0;

  virtual void
  Send(JSContext* aCx, URLSearchParams& aURLSearchParams, ErrorResult& aRv) = 0;

  virtual void
  Send(JSContext* aCx, nsIDocument& aDoc, ErrorResult& aRv) = 0;

  virtual void
  Send(JSContext* aCx, const nsAString& aString, ErrorResult& aRv) = 0;

  virtual void
  Send(JSContext* aCx, FormData& aFormData, ErrorResult& aRv) = 0;

  virtual void
  Send(JSContext* aCx, nsIInputStream* aStream, ErrorResult& aRv) = 0;

  virtual void
  Abort(ErrorResult& aRv) = 0;

  virtual void
  GetResponseURL(nsAString& aUrl) = 0;

  virtual uint32_t
  GetStatus(ErrorResult& aRv) = 0;

  virtual void
  GetStatusText(nsACString& aStatusText, ErrorResult& aRv) = 0;

  virtual void
  GetResponseHeader(const nsACString& aHeader, nsACString& aResult,
                    ErrorResult& aRv) = 0;

  virtual void
  GetAllResponseHeaders(nsACString& aResponseHeaders,
                        ErrorResult& aRv) = 0;

  virtual void
  OverrideMimeType(const nsAString& aMimeType, ErrorResult& aRv) = 0;

  virtual XMLHttpRequestResponseType
  ResponseType() const = 0;

  virtual void
  SetResponseType(XMLHttpRequestResponseType aType,
                  ErrorResult& aRv) = 0;

  virtual void
  GetResponse(JSContext* aCx, JS::MutableHandle<JS::Value> aResponse,
              ErrorResult& aRv) = 0;

  virtual void
  GetResponseText(DOMString& aResponseText, ErrorResult& aRv) = 0;

  virtual nsIDocument*
  GetResponseXML(ErrorResult& aRv) = 0;

  virtual bool
  MozBackgroundRequest() const = 0;

  virtual void
  SetMozBackgroundRequest(bool aMozBackgroundRequest, ErrorResult& aRv) = 0;

  virtual nsIChannel*
  GetChannel() const = 0;

  virtual void
  GetNetworkInterfaceId(nsACString& aId) const = 0;

  virtual void
  SetNetworkInterfaceId(const nsACString& aId) = 0;

  // We need a GetInterface callable from JS for chrome JS
  virtual void
  GetInterface(JSContext* aCx, nsIJSID* aIID,
               JS::MutableHandle<JS::Value> aRetval,
               ErrorResult& aRv) = 0;

  virtual void
  SetOriginAttributes(const mozilla::dom::OriginAttributesDictionary& aAttrs) = 0;

  virtual uint16_t
  ErrorCode() const = 0;

  virtual bool
  MozAnon() const = 0;

  virtual bool
  MozSystem() const = 0;

  virtual JSObject*
  WrapObject(JSContext *aCx, JS::Handle<JSObject*> aGivenProto) override
  {
    return mozilla::dom::XMLHttpRequestBinding::Wrap(aCx, this, aGivenProto);
  }
};

} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_XMLHttpRequest_h

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_WEBTRANSPORT_API_WEBTRANSPORTERROR__H_
#define DOM_WEBTRANSPORT_API_WEBTRANSPORTERROR__H_

#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/WebTransportErrorBinding.h"

namespace mozilla::dom {
class WebTransportError final : public DOMException {
 public:
  explicit WebTransportError(
      const nsACString& aMessage,
      WebTransportErrorSource aSource = WebTransportErrorSource::Stream,
      Nullable<uint8_t> aCode = Nullable<uint8_t>())
      : DOMException(NS_OK, aMessage, "WebTransportError"_ns, 0),
        mStreamErrorCode(aCode),
        mSource(aSource) {}

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<WebTransportError> Constructor(
      const GlobalObject& aGlobal, const WebTransportErrorInit& aInit);

  WebTransportErrorSource Source() { return mSource; }
  Nullable<uint8_t> GetStreamErrorCode() const { return mStreamErrorCode; }

 private:
  Nullable<uint8_t> mStreamErrorCode;
  const WebTransportErrorSource mSource;
};

}  // namespace mozilla::dom

#endif

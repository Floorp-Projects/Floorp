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
 protected:
  explicit WebTransportError(const nsACString& aMessage)
      : DOMException(NS_OK, aMessage, "stream"_ns, 0) {}

 public:
  static already_AddRefed<WebTransportError> Constructor(
      const GlobalObject& aGlobal, const WebTransportErrorInit& aInit);

  WebTransportErrorSource Source();
  Nullable<uint8_t> GetStreamErrorCode() const {
    return Nullable<uint8_t>(mStreamErrorCode);
  }

 private:
  uint8_t mStreamErrorCode = 0;
};

}  // namespace mozilla::dom

#endif

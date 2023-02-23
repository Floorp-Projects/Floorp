/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebTransportError.h"

namespace mozilla::dom {

JSObject* WebTransportError::WrapObject(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  return WebTransportError_Binding::Wrap(aCx, this, aGivenProto);
}

/* static */
already_AddRefed<WebTransportError> WebTransportError::Constructor(
    const GlobalObject& aGlobal, const WebTransportErrorInit& aInit) {
  //  https://w3c.github.io/webtransport/#web-transport-error-constructor1

  // Step 2: Let message be init.message if it exists, and "" otherwise.
  nsCString message(""_ns);
  if (aInit.mMessage.WasPassed()) {
    CopyUTF16toUTF8(aInit.mMessage.Value(), message);
  }

  // Step 1: Let error be this.
  // Step 3: Set up error with message and "stream".
  RefPtr<WebTransportError> error(new WebTransportError(message));

  // Step 4: Set error.[[StreamErrorCode]] to init.streamErrorCode if it exists.
  if (aInit.mStreamErrorCode.WasPassed()) {
    error->mStreamErrorCode = Nullable(aInit.mStreamErrorCode.Value());
  }
  return error.forget();
}

}  // namespace mozilla::dom

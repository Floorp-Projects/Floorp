/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebTransportError.h"

namespace mozilla::dom {

/* static */
already_AddRefed<WebTransportError> WebTransportError::Constructor(
    const GlobalObject& aGlobal, const WebTransportErrorInit& aInit) {
  nsCString message(""_ns);
  if (aInit.mMessage.WasPassed()) {
    CopyUTF16toUTF8(aInit.mMessage.Value(), message);
  }
  RefPtr<WebTransportError> error(new WebTransportError(message));
  if (aInit.mStreamErrorCode.WasPassed()) {
    error->mStreamErrorCode = aInit.mStreamErrorCode.Value();
  }
  return error.forget();
}

}  // namespace mozilla::dom

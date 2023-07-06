/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Error.h"

namespace mozilla::webgpu {

GPU_IMPL_CYCLE_COLLECTION(Error, mGlobal)

Error::Error(nsIGlobalObject* const aGlobal, const nsACString& aMessage)
    : mGlobal(aGlobal) {
  CopyUTF8toUTF16(aMessage, mMessage);
}

Error::Error(nsIGlobalObject* const aGlobal, const nsAString& aMessage)
    : mGlobal(aGlobal), mMessage(aMessage) {}

}  // namespace mozilla::webgpu

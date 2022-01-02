/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ValidationError.h"
#include "Device.h"
#include "mozilla/dom/WebGPUBinding.h"

namespace mozilla {
namespace webgpu {

GPU_IMPL_CYCLE_COLLECTION(ValidationError, mParent)
GPU_IMPL_JS_WRAP(ValidationError)

ValidationError::ValidationError(Device* aParent, const nsACString& aMessage)
    : ChildOf(aParent), mMessage(aMessage) {}

ValidationError::~ValidationError() = default;

already_AddRefed<ValidationError> ValidationError::Constructor(
    const dom::GlobalObject& aGlobal, const nsAString& aString) {
  MOZ_CRASH("TODO");
}

void ValidationError::GetMessage(nsAString& aMessage) const {
  CopyUTF8toUTF16(mMessage, aMessage);
}

}  // namespace webgpu
}  // namespace mozilla

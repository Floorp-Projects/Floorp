/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OutOfMemoryError.h"
#include "mozilla/dom/WebGPUBinding.h"

namespace mozilla::webgpu {

GPU_IMPL_JS_WRAP(OutOfMemoryError)

already_AddRefed<OutOfMemoryError> OutOfMemoryError::Constructor(
    const dom::GlobalObject& aGlobal, const nsAString& aString,
    ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  MOZ_RELEASE_ASSERT(global);
  return MakeAndAddRef<OutOfMemoryError>(global, aString);
}

}  // namespace mozilla::webgpu

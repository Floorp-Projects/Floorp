/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGPU_TYPES_H_
#define WEBGPU_TYPES_H_

#include <cstdint>
#include "mozilla/Maybe.h"
#include "nsString.h"

namespace mozilla::webgpu {

using RawId = uint64_t;
using BufferAddress = uint64_t;

struct ScopedError {
  // Did an error occur as a result the attempt to retrieve an error
  // (e.g. from a dead device, from an empty scope stack)?
  bool operationError = false;

  // If non-empty, the first error generated when this scope was on
  // the top of the stack. This is interpreted as UTF-8.
  nsCString validationMessage;
};
using MaybeScopedError = Maybe<ScopedError>;

}  // namespace mozilla::webgpu

#endif  // WEBGPU_TYPES_H_

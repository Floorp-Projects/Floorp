/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGPU_TYPES_H_
#define WEBGPU_TYPES_H_

#include <cstdint>
#include "mozilla/Maybe.h"

namespace mozilla {
namespace webgpu {

typedef uint64_t RawId;
typedef uint64_t BufferAddress;

struct ScopedError {
  bool operationError = false;
  nsCString validationMessage;
};
using MaybeScopedError = Maybe<ScopedError>;

}  // namespace webgpu
}  // namespace mozilla

#endif  // WEBGPU_TYPES_H_

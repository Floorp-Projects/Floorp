/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGPU_TYPES_H_
#define WEBGPU_TYPES_H_

#include <cstdint>
#include "mozilla/Maybe.h"
#include "nsString.h"
#include "mozilla/dom/BindingDeclarations.h"

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

enum class WebGPUCompilationMessageType { Error, Warning, Info };

// TODO: Better name? CompilationMessage alread taken by the dom object.
/// The serializable counterpart of the dom object CompilationMessage.
struct WebGPUCompilationMessage {
  nsString message;
  uint64_t lineNum = 0;
  uint64_t linePos = 0;
  // In utf16 code units.
  uint64_t offset = 0;
  // In utf16 code units.
  uint64_t length = 0;
  WebGPUCompilationMessageType messageType =
      WebGPUCompilationMessageType::Error;
};

/// A helper to reduce the boiler plate of turning the many Optional<nsAString>
/// we get from the dom to the nullable nsACString* we pass to the wgpu ffi.
class StringHelper {
 public:
  explicit StringHelper(const dom::Optional<nsString>& aWide) {
    if (aWide.WasPassed()) {
      mNarrow = Some(NS_ConvertUTF16toUTF8(aWide.Value()));
    }
  }

  const nsACString* Get() const {
    if (mNarrow.isSome()) {
      return mNarrow.ptr();
    }
    return nullptr;
  }

 private:
  Maybe<NS_ConvertUTF16toUTF8> mNarrow;
};

}  // namespace mozilla::webgpu

#endif  // WEBGPU_TYPES_H_

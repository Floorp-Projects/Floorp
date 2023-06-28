/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_InternalError_H_
#define GPU_InternalError_H_

#include "Error.h"

namespace mozilla {
class ErrorResult;
namespace dom {
class GlobalObject;
}  // namespace dom
namespace webgpu {

class InternalError final : public Error {
 public:
  GPU_DECL_JS_WRAP(InternalError)

  InternalError(nsIGlobalObject* const aGlobal, const nsAString& aMessage)
      : Error(aGlobal, aMessage) {}

  InternalError(nsIGlobalObject* const aGlobal, const nsACString& aMessage)
      : Error(aGlobal, aMessage) {}

 private:
  ~InternalError() override = default;

 public:
  static already_AddRefed<InternalError> Constructor(
      const dom::GlobalObject& aGlobal, const nsAString& aString,
      ErrorResult& aRv);
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_InternalError_H_

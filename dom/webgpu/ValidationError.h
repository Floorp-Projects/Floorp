/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_ValidationError_H_
#define GPU_ValidationError_H_

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsWrapperCache.h"
#include "ObjectModel.h"

namespace mozilla {
class ErrorResult;

namespace dom {
class GlobalObject;
}  // namespace dom
namespace webgpu {

class ValidationError final : public nsWrapperCache {
  nsCOMPtr<nsIGlobalObject> mGlobal;
  nsString mMessage;

 public:
  GPU_DECL_CYCLE_COLLECTION(ValidationError)
  GPU_DECL_JS_WRAP(ValidationError)
  ValidationError(nsIGlobalObject* aGlobal, const nsACString& aMessage);
  ValidationError(nsIGlobalObject* aGlobal, const nsAString& aMessage);

 private:
  virtual ~ValidationError();
  void Cleanup() {}

 public:
  static already_AddRefed<ValidationError> Constructor(
      const dom::GlobalObject& aGlobal, const nsAString& aString,
      ErrorResult& aRv);
  void GetMessage(nsAString& aMessage) const { aMessage = mMessage; }
  nsIGlobalObject* GetParentObject() const { return mGlobal; }
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_ValidationError_H_

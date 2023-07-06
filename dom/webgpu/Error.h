/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_Error_H_
#define GPU_Error_H_

#include "js/Value.h"
#include "mozilla/WeakPtr.h"
#include "nsIGlobalObject.h"
#include "nsString.h"
#include "ObjectModel.h"

namespace mozilla {
class ErrorResult;
namespace dom {
class GlobalObject;
}  // namespace dom
namespace webgpu {

class Error : public nsWrapperCache, public SupportsWeakPtr {
 protected:
  nsCOMPtr<nsIGlobalObject> mGlobal;
  nsString mMessage;

 public:
  GPU_DECL_CYCLE_COLLECTION(Error)

  Error(nsIGlobalObject* const aGlobal, const nsAString& aMessage);
  Error(nsIGlobalObject* const aGlobal, const nsACString& aMessage);

 protected:
  virtual ~Error() = default;
  virtual void Cleanup() {}

 public:
  void GetMessage(nsAString& aMessage) const { aMessage = mMessage; }
  nsIGlobalObject* GetParentObject() const { return mGlobal; }
  virtual JSObject* WrapObject(JSContext*, JS::Handle<JSObject*>) = 0;
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // GPU_Error_H_

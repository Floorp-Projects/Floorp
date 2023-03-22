/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_JSValidatorParent
#define mozilla_dom_JSValidatorParent

#include "mozilla/MozPromise.h"
#include "mozilla/ProcInfo.h"
#include "mozilla/dom/PJSValidatorParent.h"

namespace mozilla::ipc {
class UtilityProcessParent;
class IProtocol;
}  // namespace mozilla::ipc

namespace mozilla::dom {

class JSValidatorParent final : public PJSValidatorParent {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(JSValidatorParent, override);

  static already_AddRefed<JSValidatorParent> Create();

  void IsOpaqueResponseAllowed(
      const std::function<void(Maybe<mozilla::ipc::Shmem>, ValidatorResult)>&
          aCallback);

  void OnDataAvailable(const nsACString& aData);

  void OnStopRequest(nsresult aResult, nsIRequest& aRequest);

 private:
  virtual ~JSValidatorParent() = default;
};
}  // namespace mozilla::dom

#endif  // defined(mozilla_dom_JSValidatorParent)

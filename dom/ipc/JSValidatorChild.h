/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_JSValidatorChild
#define mozilla_dom_JSValidatorChild

#include "mozilla/ProcInfo.h"
#include "mozilla/dom/PJSValidatorChild.h"
#include "mozilla/dom/JSValidatorUtils.h"

namespace mozilla::dom {
class JSValidatorChild final : public PJSValidatorChild {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(JSValidatorChild, override);

  mozilla::ipc::IPCResult RecvIsOpaqueResponseAllowed(
      IsOpaqueResponseAllowedResolver&& aResolver);

  mozilla::ipc::IPCResult RecvOnDataAvailable(Shmem&& aData);

  mozilla::ipc::IPCResult RecvOnStopRequest(const nsresult& aReason);

  void ActorDestroy(ActorDestroyReason aReason) override;

 private:
  virtual ~JSValidatorChild() = default;

  void Resolve(bool aAllow);
  bool ShouldAllowJS() const;

  nsCString mSourceBytes;
  Maybe<IsOpaqueResponseAllowedResolver> mResolver;
};
}  // namespace mozilla::dom

#endif  // defined(mozilla_dom_JSValidatorChild)

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_FenceUtilsGonk_h
#define mozilla_layers_FenceUtilsGonk_h

#include <unistd.h>
#include <ui/Fence.h>

#include "ipc/IPCMessageUtils.h"
#include "mozilla/layers/AsyncTransactionTracker.h" // for AsyncTransactionTracker

namespace mozilla {
namespace layers {

struct FenceHandle {
  typedef android::Fence Fence;

  FenceHandle()
  { }
  FenceHandle(const android::sp<Fence>& aFence);

  bool operator==(const FenceHandle& aOther) const {
    return mFence.get() == aOther.mFence.get();
  }

  bool IsValid() const
  {
    return mFence.get() && mFence->isValid();
  }

  android::sp<Fence> mFence;
};

// FenceDeliveryTracker puts off releasing a Fence until a transaction complete.
class FenceDeliveryTracker : public AsyncTransactionTracker {
public:
  FenceDeliveryTracker(const android::sp<android::Fence>& aFence)
    : mFence(aFence)
  {
    MOZ_COUNT_CTOR(FenceDeliveryTracker);
  }

  ~FenceDeliveryTracker()
  {
    MOZ_COUNT_DTOR(FenceDeliveryTracker);
  }

  virtual void Complete() MOZ_OVERRIDE
  {
    mFence = nullptr;
  }

  virtual void Cancel() MOZ_OVERRIDE
  {
    mFence = nullptr;
  }

private:
  android::sp<android::Fence> mFence;
};

} // namespace layers
} // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::layers::FenceHandle> {
  typedef mozilla::layers::FenceHandle paramType;

  static void Write(Message* aMsg, const paramType& aParam);
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult);
};

} // namespace IPC

#endif  // mozilla_layers_FenceUtilsGonk_h

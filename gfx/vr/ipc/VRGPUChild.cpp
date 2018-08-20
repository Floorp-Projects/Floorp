/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRGPUChild.h"


namespace mozilla {
namespace gfx {

static StaticRefPtr<VRGPUChild> sVRGPUChildSingleton;

/* static */ bool
VRGPUChild::InitForGPUProcess(Endpoint<PVRGPUChild>&& aEndpoint)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sVRGPUChildSingleton);

  RefPtr<VRGPUChild> child(new VRGPUChild());
  if (!aEndpoint.Bind(child)) {
    return false;
  }
  sVRGPUChildSingleton = child;
  return true;
}

/* static */ bool
VRGPUChild::IsCreated()
{
  return !!sVRGPUChildSingleton;
}

/* static */ VRGPUChild*
VRGPUChild::Get()
{
  MOZ_ASSERT(IsCreated(), "VRGPUChild haven't initialized yet.");
  return sVRGPUChildSingleton;
}

/*static*/ void
VRGPUChild::ShutDown()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (sVRGPUChildSingleton) {
    sVRGPUChildSingleton->Destroy();
    sVRGPUChildSingleton = nullptr;
  }
}

class DeferredDeleteVRGPUChild : public Runnable
{
public:
  explicit DeferredDeleteVRGPUChild(RefPtr<VRGPUChild> aChild)
    : Runnable("gfx::DeferredDeleteVRGPUChild")
    , mChild(std::move(aChild))
  {
  }

  NS_IMETHODIMP Run() override {
    mChild->Close();
    return NS_OK;
  }

private:
  RefPtr<VRGPUChild> mChild;
};

void
VRGPUChild::Destroy()
{
  // Keep ourselves alive until everything has been shut down
  RefPtr<VRGPUChild> selfRef = this;
  NS_DispatchToMainThread(new DeferredDeleteVRGPUChild(this));
}

} // namespace gfx
} // namespace mozilla

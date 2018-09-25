/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_GPU_CHILD_H
#define GFX_VR_GPU_CHILD_H

#include "mozilla/gfx/PVRGPUChild.h"

namespace mozilla {
namespace gfx {

class VRGPUChild final : public PVRGPUChild
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VRGPUChild);

  static VRGPUChild* Get();
  static bool InitForGPUProcess(Endpoint<PVRGPUChild>&& aEndpoint);
  static bool IsCreated();
  static void ShutDown();

protected:
  explicit VRGPUChild() {}
  ~VRGPUChild() {}

private:
  DISALLOW_COPY_AND_ASSIGN(VRGPUChild);
};

} // namespace gfx
} // namespace mozilla

#endif // GFX_VR_GPU_CHILD_H
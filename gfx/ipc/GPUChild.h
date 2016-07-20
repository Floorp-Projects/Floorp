/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_mozilla_gfx_ipc_GPUChild_h_
#define _include_mozilla_gfx_ipc_GPUChild_h_

#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/gfx/PGPUChild.h"

namespace mozilla {
namespace gfx {

class GPUProcessHost;

class GPUChild final : public PGPUChild
{
public:
  explicit GPUChild(GPUProcessHost* aHost);
  ~GPUChild();

  void Init();

  static void Destroy(UniquePtr<GPUChild>&& aChild);

  void ActorDestroy(ActorDestroyReason aWhy) override;

private:
  GPUProcessHost* mHost;
};

} // namespace gfx
} // namespace mozilla

#endif // _include_mozilla_gfx_ipc_GPUChild_h_

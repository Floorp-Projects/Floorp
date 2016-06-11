/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_gfx_ipc_GPUProcessImpl_h__
#define _include_gfx_ipc_GPUProcessImpl_h__

#include "mozilla/ipc/ProcessChild.h"
#include "GPUParent.h"

namespace mozilla {
namespace gfx {

// This class owns the subprocess instance of a PGPU - which in this case,
// is a GPUParent. It is instantiated as a singleton in XRE_InitChildProcess.
class GPUProcessImpl final : public ipc::ProcessChild
{
public:
  explicit GPUProcessImpl(ProcessId aParentPid);
  ~GPUProcessImpl();

  bool Init() override;
  void CleanUp() override;

private:
  DISALLOW_COPY_AND_ASSIGN(GPUProcessImpl);
  GPUParent mGPU;
};

} // namespace gfx
} // namespace mozilla

#endif // _include_gfx_ipc_GPUProcessImpl_h__

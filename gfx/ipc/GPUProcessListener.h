/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_mozilla_gfx_ipc_GPUProcessListener_h_
#define _include_mozilla_gfx_ipc_GPUProcessListener_h_

namespace mozilla {
namespace gfx {

class GPUProcessListener {
 public:
  virtual ~GPUProcessListener() = default;

  // Called when the compositor has died and the rendering stack must be
  // recreated.
  virtual void OnCompositorUnexpectedShutdown() {}

  // Called when devices have been reset and tabs must throw away their
  // layer managers.
  virtual void OnCompositorDeviceReset() {}
};

}  // namespace gfx
}  // namespace mozilla

#endif  // _include_mozilla_gfx_ipc_GPUProcessListener_h_

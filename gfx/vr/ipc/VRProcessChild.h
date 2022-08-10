/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_PROCESS_CHILD_H
#define GFX_VR_PROCESS_CHILD_H

#include "mozilla/ipc/ProcessChild.h"
#include "VRParent.h"

namespace mozilla {
namespace gfx {

/**
 * Contains the VRChild object that facilitates IPC communication to/from
 * the instance of the VR library that is run in this process.
 */
class VRProcessChild final : public mozilla::ipc::ProcessChild {
 protected:
  typedef mozilla::ipc::ProcessChild ProcessChild;

 public:
  using ProcessChild::ProcessChild;
  ~VRProcessChild();

  // IPC channel for VR process talk to the parent process.
  static VRParent* GetVRParent();

  // ProcessChild functions.
  virtual bool Init(int aArgc, char* aArgv[]) override;
  virtual void CleanUp() override;
};

}  // namespace gfx
}  // namespace mozilla

#endif /* GFX_VR_PROCESS_CHILD_H */

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_SERVICE_MANAGER_H
#define GFX_VR_SERVICE_MANAGER_H

namespace mozilla {
namespace gfx {

class VRServiceManager {
 public:
  static VRServiceManager& Get();

  void Refresh();
  void Start();
  void Stop();
  void Shutdown();
  void CreateVRProcess();
  void ShutdownVRProcess();
  void CreateService();
  bool IsServiceValid();

  VRExternalShmem* GetAPIShmem();

 protected:
 private:
  VRServiceManager();
  ~VRServiceManager() = default;

#if !defined(MOZ_WIDGET_ANDROID)
  RefPtr<VRService> mVRService;
#endif
};

}  // namespace gfx
}  // namespace mozilla

#endif  // GFX_VR_SERVICE_MANAGER_H
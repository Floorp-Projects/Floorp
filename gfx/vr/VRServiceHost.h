/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_VR_SERVICE_HOST_H
#define GFX_VR_SERVICE_HOST_H

#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h"
#include "nsTArray.h"

#include <cstdint>

namespace mozilla {
namespace gfx {

struct VRExternalShmem;
class VRService;

class VRServiceHost {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::gfx::VRServiceHost)
 public:
  static void Init(bool aEnableVRProcess);
  static VRServiceHost* Get();

  void Refresh();
  void StartService();
  void StopService();
  void Shutdown();
#if !defined(MOZ_WIDGET_ANDROID)
  void CreateService(volatile VRExternalShmem* aShmem);
#endif

  void PuppetSubmit(const nsTArray<uint64_t>& aBuffer);
  void PuppetReset();
  bool PuppetHasEnded();

 protected:
 private:
  explicit VRServiceHost(bool aEnableVRProcess);
  ~VRServiceHost();

  bool mPuppetActive;
#if !defined(MOZ_WIDGET_ANDROID)
  void RefreshVRProcess();
  bool NeedVRProcess();
  void CreateVRProcess();
  void ShutdownVRProcess();

  RefPtr<VRService> mVRService;
  bool mVRProcessEnabled;
  bool mVRProcessStarted;
  bool mVRServiceRequested;

#endif
};

}  // namespace gfx
}  // namespace mozilla

#endif  // GFX_VR_SERVICE_HOST_H

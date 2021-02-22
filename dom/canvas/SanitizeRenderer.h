/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_RendererSanitizer_h
#define mozilla_RendererSanitizer_h

#include <string>

namespace mozilla {
namespace dom {

inline static void SanitizeRenderer(std::string& aRenderer) {
  // Remove DRM, kernel, and LLVM versions exposed by amdgpu.
  // The string looks like this:
  //
  // AMD Radeon (TM) GPU model Graphics (GPUgeneration, DRM
  // DRMversion, kernelversion, LLVM LLVMversion)
  //
  // e.g. AMD Radeon (TM) RX 460 Graphics (POLARIS11,
  // DRM 3.35.0, 5.4.0-65-generic, LLVM 11.0.0)
  //
  // OR
  //
  // AMD Radeon GPU model (GPUgeneration, DRM DRMversion, kernelversion, LLVM
  // LLVMversion)
  //
  // Just in case, let's handle the case without GPUgeneration, i.e.
  //
  // AMD Radeon GPU model (DRM DRMversion, kernelversion, LLVM LLVMversion)
  //
  // even though there's no existence proof of this variant.
  if (aRenderer.empty()) {
    return;
  }
  if (aRenderer.back() != ')') {
    return;
  }
  auto pos = aRenderer.find(", DRM ");
  if (pos != std::string::npos) {
    aRenderer.resize(pos);
    aRenderer.push_back(')');
    return;
  }
  pos = aRenderer.find(" (DRM ");
  if (pos != std::string::npos) {
    aRenderer.resize(pos);
    return;
  }
}

};  // namespace dom
};  // namespace mozilla

#endif  // mozilla_RendererSanitizer_h

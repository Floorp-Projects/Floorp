/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPU_CompilationInfo_H_
#define GPU_CompilationInfo_H_

#include "nsWrapperCache.h"
#include "ObjectModel.h"
#include "CompilationMessage.h"

namespace mozilla::webgpu {
class ShaderModule;

class CompilationInfo final : public nsWrapperCache,
                              public ChildOf<ShaderModule> {
 public:
  GPU_DECL_CYCLE_COLLECTION(CompilationInfo)
  GPU_DECL_JS_WRAP(CompilationInfo)

  explicit CompilationInfo(ShaderModule* const aParent);

  void SetMessages(
      nsTArray<mozilla::webgpu::WebGPUCompilationMessage>& aMessages);

  void GetMessages(
      nsTArray<RefPtr<mozilla::webgpu::CompilationMessage>>& aMessages);

 private:
  ~CompilationInfo() = default;
  void Cleanup() {}

  nsTArray<RefPtr<mozilla::webgpu::CompilationMessage>> mMessages;
};

}  // namespace mozilla::webgpu

#endif  // GPU_CompilationInfo_H_

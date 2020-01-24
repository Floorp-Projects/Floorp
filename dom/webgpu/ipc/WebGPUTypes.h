/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGPU_TYPES_H_
#define WEBGPU_TYPES_H_

#include <cstdint>
#include "nsTArray.h"

namespace mozilla {
namespace webgpu {
namespace ffi {
struct WGPUBindGroupLayoutBinding;
}  // namespace ffi

typedef uint64_t RawId;
typedef uint64_t BufferAddress;

struct SerialBindGroupLayoutDescriptor {
  nsTArray<ffi::WGPUBindGroupLayoutBinding> mBindings;
};

struct SerialPipelineLayoutDescriptor {
  nsTArray<RawId> mBindGroupLayouts;
};

enum class SerialBindGroupBindingType : uint8_t {
  Buffer,
  Texture,
  Sampler,
  EndGuard_
};

struct SerialBindGroupBinding {
  uint32_t mBinding;
  SerialBindGroupBindingType mType;
  RawId mValue;
  BufferAddress mBufferOffset;
  BufferAddress mBufferSize;
};

struct SerialBindGroupDescriptor {
  RawId mLayout;
  nsTArray<SerialBindGroupBinding> mBindings;
};

struct SerialProgrammableStageDescriptor {
  RawId mModule;
  nsString mEntryPoint;
};

struct SerialComputePipelineDescriptor {
  RawId mLayout;
  SerialProgrammableStageDescriptor mComputeStage;
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // WEBGPU_TYPES_H_

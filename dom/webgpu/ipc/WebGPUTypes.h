/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGPU_TYPES_H_
#define WEBGPU_TYPES_H_

#include <cstdint>
#include "nsTArray.h"
#include "mozilla/Maybe.h"
#include "mozilla/webgpu/ffi/wgpu.h"

namespace mozilla {
namespace webgpu {

typedef uint64_t RawId;
typedef uint64_t BufferAddress;

struct SerialBindGroupLayoutDescriptor {
  nsTArray<ffi::WGPUBindGroupLayoutEntry> mEntries;
};

struct SerialPipelineLayoutDescriptor {
  nsTArray<RawId> mBindGroupLayouts;
};

enum class SerialBindGroupEntryType : uint8_t {
  Buffer,
  Texture,
  Sampler,
  EndGuard_
};

struct SerialBindGroupEntry {
  uint32_t mBinding;
  SerialBindGroupEntryType mType;
  RawId mValue;
  BufferAddress mBufferOffset;
  BufferAddress mBufferSize;
};

struct SerialBindGroupDescriptor {
  RawId mLayout;
  nsTArray<SerialBindGroupEntry> mEntries;
};

struct SerialProgrammableStageDescriptor {
  RawId mModule;
  nsString mEntryPoint;
};

struct SerialComputePipelineDescriptor {
  RawId mLayout;
  SerialProgrammableStageDescriptor mComputeStage;
};

struct SerialVertexBufferLayoutDescriptor {
  ffi::WGPUBufferAddress mArrayStride;
  ffi::WGPUInputStepMode mStepMode;
  nsTArray<ffi::WGPUVertexAttributeDescriptor> mAttributes;
};

struct SerialVertexStateDescriptor {
  ffi::WGPUIndexFormat mIndexFormat;
  nsTArray<SerialVertexBufferLayoutDescriptor> mVertexBuffers;
};

struct SerialRenderPipelineDescriptor {
  RawId mLayout;
  SerialProgrammableStageDescriptor mVertexStage;
  SerialProgrammableStageDescriptor mFragmentStage;
  ffi::WGPUPrimitiveTopology mPrimitiveTopology;
  Maybe<ffi::WGPURasterizationStateDescriptor> mRasterizationState;
  nsTArray<ffi::WGPUColorStateDescriptor> mColorStates;
  Maybe<ffi::WGPUDepthStencilStateDescriptor> mDepthStencilState;
  SerialVertexStateDescriptor mVertexState;
  uint32_t mSampleCount;
  uint32_t mSampleMask;
  bool mAlphaToCoverageEnabled;
};

struct SerialTextureDescriptor {
  nsString mLabel;
  struct ffi::WGPUExtent3d mSize;
  uint32_t mArrayLayerCount;
  uint32_t mMipLevelCount;
  uint32_t mSampleCount;
  enum ffi::WGPUTextureDimension mDimension;
  enum ffi::WGPUTextureFormat mFormat;
  ffi::WGPUTextureUsage mUsage;
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // WEBGPU_TYPES_H_

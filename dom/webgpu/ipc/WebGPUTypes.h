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
  nsCString mLabel;
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
  uint32_t mBinding = 0;
  SerialBindGroupEntryType mType = SerialBindGroupEntryType::EndGuard_;
  RawId mValue = 0;
  BufferAddress mBufferOffset = 0;
  BufferAddress mBufferSize = 0;
};

struct SerialBindGroupDescriptor {
  nsCString mLabel;
  RawId mLayout = 0;
  nsTArray<SerialBindGroupEntry> mEntries;
};

struct SerialProgrammableStageDescriptor {
  RawId mModule = 0;
  nsString mEntryPoint;
};

struct SerialComputePipelineDescriptor {
  RawId mLayout = 0;
  SerialProgrammableStageDescriptor mComputeStage;
};

struct SerialVertexBufferLayoutDescriptor {
  ffi::WGPUBufferAddress mArrayStride = 0;
  ffi::WGPUInputStepMode mStepMode = ffi::WGPUInputStepMode_Sentinel;
  nsTArray<ffi::WGPUVertexAttributeDescriptor> mAttributes;
};

struct SerialVertexStateDescriptor {
  ffi::WGPUIndexFormat mIndexFormat = ffi::WGPUIndexFormat_Sentinel;
  nsTArray<SerialVertexBufferLayoutDescriptor> mVertexBuffers;
};

struct SerialRenderPipelineDescriptor {
  RawId mLayout = 0;
  SerialProgrammableStageDescriptor mVertexStage;
  SerialProgrammableStageDescriptor mFragmentStage;
  ffi::WGPUPrimitiveTopology mPrimitiveTopology =
      ffi::WGPUPrimitiveTopology_Sentinel;
  Maybe<ffi::WGPURasterizationStateDescriptor> mRasterizationState;
  nsTArray<ffi::WGPUColorStateDescriptor> mColorStates;
  Maybe<ffi::WGPUDepthStencilStateDescriptor> mDepthStencilState;
  SerialVertexStateDescriptor mVertexState;
  uint32_t mSampleCount = 0;
  uint32_t mSampleMask = 0;
  bool mAlphaToCoverageEnabled = false;
};

struct SerialSamplerDescriptor {
  nsCString mLabel;
  ffi::WGPUAddressMode mAddressU = ffi::WGPUAddressMode_Sentinel,
                       mAddressV = ffi::WGPUAddressMode_Sentinel,
                       mAddressW = ffi::WGPUAddressMode_Sentinel;
  ffi::WGPUFilterMode mMagFilter = ffi::WGPUFilterMode_Sentinel,
                      mMinFilter = ffi::WGPUFilterMode_Sentinel,
                      mMipmapFilter = ffi::WGPUFilterMode_Sentinel;
  float mLodMinClamp = 0.0, mLodMaxClamp = 0.0;
  Maybe<ffi::WGPUCompareFunction> mCompare;
  uint8_t mAnisotropyClamp = 0;
};

}  // namespace webgpu
}  // namespace mozilla

#endif  // WEBGPU_TYPES_H_

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGPU_SERIALIZE_H_
#define WEBGPU_SERIALIZE_H_

#include "WebGPUTypes.h"
#include "ipc/EnumSerializer.h"
#include "ipc/IPCMessageUtils.h"
#include "mozilla/dom/BindingIPCUtils.h"
#include "mozilla/dom/WebGPUBinding.h"
#include "mozilla/webgpu/ffi/wgpu.h"

namespace IPC {

#define DEFINE_IPC_SERIALIZER_ENUM_GUARD(something, guard) \
  template <>                                              \
  struct ParamTraits<something>                            \
      : public ContiguousEnumSerializer<something, something(0), guard> {}

#define DEFINE_IPC_SERIALIZER_DOM_ENUM(something) \
  template <>                                     \
  struct ParamTraits<something>                   \
      : public mozilla::dom::WebIDLEnumSerializer<something> {}
#define DEFINE_IPC_SERIALIZER_FFI_ENUM(something) \
  DEFINE_IPC_SERIALIZER_ENUM_GUARD(something, something##_Sentinel)

// -

DEFINE_IPC_SERIALIZER_DOM_ENUM(mozilla::dom::GPUErrorFilter);
DEFINE_IPC_SERIALIZER_DOM_ENUM(mozilla::dom::GPUPowerPreference);

DEFINE_IPC_SERIALIZER_FFI_ENUM(mozilla::webgpu::ffi::WGPUHostMap);

DEFINE_IPC_SERIALIZER_WITHOUT_FIELDS(mozilla::dom::GPUCommandBufferDescriptor);

DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::GPURequestAdapterOptions,
                                  mPowerPreference, mForceFallbackAdapter);

DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::GPUBufferDescriptor, mSize,
                                  mUsage, mMappedAtCreation);

DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::webgpu::PopErrorScopeResult,
                                  resultType, message);

DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::webgpu::WebGPUCompilationMessage,
                                  message, lineNum, linePos, offset, length);

#undef DEFINE_IPC_SERIALIZER_FFI_ENUM
#undef DEFINE_IPC_SERIALIZER_DOM_ENUM
#undef DEFINE_IPC_SERIALIZER_ENUM_GUARD

// -

template <>
struct ParamTraits<mozilla::webgpu::PopErrorScopeResultType>
    : public ContiguousEnumSerializerInclusive<
          mozilla::webgpu::PopErrorScopeResultType,
          mozilla::webgpu::PopErrorScopeResultType{0},
          mozilla::webgpu::PopErrorScopeResultType::_LAST> {};

}  // namespace IPC

#endif  // WEBGPU_SERIALIZE_H_

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGPU_SERIALIZE_H_
#define WEBGPU_SERIALIZE_H_

#include "WebGPUTypes.h"
#include "ipc/IPCMessageUtils.h"
#include "mozilla/dom/WebGPUBinding.h"

namespace IPC {

#define DEFINE_IPC_SERIALIZER_ENUM(something)                    \
  template <>                                                    \
  struct ParamTraits<something>                                  \
      : public ContiguousEnumSerializer<something, something(0), \
                                        something::EndGuard_> {}

DEFINE_IPC_SERIALIZER_ENUM(mozilla::dom::GPUPowerPreference);

DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::GPURequestAdapterOptions,
                                  mPowerPreference);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::GPUExtensions,
                                  mAnisotropicFiltering);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::GPULimits, mMaxBindGroups);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::GPUDeviceDescriptor,
                                  mExtensions, mLimits);
DEFINE_IPC_SERIALIZER_WITH_FIELDS(mozilla::dom::GPUBufferDescriptor, mSize,
                                  mUsage);
DEFINE_IPC_SERIALIZER_WITHOUT_FIELDS(mozilla::dom::GPUCommandEncoderDescriptor);
DEFINE_IPC_SERIALIZER_WITHOUT_FIELDS(mozilla::dom::GPUCommandBufferDescriptor);

#undef DEFINE_IPC_SERIALIZER_ENUM

}  // namespace IPC
#endif  // WEBGPU_SERIALIZE_H_

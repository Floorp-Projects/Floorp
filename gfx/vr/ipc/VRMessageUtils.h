/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_vr_VRMessageUtils_h
#define mozilla_gfx_vr_VRMessageUtils_h

#include "ipc/EnumSerializer.h"
#include "ipc/IPCMessageUtils.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/GfxMessageUtils.h"
#include "mozilla/dom/GamepadMessageUtils.h"

#include "gfxVR.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::gfx::VRControllerType>
    : public ContiguousEnumSerializer<mozilla::gfx::VRControllerType,
                                      mozilla::gfx::VRControllerType::_empty,
                                      mozilla::gfx::VRControllerType::_end> {};

// VRHMDSensorState is POD, we can use PlainOldDataSerializer
static_assert(std::is_pod<mozilla::gfx::VRHMDSensorState>::value,
              "mozilla::gfx::VRHMDSensorState must be a POD type.");
template <>
struct ParamTraits<mozilla::gfx::VRHMDSensorState>
    : public PlainOldDataSerializer<mozilla::gfx::VRHMDSensorState> {};

// VRDisplayInfo is POD, we can use PlainOldDataSerializer
static_assert(std::is_pod<mozilla::gfx::VRDisplayInfo>::value,
              "mozilla::gfx::VRDisplayInfo must be a POD type.");
template <>
struct ParamTraits<mozilla::gfx::VRDisplayInfo>
    : public PlainOldDataSerializer<mozilla::gfx::VRDisplayInfo> {};

template <>
struct ParamTraits<mozilla::gfx::VRSubmitFrameResultInfo> {
  typedef mozilla::gfx::VRSubmitFrameResultInfo paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mBase64Image);
    WriteParam(aWriter, aParam.mFormat);
    WriteParam(aWriter, aParam.mWidth);
    WriteParam(aWriter, aParam.mHeight);
    WriteParam(aWriter, aParam.mFrameNum);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &(aResult->mBase64Image)) ||
        !ReadParam(aReader, &(aResult->mFormat)) ||
        !ReadParam(aReader, &(aResult->mWidth)) ||
        !ReadParam(aReader, &(aResult->mHeight)) ||
        !ReadParam(aReader, &(aResult->mFrameNum))) {
      return false;
    }

    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::VRDisplayCapabilityFlags>
    : public BitFlagsEnumSerializer<
          mozilla::gfx::VRDisplayCapabilityFlags,
          mozilla::gfx::VRDisplayCapabilityFlags::Cap_All> {};

}  // namespace IPC

#endif  // mozilla_gfx_vr_VRMessageUtils_h

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_vr_VRMessageUtils_h
#define mozilla_gfx_vr_VRMessageUtils_h

#include "ipc/IPCMessageUtils.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/GfxMessageUtils.h"
#include "mozilla/dom/GamepadMessageUtils.h"
#include "VRManager.h"

#include "gfxVR.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::gfx::OpenVRControllerType>
    : public ContiguousEnumSerializer<
          mozilla::gfx::OpenVRControllerType,
          mozilla::gfx::OpenVRControllerType::Vive,
          mozilla::gfx::OpenVRControllerType::NumOpenVRControllerTypes> {};

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

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mBase64Image);
    WriteParam(aMsg, aParam.mFormat);
    WriteParam(aMsg, aParam.mWidth);
    WriteParam(aMsg, aParam.mHeight);
    WriteParam(aMsg, aParam.mFrameNum);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &(aResult->mBase64Image)) ||
        !ReadParam(aMsg, aIter, &(aResult->mFormat)) ||
        !ReadParam(aMsg, aIter, &(aResult->mWidth)) ||
        !ReadParam(aMsg, aIter, &(aResult->mHeight)) ||
        !ReadParam(aMsg, aIter, &(aResult->mFrameNum))) {
      return false;
    }

    return true;
  }
};

}  // namespace IPC

#endif  // mozilla_gfx_vr_VRMessageUtils_h

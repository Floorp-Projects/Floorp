/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_vr_VRMessageUtils_h
#define mozilla_gfx_vr_VRMessageUtils_h

#include "ipc/IPCMessageUtils.h"
#include "mozilla/GfxMessageUtils.h"
#include "VRManager.h"

#include "gfxVR.h"

namespace IPC {

template<>
struct ParamTraits<mozilla::gfx::VRDeviceType> :
  public ContiguousEnumSerializer<mozilla::gfx::VRDeviceType,
                                  mozilla::gfx::VRDeviceType(0),
                                  mozilla::gfx::VRDeviceType(mozilla::gfx::VRDeviceType::NumVRDeviceTypes)> {};

template<>
struct ParamTraits<mozilla::gfx::VRDisplayCapabilityFlags> :
  public BitFlagsEnumSerializer<mozilla::gfx::VRDisplayCapabilityFlags,
                                mozilla::gfx::VRDisplayCapabilityFlags::Cap_All> {};

template <>
struct ParamTraits<mozilla::gfx::VRDisplayInfo>
{
  typedef mozilla::gfx::VRDisplayInfo paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mType);
    WriteParam(aMsg, aParam.mDisplayID);
    WriteParam(aMsg, aParam.mDisplayName);
    WriteParam(aMsg, aParam.mCapabilityFlags);
    WriteParam(aMsg, aParam.mEyeResolution);
    WriteParam(aMsg, aParam.mIsConnected);
    WriteParam(aMsg, aParam.mIsMounted);
    WriteParam(aMsg, aParam.mPresentingGroups);
    WriteParam(aMsg, aParam.mGroupMask);
    WriteParam(aMsg, aParam.mStageSize);
    WriteParam(aMsg, aParam.mSittingToStandingTransform);
    WriteParam(aMsg, aParam.mFrameId);
    for (int i = 0; i < mozilla::gfx::VRDisplayInfo::NumEyes; i++) {
      WriteParam(aMsg, aParam.mEyeFOV[i]);
      WriteParam(aMsg, aParam.mEyeTranslation[i]);
    }
    for (int i = 0; i < mozilla::gfx::kVRMaxLatencyFrames; i++) {
      WriteParam(aMsg, aParam.mLastSensorState[i]);
    }
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &(aResult->mType)) ||
        !ReadParam(aMsg, aIter, &(aResult->mDisplayID)) ||
        !ReadParam(aMsg, aIter, &(aResult->mDisplayName)) ||
        !ReadParam(aMsg, aIter, &(aResult->mCapabilityFlags)) ||
        !ReadParam(aMsg, aIter, &(aResult->mEyeResolution)) ||
        !ReadParam(aMsg, aIter, &(aResult->mIsConnected)) ||
        !ReadParam(aMsg, aIter, &(aResult->mIsMounted)) ||
        !ReadParam(aMsg, aIter, &(aResult->mPresentingGroups)) ||
        !ReadParam(aMsg, aIter, &(aResult->mGroupMask)) ||
        !ReadParam(aMsg, aIter, &(aResult->mStageSize)) ||
        !ReadParam(aMsg, aIter, &(aResult->mSittingToStandingTransform)) ||
        !ReadParam(aMsg, aIter, &(aResult->mFrameId))) {
      return false;
    }
    for (int i = 0; i < mozilla::gfx::VRDisplayInfo::NumEyes; i++) {
      if (!ReadParam(aMsg, aIter, &(aResult->mEyeFOV[i])) ||
          !ReadParam(aMsg, aIter, &(aResult->mEyeTranslation[i]))) {
        return false;
      }
    }
    for (int i = 0; i < mozilla::gfx::kVRMaxLatencyFrames; i++) {
      if (!ReadParam(aMsg, aIter, &(aResult->mLastSensorState[i]))) {
        return false;
      }
    }

    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::VRHMDSensorState>
{
  typedef mozilla::gfx::VRHMDSensorState paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.timestamp);
    WriteParam(aMsg, aParam.inputFrameID);
    WriteParam(aMsg, aParam.flags);
    WriteParam(aMsg, aParam.orientation[0]);
    WriteParam(aMsg, aParam.orientation[1]);
    WriteParam(aMsg, aParam.orientation[2]);
    WriteParam(aMsg, aParam.orientation[3]);
    WriteParam(aMsg, aParam.position[0]);
    WriteParam(aMsg, aParam.position[1]);
    WriteParam(aMsg, aParam.position[2]);
    WriteParam(aMsg, aParam.angularVelocity[0]);
    WriteParam(aMsg, aParam.angularVelocity[1]);
    WriteParam(aMsg, aParam.angularVelocity[2]);
    WriteParam(aMsg, aParam.angularAcceleration[0]);
    WriteParam(aMsg, aParam.angularAcceleration[1]);
    WriteParam(aMsg, aParam.angularAcceleration[2]);
    WriteParam(aMsg, aParam.linearVelocity[0]);
    WriteParam(aMsg, aParam.linearVelocity[1]);
    WriteParam(aMsg, aParam.linearVelocity[2]);
    WriteParam(aMsg, aParam.linearAcceleration[0]);
    WriteParam(aMsg, aParam.linearAcceleration[1]);
    WriteParam(aMsg, aParam.linearAcceleration[2]);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &(aResult->timestamp)) ||
        !ReadParam(aMsg, aIter, &(aResult->inputFrameID)) ||
        !ReadParam(aMsg, aIter, &(aResult->flags)) ||
        !ReadParam(aMsg, aIter, &(aResult->orientation[0])) ||
        !ReadParam(aMsg, aIter, &(aResult->orientation[1])) ||
        !ReadParam(aMsg, aIter, &(aResult->orientation[2])) ||
        !ReadParam(aMsg, aIter, &(aResult->orientation[3])) ||
        !ReadParam(aMsg, aIter, &(aResult->position[0])) ||
        !ReadParam(aMsg, aIter, &(aResult->position[1])) ||
        !ReadParam(aMsg, aIter, &(aResult->position[2])) ||
        !ReadParam(aMsg, aIter, &(aResult->angularVelocity[0])) ||
        !ReadParam(aMsg, aIter, &(aResult->angularVelocity[1])) ||
        !ReadParam(aMsg, aIter, &(aResult->angularVelocity[2])) ||
        !ReadParam(aMsg, aIter, &(aResult->angularAcceleration[0])) ||
        !ReadParam(aMsg, aIter, &(aResult->angularAcceleration[1])) ||
        !ReadParam(aMsg, aIter, &(aResult->angularAcceleration[2])) ||
        !ReadParam(aMsg, aIter, &(aResult->linearVelocity[0])) ||
        !ReadParam(aMsg, aIter, &(aResult->linearVelocity[1])) ||
        !ReadParam(aMsg, aIter, &(aResult->linearVelocity[2])) ||
        !ReadParam(aMsg, aIter, &(aResult->linearAcceleration[0])) ||
        !ReadParam(aMsg, aIter, &(aResult->linearAcceleration[1])) ||
        !ReadParam(aMsg, aIter, &(aResult->linearAcceleration[2]))) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::VRFieldOfView>
{
  typedef mozilla::gfx::VRFieldOfView paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.upDegrees);
    WriteParam(aMsg, aParam.rightDegrees);
    WriteParam(aMsg, aParam.downDegrees);
    WriteParam(aMsg, aParam.leftDegrees);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &(aResult->upDegrees)) ||
        !ReadParam(aMsg, aIter, &(aResult->rightDegrees)) ||
        !ReadParam(aMsg, aIter, &(aResult->downDegrees)) ||
        !ReadParam(aMsg, aIter, &(aResult->leftDegrees))) {
      return false;
    }

    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::VRControllerInfo>
{
  typedef mozilla::gfx::VRControllerInfo paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mType);
    WriteParam(aMsg, aParam.mControllerID);
    WriteParam(aMsg, aParam.mControllerName);
    WriteParam(aMsg, aParam.mMappingType);
    WriteParam(aMsg, aParam.mNumButtons);
    WriteParam(aMsg, aParam.mNumAxes);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &(aResult->mType)) ||
        !ReadParam(aMsg, aIter, &(aResult->mControllerID)) ||
        !ReadParam(aMsg, aIter, &(aResult->mControllerName)) ||
        !ReadParam(aMsg, aIter, &(aResult->mMappingType)) ||
        !ReadParam(aMsg, aIter, &(aResult->mNumButtons)) ||
        !ReadParam(aMsg, aIter, &(aResult->mNumAxes))) {
      return false;
    }

    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::VRSubmitFrameResultInfo>
{
  typedef mozilla::gfx::VRSubmitFrameResultInfo paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mBase64Image);
    WriteParam(aMsg, aParam.mFormat);
    WriteParam(aMsg, aParam.mWidth);
    WriteParam(aMsg, aParam.mHeight);
    WriteParam(aMsg, aParam.mFrameNum);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
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

} // namespace IPC

#endif // mozilla_gfx_vr_VRMessageUtils_h

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
struct ParamTraits<mozilla::gfx::VRHMDType> :
  public ContiguousEnumSerializer<mozilla::gfx::VRHMDType,
                                  mozilla::gfx::VRHMDType(0),
                                  mozilla::gfx::VRHMDType(mozilla::gfx::VRHMDType::NumHMDTypes)> {};

template<>
struct ParamTraits<mozilla::gfx::VRStateValidFlags> :
  public BitFlagsEnumSerializer<mozilla::gfx::VRStateValidFlags,
                                mozilla::gfx::VRStateValidFlags::State_All> {};

template <>
struct ParamTraits<mozilla::gfx::VRDeviceUpdate>
{
  typedef mozilla::gfx::VRDeviceUpdate paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mDeviceInfo);
    WriteParam(aMsg, aParam.mSensorState);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &(aResult->mDeviceInfo)) ||
        !ReadParam(aMsg, aIter, &(aResult->mSensorState))) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::VRSensorUpdate>
{
  typedef mozilla::gfx::VRSensorUpdate paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mDeviceID);
    WriteParam(aMsg, aParam.mSensorState);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &(aResult->mDeviceID)) ||
        !ReadParam(aMsg, aIter, &(aResult->mSensorState))) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::gfx::VRDeviceInfo>
{
  typedef mozilla::gfx::VRDeviceInfo paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mType);
    WriteParam(aMsg, aParam.mDeviceID);
    WriteParam(aMsg, aParam.mDeviceName);
    WriteParam(aMsg, aParam.mSupportedSensorBits);
    WriteParam(aMsg, aParam.mEyeResolution);
    WriteParam(aMsg, aParam.mScreenRect);
    WriteParam(aMsg, aParam.mIsFakeScreen);
    WriteParam(aMsg, aParam.mUseMainThreadOrientation);
    for (int i = 0; i < mozilla::gfx::VRDeviceInfo::NumEyes; i++) {
      WriteParam(aMsg, aParam.mMaximumEyeFOV[i]);
      WriteParam(aMsg, aParam.mRecommendedEyeFOV[i]);
      WriteParam(aMsg, aParam.mEyeFOV[i]);
      WriteParam(aMsg, aParam.mEyeTranslation[i]);
      WriteParam(aMsg, aParam.mEyeProjectionMatrix[i]);
    }
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &(aResult->mType)) ||
        !ReadParam(aMsg, aIter, &(aResult->mDeviceID)) ||
        !ReadParam(aMsg, aIter, &(aResult->mDeviceName)) ||
        !ReadParam(aMsg, aIter, &(aResult->mSupportedSensorBits)) ||
        !ReadParam(aMsg, aIter, &(aResult->mEyeResolution)) ||
        !ReadParam(aMsg, aIter, &(aResult->mScreenRect)) ||
        !ReadParam(aMsg, aIter, &(aResult->mIsFakeScreen)) ||
        !ReadParam(aMsg, aIter, &(aResult->mUseMainThreadOrientation))
        ) {
      return false;
    }
    for (int i = 0; i < mozilla::gfx::VRDeviceInfo::NumEyes; i++) {
      if (!ReadParam(aMsg, aIter, &(aResult->mMaximumEyeFOV[i])) ||
          !ReadParam(aMsg, aIter, &(aResult->mRecommendedEyeFOV[i])) ||
          !ReadParam(aMsg, aIter, &(aResult->mEyeFOV[i])) ||
          !ReadParam(aMsg, aIter, &(aResult->mEyeTranslation[i])) ||
          !ReadParam(aMsg, aIter, &(aResult->mEyeProjectionMatrix[i]))) {
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

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &(aResult->timestamp)) ||
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

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
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

} // namespace IPC

#endif // mozilla_gfx_vr_VRMessageUtils_h

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_gamepad_GamepadMessageUtils_h
#define mozilla_dom_gamepad_GamepadMessageUtils_h

#include "ipc/EnumSerializer.h"
#include "mozilla/dom/GamepadBinding.h"
#include "mozilla/dom/GamepadHandle.h"
#include "mozilla/dom/GamepadLightIndicatorBinding.h"
#include "mozilla/dom/GamepadPoseState.h"
#include "mozilla/dom/GamepadTouchState.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::dom::GamepadLightIndicatorType>
    : public ContiguousEnumSerializer<
          mozilla::dom::GamepadLightIndicatorType,
          mozilla::dom::GamepadLightIndicatorType(0),
          mozilla::dom::GamepadLightIndicatorType(
              mozilla::dom::GamepadLightIndicatorType::EndGuard_)> {};

template <>
struct ParamTraits<mozilla::dom::GamepadMappingType>
    : public ContiguousEnumSerializer<
          mozilla::dom::GamepadMappingType, mozilla::dom::GamepadMappingType(0),
          mozilla::dom::GamepadMappingType(
              mozilla::dom::GamepadMappingType::EndGuard_)> {};

template <>
struct ParamTraits<mozilla::dom::GamepadHand>
    : public ContiguousEnumSerializer<
          mozilla::dom::GamepadHand, mozilla::dom::GamepadHand(0),
          mozilla::dom::GamepadHand(mozilla::dom::GamepadHand::EndGuard_)> {};

template <>
struct ParamTraits<mozilla::dom::GamepadCapabilityFlags>
    : public BitFlagsEnumSerializer<
          mozilla::dom::GamepadCapabilityFlags,
          mozilla::dom::GamepadCapabilityFlags::Cap_All> {};

template <>
struct ParamTraits<mozilla::dom::GamepadPoseState> {
  typedef mozilla::dom::GamepadPoseState paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.flags);
    WriteParam(aWriter, aParam.orientation[0]);
    WriteParam(aWriter, aParam.orientation[1]);
    WriteParam(aWriter, aParam.orientation[2]);
    WriteParam(aWriter, aParam.orientation[3]);
    WriteParam(aWriter, aParam.position[0]);
    WriteParam(aWriter, aParam.position[1]);
    WriteParam(aWriter, aParam.position[2]);
    WriteParam(aWriter, aParam.angularVelocity[0]);
    WriteParam(aWriter, aParam.angularVelocity[1]);
    WriteParam(aWriter, aParam.angularVelocity[2]);
    WriteParam(aWriter, aParam.angularAcceleration[0]);
    WriteParam(aWriter, aParam.angularAcceleration[1]);
    WriteParam(aWriter, aParam.angularAcceleration[2]);
    WriteParam(aWriter, aParam.linearVelocity[0]);
    WriteParam(aWriter, aParam.linearVelocity[1]);
    WriteParam(aWriter, aParam.linearVelocity[2]);
    WriteParam(aWriter, aParam.linearAcceleration[0]);
    WriteParam(aWriter, aParam.linearAcceleration[1]);
    WriteParam(aWriter, aParam.linearAcceleration[2]);
    WriteParam(aWriter, aParam.isPositionValid);
    WriteParam(aWriter, aParam.isOrientationValid);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &(aResult->flags)) ||
        !ReadParam(aReader, &(aResult->orientation[0])) ||
        !ReadParam(aReader, &(aResult->orientation[1])) ||
        !ReadParam(aReader, &(aResult->orientation[2])) ||
        !ReadParam(aReader, &(aResult->orientation[3])) ||
        !ReadParam(aReader, &(aResult->position[0])) ||
        !ReadParam(aReader, &(aResult->position[1])) ||
        !ReadParam(aReader, &(aResult->position[2])) ||
        !ReadParam(aReader, &(aResult->angularVelocity[0])) ||
        !ReadParam(aReader, &(aResult->angularVelocity[1])) ||
        !ReadParam(aReader, &(aResult->angularVelocity[2])) ||
        !ReadParam(aReader, &(aResult->angularAcceleration[0])) ||
        !ReadParam(aReader, &(aResult->angularAcceleration[1])) ||
        !ReadParam(aReader, &(aResult->angularAcceleration[2])) ||
        !ReadParam(aReader, &(aResult->linearVelocity[0])) ||
        !ReadParam(aReader, &(aResult->linearVelocity[1])) ||
        !ReadParam(aReader, &(aResult->linearVelocity[2])) ||
        !ReadParam(aReader, &(aResult->linearAcceleration[0])) ||
        !ReadParam(aReader, &(aResult->linearAcceleration[1])) ||
        !ReadParam(aReader, &(aResult->linearAcceleration[2])) ||
        !ReadParam(aReader, &(aResult->isPositionValid)) ||
        !ReadParam(aReader, &(aResult->isOrientationValid))) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::dom::GamepadTouchState> {
  typedef mozilla::dom::GamepadTouchState paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.touchId);
    WriteParam(aWriter, aParam.surfaceId);
    WriteParam(aWriter, aParam.position[0]);
    WriteParam(aWriter, aParam.position[1]);
    WriteParam(aWriter, aParam.surfaceDimensions[0]);
    WriteParam(aWriter, aParam.surfaceDimensions[1]);
    WriteParam(aWriter, aParam.isSurfaceDimensionsValid);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &(aResult->touchId)) ||
        !ReadParam(aReader, &(aResult->surfaceId)) ||
        !ReadParam(aReader, &(aResult->position[0])) ||
        !ReadParam(aReader, &(aResult->position[1])) ||
        !ReadParam(aReader, &(aResult->surfaceDimensions[0])) ||
        !ReadParam(aReader, &(aResult->surfaceDimensions[1])) ||
        !ReadParam(aReader, &(aResult->isSurfaceDimensionsValid))) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::dom::GamepadHandleKind>
    : public ContiguousEnumSerializerInclusive<
          mozilla::dom::GamepadHandleKind,
          mozilla::dom::GamepadHandleKind::GamepadPlatformManager,
          mozilla::dom::GamepadHandleKind::VR> {};

template <>
struct ParamTraits<mozilla::dom::GamepadHandle> {
  typedef mozilla::dom::GamepadHandle paramType;
  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mValue);
    WriteParam(aWriter, aParam.mKind);
  }
  static bool Read(MessageReader* aReader, paramType* aParam) {
    return ReadParam(aReader, &aParam->mValue) &&
           ReadParam(aReader, &aParam->mKind);
  }
};

}  // namespace IPC

#endif  // mozilla_dom_gamepad_GamepadMessageUtils_h

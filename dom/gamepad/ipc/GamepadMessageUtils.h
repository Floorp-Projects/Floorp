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

  static void Write(Message* aMsg, const paramType& aParam) {
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
    WriteParam(aMsg, aParam.isPositionValid);
    WriteParam(aMsg, aParam.isOrientationValid);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &(aResult->flags)) ||
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
        !ReadParam(aMsg, aIter, &(aResult->linearAcceleration[2])) ||
        !ReadParam(aMsg, aIter, &(aResult->isPositionValid)) ||
        !ReadParam(aMsg, aIter, &(aResult->isOrientationValid))) {
      return false;
    }
    return true;
  }
};

template <>
struct ParamTraits<mozilla::dom::GamepadTouchState> {
  typedef mozilla::dom::GamepadTouchState paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.touchId);
    WriteParam(aMsg, aParam.surfaceId);
    WriteParam(aMsg, aParam.position[0]);
    WriteParam(aMsg, aParam.position[1]);
    WriteParam(aMsg, aParam.surfaceDimensions[0]);
    WriteParam(aMsg, aParam.surfaceDimensions[1]);
    WriteParam(aMsg, aParam.isSurfaceDimensionsValid);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &(aResult->touchId)) ||
        !ReadParam(aMsg, aIter, &(aResult->surfaceId)) ||
        !ReadParam(aMsg, aIter, &(aResult->position[0])) ||
        !ReadParam(aMsg, aIter, &(aResult->position[1])) ||
        !ReadParam(aMsg, aIter, &(aResult->surfaceDimensions[0])) ||
        !ReadParam(aMsg, aIter, &(aResult->surfaceDimensions[1])) ||
        !ReadParam(aMsg, aIter, &(aResult->isSurfaceDimensionsValid))) {
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
  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mValue);
    WriteParam(aMsg, aParam.mKind);
  }
  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aParam) {
    return ReadParam(aMsg, aIter, &aParam->mValue) &&
           ReadParam(aMsg, aIter, &aParam->mKind);
  }
};

}  // namespace IPC

#endif  // mozilla_dom_gamepad_GamepadMessageUtils_h

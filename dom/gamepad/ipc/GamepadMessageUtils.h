
#ifndef mozilla_dom_gamepad_GamepadMessageUtils_h
#define mozilla_dom_gamepad_GamepadMessageUtils_h

#include "ipc/IPCMessageUtils.h"
#include "mozilla/dom/GamepadBinding.h"
#include "mozilla/dom/GamepadPoseState.h"
#include "mozilla/dom/GamepadServiceType.h"

namespace IPC {

template<>
struct ParamTraits<mozilla::dom::GamepadMappingType> :
  public ContiguousEnumSerializer<mozilla::dom::GamepadMappingType,
                                  mozilla::dom::GamepadMappingType(0),
                                  mozilla::dom::GamepadMappingType(
                                  mozilla::dom::GamepadMappingType::EndGuard_)> {};

template<>
struct ParamTraits<mozilla::dom::GamepadHand> :
  public ContiguousEnumSerializer<mozilla::dom::GamepadHand,
                                  mozilla::dom::GamepadHand(0),
                                  mozilla::dom::GamepadHand(
                                  mozilla::dom::GamepadHand::EndGuard_)> {};

template<>
struct ParamTraits<mozilla::dom::GamepadServiceType> :
  public ContiguousEnumSerializer<mozilla::dom::GamepadServiceType,
                                  mozilla::dom::GamepadServiceType(0),
                                  mozilla::dom::GamepadServiceType(
                                  mozilla::dom::GamepadServiceType::NumGamepadServiceType)> {};

template<>
struct ParamTraits<mozilla::dom::GamepadCapabilityFlags> :
  public BitFlagsEnumSerializer<mozilla::dom::GamepadCapabilityFlags,
                                mozilla::dom::GamepadCapabilityFlags::Cap_All> {};

template <>
struct ParamTraits<mozilla::dom::GamepadPoseState>
{
  typedef mozilla::dom::GamepadPoseState paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
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

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
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

} // namespace IPC

#endif // mozilla_dom_gamepad_GamepadMessageUtils_h
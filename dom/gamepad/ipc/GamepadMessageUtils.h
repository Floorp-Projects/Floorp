
#ifndef mozilla_dom_gamepad_GamepadMessageUtils_h
#define mozilla_dom_gamepad_GamepadMessageUtils_h

#include "ipc/IPCMessageUtils.h"
#include "mozilla/dom/GamepadServiceType.h"
#include "mozilla/dom/GamepadPoseState.h"

namespace IPC {

template<>
struct ParamTraits<mozilla::dom::GamepadServiceType> :
  public ContiguousEnumSerializer<mozilla::dom::GamepadServiceType,
                                  mozilla::dom::GamepadServiceType(0),
                                  mozilla::dom::GamepadServiceType(
                                  mozilla::dom::GamepadServiceType::NumGamepadServiceType)> {};
} // namespace IPC

#endif // mozilla_dom_gamepad_GamepadMessageUtils_h
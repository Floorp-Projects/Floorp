
#ifndef mozilla_dom_gamepad_GamepadMessageUtils_h
#define mozilla_dom_gamepad_GamepadMessageUtils_h

#include "ipc/IPCMessageUtils.h"
#include "mozilla/dom/Gamepad.h"

namespace IPC {

template<>
struct ParamTraits<mozilla::dom::GamepadMappingType> :
  public ContiguousEnumSerializer<mozilla::dom::GamepadMappingType,
                                  mozilla::dom::GamepadMappingType(mozilla::dom::GamepadMappingType::_empty),
                                  mozilla::dom::GamepadMappingType(mozilla::dom::GamepadMappingType::EndGuard_)> {};

template<>
struct ParamTraits<mozilla::dom::GamepadServiceType> :
  public ContiguousEnumSerializer<mozilla::dom::GamepadServiceType,
                                  mozilla::dom::GamepadServiceType(0),
                                  mozilla::dom::GamepadServiceType(
                                  mozilla::dom::GamepadServiceType::NumGamepadServiceType)> {};
} // namespace IPC

#endif // mozilla_dom_gamepad_GamepadMessageUtils_h
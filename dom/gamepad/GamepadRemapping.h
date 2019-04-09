/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_GamepadRemapping_h_
#define mozilla_dom_GamepadRemapping_h_

namespace mozilla {
namespace dom {

// GamepadId is (vendorId << 16) | productId)
enum class GamepadId : uint32_t {
  kSonyDualshock4 = 0x054c05c4,
  kSonyDualshock4Slim = 0x054c09cc,
  kSonyDualshock4USBReceiver = 0x054c0ba0,
};

class GamepadRemapper {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GamepadRemapper)

 public:
  virtual uint32_t GetAxisCount() const = 0;
  virtual uint32_t GetButtonCount() const = 0;
  virtual void SetAxisCount(uint32_t aButtonCount) {}
  virtual void SetButtonCount(uint32_t aButtonCount) {}
  virtual GamepadMappingType GetMappingType() const {
    return GamepadMappingType::Standard;
  }
  virtual void RemapAxisMoveEvent(uint32_t aIndex, uint32_t aAxis,
                                  double aValue) const = 0;
  virtual void RemapButtonEvent(uint32_t aIndex, uint32_t aButton,
                                bool aPressed) const = 0;
  
 protected:
  GamepadRemapper() = default;
  virtual ~GamepadRemapper() = default;
};

struct GamepadRemappingData {
  GamepadId id;
  RefPtr<GamepadRemapper> remapping;
};

already_AddRefed<GamepadRemapper> GetGamepadRemapper(const uint16_t aVendorId,
                                                     const uint16_t aProductId);

}  // namespace dom
}  // namespace mozilla

#endif

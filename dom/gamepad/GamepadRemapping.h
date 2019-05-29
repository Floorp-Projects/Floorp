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
  // Nexus Player Controller
  kAsusTekProduct4500 = 0x0b054500,
  // 2Axes 8Keys Game Pad
  kDragonRiseProduct0011 = 0x00790011,
  // ADT-1 Controller
  kGoogleProduct2c40 = 0x18d12c40,
  // Stadia Controller
  kGoogleProduct9400 = 0x18d19400,
  // Logitech F310, D-mode
  kLogitechProductc216 = 0x046dc216,
  // Logitech F510, D-mode
  kLogitechProductc218 = 0x046dc218,
  // Logitech F710, D-mode
  kLogitechProductc219 = 0x046dc219,
  // Switch Joy-Con L
  kNintendoProduct2006 = 0x057e2006,
  // Switch Joy-Con R
  kNintendoProduct2007 = 0x057e2007,
  // Switch Pro Controller
  kNintendoProduct2009 = 0x057e2009,
  // Switch Charging Grip
  kNintendoProduct200e = 0x057e200e,
  // Nvidia Shield gamepad (2015)
  kNvidiaProduct7210 = 0x09557210,
  // Nvidia Shield gamepad (2017)
  kNvidiaProduct7214 = 0x09557214,
  // iBuffalo Classic
  kPadixProduct2060 = 0x05832060,
  // XSkills Gamecube USB adapter
  kPlayComProduct0005 = 0x0b430005,
  // boom PSX+N64 USB Converter
  kPrototypeVendorProduct0667 = 0x66660667,
  // Analog game controller
  kPrototypeVendorProduct9401 = 0x66669401,
  // Razer Serval Controller
  kRazer1532Product0900 = 0x15320900,
  // Playstation Dualshock 4
  kSonyProduct05c4 = 0x054c05c4,
  // Dualshock 4 (PS4 Slim)
  kSonyProduct09cc = 0x054c09cc,
  // Dualshock 4 USB receiver
  kSonyProduct0ba0 = 0x054c0ba0,
  // Moga Pro Controller (HID mode)
  kVendor20d6Product6271 = 0x20d66271,
  // OnLive Controller (Bluetooth)
  kVendor2378Product1008 = 0x23781008,
  // OnLive Controller (Wired)
  kVendor2378Product100a = 0x2378100a,
  // OUYA Controller
  kVendor2836Product0001 = 0x28360001,
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

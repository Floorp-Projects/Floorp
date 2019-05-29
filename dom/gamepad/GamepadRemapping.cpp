/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Based on
// https://cs.chromium.org/chromium/src/device/gamepad/gamepad_standard_mappings.h

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mozilla/dom/GamepadRemapping.h"

namespace mozilla {
namespace dom {

// Follow the canonical ordering recommendation for the "Standard Gamepad"
// from https://www.w3.org/TR/gamepad/#remapping.
enum CanonicalButtonIndex {
  BUTTON_INDEX_PRIMARY,
  BUTTON_INDEX_SECONDARY,
  BUTTON_INDEX_TERTIARY,
  BUTTON_INDEX_QUATERNARY,
  BUTTON_INDEX_LEFT_SHOULDER,
  BUTTON_INDEX_RIGHT_SHOULDER,
  BUTTON_INDEX_LEFT_TRIGGER,
  BUTTON_INDEX_RIGHT_TRIGGER,
  BUTTON_INDEX_BACK_SELECT,
  BUTTON_INDEX_START,
  BUTTON_INDEX_LEFT_THUMBSTICK,
  BUTTON_INDEX_RIGHT_THUMBSTICK,
  BUTTON_INDEX_DPAD_UP,
  BUTTON_INDEX_DPAD_DOWN,
  BUTTON_INDEX_DPAD_LEFT,
  BUTTON_INDEX_DPAD_RIGHT,
  BUTTON_INDEX_META,
  BUTTON_INDEX_COUNT
};

enum CanonicalAxisIndex {
  AXIS_INDEX_LEFT_STICK_X,
  AXIS_INDEX_LEFT_STICK_Y,
  AXIS_INDEX_RIGHT_STICK_X,
  AXIS_INDEX_RIGHT_STICK_Y,
  AXIS_INDEX_COUNT
};

bool AxisNegativeAsButton(float input) {
  const float value = (input < -0.5f) ? 1.f : 0.f;
  return value > 0.1f;
}

bool AxisPositiveAsButton(float input) {
  const float value = (input > 0.5f) ? 1.f : 0.f;
  return value > 0.1f;
}

void FetchDpadFromAxis(uint32_t aIndex, double dir) {
  bool up = false;
  bool right = false;
  bool down = false;
  bool left = false;

  // Dpad is mapped as a direction on one axis, where -1 is up and it
  // increases clockwise to 1, which is up + left. It's set to a large (> 1.f)
  // number when nothing is depressed, except on start up, sometimes it's 0.0
  // for no data, rather than the large number.
  if (dir != 0.0f) {
    up = (dir >= -1.f && dir < -0.7f) || (dir >= .95f && dir <= 1.f);
    right = dir >= -.75f && dir < -.1f;
    down = dir >= -.2f && dir < .45f;
    left = dir >= .4f && dir <= 1.f;
  }

  RefPtr<GamepadPlatformService> service =
      GamepadPlatformService::GetParentService();
  if (!service) {
    return;
  }

  service->NewButtonEvent(aIndex, BUTTON_INDEX_DPAD_UP, up);
  service->NewButtonEvent(aIndex, BUTTON_INDEX_DPAD_RIGHT, right);
  service->NewButtonEvent(aIndex, BUTTON_INDEX_DPAD_DOWN, down);
  service->NewButtonEvent(aIndex, BUTTON_INDEX_DPAD_LEFT, left);
}

class DefaultRemapper final : public GamepadRemapper {
 public:
  virtual uint32_t GetAxisCount() const override { return numAxes; }

  virtual uint32_t GetButtonCount() const override { return numButtons; }

  virtual void SetAxisCount(uint32_t aAxisCount) override {
    numAxes = aAxisCount;
  }

  virtual void SetButtonCount(uint32_t aButtonCount) override {
    numButtons = aButtonCount;
  }

  virtual GamepadMappingType GetMappingType() const override {
    return GamepadMappingType::_empty;
  }

  virtual void RemapAxisMoveEvent(uint32_t aIndex, uint32_t aAxis,
                                  double aValue) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }
    service->NewAxisMoveEvent(aIndex, aAxis, aValue);
  }

  virtual void RemapButtonEvent(uint32_t aIndex, uint32_t aButton,
                                bool aPressed) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }
    service->NewButtonEvent(aIndex, aButton, aPressed);
  }

 private:
  uint32_t numAxes;
  uint32_t numButtons;
};

class ADT1Remapper final : public GamepadRemapper {
 public:
  virtual uint32_t GetAxisCount() const override { return AXIS_INDEX_COUNT; }

  virtual uint32_t GetButtonCount() const override {
    return BUTTON_INDEX_COUNT;
  }

  virtual void RemapAxisMoveEvent(uint32_t aIndex, uint32_t aAxis,
                                  double aValue) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    switch (aAxis) {
      case 0:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_LEFT_STICK_X, aValue);
        break;
      case 1:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_LEFT_STICK_Y, aValue);
        break;
      case 2:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_RIGHT_STICK_X, aValue);
        break;
      case 3:
        service->NewButtonEvent(aIndex, BUTTON_INDEX_RIGHT_TRIGGER,
                                aValue > 0.1f);
        break;
      case 4:
        service->NewButtonEvent(aIndex, BUTTON_INDEX_LEFT_TRIGGER,
                                aValue > 0.1f);
        break;
      case 5:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_RIGHT_STICK_Y, aValue);
        break;
      case 9:
        FetchDpadFromAxis(aIndex, aValue);
        break;
      default:
        NS_WARNING(
            nsPrintfCString("Axis idx '%d' doesn't support in ADT1Remapper().",
                            aAxis)
                .get());
        break;
    }
  }

  virtual void RemapButtonEvent(uint32_t aIndex, uint32_t aButton,
                                bool aPressed) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    if (GetButtonCount() <= aIndex) {
      NS_WARNING(
          nsPrintfCString("Button idx '%d' doesn't support in ADT1Remapper().",
                          aButton)
              .get());
      return;
    }

    const std::map<uint32_t, uint32_t> buttonMapping = {
        {3, BUTTON_INDEX_TERTIARY},
        {4, BUTTON_INDEX_QUATERNARY},
        {6, BUTTON_INDEX_LEFT_SHOULDER},
        {7, BUTTON_INDEX_RIGHT_SHOULDER},
        {12, BUTTON_INDEX_META},
        {13, BUTTON_INDEX_LEFT_THUMBSTICK},
        {14, BUTTON_INDEX_RIGHT_THUMBSTICK}};

    auto find = buttonMapping.find(aButton);
    if (find != buttonMapping.end()) {
      service->NewButtonEvent(aIndex, find->second, aPressed);
    } else {
      service->NewButtonEvent(aIndex, aButton, aPressed);
    }
  }
};

class TwoAxesEightKeysRemapper final : public GamepadRemapper {
 public:
  virtual uint32_t GetAxisCount() const override { return 0; }

  virtual uint32_t GetButtonCount() const override {
    return BUTTON_INDEX_COUNT - 1;
  }

  virtual void RemapAxisMoveEvent(uint32_t aIndex, uint32_t aAxis,
                                  double aValue) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    switch (aAxis) {
      case 0:
        service->NewButtonEvent(aIndex, BUTTON_INDEX_DPAD_LEFT,
                                AxisNegativeAsButton(aValue));
        service->NewButtonEvent(aIndex, BUTTON_INDEX_DPAD_RIGHT,
                                AxisPositiveAsButton(aValue));
        break;
      case 1:
        service->NewButtonEvent(aIndex, BUTTON_INDEX_DPAD_UP,
                                AxisNegativeAsButton(aValue));
        service->NewButtonEvent(aIndex, BUTTON_INDEX_DPAD_DOWN,
                                AxisPositiveAsButton(aValue));
        break;
      default:
        NS_WARNING(
            nsPrintfCString(
                "Axis idx '%d' doesn't support in TwoAxesEightKeysRemapper().",
                aAxis)
                .get());
        break;
    }
  }

  virtual void RemapButtonEvent(uint32_t aIndex, uint32_t aButton,
                                bool aPressed) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    if (GetButtonCount() <= aIndex) {
      NS_WARNING(
          nsPrintfCString(
              "Button idx '%d' doesn't support in TwoAxesEightKeysRemapper().",
              aButton)
              .get());
      return;
    }

    const std::map<uint32_t, uint32_t> buttonMapping = {
        {0, BUTTON_INDEX_QUATERNARY},
        {2, BUTTON_INDEX_PRIMARY},
        {3, BUTTON_INDEX_TERTIARY}};

    auto find = buttonMapping.find(aButton);
    if (find != buttonMapping.end()) {
      service->NewButtonEvent(aIndex, find->second, aPressed);
    } else {
      service->NewButtonEvent(aIndex, aButton, aPressed);
    }
  }
};

class StadiaControllerRemapper final : public GamepadRemapper {
 public:
  virtual uint32_t GetAxisCount() const override { return AXIS_INDEX_COUNT; }

  virtual uint32_t GetButtonCount() const override {
    return STADIA_BUTTON_COUNT;
  }

  virtual void RemapAxisMoveEvent(uint32_t aIndex, uint32_t aAxis,
                                  double aValue) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    switch (aAxis) {
      case 0:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_LEFT_STICK_X, aValue);
        break;
      case 1:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_LEFT_STICK_Y, aValue);
        break;
      case 2:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_RIGHT_STICK_X, aValue);
        break;
      case 3:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_RIGHT_STICK_Y, aValue);
        break;
      case 4:
        service->NewButtonEvent(aIndex, BUTTON_INDEX_LEFT_TRIGGER, aValue);
        break;
      case 5:
        service->NewButtonEvent(aIndex, BUTTON_INDEX_RIGHT_TRIGGER, aValue);
        break;
      default:
        NS_WARNING(
            nsPrintfCString(
                "Axis idx '%d' doesn't support in StadiaControllerRemapper().",
                aAxis)
                .get());
        break;
    }
  }

  virtual void RemapButtonEvent(uint32_t aIndex, uint32_t aButton,
                                bool aPressed) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    if (STADIA_BUTTON_COUNT <= aIndex) {
      NS_WARNING(
          nsPrintfCString(
              "Button idx '%d' doesn't support in StadiaControllerRemapper().",
              aButton)
              .get());
      return;
    }

    service->NewButtonEvent(aIndex, aButton, aPressed);
  }

 private:
  enum STADIAButtons {
    STADIA_BUTTON_EXTRA1 = BUTTON_INDEX_COUNT,
    STADIA_BUTTON_EXTRA2,
    STADIA_BUTTON_COUNT
  };
};

class Dualshock4Remapper final : public GamepadRemapper {
 public:
  virtual uint32_t GetAxisCount() const override { return AXIS_INDEX_COUNT; }

  virtual uint32_t GetButtonCount() const override {
    return DUALSHOCK_BUTTON_COUNT;
  }

  virtual void RemapAxisMoveEvent(uint32_t aIndex, uint32_t aAxis,
                                  double aValue) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    switch (aAxis) {
      case 0:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_LEFT_STICK_X, aValue);
        break;
      case 1:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_LEFT_STICK_Y, aValue);
        break;
      case 2:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_RIGHT_STICK_X, aValue);
        break;
      case 3:
        service->NewButtonEvent(aIndex, BUTTON_INDEX_LEFT_TRIGGER,
                                aValue > 0.1f);
        break;
      case 4:
        service->NewButtonEvent(aIndex, BUTTON_INDEX_RIGHT_TRIGGER,
                                aValue > 0.1f);
        break;
      case 5:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_RIGHT_STICK_Y, aValue);
        break;
      case 9:
        FetchDpadFromAxis(aIndex, aValue);
        break;
      default:
        NS_WARNING(
            nsPrintfCString(
                "Axis idx '%d' doesn't support in Dualshock4Remapper().", aAxis)
                .get());
        break;
    }
  }

  virtual void RemapButtonEvent(uint32_t aIndex, uint32_t aButton,
                                bool aPressed) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    const std::vector<uint32_t> buttonMapping = {BUTTON_INDEX_TERTIARY,
                                                 BUTTON_INDEX_PRIMARY,
                                                 BUTTON_INDEX_SECONDARY,
                                                 BUTTON_INDEX_QUATERNARY,
                                                 BUTTON_INDEX_LEFT_SHOULDER,
                                                 BUTTON_INDEX_RIGHT_SHOULDER,
                                                 BUTTON_INDEX_LEFT_TRIGGER,
                                                 BUTTON_INDEX_RIGHT_TRIGGER,
                                                 BUTTON_INDEX_BACK_SELECT,
                                                 BUTTON_INDEX_START,
                                                 BUTTON_INDEX_LEFT_THUMBSTICK,
                                                 BUTTON_INDEX_RIGHT_THUMBSTICK,
                                                 BUTTON_INDEX_META,
                                                 DUALSHOCK_BUTTON_TOUCHPAD};

    if (buttonMapping.size() <= aIndex) {
      NS_WARNING(nsPrintfCString(
                     "Button idx '%d' doesn't support in Dualshock4Remapper().",
                     aButton)
                     .get());
      return;
    }

    service->NewButtonEvent(aIndex, buttonMapping[aButton], aPressed);
  }

 private:
  enum Dualshock4Buttons {
    DUALSHOCK_BUTTON_TOUCHPAD = BUTTON_INDEX_COUNT,
    DUALSHOCK_BUTTON_COUNT
  };
};

class LogitechDInputRemapper final : public GamepadRemapper {
 public:
  virtual uint32_t GetAxisCount() const override { return AXIS_INDEX_COUNT; }

  virtual uint32_t GetButtonCount() const override {
    // The Logitech button (BUTTON_INDEX_META) is not accessible through the
    // device's D-mode.
    return BUTTON_INDEX_COUNT - 1;
  }

  virtual void RemapAxisMoveEvent(uint32_t aIndex, uint32_t aAxis,
                                  double aValue) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    switch (aAxis) {
      case 0:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_LEFT_STICK_X, aValue);
        break;
      case 1:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_LEFT_STICK_Y, aValue);
        break;
      case 2:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_RIGHT_STICK_X, aValue);
        break;
      case 5:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_RIGHT_STICK_Y, aValue);
        break;
      case 9:
        FetchDpadFromAxis(aIndex, aValue);
        break;
      default:
        NS_WARNING(
            nsPrintfCString(
                "Axis idx '%d' doesn't support in LogitechDInputRemapper().",
                aAxis)
                .get());
        break;
    }
  }

  virtual void RemapButtonEvent(uint32_t aIndex, uint32_t aButton,
                                bool aPressed) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    if (GetButtonCount() <= aIndex) {
      NS_WARNING(
          nsPrintfCString(
              "Button idx '%d' doesn't support in LogitechDInputRemapper().",
              aButton)
              .get());
      return;
    }

    const std::map<uint32_t, uint32_t> buttonMapping = {
        {0, BUTTON_INDEX_TERTIARY},
        {1, BUTTON_INDEX_PRIMARY},
        {2, BUTTON_INDEX_SECONDARY}};

    auto find = buttonMapping.find(aButton);
    if (find != buttonMapping.end()) {
      service->NewButtonEvent(aIndex, find->second, aPressed);
    } else {
      service->NewButtonEvent(aIndex, aButton, aPressed);
    }
  }
};

class SwitchJoyConRemapper final : public GamepadRemapper {
 public:
  virtual uint32_t GetAxisCount() const override { return 2; }

  virtual uint32_t GetButtonCount() const override {
    return BUTTON_INDEX_COUNT;
  }

  virtual void RemapAxisMoveEvent(uint32_t aIndex, uint32_t aAxis,
                                  double aValue) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    service->NewAxisMoveEvent(aIndex, AXIS_INDEX_LEFT_STICK_X, aValue);
  }

  virtual void RemapButtonEvent(uint32_t aIndex, uint32_t aButton,
                                bool aPressed) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    service->NewButtonEvent(aIndex, aButton, aPressed);
  }
};

class SwitchProRemapper final : public GamepadRemapper {
 public:
  virtual uint32_t GetAxisCount() const override { return AXIS_INDEX_COUNT; }

  virtual uint32_t GetButtonCount() const override {
    // The Switch Pro controller has a Capture button that has no equivalent in
    // the Standard Gamepad.
    return SWITCHPRO_BUTTON_COUNT;
  }

  virtual void RemapAxisMoveEvent(uint32_t aIndex, uint32_t aAxis,
                                  double aValue) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    service->NewAxisMoveEvent(aIndex, AXIS_INDEX_LEFT_STICK_X, aValue);
  }

  virtual void RemapButtonEvent(uint32_t aIndex, uint32_t aButton,
                                bool aPressed) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    service->NewButtonEvent(aIndex, aButton, aPressed);
  }

 private:
  enum SwitchProButtons {
    SWITCHPRO_BUTTON_EXTRA = BUTTON_INDEX_COUNT,
    SWITCHPRO_BUTTON_COUNT
  };
};

class NvShieldRemapper final : public GamepadRemapper {
 public:
  virtual uint32_t GetAxisCount() const override { return AXIS_INDEX_COUNT; }

  virtual uint32_t GetButtonCount() const override {
    return SHIELD_BUTTON_COUNT;
  }

  virtual void RemapAxisMoveEvent(uint32_t aIndex, uint32_t aAxis,
                                  double aValue) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    switch (aAxis) {
      case 0:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_LEFT_STICK_X, aValue);
        break;
      case 1:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_LEFT_STICK_Y, aValue);
        break;
      case 2:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_RIGHT_STICK_X, aValue);
        break;
      case 3:
        service->NewButtonEvent(aIndex, BUTTON_INDEX_RIGHT_TRIGGER,
                                aValue > 0.1f);
        break;
      case 4:
        service->NewButtonEvent(aIndex, BUTTON_INDEX_LEFT_TRIGGER,
                                aValue > 0.1f);
        break;
      case 5:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_RIGHT_STICK_Y, aValue);
        break;
      case 9:
        FetchDpadFromAxis(aIndex, aValue);
        break;
      default:
        NS_WARNING(
            nsPrintfCString(
                "Axis idx '%d' doesn't support in NvShieldRemapper().", aAxis)
                .get());
        break;
    }
  }

  virtual void RemapButtonEvent(uint32_t aIndex, uint32_t aButton,
                                bool aPressed) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    if (GetButtonCount() <= aIndex) {
      NS_WARNING(
          nsPrintfCString(
              "Button idx '%d' doesn't support in NvShieldRemapper().", aButton)
              .get());
      return;
    }

    const std::map<uint32_t, uint32_t> buttonMapping = {
        {2, BUTTON_INDEX_META},
        {3, BUTTON_INDEX_TERTIARY},
        {4, BUTTON_INDEX_QUATERNARY},
        {5, SHIELD_BUTTON_CIRCLE},
        {6, BUTTON_INDEX_LEFT_SHOULDER},
        {7, BUTTON_INDEX_RIGHT_SHOULDER},
        {9, BUTTON_INDEX_BACK_SELECT},
        {11, BUTTON_INDEX_START},
        {13, BUTTON_INDEX_LEFT_THUMBSTICK},
        {14, BUTTON_INDEX_RIGHT_THUMBSTICK}};

    auto find = buttonMapping.find(aButton);
    if (find != buttonMapping.end()) {
      service->NewButtonEvent(aIndex, find->second, aPressed);
    } else {
      service->NewButtonEvent(aIndex, aButton, aPressed);
    }
  }

 private:
  enum ShieldButtons {
    SHIELD_BUTTON_CIRCLE = BUTTON_INDEX_COUNT,
    SHIELD_BUTTON_COUNT
  };
};

class NvShield2017Remapper final : public GamepadRemapper {
 public:
  virtual uint32_t GetAxisCount() const override { return AXIS_INDEX_COUNT; }

  virtual uint32_t GetButtonCount() const override {
    return SHIELD2017_BUTTON_COUNT;
  }

  virtual void RemapAxisMoveEvent(uint32_t aIndex, uint32_t aAxis,
                                  double aValue) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    switch (aAxis) {
      case 0:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_LEFT_STICK_X, aValue);
        break;
      case 1:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_LEFT_STICK_Y, aValue);
        break;
      case 2:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_RIGHT_STICK_X, aValue);
        break;
      case 3:
        service->NewButtonEvent(aIndex, BUTTON_INDEX_RIGHT_TRIGGER,
                                aValue > 0.1f);
        break;
      case 4:
        service->NewButtonEvent(aIndex, BUTTON_INDEX_LEFT_TRIGGER,
                                aValue > 0.1f);
        break;
      case 5:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_RIGHT_STICK_Y, aValue);
        break;
      case 9:
        FetchDpadFromAxis(aIndex, aValue);
        break;
      default:
        NS_WARNING(
            nsPrintfCString(
                "Axis idx '%d' doesn't support in NvShield2017Remapper().",
                aAxis)
                .get());
        break;
    }
  }

  virtual void RemapButtonEvent(uint32_t aIndex, uint32_t aButton,
                                bool aPressed) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    if (GetButtonCount() <= aIndex) {
      NS_WARNING(
          nsPrintfCString(
              "Button idx '%d' doesn't support in NvShield2017Remapper().",
              aButton)
              .get());
      return;
    }

    const std::map<uint32_t, uint32_t> buttonMapping = {
        {2, BUTTON_INDEX_META},
        {3, BUTTON_INDEX_TERTIARY},
        {4, BUTTON_INDEX_QUATERNARY},
        {5, BUTTON_INDEX_START},
        {6, BUTTON_INDEX_LEFT_SHOULDER},
        {7, BUTTON_INDEX_RIGHT_SHOULDER},
        {8, BUTTON_INDEX_BACK_SELECT},
        {11, SHIELD2017_BUTTON_PLAYPAUSE},
        {13, BUTTON_INDEX_LEFT_THUMBSTICK},
        {14, BUTTON_INDEX_RIGHT_THUMBSTICK}};

    auto find = buttonMapping.find(aButton);
    if (find != buttonMapping.end()) {
      service->NewButtonEvent(aIndex, find->second, aPressed);
    } else {
      service->NewButtonEvent(aIndex, aButton, aPressed);
    }
  }

 private:
  enum Shield2017Buttons {
    SHIELD2017_BUTTON_PLAYPAUSE = BUTTON_INDEX_COUNT,
    SHIELD2017_BUTTON_COUNT
  };
};

class IBuffaloRemapper final : public GamepadRemapper {
 public:
  virtual uint32_t GetAxisCount() const override { return 2; }

  virtual uint32_t GetButtonCount() const override {
    return BUTTON_INDEX_COUNT - 1; /* no meta */
  }

  virtual void RemapAxisMoveEvent(uint32_t aIndex, uint32_t aAxis,
                                  double aValue) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    switch (aAxis) {
      case 0:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_LEFT_STICK_X, aValue);
        service->NewButtonEvent(aIndex, BUTTON_INDEX_DPAD_LEFT,
                                AxisNegativeAsButton(aValue));
        service->NewButtonEvent(aIndex, BUTTON_INDEX_DPAD_RIGHT,
                                AxisPositiveAsButton(aValue));
        break;
      case 1:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_LEFT_STICK_Y, aValue);
        service->NewButtonEvent(aIndex, BUTTON_INDEX_DPAD_UP,
                                AxisNegativeAsButton(aValue));
        service->NewButtonEvent(aIndex, BUTTON_INDEX_DPAD_DOWN,
                                AxisPositiveAsButton(aValue));
        break;
      default:
        NS_WARNING(
            nsPrintfCString(
                "Axis idx '%d' doesn't support in IBuffaloRemapper().", aAxis)
                .get());
        break;
    }
  }

  virtual void RemapButtonEvent(uint32_t aIndex, uint32_t aButton,
                                bool aPressed) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    if (GetButtonCount() <= aIndex) {
      NS_WARNING(
          nsPrintfCString(
              "Button idx '%d' doesn't support in IBuffaloRemapper().", aButton)
              .get());
      return;
    }

    const std::map<uint32_t, uint32_t> buttonMapping = {
        {0, BUTTON_INDEX_SECONDARY},     {1, BUTTON_INDEX_PRIMARY},
        {2, BUTTON_INDEX_QUATERNARY},    {3, BUTTON_INDEX_TERTIARY},
        {5, BUTTON_INDEX_RIGHT_TRIGGER}, {6, BUTTON_INDEX_BACK_SELECT},
        {7, BUTTON_INDEX_START}};

    auto find = buttonMapping.find(aButton);
    if (find != buttonMapping.end()) {
      service->NewButtonEvent(aIndex, find->second, aPressed);
    } else {
      service->NewButtonEvent(aIndex, aButton, aPressed);
    }
  }
};

class XSkillsRemapper final : public GamepadRemapper {
 public:
  virtual uint32_t GetAxisCount() const override { return AXIS_INDEX_COUNT; }

  virtual uint32_t GetButtonCount() const override {
    return GAMECUBE_BUTTON_COUNT;
  }

  virtual void RemapAxisMoveEvent(uint32_t aIndex, uint32_t aAxis,
                                  double aValue) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    switch (aAxis) {
      case 0:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_LEFT_STICK_X, aValue);
        break;
      case 1:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_LEFT_STICK_Y, aValue);
        break;
      case 2:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_RIGHT_STICK_Y, aValue);
        break;
      case 3:
        service->NewButtonEvent(aIndex, BUTTON_INDEX_RIGHT_TRIGGER,
                                aValue > 0.1f);
        break;
      case 4:
        service->NewButtonEvent(aIndex, BUTTON_INDEX_LEFT_TRIGGER,
                                aValue > 0.1f);
        break;
      case 5:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_RIGHT_STICK_X, aValue);
        break;
      default:
        NS_WARNING(
            nsPrintfCString(
                "Axis idx '%d' doesn't support in XSkillsRemapper().", aAxis)
                .get());
        break;
    }
  }

  virtual void RemapButtonEvent(uint32_t aIndex, uint32_t aButton,
                                bool aPressed) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    if (GetButtonCount() <= aIndex) {
      NS_WARNING(
          nsPrintfCString(
              "Button idx '%d' doesn't support in XSkillsRemapper().", aButton)
              .get());
      return;
    }

    const std::map<uint32_t, uint32_t> buttonMapping = {
        {0, BUTTON_INDEX_PRIMARY},     // A
        {1, BUTTON_INDEX_TERTIARY},    // B
        {2, BUTTON_INDEX_SECONDARY},   // X
        {3, BUTTON_INDEX_QUATERNARY},  // Y
        {4, GAMECUBE_BUTTON_LEFT_TRIGGER_CLICK},
        {5, GAMECUBE_BUTTON_RIGHT_TRIGGER_CLICK},
        {6, BUTTON_INDEX_RIGHT_SHOULDER},
        {7, BUTTON_INDEX_START},
        {8, BUTTON_INDEX_DPAD_LEFT},
        {9, BUTTON_INDEX_DPAD_RIGHT},
        {10, BUTTON_INDEX_DPAD_DOWN},
        {11, BUTTON_INDEX_DPAD_UP}};

    auto find = buttonMapping.find(aButton);
    if (find != buttonMapping.end()) {
      service->NewButtonEvent(aIndex, find->second, aPressed);
    } else {
      service->NewButtonEvent(aIndex, aButton, aPressed);
    }
  }

 private:
  enum GamecubeButtons {
    GAMECUBE_BUTTON_LEFT_TRIGGER_CLICK = BUTTON_INDEX_COUNT,
    GAMECUBE_BUTTON_RIGHT_TRIGGER_CLICK,
    GAMECUBE_BUTTON_COUNT
  };
};

class BoomN64PsxRemapper final : public GamepadRemapper {
 public:
  virtual uint32_t GetAxisCount() const override { return AXIS_INDEX_COUNT; }

  virtual uint32_t GetButtonCount() const override {
    return BUTTON_INDEX_COUNT - 1;  // no meta
  }

  virtual void RemapAxisMoveEvent(uint32_t aIndex, uint32_t aAxis,
                                  double aValue) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    switch (aAxis) {
      case 0:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_LEFT_STICK_X, aValue);
        break;
      case 1:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_LEFT_STICK_Y, aValue);
        break;
      case 2:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_RIGHT_STICK_X, aValue);
        break;
      case 5:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_RIGHT_STICK_Y, aValue);
        break;
      default:
        NS_WARNING(
            nsPrintfCString(
                "Axis idx '%d' doesn't support in BoomN64PsxRemapper().", aAxis)
                .get());
        break;
    }
  }

  virtual void RemapButtonEvent(uint32_t aIndex, uint32_t aButton,
                                bool aPressed) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    const std::vector<uint32_t> buttonMapping = {
        BUTTON_INDEX_QUATERNARY,       BUTTON_INDEX_SECONDARY,
        BUTTON_INDEX_PRIMARY,          BUTTON_INDEX_TERTIARY,
        BUTTON_INDEX_LEFT_TRIGGER,     BUTTON_INDEX_RIGHT_TRIGGER,
        BUTTON_INDEX_LEFT_SHOULDER,    BUTTON_INDEX_RIGHT_SHOULDER,
        BUTTON_INDEX_BACK_SELECT,      BUTTON_INDEX_LEFT_THUMBSTICK,
        BUTTON_INDEX_RIGHT_THUMBSTICK, BUTTON_INDEX_START,
        BUTTON_INDEX_DPAD_UP,          BUTTON_INDEX_DPAD_RIGHT,
        BUTTON_INDEX_DPAD_DOWN,        BUTTON_INDEX_DPAD_LEFT};

    if (buttonMapping.size() <= aIndex) {
      NS_WARNING(nsPrintfCString(
                     "Button idx '%d' doesn't support in BoomN64PsxRemapper().",
                     aButton)
                     .get());
      return;
    }

    service->NewButtonEvent(aIndex, buttonMapping[aButton], aPressed);
  }

 private:
  enum GamecubeButtons {
    GAMECUBE_BUTTON_LEFT_TRIGGER_CLICK = BUTTON_INDEX_COUNT,
    GAMECUBE_BUTTON_RIGHT_TRIGGER_CLICK,
    GAMECUBE_BUTTON_COUNT
  };
};

class AnalogGamepadRemapper final : public GamepadRemapper {
 public:
  virtual uint32_t GetAxisCount() const override { return AXIS_INDEX_COUNT; }

  virtual uint32_t GetButtonCount() const override {
    return ANALOG_GAMEPAD_BUTTON_COUNT;
  }

  virtual void RemapAxisMoveEvent(uint32_t aIndex, uint32_t aAxis,
                                  double aValue) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    switch (aAxis) {
      case 0:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_LEFT_STICK_X, aValue);
        break;
      case 1:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_LEFT_STICK_Y, aValue);
        break;
      case 2:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_RIGHT_STICK_X, aValue);
        break;
      case 3:
        service->NewButtonEvent(aIndex, BUTTON_INDEX_RIGHT_TRIGGER,
                                aValue > 0.1f);
        break;
      case 4:
        service->NewButtonEvent(aIndex, BUTTON_INDEX_LEFT_TRIGGER,
                                aValue > 0.1f);
        break;
      case 5:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_RIGHT_STICK_Y, aValue);
        break;
      case 9:
        FetchDpadFromAxis(aIndex, aValue);
        break;
      default:
        NS_WARNING(
            nsPrintfCString(
                "Axis idx '%d' doesn't support in AnalogGamepadRemapper().",
                aAxis)
                .get());
        break;
    }
  }

  virtual void RemapButtonEvent(uint32_t aIndex, uint32_t aButton,
                                bool aPressed) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    if (GetButtonCount() <= aIndex) {
      NS_WARNING(
          nsPrintfCString(
              "Button idx '%d' doesn't support in AnalogGamepadRemapper().",
              aButton)
              .get());
      return;
    }

    const std::map<uint32_t, uint32_t> buttonMapping = {
        {3, BUTTON_INDEX_TERTIARY},
        {4, BUTTON_INDEX_QUATERNARY},
        {6, BUTTON_INDEX_LEFT_SHOULDER},
        {7, BUTTON_INDEX_RIGHT_SHOULDER},
        {10, BUTTON_INDEX_BACK_SELECT},
        {11, BUTTON_INDEX_META},
        {12, BUTTON_INDEX_START},
        {13, BUTTON_INDEX_LEFT_THUMBSTICK},
        {14, BUTTON_INDEX_RIGHT_THUMBSTICK},
        {16, ANALOG_GAMEPAD_BUTTON_EXTRA},
        {17, ANALOG_GAMEPAD_BUTTON_EXTRA2}};

    auto find = buttonMapping.find(aButton);
    if (find != buttonMapping.end()) {
      service->NewButtonEvent(aIndex, find->second, aPressed);
    } else {
      service->NewButtonEvent(aIndex, aButton, aPressed);
    }
  }

 private:
  enum AnalogGamepadButtons {
    ANALOG_GAMEPAD_BUTTON_EXTRA = BUTTON_INDEX_COUNT,
    ANALOG_GAMEPAD_BUTTON_EXTRA2,
    ANALOG_GAMEPAD_BUTTON_COUNT
  };
};

class RazerServalRemapper final : public GamepadRemapper {
 public:
  virtual uint32_t GetAxisCount() const override { return AXIS_INDEX_COUNT; }

  virtual uint32_t GetButtonCount() const override {
    return BUTTON_INDEX_COUNT - 1; /* no meta */
  }

  virtual void RemapAxisMoveEvent(uint32_t aIndex, uint32_t aAxis,
                                  double aValue) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    switch (aAxis) {
      case 0:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_LEFT_STICK_X, aValue);
        break;
      case 1:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_LEFT_STICK_Y, aValue);
        break;
      case 2:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_RIGHT_STICK_X, aValue);
        break;
      case 3:
        service->NewButtonEvent(aIndex, BUTTON_INDEX_RIGHT_TRIGGER,
                                aValue > 0.1f);
        break;
      case 4:
        service->NewButtonEvent(aIndex, BUTTON_INDEX_LEFT_TRIGGER,
                                aValue > 0.1f);
        break;
      case 5:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_RIGHT_STICK_Y, aValue);
        break;
      case 9:
        FetchDpadFromAxis(aIndex, aValue);
        break;
      default:
        NS_WARNING(
            nsPrintfCString(
                "Axis idx '%d' doesn't support in RazerServalRemapper().",
                aAxis)
                .get());
        break;
    }
  }

  virtual void RemapButtonEvent(uint32_t aIndex, uint32_t aButton,
                                bool aPressed) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    if (GetButtonCount() <= aIndex) {
      NS_WARNING(
          nsPrintfCString(
              "Button idx '%d' doesn't support in RazerServalRemapper().",
              aButton)
              .get());
      return;
    }

    const std::map<uint32_t, uint32_t> buttonMapping = {
        {3, BUTTON_INDEX_TERTIARY},         {4, BUTTON_INDEX_QUATERNARY},
        {6, BUTTON_INDEX_LEFT_SHOULDER},    {7, BUTTON_INDEX_RIGHT_SHOULDER},
        {10, BUTTON_INDEX_BACK_SELECT},     {11, BUTTON_INDEX_START},
        {12, BUTTON_INDEX_START},           {13, BUTTON_INDEX_LEFT_THUMBSTICK},
        {14, BUTTON_INDEX_RIGHT_THUMBSTICK}};

    auto find = buttonMapping.find(aButton);
    if (find != buttonMapping.end()) {
      service->NewButtonEvent(aIndex, find->second, aPressed);
    } else {
      service->NewButtonEvent(aIndex, aButton, aPressed);
    }
  }
};

class MogaProRemapper final : public GamepadRemapper {
 public:
  virtual uint32_t GetAxisCount() const override { return AXIS_INDEX_COUNT; }

  virtual uint32_t GetButtonCount() const override {
    return BUTTON_INDEX_COUNT - 1; /* no meta */
  }

  virtual void RemapAxisMoveEvent(uint32_t aIndex, uint32_t aAxis,
                                  double aValue) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    switch (aAxis) {
      case 0:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_LEFT_STICK_X, aValue);
        break;
      case 1:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_LEFT_STICK_Y, aValue);
        break;
      case 2:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_RIGHT_STICK_X, aValue);
        break;
      case 3:
        service->NewButtonEvent(aIndex, BUTTON_INDEX_RIGHT_TRIGGER,
                                aValue > 0.1f);
        break;
      case 4:
        service->NewButtonEvent(aIndex, BUTTON_INDEX_LEFT_TRIGGER,
                                aValue > 0.1f);
        break;
      case 5:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_RIGHT_STICK_Y, aValue);
        break;
      case 9:
        FetchDpadFromAxis(aIndex, aValue);
        break;
      default:
        NS_WARNING(
            nsPrintfCString(
                "Axis idx '%d' doesn't support in MogaProRemapper().", aAxis)
                .get());
        break;
    }
  }

  virtual void RemapButtonEvent(uint32_t aIndex, uint32_t aButton,
                                bool aPressed) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    if (GetButtonCount() <= aIndex) {
      NS_WARNING(
          nsPrintfCString(
              "Button idx '%d' doesn't support in MogaProRemapper().", aButton)
              .get());
      return;
    }

    const std::map<uint32_t, uint32_t> buttonMapping = {
        {3, BUTTON_INDEX_TERTIARY},         {4, BUTTON_INDEX_QUATERNARY},
        {6, BUTTON_INDEX_LEFT_SHOULDER},    {7, BUTTON_INDEX_RIGHT_SHOULDER},
        {11, BUTTON_INDEX_START},           {13, BUTTON_INDEX_LEFT_THUMBSTICK},
        {14, BUTTON_INDEX_RIGHT_THUMBSTICK}};

    auto find = buttonMapping.find(aButton);
    if (find != buttonMapping.end()) {
      service->NewButtonEvent(aIndex, find->second, aPressed);
    } else {
      service->NewButtonEvent(aIndex, aButton, aPressed);
    }
  }
};

class OnLiveWirelessRemapper final : public GamepadRemapper {
 public:
  virtual uint32_t GetAxisCount() const override { return AXIS_INDEX_COUNT; }

  virtual uint32_t GetButtonCount() const override {
    return BUTTON_INDEX_COUNT;
  }

  virtual void RemapAxisMoveEvent(uint32_t aIndex, uint32_t aAxis,
                                  double aValue) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    switch (aAxis) {
      case 0:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_LEFT_STICK_X, aValue);
        break;
      case 1:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_LEFT_STICK_Y, aValue);
        break;
      case 2:
        service->NewButtonEvent(aIndex, BUTTON_INDEX_LEFT_TRIGGER,
                                aValue > 0.1f);
        break;
      case 3:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_RIGHT_STICK_X, aValue);
        break;
      case 4:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_RIGHT_STICK_Y, aValue);
        break;
      case 5:
        service->NewButtonEvent(aIndex, BUTTON_INDEX_RIGHT_TRIGGER,
                                aValue > 0.1f);
        break;
      case 9:
        FetchDpadFromAxis(aIndex, aValue);
        break;
      default:
        NS_WARNING(
            nsPrintfCString(
                "Axis idx '%d' doesn't support in OnLiveWirelessRemapper().",
                aAxis)
                .get());
        break;
    }
  }

  virtual void RemapButtonEvent(uint32_t aIndex, uint32_t aButton,
                                bool aPressed) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    if (GetButtonCount() <= aIndex) {
      NS_WARNING(
          nsPrintfCString(
              "Button idx '%d' doesn't support in OnLiveWirelessRemapper().",
              aButton)
              .get());
      return;
    }

    const std::map<uint32_t, uint32_t> buttonMapping = {
        {3, BUTTON_INDEX_TERTIARY},
        {4, BUTTON_INDEX_QUATERNARY},
        {6, BUTTON_INDEX_LEFT_SHOULDER},
        {7, BUTTON_INDEX_RIGHT_SHOULDER},
        {12, BUTTON_INDEX_META},
        {13, BUTTON_INDEX_LEFT_THUMBSTICK},
        {14, BUTTON_INDEX_RIGHT_THUMBSTICK}};

    auto find = buttonMapping.find(aButton);
    if (find != buttonMapping.end()) {
      service->NewButtonEvent(aIndex, find->second, aPressed);
    } else {
      service->NewButtonEvent(aIndex, aButton, aPressed);
    }
  }
};

class OUYARemapper final : public GamepadRemapper {
 public:
  virtual uint32_t GetAxisCount() const override { return AXIS_INDEX_COUNT; }

  virtual uint32_t GetButtonCount() const override {
    return BUTTON_INDEX_COUNT;
  }

  virtual void RemapAxisMoveEvent(uint32_t aIndex, uint32_t aAxis,
                                  double aValue) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    switch (aAxis) {
      case 0:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_LEFT_STICK_X, aValue);
        break;
      case 1:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_LEFT_STICK_Y, aValue);
        break;
      case 2:
        service->NewButtonEvent(aIndex, BUTTON_INDEX_LEFT_TRIGGER,
                                aValue > 0.1f);
        break;
      case 3:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_RIGHT_STICK_X, aValue);
        break;
      case 4:
        service->NewAxisMoveEvent(aIndex, AXIS_INDEX_RIGHT_STICK_Y, aValue);
        break;
      case 5:
        service->NewButtonEvent(aIndex, BUTTON_INDEX_RIGHT_TRIGGER,
                                aValue > 0.1f);
        break;
      default:
        NS_WARNING(
            nsPrintfCString("Axis idx '%d' doesn't support in OUYARemapper().",
                            aAxis)
                .get());
        break;
    }
  }

  virtual void RemapButtonEvent(uint32_t aIndex, uint32_t aButton,
                                bool aPressed) const override {
    RefPtr<GamepadPlatformService> service =
        GamepadPlatformService::GetParentService();
    if (!service) {
      return;
    }

    if (GetButtonCount() <= aIndex) {
      NS_WARNING(
          nsPrintfCString("Button idx '%d' doesn't support in OUYARemapper().",
                          aButton)
              .get());
      return;
    }

    const std::map<uint32_t, uint32_t> buttonMapping = {
        {1, BUTTON_INDEX_TERTIARY},         {2, BUTTON_INDEX_QUATERNARY},
        {3, BUTTON_INDEX_SECONDARY},        {6, BUTTON_INDEX_LEFT_THUMBSTICK},
        {7, BUTTON_INDEX_RIGHT_THUMBSTICK}, {8, BUTTON_INDEX_DPAD_UP},
        {9, BUTTON_INDEX_DPAD_DOWN},        {10, BUTTON_INDEX_DPAD_LEFT},
        {11, BUTTON_INDEX_DPAD_RIGHT},      {15, BUTTON_INDEX_META}};

    auto find = buttonMapping.find(aButton);
    if (find != buttonMapping.end()) {
      service->NewButtonEvent(aIndex, find->second, aPressed);
    } else {
      service->NewButtonEvent(aIndex, aButton, aPressed);
    }
  }
};

already_AddRefed<GamepadRemapper> GetGamepadRemapper(
    const uint16_t aVendorId, const uint16_t aProductId) {
  const std::vector<GamepadRemappingData> remappingRules = {
      {GamepadId::kAsusTekProduct4500, new ADT1Remapper()},
      {GamepadId::kDragonRiseProduct0011, new TwoAxesEightKeysRemapper()},
      {GamepadId::kGoogleProduct2c40, new ADT1Remapper()},
      {GamepadId::kGoogleProduct9400, new StadiaControllerRemapper()},
      {GamepadId::kLogitechProductc216, new LogitechDInputRemapper()},
      {GamepadId::kLogitechProductc218, new LogitechDInputRemapper()},
      {GamepadId::kLogitechProductc219, new LogitechDInputRemapper()},
      {GamepadId::kNintendoProduct2006, new SwitchJoyConRemapper()},
      {GamepadId::kNintendoProduct2007, new SwitchJoyConRemapper()},
      {GamepadId::kNintendoProduct2009, new SwitchProRemapper()},
      {GamepadId::kNintendoProduct200e, new SwitchProRemapper()},
      {GamepadId::kNvidiaProduct7210, new NvShieldRemapper()},
      {GamepadId::kNvidiaProduct7214, new NvShield2017Remapper()},
      {GamepadId::kPadixProduct2060, new IBuffaloRemapper()},
      {GamepadId::kPlayComProduct0005, new XSkillsRemapper()},
      {GamepadId::kPrototypeVendorProduct0667, new BoomN64PsxRemapper()},
      {GamepadId::kPrototypeVendorProduct9401, new AnalogGamepadRemapper()},
      {GamepadId::kRazer1532Product0900, new RazerServalRemapper()},
      {GamepadId::kSonyProduct05c4, new Dualshock4Remapper()},
      {GamepadId::kSonyProduct09cc, new Dualshock4Remapper()},
      {GamepadId::kSonyProduct0ba0, new Dualshock4Remapper()},
      {GamepadId::kVendor20d6Product6271, new MogaProRemapper()},
      {GamepadId::kVendor2378Product1008, new OnLiveWirelessRemapper()},
      {GamepadId::kVendor2378Product100a, new OnLiveWirelessRemapper()},
      {GamepadId::kVendor2836Product0001, new OUYARemapper()}};
  const GamepadId id = static_cast<GamepadId>((aVendorId << 16) | aProductId);

  for (uint32_t i = 0; i < remappingRules.size(); ++i) {
    if (id == remappingRules[i].id) {
      return do_AddRef(remappingRules[i].remapping.get());
    }
  }

  static RefPtr<GamepadRemapper> defaultRemapper = new DefaultRemapper();
  return do_AddRef(defaultRemapper.get());
}

}  // namespace dom
}  // namespace mozilla
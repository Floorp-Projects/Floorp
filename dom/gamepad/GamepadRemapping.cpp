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
    virtual uint32_t GetAxisCount() const override {
      return numAxes;
    }

    virtual uint32_t GetButtonCount() const override {
      return numButtons;
    }

    virtual void SetAxisCount(uint32_t aAxisCount) override {
      numAxes = aAxisCount;
    }

    virtual void SetButtonCount(uint32_t aButtonCount) override {
      numButtons = aButtonCount;
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

class Dualshock4Remapper final : public GamepadRemapper {
 public:
  virtual uint32_t GetAxisCount() const override { return AXIS_INDEX_COUNT; }

  virtual uint32_t GetButtonCount() const override {
    return DUALSHOCK_BUTTON_COUNT;
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

    const std::vector<uint32_t> buttonMapping = {
      BUTTON_INDEX_TERTIARY,
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
      DUALSHOCK_BUTTON_TOUCHPAD
    };

    if (buttonMapping.size() <= aIndex) {
      NS_WARNING(
            nsPrintfCString(
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

already_AddRefed<GamepadRemapper> GetGamepadRemapper(const uint16_t aVendorId,
                                                     const uint16_t aProductId) {
  const std::vector<GamepadRemappingData> remappingRules = {
      {GamepadId::kSonyDualshock4, new Dualshock4Remapper()},
      {GamepadId::kSonyDualshock4Slim, new Dualshock4Remapper()},
      {GamepadId::kSonyDualshock4USBReceiver, new Dualshock4Remapper()}
  };
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
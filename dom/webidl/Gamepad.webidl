/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

[Pref="dom.gamepad.enabled"]
interface GamepadButton {
  readonly    attribute boolean pressed;
  readonly    attribute double  value;
};

enum GamepadMappingType {
  "",
  "standard"
};

[Pref="dom.gamepad.enabled"]
interface Gamepad {
  /**
   * An identifier, unique per type of device.
   */
  readonly attribute DOMString id;

  /**
   * The game port index for the device. Unique per device
   * attached to this system.
   */
  readonly attribute unsigned long index;

  /**
   * The mapping in use for this device. The empty string
   * indicates that no mapping is in use.
   */
  readonly attribute GamepadMappingType mapping;

  /**
   * true if this gamepad is currently connected to the system.
   */
  readonly attribute boolean connected;

  /**
   * The current state of all buttons on the device, an
   * array of GamepadButton.
   */
  [Pure, Cached, Frozen]
  readonly attribute sequence<GamepadButton> buttons;

  /**
   * The current position of all axes on the device, an
   * array of doubles.
   */
  [Pure, Cached, Frozen]
  readonly attribute sequence<double> axes;

  /**
   * Timestamp from when the data of this device was last updated.
   */
  readonly attribute DOMHighResTimeStamp timestamp;

  /**
   * The current pose of the device, a GamepadPose.
   */
  [Pref="dom.gamepad.extensions.enabled"]
  readonly attribute GamepadPose? pose;
};

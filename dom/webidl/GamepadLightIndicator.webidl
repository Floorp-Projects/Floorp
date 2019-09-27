/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://github.com/knyg/gamepad/blob/lightindicator/extensions.html
 */

enum GamepadLightIndicatorType {
  "on-off",
  "rgb"
};

dictionary GamepadLightColor {
  required octet red;
  required octet green;
  required octet blue;
};

[SecureContext, Pref="dom.gamepad.extensions.lightindicator",
 Exposed=Window]
interface GamepadLightIndicator
{
  readonly attribute GamepadLightIndicatorType type;
  [Throws, NewObject]
  Promise<boolean> setColor(GamepadLightColor color);
};

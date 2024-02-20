/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://github.com/knyg/gamepad/blob/multitouch/extensions.html
 */

[Pref="dom.gamepad.extensions.multitouch",
 Exposed=Window]
interface GamepadTouch {
  readonly attribute unsigned long touchId;
  readonly attribute octet surfaceId;
  [Constant, Throws] readonly attribute Float32Array position;
  [Constant, Throws] readonly attribute Uint32Array? surfaceDimensions;
};

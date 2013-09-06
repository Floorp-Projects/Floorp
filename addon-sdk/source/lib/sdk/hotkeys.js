/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "stable"
};

const INVALID_HOTKEY = "Hotkey must have at least one modifier.";

const { toJSON: jsonify, toString: stringify,
        isFunctionKey } = require("./keyboard/utils");
const { register, unregister } = require("./keyboard/hotkeys");

const Hotkey = exports.Hotkey = function Hotkey(options) {
  if (!(this instanceof Hotkey))
    return new Hotkey(options);

  // Parsing key combination string.
  let hotkey = jsonify(options.combo);
  if (!isFunctionKey(hotkey.key) && !hotkey.modifiers.length) {
    throw new TypeError(INVALID_HOTKEY);
  }

  this.onPress = options.onPress && options.onPress.bind(this);
  this.toString = stringify.bind(null, hotkey);
  // Registering listener on keyboard combination enclosed by this hotkey.
  // Please note that `this.toString()` is a normalized version of
  // `options.combination` where order of modifiers is sorted and `accel` is
  // replaced with platform specific key.
  register(this.toString(), this.onPress);
  // We freeze instance before returning it in order to make it's properties
  // read-only.
  return Object.freeze(this);
};
Hotkey.prototype.destroy = function destroy() {
  unregister(this.toString(), this.onPress);
};

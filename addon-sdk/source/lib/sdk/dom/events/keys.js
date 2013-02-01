/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "unstable"
};

const { emit } = require("../events");
const { getCodeForKey, toJSON } = require("../../keyboard/utils");
const { has } = require("../../util/array");
const { isString } = require("../../lang/type");

const INITIALIZER = "initKeyEvent";
const CATEGORY = "KeyboardEvent";

function Options(options) {
  if (!isString(options))
    return options;

  var { key, modifiers } = toJSON(options);
  return {
    key: key,
    control: has(modifiers, "control"),
    alt: has(modifiers, "alt"),
    shift: has(modifiers, "shift"),
    meta: has(modifiers, "meta")
  };
}

var keyEvent = exports.keyEvent = function keyEvent(element, type, options) {

  emit(element, type, {
    initializer: INITIALIZER,
    category: CATEGORY,
    settings: [
      !("bubbles" in options) || options.bubbles !== false,
      !("cancelable" in options) || options.cancelable !== false,
      "window" in options && options.window ? options.window : null,
      "control" in options && !!options.control,
      "alt" in options && !!options.alt,
      "shift" in options && !!options.shift,
      "meta" in options && !!options.meta,
      getCodeForKey(options.key) || 0,
      options.key.length === 1 ? options.key.charCodeAt(0) : 0
    ]
  });
}

exports.keyDown = function keyDown(element, options) {
  keyEvent(element, "keydown", Options(options));
};

exports.keyUp = function keyUp(element, options) {
  keyEvent(element, "keyup", Options(options));
};

exports.keyPress = function keyPress(element, options) {
  keyEvent(element, "keypress", Options(options));
};


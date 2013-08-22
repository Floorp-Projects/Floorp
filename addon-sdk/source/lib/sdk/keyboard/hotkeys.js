/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "unstable"
};

const { observer: keyboardObserver } = require("./observer");
const { getKeyForCode, normalize, isFunctionKey,
        MODIFIERS } = require("./utils");

/**
 * Register a global `hotkey` that executes `listener` when the key combination
 * in `hotkey` is pressed. If more then one `listener` is registered on the same
 * key combination only last one will be executed.
 *
 * @param {string} hotkey
 *    Key combination in the format of 'modifier key'.
 *
 * Examples:
 *
 *     "accel s"
 *     "meta shift i"
 *     "control alt d"
 *
 * Modifier keynames:
 *
 *  - **shift**: The Shift key.
 *  - **alt**: The Alt key. On the Macintosh, this is the Option key. On
 *    Macintosh this can only be used in conjunction with another modifier,
 *    since `Alt+Letter` combinations are reserved for entering special
 *    characters in text.
 *  - **meta**: The Meta key. On the Macintosh, this is the Command key.
 *  - **control**: The Control key.
 *  - **accel**: The key used for keyboard shortcuts on the user's platform,
 *    which is Control on Windows and Linux, and Command on Mac. Usually, this
 *    would be the value you would use.
 *
 * @param {function} listener
 *    Function to execute when the `hotkey` is executed.
 */
exports.register = function register(hotkey, listener) {
  hotkey = normalize(hotkey);
  hotkeys[hotkey] = listener;
};

/**
 * Unregister a global `hotkey`. If passed `listener` is not the one registered
 * for the given `hotkey`, the call to this function will be ignored.
 *
 * @param {string} hotkey
 *    Key combination in the format of 'modifier key'.
 * @param {function} listener
 *    Function that will be invoked when the `hotkey` is pressed.
 */
exports.unregister = function unregister(hotkey, listener) {
  hotkey = normalize(hotkey);
  if (hotkeys[hotkey] === listener)
    delete hotkeys[hotkey];
};

/**
 * Map of hotkeys and associated functions.
 */
const hotkeys = exports.hotkeys = {};

keyboardObserver.on("keydown", function onKeypress(event, window) {
  let key, modifiers = [];
  let isChar = "isChar" in event && event.isChar;
  let which = "which" in event ? event.which : null;
  let keyCode = "keyCode" in event ? event.keyCode : null;

  if ("shiftKey" in event && event.shiftKey)
    modifiers.push("shift");
  if ("altKey" in event && event.altKey)
    modifiers.push("alt");
  if ("ctrlKey" in event && event.ctrlKey)
    modifiers.push("control");
  if ("metaKey" in event && event.metaKey)
    modifiers.push("meta");

  // If it's not a printable character then we fall back to a human readable
  // equivalent of one of the following constants.
  // http://mxr.mozilla.org/mozilla-central/source/dom/interfaces/events/nsIDOMKeyEvent.idl
  key = getKeyForCode(keyCode);

  // If only non-function (f1 - f24) key or only modifiers are pressed we don't
  // have a valid combination so we return immediately (Also, sometimes
  // `keyCode` may be one for the modifier which means we do not have a
  // modifier).
  if (!key || (!isFunctionKey(key) && !modifiers.length) || key in MODIFIERS)
    return;

  let combination = normalize({ key: key, modifiers: modifiers });
  let hotkey = hotkeys[combination];

  if (hotkey) {
    try {
      hotkey();
    } catch (exception) {
      console.exception(exception);
    } finally {
      // Work around bug 582052 by preventing the (nonexistent) default action.
      event.preventDefault();
    }
  }
});

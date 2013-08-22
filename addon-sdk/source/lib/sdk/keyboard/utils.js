/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "unstable"
};

const { Cc, Ci } = require("chrome");
const runtime = require("../system/runtime");
const { isString } = require("../lang/type");
const array = require("../util/array");


const SWP = "{{SEPARATOR}}";
const SEPARATOR = "-"
const INVALID_COMBINATION = "Hotkey key combination must contain one or more " +
                            "modifiers and only one key";

// Map of modifier key mappings.
const MODIFIERS = exports.MODIFIERS = {
  'accel': runtime.OS === "Darwin" ? 'meta' : 'control',
  'meta': 'meta',
  'control': 'control',
  'ctrl': 'control',
  'option': 'alt',
  'command': 'meta',
  'alt': 'alt',
  'shift': 'shift'
};

// Hash of key:code pairs for all the chars supported by `nsIDOMKeyEvent`.
// This is just a copy of the `nsIDOMKeyEvent` hash with normalized names.
// @See: http://mxr.mozilla.org/mozilla-central/source/dom/interfaces/events/nsIDOMKeyEvent.idl
const CODES = exports.CODES = new function Codes() {
  let nsIDOMKeyEvent = Ci.nsIDOMKeyEvent;
  // Names that will be substituted with a shorter analogs.
  let aliases = {
    'subtract':     '-',
    'add':          '+',
    'equals':       '=',
    'slash':        '/',
    'backslash':    '\\',
    'openbracket':  '[',
    'closebracket': ']',
    'quote':        '\'',
    'backquote':    '`',
    'period':       '.',
    'semicolon':    ';',
    'comma':        ','
  };

  // Normalizing keys and copying values to `this` object.
  Object.keys(nsIDOMKeyEvent).filter(function(key) {
    // Filter out only key codes.
    return key.indexOf('DOM_VK') === 0;
  }).map(function(key) {
    // Map to key:values
    return [ key, nsIDOMKeyEvent[key] ];
  }).map(function([key, value]) {
    return [ key.replace('DOM_VK_', '').replace('_', '').toLowerCase(), value ];
  }).forEach(function ([ key, value ]) {
    this[aliases[key] || key] = value;
  }, this);
};

// Inverted `CODES` hash of `code:key`.
const KEYS = exports.KEYS = new function Keys() {
  Object.keys(CODES).forEach(function(key) {
    this[CODES[key]] = key;
  }, this)
}

exports.getKeyForCode = function getKeyForCode(code) {
  return (code in KEYS) && KEYS[code];
};
exports.getCodeForKey = function getCodeForKey(key) {
  return (key in CODES) && CODES[key];
};

/**
 * Utility function that takes string or JSON that defines a `hotkey` and
 * returns normalized string version of it.
 * @param {JSON|String} hotkey
 * @param {String} [separator=" "]
 *    Optional string that represents separator used to concatenate keys in the
 *    given `hotkey`.
 * @returns {String}
 * @examples
 *
 *    require("keyboard/hotkeys").normalize("b Shift accel");
 *    // 'control shift b' -> on windows & linux
 *    // 'meta shift b'    -> on mac
 *    require("keyboard/hotkeys").normalize("alt-d-shift", "-");
 *    // 'alt shift d'
 */
var normalize = exports.normalize = function normalize(hotkey, separator) {
  if (!isString(hotkey))
    hotkey = toString(hotkey, separator);
  return toString(toJSON(hotkey, separator), separator);
};

/*
 * Utility function that splits a string of characters that defines a `hotkey`
 * into modifier keys and the defining key.
 * @param {String} hotkey
 * @param {String} [separator=" "]
 *    Optional string that represents separator used to concatenate keys in the
 *    given `hotkey`.
 * @returns {JSON}
 * @examples
 *
 *    require("keyboard/hotkeys").toJSON("accel shift b");
 *    // { key: 'b', modifiers: [ 'control', 'shift' ] } -> on windows & linux
 *    // { key: 'b', modifiers: [ 'meta', 'shift' ] }    -> on mac
 *
 *    require("keyboard/hotkeys").normalize("alt-d-shift", "-");
 *    // { key: 'd', modifiers: [ 'alt', 'shift' ] }
 */
var toJSON = exports.toJSON = function toJSON(hotkey, separator) {
  separator = separator || SEPARATOR;
  // Since default separator is `-`, combination may take form of `alt--`. To
  // avoid misbehavior we replace `--` with `-{{SEPARATOR}}` where
  // `{{SEPARATOR}}` can be swapped later.
  hotkey = hotkey.toLowerCase().replace(separator + separator, separator + SWP);

  let value = {};
  let modifiers = [];
  let keys = hotkey.split(separator);
  keys.forEach(function(name) {
    // If name is `SEPARATOR` than we swap it back.
    if (name === SWP)
      name = separator;
    if (name in MODIFIERS) {
      array.add(modifiers, MODIFIERS[name]);
    } else {
      if (!value.key)
        value.key = name;
      else
        throw new TypeError(INVALID_COMBINATION);
    }
  });

  if (!value.key)
      throw new TypeError(INVALID_COMBINATION);

  value.modifiers = modifiers.sort();
  return value;
};

/**
 * Utility function that takes object that defines a `hotkey` and returns
 * string representation of it.
 *
 * _Please note that this function does not validates data neither it normalizes
 * it, if you are unsure that data is well formed use `normalize` function
 * instead.
 *
 * @param {JSON} hotkey
 * @param {String} [separator=" "]
 *    Optional string that represents separator used to concatenate keys in the
 *    given `hotkey`.
 * @returns {String}
 * @examples
 *
 *    require("keyboard/hotkeys").toString({
 *      key: 'b',
 *      modifiers: [ 'control', 'shift' ]
 *    }, '+');
 *    // 'control+shift+b
 *
 */
var toString = exports.toString = function toString(hotkey, separator) {
  let keys = hotkey.modifiers.slice();
  keys.push(hotkey.key);
  return keys.join(separator || SEPARATOR);
};

/**
 * Utility function takes `key` name and returns `true` if it's function key
 * (F1, ..., F24) and `false` if it's not.
 */
var isFunctionKey = exports.isFunctionKey = function isFunctionKey(key) {
  var $
  return key[0].toLowerCase() === 'f' &&
         ($ = parseInt(key.substr(1)), 0 < $ && $ < 25);
};

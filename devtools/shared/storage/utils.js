/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(
  this,
  "validator",
  "resource://devtools/shared/storage/vendor/stringvalidator/validator.js"
);
loader.lazyRequireGetter(
  this,
  "JSON5",
  "resource://devtools/shared/storage/vendor/json5.js"
);

const MATH_REGEX =
  /(?:(?:^|[-+_*/])(?:\s*-?\d+(\.\d+)?(?:[eE][+-]?\d+)?\s*))+$/;

/**
 * Tries to parse a string into an object on the basis of key-value pairs,
 * separated by various separators. If failed, tries to parse for single
 * separator separated values to form an array.
 *
 * @param {string} value
 *        The string to be parsed into an object or array
 */
function _extractKeyValPairs(value) {
  const makeObject = (keySep, pairSep) => {
    const object = {};
    for (const pair of value.split(pairSep)) {
      const [key, val] = pair.split(keySep);
      object[key] = val;
    }
    return object;
  };

  // Possible separators.
  const separators = ["=", ":", "~", "#", "&", "\\*", ",", "\\."];
  // Testing for object
  for (let i = 0; i < separators.length; i++) {
    const kv = separators[i];
    for (let j = 0; j < separators.length; j++) {
      if (i == j) {
        continue;
      }
      const p = separators[j];
      const word = `[^${kv}${p}]*`;
      const keyValue = `${word}${kv}${word}`;
      const keyValueList = `${keyValue}(${p}${keyValue})*`;
      const regex = new RegExp(`^${keyValueList}$`);
      if (
        value.match &&
        value.match(regex) &&
        value.includes(kv) &&
        (value.includes(p) || value.split(kv).length == 2)
      ) {
        return makeObject(kv, p);
      }
    }
  }
  // Testing for array
  for (const p of separators) {
    const word = `[^${p}]*`;
    const wordList = `(${word}${p})+${word}`;
    const regex = new RegExp(`^${wordList}$`);

    if (regex.test(value)) {
      const pNoBackslash = p.replace(/\\*/g, "");
      return value.split(pNoBackslash);
    }
  }
  return null;
}

/**
 * Check whether the value string represents something that should be
 * displayed as text. If so then it shouldn't be parsed into a tree.
 *
 * @param  {String} value
 *         The value to be parsed.
 */
function _shouldParse(value) {
  const validators = [
    "isBase64",
    "isBoolean",
    "isCurrency",
    "isDataURI",
    "isEmail",
    "isFQDN",
    "isHexColor",
    "isIP",
    "isISO8601",
    "isMACAddress",
    "isSemVer",
    "isURL",
  ];

  // Check for minus calculations e.g. 8-3 because otherwise 5 will be displayed.
  if (MATH_REGEX.test(value)) {
    return false;
  }

  // Check for any other types that shouldn't be parsed.
  for (const test of validators) {
    if (validator[test](value)) {
      return false;
    }
  }

  // Seems like this is data that should be parsed.
  return true;
}

/**
 * Tries to parse a string value into either a json or a key-value separated
 * object. The value can also be a key separated array.
 *
 * @param {string} originalValue
 *        The string to be parsed into an object
 */
function parseItemValue(originalValue) {
  // Find if value is URLEncoded ie
  let decodedValue = "";
  try {
    decodedValue = decodeURIComponent(originalValue);
  } catch (e) {
    // Unable to decode, nothing to do
  }
  const value =
    decodedValue && decodedValue !== originalValue
      ? decodedValue
      : originalValue;

  if (!_shouldParse(value)) {
    return value;
  }

  let obj = null;
  try {
    obj = JSON5.parse(value);
  } catch (ex) {
    obj = null;
  }

  if (!obj && value) {
    obj = _extractKeyValPairs(value);
  }

  // return if obj is null, or same as value, or just a string.
  if (!obj || obj === value || typeof obj === "string") {
    return value;
  }

  // If we got this far, originalValue is an object literal or array,
  // and we have successfully parsed it
  return obj;
}

exports.parseItemValue = parseItemValue;

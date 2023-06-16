/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Take any number of parameters and returns a space-concatenated string.
 * If a parameter is a non-empty string, it's automatically added to the result.
 * If a parameter is an object, for each entry, if the value is truthy, then the key
 * is added to the result.
 *
 * For example: `classnames("hi", null, undefined, false, { foo: true, bar: false })` will
 * return `"hi foo"`
 *
 *
 * @param  {...string|object} argss
 * @returns String
 */
module.exports = function (...args) {
  let className = "";

  for (const arg of args) {
    if (!arg) {
      continue;
    }

    if (typeof arg == "string") {
      className += " " + arg;
    } else if (Object(arg) === arg) {
      // We don't test that we have an Object literal, so we can be as fast as we can
      for (const key in arg) {
        if (arg[key]) {
          className += " " + key;
        }
      }
    }
  }

  return className.trim();
};

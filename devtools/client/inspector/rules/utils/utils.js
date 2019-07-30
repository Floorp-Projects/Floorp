/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Returns true if the given property value is a CSS variables and contains the given
 * variable name, and false otherwise.
 *
 * @param {String} - propertyValue
 *        CSS property value (e.g. "var(--color)")
 * @param {String} - variableName
 *        CSS variable name (e.g. "--color")
 * @return {Boolean}
 */
function hasCSSVariable(propertyValue, variableName) {
  const regex = new RegExp(`(^|\\W)var\\(${variableName}\\s*[,)]`);
  return regex.test(propertyValue);
}

module.exports = { hasCSSVariable };

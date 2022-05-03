/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function getArrayTypeNames() {
  return [
    "Array",
    "Int8Array",
    "Uint8Array",
    "Int16Array",
    "Uint16Array",
    "Int32Array",
    "Uint32Array",
    "Float32Array",
    "Float64Array",
    "Uint8ClampedArray",
    "BigInt64Array",
    "BigUint64Array",
  ];
}

/**
 * Return true if the parameters passed to console.log is supported.
 * The parameters can be either from server side (without getGrip) or client
 * side (with getGrip).
 *
 * @param {Message} parameters
 * @returns {Boolean}
 */
function isSupportedByConsoleTable(parameters) {
  const supportedClasses = [
    "Object",
    "Map",
    "Set",
    "WeakMap",
    "WeakSet",
  ].concat(getArrayTypeNames());

  if (!Array.isArray(parameters) || parameters.length === 0 || !parameters[0]) {
    return false;
  }

  if (parameters[0].getGrip) {
    return supportedClasses.includes(parameters[0].getGrip().class);
  }

  return supportedClasses.includes(parameters[0].class);
}

module.exports = {
  getArrayTypeNames,
  isSupportedByConsoleTable,
};

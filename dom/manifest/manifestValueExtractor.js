/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * Helper function extracts values from manifest members
 * and reports conformance violations.
 */

function extractValue({
  objectName,
  object,
  property,
  expectedType,
  trim
}, console) {
  const value = object[property];
  const isArray = Array.isArray(value);
  // We need to special-case "array", as it's not a JS primitive.
  const type = (isArray) ? 'array' : typeof value;
  if (type !== expectedType) {
    if (type !== 'undefined') {
      let msg = `Expected the ${objectName}'s ${property} `;
      msg += `member to be a ${expectedType}.`;
      console.log(msg);
    }
    return undefined;
  }
  // Trim string and returned undefined if the empty string.
  const shouldTrim = expectedType === 'string' && value && trim;
  if (shouldTrim) {
    return value.trim() || undefined;
  }
  return value;
}

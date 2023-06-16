/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Shallow equal will consider equal:
 *  - exact same values (strict '===' equality)
 *  - distinct array instances having the exact same values in them (same number and strict equality).
 *  - distinct object instances having the exact same attributes and values.
 *
 * It will typically consider different array and object whose values
 * aren't strictly equal. You may consider using "deep equality" checks for this scenario.
 */
export function shallowEqual(value, other) {
  if (value === other) {
    return true;
  }

  if (Array.isArray(value) && Array.isArray(other)) {
    return arrayShallowEqual(value, other);
  }

  if (isObject(value) && isObject(other)) {
    return objectShallowEqual(value, other);
  }

  return false;
}

export function arrayShallowEqual(value, other) {
  // Redo this check in case we are called directly from the selectors.
  if (value === other) {
    return true;
  }
  return value.length === other.length && value.every((k, i) => k === other[i]);
}

function objectShallowEqual(value, other) {
  const existingKeys = Object.keys(other);
  const keys = Object.keys(value);

  return (
    keys.length === existingKeys.length &&
    keys.every((k, i) => k === existingKeys[i]) &&
    keys.every(k => value[k] === other[k])
  );
}

function isObject(value) {
  return typeof value === "object" && !!value;
}

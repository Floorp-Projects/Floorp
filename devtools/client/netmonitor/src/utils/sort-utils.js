/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Sorts object by keys in alphabetical order
 * If object has nested children, it sorts the child-elements also by keys
 * @param {object} which should be sorted by keys in alphabetical order
 */
function sortObjectKeys(object) {
  if (object == null) {
    return null;
  }

  if (Array.isArray(object)) {
    for (let i = 0; i < object.length; i++) {
      if (typeof object[i] === "object") {
        object[i] = sortObjectKeys(object[i]);
      }
    }
    return object;
  }

  return Object.keys(object)
    .sort(function(left, right) {
      return left.toLowerCase().localeCompare(right.toLowerCase());
    })
    .reduce((acc, key) => {
      if (typeof object[key] === "object") {
        acc[key] = sortObjectKeys(object[key]);
      } else {
        acc[key] = object[key];
      }
      return acc;
    }, {});
}

module.exports = {
  sortObjectKeys,
};

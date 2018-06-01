/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.exports = {

  /**
   * Create a simple enum-like object with keys mirrored to values from an array.
   * This makes comparison to a specfic value simpler without having to repeat and
   * mis-type the value.
   */
  createEnum(array, target = {}) {
    for (const key of array) {
      target[key] = key;
    }
    return target;
  }

};

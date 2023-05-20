/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Create a Promise and return it in an object with its resolve and reject functions.
 * This is useful when you need to settle a promise in a different location than where
 * it is created.
 *
 * @returns {Object} An object with the following properties:
 *   - {Promise} promise: The created DOM promise
 *   - {Function} resolve: The resolve parameter of `promise`
 *   - {Function} reject: The reject parameter of `promise`
 *
 */
module.exports = function defer() {
  let resolve, reject;
  const promise = new Promise(function (res, rej) {
    resolve = res;
    reject = rej;
  });
  return { resolve, reject, promise };
};
